// Moved here by tools/move_funcs.py from magic_s31.cpp (docs/source-layout-round.md).
#include "game/magic/corona_ray.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s31.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

// The phase handlers of other units a table holds (docs/magic_s31.md
// section 3): the engine's below by address; the rest ours now and named in
// the tables (S38's MagicFx_CountDownFlag10, S30's
// MagicFx_EndWhenChildrenDone, S11's MagicFx_UncountAndFree, S15's
// ChillRay_Grow / _Shrink, S12's MagicFx_CountDownRelease).
constexpr ::bof3::image::CodeVa kScriptUntilDone{bof3::addr::BattleFx_ScriptUntilDone};  // engine: the script ticked, the sprite queued, +2 on at the done flag

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S31_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four projected points at +8, +8 + step, +8 + 2 step, +8 + 3 step
// (0x10 a gouraud or flat-textured quad's, 0x14 a gouraud-textured one's).
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S31_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to layer 2.
void DrawModeCommit(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// Row 26 of Gfx_ClutStrip back from its source (0x812980 / 0x80E980), each
// word with its semi-transparency bit set.
void RestoreRow26Stp() {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStripDirty = 1;
}

// The actor-matrix push of 0x4E6DF0 / 0x4E77D0 (MagicFx_PushActorMatrix's
// shape with a turn about z): Camera_Matrix x the task's, translation
// RotTrans of (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)). One MATRIX
// block as the original lays it out on its stack, RotTrans writing its
// translation at +0x14; the task is read once, after the push.
struct Matrix {
    short m[10];
    long t[3];
};

void PushTurnedMatrix(const unsigned char* s, short turn) {
    const short rot[4] = {0, 0, turn, 0};
    short v[4];
    v[0] = static_cast<short>((rv::As<records::SpriteHead>(s)->x >> 9) - 0x4000);
    v[1] = static_cast<short>((rv::As<records::SpriteHead>(s)->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S31_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

}  // namespace

#define S31_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// One point pair of the ray's fan: (sin, cos) of 0x580 and of 0x280 at the
// radius SS(4), and the height sin(SS(8)) x SS(6), into the vertex pair at k
// (0 or 8, x / y / z).
void RayPoint(unsigned k, int angle) {
    int v = MH_CALL(Math_Sin)(angle);
    SetVW(k, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Cos)(angle);
    SetVW(k + 2, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Sin)(SS(8));
    SetVW(k + 4, static_cast<unsigned>(Mul12(v, SS(6))));
}

}  // namespace

// original 0x4E7420: the kind-2 task. A three-entry stack table by +1:
// Corona_Start, Corona_Wait, Corona_End.
S31_EXPORT void __cdecl Corona_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Corona_Start, bof3::addr::Corona_Wait,
                                                 bof3::addr::Corona_End};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Corona_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4E7420's Start, 0x4E7450: +0xB 0, +9 0x18, +1 on; the ray (kind
// 1, 0x4B, +1 0, the owner's direction) and the flash (the same, +1 1), each
// with +0x80 this task and counted in +0xB; row 26 of the CLUT strip back with
// its STP bits. In an event battle (0x904AAA) whose acting enemy's +0x100 is
// 0x29 (the actor byte 0x904B34 - 3, unchecked): a third child whose first
// 0x80 bytes are that enemy's record, +1 2, +2 0, +6 1, +5 0x4B, +9 0, +0x29 3.
S31_EXPORT void __cdecl Corona_Start(void) {
    Sc()[0xB] = 0;
    Sc()[9] = 0x18;
    Inc(Sc()[1]);
    {
        const unsigned slot = NewTask(0x4B);
        const unsigned char* const owner = Owner();
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 0;
        child[8] = owner[8];
        Inc(s[0xB]);
    }
    {
        const unsigned slot = NewTask(0x4B);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 1;
        Inc(s[0xB]);
    }
    RestoreRow26Stp();
    if (Mem(::bof3::Img(EventBattle_Id_addr))[0] == 0) return;
    if (EnemyRecord(ActorIndex())[0x100] != 0x29) return;
    const unsigned slot = NewTask(0x4B);
    unsigned char* const child = TaskSlot(slot);
    const unsigned char* const record = EnemyRecord(ActorIndex());
    // rep movsd: 0x20 dwords, forward, one at a time (the two can overlap: a
    // party actor's "record" lies among the task slots)
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, Long(record + k));
    child[1] = 2;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x4B;
    child[9] = 0;
    child[0x29] = 3;
}

// original 0x4E75B0: +9 down; at 0 the target flags 0x20 and +1 on.
S31_EXPORT void __cdecl Corona_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x20);
    Inc(Sc()[1]);
}

// original 0x4E75F0: once both children have ended (+0xB 0): the target
// flagged 0x40, +9 0x1E, the effect's done flag, the task freed.
S31_EXPORT void __cdecl Corona_End(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Sc()[9] = 0x1E;
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E7630: the children's kind-1 task, a jmp through
// CoronaChild_Kinds (three entries: the ray, the flash, the enemy copy) by +1,
// unchecked.
S31_EXPORT void __cdecl CoronaChild_Task(void) {
    static constexpr std::uint32_t kKinds[3] = {bof3::addr::CoronaRay_Run, bof3::addr::CoronaFlash_Run,
                                                bof3::addr::CoronaEnemy_Task};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("CoronaChild_Task", phase, 3);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4E7650: a call through CoronaRay_Steps (four entries: _Start,
// MAGIC067's 0x4B6B20 and 0x4B6B70, _Advance) by +2; then while +0 is set the
// ray's matrix, the ray, the matrix popped.
S31_EXPORT void __cdecl CoronaRay_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::CoronaRay_Start, bof3::addr::ChillRay_Grow,
                                                bof3::addr::ChillRay_Shrink, bof3::addr::CoronaRay_Advance};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("CoronaRay_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::CoronaRay_PushMatrix);
    Call0(bof3::addr::CoronaRay_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E7680: the offset (-0x28000, 0) turned by the direction, from
// the field's kind-2 point; the height the ground there + 0x200; sound effect
// 0x100; +0x14 0x2FFF, +0xB 0x34, +9 0, +0xA 0x10, +2 on.
S31_EXPORT void __cdecl CoronaRay_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(0xFFFD8000u));
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    {
        unsigned char* const t = Sc();
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        tt->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(tt->step_x) +
                                                    static_cast<std::uint32_t>(Field_Kind2X));
    }
    {
        unsigned char* const t = Sc();
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        tt->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(tt->step_z) +
                                                    static_cast<std::uint32_t>(Field_Kind2Z));
    }
    const long ground = MH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, static_cast<unsigned>(static_cast<std::uint32_t>(ground) + 0x200u) & 0xFFFF);
    MH_CALL(Sound_PlayEffect)(0x100);
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x2FFF);
    Sc()[0xB] = 0x34;
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4E7730: the angle +0xB on by (+9 >> 6) + 1; on odd frames the
// size +0xA down; the step (0x2000, 0) turned by the direction and added to
// the position; +9 down by 2; at 0x30 the owner's count +0xB down and the
// task freed.
S31_EXPORT void __cdecl CoronaRay_Advance(void) {
    {
        unsigned char* const s = Sc();
        AddB(s[0xB], static_cast<unsigned>(s[9] >> 6) + 1);
    }
    if (static_cast<unsigned char>(Frame_Counter) & 1) Dec(Sc()[0xA]);
    rv::Current<records::BattleTask>()->step_x = 0x2000;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->x) +
                                                    static_cast<std::uint32_t>(ts->step_x));
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->z) +
                                                    static_cast<std::uint32_t>(ts->step_z));
    }
    AddB(Sc()[9], 0xFE);
    if (Sc()[9] != 0x30) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E77D0: the ray's matrix pushed, turned about z by its direction
// +8 through a four-entry jump table (0, 0x400, 0x800, 0xC00). Past 3 the
// original leaves the angle an uninitialised stack word; ours aborts.
S31_EXPORT void __cdecl CoronaRay_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned facing = s[8];
    if (facing > 3) bof3::Fatal("CoronaRay_PushMatrix: facing %u, past the 4-entry jump table", facing);
    PushTurnedMatrix(s, static_cast<short>(facing << 10));
}

// original 0x4E78B0: the ray - +9 - 1 semi-transparent gouraud-textured quads
// (tpage 0x340 / 0x100, CLUT row 0x1FA) between the arcs at 0x580 and 0x280 of
// radius 0x80 + 0x20 i, each lifted by sin(((+0xB + i) & 0x3F) << 6) x +0xA;
// the shade 0x80 (at step 3 (+9 - 0x30) / 3 x 8) ramps up over the first 16
// and down over the last 8; the texture's u scrolls with the frame counter.
// Between two draw-mode packets.
S31_EXPORT void __cdecl CoronaRay_Draw(void) {
    DrawModeCommit(0xB5);
    {
        const unsigned char* const s = Sc();
        SetSW(2, 0x80);
        SetSW(0, s[9]);
        SetSW(4, 0x80);
        SetSW(6, s[0xA]);
        SetSW(8, (s[0xB] & 0x3Fu) << 6);
    }
    RayPoint(0, 0x580);
    RayPoint(8, 0x280);
    for (int i = 1; SS(0) > i; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned char b = Sc()[0xB];
            SetSW(4, static_cast<unsigned>(i + 4) << 5);
            const std::uint16_t v0 = VW(0), v2 = VW(2), v4 = VW(4), v8 = VW(8), va = VW(0xA), vc = VW(0xC);
            SetVW(0x10, v0);
            SetSW(8, ((b + static_cast<unsigned>(i)) & 0x3Fu) << 6);
            SetVW(0x12, v2);
            SetVW(0x14, v4);
            SetVW(0x18, v8);
            SetVW(0x1A, va);
            SetVW(0x1C, vc);
        }
        RayPoint(0, 0x580);
        RayPoint(8, 0x280);
        {
            const unsigned char* const s = Sc();
            if (s[2] == 3) {
                const int d = static_cast<int>(s[9]) - 0x30;
                SetSW(2, static_cast<unsigned>(Shl(d / 3, 3)));
            }
        }
        for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au, 0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(2);
        if (i < 0x10) {
            const auto up = static_cast<unsigned char>(i + 1);
            const auto at = static_cast<unsigned char>(i);
            for (unsigned k : {4u, 5u, 6u}) p[k] = static_cast<unsigned char>(DivPow2(SS(2), 4) * up);
            p[0x18] = p[4];
            p[0x19] = p[5];
            p[0x1A] = p[6];
            for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu}) p[k] = static_cast<unsigned char>(DivPow2(SS(2), 4) * at);
            p[0x40] = p[0x2C];
            p[0x41] = p[0x2D];
            p[0x42] = p[0x2E];
        }
        if (SS(0) - 8 < i) {
            SetSW(0xA, SW(2));
            const std::uint32_t near = static_cast<std::uint32_t>(DivPow2(SS(0xA), 3)) *
                                       (static_cast<std::uint32_t>(SD(0)) - static_cast<std::uint32_t>(i));
            SetSW(2, near);
            p[4] = static_cast<unsigned char>(near);
            for (unsigned k : {5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = SB(2);
            const std::uint32_t far = (static_cast<std::uint32_t>(SD(0)) - static_cast<std::uint32_t>(i) + 1) *
                                      static_cast<std::uint32_t>(DivPow2(SS(0xA), 3));
            SetSW(2, far);
            p[0x2C] = static_cast<unsigned char>(far);
            for (unsigned k : {0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(2);
        }
        const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100);
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, tpage);
        const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, clut);
        {
            const std::uint32_t scroll = ~(static_cast<std::uint32_t>(Frame_Counter) * 2u) + static_cast<std::uint32_t>(i);
            const std::uint32_t u = (0x3Fu - (scroll & 0x3Fu)) << 2;
            SetSW(0xE, u);
            if ((u & 0xFFFF) == 0xFC) SetSW(0xE, 0xFB);
        }
        p[0x14] = 0;
        p[0x15] = SB(0xE);
        p[0x28] = 0xFF;
        p[0x3C] = 0;
        p[0x29] = SB(0xE);
        p[0x3D] = static_cast<unsigned char>(SB(0xE) + 4);
        p[0x50] = 0xFF;
        p[0x51] = static_cast<unsigned char>(SB(0xE) + 4);
        Rtp4(p, 0x14);
        MH_CALL(Gte_PrimDepths4_14)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x54);
    }
    DrawModeCommit(0x15);
}

// original 0x4E7E30: a call through CoronaFlash_Steps (four entries: _Start,
// MAGIC086's BarrierRing_Grow, MAGIC078's MagicFx_WaitA, MAGIC060's 0x4B1740)
// by +2; then while +0 is set, the flash (a tail jmp).
S31_EXPORT void __cdecl CoronaFlash_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::CoronaFlash_Start, bof3::addr::BarrierRing_Grow,
                                                bof3::addr::MagicFx_WaitA, bof3::addr::MagicFx_CountDownRelease};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("CoronaFlash_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::CoronaFlash_Draw);
}

// original 0x4E7E60: +9 0, +0xA 0x88, +2 on.
S31_EXPORT void __cdecl CoronaFlash_Start(void) {
    Sc()[9] = 0;
    Sc()[0xA] = 0x88;
    Inc(Sc()[2]);
}

// original 0x4E7E80: the flash - one semi-transparent gouraud quad over the
// whole 320 x 240 screen, its bright edge on the side the owner's direction
// +8 faces (bit 0), (+9, +9, +9) there and (+9 x 12, +9 x 4, +9 x 4) at the
// other; between two draw-mode packets.
S31_EXPORT void __cdecl CoronaFlash_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    constexpr std::int32_t kBottom = 0x436F0000;   // 239.0f
    // DIV-0041: x from Widescreen_FillX() to 319 + the columns (the PSP's
    // -32..351); 0..319 while the fills are unarmed, as the fuzz compares.
    const auto left = std::bit_cast<std::int32_t>(Widescreen_FillX());
    const auto right = std::bit_cast<std::int32_t>(Widescreen_FillRight(319.0f));
    if (Owner()[8] & 1) {
        SetLong(p + 8, left);
        SetLong(p + 0xC, 0);
        SetLong(p + 0x18, right);
        SetLong(p + 0x1C, 0);
        SetLong(p + 0x28, left);
        SetLong(p + 0x2C, kBottom);
        SetLong(p + 0x38, right);
    } else {
        SetLong(p + 8, right);
        SetLong(p + 0xC, 0);
        SetLong(p + 0x18, left);
        SetLong(p + 0x1C, 0);
        SetLong(p + 0x28, right);
        SetLong(p + 0x2C, kBottom);
        SetLong(p + 0x38, left);
    }
    SetLong(p + 0x3C, kBottom);
    {
        const unsigned char a = Sc()[9];
        for (unsigned k : {0x36u, 6u, 0x35u, 5u, 0x34u, 4u}) p[k] = a;
    }
    {
        const auto b = static_cast<unsigned char>(Sc()[9] * 12);
        p[0x24] = b;
        p[0x14] = b;
    }
    {
        const auto c = static_cast<unsigned char>(Sc()[9] << 2);
        for (unsigned k : {0x26u, 0x25u, 0x16u, 0x15u}) p[k] = c;
    }
    MH_CALL(Gfx_CommitPrim)(2, 0x44);
    DrawModeCommit(0x15);
}

// original 0x4E7F80: the enemy copy's task. A three-entry stack table by +2:
// CoronaEnemy_Start, the engine's 0x43EC10 (the script until the done flag),
// BattleFx_FreeTask.
S31_EXPORT void __cdecl CoronaEnemy_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::CoronaEnemy_Start, kScriptUntilDone.v,
                                                bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("CoronaEnemy_Task", phase, 3);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4E7FB0: +0xB and +9 0, the copy's animation 3, its script ticked
// once, the sprite queued; +2 on.
S31_EXPORT void __cdecl CoronaEnemy_Start(void) {
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    MH_CALL(Sprite_SetAnimation)(3);
    MH_CALL(Sprite_ScriptTick)();
    MH_CALL(Sprite_QueueOverlay)();
    Inc(Sc()[2]);
}

void CoronaRay_Inject() {
    BOF3_INJECT(Corona_Task);
    BOF3_INJECT(Corona_Start);
    BOF3_INJECT(Corona_Wait);
    BOF3_INJECT(Corona_End);
    BOF3_INJECT(CoronaChild_Task);
    BOF3_INJECT(CoronaRay_Run);
    BOF3_INJECT(CoronaRay_Start);
    BOF3_INJECT(CoronaRay_Advance);
    BOF3_INJECT(CoronaRay_PushMatrix);
    BOF3_INJECT(CoronaRay_Draw);
    BOF3_INJECT(CoronaFlash_Run);
    BOF3_INJECT(CoronaFlash_Start);
    BOF3_INJECT(CoronaFlash_Draw);
    BOF3_INJECT(CoronaEnemy_Task);
    BOF3_INJECT(CoronaEnemy_Start);
}

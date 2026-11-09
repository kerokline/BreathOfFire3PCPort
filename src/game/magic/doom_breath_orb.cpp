// Moved here by tools/move_funcs.py from magic_s31.cpp (docs/source-layout-round.md).
#include "game/magic/doom_breath_orb.h"

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

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

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

// DoomBreathOrb_DrawGlow's shade: 0xC0, or at step 4 the width +0x1C / 2 x 12.
void GlowShade() {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    if (s[2] == 4) SetSW(4, (static_cast<std::uint32_t>(DivPow2(ts->work_1c, 1)) * 3u) << 2);
    else SetSW(4, 0xC0);
}

// A quarter of the shade word, toward zero, as a byte.
unsigned char Quarter() { return static_cast<unsigned char>(DivPow2(SS(4), 2)); }

}  // namespace

// original 0x4E6950: the kind-2 task. A five-entry stack table by +1:
// DoomBreath_Start, _Brighten, _Fade, MAGIC226/227's 0x4F9F70, _End.
S31_EXPORT void __cdecl DoomBreath_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::DoomBreath_Start, bof3::addr::DoomBreath_Brighten,
                                                 bof3::addr::DoomBreath_Fade, bof3::addr::MagicFx_CountDownFlag10,
                                                 bof3::addr::DoomBreath_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("DoomBreath_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4E6990: the owner's direction byte; the task to the side's
// centre; one orb (kind 1, 0x64) with +0x80 this task, +1 0, +9 0x18; row 26
// of the CLUT strip back with its STP bits; the actor's sprite (0x904B3C)
// tinted black, its tint slot to +0xA; sound 0x100; +0xB and +9 0, +1 on.
S31_EXPORT void __cdecl DoomBreath_Start(void) {
    {
        const unsigned char facing = Owner()[8];
        Sc()[8] = facing;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    const unsigned slot = NewTask(0x64);
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(Sc()));
    child[1] = 0;
    child[9] = 0x18;
    RestoreRow26Stp();
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)));
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)), 0, 0, 0, 1);
    Sc()[0xA] = tint;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4E6A50: the tint record +0xA (MoveScript_TintRecords + 12 n)
// one step brighter in each of its three channels (+2..+4), +9 up; at 0x10 the
// actor sprite's +0x29 2 and +1 on.
S31_EXPORT void __cdecl DoomBreath_Brighten(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) Inc(MoveScript_TintRecords[s[0xA] * 12u + c]);
    Inc(s[9]);
    if (Sc()[9] != 0x10) return;
    Pointer(::bof3::Img(MagicS23_ActorSprite_addr))[0x29] = 2;
    Inc(Sc()[1]);
}

// original 0x4E6AD0: the tint record one step darker, +9 down; at 0 the actor
// sprite's tint released, the actor flashed (BattleActor_Flash of the actor
// byte 0x904B34), +9 0x10, +1 on.
S31_EXPORT void __cdecl DoomBreath_Fade(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) Dec(MoveScript_TintRecords[s[0xA] * 12u + c]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)));
    MH_CALL(BattleActor_Flash)(Mem(::bof3::Img(at::kActor))[0]);
    Sc()[9] = 0x10;
    Inc(Sc()[1]);
}

// original 0x4E6B60: once the orb has ended (+0xB 0xFF): the actor sprite's
// +0x29 4, the target flagged 0x40, the effect's done flag, the task freed.
S31_EXPORT void __cdecl DoomBreath_End(void) {
    if (Sc()[0xB] != 0xFF) return;
    Pointer(::bof3::Img(MagicS23_ActorSprite_addr))[0x29] = 4;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E6BA0: the orb's kind-1 task, a jmp through
// DoomBreathOrb_TaskTable (one entry) by +1, unchecked.
S31_EXPORT void __cdecl DoomBreathOrb_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("DoomBreathOrb_Task", phase, 1);
    magic_harness::Phase(bof3::addr::DoomBreathOrb_Run)();
}

// original 0x4E6BC0: a call through DoomBreathOrb_Steps (five entries) by +2;
// then while +0 and +2 are set: +0xA up past step 1, the orb's matrix, its
// stream and its glow, the matrix popped.
S31_EXPORT void __cdecl DoomBreathOrb_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::DoomBreathOrb_Launch, bof3::addr::DoomBreathOrb_Move,
                                                bof3::addr::DoomBreathOrb_Grow, bof3::addr::DoomBreathOrb_Shrink,
                                                bof3::addr::DoomBreathOrb_End};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("DoomBreathOrb_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    if (s[2] > 1) Inc(s[0xA]);
    Call0(bof3::addr::DoomBreathOrb_PushMatrix);
    Call0(bof3::addr::DoomBreathOrb_DrawStream);
    Call0(bof3::addr::DoomBreathOrb_DrawGlow);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E6C00: +9 down; at 0 the orb starts: the owner's direction; the
// offset (-8, -15) << 16 turned by it, from the field's kind-2 point; the
// height the owner's + 0x800000; the step (0x8000, 0) turned; +0xB 0x1A, +9
// 0x2C, +0xA 0, +0x1C 0, +0x20 0x80, +2 on.
S31_EXPORT void __cdecl DoomBreathOrb_Launch(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    s[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(0xFFF80000u));
    rv::Store(rv::Current<records::BattleTask>()->step_z, static_cast<std::int32_t>(0xFFF10000u));
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
    {
        const std::uint32_t height = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) + 0x800000u;
        rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(height));
    }
    rv::Current<records::BattleTask>()->step_x = 0x8000;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    Sc()[0xB] = 0x1A;
    Sc()[9] = 0x2C;
    Sc()[0xA] = 0;
    rv::Current<records::BattleTask>()->work_1c = 0;
    rv::Store(rv::Current<records::BattleTask>()->work_20, 0x80);
    Inc(Sc()[2]);
}

// original 0x4E6D00: the position +0x34 / +0x38 on by the step +0xC / +0x10,
// +9 down; at 0 +2 on.
S31_EXPORT void __cdecl DoomBreathOrb_Move(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->x) +
                                                static_cast<std::uint32_t>(ts->step_x));
    ts->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->z) +
                                                static_cast<std::uint32_t>(ts->step_z));
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4E6D40: the angle +0xB down (mod 0x40), the width +0x1C up by 2;
// at 0x80 +2 on.
S31_EXPORT void __cdecl DoomBreathOrb_Grow(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    s[0xB] = static_cast<unsigned char>((s[0xB] - 1) & 0x3F);
    ts->work_1c = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->work_1c) + 2);
    if (ts->work_1c == 0x80) Inc(s[2]);
}

// original 0x4E6D70: the angle down (mod 0x40), the width +0x1C and the radius
// +0x20 down by 2; at a width of 0x20 +2 on.
S31_EXPORT void __cdecl DoomBreathOrb_Shrink(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    s[0xB] = static_cast<unsigned char>((s[0xB] - 1) & 0x3F);
    ts->work_1c = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->work_1c) - 2);
    rv::Store(ts->work_20, static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->work_20) - 2));
    if (ts->work_1c == 0x20) Inc(s[2]);
}

// original 0x4E6DB0: the width down by 1, the radius by 2; at a width of 0
// the owner's +0xB 0xFF (DoomBreath_End's signal) and the task freed.
S31_EXPORT void __cdecl DoomBreathOrb_End(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->work_1c = static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->work_1c) - 1);
    rv::Store(ts->work_20, static_cast<std::int32_t>(static_cast<std::uint32_t>(ts->work_20) - 2));
    if (ts->work_1c != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E6DF0: the orb's matrix pushed, turned about z by its direction
// +8 (0x400, 0x800, 0xC00 for 0, 1, 2; 0 for any other).
S31_EXPORT void __cdecl DoomBreathOrb_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned char facing = s[8];
    const short turn = facing == 0 ? short{0x400} : facing == 1 ? short{0x800} : facing == 2 ? short{0xC00} : short{0};
    PushTurnedMatrix(s, turn);
}

// original 0x4E6EC0: the stream - 63 semi-transparent textured quads (tpage
// 0x340 / 0x100, CLUT row 0x1FA) up a sine of amplitude +0x20 whose phase is
// (+0xB + i) & 0x3F, each 0x40 long and 0xE00 wide, their shade 0x80 (at step
// 4 the width +0x1C / 2 x 8), the texture's v stepping with (+0xA + i) % 14;
// between two draw-mode packets.
S31_EXPORT void __cdecl DoomBreathOrb_DrawStream(void) {
    DrawModeCommit(0xB5);
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        SetSW(0, static_cast<std::uint16_t>(ts->work_20));
        const unsigned angle = (s[0xB] & 0x3Fu) << 7;
        SetSW(2, angle);
        if (s[2] == 4) SetSW(4, static_cast<unsigned>(Shl(DivPow2(ts->work_1c, 1), 3)));
        else SetSW(4, 0x80);
        SetVW(0x10, 0);
        SetVW(0x12, 0);
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        SetVW(0x14, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    for (int i = 1; i < 0x40; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t y = VW(0x12), z = VW(0x14);
            const unsigned char* const s = Sc();
            SetVW(0, 0);
            SetVW(2, y);
            SetVW(4, z);
            SetVW(8, 0xE00);
            SetVW(0xA, y);
            SetVW(0xC, z);
            const unsigned angle = ((s[0xB] + static_cast<unsigned>(i)) & 0x3Fu) << 7;
            SetVW(0x10, 0);
            SetSW(2, angle);
            SetVW(0x12, static_cast<unsigned>(i) << 6);
            const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
            const int w = Mul12(v, SS(0));
            const std::uint16_t y2 = VW(0x12);
            SetVW(0x18, 0xE00);
            SetVW(0x14, static_cast<unsigned>(w));
            SetVW(0x1C, static_cast<unsigned>(w));
            p[4] = SB(4);
            SetVW(0x1A, y2);
            p[5] = SB(4);
            p[6] = SB(4);
        }
        const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
        const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, clut);
        SetSW(6, ((Sc()[0xA] + static_cast<unsigned>(i)) % 14u << 4) + 8);
        p[0x14] = 8;
        p[0x24] = 0xF8;
        p[0x15] = SB(6);
        p[0x25] = SB(6);
        p[0x34] = 8;
        p[0x44] = 0xF8;
        p[0x35] = static_cast<unsigned char>(SB(6) + 0x10);
        p[0x45] = static_cast<unsigned char>(SB(6) + 0x10);
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x48);
    }
    DrawModeCommit(0x95);
}

// original 0x4E7130: the glow - two semi-transparent gouraud quads, 0xC00
// wide, 0x40 long, from y -0x80, up the same sine as the stream; the first
// bright at its far edge, the second at its near one (the shade and a quarter
// of it); between two draw-mode packets.
S31_EXPORT void __cdecl DoomBreathOrb_DrawGlow(void) {
    DrawModeCommit(0xB5);
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        if (s[2] == 4) SetSW(4, static_cast<unsigned>(Shl(DivPow2(ts->work_1c, 1), 3)));
        else SetSW(4, 0x80);
        SetSW(0, static_cast<std::uint16_t>(ts->work_20));
        const unsigned angle = (s[0xB] & 0x3Fu) << 7;
        SetVW(0x10, 0);
        SetSW(2, angle);
        SetVW(0x12, 0xFF80);
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        SetVW(0x14, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    for (int i = 1; i < 3; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t y = VW(0x12), z = VW(0x14);
            SetVW(2, y);
            SetVW(0xA, y);
            const unsigned char* const s = Sc();
            SetVW(0, 0);
            SetVW(4, z);
            SetVW(8, 0xC00);
            SetVW(0xC, z);
            const unsigned angle = ((s[0xB] + static_cast<unsigned>(i)) & 0x3Fu) << 7;
            SetVW(0x10, 0);
            SetSW(2, angle);
            SetVW(0x12, (static_cast<unsigned>(i) << 6) - 0x40);
            const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
            const int w = Mul12(v, SS(0));
            const std::uint16_t y2 = VW(0x12);
            SetVW(0x18, 0xC00);
            SetVW(0x14, static_cast<unsigned>(w));
            SetVW(0x1C, static_cast<unsigned>(w));
            SetVW(0x1A, y2);
        }
        GlowShade();
        if (i == 1) {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
            p[0x24] = SB(4);
            p[0x25] = Quarter();
            p[0x26] = Quarter();
            p[0x34] = SB(4);
            p[0x35] = Quarter();
            p[0x36] = Quarter();
        } else {
            p[4] = SB(4);
            p[5] = Quarter();
            p[6] = Quarter();
            p[0x14] = SB(4);
            p[0x15] = Quarter();
            p[0x16] = Quarter();
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x44);
    }
    DrawModeCommit(0x95);
}

void DoomBreathOrb_Inject() {
    BOF3_INJECT(DoomBreath_Task);
    BOF3_INJECT(DoomBreath_Start);
    BOF3_INJECT(DoomBreath_Brighten);
    BOF3_INJECT(DoomBreath_Fade);
    BOF3_INJECT(DoomBreath_End);
    BOF3_INJECT(DoomBreathOrb_Task);
    BOF3_INJECT(DoomBreathOrb_Run);
    BOF3_INJECT(DoomBreathOrb_Launch);
    BOF3_INJECT(DoomBreathOrb_Move);
    BOF3_INJECT(DoomBreathOrb_Grow);
    BOF3_INJECT(DoomBreathOrb_Shrink);
    BOF3_INJECT(DoomBreathOrb_End);
    BOF3_INJECT(DoomBreathOrb_PushMatrix);
    BOF3_INJECT(DoomBreathOrb_DrawStream);
    BOF3_INJECT(DoomBreathOrb_DrawGlow);
}

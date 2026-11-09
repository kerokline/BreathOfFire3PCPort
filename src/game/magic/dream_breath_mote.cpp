// Moved here by tools/move_funcs.py from magic_s09.cpp (docs/source-layout-round.md).
#include "game/magic/dream_breath_mote.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s09.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
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
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(Long(at)) + v)); }

// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

// `imul` then `sar 0xC`.
int Mul12(int a, int b) { return Imul(a, b) >> 12; }

// `cdq / sub eax, edx / sar 1`: a signed halving toward zero.
int Half(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) - static_cast<std::uint32_t>(v >> 31)) >> 1; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
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

// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S09_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// A primitive linked at the current task's position (+0x34 / +0x38, read at
// the call) on layer 2.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The task's +8 and position (+0x34 / +0x38 / +0x3C) the owner's.
void FacingAndPositionFromOwner() {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// One MATRIX block as the originals lay it out on their stack: the rotation,
// then the translation RotTrans writes at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};

// Camera_Matrix x the rotation of `angles`, translated by RotTrans of `v`,
// set as the GTE's (the two matrix pushes' common tail). The original pushes
// a third argument (a flag pointer) to Gte_RotTrans, which takes two.
void SetTurnedMatrix(const short* v, const short* angles) {
    Matrix m;
    long flag;
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S09_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

}  // namespace

#define S09_EXPORT extern "C" __attribute__((disable_tail_calls))

// A step toward the destination (+0xC, +0x10, +0x14 / 2) at 0x10.
static void DreamBreathMoteStep() {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const std::int32_t x = (ts->step_x >> 9) - 0x4000;
    const std::int32_t z = (ts->step_z >> 9) - 0x4000;
    const std::int32_t y = ts->step_y >> 1;
    MH_CALL(MagicFx_StepTowardPoint)(U(x), U(z), U(y), 0, 0x10);
}

// original 0x4AAC40: the kind-2 task. A three-entry stack table by +1:
// DreamBreath_Start, MAGIC222's 0x4F7320, BattleFx_Finish.
S09_EXPORT void __cdecl DreamBreath_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::DreamBreath_Start, bof3::addr::Combustion_Wait,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "DreamBreath_Task");
}

// original 0x4AAC70: +0xB 0; the owner's direction and position; +9 0xA, +1
// on; twelve motes (kind 1, 0x1E; the slots unchecked): +0x80 this task, +1
// 0, +0xB the index i, +9 ((i ^ 0xF) + 0x1D) << 3 (a byte: the delays), +8
// ours; our count +0xB up each.
S09_EXPORT void __cdecl DreamBreath_Start(void) {
    Sc()[0xB] = 0;
    FacingAndPositionFromOwner();
    Sc()[9] = 0xA;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 12; ++i) {
        const unsigned slot = NewTask(0x1E);
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        unsigned char* const s = Sc();
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(static_cast<unsigned char>((i ^ 0xF) + 0x1D) << 3);
        child[8] = s[8];
        Inc(s[0xB]);
    }
}

// original 0x4AAD50: the motes' kind-1 task, a jmp through
// DreamBreathMote_TaskTable (one entry) by +1, unchecked.
S09_EXPORT void __cdecl DreamBreathMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {bof3::addr::DreamBreathMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "DreamBreathMote_Task");
}

// original 0x4AAD70: a call through DreamBreathMote_Steps (three entries) by
// +2; then while +2 and +0 are set the mote's matrix, its band, the matrix
// popped.
S09_EXPORT void __cdecl DreamBreathMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::DreamBreathMote_Start, bof3::addr::DreamBreathMote_Fly,
                                                bof3::addr::DreamBreathMote_Land};
    Dispatch(kSteps, 3, Sc()[2], "DreamBreathMote_Run");
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    Call0(bof3::addr::DreamBreathMote_PushMatrix);
    Call0(bof3::addr::DreamBreathMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AADB0: +9 down; at 0: the target side's mean height into
// +0x3E (DreamBreath_TargetHeight); (0x10000, 0) turned onto the owner's
// position; the destination +0xC / +0x10 (0x80000, 0) turned, from the
// field's kind-2 point, +0x14 that height + 0x100; our height the owner's +
// 0x100; +9 0, +0xA 0, +2 on.
S09_EXPORT void __cdecl DreamBreathMote_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Call0(bof3::addr::DreamBreath_TargetHeight);
    rv::Current<records::BattleTask>()->step_x = 0x10000;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->x) + U(ts->step_x));
        ts->z = static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->z) + U(ts->step_z));
        ts->step_x = 0x80000;
        ts->step_z = 0;
    }
    Turn(Sc());
    unsigned char* const s = Sc();
    AddLong(s + 0xC, U(Field_Kind2X));
    AddLong(s + 0x10, U(Field_Kind2Z));
    rv::Store(rv::As<records::BattleTask>(s)->step_y, S16(s + 0x3E) + 0x100);
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, (rv::As<records::SpriteHead>(Owner())->height_whole + 0x100u) & 0xFFFF);
    s[9] = 0;
    s[0xA] = 0;
    Inc(s[2]);
}

// original 0x4AAEB0: +9 up; a step toward the destination; the shade +0xA up
// to 0x10; near it (MagicFx_NearPoint3D within 0x60000) +2 on.
S09_EXPORT void __cdecl DreamBreathMote_Fly(void) {
    Inc(Sc()[9]);
    DreamBreathMoteStep();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (s[0xA] != 0x10) Inc(s[0xA]);
    if (MH_CALL(MagicFx_NearPoint3D)(U(ts->step_x), U(ts->step_z), U(ts->height), 0x60000) != 0) Inc(Sc()[2]);
}

// original 0x4AAF50: a step toward the destination; +9 up, the shade down;
// at 0 the owner's count down and the task freed.
S09_EXPORT void __cdecl DreamBreathMote_Land(void) {
    DreamBreathMoteStep();
    unsigned char* const s = Sc();
    Inc(s[9]);
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AAFD0: a draw-mode packet (tpage 0x35) linked at the task; the
// inner radius word 0x903852 +9 x 4, the outer 0x903854 +9 x 6 (past +9 8
// plus Rand & 0xF), shades 0x903856 +0xA x 8 and 0x903858 +0xA x 4; 32
// semi-transparent gouraud quads round the y-z plane between the two radii
// (Prim_VertexScratch: the last and next outer, the last and next inner edge;
// the scratch read again after every call), projected (Gte_RotTransPers4,
// Gte_PrimDepths4_10B) and linked at the task (0x44).
S09_EXPORT void __cdecl DreamBreathMote_Draw(void) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    {
        const unsigned char* s = Sc();
        SetSW(2, s[9] * 4u);
        SetSW(4, s[9] * 6u);
        if (s[9] > 8) {
            const std::uint32_t r = RandCall();
            SetSW(4, SW(4) + (r & 0xF));
            s = Sc();
        }
        SetSW(6, s[0xA] * 8u);
        SetSW(8, s[0xA] * 4u);
    }
    SetVW(0, 0);
    SetVW(8, 0);
    int v = Cos(0);
    SetVW(0xA, U(Mul12(v, SS(4))));
    v = Sin(0);
    SetVW(0xC, U(Mul12(v, SS(4))));
    SetVW(0x10, 0);
    SetVW(0x18, 0);
    v = Cos(0);
    SetVW(0x1A, U(Mul12(v, SS(2))));
    v = Sin(0);
    SetVW(0x1C, U(Mul12(v, SS(2))));
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t oy = VW(0xA), oz = VW(0xC);
            SetVW(2, oy);
            SetVW(4, oz);
        }
        v = Cos(a);
        SetVW(0xA, U(Mul12(v, SS(4))));
        v = Sin(a);
        {
            const int oz = Mul12(v, SS(4));
            const std::uint16_t iy = VW(0x1A);
            SetVW(0xC, U(oz));
            const std::uint16_t iz = VW(0x1C);
            SetVW(0x12, iy);
            SetVW(0x14, iz);
        }
        v = Cos(a);
        SetVW(0x1A, U(Mul12(v, SS(2))));
        v = Sin(a);
        SetVW(0x1C, U(Mul12(v, SS(2))));
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(6);
        p[0x25] = SB(8);
        p[0x26] = SB(8);
        p[0x34] = SB(6);
        p[0x35] = SB(8);
        p[0x36] = SB(8);
        long depth, flag;
        S09_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), p + 8, p + 0x18, p + 0x28, p + 0x38, &depth,
                                         &flag);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
    }
}

// original 0x4AB250: the mote's matrix pushed, turned about z toward its
// destination (Math_Ratan2 of (+0x10 - +0x38, +0xC - +0x34), & 0xFFF): its x /
// z and its own height, read after the angle.
S09_EXPORT void __cdecl DreamBreathMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short angles[3] = {0, 0, 0};
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const std::int32_t dx = static_cast<std::int32_t>(U(ts->step_x) - U(ts->x));
        const std::int32_t dz = static_cast<std::int32_t>(U(ts->step_z) - U(ts->z));
        const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dz), static_cast<float>(dx));
        angles[2] = static_cast<short>(angle & 0xFFF);
    }
    short v[4];
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    v[0] = static_cast<short>((ts->x >> 9) - 0x4000);
    v[1] = static_cast<short>((ts->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-Half(S16(s + 0x3E)));
    v[3] = 0;
    SetTurnedMatrix(v, angles);
}

// original 0x4AB330: the mean height of the target side's actors still in
// (dwords 0x903850 the sum, 0x903854 the count, each read again after every
// Battle_ActorIsOut): the eight enemies with the target's bit 0x40, else the
// three party members; into word +0x3E. The original's idiv faults with none
// in (and on the one overflowing quotient); ours aborts there (the
// live-target divide, docs/takeover-queue-round9.md section 7).
S09_EXPORT void __cdecl DreamBreath_TargetHeight(void) {
    const unsigned char target = TargetByte();
    SetSD(4, 0);
    SetSD(0, 0);
    if (target & 0x40) {
        for (unsigned i = 0; i < 8; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i + 3) & 0xFF) != 0) continue;
            SetSD(0, U(SD(0)) + U(S16(Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride + 0x3E))));
            SetSD(4, U(SD(4)) + 1);
        }
    } else {
        for (unsigned i = 0; i < 3; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i) & 0xFF) != 0) continue;
            SetSD(0, U(SD(0)) + U(S16(PartyRecord(i) + 0x3E)));
            SetSD(4, U(SD(4)) + 1);
        }
    }
    const std::int32_t sum = SD(0);
    unsigned char* const s = Sc();
    const std::int32_t count = SD(4);
    if (count == 0) bof3::Fatal("DreamBreath_TargetHeight: no actor of the target side in (the original's idiv faults)");
    if (sum == INT_MIN && count == -1) bof3::Fatal("DreamBreath_TargetHeight: the mean overflows (the original's idiv faults)");
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, static_cast<std::uint32_t>(sum / count) & 0xFFFF);
}

void DreamBreathMote_Inject() {
    BOF3_INJECT(DreamBreath_Task);
    BOF3_INJECT(DreamBreath_Start);
    BOF3_INJECT(DreamBreathMote_Task);
    BOF3_INJECT(DreamBreathMote_Run);
    BOF3_INJECT(DreamBreathMote_Start);
    BOF3_INJECT(DreamBreathMote_Fly);
    BOF3_INJECT(DreamBreathMote_Land);
    BOF3_INJECT(DreamBreathMote_Draw);
    BOF3_INJECT(DreamBreathMote_PushMatrix);
    BOF3_INJECT(DreamBreath_TargetHeight);
}

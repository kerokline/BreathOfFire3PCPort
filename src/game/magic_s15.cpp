// Three spell overlays compiled into the exe, round nine group S15
// (docs/magic_s15.md): the PSX's MAGIC067, MAGIC068 and MAGIC069.EMI,
// Magic_Rows rows 18, 40 and 75. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Chill, Foretell and Influence; the names
// below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC067 0x4B66D0..0x4B73F6: Corona's shape (group S31, MAGIC137) - a
//     ray and a full-screen flash (kind 1, 0x13), and in an event battle a
//     copy of the acting enemy - then, once both have ended, one marker child
//     at each actor of the target's side that is not out;
//   - MAGIC068 0x4B7400..0x4B7D39: one child (kind 1, 0x1C) that shows a
//     counting texture cell above the caster; then a message chosen from the
//     two sides' records (a score from each member's ratio of two record
//     words, the sides' mean of a third, and a table), six flag bits from the
//     live enemies' record bytes drawn as tiles until the message closes;
//   - MAGIC069 0x4B7DE0..0x4B8D66: six children (kind 1, 0x39) that slide in
//     from either side of the caster's screen point drawing a ring, triangles
//     and a cross; the first then spawns a child at every actor (not the
//     actor or the target) whose record words or flag bits pass a test, each
//     tinting its actor's sprite in and out under a textured quad.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), the ray matrix
// aborts on a facing past 3, where the original turns by an uninitialised
// stack word (group S31's CoronaRay_PushMatrix, the same code), and
// Foretell_Read aborts on a zero divisor, where the original's idiv faults
// (the live-target divide, docs/takeover-queue-round9.md section 6).
#include "game/magic_s15.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kEventBattle = 0x904AAA;   // u8: not 0 in an event battle
constexpr std::uint32_t kBattleB2 = 0x904AB2;      // u8: a battle byte Foretell_Read adds in (not read further)

// The overlays' own .data (read in place, as the originals read it).
constexpr std::uint32_t kForetellBias = 0x65ACB7;   // Foretell_Bias + 7: s8, by party count - enemy count
constexpr std::uint32_t kForetellTiles = 0x65ACBC;  // Foretell_TileColours: six entries of three words
constexpr std::uint32_t kInfluencePoints = 0x65AD30;   // Influence_TrianglePoints: four u8 angles

// The height tables InfluenceMark_Start lifts its quad by: a party member's
// (two bytes by its +0x89, the first when its +8 is 0 or 1) and an enemy
// type's (+0x8C x the type +0xF0; group S20's lift bytes).
constexpr std::uint32_t kPartyLift = 0x64DFC8;
constexpr std::uint32_t kEnemyLift = 0x8C564F;
constexpr std::uint32_t kEnemyLiftStride = 0x8C;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(at::kActor))) & 0xFF; }
unsigned TargetIndex() { return static_cast<unsigned>(Long(Mem(at::kTarget))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3 (a party index lands below the enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
bool IsOut(unsigned actor) { return (MH_CALL(Battle_ActorIsOut)(actor & 0xFF) & 0xFFu) != 0; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

// The phase handlers of other units a table holds (docs/magic_s15.md
// section 3): called by their addresses.
constexpr std::uint32_t kScriptUntilDone = 0x43EC10;   // engine: the script ticked, the sprite queued, +2 on at the done flag
constexpr std::uint32_t kFreeOwnerCount = 0x4AF490;    // MAGIC058: the owner's +0xB down, the task freed
constexpr std::uint32_t kFlashPhase3 = 0x4B1740;       // MAGIC060

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The callees with the arguments the originals push. Gte_RotTransPers4 gets
// the depth and flag pointers the originals pass; ours reads the first.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S15_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four projected points at +8, +8 + step, +8 + 2 step, +8 + 3 step.
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S15_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}
// A draw-mode packet (tpage `tpage`, dithered) committed to layer `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

// The actor-matrix push of 0x4B6BC0 (group S31's 0x4E77D0 shape):
// Camera_Matrix x the task's, turned about z, translation RotTrans of
// (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)). One MATRIX block as the
// original lays it out on its stack, RotTrans writing its translation at
// +0x14; the task is read once, after the push.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
void PushTurnedMatrix(const unsigned char* s, short turn) {
    const short rot[4] = {0, 0, turn, 0};
    short v[4];
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S15_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// A marker child at an actor's position: kind 1 `parameter`, +0x80 this task,
// +1 2, +3 the actor's number on its side, +4 5, and the record's +0x34 /
// +0x38 / +0x3C, each read after the create; this task's +0xB up.
void MarkAt(const unsigned char* record, unsigned index, unsigned parameter) {
    const unsigned slot = NewTask(parameter);
    unsigned char* const child = TaskSlot(slot);
    const std::int32_t x = Long(record + 0x34);
    unsigned char* const s = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
    child[1] = 2;
    child[3] = static_cast<unsigned char>(index);
    child[4] = 5;
    SetLong(child + 0x34, x);
    SetLong(child + 0x38, Long(record + 0x38));
    SetLong(child + 0x3C, Long(record + 0x3C));
    Inc(s[0xB]);
}

}  // namespace

#define S15_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC067 (row 18, Chill read one id down)

// original 0x4B66D0: the kind-2 task. A five-entry stack table by +1:
// Chill_Start, MAGIC137's Corona_Wait, Chill_WaitChildren, Chill_SpawnMarks,
// MAGIC131's MagicFx_EndWhenChildrenDone.
S15_EXPORT void __cdecl Chill_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Chill_Start, bof3::addr::Corona_Wait,
                                                 bof3::addr::Chill_WaitChildren, bof3::addr::Chill_SpawnMarks,
                                                 bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("Chill_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4B6710: +0xB 0, +9 0x18, +1 on; the ray (kind 1, 0x13, +1 0, the
// owner's direction) and the flash (the same, +1 1), each with +0x80 this task
// and counted in +0xB; CLUT row 2 back from its source with the STP bit, and
// the first 32 words of row 26 without it. In an event battle (0x904AAA)
// whose acting enemy's +0x100 is 0x29 (the actor byte 0x904B34 - 3,
// unchecked): a third child whose first 0x80 bytes are that enemy's record,
// +1 3, +2 0, +6 1, +5 0x13, +9 0. BattleTask_Create's 0xFF is not tested.
S15_EXPORT void __cdecl Chill_Start(void) {
    Sc()[0xB] = 0;
    Sc()[9] = 0x18;
    Inc(Sc()[1]);
    {
        const unsigned slot = NewTask(0x13);
        const unsigned char* const owner = Owner();
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 0;
        child[8] = owner[8];
        Inc(s[0xB]);
    }
    {
        const unsigned slot = NewTask(0x13);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 1;
        Inc(s[0xB]);
    }
    for (unsigned k = 0x200; k < 0x300; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0; k < 0x10; ++k) {
        Gfx_ClutStrip[0x1A00 + k] = Gfx_ClutStripSource[0x1A00 + k];
        Gfx_ClutStrip[0x1A10 + k] = Gfx_ClutStripSource[0x1A10 + k];
    }
    Gfx_ClutStripDirty = 1;
    if (Mem(kEventBattle)[0] == 0) return;
    if (EnemyRecord(ActorIndex())[0x100] != 0x29) return;
    const unsigned slot = NewTask(0x13);
    unsigned char* const child = TaskSlot(slot);
    const unsigned char* const record = EnemyRecord(ActorIndex());
    // rep movsd: 0x20 dwords, forward, one at a time (the two can overlap: a
    // party actor's "record" lies among the task slots)
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, Long(record + k));
    child[1] = 3;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x13;
    child[9] = 0;
}

// original 0x4B6890: once both children have ended (+0xB 0): the target
// flagged 0x40, +9 0x1E, +1 on.
S15_EXPORT void __cdecl Chill_WaitChildren(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Sc()[9] = 0x1E;
    Inc(Sc()[1]);
}

// original 0x4B68C0: +9 down; at 0 a marker (kind 1, 0x13, +1 2) at every
// actor of the target's side (0x904B44 bit 0x40: the eight enemies, else the
// three members) that is not out, then +1 on.
S15_EXPORT void __cdecl Chill_SpawnMarks(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    if (TargetByte() & 0x40) {
        for (unsigned i = 0; i < 8; ++i) {
            if (IsOut(i + 3)) continue;
            MarkAt(EnemyRecord(i + 3), i, 0x13);
        }
    } else {
        for (unsigned i = 0; i < 3; ++i) {
            if (IsOut(i)) continue;
            MarkAt(PartyRecord(i), i, 0x13);
        }
    }
    Inc(Sc()[1]);
}

// original 0x4B6A40: the children's kind-1 task, a jmp through
// ChillChild_Kinds (four entries: the ray, the flash, the marker, the enemy
// copy) by +1, unchecked.
S15_EXPORT void __cdecl ChillChild_Task(void) {
    static constexpr std::uint32_t kKinds[4] = {bof3::addr::ChillRay_Run, bof3::addr::ChillFlash_Run,
                                                bof3::addr::ChillMark_Run, bof3::addr::ChillEnemy_Task};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("ChillChild_Task", phase, 4);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4B6A60: a call through ChillRay_Steps (four entries: _Start,
// _Grow, _Shrink, MAGIC137's CoronaRay_Advance) by +2; then while +0 is set
// the ray's matrix, the ray, the matrix popped (a tail jmp).
S15_EXPORT void __cdecl ChillRay_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::ChillRay_Start, bof3::addr::ChillRay_Grow,
                                                bof3::addr::ChillRay_Shrink, bof3::addr::CoronaRay_Advance};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ChillRay_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::ChillRay_PushMatrix);
    Call0(bof3::addr::ChillRay_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4B6A90: the task to the side's centre; the offset (-0x40000, 0)
// turned by the direction, from the field's kind-2 point; sound effect 0x100;
// +0x14 0x2FFF, +0xB 0x34, +9 0, +0xA 0x10, +2 on.
S15_EXPORT void __cdecl ChillRay_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(0xFFFC0000u));
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    {
        unsigned char* const t = Sc();
        SetLong(t + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(t + 0xC)) +
                                                    static_cast<std::uint32_t>(Field_Kind2X)));
    }
    {
        unsigned char* const t = Sc();
        SetLong(t + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(t + 0x10)) +
                                                    static_cast<std::uint32_t>(Field_Kind2Z)));
    }
    MH_CALL(Sound_PlayEffect)(0x100);
    SetLong(Sc() + 0x14, 0x2FFF);
    Sc()[0xB] = 0x34;
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4B6B20: the angle +0xB on by (+9 >> 6) + 1; on odd frames the
// height +0xA up; +9 up by 2; at 0xC0 +2 on.
S15_EXPORT void __cdecl ChillRay_Grow(void) {
    {
        unsigned char* const s = Sc();
        AddB(s[0xB], static_cast<unsigned>(s[9] >> 6) + 1);
    }
    if (static_cast<unsigned char>(Frame_Counter) & 1) Inc(Sc()[0xA]);
    AddB(Sc()[9], 2);
    if (Sc()[9] == 0xC0) Inc(Sc()[2]);
}

// original 0x4B6B70: as _Grow with the height down and +9 down by 2; at 0x60
// +2 on.
S15_EXPORT void __cdecl ChillRay_Shrink(void) {
    {
        unsigned char* const s = Sc();
        AddB(s[0xB], static_cast<unsigned>(s[9] >> 6) + 1);
    }
    if (static_cast<unsigned char>(Frame_Counter) & 1) Dec(Sc()[0xA]);
    AddB(Sc()[9], 0xFE);
    if (Sc()[9] == 0x60) Inc(Sc()[2]);
}

// original 0x4B6BC0: the ray's matrix pushed, turned about z by its direction
// +8 through a four-entry jump table (0, 0x400, 0x800, 0xC00). Past 3 the
// original leaves the angle an uninitialised stack word; ours aborts.
S15_EXPORT void __cdecl ChillRay_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned facing = s[8];
    if (facing > 3) bof3::Fatal("ChillRay_PushMatrix: facing %u, past the 4-entry jump table", facing);
    PushTurnedMatrix(s, static_cast<short>(facing << 10));
}

namespace {

// One point pair of the ray's fan: (sin, cos) of `angle` at the radius SS(4),
// and the height sin(SS(8)) x SS(6), into the vertex at k (0 or 8, x / y / z).
void RayPoint(unsigned k, int angle) {
    int v = MH_CALL(Math_Sin)(angle);
    SetVW(k, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Cos)(angle);
    SetVW(k + 2, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Sin)(SS(8));
    SetVW(k + 4, static_cast<unsigned>(Mul12(v, SS(6))));
}

}  // namespace

// original 0x4B6CA0: the ray - +9 - 1 semi-transparent gouraud-textured quads
// (tpage 0x340 / 0x100, CLUT row 0x1E2) between the arcs at 0x580 and 0x280 of
// radius 0x200 + 0x20 i, each lifted by sin(((+0xB + i) & 0x3F) << 6) x +0xA;
// the shade 0x80 (at step 3 (+9 - 0x30) / 3 x 8) ramps up over the first 16
// and down over the last 8; the texture's u is (0x54 - ((not (Frame_Counter
// x 2) + i) & 0x3F)) x 3. Between two draw-mode packets.
S15_EXPORT void __cdecl ChillRay_Draw(void) {
    DrawModeCommit(0xB5, 2);
    {
        const unsigned char* const s = Sc();
        SetSW(2, 0x80);
        SetSW(0, s[9]);
        SetSW(4, 0x200);
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
            SetSW(4, static_cast<unsigned>(i + 0x10) << 5);
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
            const auto here = static_cast<unsigned char>(i);
            for (unsigned k : {4u, 5u, 6u}) p[k] = static_cast<unsigned char>(DivPow2(SS(2), 4) * up);
            p[0x18] = p[4];
            p[0x19] = p[5];
            p[0x1A] = p[6];
            for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu}) p[k] = static_cast<unsigned char>(DivPow2(SS(2), 4) * here);
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
        SetWord(p + 0x2A, tpage);
        const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1E2);
        SetWord(p + 0x16, clut);
        {
            const std::uint32_t scroll = ~(static_cast<std::uint32_t>(Frame_Counter) * 2u) + static_cast<std::uint32_t>(i);
            SetSW(0xE, (0x54u - (scroll & 0x3Fu)) * 3u);
        }
        p[0x14] = 0;
        p[0x28] = 0xFF;
        p[0x15] = SB(0xE);
        p[0x29] = SB(0xE);
        p[0x3C] = 0;
        p[0x50] = 0xFF;
        p[0x3D] = static_cast<unsigned char>(SB(0xE) + 3);
        p[0x51] = static_cast<unsigned char>(SB(0xE) + 3);
        Rtp4(p, 0x14);
        MH_CALL(Gte_PrimDepths4_14)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x54);
    }
    DrawModeCommit(0x15, 2);
}

// original 0x4B7210: a call through ChillFlash_Steps (four entries: MAGIC137's
// CoronaFlash_Start, MAGIC086's BarrierRing_Grow, MAGIC078's MagicFx_WaitA,
// MAGIC060's 0x4B1740) by +2; then while +0 is set, the flash (a tail jmp).
S15_EXPORT void __cdecl ChillFlash_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::CoronaFlash_Start, bof3::addr::BarrierRing_Grow,
                                                bof3::addr::MagicFx_WaitA, kFlashPhase3};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ChillFlash_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::ChillFlash_Draw);
}

// original 0x4B7240: the flash - one semi-transparent gouraud quad over the
// whole 320 x 240 screen, its bright edge on the side the owner's direction
// +8 faces (bit 0), (+9, +9, +9) there and (+9 x 8) in each channel at the
// other (words 0x903850 / 0x903852 keep the two); between two draw-mode
// packets.
S15_EXPORT void __cdecl ChillFlash_Draw(void) {
    DrawModeCommit(0x35, 2);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    constexpr std::int32_t kRight = 0x439F8000;    // 319.0f
    constexpr std::int32_t kBottom = 0x436F0000;   // 239.0f
    if (Owner()[8] & 1) {
        SetLong(p + 8, 0);
        SetLong(p + 0xC, 0);
        SetLong(p + 0x18, kRight);
        SetLong(p + 0x1C, 0);
        SetLong(p + 0x28, 0);
        SetLong(p + 0x2C, kBottom);
        SetLong(p + 0x38, kRight);
    } else {
        SetLong(p + 8, kRight);
        SetLong(p + 0xC, 0);
        SetLong(p + 0x18, 0);
        SetLong(p + 0x1C, 0);
        SetLong(p + 0x28, kRight);
        SetLong(p + 0x2C, kBottom);
        SetLong(p + 0x38, 0);
    }
    SetLong(p + 0x3C, kBottom);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[9]);
        SetSW(2, static_cast<unsigned>(s[9]) << 3);
        p[0x34] = s[9];
    }
    for (unsigned k : {4u, 0x35u, 5u, 0x36u, 6u}) p[k] = SB(0);
    for (unsigned k : {0x24u, 0x14u, 0x25u, 0x15u, 0x26u, 0x16u}) p[k] = SB(2);
    MH_CALL(Gfx_CommitPrim)(2, 0x44);
    DrawModeCommit(0x15, 2);
}

// original 0x4B7380: the marker's kind-1 task, a jmp through ChillMark_Steps
// (two entries: MAGIC038's WarShoutBuff_Start, MAGIC105's
// MagicFx_EndWithChildren) by +2, unchecked.
S15_EXPORT void __cdecl ChillMark_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::WarShoutBuff_Start, bof3::addr::MagicFx_EndWithChildren};
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("ChillMark_Run", phase, 2);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B73A0: the enemy copy's task. A three-entry stack table by +2:
// ChillEnemy_Start, the engine's 0x43EC10 (the script until the done flag),
// BattleFx_FreeTask.
S15_EXPORT void __cdecl ChillEnemy_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ChillEnemy_Start, kScriptUntilDone,
                                                bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("ChillEnemy_Task", phase, 3);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B73D0: +0xB and +9 0, the copy's animation 3, +2 on.
S15_EXPORT void __cdecl ChillEnemy_Start(void) {
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    MH_CALL(Sprite_SetAnimation)(3);
    Inc(Sc()[2]);
}

// ===========================================================================
// MAGIC068 (row 40, Foretell read one id down)

// original 0x4B7400: the kind-2 task. A five-entry stack table by +1:
// Foretell_Start, _Read, _Pause, _Show, _End.
S15_EXPORT void __cdecl Foretell_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Foretell_Start, bof3::addr::Foretell_Read,
                                                 bof3::addr::Foretell_Pause, bof3::addr::Foretell_Show,
                                                 bof3::addr::Foretell_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("Foretell_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4B7440: the owner's direction and position (+0x34 / +0x38 /
// +0x3C); one child (kind 1, 0x1C, +1 0, +0x80 this task); +0xB 1, +1 on.
S15_EXPORT void __cdecl Foretell_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    const unsigned slot = NewTask(0x1C);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
    child[1] = 0;
    s[0xB] = 1;
    Inc(Sc()[1]);
}

namespace {

// Word a x 100 / word b, as the original's `idiv` (both zero-extended, so the
// quotient is never negative); a zero divisor faults in the original.
int Percent(const unsigned char* record, unsigned a, unsigned b, const char* side, unsigned i) {
    const int den = Word(record + b);
    if (den == 0) bof3::Fatal("Foretell_Read: %s %u's word +0x%X is 0, the original's idiv faults", side, i, b);
    return static_cast<int>(Word(record + a)) * 100 / den;
}

// The band a ratio falls in: `zero` for 0, else the last of `bands` (the
// values for > 0, > 10, .. > 70) whose bound it passes; a negative ratio keeps
// the previous step (the original's register carried over; never reached, as
// the ratio is never negative).
int Band(int q, int previous, int zero, const int (&bands)[8]) {
    if (q == 0) return zero;
    int step = previous;
    for (int k = 0; k < 8; ++k)
        if (q > k * 10) step = bands[k];
    return step;
}

void PushMessage(unsigned id) {
    const unsigned char* const text = MH_CALL(Msg_SystemPtr)(id);
    MH_CALL(BattleQueue_Push)(1, 0x3C, static_cast<unsigned long>(Key(text)));
}

}  // namespace

// original 0x4B74C0: once the child has ended (+0xB 0). A score from the two
// sides: each present member (+0 set; +0x91 bit 0x40 takes 0x18 off and is not
// counted) adds a step by its word +0x98 x 100 / word +0xA0, and each enemy
// not out one by its word +0xA4 x 100 / word +0xB0; then (0x904AB2 - the
// enemies counted) x 8, a band of the difference of the members' mean byte
// +0x8A and the enemies' mean word +0x98 (16-bit), and Foretell_Bias by the
// count difference. The score picks one of the messages 0x3C..0x40
// (Msg_SystemPtr) queued (BattleQueue_Push(1, 0x3C, text)). Then +0xB gets a
// bit for each of six record bytes (+0xBF, +0xC0, +0xC1, +0xC3, +0xC2 at most
// 1, +0xC4 at most 3) of any enemy not out; +9 0x16, +0xA 4, +1 on. A zero
// count or divisor faults in the original; ours aborts.
S15_EXPORT void __cdecl Foretell_Read(void) {
    if (Sc()[0xB] != 0) return;
    static constexpr int kPartyBands[8] = {-0x15, -0x12, -0xF, -0xC, -9, -6, -3, 0};
    static constexpr int kEnemyBands[8] = {0x15, 0x12, 0xF, 0xC, 9, 8, 3, 0};
    std::uint32_t score = 0;
    int step = 0;
    unsigned party_n = 0;
    std::uint16_t party_sum = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char* const r = PartyRecord(i);
        if (r[0] == 0) continue;
        if (r[0x91] & 0x40) {
            score -= 0x18;
            continue;
        }
        party_sum = static_cast<std::uint16_t>(party_sum + r[0x8A]);
        ++party_n;
        step = Band(Percent(r, 0x98, 0xA0, "member", i), step, -0x18, kPartyBands);
        score += static_cast<std::uint32_t>(step);
    }
    unsigned enemy_n = 0;
    std::uint16_t enemy_sum = 0;
    for (unsigned i = 0; i < 8; ++i) {
        if (IsOut(i + 3)) continue;
        const unsigned char* const e = EnemyRecord(i + 3);
        enemy_sum = static_cast<std::uint16_t>(enemy_sum + Word(e + 0x98));
        ++enemy_n;
        step = Band(Percent(e, 0xA4, 0xB0, "enemy", i), step, 0x18, kEnemyBands);
        score += static_cast<std::uint32_t>(step);
    }
    score += (static_cast<std::uint32_t>(Mem(kBattleB2)[0]) - enemy_n) * 8u;
    if (party_n == 0) bof3::Fatal("Foretell_Read: no member counted, the original's idiv faults");
    if (enemy_n == 0) bof3::Fatal("Foretell_Read: no enemy counted, the original's idiv faults");
    const int mean_party = static_cast<short>(party_sum) / static_cast<int>(party_n);
    const int mean_enemy = static_cast<short>(enemy_sum) / static_cast<int>(enemy_n);
    const short diff = static_cast<short>(mean_party - mean_enemy);
    static constexpr struct {
        short at_most;
        int value;
    } kLevels[] = {{20, 0x78}, {15, 0x6E}, {10, 0x64}, {7, 0x5F}, {5, 0x5A}, {3, 0x55}, {1, 0x50},
                   {-1, 0x46}, {-3, 0x3C}, {-5, 0x2D}, {-7, 0x1E}, {-10, 0x14}, {-15, 0xA}, {-20, 5}};
    int level = 0x82;
    for (const auto& l : kLevels)
        if (diff <= l.at_most) level = l.value;
    const int bias = static_cast<signed char>(
        Mem(kForetellBias + static_cast<std::uint32_t>(static_cast<int>(party_n) - static_cast<int>(enemy_n)))[0]);
    score += static_cast<std::uint32_t>(bias + level);
    short s16 = static_cast<short>(score);
    if (s16 < 0) {
        PushMessage(0x40);
    } else {
        if (s16 > 100) s16 = 100;
        if (s16 > 0x5A) PushMessage(0x3C);
        else if (s16 > 0x46) PushMessage(0x3D);
        else if (s16 > 0x32) PushMessage(0x3E);
        else if (s16 > 0x1E) PushMessage(0x3F);
        else PushMessage(0x40);
    }
    for (unsigned i = 0; i < 8; ++i) {
        if (IsOut(i + 3)) continue;
        const unsigned char* const e = EnemyRecord(i + 3);
        if (e[0xBF] <= 1) Sc()[0xB] |= 1;
        if (e[0xC0] <= 1) Sc()[0xB] |= 2;
        if (e[0xC1] <= 1) Sc()[0xB] |= 4;
        if (e[0xC3] <= 1) Sc()[0xB] |= 8;
        if (e[0xC2] <= 1) Sc()[0xB] |= 0x10;
        if (e[0xC4] <= 3) Sc()[0xB] |= 0x20;
    }
    Sc()[9] = 0x16;
    Sc()[0xA] = 4;
    Inc(Sc()[1]);
}

// original 0x4B7880: +0xA down; at 0 +0xA 0x3E and +1 on.
S15_EXPORT void __cdecl Foretell_Pause(void) {
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Sc()[0xA] = 0x3E;
    Inc(Sc()[1]);
}

// original 0x4B78B0: the tiles; a button pressed (Input_Pressed) sends +1 to
// 4.
S15_EXPORT void __cdecl Foretell_Show(void) {
    Call0(bof3::addr::Foretell_DrawTiles);
    if (Input_Pressed != 0) Sc()[1] = 4;
}

// original 0x4B78D0: with the message window down (0x939F60 0) the effect's
// done flag and the task freed; else while +9 is above 0 (signed) +9 down by
// 8 and the tiles drawn.
S15_EXPORT void __cdecl Foretell_End(void) {
    if (Mem(at::kMessageUp)[0] == 0) {
        Mem(at::kFlags)[0] |= 4;
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    unsigned char* const s = Sc();
    const auto nine = static_cast<signed char>(s[9]);
    if (nine <= 0) return;
    s[9] = static_cast<unsigned char>(nine - 8);
    Call0(bof3::addr::Foretell_DrawTiles);
}

// original 0x4B7900: a draw-mode packet (tpage 0x15) to layer 0; then from
// x 0xE8 (dword 0x903850), y +9 (dword 0x903854) a flat 8 x 12 tile (Gpu_SetTile)
// for each of +0xB's six bits that is set, coloured by Foretell_TileColours,
// 10 further right each; none set: one grey (0x60) tile. Layer 0.
S15_EXPORT void __cdecl Foretell_DrawTiles(void) {
    DrawModeCommit(0x15, 0);
    unsigned char* s = Sc();
    SetSD(0, 0xE8);
    SetSD(4, s[9]);
    constexpr std::int32_t kW = 0x41000000;   // 8.0f
    constexpr std::int32_t kH = 0x41400000;   // 12.0f
    for (unsigned k = 0; k < 6; ++k) {
        if (((1u << k) & s[0xB]) == 0) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetTile)(p);
        PutFloat(p + 8, SD(0));
        PutFloat(p + 0xC, SD(4));
        SetSD(0, static_cast<std::uint32_t>(SD(0)) + 10u);
        SetLong(p + 0x14, kW);
        SetLong(p + 0x18, kH);
        const unsigned char* const colour = Mem(kForetellTiles + 6 * k);
        p[4] = colour[0];
        p[5] = colour[2];
        p[6] = colour[4];
        MH_CALL(Gfx_CommitPrim)(0, 0x1C);
        s = Sc();
    }
    if (s[0xB] != 0) return;
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetTile)(p);
    PutFloat(p + 8, SD(0));
    SetLong(p + 0x14, kW);
    SetLong(p + 0x18, kH);
    p[4] = 0x60;
    p[5] = 0x60;
    PutFloat(p + 0xC, SD(4));
    p[6] = 0x60;
    MH_CALL(Gfx_CommitPrim)(0, 0x1C);
}

// original 0x4B7A10: the child's kind-1 task, a jmp through
// ForetellChild_Kinds (one entry, ForetellOrb_Run) by +1, unchecked.
S15_EXPORT void __cdecl ForetellChild_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("ForetellChild_Task", phase, 1);
    magic_harness::Phase(bof3::addr::ForetellOrb_Run)();
}

// original 0x4B7A30: a jmp through ForetellOrb_Steps (four entries) by +2,
// unchecked.
S15_EXPORT void __cdecl ForetellOrb_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::ForetellOrb_Start, bof3::addr::ForetellOrb_Spin,
                                                bof3::addr::ForetellOrb_Count, bof3::addr::ForetellOrb_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ForetellOrb_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B7A50: the owner's direction and position, 0x200 higher (+0x3C
// + 0x2000000); the screen point (BattleActor_UpdateScreenXY); sound 0x100;
// +0xB 0, +9 0xF, +0xA 3, +2 on.
S15_EXPORT void __cdecl ForetellOrb_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) + 0x2000000u));
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0xF;
    Sc()[0xA] = 3;
    Inc(Sc()[2]);
}

// original 0x4B7AE0: the cell drawn under the actor matrix; +9 down; at 0 +9
// 0xF and +2 on.
S15_EXPORT void __cdecl ForetellOrb_Spin(void) {
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::ForetellOrb_Draw);
    MH_CALL(Gte_PopMatrix)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 0xF;
    Inc(Sc()[2]);
}

// original 0x4B7B20: +9 down; at 0 +9 0xF and the count +0xA down: not yet 0,
// sound 0x100 and +2 back one; at 0 sound 0x101, the cell +0xB 0xC, +2 on.
S15_EXPORT void __cdecl ForetellOrb_Count(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 0xF;
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) {
        MH_CALL(Sound_PlayById)(0x100);
        Dec(Sc()[2]);
        return;
    }
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[0xB] = 0xC;
    Inc(Sc()[2]);
}

// original 0x4B7B90: the cell drawn under the actor matrix; +9 down; at 0 the
// owner's +0xB down and the task freed.
S15_EXPORT void __cdecl ForetellOrb_End(void) {
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::ForetellOrb_Draw);
    MH_CALL(Gte_PopMatrix)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B7BD0: a 24 x 24 flat-textured quad (tpage 0 / 0x3C0 / 0, CLUT
// row 0x1E0, shade 0x80) centred on the screen point (+0x2E / +0x30), u from
// +0xB (12 wide), v 0x24..0x2E; between a draw-mode packet and layer 2.
S15_EXPORT void __cdecl ForetellOrb_Draw(void) {
    DrawModeCommit(0x15, 2);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    PutFloat(p + 8, S16(Sc() + 0x2E) - 0xC);
    PutFloat(p + 0xC, S16(Sc() + 0x30) - 0xC);
    PutFloat(p + 0x18, S16(Sc() + 0x2E) + 0xC);
    PutFloat(p + 0x1C, S16(Sc() + 0x30) - 0xC);
    PutFloat(p + 0x28, S16(Sc() + 0x2E) - 0xC);
    PutFloat(p + 0x2C, S16(Sc() + 0x30) + 0xC);
    PutFloat(p + 0x38, S16(Sc() + 0x2E) + 0xC);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    PutFloat(p + 0x3C, S16(Sc() + 0x30) + 0xC);
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 0, 0x3C0, 0);
    SetWord(p + 0x26, tpage);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1E0);
    SetWord(p + 0x16, clut);
    p[0x14] = Sc()[0xB];
    p[0x15] = 0x24;
    p[0x25] = 0x24;
    p[0x24] = static_cast<unsigned char>(Sc()[0xB] + 0xB);
    p[0x34] = Sc()[0xB];
    p[0x35] = 0x2E;
    p[0x45] = 0x2E;
    p[0x44] = static_cast<unsigned char>(Sc()[0xB] + 0xB);
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// ===========================================================================
// MAGIC069 (row 75, Influence read one id down)

// original 0x4B7DE0: the kind-2 task. A two-entry stack table by +1:
// Influence_Start, MAGIC222's BattleFx_Finish.
S15_EXPORT void __cdecl Influence_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Influence_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Influence_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4B7E10: the source sprite's screen point (0x904B4C +0x2E /
// +0x30); +0xB 0, +1 on; three children (kind 1, 0x39) with +1 0 and three
// with +1 1, each with +0x80 this task, +0xB its number 0..2 and +9 one more,
// counted in +0xB; the first 48 words of CLUT row 26 back from the source; sound
// 0x100. BattleTask_Create's 0xFF is not tested.
S15_EXPORT void __cdecl Influence_Start(void) {
    {
        const unsigned char* const source = Pointer(at::kSource);
        SetWord(Sc() + 0x2E, Word(source + 0x2E));
        SetWord(Sc() + 0x30, Word(source + 0x30));
    }
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned side = 0; side < 2; ++side) {
        for (unsigned n = 0; n < 3; ++n) {
            const unsigned slot = NewTask(0x39);
            unsigned char* const child = TaskSlot(slot);
            unsigned char* const s = Sc();
            SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
            child[1] = static_cast<unsigned char>(side);
            child[0xB] = static_cast<unsigned char>(n);
            child[9] = static_cast<unsigned char>(n + 1);
            Inc(s[0xB]);
        }
    }
    for (unsigned k = 0; k < 0x10; ++k) {
        Gfx_ClutStrip[0x1A00 + k] = Gfx_ClutStripSource[0x1A00 + k];
        Gfx_ClutStrip[0x1A10 + k] = Gfx_ClutStripSource[0x1A10 + k];
        Gfx_ClutStrip[0x1A20 + k] = Gfx_ClutStripSource[0x1A20 + k];
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4B7F40: the children's kind-1 task, a jmp through
// InfluenceChild_Kinds (three entries: the right-hand ones, the left-hand ones,
// the markers) by +1, unchecked.
S15_EXPORT void __cdecl InfluenceChild_Task(void) {
    static constexpr std::uint32_t kKinds[3] = {bof3::addr::InfluenceRight_Run, bof3::addr::InfluenceLeft_Run,
                                                bof3::addr::InfluenceMark_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("InfluenceChild_Task", phase, 3);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4B7F60: a call through InfluenceRight_Steps (five entries) by
// +2; a draw-mode packet (tpage 0x35) to layer 3; with +0 bit 0 set: past step
// 0 the ring and the triangles, past step 2 the cross; a closing packet
// (0x15).
S15_EXPORT void __cdecl InfluenceRight_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::InfluenceRight_Launch, bof3::addr::InfluenceRight_Slide,
                                                bof3::addr::InfluenceRight_Pick, bof3::addr::InfluenceRight_Blink,
                                                bof3::addr::InfluenceRight_Shrink};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("InfluenceRight_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    DrawModeCommit(0x35, 3);
    const unsigned char* s = Sc();
    if (s[0] & 1) {
        if (s[2] != 0) {
            Call0(bof3::addr::Influence_DrawRing);
            Call0(bof3::addr::Influence_DrawTriangles);
            s = Sc();
        }
        if ((s[0] & 1) && s[2] > 2) Call0(bof3::addr::Influence_DrawCross);
    }
    DrawModeCommit(0x15, 3);
}

// original 0x4B7FF0: +9 down; at 0 the slide +0xC 0xB (a dword), the screen
// point 0x37 right of the owner's and 0x10 up, +9 0, +0xA 0x20, +2 on.
S15_EXPORT void __cdecl InfluenceRight_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetLong(Sc() + 0xC, 0xB);
    SetWord(Sc() + 0x2E, Word(Owner() + 0x2E) + 0x37u);
    SetWord(Sc() + 0x30, Word(Owner() + 0x30) - 0x10u);
    Sc()[9] = 0;
    Sc()[0xA] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4B8060: the slide +0xC down by 1 and the x (word +0x2E) left by
// it; at 0 the first child (+0xB 0) plays sound 0x103 and goes on, the others
// count the owner's +0xB down and free themselves.
S15_EXPORT void __cdecl InfluenceRight_Slide(void) {
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sc() + 0xC)) - 1u));
    {
        unsigned char* const s = Sc();
        SetWord(s + 0x2E, Word(s + 0x2E) - Word(s + 0xC));
    }
    if (Long(Sc() + 0xC) != 0) return;
    if (Sc()[0xB] == 0) {
        MH_CALL(Sound_PlayById)(0x103);
        Inc(Sc()[2]);
        return;
    }
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B80B0: +9 up; at 0x10 the markers spawned
// (Influence_SpawnMarks), +0xB 0, +2 on.
S15_EXPORT void __cdecl InfluenceRight_Pick(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x10) return;
    Call0(bof3::addr::Influence_SpawnMarks);
    Sc()[0xB] = 0;
    Inc(Sc()[2]);
}

// original 0x4B80E0: +9 swings - up by 4 to 0x10 while +0xB is set (then +0xB
// 0), down by 2 to 0 while it is clear (then +0xB 1); once the owner's count
// +0xB is 1 (only this child left): sound 0x101, +9 0x10, +2 on.
S15_EXPORT void __cdecl InfluenceRight_Blink(void) {
    unsigned char* const s = Sc();
    if (s[0xB] != 0) {
        AddB(s[9], 4);
        if (Sc()[9] == 0x10) Sc()[0xB] = 0;
    } else {
        AddB(s[9], 0xFE);
        if (Sc()[9] == 0) Sc()[0xB] = 1;
    }
    if (Owner()[0xB] != 1) return;
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4B8150: the ring +0xA down by 2; at 8 the owner's +0xB down and
// the task freed.
S15_EXPORT void __cdecl InfluenceRight_Shrink(void) {
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 8) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B8180: a call through InfluenceLeft_Steps (two entries) by +2;
// a draw-mode packet (tpage 0x35) to layer 3; with +0 bit 0 set and past step
// 0 the ring and the triangles; a closing packet (0x15).
S15_EXPORT void __cdecl InfluenceLeft_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::InfluenceLeft_Launch, bof3::addr::InfluenceLeft_Slide};
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("InfluenceLeft_Run", phase, 2);
    magic_harness::Phase(kSteps[phase])();
    DrawModeCommit(0x35, 3);
    const unsigned char* const s = Sc();
    if ((s[0] & 1) && s[2] != 0) {
        Call0(bof3::addr::Influence_DrawRing);
        Call0(bof3::addr::Influence_DrawTriangles);
    }
    DrawModeCommit(0x15, 3);
}

// original 0x4B81F0: as InfluenceRight_Launch, 0x37 left of the owner's point.
S15_EXPORT void __cdecl InfluenceLeft_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetLong(Sc() + 0xC, 0xB);
    SetWord(Sc() + 0x2E, Word(Owner() + 0x2E) - 0x37u);
    SetWord(Sc() + 0x30, Word(Owner() + 0x30) - 0x10u);
    Sc()[9] = 0;
    Sc()[0xA] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4B8260: the slide +0xC down by 1 and the x right by it; at 0 the
// owner's +0xB down and the task freed.
S15_EXPORT void __cdecl InfluenceLeft_Slide(void) {
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sc() + 0xC)) - 1u));
    {
        unsigned char* const s = Sc();
        SetWord(s + 0x2E, Word(s + 0x2E) + Word(s + 0xC));
    }
    if (Long(Sc() + 0xC) != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B82A0: the marker's steps, a jmp through InfluenceMark_Steps
// (five entries: _Start, _Brighten, _Fade, _Show, MAGIC058's 0x4AF490) by +2,
// unchecked.
S15_EXPORT void __cdecl InfluenceMark_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::InfluenceMark_Start, bof3::addr::InfluenceMark_Brighten,
                                                bof3::addr::InfluenceMark_Fade, bof3::addr::InfluenceMark_Show,
                                                kFreeOwnerCount};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("InfluenceMark_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
}

namespace {

// The actor +0xB names: a member's record for 0..2, else the enemy's (by the
// index - 3, unchecked).
unsigned char* MarkedRecord(unsigned index) { return index < 3 ? PartyRecord(index) : EnemyRecord(index); }

}  // namespace

// original 0x4B82C0: the actor's sprite (+0xB: a member for 0..2, else an
// enemy) tinted black, its tint slot to +0xA; the screen point 0x20 left of and
// 0xC above the sprite's, then raised by its height byte (a member's
// kPartyLift pair by +0x89, the first for +8 0 or 1; an enemy type's lift byte);
// +9 0, +2 on.
S15_EXPORT void __cdecl InfluenceMark_Start(void) {
    unsigned char* const sprite = MarkedRecord(Sc()[0xB]);
    MH_CALL(Sprite_ReleaseTint)(sprite);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(sprite, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    SetWord(Sc() + 0x2E, Word(sprite + 0x2E) - 0x20u);
    SetWord(Sc() + 0x30, Word(sprite + 0x30) - 0xCu);
    unsigned char* const s = Sc();
    const unsigned index = s[0xB];
    unsigned lift;
    if (index < 3) {
        const unsigned char* const r = PartyRecord(index);
        lift = Mem(kPartyLift + 2u * r[0x89] + (r[8] <= 1 ? 0u : 1u))[0];
    } else {
        lift = Mem(kEnemyLift + EnemyRecord(index)[0xF0] * kEnemyLiftStride)[0];
    }
    SetWord(s + 0x30, Word(s + 0x30) - lift);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4B83D0: the tint record +0xA (MoveScript_TintRecords + 12 n) one
// step brighter in each of its three channels (+2..+4); at 0x10 +2 on.
S15_EXPORT void __cdecl InfluenceMark_Brighten(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) Inc(MoveScript_TintRecords[s[0xA] * 12u + c]);
    if (MoveScript_TintRecords[s[0xA] * 12u + 2] == 0x10) Inc(s[2]);
}

// original 0x4B8440: the tint record one step darker; at 0: sound 0x102 for
// the first marker (+4 0), the tint released (Tint_Release), the actor +0xB
// flashed, +9 0, +0xA 4, +2 on.
S15_EXPORT void __cdecl InfluenceMark_Fade(void) {
    {
        unsigned char* const s = Sc();
        for (unsigned c = 2; c < 5; ++c) Dec(MoveScript_TintRecords[s[0xA] * 12u + c]);
        if (MoveScript_TintRecords[s[0xA] * 12u + 2] != 0) return;
        if (s[4] == 0) MH_CALL(Sound_PlayById)(0x102);
    }
    MH_CALL(Tint_Release)(Sc()[0xA]);
    MH_CALL(BattleActor_Flash)(Sc()[0xB]);
    Sc()[9] = 0;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x4B84F0: the quad drawn; its height +0xA up by 4 to 0x18; +9 up;
// at 0x14 +2 on.
S15_EXPORT void __cdecl InfluenceMark_Show(void) {
    Call0(bof3::addr::InfluenceMark_Draw);
    unsigned char* const s = Sc();
    if (s[0xA] != 0x18) AddB(s[0xA], 4);
    Inc(s[9]);
    if (Sc()[9] == 0x14) Inc(Sc()[2]);
}

// original 0x4B8530: a 0x30-wide flat-textured quad (tpage 0 / 0x380 / 0x100,
// CLUT 0x20 / 0x1FA, shade 0x80) from the screen point down +0xA, u 0..0x40,
// v 0x20 .. 0x20 + +0xA; after a draw-mode packet, layer 2.
S15_EXPORT void __cdecl InfluenceMark_Draw(void) {
    DrawModeCommit(0x15, 2);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    PutFloat(p + 8, S16(Sc() + 0x2E));
    PutFloat(p + 0xC, S16(Sc() + 0x30));
    PutFloat(p + 0x18, S16(Sc() + 0x2E) + 0x30);
    PutFloat(p + 0x1C, S16(Sc() + 0x30));
    PutFloat(p + 0x28, S16(Sc() + 0x2E));
    PutFloat(p + 0x2C, S16(Sc() + 0x30) + Sc()[0xA]);
    PutFloat(p + 0x38, S16(Sc() + 0x2E) + 0x30);
    PutFloat(p + 0x3C, S16(Sc() + 0x30) + Sc()[0xA]);
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 0, 0x380, 0x100);
    SetWord(p + 0x26, tpage);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0x20, 0x1FA);
    SetWord(p + 0x16, clut);
    p[0x14] = 0;
    p[0x15] = 0x20;
    p[0x24] = 0x40;
    p[0x25] = 0x20;
    p[0x34] = 0;
    p[0x35] = static_cast<unsigned char>(Sc()[0xA] + 0x20);
    p[0x44] = 0x40;
    p[0x45] = static_cast<unsigned char>(Sc()[0xA] + 0x20);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
}

namespace {

// The shade the ring and the triangles share: 0x80 - 48 x +0xB before step 2.
unsigned FadeShade(const unsigned char* s) { return 0x80u - static_cast<unsigned>(s[0xB]) * 0x30u; }

}  // namespace

// original 0x4B8690: four fans of three semi-transparent gouraud triangles
// (layer 3), one at each quarter turn: each fan's centre on the circle of
// radius +0xA about the screen point, its triangles to the points at the
// angles ((Influence_TrianglePoints[j] + 16 k) & 0x3F) << 6 at radius
// (+0xA >> 2) + 1; the shade (0x903856, 0x903858, 1) - 0x80 - 48 x +0xB twice
// before step 2, else 0x80 + 6 x +9 and 0x80 - 6 x +9.
S15_EXPORT void __cdecl Influence_DrawTriangles(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xA]);
        SetSW(2, static_cast<unsigned>(s[0xA] >> 2) + 1);
        if (s[2] < 2) {
            SetSW(6, FadeShade(s));
            SetSW(8, FadeShade(s));
        } else {
            SetSW(6, static_cast<unsigned>(s[9]) * 6 + 0x80);
            SetSW(8, 0x80u - static_cast<unsigned>(s[9]) * 6);
        }
    }
    const unsigned char* const points = Mem(kInfluencePoints);
    for (int k = 0, angle = 0; angle < 0x1000; ++k, angle += 0x400) {
        int v = MH_CALL(Math_Sin)(angle);
        SetSW(0xC, static_cast<unsigned>(Mul12(v, SS(0))) + Word(Sc() + 0x2E));
        v = MH_CALL(Math_Cos)(angle);
        SetSW(0xE, static_cast<unsigned>(Mul12(v, SS(0))) + Word(Sc() + 0x30));
        const unsigned turn = static_cast<unsigned>(k) << 4;
        for (unsigned j = 0; j < 3; ++j) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG3)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            PutFloat(p + 8, SS(0xC));
            PutFloat(p + 0xC, SS(0xE));
            {
                const auto a = static_cast<short>(((points[j] + turn) & 0x3Fu) << 6);
                SetSW(4, static_cast<unsigned short>(a));
                v = MH_CALL(Math_Sin)(a);
                PutFloat(p + 0x18, Mul12(v, SS(2)) + SS(0xC));
                v = MH_CALL(Math_Cos)(SS(4));
                PutFloat(p + 0x1C, Mul12(v, SS(2)) + SS(0xE));
            }
            {
                const auto a = static_cast<short>(((points[j + 1] + turn) & 0x3Fu) << 6);
                SetSW(4, static_cast<unsigned short>(a));
                v = MH_CALL(Math_Sin)(a);
                PutFloat(p + 0x28, Mul12(v, SS(2)) + SS(0xC));
                v = MH_CALL(Math_Cos)(SS(4));
                PutFloat(p + 0x2C, Mul12(v, SS(2)) + SS(0xE));
            }
            for (unsigned c : {4u, 0x14u, 0x24u}) {
                p[c] = SB(6);
                p[c + 1] = SB(8);
                p[c + 2] = 1;
            }
            MH_CALL(Gfx_CommitPrim)(3, 0x34);
        }
    }
}

// original 0x4B8910: a dashed ring - 32 of 64 segments (those whose index has
// bit 2 or bit 3 set but not both) of radius +0xA about the screen point, each
// a semi-transparent flat line (layer 3) shaded 0x80 - 48 x +0xB before step
// 2, else 0x80.
S15_EXPORT void __cdecl Influence_DrawRing(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xA]);
        SetSW(0xC, Word(s + 0x2E));
        SetSW(0xE, Word(s + 0x30));
        SetSW(6, s[2] < 2 ? FadeShade(s) : 0x80u);
    }
    for (unsigned i = 0; i < 0x40; ++i) {
        const unsigned quarter = i & 0xC;
        if (quarter != 4 && quarter != 8) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const auto a = static_cast<short>(i << 6);
            SetSW(4, static_cast<unsigned short>(a));
            int v = MH_CALL(Math_Sin)(a);
            PutFloat(p + 8, Mul12(v, SS(0)) + SS(0xC));
            v = MH_CALL(Math_Cos)(SS(4));
            PutFloat(p + 0xC, Mul12(v, SS(0)) + SS(0xE));
        }
        {
            const auto a = static_cast<short>((i + 1) << 6);
            SetSW(4, static_cast<unsigned short>(a));
            int v = MH_CALL(Math_Sin)(a);
            PutFloat(p + 0x14, Mul12(v, SS(0)) + SS(0xC));
            v = MH_CALL(Math_Cos)(SS(4));
            PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(0xE));
        }
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = SB(6);
        MH_CALL(Gfx_CommitPrim)(3, 0x20);
    }
}

// original 0x4B8A90: a cross at the screen point - two semi-transparent flat
// lines 8 long (layer 3), horizontal then vertical, shaded 0x80.
S15_EXPORT void __cdecl Influence_DrawCross(void) {
    const unsigned char* const s = Sc();
    SetSW(6, 0x80);
    unsigned char* p = Gfx_PacketNext;
    SetSW(0xC, Word(s + 0x2E));
    SetSW(0xE, Word(s + 0x30));
    MH_CALL(Gpu_SetLineF2)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, SS(0xC) - 4);
    PutFloat(p + 0xC, SS(0xE));
    PutFloat(p + 0x14, SS(0xC) + 4);
    PutFloat(p + 0x18, SS(0xE));
    p[4] = SB(6);
    p[5] = SB(6);
    p[6] = SB(6);
    MH_CALL(Gfx_CommitPrim)(3, 0x20);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetLineF2)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, SS(0xC));
    PutFloat(p + 0xC, SS(0xE) + 4);
    PutFloat(p + 0x14, SS(0xC));
    PutFloat(p + 0x18, SS(0xE) - 4);
    p[4] = SB(6);
    p[5] = SB(6);
    p[6] = SB(6);
    MH_CALL(Gfx_CommitPrim)(3, 0x20);
}

namespace {

// A marker (kind 1, 0x39, +1 2) for actor `index`: +0x80 the owner, +4 the
// markers so far, +0xB the actor; the owner's +0xB up.
void SpawnMark(unsigned index, unsigned& count) {
    const unsigned slot = NewTask(0x39);
    unsigned char* const child = TaskSlot(slot);
    unsigned char* const owner = Owner();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(owner)));
    child[1] = 2;
    child[4] = static_cast<unsigned char>(count);
    child[0xB] = static_cast<unsigned char>(index);
    ++count;
    Inc(owner[0xB]);
}

}  // namespace

// original 0x4B8BD0: a marker for every enemy, then every member, that is not
// out, not the acting actor and not the target, and whose record passes: an
// enemy's word +0xBA at most 1, or its +0x92 bit 0x20, or its dword +0x114 bit
// 0x4000; a member's word +0xAA at most 1, or +0x90 bit 0x20, or its dword
// +0x134 bit 0x4000 or bit 0. BattleTask_Create's 0xFF is not tested.
S15_EXPORT void __cdecl Influence_SpawnMarks(void) {
    unsigned count = 0;
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned index = i + 3;
        if (IsOut(index)) continue;
        if (ActorIndex() == index) continue;
        if (TargetIndex() == index) continue;
        const unsigned char* const e = EnemyRecord(index);
        if (Word(e + 0xBA) > 1 && (e[0x92] & 0x20) == 0 && (Long(e + 0x114) & 0x4000) == 0) continue;
        SpawnMark(index, count);
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (IsOut(i)) continue;
        if (ActorIndex() == i) continue;
        if (TargetIndex() == i) continue;
        const unsigned char* const r = PartyRecord(i);
        const std::uint32_t bits = static_cast<std::uint32_t>(Long(r + 0x134));
        if (Word(r + 0xAA) > 1 && (r[0x90] & 0x20) == 0 && (bits & 0x4000) == 0 && (bits & 1) == 0) continue;
        SpawnMark(i, count);
    }
}

void MagicS15_Inject() {
    if (bof3::WantsShadow("magic_s15")) magic_s15::SelfTest();
    BOF3_INJECT(Chill_Task);
    BOF3_INJECT(Chill_Start);
    BOF3_INJECT(Chill_WaitChildren);
    BOF3_INJECT(Chill_SpawnMarks);
    BOF3_INJECT(ChillChild_Task);
    BOF3_INJECT(ChillRay_Run);
    BOF3_INJECT(ChillRay_Start);
    BOF3_INJECT(ChillRay_Grow);
    BOF3_INJECT(ChillRay_Shrink);
    BOF3_INJECT(ChillRay_PushMatrix);
    BOF3_INJECT(ChillRay_Draw);
    BOF3_INJECT(ChillFlash_Run);
    BOF3_INJECT(ChillFlash_Draw);
    BOF3_INJECT(ChillMark_Run);
    BOF3_INJECT(ChillEnemy_Task);
    BOF3_INJECT(ChillEnemy_Start);
    BOF3_INJECT(Foretell_Task);
    BOF3_INJECT(Foretell_Start);
    BOF3_INJECT(Foretell_Read);
    BOF3_INJECT(Foretell_Pause);
    BOF3_INJECT(Foretell_Show);
    BOF3_INJECT(Foretell_End);
    BOF3_INJECT(Foretell_DrawTiles);
    BOF3_INJECT(ForetellChild_Task);
    BOF3_INJECT(ForetellOrb_Run);
    BOF3_INJECT(ForetellOrb_Start);
    BOF3_INJECT(ForetellOrb_Spin);
    BOF3_INJECT(ForetellOrb_Count);
    BOF3_INJECT(ForetellOrb_End);
    BOF3_INJECT(ForetellOrb_Draw);
    BOF3_INJECT(Influence_Task);
    BOF3_INJECT(Influence_Start);
    BOF3_INJECT(InfluenceChild_Task);
    BOF3_INJECT(InfluenceRight_Run);
    BOF3_INJECT(InfluenceRight_Launch);
    BOF3_INJECT(InfluenceRight_Slide);
    BOF3_INJECT(InfluenceRight_Pick);
    BOF3_INJECT(InfluenceRight_Blink);
    BOF3_INJECT(InfluenceRight_Shrink);
    BOF3_INJECT(InfluenceLeft_Run);
    BOF3_INJECT(InfluenceLeft_Launch);
    BOF3_INJECT(InfluenceLeft_Slide);
    BOF3_INJECT(InfluenceMark_Run);
    BOF3_INJECT(InfluenceMark_Start);
    BOF3_INJECT(InfluenceMark_Brighten);
    BOF3_INJECT(InfluenceMark_Fade);
    BOF3_INJECT(InfluenceMark_Show);
    BOF3_INJECT(InfluenceMark_Draw);
    BOF3_INJECT(Influence_DrawTriangles);
    BOF3_INJECT(Influence_DrawRing);
    BOF3_INJECT(Influence_DrawCross);
    BOF3_INJECT(Influence_SpawnMarks);
}

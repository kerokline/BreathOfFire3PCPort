// Two spell overlays compiled into the exe, round nine group S04
// (docs/magic_s04.md): the PSX's MAGIC013 and MAGIC015.EMI (MAGIC016 folded
// into MAGIC015 by the linker), Magic_Rows rows 50, 4, 7, 55 and 58. Read one
// id down (docs/cut-content.md section 2) the sibling labels them Snap, Charge,
// Flying Kick and Air Raid; the names below use those labels as hypotheses,
// and say what the code does.
//
//   - MAGIC013 0x49E9F0..0x49FA66 (row 50): a wave child (kind 1, 0x29) that
//     swings out and back from the owner, drawing two tilted rings of gouraud
//     quads, and at each turn bursts up to 32 sparks out of a 64-record pool of
//     the overlay's own (flat triangles thrown on arcs); a buff child (kind 1,
//     0x48) when MagicFx_ApplyBuff says the target takes it;
//   - MAGIC015 0x49FA70..0x4A11D8 (rows 4, 7, 55, 58): copies of the acting
//     actor's record as kind-1 children (0x23) - Charge's four trails and an
//     image that dashes to the source sprite and springs back, Air Raid's three
//     images that rise, turn, dive and bounce, Flying Kick's one image that
//     rises and dives - each with a shadow of eight subtractive triangles while
//     it is in the air.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table (a stack table, or a .data table read in place) aborts where
// the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent), and the two wave
// matrices abort on a facing past 3, where the original's four-entry jump
// table leaves two angles as uninitialised stack words (the precedent of
// group S31's CoronaRay_PushMatrix and S25's SpellConfuse_PushFacingMatrix).
#include "game/magic_s04.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF), read again after every call as the
// originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kActorSprite = 0x904B3C;   // unsigned char *: the acting actor's sprite record
constexpr std::uint32_t kPoolStride = 0x84;
constexpr unsigned kPoolRecords = 64;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Source() { return Pointer(at::kSource); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char ActorByte() { return Mem(at::kActor)[0]; }

std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }
short SS(unsigned k) { return static_cast<short>(Word(Mem(kS + k))); }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
void AddL(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}
void AddW(unsigned char* at, unsigned v) { SetWord(at, (Word(at) + v) & 0xFFFF); }
void CopyL(unsigned char* to, const unsigned char* from) { SetLong(to, Long(from)); }
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl l` then `sar r` on a dword.
int ShlSar(int v, unsigned l, unsigned r) { return static_cast<int>(static_cast<std::uint32_t>(v) << l) >> r; }
// `cdq / sub eax, edx / sar 1`: a signed halving toward zero.
int Half(int v) { return (v - (v >> 31)) >> 1; }

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* PoolRecord(unsigned i) { return Mem(Addr(SnapSpark_Pool) + i * kPoolStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions called directly, and the other units' phases a
// stack table holds, by address, as the originals call them: in the game
// Capcom's code or the jmp Inject put there, in the fuzz that address's
// recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Capcom's code this group calls (docs/magic_s04.md section 3); other groups'
// phase handlers (S35's LastResort_WaitChildren, S05's Magic018Row53_Wait,
// S11's MagicFx_UncountAndFree) are ours now and named in the tables.
constexpr std::uint32_t kTurnOffset = bof3::addr::Battle_TurnVectorC;       // engine: +0xC / +0x10 of a task turned by its +8
constexpr std::uint32_t kPolyF3 = bof3::addr::Gpu_SetPolyF3;           // libgpu SetPolyF3 by shape (code 0x20), unnamed
using TaskFn = void (__cdecl*)(unsigned char*);
using PrimFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}
// A .data table's call or jmp, `[table + phase * 4]`, read in place (the fuzz
// swaps the cells for recorders); the table's own entries only - what follows
// is the next table or data.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The GTE callees with the arguments the originals push: Gte_RotTrans a flag
// pointer more than it takes, the projections a depth and a flag pointer.
using RotTransFn = void (__cdecl*)(const short*, long*, long*);
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S04_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Vertices 0..2 of Prim_VertexScratch projected to prim + 8, + 8 + step,
// + 8 + 2 step; vertices 0..3 to four.
void Rtp3(unsigned char* prim, unsigned step) {
    long p, flag;
    S04_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 8 + step, prim + 8 + 2 * step, &p, &flag);
}
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S04_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                size);
}

// The matrix pushes' common tail (MagicFx_PushActorMatrix's shape): the
// translation RotTrans of `v`, the rotation of `angles`, Camera_Matrix x it,
// both set. One MATRIX block as the original lays it out on its stack,
// RotTrans writing its translation at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
void SetMatrix(const short* angles, const short* v) {
    Matrix m;
    long flag;
    S04_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(angles, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}
// ((+0x34 sar 9) - 0x4000, (+0x38 sar 9) - 0x4000, height): the task's point in
// the GTE's units.
void GroundPoint(const unsigned char* s, short height, short* v) {
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = height;
    v[3] = 0;
}

// The acting actor's record as the originals index it (a party member at
// 0..2, else the enemy by index - 3, unchecked).
unsigned char* ActorRecord(unsigned char a) {
    if (a < 3) return Mem(at::kParty + a * at::kPartyStride);
    return Mem(at::kEnemies + (a - 3u) * at::kEnemyStride);
}
// A kind-1 child of parameter 0x23 made a copy of the acting actor's record's
// first 0x80 bytes (`rep movsd`, forward): MAGIC015's images.
unsigned char* ImageChild() {
    const unsigned slot = NewTask(0x23);
    const unsigned char* const from = ActorRecord(ActorByte());
    unsigned char* const to = TaskSlot(slot);
    for (unsigned k = 0; k < 0x80; k += 4) CopyL(to + k, from + k);
    return to;
}
// The owner's point (+0x34, +0x38, +0x3C) copied to the task.
void ToOwnerPoint() {
    CopyL(Sc() + 0x34, Owner() + 0x34);
    CopyL(Sc() + 0x38, Owner() + 0x38);
    CopyL(Sc() + 0x3C, Owner() + 0x3C);
}
// +0x27 SpriteClut_CopyToFxRow(owner), +0x28 / +0x24 the owner's.
void TakeOwnerClut() {
    const unsigned row = MH_CALL(SpriteClut_CopyToFxRow)(Owner());
    Sc()[0x27] = static_cast<unsigned char>(row);
    Sc()[0x28] = Owner()[0x28];
    Sc()[0x24] = Owner()[0x24];
}

}  // namespace

#define S04_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC013 (row 50, Snap read one id down)

namespace {

// Every live record of SnapSpark_Pool run through SnapSpark_Task as
// Sprite_Current, its +0x80 the owner cell; both put back after each. The task
// and the owner are read after the phase call that precedes the walk.
void WalkPool() {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(bof3::addr::SnapSpark_Task);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = self;
    }
}

}  // namespace

// original 0x49E9F0: the kind-2 task. A four-entry stack table by +1:
// Snap_Start, MAGIC167's 0x4EF7C0, Snap_Buff, MagicFx_EndWhenChildrenDone;
// then the spark pool walked.
S04_EXPORT void __cdecl Snap_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Snap_Start, bof3::addr::LastResort_WaitChildren,
                                                 bof3::addr::Snap_Buff, bof3::addr::MagicFx_EndWhenChildrenDone};
    StackCall(kPhases, 4, Sc()[1], "Snap_Task");
    WalkPool();
}

// original 0x49EA80: the spark pool's +0..+2 cleared; the task on the source
// sprite; row 26's first two CLUTs back from their source (no STP bit); the
// wave (kind 1, 0x29) with +0x80 this task; +0xB 2, +1 on.
S04_EXPORT void __cdecl Snap_Start(void) {
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    const unsigned char* const src = Source();
    Sc()[8] = src[8];
    CopyL(Sc() + 0x34, src + 0x34);
    CopyL(Sc() + 0x38, src + 0x38);
    CopyL(Sc() + 0x3C, src + 0x3C);
    for (unsigned k = 0x1A00; k < 0x1A10; ++k) {
        Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
        Gfx_ClutStrip[k + 0x10] = Gfx_ClutStripSource[k + 0x10];
    }
    Gfx_ClutStripDirty = 1;
    const unsigned slot = NewTask(0x29);
    unsigned char* const s = Sc();
    SetLong(TaskSlot(slot) + 0x80, static_cast<std::int32_t>(Addr(s)));
    s[0xB] = 2;
    Inc(Sc()[1]);
}

// original 0x49EB40: when the target is not out and MagicFx_ApplyBuff(1,
// target) takes, a child (kind 1, 0x48) with +4 4, +9 0x18, +0xA 4 and this
// task's +0xB up; +1 on.
S04_EXPORT void __cdecl Snap_Buff(void) {
    if (MH_CALL(Battle_ActorIsOut)(TargetByte()) == 0 && MH_CALL(MagicFx_ApplyBuff)(1, TargetByte()) != 0) {
        const unsigned slot = NewTask(0x48);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(s)));
        child[4] = 4;
        child[9] = 0x18;
        child[0xA] = 4;
        Inc(s[0xB]);
    }
    Inc(Sc()[1]);
}

// original 0x49EBC0: the wave's kind-1 task, a jmp through SnapWave_TaskTable
// (one entry) by +1.
S04_EXPORT void __cdecl SnapWave_Task(void) { CellCall(Addr(SnapWave_TaskTable), 1, Sc()[1], "SnapWave_Task"); }

// original 0x49EBE0: a call through SnapWave_Steps (five entries) by +2; then
// while +0 is set the two rings, each under its own matrix.
S04_EXPORT void __cdecl SnapWave_Run(void) {
    CellCall(Addr(SnapWave_Steps), 5, Sc()[2], "SnapWave_Run");
    if (Sc()[0] == 0) return;
    Call0(bof3::addr::SnapWave_PushMatrixA);
    Call0(bof3::addr::SnapWave_DrawRingA);
    MH_CALL(Gte_PopMatrix)();
    Call0(bof3::addr::SnapWave_PushMatrixB);
    Call0(bof3::addr::SnapWave_DrawRingB);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x49EC20: the owner's direction; the offset (0x20000, 0) turned,
// from the owner's point, 0x1000000 above it; the step (-0x6000, 0) turned; +4
// and +0xB 0, +9 0x10, +0xA 4 (the swings), +2 on.
S04_EXPORT void __cdecl SnapWave_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0xC, 0x20000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x34)) +
                                                   static_cast<std::uint32_t>(Long(Sc() + 0xC))));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x38)) +
                                                   static_cast<std::uint32_t>(Long(Sc() + 0x10))));
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) + 0x1000000u));
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(0xFFFFA000u));
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    Sc()[4] = 0;
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x49ECF0: the point on by the step, +9 down by 4; at 0 up to 32
// sparks from the pool (each +0x80 this task, +0 or 0x41, +0xB its number, +9
// Rand & 7 + 1; +0xB up for each), the target flagged 0x10 on the first swing
// (+0xA 4), the step (0x3000, 0) turned, sound 0x100, +2 on.
S04_EXPORT void __cdecl SnapWave_Burst(void) {
    {
        unsigned char* const s = Sc();
        AddL(s + 0x34, static_cast<std::uint32_t>(Long(s + 0xC)));
        AddL(s + 0x38, static_cast<std::uint32_t>(Long(s + 0x10)));
        AddB(s[9], 0xFC);
    }
    if (Sc()[9] != 0) return;
    for (unsigned i = 0; i < 0x20; ++i) {
        const unsigned k = MH_AT(ByteFn, bof3::addr::SnapSpark_Alloc)() & 0xFFu;
        if (k == 0xFF) continue;
        unsigned char* const rec = PoolRecord(k);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Addr(Sc())));
        rec[0] = static_cast<unsigned char>(rec[0] | 0x41);
        rec[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        rec[9] = static_cast<unsigned char>((r & 7) + 1);
        Inc(Sc()[0xB]);
    }
    if (Sc()[0xA] == 4) MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    SetLong(Sc() + 0xC, 0x3000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    MH_CALL(Sound_PlayById)(0x100);
    Inc(Sc()[2]);
}

// original 0x49EDF0: the point on by the step, +9 up by 2; at 0x10 (0xC on the
// last swing) +0xA down: at 0 the target flagged 0x40, sound 0x201, the
// owner's +0xB down, +2 on; else the step (-0x6000, 0) turned and +2 back to
// SnapWave_Burst.
S04_EXPORT void __cdecl SnapWave_Swing(void) {
    {
        unsigned char* const s = Sc();
        AddL(s + 0x34, static_cast<std::uint32_t>(Long(s + 0xC)));
        AddL(s + 0x38, static_cast<std::uint32_t>(Long(s + 0x10)));
    }
    unsigned char* s = Sc();
    const unsigned limit = s[0xA] == 1 ? 0xCu : 0x10u;
    AddB(s[9], 2);
    s = Sc();
    if (s[9] != limit) return;
    Dec(s[0xA]);
    s = Sc();
    if (s[0xA] == 0) {
        MH_CALL(Battle_SetTargetFlag40)(TargetByte());
        MH_CALL(Sound_PlayById)(0x201);
        Dec(Owner()[0xB]);
        Inc(Sc()[2]);
        return;
    }
    SetLong(s + 0xC, static_cast<std::int32_t>(0xFFFFA000u));
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    Dec(Sc()[2]);
}

// original 0x49EEB0: once the sparks are gone (+0xB 0), +4 down; at 0 the
// owner's +0xB down and the task freed.
S04_EXPORT void __cdecl SnapWave_End(void) {
    unsigned char* const s = Sc();
    if (s[0xB] != 0) return;
    Dec(s[4]);
    if (Sc()[4] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49EEE0: the lower ring's matrix: tipped by +9 about x or y by the
// facing +8 (a four-entry jump table), at the task's point, (+0x3E + 0x20) / 2
// up.
S04_EXPORT void __cdecl SnapWave_PushMatrixA(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned char facing = s[8];
    const int b = s[9];
    short angles[4] = {0, 0, 0, 0};
    switch (facing) {
    case 0: angles[1] = static_cast<short>((0x40 - b) << 6); break;
    case 1: angles[0] = static_cast<short>(b << 6); break;
    case 2: angles[1] = static_cast<short>(b << 6); break;
    case 3: angles[0] = static_cast<short>((0x40 - b) << 6); break;
    default: bof3::Fatal("SnapWave_PushMatrixA: facing %u past the four-entry jump table", facing);
    }
    short v[4];
    GroundPoint(s, static_cast<short>(-Half(S16(s + 0x3E) + 0x20)), v);
    SetMatrix(angles, v);
}

// original 0x49F000: the upper ring's matrix: tipped the other way (by 0x40 -
// +9 where A tips by +9), (+0x3E) / 2 up.
S04_EXPORT void __cdecl SnapWave_PushMatrixB(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const unsigned char facing = s[8];
    const int b = s[9];
    short angles[4] = {0, 0, 0, 0};
    switch (facing) {
    case 0: angles[1] = static_cast<short>(b << 6); break;
    case 1: angles[0] = static_cast<short>((0x40 - b) << 6); break;
    case 2: angles[1] = static_cast<short>((0x40 - b) << 6); break;
    case 3: angles[0] = static_cast<short>(b << 6); break;
    default: bof3::Fatal("SnapWave_PushMatrixB: facing %u past the four-entry jump table", facing);
    }
    short v[4];
    GroundPoint(s, static_cast<short>(-Half(S16(s + 0x3E))), v);
    SetMatrix(angles, v);
}

namespace {

// SnapWave_DrawRingA / B: sixteen semi-transparent gouraud quads round a
// ring whose radius swells with a sine, each from the previous rim point to
// this one, its second edge `dz` away (z - 0x50 or z + 0x50); shade +4 x 12 at
// the far edge, 1 at the near. The previous point lives in vertex 3
// (0x9037B8..0x9037BD) from one quad to the next.
void DrawRing(int dz) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(0xC);
    SetSD(8, Sc()[4] * 12u);
    {
        const int r = Sin(0);
        const unsigned char* const s = Sc();
        SetSD(0, static_cast<std::uint32_t>(ShlSar(r, 6, 12)));
        const unsigned a = static_cast<unsigned>(Mem(Addr(SnapWave_FacingPhase) + s[8])[0]) << 7;
        SetSD(4, a);
        SetVW(0x18, static_cast<unsigned>(ShlSar(Sin(static_cast<int>(a)), 7, 12)));
        SetVW(0x1A, static_cast<unsigned>(ShlSar(Cos(SD(4)), 7, 12)));
        const int z = Sin(0);
        SetVW(0x1C, static_cast<unsigned>(Mul12(z, SD(0))));
    }
    for (unsigned i = 1, angle = 0x80; angle < 0x880; ++i, angle += 0x80) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const int r = Sin(static_cast<int>(angle));
            const unsigned char* const s = Sc();
            SetSD(0, static_cast<std::uint32_t>(ShlSar(r, 6, 12)));
            const unsigned phase = Mem(Addr(SnapWave_FacingPhase) + s[8])[0];
            const std::uint16_t x = VW(0x18), y = VW(0x1A), z = VW(0x1C);
            SetVW(2, y);
            SetVW(0, x);
            SetVW(4, static_cast<unsigned>(z + dz));
            SetVW(0x10, x);
            SetVW(0x12, y);
            SetVW(0x14, z);
            const unsigned a = ((phase + i) & 0x1F) << 7;
            SetSD(4, a);
        }
        SetVW(0x18, static_cast<unsigned>(ShlSar(Sin(SD(4)), 7, 12)));
        SetVW(0x1A, static_cast<unsigned>(ShlSar(Cos(SD(4)), 7, 12)));
        {
            const int r = Sin(static_cast<int>((i & 3) << 10));
            SetVW(0x1C, static_cast<unsigned>(Mul12(r, SD(0))));
            SetVW(8, VW(0x18));
            SetVW(0xA, VW(0x1A));
            SetVW(0xC, static_cast<unsigned>(VW(0x1C) + dz));
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(8);
        LinkAtSprite(0x44);
    }
}

}  // namespace

// original 0x49F120: the lower ring (the second edge 0x50 below).
S04_EXPORT void __cdecl SnapWave_DrawRingA(void) { DrawRing(-0x50); }

// original 0x49F3A0: the upper ring (the second edge 0x50 above).
S04_EXPORT void __cdecl SnapWave_DrawRingB(void) { DrawRing(0x50); }

// original 0x49F620: a spark record's task, a jmp through SnapSpark_TaskTable
// (one entry) by +1.
S04_EXPORT void __cdecl SnapSpark_Task(void) { CellCall(Addr(SnapSpark_TaskTable), 1, Sc()[1], "SnapSpark_Task"); }

// original 0x49F640: a call through SnapSpark_Steps (two entries) by +2; then
// while +0 and +9 are set the spark under its matrix.
S04_EXPORT void __cdecl SnapSpark_Run(void) {
    CellCall(Addr(SnapSpark_Steps), 2, Sc()[2], "SnapSpark_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[9] == 0) return;
    Call0(bof3::addr::SnapSpark_PushMatrix);
    Call0(bof3::addr::SnapSpark_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x49F680: +9 down; at 0 the spark starts on the source sprite,
// 0x80 out along its angle +0xB x 0x80; +0x14 (the rise) Rand & 7 + 0x80, +0x20
// (its fall) -12, +9 0x10, +2 on.
S04_EXPORT void __cdecl SnapSpark_Launch(void) {
    Dec(Sc()[9]);
    const unsigned char* const s = Sc();
    if (s[9] != 0) return;
    const unsigned a = static_cast<unsigned>(s[0xB]) << 7;
    const unsigned char* const src = Source();
    SetSD(4, a);
    {
        const int r = Sin(static_cast<int>(a));
        SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(ShlSar(r, 8, 9)) +
                                                       static_cast<std::uint32_t>(Long(src + 0x34))));
    }
    {
        const int r = Cos(SD(4));
        SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(ShlSar(r, 8, 9)) +
                                                       static_cast<std::uint32_t>(Long(src + 0x38))));
    }
    CopyL(Sc() + 0x3C, src + 0x3C);
    const std::uint32_t r = RandCall();
    SetLong(Sc() + 0x14, static_cast<std::int32_t>((r & 7) + 0x80));
    SetLong(Sc() + 0x20, -12);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x49F730: out along the angle by 0x200 a frame (x through the task
// read before Math_Sin, z before Math_Cos), up by +0x14 which falls by +0x20;
// +9 down, at 0 the owner's +0xB down and +0..+4 cleared (the record free).
S04_EXPORT void __cdecl SnapSpark_Fly(void) {
    {
        unsigned char* const s = Sc();
        const unsigned a = static_cast<unsigned>(s[0xB]) << 7;
        unsigned char* const x = s + 0x34;
        SetSD(4, a);
        const int r = Sin(static_cast<int>(a));
        AddL(x, static_cast<std::uint32_t>(ShlSar(r, 10, 9)));
    }
    {
        unsigned char* const z = Sc() + 0x38;
        const int r = Cos(SD(4));
        AddL(z, static_cast<std::uint32_t>(ShlSar(r, 10, 9)));
    }
    {
        unsigned char* const s = Sc();
        AddL(s + 0x14, static_cast<std::uint32_t>(Long(s + 0x20)));
    }
    {
        unsigned char* const s = Sc();
        AddW(s + 0x3E, Word(s + 0x14));
    }
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x49F7F0: the spark's matrix, turned about z by +9 x 0x40, at its
// point, +0x3E / 2 up.
S04_EXPORT void __cdecl SnapSpark_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const short angles[4] = {0, 0, static_cast<short>(s[9] << 6), 0};
    short v[4];
    GroundPoint(s, static_cast<short>(-Half(S16(s + 0x3E))), v);
    SetMatrix(angles, v);
}

// original 0x49F8A0: one semi-transparent flat triangle of radius 16 (corners
// at 0, 0x555, 0xAAA), its colour three (Rand & 0xF) x +9 bytes; tpage 0x35;
// both linked at the task.
S04_EXPORT void __cdecl SnapSpark_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_AT(PrimFn, kPolyF3)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetVW(0, static_cast<unsigned>(ShlSar(Sin(0), 4, 12)));
    SetVW(2, static_cast<unsigned>(ShlSar(Cos(0), 4, 12)));
    SetVW(4, 0);
    SetVW(8, static_cast<unsigned>(ShlSar(Sin(0x555), 4, 12)));
    {
        const int r = Cos(0x555);
        SetVW(0xC, 0);
        SetVW(0xA, static_cast<unsigned>(ShlSar(r, 4, 12)));
    }
    SetVW(0x10, static_cast<unsigned>(ShlSar(Sin(0xAAA), 4, 12)));
    SetVW(0x12, static_cast<unsigned>(ShlSar(Cos(0xAAA), 4, 12)));
    SetVW(0x14, 0);
    Rtp3(p, 0xC);
    MH_CALL(Gte_PrimDepths3_0C)(p);
    for (unsigned k = 4; k < 7; ++k) {
        const std::uint32_t r = RandCall();
        p[k] = static_cast<unsigned char>((r & 0xF) * Sc()[9]);
    }
    LinkAtSprite(0x2C);
}

// original 0x49FA10: the first free record of SnapSpark_Pool (bit 0 clear)
// taken (bit 0 set) and its index answered in al; 0xFF when all 64 are live.
S04_EXPORT unsigned char __cdecl SnapSpark_Alloc(void) {
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if ((rec[0] & 1) == 0) {
            rec[0] = static_cast<unsigned char>(rec[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC015 (rows 4, 7, 55 and MAGIC016's 58: Charge, Air Raid, Flying Kick
// read one id down)

// original 0x49FA70: the kind-2 task. A four-entry stack table by +1:
// Charge_Start, MAGIC018/019's 0x4A1EC0, MagicFx_DoneAndFree,
// MagicFx_FlagTargetEnd.
S04_EXPORT void __cdecl Charge_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Charge_Start, bof3::addr::Magic018Row53_Wait,
                                                 bof3::addr::MagicFx_DoneAndFree, bof3::addr::MagicFx_FlagTargetEnd};
    StackCall(kPhases, 4, Sc()[1], "Charge_Task");
}

// original 0x49FAB0: the task on the owner; +0xB and +9 0, +1 on; the owner's
// animation 8; four trail images (+1 1, +9 4..1, +0xB 3..0) and one dashing
// image (+1 0), +0xB up for each; the owner hidden (+0 bit 0x40) and its
// palette taken for the images.
S04_EXPORT void __cdecl Charge_Start(void) {
    Sc()[8] = Owner()[8];
    CopyL(Sc() + 0x34, Owner() + 0x34);
    CopyL(Sc() + 0x38, Owner() + 0x38);
    CopyL(Sc() + 0x3C, Owner() + 0x3C);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(8, 2);
    for (unsigned n = 0; n < 4; ++n) {
        unsigned char* const child = ImageChild();
        unsigned char* const s = Sc();
        const auto back = static_cast<unsigned char>(n ^ 3);
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(s)));
        child[1] = 1;
        child[2] = 0;
        child[6] = 1;
        child[5] = 0x23;
        child[9] = static_cast<unsigned char>(back + 1);
        child[0xB] = back;
        Inc(s[0xB]);
    }
    {
        unsigned char* const child = ImageChild();
        unsigned char* const s = Sc();
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(s)));
        child[1] = 0;
        child[2] = 0;
        child[6] = 1;
        child[5] = 0x23;
        Inc(s[0xB]);
    }
    Owner()[0] = static_cast<unsigned char>(Owner()[0] | 0x40);
    TakeOwnerClut();
    MH_CALL(SpriteClut_SetStp)(Sc());
}

// original 0x49FCF0: the kind-2 task. A four-entry stack table by +1:
// AirRaid_Start, 0x4A1EC0, MagicFx_DoneAndFree, MagicFx_FlagTargetEnd.
S04_EXPORT void __cdecl AirRaid_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::AirRaid_Start, bof3::addr::Magic018Row53_Wait,
                                                 bof3::addr::MagicFx_DoneAndFree, bof3::addr::MagicFx_FlagTargetEnd};
    StackCall(kPhases, 4, Sc()[1], "AirRaid_Task");
}

// original 0x49FD30: the task on the owner, its palette taken; the owner's
// animation 8; three images (+1 2, +0xB 0..2, +9 1, 3, 5), +0xB up for each;
// sound 0x100; the owner hidden; +1 on.
S04_EXPORT void __cdecl AirRaid_Start(void) {
    Sc()[8] = Owner()[8];
    CopyL(Sc() + 0x34, Owner() + 0x34);
    CopyL(Sc() + 0x38, Owner() + 0x38);
    CopyL(Sc() + 0x3C, Owner() + 0x3C);
    TakeOwnerClut();
    MH_CALL(SpriteClut_SetStp)(Sc());
    MH_CALL(SpriteClut_ClearEntry31)(Sc());
    MH_CALL(BattleActor_SetAnimation)(8, 2);
    Sc()[0xB] = 0;
    unsigned char number = 0;
    for (unsigned char delay = 1; delay < 7; delay = static_cast<unsigned char>(delay + 2)) {
        unsigned char* const child = ImageChild();
        unsigned char* const s = Sc();
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(s)));
        child[1] = 2;
        child[2] = 0;
        child[6] = 1;
        child[5] = 0x23;
        child[0xB] = number;
        child[9] = delay;
        Inc(s[0xB]);
        ++number;
    }
    MH_CALL(Sound_PlayById)(0x100);
    Owner()[0] = static_cast<unsigned char>(Owner()[0] | 0x40);
    Inc(Sc()[1]);
}

// original 0x49FEE0: the kind-2 task. A four-entry stack table by +1:
// FlyingKick_Start, FlyingKick_End, MagicFx_DoneAndFree, MagicFx_FlagTargetEnd.
S04_EXPORT void __cdecl FlyingKick_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::FlyingKick_Start, bof3::addr::FlyingKick_End,
                                                 bof3::addr::MagicFx_DoneAndFree, bof3::addr::MagicFx_FlagTargetEnd};
    StackCall(kPhases, 4, Sc()[1], "FlyingKick_Task");
}

// original 0x49FF20: the task on the owner (its height word +0x3E, not +0x3C);
// +0xB 1; the owner's animation 8 (argument 0); one image (+1 3, +9 0); the
// owner's palette taken, the owner hidden; +1 on.
S04_EXPORT void __cdecl FlyingKick_Start(void) {
    Sc()[8] = Owner()[8];
    CopyL(Sc() + 0x34, Owner() + 0x34);
    CopyL(Sc() + 0x38, Owner() + 0x38);
    SetWord(Sc() + 0x3E, Word(Owner() + 0x3E));
    Sc()[0xB] = 1;
    MH_CALL(BattleActor_SetAnimation)(8, 0);
    {
        unsigned char* const child = ImageChild();
        unsigned char* const s = Sc();
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(s)));
        child[1] = 3;
        child[2] = 0;
        child[6] = 1;
        child[5] = 0x23;
        child[9] = 0;
    }
    TakeOwnerClut();
    MH_CALL(SpriteClut_SetStp)(Sc());
    MH_CALL(SpriteClut_ClearEntry31)(Sc());
    Owner()[0] = static_cast<unsigned char>(Owner()[0] | 0x40);
    Inc(Sc()[1]);
}

// original 0x4A0090: once the image is gone (+0xB 0): the FX palette row back,
// the owner shown again, its animation 4, +1 on.
S04_EXPORT void __cdecl FlyingKick_End(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(SpriteClut_RestoreFxRow)();
    Owner()[0] = static_cast<unsigned char>(Owner()[0] & 0xBF);
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Inc(Sc()[1]);
}

// original 0x4A00C0: an image's kind-1 task, a jmp through KickImage_Kinds
// (four entries) by +1.
S04_EXPORT void __cdecl KickImage_Task(void) { CellCall(Addr(KickImage_Kinds), 4, Sc()[1], "KickImage_Task"); }

namespace {

// The image runs' tail: the sprite updated while +0 and +2 are set; answers
// whether it was.
bool UpdateWhileLive() {
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return false;
    MH_CALL(Sprite_UpdateScreen)();
    return true;
}
// The shadow under an image: on the ground below `ground_below`, else at the
// source sprite's height.
void Shadow(bool on_ground) {
    Call0(on_ground ? bof3::addr::KickImage_PushMatrixGround : bof3::addr::KickImage_PushMatrixSource);
    Call0(bof3::addr::KickImage_DrawShadow);
    MH_CALL(Gte_PopMatrix)();
}
// +2 on by 1 for an acting party member, by 2 for an enemy.
void StepBySide() {
    const unsigned char a = ActorByte();
    unsigned char* const s = Sc();
    AddB(s[2], a < 3 ? 1u : 2u);
}
// Headed at the owner: +0xC Math_Ratan2(float dx, float dz) toward it, +0x14
// 0x40, +0x20 -8, +9 0x10, +2 on.
void AimAtOwner() {
    const unsigned char* const o = Owner();
    const unsigned char* const s = Sc();
    const int dz = static_cast<int>(static_cast<std::uint32_t>(Long(o + 0x38)) - static_cast<std::uint32_t>(Long(s + 0x38)));
    const int dx = static_cast<int>(static_cast<std::uint32_t>(Long(o + 0x34)) - static_cast<std::uint32_t>(Long(s + 0x34)));
    const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    SetLong(Sc() + 0xC, angle);
    SetLong(Sc() + 0x14, 0x40);
    SetLong(Sc() + 0x20, -8);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}
// The hit: the target flagged 0x40, the party member's cry, sound 0x203.
void Hit() {
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    if (ActorByte() < 3) MH_CALL(BattleActor_PlaySound)(2, 0);
    MH_CALL(Sound_PlayById)(0x203);
}
// +0x14 += +0x20, word +0x3E += word +0x14: the rise and its fall.
void RiseAndFall() {
    {
        unsigned char* const s = Sc();
        AddL(s + 0x14, static_cast<std::uint32_t>(Long(s + 0x20)));
    }
    unsigned char* const s = Sc();
    AddW(s + 0x3E, Word(s + 0x14));
}
// +0x5D..+0x5F each up by `v` (8 bits).
void ShadeUp(unsigned v) {
    AddB(Sc()[0x5D], v);
    AddB(Sc()[0x5E], v);
    AddB(Sc()[0x5F], v);
}

}  // namespace

// original 0x4A00E0: an eight-entry stack table by +2 (ChargeImage_Start,
// KickImage_Tick, ChargeImage_Dash, KickImage_Arc, ChargeImage_Wait, _Squash,
// _Stretch, BattleFx_FreeTask); the sprite updated while live.
S04_EXPORT void __cdecl ChargeImage_Run(void) {
    static constexpr std::uint32_t kPhases[8] = {
        bof3::addr::ChargeImage_Start, bof3::addr::KickImage_Tick,      bof3::addr::ChargeImage_Dash,
        bof3::addr::KickImage_Arc,     bof3::addr::ChargeImage_Wait,    bof3::addr::ChargeImage_Squash,
        bof3::addr::ChargeImage_Stretch, bof3::addr::BattleFx_FreeTask};
    StackCall(kPhases, 8, Sc()[2], "ChargeImage_Run");
    UpdateWhileLive();
}

// original 0x4A0150: +2 on by side (an enemy skips the script tick); +0x27 the
// owner's, +0x2B 1; on the owner's point.
S04_EXPORT void __cdecl ChargeImage_Start(void) {
    StepBySide();
    Sc()[0x27] = Owner()[0x27];
    Sc()[0x2B] = 1;
    ToOwnerPoint();
}

// original 0x4A01C0: the sprite's script ticked once; when it answers not 0,
// +2 on.
S04_EXPORT void __cdecl KickImage_Tick(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0) Inc(Sc()[2]);
}

// original 0x4A01E0: a step toward the source sprite (0x60); on reaching it
// (0xC000) the hit and the turn back at the owner.
S04_EXPORT void __cdecl ChargeImage_Dash(void) {
    MH_CALL(MagicFx_StepToward)(Source(), 0x60);
    if (MH_CALL(MagicFx_NearSprite)(Source(), 0xC000) == 0) return;
    if (ActorByte() < 3) MH_CALL(BattleActor_PlaySound)(2, 0);
    MH_CALL(Sound_PlayById)(0x203);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    AimAtOwner();
}

// original 0x4A02B0: along the heading +0xC by 2 (x through the task read
// before Math_Sin, z before Math_Cos), the rise and fall; +9 down, at 0 +2 on.
S04_EXPORT void __cdecl KickImage_Arc(void) {
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x34;
        const int r = Sin(Long(s + 0xC));
        AddL(x, static_cast<std::uint32_t>(ShlSar(r, 13, 12)));
    }
    {
        unsigned char* const s = Sc();
        unsigned char* const z = s + 0x38;
        const int r = Cos(Long(s + 0xC));
        AddL(z, static_cast<std::uint32_t>(ShlSar(r, 13, 12)));
    }
    RiseAndFall();
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4A0330: once the owner's +0xB is 1 (the trails gone): the scale
// +0x40 / +0x44 0x10000, +0x48 2, +9 8, +2 on.
S04_EXPORT void __cdecl ChargeImage_Wait(void) {
    if (Owner()[0xB] != 1) return;
    SetLong(Sc() + 0x40, 0x10000);
    SetLong(Sc() + 0x44, 0x10000);
    Sc()[0x48] = 2;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4A0370: squashed (+0x40 down 0x2000, +0x44 up 0x3000), lifted 0x20;
// +9 down, at 0 on the owner's point, +9 8, +2 on.
S04_EXPORT void __cdecl ChargeImage_Squash(void) {
    AddL(Sc() + 0x40, 0xFFFFE000u);
    AddL(Sc() + 0x44, 0x3000);
    AddW(Sc() + 0x3E, 0x20);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    ToOwnerPoint();
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4A0400: stretched back (+0x40 up 0x2000, +0x44 down 0x3000); +9
// down, at 0 the owner's +0xB down and +2 on (to BattleFx_FreeTask).
S04_EXPORT void __cdecl ChargeImage_Stretch(void) {
    AddL(Sc() + 0x40, 0x2000);
    AddL(Sc() + 0x44, 0xFFFFD000u);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4A0460: a five-entry stack table by +2 (ChargeTrail_Start,
// KickImage_Tick, ChargeTrail_Dash, KickImage_Arc, MAGIC058's 0x4AF490); the
// sprite updated while live.
S04_EXPORT void __cdecl ChargeTrail_Run(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::ChargeTrail_Start, bof3::addr::KickImage_Tick,
                                                 bof3::addr::ChargeTrail_Dash, bof3::addr::KickImage_Arc,
                                                 bof3::addr::MagicFx_UncountAndFree};
    StackCall(kPhases, 5, Sc()[2], "ChargeTrail_Run");
    UpdateWhileLive();
}

// original 0x4A04C0: +9 down; at 0 the trail shows: +0 bit 0x20, +0x5C 1, the
// shade +0x5D..+0x5F (0xFF - +0xB) x 0x14 (8 bits, the later trails darker),
// +0x2B 0, +0x27 the owner's; +2 on by side; on the owner's point.
S04_EXPORT void __cdecl ChargeTrail_Start(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    Sc()[0x5C] = 1;
    for (unsigned k = 0x5D; k < 0x60; ++k) {
        unsigned char* const t = Sc();
        t[k] = static_cast<unsigned char>((0xFFu - t[0xB]) * 0x14u);
    }
    Sc()[0x2B] = 0;
    Sc()[0x27] = Owner()[0x27];
    StepBySide();
    ToOwnerPoint();
}

// original 0x4A05A0: ChargeImage_Dash without the hit: toward the source
// sprite, and on reaching it the turn back at the owner.
S04_EXPORT void __cdecl ChargeTrail_Dash(void) {
    MH_CALL(MagicFx_StepToward)(Source(), 0x60);
    if (MH_CALL(MagicFx_NearSprite)(Source(), 0xC000) == 0) return;
    AimAtOwner();
}

// original 0x4A0640: a seven-entry stack table by +2 (AirRaidImage_Start,
// _Rise, _Turn, _Dive, _Bounce, KickImage_Settle, AirRaidImage_FadeOut); the
// sprite updated while live, and then while +9 is set, +0xB is 0 and +2 below
// 5 the shadow (on the ground below step 4).
S04_EXPORT void __cdecl AirRaidImage_Run(void) {
    static constexpr std::uint32_t kPhases[7] = {
        bof3::addr::AirRaidImage_Start, bof3::addr::AirRaidImage_Rise,   bof3::addr::AirRaidImage_Turn,
        bof3::addr::AirRaidImage_Dive,  bof3::addr::AirRaidImage_Bounce, bof3::addr::KickImage_Settle,
        bof3::addr::AirRaidImage_FadeOut};
    StackCall(kPhases, 7, Sc()[2], "AirRaidImage_Run");
    if (!UpdateWhileLive()) return;
    const unsigned char* const s = Sc();
    if (s[9] == 0 || s[0xB] != 0 || s[2] >= 5) return;
    Shadow(s[2] < 4);
}

// original 0x4A06F0: +9 down; at 0: +0x27 the owner's, +0x2B 1 for the first
// image; the others shaded 0xC0 (+0 bit 0x20, +0x5C 1); +0x14 0x80, +0x20 -2,
// the scale 0x10000, +0x48 2, +9 0x10, +2 on.
S04_EXPORT void __cdecl AirRaidImage_Start(void) {
    Dec(Sc()[9]);
    unsigned char* s = Sc();
    if (s[9] != 0) return;
    s[0x27] = Owner()[0x27];
    s = Sc();
    s[0x2B] = s[0xB] == 0 ? 1 : 0;
    s = Sc();
    if (s[0xB] != 0) {
        s[0] = static_cast<unsigned char>(s[0] | 0x20);
        Sc()[0x5C] = 1;
        Sc()[0x5F] = 0xC0;
        Sc()[0x5E] = 0xC0;
        Sc()[0x5D] = 0xC0;
    }
    SetLong(Sc() + 0x14, 0x80);
    SetLong(Sc() + 0x20, -2);
    SetLong(Sc() + 0x40, 0x10000);
    SetLong(Sc() + 0x44, 0x10000);
    Sc()[0x48] = 2;
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A07B0: round the source sprite (0x1800), rising, narrowing
// (+0x40 down 0x800, +0x44 up 0x1000); +9 down, at 0 over the source sprite
// 0xC00 up, a step round the owner (0x20000), +2 on.
S04_EXPORT void __cdecl AirRaidImage_Rise(void) {
    MH_CALL(MagicFx_StepAround)(Source(), 0x1800, 0);
    RiseAndFall();
    AddL(Sc() + 0x40, 0xFFFFF800u);
    AddL(Sc() + 0x44, 0x1000);
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    const unsigned char* const src = Source();
    CopyL(s + 0x34, src + 0x34);
    CopyL(Sc() + 0x38, src + 0x38);
    SetWord(Sc() + 0x3E, (Word(src + 0x3E) + 0xC00) & 0xFFFF);
    MH_CALL(MagicFx_StepAround)(Owner(), 0x20000, 0);
    Inc(Sc()[2]);
}

// original 0x4A0870: +9 up; at 4 the scale (0x8000, 0x18000) and the shade:
// (0xFD - +0xB, 0xFC - +0xB, 0xFC - +0xB) x 16 for the later images, (0x30,
// 0xC0, 0xC0) with +0 bit 0x20 and +0x5C 1 for the first; +2 on.
S04_EXPORT void __cdecl AirRaidImage_Turn(void) {
    Inc(Sc()[9]);
    unsigned char* s = Sc();
    if (s[9] != 4) return;
    SetLong(s + 0x40, 0x8000);
    SetLong(Sc() + 0x44, 0x18000);
    s = Sc();
    const unsigned char b = s[0xB];
    if (b != 0) {
        s[0x5D] = static_cast<unsigned char>((0xFDu - b) << 4);
        s = Sc();
        s[0x5E] = static_cast<unsigned char>((0xFCu - s[0xB]) << 4);
        s = Sc();
        s[0x5F] = static_cast<unsigned char>((0xFCu - s[0xB]) << 4);
        Inc(Sc()[2]);
        return;
    }
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    Sc()[0x5C] = 1;
    Sc()[0x5D] = 0x30;
    Sc()[0x5E] = 0xC0;
    Sc()[0x5F] = 0xC0;
    Inc(Sc()[2]);
}

// original 0x4A0920: round the source sprite (0x2000), down 0xC0 a frame; +9
// up, at 0x14 the bounce set up (+0x14 0x48, +0x20 -8, the scale 0x10000, +0x48
// 0) and for the first image the hit; +2 on.
S04_EXPORT void __cdecl AirRaidImage_Dive(void) {
    MH_CALL(MagicFx_StepAround)(Source(), 0x2000, 0);
    AddW(Sc() + 0x3E, 0xFF40);
    Inc(Sc()[9]);
    unsigned char* s = Sc();
    if (s[9] != 0x14) return;
    SetLong(s + 0x14, 0x48);
    SetLong(Sc() + 0x20, -8);
    SetLong(Sc() + 0x40, 0x10000);
    SetLong(Sc() + 0x44, 0x10000);
    Sc()[0x48] = 0;
    s = Sc();
    if (s[0xB] == 0) {
        Hit();
        s = Sc();
    }
    Inc(s[2]);
}

// original 0x4A09E0: the later images' shade stepped by AirRaidImage_ShadeSteps
// (+0xB unbounded) until +0x5D is 0x80, the first's toward (0, +4, +4) until
// +0x5D is 0; +9 down unless 0x10; the script ticked; a step round the acting
// actor's sprite (0x2000); the rise and fall; falling below the source sprite,
// a later image gone (the owner's +0xB down, the task freed), the first unshaded
// on the source's height and +2 on.
S04_EXPORT void __cdecl AirRaidImage_Bounce(void) {
    constexpr std::uint32_t kSteps = 0x65A696;   // AirRaidImage_ShadeSteps - 2: byte pairs by +0xB
    unsigned char* s = Sc();
    const unsigned char b = s[0xB];
    if (b != 0) {
        const unsigned char d = s[0x5D];
        if (d != 0x80) {
            s[0x5D] = static_cast<unsigned char>(d - Mem(kSteps + 2u * b)[0]);
            s = Sc();
            s[0x5E] = static_cast<unsigned char>(s[0x5E] - Mem(kSteps + 1 + 2u * s[0xB])[0]);
            s = Sc();
            s[0x5F] = static_cast<unsigned char>(s[0x5F] - Mem(kSteps + 1 + 2u * s[0xB])[0]);
            s = Sc();
        }
    } else {
        const unsigned char c = s[0x5D];
        if (c != 0) {
            s[0x5D] = static_cast<unsigned char>(c - 3);
            AddB(Sc()[0x5E], 4);
            AddB(Sc()[0x5F], 4);
            s = Sc();
        }
    }
    if (s[9] != 0x10) Dec(s[9]);
    MH_CALL(Sprite_ScriptTickOnce)();
    MH_CALL(MagicFx_StepAround)(Pointer(kActorSprite), 0x2000, 0);
    RiseAndFall();
    s = Sc();
    if (Long(s + 0x14) >= 0 || Long(s + 0x3C) >= Long(Source() + 0x3C)) return;
    if (s[0xB] != 0) {
        Dec(Owner()[0xB]);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    s[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    CopyL(Sc() + 0x3C, Source() + 0x3C);
    Inc(Sc()[2]);
}

// original 0x4A0B10: the shade down 0x10 a frame until +0x5D is 0x80; then once
// the owner's +0xB is 1 and the script ticks to its end, on the owner's point
// and +2 on.
S04_EXPORT void __cdecl KickImage_Settle(void) {
    unsigned char* s = Sc();
    const unsigned char d = s[0x5D];
    if (d != 0x80) {
        s[0x5D] = static_cast<unsigned char>(d - 0x10);
        AddB(Sc()[0x5E], 0xF0);
        AddB(Sc()[0x5F], 0xF0);
        s = Sc();
    }
    if (Owner()[0xB] != 1 || s[0x5D] != 0x80) return;
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    ToOwnerPoint();
    Inc(Sc()[2]);
}

// original 0x4A0BA0: the shade up 0x10 a frame; at +0x5D 0xC0 the owner's +0xB
// down and the task freed.
S04_EXPORT void __cdecl AirRaidImage_FadeOut(void) {
    ShadeUp(0x10);
    if (Sc()[0x5D] != 0xC0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A0BF0: a seven-entry stack table by +2 (FlyingKickImage_Start,
// _Rise, _Dive, _Bounce, KickImage_Settle, FlyingKickImage_FadeOut,
// BattleFx_FreeTask); the sprite updated while live, and then while +9 is set
// and +2 below 4 the shadow (on the ground below step 3).
S04_EXPORT void __cdecl FlyingKickImage_Run(void) {
    static constexpr std::uint32_t kPhases[7] = {
        bof3::addr::FlyingKickImage_Start,  bof3::addr::FlyingKickImage_Rise, bof3::addr::FlyingKickImage_Dive,
        bof3::addr::FlyingKickImage_Bounce, bof3::addr::KickImage_Settle,     bof3::addr::FlyingKickImage_FadeOut,
        bof3::addr::BattleFx_FreeTask};
    StackCall(kPhases, 7, Sc()[2], "FlyingKickImage_Run");
    if (!UpdateWhileLive()) return;
    const unsigned char* const s = Sc();
    if (s[9] == 0 || s[2] >= 4) return;
    Shadow(s[2] < 3);
}

// original 0x4A0C90: +0x14 0x80, +0x20 -16, +0x27 the owner's, +9 0x14, +0xA 7,
// +2 on.
S04_EXPORT void __cdecl FlyingKickImage_Start(void) {
    SetLong(Sc() + 0x14, 0x80);
    SetLong(Sc() + 0x20, -16);
    Sc()[0x27] = Owner()[0x27];
    Sc()[9] = 0x14;
    Sc()[0xA] = 7;
    Inc(Sc()[2]);
}

// original 0x4A0CE0: round the source sprite (0x3800), rising; +9 and +0xA
// down, at +0xA 0 +2 on.
S04_EXPORT void __cdecl FlyingKickImage_Rise(void) {
    MH_CALL(MagicFx_StepAround)(Source(), 0x3800, 0);
    RiseAndFall();
    Dec(Sc()[9]);
    Dec(Sc()[0xA]);
    unsigned char* const s = Sc();
    if (s[0xA] == 0) Inc(s[2]);
}

// original 0x4A0D40: toward the source sprite (0x60), +9 up to 0x14; on reaching
// it in 3D (0xC000) the bounce set up (+0x14 0x48, +0x20 -8), the hit, +2 on.
S04_EXPORT void __cdecl FlyingKickImage_Dive(void) {
    MH_CALL(MagicFx_StepToward)(Source(), 0x60);
    {
        unsigned char* const s = Sc();
        if (s[9] < 0x14) Inc(s[9]);
    }
    if (MH_CALL(MagicFx_NearSprite3D)(Source(), 0xC000) == 0) return;
    SetLong(Sc() + 0x14, 0x48);
    SetLong(Sc() + 0x20, -8);
    Hit();
    Inc(Sc()[2]);
}

// original 0x4A0DD0: the script ticked; a step round the acting actor's sprite
// (0x2000); the rise and fall - rising, +9 down on odd frames, falling, +9 up
// to 0x14; falling below the source sprite, unshaded (+0 bit 0x20, +0x5C 1,
// +0x5D..+0x5F 0) on the source's height, +2 on.
S04_EXPORT void __cdecl FlyingKickImage_Bounce(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    MH_CALL(MagicFx_StepAround)(Pointer(kActorSprite), 0x2000, 0);
    RiseAndFall();
    unsigned char* s = Sc();
    if (Long(s + 0x14) >= 0) {
        if (Frame_Counter & 1) {
            Dec(s[9]);
            s = Sc();
        }
    } else if (s[9] < 0x14) {
        Inc(s[9]);
        s = Sc();
    }
    if (Long(s + 0x14) >= 0 || Long(s + 0x3C) >= Long(Source() + 0x3C)) return;
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    Sc()[0x5C] = 1;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    CopyL(Sc() + 0x3C, Source() + 0x3C);
    Inc(Sc()[2]);
}

// original 0x4A0EA0: the shade up 0x10 a frame; at +0x5D 0xC0 the owner's +0xB
// down and +2 on (to BattleFx_FreeTask).
S04_EXPORT void __cdecl FlyingKickImage_FadeOut(void) {
    ShadeUp(0x10);
    if (Sc()[0x5D] != 0xC0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4A0EF0: the shadow's matrix on the ground under the image
// (AreaMap_Elevation at its point, halved), not turned.
S04_EXPORT void __cdecl KickImage_PushMatrixGround(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[4] = {0, 0, 0, 0};
    const unsigned char* const s = Sc();
    short v[4];
    GroundPoint(s, 0, v);
    const long ground = MH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    v[2] = static_cast<short>(-Half(static_cast<short>(ground)));
    SetMatrix(angles, v);
}

// original 0x4A0FA0: the shadow's matrix under the image at the source
// sprite's height (+0x3E halved), not turned.
S04_EXPORT void __cdecl KickImage_PushMatrixSource(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[4] = {0, 0, 0, 0};
    const unsigned char* const s = Sc();
    short v[4];
    GroundPoint(s, static_cast<short>(-Half(S16(Source() + 0x3E))), v);
    SetMatrix(angles, v);
}

// original 0x4A1050: the shadow: eight semi-transparent gouraud triangles
// round a disc of radius +9 x 4 (word 0x903850, read again at every use), the
// centre 0x80 grey and the rim 1, under a subtractive draw mode (tpage 0x55,
// closed with 0x15); all committed to slot 5.
S04_EXPORT void __cdecl KickImage_DrawShadow(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetSW(0, static_cast<unsigned>(Sc()[9]) << 2);
    {
        const int r = Sin(0);
        SetVW(0x10, static_cast<unsigned>(Mul12(r, SS(0))));
    }
    {
        const int r = Cos(0);
        SetVW(0x12, static_cast<unsigned>(Mul12(r, SS(0))));
    }
    for (int angle = 0x200; angle < 0x1200; angle += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(0x10), z = VW(0x12);
            SetVW(0, 0);
            SetVW(2, 0);
            SetVW(8, x);
            SetVW(0xA, z);
        }
        {
            const int r = Sin(angle);
            SetVW(0x10, static_cast<unsigned>(Mul12(r, SS(0))));
        }
        {
            const int r = Cos(angle);
            SetVW(0x12, static_cast<unsigned>(Mul12(r, SS(0))));
        }
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        Rtp3(p, 0x10);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        for (unsigned k : {4u, 5u, 6u}) p[k] = 0x80;
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

void MagicS04_Inject() {
    if (bof3::WantsShadow("magic_s04")) magic_s04::SelfTest();
    BOF3_INJECT(Snap_Task);
    BOF3_INJECT(Snap_Start);
    BOF3_INJECT(Snap_Buff);
    BOF3_INJECT(SnapWave_Task);
    BOF3_INJECT(SnapWave_Run);
    BOF3_INJECT(SnapWave_Start);
    BOF3_INJECT(SnapWave_Burst);
    BOF3_INJECT(SnapWave_Swing);
    BOF3_INJECT(SnapWave_End);
    BOF3_INJECT(SnapWave_PushMatrixA);
    BOF3_INJECT(SnapWave_PushMatrixB);
    BOF3_INJECT(SnapWave_DrawRingA);
    BOF3_INJECT(SnapWave_DrawRingB);
    BOF3_INJECT(SnapSpark_Task);
    BOF3_INJECT(SnapSpark_Run);
    BOF3_INJECT(SnapSpark_Launch);
    BOF3_INJECT(SnapSpark_Fly);
    BOF3_INJECT(SnapSpark_PushMatrix);
    BOF3_INJECT(SnapSpark_Draw);
    BOF3_INJECT(SnapSpark_Alloc);
    BOF3_INJECT(Charge_Task);
    BOF3_INJECT(Charge_Start);
    BOF3_INJECT(AirRaid_Task);
    BOF3_INJECT(AirRaid_Start);
    BOF3_INJECT(FlyingKick_Task);
    BOF3_INJECT(FlyingKick_Start);
    BOF3_INJECT(FlyingKick_End);
    BOF3_INJECT(KickImage_Task);
    BOF3_INJECT(ChargeImage_Run);
    BOF3_INJECT(ChargeImage_Start);
    BOF3_INJECT(KickImage_Tick);
    BOF3_INJECT(ChargeImage_Dash);
    BOF3_INJECT(KickImage_Arc);
    BOF3_INJECT(ChargeImage_Wait);
    BOF3_INJECT(ChargeImage_Squash);
    BOF3_INJECT(ChargeImage_Stretch);
    BOF3_INJECT(ChargeTrail_Run);
    BOF3_INJECT(ChargeTrail_Start);
    BOF3_INJECT(ChargeTrail_Dash);
    BOF3_INJECT(AirRaidImage_Run);
    BOF3_INJECT(AirRaidImage_Start);
    BOF3_INJECT(AirRaidImage_Rise);
    BOF3_INJECT(AirRaidImage_Turn);
    BOF3_INJECT(AirRaidImage_Dive);
    BOF3_INJECT(AirRaidImage_Bounce);
    BOF3_INJECT(KickImage_Settle);
    BOF3_INJECT(AirRaidImage_FadeOut);
    BOF3_INJECT(FlyingKickImage_Run);
    BOF3_INJECT(FlyingKickImage_Start);
    BOF3_INJECT(FlyingKickImage_Rise);
    BOF3_INJECT(FlyingKickImage_Dive);
    BOF3_INJECT(FlyingKickImage_Bounce);
    BOF3_INJECT(FlyingKickImage_FadeOut);
    BOF3_INJECT(KickImage_PushMatrixGround);
    BOF3_INJECT(KickImage_PushMatrixSource);
    BOF3_INJECT(KickImage_DrawShadow);
}

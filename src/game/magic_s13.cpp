// One spell overlay compiled into the exe, round nine group S13
// (docs/magic_s13.md): the PSX's MAGIC063.EMI, Magic_Rows row 143, loaded by
// id 0x3F. Read one id down (docs/cut-content.md section 2) the sibling
// labels it Sudden Death; the names below use that label as a hypothesis, and
// say what the code does.
//
//   - the kind-2 task (row 143): a child (kind 1, parameter 0x68) on every
//     actor but the caster that is not out, enemies first; then, every frame,
//     the walk of its own pool of 96 motes (SuddenDeath_Motes);
//   - each child: its actor's position, one orbit mote circling it; after a
//     random delay every child but the last frees itself once its motes are
//     gone, and the last bursts 92 motes that close in on it, spin, and rise
//     away, then flags its actor 0x40 and ends;
//   - each mote draws four semi-transparent gouraud triangles about its point
//     (SuddenDeathMote_Shape), coloured by its +5..+7 times its +9.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack or .data table aborts where the original would call through whatever
// follows it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s13.h"

#include <cstdint>

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

// The scratch the overlay keeps its working values in: DamageScratch's words
// (0x903850..0x90385F) and the three SVECTORs of Prim_VertexScratch
// (0x9037A0..0x9037B7). Both are read again after every call, as the
// originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The mote pool: 96 records of 0x20 bytes (SuddenDeath_Motes) and the cell
// naming the one being run (SuddenDeath_CurrentMote).
constexpr unsigned kMotes = 0x60;
constexpr unsigned kMoteStride = 0x20;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* M() { return SuddenDeath_CurrentMote; }
unsigned char* MoteAt(unsigned i) { return SuddenDeath_Motes + i * kMoteStride; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3.
unsigned char* ActorRecord(unsigned a) {
    if (a < 3) return Mem(at::kParty + a * at::kPartyStride);
    return Mem(at::kEnemies + (a - 3) * at::kEnemyStride);
}

unsigned RandByte() { return static_cast<unsigned>(MH_CALL(Rand)()) & 0xFFu; }

// This group's functions called directly, by address, as the originals call
// them: in the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using AllocFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
unsigned Alloc() { return MH_AT(AllocFn, bof3::addr::SuddenDeathMote_Alloc)() & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call by `phase`, unchecked by the original.
void StackCall(const std::uint32_t* table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(table[phase])();
}
// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only - what
// follows is the next table or data.
void CellCall(void* const* table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(const_cast<void*>(table[phase]))();
}

// The mote's point: the owner's +0x34 / +0x38 plus sin / cos of the angle +0xE
// times `radius` (the burst's, the word +0xC read after each call) - x after
// Math_Sin, z after Math_Cos, every cell read again after each call.
void BurstPoint() {
    {
        const int s = MH_CALL(Math_Sin)(S16(M() + 0xE));
        unsigned char* const m = M();
        SetLong(m + 0x10, static_cast<std::int32_t>(U(s) * U(S16(m + 0xC)) + U(Long(Owner() + 0x34))));
    }
    {
        const int c = MH_CALL(Math_Cos)(S16(M() + 0xE));
        unsigned char* const m = M();
        SetLong(m + 0x14, static_cast<std::int32_t>(U(c) * U(S16(m + 0xC)) + U(Long(Owner() + 0x38))));
    }
}
// The orbit's: a fixed radius of 32 (`shl 5`).
void OrbitPoint() {
    {
        const int s = MH_CALL(Math_Sin)(S16(M() + 0xE));
        SetLong(M() + 0x10, static_cast<std::int32_t>((U(s) << 5) + U(Long(Owner() + 0x34))));
    }
    {
        const int c = MH_CALL(Math_Cos)(S16(M() + 0xE));
        SetLong(M() + 0x14, static_cast<std::int32_t>((U(c) << 5) + U(Long(Owner() + 0x38))));
    }
}
// The angle word +0xE on by `step`, mod 0x1000.
void Turn(unsigned step) {
    unsigned char* const m = M();
    SetWord(m + 0xE, (Word(m + 0xE) + step) & 0xFFF);
}
// Three colour bytes (+5..+7) of (Rand & 7) + 5, the cell read after each.
void RandColours() {
    for (unsigned k = 5; k < 8; ++k) {
        const unsigned r = RandByte();
        M()[k] = static_cast<unsigned char>((r & 7) + 5);
    }
}

// A child of the spawn on actor `a`: kind 1 parameter 0x68, owned by this
// task, +1 0, +4 the actor; this task's +0xB (its children) up.
void SpawnChild(unsigned a) {
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x68) & 0xFFu;
    unsigned char* const t = TaskSlot(slot);
    unsigned char* const cur = Sc();
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(cur)));
    t[1] = 0;
    t[4] = static_cast<unsigned char>(a);
    Inc(cur[0xB]);
}

// The draw's two points of a triangle: (sin, cos) of the angle word at
// kS + `angle` times the radius word at kS + `radius`, >> 12.
void ShapePoint(unsigned out, unsigned radius, unsigned angle) {
    {
        const int s = MH_CALL(Math_Sin)(SS(angle));
        SetVW(out, static_cast<unsigned>(Mul12(s, SS(radius))));
    }
    {
        const int c = MH_CALL(Math_Cos)(SS(angle));
        SetVW(out + 2, static_cast<unsigned>(Mul12(c, SS(radius))));
    }
}

// Gte_RotTransPers3 with the eight words the original pushes (the depth and
// flag, its own stack; ours reads the first seven).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
#define S13_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// One MATRIX block as the original lays it out on its stack, Gte_RotTrans
// writing its translation at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

}  // namespace

#define S13_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// The task

// original 0x4B2F40: the kind-2 task. A two-entry stack table by +1:
// SuddenDeath_Spawn, MAGIC131's MagicFx_EndWhenChildrenDone. Then the walk:
// every mote with bit 0 becomes SuddenDeath_CurrentMote, its +0x1C the owner
// 0x93B940 for SuddenDeathMote_Task, the owner (saved once, after the phase)
// put back after each.
S13_EXPORT void __cdecl SuddenDeath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::SuddenDeath_Spawn, bof3::addr::MagicFx_EndWhenChildrenDone};
    StackCall(kPhases, 2, Sc()[1], "SuddenDeath_Task");
    const std::int32_t saved = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = MoteAt(i);
        if ((m[0] & 1) == 0) continue;
        SuddenDeath_CurrentMote = m;
        SetLong(Mem(at::kOwner), Long(m + 0x1C));
        Call0(bof3::addr::SuddenDeathMote_Task);
        SetLong(Mem(at::kOwner), saved);
    }
}

// original 0x4B2FB0: the pool's +0..+2 cleared; +0xB and +9 0, +1 on; a
// child on every enemy 3..10, then every member 0..2, that Battle_ActorIsOut
// does not answer for and that is not the acting actor; sound 0x100.
S13_EXPORT void __cdecl SuddenDeath_Spawn(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = MoteAt(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    for (unsigned a = 3; a < 11; ++a) {
        if (MH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        if ((U(Long(Mem(at::kActor))) & 0xFFu) == a) continue;
        SpawnChild(a);
    }
    for (unsigned a = 0; a < 3; ++a) {
        if (MH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        if (Mem(at::kActor)[0] == a) continue;
        SpawnChild(a);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// ===========================================================================
// The children (kind 1, parameter 0x68)

// original 0x4B30F0: a jmp through SuddenDeathChild_Kinds (one entry) by +1.
S13_EXPORT void __cdecl SuddenDeathChild_Task(void) {
    CellCall(SuddenDeathChild_Kinds, 1, Sc()[1], "SuddenDeathChild_Task");
}

// original 0x4B3110: a five-entry stack table by +2.
S13_EXPORT void __cdecl SuddenDeathChild_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::SuddenDeathChild_Start, bof3::addr::SuddenDeathChild_Delay,
                                                bof3::addr::SuddenDeathChild_WaitMotes, bof3::addr::SuddenDeathChild_Burst,
                                                bof3::addr::SuddenDeathChild_End};
    StackCall(kSteps, 5, Sc()[2], "SuddenDeathChild_Run");
}

// original 0x4B3150: the actor's position (its +0x3C raised by 0x2000000);
// +9 a delay of (Rand & 0xF) + 1, +0xB 0, +2 on; one orbit mote (+1 1) owned
// by this task, counted in its +0xB.
S13_EXPORT void __cdecl SuddenDeathChild_Start(void) {
    {
        const unsigned char* const rec = ActorRecord(Sc()[4]);
        SetLong(Sc() + 0x34, Long(rec + 0x34));
        SetLong(Sc() + 0x38, Long(rec + 0x38));
        SetLong(Sc() + 0x3C, static_cast<std::int32_t>(U(Long(rec + 0x3C)) + 0x2000000u));
    }
    {
        const unsigned r = RandByte();
        Sc()[9] = static_cast<unsigned char>((r & 0xF) + 1);
    }
    Sc()[0xB] = 0;
    Inc(Sc()[2]);
    const unsigned idx = Alloc();
    if (idx == 0xFF) return;
    unsigned char* const cur = Sc();
    unsigned char* const m = MoteAt(idx);
    SetLong(m + 0x1C, static_cast<std::int32_t>(Key(cur)));
    m[1] = 1;
    Inc(cur[0xB]);
}

// original 0x4B3210: +9 down; at 0 the last child (the parent's +0xB 1) goes
// to the burst (+2 3); any other counts itself out of the parent and goes on
// to wait for its motes.
S13_EXPORT void __cdecl SuddenDeathChild_Delay(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    unsigned char* const parent = Owner();
    if (parent[0xB] == 1) {
        s[2] = 3;
        return;
    }
    Dec(parent[0xB]);
    Inc(Sc()[2]);
}

// original 0x4B3250: its motes gone (+0xB 0): the task freed.
S13_EXPORT void __cdecl SuddenDeathChild_WaitMotes(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B3270: its orbit mote gone: 92 burst motes (+1 0), mote n with
// +4 n, +0xA (Rand & 0xF) + (n / 32) * 16 + 1 and +8 0x40 less that, each
// counted in this task's +0xB; sounds 0x101 and 0x102; +2 on.
S13_EXPORT void __cdecl SuddenDeathChild_Burst(void) {
    if (Sc()[0xB] != 0) return;
    for (unsigned n = 0; n < 0x5C; ++n) {
        const unsigned idx = Alloc();
        if (idx == 0xFF) continue;
        unsigned char* const m = MoteAt(idx);
        SetLong(m + 0x1C, static_cast<std::int32_t>(Key(Sc())));
        m[1] = 0;
        m[4] = static_cast<unsigned char>(n);
        const unsigned r = RandByte();
        const unsigned char start = static_cast<unsigned char>((r & 0xF) + ((n >> 5) << 4) + 1);
        m[0xA] = start;
        m[8] = static_cast<unsigned char>(0x40 - start);
        Inc(Sc()[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x101);
    MH_CALL(Sound_PlayById)(0x102);
    Inc(Sc()[2]);
}

// original 0x4B3320: its motes gone: the actor flagged 0x40, the owner's +0xB
// down, the task freed. Also InkInkActor_Phases' second entry (group C1).
S13_EXPORT void __cdecl SuddenDeathChild_End(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(Sc()[4]);
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// The motes (SuddenDeath_Motes, the current one SuddenDeath_CurrentMote, its
// owner 0x93B940 the child)

// original 0x4B3350: a jmp through SuddenDeathMote_Kinds (two entries) by +1.
S13_EXPORT void __cdecl SuddenDeathMote_Task(void) {
    CellCall(SuddenDeathMote_Kinds, 2, M()[1], "SuddenDeathMote_Task");
}

namespace {
// 0x4B3370 / 0x4B3A90: the step through `steps` by +2; then, while the mote
// is live (+0) and past its first step (+2), its matrix, its triangles, the
// matrix popped.
void MoteRun(void* const* steps, unsigned n, const char* who) {
    CellCall(steps, n, M()[2], who);
    const unsigned char* const m = M();
    if (m[0] == 0 || m[2] == 0) return;
    Call0(bof3::addr::SuddenDeathMote_PushMatrix);
    Call0(bof3::addr::SuddenDeathMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}
}  // namespace

// original 0x4B3370: the burst mote's run, SuddenDeathBurst_Steps (four).
S13_EXPORT void __cdecl SuddenDeathBurst_Run(void) {
    MoteRun(SuddenDeathBurst_Steps, 4, "SuddenDeathBurst_Run");
}

// original 0x4B33B0: +0xA down; at 0 the mote is thrown out: a radius of
// ((+4 / 32) + 6) * 16 and an angle of (+4 & 0x1F) * 128 (the words 0x903850 /
// 0x903854), its point about the owner at them, its height the owner's; its
// colours; +3 and +9 0; +0xA (Rand & 3) * 8; +0xB Rand; +0xC the radius; +2
// on.
S13_EXPORT void __cdecl SuddenDeathBurst_Launch(void) {
    Dec(M()[0xA]);
    {
        const unsigned char* const m = M();
        if (m[0xA] != 0) return;
        SetSW(0, ((m[4] >> 5) + 6u) << 4);
        const unsigned angle = (m[4] & 0x1Fu) << 7;
        SetSW(4, angle);
        const int s = MH_CALL(Math_Sin)(static_cast<int>(angle));
        SetLong(M() + 0x10, static_cast<std::int32_t>(U(s) * U(SS(0)) + U(Long(Owner() + 0x34))));
    }
    {
        const int c = MH_CALL(Math_Cos)(SS(4));
        SetLong(M() + 0x14, static_cast<std::int32_t>(U(c) * U(SS(0)) + U(Long(Owner() + 0x38))));
    }
    SetLong(M() + 0x18, Long(Owner() + 0x3C));
    SetWord(M() + 0xE, SW(4));
    RandColours();
    M()[3] = 0;
    M()[9] = 0;
    {
        const unsigned r = RandByte();
        M()[0xA] = static_cast<unsigned char>((r & 3) << 3);
    }
    {
        const unsigned r = RandByte();
        M()[0xB] = static_cast<unsigned char>(r);
    }
    SetWord(M() + 0xC, SW(0));
    Inc(M()[2]);
}

// original 0x4B3500: the radius +0xC in by 2; the point; +0xB up; +9 up by 2
// below 0x10; at a radius not past (+4 / 32) * 8 + 0x20, +2 on, and mote 0
// flags the owner's actor 0x10.
S13_EXPORT void __cdecl SuddenDeathBurst_Close(void) {
    SetWord(M() + 0xC, Word(M() + 0xC) - 2u);
    BurstPoint();
    Inc(M()[0xB]);
    unsigned char* const m = M();
    if (m[9] < 0x10) AddB(m[9], 2);
    const int limit = static_cast<int>((m[4] >> 5) * 8u + 0x20u);
    if (S16(m + 0xC) > limit) return;
    Inc(m[2]);
    if (M()[4] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(Owner()[4], 0x10);
}

// original 0x4B35D0: the angle on by 0x40; the point; +0xB up; +5 up to 0xC,
// +6 and +7 down to 4, +3 up to 7; +8 down, at 0 +2 on.
S13_EXPORT void __cdecl SuddenDeathBurst_Spin(void) {
    Turn(0x40);
    BurstPoint();
    unsigned char* const m = M();
    Inc(m[0xB]);
    if (m[5] < 0xC) Inc(m[5]);
    if (m[6] > 4) Dec(m[6]);
    if (m[7] > 4) Dec(m[7]);
    if (m[3] < 7) Inc(m[3]);
    Dec(m[8]);
    if (m[8] == 0) Inc(m[2]);
}

// original 0x4B36C0: the angle on by 0x40; the point; the height word +0x1A up
// by +0xA; +0xB up; on odd frames +9 down, at 0 the owner's +0xB down and the
// mote freed.
S13_EXPORT void __cdecl SuddenDeathBurst_Rise(void) {
    Turn(0x40);
    BurstPoint();
    {
        unsigned char* const m = M();
        SetWord(m + 0x1A, Word(m + 0x1A) + m[0xA]);
        Inc(m[0xB]);
    }
    if ((Frame_Counter & 1u) == 0) return;
    unsigned char* const m = M();
    Dec(m[9]);
    if (m[9] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::SuddenDeathMote_Free);
}

// original 0x4B3780: the mote's matrix pushed: Camera_Matrix times a turn
// about z of (0x800 - +0xE + +3 * 128) & 0xFFF, translated by Gte_RotTrans of
// ((+0x10 >> 9) - 0x4000, (+0x14 >> 9) - 0x4000, -(height / 2)). The mote is
// read once, after the push.
S13_EXPORT void __cdecl SuddenDeathMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const m = M();
    const short rot[4] = {0, 0, static_cast<short>((0x800u - Word(m + 0xE) + (static_cast<unsigned>(m[3]) << 7)) & 0xFFF), 0};
    short v[4];
    v[0] = static_cast<short>((Long(m + 0x10) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(m + 0x14) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(m + 0x1A) / 2));
    v[3] = 0;
    Matrix mx;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S13_AS(RotTransFn, Gte_RotTrans)(v, mx.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, mx.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, mx.m, mx.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&mx));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&mx));
}

// original 0x4B3840: a draw-mode packet (tpage 0x35) linked at the mote; its
// colour (+5, +6, +7 each times +9) into the words 0x90385A / C / E; then
// four semi-transparent gouraud triangles, triangle i from the origin to the
// points (radius, angle) of SuddenDeathMote_Shape's words i and i + 4, both
// lifted by sin(((+0xB + SuddenDeathMote_Turns[i]) & 0xF) * 256) * 32 >> 12,
// its first vertex coloured (E, C, A) and the others (A, C, E), each linked
// at the mote.
S13_EXPORT void __cdecl SuddenDeathMote_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    {
        const unsigned char* const m = M();
        MH_CALL(MapView_LinkPrimAt)(U(Long(m + 0x10)), U(Long(m + 0x14)), 2, 0xC);
    }
    {
        const unsigned char* const m = M();
        SetSW(0xA, static_cast<unsigned>(m[5]) * m[9]);
        SetSW(0xC, static_cast<unsigned>(m[6]) * m[9]);
        SetSW(0xE, static_cast<unsigned>(m[7]) * m[9]);
    }
    SetVW(4, 0);
    SetVW(2, 0);
    SetVW(0, 0);
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char* const m = M();
        SetSW(0, static_cast<unsigned>(SuddenDeathMote_Shape[i]));
        SetSW(2, static_cast<unsigned>(SuddenDeathMote_Shape[4 + i]));
        unsigned char* const p = Gfx_PacketNext;
        SetSW(4, static_cast<unsigned>(SuddenDeathMote_Shape[8 + i]));
        SetSW(6, static_cast<unsigned>(SuddenDeathMote_Shape[12 + i]));
        SetSW(8, ((m[0xB] + SuddenDeathMote_Turns[i]) & 0xFu) << 8);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        ShapePoint(8, 0, 4);
        ShapePoint(0x10, 2, 6);
        {
            const int s = MH_CALL(Math_Sin)(SS(8));
            const unsigned lift = static_cast<unsigned>(static_cast<int>(U(s) << 5) >> 12);
            SetVW(0xC, lift);
            SetVW(0x14, lift);
        }
        {
            long depth, flag;
            S13_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), p + 8, p + 0x18, p + 0x28, &depth, &flag);
        }
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(0xE);
        p[5] = SB(0xC);
        p[6] = SB(0xA);
        p[0x14] = SB(0xA);
        p[0x15] = SB(0xC);
        p[0x16] = SB(0xE);
        p[0x24] = SB(0xA);
        p[0x25] = SB(0xC);
        p[0x26] = SB(0xE);
        const unsigned char* const mote = M();
        MH_CALL(MapView_LinkPrimAt)(U(Long(mote + 0x10)), U(Long(mote + 0x14)), 2, 0x34);
    }
}

// original 0x4B3A90: the orbit mote's run, SuddenDeathOrbit_Steps (five).
S13_EXPORT void __cdecl SuddenDeathOrbit_Run(void) {
    MoteRun(SuddenDeathOrbit_Steps, 5, "SuddenDeathOrbit_Run");
}

// original 0x4B3AD0: the point 32 from the owner at angle 0; its height the
// owner's; its colours; +0xE 0, +3 7, +9 0, +0xA 0x40, +0xB Rand; +2 on.
S13_EXPORT void __cdecl SuddenDeathOrbit_Start(void) {
    {
        const int s = MH_CALL(Math_Sin)(0);
        SetLong(M() + 0x10, static_cast<std::int32_t>((U(s) << 5) + U(Long(Owner() + 0x34))));
    }
    {
        const int c = MH_CALL(Math_Cos)(0);
        SetLong(M() + 0x14, static_cast<std::int32_t>((U(c) << 5) + U(Long(Owner() + 0x38))));
    }
    SetLong(M() + 0x18, Long(Owner() + 0x3C));
    RandColours();
    SetWord(M() + 0xE, 0);
    M()[3] = 7;
    M()[9] = 0;
    M()[0xA] = 0x40;
    {
        const unsigned r = RandByte();
        M()[0xB] = static_cast<unsigned char>(r);
    }
    Inc(M()[2]);
}

// original 0x4B3BA0: the angle on by 0x20; the point; +9 up by 2, +0xB up;
// at +9 0x10 +2 on.
S13_EXPORT void __cdecl SuddenDeathOrbit_Brighten(void) {
    Turn(0x20);
    OrbitPoint();
    unsigned char* const m = M();
    AddB(m[9], 2);
    Inc(m[0xB]);
    if (m[9] == 0x10) Inc(m[2]);
}

// original 0x4B3C40: the angle on by 0x20; the point; +0xB up; +0xA down, at
// 0 +0xA again (0x40 while the owner is at its burst, step 3; else 1) and +2
// on.
S13_EXPORT void __cdecl SuddenDeathOrbit_Circle(void) {
    Turn(0x20);
    OrbitPoint();
    unsigned char* const m = M();
    Inc(m[0xB]);
    Dec(m[0xA]);
    if (m[0xA] != 0) return;
    m[0xA] = Owner()[2] != 3 ? 1 : 0x40;
    Inc(m[2]);
}

namespace {
// 0x4B3CF0 / 0x4B3DB0: while the owner is at its burst (step 3) +0xB up by
// `fast` and the angle by 0x40, else by 1 and 0x20; the point.
void OrbitStep(unsigned fast) {
    if (Owner()[2] == 3) {
        AddB(M()[0xB], fast);
        Turn(0x40);
    } else {
        Inc(M()[0xB]);
        Turn(0x20);
    }
    OrbitPoint();
}
}  // namespace

// original 0x4B3CF0: the step (4); +0xA down, at 0 +2 on.
S13_EXPORT void __cdecl SuddenDeathOrbit_Spin(void) {
    OrbitStep(4);
    unsigned char* const m = M();
    Dec(m[0xA]);
    if (m[0xA] == 0) Inc(m[2]);
}

// original 0x4B3DB0: the step (2); +9 down, at 0 the owner's +0xB down and
// the mote freed.
S13_EXPORT void __cdecl SuddenDeathOrbit_Fade(void) {
    OrbitStep(2);
    unsigned char* const m = M();
    Dec(m[9]);
    if (m[9] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::SuddenDeathMote_Free);
}

// original 0x4B3E80: the first mote without bit 0, marked and its index
// answered; 0xFF when all 96 are in use. Only al is the original's answer.
S13_EXPORT unsigned char __cdecl SuddenDeathMote_Alloc(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = MoteAt(i);
        if ((m[0] & 1) != 0) continue;
        m[0] = static_cast<unsigned char>(m[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4B3ED0: bytes +0..+4 of the current mote cleared, the cell read
// again for each.
S13_EXPORT void __cdecl SuddenDeathMote_Free(void) {
    for (unsigned k = 0; k < 5; ++k) M()[k] = 0;
}

void MagicS13_Inject() {
    if (bof3::WantsShadow("magic_s13")) magic_s13::SelfTest();
    BOF3_INJECT(SuddenDeath_Task);
    BOF3_INJECT(SuddenDeath_Spawn);
    BOF3_INJECT(SuddenDeathChild_Task);
    BOF3_INJECT(SuddenDeathChild_Run);
    BOF3_INJECT(SuddenDeathChild_Start);
    BOF3_INJECT(SuddenDeathChild_Delay);
    BOF3_INJECT(SuddenDeathChild_WaitMotes);
    BOF3_INJECT(SuddenDeathChild_Burst);
    BOF3_INJECT(SuddenDeathChild_End);
    BOF3_INJECT(SuddenDeathMote_Task);
    BOF3_INJECT(SuddenDeathBurst_Run);
    BOF3_INJECT(SuddenDeathBurst_Launch);
    BOF3_INJECT(SuddenDeathBurst_Close);
    BOF3_INJECT(SuddenDeathBurst_Spin);
    BOF3_INJECT(SuddenDeathBurst_Rise);
    BOF3_INJECT(SuddenDeathMote_PushMatrix);
    BOF3_INJECT(SuddenDeathMote_Draw);
    BOF3_INJECT(SuddenDeathOrbit_Run);
    BOF3_INJECT(SuddenDeathOrbit_Start);
    BOF3_INJECT(SuddenDeathOrbit_Brighten);
    BOF3_INJECT(SuddenDeathOrbit_Circle);
    BOF3_INJECT(SuddenDeathOrbit_Spin);
    BOF3_INJECT(SuddenDeathOrbit_Fade);
    BOF3_INJECT(SuddenDeathMote_Alloc);
    BOF3_INJECT(SuddenDeathMote_Free);
}

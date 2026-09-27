// Two spell overlays compiled into the exe, round nine group S32
// (docs/magic_s32.md): the PSX's MAGIC144 and MAGIC150.EMI, Magic_Rows rows
// 67 and 72. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Wall of Fire and Eye Beam; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC144 0x4E9140..0x4E9A66: a kind-2 task that walks a pool of its own
//     (48 records of 0x84 at 0x6A5680) and one kind-1 child (parameter 0x36)
//     at the source sprite: the child spawns three pool motes (a flame and two
//     sparks), darkens the source and flags the target, and draws a glowing
//     disc under the actor's matrix. The flame draws with MAGIC092's
//     Magic092_DrawFlame (group S20); MAGIC092 in turn runs this overlay's
//     flame and spark steps, the disc and the spark draw by address - the
//     two overlays are one effect's two variants;
//   - MAGIC150 0x4EA450..0x4EAE61: a seven-step task over a state block of its
//     own (0x6A9040..0x6A9107): eight sparks gather at a point in front of the
//     caster, then a beam - a ring of flat quads round the x axis and a
//     spiral ribbon of 0x80 gouraud quads jittered by Rand - widens, fires
//     (the target flagged 0x40) and narrows.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// table read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. No divergence: each is a faithful
// replacement, except that a phase past one of the two stack tables or the
// five .data tables aborts where the original would call through whatever
// follows it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s32.h"

#include <cstdint>
#include <cstring>

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
// sixteen bytes (0x903850..0x90385F) and the SVECTORs of Prim_VertexScratch
// (0x9037A0..). Both are read again after every call, as the originals read
// them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// MAGIC144's pool: 48 task-like records of 0x84 bytes (+0 bit 0 in use, +1
// kind, +2 step, +0x80 owner), walked by WallOfFire_Task.
constexpr std::uint32_t kPool = 0x6A5680;
constexpr unsigned kPoolCount = 0x30;
constexpr std::uint32_t kRecord = 0x84;

// MAGIC144's .data dispatch tables (symbols.toml, read in place).
constexpr std::uint32_t kChildTaskTable = 0x65BDC4;   // by +1: one entry
constexpr std::uint32_t kChildSteps = 0x65BDC8;       // by +2: four
constexpr std::uint32_t kMoteKinds = 0x65BDD8;        // by +1: flame, spark
constexpr std::uint32_t kFlameSteps = 0x65BDE0;       // by +2: four
constexpr std::uint32_t kSparkSteps = 0x65BDF0;       // by +2: four

// MAGIC150's state block (EyeBeam_State): the beam's origin x, z, height
// (dwords), the cylinder's radius, the spiral's radius and the spiral's angle
// (words); the eight sparks (EyeBeam_Sparks, 8 bytes each: +0 live, +2
// radius, +4 angle, +6 disc radius), the spark pointer cell, and two spiral
// points of 0x34 bytes (the previous one, the new one).
constexpr std::uint32_t kBeamX = 0x6A9040;
constexpr std::uint32_t kBeamZ = 0x6A9044;
constexpr std::uint32_t kBeamH = 0x6A9048;
constexpr std::uint32_t kBeamR1 = 0x6A9050;
constexpr std::uint32_t kBeamR2 = 0x6A9052;
constexpr std::uint32_t kBeamAngle = 0x6A9054;
constexpr std::uint32_t kSparks = 0x6A9058;
constexpr std::uint32_t kSparkCell = 0x6A9098;
constexpr std::uint32_t kSpiralLast = 0x6A90A0;
constexpr std::uint32_t kSpiralNew = 0x6A90D4;
constexpr unsigned kSpiralPoint = 0x34;

// Capcom's, unnamed, outside the band and in no group: the map camera's
// rotation and translation (no arguments), a world point (x, z, height << 16)
// projected into a vertex (two floats and a depth), and the sign of the cross
// product of three projected vertices, truncated by _ftol (docs/magic_c2.md
// section 5 reads the first two).
constexpr std::uint32_t kSetMapCamera = 0x494060;
constexpr std::uint32_t kProjectPoint = 0x494110;
constexpr std::uint32_t kFacing = 0x4941B0;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char* PoolRecord(unsigned i) { return Mem(kPool + i * kRecord); }
unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
int SS(unsigned k) { return static_cast<short>(SW(k)); }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }
int S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
void AddWord(unsigned char* p, unsigned v) { SetWord(p, (Word(p) + v) & 0xFFFF); }
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
// `imul` then `sar n`: the 32-bit product wraps, the shift is arithmetic.
int MulSar(int a, int b, unsigned n) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}
// `imul` then `shl 4`.
int MulShl4(int a, int b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b) << 4);
}

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// `fild dword i; fadd dword [f]; fstp dword [out]`, on the x87 as the original
// does it (the precision control the game set rounds the sum, fstp to float).
void AddIntToFloat(unsigned char* out, int i, const unsigned char* f) {
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(out))
                     : "m"(i), "m"(*reinterpret_cast<const float*>(f))
                     : "st");
}
// `fld dword [f]; fst [a]; fst [b]; fstp [c]`: one float through the x87 into
// three cells.
void CopyFloat3(const unsigned char* f, unsigned char* a, unsigned char* b, unsigned char* c) {
    __asm__ volatile("flds %3\n\tfsts %0\n\tfsts %1\n\tfstps %2"
                     : "=m"(*reinterpret_cast<float*>(a)), "=m"(*reinterpret_cast<float*>(b)),
                       "=m"(*reinterpret_cast<float*>(c))
                     : "m"(*reinterpret_cast<const float*>(f))
                     : "st");
}

int RandCall() { return MH_CALL(Rand)(); }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions and Capcom's called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using PtrFn = void (__cdecl*)(unsigned char*);
using ProjectFn = void (__cdecl*)(const std::int32_t*, unsigned char*);
using FacingFn = int (__cdecl*)(const unsigned char*, const unsigned char*, const unsigned char*);
using DiscFn = void (__cdecl*)(const unsigned char*, unsigned, unsigned, unsigned);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Project(const std::int32_t* v, unsigned char* out) { MH_AT(ProjectFn, kProjectPoint)(v, out); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only - what
// follows is the next table.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The GTE projection as the originals push it: one pointer more than the
// prototype names (the flag, in the caller's frame).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
#define S32_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))
void Rtp3(unsigned char* prim) {
    long p, flag;
    S32_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// The draw-mode packet of MAGIC150's draws: texture page Gpu_GetTPage(0, 1,
// 0x2C0, 0x100), committed to layer 1.
void BeamDrawMode() {
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100) & 0xFFFFu;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
}

// A task's position (+0x34, +0x38, +0x3C) from another's, read one dword at a
// time as the originals read it.
void CopyPosition(unsigned char* to, const unsigned char* from) {
    SetLong(to + 0x34, Long(from + 0x34));
    SetLong(to + 0x38, Long(from + 0x38));
    SetLong(to + 0x3C, Long(from + 0x3C));
}

}  // namespace

#define S32_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC144 (row 67, Wall of Fire read one id down)

// original 0x4E9140: the kind-2 task. A two-entry stack table by +1
// (WallOfFire_Start, MagicFx_EndWhenChildrenDone); then every pool record with
// +0 bit 0 run (WallOfFireMote_Run) with Sprite_Current the record and
// 0x93B940 its +0x80, both put back to the values read after the phase.
S32_EXPORT void __cdecl WallOfFire_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::WallOfFire_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    StackCall(kPhases, 2, Sc()[1], "WallOfFire_Task");
    unsigned char* const sc = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const m = PoolRecord(i);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = Long(m + 0x80);
        Sprite_Current = m;
        SetLong(Mem(at::kOwner), its);
        Call0(bof3::addr::WallOfFireMote_Run);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = sc;
    }
}

// original 0x4E91C0: +0xB 0; the pool's +0 / +1 / +2 cleared; one child (kind
// 1, parameter 0x36) owned by this task, +1 0, at the source sprite's position
// (the pointer 0x904B4C read before the create, the record after); +0xB 1;
// CLUT strip row 26: cells 1..15 from the source with bit 15, cell 0 cleared;
// dirty; +1 on. The slot is unchecked (0xFF writes past the slots).
S32_EXPORT void __cdecl WallOfFire_Start(void) {
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const m = PoolRecord(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    const unsigned char* const source = Pointer(at::kSource);
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x36) & 0xFFu;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(sc)));
    child[1] = 0;
    CopyPosition(child, source);
    sc[0xB] = 1;
    for (unsigned k = 0x1A01; k < 0x1A10; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = 0;
    Gfx_ClutStripDirty = 1;
    Inc(Sprite_Current[1]);
}

// original 0x4E9270: the child (kind 1, parameter 0x36): a jmp through
// WallOfFireChild_TaskTable (one entry, WallOfFireChild_Run) by +1.
S32_EXPORT void __cdecl WallOfFireChild_Task(void) { CellCall(kChildTaskTable, 1, Sc()[1], "WallOfFireChild_Task"); }

// original 0x4E9290: a call through WallOfFireChild_Steps (four: _Spawn,
// _Tint, _WaitFlame, _End) by +2; then while +0 and +2 are set the actor's
// matrix (MagicFx_PushActorMatrix), the disc (WallOfFire_DrawDisc), the
// matrix popped (a tail jmp).
S32_EXPORT void __cdecl WallOfFireChild_Run(void) {
    CellCall(kChildSteps, 4, Sc()[2], "WallOfFireChild_Run");
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::WallOfFire_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E92D0: +9 and +0xB 0; three pool records (WallOfFire_PoolAlloc),
// each one the pool answers owned by this child, +0 |= 0x41, +1 its kind (0
// the flame for the first, 1 a spark after), +2 0, +0xB its number, and
// counted in +0xB; sound 0x100; +2 on.
S32_EXPORT void __cdecl WallOfFireChild_Spawn(void) {
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    for (unsigned n = 0; n < 3; ++n) {
        const unsigned char index = MH_AT(ByteFn, bof3::addr::WallOfFire_PoolAlloc)();
        if (index == 0xFF) continue;
        unsigned char* const sc = Sprite_Current;
        unsigned char* const m = PoolRecord(index);
        SetLong(m + 0x80, static_cast<std::int32_t>(Key(sc)));
        m[0] = static_cast<unsigned char>(m[0] | 0x41);
        m[1] = n != 0 ? 1 : 0;
        m[2] = 0;
        m[0xB] = static_cast<unsigned char>(n);
        Inc(sc[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    Inc(Sprite_Current[2]);
}

// original 0x4E9370: +9 up by 2; at 0x10 the source sprite's tints released
// and a tint (-8, -8, -8, 1) set on it, the target flagged 0x40, +2 on.
S32_EXPORT void __cdecl WallOfFireChild_Tint(void) {
    AddB(Sc()[9], 2);
    if (Sc()[9] != 0x10) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(at::kSource));
    MH_CALL(Sprite_SetTint)(Pointer(at::kSource), 0xF8, 0xF8, 0xF8, 1);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Inc(Sprite_Current[2]);
}

// original 0x4E93C0: +2 on once +0xB has bit 7 (the flame sets it,
// Magic092_FlameGrow).
S32_EXPORT void __cdecl WallOfFireChild_WaitFlame(void) {
    unsigned char* const s = Sc();
    if (s[0xB] & 0x80) Inc(s[2]);
}

// original 0x4E93D0: +9 down to 0; once +0xB is 0x80 (the flag with every mote
// gone): the source sprite's tints released, the target actor flashed, the
// owner's +0xB down, the task freed (a tail jmp).
S32_EXPORT void __cdecl WallOfFireChild_End(void) {
    unsigned char* s = Sc();
    if (s[9] != 0) {
        Dec(s[9]);
        s = Sc();
    }
    if (s[0xB] != 0x80) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(at::kSource));
    MH_CALL(BattleActor_Flash)(TargetByte());
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E9420: under the caller's matrix, a disc of eight
// semi-transparent gouraud triangles, radius 0x180 (word 0x903850), round
// the origin at z 0: each from the centre (shade word 0x90385E: +9 x 8 below
// 0x10, else 0x80, in all three channels) to two rim points (shade 1) at
// angles 0x200 apart, the first rim point the last one's second (vertex
// scratch +8 from +0x10); projected by Gte_RotTransPers3, depths by
// Gte_PrimDepths3_10B, committed at 5. Draw modes 0x55 before, 0x15 after.
S32_EXPORT void __cdecl WallOfFire_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetSW(0, 0x180);
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(MulSar(v, SS(0), 12)));
    v = Cos(0);
    SetVW(0x12, static_cast<unsigned>(MulSar(v, SS(0), 12)));
    const unsigned char b = Sc()[9];
    SetSW(0xE, b < 0x10 ? b * 8u : 0x80u);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), z = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, z);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(MulSar(v, SS(0), 12)));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(MulSar(v, SS(0), 12)));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        const unsigned char shade = Mem(kS + 0xE)[0];
        p[4] = shade;
        p[5] = shade;
        p[6] = shade;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4E95E0: a pool record: a jmp through WallOfFireMote_Kinds (two:
// the flame, the spark) by +1.
S32_EXPORT void __cdecl WallOfFireMote_Run(void) { CellCall(kMoteKinds, 2, Sc()[1], "WallOfFireMote_Run"); }

// original 0x4E9600: the flame. A call through WallOfFireFlame_Steps (four:
// _Start, _Rise, Magic092_FlameGrow, _Fade) by +2; then while +0 and +2 are
// set the screen point (BattleActor_UpdateScreenXY) and MAGIC092's flame
// (Magic092_DrawFlame, a tail jmp).
S32_EXPORT void __cdecl WallOfFireFlame_Run(void) {
    CellCall(kFlameSteps, 4, Sc()[2], "WallOfFireFlame_Run");
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(Magic092_DrawFlame)();
}

// original 0x4E9630: +0 bit 6 cleared; the owner's position; +9 and +0xA 1;
// +2 on.
S32_EXPORT void __cdecl WallOfFireFlame_Start(void) {
    Sc()[0] = static_cast<unsigned char>(Sc()[0] & 0xBF);
    CopyPosition(Sc(), Owner());
    Sc()[9] = 1;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4E9690: +9 up by 4; at 0x11 +2 on.
S32_EXPORT void __cdecl WallOfFireFlame_Rise(void) {
    AddB(Sc()[9], 4);
    if (Sc()[9] == 0x11) Inc(Sc()[2]);
}

// original 0x4E96B0: +9 down by 2, +0xA up on odd frames; at +9 1 the owner's
// +0xB down and +0..+4 cleared (the record free).
S32_EXPORT void __cdecl WallOfFireFlame_Fade(void) {
    AddB(Sc()[9], 0xFE);
    if (static_cast<unsigned char>(Frame_Counter) & 1) Inc(Sc()[0xA]);
    if (Sc()[9] != 1) return;
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x4E9720: a spark. A call through WallOfFireSpark_Steps (four:
// _Start, MagicFx_CountUp9By2, _Wait, _End) by +2; then while +0 and +2 are
// set the screen point, moved 16 left (+0xB 1) or right and 12 up, and the
// spark (WallOfFireSpark_Draw, a tail jmp).
S32_EXPORT void __cdecl WallOfFireSpark_Run(void) {
    CellCall(kSparkSteps, 4, Sc()[2], "WallOfFireSpark_Run");
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sprite_Current;
    AddWord(s + 0x2E, s[0xB] == 1 ? 0xFFF0u : 0x10u);
    AddWord(Sprite_Current + 0x30, 0xFFF4u);
    Call0(bof3::addr::WallOfFireSpark_Draw);
}

// original 0x4E9770: +0 bit 6 cleared; the owner's position; +9 0; +2 on.
S32_EXPORT void __cdecl WallOfFireSpark_Start(void) {
    Sc()[0] = static_cast<unsigned char>(Sc()[0] & 0xBF);
    CopyPosition(Sc(), Owner());
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4E97D0: +2 on once the owner's +0xB has bit 7.
S32_EXPORT void __cdecl WallOfFireSpark_Wait(void) {
    if (Owner()[0xB] & 0x80) Inc(Sc()[2]);
}

// original 0x4E97F0: +9 down; at 0 the owner's +0xB down and +0..+4 cleared.
S32_EXPORT void __cdecl WallOfFireSpark_End(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x4E9850: a burst of eight semi-transparent gouraud triangles in
// screen space round the task's screen point (+0x2E, +0x30), radius 0x40 +
// Rand & 15 (word 0x903850), each linked at the task's position (layer 2)
// after a draw-mode packet (0x35): the centre shade (+9 x 10, word 0x90385E)
// red, a fifth of it green and blue; the rim 1.
S32_EXPORT void __cdecl WallOfFireSpark_Draw(void) {
    SetSW(0, (static_cast<unsigned>(RandCall()) & 0xFu) + 0x40u);
    SetSW(0xE, Sc()[9] * 10u);
    for (int a = 0; a < 0x1000;) {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        {
            const unsigned char* const s = Sc();
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)),
                                        2, 0xC);
        }
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        int v = Sin(a);
        PutFloat(p + 0x18, Add(MulSar(v, SS(0), 12), S16(Sc() + 0x2E)));
        v = Cos(a);
        PutFloat(p + 0x1C, Add(MulSar(v, SS(0), 12), S16(Sc() + 0x30)));
        a += 0x200;
        v = Sin(a);
        PutFloat(p + 0x28, Add(MulSar(v, SS(0), 12), S16(Sc() + 0x2E)));
        v = Cos(a);
        PutFloat(p + 0x2C, Add(MulSar(v, SS(0), 12), S16(Sc() + 0x30)));
        p[4] = Mem(kS + 0xE)[0];
        const auto fifth = static_cast<unsigned char>(SS(0xE) / 5);
        p[5] = fifth;
        p[6] = fifth;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        const unsigned char* const s = Sc();
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                    0x34);
    }
}

// original 0x4E9A10: the first of the 48 pool records whose +0 lacks bit 0
// gets it; its index in al, 0xFF when none.
S32_EXPORT unsigned char __cdecl WallOfFire_PoolAlloc(void) {
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const m = PoolRecord(i);
        if ((m[0] & 1) == 0) {
            m[0] = static_cast<unsigned char>(m[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC150 (row 72, Eye Beam read one id down)

namespace {
std::int32_t BeamX() { return Long(Mem(kBeamX)); }
std::int32_t BeamZ() { return Long(Mem(kBeamZ)); }
std::int32_t BeamH() { return Long(Mem(kBeamH)); }
unsigned char* SparkCurrent() { return Pointer(kSparkCell); }
// A point of the beam at angle `a` round the x axis, radius `r` (a word read
// after the call): z = Z + cos x r >> 4, height = H + sin x r << 4, each
// origin read after its call.
std::int32_t RingZ(int c, int r) { return Add(MulSar(c, r, 4), BeamZ()); }
std::int32_t RingH(int s, int r) { return Add(MulShl4(s, r), BeamH()); }
}  // namespace

// original 0x4EA450: the kind-2 task. A seven-entry stack table by +1:
// EyeBeam_Start, _Charge, _WaitSparks, _Widen, _Fire, _Narrow,
// MagicFx_DoneAndFree.
S32_EXPORT void __cdecl EyeBeam_Task(void) {
    static constexpr std::uint32_t kPhases[7] = {bof3::addr::EyeBeam_Start,  bof3::addr::EyeBeam_Charge,
                                                 bof3::addr::EyeBeam_WaitSparks, bof3::addr::EyeBeam_Widen,
                                                 bof3::addr::EyeBeam_Fire,   bof3::addr::EyeBeam_Narrow,
                                                 bof3::addr::MagicFx_DoneAndFree};
    StackCall(kPhases, 7, Sc()[1], "EyeBeam_Task");
}

// original 0x4EA4A0: the beam's origin from the current slot's (0x93B8C4)
// owner: x + 0x14000, z, height + 0x2C00000; the two radii and the angle 0;
// the sparks cleared; +9 0x3C, +1 on; sound effect 0x100.
S32_EXPORT void __cdecl EyeBeam_Start(void) {
    const unsigned char* const slot = Pointer(at::kCurrentSlot);
    const auto* caster = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(slot + 0x80))));
    SetLong(Mem(kBeamX), Add(Long(caster + 0x34), 0x14000));
    SetLong(Mem(kBeamZ), Long(caster + 0x38));
    SetLong(Mem(kBeamH), Add(Long(caster + 0x3C), 0x2C00000));
    SetWord(Mem(kBeamR1), 0);
    SetWord(Mem(kBeamR2), 0);
    SetWord(Mem(kBeamAngle), 0);
    Call0(bof3::addr::EyeBeamSparks_Clear);
    Sc()[9] = 0x3C;
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayEffect)(0x100);
}

// original 0x4EA520: a spark spawned (EyeBeamSparks_Spawn) and the sparks
// drawn (EyeBeamSparks_Draw, its answer unread); +9 down; at 0 +1 on.
S32_EXPORT void __cdecl EyeBeam_Charge(void) {
    Call0(bof3::addr::EyeBeamSparks_Spawn);
    MH_AT(ByteFn, bof3::addr::EyeBeamSparks_Draw)();
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[1]);
}

// original 0x4EA550: the sparks drawn; once none is live, +9 4 and +1 on.
S32_EXPORT void __cdecl EyeBeam_WaitSparks(void) {
    if (MH_AT(ByteFn, bof3::addr::EyeBeamSparks_Draw)() != 0) return;
    Sc()[9] = 4;
    Inc(Sc()[1]);
}

// original 0x4EA570: the cylinder's radius up by 0x40, the spiral's by 0x60;
// the beam drawn; +9 down; at 0 +9 0x78, +1 on, sound effect 0x101.
S32_EXPORT void __cdecl EyeBeam_Widen(void) {
    AddWord(Mem(kBeamR1), 0x40);
    AddWord(Mem(kBeamR2), 0x60);
    Call0(bof3::addr::EyeBeam_Draw);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 0x78;
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayEffect)(0x101);
}

// original 0x4EA5C0: the spiral turned by -0x200; the cylinder's radius 0x100
// on even frames, 0x110 on odd; the beam drawn; +9 down; at 0 +9 4, +1 on,
// the target flagged 0x40, sound effect 0x102.
S32_EXPORT void __cdecl EyeBeam_Fire(void) {
    const unsigned frame = static_cast<unsigned char>(Frame_Counter);
    AddWord(Mem(kBeamAngle), 0xFE00);
    SetWord(Mem(kBeamR1), ((frame & 1u) + 0x10u) << 4);
    Call0(bof3::addr::EyeBeam_Draw);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 4;
    Inc(Sc()[1]);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    MH_CALL(Sound_PlayEffect)(0x102);
}

// original 0x4EA630: the radii down by 0x40 and 0x60; the beam drawn; +9 down;
// at 0 +1 on.
S32_EXPORT void __cdecl EyeBeam_Narrow(void) {
    AddWord(Mem(kBeamR1), 0xFFC0);
    AddWord(Mem(kBeamR2), 0xFFA0);
    Call0(bof3::addr::EyeBeam_Draw);
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[1]);
}

// original 0x4EA670: a draw-mode packet (layer 1), the map camera (0x494060),
// the cylinder and the spiral (a tail jmp).
S32_EXPORT void __cdecl EyeBeam_Draw(void) {
    BeamDrawMode();
    Call0(kSetMapCamera);
    Call0(bof3::addr::EyeBeam_DrawCylinder);
    Call0(bof3::addr::EyeBeam_DrawSpiral);
}

// original 0x4EA6C0: sixteen semi-transparent flat quads (red 0x80) round the
// x axis from the origin to 0x100000 along it: quad i between the angles
// 0x100 i and 0x100 (i + 1), radius the word 0x6A9050; each corner a world
// point projected by 0x494110 (a vector of the caller's, x changed between the
// two of an angle); committed at 1 only when 0x4941B0 of its first three
// vertices is negative in its low word (the quad faces the camera).
S32_EXPORT void __cdecl EyeBeam_DrawCylinder(void) {
    unsigned angle = 0;
    unsigned next = 0;   // the original's stack dword, stepped by 0x100
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyF4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        std::int32_t v[3];
        unsigned char out[12];
        v[0] = BeamX();
        int c = Cos(static_cast<int>(angle));
        v[1] = RingZ(c, S16(Mem(kBeamR1)));
        int s = Sin(static_cast<int>(angle));
        v[2] = RingH(s, S16(Mem(kBeamR1)));
        Project(v, out);
        std::memcpy(p + 8, out, 12);
        v[0] = Add(BeamX(), 0x100000);
        Project(v, out);
        next += 0x100;
        angle = next & 0xFFFF;
        std::memcpy(p + 0x14, out, 12);
        v[0] = BeamX();
        c = Cos(static_cast<int>(angle));
        v[1] = RingZ(c, S16(Mem(kBeamR1)));
        s = Sin(static_cast<int>(angle));
        v[2] = RingH(s, S16(Mem(kBeamR1)));
        Project(v, out);
        std::memcpy(p + 0x20, out, 12);
        v[0] = Add(BeamX(), 0x100000);
        Project(v, out);
        std::memcpy(p + 0x2C, out, 12);
        p[4] = 0x80;
        p[5] = 0;
        p[6] = 0;
        const int facing = MH_AT(FacingFn, kFacing)(p + 8, p + 0x14, p + 0x20);
        if (static_cast<short>(facing) < 0) MH_CALL(Gfx_CommitPrim)(1, 0x38);
    }
}

// original 0x4EA890: the map camera; a spiral of 0x80 segments along x from
// the origin (x read once, after the camera), the steps 0x1000, 0x1040, ...,
// each point jittered in x by ((Rand & 0xFFF) - 0x800) << 4, turned 0x80 on
// from the angle 0x6A9054 (only its low word counts), radius the word
// 0x6A9052: each new point (EyeBeam_SpiralNew) projected three ways
// (EyeBeam_ProjectSpiralPoint), the previous one copied to EyeBeam_SpiralLast
// first, and the segment between them drawn (EyeBeam_DrawSpiralSegment).
S32_EXPORT void __cdecl EyeBeam_DrawSpiral(void) {
    using PointFn = void (__cdecl*)(unsigned char*);
    Call0(kSetMapCamera);
    std::uint32_t angle = static_cast<std::uint32_t>(Long(Mem(kBeamAngle)));
    std::int32_t x = BeamX();
    std::int32_t step = 0x1000;
    unsigned char* const point = Mem(kSpiralNew);
    SetLong(point, x);
    unsigned a = angle & 0xFFFF;
    int c = Cos(static_cast<int>(a));
    SetLong(point + 4, RingZ(c, S16(Mem(kBeamR2))));
    int s = Sin(static_cast<int>(a));
    SetLong(point + 8, RingH(s, S16(Mem(kBeamR2))));
    MH_AT(PointFn, bof3::addr::EyeBeam_ProjectSpiralPoint)(point);
    for (unsigned n = 0x80; n != 0; --n) {
        x = Add(x, step);
        step += 0x40;
        for (unsigned k = 0; k < kSpiralPoint; k += 4) SetLong(Mem(kSpiralLast + k), Long(Mem(kSpiralNew + k)));
        angle += 0x80;
        const int r = RandCall();
        SetLong(point, Add(static_cast<std::int32_t>((static_cast<std::uint32_t>(r & 0xFFF) - 0x800u) << 4), x));
        a = angle & 0xFFFF;
        c = Cos(static_cast<int>(a));
        SetLong(point + 4, RingZ(c, S16(Mem(kBeamR2))));
        s = Sin(static_cast<int>(a));
        SetLong(point + 8, RingH(s, S16(Mem(kBeamR2))));
        MH_AT(PointFn, bof3::addr::EyeBeam_ProjectSpiralPoint)(point);
        MH_AT(PointFn, bof3::addr::EyeBeam_DrawSpiralSegment)(Mem(kSpiralLast));
    }
}

// original 0x4EA9D0: a spiral point (x, z, height at +0, +4, +8) projected
// three times by 0x494110: as it is into +0x1C, 0x8000 back in x into +0x10,
// 0x8000 on into +0x28 (x left 0x8000 on).
S32_EXPORT void __cdecl EyeBeam_ProjectSpiralPoint(unsigned char* point) {
    const auto* v = reinterpret_cast<const std::int32_t*>(point);
    Project(v, point + 0x1C);
    SetLong(point, Add(Long(point), -0x8000));
    Project(v, point + 0x10);
    SetLong(point, Add(Long(point), 0x10000));
    Project(v, point + 0x28);
}

// original 0x4EAA10: the segment from a spiral point to the next (the next
// 0x34 bytes on): two semi-transparent gouraud quads (yellow 0x40 on the
// first and third corners, black on the others), the second a copy of the
// first packet with two corners replaced, then a flat line (0x20) between the
// two points' middles; all committed at 1.
S32_EXPORT void __cdecl EyeBeam_DrawSpiralSegment(unsigned char* point) {
    unsigned char* const next = point + kSpiralPoint;
    unsigned char* p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    std::memcpy(p + 8, point + 0x1C, 12);
    std::memcpy(p + 0x18, point + 0x10, 12);
    std::memcpy(p + 0x28, next + 0x1C, 12);
    std::memcpy(p + 0x38, next + 0x10, 12);
    p[4] = 0x40;
    p[5] = 0x40;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0x40;
    p[0x25] = 0x40;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    // `rep movsd` from the 0x44 bytes before the packet pointer: the quad just
    // committed, whichever the pointer is now.
    p = Gfx_PacketNext;
    for (unsigned k = 0; k < 0x44; k += 4) SetLong(p + k, Long(p - 0x44 + k));
    std::memcpy(p + 0x18, point + 0x28, 12);
    std::memcpy(p + 0x38, next + 0x28, 12);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetLineF2)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    std::memcpy(p + 8, point + 0x1C, 12);
    std::memcpy(p + 0x14, next + 0x1C, 12);
    p[6] = 0;
    p[4] = 0x20;
    p[5] = 0x20;
    MH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x4EAB70: the eight sparks' +0 words cleared through the pointer
// cell 0x6A9098 (left one past the last).
S32_EXPORT void __cdecl EyeBeamSparks_Clear(void) {
    SetLong(Mem(kSparkCell), static_cast<std::int32_t>(kSparks));
    for (unsigned n = 8; n != 0; --n) {
        SetWord(SparkCurrent(), 0);
        SetLong(Mem(kSparkCell), Add(Long(Mem(kSparkCell)), 8));
    }
}

// original 0x4EABA0: the first spark whose +0 word is 0 (the pointer cell left
// at it, or one past the last when none): +0 1, radius +2 0x300, angle +4 (Rand
// & 0xFF) << 4, disc radius +6 8 - each written through the cell, read again
// after Rand.
S32_EXPORT void __cdecl EyeBeamSparks_Spawn(void) {
    std::uint32_t e = kSparks;
    SetLong(Mem(kSparkCell), static_cast<std::int32_t>(e));
    for (unsigned n = 0;; ) {
        if (Word(Mem(e)) == 0) break;
        e += 8;
        ++n;
        SetLong(Mem(kSparkCell), static_cast<std::int32_t>(e));
        if (n >= 8) return;
    }
    SetWord(Mem(e), 1);
    SetWord(SparkCurrent() + 2, 0x300);
    const int r = RandCall();
    SetWord(SparkCurrent() + 4, (static_cast<unsigned>(r) & 0xFFu) << 4);
    SetWord(SparkCurrent() + 6, 8);
}

// original 0x4EAC00: a draw-mode packet (layer 1) and the map camera; then
// each live spark (walked through the pointer cell, re-read after every call)
// drawn as a disc (EyeBeamSpark_DrawDisc: shade 0x80 at the centre, radius
// its +6) at the origin x and angle +4 round the x axis at radius +2 (z by
// cos >> 4, height by sin << 4); its angle on 0x80, radius down 0x60, disc
// radius down 1; freed at radius 0. Answers in al whether any was live.
S32_EXPORT unsigned char __cdecl EyeBeamSparks_Draw(void) {
    BeamDrawMode();
    Call0(kSetMapCamera);
    std::uint32_t e = kSparks;
    unsigned char any = 0;
    SetLong(Mem(kSparkCell), static_cast<std::int32_t>(e));
    for (unsigned n = 8; n != 0; --n) {
        if (Word(Mem(e)) != 0) {
            std::int32_t v[3];
            unsigned char out[12];
            v[0] = BeamX();
            const int c = Cos(Word(Mem(e) + 4));
            unsigned char* cur = SparkCurrent();
            v[1] = Add(MulSar(c, S16(cur + 2), 4), BeamZ());
            const int s = Sin(Word(cur + 4));
            cur = SparkCurrent();
            v[2] = Add(MulShl4(s, S16(cur + 2)), BeamH());
            Project(v, out);
            cur = SparkCurrent();
            MH_AT(DiscFn, bof3::addr::EyeBeamSpark_DrawDisc)(out, Word(cur + 6), 0x80, 0);
            AddWord(SparkCurrent() + 4, 0x80);
            AddWord(SparkCurrent() + 2, 0xFFA0);
            AddWord(SparkCurrent() + 6, 0xFFFF);
            e = static_cast<std::uint32_t>(Long(Mem(kSparkCell)));
            any = 1;
            if (Word(Mem(e) + 2) == 0) {
                SetWord(Mem(e), 0);
                e = static_cast<std::uint32_t>(Long(Mem(kSparkCell)));
            }
        }
        e += 8;
        SetLong(Mem(kSparkCell), static_cast<std::int32_t>(e));
    }
    return any;
}

// original 0x4EAD40: a disc of sixteen semi-transparent gouraud triangles in
// screen space round a projected point (x, y floats and a depth at `centre`),
// radius the low word of `radius`: the centre coloured red `shade` (a byte),
// the rim red `rim`; each rim point cos / sin x radius >> 12 added on the
// x87 to the centre's floats, all three at the centre's depth; committed at 1.
S32_EXPORT void __cdecl EyeBeamSpark_DrawDisc(const unsigned char* centre, unsigned radius, unsigned shade,
                                              unsigned rim) {
    const int r = static_cast<short>(radius & 0xFFFF);
    std::uint32_t a = 0, b = 0x100;
    for (;;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetLong(p + 8, Long(centre));
        SetLong(p + 0xC, Long(centre + 4));
        int v = MulSar(Cos(static_cast<int>(a)), r, 12);
        AddIntToFloat(p + 0x18, v, centre);
        v = MulSar(Sin(static_cast<int>(a)), r, 12);
        const unsigned b16 = b & 0xFFFF;
        AddIntToFloat(p + 0x1C, v, centre + 4);
        v = MulSar(Cos(static_cast<int>(b16)), r, 12);
        AddIntToFloat(p + 0x28, v, centre);
        v = MulSar(Sin(static_cast<int>(b16)), r, 12);
        AddIntToFloat(p + 0x2C, v, centre + 4);
        CopyFloat3(centre + 8, p + 0x30, p + 0x20, p + 0x10);
        p[4] = static_cast<unsigned char>(shade);
        p[5] = 0;
        p[6] = 0;
        p[0x14] = static_cast<unsigned char>(rim);
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = static_cast<unsigned char>(rim);
        p[0x25] = 0;
        p[0x26] = 0;
        MH_CALL(Gfx_CommitPrim)(1, 0x34);
        const std::uint32_t was = b;
        b += 0x100;
        a += 0x100;
        if ((was & 0xFFFF) >= 0x1000) break;
    }
}

void MagicS32_Inject() {
    if (bof3::WantsShadow("magic_s32")) magic_s32::SelfTest();
    BOF3_INJECT(WallOfFire_Task);
    BOF3_INJECT(WallOfFire_Start);
    BOF3_INJECT(WallOfFireChild_Task);
    BOF3_INJECT(WallOfFireChild_Run);
    BOF3_INJECT(WallOfFireChild_Spawn);
    BOF3_INJECT(WallOfFireChild_Tint);
    BOF3_INJECT(WallOfFireChild_WaitFlame);
    BOF3_INJECT(WallOfFireChild_End);
    BOF3_INJECT(WallOfFire_DrawDisc);
    BOF3_INJECT(WallOfFireMote_Run);
    BOF3_INJECT(WallOfFireFlame_Run);
    BOF3_INJECT(WallOfFireFlame_Start);
    BOF3_INJECT(WallOfFireFlame_Rise);
    BOF3_INJECT(WallOfFireFlame_Fade);
    BOF3_INJECT(WallOfFireSpark_Run);
    BOF3_INJECT(WallOfFireSpark_Start);
    BOF3_INJECT(WallOfFireSpark_Wait);
    BOF3_INJECT(WallOfFireSpark_End);
    BOF3_INJECT(WallOfFireSpark_Draw);
    BOF3_INJECT(WallOfFire_PoolAlloc);
    BOF3_INJECT(EyeBeam_Task);
    BOF3_INJECT(EyeBeam_Start);
    BOF3_INJECT(EyeBeam_Charge);
    BOF3_INJECT(EyeBeam_WaitSparks);
    BOF3_INJECT(EyeBeam_Widen);
    BOF3_INJECT(EyeBeam_Fire);
    BOF3_INJECT(EyeBeam_Narrow);
    BOF3_INJECT(EyeBeam_Draw);
    BOF3_INJECT(EyeBeam_DrawCylinder);
    BOF3_INJECT(EyeBeam_DrawSpiral);
    BOF3_INJECT(EyeBeam_ProjectSpiralPoint);
    BOF3_INJECT(EyeBeam_DrawSpiralSegment);
    BOF3_INJECT(EyeBeamSparks_Clear);
    BOF3_INJECT(EyeBeamSparks_Spawn);
    BOF3_INJECT(EyeBeamSparks_Draw);
    BOF3_INJECT(EyeBeamSpark_DrawDisc);
}

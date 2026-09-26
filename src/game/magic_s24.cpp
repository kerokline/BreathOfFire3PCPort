// Spell group S24 of round nine (docs/magic_s24.md): three BMAGIC overlays of
// Magic_Rows compiled into the exe, 47 functions.
//
//   MAGIC104, row 14 (read one id down: Sirocco): a kind-2 task that rings the
//     targets with eight whirls twice over - each a kind-1 child drawing a
//     funnel of POLY_G4, a disc of POLY_G3 and a ring of POLY_G4 - then a
//     second ring of shrinking spheres, and ends with the targets' 0x40 flag.
//   MAGIC105, row 64 (read one id down: Kyrie): a kind-2 task with seven orbs
//     (kind-1 children, drawn as stacked POLY_G4 bands) that shed motes into a
//     64-record pool of its own; each mote is a sprite that drifts and sinks.
//   MAGIC106, row 104 (read one id down: Death): a kind-2 task with a pool of
//     96 sparks that close in on the owner, spin, rise and fade, each drawn as
//     four POLY_G3 shards.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a direct call to another function of the group goes by its address too
// (Phase), which in the game is the jmp Inject put there to ours.
//
// No divergence: each function is a faithful replacement, except that a
// dispatch past its table - a stack table's or a .data table's - aborts where
// the original would call through whatever follows (docs/magic_fx_reached.md
// section 3, the precedent). Where the original reads memory again after a
// call (Sprite_Current, the owner, the pools' current spark, the scratch
// words), ours reads it again.
#include "game/magic_s24.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s24_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
namespace cell = magic_s24::cell;
namespace tbl = magic_s24::tbl;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// --- small helpers ---------------------------------------------------------

// The original's imul: 32 bits, wrapping.
inline int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
inline int Shl(int a, int n) { return static_cast<int>(static_cast<std::uint32_t>(a) << n); }
inline std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
inline std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
inline short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
inline short G16(std::uint32_t address) { return static_cast<short>(Word(Mem(address))); }
inline void Put16(std::uint32_t address, int v) { SetWord(Mem(address), static_cast<unsigned>(v) & 0xFFFFu); }
inline std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
inline void SetPtr(unsigned char* at_, const void* p) { SetLong(at_, static_cast<std::int32_t>(Addr(p))); }
inline void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
inline void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
inline void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
inline unsigned char* Owner() { return Pointer(at::kOwner); }
inline unsigned char* TaskSlot(unsigned index) { return Mem(at::kTasks + (index & 0xFFu) * at::kTaskStride); }
inline unsigned char* Mote(unsigned index) { return Mem(cell::kMotePool + (index & 0xFFu) * cell::kMoteStride); }
inline unsigned char* SparkRec(unsigned index) { return Mem(cell::kSparkPool + (index & 0xFFu) * cell::kSparkStride); }
inline unsigned char* Spark() { return Pointer(cell::kSparkCurrent); }

// A .data table of handlers, read in place as the original reads it (during
// the fuzz its entries are the harness's recorders).
Handler DataPhase(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Callees.
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }
using TurnFn = void (__cdecl*)(unsigned char*);
using AllocFn = unsigned char (__cdecl*)();
void CentreOnTargets() { MH_AT(Handler, magic_s24::kCentreOnTargets)(); }
void TurnByFacing(unsigned char* task) { MH_AT(TurnFn, magic_s24::kTurnByFacing)(task); }
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The scratch words (DamageScratch): radii at +0 / +2, then colours or angles.
constexpr std::uint32_t kR = cell::kScratch, kR2 = cell::kScratch + 2;
constexpr std::uint32_t kW4 = cell::kScratch + 4, kW6 = cell::kScratch + 6, kW8 = cell::kScratch + 8;
constexpr std::uint32_t kWA = cell::kScratch + 0xA, kWC = cell::kScratch + 0xC, kWE = cell::kScratch + 0xE;
// The four SVECTORs of Prim_VertexScratch, as the originals name them: a (+0), b (+8), c (+0x10), d (+0x18).
constexpr std::uint32_t kA = cell::kVertices, kB = cell::kVertices + 8, kC = cell::kVertices + 0x10, kD = cell::kVertices + 0x18;

const short* Vec(std::uint32_t address) { return reinterpret_cast<const short*>(Mem(address)); }
float* Xy(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

// The four-vertex projection every POLY_G4 of the group makes: a b c d into
// the primitive's four points, then its depth.
void Project4(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers4)(Vec(kA), Vec(kB), Vec(kC), Vec(kD), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), Xy(p, 0x38), &depth);
    MH_CALL(Gte_PrimDepths4_10B)(p);
}
void Project3(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers3)(Vec(kA), Vec(kB), Vec(kC), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), &depth);
    MH_CALL(Gte_PrimDepths3_10B)(p);
}

// The matrix every draw of the group pushes: the angles rotated, times the
// camera, translated by the effect's point.
struct Matrix { short m[3][3]; short pad; std::int32_t t[3]; };
void PushMatrix(const short vector[3], const short angles[3]) {
    Matrix m;
    MH_CALL(Gte_RotTrans)(vector, reinterpret_cast<long*>(m.t));
    MH_CALL(Gte_RotMatrix)(angles, &m.m[0][0]);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, &m.m[0][0], &m.m[0][0]);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}
// The effect point as the GTE takes it: (x >> 9) - 0x4000, (z >> 9) - 0x4000,
// minus half the height (rounded toward 0).
void PointOf(short v[3], std::int32_t x, std::int32_t z, short height) {
    v[0] = static_cast<short>((x >> 9) - 0x4000);
    v[1] = static_cast<short>((z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(height / 2));
}

// A child of a ring: BattleTask_Create(1, parameter), its owner the current
// task, its offset from the pair table turned by the facing, placed at the
// field's kind-2 point plus it. The original's order of reads and writes,
// which matters when the new slot is the current one.
unsigned char* SpawnRingChild(unsigned parameter, unsigned phase, unsigned index, unsigned char spin) {
    unsigned char* const t = TaskSlot(MH_CALL(BattleTask_Create)(1, parameter));
    const std::int32_t dx = Long(Mem(tbl::kChildOffsets + 8 * index));
    unsigned char* const sc = Sprite_Current;
    SetPtr(t + 0x80, sc);
    t[1] = static_cast<unsigned char>(phase);
    t[0xB] = static_cast<unsigned char>(index);
    t[9] = 0;
    t[0xA] = spin;
    SetLong(sc + 0xC, dx);
    SetLong(Sprite_Current + 0x10, Long(Mem(tbl::kChildOffsets + 8 * index + 4)));
    TurnByFacing(Sprite_Current);
    const unsigned char* const s = Sprite_Current;
    SetLong(t + 0x34, Add(Long(s + 0xC), Field_Kind2X));
    SetLong(t + 0x38, Add(Long(s + 0x10), Field_Kind2Z));
    SetLong(t + 0x3C, Long(s + 0x3C));
    return t;
}

}  // namespace

#define S24_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC104 (row 14)

// original 0x4D04B0: the kind-2 task. Its phase +1 through a four-entry table
// the original builds on its stack - Start, WaitFirstRing, SecondRing, End -
// unchecked (ours aborts past it).
S24_EXPORT void __cdecl Fx104_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Fx104_Start, bof3::addr::Fx104_WaitFirstRing,
                                                 bof3::addr::Fx104_SecondRing, bof3::addr::Fx104_End};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 4) bof3::Fatal("Fx104_Task: phase %u, past the four-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D04F0: the facing from the acting actor's record (+8, flipped
// by 2 when the target side's 0x40 bit and "the actor is a party member"
// agree), the targets' centre (0x4FC0E0), +0xB 0, the phase on, then eight
// whirls round the centre: kind-1 children of parameter 0xB, phase 1, index
// i, spin 2i, each with Sound_PlayById(0x102).
S24_EXPORT void __cdecl Fx104_Start(void) {
    const unsigned char target = Mem(at::kTarget)[0];
    const unsigned char actor = Mem(at::kActor)[0];
    unsigned char facing = Pointer(cell::kActorRecord)[8];
    if (((target & 0x40) != 0) == (actor < 3)) facing ^= 2;
    Sprite_Current[8] = facing;
    CentreOnTargets();
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
    for (unsigned i = 0; i < 8; ++i) {
        SpawnRingChild(0xB, 1, i, static_cast<unsigned char>(i * 2));
        MH_CALL(Sound_PlayById)(0x102);
    }
}

// original 0x4D0600: wait for the ring's last whirl (it sets the task's +0xB
// to its own index, 7, as it ends); then +9 8, on.
S24_EXPORT void __cdecl Fx104_WaitFirstRing(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[0xB] != 7) return;
    sc[9] = 8;
    Bump(Sprite_Current[1]);
}

// original 0x4D0620: +9 down; at 0, +0xB 0, the phase on, and eight bursts:
// kind-1 children of parameter 0xB, phase 0, index i, delay 16i + 1 - no sound.
S24_EXPORT void __cdecl Fx104_SecondRing(void) {
    unsigned char* sc = Sprite_Current;
    Drop(sc[9]);
    sc = Sprite_Current;
    if (sc[9] != 0) return;
    sc[0xB] = 0;
    Bump(Sprite_Current[1]);
    for (unsigned i = 0; i < 8; ++i) SpawnRingChild(0xB, 0, i, static_cast<unsigned char>((i << 4) + 1));
}

// original 0x4D0700: once the second ring's last burst is done (+0xB 7):
// Battle_SetTargetFlag40(target), the effect-done bit, free.
S24_EXPORT void __cdecl Fx104_End(void) {
    if (Sprite_Current[0xB] != 7) return;
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D0730: kind-1 task parameter 0xB. +1 through Fx104_ChildPhases
// (0x65B868: Whirl, Burst), read in place, unchecked (ours aborts past it).
S24_EXPORT void __cdecl Fx104_Child(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Fx104_Child: +1 %u, past Fx104_ChildPhases' two", phase);
    DataPhase(tbl::kChildPhases104, phase)();
}

// original 0x4D0750: a whirl. +2 through Fx104_WhirlPhases (0x65B870, four,
// unchecked); then while the task is live and +2 is not 0: its matrix, the
// ring, the disc and the funnel, the matrix popped.
S24_EXPORT void __cdecl Fx104_Whirl(void) {
    const unsigned phase = Sprite_Current[2];
    if (phase >= 4) bof3::Fatal("Fx104_Whirl: +2 %u, past Fx104_WhirlPhases' four", phase);
    DataPhase(tbl::kWhirlPhases, phase)();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    magic_harness::Phase(bof3::addr::Fx104_PushMatrix)();
    magic_harness::Phase(bof3::addr::Fx104_DrawRing)();
    magic_harness::Phase(bof3::addr::Fx104_DrawDisc)();
    magic_harness::Phase(bof3::addr::Fx104_DrawFunnel)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D0790: +0xA down; at 0 Sound_PlayById(0x101 for an odd index
// +0xB, 0x100 for an even one), +9 and +0xA 0, +2 on.
S24_EXPORT void __cdecl Fx104_WhirlDelay(void) {
    unsigned char* sc = Sprite_Current;
    Drop(sc[0xA]);
    sc = Sprite_Current;
    if (sc[0xA] != 0) return;
    MH_CALL(Sound_PlayById)((sc[0xB] & 1) ? 0x101 : 0x100);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4D07E0: +9 up; at 8, +0xA 0 and +2 on.
S24_EXPORT void __cdecl Fx104_WhirlGrow(void) {
    Bump(Sprite_Current[9]);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 8) return;
    sc[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4D0810: +0xA and +9 up; at +9 16, +0xA 0 and +2 on.
S24_EXPORT void __cdecl Fx104_WhirlSpin(void) {
    Bump(Sprite_Current[0xA]);
    Bump(Sprite_Current[9]);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0x10) return;
    sc[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4D0850: +0xA and +9 up; at +9 24, the owner's +0xB = this
// whirl's index, free.
S24_EXPORT void __cdecl Fx104_WhirlEnd(void) {
    Bump(Sprite_Current[0xA]);
    Bump(Sprite_Current[9]);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0x18) return;
    Owner()[0xB] = sc[0xB];
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D0890: the whirl's funnel. Radius +9 * 30 (on +2 above 1,
// (+9 + 8) * 15) and rows from 0x18 (on +2 above 1, 0x18 - (+9 - 8) / 2,
// rounded toward 0) down to 17, each row 16 POLY_G4 round the point, the
// rows' elevation angle row * 0x80, the ring's step 0x100. Colour by +2: 1 a
// cool gradient by row and column (it steps at half way round), 2 and 3 a
// fade by +0xA, else the primitive's colour bytes left as they are.
S24_EXPORT void __cdecl Fx104_DrawFunnel(void) {
    const unsigned char* sc = Sprite_Current;
    Put16(kR, sc[9] * 30);
    if (sc[2] > 1) Put16(kR, (sc[9] + 8) * 15);
    int rows = 0x18;
    Put16(kW4, 0x18);
    if (sc[2] > 1) {
        rows = 0x18 - (static_cast<int>(sc[9]) - 8) / 2;
        Put16(kW4, rows);
    }
    rows = static_cast<short>(rows);
    if (rows <= 0x10) return;
    int count = rows;
    int top = rows << 7;
    do {
        const int lo = top - 0x80;
        int t = Cos(lo);
        t = Mul(t, G16(kR)) >> 12;
        t = Mul(t, Sin(0)) >> 12;
        Put16(kB, t);
        t = Cos(lo);
        t = Mul(t, G16(kR)) >> 12;
        t = Mul(t, Cos(0)) >> 12;
        Put16(kB + 2, t);
        t = Sin(lo);
        t = Mul(t, G16(kR)) >> 12;
        Put16(kA + 4, t);
        Put16(kB + 4, t);
        t = Cos(top);
        t = Mul(t, G16(kR)) >> 12;
        t = Mul(t, Sin(0)) >> 12;
        Put16(kD, t);
        t = Cos(top);
        t = Mul(t, G16(kR)) >> 12;
        t = Mul(t, Cos(0)) >> 12;
        Put16(kD + 2, t);
        t = Sin(top);
        t = Mul(t, G16(kR)) >> 12;
        Put16(kC + 4, t);
        Put16(kD + 4, t);
        int column = 1;
        for (int angle = 0x100; angle < 0x1100; angle += 0x100, ++column) {
            Put16(kA, G16(kB));
            Put16(kA + 2, G16(kB + 2));
            t = Cos(lo);
            t = Mul(t, G16(kR)) >> 12;
            t = Mul(t, Sin(angle)) >> 12;
            Put16(kB, t);
            t = Cos(lo);
            t = Mul(t, G16(kR)) >> 12;
            t = Mul(t, Cos(angle)) >> 12;
            const short d0 = G16(kD), d2 = G16(kD + 2);
            Put16(kB + 2, t);
            Put16(kC, d0);
            Put16(kC + 2, d2);
            t = Cos(top);
            t = Mul(t, G16(kR)) >> 12;
            t = Mul(t, Sin(angle)) >> 12;
            Put16(kD, t);
            t = Cos(top);
            t = Mul(t, G16(kR)) >> 12;
            t = Mul(t, Cos(angle)) >> 12;
            const short dx = G16(kD);
            const unsigned char* const s = Sprite_Current;
            unsigned char* const mode = Gfx_PacketNext;
            Put16(kD + 2, t);
            const std::int32_t x = Add(Shl(dx, 9), Long(s + 0x34));
            const std::int32_t z = Add(Shl(static_cast<short>(t), 9), Long(s + 0x38));
            MH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0xB5, 0);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 0, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Project4(p);
            const unsigned char* const s2 = Sprite_Current;
            switch (s2[2]) {
            case 1: {
                unsigned char c0, c1;
                const auto grey = static_cast<unsigned char>((count << 3) - 0x7F);
                if (angle < 0x800) {
                    c0 = static_cast<unsigned char>((column + 5) << 4);
                    c1 = static_cast<unsigned char>((column + 6) << 4);
                    p[4] = c0;
                    p[5] = grey;
                    p[6] = grey;
                    p[0x14] = c1;
                } else {
                    c0 = static_cast<unsigned char>(0x50 - (column << 4));
                    p[4] = c0;
                    p[5] = grey;
                    p[6] = grey;
                    c1 = static_cast<unsigned char>(0x60 - (column << 4));
                    p[0x14] = c1;
                }
                p[0x15] = grey;
                p[0x16] = grey;
                p[0x24] = c0;
                p[0x25] = grey;
                p[0x26] = grey;
                p[0x34] = c1;
                p[0x35] = grey;
                p[0x36] = grey;
                break;
            }
            case 2: {
                p[4] = static_cast<unsigned char>(0x51 - s2[0xA] * 10);
                p[5] = static_cast<unsigned char>(0x11 - (Sprite_Current[0xA] << 1));
                const unsigned char a = Sprite_Current[0xA];
                p[0x24] = 0x50;
                p[6] = static_cast<unsigned char>(0x11 - a);
                const unsigned char r = p[4], g = p[5], b = p[6];
                p[0x14] = r;
                p[0x15] = g;
                p[0x16] = b;
                p[0x25] = 0x10;
                p[0x26] = 0x10;
                p[0x34] = 0x50;
                p[0x35] = 0x10;
                p[0x36] = 0x10;
                break;
            }
            case 3: {
                p[4] = p[5] = p[6] = 1;
                p[0x14] = p[0x15] = p[0x16] = 1;
                p[0x24] = static_cast<unsigned char>(0x51 - Sprite_Current[0xA] * 10);
                p[0x25] = static_cast<unsigned char>(0x11 - (Sprite_Current[0xA] << 1));
                const auto b = static_cast<unsigned char>(0x11 - (Sprite_Current[0xA] << 1));
                const unsigned char r = p[0x24], g = p[0x25];
                p[0x26] = b;
                p[0x34] = r;
                p[0x35] = g;
                p[0x36] = b;
                break;
            }
            default:
                break;
            }
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 0, 0x44);
        }
        --count;
        top -= 0x80;
    } while (count > 0x10);
}

// original 0x4D0D30: Gte_PushMatrix, then the whirl's matrix: angles (0, 0,
// (Frame_Counter & 15) << 8 when +5 is 12, else 0), the point from +0x34 /
// +0x38 / +0x3E.
S24_EXPORT void __cdecl Fx104_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short angles[3] = {0, 0, 0};
    const unsigned char* const sc = Sprite_Current;
    if (sc[5] == 0xC) angles[2] = static_cast<short>((Frame_Counter & 0xF) << 8);
    short v[3];
    PointOf(v, Long(sc + 0x34), Long(sc + 0x38), S16(sc + 0x3E));
    PushMatrix(v, angles);
}

// original 0x4D0DF0: the whirl's disc. Radius cos(0x800) * +9 * 30 >> 12;
// 32 POLY_G3 fanned round the point (0, 0, 0) to the rim, each placed at the
// rim point >> 9 (sorted by LinkPrimAt(.., 2, ..)); colour (0x41 - 8 * +0xA)
// twice on +2 == 3, else 0x40 twice, then 1s. A closing draw mode 0x15 on
// ordering slot 5.
S24_EXPORT void __cdecl Fx104_DrawDisc(void) {
    int t = Cos(0x800);
    t = Mul(Mul(t, Sprite_Current[9]), 30) >> 12;
    Put16(kR, t);
    t = Sin(0);
    Put16(kC, Mul(t, G16(kR)) >> 12);
    t = Cos(0);
    t = Mul(t, G16(kR)) >> 12;
    Put16(kC + 2, t);
    short rim2 = static_cast<short>(t);
    for (int angle = 0x100; angle < 0x2100; angle += 0x100) {
        if (angle != 0x100) rim2 = G16(kC + 2);
        const short rim0 = G16(kC);
        Put16(kA, 0);
        Put16(kA + 2, 0);
        Put16(kA + 4, 0);
        Put16(kB, rim0);
        Put16(kB + 2, rim2);
        Put16(kB + 4, 0);
        t = Sin(angle);
        Put16(kC, Mul(t, G16(kR)) >> 12);
        t = Cos(angle);
        t = Mul(t, G16(kR));
        const short c0 = G16(kC);
        const unsigned char* const s = Sprite_Current;
        Put16(kC + 4, 0);
        t >>= 12;
        Put16(kC + 2, t);
        const std::int32_t x = Add(c0 >> 9, Long(s + 0x34));
        const std::int32_t z = Add(static_cast<short>(t) >> 9, Long(s + 0x38));
        DrawMode(0x35);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const unsigned char* const s2 = Sprite_Current;
        if (s2[2] == 3) {
            const int v = 0x41 - (s2[0xA] << 3);
            Put16(kW6, v);
            p[4] = static_cast<unsigned char>(v);
            p[5] = Mem(kW6)[0];
        } else {
            p[4] = 0x40;
            p[5] = 0x40;
        }
        p[6] = 1;
        p[0x14] = p[0x15] = p[0x16] = 1;
        p[0x24] = p[0x25] = p[0x26] = 1;
        Project3(p);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4D0FF0: the whirl's ring. Outer radius cos(0x800) * (+9 + 1) *
// 34 >> 12, inner cos(0x800) * +9 * 34 >> 12; 32 POLY_G4 between them on
// ordering slot 5, colour (0x31 - 2 * +9) on the outer edge above +9 == 8,
// else 0x20, the inner edge 1. Draw mode 0x35 before, 0x15 after.
S24_EXPORT void __cdecl Fx104_DrawRing(void) {
    int t = Cos(0x800);
    t = Mul(Mul(t, Sprite_Current[9] + 1), 34) >> 12;
    Put16(kR, t);
    t = Cos(0x800);
    t = Mul(Mul(t, Sprite_Current[9]), 34) >> 12;
    Put16(kR2, t);
    t = Sin(0);
    Put16(kB, Mul(t, G16(kR)) >> 12);
    t = Cos(0);
    Put16(kB + 2, Mul(t, G16(kR)) >> 12);
    t = Sin(0);
    Put16(kD, Mul(t, G16(kR2)) >> 12);
    t = Cos(0);
    Put16(kD + 2, Mul(t, G16(kR2)) >> 12);
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    for (int angle = 0x100; angle < 0x2100; angle += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const short b0 = G16(kB), b2 = G16(kB + 2);
        Put16(kA, b0);
        Put16(kA + 2, b2);
        Put16(kA + 4, 0);
        t = Sin(angle);
        Put16(kB, Mul(t, G16(kR)) >> 12);
        t = Cos(angle);
        t = Mul(t, G16(kR)) >> 12;
        const short d2 = G16(kD + 2);
        Put16(kB + 2, t);
        const short d0 = G16(kD);
        Put16(kB + 4, 0);
        Put16(kC, d0);
        Put16(kC + 2, d2);
        Put16(kC + 4, 0);
        t = Sin(angle);
        Put16(kD, Mul(t, G16(kR2)) >> 12);
        t = Cos(angle);
        t = Mul(t, G16(kR2));
        const unsigned char* const s = Sprite_Current;
        Put16(kD + 4, 0);
        Put16(kD + 2, t >> 12);
        const unsigned char size = s[9];
        if (size > 8) {
            const int v = 0x31 - (size << 1);
            Put16(kW6, v);
            p[4] = static_cast<unsigned char>(v);
            p[5] = Mem(kW6)[0];
            p[6] = Mem(kW6)[0];
            p[0x14] = Mem(kW6)[0];
            p[0x15] = Mem(kW6)[0];
            p[0x16] = Mem(kW6)[0];
        } else {
            p[4] = p[5] = p[6] = 0x20;
            p[0x14] = p[0x15] = p[0x16] = 0x20;
        }
        p[0x24] = p[0x25] = p[0x26] = 1;
        p[0x34] = p[0x35] = p[0x36] = 1;
        Project4(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4D12A0: a burst. A draw mode 0x35 on ordering slot 4 before and
// after; between, +2 through Fx104_BurstPhases (0x65B880, two, unchecked).
S24_EXPORT void __cdecl Fx104_Burst(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(4, 0xC);
    const unsigned phase = Sprite_Current[2];
    if (phase >= 2) bof3::Fatal("Fx104_Burst: +2 %u, past Fx104_BurstPhases' two", phase);
    DataPhase(tbl::kBurstPhases, phase)();
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(4, 0xC);
}

// original 0x4D12F0: +0xA down to 0; then the height +0x3E up 0x200, +9 8,
// +2 on.
S24_EXPORT void __cdecl Fx104_BurstDelay(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[0xA] != 0) {
        Drop(sc[0xA]);
        return;
    }
    SetWord(sc + 0x3E, Word(sc + 0x3E) + 0x200u);
    Sprite_Current[9] = 8;
    Bump(Sprite_Current[2]);
}

// original 0x4D1320: the burst's matrix and sphere, the matrix popped; the
// height down 0x40, +9 down; at 0 the owner's +0xB = the burst's index, free.
S24_EXPORT void __cdecl Fx104_BurstShrink(void) {
    magic_harness::Phase(bof3::addr::Fx104_PushMatrix)();
    magic_harness::Phase(bof3::addr::Fx104_DrawSphere)();
    MH_CALL(Gte_PopMatrix)();
    unsigned char* sc = Sprite_Current;
    SetWord(sc + 0x3E, Word(sc + 0x3E) - 0x40u);
    sc = Sprite_Current;
    Drop(sc[9]);
    sc = Sprite_Current;
    if (sc[9] != 0) return;
    Owner()[0xB] = sc[0xB];
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D1370: the burst's sphere. Radius +9 * 8; eight latitude bands
// (0x400 .. 0xB00, step 0x100) of eight POLY_G4 (longitude 0x200 .. 0x1000,
// step 0x200), draw mode 0xB5, colour +9 * 24 + 1 twice and +9 * 16 + 1.
S24_EXPORT void __cdecl Fx104_DrawSphere(void) {
    Put16(kR, Sprite_Current[9] << 3);
    int lat = 0x400;
    int next;
    do {
        int t = Cos(lat);
        t = Mul(t, G16(kR)) >> 12;
        t = Mul(t, Sin(0)) >> 12;
        Put16(kB, t);
        t = Cos(lat);
        t = Mul(t, G16(kR)) >> 12;
        t = Mul(t, Cos(0)) >> 12;
        Put16(kB + 2, t);
        t = Sin(lat);
        Put16(kB + 4, Mul(t, G16(kR)) >> 12);
        next = lat + 0x100;
        t = Cos(next);
        t = Mul(t, G16(kR)) >> 12;
        t = Mul(t, Sin(0)) >> 12;
        Put16(kD, t);
        t = Cos(next);
        t = Mul(t, G16(kR)) >> 12;
        t = Mul(t, Cos(0)) >> 12;
        Put16(kD + 2, t);
        t = Sin(next);
        Put16(kD + 4, Mul(t, G16(kR)) >> 12);
        for (int lon = 0x200; lon < 0x1200; lon += 0x200) {
            const short b0 = G16(kB), b2 = G16(kB + 2), b4 = G16(kB + 4);
            Put16(kA, b0);
            Put16(kA + 2, b2);
            Put16(kA + 4, b4);
            t = Cos(lat);
            t = Mul(t, G16(kR)) >> 12;
            t = Mul(t, Sin(lon)) >> 12;
            Put16(kB, t);
            t = Cos(lat);
            t = Mul(t, G16(kR)) >> 12;
            t = Mul(t, Cos(lon)) >> 12;
            Put16(kB + 2, t);
            t = Sin(lat);
            t = Mul(t, G16(kR)) >> 12;
            const short d0 = G16(kD), d4 = G16(kD + 4);
            Put16(kB + 4, t);
            const short d2 = G16(kD + 2);
            Put16(kC, d0);
            Put16(kC + 2, d2);
            Put16(kC + 4, d4);
            t = Cos(next);
            t = Mul(t, G16(kR)) >> 12;
            t = Mul(t, Sin(lon)) >> 12;
            Put16(kD, t);
            t = Cos(next);
            t = Mul(t, G16(kR)) >> 12;
            t = Mul(t, Cos(lon)) >> 12;
            Put16(kD + 2, t);
            t = Sin(next);
            t = Mul(t, G16(kR));
            const short dx = G16(kD), dz = G16(kD + 2);
            Put16(kD + 4, t >> 12);
            const unsigned char* const s = Sprite_Current;
            const std::int32_t x = Add(Shl(dx, 9), Long(s + 0x34));
            const std::int32_t z = Add(Shl(dz, 9), Long(s + 0x38));
            DrawMode(0xB5);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 0, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Project4(p);
            p[4] = static_cast<unsigned char>(Sprite_Current[9] * 0x18 + 1);
            p[5] = static_cast<unsigned char>(Sprite_Current[9] * 0x18 + 1);
            const unsigned char g = p[5];
            const auto b = static_cast<unsigned char>((Sprite_Current[9] << 4) + 1);
            p[0x15] = g;
            p[0x25] = g;
            p[6] = b;
            const unsigned char r = p[4], bl = p[6];
            p[0x14] = r;
            p[0x16] = bl;
            p[0x24] = r;
            p[0x26] = bl;
            p[0x34] = r;
            p[0x35] = g;
            p[0x36] = bl;
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 0, 0x44);
        }
        lat = next;
    } while (next < 0xC00);
}

// ===========================================================================
// MAGIC105 (row 64)

// original 0x4D16E0: the kind-2 task. +1 through a two-entry stack table -
// Start, BattleFx_Finish - unchecked; then every live mote of the pool
// (0x695C38, 64 of 0x84) run and drawn with Sprite_Current the mote and the
// owner its +0x80, both put back after.
S24_EXPORT void __cdecl Fx105_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Fx105_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Fx105_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const task = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = Long(m + 0x80);
        Sprite_Current = m;
        SetLong(Mem(at::kOwner), its);
        magic_harness::Phase(bof3::addr::Fx105_Mote)();
        magic_harness::Phase(bof3::addr::Fx105_MoteDraw)();
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = task;
    }
}

// original 0x4D1760: the pool emptied (bytes 0..2 of each), the targets'
// centre (0x4FC0E0), the owner's facing, an offset (-8.0, 0) turned by it
// (0x446770) and added to the field's kind-2 point, +0xC the angle from
// there back to that point (Math_Ratan2), +0xB 0; seven orbs (kind 1,
// parameter 0x33, +1 0, +4 the index), +0xB counting them; CLUT strip row 26
// made semi-transparent (cells 1..15 from the buffer 0x4000 below with bit
// 15, cell 0 cleared); Sound_PlayById(0x100); on.
S24_EXPORT void __cdecl Fx105_Start(void) {
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    CentreOnTargets();
    Sprite_Current[8] = Owner()[8];
    SetLong(Sprite_Current + 0xC, static_cast<std::int32_t>(0xFFF80000u));
    SetLong(Sprite_Current + 0x10, 0);
    TurnByFacing(Sprite_Current);
    unsigned char* sc = Sprite_Current;
    SetLong(sc + 0x34, Add(Long(sc + 0xC), Field_Kind2X));
    sc = Sprite_Current;
    SetLong(sc + 0x38, Add(Long(sc + 0x10), Field_Kind2Z));
    sc = Sprite_Current;
    const std::int32_t dz = Sub(Field_Kind2Z, Long(sc + 0x38));
    const std::int32_t dx = Sub(Field_Kind2X, Long(sc + 0x34));
    const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    SetLong(Sprite_Current + 0xC, angle);
    Sprite_Current[0xB] = 0;
    for (unsigned i = 0; i < 7; ++i) {
        unsigned char* const t = TaskSlot(MH_CALL(BattleTask_Create)(1, 0x33));
        unsigned char* const s = Sprite_Current;
        SetPtr(t + 0x80, s);
        t[1] = 0;
        t[4] = static_cast<unsigned char>(i);
        Bump(s[0xB]);
    }
    for (unsigned i = 0; i < 16; ++i)
        SetWord(Mem(cell::kClutRow + 2 * i), Word(Mem(cell::kClutSource + 2 * i)) | 0x8000u);
    SetWord(Mem(cell::kClutRow), 0);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Bump(Sprite_Current[1]);
}

// original 0x4D18C0: kind-1 task parameter 0x33 (an orb). +1 through
// Fx105_ChildPhases (0x65B888, one entry: Orb), unchecked.
S24_EXPORT void __cdecl Fx105_Child(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 1) bof3::Fatal("Fx105_Child: +1 %u, past Fx105_ChildPhases' one", phase);
    DataPhase(tbl::kChildPhases105, phase)();
}

// original 0x4D18E0: an orb. +2 through a four-entry stack table - OrbStart,
// OrbEmit, OrbHold, MagicFx_EndWithChildren - unchecked; then while live and
// +2 is not 3, its matrix and bands drawn, the matrix popped.
S24_EXPORT void __cdecl Fx105_Orb(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Fx105_OrbStart, bof3::addr::Fx105_OrbEmit,
                                                 bof3::addr::Fx105_OrbHold, bof3::addr::MagicFx_EndWithChildren};
    const unsigned phase = Sprite_Current[2];
    if (phase >= 4) bof3::Fatal("Fx105_Orb: +2 %u, past the four-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 3) return;
    magic_harness::Phase(bof3::addr::Fx105_PushMatrix)();
    magic_harness::Phase(bof3::addr::Fx105_DrawOrb)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D1940: the owner's point; two angles, +0xC and +0x10, each
// the owner's +0xC plus a word of Fx105_OrbAngles by the orb's index +4
// (0x65B88C + 2i and 0x65B88E + 2i, unbounded); +0xB, +9, +0xA 0; +2 on.
S24_EXPORT void __cdecl Fx105_OrbStart(void) {
    SetLong(Sprite_Current + 0x34, Long(Owner() + 0x34));
    SetLong(Sprite_Current + 0x38, Long(Owner() + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(Owner() + 0x3C));
    unsigned char* sc = Sprite_Current;
    SetLong(sc + 0xC, Add(G16(tbl::kOrbAngles + 2u * sc[4]), Long(Owner() + 0xC)));
    sc = Sprite_Current;
    SetLong(sc + 0x10, Add(G16(tbl::kOrbAngles + 2 + 2u * sc[4]), Long(Owner() + 0xC)));
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4D19E0: every fourth frame (by the orb's index), a mote from the
// pool (Fx105_MoteAlloc; 0xFF none): its owner the orb, +1 +2 0, +9 the orb's
// +9, +0x10 the orb's +0x10; the orb's +0xB counts it. On odd frames +9 up;
// at 32, +2 on.
S24_EXPORT void __cdecl Fx105_OrbEmit(void) {
    const unsigned char* sc = Sprite_Current;
    if (((sc[4] ^ static_cast<unsigned char>(Frame_Counter)) & 3) == 0) {
        const unsigned char k = MH_AT(AllocFn, bof3::addr::Fx105_MoteAlloc)();
        if (k != 0xFF) {
            unsigned char* const m = Mote(k);
            unsigned char* const s = Sprite_Current;
            SetPtr(m + 0x80, s);
            m[1] = 0;
            m[2] = 0;
            m[9] = s[9];
            SetLong(m + 0x10, Long(s + 0x10));
            Bump(s[0xB]);
        }
    }
    if ((Frame_Counter & 1) == 0) return;
    Bump(Sprite_Current[9]);
    unsigned char* const s = Sprite_Current;
    if (s[9] == 0x20) Bump(s[2]);
}

// original 0x4D1A80: +0xA up; at 64, +2 on.
S24_EXPORT void __cdecl Fx105_OrbHold(void) {
    Bump(Sprite_Current[0xA]);
    unsigned char* const sc = Sprite_Current;
    if (sc[0xA] == 0x40) Bump(sc[2]);
}

// original 0x4D1AA0 (reached from 15 overlays: one body the linker kept here):
// once the task's own children are gone (+0xB 0), the owner's +0xB down and
// the task freed.
S24_EXPORT void __cdecl MagicFx_EndWithChildren(void) {
    if (Sprite_Current[0xB] != 0) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D1AC0: Gte_PushMatrix, then the orb's matrix: angles 0, the
// point from +0x34 / +0x38 / +0x3E.
S24_EXPORT void __cdecl Fx105_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    const unsigned char* const sc = Sprite_Current;
    short v[3];
    PointOf(v, Long(sc + 0x34), Long(sc + 0x38), S16(sc + 0x3E));
    PushMatrix(v, angles);
}

// original 0x4D1B60: the orb. Six colour words from Fx105_OrbColours (by the
// index +4 * 3, unbounded), each c - (c >> 6) * +0xA; the first ring at
// radius 64 by the angles +0xC and +0x10; then +9 bands (radius 64 more each,
// height -(sin(step) * 3 >> 5), step 0x40 on), each a POLY_G4 on the
// ordering row of its first corner, draw mode 0x35.
S24_EXPORT void __cdecl Fx105_DrawOrb(void) {
    const unsigned char* const sc = Sprite_Current;
    const std::uint32_t colours = tbl::kOrbColours + 3u * sc[4];
    for (unsigned j = 0; j < 6; ++j) {
        const unsigned char c = Mem(colours + j)[0];
        Put16(kW4 + 2 * j, c - (c >> 6) * sc[0xA]);
    }
    int t = Sin(Long(sc + 0xC));
    Put16(kA, Shl(t, 6) >> 12);
    t = Cos(Long(Sprite_Current + 0xC));
    const unsigned char* s = Sprite_Current;
    Put16(kA + 2, Shl(t, 6) >> 12);
    Put16(kA + 4, 0);
    t = Sin(Long(s + 0x10));
    Put16(kB, Shl(t, 6) >> 12);
    t = Cos(Long(Sprite_Current + 0x10));
    Put16(kB + 2, Shl(t, 6) >> 12);
    Put16(kB + 4, 0);
    if (Sprite_Current[9] + 1 <= 1) return;
    int step = 0x40;
    for (int band = 1; band < Sprite_Current[9] + 1; ++band, step += 0x40) {
        t = Sin(step);
        const short a0 = G16(kA);
        const int dz = -(Shl(Mul(t, 3), 7) >> 12);
        Put16(kC, a0);
        const short b0 = G16(kB);
        Put16(kR2, dz);
        const short a2 = G16(kA + 2);
        Put16(kR, (band + 1) << 6);
        const short a4 = G16(kA + 4);
        Put16(kC + 2, a2);
        const short b2 = G16(kB + 2);
        Put16(kC + 4, a4);
        const short b4 = G16(kB + 4);
        Put16(kD, b0);
        s = Sprite_Current;
        Put16(kD + 2, b2);
        Put16(kD + 4, b4);
        t = Sin(Long(s + 0xC));
        Put16(kA, Mul(t, G16(kR)) >> 12);
        t = Cos(Long(Sprite_Current + 0xC));
        t = Mul(t, G16(kR)) >> 12;
        const short h = G16(kR2);
        Put16(kA + 2, t);
        Put16(kA + 4, h);
        t = Sin(Long(Sprite_Current + 0x10));
        Put16(kB, Mul(t, G16(kR)) >> 12);
        t = Cos(Long(Sprite_Current + 0x10));
        t = Mul(t, G16(kR));
        const short ax = G16(kA), az = G16(kA + 2);
        unsigned char* const mode = Gfx_PacketNext;
        Put16(kB + 2, t >> 12);
        Put16(kB + 4, G16(kR2));
        s = Sprite_Current;
        const std::int32_t x = Add(Shl(ax, 9), Long(s + 0x34));
        const std::int32_t z = Add(Shl(az, 9), Long(s + 0x38));
        MH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = Mem(kW4)[0];
        p[5] = Mem(kW6)[0];
        p[6] = Mem(kW8)[0];
        p[0x14] = Mem(kWA)[0];
        p[0x15] = Mem(kWC)[0];
        p[0x16] = Mem(kWE)[0];
        p[0x24] = Mem(kW4)[0];
        p[0x25] = Mem(kW6)[0];
        p[0x26] = Mem(kW8)[0];
        p[0x34] = Mem(kWA)[0];
        p[0x35] = Mem(kWC)[0];
        p[0x36] = Mem(kWE)[0];
        Project4(p);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0x44);
    }
}

// original 0x4D1F30: a mote (run with Sprite_Current the mote). +1 through
// Fx105_MotePhases (0x65B8B4, one entry: MoteRun), unchecked.
S24_EXPORT void __cdecl Fx105_Mote(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 1) bof3::Fatal("Fx105_Mote: +1 %u, past Fx105_MotePhases' one", phase);
    DataPhase(tbl::kMotePhases, phase)();
}

// original 0x4D1F50: with the sprite bank 0x9039D8 the effects' (0x8E3580),
// +2 through Fx105_MoteRunPhases (0x65B8B8, two, unchecked), then
// BattleActor_UpdateScreenXY; the bank back to 0x8B3580.
S24_EXPORT void __cdecl Fx105_MoteRun(void) {
    const unsigned phase = Sprite_Current[2];
    SetLong(Mem(cell::kSpriteBank), 0x8E3580);
    if (phase >= 2) bof3::Fatal("Fx105_MoteRun: +2 %u, past Fx105_MoteRunPhases' two", phase);
    DataPhase(tbl::kMoteRunPhases, phase)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetLong(Mem(cell::kSpriteBank), 0x8B3580);
}

// original 0x4D1F80: a mote's start. Height offset sin(+9 << 6) * 3 >> 4
// (scratch +2), radius (+9 + 1) << 6 (scratch +0); the point round the owner's
// by the angle +0x10 (sin / cos * radius >> 3), its height the owner's plus
// the offset less 0x20; the ground under it (AreaMap_Elevation) to +0x14; the
// sprite's fields (texture 0x1D, CLUT 0xA0, semi-transparent, tint 0xC0,
// ordering slot 3, ...); Sprite_SetAnimation(1 or 2 by Rand); +0xA a Rand in
// 0..63; +2 on.
S24_EXPORT void __cdecl Fx105_MoteStart(void) {
    int t = Sin(Sprite_Current[9] << 6);
    unsigned char* sc = Sprite_Current;
    Put16(kR2, Shl(Mul(t, 3), 7) >> 11);
    Put16(kR, (sc[9] + 1) << 6);
    t = Sin(Long(sc + 0x10));
    t = (Mul(t, G16(kR)) >> 3);
    SetLong(Sprite_Current + 0x34, Add(t, Long(Owner() + 0x34)));
    t = Cos(Long(Sprite_Current + 0x10));
    t = (Mul(t, G16(kR)) >> 3);
    SetLong(Sprite_Current + 0x38, Add(t, Long(Owner() + 0x38)));
    SetWord(Sprite_Current + 0x3E, Word(Owner() + 0x3E) + Word(Mem(kR2)) - 0x20u);
    sc = Sprite_Current;
    const long ground = MH_CALL(AreaMap_Elevation)(Long(sc + 0x34), Long(sc + 0x38));
    SetLong(Sprite_Current + 0x14, static_cast<short>(ground));
    Sprite_Current[0x25] = 0x1D;
    Sprite_Current[0x26] = 0;
    Sprite_Current[0x27] = 0xA0;
    Sprite_Current[0x28] = 0;
    Sprite_Current[0] |= 0x20;
    Sprite_Current[0x5D] = 0xC0;
    Sprite_Current[0x5E] = 0xC0;
    Sprite_Current[0x5F] = 0xC0;
    Sprite_Current[0x5C] = 1;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x29] = 3;
    Sprite_Current[0x24] = 4;
    SetWord(Sprite_Current + 0x2C, 0);
    Sprite_Current[0x2B] = 0;
    const int r = MH_CALL(Rand)();
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>((r & 1) + 1));
    const int r2 = MH_CALL(Rand)();
    Sprite_Current[0xA] = static_cast<unsigned char>(r2 & 0x3F);
    Bump(Sprite_Current[2]);
}

// original 0x4D2110: a mote drifting: +0xA up, the point moved by sin / cos
// of (+0xA & 63) << 8 (sign-extended from 29 bits), the height down 8, the
// script ticked once; when it ends or the height falls below the ground +0x14,
// the owner's +0xB down and the mote's bytes 0..2 cleared.
S24_EXPORT void __cdecl Fx105_MoteDrift(void) {
    Bump(Sprite_Current[0xA]);
    unsigned char* sc = Sprite_Current;
    unsigned char* const px = sc + 0x34;
    int t = Sin((sc[0xA] & 0x3F) << 8);
    SetLong(px, Add(Long(px), Shl(t, 3) >> 3));
    sc = Sprite_Current;
    unsigned char* const pz = sc + 0x38;
    t = Cos((sc[0xA] & 0x3F) << 8);
    SetLong(pz, Add(Long(pz), Shl(t, 3) >> 3));
    sc = Sprite_Current;
    SetWord(sc + 0x3E, Word(sc + 0x3E) - 8u);
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) {
        sc = Sprite_Current;
        if (S16(sc + 0x3E) >= Long(sc + 0x14)) return;
    }
    Drop(Owner()[0xB]);
    Sprite_Current[0] = 0;
    Sprite_Current[1] = 0;
    Sprite_Current[2] = 0;
}

// original 0x4D21C0: the first mote not in use (bit 0 of +0), marked; 0xFF
// when all 64 are.
S24_EXPORT unsigned char __cdecl Fx105_MoteAlloc(void) {
    for (unsigned k = 0; k < cell::kMotes; ++k) {
        unsigned char* const m = Mote(k);
        if (m[0] & 1) continue;
        m[0] |= 1;
        return static_cast<unsigned char>(k);
    }
    return 0xFF;
}

// original 0x4D2220: a mote's sprite. Unless hidden (+0 bit 6) or its screen
// point (+0x2E, +0x30) is outside (-0x40..0x180, -0x40..0x130): the frame at
// [+0x54] + u16 +0x5A - a count, then that many 5-byte parts (size index,
// x, y, u, v) - each a draw mode (texture page by +0x28 and +0x5C) and a
// SPRT on ordering slot +0x29, its size from Fx105_MoteSizes, CLUT by +0x27
// (+0x28 picks the row), tint +0x5D..+0x5F + 0x80, flipped by bit 7 when
// +0x2A, semi-transparent by +0 bit 5.
S24_EXPORT void __cdecl Fx105_MoteDraw(void) {
    const unsigned char* sc = Sprite_Current;
    if (sc[0] & 0x40) return;
    const short sx = S16(sc + 0x2E);
    if (sx > 0x180 || sx < -0x40) return;
    const short sy = S16(sc + 0x30);
    if (sy > 0x130 || sy < -0x40) return;
    const unsigned char* part = Mem(static_cast<std::uint32_t>(Long(sc + 0x54)) + Word(sc + 0x5A));
    const unsigned char count = part[0];
    ++part;
    unsigned clut;
    if (sc[0x28] != 0) {
        clut = ((sc[0x27] + 0x1E0u) << 6) & 0xFFFFu;
    } else {
        clut = ((((sc[0x27] >> 4) + 0x1F0u) << 6) | (sc[0x27] & 0xFu)) & 0xFFFFu;
    }
    for (unsigned n = count; n != 0; --n, part += 5) {
        sc = Sprite_Current;
        const unsigned tpage = ((((sc[0x28] != 0 ? 4u : 0u) | (sc[0x5C] & 3u)) << 5) & 0xFFFFu) | 0x1Du;
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
        MH_CALL(Gfx_CommitPrim)(Sprite_Current[0x29], 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetSprt)(p);
        SetWord(p + 0x18, Mem(tbl::kMoteSizes)[(part[0] & 0xF) * 2]);
        SetWord(p + 0x1A, Mem(tbl::kMoteSizes + 1)[(part[0] & 0xF) * 2]);
        p[0x14] = part[3];
        const unsigned char v = part[4];
        const unsigned char row = Sprite_Current[0x26];
        SetWord(p + 0x16, clut);
        p[0x15] = static_cast<unsigned char>(row + v);
        p[4] = static_cast<unsigned char>(Sprite_Current[0x5D] + 0x80);
        p[5] = static_cast<unsigned char>(Sprite_Current[0x5E] + 0x80);
        p[6] = static_cast<unsigned char>(Sprite_Current[0x5F] + 0x80);
        const unsigned char* const s = Sprite_Current;
        if ((part[0] & 0x80) && s[0x2A] != 0) {
            PutFloat(p + 8, S16(s + 0x2E) - static_cast<signed char>(part[1]) - static_cast<int>(Word(p + 0x18)));
        } else {
            PutFloat(p + 8, S16(s + 0x2E) + static_cast<signed char>(part[1]));
        }
        PutFloat(p + 0xC, S16(Sprite_Current + 0x30) + static_cast<signed char>(part[2]));
        MH_CALL(Gpu_SetSemiTrans)(p, (Sprite_Current[0] >> 5) & 1u);
        MH_CALL(Gfx_CommitPrim)(Sprite_Current[0x29], 0x1C);
    }
}

// ===========================================================================
// MAGIC106 (row 104)

// original 0x4D2450: the kind-2 task. +1 through a two-entry stack table -
// Start, BattleFx_Finish - unchecked; then every live spark of the pool
// (0x697D38, 96 of 0x20) run with 0x698938 the spark and the owner its +0x1C
// (the owner put back after each, 0x698938 left at the last).
S24_EXPORT void __cdecl Fx106_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Fx106_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Fx106_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < cell::kSparks; ++i) {
        unsigned char* const k = SparkRec(i);
        if ((k[0] & 1) == 0) continue;
        const std::int32_t its = Long(k + 0x1C);
        SetPtr(Mem(cell::kSparkCurrent), k);
        SetLong(Mem(at::kOwner), its);
        magic_harness::Phase(bof3::addr::Fx106_Spark)();
        SetLong(Mem(at::kOwner), owner);
    }
}

// original 0x4D24C0: the pool emptied (bytes 0..2 of each), the point the
// source sprite's (0x904B4C; its +0x3C plus 0x1000000), +0xB +9 0, on; 96
// sparks (Fx106_SparkAlloc, the index unchecked): owner the task, +1 0, +4
// the number, the delay +0xA (Rand & 15) + 16 * (number >> 5) + 1 and +8 its
// complement to 0x40; the task's +0xB counts them. Sound_PlayById(0x100) and
// (0x101).
S24_EXPORT void __cdecl Fx106_Start(void) {
    for (unsigned i = 0; i < cell::kSparks; ++i) {
        unsigned char* const k = SparkRec(i);
        k[0] = 0;
        k[1] = 0;
        k[2] = 0;
    }
    SetLong(Sprite_Current + 0x34, Long(Pointer(at::kSource) + 0x34));
    SetLong(Sprite_Current + 0x38, Long(Pointer(at::kSource) + 0x38));
    SetLong(Sprite_Current + 0x3C, Add(Long(Pointer(at::kSource) + 0x3C), 0x1000000));
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    Bump(Sprite_Current[1]);
    for (unsigned i = 0; i < cell::kSparks; ++i) {
        const unsigned char n = MH_AT(AllocFn, bof3::addr::Fx106_SparkAlloc)();
        unsigned char* const s = Sprite_Current;
        unsigned char* const k = SparkRec(n);
        SetPtr(k + 0x1C, s);
        k[1] = 0;
        k[4] = static_cast<unsigned char>(i);
        const int r = MH_CALL(Rand)();
        const auto delay = static_cast<unsigned char>((r & 0xF) + ((i >> 5) << 4) + 1);
        k[0xA] = delay;
        k[8] = static_cast<unsigned char>(0x40 - delay);
        Bump(Sprite_Current[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Sound_PlayById)(0x101);
}

// original 0x4D25C0: a spark. Its +1 through Fx106_SparkPhases (0x65B8E0, one
// entry: SparkRun), unchecked.
S24_EXPORT void __cdecl Fx106_Spark(void) {
    const unsigned phase = Spark()[1];
    if (phase >= 1) bof3::Fatal("Fx106_Spark: +1 %u, past Fx106_SparkPhases' one", phase);
    DataPhase(tbl::kSparkPhases, phase)();
}

// original 0x4D25E0: +2 through Fx106_SparkRunPhases (0x65B8E4, four,
// unchecked); then while live and +2 is not 0, its matrix and shards drawn,
// the matrix popped.
S24_EXPORT void __cdecl Fx106_SparkRun(void) {
    const unsigned phase = Spark()[2];
    if (phase >= 4) bof3::Fatal("Fx106_SparkRun: +2 %u, past Fx106_SparkRunPhases' four", phase);
    DataPhase(tbl::kSparkRunPhases, phase)();
    const unsigned char* const k = Spark();
    if (k[0] == 0 || k[2] == 0) return;
    magic_harness::Phase(bof3::addr::Fx106_PushMatrix)();
    magic_harness::Phase(bof3::addr::Fx106_DrawSpark)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D2620: +0xA down; at 0 the spark placed: radius ((+4 >> 5) + 6)
// << 4, angle (+4 & 31) << 7, round the owner's point (sin / cos * radius, no
// shift), its height the owner's +0x3C; colours +5..+7 each Rand & 7 + 5; +3
// +9 0; +0xA (Rand & 3) << 3; +0xB a Rand; +0xC the radius, +0xE the angle;
// +2 on.
S24_EXPORT void __cdecl Fx106_SparkDelay(void) {
    unsigned char* k = Spark();
    Drop(k[0xA]);
    k = Spark();
    if (k[0xA] != 0) return;
    Put16(kR, ((k[4] >> 5) + 6) << 4);
    const int angle = (k[4] & 0x1F) << 7;
    Put16(kW4, angle);
    int t = Sin(static_cast<short>(angle));
    SetLong(Spark() + 0x10, Add(Mul(t, G16(kR)), Long(Owner() + 0x34)));
    t = Cos(G16(kW4));
    SetLong(Spark() + 0x14, Add(Mul(t, G16(kR)), Long(Owner() + 0x38)));
    SetLong(Spark() + 0x18, Long(Owner() + 0x3C));
    SetWord(Spark() + 0xE, Word(Mem(kW4)));
    for (unsigned c = 5; c <= 7; ++c) {
        const int r = MH_CALL(Rand)();
        Spark()[c] = static_cast<unsigned char>((r & 7) + 5);
    }
    Spark()[3] = 0;
    Spark()[9] = 0;
    int r = MH_CALL(Rand)();
    Spark()[0xA] = static_cast<unsigned char>((r & 3) << 3);
    r = MH_CALL(Rand)();
    Spark()[0xB] = static_cast<unsigned char>(r);
    SetWord(Spark() + 0xC, Word(Mem(kR)));
    Bump(Spark()[2]);
}

namespace {
// The three moving phases' shared step: the spark's point round the owner's
// by the angle +0xE and the radius +0xC (sin / cos * radius, no shift).
void SparkPlace() {
    int t = Sin(S16(Spark() + 0xE));
    unsigned char* k = Spark();
    SetLong(k + 0x10, Add(Mul(t, S16(k + 0xC)), Long(Owner() + 0x34)));
    t = Cos(S16(Spark() + 0xE));
    k = Spark();
    SetLong(k + 0x14, Add(Mul(t, S16(k + 0xC)), Long(Owner() + 0x38)));
}
void SparkTurn() {
    unsigned char* const k = Spark();
    SetWord(k + 0xE, (Word(k + 0xE) + 0x40u) & 0xFFFu);
}
}  // namespace

// original 0x4D2770: closing in - the radius down 2, placed, +0xB up, +9 up 2
// to 16; at the radius (+4 >> 5) * 8 + 0x20 or less +2 on, and spark 0 sets
// the target's 0x10 flag (Battle_SetTargetFlags(target, 0x10)).
S24_EXPORT void __cdecl Fx106_SparkConverge(void) {
    unsigned char* k = Spark();
    SetWord(k + 0xC, Word(k + 0xC) - 2u);
    SparkPlace();
    Bump(Spark()[0xB]);
    k = Spark();
    if (k[9] < 0x10) {
        k[9] = static_cast<unsigned char>(k[9] + 2);
        k = Spark();
    }
    const int limit = (k[4] >> 5) * 8 + 0x20;
    if (S16(k + 0xC) > limit) return;
    Bump(k[2]);
    if (Spark()[4] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(Mem(at::kTarget)[0], 0x10);
}

// original 0x4D2840: spinning - the angle on 0x40, placed, +0xB up; +5 up to
// 12, +6 and +7 down to 4, +3 up to 7; +8 down, at 0 +2 on.
S24_EXPORT void __cdecl Fx106_SparkSpin(void) {
    SparkTurn();
    SparkPlace();
    Bump(Spark()[0xB]);
    unsigned char* k = Spark();
    if (k[5] < 0xC) {
        Bump(k[5]);
        k = Spark();
    }
    if (k[6] > 4) {
        Drop(k[6]);
        k = Spark();
    }
    if (k[7] > 4) {
        Drop(k[7]);
        k = Spark();
    }
    if (k[3] < 7) {
        Bump(k[3]);
        k = Spark();
    }
    Drop(k[8]);
    k = Spark();
    if (k[8] == 0) Bump(k[2]);
}

// original 0x4D2930: rising - the angle on 0x40, placed, the height +0x1A up
// by +0xA, +0xB up; on odd frames +9 down, at 0 the owner's +0xB down and
// the spark freed (Fx106_SparkFree).
S24_EXPORT void __cdecl Fx106_SparkRise(void) {
    SparkTurn();
    SparkPlace();
    unsigned char* k = Spark();
    SetWord(k + 0x1A, Word(k + 0x1A) + k[0xA]);
    Bump(Spark()[0xB]);
    if ((Frame_Counter & 1) == 0) return;
    Drop(Spark()[9]);
    if (Spark()[9] != 0) return;
    Drop(Owner()[0xB]);
    magic_harness::Phase(bof3::addr::Fx106_SparkFree)();
}

// original 0x4D29F0: Gte_PushMatrix, then the spark's matrix: angles (0, 0,
// (0x800 - +0xE + +3 * 0x80) & 0xFFF), the point from +0x10 / +0x14 / +0x1A.
S24_EXPORT void __cdecl Fx106_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const k = Spark();
    const short angles[3] = {0, 0, static_cast<short>((0x800u - Word(k + 0xE) + (k[3] << 7)) & 0xFFFu)};
    short v[3];
    PointOf(v, Long(k + 0x10), Long(k + 0x14), S16(k + 0x1A));
    PushMatrix(v, angles);
}

// original 0x4D2AB0: the spark's four shards. A draw mode 0x35 at its point
// (LinkPrimAt(.., 2, 0xC)); colours +5 / +6 / +7 times +9; for each shard j a
// POLY_G3 from the origin to two points at the radii and angles of
// Fx106_ShardTables / _ShardAngles (0x65B8F4.. four words each), the third
// corner's height and the second's by sin of ((+0xB + Fx106_ShardTilt[j]) &
// 15) << 8, on the spark's ordering row.
S24_EXPORT void __cdecl Fx106_DrawSpark(void) {
    DrawMode(0x35);
    const unsigned char* k = Spark();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(k + 0x10)), static_cast<unsigned long>(Long(k + 0x14)), 2,
                                0xC);
    k = Spark();
    Put16(kWA, k[5] * k[9]);
    Put16(kWC, k[6] * k[9]);
    Put16(kWE, k[7] * k[9]);
    Put16(kA + 4, 0);
    Put16(kA + 2, 0);
    Put16(kA, 0);
    for (unsigned j = 0; j < 4; ++j) {
        k = Spark();
        Put16(kR, G16(tbl::kShardTables + 2 * j));
        Put16(kR2, G16(tbl::kShardTables + 8 + 2 * j));
        Put16(kW4, G16(tbl::kShardTables + 0x10 + 2 * j));
        unsigned char* const p = Gfx_PacketNext;
        Put16(kW6, G16(tbl::kShardTables + 0x18 + 2 * j));
        const unsigned char tilt = Mem(tbl::kShardTables + 0x20 + j)[0];
        Put16(kW8, ((k[0xB] + tilt) & 0xF) << 8);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int t = Sin(G16(kW4));
        Put16(kB, Mul(t, G16(kR)) >> 12);
        t = Cos(G16(kW4));
        Put16(kB + 2, Mul(t, G16(kR)) >> 12);
        t = Sin(G16(kW6));
        Put16(kC, Mul(t, G16(kR2)) >> 12);
        t = Cos(G16(kW6));
        Put16(kC + 2, Mul(t, G16(kR2)) >> 12);
        t = Sin(G16(kW8));
        t = Shl(t, 5) >> 12;
        Put16(kB + 4, t);
        Put16(kC + 4, t);
        Project3(p);
        p[4] = Mem(kWE)[0];
        p[5] = Mem(kWC)[0];
        p[6] = Mem(kWA)[0];
        p[0x14] = Mem(kWA)[0];
        p[0x15] = Mem(kWC)[0];
        p[0x16] = Mem(kWE)[0];
        p[0x24] = Mem(kWA)[0];
        p[0x25] = Mem(kWC)[0];
        p[0x26] = Mem(kWE)[0];
        k = Spark();
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(k + 0x10)), static_cast<unsigned long>(Long(k + 0x14)),
                                    2, 0x34);
    }
}

// original 0x4D2D00: the first spark not in use (bit 0 of +0), marked; 0xFF
// when all 96 are.
S24_EXPORT unsigned char __cdecl Fx106_SparkAlloc(void) {
    for (unsigned n = 0; n < cell::kSparks; ++n) {
        unsigned char* const k = SparkRec(n);
        if (k[0] & 1) continue;
        k[0] |= 1;
        return static_cast<unsigned char>(n);
    }
    return 0xFF;
}

// original 0x4D2D50: the current spark's bytes 0..4 cleared (0x698938 read
// again for each).
S24_EXPORT void __cdecl Fx106_SparkFree(void) {
    Spark()[0] = 0;
    Spark()[1] = 0;
    Spark()[2] = 0;
    Spark()[3] = 0;
    Spark()[4] = 0;
}

void MagicS24_Inject() {
    if (bof3::WantsShadow("magic_s24")) magic_s24::SelfTest();
    BOF3_INJECT(Fx104_Task);
    BOF3_INJECT(Fx104_Start);
    BOF3_INJECT(Fx104_WaitFirstRing);
    BOF3_INJECT(Fx104_SecondRing);
    BOF3_INJECT(Fx104_End);
    BOF3_INJECT(Fx104_Child);
    BOF3_INJECT(Fx104_Whirl);
    BOF3_INJECT(Fx104_WhirlDelay);
    BOF3_INJECT(Fx104_WhirlGrow);
    BOF3_INJECT(Fx104_WhirlSpin);
    BOF3_INJECT(Fx104_WhirlEnd);
    BOF3_INJECT(Fx104_DrawFunnel);
    BOF3_INJECT(Fx104_PushMatrix);
    BOF3_INJECT(Fx104_DrawDisc);
    BOF3_INJECT(Fx104_DrawRing);
    BOF3_INJECT(Fx104_Burst);
    BOF3_INJECT(Fx104_BurstDelay);
    BOF3_INJECT(Fx104_BurstShrink);
    BOF3_INJECT(Fx104_DrawSphere);
    BOF3_INJECT(Fx105_Task);
    BOF3_INJECT(Fx105_Start);
    BOF3_INJECT(Fx105_Child);
    BOF3_INJECT(Fx105_Orb);
    BOF3_INJECT(Fx105_OrbStart);
    BOF3_INJECT(Fx105_OrbEmit);
    BOF3_INJECT(Fx105_OrbHold);
    BOF3_INJECT(MagicFx_EndWithChildren);
    BOF3_INJECT(Fx105_PushMatrix);
    BOF3_INJECT(Fx105_DrawOrb);
    BOF3_INJECT(Fx105_Mote);
    BOF3_INJECT(Fx105_MoteRun);
    BOF3_INJECT(Fx105_MoteStart);
    BOF3_INJECT(Fx105_MoteDrift);
    BOF3_INJECT(Fx105_MoteAlloc);
    BOF3_INJECT(Fx105_MoteDraw);
    BOF3_INJECT(Fx106_Task);
    BOF3_INJECT(Fx106_Start);
    BOF3_INJECT(Fx106_Spark);
    BOF3_INJECT(Fx106_SparkRun);
    BOF3_INJECT(Fx106_SparkDelay);
    BOF3_INJECT(Fx106_SparkConverge);
    BOF3_INJECT(Fx106_SparkSpin);
    BOF3_INJECT(Fx106_SparkRise);
    BOF3_INJECT(Fx106_PushMatrix);
    BOF3_INJECT(Fx106_DrawSpark);
    BOF3_INJECT(Fx106_SparkAlloc);
    BOF3_INJECT(Fx106_SparkFree);
}

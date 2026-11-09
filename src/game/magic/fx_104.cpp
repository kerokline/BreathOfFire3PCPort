// Moved here by tools/move_funcs.py from magic_s24.cpp (docs/source-layout-round.md).
#include "game/magic/fx_104.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s24.h"
#include "game/magic_s24_callees.h"
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

namespace cell = magic_s24::cell;

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The original's imul: 32 bits, wrapping.
inline int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

inline int Shl(int a, int n) { return static_cast<int>(static_cast<std::uint32_t>(a) << n); }

inline std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

inline short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

inline short G16(std::uint32_t address) { return static_cast<short>(Word(Mem(address))); }

inline void Put16(std::uint32_t address, int v) { SetWord(Mem(address), static_cast<unsigned>(v) & 0xFFFFu); }

inline std::uint32_t Addr(const void* p) { return Low32(p); }

inline void SetPtr(unsigned char* at_, const void* p) { SetLong(at_, static_cast<std::int32_t>(Addr(p))); }

inline void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

inline void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

inline unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

inline unsigned char* TaskSlot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (index & 0xFFu) * at::kTaskStride); }

// A .data table of handlers, read in place as the original reads it (during
// the fuzz its entries are the harness's recorders).
Handler DataPhase(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Callees.
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

using TurnFn = void (__cdecl*)(unsigned char*);

void CentreOnTargets() { MH_CALL(MagicFx_CenterOnSide)(); }

void TurnByFacing(unsigned char* task) { MH_AT(TurnFn, magic_s24::kTurnByFacing)(task); }

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The scratch words (SharedScratch): radii at +0 / +2, then colours or angles.
constexpr ::bof3::image::Va kR{cell::kScratch};

constexpr ::bof3::image::Va kR2{cell::kScratch + 2};

constexpr ::bof3::image::Va kW4{cell::kScratch + 4};

constexpr ::bof3::image::Va kW6{cell::kScratch + 6};

const short* Vec(std::uint32_t address) { return reinterpret_cast<const short*>(Mem(address)); }

float* Xy(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

// The four-vertex projection every POLY_G4 of the group makes: a b c d into
// the primitive's four points, then its depth.
void Project4(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers4)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x18), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), Xy(p, 0x38), &depth);
    MH_CALL(Gte_PrimDepths4_10B)(p);
}

void Project3(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers3)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), &depth);
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
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    const std::int32_t dx = Long(Mem(::bof3::Img(Fx104_ChildOffsets_addr) + 8 * index));
    unsigned char* const sc = Sprite_Current;
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    SetPtr(t + 0x80, sc);
    t[1] = static_cast<unsigned char>(phase);
    t[0xB] = static_cast<unsigned char>(index);
    t[9] = 0;
    t[0xA] = spin;
    tsc->step_x = dx;
    rv::Store(rv::Current<records::BattleTask>()->step_z, Long(Mem(::bof3::Img(Fx104_ChildOffsets_addr) + 8 * index + 4)));
    TurnByFacing(Sprite_Current);
    const unsigned char* const s = Sprite_Current;
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    tt->x = Add(ts->step_x, Field_Kind2X);
    tt->z = Add(ts->step_z, Field_Kind2Z);
    rv::Store(tt->height, ts->height);
    return t;
}

}  // namespace

#define S24_EXPORT extern "C" __attribute__((disable_tail_calls))

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
    const unsigned char target = Mem(::bof3::Img(at::kTarget))[0];
    const unsigned char actor = Mem(::bof3::Img(at::kActor))[0];
    unsigned char facing = Pointer(::bof3::Img(MagicS23_ActorSprite_addr))[8];
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
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D0730: kind-1 task parameter 0xB. +1 through Fx104_ChildPhases
// (0x65B868: Whirl, Burst), read in place, unchecked (ours aborts past it).
S24_EXPORT void __cdecl Fx104_Child(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Fx104_Child: +1 %u, past Fx104_ChildPhases' two", phase);
    DataPhase(::bof3::Img(Fx104_ChildPhases_addr), phase)();
}

// original 0x4D0750: a whirl. +2 through Fx104_WhirlPhases (0x65B870, four,
// unchecked); then while the task is live and +2 is not 0: its matrix, the
// ring, the disc and the funnel, the matrix popped.
S24_EXPORT void __cdecl Fx104_Whirl(void) {
    const unsigned phase = Sprite_Current[2];
    if (phase >= 4) bof3::Fatal("Fx104_Whirl: +2 %u, past Fx104_WhirlPhases' four", phase);
    DataPhase(::bof3::Img(Fx104_WhirlPhases_addr), phase)();
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
    Put16(::bof3::Img(kR.v), sc[9] * 30);
    if (sc[2] > 1) Put16(::bof3::Img(kR.v), (sc[9] + 8) * 15);
    int rows = 0x18;
    Put16(::bof3::Img(kW4.v), 0x18);
    if (sc[2] > 1) {
        rows = 0x18 - (static_cast<int>(sc[9]) - 8) / 2;
        Put16(::bof3::Img(kW4.v), rows);
    }
    rows = static_cast<short>(rows);
    if (rows <= 0x10) return;
    int count = rows;
    int top = rows << 7;
    do {
        const int lo = top - 0x80;
        int t = Cos(lo);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        t = Mul(t, Sin(0)) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, t);
        t = Cos(lo);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        t = Mul(t, Cos(0)) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, t);
        t = Sin(lo);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, t);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, t);
        t = Cos(top);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        t = Mul(t, Sin(0)) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, t);
        t = Cos(top);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        t = Mul(t, Cos(0)) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, t);
        t = Sin(top);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, t);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, t);
        int column = 1;
        for (int angle = 0x100; angle < 0x1100; angle += 0x100, ++column) {
            Put16(::bof3::Img(Prim_VertexScratch_addr), G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
            t = Cos(lo);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            t = Mul(t, Sin(angle)) >> 12;
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, t);
            t = Cos(lo);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            t = Mul(t, Cos(angle)) >> 12;
            const short d0 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18), d2 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, t);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, d0);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, d2);
            t = Cos(top);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            t = Mul(t, Sin(angle)) >> 12;
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, t);
            t = Cos(top);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            t = Mul(t, Cos(angle)) >> 12;
            const short dx = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18);
            const unsigned char* const s = Sprite_Current;
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            unsigned char* const mode = Gfx_PacketNext;
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, t);
            const std::int32_t x = Add(Shl(dx, 9), ts->x);
            const std::int32_t z = Add(Shl(static_cast<short>(t), 9), ts->z);
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
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    if (sc[5] == 0xC) angles[2] = static_cast<short>((Frame_Counter & 0xF) << 8);
    short v[3];
    PointOf(v, tsc->x, tsc->z, S16(sc + 0x3E));
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
    Put16(::bof3::Img(kR.v), t);
    t = Sin(0);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
    t = Cos(0);
    t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, t);
    short rim2 = static_cast<short>(t);
    for (int angle = 0x100; angle < 0x2100; angle += 0x100) {
        if (angle != 0x100) rim2 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2);
        const short rim0 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x10);
        Put16(::bof3::Img(Prim_VertexScratch_addr), 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, rim0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, rim2);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        t = Sin(angle);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
        t = Cos(angle);
        t = Mul(t, G16(::bof3::Img(kR.v)));
        const short c0 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x10);
        const unsigned char* const s = Sprite_Current;
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        t >>= 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, t);
        const std::int32_t x = Add(c0 >> 9, ts->x);
        const std::int32_t z = Add(static_cast<short>(t) >> 9, ts->z);
        DrawMode(0x35);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const unsigned char* const s2 = Sprite_Current;
        if (s2[2] == 3) {
            const int v = 0x41 - (s2[0xA] << 3);
            Put16(::bof3::Img(kW6.v), v);
            p[4] = static_cast<unsigned char>(v);
            p[5] = Mem(::bof3::Img(kW6.v))[0];
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
    Put16(::bof3::Img(kR.v), t);
    t = Cos(0x800);
    t = Mul(Mul(t, Sprite_Current[9]), 34) >> 12;
    Put16(::bof3::Img(kR2.v), t);
    t = Sin(0);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
    t = Cos(0);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
    t = Sin(0);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, Mul(t, G16(::bof3::Img(kR2.v))) >> 12);
    t = Cos(0);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, Mul(t, G16(::bof3::Img(kR2.v))) >> 12);
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    for (int angle = 0x100; angle < 0x2100; angle += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const short b0 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8), b2 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2);
        Put16(::bof3::Img(Prim_VertexScratch_addr), b0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, b2);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        t = Sin(angle);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
        t = Cos(angle);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        const short d2 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, t);
        const short d0 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, d0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, d2);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        t = Sin(angle);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, Mul(t, G16(::bof3::Img(kR2.v))) >> 12);
        t = Cos(angle);
        t = Mul(t, G16(::bof3::Img(kR2.v)));
        const unsigned char* const s = Sprite_Current;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, t >> 12);
        const unsigned char size = s[9];
        if (size > 8) {
            const int v = 0x31 - (size << 1);
            Put16(::bof3::Img(kW6.v), v);
            p[4] = static_cast<unsigned char>(v);
            p[5] = Mem(::bof3::Img(kW6.v))[0];
            p[6] = Mem(::bof3::Img(kW6.v))[0];
            p[0x14] = Mem(::bof3::Img(kW6.v))[0];
            p[0x15] = Mem(::bof3::Img(kW6.v))[0];
            p[0x16] = Mem(::bof3::Img(kW6.v))[0];
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
    DataPhase(::bof3::Img(Fx104_BurstPhases_addr), phase)();
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
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->height_whole, rv::As<records::BattleTask>(sc)->height_whole + 0x200u);
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
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->height_whole, rv::As<records::BattleTask>(sc)->height_whole - 0x40u);
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
    Put16(::bof3::Img(kR.v), Sprite_Current[9] << 3);
    int lat = 0x400;
    int next;
    do {
        int t = Cos(lat);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        t = Mul(t, Sin(0)) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, t);
        t = Cos(lat);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        t = Mul(t, Cos(0)) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, t);
        t = Sin(lat);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
        next = lat + 0x100;
        t = Cos(next);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        t = Mul(t, Sin(0)) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, t);
        t = Cos(next);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        t = Mul(t, Cos(0)) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, t);
        t = Sin(next);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
        for (int lon = 0x200; lon < 0x1200; lon += 0x200) {
            const short b0 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8), b2 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2), b4 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4);
            Put16(::bof3::Img(Prim_VertexScratch_addr), b0);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, b2);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, b4);
            t = Cos(lat);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            t = Mul(t, Sin(lon)) >> 12;
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, t);
            t = Cos(lat);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            t = Mul(t, Cos(lon)) >> 12;
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, t);
            t = Sin(lat);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            const short d0 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18), d4 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, t);
            const short d2 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, d0);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, d2);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, d4);
            t = Cos(next);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            t = Mul(t, Sin(lon)) >> 12;
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, t);
            t = Cos(next);
            t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
            t = Mul(t, Cos(lon)) >> 12;
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, t);
            t = Sin(next);
            t = Mul(t, G16(::bof3::Img(kR.v)));
            const short dx = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18), dz = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, t >> 12);
            const unsigned char* const s = Sprite_Current;
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            const std::int32_t x = Add(Shl(dx, 9), ts->x);
            const std::int32_t z = Add(Shl(dz, 9), ts->z);
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

void Fx104_Inject() {
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
}

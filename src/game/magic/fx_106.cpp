// Moved here by tools/move_funcs.py from magic_s24.cpp (docs/source-layout-round.md).
#include "game/magic/fx_106.h"

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

inline unsigned char* SparkRec(unsigned index) { return Mem(::bof3::Img(Fx106_SparkPool_addr) + (index & 0xFFu) * cell::kSparkStride); }

inline unsigned char* Spark() { return Pointer(::bof3::Img(Fx106_SparkCurrent_addr)); }

// A .data table of handlers, read in place as the original reads it (during
// the fuzz its entries are the harness's recorders).
Handler DataPhase(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Callees.
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

using AllocFn = unsigned char (__cdecl*)();

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The scratch words (SharedScratch): radii at +0 / +2, then colours or angles.
constexpr ::bof3::image::Va kR{cell::kScratch};

constexpr ::bof3::image::Va kR2{cell::kScratch + 2};

constexpr ::bof3::image::Va kW4{cell::kScratch + 4};

constexpr ::bof3::image::Va kW6{cell::kScratch + 6};

constexpr ::bof3::image::Va kW8{cell::kScratch + 8};

// kWC: now Scratch_Swap (symbols.toml).
constexpr ::bof3::image::Va kWA{cell::kScratch + 0xA};

constexpr ::bof3::image::Va kWE{cell::kScratch + 0xE};

const short* Vec(std::uint32_t address) { return reinterpret_cast<const short*>(Mem(address)); }

float* Xy(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

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

}  // namespace

#define S24_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The three moving phases' shared step: the spark's point round the owner's
// by the angle +0xE and the radius +0xC (sin / cos * radius, no shift).
void SparkPlace() {
    int t = Sin(S16(Spark() + 0xE));
    unsigned char* k = Spark();
    SetLong(k + 0x10, Add(Mul(t, S16(k + 0xC)), rv::As<records::SpriteHead>(Owner())->x));
    t = Cos(S16(Spark() + 0xE));
    k = Spark();
    SetLong(k + 0x14, Add(Mul(t, S16(k + 0xC)), rv::As<records::SpriteHead>(Owner())->z));
}

void SparkTurn() {
    unsigned char* const k = Spark();
    SetWord(k + 0xE, (Word(k + 0xE) + 0x40u) & 0xFFFu);
}

}  // namespace

// original 0x4D2450: the kind-2 task. +1 through a two-entry stack table -
// Start, BattleFx_Finish - unchecked; then every live spark of the pool
// (0x697D38, 96 of 0x20) run with 0x698938 the spark and the owner its +0x1C
// (the owner put back after each, 0x698938 left at the last).
S24_EXPORT void __cdecl Fx106_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Fx106_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Fx106_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < cell::kSparks; ++i) {
        unsigned char* const k = SparkRec(i);
        if ((k[0] & 1) == 0) continue;
        const std::int32_t its = Long(k + 0x1C);
        SetPtr(Mem(::bof3::Img(Fx106_SparkCurrent_addr)), k);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        magic_harness::Phase(bof3::addr::Fx106_Spark)();
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
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
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->z);
    rv::Store(rv::Current<records::BattleTask>()->height, Add(rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->height, 0x1000000));
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
    DataPhase(::bof3::Img(Fx106_SparkPhases_addr), phase)();
}

// original 0x4D25E0: +2 through Fx106_SparkRunPhases (0x65B8E4, four,
// unchecked); then while live and +2 is not 0, its matrix and shards drawn,
// the matrix popped.
S24_EXPORT void __cdecl Fx106_SparkRun(void) {
    const unsigned phase = Spark()[2];
    if (phase >= 4) bof3::Fatal("Fx106_SparkRun: +2 %u, past Fx106_SparkRunPhases' four", phase);
    DataPhase(::bof3::Img(Fx106_SparkRunPhases_addr), phase)();
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
    Put16(::bof3::Img(kR.v), ((k[4] >> 5) + 6) << 4);
    const int angle = (k[4] & 0x1F) << 7;
    Put16(::bof3::Img(kW4.v), angle);
    int t = Sin(static_cast<short>(angle));
    SetLong(Spark() + 0x10, Add(Mul(t, G16(::bof3::Img(kR.v))), rv::As<records::SpriteHead>(Owner())->x));
    t = Cos(G16(::bof3::Img(kW4.v)));
    SetLong(Spark() + 0x14, Add(Mul(t, G16(::bof3::Img(kR.v))), rv::As<records::SpriteHead>(Owner())->z));
    SetLong(Spark() + 0x18, rv::As<records::SpriteHead>(Owner())->height);
    SetWord(Spark() + 0xE, Word(Mem(::bof3::Img(kW4.v))));
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
    SetWord(Spark() + 0xC, Word(Mem(::bof3::Img(kR.v))));
    Bump(Spark()[2]);
}

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
    MH_CALL(Battle_SetTargetFlags)(Mem(::bof3::Img(at::kTarget))[0], 0x10);
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
    Put16(::bof3::Img(kWA.v), k[5] * k[9]);
    Put16(::bof3::Img(Scratch_Swap_addr), k[6] * k[9]);
    Put16(::bof3::Img(kWE.v), k[7] * k[9]);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
    Put16(::bof3::Img(Prim_VertexScratch_addr), 0);
    for (unsigned j = 0; j < 4; ++j) {
        k = Spark();
        Put16(::bof3::Img(kR.v), G16(::bof3::Img(Fx106_ShardTables_addr) + 2 * j));
        Put16(::bof3::Img(kR2.v), G16(::bof3::Img(Fx106_ShardTables_addr) + 8 + 2 * j));
        Put16(::bof3::Img(kW4.v), G16(::bof3::Img(Fx106_ShardTables_addr) + 0x10 + 2 * j));
        unsigned char* const p = Gfx_PacketNext;
        Put16(::bof3::Img(kW6.v), G16(::bof3::Img(Fx106_ShardTables_addr) + 0x18 + 2 * j));
        const unsigned char tilt = Mem(::bof3::Img(Fx106_ShardTables_addr) + 0x20 + j)[0];
        Put16(::bof3::Img(kW8.v), ((k[0xB] + tilt) & 0xF) << 8);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int t = Sin(G16(::bof3::Img(kW4.v)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
        t = Cos(G16(::bof3::Img(kW4.v)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
        t = Sin(G16(::bof3::Img(kW6.v)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul(t, G16(::bof3::Img(kR2.v))) >> 12);
        t = Cos(G16(::bof3::Img(kW6.v)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Mul(t, G16(::bof3::Img(kR2.v))) >> 12);
        t = Sin(G16(::bof3::Img(kW8.v)));
        t = Shl(t, 5) >> 12;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, t);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, t);
        Project3(p);
        p[4] = Mem(::bof3::Img(kWE.v))[0];
        p[5] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        p[6] = Mem(::bof3::Img(kWA.v))[0];
        p[0x14] = Mem(::bof3::Img(kWA.v))[0];
        p[0x15] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        p[0x16] = Mem(::bof3::Img(kWE.v))[0];
        p[0x24] = Mem(::bof3::Img(kWA.v))[0];
        p[0x25] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        p[0x26] = Mem(::bof3::Img(kWE.v))[0];
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

void Fx106_Inject() {
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

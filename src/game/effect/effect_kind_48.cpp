// Moved here by tools/move_funcs.py from effect_2d.cpp, effect_2e.cpp, rest_3e.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_48.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect/effect_kind_48_callees.h"
#include "game/effect_2d.h"
#include "game/effect_2d_callees.h"
#include "game/effect_2e.h"
#include "game/effect_2e_callees.h"
#include "game/effect_gte.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3e.h"
#include "game/rest_3e_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_kind_48::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

void StoreFloat(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

// The x87 at the game's 53-bit precision: a float plus or less a whole number
// rounded once to 53 bits, then to a float at the store.
void StoreSum(unsigned char* p, float a, int b) { StoreFloat(p, static_cast<float>(static_cast<double>(a) + static_cast<double>(b))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + which]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table, or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned which) {
    const unsigned state = Sprite_Current[which];
    if (state >= entries)
        bof3::Fatal("%s: byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2d.md section 2)",
                    who, which, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

unsigned char* SparkEffect2d(unsigned i) { return EffectKind30_Shards + i * effect_2d::at::kSparkStride; }

// Variant 2's glow record 0x92C060 stepped (0x4794D0) and drawn (0x4796B0).
void Glow() {
    SH_AT(void (__cdecl*)(unsigned char*), at::kGlowStep.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
    SH_AT(void (__cdecl*)(unsigned char*), at::kGlowDraw.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
}

std::int32_t SL(const unsigned char* p) { return Long(p); }

U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }

void AddWord(unsigned char* p, U d) { SetWord(p, Word(p) + d); }

void AddLong(unsigned char* p, U d) { SetUL(p, UL(p) + d); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

void Copy12(unsigned char* to, const void* from) { std::memcpy(to, from, 12); }

// The pointer a .data cell holds.
unsigned char* Cell(U cell) { return At(UL(At(cell))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + byte]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through
// the dword after - the next table or data.
void DispatchEffect2e(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned index = Sprite_Current[byte];
    if (index >= entries)
        bof3::Fatal("%s: byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2e.md section 2)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * index))))();
}

// +2 up, Sprite_Current read afresh.
void NextStep() {
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// The dword +0xC down one (Sprite_Current read afresh); true at 0 (the
// record as read then in *out).
bool CountDownC(unsigned char** out) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1u);
    s = S();
    *out = s;
    return rv::As<records::EffectWork>(s)->work_0c == 0;
}

// The byte +9 down one; true at 0.
bool CountDown9(unsigned char** out) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    *out = s;
    return s[9] == 0;
}

// Sprite_Current's +0x3C = the ground under its +0x34 / +0x38 (AreaMap_Elevation's
// low word, sign-extended, plus `lift`) << 16, the record read again after the call.
void Ground(U lift) {
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    const long g = SH_CALL(AreaMap_Elevation)(so->x, so->z);
    rv::Store<U>(rv::Current<records::EffectObject>()->height, (SW(reinterpret_cast<const unsigned char*>(&g)) + lift) << 16);
}

// Sprite_Current's +0x34 / +0x38 = the leader's x / z (each store to the record
// as read then).
void AtLeader() {
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(ObjTrio[0].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(ObjTrio[0].z));
}

// States 8 and 10's start: 0x6761CC = the spiral record; its point = the
// record's +0x34, +0x38 and +0x3C + 0x2000000; its turn word `turn`; set up
// (0x4799C0); +0xC = 0x28, +2 up.
void SpiralStart(unsigned turn) {
    unsigned char* s = S();
    Effect2e_SpiralPtr = ::bof3::Img(EffectKind30_Shards_addr) + 0x524;
    SetUL(At(::bof3::Img(EffectKind30_Shards_addr) + 0x524), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x));
    SetUL(At(::bof3::Img(EffectKind30_Shards_addr) + 0x524 + 4), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    const U height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 0x2000000u;
    Effect2e_SpiralTurn = turn;
    SetUL(At(::bof3::Img(EffectKind30_Shards_addr) + 0x524 + 8), height);
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralInit.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0x524));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, 0x28);
    NextStep();
}

// States 8 and 10's frame: the spiral (0x6761CC) stepped and drawn (0x479B70);
// while +0xC is above 8 (signed) its turn word +0xD10 up `turn`; +0xC down, at 0
// a tail jmp to Effect_Release.
void SpiralTurn(unsigned turn) {
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralDraw.v)(Cell(::bof3::Img(at::kSpiralPtr.v)));
    const unsigned char* s = S();
    if (SL(s + 0xC) > 8) AddWord(Cell(::bof3::Img(at::kSpiralPtr.v)) + 0xD10, turn);
    unsigned char* t;
    if (!CountDownC(&t)) return;
    SH_CALL(Effect_Release)();
}

// State 12's frame: the burst record stepped (0x4794D0) and drawn (0x4796B0).
void BurstFrame() {
    SH_AT(void (__cdecl*)(unsigned char*), at::kBurstStep.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
    SH_AT(void (__cdecl*)(unsigned char*), at::kBurstDraw.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
}

// A draw mode: Gpu_GetTPage(0, 1, x, y) into Gpu_SetDrawMode(packet, 0, dtd,
// it, 0), committed 0xC.
void DrawMode(int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
}

// A sphere vertex number, checked against the 0x1E2 (the original reads past
// the projected points and the shades into the next arrays).
unsigned SphereVertex(U n) {
    if (n >= at::kSphereVertCount)
        bof3::Fatal("EffectSphere_Draw: a quad names vertex %u, past the sphere's %u - the original reads past its points "
                    "(docs/effect_2e.md section 2)",
                    (unsigned)n, at::kSphereVertCount);
    return n;
}

using LD = long double;

U ULRest3e(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3e(U a) { return ULRest3e(move_script::At(a)); }

// imul r32, r32: the product's low 32 bits.
U Mul(U a, U b) { return a * b; }

// (cos or sin answered first) * the width word +0xE, zero-extended, sar 12:
// the width read after the call, as the original reads it.
U Turn(int answer, const unsigned char* point) { return Sar(Mul(static_cast<U>(answer), Word(point + 0xE)), 12); }

void Copy12Rest3e(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, ULRest3e(from + i));
}

// --- x87 as the original has it (effect_3a.cpp's form): `fld dword` through
// inline assembly, every operation in long double at the game's control word,
// `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}

LD I(std::int32_t v) { return static_cast<LD>(v); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// fcomp; fnstsw; test ah, 0x40: C3 set - equal, or unordered.
bool EqualOrUnordered(const unsigned char* a, const unsigned char* b) {
    const LD x = F(a), y = F(b);
    return !(x < y) && !(x > y);
}

const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawModeRest3e(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

}  // namespace

unsigned char* Spark(unsigned i) { return EffectKind30_Shards + i * rest_3e::at::kSparkStride; }

namespace {

constexpr U kRing = 0x180;

// Ring 0's point i: angle i * 0x80; with the tilt t: a = sin t cos i sar 12,
// b = sin t sin i sar 12, c = cos t; the shade from the light (a * l0 + b * l1
// + c * l2, summed as the original: l1 b + c l2, then + a l0): at most 0 the
// `dark` value, else ((sqrt, at most 0xFFF) sar 5 & 0x78) + 7; the point (a <<
// 5 + x, b << 5 + z, c << 13 + y) projected.
void SpiralPoint(unsigned char* spiral, unsigned i, unsigned char dark, unsigned char* shade, unsigned char* screen) {
    const int angle = static_cast<int>((i * 0x80u) & 0xFFFFu);
    U a = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(Word(spiral + 0xD10))));
    a = Sar(Mul(a, static_cast<U>(SH_CALL(Math_Cos)(angle))), 12);
    U b = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(Word(spiral + 0xD10))));
    b = Sar(Mul(b, static_cast<U>(SH_CALL(Math_Sin)(angle))), 12);
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(Word(spiral + 0xD10))));
    const U dot = Mul(ULRest3e(spiral + 0xD18), b) + Mul(c, ULRest3e(spiral + 0xD1C)) + Mul(a, ULRest3e(spiral + 0xD14));
    if (static_cast<std::int32_t>(dot) <= 0) {
        *shade = dark;
    } else {
        std::int32_t root = static_cast<std::int32_t>(SH_AT(long (__cdecl*)(long), rest_3e::at::kSqrt.v)(static_cast<long>(dot)));
        if (root > 0xFFF) root = 0xFFF;
        *shade = static_cast<unsigned char>(((static_cast<U>(root) >> 5) & 0x78u) + 7u);
    }
    alignas(4) unsigned char pt[12];
    SetUL(pt, (a << 5) + ULRest3e(spiral));
    SetUL(pt + 4, (b << 5) + ULRest3e(spiral + 4));
    SetUL(pt + 8, (c << 13) + ULRest3e(spiral + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(screen));
}

// The ring's rim at angle t: x + cos t << 5, z + sin t << 5, the height (each
// read after its call).
void RimPoint(const unsigned char* ring, int t, unsigned char* pt) {
    const U cos = static_cast<U>(SH_CALL(Math_Cos)(t));
    SetUL(pt, (cos << 5) + ULRest3e(ring));
    const U sin = static_cast<U>(SH_CALL(Math_Sin)(t));
    SetUL(pt + 4, (sin << 5) + ULRest3e(ring + 4));
    SetUL(pt + 8, ULRest3e(ring + 8));
}

}  // namespace

unsigned char* Dust(unsigned i) { return At(::bof3::Img(rest_3e::at::kDust.v) + i * at::kDustStride); }

// original 0x4781B0 (0x12 bytes): jmp [EffectKind48_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind48_Run(void) { Dispatch("EffectKind48_Run", AddressOf(EffectKind48_States), EffectKind48_States_count, 1); }

// original 0x4781D0 (state 0): another live kind-0x48 record in the pool
// (Sprite_Current read once): this one released. Else +6 (the rings thrown) 0,
// +9 0, the sparks cleared, Sound_PlayEffect(0x203), +1 up.
extern "C" void __cdecl EffectKind48_Start(void) {
    unsigned char* const self = S();
    for (unsigned i = 0; i < at::kEffects; ++i) {
        const unsigned char* const e = rv::Bytes(&Effect_Objects[i]);
        if (e[0] != 0 && e[5] == 0x48 && e != self) {
            SH_CALL(Effect_Release)();
            return;
        }
    }
    self[6] = 0;
    S()[9] = 0;
    SH_CALL(EffectKind48_ClearSparks)();
    SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x478230 (state 1): a ring spawned when +9 is 0; +9 up, at 4 back to
// 0 and +6 up; the sparks stepped; +6 at +0xB: +1 up.
extern "C" void __cdecl EffectKind48_Burst(void) {
    if (S()[9] == 0) SH_CALL(EffectKind48_SpawnRing)();
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (s[9] == 4) {
        s[9] = 0;
        s[6] = static_cast<unsigned char>(s[6] + 1);
    }
    SH_CALL(EffectKind48_StepSparks)();
    s = S();
    if (s[6] == s[0xB]) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x478280 (state 2): the sparks stepped; none live: Effect_Release (a
// tail jump).
extern "C" void __cdecl EffectKind48_Fade(void) {
    if (SH_CALL(EffectKind48_StepSparks)() == 0) SH_CALL(Effect_Release)();
}

// original 0x478290: four sparks at the angles 0x80, 0x180, 0x280, 0x380, each
// the first free (none: skipped): +0x15 = 1 (live), +0x16 = 8 (its life), x =
// Math_Cos(angle) << 4 + Sprite_Current's x, z = Math_Sin(angle) << 4 + its z
// (Sprite_Current read after each call), the height its height, +0x10 = 0x1800
// (the size), +0x14 = 0xC0 (the shade).
extern "C" void __cdecl EffectKind48_SpawnRing(void) {
    U angle = 0x80;
    for (unsigned k = 0; k < 4; ++k, angle += 0x100) {
        unsigned char* const spark = SH_CALL(EffectKind48_FindFreeSpark)();
        if (spark == nullptr) continue;
        spark[0x15] = 1;
        const U a = angle & 0xFFFFu;
        spark[0x16] = 8;
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        SetUL(spark, (c << 4) + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        SetUL(spark + 4, (sn << 4) + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
        const U height = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height);
        SetUL(spark + 0x10, 0x1800);
        SetUL(spark + 8, height);
        spark[0x14] = 0xC0;
    }
}

// original 0x478320: the first of the 16 sparks of 0x18 at EffectKind30_Shards
// whose +0x15 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind48_FindFreeSpark(void) {
    for (unsigned i = 0; i < effect_2d::at::kSparks; ++i)
        if (SparkEffect2d(i)[0x15] == 0) return SparkEffect2d(i);
    return nullptr;
}

// original 0x478340: +0x15 of the 16 sparks 0.
extern "C" void __cdecl EffectKind48_ClearSparks(void) {
    for (unsigned i = 0; i < effect_2d::at::kSparks; ++i) SparkEffect2d(i)[0x15] = 0;
}

// original 0x478360: the map camera loaded (EffectGte_LoadMapCamera); each live
// spark (+0x15) drawn, then its height +0x3000, its shade - 0x18, its life down
// and at 0 +0x15 = 0. Answers 1 in al when any spark was live, else 0.
extern "C" unsigned char __cdecl EffectKind48_StepSparks(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < effect_2d::at::kSparks; ++i) {
        unsigned char* const spark = SparkEffect2d(i);
        if (spark[0x15] == 0) continue;
        SH_CALL(EffectKind48_DrawSpark)(spark);
        const U height = UL(spark + 8) + 0x3000u;
        const unsigned char shade = static_cast<unsigned char>(spark[0x14] + 0xE8);
        const unsigned char life = static_cast<unsigned char>(spark[0x16] - 1);
        SetUL(spark + 8, height);
        spark[0x14] = shade;
        spark[0x16] = life;
        if (life == 0) spark[0x15] = 0;
        any = 1;
    }
    return any;
}

// original 0x4783C0: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), committed
// Gfx_CommitPrim(1, 0xC)); a POLY_FT4 at the new Gfx_PacketNext,
// semi-transparent. The spark's point (x, z, height << 8) projected
// (EffectGte_ProjectPoint: screen x, y, depth) and its size (+0x10 >> 6, the
// low word) scaled at that depth (EffectGte_ProjectSize: h); the corners x - h
// .. x + h, y - h .. y + h (the x87 at 53 bits, rounded once to floats), the
// depth at each; u 0xE0 / 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0,
// 0x1E3) at +0x16, the page again at +0x26; the colour the shade +0x14;
// Gfx_CommitPrim(1, 0x48). The original hands EffectGte_ProjectSize a size
// whose second word is its uninitialised stack and never reads that quotient;
// ours passes 0 there.
extern "C" void __cdecl EffectKind48_DrawSpark(const unsigned char* spark) {
    const unsigned page = SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    short size[2] = {static_cast<short>(UL(spark + 0x10) >> 6), 0};
    const long point[3] = {Long(spark), Long(spark + 4), static_cast<long>(UL(spark + 8) << 8)};
    short scaled[2] = {0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, size, scaled);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    const int h = scaled[0];
    StoreSum(prim + 0x28, screen[0], -h);
    StoreSum(prim + 0x08, screen[0], -h);
    StoreSum(prim + 0x38, screen[0], h);
    StoreSum(prim + 0x18, screen[0], h);
    StoreSum(prim + 0x1C, screen[1], -h);
    StoreSum(prim + 0x0C, screen[1], -h);
    StoreSum(prim + 0x3C, screen[1], h);
    StoreSum(prim + 0x2C, screen[1], h);
    std::memcpy(prim + 0x40, &screen[2], 4);
    std::memcpy(prim + 0x30, &screen[2], 4);
    std::memcpy(prim + 0x20, &screen[2], 4);
    std::memcpy(prim + 0x10, &screen[2], 4);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    prim[4] = spark[0x14];
    prim[5] = spark[0x14];
    prim[6] = spark[0x14];
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x478550 (0x12 bytes): jmp [EffectKind49_Variants + +1 * 4],
// unbounded. +1 is the variant Scena07_TakeEffect49 stored; entries 5..9 are
// group E2E's.
extern "C" void __cdecl EffectKind49_Run(void) {
    Dispatch("EffectKind49_Run", AddressOf(EffectKind49_Variants), EffectKind49_Variants_count, 1);
}

// original 0x478570: variant 0, jmp [EffectKind49_V0States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V0Run(void) {
    Dispatch("EffectKind49_V0Run", AddressOf(EffectKind49_V0States), EffectKind49_V0States_count, 2);
}

// original 0x478590 (variant 0, state 0): the puffs cleared (0x4790C0); the
// dword +0xC = 0x40; +2 up.
extern "C" void __cdecl EffectKind49_V0Start(void) {
    SH_AT(void (__cdecl*)(), at::kPuffsClear.v)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x40);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4785B0 (variant 0, state 1): on a count +0xC with its low three
// bits 0, a free puff (0x47CF20) started (0x4790F0); the puffs stepped
// (0x479260); +0xC down, at 0 +2 up.
extern "C" void __cdecl EffectKind49_V0Emit(void) {
    if ((S()[0xC] & 7) == 0) {
        unsigned char* const puff = SH_AT(unsigned char* (__cdecl*)(), at::kPuffFindFree.v)();
        if (puff != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kPuffStart.v)(puff);
    }
    SH_AT(unsigned char (__cdecl*)(), at::kPuffsStep.v)();
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1u);
    s = S();
    if (rv::As<records::EffectWork>(s)->work_0c == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4785F0 (variant 0, state 2): the puffs stepped; none live:
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V0End(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kPuffsStep.v)() == 0) SH_CALL(Effect_Release)();
}

// original 0x478600: variant 1, jmp [EffectKind49_V1States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V1Run(void) {
    Dispatch("EffectKind49_V1Run", AddressOf(EffectKind49_V1States), EffectKind49_V1States_count, 2);
}

// original 0x478620 (variant 1, state 0): the dword +0x10 = -8; +2 up.
extern "C" void __cdecl EffectKind49_V1Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0xFFFFFFF8u);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x478640 (variant 1, state 1): Area85_ClutShift(+0x10); +0x10 up 1 on
// a frame a multiple of 16; at 0 Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V1Fade(void) {
    SH_CALL(Area85_ClutShift)(static_cast<std::int32_t>(rv::As<records::EffectWork>(S())->work_10));
    const U up = (Frame_Counter & 0xFu) == 0 ? 1u : 0u;
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, rv::As<records::EffectWork>(s)->work_10 + up);
    s = S();
    if (rv::As<records::EffectWork>(s)->work_10 == 0) SH_CALL(Effect_Release)();
}

// original 0x478680: variant 2, jmp [EffectKind49_V2States + +2 * 4],
// unbounded; entries 5 and 6 are WeretigerFx_Next (+2 on), 7 Effect_StateRelease.
extern "C" void __cdecl EffectKind49_V2Run(void) {
    Dispatch("EffectKind49_V2Run", AddressOf(EffectKind49_V2States), EffectKind49_V2States_count, 2);
}

// original 0x4786A0 (variant 2, state 0): the glow's word 0x92C49E = 0xC0; the
// dword +0x14 (a speed) 0, then 16 times: +0x14 up 0x800000, x +0x34 up
// 0x10000, the height +0x3C up +0x14 - the glow lifted along its arc at once;
// Sound_PlayEffect(0x202); +0x14 negated; +2 up.
extern "C" void __cdecl EffectKind49_V2Launch(void) {
    SetWord(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e), 0xC0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0);
    for (unsigned i = 0; i < 16; ++i) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y) + 0x800000u);
        rv::Current<records::EffectObject>()->x = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x) + 0x10000u;
        rv::Current<records::EffectObject>()->height = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y);
    }
    SH_CALL(Sound_PlayEffect)(0x202);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0u - static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y));
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x478720 (variant 2, state 1): the glow; x +0x34 down 0x10000, the
// height +0x3C up the speed +0x14, a speed not 0 up 0x800000; at the target
// (+0x34 = +0xC and +0x38 = +0x10) +9 = 0x2F and +2 up.
extern "C" void __cdecl EffectKind49_V2Fly(void) {
    Glow();
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    so->x = static_cast<std::uint32_t>(so->x) + 0xFFFF0000u;
    so->height = static_cast<std::uint32_t>(so->height) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y);
    const U speed = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y);
    if (speed != 0) rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, speed + 0x800000u);
    if (static_cast<std::uint32_t>(so->x) == rv::As<records::EffectWork>(s)->work_0c && static_cast<std::uint32_t>(so->z) == rv::As<records::EffectWork>(s)->work_10) {
        s[9] = 0x2F;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
}

// original 0x4787A0 (variant 2, state 2): the glow; +9 down; at 0
// Sound_PlayEffect(0x203), +9 = 0x20, +2 up.
extern "C" void __cdecl EffectKind49_V2Wait(void) {
    Glow();
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    SH_CALL(Sound_PlayEffect)(0x203);
    S()[9] = 0x20;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4787F0 (variant 2, state 3): the glow; its word 0x92C49E up 6; +9
// down; at 0 +9 = 0x20, +2 up.
extern "C" void __cdecl EffectKind49_V2Rise(void) {
    Glow();
    unsigned char* const s = S();
    SetWord(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e), Word(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e)) + 6u);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    s[9] = 0x20;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478840 (variant 2, state 4): the glow; its word 0x92C49E down 0xC;
// +9 down; at 0 +2 up.
extern "C" void __cdecl EffectKind49_V2Sink(void) {
    Glow();
    unsigned char* const s = S();
    SetWord(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e), Word(At(::bof3::Img(EffectKind30_Shards_addr) + 0x51e)) + 0xFFF4u);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478880: variant 3, jmp [EffectKind49_V3States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V3Run(void) {
    Dispatch("EffectKind49_V3Run", AddressOf(EffectKind49_V3States), EffectKind49_V3States_count, 2);
}

// original 0x4788A0 (variant 3, state 0): the height +0x3C = the ground's
// elevation at (+0x34, +0x38) (AreaMap_Elevation, its low word signed) << 16;
// the dust cleared (0x47A110); +9 = 0, +0xC = 0x5F, +2 up;
// Sound_PlayEffect(0x205).
extern "C" void __cdecl EffectKind49_V3Start(void) {
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    const long ground = SH_CALL(AreaMap_Elevation)(so->x, so->z);
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<U>(static_cast<std::int32_t>(static_cast<short>(ground))) << 16);
    SH_AT(void (__cdecl*)(), at::kDustClear.v)();
    S()[9] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x5F);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(Sound_PlayEffect)(0x205);
}

// original 0x478900 (variant 3, state 1): with +9 the step: when the low byte of
// the count +0xC has none of the step's mask bits (0x6545EC[+9]), the word +0x10
// of what 0x6761D0 points at up the step's count (0x6545F4[+9]) and that many
// free dust records (0x47A130) started (0x47A150) - the count re-read by +9
// after each. Then a count with its low nibble 0 moves +9 on; the dust stepped
// (0x47A200); +0xC down, at 0 +2 up. The byte tables are read in place.
extern "C" void __cdecl EffectKind49_V3Burst(void) {
    unsigned char* s = S();
    const unsigned step = s[9];
    if ((s[0xC] & At(::bof3::Img(at::kBurstMask.v))[step]) == 0) {
        const unsigned char add = At(::bof3::Img(at::kBurstCount.v))[step];
        unsigned char* const anchor = At(Effect2d_DustAnchor);
        SetWord(anchor + 0x10, Word(anchor + 0x10) + add);
        s = S();
        if (At(::bof3::Img(at::kBurstCount.v))[s[9]] != 0) {
            unsigned char n = 0;
            do {
                unsigned char* const dust = SH_AT(unsigned char* (__cdecl*)(), at::kDustFindFree.v)();
                if (dust != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kDustStart.v)(dust);
                s = S();
                n = static_cast<unsigned char>(n + 1);
            } while (n < At(::bof3::Img(at::kBurstCount.v))[s[9]]);
        }
    }
    if ((s[0xC] & 0xF) == 0) s[9] = static_cast<unsigned char>(s[9] + 1);
    SH_AT(unsigned char (__cdecl*)(), at::kDustStep.v)();
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1u);
    s = S();
    if (rv::As<records::EffectWork>(s)->work_0c == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4789A0 (variant 3, state 2): the dust stepped; none live:
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V3End(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kDustStep.v)() == 0) SH_CALL(Effect_Release)();
}

// original 0x4789B0: variant 4, jmp [EffectKind49_V4States + +2 * 4], unbounded
// (its four states are group E2E's).
extern "C" void __cdecl EffectKind49_V4Run(void) {
    Dispatch("EffectKind49_V4Run", AddressOf(EffectKind49_V4States), EffectKind49_V4States_count, 2);
}

// original 0x4789D0 (step 0): the ground under the record (+0x3C); 0x6761D0 =
// the ring record 0x92D1C8, its point the record's +0x34 / +0x38 / +0x3C, its
// word +0x10 = 0; +9 = 0, +0xC = 0x5F, +2 up.
extern "C" void __cdecl EffectKind48_State7_Start(void) {
    Ground(0);
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    Effect2d_DustAnchor = ::bof3::Img(Effect2e_RingRecord_addr);
    Effect2e_RingRecord = static_cast<std::uint32_t>(so->x);
    SetUL(At(::bof3::Img(Effect2e_RingRecord_addr) + 4), static_cast<std::uint32_t>(so->z));
    SetUL(At(::bof3::Img(Effect2e_RingRecord_addr) + 8), static_cast<std::uint32_t>(so->height));
    SetWord(At(::bof3::Img(Effect2e_RingRecord_addr) + 0x10), 0);
    s[9] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x5F);
    NextStep();
}

// original 0x478A40 (step 1): unless the low byte of +0xC meets the mask
// 0x65460C[+9], the ring's word +0x10 up 0x654614[+9]; when +0xC's low four
// bits are 0, +9 up; the ring drawn (0x479EE0); +0xC down, at 0 +0xC = 0x10 and
// +2 up. The two byte tables are read in place by the whole byte +9 (the
// original's reach past their eight entries is the image's next bytes).
extern "C" void __cdecl EffectKind48_State7_Swell(void) {
    unsigned char* s = S();
    const unsigned index = s[9];
    if ((s[0xC] & At(::bof3::Img(at::kState7Steps.v))[index]) == 0) AddWord(Cell(::bof3::Img(at::kRingPtr.v)) + 0x10, At(::bof3::Img(at::kState7Adds.v))[index]);
    if ((s[0xC] & 0xF) == 0) s[9] = static_cast<unsigned char>(s[9] + 1);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing.v)(Cell(::bof3::Img(at::kRingPtr.v)));
    if (!CountDownC(&s)) return;
    SetUL(s + 0xC, 0x10);
    NextStep();
}

// original 0x478AB0 (step 2): the ring's word +0x10 up 4; the ring drawn; +0xC
// down, at 0 +2 up.
extern "C" void __cdecl EffectKind48_State7_Widen(void) {
    AddWord(Cell(::bof3::Img(at::kRingPtr.v)) + 0x10, 4);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing.v)(Cell(::bof3::Img(at::kRingPtr.v)));
    unsigned char* s;
    if (!CountDownC(&s)) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478AF0 (step 3): the ring's height +8 up 0x1000000; the ring
// drawn; once its height is 0x10000000 or more (signed), a tail jmp to
// Effect_Release.
extern "C" void __cdecl EffectKind48_State7_Lift(void) {
    AddLong(Cell(::bof3::Img(at::kRingPtr.v)) + 8, 0x1000000u);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing.v)(Cell(::bof3::Img(at::kRingPtr.v)));
    if (SL(Cell(::bof3::Img(at::kRingPtr.v)) + 8) < 0x10000000) return;
    SH_CALL(Effect_Release)();
}

// original 0x478B30: jmp [EffectKind48_State8_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State8_Run(void) {
    DispatchEffect2e("EffectKind48_State8_Run", AddressOf(EffectKind48_State8_Steps), EffectKind48_State8_Steps_count, 2);
}

// original 0x478B50 (step 0): the ground under the record; the spiral at its
// point, turn word 0 (SpiralStart); Sound_PlayEffect(0x209) after +2 up.
extern "C" void __cdecl EffectKind48_State8_Start(void) {
    Ground(0);
    SpiralStart(0);
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x478BE0 (step 1): the spiral's frame, its turn up 0x40.
extern "C" void __cdecl EffectKind48_State8_Turn(void) { SpiralTurn(0x40); }

// original 0x478C30: jmp [EffectKind48_State9_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State9_Run(void) {
    DispatchEffect2e("EffectKind48_State9_Run", AddressOf(EffectKind48_State9_Steps), EffectKind48_State9_Steps_count, 2);
}

// original 0x478C50 (step 0): the record at the leader's x / z, the ground
// under it; the sparks cleared (0x4790C0); +0xC = 0x28, +6 = 1, +2 up.
extern "C" void __cdecl EffectKind48_State9_Start(void) {
    AtLeader();
    Ground(0);
    SH_AT(void (__cdecl*)(), at::kSparksInit.v)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x28);
    S()[6] = 1;
    NextStep();
}

// original 0x478CC0 (step 1): the spiral stepped and drawn (0x479B70); while
// +0xC is above 8 (signed), a free spark looked for (0x47CF20, eax) and, when
// +0xC's low two bits are 0 and one was found, set (0x479160); the sparks run
// (0x479260); +0xC down, at 0 +2 up.
extern "C" void __cdecl EffectKind48_State9_Emit(void) {
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralDraw.v)(Cell(::bof3::Img(at::kSpiralPtr.v)));
    if (SL(S() + 0xC) > 8) {
        unsigned char* const spark = SH_AT(unsigned char* (__cdecl*)(), at::kSparkFree.v)();
        if ((S()[0xC] & 3) == 0 && spark != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kSparkSet.v)(spark);
    }
    SH_AT(unsigned char (__cdecl*)(), at::kSparksRun.v)();
    unsigned char* s;
    if (!CountDownC(&s)) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478D20 (step 2): the sparks run; when none is left (al 0), a tail
// jmp to Effect_Release.
extern "C" void __cdecl EffectKind48_State9_Drain(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kSparksRun.v)() != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x478D30: jmp [EffectKind48_State10_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State10_Run(void) {
    DispatchEffect2e("EffectKind48_State10_Run", AddressOf(EffectKind48_State10_Steps), EffectKind48_State10_Steps_count, 2);
}

// original 0x478D50 (step 0): the record at the leader's x / z, the ground under
// it; the spiral there, turn word 0x800 (no sound).
extern "C" void __cdecl EffectKind48_State10_Start(void) {
    AtLeader();
    Ground(0);
    SpiralStart(0x800);
}

// original 0x478DF0 (step 1): the spiral's frame, its turn down 0x40.
extern "C" void __cdecl EffectKind48_State10_Turn(void) { SpiralTurn(0xFFC0u); }

// original 0x478E40: jmp [EffectKind48_State11_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State11_Run(void) {
    DispatchEffect2e("EffectKind48_State11_Run", AddressOf(EffectKind48_State11_Steps), EffectKind48_State11_Steps_count, 2);
}

// original 0x478E60 (step 0): the record at the leader's x / z, the ground under
// it; the sphere built; +0xC = 0x78, +2 up.
extern "C" void __cdecl EffectKind48_State11_Start(void) {
    AtLeader();
    Ground(0);
    SH_CALL(EffectSphere_Build)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x78);
    NextStep();
}

// original 0x478EC0 (step 1): the sphere drawn; +0xC down, at 0 a tail jmp to
// Effect_Release.
extern "C" void __cdecl EffectKind48_State11_Draw(void) {
    SH_CALL(EffectSphere_Draw)();
    unsigned char* s;
    if (!CountDownC(&s)) return;
    SH_CALL(Effect_Release)();
}

// original 0x478EF0: jmp [EffectKind48_State12_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State12_Run(void) {
    DispatchEffect2e("EffectKind48_State12_Run", AddressOf(EffectKind48_State12_Steps), EffectKind48_State12_Steps_count, 2);
}

// original 0x478F10 (step 0): the burst's size word 0x92C49E = 0xC0; the burst
// stepped 32 times (0x4794D0); when +6 is set Sound_PlayEffect(0x203); +2 up.
extern "C" void __cdecl EffectKind48_State12_Start(void) {
    SetWord(At(::bof3::Img(at::kBurstSize.v)), 0xC0);
    for (int i = 0; i < 0x20; ++i) SH_AT(void (__cdecl*)(unsigned char*), at::kBurstStep.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
    if (S()[6] != 0) SH_CALL(Sound_PlayEffect)(0x203);
    NextStep();
}

// original 0x478F60 (step 1): the burst's frame; the record's point +0x34 /
// +0x38 / +0x3C moved by +0xC / +0x10 / +0x14; once +0x34 is +0x18 and +0x38 is
// +0x1C, +9 = 0x2F and +2 up.
extern "C" void __cdecl EffectKind48_State12_Travel(void) {
    BurstFrame();
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    AddLong(s + 0x34, rv::As<records::EffectWork>(s)->work_0c);
    AddLong(s + 0x38, rv::As<records::EffectWork>(s)->work_10);
    AddLong(s + 0x3C, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y));
    if (static_cast<std::uint32_t>(so->x) != static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) || static_cast<std::uint32_t>(so->z) != static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c)) return;
    s[9] = 0x2F;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478FD0 (step 2): the burst's frame; +9 down; at 0, when +6 is 0 a
// tail jmp to Effect_Release, else Sound_PlayEffect(0x204), +9 = 0x20 and +2 up.
extern "C" void __cdecl EffectKind48_State12_Hold(void) {
    BurstFrame();
    unsigned char* s;
    if (!CountDown9(&s)) return;
    if (s[6] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(Sound_PlayEffect)(0x204);
    S()[9] = 0x20;
    NextStep();
}

// original 0x479030 (step 3): the burst's frame; its size word up 6; +9 down,
// at 0 +9 = 0x20 and +2 up.
extern "C" void __cdecl EffectKind48_State12_Grow(void) {
    BurstFrame();
    AddWord(At(::bof3::Img(at::kBurstSize.v)), 6);
    unsigned char* s;
    if (!CountDown9(&s)) return;
    s[9] = 0x20;
    NextStep();
}

// original 0x479080 (step 4): the burst's frame; its size word down 0xC; +9
// down, at 0 +2 up (to WeretigerFx_Next).
extern "C" void __cdecl EffectKind48_State12_Shrink(void) {
    BurstFrame();
    AddWord(At(::bof3::Img(at::kBurstSize.v)), 0xFFF4u);
    unsigned char* s;
    if (!CountDown9(&s)) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4790C0: +0 of each of the 8 sparks = 0; the two sound flags
// 0x6761C9 and 0x6761C8 = 0.
extern "C" void __cdecl EffectGlowSparks_Clear(void) {
    for (unsigned i = 0; i < rest_3e::at::kSparks; ++i) Spark(i)[0] = 0;
    Rest3e_GlowSounded = 0;
    Rest3e_RiseSounded = 0;
}

// original 0x4790F0: a spark set going near the record: +0 = 1, +1 = 0, +2 =
// 8, +3 = 0, +8 = 0, +4 = 0x40; x = ((Rand & 0xFF) - 0x80) << 11 + the
// record's x, z likewise (Sprite_Current read after each Rand), the height the
// record's +0x3C.
extern "C" void __cdecl EffectGlowSparks_StartRise(unsigned char* spark) {
    spark[0] = 1;
    spark[1] = 0;
    spark[2] = 8;
    spark[3] = 0;
    SetUL(spark + 8, 0);
    spark[4] = 0x40;
    U r = static_cast<U>(SH_CALL(Rand)());
    SetUL(spark + 0xC, (((r & 0xFFu) - 0x80u) << 11) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x));
    r = static_cast<U>(SH_CALL(Rand)());
    SetUL(spark + 0x10, (((r & 0xFFu) - 0x80u) << 11) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z));
    SetUL(spark + 0x14, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->height));
}

// original 0x479160: a spark set on a sphere round the record: +0 = 1, +1 =
// 0, +2 = 8, +3 = 0, +8 = 0, +4 = 1; a = Rand & 0x7FF, b = Rand & 0xFFF, the
// radius r = the low word, signed, of ((Rand % 2, signed) << 12 | Rand &
// 0xFFF); x = (sin a cos b sar 12) r sar 8 + the record's x, z = (sin a sin b
// sar 12) r sar 8 + its z, the height cos a * r + its +0x3C + 0x2000000
// (Sprite_Current read for each).
extern "C" void __cdecl EffectGlowSparks_StartBurst(unsigned char* spark) {
    spark[0] = 1;
    spark[1] = 0;
    spark[2] = 8;
    spark[3] = 0;
    SetUL(spark + 8, 0);
    spark[4] = 1;
    const U a = static_cast<U>(SH_CALL(Rand)()) & 0x7FFu;
    const U b = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const std::int32_t half = static_cast<std::int32_t>(SH_CALL(Rand)()) % 2;
    const U low = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U r = static_cast<U>(static_cast<std::int16_t>((static_cast<U>(half) << 12) | low));
    const U sa = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    const U cb = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(b)));
    U v = Sar(Mul(sa, cb), 12);
    SetUL(spark + 0xC, Sar(Mul(v, r), 8) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x));
    const U sa2 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    const U sb = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(b)));
    v = Sar(Mul(sa2, sb), 12);
    SetUL(spark + 0x10, Sar(Mul(v, r), 8) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z));
    const U ca = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
    SetUL(spark + 0x14, Mul(ca, r) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->height) + 0x2000000u);
}

// original 0x479260 (E2F's EffectKind52_MoveSparks instruction for
// instruction but its table): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd
// 1, slot 1); EffectGte_LoadMapCamera; each spark in use called through
// EffectGlowSparks_States by its +1 (unbounded; each handed the spark) and
// drawn (EffectGlowSparks_Draw). Answers 1 when a spark was in use, else 0.
extern "C" unsigned char __cdecl EffectGlowSparks_Run(void) {
    DrawModeRest3e(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < rest_3e::at::kSparks; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] == 0) continue;
        const unsigned state = k[1];
        if (state >= EffectGlowSparks_States_count)
            bof3::Fatal("EffectGlowSparks_Run: spark %u's state +1 is %u, past the %u entries of EffectGlowSparks_States - "
                        "the original calls through the dword after (docs/rest_3e.md section 7)",
                        i, state, EffectGlowSparks_States_count);
        reinterpret_cast<void (__cdecl*)(unsigned char*)>(
            static_cast<std::uintptr_t>(ULRest3e(reinterpret_cast<const unsigned char*>(EffectGlowSparks_States) + 4 * state)))(k);
        SH_CALL(EffectGlowSparks_Draw)(k);
        any = 1;
    }
    return any;
}

// original 0x4792E0 (E3A's EffectKind68_DrawMote but its size 0x20 and slot
// 1): the spark's point +0xC projected (o) and its size {0x20, a word the
// original never writes} projected (EffectGte_ProjectSize reads the first); r
// = that size's first word + (Frame_Counter & 1), its low word signed; a disc
// of 16 semi-transparent POLY_G3 round o, the centre in the shade +3, the rim
// black at (cos, sin) * r sar 12, 0x100 apart; each committed to slot 1
// (0x34).
extern "C" void __cdecl EffectGlowSparks_Draw(unsigned char* spark) {
    alignas(4) short sz[2] = {0x20, 0};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(Point(spark + 0xC), sz, r);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0xC), Out(o));
    const U radius = static_cast<U>(static_cast<std::int16_t>(static_cast<U>(static_cast<unsigned short>(r[0])) + (Frame_Counter & 1u)));
    U angle = 0;
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, ULRest3e(o));
        SetUL(p + 0xC, ULRest3e(o + 4));
        U d = Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x18, I(static_cast<std::int32_t>(d)) + F(o));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), radius), 12);
        angle += 0x100;
        StF(p + 0x1C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x28, I(static_cast<std::int32_t>(d)) + F(o));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x2C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        SetUL(p + 0x30, ULRest3e(o + 8));
        SetUL(p + 0x20, ULRest3e(o + 8));
        SetUL(p + 0x10, ULRest3e(o + 8));
        const unsigned char shade = spark[3];
        p[6] = shade;
        p[5] = shade;
        p[4] = shade;
        p[0x26] = p[0x25] = p[0x24] = 0;
        p[0x16] = p[0x15] = p[0x14] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x479420 (EffectGlowSparks_States[0]): the shade +3 up 6; the count
// +2 down, at 0 the count its reload +4, +1 up, and sound 0x200 the first time
// (the flag 0x6761C9).
extern "C" void __cdecl EffectGlowSparks_Glow(unsigned char* spark) {
    spark[3] = static_cast<unsigned char>(spark[3] + 6);
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] != 0) return;
    spark[2] = spark[4];
    spark[1] = static_cast<unsigned char>(spark[1] + 1);
    if (Rest3e_GlowSounded != 0) return;
    SH_CALL(Sound_PlayEffect)(at::kSoundGlow);
    Rest3e_GlowSounded = 1;
}

// original 0x479470 (EffectGlowSparks_States[2]): the first time (the flag
// 0x6761C8) sound 0x201 unless the record's +6 is set, the flag set either way;
// then the rise speed +8 up 0x40000, the height +0x14 up by it, the shade +3
// up 6, the count +2 down, at 0 the spark freed (+0 = 0).
extern "C" void __cdecl EffectGlowSparks_Rise(unsigned char* spark) {
    if (Rest3e_RiseSounded == 0) {
        if (S()[6] == 0) SH_CALL(Sound_PlayEffect)(at::kSoundRise);
        Rest3e_RiseSounded = 1;
    }
    const U speed = ULRest3e(spark + 8) + 0x40000u;
    SetUL(spark + 8, speed);
    SetUL(spark + 0x14, ULRest3e(spark + 0x14) + speed);
    spark[3] = static_cast<unsigned char>(spark[3] + 6);
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] == 0) spark[0] = 0;
}

// original 0x4794D0 (0x1D1 bytes): E2F's EffectKind52_TrailUpdate (0x47D040)
// and E3D's EffectKind80_TrailStep (0x4873E0) instruction for instruction but
// the call targets, which are the same callees: called directly (its callees
// go through the harness inside it). docs/effect_2f.md has the body.
extern "C" void __cdecl EffectGlowTrail_Update(unsigned char* trail) { EffectKind52_TrailUpdate(trail); }

// original 0x4796B0 (0x2BE bytes): E2F's EffectKind52_TrailDraw without the
// depths - a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1); the cap
// at point 0 (EffectTrail_DrawCap at its angle - 0x400, shade 0x80); for each
// pair of points k, k + 1 (screen floats at +0x10) whose x or y differ (equal
// or unordered both = skipped), two Gouraud quads at the packet cursor,
// semi-transparent: the first from point k's screen x, y and x + (cos(a +
// 0x400) w sar 12), y + (sin ..) to point k + 1's likewise (a, w each point's
// angle +0x1C and width +0x1E, zero-extended), shaded the running shade at
// point k's and it - 4 at k + 1's (black at the offset ones), committed to slot
// 1 (0x44); the second a copy of the first (0x44 bytes from the cursor back)
// with the offset vertices at a - 0x400, committed; the running shade down 4.
// The depth words of the quads are not written. The cap at point 31 at its
// angle + 0x400 and the running shade.
extern "C" void __cdecl EffectGlowTrail_Draw(unsigned char* trail) {
    DrawModeRest3e(1, 0x3C0, 0, 1, 1);
    unsigned char shade = 0x80;
    SH_CALL(EffectTrail_DrawCap)(trail + 0x10, Word(trail + 0x1E), (Word(trail + 0x1C) - 0x400u) & 0xFFFFu, 0x80);
    for (unsigned k = 0; k < 0x1F; ++k) {
        const unsigned char* const p = trail + 0x10 + 0x20 * k;
        const unsigned char* const q = p + 0x20;
        if (EqualOrUnordered(p, q) && EqualOrUnordered(p + 4, q + 4)) continue;
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SetUL(prim + 8, ULRest3e(p));
        SetUL(prim + 0xC, ULRest3e(p + 4));
        U v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(p + 0xC) + 0x400u)), p);
        StF(prim + 0x18, I(static_cast<std::int32_t>(v)) + F(p));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(p + 0xC) + 0x400u)), p);
        StF(prim + 0x1C, I(static_cast<std::int32_t>(v)) + F(p + 4));
        SetUL(prim + 0x28, ULRest3e(q));
        SetUL(prim + 0x2C, ULRest3e(q + 4));
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(q + 0xC) + 0x400u)), q);
        StF(prim + 0x38, I(static_cast<std::int32_t>(v)) + F(q));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(q + 0xC) + 0x400u)), q);
        const auto next = static_cast<unsigned char>(shade - 4);
        prim[6] = prim[5] = prim[4] = shade;
        prim[0x16] = prim[0x15] = prim[0x14] = 0;
        StF(prim + 0x3C, I(static_cast<std::int32_t>(v)) + F(q + 4));
        prim[0x26] = prim[0x25] = prim[0x24] = next;
        prim[0x36] = prim[0x35] = prim[0x34] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        unsigned char* const copy = Gfx_PacketNext;
        std::memmove(copy, copy - 0x44, 0x44);
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(p + 0xC) - 0x400u)), p);
        StF(copy + 0x18, I(static_cast<std::int32_t>(v)) + F(p));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(p + 0xC) - 0x400u)), p);
        StF(copy + 0x1C, I(static_cast<std::int32_t>(v)) + F(p + 4));
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(q + 0xC) - 0x400u)), q);
        StF(copy + 0x38, I(static_cast<std::int32_t>(v)) + F(q));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(q + 0xC) - 0x400u)), q);
        StF(copy + 0x3C, I(static_cast<std::int32_t>(v)) + F(q + 4));
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        shade = next;
    }
    SH_CALL(EffectTrail_DrawCap)(trail + 0x3F0, Word(trail + 0x3FE), (Word(trail + 0x3FC) + 0x400u) & 0xFFFFu, shade);
}

// original 0x479970 (0x44 bytes): a & 0xFFF and b & 0xFFF ordered (lo, hi);
// (lo + hi) >> 1, plus 0x800 when hi - lo is 0x800 or more. eax.
extern "C" long __cdecl EffectAngle_Mean(unsigned a, unsigned b) {
    U lo = a & 0xFFFu;
    U hi = b & 0xFFFu;
    if (lo > hi) {
        const U t = lo;
        lo = hi;
        hi = t;
    }
    const U mean = (lo + hi) >> 1;
    return static_cast<long>(hi - lo < 0x800u ? mean : mean + 0x800u);
}

// original 0x4799C0 (E2E's kind 0x48 states 8 and 10 call it): set up.
// EffectGte_LoadMapCamera; the light +0xD14 = (0, 0x1000, 0x1000) normalised in
// place (Gte_VectorNormal); ring 0's 32 points and shades (SpiralPoint, the
// dark shade 0x70), each copied to rings 1..7 at once (the point, then the
// shade).
extern "C" void __cdecl EffectSpiral_Init(unsigned char* spiral) {
    SH_CALL(EffectGte_LoadMapCamera)();
    SetUL(spiral + 0xD18, 0x1000);
    SetUL(spiral + 0xD14, 0);
    SetUL(spiral + 0xD1C, 0x1000);
    SH_CALL(Gte_VectorNormal)(Point(spiral + 0xD14), reinterpret_cast<long*>(spiral + 0xD14));
    for (unsigned i = 0; i < 0x20; ++i) {
        unsigned char* const shade = spiral + 0xC10 + i;
        unsigned char* const screen = spiral + 0x10 + 12 * i;
        SpiralPoint(spiral, i, 0x70, shade, screen);
        for (unsigned r = 1; r < 8; ++r) {
            Copy12Rest3e(screen + kRing * r, screen);
            shade[0x20 * r] = *shade;
        }
    }
}

// original 0x479B70 (E2E's kind 0x48 states 8, 9 and 10 call it; PSX twin
// 0x801F9330): the rings' points moved out one (ring 6 to 7 .. 0 to 1; the
// shades stay); EffectGte_LoadMapCamera; ring 0 again (the dark shade 7); a
// draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1); for ring k = 1..7
// and point i of 32 (n = i + 1 mod 32): a POLY_G4 at the packet cursor,
// semi-transparent - ring k - 1's points i and n, ring k's points i and n -
// shaded ring k - 1's shades i and n times (8 - k) / 7 and ring k's shade i
// (twice: the original reads i for the fourth vertex too) times (7 - k) / 7;
// committed to slot 1 (0x44).
extern "C" void __cdecl EffectSpiral_StepDraw(unsigned char* spiral) {
    for (unsigned r = 7; r != 0; --r)
        for (unsigned i = 0; i < 0x20; ++i) Copy12Rest3e(spiral + 0x10 + kRing * r + 12 * i, spiral + 0x10 + kRing * (r - 1) + 12 * i);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < 0x20; ++i) SpiralPoint(spiral, i, 7, spiral + 0xC10 + i, spiral + 0x10 + 12 * i);
    DrawModeRest3e(1, 0x3C0, 0, 1, 1);
    for (unsigned k = 1; k < 8; ++k) {
        const std::int32_t outer = static_cast<std::int32_t>(8 - k), inner = static_cast<std::int32_t>(7 - k);
        for (unsigned i = 0; i < 0x20; ++i) {
            const unsigned n = (i + 1) & 0x1F;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            Copy12Rest3e(p + 8, spiral + 0x10 + kRing * (k - 1) + 12 * i);
            Copy12Rest3e(p + 0x18, spiral + 0x10 + kRing * (k - 1) + 12 * n);
            p[4] = p[5] = p[6] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * (k - 1) + i] * outer / 7);
            p[0x14] = p[0x15] = p[0x16] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * (k - 1) + n] * outer / 7);
            Copy12Rest3e(p + 0x28, spiral + 0x10 + kRing * k + 12 * i);
            Copy12Rest3e(p + 0x38, spiral + 0x10 + kRing * k + 12 * n);
            p[0x24] = p[0x25] = p[0x26] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * k + i] * inner / 7);
            p[0x34] = p[0x35] = p[0x36] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * k + i] * inner / 7);
            SH_CALL(Gfx_CommitPrim)(1, 0x44);
        }
    }
}

extern "C" void __cdecl EffectRing_Draw(const unsigned char* ring) {
    alignas(4) unsigned char pt[12];
    alignas(4) unsigned char bottom[12], top[12];
    int angle = 0;
    RimPoint(ring, angle, pt);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(bottom));
    SetUL(pt + 8, ULRest3e(ring + 8) + 0x7000000u);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(top));
    unsigned char s = 0x80;
    unsigned shade = Word(ring + 0x10);
    if (shade > 0xFF) shade = 0xFF;
    for (unsigned i = 0; i < 0x20; ++i) {
        angle += 0x80;
        const int t = angle & 0xFFFF;
        RimPoint(ring, t, pt);
        const U tp = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tp & 0xFFFFu, 0);
        SH_CALL(MapView_LinkPrimAt)(ULRest3e(pt), ULRest3e(pt + 4), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12Rest3e(p + 8, bottom);
        Copy12Rest3e(p + 0x18, top);
        p[4] = p[5] = p[6] = static_cast<unsigned char>(shade);
        p[0x14] = p[0x15] = p[0x16] = static_cast<unsigned char>(shade);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(bottom));
        SetUL(pt + 8, ULRest3e(ring + 8) + 0x7000000u);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(top));
        Copy12Rest3e(p + 0x28, bottom);
        Copy12Rest3e(p + 0x38, top);
        if (i >= 4 && i < 0x14)
            s = static_cast<unsigned char>(s - 4);
        else
            s = static_cast<unsigned char>(s + 4);
        shade = ((static_cast<unsigned>(s) * Word(ring + 0x10)) & 0xFFFFu) >> 7;
        if (shade > 0xFF) shade = 0xFF;
        p[0x24] = p[0x25] = p[0x26] = static_cast<unsigned char>(shade);
        p[0x34] = p[0x35] = p[0x36] = static_cast<unsigned char>(shade);
        SH_CALL(MapView_LinkPrimAt)(ULRest3e(pt), ULRest3e(pt + 4), 0, 0x44);
    }
}

// original 0x47A110: +0 of each of the 64 = 0.
extern "C" void __cdecl EffectDust_Clear(void) {
    for (unsigned i = 0; i < at::kDustCount; ++i) Dust(i)[0] = 0;
}

// original 0x47A130: the first of the 64 whose +0 is 0, or null.
extern "C" unsigned char* __cdecl EffectDust_FindFree(void) {
    for (unsigned i = 0; i < at::kDustCount; ++i)
        if (Dust(i)[0] == 0) return Dust(i);
    return nullptr;
}

// original 0x47A150: a dust record set round Sprite_Current's point: +0 = 1,
// +1 = 0, +2 = 0x10, +0x18 = +0x14 = 0; the radius ((Rand % 2, signed) << 8 |
// Rand & 0xFF) << 8, the angle Rand & 0xFC0; x = cos * radius sar 12 + the
// record's x, z = sin * radius sar 12 + its z (Sprite_Current read for each);
// the height and the ground AreaMap_Elevation(x, z) << 16 (its low word,
// signed).
extern "C" void __cdecl EffectDust_Start(unsigned char* dust) {
    dust[0] = 1;
    dust[1] = 0;
    dust[2] = 0x10;
    SetUL(dust + 0x18, 0);
    SetUL(dust + 0x14, 0);
    const std::int32_t half = static_cast<std::int32_t>(SH_CALL(Rand)()) % 2;
    const U low = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
    const U radius = ((static_cast<U>(half) << 8) | low) << 8;
    const int angle = static_cast<int>(static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    const U cos = static_cast<U>(SH_CALL(Math_Cos)(angle));
    SetUL(dust + 4, Sar(Mul(cos, radius), 12) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x));
    const U sin = static_cast<U>(SH_CALL(Math_Sin)(angle));
    const U z = Sar(Mul(sin, radius), 12) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z);
    SetUL(dust + 8, z);
    const U ground = static_cast<U>(static_cast<std::int16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(ULRest3e(dust + 4)), static_cast<long>(z)))) << 16;
    SetUL(dust + 0x1C, ground);
    SetUL(dust + 0xC, ground);
}

// original 0x47A200 (E2D's kind 0x49 variant 3 calls it; PSX twin 0x801F9EFC):
// a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1);
// EffectGte_LoadMapCamera; each dust record in use: its column drawn
// (EffectDust_DrawColumn), its fan when the column answered 1; then the rise
// +0x18 up 0x100000, the height +0xC up by it, the spread +0x14 up 0x2000, the
// count +2 down, at 0 the record freed. Answers 1 when one was in use.
extern "C" unsigned char __cdecl EffectDust_Run(void) {
    DrawModeRest3e(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kDustCount; ++i) {
        unsigned char* const d = Dust(i);
        if (d[0] == 0) continue;
        if (SH_CALL(EffectDust_DrawColumn)(d) != 0) SH_CALL(EffectDust_DrawFan)(d);
        const U rise = ULRest3e(d + 0x18) + 0x100000u;
        SetUL(d + 0x14, ULRest3e(d + 0x14) + 0x2000u);
        SetUL(d + 0x18, rise);
        SetUL(d + 0xC, ULRest3e(d + 0xC) + rise);
        d[2] = static_cast<unsigned char>(d[2] - 1);
        if (d[2] == 0) d[0] = 0;
        any = 1;
    }
    return any;
}

// original 0x47A2B0 (PSX twin 0x801F9A80): a POLY_F4 at the packet cursor,
// semi-transparent, colour 0x40: the top point (x, z, height + spread << 8)
// and the bottom (x, z, height - spread << 8, raised to the ground +0x1C when
// below it - the answer 1 then, else 0), each projected; each end's two
// vertices its screen x -/+ the float 0x5C41B8, its y and depth; committed to
// slot 1 (0x38).
extern "C" unsigned char __cdecl EffectDust_DrawColumn(const unsigned char* dust) {
    unsigned char clamped = 0;
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    alignas(4) unsigned char pt[12], sc[12];
    SetUL(pt, ULRest3e(dust + 4));
    SetUL(pt + 4, ULRest3e(dust + 8));
    SetUL(pt + 8, (ULRest3e(dust + 0x14) << 8) + ULRest3e(dust + 0xC));
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(sc));
    const unsigned char* const k = At(at::kHalfWidth);
    StF(p + 8, F(sc) - F(k));
    SetUL(p + 0xC, ULRest3e(sc + 4));
    StF(p + 0x14, F(sc) + F(k));
    SetUL(p + 0x18, ULRest3e(sc + 4));
    SetUL(p + 0x1C, ULRest3e(sc + 8));
    SetUL(p + 0x10, ULRest3e(sc + 8));
    SetUL(pt, ULRest3e(dust + 4));
    SetUL(pt + 4, ULRest3e(dust + 8));
    U low = ULRest3e(dust + 0xC) - (ULRest3e(dust + 0x14) << 8);
    const U ground = ULRest3e(dust + 0x1C);
    if (static_cast<std::int32_t>(low) < static_cast<std::int32_t>(ground)) {
        low = ground;
        clamped = 1;
    }
    SetUL(pt + 8, low);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(sc));
    StF(p + 0x20, F(sc) - F(k));
    SetUL(p + 0x24, ULRest3e(sc + 4));
    StF(p + 0x2C, F(sc) + F(k));
    SetUL(p + 0x30, ULRest3e(sc + 4));
    SetUL(p + 0x34, ULRest3e(sc + 8));
    SetUL(p + 0x28, ULRest3e(sc + 8));
    p[4] = p[5] = p[6] = 0x40;
    SH_CALL(Gfx_CommitPrim)(1, 0x38);
    return clamped;
}

// original 0x47A3D0 (PSX twin 0x801F9BDC): a fan of 16 semi-transparent
// POLY_G3 on the ground round the dust's point: the centre (x, z, the ground
// AreaMap_Elevation(x, z) << 16) projected, shade 0x40; the rim at angle t
// (x + cos t * 4, z + sin t * 4) at the height AreaMap_Elevation(x, z) << 16 -
// read again at the dust's own point for each, not at the rim - projected,
// black; triangle j joins the rim at t = 0x100 j and 0x100 (j + 1); each
// committed to slot 1 (0x34).
extern "C" void __cdecl EffectDust_DrawFan(const unsigned char* dust) {
    alignas(4) unsigned char pt[12], centre[12], rim[12];
    SetUL(pt, ULRest3e(dust + 4));
    SetUL(pt + 4, ULRest3e(dust + 8));
    SetUL(pt + 8, static_cast<U>(static_cast<std::int16_t>(
                      SH_CALL(AreaMap_Elevation)(static_cast<long>(ULRest3e(dust + 4)), static_cast<long>(ULRest3e(dust + 8)))))
                      << 16);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(centre));
    U angle = 0;
    // the rim point at angle t: x, z, then the height
    auto rimAt = [&](int t) {
        const U cos = static_cast<U>(SH_CALL(Math_Cos)(t));
        SetUL(pt, ULRest3e(dust + 4) + (cos << 2));
        const U sin = static_cast<U>(SH_CALL(Math_Sin)(t));
        SetUL(pt + 4, ULRest3e(dust + 8) + (sin << 2));
        SetUL(pt + 8, static_cast<U>(static_cast<std::int16_t>(
                          SH_CALL(AreaMap_Elevation)(static_cast<long>(ULRest3e(dust + 4)), static_cast<long>(ULRest3e(dust + 8)))))
                          << 16);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(rim));
    };
    rimAt(0);
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12Rest3e(p + 8, centre);
        angle += 0x100;
        Copy12Rest3e(p + 0x18, rim);
        rimAt(static_cast<int>(angle & 0xFFFFu));
        Copy12Rest3e(p + 0x28, rim);
        p[4] = p[5] = p[6] = 0x40;
        p[0x14] = p[0x15] = p[0x16] = 0;
        p[0x24] = p[0x25] = p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x47A560 (0x211 bytes): the sphere's vertices (x, y, z of s16 and a
// pad, 0x1000 = 1) - the pole (0, 0, 0x1000), 15 rings at the polar angles p =
// 0x80..0x780 of 32 at t = 0..0xF80 ((sin p cos t) sar 12, (sin p sin t) sar
// 12, cos p; Math_Sin(p), Math_Cos(t), Math_Sin(p), Math_Sin(t), Math_Cos(p)),
// the pole (0, 0, -0x1000); its 0x200 quads (vertex numbers: the cap round the
// first pole, 14 bands, the cap round the last); the light (0, 0x1000, 0x1000)
// normalised in place (Gte_VectorNormal); each vertex's shade: its dot with
// the light (32-bit), when above 0 the square root (0x5A7A90) at most 0xFFF,
// sar 7, else 0. al 0.
extern "C" unsigned char __cdecl EffectSphere_Build(void) {
    unsigned char* v = At(::bof3::Img(Effect2e_SphereVerts_addr));
    SetWord(v, 0);
    SetWord(v + 2, 0);
    SetWord(v + 4, 0x1000);
    v += 8;
    U polar = 0;
    for (int ring = 0; ring < 15; ++ring) {
        polar += 0x80;
        const int p = static_cast<int>(polar & 0xFFFFu);
        U turn = 0;
        for (int k = 0; k < 0x20; ++k) {
            const int t = static_cast<int>(turn & 0xFFFFu);
            U a = static_cast<U>(SH_CALL(Math_Sin)(p));
            SetWord(v, Sar(a * static_cast<U>(SH_CALL(Math_Cos)(t)), 12));
            a = static_cast<U>(SH_CALL(Math_Sin)(p));
            SetWord(v + 2, Sar(a * static_cast<U>(SH_CALL(Math_Sin)(t)), 12));
            SetWord(v + 4, static_cast<U>(SH_CALL(Math_Cos)(p)));
            v += 8;
            turn += 0x80;
        }
    }
    SetWord(v + 4, 0xF000);
    SetWord(v, 0);
    SetWord(v + 2, 0);
    unsigned char* q = At(::bof3::Img(at::kSphereQuads.v));
    auto quad = [&q](U a, U b, U c, U d) {
        SetWord(q, a);
        SetWord(q + 2, b);
        SetWord(q + 4, c);
        SetWord(q + 6, d);
        q += 8;
    };
    for (U j = 1; j <= 0x20; ++j) quad(0, 0, j, (j < 0x20 ? j : 0) + 1);
    for (U base = 0x21; base < 0x1E1; base += 0x20)
        for (U k = 0; k < 0x20; ++k) {
            const U e = k + 1 < 0x20 ? k + 1 : 0;
            quad(base + k - 0x20, base + e - 0x20, base + k, base + e);
        }
    for (U k = 0; k < 0x20; ++k) {
        const U e = k + 1 < 0x20 ? k + 1 : 0;
        quad(0x1C1 + k, 0x1C1 + e, 0x1E1, 0x1E1);
    }
    unsigned char* const light = At(::bof3::Img(Effect2e_SphereLight_addr));
    SetUL(light, 0);
    SetUL(light + 4, 0x1000);
    SetUL(light + 8, 0x1000);
    SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(light), reinterpret_cast<long*>(light));
    const unsigned char* vert = At(::bof3::Img(Effect2e_SphereVerts_addr));
    unsigned char* shade = At(::bof3::Img(at::kSphereShades.v));
    for (unsigned i = 0; i < at::kSphereVertCount; ++i, vert += 8, ++shade) {
        const U dot = SW(vert + 4) * UL(light + 8) + SW(vert) * UL(light) + SW(vert + 2) * UL(light + 4);
        if (static_cast<std::int32_t>(dot) <= 0) {
            *shade = 0;
            continue;
        }
        U root = static_cast<U>(SH_AT(long (__cdecl*)(long), effect_2e::at::kSqrt.v)(static_cast<long>(dot)));
        if (static_cast<std::int32_t>(root) > 0xFFF) root = 0xFFF;
        *shade = static_cast<unsigned char>(Sar(root, 7));
    }
    return 0;
}

// original 0x47A780 (0x1CE bytes): EffectGte_LoadMapCamera; each vertex (x, y,
// z) projected as the world point (x << 5 + Sprite_Current +0x34, y << 5 + +0x38,
// (z + 0x1000) << 13 + +0x3C; the record read afresh each vertex) into the
// screen table; a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), committed 0xC);
// then per quad a Gouraud quad at the packet cursor (Gpu_SetPolyG4,
// Gpu_SetSemiTrans(1)) of its four vertices' screen points, each shaded its
// vertex's shade (r = g = b), committed 0x44. al 0.
extern "C" unsigned char __cdecl EffectSphere_Draw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned char* v = At(::bof3::Img(Effect2e_SphereVerts_addr));
    for (unsigned i = 0; i < at::kSphereVertCount; ++i, v += 8) {
        const unsigned char* const s = S();
        long point[3];
        point[0] = static_cast<long>((SW(v) << 5) + UL(s + 0x34));
        point[1] = static_cast<long>((SW(v + 2) << 5) + UL(s + 0x38));
        point[2] = static_cast<long>(((SW(v + 4) + 0x1000u) << 13) + UL(s + 0x3C));
        SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(At(::bof3::Img(at::kSphereScreen.v) + 0xC * i)));
    }
    DrawMode(0x3C0, 0, 1);
    const unsigned char* q = At(::bof3::Img(at::kSphereQuads.v));
    const unsigned char* const shades = At(::bof3::Img(at::kSphereShades.v));
    for (unsigned n = 0; n < at::kSphereQuadCount; ++n, q += 8) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        unsigned vertex[4];
        for (unsigned c = 0; c < 4; ++c) vertex[c] = SphereVertex(Word(q + 2 * c));
        for (unsigned c = 0; c < 4; ++c) Copy12(prim + 8 + 0x10 * c, At(::bof3::Img(at::kSphereScreen.v) + 0xC * vertex[c]));
        for (unsigned c = 0; c < 4; ++c) prim[4 + 0x10 * c] = prim[5 + 0x10 * c] = prim[6 + 0x10 * c] = shades[vertex[c]];
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
    return 0;
}

// original 0x47A950: jmp [EffectKind4A_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4A_Run(void) {
    DispatchEffect2e("EffectKind4A_Run", AddressOf(EffectKind4A_States), EffectKind4A_States_count, 1);
}

// original 0x47A970 (state 0): the record's point = Sprite_Objects[1]'s
// (+0x34 / +0x38 / +0x3C); +0xC = 0xB4; the trail set (EffectKind4A_TrailInit);
// +1 up.
extern "C" void __cdecl EffectKind4A_Start(void) {
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(Sprite_Objects[1].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(Sprite_Objects[1].z));
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<std::uint32_t>(Sprite_Objects[1].height));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0xB4);
    SH_CALL(EffectKind4A_TrailInit)();
    NextState();
}

// original 0x47A9C0 (state 1): Gte_PushMatrix; at +0xC = 0xA5 the trail's spin
// byte 0x92CC82 = 1; while +0xC is above 0x5A (signed) the record's point =
// Sprite_Objects[1]'s; the trail stepped and drawn, the glow at its head
// (EffectKind4A_DrawGlow(0x92BF80, 0x80, 0x80, 0)); Gte_PopMatrix; +0xC down, at
// 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind4A_Follow(void) {
    SH_CALL(Gte_PushMatrix)();
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    if (rv::As<records::EffectWork>(s)->work_0c == 0xA5) Effect2e_TrailSpin = 1;
    if (SL(s + 0xC) > 0x5A) {
        so->x = static_cast<std::uint32_t>(Sprite_Objects[1].x);
        rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(Sprite_Objects[1].z));
        rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<std::uint32_t>(Sprite_Objects[1].height));
    }
    SH_CALL(EffectKind4A_TrailStep)();
    SH_CALL(EffectKind4A_TrailDraw)();
    SH_CALL(EffectKind4A_DrawGlow)(reinterpret_cast<const long*>(At(::bof3::Img(EffectKind30_Shards_addr))), 0x80, 0x80, 0);
    SH_CALL(Gte_PopMatrix)();
    unsigned char* t;
    if (!CountDownC(&t)) return;
    SH_CALL(Effect_Release)();
}

void EffectKind48FromEffect2d_Inject() {
    BOF3_INJECT(EffectKind48_Run);
    BOF3_INJECT(EffectKind48_Start);
    BOF3_INJECT(EffectKind48_Burst);
    BOF3_INJECT(EffectKind48_Fade);
    BOF3_INJECT(EffectKind48_SpawnRing);
    BOF3_INJECT(EffectKind48_FindFreeSpark);
    BOF3_INJECT(EffectKind48_ClearSparks);
    BOF3_INJECT(EffectKind48_StepSparks);
    BOF3_INJECT(EffectKind48_DrawSpark);
    BOF3_INJECT(EffectKind49_Run);
    BOF3_INJECT(EffectKind49_V0Run);
    BOF3_INJECT(EffectKind49_V0Start);
    BOF3_INJECT(EffectKind49_V0Emit);
    BOF3_INJECT(EffectKind49_V0End);
    BOF3_INJECT(EffectKind49_V1Run);
    BOF3_INJECT(EffectKind49_V1Start);
    BOF3_INJECT(EffectKind49_V1Fade);
    BOF3_INJECT(EffectKind49_V2Run);
    BOF3_INJECT(EffectKind49_V2Launch);
    BOF3_INJECT(EffectKind49_V2Fly);
    BOF3_INJECT(EffectKind49_V2Wait);
    BOF3_INJECT(EffectKind49_V2Rise);
    BOF3_INJECT(EffectKind49_V2Sink);
    BOF3_INJECT(EffectKind49_V3Run);
    BOF3_INJECT(EffectKind49_V3Start);
    BOF3_INJECT(EffectKind49_V3Burst);
    BOF3_INJECT(EffectKind49_V3End);
    BOF3_INJECT(EffectKind49_V4Run);
}

void EffectKind48FromEffect2e_Inject() {
    BOF3_INJECT(EffectKind48_State7_Start);
    BOF3_INJECT(EffectKind48_State7_Swell);
    BOF3_INJECT(EffectKind48_State7_Widen);
    BOF3_INJECT(EffectKind48_State7_Lift);
    BOF3_INJECT(EffectKind48_State8_Run);
    BOF3_INJECT(EffectKind48_State8_Start);
    BOF3_INJECT(EffectKind48_State8_Turn);
    BOF3_INJECT(EffectKind48_State9_Run);
    BOF3_INJECT(EffectKind48_State9_Start);
    BOF3_INJECT(EffectKind48_State9_Emit);
    BOF3_INJECT(EffectKind48_State9_Drain);
    BOF3_INJECT(EffectKind48_State10_Run);
    BOF3_INJECT(EffectKind48_State10_Start);
    BOF3_INJECT(EffectKind48_State10_Turn);
    BOF3_INJECT(EffectKind48_State11_Run);
    BOF3_INJECT(EffectKind48_State11_Start);
    BOF3_INJECT(EffectKind48_State11_Draw);
    BOF3_INJECT(EffectKind48_State12_Run);
    BOF3_INJECT(EffectKind48_State12_Start);
    BOF3_INJECT(EffectKind48_State12_Travel);
    BOF3_INJECT(EffectKind48_State12_Hold);
    BOF3_INJECT(EffectKind48_State12_Grow);
    BOF3_INJECT(EffectKind48_State12_Shrink);
    BOF3_INJECT(EffectAngle_Mean);
    BOF3_INJECT(EffectSphere_Build);
    BOF3_INJECT(EffectSphere_Draw);
    BOF3_INJECT(EffectKind4A_Run);
    BOF3_INJECT(EffectKind4A_Start);
    BOF3_INJECT(EffectKind4A_Follow);
}

void EffectKind48FromRest3e_Inject() {
    BOF3_INJECT(EffectGlowSparks_Clear);
    BOF3_INJECT(EffectGlowSparks_StartRise);
    BOF3_INJECT(EffectGlowSparks_StartBurst);
    BOF3_INJECT(EffectGlowSparks_Run);
    BOF3_INJECT(EffectGlowSparks_Draw);
    BOF3_INJECT(EffectGlowSparks_Glow);
    BOF3_INJECT(EffectGlowSparks_Rise);
    BOF3_INJECT(EffectGlowTrail_Update);
    BOF3_INJECT(EffectGlowTrail_Draw);
    BOF3_INJECT(EffectSpiral_Init);
    BOF3_INJECT(EffectSpiral_StepDraw);
    BOF3_INJECT(EffectRing_Draw);
    BOF3_INJECT(EffectDust_Clear);
    BOF3_INJECT(EffectDust_FindFree);
    BOF3_INJECT(EffectDust_Start);
    BOF3_INJECT(EffectDust_Run);
    BOF3_INJECT(EffectDust_DrawColumn);
    BOF3_INJECT(EffectDust_DrawFan);
}

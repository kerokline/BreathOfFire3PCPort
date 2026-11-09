// Moved here by tools/move_funcs.py from effect_2a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_2e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2a.h"
#include "game/effect_2a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

void SetD(unsigned char* p, double d) { SetF(p, static_cast<float>(d)); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2a.md section 2)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// A draw mode for the page (x, y) with transparency `abr`, and `dtd` its
// dither flag: Gpu_GetTPage(0, abr, x, y), Gpu_SetDrawMode(the packet cursor,
// 0, dtd, the page's low word, 0).
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
}

// The pool's records.
unsigned char* Pool(U stride, unsigned i) { return At(::bof3::Img(EffectKind30_Shards_addr) + stride * i); }

}  // namespace

// The centre 0x92D380: Sprite_ObjectsExtra[0]'s x, z and height sar 8.
static void SparkCentreFromExtra() {
    const U h = static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].height);
    const U x = static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].x);
    const U z = static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].z);
    SetUL(At(::bof3::Img(at::kSparkCentre.v)), x);
    SetUL(At(::bof3::Img(at::kSparkCentre.v) + 4), z);
    SetUL(At(::bof3::Img(at::kSparkCentre.v) + 8), Sar(h, 8));
}

// Sprite_Objects[1]'s point into a spark: x, z, height sar 8.
static void SparkAtObject1(unsigned char* r) {
    SetUL(r, static_cast<std::uint32_t>(Sprite_Objects[1].x));
    rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->z, static_cast<std::uint32_t>(Sprite_Objects[1].z));
    rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->height, Sar(static_cast<std::uint32_t>(Sprite_Objects[1].height), 8));
}

// original 0x471EA0: jmp [EffectKind2C_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind2C_Run(void) { Dispatch("EffectKind2C_Run", AddressOf(EffectKind2C_States), EffectKind2C_States_count); }

// original 0x471EC0 (state 0): the centre from Sprite_ObjectsExtra[0]; eight
// ground points 0x92D390 (0x10 each): x, z = the centre's + the dwords of
// 0x65436C / 0x65438C, the height AreaMap_Elevation(x, z) (its low word, signed,
// << 8); the sparks cleared; the dword +0xC = 0x200, +1 up.
extern "C" void __cdecl EffectKind2C_Start(void) {
    SparkCentreFromExtra();
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const pt = At(::bof3::Img(at::kSparkPoints.v) + 0x10 * i);
        SetUL(pt, UL(At(::bof3::Img(at::kSparkOffsetX.v) + 4 * i)) + UL(At(::bof3::Img(at::kSparkCentre.v))));
        const U z = UL(At(::bof3::Img(at::kSparkOffsetZ.v) + 4 * i)) + UL(At(::bof3::Img(at::kSparkCentre.v) + 4));
        SetUL(pt + 4, z);
        const long ground = SH_CALL(AreaMap_Elevation)(Long(pt), static_cast<long>(z));
        SetUL(pt + 8, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 8);
    }
    SH_CALL(EffectSparks_Clear)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x200);
    NextState();
}

// original 0x471F60 (state 1): when the low four bits of +0xC are 0, for each
// ground point a free spark (EffectSparks_FindFree; none: the next point): its
// +0..+0xC the centre's four dwords, its velocity +0x10..+0x18 = (the point -
// the centre) sar 4, normalised in place (Gte_VectorNormal), +0x24 (its shade)
// = 0x80, +0x20 (its size) = 0x1000, +0x25 (frames left) = 0x80. Then the centre
// from Sprite_ObjectsExtra[0] again, the sparks moved (EffectSparks_Move); +0xC
// down, at 0 +1 up.
extern "C" void __cdecl EffectKind2C_Emit(void) {
    if ((S()[0xC] & 0xF) == 0) {
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const r = SH_CALL(EffectSparks_FindFree)();
            if (!r) continue;
            const unsigned char* const c = At(::bof3::Img(at::kSparkCentre.v));
            const unsigned char* const pt = At(::bof3::Img(at::kSparkPoints.v) + 0x10 * i);
            for (unsigned k = 0; k < 4; ++k) SetUL(r + 4 * k, UL(c + 4 * k));
            for (unsigned k = 0; k < 3; ++k) SetUL(r + 0x10 + 4 * k, Sar(UL(pt + 4 * k) - UL(c + 4 * k), 4));
            SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(r + 0x10), reinterpret_cast<long*>(r + 0x10));
            r[0x24] = 0x80;
            rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->size, 0x1000);
            r[0x25] = 0x80;
        }
    }
    SparkCentreFromExtra();
    SH_CALL(EffectSparks_Move)();
    unsigned char* t = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(t)->work_0c, rv::As<records::EffectWork>(t)->work_0c - 1u);
    t = S();
    if (rv::As<records::EffectWork>(t)->work_0c == 0) t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x472060: jmp [EffectKind2E_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind2E_Run(void) { Dispatch("EffectKind2E_Run", AddressOf(EffectKind2E_States), EffectKind2E_States_count); }

// original 0x472080 (state 0): the sparks cleared, +1 up.
extern "C" void __cdecl EffectKind2E_Start(void) {
    SH_CALL(EffectSparks_Clear)();
    NextState();
}

// original 0x472090 (state 1): when the frame's low three bits are 0, a free
// spark at Sprite_Objects[1]'s point (x, z, height sar 8), still (velocity 0),
// shade 0x80, size 0x800, 0x80 frames; the sparks moved; when the counter byte
// 0x903848 is 3, +1 up.
extern "C" void __cdecl EffectKind2E_Trickle(void) {
    if ((Frame_Counter & 7u) == 0) {
        unsigned char* const r = SH_CALL(EffectSparks_FindFree)();
        if (r) {
            SparkAtObject1(r);
            rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->vx, 0);
            rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->vz, 0);
            rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->vheight, 0);
            r[0x24] = 0x80;
            rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->size, 0x800);
            r[0x25] = 0x80;
        }
    }
    SH_CALL(EffectSparks_Move)();
    if (At(::bof3::Img(at::kCounter0.v))[0] == 3) NextState();
}

// original 0x4720F0 (state 2): sixteen free sparks at Sprite_Objects[1]'s point
// (a spark not found skips its angle), each thrown flat at the angle 0x100 k:
// velocity (sin sar 2, cos sar 2, 0), shade 0x40, size 0x800, 0x40 frames; the
// sparks moved; the dword +0xC = 0x40. +1 is not moved: the state repeats.
extern "C" void __cdecl EffectKind2E_Burst(void) {
    for (unsigned k = 0; k < 16; ++k) {
        unsigned char* const r = SH_CALL(EffectSparks_FindFree)();
        if (!r) continue;
        SparkAtObject1(r);
        const U angle = (0x1000u * k / 16u) & 0xFFFFu;
        rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->vx, Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), 2));
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
        rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->vheight, 0);
        rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->vz, Sar(c, 2));
        r[0x24] = 0x40;
        rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->size, 0x800);
        r[0x25] = 0x40;
    }
    SH_CALL(EffectSparks_Move)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x40);
}

// original 0x4721A0 (0xA0 bytes): EffectGte_LoadMapCamera; each spark alive (+0x25
// not 0): drawn (EffectSparks_Draw), moved by its velocity (x, z, height in
// that order); AreaMap_Elevation(x, z) - above the height (its low word, signed),
// the height = it, the vertical velocity 0 and the velocity normalised in place;
// +0x24 down one, +0x20 up 4, +0x25 down one.
extern "C" void __cdecl EffectSparks_Move(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        unsigned char* const r = Pool(at::kSparkStride, i);
        if (r[0x25] == 0) continue;
        SH_CALL(EffectSparks_Draw)(r);
        const U x = UL(r) + static_cast<std::uint32_t>(rv::As<records::EffectSpark>(r)->vx);
        SetUL(r, x);
        const U z = static_cast<std::uint32_t>(rv::As<records::EffectSpark>(r)->z) + static_cast<std::uint32_t>(rv::As<records::EffectSpark>(r)->vz);
        rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->z, z);
        rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->height, static_cast<std::uint32_t>(rv::As<records::EffectSpark>(r)->height) + static_cast<std::uint32_t>(rv::As<records::EffectSpark>(r)->vheight));
        const long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        const std::int32_t g = static_cast<std::int16_t>(ground);
        if (g > rv::As<records::EffectSpark>(r)->height) {
            rv::Store<std::int32_t>(rv::As<records::EffectSpark>(r)->height, g);
            rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->vheight, 0);
            SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(r + 0x10), reinterpret_cast<long*>(r + 0x10));
        }
        const auto shade = static_cast<unsigned char>(r[0x24] - 1);
        const U size = rv::As<records::EffectSpark>(r)->size + 4u;
        const auto left = static_cast<unsigned char>(r[0x25] - 1);
        r[0x24] = shade;
        rv::Store<std::uint32_t>(rv::As<records::EffectSpark>(r)->size, size);
        r[0x25] = left;
    }
}

// original 0x472240 (0x198 bytes): a draw mode (page 0x2C0, 0x100, transparency
// 1, not dithered) linked at the spark's x, z (MapView_LinkPrimAt(x, z, 1, 0xC));
// a semi-transparent textured quad at the packet cursor: the half size
// EffectGte_ProjectSize(the point (x, z, height << 8), {+0x20 >> 6, -}) - its
// first word only (the original's second size word is stack it never wrote, and
// the second quotient is not read) -, the centre EffectGte_ProjectPoint(the
// point), the corners (sx -/+ half, sy -/+ half), all at the centre's depth; the
// CLUT Gpu_GetClut(0xA0, 0x1E3), the page Gpu_GetTPage(0, 1, 0x2C0, 0x100), the
// texture corners fixed, shaded +0x24; linked at x, z (1, 0x48).
extern "C" void __cdecl EffectSparks_Draw(const unsigned char* spark) {
    DrawMode(1, 0x2C0, 0x100, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(spark), static_cast<std::uint32_t>(rv::As<records::EffectSpark>(spark)->z), 1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    short size[2] = {static_cast<short>(rv::As<records::EffectSpark>(spark)->size >> 6), 0};
    const long point[3] = {static_cast<long>(UL(spark)), static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectSpark>(spark)->z)),
                           static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectSpark>(spark)->height) << 8)};
    short out[2] = {0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, size, out);
    float centre[3];
    SH_CALL(EffectGte_ProjectPoint)(point, centre);
    const double half = static_cast<double>(out[0]);
    const double cx = static_cast<double>(centre[0]), cy = static_cast<double>(centre[1]);
    SetD(p + 0x28, cx - half);
    SetD(p + 8, cx - half);
    SetD(p + 0x38, cx + half);
    SetD(p + 0x18, cx + half);
    SetD(p + 0x1C, cy - half);
    SetD(p + 0xC, cy - half);
    SetD(p + 0x3C, cy + half);
    SetD(p + 0x2C, cy + half);
    U depth;
    std::memcpy(&depth, &centre[2], sizeof depth);
    SetUL(p + 0x40, depth);
    SetUL(p + 0x30, depth);
    SetUL(p + 0x20, depth);
    SetUL(p + 0x10, depth);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    p[0x15] = 0x30;
    p[0x25] = 0x30;
    p[0x14] = 0xE0;
    p[0x24] = 0xFF;
    p[0x34] = 0xE0;
    p[0x35] = 0x4F;
    p[0x44] = 0xFF;
    p[0x45] = 0x4F;
    p[4] = spark[0x24];
    p[5] = spark[0x24];
    p[6] = spark[0x24];
    SH_CALL(MapView_LinkPrimAt)(UL(spark), static_cast<std::uint32_t>(rv::As<records::EffectSpark>(spark)->z), 1, 0x48);
}

// original 0x4723E0 (0x14 bytes): every spark's +0x25 = 0 (all free).
extern "C" void __cdecl EffectSparks_Clear(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i) Pool(at::kSparkStride, i)[0x25] = 0;
}

// original 0x472400 (0x1B bytes): the first spark whose +0x25 is 0, or 0 (eax).
extern "C" unsigned char* __cdecl EffectSparks_FindFree(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i)
        if (Pool(at::kSparkStride, i)[0x25] == 0) return Pool(at::kSparkStride, i);
    return nullptr;
}

void EffectKind2e_Inject() {
    BOF3_INJECT(EffectKind2C_Run);
    BOF3_INJECT(EffectKind2C_Start);
    BOF3_INJECT(EffectKind2C_Emit);
    BOF3_INJECT(EffectKind2E_Run);
    BOF3_INJECT(EffectKind2E_Start);
    BOF3_INJECT(EffectKind2E_Trickle);
    BOF3_INJECT(EffectKind2E_Burst);
    BOF3_INJECT(EffectSparks_Move);
    BOF3_INJECT(EffectSparks_Draw);
    BOF3_INJECT(EffectSparks_Clear);
    BOF3_INJECT(EffectSparks_FindFree);
}

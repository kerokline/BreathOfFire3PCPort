// Moved here by tools/move_funcs.py from effect_4c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_8e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

using Handler = scenario_harness::Handler;

using RecordFn = void (__cdecl*)(unsigned char*);

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

U Shards() { return AddressOf(EffectKind30_Shards); }

// --- x87 as the original has it (the game's control word: round to nearest, 53
// bits); every value goes through the FPU as Capcom's does (effect_3c.cpp's).
// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}

// `fld dword [a]; fst dword [o1]; fstp dword [o2]` (a copy through the FPU,
// which turns a signalling NaN quiet)
void FldCopy2(const void* a, void* o1, void* o2) {
    __asm__ volatile("flds (%0)\n\tfsts (%1)\n\tfstps (%2)" : : "r"(a), "r"(o1), "r"(o2) : "st", "memory");
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through
// the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4c.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void Step() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// The draw mode these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// The fall the two particle pools share: x, z on by their speeds, the height
// on by its speed (read before), the speed down 0x40000; below the floor
// 0xFE000000 the height held there and the new speed negated (a bounce).
// `p` is the point (x, z, height), `v` the three speeds.
void Fall(unsigned char* p, unsigned char* v) {
    const U vy = UL(v + 8);
    SetUL(p, UL(p) + UL(v));
    SetUL(p + 4, UL(p + 4) + UL(v + 4));
    const U y = UL(p + 8) + vy;
    SetUL(v + 8, vy + 0xFFFC0000u);
    SetUL(p + 8, y);
    if (S32(y) < S32(0xFE000000u)) {
        SetUL(p + 8, 0xFE000000u);
        SetUL(v + 8, 0u - UL(v + 8));
    }
}

// The particle set-up the two pools share: a coordinate in [lo, hi) from
// Rand - ((Rand & 0xFFF) << shift) % (hi - lo) + lo, the remainder of idiv
// (truncated, its sign the dividend's: never negative here); lo read before
// the call, hi after it, as the original reads them. hi - lo 0 is idiv's
// fault in the original; ours aborts.
U Spread(const char* who, U lo, const long* hi_at, unsigned shift) {
    const U r = static_cast<U>(SH_CALL(Rand)());
    const U hi = static_cast<U>(*hi_at);
    const std::int32_t d = S32(hi - lo);
    if (d == 0)
        bof3::Fatal("%s: the box's two corners are equal (hi - lo 0) - the original's idiv faults "
                    "(docs/effect_4c.md section 6)",
                    who);
    const std::int32_t n = static_cast<std::int32_t>((r & 0xFFFu) << shift);
    return static_cast<U>(n % d) + lo;
}

}  // namespace

// original 0x48B300 (Effect_KindHandlers[0x8E]; a start the cut does not list,
// between 0x48B2F0 and 0x48B320): jmp [EffectKind8E_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKind8E_Run(void) {
    Dispatch("EffectKind8E_Run", AddressOf(EffectKind8E_States), EffectKind8E_States_count);
}

// original 0x48B320 (state 0): both pools cleared; then three boxes, each
// set up in both pools over its slice of the 128 records: records 0..0x1F in
// x 0x20000..0x50000, 0x20..0x5F in 0x50000..0xA0000, 0x60..0x7F in
// 0xA0000..0xD0000, all at z 0x770000 and a height 0xFF000000..0x2000000 (the
// two corners two stack vectors, written again before each box). Sound 0x207,
// +2 = 0, +1 up.
extern "C" void __cdecl EffectKind8E_Start(void) {
    SH_CALL(EffectKind8E_ClearChips)();
    SH_CALL(EffectKind8E_ClearDots)();
    static const U kBoxes[3][4] = {
        {0, 0x20, 0x20000, 0x50000}, {0x20, 0x60, 0x50000, 0xA0000}, {0x60, 0x80, 0xA0000, 0xD0000}};
    long lo[3], hi[3];
    for (const U* b : kBoxes) {
        lo[0] = static_cast<long>(b[2]);
        lo[1] = 0x770000;
        lo[2] = static_cast<long>(0xFF000000u);
        hi[0] = static_cast<long>(b[3]);
        hi[1] = 0x770000;
        hi[2] = 0x2000000;
        SH_CALL(EffectKind8E_InitChips)(b[0], b[1], lo, hi);
        SH_CALL(EffectKind8E_InitDots)(b[0], b[1], lo, hi);
    }
    SH_CALL(Sound_PlayEffect)(0x207);
    S()[2] = 0;
    Step();
}

// original 0x48B450 (state 1): the first time (+2 0) Cond_ByteFE = 1,
// MoveCmd_TestFB(2, 0x77), +2 up; then EffectGte_LoadMapCamera and both pools
// moved and drawn; neither live: +1 up.
extern "C" void __cdecl EffectKind8E_Fall(void) {
    if (S()[2] == 0) {
        Cond_ByteFE = 1;
        SH_CALL(MoveCmd_TestFB)(2, 0x77);
        unsigned char* const s = S();
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned char chips = SH_CALL(EffectKind8E_MoveChips)();
    const unsigned char dots = SH_CALL(EffectKind8E_MoveDots)();
    if (chips == 0 && dots == 0) Step();
}

// original 0x48B4A0: the 128 chips (0x28 at EffectKind30_Shards): each live
// one falls (point +0x18, speeds +8; Fall), +3 down 2, its shape +4 = Rand &
// 7, its life +2 down (0 frees it: +0 = 0) and drawn (EffectKind8E_DrawChip).
// al 1 when any was live, else 0.
extern "C" unsigned char __cdecl EffectKind8E_MoveChips(void) {
    unsigned char any = 0;
    unsigned char* r = At(Shards());
    for (unsigned n = at::kChipCount; n != 0; --n, r += at::kChipStride) {
        if (r[0] == 0) continue;
        Fall(r + 0x18, r + 8);
        r[3] = static_cast<unsigned char>(r[3] + 0xFE);
        r[4] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 7u);
        r[2] = static_cast<unsigned char>(r[2] - 1);
        if (r[2] == 0) r[0] = 0;
        SH_CALL(EffectKind8E_DrawChip)(r);
        any = 1;
    }
    return any;
}

// original 0x48B530 (a chip): a draw mode (page (0x3C0, 0), dtd 0) linked at
// the chip's (+0x18, +0x1C) (MapView_LinkPrimAt(.., -2, 0xC)); a flat triangle
// (0x5A7570) at the cursor, semi-transparent; its first corner the chip's
// point projected, the other two that corner plus a row of kChipShapes by +4
// (fild of each s16, fadd of the projected float); the depth copied to all
// three through the FPU; colour (0, +3, +3); linked at (+0x18, +0x1C) with dy
// -2, 0x2C. E3C's EffectKind6D_DrawParticle but the first link's dy and the
// table. +4 past the eight rows: ours aborts.
extern "C" void __cdecl EffectKind8E_DrawChip(unsigned char* p) {
    DrawMode(1, 0x3C0, 0, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(p + 0x18), UL(p + 0x1C), -2, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_AT(RecordFn, at::kPolyF3.v)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(p + 0x18), reinterpret_cast<float*>(prim + 8));
    const unsigned k = p[4];
    if (k >= at::kChipShapeCount)
        bof3::Fatal("EffectKind8E_DrawChip: +4 is %u, past the eight shapes at 0x%X - the original reads what "
                    "follows (docs/effect_4c.md section 6)",
                    k, at::kChipShapes.v);
    const unsigned char* const row = At(::bof3::Img(at::kChipShapes.v) + 8 * k);
    FildAdd(SW(row + 0), prim + 8, prim + 0x14);
    FildAdd(SW(row + 2), prim + 0xC, prim + 0x18);
    FildAdd(SW(row + 4), prim + 8, prim + 0x20);
    prim[4] = 0;
    FildAdd(SW(row + 6), prim + 0xC, prim + 0x24);
    FldCopy2(prim + 0x10, prim + 0x28, prim + 0x1C);
    prim[5] = p[3];
    prim[6] = p[3];
    SH_CALL(MapView_LinkPrimAt)(UL(p + 0x18), UL(p + 0x1C), -2, 0x2C);
}

// original 0x48B640: +0 = 0 in the 128 chips.
extern "C" void __cdecl EffectKind8E_ClearChips(void) {
    unsigned char* r = At(Shards());
    for (unsigned n = at::kChipCount; n != 0; --n, r += at::kChipStride) r[0] = 0;
}

// original 0x48B660 (first, last - each read as its low word -, lo, hi: two
// points of three dwords): chips first..last - 1 (none when first >= last, an
// unsigned compare of the words) set up in the box: x (+0x18) and height
// (+0x20) Spread from Rand (<< 8 and << 16), z (+0x1C) lo's; +8 0, +0x10 0,
// +0 1, +1 0, +0xC (Rand & 0xFFF) * 4 + 0x1000 (its z speed), +3 0x48, +2
// (Rand & 7) + 0x18 (its life), +4 Rand & 7 (its shape). A last past the 128
// chips: ours aborts (the original writes on past the pool).
extern "C" void __cdecl EffectKind8E_InitChips(unsigned first, unsigned last, const long* lo, const long* hi) {
    const U f = first & 0xFFFFu, l = last & 0xFFFFu;
    if (f >= l) return;
    if (l > at::kChipCount)
        bof3::Fatal("EffectKind8E_InitChips: chips %u..%u, past the 128 at EffectKind30_Shards - the original writes "
                    "on past the pool (docs/effect_4c.md section 6)",
                    (unsigned)f, (unsigned)l);
    unsigned char* r = At(Shards() + at::kChipStride * f);
    for (U n = l - f; n != 0; --n, r += at::kChipStride) {
        const U lo0 = static_cast<U>(lo[0]);
        SetUL(r + 0x18, Spread("EffectKind8E_InitChips", lo0, &hi[0], 8));
        const U lo2 = static_cast<U>(lo[2]);
        SetUL(r + 0x20, Spread("EffectKind8E_InitChips", lo2, &hi[2], 16));
        SetUL(r + 0x1C, static_cast<U>(lo[1]));
        SetUL(r + 8, 0);
        const U speed = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
        SetUL(r + 0x10, 0);
        r[0] = 1;
        r[1] = 0;
        SetUL(r + 0xC, speed * 4 + 0x1000);
        const U life = static_cast<U>(SH_CALL(Rand)());
        r[3] = 0x48;
        r[2] = static_cast<unsigned char>((life & 7u) + 0x18);
        r[4] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 7u);
    }
}

// original 0x48B730: the 128 dots (0x24 at 0x92D380): each live one falls
// (point +0x14, speeds +4; Fall), +3 down 2, its life +2 down (0 frees it)
// and drawn (EffectKind8E_DrawDot). al 1 when any was live, else 0.
extern "C" unsigned char __cdecl EffectKind8E_MoveDots(void) {
    unsigned char any = 0;
    unsigned char* r = At(::bof3::Img(at::kDots.v));
    for (unsigned n = at::kDotCount; n != 0; --n, r += at::kDotStride) {
        if (r[0] == 0) continue;
        Fall(r + 0x14, r + 4);
        r[3] = static_cast<unsigned char>(r[3] + 0xFE);
        r[2] = static_cast<unsigned char>(r[2] - 1);
        if (r[2] == 0) r[0] = 0;
        SH_CALL(EffectKind8E_DrawDot)(r);
        any = 1;
    }
    return any;
}

// original 0x48B7C0 (a dot): a draw mode (page (0x3C0, 0), dtd 0) linked at
// the dot's (+0x14, +0x18) (dy -2, 0xC); a one-pixel tile (Gpu_SetTile1) at
// the cursor, semi-transparent, its point +0x14 projected to +8; colour (0,
// +3, +3); linked (+0x14, +0x18, -2, 0x14).
extern "C" void __cdecl EffectKind8E_DrawDot(unsigned char* p) {
    DrawMode(1, 0x3C0, 0, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(p + 0x14), UL(p + 0x18), -2, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(p + 0x14), reinterpret_cast<float*>(prim + 8));
    prim[4] = 0;
    prim[5] = p[3];
    prim[6] = p[3];
    SH_CALL(MapView_LinkPrimAt)(UL(p + 0x14), UL(p + 0x18), -2, 0x14);
}

// original 0x48B850: +0 = 0 in the 128 dots.
extern "C" void __cdecl EffectKind8E_ClearDots(void) {
    unsigned char* r = At(::bof3::Img(at::kDots.v));
    for (unsigned n = at::kDotCount; n != 0; --n, r += at::kDotStride) r[0] = 0;
}

// original 0x48B870 (first, last, lo, hi as EffectKind8E_InitChips): dots
// first..last - 1 set up in the box: x (+0x14) and height (+0x1C) Spread, z
// (+0x18) lo's; +4 0, +0xC 0, +0 1, +1 0, +8 (Rand & 0xFFF) * 4 + 0x2000, +3
// 0x48, +2 (Rand & 7) + 0x18. A last past the 128 dots: ours aborts.
extern "C" void __cdecl EffectKind8E_InitDots(unsigned first, unsigned last, const long* lo, const long* hi) {
    const U f = first & 0xFFFFu, l = last & 0xFFFFu;
    if (f >= l) return;
    if (l > at::kDotCount)
        bof3::Fatal("EffectKind8E_InitDots: dots %u..%u, past the 128 at 0x92D380 - the original writes on past the "
                    "pool (docs/effect_4c.md section 6)",
                    (unsigned)f, (unsigned)l);
    unsigned char* r = At(::bof3::Img(at::kDots.v) + at::kDotStride * f);
    for (U n = l - f; n != 0; --n, r += at::kDotStride) {
        const U lo0 = static_cast<U>(lo[0]);
        SetUL(r + 0x14, Spread("EffectKind8E_InitDots", lo0, &hi[0], 8));
        const U lo2 = static_cast<U>(lo[2]);
        SetUL(r + 0x1C, Spread("EffectKind8E_InitDots", lo2, &hi[2], 16));
        SetUL(r + 0x18, static_cast<U>(lo[1]));
        SetUL(r + 4, 0);
        const U speed = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
        SetUL(r + 0xC, 0);
        r[0] = 1;
        r[1] = 0;
        SetUL(r + 8, speed * 4 + 0x2000);
        const U life = static_cast<U>(SH_CALL(Rand)());
        r[3] = 0x48;
        r[2] = static_cast<unsigned char>((life & 7u) + 0x18);
    }
}

void EffectKind8e_Inject() {
    BOF3_INJECT(EffectKind8E_Run);
    BOF3_INJECT(EffectKind8E_Start);
    BOF3_INJECT(EffectKind8E_Fall);
    BOF3_INJECT(EffectKind8E_MoveChips);
    BOF3_INJECT(EffectKind8E_DrawChip);
    BOF3_INJECT(EffectKind8E_ClearChips);
    BOF3_INJECT(EffectKind8E_InitChips);
    BOF3_INJECT(EffectKind8E_MoveDots);
    BOF3_INJECT(EffectKind8E_DrawDot);
    BOF3_INJECT(EffectKind8E_ClearDots);
    BOF3_INJECT(EffectKind8E_InitDots);
}

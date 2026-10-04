// Round thirteen group E4C (docs/effect_4c.md): the 49 functions of
// analysis/round13_cut.tsv's group E4C, 0x48B200..0x48C985, and one start the
// cut does not list (kind 0x8E's dispatcher 0x48B300, Effect_KindHandlers
// [0x8E], hidden between 0x48B2F0 and 0x48B320), each read with capstone to its
// last instruction (2026-10-03). Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x8D   a camera pull: Camera_Distance set to 0x1C80 and the three
//               party records' byte +0x48 set; a full-screen tint (E4D's
//               0x48CA90) of +0x5D..+0x5F held at 0x7F while +1 is 1 (nothing
//               here moves it on); at +1 = 2 the distance drawn in by 0x2A a
//               frame for +9 frames as the tint fades, then 0x400; +1 = 3
//               steps the byte after MoveScript_Var7 and releases the record
//   kind 0x8E   two pools of 128 falling particles (flat triangles of 0x28
//               from EffectKind30_Shards, single-pixel tiles of 0x24 from
//               0x92D380) set up in three boxes and thrown, bouncing off a
//               floor, until all are spent
//   kind 0x8F   sixteen shards (0x28 at EffectKind30_Shards) spawned at fixed
//               offsets from two .data tables - three, then two every fifteen
//               frames, then in five stages by a frame word - each brightening,
//               then fading and drifting, drawn as a sized textured quad on the
//               ground's height
//   kind 0x90   thirty-two shards (0x28) emitted one every fourth frame at
//               the record's point, each tagged with the record's +6 (a
//               counter 0x676294 hands out) so several records share the pool
//   kind 0x93   kind 0x74's column (E3C's EffectKind74_*) at a lighter grey,
//               the shard pool cleared first, sound 0x208
//   kind 0x99   a disc on the ground one unit back from the record's point,
//               sixteen triangles in four fans, its radius growing for ten
//               frames, then sound 0x20F
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes a table or pool of its own past its rows, reads
// its own frame past a local array or divides by zero, ours aborts with a
// message (docs/effect_4c.md section 6).
#include "game/effect_4c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;
using RecordFn = void (__cdecl*)(unsigned char*);
using VoidFn = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Shards() { return AddressOf(EffectKind30_Shards); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul of two whole registers: the low 32 bits.
U Mul(U a, U b) { return a * b; }
// movsx eax, ax; shl eax, 0x10: a height word into the high half.
U High(long h) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(h))) << 16; }

// --- x87 as the original has it (the game's control word: round to nearest, 53
// bits); every value goes through the FPU as Capcom's does (effect_3c.cpp's).
// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}
// `fild dword [v]; fsubr dword [c]; fstp dword [o]`: c - v
void FildSubr(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}
// `fild dword [v]; fsubr dword [c]; fiadd dword [w]; fstp dword [o]`: c - v + w
void FildSubrAdd(std::int32_t v, const void* c, std::int32_t w, void* o) {
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfiaddl %3\n\tfstps (%0)"
                     :
                     : "r"(o), "r"(c), "m"(v), "m"(w)
                     : "st", "memory");
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
// +9 down one; answers whether it is now 0 (Sprite_Current read for each access).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
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

// A shard's sized quad (kinds 0x8F and 0x90): a POLY_FT4 at the cursor,
// semi-transparent; its centre the shard's point +8 projected, its size +4
// (both axes) scaled at its depth in place (EffectGte_ProjectSize, size and
// out one local); the corners x - (w >> 1) and + w, y - (h >> 1) and + h on
// the x87 (fild, fsubr, fiadd); the depth to +0x10..+0x40 by mov; u 0xE0 /
// 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0, 0x1E3), the page
// Gpu_GetTPage(0, 1, 0x2C0, 0x100). Answers the primitive; the caller colours
// and links or commits it.
unsigned char* ShardQuad(unsigned char* r) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const long* const point = reinterpret_cast<const long*>(r + 8);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    short wh[2] = {static_cast<short>(Word(r + 4)), static_cast<short>(Word(r + 4))};
    SH_CALL(EffectGte_ProjectSize)(point, wh, wh);
    const std::int32_t w = wh[0], h = wh[1];
    const std::int32_t hw = static_cast<short>(wh[0] >> 1), hh = static_cast<short>(wh[1] >> 1);
    FildSubr(hw, &screen[0], prim + 8);
    FildSubr(hh, &screen[1], prim + 0xC);
    FildSubrAdd(hw, &screen[0], w, prim + 0x18);
    FildSubr(hh, &screen[1], prim + 0x1C);
    FildSubr(hw, &screen[0], prim + 0x28);
    FildSubrAdd(hh, &screen[1], h, prim + 0x2C);
    FildSubrAdd(hw, &screen[0], w, prim + 0x38);
    FildSubrAdd(hh, &screen[1], h, prim + 0x3C);
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    U depth;
    std::memcpy(&depth, &screen[2], 4);
    SetUL(prim + 0x40, depth);
    SetUL(prim + 0x30, depth);
    SetUL(prim + 0x20, depth);
    SetUL(prim + 0x10, depth);
    SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    return prim;
}

// The column kinds 0x74 (E3C's EffectKind74_Draw) and 0x93 draw, `grey` its
// colour: EffectGte_LoadMapCamera; from the foot's height (+0x14) up to the
// top (+0x1C) + 0x1000000 in steps of 0x100000, its edge a point at the angle
// (+0x20, up 0x100 a step) 32 * (Math_Cos, Math_Sin) round the foot (+0xC,
// +0x10 - read once, at entry); each step's two heights (the step less the
// width +0x24, no lower than the foot; the step, no higher than the top -
// +0x14, +0x24, +0x1C read afresh) projected, and a POLY_FT4 (opaque, u 0x1F
// / 0, v 0xFF / 0xE0, the CLUT Gpu_GetClut(0, 0x1E5), the page
// Gpu_GetTPage(1, 0, 0x1C0, 0x100)) from the previous step's two projections
// to this one's, linked at the edge point (dy 2, 0x48).
void Column(unsigned char grey) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    U angle = Word(s + 0x20);
    const U x = UL(s + 0xC), z = UL(s + 0x10);
    U y = UL(s + 0x14);
    long pt[3];
    float low[3], high[3];
    auto project = [&](U a) {
        pt[0] = static_cast<long>((static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))) << 5) + x);
        pt[1] = static_cast<long>((static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))) << 5) + z);
        unsigned char* c = S();
        U h = y - UL(c + 0x24);
        if (S32(h) < S32(UL(c + 0x14))) h = UL(c + 0x14);
        pt[2] = static_cast<long>(h);
        SH_CALL(EffectGte_ProjectPoint)(pt, low);
        c = S();
        h = y;
        if (S32(y) > S32(UL(c + 0x1C))) h = UL(c + 0x1C);
        pt[2] = static_cast<long>(h);
        SH_CALL(EffectGte_ProjectPoint)(pt, high);
    };
    project(angle);
    s = S();
    if (S32(y) >= S32(UL(s + 0x1C) + 0x1000000u)) return;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        angle += 0x100;
        y += 0x100000;
        std::memcpy(prim + 0x08, low, 12);
        std::memcpy(prim + 0x18, high, 12);
        project(angle & 0xFFFFu);
        std::memcpy(prim + 0x28, low, 12);
        std::memcpy(prim + 0x38, high, 12);
        prim[0x14] = 0x1F;
        prim[0x15] = 0xFF;
        prim[0x24] = 0x1F;
        prim[0x25] = 0xE0;
        prim[0x34] = 0;
        prim[0x35] = 0xFF;
        prim[0x44] = 0;
        prim[0x45] = 0xE0;
        prim[4] = prim[5] = prim[6] = grey;
        SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(1, 0, 0x1C0, 0x100));
        SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0, 0x1E5));
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(pt[0]), static_cast<unsigned long>(pt[1]), 2, 0x48);
        s = S();
    } while (S32(y) < S32(UL(s + 0x1C) + 0x1000000u));
}

}  // namespace

// ===========================================================================
// Kind 0x8D: Effect_KindHandlers[0x8D] (0x655584), EffectKind8D_States (four)
// ===========================================================================

// original 0x48B200 (Effect_KindHandlers[0x8D], hidden in E4B's 0x48A8E0):
// jmp [EffectKind8D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8D_Run(void) {
    Dispatch("EffectKind8D_Run", AddressOf(EffectKind8D_States), EffectKind8D_States_count);
}

// original 0x48B220 (state 0): Camera_Distance 0x1C80, MapView_Redraw 2, the
// byte +0x48 of the three party records (ObjTrio) 1; the tint +0x5F / +0x5E
// / +0x5D 0x7F (+0x5F through the record read first); +9 = 0x96, +1 = 1.
extern "C" void __cdecl EffectKind8D_Start(void) {
    unsigned char* const s = S();
    Camera_Distance = 0x1C80;
    MapView_Redraw = 2;
    for (unsigned i = 0; i < 3; ++i) At(at::kLeaderLift + at::kObjStride * i)[0] = 1;
    s[0x5F] = 0x7F;
    S()[0x5E] = 0x7F;
    S()[0x5D] = 0x7F;
    S()[9] = 0x96;
    S()[1] = 1;
}

// original 0x48B280 (state 1): a tail jump to E4D's screen tint 0x48CA90 (the
// tint held; nothing here moves +1 on).
extern "C" void __cdecl EffectKind8D_Hold(void) { SH_AT(VoidFn, at::kScreenTile)(); }

// original 0x48B290 (state 2): +9 down; not 0: Camera_Distance - 0x2A and,
// while +0x5D is not 0, the tint +0x5D / +0x5E / +0x5F each down one (+0x5D
// through the record read after the count); at 0: Camera_Distance 0x400, +1 =
// 3. Then MapView_Redraw 2 and a tail jump to the tint.
extern "C" void __cdecl EffectKind8D_Pull(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) {
        Camera_Distance = static_cast<short>(Camera_Distance - 0x2A);
        const unsigned char red = s[0x5D];
        if (red != 0) {
            s[0x5D] = static_cast<unsigned char>(red - 1);
            unsigned char* t = S();
            t[0x5E] = static_cast<unsigned char>(t[0x5E] - 1);
            t = S();
            t[0x5F] = static_cast<unsigned char>(t[0x5F] - 1);
        }
    } else {
        Camera_Distance = 0x400;
        s[1] = 3;
    }
    MapView_Redraw = 2;
    SH_AT(VoidFn, at::kScreenTile)();
}

// original 0x48B2F0 (state 3): the byte after MoveScript_Var7 (0x8034E5) up;
// a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind8D_End(void) {
    unsigned char* const step = At(at::kVar7Step);
    step[0] = static_cast<unsigned char>(step[0] + 1);
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x8E: Effect_KindHandlers[0x8E] (0x655588), EffectKind8E_States (three)
// ===========================================================================

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
    SH_AT(RecordFn, at::kPolyF3)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(p + 0x18), reinterpret_cast<float*>(prim + 8));
    const unsigned k = p[4];
    if (k >= at::kChipShapeCount)
        bof3::Fatal("EffectKind8E_DrawChip: +4 is %u, past the eight shapes at 0x%X - the original reads what "
                    "follows (docs/effect_4c.md section 6)",
                    k, (unsigned)at::kChipShapes);
    const unsigned char* const row = At(at::kChipShapes + 8 * k);
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
    unsigned char* r = At(at::kDots);
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
    unsigned char* r = At(at::kDots);
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
    unsigned char* r = At(at::kDots + at::kDotStride * f);
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

// ===========================================================================
// Kind 0x8F: Effect_KindHandlers[0x8F] (0x65558C), EffectKind8F_States (nine)
// ===========================================================================

// original 0x48B930 (Effect_KindHandlers[0x8F], hidden in 0x48B870):
// jmp [EffectKind8F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8F_Run(void) {
    Dispatch("EffectKind8F_Run", AddressOf(EffectKind8F_States), EffectKind8F_States_count);
}

// original 0x48B950 (state 0): the sixteen shards cleared, +1 up.
extern "C" void __cdecl EffectKind8F_Start(void) {
    SH_CALL(EffectKind8F_ClearShards)();
    Step();
}

// original 0x48B960 (state 1): shards 6, 9 and 0xD spawned; +9 = 0x3C, +1 up.
extern "C" void __cdecl EffectKind8F_Burst(void) {
    SH_CALL(EffectKind8F_SpawnShard)(6);
    SH_CALL(EffectKind8F_SpawnShard)(9);
    SH_CALL(EffectKind8F_SpawnShard)(0xD);
    S()[9] = 0x3C;
    Step();
}

// original 0x48B990 (state 2): the shards moved; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind8F_Rise(void) {
    SH_CALL(EffectKind8F_MoveShards)();
    if (CountDown()) Step();
}

// original 0x48B9C0 (state 3): +9 = 0x96, +1 up.
extern "C" void __cdecl EffectKind8F_Arm(void) {
    S()[9] = 0x96;
    Step();
}

// original 0x48B9E0 (state 4): every fifteenth count (+9 % 15 0) two shards
// spawned in a shuffled order; the shards moved; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind8F_Trickle(void) {
    if (S()[9] % 15u == 0) SH_CALL(EffectKind8F_SpawnShuffled)(2);
    SH_CALL(EffectKind8F_MoveShards)();
    if (CountDown()) Step();
}

// original 0x48BA30 (state 5): the shards moved; none live: +1 up.
extern "C" void __cdecl EffectKind8F_Settle(void) {
    if (SH_CALL(EffectKind8F_MoveShards)() == 0) Step();
}

// original 0x48BA50 (state 6): the frame word +0x2E and the wait word +0x30 0
// (each through Sprite_Current read again), +1 up.
extern "C" void __cdecl EffectKind8F_ResetClock(void) {
    SetWord(S() + 0x2E, 0);
    SetWord(S() + 0x30, 0);
    Step();
}

// original 0x48BA70 (state 7): the frame word +0x2E (s16; the record read
// once, at entry) at 0x1A4 or more: +1 up. Below: the stage byte 0x676290 by
// it (below 0x3C 0, 0xB4 1, 0x12C 2, 0x168 3, else 4); the wait word +0x30 0:
// as many shards as kStageCounts[stage] spawned shuffled and the wait
// kStageWaits[stage] (the stage read back from its cell each time), else the
// wait down one; the frame word up (Sprite_Current read again). Then a tail
// jump to the shards' move.
extern "C" void __cdecl EffectKind8F_Sequence(void) {
    unsigned char* const s = S();
    const std::int32_t frame = SW(s + 0x2E);
    if (frame >= 0x1A4) {
        s[1] = static_cast<unsigned char>(s[1] + 1);
    } else {
        unsigned char stage;
        if (frame < 0x3C)
            stage = 0;
        else if (frame < 0xB4)
            stage = 1;
        else if (frame < 0x12C)
            stage = 2;
        else
            stage = frame >= 0x168 ? 4 : 3;
        At(at::kStage)[0] = stage;
        const unsigned wait = Word(s + 0x30);
        if (wait == 0) {
            const unsigned a = At(at::kStage)[0];
            if (a >= at::kStageCount)
                bof3::Fatal("EffectKind8F_Sequence: the stage byte 0x%X is %u, past the five rows of 0x%X - the "
                            "original reads on (docs/effect_4c.md section 6)",
                            (unsigned)at::kStage, a, (unsigned)at::kStageCounts);
            SH_CALL(EffectKind8F_SpawnShuffled)(At(at::kStageCounts + a)[0]);
            const unsigned b = At(at::kStage)[0];
            if (b >= at::kStageCount)
                bof3::Fatal("EffectKind8F_Sequence: the stage byte 0x%X is %u, past the five rows of 0x%X - the "
                            "original reads on (docs/effect_4c.md section 6)",
                            (unsigned)at::kStage, b, (unsigned)at::kStageWaits);
            SetWord(S() + 0x30, At(at::kStageWaits + b)[0]);
        } else {
            SetWord(s + 0x30, wait - 1);
        }
        unsigned char* const t = S();
        SetWord(t + 0x2E, Word(t + 0x2E) + 1u);
    }
    SH_CALL(EffectKind8F_MoveShards)();
}

// original 0x48BB30 (state 8): the shards moved; none live: a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind8F_Fade(void) {
    if (SH_CALL(EffectKind8F_MoveShards)() == 0) SH_CALL(Effect_Release)();
}

// original 0x48BB40 (i, a byte): a free shard (E3C's EffectKind6E_FindShard);
// none: nothing. Else +0 1, +1 0, +2 0, +3 0x20, +4 (s16) 0x80, its x +8 and
// z +0xC kShardStarts' row i << 8, +0x10 0, its speeds +0x18 / +0x1C
// kShardSpeeds' row i << 5, +0x20 0. i past the sixteen rows: ours aborts.
extern "C" void __cdecl EffectKind8F_SpawnShard(unsigned i) {
    unsigned char* const r = SH_CALL(EffectKind6E_FindShard)();
    if (r == nullptr) return;
    const unsigned k = i & 0xFFu;
    if (k >= at::kShardRowCount)
        bof3::Fatal("EffectKind8F_SpawnShard: row %u, past the sixteen of 0x%X - the original reads what follows "
                    "(docs/effect_4c.md section 6)",
                    k, (unsigned)at::kShardStarts);
    r[0] = 1;
    r[1] = 0;
    r[2] = 0;
    r[3] = 0x20;
    SetWord(r + 4, 0x80);
    SetUL(r + 8, static_cast<U>(SW(At(at::kShardStarts + 4 * k))) << 8);
    SetUL(r + 0xC, static_cast<U>(SW(At(at::kShardStarts + 4 * k + 2))) << 8);
    SetUL(r + 0x10, 0);
    SetUL(r + 0x18, static_cast<U>(SW(At(at::kShardSpeeds + 4 * k))) << 5);
    SetUL(r + 0x1C, static_cast<U>(SW(At(at::kShardSpeeds + 4 * k + 2))) << 5);
    SetUL(r + 0x20, 0);
}

// original 0x48BBB0 (n, a byte): the rows 0..15 in a local, shuffled by
// sixteen swaps of two Rand & 0xF; the first n spawned (EffectKind8F_SpawnShard)
// in that order. n past sixteen: ours aborts (the original reads its own
// frame past the local).
extern "C" void __cdecl EffectKind8F_SpawnShuffled(unsigned n) {
    unsigned char order[16];
    for (unsigned i = 0; i < 16; ++i) order[i] = static_cast<unsigned char>(i);
    for (unsigned k = 16; k != 0; --k) {
        const unsigned a = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
        const unsigned b = static_cast<U>(SH_CALL(Rand)()) & 0xFu;
        const unsigned char x = order[a];
        order[a] = order[b];
        order[b] = x;
    }
    const unsigned count = n & 0xFFu;
    if (count == 0) return;
    if (count > 16)
        bof3::Fatal("EffectKind8F_SpawnShuffled: %u shards, past the sixteen of its local order - the original reads "
                    "its own frame (docs/effect_4c.md section 6)",
                    count);
    for (unsigned i = 0; i < count; ++i) SH_CALL(EffectKind8F_SpawnShard)(order[i]);
}

// original 0x48BC40: +0 = 0 in the sixteen shards.
extern "C" void __cdecl EffectKind8F_ClearShards(void) {
    unsigned char* r = At(Shards());
    for (unsigned n = at::kShard8FCount; n != 0; --n, r += at::kShardStride) r[0] = 0;
}

// original 0x48BC60: EffectGte_LoadMapCamera; each live shard of the sixteen
// stepped by its phase +1: 0 - +3 up 4, +2 up, at 0x10 the phase 1; 1 - +3
// down 4, +2 up, at 0x20 freed (+0 = 0); then x +8 += +0x18, z +0xC += +0x1C,
// its size +4 (s16) up 8, its height +0x10 AreaMap_Elevation(x, z) << 16, and
// drawn (EffectKind8F_DrawShard). al 1 when any was live.
extern "C" unsigned char __cdecl EffectKind8F_MoveShards(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    unsigned char* r = At(Shards());
    for (unsigned n = at::kShard8FCount; n != 0; --n, r += at::kShardStride) {
        if (r[0] == 0) continue;
        any = 1;
        if (r[1] == 0) {
            r[3] = static_cast<unsigned char>(r[3] + 4);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x10) r[1] = static_cast<unsigned char>(r[1] + 1);
        } else if (r[1] == 1) {
            r[3] = static_cast<unsigned char>(r[3] + 0xFC);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x20) r[0] = 0;
        }
        const U x = UL(r + 8) + UL(r + 0x18);
        SetUL(r + 8, x);
        SetWord(r + 4, Word(r + 4) + 8u);
        const U z = UL(r + 0xC) + UL(r + 0x1C);
        SetUL(r + 0xC, z);
        const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        SetUL(r + 0x10, High(h));
        SH_CALL(EffectKind8F_DrawShard)(r);
    }
    return any;
}

// original 0x48BD10 (a shard): its sized quad (ShardQuad); colour (+3, (+3 >>
// 2) * 3, +3 >> 1); linked at (+8, +0xC) (dy 3, 0x48).
extern "C" void __cdecl EffectKind8F_DrawShard(unsigned char* r) {
    unsigned char* const prim = ShardQuad(r);
    prim[4] = r[3];
    prim[5] = static_cast<unsigned char>((r[3] >> 2) * 3);
    prim[6] = static_cast<unsigned char>(r[3] >> 1);
    SH_CALL(MapView_LinkPrimAt)(UL(r + 8), UL(r + 0xC), 3, 0x48);
}

// ===========================================================================
// Kind 0x90: Effect_KindHandlers[0x90] (0x655590), EffectKind90_States (three)
// ===========================================================================

// original 0x48BF00 (Effect_KindHandlers[0x90], hidden in 0x48BD10):
// jmp [EffectKind90_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind90_Run(void) {
    Dispatch("EffectKind90_Run", AddressOf(EffectKind90_States), EffectKind90_States_count);
}

// original 0x48BF20 (state 0): the tag 0x676294 0 (no kind-0x90 record yet):
// the 32 shards cleared. +9 = 0x80; the tag up, its low byte the record's +6;
// +1 up.
extern "C" void __cdecl EffectKind90_Start(void) {
    if (Long(At(at::kTag)) == 0) SH_CALL(EffectKind90_ClearShards)();
    S()[9] = 0x80;
    const U tag = static_cast<U>(Long(At(at::kTag))) + 1;
    unsigned char* const s = S();
    SetUL(At(at::kTag), tag);
    s[6] = static_cast<unsigned char>(tag);
    Step();
}

// original 0x48BF60 (state 1): every fourth count (+9 & 3 0) a shard emitted
// (EffectKind90_EmitOne); +9 down, at 0 +1 up; a tail jump to the shards'
// move.
extern "C" void __cdecl EffectKind90_Emit(void) {
    unsigned char* s = S();
    if ((s[9] & 3u) == 0) {
        SH_CALL(EffectKind90_EmitOne)();
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(EffectKind90_MoveShards)();
}

// original 0x48BFA0 (state 2): the shards moved; none of this record's live:
// a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind90_Fade(void) {
    if (SH_CALL(EffectKind90_MoveShards)() == 0) SH_CALL(Effect_Release)();
}

// original 0x48C1C0: a free shard (EffectKind90_FindShard); none: nothing.
// Else +1 0, +0 the record's +6 (its tag), +2 0, +3 0x80, +4 (s16) 0x80, x +8
// the record's +0x34, +0x10 0, z +0xC the record's +0x38 (Sprite_Current read
// for each), its rise +0x18 0x1800, +0x1C 0, +0x20 0.
extern "C" void __cdecl EffectKind90_EmitOne(void) {
    unsigned char* const r = SH_CALL(EffectKind90_FindShard)();
    if (r == nullptr) return;
    const unsigned char tag = S()[6];
    r[1] = 0;
    r[0] = tag;
    r[2] = 0;
    r[3] = 0x80;
    SetWord(r + 4, 0x80);
    SetUL(r + 8, UL(S() + 0x34));
    const U z = UL(S() + 0x38);
    SetUL(r + 0x10, 0);
    SetUL(r + 0xC, z);
    SetUL(r + 0x18, 0x1800);
    SetUL(r + 0x1C, 0);
    SetUL(r + 0x20, 0);
}

// original 0x48C220: the first of the 32 shards of 0x28 at EffectKind30_Shards
// whose +0 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind90_FindShard(void) {
    unsigned char* r = At(Shards());
    for (unsigned i = 0; i < at::kShard90Count; ++i, r += at::kShardStride)
        if (r[0] == 0) return r;
    return nullptr;
}

// original 0x48C240: +0 = 0 in the 32 shards (kinds 0x90 and 0x93).
extern "C" void __cdecl EffectKind90_ClearShards(void) {
    unsigned char* r = At(Shards());
    for (unsigned n = at::kShard90Count; n != 0; --n, r += at::kShardStride) r[0] = 0;
}

// original 0x48C260: EffectGte_LoadMapCamera; each of the 32 shards whose +0
// is Sprite_Current's +6 (read again for each - a free shard's 0 matches a tag
// of 0) stepped by its phase +1 through a bounded switch (0x48C354, four
// cases; above 3 none): 0 - its size +4 (s16) up 4, +2 up, at 8 the phase 1;
// 1 - +3 down 8, +2 up, at 0x10 the phase 2; 2 - the size down 0x10, +2 up,
// at 0x18 the phase 3; 3 - the size down 0x10, +2 up, at 0x20 freed (+0 = 0).
// Then x +8 += +0x18, z +0xC += +0x1C, the size up 8, its height +0x10
// AreaMap_Elevation(x, z) << 16, and drawn (EffectKind90_DrawShard). al 1
// when any matched.
extern "C" unsigned char __cdecl EffectKind90_MoveShards(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    unsigned char* r = At(Shards());
    for (unsigned n = at::kShard90Count; n != 0; --n, r += at::kShardStride) {
        if (r[0] != S()[6]) continue;
        any = 1;
        switch (r[1]) {
        case 0:
            SetWord(r + 4, Word(r + 4) + 4u);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 8) r[1] = static_cast<unsigned char>(r[1] + 1);
            break;
        case 1:
            r[3] = static_cast<unsigned char>(r[3] + 0xF8);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x10) r[1] = static_cast<unsigned char>(r[1] + 1);
            break;
        case 2:
            SetWord(r + 4, Word(r + 4) + 0xFFF0u);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x18) r[1] = static_cast<unsigned char>(r[1] + 1);
            break;
        case 3:
            SetWord(r + 4, Word(r + 4) + 0xFFF0u);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            if (r[2] == 0x20) r[0] = 0;
            break;
        default: break;
        }
        const U x = UL(r + 8) + UL(r + 0x18);
        SetWord(r + 4, Word(r + 4) + 8u);
        const U z = UL(r + 0xC) + UL(r + 0x1C);
        SetUL(r + 8, x);
        SetUL(r + 0xC, z);
        const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        SetUL(r + 0x10, High(h));
        SH_CALL(EffectKind90_DrawShard)(r);
    }
    return any;
}

// original 0x48C370 (a shard): its sized quad (ShardQuad); colour +3 in all
// three channels; committed (Gfx_CommitPrim(1, 0x48)), not linked.
extern "C" void __cdecl EffectKind90_DrawShard(unsigned char* r) {
    unsigned char* const prim = ShardQuad(r);
    const unsigned char c = r[3];
    prim[6] = c;
    prim[5] = c;
    prim[4] = c;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// ===========================================================================
// Kind 0x93: Effect_KindHandlers[0x93] (0x65559C), EffectKind93_States (four)
// ===========================================================================

// original 0x48BFB0 (Effect_KindHandlers[0x93], hidden in 0x48BD10):
// jmp [EffectKind93_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind93_Run(void) {
    Dispatch("EffectKind93_Run", AddressOf(EffectKind93_States), EffectKind93_States_count);
}

// original 0x48BFD0 (state 0; E3C's EffectKind74_Start but the pool cleared
// first and the sound): the 32 shards cleared; the column's foot +0xC / +0x10
// / +0x14 the record's point +0x34 / +0x38 / +0x3C, its top +0x1C the foot,
// its angle +0x20 (s16) 0, its width +0x24 0x1000000; +9 = 0x40, +1 up, sound
// 0x208.
extern "C" void __cdecl EffectKind93_Start(void) {
    SH_CALL(EffectKind90_ClearShards)();
    unsigned char* s = S();
    SetUL(s + 0xC, UL(s + 0x34));
    s = S();
    SetUL(s + 0x10, UL(s + 0x38));
    s = S();
    SetUL(s + 0x14, UL(s + 0x3C));
    s = S();
    SetUL(s + 0x1C, UL(s + 0x14));
    SetWord(S() + 0x20, 0);
    SetUL(S() + 0x24, 0x1000000);
    S()[9] = 0x40;
    Step();
    SH_CALL(Sound_PlayEffect)(0x208);
}

// original 0x48C040 (state 1; EffectKind74_Rise's twin): the top +0x1C up
// 0x1000000, no higher than the foot + 0x8000000; the angle +0x20 turned by
// 0xFE00; drawn (EffectKind93_Draw); +9 down, at 0 +9 = 0x10 and +1 up.
extern "C" void __cdecl EffectKind93_Rise(void) {
    unsigned char* s = S();
    SetUL(s + 0x1C, UL(s + 0x1C) + 0x1000000u);
    s = S();
    const U cap = UL(s + 0x14) + 0x8000000u;
    if (S32(UL(s + 0x1C)) > S32(cap)) {
        SetUL(s + 0x1C, cap);
        s = S();
    }
    SetWord(s + 0x20, Word(s + 0x20) + 0xFE00u);
    SH_CALL(EffectKind93_Draw)();
    if (CountDown()) {
        S()[9] = 0x10;
        Step();
    }
}

// original 0x48C0A0 (state 2; EffectKind74_Fade's twin): +9 down; at 0 a tail
// jump to Effect_Release; else the angle turned by 0xFE00, the width +0x24
// down 0x100000 and a tail jump to EffectKind93_Draw.
extern "C" void __cdecl EffectKind93_Fade(void) {
    if (CountDown()) {
        SH_CALL(Effect_Release)();
        return;
    }
    unsigned char* s = S();
    SetWord(s + 0x20, Word(s + 0x20) + 0xFE00u);
    s = S();
    SetUL(s + 0x24, UL(s + 0x24) + 0xFFF00000u);
    SH_CALL(EffectKind93_Draw)();
}

// original 0x48C550: E3C's EffectKind74_Draw at grey 0xC0 (that one 0x80):
// the column of Column above.
extern "C" void __cdecl EffectKind93_Draw(void) { Column(0xC0); }

// ===========================================================================
// Kind 0x99: Effect_KindHandlers[0x99] (0x6555B4), EffectKind99_States (four)
// ===========================================================================

// original 0x48C0E0 (Effect_KindHandlers[0x99], hidden in 0x48BD10):
// jmp [EffectKind99_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind99_Run(void) {
    Dispatch("EffectKind99_Run", AddressOf(EffectKind99_States), EffectKind99_States_count);
}

// original 0x48C100 (state 0): the disc's centre +0xC = the record's +0x34 -
// 0x10000, +0x10 = +0x38 - 0x10000 (Sprite_Current read again; both written
// through the record read first), its height +0x14 AreaMap_Elevation(+0xC,
// +0x10) << 16, its radius +0x1C 0; +9 = 0xA, +1 up.
extern "C" void __cdecl EffectKind99_Start(void) {
    unsigned char* const s = S();
    unsigned char* const c = s + 0xC;
    SetUL(c, UL(s + 0x34) - 0x10000u);
    const U x = UL(c);
    const U z = UL(S() + 0x38) + 0xFFFF0000u;
    SetUL(c + 4, z);
    const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
    SetUL(c + 8, High(h));
    SetUL(c + 0x10, 0);
    S()[9] = 0xA;
    Step();
}

// original 0x48C160 (state 1): the disc drawn (EffectKind99_DrawDisc(+0xC,
// +0x1C)), its radius +0x1C up 0x1199 (through the record read first); +9
// down (read again), at 0 sound 0x20F and +1 up.
extern "C" void __cdecl EffectKind99_Spread(void) {
    unsigned char* const s = S();
    const U radius = UL(s + 0x1C);
    unsigned char* const c = s + 0xC;
    SH_CALL(EffectKind99_DrawDisc)(reinterpret_cast<const long*>(c), static_cast<long>(radius));
    SetUL(c + 0x10, UL(c + 0x10) + 0x1199u);
    if (CountDown()) {
        SH_CALL(Sound_PlayEffect)(0x20F);
        Step();
    }
}

// original 0x48C7A0 (point, radius): EffectGte_LoadMapCamera; four fans
// (EffectKind99_DrawFan) at the angles 0, 0x400, 0x800, 0xC00, each linked at
// the point offset by kFanDx / kFanDz's dword i.
extern "C" void __cdecl EffectKind99_DrawDisc(const long* point, long radius) {
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kFanCount; ++i) {
        const long dz = Long(At(at::kFanDz + 4 * i));
        const long dx = Long(At(at::kFanDx + 4 * i));
        SH_CALL(EffectKind99_DrawFan)(point, radius, i << 10, dx, dz);
    }
}

// original 0x48C7F0 (point, radius, angle - its low word -, dx, dz): the
// centre (the point) projected; the edge at the angle - (Math_Cos * radius
// >> 12) + x, (Math_Sin * radius >> 12) + z, the point's height (each word of
// the point read after the call before it) - projected; four times: a draw
// mode (page (0x380, 0x100), abr 2, dtd 0) linked at (x + dx, z + dz) (dy 0,
// 0x38; the point read after the mode); a flat triangle (0x5A7570) at the
// cursor, semi-transparent: the centre (dwords copied), the last edge, the
// angle up 0x100 and the edge there projected; grey 0x80; linked again at
// the same cell.
extern "C" void __cdecl EffectKind99_DrawFan(const long* point, long radius, unsigned angle, long dx, long dz) {
    float centre[3], edge[3];
    long v[3];
    SH_CALL(EffectGte_ProjectPoint)(point, centre);
    const U r = static_cast<U>(radius);
    U a = angle & 0xFFFFu;
    const U cosine = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
    v[0] = static_cast<long>(Sar(Mul(cosine, r), 12) + static_cast<U>(point[0]));
    const U sine = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    v[1] = static_cast<long>(Sar(Mul(sine, r), 12) + static_cast<U>(point[1]));
    v[2] = point[2];
    SH_CALL(EffectGte_ProjectPoint)(v, edge);
    U turn = angle;
    for (unsigned n = 4; n != 0; --n) {
        DrawMode(2, 0x380, 0x100, 0);
        const U lx = static_cast<U>(point[0]) + static_cast<U>(dx);
        const U lz = static_cast<U>(point[1]) + static_cast<U>(dz);
        SH_CALL(MapView_LinkPrimAt)(lx, lz, 0, 0x38);
        unsigned char* const prim = Gfx_PacketNext;
        SH_AT(RecordFn, at::kPolyF3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, centre, 12);
        std::memcpy(prim + 0x14, edge, 12);
        turn += 0x100;
        a = turn & 0xFFFFu;
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        v[0] = static_cast<long>(Sar(Mul(c, r), 12) + static_cast<U>(point[0]));
        const U s = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        v[1] = static_cast<long>(Sar(Mul(s, r), 12) + static_cast<U>(point[1]));
        SH_CALL(EffectGte_ProjectPoint)(v, edge);
        std::memcpy(prim + 0x20, edge, 12);
        prim[4] = 0x80;
        prim[5] = 0x80;
        prim[6] = 0x80;
        SH_CALL(MapView_LinkPrimAt)(lx, lz, 0, 0x38);
    }
}

void Effect4C_Inject() {
    if (bof3::WantsShadow("effect_4c")) effect_4c::SelfTest();
    BOF3_INJECT(EffectKind8D_Run);
    BOF3_INJECT(EffectKind8D_Start);
    BOF3_INJECT(EffectKind8D_Hold);
    BOF3_INJECT(EffectKind8D_Pull);
    BOF3_INJECT(EffectKind8D_End);
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
    BOF3_INJECT(EffectKind8F_Run);
    BOF3_INJECT(EffectKind8F_Start);
    BOF3_INJECT(EffectKind8F_Burst);
    BOF3_INJECT(EffectKind8F_Rise);
    BOF3_INJECT(EffectKind8F_Arm);
    BOF3_INJECT(EffectKind8F_Trickle);
    BOF3_INJECT(EffectKind8F_Settle);
    BOF3_INJECT(EffectKind8F_ResetClock);
    BOF3_INJECT(EffectKind8F_Sequence);
    BOF3_INJECT(EffectKind8F_Fade);
    BOF3_INJECT(EffectKind8F_SpawnShard);
    BOF3_INJECT(EffectKind8F_SpawnShuffled);
    BOF3_INJECT(EffectKind8F_ClearShards);
    BOF3_INJECT(EffectKind8F_MoveShards);
    BOF3_INJECT(EffectKind8F_DrawShard);
    BOF3_INJECT(EffectKind90_Run);
    BOF3_INJECT(EffectKind90_Start);
    BOF3_INJECT(EffectKind90_Emit);
    BOF3_INJECT(EffectKind90_Fade);
    BOF3_INJECT(EffectKind90_EmitOne);
    BOF3_INJECT(EffectKind90_FindShard);
    BOF3_INJECT(EffectKind90_ClearShards);
    BOF3_INJECT(EffectKind90_MoveShards);
    BOF3_INJECT(EffectKind90_DrawShard);
    BOF3_INJECT(EffectKind93_Run);
    BOF3_INJECT(EffectKind93_Start);
    BOF3_INJECT(EffectKind93_Rise);
    BOF3_INJECT(EffectKind93_Fade);
    BOF3_INJECT(EffectKind93_Draw);
    BOF3_INJECT(EffectKind99_Run);
    BOF3_INJECT(EffectKind99_Start);
    BOF3_INJECT(EffectKind99_Spread);
    BOF3_INJECT(EffectKind99_DrawDisc);
    BOF3_INJECT(EffectKind99_DrawFan);
}

// Moved here by tools/move_funcs.py from effect_4f.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_ae.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/effect/effect_kind_ae_callees.h"
#include "game/effect_4f.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_ae::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULEffect4f(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect4f(U a) { return ULEffect4f(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

unsigned char& B(U a) { return At(a)[0]; }

U AddressOf(const void* p) { return Low32(p); }

void SetBits(unsigned char* to, U bits) { std::memcpy(to, &bits, 4); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSar(int a, U b, unsigned n) { return Sar(static_cast<U>(a) * b, n); }

// movsx r32, r16 of a register's low word.
U Sx16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v & 0xFFFFu))); }

// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, ULEffect4f(from + i));
}

// fild dword v; fadd dword [f]; fstp dword [to]
void FildAdd(unsigned char* to, U v, const unsigned char* f) {
    const auto i = static_cast<std::int32_t>(v);
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(to))
                     : "m"(i), "m"(*reinterpret_cast<const float*>(f)));
}

// fld dword [a]; fsub dword [b]; fstp dword [to]
void FldSub(unsigned char* to, const unsigned char* a, const unsigned char* b) {
    __asm__ volatile("flds %1\n\tfsubs %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(to))
                     : "m"(*reinterpret_cast<const float*>(a)), "m"(*reinterpret_cast<const float*>(b)));
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }

using Handler = scenario_harness::Handler;

// The entry `state` of a kind's table (read in place: the fuzz swaps the cells
// for recorders); a Fatal past the table's own length, where the original jumps
// (or calls) through the dword after - the next kind's table.
Handler Entry(const char* who, U table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4f.md section 6)",
                    who, state, entries, (unsigned)table);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect4f(table + 4 * state)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]
void DispatchEffect4f(const char* who, U table, unsigned entries) { Entry(who, table, entries)(); }

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +9 down one; true when it reached 0 (Sprite_Current read for each access,
// as the originals).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

unsigned char* Drop(U i) { return At(::bof3::Img(EffectKind30_Shards_addr) + i * at::kDropStride); }

unsigned char* Source(U i) { return At(::bof3::Img(at::kSources.v) + i * at::kSourceStride); }

unsigned char* Bar(U i) { return At(::bof3::Img(at::kBars.v) + i * at::kBarStride); }

// The arc's offset: (v * 5 << 6) sar 4 (lea, shl, sar on the 32 bits) - v * 20.
U ArcOffset(int v) { return Sar((static_cast<U>(v) * 5u) << 6, 4); }

void SetULRest3g(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetULRest3g(U a, U v) { SetULRest3g(At(a), v); }

}  // namespace

// original 0x492660 (hidden in 0x492400): jmp [EffectKindAD_States + +1 * 4],
// unbounded. States: 0x492680, 0x492710 (part 6), Effect_StateNext twice,
// Effect_StateRelease.
extern "C" void __cdecl EffectKindAD_Run(void) {
    DispatchEffect4f("EffectKindAD_Run", AddressOf(EffectKindAD_States), EffectKindAD_States_count);
}

// original 0x492680 (state 0): +0x34 / +0x38 the third member's x / z (ObjTrio
// record 2), +0x3C the ground there (AreaMap_Elevation's s16 << 16); the copy
// +0xC..+0x14; +0x5D 0, +0x5E 0x80, +9 0x20, +1 up.
extern "C" void __cdecl EffectKindAD_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->x, static_cast<std::uint32_t>(ObjTrio[2].x));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->z, static_cast<std::uint32_t>(ObjTrio[2].z));
    const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(S())->x, rv::As<records::EffectObject>(S())->z);
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->height = static_cast<U>(static_cast<std::int16_t>(h)) << 16;
    rv::As<records::EffectWork>(s)->work_0c = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x);
    rv::As<records::EffectWork>(s)->work_10 = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z);
    SetULRest3g(s + 0x14, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    s[0x5D] = 0;
    s[0x5E] = 0x80;
    s[9] = 0x20;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x492710 (state 1): +0x3C up 0x800000; EffectKindAD_DrawArc(+0x34,
// +0xC, +0x5D, +0x5E); once the counter byte 0x903848 is 0xB, +1 up.
extern "C" void __cdecl EffectKindAD_Rise(void) {
    {
        unsigned char* const s = S();
        rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 0x800000u;
        SH_CALL(EffectKindAD_DrawArc)(reinterpret_cast<const long*>(s + 0x34), reinterpret_cast<const long*>(s + 0xC),
                                      s[0x5D], s[0x5E]);
    }
    if (At(::bof3::Img(rest_3g::at::kCounter.v))[0] == 0xB) S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x492750 (hidden in 0x492400, catalog part 6; kind 0xAD's states 2
// and 3, and EffectKind60_States' too): +1 up.
extern "C" void __cdecl Effect_StateNext(void) { NextState(); }

// original 0x492760 (hidden in 0x492400): jmp [EffectKindAE_States + +1 * 4],
// unbounded. States: 0x492780 (part 6), _Sink, _Widen, _Narrow, _Shrink.
extern "C" void __cdecl EffectKindAE_Run(void) {
    DispatchEffect4f("EffectKindAE_Run", AddressOf(EffectKindAE_States), EffectKindAE_States_count);
}

// original 0x492780 (state 0): the point (0x188000, 0x120000) and its ground
// plus 0x800 (<< 16) at +0x34..+0x3C; +0x2E 0x80, +0x30 0x180, +0x32 0, +9 8,
// +1 up; the map camera; the point projected (a) and again 0x100 higher (b);
// +0x32 the angle Math_Ratan2(b.y - a.y, b.x - a.x) + 0x400 (each difference
// rounded to a float, as fstp leaves it); sound 0x202.
extern "C" void __cdecl EffectKindAE_Start(void) {
    rv::As<records::EffectObject>(S())->x = 0x188000u;
    rv::As<records::EffectObject>(S())->z = 0x120000u;
    const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(S())->x, rv::As<records::EffectObject>(S())->z);
    {
        unsigned char* const s = S();
        rv::As<records::EffectObject>(s)->height = static_cast<U>(static_cast<std::int16_t>(h) + 0x800) << 16;
        rv::As<records::EffectObject>(s)->screen_x = 0x80;
        rv::As<records::EffectObject>(s)->screen_y = 0x180;
        rv::As<records::EffectWork>(s)->work_32 = 0;
        s[9] = 8;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3];
    {
        const unsigned char* const s = S();
        point[0] = rv::As<records::EffectObject>(s)->x;
        point[1] = rv::As<records::EffectObject>(s)->z;
        point[2] = rv::As<records::EffectObject>(s)->height;
    }
    float a[3], b[3];
    SH_CALL(EffectGte_ProjectPoint)(point, a);
    point[2] = static_cast<long>(static_cast<U>(point[2]) - 0x1000000u);
    SH_CALL(EffectGte_ProjectPoint)(point, b);
    const float dx = static_cast<float>(static_cast<long double>(b[0]) - static_cast<long double>(a[0]));
    const float dy = static_cast<float>(static_cast<long double>(b[1]) - static_cast<long double>(a[1]));
    const int angle = SH_CALL(Math_Ratan2)(dy, dx);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, static_cast<U>(angle) + 0x400u);
    SH_CALL(Sound_PlayEffect)(0x202);
}

// original 0x492880 (state 1): the height +0x3C down 0x1000000; drawn
// (EffectKindAE_Draw); +9 down, at 0 +9 = 4, +1 up.
extern "C" void __cdecl EffectKindAE_Sink(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    so->height = static_cast<std::uint32_t>(so->height) + 0xFF000000u;
    SH_CALL(EffectKindAE_Draw)();
    if (!CountDown()) return;
    S()[9] = 4;
    NextState();
}

// original 0x4928C0 (state 2): the width +0x2E up 0x40, the height +0x30 down
// 0x40; drawn; +9 down, at 0 +9 = 2, +1 up.
extern "C" void __cdecl EffectKindAE_Widen(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, rv::As<records::EffectWork>(S())->work_2e + 0x40u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, rv::As<records::EffectWork>(S())->work_30 + 0xFFC0u);
    SH_CALL(EffectKindAE_Draw)();
    if (!CountDown()) return;
    S()[9] = 2;
    NextState();
}

// original 0x492900 (state 3): the width down 0x40, the height up 0x40; drawn;
// +9 down, at 0 +9 = 0x10, +1 up.
extern "C" void __cdecl EffectKindAE_Narrow(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, rv::As<records::EffectWork>(S())->work_2e + 0xFFC0u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, rv::As<records::EffectWork>(S())->work_30 + 0x40u);
    SH_CALL(EffectKindAE_Draw)();
    if (!CountDown()) return;
    S()[9] = 0x10;
    NextState();
}

// original 0x492940 (state 4): the width and the height down 0x10; drawn; +9
// down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindAE_Shrink(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, rv::As<records::EffectWork>(S())->work_2e + 0xFFF0u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, rv::As<records::EffectWork>(S())->work_30 + 0xFFF0u);
    SH_CALL(EffectKindAE_Draw)();
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x492980 (hidden in 0x492400): jmp [EffectKindAF_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindAF_Run(void) {
    DispatchEffect4f("EffectKindAF_Run", AddressOf(EffectKindAF_States), EffectKindAF_States_count);
}

// original 0x4929A0 (state 0): +9 = 0, +6 = 0x10, +1 up.
extern "C" void __cdecl EffectKindAF_Start(void) {
    S()[9] = 0;
    S()[6] = 0x10;
    NextState();
}

// original 0x4929C0 (state 1): +9 up 0x10; at 0 (the byte wrapped) the screen
// drawn at 0xFF and +1 up, else drawn at +9.
extern "C" void __cdecl EffectKindAF_FadeIn(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 0x10);
    s = S();
    const unsigned char shade = s[9];
    if (shade == 0) {
        SH_CALL(EffectKindAF_DrawScreen)(0xFF);
        NextState();
        return;
    }
    SH_CALL(EffectKindAF_DrawScreen)(shade);
}

// original 0x492A00 (state 2): +9 down 0x10; while Draw_PassFlags is set the
// screen drawn at +9; at +9 0: Effect_Release (a tail jmp) when Draw_PassFlags
// is 0, else +1 down (state 1 again).
extern "C" void __cdecl EffectKindAF_FadeOut(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 0xF0);
    if (Draw_PassFlags != 0) SH_CALL(EffectKindAF_DrawScreen)(S()[9]);
    s = S();
    if (s[9] != 0) return;
    if (Draw_PassFlags == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    s[1] = static_cast<unsigned char>(s[1] - 1);
}

// original 0x492A50 (hidden in 0x492400): jmp [EffectKindB0_States + +1 * 4],
// unbounded. States: _Start, _Spawn, Effect_StateRelease.
extern "C" void __cdecl EffectKindB0_Run(void) {
    DispatchEffect4f("EffectKindB0_Run", AddressOf(EffectKindB0_States), EffectKindB0_States_count);
}

// original 0x492A70 (state 0): the word +0x2E = 0x12C, +9 = 0; the bars
// cleared; +1 up.
extern "C" void __cdecl EffectKindB0_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0x12C);
    S()[9] = 0;
    SH_CALL(EffectKindB0_ClearBars)();
    NextState();
}

// original 0x492AA0 (state 1): at +9 0 a bar started (EffectKindB0_NewBar) and
// +9 = (Rand & 0xF) + 0x18, else +9 down; while Draw_PassFlags is set the bars
// stepped (EffectKindB0_StepBars(0xF0), its answer unread) - and +1 up when
// Draw_PassFlags is 0, before or after.
extern "C" void __cdecl EffectKindB0_Spawn(void) {
    unsigned char* s = S();
    unsigned char count = s[9];
    if (count == 0) {
        SH_CALL(EffectKindB0_NewBar)();
        const U r = static_cast<U>(SH_CALL(Rand)());
        s = S();
        count = static_cast<unsigned char>((r & 0xFu) + 0x18u);
    } else {
        count = static_cast<unsigned char>(count - 1);
    }
    s[9] = count;
    if (Draw_PassFlags != 0) {
        SH_CALL(EffectKindB0_StepBars)(0xF0);
        if (Draw_PassFlags != 0) return;
    }
    NextState();
}

// original 0x492AF0 (inside the cut's 0x492AA0 extent, no list's; reached by a
// tail jmp from kind 0xAB's state 1 EffectKindAB_Drops): EffectKind81_MoveDrops without its
// moving count - the map camera; each drop in use: its speed +0x14 0x20000 more,
// its height +0xC moved by the new speed, its blink bit +3 flipped, drawn
// (EffectKindAB_DrawDrop), its life +2 down - at 0 its +0 and +1 cleared. al 1
// when any was in use, else 0.
extern "C" unsigned char __cdecl EffectKindAB_MoveDrops(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = Drop(i);
        if (d[0] == 0) continue;
        const U speed = ULEffect4f(d + 0x14) + 0x20000u;
        const U height = ULEffect4f(d + 0xC);
        any = 1;
        const auto blink = static_cast<unsigned char>(d[3] ^ 1u);
        SetUL(d + 0x14, speed);
        SetUL(d + 0xC, height + speed);
        d[3] = blink;
        SH_CALL(EffectKindAB_DrawDrop)(d);
        d[2] = static_cast<unsigned char>(d[2] - 1);
        if (d[2] == 0) {
            d[0] = 0;
            d[1] = 0;
        }
    }
    return any;
}

// original 0x492B60 (cdecl): a drop (+4 x, z, height) drawn - a draw mode (page
// Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 0) linked into the map at its x, z
// (MapView_LinkPrimAt, dy 0, 0xC); a semi-transparent one-pixel tile at its
// projection, (0x80, 0x80, 0x80) with the blink bit +3 set, else (0x80, 0, 0),
// linked (0x14); a semi-transparent 3 by 3 tile one up and left of it, (0x20,
// 0x20, 0x20) or (0x20, 0, 0), linked (0x1C). Neither tile's +0x10 is written.
extern "C" void __cdecl EffectKindAB_DrawDrop(unsigned char* drop) {
    const U page = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(MapView_LinkPrimAt)(ULEffect4f(drop + 4), ULEffect4f(drop + 8), 0, 0xC);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(drop + 4), Out(o));
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    Copy12(p + 8, o);
    if (drop[3] != 0) {
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
    } else {
        p[4] = 0x80;
        p[5] = 0;
        p[6] = 0;
    }
    SH_CALL(MapView_LinkPrimAt)(ULEffect4f(drop + 4), ULEffect4f(drop + 8), 0, 0x14);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    FldSub(p + 8, o, At(at::kOne));
    SetBits(p + 0x14, 0x40400000u);   // 3.0f
    SetBits(p + 0x18, 0x40400000u);
    FldSub(p + 0xC, o + 4, At(at::kOne));
    if (drop[3] != 0) {
        p[4] = 0x20;
        p[5] = 0x20;
        p[6] = 0x20;
    } else {
        p[4] = 0x20;
        p[5] = 0;
        p[6] = 0;
    }
    SH_CALL(MapView_LinkPrimAt)(ULEffect4f(drop + 4), ULEffect4f(drop + 8), 0, 0x1C);
}

// original 0x492C80 (cdecl): EffectKind81_PlaceSources for eight - the sources
// at 0x92D780 (0x14 each): +0 1, +1 0, +2 (Rand & 0xF) - 0x10, +3 Rand & 0x1F
// (the first wait), +4 / +8 the cell EffectKindAB_SourceCells gives (s8 x, z <<
// 16), +0xC the ground's height there (AreaMap_Elevation's low word << 16).
extern "C" void __cdecl EffectKindAB_PlaceSources(void) {
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = Source(i);
        src[0] = 1;
        src[1] = 0;
        src[2] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0xFu) - 0x10u);
        const U r = static_cast<U>(SH_CALL(Rand)());
        const U z = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kSourceCells.v) + 2 * i + 1)))) << 16;
        src[3] = static_cast<unsigned char>(r & 0x1Fu);
        const U x = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kSourceCells.v) + 2 * i)))) << 16;
        SetUL(src + 4, x);
        SetUL(src + 8, z);
        const long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        SetUL(src + 0xC, Sx16(static_cast<U>(ground)) << 16);
    }
}

// original 0x492CF0 (cdecl; catalog part 7, unplaced): EffectKind81_Emit for
// eight - each source on (+0): its wait +3 down; when it was 0, two drops from
// free records (EffectKind81_FindFreeDrop; none, none) - +0 1, +2 life 0x20 +
// (Rand & 0x1F), +4 / +8 the source's x / z less 0x10000 and plus Rand's low
// byte << 8, +0xC its height, +0x14 0, +3 Rand & 1 - and the wait Rand & 7. al 1
// when any source was on, else 0.
extern "C" unsigned char __cdecl EffectKindAB_Emit(void) {
    unsigned char any = 0;
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = Source(i);
        if (src[0] == 0) continue;
        const unsigned wait = src[3];
        any = 1;
        src[3] = static_cast<unsigned char>(wait - 1);
        if (wait != 0) continue;
        for (unsigned n = 0; n < 2; ++n) {
            unsigned char* const d = SH_CALL(EffectKind81_FindFreeDrop)();
            if (d == nullptr) continue;
            d[0] = 1;
            d[2] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0x1Fu) + 0x20u);
            const U rx = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(d + 4, ((rx - 0x100u) << 8) + ULEffect4f(src + 4));
            const U rz = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(d + 8, ((rz - 0x100u) << 8) + ULEffect4f(src + 8));
            SetUL(d + 0xC, ULEffect4f(src + 0xC));
            SetUL(d + 0x14, 0);
            d[3] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 1u);
        }
        src[3] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 7u);
    }
    return any;
}

// original 0x492DC0 (cdecl; kind 0xAD's state 1 0x492710 draws it with the
// record's +0x34 and +0xC and its bytes +0x5D, +0x5E): a draw mode (Gpu_GetTPage
// (0, 1, 0x380, 0x100), dtd 1, committed to slot 1); about `a` and `b` each a
// point at (cos, sin) x 20 from the angle 0xFE00, projected; then 16 semi-
// transparent POLY_G4, the angle 0x80 more each: the last two projections and
// the new two, shaded `inner` at a's and `outer` at b's, committed 0x44 to slot
// 1. The second vertex's depth (+0x20) is b's projection's y, not its depth: the
// original copies the y twice (docs/effect_4f.md section 6, a latent defect kept).
extern "C" void __cdecl EffectKindAD_DrawArc(const long* a, const long* b, unsigned inner, unsigned outer) {
    DrawMode(1, 0x380, 0x100, 1, 1);
    const unsigned char* const pa = Bytes(a);
    const unsigned char* const pb = Bytes(b);
    alignas(4) unsigned char o1[12], o2[12], q[12], r[12];
    U angle = 0xFE00;
    {
        const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
        SetUL(q, ArcOffset(c) + ULEffect4f(pa));
        const int s = SH_CALL(Math_Sin)(static_cast<int>(angle));
        SetUL(q + 4, ArcOffset(s) + ULEffect4f(pa + 4));
        SetUL(q + 8, ULEffect4f(pa + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o1));
    }
    {
        const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
        SetUL(r, ArcOffset(c) + ULEffect4f(pb));
        const int s = SH_CALL(Math_Sin)(static_cast<int>(angle));
        SetUL(r + 4, ArcOffset(s) + ULEffect4f(pb + 4));
        SetUL(r + 8, ULEffect4f(pb + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(r), Out(o2));
    }
    const unsigned char shadeA = static_cast<unsigned char>(inner);
    const unsigned char shadeB = static_cast<unsigned char>(outer);
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        angle += 0x80;
        SetUL(p + 8, ULEffect4f(o1));
        SetUL(p + 0xC, ULEffect4f(o1 + 4));
        SetUL(p + 0x10, ULEffect4f(o1 + 8));
        SetUL(p + 0x18, ULEffect4f(o2));
        SetUL(p + 0x1C, ULEffect4f(o2 + 4));
        SetUL(p + 0x20, ULEffect4f(o2 + 4));   // the original's: the y again, not the depth o2 + 8
        const int t = static_cast<int>(angle & 0xFFFFu);
        {
            const int c = SH_CALL(Math_Cos)(t);
            SetUL(q, ArcOffset(c) + ULEffect4f(pa));
            const int s = SH_CALL(Math_Sin)(t);
            SetUL(q + 4, ArcOffset(s) + ULEffect4f(pa + 4));
            SetUL(q + 8, ULEffect4f(pa + 8));
            SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o1));
        }
        {
            const int c = SH_CALL(Math_Cos)(t);
            SetUL(r, ArcOffset(c) + ULEffect4f(pb));
            const int s = SH_CALL(Math_Sin)(t);
            SetUL(r + 4, ArcOffset(s) + ULEffect4f(pb + 4));
            SetUL(r + 8, ULEffect4f(pb + 8));
            SH_CALL(EffectGte_ProjectPoint)(Point(r), Out(o2));
        }
        SetUL(p + 0x28, ULEffect4f(o1));
        SetUL(p + 0x2C, ULEffect4f(o1 + 4));
        SetUL(p + 0x30, ULEffect4f(o1 + 8));
        SetUL(p + 0x38, ULEffect4f(o2));
        SetUL(p + 0x3C, ULEffect4f(o2 + 4));
        SetUL(p + 0x40, ULEffect4f(o2 + 8));
        p[4] = shadeA;
        p[5] = shadeA;
        p[6] = shadeA;
        p[0x14] = shadeB;
        p[0x15] = shadeB;
        p[0x16] = shadeB;
        p[0x24] = shadeA;
        p[0x25] = shadeA;
        p[0x26] = shadeA;
        p[0x34] = shadeB;
        p[0x35] = shadeB;
        p[0x36] = shadeB;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

// original 0x493010: a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1,
// committed to slot 1); the record's point +0x34.. copied to a local and the
// ellipse drawn there (Effect_DrawEllipse(local, +0x2E, +0x30, +0x32, 0xC0, 0)).
extern "C" void __cdecl EffectKindAE_Draw(void) {
    DrawMode(1, 0x380, 0x100, 1, 1);
    unsigned char* const s = S();
    alignas(4) unsigned char point[12];
    Copy12(point, s + 0x34);
    SH_CALL(Effect_DrawEllipse)(Point(point), rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_32, 0xC0, 0);
}

// original 0x493090 (cdecl; EffectKindAE_Draw's and EffectKind20_Draw's): the
// map camera; `point` projected (o) and its size {w, h} projected
// (EffectGte_ProjectSize) to the radii {rx, ry}, each one more on odd frames
// (Frame_Counter & 1, f); a fan of 16 semi-transparent POLY_G3 round o - the
// rim at the ellipse (cos a * rx, sin a * ry) sar 12 (each an s16) turned by
// `angle`, a from 0 by 0x100 - the centre (shade, shade, f ? shade : 0), the rim
// (rim, rim, f ? rim : 0); each committed 0x34 to slot 1.
extern "C" void __cdecl Effect_DrawEllipse(const long* point, unsigned w, unsigned h, unsigned angle, unsigned shade,
                                           unsigned rim) {
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(point, Out(o));
    alignas(4) short size[2] = {static_cast<short>(w), static_cast<short>(h)};
    alignas(4) short radius[2];
    SH_CALL(EffectGte_ProjectSize)(point, size, radius);
    const unsigned char odd = static_cast<unsigned char>(Frame_Counter & 1u);
    radius[0] = static_cast<short>(radius[0] + odd);
    radius[1] = static_cast<short>(radius[1] + odd);
    const U theta = angle & 0xFFFFu;
    const unsigned char c0 = static_cast<unsigned char>(shade);
    const unsigned char c1 = static_cast<unsigned char>(rim);
    // the angle a: the original keeps the sum unmasked and pushes it & 0xFFFF
    // (both the same below 0x10000; it ends at 0x1000)
    U a = 0;
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, ULEffect4f(o));
        SetUL(p + 0xC, ULEffect4f(o + 4));
        SetUL(p + 0x30, ULEffect4f(o + 8));
        SetUL(p + 0x20, ULEffect4f(o + 8));
        SetUL(p + 0x10, ULEffect4f(o + 8));
        // the first rim point, at a
        U x = Sx16(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a)), static_cast<U>(static_cast<std::int32_t>(radius[0])), 12));
        U y = Sx16(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a)), static_cast<U>(static_cast<std::int32_t>(radius[1])), 12));
        U cx = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(theta))) * x;
        U sy = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(theta))) * y;
        FildAdd(p + 0x18, Sar(cx - sy, 12), o);
        U sx = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(theta))) * x;
        U cy = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(theta))) * y;
        a += 0x100;
        const U next = a & 0xFFFFu;
        FildAdd(p + 0x1C, Sar(sx + cy, 12), o + 4);
        // the second, at a + 0x100
        x = Sx16(MulSar(SH_CALL(Math_Cos)(static_cast<int>(next)), static_cast<U>(static_cast<std::int32_t>(radius[0])), 12));
        y = Sx16(MulSar(SH_CALL(Math_Sin)(static_cast<int>(next)), static_cast<U>(static_cast<std::int32_t>(radius[1])), 12));
        cx = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(theta))) * x;
        sy = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(theta))) * y;
        FildAdd(p + 0x28, Sar(cx - sy, 12), o);
        sx = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(theta))) * x;
        cy = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(theta))) * y;
        FildAdd(p + 0x2C, Sar(sx + cy, 12), o + 4);
        p[4] = c0;
        p[5] = c0;
        p[6] = odd != 0 ? c0 : 0;
        p[0x14] = c1;
        p[0x15] = c1;
        p[0x16] = odd != 0 ? c1 : 0;
        p[0x24] = c1;
        p[0x25] = c1;
        p[0x26] = odd != 0 ? c1 : 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x4932E0 (cdecl): a semi-transparent tile over the screen - (0, 0),
// 320.0 by 240.0 (the floats at +0x14 / +0x18; +0x10 not written), the colour
// (shade, 0, 0) - committed 0x1C to slot 1. The fill is DIV-0041's site
// 0x493308 (section 3c): (Widescreen_FillX(), 0) Widescreen_FillWidth() x 240,
// which is the original's (0, 0) 320 x 240 until Widescreen_ArmFills has run
// and whenever the picture is narrow (docs/effect_4f.md section 6).
extern "C" void __cdecl EffectKindAF_DrawScreen(unsigned shade) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char c = static_cast<unsigned char>(shade);
    SetBits(p + 8, std::bit_cast<U>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    SetUL(p + 0xC, 0);
    SetBits(p + 0x14, std::bit_cast<U>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetBits(p + 0x18, 0x43700000u);   // 240.0f
    p[4] = c;
    p[5] = 0;
    p[6] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x493330: the sixteen bars of 6 at 0x92D820 out of use.
extern "C" void __cdecl EffectKindB0_ClearBars(void) {
    for (U i = 0; i < at::kBarCount; ++i) Bar(i)[0] = 0;
}

// original 0x493350: the first free bar (+0 0) of the sixteen in use, its
// state +2 0; none free, nothing.
extern "C" void __cdecl EffectKindB0_NewBar(void) {
    for (U i = 0; i < at::kBarCount; ++i) {
        unsigned char* const b = Bar(i);
        if (b[0] != 0) continue;
        b[0] = 1;
        b[2] = 0;
        return;
    }
}

// original 0x493370 (cdecl): a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100),
// dtd 1, committed to slot 7); the map camera; each bar in use by its state +2:
// 0 - +1 = 4, +3 = 4, the word +4 = `length`'s low word, +2 = 1; 1 - +2 = 2; 2
// - the word +4 down 8, below 0 the bar out of use (+0 = 0); then drawn (EffectKindA8_DrawBar,
// catalog part 6) unless its state is 0. al 1 when any was in use, else 0.
extern "C" unsigned char __cdecl EffectKindB0_StepBars(unsigned length) {
    DrawMode(1, 0x380, 0x100, 1, 7);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (U i = 0; i < at::kBarCount; ++i) {
        unsigned char* const b = Bar(i);
        if (b[0] == 0) continue;
        any = 1;
        switch (b[2]) {
        case 0: {
            const unsigned char state = b[2];
            b[1] = 4;
            b[3] = 4;
            SetWord(b + 4, length & 0xFFFFu);
            b[2] = static_cast<unsigned char>(state + 1);
            break;
        }
        case 1: b[2] = 2; break;
        case 2:
            SetWord(b + 4, Word(b + 4) + 0xFFF8u);
            if (static_cast<std::int16_t>(Word(b + 4)) < 0) b[0] = 0;
            break;
        default: break;
        }
        if (b[2] != 0) SH_AT(void (__cdecl*)(unsigned char*), at::kBarDraw.v)(b);
    }
    return any;
}

void EffectKindAe_InjectFromEffect4f() {
    BOF3_INJECT(EffectKindAD_Run);
    BOF3_INJECT(Effect_StateNext);
    BOF3_INJECT(EffectKindAE_Run);
    BOF3_INJECT(EffectKindAE_Sink);
    BOF3_INJECT(EffectKindAE_Widen);
    BOF3_INJECT(EffectKindAE_Narrow);
    BOF3_INJECT(EffectKindAE_Shrink);
    BOF3_INJECT(EffectKindAF_Run);
    BOF3_INJECT(EffectKindAF_Start);
    BOF3_INJECT(EffectKindAF_FadeIn);
    BOF3_INJECT(EffectKindAF_FadeOut);
    BOF3_INJECT(EffectKindB0_Run);
    BOF3_INJECT(EffectKindB0_Start);
    BOF3_INJECT(EffectKindB0_Spawn);
    BOF3_INJECT(EffectKindAB_MoveDrops);
    BOF3_INJECT(EffectKindAB_DrawDrop);
    BOF3_INJECT(EffectKindAB_PlaceSources);
    BOF3_INJECT(EffectKindAB_Emit);
    BOF3_INJECT(EffectKindAD_DrawArc);
    BOF3_INJECT(EffectKindAE_Draw);
    BOF3_INJECT(Effect_DrawEllipse);
    BOF3_INJECT(EffectKindAF_DrawScreen);
    BOF3_INJECT(EffectKindB0_ClearBars);
    BOF3_INJECT(EffectKindB0_NewBar);
    BOF3_INJECT(EffectKindB0_StepBars);
}

void EffectKindAe_InjectFromRest3g() {
    BOF3_INJECT(EffectKindAD_Start);
    BOF3_INJECT(EffectKindAD_Rise);
    BOF3_INJECT(EffectKindAE_Start);
}

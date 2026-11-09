// Moved here by tools/move_funcs.py from effect_2g.cpp, effect_3a.cpp, magic_engine.cpp, rest_3e.cpp, rest_3f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_5f.h"

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
#include "game/effect/effect_kind_5f_callees.h"
#include "game/effect_2g.h"
#include "game/effect_2g_callees.h"
#include "game/effect_3a.h"
#include "game/effect_3a_callees.h"
#include "game/effect_gte.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/magic_engine.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3e.h"
#include "game/rest_3e_callees.h"
#include "game/rest_3f.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_kind_5f::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

U ULEffect2g(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect2g(U a) { return ULEffect2g(At(a)); }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2g.md section 6)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect2g(table + 4 * state)))();
}

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table.
void DispatchEffect3a(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3a.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

}  // namespace

#define MENGINE_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

U ULRest3e(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3e(U a) { return ULRest3e(move_script::At(a)); }

const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }

// The moving states' draw: R3F's 0x480300(the point, 0x20 unread, the wobble 8,
// the dy `last`).
void Ring(U last) {
    SH_AT(void (__cdecl*)(const long*, unsigned, unsigned, unsigned), at::kR3FRing.v)(Point(S() + 0x34), 0x20, 8, last);
}

void Add(U offset, U by) { SetUL(S() + offset, ULRest3e(S() + offset) + by); }

void Placed(U x, U z, U y) {
    rv::As<records::EffectObject>(S())->x = x;
    rv::As<records::EffectObject>(S())->z = z;
    rv::As<records::EffectObject>(S())->height = y;
}

void TwoSounds(unsigned a, unsigned b) {
    SH_CALL(Sound_PlayEffect)(a);
    SH_CALL(Sound_PlayEffect)(b);
}

using LD = long double;

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// --- x87 as the original has it (effect_4e.cpp's form): `fld dword` through
// inline assembly, every operation in long double, `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}

LD I(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

long* PointRest3f(unsigned char* p) { return reinterpret_cast<long*>(p); }

// Gpu_GetTPage(0, abr, x, y), then Gpu_SetDrawMode(cursor, 0, dtd, the page's
// low word, 0 - the fifth word the leftover of GetTPage's five pushes).
void SetMode(U abr, int x, int y, int dtd) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
}

}  // namespace

// original 0x47FD80 (Effect_KindHandlers[0x5F], hidden in 0x47FBE0):
// jmp [EffectKind5F_States + +1 * 4], unbounded. Its state 0 is
// Task_StartHold60 (magic_engine.cpp); the rest are part 6 rows and E3A's.
extern "C" void __cdecl EffectKind5F_Run(void) {
    Dispatch("EffectKind5F_Run", AddressOf(EffectKind5F_States), EffectKind5F_States_count, 1);
}

// original 0x47FDA0: +9 = 60, the phase on. Shared: also entry 0 of the .data
// table EffectKind5F_States (0x654904) EffectKind5F_Run (0x47FD80) jumps through.
MENGINE_EXPORT void __cdecl Task_StartHold60(void) {
    Sprite_Current[9] = 0x3C;
    Bump(Sprite_Current[1]);
}

// original 0x47FDC0 (state 1): +9 down; at 0 the point (0x510000, 0x420000,
// 0x2C00000), the rise +0x14 = 0x800000, +1 up, sounds 0x202 and 0x203.
extern "C" void __cdecl EffectKind5F_Launch(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] != 0) return;
    Placed(0x510000, 0x420000, 0x2C00000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0x800000);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    TwoSounds(0x202, 0x203);
}

// original 0x47FE30 (state 2; PSX twin 0x801F995C): x down 0x10000, the height
// up by the rise, the rise down 0x200000; drawn (Ring, last 0); at x 0x490000
// the velocity (0x38E3, 0x10000, 0x38E38E + 0x800000), +1 up, sounds 0x202,
// 0x203.
extern "C" void __cdecl EffectKind5F_Hop(void) {
    Add(0x34, 0xFFFF0000u);
    Add(0x3C, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y));
    Add(0x14, 0xFFE00000u);
    Ring(0);
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x) != 0x490000) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_x, 0x38E3);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_z, 0x10000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0x38E38E);
    Add(0x14, 0x800000);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    TwoSounds(0x202, 0x203);
}

// original 0x47FEE0 (state 3): the point moved by the velocity, the rise down
// 0x200000; drawn (Ring, last 0); at z 0x4B0000 +9 = 0x3C and +1 up.
extern "C" void __cdecl EffectKind5F_Fly(void) {
    Add(0x34, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_x));
    Add(0x38, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_z));
    Add(0x3C, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y));
    Add(0x14, 0xFFE00000u);
    Ring(0);
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z) != 0x4B0000) return;
    S()[9] = 0x3C;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x47FF60 (state 4): +9 down; at 0 x = z = 0x4B0000, the height
// AreaMap_Elevation(x, z) << 16 (its low word, signed), the velocity
// (0xFFFF4925, 0xFFFF0000, 0xFFCC30C4 + 0x800000), +1 up, sounds 0x202,
// 0x203.
extern "C" void __cdecl EffectKind5F_Bounce(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] != 0) return;
    rv::As<records::EffectObject>(S())->x = 0x4B0000;
    rv::As<records::EffectObject>(S())->z = 0x4B0000;
    const U ground = static_cast<U>(static_cast<std::int16_t>(
                         SH_CALL(AreaMap_Elevation)(static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x)), static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z)))))
                     << 16;
    rv::As<records::EffectObject>(S())->height = ground;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_x, 0xFFFF4925u);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_z, 0xFFFF0000u);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0xFFCC30C4u);
    Add(0x14, 0x800000);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    TwoSounds(0x202, 0x203);
}

// original 0x480010 (state 5): the point moved by the velocity, the rise down
// 0x100000; drawn (Ring, last 0); at z 0x360000 a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind5F_FlyAway(void) {
    Add(0x34, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_x));
    Add(0x38, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_z));
    Add(0x3C, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y));
    Add(0x14, 0xFFF00000u);
    Ring(0);
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z) == 0x360000) SH_CALL(Effect_Release)();
}

// original 0x480080 (state 6): the point (0x270000, 0x80000, 0x8000000), the
// rise 0, +1 up, sounds 0x204 and 0x205.
extern "C" void __cdecl EffectKind5F_PlaceHigh(void) {
    Placed(0x270000, 0x80000, 0x8000000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    TwoSounds(0x204, 0x205);
}

// original 0x4800E0 (state 7): z up 0x10000, the height up by the rise, the
// rise down 0x200000; drawn (Ring, last 0); at x 0xF0000 a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind5F_Slide(void) {
    Add(0x38, 0x10000);
    Add(0x3C, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y));
    Add(0x14, 0xFFE00000u);
    Ring(0);
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x) == 0xF0000) SH_CALL(Effect_Release)();
}

// original 0x480140 (state 8): the point (0x190000, 0xF0000, 0x6000000), the
// rise 0, +1 up, sound 0x20A.
extern "C" void __cdecl EffectKind5F_PlaceLow(void) {
    Placed(0x190000, 0xF0000, 0x6000000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    SH_CALL(Sound_PlayEffect)(0x20A);
}

// original 0x480190 (state 9): z up 0x10000, the height up by the rise, the
// rise down 0x200000; drawn (Ring, last 4); at z 0x190000 or more (signed) a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKind5F_SlideOut(void) {
    Add(0x38, 0x10000);
    Add(0x3C, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y));
    Add(0x14, 0xFFE00000u);
    Ring(4);
    if (static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z)) >= 0x190000) SH_CALL(Effect_Release)();
}

// original 0x4801F0 (hidden in 0x47FBE0): jmp [EffectKind60_States + +1 * 4],
// unbounded. Its states are catalog part 6 rows (EffectKind60_Start, _SlideBoth, _SlideEnd),
// 0x492750 twice and Effect_StateRelease.
extern "C" void __cdecl EffectKind60_Run(void) {
    DispatchEffect3a("EffectKind60_Run", EffectKind60_States, EffectKind60_States_count);
}

// original 0x480210 (state 0, hidden in R3E's 0x47FBE0): the point +0x34..
// (0x198000, 0x198000, 0x4000000), the line's other end +0xC.. that point plus
// 0x40000 in x; +1 up.
extern "C" void __cdecl EffectKind60_Start(void) {
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->x = 0x198000;
    rv::As<records::EffectObject>(s)->z = 0x198000;
    rv::As<records::EffectObject>(s)->height = 0x4000000;
    rv::As<records::EffectWork>(s)->work_0c = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + 0x40000u;
    rv::As<records::EffectWork>(s)->work_10 = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z);
    SetUL(s + 0x14, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x480270 (state 1, hidden in 0x47FBE0): both ends' x down 0x20000;
// the line (EffectKind60_DrawLine(+0x34, +0xC)); +1 up once +0x34 is 0x98000.
extern "C" void __cdecl EffectKind60_SlideBoth(void) {
    unsigned char* s = S();
    rv::As<records::EffectObject>(s)->x = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + 0xFFFE0000u;
    s = S();
    rv::As<records::EffectWork>(s)->work_0c = rv::As<records::EffectWork>(s)->work_0c + 0xFFFE0000u;
    s = S();
    SH_CALL(EffectKind60_DrawLine)(PointRest3f(s + 0x34), PointRest3f(s + 0xC));
    s = S();
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) == 0x98000u) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4802C0 (state 2, hidden in 0x47FBE0): the other end's x down
// 0x20000; the line; +1 up once +0xC is 0x98000.
extern "C" void __cdecl EffectKind60_SlideEnd(void) {
    unsigned char* s = S();
    rv::As<records::EffectWork>(s)->work_0c = rv::As<records::EffectWork>(s)->work_0c + 0xFFFE0000u;
    s = S();
    SH_CALL(EffectKind60_DrawLine)(PointRest3f(s + 0x34), PointRest3f(s + 0xC));
    s = S();
    if (rv::As<records::EffectWork>(s)->work_0c == 0x98000u) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x480300 (point, unused, wobble, dy; PSX twin 0x801F9F8C): a draw
// mode (Gpu_GetTPage(0, 0, 0x3C0, 0x100), dtd 0) linked 0xC at the point's x,
// y (MapView_LinkPrimAt, dy the fourth word); EffectGte_LoadMapCamera; o the
// point projected and r EffectGte_ProjectSize(point, {0x50, 0x50})'s first
// word; an angle Rand & 0xFFF. For each row i from -r to r (s16): s =
// sqrt(r * r - i * i) (0x5A7A90), a semi-transparent black LINE_F2 from
// (sinA + b + o.x - s) to (sinB + s + b + o.x) at y o.y + i, both at o's depth,
// where sinA / sinB are (Math_Sin(angle) << 4) sar 12 (called twice) and b is
// the wobble's low word, negated every row; linked 0x20 at the point; the angle
// up Rand & 0xFF. The second word is not read.
extern "C" void __cdecl EffectKind5F_DrawLineDisc(const long* point, unsigned unused, unsigned wobble, unsigned dy) {
    (void)unused;
    SetMode(0, 0x3C0, 0x100, 0);
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]),
                                static_cast<int>(dy), 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(point, Out(o));
    alignas(4) short size[2] = {0x50, 0x50};
    alignas(4) unsigned char r[4];
    SH_CALL(EffectGte_ProjectSize)(point, size, reinterpret_cast<short*>(r));
    U angle = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U radius = UL(r);
    U i = 0u - radius;
    if (static_cast<std::int16_t>(i) > static_cast<std::int16_t>(radius)) return;
    U b = wobble;
    do {
        const std::int32_t row = static_cast<std::int16_t>(i);
        const std::int32_t rr = static_cast<std::int16_t>(radius);
        const int s = SH_AT(int (__cdecl*)(int), rest_3f::at::kSqrt.v)(rr * rr - row * row);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineF2)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        alignas(4) unsigned char fb[4], fs[4];
        StF(fb, I(static_cast<U>(static_cast<std::int16_t>(b))));
        StF(fs, I(static_cast<U>(static_cast<std::int16_t>(s))));
        const int a = static_cast<int>(angle & 0xFFFFu);
        const U sin_a = Sar(static_cast<U>(SH_CALL(Math_Sin)(a)) << 4, 12);
        StF(p + 8, I(sin_a) + F(fb) + F(o) - F(fs));
        const U sin_b = Sar(static_cast<U>(SH_CALL(Math_Sin)(a)) << 4, 12);
        StF(p + 0x14, I(sin_b) + F(fs) + F(fb) + F(o));
        const LD y = I(static_cast<U>(row)) + F(o + 4);
        StF(p + 0x18, y);
        StF(p + 0xC, y);
        SetUL(p + 0x1C, UL(o + 8));
        SetUL(p + 0x10, UL(o + 8));
        p[4] = 0;
        p[5] = 0;
        p[6] = 0;
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]),
                                    static_cast<int>(dy), 0x20);
        angle += static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
        b = 0u - b;
        ++i;
    } while (static_cast<std::int16_t>(i) <= static_cast<std::int16_t>(radius));
}

// original 0x4804C0: a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0x100), committed
// to slot 1); a semi-transparent LINE_G2 from `from` projected (grey 0x40) to
// `to` projected (black), committed 0x24 to slot 1.
extern "C" void __cdecl EffectKind60_DrawLine(const long* from, const long* to) {
    DrawMode(1, 0x3C0, 0x100, 0, 1);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineG2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    SetUL(p + 8, UL(o));
    SetUL(p + 0xC, UL(o + 4));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    Copy12(p + 0x18, o);
    p[4] = 0x40;
    p[5] = 0x40;
    p[6] = 0x40;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x24);
}

void EffectKind5fFromEffect2g_Inject() {
    BOF3_INJECT(EffectKind5F_Run);
}

void EffectKind5fFromEffect3a_Inject() {
    BOF3_INJECT(EffectKind60_Run);
    BOF3_INJECT(EffectKind60_DrawLine);
}

void EffectKind5fFromMagicEngine_Inject() {
    BOF3_INJECT(Task_StartHold60);
}

void EffectKind5fFromRest3e_Inject() {
    BOF3_INJECT(EffectKind5F_Launch);
    BOF3_INJECT(EffectKind5F_Hop);
    BOF3_INJECT(EffectKind5F_Fly);
    BOF3_INJECT(EffectKind5F_Bounce);
    BOF3_INJECT(EffectKind5F_FlyAway);
    BOF3_INJECT(EffectKind5F_PlaceHigh);
    BOF3_INJECT(EffectKind5F_Slide);
    BOF3_INJECT(EffectKind5F_PlaceLow);
    BOF3_INJECT(EffectKind5F_SlideOut);
}

void EffectKind5fFromRest3f_Inject() {
    BOF3_INJECT(EffectKind60_Start);
    BOF3_INJECT(EffectKind60_SlideBoth);
    BOF3_INJECT(EffectKind60_SlideEnd);
    BOF3_INJECT(EffectKind5F_DrawLineDisc);
}

// Moved here by tools/move_funcs.py from effect_4e.cpp, rest_3f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_9e.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_9e_callees.h"
#include "game/effect_4e.h"
#include "game/effect_4e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3f.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_9e::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

U AddressOf(const void* p) { return Low32(p); }

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }

unsigned char* Bytes(long* p) { return reinterpret_cast<unsigned char*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next kind's table or data.
void DispatchEffect4e(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4e.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// Gpu_GetTPage(0, abr, x, y), then Gpu_SetDrawMode(cursor, 0, dtd, the page's
// low word, 0 - the fifth word the leftover of GetTPage's five pushes).
void SetMode(U abr, int x, int y, int dtd) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
}

// The corners the four quads share (Sprite_Current +6 read afresh for each):
// "plus-minus" is (a.x + b.x, a.z - b.z) when +6 bit 0 is set, else (a.x -
// b.x, a.z + b.z); "minus-plus" the other.
void PlusMinus(unsigned char* v, const unsigned char* a, const unsigned char* b, bool set) {
    if (set) {
        SetUL(v, UL(a) + UL(b));
        SetUL(v + 4, UL(a + 4) - UL(b + 4));
    } else {
        SetUL(v, UL(a) - UL(b));
        SetUL(v + 4, UL(b + 4) + UL(a + 4));
    }
}

bool Bit0() { return (S()[6] & 1) != 0; }

void Sum(unsigned char* v, const unsigned char* a, const unsigned char* b, U lift) {
    SetUL(v, UL(a) + UL(b));
    SetUL(v + 4, UL(b + 4) + UL(a + 4));
    SetUL(v + 8, UL(b + 8) + UL(a + 8) + lift);
}

void Difference(unsigned char* v, const unsigned char* a, const unsigned char* b) {
    SetUL(v, UL(a) - UL(b));
    SetUL(v + 4, UL(a + 4) - UL(b + 4));
    SetUL(v + 8, UL(a + 8) - UL(b + 8));
}

// The corners of a flat quad into p + 8, + 0x14, + 0x20, + 0x2C: a + b; the
// one of +6 bit 0 at a.y + b.y; the other at a.y - b.y; a - b.
void FlatCorners(unsigned char* p, const unsigned char* a, const unsigned char* b) {
    alignas(4) unsigned char v[12];
    Sum(v, a, b, 0);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 8));
    PlusMinus(v, a, b, Bit0());
    SetUL(v + 8, UL(b + 8) + UL(a + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x14));
    PlusMinus(v, a, b, !Bit0());
    SetUL(v + 8, UL(a + 8) - UL(b + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x20));
    Difference(v, a, b);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x2C));
}

// The shade of the two shaded quads: ((+6 & 0xFE) << 6, (~+6 & 0xFE) << 6, 0).
void ShadeBy6(unsigned char* p) {
    p[4] = static_cast<unsigned char>((S()[6] & 0xFE) << 6);
    p[6] = 0;
    p[5] = static_cast<unsigned char>((~S()[6] & 0xFE) << 6);
}

using move_script::At;

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// Twelve bytes moved as the original moves them (three dword movs).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

long* PointRest3f(unsigned char* p) { return reinterpret_cast<long*>(p); }

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

}  // namespace

// The extent +0xC.. stepped by its speed +0x18.. (`sign` +1 or -1), then the
// outline drawn round the point; +9 down, at 0 +1 up.
static void Step9E(int sign) {
    unsigned char* const s = S();
    for (unsigned i = 0; i < 12; i += 4)
        SetUL(s + 0xC + i, sign > 0 ? UL(s + 0xC + i) + UL(s + 0x18 + i) : UL(s + 0xC + i) - UL(s + 0x18 + i));
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const t = S();
    SH_CALL(EffectKind9E_DrawOutline)(PointRest3f(t + 0x34), PointRest3f(t + 0xC));
    if (CountDown()) NextState();
}

// original 0x48F070 (hidden in catalog part 6's 0x48ED80): jmp
// [EffectKind9E_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9E_Run(void) {
    DispatchEffect4e("EffectKind9E_Run", EffectKind9E_States, EffectKind9E_States_count);
}

// original 0x48F090 (state 0, hidden in 0x48ED80): the half-extent +0xC.. 0;
// its speed +0x18.. (0x3000, 0, 0x200000), or (0, 0x3000, 0x200000) when +6 bit
// 0 is set; +6 bit 3 set: the extent already 8 steps on and +1 up three (to
// state 4), else +9 = 8 and +1 up. Unless +0xB bit 0, sound 0x207 (0x202 when
// +6 bit 2).
extern "C" void __cdecl EffectKind9E_Start(void) {
    unsigned char* const s = S();
    rv::As<records::EffectWork>(s)->work_0c = 0;
    rv::As<records::EffectWork>(s)->work_10 = 0;
    SetUL(s + 0x14, 0);
    if ((s[6] & 1) == 0) {
        rv::As<records::EffectObject>(s)->work_18 = 0x3000;
        rv::As<records::EffectObject>(s)->work_1c = 0;
    } else {
        rv::As<records::EffectObject>(s)->work_18 = 0;
        rv::As<records::EffectObject>(s)->work_1c = 0x3000;
    }
    rv::As<records::EffectObject>(s)->work_20 = 0x200000;
    if ((s[6] & 8) != 0) {
        rv::As<records::EffectWork>(s)->work_0c = rv::As<records::EffectWork>(s)->work_0c + (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) << 3);
        rv::As<records::EffectWork>(s)->work_10 = rv::As<records::EffectWork>(s)->work_10 + (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) << 3);
        SetUL(s + 0x14, UL(s + 0x14) + (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20) << 3));
        s[1] = static_cast<unsigned char>(s[1] + 3);
    } else {
        s[9] = 8;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
    if ((s[0xB] & 1) == 0) SH_CALL(Sound_PlayEffect)((s[6] & 4) != 0 ? at::kSound9EOpenB : at::kSound9EOpenA);
}

// original 0x48F170 (state 1, hidden in 0x48ED80): the extent grows by its
// speed; the outline; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind9E_Grow(void) { Step9E(1); }

// original 0x48F1E0 (state 2; in no list - the cut's 0x48F170 runs over it):
// the outline and the plate (EffectKind9E_DrawPlate); the wall vector
// 0x6762A0 = -extent; +9 = 0x80, +1 up.
extern "C" void __cdecl EffectKind9E_Flash(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    SH_CALL(EffectKind9E_DrawOutline)(PointRest3f(s + 0x34), PointRest3f(s + 0xC));
    s = S();
    SH_CALL(EffectKind9E_DrawPlate)(PointRest3f(s + 0x34), PointRest3f(s + 0xC));
    s = S();
    unsigned char* const w = At(::bof3::Img(Rest3f_9EWall_addr));
    SetUL(w, 0u - rv::As<records::EffectWork>(s)->work_0c);
    SetUL(w + 4, 0u - rv::As<records::EffectWork>(s)->work_10);
    SetUL(w + 8, 0u - UL(s + 0x14));
    s[9] = 0x80;
    NextState();
}

// original 0x48F240 (state 3, hidden in 0x48ED80): the wall vector's third
// word up +0x20 sar 1, negated when it equals +0x14; on odd frames the shaded
// plate, then the textured plate 0x4000 further along x (along y when +6 bit
// 0) and the point put back; on even frames the wall (EffectKind9E_DrawWall(
// +0x34, 0x6762A0)). +1 up once the chapter's count equals +7.
extern "C" void __cdecl EffectKind9E_Hold(void) {
    {
        unsigned char* const s = S();
        unsigned char* const w = At(::bof3::Img(Rest3f_9EWall_addr));
        const U v = UL(w + 8) + Sar(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20), 1);
        SetUL(w + 8, v);
        if (v == UL(s + 0x14)) SetUL(w + 8, 0u - v);
    }
    SH_CALL(EffectGte_LoadMapCamera)();
    if ((Frame_Counter & 1) != 0) {
        unsigned char* s = S();
        SH_CALL(EffectKind9E_DrawShadePlate)(PointRest3f(s + 0x34), PointRest3f(s + 0xC));
        s = S();
        if ((s[6] & 1) == 0)
            rv::As<records::EffectObject>(s)->x = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + 0x4000u;
        else
            rv::As<records::EffectObject>(s)->z = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) + 0x4000u;
        s = S();
        SH_CALL(EffectKind9E_DrawTexPlate)(PointRest3f(s + 0x34), PointRest3f(s + 0xC));
        s = S();
        if ((s[6] & 1) == 0)
            rv::As<records::EffectObject>(s)->x = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + 0xFFFFC000u;
        else
            rv::As<records::EffectObject>(s)->z = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) + 0xFFFFC000u;
    } else {
        unsigned char* const s = S();
        SH_CALL(EffectKind9E_DrawWall)(PointRest3f(s + 0x34), reinterpret_cast<const long*>(At(::bof3::Img(Rest3f_9EWall_addr))));
    }
    unsigned char* const s = S();
    if (At(::bof3::Img(at::kCounter.v))[0] == s[7]) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x48F300 (state 4; in no list - the cut's 0x48F240 runs over it):
// the plate; +9 = 8, +1 up; unless +0xB bit 1, sound 0x208 (0x203 when +6 bit
// 2).
extern "C" void __cdecl EffectKind9E_Close(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    SH_CALL(EffectKind9E_DrawPlate)(PointRest3f(s + 0x34), PointRest3f(s + 0xC));
    S()[9] = 8;
    NextState();
    s = S();
    if ((s[0xB] & 2) == 0) SH_CALL(Sound_PlayEffect)((s[6] & 4) != 0 ? at::kSound9EShutB : at::kSound9EShutA);
}

// original 0x48F360 (state 5, hidden in 0x48ED80): the extent shrinks by its
// speed; the outline; +9 down, at 0 +1 up (to Effect_StateRelease).
extern "C" void __cdecl EffectKind9E_Shrink(void) { Step9E(-1); }

// original 0x48F3D0 (a, b; PSX twin 0x801F9020): four corners projected - a +
// b, the crossed corner (a.x - b.x, a.y + b.y, a.z + b.z) or (a.x + b.x, a.y -
// b.y, a.z + b.z) by Sprite_Current +6 bit 0 (clear: the first), the other
// crossed corner at z a.z - b.z, and a - b; an opaque white LINE_F4 round the
// four (a + b, the first crossed, a - b, the second) linked 0x38 at a's x, y,
// and an opaque white LINE_F2 from a + b to the second crossed corner linked
// 0x20.
extern "C" void __cdecl EffectKind9E_DrawOutline(const long* a, const long* b) {
    alignas(4) long q[3];
    alignas(4) unsigned char o0[12], o1[12], o2[12], o3[12];
    q[0] = static_cast<long>(static_cast<U>(a[0]) + static_cast<U>(b[0]));
    q[1] = static_cast<long>(static_cast<U>(b[1]) + static_cast<U>(a[1]));
    q[2] = static_cast<long>(static_cast<U>(b[2]) + static_cast<U>(a[2]));
    SH_CALL(EffectGte_ProjectPoint)(q, Out(o0));
    if ((S()[6] & 1) == 0) {
        q[0] = static_cast<long>(static_cast<U>(a[0]) - static_cast<U>(b[0]));
        q[1] = static_cast<long>(static_cast<U>(b[1]) + static_cast<U>(a[1]));
    } else {
        q[0] = static_cast<long>(static_cast<U>(a[0]) + static_cast<U>(b[0]));
        q[1] = static_cast<long>(static_cast<U>(a[1]) - static_cast<U>(b[1]));
    }
    q[2] = static_cast<long>(static_cast<U>(b[2]) + static_cast<U>(a[2]));
    SH_CALL(EffectGte_ProjectPoint)(q, Out(o1));
    if ((S()[6] & 1) == 0) {
        q[0] = static_cast<long>(static_cast<U>(a[0]) + static_cast<U>(b[0]));
        q[1] = static_cast<long>(static_cast<U>(a[1]) - static_cast<U>(b[1]));
    } else {
        q[0] = static_cast<long>(static_cast<U>(a[0]) - static_cast<U>(b[0]));
        q[1] = static_cast<long>(static_cast<U>(b[1]) + static_cast<U>(a[1]));
    }
    q[2] = static_cast<long>(static_cast<U>(a[2]) - static_cast<U>(b[2]));
    SH_CALL(EffectGte_ProjectPoint)(q, Out(o2));
    q[1] = static_cast<long>(static_cast<U>(a[1]) - static_cast<U>(b[1]));
    q[0] = static_cast<long>(static_cast<U>(a[0]) - static_cast<U>(b[0]));
    q[2] = static_cast<long>(static_cast<U>(a[2]) - static_cast<U>(b[2]));
    SH_CALL(EffectGte_ProjectPoint)(q, Out(o3));
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    Copy12(p + 8, o0);
    Copy12(p + 0x14, o1);
    Copy12(p + 0x20, o3);
    Copy12(p + 0x2C, o2);
    p[4] = 0xFF;
    p[5] = 0xFF;
    p[6] = 0xFF;
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(a[0]), static_cast<unsigned long>(a[1]), 0, 0x38);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    Copy12(p + 8, o0);
    Copy12(p + 0x14, o2);
    p[4] = 0xFF;
    p[5] = 0xFF;
    p[6] = 0xFF;
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(a[0]), static_cast<unsigned long>(a[1]), 0, 0x20);
}

// original 0x48F5D0: an opaque white POLY_F4 (0xFF, 0xFF, 0xFF), its corners
// FlatCorners(a, b) projected; linked 0x38 at (a.x, a.z) (MapView_LinkPrimAt).
extern "C" void __cdecl EffectKind9E_DrawPlate(const long* a, const long* b) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    FlatCorners(p, Bytes(a), Bytes(b));
    p[4] = 0xFF;
    p[5] = 0xFF;
    p[6] = 0xFF;
    SH_CALL(MapView_LinkPrimAt)(UL(Bytes(a)), UL(Bytes(a) + 4), 0, 0x38);
}

// original 0x48F720: a semi-transparent textured POLY_FT4 (0x20, 0x20, 0x20):
// corners (the one of +6 bit 0 at a.y + b.y, a + b, a - b, the other at a.y -
// b.y) projected into +8, +0x18, +0x28, +0x38; the texture from
// EffectKind9E_Pages' record 0 or (+6 bit 2) record 1 - (s16 x, s16 y, u, v):
// u / v and u + 0x18 / v + 0x10 at the corners, the page Gpu_GetTPage(1, 1, x,
// y), the CLUT Gpu_GetClut(0xA0, 0x1F0); linked 0x48 at (a.x, a.z).
extern "C" void __cdecl EffectKind9E_DrawTexPlate(const long* a, const long* b) {
    const unsigned char* const page = EffectKind9E_Pages + ((S()[6] & 4) != 0 ? 6 : 0);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char* const pa = Bytes(a);
    const unsigned char* const pb = Bytes(b);
    alignas(4) unsigned char v[12];
    PlusMinus(v, pa, pb, Bit0());
    SetUL(v + 8, UL(pa + 8) + UL(pb + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 8));
    Sum(v, pa, pb, 0);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x18));
    Difference(v, pa, pb);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x28));
    PlusMinus(v, pa, pb, !Bit0());
    SetUL(v + 8, UL(pa + 8) - UL(pb + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x38));
    p[0x14] = page[4];
    p[0x15] = page[5];
    p[0x24] = static_cast<unsigned char>(page[4] + 0x18);
    p[0x25] = page[5];
    p[0x34] = page[4];
    p[0x35] = static_cast<unsigned char>(page[5] + 0x10);
    p[0x44] = static_cast<unsigned char>(page[4] + 0x18);
    p[0x45] = static_cast<unsigned char>(page[5] + 0x10);
    const U tp = SH_CALL(Gpu_GetTPage)(1, 1, SW(page), SW(page + 2));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, tp);
    const U clut = SH_CALL(Gpu_GetClut)(0xA0, 0x1F0);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, clut);
    p[4] = 0x20;
    p[5] = 0x20;
    p[6] = 0x20;
    SH_CALL(MapView_LinkPrimAt)(UL(pa), UL(pa + 4), 0, 0x48);
}

// original 0x48F8F0: an opaque POLY_F4 wall: its bottom corners a + b and the
// one of +6 bit 0 (at a.y + b.y), its top the same two 0x100000 higher; the
// shade ShadeBy6; linked 0x38 at (a.x, a.z).
extern "C" void __cdecl EffectKind9E_DrawWall(const long* a, const long* b) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    const unsigned char* const pa = Bytes(a);
    const unsigned char* const pb = Bytes(b);
    alignas(4) unsigned char v[12];
    Sum(v, pa, pb, 0);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 8));
    PlusMinus(v, pa, pb, Bit0());
    SetUL(v + 8, UL(pb + 8) + UL(pa + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x14));
    Sum(v, pa, pb, 0x100000);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x20));
    PlusMinus(v, pa, pb, Bit0());
    SetUL(v + 8, UL(pb + 8) + UL(pa + 8) + 0x100000u);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x2C));
    ShadeBy6(p);
    SH_CALL(MapView_LinkPrimAt)(UL(pa), UL(pa + 4), 0, 0x38);
}

// original 0x48FA80: a draw mode (Gpu_GetTPage(0, 0, 0x3C0, 0), dtd 1) linked
// 0xC at (a.x, a.z); a semi-transparent POLY_F4, its corners FlatCorners(a,
// b), the shade ShadeBy6; linked 0x38 at (a.x, a.z).
extern "C" void __cdecl EffectKind9E_DrawShadePlate(const long* a, const long* b) {
    SetMode(0, 0x3C0, 0, 1);
    const unsigned char* const pa = Bytes(a);
    SH_CALL(MapView_LinkPrimAt)(UL(pa), UL(pa + 4), 0, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    FlatCorners(p, pa, Bytes(b));
    ShadeBy6(p);
    SH_CALL(MapView_LinkPrimAt)(UL(pa), UL(pa + 4), 0, 0x38);
}

void EffectKind9e_Inject() {
    BOF3_INJECT(EffectKind9E_Run);
    BOF3_INJECT(EffectKind9E_Start);
    BOF3_INJECT(EffectKind9E_Grow);
    BOF3_INJECT(EffectKind9E_Flash);
    BOF3_INJECT(EffectKind9E_Hold);
    BOF3_INJECT(EffectKind9E_Close);
    BOF3_INJECT(EffectKind9E_Shrink);
    BOF3_INJECT(EffectKind9E_DrawOutline);
    BOF3_INJECT(EffectKind9E_DrawPlate);
    BOF3_INJECT(EffectKind9E_DrawTexPlate);
    BOF3_INJECT(EffectKind9E_DrawWall);
    BOF3_INJECT(EffectKind9E_DrawShadePlate);
}

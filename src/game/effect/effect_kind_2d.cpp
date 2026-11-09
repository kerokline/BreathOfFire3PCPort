// Moved here by tools/move_funcs.py from effect_2a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_2d.h"

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

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

void AddWord(unsigned char* p, U d) { SetWord(p, Word(p) + d); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// cdq; and edx, 7; add eax, edx; sar eax, 3: a signed divide by 8, toward zero.
U Div8(U v) { return Sar(v + (static_cast<std::int32_t>(v) < 0 ? 7u : 0u), 3); }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}

double D(const unsigned char* p) { return static_cast<double>(F(p)); }

void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

void SetD(unsigned char* p, double d) { SetF(p, static_cast<float>(d)); }

// fld dword / fstp dword: the bits kept, but a signalling NaN made quiet.
void CopyQuiet(unsigned char* to, const unsigned char* from) {
    U bits = UL(from);
    if ((bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0) bits |= 0x00400000u;
    SetUL(to, bits);
}

// fld dword a; fadd dword b; fstp: two floats added on the x87 itself, under the
// control word the original runs with - where both may be NaN, the x87 keeps the
// one with the larger significand (a quiet one over a signalling one), which
// SSE's "first operand" rule does not (kind 0x2D's start adds two projections).
float X87Add(float a, float b) {
    float r;
    __asm__("flds %1\n\tfadds %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b));
    return r;
}

// fild dword of a whole number: (double) of it, exact.
double I(U v) { return static_cast<double>(static_cast<std::int32_t>(v)); }

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax, its low 32 bits.
U Ftol(double v) {
    if (!(v > -9.2233720368547758e18 && v < 9.2233720368547758e18)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

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

// +9 down one (Sprite_Current read for each access, as the originals); at 0,
// +9 = `reload` and +1 up - or, when `reload` is negative, +1 up with +9 left 0.
void CountDown(int reload) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    if (reload >= 0) {
        s[9] = static_cast<unsigned char>(reload);
        s = S();
    }
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// A draw mode for the page (x, y) with transparency `abr`, and `dtd` its
// dither flag: Gpu_GetTPage(0, abr, x, y), Gpu_SetDrawMode(the packet cursor,
// 0, dtd, the page's low word, 0).
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
}

void Shade3(unsigned char* v, unsigned char value) { v[0] = v[1] = v[2] = value; }

U LinkX() { return Effect2a_LinkX; }

U LinkZ() { return Effect2a_LinkZ; }

// A draw mode for the page (0x380, 0x100) linked at kind 0x2D's cell.
void LinkedMode(unsigned abr, int dtd) {
    DrawMode(abr, 0x380, 0x100, dtd);
    SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0xC);
}

// The pool's records.
unsigned char* Pool(U stride, unsigned i) { return At(::bof3::Img(EffectKind30_Shards_addr) + stride * i); }

}  // namespace

// original 0x472420: jmp [EffectKind2D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind2D_Run(void) { Dispatch("EffectKind2D_Run", AddressOf(EffectKind2D_States), EffectKind2D_States_count); }

// original 0x472440 (state 0): the link cell 0x676100 / 0x676104 = (0x1C0000,
// 0x180000); EffectGte_LoadMapCamera; Sprite_Objects[0]'s point projected into
// the record's +0xC..+0x14 (three floats), Sprite_Objects[1]'s into a stack
// vector, the two added into +0xC..+0x14 (x, y, depth), the words +0x18 / +0x1A
// = 0, x and y scaled by the float 0x5C41D8; Msg_OpenScript(0x14), Field_Request
// = 2, +9 = 0x32, +1 up, Sound_PlayEffect(0x200).
extern "C" void __cdecl EffectKind2D_Start(void) {
    Effect2a_LinkX = 0x1C0000;
    Effect2a_LinkZ = 0x180000;
    unsigned char* const q = S() + 0xC;
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3] = {Sprite_Objects[0].x, Sprite_Objects[0].z, Sprite_Objects[0].height};
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(q));
    point[0] = Sprite_Objects[1].x;
    point[1] = Sprite_Objects[1].z;
    point[2] = Sprite_Objects[1].height;
    float other[3];
    SH_CALL(EffectGte_ProjectPoint)(point, other);
    SetF(q, X87Add(other[0], F(q)));
    SetF(q + 4, X87Add(other[1], F(q + 4)));
    const float depth = X87Add(other[2], F(q + 8));
    SetWord(q + 0xC, 0);
    SetWord(q + 0xE, 0);
    SetF(q + 8, depth);
    const double scale = D(At(at::kScreenScale));
    SetD(q, D(q) * scale);
    SetD(q + 4, D(q + 4) * scale);
    SH_CALL(Msg_OpenScript)(0x14);
    Field_Request = 2;
    S()[9] = 0x32;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x200);
}

// original 0x472530 (state 1): the radius word +0x18 up 2 while +9 is above 0x19
// (unsigned), else down 2; the angle +0x1A up 0x40; the rays drawn (the record +
// 0xC taken on entry); +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind2D_Spin(void) {
    unsigned char* const s = S();
    unsigned char* const q = s + 0xC;
    AddWord(q + 0xC, s[9] > 0x19 ? 2u : 0xFFFEu);
    AddWord(q + 0xE, 0x40);
    SH_CALL(EffectKind2D_DrawRays)(q);
    CountDown(-1);
}

// original 0x472580 (state 2): the words +0x18 / +0x1A = 0, +9 = 0x5A, +1 up,
// Sound_PlayEffect(0x201).
extern "C" void __cdecl EffectKind2D_Pause(void) {
    unsigned char* const q = S() + 0xC;
    SetWord(q + 0xC, 0);
    SetWord(q + 0xE, 0);
    S()[9] = 0x5A;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x201);
}

// original 0x4725B0 (state 3): the radius +0x18 up one, held at 0x40 (signed);
// the angle +0x1A up 0x40; the rings drawn; +9 down - at 0 the drops cleared,
// the level word +0x1C = 0, +9 = 0 and +1 up.
extern "C" void __cdecl EffectKind2D_Open(void) {
    unsigned char* const s = S();
    AddWord(s + 0x18, 1);
    unsigned char* const q = s + 0xC;
    if (SW(s + 0x18) > 0x40) SetWord(q + 0xC, 0x40);
    AddWord(q + 0xE, 0x40);
    SH_CALL(EffectKind2D_DrawRings)(q);
    unsigned char* t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    if (S()[9] != 0) return;
    SH_CALL(EffectDrops_Clear)();
    SetWord(q + 0x10, 0);
    S()[9] = 0;
    NextState();
}

// original 0x472620 (state 4): the angle +0x1A up 0x40, the level +0x1C up 2;
// +9 up, at 0xF Sound_PlayEffect(0x202); the rings drawn, the drops moved, the
// curtain drawn; on odd frames a free drop (EffectDrops_FindFree) launched;
// once the level is above 0x100 (signed), +9 = 7 and +1 up.
extern "C" void __cdecl EffectKind2D_Pour(void) {
    unsigned char* const s = S();
    AddWord(s + 0x1A, 0x40);
    AddWord(s + 0x1C, 2);
    unsigned char* const q = s + 0xC;
    unsigned char* const t = S();
    t[9] = static_cast<unsigned char>(t[9] + 1);
    if (S()[9] == 0xF) SH_CALL(Sound_PlayEffect)(0x202);
    SH_CALL(EffectKind2D_DrawRings)(q);
    SH_CALL(EffectDrops_Move)();
    SH_CALL(EffectKind2D_DrawCurtain)(q);
    if (Frame_Counter & 1u) {
        unsigned char* const drop = SH_CALL(EffectDrops_FindFree)();
        if (drop) SH_CALL(EffectDrops_Launch)(drop);
    }
    if (SW(q + 0x10) <= 0x100) return;
    S()[9] = 7;
    NextState();
}

// original 0x4726B0 (state 5): the curtain drawn; the radius +0x18 down 8; +9
// down - at 0 the counter byte 0x903848 = 0x31, +0x18 = 2, +0x1A = 0, +9 = 0xF,
// +1 up, Sound_PlayEffect(0x203).
extern "C" void __cdecl EffectKind2D_Close(void) {
    unsigned char* const q = S() + 0xC;
    SH_CALL(EffectKind2D_DrawCurtain)(q);
    AddWord(q + 0xC, 0xFFF8u);
    unsigned char* const t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    if (S()[9] != 0) return;
    At(::bof3::Img(at::kCounter0.v))[0] = 0x31;
    SetWord(q + 0xC, 2);
    SetWord(q + 0xE, 0);
    S()[9] = 0xF;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x203);
}

// original 0x472720 (state 6): the curtain drawn; the word +0x1A up 2 and the
// screen y (+0x10) up by it (y - the word, signed); drawn again; +9 down, at 0
// +1 up.
extern "C" void __cdecl EffectKind2D_Lift(void) {
    unsigned char* const q = S() + 0xC;
    SH_CALL(EffectKind2D_DrawCurtain)(q);
    AddWord(q + 0xE, 2);
    SetD(q + 4, D(q + 4) - static_cast<double>(SW(q + 0xE)));
    SH_CALL(EffectKind2D_DrawCurtain)(q);
    CountDown(-1);
}

// original 0x472770 (state 7): the byte 0x7DEE44 |= 2, the counter byte
// 0x903848 = 0x32, a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind2D_End(void) {
    At(::bof3::Img(at::kMessageBits.v))[0] = static_cast<unsigned char>(At(::bof3::Img(at::kMessageBits.v))[0] | 2);
    At(::bof3::Img(at::kCounter0.v))[0] = 0x32;
    SH_CALL(Effect_Release)();
}

// original 0x472790 (0x181 bytes): a draw mode (page 0x380, 0x100, transparency
// 1, not dithered) linked at kind 0x2D's cell (4, 0xC); eight semi-transparent
// Gouraud lines at the packet cursor from the screen point (x, y copied, the
// depth for both ends) to (x + cos(u) * r sar s, y + sin(u) * r sar s), u = the
// angle word + 0x200 k (its low word), r the radius word (signed), s 12 on even
// k and 13 on odd; grey 0x80 at the centre, 0 at the tip, each linked (4,
// 0x24). Then EffectKind2D_DrawDisc and the draw mode again.
extern "C" void __cdecl EffectKind2D_DrawRays(const unsigned char* q) {
    LinkedMode(1, 0);
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        const U u = ((k << 9) + Word(q + 0xE)) & 0xFFFFu;
        SH_CALL(Gpu_SetLineG2)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(q));
        SetUL(p + 0xC, UL(q + 4));
        const unsigned shift = (k & 1) + 12;
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u)));
        SetD(p + 0x18, I(Sar(c * static_cast<U>(SW(q + 0xC)), shift)) + D(q));
        const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u)));
        SetD(p + 0x1C, I(Sar(n * static_cast<U>(SW(q + 0xC)), shift)) + D(q + 4));
        CopyQuiet(p + 0x20, q + 8);
        CopyQuiet(p + 0x10, q + 8);
        Shade3(p + 4, 0x80);
        Shade3(p + 0x14, 0);
        SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x24);
    }
    SH_CALL(EffectKind2D_DrawDisc)(q);
    LinkedMode(1, 0);
}

// original 0x472920 (0x18C bytes): the draw mode linked (as the rays'); h = the
// radius word sar 2 (16 bits); when h is not negative, for each row i = -h..h a
// half width w = the low word of 0x5A7A90(h * h - i * i) (the square root,
// truncated) and two semi-transparent TILE_1s at the packet cursor: one at (x +
// w, y + i), grey 0x40, at the point's depth, linked (4, 0x14); a copy of it
// right after (not the cursor read again) at x - w, linked. Then the draw mode
// again.
extern "C" void __cdecl EffectKind2D_DrawDisc(const unsigned char* q) {
    LinkedMode(1, 0);
    const auto h = static_cast<std::int16_t>(static_cast<std::int16_t>(Word(q + 0xC)) >> 2);
    const auto minus = static_cast<std::int16_t>(-h);
    if (minus <= h) {
        const std::int32_t square = static_cast<std::int32_t>(h) * h;
        std::int32_t i = minus;
        std::int32_t count = static_cast<std::int32_t>(h) - minus + 1;
        do {
            const U root = SH_AT(U (__cdecl*)(long), at::kRootWord.v)(static_cast<long>(square - i * i));
            const std::int32_t w = static_cast<std::int16_t>(root);
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            const float fw = static_cast<float>(w);
            SetD(p + 8, static_cast<double>(w) + D(q));
            SetD(p + 0xC, static_cast<double>(i) + D(q + 4));
            SetUL(p + 0x10, UL(q + 8));
            Shade3(p + 4, 0x40);
            SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x14);
            unsigned char* const mirror = p + 0x14;
            std::memcpy(mirror, p, 0x14);
            SetD(mirror + 8, D(q) - static_cast<double>(fw));
            SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x14);
            ++i;
        } while (--count != 0);
    }
    LinkedMode(1, 0);
}

// original 0x472AB0 (0x2B4 bytes): a draw mode (0x380, 0x100, transparency 2)
// linked (4, 0xC); eight rings j = 0..7 between the radii j R / 8 and (j + 1) R
// / 8 (R the radius word, signed; signed divides toward zero, each an s16), each
// of sixteen semi-transparent Gouraud quads at the packet cursor: the points
// (x + cos(u) r sar 12, y + sin(u) r sar 13) - an ellipse half as tall - at
// (inner, A - 0x80 j + 0x100 n), (outer, A - 0x80 (j + 1) + 0x100 n) and the
// same two a step 0x100 on (A the angle word, each u its low word), shaded 0,
// 0, 0x40, 0x40, at the point's depth, linked (4, 0x44). Then the draw mode with
// transparency 1, linked (4, 0xC).
extern "C" void __cdecl EffectKind2D_DrawRings(const unsigned char* q) {
    LinkedMode(2, 0);
    for (U j = 0; j < 8; ++j) {
        const U radius = static_cast<U>(SW(q + 0xC));
        const U inner = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Div8(j * radius))));
        const U outer = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Div8((j + 1) * radius))));
        const U angle = Word(q + 0xE);
        U a1 = angle - 0x80 * j;
        U a2 = angle - 0x80 * (j + 1);
        auto Cos = [](U u) { return static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u & 0xFFFFu))); };
        auto Sin = [](U u) { return static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u & 0xFFFFu))); };
        for (unsigned n = 0; n < 16; ++n) {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetD(p + 8, I(Sar(Cos(a1) * inner, 12)) + D(q));
            SetD(p + 0xC, I(Sar(Sin(a1) * inner, 13)) + D(q + 4));
            SetD(p + 0x18, I(Sar(Cos(a2) * outer, 12)) + D(q));
            SetD(p + 0x1C, I(Sar(Sin(a2) * outer, 13)) + D(q + 4));
            a1 += 0x100;
            a2 += 0x100;
            SetD(p + 0x28, I(Sar(Cos(a1) * inner, 12)) + D(q));
            SetD(p + 0x2C, I(Sar(Sin(a1) * inner, 13)) + D(q + 4));
            SetD(p + 0x38, I(Sar(Cos(a2) * outer, 12)) + D(q));
            SetD(p + 0x3C, I(Sar(Sin(a2) * outer, 13)) + D(q + 4));
            CopyQuiet(p + 0x40, q + 8);
            CopyQuiet(p + 0x30, q + 8);
            CopyQuiet(p + 0x20, q + 8);
            CopyQuiet(p + 0x10, q + 8);
            Shade3(p + 4, 0);
            Shade3(p + 0x14, 0);
            Shade3(p + 0x24, 0x40);
            Shade3(p + 0x34, 0x40);
            SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x44);
        }
    }
    LinkedMode(1, 0);
}

// original 0x472D70 (0x16 bytes): every drop's word +0x14 = 0 (all free).
extern "C" void __cdecl EffectDrops_Clear(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i) SetWord(Pool(at::kDropStride, i) + 0x14, 0);
}

// original 0x472D90 (0x67 bytes): each drop alive (+0x14 not 0): drawn
// (EffectDrops_Draw); its screen y +4 down by the word +0xE (y - it, signed),
// +0xE = its old value + (+0x14's low bit), +0xC up one, +0x14 down one - at 0
// the drop launched again (EffectDrops_Launch).
extern "C" void __cdecl EffectDrops_Move(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        unsigned char* const r = Pool(at::kDropStride, i);
        if (rv::As<records::EffectDrop>(r)->frames == 0) continue;
        SH_CALL(EffectDrops_Draw)(r);
        const U speed = static_cast<std::uint16_t>(rv::As<records::EffectDrop>(r)->speed);
        const U left = rv::As<records::EffectDrop>(r)->frames;
        const double y = D(r + 4) - static_cast<double>(static_cast<std::int16_t>(speed));
        AddWord(r + 0xC, 1);
        rv::Store<std::uint32_t>(rv::As<records::EffectDrop>(r)->speed, (left & 1) + speed);
        SetD(r + 4, y);
        rv::Store<std::uint32_t>(rv::As<records::EffectDrop>(r)->frames, left - 1);
        if (((left - 1) & 0xFFFFu) == 0) SH_CALL(EffectDrops_Launch)(r);
    }
}

// original 0x472E00 (0xDF bytes): an opaque flat quad at the packet cursor
// (Gpu_SetPolyF4), grey 0x80: the column x .. x + the float 0x5C41C0, from y -
// the word +0xC (held at the word +0x10 at least) to y + +0xC (held at +0x12 at
// most) - each held by the x87 compare of the unrounded sum with the bound
// (fcomp: an unordered compare holds the top, not the bottom) - at the drop's
// depth +8; linked at kind 0x2D's cell (4, 0x38).
extern "C" void __cdecl EffectDrops_Draw(const unsigned char* drop) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    const double top = D(drop + 4) - static_cast<double>(static_cast<std::int32_t>(rv::As<records::EffectDrop>(drop)->half_length));
    SetD(p + 0x18, top);
    SetD(p + 0xC, top);
    const float least = static_cast<float>(static_cast<std::int32_t>(rv::As<records::EffectDrop>(drop)->top));
    if (!(top >= static_cast<double>(least))) {
        SetF(p + 0x18, least);
        SetF(p + 0xC, least);
    }
    const double bottom = static_cast<double>(static_cast<std::int32_t>(rv::As<records::EffectDrop>(drop)->half_length)) + D(drop + 4);
    SetD(p + 0x30, bottom);
    SetD(p + 0x24, bottom);
    const float most = static_cast<float>(static_cast<std::int32_t>(rv::As<records::EffectDrop>(drop)->bottom));
    if (bottom > static_cast<double>(most)) {
        SetF(p + 0x30, most);
        SetF(p + 0x24, most);
    }
    CopyQuiet(p + 0x20, drop);
    CopyQuiet(p + 8, drop);
    const double right = D(drop) + D(At(at::kDropWidth));
    SetD(p + 0x2C, right);
    SetD(p + 0x14, right);
    CopyQuiet(p + 0x34, drop + 8);
    CopyQuiet(p + 0x28, drop + 8);
    CopyQuiet(p + 0x1C, drop + 8);
    CopyQuiet(p + 0x10, drop + 8);
    Shade3(p + 4, 0x80);
    SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x38);
}

// original 0x472EE0 (0x1B bytes): the first drop whose word +0x14 is 0, or 0.
extern "C" unsigned char* __cdecl EffectDrops_FindFree(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i)
        if (Word(Pool(at::kDropStride, i) + 0x14) == 0) return Pool(at::kDropStride, i);
    return nullptr;
}

// original 0x472F00 (0x1F6 bytes): a draw mode (0x380, 0x100, transparency 1,
// dithered) linked (4, 0xC); eight semi-transparent Gouraud quads at the packet
// cursor over u = 0, 0x100, .. 0x800: at u and the next, a top vertex (x + cos(u)
// r sar 12, 0.0) and a bottom one (the same x, y + sin(u) r sar 13), r the radius
// word (signed), at the point's depth; the shade the level word +0x10 clamped to
// 0..0xFF, the first pair's before it moves by 4 (up for quads 0 and 1, down
// after), the second pair's after (16 bits, kept from quad to quad); linked (4,
// 0x44). Then the draw mode not dithered, linked (4, 0xC).
extern "C" void __cdecl EffectKind2D_DrawCurtain(const unsigned char* q) {
    LinkedMode(1, 1);
    auto level = static_cast<std::uint16_t>(Word(q + 0x10));
    auto Clamp = [](std::uint16_t v) {
        const auto s = static_cast<std::int16_t>(v);
        return static_cast<unsigned char>(s > 0xFF ? 0xFF : s < 0 ? 0 : v);
    };
    U next = 0;   // the original's angle slot, stepped whole
    U u = 0;      // what Math_* are handed: its low word
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u)));
            const double x = I(Sar(c * static_cast<U>(SW(q + 0xC)), 12)) + D(q);
            SetUL(p + 0xC, 0);
            SetD(p + 0x18, x);
            SetD(p + 8, x);
            const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u)));
            SetD(p + 0x1C, I(Sar(n * static_cast<U>(SW(q + 0xC)), 13)) + D(q + 4));
        }
        Shade3(p + 4, Clamp(level));
        Shade3(p + 0x14, Clamp(level));
        level = static_cast<std::uint16_t>(k < 2 ? level + 4 : level - 4);
        next += 0x100;
        u = next & 0xFFFFu;
        {
            const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u)));
            const double x = I(Sar(c * static_cast<U>(SW(q + 0xC)), 12)) + D(q);
            SetUL(p + 0x2C, 0);
            SetD(p + 0x38, x);
            SetD(p + 0x28, x);
            const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u)));
            SetD(p + 0x3C, I(Sar(n * static_cast<U>(SW(q + 0xC)), 13)) + D(q + 4));
        }
        CopyQuiet(p + 0x40, q + 8);
        CopyQuiet(p + 0x30, q + 8);
        CopyQuiet(p + 0x20, q + 8);
        CopyQuiet(p + 0x10, q + 8);
        Shade3(p + 0x24, Clamp(level));
        Shade3(p + 0x34, Clamp(level));
        SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x44);
    }
    LinkedMode(1, 0);
}

// original 0x473100 (0x92 bytes): the drop at the record's screen point (+0xC,
// Sprite_Current read on entry): an angle Rand & 0xFC0 and a distance Rand & 0x38;
// x = (cos * d sar 12) + the point's x, y = (sin * d sar 13) + its y (floats), the
// depth copied; the word +0x12 (its floor) = _ftol(y)'s low word, +0x10, +0xC,
// +0xE = 0, +0x14 = 0x20 frames.
extern "C" void __cdecl EffectDrops_Launch(unsigned char* drop) {
    const unsigned char* const q = S() + 0xC;
    const U angle = static_cast<U>(SH_CALL(Rand)()) & 0xFC0u;
    const U distance = static_cast<U>(SH_CALL(Rand)()) & 0x38u;
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
    SetD(drop, I(Sar(c * distance, 12)) + D(q));
    const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
    SetD(drop + 4, I(Sar(n * distance, 13)) + D(q + 4));
    SetUL(drop + 8, UL(q + 8));
    rv::Store<std::uint32_t>(rv::As<records::EffectDrop>(drop)->bottom, Ftol(D(drop + 4)));
    rv::Store<std::uint32_t>(rv::As<records::EffectDrop>(drop)->top, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectDrop>(drop)->half_length, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectDrop>(drop)->speed, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectDrop>(drop)->frames, 0x20);
}

void EffectKind2d_Inject() {
    BOF3_INJECT(EffectKind2D_Run);
    BOF3_INJECT(EffectKind2D_Start);
    BOF3_INJECT(EffectKind2D_Spin);
    BOF3_INJECT(EffectKind2D_Pause);
    BOF3_INJECT(EffectKind2D_Open);
    BOF3_INJECT(EffectKind2D_Pour);
    BOF3_INJECT(EffectKind2D_Close);
    BOF3_INJECT(EffectKind2D_Lift);
    BOF3_INJECT(EffectKind2D_End);
    BOF3_INJECT(EffectKind2D_DrawRays);
    BOF3_INJECT(EffectKind2D_DrawDisc);
    BOF3_INJECT(EffectKind2D_DrawRings);
    BOF3_INJECT(EffectDrops_Clear);
    BOF3_INJECT(EffectDrops_Move);
    BOF3_INJECT(EffectDrops_Draw);
    BOF3_INJECT(EffectDrops_FindFree);
    BOF3_INJECT(EffectKind2D_DrawCurtain);
    BOF3_INJECT(EffectDrops_Launch);
}

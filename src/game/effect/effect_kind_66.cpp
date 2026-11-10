// Moved here by tools/move_funcs.py from effect_2f.cpp, effect_2g.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_66.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect/effect_kind_66_callees.h"
#include "game/effect_2f.h"
#include "game/effect_2f_callees.h"
#include "game/effect_2g_callees.h"
#include "game/effect_gte.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_66::at;

}  // namespace

namespace {

using U = std::uint32_t;

using LD = long double;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }

U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// --- x87 as the original has it -------------------------------------------------
// `fld dword [p]`: the float widened (a signalling NaN comes out quiet, as fld
// makes it).
LD X(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return static_cast<LD>(f);
}

// `fild dword`: an int widened, exactly.
LD XI(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }

// `fstp dword [p]`: rounded to a float at the control word's rounding.
void Fstp(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// The CRT's _ftol 0x5B9550 on st(0): truncation to 64 bits in a copy of the
// control word, the low dword kept; NaN and out-of-range answer the integer
// indefinite 0x8000000000000000, whose low dword is 0.
U Ftol(LD v) {
    if (!(v > -9223372036854775809.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Jump(const char* who, U table, unsigned entries, unsigned index) {
    if (index >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2f.md section 2)",
                    who, index, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * index))))();
}

// A Sprite_ObjectsExtra record by the dword +0x18: a Fatal past the four.
unsigned char* Extra(U index, const char* who) {
    if (index >= at::kExtras)
        bof3::Fatal("%s: +0x18 is %u, past the 4 records of Sprite_ObjectsExtra - the original reads past them "
                    "(docs/effect_2f.md section 2)",
                    who, (unsigned)index);
    return rv::Bytes(&Sprite_ObjectsExtra[index]);
}

// Gpu_GetTPage(tp, abr, x, y), then the draw mode of that page at the packet
// cursor (dfe 0, dtd as given), committed to slot `slot` (0xC bytes).
void DrawMode(unsigned tp, unsigned abr, int x, int y, int dtd, unsigned slot) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(tp, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

U ULEffect2g(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect2g(U a) { return ULEffect2g(At(a)); }

U W(U a) { return Word(At(a)); }

unsigned char& B(U a) { return *At(a); }

std::int32_t S16Effect2g(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

void StoreFloat(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);   // fild dword; fstp dword
    std::memcpy(p, &f, sizeof f);
}

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

// The growing / shrinking outline of the window at frame count b (+9):
// Window_DrawOutline(x, y, w, h), w = 25 b, h = 6 b, centred on the window's
// rectangle - x = (0xC8 - w) / 2 + the dword 0x654750 (both its words), y =
// (0x30 - h) / 2 with the word 0x654752 added to its low half. The original
// pushes w and h as the whole registers it multiplied, whose upper halves hold
// a caller's leftover; Window_DrawOutline reads their low bytes (the fuzz
// masks them so).
void Outline(unsigned b) {
    const U w = 25u * b, h = 6u * b;
    const U qy = static_cast<U>((0x30 - S16Effect2g(h)) / 2);
    const U y = (qy & 0xFFFF0000u) | ((qy + W(::bof3::Img(at::kKind66Rect.v) + 2)) & 0xFFFFu);
    const U x = static_cast<U>((0xC8 - S16Effect2g(w)) / 2) + ULEffect2g(::bof3::Img(at::kKind66Rect.v));
    SH_CALL(Window_DrawOutline)(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h));
}

// The window at its rectangle and the message's step, when +6 is set.
void Window() {
    if (S()[6] == 0) return;
    SH_CALL(Window_DrawFrame)(static_cast<int>(W(::bof3::Img(at::kKind66Rect.v))), static_cast<int>(W(::bof3::Img(at::kKind66Rect.v) + 2)),
                              B(::bof3::Img(at::kKind66Rect.v) + 4), B(::bof3::Img(at::kKind66Rect.v) + 6));
    SH_CALL(EffectKind66_Message)();
}

}  // namespace

// original 0x47D600 (hidden in 0x47D4F0's recorded extent): jmp
// [EffectKind53_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind53_Run(void) {
    Jump("EffectKind53_Run", AddressOf(EffectKind53_States), EffectKind53_States_count, S()[1]);
}

// original 0x47D620 (state 0): +0x54 = the dword +0x54 of the
// Sprite_ObjectsExtra record the dword +0x18 names; +9 = 0, +1 = 1.
extern "C" void __cdecl EffectKind53_Start(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->frames, rv::As<records::SpriteObject>(Extra(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18), "EffectKind53_Start"))->frames);
    s[9] = 0;
    s[1] = 1;
}

// original 0x47D650 (0x251 bytes; PSX twin 0x801F2D20 by callers, unnamed in
// the sibling): Sprite_Current set to the Sprite_ObjectsExtra record +0x18
// names (the effect record kept); the record's point = that sprite's (+0x34,
// +0x38, +0x3C); when the sprite's +9 is not 0 the record's +9 down (the
// sprite's +0x14 below 0) or up. The texture window (0, 0xF0, 0x10, 0x10) at
// the sprite; an FT4 at the packet cursor (read once): the sprite's point plus
// MoveCmd_AttachOffset(the record's +0x1C) turned into a camera vector
// ((x sar 9) - 0x4000, (z sar 9) - 0x4000, -((height word + dy) / 2)) and
// Gte_RotTransPers'd to the quad's last vertex (its answer to the record's
// +0x60), Gte_PrimDepthFlat4_10; the quad from the top of the screen (y 0)
// down to that point's y (240.0f when it is 240 or more, at 0x5C41E8), x - 8
// to x + 8 (8.0f at 0x5C41CC); Sprite_Current put back; the CLUT (0, 0x1E3), the
// page (0, 1, 0x2C0, 0x100); u 0 / 0x10, v the record's +9 & 0x15 at the top and
// ftol(that + y) at the bottom; Gpu_SetShadeTex(0). By the depth +0x60: past
// 0x220 shaded 0x80, opaque; past 0x1E0 shaded (depth's low byte + 0x20) << 1,
// semi-transparent; either linked at the record's point (0x48); nearer, not
// linked. Then the texture window (0, 0, 0x100, 0x100) at the record.
extern "C" void __cdecl EffectKind53_Beam(void) {
    unsigned char* const self = S();
    auto* const selfo = rv::As<records::EffectObject>(self);
    unsigned char* const sprite = Extra(static_cast<std::uint32_t>(rv::As<records::EffectObject>(self)->work_18), "EffectKind53_Beam");
    auto* const spriteo = rv::As<records::SpriteObject>(sprite);
    Sprite_Current = sprite;
    selfo->x = static_cast<std::uint32_t>(spriteo->x);
    selfo->z = UL(S() + 0x38);
    selfo->height = UL(S() + 0x3C);
    const unsigned char* s = S();
    if (s[9] != 0) {
        unsigned char c = self[9];
        c = static_cast<unsigned char>(I(UL(s + 0x14)) < 0 ? c - 1 : c + 1);
        self[9] = c;
    }
    SH_CALL(EffectKind53_TexWindow)(0, 0xF0, 0x10, 0x10);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    long offset[3] = {0, 0, 0};
    SH_CALL(MoveCmd_AttachOffset)(offset, self[0x1C]);
    s = S();
    short vertex[4] = {0, 0, 0, 0};
    vertex[0] = static_cast<short>(Sar(UL(s + 0x34) + static_cast<U>(offset[0]), 9) - 0x4000u);
    vertex[1] = static_cast<short>(Sar(UL(s + 0x38) + static_cast<U>(offset[1]), 9) - 0x4000u);
    const U height = SW(s + 0x3E) + static_cast<U>(offset[2]);
    vertex[2] = static_cast<short>(0u - Sar(height + (height >> 31), 1));
    long depth_cue = 0;
    selfo->depth = static_cast<U>(SH_CALL(Gte_RotTransPers)(vertex, reinterpret_cast<unsigned long*>(prim + 0x38),
                                                                &depth_cue));
    SH_CALL(Gte_PrimDepthFlat4_10)(prim);
    LD t = X(prim + 0x38) - 8.0L;
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x1C, 0);
    Fstp(prim + 8, t);
    Fstp(prim + 0x28, t);
    t = X(prim + 0x38) + 8.0L;
    Fstp(prim + 0x18, t);
    Fstp(prim + 0x38, t);
    U y;
    if (!(X(prim + 0x3C) >= 240.0L)) {
        y = UL(prim + 0x3C);
    } else {
        y = 0x43700000u;
        SetUL(prim + 0x3C, y);
    }
    SetUL(prim + 0x2C, y);
    Sprite_Current = self;
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    prim[0x14] = 0;
    s = S();
    prim[0x24] = 0x10;
    prim[0x15] = static_cast<unsigned char>(s[9] & 0x15);
    prim[0x25] = static_cast<unsigned char>(s[9] & 0x15);
    prim[0x34] = 0;
    prim[0x35] = static_cast<unsigned char>(Ftol(XI(s[9] & 0x15u) + X(prim + 0x2C)));
    prim[0x44] = 0x10;
    prim[0x45] = static_cast<unsigned char>(Ftol(XI(S()[9] & 0x15u) + X(prim + 0x2C)));
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    s = S();
    const std::int32_t depth = static_cast<std::int32_t>(UL(s + 0x60));
    if (depth > 0x1E0) {
        unsigned semi = 0;
        unsigned char c = 0x80;
        if (depth <= 0x220) {
            c = static_cast<unsigned char>(static_cast<unsigned char>(s[0x60] + 0x20) << 1);
            semi = 1;
        }
        prim[4] = prim[5] = prim[6] = c;
        SH_CALL(Gpu_SetSemiTrans)(prim, semi);
        const unsigned char* const t2 = S();
        SH_CALL(MapView_LinkPrimAt)(UL(t2 + 0x34), UL(t2 + 0x38), 0, 0x48);
    }
    SH_CALL(EffectKind53_TexWindow)(0, 0, 0x100, 0x100);
}

// original 0x47D8B0 (catalog part 7, in no group): an 8-byte rect (x, y, w, h,
// each the argument's low word) taken at the packet cursor (the cursor up 8),
// then 0x5A7840(cursor, rect): a primitive of code 0xF0 holding the rect's
// address (a texture window, as Window_DrawFrame 0x595C50 makes one), linked at
// Sprite_Current's point (+0x34, +0x38), 0xC bytes.
extern "C" void __cdecl EffectKind53_TexWindow(unsigned x, unsigned y, unsigned w, unsigned h) {
    unsigned char* const rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, x);
    SetWord(rect + 2, y);
    SetWord(rect + 4, w);
    SetWord(rect + 6, h);
    SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), at::kPrimRect.v)(Gfx_PacketNext, rect);
    const unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 0, 0xC);
}

// original 0x47D910 (hidden in 0x47D8B0's recorded extent): jmp
// [EffectKind56_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind56_Run(void) {
    Jump("EffectKind56_Run", AddressOf(EffectKind56_States), EffectKind56_States_count, S()[1]);
}

// original 0x47D930 (state 0): the record placed; +9 = 0, +1 = 1.
extern "C" void __cdecl EffectKind56_Start(void) {
    SH_CALL(EffectKind56_Place)();
    S()[9] = 0;
    S()[1] = 1;
}

// original 0x47D950 (state 1): once the counter byte 0x90384B is 0xA, +1 = 2.
extern "C" void __cdecl EffectKind56_Wait(void) {
    if (At(::bof3::Img(at::kCounter3.v))[0] == 0xA) S()[1] = 2;
}

// original 0x47D970 (state 2): +1 = 3.
extern "C" void __cdecl EffectKind56_Arm(void) { S()[1] = 3; }

// original 0x47D980 (state 3): a draw mode (Gpu_GetTPage(1, 1, 0x140, 0), dtd
// 1, committed 0xC in slot 3); the record placed; its red +0x5D = 0; from h =
// its height +0x3C - ((Frame_Counter & 0xF) << 20), while the ground +0x14 is
// below h (signed): a cross at (x, z, h), semi-transparent, the red up 0x2A (at
// most 0x80), h down one cell (2^24); then the red 0xFF and an opaque cross on
// the ground; once the counter byte 0x90384B is 0xE, +1 = 1. The step repeats
// h every 256 steps, so a walk that has not ended by then never ends (the
// ground within 2^24 of -2^31): ours aborts there.
extern "C" void __cdecl EffectKind56_Markers(void) {
    DrawMode(1, 1, 0x140, 0, 1, 3);
    SH_CALL(EffectKind56_Place)();
    const U phase = Frame_Counter & 0xFu;
    S()[0x5D] = 0;
    unsigned char* s = S();
    U h = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) - (phase << 20);
    for (unsigned steps = 0; I(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y)) < I(h); ++steps) {
        if (steps == 0x100)
            bof3::Fatal("EffectKind56_Markers: the ground 0x%X is below every height the walk from 0x%X reaches - the "
                        "original loops for ever (docs/effect_2f.md section 2)",
                        (unsigned)static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y), (unsigned)h);
        SH_CALL(EffectKind56_DrawCross)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z, static_cast<long>(h), 1);
        s = S();
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0x2A);
        if (s[0x5D] > 0x80) s[0x5D] = 0x80;
        h -= 0x1000000u;
    }
    s[0x5D] = 0xFF;
    s = S();
    SH_CALL(EffectKind56_DrawCross)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z, rv::As<records::EffectObject>(s)->step_y, 0);
    if (At(::bof3::Img(at::kCounter3.v))[0] == 0xE) S()[1] = 1;
}

// original 0x47DA60: the record's point = Sprite_ObjectsExtra[0]'s (+0x34,
// +0x38, +0x3C); MapView_HeightScale = 1; the ground +0x14 =
// MapView_GroundAt(x, z)'s low word << 16. The callers push an argument it
// never reads.
extern "C" void __cdecl EffectKind56_Place(void) {
    const unsigned char* const extra = At(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x34);
    rv::Store<U>(rv::Current<records::EffectObject>()->x, UL(extra));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, UL(extra + 4));
    rv::Store<U>(rv::Current<records::EffectObject>()->height, UL(extra + 8));
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    MapView_HeightScale = 1;
    const long ground = SH_CALL(MapView_GroundAt)(so->x, so->z);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, S16(static_cast<U>(ground)) << 16);
}

// original 0x47DAC0 (0x111 bytes): two flat lines at the packet cursor, a cross
// at the world point (x, z, height): from (x + 0x4000, z) to (x - 0x4000, z),
// each end EffectGte_ProjectPoint'd into the line (+8, +0x14), coloured (the
// record's +0x5D, 0, 0), Gpu_SetSemiTrans(`semi`'s low byte), committed 0x20 in
// slot 3; then from (x, z + 0x4000) to (x, z - 0x4000), coloured likewise,
// committed, and only then Gpu_SetSemiTrans(`semi`) on it.
extern "C" void __cdecl EffectKind56_DrawCross(long x, long z, long height, unsigned semi) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const U abe = semi & 0xFFu;
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    long point[3] = {static_cast<long>(ux + 0x4000u), z, height};
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 8));
    point[0] = static_cast<long>(ux - 0x4000u);
    point[1] = z;
    point[2] = height;
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x14));
    prim[4] = S()[0x5D];
    prim[5] = 0;
    prim[6] = 0;
    SH_CALL(Gpu_SetSemiTrans)(prim, abe);
    SH_CALL(Gfx_CommitPrim)(3, 0x20);
    prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    point[0] = x;
    point[1] = static_cast<long>(uz + 0x4000u);
    point[2] = height;
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 8));
    point[0] = x;
    point[1] = static_cast<long>(uz - 0x4000u);
    point[2] = height;
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x14));
    const unsigned char red = S()[0x5D];
    prim[5] = 0;
    prim[4] = red;
    prim[6] = 0;
    SH_CALL(Gfx_CommitPrim)(3, 0x20);
    SH_CALL(Gpu_SetSemiTrans)(prim, abe);
}

// original 0x47DBE0 (Effect_KindHandlers[0x66], hidden in E2F's 0x47DAC0):
// jmp [EffectKind66_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind66_Run(void) {
    Dispatch("EffectKind66_Run", AddressOf(EffectKind66_States), EffectKind66_States_count, 1);
}

// original 0x47DC00 (state 0): no message (0x7DEE48 = 0xFFFF), +9 = 0, +1 = 1.
extern "C" void __cdecl EffectKind66_Start(void) {
    SetWord(At(::bof3::Img(at::kMessageWord.v)), 0xFFFF);
    S()[9] = 0;
    S()[1] = 1;
}

// original 0x47DC20 (state 1): +9 up; with +6 set the outline at that count
// (Sprite_Current read again after it); at a count of 8, +1 = 2.
extern "C" void __cdecl EffectKind66_Open(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[6] != 0) {
        Outline(s[9]);
        s = S();
    }
    if (s[9] == 8) s[1] = 2;
}

// original 0x47DCA0 (state 2): the window and the message (with +6), then
// piece 0 at place 0 and piece 2 at place 1.
extern "C" void __cdecl EffectKind66_Show2(void) {
    Window();
    SH_CALL(EffectKind66_DrawPiece)(0, 0);
    SH_CALL(EffectKind66_DrawPiece)(2, 1);
}

// original 0x47DCF0 (state 3): as state 2, piece 1 at place 0 and piece 2 at
// place 2.
extern "C" void __cdecl EffectKind66_Show3(void) {
    Window();
    SH_CALL(EffectKind66_DrawPiece)(1, 0);
    SH_CALL(EffectKind66_DrawPiece)(2, 2);
}

// original 0x47DD40 (state 4): as state 2, and piece 3 at place 1.
extern "C" void __cdecl EffectKind66_Show4(void) {
    Window();
    SH_CALL(EffectKind66_DrawPiece)(0, 0);
    SH_CALL(EffectKind66_DrawPiece)(2, 1);
    SH_CALL(EffectKind66_DrawPiece)(3, 1);
}

// original 0x47DDA0 (state 5): with +6 the outline at +9 (Sprite_Current read
// again after it); +9 at 0: a tail jump to Effect_Release; else +9 down.
extern "C" void __cdecl EffectKind66_Close(void) {
    unsigned char* s = S();
    if (s[6] != 0) {
        Outline(s[9]);
        s = S();
    }
    if (s[9] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
}

// original 0x47DE10 (called by states 2..4): a message set (0x7DEE48 not
// 0xFFFF): MsgBox_FrameTask (its answer unread); then MsgBoxState's first byte
// 2 -> 7 and its +6 = 0xA; 7 -> +6 = 0xA.
extern "C" void __cdecl EffectKind66_Message(void) {
    if (W(::bof3::Img(at::kMessageWord.v)) == 0xFFFF) return;
    SH_CALL(MsgBox_FrameTask)();
    const unsigned char state = B(::bof3::Img(bof3::addr::MsgBoxState));
    if (state == 2) {
        B(::bof3::Img(bof3::addr::MsgBoxState)) = 7;
        Msgbox_Delay = 0xA;
        return;
    }
    if (state == 7) Msgbox_Delay = 0xA;
}

// original 0x47DE50 (called by states 2..4; cdecl): a POLY_FT4 at
// Gfx_PacketNext - the corners the place record (0x654770 + 8 * place) gives
// as floats (x, y, x + w, y + h), the texture corners the piece record
// (0x654788 + 4 * piece) gives, shade 0x80, CLUT Gpu_GetClut(0, 0x1E8), page
// Gpu_GetTPage(1, 0, 0x240, 0x100) - committed to slot 1 (0x48). The low byte
// of each argument is read; past the three places or the four pieces the
// original reads the .data after them: ours aborts.
extern "C" void __cdecl EffectKind66_DrawPiece(unsigned piece, unsigned place) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    const unsigned r = place & 0xFF, t = piece & 0xFF;
    if (r >= at::kKind66PlaceCount || t >= at::kKind66PieceCount)
        bof3::Fatal("EffectKind66_DrawPiece: piece %u, place %u - past the four pieces of 0x654788 or the three places of "
                    "0x654770 (docs/effect_2g.md section 6)",
                    t, r);
    const U place_at = ::bof3::Img(at::kKind66Places.v) + 8 * r;
    const std::int32_t x = S16Effect2g(W(place_at)), y = S16Effect2g(W(place_at + 2));
    const std::int32_t w = S16Effect2g(W(place_at + 4)), h = S16Effect2g(W(place_at + 6));
    StoreFloat(p + 8, x);
    StoreFloat(p + 0xC, y);
    StoreFloat(p + 0x18, x + w);
    StoreFloat(p + 0x1C, y);
    StoreFloat(p + 0x28, x);
    StoreFloat(p + 0x2C, y + h);
    StoreFloat(p + 0x38, x + w);
    StoreFloat(p + 0x3C, y + h);
    const U uv = ::bof3::Img(at::kKind66Pieces.v) + 4 * t;
    const unsigned char u = B(uv), v = B(uv + 1), du = B(uv + 2), dv = B(uv + 3);
    p[0x14] = u;
    p[0x15] = v;
    p[0x24] = static_cast<unsigned char>(u + du);
    p[0x25] = v;
    p[0x34] = u;
    p[0x35] = static_cast<unsigned char>(v + dv);
    p[0x44] = static_cast<unsigned char>(u + du);
    p[4] = 0x80;
    p[0x45] = static_cast<unsigned char>(v + dv);
    p[5] = 0x80;
    p[6] = 0x80;
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, SH_CALL(Gpu_GetClut)(0, 0x1E8));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, SH_CALL(Gpu_GetTPage)(1, 0, 0x240, 0x100));
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x47DFD0 (EffectKind18_States[0x20], hidden in 0x47DE50):
// jmp [EffectKind18Sub20_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub20_Run(void) {
    Dispatch("EffectKind18Sub20_Run", AddressOf(EffectKind18Sub20_States), EffectKind18Sub20_States_count, 2);
}

// original 0x47DFF0 (sub-state 0): story flag 0x2A set: the height +0x3C =
// -0x180 and +2 = 3; clear: +0x3C = -0x90 and +2 up. Then the draw (a tail jump).
extern "C" void __cdecl EffectKind18Sub20_Start(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kSub20Flag)) {
        rv::Current<records::EffectObject>()->height = 0xFFFFFE80u;
        S()[2] = 3;
    } else {
        rv::Current<records::EffectObject>()->height = 0xFFFFFF70u;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub20_Draw)();
}

// original 0x47E040 (sub-state 1): Cond_ByteFE = 1; once story flag 0x2A is
// set, sound 0x202 and +2 up (read after the sound). Nothing drawn.
extern "C" void __cdecl EffectKind18Sub20_WaitSet(void) {
    Cond_ByteFE = 1;
    if (!SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kSub20Flag)) return;
    SH_CALL(Sound_PlayEffect)(0x202);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x47E070 (sub-state 2): Cond_ByteFE = 0; +0x3C down 8; at -0x180 or
// below, MoveCmd_TestFB(0x27, 0x2F) (its answer unread) and +2 up. Then the draw.
extern "C" void __cdecl EffectKind18Sub20_Lower(void) {
    unsigned char* s = S();
    Cond_ByteFE = 0;
    rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) - 8u;
    s = S();
    if (rv::As<records::EffectObject>(s)->height <= -0x180) {
        SH_CALL(MoveCmd_TestFB)(0x27, 0x2F);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub20_Draw)();
}

// original 0x47E0B0 (sub-state 3): Cond_ByteFE = 2; once story flag 0x2A is
// clear, MoveCmd_TestFB(0x27, 0x2F), sound 0x202 and +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub20_WaitClear(void) {
    Cond_ByteFE = 2;
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kSub20Flag)) return;
    SH_CALL(MoveCmd_TestFB)(0x27, 0x2F);
    SH_CALL(Sound_PlayEffect)(0x202);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x47E0F0 (sub-state 4): Cond_ByteFE = 0; +0x3C up 8; at -0x90 or
// above, +2 = 1. Then the draw (the jmp to 0x47E120, the code after it).
extern "C" void __cdecl EffectKind18Sub20_Raise(void) {
    unsigned char* s = S();
    Cond_ByteFE = 0;
    rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 8u;
    s = S();
    if (rv::As<records::EffectObject>(s)->height >= -0x90) s[2] = 1;
    SH_CALL(EffectKind18Sub20_Draw)();
}

// original 0x47E120 (the tail sub-states 0, 2 and 4 jump to; inside 0x47E0F0's
// extent): four rows (the z 0x300000 + 0x20000 i, i = 0..3) of two POLY_FT4s,
// shaded, their vertices built in Prim_VertexScratch - x -0x2940 (then -0x2A40
// for the second's far pair), y 0x100 i - 0x2740 / - 0x2840, z the height's
// low word +0x3C and 0x100 above it (Sprite_Current read after the primitive's
// set-up, each quad) - projected by Gte_RotTransPers4, depths by
// Gte_PrimDepths4_10, textures 0x23500125 and 0x23800126, each linked at
// (0x2C0000, z) with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub20_Draw(void) {
    constexpr U kNear = 0xFFFFD6C0u, kFar = 0xFFFFD5C0u;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    auto vertex = [v](unsigned i) { return reinterpret_cast<const short*>(v + 8 * i); };
    U row = 0;
    for (U z = 0x300000; z < 0x380000; z += 0x20000, ++row) {
        unsigned char* p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const U y0 = (row << 8) - 0x2740u, y1 = (row << 8) + 0xFFFFD7C0u;
        SetWord(v + 0x18, kNear);
        SetWord(v + 0x10, kNear);
        SetWord(v + 8, kNear);
        SetWord(v + 0x1A, y1);
        SetWord(v + 0xA, y1);
        unsigned char* s = S();
        SetWord(v, kNear);
        SetWord(v + 0x12, y0);
        SetWord(v + 2, y0);
        SetWord(v + 0xC, Word(s + 0x3C));
        SetWord(v + 4, Word(s + 0x3C));
        SetWord(v + 0x1C, Word(s + 0x3C) + 0x100u);
        SetWord(v + 0x14, Word(s + 0x3C) + 0x100u);
        long depth;
        SH_CALL(Gte_RotTransPers4)(vertex(0), vertex(1), vertex(2), vertex(3), reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)(0x23500125, p, 1);
        SH_CALL(MapView_LinkPrimAt)(0x2C0000, z, 0, 0x48);
        p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SetWord(v + 0x18, kFar);
        SetWord(v + 0x10, kFar);
        s = S();
        SetWord(v + 0x1C, rv::As<records::EffectObject>(s)->height_frac);
        SetWord(v + 0x14, rv::As<records::EffectObject>(s)->height_frac);
        SH_CALL(Gte_RotTransPers4)(vertex(3), vertex(1), vertex(2), vertex(0), reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)(0x23800126, p, 1);
        SH_CALL(MapView_LinkPrimAt)(0x2C0000, z, 0, 0x48);
    }
}

void EffectKind66FromEffect2f_Inject() {
    BOF3_INJECT(EffectKind53_Run);
    BOF3_INJECT(EffectKind53_Start);
    BOF3_INJECT(EffectKind53_Beam);
    BOF3_INJECT(EffectKind53_TexWindow);
    BOF3_INJECT(EffectKind56_Run);
    BOF3_INJECT(EffectKind56_Start);
    BOF3_INJECT(EffectKind56_Wait);
    BOF3_INJECT(EffectKind56_Arm);
    BOF3_INJECT(EffectKind56_Markers);
    BOF3_INJECT(EffectKind56_Place);
    BOF3_INJECT(EffectKind56_DrawCross);
}

void EffectKind66FromEffect2g_Inject() {
    BOF3_INJECT(EffectKind66_Run);
    BOF3_INJECT(EffectKind66_Start);
    BOF3_INJECT(EffectKind66_Open);
    BOF3_INJECT(EffectKind66_Show2);
    BOF3_INJECT(EffectKind66_Show3);
    BOF3_INJECT(EffectKind66_Show4);
    BOF3_INJECT(EffectKind66_Close);
    BOF3_INJECT(EffectKind66_Message);
    BOF3_INJECT(EffectKind66_DrawPiece);
    BOF3_INJECT(EffectKind18Sub20_Run);
    BOF3_INJECT(EffectKind18Sub20_Start);
    BOF3_INJECT(EffectKind18Sub20_WaitSet);
    BOF3_INJECT(EffectKind18Sub20_Lower);
    BOF3_INJECT(EffectKind18Sub20_WaitClear);
    BOF3_INJECT(EffectKind18Sub20_Raise);
    BOF3_INJECT(EffectKind18Sub20_Draw);
}

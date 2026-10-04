// Round fourteen group R3F (docs/rest_3f.md): the 48 functions of
// analysis/round14_cut.tsv's group R3F and the two states of kind 0x9E no list
// held (0x48F1E0, 0x48F300), 0x480210..0x49259C, each read with capstone to its
// last instruction (2026-10-04). What round thirteen's effect groups left in
// their bands: the states of kinds whose dispatchers they took, and the draws
// those and their neighbours call. Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind's dispatcher (E3A's, E4E's, E4F's) jumps
// or calls through its state table by +1.
//
//   kind 0x60   a grey line from (0x198000, 0x198000) whose ends slide 0x20000
//               a frame to 0x98000 (EffectKind60_DrawLine, E3A's)
//   kind 0x5F   (R3E's states) a disc of semi-transparent LINE_F2, one a row,
//               round a projected point, wobbling: EffectKind5F_DrawLineDisc
//   kind 0x69   (E3B's part 2) a chain of LINE_G2 down a wave: _DrawLines
//   kind 0x9C   (E4E's states 1..6) a trail of two quads and eight dots
//   kind 0x9E   a box outline that grows from the record's point, a flash of
//               its plate, a wall and a textured plate until the chapter's
//               count, the plate again and the outline shrinking (E4E's
//               four plate draws)
//   kinds 0xA1, 0xA3   the sprite the record's +0x4C points at drawn into VRAM
//               (0x340, 0x100) by a borrowed effect record, read back, and
//               every pixel not 0 made a particle - TILE_1s that blink, fly
//               apart or come together; 0xA3 waits for the count 0x29 and
//               sets it to 0x2B when done
//   kind 0xA2   a ring of 32 POLY_G4 between two radii that grow and fall
//   kind 0xA7   a glow (EffectKindA0_DrawGlow's on slot 7) that grows, holds
//               and shrinks
//   kind 0xA8   sixteen red bars across the screen started at random
//   kind 0xA9   a half disc from the screen's bottom centre
//   kind 0xAA   a red gradient over the whole frame that fades in and out
//               (drawn through DIV-0041's fill: docs/rest_3f.md section 2)
//   kind 0xAB   its state 1: the drops (E4F's) while Draw_PassFlags is set
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies; Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call. The
// x87 arithmetic is done in long double, as the originals' fild / fadd / fsub /
// fstp chains are (effect_4e.cpp's form). Where the original indexes past a
// table or its row loop could never end, ours aborts with a message
// (docs/rest_3f.md section 7).
#include "game/rest_3f.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3f::at;
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
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
// Twelve bytes moved as the original moves them (three dword movs).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

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

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

// Gpu_GetTPage(0, abr, x, y), then Gpu_SetDrawMode(cursor, 0, dtd, the page's
// low word, 0 - the fifth word the leftover of GetTPage's five pushes).
void SetMode(U abr, int x, int y, int dtd) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
}
// The same, committed 0xC to `slot`.
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    SetMode(abr, x, y, dtd);
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

// --- kinds 0xA1 / 0xA3 --------------------------------------------------------------

// The capture record kPixIndex names, checked against the table's three.
U PixRecord(const char* who) {
    const unsigned k = At(at::kPixIndex)[0];
    if (k >= at::kPixRecords)
        bof3::Fatal("%s: the capture record 0x%X is %u, past the %u of 0x%X - the original reads EffectKindA1_States' "
                    "code pointers as a rectangle (docs/rest_3f.md section 7)",
                    who, (unsigned)at::kPixIndex, k, at::kPixRecords, (unsigned)at::kPixTable);
    return at::kPixTable + 8 * k;
}
unsigned char* Particle(unsigned i) { return P(at::kParticles + i * at::kPartStride); }
constexpr unsigned kEffectRecords = 20;   // Effect_Objects, 20 of 0x80

// State 0 of kinds 0xA1 (0x490A80) and 0xA3 (0x491310, once the count is
// 0x29): a free effect record borrowed to draw the sprite +0x4C points at into
// VRAM (0x340, 0x100) - the rectangle cleared (0x80 x 0x100), the sprite's
// first 0x80 bytes copied into the record (rep movsd), made kind 0x3A in state
// 2, its +0x2E / +0x30 the capture record's offsets (kPixIndex = 1), its page
// set, +0x24 |= 0x88; with it current an animation set, Sprite_UpdateScreen,
// Effect_Release; Sprite_Current put back, +9 = 2, +1 up. No free record: +1
// up only. `a3` is kind 0xA3's form: the page always set and Sprite_SetAnimation(7).
void CaptureSprite(const char* who, bool a3) {
    const unsigned char k = SH_CALL(Effect_FindFree)();
    if (k == 0xFF) {
        NextState();
        return;
    }
    if (k >= kEffectRecords)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original copies a sprite past "
                    "Effect_Objects (docs/rest_3f.md section 7)",
                    who, (unsigned)k);
    unsigned char* const e = Effect_Objects + 0x80u * k;
    At(at::kPixIndex)[0] = 1;
    SH_CALL(Gfx_ClearRect)(at::kCaptureX, at::kCaptureY, 0x80, 0x100);
    {
        const unsigned char* const from = P(UL(S() + 0x4C));
        for (unsigned i = 0; i < 0x80; i += 4) SetUL(e + i, UL(from + i));
    }
    e[0] = 1;
    e[1] = 2;
    e[2] = 0;
    e[5] = 0x3A;
    SetWord(e + 0x2E, Word(P(PixRecord(who))));
    SetWord(e + 0x30, Word(P(PixRecord(who) + 2)));
    if (a3 || (e[0x24] & 1) != 0) {
        const U tp = SH_CALL(Gpu_GetTPage)(0, 0, at::kCaptureX, at::kCaptureY);
        e[0x25] = static_cast<unsigned char>(tp);
        e[0x26] = 0x80;
    }
    e[0x24] = static_cast<unsigned char>(e[0x24] | 0x88);
    unsigned char* const old = S();
    Sprite_Current = e;
    if (a3) {
        SH_CALL(Sprite_SetAnimation)(7);
    } else if ((e[0x24] & 1) != 0) {
        if (Word(e + 0x2C) == 0) {
            SH_CALL(Sprite_SetAnimationAt)(0x4D, 0x1C);
            S()[0x2A] = 0;
        } else {
            SH_CALL(Sprite_SetAnimationAt)(0x50, 0x24);
        }
    }
    SH_CALL(Sprite_UpdateScreen)();
    SH_CALL(Effect_Release)();
    Sprite_Current = old;
    old[9] = 2;
    NextState();
}

// State 1 of both (0x490BB0, 0x491410, the same code): +9 down; at 0 the
// capture record's width x height read back from VRAM (0x340, 0x100) into
// EffectKind30_Shards, the cursors put at its start and at the particles,
// the count 0, +9 = 0, +1 up.
void ReadBack(const char* who) {
    if (!CountDown()) return;
    const U rec = PixRecord(who);
    alignas(4) short rect[4] = {static_cast<short>(at::kCaptureX), static_cast<short>(at::kCaptureY),
                                static_cast<short>(Word(P(rec + 4))), static_cast<short>(Word(P(rec + 6)))};
    SH_AT(void (__cdecl*)(const short*, unsigned char*), at::kStoreImage)(rect, EffectKind30_Shards);
    unsigned char* const s = S();
    SetUL(At(at::kPixCursor), static_cast<U>(reinterpret_cast<std::uintptr_t>(EffectKind30_Shards)));
    SetUL(At(at::kPartCursor), at::kParticles);
    SetWord(At(at::kPartCount), 0);
    s[9] = 0;
    NextState();
}

// State 2 of both (0x490C50, 0x4914B0, the same code): the sprite's point
// (+0x4C's +0x34..) projected; the rows +9 * h / 2 .. (+9 + 1) * h / 2 of the
// read-back walked by the pixel cursor (w a row), every pixel not 0 a
// particle at the cursor: its pixel, x = screen x - dx + column, y = screen y -
// dy + row, the depth copied (fld / fstp), the count up. +9 up; at 2, +1 up.
void SplitPixels(const char* who) {
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    {
        alignas(4) long q[3];
        unsigned char* const sprite = P(UL(S() + 0x4C));
        q[0] = Long(sprite + 0x34);
        q[1] = Long(sprite + 0x38);
        q[2] = Long(sprite + 0x3C);
        SH_CALL(EffectGte_ProjectPoint)(q, Out(o));
    }
    const U rec = PixRecord(who);
    unsigned char* const s = S();
    const U h = Word(P(rec + 6));
    // imul dl: the low byte of (h >> 1) times +9 (the low bytes of the signed
    // and the unsigned products agree)
    unsigned row = static_cast<unsigned char>(((h >> 1) & 0xFFu) * s[9]);
    for (;;) {
        const std::int32_t bound = (SW(P(rec + 6)) >> 1) * (static_cast<std::int32_t>(s[9]) + 1);
        if (static_cast<std::int32_t>(row) >= bound) break;
        if (bound > 0xFF)
            bof3::Fatal("%s: +9 is %u, so the rows run to %d - the original's row is a byte and never reaches it "
                        "(docs/rest_3f.md section 7)",
                        who, (unsigned)s[9], (int)bound);
        if (SW(P(rec + 4)) > 0) {
            unsigned column = 0;
            do {
                unsigned char* const pixel = P(UL(At(at::kPixCursor)));
                const U v = Word(pixel);
                if (v != 0) {
                    SetWord(P(UL(At(at::kPartCursor))) + 2, v);
                    StF(P(UL(At(at::kPartCursor))) + 4, F(o) - I(static_cast<U>(SW(P(rec)))) + I(column));
                    StF(P(UL(At(at::kPartCursor))) + 8, F(o + 4) - I(static_cast<U>(SW(P(rec + 2)))) + I(row));
                    StF(P(UL(At(at::kPartCursor))) + 0xC, F(o + 8));
                    SetWord(At(at::kPartCount), Word(At(at::kPartCount)) + 1u);
                    SetUL(At(at::kPartCursor), UL(At(at::kPartCursor)) + at::kPartStride);
                }
                SetUL(At(at::kPixCursor), UL(At(at::kPixCursor)) + 2);
                column = (column + 1) & 0xFFu;
            } while (static_cast<std::int32_t>(column) < SW(P(rec + 4)));
        }
        row = (row + 1) & 0xFFu;
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    unsigned char* const t = S();
    if (t[9] == 2) t[1] = static_cast<unsigned char>(t[1] + 1);
}

// The shuffle both aims share: every sixteenth particle, sixteen swaps of two
// entries of `order` by Rand & 0xF (the first then the second).
void Shuffle(unsigned char* order) {
    for (unsigned n = 0; n < 16; ++n) {
        const unsigned a = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
        const unsigned b = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
        const unsigned char x = order[a];
        const unsigned char y = order[b];
        order[b] = x;
        order[a] = y;
    }
}
// Two steps of 0..0xF, the larger first, the second negated.
void Steps(unsigned char* part) {
    part[0x10] = static_cast<unsigned char>(SH_CALL(Rand)() & 0xF);
    part[0x11] = static_cast<unsigned char>(SH_CALL(Rand)() & 0xF);
    if (static_cast<signed char>(part[0x10]) < static_cast<signed char>(part[0x11])) {
        const unsigned char t = part[0x10];
        part[0x10] = part[0x11];
        part[0x11] = t;
    }
    part[0x11] = static_cast<unsigned char>(0u - part[0x11]);
}
// The particle d steps back (`sign` -1) or on (+1) along its step: x, y each
// minus / plus step * d (fild; fsubr / fadd).
void Offset(unsigned char* part, unsigned d, int sign) {
    const U ex = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(part[0x10])) * static_cast<std::int32_t>(d));
    const U ey = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(part[0x11])) * static_cast<std::int32_t>(d));
    if (sign < 0) {
        StF(part + 4, F(part + 4) - I(ex));
        StF(part + 8, F(part + 8) - I(ey));
    } else {
        StF(part + 4, I(ex) + F(part + 4));
        StF(part + 8, I(ey) + F(part + 8));
    }
}
// The end both aims share: +9 = 0x50, +1 up, sound 0x20C.
void AimEnd() {
    S()[9] = 0x50;
    NextState();
    SH_CALL(Sound_PlayEffect)(at::kSoundPixels);
}

// A particle drawn: a TILE_1 at its x, y, depth, coloured from its 15-bit
// pixel (r = low byte << 3, g = bits 5..9, b = bits 10..14, each << 3),
// committed 0x14 to slot 2.
void DrawPixel(const unsigned char* part) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SetUL(p + 8, UL(part + 4));
    SetUL(p + 0xC, UL(part + 8));
    SetUL(p + 0x10, UL(part + 0xC));
    p[4] = static_cast<unsigned char>(part[2] << 3);
    p[5] = static_cast<unsigned char>((Word(part + 2) >> 2) & 0xF8u);
    p[6] = static_cast<unsigned char>((Word(part + 2) >> 7) & 0xF8u);
    SH_CALL(Gfx_CommitPrim)(2, 0x14);
}

// --- kind 0xA8's bars -------------------------------------------------------------------

unsigned char* Bar(unsigned i) { return EffectKind30_Shards + i * at::kBarStride; }

}  // namespace

// ===========================================================================
// Kind 0x60 (EffectKind60_Run 0x4801F0, E3A's): EffectKind60_States 0..2
// ===========================================================================

// original 0x480210 (state 0, hidden in R3E's 0x47FBE0): the point +0x34..
// (0x198000, 0x198000, 0x4000000), the line's other end +0xC.. that point plus
// 0x40000 in x; +1 up.
extern "C" void __cdecl EffectKind60_Start(void) {
    unsigned char* const s = S();
    SetUL(s + 0x34, 0x198000);
    SetUL(s + 0x38, 0x198000);
    SetUL(s + 0x3C, 0x4000000);
    SetUL(s + 0xC, UL(s + 0x34) + 0x40000u);
    SetUL(s + 0x10, UL(s + 0x38));
    SetUL(s + 0x14, UL(s + 0x3C));
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x480270 (state 1, hidden in 0x47FBE0): both ends' x down 0x20000;
// the line (EffectKind60_DrawLine(+0x34, +0xC)); +1 up once +0x34 is 0x98000.
extern "C" void __cdecl EffectKind60_SlideBoth(void) {
    unsigned char* s = S();
    SetUL(s + 0x34, UL(s + 0x34) + 0xFFFE0000u);
    s = S();
    SetUL(s + 0xC, UL(s + 0xC) + 0xFFFE0000u);
    s = S();
    SH_CALL(EffectKind60_DrawLine)(Point(s + 0x34), Point(s + 0xC));
    s = S();
    if (UL(s + 0x34) == 0x98000u) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4802C0 (state 2, hidden in 0x47FBE0): the other end's x down
// 0x20000; the line; +1 up once +0xC is 0x98000.
extern "C" void __cdecl EffectKind60_SlideEnd(void) {
    unsigned char* s = S();
    SetUL(s + 0xC, UL(s + 0xC) + 0xFFFE0000u);
    s = S();
    SH_CALL(EffectKind60_DrawLine)(Point(s + 0x34), Point(s + 0xC));
    s = S();
    if (UL(s + 0xC) == 0x98000u) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// ===========================================================================
// Kind 0x5F's disc (R3E's states call it) and kind 0x69's lines (E3B's)
// ===========================================================================

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
        const int s = SH_AT(int (__cdecl*)(int), at::kSqrt)(rr * rr - row * row);
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

// original 0x4837B0 (EffectKind69_Part2's lines; PSX twin 0x801D218C): in the
// scratch cells (DamageScratch +0, +4, +8, Scratch_Swap) the amplitude 0x40,
// the angle ((+0xB + 2) & 0xF) << 8, the count +0xA, a shade Rand & 0x3F;
// Prim_VertexScratch (0, Math_Sin(angle) * amplitude sar 12, 0). For each i
// from 1 below the count (read again after every call): a draw mode (page
// 0xB5, dtd 1) committed 0xC to slot 2; a semi-transparent LINE_G2 from the
// vertex projected; the amplitude 0x80 +/- Rand & 0x3F (Rand & 1 chooses);
// the vertex (0, Math_Sin(((+0xB + i + 2) & 0xF) << 8) * amplitude sar 12,
// -i << 6) projected for the other end; the shades (shade, shade, 0x20) and
// (next, next, 0x20), next Rand & 0x3F kept; committed 0x24 to slot 2.
extern "C" void __cdecl EffectKind69_DrawLines(void) {
    unsigned char* s = S();
    SetUL(At(at::kLineAmp), 0x40);
    SetUL(At(at::kLineAngle), ((s[0xB] + 2u) & 0xFu) << 8);
    Scratch_Swap = s[0xA];
    SetUL(At(at::kLineShade), static_cast<U>(SH_CALL(Rand)()) & 0x3Fu);
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    {
        const U angle = UL(At(at::kLineAngle));
        SetWord(v, 0);
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
        SetWord(v + 2, Sar(sn * UL(At(at::kLineAmp)), 12));
        SetWord(v + 4, 0);
    }
    if (static_cast<std::int32_t>(Scratch_Swap) <= 1) return;
    for (U i = 1;; ++i) {
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
        SH_CALL(Gfx_CommitPrim)(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineG2)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        long depth;
        SH_CALL(Gte_RotTransPers)(Prim_VertexScratch, reinterpret_cast<unsigned long*>(p + 8), &depth);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        if ((SH_CALL(Rand)() & 1) != 0)
            SetUL(At(at::kLineAmp), (static_cast<U>(SH_CALL(Rand)()) & 0x3Fu) + 0x80u);
        else
            SetUL(At(at::kLineAmp), 0x80u - (static_cast<U>(SH_CALL(Rand)()) & 0x3Fu));
        s = S();
        const U angle = ((s[0xB] + i + 2u) & 0xFu) << 8;
        SetWord(v, 0);
        SetUL(At(at::kLineAngle), angle);
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
        SetWord(v + 2, Sar(sn * UL(At(at::kLineAmp)), 12));
        SetWord(v + 4, (0u - i) << 6);
        SH_CALL(Gte_RotTransPers)(Prim_VertexScratch, reinterpret_cast<unsigned long*>(p + 0x18), &depth);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x20));
        p[4] = At(at::kLineShade)[0];
        p[5] = At(at::kLineShade)[0];
        p[6] = 0x20;
        const U next = static_cast<U>(SH_CALL(Rand)()) & 0x3Fu;
        SetUL(At(at::kLineShade), next);
        p[0x14] = static_cast<unsigned char>(next);
        p[0x15] = At(at::kLineShade)[0];
        p[0x16] = 0x20;
        SH_CALL(Gfx_CommitPrim)(2, 0x24);
        if (!(static_cast<std::int32_t>(i + 1) < static_cast<std::int32_t>(Scratch_Swap))) break;
    }
}

// ===========================================================================
// Kind 0x9C's trail (E4E's states 1..6 call it)
// ===========================================================================

// original 0x48ED80 (from, to, shade; PSX twin 0x801D1C1C): a draw mode
// (Gpu_GetTPage(0, 1, 0x2C0, 0x100), dtd 1) committed 0xC to slot 2; two
// semi-transparent POLY_G4 from `from` projected (a) to `to` projected (b),
// each committed 0x44 to slot 2: the first (a + (hx, hy), a, b - (hx, hy), b)
// shaded (black, (s, s, 0), black, (s, s, 0)), the second (a, a + (hx, hy), b,
// b - (hx, hy)) shaded ((0, s, 0), black, (0, s, 0), black) - hx / hy the
// floats 0x5C41B8 / 0x5C41C0, every vertex at its end's depth. Then up to
// eight TILE_1 dots (0, s, 0) from from + (Frame_Counter & 7) * d stepping 8 *
// d, d = (to - from) sar 6, while the dot's y is not below to's, each at its
// projection less hx in y, committed 0x14 to slot 2.
extern "C" void __cdecl EffectKind9C_DrawTrail(const long* from, const long* to, unsigned shade) {
    DrawMode(1, 0x2C0, 0x100, 1, 2);
    const unsigned char c = static_cast<unsigned char>(shade);
    const unsigned char* const hx = At(at::kTrailHalfX);
    const unsigned char* const hy = At(at::kTrailHalfY);
    alignas(4) unsigned char o[12];
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    StF(p + 8, F(o) + F(hx));
    StF(p + 0xC, F(o + 4) + F(hy));
    SetUL(p + 0x18, UL(o));
    SetUL(p + 0x1C, UL(o + 4));
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    StF(p + 0x28, F(o) - F(hx));
    StF(p + 0x2C, F(o + 4) - F(hy));
    SetUL(p + 0x38, UL(o));
    SetUL(p + 0x3C, UL(o + 4));
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = c;
    p[0x15] = c;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = c;
    p[0x35] = c;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    SetUL(p + 8, UL(o));
    SetUL(p + 0xC, UL(o + 4));
    StF(p + 0x18, F(o) + F(hx));
    StF(p + 0x1C, F(o + 4) + F(hy));
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    SetUL(p + 0x28, UL(o));
    SetUL(p + 0x2C, UL(o + 4));
    StF(p + 0x38, F(o) - F(hx));
    StF(p + 0x3C, F(o + 4) - F(hy));
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = 0;
    p[5] = c;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = c;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    // the dots
    const U fx = static_cast<U>(from[0]), fy = static_cast<U>(from[1]);
    const U dx = Sar(static_cast<U>(to[0]) - fx, 6);
    const U dy = Sar(static_cast<U>(to[1]) - fy, 6);
    const U fz = static_cast<U>(from[2]);
    const U dz = Sar(static_cast<U>(to[2]) - fz, 6);
    const U f = Frame_Counter & 7u;
    alignas(4) long q[3] = {static_cast<long>(f * dx + fx), static_cast<long>(f * dy + fy), static_cast<long>(f * dz + fz)};
    for (unsigned j = 0; j < 8; ++j) {
        if (q[1] < to[1]) break;
        SH_CALL(EffectGte_ProjectPoint)(q, Out(o));
        p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(o));
        StF(p + 0xC, F(o + 4) - F(hx));
        SetUL(p + 0x10, UL(o + 8));
        p[4] = 0;
        p[5] = c;
        p[6] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x14);
        q[0] = static_cast<long>(static_cast<U>(q[0]) + dx * 8);
        q[1] = static_cast<long>(static_cast<U>(q[1]) + dy * 8);
        q[2] = static_cast<long>(static_cast<U>(q[2]) + dz * 8);
    }
}

// ===========================================================================
// Kind 0x9E (EffectKind9E_Run 0x48F070, E4E's): EffectKind9E_States 0..5
// ===========================================================================

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

// original 0x48F090 (state 0, hidden in 0x48ED80): the half-extent +0xC.. 0;
// its speed +0x18.. (0x3000, 0, 0x200000), or (0, 0x3000, 0x200000) when +6 bit
// 0 is set; +6 bit 3 set: the extent already 8 steps on and +1 up three (to
// state 4), else +9 = 8 and +1 up. Unless +0xB bit 0, sound 0x207 (0x202 when
// +6 bit 2).
extern "C" void __cdecl EffectKind9E_Start(void) {
    unsigned char* const s = S();
    SetUL(s + 0xC, 0);
    SetUL(s + 0x10, 0);
    SetUL(s + 0x14, 0);
    if ((s[6] & 1) == 0) {
        SetUL(s + 0x18, 0x3000);
        SetUL(s + 0x1C, 0);
    } else {
        SetUL(s + 0x18, 0);
        SetUL(s + 0x1C, 0x3000);
    }
    SetUL(s + 0x20, 0x200000);
    if ((s[6] & 8) != 0) {
        SetUL(s + 0xC, UL(s + 0xC) + (UL(s + 0x18) << 3));
        SetUL(s + 0x10, UL(s + 0x10) + (UL(s + 0x1C) << 3));
        SetUL(s + 0x14, UL(s + 0x14) + (UL(s + 0x20) << 3));
        s[1] = static_cast<unsigned char>(s[1] + 3);
    } else {
        s[9] = 8;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
    if ((s[0xB] & 1) == 0) SH_CALL(Sound_PlayEffect)((s[6] & 4) != 0 ? at::kSound9EOpenB : at::kSound9EOpenA);
}

// The extent +0xC.. stepped by its speed +0x18.. (`sign` +1 or -1), then the
// outline drawn round the point; +9 down, at 0 +1 up.
static void Step9E(int sign) {
    unsigned char* const s = S();
    for (unsigned i = 0; i < 12; i += 4)
        SetUL(s + 0xC + i, sign > 0 ? UL(s + 0xC + i) + UL(s + 0x18 + i) : UL(s + 0xC + i) - UL(s + 0x18 + i));
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const t = S();
    SH_CALL(EffectKind9E_DrawOutline)(Point(t + 0x34), Point(t + 0xC));
    if (CountDown()) NextState();
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
    SH_CALL(EffectKind9E_DrawOutline)(Point(s + 0x34), Point(s + 0xC));
    s = S();
    SH_CALL(EffectKind9E_DrawPlate)(Point(s + 0x34), Point(s + 0xC));
    s = S();
    unsigned char* const w = At(at::k9EWall);
    SetUL(w, 0u - UL(s + 0xC));
    SetUL(w + 4, 0u - UL(s + 0x10));
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
        unsigned char* const w = At(at::k9EWall);
        const U v = UL(w + 8) + Sar(UL(s + 0x20), 1);
        SetUL(w + 8, v);
        if (v == UL(s + 0x14)) SetUL(w + 8, 0u - v);
    }
    SH_CALL(EffectGte_LoadMapCamera)();
    if ((Frame_Counter & 1) != 0) {
        unsigned char* s = S();
        SH_CALL(EffectKind9E_DrawShadePlate)(Point(s + 0x34), Point(s + 0xC));
        s = S();
        if ((s[6] & 1) == 0)
            SetUL(s + 0x34, UL(s + 0x34) + 0x4000u);
        else
            SetUL(s + 0x38, UL(s + 0x38) + 0x4000u);
        s = S();
        SH_CALL(EffectKind9E_DrawTexPlate)(Point(s + 0x34), Point(s + 0xC));
        s = S();
        if ((s[6] & 1) == 0)
            SetUL(s + 0x34, UL(s + 0x34) + 0xFFFFC000u);
        else
            SetUL(s + 0x38, UL(s + 0x38) + 0xFFFFC000u);
    } else {
        unsigned char* const s = S();
        SH_CALL(EffectKind9E_DrawWall)(Point(s + 0x34), reinterpret_cast<const long*>(At(at::k9EWall)));
    }
    unsigned char* const s = S();
    if (At(at::kCounter)[0] == s[7]) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x48F300 (state 4; in no list - the cut's 0x48F240 runs over it):
// the plate; +9 = 8, +1 up; unless +0xB bit 1, sound 0x208 (0x203 when +6 bit
// 2).
extern "C" void __cdecl EffectKind9E_Close(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    SH_CALL(EffectKind9E_DrawPlate)(Point(s + 0x34), Point(s + 0xC));
    S()[9] = 8;
    NextState();
    s = S();
    if ((s[0xB] & 2) == 0) SH_CALL(Sound_PlayEffect)((s[6] & 4) != 0 ? at::kSound9EShutB : at::kSound9EShutA);
}

// original 0x48F360 (state 5, hidden in 0x48ED80): the extent shrinks by its
// speed; the outline; +9 down, at 0 +1 up (to Effect_StateRelease).
extern "C" void __cdecl EffectKind9E_Shrink(void) { Step9E(-1); }

// ===========================================================================
// Kinds 0xA1 and 0xA3 (EffectKindA1_Run 0x490A60, EffectKindA3_Run 0x4912F0,
// E4E's): EffectKindA1_States 0..4 = EffectKindA3_States 0..4, and 0xA3's 5..9
// ===========================================================================

// original 0x490A80 (state 0, hidden in E4E's EffectKindA0_SwapLong; PSX twin
// 0x801D21B4): CaptureSprite, the page set only when the copy's +0x24 bit 0
// is, and the animation (0x4D from 0x1C, +0x2A 0, when its word +0x2C is 0;
// else 0x50 from 0x24) only then.
extern "C" void __cdecl EffectKindA1_CaptureSprite(void) { CaptureSprite("EffectKindA1_CaptureSprite", false); }

// original 0x490BB0 (state 1, hidden in EffectKindA0_SwapLong): ReadBack.
extern "C" void __cdecl EffectKindA1_ReadBack(void) { ReadBack("EffectKindA1_ReadBack"); }

// original 0x490C50 (state 2, hidden in EffectKindA0_SwapLong): SplitPixels.
extern "C" void __cdecl EffectKindA1_SplitPixels(void) { SplitPixels("EffectKindA1_SplitPixels"); }

// original 0x490E20 (state 3, hidden in EffectKindA0_SwapLong): every particle
// its blink (an order of eight 0s and eight 1s shuffled every sixteenth) and
// its step - +0xB 0: (Rand & 0x1F) - 0x10 each; else Steps - and, +6 0: a
// delay d = (Rand & 0x1E) + 0xA and the particle d steps back; else the delay
// 0x28 - (Rand & 0x1E). AimEnd.
extern "C" void __cdecl EffectKindA1_AimPixels(void) {
    unsigned char order[16];
    for (unsigned n = 0; n < 16; ++n) order[n] = static_cast<unsigned char>(n & 1);
    unsigned j = 0;
    for (U i = 0; (i & 0xFFFFu) < Word(At(at::kPartCount)); ++i) {
        unsigned char* const part = Particle(i & 0xFFFFu);
        if ((i & 0xF) == 0) {
            Shuffle(order);
            j = 0;
        }
        const unsigned char* s = S();
        part[0] = order[j & 0xFFFF];
        ++j;
        if (s[0xB] == 0) {
            part[0x10] = static_cast<unsigned char>((SH_CALL(Rand)() & 0x1F) - 0x10);
            part[0x11] = static_cast<unsigned char>((SH_CALL(Rand)() & 0x1F) - 0x10);
        } else {
            Steps(part);
        }
        s = S();
        if (s[6] == 0) {
            const unsigned d = static_cast<unsigned char>((SH_CALL(Rand)() & 0x1E) + 0xA);
            part[1] = static_cast<unsigned char>(d);
            Offset(part, d, -1);
        } else {
            part[1] = static_cast<unsigned char>(0x28 - (SH_CALL(Rand)() & 0x1E));
        }
    }
    AimEnd();
}

// original 0x490FA0 (state 4, hidden in EffectKindA0_SwapLong): the sprite's
// byte +0 |= 0x40; every particle shown this frame (+0 not 0): +6 0 - while its
// delay runs it steps (and the delay counts down); else it waits out its delay
// and then steps; drawn (DrawPixel). Every particle's +0 flipped. +9 down, at
// 0 +1 up.
extern "C" void __cdecl EffectKindA1_MovePixels(void) {
    unsigned char* const sprite = P(UL(S() + 0x4C));
    sprite[0] = static_cast<unsigned char>(sprite[0] | 0x40);
    for (U i = 0; (i & 0xFFFFu) < Word(At(at::kPartCount)); ++i) {
        unsigned char* const part = Particle(i & 0xFFFFu);
        if (part[0] != 0) {
            if (S()[6] == 0) {
                if (part[1] != 0) {
                    part[1] = static_cast<unsigned char>(part[1] - 1);
                    Offset(part, 1, 1);
                }
            } else if (part[1] != 0) {
                part[1] = static_cast<unsigned char>(part[1] - 1);
            } else {
                Offset(part, 1, 1);
            }
            DrawPixel(part);
        }
        part[0] = static_cast<unsigned char>(part[0] ^ 1);
    }
    if (CountDown()) NextState();
}

// original 0x491310 (kind 0xA3's state 5, hidden in EffectKindA0_SwapLong):
// nothing until the chapter's count is 0x29; then CaptureSprite in kind
// 0xA3's form (the page always, Sprite_SetAnimation(7)).
extern "C" void __cdecl EffectKindA3_CaptureSprite(void) {
    if (At(at::kCounter)[0] != 0x29) return;
    CaptureSprite("EffectKindA3_CaptureSprite", true);
}

// original 0x491410 (state 6, hidden in EffectKindA0_SwapLong): ReadBack (the
// same code as 0x490BB0).
extern "C" void __cdecl EffectKindA3_ReadBack(void) { ReadBack("EffectKindA3_ReadBack"); }

// original 0x4914B0 (state 7, hidden in EffectKindA0_SwapLong): SplitPixels
// (the same code as 0x490C50).
extern "C" void __cdecl EffectKindA3_SplitPixels(void) { SplitPixels("EffectKindA3_SplitPixels"); }

// original 0x491680 (state 8, hidden in EffectKindA0_SwapLong): every particle
// its blink (the shuffled order) and Steps, a delay d = (Rand & 0x1E) + 0xA and
// the particle d steps on (it comes back in state 9). AimEnd.
extern "C" void __cdecl EffectKindA3_AimPixels(void) {
    unsigned char order[16];
    for (unsigned n = 0; n < 16; ++n) order[n] = static_cast<unsigned char>(n & 1);
    unsigned j = 0;
    for (U i = 0; (i & 0xFFFFu) < Word(At(at::kPartCount)); ++i) {
        unsigned char* const part = Particle(i & 0xFFFFu);
        if ((i & 0xF) == 0) {
            Shuffle(order);
            j = 0;
        }
        part[0] = order[j & 0xFFFF];
        ++j;
        Steps(part);
        const unsigned d = static_cast<unsigned char>((SH_CALL(Rand)() & 0x1E) + 0xA);
        part[1] = static_cast<unsigned char>(d);
        Offset(part, d, 1);
    }
    AimEnd();
}

// original 0x4917C0 (state 9, hidden in EffectKindA0_SwapLong): the sprite's
// byte +0 |= 0x40; every particle shown this frame steps back while its delay
// runs and is drawn; every +0 flipped. +9 down, at 0 the chapter's count =
// 0x2B and +1 up.
extern "C" void __cdecl EffectKindA3_MovePixels(void) {
    unsigned char* const sprite = P(UL(S() + 0x4C));
    sprite[0] = static_cast<unsigned char>(sprite[0] | 0x40);
    for (U i = 0; (i & 0xFFFFu) < Word(At(at::kPartCount)); ++i) {
        unsigned char* const part = Particle(i & 0xFFFFu);
        if (part[0] != 0) {
            if (part[1] != 0) {
                part[1] = static_cast<unsigned char>(part[1] - 1);
                Offset(part, 1, -1);
            }
            DrawPixel(part);
        }
        part[0] = static_cast<unsigned char>(part[0] ^ 1);
    }
    if (CountDown()) {
        At(at::kCounter)[0] = 0x2B;
        NextState();
    }
}

// ===========================================================================
// Kind 0xA2 (EffectKindA2_Run 0x4910D0, E4E's): EffectKindA2_States 0..3
// ===========================================================================

// original 0x4918B0 (point, inner, outer): a draw mode (Gpu_GetTPage(0, 1,
// 0x2C0, 0x100), dtd 0) committed 0xC to slot 2; EffectGte_LoadMapCamera; a
// ring of 32 semi-transparent POLY_G4 round the point in x and y - inner corners
// at the point + ((cos, sin) * inner sar 8), outer at outer's, the angle 0x80
// by 0x80 (the first edge the point + (inner << 4, 0) and (outer << 4, 0)),
// each corner projected (the point's z); inner corners (0xFF, 0x7F, 0), outer
// black; each committed 0x44 to slot 2. al 0.
extern "C" unsigned char __cdecl EffectKindA2_DrawRing(const long* point, long inner, long outer) {
    SetMode(1, 0x2C0, 0x100, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) long q[3];
    alignas(4) unsigned char o0[12], o1[12];
    q[0] = static_cast<long>((static_cast<U>(inner) << 4) + static_cast<U>(point[0]));
    q[1] = point[1];
    q[2] = point[2];
    SH_CALL(EffectGte_ProjectPoint)(q, Out(o0));
    q[0] = static_cast<long>((static_cast<U>(outer) << 4) + static_cast<U>(point[0]));
    SH_CALL(EffectGte_ProjectPoint)(q, Out(o1));
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        angle += 0x80;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, o0);
        Copy12(p + 0x18, o1);
        const int t = static_cast<int>(angle & 0xFFFFu);
        const U c1 = static_cast<U>(SH_CALL(Math_Cos)(t));
        q[0] = static_cast<long>(Sar(c1 * static_cast<U>(inner), 8) + static_cast<U>(point[0]));
        const U s1 = static_cast<U>(SH_CALL(Math_Sin)(t));
        q[1] = static_cast<long>(Sar(s1 * static_cast<U>(inner), 8) + static_cast<U>(point[1]));
        SH_CALL(EffectGte_ProjectPoint)(q, Out(o0));
        const U c2 = static_cast<U>(SH_CALL(Math_Cos)(t));
        q[0] = static_cast<long>(Sar(c2 * static_cast<U>(outer), 8) + static_cast<U>(point[0]));
        const U s2 = static_cast<U>(SH_CALL(Math_Sin)(t));
        q[1] = static_cast<long>(Sar(s2 * static_cast<U>(outer), 8) + static_cast<U>(point[1]));
        SH_CALL(EffectGte_ProjectPoint)(q, Out(o1));
        Copy12(p + 0x28, o0);
        Copy12(p + 0x38, o1);
        p[4] = 0xFF;
        p[5] = 0x7F;
        p[6] = 0;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0xFF;
        p[0x25] = 0x7F;
        p[0x26] = 0;
        p[0x34] = 0;
        p[0x35] = 0;
        p[0x36] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x44);
    }
    return 0;
}

// original 0x4910F0 (state 0, hidden in EffectKindA0_SwapLong): the radii
// +0xC, +0x10 and their speeds +0x18, +0x1C 0; +9 = 0x14, +1 up; sound 0x208.
extern "C" void __cdecl EffectKindA2_Start(void) {
    unsigned char* const s = S();
    SetUL(s + 0xC, 0);
    SetUL(s + 0x10, 0);
    SetUL(s + 0x18, 0);
    SetUL(s + 0x1C, 0);
    s[9] = 0x14;
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(at::kSoundRing);
}

// original 0x491140 (state 1, hidden in EffectKindA0_SwapLong): the speeds up
// 0x200 / 0x100, the radii up by them; the ring (+0x34, +0xC, +0x10); +9 down,
// at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA2_Grow(void) {
    unsigned char* const s = S();
    SetUL(s + 0x18, UL(s + 0x18) + 0x200u);
    SetUL(s + 0x1C, UL(s + 0x1C) + 0x100u);
    SetUL(s + 0xC, UL(s + 0xC) + UL(s + 0x18));
    SetUL(s + 0x10, UL(s + 0x10) + UL(s + 0x1C));
    SH_CALL(EffectKindA2_DrawRing)(Point(s + 0x34), Long(s + 0xC), Long(s + 0x10));
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x4911C0 (state 2, hidden in EffectKindA0_SwapLong): the radii and
// speeds 0, then twenty steps of the speeds (+0x100 / +0x200) and the radii by
// them, the radii negated; +9 = 0x14, +1 up; sound 0x208.
extern "C" void __cdecl EffectKindA2_Rewind(void) {
    unsigned char* const s = S();
    SetUL(s + 0xC, 0);
    SetUL(s + 0x10, 0);
    SetUL(s + 0x18, 0);
    SetUL(s + 0x1C, 0);
    for (unsigned n = 0; n < 0x14; ++n) {
        SetUL(s + 0x18, UL(s + 0x18) + 0x100u);
        SetUL(s + 0x1C, UL(s + 0x1C) + 0x200u);
        SetUL(s + 0xC, UL(s + 0xC) + UL(s + 0x18));
        SetUL(s + 0x10, UL(s + 0x10) + UL(s + 0x1C));
    }
    SetUL(s + 0xC, 0u - UL(s + 0xC));
    SetUL(s + 0x10, 0u - UL(s + 0x10));
    s[9] = 0x14;
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(at::kSoundRing);
}

// original 0x491270 (state 3, hidden in EffectKindA0_SwapLong): the ring; the
// radii up by the speeds, the speeds down 0x100 / 0x200; +9 down, at 0
// Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA2_Shrink(void) {
    unsigned char* s = S();
    SH_CALL(EffectKindA2_DrawRing)(Point(s + 0x34), Long(s + 0xC), Long(s + 0x10));
    s = S();
    SetUL(s + 0xC, UL(s + 0xC) + UL(s + 0x18));
    SetUL(s + 0x10, UL(s + 0x10) + UL(s + 0x1C));
    SetUL(s + 0x18, UL(s + 0x18) + 0xFFFFFF00u);
    SetUL(s + 0x1C, UL(s + 0x1C) + 0xFFFFFE00u);
    if (CountDown()) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0xA7 (EffectKindA7_Run 0x491AA0 calls its states, then the glow)
// ===========================================================================

// original 0x491E30 (point, size, colour): EffectKindA0_DrawGlow's code with
// the draw mode committed to slot 7 and every POLY_G3 opaque (SemiTrans 0) and
// committed 0x34 to slot 7: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), dtd
// 1); EffectGte_LoadMapCamera; o the point projected, r
// EffectGte_ProjectSize(point, {size, size})'s first word + (Frame_Counter &
// 1); a fan of 32 round o, the centre ((c & 0xFC) << 5, (c & 0xFE) << 6, c <<
// 7), the rim black at o + (cos, sin) * r sar 12 from the angle 0x80 by 0x80,
// every vertex at o's depth.
extern "C" void __cdecl EffectKindA7_DrawGlow(const long* point, unsigned size, unsigned colour) {
    DrawMode(1, 0x2C0, 0x100, 1, 7);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(point, Out(o));
    alignas(4) short sz[2] = {static_cast<short>(size), static_cast<short>(size)};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(point, sz, r);
    const std::int32_t radius = static_cast<std::int16_t>(static_cast<U>(static_cast<unsigned short>(r[0])) + (Frame_Counter & 1u));
    const unsigned char c = static_cast<unsigned char>(colour);
    const unsigned char red = static_cast<unsigned char>((c & 0xFC) << 5);
    const unsigned char green = static_cast<unsigned char>((c & 0xFE) << 6);
    const unsigned char blue = static_cast<unsigned char>(c << 7);
    alignas(4) unsigned char rim[8];
    StF(rim, I(static_cast<U>(radius)) + F(o));
    SetUL(rim + 4, UL(o + 4));
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        angle += 0x80;
        const int t = static_cast<int>(angle & 0xFFFFu);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 0);
        SetUL(p + 8, UL(o));
        SetUL(p + 0xC, UL(o + 4));
        SetUL(p + 0x18, UL(rim));
        SetUL(p + 0x1C, UL(rim + 4));
        const U dx = Sar(static_cast<U>(SH_CALL(Math_Cos)(t)) * static_cast<U>(radius), 12);
        StF(rim, I(dx) + F(o));
        const U dy = Sar(static_cast<U>(SH_CALL(Math_Sin)(t)) * static_cast<U>(radius), 12);
        SetUL(p + 0x28, UL(rim));
        StF(rim + 4, I(dy) + F(o + 4));
        SetUL(p + 0x2C, UL(rim + 4));
        SetUL(p + 0x30, UL(o + 8));
        SetUL(p + 0x20, UL(o + 8));
        SetUL(p + 0x10, UL(o + 8));
        p[4] = red;
        p[5] = green;
        p[6] = blue;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0;
        p[0x25] = 0;
        p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(7, 0x34);
    }
}

// original 0x491AE0 (state 0, hidden in 0x4918B0): the size +0xC 0, +9 = 0x10,
// +1 up.
extern "C" void __cdecl EffectKindA7_Start(void) {
    unsigned char* const s = S();
    SetUL(s + 0xC, 0);
    s[9] = 0x10;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x491B00 (state 1, hidden in 0x4918B0): the size up 0x10; +9 down,
// at 0 +9 = 0x20 and +1 up.
extern "C" void __cdecl EffectKindA7_Grow(void) {
    unsigned char* const s = S();
    SetUL(s + 0xC, UL(s + 0xC) + 0x10u);
    if (CountDown()) {
        S()[9] = 0x20;
        NextState();
    }
}

// original 0x491B40 (state 2, hidden in 0x4918B0): +9 down, at 0 +9 = 0x10 and
// +1 up.
extern "C" void __cdecl EffectKindA7_Hold(void) {
    if (CountDown()) {
        S()[9] = 0x10;
        NextState();
    }
}

// original 0x491B70 (state 3, hidden in 0x4918B0): the size down 0x10; +9 down,
// at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA7_Shrink(void) {
    unsigned char* const s = S();
    SetUL(s + 0xC, UL(s + 0xC) + 0xFFFFFFF0u);
    if (CountDown()) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0xA8 (EffectKindA8_Run 0x491BA0): EffectKindA8_States 0..2, the bars
// ===========================================================================

// original 0x491FF0: the sixteen bars out of use (+0 = 0).
extern "C" void __cdecl EffectKindA8_ClearBars(void) {
    for (unsigned i = 0; i < at::kBarCount; ++i) Bar(i)[0] = 0;
}

// original 0x492010: the first bar out of use put in use (+0 = 1, +2 = 0);
// none free, nothing.
extern "C" void __cdecl EffectKindA8_StartBar(void) {
    for (unsigned i = 0; i < at::kBarCount; ++i) {
        unsigned char* const b = Bar(i);
        if (b[0] == 0) {
            b[0] = 1;
            b[2] = 0;
            return;
        }
    }
}

// original 0x4920F0 (bar): two opaque POLY_G4 across the screen, x 0..320
// (0x43A00000) - from y (+4 - +3 - (Frame_Counter & 1)) black to y +4 red
// (0x80, 0, 0), then from y +4 red to y (+4 + (Frame_Counter & 1) + +3) black;
// each committed 0x44 to slot 7. The depth words are not written. Also called by
// E4F's EffectKindB0_StepBars.
extern "C" void __cdecl EffectKindA8_DrawBar(const unsigned char* bar) {
    const U f = Frame_Counter & 1u;
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetUL(p + 8, 0);
    StF(p + 0xC, I(static_cast<U>(SW(bar + 4)) - bar[3] - f));
    SetUL(p + 0x18, 0x43A00000u);
    SetUL(p + 0x28, 0);
    StF(p + 0x1C, I(static_cast<U>(SW(bar + 4)) - bar[3] - f));
    SetUL(p + 0x38, 0x43A00000u);
    StF(p + 0x2C, I(static_cast<U>(SW(bar + 4))));
    StF(p + 0x3C, I(static_cast<U>(SW(bar + 4))));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0x80;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = 0x80;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(7, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetUL(p + 8, 0);
    SetUL(p + 0x18, 0x43A00000u);
    StF(p + 0xC, I(static_cast<U>(SW(bar + 4))));
    SetUL(p + 0x28, 0);
    StF(p + 0x1C, I(static_cast<U>(SW(bar + 4))));
    SetUL(p + 0x38, 0x43A00000u);
    StF(p + 0x2C, I(bar[3] + f + static_cast<U>(SW(bar + 4))));
    p[4] = 0x80;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0x80;
    p[0x15] = 0;
    p[0x16] = 0;
    StF(p + 0x3C, I(bar[3] + f + static_cast<U>(SW(bar + 4))));
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(7, 0x44);
}

// original 0x492030 (length, its low word read): a draw mode
// (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1) committed 0xC to slot 7;
// EffectGte_LoadMapCamera; each bar in use by its step +2: 0 - +1 = +3 = 4,
// +4 = length, +2 = 1; 1 - +2 = 2; 2 - +4 down 8, below 0 the bar out of use;
// then drawn unless +2 is 0. al 1 when any was in use.
extern "C" unsigned char __cdecl EffectKindA8_StepBars(unsigned length) {
    DrawMode(1, 0x380, 0x100, 1, 7);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kBarCount; ++i) {
        unsigned char* const b = Bar(i);
        if (b[0] == 0) continue;
        any = 1;
        switch (b[2]) {
        case 0: {
            const unsigned char next = static_cast<unsigned char>(b[2] + 1);
            b[1] = 4;
            b[3] = 4;
            SetWord(b + 4, length & 0xFFFFu);
            b[2] = next;
            break;
        }
        case 1: b[2] = static_cast<unsigned char>(b[2] + 1); break;
        case 2:
            SetWord(b + 4, Word(b + 4) - 8u);
            if (static_cast<std::int16_t>(Word(b + 4)) < 0) b[0] = 0;
            break;
        default: break;
        }
        if (b[2] != 0) SH_CALL(EffectKindA8_DrawBar)(b);
    }
    return any;
}

// original 0x491BC0 (state 0, hidden in 0x4918B0): the word +0x2E = 0x12C,
// +9 = 0; the bars cleared; +1 up.
extern "C" void __cdecl EffectKindA8_Start(void) {
    unsigned char* s = S();
    SetWord(s + 0x2E, 0x12C);
    s = S();
    s[9] = 0;
    SH_CALL(EffectKindA8_ClearBars)();
    NextState();
}

// original 0x491BF0 (state 1, hidden in 0x4918B0): +9 0 - a bar started and +9
// = (Rand & 0xF) + 0x18; else +9 down; the bars stepped (0xF0); +1 up once the
// chapter's count is 0xE.
extern "C" void __cdecl EffectKindA8_Bars(void) {
    unsigned char* s = S();
    unsigned char next;
    if (s[9] == 0) {
        SH_CALL(EffectKindA8_StartBar)();
        const int r = SH_CALL(Rand)();
        s = S();
        next = static_cast<unsigned char>((r & 0xF) + 0x18);
    } else {
        next = static_cast<unsigned char>(s[9] - 1);
    }
    s[9] = next;
    SH_CALL(EffectKindA8_StepBars)(0xF0);
    if (At(at::kCounter)[0] == 0xE) NextState();
}

// original 0x491C40 (state 2, hidden in 0x4918B0): the bars stepped (0xF0);
// none left in use, Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA8_End(void) {
    if (SH_CALL(EffectKindA8_StepBars)(0xF0) == 0) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0xA9 (EffectKindA9_Run 0x491C60 calls its states, then the disc)
// ===========================================================================

// original 0x492260 (rx, ry, shade - two s16 and a byte): a draw mode
// (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1) committed 0xC to slot 7; a fan of
// 32 semi-transparent POLY_G3 from the screen point (160, 240), its rim from
// the angle 0x800 by 0x40 to 0x1000 at (160 + f + cos * rx sar 12, 240 + f +
// sin * ry sar 12) - each a word, f = (Frame_Counter & 1) << 2 - the centre
// (shade, shade >> 2, shade >> 2), the rim black; each committed 0x34 to slot
// 7. The depth words are not written.
extern "C" void __cdecl EffectKindA9_DrawDisc(unsigned rx, unsigned ry, unsigned shade) {
    DrawMode(1, 0x380, 0x100, 1, 7);
    const U f = (Frame_Counter & 1u) << 2;
    const U r1 = static_cast<U>(static_cast<std::int16_t>(rx));
    const U r2 = static_cast<U>(static_cast<std::int16_t>(ry));
    U angle = 0x800;
    alignas(4) unsigned char prev[8];
    {
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
        const U x = static_cast<U>(static_cast<std::int16_t>(Sar(c * r1, 12) + f + 0xA0u));
        const U s = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
        const U y = static_cast<U>(static_cast<std::int16_t>(Sar(s * r2, 12) + f + 0xF0u));
        StF(prev, I(x));
        StF(prev + 4, I(y));
    }
    const unsigned char dim = static_cast<unsigned char>(static_cast<unsigned char>(shade) >> 2);
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        angle += 0x40;
        SetUL(p + 8, 0x43200000u);
        SetUL(p + 0xC, 0x43700000u);
        SetUL(p + 0x18, UL(prev));
        SetUL(p + 0x1C, UL(prev + 4));
        const int t = static_cast<int>(angle & 0xFFFFu);
        const U c = static_cast<U>(SH_CALL(Math_Cos)(t));
        const U x = static_cast<U>(static_cast<std::int16_t>(Sar(c * r1, 12) + f + 0xA0u));
        const U s = static_cast<U>(SH_CALL(Math_Sin)(t));
        const U y = static_cast<U>(static_cast<std::int16_t>(Sar(s * r2, 12) + f + 0xF0u));
        StF(prev, I(x));
        SetUL(p + 0x28, UL(prev));
        StF(prev + 4, I(y));
        SetUL(p + 0x2C, UL(prev + 4));
        p[4] = static_cast<unsigned char>(shade);
        p[5] = dim;
        p[6] = dim;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0;
        p[0x25] = 0;
        p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(7, 0x34);
    }
}

// original 0x491CA0 (state 0, hidden in 0x4918B0): the radii words +0x2E, +0x30
// 0, +9 = 0xA, +1 up.
extern "C" void __cdecl EffectKindA9_Start(void) {
    unsigned char* const s = S();
    SetWord(s + 0x2E, 0);
    SetWord(s + 0x30, 0);
    s[9] = 0xA;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x491CD0 (state 1, hidden in 0x4918B0): the radii up 0x15 / 9; +9
// down, at 0 +9 = 0x5A and +1 up.
extern "C" void __cdecl EffectKindA9_Grow(void) {
    unsigned char* const s = S();
    SetWord(s + 0x2E, Word(s + 0x2E) + 0x15u);
    SetWord(s + 0x30, Word(s + 0x30) + 9u);
    if (CountDown()) {
        S()[9] = 0x5A;
        NextState();
    }
}

// original 0x491D10 (state 2, hidden in 0x4918B0): once the chapter's count is
// 9, +9 = 0x1E and +1 up.
extern "C" void __cdecl EffectKindA9_Wait(void) {
    if (At(at::kCounter)[0] != 9) return;
    S()[9] = 0x1E;
    NextState();
}

// original 0x491D30 (state 3, hidden in 0x4918B0): the radii down 7 / 3; +9
// down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA9_Shrink(void) {
    unsigned char* const s = S();
    SetWord(s + 0x2E, Word(s + 0x2E) + 0xFFF9u);
    SetWord(s + 0x30, Word(s + 0x30) + 0xFFFDu);
    if (CountDown()) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0xAA (EffectKindAA_Run 0x491D70, E4F's): EffectKindAA_States 0..2
// ===========================================================================

// original 0x492400 (shade, a byte): a draw mode (Gpu_GetTPage(0, 1, 0x380,
// 0x100), dtd 1) committed 0xC to slot 7; two opaque POLY_G4 over the frame -
// rows 0..120 from black to (shade, 0, 0), rows 120..240 back to black - each
// committed 0x44 to slot 7; the depth words not written. A full-frame fill
// (0, 0) 320 x 240: DIV-0041 section 3c's, drawn from Widescreen_FillX() to
// 320 + Widescreen_Fill() (the original's 0 .. 320 narrow or unarmed: every
// self-test compares those).
extern "C" void __cdecl EffectKindAA_DrawFill(unsigned shade) {
    DrawMode(1, 0x380, 0x100, 1, 7);
    const U left = std::bit_cast<U>(Widescreen_FillX());                                 // DIV-0041: -53 wide, 0 narrow
    const U right = std::bit_cast<U>(320.0f + static_cast<float>(Widescreen_Fill()));    // 0x43A00000, 320.0f narrow
    const unsigned char c = static_cast<unsigned char>(shade);
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetUL(p + 8, left);
    SetUL(p + 0xC, 0);
    SetUL(p + 0x18, right);
    SetUL(p + 0x1C, 0);
    SetUL(p + 0x28, left);
    SetUL(p + 0x2C, 0x42F00000u);   // 120.0f
    SetUL(p + 0x38, right);
    SetUL(p + 0x3C, 0x42F00000u);
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = c;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = c;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(7, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetUL(p + 8, left);
    SetUL(p + 0x2C, 0x43700000u);   // 240.0f
    SetUL(p + 0x3C, 0x43700000u);
    SetUL(p + 0xC, 0x42F00000u);
    SetUL(p + 0x18, right);
    SetUL(p + 0x1C, 0x42F00000u);
    SetUL(p + 0x28, left);
    SetUL(p + 0x38, right);
    p[4] = c;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = c;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(7, 0x44);
}

// original 0x491D90 (state 0, hidden in 0x4918B0): +9 = 0x10, +1 up.
extern "C" void __cdecl EffectKindAA_Start(void) {
    unsigned char* const s = S();
    s[9] = 0x10;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x491DB0 (state 1, hidden in 0x4918B0): the fill in the shade +9 *
// 0xF0 (its low byte: 0 at 0x10 up to 0xF0 at 1); +9 down, at 0 +9 = 0x10 and
// +1 up.
extern "C" void __cdecl EffectKindAA_FadeIn(void) {
    const unsigned char shade = static_cast<unsigned char>(S()[9] * 0xF0u);
    SH_CALL(EffectKindAA_DrawFill)(shade);
    if (CountDown()) {
        S()[9] = 0x10;
        NextState();
    }
}

// original 0x491DF0 (state 2, hidden in 0x4918B0): the fill in the shade 0xFF
// when +9 is 0x10, else +9 << 4 (a byte); +9 down, at 0 Effect_Release (a tail
// jmp).
extern "C" void __cdecl EffectKindAA_FadeOut(void) {
    const unsigned char n = S()[9];
    SH_CALL(EffectKindAA_DrawFill)(n == 0x10 ? 0xFFu : static_cast<unsigned char>(n << 4));
    if (CountDown()) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0xAB's state 1 (EffectKindAB_Run 0x492510, E4F's)
// ===========================================================================

// original 0x492580 (state 1, hidden in 0x492400): Draw_PassFlags 0 - +1 up;
// else EffectKindAB_Emit and a tail jmp to EffectKindAB_MoveDrops.
extern "C" void __cdecl EffectKindAB_Drops(void) {
    if (Draw_PassFlags == 0) {
        NextState();
        return;
    }
    SH_CALL(EffectKindAB_Emit)();
    SH_CALL(EffectKindAB_MoveDrops)();
}

void Rest3F_Inject() {
    if (bof3::WantsShadow("rest_3f")) rest_3f::SelfTest();
    BOF3_INJECT(EffectKind60_Start);
    BOF3_INJECT(EffectKind60_SlideBoth);
    BOF3_INJECT(EffectKind60_SlideEnd);
    BOF3_INJECT(EffectKind5F_DrawLineDisc);
    BOF3_INJECT(EffectKind69_DrawLines);
    BOF3_INJECT(EffectKind9C_DrawTrail);
    BOF3_INJECT(EffectKind9E_Start);
    BOF3_INJECT(EffectKind9E_Grow);
    BOF3_INJECT(EffectKind9E_Flash);
    BOF3_INJECT(EffectKind9E_Hold);
    BOF3_INJECT(EffectKind9E_Close);
    BOF3_INJECT(EffectKind9E_Shrink);
    BOF3_INJECT(EffectKind9E_DrawOutline);
    BOF3_INJECT(EffectKindA1_CaptureSprite);
    BOF3_INJECT(EffectKindA1_ReadBack);
    BOF3_INJECT(EffectKindA1_SplitPixels);
    BOF3_INJECT(EffectKindA1_AimPixels);
    BOF3_INJECT(EffectKindA1_MovePixels);
    BOF3_INJECT(EffectKindA2_Start);
    BOF3_INJECT(EffectKindA2_Grow);
    BOF3_INJECT(EffectKindA2_Rewind);
    BOF3_INJECT(EffectKindA2_Shrink);
    BOF3_INJECT(EffectKindA3_CaptureSprite);
    BOF3_INJECT(EffectKindA3_ReadBack);
    BOF3_INJECT(EffectKindA3_SplitPixels);
    BOF3_INJECT(EffectKindA3_AimPixels);
    BOF3_INJECT(EffectKindA3_MovePixels);
    BOF3_INJECT(EffectKindA2_DrawRing);
    BOF3_INJECT(EffectKindA7_Start);
    BOF3_INJECT(EffectKindA7_Grow);
    BOF3_INJECT(EffectKindA7_Hold);
    BOF3_INJECT(EffectKindA7_Shrink);
    BOF3_INJECT(EffectKindA8_Start);
    BOF3_INJECT(EffectKindA8_Bars);
    BOF3_INJECT(EffectKindA8_End);
    BOF3_INJECT(EffectKindA9_Start);
    BOF3_INJECT(EffectKindA9_Grow);
    BOF3_INJECT(EffectKindA9_Wait);
    BOF3_INJECT(EffectKindA9_Shrink);
    BOF3_INJECT(EffectKindAA_Start);
    BOF3_INJECT(EffectKindAA_FadeIn);
    BOF3_INJECT(EffectKindAA_FadeOut);
    BOF3_INJECT(EffectKindA7_DrawGlow);
    BOF3_INJECT(EffectKindA8_ClearBars);
    BOF3_INJECT(EffectKindA8_StartBar);
    BOF3_INJECT(EffectKindA8_StepBars);
    BOF3_INJECT(EffectKindA8_DrawBar);
    BOF3_INJECT(EffectKindA9_DrawDisc);
    BOF3_INJECT(EffectKindAA_DrawFill);
    BOF3_INJECT(EffectKindAB_Drops);
}

// The item lists' L1 / R1 corner as σ has it (DIV-0138, group IC-L of
// docs/button-unification-round.md 3; docs/button-shoulder-corner.md).
//
// Ten list draws frame their scroll bar with pieces of the UI page
// (0x1A080200, VRAM (832, 256), 4-bit) through Menu_DrawPiece 0x57D860 with
// flag bit 0 (the rectangles 0x663C8C). Five of those pieces carry a baked
// shoulder label: the L1 label is cut across the bar's top piece (0x15, its
// lit twin 0x2E, or 0x33 - its rows 19..23) and the cap under it (0x16 - its
// rows 0..1); the R1 label is in the bar's bottom piece (0x18, rows 8..15).
// The lists page with L1 and R1 (0x500C / 0xF00C, hard-coded; group HT asks
// σ there).
//
// Here, one place for the ten lists: each of the five pieces is drawn with
// its label box (columns 1..8 of those rows) refilled from a plain row of its
// own, and the glyph of σ(slot) drawn over the slot:
//   - L1 or R1, from the page's own label art (the PC's and every PSX disc's
//     page: one L1 and one R1 cell, measured identical across the 59 pages);
//   - a face button, or L1 / R1 on a PSP-based install (whose page has only
//     `L` / `R`, and no L1 / R1 art anywhere on the PSP disc), the glyph the
//     install's Config panel draws for that button: DIV-0051's icon under a
//     Latin layer, else the port's stand-in (0x66A2FC's codes and colours).
// With σ(slot) the slot's own button and the page's own art, the piece is
// drawn exactly as the original draws it: nothing changes at the identity.
#pragma once

#include <cstdint>

namespace shoulder_glyphs {

constexpr std::uint16_t kL1 = 0x04, kR1 = 0x08;

// The steps one labelled piece is drawn with - pure, so the self-test can
// raster them.
struct Step {
    enum Kind : unsigned char { kSprite, kGlyph, kPage } kind;
    int x, y;                     // where on screen (the call's x / y plus the step's offset)
    unsigned char u, v, w, h;     // kSprite: the page rectangle
    std::uint16_t button;         // kGlyph: the button whose font glyph is drawn
};
struct Plan {
    bool original = false;        // draw the piece as Capcom's code does
    int n = 0;
    Step s[32];
};

// A piece's page rectangle (u, v, w, h), as Menu_PieceRect hands it.
struct Rect { unsigned char u, v, w, h; };

// The label of piece `id` (flag bit 0 set): its slot's button (kL1 / kR1), or
// 0 for a piece with no label.
std::uint16_t SlotOf(unsigned id);

// The steps for piece `id` at (x, y). `rect(id)` gives a piece's rectangle
// (the art cells are read through the rectangles of 0x15, 0x16 and 0x18);
// `shown` is σ(slot); `page_art` false when the page's labels are not the
// L1 / R1 art (a PSP base); `force` the long path even at the identity (the
// self-test's geometry check).
Plan Make(unsigned id, int x, int y, const Rect& own, const Rect& r15, const Rect& r16, const Rect& r18,
          std::uint16_t shown, bool page_art, bool force);

// The font glyph of `button` as the install's Config panel draws it: the
// code bytes (NUL-ended) and the colour.
struct Code { unsigned char text[3]; int colour; };
Code GlyphCode(std::uint16_t button, bool latin);

// The live switch (BOF3X_ORIGINAL=ShoulderGlyphs) and the install's page.
bool On();
bool PageArt();

// After every module's self-test and after sigma::Arm: the self-test
// (BOF3X_SHADOW=shoulder_glyphs), the cache's base policy read, the switch.
void Arm();

}  // namespace shoulder_glyphs

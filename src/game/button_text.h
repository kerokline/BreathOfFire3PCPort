// The dialogue's button mentions, drawn as the button the Config has put the
// role on: the button unification round's group DT-2 (docs/button-dialogue.md
// section 4; DIV-0140). The pure half - no game memory, no DLL - so the
// two readers (Text_DrawString in text_draw.cpp, MsgBox_Step in msgbox.cpp)
// and a host-side check over the real text can share one decision.
//
// A mention is one of two shapes in an area's text block:
//
//   the button code   05 (0xB0 + n) 06  05 cc G   written by tools/loc_build.py
//                     (DT-1) in a language layer: Japan's bit 1 << n, then the
//                     release's own colour and icon G - the fallback, drawn as
//                     today until this resolves it.
//   the colour cue    05 cc G   the PC's own Chinese text with no layer, which
//                     carries no code: G one of the port's single-byte
//                     stand-ins (b .. g, X), and the colour, not G, names the
//                     button - circle 2, cross 1, triangle 6, square 5
//                     (DT-1's census, docs/button-dialogue.md 1.2).
//
// For either, the drawn button is Sigma(Japan's bit). When that is the button
// the fallback already shows, nothing changes: the reader draws its bytes
// (the identity is byte-equal by construction). Otherwise the icon is drawn
// as the drawn button's, in the drawn button's colour, from the same family
// of glyphs the fallback came from:
//
//   a Latin layer's icons   0x9E3..0x9E6 circle, cross, triangle, square
//                           (loc_build encode_char of the West's 0x80..0x83);
//                           L1 / R1 as DIV-0051's 0xA69 / 0xA6A (ICONS_AT + 4, + 5)
//   ja-JP's icons           0x9F6..0x9F9 (ja_cell_of 0x15 08 .. 0x15 0B);
//                           L1 / R1 as the port's stand-ins (no JP icon exists)
//   the port's stand-ins    the Config cell's own set, 0x461C00's six strings
//                           through 0x66A2FC (circle, cross, triangle, square,
//                           L1, R1), read live by the caller
//
// and the colour is 0x461C00's for that button: 2, 1, 6, 5, and 0 for L1 / R1.
#pragma once

#include <cstdint>

namespace button_text {

constexpr std::uint8_t kColourCtrl = 0x05, kCodeArg0 = 0xB0, kClose = 0x06;

// The six buttons in the Config cell's order (0x461C00 tests 0x20, 0x40,
// 0x10, 0x80, 4, 8) and the colours it draws each in.
constexpr std::uint16_t kShapeBit[6] = {0x20, 0x40, 0x10, 0x80, 0x04, 0x08};
constexpr std::uint8_t kShapeColour[6] = {2, 1, 6, 5, 0, 0};

constexpr unsigned kLatinIcons = 0x9E3;      // tools/loc_build.py: APPEND_AT + 0x80 - FIRST_CODE
constexpr unsigned kLatinShoulders = 0xA69;  // ICONS_AT + 4 (L1), + 5 (R1): DIV-0051's
constexpr unsigned kJapaneseIcons = 0x9F6;   // ja_cell_of(0x08, 0x15): JA_AT + 0x63

enum class Layer : std::uint8_t { kNone, kLatin, kJapanese };

struct Context {
    Layer layer = Layer::kNone;
    std::uint16_t (*sigma)(std::uint16_t) = nullptr;   // Japan's bit -> the button now
    std::uint8_t port[6] = {};                          // the stand-in byte of each button, kShapeBit's order
};

// What to draw in place of the fallback: its colour byte and its glyph bytes
// (one, or two when the first has bit 7), NUL after.
struct Icon {
    std::uint8_t colour = 0;
    std::uint8_t glyph[3] = {};
};

inline int ShapeOf(std::uint16_t bit) {
    for (int i = 0; i < 6; ++i)
        if (kShapeBit[i] == bit) return i;
    return -1;
}

// The PC's colour cue: the button a colour names, as a kShapeBit index; -1
// for a colour that names none.
inline int ShapeOfColour(std::uint8_t colour) {
    for (int i = 0; i < 4; ++i)
        if (kShapeColour[i] == colour) return i;
    return -1;
}

inline bool IsPortStandIn(std::uint8_t b) {
    return b == 'X' || (b >= 'b' && b <= 'g');
}

// The glyph of button `shape` in `family`, into icon.glyph.
inline void GlyphOf(const Context& c, Layer family, int shape, Icon& icon) {
    unsigned g = 0;
    if (family == Layer::kLatin) g = shape < 4 ? kLatinIcons + shape : kLatinShoulders + (shape - 4);
    else if (family == Layer::kJapanese && shape < 4) g = kJapaneseIcons + shape;
    if (g) {
        icon.glyph[0] = static_cast<std::uint8_t>(0x80 | (g >> 8));
        icon.glyph[1] = static_cast<std::uint8_t>(g);
        icon.glyph[2] = 0;
    } else {
        icon.glyph[0] = c.port[shape];
        icon.glyph[1] = 0;
    }
}

// The drawn button for Japan's `bit`, as a kShapeBit index; -1 (and `why`)
// when σ answers anything but one of the six.
inline int Drawn(const Context& c, std::uint16_t bit, const char*& why) {
    const int s = ShapeOf(c.sigma(bit));
    if (s < 0) why = "sigma gave no single face or shoulder button";
    return s;
}

// At `p`, a 0x05. True when p starts a complete button code - 05 Bn 06 05 cc
// G - whose drawn button is not the one the fallback shows: `out` then holds
// the icon. False otherwise, with `why` set when the bytes have the code's
// shape but cannot be drawn honestly (the caller aborts: rule 4).
inline bool ResolveCode(const Context& c, const std::uint8_t* p, Icon& out, const char*& why) {
    why = nullptr;
    if (p[0] != kColourCtrl || p[1] < kCodeArg0 || p[1] > kCodeArg0 + 7) return false;
    if (p[2] != kClose || p[3] != kColourCtrl || p[4] == 0 || p[5] == 0) return false;
    const auto bit = static_cast<std::uint16_t>(1u << (p[1] - kCodeArg0));
    const int code_shape = ShapeOf(bit);
    if (code_shape < 0 || code_shape > 3) {
        why = "a button code names no face button";
        return false;
    }
    const std::uint8_t* const g = p + 5;
    Layer family;
    int shown;   // the button the fallback shows
    if (g[0] & 0x80) {
        const unsigned index = (static_cast<unsigned>(g[0] & 0x7F) << 8) | g[1];
        if (index >= kLatinIcons && index < kLatinIcons + 4 && c.layer == Layer::kLatin) {
            family = Layer::kLatin;
            shown = static_cast<int>(index - kLatinIcons);
        } else if (index >= kJapaneseIcons && index < kJapaneseIcons + 4 && c.layer == Layer::kJapanese) {
            family = Layer::kJapanese;
            shown = static_cast<int>(index - kJapaneseIcons);
        } else {
            why = "a button code before a glyph that is not this layer's icon";
            return false;
        }
    } else if (IsPortStandIn(g[0])) {
        // A PC message a layer kept (loc_build pc_button_codes): the stand-in
        // is not the button, its colour is. Drawn in the layer's own family.
        family = c.layer;
        shown = ShapeOfColour(p[4]);
        if (shown < 0) {
            why = "a button code before a port stand-in in a colour that names no button";
            return false;
        }
    } else {
        why = "a button code before a glyph that is no icon";
        return false;
    }
    const int drawn = Drawn(c, bit, why);
    if (drawn < 0 || drawn == shown) return false;
    out.colour = kShapeColour[drawn];
    GlyphOf(c, family, drawn, out);
    return true;
}

// At `p`, a 0x05, in the PC's own text with no layer (the caller checks the
// layer and the area). True when p is the colour cue 05 cc G and the drawn
// button is not the one the colour names: `out` then holds the port's
// stand-in for the drawn button and its colour.
inline bool ResolveCue(const Context& c, const std::uint8_t* p, Icon& out, const char*& why) {
    why = nullptr;
    if (p[0] != kColourCtrl || p[1] == 0 || !IsPortStandIn(p[2])) return false;
    const int named = ShapeOfColour(p[1]);
    if (named < 0) return false;
    const int drawn = Drawn(c, kShapeBit[named], why);
    if (drawn < 0 || drawn == named) return false;
    out.colour = kShapeColour[drawn];
    GlyphOf(c, Layer::kNone, drawn, out);
    return true;
}

}  // namespace button_text

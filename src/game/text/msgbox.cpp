// The dialogue box, read to the last instruction of each function against the
// PSX twins where they pair (docs/msgbox.md). Forty-four functions: the
// script-side opener, the reset, the frame task, the control-code stepper and
// its 23-entry table, the eight state handlers under MsgBox_StateDispatch and
// their own stack-built tables, the box effect task and its five effects, the
// window slot allocator, the two remaining text pens, and (2026-10-03) the
// grow / shrink draw MsgBox_EffectDraw - DIV-0070 lives there.
//
// DIVERGENCE DIV-0006 lives here now: MsgBox_Step's one draw call goes to
// MsgBox_DrawChar (src/game/text_advance.cpp) rather than straight to
// Text_DrawAt, which is what the RetargetCall in that file did to Capcom's
// body. With no advance table loaded - every shipped file - MsgBox_DrawChar
// IS Text_DrawAt, so this is the original. The RetargetCall stays where it
// is: it is what keeps DIV-0006 alive under BOF3X_ORIGINAL=MsgBox_Step.
//
// DIVERGENCE (DIV-0140, docs/button-dialogue.md 4): MsgBox_Step draws a
// button mention as the button the Config has put its role on - the same
// decision as Text_DrawString's (text_draw.h ButtonText_Code / _Cue). Under
// BOF3X_ORIGINAL=MsgBox_Step Capcom's body runs and draws the text as it is.
#include "game/msgbox.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/button_text.h"
#include "game/dat_load.h"
#include "game/msgbox_callees.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_pairs.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace msgbox {

unsigned char g_effect_space_skips = 0;   // DIV-0070, set by MsgBox_Inject after the self-test

// DIV-0158, the faster text (BOF3X_TEXT_SPEED=1, text_speed= in bof3x.ini):
// Msg Speed's Med. prints at Fast's delays and Fast prints a page at once.
// Slow is the original's. Off (0, the default) the printer is Capcom's.
bool g_text_faster = false;
// ===========================================================================
// THE READING ROW (docs/reading-row.md 1.2 is the contract, section 3 this
// code). A message opened from a kind-18 side chunk (DatLoad_ReadingRow) is a
// sequence of pages `ruby 01 text [01 ruby 01 text]`; MsgBox_Step walks it as
// row pairs (ruby::Step) and MsgBox_StatePrint steps over its ruby rows
// (ruby::SkipRow), so the typewriter counts text glyphs only. With no side
// chunk loaded nothing here is reached: every test below is false before the
// first kind-18 chunk lands, and the walk, the printer and both openers are
// the original's (the shadow fuzz, the state hash).
namespace ruby {

// --- THE ROW TABLE: the reading row's geometry, in one place --------------
// Pen y per row of a page, from Msgbox_OriginY (window y + 6): ruby 1, text
// 1, ruby 2, text 2. Bottom-aligned (PR, docs/reading-row.md 2): the last
// text row sits on the plain page's third row, O + 28, so its glyph
// bottoms, the next-page arrow and the room under them are the original
// page's. The owner's call 1 (2026-10-22, off PR's sheets): the gap is even,
// "2px top 2px bottom" - a reading's ink 2 units below the text row above it
// and 2 units above its own text row. Against PR's gap-2 table (4 above, 1
// below) that is the ruby row one unit higher relative to its text (7, not
// 6) and a pair pitch of 19. A glyph draws at pen y + 1. The plain newline's
// 14 stays for every message not from a side chunk.
struct RowTable {
    short row_y[4];
};
constexpr RowTable kRowTable = {{2, 9, 21, 28}};
constexpr int kRubyDy = 1;              // a ruby quad's top below its row's pen y: Text_DrawAt's own + 1
constexpr int kHalfCell = 6;            // a ruby half-cell: 0x09's advance
// A reading glyph is its whole 12-unit texture cell on a 6 x 6 quad (scale
// 1/2: texel for texel at the PC's 2x, probe 3), advancing its DIV-0006
// advance times 1/2 - a kana 6, a Latin letter 4 (the owner, 2026-10-22:
// the Latin row stays at 1/2).
constexpr int kScaleNum = 1, kScaleDen = 2;
constexpr int kPairs = 2;               // row pairs per page (reading-row.md 1.2)
const RowTable& Rows() { return kRowTable; }
// The scale of the glyph at `at`: (num, den), 1/2 for every glyph.
void Scale(const unsigned char*, int& num, int& den) {
    num = kScaleNum;
    den = kScaleDen;
}

// Armed by MsgBox_Inject once MsgBox_Step and MsgBox_StatePrint are both
// ours: Capcom's two (BOF3X_ORIGINAL) would count and draw a ruby row as
// text and fault on the insert marker, so under either the openers keep the
// block's own message.
bool g_armed;
bool g_told_unarmed;
// The study key (BOF3X_RUBY_KEY, F4 by default): while set, the walk lays out
// and reveals every reading as before but emits none of its glyphs (nor a
// 0x13 insert's) - the page, its rows, its breaks and its count unchanged.
bool g_hidden;
unsigned g_key_vk;   // 0: no key
// The last composed message opened, for the refusals below.
unsigned g_tag, g_half, g_slot;

[[noreturn]] void Refuse(const unsigned char* at, const char* what) {
    bof3::Fatal("reading row: tag 0x%X half %u slot %u: byte 0x%02X %s", g_tag, g_half, g_slot,
                static_cast<unsigned>(at[0]), what);
}

// The openers' repoint: the composed copy of (tag, half, slot) when the side
// chunk has one, else `plain`, the block's own message.
const unsigned char* Open(unsigned tag, unsigned half, unsigned slot, const unsigned char* plain) {
    const unsigned char* const composed = DatLoad_ReadingRow(tag, half, slot);
    if (!composed) return plain;
    if (!g_armed) {
        if (!g_told_unarmed)
            bof3::Log("reading row: MsgBox_Step or MsgBox_StatePrint is Capcom's (BOF3X_ORIGINAL); composed messages "
                      "are not opened");
        g_told_unarmed = true;
        return plain;
    }
    g_tag = tag;
    g_half = half;
    g_slot = slot;
    bof3::Log("reading row: tag 0x%X half %u slot %u opened composed", tag, half, slot);
    return composed;
}

// "This message is annotated": its base came from a side chunk.
bool Annotated(const unsigned char* base) { return g_armed && DatLoad_ReadingRowHolds(base); }

// A ruby row from `at`: the byte that ends it - the 0x01 before its text row,
// or, for a row with nothing in it, the page's or the message's end (0x00
// 0x02 0x14 0x16, a page the builder left a single empty row). `color` takes
// the colour codes on the way. Anything else the contract does not allow in a
// ruby row (reading-row.md 1.2) is refused.
const unsigned char* RowEnd(const unsigned char* at, unsigned char* color) {
    for (const unsigned char* p = at;;) {
        const unsigned char c = p[0];
        if (c == 0x01) return p;
        if (c == 0x00 || c == 0x02 || c == 0x14 || c == 0x16) {
            if (p != at) Refuse(p, "ends a ruby row that has no text row under it");
            return p;
        }
        if (c == 0x05) {
            if (color) *color = p[1];
            p += 2;
        } else if (c == 0x06) {
            if (color) *color = 0;
            ++p;
        } else if (c == 0x09 || c == 0x12 || c == 0x13) {
            ++p;
        } else if (c & 0x80) {
            p += 2;
        } else if (c > 0x16 && c != 0x20 && c != 0x2A && c != 0x3C) {
            ++p;   // a one-byte glyph (a Latin reading); the hanging brackets come as two-byte
                   // codes, a space as the composed font's blank cell (reading-row.md 2)
        } else {
            Refuse(p, "is not allowed in a ruby row");
        }
    }
}

// Whether `at` is where a ruby row starts, by the row parity of the page
// walked from `base`: rows alternate ruby / text from a page's start, and a
// page starts at the base (after a leading 0x0C nn) and after 0x02 or 0x16 nn.
// The tokens are the stepper's (0x04 0x05 0x07 0x08 0x0A 0x0C 0x0F and a
// glyph from 0x80 two bytes).
bool AtRubyRow(const unsigned char* base, const unsigned char* at) {
    const unsigned char* p = base;
    if (p[0] == 0x0C) p += 2;
    bool ruby = true;
    while (p < at) {
        const unsigned char c = p[0];
        if (c == 0x00 || c == 0x14) return false;
        if (c == 0x01) {
            ruby = !ruby;
            ++p;
        } else if (c == 0x02) {
            ruby = true;
            ++p;
        } else if (c == 0x16) {
            ruby = true;
            p += 2;
        } else if (c == 0x04 || c == 0x05 || c == 0x07 || c == 0x08 || c == 0x0A || c == 0x0C || c == 0x0F ||
                   (c & 0x80)) {
            p += 2;
        } else {
            ++p;
        }
    }
    return p == at && ruby;
}

// MsgBox_StatePrint's step over a ruby row: from `at`, the first byte of the
// text row under it when `at` starts a ruby row of the annotated message at
// `base`, else `at`. A ruby row costs the printer nothing - no glyph counted,
// no delay - so the count 0x7DEE59 is text glyphs only.
const unsigned char* SkipRow(const unsigned char* base, const unsigned char* at) {
    if (!Annotated(base)) return at;
    if (!(at == base || at[-1] == 0x01 || (base[0] == 0x0C && at == base + 2))) return at;
    if (!AtRubyRow(base, at)) return at;
    const unsigned char* const end = RowEnd(at, nullptr);
    return end[0] == 0x01 ? end + 1 : at;
}

// A text row as the walk found it: where each segment's glyphs sit, each
// insert's drawn width, and the count at which each glyph is drawn. Segment
// 0 is the row up to its first insert (0x03 0x04 0x07 0x08), segment k the
// row after the k-th (tools/ruby_build.py RowLayout).
constexpr int kMaxSegs = 16, kMaxGlyphs = 96;
struct TextRow {
    int segs;                      // inserts + 1
    int width[kMaxSegs];           // width[k]: the k-th insert's drawn width, k >= 1
    const unsigned char* text[kMaxSegs];   // text[k]: its first character, as the walk reads it
    int chars[kMaxSegs];           // chars[k]: the characters the substitution allows (8, 32, 16)
    int ins_first[kMaxSegs];       // ins_first[k]: the count that draws its first character
    int seg_first[kMaxSegs];       // the segment's first entry in cell[] / ordinal[]
    int seg_glyphs[kMaxSegs];      // its own glyphs (an insert's are the insert's)
    int seg_before[kMaxSegs];      // glyphs drawn before it began
    int cells;
    short cell[kMaxGlyphs];        // the glyph's cell in its segment (-1: hung into the margin)
    short ordinal[kMaxGlyphs];     // the count that draws it (1 = the page's first glyph)
    bool pen[kMaxGlyphs];          // drawn under the offset pen (0x0D .. 0x0E): at the effect's offset
    bool ins_pen[kMaxSegs];        // ins_pen[k]: the k-th insert's first character under the offset pen
    bool effect;                   // a glyph of the row is MsgBox_EffectDraw's (the pen while flag 8 is set)
    int before;                    // glyphs on the page before the row
    int first;                     // the count that draws the row's first glyph (its inserts' too),
                                   // or `before` for a row with none
};

// THE REVEAL RULE - the owner's call 3 (reading-row-round.md 6) is open; this
// is the recommended rule: a reading appears with the first glyph of its
// word, the first glyph of its segment whose cell is at or after the cell its
// first half-cell begins (a reading laid one half-cell left of its stem still
// waits for the stem). A reading past its segment's last glyph waits for that
// glyph; over a segment with none, for what was drawn before it. The other
// rule, the whole ruby row with the row's first glyph, is the one line
// `return row.first;` here.
// The glyph a reading waits for (Reveal's stem): its entry in cell[], or -1
// over a segment with no glyph of its own.
int Stem(const TextRow& row, int seg, int half) {
    const int cell = (half + 1) >> 1;
    const int from = row.seg_first[seg], n = row.seg_glyphs[seg];
    for (int i = from; i < from + n; ++i)
        if (row.cell[i] >= cell) return i;
    return n ? from + n - 1 : -1;
}
int Reveal(const TextRow& row, int seg, int half) {
    const int i = Stem(row, seg, half);
    return i >= 0 ? row.ordinal[i] : row.seg_before[seg];
}

// A ruby glyph: Text_EmitGlyph's record (the glyph's whole 12-unit cell, the
// texture window a flat 12 wide) on a q x q quad, the page scroll's clip pair
// (u, v) - Text_DrawString's, by the colour byte's high nibble - scaled with
// it. `index` is the glyph as Text_DrawString derives it (Index); one past
// the loaded table is refused, `at` naming the byte.
unsigned Index(const unsigned char* at) {
    return (at[0] & 0x80) ? ((at[0] & 0x7Fu) << 8) + at[1] : at[0] - 0x26u;
}
void Emit(int x, int y, unsigned index, const unsigned char* at, unsigned clut_byte, int q) {
    const unsigned glyphs = TextDraw_GlyphCount();
    if (index > (glyphs ? glyphs - 1 : 0xA00u)) Refuse(at, "is a glyph past the loaded table, in a ruby row");
    if (g_hidden) return;   // the study key: laid out, not drawn
    const std::uint8_t* const pair = reinterpret_cast<const std::uint8_t*>(::bof3::Img(bof3::addr::Text_ClipPairs)) + ((clut_byte & 0xFF) >> 4) * 2;
    const int u = pair[0], v = pair[1];
    unsigned char* const p = Gfx_PacketNext;
    const auto put = [](unsigned char* to, unsigned value) {
        const auto word = static_cast<std::uint16_t>(value);
        std::memcpy(to, &word, sizeof word);
    };
    put(p + 0x16, index);
    put(p + 0x0E, 0x7800u | (clut_byte & 0xFu));
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    const auto row = static_cast<unsigned char>(u);
    const auto far_row = static_cast<unsigned char>(u - v + 12);
    p[0x0C] = 0;
    p[0x0D] = row;
    p[0x14] = 0x0C;
    p[0x15] = row;
    p[0x1C] = 0;
    p[0x1D] = far_row;
    p[0x24] = 0x0C;
    p[0x25] = far_row;
    const int top = y + (u * q + 6) / 12, bottom = y + ((12 - v + u) * q + 6) / 12;
    put(p + 0x08, static_cast<unsigned>(x));
    put(p + 0x0A, static_cast<unsigned>(top));
    put(p + 0x10, static_cast<unsigned>(x + q));
    put(p + 0x12, static_cast<unsigned>(top));
    put(p + 0x18, static_cast<unsigned>(x));
    put(p + 0x1A, static_cast<unsigned>(bottom));
    put(p + 0x20, static_cast<unsigned>(x + q));
    put(p + 0x22, static_cast<unsigned>(bottom));
    g.set_code6c(p);
    g.set_semitrans(p, 1);
    g.commit(1, 0x28);
}

// A text row, walked as MsgBox_Step walks a row - the same substitutions,
// colour codes (DIV-0140's button mentions included), the hang, the draw
// through g.draw_char (DIV-0006) at pen y `y` - drawing while `drawn` is
// below `count` and measuring to the row's end either way. Returns the byte
// that ends the row: 0x01, or the page's or message's end (0x02 0x16 0x14,
// 0x00 outside a substitution). A row end inside a substitution is refused.
//
// The offset pen (0x0D on, 0x0E off; since 2026-10-22 composed, DIV-0155 /
// 0156): `pen` carries it from row to row of the page, as MsgBox_Step's flag
// bit 5 does (a newline leaves it set; each page's walk starts with it off).
// Under it a glyph is drawn as MsgBox_Step draws one: with flag 8 of 0x7DEE44
// (effects 2 and 3) by MsgBox_EffectDraw at MsgBox_PenX / Msgbox_PenY in
// memory - Msgbox_PenY is the row's pen y here, as the original's newline
// leaves it - else by g.draw_char at MsgBox_PenX + the s8 0x7DEE5A and the
// row's y + the s8 0x7DEE5B. Measured past the count, the effect draw's
// advance is 12 + P (P the word 0x7DEE68), every other glyph's DIV-0006's.
const unsigned char* WalkText(const unsigned char* at, int y, int count, int& drawn, unsigned char& color,
                              TextRow& row, short& last_x, bool& pen) {
    unsigned char flags = 0;   // bit 0 in a substitution, as MsgBox_Step's
    const unsigned char* resume = nullptr;
    unsigned char sub_count = 0;
    int button = 0;
    button_text::Icon icon;
    short pen_x = MsgBox_LineX;
    MsgBox_PenX = pen_x;
    int seg_x = 0, insert_from = 0;
    row.segs = 1;
    row.seg_first[0] = 0;
    row.seg_glyphs[0] = 0;
    row.seg_before[0] = drawn;
    row.cells = 0;
    row.before = drawn;
    row.first = drawn;
    row.effect = false;
    Msgbox_PenY = static_cast<std::uint16_t>(y);   // MsgBox_EffectDraw's y, as MsgBox_Step's newline leaves it
    const auto begin_insert = [&](const unsigned char* code) {
        if (flags & 1) Refuse(code, "starts an insert inside an insert");
        if (row.segs == kMaxSegs) Refuse(code, "starts one insert too many for a row");
        insert_from = pen_x;
    };
    // Once a substitution's pointer and count are set: the k-th insert's text
    // is the byte after `at` (the loop's ++at), sub_count - 1 characters.
    const auto note_insert = [&]() {
        const int k = row.segs;
        row.text[k] = at + 1;
        row.chars[k] = sub_count - 1;
        row.ins_first[k] = drawn + 1;
        row.ins_pen[k] = pen;
    };
    const auto end_insert = [&](const unsigned char*) {
        const int k = row.segs++;
        row.width[k] = pen_x - insert_from;
        row.seg_first[k] = row.cells;
        row.seg_glyphs[k] = 0;
        row.seg_before[k] = drawn;
        seg_x = 0;
    };
    for (;;) {
        if (flags & 1) {
            sub_count = static_cast<unsigned char>(sub_count - 1);
            if (sub_count == 0) {
                at = resume + 1;
                flags = static_cast<unsigned char>(flags & 0xFEu);
                end_insert(at);
            }
        }
        const unsigned char c = at[0];
        bool glyph = c > 0x16;
        if (!glyph) {
            switch (c) {
            case 0x00:
                if (flags & 1) {
                    at = resume;
                    ++resume;
                    flags = static_cast<unsigned char>(flags & 0xFEu);
                    end_insert(at);
                    break;
                }
                return at;
            case 0x01:
            case 0x02:
            case 0x14:
            case 0x16:
                if (flags & 1) Refuse(at, "ends a row inside an insert");
                return at;
            case 0x03: {
                begin_insert(at);
                flags = static_cast<unsigned char>(flags | 1u);
                resume = at;
                const unsigned member = B(kNameMap + ObjTrio[0].char_id);
                sub_count = 9;
                at = At(kCharRecords + 164u * member);
                note_insert();
                break;
            }
            case 0x04: {
                begin_insert(at);
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned member = at[0];
                sub_count = 9;
                at = At(kCharRecords + 164u * member);
                note_insert();
                break;
            }
            case 0x05: {
                const unsigned char* const control = at;
                ++at;
                color = at[0];
                if (button == 1 || (button == 0 && ButtonText_Cue(control, icon))) {   // DIV-0140
                    color = icon.colour;
                    button = 2;
                } else if (button == 0 && ButtonText_Code(control, icon)) {
                    button = 1;
                }
                break;
            }
            case 0x06:
                color = 0;
                break;
            case 0x07: {
                begin_insert(at);
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned record = at[0];
                sub_count = 0x21;
                at = At(kTextRecords + (record << 5));
                note_insert();
                break;
            }
            case 0x08: {
                begin_insert(at);
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned entry = at[0];
                const unsigned char* const pool = At(kScriptPool + L(kPoolSelector));
                sub_count = 0x11;
                std::uint16_t off;
                std::memcpy(&off, pool + 2 * entry, sizeof off);
                at = pool + off;
                note_insert();
                break;
            }
            case 0x0A:
            case 0x0C:
            case 0x0F:
                ++at;
                break;
            case 0x0D:
                pen = true;
                break;
            case 0x0E:
                pen = false;
                break;
            case 0x12:
            case 0x13:
            case 0x15:
                glyph = true;
                break;
            default:   // 0x09 0x0B 0x10 0x11: one byte, nothing drawn
                break;
            }
        }
        if (glyph) {
            const unsigned char* const shown = button == 2 ? icon.glyph : at;
            button = 0;
            bool hung = false;
            if ((c == 0x2A || c == 0x3C) &&
                static_cast<std::uint16_t>(pen_x) == static_cast<std::uint16_t>(MsgBox_LineX)) {
                pen_x = static_cast<short>(pen_x - 12);
                MsgBox_PenX = pen_x;
                hung = true;
            }
            const bool effect = pen && (B(kFlags) & 8);
            if (effect) row.effect = true;
            if (drawn < count) {
                const auto clut = static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color);
                if (effect) {
                    g.effect_draw(color, shown);   // MsgBox_Step's own call: only the low nibble reaches it
                } else if (pen) {
                    const int x = static_cast<short>(MsgBox_PenX + Msgbox_OffsetX);
                    const int oy = static_cast<short>(y + Msgbox_OffsetY);
                    g.draw_char(x, oy, clut, 1, shown);
                } else {
                    g.draw_char(pen_x, y, clut, 1, shown);
                }
                pen_x = static_cast<short>(MsgBox_PenX + 12);
                last_x = pen_x;
            } else {
                // Measured, not drawn: the pen MsgBox_DrawChar would leave
                // (DIV-0006's advance; the hang's draw leaves it, the + 12
                // brings it back to the line start), or MsgBox_EffectDraw's
                // 12 + P.
                pen_x = static_cast<short>(pen_x + (hung ? 12
                                                    : effect ? 12 + static_cast<std::int16_t>(W(kEffectOff))
                                                             : TextAdvance_Of(shown)));
            }
            MsgBox_PenX = pen_x;
            if (at[0] & 0x80) ++at;
            ++drawn;
            if (row.first == row.before) row.first = drawn;
            if (!(flags & 1)) {
                if (row.cells == kMaxGlyphs) Refuse(at, "is one glyph too many for a row");
                if (hung) seg_x -= 1;
                row.cell[row.cells] = static_cast<short>(seg_x++);
                row.ordinal[row.cells] = static_cast<short>(drawn);
                row.pen[row.cells] = pen;
                ++row.cells;
                ++row.seg_glyphs[row.segs - 1];
            }
        }
        ++at;
    }
}

// The k-th insert of the text row drawn at the reading scale from x, y - the
// second-language marker 0x13 (reading-row.md 1.2): the substitution's own
// characters as the walk read them (its count, a NUL ends it), colour codes
// taken, a pair code (DIV-0057) as its two glyphs, a space advancing; all of
// it revealed with the insert's first character. Returns its drawn width.
// Under the offset pen it takes the effect's offset with its insert; in a row
// MsgBox_EffectDraw draws, it is laid out and not drawn (DrawRuby).
int DrawInsert(const TextRow& row, int k, int x, int y, int count, unsigned char color, const unsigned char* marker) {
    const bool shown = row.ins_first[k] <= count && !row.effect;
    if (row.ins_pen[k]) {
        x += Msgbox_OffsetX;
        y += Msgbox_OffsetY;
    }
    int w = 0;
    const unsigned char* t = row.text[k];
    for (int left = row.chars[k]; left > 0 && t[0] != 0; --left) {
        const unsigned char c = t[0];
        if (c == 0x05) {
            color = t[1];
            t += 2;
        } else if (c == 0x06) {
            color = 0;
            ++t;
        } else if (c == 0x20) {
            int num, den;
            Scale(t, num, den);
            w += TextAdvance_Of(t) * num / den;
            ++t;
        } else if ((c & 0x80) || c > 0x16) {
            const unsigned index = Index(t);
            const auto clut = static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color);
            int num, den;
            Scale(t, num, den);
            const int q = 12 * num / den;
            std::uint32_t pa, pb;
            if (TextPair_Of(index, &pa, &pb)) {
                if (shown) {
                    Emit(x + w, y, pa, t, clut, q);
                    Emit(x + w + TextAdvance_OfGlyph(pa) * num / den, y, pb, t, clut, q);
                }
            } else if (shown) {
                Emit(x + w, y, index, t, clut, q);
            }
            w += TextAdvance_Of(t) * num / den;
            t += (c & 0x80) ? 2 : 1;
        } else {
            Refuse(marker, "(the second-language marker) meets a control its insert cannot draw at reading size");
        }
    }
    return w;
}

// A ruby row's glyphs at pen y `y`: x in units from the line start, 0x09 a
// half-cell, 0x12 the drawn width of the text row's next insert, 0x13 that
// insert drawn at the reading scale (k counted from 1 at the row's first
// marker; one row holds one kind of marker), a glyph its cell at 1/2 on its
// advance at 1/2, revealed by Reveal with the first glyph of its word - a run
// of glyphs with only colour codes between them is one reading.
//
// The offset pen and the effect presets (2026-10-22, DIV-0155 / 0156): a
// reading whose stem (Stem, the glyph it waits for) was drawn under the
// offset pen is drawn at the same offset as that glyph - the s8 0x7DEE5A /
// 0x7DEE5B the effects rock, wander and raise - so it moves with its word. A
// row MsgBox_EffectDraw draws in (the grow and the shrink, flag 8 of
// 0x7DEE44) shows no reading while the effect runs: the effect draw is left
// unannotated, its glyphs a scaled cell on a moving pen the half-cell grid
// does not follow; the readings return when the effect clears flag 8.
void DrawRuby(const unsigned char* at, const unsigned char* end, int y, int count, unsigned char color,
              const TextRow& row) {
    const int line = MsgBox_LineX;
    int x = 0, seg = 0, seg_x0 = 0, reveal = 0, dx = 0, dy = 0;
    bool in_run = false;
    unsigned char marker = 0;
    for (const unsigned char* p = at; p < end;) {
        const unsigned char c = p[0];
        if (c == 0x05) {
            color = p[1];
            p += 2;
        } else if (c == 0x06) {
            color = 0;
            ++p;
        } else if (c == 0x09) {
            x += kHalfCell;
            in_run = false;
            ++p;
        } else if (c == 0x12 || c == 0x13) {
            if (marker && marker != c) Refuse(p, "is a second kind of insert marker in one ruby row");
            marker = c;
            if (++seg >= row.segs) Refuse(p, "(an insert marker) has no insert under it in the text row");
            x += c == 0x12 ? row.width[seg] : DrawInsert(row, seg, line + x, y + kRubyDy, count, color, p);
            seg_x0 = x;
            in_run = false;
            ++p;
        } else {
            // A glyph: RowEnd has let nothing else through.
            if (!in_run) {
                const int half = (x - seg_x0) / kHalfCell;
                const int stem = Stem(row, seg, half);
                reveal = Reveal(row, seg, half);
                const bool moved = stem >= 0 && row.pen[stem];
                dx = moved ? static_cast<int>(Msgbox_OffsetX) : 0;
                dy = moved ? static_cast<int>(Msgbox_OffsetY) : 0;
            }
            in_run = true;
            int num, den;
            Scale(p, num, den);
            const int q = 12 * num / den;
            if (reveal <= count && !row.effect) {
                const unsigned index = Index(p);
                std::uint32_t pa, pb;
                if (TextPair_Of(index, &pa, &pb)) Refuse(p, "is a pair code (DIV-0057), in a ruby row");
                Emit(line + x + dx, y + kRubyDy + dy, index, p,
                     static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color), q);
            }
            x += TextAdvance_Of(p) * num / den;
            p += (c & 0x80) ? 2 : 1;
        }
    }
}

// MsgBox_Step for an annotated message: the page from the base, pair by pair
// - the ruby row's bytes, the text row walked (drawn up to the count,
// measured to its end), then the ruby row drawn over it - until the page or
// the message ends. The prologue is MsgBox_Step's: the pen at the box origin,
// nothing drawn at a count of 0. The pen left behind is the last drawn text
// glyph's x and the y of the last text row the walk entered, which is what
// MsgBox_State3Arrow places the next-page arrow off.
void Step() {
    const unsigned char* at = Ptr(kBase);
    const short line = MsgBox_LineX;
    const auto origin = Msgbox_OriginY;
    MsgBox_PenX = line;
    Msgbox_PenY = origin;
    if (Msgbox_StepCount == 0) return;
    const int count = Msgbox_StepCount;
    if (at[0] == 0x0C) at += 2;   // the leading placement MsgBox_Reset ate into the read pointer
    unsigned char color = 0;
    int drawn = 0;
    short last_x = line;
    auto last_y = origin;
    static TextRow row;   // static: the box's frame runs on a task stack
    bool pen = false;     // the offset pen, MsgBox_Step's flag bit 5: off at each page's walk
    for (int pair = 0;; ++pair) {
        unsigned char ruby_color = color;
        const unsigned char* const end = RowEnd(at, &ruby_color);
        if (end[0] != 0x01) break;   // an empty page, or the message's end
        if (pair == kPairs) Refuse(at, "starts a third row pair on a page");
        const auto text_y = static_cast<std::uint16_t>(origin + Rows().row_y[2 * pair + 1]);
        const auto ruby_y = static_cast<int>(static_cast<std::uint16_t>(origin + Rows().row_y[2 * pair]));
        unsigned char text_color = ruby_color;
        const int before = drawn;
        const unsigned char* const stop = WalkText(end + 1, text_y, count, drawn, text_color, row, last_x, pen);
        DrawRuby(at, end, ruby_y, count, color, row);
        if (before < count || (row.first == before && before <= count)) last_y = text_y;
        color = text_color;
        if (stop[0] != 0x01) break;
        at = stop + 1;
    }
    MsgBox_PenX = last_x;
    Msgbox_PenY = last_y;
}

}  // namespace ruby
}  // namespace msgbox

using namespace msgbox;

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

namespace {
MsgBoxDraw* g_record;
unsigned g_record_max, g_record_n;

const unsigned char* __cdecl RecordDrawChar(int x, int y, int color, int, const unsigned char* text) {
    if (g_record_n < g_record_max)
        g_record[g_record_n] = {x, y, color, {text[0], static_cast<unsigned char>(text[0] & 0x80 ? text[1] : 0)}};
    ++g_record_n;
    return text + 1;
}
}  // namespace

unsigned MsgBox_StepRecord(const unsigned char* msg, unsigned glyphs, MsgBoxDraw* out, unsigned max) {
    const auto saved_draw = g.draw_char;
    const unsigned char* const saved_base = Ptr(kBase);
    const unsigned saved_count = Msgbox_StepCount, saved_flags = W(kFlags), saved_high = ItemUse_ColorHigh;
    const unsigned saved_origin = Msgbox_OriginY, saved_pen_y = Msgbox_PenY;
    const short saved_line = MsgBox_LineX, saved_pen_x = MsgBox_PenX;
    g_record = out;
    g_record_max = max;
    g_record_n = 0;
    g.draw_char = &RecordDrawChar;
    SetPtr(kBase, msg);
    Msgbox_StepCount = glyphs;
    SetW(kFlags, 0);
    ItemUse_ColorHigh = 0;
    Msgbox_OriginY = 0x20;
    MsgBox_LineX = 0x10;
    MsgBox_Step();
    g.draw_char = saved_draw;
    SetPtr(kBase, saved_base);
    Msgbox_StepCount = saved_count;
    SetW(kFlags, saved_flags);
    ItemUse_ColorHigh = saved_high;
    Msgbox_OriginY = saved_origin;
    Msgbox_PenY = saved_pen_y;
    MsgBox_LineX = saved_line;
    MsgBox_PenX = saved_pen_x;
    return g_record_n;
}

void MsgBox_Inject() {
    if (bof3::WantsShadow("msgbox")) msgbox::SelfTest();
    // DIVERGENCE DIV-0070: after the self-test, which compares Capcom's spaces.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MsgBoxEffectSpaceSkips",
                         Low32(&g_effect_space_skips), &was, &is, 1);
        bof3::Log("DIV-0070    a space in a growing or shrinking shout commits no primitive");
    }
}

// After TextMsgBoxFromMsgbox_Inject, which binds MsgBox_Step and MsgBox_StatePrint: IsEnabled answers "not ours"
// for a function no Inject has named yet.
void MsgBox_ArmAfterBinds() {
    // The reading row needs the walk and the printer both ours.
    ruby::g_armed = bof3::IsEnabled(bof3::addr::MsgBox_Step) && bof3::IsEnabled(bof3::addr::MsgBox_StatePrint);
    // The study key: BOF3X_RUBY_KEY = f1..f12 (any case; default f4) or none.
    {
        char key[16] = "f4";
        const DWORD n = GetEnvironmentVariableA("BOF3X_RUBY_KEY", key, sizeof key);
        if (n >= sizeof key) bof3::Fatal("BOF3X_RUBY_KEY is %lu characters: f1..f12 or none", n);
        for (char* c = key; *c; ++c)
            if (*c >= 'A' && *c <= 'Z') *c = static_cast<char>(*c - 'A' + 'a');
        if (std::strcmp(key, "none") == 0) {
            ruby::g_key_vk = 0;
        } else {
            const int f = (key[0] == 'f' && key[1] >= '1' && key[1] <= '9') ? std::atoi(key + 1) : 0;
            char again[8];
            std::snprintf(again, sizeof again, "f%d", f);
            if (f < 1 || f > 12 || std::strcmp(again, key) != 0)
                bof3::Fatal("BOF3X_RUBY_KEY is \"%s\": f1..f12 or none", key);
            ruby::g_key_vk = static_cast<unsigned>(VK_F1 + f - 1);
        }
    }
    // The faster text (DIV-0158): BOF3X_TEXT_SPEED = 0 (the original's, the
    // default) or 1. Its home is the printer, so it needs the printer ours.
    {
        char v[8] = "0";
        const DWORD n = GetEnvironmentVariableA("BOF3X_TEXT_SPEED", v, sizeof v);
        if (n >= sizeof v || (std::strcmp(v, "0") != 0 && std::strcmp(v, "1") != 0))
            bof3::Fatal("BOF3X_TEXT_SPEED is \"%s\": 0 or 1", n >= sizeof v ? "(too long)" : v);
        msgbox::g_text_faster = v[0] == '1';
        if (msgbox::g_text_faster && !bof3::IsEnabled(bof3::addr::MsgBox_StatePrint))
            bof3::Fatal("BOF3X_TEXT_SPEED=1 needs MsgBox_StatePrint ours, and it is disabled");
        if (msgbox::g_text_faster) bof3::Log("text speed: Med. at Fast, Fast a page at once (DIV-0158)");
    }
}

bool MsgBox_ReadingRowKey(unsigned vk, bool repeat) {
    if (ruby::g_key_vk == 0 || vk != ruby::g_key_vk) return false;
    if (!repeat) {
        ruby::g_hidden = !ruby::g_hidden;
        bof3::Log("reading row: %s (the study key)", ruby::g_hidden ? "hidden" : "shown");
    }
    return true;
}

const unsigned char* MsgBox_ReadingRowSystem(unsigned id, const unsigned char* plain) {
    return ruby::Open(0x4000, (id >> 14) & 3u, id & 0x3FFFu, plain);
}

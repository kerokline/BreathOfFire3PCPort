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

const Callees kOriginals = {
    {MsgBox_StatePrint, MsgBox_StateDelay, MsgBox_State2, MsgBox_State3, MsgBox_State4, MsgBox_State5,
     MsgBox_State6, MsgBox_State7},
    {MsgBox_State2Press, MsgBox_State2Closed},
    {MsgBox_State3Arrow, MsgBox_State3Step},
    {MsgBox_State3Arrow, MsgBox_ChoiceOpen, MsgBox_ChoiceWaitOpen, MsgBox_ChoiceInput, MsgBox_ChoiceWaitShut,
     reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(kChoiceCommit)), MsgBox_ChoiceReopen,
     MsgBox_ChoiceDone},
    {MsgBox_MenuOpen, MsgBox_MenuWaitOpen, MsgBox_MenuInput,
     reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(kMenuCommit)), MsgBox_MenuDone,
     MsgBox_State3Step},
    {MsgBox_State7Delay, MsgBox_State2Closed},
    {reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(kRetOnly)), MsgBox_EffectShake,
     MsgBox_EffectGrow, MsgBox_EffectGrow, MsgBox_EffectWander, MsgBox_EffectRise},
    {MsgBox_ShakeOut, MsgBox_ShakeBack},
    {MsgBox_GrowStart, MsgBox_GrowStep},
    {MsgBox_RiseStart, MsgBox_RiseStep},
    MsgBox_Reset,
    Window_Alloc,
    MsgBox_StateDispatch,
    MsgBox_EffectTask,
    MsgBox_Step,
    MsgBox_Reopen,
    Sound_PlayEffect,
    MsgBox_DrawChar,   // DIV-0006; Text_DrawAt when no advance table is loaded
    MsgBox_EffectDraw,
    reinterpret_cast<void (__cdecl*)(int, int)>(static_cast<std::uintptr_t>(kPageArrow)),
    reinterpret_cast<unsigned (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(kAutoRepeat)),
    Text_DrawString,
    Gpu_SetCode6C,
    Gpu_SetSemiTrans,
    Gfx_CommitPrim,
    Gpu_GetClut,
};
Callees g = kOriginals;
unsigned char g_effect_space_skips = 0;   // DIV-0070, set by MsgBox_Inject after the self-test

namespace {

// The original's `call dword ptr [esp + eax*4]` has no bound: a byte above
// the table's length calls whatever lies past the dispatcher's own stack
// frame. Every writer of each index byte is enumerated in docs/msgbox.md
// section 2 and none can leave the range, so this refuses instead - loudly,
// and in the one configuration the original would have crashed in.
void Call(void (__cdecl* const* table)(), unsigned n, unsigned index, const char* who) {
    if (index >= n) bof3::Fatal("msgbox: %s reached index %u of %u", who, index, n);
    table[index]();
}

}  // namespace

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
int Reveal(const TextRow& row, int seg, int half) {
    const int cell = (half + 1) >> 1;
    const int from = row.seg_first[seg], n = row.seg_glyphs[seg];
    for (int i = from; i < from + n; ++i)
        if (row.cell[i] >= cell) return row.ordinal[i];
    if (n) return row.ordinal[from + n - 1];
    return row.seg_before[seg];
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
    const std::uint8_t* const pair = reinterpret_cast<const std::uint8_t*>(bof3::addr::Text_ClipPairs) + ((clut_byte & 0xFF) >> 4) * 2;
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
// 0x00 outside a substitution). The offset pen (0x0D / 0x0E: the builder
// leaves such messages plain) and a row end inside a substitution are refused.
const unsigned char* WalkText(const unsigned char* at, int y, int count, int& drawn, unsigned char& color,
                              TextRow& row, short& last_x) {
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
            case 0x0E:
                Refuse(at, "(the offset pen) in an annotated message");
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
            if (drawn < count) {
                const auto clut = static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color);
                g.draw_char(pen_x, y, clut, 1, shown);
                pen_x = static_cast<short>(MsgBox_PenX + 12);
                last_x = pen_x;
            } else {
                // Measured, not drawn: the pen MsgBox_DrawChar would leave
                // (DIV-0006's advance; the hang's draw leaves it, the + 12
                // brings it back to the line start).
                pen_x = static_cast<short>(pen_x + (hung ? 12 : TextAdvance_Of(shown)));
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
int DrawInsert(const TextRow& row, int k, int x, int y, int count, unsigned char color, const unsigned char* marker) {
    const bool shown = row.ins_first[k] <= count;
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
void DrawRuby(const unsigned char* at, const unsigned char* end, int y, int count, unsigned char color,
              const TextRow& row) {
    const int line = MsgBox_LineX;
    int x = 0, seg = 0, seg_x0 = 0, reveal = 0;
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
            if (!in_run) reveal = Reveal(row, seg, (x - seg_x0) / kHalfCell);
            in_run = true;
            int num, den;
            Scale(p, num, den);
            const int q = 12 * num / den;
            if (reveal <= count) {
                const unsigned index = Index(p);
                std::uint32_t pa, pb;
                if (TextPair_Of(index, &pa, &pb)) Refuse(p, "is a pair code (DIV-0057), in a ruby row");
                Emit(line + x, y + kRubyDy, index, p, static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color), q);
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
    for (int pair = 0;; ++pair) {
        unsigned char ruby_color = color;
        const unsigned char* const end = RowEnd(at, &ruby_color);
        if (end[0] != 0x01) break;   // an empty page, or the message's end
        if (pair == kPairs) Refuse(at, "starts a third row pair on a page");
        const auto text_y = static_cast<std::uint16_t>(origin + Rows().row_y[2 * pair + 1]);
        const auto ruby_y = static_cast<int>(static_cast<std::uint16_t>(origin + Rows().row_y[2 * pair]));
        unsigned char text_color = ruby_color;
        const int before = drawn;
        const unsigned char* const stop = WalkText(end + 1, text_y, count, drawn, text_color, row, last_x);
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

// original 0x4976D0 (PSX 0x8015034C). Opens message `id` of the script pool:
// the pool header's u16 at 2*id is the message's offset from the pool base.
//
// As the original has it: the id is taken as a WORD and sign-extended for the
// header index (movsx eax, si), so ids 0x8000..0xFFFF read below the pool -
// no bounds check either way. The message index is stored AFTER MsgBox_Reset,
// which reads the pointer this just wrote.
//
// The reading row (docs/reading-row.md 3): when the area's side chunk has a
// composed message for the slot, both pointers take it instead (a slot of
// the sign-extended index below 0 has none). No side chunk: the original.
extern "C" void __cdecl Msg_OpenScript(unsigned short id) {
    const int index = static_cast<short>(id);
    const std::uint32_t off = W(kScriptPool + static_cast<std::uint32_t>(2 * index));
    const unsigned char* at = At(kScriptPool) + off;
    if (index >= 0) at = ruby::Open(0, 0, static_cast<unsigned>(index), at);
    SetPtr(kBase, at);
    SetPtr(kAt, at);
    g.reset();
    SetW(kMessage, id);
}

// original 0x497770 (PSX 0x8015042C). Starts the box on the message the
// pointer at 0x7DEE50 names: the state bytes cleared, the flag word cut to
// its two kept bits, a leading 0x0C eaten into the placement byte, and
// window 0 claimed.
//
// As the original has it: the flag word keeps only bit 6 (0x40); a 0x0C
// advances the read pointer twice and both stores happen (the pointer is
// written once between them, which nothing can observe); with no 0x0C the
// placement byte is zeroed; Window_Alloc is called (0, 0) and its answer
// discarded; window 0's kind byte 0x80316F is set to 2 after it.
extern "C" void __cdecl MsgBox_Reset(void) {
    const unsigned char* at = Ptr(kAt);
    SetW(kFlags, W(kFlags) & 0x40u);
    SetB(kState, 0);
    ItemUse_SubState = 0;
    Msgbox_EffectKind = 0;
    Msgbox_EffectPhase = 0;
    Msgbox_OffsetX = 0;
    Msgbox_OffsetY = 0;
    Msgbox_EffectTimer = 0;
    if (at[0] == 0x0C) {
        ++at;
        SetPtr(kAt, at);
        const unsigned char placement = at[0];
        ++at;
        Msgbox_Placement = placement;
        SetPtr(kAt, at);
    } else {
        Msgbox_Placement = 0;
    }
    Msgbox_Delay = 0;
    Msgbox_StepCount = 0;
    SetW(kEffectOff, 0);
    ItemUse_ColorHigh = 0;
    g.window_alloc(0, 0);
    WindowRecords[0].pass = 2;
}

// original 0x4977F0 (PSX 0x80150508). The box's frame: the box origin from
// window 0's position, the state machine, the effect, and - unless flag 4
// holds it - one step of the printer. Returns "the box is not finished".
//
// As the original has it: the origin is (window x >> 4) + 10 and
// (window y >> 4) + 6, the shift arithmetic on the signed 16-bit word and the
// add on the full register, of which only the low word is stored; the flag
// byte is read again for the return value, after the three calls.
//
// The return is a BYTE: bit 1 of the flag word, inverted. The original leaves
// the rest of eax holding the box origin x it computed, which no caller can
// depend on - 0x595A80 tests al, and the two tail jumps at 0x595855 and
// 0x595A6E hand it to their own callers (docs/msgbox.md section 2).
extern "C" unsigned char __cdecl MsgBox_FrameTask(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    const int x = static_cast<short>(win0->x) >> 4;
    const int y = static_cast<short>(win0->y) >> 4;
    MsgBox_LineX = static_cast<short>(x + 10);
    Msgbox_OriginY = static_cast<unsigned>(y + 6);
    g.dispatch();
    g.effect_task();
    if ((B(kFlags) & 4) == 0) g.step();
    return static_cast<unsigned char>((static_cast<unsigned char>(~B(kFlags)) >> 1) & 1);
}

// original 0x497840 (PSX 0x80150598), 0x497840..0x497A6C with its 23-entry
// jump table at 0x497A70. One frame of the printer: it re-reads the message
// from the top every frame and walks it until it has drawn as many characters
// as 0x7DEE59 says, so the box fills in a character at a time.
//
//   0x00      end: outside a substitution it is skipped, so the walk runs on
//   0x01      newline: pen y += 14, pen x = the box's line start
//   0x03      the current member's name, 8 characters (index 0x802DC9
//             through 0x66972C, record 0x903A70 + n * 164)
//   0x04 nn   member nn's name, the same 8 characters
//   0x05 nn   take colour nn
//   0x06      colour 0
//   0x07 nn   the 32-byte record Text_Records + nn * 32, 32 characters
//   0x08 nn   message nn of the sub-pool at 0x803580 + [0x803584], 16
//             characters, starting ONE byte past where its header points
//   0x0A 0x0C 0x0F 0x14   two bytes, nothing drawn
//   0x0D      the offset pen on; 0x0E off
//   0x02 0x09 0x0B 0x10 0x11 0x16   one byte, nothing drawn
//   0x12 0x13 0x15 and every byte above 0x16   a glyph
//
// As the original has it:
//   - the pen is reset to the box origin on entry and the whole message
//     re-walked, so a colour or an offset set by a control is re-applied
//     every frame;
//   - a substitution is one deep: the countdown is decremented BEFORE the
//     character is read, so a count of 9 draws eight characters, and the
//     return position is the saved pointer plus one;
//   - the loop ends only when the drawn count EQUALS 0x7DEE59, never when it
//     passes it, and 0x7DEE59 of 0 returns before the first read;
//   - a first byte 0x2A or 0x3C exactly at the line start hangs 12 px into
//     the margin, whatever else it is (the PSX does the same for its own
//     quote at 8 px, SLUS-00422 0x80150680);
//   - under the offset pen (0x0D) the x comes from MsgBox_PenX in memory and
//     the y from the register copy, and both take the signed byte offsets
//     0x7DEE5A / 0x7DEE5B; without it both come from the registers and take
//     no offset;
//   - the colour handed on is (0x7DEE58 << 4) + the remembered byte, a byte;
//   - a second text byte is skipped when the FIRST has bit 7 set - read back
//     from memory after the draw, not from the register;
//   - the pen is reloaded from memory after every draw and then advanced.
//     DIV-0006 hangs on exactly that: g.draw_char is MsgBox_DrawChar, which
//     moves MsgBox_PenX by (advance - 12) so this `+ 12` lands it right.
//
// DIV-0140: at a button mention the `05 cc` takes the drawn button's colour
// and the icon is handed to the draw as the drawn button's glyph (its advance
// then moves the pen, DIV-0006's way); the walk steps over the text's own
// bytes, so the drawn count and every later byte are as before.
//
// The reading row (docs/reading-row.md 3): a message whose base lies in a
// side chunk is walked as row pairs by ruby::Step; no other message reaches
// it, and with no side chunk loaded the test is false before it reads
// anything.
extern "C" void __cdecl MsgBox_Step(void) {
    if (ruby::Annotated(Ptr(kBase))) {
        ruby::Step();
        return;
    }
    unsigned char flags = 0;                 // bl: bit 0 in a substitution, bit 5 the offset pen
    const unsigned char* at = Ptr(kBase);
    const unsigned char* resume = nullptr;
    unsigned char sub_count = 0;
    unsigned char drawn = 0;
    unsigned char color = 0;
    // DIV-0140 (src/game/button_text.h): 1 after a button code that resolved
    // (its `05 cc` next), 2 once the drawn colour is taken (the glyph next).
    int button = 0;
    button_text::Icon icon;
    short pen_x = MsgBox_LineX;
    auto pen_y = Msgbox_OriginY;
    MsgBox_PenX = pen_x;
    Msgbox_PenY = pen_y;
    if (Msgbox_StepCount == 0) return;
    for (;;) {
        if (flags & 1) {
            sub_count = static_cast<unsigned char>(sub_count - 1);
            if (sub_count == 0) {
                at = resume + 1;
                flags = static_cast<unsigned char>(flags & 0xFEu);
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
                }
                break;
            case 0x01:
                pen_x = MsgBox_LineX;
                pen_y = static_cast<std::uint16_t>(pen_y + 0x0E);
                MsgBox_PenX = pen_x;
                Msgbox_PenY = pen_y;
                break;
            case 0x03: {
                flags = static_cast<unsigned char>(flags | 1u);
                resume = at;
                const unsigned member = B(kNameMap + ObjTrio[0].char_id);
                sub_count = 9;
                at = At(kCharRecords + 164u * member);
                break;
            }
            case 0x04: {
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned member = at[0];
                sub_count = 9;
                at = At(kCharRecords + 164u * member);
                break;
            }
            case 0x05: {
                const unsigned char* const control = at;
                ++at;
                color = at[0];
                // DIV-0140: the code's own colour stands (06 clears it at
                // once); the `05 cc` after it, or a colour cue, takes the
                // drawn button's colour instead.
                if (button == 1 || (button == 0 && ButtonText_Cue(control, icon))) {
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
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned record = at[0];
                sub_count = 0x21;
                at = At(kTextRecords + (record << 5));
                break;
            }
            case 0x08: {
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned entry = at[0];
                const unsigned char* const pool = At(kScriptPool + L(kPoolSelector));
                sub_count = 0x11;
                std::uint16_t off;
                std::memcpy(&off, pool + 2 * entry, sizeof off);
                at = pool + off;
                break;
            }
            case 0x0A:
            case 0x0C:
            case 0x0F:
            case 0x14:
                ++at;
                break;
            case 0x0D:
                flags = static_cast<unsigned char>(flags | 0x20u);
                break;
            case 0x0E:
                flags = static_cast<unsigned char>(flags & 0xDFu);
                break;
            case 0x12:
            case 0x13:
            case 0x15:
                glyph = true;
                break;
            default:   // 0x02 0x09 0x0B 0x10 0x11 0x16: one byte, nothing drawn
                break;
            }
        }
        if (glyph) {
            // DIV-0140: the drawn button's glyph in the fallback's place; the
            // walk still steps over the fallback's own bytes below.
            const unsigned char* const shown = button == 2 ? icon.glyph : at;
            button = 0;
            if ((c == 0x2A || c == 0x3C) &&
                static_cast<std::uint16_t>(pen_x) == static_cast<std::uint16_t>(MsgBox_LineX)) {
                pen_x = static_cast<short>(pen_x - 12);
                MsgBox_PenX = pen_x;
            }
            if (flags & 0x20) {
                if (B(kFlags) & 8) {
                    // The original pushes the colour local as a DWORD, whose
                    // upper three bytes are stack it never wrote; 0x4987E0
                    // reads `mov al, [esp+4]` and masks to 0xF, so only this
                    // byte can reach it.
                    g.effect_draw(color, shown);
                } else {
                    const auto clut = static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color);
                    const int x = static_cast<short>(MsgBox_PenX + Msgbox_OffsetX);
                    const int y = static_cast<short>(pen_y + Msgbox_OffsetY);
                    g.draw_char(x, y, clut, 1, shown);
                }
            } else {
                const auto clut = static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color);
                g.draw_char(pen_x, pen_y, clut, 1, shown);
            }
            if (at[0] & 0x80) ++at;
            drawn = static_cast<unsigned char>(drawn + 1);
            pen_x = MsgBox_PenX;
            pen_y = Msgbox_PenY;
            pen_x = static_cast<short>(pen_x + 12);
            MsgBox_PenX = pen_x;
        }
        ++at;
        if (Msgbox_StepCount == drawn) return;
    }
}

// original 0x497AD0 (PSX 0x801508EC, which copies the same eight pointers
// from 0x80149A5C onto its own stack). One frame of the box's state machine.
extern "C" void __cdecl MsgBox_StateDispatch(void) {
    Call(g.state, 8, B(kState), "MsgBox_StateDispatch");
}

// original 0x497B30 (PSX 0x8015096C), 0x497B30..0x497E30 with its own
// 23-entry jump table at 0x497E34 - a second table inside what pe_funcs.py
// calls one 256-byte function. State 0: it consumes the message's control
// codes ahead of the printer and decides how long the next character waits.
//
// It loops while its "keep going" flag is set; almost every code clears it,
// so one frame normally consumes one code. A glyph sets the per-character
// delay from the speed table and leaves for state 0 or 1; instant print
// (flag 0x10) makes the tail set the flag again, so the whole span between
// the 0x10 and the 0x11 that toggles it back is consumed in one frame.
//
// As the original has it:
//   - a substitution's countdown is decremented at the TOP, before the code
//     is read, and the return position is the saved pointer plus one;
//   - codes 0x05 and 0x0C step over one argument byte and keep going;
//   - code 0x08 does NOT clear the flag - it starts a sub-pool substitution
//     and the loop runs on into it;
//   - code 0x0A adds 0x200 to its argument for Sound_PlayEffect and reloads
//     both pointers from memory after the call;
//   - code 0x0B adds 8 to the delay rather than setting it;
//   - code 0x0F reads its argument back from memory three times, once for
//     each field of the 4-byte effect record;
//   - code 0x14's second byte is (rows | column << 4): the low nibble minus
//     one is the last row, and a zero high nibble picks state 5, anything
//     else state 4;
//   - the two tests against 0x100 at 0x497B80 / 0x497B86 are dead - the code
//     is a byte - so the flag-word bit 8 they would set is never set here;
//   - a glyph with bit 7 set steps over its second byte; under flag 0x10 it
//     only counts up 0x7DEE59; the speed comes from 0x658E94 (bit 5 of
//     Input_Held held) or 0x658E95, indexed by the signed speed byte, and
//     bit 8 of the flag word overrides it with 3 and forces state 1.
//
// The reading row (docs/reading-row.md 3): outside a substitution, a ruby row
// of an annotated message is stepped over whole (ruby::SkipRow), so the count
// 0x7DEE59 is the text's glyphs and a reading adds no delay.
extern "C" void __cdecl MsgBox_StatePrint(void) {
    const unsigned char* at = Ptr(kAt);
    const unsigned char* resume = Ptr(kResume);
    bool again = true;
    do {
        if (B(kFlags) & 1) {
            const auto left = static_cast<unsigned char>(Msgbox_SubCount - 1);
            Msgbox_SubCount = left;
            if (left == 0) {
                SetW(kFlags, W(kFlags) & 0xFFFEu);
                at = resume + 1;
                SetPtr(kAt, at);
            }
        } else if (const unsigned char* const text = ruby::SkipRow(Ptr(kBase), at); text != at) {
            // The reading row (docs/reading-row.md 3): a ruby row of an
            // annotated message is stepped over whole, costing nothing.
            at = text;
            SetPtr(kAt, at);
        }
        const unsigned char c = at[0];
        bool glyph = c > 0x16;
        if (!glyph) {
            switch (c) {
            case 0x00:
                if (B(kFlags) & 1) {
                    SetW(kFlags, W(kFlags) & 0xFFFEu);
                    at = resume;
                    break;
                }
                again = false;
                SetB(kState, 2);
                break;
            case 0x02:
                again = false;
                SetB(kState, 3);
                break;
            case 0x03: {
                SetB(kFlags, B(kFlags) | 1u);
                resume = at;
                again = false;
                SetPtr(kResume, resume);
                const unsigned member = B(kNameMap + ObjTrio[0].char_id);
                Msgbox_SubCount = 9;
                at = At(kCharRecords + 164u * member);
                break;
            }
            case 0x04: {
                SetB(kFlags, B(kFlags) | 1u);
                again = false;
                ++at;
                resume = at;
                SetPtr(kAt, at);
                SetPtr(kResume, resume);
                const unsigned member = at[0];
                Msgbox_SubCount = 9;
                at = At(kCharRecords + 164u * member);
                break;
            }
            case 0x05:
            case 0x0C:
                ++at;
                break;
            case 0x07: {
                SetB(kFlags, B(kFlags) | 1u);
                again = false;
                ++at;
                resume = at;
                SetPtr(kAt, at);
                SetPtr(kResume, resume);
                const unsigned record = at[0];
                Msgbox_SubCount = 0x21;
                at = At(kTextRecords + (record << 5));
                break;
            }
            case 0x08: {
                SetB(kFlags, B(kFlags) | 1u);
                ++at;
                resume = at;
                SetPtr(kAt, at);
                SetPtr(kResume, resume);
                const unsigned entry = at[0];
                const unsigned char* const pool = At(kScriptPool + L(kPoolSelector));
                SetPtr(kAt, pool);
                std::uint16_t off;
                std::memcpy(&off, pool + 2 * entry, sizeof off);
                Msgbox_SubCount = 0x11;
                at = pool + off;
                break;
            }
            case 0x0A: {
                ++at;
                SetPtr(kAt, at);
                const auto id = static_cast<std::uint16_t>(at[0] + 0x200);
                g.sound(id);
                at = Ptr(kAt);
                resume = Ptr(kResume);
                break;
            }
            case 0x0B: {
                const auto delay = static_cast<unsigned char>(Msgbox_Delay + 8);
                SetB(kState, 1);
                again = false;
                Msgbox_Delay = delay;
                break;
            }
            case 0x0F: {
                ++at;
                SetPtr(kAt, at);
                Msgbox_EffectPhase = 0;
                ItemUse_SubState = 0;
                Msgbox_EffectKind = B(kEffectTable + 4u * at[0]);
                Msgbox_EffectTimer = W(kEffectTable + 2 + 4u * at[0]);
                SetW(kEffectOff, static_cast<unsigned>(static_cast<signed char>(B(kEffectTable + 1 + 4u * at[0]))));
                break;
            }
            case 0x10:
            case 0x11:
                SetW(kFlags, W(kFlags) ^ 0x10u);
                break;
            case 0x14: {
                again = false;
                ++at;
                SetPtr(kAt, at);
                ItemUse_ChoiceId = at[0];
                at += 2;
                SetPtr(kAt, at);
                MenuWindows_ListCount = static_cast<unsigned char>((at[0] & 0x0F) - 1);
                const auto column = static_cast<unsigned char>(at[0] >> 4);
                MenuWindows_ListSet = column;
                SetB(kState, column == 0 ? 5 : 4);
                break;
            }
            case 0x16: {
                ++at;
                again = false;
                SetPtr(kAt, at);
                SetB(kState, 7);
                Msgbox_Delay = at[0];
                break;
            }
            case 0x12:
            case 0x13:
            case 0x15:
                // The table's entries 0x12, 0x13 and 0x15 are the glyph
                // handler 0x497DAA, exactly as MsgBox_Step's are.
                glyph = true;
                break;
            default:   // 0x01 0x06 0x09 0x0D 0x0E: one byte, nothing else
                break;
            }
        }
        if (glyph) {
            if (c & 0x80) ++at;
            if ((B(kFlags) & 0x10) == 0) {
                // Holding the Speak line's button speeds the text: Japan's
                // circle asked through sigma, not word 0, which also holds
                // L2 / R2 (docs/button-tests.md 3, DIV-0139).
                const unsigned speed =
                    (static_cast<unsigned char>(Input_Held) & bof3x::input::sigma::Sigma(0x20)) ? 0 : 1;
                const auto row = ModeFlow_ConfigBytes;
                Msgbox_Delay = B(static_cast<std::uint32_t>(kSpeedTable + speed + row));
                if (L(kFlags) & 0x100u) {
                    Msgbox_Delay = 3;
                    SetB(kState, 1);
                } else {
                    SetB(kState, 0);
                    if (Msgbox_Delay != 0) SetB(kState, 1);
                }
                again = false;
            }
            Msgbox_StepCount = Msgbox_StepCount + 1;
        }
        if (B(kFlags) & 0x10) again = true;
        ++at;
        SetPtr(kAt, at);
    } while (again);
}

// original 0x497E90. State 1: count the per-character delay down; at zero
// back to state 0 for the next character. As the original has it, the byte is
// decremented first, so a delay of 0 wraps to 0xFF and waits 255 frames more.
extern "C" void __cdecl MsgBox_StateDelay(void) {
    const auto left = static_cast<unsigned char>(Msgbox_Delay - 1);
    Msgbox_Delay = left;
    if (left == 0) SetB(kState, 0);
}

// original 0x497EB0. State 2, the wait at the end of a message: two entries
// on 0x7DEE41, built on the stack.
extern "C" void __cdecl MsgBox_State2(void) { Call(g.sub2, 2, ItemUse_SubState, "MsgBox_State2"); }

// original 0x497EE0. Any button pressed - or either kept flag 0x40 / 0x80
// still set - closes window 0 and holds the printer (flag 4).
extern "C" void __cdecl MsgBox_State2Press(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    if (Input_Pressed == 0 && (B(kFlags) & 0xC0) == 0) return;
    rv::Store<unsigned>(win0->state, win0->state + 1);
    const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
    SetB(kFlags, B(kFlags) | 4u);
    ItemUse_SubState = next;
}

// original 0x497F20, shared by state 2 and state 7: once window 0 reports 4
// the box is done (flag 2), which is what MsgBox_FrameTask returns.
extern "C" void __cdecl MsgBox_State2Closed(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    if (win0->state == 4) SetB(kFlags, B(kFlags) | 2u);
}

// original 0x497F40. State 3, a page break: two entries on 0x7DEE41.
extern "C" void __cdecl MsgBox_State3(void) { Call(g.sub3, 2, ItemUse_SubState, "MsgBox_State3"); }

// original 0x497F70, shared by state 3 and state 4: draws the "there is more"
// arrow under the box every frame (0x498D20 shows it only while bit 5 of the
// frame counter is set - it blinks), and moves on once a button is pressed or
// the kept flag 0x40 is set.
//
// As the original has it: the colour nibble 0x7DEE58 is zeroed on the way
// out; the arrow's x is (window 0's row byte >> 1) + the box line start - 14
// and its y the effect offset plus the pen y plus 12; both reach 0x498D20 as
// 16 bits (movsx word at 0x498D7C), so the stale halves the original's
// registers carry there cannot be observed.
extern "C" void __cdecl MsgBox_State3Arrow(void) {
    if (Input_Pressed != 0 || (B(kFlags) & 0x40) != 0) {
        const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
        ItemUse_ColorHigh = 0;
        ItemUse_SubState = next;
    }
    const int y = static_cast<std::uint16_t>(W(kEffectOff) + Msgbox_PenY) + 0x0C;
    const int x = (Window0_ParamA >> 1) + MsgBox_LineX - 0x0E;
    g.page_arrow(x, y);
}

// original 0x497FD0, shared by state 3 and state 5. Scrolls the box one row
// every frame the delay expires: seven rows, then either the choice list is
// re-opened (states 4 and 5) or the printer restarts on what is left of the
// message, with the hold and the effect-draw flags cleared.
//
// As the original has it: the delay byte is tested BEFORE it is decremented,
// so a delay of 0 does the work and leaves 0xFF behind; the row counter is
// stored whatever it reached; the restart takes the message base from the
// read position, so the rest of the message becomes the whole message.
extern "C" void __cdecl MsgBox_State3Step(void) {
    const unsigned char was = Msgbox_Delay;
    Msgbox_Delay = static_cast<unsigned char>(was - 1);
    if (was != 0) return;
    const auto row = static_cast<unsigned char>(ItemUse_ColorHigh + 1);
    Msgbox_Delay = 1;
    ItemUse_ColorHigh = row;
    if (row != 7) return;
    if (B(kState) == 4 || B(kState) == 5) {
        g.reopen();
        return;
    }
    const std::uint32_t at = MenuWindows_ListText;
    SetW(kFlags, W(kFlags) & 0xFFF3u);
    ItemUse_SubState = 0;
    SetB(kState, 1);
    Msgbox_Delay = 8;
    Msgbox_Base = at;
    SetW(kEffectOff, 0);
    Msgbox_StepCount = 0;
    ItemUse_ColorHigh = 0;
}

// original 0x498050. State 4, a choice list in its own window: eight entries
// on 0x7DEE41, the first of them state 3's arrow.
extern "C" void __cdecl MsgBox_State4(void) { Call(g.sub4, 8, ItemUse_SubState, "MsgBox_State4"); }

// original 0x4980B0. Claims window 1 as kind 1 (a choice list), tells window
// 0 to go to state 3, and holds the printer.
extern "C" void __cdecl MsgBox_ChoiceOpen(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    AreaW0c_ChoiceAnswer = 0;
    g.window_alloc(1, 0);
    const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
    SetB(kFlags, B(kFlags) | 4u);
    WindowRecords[1].kind = 1;
    rv::Store<unsigned>(win0->state, 3);
    ItemUse_SubState = next;
}

// original 0x4980F0. Waits until window 1 is open and window 0 has finished
// its own move.
extern "C" void __cdecl MsgBox_ChoiceWaitOpen(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    if (WindowRecords[1].state == 2 && win0->state == 4) ItemUse_SubState = ItemUse_SubState + 1;
}

// original 0x498110. The choice list's input: cancel closes it, confirm
// jumps two entries on, and up / down move the cursor through the auto-repeat
// at 0x461EB0 - which is handed the whole pad word here (the menu's copy at
// 0x498350 masks it to 0x5000 first).
//
// As the original has it: the cursor wraps at both ends, the bottom test is
// SIGNED against the last row, and the top one on the sign of the decremented
// byte - so a cursor of 0x80 stays where it is going down.
extern "C" void __cdecl MsgBox_ChoiceInput(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    const unsigned pressed = Input_Pressed;
    if ((Field_CancelButtons & pressed) != 0) {
        const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
        WindowRecords[1].state = 3;
        rv::Store<unsigned>(win0->state, 1);
        ItemUse_SubState = next;
        return;
    }
    if ((Field_ConfirmButtons & pressed) != 0) {
        ItemUse_SubState = ItemUse_SubState + 2;
        return;
    }
    const unsigned repeat = g.repeat(pressed);
    const unsigned char last = static_cast<unsigned char>(MenuWindows_ListCount);
    if ((repeat >> 8) & 0x10) {
        const auto up = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) - 1);
        AreaW0c_ChoiceAnswer = up;
        if (static_cast<signed char>(up) < 0) AreaW0c_ChoiceAnswer = last;
    }
    if ((repeat >> 8) & 0x40) {
        const auto down = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) + 1);
        AreaW0c_ChoiceAnswer = down;
        if (static_cast<signed char>(down) > static_cast<signed char>(last)) AreaW0c_ChoiceAnswer = 0;
    }
}

// original 0x4981A0. Once window 0 reports 2 the hold flag is toggled off and
// the state's own index goes back to 0.
extern "C" void __cdecl MsgBox_ChoiceWaitShut(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    if (win0->state != 2) return;
    SetW(kFlags, W(kFlags) ^ 4u);
    ItemUse_SubState = 0;
}

// original 0x498230. Re-opens the message at the answer's own text and
// releases the hold.
extern "C" void __cdecl MsgBox_ChoiceReopen(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    if (win0->state != 2) return;
    Msgbox_Delay = 0x10;
    g.reopen();
    SetW(kFlags, W(kFlags) ^ 4u);
}

// original 0x498250. Once window 1 reports 4 the box is finished: flag 0x80
// set (which MsgBox_Reset keeps only 0x40 of, so it is cleared on the next
// message), window 0 to state 4, and state 2 sub 1 - the wait for it to shut.
extern "C" void __cdecl MsgBox_ChoiceDone(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    if (WindowRecords[1].state != 4) return;
    SetB(kFlags, B(kFlags) | 0x80u);
    rv::Store<unsigned>(win0->state, 4);
    SetB(kState, 2);
    ItemUse_SubState = 1;
}

// original 0x498280. Re-opens the message the box is on, from the top: the
// same lookup as Msg_OpenScript but with the id taken UNSIGNED, and without
// MsgBox_Reset - only the delay, the state and the two counters are set.
//
// The reading row (docs/reading-row.md 3): the slot's composed message when
// the area's side chunk has one, as Msg_OpenScript.
extern "C" void __cdecl MsgBox_Reopen(void) {
    const unsigned id = W(kMessage);
    Msgbox_Delay = 8;
    SetB(kState, 1);
    const std::uint32_t off = W(kScriptPool + 2u * id);
    const unsigned char* const at = ruby::Open(0, 0, id, At(kScriptPool) + off);
    SetPtr(kBase, at);
    SetPtr(kAt, at);
    Msgbox_StepCount = 0;
    ItemUse_SubState = 0;
}

// original 0x4982C0. State 5, the same list drawn as a menu (code 0x14 with a
// zero high nibble): six entries on 0x7DEE41, the last of them state 3's
// scroll.
extern "C" void __cdecl MsgBox_State5(void) { Call(g.sub5, 6, ItemUse_SubState, "MsgBox_State5"); }

// original 0x498310. Claims window 1 as kind 2 (a menu). Unlike the choice
// list's opener it does not touch window 0 or the hold flag.
extern "C" void __cdecl MsgBox_MenuOpen(void) {
    AreaW0c_ChoiceAnswer = 0;
    g.window_alloc(1, 0);
    const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
    WindowRecords[1].kind = 2;
    ItemUse_SubState = next;
}

// original 0x498340. Waits until window 1 reports 2.
extern "C" void __cdecl MsgBox_MenuWaitOpen(void) {
    if (WindowRecords[1].state == 2) ItemUse_SubState = ItemUse_SubState + 1;
}

// original 0x498350. The menu's input: confirm moves on, and the cursor moves
// on the auto-repeat of the pad masked to 0x5000 - up and down only, so a
// left or right press cannot restart the repeat timer here.
extern "C" void __cdecl MsgBox_MenuInput(void) {
    const unsigned pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) ItemUse_SubState = ItemUse_SubState + 1;
    const unsigned repeat = g.repeat(pressed & 0x5000u);
    const unsigned char last = static_cast<unsigned char>(MenuWindows_ListCount);
    if ((repeat >> 8) & 0x10) {
        const auto up = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) - 1);
        AreaW0c_ChoiceAnswer = up;
        if (static_cast<signed char>(up) < 0) AreaW0c_ChoiceAnswer = last;
    }
    if ((repeat >> 8) & 0x40) {
        const auto down = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) + 1);
        AreaW0c_ChoiceAnswer = down;
        if (static_cast<signed char>(down) > static_cast<signed char>(last)) AreaW0c_ChoiceAnswer = 0;
    }
}

// original 0x498410. Once window 1 reports 4: a message id of 0xFFFF means
// there is nothing left to say and the box is finished; otherwise on to the
// next entry.
extern "C" void __cdecl MsgBox_MenuDone(void) {
    if (WindowRecords[1].state != 4) return;
    if (W(kMessage) != 0xFFFF) {
        ItemUse_SubState = ItemUse_SubState + 1;
        return;
    }
    SetB(kFlags, B(kFlags) | 0x80u);
    SetB(kState, 2);
    ItemUse_SubState = 0;
}

// original 0x498450. State 6: back to state 0 once window 0 reports 2. No
// path in this file reaches it - nothing writes 6 to 0x7DEE40 (section 2).
extern "C" void __cdecl MsgBox_State6(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    if (win0->state == 2) SetB(kState, 0);
}

// original 0x498470. State 7, the wait code 0x16 asks for: two entries on
// 0x7DEE41, the second of them state 2's "window closed" test.
extern "C" void __cdecl MsgBox_State7(void) { Call(g.sub7, 2, ItemUse_SubState, "MsgBox_State7"); }

// original 0x4984A0. Counts code 0x16's argument down; at zero window 0 is
// told to move on, the printer is held, and the state's index advances.
//
// As the original has it: the delay is tested BEFORE the decrement, so a 0
// does the work and leaves 0xFF.
extern "C" void __cdecl MsgBox_State7Delay(void) {
    auto* const win0 = rv::As<records::WindowRecord>(At(WindowRecords_addr) + 0 * sizeof(records::WindowRecord));
    const unsigned char was = Msgbox_Delay;
    Msgbox_Delay = static_cast<unsigned char>(was - 1);
    if (was != 0) return;
    const auto window = static_cast<unsigned char>(win0->state + 1);
    const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
    SetB(kFlags, B(kFlags) | 4u);
    rv::Store<unsigned>(win0->state, window);
    ItemUse_SubState = next;
}

// original 0x4984E0 (PSX 0x80151A1C). The box's effect, one frame: six
// entries on 0x7DEE42, which code 0x0F sets from the record table 0x658E98.
// Kind 0 is a bare `ret` at 0x437CC0 - the only entry the attract sequence
// ever reaches - and kinds 2 and 3 share one handler.
extern "C" void __cdecl MsgBox_EffectTask(void) {
    Call(g.effect, 6, Msgbox_EffectKind, "MsgBox_EffectTask");
}

// original 0x498520. Effect 1: the pen x offset rocks between -2 and +2, two
// units a frame, until the effect's own timer runs out.
//
// As the original has it: the timer is only decremented when it is not
// 0xFFFF (which means forever), and the kind is cleared when the timer READS
// zero - including the 0xFFFF case's zero, which it cannot be.
extern "C" void __cdecl MsgBox_EffectShake(void) {
    Call(g.shake, 2, Msgbox_EffectPhase, "MsgBox_EffectShake");
    std::uint16_t left = Msgbox_EffectTimer;
    if (left != 0xFFFF) {
        left = static_cast<std::uint16_t>(left - 1);
        Msgbox_EffectTimer = left;
    }
    if (left == 0) Msgbox_EffectKind = 0;
}

// original 0x498560 / 0x498580: out to -2, then back to +2, then out again.
extern "C" void __cdecl MsgBox_ShakeOut(void) {
    if (static_cast<unsigned char>(Msgbox_OffsetX) == 0xFE) {
        Msgbox_EffectPhase = Msgbox_EffectPhase + 1;
        return;
    }
    Msgbox_OffsetX = static_cast<unsigned char>(Msgbox_OffsetX) + 0xFE;
}
extern "C" void __cdecl MsgBox_ShakeBack(void) {
    if (static_cast<unsigned char>(Msgbox_OffsetX) == 2) {
        Msgbox_EffectPhase = Msgbox_EffectPhase - 1;
        return;
    }
    Msgbox_OffsetX = static_cast<unsigned char>(Msgbox_OffsetX) + 2;
}

// original 0x4985A0, effects 2 and 3: the effect draw's own offset 0x7DEE68
// walks to its limit and back. Two entries on 0x7DEE43, then the timer down
// unless it is 0xFFFF.
extern "C" void __cdecl MsgBox_EffectGrow(void) {
    Call(g.grow, 2, Msgbox_EffectPhase, "MsgBox_EffectGrow");
    if (Msgbox_EffectTimer != 0xFFFF) Msgbox_EffectTimer = Msgbox_EffectTimer - 1;
}

// original 0x4985E0. Turns the effect draw on (flag 8) and, once the timer
// has reached zero, sets it to 1 and moves to the second phase.
extern "C" void __cdecl MsgBox_GrowStart(void) {
    SetB(kFlags, B(kFlags) | 8u);
    if (Msgbox_EffectTimer != 0) return;
    const auto next = static_cast<unsigned char>(Msgbox_EffectPhase + 1);
    Msgbox_EffectTimer = 1;
    Msgbox_EffectPhase = next;
}

// original 0x498610. Kind 2 walks the offset down by 2 and stops when it goes
// negative; every other kind walks it up by 2 and stops when it goes
// positive. Stopping clears flag 8, the offset and the kind. Either way the
// timer is put back to 1, so this runs every frame.
extern "C" void __cdecl MsgBox_GrowStep(void) {
    if (Msgbox_EffectTimer != 0) return;
    bool stop;
    if (Msgbox_EffectKind == 2) {
        const auto v = static_cast<std::int16_t>(W(kEffectOff) - 2);
        SetW(kEffectOff, static_cast<std::uint16_t>(v));
        stop = v < 0;
    } else {
        const auto v = static_cast<std::int16_t>(W(kEffectOff) + 2);
        SetW(kEffectOff, static_cast<std::uint16_t>(v));
        stop = v > 0;
    }
    if (stop) {
        SetW(kFlags, W(kFlags) & 0xFFF7u);
        SetW(kEffectOff, 0);
        Msgbox_EffectKind = 0;
    }
    Msgbox_EffectTimer = 1;
}

// original 0x498670. Effect 4: the pen x offset wanders between -3 and +3 one
// unit at a time, and each time it comes back through zero the y offset takes
// a step of 0x7DEE68's low byte; a y of 0xF6 turns the step around (offset 1,
// wait 15) and a y of exactly 0 counts one repetition off the timer and, at
// the last one, stops the effect.
//
// As the original has it: the x offset is stored BEFORE the wait counter is
// tested, so a frame that only counts the wait down still moves it; the wait
// is 3 between steps and 15 at each turn; a timer of 0xFFFF never ends.
extern "C" void __cdecl MsgBox_EffectWander(void) {
    unsigned char x;
    if (Msgbox_EffectPhase == 0) {
        if (static_cast<unsigned char>(Msgbox_OffsetX) == 0xFD) {
            Msgbox_EffectPhase = 1;
            return;
        }
        x = static_cast<unsigned char>(static_cast<unsigned char>(Msgbox_OffsetX) - 1);
    } else {
        if (static_cast<unsigned char>(Msgbox_OffsetX) == 3) {
            Msgbox_EffectPhase = Msgbox_EffectPhase - 1;
            return;
        }
        x = static_cast<unsigned char>(static_cast<unsigned char>(Msgbox_OffsetX) + 1);
    }
    const bool waiting = Msgbox_EffectWait != 0;
    Msgbox_OffsetX = x;
    if (waiting) {
        Msgbox_EffectWait = Msgbox_EffectWait - 1;
        return;
    }
    const auto y = static_cast<unsigned char>(static_cast<unsigned char>(Msgbox_OffsetY) + static_cast<unsigned char>(W(kEffectOff)));
    Msgbox_OffsetY = y;
    if (y == 0xF6) {
        Msgbox_EffectWait = 0x0F;
        SetW(kEffectOff, 1);
        return;
    }
    if (y != 0) {
        Msgbox_EffectWait = 3;
        return;
    }
    const std::uint16_t left = Msgbox_EffectTimer;
    Msgbox_EffectWait = 0x0F;
    SetW(kEffectOff, 0xFFFF);
    if (left == 0xFFFF) return;
    Msgbox_EffectTimer = static_cast<std::uint16_t>(left - 1);
    if (left != 0) return;
    Msgbox_EffectKind = 0;
    Msgbox_OffsetY = 0;
    Msgbox_OffsetX = 0;
}

// original 0x498740. Effect 5: two entries on 0x7DEE43 - a set-up, then a
// per-frame integration of 0x7DEE68 into the y offset.
extern "C" void __cdecl MsgBox_EffectRise(void) { Call(g.rise, 2, Msgbox_EffectPhase, "MsgBox_EffectRise"); }

// original 0x498770. The record's frame count becomes the accumulator's
// starting value and its offset is scaled by 16 - an 8.8 fixed-point speed.
extern "C" void __cdecl MsgBox_RiseStart(void) {
    const std::uint16_t frames = Msgbox_EffectTimer;
    SetW(kEffectOff, W(kEffectOff) << 4);
    Msgbox_EffectWait = frames;
    Msgbox_EffectPhase = Msgbox_EffectPhase + 1;
}

// original 0x4987A0. Adds the speed to the accumulator every frame and the
// accumulator's HIGH byte - before the add - to the y offset; once the y
// offset is no longer negative it and the kind are cleared, which ends the
// effect. As the original has it, the high byte is taken with `sar edx, 8`
// on a register whose upper half is stale, which cannot reach dl.
extern "C" void __cdecl MsgBox_RiseStep(void) {
    const std::uint16_t was = Msgbox_EffectWait;
    const auto y = static_cast<unsigned char>(static_cast<unsigned char>(Msgbox_OffsetY) + static_cast<unsigned char>(was >> 8));
    Msgbox_EffectWait = static_cast<std::uint16_t>(was + W(kEffectOff));
    Msgbox_OffsetY = y;
    if (static_cast<signed char>(y) < 0) return;
    Msgbox_OffsetY = 0;
    Msgbox_EffectKind = 0;
}

// original 0x4987E0 (PSX 0x80151F4C), 0x4987E0..0x498A2A. MsgBox_Step's draw
// for a character inside an offset-pen span (0x0D .. 0x0E) while flag 8 of
// 0x7DEE44 is set - effects 2 and 3, the grow and the shrink. One
// POLY_FT4-shaped record at Gfx_PacketNext, scaled: the quad is 12 + P wide
// and (about) 12 + P tall, P the signed word 0x7DEE68 that MsgBox_GrowStep
// walks, out of the same 12-unit glyph cell; then the pen is moved by P, and
// the stepper's own + 12 makes the advance the drawn width.
//
// As the original has it:
//   - the CLUT word +0x0E is Gpu_GetClut((colour & 0xF) << 4, 0x1E0), only
//     the low nibble of the pushed colour taken;
//   - the texture extent W is 12 while P <= 12 (a signed compare), 11 above;
//   - the glyph word +0x16 is ((c & 0x7F) << 8) + the next byte for a lead
//     byte with bit 7 set, else c - 0x26 as a WORD (so 0x12..0x25 wrap);
//   - the texture bytes are (0, 0), (W, 0), (0, W - T), (W, W - T), T the
//     clip byte 0x658F00[0x7DEE58] - the row the page scroll is on;
//   - the corners are (x, y), (x + P + 12, y), (x, y'), (x + P + 12, y') with
//     y' = y + P + 12 - (T * (((P + 12) << 4) / 12) >> 4), the division
//     signed and truncating (the 0x2AAAAAAB multiply), each stored as a word;
//   - shade 0x80, code 0x6C, semi-transparency 1, Gfx_CommitPrim(1, 0x28);
//   - a SPACE (0x20, `cmp cl, 0x20 / je 0x4988BE` at 0x498819) skips the
//     glyph word and all eight texture bytes but still writes the shade and
//     the corners and commits the primitive - whatever glyph word and
//     texture the packet slot last held are drawn in the gap. The PSX twin
//     does the same for its word separator 0xFF (0x80152010 -> 0x80152BF8:
//     no tpage, no u, v, the corners and the commit), so the slip is the
//     PlayStation's, carried over with 0x20 standing for 0xFF.
//
// DIVERGENCE DIV-0070: a space commits nothing. The clut word is written and
// the pen moves by P exactly as the original's do; the quad is not built and
// not committed. The stale glyph word was what crashed Font_UnpackGlyph on
// 2026-10-02 (area 0x63, message 0x24, docs/msgbox.md section 4).
//
// Not DIV-0006's: the advance is 12 + P for every character, the space too;
// the advance table is not consulted (docs/msgbox.md section 4).
extern "C" void __cdecl MsgBox_EffectDraw(unsigned color, const unsigned char* text) {
    const auto put = [](unsigned char* at, unsigned value) {
        const auto word = static_cast<std::uint16_t>(value);
        std::memcpy(at, &word, sizeof word);
    };
    const unsigned clut = g.get_clut(static_cast<int>((color & 0xFu) << 4), 0x1E0);
    put(Gfx_PacketNext + 0x0E, clut);
    const unsigned char extent = static_cast<std::int16_t>(W(kEffectOff)) <= 0x0C ? 0x0C : 0x0B;
    const unsigned char c = text[0];
    if (c != 0x20) {
        const auto glyph = static_cast<std::uint16_t>((c & 0x80) ? ((c & 0x7Fu) << 8) + text[1] : c - 0x26u);
        unsigned char* const p = Gfx_PacketNext;
        put(p + 0x16, glyph);
        p[0x0C] = 0;
        p[0x0D] = 0;
        p[0x14] = extent;
        p[0x15] = 0;
        p[0x1C] = 0;
        p[0x1D] = static_cast<unsigned char>(extent - B(kClipTable + ItemUse_ColorHigh));
        p[0x24] = extent;
        p[0x25] = static_cast<unsigned char>(extent - B(kClipTable + ItemUse_ColorHigh));
    } else if (g_effect_space_skips) {
        MsgBox_PenX = static_cast<short>(MsgBox_PenX + W(kEffectOff));   // DIV-0070
        return;
    }
    unsigned char* const p = Gfx_PacketNext;
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    const auto x = static_cast<std::uint16_t>(MsgBox_PenX);
    const std::uint16_t y = Msgbox_PenY;
    const auto off = static_cast<std::int16_t>(W(kEffectOff));
    const auto right = static_cast<unsigned>(x + static_cast<std::uint16_t>(off) + 0x0C);
    const int scale = ((off + 0x0C) << 4) / 0x0C;
    const int cut = static_cast<int>(B(kClipTable + ItemUse_ColorHigh)) * scale >> 4;
    const auto bottom = static_cast<unsigned>(y - static_cast<unsigned>(cut) + static_cast<std::uint16_t>(off) + 0x0C);
    put(p + 0x08, x);
    put(p + 0x0A, y);
    put(p + 0x10, right);
    put(p + 0x12, y);
    put(p + 0x18, x);
    put(p + 0x1A, bottom);
    put(p + 0x20, right);
    put(p + 0x22, bottom);
    g.set_code6c(Gfx_PacketNext);
    g.set_semitrans(Gfx_PacketNext, 1);
    g.commit(1, 0x28);
    MsgBox_PenX = static_cast<short>(MsgBox_PenX + W(kEffectOff));
}

// original 0x59E2D0 (PSX 0x80159874). Claims window record `slot` (stride
// 0x24 off 0x803160) for `kind` if its first byte is free.
//
// As the original has it: the slot is masked to a byte for the index but the
// ANSWER is the argument dword - unchanged on success, with its low byte
// forced to 0xFF on failure (`or al, 0xFF`), so a caller that passes a slot
// with high bytes set gets them back. No bound on the slot.
extern "C" unsigned __cdecl Window_Alloc(unsigned slot, unsigned kind) {
    unsigned char* const rec = At(WindowRecords_addr + (slot & 0xFF) * 0x24u);
    if (rec[0] != 0) return slot | 0xFFu;
    rec[0] = 1;
    rec[1] = static_cast<unsigned char>(kind);
    rec[2] = 0;
    rec[3] = 0;
    return slot;
}

// original 0x516B30. The port's general text draw, 341 callers: it plants the
// pen and hands the string to Text_DrawString.
//
// As the original has it: x is taken as a WORD and stored to both the pen and
// the line start; y is incremented as a dword and stored as a word, so a y of
// 0xFFFF draws at 0; the colour and the count go on unchanged.
extern "C" const unsigned char* __cdecl Text_DrawAt(int x, int y, int color, int count,
                                                    const unsigned char* text) {
    Text_PenX = static_cast<short>(x);
    Text_LineX = static_cast<short>(x);
    Text_PenY = static_cast<short>(y + 1);
    return g.draw_string(static_cast<unsigned>(color), static_cast<unsigned>(count), text);
}

// original 0x516D50. One glyph: fills the POLY_FT4-shaped record at
// Gfx_PacketNext and hands it on. Its only caller is Text_DrawString, as
// (pen x, pen y, 12, 12 - v, u, v, clut).
//
// As the original has it: the texture window is a flat 12 units wide (0 and
// 12 in every u) and `u` is the row - what DIV-0010 named, in the config
// screen's words, "the texture extent fixed at 0xC whatever the quad"; the
// three shade bytes are 0x80; the v coordinates are u and (u - v + 12) as
// BYTES, so they wrap; the corners are (x, y + u), (x + w, y + u),
// (x, y + h + u), (x + w, y + h + u), each stored as a word; the primitive
// pointer is re-read from Gfx_PacketNext before every single store, which
// nothing between them can change; +0x16 (the glyph index) is the caller's.
//
// This is faithful - no divergence. DIV-0010 is the config screen's own
// stand-in for Text_DrawAt (src/game/config_text.cpp), not a change here.
extern "C" void __cdecl Text_EmitGlyph(int x, int y, int w, int h, int u, int v, int clut) {
    unsigned char* const p = Gfx_PacketNext;
    const auto row = static_cast<unsigned char>(u);
    const auto far_row = static_cast<unsigned char>(u - v + 12);
    const auto put = [](unsigned char* at, unsigned value) {
        const auto word = static_cast<std::uint16_t>(value);
        std::memcpy(at, &word, sizeof word);
    };
    put(p + 0x0E, static_cast<unsigned>(clut));
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    p[0x0C] = 0;
    p[0x0D] = row;
    p[0x14] = 0x0C;
    p[0x15] = row;
    p[0x1C] = 0;
    p[0x1D] = far_row;
    p[0x24] = 0x0C;
    p[0x25] = far_row;
    put(p + 0x08, static_cast<unsigned>(x));
    put(p + 0x0A, static_cast<unsigned>(y + u));
    put(p + 0x10, static_cast<unsigned>(x + w));
    put(p + 0x12, static_cast<unsigned>(y + u));
    put(p + 0x18, static_cast<unsigned>(x));
    put(p + 0x1A, static_cast<unsigned>(y + h + u));
    put(p + 0x20, static_cast<unsigned>(x + w));
    put(p + 0x22, static_cast<unsigned>(y + h + u));
    g.set_code6c(p);
    g.set_semitrans(p, 1);
    g.commit(1, 0x28);
}

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
                         static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_effect_space_skips)), &was, &is, 1);
        bof3::Log("DIV-0070    a space in a growing or shrinking shout commits no primitive");
    }
    BOF3_INJECT(Msg_OpenScript);
    BOF3_INJECT(MsgBox_Reset);
    BOF3_INJECT(MsgBox_FrameTask);
    BOF3_INJECT(MsgBox_Step);
    BOF3_INJECT(MsgBox_StateDispatch);
    BOF3_INJECT(MsgBox_StatePrint);
    BOF3_INJECT(MsgBox_StateDelay);
    BOF3_INJECT(MsgBox_State2);
    BOF3_INJECT(MsgBox_State2Press);
    BOF3_INJECT(MsgBox_State2Closed);
    BOF3_INJECT(MsgBox_State3);
    BOF3_INJECT(MsgBox_State3Arrow);
    BOF3_INJECT(MsgBox_State3Step);
    BOF3_INJECT(MsgBox_State4);
    BOF3_INJECT(MsgBox_ChoiceOpen);
    BOF3_INJECT(MsgBox_ChoiceWaitOpen);
    BOF3_INJECT(MsgBox_ChoiceInput);
    BOF3_INJECT(MsgBox_ChoiceWaitShut);
    BOF3_INJECT(MsgBox_ChoiceReopen);
    BOF3_INJECT(MsgBox_ChoiceDone);
    BOF3_INJECT(MsgBox_Reopen);
    BOF3_INJECT(MsgBox_State5);
    BOF3_INJECT(MsgBox_MenuOpen);
    BOF3_INJECT(MsgBox_MenuWaitOpen);
    BOF3_INJECT(MsgBox_MenuInput);
    BOF3_INJECT(MsgBox_MenuDone);
    BOF3_INJECT(MsgBox_State6);
    BOF3_INJECT(MsgBox_State7);
    BOF3_INJECT(MsgBox_State7Delay);
    BOF3_INJECT(MsgBox_EffectTask);
    BOF3_INJECT(MsgBox_EffectShake);
    BOF3_INJECT(MsgBox_ShakeOut);
    BOF3_INJECT(MsgBox_ShakeBack);
    BOF3_INJECT(MsgBox_EffectGrow);
    BOF3_INJECT(MsgBox_GrowStart);
    BOF3_INJECT(MsgBox_GrowStep);
    BOF3_INJECT(MsgBox_EffectWander);
    BOF3_INJECT(MsgBox_EffectRise);
    BOF3_INJECT(MsgBox_RiseStart);
    BOF3_INJECT(MsgBox_RiseStep);
    BOF3_INJECT(MsgBox_EffectDraw);
    BOF3_INJECT(Window_Alloc);
    BOF3_INJECT(Text_DrawAt);
    BOF3_INJECT(Text_EmitGlyph);
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

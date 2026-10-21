// DIVERGENCE (DIV-0138, docs/button-shoulder-corner.md): the item
// lists' L1 / R1 corner drawn as σ has it. See shoulder_glyphs.h.
//
// Ours entirely: the label geometry below was measured on the UI page of the
// PC (59 modules), the European PSX disc and the PSP-EU disc
// (docs/button-shoulder-corner.md 2); no pixel of Capcom's is in this file -
// the art is drawn from the page in VRAM, the rectangles read from
// Menu_PieceRect's table 0x663C8C, the stand-in codes from 0x66A2FC.
#include "game/shoulder_glyphs.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "common/cache_manifest.h"
#include "game/button_text.h"
#include "game/lang_layout.h"
#include "game/text_draw.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace shoulder_glyphs {
namespace {

// Each labelled piece: the label box's first row and row count (columns
// kBoxCol .. kBoxCol + kBoxW - 1 of the piece), a row of the piece that is
// plain background across those columns (the box's refill), and where the
// slot's glyph starts, relative to the piece's y. The L1 slot is cut across
// two pieces: the bar's top piece holds its first five rows (19..23) and the
// cap 0x16 under it (at y + 0x18 in all ten list draws) its last two (0..1),
// so 0x16 places the glyph 5 rows above its own y.
struct Label { unsigned id; int box_row, box_rows, plain_row, glyph_dy; std::uint16_t slot; };
constexpr Label kLabels[] = {
    {0x15, 19, 5, 18, 19, kL1},   // the bar's top, with the next-category arrow
    {0x2E, 19, 5, 18, 19, kL1},   // the same, the arrow lit (piece list 0x6634A8)
    {0x33, 19, 5, 18, 19, kL1},   // the bar's top, no arrow (Menu_DrawItemList's key items, ...)
    {0x16, 0, 2, 2, -5, kL1},     // the cap under it: the L1 label's last two rows
    {0x18, 8, 8, 7, 8, kR1},      // the bar's bottom: the R1 label
};
constexpr int kBoxCol = 1, kBoxW = 8;
constexpr int kPieceW = 16;
// The L1 art: piece 0x15's box (5 rows) over piece 0x16's (2 rows); the R1
// art: piece 0x18's box (8 rows).
constexpr int kL1Top = 5, kL1Low = 2, kR1Rows = 8;

const Label* Find(unsigned id) {
    for (const Label& l : kLabels)
        if (l.id == (id & 0xFF)) return &l;
    return nullptr;
}

void Push(Plan& p, Step::Kind k, int x, int y, int u, int v, int w, int h, std::uint16_t button = 0) {
    if (p.n >= static_cast<int>(sizeof p.s / sizeof p.s[0])) bof3::Fatal("ShoulderGlyphs: a plan over %d steps", p.n);
    Step& s = p.s[p.n++];
    s.kind = k;
    s.x = x;
    s.y = y;
    s.u = static_cast<unsigned char>(u);
    s.v = static_cast<unsigned char>(v);
    s.w = static_cast<unsigned char>(w);
    s.h = static_cast<unsigned char>(h);
    s.button = button;
}

// DIVERGENCE DIV-0138: 1 after every module's self-test (PatchBytes "ShoulderGlyphs").
unsigned char g_on = 0;
bool g_page_art = true;

}  // namespace

std::uint16_t SlotOf(unsigned id) {
    const Label* const l = Find(id);
    return l ? l->slot : 0;
}

Plan Make(unsigned id, int x, int y, const Rect& own, const Rect& r15, const Rect& r16, const Rect& r18,
          std::uint16_t shown, bool page_art, bool force) {
    Plan p;
    const Label* const l = Find(id);
    if (!l || (!force && page_art && shown == l->slot)) {
        p.original = true;
        return p;
    }
    const int u = own.u, v = own.v, w = own.w, h = own.h;
    const int end = l->box_row + l->box_rows;
    // The piece, its label box left out.
    if (l->box_row > 0) Push(p, Step::kSprite, x, y, u, v, w, l->box_row);
    Push(p, Step::kSprite, x, y + l->box_row, u, v + l->box_row, kBoxCol, l->box_rows);
    Push(p, Step::kSprite, x + kBoxCol + kBoxW, y + l->box_row, u + kBoxCol + kBoxW, v + l->box_row,
         w - kBoxCol - kBoxW, l->box_rows);
    // The box refilled, a row at a time, from the piece's own plain row.
    for (int r = l->box_row; r < end; ++r)
        Push(p, Step::kSprite, x + kBoxCol, y + r, u + kBoxCol, v + l->plain_row, kBoxW, 1);
    if (end < h) Push(p, Step::kSprite, x, y + end, u, v + end, w, h - end);
    // The glyph of σ(slot), whole, at the slot - drawn by both pieces of the
    // L1 slot, so whichever the list draws last leaves it complete.
    const int gx = x + kBoxCol, gy = y + l->glyph_dy;
    if (page_art && shown == kL1) {
        Push(p, Step::kSprite, gx, gy, r15.u + kBoxCol, r15.v + 19, kBoxW, kL1Top);
        Push(p, Step::kSprite, gx, gy + kL1Top, r16.u + kBoxCol, r16.v, kBoxW, kL1Low);
    } else if (page_art && shown == kR1) {
        Push(p, Step::kSprite, gx, gy, r18.u + kBoxCol, r18.v + 8, kBoxW, kR1Rows);
    } else {
        Push(p, Step::kGlyph, gx, gy, 0, 0, 0, 0, shown);
        Push(p, Step::kPage, 0, 0, 0, 0, 0, 0);
    }
    return p;
}

// The Config panel's own glyph for a button (Config_DrawControllerCell, the
// original 0x461C00 and DIV-0051's): the six in the panel's order, circle,
// cross, triangle, square, L1, R1, each in its colour (2, 1, 6, 5, 0, 0,
// read off 0x461C00's pushes). Under a Latin layer the glyph is DIV-0051's
// icon, which loc_build.py puts at ICONS_AT (0xA65 on): the same numbers as
// config_text.cpp's kIconCircle..kIconR1. Under `ja-JP` (`ja`) it is the
// Japan disc's own, from button_text.h's numbers: the face buttons the
// single-byte sheet's 0x15 08..0B (circle, cross, triangle, square - the
// owner's look at the sheet, 2026-10-21), and L1 / R1 the disc's small
// labels loc_build.py puts at JA_SHOULDERS_AT, as the Latin layers' are (the
// owner 2026-10-21, over D321's capital L and R; drawn only on a PSP base,
// the others have the page art). No Japanese layer writes 0x66A2FC's
// strings, so a remapped slot there would find them empty. Otherwise the
// port's stand-in, the one-byte string 0x66A2FC[k] (circled numerals and an
// X in the Chinese font).
Code GlyphCode(std::uint16_t button, bool latin, bool ja) {
    static const std::uint16_t kOrder[6] = {0x20, 0x40, 0x10, 0x80, kL1, kR1};
    static const int kColour[6] = {2, 1, 6, 5, 0, 0};
    constexpr std::uint32_t kIconFirst = 0xA65;           // loc_build.py ICONS_AT; config_text.cpp kIconCircle
    constexpr std::uint32_t kStandIns = 0x66A2FC;         // the original's eight one-byte glyph strings
    for (int k = 0; k < 6; ++k) {
        if (kOrder[k] != button) continue;
        Code c{};
        c.colour = kColour[k];
        if (latin) {
            const std::uint32_t g = kIconFirst + static_cast<std::uint32_t>(k);
            c.text[0] = static_cast<unsigned char>(0x80 | (g >> 8));
            c.text[1] = static_cast<unsigned char>(g & 0xFF);
        } else if (ja) {
            const unsigned g = k < 4 ? button_text::kJapaneseIcons + static_cast<unsigned>(k)
                                     : button_text::kJapaneseShoulders + static_cast<unsigned>(k - 4);
            c.text[0] = static_cast<unsigned char>(0x80 | (g >> 8));
            c.text[1] = static_cast<unsigned char>(g & 0xFF);
        } else {
            std::uint32_t at;
            std::memcpy(&at, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(kStandIns + 4u * k)), 4);
            const unsigned char* const s = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(at));
            if (s[0] == 0 || s[0] >= 0x80 || s[1] != 0)
                bof3::Fatal("ShoulderGlyphs: 0x66A2FC[%d] is not a one-byte glyph string", k);
            c.text[0] = s[0];
        }
        return c;
    }
    bof3::Fatal("ShoulderGlyphs: no glyph for button 0x%04X (not one of the six)", button);
}

Code DrawCode(std::uint16_t button) {
    const bool latin = Lang_Latin();
    const bool ja = !latin && Lang_Japanese();
    const Code c = GlyphCode(button, latin, ja);
    if (ja) {
        const unsigned g = (static_cast<unsigned>(c.text[0] & 0x7F) << 8) | c.text[1];
        const unsigned glyphs = TextDraw_GlyphCount();
        if (g >= glyphs)
            bof3::Fatal("ShoulderGlyphs: the ja-JP glyph is 0x%X, the font loaded has %u glyphs (no ja-JP layer)",
                        g, glyphs);
    }
    return c;
}

bool On() { return g_on != 0; }
bool PageArt() { return g_page_art; }

// ===========================================================================
// The self-test (BOF3X_SHADOW=shoulder_glyphs): the plans rastered over a
// page of our own (random nibbles, with the two properties measured on every
// shipped UI page: the plain rows are one index across the box columns, and
// the three top pieces' L1 boxes are equal), for every σ(slot) of the six
// buttons on both slots, both page kinds, the three top pieces and both draw
// orders of top piece and cap; and the stand-in Configs through σ's builder.

namespace {

unsigned g_bad = 0;
void Check(bool ok, const char* what, int a = 0, int b = 0, int c = 0) {
    if (ok) return;
    if (g_bad < 20) bof3::Log("shadow      shoulder_glyphs MISMATCH: %s (%d, %d, %d)", what, a, b, c);
    ++g_bad;
}

constexpr int kCanvasW = 32, kCanvasH = 64;
constexpr int kStamp = 0x100;   // a font glyph's 8 x 8, as the canvas marks it: kStamp + button
unsigned char g_page[256][256];
int g_canvas[kCanvasH][kCanvasW];
int g_glyphs, g_pages;

std::uint32_t g_rng = 0x3C6EF372u;
std::uint32_t Rnd() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }

void Blit(int x, int y, int u, int v, int w, int h) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (y + j >= 0 && y + j < kCanvasH && x + i >= 0 && x + i < kCanvasW)
                g_canvas[y + j][x + i] = g_page[v + j][u + i];
}

void Run(const Plan& p) {
    for (int i = 0; i < p.n; ++i) {
        const Step& s = p.s[i];
        if (s.kind == Step::kSprite) {
            Blit(s.x, s.y, s.u, s.v, s.w, s.h);
        } else if (s.kind == Step::kGlyph) {
            ++g_glyphs;
            for (int j = 0; j < 8; ++j)
                for (int k = 0; k < 8; ++k) g_canvas[s.y + j][s.x + k] = kStamp + s.button;
            Check(i + 1 < p.n && p.s[i + 1].kind == Step::kPage, "a font glyph not followed by the page", s.button);
        } else {
            ++g_pages;
        }
    }
}

struct Pieces { Rect top, cap, bottom, r15; };

// The original's draw of one piece.
void Original(const Rect& r, int x, int y) { Blit(x, y, r.u, r.v, r.w, r.h); }

// The column (x, y) of the canvas the given layout's slot box or glyph covers.
// Layout: the top piece at (0, 0), the cap at (0, 24), the bottom at (0, 40).
constexpr int kTopY = 0, kCapY = 24, kBottomY = 40;

int Expect(const int ref[kCanvasH][kCanvasW], int x, int y, std::uint16_t up, std::uint16_t down, bool art,
           const Pieces& pc) {
    // the L1 slot: box rows 19..23 of the top and 0..1 of the cap; the R1
    // slot rows 8..15 of the bottom; columns 1..8
    const bool in_cols = x >= kBoxCol && x < kBoxCol + kBoxW;
    auto glyph_at = [&](std::uint16_t b, int gy, int& out) {
        if (!in_cols) return false;
        const int j = y - gy;
        if (art && b == kL1) {
            if (j >= 0 && j < kL1Top) { out = g_page[pc.r15.v + 19 + j][pc.r15.u + x]; return true; }
            if (j >= kL1Top && j < kL1Top + kL1Low) { out = g_page[pc.cap.v + j - kL1Top][pc.cap.u + x]; return true; }
            return false;
        }
        if (art && b == kR1) {
            if (j >= 0 && j < kR1Rows) { out = g_page[pc.bottom.v + 8 + j][pc.bottom.u + x]; return true; }
            return false;
        }
        if (j >= 0 && j < 8) { out = kStamp + b; return true; }
        return false;
    };
    int g;
    const bool up_moved = !(art && up == kL1), down_moved = !(art && down == kR1);
    if (up_moved && glyph_at(up, kTopY + 19, g)) return g;
    if (down_moved && glyph_at(down, kBottomY + 8, g)) return g;
    if (up_moved && in_cols && ((y >= kTopY + 19 && y < kTopY + 24) || (y >= kCapY && y < kCapY + 2)))
        return g_page[y < kCapY ? pc.top.v + 18 : pc.cap.v + 2][(y < kCapY ? pc.top.u : pc.cap.u) + x];
    if (down_moved && in_cols && y >= kBottomY + 8 && y < kBottomY + 16) return g_page[pc.bottom.v + 7][pc.bottom.u + x];
    return ref[y][x];
}

void SelfTest() {
    g_bad = 0;
    // A page of our own: random nibbles, then the two measured properties.
    for (auto& row : g_page)
        for (auto& t : row) t = static_cast<unsigned char>(1 + Rnd() % 15);
    const Rect r15{144, 128, 16, 24}, r2e{216, 144, 16, 24}, r33{144, 168, 16, 24}, r16{160, 128, 16, 16},
        r18{176, 128, 16, 24};
    for (const Label& l : kLabels) {
        const Rect& r = l.id == 0x15 ? r15 : l.id == 0x2E ? r2e : l.id == 0x33 ? r33 : l.id == 0x16 ? r16 : r18;
        for (int i = 0; i < kBoxW; ++i) g_page[r.v + l.plain_row][r.u + kBoxCol + i] = 4;
    }
    for (const Rect* r : {&r2e, &r33})
        for (int j = 19; j < 24; ++j)
            for (int i = 0; i < kBoxW; ++i) g_page[r->v + j][r->u + kBoxCol + i] = g_page[r15.v + j][r15.u + kBoxCol + i];

    static const std::uint16_t kSix[6] = {0x20, 0x40, 0x10, 0x80, kL1, kR1};
    static int ref[kCanvasH][kCanvasW];
    unsigned cases = 0, originals = 0;
    for (const Rect* top : {&r15, &r2e, &r33}) {
        const unsigned top_id = top == &r15 ? 0x15 : top == &r2e ? 0x2E : 0x33;
        const Pieces pc{*top, r16, r18, r15};
        for (int x = 0; x < kCanvasW; ++x)
            for (int y = 0; y < kCanvasH; ++y) ref[y][x] = -1;
        std::memcpy(g_canvas, ref, sizeof g_canvas);
        Original(*top, 0, kTopY);
        Original(r16, 0, kCapY);
        Original(r18, 0, kBottomY);
        std::memcpy(ref, g_canvas, sizeof ref);
        for (bool art : {true, false})
            for (bool cap_first : {false, true})
                for (bool force : {false, true})
                    for (std::uint16_t up : kSix)
                        for (std::uint16_t down : kSix) {
                            if (up == down) continue;   // σ is a permutation
                            ++cases;
                            for (int y = 0; y < kCanvasH; ++y)
                                for (int x = 0; x < kCanvasW; ++x) g_canvas[y][x] = -1;
                            g_glyphs = g_pages = 0;
                            const Plan pt = Make(top_id, 0, kTopY, *top, r15, r16, r18, up, art, force);
                            const Plan pp = Make(0x16, 0, kCapY, r16, r15, r16, r18, up, art, force);
                            const Plan pb = Make(0x18, 0, kBottomY, r18, r15, r16, r18, down, art, force);
                            auto draw = [&](const Plan& p, const Rect& r, int y) {
                                if (p.original) { ++originals; Original(r, 0, y); } else Run(p);
                            };
                            if (cap_first) { draw(pp, r16, kCapY); draw(pt, *top, kTopY); }
                            else { draw(pt, *top, kTopY); draw(pp, r16, kCapY); }
                            draw(pb, r18, kBottomY);
                            Check(pt.original == (!force && art && up == kL1), "the top piece's shortcut", top_id, up);
                            Check(pb.original == (!force && art && down == kR1), "the bottom piece's shortcut", down);
                            int font = 0;
                            if (!pt.original && !(art && (up == kL1 || up == kR1))) font += 2;
                            if (!pb.original && !(art && (down == kL1 || down == kR1))) font += 1;
                            Check(g_glyphs == font && g_pages == font, "font glyphs drawn", g_glyphs, font);
                            for (int y = 0; y < kCanvasH; ++y)
                                for (int x = 0; x < kCanvasW; ++x) {
                                    const int want = Expect(ref, x, y, up, down, art, pc);
                                    if (g_canvas[y][x] != want) {
                                        Check(false, "a pixel", x, y, (top_id << 16) | (up << 8) | down);
                                        y = kCanvasH;
                                        break;
                                    }
                                }
                        }
    }
    // The geometry the plans assume, against the live rectangle table.
    unsigned rects = 0;
    for (const Label& l : kLabels) {
        const unsigned char* r = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(0x663C8Cu + 4u * l.id));
        Check(r[2] == kPieceW && r[3] == (l.id == 0x16 ? 16 : 24), "a labelled piece's size", static_cast<int>(l.id), r[2], r[3]);
        ++rects;
    }
    // The glyph codes, over stand-ins of our own planted in 0x66A2FC's six
    // entries (the shipped pointers put back after): Latin DIV-0051's six,
    // in order, two bytes; the stand-ins one byte each, as planted; and
    // the ja-JP branch, with all six entries planted empty as a ja-JP image
    // leaves them: the face buttons the Japan sheet's 0x15 08..0B (JA_AT +
    // 0x63..0x66), L1 / R1 the disc's small labels (JA_SHOULDERS_AT, + 1).
    // A Latin layer wins over the flag.
    static const std::uint16_t kOrder[6] = {0x20, 0x40, 0x10, 0x80, kL1, kR1};
    static const int kColour[6] = {2, 1, 6, 5, 0, 0};
    static const unsigned char kPlanted[6][2] = {{'b', 0}, {'c', 0}, {'d', 0}, {'e', 0}, {'f', 0}, {'g', 0}};
    static const unsigned char kEmpty[2] = {0, 0};
    auto entry = [](int k) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x66A2FCu + 4u * k)); };
    auto plant = [&](int k, const unsigned char* s) {
        const auto at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(s));
        std::memcpy(entry(k), &at, 4);
    };
    std::uint32_t shipped[6];
    for (int k = 0; k < 6; ++k) std::memcpy(&shipped[k], entry(k), 4);
    for (int k = 0; k < 6; ++k) plant(k, kPlanted[k]);
    unsigned codes = 0;
    for (int k = 0; k < 6; ++k) {
        const Code a = GlyphCode(kOrder[k], true, false), b = GlyphCode(kOrder[k], false, false);
        const Code aj = GlyphCode(kOrder[k], true, true);
        const unsigned g = (static_cast<unsigned>(a.text[0] & 0x7F) << 8) | a.text[1];
        Check(g == 0xA65u + static_cast<unsigned>(k) && a.text[2] == 0, "a Latin icon code", k, static_cast<int>(g));
        Check(std::memcmp(aj.text, a.text, sizeof a.text) == 0, "a Latin icon under the ja-JP flag", k);
        Check(b.text[0] == kPlanted[k][0] && b.text[1] == 0, "a stand-in code", k, b.text[0]);
        Check(a.colour == kColour[k] && b.colour == kColour[k], "a glyph colour", k, a.colour, b.colour);
        codes += 3;
    }
    for (int k = 0; k < 6; ++k) plant(k, kEmpty);
    for (int k = 0; k < 6; ++k) {
        const Code j = GlyphCode(kOrder[k], false, true);
        Check(j.colour == kColour[k], "a ja-JP glyph colour", k, j.colour);
        const unsigned g = (static_cast<unsigned>(j.text[0] & 0x7F) << 8) | j.text[1];
        // 0x993 + 0x5B + 0x08..0x0B; 0x993 + 2 * 441, + 1
        static const unsigned kWant[6] = {0x9F6u, 0x9F7u, 0x9F8u, 0x9F9u, 0xD05u, 0xD06u};
        Check((j.text[0] & 0x80) != 0 && g == kWant[k] && j.text[2] == 0, "a ja-JP corner glyph", k,
              static_cast<int>(g), static_cast<int>(kWant[k]));
        ++codes;
    }
    for (int k = 0; k < 6; ++k) std::memcpy(entry(k), &shipped[k], 4);
    // Stand-in Configs through σ's own builder: Japan's defaults (the
    // identity), Speak <-> Action, View <-> Change, and Change on circle.
    namespace sg = bof3x::input::sigma;
    struct Swap { int a, b; std::uint16_t l1, r1; const char* what; };
    static const Swap kSwaps[] = {
        {-1, -1, kL1, kR1, "identity"},
        {0, 6, kL1, kR1, "Speak <-> Action"},
        {3, 4, kR1, kL1, "View <-> Change"},
        {0, 4, 0x20, kR1, "Speak <-> Change"},
    };
    for (const Swap& s : kSwaps) {
        std::uint16_t w[9];
        sg::DefaultWords(sg::Release::kJapan, true, w);
        if (s.a >= 0) {
            const std::uint16_t t = w[s.a];
            w[s.a] = w[s.b];
            w[s.b] = t;
        }
        std::string why;
        const sg::Table t = sg::Build(w, why);
        Check(t.valid, s.what);
        if (!t.valid) continue;
        Check(sg::Apply(t, kL1) == s.l1 && sg::Apply(t, kR1) == s.r1, s.what, sg::Apply(t, kL1), sg::Apply(t, kR1));
    }
    bof3::Log("shadow      shoulder_glyphs self-test: %u plans rastered (%u drawn as the original), %u rectangles, "
              "%u glyph codes (6 under ja-JP, D321), %u stand-in Configs; %u MISMATCHES",
              cases, originals, rects, codes, static_cast<unsigned>(sizeof kSwaps / sizeof kSwaps[0]), g_bad);
    if (g_bad) bof3::Fatal("shoulder_glyphs self-test: %u mismatches (see the log)", g_bad);
}

}  // namespace

void Arm() {
    if (bof3::WantsShadow("shoulder_glyphs")) SelfTest();
    // A PSP base (cache_manifest.h: base_policy "psp", the PSP-only build)
    // brings the PSP's UI page, whose labels are `L` / `R`: no L1 / R1 art
    // on the page, so the corner's L1 / R1 are the Config panel's glyphs too.
    std::string how = "no cache named: the install's own page";
    wchar_t root[1024];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_CACHE", root, 1024);
    if (n > 0 && n < 1024) {
        std::wstring path(root, n);
        while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
        const bof3x::CacheManifest m = bof3x::ManifestRead(path);
        if (m.present && !m.error.empty()) bof3::Fatal("ShoulderGlyphs: the cache's manifest: %s", m.error.c_str());
        g_page_art = !bof3x::ManifestPspBase(m);
        how = !m.present ? "the cache has no manifest: the PC's page"
              : g_page_art ? "a PC or PSX base: L1 / R1 art"
              : !Lang_Latin() && Lang_Japanese()
                  ? "a PSP base under ja-JP: L / R, the Japan sheet's glyphs, L1 / R1 the disc's small labels (D321)"
                  : "a PSP base: L / R, the Config panel's glyphs";
    }
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("ShoulderGlyphs", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_on)), &was, &is, 1);
    bof3::Log("ShoulderGlyphs the item lists' L1 / R1 corner as sigma has it: %s (%s)",
              g_on ? "on" : "OFF (BOF3X_ORIGINAL=ShoulderGlyphs)", how.c_str());
}

}  // namespace shoulder_glyphs

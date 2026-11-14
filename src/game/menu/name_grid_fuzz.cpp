// BOF3X_SHADOW=name_grid: the name entry's routine (name_grid.cpp) against two
// synthetic grids built here - one of the Japan family's shape (three pages,
// five codes, a page with holes) and one of the West's (one page numbered 2,
// four codes, an empty row and a hole beside a special) - never a disc's: the
// cells' codes and bytes are invented, so the test does not depend on
// whether a layer is loaded, and touches no game state (the draws go to a
// recorder, the sounds to a log). Checked: the chunk's parse and each of its
// rules, every branch of the input step (the hand's four moves with their
// skips and wraps, Left / Right from a special, the clamp "up to the first
// cell that is not empty" after a move and after a page change, each special
// code of both families with its limits, the page key, the name cursor's
// keys, backspace, the finish under both copies' rules, the byte-room refusal,
// the 4-cell cap), the key map of both families, the encode / decode pair
// with a pair code, the grid's and the legend's draws; then a random walk
// over both grids and both copies that asserts the hand is never on an empty
// cell. docs/new-game-round.md (group NG).
#include "game/menu/name_grid.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "hook/log.h"

namespace name_grid {
namespace {

using namespace detail;

// --- the checks' ledger ---------------------------------------------------------

unsigned g_checks = 0, g_bad = 0;

void Check(bool ok, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void Check(bool ok, const char* fmt, ...) {
    ++g_checks;
    if (ok) return;
    if (++g_bad <= 24) {
        char line[240];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(line, sizeof line, fmt, ap);
        va_end(ap);
        bof3::Log("shadow      name_grid self-test MISMATCH: %s", line);
    }
}

// --- the sounds and the draws, recorded -------------------------------------------

std::vector<unsigned short> g_sounds;
void Play(unsigned short id) { g_sounds.push_back(id); }
bool Sounds(std::initializer_list<unsigned short> want) { return g_sounds == std::vector<unsigned short>(want); }

struct Drawn { char what; int x, y, a, b; std::string s; };
std::vector<Drawn> g_drawn;
void RBox(int x, int y, int w, int h) { g_drawn.push_back({'B', x, y, w, h, {}}); }
void RText(int x, int y, int colour, int count, const unsigned char* s) { g_drawn.push_back({'T', x, y, colour, count, reinterpret_cast<const char*>(s)}); }
void RSmall(int x, int y, int colour, int count, const unsigned char* s) { g_drawn.push_back({'S', x, y, colour, count, reinterpret_cast<const char*>(s)}); }
void RHand(int x, int y) { g_drawn.push_back({'H', x, y, 0, 0, {}}); }
void RMode(unsigned dtd, unsigned tpage) { g_drawn.push_back({'M', 0, 0, static_cast<int>(dtd), static_cast<int>(tpage), {}}); }
void RTile(int x, int y, unsigned u, unsigned v, unsigned clut) { g_drawn.push_back({'I', x, y, static_cast<int>(u * 256 + v), static_cast<int>(clut), {}}); }
void RPiece(int x, int y, unsigned id, unsigned flags, unsigned clut) { g_drawn.push_back({'P', x, y, static_cast<int>(id * 256 + flags), static_cast<int>(clut), {}}); }
void RFrame(int x, int y, Copy copy) { g_drawn.push_back({'F', x, y, static_cast<int>(copy), 0, {}}); }
constexpr Painter kRecorder{RBox, RText, RSmall, RHand, RMode, RTile, RPiece, RFrame};
unsigned CountDrawn(char what) {
    unsigned n = 0;
    for (const Drawn& d : g_drawn) n += d.what == what;
    return n;
}

// --- a synthetic chunk, written to section 5a's layout ---------------------------------

struct SynCell { unsigned char code, kind, b0, b1; };
struct Spec {
    unsigned char family, pages, first, codes;
    SynCell cell[3][96];
    unsigned char map[5][4];
    unsigned slots, count, room;          // both copies' labels
    unsigned char box_w[2];
    std::vector<std::vector<unsigned char>> ops[2];
};

// Invented glyphs: code 6 + n, one byte
// 0x30 + n for the first 40, two bytes (0x81, n) after. Code 0xFF, the space,
// is one byte 0x20, wherever a builder puts it.
SynCell Glyph(unsigned n) {
    const unsigned code = 6 + n;   // unique below 0xFF for the 224 glyphs a synthetic grid has
    if (n < 40) return {static_cast<unsigned char>(code), 1, static_cast<unsigned char>(0x30 + n), 0};
    return {static_cast<unsigned char>(code), 2, 0x81, static_cast<unsigned char>(n)};
}
constexpr SynCell kSpace{0xFF, 1, 0x20, 0};
constexpr SynCell kHole{0, 0, 0, 0};
SynCell Special(unsigned code) { return {static_cast<unsigned char>(code), 3, 0, 0}; }

std::vector<unsigned char> Op(unsigned char op, std::initializer_list<unsigned char> a) {
    std::vector<unsigned char> o{op};
    o.insert(o.end(), a.begin(), a.end());
    o.resize(8, 0);
    return o;
}

// Japan's shape: three pages from 0; codes 1..5 in rows 3..7; page 2 (the
// last) has holes in rows 5..7 of columns 0..9 and in row 1 of columns 3..4.
Spec JapanSpec() {
    Spec s{};
    s.family = 0; s.pages = 3; s.first = 0; s.codes = 5;
    unsigned n = 0;
    for (unsigned p = 0; p < 3; ++p)
        for (unsigned r = 0; r < 8; ++r)
            for (unsigned c = 0; c < 12; ++c) {
                SynCell& cell = s.cell[p][r * 12 + c];
                if (r >= 3 && c >= 10) cell = Special(r - 2);
                else if (p == 2 && ((r >= 5 && c <= 9) || (r == 1 && (c == 3 || c == 4)))) cell = kHole;
                else if (p == 0 && r == 2 && c == 5) cell = kSpace;
                else cell = Glyph(n++);
            }
    const unsigned char map[5][4] = {{1, 0, 0, 4}, {2, 1, 0, 4}, {3, 4, 1, 4}, {4, 2, 0, 2}, {5, 3, 0, 2}};
    std::memcpy(s.map, map, sizeof map);
    s.slots = 7; s.count = 4; s.room = 8;
    s.box_w[0] = 0x78; s.box_w[1] = 0x4D;
    for (unsigned k = 0; k < 2; ++k) {
        if (k == 0) { s.ops[k].push_back(Op(5, {0, 0x1D, 0})); s.ops[k].push_back(Op(4, {0x50, 5, 0x2C, 1, 0x4C, 0x78})); }
        for (unsigned i = 0; i < 3; ++i) s.ops[k].push_back(Op(1, {0x2A, static_cast<unsigned char>(6 + 8 * i), static_cast<unsigned char>(i), 0, 0x10, 0}));
        s.ops[k].push_back(Op(1, {0x2A, 0x26, 3, 0, 0x10, 0}));
        s.ops[k].push_back(Op(1, {0x2A, 0x1E, 4, 1, 0x10, 0}));
        s.ops[k].push_back(Op(2, {0x1A, 6, 7, 5, 3, 4, 8}));
        s.ops[k].push_back(Op(5, {0, 0x2F, 0}));
        const unsigned char shapes[4] = {0x1B, 0x1D, 0x1E, 0x1C};   // circle, cross, square, triangle: Japan's rows
        for (unsigned i = 0; i < 4; ++i) s.ops[k].push_back(Op(3, {0xE, static_cast<unsigned char>(6 + 8 * i), shapes[i], 0, 3, 0x78}));
        s.ops[k].push_back(Op(3, {0xA, 0x26, 4, 2, 0, 0x78}));    // the sheet's START label, two halves
        s.ops[k].push_back(Op(3, {0x12, 0x26, 5, 2, 0, 0x78}));
    }
    return s;
}

// The West's shape: one page numbered 2; codes 1..4 in rows 3..6, row 7
// empty, holes in row 5 at columns 7..9 and 0 (Left / Right from that row's
// special lands on one) and in row 2 at column 9.
Spec WestSpec() {
    Spec s{};
    s.family = 1; s.pages = 1; s.first = 2; s.codes = 4;
    unsigned n = 0;
    for (unsigned r = 0; r < 8; ++r)
        for (unsigned c = 0; c < 12; ++c) {
            SynCell& cell = s.cell[0][r * 12 + c];
            if (r == 7) cell = kHole;
            else if (r >= 3 && c >= 10) cell = Special(r - 2);
            else if ((r == 5 && ((c >= 7 && c <= 9) || c == 0)) || (r == 2 && c == 9)) cell = kHole;
            else if (r == 6 && c == 0) cell = kSpace;
            else cell = Glyph(n++);
        }
    const unsigned char map[4][4] = {{1, 0, 0, 4}, {2, 1, 0, 4}, {4, 2, 0, 2}, {5, 3, 0, 2}};
    std::memcpy(s.map, map, sizeof map);
    s.slots = 4; s.count = 5; s.room = 12;
    s.box_w[0] = 0x78; s.box_w[1] = 0x4D;
    for (unsigned k = 0; k < 2; ++k) {
        for (unsigned i = 0; i < 5; ++i) s.ops[k].push_back(Op(1, {0x1B, static_cast<unsigned char>(6 + 8 * i), static_cast<unsigned char>(i), 0, 0x10, 0}));
        const unsigned char shapes[4] = {0x1D, 0x1C, 0x1B, 0x1E};   // cross, triangle, circle, square: the West's rows
        for (unsigned i = 0; i < 4; ++i) s.ops[k].push_back(Op(3, {0xE, static_cast<unsigned char>(6 + 8 * i), shapes[i], 0, 3, 0x78}));
    }
    return s;
}

std::vector<unsigned char> Build(const Spec& s) {
    std::vector<unsigned char> b{1, s.family, s.pages, s.first, s.codes, 0, 0, 0};
    const char id[12] = {'s', 'y', 'n', 't', 'h', 'e', 't', 'i', 'c'};
    b.insert(b.end(), id, id + 12);
    for (unsigned p = 0; p < s.pages; ++p)
        for (unsigned i = 0; i < 96; ++i) {
            const SynCell& c = s.cell[p][i];
            b.insert(b.end(), {c.code, c.kind, c.b0, c.b1});
        }
    for (unsigned k = 0; k < s.codes; ++k) b.insert(b.end(), s.map[k], s.map[k] + 4);
    for (unsigned k = 0; k < 2; ++k) {
        b.insert(b.end(), {static_cast<unsigned char>(s.slots), static_cast<unsigned char>(s.count), static_cast<unsigned char>(s.room), 0});
        for (unsigned slot = 0; slot < s.slots; ++slot) {
            std::vector<unsigned char> label(s.room, 0);
            for (unsigned i = 0; i < 3; ++i) label[i] = static_cast<unsigned char>('a' + slot + k);
            b.insert(b.end(), label.begin(), label.end());
        }
        b.insert(b.end(), {s.box_w[k], 0x2F, 8, 6, static_cast<unsigned char>(s.ops[k].size()), 0, 0, 0});
        for (unsigned e = 0; e < 8; ++e) b.insert(b.end(), {0x81, static_cast<unsigned char>(0x10 + e), 0x81, static_cast<unsigned char>(0x20 + e), 0, 0});
        for (const auto& o : s.ops[k]) b.insert(b.end(), o.begin(), o.end());
    }
    return b;
}

bool Parses(const std::vector<unsigned char>& bytes, Grid& g, std::string& why) {
    return ParseChunk(bytes.data(), static_cast<std::uint32_t>(bytes.size()), g, why);
}

// --- the chunk's rules ------------------------------------------------------------

void CheckRules(const Spec& base, const char* name) {
    Grid g;
    std::string why;
    Check(Parses(Build(base), g, why), "%s: the synthetic chunk does not parse: %s", name, why.c_str());
    struct Breaker { const char* what; void (*edit)(Spec&); };
    const Breaker breakers[] = {
        {"row 0 empty", [](Spec& s) { s.cell[0][3] = kHole; }},
        {"a special off its place", [](Spec& s) { s.cell[0][12 * 2 + 10] = Special(1); }},
        {"columns 10 and 11 differ", [](Spec& s) { s.cell[0][12 * 4 + 11] = kHole; }},
        {"a code in two rows", [](Spec& s) { s.cell[0][12 * 4 + 10] = s.cell[0][12 * 4 + 11] = Special(1); }},
        {"a glyph code below 6", [](Spec& s) { s.cell[0][1].code = 5; }},
        {"a one-byte cell of 0x80", [](Spec& s) { s.cell[0][1] = {0xF0, 1, 0x80, 0}; }},
        {"two codes, one encoding", [](Spec& s) { s.cell[0][1].b0 = s.cell[0][0].b0; s.cell[0][1].b1 = s.cell[0][0].b1; s.cell[0][1].kind = s.cell[0][0].kind; }},
        {"one code, two encodings", [](Spec& s) { s.cell[0][1].code = s.cell[0][0].code; s.cell[0][1].b0 = 0x7E; s.cell[0][1].kind = 1; s.cell[0][1].b1 = 0; }},
        {"no space", [](Spec& s) { for (auto& page : s.cell) for (auto& c : page) if (c.code == 0xFF) c = {0xFE, 2, 0x82, 0x01}; }},
        {"an action twice", [](Spec& s) { s.map[1][0] = 1; }},
        {"a label slot past the slots", [](Spec& s) { s.map[0][1] = 9; }},
        {"a legend op of kind 6", [](Spec& s) { s.ops[1][0][0] = 6; }},
        {"a legend entry past 8", [](Spec& s) { s.ops[0].back() = Op(1, {0, 0, 8, 0, 1, 0}); }},
        {"a CLUT with bit 15", [](Spec& s) { s.ops[0].push_back(Op(3, {0, 0, 0, 0, 0, 0x80})); }},
    };
    for (const Breaker& b : breakers) {
        Spec s = base;
        b.edit(s);
        Check(!Parses(Build(s), g, why), "%s: a chunk with %s passes", name, b.what);
    }
    std::vector<unsigned char> bytes = Build(base);
    bytes.push_back(0);
    Check(!Parses(bytes, g, why), "%s: a byte after the chunk passes", name);
    bytes = Build(base);
    bytes.pop_back();
    Check(!Parses(bytes, g, why), "%s: a short chunk passes", name);
    bytes = Build(base);
    bytes[0] = 2;
    Check(!Parses(bytes, g, why), "%s: version 2 passes", name);
    bytes = Build(base);
    bytes[2] = base.family == 1 ? 2 : 4;
    Check(!Parses(bytes, g, why), "%s: a bad page count passes", name);
    bytes = Build(base);
    bytes[8] = 1;
    Check(!Parses(bytes, g, why), "%s: a control byte in the build id passes", name);
}

// --- the step --------------------------------------------------------------------

Keys None() { return Keys{}; }
Cells Fresh(const Grid& g) {
    Cells c{};
    std::memset(c.buffer, 0xFF, 5);
    c.buffer[5] = 0x77;
    c.page = g.first;
    return c;
}
unsigned Run(const Grid& g, Copy copy, Cells& c, const Keys& k, unsigned room = 8, unsigned cells = 5, const unsigned char* fallback = nullptr) {
    g_sounds.clear();
    return Step(g, copy, c, k, room, cells, fallback, Play);
}
Cells At(const Grid& g, unsigned row, unsigned col, unsigned page = 0xFF) {
    Cells c = Fresh(g);
    c.row = static_cast<unsigned char>(row);
    c.column = static_cast<unsigned char>(col);
    if (page != 0xFF) c.page = static_cast<unsigned char>(page);
    return c;
}
bool EmptyAt(const Grid& g, const Cells& c) { return g.cell[c.page - g.first][c.row * 12 + c.column].kind == 0; }

void CheckMoves(const Grid& jp, const Grid& w) {
    Keys up = None(); up.up = true;
    Keys down = None(); down.down = true;
    Keys left = None(); left.left = true;
    Keys right = None(); right.right = true;

    Cells c = At(jp, 0, 0);
    Run(jp, Copy::kNewGame, c, up);
    Check(c.row == 7 && Sounds({0x100}), "Up from row 0 wraps to row 7 (row %u)", c.row);
    c = At(w, 0, 0);
    Run(w, Copy::kNewGame, c, up);
    Check(c.row == 6, "West: Up from row 0 skips the empty row 7 (row %u)", c.row);
    c = At(w, 6, 3);
    Run(w, Copy::kNewGame, c, down);
    Check(c.row == 0, "West: Down from row 6 skips row 7 to row 0 (row %u)", c.row);
    c = At(w, 4, 8);
    Run(w, Copy::kNewGame, c, down);
    Check(c.row == 6, "West: Down from row 4 column 8 skips row 5's hole (row %u)", c.row);
    c = At(w, 6, 8);
    Run(w, Copy::kNewGame, c, up);
    Check(c.row == 4, "West: Up from row 6 column 8 skips row 5's hole (row %u)", c.row);
    c = At(w, 5, 10);
    Run(w, Copy::kNewGame, c, left);
    Check(c.row == 5 && c.column == 6 && Sounds({0x100}),
          "West: Left from row 5's special goes on left past its holes to the row's last letter (row %u column %u)", c.row, c.column);
    c = At(w, 5, 11);
    Run(w, Copy::kNewGame, c, right);
    Check(c.row == 5 && c.column == 1, "West: Right from row 5's special goes on right past column 0's hole (row %u column %u)", c.row, c.column);
    c = At(w, 4, 11);
    Run(w, Copy::kNewGame, c, right);
    Check(c.row == 4 && c.column == 0, "West: Right from a special goes to column 0 (column %u)", c.column);
    c = At(w, 4, 10);
    Run(w, Copy::kNewGame, c, left);
    Check(c.row == 4 && c.column == 9, "West: Left from a special goes to column 9 (column %u)", c.column);
    c = At(w, 5, 1);
    Run(w, Copy::kNewGame, c, left);
    Check(c.row == 5 && c.column == 11, "West: Left from column 1 skips column 0's hole and wraps to the special (column %u)", c.column);
    c = At(w, 5, 6);
    Run(w, Copy::kNewGame, c, right);
    Check(c.row == 5 && c.column == 10, "West: Right from column 6 skips the hole to the special (column %u)", c.column);
    c = At(w, 5, 10);
    Run(w, Copy::kNewGame, c, down);
    Check(c.column == 10 && c.row == 6, "West: Down along the specials (row %u)", c.row);
    c = At(w, 2, 8);
    Run(w, Copy::kNewGame, c, right);
    Check(c.column == 10, "West: Right from row 2 column 8 skips column 9 (column %u)", c.column);
    c = At(w, 0, 0);
    Run(w, Copy::kNewGame, c, left);
    Check(c.column == 11, "Left from column 0 wraps to column 11 (column %u)", c.column);
    c = At(w, 0, 11);
    Run(w, Copy::kNewGame, c, right);
    Check(c.column == 0, "Right from column 11 wraps to column 0 (column %u)", c.column);
    c = At(jp, 3, 10, 2);
    Run(jp, Copy::kNewGame, c, down);
    Check(c.row == 4 && c.column == 10, "Japan: Down along the specials on page 2 (row %u)", c.row);
    c = At(jp, 6, 10, 2);
    Run(jp, Copy::kNewGame, c, left);
    Check(c.row == 4 && c.column == 9, "Japan: Left from a special on page 2 clamps up, the disc's rule (row %u column %u)", c.row, c.column);
    c = At(jp, 6, 11, 2);
    Run(jp, Copy::kNewGame, c, right);
    Check(c.row == 4 && c.column == 0, "Japan: Right from a special on page 2 clamps up, the disc's rule (row %u column %u)", c.row, c.column);
    c = At(jp, 4, 0, 2);
    Run(jp, Copy::kNewGame, c, down);
    Check(c.row == 0 && c.column == 0, "Japan: Down from row 4 over page 2's holes wraps to row 0 (row %u)", c.row);
    c = At(jp, 0, 2, 2);
    Run(jp, Copy::kNewGame, c, right);
    Check(c.column == 3 && c.row == 0, "Japan: Right on row 0 (column %u)", c.column);
    c = At(jp, 1, 2, 2);
    Run(jp, Copy::kNewGame, c, right);
    Check(c.column == 5 && c.row == 1, "Japan: Right over row 1's holes (column %u)", c.column);
    // One direction a frame, Up first.
    Keys both = None(); both.up = both.left = true;
    c = At(jp, 2, 2);
    Run(jp, Copy::kNewGame, c, both);
    Check(c.row == 1 && c.column == 2 && Sounds({0x100}), "Up and Left: Up alone (row %u column %u)", c.row, c.column);
}

void CheckCodes(const Grid& jp, const Grid& w) {
    Keys ok = None(); ok.confirm = true;
    for (unsigned family = 0; family < 2; ++family) {
        const Grid& g = family == 0 ? jp : w;
        const char* const name = family == 0 ? "Japan" : "West";
        // Code 1 (next) and code 2 (back) at and off their limits.
        Cells c = At(g, 3, 10);
        Run(g, Copy::kRenamer, c, ok);
        Check(c.cursor == 1 && Sounds({0x103}), "%s: code 1 moves the name cursor on", name);
        c.cursor = 4;
        Run(g, Copy::kRenamer, c, ok);
        Check(c.cursor == 4 && Sounds({0x107}), "%s: code 1 at the last cell plays 0x107", name);
        c.cursor = 3;
        Run(g, Copy::kRenamer, c, ok, 8, 4);
        Check(c.cursor == 3 && Sounds({0x107}), "%s: code 1 under the 4-cell cap stops at 3", name);
        c = At(g, 4, 11);
        c.cursor = 2;
        Run(g, Copy::kRenamer, c, ok);
        Check(c.cursor == 1 && Sounds({0x103}), "%s: code 2 moves the name cursor back", name);
        c.cursor = 0;
        Run(g, Copy::kRenamer, c, ok);
        Check(c.cursor == 0 && Sounds({0x107}), "%s: code 2 at 0 plays 0x107", name);
        // Quit: Japan code 4 (row 6), West code 3 (row 5).
        const unsigned quit_row = family == 0 ? 6 : 5;
        c = At(g, quit_row, 10);
        Keys ok_next = ok; ok_next.next = true;
        unsigned r = Run(g, Copy::kRenamer, c, ok_next);
        Check(r == kQuit && Sounds({}) && c.cursor == 0, "%s: the renamer's quit ends the step at once, no sound (result %u)", name, r);
        c = At(g, quit_row, 10);
        r = Run(g, Copy::kNewGame, c, ok_next);
        Check(r == kQuit && Sounds({0x103, 0x101}) && c.cursor == 1, "%s: New Game's quit plays 0x103 and the step goes on (result %u)", name, r);
        // Finish by its code: Japan code 5 (row 7), West code 4 (row 6).
        const unsigned finish_row = family == 0 ? 7 : 6;
        c = At(g, finish_row, 10);
        c.buffer[0] = g.cell[0][0].code;
        r = Run(g, Copy::kRenamer, c, ok);
        Check(r == kFinished && Sounds({0x104}) && c.buffer[0] == g.cell[0][0].code && c.buffer[1] == 0 && c.buffer[4] == 0,
              "%s: the finish code finishes the renamer, trailing blanks to 0 (result %u)", name, r);
        // The finish code loses to the name cursor's key in the same frame, as the original's chain.
        c = At(g, finish_row, 10);
        r = Run(g, Copy::kRenamer, c, ok_next);
        Check(r == kNothing && c.cursor == 1, "%s: the finish code with the next key moves the cursor only (result %u)", name, r);
    }
    // Japan's page code (row 5) and the page key.
    Cells c = At(jp, 5, 10, 1);
    Run(jp, Copy::kRenamer, c, ok);
    Check(c.page == 2 && c.row == 5 && c.column == 10 && Sounds({0x103}), "Japan: the page code to page 2, specials kept (row %u)", c.row);
    c = At(jp, 7, 3, 1);
    Keys page = None(); page.page = true;
    Run(jp, Copy::kRenamer, c, page);
    Check(c.page == 2 && c.row == 4 && c.column == 3 && Sounds({0x103}), "Japan: a page change clamps up out of page 2's holes (row %u)", c.row);
    c = At(jp, 1, 4, 1);
    Run(jp, Copy::kRenamer, c, page);
    Check(c.page == 2 && c.row == 0, "Japan: a page change clamps row 1's hole to row 0 (row %u)", c.row);
    Run(jp, Copy::kRenamer, c, page);
    Check(c.page == 0, "Japan: the page wraps from 2 to 0 (page %u)", c.page);
    c = At(w, 0, 0);
    Run(w, Copy::kRenamer, c, page);
    Check(c.page == 2 && Sounds({}), "West: no page change (page %u)", c.page);
}

void CheckEntry(const Grid& jp, const Grid& w) {
    Keys ok = None(); ok.confirm = true;
    // A glyph: the cell's code into the buffer, the cursor on; byte 5 by copy.
    Cells c = At(w, 1, 1);
    const unsigned char code = w.cell[0][13].code;
    Run(w, Copy::kNewGame, c, ok);
    Check(c.buffer[0] == code && c.cursor == 1 && c.buffer[5] == 0 && Sounds({0x103}), "a glyph enters, START clears byte 5");
    c = At(w, 1, 1);
    Run(w, Copy::kRenamer, c, ok);
    Check(c.buffer[0] == code && c.buffer[5] == 0x77, "the renamer leaves byte 5");
    c = At(w, 1, 1);
    c.cursor = 4;
    Run(w, Copy::kRenamer, c, ok);
    Check(c.buffer[4] == code && c.cursor == 4, "a glyph at the last cell keeps the cursor there");
    // The byte room: West glyphs from 40 on are two bytes (row 3 column 4 on).
    Cells two = At(w, 4, 0);
    const unsigned char wide = w.cell[0][48].code;
    Check(w.enc_len[wide] == 2, "the synthetic West grid has a two-byte cell at row 4");
    for (unsigned i = 0; i < 4; ++i) Run(w, Copy::kRenamer, two, ok, 8);
    Check(two.cursor == 4 && two.buffer[3] == wide, "four two-byte cells fit 8 bytes");
    Run(w, Copy::kRenamer, two, ok, 8);
    Check(two.buffer[4] == 0xFF && two.cursor == 4 && Sounds({0x107}), "a fifth two-byte cell is refused at 8 bytes with 0x107");
    Cells slot = At(w, 4, 0);
    Run(w, Copy::kRenamer, slot, ok, 5);
    Run(w, Copy::kRenamer, slot, ok, 5);
    Run(w, Copy::kRenamer, slot, ok, 5);
    Check(slot.cursor == 2 && slot.buffer[2] == 0xFF && Sounds({0x107}), "a third two-byte cell is refused in 5 bytes");
    Cells one = slot;
    one.row = 0;
    Run(w, Copy::kRenamer, one, ok, 5);
    Check(one.cursor == 3 && w.enc_len[one.buffer[2]] == 1, "a one-byte cell still fits 5 bytes");
    // Overwriting a cell counts the bytes it leaves: 2 + 2 + 1, a two-byte cell over the one-byte one.
    one.cursor = 2;
    one.row = 4;
    Run(w, Copy::kRenamer, one, ok, 5);
    Check(one.buffer[2] != wide && Sounds({0x107}), "overwriting a one-byte cell with a two-byte one past the room is refused");
    // The 4-cell cap: the cursor never reaches 4.
    Cells cap = At(jp, 0, 0);
    for (unsigned i = 0; i < 6; ++i) Run(jp, Copy::kNewGame, cap, ok, 8, 4);
    Check(cap.cursor == 3 && cap.buffer[4] == 0xFF, "under the 4-cell cap five entries stop at cell 3");

    // The name cursor's keys.
    Keys next = None(); next.next = true;
    Keys back = None(); back.back = true;
    c = At(w, 0, 0);
    Run(w, Copy::kRenamer, c, next);
    Check(c.cursor == 1 && Sounds({0x101}), "the next key moves the name cursor with 0x101");
    c.cursor = 4;
    Run(w, Copy::kRenamer, c, next);
    Check(c.cursor == 4 && Sounds({}), "the next key at the last cell is silent");
    Run(w, Copy::kRenamer, c, back);
    Check(c.cursor == 3 && Sounds({0x101}), "the back key moves the name cursor back");
    c.cursor = 0;
    Run(w, Copy::kRenamer, c, back);
    Check(c.cursor == 0 && Sounds({}), "the back key at 0 is silent");
    Keys both = None(); both.next = both.back = true;
    c.cursor = 2;
    Run(w, Copy::kRenamer, c, both);
    Check(c.cursor == 3, "next before back");

    // Backspace.
    Keys erase = None(); erase.erase = true;
    c = At(w, 0, 0);
    c.cursor = 2;
    c.buffer[0] = c.buffer[1] = code;
    Run(w, Copy::kNewGame, c, erase);
    Check(c.cursor == 1 && c.buffer[1] == 0xFF && c.buffer[5] == 0 && Sounds({0x106}), "backspace on a blank steps back and clears");
    c.cursor = 1;
    c.buffer[1] = code;
    c.buffer[5] = 0x77;
    Run(w, Copy::kRenamer, c, erase);
    Check(c.cursor == 1 && c.buffer[1] == 0xFF && c.buffer[5] == 0x77, "backspace on a glyph clears it in place");
    c.cursor = 2;
    c.buffer[2] = 0;
    Run(w, Copy::kRenamer, c, erase);
    Check(c.cursor == 1, "backspace on a 0 steps back too");
    c.cursor = 0;
    Run(w, Copy::kRenamer, c, erase);
    Check(c.cursor == 0 && c.buffer[0] == 0xFF && Sounds({0x106}), "backspace at 0 clears cell 0");
    Keys erase_finish = erase; erase_finish.finish = true;
    c = At(w, 0, 0);
    unsigned r = Run(w, Copy::kRenamer, c, erase_finish);
    Check(r == kNothing, "backspace before finish");

    // The finish: both copies' all-blank rules and the trim.
    Keys finish = None(); finish.finish = true;
    c = At(w, 0, 0);
    r = Run(w, Copy::kRenamer, c, finish);
    Check(r == kFinished && Sounds({0x104}) && c.buffer[0] == 0 && c.buffer[4] == 0, "the renamer's all-blank finish: five 0 bytes");
    const unsigned char fallback[5] = {code, code, 0, 0, 0};
    c = At(w, 0, 0);
    r = Run(w, Copy::kNewGame, c, finish, 8, 5, fallback);
    Check(r == kFinished && Sounds({0x105}) && std::memcmp(c.buffer, fallback, 5) == 0, "New Game's all-blank finish: the default name");
    c = At(w, 0, 0);
    c.buffer[0] = code; c.buffer[1] = 0xFF; c.buffer[2] = code;
    r = Run(w, Copy::kNewGame, c, finish, 8, 5, fallback);
    Check(r == kFinished && c.buffer[1] == 0xFF && c.buffer[2] == code && c.buffer[3] == 0 && c.buffer[4] == 0,
          "a finish keeps a blank inside the name and ends it after the last cell");
    std::memset(c.buffer, code, 5);
    r = Run(w, Copy::kRenamer, c, finish);
    Check(r == kFinished && c.buffer[4] == code, "a finish of five cells keeps them");
}

void CheckKeys() {
    const auto identity = [](std::uint16_t b) { return b; };
    Keys k = KeysOf(Family::kJapan, 0x20, 0, identity);
    Check(k.confirm && !k.erase, "Japan: 0x20 confirms");
    k = KeysOf(Family::kJapan, 0x40 | 0x10 | 0x80 | 0x800, 0, identity);
    Check(k.erase && k.page && k.next && k.finish && !k.confirm && !k.back, "Japan: 0x40 erases, 0x10 pages, 0x80 next, 0x800 finishes");
    k = KeysOf(Family::kJapan, 0, 0x4 | 0x1000, identity);
    Check(k.back && k.up && !k.next, "Japan: repeated 0x4 back, 0x1000 up");
    k = KeysOf(Family::kJapan, 0, 0x8, identity);
    Check(k.next, "Japan: repeated 0x8 next");
    k = KeysOf(Family::kWest, 0x40, 0, identity);
    Check(k.confirm && !k.erase, "West: 0x40 confirms");
    k = KeysOf(Family::kWest, 0x10 | 0x20 | 0x80, 0, identity);
    Check(k.erase && k.next && k.back && !k.page, "West: 0x10 erases, 0x20 next, 0x80 back, no page");
    k = KeysOf(Family::kWest, 0, 0x2 | 0x8000 | 0x2000 | 0x4000, identity);
    Check(k.next && k.left && k.right && k.down, "West: repeated 0x2 next and the hand");
    // A map is applied to each bit: a swap of 0x20 and 0x40 swaps Japan's confirm and erase.
    const auto swap = [](std::uint16_t b) -> std::uint16_t { return b == 0x20 ? 0x40 : b == 0x40 ? 0x20 : b; };
    k = KeysOf(Family::kJapan, 0x40, 0, swap);
    Check(k.confirm && !k.erase, "a mapped confirm");
}

void CheckCodec(const Grid& jp, const Grid& w) {
    unsigned char cells[5] = {w.cell[0][0].code, 0xFF, w.cell[0][48].code, 0xFF, 0xFF};
    unsigned char out[9];
    std::memset(out, 0xEE, sizeof out);
    const unsigned n = Encode(w, cells, out, 9);
    Check(n == 4 && out[0] == w.cell[0][0].b0 && out[1] == 0x20 && out[2] == 0x81 && out[4] == 0 && out[8] == 0,
          "encode: a glyph, the space, a two-byte glyph, NUL-filled (%u bytes)", n);
    Check(EncodedLength(w, cells) == 4, "the encoded length");
    unsigned char back[5];
    Check(Decode(w, out, 9, back, nullptr) && back[0] == cells[0] && back[1] == 0xFF && back[2] == cells[2] && back[3] == 0xFF,
          "decode is the reverse");
    const unsigned char five[5] = {w.cell[0][1].code, w.cell[0][2].code, w.cell[0][3].code, w.cell[0][4].code, w.cell[0][5].code};
    Check(Encode(w, five, out, 5) == 5 && Decode(w, out, 5, back, nullptr) && std::memcmp(back, five, 5) == 0, "five one-byte cells in 5 bytes, both ways");
    const unsigned char unknown[3] = {0x7F, 0, 0};
    std::memset(back, 0x11, 5);
    Check(!Decode(w, unknown, 3, back, nullptr) && back[0] == 0x11, "decode refuses a byte on no cell and leaves the cells");
    const unsigned char six[7] = {out[0], out[1], out[2], out[3], out[4], out[0], 0};
    Check(!Decode(w, six, 7, back, nullptr), "decode refuses a sixth cell");
    // A pair code: one two-byte unit standing for two of the grid's glyphs.
    const auto pairs = [](std::uint32_t glyph, std::uint32_t* a, std::uint32_t* b) {
        if (glyph != 0x7F00) return false;
        *a = 0x100 + 40;
        *b = 0x100 + 41;
        return true;
    };
    const unsigned char paired[3] = {0xFF, 0x00, 0};
    Check(Decode(jp, paired, 3, back, pairs) && jp.enc_len[back[0]] == 2 && jp.enc[back[0]][1] == 40 && jp.enc[back[1]][1] == 41 && back[2] == 0xFF,
          "decode splits a pair code into its two cells");
    const unsigned char stops[3] = {out[0], 0, out[0]};
    Check(Decode(w, stops, 3, back, nullptr) && back[1] == 0xFF, "decode stops at a NUL");
}

std::uint16_t Identity(std::uint16_t b) { return b; }
std::uint16_t SwapPair(std::uint16_t b, std::uint16_t x, std::uint16_t y) { return b == x ? y : b == y ? x : b; }
std::uint16_t SwapCircleCross(std::uint16_t b) { return SwapPair(b, 0x20, 0x40); }
std::uint16_t SwapCircleL1(std::uint16_t b) { return SwapPair(b, 0x20, 0x04); }
std::uint16_t SwapCrossTriangle(std::uint16_t b) { return SwapPair(b, 0x40, 0x10); }
std::vector<int> Tiles() {   // the tiles drawn, u * 256 + v, in order
    std::vector<int> out;
    for (const Drawn& d : g_drawn)
        if (d.what == 'I') out.push_back(d.a);
    return out;
}
int TileClut(unsigned n) {
    for (const Drawn& d : g_drawn)
        if (d.what == 'I' && n-- == 0) return d.b;
    return -1;
}
detail::Op Op8(unsigned char op, std::initializer_list<unsigned char> a) {
    detail::Op o{op, {}};
    unsigned i = 0;
    for (unsigned char b : a) o.a[i++] = b;
    return o;
}

void CheckDraws(const Grid& jp, const Grid& w) {
    Cells c = At(jp, 4, 11, 2);
    g_drawn.clear();
    DrawGrid(jp, Copy::kRenamer, c, 0x18, 0x67, kRecorder);
    Check(g_drawn.front().what == 'B' && g_drawn.front().a == 0x110 && g_drawn.front().b == 0x7B, "the grid's box");
    Check(CountDrawn('T') == 96 - 5, "the grid draws every cell but column 11 of rows 3..7 (%u)", CountDrawn('T'));
    const Drawn& h = g_drawn.back();
    Check(h.what == 'H' && h.x == 0x18 + 6 + 20 * 11 + 16 - 0x14 && h.y == 0x67 + 8 + 14 * 4, "the hand on column 11's label (%d, %d)", h.x, h.y);
    bool page_label = false;
    for (const Drawn& d : g_drawn)
        if (d.what == 'T' && d.y == 0x67 + 6 + 14 * 5 && d.x == 0x18 + 4 + 0x14 * 8 + 0x1C * 2) page_label = d.s == std::string(3, static_cast<char>('a' + 4 + 2 + 1)) && d.b == 4;
    Check(page_label, "page 2's code-3 label is slot 4 + 2 of the renamer's labels, count 4");
    g_drawn.clear();
    DrawLegend(jp, Copy::kNewGame, 1, 0xA8, 0x2B, kRecorder, Identity);
    Check(g_drawn[0].what == 'M' && g_drawn[0].a == 1 && g_drawn[0].b == 0xF && g_drawn[1].what == 'B' && g_drawn[1].a == 0x78,
          "START's legend: its own mode, then its box");
    Check(CountDrawn('S') == 5 + 15 && CountDrawn('I') == 6 && CountDrawn('P') == 1 && CountDrawn('M') == 3 && g_drawn.back().what == 'F',
          "START's legend: five texts, fifteen leaders, six tiles, the piece, three modes (no extra one), the frame");
    bool by_page = false;
    for (const Drawn& d : g_drawn) by_page |= d.what == 'S' && d.y == 0x2B + 0x1E && static_cast<unsigned char>(d.s[1]) == 0x10 + 4 + 1;
    Check(by_page, "the legend's entry 4 + page");
    Check(Tiles() == std::vector<int>({0x1B00, 0x1D00, 0x1E00, 0x1C00, 0x0402, 0x0502}), "the identity map draws the chunk's tiles");
    // A Config swap of circle and cross (Japan's terms): the circle and cross rows trade pictures.
    g_drawn.clear();
    DrawLegend(jp, Copy::kNewGame, 1, 0xA8, 0x2B, kRecorder, SwapCircleCross);
    Check(Tiles() == std::vector<int>({0x1D00, 0x1B00, 0x1E00, 0x1C00, 0x0402, 0x0502}) && TileClut(0) == 0x7803 && TileClut(1) == 0x7803,
          "a circle / cross swap trades the two rows' tiles, in the shapes' CLUT");
    // circle on L1: the circle row shows the sheet's L1 label, in the CLUT the legend draws START with.
    g_drawn.clear();
    DrawLegend(jp, Copy::kNewGame, 1, 0xA8, 0x2B, kRecorder, SwapCircleL1);
    Check(Tiles() == std::vector<int>({0x0002, 0x1D00, 0x1E00, 0x1C00, 0x0402, 0x0502}) && TileClut(0) == 0x7800,
          "circle on L1: the sheet's L1 label in START's CLUT (tile %04X clut %04X)", Tiles()[0], TileClut(0));
    // COMMU02 of the French shape: texts then tiles with no mode of its own -
    // the page the PlayStation's small draw left is given before the first tile.
    g_drawn.clear();
    DrawLegend(w, Copy::kRenamer, 2, 0xA8, 0x2B, kRecorder, Identity);
    unsigned first_tile = 0, mode_at = 0;
    for (unsigned i = 0; i < g_drawn.size(); ++i) {
        if (g_drawn[i].what == 'I' && first_tile == 0) first_tile = i;
        if (g_drawn[i].what == 'M') mode_at = i;
    }
    Check(g_drawn[0].what == 'B' && g_drawn[0].a == 0x4D && CountDrawn('M') == 1 && CountDrawn('I') == 4 && mode_at + 1 == first_tile &&
              g_drawn[mode_at].a == 0 && g_drawn[mode_at].b == 0x2F,
          "COMMU02's legend: no mode of its own; the small draw's page 0x2F before its tiles");
    g_drawn.clear();
    DrawLegend(w, Copy::kRenamer, 2, 0xA8, 0x2B, kRecorder, SwapCrossTriangle);
    Check(Tiles() == std::vector<int>({0x1C00, 0x1D00, 0x1B00, 0x1E00}), "a cross / triangle swap trades those rows");
    // A tile off the column is drawn as the chunk has it, on the page the disc
    // leaves; a column row is the sheet's tile on page 0x2F whatever came before.
    Grid g2 = jp;
    CopyData& d = g2.copy[0];
    d.ops = 0;
    d.op[d.ops++] = Op8(5, {0, 0x1D, 0});
    d.op[d.ops++] = Op8(3, {0x2A, 0x26, 0x1B, 0, 3, 0x78});   // off the column, on the UI page: as it is, no mode
    d.op[d.ops++] = Op8(3, {0xE, 6, 0x1B, 0, 3, 0x78});       // the column's row 0: page 0x2F given first
    d.op[d.ops++] = Op8(5, {0, 0x1D, 0});
    d.op[d.ops++] = Op8(1, {0x2A, 6, 0, 0, 0x10, 0});
    d.op[d.ops++] = Op8(3, {0x2A, 0x26, 0, 0x13, 0, 0x78});   // after a text, off the column: the small draw's page
    g_drawn.clear();
    DrawLegend(g2, Copy::kNewGame, 0, 0, 0, kRecorder, SwapCircleCross);
    std::vector<int> modes;
    for (const Drawn& dr : g_drawn)
        if (dr.what == 'M') modes.push_back(dr.b);
    Check(Tiles() == std::vector<int>({0x1B00, 0x1D00, 0x0013}) && modes == std::vector<int>({0xF, 0x1D, 0x2F, 0x1D, 0x2F}),
          "off the column as the chunk has it; the column's row on 0x2F, remapped; after a text 0x2F (%zu modes)", modes.size());
    // psp-eu's shape (PE's chunk): rows 2 and 3 of the column are one-letter
    // texts; they are drawn as the row's button tile like the others.
    Grid g3 = w;
    CopyData& e = g3.copy[1];
    e.op[7] = Op8(1, {0xE, 0x16, 6, 0, 1, 0});
    e.op[8] = Op8(1, {0xE, 0x1E, 5, 0, 1, 0});
    g_drawn.clear();
    DrawLegend(g3, Copy::kRenamer, 2, 0xA8, 0x2B, kRecorder, Identity);
    Check(Tiles() == std::vector<int>({0x1D00, 0x1C00, 0x1B00, 0x1E00}) && CountDrawn('S') == 5 && TileClut(3) == 0x7803,
          "psp-eu's letters in the column are drawn as the West's row tiles (%zu tiles, %u texts)", Tiles().size(), CountDrawn('S'));
}

// The random walk: the hand never on an empty cell, the cells in range.
std::uint32_t g_rng = 0x6C078965u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }

void Walk(const Grid& g, const char* name) {
    unsigned frames = 0, empties = 0, finishes = 0, quits = 0, pages_seen = 0;
    const unsigned char fallback[5] = {g.cell[0][0].code, 0, 0, 0, 0};
    for (unsigned run = 0; run < 64; ++run) {
        const Copy copy = (run & 1) ? Copy::kRenamer : Copy::kNewGame;
        const unsigned cells = (run & 2) ? 4 : 5;
        const unsigned room = (run & 4) ? 5 : 8;
        Cells c = Fresh(g);
        for (unsigned f = 0; f < 400; ++f) {
            const std::uint32_t r = Next();
            Keys k{};
            k.up = r & 1; k.down = r & 2; k.left = r & 4; k.right = r & 8;
            k.confirm = (r >> 4) % 3 == 0; k.page = (r >> 6) % 11 == 0; k.next = (r >> 9) % 7 == 0;
            k.back = (r >> 12) % 7 == 0; k.erase = (r >> 15) % 9 == 0; k.finish = (r >> 18) % 61 == 0;
            const unsigned result = Run(g, copy, c, k, room, cells, fallback);
            ++frames;
            if (EmptyAt(g, c)) ++empties;
            if (result & kFinished) { ++finishes; c = Fresh(g); }
            if (result & kQuit) { ++quits; c = Fresh(g); }
            if (c.page != g.first) ++pages_seen;
            if (c.cursor >= cells) Check(false, "%s: the name cursor at %u of %u", name, c.cursor, cells);
            if (EncodedLength(g, c.buffer) < 0) Check(false, "%s: a buffer cell on no glyph", name);
            else if (!(result & kFinished) && static_cast<unsigned>(EncodedLength(g, c.buffer)) > room)
                Check(false, "%s: the name passes the room", name);
        }
    }
    Check(empties == 0, "%s: the hand on an empty cell in %u of %u frames", name, empties, frames);
    Check(finishes > 0 && quits > 0, "%s: the walk finished %u and quit %u times", name, finishes, quits);
    Check(g.pages == 1 ? pages_seen == 0 : pages_seen > 0, "%s: pages left %u times", name, pages_seen);
}

}  // namespace

void SelfTest() {
    g_checks = g_bad = 0;
    const Spec jp_spec = JapanSpec(), w_spec = WestSpec();
    CheckRules(jp_spec, "Japan");
    CheckRules(w_spec, "West");
    Grid jp, w;
    std::string why;
    if (!Parses(Build(jp_spec), jp, why) || !Parses(Build(w_spec), w, why))
        bof3::Fatal("name_grid self-test: the synthetic grids do not parse: %s", why.c_str());
    CheckMoves(jp, w);
    CheckCodes(jp, w);
    CheckEntry(jp, w);
    CheckKeys();
    CheckCodec(jp, w);
    CheckDraws(jp, w);
    Walk(jp, "Japan");
    Walk(w, "West");
    bof3::Log("shadow      name_grid self-test: %u checks over two synthetic grids (no layer read), %u MISMATCHES",
              g_checks, g_bad);
}

}  // namespace name_grid

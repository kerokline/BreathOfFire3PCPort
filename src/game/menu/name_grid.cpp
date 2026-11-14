// The name entry's grid, ours: the PlayStation's routine (START.EMI and
// COMMU02.EMI, one body), the kind-22 chunk's reader and the commit side's
// helpers. docs/new-game-round.md (group NG; section 5a is the chunk's
// layout), docs/name-entry-restoration.md section 4 (the routine's skeleton).
//
// Read for this file (capstone over the held discs, 2026-11-14): the US
// START input step 0x801E4744..0x801E4E8C, the Japan START input step
// 0x801E459C..0x801E4E0C, the US COMMU02 input step 0x801DA11C..0x801DA7A4,
// the US and Japan START drawers 0x801E6490.. / 0x801E6418.., and the six
// legend shapes of section 5a. What differs from them is said where it
// happens, marked "ours:" (the round's ledger entries are stage C's).
//
// Nothing calls this unit yet (stage B: TF, CR, BT).
#include "game/menu/name_grid.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

#include "bof3/symbols.gen.h"
#include "common/image_base.h"
#include "common/sha256.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/text_pairs.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace name_grid {
namespace detail {
namespace {

// --- the chunk's layout (section 5a): the one place in the source that knows it ---

constexpr std::uint32_t kHeaderSize = 0x14;
constexpr std::uint32_t kCellSize = 4;
constexpr std::uint32_t kCodeSize = 4;
constexpr std::uint32_t kLabelHead = 4;
constexpr std::uint32_t kLegendHead = 8;
constexpr std::uint32_t kOpSize = 8;
constexpr unsigned char kVersion = 1;
constexpr unsigned kEntries = 8;
enum Kind : unsigned char { kEmpty = 0, kOneByte = 1, kTwoBytes = 2, kSpecial = 3 };
enum OpKind : unsigned char { kText = 1, kLeaders = 2, kTile = 3, kPiece = 4, kMode = 5 };

// A reader over the payload that names what it ran out in.
struct Reader {
    const std::uint8_t* p;
    std::uint32_t size, at;
    std::string* why;
    bool Need(std::uint32_t n, const char* what) {
        if (at <= size && size - at >= n) return true;
        *why = std::string("ends inside ") + what;
        return false;
    }
};

bool Fail(std::string& why, const char* fmt, unsigned a = 0, unsigned b = 0, unsigned c = 0) {
    char line[200];
    std::snprintf(line, sizeof line, fmt, a, b, c);
    why = line;
    return false;
}

// A NUL within `room` bytes and NULs after it; `nonempty` wants a first byte.
bool StringOk(const unsigned char* s, unsigned room) {
    unsigned n = 0;
    while (n < room && s[n] != 0) ++n;
    if (n == room) return false;
    for (unsigned i = n; i < room; ++i)
        if (s[i] != 0) return false;
    return true;
}

bool ParseCopy(Reader& r, CopyData& c, unsigned index, std::string& why) {
    const char* const name = index == 0 ? "START's" : "COMMU02's";
    if (!r.Need(kLabelHead, "a label block")) return false;
    const std::uint8_t* h = r.p + r.at;
    c.slots = h[0];
    c.count = h[1];
    c.room = h[2];
    if (c.slots < 1 || c.slots > kMaxSlots) return Fail(why, "copy %u: %u label slots (1..8)", index, c.slots);
    if (c.count < 1 || c.count > 15) return Fail(why, "copy %u: label draw count %u (1..15)", index, c.count);
    if (c.room < 2 || c.room > kMaxRoom) return Fail(why, "copy %u: label room %u (2..32)", index, c.room);
    if (h[3] != 0) return Fail(why, "copy %u: label block byte 3 is not 0", index);
    r.at += kLabelHead;
    if (!r.Need(static_cast<std::uint32_t>(c.slots) * c.room, name)) return false;
    for (unsigned s = 0; s < c.slots; ++s) {
        std::memset(c.label[s], 0, kMaxRoom);
        std::memcpy(c.label[s], r.p + r.at + s * c.room, c.room);
        if (!StringOk(c.label[s], c.room)) return Fail(why, "copy %u: label slot %u has no NUL or bytes after it", index, s);
    }
    r.at += static_cast<std::uint32_t>(c.slots) * c.room;

    if (!r.Need(kLegendHead, "a legend block")) return false;
    h = r.p + r.at;
    c.box_w = h[0];
    c.box_h = h[1];
    c.entries = h[2];
    c.entry_room = h[3];
    c.ops = h[4];
    if (c.box_w == 0 || c.box_h == 0) return Fail(why, "copy %u: a legend box of %u x %u", index, c.box_w, c.box_h);
    if (c.entries != kEntries) return Fail(why, "copy %u: %u legend entries (8)", index, c.entries);
    if (c.entry_room < 2 || c.entry_room > kMaxRoom) return Fail(why, "copy %u: legend entry room %u (2..32)", index, c.entry_room);
    if (c.ops < 1 || c.ops > kMaxOps) return Fail(why, "copy %u: %u legend operations (1..32)", index, c.ops);
    if (h[5] | h[6] | h[7]) return Fail(why, "copy %u: legend block bytes 5..7 are not 0", index);
    r.at += kLegendHead;
    if (!r.Need(static_cast<std::uint32_t>(c.entries) * c.entry_room, name)) return false;
    for (unsigned e = 0; e < c.entries; ++e) {
        std::memset(c.entry[e], 0, kMaxRoom);
        std::memcpy(c.entry[e], r.p + r.at + e * c.entry_room, c.entry_room);
        if (!StringOk(c.entry[e], c.entry_room)) return Fail(why, "copy %u: legend entry %u has no NUL or bytes after it", index, e);
    }
    r.at += static_cast<std::uint32_t>(c.entries) * c.entry_room;
    if (!r.Need(static_cast<std::uint32_t>(c.ops) * kOpSize, name)) return false;
    for (unsigned k = 0; k < c.ops; ++k) std::memcpy(&c.op[k], r.p + r.at + k * kOpSize, kOpSize);
    r.at += static_cast<std::uint32_t>(c.ops) * kOpSize;
    return true;
}

// The rules that need the whole chunk: an op's entry, a code's slot.
bool CheckCopy(const Grid& g, const CopyData& c, unsigned index, std::string& why) {
    const unsigned last_page = g.first + g.pages - 1u;
    for (unsigned k = 0; k < c.ops; ++k) {
        const Op& o = c.op[k];
        switch (o.op) {
        case kText:
        case kLeaders: {
            const unsigned entry = o.a[2];
            if (o.op == kText) {
                if (o.a[3] & ~1u) return Fail(why, "copy %u op %u: text flags 0x%X", index, k, o.a[3]);
                if (o.a[4] == 0) return Fail(why, "copy %u op %u: a text count of 0", index, k);
                if (o.a[6] != 0) return Fail(why, "copy %u op %u: byte 7 is not 0", index, k);
            } else if (o.a[3] == 0 || o.a[4] == 0) {
                return Fail(why, "copy %u op %u: leaders of %u rows", index, k, o.a[3]);
            }
            const bool by_page = o.op == kText && (o.a[3] & 1);
            for (unsigned page = g.first; page <= (by_page ? last_page : g.first); ++page) {
                const unsigned e = entry + (by_page ? page : 0);
                if (e >= c.entries) return Fail(why, "copy %u op %u: entry %u past the table", index, k, e);
                if (c.entry[e][0] == 0) return Fail(why, "copy %u op %u: entry %u is empty", index, k, e);
            }
            break;
        }
        case kTile:
        case kPiece:
            if (o.a[5] & 0x80) return Fail(why, "copy %u op %u: CLUT 0x%04X", index, k, o.a[4] | (o.a[5] << 8));
            if (o.a[6] != 0) return Fail(why, "copy %u op %u: byte 7 is not 0", index, k);
            break;
        case kMode:
            if (o.a[0] > 1) return Fail(why, "copy %u op %u: dtd %u", index, k, o.a[0]);
            if (o.a[3] | o.a[4] | o.a[5] | o.a[6]) return Fail(why, "copy %u op %u: bytes 4..7 are not 0", index, k);
            break;
        default:
            return Fail(why, "copy %u op %u: kind %u (1..5)", index, k, o.op);
        }
    }
    for (unsigned code = 1; code <= g.codes; ++code) {
        const Code& m = g.code[code - 1];
        const unsigned top = m.slot + ((m.flags & 1) ? last_page : 0);
        if (top >= c.slots) return Fail(why, "copy %u: code %u's label slot %u past the slots", index, code, top);
        for (unsigned page = g.first; page <= ((m.flags & 1) ? last_page : g.first); ++page)
            if (c.label[m.slot + ((m.flags & 1) ? page : 0)][0] == 0)
                return Fail(why, "copy %u: code %u's label slot is empty", index, code);
    }
    return true;
}

}  // namespace

bool ParseChunk(const std::uint8_t* p, std::uint32_t size, Grid& out, std::string& why) {
    std::memset(&out, 0, sizeof out);
    Reader r{p, size, 0, &why};
    if (!r.Need(kHeaderSize, "the header")) return false;
    if (p[0] != kVersion) return Fail(why, "version %u (1)", p[0]);
    out.family = p[1];
    out.pages = p[2];
    out.first = p[3];
    out.codes = p[4];
    if (out.family > 1) return Fail(why, "family %u (0, 1)", out.family);
    if (out.pages < 1 || out.pages > kMaxPages || out.first + out.pages > kMaxPages)
        return Fail(why, "pages %u from %u (F + P <= 3)", out.pages, out.first);
    if (out.family == static_cast<unsigned char>(Family::kWest) && out.pages != 1)
        return Fail(why, "a Western grid of %u pages (1)", out.pages);
    if (out.codes < 1 || out.codes > kMaxCodes) return Fail(why, "%u special codes (1..5)", out.codes);
    if (p[5] | p[6] | p[7]) return Fail(why, "header bytes 5..7 are not 0");
    bool ended = false;
    for (unsigned i = 0; i < 12; ++i) {
        const unsigned char ch = p[8 + i];
        if (ch == 0) ended = true;
        else if (ended || ch < 0x20 || ch > 0x7E) return Fail(why, "the build id's byte %u", i);
        out.build[i] = static_cast<char>(ch);
    }
    r.at = kHeaderSize;

    if (!r.Need(static_cast<std::uint32_t>(out.pages) * kPageCells * kCellSize, "the grid")) return false;
    for (unsigned pg = 0; pg < out.pages; ++pg)
        for (unsigned i = 0; i < kPageCells; ++i) {
            const std::uint8_t* c = p + r.at + (pg * kPageCells + i) * kCellSize;
            out.cell[pg][i] = Cell{c[0], c[1], c[2], c[3]};
        }
    r.at += static_cast<std::uint32_t>(out.pages) * kPageCells * kCellSize;

    if (!r.Need(static_cast<std::uint32_t>(out.codes) * kCodeSize, "the code map")) return false;
    unsigned seen[6] = {};
    for (unsigned k = 0; k < out.codes; ++k) {
        const std::uint8_t* c = p + r.at + k * kCodeSize;
        out.code[k] = Code{c[0], c[1], c[2], c[3]};
        if (c[0] < 1 || c[0] > 5) return Fail(why, "code %u: action %u (1..5)", k + 1, c[0]);
        if (c[2] & ~1u) return Fail(why, "code %u: flags 0x%X", k + 1, c[2]);
        ++seen[c[0]];
    }
    r.at += static_cast<std::uint32_t>(out.codes) * kCodeSize;
    for (unsigned a : {1u, 2u, 4u, 5u})
        if (seen[a] != 1) return Fail(why, "action %u is in the map %u times (once)", a, seen[a]);
    if (seen[3] != (out.pages > 1 ? 1u : 0u)) return Fail(why, "the page action %u times with %u pages", seen[3], out.pages);

    for (unsigned k = 0; k < 2; ++k)
        if (!ParseCopy(r, out.copy[k], k, why)) return false;
    if (r.at != size) return Fail(why, "%u bytes after the COMMU02 legend", size - r.at);
    if (out.copy[0].slots != out.copy[1].slots) return Fail(why, "the copies have %u and %u label slots", out.copy[0].slots, out.copy[1].slots);

    // The grid as a whole.
    for (unsigned pg = 0; pg < out.pages; ++pg) {
        unsigned rows_of[6] = {};
        for (unsigned row = 0; row < kRows; ++row)
            for (unsigned col = 0; col < kColumns; ++col) {
                const Cell& c = out.cell[pg][row * kColumns + col];
                const bool special_place = row >= 3 && col >= 10;
                switch (c.kind) {
                case kEmpty:
                    if (c.code | c.b0 | c.b1) return Fail(why, "page %u cell %u: an empty cell with bytes", pg, row * kColumns + col);
                    if (row == 0) return Fail(why, "page %u: row 0 column %u is empty", pg, col);
                    break;
                case kOneByte:
                case kTwoBytes:
                    if (special_place) return Fail(why, "page %u row %u column %u: a glyph where a special goes", pg, row, col);
                    if (c.code < 6) return Fail(why, "page %u cell %u: a glyph of code %u", pg, row * kColumns + col, c.code);
                    if (c.kind == kOneByte && (c.b0 == 0 || c.b0 >= 0x80 || c.b1 != 0))
                        return Fail(why, "page %u cell %u: one byte 0x%02X", pg, row * kColumns + col, c.b0);
                    if (c.kind == kTwoBytes && c.b0 < 0x80)
                        return Fail(why, "page %u cell %u: two bytes from 0x%02X", pg, row * kColumns + col, c.b0);
                    break;
                case kSpecial:
                    if (!special_place) return Fail(why, "page %u row %u column %u: a special off columns 10..11 of rows 3..7", pg, row, col);
                    if (c.code < 1 || c.code > out.codes) return Fail(why, "page %u: special code %u (1..C)", pg, c.code);
                    if (c.b0 | c.b1) return Fail(why, "page %u cell %u: a special with bytes", pg, row * kColumns + col);
                    if (col == 10) ++rows_of[c.code];
                    break;
                default:
                    return Fail(why, "page %u cell %u: kind %u", pg, row * kColumns + col, c.kind);
                }
                if (col == 11 && special_place) {
                    const Cell& left = out.cell[pg][row * kColumns + 10];
                    if (std::memcmp(&left, &c, sizeof c) != 0) return Fail(why, "page %u row %u: columns 10 and 11 differ", pg, row);
                }
            }
        for (unsigned code = 1; code <= out.codes; ++code)
            if (rows_of[code] != 1) return Fail(why, "page %u holds code %u in %u rows (1)", pg, code, rows_of[code]);
    }
    // One code, one encoding; two codes, two encodings; the space.
    for (unsigned pg = 0; pg < out.pages; ++pg)
        for (unsigned i = 0; i < kPageCells; ++i) {
            const Cell& c = out.cell[pg][i];
            if (c.kind != kOneByte && c.kind != kTwoBytes) continue;
            const unsigned len = c.kind == kOneByte ? 1u : 2u;
            if (out.enc_len[c.code] == 0) {
                out.enc_len[c.code] = static_cast<unsigned char>(len);
                out.enc[c.code][0] = c.b0;
                out.enc[c.code][1] = c.b1;
            } else if (out.enc_len[c.code] != len || out.enc[c.code][0] != c.b0 || out.enc[c.code][1] != c.b1) {
                return Fail(why, "code 0x%02X has two encodings", c.code);
            }
        }
    for (unsigned a = 0; a < 256; ++a)
        for (unsigned b = a + 1; b < 256; ++b)
            if (out.enc_len[a] && out.enc_len[a] == out.enc_len[b] && out.enc[a][0] == out.enc[b][0] && out.enc[a][1] == out.enc[b][1])
                return Fail(why, "codes 0x%02X and 0x%02X share an encoding", a, b);
    if (out.enc_len[0xFF] == 0) return Fail(why, "no glyph of code 0xFF (the space)");

    for (unsigned k = 0; k < 2; ++k)
        if (!CheckCopy(out, out.copy[k], k, why)) return false;
    return true;
}

// --- the keys --------------------------------------------------------------------

namespace {

// The family's bits (M2's table, M6's; section 5a "constants of the routine").
struct Bits { std::uint16_t confirm, page, next, back, erase, finish; };
constexpr Bits kJapanBits{0x20, 0x10, 0x80, 0, 0x40, 0x800};
constexpr Bits kWestBits{0x40, 0, 0x20, 0x80, 0x10, 0x800};
constexpr std::uint16_t kRepeatNext = 0xA, kRepeatBack = 0x5;

std::uint16_t MapMask(std::uint16_t (*map)(std::uint16_t), std::uint16_t mask) {
    std::uint16_t out = 0;
    for (unsigned b = 0; b < 16; ++b)
        if (mask & (1u << b)) out = static_cast<std::uint16_t>(out | map(static_cast<std::uint16_t>(1u << b)));
    return out;
}

}  // namespace

Keys KeysOf(Family family, std::uint16_t pressed, std::uint16_t repeated, std::uint16_t (*map)(std::uint16_t)) {
    const Bits& b = family == Family::kJapan ? kJapanBits : kWestBits;
    const auto on = [&](std::uint16_t word, std::uint16_t bits) { return bits != 0 && (word & MapMask(map, bits)) != 0; };
    Keys k{};
    k.up = on(repeated, 0x1000);
    k.down = on(repeated, 0x4000);
    k.left = on(repeated, 0x8000);
    k.right = on(repeated, 0x2000);
    k.confirm = on(pressed, b.confirm);
    k.page = on(pressed, b.page);
    k.next = on(pressed, b.next) || on(repeated, kRepeatNext);
    k.back = on(pressed, b.back) || on(repeated, kRepeatBack);
    k.erase = on(pressed, b.erase);
    k.finish = on(pressed, b.finish);
    return k;
}

// --- the step (research 4.4, M1, M2; the US START step read whole) ------------------

namespace {

const Cell& At(const Grid& g, unsigned page, unsigned row, unsigned col) {
    return g.cell[page - g.first][row * kColumns + col];
}
bool Empty(const Grid& g, const Cells& c) { return At(g, c.page, c.row, c.column).kind == kEmpty; }

// "Up to the first cell that is not empty": Japan's page-2 constants (row 4..7
// of columns 0..9 to row 3 after a move; after a page change also row 2 of
// columns 6..9 to row 1) are this rule on Japan's page 2. Row 0 is never
// empty (section 5a), so it ends.
// ours: the West's step has no clamp, and its Left from a special sets column
// 9 with no test (US START 0x801E48FC..0x801E491C), so on the US / EU row 5
// and the French row 5 the original hand lands on an empty cell; the owner
// observed it never does (call 7), and ours goes up from it as Japan's does.
void ClampUp(const Grid& g, Cells& c) {
    while (Empty(g, c)) {
        if (c.row == 0) bof3::Fatal("name grid: column %u of page %u is empty to row 0", c.column, c.page);
        --c.row;
    }
}

void Trim(unsigned char* buffer) {
    for (int i = 4; i >= 0 && buffer[i] == 0xFF; --i) buffer[i] = 0;
}

}  // namespace

int EncodedLength(const Grid& g, const unsigned char cells[5]) {
    unsigned n = 0;
    while (n < 5 && cells[n] != 0) ++n;
    while (n > 0 && cells[n - 1] == 0xFF) --n;
    int bytes = 0;
    for (unsigned i = 0; i < n; ++i) {
        const unsigned len = g.enc_len[cells[i]];
        if (len == 0) return -1;
        bytes += static_cast<int>(len);
    }
    return bytes;
}

unsigned Step(const Grid& g, Copy copy, Cells& c, const Keys& k, unsigned room, unsigned cell_count,
              const unsigned char* fallback, void (*play)(unsigned short)) {
    if (c.page < g.first || c.page >= g.first + g.pages || c.row >= kRows || c.column >= kColumns)
        bof3::Fatal("name grid: the hand at page %u row %u column %u, off the grid", c.page, c.row, c.column);
    if (cell_count < 1 || cell_count > 5 || c.cursor >= cell_count)
        bof3::Fatal("name grid: name cursor %u of %u cells", c.cursor, cell_count);
    if (Empty(g, c)) bof3::Fatal("name grid: the hand is on an empty cell (page %u row %u column %u)", c.page, c.row, c.column);
    const unsigned last = cell_count - 1;
    const bool new_game = copy == Copy::kNewGame;
    unsigned result = kNothing;

    // 1. The hand: one direction a frame, Up first (both families).
    const bool special_here = At(g, c.page, c.row, c.column).kind == kSpecial;
    if (k.up) {
        play(0x100);
        do c.row = static_cast<unsigned char>(c.row == 0 ? kRows - 1 : c.row - 1); while (Empty(g, c));
    } else if (k.down) {
        play(0x100);
        do c.row = static_cast<unsigned char>(c.row + 1 >= kRows ? 0 : c.row + 1); while (Empty(g, c));
    } else if (k.left) {
        play(0x100);
        if (special_here) c.column = 9;
        else do c.column = static_cast<unsigned char>(c.column == 0 ? kColumns - 1 : c.column - 1); while (Empty(g, c));
    } else if (k.right) {
        play(0x100);
        if (special_here) c.column = 0;
        else do c.column = static_cast<unsigned char>(c.column + 1 >= kColumns ? 0 : c.column + 1); while (Empty(g, c));
    }
    ClampUp(g, c);

    // 2. The cell under the hand. A page or a finish code is the original's
    // write of the bit into the pad word, kept here as a flag: the branch
    // below takes it in the same frame, and nothing else reads the word.
    bool page = k.page, finish = k.finish;
    if (k.confirm) {
        const Cell& cell = At(g, c.page, c.row, c.column);
        if (cell.kind == kSpecial) {
            switch (g.code[cell.code - 1].action) {
            case kNext:
                if (c.cursor < last) { play(0x103); ++c.cursor; } else play(0x107);
                break;
            case kBack:
                if (c.cursor > 0) { play(0x103); --c.cursor; } else play(0x107);
                break;
            case kPage:
                page = true;
                break;
            case kQuitCode:
                if (!new_game) return kQuit;   // COMMU02: the step ends unanswered, no sound
                play(0x103);                   // START: back to the title (the caller's), the step goes on
                result |= kQuit;
                break;
            case kFinish:
                finish = true;
                break;
            default:
                bof3::Fatal("name grid: code %u has action %u", cell.code, g.code[cell.code - 1].action);
            }
        } else {
            unsigned char trial[5];
            std::memcpy(trial, c.buffer, 5);
            trial[c.cursor] = cell.code;
            const int bytes = EncodedLength(g, trial);
            if (bytes < 0 || static_cast<unsigned>(bytes) > room) {
                // ours: the field's byte room (M3: a Latin name of more than
                // three two-byte cells, a kana name in a faerie slot) - the
                // limit codes' sound, and nothing entered.
                play(0x107);
            } else {
                if (new_game) c.buffer[5] = 0;
                play(0x103);
                c.buffer[c.cursor] = cell.code;
                if (c.cursor < last) ++c.cursor;
            }
        }
    }

    // 3. Exactly one of these, in this order (Japan 0x801E4AB8.., West 0x801E4BFC..).
    if (page && g.pages > 1) {
        play(0x103);
        c.page = static_cast<unsigned char>(g.first + (c.page - g.first + 1u) % g.pages);
        ClampUp(g, c);
    } else if (k.next) {
        if (c.cursor < last) { play(0x101); ++c.cursor; }
    } else if (k.back) {
        if (c.cursor > 0) { play(0x101); --c.cursor; }
    } else if (k.erase) {
        if (new_game) c.buffer[5] = 0;
        if (c.cursor != 0 && (c.buffer[c.cursor] == 0xFF || c.buffer[c.cursor] == 0)) --c.cursor;
        play(0x106);
        c.buffer[c.cursor] = 0xFF;
    } else if (finish) {
        play(new_game ? 0x105 : 0x104);
        unsigned kept = 5;
        while (kept > 0 && c.buffer[kept - 1] == 0xFF) --kept;
        if (new_game && kept == 0) {
            if (fallback == nullptr) bof3::Fatal("name grid: New Game's all-blank finish without a default name");
            std::memcpy(c.buffer, fallback, 5);   // START: record 0's name, as the PlayStation copies it
        } else if (kept < 5) {
            Trim(c.buffer);                       // COMMU02: an all-blank name is five 0 bytes
        }
        result |= kFinished;
    }
    return result;
}

// --- encode and decode ---------------------------------------------------------------------

unsigned Encode(const Grid& g, const unsigned char cells[5], unsigned char* out, unsigned room) {
    const int bytes = EncodedLength(g, cells);
    if (bytes < 0) bof3::Fatal("name grid: a name cell's code is on no glyph cell of the grid");
    if (static_cast<unsigned>(bytes) > room) bof3::Fatal("name grid: a name of %d bytes for a field of %u", bytes, room);
    unsigned at = 0, n = 0;
    while (n < 5 && cells[n] != 0) ++n;
    while (n > 0 && cells[n - 1] == 0xFF) --n;
    for (unsigned i = 0; i < n; ++i)
        for (unsigned b = 0; b < g.enc_len[cells[i]]; ++b) out[at++] = g.enc[cells[i]][b];
    for (unsigned i = at; i < room; ++i) out[i] = 0;
    return at;
}

namespace {

int CodeOf(const Grid& g, unsigned len, unsigned char b0, unsigned char b1) {
    for (unsigned code = 1; code < 256; ++code)
        if (g.enc_len[code] == len && g.enc[code][0] == b0 && (len == 1 || g.enc[code][1] == b1)) return static_cast<int>(code);
    return -1;
}

}  // namespace

bool Decode(const Grid& g, const unsigned char* stored, unsigned room, unsigned char out[5],
            bool (*pair_of)(std::uint32_t, std::uint32_t*, std::uint32_t*)) {
    unsigned char cells[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    unsigned n = 0, i = 0;
    const auto put = [&](int code) {
        if (code < 0 || n >= 5) return false;
        cells[n++] = static_cast<unsigned char>(code);
        return true;
    };
    while (i < room && stored[i] != 0) {
        if (stored[i] & 0x80) {
            if (i + 1 >= room) return false;
            const std::uint32_t glyph = ((stored[i] & 0x7Fu) << 8) | stored[i + 1];
            std::uint32_t a = 0, b = 0;
            if (pair_of != nullptr && pair_of(glyph, &a, &b)) {
                if (!put(CodeOf(g, 2, static_cast<unsigned char>(0x80 | (a >> 8)), static_cast<unsigned char>(a)))) return false;
                if (!put(CodeOf(g, 2, static_cast<unsigned char>(0x80 | (b >> 8)), static_cast<unsigned char>(b)))) return false;
            } else if (!put(CodeOf(g, 2, stored[i], stored[i + 1]))) {
                return false;
            }
            i += 2;
        } else {
            if (!put(CodeOf(g, 1, stored[i], 0))) return false;
            ++i;
        }
    }
    std::memcpy(out, cells, 5);
    return true;
}

// --- the draws (research 4.1 and 4.2; the US and Japan START drawers) ------------------

void DrawGrid(const Grid& g, Copy copy, const Cells& c, int x, int y, const Painter& paint) {
    const CopyData& d = g.copy[static_cast<unsigned>(copy)];
    paint.box(x, y, 0x110, 0x7B);
    static const unsigned char kNul[1] = {0};
    for (unsigned row = 0; row < kRows; ++row) {
        const int cy = y + 6 + 14 * static_cast<int>(row);
        int cx = x + 4;
        for (unsigned col = 0; col < kColumns; ++col) {
            const Cell& cell = At(g, c.page, row, col);
            if (row >= 3 && col >= 10) {
                if (col == 10 && cell.kind == kSpecial) {
                    const Code& m = g.code[cell.code - 1];
                    const unsigned slot = m.slot + ((m.flags & 1) ? c.page : 0u);
                    paint.text(cx, cy, m.colour, d.count, d.label[slot]);
                }
            } else if (cell.kind == kEmpty) {
                paint.text(cx, cy, 0, 1, kNul);   // the original draws the 0 byte: nothing
            } else {
                const unsigned char s[3] = {cell.b0, cell.kind == kTwoBytes ? cell.b1 : static_cast<unsigned char>(0), 0};
                paint.text(cx, cy, 0, 1, s);
            }
            cx += (col == 4 || col == 9) ? 0x1C : 0x14;
        }
    }
    int hx = x + 6 + 20 * c.column;
    if (c.column >= 5) hx += 8;
    if (c.column >= 10) hx += 8;
    if (c.column == 11 && c.row >= 3) hx -= 0x14;
    paint.hand(hx, y + 8 + 14 * c.row);
}

void DrawLegend(const Grid& g, Copy copy, unsigned page, int x, int y, const Painter& paint) {
    const CopyData& d = g.copy[static_cast<unsigned>(copy)];
    if (copy == Copy::kNewGame) paint.mode(1, 0xF);   // every START legend's first draw mode, before its box
    paint.box(x + 3, y + 3, d.box_w, d.box_h);
    for (unsigned k = 0; k < d.ops; ++k) {
        const Op& o = d.op[k];
        const int dx = o.a[0], dy = o.a[1];
        switch (o.op) {
        case kText: {
            const unsigned e = o.a[2] + ((o.a[3] & 1) ? page : 0u);
            if (e >= d.entries) bof3::Fatal("name grid: legend entry %u on page %u", e, page);
            paint.small(x + dx, y + dy, o.a[5], o.a[4], d.entry[e]);
            break;
        }
        case kLeaders:
            for (unsigned i = 0; i < o.a[3]; ++i)
                for (unsigned j = 0; j < o.a[4]; ++j)
                    paint.small(x + dx + o.a[5] * static_cast<int>(j), y + dy + o.a[6] * static_cast<int>(i), 0, 1, d.entry[o.a[2]]);
            break;
        case kTile:
            paint.tile(x + dx, y + dy, o.a[2], o.a[3], static_cast<unsigned>(o.a[4] | (o.a[5] << 8)));
            break;
        case kPiece:
            paint.piece(x + dx, y + dy, o.a[2], o.a[3], static_cast<unsigned>(o.a[4] | (o.a[5] << 8)));
            break;
        case kMode:
            paint.mode(o.a[0], static_cast<unsigned>(o.a[1] | (o.a[2] << 8)));
            break;
        default:
            bof3::Fatal("name grid: legend operation kind %u", o.op);
        }
    }
    paint.frame(x, y, copy);
}

}  // namespace detail

// --- the live unit -------------------------------------------------------------------

namespace {

using detail::Grid;

Grid g_grid;
bool g_taken = false;
bool g_available = false;
std::string g_why = "no kind-22 chunk taken";

// The legend frames' piece lists in the PC's .data (section 5a): each disc's
// list, byte for byte, measured in BOF3.exe at these addresses (53 and 34
// three-byte pieces and the 0xFF end), and their SHA-256 there.
constexpr ::bof3::image::Va kStartFrame{0x667148};    // START's legend frame, a Menu_DrawPieces list
constexpr ::bof3::image::Va kCommuFrame{0x653000};    // COMMU02's
struct FrameList { ::bof3::image::Va at; unsigned pieces; const char* sha256; };
constexpr FrameList kFrames[2] = {
    {kStartFrame, 53, "02738f54768493ee070d9e38c53d8bbc74f96e5bc6ce0609860e352c24354e51"},
    {kCommuFrame, 34, "f99b328028c50ec61b7f7fc36b11daeeda7faa8e6468d8aa0aabb05a3524cf94"},
};

bool FramesHeld(std::string& why) {
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned char* const list = move_script::At(::bof3::Img(kFrames[k].at.v));
        const unsigned bytes = 3 * kFrames[k].pieces + 3;
        std::string hex;
        bof3x::Sha256HexBytes(list, bytes, hex);
        if (list[bytes - 1] != 0xFF || hex != kFrames[k].sha256) {
            char line[160];
            std::snprintf(line, sizeof line, "the running image does not hold %s legend frame at 0x%X (a disc-built or exe-less image)",
                          k == 0 ? "START's" : "COMMU02's", static_cast<unsigned>(kFrames[k].at.v));
            why = line;
            return false;
        }
    }
    return true;
}

const Grid& Live(const char* what) {
    if (!g_available) bof3::Fatal("name grid: %s with no grid (%s)", what, g_why.c_str());
    return g_grid;
}

void PlayLive(unsigned short id) { SH_CALL(Sound_PlayEffect)(id); }

std::uint16_t SigmaJapan(std::uint16_t bit) { return bof3x::input::sigma::Sigma(bit); }
std::uint16_t SigmaWest(std::uint16_t bit) { return bof3x::input::sigma::SigmaFrom(bof3x::input::sigma::Release::kWest, bit); }

// The game's side of the draws.
void PaintCommit(unsigned size) { SH_CALL(Gfx_CommitPrim)(1, size); }
void PaintBox(int x, int y, int w, int h) { SH_CALL(Menu_DrawBox)(x, y, w, h, 0, static_cast<unsigned char>(Config_WindowColour)); }
void PaintText(int x, int y, int colour, int count, const unsigned char* s) { SH_CALL(Text_DrawAt)(x, y, colour, count, s); }
void PaintSmall(int x, int y, int colour, int count, const unsigned char* s) {
    SH_CALL(Text_DrawSmall)(x, y, static_cast<unsigned>(colour), static_cast<unsigned>(count), s);
}
void PaintHand(int x, int y) { SH_CALL(Menu_DrawHand)(x, y, 0); }
void PaintMode(unsigned dtd, unsigned tpage) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, static_cast<int>(dtd), tpage, 0);
    PaintCommit(0xC);
}
template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }
void PaintSprite(int x, int y, unsigned u, unsigned v, unsigned w, unsigned h, unsigned clut, unsigned char shade) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(p);
    p[4] = p[5] = p[6] = shade;
    Put<float>(p, 8, static_cast<float>(static_cast<short>(x)));
    Put<float>(p, 0xC, static_cast<float>(static_cast<short>(y)));
    p[0x14] = static_cast<unsigned char>(u);
    p[0x15] = static_cast<unsigned char>(v);
    Put<std::uint16_t>(p, 0x16, static_cast<std::uint16_t>(clut));
    Put<std::uint16_t>(p, 0x18, static_cast<std::uint16_t>(w));
    Put<std::uint16_t>(p, 0x1A, static_cast<std::uint16_t>(h));
    PaintCommit(0x1C);
}
// The PlayStation's helper (Japan COMMU02 0x801D9B8C): an 8 x 8 sprite, shade 0x7F.
void PaintTile(int x, int y, unsigned u, unsigned v, unsigned clut) { PaintSprite(x, y, u * 8, v * 8, 8, 8, clut, 0x7F); }
void PaintPiece(int x, int y, unsigned id, unsigned flags, unsigned clut) {
    const unsigned char* const rect = SH_CALL(Menu_PieceRect)(id, flags);
    PaintSprite(x, y, rect[0], rect[1], rect[2], rect[3], clut, 0x80);
}
void PaintFrame(int x, int y, Copy copy) {
    SH_CALL(Menu_DrawPieces)(x, y, move_script::At(::bof3::Img(kFrames[static_cast<unsigned>(copy)].at.v)), 0);
}
constexpr detail::Painter kGame{PaintBox, PaintText, PaintSmall, PaintHand, PaintMode, PaintTile, PaintPiece, PaintFrame};

}  // namespace

void NameGrid_Take(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (tag != 0) bof3::Fatal("name grid: FIRST.DAT kind 22 tag %u; this build knows tag 0 (docs/new-game-round.md 5a)", static_cast<unsigned>(tag));
    std::string why;
    if (!detail::ParseChunk(payload, size, g_grid, why))
        bof3::Fatal("name grid: FIRST.DAT kind 22 (%u bytes) breaks section 5a: %s", static_cast<unsigned>(size), why.c_str());
    const bool first = !g_taken;
    g_taken = true;
    if (!FramesHeld(why)) {
        if (g_available || first) bof3::Log("name grid: the %s grid is not used: %s", g_grid.build, why.c_str());
        g_available = false;
        g_why = why;
        return;
    }
    if (first) bof3::Log("name grid: the %s grid taken (%s, %u page(s) from %u)", g_grid.build,
                         g_grid.family == 0 ? "Japan" : "West", g_grid.pages, g_grid.first);
    g_available = true;
    g_why.clear();
}

bool NameGrid_Available() { return g_available; }

const char* NameGrid_Why() { return g_why.c_str(); }

Family NameGrid_Family() { return static_cast<Family>(Live("NameGrid_Family").family); }

unsigned NameGrid_CellCount() { return Live("NameGrid_CellCount").family == 0 ? 4u : 5u; }

unsigned NameGrid_FirstPage() { return Live("NameGrid_FirstPage").first; }

void NameGrid_Draw(Copy copy, const Cells& cells, int x, int y) { detail::DrawGrid(Live("NameGrid_Draw"), copy, cells, x, y, kGame); }

void NameGrid_DrawLegend(Copy copy, unsigned page, int x, int y) {
    detail::DrawLegend(Live("NameGrid_DrawLegend"), copy, page, x, y, kGame);
}

unsigned NameGrid_Input(Copy copy, Cells& cells, unsigned room, const unsigned char* fallback) {
    const Grid& g = Live("NameGrid_Input");
    const Family family = static_cast<Family>(g.family);
    std::uint16_t (*const map)(std::uint16_t) = family == Family::kJapan ? SigmaJapan : SigmaWest;
    const auto pressed = static_cast<std::uint16_t>(Input_Pressed);
    std::uint16_t mask = 0;
    for (unsigned b = 0; b < 16; ++b)
        if (detail::kRepeatMask & (1u << b)) mask = static_cast<std::uint16_t>(mask | map(static_cast<std::uint16_t>(1u << b)));
    const auto repeated = static_cast<std::uint16_t>(SH_CALL(Input_AutoRepeat)(pressed & mask) & 0xFFFF);
    const detail::Keys keys = detail::KeysOf(family, pressed, repeated, map);
    return detail::Step(g, copy, cells, keys, room, family == Family::kJapan ? 4u : 5u, fallback, PlayLive);
}

unsigned NameGrid_Encode(const unsigned char cells[5], unsigned char* out, unsigned room) {
    return detail::Encode(Live("NameGrid_Encode"), cells, out, room);
}

bool NameGrid_Decode(const unsigned char* stored, unsigned room, unsigned char out[5]) {
    return detail::Decode(Live("NameGrid_Decode"), stored, room, out, TextPair_Of);
}

}  // namespace name_grid

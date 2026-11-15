#pragma once

// The name entry's grid, ours (docs/new-game-round.md, group NG; the research
// is docs/name-entry-restoration.md section 4). One body for both of the
// PlayStation's copies of the routine - START.EMI's (New Game, character
// record 0) and COMMU02.EMI's (the faerie village's renamer) - over a cell
// block the caller owns, fed by the kind-22 chunk of the language layer's
// FIRST.DAT (the round doc's section 5a, the chunk's layout; that section and
// ParseChunk in name_grid.cpp are the two places that know it).
//
// Nothing here is wired to a caller yet (stage B: TF, CR, BT). No fallback
// grid exists in the source, in any language: with no chunk that passes,
// NameGrid_Available() is false and every draw and input call is a Fatal
// (rule 4).
//
// The cells hold the disc's codes, as the PlayStation's buffer does: 0xFF a
// blank, 0 the end once a name is finished. The layer's bytes are made from
// them only by NameGrid_Encode and read back only by NameGrid_Decode.

#include <cstdint>
#include <string>

namespace name_grid {

// Which of the PlayStation's two copies a caller is (M1's five differences).
enum class Copy : unsigned char {
    kNewGame,   // START: draws first (the caller's), quit = back to the title with 0x103 and the
                // step goes on, finish 0x105, an all-blank name takes the default, buffer byte 5
                // cleared on every glyph and backspace
    kRenamer,   // COMMU02: quit ends the step at once with no sound, finish 0x104, an all-blank
                // name becomes five 0 bytes
};

// The grid's family, the chunk's header byte 1.
enum class Family : unsigned char { kJapan = 0, kWest = 1 };

// The caller's cells: the PlayStation's buffer, name cursor, column, row and
// page (research section 3's pairing; START's own at 0x801ED070..0x801ED768).
struct Cells {
    unsigned char buffer[6];   // five cells and the byte START clears
    unsigned char cursor;      // the name cursor, 0 .. NameGrid_CellCount() - 1
    unsigned char column;      // 0..11
    unsigned char row;         // 0..7
    unsigned char page;        // the disc's page number (a Western page is 2)
};

// What an input step did (bits). kQuit and kFinished can come in one frame
// for kNewGame, as on the PlayStation, whose quit does not leave the step.
enum Result : unsigned { kNothing = 0, kFinished = 1, kQuit = 2 };

// --- the chunk ---------------------------------------------------------------

// dat_load's kind 22 (any tag but 0 is a Fatal): parses the payload into our
// copy, replacing what it held. A chunk that breaks a rule of section 5a is a
// Fatal naming the rule (our builder wrote it). One whose rules hold but whose
// image lacks the legend frames' piece lists (0x667148 START's, 0x653000
// COMMU02's, each checked by its length and SHA-256) leaves the grid
// unavailable with the reason logged once: a disc-built or exe-less image.
void NameGrid_Take(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// True only with a taken chunk whose shape passes, on an image that has the
// frames' lists.
bool NameGrid_Available();

// Why NameGrid_Available() is false ("" when it is true).
const char* NameGrid_Why();

// The loaded grid's family. A Fatal without a grid.
Family NameGrid_Family();

// The cells a name may have: 4 for the Japan family (the owner's call 5 of
// the round doc's section 3b: the ja-JP field holds four kana without a
// pair code), else 5. A Fatal without a grid.
unsigned NameGrid_CellCount();

// The first page in use: the page an entry opens on (the Western open step's
// constant 2; Japan's 0). A Fatal without a grid.
unsigned NameGrid_FirstPage();

// --- the routine (the PlayStation's, research 4.1, 4.2, 4.4) ---------------------

// The grid box at (x, y): Menu_DrawBox 0x110 x 0x7B, the cells and the
// special cells' labels of `copy`, the hand at the cell block's column and
// row. The PlayStation's x, y: (0x18, 0x67).
void NameGrid_Draw(Copy copy, const Cells& cells, int x, int y);

// The legend at (x, y) for page `page`: `copy`'s box, its draw operations
// (the chunk's), then its frame. START's (0xA8, 0x2B). Each button tile
// pictures the button the input step tests for its row under the Config now
// (the family's map, as NameGrid_Input), not the disc's fixed tile; a tile or
// the piece after a text is drawn on the page the PlayStation's small draw
// leaves (0x2F). Group BT, docs/new-game-round.md 4c-BT.
void NameGrid_DrawLegend(Copy copy, unsigned page, int x, int y);

// One frame of the input step on the live pad words (Input_Pressed and
// Input_AutoRepeat's low half, each face and shoulder test through sigma:
// Sigma for the Japan family's bits, SigmaFrom(West) for the West's). `room`
// is the field's bytes (8 a record's name, 5 a faerie slot): a glyph whose
// entry would pass it is refused with 0x107. `fallback` is kNewGame's
// all-blank default, five cells (NameGrid_Decode of record 0's name);
// nullptr for kRenamer. Plays the routine's sounds. Returns Result bits.
unsigned NameGrid_Input(Copy copy, Cells& cells, unsigned room, const unsigned char* fallback);

// --- the commit side ---------------------------------------------------------------

// The cells as the layer stores them: each cell up to the end (a 0, or the
// last cell that is not 0xFF) encoded, a 0xFF inside the name as the space's
// encoding; then NUL-filled to `room` bytes when `room` > the length. Returns
// the bytes written before the fill. A name that does not fit `room`, or a
// code that is not on the grid, is a Fatal (the input step refuses those).
unsigned NameGrid_Encode(const unsigned char cells[5], unsigned char* out, unsigned room);

// The stored name (at most `room` bytes, to a NUL) back to five cells, 0xFF
// after the name: exact by section 5a's rule. A pair code (DIV-0057) is its
// two glyphs. False, with `out` untouched, when a unit is on no cell of the
// grid or the name is longer than five cells - the caller decides.
bool NameGrid_Decode(const unsigned char* stored, unsigned room, unsigned char out[5]);

// --- for the self-test (name_grid_fuzz.cpp) ------------------------------------------

namespace detail {

constexpr unsigned kRows = 8, kColumns = 12, kPageCells = kRows * kColumns;
constexpr unsigned kMaxPages = 3, kMaxCodes = 5, kMaxSlots = 8, kMaxEntries = 8, kMaxOps = 32, kMaxRoom = 32;

struct Cell { unsigned char code, kind, b0, b1; };            // kind: 0 empty, 1 one byte, 2 two bytes, 3 special
struct Code { unsigned char action, slot, flags, colour; };   // action: 1 next, 2 back, 3 page, 4 quit, 5 finish
struct Op { unsigned char op, a[7]; };
struct CopyData {
    unsigned char slots, count, room;
    unsigned char label[kMaxSlots][kMaxRoom];
    unsigned char box_w, box_h, entries, entry_room, ops;
    unsigned char entry[kMaxEntries][kMaxRoom];
    Op op[kMaxOps];
};
struct Grid {
    unsigned char family, pages, first, codes;
    char build[13];
    Cell cell[kMaxPages][kPageCells];
    Code code[kMaxCodes];
    CopyData copy[2];   // Copy::kNewGame, Copy::kRenamer
    // Built by ParseChunk from the cells: each glyph code's encoding (0 bytes:
    // the code is on no glyph cell).
    unsigned char enc_len[256];
    unsigned char enc[256][2];
};

enum Action : unsigned char { kNext = 1, kBack = 2, kPage = 3, kQuitCode = 4, kFinish = 5 };

// The chunk's payload into `out`; false with `why` naming the first rule
// broken.
bool ParseChunk(const std::uint8_t* p, std::uint32_t size, Grid& out, std::string& why);

// The keys of one frame in the family's own terms.
struct Keys {
    bool up, down, left, right;   // the repeated word's hand bits (Up first, as the original)
    bool confirm, page, next, back, erase, finish;   // pressed (next / back: pressed or repeated)
};
// From the pad words: `pressed`, `repeated` (Input_AutoRepeat's low half) and
// a bit map (the family's bit -> the bit tested now).
Keys KeysOf(Family family, std::uint16_t pressed, std::uint16_t repeated, std::uint16_t (*map)(std::uint16_t));
// The family's mask for Input_AutoRepeat before the map: the hand and the shoulders.
constexpr std::uint16_t kRepeatMask = 0xF00F;

// One input step on `grid`; each sound through `play`. Pure but for `play`.
unsigned Step(const Grid& grid, Copy copy, Cells& cells, const Keys& keys, unsigned room, unsigned cell_count,
              const unsigned char* fallback, void (*play)(unsigned short));

// The bytes the cells encode to (NameGrid_Encode's length), or -1 when a code
// is on no cell.
int EncodedLength(const Grid& grid, const unsigned char cells[5]);
unsigned Encode(const Grid& grid, const unsigned char cells[5], unsigned char* out, unsigned room);
bool Decode(const Grid& grid, const unsigned char* stored, unsigned room, unsigned char out[5],
            bool (*pair_of)(std::uint32_t, std::uint32_t*, std::uint32_t*));

// What the draws ask of the game, so the self-test can watch them.
struct Painter {
    void (*box)(int x, int y, int w, int h);
    void (*text)(int x, int y, int colour, int count, const unsigned char* s);      // Text_DrawAt
    void (*small)(int x, int y, int colour, int count, const unsigned char* s);     // Text_DrawSmall
    void (*hand)(int x, int y);
    void (*mode)(unsigned dtd, unsigned tpage);
    void (*tile)(int x, int y, unsigned u, unsigned v, unsigned clut);               // 8 x 8, shade 0x7F
    void (*piece)(int x, int y, unsigned id, unsigned flags, unsigned clut);         // shade 0x80
    void (*frame)(int x, int y, Copy copy);                                          // Menu_DrawPieces
};
void DrawGrid(const Grid& grid, Copy copy, const Cells& cells, int x, int y, const Painter& paint);
// `map`: the family's bit -> the button it is on now (NameGrid_Input's map);
// a button tile of the sheet is drawn as the tile of map(its bit).
void DrawLegend(const Grid& grid, Copy copy, unsigned page, int x, int y, const Painter& paint,
                std::uint16_t (*map)(std::uint16_t));

}  // namespace detail

}  // namespace name_grid

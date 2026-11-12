#pragma once

// The two members screens in the solo-character mode (group MS;
// docs/solo-character-screens.md, docs/solo-character-round.md 4 "MS", 6a
// item 1): the Tactics screen's members step (TacticsMembers_*, src/game/
// field/field_ability_2.cpp) and PartyForm (Field_Request 6, src/game/field/
// party_form.cpp).
//
// Both screens keep their reserve in a list of their own that holds three
// rows: Tactics' 0x6BDFD4.. is followed by the refused member 0x6BDFD7 and
// Config's saved cursor 0x6BDFD8; PartyForm's 0x6BC894.. by its count
// 0x6BC897 and its answer 0x6BC898. Each screen's cursor places (Tactics
// 0x6674C8, PartyForm 0x664078) hold three rows, and three rows of 0x34
// pixels from y 0x41 are what fits on the 240-line screen. A fourth reserve
// member is written over the byte after the list and has no place
// (docs/solo-character-screens.md 2). The mode's map team is the hero and one
// other, so six joined members leave four in reserve; this module is the
// reserve those screens use instead while it is on: every joined record not
// in the party, in a list of ours with room for all eight, of which the
// screen shows three rows at a time and scrolls.
//
// The game's three-row list keeps the rows shown (so the reserve's drawers,
// MenuList_ReserveWinDraw and PartyForm_DrawReserve, run unchanged), the
// game's reserve count byte holds the number of rows shown, and the cursor's
// and the pick's row bytes hold the row in the whole list.

#include "bof3/symbols.gen.h"

namespace solo_screens {

// Whether a members screen opened now takes the long-reserve path. Today: the
// mode is active (solo::Solo_Active()). The two latent defects are Capcom's
// and reachable without the mode only by a reserve of four or more, which
// the measured saves never hold with a team of three (round doc 6 open 2
// recommends the fixes for the mode only); to make the fix unconditional,
// Wide() returns true - one line in solo_screens.cpp.
bool Wide();

// The champion's mark: the mode active and the member id's record
// (Character_IndexById) the champion's. False whenever the mode is not active.
bool IsChampion(unsigned char id);

// How the champion's panel is marked (docs/solo-character-screens.md 6):
// kCleared, the look the load list gives a cleared save (Menu_DrawSaveSlot
// 0x576960 with summary +0x14 set: its frame pieces drawn with flags 2, the
// gold CLUT (0x80, 0x1E2) of Menu_DrawPiece, and the piece 0x3D "CLEAR");
// or kHand, the game's pointing hand, kept for the owner to compare.
enum class ChampionMark { kCleared, kHand };
constexpr ChampionMark kChampionMark = ChampionMark::kCleared;

// With kCleared: whether the piece 0x3D (the cleared slot's "CLEAR" emblem)
// is drawn too, as the slot draws it, or only the frame's gold palette. Off:
// on a member panel it covers the top of the HP line (solo_screens.cpp).
constexpr bool kChampionClearEmblem = false;

// With kHand: the hand's tip (Menu_DrawHand puts it at x) at the champion's
// cursor place plus this offset - inside the panel's top-left corner,
// pointing at the name.
constexpr int kChampionHandDx = 0x12;
constexpr int kChampionHandDy = 0;

// The cleared look on the champion's panel, drawn as a member status panel
// (Menu_DrawMemberStatus(x, y, record), its box at x + 3, y + 3): the frame
// that panel draws, Menu_MemberPieces 0x6632C8, again with flags 2 (the
// cleared slot's palette), then its EXP bar again (the panel's last draw,
// which the pieces would cover), and with kChampionClearEmblem the piece
// 0x3D. `whole`: the panel itself first - for a reserve row, which is a box
// and tiles with no piece frame, so the frame cannot be laid over it (its
// opaque name box and label pieces cover the row's level and HP: captured
// 2026-11-12); the champion's row is shown as a member panel instead. The
// mode only: callers ask IsChampion first, and draw it through InFront.
void DrawCleared(int x, int y, unsigned record, bool whole);

// `draw` with the menus' ordering-table slot 1 pointed at slot 0, which the
// GPU draws after slot 1 (measured: docs/solo-character-screens.md 6), so what
// it commits shows over everything the menus draw this frame, window records
// included; both slots' tails put back after (the save and restore the
// field's draw passes make of Gfx_OtPointers, draw_pass.cpp).
template <typename Draw> void InFront(Draw draw) {
    const auto saved = Gfx_OtPointers[1];
    Gfx_OtPointers[1] = Gfx_OtPointers[0];
    draw();
    Gfx_OtPointers[0] = Gfx_OtPointers[1];
    Gfx_OtPointers[1] = saved;
}

constexpr unsigned kMaxReserve = 8;  // CharacterRecords' eight records
constexpr unsigned kRows = 3;        // the rows a screen shows

struct Reserve {
    bool wide = false;                     // latched when the screen is built
    unsigned char ids[kMaxReserve] = {};   // the member ids, in record order
    unsigned char count = 0;               // how many
    unsigned char top = 0;                 // the first row shown
};

// The long reserve each screen keeps while open.
extern Reserve g_tactics;
extern Reserve g_party_form;

// Every record of the eight with +0xB bit 0 whose id (+9) is not among
// party[0 .. party_count), in record order: the screens' own rule
// (TacticsMembers_Build, PartyForm_Setup). top 0.
void Fill(Reserve& r, const unsigned char* party, unsigned party_count);

// The rows shown: count - top, at most kRows.
unsigned Shown(const Reserve& r);

// The rows shown, written to the game's list (kRows bytes; only Shown(r) of
// them are written).
void Window(const Reserve& r, unsigned char* list);

// The row in the whole list a shown row is, and back (-1: not shown).
int Whole(const Reserve& r, int shown);
int ShownRow(const Reserve& r, int whole);

// The cursor in the reserve column, rows in the whole list. Hold: a row past
// the last held at the last (the original's "row held inside the column").
// Move: step -1 or +1 with the original's wrap (below 0 to the last, past the
// last to 0). Both scroll `top` so the row is shown, and answer the row. With
// an empty reserve both answer 0 and leave top 0.
int Hold(Reserve& r, int whole);
int Move(Reserve& r, int whole, int step);

// The model's self-test (no game state). A Fatal on a case that does not
// hold. Run with the exe host's BOF3X_SELFTEST_ONLY or hosted
// BOF3X_SHADOW=solo_screens.
void SelfTest();

}  // namespace solo_screens

#pragma once

// The champion's mark on the two members screens in the solo-character mode
// (group MS; docs/solo-character-screens.md, docs/solo-character-round.md 4
// "MS"): the Tactics members step (TacticsMembers_Pick, src/game/field/
// field_ability_2.cpp) and PartyForm (Field_Request 6, PartyForm_Draw in
// src/game/field/party_form.cpp). Both screens are the original's code in
// every state; while solo::Solo_Active() the champion's panel is marked.
//
// The mark is the look the load list gives a cleared save. Menu_DrawSaveSlot
// (0x576960, src/game/menu/menu.cpp) draws a slot whose summary +0x14 is set
// (block +0xCB4, event flag 146) with its frame pieces at flags 2 -
// Menu_DrawPiece's CLUT (0x80, 0x1E2) in place of (0xB0, 0x1E1), the frame in
// gold - and the piece 0x3D, the "CLEAR" emblem.

#include "bof3/symbols.gen.h"

namespace solo_screens {

// The mode active (solo::Solo_Active()) and the member id's record
// (Character_IndexById) the champion's. False whenever the mode is not active.
bool IsChampion(unsigned char id);

// How the champion's panel is marked: kCleared, the cleared save's look; or
// kHand, the game's pointing hand (Menu_DrawHand), kept for the owner to
// compare.
enum class ChampionMark { kCleared, kHand };
constexpr ChampionMark kChampionMark = ChampionMark::kCleared;

// With kCleared: whether the piece 0x3D (the cleared slot's "CLEAR" emblem)
// is drawn too, as the slot draws it. Off: on a member panel it covers the
// top of the HP line (solo_screens.cpp), and its word says "cleared".
constexpr bool kChampionClearEmblem = false;

// With kHand: the hand's tip (Menu_DrawHand puts it at x) at the champion's
// cursor place plus this offset - inside the panel's top-left corner,
// pointing at the name.
constexpr int kChampionHandDx = 0x12;
constexpr int kChampionHandDy = 0;

// The rows each screen shows a column of (its cursor places, Tactics
// 0x6674C8 and PartyForm 0x664078, hold three); a reserve row past them has
// no place (the original's latent defect, docs/solo-character-screens.md 2)
// and is not marked.
constexpr unsigned kRows = 3;

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

// `draw` with the menus' ordering-table slot 1 pointed at slot 0, which is
// drawn after slot 1 (measured by the captures, docs/solo-character-
// screens.md 5a), so what it commits shows over everything the menus draw
// this frame, window records included; both slots' tails put back after (the
// save and restore the field's draw passes make of Gfx_OtPointers,
// draw_pass.cpp).
template <typename Draw> void InFront(Draw draw) {
    const auto saved = Gfx_OtPointers[1];
    Gfx_OtPointers[1] = Gfx_OtPointers[0];
    draw();
    Gfx_OtPointers[0] = Gfx_OtPointers[1];
    Gfx_OtPointers[1] = saved;
}

}  // namespace solo_screens

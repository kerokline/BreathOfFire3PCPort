#include "game/solo/solo_screens.h"

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/solo/solo.h"
#include "hook/detour.h"

// The champion's mark on the members screens (group MS; solo_screens.h has
// the why). What each address is:
//
//   CharacterRecords     0x903A70  eight records, 0xA4 each
//   Character_IndexById  0x66972C  a member id's record
//   Menu_MemberPieces    0x6632C8  the member status panel's frame pieces
//
// No state of its own.

namespace solo_screens {

bool IsChampion(unsigned char id) {
    if (!solo::Solo_Active()) return false;
    return static_cast<int>(Character_IndexById[id]) == solo::Solo_ChampionRecord();
}

// The cleared slot's draws, read from Menu_DrawSaveSlot (src/game/menu/
// menu.cpp, 0x576960): with summary +0x14 set the slot's frame pieces
// (0x663584) go to Menu_DrawPieces with flags 2 - Menu_DrawPiece's CLUT
// (0x80, 0x1E2) in place of (0xB0, 0x1E1) (menu_2.cpp) - and the piece 0x3D
// (rectangle 0xC8, 0x30, 27 x 13 at 0x663B94 + 4 * 0x3D) at (x + 0x32,
// y + 0x16), right of the level (x + 0x1F, y + 0x18).
//
// The member status panel (Menu_DrawMemberStatus, menu.cpp) ends with its
// frame pieces Menu_MemberPieces (flags 0) and then its EXP bar, so the
// cleared look on it is those pieces again with flags 2 and the bar again
// over them. Its level is drawn "%3d" from x + 0x16, so the emblem at the
// slot's distance from the level, x + 0x2F, y + 0x15, covers the top of the HP
// line (rows 8 apart, the emblem 13 high): kChampionClearEmblem is off.
// Menu_DrawMemberStatus also sets or clears the record's +0x10 bit 13 (HP
// below a quarter) from its HP, as it does for every member it draws; for a
// champion in reserve (`whole`) that is a write the original's screen does
// not make, of a bit the next status draw sets the same way.
void DrawCleared(int x, int y, unsigned record, bool whole) {
    unsigned char* const rec = move_script::At(::bof3::Img(CharacterRecords_addr) + record * 0xA4u);
    if (whole) SH_CALL(Menu_DrawMemberStatus)(x, y, record, 0);
    SH_CALL(Menu_DrawPieces)(x, y, move_script::At(::bof3::Img(Menu_MemberPieces_addr)), 2);
    SH_CALL(Menu_DrawExpBar)(x + 0x14, y + 0x12, record, rec[0xA], static_cast<unsigned>(rv::As<records::CharRecord>(rec)->exp));
    if (kChampionClearEmblem) SH_CALL(Menu_DrawPiece)(x + 0x2F, y + 0x15, 0x3D, 0);
}

}  // namespace solo_screens

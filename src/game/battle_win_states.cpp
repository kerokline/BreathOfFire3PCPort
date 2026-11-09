// The battle windows' states (the eighth round's group CL).
// docs/battle_win_states.md.
//
// Nineteen functions the owner's combat route reaches, none of them ever a
// `call rel32` target: record handler 3 of Field_RunTaskRecords (0x596FA0,
// the immediate at 0x59E25C), which runs a battle window by its kind byte +2
// from an eight-entry table built on its own stack, and the six kinds' state
// dispatchers and states under it, each reached through another such table
// (`mov [esp + 4 i], imm32`, then `call [esp + index * 4]`). Each was read to
// its last instruction (2026-09-25); the catalogue's "jumped to" is the
// return address after `call [esp + eax * 4]`, which pe_funcs.py does not
// count as a call.
//
// Every call goes through battle_win_states::g (battle_win_states_callees.h)
// so that the start-up fuzz can stand recorders in for them - for ours and
// for the originals' copies alike. Where the original reads memory after a
// call, ours reads it after the same call; the current window record
// 0x905B84 is re-read where the original re-reads it.
//
// Upper halves. The originals push coordinates as 16-bit registers whose
// upper halves are whatever the register held, and bytes likewise; every
// callee here reads only the low 16 bits of a coordinate and the low byte of
// a slot or an icon (docs/battle_windows.md section 2 for the BC draws;
// Text_DrawAt stores word x and y; Menu_DrawIcon and Battle_ActorIsOut take
// low bytes), so ours passes the value as C++ computes it. The one argument
// whose upper bits are the original's arithmetic and not garbage - the banner
// text's colour, pushed as eax with the record offset's upper bits - ours
// computes the same way.
#include "game/battle_win_states.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_win_states_callees.h"
#include "game/lang_layout.h"
#include "game/text_advance.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_win_states {

const Callees kOriginals = {
    BattleWin_DrawPartyStatus,
    BattleWin_DrawCommandCross,
    BattleWin_DrawCommandLabel,
    BattleWin_DrawSmallBox,
    BattleWin_DrawMediumBox,
    BattleWin_DrawTargetEnemy,
    BattleWin_DrawEnemyStatus,
    BattleWin_DrawTargetMember,
    Window_FlagAndAdvance,
    Window_RestoreAndBack,
    Window_DispatchKind,
    Text_GlyphCount,
    Text_DrawAt,
    Menu_DrawIcon,
    Battle_ActorIsOut,
    Battle_ReturnTrue,
    {at::kKinds[0], at::kKinds[1], at::kKinds[2], at::kKinds[3], at::kKinds[4], at::kKinds[5], at::kKinds[6],
     at::kKinds[7]},
    {at::kPartyStates[0], at::kPartyStates[1], at::kPartyStates[2]},
    {at::kCrossStates[0], at::kCrossStates[1], at::kCrossStates[2]},
    {at::kLabelStates[0], at::kLabelStates[1], at::kLabelStates[2]},
    {at::kBannerStates[0], at::kBannerStates[1], at::kBannerStates[2]},
    {at::kEnemyStates[0], at::kEnemyStates[1], at::kEnemyStates[2], at::kEnemyStates[3]},
    {at::kMemberStates[0], at::kMemberStates[1], at::kMemberStates[2]},
};
Callees g = kOriginals;

}  // namespace battle_win_states

using namespace battle_win_states;

namespace {

// Enemy() is +0x80 into the enemy object (EnemyWorkingRecords); the record, and
// its typed view, starts 0x80 before.

}  // namespace

// ===========================================================================
// The record handler and the kinds' dispatchers

// ===========================================================================
// Kind 0: the party row

// ===========================================================================
// Kind 1: the command cross

// ===========================================================================
// Kind 2: the command label

// ===========================================================================
// Kind 3: the action banner

// original 0x597230 (PSX 0x801E9470): state 1 - banner record k = +0xA of the
// pool 0x93B8E0 (12 bytes a record): its text (+4) counted by
// Text_GlyphCount; then, record re-read, a wide banner (+2 set) is the medium
// box at (x - 0x10, y) and the text at x + 0x26 - 6 n, 0x12 characters; a
// narrow one the small box at (x, y) and the text at x + 6 (6 - n), 8
// characters; y + 3, colour +0xA, through Text_DrawAt. The record is re-read
// after the count and after the box.
//
// DIVERGENCE DIV-0061: under a Latin language overlay the text is centred
// on the width the pen covers (DIV-0006's advances) about the same middle -
// x + 0x26 wide, x + 0x24 narrow - instead of by 6 units a glyph: the
// owner's "Green Apple" and "Flare" left of centre, 2026-09-27. 0 until
// BattleWinStates_Inject patches it to 1 under the name BattleBannerCentre,
// so BOF3X_ORIGINAL=BattleBannerCentre and the fuzz keep the original's x.
// The same flag lets the text run to half again the characters (amended for
// issue 127): the room for 8 glyphs of 12 units holds 12 letters of 8, and
// "Lucky Strike" and "Instant Kill", 12 each, drew as "Lucky St".
unsigned char g_banner_centre = 0;

// ===========================================================================
// Kind 4: an enemy's gauge

// ===========================================================================
// Kind 5: a member's gauges

// ===========================================================================

void BattleWinStates_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_win_states")) SelfTest();
    // DIVERGENCE DIV-0061: a Latin language overlay only, as MenuLists_Inject
    // tests it for DIV-0058.
    {
        if (Lang_Latin()) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("BattleBannerCentre",
                             Low32(&g_banner_centre),
                             &was, &is, 1);
            bof3::Log("DIV-0061    action banners centred on their width: %s", g_banner_centre ? "on" : "off");
        }
    }

}

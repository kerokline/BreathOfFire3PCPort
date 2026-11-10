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

// ===========================================================================
// Kind 4: an enemy's gauge

// ===========================================================================
// Kind 5: a member's gauges

// ===========================================================================

void BattleWinStates_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_win_states")) SelfTest();
}

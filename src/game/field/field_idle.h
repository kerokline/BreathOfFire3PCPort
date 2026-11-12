// The field's idle frame and the mod behaviours' statics of ours that live
// beside it (docs/mods.md section 11; DIVERGENCE DIV-0217, DIV-0218,
// DIV-0223).
//
// The Definitive Edition romhack put its per-frame field code in slot 0 of
// Field_ModeTailKinds - the slot that runs when no tail is armed, a bare ret
// in the release - and its state in bytes it freed inside GAME.EMI's image:
// RAM only, never in the save. Ours is FieldIdle_Tick (field_7.cpp), called
// by Field_ModeTailRun before slot 0's entry, and these statics. They are
// zero unless a behaviour is on, are not in the save, are cleared at the start
// of every started or loaded game (GameMode_Start), and are one quicksave
// participant ("BEHAVIOURS", field_7.cpp), so a quick-load puts back the
// moment it took. The state hash does not see them (docs/state-hash.md 3: the
// statics of our own module are outside .data).
#pragma once

#include <cstdint>

namespace field_idle {

// DIV-0217 AllyAnywhere: the hack's two bytes. `ally_close_menu`: Tactics_Top
// refused a member change, so the camp menu's top bar takes its cancel branch
// once. `ally_pending`: the next idle field frame with no request raises
// Field_Request 6, the full-party swap.
extern std::uint8_t g_ally_close_menu;
extern std::uint8_t g_ally_pending;

// DIV-0218 SmokeSkill: the hack's halfword - the encounter checks left that
// answer "none" (Field_EncounterDue); the field use of Smoke sets it to 128.
extern std::uint16_t g_smoke_steps;

// DIV-0223 CampMasters (docs/camp-masters.md): the hack's two bytes and the
// swap's state. `master_pending`: the master list's choice, list entry + 1
// (ShopBrowse_Choose), until the next idle field frame arms the talk.
// `master_active`: the same value from that frame to the talk's state 3 -
// the talk is a camp talk. `master_swapped`: the home area's text block is
// in the arena (state 1 to state 3), `master_home` that area.
extern std::uint8_t g_master_pending;
extern std::uint8_t g_master_active;
extern std::uint8_t g_master_swapped;
extern std::uint8_t g_master_home;

// Every static above to zero: GameMode_Start, the start of every game.
void ClearAtStart();

// DIV-0223: the area whose text block and choice table are in force - a camp
// master's home area while its talk holds them, else `standing`
// (Game_AreaNumber, which the caller reads as the original does).
unsigned short TextArea(unsigned short standing);

// DIV-0223: a camp talk is running (from the arm to state 3).
bool CampTalk();

// DIV-0223: FieldTail_LoadBank's state 1 and state 3 (field_7.cpp).
void CampMastersSwapIn();
void CampMastersSwapOut();

}  // namespace field_idle

// The field's idle frame: Field_ModeTailRun calls it when AreaW0a_TailKind is
// 0, before slot 0's entry. Each behaviour that needs the idle frame is a case
// in it; off, none does anything.
void FieldIdle_Tick();

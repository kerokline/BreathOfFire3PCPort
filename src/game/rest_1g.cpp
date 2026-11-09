// Group R1G of round fourteen (wave one): the fishing spot's code, 45 functions
// at 0x5289A0..0x52CD46 - the cut's 43 rows (analysis/round14_cut.tsv) and the
// two starts in their spans no list had (0x528BE0, 0x52BF90), each read to its
// last instruction with capstone (2026-10-04) and fuzzed through the scenario
// harness's field mode (rest_1g_fuzz.cpp). docs/rest_1g.md has them one row
// each.
//
// The leader's state 9 (LeaderPanel_*): E1E took stages 2..9 in round thirteen
// (docs/effect_1e.md); here are stage 1's dispatcher and its steps 0..4 and 7,
// stage 9's step 4, stages 10 and 11 (dispatchers and steps), the two pages
// ChoiceMenu_Run (E1F) jumps to, and the helpers E1E called by address. They
// run with Sprite_Current the leader's ObjTrio record and drive Effect_Objects
// records 0..6 as E1E's do.
//
// Game mode 8's fish (Fish_*): Fish_Spawn fills Sprite_Objects from the spot's
// counts (0x905B88) and the kind table 0x66A690 (36 bytes a kind); Fish_RunAll,
// called by GameMode8_Frame, runs every record in use through Fish_States by
// its +1, with Sprite_Current and Field_ActiveMember the record.
//
// Every one is a faithful replacement. Where the original indexes a .data
// table by a byte it never bounds (a dispatcher's state byte, Fish_Spawn's
// record count) ours aborts with a message (round9 doc section 6); where it
// reads a .data table in place by an unmasked byte that stays inside the image
// (the fish kind, a direction) ours reads the same bytes in place. Every call
// goes through the harness (SH_CALL), so the start-up fuzz can stand recorders
// in for the callees; Sprite_Current and Field_ActiveMember are re-read after
// every call, as the originals re-read them.
#include "game/rest_1g.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

// --- the cells ------------------------------------------------------------------------

constexpr ::bof3::image::Va kEff0StepZ{0x7E11F4};     // record 0 +0x14
constexpr ::bof3::image::Va kEff0X{0x7E1214};         // record 0 +0x34
constexpr ::bof3::image::Va kEff0Z{0x7E1218};         // record 0 +0x38
constexpr ::bof3::image::Va kEff0Height{0x7E121E};    // record 0 +0x3E (s16)

}  // namespace

// ===========================================================================
// Stage 1 (LeaderPanel_Stage1Steps 0x66022C: steps 0..4 and 7 here, 5, 6, 8..10
// E1E's)
// ===========================================================================

// ===========================================================================
// The fishing menu's pages (ChoiceMenu_Choices 0x6602F4 [1], [2]: E1F's
// ChoiceMenu_Run jumps by record 6 +6) and stage 9's step 4
// ===========================================================================

// ===========================================================================
// Stages 10 and 11
// ===========================================================================

// ===========================================================================
// The stages' helpers (E1E called them by address)
// ===========================================================================

// ===========================================================================
// Game mode 8's fish
// ===========================================================================

void Rest1G_Inject() {
    if (bof3::WantsShadow("rest_1g")) rest_1g::SelfTest();
}

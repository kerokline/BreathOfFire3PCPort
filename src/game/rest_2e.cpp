// Group R2E of round fourteen (wave two): three of the field menu's screens,
// 49 functions at 0x58B1D0..0x58ED3F - the cut's 48 rows (analysis/round14_cut.tsv)
// and the start in their span no list had (0x58CFC0), each read to its last
// instruction with capstone (2026-10-04) and fuzzed through the scenario
// harness's field mode (rest_2e_fuzz.cpp). docs/rest_2e.md has them one row
// each.
//
//   - The Items screen (FieldMenu_States[2], whose dispatcher 0x58AAE0 and
//     state table 0x667328 are R2D's): the three arrange steps R2D's 0x58B1C0
//     jumps to through FieldMenuItems_State5Steps, the discard prompt (state 6),
//     the use-on-a-member state (7), a 32-entry list view (9), two window
//     helpers, and FieldItems_Sort with its seven sorts.
//   - The Equipment screen (FieldMenu_States[4]): FieldEquip_Run and its
//     states but FS's two choosers, the "best equipment" previews and their
//     apply, the remove-slot state.
//   - The Ability screen (FieldMenu_States[3]): FieldAbility_Run, its states,
//     and the arrange and view step machines under states 7 and 9.
//
// The menu block 0x929F00 (mode, state +1, step +2, timer +4, cursor bytes) and
// WindowRecords 0x803160 (22 of 0x24) are the cells; the names say what the
// code does, not what the player sees (no gameplay fact from memory).
//
// Every one is a faithful replacement. Where the original indexes a .data
// table by a byte it never bounds (the dispatchers' state and step bytes,
// FieldItems_Sort's argument) ours aborts with a message past the table's own
// length (round9 doc section 6); where it reads an image table in place by a
// byte (help words, the name tables, Character_IndexById) ours reads the
// same bytes in place. The inventory's list pointers (Inventory_IdLists /
// Inventory_CountLists by a category byte) are read in place for the five
// categories; past them, or category 4's null count list, ours aborts.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for the callees; each call's answer is taken into a
// local before the next memory read, in the original's order.
#include "game/rest_2e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2e::at;

// --- the cells --------------------------------------------------------------------------

// kEquipCursor: now Rest2e_EquipCursor (symbols.toml). FieldEquip_TopMenu's cursor
// kAbilityResult: now Rest2e_AbilityResult (symbols.toml). the last ability's answer
// kAbilityCursor: now Rest2e_AbilityCursor (symbols.toml). FieldAbility_TopMenu's cursor

// --- the helpers ------------------------------------------------------------------------

}  // namespace

// ===========================================================================
// The Items screen
// ===========================================================================

// DIVERGENCE DIV-0189: the two consumable sorts order each bucket. The
// original's NormalItem / CombatItem sorts are a partition: the items whose
// flag bit (bit 0 field-usable, bit 1 battle-usable) is set go first, and
// within each half the bubble sort, being stable, leaves the list in the
// order the player had it. Ours, once g_sort_by_id is armed (after the
// self-test, PatchBytes "ItemSortById"), sorts within each half by the item
// id ascending - the record's index in the consumables table, the EMI's
// order (Green Apple 1 .. Water Jug 91). Unarmed, the original's partition.
namespace rest_2e { unsigned char g_sort_by_id = 0; }
// ===========================================================================
// The Equipment screen
// ===========================================================================

// ===========================================================================
// The Ability screen
// ===========================================================================

void Rest2E_Inject() {
    if (bof3::WantsShadow("rest_2e")) rest_2e::SelfTest();
    {
        // DIV-0189: armed after the self-test, which compares the two
        // consumable sorts with Capcom's partition.
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("ItemSortById", Low32(&rest_2e::g_sort_by_id), &was, &is, 1);
        bof3::Log("DIV-0189    the Items screen's NormalItem / CombatItem sorts order each half by item id: %s",
                  rest_2e::g_sort_by_id ? "on" : "OFF (BOF3X_ORIGINAL=ItemSortById)");
    }
}

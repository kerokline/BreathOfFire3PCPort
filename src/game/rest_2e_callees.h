// The addresses rest_2e.cpp calls of other groups of round fourteen's wave
// two - each a load-bearing constant (CLAUDE.md rule 3), called through the
// harness by address (SH_AT). Each is ours since the wave merged and is named
// here by its symbol, the value unchanged (round fourteen's rebinding,
// docs/round-14-cleanup.md). docs/rest_2e.md section 8.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_2e::at {

// R2D's (0x5869A0..0x58B1C0).
constexpr std::uint32_t kAbilityUse = bof3::addr::FieldAbility_Use;       // (user, target, ability, battle) -> al: a field ability used from
                                                      // the menu - Skill_ApCost against the user's AP, a handler of
                                                      // 0x6672EC by the ability; 1 or 5 when it took effect
// R2F's (0x58ED40..0x596A90).
constexpr std::uint32_t kAbilityWindows = bof3::addr::AbilityMenu_InitRecords;   // (): the Ability screen's window records set up
constexpr std::uint32_t kAbilitySort = bof3::addr::AbilityList_SortBy;      // (how): jmp through 0x667444 by how's low byte (the ability
                                                      // lists' sorts)
constexpr std::uint32_t kAbilityViewWindow = bof3::addr::AbilityMenu_InitRecord17;  // (): window record 0x8033C4 (the view's list) set up
constexpr std::uint32_t kAbilityCloseWindows = bof3::addr::FieldMenu_FreeRecords13To18;  // (): six window records cleared

}  // namespace rest_2e::at

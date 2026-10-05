// The raw addresses rest_2e.cpp calls that are not ours yet - each a
// load-bearing constant (CLAUDE.md rule 3), another group of round fourteen's
// wave two, called through the harness by address (SH_AT) until the round's
// rebinding. docs/rest_2e.md section 8.
#pragma once

#include <cstdint>

namespace rest_2e::at {

// R2D's (0x5869A0..0x58B1C0).
constexpr std::uint32_t kAbilityUse = 0x58A3C0;       // (user, target, ability, battle) -> al: a field ability used from
                                                      // the menu - Skill_ApCost against the user's AP, a handler of
                                                      // 0x6672EC by the ability; 1 or 5 when it took effect
// R2F's (0x58ED40..0x596A90).
constexpr std::uint32_t kAbilityWindows = 0x58ED40;   // (): the Ability screen's window records set up
constexpr std::uint32_t kAbilitySort = 0x58EE40;      // (how): jmp through 0x667444 by how's low byte (the ability
                                                      // lists' sorts)
constexpr std::uint32_t kAbilityViewWindow = 0x58F000;  // (): window record 0x8033C4 (the view's list) set up
constexpr std::uint32_t kAbilityCloseWindows = 0x58F050;  // (): six window records cleared

}  // namespace rest_2e::at

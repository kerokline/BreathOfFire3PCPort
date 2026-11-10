// The action phases (originals 0x42F220..0x42F5DD, 0x42F670..0x42F9C8,
// 0x42FAB0..0x42FE17, 0x42FF70..0x4301A4): the battle's phase 3 and the step tables under it -
// the next actor taken from the turn order, its command kind dispatched, the
// ability and item checks and their magic started, the effects waited for,
// the enemy messages queued, and the action's end with the actor-target
// swap of the round flags' bit 6. docs/battle_actions.md.
#pragma once

#include <cstdint>


namespace battle_actions {
extern std::uint8_t g_effect_key_off;   // defined in battle_actions.cpp (source-layout B0: promoted for a moved reader)
extern bool g_unlearned_blue;   // defined in battle_actions.cpp (source-layout B0: promoted for a moved reader)
}  // namespace battle_actions

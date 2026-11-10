// The per-turn steps (originals 0x4302B0..0x43192F): the battle's phase 4
// (the round's end: the faster side's extra round, the status chain, the
// battle-end test and the next round) and phase 5 (the battle's end: the
// members' tasks, the win, the result pages, the write-back and the way out),
// eight dispatch stubs and the twelve steps they reach. docs/battle_turn_steps.md.
#pragma once

#include <cstdint>


namespace battle_turn_steps {
extern std::uint8_t g_extra_round_clear;   // defined in battle_turn_steps.cpp (source-layout B0: promoted for a moved reader)
}  // namespace battle_turn_steps

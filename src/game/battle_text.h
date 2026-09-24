#pragma once

#include <cstdint>

// DIVERGENCE DIV-0052: the battle banner's words from a language overlay. See
// src/game/battle_text.cpp.

// Applies a kind-12 chunk: a count of 12, then twelve NUL-terminated strings of
// at most 12 bytes, for the twelve entries of the banner message table
// 0x669DE0. Aborts loudly on anything else.
void BattleMessages_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// How many bytes BattleBanner_SetMessage copies of a message: the PC's 8, or
// the US release's 12 once a kind-12 chunk has been applied.
std::uint32_t BattleMessages_CopyRoom();

// The raw addresses rest_1e.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/rest_1e.md.
// Every callee of the group is ours (by name: R0A's helpers through
// game/rest_0a.h, the engine's through symbols.gen.h), the group's own, or
// Capcom's Rand; nothing is called by a raw address. The state tables the
// dispatchers jump through are named in symbols.toml ([[data]]); the
// constants below are the cells and the image's table the states read.
#pragma once

#include <cstdint>

namespace rest_1e::at {

// Field_DirectionSteps: 8 rows of two longs (x, z), read in place with the
// direction byte unmasked (shl eax, 3 on the zero-extended byte); the side
// probes read rows 3 and 5 by address.
constexpr std::uint32_t kSteps = 0x6697B0;
constexpr std::uint32_t kStepRow3 = 0x6697C8;
constexpr std::uint32_t kStepRow5 = 0x6697D8;

// The objects Sprite_ObjectAt answers: 0..0x1D Sprite_Objects, 0x1E..0x21
// Sprite_ObjectsExtra, 0xA4 bytes each; the states set bit 0 of the found
// one's +0x80 (or byte [index * 0xA4 + 0x7DEF00] / [(index - 0x1E) * 0xA4 +
// 0x802080] as the originals address them).
constexpr std::uint32_t kObjectFlag = 0x7DEF00;     // Sprite_Objects + 0x80
constexpr std::uint32_t kExtraFlag = 0x802080;      // Sprite_ObjectsExtra + 0x80
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 0x1E;
constexpr unsigned kExtraCount = 4;

// --- the ids the states pass ---------------------------------------------------------
constexpr unsigned kSoundStrike = 0x10B;            // the strike's sound (an object or cell struck)
constexpr unsigned kSoundTaken = 0x106;             // an item taken
constexpr unsigned kPickupItem = 0x56;              // the cell pickup's item (category 0)
constexpr unsigned kStrikeItem = 0x29;              // the strike cell's item (category 0)
constexpr unsigned kStrikeHurtMessage = 0xD9;       // Msg_OpenSystem's id when the strike hurts

}  // namespace rest_1e::at

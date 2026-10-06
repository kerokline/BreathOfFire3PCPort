// The raw addresses game_last.cpp and its fuzz read that symbols.toml does not
// name as typed data - each a load-bearing constant (CLAUDE.md rule 3).
// docs/game-last.md.
//
// Neither function calls anything. The constants are the image's item tables
// (.data, read in place, never copied) and the trade screen's state byte.
#pragma once

#include <cstdint>

namespace game_last::at {

using U = std::uint32_t;

// --- Item_UseFlags: the flag byte of each category's record ---------------------
// (the records are symbols.toml's NameTable_*: a 16-byte name, then the numbers)
constexpr U kWeaponFlags = 0x657461;       // NameTable_Weapons 0x657450 + 0x11, 28-byte records (category 1)
constexpr U kWeaponStride = 28;
constexpr U kArmourFlags = 0x657D79;       // NameTable_Armour 0x657D68 + 0x11, 26-byte records (category 2)
constexpr U kArmourStride = 26;
constexpr U kAccessoryFlags = 0x658461;    // NameTable_Accessories 0x658450 + 0x11, 24-byte records (category 3)
constexpr U kAccessoryStride = 24;
constexpr U kConsumableFlags = 0x656B38;   // NameTable_Consumables 0x656B28 + 0x10, the u16 flags' low byte, 22-byte records
constexpr U kConsumableStride = 22;        // (category 0 and 5 and above)
// The farthest byte any category reaches with item 255: accessories, 0x658461
// + 24 * 255 = 0x659C49 (the fuzz's region runs from kConsumableFlags to it).
constexpr U kFlagsReachEnd = 0x659C4A;

// --- ItemTrade_Dispatch -------------------------------------------------------------
constexpr U kTradeState = 0x93985C;        // u8: ItemTrade_States' index (0 open, 1 run, 2 leave; docs/game-last.md section 1.2)

}  // namespace game_last::at

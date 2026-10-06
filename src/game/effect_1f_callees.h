// The raw addresses effect_1f.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_1f.md
// sections 1 and 7.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round fourteen's rebinding: kMode8Panel reads bof3::addr::Fish_RunAll, the value unchanged

namespace effect_1f::at {

// --- callees nobody owns (no group of round thirteen lists them), by address ---
constexpr std::uint32_t kMode8Panel = bof3::addr::Fish_RunAll;    // (): R1G's (round fourteen); game mode 8's frame work, a call through 0x660324 by a
                                                   // byte (catalogue part 7, in no cut)
constexpr std::uint32_t kTradeDispatch = bof3::addr::ItemTrade_Dispatch; // group TWO's (2026-10-06), the value unchanged: (): jmp [0x66A470 + 4 * byte 0x93985C] - ItemTrade_Open,
                                                   // ItemTrade_Run, ... (FE2's table; the dispatcher is in no cut)

// --- the cells ---
constexpr std::uint32_t kRecord6 = 0x7E14E0;       // Effect_Objects record 6: the menu's state (+1, +6, +7, +8,
                                                   // +0xA, +0xC, +0x10, +0x18, +0x1C, +0x2C, +0x36, +0x38, +0x3A)
constexpr std::uint32_t kRecord2State = 0x7E1261;  // Effect_Objects record 2 +1
constexpr std::uint32_t kLabelA = 0x803622;        // MessagePools' word: the first slot's label
constexpr std::uint32_t kLabelB = 0x803624;        // the second slot's label
constexpr std::uint32_t kListLabelA = 0x803626;    // the kind-0xB list's label
constexpr std::uint32_t kListLabelB = 0x803628;    // the kind-0xA list's label
constexpr std::uint32_t kKindCounts = 0x9040EC;    // 32 count bytes, one a kind (FieldPanel_KindTotal's)
constexpr std::uint32_t kExtraItem = 0x90412E;     // the extra item slot (char_stats.cpp's name), its count 0x90412F
constexpr std::uint32_t kExtraCount = 0x90412F;
constexpr std::uint32_t kExtraAccessory = 0x904130;// the extra accessory slot
constexpr std::uint32_t kAccessoryIds = 0x9042D4;  // category 3's 128 inventory ids (Inventory_IdLists[3])
constexpr std::uint32_t kAccessoryKind = 0x658462; // NameTable_Accessories + 0x12: a 24-byte record's kind byte
constexpr std::uint32_t kKindRecords = 0x66A6A0;   // the kinds' 36-byte records (+0xF threshold, +0x12 points)

}  // namespace effect_1f::at

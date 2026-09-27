// Internal to area_011.cpp and area_011_fuzz.cpp: the cells area 11's code
// touches that symbols.toml has no name for. Every call it makes is to a
// named function (Flags_Test, ours; Effect_Spawn, Capcom's) through the
// area harness (AH_CALL). docs/area_011.md.
#pragma once

#include <cstdint>

namespace area_011 {
namespace at {

// The first party list's third byte (the lists: two of three member ids at
// 0x904062 / 0x904065, docs/field-event.md): read as a dword, masked to its
// low byte.
constexpr std::uint32_t kPartyThird = 0x904064;
// Cond_Flags row 14 (Cond_Flags + 0x70): the init tests its bits 0x13 and
// 0x14 (the byte 0x904002, bits 3 and 4).
constexpr std::uint32_t kInitFlags = 0x904000;
constexpr unsigned kInitFlagOn = 0x13, kInitFlagOff = 0x14;
// The leader's record (ObjTrio + 0) and its words +0x2E / +0x30, what the
// movement-script ops 90..95 pass Effect_Spawn as x and z.
constexpr std::uint32_t kLeader = 0x802D40;
// The area block's header entries, from AreaMap_EntryBase (as
// AreaMap_HeaderPass walks them): the kind in the top byte, the step to the
// next in dwords at byte +2, a zero dword the end. Kind 0x81 (kind 1 with
// bit 7) is AreaMap_DrawBackdrop's: colour dwords at +8 and +12.
constexpr std::uint32_t kBackdropKind = 0x81000000u;
constexpr std::uint32_t kDimColour = 0x21080000u;   // low word 0 (black), high word 0x2108 (5-bit 8, 8, 8)

}  // namespace at
}  // namespace area_011

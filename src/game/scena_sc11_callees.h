// The addresses chapter 11's code calls or reads that no group owns this wave
// (or that another group names): called through SH_AT by address, re-aimed at a
// recorder in the fuzz. docs/scena_sc11.md section 5. Round nine's rebinding
// pass turns the calls into names once their owners take them.
#pragma once

#include <cstdint>

namespace scena_sc11 {

// --- functions outside the band, nobody's this wave ---
constexpr std::uint32_t kCallB = 0x5341C0;         // Scenario_CallA's twin through call table B (0x660BD4[chapter]); a tail jmp
constexpr std::uint32_t kSpriteSetUp = 0x534DB0;   // (u8 k): reads Sprite_Current and k & 0xFF (its first lines only read)
constexpr std::uint32_t kCountDown = 0x537480;     // (u16 amount, u8 k): a u16 of the 0x903A88 records, by MoveScript_EffectState[k], counted down (first lines read)
constexpr std::uint32_t kSoundJmp = 0x587B80;      // (): a jmp to 0x5A6FF0, the sound layer
constexpr std::uint32_t kStatusBit80 = 0x56D6F0;   // (): Field_StatusBits (0x8034E1) |= 0x80

// --- tables of this chapter the vtable, SCH's, points at ---
constexpr std::uint32_t kVtable = 0x661658;        // chapter 11's 5-slot vtable (0x662C80[11]; SCH names it)

}  // namespace scena_sc11

// The addresses chapter 11's code calls or reads that no group owns this wave
// (or that another group names): called through SH_AT by address, re-aimed at a
// recorder in the fuzz. docs/scena_sc11.md section 5. Round nine's rebinding
// pass turns the calls into names once their owners take them.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc11 {

// --- functions outside the band, nobody's this wave ---
// (0x5341C0, call table B's thunk, is named Scenario_CallB by SCH and called by name.)
constexpr std::uint32_t kSpriteSetUp = bof3::addr::Sprite_FlashClut;   // (u8 k): reads Sprite_Current and k & 0xFF (its first lines only read)
constexpr std::uint32_t kCountDown = bof3::addr::Char_LoseHp;     // (u16 amount, u8 k): a u16 of the 0x903A88 records, by MoveScript_EffectState[k], counted down (first lines read)
constexpr std::uint32_t kSoundJmp = 0x587B80;      // (): a jmp to 0x5A6FF0, the sound layer
constexpr std::uint32_t kStatusBit80 = bof3::addr::Field_SetStatus80;   // (): Field_StatusBits (0x8034E1) |= 0x80

// --- tables of this chapter the vtable, SCH's, points at ---
constexpr std::uint32_t kVtable = 0x661658;        // chapter 11's 5-slot vtable (0x662C80[11]; SCH names it)

}  // namespace scena_sc11

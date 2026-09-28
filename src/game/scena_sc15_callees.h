// The addresses chapters 15..19's code calls that no group owns this wave (or
// that another group owns): called through SH_AT by address, re-aimed at a
// recorder in the fuzz. docs/scena_sc15.md section 6. The round's rebinding
// pass turns them into names once their owners take them.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc15 {

// --- engine functions nobody owns (group SX's this wave) ---
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;    // (x, z, kind): an event battle's party placement (SX)
constexpr std::uint32_t kPartyPass = bof3::addr::Party_HealJoined;     // (): the party's field pass (SX)
constexpr std::uint32_t kStatusBit80 = bof3::addr::Field_SetStatus80;   // (): Field_StatusBits |= 0x80 (SX)
constexpr std::uint32_t kEventSlot = bof3::addr::Sprite_FindFree;     // (): an event object's slot in al, 0xFF none (SX)
// --- engine functions nobody owns, in no group this wave ---
constexpr std::uint32_t kSoundStop = bof3::addr::Sound_StopChannels;     // (): the sound layer, before a restart to Boot_Task
constexpr std::uint32_t kPrimSprt16 = bof3::addr::Gpu_SetSprt16;    // (prim): byte +7 = 0x7C and the float 0.01 to +0x10 (a 16x16 SPRT)
// --- area overlay code (the area round's, no group this wave) ---
constexpr std::uint32_t kViewShift = bof3::addr::Area149_ViewShiftBack;     // (): a view shift on MapView_FocusX 0x63FF (area code)
constexpr std::uint32_t kTalkIdC0 = bof3::addr::Area192_TalkMessage;      // (u8 who): a message id, area 0xC0's table (area code)
constexpr std::uint32_t kTalkId = bof3::addr::Area191_TalkMessage;        // (u8 who): a message id, another area's table (area code)
// --- chapter 13/14's state 0, SC13's band this wave ---
constexpr std::uint32_t kState0 = bof3::addr::ScenaShared_State0;        // Scena15_States / Scena18_States / Scena19_States entry 0

}  // namespace scena_sc15

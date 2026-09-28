// The addresses chapters 15..19's code calls that no group owns this wave (or
// that another group owns): called through SH_AT by address, re-aimed at a
// recorder in the fuzz. docs/scena_sc15.md section 6. The round's rebinding
// pass turns them into names once their owners take them.
#pragma once

#include <cstdint>

namespace scena_sc15 {

// --- engine functions nobody owns (group SX's this wave) ---
constexpr std::uint32_t kPartyPlace = 0x532ED0;    // (x, z, kind): an event battle's party placement (SX)
constexpr std::uint32_t kPartyPass = 0x533E50;     // (): the party's field pass (SX)
constexpr std::uint32_t kStatusBit80 = 0x56D6F0;   // (): Field_StatusBits |= 0x80 (SX)
constexpr std::uint32_t kEventSlot = 0x57CD90;     // (): an event object's slot in al, 0xFF none (SX)
// --- engine functions nobody owns, in no group this wave ---
constexpr std::uint32_t kSoundStop = 0x587860;     // (): the sound layer, before a restart to Boot_Task
constexpr std::uint32_t kPrimSprt16 = 0x5A7730;    // (prim): byte +7 = 0x7C and the float 0.01 to +0x10 (a 16x16 SPRT)
// --- area overlay code (the area round's, no group this wave) ---
constexpr std::uint32_t kViewShift = 0x423380;     // (): a view shift on MapView_FocusX 0x63FF (area code)
constexpr std::uint32_t kTalkIdC0 = 0x42C0A0;      // (u8 who): a message id, area 0xC0's table (area code)
constexpr std::uint32_t kTalkId = 0x42BA90;        // (u8 who): a message id, another area's table (area code)
// --- chapter 13/14's state 0, SC13's band this wave ---
constexpr std::uint32_t kState0 = 0x5646B0;        // Scena15_States / Scena18_States / Scena19_States entry 0

}  // namespace scena_sc15

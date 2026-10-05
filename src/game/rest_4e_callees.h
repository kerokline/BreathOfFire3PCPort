// Group R4E's callees that are not ours yet, by raw address (the round's
// rebinding turns them into names once their owners merge). docs/rest_4e.md
// section 8.
#pragma once

#include <cstdint>

namespace rest_4e::at {

// R4D (wave four): the index of the n-th in-use entry of the 60-entry table
// 0x9046D0 (8 bytes an entry, byte 0 non-zero), al; 0xFF when there is none.
// unsigned char(unsigned n), n read as a byte. Called by CommuName_CommitEntry.
constexpr std::uint32_t kEntryNth = 0x45E6D0;

// Nobody's (the library layer; the effect harness's FX_RAW row): a 0x2C-byte
// primitive's header written at the pointer it is handed (the triangle
// CommuCursor_DrawArrow fills). void(unsigned char *prim).
constexpr std::uint32_t kSetPolyF3 = 0x5A7570;

}  // namespace rest_4e::at

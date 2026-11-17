// The callees seam of field_blocked.cpp: the struct and the one `gFieldBlocked` its functions call through, so that a
// function moved out of field_blocked.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in field_blocked.cpp.
#pragma once

#include <cstdint>

namespace field_blocked {

using ObjectFn = std::uint32_t (__cdecl*)(unsigned char*);
using PointFn = std::uint32_t (__cdecl*)(std::int32_t, std::int32_t, unsigned);   // x, y, a direction or a margin
// Every callee, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. Ours among them are tested alone.
struct CalleesFieldBlocked {
    ObjectFn map_ahead;                                              // Field_MapBlockedAhead
    PointFn object_at;                                               // Sprite_ObjectAt
    PointFn member_at;                                               // Party_MemberAt
    PointFn wide;                                                    // AreaMap_BlockedWide
    PointFn narrow;                                                  // AreaMap_BlockedNarrow (Capcom's)
    std::uint32_t (__cdecl* cell)(short, short);                     // AreaMap_CellBlocked
    std::uint32_t (__cdecl* steep)(std::int32_t, std::int32_t);                      // AreaMap_TooSteep
    std::uint32_t (__cdecl* byte_at)(short, short);                  // AreaMap_ByteAt
    std::int32_t (__cdecl* slope)(std::int32_t, std::int32_t, std::uint32_t);                // AreaMap_Slope
    std::int32_t (__cdecl* probe)(std::int32_t, std::int32_t);                               // Field_ProbeHeight
    std::int32_t (__cdecl* elevation)(std::int32_t, std::int32_t);                           // AreaMap_Elevation
    // Sprite_PointInReach, with the height as the dword the originals pass
    // (it reads the low 16 bits).
    std::uint32_t (__cdecl* reach)(std::int32_t, std::int32_t, std::int32_t, unsigned, const unsigned char*);
};

extern CalleesFieldBlocked gFieldBlocked;   // defined in field_blocked.cpp (source-layout B0: promoted for a moved reader)
}  // namespace field_blocked

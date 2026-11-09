// The callees seam of field_blocked.cpp: the struct and the one `gFieldBlocked` its functions call through, so that a
// function moved out of field_blocked.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in field_blocked.cpp.
#pragma once

#include <cstdint>

namespace field_blocked {

using ObjectFn = std::uint32_t (__cdecl*)(unsigned char*);
using PointFn = std::uint32_t (__cdecl*)(long, long, unsigned);   // x, y, a direction or a margin
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
    std::uint32_t (__cdecl* steep)(long, long);                      // AreaMap_TooSteep
    std::uint32_t (__cdecl* byte_at)(short, short);                  // AreaMap_ByteAt
    long (__cdecl* slope)(long, long, unsigned long);                // AreaMap_Slope
    long (__cdecl* probe)(long, long);                               // Field_ProbeHeight
    long (__cdecl* elevation)(long, long);                           // AreaMap_Elevation
    // Sprite_PointInReach, with the height as the dword the originals pass
    // (it reads the low 16 bits).
    std::uint32_t (__cdecl* reach)(long, long, long, unsigned, const unsigned char*);
};

extern CalleesFieldBlocked gFieldBlocked;   // defined in field_blocked.cpp (source-layout B0: promoted for a moved reader)
}  // namespace field_blocked

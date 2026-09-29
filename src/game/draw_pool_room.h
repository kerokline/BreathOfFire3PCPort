// DIVERGENCE DIV-0062: the handshake between the launcher, which reserves the
// grown draw-item pool's array in the suspended child (launcher.cpp,
// ReserveDrawPoolIn), and the dll, which looks for it by its stamp
// (draw_pool.cpp, DrawPool_Reserve). One copy, so the two cannot drift apart:
// a launcher and a dll that disagree on any of these never meet, and the game
// quietly keeps the original's 1,024 items.
#pragma once

#include <cstdint>

namespace draw_pool_room {
inline constexpr unsigned kCount = 2048;                          // items
inline constexpr std::uint32_t kBytes = kCount * 0x90;            // 0x90 bytes an item
inline constexpr char kStamp[] = "BOF3X-DRAWPOOL-2048";
// Below 16 MB (the items link by 24-bit addresses), highest first.
inline constexpr std::uint32_t kCandidates[] = {0x00F00000, 0x00E00000, 0x00D00000,
                                                0x00C00000, 0x00B00000, 0x00A00000};
}  // namespace draw_pool_room

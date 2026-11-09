// Moved here by tools/move_funcs.py from gfx_vram_ops.cpp (docs/source-layout-round.md).
#include "game/platform/gfx.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/gfx_vram_ops.h"
#include "hook/detour.h"
#include "hook/log.h"

// original 0x5AA5D6, hand-written assembly. Copies rect, within the buffer at
// `base` (1,024 cells a row), to (to_x, to_y). As the original has it:
//   - x, y, w, h are read UNSIGNED;
//   - each row copies w / 2 dwords, so an odd w loses its last cell - and the
//     row step is computed from the full w, so every row after an odd one
//     starts two bytes further left;
//   - always forward, a dword at a time, whatever the overlap;
//   - the row counter is h in the low half of a register whose high half is
//     to_x's: it counts down to zero as 32 bits. h = 0 is 65,536 rows at best.
extern "C" void __cdecl Gfx_MoveCells(void* base, const short* rect, int to_x, int to_y) {
    const auto x = static_cast<std::uint16_t>(rect[0]), y = static_cast<std::uint16_t>(rect[1]);
    const auto w = static_cast<std::uint16_t>(rect[2]), h = static_cast<std::uint16_t>(rect[3]);
    auto* src = static_cast<std::uint8_t*>(base) + (static_cast<std::uint32_t>(y) << 11) + x * 2u;
    auto* dst = static_cast<std::uint8_t*>(base) + (to_y << 11) + to_x * 2;
    const std::uint32_t step = (0x400u - w) << 1;
    std::uint32_t rows = (static_cast<std::uint32_t>(to_x) & 0xFFFF0000u) | h;
    do {
        for (std::uint32_t n = w >> 1; n != 0; --n) {
            std::uint32_t cells;
            std::memcpy(&cells, src, 4);
            std::memcpy(dst, &cells, 4);
            src += 4;
            dst += 4;
        }
        src += step;
        dst += step;
    } while (--rows != 0);
}

void Gfx_Inject() {
    BOF3_INJECT(Gfx_MoveCells);
}

// DIVERGENCE DIV-0014: the title menu in the overlay's language.
//
// The title menu is artwork: image chunk 0x1C000200 of START.DAT, a 4bpp page
// at VRAM (896, 0). The port's draw 0x5888D0 (y, row, glow) draws row i as
// one SPRT - two, the first semi-transparent, when `glow` is set - from
// (u, v) = (0, 32 i), 32 tall, centred on x = 160 - w / 2, its width from a
// table the function builds on its stack:
//
//   0x5888E4  C6 44 24 10 60   mov byte [esp + 0x10], 0x60    new game   96
//   0x5888E9  C6 44 24 11 80   mov byte [esp + 0x11], 0x80    load game 128
//   0x5888EE  C6 44 24 12 40   mov byte [esp + 0x12], 0x40    options    64
//
// - three, four and two 32 px Chinese characters. An overlay can replace the
// sheet as it replaces any image chunk, but NEW GAME as the PlayStation
// lettered it is 130 wide and LOAD GAME 140, so the widths have to come with
// it: a DAT chunk of kind 6, which is ours, carries the three bytes, and they
// go into those immediates. Nothing else of the draw changes - this is not a
// reimplementation.
//
// The page is 256 texels wide and the rows start at u = 0, so any byte fits;
// DIV-0010's far-edge table has an entry for every u + w. START.DAT is walked again each
// time the title comes round, so the bytes found there are the last ones
// written, and that is what PatchBytes is told to expect.
// BOF3X_ORIGINAL=TitleMenu_Widths leaves the immediates alone.
#include "game/title_menu.h"

#include <cstring>

#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr std::uint32_t kRowCount = 3;
constexpr std::uint32_t kInsnSize = 5;
constexpr std::uint32_t kInsnAt[kRowCount] = {0x5888E4, 0x5888E9, 0x5888EE};

std::uint8_t g_now[kRowCount] = {0x60, 0x80, 0x40};  // the original's

}  // namespace

void TitleMenu_SetWidths(std::uint32_t tag, const std::uint8_t* widths, std::uint32_t size) {
    if (tag != 0 || size != kRowCount)
        bof3::Fatal("title menu chunk: tag 0x%08X, %u bytes - wants tag 0 and 3", (unsigned)tag, (unsigned)size);
    for (std::uint32_t i = 0; i < kRowCount; ++i) {
        if (widths[i] == 0) bof3::Fatal("title menu chunk: row %u has no width", (unsigned)i);
        // The whole instruction, so that a wrong address cannot pass for the right one.
        const auto slot = static_cast<std::uint8_t>(0x10 + i);
        const std::uint8_t was[kInsnSize] = {0xC6, 0x44, 0x24, slot, g_now[i]};
        const std::uint8_t is[kInsnSize] = {0xC6, 0x44, 0x24, slot, widths[i]};
        bof3::PatchBytes("TitleMenu_Widths", kInsnAt[i], was, is, kInsnSize);
    }
    // PatchBytes leaves the bytes alone under BOF3X_ORIGINAL, every time alike,
    // so g_now only moves when the code did.
    const auto* first = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(kInsnAt[0]));
    if (first[kInsnSize - 1] == widths[0]) std::memcpy(g_now, widths, kRowCount);
    bof3::Log("DIV-0014: title menu rows %u, %u, %u wide", (unsigned)widths[0], (unsigned)widths[1],
              (unsigned)widths[2]);
}

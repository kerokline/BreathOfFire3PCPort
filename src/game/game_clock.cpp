#include "game/game_clock.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// DIV-0022 (docs/known-defects.md D5). GetTickCount is the exe's only clock,
// and WinMain is its only reader: it loads the import slot into a register
// once (0x4FCD90) and calls it three times, all to pace logic frames against
// a deadline kept in a 32-bit FLOAT. A float that large has no room for the
// milliseconds - past 2^29 ms since the last full boot each frame's +33.334
// rounds to +64, half speed, and Fast Startup carries that count across a
// player's nightly shutdowns. So the slot gets a clock that starts near zero
// when the game does: the float code, untouched, then sees the numbers it
// was written for.
//
// BOF3X_TICK_BASE=N (tooling; decimal or 0x hex) starts this clock at N ms
// instead of 0, to put the original pacing code in any band of D5 on
// demand. BOF3X_ORIGINAL=Game_Clock leaves the slot on Windows' clock.

namespace {

ULONGLONG g_start;
DWORD g_base;
ULONGLONG g_paused_at;   // 0: running

// Time spent paused is taken out by moving the start forward, so the clock
// resumes from where it stopped.
DWORD WINAPI SessionTickCount() {
    const ULONGLONG now = g_paused_at ? g_paused_at : GetTickCount64();
    return g_base + static_cast<DWORD>(now - g_start);
}

bool g_ours;

}  // namespace

bool GameClock_Pause() {
    if (!g_ours || g_paused_at) return false;
    g_paused_at = GetTickCount64();
    if (g_paused_at == 0) g_paused_at = 1;
    return true;
}

void GameClock_Resume() {
    if (!g_paused_at) return;
    g_start += GetTickCount64() - g_paused_at;
    g_paused_at = 0;
}

void GameClock_Inject() {
    char text[32];
    const DWORD n = GetEnvironmentVariableA("BOF3X_TICK_BASE", text, sizeof text);
    if (n > 0 && n < sizeof text) g_base = static_cast<DWORD>(std::strtoul(text, nullptr, 0));
    g_start = GetTickCount64();

    // The slot must already hold kernel32's GetTickCount - the loader has
    // bound the imports by the time the DLL is injected. PatchBytes refuses
    // otherwise.
    const auto real = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&GetTickCount));
    const auto ours = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&SessionTickCount));
    std::uint8_t expected[4], replacement[4];
    std::memcpy(expected, &real, 4);
    std::memcpy(replacement, &ours, 4);
    const auto slot = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&Imp_GetTickCount));
    bof3::PatchBytes("Game_Clock", slot, expected, replacement, 4);
    g_ours = reinterpret_cast<std::uintptr_t>(Imp_GetTickCount) == ours;
    if (g_ours)
        bof3::Log("DIV-0022    game clock: milliseconds since the game started, from %lu", g_base);
}

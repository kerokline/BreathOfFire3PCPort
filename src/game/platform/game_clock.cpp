#include "game/game_clock.h"


#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "os/os.h"

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
// That is the original WinMain (BOF3X_ORIGINAL=Game_WinMain). Ours
// (win_main.cpp, DIV-0047) paces on GameClock_NowMs below and reads the slot
// only for the once-a-second frame-rate text.
//
// BOF3X_TICK_BASE=N (tooling; decimal or 0x hex) starts this clock at N ms
// instead of 0, to put the original pacing code in any band of D5 on
// demand. BOF3X_ORIGINAL=Game_Clock leaves the slot on Windows' clock.

namespace {

std::uint64_t g_start;
std::uint32_t g_base;
std::uint64_t g_paused_at;   // 0: running

// The high-resolution clock beside it, in os::Counter's ticks (QueryPerformanceCounter's on Windows).
std::uint64_t g_qpf;
std::int64_t g_qpc_start;
std::int64_t g_qpc_paused_at;   // 0: running

// Time spent paused is taken out by moving the start forward, so the clock
// resumes from where it stopped.
std::uint32_t __stdcall SessionTickCount() {
    const std::uint64_t now = g_paused_at ? g_paused_at : bof3::os::TickMs64();
    return g_base + static_cast<std::uint32_t>(now - g_start);
}

bool g_ours;

}  // namespace

bool GameClock_Pause() {
    if (!g_ours || g_paused_at) return false;
    g_paused_at = bof3::os::TickMs64();
    if (g_paused_at == 0) g_paused_at = 1;
    const std::int64_t q = static_cast<std::int64_t>(bof3::os::Counter());
    g_qpc_paused_at = q ? q : 1;
    return true;
}

void GameClock_Resume() {
    if (!g_paused_at) return;
    g_start += bof3::os::TickMs64() - g_paused_at;
    g_paused_at = 0;
    const std::int64_t q = static_cast<std::int64_t>(bof3::os::Counter());
    g_qpc_start += q - g_qpc_paused_at;
    g_qpc_paused_at = 0;
}

double GameClock_NowMs() {
    const std::int64_t q = g_qpc_paused_at ? g_qpc_paused_at : static_cast<std::int64_t>(bof3::os::Counter());
    return static_cast<double>(q - g_qpc_start) * 1000.0 / static_cast<double>(g_qpf);
}

void GameClock_Inject() {
    char text[32];
    const std::uint32_t n = bof3::os::GetEnv("BOF3X_TICK_BASE", text, sizeof text);
    if (n > 0 && n < sizeof text) g_base = static_cast<std::uint32_t>(std::strtoul(text, nullptr, 0));
    g_start = bof3::os::TickMs64();
    g_qpf = bof3::os::CounterFrequency();
    g_qpc_start = static_cast<std::int64_t>(bof3::os::Counter());

    // The slot must already be bound - hosted, the loader bound the imports
    // before the DLL was injected; in bof3x.exe, BindSlots did. `expected` is
    // the word as it stands (docs/x64-build-design.md 4, the SoundSdl switch's
    // shape): hosted and in the 32-bit exe host that is kernel32's
    // GetTickCount; in the 64-bit exe host the slot holds its thunk's place,
    // and kernel32's own address lies above 4 GB, where no 32-bit word can
    // name it. An empty slot is a Fatal, as a slot holding something else
    // was.
    std::uint8_t expected[4], replacement[4];
    std::memcpy(expected, &Imp_GetTickCount, 4);   // the 4-byte slot itself (Slot32<void>)
    std::uint32_t real;
    std::memcpy(&real, expected, 4);
    if (real == 0) bof3::Fatal("Game_Clock: the import slot 0x%08X is empty: GameClock_Inject before it is bound", (unsigned)Imp_GetTickCount_addr);
    const auto ours = Low32(&SessionTickCount);
    std::memcpy(replacement, &ours, 4);
    // PatchBytes takes the slot's VA, a name (detour.cpp: a VA in the linked
    // span is written at its place, Img(va)); &Imp_GetTickCount is already the
    // place, which at a moved base PatchBytes would only write right by its
    // "not in the linked span" branch, and name in its messages.
    bof3::PatchBytes("Game_Clock", Imp_GetTickCount_addr, expected, replacement, 4);
    g_ours = reinterpret_cast<std::uintptr_t>(Imp_GetTickCount.Native()) == ours;
    if (g_ours)
        bof3::Log("DIV-0022    game clock: milliseconds since the game started, from %lu", static_cast<unsigned long>(g_base));
}

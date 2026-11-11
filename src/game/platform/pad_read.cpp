// The input devices, ours (DIV-0050, docs/controls.md).
//
// Three functions of the port's input layer are replaced together:
//
//   DInput_Init      0x5A94C0  opens the devices at Game_Init
//   Pad_Read         0x5A9700  builds the PlayStation pad word each latch
//   DInput_Shutdown  0x5A9690  releases them at WinMain's exit
//
// The keyboard is Capcom's, unchanged: the DirectInput 7 system keyboard,
// non-exclusive and background, read into the 256 key bytes at Key_State and
// mapped through the 32-entry (DIK scancode, pad bits) table at Key_Table -
// BOF3.CFG's lines 3 and up, or the default at Key_TableDefault. That path is
// reproduced instruction for instruction (the shadow check below compares it
// with a clone of the original over random key states and tables).
//
// The pad is not Capcom's. The original enumerated the first DirectInput
// joystick and read it digitally with a fixed button order that matches no
// modern pad - the POV hat was never read, no button reached L2, and an
// Xbox-class pad came out as A = cross, B = square, X = triangle, Y = circle
// (docs/controls.md section 1). The joystick enumeration is dropped, and the
// pad word comes from SDL3's gamepad layer instead: one pad, the first
// connected, re-opened on hot-plug; the d-pad and the left stick both the
// directions, the triggers L2 / R2, the face buttons by position or by the
// Nintendo layout (BOF3X_PAD_LAYOUT=nintendo swaps them; auto follows the
// pad's own labels). The stick threshold is the original's: half travel.
// Both maps can be replaced from the launcher's Controls dialog: BOF3X_KEYS
// rewrites the game's key table, BOF3X_PAD the pad map (input/bindings.h).
//
// Pad_Read runs from WinMain's loop on the main stack, so SDL's calls have
// room; nothing here runs on a task's 16 KB stack.
#include "game/pad_read.h"

#include <dinput.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/bindings.h"
#include "input/pad_sdl.h"
#include "os/os.h"

namespace {

namespace input = bof3x::input;

constexpr unsigned kKeyTableEntries = 32;   // (u16 key, u16 bits) pairs; a key of 0 ends it

// --- The keyboard, as the original reads it ----------------------------------

// The table over the key bytes: the loop at 0x5A981F..0x5A984E.
unsigned KeyboardWord() {
    unsigned word = 0;
    for (unsigned i = 0; i < kKeyTableEntries; ++i) {
        const unsigned short key = Key_Table[i * 2];
        if (key == 0) break;
        if (Key_State[key]) word |= Key_Table[i * 2 + 1];
    }
    return word;
}

// --- The pad, through SDL3 -----------------------------------------------------
//
// The reading itself is input/pad_sdl.cpp, shared with the launcher so that
// what its Controls dialog captures is what the game reads. The map is a
// list of (pad input -> PlayStation bits), input::Bindings' pad half: the
// default of DIV-0050, or BOF3X_PAD from the launcher (bof3x.ini `pad.*`).

using input::Layout;
}  // namespace
namespace pad_read { Layout g_layout = Layout::kPositional; }  namespace {
}  namespace pad_read { std::vector<input::PadBinding> g_pad_map; }
namespace {
void PadLogLine(const char* line) { bof3::Log("%s", line); }

Layout LayoutFromEnv(Layout fallback) {
    char v[32] = {};
    const DWORD n = bof3::os::GetEnv("BOF3X_PAD_LAYOUT", v, sizeof v);
    if (n == 0 || n >= sizeof v) return fallback;
    const int l = input::LayoutFromName(v);
    if (l < 0) {
        bof3::Log("pad: BOF3X_PAD_LAYOUT=%s not understood; %s", v, input::LayoutName(fallback));
        return fallback;
    }
    return static_cast<Layout>(l);
}

// BOF3X_PAD: `input=action,...,layout=NAME` (input/bindings.h). Unset, the
// defaults; set, only what it names.
void LoadPadMap() {
    input::Bindings defaults = input::Bindings::Defaults();
    pad_read::g_pad_map = defaults.pad;
    Layout layout = defaults.layout;
    char text[2048] = {};
    const DWORD n = bof3::os::GetEnv("BOF3X_PAD", text, sizeof text);
    if (n > 0 && n < sizeof text) {
        std::vector<input::PadBinding> pad;
        const int understood = input::ParsePad(text, ',', pad, layout);
        pad_read::g_pad_map = pad;
        bof3::Log("pad: BOF3X_PAD - %d items understood, %u inputs bound, layout %s", understood,
                  static_cast<unsigned>(pad.size()), input::LayoutName(layout));
    } else if (n >= sizeof text) {
        bof3::Log("pad: BOF3X_PAD too long (%lu); the default map", static_cast<unsigned long>(n));
    }
    pad_read::g_layout = LayoutFromEnv(layout);
}

void StartSdl() {
    if (input::PadSdl_Started()) return;
    LoadPadMap();
    input::PadSdl_Start(pad_read::g_layout, &PadLogLine);
}

// --- BOF3X_SHADOW=pad_read: the keyboard path against a clone --------------------
// With both device pointers null the original is the table loop and nothing
// else (0x5A9700: je on DInput_Joystick, je on DInput_Keyboard). The clone
// runs that way over random key bytes and random tables - including tables
// that end early, and bit patterns across both bytes - against KeyboardWord.
// The function has no direct calls (only COM calls through vtables, which
// the nulls skip), so a byte copy runs in place.

using WordFn = unsigned (__cdecl*)();

void SelfTest(WordFn theirs) {
    constexpr unsigned kRounds = 20000;
    unsigned char saved_state[256];
    unsigned short saved_table[kKeyTableEntries * 2];
    std::memcpy(saved_state, Key_State, sizeof saved_state);
    std::memcpy(saved_table, Key_Table, sizeof saved_table);
    void* const saved_joy = DInput_Joystick;
    void* const saved_kb = DInput_Keyboard;
    DInput_Joystick = nullptr;
    DInput_Keyboard = nullptr;

    std::uint32_t rng = 0x2545F491u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };

    unsigned bad = 0, nonzero = 0, short_tables = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (unsigned i = 0; i < 256; ++i) Key_State[i] = static_cast<unsigned char>(next() % 4 == 0 ? next() : 0);
        const unsigned len = next() % (kKeyTableEntries + 1);
        if (len < kKeyTableEntries) ++short_tables;
        for (unsigned i = 0; i < kKeyTableEntries; ++i) {
            // keys are scancodes 0..255; a 0 key ends the table, so live entries avoid it
            Key_Table[i * 2] = i < len ? static_cast<unsigned short>(1 + next() % 255) : 0;
            Key_Table[i * 2 + 1] = static_cast<unsigned short>(next());
        }
        if (round % 7 == 0) Key_Table[(next() % kKeyTableEntries) * 2] = 0;   // an early end at random
        const unsigned t = theirs();
        const unsigned o = KeyboardWord();
        if (o) ++nonzero;
        if (t != o) {
            if (bad < 5) bof3::Log("pad_read shadow: round %u theirs %#x ours %#x", round, t, o);
            ++bad;
        }
    }

    DInput_Joystick = saved_joy;
    DInput_Keyboard = saved_kb;
    std::memcpy(Key_State, saved_state, sizeof saved_state);
    std::memcpy(Key_Table, saved_table, sizeof saved_table);
    bof3::Log("pad_read shadow: %u rounds, %u differ, %u non-zero words, %u short tables%s", kRounds, bad, nonzero,
              short_tables, bad ? " - FAIL" : "");
}

}  // namespace

// --- The three originals ------------------------------------------------------

bool PadRead_AnyInputDown() {
    // The intro videos play before DInput_Init (win_main.cpp: the two
    // Fmv_Play calls come first), so the pad is started here when it has not
    // been; DInput_Init's StartSdl then does nothing. The owner's first try,
    // 2026-09-30: no skip, the pad not yet open.
    StartSdl();
    if (!input::PadSdl_Started()) return false;
    input::PadSdl_Poll();
    return input::PadSdl_FirstInputDown() >= 0;
}

void PadRead_Inject() {
    static_assert(Key_State_count == 256);
    static_assert(Key_Table_count == kKeyTableEntries * 2);
    // 0x5A9700..0x5A9855, 0x155 bytes: no direct calls, every jump internal (disasm 2026-09-24).
    if (bof3::WantsShadow("pad_read"))
        SelfTest(reinterpret_cast<WordFn>(bof3::CloneOriginal("Pad_Read", bof3::addr::Pad_Read, 0x155)));
}

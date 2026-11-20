// Host test of the drawn launcher's bind capture against the Win32 one
// (docs/launcher-portable.md, stage P3: "the stored binding numbers for every
// key equal the Win32 capture's"). No key is pressed: the proof is made from
// the two mappings, for every key a WM_KEYDOWN can name. The Win32 capture is
// the Win32 capture as it was, removed 2026-11-20 with the Win32 dialogs
// (stage P6): Win32Take below holds its own copy of that rule.
//
// For a key whose message carries the scan code `scan` (1..0x7F) and the
// extended flag `ext`:
//   the Win32 capture   the Win32 capture as it was, removed 2026-11-20
//                       (src/launcher/config_dialog.cpp CaptureKeyHook): scan 1
//                       cancels; on a Function keys cell F10 (0x44, not
//                       extended) and either Alt (0x38) are refused; else it
//                       stores scan | ext << 7. Win32Take below is that code.
//   the drawn capture   SDL on Windows makes the message a scancode
//                       (SDL_windowsevents.c WindowsScanCodeToSDLScanCode:
//                       index scan | ext << 7, but Pause - 0x45 not extended -
//                       at 0xC6; windows_scancode_table, SDL's own, read here
//                       as data); src/launcher/ui/capture_key.cpp makes the
//                       scancode a stored number (CaptureTakeKey).
// The test walks every (scan, ext), on a binding cell and on a Function keys
// cell, and lists every key where the two answers differ. A difference not in
// kExpected fails the test.
//
// Then D427's facts: what each path stores for Pause and NumLock, and what the
// game reads for each key - a game button by the 256 key bytes
// (input/key_sdl.h FillKeyState, DirectInput's numbering), a function key by
// its key message (input/function_keys.h FnKeyOfMessage on Windows,
// FnMessageBitsOfDik off it).
//
//   cmake -S tools/key_host -B <scratch>/key-host -DSDL_SRC=<build>/_deps/sdl3-src
//   cmake --build <scratch>/key-host && <scratch>/key-host/capture_tests
#include <cstdint>
#include <cstdio>
#include <string>

#include "events/scancodes_windows.h"   // SDL's windows_scancode_table, through shim/SDL_internal.h
#include "input/bindings.h"
#include "input/function_keys.h"
#include "input/key_sdl.h"
#include "launcher/ui/capture_key.h"

namespace {

namespace in = bof3x::input;
namespace ui = bof3x::ui;

int g_checks = 0, g_failed = 0;

void Check(bool ok, const char* what, int a = 0, int b = 0) {
    ++g_checks;
    if (ok) return;
    ++g_failed;
    std::printf("FAIL %s (0x%02X, 0x%02X)\n", what, a, b);
}

// An answer of either capture: what it does with the key, and the number.
struct Answer {
    ui::CaptureTake take;
    int number;
    bool operator==(const Answer& o) const { return take == o.take && (take != ui::CaptureTake::kKey || number == o.number); }
};

// CaptureKeyHook, for a key going down: the Win32 capture as it was, removed
// 2026-11-20 (config_dialog.cpp, the capture).
Answer Win32Take(int scan, bool extended, bool fn) {
    if (scan == 0x01) return {ui::CaptureTake::kCancel, -1};
    if (fn && ((scan == 0x44 && !extended) || scan == 0x38)) return {ui::CaptureTake::kRefused, -1};
    if (scan != 0) return {ui::CaptureTake::kKey, in::FnKeyOfScan(scan | (extended ? 0x80 : 0))};
    return {ui::CaptureTake::kNothing, -1};
}

// SDL on Windows: the message's scancode.
SDL_Scancode SdlScancode(int scan, bool extended) {
    const int index = extended ? (scan | 0x80) : scan == 0x45 ? 0xC6 : scan;
    return windows_scancode_table[index];
}

// The drawn capture: SDL's scancode through capture_key.h. An unknown
// scancode never reaches it (ui_imgui.cpp takes no SDL_SCANCODE_UNKNOWN), so
// the capture waits on, as for a key with no number.
Answer DrawnTake(int scan, bool extended, bool fn) {
    const SDL_Scancode s = SdlScancode(scan, extended);
    if (s == SDL_SCANCODE_UNKNOWN) return {ui::CaptureTake::kNothing, -1};
    int number = -1;
    const ui::CaptureTake take = ui::CaptureTakeKey(s, fn, &number);
    return {take, number};
}

std::string Describe(const Answer& a) {
    char text[64];
    switch (a.take) {
    case ui::CaptureTake::kNothing: return "takes nothing (waits on)";
    case ui::CaptureTake::kCancel: return "cancels";
    case ui::CaptureTake::kRefused: return "refuses (F10 or Alt)";
    case ui::CaptureTake::kKey:
        std::snprintf(text, sizeof text, "stores 0x%02X (%s)", a.number, in::KeyName(a.number).c_str());
        return text;
    }
    return "?";
}

// The keys where the two captures differ, known and explained.
struct Expected {
    int code;   // scan | ext << 7
    const char* why;
};
constexpr Expected kExpected[] = {
    {0xC6, "Ctrl+Pause (Break, E0 46): SDL reports it as Pause, so the drawn capture stores Pause's number (0xC5); "
           "the Win32 capture stores 0xC6"},
};

const Expected* ExpectedFor(int code) {
    for (const Expected& e : kExpected)
        if (e.code == code) return &e;
    return nullptr;
}

void TestEveryKey() {
    int keys = 0, equal = 0, sdl_unknown = 0;
    std::string unknown_list;
    for (int fn = 0; fn < 2; ++fn) {
        std::printf("\n%s cell: every key a WM_KEYDOWN can name (scan 0x01..0x7F, extended or not)\n",
                    fn ? "A Function keys" : "A binding");
        for (int ext = 0; ext < 2; ++ext)
            for (int scan = 1; scan < 0x80; ++scan) {
                const int code = scan | (ext << 7);
                const Answer w = Win32Take(scan, ext != 0, fn != 0), d = DrawnTake(scan, ext != 0, fn != 0);
                if (!fn) ++keys;
                if (w == d) {
                    if (!fn) ++equal;
                    continue;
                }
                if (SdlScancode(scan, ext != 0) == SDL_SCANCODE_UNKNOWN) {
                    // SDL on Windows has no scancode for the code: no keyboard key SDL knows.
                    if (!fn) {
                        ++sdl_unknown;
                        char t[8];
                        std::snprintf(t, sizeof t, " %02X", code);
                        unknown_list += t;
                    }
                    continue;
                }
                const Expected* e = ExpectedFor(code);
                std::printf("  differs: code 0x%02X: Win32 %s, drawn %s%s%s\n", code, Describe(w).c_str(),
                            Describe(d).c_str(), e ? "\n           " : "", e ? e->why : "");
                Check(e != nullptr, "a key where the two captures differ, not listed", code, d.number);
            }
    }
    std::printf("\n%d codes on a binding cell: %d answered alike; %d have no SDL scancode on Windows (the Win32 capture "
                "stores the code, the drawn one never sees a key):\n ", keys, equal, sdl_unknown);
    std::printf("%s\n", unknown_list.c_str());
}

// Every one of key_sdl.h's pairs: the physical key SDL names, pressed in either
// capture, gives one number.
void TestEveryPair() {
    int n = 0, alike = 0;
    const in::ScancodeDik* pairs = in::ScancodeDiks(&n);
    for (int i = 0; i < n; ++i) {
        int found = -1;
        for (int code = 1; code < 256 && found < 0; ++code) {
            if ((code & 0x7F) == 0) continue;
            if (SdlScancode(code & 0x7F, code > 0x7F) == pairs[i].scancode) found = code;
        }
        Check(found >= 0, "a pair whose key no message names", pairs[i].scancode, pairs[i].dik);
        if (found < 0) continue;
        const Answer w = Win32Take(found & 0x7F, found > 0x7F, false), d = DrawnTake(found & 0x7F, found > 0x7F, false);
        if (w == d) ++alike;
        else std::printf("  pair %d (DIK 0x%02X): Win32 %s, drawn %s\n", i, pairs[i].dik, Describe(w).c_str(), Describe(d).c_str());
    }
    std::printf("key_sdl.h's %d pairs: the key of each, pressed in either capture, gives the same answer for %d\n", n, alike);
    Check(alike == n, "a pair the two captures answer apart", alike, n);
    // The drawn launcher's check step bind-key names a key by KeyName (main.cpp):
    // every pair's key has a name that reads back as its number.
    int named = 0;
    for (int i = 0; i < n; ++i) {
        const bool back = in::KeyFromName(in::KeyName(pairs[i].dik)) == pairs[i].dik;
        Check(back, "a pair's key name does not read back", pairs[i].dik);
        named += back;
    }
    std::printf("bind-key: %d of the %d keys named by KeyName, each read back to its number\n", named, n);
}

// D427, fixed 2026-11-20: Pause and NumLock, each path and what the game reads. The stored number is
// DirectInput's in both captures, the game button's key byte is that number, and a function key's
// message reads as it on Windows and off it.
void TestD427() {
    std::printf("\nD427 (fixed), Pause and NumLock (VK_PAUSE 0x13 comes as scan 0x45 plain, VK_NUMLOCK 0x90 as 0x45 extended):\n");
    struct Key {
        const char* name;
        int scan;
        bool ext;
        unsigned vk;
        int dik;   // DirectInput's number of the key
    };
    constexpr Key keys[] = {{"Pause", 0x45, false, 0x13, 0xC5}, {"NumLock", 0x45, true, 0x90, 0x45}};
    for (const Key& k : keys) {
        const Answer w = Win32Take(k.scan, k.ext, false), d = DrawnTake(k.scan, k.ext, false);
        // A game button: the key bytes, by DirectInput's numbering.
        bool state[SDL_SCANCODE_COUNT] = {};
        state[SdlScancode(k.scan, k.ext)] = true;
        unsigned char bytes[in::kKeyStateBytes];
        in::FillKeyState(state, SDL_SCANCODE_COUNT, bytes);
        int byte = -1;
        for (int i = 0; i < in::kKeyStateBytes; ++i)
            if (bytes[i]) byte = i;
        // A function key: the key message's number, on Windows and off it.
        const std::int32_t lparam = (k.scan << 16) | (k.ext ? 1 << 24 : 0) | 1;
        const int fn_windows = in::FnKeyOfMessage(k.vk, lparam);
        const int fn_sdl = in::FnKeyOfMessage(k.vk, in::FnMessageBitsOfDik(k.dik) | 1);
        std::printf("  %-7s  Win32 capture %s; drawn capture %s\n", k.name, Describe(w).c_str(), Describe(d).c_str());
        std::printf("           the game, a game button: the key byte 0x%02X goes down (%s); a binding of 0x%02X (%s) "
                    "is worked by this key\n",
                    byte, in::KeyName(byte).c_str(), w.number, in::KeyName(w.number).c_str());
        std::printf("           the game, a function key: the key message reads as 0x%02X on Windows, 0x%02X off it "
                    "(%s): a function key bound by the capture works with this key\n",
                    fn_windows, fn_sdl, in::KeyName(fn_windows).c_str());
        Check(w == d, "the two captures store one number", w.number, d.number);
        Check(byte == k.dik, "the key byte is DirectInput's number", byte, k.dik);
        Check(byte == w.number, "D427: the game button's byte is the stored number", byte, w.number);
        Check(fn_windows == w.number && fn_sdl == w.number, "a function key's message reads as the stored number",
              fn_windows, fn_sdl);
    }
}

}  // namespace

int main() {
    std::printf("capture_tests (%d-bit)\n", static_cast<int>(sizeof(void*) * 8));
    TestEveryKey();
    std::printf("\n");
    TestEveryPair();
    TestD427();
    std::printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

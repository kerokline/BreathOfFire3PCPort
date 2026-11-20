// Host test of the keyboard's SDL scancode table (src/input/key_sdl.h;
// docs/linux-round.md, wave 2, R5). No SDL library, no game data: SDL's
// scancode header and SDL's own Windows table (its src/events, read as data).
//
// What a key gives, per path, for a key whose WM_KEYDOWN lParam carries the
// scan code `scan` and the extended flag `ext` (c = scan | ext << 7):
//   the launcher's capture   c (the Win32 capture as it was, removed 2026-11-20:
//                            src/launcher/config_dialog.cpp CaptureKeyHook)
//   SDL on Windows           windows_scancode_table[c], but Pause - c 0x45 -
//                            is looked up at 0xC6 (WindowsScanCodeToSDLScanCode)
//   DirectInput              the key's set-1 code, E0 as bit 7: c, but NumLock
//                            (c 0xC5, Windows flags it extended) is 0x45 and
//                            Pause (c 0x45; the code E1 1D 45) is 0xC5
// The checks: ours (SDL's scancode through key_sdl's table) is DirectInput's
// number for every key SDL on Windows knows; where that is not the capture's
// number, the key is in kCaptureDiffers; every key name of input/bindings.h
// is reachable; the table is one to one; FillKeyState writes 0x80 and 0 only.
//
//   cmake -S tools/key_host -B <scratch>/key-host -DSDL_SRC=<build>/_deps/sdl3-src
//   cmake --build <scratch>/key-host && <scratch>/key-host/key_tests
#include <cstdio>
#include <cstring>
#include <set>

#include "events/scancodes_windows.h"   // SDL's windows_scancode_table, through shim/SDL_internal.h
#include "input/bindings.h"
#include "input/key_codes.h"
#include "input/key_sdl.h"

namespace {

int g_checks = 0, g_failed = 0;

void Check(bool ok, const char* what, int a = 0, int b = 0) {
    ++g_checks;
    if (ok) return;
    ++g_failed;
    if (g_failed <= 40) std::printf("FAIL %s (0x%02X, 0x%02X)\n", what, a, b);
}

constexpr int kWindowsTableSize = static_cast<int>(sizeof windows_scancode_table / sizeof windows_scancode_table[0]);
static_assert(kWindowsTableSize == 256, "SDL's Windows table: 128 plain and 128 E0 codes");

// SDL on Windows, from a WM_KEYDOWN's c.
SDL_Scancode SdlFromCapture(int c) { return windows_scancode_table[c == 0x45 ? 0xC6 : c]; }

// DirectInput, from the same c.
int DirectInputFromCapture(int c) {
    if (c == 0xC5) return 0x45;   // NumLock
    if (c == 0x45) return 0xC5;   // Pause
    return c;
}

// What the launcher's capture stores for c: DirectInput's number (the capture
// crosses Pause and NumLock from how Windows sends them, input/function_keys.h
// FnKeyOfScan; D427, fixed 2026-11-20).
int CaptureStores(int c) { return DirectInputFromCapture(c); }

// The keys whose number in the game (DirectInput's, and now SDL's through the
// table) is not what the launcher's capture stores for the same key.
struct Differs {
    int capture;
    int ours;
    const char* key;
};
constexpr Differs kCaptureDiffers[] = {
    {0xC6, 0xC5, "Ctrl+Pause (Break, E0 46): SDL reports it as Pause; DirectInput gave 0xC6"},
};

const Differs* CaptureDiffers(int c) {
    for (const Differs& d : kCaptureDiffers)
        if (d.capture == c) return &d;
    return nullptr;
}

void TestAgainstSdlWindows() {
    namespace in = bof3x::input;
    int known = 0;
    for (int c = 1; c < 256; ++c) {
        const SDL_Scancode s = SdlFromCapture(c);
        if (s == SDL_SCANCODE_UNKNOWN) continue;
        ++known;
        const int ours = in::DikOfScancode(s);
        Check(ours != 0, "a key SDL on Windows reports has no DIK number", c, s);
        const int di = DirectInputFromCapture(c);
        // Break is the one key DirectInput tells apart and SDL does not.
        if (c != 0xC6) Check(ours == di, "ours is not DirectInput's number", c, ours);
        const Differs* d = CaptureDiffers(c);
        if (d) Check(ours == d->ours, "a listed capture difference is not as listed", c, ours);
        else Check(ours == CaptureStores(c), "ours is not the capture's number and the key is not listed", c, ours);
    }
    std::printf("SDL's Windows table: %d codes with a scancode (Pause at 0xC6), each with DirectInput's number\n", known);
    for (const Differs& d : kCaptureDiffers)
        std::printf("  capture 0x%02X -> game 0x%02X: %s\n", d.capture, d.ours, d.key);
}

void TestOneToOne() {
    namespace in = bof3x::input;
    int n = 0;
    const in::ScancodeDik* pairs = in::ScancodeDiks(&n);
    std::set<int> scancodes, diks;
    for (int i = 0; i < n; ++i) {
        Check(scancodes.insert(pairs[i].scancode).second, "an SDL scancode twice", pairs[i].scancode, pairs[i].dik);
        Check(diks.insert(pairs[i].dik).second, "a DIK number twice", pairs[i].scancode, pairs[i].dik);
        Check(in::DikOfScancode(pairs[i].scancode) == pairs[i].dik, "DikOfScancode", pairs[i].scancode, pairs[i].dik);
        Check(in::ScancodeOfDik(pairs[i].dik) == pairs[i].scancode, "ScancodeOfDik", pairs[i].scancode, pairs[i].dik);
        // Every pair is a key SDL on Windows can report (so none is a guess).
        bool on_windows = false;
        for (int c = 1; c < 256 && !on_windows; ++c) on_windows = SdlFromCapture(c) == pairs[i].scancode;
        Check(on_windows, "a pair SDL on Windows never reports", pairs[i].scancode, pairs[i].dik);
    }
    int without = 0;
    for (int s = 1; s < SDL_SCANCODE_COUNT; ++s)
        if (in::DikOfScancode(s) == 0) ++without;
    Check(in::DikOfScancode(SDL_SCANCODE_UNKNOWN) == 0 && in::DikOfScancode(-1) == 0 &&
              in::DikOfScancode(SDL_SCANCODE_COUNT) == 0, "out-of-range scancodes have no number");
    Check(in::ScancodeOfDik(0) == 0 && in::ScancodeOfDik(256) == 0 && in::ScancodeOfDik(-1) == 0,
          "out-of-range numbers have no scancode");
    std::printf("the table: %d pairs, one to one; %d of SDL's %d scancodes have no DIK number (no PC scan code)\n", n,
                without, SDL_SCANCODE_COUNT - 1);
}

void TestNames() {
    namespace in = bof3x::input;
    int reachable = 0;
    for (int i = 0; i < in::KeyCount(); ++i) {
        const in::KeyInfo& k = in::Keys()[i];
        const int s = in::ScancodeOfDik(k.dik);
        Check(s != 0, "a key name of bindings.h no SDL scancode reaches", k.dik);
        if (s) ++reachable;
    }
    std::printf("bindings.h: %d key names, %d reachable from SDL, %d not\n", in::KeyCount(), reachable,
                in::KeyCount() - reachable);
    // The numbers the engine compares by name (key_codes.h).
    struct Named {
        int dik;
        SDL_Scancode s;
    };
    const Named named[] = {
        {in::kDikEscape, SDL_SCANCODE_ESCAPE}, {in::kDikQ, SDL_SCANCODE_Q},     {in::kDikW, SDL_SCANCODE_W},
        {in::kDikReturn, SDL_SCANCODE_RETURN}, {in::kDikA, SDL_SCANCODE_A},     {in::kDikS, SDL_SCANCODE_S},
        {in::kDikZ, SDL_SCANCODE_Z},           {in::kDikX, SDL_SCANCODE_X},     {in::kDikC, SDL_SCANCODE_C},
        {in::kDikV, SDL_SCANCODE_V},           {in::kDikRShift, SDL_SCANCODE_RSHIFT}, {in::kDikSpace, SDL_SCANCODE_SPACE},
        {in::kDikNumpad7, SDL_SCANCODE_KP_7},  {in::kDikNumpad8, SDL_SCANCODE_KP_8}, {in::kDikNumpad9, SDL_SCANCODE_KP_9},
        {in::kDikNumpad4, SDL_SCANCODE_KP_4},  {in::kDikNumpad6, SDL_SCANCODE_KP_6}, {in::kDikNumpad1, SDL_SCANCODE_KP_1},
        {in::kDikNumpad2, SDL_SCANCODE_KP_2},  {in::kDikNumpad3, SDL_SCANCODE_KP_3}, {in::kDikF12, SDL_SCANCODE_F12},
        {in::kDikUp, SDL_SCANCODE_UP},         {in::kDikLeft, SDL_SCANCODE_LEFT}, {in::kDikRight, SDL_SCANCODE_RIGHT},
        {in::kDikDown, SDL_SCANCODE_DOWN},
    };
    for (const Named& n : named) Check(in::DikOfScancode(n.s) == n.dik, "key_codes.h's number", n.dik, n.s);
    // The default table's keys (Key_TableDefault's rule) all reachable.
    for (const in::KeyBinding& b : in::DefaultKeys()) Check(in::ScancodeOfDik(b.dik) != 0, "a default key unreachable", b.dik);
}

void TestFill() {
    namespace in = bof3x::input;
    bool sdl[SDL_SCANCODE_COUNT] = {};
    unsigned char ks[in::kKeyStateBytes];
    std::memset(ks, 0x55, sizeof ks);
    in::FillKeyState(sdl, SDL_SCANCODE_COUNT, ks);
    bool zero = true;
    for (unsigned char b : ks) zero = zero && b == 0;
    Check(zero, "nothing down fills zeros");
    // One key at a time: exactly its byte, 0x80.
    for (int s = 1; s < SDL_SCANCODE_COUNT; ++s) {
        sdl[s] = true;
        std::memset(ks, 0x55, sizeof ks);
        in::FillKeyState(sdl, SDL_SCANCODE_COUNT, ks);
        const int dik = in::DikOfScancode(s);
        int set = 0;
        for (int i = 0; i < in::kKeyStateBytes; ++i) {
            Check(ks[i] == 0 || ks[i] == 0x80, "a byte not 0 or 0x80", i, ks[i]);
            if (ks[i]) ++set;
        }
        Check(set == (dik ? 1 : 0) && (!dik || ks[dik] == 0x80), "one key down fills its byte alone", s, dik);
        sdl[s] = false;
    }
    // Everything down: one byte per pair.
    for (bool& b : sdl) b = true;
    in::FillKeyState(sdl, SDL_SCANCODE_COUNT, ks);
    int n = 0, set = 0;
    in::ScancodeDiks(&n);
    for (unsigned char b : ks) set += b == 0x80;
    Check(set == n, "every key down fills one byte per pair", set, n);
    // A short state array (an older SDL) reads no further than its count.
    in::FillKeyState(sdl, SDL_SCANCODE_A, ks);
    set = 0;
    for (unsigned char b : ks) set += b != 0;
    Check(set == 0, "a short array: nothing past its count");
    in::FillKeyState(nullptr, 0, ks);
    set = 0;
    for (unsigned char b : ks) set += b != 0;
    Check(set == 0, "no array: zeros");
}

// The keys Windows posts as WM_SYSKEYDOWN: Alt, F10, a key with Alt held.
void TestSystemKeys() {
    namespace in = bof3x::input;
    for (int s = 0; s < SDL_SCANCODE_COUNT; ++s) {
        const bool sys = s == SDL_SCANCODE_LALT || s == SDL_SCANCODE_RALT || s == SDL_SCANCODE_F10;
        Check(in::IsSystemKeyDown(s, false) == sys, "a key alone: Alt and F10 only", s);
        Check(in::IsSystemKeyDown(s, true), "a key with Alt held", s);
    }
}

}  // namespace

int main() {
    std::printf("key_tests (%d-bit)\n", static_cast<int>(sizeof(void*) * 8));
    TestAgainstSdlWindows();
    TestOneToOne();
    TestNames();
    TestFill();
    TestSystemKeys();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

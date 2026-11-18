// SDL3's scancodes as DirectInput key numbers (key_sdl.h).
//
// Where the numbers come from. DirectInput's keyboard (c_dfDIKeyboard) has
// 256 objects, object N the key whose set-1 scan code is N, an E0-prefixed
// code at N | 0x80, and Pause (E1 1D 45) at 0xC5 (dinput.h: DIK_PAUSE). SDL
// on Windows takes a key from WM_KEYDOWN / WM_KEYUP and looks it up in its
// table by the same index - the lParam's scan code, the extended flag as bit
// 7 (SDL_windowsevents.c WindowsScanCodeToSDLScanCode, scancodes_windows.h) -
// with two exceptions, both of the key 45: Windows flags NumLock (45) as
// extended, so NumLock comes to SDL as index 0xC5, and SDL rewrites Pause
// (45, not extended) as E0 46, index 0xC6. The pairs below are SDL's table
// read backwards, NumLock at DirectInput's 0x45 and Pause at its 0xC5; the
// host test (tools/key_host) holds them against SDL's own table.
//
// SDL's scancodes are the USB HID usages on every system, so the same pairs
// hold where SDL takes the keys from another source (evdev, X11, Wayland).
#include "input/key_sdl.h"

#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstring>

namespace bof3x::input {
namespace {

constexpr ScancodeDik kPairs[] = {
    {SDL_SCANCODE_ESCAPE, 0x01},
    {SDL_SCANCODE_1, 0x02},
    {SDL_SCANCODE_2, 0x03},
    {SDL_SCANCODE_3, 0x04},
    {SDL_SCANCODE_4, 0x05},
    {SDL_SCANCODE_5, 0x06},
    {SDL_SCANCODE_6, 0x07},
    {SDL_SCANCODE_7, 0x08},
    {SDL_SCANCODE_8, 0x09},
    {SDL_SCANCODE_9, 0x0A},
    {SDL_SCANCODE_0, 0x0B},
    {SDL_SCANCODE_MINUS, 0x0C},
    {SDL_SCANCODE_EQUALS, 0x0D},
    {SDL_SCANCODE_BACKSPACE, 0x0E},
    {SDL_SCANCODE_TAB, 0x0F},
    {SDL_SCANCODE_Q, 0x10},
    {SDL_SCANCODE_W, 0x11},
    {SDL_SCANCODE_E, 0x12},
    {SDL_SCANCODE_R, 0x13},
    {SDL_SCANCODE_T, 0x14},
    {SDL_SCANCODE_Y, 0x15},
    {SDL_SCANCODE_U, 0x16},
    {SDL_SCANCODE_I, 0x17},
    {SDL_SCANCODE_O, 0x18},
    {SDL_SCANCODE_P, 0x19},
    {SDL_SCANCODE_LEFTBRACKET, 0x1A},
    {SDL_SCANCODE_RIGHTBRACKET, 0x1B},
    {SDL_SCANCODE_RETURN, 0x1C},
    {SDL_SCANCODE_LCTRL, 0x1D},
    {SDL_SCANCODE_A, 0x1E},
    {SDL_SCANCODE_S, 0x1F},
    {SDL_SCANCODE_D, 0x20},
    {SDL_SCANCODE_F, 0x21},
    {SDL_SCANCODE_G, 0x22},
    {SDL_SCANCODE_H, 0x23},
    {SDL_SCANCODE_J, 0x24},
    {SDL_SCANCODE_K, 0x25},
    {SDL_SCANCODE_L, 0x26},
    {SDL_SCANCODE_SEMICOLON, 0x27},
    {SDL_SCANCODE_APOSTROPHE, 0x28},
    {SDL_SCANCODE_GRAVE, 0x29},
    {SDL_SCANCODE_LSHIFT, 0x2A},
    {SDL_SCANCODE_BACKSLASH, 0x2B},
    {SDL_SCANCODE_Z, 0x2C},
    {SDL_SCANCODE_X, 0x2D},
    {SDL_SCANCODE_C, 0x2E},
    {SDL_SCANCODE_V, 0x2F},
    {SDL_SCANCODE_B, 0x30},
    {SDL_SCANCODE_N, 0x31},
    {SDL_SCANCODE_M, 0x32},
    {SDL_SCANCODE_COMMA, 0x33},
    {SDL_SCANCODE_PERIOD, 0x34},
    {SDL_SCANCODE_SLASH, 0x35},
    {SDL_SCANCODE_RSHIFT, 0x36},
    {SDL_SCANCODE_KP_MULTIPLY, 0x37},
    {SDL_SCANCODE_LALT, 0x38},
    {SDL_SCANCODE_SPACE, 0x39},
    {SDL_SCANCODE_CAPSLOCK, 0x3A},
    {SDL_SCANCODE_F1, 0x3B},
    {SDL_SCANCODE_F2, 0x3C},
    {SDL_SCANCODE_F3, 0x3D},
    {SDL_SCANCODE_F4, 0x3E},
    {SDL_SCANCODE_F5, 0x3F},
    {SDL_SCANCODE_F6, 0x40},
    {SDL_SCANCODE_F7, 0x41},
    {SDL_SCANCODE_F8, 0x42},
    {SDL_SCANCODE_F9, 0x43},
    {SDL_SCANCODE_F10, 0x44},
    {SDL_SCANCODE_NUMLOCKCLEAR, 0x45},   // DIK_NUMLOCK; SDL on Windows meets it as 0xC5 (above)
    {SDL_SCANCODE_SCROLLLOCK, 0x46},
    {SDL_SCANCODE_KP_7, 0x47},
    {SDL_SCANCODE_KP_8, 0x48},
    {SDL_SCANCODE_KP_9, 0x49},
    {SDL_SCANCODE_KP_MINUS, 0x4A},
    {SDL_SCANCODE_KP_4, 0x4B},
    {SDL_SCANCODE_KP_5, 0x4C},
    {SDL_SCANCODE_KP_6, 0x4D},
    {SDL_SCANCODE_KP_PLUS, 0x4E},
    {SDL_SCANCODE_KP_1, 0x4F},
    {SDL_SCANCODE_KP_2, 0x50},
    {SDL_SCANCODE_KP_3, 0x51},
    {SDL_SCANCODE_KP_0, 0x52},
    {SDL_SCANCODE_KP_PERIOD, 0x53},
    {SDL_SCANCODE_NONUSBACKSLASH, 0x56},   // DIK_OEM_102, the key beside left Shift on ISO keyboards
    {SDL_SCANCODE_F11, 0x57},
    {SDL_SCANCODE_F12, 0x58},
    {SDL_SCANCODE_KP_EQUALS, 0x59},   // no DIK name (DIK_NUMPADEQUALS 0x8D is the NEC PC-98's)
    {SDL_SCANCODE_INTERNATIONAL6, 0x5C},
    {SDL_SCANCODE_F13, 0x64},
    {SDL_SCANCODE_F14, 0x65},
    {SDL_SCANCODE_F15, 0x66},
    {SDL_SCANCODE_F16, 0x67},
    {SDL_SCANCODE_F17, 0x68},
    {SDL_SCANCODE_F18, 0x69},
    {SDL_SCANCODE_F19, 0x6A},
    {SDL_SCANCODE_F20, 0x6B},
    {SDL_SCANCODE_F21, 0x6C},
    {SDL_SCANCODE_F22, 0x6D},
    {SDL_SCANCODE_F23, 0x6E},
    {SDL_SCANCODE_INTERNATIONAL2, 0x70},   // DIK_KANA
    {SDL_SCANCODE_LANG2, 0x71},
    {SDL_SCANCODE_LANG1, 0x72},
    {SDL_SCANCODE_INTERNATIONAL1, 0x73},   // DIK_ABNT_C1
    {SDL_SCANCODE_F24, 0x76},
    {SDL_SCANCODE_LANG4, 0x77},
    {SDL_SCANCODE_LANG3, 0x78},
    {SDL_SCANCODE_INTERNATIONAL4, 0x79},   // DIK_CONVERT
    {SDL_SCANCODE_INTERNATIONAL5, 0x7B},   // DIK_NOCONVERT
    {SDL_SCANCODE_INTERNATIONAL3, 0x7D},   // DIK_YEN
    {SDL_SCANCODE_KP_COMMA, 0x7E},         // DIK_ABNT_C2
    {SDL_SCANCODE_AGAIN, 0x87},
    {SDL_SCANCODE_UNDO, 0x88},
    {SDL_SCANCODE_PASTE, 0x8A},
    {SDL_SCANCODE_MEDIA_PREVIOUS_TRACK, 0x90},   // DIK_PREVTRACK
    {SDL_SCANCODE_CUT, 0x97},
    {SDL_SCANCODE_COPY, 0x98},
    {SDL_SCANCODE_MEDIA_NEXT_TRACK, 0x99},   // DIK_NEXTTRACK
    {SDL_SCANCODE_KP_ENTER, 0x9C},           // DIK_NUMPADENTER
    {SDL_SCANCODE_RCTRL, 0x9D},
    {SDL_SCANCODE_MUTE, 0xA0},
    {SDL_SCANCODE_MEDIA_PLAY_PAUSE, 0xA2},
    {SDL_SCANCODE_MEDIA_STOP, 0xA4},
    {SDL_SCANCODE_MEDIA_EJECT, 0xAC},
    {SDL_SCANCODE_VOLUMEDOWN, 0xAE},
    {SDL_SCANCODE_VOLUMEUP, 0xB0},
    {SDL_SCANCODE_AC_HOME, 0xB2},            // DIK_WEBHOME
    {SDL_SCANCODE_KP_DIVIDE, 0xB5},          // DIK_DIVIDE
    {SDL_SCANCODE_PRINTSCREEN, 0xB7},        // DIK_SYSRQ
    {SDL_SCANCODE_RALT, 0xB8},
    {SDL_SCANCODE_HELP, 0xBB},
    {SDL_SCANCODE_AC_NEW, 0xBE},
    {SDL_SCANCODE_AC_OPEN, 0xBF},
    {SDL_SCANCODE_AC_CLOSE, 0xC0},
    {SDL_SCANCODE_PAUSE, 0xC5},              // DIK_PAUSE; SDL on Windows meets it as 0xC6 (above)
    {SDL_SCANCODE_HOME, 0xC7},
    {SDL_SCANCODE_UP, 0xC8},
    {SDL_SCANCODE_PAGEUP, 0xC9},
    {SDL_SCANCODE_LEFT, 0xCB},
    {SDL_SCANCODE_RIGHT, 0xCD},
    {SDL_SCANCODE_END, 0xCF},
    {SDL_SCANCODE_DOWN, 0xD0},
    {SDL_SCANCODE_PAGEDOWN, 0xD1},
    {SDL_SCANCODE_INSERT, 0xD2},
    {SDL_SCANCODE_DELETE, 0xD3},
    {SDL_SCANCODE_AC_SAVE, 0xD7},
    {SDL_SCANCODE_AC_PRINT, 0xD8},
    {SDL_SCANCODE_LGUI, 0xDB},               // DIK_LWIN
    {SDL_SCANCODE_RGUI, 0xDC},               // DIK_RWIN
    {SDL_SCANCODE_APPLICATION, 0xDD},        // DIK_APPS
    {SDL_SCANCODE_POWER, 0xDE},
    {SDL_SCANCODE_SLEEP, 0xDF},
    {SDL_SCANCODE_WAKE, 0xE3},
    {SDL_SCANCODE_AC_SEARCH, 0xE5},          // DIK_WEBSEARCH
    {SDL_SCANCODE_AC_BOOKMARKS, 0xE6},       // DIK_WEBFAVORITES
    {SDL_SCANCODE_AC_REFRESH, 0xE7},
    {SDL_SCANCODE_AC_STOP, 0xE8},
    {SDL_SCANCODE_AC_FORWARD, 0xE9},
    {SDL_SCANCODE_AC_BACK, 0xEA},
    {SDL_SCANCODE_MEDIA_SELECT, 0xED},
};
constexpr int kPairCount = static_cast<int>(sizeof kPairs / sizeof kPairs[0]);

// The two lookups, built from the pairs at compile time; a scancode or a
// number given twice stops the build.
struct Tables {
    std::array<unsigned char, SDL_SCANCODE_COUNT> dik{};
    std::array<int, kKeyStateBytes> scancode{};
};

consteval Tables Build() {
    Tables t;
    for (const ScancodeDik& p : kPairs) {
        if (p.scancode <= SDL_SCANCODE_UNKNOWN || p.scancode >= SDL_SCANCODE_COUNT) throw "a scancode out of SDL's range";
        if (p.dik == 0) throw "DIK 0 is no key";
        if (t.dik[p.scancode] != 0) throw "an SDL scancode given two numbers";
        if (t.scancode[p.dik] != 0) throw "a DIK number given to two SDL scancodes";
        t.dik[p.scancode] = p.dik;
        t.scancode[p.dik] = p.scancode;
    }
    return t;
}
constexpr Tables kTables = Build();

consteval bool InDikOrder() {
    for (int i = 1; i < kPairCount; ++i)
        if (kPairs[i - 1].dik >= kPairs[i].dik) return false;
    return true;
}
static_assert(InDikOrder(), "the pairs in DIK order");

}  // namespace

const ScancodeDik* ScancodeDiks(int* count) {
    *count = kPairCount;
    return kPairs;
}

int DikOfScancode(int scancode) {
    if (scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_SCANCODE_COUNT) return 0;
    return kTables.dik[static_cast<unsigned>(scancode)];
}

int ScancodeOfDik(int dik) {
    if (dik <= 0 || dik >= kKeyStateBytes) return SDL_SCANCODE_UNKNOWN;
    return kTables.scancode[static_cast<unsigned>(dik)];
}

void FillKeyState(const bool* sdl_state, int count, unsigned char* key_state) {
    std::memset(key_state, 0, kKeyStateBytes);
    if (!sdl_state) return;
    for (const ScancodeDik& p : kPairs)
        if (p.scancode < count && sdl_state[p.scancode]) key_state[p.dik] = 0x80;
}

}  // namespace bof3x::input

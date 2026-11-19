// The keyboard on SDL3 (docs/linux-round.md, wave 2, group R5): SDL's
// scancodes as the DirectInput key numbers (DIK_*) the game's 256 key bytes
// Key_State are indexed by, and that BOF3.CFG's key table and bof3x.ini's
// key. items hold. The numbers are the configuration's and never change.
//
// A DIK number is the key's PC set-1 scan code with the E0 prefix as bit 7
// (Up is E0 48, DIK 0xC8), and Pause, whose code is E1 1D 45, is 0xC5. The
// table below gives each SDL scancode the number DirectInput gives the same
// physical key, so that the bytes filled from SDL's keyboard state are the
// bytes DirectInput filled: 0x80 for a key down, 0 for a key up, nothing
// else. An SDL scancode with no PC scan code (keys of other systems' keyboards
// only) has no number and is never in the bytes.
//
// Plain C++ over SDL's scancode enumeration alone: no SDL call, no system
// header, so the host test (tools/key_host) builds it on every system.
#pragma once

namespace bof3x::input {

constexpr int kKeyStateBytes = 256;   // Key_State: DirectInput's c_dfDIKeyboard, 256 bytes

// One SDL scancode and its DIK number.
struct ScancodeDik {
    int scancode;        // SDL_Scancode
    unsigned char dik;   // DIK_*, never 0
};

// Every pair, in DIK order; `count` receives how many.
const ScancodeDik* ScancodeDiks(int* count);

// The DIK number of an SDL scancode; 0 when it has none.
int DikOfScancode(int scancode);

// The SDL scancode of a DIK number; 0 (SDL_SCANCODE_UNKNOWN) when no SDL
// scancode gives it.
int ScancodeOfDik(int dik);

// The 256 key bytes from SDL's keyboard state (SDL_GetKeyboardState's array
// of `count` entries): every byte written, 0x80 where a key with that number
// is down, 0 elsewhere.
void FillKeyState(const bool* sdl_state, int count, unsigned char* key_state);

// Whether a key going down is one Windows posts as WM_SYSKEYDOWN and not as
// WM_KEYDOWN: Alt itself, F10, or any key while Alt is held (`alt_held`:
// SDL_KMOD_ALT in the event's modifiers). The game's key handler and the
// movie's skip took WM_KEYDOWN alone, so such a key never reached them; where
// the keys come from SDL's events (game_window.cpp's watch, off Windows) this
// is the test that keeps it so.
bool IsSystemKeyDown(int scancode, bool alt_held);

}  // namespace bof3x::input

// The game's window on a system that is not Windows: what
// game_window_win32.cpp is on Windows (game_window_native.h).
//
//   * The Win32 path (BOF3X_OFF=WindowSdl) does not exist here: each of its
//     calls stops the game with a sentence that names the switch (CLAUDE.md
//     rule 4), and so does Capcom's window procedure, which only that path
//     would register.
//   * The SDL path's steps are SDL's own: the keys and the focus come as SDL's
//     events, the drag keeps the picture's aspect by SDL_SetWindowAspectRatio,
//     the pointer is SDL's, the renderer takes the SDL window (DXVK-native,
//     docs/linux-round.md 6, call 1), Game_Hwnd holds the window's id.
//
// The Windows builds compile this file (the src/game glob takes it) to
// nothing.
#ifndef _WIN32

#include <SDL3/SDL.h>

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/platform/game_window_native.h"
#include "hook/log.h"

namespace game_window {

namespace {

[[noreturn]] void Refuse(const char* what) {
    bof3::Fatal("the Win32 window (%s) was asked for - BOF3X_OFF=WindowSdl or BOF3X_ORIGINAL=Game_WndProc - but it is "
                "Windows only; unset the switch (docs/linux-round.md, R6)",
                what);
}

}  // namespace

namespace win32 {
bool Register(void*) { Refuse("the window class"); }
void Desktop(int*, int*, int*) { Refuse("the desktop's size"); }
void WorkArea(Rect*) { Refuse("the work area"); }
unsigned SystemDpi() { Refuse("the system's DPI"); }
void* Create(void*, bool) { Refuse("the window"); }
bool Pump() { Refuse("the message pump"); }
void ApplyMode(void*) { Refuse("F8's mode change"); }
void ShowPointer(bool) { Refuse("the pointer"); }
bool ClientSize(void*, unsigned*, unsigned*) { Refuse("the client's size"); }
}  // namespace win32

namespace native {

void BeforeVideo() {}

void* AfterCreate(SDL_Window* window, void*) {
    const SDL_WindowID id = SDL_GetWindowID(window);
    if (id == 0) bof3::Fatal("game_window: SDL_GetWindowID: %s", SDL_GetError());
    void* handle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));   // Game_Hwnd is a 32-bit word
    Game_Hwnd = handle;
    return handle;
}

bool KeysAndFocusFromMessages() { return false; }

bool SizingFromMessages() { return false; }

void* RendererWindow(SDL_Window* window) { return window; }

void ShowPointer(bool show) {
    if (!(show ? SDL_ShowCursor() : SDL_HideCursor())) bof3::Log("game_window: SDL_%sCursor: %s", show ? "Show" : "Hide", SDL_GetError());
}

// DIV-0036's default k: the primary display's content scale as dots per inch
// (96 at 100 %), 96 before SDL's video is up.
unsigned SystemDpi() {
    const SDL_DisplayID d = SDL_GetPrimaryDisplay();
    const float scale = d ? SDL_GetDisplayContentScale(d) : 0.0f;
    return scale > 0.0f ? static_cast<unsigned>(scale * 96.0f + 0.5f) : 96;
}

void BeforeDestroy(SDL_Window*) {}

}  // namespace native

}  // namespace game_window

// Capcom's procedure and ours are the Win32 path's class procedure, which
// this system has not.
extern "C" std::int32_t __stdcall Game_WndProc(void*, unsigned int, unsigned int, std::int32_t) {
    bof3::Fatal("Game_WndProc: a Win32 window procedure was called, and this system has no Win32 window (docs/linux-round.md, R6)");
}

#endif  // !_WIN32

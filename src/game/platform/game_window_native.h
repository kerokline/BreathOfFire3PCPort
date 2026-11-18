// What game_window.cpp (the portable half, SDL3) and game_window_win32.cpp
// (Windows only) hand each other. Not for any other file: the game's calls
// are game_window.h. On a system that is not Windows,
// game_window_other.cpp defines the `win32` and `native` functions: the
// Win32 path refuses (a Fatal), the native helpers are SDL's.
#pragma once

#include <cstdint>

struct SDL_Window;

namespace game_window {

// DIVERGENCE DIV-0039: the window's title in English, in place of the GBK
// 0x65DA78 龙战士Ⅲ ("Breath of Fire III"). The original is GBK bytes handed
// to the ANSI calls, so outside a Chinese locale Windows shows it as mojibake
// (the title as "ÁúÕ½Ê¿¢ó"). This is Windows' chrome, not the game's text, so
// it does not follow BOF3X_LANG. (The missing-disc box: magic_080.cpp.)
constexpr const char* kTitle = "Breath of Fire III";

// A rectangle as the window system holds a window's: left, top, right,
// bottom, the frame included.
struct Rect {
    int left, top, right, bottom;
};

// ---- the portable half's services (game_window.cpp) -------------------------

// One thing the window system said, kept until the loop's message step
// (Pump) hands it to the game, in the order it came.
// Click (a left or right button going down) and Exposed (the client needs
// drawing again) are the movie's alone (TakeForMovie); Pump drops them.
enum class Got : std::uint8_t { Key, Activate, Moved, Sized, Close, Quit, Click, Exposed };
struct Event {
    Got what;
    std::int32_t a, b;   // Key: vk, lParam; Activate: 1 / 0; Moved: x, y; Sized: w, h
};
void Post(const Event& e);

// DIV-0042: the windowed placement. The last one is kept for F8 back from
// borderless and written to bof3x.window beside this module ("left top right
// bottom"); `on_a_monitor(x, y)` says whether a point is on a monitor.
void NoteWindowedRect(const Rect& r, bool save);
bool HaveWindowedRect(Rect* r);
void SavePlacement(const Rect& r);
// The computed placement (DIV-0036, DIV-0042): the saved one when it is
// usable, else the target's size times k centred on the desktop, k lowered
// while the frame would not fit the work area. `frame_w` and `frame_h` are
// what the windowed style adds to a client.
Rect WindowedRect(int frame_w, int frame_h, const Rect& work, bool (*on_a_monitor)(int x, int y));

// DIV-0042's drag rule (WM_SIZING): a windowed drag keeps the picture's shape.
// `rect` is the dragged window rectangle, changed in place; `frame_w` and
// `frame_h` as above. The edge is the one Windows names (WMSZ_*: 1 left, 2
// right, 3 top, 4 top-left, 5 top-right, 6 bottom, 7 bottom-left, 8
// bottom-right).
void SizingRule(unsigned edge, int frame_w, int frame_h, Rect* rect);

// ---- the Win32 path (game_window_win32.cpp; a Fatal elsewhere) -------------

namespace win32 {
bool Register(void* hinstance);
void Desktop(int* width, int* height, int* bpp);
void WorkArea(Rect* work);
unsigned SystemDpi();
void* Create(void* hinstance, bool borderless);
bool Pump();
void ApplyMode(void* hwnd);
void ShowPointer(bool show);
bool ClientSize(void* hwnd, unsigned* width, unsigned* height);
}  // namespace win32

// ---- the SDL path's system-specific steps ----------------------------------
// (game_window_win32.cpp on Windows, game_window_other.cpp elsewhere)

namespace native {
// Before SDL's video subsystem starts: the hints that keep the system's
// behaviour (on Windows: the process's DPI awareness as it is, the
// screensaver refused by the window as before).
void BeforeVideo();
// The window made, not yet shown: what Game_Hwnd holds, and the system's
// part of the window (on Windows: the icon, the black background, the
// procedure in front of SDL's for WM_SIZING, WM_EXITSIZEMOVE and
// WM_ACTIVATEAPP, the hook that reads WM_KEYDOWN as it was posted).
void* AfterCreate(SDL_Window* window, void* hinstance);
// True when the system's own messages carry the keys and the activation
// (Windows); false when SDL's events do.
bool KeysAndFocusFromMessages();
// True when the drag rule is applied by the system's own message (Windows'
// WM_SIZING); false when SDL's aspect ratio stands in for it.
bool SizingFromMessages();
// What the renderer is handed for this window.
void* RendererWindow(SDL_Window* window);
// The pointer shown or hidden over the window.
void ShowPointer(bool show);
// The system DPI (DIV-0036).
unsigned SystemDpi();
// Before SDL_DestroyWindow: the procedure and the hook taken out.
void BeforeDestroy(SDL_Window* window);
}  // namespace native

}  // namespace game_window

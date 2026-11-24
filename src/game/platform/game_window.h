// The game's window, its placement and its events (docs/linux-round.md, wave
// 2, group R6). Game_WinMain (magic_080.cpp) makes, sizes and pumps its
// window through these calls; two paths stand behind them:
//
//   * SDL3 (game_window.cpp), the default: SDL_CreateWindow, SDL's display
//     calls for the desktop, the work area and the monitors, SDL's events
//     for the moves, the sizes, the close and (off Windows) the focus and the
//     keys. The same code on every system.
//   * Win32 (game_window_win32.cpp), Windows only: the window class, the
//     procedure Game_WndProc and the one-message pump the port had before.
//     BOF3X_OFF=WindowSdl (the launcher's off=) keeps it, so that the two can
//     be compared; so does a hosted BOF3X_ORIGINAL=WindowSdl or
//     BOF3X_ORIGINAL=Game_WndProc (Capcom's procedure needs a window it is
//     the procedure of).
//
// What the window does for the game is one set of functions, below the
// paths (WinGame_*, magic_080.cpp): a key, the activation, the close, a new
// client size. Each path only turns its system's events into those calls.
#pragma once

#include <cstdint>
#include <vector>

namespace game_window {

// The switch: 1 through PatchBytes("WindowSdl") in WinMain_Inject; 0 when the
// code name is off. Read once, by Start.
extern std::uint8_t g_sdl;

// Called once by Game_WinMain before anything below, where the port
// registered its window class: decides the path (and logs it); registers the
// Win32 class, or brings up SDL's video subsystem. False when the class could
// not be registered (WinMain then returns -1, as the port's did). Fatal when
// SDL is asked for and its video cannot start.
bool Start(void* hinstance);

// True when the SDL path is the one in use (valid after Start).
bool SdlOn();

// The desktop of the primary display: its size in the window system's
// coordinates and its bits per pixel (Desktop_Width, Desktop_Height,
// Desktop_Bpp).
void Desktop(int* width, int* height, int* bpp);

// The work area of the primary display (left, top, right, bottom).
void WorkArea(int* left, int* top, int* right, int* bottom);

// The system's dots per inch for DIV-0036's default scale; 96 when the
// process is not told otherwise.
unsigned SystemDpi();

// Makes the window in its mode's style and placement (DIV-0032, DIV-0036,
// DIV-0042), shows it, and returns the handle Game_Hwnd holds: the HWND on
// Windows (both paths), the SDL window's id elsewhere. The Win32 path hands
// back what CreateWindowExA answered, null included, as the port went on
// with it; the SDL path is a Fatal when SDL cannot make the window.
void* Create(bool borderless);

// The loop's message step, where the port's PeekMessage stood: what came in
// since the last step is handed to the game (WinGame_*). False when the
// thread's quit message was seen (WM_QUIT).
bool Pump();

// F8 (DIV-0032): the window's style and placement follow Cfg_Fullscreen.
void ApplyMode();

// Cursor_Sync's half: the pointer shown over the window, or hidden.
void ShowPointer(bool show);

// The client size of `window` (a Game_Hwnd value, or the HWND Capcom's
// WinMain handed Display_Setup). False when the window is gone.
bool ClientSize(void* window, unsigned* width, unsigned* height);

// The one function that hands the renderer its window: render::Options.hwnd.
// For our window, an HWND on Windows and the SDL_Window on other systems
// (DXVK-native takes the SDL window as its HWND, docs/linux-round.md 6,
// call 1); any other window (Capcom's WinMain's) as it was handed in.
void* RendererWindow(void* window);

// After Display_Teardown: the window and SDL's video subsystem go.
void Destroy();

// ---- the movie's events (fmv_play.cpp; docs/linux-round.md, wave 2, R6m) ----
// While a movie plays on the SDL path, its loop takes the window's events
// here in place of Pump: the movie player no longer puts a procedure of its
// own in front of the window (on Windows the Win32 path keeps that, as the
// port had it). What came in since the last call, in order:
//   Key       a key going down (a: its Windows virtual-key number)
//   Click     the left or the right button going down over the window
//   Activate  the application activated (a 1) or deactivated (a 0); the
//             last one a movie takes is handed to the game too, first at
//             the next Pump (WinGame_Activate)
//   Repaint   the client needs drawing again
//   Close     the window was closed: the path's close has run (the placement
//             saved, the quit flag set, the window hidden), and every later
//             call starts with a Close, as the port's second movie met the
//             quit message the first one put back
//   Quit      the thread's quit message: handed back, and kept for Pump
// A move and a resize are handed to the game as Pump hands them. Fatal on the
// Win32 path, whose messages go to the movie's own procedure.
enum class MovieGot : std::uint8_t { Key, Click, Activate, Repaint, Close, Quit };
struct MovieEvent {
    MovieGot what;
    std::int32_t a;
};
std::vector<MovieEvent> TakeForMovie();

}  // namespace game_window

// The game's side of the window, shared by both paths (magic_080.cpp).
// WinGame_Key: a key going down, by its Windows virtual-key number and a
// WM_KEYDOWN lParam (bit 30: the key was already down - a repeat).
void WinGame_Key(unsigned vk, std::int32_t lparam);
// The application activated (true) or deactivated: DIV-0033's rule.
void WinGame_Activate(bool active);
// The window closed (WM_DESTROY's half): the quit flag. The placement is the
// path's to save.
void WinGame_Close();
// The client is now cw x ch (WM_SIZE's half, DIV-0042): the target's scale
// follows it, between frames.
void WinGame_ClientSized(unsigned cw, unsigned ch);

// ---- the input recipe's keys (wave 2, I1; hook/input_script.cpp) -------------
namespace game_window {
// A key going down, put in the window's queue as the SDL path's own keys are
// (the procedure in front of SDL's on Windows, SDL's key event elsewhere): the
// next Pump hands it to WinGame_Key, in the order it came, with `vk` and
// `lparam` as given (a WM_KEYDOWN's wParam and lParam). The input recipe's
// `key` step, which before this posted WM_KEYDOWN to the HWND; its WM_KEYUP
// went on to SDL's procedure alone, so a key's release has no call. Fatal when
// the SDL path is not in use: the Win32 window takes keys as messages
// (os::PostWindowKey).
void QueueKey(unsigned vk, std::int32_t lparam);
}  // namespace game_window

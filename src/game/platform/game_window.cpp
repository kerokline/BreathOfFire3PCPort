// The game's window on SDL3, and what both window paths share
// (game_window.h; docs/linux-round.md, wave 2, group R6).
//
// The SDL path, as it stands in for the Win32 one (game_window_win32.cpp):
//
//   CreateWindowExA, ShowWindow   SDL_CreateWindow, hidden; the frame read
//                                 (SDL_GetWindowBordersSize) so that the
//                                 placement is the window rectangle it was;
//                                 positioned, then shown
//   GetDeviceCaps, SPI_GETWORKAREA, MonitorFromPoint
//                                 SDL_GetDisplayBounds / UsableBounds of the
//                                 primary display, a walk of SDL_GetDisplays
//   WS_POPUP at the monitor (F8)  SDL_SetWindowFullscreen with no mode: SDL's
//                                 borderless desktop window on the window's
//                                 display
//   WM_MOVE, WM_SIZE, WM_DESTROY  SDL_EVENT_WINDOW_MOVED, _RESIZED,
//                                 _CLOSE_REQUESTED (and SDL_EVENT_QUIT)
//   WM_KEYDOWN, WM_ACTIVATEAPP    on Windows the messages themselves (the
//                                 hook and the procedure in front of SDL's,
//                                 game_window_win32.cpp), so that a key is
//                                 its virtual-key number and lParam as
//                                 posted; elsewhere SDL_EVENT_KEY_DOWN through
//                                 a scancode table, and the focus events
//   WM_SIZING                     on Windows the same message, in front of
//                                 SDL's procedure; elsewhere the picture's
//                                 aspect ratio (SDL_SetWindowAspectRatio)
//   PeekMessage                   SDL_PumpEvents, the one pump: SDL's event
//                                 watch takes what concerns the window as it
//                                 is queued, whichever caller pumped (the
//                                 pad's poll drains SDL's queue, pad_sdl.cpp)
//
// What is taken is kept in a small queue and handed to the game at the loop's
// message step (Pump), where the port's PeekMessage stood, so that a key
// still acts between the input latch and the frame as it did. While a movie
// plays, its loop takes the queue instead (TakeForMovie, R6m), with the two
// events only it reads: a click (WM_LBUTTONDOWN / WM_RBUTTONDOWN on Windows,
// SDL_EVENT_MOUSE_BUTTON_DOWN elsewhere) and SDL_EVENT_WINDOW_EXPOSED.
#include "game/platform/game_window.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/display_setup.h"
#include "game/platform/game_window_native.h"
#include "game/win_main.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/function_keys.h"
#include "input/key_sdl.h"
#include "os/os.h"

namespace game_window {

std::uint8_t g_sdl = 0;

namespace {

bool g_started = false;
bool g_on = false;              // the SDL path is in use
void* g_hinstance = nullptr;
SDL_Window* g_window = nullptr;
void* g_handle = nullptr;       // what Game_Hwnd holds
int g_border_top = 0, g_border_left = 0, g_border_bottom = 0, g_border_right = 0;   // the windowed frame

Rect g_windowed = {};           // the last windowed placement, for F8 back from borderless
bool g_have_windowed = false;

std::mutex g_queue_lock;        // SDL may queue from another thread; the window's events come from ours
std::vector<Event> g_queue;

// ---- the placement file (DIV-0042) ------------------------------------------

std::filesystem::path PlacementPath() {
    std::string self;
    if (!bof3::os::ModulePath(reinterpret_cast<const void*>(&PlacementPath), &self)) return {};
    const std::filesystem::path p = bof3::os::PathOfUtf8(self.c_str());
    if (p.empty()) return {};
    return p.parent_path() / "bof3x.window";
}

bool LoadPlacement(Rect* r, bool (*on_a_monitor)(int, int)) {
    const std::filesystem::path path = PlacementPath();
    if (path.empty()) return false;
    std::ifstream f(path);
    if (!f) return false;
    int v[4];
    if (!(f >> v[0] >> v[1] >> v[2] >> v[3])) return false;
    if (v[2] - v[0] < 64 || v[3] - v[1] < 64) return false;
    *r = {v[0], v[1], v[2], v[3]};
    return on_a_monitor(r->left + 8, r->top + 8);
}

// ---- SDL's displays ---------------------------------------------------------

SDL_DisplayID Primary() {
    const SDL_DisplayID d = SDL_GetPrimaryDisplay();
    if (d == 0) bof3::Fatal("game_window: SDL_GetPrimaryDisplay: %s", SDL_GetError());
    return d;
}

// MonitorFromPoint(MONITOR_DEFAULTTONULL): on a display, not the nearest.
bool OnADisplay(int x, int y) {
    int n = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&n);
    bool on = false;
    for (int i = 0; ids && i < n && !on; ++i) {
        SDL_Rect b;
        const SDL_Point p = {x, y};
        if (SDL_GetDisplayBounds(ids[i], &b) && SDL_PointInRect(&p, &b)) on = true;
    }
    SDL_free(ids);
    return on;
}

Rect SdlWorkArea() {
    SDL_Rect b;
    if (!SDL_GetDisplayUsableBounds(Primary(), &b)) return {0, 0, Desktop_Width, Desktop_Height};
    return {b.x, b.y, b.x + b.w, b.y + b.h};
}

int FrameW() { return g_border_left + g_border_right; }
int FrameH() { return g_border_top + g_border_bottom; }

// The window rectangle of the SDL window as it stands: its client and the frame.
Rect OuterNow() {
    int x = 0, y = 0, w = 0, h = 0;
    SDL_GetWindowPosition(g_window, &x, &y);
    SDL_GetWindowSize(g_window, &w, &h);
    return {x - g_border_left, y - g_border_top, x + w + g_border_right, y + h + g_border_bottom};
}

// A window rectangle as the SDL window's client.
void PlaceOuter(const Rect& r) {
    SDL_SetWindowSize(g_window, r.right - r.left - FrameW(), r.bottom - r.top - FrameH());
    SDL_SetWindowPosition(g_window, r.left + g_border_left, r.top + g_border_top);
}

Rect DisplayRect(SDL_DisplayID d) {
    SDL_Rect b;
    if (d == 0 || !SDL_GetDisplayBounds(d, &b)) return {0, 0, Desktop_Width, Desktop_Height};
    return {b.x, b.y, b.x + b.w, b.y + b.h};
}

// Off Windows, the picture's shape held by SDL while the window is dragged.
void HoldAspect() {
    if (native::SizingFromMessages()) return;
    const float a = static_cast<float>(DisplaySetup_ViewWidth()) / 240.0f;
    if (!SDL_SetWindowAspectRatio(g_window, a, a))
        bof3::Log("game_window: SDL_SetWindowAspectRatio: %s", SDL_GetError());
}

// Off Windows: SDL's scancode as the Windows virtual-key number the game's
// keys are compared with (key_codes.h, quick_menu.cpp, msgbox.cpp's study
// key, the recorder). By position, as a US keyboard; 0 for a key not here.
unsigned VkOf(SDL_Scancode s) {
    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) return 'A' + static_cast<unsigned>(s - SDL_SCANCODE_A);
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9) return '1' + static_cast<unsigned>(s - SDL_SCANCODE_1);
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F12) return 0x70 + static_cast<unsigned>(s - SDL_SCANCODE_F1);
    if (s >= SDL_SCANCODE_KP_1 && s <= SDL_SCANCODE_KP_9) return 0x61 + static_cast<unsigned>(s - SDL_SCANCODE_KP_1);
    switch (s) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_KP_0: return 0x60;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x0D;
    case SDL_SCANCODE_ESCAPE: return 0x1B;
    case SDL_SCANCODE_BACKSPACE: return 0x08;
    case SDL_SCANCODE_TAB: return 0x09;
    case SDL_SCANCODE_SPACE: return 0x20;
    case SDL_SCANCODE_LEFT: return 0x25;
    case SDL_SCANCODE_UP: return 0x26;
    case SDL_SCANCODE_RIGHT: return 0x27;
    case SDL_SCANCODE_DOWN: return 0x28;
    case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_RSHIFT: return 0x10;
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return 0x11;
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return 0x12;
    case SDL_SCANCODE_PAUSE: return 0x13;
    case SDL_SCANCODE_INSERT: return 0x2D;
    case SDL_SCANCODE_DELETE: return 0x2E;
    case SDL_SCANCODE_HOME: return 0x24;
    case SDL_SCANCODE_END: return 0x23;
    case SDL_SCANCODE_PAGEUP: return 0x21;
    case SDL_SCANCODE_PAGEDOWN: return 0x22;
    default: return 0;
    }
}

// SDL's event watch: called as each event is queued, by whichever call
// pumped. Only what concerns our window is kept.
bool SDLCALL Watch(void*, SDL_Event* e) {
    if (!g_window) return true;
    const SDL_WindowID id = SDL_GetWindowID(g_window);
    switch (e->type) {
    case SDL_EVENT_WINDOW_MOVED:
        if (e->window.windowID == id) Post({Got::Moved, e->window.data1, e->window.data2});
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        if (e->window.windowID == id) Post({Got::Sized, e->window.data1, e->window.data2});
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (e->window.windowID == id) Post({Got::Close, 0, 0});
        break;
    case SDL_EVENT_QUIT:
        Post({Got::Close, 0, 0});
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (e->window.windowID == id && !native::KeysAndFocusFromMessages())
            Post({Got::Activate, e->type == SDL_EVENT_WINDOW_FOCUS_GAINED ? 1 : 0, 0});
        break;
    case SDL_EVENT_KEY_DOWN:
        // A key Windows would post as WM_SYSKEYDOWN (Alt, F10, a key with Alt
        // held) is not a key-down to the game or to the movie's skip, as it
        // never was one there: the one test for both, since Pump and
        // TakeForMovie read what is posted here.
        if (e->key.windowID == id && !native::KeysAndFocusFromMessages() &&
            !bof3x::input::IsSystemKeyDown(e->key.scancode, (e->key.mod & SDL_KMOD_ALT) != 0)) {
            // The scan code and extended flag as Windows gives them (bits 16..24),
            // from key_sdl.h's DIK number: the function keys' table looks a key
            // that is not an F key up by them (input/function_keys.h).
            const std::int32_t lparam = 1 | (e->key.repeat ? (1 << 30) : 0) |
                                        bof3x::input::FnMessageBitsOfDik(bof3x::input::DikOfScancode(e->key.scancode));
            Post({Got::Key, static_cast<std::int32_t>(VkOf(e->key.scancode)), lparam});
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:   // the movie's skip (WM_LBUTTONDOWN, WM_RBUTTONDOWN)
        if (e->button.windowID == id && !native::KeysAndFocusFromMessages() &&
            (e->button.button == SDL_BUTTON_LEFT || e->button.button == SDL_BUTTON_RIGHT))
            Post({Got::Click, 0, 0});
        break;
    case SDL_EVENT_WINDOW_EXPOSED:      // the movie's WM_PAINT
        if (e->window.windowID == id) Post({Got::Exposed, 0, 0});
        break;
    default:
        break;
    }
    return true;
}

bool g_closed = false;   // a close was handed to the game

void Close() {
    if (g_closed) return;
    g_closed = true;
    bof3::Log("game_window: the window closed - quitting");
    // DIV-0042: the windowed placement outlives the window.
    if (!Cfg_Fullscreen) SavePlacement(OuterNow());
    else if (g_have_windowed) SavePlacement(g_windowed);
    WinGame_Close();
    // The port's window was gone at this point (DefWindowProc destroyed it);
    // ours stays for the teardown of the swap chain on it, out of sight.
    SDL_HideWindow(g_window);
}

}  // namespace

// ---- shared by both paths ---------------------------------------------------

void Post(const Event& e) {
    std::lock_guard<std::mutex> lock(g_queue_lock);
    g_queue.push_back(e);
}

void NoteWindowedRect(const Rect& r, bool save) {
    g_windowed = r;
    g_have_windowed = true;
    if (save) SavePlacement(r);
}

bool HaveWindowedRect(Rect* r) {
    if (g_have_windowed) *r = g_windowed;
    return g_have_windowed;
}

void SavePlacement(const Rect& r) {
    const std::filesystem::path path = PlacementPath();
    if (path.empty()) return;
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return;
    char line[64];
    std::snprintf(line, sizeof line, "%d %d %d %d\n", r.left, r.top, r.right, r.bottom);
    f << line;
}

// The original's windowed placement: a 640 x 480 client, the frame added by
// AdjustWindowRect, centred on the desktop by (desktop - outer) / 2 - the
// cdq / sub / sar sequence at 0x4FCCB1, a signed division toward zero, which
// C's integer division also is. DIV-0036: the client is the render target's
// size, 320k x 240k (426k x 240k wide, DIV-0041) - the target's own k once the
// display is up, the window size setting before - with k lowered while the
// frame would not fit the work area. At k = 2 that is the original's 640 x 480.
// DIV-0042: the last windowed placement, kept in bof3x.window beside this
// module, used when its frame still fits the work area and its top-left is on
// a monitor; else the computed placement.
Rect WindowedRect(int frame_w, int frame_h, const Rect& work, bool (*on_a_monitor)(int, int)) {
    Rect saved;
    if (LoadPlacement(&saved, on_a_monitor)) return saved;
    unsigned k = DisplaySetup_TargetScale();
    if (k == 0) k = DisplaySetup_WindowedScale();
    int w = 0, h = 0;
    for (;; --k) {
        w = static_cast<int>(DisplaySetup_ViewWidth() * k) + frame_w;   // 426k wide under DIV-0041
        h = static_cast<int>(240 * k) + frame_h;
        if (k <= 2 || (w <= work.right - work.left && h <= work.bottom - work.top)) break;
    }
    const int x = (Desktop_Width - w) / 2, y = (Desktop_Height - h) / 2;
    return {x, y, x + w, y + h};
}

void SizingRule(unsigned edge, int frame_w, int frame_h, Rect* r) {
    // DIV-0042: with snap the client lands on a whole multiple of the picture
    // (the nearest to the dragged height, 1..8); without, on the picture's
    // aspect at the dragged height (a side edge: at the dragged width).
    constexpr unsigned kLeft = 1, kRight = 2, kTop = 3, kTopLeft = 4, kTopRight = 5, kBottomLeft = 7;
    const int view_w = static_cast<int>(DisplaySetup_ViewWidth());
    int cw = (r->right - r->left) - frame_w, ch = (r->bottom - r->top) - frame_h;
    const bool by_width = edge == kLeft || edge == kRight;
    if (DisplaySetup_Snap()) {
        int k = by_width ? (cw + view_w / 2) / view_w : (ch + 120) / 240;
        if (k < 1) k = 1;
        if (k > 8) k = 8;
        cw = view_w * k;
        ch = 240 * k;
    } else {
        if (by_width) ch = cw * 240 / view_w;
        else cw = ch * view_w / 240;
        if (cw < view_w) cw = view_w, ch = 240;
    }
    const int w = cw + frame_w, h = ch + frame_h;
    // Anchor the edge the user is not dragging.
    if (edge == kLeft || edge == kTopLeft || edge == kBottomLeft) r->left = r->right - w;
    else r->right = r->left + w;
    if (edge == kTop || edge == kTopLeft || edge == kTopRight) r->top = r->bottom - h;
    else r->bottom = r->top + h;
}

// ---- the calls Game_WinMain makes ---------------------------------------------

bool Start(void* hinstance) {
    if (g_started) bof3::Fatal("game_window: Start twice");
    g_started = true;
    g_hinstance = hinstance;
    if (!g_sdl) {
        bof3::Log("game_window: the Win32 window (WindowSdl off)");
        return win32::Register(hinstance);
    }
    if (!bof3::IsEnabled(bof3::addr::Game_WndProc)) {
        // Capcom's window procedure is the class procedure of the Win32
        // path; SDL's window has SDL's, so the original would never run.
        bof3::Log("game_window: the Win32 window, since Game_WndProc is Capcom's (BOF3X_ORIGINAL); WindowSdl is not used");
        return win32::Register(hinstance);
    }
    if (!bof3::IsEnabled(bof3::addr::Fmv_Play) || !bof3::IsEnabled(bof3::addr::Fmv_WndProc)) {
        // Capcom's player, or Capcom's movie procedure, is put in front of
        // the window's procedure for the length of a movie (SetWindowLongA):
        // on SDL's window that would take SDL's procedure out, so the switch
        // keeps the window that procedure was written for.
        bof3::Log("game_window: the Win32 window, since %s is Capcom's (BOF3X_ORIGINAL); WindowSdl is not used",
                  bof3::IsEnabled(bof3::addr::Fmv_Play) ? "Fmv_WndProc" : "Fmv_Play");
        return win32::Register(hinstance);
    }
    native::BeforeVideo();
    // The borderless window stays where it is when the focus goes, as the
    // port's WS_POPUP did (SDL's default already, for a mode it did not set).
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) bof3::Fatal("game_window: SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
    if (!SDL_AddEventWatch(&Watch, nullptr)) bof3::Fatal("game_window: SDL_AddEventWatch: %s", SDL_GetError());
    g_on = true;
    bof3::Log("game_window: SDL3's window (video driver %s)", SDL_GetCurrentVideoDriver());
    return true;
}

bool SdlOn() { return g_on; }

void Desktop(int* width, int* height, int* bpp) {
    if (!g_on) return win32::Desktop(width, height, bpp);
    const SDL_DisplayID d = Primary();
    SDL_Rect b;
    if (!SDL_GetDisplayBounds(d, &b)) bof3::Fatal("game_window: SDL_GetDisplayBounds: %s", SDL_GetError());
    *width = b.w;
    *height = b.h;
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(d);
    // Bytes, not SDL_BITSPERPIXEL: an X-8-8-8 desktop is 32 to GetDeviceCaps, 24 to that.
    *bpp = mode ? 8 * SDL_BYTESPERPIXEL(mode->format) : 32;
}

void WorkArea(int* left, int* top, int* right, int* bottom) {
    Rect w;
    if (g_on) w = SdlWorkArea();
    else win32::WorkArea(&w);
    *left = w.left;
    *top = w.top;
    *right = w.right;
    *bottom = w.bottom;
}

// The same call on both paths (on Windows the port's), so that it may be
// asked before Start.
unsigned SystemDpi() { return native::SystemDpi(); }

void* Create(bool borderless) {
    if (!g_on) return win32::Create(g_hinstance, borderless);
    // DIVERGENCE DIV-0039: the title in English, not GBK bytes.
    g_window = SDL_CreateWindow(kTitle, 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (!g_window) bof3::Fatal("game_window: SDL_CreateWindow failed: %s", SDL_GetError());
    g_handle = native::AfterCreate(g_window, g_hinstance);
    if (!SDL_GetWindowBordersSize(g_window, &g_border_top, &g_border_left, &g_border_bottom, &g_border_right)) {
        g_border_top = g_border_left = g_border_bottom = g_border_right = 0;
        bof3::Log("game_window: the frame's size is not known yet (%s): the placement is the client's", SDL_GetError());
    }
    // DIV-0032: the window in its mode's style and placement.
    Rect r;
    if (borderless) {
        // The original's fullscreen window was the desktop's size at 0, 0: the
        // primary display, whatever the saved placement says.
        r = DisplayRect(Primary());
        SDL_SetWindowPosition(g_window, SDL_WINDOWPOS_CENTERED_DISPLAY(Primary()), SDL_WINDOWPOS_CENTERED_DISPLAY(Primary()));
        if (!SDL_SetWindowFullscreen(g_window, true)) bof3::Log("game_window: SDL_SetWindowFullscreen: %s", SDL_GetError());
    } else {
        r = WindowedRect(FrameW(), FrameH(), SdlWorkArea(), &OnADisplay);
        PlaceOuter(r);
    }
    HoldAspect();
    bof3::Log("DIV-0032    window %s: %d,%d %d x %d%s", borderless ? "borderless" : "windowed", r.left, r.top,
              r.right - r.left, r.bottom - r.top, win_main::g_background ? "; DIV-0033 runs unfocused" : "");
    if (!SDL_ShowWindow(g_window)) bof3::Log("game_window: SDL_ShowWindow: %s", SDL_GetError());
    SDL_SyncWindow(g_window);
    SDL_PumpEvents();
    int x = 0, y = 0;
    SDL_GetWindowPosition(g_window, &x, &y);
    Display_WindowMoved(x & 0xFFFF, y & 0xFFFF);   // WM_MOVE's words, as the port's window sent them
    // The port's ShowWindow activated the window inside the call: the
    // activation the show brought is handed to the game here, as then.
    Pump();
    return g_handle;
}

namespace {

// SDL pumped and what the watch queued taken, in order.
std::vector<Event> TakeQueue() {
    SDL_PumpEvents();
    std::vector<Event> got;
    std::lock_guard<std::mutex> lock(g_queue_lock);
    got.swap(g_queue);
    return got;
}

// What the watch took stays in SDL's queue for no one: dropped, so that it
// never fills. The pad's events are left for its poll (pad_sdl.cpp).
void FlushSdl() {
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_JOYSTICK_AXIS_MOTION - 1);
    SDL_FlushEvents(SDL_EVENT_FINGER_DOWN, SDL_EVENT_LAST);
}

void Moved(const Event& e) { Display_WindowMoved(e.a & 0xFFFF, e.b & 0xFFFF); }

void Sized(const Event& e) {
    if (e.a > 0 && e.b > 0 && !(SDL_GetWindowFlags(g_window) & SDL_WINDOW_MINIMIZED))
        WinGame_ClientSized(static_cast<unsigned>(e.a), static_cast<unsigned>(e.b));
}

}  // namespace

bool Pump() {
    if (!g_on) return win32::Pump();
    const std::vector<Event> got = TakeQueue();
    bool quit = false;
    for (const Event& e : got) {
        switch (e.what) {
        case Got::Key: WinGame_Key(static_cast<unsigned>(e.a), e.b); break;
        case Got::Activate: WinGame_Activate(e.a != 0); break;
        case Got::Moved: Moved(e); break;
        case Got::Sized: Sized(e); break;
        case Got::Close: Close(); break;
        case Got::Quit:
            bof3::Log("WinMain: WM_QUIT");
            quit = true;
            break;
        case Got::Click:     // the movie's alone
        case Got::Exposed:   // the game's presents draw the client
            break;
        }
    }
    FlushSdl();
    return !quit;
}

// DIV-0032: F8's half - the window's style and placement follow
// Cfg_Fullscreen; the display objects stay.
void ApplyMode() {
    if (!g_on) return win32::ApplyMode(static_cast<void*>(Game_Hwnd));
    Rect r;
    if (Cfg_Fullscreen) {
        if (!(SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN)) NoteWindowedRect(OuterNow(), false);
        if (!SDL_SetWindowFullscreen(g_window, true)) bof3::Log("game_window: SDL_SetWindowFullscreen: %s", SDL_GetError());
        SDL_SyncWindow(g_window);
        r = DisplayRect(SDL_GetDisplayForWindow(g_window));
    } else {
        if (!SDL_SetWindowFullscreen(g_window, false)) bof3::Log("game_window: SDL_SetWindowFullscreen: %s", SDL_GetError());
        SDL_SyncWindow(g_window);
        if (!HaveWindowedRect(&r)) r = WindowedRect(FrameW(), FrameH(), SdlWorkArea(), &OnADisplay);
        PlaceOuter(r);
        SDL_SyncWindow(g_window);
    }
    bof3::Log("DIV-0032    window %s: %d,%d %d x %d", Cfg_Fullscreen ? "borderless" : "windowed", r.left, r.top,
              r.right - r.left, r.bottom - r.top);
}

void ShowPointer(bool show) {
    if (!g_on) return win32::ShowPointer(show);
    native::ShowPointer(show);
}

bool ClientSize(void* window, unsigned* width, unsigned* height) {
    if (!g_on || window != g_handle) return win32::ClientSize(window, width, height);
    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(g_window, &w, &h)) return false;
    *width = static_cast<unsigned>(w);
    *height = static_cast<unsigned>(h);
    return true;
}

void* RendererWindow(void* window) {
    if (!g_on || window != g_handle) return window;
    return native::RendererWindow(g_window);
}

void Destroy() {
    if (!g_on || !g_window) return;
    native::BeforeDestroy(g_window);
    SDL_RemoveEventWatch(&Watch, nullptr);
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    bof3::Log("game_window: the window and SDL's video are down");
}

// ---- the movie's events (fmv_play.cpp, R6m) ----------------------------------

std::vector<MovieEvent> TakeForMovie() {
    if (!g_on) bof3::Fatal("game_window: TakeForMovie on the Win32 path, where the movie's own procedure takes its messages");
    const std::vector<Event> got = TakeQueue();
    std::vector<MovieEvent> out;
    if (g_closed) out.push_back({MovieGot::Close, 0});
    bool quit = false;
    for (const Event& e : got) {
        switch (e.what) {
        case Got::Key: out.push_back({MovieGot::Key, e.a}); break;
        case Got::Click: out.push_back({MovieGot::Click, 0}); break;
        case Got::Activate: out.push_back({MovieGot::Activate, e.a}); break;
        case Got::Exposed: out.push_back({MovieGot::Repaint, 0}); break;
        case Got::Moved: Moved(e); break;
        case Got::Sized: Sized(e); break;
        case Got::Close:
            Close();
            out.push_back({MovieGot::Close, 0});
            break;
        case Got::Quit:
            out.push_back({MovieGot::Quit, 0});
            quit = true;
            break;
        }
    }
    if (quit) Post({Got::Quit, 0, 0});   // for WinMain's loop, as the movie's pump put WM_QUIT back
    FlushSdl();
    return out;
}

}  // namespace game_window

// ---- the input recipe's keys (wave 2, I1; game_window.h) ---------------------
namespace game_window {
void QueueKey(unsigned vk, std::int32_t lparam) {
    if (!g_on) bof3::Fatal("game_window: a recipe key queued with the Win32 window in use (it takes window messages)");
    Post({Got::Key, static_cast<std::int32_t>(vk), lparam});
}
}  // namespace game_window

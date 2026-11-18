// The game's window on Windows: the Win32 path the port had (BOF3X_OFF=WindowSdl
// keeps it), and the Windows steps of the SDL path (game_window.h,
// game_window_native.h). A Windows file by design: it leaves the Linux source
// list (tools/linux_probe/linux_tree.py WINDOWS_ONLY), and
// game_window_other.cpp stands in for it there.
#include "game/platform/game_window_native.h"

#include <SDL3/SDL.h>
#include <windows.h>

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/display_setup.h"
#include "game/platform/game_window.h"
#include "game/win_main.h"
#include "hook/log.h"
#include "input/key_codes.h"
#include "os/os.h"

namespace game_window {

namespace {

// DIV-0032: the two styles. The original's windowed style was 0xCA0000
// (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX), not resizable.
constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;

constexpr DWORD kBorderlessStyle = WS_POPUP;

constexpr ::bof3::image::Va kStrClass{0x65DA84};   // "Bof3"

constexpr ::bof3::image::Va kStrIcon{0x65DA8C};    // "BOF3_ICON"

const char* StrImage(std::uint32_t address) { return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address)); }

Rect FromRect(const RECT& r) { return {static_cast<int>(r.left), static_cast<int>(r.top), static_cast<int>(r.right), static_cast<int>(r.bottom)}; }

RECT ToRect(const Rect& r) { return {r.left, r.top, r.right, r.bottom}; }

bool OnAMonitor(int x, int y) { return MonitorFromPoint({x, y}, MONITOR_DEFAULTTONULL) != nullptr; }

// What the windowed style adds to a client: AdjustWindowRect of an empty one.
void Frame(DWORD style, DWORD ex_style, int* w, int* h) {
    RECT frame = {0, 0, 0, 0};
    AdjustWindowRectEx(&frame, style, FALSE, ex_style);
    *w = frame.right - frame.left;
    *h = frame.bottom - frame.top;
}

Rect W32WindowedRect() {
    Rect work;
    win32::WorkArea(&work);
    int fw = 0, fh = 0;
    Frame(kWindowedStyle, 0, &fw, &fh);
    return WindowedRect(fw, fh, work, &OnAMonitor);
}

// DIV-0032: the borderless window covers the monitor the window is on, or
// the primary one before there is a window (the original's fullscreen window
// was the desktop's size at 0, 0, which on one monitor is the same thing).
RECT BorderlessRect(HWND hwnd) {
    HMONITOR mon = hwnd ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info = {};
    info.cbSize = sizeof info;
    if (mon && GetMonitorInfoA(mon, &info)) return info.rcMonitor;
    return {0, 0, Desktop_Width, Desktop_Height};
}

// WM_SIZING's WMSZ_* is the rule's edge number (game_window_native.h).
static_assert(WMSZ_LEFT == 1 && WMSZ_RIGHT == 2 && WMSZ_TOP == 3 && WMSZ_TOPLEFT == 4 && WMSZ_TOPRIGHT == 5 &&
              WMSZ_BOTTOM == 6 && WMSZ_BOTTOMLEFT == 7 && WMSZ_BOTTOMRIGHT == 8, "the WMSZ_ edge numbers");

// The frame is the windowed style's, as the port computed it; SDL's
// resizable window has the same style (SDL_windowswindow.c: STYLE_NORMAL |
// STYLE_RESIZABLE is WS_OVERLAPPEDWINDOW).
void Sizing(WPARAM edge, RECT* r) {
    int fw = 0, fh = 0;
    Frame(kWindowedStyle, 0, &fw, &fh);
    Rect dragged = FromRect(*r);
    SizingRule(static_cast<unsigned>(edge), fw, fh, &dragged);
    *r = ToRect(dragged);
}

}  // namespace

// ---- the Win32 path -----------------------------------------------------------

namespace win32 {

// The window procedure, the FMV player and the set-up are reached through
// Capcom's addresses, as the original reached them, so that each one's
// BOF3X_ORIGINAL switch holds on its own under our WinMain: the address is
// ours when injected and Capcom's when not.
bool Register(void* hinstance_) {
    HINSTANCE hinstance = static_cast<HINSTANCE>(hinstance_);
    WNDCLASSA wc = {};
    wc.style = 0;
    wc.lpfnWndProc = reinterpret_cast<WNDPROC>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Game_WndProc)));
    wc.hInstance = hinstance;
    wc.hIcon = LoadIconA(hinstance, StrImage(::bof3::Img(kStrIcon.v)));
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = StrImage(::bof3::Img(kStrClass.v));
    return RegisterClassA(&wc) != 0;
}

void Desktop(int* width, int* height, int* bpp) {
    HDC dc = GetDC(nullptr);
    *width = GetDeviceCaps(dc, HORZRES);
    *height = GetDeviceCaps(dc, VERTRES);
    *bpp = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
    ReleaseDC(nullptr, dc);
}

void WorkArea(Rect* work) {
    RECT r = {0, 0, Desktop_Width, Desktop_Height};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &r, 0);
    *work = FromRect(r);
}

// The default k follows the display's scale (display_setup.cpp,
// DefaultWindowedScale): GetDpiForSystem, Windows 10 1607 and later; absent,
// or 96, the system DC's LOGPIXELSX.
unsigned SystemDpi() {
    unsigned dpi = 96;
    if (HMODULE user32 = GetModuleHandleA("user32.dll")) {
        using Fn = UINT(WINAPI*)();
        if (auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForSystem"))))
            dpi = fn();
    }
    if (dpi == 96) {
        if (HDC dc = GetDC(nullptr)) {
            dpi = static_cast<unsigned>(GetDeviceCaps(dc, LOGPIXELSX));
            ReleaseDC(nullptr, dc);
        }
    }
    return dpi;
}

void* Create(void* hinstance, bool borderless) {
    // DIV-0032: the window in its mode's style and placement.
    const RECT r = borderless ? BorderlessRect(nullptr) : ToRect(W32WindowedRect());
    const DWORD style = borderless ? kBorderlessStyle : kWindowedStyle;
    HWND hwnd = CreateWindowExA(0, StrImage(::bof3::Img(kStrClass.v)), kTitle, style, r.left, r.top, r.right - r.left,
                                r.bottom - r.top, nullptr, nullptr, static_cast<HINSTANCE>(hinstance), nullptr);
    Game_Hwnd = hwnd;
    bof3::Log("DIV-0032    window %s: %ld,%ld %ld x %ld%s", borderless ? "borderless" : "windowed", r.left, r.top,
              r.right - r.left, r.bottom - r.top, win_main::g_background ? "; DIV-0033 runs unfocused" : "");
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return hwnd;
}

bool Pump() {
    MSG msg;
    if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            bof3::Log("WinMain: WM_QUIT");
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return true;
}

// DIV-0032: F8's half - the window's style and placement follow
// Cfg_Fullscreen; the display objects stay.
void ApplyMode(void* hwnd_) {
    HWND hwnd = static_cast<HWND>(hwnd_);
    RECT r;
    if (Cfg_Fullscreen) {
        if (!(GetWindowLongA(hwnd, GWL_STYLE) & WS_POPUP)) {
            GetWindowRect(hwnd, &r);
            NoteWindowedRect(FromRect(r), false);
        }
        r = BorderlessRect(hwnd);
        SetWindowLongA(hwnd, GWL_STYLE, static_cast<LONG>(kBorderlessStyle | WS_VISIBLE));
    } else {
        Rect have;
        r = ToRect(HaveWindowedRect(&have) ? have : W32WindowedRect());
        SetWindowLongA(hwnd, GWL_STYLE, static_cast<LONG>(kWindowedStyle | WS_VISIBLE));
    }
    SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOZORDER);
    bof3::Log("DIV-0032    window %s: %ld,%ld %ld x %ld", Cfg_Fullscreen ? "borderless" : "windowed", r.left, r.top,
              r.right - r.left, r.bottom - r.top);
}

void ShowPointer(bool show) { ShowCursor(show ? TRUE : FALSE); }

bool ClientSize(void* hwnd, unsigned* width, unsigned* height) {
    RECT client = {};
    const bool ok = GetClientRect(static_cast<HWND>(hwnd), &client) != 0;
    *width = static_cast<unsigned>(client.right - client.left);
    *height = static_cast<unsigned>(client.bottom - client.top);
    return ok;
}

}  // namespace win32

// ---- the SDL path's Windows steps ---------------------------------------------

namespace native {

namespace {

HWND g_hwnd = nullptr;
WNDPROC g_sdl_proc = nullptr;   // SDL's procedure, behind ours

// In front of SDL's procedure (WIN_WindowProc), for what SDL has no event for
// or turns into one that is not the same:
//   WM_KEYDOWN      the key as posted - its virtual-key number and lParam,
//                   which the game's keys, the recorder and the input
//                   recipe's posted keys are (input_script.cpp)
//   WM_ACTIVATEAPP  the application's activation, DIV-0033's word (SDL's
//                   focus is the window's)
//   WM_SIZING       DIV-0042's drag rule: whole multiples, which SDL's aspect
//                   ratio cannot say
//   WM_EXITSIZEMOVE the placement written at the end of a drag, as before
//   WM_SYSCOMMAND   the screensaver and the monitor's power-down refused while
//                   the window is in front, as the port did (SDL's own refusal
//                   is off, SDL_HINT_VIDEO_ALLOW_SCREENSAVER, since it holds the
//                   display on whether or not the window is in front)
//   WM_POWERBROADCAST  the suspend query refused, as the port did
// Everything goes on to SDL's procedure but WM_SIZING, which this answers,
// and the two refusals.
LRESULT CALLBACK FrontProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_KEYDOWN:
        Post({Got::Key, static_cast<std::int32_t>(wparam), static_cast<std::int32_t>(lparam)});
        break;
    case WM_ACTIVATEAPP:
        Post({Got::Activate, wparam ? 1 : 0, 0});
        break;
    case WM_SIZING:
        if (Cfg_Fullscreen) break;
        Sizing(wparam, reinterpret_cast<RECT*>(lparam));
        return TRUE;
    case WM_EXITSIZEMOVE:
        if (!Cfg_Fullscreen) {
            RECT r;
            if (GetWindowRect(hwnd, &r)) NoteWindowedRect(FromRect(r), true);
        }
        break;
    case WM_SYSCOMMAND: {
        const unsigned command = static_cast<unsigned>(wparam) & 0xFFF0;
        if (command == SC_SCREENSAVE || command == SC_MONITORPOWER) return 1;
        break;
    }
    case WM_POWERBROADCAST:
        if (wparam == 0) return BROADCAST_QUERY_DENY;   // PBT_APMQUERYSUSPEND
        break;
    default:
        break;
    }
    return CallWindowProcW(g_sdl_proc, hwnd, msg, wparam, lparam);
}

// SDL's pump sees every message the thread takes; the thread's quit message
// is the loop's (Fmv_Play puts one back when it takes it, as WinMain's loop
// left on it).
bool SDLCALL MessageHook(void*, MSG* msg) {
    if (msg->message == WM_QUIT) Post({Got::Quit, 0, 0});
    return true;
}

}  // namespace

void BeforeVideo() {
    // SDL declares the process per-monitor DPI aware unless told otherwise
    // (SDL_windowsvideo.c, WIN_InitDPIAwareness). "unaware" asks for what an
    // unaware process already is (the hosted BOF3.exe, which has no
    // manifest) and fails without effect where the manifest decided
    // (bof3x.exe's PerMonitorV2): either way the awareness is as before.
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "unaware");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
}

void* AfterCreate(SDL_Window* window, void* hinstance_) {
    HINSTANCE hinstance = static_cast<HINSTANCE>(hinstance_);
    g_hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!g_hwnd) bof3::Fatal("game_window: the SDL window has no HWND: %s", SDL_GetError());
    Game_Hwnd = g_hwnd;
    // The port's class icon, BOF3.exe's BOF3_ICON (DIV-0095's exe host sets
    // its own once a frame is drawn, exe_icon.cpp), and its black background.
    const char* icon = StrImage(::bof3::Img(kStrIcon.v));
    if (HICON big = LoadIconA(hinstance, icon)) SendMessageA(g_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    if (HANDLE little = LoadImageA(hinstance, icon, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0))
        SendMessageA(g_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(little));
    SetClassLongPtrA(g_hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(GetStockObject(BLACK_BRUSH)));
    g_sdl_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&FrontProc)));
    if (!g_sdl_proc) bof3::Fatal("game_window: the procedure in front of SDL's: error %u", static_cast<unsigned>(GetLastError()));
    SDL_SetWindowsMessageHook(&MessageHook, nullptr);
    return g_hwnd;
}

bool KeysAndFocusFromMessages() { return true; }

bool SizingFromMessages() { return true; }

void* RendererWindow(SDL_Window*) { return g_hwnd; }

void ShowPointer(bool show) { ShowCursor(show ? TRUE : FALSE); }

unsigned SystemDpi() { return win32::SystemDpi(); }

void BeforeDestroy(SDL_Window*) {
    SDL_SetWindowsMessageHook(nullptr, nullptr);
    if (g_hwnd && g_sdl_proc && IsWindow(g_hwnd)) SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_sdl_proc));
    g_hwnd = nullptr;
}

}  // namespace native

}  // namespace game_window

// ---- the port's window procedure (the Win32 path's class procedure) ----------

extern "C" std::int32_t __stdcall Game_WndProc(void* hwnd_, unsigned int msg, unsigned int wparam, std::int32_t lparam) {
    using namespace game_window;
    HWND hwnd = static_cast<HWND>(hwnd_);
    switch (msg) {
    case WM_DESTROY: {
        bof3::Log("WndProc: WM_DESTROY - quitting");
        // The loop leaves on Game_QuitFlag as well as on WM_QUIT. With SDL's
        // gamepad layer in the process (DIV-0050) the WM_QUIT that
        // PostQuitMessage sets can be taken by SDL's HIDAPI device discovery,
        // which pumps its message window from our thread on every update
        // (SDL_hidapi.c, PeekMessage / GetMessage on m_hwndMsg); the game then
        // ran on without a window (2026-09-24, closing at the title, 3 of 3).
        // The flag is what the original's Fmv_WndProc sets on WM_DESTROY too.
        WinGame_Close();
        // DIV-0042: the windowed placement outlives the window.
        Rect have;
        if (!Cfg_Fullscreen) {
            RECT r;
            if (GetWindowRect(hwnd, &r)) SavePlacement(FromRect(r));
        } else if (HaveWindowedRect(&have)) {
            SavePlacement(have);
        }
        PostQuitMessage(0);
        return 0;
    }
    case WM_MOVE:
        Display_WindowMoved(static_cast<int>(static_cast<std::uint32_t>(lparam) & 0xFFFF),
                            static_cast<int>(static_cast<std::uint32_t>(lparam) >> 16));
        return 0;
    case WM_SIZING:
        if (Cfg_Fullscreen) break;
        Sizing(wparam, reinterpret_cast<RECT*>(lparam));
        return 1;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED)
            WinGame_ClientSized(static_cast<std::uint32_t>(lparam) & 0xFFFF, static_cast<std::uint32_t>(lparam) >> 16);
        return 0;
    case WM_EXITSIZEMOVE:
        if (!Cfg_Fullscreen) {
            RECT r;
            if (GetWindowRect(hwnd, &r)) NoteWindowedRect(FromRect(r), true);
        }
        return 0;
    case WM_ACTIVATEAPP:
        if (wparam) SetFocus(hwnd);
        WinGame_Activate(wparam != 0);
        return 0;
    case WM_KEYDOWN:
        WinGame_Key(wparam, lparam);
        return 0;
    case WM_SYSKEYDOWN:
        if (wparam == VK_F10 || wparam == VK_MENU) return 1;
        break;
    case WM_SYSCOMMAND: {
        const unsigned int command = wparam & 0xFFF0;
        if (command == SC_SCREENSAVE || command == SC_MONITORPOWER) return 1;
        break;
    }
    case WM_POWERBROADCAST:
        if (wparam == 0) return static_cast<std::int32_t>(BROADCAST_QUERY_DENY);   // PBT_APMQUERYSUSPEND
        break;
    default:
        break;
    }
    return static_cast<std::int32_t>(DefWindowProcA(hwnd, msg, wparam, lparam));
}

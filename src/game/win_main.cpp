// The window and the frame loop, replaced (docs/window-modes.md). Five of
// Capcom's functions are taken over, read whole on 2026-09-23:
//
//   Game_WinMain          0x4FCB00..0x4FD02A (0x52A bytes)  int __stdcall (hinstance, hprev, cmdline, show)
//   Game_WndProc          0x4FC6F0..0x4FCAB4 (0x3C4 bytes)  long __stdcall (hwnd, msg, wparam, lparam)
//   Cursor_Sync           0x4FCAC0..0x4FCAFD (0x3D bytes)   void (void)
//   Display_WindowMoved   0x5A5130..0x5A515E (0x2E bytes)   void (int x, int y)
//   Display_DeviceName    0x5A6690..0x5A66A4 (0x14 bytes)   char *(int device)
//
// WinMain is the program: the disc probe, the window class and window, the
// two FMVs, the display set-up, and then the frame loop that paces logic
// frames at 30 Hz against a deadline kept in a 32-bit float (docs/windowed-mode.md,
// known-defects D5, DIV-0022), renders when it is early and skips when it is
// late, and runs the four cooperative tasks. WndProc is the keyboard's
// function keys (F7 renderer, F8 fullscreen, F9 pause, F11 frame rate, F12 a
// quick save to slot 0 - F7 and F11 are nothing since DIV-0040), the activation that freezes the loop, and the screensaver and
// suspend refusals. Both are written to the original's order, call for call,
// so that a frame of ours asks Capcom's code for exactly what a frame of the
// original did (the frame hash is the check), with these differences:
//
// DIVERGENCE DIV-0032 (docs/DIVERGENCE.md): the window modes. Windowed is a
// resizable WS_OVERLAPPEDWINDOW with a 640 x 480 client to start; fullscreen
// (Cfg_Fullscreen, BOF3.CFG line 1, F8) is a borderless WS_POPUP window the
// size of its monitor, and no exclusive display mode is ever set. F8 changes
// the window's style and size and nothing else - no Display_Teardown, no
// Display_Setup. (F7 cycled the device name without re-making the display
// until DIV-0040 made it nothing.) The three on-screen overlays (F7's device name, F11's frame
// rate, F12's Save OK) were TextOut through the back buffer's GDI device
// context, which the backend's surface does not have: they go to the log.
// The saved F8 restores the windowed size and position the player last had.
//
// DIVERGENCE DIV-0033: the game keeps running while its window is not in
// front (IDEAS I12). App_Active stays set on WM_ACTIVATEAPP deactivation,
// the sound is not paused, and while the window is not the foreground
// application the pad words are zeroed after Input_Latch (the DirectInput
// keyboard is DISCL_BACKGROUND: it reads keys typed into other windows).
// BOF3X_BACKGROUND=0 restores the freeze - and its replay.
//
// DIVERGENCE DIV-0034: frame debt is clamped. When the deadline is more
// than kMaxDebtMs behind the clock (a window drag, a suspend, the F9 pause
// under BOF3X_BACKGROUND=0 and the freeze it keeps), the deadline restarts
// from now as at the loop's start, so the time away is dropped rather than
// replayed as unrendered logic frames.
//
// Every original quirk not covered by those three is kept: the frame
// deadline in a float with its 2^32 wrap, Sound_Tick on every spin of the
// wait, the fps string once a second, the pause lines, F9's second press
// quitting to the title in game and the program on the title, WM_MOVE's
// window rect for Capcom's Blt, the F10 / Alt swallow, the screensaver and
// suspend refusals, and the task stack base taken from this function's own
// frame (Task_SetStackBase reads esp: the task stacks lie 16 KB below the
// loop, which is why the loop's locals stay small).
#include "game/win_main.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "game/display_setup.h"
#include "game/fmv_play.h"
#include "game/pause_text.h"
#include "hook/detour.h"
#include "hook/input_script.h"
#include "hook/log.h"
#include "render/render_d3d11.h"
#include "render/render_shim.h"

namespace {

using U = std::uint32_t;

// Strings in the image (.data, read 2026-09-23).
constexpr U kStrSaveOk = 0x65DA54;       // "Save OK"
constexpr U kStrFrameRate = 0x65DA5C;    // "Frame Rate = %d"
constexpr U kStrLogosAvi = 0x65DA6C;     // "LOGOS.AVI"
constexpr U kStrClass = 0x65DA84;        // "Bof3"
constexpr U kStrIcon = 0x65DA8C;         // "BOF3_ICON"

// DIVERGENCE DIV-0039: the window's title and the missing-disc box in English,
// in place of the GBK 0x65DA78 龙战士Ⅲ ("Breath of Fire III"), 0x65DA98
// 请插入龙战士Ⅲ光盘！ ("insert the disc!") and 0x65DAB0 错误 ("error").
// The originals are GBK bytes handed to the ANSI calls, so outside a Chinese
// locale Windows shows them as mojibake (the title as "ÁúÕ½Ê¿¢ó"). This is
// Windows' chrome, not the game's text, so it does not follow BOF3X_LANG.
constexpr const char* kTitle = "Breath of Fire III";
constexpr const char* kInsertDisc = "Please insert the Breath of Fire III disc.";
constexpr const char* kError = "Breath of Fire III";
constexpr U kStrCapcomAvi = 0x65DAB8;    // "CAPCOM.AVI"
constexpr U kStrBof3Exe = 0x65DAC4;      // "BOF3.EXE"
const char* Str(U address) { return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address)); }

// The pacing constants (.rdata): the first deadline's offset, the frame
// period, and the wrap the float deadline is held under.
constexpr double kFirstFrameMs = 33.34;        // double at 0x5C4220
// DIV-0047: the period is the PlayStation's NTSC frame, 1001 / 30 ms
// (29.970 a second), not the port's 33.334 (29.999); and the deadline is
// kept as a base plus a count of frames times the period, in a double -
// never in the float at 0x6BC628 (Frame_Deadline), whose spacing past
// 2^24 ms made the pace depend on the clock's size (known-defects D5;
// DIV-0022 keeps the clock small, this makes the size not matter). The
// float at 0x5C4214, 2^32, was the original's tick wrap; the clock is
// unwrapped into 64 bits below instead. BOF3X_FRAME_MS=n (tooling, 1..1000)
// sets another period: 33.334 is the port's own.
constexpr double kFrameMs = 1001.0 / 30.0;     // the original's: double 33.334 at 0x5C4218
double g_frame_ms = kFrameMs;
// DIV-0048: F1 toggles double speed - the period halved, so two logic
// frames run per frame of wall time. Logic counts frames and reads no
// clock, so what the game computes is the same; when drawing cannot keep
// up the loop skips presents as it does catching up after a stall. Read by
// the loop, which rebases the deadline when it changes.
int g_speed = 1;
bool g_fps_log = false;   // BOF3X_FPS_LOG=1 (tooling): drawn and logic frames a second, to the log
constexpr const char* kStrSpeed2 = "Speed x2";
constexpr const char* kStrSpeed1 = "Speed x1";
constexpr U kEnvBase = 0x903880, kEnvStride = 0x90;   // the two display-environment pairs
constexpr int kOverlayFrames = 0x78;

// DIV-0034: how far behind the clock the deadline may fall before it restarts.
constexpr double kMaxDebtMs = 500.0;

// DIV-0032: the two styles. The original's windowed style was 0xCA0000
// (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX), not resizable.
constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;
constexpr DWORD kBorderlessStyle = WS_POPUP;

bool g_background = true;     // DIV-0033 on; BOF3X_BACKGROUND=0 turns it off
bool g_foreground = true;     // the last WM_ACTIVATEAPP's word
bool g_loop_ours = false;     // set by Game_WinMain: WinMain_InputAllowed answers true until then
RECT g_windowed_rect = {};    // the last windowed placement, for F8 back from borderless
bool g_have_windowed_rect = false;

// GetTickCount through the import slot, as the original reads it: DIV-0022's
// clock lives in that slot.
DWORD Tick() { return reinterpret_cast<DWORD(WINAPI*)(void)>(Imp_GetTickCount)(); }

// The original's windowed placement: a 640 x 480 client, the frame added by
// AdjustWindowRect, centred on the desktop by (desktop - outer) / 2 - the
// cdq / sub / sar sequence at 0x4FCCB1, a signed division toward zero, which
// C's integer division also is. DIV-0036: the client is the render target's
// size, 320k x 240k (426k x 240k wide, DIV-0041) - the target's own k once the display is up, the window
// size setting before - with k lowered while the frame would not fit the
// work area. At k = 2 that is the original's 640 x 480.
// DIV-0042: the last windowed placement, kept in bof3x.window beside this
// dll ("left top right bottom" of the window rectangle), so that a resized
// window comes back at its size. Used when its frame still fits the work
// area and its top-left is on a monitor; else the computed placement.
std::wstring PlacementPath() {
    HMODULE self = nullptr;
    wchar_t path[MAX_PATH];
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&PlacementPath), &self) ||
        GetModuleFileNameW(self, path, MAX_PATH) == 0)
        return L"";
    std::wstring p = path;
    const size_t slash = p.find_last_of(L"\\/");
    return (slash == std::wstring::npos ? p : p.substr(0, slash)) + L"\\bof3x.window";
}

bool LoadPlacement(RECT& r) {
    const std::wstring path = PlacementPath();
    if (path.empty()) return false;
    FILE* f = _wfopen(path.c_str(), L"r");
    if (!f) return false;
    long v[4];
    const bool ok = fscanf(f, "%ld %ld %ld %ld", &v[0], &v[1], &v[2], &v[3]) == 4;
    fclose(f);
    if (!ok || v[2] - v[0] < 64 || v[3] - v[1] < 64) return false;
    r = {v[0], v[1], v[2], v[3]};
    if (!MonitorFromPoint({r.left + 8, r.top + 8}, MONITOR_DEFAULTTONULL)) return false;
    return true;
}

void SavePlacement(const RECT& r) {
    const std::wstring path = PlacementPath();
    if (path.empty()) return;
    FILE* f = _wfopen(path.c_str(), L"w");
    if (!f) return;
    fprintf(f, "%ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom);
    fclose(f);
}

RECT WindowedRect() {
    RECT saved;
    if (LoadPlacement(saved)) return saved;
    unsigned k = DisplaySetup_TargetScale();
    if (k == 0) k = DisplaySetup_WindowedScale();
    RECT work = {0, 0, Desktop_Width, Desktop_Height};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    RECT rc;
    for (;; --k) {
        rc = {0, 0, static_cast<LONG>(DisplaySetup_ViewWidth() * k), static_cast<LONG>(240 * k)};   // 426k wide under DIV-0041
        AdjustWindowRect(&rc, kWindowedStyle, FALSE);
        if (k <= 2 || (rc.right - rc.left <= work.right - work.left && rc.bottom - rc.top <= work.bottom - work.top)) break;
    }
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    const int x = (Desktop_Width - w) / 2, y = (Desktop_Height - h) / 2;
    return {x, y, x + w, y + h};
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

// DIV-0032: F8's half - the window's style and placement follow
// Cfg_Fullscreen; the display objects stay.
void ApplyMode(HWND hwnd) {
    RECT r;
    if (Cfg_Fullscreen) {
        if (!(GetWindowLongA(hwnd, GWL_STYLE) & WS_POPUP)) {
            GetWindowRect(hwnd, &g_windowed_rect);
            g_have_windowed_rect = true;
        }
        r = BorderlessRect(hwnd);
        SetWindowLongA(hwnd, GWL_STYLE, static_cast<LONG>(kBorderlessStyle | WS_VISIBLE));
    } else {
        r = g_have_windowed_rect ? g_windowed_rect : WindowedRect();
        SetWindowLongA(hwnd, GWL_STYLE, static_cast<LONG>(kWindowedStyle | WS_VISIBLE));
    }
    SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOZORDER);
    bof3::Log("DIV-0032    window %s: %ld,%ld %ld x %ld", Cfg_Fullscreen ? "borderless" : "windowed", r.left, r.top,
              r.right - r.left, r.bottom - r.top);
}

// DIV-0032: the overlays. Display_TextOut needs the back buffer's GDI
// device context; the backend's surface has none, so with the backend's
// back buffer in the slot the text is logged, and with Capcom's
// (BOF3X_ORIGINAL=Display_Setup) it is drawn as it was.
bool BackBufferIsOurs() {
    U n = 0;
    render::Surface* const* all = render::AllSurfaces(&n);
    for (U i = 0; i < n; ++i)
        if (static_cast<const void*>(all[i]) == DDraw_BackBuffer) return true;
    return false;
}

void Overlay(const char* text) {
    if (BackBufferIsOurs()) bof3::Log("overlay     %s", text);
    else Display_TextOut(0, 0, text);
}

}  // namespace

bool WinMain_InputAllowed() { return !g_loop_ours || !g_background || g_foreground; }

extern "C" void __cdecl Cursor_Sync(void) {
    if (Cfg_Fullscreen) {
        if (Cursor_Hidden == 1) {
            ShowCursor(FALSE);
            Cursor_Hidden = 0;
        }
    } else if (Cursor_Hidden == 0) {
        ShowCursor(TRUE);
        Cursor_Hidden = 1;
    }
}

extern "C" void __cdecl Display_WindowMoved(int x, int y) {
    Gfx_WindowRect[0] = x;
    Gfx_WindowRect[1] = y;
    Gfx_WindowRect[2] = x + Gfx_ScreenRect[2];   // 0x66B710, the right edge
    Gfx_WindowRect[3] = y + Gfx_ScreenRect[3];   // 0x66B714, the bottom
}

extern "C" char* __cdecl Display_DeviceName(int device) {
    return reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x6C3A68 + static_cast<U>(device) * 0x13C));
}

extern "C" long __stdcall Game_WndProc(void* hwnd_, unsigned int msg, unsigned int wparam, long lparam) {
    HWND hwnd = static_cast<HWND>(hwnd_);
    switch (msg) {
    case WM_DESTROY:
        // DIV-0042: the windowed placement outlives the window.
        if (!Cfg_Fullscreen) {
            RECT r;
            if (GetWindowRect(hwnd, &r)) SavePlacement(r);
        } else if (g_have_windowed_rect) {
            SavePlacement(g_windowed_rect);
        }
        PostQuitMessage(0);
        return 0;
    case WM_MOVE:
        Display_WindowMoved(static_cast<int>(static_cast<unsigned long>(lparam) & 0xFFFF),
                            static_cast<int>(static_cast<unsigned long>(lparam) >> 16));
        return 0;
    case WM_SIZING: {
        // DIV-0042: a windowed drag keeps the picture's shape. With snap the
        // client lands on a whole multiple of the picture (the nearest to the
        // dragged height, 1..8); without, on the picture's aspect at the
        // dragged height (a side edge: at the dragged width).
        if (Cfg_Fullscreen) break;
        RECT* r = reinterpret_cast<RECT*>(lparam);
        RECT frame = {0, 0, 0, 0};
        AdjustWindowRect(&frame, kWindowedStyle, FALSE);
        const LONG fw = frame.right - frame.left, fh = frame.bottom - frame.top;
        const LONG view_w = static_cast<LONG>(DisplaySetup_ViewWidth());
        LONG cw = (r->right - r->left) - fw, ch = (r->bottom - r->top) - fh;
        const bool by_width = wparam == WMSZ_LEFT || wparam == WMSZ_RIGHT;
        if (DisplaySetup_Snap()) {
            LONG k = by_width ? (cw + view_w / 2) / view_w : (ch + 120) / 240;
            if (k < 1) k = 1;
            if (k > 8) k = 8;
            cw = view_w * k;
            ch = 240 * k;
        } else {
            if (by_width) ch = cw * 240 / view_w;
            else cw = ch * view_w / 240;
            if (cw < view_w) cw = view_w, ch = 240;
        }
        const LONG w = cw + fw, h = ch + fh;
        // Anchor the edge the user is not dragging.
        if (wparam == WMSZ_LEFT || wparam == WMSZ_TOPLEFT || wparam == WMSZ_BOTTOMLEFT) r->left = r->right - w;
        else r->right = r->left + w;
        if (wparam == WMSZ_TOP || wparam == WMSZ_TOPLEFT || wparam == WMSZ_TOPRIGHT) r->top = r->bottom - h;
        else r->bottom = r->top + h;
        return 1;
    }
    case WM_SIZE:
        // DIV-0042: the target follows the client, between frames.
        if (wparam != SIZE_MINIMIZED && DisplaySetup_TargetScale() != 0) {
            const unsigned cw = static_cast<unsigned long>(lparam) & 0xFFFF, ch = static_cast<unsigned long>(lparam) >> 16;
            if (cw && ch) render::RequestScale(DisplaySetup_ScaleForClient(cw, ch));
        }
        return 0;
    case WM_EXITSIZEMOVE:
        if (!Cfg_Fullscreen) {
            RECT r;
            if (GetWindowRect(hwnd, &r)) {
                g_windowed_rect = r;
                g_have_windowed_rect = true;
                SavePlacement(r);
            }
        }
        return 0;
    case WM_ACTIVATEAPP:
        if (wparam) {
            if (App_WasInactive) Sound_ResumeAll();
            SetFocus(hwnd);
            App_Active = 1;
            g_foreground = true;
        } else {
            g_foreground = false;
            if (g_background) {
                // DIV-0033: nothing stops. The pads read zero from here on
                // (WinMain_InputAllowed) until the next activation.
            } else {
                App_WasInactive = 1;
                App_Active = 0;
                Sound_PauseAll();
            }
        }
        return 0;
    case WM_KEYDOWN: {
        const bool was_paused = Game_Paused != 0;
        if (was_paused && wparam != VK_F9) {
            Game_Paused = 0;
            Sound_ResumeAll();
        }
        if (wparam == VK_F9) {
            if (!was_paused) {
                Game_Paused = 1;
                Sound_PauseAll();
            } else {
                Game_Paused = 0;
                if (Title_LogoState == 0 && Title_Fade == 0) Game_RestartFlag = 1;
                else Game_QuitFlag = 1;
            }
            return 0;
        }
        if (wparam == VK_F8) {
            if (Desktop_Width > 640 && Desktop_Height > 480) {
                Display_Changed = 1;
                Cfg_Fullscreen ^= 1;
                ApplyMode(hwnd);   // DIV-0032: no teardown, no set-up
                Cursor_Sync();
            }
            return 0;
        }
        // DIVERGENCE DIV-0040: F7 (the next renderer device, re-made, and its
        // name on screen) and F11 (the frame-rate readout) do nothing - the
        // owner, 2026-09-23: not needed as keys. With the backend there is
        // one device, and neither readout could be drawn (DIV-0032). Like
        // any other key they still end a pause, above.
        if (wparam == VK_F12) {
            Save_QuickWrite();
            Overlay_Frames = kOverlayFrames;
            Overlay_Text = Str(kStrSaveOk);
            return 0;
        }
        // DIV-0048: F1 toggles double speed; a held key's repeats (bit 30 of
        // lparam, the previous state) do not toggle it back.
        if (wparam == VK_F1) {
            if ((lparam & (1 << 30)) == 0) {
                g_speed = g_speed == 1 ? 2 : 1;
                Overlay_Frames = kOverlayFrames;
                Overlay_Text = g_speed == 2 ? kStrSpeed2 : kStrSpeed1;
                bof3::Log("DIV-0048    speed x%d (F1) at Frame_Counter %lu", g_speed, static_cast<unsigned long>(Frame_Counter));
            }
            return 0;
        }
        return 0;
    }
    case WM_SYSKEYDOWN:
        if (wparam == VK_F10 || wparam == VK_MENU) return 1;
        break;
    case WM_SYSCOMMAND: {
        const unsigned int command = wparam & 0xFFF0;
        if (command == SC_SCREENSAVE || command == SC_MONITORPOWER) return 1;
        break;
    }
    case WM_POWERBROADCAST:
        if (wparam == 0) return static_cast<long>(BROADCAST_QUERY_DENY);   // PBT_APMQUERYSUSPEND
        break;
    default:
        break;
    }
    return static_cast<long>(DefWindowProcA(hwnd, msg, wparam, lparam));
}

extern "C" int __stdcall Game_WinMain(void* hinstance_, void* /*hprev*/, char* /*cmdline*/, int /*show*/) {
    HINSTANCE hinstance = static_cast<HINSTANCE>(hinstance_);
    g_loop_ours = true;
    {
        char text[32];
        if (GetEnvironmentVariableA("BOF3X_BACKGROUND", text, sizeof text) > 0 && text[0] == '0') g_background = false;
        const DWORD n = GetEnvironmentVariableA("BOF3X_FRAME_MS", text, sizeof text);
        if (n > 0) {
            char* end = nullptr;
            const double v = n < sizeof text ? std::strtod(text, &end) : 0.0;
            if (end == nullptr || *end != '\0' || !(v >= 1.0 && v <= 1000.0)) bof3::Fatal("BOF3X_FRAME_MS must be 1..1000");
            g_frame_ms = v;
        }
        if (GetEnvironmentVariableA("BOF3X_FPS_LOG", text, sizeof text) > 0 && text[0] == '1') g_fps_log = true;
    }

    if (!Disc_Probe(Str(kStrCapcomAvi), Str(kStrBof3Exe))) {
        MessageBoxA(nullptr, kInsertDisc, kError, MB_ICONHAND);   // DIV-0039
        return 1;
    }
    Game_HInstance = hinstance;

    // The window procedure, the FMV player and the set-up are reached through
    // Capcom's addresses, as the original reached them, so that each one's
    // BOF3X_ORIGINAL switch holds on its own under our WinMain: the address
    // is ours when injected and Capcom's when not.
    WNDCLASSA wc = {};
    wc.style = 0;
    wc.lpfnWndProc = reinterpret_cast<WNDPROC>(static_cast<std::uintptr_t>(bof3::addr::Game_WndProc));
    wc.hInstance = hinstance;
    wc.hIcon = LoadIconA(hinstance, Str(kStrIcon));
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = Str(kStrClass);
    if (!RegisterClassA(&wc)) return -1;

    Cfg_Load();
    {
        HDC dc = GetDC(nullptr);
        Desktop_Width = GetDeviceCaps(dc, HORZRES);
        Desktop_Height = GetDeviceCaps(dc, VERTRES);
        Desktop_Bpp = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
        ReleaseDC(nullptr, dc);
    }
    if (Desktop_Width == 640 && Desktop_Height == 480) Cfg_Fullscreen = 1;

    // DIV-0032: the window in its mode's style and placement.
    RECT r = Cfg_Fullscreen ? BorderlessRect(nullptr) : WindowedRect();
    const DWORD style = Cfg_Fullscreen ? kBorderlessStyle : kWindowedStyle;
    HWND hwnd = CreateWindowExA(0, Str(kStrClass), kTitle, style, r.left, r.top, r.right - r.left,
                                r.bottom - r.top, nullptr, nullptr, hinstance, nullptr);
    Game_Hwnd = hwnd;
    bof3::Log("DIV-0032    window %s: %ld,%ld %ld x %ld%s", Cfg_Fullscreen ? "borderless" : "windowed", r.left, r.top,
              r.right - r.left, r.bottom - r.top, g_background ? "; DIV-0033 runs unfocused" : "");
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    Cursor_Sync();

    bof3::orig::Fmv_Play(Str(kStrCapcomAvi), hwnd, Cfg_Fullscreen);
    bof3::orig::Fmv_Play(Str(kStrLogosAvi), hwnd, Cfg_Fullscreen);
    bof3::Log("WinMain: FMVs done, quit flag %d", Game_QuitFlag);

    // A quit during the FMVs returns at once (0x4FCD3A): nothing is set up,
    // so nothing is torn down.
    if (Game_QuitFlag) return 1;
    {
        const int error = bof3::orig::Display_Setup(hwnd, &Cfg_Fullscreen, &Cfg_RenderMode, nullptr);
        if (error) {
            bof3::Log("WinMain: Display_Setup returned %d - the error box, then exit", error);
            Display_Teardown(hwnd);
            Display_ErrorBox(error);
            return 1;
        }
        Cursor_Sync();
        Game_Init();   // ends in Task_SetStackBase: the task stacks hang from this frame
        bof3::Log("WinMain: display up, Game_Init done, entering the loop");

        // The loop. Small locals on purpose (see the header comment).
        char fps[0x50] = {};
        int frames_drawn = 0;
        DWORD last_fps_tick = 0;
        unsigned long logic_at_fps_tick = 0;
        MSG msg;
        bool quit = false;
        // DIV-0047: the clock unwrapped into 64 bits (the slot's DWORD wraps
        // at 49.7 days), and the deadline as base + frames * period.
        DWORD last_tick = Tick();
        std::uint64_t now64 = 0;
        const auto Now = [&]() {
            const DWORD t = Tick();
            now64 += static_cast<DWORD>(t - last_tick);
            last_tick = t;
            return static_cast<double>(now64);
        };
        double deadline_base = 0.0;
        std::uint64_t deadline_frames = 0;
        int speed = 1;   // the period in force: g_frame_ms / speed (DIV-0048)
        while (!quit) {
            Task_Create(0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(bof3::addr::Boot_Task)));
            deadline_base = Now() + kFirstFrameMs;
            deadline_frames = 0;
            for (;;) {
                if (Game_QuitFlag) {
                    quit = true;
                    break;
                }
                if (!Game_Paused) bof3::InputScript_Latch();
                if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        quit = true;
                        break;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                }
                if (!App_Active) continue;
                if (Game_RestartFlag) {
                    Game_RestartFlag = 0;
                    Task_SetStackBase();
                    break;   // Task_Create again
                }
                if (g_speed != speed) {
                    // DIV-0048: the count so far at the old period becomes the
                    // base, so the change starts from the current deadline.
                    deadline_base += static_cast<double>(deadline_frames) * (g_frame_ms / speed);
                    deadline_frames = 0;
                    speed = g_speed;
                }
                double now = Now();
                double deadline = deadline_base + static_cast<double>(deadline_frames) * (g_frame_ms / speed);
                // DIV-0034: the debt clamp.
                if (now - deadline > kMaxDebtMs) {
                    bof3::Log("DIV-0034    frame deadline %.0f ms behind at Frame_Counter %lu: restarted",
                              now - deadline, static_cast<unsigned long>(Frame_Counter));
                    deadline_base = now + kFirstFrameMs;
                    deadline_frames = 0;
                    deadline = deadline_base;
                }
                if (now < deadline) {
                    unsigned char* env = Gfx_CurrentEnv;
                    Gpu_PutDispEnv(env);
                    Gpu_PutDrawEnv(env + 0x14);
                    Gfx_FlushDirtyStrip();
                    Gfx_FlushUploadQueue();
                    Gfx_DrawOTag(reinterpret_cast<unsigned long*>(env + 0x8C));
                    if (Overlay_Frames != 0) {
                        Overlay(Overlay_Text);
                        Overlay_Frames -= 1;
                    } else if (Fps_Shown) {
                        Overlay(fps);
                    }
                    ++frames_drawn;
                }
                // The wait: Sound_Tick on every spin, at least once a frame.
                for (;;) {
                    Sound_Tick();
                    now = Now();
                    if (!(now < deadline)) break;
                }
                if (last_tick - last_fps_tick > 1000) {
                    last_fps_tick = last_tick;
                    Crt_sprintf(fps, Str(kStrFrameRate), frames_drawn);
                    if (g_fps_log)
                        bof3::Log("fps         %d drawn, %lu logic, speed x%d", frames_drawn,
                                  static_cast<unsigned long>(Frame_Counter - logic_at_fps_tick), speed);
                    logic_at_fps_tick = Frame_Counter;
                    frames_drawn = 0;
                }
                // DIV-0047: one more frame of the period. The original added
                // 33.334 to the float at 0x6BC628 and held it under 2^32
                // (0x4FCF0F..0x4FCF3A); that float is not written here.
                ++deadline_frames;
                Gfx_BufferIndex ^= 1;
                unsigned char* env = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(kEnvBase + Gfx_BufferIndex * kEnvStride));
                Gfx_CurrentEnv = env;
                Gpu_ClearOTagR(reinterpret_cast<unsigned long*>(env + 0x70), 8);
                Gfx_BeginFrame();
                SpriteCell_Reset();
                if (!Game_Paused) {
                    Task_RunAll();
                } else {
                    const unsigned char* const* lines = (Title_LogoState == 0 && Title_Fade == 0) ? Pause_LinesGame : Pause_LinesTitle;
                    // DIV-0038: the English lines are centred on their width
                    Text_DrawAt(PauseText_X(lines[0], 100), 100, 0, 100, lines[0]);
                    Text_DrawAt(PauseText_X(lines[1], 0x70), 0x80, 0, 100, lines[1]);
                }
                Gfx_LinkOTags();
                Frame_Counter += 1;
            }
        }
    }
    Display_Teardown(hwnd);
    Sound_Shutdown();
    DInput_Shutdown();
    return 1;
}

// --- BOF3X_EXITTRACE: who ends the process ---------------------------------
//
// Tooling. A run that ends without a CRASH line or a FATAL line left through
// the game's own code - a return from WinMain, the CRT's exit, a WM_QUIT -
// and nothing says from where. With BOF3X_EXITTRACE set, the exe's import
// slots for ExitProcess, TerminateProcess, PostQuitMessage and MessageBoxA
// are pointed at loggers that write the caller's return address and the
// first dozen return addresses found on the stack inside the image, then go
// on to the real import; and a last vectored handler logs any breakpoint or
// single step that no other handler took. Nothing of Capcom's is changed:
// the slots are the loader's.
namespace {

constexpr U kImpExitProcess = 0x5C40B8, kImpTerminateProcess = 0x5C4114, kImpPostQuitMessage = 0x5C4158;
void (WINAPI* g_real_exit)(UINT);
BOOL (WINAPI* g_real_terminate)(HANDLE, UINT);
void (WINAPI* g_real_post_quit)(int);

void LogStack(const char* what, const void* frame) {
    char line[1024];
    int n = std::snprintf(line, sizeof line, "exittrace   %s from", what);
    const U* p = static_cast<const U*>(frame);
    int found = 0;
    for (int i = 0; i < 4096 && found < 12; ++i) {
        const U v = p[i];
        if (v >= 0x401000 && v < 0x5C4000) {
            n += std::snprintf(line + n, sizeof line - static_cast<size_t>(n), " 0x%06X", v);
            ++found;
        }
    }
    bof3::Log("%s", line);
    bof3::LogFlush();
}

void WINAPI TracedExitProcess(UINT code) {
    bof3::Log("exittrace   ExitProcess(0x%08X)", code);
    LogStack("ExitProcess", __builtin_frame_address(0));
    g_real_exit(code);
}

// The CRT's own __except in WinMainCRTStartup (0x5BA154) turns an exception
// nobody handled into _exit(code) with no dialog, and the crash reporter
// skips breakpoints and single steps, which are signals, not faults. This
// handler is called last, so it sees only what every other vectored handler
// passed on - the calltrace's among them.
LONG CALLBACK TracedUnhandled(EXCEPTION_POINTERS* info) {
    static volatile LONG reports = 0;
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_SINGLE_STEP && code != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&reports) > 4) return EXCEPTION_CONTINUE_SEARCH;
    bof3::Log("exittrace   exception 0x%08lX at 0x%08lX, esp 0x%08lX, eflags 0x%08lX, thread %lu, passed on by every handler",
              code, static_cast<unsigned long>(info->ContextRecord->Eip), static_cast<unsigned long>(info->ContextRecord->Esp),
              static_cast<unsigned long>(info->ContextRecord->EFlags), GetCurrentThreadId());
    LogStack("the exception", reinterpret_cast<const void*>(static_cast<std::uintptr_t>(info->ContextRecord->Esp)));
    return EXCEPTION_CONTINUE_SEARCH;
}
BOOL WINAPI TracedTerminateProcess(HANDLE h, UINT code) {
    LogStack("TerminateProcess", __builtin_frame_address(0));
    return g_real_terminate(h, code);
}
void WINAPI TracedPostQuitMessage(int code) {
    LogStack("PostQuitMessage", __builtin_frame_address(0));
    g_real_post_quit(code);
}
constexpr U kImpMessageBoxA = 0x5C4174;
int (WINAPI* g_real_message_box)(HWND, const char*, const char*, UINT);
int WINAPI TracedMessageBoxA(HWND h, const char* text, const char* caption, UINT type) {
    LogStack("MessageBoxA", __builtin_frame_address(0));
    const int r = g_real_message_box(h, text, caption, type);
    bof3::Log("exittrace   MessageBoxA returned %d", r);
    return r;
}

// Written directly, not through PatchBytes: this is an instrument, and
// BOF3X_ORIGINAL=* must not switch it off (the RetargetCall `instrument`
// rule, HANDOFF Traps 2026-09-23).
template <typename F>
void PatchImport(U slot, F& real, F ours) {
    void* at = reinterpret_cast<void*>(static_cast<std::uintptr_t>(slot));
    std::memcpy(&real, at, 4);
    DWORD old = 0;
    if (!VirtualProtect(at, 4, PAGE_READWRITE, &old)) bof3::Fatal("exittrace: VirtualProtect of the import slot 0x%08X failed", slot);
    std::memcpy(at, &ours, 4);
    VirtualProtect(at, 4, old, &old);
}

}  // namespace

void WinMain_Inject() {
    if (GetEnvironmentVariableA("BOF3X_EXITTRACE", nullptr, 0) > 0) {
        PatchImport(kImpExitProcess, g_real_exit, &TracedExitProcess);
        PatchImport(kImpTerminateProcess, g_real_terminate, &TracedTerminateProcess);
        PatchImport(kImpPostQuitMessage, g_real_post_quit, &TracedPostQuitMessage);
        PatchImport(kImpMessageBoxA, g_real_message_box, &TracedMessageBoxA);
        if (!AddVectoredExceptionHandler(0, TracedUnhandled)) bof3::Fatal("exittrace: no vectored handler");
        bof3::Log("exittrace   ExitProcess, TerminateProcess, PostQuitMessage and MessageBoxA log their callers; "
                  "breakpoints and single steps no handler took are logged");
    }
    BOF3_INJECT(Game_WinMain);
    BOF3_INJECT(Game_WndProc);
    BOF3_INJECT(Cursor_Sync);
    BOF3_INJECT(Display_WindowMoved);
    BOF3_INJECT(Display_DeviceName);
}

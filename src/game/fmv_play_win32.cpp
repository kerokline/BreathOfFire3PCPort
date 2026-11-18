// The movie player's Windows half (fmv_play_native.h; docs/linux-round.md,
// wave 2, R6m): the picture by GDI on the window, as DIV-0205 built it, and
// the Win32 window path's procedure and pump (BOF3X_OFF=WindowSdl), as the
// port had them. A Windows file by design: it leaves the Linux source list
// (tools/linux_probe/linux_tree.py WINDOWS_ONLY); fmv_play.cpp stands in for
// it there.
#include "game/fmv_play_native.h"

#include <windows.h>
#include <mmsystem.h>

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/win_main.h"
#include "hook/detour.h"
#include "video/movie.h"

namespace fmv_play {

namespace {

// DIV-0042's whole multiple copies pixels (COLORONCOLOR); a fit averages them
// (HALFTONE), as MCI's own draw did when told to stretch.
void DrawOn(HDC dc, const Picture& p, bool border) {
    if (border) {
        // The client outside the rectangle, black: the class's brush did it
        // under the port's procedure on every erase, SDL's procedure erases
        // once (SDL_HINT_WINDOWS_ERASE_BACKGROUND_MODE's "initial").
        RECT client = {};
        if (GetClientRect(static_cast<HWND>(p.window), &client)) {
            const int save = SaveDC(dc);
            ExcludeClipRect(dc, p.dest.left, p.dest.top, p.dest.left + p.dest.width, p.dest.top + p.dest.height);
            FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            RestoreDC(dc, save);
        }
    }
    const std::uint32_t w = p.movie->Width(), h = p.movie->Height();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h);   // top-down rows, as the decoders give them
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    if (p.whole) {
        SetStretchBltMode(dc, COLORONCOLOR);
    } else {
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, nullptr);
    }
    StretchDIBits(dc, p.dest.left, p.dest.top, p.dest.width, p.dest.height, 0, 0, static_cast<int>(w), static_cast<int>(h),
                  p.movie->Frame(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

}  // namespace

namespace native {

void Draw(const Picture& p, bool border) {
    HWND hwnd = static_cast<HWND>(p.window);
    HDC dc = GetDC(hwnd);
    if (!dc) return;
    DrawOn(dc, p, border);
    ReleaseDC(hwnd, dc);
}

// SetWindowLongPtrA / GWLP_WNDPROC: SetWindowLongA / GWL_WNDPROC on i686
// (the SDK's own aliases), the pointer-wide form on x86-64, where the 32-bit
// one does not exist.
std::intptr_t Subclass(void* window) {
    return static_cast<std::intptr_t>(SetWindowLongPtrA(static_cast<HWND>(window), GWLP_WNDPROC,
                                                        static_cast<LONG_PTR>(::bof3::Code(bof3::addr::Fmv_WndProc))));
}

void Unsubclass(void* window, std::intptr_t saved) {
    SetWindowLongPtrA(static_cast<HWND>(window), GWLP_WNDPROC, static_cast<LONG_PTR>(saved));
}

void PumpMessages() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            Fmv_Playing = 0;
            PostQuitMessage(static_cast<int>(msg.wParam));
            return;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (!Fmv_Playing) return;
    }
}

void WaitMessages(unsigned ms) { MsgWaitForMultipleObjects(0, nullptr, FALSE, ms, QS_ALLINPUT); }

}  // namespace native

}  // namespace fmv_play

// original 0x59E570 (read whole, 0xD4 bytes; DIV-0049): the window procedure
// Fmv_Play installs for the length of a video - on the Win32 window path
// (BOF3X_OFF=WindowSdl) alone since R6m; on SDL's window the play loop takes
// the same events from game_window::TakeForMovie (fmv_play.cpp).
//   WM_KEYDOWN, WM_LBUTTONDOWN, WM_RBUTTONDOWN: Fmv_Playing cleared if set
//     (the skip); 0 returned.
//   WM_DESTROY: Fmv_Playing = 0, Game_QuitFlag = 1, PostQuitMessage(0); 0.
//   WM_ACTIVATEAPP: wParam set - SetFocus(hwnd), "resume vfw"; clear -
//     "pause vfw"; 0. (The original; symbols.toml's "falls through to the
//     saved procedure" was wrong - the default case is DefWindowProcA.)
//   MM_MCINOTIFY (0x3B9): Fmv_Playing = 0 when wParam is
//     MCI_NOTIFY_SUCCESSFUL (1), the video's end; 0 either way.
//   else: DefWindowProcA.
// DIV-0049: under DIV-0033 (the game keeps running when the window is not
// in front) the pause and resume are not sent, so the video plays on;
// BOF3X_BACKGROUND=0 keeps the original's pair.
// DIV-0205: the pause and resume stop and start our clock and audio stream
// (Pause) in place of the MCI strings; the video's end is the loop's own,
// so MM_MCINOTIFY never arrives (the case is kept, harmless); WM_PAINT
// draws the current frame again, which MCI's window did for itself.
extern "C" std::int32_t __stdcall Fmv_WndProc(void* hwnd_, unsigned msg, unsigned wparam, std::int32_t lparam) {
    using namespace fmv_play;
    HWND hwnd = static_cast<HWND>(hwnd_);
    switch (msg) {
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (Fmv_Playing) Fmv_Playing = 0;
        return 0;
    case WM_DESTROY:
        Fmv_Playing = 0;
        Game_QuitFlag = 1;
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATEAPP:
        if (wparam) {
            SetFocus(hwnd);
            if (!WinMain_Background()) Pause(false);
        } else if (!WinMain_Background()) {
            Pause(true);
        }
        return 0;
    case WM_PAINT:
        if (const Picture* p = Playing(); p && p->movie->FramesDecoded()) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (dc) DrawOn(dc, *p, false);
            EndPaint(hwnd, &ps);
            return 0;
        }
        return static_cast<std::int32_t>(DefWindowProcA(hwnd, msg, wparam, static_cast<LPARAM>(lparam)));
    case MM_MCINOTIFY:
        if (wparam == MCI_NOTIFY_SUCCESSFUL) Fmv_Playing = 0;
        return 0;
    default:
        return static_cast<std::int32_t>(DefWindowProcA(hwnd, msg, wparam, static_cast<LPARAM>(lparam)));
    }
}

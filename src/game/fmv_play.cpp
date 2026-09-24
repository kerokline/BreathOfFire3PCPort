// The FMV player, replaced (docs/replacing-mci.md for the original read whole,
// docs/window-modes.md for this):
//
//   Fmv_Play   0x59E360..0x59E4E3 (0x183 bytes)   void (const char *filename, void *hwnd, int fullscreen)
//
// The original opens the AVI through MCI, and when `fullscreen` is set calls
// Fmv_EnterFullscreen 0x59E4F0: a second DirectDraw object, an exclusive
// 640 x 480 x 16 SetDisplayMode, WS_POPUP on the window. Then it subclasses
// the window with Fmv_WndProc 0x59E570, tells MCI the window and a fixed
// "0 0 640 480" destination, plays, pumps messages until Fmv_WndProc clears
// Fmv_Playing (the video's end, a key, a click), stops and closes, restores
// the window procedure and, if it went fullscreen, releases the DirectDraw
// and puts the window's style and size back.
//
// DIVERGENCE DIV-0035 (docs/DIVERGENCE.md): no display mode is set and no
// second DirectDraw is made, whatever `fullscreen` says. The video is played
// into the window's client area at the largest integer multiple of 640 x
// 480 that fits, centred, black around it (the window class's brush); a
// client smaller than 640 x 480 gets the largest 4:3 fit instead. In the
// original's windowed mode the client is 640 x 480, so the destination is
// the original's "0 0 640 480" there. Everything else - the open with the
// disc-root retry, the subclass, the play and the pump, the skip on a key
// or click, the stop and close, the restore of the window procedure - is
// the original's, call for call. The fullscreen restore (Release, the
// SetWindowPos to the desktop's size, the saved style) has nothing to
// restore and is not reached: Fmv_DDraw stays null.
//
// Before the loop, on the main thread's stack - no task stack here.
#include "game/fmv_play.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>

#include "bof3/symbols.gen.h"
#include "game/display_setup.h"
#include "game/win_main.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;

// The seven MCI strings, each referenced by this function alone (pe_xref.py,
// docs/replacing-mci.md section 1).
constexpr U kFmtOpen = 0x66B674;        // "open avivideo!%s alias vfw"
constexpr U kFmtOpenRoot = 0x66B654;    // "open avivideo!%s%s alias vfw"
constexpr U kFmtWindow = 0x66B62C;      // "window vfw handle %d"
constexpr U kPlay = 0x66B5E8;           // "play vfw window from 0 notify"
constexpr U kStop = 0x66B5D8;           // "stop vfw wait"
constexpr U kClose = 0x66B644;          // "close vfw wait"
constexpr U kPause = 0x66B690;          // "pause vfw"
constexpr U kResume = 0x66B69C;         // "resume vfw"
// 0x66B608 "put vfw destination at 0 0 640 480" is the literal DIV-0035
// replaces with a computed rectangle.
const char* Str(U address) { return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address)); }

// DIV-0035: where the 640 x 480 video lands in a client of cw x ch. DIV-0042
// (the owner, 2026-09-23): with snap, the largest whole multiple of 640 x 480
// that fits, centred - a 3x game window shows the video at 2x with a border,
// and the window never changes size for a video; without snap, or when not
// even 1x fits, the largest 4:3 fit.
RECT Destination(LONG cw, LONG ch) {
    const LONG kx = cw / 640, ky = ch / 480;
    LONG k = DisplaySetup_Snap() ? (kx < ky ? kx : ky) : 0;
    LONG w, h;
    if (k >= 1) {
        w = 640 * k;
        h = 480 * k;
    } else if (cw * 3 >= ch * 4) {
        h = ch;
        w = ch * 4 / 3;
    } else {
        w = cw;
        h = cw * 3 / 4;
    }
    const LONG x = (cw - w) / 2, y = (ch - h) / 2;
    return {x, y, w, h};   // right = width, bottom = height, as MCI's "put" takes them
}

}  // namespace

extern "C" void __cdecl Fmv_Play(const char* filename, void* hwnd_, int fullscreen) {
    HWND hwnd = static_cast<HWND>(hwnd_);
    char command[0x80];
    Fmv_Playing = 0;

    Crt_sprintf(command, Str(kFmtOpen), filename);
    if (mciSendStringA(command, nullptr, 0, nullptr) != 0) {
        Crt_sprintf(command, Str(kFmtOpenRoot), File_CdRoot(), filename);
        if (mciSendStringA(command, nullptr, 0, nullptr) != 0) return;
    }

    (void)fullscreen;   // DIV-0035: no Fmv_EnterFullscreen, whatever it says

    const LONG saved_proc = SetWindowLongA(hwnd, GWL_WNDPROC, static_cast<LONG>(bof3::addr::Fmv_WndProc));
    Crt_sprintf(command, Str(kFmtWindow), hwnd);
    mciSendStringA(command, nullptr, 0, nullptr);

    RECT client = {};
    GetClientRect(hwnd, &client);
    const RECT dest = Destination(client.right - client.left, client.bottom - client.top);
    std::snprintf(command, sizeof command, "put vfw destination at %ld %ld %ld %ld", dest.left, dest.top, dest.right,
                  dest.bottom);
    mciSendStringA(command, nullptr, 0, nullptr);
    bof3::Log("DIV-0035    %s into the window at %ld,%ld %ld x %ld of %ld x %ld", filename, dest.left, dest.top,
              dest.right, dest.bottom, client.right - client.left, client.bottom - client.top);

    mciSendStringA(Str(kPlay), nullptr, 0, hwnd);
    Fmv_Playing = 1;
    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (!Fmv_Playing) break;
    }
    mciSendStringA(Str(kStop), nullptr, 0, nullptr);
    mciSendStringA(Str(kClose), nullptr, 0, nullptr);
    SetWindowLongA(hwnd, GWL_WNDPROC, saved_proc);
}

// original 0x59E570 (read whole, 0xD4 bytes; DIV-0049): the window procedure
// Fmv_Play installs for the length of a video.
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
extern "C" long __stdcall Fmv_WndProc(void* hwnd_, unsigned msg, unsigned wparam, long lparam) {
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
            if (!WinMain_Background()) mciSendStringA(Str(kResume), nullptr, 0, nullptr);
        } else if (!WinMain_Background()) {
            mciSendStringA(Str(kPause), nullptr, 0, nullptr);
        }
        return 0;
    case MM_MCINOTIFY:
        if (wparam == MCI_NOTIFY_SUCCESSFUL) Fmv_Playing = 0;
        return 0;
    default:
        return static_cast<long>(DefWindowProcA(hwnd, msg, wparam, static_cast<LPARAM>(lparam)));
    }
}

void FmvPlay_Inject() {
    BOF3_INJECT(Fmv_Play);
    BOF3_INJECT(Fmv_WndProc);
}

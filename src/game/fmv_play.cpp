// The FMV player, replaced (docs/replacing-mci.md for the original read whole
// and for this build, docs/window-modes.md for the rectangle):
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
// the original's "0 0 640 480" there.
//
// DIVERGENCE DIV-0205: no MCI. The file is read whole - the open's two
// paths as the original's, the name then the CD root and the name - and
// decoded by our own decoders (src/video: the AVI reader, Cinepak for
// capcom.avi, Microsoft Video 1 for logos.avi), each frame drawn into the
// DIV-0035 rectangle by StretchDIBits at the file's frame rate (strh's
// dwRate / dwScale) on the wall clock, the PCM stream of the file that has
// one through an SDL3 audio stream of its own. Windows' Video for Windows
// decompressors are 32-bit only (D407: the 64-bit build drew nothing). The
// subclass, the modal pump, the skip on a key, a click or the pad, the pause
// and resume on WM_ACTIVATEAPP (DIV-0049) and the restore of the window
// procedure keep their shape; what MCI did on each of its commands is done
// here instead. The renderer (src/render) does not exist yet when the
// movies play - Display_Setup runs after both - so the frames go to the
// window through GDI, as MCI's did.
//
// Before the loop, on the main thread's stack - no task stack here.
#include "game/fmv_play.h"

#include <windows.h>

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/display_setup.h"
#include "game/pad_read.h"
#include "game/win_main.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "video/movie.h"

namespace {

// --- the playing state Fmv_WndProc and the loop share ---------------------------

}  // namespace

void FmvPlay_Inject() {
}

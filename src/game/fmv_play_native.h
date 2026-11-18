// What fmv_play.cpp (the movie player, portable) and fmv_play_win32.cpp (its
// Windows half) hand each other (docs/linux-round.md, wave 2, R6m). Not for
// any other file. On a system that is not Windows, fmv_play.cpp defines the
// `native` functions itself: the Win32 path's refuse, and the picture is a
// Fatal until the owner chooses how it is drawn there.
#pragma once

#include <cstdint>

namespace video {
class Movie;
}

namespace fmv_play {

// DIV-0035's rectangle in the client: left, top, width, height.
struct Rect {
    int left, top, width, height;
};

// The movie on screen.
struct Picture {
    void* window;               // what Game_Hwnd holds: the HWND on Windows (both window paths)
    const video::Movie* movie;
    Rect dest;
    bool whole;                 // DIV-0042: a whole multiple - pixels copied, not averaged
};

// ---- the portable half's services (fmv_play.cpp) ---------------------------

// The movie playing, or null between movies.
const Picture* Playing();

// WM_ACTIVATEAPP's pair, in place of MCI's "pause vfw" / "resume vfw": the
// clock stops with the picture, and the audio stream with them.
void Pause(bool on);

// ---- the system's half (fmv_play_win32.cpp on Windows) ----------------------

namespace native {

// The current frame into the rectangle. DIV-0042's whole multiple copies
// pixels; a fit averages them. With `border`, the client outside the
// rectangle is painted black first (on SDL's window, whose procedure erases
// the background once only).
void Draw(const Picture& p, bool border);

// The Win32 window path (BOF3X_OFF=WindowSdl): Fmv_WndProc in front of the
// window's procedure for the length of the movie, and the saved one back.
std::intptr_t Subclass(void* window);
void Unsubclass(void* window, std::intptr_t saved);

// The Win32 path's message step: the thread's queue drained and dispatched
// (Fmv_WndProc acts on what concerns the movie). A WM_QUIT taken ends the
// movie and is posted again for WinMain's loop.
void PumpMessages();

// The Win32 path's wait: up to `ms` milliseconds, or until a message comes.
void WaitMessages(unsigned ms);

}  // namespace native

}  // namespace fmv_play

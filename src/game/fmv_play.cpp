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

using U = std::uint32_t;

// DIV-0035: where the 640 x 480 video lands in a client of cw x ch. DIV-0042
// (the owner, 2026-09-23): with snap, the largest whole multiple of 640 x 480
// that fits, centred - a 3x game window shows the video at 2x with a border,
// and the window never changes size for a video; without snap, or when not
// even 1x fits, the largest 4:3 fit. `whole` says whether the fit is a whole
// multiple (the stretch then copies pixels; else it averages).
RECT Destination(LONG cw, LONG ch, bool* whole) {
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
    *whole = k >= 1;
    const LONG x = (cw - w) / 2, y = (ch - h) / 2;
    return {x, y, w, h};   // right = width, bottom = height, as MCI's "put" took them
}

// A movie that cannot be opened - a disc-only install has no AVIs (docs/
// disc-only-install.md), and LOGOS.AVI has no disc counterpart at all - is
// skipped and the game goes on, exactly as the original's two failed opens
// return; the original said nothing, this says it once per file name a
// process.
void LogNotOpened(const char* filename, const char* why) {
    static char seen[4][64];
    static unsigned count = 0;
    for (unsigned i = 0; i < count; ++i)
        if (std::strncmp(seen[i], filename, sizeof seen[i] - 1) == 0) return;
    if (count < 4) std::snprintf(seen[count++], sizeof seen[0], "%s", filename);
    bof3::Log("fmv: %s not opened (%s; the CD root %s tried too): skipped, the game goes on as the original does when "
              "the open fails",
              filename, why, File_CdRoot());
}

// The file's bytes, read whole (both movies are under 5 MB). Null when it
// cannot be opened or read, with the reason in `why`.
std::uint8_t* ReadWhole(const char* path, std::size_t* size, char* why, std::size_t why_n) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        std::snprintf(why, why_n, "error %lu", GetLastError());
        return nullptr;
    }
    LARGE_INTEGER n;
    if (!GetFileSizeEx(f, &n) || n.QuadPart <= 0 || n.QuadPart > 256 * 1024 * 1024) {
        std::snprintf(why, why_n, "its size (%lld bytes) is not one this reads", static_cast<long long>(n.QuadPart));
        CloseHandle(f);
        return nullptr;
    }
    auto* data = static_cast<std::uint8_t*>(HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(n.QuadPart)));
    if (!data) {
        std::snprintf(why, why_n, "no memory for %lld bytes", static_cast<long long>(n.QuadPart));
        CloseHandle(f);
        return nullptr;
    }
    std::size_t got = 0;
    while (got < static_cast<std::size_t>(n.QuadPart)) {
        DWORD read = 0;
        const DWORD want = static_cast<DWORD>(static_cast<std::size_t>(n.QuadPart) - got > 0x1000000 ? 0x1000000 : static_cast<std::size_t>(n.QuadPart) - got);
        if (!ReadFile(f, data + got, want, &read, nullptr) || read == 0) {
            std::snprintf(why, why_n, "read error %lu at %zu", GetLastError(), got);
            HeapFree(GetProcessHeap(), 0, data);
            CloseHandle(f);
            return nullptr;
        }
        got += read;
    }
    CloseHandle(f);
    *size = got;
    return data;
}

// --- the playing state Fmv_WndProc and the loop share ---------------------------

struct Playing {
    HWND hwnd;
    const video::Movie* movie;
    RECT dest;         // DIV-0035's rectangle: left, top, width, height
    bool whole;        // a whole multiple: pixels copied, not averaged
    bool paused;       // WM_ACTIVATEAPP's pause (DIV-0049: not under DIV-0033)
    LONGLONG pause_began;    // QPC ticks when the pause began
    LONGLONG paused_ticks;   // QPC ticks spent paused so far: the clock stands still through them
    SDL_AudioStream* audio;
};
Playing* g_playing;

// The current frame into the rectangle. DIV-0042's whole multiple copies
// pixels (COLORONCOLOR); a fit averages them (HALFTONE), as MCI's own draw
// did when told to stretch.
void Draw(HDC dc) {
    const Playing& p = *g_playing;
    const U w = p.movie->Width(), h = p.movie->Height();
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
    StretchDIBits(dc, p.dest.left, p.dest.top, p.dest.right, p.dest.bottom, 0, 0, static_cast<int>(w), static_cast<int>(h),
                  p.movie->Frame(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

void DrawToWindow() {
    HDC dc = GetDC(g_playing->hwnd);
    if (!dc) return;
    Draw(dc);
    ReleaseDC(g_playing->hwnd, dc);
}

LONGLONG Now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

// WM_ACTIVATEAPP's pair, in place of MCI's "pause vfw" / "resume vfw": the
// clock stops with the picture, and the audio stream with them.
void Pause(bool on) {
    Playing* p = g_playing;
    if (!p || p->paused == on) return;
    p->paused = on;
    if (on) {
        p->pause_began = Now();
        if (p->audio) SDL_PauseAudioStreamDevice(p->audio);
    } else {
        p->paused_ticks += Now() - p->pause_began;
        if (p->audio) SDL_ResumeAudioStreamDevice(p->audio);
    }
}

// The PCM stream, when the file has one, as an SDL3 audio stream of its own
// (DIV-0201's library; the game's sound output does not exist yet, the
// movies play before Display_Setup and Game_Init). The whole stream is put
// in before the play starts - 7 s of 22 kHz stereo - and the device plays it
// at its own pace. Null, with a log line, when the device cannot be opened:
// the picture plays without it.
SDL_AudioStream* OpenAudio(video::Movie& movie, const char* filename) {
    if (!movie.HasPcm()) return nullptr;
    const video::AudioStream& a = movie.File().audio;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        bof3::Log("fmv: %s: SDL_InitSubSystem(AUDIO) failed: %s - the picture plays without its sound", filename,
                  SDL_GetError());
        return nullptr;
    }
    const SDL_AudioSpec want = {a.bits == 8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE, static_cast<int>(a.channels),
                                static_cast<int>(a.rate)};
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, nullptr, nullptr);
    if (!stream) {
        bof3::Log("fmv: %s: SDL_OpenAudioDeviceStream failed: %s - the picture plays without its sound", filename,
                  SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return nullptr;
    }
    U n;
    while (const std::uint8_t* chunk = movie.NextAudio(&n))
        if (n) SDL_PutAudioStreamData(stream, chunk, static_cast<int>(n));
    SDL_FlushAudioStream(stream);
    return stream;
}

void CloseAudio(SDL_AudioStream* stream) {
    if (!stream) return;
    SDL_DestroyAudioStream(stream);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

}  // namespace

extern "C" void __cdecl Fmv_Play(const char* filename, void* hwnd_, int fullscreen) {
    HWND hwnd = static_cast<HWND>(hwnd_);
    Fmv_Playing = 0;

    // The open: the name as given, then under the CD root - the original's
    // two MCI opens, "%s" then "%s%s" with File_CdRoot (0x66B674, 0x66B654).
    char path[0x104];
    char why[256];
    std::size_t size = 0;
    std::snprintf(path, sizeof path, "%s", filename);
    std::uint8_t* data = ReadWhole(path, &size, why, sizeof why);
    if (!data) {
        char root_why[128];
        std::snprintf(path, sizeof path, "%s%s", File_CdRoot(), filename);
        data = ReadWhole(path, &size, root_why, sizeof root_why);
        if (!data) {
            std::snprintf(why + std::strlen(why), sizeof why - std::strlen(why), "; under the root %s", root_why);
            LogNotOpened(filename, why);
            return;
        }
    }
    video::Movie movie;
    if (!movie.Open(data, size, why, sizeof why)) {
        LogNotOpened(filename, why);
        HeapFree(GetProcessHeap(), 0, data);
        return;
    }

    (void)fullscreen;   // DIV-0035: no Fmv_EnterFullscreen, whatever it says

    // SetWindowLongPtrA / GWLP_WNDPROC: SetWindowLongA / GWL_WNDPROC on i686
    // (the SDK's own aliases), the pointer-wide form on x86-64, where the
    // 32-bit one does not exist.
    const LONG_PTR saved_proc =
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, static_cast<LONG_PTR>(::bof3::Code(bof3::addr::Fmv_WndProc)));

    RECT client = {};
    GetClientRect(hwnd, &client);
    Playing playing = {};
    playing.hwnd = hwnd;
    playing.movie = &movie;
    playing.dest = Destination(client.right - client.left, client.bottom - client.top, &playing.whole);
    bof3::Log("DIV-0035    %s into the window at %ld,%ld %ld x %ld of %ld x %ld", filename, playing.dest.left,
              playing.dest.top, playing.dest.right, playing.dest.bottom, client.right - client.left,
              client.bottom - client.top);
    const video::AviFile& f = movie.File();
    const double frame_s = movie.FrameSeconds();
    const U frames = movie.FrameCount();
    double audio_s = 0;
    if (movie.HasPcm()) audio_s = static_cast<double>(movie.AudioBytes()) / (f.audio.rate * f.audio.channels * (f.audio.bits / 8));
    const double video_s = frames * frame_s;
    const double end_s = audio_s > video_s ? audio_s : video_s;
    bof3::Log("DIV-0205    %s: %s %u x %u, %u frames at %u/%u a second (%.2f s)%s", filename, movie.Codec(),
              movie.Width(), movie.Height(), frames, f.video.rate, f.video.scale, video_s,
              movie.HasPcm() ? "" : ", no audio stream");
    if (movie.HasPcm())
        bof3::Log("DIV-0205    %s: PCM %u Hz, %u channel(s), %u bits, %.2f s", filename, f.audio.rate, f.audio.channels,
                  f.audio.bits, audio_s);
    playing.audio = OpenAudio(movie, filename);
    g_playing = &playing;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    const LONGLONG t0 = Now();
    if (playing.audio) SDL_ResumeAudioStreamDevice(playing.audio);
    Fmv_Playing = 1;
    // DIVERGENCE DIV-0066: the pad skips too. The original's pump is a
    // blocking GetMessage, ended by Fmv_WndProc (a key, a click, the video's
    // end, WM_DESTROY); the pad reaches the game only through Pad_Read, which
    // the loop never calls. Ours drains the queue the same way, then polls
    // the pad and waits up to a frame for the next message. A pad input
    // going down during the video ends it as a key does; one held from
    // before the video is ignored until released. A WM_QUIT taken off the
    // queue is put back for WinMain's loop, which GetMessage returning 0
    // used to leave there.
    bool pad_was_down = PadRead_AnyInputDown();
    MSG msg;
    U shown = 0;        // frames decoded so far; the frame on screen is shown - 1
    bool faulted = false;
    while (Fmv_Playing) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                Fmv_Playing = 0;
                PostQuitMessage(static_cast<int>(msg.wParam));
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (!Fmv_Playing) break;
        }
        if (!Fmv_Playing) break;
        const bool pad_down = PadRead_AnyInputDown();
        if (pad_down && !pad_was_down) {
            bof3::Log("DIV-0066    %s skipped by the pad", filename);
            Fmv_Playing = 0;
            break;
        }
        pad_was_down = pad_down;
        DWORD wait = 16;
        if (!playing.paused) {
            const double elapsed =
                static_cast<double>(Now() - t0 - playing.paused_ticks) / static_cast<double>(freq.QuadPart);
            if (elapsed >= end_s) {   // the video's end - MCI's notify
                Fmv_Playing = 0;
                break;
            }
            // The frame due now; every frame up to it is decoded (each is
            // coded over the last), only the due one drawn.
            U due = static_cast<U>(elapsed / frame_s) + 1;
            if (due > frames) due = frames;
            while (shown < due && !faulted) {
                if (!movie.NextFrame(why, sizeof why)) {
                    faulted = true;   // the picture stays on the last good frame to the end
                    bof3::Log("fmv: %s: frame %u: %s - the picture holds from here", filename, movie.FramesDecoded(), why);
                }
                ++shown;
                if (shown == due || faulted) DrawToWindow();
            }
            const double next = shown < frames ? shown * frame_s : end_s;
            const double until = next - elapsed;
            wait = until <= 0 ? 0 : until >= 0.016 ? 16 : static_cast<DWORD>(until * 1000);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, wait, QS_ALLINPUT);
    }
    // The stop and close: the audio stream and the subclass.
    bof3::Log("DIV-0205    %s: %s after %.2f s, %u of %u frames shown%s", filename,
              shown >= frames ? "ended" : "skipped",
              static_cast<double>(Now() - t0 - playing.paused_ticks) / static_cast<double>(freq.QuadPart), shown, frames,
              faulted ? ", a decode fault held the picture" : "");
    CloseAudio(playing.audio);
    g_playing = nullptr;
    SetWindowLongPtrA(hwnd, GWLP_WNDPROC, saved_proc);
    HeapFree(GetProcessHeap(), 0, data);
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
// DIV-0205: the pause and resume stop and start our clock and audio stream
// (Pause) in place of the MCI strings; the video's end is the loop's own,
// so MM_MCINOTIFY never arrives (the case is kept, harmless); WM_PAINT
// draws the current frame again, which MCI's window did for itself.
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
            if (!WinMain_Background()) Pause(false);
        } else if (!WinMain_Background()) {
            Pause(true);
        }
        return 0;
    case WM_PAINT:
        if (g_playing && g_playing->movie->FramesDecoded()) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (dc) Draw(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        return static_cast<long>(DefWindowProcA(hwnd, msg, wparam, static_cast<LPARAM>(lparam)));
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

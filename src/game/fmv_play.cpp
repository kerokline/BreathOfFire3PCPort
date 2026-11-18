// Moved here by tools/move_funcs.py from battle_menu_win.cpp (docs/source-layout-round.md).
#include "game/fmv_play.h"

#include <SDL3/SDL.h>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dinput.h>
#include <fstream>

#include "bof3/symbols.gen.h"
#include "game/area_data_reload.h"
#include "game/battle/battle_menu_win.h"
#include "game/battle/battle_menu_win_callees.h"
#include "game/battle_draw.h"
#include "game/battle_draw_callees.h"
#include "game/battle_e7.h"
#include "game/battle_e7_callees.h"
#include "game/boss_harness.h"
#include "game/button_text.h"
#include "game/d3d_lines.h"
#include "game/dat_load.h"
#include "game/display_setup.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_draw_helpers.h"
#include "game/menu_draw_helpers_callees.h"
#include "game/menu_frame.h"
#include "game/mode_flow.h"
#include "game/mode_flow_callees.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/msgbox_callees.h"
#include "game/pad_read.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2h.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/win_main.h"
#include "game/window_task.h"
#include "game/window_task_callees.h"
#include "game/yes_no_layout.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"
#include "os/os.h"
#include "video/movie.h"

namespace {

namespace at = battle_menu_win::at;

}  // namespace

namespace {

using namespace battle_draw;

using namespace menu_draw_helpers;

}  // namespace

using namespace mode_flow;

using namespace msgbox;

using namespace window_task;

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
    std::ifstream f(bof3::os::Path(path), std::ios::binary);
    if (!f) {
        std::snprintf(why, why_n, "cannot open");
        return nullptr;
    }
    f.seekg(0, std::ios::end);
    const long long n = static_cast<long long>(f.tellg());
    f.seekg(0);
    if (!f || n <= 0 || n > 256 * 1024 * 1024) {
        std::snprintf(why, why_n, "its size (%lld bytes) is not one this reads", n);
        return nullptr;
    }
    auto* data = static_cast<std::uint8_t*>(std::malloc(static_cast<std::size_t>(n)));
    if (!data) {
        std::snprintf(why, why_n, "no memory for %lld bytes", static_cast<long long>(n));
        return nullptr;
    }
    std::size_t got = 0;
    while (got < static_cast<std::size_t>(n)) {
        const std::size_t want = static_cast<std::size_t>(n) - got > 0x1000000 ? 0x1000000 : static_cast<std::size_t>(n) - got;
        f.read(reinterpret_cast<char*>(data + got), static_cast<std::streamsize>(want));
        const std::size_t read = static_cast<std::size_t>(f.gcount());
        if (read == 0) {
            std::snprintf(why, why_n, "read error at %zu", got);
            std::free(data);
            return nullptr;
        }
        got += read;
    }
    *size = got;
    return data;
}

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

LONGLONG Now() { return static_cast<LONGLONG>(bof3::os::Counter()); }

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
// movies play before Display_Setup and Game_Init). The stream takes the
// movie's own rate - 22 kHz for the AVI, 37,800 Hz for CAPCOM30.STR
// (DIV-0214) - and SDL3's audio stream resamples it to the device. The whole
// stream is put in before the play starts - 7 s of stereo - and the device
// plays it at its own pace. Null, with a log line, when the device cannot be
// opened: the picture plays without it.
SDL_AudioStream* OpenAudio(video::Movie& movie, const char* filename) {
    if (!movie.HasPcm()) return nullptr;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        bof3::Log("fmv: %s: SDL_InitSubSystem(AUDIO) failed: %s - the picture plays without its sound", filename,
                  SDL_GetError());
        return nullptr;
    }
    const SDL_AudioSpec want = {movie.AudioBits() == 8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE,
                                static_cast<int>(movie.AudioChannels()), static_cast<int>(movie.AudioRate())};
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
    //
    // DIVERGENCE DIV-0214: for CAPCOM.AVI, when neither open succeeds, the
    // same two opens of CAPCOM30.STR, the PlayStation disc's own logo movie
    // (a disc-only install has no AVI); a CAPCOM.AVI that is there still wins.
    const char* const requested = filename;
    const char* const names[2] = {filename, _stricmp(filename, "CAPCOM.AVI") == 0 ? "CAPCOM30.STR" : nullptr};
    char path[0x104];
    char why[256];
    char why_str[256] = "";
    std::size_t size = 0;
    std::uint8_t* data = nullptr;
    for (int n = 0; n < 2 && names[n] && !data; ++n) {
        char* const w = n == 0 ? why : why_str;
        const std::size_t wn = n == 0 ? sizeof why : sizeof why_str;
        std::snprintf(path, sizeof path, "%s", names[n]);
        data = ReadWhole(path, &size, w, wn);
        if (!data) {
            char root_why[128];
            std::snprintf(path, sizeof path, "%s%s", File_CdRoot(), names[n]);
            data = ReadWhole(path, &size, root_why, sizeof root_why);
            if (!data) std::snprintf(w + std::strlen(w), wn - std::strlen(w), "; under the root %s", root_why);
        }
        if (data) filename = names[n];
    }
    if (!data) {
        if (names[1]) std::snprintf(why + std::strlen(why), sizeof why - std::strlen(why), "; %s: %s", names[1], why_str);
        LogNotOpened(requested, why);
        return;
    }
    if (filename != requested)
        bof3::Log("DIV-0214    %s is not there; %s plays in its place, the PlayStation disc's own movie", requested, filename);
    video::Movie movie;
    if (!movie.Open(data, size, why, sizeof why)) {
        LogNotOpened(filename, why);
        std::free(data);
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
    const double frame_s = movie.FrameSeconds();
    const U frames = movie.FrameCount();
    const double audio_s = movie.AudioSeconds();
    const double video_s = frames * frame_s;
    const double end_s = audio_s > video_s ? audio_s : video_s;
    // The log lines carry the ledger entry of the movie that ran: DIV-0205 for an AVI, DIV-0214 for an STR.
    const bool str = std::strcmp(movie.Container(), "STR") == 0;
    bof3::Log("%s    %s: %s %u x %u, %u frames at %.3f a second (%.2f s)%s", str ? "DIV-0214" : "DIV-0205", filename,
              movie.Codec(), movie.Width(), movie.Height(), frames, 1.0 / frame_s, video_s,
              movie.HasPcm() ? "" : ", no audio stream");
    if (movie.HasPcm())
        bof3::Log("%s    %s: PCM %u Hz, %u channel(s), %u bits, %.2f s", str ? "DIV-0214" : "DIV-0205", filename,
                  movie.AudioRate(), movie.AudioChannels(), movie.AudioBits(), audio_s);
    playing.audio = OpenAudio(movie, filename);
    g_playing = &playing;

    const std::uint64_t freq = bof3::os::CounterFrequency();
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
                static_cast<double>(Now() - t0 - playing.paused_ticks) / static_cast<double>(freq);
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
    bof3::Log("%s    %s: %s after %.2f s, %u of %u frames shown%s", str ? "DIV-0214" : "DIV-0205", filename,
              shown >= frames ? "ended" : "skipped",
              static_cast<double>(Now() - t0 - playing.paused_ticks) / static_cast<double>(freq), shown, frames,
              faulted ? ", a decode fault held the picture" : "");
    CloseAudio(playing.audio);
    g_playing = nullptr;
    SetWindowLongPtrA(hwnd, GWLP_WNDPROC, saved_proc);
    std::free(data);
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
extern "C" std::int32_t __stdcall Fmv_WndProc(void* hwnd_, unsigned msg, unsigned wparam, std::int32_t lparam) {
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
        return static_cast<std::int32_t>(DefWindowProcA(hwnd, msg, wparam, static_cast<LPARAM>(lparam)));
    case MM_MCINOTIFY:
        if (wparam == MCI_NOTIFY_SUCCESSFUL) Fmv_Playing = 0;
        return 0;
    default:
        return static_cast<std::int32_t>(DefWindowProcA(hwnd, msg, wparam, static_cast<LPARAM>(lparam)));
    }
}

void FmvPlay_Inject() {
    BOF3_INJECT(Fmv_Play);
    BOF3_INJECT(Fmv_WndProc);
}

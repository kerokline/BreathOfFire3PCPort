// Moved here by tools/move_funcs.py from battle_menu_win.cpp (docs/source-layout-round.md).
//
// The movie player (DIV-0035, DIV-0205, DIV-0214): the open, the decode, the
// clock, the sound and the play loop, the same on every system. Its events
// and its picture (docs/linux-round.md, wave 2, R6m):
//
//   * SDL's window (game_window.h, the default): no procedure of the player's
//     in front of the window's. The loop takes the window's events from
//     game_window::TakeForMovie - a key or a click skips, a close quits the
//     game, the activation pauses and resumes under DIV-0049's rule, a
//     repaint draws the frame again - polls the pad (DIV-0066) and waits with
//     SDL's delay.
//   * The Win32 window (BOF3X_OFF=WindowSdl, Windows only): Fmv_WndProc in
//     front of the window's procedure and the message pump, as the port had
//     them (fmv_play_win32.cpp).
//
// The picture is GDI's on Windows on both paths (fmv_play_win32.cpp); on any
// other system it is not drawn yet - native::Draw below stops the game - until
// the owner chooses how (docs/replacing-mci.md section 9).
#include "game/fmv_play.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/display_setup.h"
#include "game/fmv_play_native.h"
#include "game/pad_read.h"
#include "game/platform/game_window.h"
#include "game/win_main.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "os/os.h"
#include "video/movie.h"

namespace fmv_play {

namespace {

using U = std::uint32_t;

// DIV-0035: where the 640 x 480 video lands in a client of cw x ch. DIV-0042
// (the owner, 2026-09-23): with snap, the largest whole multiple of 640 x 480
// that fits, centred - a 3x game window shows the video at 2x with a border,
// and the window never changes size for a video; without snap, or when not
// even 1x fits, the largest 4:3 fit. `whole` says whether the fit is a whole
// multiple (the stretch then copies pixels; else it averages).
Rect Destination(int cw, int ch, bool* whole) {
    const int kx = cw / 640, ky = ch / 480;
    int k = DisplaySetup_Snap() ? (kx < ky ? kx : ky) : 0;
    int w, h;
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
    const int x = (cw - w) / 2, y = (ch - h) / 2;
    return {x, y, w, h};
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

struct Play {
    Picture picture;
    bool paused;                // WM_ACTIVATEAPP's pause (DIV-0049: not under DIV-0033)
    std::int64_t pause_began;   // counter ticks when the pause began
    std::int64_t paused_ticks;  // counter ticks spent paused so far: the clock stands still through them
    SDL_AudioStream* audio;
};

Play* g_play;

std::int64_t Now() { return static_cast<std::int64_t>(bof3::os::Counter()); }

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

// The SDL path's message step: Fmv_WndProc's cases, read off the window's
// events (game_window::TakeForMovie) in place of the procedure.
void TakeSdlEvents(const char* filename) {
    constexpr std::int32_t kVkMenu = 0x12, kVkF10 = 0x79;
    for (const game_window::MovieEvent& e : game_window::TakeForMovie()) {
        switch (e.what) {
        case game_window::MovieGot::Key:
            // WM_KEYDOWN's keys: Alt and F10 come to a Win32 window as
            // WM_SYSKEYDOWN, which never skipped; off Windows SDL's key
            // events carry them too, and they do not skip either.
            if (e.a != kVkMenu && e.a != kVkF10) Fmv_Playing = 0;
            break;
        case game_window::MovieGot::Click: Fmv_Playing = 0; break;
        case game_window::MovieGot::Activate:
            // DIV-0049. The focus is SDL's to take back on activation (the
            // procedure's SetFocus).
            if (!WinMain_Background()) Pause(e.a == 0);
            break;
        case game_window::MovieGot::Repaint:
            if (g_play->picture.movie->FramesDecoded()) native::Draw(g_play->picture, true);
            break;
        case game_window::MovieGot::Close:   // WM_DESTROY's case: the path's close set Game_QuitFlag
            if (Fmv_Playing) bof3::Log("fmv: %s: the window closed - the movie ends, the game quits", filename);
            Fmv_Playing = 0;
            break;
        case game_window::MovieGot::Quit: Fmv_Playing = 0; break;   // put back for WinMain's loop by TakeForMovie
        }
        if (!Fmv_Playing) return;
    }
}

}  // namespace

const Picture* Playing() { return g_play ? &g_play->picture : nullptr; }

void Pause(bool on) {
    Play* p = g_play;
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

#ifndef _WIN32
// ---- the system's half on a system that is not Windows -----------------------

namespace native {

namespace {

[[noreturn]] void Refuse(const char* what) {
    bof3::Fatal("fmv: the Win32 window's %s was asked for - BOF3X_OFF=WindowSdl or BOF3X_ORIGINAL - but it is Windows "
                "only; unset the switch (docs/linux-round.md, R6m)",
                what);
}

}  // namespace

void Draw(const Picture&, bool) {
    bof3::Fatal("fmv: the movie's picture has no way to the screen on this system yet. The owner's call is open: the "
                "renderer's device brought up before the movies, or SDL's own surface on the window until "
                "Display_Setup (docs/replacing-mci.md section 9). Until then a movie stops the game here");
}

std::intptr_t Subclass(void*) { Refuse("movie procedure"); }
void Unsubclass(void*, std::intptr_t) { Refuse("movie procedure"); }
void PumpMessages() { Refuse("message pump"); }
void WaitMessages(unsigned) { Refuse("message wait"); }

}  // namespace native
#endif  // !_WIN32

}  // namespace fmv_play

extern "C" void __cdecl Fmv_Play(const char* filename, void* hwnd, int fullscreen) {
    using namespace fmv_play;
    Fmv_Playing = 0;

    // The open: the name as given, then under the CD root - the original's
    // two MCI opens, "%s" then "%s%s" with File_CdRoot (0x66B674, 0x66B654).
    //
    // DIVERGENCE DIV-0214: for CAPCOM.AVI, when neither open succeeds, the
    // same two opens of CAPCOM30.STR, the PlayStation disc's own logo movie
    // (a disc-only install has no AVI); a CAPCOM.AVI that is there still wins.
    // SDL_strcasecmp: ASCII case, as _stricmp in the C locale.
    const char* const requested = filename;
    const char* const names[2] = {filename, SDL_strcasecmp(filename, "CAPCOM.AVI") == 0 ? "CAPCOM30.STR" : nullptr};
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

    // The Win32 window keeps the port's procedure in front of the window's;
    // SDL's window is left as it is (TakeForMovie brings its events).
    const bool sdl = game_window::SdlOn();
    const std::intptr_t saved_proc = sdl ? 0 : native::Subclass(hwnd);

    unsigned cw = 0, ch = 0;
    game_window::ClientSize(hwnd, &cw, &ch);   // GetClientRect on the Win32 path; 0 x 0 for a window that is gone
    Play play = {};
    play.picture.window = hwnd;
    play.picture.movie = &movie;
    play.picture.dest = Destination(static_cast<int>(cw), static_cast<int>(ch), &play.picture.whole);
    const Rect& dest = play.picture.dest;
    bof3::Log("DIV-0035    %s into the window at %d,%d %d x %d of %u x %u", filename, dest.left, dest.top, dest.width,
              dest.height, cw, ch);
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
    play.audio = OpenAudio(movie, filename);
    g_play = &play;

    const std::uint64_t freq = bof3::os::CounterFrequency();
    const std::int64_t t0 = Now();
    if (play.audio) SDL_ResumeAudioStreamDevice(play.audio);
    Fmv_Playing = 1;
    // DIVERGENCE DIV-0066: the pad skips too. The original's pump is a
    // blocking GetMessage, ended by Fmv_WndProc (a key, a click, the video's
    // end, WM_DESTROY); the pad reaches the game only through Pad_Read, which
    // the loop never calls. Ours takes the window's events the same way (the
    // Win32 path: the queue drained and dispatched to Fmv_WndProc; SDL's
    // window: TakeForMovie), then polls the pad and waits up to a frame. A
    // pad input going down during the video ends it as a key does; one held
    // from before the video is ignored until released. A quit taken during
    // the video is put back for WinMain's loop, which GetMessage returning 0
    // used to leave there.
    bool pad_was_down = PadRead_AnyInputDown();
    U shown = 0;        // frames decoded so far; the frame on screen is shown - 1
    bool faulted = false;
    while (Fmv_Playing) {
        if (sdl) TakeSdlEvents(filename);
        else native::PumpMessages();
        if (!Fmv_Playing) break;
        const bool pad_down = PadRead_AnyInputDown();
        if (pad_down && !pad_was_down) {
            bof3::Log("DIV-0066    %s skipped by the pad", filename);
            Fmv_Playing = 0;
            break;
        }
        pad_was_down = pad_down;
        unsigned wait = 16;
        if (!play.paused) {
            const double elapsed = static_cast<double>(Now() - t0 - play.paused_ticks) / static_cast<double>(freq);
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
                if (shown == due || faulted) native::Draw(play.picture, false);
            }
            const double next = shown < frames ? shown * frame_s : end_s;
            const double until = next - elapsed;
            wait = until <= 0 ? 0 : until >= 0.016 ? 16 : static_cast<unsigned>(until * 1000);
        }
        // The wait: the Win32 path's ends early on a message, as the port's
        // did; SDL's window's is SDL's delay, and an event that comes during
        // it is taken at the next step, at most 16 ms on.
        if (sdl) SDL_Delay(wait);
        else native::WaitMessages(wait);
    }
    // The stop and close: the audio stream and, on the Win32 path, the subclass.
    bof3::Log("%s    %s: %s after %.2f s, %u of %u frames shown%s", str ? "DIV-0214" : "DIV-0205", filename,
              shown >= frames ? "ended" : "skipped",
              static_cast<double>(Now() - t0 - play.paused_ticks) / static_cast<double>(freq), shown, frames,
              faulted ? ", a decode fault held the picture" : "");
    CloseAudio(play.audio);
    g_play = nullptr;
    if (!sdl) native::Unsubclass(hwnd, saved_proc);
    std::free(data);
}

void FmvPlay_Inject() {
    BOF3_INJECT(Fmv_Play);
    BOF3_INJECT(Fmv_WndProc);
}

#ifndef _WIN32
// Capcom's procedure and ours are the Win32 window path's (fmv_play_win32.cpp
// on Windows), which this system has not.
extern "C" std::int32_t __stdcall Fmv_WndProc(void*, unsigned, unsigned, std::int32_t) {
    bof3::Fatal("Fmv_WndProc: a Win32 window procedure was called, and this system has no Win32 window (docs/linux-round.md, R6m)");
}
#endif

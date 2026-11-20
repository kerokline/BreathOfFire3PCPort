// launcher/ui/jukebox_play.h: the Jukebox's player. The Win32 Jukebox's
// Mp3Stream, SeqStream and Output (music_player.cpp), ported: the decoders are
// the same shared code (audio/seq.h, audio/song.h, media/xa_clip.h, minimp3),
// and the sound goes out through SDL's audio stream in place of waveOut.
//
// This file and ui_imgui.cpp are the drawn launcher's two files that call SDL:
// ui_imgui.cpp is the window, its events and the drawing (the one file that
// includes Dear ImGui and SDL's window and event calls); this one is the
// Jukebox's sound alone, SDL's audio subsystem and one device stream. Neither
// names an operating system: SDL picks the system's output (WASAPI on
// Windows; PulseAudio, PipeWire or ALSA on Linux), and a check takes its dummy
// driver.
//
// The samples are the Win32 player's. Its Output had a thread of its own
// refill whichever of four waveOut buffers of 2048 frames came back done,
// each by one Render of 2048 frames from the stream under a lock (silence
// with no stream); Set swapped the stream between two refills and cleared
// "ended"; a refill after which the stream said Ended raised it. Here SDL's
// audio thread asks for more through the stream's get-callback, which renders
// blocks of the same 2048 frames, under the stream's own lock, until it has
// handed SDL what was asked; Set swaps under that lock. So a song's samples
// are the same Renders of the same sizes from the same start: equal to the
// sample. A song's loop is in the stream (the sequencer's data, the MP3's
// measured row or its rewind); a song that plays once ends in silence and
// raises "ended", and the window is woken to say so (the Win32 window's 250 ms
// timer read it).
#include "launcher/ui/jukebox_play.h"

#include "audio/seq.h"
#include "audio/song.h"
#include "game/music_loops.h"
#include "launcher/config.h"
#include "launcher/ui/ui.h"
#include "media/xa_clip.h"

#include "os/thread.h"
#include "os/wide_file.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <cstring>

// third_party/minimp3/minimp3.h, verbatim (docs/THIRD_PARTY.md section 3): the
// drawn launcher's own copy of the implementation, as music_player.cpp is the
// Win32 launcher's and src/audio/mp3.cpp the DLL's.
#define MINIMP3_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#include "minimp3/minimp3.h"
#pragma clang diagnostic pop

namespace bof3x::ui {

namespace {

const char kTitle[] = "bof3x-launcher - music";

bool g_check = false;      // JukeOutCheck: no message box, the dummy driver
std::wstring g_reading;    // the file being parsed, for the message

}  // namespace

// ---- a malformed file ends the launcher, naming it (CLAUDE.md rule 4) -------

void JukeReading(const std::wstring& path) { g_reading = path; }

void JukeFatal(const std::wstring& message) {
    const std::string text = ConfigToUtf8(message);
    std::fprintf(stderr, "bof3x-launcher: music: %s\n", text.c_str());
    if (!g_check) Message(kTitle, text.c_str(), true);
    bof3::os::EndProcess(1);   // the Win32 player's ExitProcess(1)
}

namespace {

// The engine's sequencer refuses a malformed song or bank through this hook,
// then aborts (song.cpp MusicFatal).
void MusicAbort(const char* message) {
    const std::string text = std::string(message) + "\n\nwhile reading " +
                             (g_reading.empty() ? std::string("(no file)") : ConfigToUtf8(g_reading));
    std::fprintf(stderr, "bof3x-launcher: music: %s\n", text.c_str());
    if (!g_check) Message(kTitle, text.c_str(), true);
}

}  // namespace

std::vector<std::uint8_t> JukeReadWhole(const std::wstring& path) {
    const bof3::os::InFile f = bof3::os::InOpenW(path, false);
    if (f == bof3::os::kNoInFile) JukeFatal(path + L" does not open");
    std::uint64_t size = 0;
    constexpr std::uint64_t kFileMax = 64ull << 20;   // far above any song, bank or MP3
    if (!bof3::os::InSize(f, &size) || size == 0 || size > kFileMax) {
        bof3::os::InClose(f);
        JukeFatal(path + L" is empty or too large to be a song");
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
    std::size_t have = 0;
    bool ok = true;
    while (ok && have < out.size()) {
        std::uint32_t got = 0;
        ok = bof3::os::InRead(f, out.data() + have, static_cast<std::uint32_t>(out.size() - have), &got) && got > 0;
        have += got;
    }
    bof3::os::InClose(f);
    if (!ok || have != out.size()) JukeFatal(path + L" could not be read whole");
    return out;
}

namespace {

std::wstring WideOf(const char* text) { return ConfigFromUtf8(text); }

// ---- what plays ----------------------------------------------------------------

// A PC MP3, decoded whole. Sample positions count from the file's first
// decoded frame, as music_loops' rows and the game's LoopDecode count them.
class Mp3Stream : public JukeStream {
public:
    Mp3Stream(const std::wstring& path, unsigned track, bool loops) : loops_(loops) {
        const std::vector<std::uint8_t> file = JukeReadWhole(path);
        if (media::IsXaClip(file.data(), file.size())) {
            // DIVERGENCE DIV-0215: a disc jingle kept as an XA clip is decoded
            // and resampled by media::XaClip, the engine's own reader.
            media::XaClip clip;
            char why[160];
            if (!clip.Open(file.data(), file.size(), why, sizeof why))
                JukeFatal(path + L" is not a valid XA clip: " + WideOf(why));
            const std::uint64_t total = clip.TotalFrames();
            pcm_.resize(static_cast<std::size_t>(total) * 2);
            if (clip.Read(pcm_.data(), static_cast<std::size_t>(total)) != total)
                JukeFatal(path + L" is an XA clip that decodes short");
            frames_ = static_cast<std::uint32_t>(total);
            if (frames_ == 0) JukeFatal(path + L" decodes to no samples");
            return;
        }
        mp3dec_t dec;
        mp3dec_init(&dec);
        std::vector<mp3d_sample_t> frame(MINIMP3_MAX_SAMPLES_PER_FRAME);
        std::size_t at = 0;
        while (at < file.size()) {
            mp3dec_frame_info_t info;
            const int samples = mp3dec_decode_frame(&dec, file.data() + at, static_cast<int>(file.size() - at),
                                                    frame.data(), &info);
            if (info.frame_bytes <= 0) break;
            at += static_cast<std::size_t>(info.frame_bytes);
            if (samples == 0) continue;
            if (info.channels != 2 || info.hz != 44100)
                JukeFatal(path + L" has a frame that is not 44,100 Hz stereo");
            pcm_.insert(pcm_.end(), frame.begin(), frame.begin() + 2 * samples);
        }
        frames_ = static_cast<std::uint32_t>(pcm_.size() / 2);
        if (frames_ == 0) JukeFatal(path + L" decodes to no samples");
        if (loops_) {
            for (const music_loops::Row& r : kRows) {
                if (r.track != track || r.file_bytes != file.size()) continue;
                if (r.start >= r.end || r.end + r.fade > frames_ || r.start + r.fade > frames_)
                    JukeFatal(path + L": its loop row runs past the decoded stream");
                row_ = &r;
            }
        }
    }

    void Render(std::int16_t* out, int frames) override {
        for (int i = 0; i < frames; ++i, out += 2) {
            if (ended_) {
                out[0] = out[1] = 0;
                continue;
            }
            const std::uint32_t stop = row_ ? row_->end : frames_;
            if (pos_ >= stop) {
                if (!loops_) {
                    ended_ = true;
                    out[0] = out[1] = 0;
                    continue;
                }
                // DIV-0081's measured loop, or the port's rewind to the file's start
                pos_ = row_ ? row_->start : 0;
                fade_ = 0;
                fading_ = row_ && row_->fade > 0;
            }
            const std::int16_t* s = &pcm_[2 * static_cast<std::size_t>(pos_)];
            if (fading_) {
                // the game's crossfade (sound.cpp LoopDecode): the samples after
                // the end, faded out over the start's
                const std::int16_t* a = &pcm_[2 * static_cast<std::size_t>(row_->end + fade_)];
                const std::int32_t w = static_cast<std::int32_t>(2 * fade_ + 1);
                const std::int32_t f2 = static_cast<std::int32_t>(2 * row_->fade);
                for (int c = 0; c < 2; ++c) out[c] = static_cast<std::int16_t>((a[c] * (f2 - w) + s[c] * w) / f2);
                if (++fade_ == row_->fade) fading_ = false;
            } else {
                out[0] = s[0];
                out[1] = s[1];
            }
            ++pos_;
        }
    }

    bool Ended() const override { return ended_; }
    bool Measured() const { return row_ != nullptr; }

private:
    static constexpr music_loops::Row kRows[] = {
#include "game/music_loops_table.inc"
    };

    std::vector<std::int16_t> pcm_;
    std::uint32_t frames_ = 0;
    std::uint32_t pos_ = 0;
    const music_loops::Row* row_ = nullptr;
    bool loops_;
    bool ended_ = false;
    bool fading_ = false;
    std::uint32_t fade_ = 0;
};

// A disc song from the cache through the engine's sequencer, started as the
// game's music_seq.cpp Begin starts one: Play at volume 1 over one frame,
// then SsSepSetVol(127, 127). The loop is in the song's data.
class SeqStream : public JukeStream {
public:
    SeqStream(const std::wstring& song_path, const std::wstring& bank_dir) : synth_(new psx::MusicSynth) {
        const std::vector<std::uint8_t> song_bytes = JukeReadWhole(song_path);
        JukeReading(song_path);
        psx::LoadSong(song_bytes.data(), song_bytes.size(), &song_);
        const std::wstring bank_path = bank_dir + std::wstring(song_.bank.begin(), song_.bank.end()) + L".DAT";
        const std::vector<std::uint8_t> bank_bytes = JukeReadWhole(bank_path);
        JukeReading(bank_path);
        psx::LoadBank(bank_bytes.data(), bank_bytes.size(), &bank_);
        psx::MusicSynth::CheckBank(bank_);
        JukeReading(std::wstring());
        synth_->LoadBank(bank_);
        synth_->Play(song_, 1, 1);
        synth_->SetVolume(127, 127);
    }

    void Render(std::int16_t* out, int frames) override { synth_->Render(out, frames); }
    bool Ended() const override { return synth_->Ended(); }

private:
    std::unique_ptr<psx::MusicSynth> synth_;
    psx::Song song_;
    psx::Bank bank_;
};

// ---- the sound out: SDL's device stream, refilled in blocks of 2048 frames ---

constexpr int kFrames = 2048;   // the Win32 player's buffer: 46 ms

struct Out {
    SDL_AudioStream* stream = nullptr;
    bool up = false;                       // SDL's audio subsystem, started by Open
    std::unique_ptr<JukeStream> playing;   // under the stream's lock
    std::atomic<bool> ended{false};
    std::int16_t block[2 * kFrames] = {};
    std::FILE* samples = nullptr;          // a check's file (JukeOutCheck)
    std::atomic<std::uint64_t> written{0};
};
Out g_out;

// One block, as the Win32 Output's Fill: rendered from the stream, or silence.
// On SDL's audio thread, under the stream's lock.
void Fill() {
    Out& o = g_out;
    if (o.playing) {
        o.playing->Render(o.block, kFrames);
        if (o.samples) {
            std::fwrite(o.block, sizeof o.block, 1, o.samples);
            o.written += kFrames;
        }
        if (o.playing->Ended() && !o.ended.exchange(true)) WakeFrame();   // the window says it ended
    } else {
        std::memset(o.block, 0, sizeof o.block);
    }
}

// SDL's get-callback, on its audio thread with the stream locked: blocks
// until what SDL asked for is handed over.
void SDLCALL Feed(void*, SDL_AudioStream* stream, int additional, int) {
    while (additional > 0) {
        Fill();
        SDL_PutAudioStreamData(stream, g_out.block, static_cast<int>(sizeof g_out.block));
        additional -= static_cast<int>(sizeof g_out.block);
    }
}

}  // namespace

std::unique_ptr<JukeStream> JukeMp3(const std::wstring& path, unsigned track, bool loops, bool* measured) {
    auto* mp3 = new Mp3Stream(path, track, loops);
    if (measured) *measured = mp3->Measured();
    return std::unique_ptr<JukeStream>(mp3);
}

std::unique_ptr<JukeStream> JukeSeq(const std::wstring& song_path, const std::wstring& bank_dir) {
    return std::unique_ptr<JukeStream>(new SeqStream(song_path, bank_dir));
}

void JukeArmAbort() { psx::SetMusicAbortHook(MusicAbort); }

bool JukeOutOpen() {
    Out& o = g_out;
    if (o.stream) return true;
    // A check never sounds, whatever SDL_AUDIODRIVER says.
    if (g_check) SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    o.up = true;
    const SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, 44100};   // 44.1 kHz 16-bit stereo, as waveOut was opened
    o.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Feed, nullptr);
    if (!o.stream) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        o.up = false;
        return false;
    }
    o.ended = false;
    SDL_ResumeAudioStreamDevice(o.stream);
    return true;
}

void JukeOutClose() {
    Out& o = g_out;
    if (o.stream) SDL_DestroyAudioStream(o.stream);   // the device closes; no callback runs after it
    o.stream = nullptr;
    o.playing.reset();
    o.ended = false;
    if (o.up) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    o.up = false;
    if (o.samples) std::fflush(o.samples);
}

void JukeOutSet(std::unique_ptr<JukeStream> stream) {
    Out& o = g_out;
    if (o.stream) SDL_LockAudioStream(o.stream);
    o.playing.swap(stream);
    o.ended = false;
    if (o.stream) SDL_UnlockAudioStream(o.stream);
    // the old stream is destroyed here, outside the lock
}

bool JukeOutEnded() { return g_out.ended; }

void JukeOutCheck(const std::string& samples) {
    g_check = true;
    if (samples.empty() || g_out.samples) return;
    g_out.samples = std::fopen(samples.c_str(), "wb");
    if (!g_out.samples) {
        std::fprintf(stderr, "bof3x-launcher: cannot write %s\n", samples.c_str());
        bof3::os::EndProcess(1);
    }
}

std::uint64_t JukeOutSamples() { return g_out.written; }

std::string JukeOutDriver() {
    const char* name = g_out.stream ? SDL_GetCurrentAudioDriver() : nullptr;
    return name ? name : "";
}

}  // namespace bof3x::ui

// The sound layer - 23 functions of the PC port's own sound driver over
// DirectSound: the sound effects (Sound_PlayEffect and the buffer helpers under
// it), the music (Music_Play, the fades, the MP3 stream and its pump) and
// Sound_Tick, which WinMain calls on every spin of its frame wait. No PSX twin
// for any body: the PSX drives the SPU (SE_Play 0x8015E908 in the sibling's
// symbols), the port rewrote all of it. The MP3 decoder under it stays
// Capcom's. The start-up fuzz is sound_fuzz.cpp; docs/sound.md.
//
// Five of them return a value in eax that symbols.toml types as void, because
// callers written against that type exist: Sound_PlayEffect (0), Music_Play
// and the three fades (frames, or the track). Of their 1,500 or so call sites
// many pass eax on to their own callers, so ours return the same: each is
// defined under the assembler name of the void declaration, with an int
// result.
#include "game/sound.h"

#include <malloc.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "audio/mp3.h"
#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/display_env_callees.h"
#include "game/mode_flow.h"
#include "game/music_loops.h"
#include "game/music_resume.h"
#include "game/music_seq.h"
#include "game/save_menu.h"
#include "game/sound_callees.h"
#include "game/sound_in.h"
#include "game/sound_st.h"
#include "game/sound_sp.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/snapshot.h"

namespace sound {
namespace {

unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
std::uint32_t Dword(std::uint32_t address) {
    std::uint32_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void SetDword(std::uint32_t address, std::uint32_t v) { std::memcpy(At(address), &v, sizeof v); }
std::uint32_t Address(const volatile void* p) { return Low32(p); }

}  // namespace
}  // namespace sound

// The five with an eax result, under the assembler names of the void
// declarations symbols.gen.h makes (common/asm_name.h: the i686 ABI's leading
// underscore, none on x86-64).
extern "C" int __cdecl Sound_PlayEffectEax(unsigned short id) BOF3_ASM_NAME(Sound_PlayEffect);
extern "C" int __cdecl Music_PlayEax(unsigned int track, int frames) BOF3_ASM_NAME(Music_Play);
extern "C" int __cdecl Music_FadeOutStopEax(int frames) BOF3_ASM_NAME(Music_FadeOutStop);
extern "C" int __cdecl Music_FadeInEax(int frames) BOF3_ASM_NAME(Music_FadeIn);
extern "C" int __cdecl Music_FadeOutEax(int frames) BOF3_ASM_NAME(Music_FadeOut);

namespace sound {

// BOF3X_MUSIC_LOG=1: one log line per music command (play, the fades, start,
// stop) with its caller and the logic frame - a diagnostic for the music-stop
// reports (known-defects.md D241), off by default; no behaviour.
bool MusicLogOn() {
    static const bool on = [] {
        const char* v = std::getenv("BOF3X_MUSIC_LOG");
        return v && v[0] && v[0] != '0';
    }();
    return on;
}

// Under the same switch, once every 60 logic frames while a track's stream
// is live: the sample heard against the samples written (RingLog, below).
// Held off (1) while the injection-time self-tests run, and for the whole of
// a BOF3X_SELFTEST_ONLY run: the stand-in buffers answer no
// GetCurrentPosition, and the fuzz counts every call.
std::uint8_t g_ring_log_hold = 0;

}  // namespace sound

#define MUSIC_LOG(...)     do { if (sound::MusicLogOn()) bof3::Log("music_log   f%u " __VA_ARGS__); } while (0)

namespace sound {


namespace {

// Music_SetVolume with the float's bits, passed as the originals pass them
// (a dword move, never an x87 load that would quiet a signalling NaN): the
// bits copied into a float and handed over as the float parameter it is -
// an SSE move on both widths, so the bit pattern arrives whole. (Until the
// 64-bit build this called through a `void (*)(std::uint32_t)` type, which
// on i686 is the same stack dword and on x86-64 is rcx where the callee
// reads xmm0: the first 64-bit self-test read garbage volumes.)
void SetVolumeBits(std::uint32_t bits) {
    float volume;
    std::memcpy(&volume, &bits, sizeof volume);
    g.music_set_volume(volume);
}

// GetStatus's bit 0, DSBSTATUS_PLAYING, into `status` - which holds on entry
// what the original's slot held, for a GetStatus that did not write it.
bool Playing(void* buffer, unsigned long status) {
    Method<ComStatus>(buffer, kGetStatus)(buffer, &status);
    return (status & 1) != 0;
}

}  // namespace
}  // namespace sound

using namespace sound;

// DIV-0028's state (the switch and the deadline Sound_Tick steps on), here
// because the music commands below need it too.
//
// A stopping fade that lasts frames must not change what the game can see.
// In the original it is over at the first Sound_Tick after it starts - in the
// frame wait at the end of the logic frame that asked for it: the music
// stopped and Music_Track 0xFF, which some thirty functions read. So under
// DIV-0028 that first tick marks the track stopped and only the sound
// lingers; and a music command that arrives after it, while the sound still
// fades, completes the stop first, as the original's would already have
// happened. Within the asking frame nothing changes: the original's commands
// there see the track still playing, and so do ours. Without this, Music_Play
// of the same track inside the fade did nothing and the fade then stopped the
// music: the attract sequence's frame 3439, where the original restarts a
// track and ours stayed silent (the ab25 frame hash).
namespace sound {
std::uint32_t g_fade_per_frame = 0;  // 1 through PatchBytes("MusicFadePerFrame")
std::uint32_t g_fade_frame = ~0u;    // Frame_Counter at the last step (DIV-0028)
std::uint32_t g_stop_unseen = 0;     // a stopping fade no Sound_Tick has seen yet

void FinishStoppingFade() {
    if (!g_fade_per_frame || g_stop_unseen || Music_FadeCount == 0 || !Music_FadeStops) return;
    Music_FadeCount = 0;
    g.music_stop();
    Music_Track = 0xFF;
}
}  // namespace sound

// DIVERGENCE DIV-0168: 1 through PatchBytes("MusicPlayRefusesNone") at the
// end of Sound_Inject, after every self-test there (they compare Capcom's
// Music_Play).
namespace sound {
std::uint8_t g_play_refuses_none = 0;
}

// DIVERGENCE DIV-0167: the PlayStation's SsUtKeyOffV on voices 16..23 at the
// start of a battle action's effect (BattleAction_EffectPreload; US
// 0x801D3E80, JP 0x801D3E60), on the PC's channels. A cue word names its
// voice in bits 8-12, and the shipped banks name only 16..23, the
// PlayStation's sound-effect voices (docs/sound.md section 1);
// Sound_PlayEffect files voice v under channel v, less 23 from 23 on - so
// channels 16..22 and 0. Each of those that remembers a buffer has it stopped
// and forgotten, as a cue's 0xFF voice does. The PC remembers only a looping
// voice's buffer (a one-shot's is the bank entry's, shared by every cue that
// names it), so a one-shot still sounding plays to its end where the
// PlayStation's key-off would release it.
namespace sound {
void KeyOffEffectVoices() {
    for (std::uint32_t voice = 16; voice <= 23; ++voice) {
        const std::uint32_t channel = voice >= 0x17 ? voice - 0x17 : voice;
        void* const playing = reinterpret_cast<void*>(Sound_Channels[channel]);
        if (!playing) continue;
        g.sndbuf_stop(playing);
        Sound_Channels[channel] = 0;
    }
}
}  // namespace sound

// DIVERGENCE DIV-0132: the PlayStation's duck and its undo for the PC's fade
// to nothing and back to 127. The PlayStation's song plays at sequence volume
// 97 (Music_Play's crescendo to 100 ends there, libsnd-reading.md 5.1);
// _SsSndDecrescendo of 0x14 over 0x10 VSyncs takes one unit a VSync, 16 in
// all, to 81 - amplitude (81 / 97)^2, -313 hundredths of a decibel. The duck
// asks the same of the buffer: under the sequencer (music_seq::Level, the
// same square law) the volume times 81 / 97; under an MP3 (Music_SetVolume,
// 10000 / 127 hundredths a unit) the volume less 313 * 127 / 10000. Carried
// by the fade state Sound_Tick steps (no stop at its end), so a later fade or
// Music_Play takes over as it would from Capcom's.
namespace sound {
namespace {
constexpr float kPsxSongVolume = 97.0f;
constexpr float kPsxDuck = 16.0f;
float g_unducked = -1.0f;  // the volume DuckMusic found; below 0, none outstanding

void FadeTo(float target, int frames) {
    sound::FinishStoppingFade();  // DIV-0028
    Music_FadeStep = frames > 0 ? (target - Music_Volume) / static_cast<float>(frames) : 0.0f;
    if (frames <= 0) {
        Music_Volume = target;
        SetVolumeBits(Dword(Address(&Music_Volume)));
    }
    Music_FadeStops = 0;
    Music_FadeCount = frames > 0 ? frames : 0;
}
}  // namespace

void DuckMusic(int frames) {
    MUSIC_LOG("DuckMusic(%d) track %u volume %.2f", Frame_Counter, frames, (unsigned)Music_Track, Music_Volume);
    const float from = Music_Volume;
    float target;
    if (music_seq::Active()) {
        target = from * (kPsxSongVolume - kPsxDuck) / kPsxSongVolume;
    } else {
        const float hundredths = 4000.0f * std::log10((kPsxSongVolume - kPsxDuck) / kPsxSongVolume);
        target = from + hundredths * 127.0f / 10000.0f;
    }
    if (target < 0.0f) target = 0.0f;
    g_unducked = from;
    FadeTo(target, frames);
}

void UnduckMusic(int frames) {
    MUSIC_LOG("UnduckMusic(%d) track %u volume %.2f to %.2f", Frame_Counter, frames, (unsigned)Music_Track, Music_Volume, g_unducked);
    const float target = g_unducked >= 0.0f ? g_unducked : 127.0f;
    g_unducked = -1.0f;
    FadeTo(target, frames);
}
}  // namespace sound

// original 0x587C70: WinMain calls it on every spin while it waits out the
// frame (6.7 million calls in an attract run) - so no logging here. The music
// stream is kept fed; then, while a fade runs, one step of it: the volume plus
// the step (x87, stored as a float), handed on, and at the fade's end, for a
// stopping fade, the music stopped and no track playing.
// As the original has it: the step is taken per call, not per frame - a fade
// lasts `frames` spins of the wait loop, near instant (known-defects D26,
// docs/sound.md section 3); the count is
// read again after Music_SetVolume.
//
// DIV-0028: unless BOF3X_ORIGINAL=MusicFadePerFrame, a step is taken only on
// the first call after Frame_Counter has moved. WinMain increments it at the
// end of every pass through the frame path (0x4FCFE5 in the original; ours
// likewise), and reaches this at least once in every logic frame - once from
// 0x4FCEBC when the frame is late, replayed frames included - so a fade lasts
// `frames` logic frames, as on the PlayStation. The first call of a new fade
// steps at once (the remembered frame is an older one). Until 2026-10-10 the
// step keyed on the float frame deadline at 0x6BC628 instead, which DIV-0047's
// WinMain (2026-09-24) never writes: every fade took one step and stalled, so
// a stopping fade never stopped the music (known-defects.md D241).
namespace sound {
void RingLog();
}

// DIV-pending (music_loops.h, docs/bgm-comparison.md section 12): the loop of
// a track with a measured row. Every frame is decoded into a frame buffer of
// ours and copied out, so the stream can be cut at any sample: at the row's
// end the decoder is rewound (Mp3_Seek(decoder, 0), the one position whose
// meaning is measured) and decoded again from the file's start, every frame
// before the one holding the row's start thrown away, and that frame's
// samples from the start on carried into the output. Decoding from the start
// leaves the decoder exactly as on the first pass (bit reservoir, overlap), so
// the samples after the jump are the first pass's, bit for bit. What does not
// fit the caller's `size` is carried to the next call - the original writes
// whole frames, and 0x12000 bytes is a whole number of them; a cut stream is
// not.
namespace sound { Loop g_loop; }
namespace {
void LoopBegin(int track, unsigned size) {
    sound::g_loop.row = music_loops::g_on ? music_loops::Find(track, size) : nullptr;
    sound::g_loop.pos = 0;
    sound::g_loop.carry_at = sound::g_loop.carry_len = 0;
}

}  // namespace

// DIV-0092's resume (music_resume.h): the ring's bookkeeping and the
// position. All of it the DLL's own state.
namespace sound { Ring g_ring; }

namespace sound { Resume g_resume; }
namespace {
// The quicksave's rebuild (SOUND, below): the stream the restore starts again
// is advanced to the sample heard at the take, whatever stream it is - a BGM
// track or Sound_LoadStream's - by ResumeSkip, as DIV-0092's resume advances
// a track. Armed only inside the restore, for the one Music_Start it makes.
// (RestoreSkip, its type, is in sound.h with Loop, Ring and Resume: source-layout B0)

}  // namespace
namespace sound { RestoreSkip g_restore_skip; }
namespace {
constexpr std::uint32_t kFrameSamples = 1152;  // every frame of the game's MP3s (bgm-comparison.md 6.1)
constexpr std::uint32_t kHalfSamples = kHalf / 4;
}  // namespace
// Music_Start's resume, after the stream is set up and before its first half
// is decoded: the stream advanced `target` samples. The sequenced song by
// FastForward; the MP3 by decoding through Music_Decode into a scratch buffer
// and dropping it (the loop table's own mechanism: decoding from the top
// leaves the decoder as the first pass left it, bit for bit; the decoders
// seek to 0 alone). Past a once-only song's end: from the top. The stream
// sample reached.
namespace sound { std::uint32_t ResumeSkip(std::uint32_t target, int loops, int loop_track, unsigned size) {
    if (music_seq::Active()) {
        if (music_seq::FastForward(target)) return target;
        music_seq::Begin();
        return 0;
    }
    if (!Music_Decoder) return 0;
    static unsigned char scratch[kHalf];
    const int saved_loops = Music_Loops;
    Music_Loops = loops;  // the original sets it after the first half; the skip runs on the new track's
    Music_Finished = 0;
    std::uint32_t left = target;
    while (left && !Music_Finished) {
        const std::uint32_t n = left < kHalfSamples ? left : kHalfSamples;
        Music_Decode(scratch, static_cast<int>(n * 4));
        left -= n;
    }
    std::uint32_t reached = target;
    if (Music_Finished) {  // a once-only track asked past its end: from the top
        g.mp3_seek(Music_Decoder, 0);
        LoopBegin(loop_track, size);
        Music_Finished = 0;
        reached = 0;
    }
    Music_Loops = saved_loops;
    return reached;
} }

// DIV-0092's resume (music_resume.h): the position taken and handed on.
namespace {

// A heard MP3 stream sample as the same point inside the first pass (and, for
// a measured loop, its first loop), where Music_Start's skip can reach it by
// decoding from the top. A measured loop repeats start..end after `end`, its
// first `fade` samples blended with the end's: a point in the blend is taken
// in the second pass, any other in the first. A track the original rewinds
// repeats every `pass` samples (its frames and the 1,152 the end-of-stream
// step counts, known-defects.md D240). Without either (a once-only track) the
// point stays; Music_Start's skip finds it past the end and starts from the
// top. The original's path decodes whole frames, so its point is taken down
// to a frame's start (26 ms at most).
std::uint32_t FoldMp3(std::uint32_t heard, const music_loops::Row* row, bool loops, std::uint32_t pass) {
    if (row) {
        if (heard < row->end) return heard;
        const std::uint32_t k = (heard - row->end) % (row->end - row->start);
        return k >= row->fade ? row->start + k : row->end + k;
    }
    if (loops && pass) heard %= pass;
    return heard - heard % kFrameSamples;
}

// The stream sample under `buffer`'s play cursor (the ring's bookkeeping) and
// the same point folded into the first pass as above; `size` is the bytes of
// Music_Data (a rewinding MP3's period is counted from them). False when the
// cursor is in a half not yet written: no position to trust.
bool HeardFolded(void* buffer, std::uint32_t size, std::uint32_t* heard_out, std::uint32_t* folded) {
    unsigned long play = 0, cursor_w = 0;
    if (Method<ComPosition>(buffer, kGetCurrentPosition)(buffer, &play, &cursor_w) != 0 || play >= 2 * kHalf) return false;
    const std::uint32_t heard = sound::g_ring.half_base[play / kHalf] + static_cast<std::uint32_t>(play % kHalf) / 4;
    if (heard > sound::g_ring.written) return false;  // a cursor in a half not yet written: no position to trust
    if (music_seq::Active()) {
        *folded = music_seq::Fold(heard);
    } else {
        const bool loops = Music_Loops != 0;
        const std::uint32_t pass =
            loops && !sound::g_loop.row && Music_Data ? (mp3dec::CountFrames(Music_Data, size) + 1) * kFrameSamples : 0;
        *folded = FoldMp3(heard, sound::g_loop.row, loops, pass);
    }
    *heard_out = heard;
    return true;
}

}  // namespace

namespace sound {

// BOF3X_MUSIC_LOG=1's periodic line, from Sound_Tick: once in every 60 logic
// frames (the first tick whose frame falls in a new sixty) while a track
// stream is live - the track, the stream sample under the play cursor (`-`
// when the cursor is in a half not yet written), the samples written into
// the ring, and whether the buffer plays. The same text on both widths;
// tools/music_heard.py reads it: a track `playing` whose heard sample does
// not move is the 64-bit round's silent stream (docs/x64-build-round.md 5).
void RingLog() {
    if (g_ring_log_hold) return;
    const std::uint32_t frame = Frame_Counter;
    static std::uint32_t slot = ~0u;
    if (frame / 60 == slot) return;
    slot = frame / 60;
    void* const buffer = Music_Buffer;
    const unsigned track = Music_Track;
    if (track == 0xFF || !buffer || !g_ring.track_stream) return;
    std::uint32_t heard = 0, folded = 0;
    const bool trusted = HeardFolded(buffer, static_cast<std::uint32_t>(Music_FileSize), &heard, &folded);
    const bool playing = Playing(buffer, 0);
    char heard_text[16];
    if (trusted) std::snprintf(heard_text, sizeof heard_text, "%u", heard);
    else std::snprintf(heard_text, sizeof heard_text, "-");
    bof3::Log("music_log   f%u ring track %u heard %s written %u %s", frame, track, heard_text, g_ring.written,
              playing ? "playing" : "halted");
}

}  // namespace sound

namespace music_resume {

void Remember() {
    sound::g_resume.remembered = false;
    if (!mode_flow::g_area_track_fallback) return;  // DIV-0092's switch: Capcom's rule, and the fuzzes
    const unsigned track = Music_Track;
    void* const buffer = Music_Buffer;
    if (track == 0xFF || !buffer || !sound::g_ring.track_stream || sound::g_ring.track != static_cast<int>(track)) return;
    std::uint32_t heard = 0, folded = 0;
    if (!HeardFolded(buffer, static_cast<std::uint32_t>(Music_FileSize), &heard, &folded)) return;
    sound::g_resume.remembered = true;
    sound::g_resume.track = track;
    sound::g_resume.sample = folded;
    MUSIC_LOG("remember track %u at sample %u (heard %u; %u written, the ring %u samples ahead)", Frame_Counter, track,
              folded, heard, sound::g_ring.written, sound::g_ring.written - heard);
}

bool Arm(unsigned track) {
    const bool hit = sound::g_resume.remembered && sound::g_resume.track == track;
    sound::g_resume.remembered = false;
    sound::g_resume.pending = hit;
    sound::g_resume.pending_track = track;
    sound::g_resume.pending_sample = hit ? sound::g_resume.sample : 0;
    return hit;
}

void Forget() {
    sound::g_resume.remembered = false;
    sound::g_resume.pending = false;
}

void Pend(unsigned track, unsigned sample) {
    sound::g_resume.pending = true;
    sound::g_resume.pending_track = track;
    sound::g_resume.pending_sample = sample;
}

bool Heard(unsigned* track_out, unsigned* sample_out) {
    const unsigned track = Music_Track;
    void* const buffer = Music_Buffer;
    if (track == 0xFF || !buffer || !sound::g_ring.track_stream || sound::g_ring.track != static_cast<int>(track)) return false;
    if (!Playing(buffer, 0)) return false;
    std::uint32_t heard = 0, folded = 0;
    if (!HeardFolded(buffer, static_cast<std::uint32_t>(Music_FileSize), &heard, &folded)) return false;
    *track_out = track;
    *sample_out = folded;
    MUSIC_LOG("heard track %u at sample %u (heard %u; %u written, the ring %u samples ahead)", Frame_Counter, track,
              folded, heard, sound::g_ring.written, sound::g_ring.written - heard);
    return true;
}

bool Kept(unsigned* track_out, unsigned* sample_out, bool* playing_out) {
    const unsigned track = Music_Track;
    void* const buffer = Music_Buffer;
    if (track == 0xFF || !buffer || !sound::g_ring.track_stream || sound::g_ring.track != static_cast<int>(track)) return false;
    std::uint32_t heard = 0, folded = 0;
    if (!HeardFolded(buffer, static_cast<std::uint32_t>(Music_FileSize), &heard, &folded)) return false;
    const bool playing = Playing(buffer, 0);
    *track_out = track;
    *sample_out = folded;
    *playing_out = playing;
    MUSIC_LOG("kept track %u at sample %u, %s (heard %u; %u written)", Frame_Counter, track, folded,
              playing ? "playing" : "halted", heard, sound::g_ring.written);
    return true;
}

}  // namespace music_resume

// DIV-0028's switch and its check. Under BOF3X_SHADOW=sound (after the fuzz,
// which ran the per-call path the original has): an 8-frame stopping fade
// driven through Sound_Tick with counting stand-ins - five calls within one
// frame take one step, then each of seven new deadlines takes one more, and
// the eighth step stops the music. Everything it touches is put back.
namespace {

int g_fpf_volume_calls, g_fpf_stop_calls, g_fpf_start_calls;
void __cdecl FpfPump() {}
void __cdecl FpfSetVolume(float) { ++g_fpf_volume_calls; }
void __cdecl FpfStop() { ++g_fpf_stop_calls; }
void __cdecl FpfStart(const void*, unsigned, int) { ++g_fpf_start_calls; }
int __cdecl FpfLoad(unsigned) { return 0; }

void FadePerFrame_SelfTest() {
    const Callees saved_g = g;
    const auto count = Music_FadeCount;
    const auto stops = Music_FadeStops;
    const auto volume = Music_Volume;
    const auto step = Music_FadeStep;
    const auto track = Music_Track;
    const std::uint32_t frame_saved = Frame_Counter;
    const std::uint32_t remembered = sound::g_fade_frame;

    g.music_pump = FpfPump;
    g.music_set_volume = FpfSetVolume;
    g.music_stop = FpfStop;
    g_fpf_volume_calls = g_fpf_stop_calls = 0;
    sound::g_fade_per_frame = 1;
    Music_Volume = 127.0f;
    Music_FadeOutStopEax(8);
    bool ok = true;
    const std::uint32_t frame = 0x45000000;  // any value unlike the remembered one
    sound::g_fade_frame = ~0u;
    for (int f = 0; f < 8; ++f) {
        Frame_Counter = frame + static_cast<std::uint32_t>(f);
        for (int spin = 0; spin < 5; ++spin) Sound_Tick();
        ok = ok && g_fpf_volume_calls == f + 1 && static_cast<int>(Music_FadeCount) == 7 - f;
    }
    ok = ok && g_fpf_stop_calls == 1 && Music_Track == 0xFF;
    for (int spin = 0; spin < 5; ++spin) Sound_Tick();  // a finished fade takes no more steps
    ok = ok && g_fpf_volume_calls == 8 && g_fpf_stop_calls == 1;
    const bool first_ok = ok;

    // A stopping fade, then Music_Play of the same track: in the asking frame
    // it is ignored (the track still plays, as in the original); after the
    // first tick the game sees the track stopped, and a Music_Play restarts it
    // - the stop completed first.
    g.music_start = FpfStart;
    g.music_load_file = FpfLoad;
    g_fpf_stop_calls = g_fpf_start_calls = 0;
    Music_Track = 5;
    Music_Volume = 127.0f;
    Music_FadeOutStopEax(8);
    Music_PlayEax(5, 8);
    ok = ok && Music_Track == 5 && g_fpf_start_calls == 0 && g_fpf_stop_calls == 0;
    Frame_Counter = frame + 0x1000;
    Sound_Tick();
    ok = ok && Music_Track == 0xFF && g_fpf_stop_calls == 0;
    Music_PlayEax(5, 8);
    ok = ok && Music_Track == 5 && g_fpf_stop_calls == 1 && g_fpf_start_calls == 1 && !Music_FadeStops;
    const bool second_ok = ok;

    sound::g_fade_per_frame = 0;
    sound::g_stop_unseen = 0;
    g = saved_g;
    Music_FadeCount = count;
    Music_FadeStops = stops;
    Music_Volume = volume;
    Music_FadeStep = step;
    Music_Track = track;
    Frame_Counter = frame_saved;
    sound::g_fade_frame = remembered;
    if (!first_ok)
        bof3::Fatal("DIV-0028 self-test: %d volume steps, %d stops for an 8-frame fade over 8 frames of 5 spins",
                    g_fpf_volume_calls, g_fpf_stop_calls);
    if (!second_ok)
        bof3::Fatal("DIV-0028 self-test: Music_Play during a stopping fade - %d stops, %d starts",
                    g_fpf_stop_calls, g_fpf_start_calls);
    bof3::Log("shadow      DIV-0028 self-test: an 8-frame fade took 8 steps over 8 frames of 5 spins, stopped once; "
              "Music_Play of its track ignored in the asking frame, restarted after the first tick");
}

// The measured loops' check (music_loops.h), under BOF3X_SHADOW=sound: a
// stand-in decoder whose every sample carries its own index (left the low 16
// bits, right the high), a row whose start and end fall inside frames, and
// Music_Decode asked for 0x12000 bytes and for 4,000 (not a whole number of
// frames, so the end lands across two calls): the stream must run 0..end-1,
// then start..end-1 for ever, no sample lost or repeated. Then every row of
// the table against its own bounds. Everything it touches is put back.
namespace {

struct FakeFrame { unsigned char pad[8]; std::uint32_t bytes; };
struct FakeDecoder { unsigned char pad[0x14]; FakeFrame* frame; };
FakeFrame g_lt_frame;
FakeDecoder g_lt_decoder;
std::uint32_t g_lt_next, g_lt_frames, g_lt_seeks;

int __stdcall LtDecode(void*, void* out) {
    if (g_lt_next >= g_lt_frames) return kEndOfStream;
    auto* p = static_cast<std::uint16_t*>(out);
    for (std::uint32_t i = 0; i < 1152; ++i) {
        const std::uint32_t idx = g_lt_next * 1152 + i;
        p[2 * i] = static_cast<std::uint16_t>(idx);
        p[2 * i + 1] = static_cast<std::uint16_t>(idx >> 16);
    }
    ++g_lt_next;
    g_lt_frame.bytes = 4608;
    return 0;
}
int __stdcall LtSeek(void*, int position) {
    if (position != 0) bof3::Fatal("music_loops self-test: a seek to %d", position);
    g_lt_next = 0;
    ++g_lt_seeks;
    return 0;
}

// What a run should hand out inside a crossfade: the engine's blend of the
// index-coded lanes of end + j and start + j.
std::uint32_t LtBlend(std::uint32_t a, std::uint32_t b, std::uint32_t j, std::uint32_t fade) {
    std::uint32_t out = 0;
    for (int c = 0; c < 2; ++c) {
        const std::int32_t x = static_cast<std::int16_t>(a >> (16 * c)), y = static_cast<std::int16_t>(b >> (16 * c));
        const std::int32_t w = static_cast<std::int32_t>(2 * j + 1), f2 = static_cast<std::int32_t>(2 * fade);
        out |= static_cast<std::uint32_t>(static_cast<std::uint16_t>((x * (f2 - w) + y * w) / f2)) << (16 * c);
    }
    return out;
}

bool LtRun(std::uint32_t chunk, std::uint32_t start, std::uint32_t end, std::uint32_t total, unsigned* seeks,
           std::uint32_t fade = 0) {
    static unsigned char buf[0x12000];
    const music_loops::Row row = {200, 0, start, end, fade};
    sound::g_loop.row = &row;
    sound::g_loop.pos = sound::g_loop.carry_at = sound::g_loop.carry_len = 0;
    g_lt_next = 0;
    g_lt_seeks = 0;
    std::uint32_t expect = 0, out = 0;
    bool ok = true;
    while (out < total && ok) {
        Music_Decode(buf, static_cast<int>(chunk));
        for (std::uint32_t i = 0; i < chunk / 4 && ok; ++i, ++out) {
            const std::uint16_t* s = reinterpret_cast<const std::uint16_t*>(buf) + 2 * i;
            const std::uint32_t idx = s[0] | static_cast<std::uint32_t>(s[1]) << 16;
            // after the first pass the first `fade` samples from the start are blends of end + j and start + j
            const std::uint32_t j = expect - start;
            const bool faded = out >= end && expect >= start && j < fade;
            ok = idx == (faded ? LtBlend(end + j, start + j, j, fade) : expect);
            expect = expect + 1 == end ? start : expect + 1;
        }
    }
    sound::g_loop.row = nullptr;
    *seeks = g_lt_seeks;
    return ok;
}

void LoopTable_SelfTest() {
    const Callees saved_g = g;
    void* const decoder = Music_Decoder;
    g.mp3_decode = LtDecode;
    g.mp3_seek = LtSeek;
    g_lt_decoder.frame = &g_lt_frame;
    Music_Decoder = &g_lt_decoder;
    g_lt_frames = 40;
    const std::uint32_t start = 1152 * 3 + 500, end = 1152 * 20 + 77, total = 1152 * 120;
    unsigned seeks_a = 0, seeks_b = 0, seeks_c = 0;
    const bool a = LtRun(0x12000, start, end, total, &seeks_a);
    const bool b = LtRun(4000, start, end, total, &seeks_b);
    const bool c = LtRun(4000, 1152 * 5, 1152 * 9, total, &seeks_c);  // both on frame boundaries
    unsigned seeks_d = 0;
    const bool d = LtRun(4000, 1152 * 4 + 300, 1152 * 15 + 600, total, &seeks_d, 256);  // with a crossfade
    g = saved_g;
    Music_Decoder = decoder;
    sound::g_loop.pos = sound::g_loop.carry_at = sound::g_loop.carry_len = 0;
    std::memset(sound::g_loop.logged, 0, sizeof sound::g_loop.logged);  // the test's track 200 logged its loop; the game's start unlogged
    if (!a || !b || !c || !d)
        bof3::Fatal("music_loops self-test: the looped stream went wrong (0x12000-byte calls %s, 4,000-byte calls %s, "
                    "frame-aligned points %s, a 256-sample crossfade %s)", a ? "ok" : "WRONG", b ? "ok" : "WRONG",
                    c ? "ok" : "WRONG", d ? "ok" : "WRONG");
    unsigned count = 0;
    const music_loops::Row* rows = music_loops::Rows(&count);
    for (unsigned i = 0; i < count; ++i)
        if (rows[i].start >= rows[i].end || rows[i].end - rows[i].start < 44100)
            bof3::Fatal("music_loops self-test: track %u's row %u..%u", rows[i].track, rows[i].start, rows[i].end);
    bof3::Log("shadow      music_loops self-test: %u samples through a stand-in decoder looped %u..%u exactly, in "
              "0x12000- and 4,000-byte calls, on frame boundaries and with a 256-sample crossfade (%u, %u, %u, %u rewinds); "
              "%u table rows in bounds", total, start, end, seeks_a, seeks_b, seeks_c, seeks_d, count);
}

// DIV-0092's resume on the MP3 path (music_resume.h), with the loop check's
// stand-in decoder (index-coded samples, 40 frames): the stream from the top
// against the stream resumed at a folded position, sample for sample - for a
// track the original rewinds (inside the first pass; three passes on, folded
// back), for a once-only track asked past its end (from the top), for a
// measured loop (two loops on, folded; and inside the loop's crossfade).
// Then the heard position and the switch: the play cursor through a stand-in
// buffer, nothing remembered with DIV-0092 off, the position remembered with
// it on, used once and only for its own track. Everything put back.
std::uint32_t g_rt_cursor;
long __stdcall RtPosition(void*, unsigned long* play, unsigned long* cursor_w) {
    *play = g_rt_cursor;
    *cursor_w = 0;
    return 0;
}
long __stdcall RtUnexpected(void*) { bof3::Fatal("DIV-0092 resume self-test: a DirectSound method other than GetCurrentPosition"); }

// `samples` of the stream as Music_Decode makes it, from the decoder's state
// now, in 0x12000-byte calls (a whole number of frames, as the game asks).
std::vector<std::uint32_t> RtStream(std::uint32_t samples) {
    static unsigned char buf[kHalf];
    std::vector<std::uint32_t> out;
    while (out.size() < samples) {
        std::memset(buf, 0xEE, sizeof buf);  // what a phantom step leaves (D240)
        Music_Decode(buf, static_cast<int>(kHalf));
        for (std::uint32_t i = 0; i < kHalfSamples; ++i) {
            std::uint32_t v;
            std::memcpy(&v, buf + 4 * i, 4);
            out.push_back(v);
        }
    }
    out.resize(samples);
    return out;
}

void RtFresh(const music_loops::Row* row) {
    g_lt_next = 0;
    sound::g_loop.row = row;
    sound::g_loop.pos = sound::g_loop.carry_at = sound::g_loop.carry_len = 0;
    Music_Finished = 0;
}

// The stream resumed at `heard` (folded as Remember folds it) against the
// stream from the top at the folded point's equivalent: the samples a
// phantom step leaves are not compared (they are whatever the buffer held).
bool RtCase(const music_loops::Row* row, bool loops, std::uint32_t heard, std::uint32_t expect_at,
            std::uint32_t* reached) {
    const std::uint32_t pass = (g_lt_frames + 1) * 1152;
    const std::uint32_t folded = FoldMp3(heard, row, loops, pass);
    const std::uint32_t length = 1152 * 70;
    RtFresh(row);
    Music_Loops = loops;
    const std::vector<std::uint32_t> top = RtStream(expect_at + length);
    RtFresh(row);
    *reached = sound::ResumeSkip(folded, loops, -1, 0);
    Music_Loops = loops;
    const std::vector<std::uint32_t> resumed = RtStream(length);
    bool ok = true;
    for (std::uint32_t k = 0; k < length && ok; ++k) {
        const std::uint32_t at = expect_at + k;
        if (!row && loops && at % pass >= g_lt_frames * 1152) continue;  // the phantom step's samples
        if (!row && !loops && at >= g_lt_frames * 1152) continue;        // the zeros after the end
        ok = resumed[k] == top[at];
    }
    return ok;
}

void ResumeMp3_SelfTest() {
    const Callees saved_g = g;
    void* const decoder = Music_Decoder;
    const int loops_saved = Music_Loops, finished = Music_Finished;
    const Ring ring = sound::g_ring;
    const Resume resume = sound::g_resume;
    const unsigned char track_saved = Music_Track;
    void* const buffer_saved = Music_Buffer;
    const std::uint32_t fallback = mode_flow::g_area_track_fallback;
    g.mp3_decode = LtDecode;
    g.mp3_seek = LtSeek;
    g_lt_decoder.frame = &g_lt_frame;
    Music_Decoder = &g_lt_decoder;
    g_lt_frames = 40;
    const std::uint32_t pass = 41 * 1152;
    std::uint32_t r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
    // a rewinding track: inside the first pass; three passes on, folded back
    const bool first = RtCase(nullptr, true, 1152 * 7 + 300, 1152 * 7, &r1) && r1 == 1152 * 7;
    const bool later = RtCase(nullptr, true, 3 * pass + 1152 * 11 + 5, 3 * pass + 1152 * 11, &r2) && r2 == 1152 * 11;
    // a once-only track asked past its end: from the top
    const bool past = RtCase(nullptr, false, 1152 * 50, 0, &r3) && r3 == 0;
    // a measured loop: two loops on, and inside its crossfade one loop on
    const music_loops::Row row = {200, 0, 1152 * 4 + 300, 1152 * 15 + 600, 256};
    const std::uint32_t period = row.end - row.start;
    const bool looped = RtCase(&row, true, row.end + 2 * period + 1000, row.end + 2 * period + 1000, &r4) &&
                        r4 == row.start + 1000;
    const bool blend = RtCase(&row, true, row.end + period + 100, row.end + period + 100, &r5) && r5 == row.end + 100;
    sound::g_loop.row = nullptr;
    sound::g_loop.pos = sound::g_loop.carry_at = sound::g_loop.carry_len = 0;
    std::memset(sound::g_loop.logged, 0, sizeof sound::g_loop.logged);

    // the heard position: half 1 holds samples from 3 halves on, half 0 (the
    // last written) from 4; the cursor 0x800 bytes into half 1
    const void* vtable[0x54 / 4];
    for (auto& v : vtable) v = reinterpret_cast<const void*>(&RtUnexpected);
    vtable[kGetCurrentPosition / 4] = reinterpret_cast<const void*>(&RtPosition);
    struct { const void* const* vtable; } fake = {vtable};
    Music_Buffer = &fake;
    Music_Track = 200;
    Music_Loops = 0;
    sound::g_ring.track_stream = true;
    sound::g_ring.track = 200;
    sound::g_ring.half_base[1] = 3 * kHalfSamples;
    sound::g_ring.half_base[0] = 4 * kHalfSamples;
    sound::g_ring.written = 5 * kHalfSamples;
    g_rt_cursor = kHalf + 0x800;
    const std::uint32_t heard = 3 * kHalfSamples + 0x200, want = heard - heard % 1152;
    mode_flow::g_area_track_fallback = 0;
    music_resume::Remember();
    const bool off = !sound::g_resume.remembered && !music_resume::Arm(200) && !sound::g_resume.pending;
    mode_flow::g_area_track_fallback = 1;
    music_resume::Remember();
    const bool on = sound::g_resume.remembered && sound::g_resume.track == 200 && sound::g_resume.sample == want;
    const bool other = !music_resume::Arm(201) && !sound::g_resume.remembered && !sound::g_resume.pending;
    music_resume::Remember();
    const bool own = music_resume::Arm(200) && !sound::g_resume.remembered && sound::g_resume.pending &&
                     sound::g_resume.pending_sample == want && !music_resume::Arm(200);
    Music_Track = 0xFF;  // nothing playing: nothing remembered
    music_resume::Remember();
    const bool silent = !sound::g_resume.remembered;

    g = saved_g;
    Music_Decoder = decoder;
    Music_Loops = loops_saved;
    Music_Finished = finished;
    sound::g_ring = ring;
    sound::g_resume = resume;
    Music_Track = track_saved;
    Music_Buffer = buffer_saved;
    mode_flow::g_area_track_fallback = fallback;
    if (!first || !later || !past || !looped || !blend || !off || !on || !other || !own || !silent)
        bof3::Fatal("DIV-0092 resume self-test: rewinding track first pass %s, three passes on %s (reached %u, %u); "
                    "once-only past its end %s (reached %u); measured loop two loops on %s, in the crossfade %s "
                    "(reached %u, %u); switch off %s, on %s, another track %s, its own track once %s, silent %s",
                    first ? "ok" : "WRONG", later ? "ok" : "WRONG", r1, r2, past ? "ok" : "WRONG", r3,
                    looped ? "ok" : "WRONG", blend ? "ok" : "WRONG", r4, r5, off ? "ok" : "WRONG", on ? "ok" : "WRONG",
                    other ? "ok" : "WRONG", own ? "ok" : "WRONG", silent ? "ok" : "WRONG");
    bof3::Log("shadow      DIV-0092 resume self-test (MP3 path): a stand-in stream resumed equal to the stream from the "
              "top - a rewinding track at sample %u and three passes on (folded to %u), a once-only track past its end "
              "(from the top), a measured loop two loops on (folded to %u) and in its crossfade (%u); the heard sample "
              "%u from the play cursor; nothing remembered with DIV-0092 off, used once and for its own track only",
              r1, r2, r4, r5, want);
}

}  // namespace

void FadePerFrame_Inject() {
    if (bof3::WantsShadow("sound")) FadePerFrame_SelfTest();
    if (bof3::WantsShadow("sound")) LoopTable_SelfTest();
    if (bof3::WantsShadow("sound")) music_resume::SelfTest();  // DIV-0092's resume, the MP3 path
    if (bof3::WantsShadow("sound")) music_seq::SelfTest();  // DIV-0087's seam on a synthetic song
    if (bof3::WantsShadow("sound")) mp3dec::SelfTest();     // DIV-0091: ours against Capcom's decoder on one track
    const std::uint8_t was[] = {0, 0, 0, 0}, is[] = {1, 0, 0, 0};
    bof3::PatchBytes("MusicFadePerFrame", Address(&sound::g_fade_per_frame), was, is, 4);
    bof3::Log("DIV-0028    music fades step once per logic frame (on unless the line above says OFF)");
}

}  // namespace

// ============================================================================
// The quicksave's audio participant, SOUND (docs/quicksave.md section 10; the
// quicksave round's group SP-A, docs/quicksave-census.md 3 to 6). Nothing
// here changes what the game does between a take and a restore: it decides
// what a restore puts back.
//
// The span holds the game's words about sound, but what they point at is the
// process's: DirectSound buffers, heap blocks, the decoder, the synth. A
// restore therefore runs in two halves around SN's span write.
//
//   Discard (before the span is written, on the live words): every effect
//   buffer of every loaded bank released (SndBuf_Release - stopped, then
//   released) and the bank's sample data freed, as Snd_LoadBank's reload
//   frees them; the voice stream released (SndStream_Stop); the music stream
//   released (Music_Release: buffer, decoder, events, the file's copy, the
//   synth stopped); the last stream's file and Music_File freed. The device
//   and the primary buffer are the process's and are kept, by value.
//
//   Restore (after it, on the take's words, which name nothing of this
//   process): the device put back; every stale pointer cleared; each bank's
//   sample data put back from the record (a copy of the heap block, its size
//   the allocator's), the voice entries rebased onto it and their buffers made
//   again from their waves as Snd_LoadBank makes them (SndBuf_FromWave, so
//   0x5A69E0's buffer); the channels pointed at the new buffers of the entries
//   they named, and the looping voices that played at the take played again
//   at their frequency and position (a one-shot cut by a restore is not a
//   defect, the round doc says); the last stream's file put back; Music_File
//   read again for its track (Music_LoadFile); the music stream started again
//   (Music_Start) and moved to the sample heard at the take (ResumeSkip,
//   DIV-0092's own mechanism), its volume set from the span's Music_Volume,
//   stopped again if it was not playing; then a voice stream playing from the
//   last stream's file at the take put back at its position - after the
//   music, whose skip decodes to the heard sample before its first half plays
//   (D406: played first, the voice had the decode's length alone); then our
//   statics: DIV-0092's position, DIV-0028's fade frame and unseen stop,
//   DIV-0132's duck, DIV-0129's missing stream.
//
// What is rebuilt rather than copied: the decoder (decoded again from the
// top), the synth (FastForward), the ring and Music_Staging (refilled), the
// measured loop's position (g_loop, by the skip), every buffer.
namespace {

constexpr std::uint32_t kSnapVersion = 1;
constexpr unsigned kSnapBanks = 6;                // Sound_Banks: 0x1518 bytes = 6 records of 0x384
constexpr unsigned kSnapEntries = 64;             // voice entries a record
constexpr std::uint32_t kBankData = 0x380;        // the record's sample-data pointer
constexpr unsigned kSnapChannels = 23;            // Sound_Channels 0x6BC8C8..0x6BC924
constexpr unsigned kGetFrequency = 0x20;          // IDirectSoundBuffer::GetFrequency
constexpr unsigned char kNoEntry = 0xFF;
enum : unsigned char { kMusicNone = 0, kMusicTrack = 1, kMusicOther = 2 };

std::size_t HeapBlockBytes(void* block) { return _msize(block); }

// The size of the file Music_LoadFile would read for `track` in this
// process (the cache song, else BGM\NNN.DAT, else BGM\NNNN.DAT), nothing
// read; -1 when there is none.
int MusicFileSize(unsigned track) {
    int size = -1;
    const bool pc_first = music_seq::PcFirst();  // DIV-0150, as Music_LoadFile
    if (!pc_first && music_seq::Probe(track, &size)) return size;
    char looping[0x28], once[0x28];
    g.sprintf(looping, reinterpret_cast<const char*>(::bof3::Img(kLoopingName.v)), track);
    g.sprintf(once, reinterpret_cast<const char*>(::bof3::Img(kOnceName.v)), track);
    int handle = g.file_open(looping, 0, 0);
    if (handle == -1) handle = g.file_open(once, 0, 0);
    if (handle == -1) return pc_first && music_seq::Probe(track, &size) ? size : -1;
    size = g.file_size(handle);
    g.file_close(handle);
    return size;
}

// What the participant calls outside the callee table g, through pointers so
// the self-test can stand in for them.
struct SnapCallees {
    void (__cdecl* stream_stop)();                       // SndStream_Stop (battle_items.cpp)
    void (__cdecl* stream_play)(const unsigned char*);   // SndStream_Play
    void (__cdecl* release)(void*);                      // SndBuf_Release
    void* (__cdecl* from_wave)(const unsigned char*);    // SndBuf_FromWave
    std::size_t (*block_bytes)(void*);                   // a Crt_malloc block's size (the toolchain's heap: crt-rest 2.6)
    int (*music_file_size)(unsigned);                    // MusicFileSize (SnapCheck)
};
const SnapCallees kSnapOriginals = {SndStream_Stop, SndStream_Play, SndBuf_Release, SndBuf_FromWave, HeapBlockBytes,
                                    MusicFileSize};
SnapCallees g_snap = kSnapOriginals;

// The live device and primary, read by Discard before the span is written.
void* g_live_device = nullptr;
void* g_live_primary = nullptr;
bool g_discarded = false;

std::uint32_t BankRecord(unsigned b) { return Address(Sound_Banks) + b * kBankBytes; }
std::uint32_t EntryAt(unsigned b, unsigned e) { return BankRecord(b) + kVoices + e * 8; }

// A Sound_Banks record (symbols.toml Sound_Banks; Snd_LoadBank): 24 cues of
// four voice words at +0, 64 voice entries {sample pointer, buffer} at
// +0x180, the sample data pointer at +0x380; each typed where its address is
// made. Addresses as std::uint32_t: the image is 32-bit.
struct SoundVoice {
    std::uint32_t sample;  // +0 the voice's sample, inside the bank's data; 0 no voice
    std::uint32_t buffer;  // +4 its IDirectSoundBuffer (SndBuf_FromWave)
};
struct SoundBank {
    std::uint32_t cues[24][4];  // +0 four voice words a cue (Sound_PlayEffect)
    SoundVoice voices[64];      // +0x180 kVoices
    std::uint32_t data;         // +0x380 kBankData: the record's sample data (Crt_malloc)
};
static_assert(sizeof(SoundBank) == kBankBytes, "a Sound_Banks record");
static_assert(offsetof(SoundBank, voices) == kVoices, "SoundBank::voices");
static_assert(offsetof(SoundBank, data) == kBankData, "SoundBank::data");
static_assert(kSnapBanks * sizeof(SoundBank) == Sound_Banks_count, "Sound_Banks' size");

SoundBank* Bank(std::uint32_t record) { return reinterpret_cast<SoundBank*>(At(record)); }
SoundVoice* Voice(std::uint32_t entry) { return reinterpret_cast<SoundVoice*>(At(entry)); }

void SnapSave(bof3::SnapshotWriter& w) {
    w.Put(kSnapVersion);
    // The banks: each loaded one's sample data, whole.
    std::uint32_t bank_bytes = 0;
    unsigned banks = 0;
    for (unsigned b = 0; b < kSnapBanks; ++b) {
        const std::uint32_t data = Dword(BankRecord(b) + kBankData);   // raw: ->data compiles otherwise (the typed-data gate)
        const std::uint32_t bytes = data ? static_cast<std::uint32_t>(g_snap.block_bytes(At(data))) : 0;
        w.Put(data);
        w.Put(bytes);
        if (bytes) w.Put(At(data), bytes);
        bank_bytes += bytes;
        banks += data != 0;
    }
    // The channels: which entry's buffer each names, and how it plays.
    unsigned looping = 0;
    for (unsigned c = 0; c < kSnapChannels; ++c) {
        const std::uint32_t buffer = Sound_Channels[c];
        unsigned char bank = kNoEntry, entry = 0, playing = 0;
        unsigned long frequency = 0, position = 0, write = 0;
        for (unsigned b = 0; buffer && b < kSnapBanks && bank == kNoEntry; ++b)
            for (unsigned e = 0; e < kSnapEntries; ++e)
                // the buffer word raw: ->buffer compiles otherwise (the typed-data gate), here and below
                if (Voice(EntryAt(b, e))->sample != 0 && Dword(EntryAt(b, e) + 4) == buffer) {
                    bank = static_cast<unsigned char>(b);
                    entry = static_cast<unsigned char>(e);
                    break;
                }
        if (buffer && bank == kNoEntry)
            bof3::Log("quicksave   SOUND: channel %u names buffer %08X, which no bank's entry holds: silent after a restore",
                      c, buffer);
        if (bank != kNoEntry) {
            void* const b = At(buffer);
            playing = Playing(b, 0) ? 1 : 0;
            Method<ComStatus>(b, kGetFrequency)(b, &frequency);
            Method<ComPosition>(b, kGetCurrentPosition)(b, &position, &write);
            looping += playing;
        }
        w.Put(bank);
        w.Put(entry);
        w.Put(playing);
        w.Put(static_cast<std::uint32_t>(frequency));
        w.Put(static_cast<std::uint32_t>(position));
    }
    // The last stream's file and the voice stream (Sound_LoadStream, kind > 0).
    const std::uint32_t stream = SaveMenu_StreamData;
    const std::uint32_t stream_bytes = stream ? static_cast<std::uint32_t>(g_snap.block_bytes(At(stream))) : 0;
    w.Put(stream_bytes);
    if (stream_bytes) w.Put(At(stream), stream_bytes);
    {
        void* const voice = SndStream_Buffer;
        unsigned long position = 0, write = 0;
        const unsigned char has = voice != nullptr;
        const unsigned char playing = voice && Playing(voice, 0) ? 1 : 0;
        if (voice) Method<ComPosition>(voice, kGetCurrentPosition)(voice, &position, &write);
        w.Put(has);
        w.Put(playing);
        w.Put(static_cast<std::uint32_t>(position));
    }
    // Music_File: its track, read again at a restore; the span's words checked.
    const unsigned char file = Music_File != nullptr;
    w.Put(file);
    w.Put(static_cast<std::int32_t>(Music_FileSize));
    w.Put(static_cast<std::int32_t>(Music_FileLoops));
    w.Put(static_cast<std::int32_t>(Music_LoadedTrack));
    // The music stream: what it plays and the sample heard.
    unsigned char kind = kMusicNone, playing = 0, trusted = 0;
    std::uint32_t heard = 0, folded = 0, data_bytes = 0;
    void* const buffer = Music_Buffer;
    if (buffer) {
        const bool track = sound::g_ring.track_stream && sound::g_ring.track == Music_LoadedTrack && Music_File;
        if (music_seq::Active() && !track)
            bof3::Fatal("quicksave SOUND: the synth plays a stream that is not Music_File's track %d", Music_LoadedTrack);
        kind = track ? kMusicTrack : kMusicOther;
        if (kind == kMusicOther) data_bytes = Music_Data ? static_cast<std::uint32_t>(g_snap.block_bytes(Music_Data)) : 0;
        playing = Playing(buffer, 0) ? 1 : 0;
        const std::uint32_t size = kind == kMusicTrack ? static_cast<std::uint32_t>(Music_FileSize) : data_bytes;
        trusted = HeardFolded(buffer, size, &heard, &folded) ? 1 : 0;
        if (!trusted) heard = folded = 0;
    }
    w.Put(kind);
    w.Put(playing);
    w.Put(static_cast<unsigned char>(Music_Finished != 0));
    w.Put(trusted);
    w.Put(static_cast<std::int32_t>(Music_Loops));
    w.Put(heard);
    w.Put(folded);
    w.Put(data_bytes);
    if (data_bytes) w.Put(Music_Data, data_bytes);
    // Our statics.
    w.Put(sound::g_resume);
    w.Put(sound::g_fade_frame);
    w.Put(sound::g_stop_unseen);
    w.Put(sound::g_unducked);
    w.Put(static_cast<unsigned char>(save_menu::StreamMissing()));
    bof3::Log("quicksave   SOUND saved: %u banks (%u bytes of samples), %u looping voices, the last stream %u bytes, "
              "music %s%s at sample %u (heard %u%s), fade %d steps left%s",
              banks, bank_bytes, looping, stream_bytes,
              kind == kMusicNone ? "none" : kind == kMusicTrack ? "track" : "stream",
              kind == kMusicNone || playing ? "" : " (stopped)", folded, heard,
              trusted || kind == kMusicNone ? "" : ", no position to trust: from the top", static_cast<int>(Music_FadeCount),
              Music_FadeStops && Music_FadeCount ? " (to a stop)" : "");
}

void SnapDiscard() {
    g_live_device = Snd_Device;
    g_live_primary = Snd_Primary;
    for (unsigned b = 0; b < kSnapBanks; ++b) {
        const std::uint32_t data = Bank(BankRecord(b))->data;
        if (!data) continue;
        for (unsigned e = 0; e < kSnapEntries; ++e) {
            const std::uint32_t buffer = Dword(EntryAt(b, e) + 4);
            if (Voice(EntryAt(b, e))->sample != 0 && buffer) g_snap.release(At(buffer));  // the ones Snd_LoadBank made
        }
        g.free(At(data));
    }
    g_snap.stream_stop();
    g.music_release();
    if (const std::uint32_t stream = SaveMenu_StreamData) g.free(At(stream));
    if (Music_File) {
        music_seq::Forget(Music_File);
        g.free(Music_File);
    }
    g_discarded = true;
}

// The SOUND record checked before anything is written: the music file it
// names must be the one this process reads for that track. Which file that is
// follows the launch (BOF3X_MUSIC, the cache's layers, the install's BGM),
// which the build check cannot see; SnapRestore's read-again would otherwise
// stop the game after the span was written (the review of PR #95). A record
// this check cannot walk is let through to SnapRestore's own Fatals.
bool SnapCheck(bof3::SnapshotReader& r, std::string& why, std::string& detail) {
    if (r.Left() < sizeof(std::uint32_t) || r.Get<std::uint32_t>() != kSnapVersion) return true;
    for (unsigned b = 0; b < kSnapBanks; ++b) {
        if (r.Left() < 8) return true;
        r.Get<std::uint32_t>();
        const std::uint32_t bytes = r.Get<std::uint32_t>();
        if (r.Left() < bytes) return true;
        r.Skip(bytes);
    }
    constexpr std::size_t kChannel = 3 + 2 * sizeof(std::uint32_t);
    if (r.Left() < kSnapChannels * kChannel + sizeof(std::uint32_t)) return true;
    r.Skip(kSnapChannels * kChannel);
    const std::uint32_t stream_bytes = r.Get<std::uint32_t>();
    constexpr std::size_t kVoice = 2 + sizeof(std::uint32_t), kFile = 1 + 3 * sizeof(std::int32_t);
    if (r.Left() < std::size_t{stream_bytes} + kVoice + kFile) return true;
    r.Skip(stream_bytes + kVoice);
    const unsigned char file = r.Get<unsigned char>();
    const std::int32_t file_size = r.Get<std::int32_t>();
    r.Get<std::int32_t>();
    const std::int32_t file_track = r.Get<std::int32_t>();
    if (!file || file_track < 0) return true;
    const int now = g_snap.music_file_size(static_cast<unsigned>(file_track));
    if (now == file_size) return true;
    why = "another music source";
    char text[160];
    std::snprintf(text, sizeof text,
                  "track %d was a %d-byte file at the take; this game reads %s (BOF3X_MUSIC, the cache's layers or "
                  "the install's BGM differ)",
                  file_track, file_size, now < 0 ? "no file for it" : (std::to_string(now) + " bytes").c_str());
    detail = text;
    return false;
}

void SnapRestore(bof3::SnapshotReader& r) {
    if (r.Get<std::uint32_t>() != kSnapVersion) bof3::Fatal("quicksave SOUND: the record is another version");
    if (!g_discarded) bof3::Fatal("quicksave SOUND: a restore without the discard before the span");
    g_discarded = false;
    Snd_Device = g_live_device;
    Snd_Primary = g_live_primary;
    // The buffer description and wave format every CreateSoundBuffer call
    // writes (Snd_BufferDesc .. Snd_WaveFormat, up to Music_Events): the
    // rebuild below writes them once a buffer, so the take's bytes - the last
    // buffer made before it - are put back at the end, as the span had them.
    const std::uint32_t scratch_at = Address(Snd_BufferDesc), scratch_end = Address(Music_Events);
    unsigned char scratch[0x40];
    if (scratch_end - scratch_at != 0x3C) bof3::Fatal("quicksave SOUND: Snd_BufferDesc..Music_Events is %u bytes", scratch_end - scratch_at);
    std::memcpy(scratch, At(scratch_at), scratch_end - scratch_at);
    // The take's words, naming nothing of this process, cleared.
    Music_Buffer = nullptr;
    Music_Notify = nullptr;
    Music_Events[0] = Music_Events[1] = nullptr;
    Music_Decoder = nullptr;
    Music_Data = nullptr;
    SndStream_Buffer = nullptr;
    SaveMenu_StreamData = 0;
    Music_File = nullptr;

    // The banks.
    unsigned banks = 0;
    for (unsigned b = 0; b < kSnapBanks; ++b) {
        const std::uint32_t old = r.Get<std::uint32_t>();
        const std::uint32_t bytes = r.Get<std::uint32_t>();
        const std::uint32_t record = BankRecord(b);
        if (Bank(record)->data != old)
            bof3::Fatal("quicksave SOUND: bank %u's data word is %08X in the span, %08X in the record", b + 1,
                        Bank(record)->data, old);
        if (!old) continue;
        if (!bytes) bof3::Fatal("quicksave SOUND: bank %u had sample data at the take and the record holds none", b + 1);
        void* const block = g.malloc(bytes);
        if (!block) bof3::Fatal("quicksave SOUND: no %u bytes for bank %u's samples", bytes, b + 1);
        r.Get(block, bytes);
        Bank(record)->data = Address(block);
        for (unsigned e = 0; e < kSnapEntries; ++e) {
            const std::uint32_t sample = Voice(EntryAt(b, e))->sample;
            if (sample == 0) continue;  // no voice: Snd_LoadBank left the entry as the payload had it
            if (sample - old >= bytes)
                bof3::Fatal("quicksave SOUND: bank %u entry %u's sample %08X lies outside its data %08X+%u", b + 1, e,
                            sample, old, bytes);
            const std::uint32_t moved = Address(block) + (sample - old);
            Voice(EntryAt(b, e))->sample = moved;
            SetDword(EntryAt(b, e) + 4, Address(g_snap.from_wave(At(moved))));
        }
        ++banks;
    }
    // The channels.
    unsigned restarted = 0;
    for (unsigned c = 0; c < kSnapChannels; ++c) {
        const unsigned char bank = r.Get<unsigned char>();
        const unsigned char entry = r.Get<unsigned char>();
        const unsigned char playing = r.Get<unsigned char>();
        const std::uint32_t frequency = r.Get<std::uint32_t>();
        const std::uint32_t position = r.Get<std::uint32_t>();
        if (bank == kNoEntry) {
            Sound_Channels[c] = 0;
            continue;
        }
        if (bank >= kSnapBanks || entry >= kSnapEntries) bof3::Fatal("quicksave SOUND: channel %u names bank %u entry %u", c, bank, entry);
        const std::uint32_t buffer = Voice(EntryAt(bank, entry))->buffer;
        Sound_Channels[c] = buffer;
        if (!buffer || !playing) continue;
        void* const b = At(buffer);
        Method<ComValue>(b, kSetFrequency)(b, frequency);
        Method<ComValue>(b, kSetCurrentPosition)(b, position);
        Method<ComPlay>(b, kPlay)(b, 0, 0, 1);  // a channel holds only a looping voice (Sound_PlayEffect)
        ++restarted;
    }
    // The last stream's file, and the voice stream playing from it.
    const char* voice_state = "none";
    const std::uint32_t stream_bytes = r.Get<std::uint32_t>();
    if (stream_bytes) {
        void* const block = g.malloc(stream_bytes);
        if (!block) bof3::Fatal("quicksave SOUND: no %u bytes for the last stream's file", stream_bytes);
        r.Get(block, stream_bytes);
        SaveMenu_StreamData = Address(block);
    }
    // The voice stream's words. It is played below, after the music stream
    // has started: Music_Start's skip decodes the song from its top to the
    // sample heard, and a voice played before it had that long alone (D406).
    const unsigned char voice_has = r.Get<unsigned char>();
    const unsigned char voice_playing = r.Get<unsigned char>();
    const std::uint32_t voice_position = r.Get<std::uint32_t>();
    // Music_File, read again.
    const unsigned char file = r.Get<unsigned char>();
    const std::int32_t file_size = r.Get<std::int32_t>();
    const std::int32_t file_loops = r.Get<std::int32_t>();
    const std::int32_t file_track = r.Get<std::int32_t>();
    if (file) {
        g.music_load_file(static_cast<unsigned>(file_track));
        if (!Music_File || Music_FileSize != file_size || Music_FileLoops != file_loops || Music_LoadedTrack != file_track)
            bof3::Fatal("quicksave SOUND: track %d read again is not the take's (file %p, %d bytes looping %d, track %d; "
                        "the take's %d bytes looping %d)", file_track, Music_File.Native(), Music_FileSize, Music_FileLoops,
                        Music_LoadedTrack, file_size, file_loops);
    }
    // The music stream.
    const unsigned char kind = r.Get<unsigned char>();
    const unsigned char playing = r.Get<unsigned char>();
    const unsigned char finished = r.Get<unsigned char>();
    const unsigned char trusted = r.Get<unsigned char>();
    const std::int32_t loops = r.Get<std::int32_t>();
    const std::uint32_t heard = r.Get<std::uint32_t>();
    const std::uint32_t folded = r.Get<std::uint32_t>();
    const std::uint32_t data_bytes = r.Get<std::uint32_t>();
    std::vector<unsigned char> data(data_bytes);
    if (data_bytes) r.Get(data.data(), data_bytes);
    sound::g_restore_skip.reached = 0;
    unsigned start_ms = 0;  // what Music_Start took: the skip's decode to the sample heard (D406's measure)
    if (kind != kMusicNone) {
        if (kind == kMusicTrack && !file) bof3::Fatal("quicksave SOUND: the take's stream is a track with no Music_File");
        sound::g_resume.pending = false;  // the record's comes back below; the rebuild's skip is its own
        sound::g_restore_skip.armed = true;
        sound::g_restore_skip.sample = folded;
        const auto start_at = std::chrono::steady_clock::now();
        if (kind == kMusicTrack) g.music_start(Music_File, static_cast<unsigned>(Music_FileSize), loops);
        else g.music_start(data.data(), data_bytes, loops);
        start_ms = static_cast<unsigned>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_at).count());
        sound::g_restore_skip.armed = false;  // Music_Start without a device does nothing
        if (Music_Buffer) {
            SetVolumeBits(Dword(Address(&Music_Volume)));
            if (finished) {  // a once-only stream past its end: zeros until the pump's next notification stops it
                std::memset(Music_Staging, 0, kHalf);
                g.sndbuf_write(Music_Buffer, Music_Staging, 0, kHalf);
                g.sndbuf_write(Music_Buffer, Music_Staging, kHalf, kHalf);
            }
            if (!playing) g.music_stop();
        }
        Music_Finished = finished;
        Music_Loops = loops;
    }
    // The voice stream playing from the last stream's file, after the music
    // stream: made again by SndStream_Play at its position, stopped if it had
    // stopped; one that played an earlier file than the last stream's is cut.
    if (voice_has && SaveMenu_StreamKind != 0 && SaveMenu_StreamData != 0) {
        voice_state = voice_playing ? "playing again at its position" : "stopped";
        g_snap.stream_play(At(SaveMenu_StreamData));
        if (void* const voice = SndStream_Buffer) {
            if (voice_playing) Method<ComValue>(voice, kSetCurrentPosition)(voice, voice_position);
            else Method<ComCall>(voice, kStop)(voice);
        }
    } else if (voice_has && voice_playing) {
        voice_state = "cut";
        bof3::Log("quicksave   SOUND: the voice stream at the take played an earlier file than the last stream's: cut");
    }
    // Our statics.
    sound::g_resume = r.Get<Resume>();
    sound::g_fade_frame = r.Get<std::uint32_t>();
    sound::g_stop_unseen = r.Get<std::uint32_t>();
    sound::g_unducked = r.Get<float>();
    save_menu::SetStreamMissing(r.Get<unsigned char>() != 0);
    std::memcpy(At(scratch_at), scratch, scratch_end - scratch_at);
    bof3::Log("quicksave   SOUND restored: %u banks rebuilt, %u looping voices restarted, voice stream %s, music %s%s "
              "at sample %u (asked %u, heard %u%s; its start took %u ms)",
              banks, restarted, voice_state, kind == kMusicNone ? "none" : kind == kMusicTrack ? "track" : "stream",
              kind == kMusicNone || playing ? "" : " (stopped)", sound::g_restore_skip.reached, folded, heard,
              trusted || kind == kMusicNone ? "" : ", untrusted", start_ms);
}

}  // namespace

// The participant's self-test is below Sound_Inject (sound_snapshot_test in
// this file): it needs the stand-ins.
namespace sound_snapshot {
void SelfTest();
}

void Sound_Inject() {
    sound::g_ring_log_hold = 1;  // the self-tests below drive Sound_Tick on stand-in buffers
    if (bof3::WantsShadow("sound")) sound::SelfTest();
    FadePerFrame_Inject();
    // The quicksave's audio participant: its self-test where SN's runs (the
    // exe host's BOF3X_SELFTEST_ONLY start, or BOF3X_SHADOW=sound hosted).
    const char* const selftest_only = std::getenv("BOF3X_SELFTEST_ONLY");
    if (bof3::IsExeHost() ? selftest_only && selftest_only[0] : bof3::WantsShadow("sound")) sound_snapshot::SelfTest();
    bof3::Snapshot_Register("SOUND", &SnapSave, &SnapRestore, &SnapDiscard, &SnapCheck);
    // The ring line's hold is lifted for play; a self-test-only run keeps it,
    // whatever the later modules' fuzzes put in Music_Buffer.
    if (!(selftest_only && selftest_only[0])) sound::g_ring_log_hold = 0;
    {
        // DIV-0168: after every self-test above, which ran Capcom's rule.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MusicPlayRefusesNone", Address(&sound::g_play_refuses_none), &was, &is, 1);
        bof3::Log("DIV-0168    Music_Play refuses track 0xFF, as the PlayStation does (on unless the line above says OFF)");
    }
    sound_st::ArmStopCuts();  // DIV-0173: after every self-test above, which ran Capcom's fade
}

void music_resume::SelfTest() { ResumeMp3_SelfTest(); }

// ============================================================================
// SOUND's self-test (docs/quicksave.md section 10.4): the participant driven
// through its Save, Discard and Restore with SN's span write played by hand
// (the take's words written back between the two), on stand-ins - DirectSound
// buffers with a vtable of ours, the heap through std::malloc, the music's
// file and start recorded. Three parts:
//   - fades: a duck in progress, a fade to a stop four steps in (DIV-0143's
//     case) and one whose first tick has not come (DIV-0028's unseen stop), a
//     fade in half way, each saved, run on, restored and run again - the two
//     runs equal tick for tick: volume, count, track, stops, volume calls;
//   - the banks: two loaded by Snd_LoadBank, a looping voice playing on two
//     channels and a stopped one on a third, a voice stream; after the take
//     one bank reloaded (its old buffers released, its data freed: the
//     combat route's crash) and another loaded; then the restore: every live
//     buffer released once and no stale one again, the banks' heads and
//     sample data equal to the take's, every voice's buffer made again from
//     its own wave, the channels on the new buffers, playing as they played;
//   - the music: a track stream at a known play cursor, rebuilt by its file
//     read again and Music_Start with the skip at the folded sample heard,
//     the volume set, DIV-0092's position back; and the same stopped.
// Everything it touches is put back.
namespace sound_snapshot {
namespace {

struct FakeBuf {
    const void* const* vtable;
    int releases, plays;
    bool playing;
    unsigned long frequency, position, play_flags;
    const unsigned char* wave;
    std::uint32_t hash;
};
FakeBuf g_fakes[256];
unsigned g_fake_count;
const void* g_fake_vtable[0x54 / 4];
FakeBuf g_fake_device;

FakeBuf* Self(void* p) { return static_cast<FakeBuf*>(p); }
long __stdcall FkUnexpected(void*) { bof3::Fatal("SOUND self-test: a DirectSound method the stand-in buffer lacks"); }
long __stdcall FkRelease(void* p) { return ++Self(p)->releases, 0; }
long __stdcall FkPosition(void* p, unsigned long* play, unsigned long* write) {
    *play = Self(p)->position;
    if (write) *write = 0;
    return 0;
}
long __stdcall FkFrequency(void* p, unsigned long* f) { return *f = Self(p)->frequency, 0; }
long __stdcall FkStatus(void* p, unsigned long* s) { return *s = Self(p)->playing ? 1 : 0, 0; }
long __stdcall FkPlay(void* p, unsigned long, unsigned long, unsigned long flags) {
    Self(p)->playing = true;
    Self(p)->play_flags = flags;
    ++Self(p)->plays;
    return 0;
}
long __stdcall FkSetPosition(void* p, unsigned long v) { return Self(p)->position = v, 0; }
long __stdcall FkSetFrequency(void* p, unsigned long v) { return Self(p)->frequency = v, 0; }
long __stdcall FkSetVolume(void*, unsigned long) { return 0; }
long __stdcall FkStop(void* p) { return Self(p)->playing = false, 0; }

std::uint32_t WaveHash(const unsigned char* wave) {
    if (!wave) return 0;
    const unsigned char* data = nullptr;
    const unsigned n = Wave_FindData(wave, &data);
    std::uint32_t h = 2166136261u ^ n;
    for (unsigned i = 0; i < n; ++i) h = (h ^ data[i]) * 16777619u;
    return h;
}

void* __cdecl FakeCreate(const unsigned char* wave) {
    if (g_fake_count >= sizeof g_fakes / sizeof g_fakes[0]) bof3::Fatal("SOUND self-test: out of stand-in buffers");
    FakeBuf& f = g_fakes[g_fake_count++];
    f = FakeBuf{};
    f.vtable = g_fake_vtable;
    f.wave = wave;
    f.hash = WaveHash(wave);
    return &f;
}

void* __cdecl TestMalloc(unsigned n) { return std::malloc(n ? n : 1); }
void __cdecl TestFree(void* p) { std::free(p); }

// The span's words the participant reads and writes: SN's span write, by hand.
struct SpanWords {
    unsigned char banks[0x1518];
    std::uint32_t channels[23];
    void *device, *primary, *voice, *buffer, *notify, *event0, *event1, *decoder, *data, *file;
    std::uint32_t stream_kind, stream_data;
    int file_size, file_loops, loaded_track, loops, finished, fade_count, fade_stops;
    unsigned char track;
    float volume, fade_step;
    unsigned frame;

    void Read() {
        std::memcpy(banks, Sound_Banks, sizeof banks);
        std::memcpy(channels, Sound_Channels, sizeof channels);
        device = Snd_Device, primary = Snd_Primary, voice = SndStream_Buffer, buffer = Music_Buffer;
        notify = Music_Notify, event0 = Music_Events[0], event1 = Music_Events[1], decoder = Music_Decoder;
        data = Music_Data, file = Music_File;
        stream_kind = SaveMenu_StreamKind, stream_data = SaveMenu_StreamData;
        file_size = Music_FileSize, file_loops = Music_FileLoops, loaded_track = Music_LoadedTrack;
        loops = Music_Loops, finished = Music_Finished, fade_count = Music_FadeCount, fade_stops = Music_FadeStops;
        track = Music_Track, volume = Music_Volume, fade_step = Music_FadeStep, frame = Frame_Counter;
    }
    void Write() const {
        std::memcpy(Sound_Banks, banks, sizeof banks);
        std::memcpy(Sound_Channels, channels, sizeof channels);
        Snd_Device = device, Snd_Primary = primary, SndStream_Buffer = voice, Music_Buffer = buffer;
        Music_Notify = notify, Music_Events[0] = event0, Music_Events[1] = event1, Music_Decoder = decoder;
        Music_Data = data, Music_File = file;
        SaveMenu_StreamKind = stream_kind, SaveMenu_StreamData = stream_data;
        Music_FileSize = file_size, Music_FileLoops = file_loops, Music_LoadedTrack = loaded_track;
        Music_Loops = loops, Music_Finished = finished, Music_FadeCount = fade_count, Music_FadeStops = fade_stops;
        Music_Track = track, Music_Volume = volume, Music_FadeStep = fade_step, Frame_Counter = frame;
    }
};

// Everything the test changes, put back at its end.
struct Everything {
    SpanWords span;
    Callees g;
    display_env::Callees de;
    SnapCallees snap;
    Ring ring;
    Resume resume;
    std::uint32_t fade_frame, stop_unseen, fade_per_frame;
    float unducked;
    bool stream_missing;
    void Read() {
        span.Read();
        g = ::sound::g, de = display_env::g, snap = g_snap, ring = sound::g_ring, resume = sound::g_resume;
        fade_frame = sound::g_fade_frame, stop_unseen = sound::g_stop_unseen, fade_per_frame = sound::g_fade_per_frame;
        unducked = sound::g_unducked, stream_missing = save_menu::StreamMissing();
    }
    void Write() const {
        span.Write();
        ::sound::g = g, display_env::g = de, g_snap = snap, sound::g_ring = ring, sound::g_resume = resume;
        sound::g_fade_frame = fade_frame, sound::g_stop_unseen = stop_unseen, sound::g_fade_per_frame = fade_per_frame;
        sound::g_unducked = unducked;
        save_menu::SetStreamMissing(stream_missing);
    }
};

std::vector<unsigned char> TestSave() {
    std::vector<unsigned char> v;
    bof3::SnapshotWriter w(v);
    SnapSave(w);
    return v;
}
// What SN does at a load: the discard on the live words, the span written,
// the participant's restore from its record.
void TestRestore(const std::vector<unsigned char>& record, const SpanWords& take) {
    SnapDiscard();
    take.Write();
    bof3::SnapshotReader r("SOUND", record.data(), record.size());
    SnapRestore(r);
    if (r.Left()) bof3::Fatal("SOUND self-test: the restore left %u bytes of its record", static_cast<unsigned>(r.Left()));
}

int g_fails;
void Check(bool ok, const char* what) {
    if (ok) return;
    ++g_fails;
    bof3::Log("shadow      SOUND: FAIL %s", what);
}

// --- fades -------------------------------------------------------------------

std::vector<std::uint32_t> g_volume_bits;
int g_stops;
void __cdecl TfPump() {}
void __cdecl TfVolume(float v) {
    std::uint32_t b;
    std::memcpy(&b, &v, 4);
    g_volume_bits.push_back(b);
}
void __cdecl TfStop() { ++g_stops; }

struct Tick {
    std::uint32_t volume;
    int count;
    unsigned char track;
    int stops;
    std::size_t volume_calls;
    std::uint32_t last_bits;
};
std::vector<Tick> RunTicks(int frames, std::vector<Tick> into = {}) {
    for (int f = 0; f < frames; ++f) {
        Frame_Counter = Frame_Counter + 1;
        for (int spin = 0; spin < 3; ++spin) Sound_Tick();
        const float volume = Music_Volume;
        std::uint32_t bits;
        std::memcpy(&bits, &volume, 4);
        into.push_back({bits, static_cast<int>(Music_FadeCount), static_cast<unsigned char>(Music_Track), g_stops,
                        g_volume_bits.size(), g_volume_bits.empty() ? 0 : g_volume_bits.back()});
    }
    return into;
}
bool SameTicks(const std::vector<Tick>& a, const std::vector<Tick>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].volume != b[i].volume || a[i].count != b[i].count || a[i].track != b[i].track || a[i].stops != b[i].stops ||
            a[i].volume_calls != b[i].volume_calls || a[i].last_bits != b[i].last_bits)
            return false;
    return true;
}

// One fade case: `start` begins it, `before` frames run, the take; `after`
// frames, then `then`, then `after` again, run twice - from the take, and
// from the restore after everything of ours was scribbled.
void FadeCase(const char* name, void (*start)(), int before, int after, void (*then)(), bool expect_stop) {
    Music_Volume = 127.0f;
    Music_Track = 7;
    Music_FadeCount = 0;
    sound::g_unducked = -1.0f;
    sound::g_fade_frame = ~0u;
    sound::g_stop_unseen = 0;
    Frame_Counter = 0x46000000u;
    g_volume_bits.clear();
    g_stops = 0;
    start();
    RunTicks(before);
    save_menu::SetStreamMissing(true);
    const std::uint32_t fade_frame = sound::g_fade_frame, unseen = sound::g_stop_unseen;
    const float unducked = sound::g_unducked;
    const std::size_t calls = g_volume_bits.size();
    const int stops = g_stops;
    const std::vector<unsigned char> record = TestSave();
    SpanWords take;
    take.Read();
    std::vector<Tick> live = RunTicks(after);
    if (then) then();
    live = RunTicks(after, live);
    // everything of ours scribbled, the stand-ins' counters back to the take's
    sound::g_fade_frame = 0x12345u;
    sound::g_stop_unseen = !unseen;
    sound::g_unducked = 99.0f;
    save_menu::SetStreamMissing(false);
    g_volume_bits.resize(calls);
    g_stops = stops;
    TestRestore(record, take);
    char what[200];
    std::snprintf(what, sizeof what, "%s: the restore puts back the fade's frame, its unseen stop, the duck, the missing stream",
                  name);
    Check(sound::g_fade_frame == fade_frame && sound::g_stop_unseen == unseen && sound::g_unducked == unducked &&
              save_menu::StreamMissing() && static_cast<int>(Music_FadeCount) == take.fade_count &&
              Music_FadeStep == take.fade_step,
          what);
    std::vector<Tick> again = RunTicks(after);
    if (then) then();
    again = RunTicks(after, again);
    std::snprintf(what, sizeof what, "%s: the frames after the restore equal the frames after the take", name);
    Check(SameTicks(live, again), what);
    std::snprintf(what, sizeof what, "%s: %s", name, expect_stop ? "the fade stopped the music once" : "no stop");
    Check(expect_stop ? live.back().stops == stops + 1 && live.back().track == 0xFF : live.back().stops == stops, what);
}

void Fades() {
    ::sound::g.music_pump = TfPump;
    ::sound::g.music_set_volume = TfVolume;
    ::sound::g.music_stop = TfStop;
    sound::g_fade_per_frame = 1;
    FadeCase("a duck in progress", [] { sound::DuckMusic(16); }, 5, 14, [] { sound::UnduckMusic(8); }, false);
    FadeCase("a fade to a stop four steps in", [] { Music_FadeOutStopEax(10); }, 4, 8, nullptr, true);
    FadeCase("a fade to a stop before its first tick", [] { Music_FadeOutStopEax(10); }, 0, 8, nullptr, true);
    FadeCase("a fade in half way", [] { Music_Volume = 0.0f; Music_FadeInEax(20); }, 10, 6, nullptr, false);
}

// --- the banks ---------------------------------------------------------------

void Put32(std::vector<unsigned char>& v, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<unsigned char>(x >> (8 * i)));
}
void PutTag(std::vector<unsigned char>& v, const char* t) { v.insert(v.end(), t, t + 4); }

// A kind-2 chunk's payload as Snd_LoadBank takes it: 0x380 bytes of head
// (cues, then the voice entries' offsets from the payload's start), then
// `voices` RIFF waves.
std::vector<unsigned char> Payload(unsigned seed, unsigned voices) {
    std::vector<unsigned char> p(0x380, 0);
    for (unsigned i = 0; i < 0x180; ++i) p[i] = static_cast<unsigned char>(i * seed + 1);
    for (unsigned v = 0; v < voices; ++v) {
        const std::uint32_t offset = static_cast<std::uint32_t>(p.size());
        std::memcpy(&p[0x180 + v * 8], &offset, 4);
        const unsigned n = 64 + v * 12 + (seed % 7) * 4;
        PutTag(p, "RIFF");
        Put32(p, 36 + n);
        PutTag(p, "WAVE");
        PutTag(p, "fmt ");
        Put32(p, 16);
        Put32(p, 0x00010001u);  // PCM, mono
        Put32(p, 22050);
        Put32(p, 44100);
        Put32(p, 0x00100002u);  // block 2, 16 bits
        PutTag(p, "data");
        Put32(p, n);
        for (unsigned k = 0; k < n; ++k) p.push_back(static_cast<unsigned char>(k * seed + v * 31));
    }
    return p;
}

struct LoadCall { unsigned track; bool file_null; };
std::vector<LoadCall> g_loads;
struct StartCall { const void* file; unsigned size; int loops; bool armed; std::uint32_t sample; };
std::vector<StartCall> g_starts;

FakeBuf* EntryBuffer(unsigned b, unsigned e) { return static_cast<FakeBuf*>(static_cast<void*>(At(Dword(EntryAt(b, e) + 4)))); }

std::size_t g_stream_plays, g_starts_before_stream;  // TsStreamPlay's count, and g_starts.size() at its last call
void __cdecl TsStreamPlay(const unsigned char* wave) {
    ++g_stream_plays;
    g_starts_before_stream = g_starts.size();
    g_snap.stream_stop();
    FakeBuf* const f = static_cast<FakeBuf*>(FakeCreate(wave));
    SndStream_Buffer = f;
    f->position = 0;
    f->playing = true;
}

void Banks() {
    Snd_Device = &g_fake_device;
    const std::vector<unsigned char> one = Payload(3, 5), three = Payload(5, 3), one_again = Payload(7, 4), two = Payload(11, 2);
    ::Snd_LoadBank(1, one.data(), static_cast<unsigned>(one.size()));
    ::Snd_LoadBank(3, three.data(), static_cast<unsigned>(three.size()));
    FakeBuf* const loop_a = EntryBuffer(0, 2);
    FakeBuf* const loop_b = EntryBuffer(0, 1);
    FakeBuf* const quiet = EntryBuffer(2, 0);
    Sound_Channels[5] = Address(loop_a);
    Sound_Channels[9] = Address(loop_b);
    Sound_Channels[7] = Address(quiet);
    loop_a->playing = true, loop_a->frequency = 22050, loop_a->position = 0x123;
    loop_b->playing = true, loop_b->frequency = 11025, loop_b->position = 0x40;
    quiet->frequency = 8000;
    // a voice stream playing from the last stream's file
    const std::vector<unsigned char> voice = Payload(13, 1);
    const std::size_t voice_bytes = voice.size() - 0x380;
    void* const stream = std::malloc(voice_bytes);
    std::memcpy(stream, voice.data() + 0x380, voice_bytes);
    SaveMenu_StreamData = Address(stream);
    SaveMenu_StreamKind = 1;
    FakeBuf* const voice_buffer = static_cast<FakeBuf*>(FakeCreate(static_cast<const unsigned char*>(stream)));
    voice_buffer->playing = true, voice_buffer->position = 0x80;
    SndStream_Buffer = voice_buffer;

    const std::vector<unsigned char> record = TestSave();
    SpanWords take;
    take.Read();
    std::vector<std::uint32_t> hashes[6];
    for (unsigned b = 0; b < 6; ++b)
        for (unsigned e = 0; e < 64; ++e) hashes[b].push_back(Voice(EntryAt(b, e))->sample ? EntryBuffer(b, e)->hash : 0);

    // after the take: bank 1 reloaded (its buffers released, its data freed), bank 2 loaded
    ::Snd_LoadBank(1, one_again.data(), static_cast<unsigned>(one_again.size()));
    ::Snd_LoadBank(2, two.data(), static_cast<unsigned>(two.size()));
    const unsigned made_live = g_fake_count;
    Check(loop_a->releases == 1 && Sound_Channels[5] == 0, "banks: the reload after the take released bank 1's buffers");

    TestRestore(record, take);
    bool once = true;
    for (unsigned i = 0; i < made_live; ++i) once = once && g_fakes[i].releases == 1;
    Check(once, "banks: every buffer of the take and of the live process released exactly once (none twice)");
    bool fresh = true, equal = true, heads = true;
    for (unsigned b = 0; b < 6; ++b) {
        heads = heads && std::memcmp(At(BankRecord(b)), take.banks + b * kBankBytes, kVoices) == 0;
        const std::uint32_t data = Bank(BankRecord(b))->data;
        std::uint32_t take_data;
        std::memcpy(&take_data, take.banks + b * kBankBytes + kBankData, 4);
        if ((data == 0) != (take_data == 0)) equal = false;
        if (!data) continue;
        const std::vector<unsigned char>& payload = b == 0 ? one : three;
        equal = equal && b != 1 && _msize(At(data)) == payload.size() - 0x380 &&
                std::memcmp(At(data), payload.data() + 0x380, payload.size() - 0x380) == 0;
        for (unsigned e = 0; e < 64; ++e) {
            std::uint32_t take_sample;
            std::memcpy(&take_sample, take.banks + b * kBankBytes + kVoices + e * 8, 4);
            const std::uint32_t sample = Voice(EntryAt(b, e))->sample;
            if (!take_sample) {
                equal = equal && sample == 0;
                continue;
            }
            equal = equal && sample - data == take_sample - take_data;
            FakeBuf* const buf = EntryBuffer(b, e);
            fresh = fresh && buf >= g_fakes + made_live && buf < g_fakes + g_fake_count && buf->releases == 0 &&
                    buf->wave == At(sample) && buf->hash == hashes[b][e];
        }
    }
    Check(heads, "banks: every record's cues as at the take");
    Check(equal, "banks: the sample data of banks 1 and 3 equal to the take's, bank 2 unloaded, the voices rebased");
    Check(fresh, "banks: every voice's buffer made anew from its own wave (0x5A69E0's way, SndBuf_FromWave)");
    FakeBuf* const a = EntryBuffer(0, 2);
    FakeBuf* const b = EntryBuffer(0, 1);
    FakeBuf* const q = EntryBuffer(2, 0);
    bool channels = Sound_Channels[5] == Address(a) && Sound_Channels[9] == Address(b) && Sound_Channels[7] == Address(q);
    for (unsigned c = 0; c < 23; ++c)
        if (c != 5 && c != 7 && c != 9) channels = channels && Sound_Channels[c] == 0;
    Check(channels, "banks: the channels name the new buffers of their entries, the others none");
    Check(a->playing && a->play_flags == 1 && a->frequency == 22050 && a->position == 0x123 && b->playing &&
              b->play_flags == 1 && b->frequency == 11025 && b->position == 0x40 && !q->playing && q->plays == 0,
          "banks: the looping voices play again at their frequency and position, the stopped one does not");
    const std::uint32_t stream_now = SaveMenu_StreamData;
    FakeBuf* const v = static_cast<FakeBuf*>(SndStream_Buffer);
    Check(stream_now && _msize(At(stream_now)) == voice_bytes &&
              std::memcmp(At(stream_now), voice.data() + 0x380, voice_bytes) == 0 && v && v != voice_buffer &&
              v->playing && v->position == 0x80 && v->hash == voice_buffer->hash && voice_buffer->releases == 1,
          "banks: the last stream's file and its voice, playing again at its position");
    Check(Snd_Device == &g_fake_device, "banks: the live device kept");
    SnapDiscard();  // let go of the test's banks, stream and file
    std::memset(Sound_Banks, 0, sizeof take.banks);
    std::memset(Sound_Channels, 0, sizeof take.channels);
    SndStream_Buffer = nullptr;
    SaveMenu_StreamData = 0;
    SaveMenu_StreamKind = 0;
    g_discarded = false;
}

// --- the music ---------------------------------------------------------------

int __cdecl TmLoad(unsigned track) {
    g_loads.push_back({track, Music_File == nullptr});
    unsigned char* const f = static_cast<unsigned char*>(std::malloc(1000));
    for (unsigned i = 0; i < 1000; ++i) f[i] = static_cast<unsigned char>(i * 7);
    Music_File = f;
    Music_FileSize = 1000;
    Music_FileLoops = 0;
    Music_LoadedTrack = static_cast<int>(track);
    return 0;
}
void __cdecl TmStart(const void* file, unsigned size, int loops) {
    g_starts.push_back({file, size, loops, sound::g_restore_skip.armed, sound::g_restore_skip.sample});
    if (sound::g_restore_skip.armed) sound::g_restore_skip.armed = false, sound::g_restore_skip.reached = sound::g_restore_skip.sample;
    Music_Buffer = FakeCreate(nullptr);
    Music_Data = std::malloc(size);
    Music_Loops = loops;
    Music_Finished = 0;
}

void Music(bool playing) {
    Snd_Device = &g_fake_device;
    ::sound::g.music_load_file = TmLoad;
    ::sound::g.music_start = TmStart;
    ::sound::g.music_set_volume = TfVolume;
    ::sound::g.music_stop = TfStop;
    g_loads.clear(), g_starts.clear(), g_volume_bits.clear(), g_stops = 0;
    FakeBuf* const old = static_cast<FakeBuf*>(FakeCreate(nullptr));
    old->playing = playing;
    old->position = kHalf + 0x800;
    Music_Buffer = old;
    sound::g_ring.track_stream = true, sound::g_ring.track = 200;
    sound::g_ring.half_base[1] = 3 * kHalfSamples, sound::g_ring.half_base[0] = 4 * kHalfSamples, sound::g_ring.written = 5 * kHalfSamples;
    Music_Track = 200;
    TmLoad(200);
    Music_Loops = 0, Music_Finished = 0, Music_Decoder = nullptr;
    Music_Data = std::malloc(1000);
    std::memcpy(Music_Data, Music_File, 1000);
    Music_Volume = 50.0f;
    g_loads.clear();
    sound::g_resume = Resume{};
    sound::g_resume.remembered = true, sound::g_resume.track = 91, sound::g_resume.sample = 12345;
    // a voice stream playing beside the music (D406: it comes back after the music's start)
    const std::vector<unsigned char> voice = Payload(17, 1);
    const std::size_t voice_bytes = voice.size() - 0x380;
    void* const stream = std::malloc(voice_bytes);
    std::memcpy(stream, voice.data() + 0x380, voice_bytes);
    SaveMenu_StreamData = Address(stream);
    SaveMenu_StreamKind = 1;
    FakeBuf* const voice_buffer = static_cast<FakeBuf*>(FakeCreate(static_cast<const unsigned char*>(stream)));
    voice_buffer->playing = true, voice_buffer->position = 0x90;
    SndStream_Buffer = voice_buffer;
    g_stream_plays = 0;
    const std::uint32_t heard = 3 * kHalfSamples + 0x200, want = heard - heard % kFrameSamples;
    const std::vector<unsigned char> record = TestSave();
    {
        // The check before the span: the same file passes, another refuses.
        std::string why, detail;
        g_snap.music_file_size = [](unsigned track) { return track == 200 ? 1000 : -1; };
        bof3::SnapshotReader same("SOUND", record.data(), record.size());
        Check(SnapCheck(same, why, detail), "SOUND's check lets the take's own music file through");
        g_snap.music_file_size = [](unsigned track) { return track == 200 ? 999 : -1; };
        bof3::SnapshotReader other("SOUND", record.data(), record.size());
        Check(!SnapCheck(other, why, detail) && why == "another music source",
              "SOUND's check refuses a take whose music file this game reads at another size");
        g_snap.music_file_size = [](unsigned) { return -1; };
        bof3::SnapshotReader none("SOUND", record.data(), record.size());
        Check(!SnapCheck(none, why, detail), "SOUND's check refuses a take whose track has no file here");
        g_snap.music_file_size = kSnapOriginals.music_file_size;
    }
    SpanWords take;
    take.Read();
    sound::g_resume = Resume{};
    Music_Volume = 3.0f;
    TestRestore(record, take);
    const bool loaded = g_loads.size() == 1 && g_loads[0].track == 200 && g_loads[0].file_null;
    const bool started = g_starts.size() == 1 && g_starts[0].file == Music_File && g_starts[0].size == 1000 &&
                         g_starts[0].loops == 0 && g_starts[0].armed && g_starts[0].sample == want;
    const float fifty = 50.0f;
    std::uint32_t bits;
    std::memcpy(&bits, &fifty, 4);
    const bool volume = !g_volume_bits.empty() && g_volume_bits.back() == bits;
    const bool resume = sound::g_resume.remembered && sound::g_resume.track == 91 && sound::g_resume.sample == 12345 && !sound::g_resume.pending;
    char what[200];
    std::snprintf(what, sizeof what, "music %s: Music_File read again for track 200, before which it named nothing",
                  playing ? "playing" : "stopped");
    Check(loaded, what);
    std::snprintf(what, sizeof what, "music %s: Music_Start of it with the skip armed at the folded sample heard (%u)",
                  playing ? "playing" : "stopped", want);
    Check(started, what);
    std::snprintf(what, sizeof what,
                  "music %s: the volume from the span, DIV-0092's position back, the old buffer released, %s",
                  playing ? "playing" : "stopped", playing ? "not stopped" : "stopped once");
    Check(volume && resume && old->releases == 1 && Music_Buffer != old && g_stops == (playing ? 0 : 1), what);
    FakeBuf* const v = static_cast<FakeBuf*>(SndStream_Buffer);
    std::snprintf(what, sizeof what,
                  "music %s: the voice stream played again at its position once, after the music's start (D406)",
                  playing ? "playing" : "stopped");
    Check(g_stream_plays == 1 && g_starts_before_stream == 1 && v && v != voice_buffer && v->playing &&
              v->position == 0x90 && v->hash == voice_buffer->hash,
          what);
    SnapDiscard();  // let go of the test's streams (Music_Release on the stand-in), voice and file
    Music_Buffer = nullptr, Music_Data = nullptr, Music_File = nullptr;
    SndStream_Buffer = nullptr;
    SaveMenu_StreamData = 0;
    SaveMenu_StreamKind = 0;
    g_discarded = false;
}

}  // namespace

void SelfTest() {
    for (auto& v : g_fake_vtable) v = reinterpret_cast<const void*>(&FkUnexpected);
    g_fake_vtable[kRelease / 4] = reinterpret_cast<const void*>(&FkRelease);
    g_fake_vtable[kGetCurrentPosition / 4] = reinterpret_cast<const void*>(&FkPosition);
    g_fake_vtable[kGetFrequency / 4] = reinterpret_cast<const void*>(&FkFrequency);
    g_fake_vtable[kGetStatus / 4] = reinterpret_cast<const void*>(&FkStatus);
    g_fake_vtable[kPlay / 4] = reinterpret_cast<const void*>(&FkPlay);
    g_fake_vtable[kSetCurrentPosition / 4] = reinterpret_cast<const void*>(&FkSetPosition);
    g_fake_vtable[kSetVolume / 4] = reinterpret_cast<const void*>(&FkSetVolume);
    g_fake_vtable[kSetFrequency / 4] = reinterpret_cast<const void*>(&FkSetFrequency);
    g_fake_vtable[kStop / 4] = reinterpret_cast<const void*>(&FkStop);
    g_fake_device = FakeBuf{};
    g_fake_device.vtable = g_fake_vtable;
    g_fake_count = 0;
    g_fails = 0;
    static Everything saved;  // 5 KB of banks: off the stack
    saved.Read();
    // a clean slate: no bank, no channel, no stream, no music
    static SpanWords clean;
    clean = saved.span;
    std::memset(clean.banks, 0, sizeof clean.banks);
    std::memset(clean.channels, 0, sizeof clean.channels);
    clean.voice = clean.buffer = clean.notify = clean.event0 = clean.event1 = clean.decoder = clean.data = clean.file = nullptr;
    clean.stream_kind = clean.stream_data = 0;
    clean.Write();
    ::sound::g.malloc = TestMalloc;
    ::sound::g.free = TestFree;
    display_env::g.malloc = TestMalloc;
    display_env::g.free = TestFree;
    ::sound::g.sndbuf_create = FakeCreate;
    g_snap.stream_play = TsStreamPlay;
    g_discarded = false;

    Fades();
    Banks();
    Music(true);
    Music(false);

    saved.Write();
    g_snap = kSnapOriginals;
    g_discarded = false;
    if (g_fails) bof3::Fatal("SOUND self-test: %d check(s) failed (the log has each)", g_fails);
    bof3::Log("shadow      SOUND self-test: four fades (a duck, a fade to a stop four steps in and before its first "
              "tick, a fade in) restored equal tick for tick; two banks rebuilt equal after one was reloaded past the "
              "take, every buffer released once, the looping voices and the voice stream playing again; a track "
              "stream rebuilt at its folded heard sample, playing and stopped; %u stand-in buffers",
              g_fake_count);
}

}  // namespace sound_snapshot

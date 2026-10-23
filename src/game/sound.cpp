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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "audio/mp3.h"
#include "bof3/symbols.gen.h"
#include "game/display_env_callees.h"
#include "game/mode_flow.h"
#include "game/music_loops.h"
#include "game/music_resume.h"
#include "game/music_seq.h"
#include "game/save_menu.h"
#include "game/sound_callees.h"
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
void SetWord(std::uint32_t address, std::uint16_t v) { std::memcpy(At(address), &v, sizeof v); }
std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// Win32 through the game's own import slots, as the originals call it.
template <class Fn>
Fn Import(std::uint32_t slot) {
    Fn fn;
    std::memcpy(&fn, At(slot), sizeof fn);
    return fn;
}
void* __stdcall ImportCreateEvent(void* security, int manual, int initial, const char* name) {
    return Import<void* (__stdcall*)(void*, int, int, const char*)>(Sound_ImportCreateEvent_addr)(security, manual, initial, name);
}
int __stdcall ImportCloseHandle(void* handle) { return Import<int (__stdcall*)(void*)>(Sound_ImportCloseHandle_addr)(handle); }
unsigned __stdcall ImportMsgWait(unsigned count, const void* handles, int all, unsigned ms, unsigned mask) {
    return Import<unsigned (__stdcall*)(unsigned, const void*, int, unsigned, unsigned)>(Sound_ImportMsgWait_addr)(count, handles, all,
                                                                                                         ms, mask);
}

}  // namespace
}  // namespace sound

// The five with an eax result, under the assembler names of the void
// declarations symbols.gen.h makes (i686 cdecl: a leading underscore).
extern "C" int __cdecl Sound_PlayEffectEax(unsigned short id) __asm__("_Sound_PlayEffect");
extern "C" int __cdecl Music_PlayEax(unsigned int track, int frames) __asm__("_Music_Play");
extern "C" int __cdecl Music_FadeOutStopEax(int frames) __asm__("_Music_FadeOutStop");
extern "C" int __cdecl Music_FadeInEax(int frames) __asm__("_Music_FadeIn");
extern "C" int __cdecl Music_FadeOutEax(int frames) __asm__("_Music_FadeOut");

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

}  // namespace sound

#define MUSIC_LOG(...)     do { if (sound::MusicLogOn()) bof3::Log("music_log   f%u " __VA_ARGS__); } while (0)

namespace sound {

const Callees kOriginals = {
    Crt_sprintf, Crt_malloc, Crt_free, File_Open, File_Size, File_Read, File_Close,
    Mp3_Create, Mp3_MemoryIo, Mp3_SetIo, Mp3_Open, Mp3_Start, Mp3_Decode, Mp3_Seek, Mp3_Destroy,
    ImportCreateEvent, ImportCloseHandle, ImportMsgWait,
    Music_LoadFile, Music_SetVolume, Music_Start, Music_FadeInEax, Music_Pump, Music_Stop, Wave_FindData, SndBuf_Write,
    SndBuf_Play, SndBuf_Stop, SndBuf_CreateFromWave, Music_Release, Music_OpenDecoder, Music_CreateBuffer,
    Music_SetNotify, Music_Decode,
};
Callees g = kOriginals;

namespace {

// Music_SetVolume with the float's bits, pushed as the originals push them
// (a dword move, never an x87 load that would quiet a signalling NaN).
void SetVolumeBits(std::uint32_t bits) {
    reinterpret_cast<void (__cdecl*)(std::uint32_t)>(reinterpret_cast<void*>(g.music_set_volume))(bits);
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

// original 0x587740: a sound effect - the PSX SE_Play's cue of up to four
// chord voices, bank:id alike, on DirectSound buffers. The cue is the four
// dwords at Sound_Banks + (bank - 1) * 0x384 + cue * 16 with bank = id bits
// 8-11 and cue its low byte. A word of 0 is no voice. Otherwise its bits 8-12
// are a channel of Sound_Channels (23 or more less 23) and its low byte the
// bank's voice entry: 0xFF stops and forgets the channel's buffer; any other
// stops it, plays the voice's buffer at the word's high half as a frequency,
// looping when bit 15 is set, and remembers the buffer in the channel only
// then (0 otherwise), so a later cue can stop it.
// As the original has it: bank 0 reads the 0x384 bytes before the table and
// a cue past 23 reads on into the voice entries, a voice past 63 into the
// next bank; the entry's buffer is read again after SndBuf_Play; the word is
// read afresh for each voice. eax is 0 on return (the original's loop count).
int Sound_PlayEffectEax(unsigned short id) {
    const std::uint32_t cue_id = id & 0xFFFFu;
    const std::uint32_t base = (((cue_id >> 8) & 0xF) - 1) * kBankBytes;
    std::uint32_t cue = Address(Sound_Banks) + base + (cue_id & 0xFF) * 16;
    for (int n = 4; n != 0; --n, cue += 4) {
        const std::uint32_t word = Dword(cue);
        if (word == 0) continue;
        std::uint32_t channel = (word >> 8) & 0x1F;
        if (channel >= 0x17) channel -= 0x17;
        const std::uint32_t voice = word & 0xFF;
        if (voice == 0xFF) {
            void* const playing = reinterpret_cast<void*>(Sound_Channels[channel]);
            if (playing) {
                g.sndbuf_stop(playing);
                Sound_Channels[channel] = 0;
            }
            continue;
        }
        const std::uint32_t loops = (word & 0xFFFF8000u) << 16;
        void* const playing = reinterpret_cast<void*>(Sound_Channels[channel]);
        if (playing) g.sndbuf_stop(playing);
        const std::uint32_t entry = Address(Sound_Banks) + base + kVoices + voice * 8;
        g.sndbuf_play(Dword(entry) | loops, reinterpret_cast<void*>(Dword(entry + 4)), word >> 16);
        Sound_Channels[channel] = loops ? Dword(entry + 4) : 0;
    }
    return 0;
}

// original 0x587A20: reads BGM track `track` whole into Music_File. Both names
// are formatted first: the plain one (the track loops) and the N one (it
// plays once); Music_FileLoops is 1 before the plain one is tried and 0 only
// when the N one opened (docs/asset-loading-path.md section 1a).
// As the original has it: when neither opens it returns -1 with
// Music_FileLoops left 1 and nothing else changed - Music_Play then starts
// the previous file, looping (known-defects D24);
// the old file is freed only once a new one has opened; File_Read is asked
// for Music_FileSize as read back after the allocation, into the pointer the
// allocation returned. The original's success eax is File_Close's; ours is 0.
//
// DIVERGENCE DIV-0087: first, when armed (BOF3X_CACHE, BOF3X_MUSIC), the
// cache's base\bgm\NNN.DAT in the MP3's place (music_seq.h); not formatted,
// opened or read otherwise.
//
// DIVERGENCE DIV-0150: under BOF3X_MUSIC=pc the MP3 names first, and the
// cache's song only for a track the install lacks.
extern "C" int __cdecl Music_LoadFile(unsigned track) {
    const bool pc_first = music_seq::PcFirst();               // DIV-0150
    if (!pc_first && music_seq::LoadFile(track)) return 0;  // DIV-0087
    char looping[0x28], once[0x28];
    g.sprintf(looping, reinterpret_cast<const char*>(kLoopingName), track);
    g.sprintf(once, reinterpret_cast<const char*>(kOnceName), track);
    Music_FileLoops = 1;
    int handle = g.file_open(looping, 0, 0);
    if (handle == -1) {
        handle = g.file_open(once, 0, 0);
        if (handle == -1) return pc_first && music_seq::LoadFile(track) ? 0 : -1;  // DIV-0150
        Music_FileLoops = 0;
    }
    music_seq::Forget(Music_File);  // DIV-0087: a cache song in it is gone
    if (Music_File) g.free(Music_File);
    const int size = g.file_size(handle);
    Music_FileSize = size;
    void* const file = g.malloc(static_cast<unsigned>(size));
    Music_File = file;
    g.file_read(handle, file, static_cast<unsigned>(Music_FileSize));
    g.file_close(handle);
    Music_LoadedTrack = static_cast<int>(track);
    return 0;
}

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

// original 0x587AE0 (PSX 0x80161B24 and 0x80161C20; JP 0x80162514 and
// 0x80162610): starts BGM track `track`, fading in over `frames`.
// Nothing if it is the track playing (Music_Track, the whole argument against
// the byte); else the byte is set, the file loaded unless Music_File already
// holds it, the volume set to 0 and the stream started from the top, and the
// fade begun.
// As the original has it: a track above 0xFF never equals the byte and is
// stored truncated; eax is the track when nothing was done, else the fade's
// (frames). Track 0xFF while a track plays is stored (nothing playing, as far
// as the game can see) and the stream restarted from the top of whatever
// Music_File still holds - Music_LoadFile finds no file 255 and leaves it
// (known-defects D346, D24).
//
// DIVERGENCE DIV-0168: with the switch, a selection whose low byte is 0xFF
// does nothing (eax the track), as both PlayStation twins do (`andi 0xFF;
// beq 0xFF` at JP 0x80162530 / 0x80162630, the US words the same). The test
// comes after DIV-0028's completion of a stopping fade, which only models what
// Capcom's own state would already be.
int Music_PlayEax(unsigned int track, int frames) {
    MUSIC_LOG("Music_Play(%u, %d) track %u loaded %d from %p", Frame_Counter, track, frames, (unsigned)Music_Track, Music_LoadedTrack, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    if (sound::g_play_refuses_none && (track & 0xFF) == 0xFF) return static_cast<int>(track);  // DIV-0168
    if (track == Music_Track) return static_cast<int>(track);
    const std::uint32_t loaded = static_cast<std::uint32_t>(Music_LoadedTrack);
    Music_Track = static_cast<unsigned char>(track);
    if (track != loaded) g.music_load_file(track);
    SetDword(Address(&Music_Volume), 0);
    SetVolumeBits(0);
    g.music_start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
    return g.music_fade_in(frames);
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

// original 0x587B40: fades the music to nothing over `frames` of Sound_Tick,
// then stops it. x87 as the original: the step is -(volume / frames) with
// frames loaded as a 64-bit integer whose high half is 0 - unsigned, where
// Music_FadeIn divides by it signed - rounded at the control word's precision,
// then stored as a float. eax is frames.
int Music_FadeOutStopEax(int frames) {
    MUSIC_LOG("Music_FadeOutStop(%d) track %u from %p", Frame_Counter, frames, (unsigned)Music_Track, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    const std::uint32_t wide[2] = {static_cast<std::uint32_t>(frames), 0};
    Music_FadeStops = 1;
    __asm__ volatile(
        "fildll %[n]\n\t"
        "fdivrs %[volume]\n\t"
        "fchs\n\t"
        "fstps %[step]\n\t"
        : [step] "=m"(Music_FadeStep)
        : [n] "m"(wide), [volume] "m"(Music_Volume)
        : "st");
    Music_FadeCount = frames;
    sound::g_stop_unseen = sound::g_fade_per_frame && frames != 0;  // DIV-0028: the first tick marks it stopped
    return frames;
}

// original 0x587BA0: fades the music up to 127 over `frames`: the step is
// (127.0 - volume) / frames with frames a signed dword (fidiv), x87 as the
// original. A fade in never stops the music. eax is frames.
int Music_FadeInEax(int frames) {
    MUSIC_LOG("Music_FadeIn(%d) track %u from %p", Frame_Counter, frames, (unsigned)Music_Track, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    __asm__ volatile(
        "flds %[full]\n\t"
        "fsubs %[volume]\n\t"
        "fidivl %[n]\n\t"
        "fstps %[step]\n\t"
        : [step] "=m"(Music_FadeStep)
        : [full] "m"(*reinterpret_cast<const float*>(static_cast<std::uintptr_t>(k127At))), [volume] "m"(Music_Volume), [n] "m"(frames)
        : "st");
    Music_FadeStops = 0;
    Music_FadeCount = frames;
    return frames;
}

// original 0x587BE0: Music_FadeOutStop without the stop - the music plays on
// at nothing. eax is frames.
int Music_FadeOutEax(int frames) {
    MUSIC_LOG("Music_FadeOut(%d) track %u from %p", Frame_Counter, frames, (unsigned)Music_Track, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    const std::uint32_t wide[2] = {static_cast<std::uint32_t>(frames), 0};
    __asm__ volatile(
        "fildll %[n]\n\t"
        "fdivrs %[volume]\n\t"
        "fchs\n\t"
        "fstps %[step]\n\t"
        : [step] "=m"(Music_FadeStep)
        : [n] "m"(wide), [volume] "m"(Music_Volume)
        : "st");
    Music_FadeStops = 0;
    Music_FadeCount = frames;
    return frames;
}

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
extern "C" void __cdecl Sound_Tick(void) {
    g.music_pump();
    if (Music_FadeCount == 0) return;
    if (sound::g_fade_per_frame) {
        if (sound::g_stop_unseen) {
            sound::g_stop_unseen = 0;
            if (Music_FadeStops) Music_Track = 0xFF;  // stopped, as far as the game can see
        }
        const std::uint32_t frame = Frame_Counter;
        if (frame == sound::g_fade_frame) return;
        sound::g_fade_frame = frame;
    }
    __asm__ volatile(
        "flds %[step]\n\t"
        "fadds %[volume]\n\t"
        "fstps %[volume]\n\t"
        : [volume] "+m"(Music_Volume)
        : [step] "m"(Music_FadeStep)
        : "st");
    SetVolumeBits(Dword(Address(&Music_Volume)));
    const int count = Music_FadeCount - 1;
    Music_FadeCount = count;
    if (count != 0) return;
    if (!Music_FadeStops) return;
    g.music_stop();
    Music_Track = 0xFF;
}

// original 0x5A69C0: a buffer for a bank's wave, when there is a DirectSound
// device (Snd_LoadBank's voice entries); 0 when there is none.
extern "C" void* __cdecl SndBuf_FromWave(const unsigned char* wave) {
    if (!Snd_Device) return nullptr;
    return g.sndbuf_create(wave);
}

// original 0x5A69E0: a static DirectSound buffer holding a RIFF WAVE's data:
// the fmt chunk's 16 bytes as the format (cbSize 0), the data chunk's size as
// the buffer's, flags 0x180E2 (STATIC, CTRLFREQUENCY, CTRLPAN, CTRLVOLUME,
// GLOBALFOCUS, GETCURRENTPOSITION2), filled from the data chunk. 0 when
// CreateSoundBuffer fails.
// As the original has it: the fmt chunk is assumed at +0x14; the format is
// written before the data chunk is looked for, the description after; the new
// buffer lands in the original's argument slot, which holds `wave` until
// CreateSoundBuffer writes it (ours likewise).
extern "C" void* __cdecl SndBuf_CreateFromWave(const unsigned char* wave) {
    const std::uint32_t format = Address(Snd_WaveFormat);
    for (unsigned i = 0; i < 16; i += 4) SetDword(format + i, Dword(Address(wave) + 0x14 + i));
    SetWord(format + 0x10, 0);
    const unsigned char* data = nullptr;
    const unsigned size = g.wave_find_data(wave, &data);
    const std::uint32_t desc = Address(Snd_BufferDesc);
    std::memset(Snd_BufferDesc, 0, 0x24);
    void* const device = Snd_Device;
    SetDword(desc + 0x00, 0x24);
    SetDword(desc + 0x04, 0x180E2);
    SetDword(desc + 0x08, size);
    SetDword(desc + 0x10, format);
    void* buffer = const_cast<unsigned char*>(wave);
    if (Method<ComCreate>(device, kCreateSoundBuffer)(device, Snd_BufferDesc, &buffer, nullptr) != 0) return nullptr;
    g.sndbuf_write(buffer, data, 0, size);
    return buffer;
}

// original 0x5A6AA0: a RIFF WAVE's data chunk: from +0x14 past the fmt chunk
// (whose size is the dword at +0x10), chunk by chunk (size + 8) to the tag
// d a t a; its body's address through `data` unless that is null, its size
// returned.
// As the original has it: no end test - a wave without a data chunk walks on
// through memory; no padding of odd chunk sizes; the offset wraps at 32 bits.
extern "C" unsigned __cdecl Wave_FindData(const unsigned char* wave, const unsigned char** data) {
    const std::uint32_t base = Address(wave);
    std::uint32_t at = Dword(base + 0x10) + 0x14;
    for (;;) {
        const unsigned char* const tag = At(base + at);
        if (tag[0] == 'd' && tag[1] == 'a' && tag[2] == 't' && tag[3] == 'a') break;
        at = at + Dword(base + at + 4) + 8;
    }
    const unsigned size = Dword(base + at + 4);
    if (data) *data = At(base + at + 8);
    return size;
}

// original 0x5A6AF0: copies `size` bytes of `src` into a buffer at `offset`:
// Lock (flags 0), once more after Restore when the buffer was lost; the first
// region gets n1 bytes and, only when n1 is not `size`, the second gets n2
// bytes from src + n1; Unlock. Returns Lock's failure or Unlock's HRESULT.
// As the original has it: when n1 is `size` the second region is not written
// whatever n2 is; when it is not, n2 bytes are copied, not size - n1. Lock's
// outputs start as what the original's argument slots held (the buffer, the
// offset, the size) - its first pointer as 0, where the original's slot is
// the caller's ecx - for a Lock that reports success without writing them.
extern "C" long __cdecl SndBuf_Write(void* buffer, const void* src, unsigned offset, unsigned size) {
    void* first = nullptr;
    unsigned long first_bytes = Address(buffer);
    void* second = reinterpret_cast<void*>(static_cast<std::uintptr_t>(offset));
    unsigned long second_bytes = size;
    long result = Method<ComLock>(buffer, kLock)(buffer, offset, size, &first, &first_bytes, &second, &second_bytes, 0);
    if (result == kBufferLost) {
        Method<ComCall>(buffer, kRestore)(buffer);
        result = Method<ComLock>(buffer, kLock)(buffer, offset, size, &first, &first_bytes, &second, &second_bytes, 0);
    }
    if (result != 0) return result;
    const unsigned char* const bytes = static_cast<const unsigned char*>(src);
    std::memcpy(first, bytes, first_bytes);
    if (first_bytes != size) std::memcpy(second, bytes + first_bytes, second_bytes);
    return Method<ComUnlock>(buffer, kUnlock)(buffer, first, first_bytes, second, second_bytes);
}

// original 0x5A6BB0: plays a voice's buffer from the start at `frequency`,
// looping when bit 31 of `sample` is set; the rest of `sample` is the voice's
// wave, from which a lost buffer is written again (then played again).
// As the original has it: SetCurrentPosition and SetFrequency results are not
// looked at, nor the second Play's; the data pointer lands in the argument
// slot that held `sample`.
extern "C" void __cdecl SndBuf_Play(unsigned sample, void* buffer, unsigned frequency) {
    const std::uint32_t wave = sample & 0x7FFFFFFFu;
    if (!buffer) return;
    const unsigned long loops = (sample & 0x80000000u) ? 1 : 0;
    Method<ComValue>(buffer, kSetCurrentPosition)(buffer, 0);
    Method<ComValue>(buffer, kSetFrequency)(buffer, frequency);
    if (Method<ComPlay>(buffer, kPlay)(buffer, 0, 0, loops) != kBufferLost) return;
    const unsigned char* data = At(sample);
    const unsigned size = g.wave_find_data(At(wave), &data);
    g.sndbuf_write(buffer, data, 0, size);
    Method<ComPlay>(buffer, kPlay)(buffer, 0, 0, loops);
}

// original 0x5A6C30: stops a buffer that is playing. GetStatus writes into
// the original's argument slot, so the status starts as the pointer.
extern "C" void __cdecl SndBuf_Stop(void* buffer) {
    if (!buffer) return;
    if (Playing(buffer, Address(buffer))) Method<ComCall>(buffer, kStop)(buffer);
}

// original 0x5A6C90: SndBuf_Stop, then Release.
extern "C" void __cdecl SndBuf_Release(void* buffer) {
    if (!buffer) return;
    if (Playing(buffer, Address(buffer))) Method<ComCall>(buffer, kStop)(buffer);
    Method<ComCall>(buffer, kRelease)(buffer);
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
namespace {

struct Loop {
    const music_loops::Row* row;  // nullptr: the original's rewind
    std::uint32_t pos;            // samples decoded from the file's first frame
    unsigned char frame[0x4000];
    std::uint32_t carry_at, carry_len;  // bytes of `frame` still to hand out
    unsigned logged[8];                 // tracks whose first loop was logged (a bitmap of 256)
};
Loop g_loop;

void LoopBegin(int track, unsigned size) {
    g_loop.row = music_loops::g_on ? music_loops::Find(track, size) : nullptr;
    g_loop.pos = 0;
    g_loop.carry_at = g_loop.carry_len = 0;
}

// One frame into g_loop.frame; its byte count. End of stream before the row's
// end cannot happen for a row the generator checked against its file, so it
// is fatal rather than papered over.
std::uint32_t LoopFrame(std::uint32_t& decoder) {
    const int result = g.mp3_decode(At(decoder), g_loop.frame);
    decoder = Address(Music_Decoder);
    const std::uint32_t bytes = Dword(Dword(decoder + 0x14) + 8);
    if (result == kEndOfStream)
        bof3::Fatal("music_loops: track %u's stream ended at sample %u, before its loop end %u", g_loop.row->track,
                    g_loop.pos, g_loop.row->end);
    if (bytes > sizeof g_loop.frame || (bytes & 3))
        bof3::Fatal("music_loops: a frame of %u bytes (track %u)", bytes, g_loop.row->track);
    return bytes;
}

void LoopDecode(unsigned char* dst, std::uint32_t size) {
    std::uint32_t decoder = Address(Music_Decoder);
    std::uint32_t filled = 0;
    while (filled < size) {
        if (g_loop.carry_len) {
            std::uint32_t n = size - filled;
            if (n > g_loop.carry_len) n = g_loop.carry_len;
            std::memcpy(dst + filled, g_loop.frame + g_loop.carry_at, n);
            filled += n;
            g_loop.carry_at += n;
            g_loop.carry_len -= n;
            continue;
        }
        const music_loops::Row& row = *g_loop.row;
        std::uint32_t bytes = LoopFrame(decoder);
        const std::uint32_t samples = bytes / 4;
        if (g_loop.pos + samples < row.end) {
            g_loop.pos += samples;
            g_loop.carry_at = 0;
            g_loop.carry_len = bytes;
            continue;
        }
        // this frame holds the end: hand out the samples before it, then jump
        const std::uint32_t keep = (row.end - g_loop.pos) * 4;
        std::uint32_t n = size - filled;
        if (n > keep) n = keep;
        std::memcpy(dst + filled, g_loop.frame, n);
        filled += n;
        // what of `keep` did not fit goes out before the jump's samples: hold it
        // at the end of the frame buffer's free part
        const std::uint32_t pending = keep - n;
        unsigned char tail[0x4000];
        std::memcpy(tail, g_loop.frame + n, pending);
        // the row's crossfade: the samples after the end, faded out over the start's
        const std::uint32_t fade_bytes = row.fade * 4;
        if (keep + fade_bytes > bytes)
            bof3::Fatal("music_loops: track %u's crossfade runs past the frame holding its end", row.track);
        std::int16_t fade_from[2 * 1152];
        if (fade_bytes > sizeof fade_from) bof3::Fatal("music_loops: track %u's crossfade of %u samples", row.track, row.fade);
        std::memcpy(fade_from, g_loop.frame + keep, fade_bytes);
        g.mp3_seek(At(decoder), 0);
        decoder = Address(Music_Decoder);
        g_loop.pos = 0;
        for (;;) {
            bytes = LoopFrame(decoder);
            if (g_loop.pos + bytes / 4 > row.start) break;
            g_loop.pos += bytes / 4;
        }
        const std::uint32_t skip = (row.start - g_loop.pos) * 4;
        g_loop.pos += bytes / 4;
        if (skip + fade_bytes > bytes)
            bof3::Fatal("music_loops: track %u's crossfade runs past the frame holding its start", row.track);
        for (std::uint32_t j = 0; j < row.fade; ++j) {
            std::int16_t s[2];
            std::memcpy(s, g_loop.frame + skip + j * 4, 4);
            for (int c = 0; c < 2; ++c) {
                const std::int32_t a = fade_from[2 * j + c], b = s[c];
                const std::int32_t w = static_cast<std::int32_t>(2 * j + 1), f2 = static_cast<std::int32_t>(2 * row.fade);
                s[c] = static_cast<std::int16_t>((a * (f2 - w) + b * w) / f2);
            }
            std::memcpy(g_loop.frame + skip + j * 4, s, 4);
        }
        // carry: the end's pending samples, then the start frame from the start
        if (pending + bytes - skip > sizeof g_loop.frame)
            bof3::Fatal("music_loops: %u bytes to carry across track %u's loop", pending + bytes - skip, row.track);
        if (pending) {
            std::memmove(g_loop.frame + pending, g_loop.frame + skip, bytes - skip);
            std::memcpy(g_loop.frame, tail, pending);
            g_loop.carry_at = 0;
            g_loop.carry_len = pending + bytes - skip;
        } else {
            g_loop.carry_at = skip;
            g_loop.carry_len = bytes - skip;
        }
        const unsigned t = row.track & 0xFF;
        if (!(g_loop.logged[t >> 5] & (1u << (t & 31)))) {
            g_loop.logged[t >> 5] |= 1u << (t & 31);
            bof3::Log("music_loops track %03u looped at its measured points: sample %u back to %u", row.track, row.end,
                      row.start);
        }
    }
}

}  // namespace

// DIV-0092's resume (music_resume.h): the ring's bookkeeping and the
// position. All of it the DLL's own state.
namespace {

struct Ring {
    std::uint32_t written = 0;        // stream samples written into the ring since Music_Start
    std::uint32_t half_base[2] = {};  // the stream sample each half starts with
    bool track_stream = false;        // the stream is the BGM file Music_Play started
    int track = -1;                   // Music_LoadedTrack then
};
Ring g_ring;

struct Resume {
    bool remembered = false;
    unsigned track = 0;
    std::uint32_t sample = 0;
    bool pending = false;
    unsigned pending_track = 0;
    std::uint32_t pending_sample = 0;
};
Resume g_resume;

// The quicksave's rebuild (SOUND, below): the stream the restore starts again
// is advanced to the sample heard at the take, whatever stream it is - a BGM
// track or Sound_LoadStream's - by ResumeSkip, as DIV-0092's resume advances
// a track. Armed only inside the restore, for the one Music_Start it makes.
struct RestoreSkip {
    bool armed = false;
    std::uint32_t sample = 0;
    std::uint32_t reached = 0;  // where the skip landed (the restore's log)
};
RestoreSkip g_restore_skip;

constexpr std::uint32_t kFrameSamples = 1152;  // every frame of the game's MP3s (bgm-comparison.md 6.1)
constexpr std::uint32_t kHalfSamples = kHalf / 4;

// Music_Start's resume, after the stream is set up and before its first half
// is decoded: the stream advanced `target` samples. The sequenced song by
// FastForward; the MP3 by decoding through Music_Decode into a scratch buffer
// and dropping it (the loop table's own mechanism: decoding from the top
// leaves the decoder as the first pass left it, bit for bit; the decoders
// seek to 0 alone). Past a once-only song's end: from the top. The stream
// sample reached.
std::uint32_t ResumeSkip(std::uint32_t target, int loops, int loop_track, unsigned size) {
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
}

}  // namespace

// original 0x5A6CC0: starts the music stream from a BGM file in memory:
// whatever was streaming released, the file copied (the decoder reads it from
// the copy), the decoder opened, the streaming buffer made with its first
// half decoded, its two notifications set, and played looping.
// As the original has it: nothing at all without a device; the copy is made
// even when the decoder then fails to open, and the buffer is made and played
// without a decoder (known-defects D25); Music_Loops and Music_Finished are
// set only after the first half was decoded, so that half ends by the
// previous track's Music_Loops.
extern "C" void __cdecl Music_Start(const void* file, unsigned size, int loops) {
    MUSIC_LOG("Music_Start(%p, %u, %d) track %u from %p", Frame_Counter, file, size, loops, (unsigned)Music_Track, __builtin_return_address(0));
    if (!Snd_Device) return;
    if (Music_Data) g.music_release();
    void* const copy = g.malloc(size);
    Music_Data = copy;
    std::memcpy(copy, file, size);
    if (music_seq::IsSong(file, size)) {  // DIV-0087: the cache's song - the synth, no decoder, no measured loop
        Music_Decoder = nullptr;
        LoopBegin(-1, size);
        music_seq::Begin();
    } else {
        Music_Decoder = g.music_open_decoder(Music_Data, size);
        // DIV-pending (music_loops.h): the measured loop of a BGM track Music_Play
        // started, looked up before the first half is decoded; Sound_LoadStream's
        // streams (not Music_File) and once-only tracks have none.
        LoopBegin(loops && file == Music_File && size == static_cast<unsigned>(Music_FileSize) ? Music_LoadedTrack : -1,
                  size);
    }
    // DIV-0092's resume: the stream's sample count from here, and a pending
    // position for this track taken before the first half is decoded.
    g_ring.track_stream = file == Music_File && size == static_cast<unsigned>(Music_FileSize);
    g_ring.track = Music_LoadedTrack;
    g_ring.written = 0;
    if (g_restore_skip.armed) {  // the quicksave's rebuild (SOUND): the sample heard at the take
        g_restore_skip.armed = false;
        const int loop_track = !music_seq::Active() && loops && g_ring.track_stream ? Music_LoadedTrack : -1;
        g_ring.written = ResumeSkip(g_restore_skip.sample, loops, loop_track, size);
        g_restore_skip.reached = g_ring.written;
    } else if (g_resume.pending) {
        g_resume.pending = false;
        if (g_ring.track_stream && Music_LoadedTrack == static_cast<int>(g_resume.pending_track)) {
            const int loop_track = !music_seq::Active() && loops && g_ring.track_stream ? Music_LoadedTrack : -1;
            g_ring.written = ResumeSkip(g_resume.pending_sample, loops, loop_track, size);
            MUSIC_LOG("resume track %u at sample %u%s", Frame_Counter, g_resume.pending_track, g_ring.written,
                      g_ring.written == g_resume.pending_sample ? "" : " (asked past its end: from the top)");
        }
    }
    void* const buffer = g.music_create_buffer();
    Music_Buffer = buffer;
    g_ring.half_base[0] = g_ring.written;  // Music_CreateBuffer wrote the first half
    g_ring.written += kHalfSamples;
    if (buffer) {
        g.music_set_notify(buffer);
        void* const playing = Music_Buffer;
        Method<ComPlay>(playing, kPlay)(playing, 0, 0, 1);
    }
    Music_Finished = 0;
    Music_Loops = loops;
}

// original 0x5A6D60: an MP3 decoder on `size` bytes at `file`, read from
// memory through the decoder's memory table and the name size@address; the
// decoder, or 0 when it cannot be made, opened or started.
// As the original has it: a decoder that is made but does not open or start
// is not destroyed.
extern "C" void* __cdecl Music_OpenDecoder(const void* file, unsigned size) {
    void* decoder = nullptr;
    if (g.mp3_create(&decoder) != 0) return nullptr;
    unsigned char io[0x2C];
    g.mp3_memory_io(io);
    g.mp3_set_io(decoder, io);
    char name[0x20];
    g.sprintf(name, reinterpret_cast<const char*>(kMemoryName), size, file);
    if (g.mp3_open(decoder, name, -1) != 0) return nullptr;
    return g.mp3_start(decoder) == 0 ? decoder : nullptr;
}

// original 0x5A6DF0: the streaming buffer's two notifications - one event as
// the play cursor passes the start, one at the half.
// As the original has it: QueryInterface's result is not looked at; the first
// event is read back from Music_Events after the second is made, the second
// used as CreateEventA returned it. Returns SetNotificationPositions' result.
extern "C" long __cdecl Music_SetNotify(void* buffer) {
    Method<ComQuery>(buffer, kQueryInterface)(buffer, At(kIidNotify), &Music_Notify);
    Music_Events[0] = g.create_event(nullptr, 0, 0, nullptr);
    void* const second = g.create_event(nullptr, 0, 0, nullptr);
    Music_Events[1] = second;
    struct Position { unsigned long offset; void* event; };
    const Position positions[2] = {{0, Music_Events[0]}, {kHalf, second}};
    void* const notify = Music_Notify;
    return Method<ComPositions>(notify, kSetNotificationPositions)(notify, 2, positions);
}

// original 0x5A6E60: the streaming buffer: 44,100 Hz 16-bit stereo PCM,
// 0x24000 bytes (two halves), flags 0x181E0 (CTRLFREQUENCY, CTRLPAN,
// CTRLVOLUME, CTRLPOSITIONNOTIFY, GLOBALFOCUS, GETCURRENTPOSITION2), its first
// half decoded and written. 0 when CreateSoundBuffer fails.
extern "C" void* __cdecl Music_CreateBuffer(void) {
    std::memset(Snd_BufferDesc, 0, 0x24);
    void* const device = Snd_Device;
    const std::uint32_t format = Address(Snd_WaveFormat);
    SetWord(format + 0x00, 1);          // WAVE_FORMAT_PCM
    SetWord(format + 0x02, 2);
    SetDword(format + 0x04, 0xAC44);    // 44,100
    SetDword(format + 0x08, 0x2B110);   // bytes a second
    SetWord(format + 0x0C, 4);
    SetWord(format + 0x0E, 0x10);
    SetWord(format + 0x10, 0);
    const std::uint32_t desc = Address(Snd_BufferDesc);
    SetDword(desc + 0x00, 0x24);
    SetDword(desc + 0x04, 0x181E0);
    SetDword(desc + 0x08, 2 * kHalf);
    SetDword(desc + 0x10, format);
    void* buffer = nullptr;
    if (Method<ComCreate>(device, kCreateSoundBuffer)(device, Snd_BufferDesc, &buffer, nullptr) != 0) return nullptr;
    g.music_decode(Music_Staging, kHalf);
    g.sndbuf_write(buffer, Music_Staging, 0, kHalf);
    return buffer;
}

// original 0x5A6F30: decodes frames into dst until `size` bytes are filled.
// At the end of the stream a looping track rewinds and goes on; one that does
// not has the rest of dst zeroed and Music_Finished set.
// As the original has it: the frame's bytes are read from the decoder after
// each call ([[decoder + 0x14] + 8]) and counted whatever the call returned,
// the end of stream included; the last frame may run past `size` (the
// callers' 0x12000 is a whole number of frames); the comparison is signed;
// a decoder that makes no progress and never ends loops for ever.
// DIV-0087: the synth's stream renders instead (music_seq.h), ending as this
// does: zeros after a once-only song's end, and Music_Finished.
extern "C" void __cdecl Music_Decode(unsigned char* dst, int size) {
    if (music_seq::Active()) {  // DIV-0087
        music_seq::Decode(dst, size);
        return;
    }
    std::uint32_t decoder = Address(Music_Decoder);
    std::uint32_t filled = 0;
    if (!decoder) return;
    if (g_loop.row) {  // a measured loop (music_loops.h); everything else as the original
        LoopDecode(dst, static_cast<std::uint32_t>(size));
        return;
    }
    for (;;) {
        const int result = g.mp3_decode(At(decoder), At(filled + Address(dst)));
        decoder = Address(Music_Decoder);
        filled += Dword(Dword(decoder + 0x14) + 8);
        if (result == kEndOfStream) {
            if (!Music_Loops) {
                std::memset(At(Address(dst) + filled), 0, static_cast<std::uint32_t>(size) - filled);
                Music_Finished = 1;
                return;
            }
            g.mp3_seek(At(decoder), 0);
            decoder = Address(Music_Decoder);
        }
        if (static_cast<std::int32_t>(filled) >= size) return;
    }
}

// original 0x5A6FB0: the music's volume, 0..127, as DirectSound hundredths of
// a decibel: volume * (1 / 127.0) * 10000.0 - 10000.0, truncated as the CRT's
// _ftol 0x5B9550 does (round toward zero set in a copy of the control word for
// one fistp to 64 bits; the low dword used). x87 as the original, with the
// image's own floats, so it rounds as the original under any control word.
// As the original has it: linear in the level, so a fade is not linear in
// loudness; a volume past 127 asks for more than 0, which DirectSound refuses.
// DIV-0087: for the synth's stream the level follows libsnd's sequence volume
// instead - amplitude (v / 127)^2 (music_seq::Level); the synth itself plays
// at 127, since it renders 0.4 to 0.8 s ahead of what is heard.
extern "C" void __cdecl Music_SetVolume(float volume) {
    void* const buffer = Music_Buffer;
    if (!buffer) return;
    if (music_seq::Active()) {  // DIV-0087
        Method<ComValue>(buffer, kSetVolume)(buffer, static_cast<unsigned long>(music_seq::Level(volume)));
        return;
    }
    std::int64_t level;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds %[volume]\n\t"
        "fmuls %[inverse]\n\t"
        "fmuls %[scale]\n\t"
        "fsubs %[scale]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[level]\n\t"
        "fldcw %[saved]\n\t"
        : [level] "=m"(level), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [volume] "m"(volume), [inverse] "m"(*reinterpret_cast<const float*>(static_cast<std::uintptr_t>(kInverse127At))),
          [scale] "m"(*reinterpret_cast<const float*>(static_cast<std::uintptr_t>(k10000At)))
        : "eax", "st");
    Method<ComValue>(buffer, kSetVolume)(buffer, static_cast<unsigned long>(static_cast<std::uint64_t>(level)));
}

// original 0x5A7050: stops the music buffer if it is playing.
// The original's status slot is its caller's ecx for a GetStatus that did not
// write it; ours is 0 (DirectSound writes it).
// DIV-0087: the synth's song stopped with it.
extern "C" void __cdecl Music_Stop(void) {
    MUSIC_LOG("Music_Stop track %u from %p", Frame_Counter, (unsigned)Music_Track, __builtin_return_address(0));
    music_seq::Stop();  // DIV-0087
    void* const buffer = Music_Buffer;
    if (!buffer) return;
    if (!Playing(buffer, 0)) return;
    void* const playing = Music_Buffer;
    Method<ComCall>(playing, kStop)(playing);
}

// original 0x5A70A0: everything the music stream holds let go: the buffer
// (stopped if playing, released), the file's copy, the notification object and
// its two events, the decoder.
// As the original has it: Music_Buffer is read again for Stop and for
// Release; the events' handles stay in Music_Events once closed; Music_Notify
// is cleared before the events are closed, the second event read after the
// first is.
extern "C" void __cdecl Music_Release(void) {
    void* const buffer = Music_Buffer;
    if (buffer) {
        if (Playing(buffer, 0)) {
            void* const playing = Music_Buffer;
            Method<ComCall>(playing, kStop)(playing);
        }
        void* const releasing = Music_Buffer;
        Method<ComCall>(releasing, kRelease)(releasing);
        Music_Buffer = nullptr;
    }
    void* const data = Music_Data;
    if (data) {
        g.free(data);
        Music_Data = nullptr;
    }
    void* const notify = Music_Notify;
    if (notify) {
        Method<ComCall>(notify, kRelease)(notify);
        void* const first = Music_Events[0];
        Music_Notify = nullptr;
        g.close_handle(first);
        g.close_handle(Music_Events[1]);
    }
    void* const decoder = Music_Decoder;
    if (decoder) {
        g.mp3_destroy(decoder);
        Music_Decoder = nullptr;
    }
    music_seq::Release();  // DIV-0087: the synth stopped, its bank kept
}

// original 0x5A7230: keeps the stream fed - on every spin of WinMain's frame
// wait, through Sound_Tick, so no logging here. Without waiting, asks which
// half the play cursor has just entered: event 0 (the start) means the second
// half is free to fill, event 1 the first. A track that has ended is stopped
// at the next notification, once the zeros after it have played.
// As the original has it: any other wait result (nothing signalled, a
// message, a failure) does nothing; the buffer is read again after decoding.
// DIV-0087: the synth's stream has no decoder and is fed all the same.
extern "C" void __cdecl Music_Pump(void) {
    if ((!Music_Decoder && !music_seq::Active()) || !Music_Buffer) return;
    const unsigned signalled = g.msg_wait(2, Music_Events, 0, 0, 0xFF);
    if (signalled >= 2) return;
    if (Music_Finished) {
        g.music_stop();
        Music_Finished = 0;
        return;
    }
    g.music_decode(Music_Staging, kHalf);
    g.sndbuf_write(Music_Buffer, Music_Staging, signalled != 0 ? 0 : kHalf, kHalf);
    g_ring.half_base[signalled != 0 ? 0 : 1] = g_ring.written;  // DIV-0092's resume: the ring's bookkeeping
    g_ring.written += kHalfSamples;
}

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
    const std::uint32_t heard = g_ring.half_base[play / kHalf] + static_cast<std::uint32_t>(play % kHalf) / 4;
    if (heard > g_ring.written) return false;  // a cursor in a half not yet written: no position to trust
    if (music_seq::Active()) {
        *folded = music_seq::Fold(heard);
    } else {
        const bool loops = Music_Loops != 0;
        const std::uint32_t pass =
            loops && !g_loop.row && Music_Data ? (mp3dec::CountFrames(Music_Data, size) + 1) * kFrameSamples : 0;
        *folded = FoldMp3(heard, g_loop.row, loops, pass);
    }
    *heard_out = heard;
    return true;
}

}  // namespace

namespace music_resume {

void Remember() {
    g_resume.remembered = false;
    if (!mode_flow::g_area_track_fallback) return;  // DIV-0092's switch: Capcom's rule, and the fuzzes
    const unsigned track = Music_Track;
    void* const buffer = Music_Buffer;
    if (track == 0xFF || !buffer || !g_ring.track_stream || g_ring.track != static_cast<int>(track)) return;
    std::uint32_t heard = 0, folded = 0;
    if (!HeardFolded(buffer, static_cast<std::uint32_t>(Music_FileSize), &heard, &folded)) return;
    g_resume.remembered = true;
    g_resume.track = track;
    g_resume.sample = folded;
    MUSIC_LOG("remember track %u at sample %u (heard %u; %u written, the ring %u samples ahead)", Frame_Counter, track,
              folded, heard, g_ring.written, g_ring.written - heard);
}

bool Arm(unsigned track) {
    const bool hit = g_resume.remembered && g_resume.track == track;
    g_resume.remembered = false;
    g_resume.pending = hit;
    g_resume.pending_track = track;
    g_resume.pending_sample = hit ? g_resume.sample : 0;
    return hit;
}

void Forget() {
    g_resume.remembered = false;
    g_resume.pending = false;
}

void Pend(unsigned track, unsigned sample) {
    g_resume.pending = true;
    g_resume.pending_track = track;
    g_resume.pending_sample = sample;
}

bool Heard(unsigned* track_out, unsigned* sample_out) {
    const unsigned track = Music_Track;
    void* const buffer = Music_Buffer;
    if (track == 0xFF || !buffer || !g_ring.track_stream || g_ring.track != static_cast<int>(track)) return false;
    if (!Playing(buffer, 0)) return false;
    std::uint32_t heard = 0, folded = 0;
    if (!HeardFolded(buffer, static_cast<std::uint32_t>(Music_FileSize), &heard, &folded)) return false;
    *track_out = track;
    *sample_out = folded;
    MUSIC_LOG("heard track %u at sample %u (heard %u; %u written, the ring %u samples ahead)", Frame_Counter, track,
              folded, heard, g_ring.written, g_ring.written - heard);
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
    g_loop.row = &row;
    g_loop.pos = g_loop.carry_at = g_loop.carry_len = 0;
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
    g_loop.row = nullptr;
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
    g_loop.pos = g_loop.carry_at = g_loop.carry_len = 0;
    std::memset(g_loop.logged, 0, sizeof g_loop.logged);  // the test's track 200 logged its loop; the game's start unlogged
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
    g_loop.row = row;
    g_loop.pos = g_loop.carry_at = g_loop.carry_len = 0;
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
    *reached = ResumeSkip(folded, loops, -1, 0);
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
    const Ring ring = g_ring;
    const Resume resume = g_resume;
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
    g_loop.row = nullptr;
    g_loop.pos = g_loop.carry_at = g_loop.carry_len = 0;
    std::memset(g_loop.logged, 0, sizeof g_loop.logged);

    // the heard position: half 1 holds samples from 3 halves on, half 0 (the
    // last written) from 4; the cursor 0x800 bytes into half 1
    const void* vtable[0x54 / 4];
    for (auto& v : vtable) v = reinterpret_cast<const void*>(&RtUnexpected);
    vtable[kGetCurrentPosition / 4] = reinterpret_cast<const void*>(&RtPosition);
    struct { const void* const* vtable; } fake = {vtable};
    Music_Buffer = &fake;
    Music_Track = 200;
    Music_Loops = 0;
    g_ring.track_stream = true;
    g_ring.track = 200;
    g_ring.half_base[1] = 3 * kHalfSamples;
    g_ring.half_base[0] = 4 * kHalfSamples;
    g_ring.written = 5 * kHalfSamples;
    g_rt_cursor = kHalf + 0x800;
    const std::uint32_t heard = 3 * kHalfSamples + 0x200, want = heard - heard % 1152;
    mode_flow::g_area_track_fallback = 0;
    music_resume::Remember();
    const bool off = !g_resume.remembered && !music_resume::Arm(200) && !g_resume.pending;
    mode_flow::g_area_track_fallback = 1;
    music_resume::Remember();
    const bool on = g_resume.remembered && g_resume.track == 200 && g_resume.sample == want;
    const bool other = !music_resume::Arm(201) && !g_resume.remembered && !g_resume.pending;
    music_resume::Remember();
    const bool own = music_resume::Arm(200) && !g_resume.remembered && g_resume.pending &&
                     g_resume.pending_sample == want && !music_resume::Arm(200);
    Music_Track = 0xFF;  // nothing playing: nothing remembered
    music_resume::Remember();
    const bool silent = !g_resume.remembered;

    g = saved_g;
    Music_Decoder = decoder;
    Music_Loops = loops_saved;
    Music_Finished = finished;
    g_ring = ring;
    g_resume = resume;
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
//   defect, the round doc says); the last stream's file and a voice stream
//   playing from it put back; Music_File read again for its track
//   (Music_LoadFile); the music stream started again (Music_Start) and moved
//   to the sample heard at the take (ResumeSkip, DIV-0092's own mechanism),
//   its volume set from the span's Music_Volume, stopped again if it was not
//   playing; then our statics: DIV-0092's position, DIV-0028's fade frame and
//   unseen stop, DIV-0132's duck, DIV-0129's missing stream.
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
    g.sprintf(looping, reinterpret_cast<const char*>(kLoopingName), track);
    g.sprintf(once, reinterpret_cast<const char*>(kOnceName), track);
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
        const bool track = g_ring.track_stream && g_ring.track == Music_LoadedTrack && Music_File;
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
    w.Put(g_resume);
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
    const std::uint32_t stream_bytes = r.Get<std::uint32_t>();
    if (stream_bytes) {
        void* const block = g.malloc(stream_bytes);
        if (!block) bof3::Fatal("quicksave SOUND: no %u bytes for the last stream's file", stream_bytes);
        r.Get(block, stream_bytes);
        SaveMenu_StreamData = Address(block);
    }
    {
        const unsigned char has = r.Get<unsigned char>();
        const unsigned char playing = r.Get<unsigned char>();
        const std::uint32_t position = r.Get<std::uint32_t>();
        if (has && SaveMenu_StreamKind != 0 && SaveMenu_StreamData != 0) {
            g_snap.stream_play(At(SaveMenu_StreamData));
            if (void* const voice = SndStream_Buffer) {
                if (playing) Method<ComValue>(voice, kSetCurrentPosition)(voice, position);
                else Method<ComCall>(voice, kStop)(voice);
            }
        } else if (has && playing) {
            bof3::Log("quicksave   SOUND: the voice stream at the take played an earlier file than the last stream's: cut");
        }
    }
    // Music_File, read again.
    const unsigned char file = r.Get<unsigned char>();
    const std::int32_t file_size = r.Get<std::int32_t>();
    const std::int32_t file_loops = r.Get<std::int32_t>();
    const std::int32_t file_track = r.Get<std::int32_t>();
    if (file) {
        g.music_load_file(static_cast<unsigned>(file_track));
        if (!Music_File || Music_FileSize != file_size || Music_FileLoops != file_loops || Music_LoadedTrack != file_track)
            bof3::Fatal("quicksave SOUND: track %d read again is not the take's (file %p, %d bytes looping %d, track %d; "
                        "the take's %d bytes looping %d)", file_track, Music_File, Music_FileSize, Music_FileLoops,
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
    g_restore_skip.reached = 0;
    if (kind != kMusicNone) {
        if (kind == kMusicTrack && !file) bof3::Fatal("quicksave SOUND: the take's stream is a track with no Music_File");
        g_resume.pending = false;  // the record's comes back below; the rebuild's skip is its own
        g_restore_skip.armed = true;
        g_restore_skip.sample = folded;
        if (kind == kMusicTrack) g.music_start(Music_File, static_cast<unsigned>(Music_FileSize), loops);
        else g.music_start(data.data(), data_bytes, loops);
        g_restore_skip.armed = false;  // Music_Start without a device does nothing
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
    // Our statics.
    g_resume = r.Get<Resume>();
    sound::g_fade_frame = r.Get<std::uint32_t>();
    sound::g_stop_unseen = r.Get<std::uint32_t>();
    sound::g_unducked = r.Get<float>();
    save_menu::SetStreamMissing(r.Get<unsigned char>() != 0);
    std::memcpy(At(scratch_at), scratch, scratch_end - scratch_at);
    bof3::Log("quicksave   SOUND restored: %u banks rebuilt, %u looping voices restarted, music %s%s at sample %u "
              "(asked %u, heard %u%s)",
              banks, restarted, kind == kMusicNone ? "none" : kind == kMusicTrack ? "track" : "stream",
              kind == kMusicNone || playing ? "" : " (stopped)", g_restore_skip.reached, folded, heard,
              trusted || kind == kMusicNone ? "" : ", untrusted");
}

}  // namespace

// The participant's self-test is below Sound_Inject (sound_snapshot_test in
// this file): it needs the stand-ins.
namespace sound_snapshot {
void SelfTest();
}

void Sound_Inject() {
    if (bof3::WantsShadow("sound")) sound::SelfTest();
    BOF3_INJECT(Sound_PlayEffect);
    BOF3_INJECT(Music_LoadFile);
    BOF3_INJECT(Music_Play);
    BOF3_INJECT(Music_FadeOutStop);
    BOF3_INJECT(Music_FadeIn);
    BOF3_INJECT(Music_FadeOut);
    BOF3_INJECT(Sound_Tick);
    BOF3_INJECT(SndBuf_FromWave);
    BOF3_INJECT(SndBuf_CreateFromWave);
    BOF3_INJECT(Wave_FindData);
    BOF3_INJECT(SndBuf_Write);
    BOF3_INJECT(SndBuf_Play);
    BOF3_INJECT(SndBuf_Stop);
    BOF3_INJECT(SndBuf_Release);
    BOF3_INJECT(Music_Start);
    BOF3_INJECT(Music_OpenDecoder);
    BOF3_INJECT(Music_SetNotify);
    BOF3_INJECT(Music_CreateBuffer);
    BOF3_INJECT(Music_Decode);
    BOF3_INJECT(Music_SetVolume);
    BOF3_INJECT(Music_Stop);
    BOF3_INJECT(Music_Release);
    BOF3_INJECT(Music_Pump);
    FadePerFrame_Inject();
    // The quicksave's audio participant: its self-test where SN's runs (the
    // exe host's BOF3X_SELFTEST_ONLY start, or BOF3X_SHADOW=sound hosted).
    const char* const selftest_only = std::getenv("BOF3X_SELFTEST_ONLY");
    if (bof3::IsExeHost() ? selftest_only && selftest_only[0] : bof3::WantsShadow("sound")) sound_snapshot::SelfTest();
    bof3::Snapshot_Register("SOUND", &SnapSave, &SnapRestore, &SnapDiscard, &SnapCheck);
    {
        // DIV-0168: after every self-test above, which ran Capcom's rule.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MusicPlayRefusesNone", Address(&sound::g_play_refuses_none), &was, &is, 1);
        bof3::Log("DIV-0168    Music_Play refuses track 0xFF, as the PlayStation does (on unless the line above says OFF)");
    }
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
        g = ::sound::g, de = display_env::g, snap = g_snap, ring = g_ring, resume = g_resume;
        fade_frame = sound::g_fade_frame, stop_unseen = sound::g_stop_unseen, fade_per_frame = sound::g_fade_per_frame;
        unducked = sound::g_unducked, stream_missing = save_menu::StreamMissing();
    }
    void Write() const {
        span.Write();
        ::sound::g = g, display_env::g = de, g_snap = snap, g_ring = ring, g_resume = resume;
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

FakeBuf* EntryBuffer(unsigned b, unsigned e) { return static_cast<FakeBuf*>(static_cast<void*>(At(Dword(EntryAt(b, e) + 4)))); }

void __cdecl TsStreamPlay(const unsigned char* wave) {
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

struct LoadCall { unsigned track; bool file_null; };
std::vector<LoadCall> g_loads;
struct StartCall { const void* file; unsigned size; int loops; bool armed; std::uint32_t sample; };
std::vector<StartCall> g_starts;
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
    g_starts.push_back({file, size, loops, g_restore_skip.armed, g_restore_skip.sample});
    if (g_restore_skip.armed) g_restore_skip.armed = false, g_restore_skip.reached = g_restore_skip.sample;
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
    g_ring.track_stream = true, g_ring.track = 200;
    g_ring.half_base[1] = 3 * kHalfSamples, g_ring.half_base[0] = 4 * kHalfSamples, g_ring.written = 5 * kHalfSamples;
    Music_Track = 200;
    TmLoad(200);
    Music_Loops = 0, Music_Finished = 0, Music_Decoder = nullptr;
    Music_Data = std::malloc(1000);
    std::memcpy(Music_Data, Music_File, 1000);
    Music_Volume = 50.0f;
    g_loads.clear();
    g_resume = Resume{};
    g_resume.remembered = true, g_resume.track = 91, g_resume.sample = 12345;
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
    g_resume = Resume{};
    Music_Volume = 3.0f;
    TestRestore(record, take);
    const bool loaded = g_loads.size() == 1 && g_loads[0].track == 200 && g_loads[0].file_null;
    const bool started = g_starts.size() == 1 && g_starts[0].file == Music_File && g_starts[0].size == 1000 &&
                         g_starts[0].loops == 0 && g_starts[0].armed && g_starts[0].sample == want;
    const float fifty = 50.0f;
    std::uint32_t bits;
    std::memcpy(&bits, &fifty, 4);
    const bool volume = !g_volume_bits.empty() && g_volume_bits.back() == bits;
    const bool resume = g_resume.remembered && g_resume.track == 91 && g_resume.sample == 12345 && !g_resume.pending;
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
    SnapDiscard();  // let go of the test's stream (Music_Release on the stand-in) and file
    Music_Buffer = nullptr, Music_Data = nullptr, Music_File = nullptr;
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

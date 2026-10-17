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

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "audio/mp3.h"
#include "bof3/symbols.gen.h"
#include "game/mode_flow.h"
#include "game/music_loops.h"
#include "game/music_resume.h"
#include "game/music_seq.h"
#include "game/sound_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

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
    return Import<void* (__stdcall*)(void*, int, int, const char*)>(kImportCreateEvent)(security, manual, initial, name);
}
int __stdcall ImportCloseHandle(void* handle) { return Import<int (__stdcall*)(void*)>(kImportCloseHandle)(handle); }
unsigned __stdcall ImportMsgWait(unsigned count, const void* handles, int all, unsigned ms, unsigned mask) {
    return Import<unsigned (__stdcall*)(unsigned, const void*, int, unsigned, unsigned)>(kImportMsgWait)(count, handles, all,
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
extern "C" int __cdecl Music_LoadFile(unsigned track) {
    if (music_seq::LoadFile(track)) return 0;  // DIV-0087
    char looping[0x28], once[0x28];
    g.sprintf(looping, reinterpret_cast<const char*>(kLoopingName), track);
    g.sprintf(once, reinterpret_cast<const char*>(kOnceName), track);
    Music_FileLoops = 1;
    int handle = g.file_open(looping, 0, 0);
    if (handle == -1) {
        handle = g.file_open(once, 0, 0);
        if (handle == -1) return -1;
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

// original 0x587AE0: starts BGM track `track`, fading in over `frames`.
// Nothing if it is the track playing (Music_Track, the whole argument against
// the byte); else the byte is set, the file loaded unless Music_File already
// holds it, the volume set to 0 and the stream started from the top, and the
// fade begun.
// As the original has it: a track above 0xFF never equals the byte and is
// stored truncated; eax is the track when nothing was done, else the fade's
// (frames).
int Music_PlayEax(unsigned int track, int frames) {
    MUSIC_LOG("Music_Play(%u, %d) track %u loaded %d from %p", Frame_Counter, track, frames, (unsigned)Music_Track, Music_LoadedTrack, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    if (track == Music_Track) return static_cast<int>(track);
    const std::uint32_t loaded = static_cast<std::uint32_t>(Music_LoadedTrack);
    Music_Track = static_cast<unsigned char>(track);
    if (track != loaded) g.music_load_file(track);
    SetDword(Address(&Music_Volume), 0);
    SetVolumeBits(0);
    g.music_start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
    return g.music_fade_in(frames);
}

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
    if (g_resume.pending) {
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

}  // namespace

namespace music_resume {

void Remember() {
    g_resume.remembered = false;
    if (!mode_flow::g_area_track_fallback) return;  // DIV-0092's switch: Capcom's rule, and the fuzzes
    const unsigned track = Music_Track;
    void* const buffer = Music_Buffer;
    if (track == 0xFF || !buffer || !g_ring.track_stream || g_ring.track != static_cast<int>(track)) return;
    unsigned long play = 0, cursor_w = 0;
    if (Method<ComPosition>(buffer, kGetCurrentPosition)(buffer, &play, &cursor_w) != 0 || play >= 2 * kHalf) return;
    const std::uint32_t heard = g_ring.half_base[play / kHalf] + static_cast<std::uint32_t>(play % kHalf) / 4;
    if (heard > g_ring.written) return;  // a cursor in a half not yet written: no position to trust
    std::uint32_t folded;
    if (music_seq::Active()) {
        folded = music_seq::Fold(heard);
    } else {
        const bool loops = Music_Loops != 0;
        const std::uint32_t pass =
            loops && !g_loop.row && Music_Data
                ? (mp3dec::CountFrames(Music_Data, static_cast<std::uint32_t>(Music_FileSize)) + 1) * kFrameSamples
                : 0;
        folded = FoldMp3(heard, g_loop.row, loops, pass);
    }
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
}

void music_resume::SelfTest() { ResumeMp3_SelfTest(); }

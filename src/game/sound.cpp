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

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
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
extern "C" int __cdecl Music_LoadFile(unsigned track) {
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

// original 0x587AE0: starts BGM track `track`, fading in over `frames`.
// Nothing if it is the track playing (Music_Track, the whole argument against
// the byte); else the byte is set, the file loaded unless Music_File already
// holds it, the volume set to 0 and the stream started from the top, and the
// fade begun.
// As the original has it: a track above 0xFF never equals the byte and is
// stored truncated; eax is the track when nothing was done, else the fade's
// (frames).
int Music_PlayEax(unsigned int track, int frames) {
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
    return frames;
}

// original 0x587BA0: fades the music up to 127 over `frames`: the step is
// (127.0 - volume) / frames with frames a signed dword (fidiv), x87 as the
// original. A fade in never stops the music. eax is frames.
int Music_FadeInEax(int frames) {
    __asm__ volatile(
        "flds %[full]\n\t"
        "fsubs %[volume]\n\t"
        "fidivl %[n]\n\t"
        "fstps %[step]\n\t"
        : [step] "=m"(Music_FadeStep)
        : [full] "m"(*reinterpret_cast<const float*>(k127At)), [volume] "m"(Music_Volume), [n] "m"(frames)
        : "st");
    Music_FadeStops = 0;
    Music_FadeCount = frames;
    return frames;
}

// original 0x587BE0: Music_FadeOutStop without the stop - the music plays on
// at nothing. eax is frames.
int Music_FadeOutEax(int frames) {
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
// the first call after the frame deadline has moved. WinMain's loop advances
// the float deadline at 0x6BC628 once per logic frame (0x4FCF0F..0x4FCF3A,
// after the wait), and reaches this at least once in every logic frame -
// once from 0x4FCEBC when the frame is late, replayed frames included - so a
// fade lasts `frames` logic frames, as on the PlayStation. The first call of
// a new fade steps at once (the remembered deadline is an older frame's).
namespace sound {
std::uint32_t g_fade_per_frame = 0;  // 1 through PatchBytes("MusicFadePerFrame")
std::uint32_t g_fade_deadline = 0;   // the deadline's bits at the last step
}  // namespace sound

extern "C" void __cdecl Sound_Tick(void) {
    g.music_pump();
    if (Music_FadeCount == 0) return;
    if (sound::g_fade_per_frame) {
        const std::uint32_t deadline = Dword(kFrameDeadline);
        if (deadline == sound::g_fade_deadline) return;
        sound::g_fade_deadline = deadline;
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
    if (!Snd_Device) return;
    if (Music_Data) g.music_release();
    void* const copy = g.malloc(size);
    Music_Data = copy;
    std::memcpy(copy, file, size);
    Music_Decoder = g.music_open_decoder(Music_Data, size);
    void* const buffer = g.music_create_buffer();
    Music_Buffer = buffer;
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
extern "C" void __cdecl Music_Decode(unsigned char* dst, int size) {
    std::uint32_t decoder = Address(Music_Decoder);
    std::uint32_t filled = 0;
    if (!decoder) return;
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
extern "C" void __cdecl Music_SetVolume(float volume) {
    void* const buffer = Music_Buffer;
    if (!buffer) return;
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
        : [volume] "m"(volume), [inverse] "m"(*reinterpret_cast<const float*>(kInverse127At)),
          [scale] "m"(*reinterpret_cast<const float*>(k10000At))
        : "eax", "st");
    Method<ComValue>(buffer, kSetVolume)(buffer, static_cast<unsigned long>(static_cast<std::uint64_t>(level)));
}

// original 0x5A7050: stops the music buffer if it is playing.
// The original's status slot is its caller's ecx for a GetStatus that did not
// write it; ours is 0 (DirectSound writes it).
extern "C" void __cdecl Music_Stop(void) {
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
}

// original 0x5A7230: keeps the stream fed - on every spin of WinMain's frame
// wait, through Sound_Tick, so no logging here. Without waiting, asks which
// half the play cursor has just entered: event 0 (the start) means the second
// half is free to fill, event 1 the first. A track that has ended is stopped
// at the next notification, once the zeros after it have played.
// As the original has it: any other wait result (nothing signalled, a
// message, a failure) does nothing; the buffer is read again after decoding.
extern "C" void __cdecl Music_Pump(void) {
    if (!Music_Decoder || !Music_Buffer) return;
    const unsigned signalled = g.msg_wait(2, Music_Events, 0, 0, 0xFF);
    if (signalled >= 2) return;
    if (Music_Finished) {
        g.music_stop();
        Music_Finished = 0;
        return;
    }
    g.music_decode(Music_Staging, kHalf);
    g.sndbuf_write(Music_Buffer, Music_Staging, signalled != 0 ? 0 : kHalf, kHalf);
}

// DIV-0028's switch and its check. Under BOF3X_SHADOW=sound (after the fuzz,
// which ran the per-call path the original has): an 8-frame stopping fade
// driven through Sound_Tick with counting stand-ins - five calls within one
// frame take one step, then each of seven new deadlines takes one more, and
// the eighth step stops the music. Everything it touches is put back.
namespace {

int g_fpf_volume_calls, g_fpf_stop_calls;
void __cdecl FpfPump() {}
void __cdecl FpfSetVolume(float) { ++g_fpf_volume_calls; }
void __cdecl FpfStop() { ++g_fpf_stop_calls; }

void FadePerFrame_SelfTest() {
    const Callees saved_g = g;
    const auto count = Music_FadeCount;
    const auto stops = Music_FadeStops;
    const auto volume = Music_Volume;
    const auto step = Music_FadeStep;
    const auto track = Music_Track;
    const std::uint32_t deadline = Dword(kFrameDeadline);
    const std::uint32_t remembered = sound::g_fade_deadline;

    g.music_pump = FpfPump;
    g.music_set_volume = FpfSetVolume;
    g.music_stop = FpfStop;
    g_fpf_volume_calls = g_fpf_stop_calls = 0;
    sound::g_fade_per_frame = 1;
    Music_Volume = 127.0f;
    Music_FadeOutStopEax(8);
    bool ok = true;
    std::uint32_t frame = 0x45000000;  // 2048.0f, any bits unlike the remembered ones
    sound::g_fade_deadline = 0;
    for (int f = 0; f < 8; ++f) {
        SetDword(kFrameDeadline, frame + static_cast<std::uint32_t>(f) * 0x100);
        for (int spin = 0; spin < 5; ++spin) Sound_Tick();
        ok = ok && g_fpf_volume_calls == f + 1 && static_cast<int>(Music_FadeCount) == 7 - f;
    }
    ok = ok && g_fpf_stop_calls == 1 && Music_Track == 0xFF;
    for (int spin = 0; spin < 5; ++spin) Sound_Tick();  // a finished fade takes no more steps
    ok = ok && g_fpf_volume_calls == 8 && g_fpf_stop_calls == 1;

    sound::g_fade_per_frame = 0;
    g = saved_g;
    Music_FadeCount = count;
    Music_FadeStops = stops;
    Music_Volume = volume;
    Music_FadeStep = step;
    Music_Track = track;
    SetDword(kFrameDeadline, deadline);
    sound::g_fade_deadline = remembered;
    if (!ok)
        bof3::Fatal("DIV-0028 self-test: %d volume steps, %d stops for an 8-frame fade over 8 frames of 5 spins",
                    g_fpf_volume_calls, g_fpf_stop_calls);
    bof3::Log("shadow      DIV-0028 self-test: an 8-frame fade took 8 steps over 8 frames of 5 spins, stopped once");
}

void FadePerFrame_Inject() {
    if (bof3::WantsShadow("sound")) FadePerFrame_SelfTest();
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

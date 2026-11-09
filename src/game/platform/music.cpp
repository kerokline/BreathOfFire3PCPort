// Moved here by tools/move_funcs.py from battle_items.cpp, psx_rest.cpp, save_menu.cpp, sound.cpp, sound_rest.cpp (docs/source-layout-round.md).
#include "game/platform/music.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <emmintrin.h>
#include <initializer_list>
#include <io.h>
#include <malloc.h>
#include <vector>

#include "audio/mp3.h"
#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle_items.h"
#include "game/battle_items_callees.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/display_env_callees.h"
#include "game/mode_flow.h"
#include "game/move_script_bytes.h"
#include "game/music_loops.h"
#include "game/music_resume.h"
#include "game/music_seq.h"
#include "game/psx_rest.h"
#include "game/psx_rest_callees.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/save_menu.h"
#include "game/save_menu_callees.h"
#include "game/sound.h"
#include "game/sound_callees.h"
#include "game/sound_in.h"
#include "game/sound_rest.h"
#include "game/sound_rest_callees.h"
#include "game/sound_sp.h"
#include "game/sound_st.h"
#include "game/title_menu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/run_speed.h"
#include "hook/snapshot.h"
#include "input/sigma.h"

using namespace battle_items;

using namespace psx_rest;

using namespace save_menu;

using namespace sound;

using namespace sound_rest;

namespace {

// A method of the IDirectSoundBuffer the SND stream holds, by the 32-bit
// vtable offset: entry offset / 4, as sound_callees.h's Method<> reads it,
// so the index is right where an entry is 8 bytes (an object of ours on
// x86-64; docs/x64-build-design.md 6.1).
template <typename F> F MethodBattleItems(void* object, unsigned offset) {
    void* const* vtable;
    std::memcpy(&vtable, object, sizeof vtable);
    return reinterpret_cast<F>(vtable[offset / 4]);
}

::bof3::Slot32<void>& StreamBuffer() { return SndStream_Buffer; }   // the 4-byte slot, read and written as one

constexpr long kBufferLostBattleItems = static_cast<long>(0x88780096u);   // DSERR_BUFFERLOST

}  // namespace

namespace psx_rest {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

void* Ptr(U address) { return At(Long(address)); }

// One COM pointer released through its vtable's +8 if it is not null. The
// caller says whether the slot is zeroed after (inside the test, as the
// originals do it).
void ReleaseIf(U slot, bool zero) {
    void* const object = Ptr(slot);
    if (!object) return;
    Method<ComCall>(object, kRelease)(object);
    if (zero) PutLong(slot, 0);
}

}  // namespace

}  // namespace psx_rest

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

std::uint32_t Address(const volatile void* p) { return Low32(p); }

}  // namespace

}  // namespace sound

#define MUSIC_LOG(...)     do { if (sound::MusicLogOn()) bof3::Log("music_log   f%u " __VA_ARGS__); } while (0)

namespace sound {

namespace {

// GetStatus's bit 0, DSBSTATUS_PLAYING, into `status` - which holds on entry
// what the original's slot held, for a GetStatus that did not write it.
bool Playing(void* buffer, unsigned long status) {
    Method<ComStatus>(buffer, kGetStatus)(buffer, &status);
    return (status & 1) != 0;
}

}  // namespace

}  // namespace sound

namespace {

void LoopBegin(int track, unsigned size) {
    sound::g_loop.row = music_loops::g_on ? music_loops::Find(track, size) : nullptr;
    sound::g_loop.pos = 0;
    sound::g_loop.carry_at = sound::g_loop.carry_len = 0;
}

// One frame into g_loop.frame; its byte count. End of stream before the row's
// end cannot happen for a row the generator checked against its file, so it
// is fatal rather than papered over.
std::uint32_t LoopFrame(std::uint32_t& decoder) {
    const int result = sound::g.mp3_decode(sound::At(decoder), sound::g_loop.frame);
    decoder = sound::Address(Music_Decoder);
    const std::uint32_t bytes = sound::Dword(sound::Dword(decoder + 0x14) + 8);
    if (result == kEndOfStream)
        bof3::Fatal("music_loops: track %u's stream ended at sample %u, before its loop end %u", sound::g_loop.row->track,
                    sound::g_loop.pos, sound::g_loop.row->end);
    if (bytes > sizeof sound::g_loop.frame || (bytes & 3))
        bof3::Fatal("music_loops: a frame of %u bytes (track %u)", bytes, sound::g_loop.row->track);
    return bytes;
}

void LoopDecode(unsigned char* dst, std::uint32_t size) {
    std::uint32_t decoder = sound::Address(Music_Decoder);
    std::uint32_t filled = 0;
    while (filled < size) {
        if (sound::g_loop.carry_len) {
            std::uint32_t n = size - filled;
            if (n > sound::g_loop.carry_len) n = sound::g_loop.carry_len;
            std::memcpy(dst + filled, sound::g_loop.frame + sound::g_loop.carry_at, n);
            filled += n;
            sound::g_loop.carry_at += n;
            sound::g_loop.carry_len -= n;
            continue;
        }
        const music_loops::Row& row = *sound::g_loop.row;
        std::uint32_t bytes = LoopFrame(decoder);
        const std::uint32_t samples = bytes / 4;
        if (sound::g_loop.pos + samples < row.end) {
            sound::g_loop.pos += samples;
            sound::g_loop.carry_at = 0;
            sound::g_loop.carry_len = bytes;
            continue;
        }
        // this frame holds the end: hand out the samples before it, then jump
        const std::uint32_t keep = (row.end - sound::g_loop.pos) * 4;
        std::uint32_t n = size - filled;
        if (n > keep) n = keep;
        std::memcpy(dst + filled, sound::g_loop.frame, n);
        filled += n;
        // what of `keep` did not fit goes out before the jump's samples: hold it
        // at the end of the frame buffer's free part
        const std::uint32_t pending = keep - n;
        unsigned char tail[0x4000];
        std::memcpy(tail, sound::g_loop.frame + n, pending);
        // the row's crossfade: the samples after the end, faded out over the start's
        const std::uint32_t fade_bytes = row.fade * 4;
        if (keep + fade_bytes > bytes)
            bof3::Fatal("music_loops: track %u's crossfade runs past the frame holding its end", row.track);
        std::int16_t fade_from[2 * 1152];
        if (fade_bytes > sizeof fade_from) bof3::Fatal("music_loops: track %u's crossfade of %u samples", row.track, row.fade);
        std::memcpy(fade_from, sound::g_loop.frame + keep, fade_bytes);
        sound::g.mp3_seek(sound::At(decoder), 0);
        decoder = sound::Address(Music_Decoder);
        sound::g_loop.pos = 0;
        for (;;) {
            bytes = LoopFrame(decoder);
            if (sound::g_loop.pos + bytes / 4 > row.start) break;
            sound::g_loop.pos += bytes / 4;
        }
        const std::uint32_t skip = (row.start - sound::g_loop.pos) * 4;
        sound::g_loop.pos += bytes / 4;
        if (skip + fade_bytes > bytes)
            bof3::Fatal("music_loops: track %u's crossfade runs past the frame holding its start", row.track);
        for (std::uint32_t j = 0; j < row.fade; ++j) {
            std::int16_t s[2];
            std::memcpy(s, sound::g_loop.frame + skip + j * 4, 4);
            for (int c = 0; c < 2; ++c) {
                const std::int32_t a = fade_from[2 * j + c], b = s[c];
                const std::int32_t w = static_cast<std::int32_t>(2 * j + 1), f2 = static_cast<std::int32_t>(2 * row.fade);
                s[c] = static_cast<std::int16_t>((a * (f2 - w) + b * w) / f2);
            }
            std::memcpy(sound::g_loop.frame + skip + j * 4, s, 4);
        }
        // carry: the end's pending samples, then the start frame from the start
        if (pending + bytes - skip > sizeof sound::g_loop.frame)
            bof3::Fatal("music_loops: %u bytes to carry across track %u's loop", pending + bytes - skip, row.track);
        if (pending) {
            std::memmove(sound::g_loop.frame + pending, sound::g_loop.frame + skip, bytes - skip);
            std::memcpy(sound::g_loop.frame, tail, pending);
            sound::g_loop.carry_at = 0;
            sound::g_loop.carry_len = pending + bytes - skip;
        } else {
            sound::g_loop.carry_at = skip;
            sound::g_loop.carry_len = bytes - skip;
        }
        const unsigned t = row.track & 0xFF;
        if (!(sound::g_loop.logged[t >> 5] & (1u << (t & 31)))) {
            sound::g_loop.logged[t >> 5] |= 1u << (t & 31);
            bof3::Log("music_loops track %03u looped at its measured points: sample %u back to %u", row.track, row.end,
                      row.start);
        }
    }
}

constexpr std::uint32_t kHalfSamples = kHalf / 4;

}  // namespace

using sound::ComPlay;

using sound::ComValue;

// original 0x5A6980. SndStream_Stop, Music_Release, then the primary sound
// buffer and the DirectSound object - both the real DirectSound's, made by
// the sound set-up 0x5A6830 - each released and zeroed when not null.
extern "C" void __cdecl Sound_Shutdown() {
    psx_rest::g.stream_stop();
    psx_rest::g.music_release();
    ReleaseIf(::bof3::Img(Snd_Primary_addr), true);
    ReleaseIf(::bof3::Img(Snd_Device_addr), true);
}

// original 0x5A69C0: a buffer for a bank's wave, when there is a DirectSound
// device (Snd_LoadBank's voice entries); 0 when there is none.
extern "C" void* __cdecl SndBuf_FromWave(const unsigned char* wave) {
    if (!Snd_Device) return nullptr;
    return sound::g.sndbuf_create(wave);
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
    const std::uint32_t format = sound::Address(Snd_WaveFormat);
    for (unsigned i = 0; i < 16; i += 4) sound::SetDword(format + i, sound::Dword(sound::Address(wave) + 0x14 + i));
    sound::SetWord(format + 0x10, 0);
    const unsigned char* data = nullptr;
    const unsigned size = sound::g.wave_find_data(wave, &data);
    const std::uint32_t desc = sound::Address(Snd_BufferDesc);
    std::memset(Snd_BufferDesc, 0, 0x24);
    void* const device = Snd_Device;
    sound::SetDword(desc + 0x00, 0x24);
    sound::SetDword(desc + 0x04, 0x180E2);
    sound::SetDword(desc + 0x08, size);
    sound::SetDword(desc + 0x10, format);
    void* buffer = const_cast<unsigned char*>(wave);
    if (sound::Method<ComCreate>(device, kCreateSoundBuffer)(device, Snd_BufferDesc, &buffer, nullptr) != 0) return nullptr;
    sound::g.sndbuf_write(buffer, data, 0, size);
    return buffer;
}

// original 0x5A6AA0: a RIFF WAVE's data chunk: from +0x14 past the fmt chunk
// (whose size is the dword at +0x10), chunk by chunk (size + 8) to the tag
// d a t a; its body's address through `data` unless that is null, its size
// returned.
// As the original has it: no end test - a wave without a data chunk walks on
// through memory; no padding of odd chunk sizes; the offset wraps at 32 bits.
extern "C" unsigned __cdecl Wave_FindData(const unsigned char* wave, const unsigned char** data) {
    const std::uint32_t base = sound::Address(wave);
    std::uint32_t at = sound::Dword(base + 0x10) + 0x14;
    for (;;) {
        const unsigned char* const tag = sound::At(base + at);
        if (tag[0] == 'd' && tag[1] == 'a' && tag[2] == 't' && tag[3] == 'a') break;
        at = at + sound::Dword(base + at + 4) + 8;
    }
    const unsigned size = sound::Dword(base + at + 4);
    if (data) *data = sound::At(base + at + 8);
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
    unsigned long first_bytes = sound::Address(buffer);
    void* second = reinterpret_cast<void*>(static_cast<std::uintptr_t>(offset));
    unsigned long second_bytes = size;
    long result = sound::Method<ComLock>(buffer, kLock)(buffer, offset, size, &first, &first_bytes, &second, &second_bytes, 0);
    if (result == kBufferLost) {
        sound::Method<sound::ComCall>(buffer, kRestore)(buffer);
        result = sound::Method<ComLock>(buffer, kLock)(buffer, offset, size, &first, &first_bytes, &second, &second_bytes, 0);
    }
    if (result != 0) return result;
    const unsigned char* const bytes = static_cast<const unsigned char*>(src);
    std::memcpy(first, bytes, first_bytes);
    if (first_bytes != size) std::memcpy(second, bytes + first_bytes, second_bytes);
    return sound::Method<ComUnlock>(buffer, kUnlock)(buffer, first, first_bytes, second, second_bytes);
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
    sound::Method<ComValue>(buffer, kSetCurrentPosition)(buffer, 0);
    sound::Method<ComValue>(buffer, kSetFrequency)(buffer, frequency);
    if (sound::Method<ComPlay>(buffer, kPlay)(buffer, 0, 0, loops) != kBufferLost) return;
    const unsigned char* data = sound::At(sample);
    const unsigned size = sound::g.wave_find_data(sound::At(wave), &data);
    sound::g.sndbuf_write(buffer, data, 0, size);
    sound::Method<ComPlay>(buffer, kPlay)(buffer, 0, 0, loops);
}

// original 0x5A6C30: stops a buffer that is playing. GetStatus writes into
// the original's argument slot, so the status starts as the pointer.
extern "C" void __cdecl SndBuf_Stop(void* buffer) {
    if (!buffer) return;
    if (Playing(buffer, sound::Address(buffer))) sound::Method<sound::ComCall>(buffer, kStop)(buffer);
}

// original 0x5A6C60: a voice's volume - level 0..127 as DirectSound
// hundredths of a decibel, fild(level) * (1 / 127.0) * 10000.0 - 10000.0,
// truncated as the CRT's _ftol 0x5B9550 does (round toward zero in a copy of
// the control word for one fistp to 64 bits; the low dword). The x87 as the
// original, on the image's own floats, so it rounds as the original under any
// control word. Nothing for a null buffer.
// As the original has it: the level is not clamped - above 127 asks for more
// than 0, below 0 for less than -10000, which DirectSound refuses
// (DSERR_INVALIDPARAM) and the original does not look at.
extern "C" void __cdecl SndBuf_SetVolume(void* buffer, int level) {
    if (buffer == nullptr) return;
    std::int64_t value;
    unsigned short saved, truncating;
    __asm__ volatile(
        "fildl %[level]\n\t"
        "fmuls %[inverse]\n\t"
        "fmuls %[scale]\n\t"
        "fsubs %[scale]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[value]\n\t"
        "fldcw %[saved]\n\t"
        : [value] "=m"(value), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [level] "m"(level), [inverse] "m"(*reinterpret_cast<const float*>(static_cast<std::uintptr_t>(sound::kInverse127At))),
          [scale] "m"(*reinterpret_cast<const float*>(static_cast<std::uintptr_t>(sound::k10000At)))
        : "eax", "st");
    sound::Method<ComValue>(buffer, sound::kSetVolume)(buffer, static_cast<unsigned long>(static_cast<std::uint64_t>(value)));
}

// original 0x5A6C90: SndBuf_Stop, then Release.
extern "C" void __cdecl SndBuf_Release(void* buffer) {
    if (!buffer) return;
    if (Playing(buffer, sound::Address(buffer))) sound::Method<sound::ComCall>(buffer, kStop)(buffer);
    sound::Method<sound::ComCall>(buffer, sound::kRelease)(buffer);
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
    MUSIC_LOG("Music_Start(%p, %u, %d) track %u from %p", Frame_Counter, file, size, loops, (unsigned)Music_Track, __builtin_return_address(0));
    if (!Snd_Device) return;
    if (Music_Data) sound::g.music_release();
    void* const copy = sound::g.malloc(size);
    Music_Data = copy;
    std::memcpy(copy, file, size);
    if (music_seq::IsSong(file, size)) {  // DIV-0087: the cache's song - the synth, no decoder, no measured loop
        Music_Decoder = nullptr;
        LoopBegin(-1, size);
        music_seq::Begin();
    } else {
        Music_Decoder = sound::g.music_open_decoder(Music_Data, size);
        // DIV-pending (music_loops.h): the measured loop of a BGM track Music_Play
        // started, looked up before the first half is decoded; Sound_LoadStream's
        // streams (not Music_File) and once-only tracks have none.
        LoopBegin(loops && file == Music_File && size == static_cast<unsigned>(Music_FileSize) ? Music_LoadedTrack : -1,
                  size);
    }
    // DIV-0092's resume: the stream's sample count from here, and a pending
    // position for this track taken before the first half is decoded.
    sound::g_ring.track_stream = file == Music_File && size == static_cast<unsigned>(Music_FileSize);
    sound::g_ring.track = Music_LoadedTrack;
    sound::g_ring.written = 0;
    if (sound::g_restore_skip.armed) {  // the quicksave's rebuild (SOUND): the sample heard at the take
        sound::g_restore_skip.armed = false;
        const int loop_track = !music_seq::Active() && loops && sound::g_ring.track_stream ? Music_LoadedTrack : -1;
        sound::g_ring.written = sound::ResumeSkip(sound::g_restore_skip.sample, loops, loop_track, size);
        sound::g_restore_skip.reached = sound::g_ring.written;
    } else if (sound::g_resume.pending) {
        sound::g_resume.pending = false;
        if (sound::g_ring.track_stream && Music_LoadedTrack == static_cast<int>(sound::g_resume.pending_track)) {
            const int loop_track = !music_seq::Active() && loops && sound::g_ring.track_stream ? Music_LoadedTrack : -1;
            sound::g_ring.written = sound::ResumeSkip(sound::g_resume.pending_sample, loops, loop_track, size);
            MUSIC_LOG("resume track %u at sample %u%s", Frame_Counter, sound::g_resume.pending_track, sound::g_ring.written,
                      sound::g_ring.written == sound::g_resume.pending_sample ? "" : " (asked past its end: from the top)");
        }
    }
    void* const buffer = sound::g.music_create_buffer();
    Music_Buffer = buffer;
    sound::g_ring.half_base[0] = sound::g_ring.written;  // Music_CreateBuffer wrote the first half
    sound::g_ring.written += kHalfSamples;
    if (buffer) {
        sound::g.music_set_notify(buffer);
        void* const playing = Music_Buffer;
        sound::Method<ComPlay>(playing, kPlay)(playing, 0, 0, 1);
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
    if (sound::g.mp3_create(&decoder) != 0) return nullptr;
    unsigned char io[0x2C];
    sound::g.mp3_memory_io(io);
    sound::g.mp3_set_io(decoder, io);
    char name[0x20];
    sound::g.sprintf(name, reinterpret_cast<const char*>(::bof3::Img(kMemoryName.v)), size, file);
    if (sound::g.mp3_open(decoder, name, -1) != 0) return nullptr;
    return sound::g.mp3_start(decoder) == 0 ? decoder : nullptr;
}

// original 0x5A6DF0: the streaming buffer's two notifications - one event as
// the play cursor passes the start, one at the half.
// As the original has it: QueryInterface's result is not looked at; the first
// event is read back from Music_Events after the second is made, the second
// used as CreateEventA returned it. Returns SetNotificationPositions' result.
extern "C" long __cdecl Music_SetNotify(void* buffer) {
    // The slot as QueryInterface's out parameter (void**): the 32-bit spelling,
    // as Snd_Init's (sound_rest.cpp); B5's sound layer replaces it.
    sound::Method<ComQuery>(buffer, kQueryInterface)(buffer, sound::At(kIidNotify), reinterpret_cast<void**>(&Music_Notify));
    Music_Events[0] = sound::g.create_event(nullptr, 0, 0, nullptr);
    void* const second = sound::g.create_event(nullptr, 0, 0, nullptr);
    Music_Events[1] = second;
    struct Position { unsigned long offset; void* event; };
    const Position positions[2] = {{0, Music_Events[0]}, {kHalf, second}};
    void* const notify = Music_Notify;
    return sound::Method<ComPositions>(notify, kSetNotificationPositions)(notify, 2, positions);
}

// original 0x5A6E60: the streaming buffer: 44,100 Hz 16-bit stereo PCM,
// 0x24000 bytes (two halves), flags 0x181E0 (CTRLFREQUENCY, CTRLPAN,
// CTRLVOLUME, CTRLPOSITIONNOTIFY, GLOBALFOCUS, GETCURRENTPOSITION2), its first
// half decoded and written. 0 when CreateSoundBuffer fails.
extern "C" void* __cdecl Music_CreateBuffer(void) {
    std::memset(Snd_BufferDesc, 0, 0x24);
    void* const device = Snd_Device;
    const std::uint32_t format = sound::Address(Snd_WaveFormat);
    sound::SetWord(format + 0x00, 1);          // WAVE_FORMAT_PCM
    sound::SetWord(format + 0x02, 2);
    sound::SetDword(format + 0x04, 0xAC44);    // 44,100
    sound::SetDword(format + 0x08, 0x2B110);   // bytes a second
    sound::SetWord(format + 0x0C, 4);
    sound::SetWord(format + 0x0E, 0x10);
    sound::SetWord(format + 0x10, 0);
    const std::uint32_t desc = sound::Address(Snd_BufferDesc);
    sound::SetDword(desc + 0x00, 0x24);
    sound::SetDword(desc + 0x04, 0x181E0);
    sound::SetDword(desc + 0x08, 2 * kHalf);
    sound::SetDword(desc + 0x10, format);
    void* buffer = nullptr;
    if (sound::Method<ComCreate>(device, kCreateSoundBuffer)(device, Snd_BufferDesc, &buffer, nullptr) != 0) return nullptr;
    sound::g.music_decode(Music_Staging, kHalf);
    sound::g.sndbuf_write(buffer, Music_Staging, 0, kHalf);
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
    std::uint32_t decoder = sound::Address(Music_Decoder);
    std::uint32_t filled = 0;
    if (!decoder) return;
    if (sound::g_loop.row) {  // a measured loop (music_loops.h); everything else as the original
        LoopDecode(dst, static_cast<std::uint32_t>(size));
        return;
    }
    for (;;) {
        const int result = sound::g.mp3_decode(sound::At(decoder), sound::At(filled + sound::Address(dst)));
        decoder = sound::Address(Music_Decoder);
        filled += sound::Dword(sound::Dword(decoder + 0x14) + 8);
        if (result == kEndOfStream) {
            if (!Music_Loops) {
                std::memset(sound::At(sound::Address(dst) + filled), 0, static_cast<std::uint32_t>(size) - filled);
                Music_Finished = 1;
                return;
            }
            sound::g.mp3_seek(sound::At(decoder), 0);
            decoder = sound::Address(Music_Decoder);
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
        sound::Method<ComValue>(buffer, kSetVolume)(buffer, static_cast<unsigned long>(music_seq::Level(volume)));
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
    sound::Method<ComValue>(buffer, kSetVolume)(buffer, static_cast<unsigned long>(static_cast<std::uint64_t>(level)));
}

// The two entries whose originals read the ecx their caller left: each passes
// it to its body, above, as it found it - a pair by width
// (docs/x64-asm-policy.md 8 unit 4): on i686 pushed as the body's argument,
// on x86-64 one jump, since the caller's ecx is the first argument register
// already.
extern "C" __attribute__((naked)) int __cdecl Music_Halt(void) {
#if defined(__i386__)
    __asm__ volatile(
        "push %ecx\n\t"
        "call " BOF3_SYM(SoundRest_HaltBody) "\n\t"
        "add $4, %esp\n\t"
        "ret");
#elif defined(__x86_64__)
    __asm__ volatile("jmp " BOF3_SYM(SoundRest_HaltBody));   // the caller's ecx is the body's first argument already
#else
#error "no ecx entry for this architecture"
#endif
}

// The two entries whose originals read the ecx their caller left, through
// an uninitialised local: 0x5A7020 (Music_IsPlaying) and 0x5884A0
// (LoadMenu_Error). Each passes ecx to its body, above, as it found it: a
// pair by width (docs/x64-asm-policy.md 8 unit 4) - on i686 pushed as the
// body's argument, on x86-64 one jump, since the caller's ecx is the first
// argument register already.
extern "C" __attribute__((naked)) int __cdecl Music_IsPlaying(void) {
#if defined(__i386__)
    __asm__ volatile(
        "push %ecx\n\t"
        "call " BOF3_SYM(SaveMenu_MusicPlayingBody) "\n\t"
        "add $4, %esp\n\t"
        "ret");
#elif defined(__x86_64__)
    __asm__ volatile("jmp " BOF3_SYM(SaveMenu_MusicPlayingBody));   // the caller's ecx is the body's first argument already
#else
#error "no ecx entry for this architecture"
#endif
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
    sound::Method<sound::ComCall>(playing, kStop)(playing);
}

// original 0x5A7080: the music played again, looping, from where it stopped
// (Play(0, 0, DSBPLAY_LOOPING); no rewind). eax Play's, or 0 without a buffer.
//
// DIVERGENCE DIV-0177 (sound_in.h): after a jingle the game had bracketed with
// Sound_StopMusic, the song taken at the jingle's start is started again
// instead (eax 0, DS_OK) - the ring holds the spent jingle, which Capcom's
// Play would replay. With the switch off, or no such song, Capcom's.
extern "C" int __cdecl Music_Resume(void) {
    if (sound::MusicLogOn()) bof3::Log("music_log   f%u Music_Resume track %u", Frame_Counter, static_cast<unsigned>(Music_Track));
    if (sound_in::JingleResume()) return 0;  // DIV-0177
    void* const buffer = Music_Buffer;
    if (buffer == nullptr) return 0;
    return static_cast<int>(sound::Method<ComPlay>(buffer, sound::kPlay)(buffer, 0, 0, kLooping));
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
            sound::Method<sound::ComCall>(playing, kStop)(playing);
        }
        void* const releasing = Music_Buffer;
        sound::Method<sound::ComCall>(releasing, sound::kRelease)(releasing);
        Music_Buffer = nullptr;
    }
    void* const data = Music_Data;
    if (data) {
        sound::g.free(data);
        Music_Data = nullptr;
    }
    void* const notify = Music_Notify;
    if (notify) {
        sound::Method<sound::ComCall>(notify, sound::kRelease)(notify);
        void* const first = Music_Events[0];
        Music_Notify = nullptr;
        sound::g.close_handle(first);
        sound::g.close_handle(Music_Events[1]);
    }
    void* const decoder = Music_Decoder;
    if (decoder) {
        sound::g.mp3_destroy(decoder);
        Music_Decoder = nullptr;
    }
    music_seq::Release();  // DIV-0087: the synth stopped, its bank kept
}

// original 0x5A7140: nothing without Snd_Device. Else the playing stream
// stopped, a buffer made of the WAV image and kept at 0x7DE3C8,
// SetCurrentPosition(0) and Play; a lost buffer (DSERR_BUFFERLOST) gets the
// wave's data written again (SndBuf_Write restores it) and a second Play.
extern "C" void __cdecl SndStream_Play(const unsigned char* wave) {
    if (sound::MusicLogOn())  // the voice stream's line in the music log (BOF3X_MUSIC_LOG=1; sound.cpp)
        bof3::Log("music_log   f%u SndStream_Play(%p) from %p", Frame_Counter, static_cast<const void*>(wave),
                  __builtin_return_address(0));
    if (Snd_Device == nullptr) return;
    battle_items::g.stream_stop();
    void* const buffer = battle_items::g.create_from_wave(wave);
    StreamBuffer() = buffer;
    if (buffer == nullptr) return;
    MethodBattleItems<long (__stdcall*)(void*, unsigned long)>(buffer, 0x34)(buffer, 0);
    void* b = StreamBuffer();
    if (MethodBattleItems<long (__stdcall*)(void*, unsigned long, unsigned long, unsigned long)>(b, 0x30)(b, 0, 0, 0) != kBufferLostBattleItems)
        return;
    const unsigned char* data = nullptr;
    const unsigned size = battle_items::g.find_data(wave, &data);
    battle_items::g.buffer_write(StreamBuffer(), data, 0, size);
    b = StreamBuffer();
    MethodBattleItems<long (__stdcall*)(void*, unsigned long, unsigned long, unsigned long)>(b, 0x30)(b, 0, 0, 0);
}

// original 0x5A71C0: the stream's buffer, if any, stopped when GetStatus says
// it plays (bit 0), released, and forgotten. The original's status word is
// an uninitialised local that GetStatus fills; ours starts at 0.
extern "C" void __cdecl SndStream_Stop(void) {
    void* b = StreamBuffer();
    if (b == nullptr) return;
    unsigned long status = 0;
    MethodBattleItems<long (__stdcall*)(void*, unsigned long*)>(b, 0x24)(b, &status);
    if (status & 1) {
        b = StreamBuffer();
        MethodBattleItems<long (__stdcall*)(void*)>(b, 0x48)(b);
    }
    b = StreamBuffer();
    MethodBattleItems<unsigned long (__stdcall*)(void*)>(b, 8)(b);
    StreamBuffer() = nullptr;
}

extern "C" __attribute__((naked)) int __cdecl SndStream_IsPlaying(void) {
#if defined(__i386__)
    __asm__ volatile(
        "push %ecx\n\t"
        "call " BOF3_SYM(SoundRest_StreamPlayingBody) "\n\t"
        "add $4, %esp\n\t"
        "ret");
#elif defined(__x86_64__)
    __asm__ volatile("jmp " BOF3_SYM(SoundRest_StreamPlayingBody));   // the caller's ecx is the body's first argument already
#else
#error "no ecx entry for this architecture"
#endif
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
#if defined(__i386__)
    const unsigned signalled = sound::g.msg_wait(2, Music_Events, 0, 0, 0xFF);
#else
    // MsgWaitForMultipleObjects reads a native HANDLE array, 8 bytes an entry
    // on x86-64; Music_Events is two 32-bit image slots. Handed on as they
    // stand, the wait read one merged handle and the word after them, failed
    // every frame, and the ring was never refilled after its first half.
    void* const events[2] = {Music_Events[0], Music_Events[1]};
    const unsigned signalled = sound::g.msg_wait(2, events, 0, 0, 0xFF);
#endif
    if (signalled >= 2) return;
    if (Music_Finished) {
        sound::g.music_stop();
        Music_Finished = 0;
        return;
    }
    sound::g.music_decode(Music_Staging, kHalf);
    sound::g.sndbuf_write(Music_Buffer, Music_Staging, signalled != 0 ? 0 : kHalf, kHalf);
    sound::g_ring.half_base[signalled != 0 ? 0 : 1] = sound::g_ring.written;  // DIV-0092's resume: the ring's bookkeeping
    sound::g_ring.written += kHalfSamples;
}

void Music_Inject() {
    BOF3_INJECT(Sound_Shutdown);
    BOF3_INJECT(SndBuf_FromWave);
    BOF3_INJECT(SndBuf_CreateFromWave);
    BOF3_INJECT(Wave_FindData);
    BOF3_INJECT(SndBuf_Write);
    BOF3_INJECT(SndBuf_Play);
    BOF3_INJECT(SndBuf_Stop);
    BOF3_INJECT(SndBuf_SetVolume);
    BOF3_INJECT(SndBuf_Release);
    BOF3_INJECT(Music_Start);
    BOF3_INJECT(Music_OpenDecoder);
    BOF3_INJECT(Music_SetNotify);
    BOF3_INJECT(Music_CreateBuffer);
    BOF3_INJECT(Music_Decode);
    BOF3_INJECT(Music_SetVolume);
    BOF3_INJECT(Music_Halt);
    BOF3_INJECT(Music_IsPlaying);
    BOF3_INJECT(Music_Stop);
    BOF3_INJECT(Music_Resume);
    BOF3_INJECT(Music_Release);
    BOF3_INJECT(SndStream_Play);
    BOF3_INJECT(SndStream_Stop);
    BOF3_INJECT(SndStream_IsPlaying);
    BOF3_INJECT(Music_Pump);
}

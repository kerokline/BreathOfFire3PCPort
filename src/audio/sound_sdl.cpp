// DIV-0201: the sound output through SDL3 (sound_sdl.h; docs/x64-build-design.md
// section 6.3, docs/x64-build-round.md stage B group B5).
//
// What the game keeps a pointer to (design 6.2): a DirectSound device in
// Snd_Device, the primary buffer in Snd_Primary, the music ring in
// Music_Buffer with its notify object in Music_Notify, the SND stream in
// SndStream_Buffer, and a static secondary buffer per voice of every loaded
// bank (Sound_Banks' entries, Sound_Channels). It calls them through their
// vtables - 21 methods of three interfaces, sound_callees.h's offsets - and
// through DirectSoundCreate's import slot 0x5C4014. Every one of those words
// is a 32-bit slot of the image, so on the 64-bit host the objects must lie
// below 4 GB and answer through 32-bit out-places: Windows' own dsound.dll
// cannot (x64 descriptors, 8-byte out-pointers), and the owner's call 4 of
// 2026-11-01 retires it for SDL3 rather than a marshalling shim.
//
// The objects here:
//   - Device: a static singleton of ours, IDirectSound's 11 slots. Create
//     writes its address into the slot the game passes (a 32-bit word) and
//     opens one SDL3 device stream, 44,100 Hz 16-bit stereo. CreateSoundBuffer
//     reads the game's 32-bit DSBUFFERDESC (0x24 bytes, the format a 32-bit
//     place at +0x10) and hands out a Buffer.
//   - Buffer: IDirectSoundBuffer's 21 slots, from a static pool (so below 4
//     GB without a heap check), a PCM ring in a heap block, the format, a play
//     cursor in 32.32 fixed-point source frames, frequency, volume and pan in
//     DirectSound's hundredths of a decibel, playing and looping, and up to
//     eight position notifications. The primary is a Buffer with SetFormat and
//     no data: the game sets 22,050 Hz 8-bit stereo on it and writes nothing.
//   - the notify facet: a second vtable pointer inside the Buffer, reached by
//     QueryInterface(IID_IDirectSoundNotify); its Release is the buffer's.
//
// The mixer (Feed, SDL's callback on its device thread): for every frame the
// device asks for, each playing buffer is read at its own rate (linear
// interpolation between source frames, as the WAV dump models DirectSound),
// its volume and pan applied as gains (10^(hundredths / 2000); -10,000 is
// silence), summed and clipped to 16 bits; the play cursor advances by what
// was consumed, so GetCurrentPosition is exact, and a notification fires as
// the cursor moves onto or past its offset - at Play for an offset equal to
// the start position (the ring's offset-0 event, which Music_Pump relies on
// to fill the second half before the cursor reaches it), and on every wrap of
// a looping buffer. A once-only buffer that reaches its end stops with its
// cursor at 0. Stop leaves the cursor where it is, as DirectSound does.
//
// What the game sees of it: the same answers DirectSound gave at the same
// frames up to the device's timing (design 6.4): PLAYING from Play until Stop
// or a once-only end, a cursor advancing at the buffer's rate, GetFrequency
// the effective rate, GetVolume / GetPan what was set, never BUFFERLOST (so
// Restore is a no-op and the lost-buffer paths of SndBuf_Write, SndBuf_Play and
// SndStream_Play are dead). No word of .data is written by this file: only the
// out-places the game passes, which DirectSound wrote too, and the import slot
// in .rdata, outside the state hash's range.
//
// Width: the game's out-slot for an object (Create's `out`, CreateSoundBuffer's,
// QueryInterface's) is a 32-bit word of the image or a local the game zeroed,
// so the object's address is written as 4 bytes in both widths - an 8-byte
// write at &Snd_Primary would clobber Music_Buffer beside it. Lock's out-pointers
// are the game's own locals (void* first / second in SndBuf_Write), native in
// width, and get a whole pointer. Everything else the game passes is a 32-bit
// value (unsigned long is 32 bits on Win64 too) or a native pointer to a local.
// Nothing in this file is conditional on the width.
#include "audio/sound_sdl.h"

#include <windows.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/image_base.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"

namespace bof3 {
namespace {

constexpr unsigned kOutRate = 44100;     // the device stream's rate: the music ring's own
constexpr unsigned kOutChannels = 2;
constexpr unsigned kChunk = 1024;        // output frames a render pass
constexpr unsigned kPool = 1024;         // buffers at once: six banks of 64 voices, the ring, the stream, the primary
constexpr unsigned kMaxNotify = 8;       // the ring asks for two
constexpr unsigned kWriteLeadMs = 15;    // DirectSound's write cursor runs about this far ahead of the play cursor

// HRESULTs, dsound.h's.
constexpr long kOk = 0;
constexpr long kNoInterface = static_cast<long>(0x80004002u);          // E_NOINTERFACE
constexpr long kNoAggregation = static_cast<long>(0x80040110u);        // CLASS_E_NOAGGREGATION
constexpr long kInvalidParam = static_cast<long>(0x80070057u);         // DSERR_INVALIDPARAM
constexpr long kInvalidCall = static_cast<long>(0x88780032u);          // DSERR_INVALIDCALL
constexpr long kNoDriver = static_cast<long>(0x88780078u);             // DSERR_NODRIVER
constexpr long kAlreadyInitialized = static_cast<long>(0x88780082u);   // DSERR_ALREADYINITIALIZED
// DSBCAPS_*, DSBPLAY_*, DSBSTATUS_*, DSBLOCK_*.
constexpr std::uint32_t kCapsPrimary = 0x1;   // DSBCAPS_PRIMARYBUFFER; the rest of the flags are recorded, not read
constexpr std::uint32_t kPlayLooping = 0x1;
constexpr std::uint32_t kStatusPlaying = 0x1, kStatusLooping = 0x4;
constexpr std::uint32_t kLockFromWriteCursor = 0x1, kLockEntireBuffer = 0x2;
constexpr long kVolumeMin = -10000, kVolumeMax = 0, kPanLeft = -10000, kPanRight = 10000;
constexpr std::uint32_t kFrequencyMin = 100, kFrequencyMax = 200000;
constexpr std::uint32_t kDescSize = 0x24, kFormatSize = 0x12;
// IID_IDirectSoundNotify {B0210783-89CD-11D0-AF08-00A0C925CD16}, the bytes of
// the game's own copy at 0x5C45B8 (sound_callees.h kIidNotify).
constexpr unsigned char kIidNotify[16] = {0x83, 0x07, 0x21, 0xB0, 0xCD, 0x89, 0xD0, 0x11,
                                          0xAF, 0x08, 0x00, 0xA0, 0xC9, 0x25, 0xCD, 0x16};
// IID_IUnknown and IID_IDirectSoundBuffer, answered with the buffer itself.
constexpr unsigned char kIidUnknown[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46};
constexpr unsigned char kIidBuffer[16] = {0x85, 0xFA, 0x9A, 0x27, 0x81, 0x49, 0xCE, 0x11,
                                          0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60};   // {279AFA85-4981-11CE-A521-0020AF0BE560}
// The game's own copy of IID_IDirectSoundNotify, .rdata 0x5C45B8 (sound_callees.h kIidNotify): the
// self-test reads it against kIidNotify above.
constexpr std::uint32_t kIidNotifyVa = 0x5C45B8;

// --- Width helpers ----------------------------------------------------------

template <class T>
T* Widen(std::uint32_t place) { return reinterpret_cast<T*>(static_cast<std::uintptr_t>(place)); }

std::uint32_t U32At(const void* p, unsigned off) {
    std::uint32_t v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + off, 4);
    return v;
}
std::uint16_t U16At(const void* p, unsigned off) {
    std::uint16_t v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + off, 2);
    return v;
}

// An object's address into the game's out-slot, as 4 bytes (the header
// comment's width rule).
void PutObject(void** out, const void* object) {
    const std::uint32_t word = Low32(object);
    std::memcpy(out, &word, 4);
}

// --- The lock ----------------------------------------------------------------

CRITICAL_SECTION g_lock;
bool g_lock_made = false;
struct Guard {
    Guard() { EnterCriticalSection(&g_lock); }
    ~Guard() { LeaveCriticalSection(&g_lock); }
};

// --- The objects --------------------------------------------------------------

struct Format {
    std::uint16_t tag = 0, channels = 0, block = 0, bits = 0, cb = 0;
    std::uint32_t rate = 0, avg = 0;
};

Format ReadFormat(const void* p) {
    Format f;
    f.tag = U16At(p, 0x00);
    f.channels = U16At(p, 0x02);
    f.rate = U32At(p, 0x04);
    f.avg = U32At(p, 0x08);
    f.block = U16At(p, 0x0C);
    f.bits = U16At(p, 0x0E);
    f.cb = U16At(p, 0x10);
    return f;
}

void WriteFormat(const Format& f, void* p) {
    unsigned char b[kFormatSize];
    std::memcpy(b + 0x00, &f.tag, 2);
    std::memcpy(b + 0x02, &f.channels, 2);
    std::memcpy(b + 0x04, &f.rate, 4);
    std::memcpy(b + 0x08, &f.avg, 4);
    std::memcpy(b + 0x0C, &f.block, 2);
    std::memcpy(b + 0x0E, &f.bits, 2);
    std::memcpy(b + 0x10, &f.cb, 2);
    std::memcpy(p, b, kFormatSize);
}

struct Notification {
    std::uint32_t offset;
    HANDLE event;
};

struct Buffer {
    void* vtable;          // +0: IDirectSoundBuffer's
    void* notify_vtable;   // the IDirectSoundNotify facet: its address is what QueryInterface hands out
    unsigned refs;         // the buffer's and the facet's together
    bool live, primary, playing, looping;
    Format format;
    std::uint32_t flags;   // the descriptor's
    std::uint32_t size;    // bytes
    unsigned char* data;   // the ring; null for the primary
    std::uint64_t pos;     // the play cursor, 32.32 fixed-point source frames
    std::uint64_t step;    // source frames an output frame advances, 32.32
    std::uint32_t frequency;   // 0: the format's rate
    long volume, pan;
    float gain_l, gain_r;
    unsigned notify_count;
    Notification notifies[kMaxNotify];
};

struct Device {
    void* vtable;
    unsigned refs;
    bool cooperative;
    std::uint32_t level;
};

void* g_device_vtable[0x2C / 4];
void* g_buffer_vtable[0x54 / 4];
void* g_notify_vtable[0x10 / 4];
Device g_device = {g_device_vtable, 0, false, 0};
Buffer g_pool[kPool];
unsigned g_live = 0, g_live_max = 0, g_made = 0;
SDL_AudioStream* g_stream = nullptr;
bool g_audio_up = false;   // SDL_InitSubSystem(AUDIO) succeeded
bool g_on = false;         // the slot bound to Create
bool g_patched = false;    // the PatchBytes made (its OFF answer logged); the self-test skips it
std::uint64_t g_frames_out = 0;   // frames the device took

Buffer* Self(void* p) { return static_cast<Buffer*>(p); }
Buffer* OfNotify(void* p) {
    return reinterpret_cast<Buffer*>(static_cast<unsigned char*>(p) - offsetof(Buffer, notify_vtable));
}

// A method on an object that is not one of ours, or one already released:
// the loud answer, never a read of free memory.
void CheckBuffer(const Buffer* b, const char* method) {
    const std::uintptr_t at = reinterpret_cast<std::uintptr_t>(b), lo = reinterpret_cast<std::uintptr_t>(g_pool);
    if (at < lo || at >= lo + sizeof g_pool || (at - lo) % sizeof(Buffer) != 0)
        Fatal("sound_sdl: %s on %p, which is no buffer of ours", method, static_cast<const void*>(b));
    if (!b->live) Fatal("sound_sdl: %s on buffer %p after its last Release", method, static_cast<const void*>(b));
}
void CheckDevice(const void* d, const char* method) {
    if (d != &g_device) Fatal("sound_sdl: %s on %p, which is not our device", method, d);
    if (g_device.refs == 0) Fatal("sound_sdl: %s on the device after its last Release", method);
}

std::uint32_t EffectiveRate(const Buffer* b) { return b->frequency ? b->frequency : b->format.rate; }

void SetStep(Buffer* b) {
    const std::uint64_t rate = EffectiveRate(b);
    b->step = (rate << 32) / kOutRate;
}

float Gain(long hundredths) {
    if (hundredths <= kVolumeMin) return 0.0f;
    if (hundredths >= 0) return 1.0f;
    return static_cast<float>(std::pow(10.0, hundredths / 2000.0));
}

void SetGains(Buffer* b) {
    const float v = Gain(b->volume);
    const float l = b->pan > 0 ? Gain(-b->pan) : 1.0f;
    const float r = b->pan < 0 ? Gain(b->pan) : 1.0f;
    b->gain_l = v * l;
    b->gain_r = v * r;
}

std::uint32_t FramesOf(const Buffer* b) { return b->format.block ? b->size / b->format.block : 0; }
std::uint32_t PlayBytes(const Buffer* b) { return static_cast<std::uint32_t>(b->pos >> 32) * b->format.block; }

// The notifications whose offset the cursor moved onto or past, from
// `old_frame` (exclusive) to `new_frame` (inclusive), through the wrap when
// `wrapped`; `whole` when a pass advanced a whole buffer or more (every one).
void Signal(Buffer* b, std::uint32_t old_frame, std::uint32_t new_frame, bool wrapped, bool whole) {
    for (unsigned i = 0; i < b->notify_count; ++i) {
        const std::uint32_t at = b->notifies[i].offset / b->format.block;
        const bool hit = whole || (!wrapped ? old_frame < at && at <= new_frame : at > old_frame || at <= new_frame);
        if (hit) SetEvent(b->notifies[i].event);
    }
}

// A sample of the ring at `frame`, channel `ch`, as a 16-bit value (8-bit
// unsigned PCM centred at 0x80, mono on both channels).
inline int Sample(const Buffer* b, std::uint32_t frame, unsigned ch) {
    const unsigned char* p = b->data + static_cast<std::size_t>(frame) * b->format.block;
    const unsigned c = ch < b->format.channels ? ch : 0;
    if (b->format.bits == 8) return (static_cast<int>(p[c]) - 128) << 8;
    std::int16_t v;
    std::memcpy(&v, p + 2 * c, 2);
    return v;
}

// One buffer into `acc` (interleaved stereo, `n` frames), the cursor advanced.
void MixInto(Buffer* b, std::int32_t* acc, unsigned n) {
    const std::uint32_t total = FramesOf(b);
    if (total == 0 || !b->data) return;
    const std::uint64_t end = static_cast<std::uint64_t>(total) << 32;
    const std::uint64_t start = b->pos;
    std::uint64_t pos = b->pos;
    std::uint64_t advanced = 0;
    bool wrapped = false, stopped = false;
    for (unsigned i = 0; i < n; ++i) {
        const std::uint32_t idx = static_cast<std::uint32_t>(pos >> 32);
        const std::int64_t frac = static_cast<std::int64_t>(pos & 0xFFFFFFFFu);   // 32-bit fraction
        std::uint32_t next = idx + 1;
        if (next >= total) next = b->looping ? next - total : idx;
        const int l0 = Sample(b, idx, 0), l1 = Sample(b, next, 0);
        const int r0 = Sample(b, idx, 1), r1 = Sample(b, next, 1);
        const int l = l0 + static_cast<int>((static_cast<std::int64_t>(l1 - l0) * frac) >> 32);
        const int r = r0 + static_cast<int>((static_cast<std::int64_t>(r1 - r0) * frac) >> 32);
        acc[2 * i] += static_cast<std::int32_t>(std::lrint(l * b->gain_l));
        acc[2 * i + 1] += static_cast<std::int32_t>(std::lrint(r * b->gain_r));
        pos += b->step;
        advanced += b->step;
        if (pos >= end) {
            if (!b->looping) {
                stopped = true;
                break;
            }
            wrapped = true;
            while (pos >= end) pos -= end;
        }
    }
    if (stopped) {
        // A once-only buffer at its end: stopped, the cursor at 0 (DirectSound's).
        b->playing = false;
        b->pos = 0;
        Signal(b, static_cast<std::uint32_t>(start >> 32), total - 1, false, false);
        return;
    }
    b->pos = pos;
    Signal(b, static_cast<std::uint32_t>(start >> 32), static_cast<std::uint32_t>(pos >> 32), wrapped, advanced >= end);
}

// `n` output frames of the mix into `out`, under the lock.
void RenderLocked(std::int16_t* out, unsigned n) {
    static std::int32_t acc[kChunk * kOutChannels];
    while (n) {
        const unsigned m = n < kChunk ? n : kChunk;
        std::memset(acc, 0, sizeof(std::int32_t) * m * kOutChannels);
        for (unsigned i = 0; i < kPool; ++i) {
            Buffer& b = g_pool[i];
            if (b.live && b.playing && !b.primary) MixInto(&b, acc, m);
        }
        for (unsigned i = 0; i < m * kOutChannels; ++i) {
            const std::int32_t v = acc[i];
            out[i] = static_cast<std::int16_t>(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        out += m * kOutChannels;
        n -= m;
        g_frames_out += m;
    }
}

// SDL's callback, on its device thread: what the device wants more of.
void SDLCALL Feed(void*, SDL_AudioStream* stream, int additional, int) {
    if (additional <= 0) return;
    static std::int16_t out[kChunk * kOutChannels];
    unsigned frames = static_cast<unsigned>(additional) / (kOutChannels * 2);
    if (static_cast<unsigned>(additional) % (kOutChannels * 2)) ++frames;
    Guard g;
    while (frames) {
        const unsigned m = frames < kChunk ? frames : kChunk;
        RenderLocked(out, m);
        SDL_PutAudioStreamData(stream, out, static_cast<int>(m * kOutChannels * 2));
        frames -= m;
    }
}

// --- The SDL3 device stream -----------------------------------------------------

bool OpenStream(bool quiet) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        Log("sound_sdl   SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        return false;
    }
    g_audio_up = true;
    const SDL_AudioSpec want = {SDL_AUDIO_S16, static_cast<int>(kOutChannels), static_cast<int>(kOutRate)};
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, &Feed, nullptr);
    if (!g_stream) {
        Log("sound_sdl   SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        g_audio_up = false;
        return false;
    }
    const SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(g_stream);
    SDL_AudioSpec have{};
    int sample_frames = 0;
    SDL_GetAudioDeviceFormat(dev, &have, &sample_frames);
    const char* name = SDL_GetAudioDeviceName(dev);
    if (!quiet)
        Log("sound_sdl   device stream open: %s, the device at %d Hz, %d channel(s), format 0x%04X, %d frames a "
            "buffer; our stream %u Hz 16-bit stereo",
            name ? name : "(unnamed)", have.freq, have.channels, static_cast<unsigned>(have.format), sample_frames,
            kOutRate);
    if (!SDL_ResumeAudioStreamDevice(g_stream)) Log("sound_sdl   SDL_ResumeAudioStreamDevice failed: %s", SDL_GetError());
    return true;
}

void CloseStream() {
    if (g_stream) {
        SDL_DestroyAudioStream(g_stream);
        g_stream = nullptr;
    }
    if (g_audio_up) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        g_audio_up = false;
    }
}

// --- The device: IDirectSound ----------------------------------------------------

long __stdcall DevQueryInterface(void* self, const void*, void**) {
    CheckDevice(self, "IDirectSound::QueryInterface");
    Fatal("sound_sdl: IDirectSound::QueryInterface, which the game never calls");
}
unsigned long __stdcall DevAddRef(void* self) {
    CheckDevice(self, "IDirectSound::AddRef");
    Guard g;
    return ++g_device.refs;
}
unsigned long __stdcall DevRelease(void* self) {
    CheckDevice(self, "IDirectSound::Release");
    unsigned left;
    {
        Guard g;
        left = --g_device.refs;
    }
    if (left == 0) {
        CloseStream();   // outside our lock: SDL's thread may be in Feed, waiting for it
        Log("sound_sdl   device released: %u buffers made, %u live at most, %u still live; %llu frames mixed (%.1f s)",
            g_made, g_live_max, g_live, static_cast<unsigned long long>(g_frames_out),
            static_cast<double>(g_frames_out) / kOutRate);
    }
    return left;
}

Buffer* Take() {
    for (unsigned i = 0; i < kPool; ++i)
        if (!g_pool[i].live) {
            Buffer& b = g_pool[i];
            std::memset(&b, 0, sizeof b);
            b.vtable = g_buffer_vtable;
            b.notify_vtable = g_notify_vtable;
            b.live = true;
            b.refs = 1;
            if (++g_live > g_live_max) g_live_max = g_live;
            ++g_made;
            return &b;
        }
    Fatal("sound_sdl: more than %u buffers live at once", kPool);
}

void Drop(Buffer* b) {
    if (b->data) std::free(b->data);
    b->data = nullptr;
    b->live = false;
    b->playing = false;
    --g_live;
}

long __stdcall DevCreateSoundBuffer(void* self, const void* desc, void** out, void* outer) {
    CheckDevice(self, "IDirectSound::CreateSoundBuffer");
    if (!desc || !out) return kInvalidParam;
    if (outer) return kNoAggregation;
    // The game's 32-bit DSBUFFERDESC (design 6.2): size, flags, bytes,
    // reserved, the format's place as a 32-bit word. A caller passing the
    // x64 struct (40 bytes) is refused here, loudly enough to read in the log.
    const std::uint32_t size = U32At(desc, 0x00), flags = U32At(desc, 0x04), bytes = U32At(desc, 0x08),
                        format_place = U32At(desc, 0x10);
    if (size != kDescSize) {
        Log("sound_sdl   CreateSoundBuffer with a descriptor of %u bytes, not the game's 0x24: refused", size);
        return kInvalidParam;
    }
    Guard g;
    if (flags & kCapsPrimary) {
        if (bytes != 0 || format_place != 0) return kInvalidParam;
        Buffer* b = Take();
        b->primary = true;
        b->flags = flags;
        // The primary's format before SetFormat: DirectSound's default, 22,050 Hz 8-bit stereo.
        b->format = Format{1, 2, 2, 8, 0, 22050, 44100};
        PutObject(out, b);
        return kOk;
    }
    if (format_place == 0 || bytes < 4) return kInvalidParam;
    const Format f = ReadFormat(Widen<const void>(format_place));
    if (f.tag != 1 || (f.bits != 8 && f.bits != 16) || f.channels < 1 || f.channels > 2 ||
        f.block != f.channels * f.bits / 8 || f.rate < kFrequencyMin || f.rate > kFrequencyMax)
        Fatal("sound_sdl: a secondary buffer of format tag %u, %u channel(s), %u bits, block %u, %u Hz is not one this "
              "layer mixes", f.tag, f.channels, f.bits, f.block, f.rate);
    if (bytes % f.block) return kInvalidParam;
    unsigned char* data = static_cast<unsigned char*>(std::malloc(bytes));
    if (!data) Fatal("sound_sdl: no memory for a %u-byte buffer", bytes);
    std::memset(data, f.bits == 8 ? 0x80 : 0, bytes);
    Buffer* b = Take();
    b->flags = flags;
    b->format = f;
    b->size = bytes;
    b->data = data;
    b->volume = 0;
    b->pan = 0;
    SetStep(b);
    SetGains(b);
    PutObject(out, b);
    return kOk;
}
long __stdcall DevGetCaps(void* self, void*) {
    CheckDevice(self, "IDirectSound::GetCaps");
    Fatal("sound_sdl: IDirectSound::GetCaps, which the game never calls");
}
long __stdcall DevDuplicateSoundBuffer(void* self, void*, void**) {
    CheckDevice(self, "IDirectSound::DuplicateSoundBuffer");
    Fatal("sound_sdl: IDirectSound::DuplicateSoundBuffer, which the game never calls (the WAV dump refuses it too)");
}
long __stdcall DevSetCooperativeLevel(void* self, void* hwnd, std::uint32_t level) {
    CheckDevice(self, "IDirectSound::SetCooperativeLevel");
    // DSSCL_NORMAL 1 .. DSSCL_WRITEPRIMARY 4; the game asks for EXCLUSIVE (3),
    // which since Vista means the same as PRIORITY. Nothing to do: the
    // mixer is ours and the focus rule is the window's.
    if (level < 1 || level > 4) return kInvalidParam;
    Guard g;
    g_device.cooperative = true;
    g_device.level = level;
    (void)hwnd;
    return kOk;
}
long __stdcall DevCompact(void* self) {
    CheckDevice(self, "IDirectSound::Compact");
    Fatal("sound_sdl: IDirectSound::Compact, which the game never calls");
}
long __stdcall DevGetSpeakerConfig(void* self, std::uint32_t*) {
    CheckDevice(self, "IDirectSound::GetSpeakerConfig");
    Fatal("sound_sdl: IDirectSound::GetSpeakerConfig, which the game never calls");
}
long __stdcall DevSetSpeakerConfig(void* self, std::uint32_t) {
    CheckDevice(self, "IDirectSound::SetSpeakerConfig");
    Fatal("sound_sdl: IDirectSound::SetSpeakerConfig, which the game never calls");
}
long __stdcall DevInitialize(void* self, const void*) {
    CheckDevice(self, "IDirectSound::Initialize");
    return kAlreadyInitialized;
}

// DirectSoundCreate's shape: the slot 0x5C4014 holds this (SoundSdl_Start).
// `out` is &Snd_Device, a 32-bit slot of the image (Snd_Init, sound_rest.cpp).
long __stdcall Create(const void* guid, void** out, void* outer) {
    if (!out) return kInvalidParam;
    if (outer) return kNoAggregation;
    {
        Guard g;
        if (g_device.refs != 0) Fatal("sound_sdl: DirectSoundCreate while the device is live (%u references)", g_device.refs);
    }
    if (guid) Log("sound_sdl   DirectSoundCreate with a device GUID: the default playback device is used all the same");
    if (!OpenStream(false)) {   // outside our lock, as CloseStream is
        Log("sound_sdl   DirectSoundCreate: no audio device could be opened; the game plays silent (Snd_Init's code 100)");
        return kNoDriver;
    }
    Guard g;
    g_device.refs = 1;
    g_device.cooperative = false;
    g_frames_out = 0;
    PutObject(out, &g_device);
    return kOk;
}

// --- The buffer: IDirectSoundBuffer ------------------------------------------------

long __stdcall BufQueryInterface(void* self, const void* iid, void** out) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::QueryInterface");
    if (!iid || !out) return kInvalidParam;
    Guard g;
    if (std::memcmp(iid, kIidNotify, 16) == 0) {
        if (b->primary) {
            std::memset(out, 0, 4);
            return kNoInterface;
        }
        ++b->refs;
        PutObject(out, &b->notify_vtable);
        return kOk;
    }
    if (std::memcmp(iid, kIidUnknown, 16) == 0 || std::memcmp(iid, kIidBuffer, 16) == 0) {
        ++b->refs;
        PutObject(out, b);
        return kOk;
    }
    std::memset(out, 0, 4);
    return kNoInterface;
}
unsigned long __stdcall BufAddRef(void* self) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::AddRef");
    Guard g;
    return ++b->refs;
}
unsigned long __stdcall BufRelease(void* self) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::Release");
    Guard g;
    const unsigned left = --b->refs;
    if (left == 0) Drop(b);
    return left;
}
long __stdcall BufGetCaps(void* self, void* caps) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::GetCaps");
    if (!caps || U32At(caps, 0) != 0x14) return kInvalidParam;   // DSBCAPS: dwSize, dwFlags, dwBufferBytes, dwUnlockTransferRate, dwPlayCpuOverhead
    Guard g;
    const std::uint32_t words[5] = {0x14, b->flags, b->size, 0, 0};
    std::memcpy(caps, words, sizeof words);
    return kOk;
}
long __stdcall BufGetCurrentPosition(void* self, std::uint32_t* play, std::uint32_t* write) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::GetCurrentPosition");
    Guard g;
    const std::uint32_t p = b->primary ? 0 : PlayBytes(b);
    std::uint32_t w = p;
    if (!b->primary && b->playing && b->size) {
        const std::uint64_t lead_frames = static_cast<std::uint64_t>(EffectiveRate(b)) * kWriteLeadMs / 1000;
        w = static_cast<std::uint32_t>((p + lead_frames * b->format.block) % b->size);
    }
    if (play) *play = p;
    if (write) *write = w;
    return kOk;
}
long __stdcall BufGetFormat(void* self, void* format, std::uint32_t size, std::uint32_t* written) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::GetFormat");
    Guard g;
    if (!format) {
        if (!written) return kInvalidParam;
        *written = kFormatSize;
        return kOk;
    }
    if (size < kFormatSize) return kInvalidParam;
    WriteFormat(b->format, format);
    if (written) *written = kFormatSize;
    return kOk;
}
long __stdcall BufGetVolume(void* self, long* v) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::GetVolume");
    if (!v) return kInvalidParam;
    Guard g;
    *v = b->volume;
    return kOk;
}
long __stdcall BufGetPan(void* self, long* v) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::GetPan");
    if (!v) return kInvalidParam;
    Guard g;
    *v = b->pan;
    return kOk;
}
long __stdcall BufGetFrequency(void* self, std::uint32_t* v) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::GetFrequency");
    if (!v) return kInvalidParam;
    Guard g;
    *v = EffectiveRate(b);   // DirectSound answers the rate in force, the format's for DSBFREQUENCY_ORIGINAL
    return kOk;
}
long __stdcall BufGetStatus(void* self, std::uint32_t* s) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::GetStatus");
    if (!s) return kInvalidParam;
    Guard g;
    *s = b->playing ? kStatusPlaying | (b->looping ? kStatusLooping : 0) : 0;   // never DSBSTATUS_BUFFERLOST
    return kOk;
}
long __stdcall BufInitialize(void* self, void*, const void*) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::Initialize");
    return kAlreadyInitialized;
}
long __stdcall BufLock(void* self, std::uint32_t offset, std::uint32_t bytes, void** first, std::uint32_t* first_bytes,
                       void** second, std::uint32_t* second_bytes, std::uint32_t flags) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::Lock");
    if (b->primary) Fatal("sound_sdl: Lock on the primary buffer, which the game never writes (DSSCL_WRITEPRIMARY)");
    if (!first || !first_bytes) return kInvalidParam;
    Guard g;
    if (flags & kLockFromWriteCursor) {
        std::uint32_t play = 0;
        BufGetCurrentPosition(self, &play, &offset);
    }
    if (flags & kLockEntireBuffer) bytes = b->size;
    if (offset >= b->size || bytes == 0 || bytes > b->size) return kInvalidParam;
    const std::uint32_t n1 = bytes <= b->size - offset ? bytes : b->size - offset;
    *first = b->data + offset;   // the game's own local: a whole pointer
    *first_bytes = n1;
    if (second) *second = n1 < bytes ? b->data : nullptr;
    if (second_bytes) *second_bytes = bytes - n1;
    return kOk;
}
long __stdcall BufPlay(void* self, std::uint32_t, std::uint32_t, std::uint32_t flags) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::Play");
    Guard g;
    b->looping = (flags & kPlayLooping) != 0;
    if (b->primary) {   // the primary "plays" the mix; nothing of its own
        b->playing = true;
        return kOk;
    }
    if (FramesOf(b) == 0) return kInvalidCall;
    b->playing = true;
    // A notification at the position played from fires now: the ring's
    // offset-0 event at Music_Start, which fills the second half.
    const std::uint32_t frame = static_cast<std::uint32_t>(b->pos >> 32);
    for (unsigned i = 0; i < b->notify_count; ++i)
        if (b->notifies[i].offset / b->format.block == frame) SetEvent(b->notifies[i].event);
    return kOk;
}
long __stdcall BufSetCurrentPosition(void* self, std::uint32_t bytes) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::SetCurrentPosition");
    if (b->primary) return kInvalidCall;
    Guard g;
    if (bytes >= b->size) return kInvalidParam;
    b->pos = static_cast<std::uint64_t>(bytes / b->format.block) << 32;
    return kOk;
}
long __stdcall BufSetFormat(void* self, const void* format) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::SetFormat");
    if (!format) return kInvalidParam;
    if (!b->primary) return kInvalidCall;   // DirectSound: a secondary's format is fixed at its creation
    const Format f = ReadFormat(format);
    if (f.tag != 1 || (f.bits != 8 && f.bits != 16) || f.channels < 1 || f.channels > 2 || f.block != f.channels * f.bits / 8)
        return kInvalidParam;
    Guard g;
    b->format = f;
    // Recorded, not applied: since Vista the primary's format does not bind
    // the mixer (docs/wav-dump.md 1), and ours mixes at the music's rate.
    Log("sound_sdl   the primary's format set to %u Hz %u-bit %u channel(s) (recorded; the mix stays %u Hz 16-bit stereo)",
        f.rate, f.bits, f.channels, kOutRate);
    return kOk;
}
long __stdcall BufSetVolume(void* self, long v) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::SetVolume");
    if (v < kVolumeMin || v > kVolumeMax) return kInvalidParam;   // DirectSound refuses a level above 0 (Music_SetVolume's note)
    Guard g;
    b->volume = v;
    SetGains(b);
    return kOk;
}
long __stdcall BufSetPan(void* self, long v) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::SetPan");
    if (v < kPanLeft || v > kPanRight) return kInvalidParam;
    Guard g;
    b->pan = v;
    SetGains(b);
    return kOk;
}
long __stdcall BufSetFrequency(void* self, std::uint32_t v) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::SetFrequency");
    if (b->primary) return kInvalidCall;
    if (v != 0 && (v < kFrequencyMin || v > kFrequencyMax)) return kInvalidParam;
    Guard g;
    b->frequency = v;
    SetStep(b);
    return kOk;
}
long __stdcall BufStop(void* self) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::Stop");
    Guard g;
    b->playing = false;   // the cursor stays: Play resumes from it (Music_Resume's Play with no rewind)
    return kOk;
}
long __stdcall BufUnlock(void* self, void* first, std::uint32_t first_bytes, void* second, std::uint32_t second_bytes) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::Unlock");
    Guard g;
    const unsigned char* const f = static_cast<const unsigned char*>(first);
    const unsigned char* const s = static_cast<const unsigned char*>(second);
    if (!f || f < b->data || f + first_bytes > b->data + b->size) return kInvalidParam;
    if (s && (s < b->data || s + second_bytes > b->data + b->size)) return kInvalidParam;
    return kOk;   // the game wrote the ring's bytes in place; nothing to copy
}
long __stdcall BufRestore(void* self) {
    Buffer* b = Self(self);
    CheckBuffer(b, "IDirectSoundBuffer::Restore");
    return kOk;   // nothing is ever lost
}

// --- The notify facet: IDirectSoundNotify ------------------------------------------

long __stdcall NotQueryInterface(void* self, const void* iid, void** out) {
    Buffer* b = OfNotify(self);
    CheckBuffer(b, "IDirectSoundNotify::QueryInterface");
    return BufQueryInterface(b, iid, out);
}
unsigned long __stdcall NotAddRef(void* self) {
    Buffer* b = OfNotify(self);
    CheckBuffer(b, "IDirectSoundNotify::AddRef");
    return BufAddRef(b);
}
unsigned long __stdcall NotRelease(void* self) {
    Buffer* b = OfNotify(self);
    CheckBuffer(b, "IDirectSoundNotify::Release");
    return BufRelease(b);
}
// The game's own array: `struct Position { unsigned long offset; void* event; }`
// (Music_SetNotify, sound.cpp), DSBPOSITIONNOTIFY's shape in either width.
struct Position {
    unsigned long offset;
    void* event;
};
long __stdcall NotSetNotificationPositions(void* self, std::uint32_t count, const Position* positions) {
    Buffer* b = OfNotify(self);
    CheckBuffer(b, "IDirectSoundNotify::SetNotificationPositions");
    if (count && !positions) return kInvalidParam;
    if (count > kMaxNotify) Fatal("sound_sdl: %u notification positions asked for; this layer keeps %u", count, kMaxNotify);
    Guard g;
    if (b->playing) return kInvalidCall;   // DirectSound: not while the buffer plays
    for (unsigned i = 0; i < count; ++i) {
        if (positions[i].offset >= b->size || positions[i].offset % b->format.block || !positions[i].event) return kInvalidParam;
        b->notifies[i].offset = static_cast<std::uint32_t>(positions[i].offset);
        b->notifies[i].event = static_cast<HANDLE>(positions[i].event);
    }
    b->notify_count = count;
    return kOk;
}

// --- The vtables, DirectSound's order (dsound.h) -----------------------------------------

template <class F>
void* Fn(F* f) { return reinterpret_cast<void*>(f); }

void FillVtables() {
    void** d = g_device_vtable;
    d[0x00 / 4] = Fn(&DevQueryInterface);
    d[0x04 / 4] = Fn(&DevAddRef);
    d[0x08 / 4] = Fn(&DevRelease);
    d[0x0C / 4] = Fn(&DevCreateSoundBuffer);
    d[0x10 / 4] = Fn(&DevGetCaps);
    d[0x14 / 4] = Fn(&DevDuplicateSoundBuffer);
    d[0x18 / 4] = Fn(&DevSetCooperativeLevel);
    d[0x1C / 4] = Fn(&DevCompact);
    d[0x20 / 4] = Fn(&DevGetSpeakerConfig);
    d[0x24 / 4] = Fn(&DevSetSpeakerConfig);
    d[0x28 / 4] = Fn(&DevInitialize);
    void** b = g_buffer_vtable;
    b[0x00 / 4] = Fn(&BufQueryInterface);
    b[0x04 / 4] = Fn(&BufAddRef);
    b[0x08 / 4] = Fn(&BufRelease);
    b[0x0C / 4] = Fn(&BufGetCaps);
    b[0x10 / 4] = Fn(&BufGetCurrentPosition);
    b[0x14 / 4] = Fn(&BufGetFormat);
    b[0x18 / 4] = Fn(&BufGetVolume);
    b[0x1C / 4] = Fn(&BufGetPan);
    b[0x20 / 4] = Fn(&BufGetFrequency);
    b[0x24 / 4] = Fn(&BufGetStatus);
    b[0x28 / 4] = Fn(&BufInitialize);
    b[0x2C / 4] = Fn(&BufLock);
    b[0x30 / 4] = Fn(&BufPlay);
    b[0x34 / 4] = Fn(&BufSetCurrentPosition);
    b[0x38 / 4] = Fn(&BufSetFormat);
    b[0x3C / 4] = Fn(&BufSetVolume);
    b[0x40 / 4] = Fn(&BufSetPan);
    b[0x44 / 4] = Fn(&BufSetFrequency);
    b[0x48 / 4] = Fn(&BufStop);
    b[0x4C / 4] = Fn(&BufUnlock);
    b[0x50 / 4] = Fn(&BufRestore);
    void** n = g_notify_vtable;
    n[0x00 / 4] = Fn(&NotQueryInterface);
    n[0x04 / 4] = Fn(&NotAddRef);
    n[0x08 / 4] = Fn(&NotRelease);
    n[0x0C / 4] = Fn(&NotSetNotificationPositions);
}

// --- The self-test (BOF3X_SHADOW=sound_sdl hosted; BOF3X_SELFTEST_ONLY in bof3x.exe) ---
//
// The objects driven as the game drives them, through their vtables, and the
// mixer pulled by hand (RenderLocked, no device): the slots' 32-bit words,
// the descriptor read, Lock's regions and wrap, a ramp resampled from
// 22,050 Hz (out[k] = 5k exactly), the cursor after each pull, a once-only
// end, a looping wrap, the volume law at -600 (10^-0.3), -10,000 silent, pan
// hard left and -2,000, SetFrequency(44,100) and back to the format's, two
// notifications on a ring (fired at Play, at the half, at the wrap, and only
// then), the notify facet's reference counting, Stop keeping the cursor, two
// full-scale buffers clipped, an 8-bit source centred at 0x80, and the
// refusals DirectSound makes (a level above 0, a position past the end, a
// descriptor of the wrong size). Then the SDL3 device opened and closed once,
// logged, not a failure where this machine has no playback device. Everything
// is put back: every pool entry free, the device's references 0.

#define SDL_CHECK(cond, ...) \
    do {                     \
        if (!(cond)) Fatal("sound_sdl self-test: " __VA_ARGS__); \
    } while (0)

struct TestDesc {
    unsigned char bytes[kDescSize];
};
TestDesc MakeDesc(std::uint32_t flags, std::uint32_t size, std::uint32_t format_place) {
    TestDesc d{};
    const std::uint32_t words[5] = {kDescSize, flags, size, 0, format_place};
    std::memcpy(d.bytes, words, sizeof words);
    return d;
}

template <class F>
F Method(void* object, unsigned offset) {
    void* const* vt;
    std::memcpy(&vt, object, sizeof vt);
    return reinterpret_cast<F>(vt[offset / 4]);
}
using MCall = long(__stdcall*)(void*);
using MStatus = long(__stdcall*)(void*, std::uint32_t*);
using MGetLong = long(__stdcall*)(void*, long*);
using MPosition = long(__stdcall*)(void*, std::uint32_t*, std::uint32_t*);
using MValue = long(__stdcall*)(void*, std::uint32_t);
using MLong = long(__stdcall*)(void*, long);
using MPlay = long(__stdcall*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);
using MLock = long(__stdcall*)(void*, std::uint32_t, std::uint32_t, void**, std::uint32_t*, void**, std::uint32_t*, std::uint32_t);
using MUnlock = long(__stdcall*)(void*, void*, std::uint32_t, void*, std::uint32_t);
using MCreate = long(__stdcall*)(void*, const void*, void**, void*);
using MQuery = long(__stdcall*)(void*, const void*, void**);
using MPositions = long(__stdcall*)(void*, std::uint32_t, const Position*);
using MFormat = long(__stdcall*)(void*, const void*);
using MCooperate = long(__stdcall*)(void*, void*, std::uint32_t);

void* FromSlot(std::uint32_t word) { return Widen<void>(word); }

// `n` frames pulled from the mixer, as the device would pull them.
void Pull(std::int16_t* out, unsigned n) {
    Guard g;
    RenderLocked(out, n);
}

void Write(void* buf, std::uint32_t offset, const void* src, std::uint32_t n) {
    void* first = nullptr;
    void* second = nullptr;
    std::uint32_t n1 = 0, n2 = 0;
    SDL_CHECK(Method<MLock>(buf, 0x2C)(buf, offset, n, &first, &n1, &second, &n2, 0) == kOk, "Lock(%u, %u) refused", offset, n);
    std::memcpy(first, src, n1);
    if (n2) std::memcpy(second, static_cast<const unsigned char*>(src) + n1, n2);
    SDL_CHECK(Method<MUnlock>(buf, 0x4C)(buf, first, n1, second, n2) == kOk, "Unlock refused");
}

void SelfTest() {
    const unsigned live_before = g_live;
    SDL_CHECK(g_device.refs == 0, "the device is live before the test");
    static std::int16_t out[kChunk * kOutChannels * 2];

    // The device into a 32-bit slot word.
    std::uint32_t slot_words[2] = {0xDEADBEEFu, 0xCAFEF00Du};   // the second must stay: a 4-byte write
    SDL_CHECK(std::memcmp(Widen<const void>(Img(kIidNotifyVa)), kIidNotify, 16) == 0,
              "the game's IID_IDirectSoundNotify at 0x%08X is not the one QueryInterface answers", kIidNotifyVa);
    // Create opens the SDL device; the test of that is at the end. Here the
    // device object alone, as a Create that found no device would not make it.
    g_device.refs = 1;
    g_device.cooperative = false;
    PutObject(reinterpret_cast<void**>(&slot_words[0]), &g_device);
    SDL_CHECK(slot_words[0] == Low32(&g_device) && slot_words[1] == 0xCAFEF00Du, "the device's slot word");
    void* const device = FromSlot(slot_words[0]);
    SDL_CHECK(Method<MCooperate>(device, 0x18)(device, nullptr, 3) == kOk, "SetCooperativeLevel(EXCLUSIVE)");
    SDL_CHECK(Method<MCooperate>(device, 0x18)(device, nullptr, 9) == kInvalidParam, "SetCooperativeLevel(9) accepted");

    // The primary: no data, SetFormat recorded.
    std::uint32_t primary_word = 0;
    const TestDesc pdesc = MakeDesc(kCapsPrimary, 0, 0);
    SDL_CHECK(Method<MCreate>(device, 0x0C)(device, pdesc.bytes, reinterpret_cast<void**>(&primary_word), nullptr) == kOk, "the primary");
    void* const primary = FromSlot(primary_word);
    SDL_CHECK(Self(primary)->primary && Self(primary)->data == nullptr, "the primary's shape");
    static unsigned char fmt[kFormatSize];   // static: a format's place is a 32-bit word of the descriptor
    WriteFormat(Format{1, 2, 2, 8, 0, 22050, 44100}, fmt);
    SDL_CHECK(Method<MFormat>(primary, 0x38)(primary, fmt) == kOk, "SetFormat on the primary");
    SDL_CHECK(Self(primary)->format.rate == 22050 && Self(primary)->format.bits == 8, "the primary's format recorded");
    {
        unsigned char back[kFormatSize] = {};
        std::uint32_t written = 0;
        SDL_CHECK(Method<long(__stdcall*)(void*, void*, std::uint32_t, std::uint32_t*)>(primary, 0x14)(primary, back, kFormatSize, &written) == kOk &&
                      written == kFormatSize && std::memcmp(back, fmt, kFormatSize) == 0,
                  "GetFormat reads the primary's format back");
    }
    // A descriptor of the x64 size is refused.
    {
        TestDesc wrong = MakeDesc(kCapsPrimary, 0, 0);
        const std::uint32_t forty = 40;
        std::memcpy(wrong.bytes, &forty, 4);
        std::uint32_t w = 0;
        SDL_CHECK(Method<MCreate>(device, 0x0C)(device, wrong.bytes, reinterpret_cast<void**>(&w), nullptr) == kInvalidParam && w == 0,
                  "a 40-byte descriptor accepted");
    }

    // A 16-bit mono 22,050 Hz buffer of 400 frames holding a ramp s[i] = 10 i.
    static unsigned char fmt_mono[kFormatSize];
    WriteFormat(Format{1, 1, 2, 16, 0, 22050, 44100}, fmt_mono);
    const TestDesc sdesc = MakeDesc(0x180E2, 800, Low32(fmt_mono));
    std::uint32_t words[2] = {0, 0x12345678u};
    SDL_CHECK(Method<MCreate>(device, 0x0C)(device, sdesc.bytes, reinterpret_cast<void**>(&words[0]), nullptr) == kOk && words[1] == 0x12345678u,
              "a secondary buffer, 4 bytes into its slot");
    void* const ramp = FromSlot(words[0]);
    SDL_CHECK(Self(ramp)->size == 800 && Self(ramp)->format.block == 2 && Self(ramp)->refs == 1, "the secondary's shape");
    std::int16_t src[400];
    for (int i = 0; i < 400; ++i) src[i] = static_cast<std::int16_t>(10 * i);
    {
        void* first = nullptr;
        void* second = reinterpret_cast<void*>(1);
        std::uint32_t n1 = 0, n2 = 7;
        SDL_CHECK(Method<MLock>(ramp, 0x2C)(ramp, 0, 800, &first, &n1, &second, &n2, 0) == kOk && first == Self(ramp)->data && n1 == 800 &&
                      second == nullptr && n2 == 0,
                  "Lock(0, 800): one region");
        SDL_CHECK(Method<MUnlock>(ramp, 0x4C)(ramp, first, n1, second, n2) == kOk, "Unlock");
        SDL_CHECK(Method<MLock>(ramp, 0x2C)(ramp, 600, 400, &first, &n1, &second, &n2, 0) == kOk && first == Self(ramp)->data + 600 &&
                      n1 == 200 && second == Self(ramp)->data && n2 == 200,
                  "Lock(600, 400): two regions around the wrap");
        SDL_CHECK(Method<MUnlock>(ramp, 0x4C)(ramp, first, n1, second, n2) == kOk, "Unlock of two regions");
        SDL_CHECK(Method<MLock>(ramp, 0x2C)(ramp, 0, 0, &first, &n1, &second, &n2, kLockEntireBuffer) == kOk && n1 == 800, "DSBLOCK_ENTIREBUFFER");
        SDL_CHECK(Method<MUnlock>(ramp, 0x4C)(ramp, first, n1, second, n2) == kOk, "Unlock of the whole");
        SDL_CHECK(Method<MLock>(ramp, 0x2C)(ramp, 800, 4, &first, &n1, &second, &n2, 0) == kInvalidParam, "Lock past the end accepted");
    }
    Write(ramp, 0, src, 800);
    std::uint32_t status = 99, play = 99, write = 99, freq = 0;
    SDL_CHECK(Method<MStatus>(ramp, 0x24)(ramp, &status) == kOk && status == 0, "status before Play");
    SDL_CHECK(Method<MValue>(ramp, 0x34)(ramp, 0) == kOk && Method<MValue>(ramp, 0x44)(ramp, 22050) == kOk, "SetCurrentPosition / SetFrequency");
    SDL_CHECK(Method<MStatus>(ramp, 0x20)(ramp, &freq) == kOk && freq == 22050, "GetFrequency %u", freq);
    SDL_CHECK(Method<MPlay>(ramp, 0x30)(ramp, 0, 0, 0) == kOk, "Play once");
    SDL_CHECK(Method<MStatus>(ramp, 0x24)(ramp, &status) == kOk && status == kStatusPlaying, "status after Play: 0x%X", status);
    Pull(out, 200);   // 100 source frames consumed
    for (unsigned k = 0; k < 200; ++k)
        SDL_CHECK(out[2 * k] == static_cast<int>(5 * k) && out[2 * k + 1] == static_cast<int>(5 * k), "out[%u] = %d / %d, not %u", k,
                  out[2 * k], out[2 * k + 1], 5 * k);
    SDL_CHECK(Method<MPosition>(ramp, 0x10)(ramp, &play, &write) == kOk && play == 200, "the cursor after 200 output frames: %u", play);
    SDL_CHECK(write == (200 + (22050 * kWriteLeadMs / 1000) * 2) % 800, "the write cursor %u", write);
    Pull(out, 700);   // past the end at output frame 600 of this pull
    SDL_CHECK(Method<MStatus>(ramp, 0x24)(ramp, &status) == kOk && status == 0, "a once-only buffer stops at its end");
    SDL_CHECK(Method<MPosition>(ramp, 0x10)(ramp, &play, &write) == kOk && play == 0 && write == 0, "the cursor at 0 after the end: %u", play);
    // The last frame interpolates toward itself (no next frame to lean on): s[399] = 3,990.
    SDL_CHECK(out[2 * 599] == 3990 && out[2 * 600] == 0 && out[2 * 699] == 0, "the end (%d) and the silence after it (%d, %d)", out[2 * 599],
              out[2 * 600], out[2 * 699]);

    // Looping: 1,000 output frames = 500 source frames = one pass and a quarter.
    SDL_CHECK(Method<MPlay>(ramp, 0x30)(ramp, 0, 0, kPlayLooping) == kOk, "Play looping");
    SDL_CHECK(Method<MStatus>(ramp, 0x24)(ramp, &status) == kOk && status == (kStatusPlaying | kStatusLooping), "status looping 0x%X", status);
    Pull(out, 1000);
    SDL_CHECK(Method<MPosition>(ramp, 0x10)(ramp, &play, nullptr) == kOk && play == 200, "the cursor after a wrap: %u", play);
    SDL_CHECK(out[2 * 800] == 0 && out[2 * 801] == 5 && out[2 * 802] == 10, "the wrap starts the ramp again (%d, %d, %d)", out[2 * 800],
              out[2 * 801], out[2 * 802]);
    // Stop keeps the cursor; Play goes on from it.
    SDL_CHECK(Method<MCall>(ramp, 0x48)(ramp) == kOk, "Stop");
    SDL_CHECK(Method<MPosition>(ramp, 0x10)(ramp, &play, &write) == kOk && play == 200 && write == 200, "Stop keeps the cursor (%u, %u)", play, write);
    Pull(out, 10);
    SDL_CHECK(out[0] == 0 && Method<MPosition>(ramp, 0x10)(ramp, &play, nullptr) == kOk && play == 200, "a stopped buffer is silent and still");
    SDL_CHECK(Method<MPlay>(ramp, 0x30)(ramp, 0, 0, kPlayLooping) == kOk, "Play again");
    Pull(out, 2);
    SDL_CHECK(out[0] == 1000 && out[2] == 1005, "Play resumes from the cursor (%d, %d)", out[0], out[2]);

    // The volume law and the pan.
    long got = 1;
    SDL_CHECK(Method<MLong>(ramp, 0x3C)(ramp, 1) == kInvalidParam && Method<MLong>(ramp, 0x3C)(ramp, -10001) == kInvalidParam, "a level outside -10,000..0 accepted");
    SDL_CHECK(Method<MLong>(ramp, 0x3C)(ramp, -600) == kOk && Method<MGetLong>(ramp, 0x18)(ramp, &got) == kOk && got == -600, "SetVolume / GetVolume");
    SDL_CHECK(Method<MValue>(ramp, 0x34)(ramp, 200) == kOk, "SetCurrentPosition(200)");
    Pull(out, 2);
    {
        const int want = static_cast<int>(std::lrint(1000 * std::pow(10.0, -0.3)));   // 501
        SDL_CHECK(out[0] == want && out[1] == want, "-600 hundredths: %d, not %d", out[0], want);
    }
    SDL_CHECK(Method<MLong>(ramp, 0x3C)(ramp, kVolumeMin) == kOk, "SetVolume(-10000)");
    Pull(out, 2);
    SDL_CHECK(out[0] == 0 && out[1] == 0, "-10,000 is silence (%d)", out[0]);
    SDL_CHECK(Method<MLong>(ramp, 0x3C)(ramp, 0) == kOk, "SetVolume(0)");
    SDL_CHECK(Method<MLong>(ramp, 0x40)(ramp, 10001) == kInvalidParam, "a pan past 10,000 accepted");
    SDL_CHECK(Method<MLong>(ramp, 0x40)(ramp, kPanLeft) == kOk && Method<MGetLong>(ramp, 0x1C)(ramp, &got) == kOk && got == kPanLeft, "SetPan / GetPan");
    SDL_CHECK(Method<MValue>(ramp, 0x34)(ramp, 200) == kOk, "SetCurrentPosition(200)");
    Pull(out, 2);
    SDL_CHECK(out[0] == 1000 && out[1] == 0, "pan hard left: %d / %d", out[0], out[1]);
    SDL_CHECK(Method<MLong>(ramp, 0x40)(ramp, 2000) == kOk && Method<MValue>(ramp, 0x34)(ramp, 200) == kOk, "SetPan(2000)");
    Pull(out, 2);
    SDL_CHECK(out[0] == 100 && out[1] == 1000, "pan 2,000 (-20 dB on the left): %d / %d", out[0], out[1]);
    SDL_CHECK(Method<MLong>(ramp, 0x40)(ramp, 0) == kOk, "SetPan(0)");
    // The frequency: at 44,100 the ramp passes one to one; 0 is the format's.
    SDL_CHECK(Method<MValue>(ramp, 0x44)(ramp, 44100) == kOk && Method<MValue>(ramp, 0x34)(ramp, 0) == kOk, "SetFrequency(44100)");
    Pull(out, 4);
    SDL_CHECK(out[0] == 0 && out[2] == 10 && out[4] == 20 && out[6] == 30, "44,100 Hz passes the ramp (%d %d %d %d)", out[0], out[2], out[4], out[6]);
    SDL_CHECK(Method<MValue>(ramp, 0x44)(ramp, 0) == kOk && Method<MStatus>(ramp, 0x20)(ramp, &freq) == kOk && freq == 22050, "SetFrequency(0) is the format's: %u", freq);
    SDL_CHECK(Method<MValue>(ramp, 0x44)(ramp, 50) == kInvalidParam, "a frequency below 100 accepted");
    SDL_CHECK(Method<MValue>(ramp, 0x34)(ramp, 800) == kInvalidParam, "a position at the end accepted");
    SDL_CHECK(Method<MCall>(ramp, 0x50)(ramp) == kOk, "Restore");
    SDL_CHECK(Method<MCall>(ramp, 0x48)(ramp) == kOk, "Stop");

    // A ring with two notifications, the music's shape: 16-bit stereo 44,100,
    // 1,000 frames (4,000 bytes), events at 0 and 2,000.
    static unsigned char fmt_ring[kFormatSize];
    WriteFormat(Format{1, 2, 4, 16, 0, 44100, 176400}, fmt_ring);
    const TestDesc rdesc = MakeDesc(0x181E0, 4000, Low32(fmt_ring));
    std::uint32_t ring_word = 0;
    SDL_CHECK(Method<MCreate>(device, 0x0C)(device, rdesc.bytes, reinterpret_cast<void**>(&ring_word), nullptr) == kOk, "the ring");
    void* const ring = FromSlot(ring_word);
    std::uint32_t notify_word = 0;
    SDL_CHECK(Method<MQuery>(ring, 0x00)(ring, kIidNotify, reinterpret_cast<void**>(&notify_word)) == kOk && notify_word != 0 &&
                  notify_word != ring_word && Self(ring)->refs == 2,
              "QueryInterface(IID_IDirectSoundNotify)");
    {
        const unsigned char other[16] = {1, 2, 3};
        std::uint32_t w = 7;
        SDL_CHECK(Method<MQuery>(ring, 0x00)(ring, other, reinterpret_cast<void**>(&w)) == kNoInterface && w == 0, "an unknown IID answered");
    }
    void* const notify = FromSlot(notify_word);
    HANDLE e0 = CreateEventA(nullptr, FALSE, FALSE, nullptr), e1 = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    SDL_CHECK(e0 && e1, "CreateEventA");
    const Position positions[2] = {{0, e0}, {2000, e1}};
    SDL_CHECK(Method<MPositions>(notify, 0x0C)(notify, 2, positions) == kOk && Self(ring)->notify_count == 2, "SetNotificationPositions");
    auto fired = [](HANDLE e) { return WaitForSingleObject(e, 0) == WAIT_OBJECT_0; };
    SDL_CHECK(!fired(e0) && !fired(e1), "an event fired before Play");
    SDL_CHECK(Method<MValue>(ring, 0x34)(ring, 0) == kOk && Method<MPlay>(ring, 0x30)(ring, 0, 0, kPlayLooping) == kOk, "the ring plays");
    SDL_CHECK(fired(e0) && !fired(e1), "Play at 0 fires the offset-0 event and not the half's");
    SDL_CHECK(Method<MPositions>(notify, 0x0C)(notify, 2, positions) == kInvalidCall, "SetNotificationPositions while playing accepted");
    Pull(out, 400);   // 1,600 bytes: short of the half
    SDL_CHECK(!fired(e0) && !fired(e1), "an event fired before the half");
    Pull(out, 200);   // 2,400 bytes: past the half
    SDL_CHECK(!fired(e0) && fired(e1), "the half's event alone fires as the cursor passes 2,000");
    Pull(out, 500);   // 4,400 bytes: wrapped to 400
    SDL_CHECK(fired(e0) && !fired(e1), "the wrap fires the offset-0 event alone");
    SDL_CHECK(Method<MPosition>(ring, 0x10)(ring, &play, nullptr) == kOk && play == 400, "the ring's cursor after the wrap: %u", play);
    SDL_CHECK(Method<MCall>(ring, 0x48)(ring) == kOk, "the ring stopped");
    // Release order as Music_Release: the buffer, then the notify facet.
    SDL_CHECK(Method<MCall>(ring, 0x08)(ring) == 1 && Self(ring)->live, "the ring's first Release leaves the facet's reference");
    SDL_CHECK(Method<MCall>(notify, 0x08)(notify) == 0 && !Self(ring)->live && Self(ring)->data == nullptr, "the facet's Release frees the ring");
    CloseHandle(e0);
    CloseHandle(e1);

    // Two full-scale buffers clip; an 8-bit source is centred at 0x80.
    static unsigned char fmt_8[kFormatSize];
    WriteFormat(Format{1, 1, 1, 8, 0, 44100, 44100}, fmt_8);
    const TestDesc d8 = MakeDesc(0x180E2, 4, Low32(fmt_8));
    std::uint32_t w8 = 0;
    SDL_CHECK(Method<MCreate>(device, 0x0C)(device, d8.bytes, reinterpret_cast<void**>(&w8), nullptr) == kOk, "an 8-bit buffer");
    void* const eight = FromSlot(w8);
    const unsigned char eight_src[4] = {0x80, 0xFF, 0x00, 0x80};
    Write(eight, 0, eight_src, 4);
    SDL_CHECK(Method<MPlay>(eight, 0x30)(eight, 0, 0, 0) == kOk, "the 8-bit buffer plays");
    Pull(out, 4);
    SDL_CHECK(out[0] == 0 && out[2] == 0x7F00 && out[4] == -0x8000 && out[6] == 0, "8-bit samples: %d %d %d %d", out[0], out[2], out[4], out[6]);
    {
        std::int16_t loud[4] = {32767, 32767, -32768, -32768};
        const TestDesc dl = MakeDesc(0x180E2, 8, Low32(fmt_mono));
        std::uint32_t wa = 0, wb = 0;
        SDL_CHECK(Method<MCreate>(device, 0x0C)(device, dl.bytes, reinterpret_cast<void**>(&wa), nullptr) == kOk &&
                      Method<MCreate>(device, 0x0C)(device, dl.bytes, reinterpret_cast<void**>(&wb), nullptr) == kOk,
                  "two loud buffers");
        void* const a = FromSlot(wa);
        void* const b = FromSlot(wb);
        Write(a, 0, loud, 8);
        Write(b, 0, loud, 8);
        SDL_CHECK(Method<MValue>(a, 0x44)(a, 44100) == kOk && Method<MValue>(b, 0x44)(b, 44100) == kOk, "at 44,100");
        SDL_CHECK(Method<MPlay>(a, 0x30)(a, 0, 0, 0) == kOk && Method<MPlay>(b, 0x30)(b, 0, 0, 0) == kOk, "both play");
        Pull(out, 4);
        SDL_CHECK(out[0] == 32767 && out[2] == 32767 && out[4] == -32768 && out[6] == -32768, "the sum clips (%d %d %d %d)", out[0], out[2], out[4], out[6]);
        SDL_CHECK(Method<MCall>(a, 0x08)(a) == 0 && Method<MCall>(b, 0x08)(b) == 0, "the loud buffers released");
    }
    SDL_CHECK(Method<MCall>(eight, 0x08)(eight) == 0, "the 8-bit buffer released");
    SDL_CHECK(Method<MCall>(ramp, 0x08)(ramp) == 0, "the ramp released");
    SDL_CHECK(Method<MCall>(primary, 0x08)(primary) == 0, "the primary released");
    SDL_CHECK(Method<MCall>(device, 0x08)(device) == 0, "the device released");
    SDL_CHECK(g_live == live_before && g_device.refs == 0 && g_stream == nullptr, "not everything was put back (%u live)", g_live);

    // The SDL3 device itself, once: opened as Create opens it, closed as Release
    // does - in bof3x.exe only. Hosted, this runs inside DllMain under the
    // loader lock, where SDL's audio start (a device thread, COM) deadlocked
    // the 2026-11-01 run; in play Create is called from Snd_Init on the game's
    // thread, never under that lock.
    if (!IsExeHost()) {
        Log("sound_sdl   self-test: the objects and the mixer as described above, 0 mismatches; the SDL3 device is not "
            "opened under DllMain's loader lock (bof3x.exe's self-test opens it)");
        g_frames_out = 0;
        g_made = 0;
        g_live_max = 0;
        return;
    }
    const bool opened = OpenStream(true);
    if (opened) {
        const SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(g_stream);
        SDL_AudioSpec have{};
        int sample_frames = 0;
        SDL_GetAudioDeviceFormat(dev, &have, &sample_frames);
        const char* name = SDL_GetAudioDeviceName(dev);
        CloseStream();
        Log("sound_sdl   self-test: the objects and the mixer as described above, 0 mismatches; the SDL3 playback device "
            "opened and closed (%s, %d Hz, %d channel(s), %d frames a buffer)",
            name ? name : "(unnamed)", have.freq, have.channels, sample_frames);
    } else {
        Log("sound_sdl   self-test: the objects and the mixer as described above, 0 mismatches; no SDL3 playback device on "
            "this machine (%s): in play the game would run silent, as without DirectSound", SDL_GetError());
    }
    g_frames_out = 0;
    g_made = 0;
    g_live_max = 0;
}

}  // namespace

// --- The start-up -------------------------------------------------------------------

bool SoundSdl_On() { return g_on; }

bool SoundSdl_IsCreate(const void* fn) { return fn == reinterpret_cast<const void*>(&Create); }

void SoundSdl_Start() {
    if (!g_lock_made) {
        InitializeCriticalSection(&g_lock);
        g_lock_made = true;
        FillVtables();
    }
    if (g_patched) Fatal("sound_sdl: SoundSdl_Start twice");
    g_patched = true;
    static_assert(sizeof(Position) == 2 * sizeof(void*), "DSBPOSITIONNOTIFY's shape, the game's Position struct");
    static_assert(offsetof(Buffer, vtable) == 0, "the vtable pointer first");

    const char* selftest_only = std::getenv("BOF3X_SELFTEST_ONLY");
    if (IsExeHost() ? selftest_only && selftest_only[0] : WantsShadow("sound_sdl")) SelfTest();

    // The slot: 0x5C4014 holds DSOUND.dll's DirectSoundCreate (hosted, the
    // loader's; in bof3x.exe, BindSlots'). The switch is a PatchBytes of
    // that word to our Create - the code name SoundSdl, which BOF3X_OFF and,
    // hosted, BOF3X_ORIGINAL turn off, leaving Windows' DirectSound in the
    // slot for the owner's A/B listen. `expected` is the word as it stands:
    // the function differs per machine and per host.
    constexpr std::uint32_t kSlotVa = Imp_DirectSoundCreate_addr;
    unsigned char now[4], ours[4];
    std::memcpy(now, Widen<const void>(Img(kSlotVa)), 4);
    const std::uint32_t create = Low32(&Create);
    std::memcpy(ours, &create, 4);
    std::uint32_t before;
    std::memcpy(&before, now, 4);
    if (before == 0) Fatal("sound_sdl: the import slot 0x%08X is empty: SoundSdl_Start before it is bound", (unsigned)kSlotVa);
    g_on = PatchBytes("SoundSdl", kSlotVa, now, ours, 4);
    if (!g_on && sizeof(void*) == 8) {
        // The 64-bit build has no DirectSound path (the header comment): the
        // switch is ignored, said once, and the slot bound all the same.
        Log("DIV-0201    SoundSdl: off was asked for, but the 64-bit build has no DirectSound path; the switch is ignored and the "
            "sound output is SDL3's");
        void* const slot = Widen<void>(Img(kSlotVa));
        DWORD old = 0;
        if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) Fatal("sound_sdl: VirtualProtect(0x%08X) failed, error %lu", (unsigned)kSlotVa, GetLastError());
        std::memcpy(slot, ours, 4);
        VirtualProtect(slot, 4, old, &old);
        g_on = true;
    }
    std::uint32_t after;
    std::memcpy(&after, Widen<const void>(Img(kSlotVa)), 4);
    if (g_on && after != create) Fatal("sound_sdl: the import slot 0x%08X reads 0x%08X after the patch, not our Create 0x%08X", (unsigned)kSlotVa, after, create);
    if (!g_on && after != before) Fatal("sound_sdl: the import slot 0x%08X changed under a switch that is off", (unsigned)kSlotVa);
    Log("DIV-0201    sound output: %s (the DirectSoundCreate slot 0x%08X holds 0x%08X; SDL %d.%d.%d)",
        g_on ? "SDL3, objects of ours with DirectSound-shaped vtables and one device stream at 44,100 Hz 16-bit stereo"
             : "Windows' DirectSound (SoundSdl off)",
        (unsigned)kSlotVa, after, SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
        SDL_VERSIONNUM_MICRO(SDL_GetVersion()));
}

}  // namespace bof3

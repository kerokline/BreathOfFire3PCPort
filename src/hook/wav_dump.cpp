// BOF3X_WAV_DIR: the game's audio mix to a WAV file (wav_dump.h,
// docs/wav-dump.md).
//
// How the game's sound reaches the device (docs/sound.md section 1): every
// sound effect voice is a static DirectSound secondary buffer made from a
// RIFF WAVE (SndBuf_CreateFromWave 0x5A69E0, flags 0x180E2), played from 0 at
// a cue's frequency (SndBuf_Play 0x5A6BB0); the music is one 0x24000-byte
// 44,100 Hz 16-bit stereo ring (Music_CreateBuffer 0x5A6E60, flags 0x181E0,
// CTRLPOSITIONNOTIFY among them) refilled half by half as DirectSound's
// position notifications fire (Music_Pump 0x5A7230); the SND stream (a
// jingle) is another static buffer (SndStream_Play 0x5A7140). DirectSound -
// Windows, not the game - sums them. There is no buffer of the final mix in
// the process to copy, so the dump watches the hand-over and sums the
// buffers itself:
//
// - the DirectSoundCreate import slot 0x5C4014 is pointed at a wrapper (both
//   hosts: the loader binds it hosted before our DllMain - the launcher's
//   remote LoadLibraryW runs after the process initialises - and BindSlots
//   does in the exe host), which puts hooks on the device's vtable
//   (CreateSoundBuffer, DuplicateSoundBuffer) and on each secondary buffer's
//   (Release, Lock, Unlock, Play, Stop, SetCurrentPosition, SetVolume,
//   SetPan, SetFrequency). Every hook calls the real method first and
//   changes the dump's model only when it succeeded, so DirectSound and the
//   game see exactly what they saw without the dump. Capcom's code and ours
//   both reach DirectSound only through those vtables (sound_callees.h), so
//   BOF3X_ORIGINAL sides are dumped the same way.
// - a static buffer is modelled as DirectSound plays it: a copy of its bytes
//   (written at Unlock, at the offsets Lock gave), a play cursor, looping or
//   not, its frequency, volume and pan.
// - a buffer made with CTRLPOSITIONNOTIFY (the music's ring) is a stream: its
//   bytes are queued in the order the game wrote them and played from the
//   queue, at the logic frames' pace and never skipped. The game refills
//   the ring when DirectSound's notifications fire - real time - and a
//   frozen shot or a stall has the pump write two halves at once afterwards,
//   so a ring-shaped copy read at the frames' pace would read halves already
//   overwritten (the first cut dropped the queue back to a ring's length
//   when it grew past one and a half, and two runs of combat.txt then
//   differed from 19.5 s on: 24,084 samples dropped in one, none in the
//   other). Played from the queue, the music is the decoder's stream without
//   the real device's stalls, the same on every run; the queue only grows
//   (its lead is logged). It runs dry only if the frames outrun the device by
//   more than the ring holds (BOF3X_SPEED; counted and logged).
//
// The clock: each logic frame - WinMain's Frame_Counter 0x937F94 - is
// round(frames * rate * period) samples from the run's first frame, the
// period DIV-0047's 1001 / 30 ms (BOF3X_FRAME_MS when set) under our WinMain
// and the port's 33.334 ms under Capcom's (0x5C4218). A frame is mixed when
// the latch of a later frame runs (WavDump_Tick from DeviceLatch) or a hooked
// call arrives in a later frame - always before that call's change - so what
// a run's calls do lands on the frame they were made in, the same on every
// run of a recipe.
//
// What it is not: the device's output after Windows' own resampling to the
// endpoint's rate, nor the gaps and stale ring halves a stalled or frozen
// game makes the real device play (a recipe's frozen shots, a DIV-0034
// restart). The dump is the mix the game asked for, frame by frame.
#include "hook/wav_dump.h"

#include <windows.h>
#include <mmsystem.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace bof3 {
namespace {

constexpr unsigned kRate = 44100;
constexpr unsigned kChannels = 2;
// kCreateSlot: now Imp_DirectSoundCreate (symbols.toml). DSOUND.dll #1 (DirectSoundCreate), exe_host.cpp kSlots
constexpr double kCapcomFrameMs = 33.334;                // double at 0x5C4218, Capcom's WinMain
constexpr double kOurFrameMs = 1001.0 / 30.0;            // DIV-0047
constexpr unsigned kFlushFrames = 15;                    // header and data on disk every half second

// DirectSound, by vtable offset (dsound.h's order).
constexpr unsigned kRelease = 0x08;
constexpr unsigned kCreateSoundBuffer = 0x0C, kDuplicateSoundBuffer = 0x14;   // IDirectSound
constexpr unsigned kLock = 0x2C, kPlay = 0x30, kSetCurrentPosition = 0x34, kSetVolume = 0x3C, kSetPan = 0x40,
                   kSetFrequency = 0x44, kStop = 0x48, kUnlock = 0x4C;          // IDirectSoundBuffer
constexpr unsigned long kPrimaryBuffer = 0x01, kPositionNotify = 0x100;          // DSBCAPS_*
constexpr unsigned long kLockFromWriteCursor = 0x01, kLockEntireBuffer = 0x02;   // DSBLOCK_*
constexpr unsigned long kPlayLooping = 0x01;                                     // DSBPLAY_LOOPING

struct BufferDesc {   // DSBUFFERDESC
    unsigned long size, flags, bytes, reserved;
    const WAVEFORMATEX* format;
};

// --- The model --------------------------------------------------------------

struct Buffer {
    void* object = nullptr;
    bool stream = false;
    unsigned channels = 0, bits = 0, block = 0, rate = 0;
    unsigned long bytes = 0;
    std::vector<std::uint8_t> data;   // a static buffer's copy
    std::vector<std::uint8_t> queue;  // a stream's bytes in write order, from `head`
    std::size_t head = 0;
    double pos = 0.0;                 // a static buffer's cursor, in its frames; a stream's fraction
    bool playing = false, looping = false;
    unsigned long frequency = 0;      // 0: the format's
    long volume = 0, pan = 0;         // hundredths of a decibel
    // The last successful Lock: the offset its first region starts at.
    void* lock_first = nullptr;
    unsigned long lock_offset = 0;
};

CRITICAL_SECTION g_lock;
bool g_on = false;
std::vector<Buffer*> g_buffers;
std::wstring g_dir, g_label;
HANDLE g_file = INVALID_HANDLE_VALUE;
std::wstring g_path;
std::uint64_t g_data_bytes = 0;
std::vector<std::int16_t> g_pending;   // mixed, not yet written
std::vector<float> g_mix;
bool g_started = false;                // the first frame seen
std::uint32_t g_first_frame = 0, g_next_frame = 0;   // g_next_frame: the first frame not yet mixed
unsigned g_frames_since_flush = 0;
double g_frame_ms = kOurFrameMs;
std::uint64_t g_underrun_samples = 0, g_max_lead = 0;   // g_max_lead: the most samples a stream's queue held unplayed
bool g_dry = false;                    // the queue ran dry this frame (one log line a stretch)
unsigned g_created = 0, g_streams = 0, g_plays = 0;
constexpr unsigned kStatusFrames = 1800;               // a status line a minute

long (__stdcall* g_real_create)(const void*, void**, void*) = nullptr;
void* g_device_originals[2] = {};      // CreateSoundBuffer, DuplicateSoundBuffer
void** g_device_vtable = nullptr;

// Each buffer vtable seen, with its originals (one in practice; dsound.dll
// may use one class for static and streaming buffers or two).
struct VTable {
    void** vt;
    void* release; void* lock; void* play; void* set_position; void* set_volume; void* set_pan;
    void* set_frequency; void* stop; void* unlock;
};
std::vector<VTable> g_vtables;

void** VtableOf(void* object) {
    void** vt;
    std::memcpy(&vt, object, sizeof vt);
    return vt;
}

const VTable& Originals(void* object) {
    void** const vt = VtableOf(object);
    for (const VTable& v : g_vtables)
        if (v.vt == vt) return v;
    Fatal("wav: a DirectSound buffer call through vtable %p, which the dump never hooked", static_cast<void*>(vt));
}

Buffer* Find(void* object) {
    for (Buffer* b : g_buffers)
        if (b->object == object) return b;
    return nullptr;
}

double Gain(long hundredths) { return hundredths <= -10000 ? 0.0 : std::pow(10.0, static_cast<double>(hundredths) / 2000.0); }

// One sample of channel `c` of frame `i` of `bytes`, as a float in -1..1.
float Sample(const Buffer& b, const std::uint8_t* frames, std::size_t i, unsigned c) {
    const unsigned ch = b.channels == 1 ? 0 : c;
    const std::uint8_t* p = frames + i * b.block + ch * (b.bits / 8);
    if (b.bits == 8) return (static_cast<int>(*p) - 128) / 128.0f;
    std::int16_t v;
    std::memcpy(&v, p, 2);
    return v / 32768.0f;
}

// Adds `n` output samples of `b` to g_mix from index `at`, advancing it.
void MixBuffer(Buffer& b, std::size_t at, std::size_t n) {
    const double freq = b.frequency ? static_cast<double>(b.frequency) : static_cast<double>(b.rate);
    const double step = freq / kRate;
    const double gain = Gain(b.volume);
    const float gl = static_cast<float>(gain * (b.pan > 0 ? Gain(-b.pan) : 1.0));
    const float gr = static_cast<float>(gain * (b.pan < 0 ? Gain(b.pan) : 1.0));
    if (b.stream) {
        // From the queue: frame `head / block` + pos, linear between neighbours.
        for (std::size_t k = 0; k < n; ++k) {
            const std::size_t have = (b.queue.size() - b.head) / b.block;
            const std::size_t i = static_cast<std::size_t>(b.pos);
            if (i + 1 >= have) {   // the device would be playing what the game has not written: count it
                g_underrun_samples += n - k;
                if (!g_dry)
                    Log("wav         stream %p ran dry at Frame_Counter %u (%llu samples so far): silence until the "
                        "game writes more", b.object, static_cast<unsigned>(Frame_Counter),
                        static_cast<unsigned long long>(g_underrun_samples));
                g_dry = true;
                return;
            }
            g_dry = false;
            const float frac = static_cast<float>(b.pos - static_cast<double>(i));
            const std::uint8_t* const q = b.queue.data() + b.head;
            for (unsigned c = 0; c < kChannels; ++c) {
                const float s0 = Sample(b, q, i, c), s1 = Sample(b, q, i + 1, c);
                g_mix[(at + k) * kChannels + c] += (s0 + (s1 - s0) * frac) * (c == 0 ? gl : gr);
            }
            b.pos += step;
            const std::size_t whole = static_cast<std::size_t>(b.pos);
            b.head += whole * b.block;
            b.pos -= static_cast<double>(whole);
        }
        // Keep the queue small: drop what has been played once it is a ring long.
        if (b.head >= b.bytes) {
            b.queue.erase(b.queue.begin(), b.queue.begin() + static_cast<std::ptrdiff_t>(b.head));
            b.head = 0;
        }
        return;
    }
    const std::size_t frames = b.data.size() / b.block;
    if (frames == 0) {
        b.playing = false;
        return;
    }
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t i = static_cast<std::size_t>(b.pos);
        const float frac = static_cast<float>(b.pos - static_cast<double>(i));
        const std::size_t j = i + 1 < frames ? i + 1 : (b.looping ? 0 : i);
        for (unsigned c = 0; c < kChannels; ++c) {
            const float s0 = Sample(b, b.data.data(), i, c), s1 = Sample(b, b.data.data(), j, c);
            g_mix[(at + k) * kChannels + c] += (s0 + (s1 - s0) * frac) * (c == 0 ? gl : gr);
        }
        b.pos += step;
        if (b.pos >= static_cast<double>(frames)) {
            if (b.looping) {
                b.pos = std::fmod(b.pos, static_cast<double>(frames));
            } else {   // a one-shot plays to its end and stops there, its cursor at 0
                b.playing = false;
                b.pos = 0.0;
                return;
            }
        }
    }
}

void WriteAll(const void* p, DWORD n) {
    DWORD done = 0;
    if (!WriteFile(g_file, p, n, &done, nullptr) || done != n)
        Fatal("wav: writing %s failed, error %lu", Utf8Path(g_path.c_str()).text, GetLastError());
}

void PutDword(std::uint8_t* p, std::uint32_t v) { std::memcpy(p, &v, 4); }

// The 44-byte header for `data` bytes of 44,100 Hz 16-bit stereo PCM.
void Header(std::uint8_t (&h)[44], std::uint64_t data) {
    const std::uint32_t d = data > 0xFFFFFFF0ull - 36 ? 0xFFFFFFF0u - 36 : static_cast<std::uint32_t>(data);
    std::memcpy(h, "RIFF", 4);
    PutDword(h + 4, 36 + d);
    std::memcpy(h + 8, "WAVEfmt ", 8);
    PutDword(h + 16, 16);
    h[20] = 1; h[21] = 0;                                  // PCM
    h[22] = kChannels; h[23] = 0;
    PutDword(h + 24, kRate);
    PutDword(h + 28, kRate * kChannels * 2);
    h[32] = kChannels * 2; h[33] = 0;
    h[34] = 16; h[35] = 0;
    std::memcpy(h + 36, "data", 4);
    PutDword(h + 40, d);
}

// The next free <n>_ in the directory: one more than the largest number any
// .wav there starts with.
unsigned NextNumber() {
    unsigned next = 1;
    WIN32_FIND_DATAW fd;
    const HANDLE h = FindFirstFileW((g_dir + L"\\*.wav").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return next;
    do {
        unsigned v = 0;
        const wchar_t* p = fd.cFileName;
        if (*p < L'0' || *p > L'9') continue;
        while (*p >= L'0' && *p <= L'9') v = v * 10 + static_cast<unsigned>(*p++ - L'0');
        if (*p == L'_' && v >= next) next = v + 1;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return next;
}

void OpenFile() {
    CreateDirectoryW(g_dir.c_str(), nullptr);
    wchar_t number[16];
    std::swprintf(number, 16, L"%03u_", NextNumber());
    g_path = g_dir + L"\\" + number + g_label + L".wav";
    g_file = CreateFileW(g_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_file == INVALID_HANDLE_VALUE)
        Fatal("wav: cannot create %s, error %lu", Utf8Path(g_path.c_str()).text, GetLastError());
    std::uint8_t h[44];
    Header(h, 0);
    WriteAll(h, sizeof h);
    Log("wav         writing the mix to %s (44,100 Hz 16-bit stereo; a logic frame is %.4f ms, from Frame_Counter %u)",
        Utf8Path(g_path.c_str()).text, g_frame_ms, g_first_frame);
}

// What is mixed goes to the file, then the header is made to cover it.
void Flush() {
    if (g_file == INVALID_HANDLE_VALUE || g_pending.empty()) return;
    const DWORD n = static_cast<DWORD>(g_pending.size() * sizeof(std::int16_t));
    WriteAll(g_pending.data(), n);
    g_data_bytes += n;
    g_pending.clear();
    std::uint8_t h[44];
    Header(h, g_data_bytes);
    LARGE_INTEGER zero = {}, end = {};
    SetFilePointerEx(g_file, zero, &end, FILE_CURRENT);
    SetFilePointerEx(g_file, zero, nullptr, FILE_BEGIN);
    WriteAll(h, sizeof h);
    SetFilePointerEx(g_file, end, nullptr, FILE_BEGIN);
}

std::uint64_t SampleAt(std::uint32_t frame) {
    return static_cast<std::uint64_t>(std::llround(static_cast<double>(frame - g_first_frame) * kRate * g_frame_ms / 1000.0));
}

// Every frame before `now` mixed, with the model as it stands.
void Advance(std::uint32_t now) {
    if (!g_started) {
        g_started = true;
        g_first_frame = g_next_frame = now;
        OpenFile();
        return;
    }
    if (now < g_next_frame) {
        Log("wav         Frame_Counter went back from %u to %u: the dump goes on from there", g_next_frame, now);
        g_first_frame = now - static_cast<std::uint32_t>(
                                  std::llround(static_cast<double>(g_data_bytes / 4 + g_pending.size() / 2) * 1000.0 /
                                               (kRate * g_frame_ms)));
        g_next_frame = now;
        return;
    }
    for (; g_next_frame < now; ++g_next_frame) {
        const std::size_t n = static_cast<std::size_t>(SampleAt(g_next_frame + 1) - SampleAt(g_next_frame));
        g_mix.assign(n * kChannels, 0.0f);
        for (Buffer* b : g_buffers)
            if (b->playing) MixBuffer(*b, 0, n);
        for (float v : g_mix) {
            const float s = v * 32768.0f;
            g_pending.push_back(static_cast<std::int16_t>(s >= 32767.0f ? 32767 : s <= -32768.0f ? -32768 : std::lrint(s)));
        }
        if ((g_next_frame + 1 - g_first_frame) % kStatusFrames == 0)
            Log("wav         frame %u: %.1f s written; %u buffers made (%u streams), %u effect plays; %llu samples the "
                "music's queue ran dry; the queue's lead at most %llu samples",
                g_next_frame + 1, static_cast<double>(SampleAt(g_next_frame + 1)) / kRate, g_created, g_streams, g_plays,
                static_cast<unsigned long long>(g_underrun_samples), static_cast<unsigned long long>(g_max_lead));
        if (++g_frames_since_flush >= kFlushFrames) {
            g_frames_since_flush = 0;
            Flush();
        }
    }
}

void AdvanceNow() { Advance(Frame_Counter); }

struct Guard {
    Guard() { EnterCriticalSection(&g_lock); }
    ~Guard() { LeaveCriticalSection(&g_lock); }
};

// --- The hooks ---------------------------------------------------------------

template <class Fn>
Fn As(void* p) { return reinterpret_cast<Fn>(p); }

unsigned long __stdcall HookRelease(void* self) {
    const VTable& v = Originals(self);
    const unsigned long left = As<unsigned long (__stdcall*)(void*)>(v.release)(self);
    if (left == 0) {
        Guard g;
        AdvanceNow();
        for (std::size_t i = 0; i < g_buffers.size(); ++i)
            if (g_buffers[i]->object == self) {
                delete g_buffers[i];
                g_buffers.erase(g_buffers.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
    }
    return left;
}

long __stdcall HookLock(void* self, unsigned long offset, unsigned long bytes, void** first, unsigned long* first_bytes,
                        void** second, unsigned long* second_bytes, unsigned long flags) {
    const VTable& v = Originals(self);
    const long r = As<long (__stdcall*)(void*, unsigned long, unsigned long, void**, unsigned long*, void**, unsigned long*,
                                        unsigned long)>(v.lock)(self, offset, bytes, first, first_bytes, second, second_bytes, flags);
    Guard g;
    Buffer* const b = Find(self);
    if (r != 0 || !b) return r;
    if (flags & kLockFromWriteCursor)
        Fatal("wav: Lock from the write cursor on buffer %p is not modelled (the game never asked for it until now)", self);
    b->lock_first = first ? *first : nullptr;
    b->lock_offset = (flags & kLockEntireBuffer) ? 0 : offset;
    return r;
}

long __stdcall HookUnlock(void* self, void* first, unsigned long first_bytes, void* second, unsigned long second_bytes) {
    Guard g;
    Buffer* const b = Find(self);
    if (b) {
        AdvanceNow();
        if (first != b->lock_first)
            Fatal("wav: Unlock of buffer %p with %p, not the region its Lock gave (%p)", self, first, b->lock_first);
        const auto* p1 = static_cast<const std::uint8_t*>(first);
        const auto* p2 = static_cast<const std::uint8_t*>(second);
        if (b->stream) {
            if (p1) b->queue.insert(b->queue.end(), p1, p1 + first_bytes);
            if (p2) b->queue.insert(b->queue.end(), p2, p2 + second_bytes);
            const std::size_t lead = (b->queue.size() - b->head) / b->block;
            if (lead > g_max_lead) g_max_lead = lead;
        } else {
            if (p1 && first_bytes && b->lock_offset + first_bytes <= b->data.size())
                std::memcpy(b->data.data() + b->lock_offset, p1, first_bytes);
            if (p2 && second_bytes && second_bytes <= b->data.size()) std::memcpy(b->data.data(), p2, second_bytes);
        }
        b->lock_first = nullptr;
    }
    const VTable& v = Originals(self);
    return As<long (__stdcall*)(void*, void*, unsigned long, void*, unsigned long)>(v.unlock)(self, first, first_bytes, second,
                                                                                           second_bytes);
}

long __stdcall HookPlay(void* self, unsigned long reserved, unsigned long priority, unsigned long flags) {
    Guard g;
    AdvanceNow();
    const VTable& v = Originals(self);
    const long r = As<long (__stdcall*)(void*, unsigned long, unsigned long, unsigned long)>(v.play)(self, reserved, priority, flags);
    if (Buffer* const b = Find(self); b && r == 0) {
        if (!b->stream) ++g_plays;
        b->playing = true;
        b->looping = (flags & kPlayLooping) != 0;
    }
    return r;
}

long __stdcall HookStop(void* self) {
    Guard g;
    AdvanceNow();
    const VTable& v = Originals(self);
    const long r = As<long (__stdcall*)(void*)>(v.stop)(self);
    if (Buffer* const b = Find(self); b && r == 0) b->playing = false;
    return r;
}

long __stdcall HookSetPosition(void* self, unsigned long position) {
    Guard g;
    AdvanceNow();
    const VTable& v = Originals(self);
    const long r = As<long (__stdcall*)(void*, unsigned long)>(v.set_position)(self, position);
    if (Buffer* const b = Find(self); b && r == 0) {
        if (b->stream)
            Fatal("wav: SetCurrentPosition(%lu) on the stream %p is not modelled (the game never asked for it until now)",
                  position, self);
        b->pos = static_cast<double>(position / b->block);
    }
    return r;
}

long __stdcall HookSetVolume(void* self, long volume) {
    Guard g;
    AdvanceNow();
    const VTable& v = Originals(self);
    const long r = As<long (__stdcall*)(void*, long)>(v.set_volume)(self, volume);
    if (Buffer* const b = Find(self); b && r == 0) b->volume = volume;
    return r;
}

long __stdcall HookSetPan(void* self, long pan) {
    Guard g;
    AdvanceNow();
    const VTable& v = Originals(self);
    const long r = As<long (__stdcall*)(void*, long)>(v.set_pan)(self, pan);
    if (Buffer* const b = Find(self); b && r == 0) b->pan = pan;
    return r;
}

long __stdcall HookSetFrequency(void* self, unsigned long frequency) {
    Guard g;
    AdvanceNow();
    const VTable& v = Originals(self);
    const long r = As<long (__stdcall*)(void*, unsigned long)>(v.set_frequency)(self, frequency);
    if (Buffer* const b = Find(self); b && r == 0) b->frequency = frequency;
    return r;
}

void PatchSlot(void** slot, void* to) {
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old))
        Fatal("wav: VirtualProtect(%p) failed, error %lu", static_cast<void*>(slot), GetLastError());
    *slot = to;
    VirtualProtect(slot, sizeof(void*), old, &old);
}

void HookBufferVtable(void* object) {
    void** const vt = VtableOf(object);
    for (const VTable& v : g_vtables)
        if (v.vt == vt) return;
    VTable v{vt,
             vt[kRelease / 4], vt[kLock / 4], vt[kPlay / 4], vt[kSetCurrentPosition / 4], vt[kSetVolume / 4],
             vt[kSetPan / 4], vt[kSetFrequency / 4], vt[kStop / 4], vt[kUnlock / 4]};
    g_vtables.push_back(v);
    PatchSlot(vt + kRelease / 4, reinterpret_cast<void*>(&HookRelease));
    PatchSlot(vt + kLock / 4, reinterpret_cast<void*>(&HookLock));
    PatchSlot(vt + kPlay / 4, reinterpret_cast<void*>(&HookPlay));
    PatchSlot(vt + kSetCurrentPosition / 4, reinterpret_cast<void*>(&HookSetPosition));
    PatchSlot(vt + kSetVolume / 4, reinterpret_cast<void*>(&HookSetVolume));
    PatchSlot(vt + kSetPan / 4, reinterpret_cast<void*>(&HookSetPan));
    PatchSlot(vt + kSetFrequency / 4, reinterpret_cast<void*>(&HookSetFrequency));
    PatchSlot(vt + kStop / 4, reinterpret_cast<void*>(&HookStop));
    PatchSlot(vt + kUnlock / 4, reinterpret_cast<void*>(&HookUnlock));
    Log("wav         hooked the DirectSound buffer vtable %p (%u so far)", static_cast<void*>(vt),
        static_cast<unsigned>(g_vtables.size()));
}

long __stdcall HookCreateBuffer(void* device, const BufferDesc* desc, void** out, void* outer) {
    const long r = As<long (__stdcall*)(void*, const BufferDesc*, void**, void*)>(g_device_originals[0])(device, desc, out, outer);
    if (r != 0 || !out || !*out || !desc) return r;
    if (desc->flags & kPrimaryBuffer) {
        Log("wav         the primary buffer %p: not mixed from (DirectSound mixes the secondaries into it)", *out);
        return r;
    }
    Guard g;
    AdvanceNow();
    const WAVEFORMATEX* const f = desc->format;
    if (!f) Fatal("wav: a secondary buffer with no format");
    if (f->wFormatTag != WAVE_FORMAT_PCM || (f->wBitsPerSample != 8 && f->wBitsPerSample != 16) || f->nChannels < 1 ||
        f->nChannels > 2 || f->nBlockAlign != f->nChannels * f->wBitsPerSample / 8)
        Fatal("wav: a secondary buffer of format tag %u, %u channels, %u bits, block %u is not modelled", f->wFormatTag,
              f->nChannels, f->wBitsPerSample, f->nBlockAlign);
    auto* b = new Buffer;
    b->object = *out;
    b->stream = (desc->flags & kPositionNotify) != 0;
    b->channels = f->nChannels;
    b->bits = f->wBitsPerSample;
    b->block = f->nBlockAlign;
    b->rate = f->nSamplesPerSec;
    b->bytes = desc->bytes;
    if (!b->stream) b->data.assign(desc->bytes, b->bits == 8 ? 0x80 : 0);
    g_buffers.push_back(b);
    ++g_created;
    if (b->stream) {
        ++g_streams;
        Log("wav         stream %p: %lu bytes, %lu Hz %u-bit %u channel(s), flags 0x%lX (queued in write order)", *out,
            desc->bytes, static_cast<unsigned long>(f->nSamplesPerSec), b->bits, b->channels, desc->flags);
    }
    HookBufferVtable(*out);
    return r;
}

long __stdcall HookDuplicate(void* device, void* original, void** out) {
    const long r = As<long (__stdcall*)(void*, void*, void**)>(g_device_originals[1])(device, original, out);
    if (r == 0) Fatal("wav: DuplicateSoundBuffer is not modelled (the game never asked for it until now)");
    return r;
}

long __stdcall HookDirectSoundCreate(const void* guid, void** out, void* outer) {
    const long r = g_real_create(guid, out, outer);
    if (r != 0 || !out || !*out) {
        Log("wav         DirectSoundCreate failed (0x%08lX): the game plays silent and so does the dump", static_cast<unsigned long>(r));
        return r;
    }
    void** const vt = VtableOf(*out);
    if (g_device_vtable && g_device_vtable != vt) Fatal("wav: a second DirectSound device class (%p, then %p)", static_cast<void*>(g_device_vtable), static_cast<void*>(vt));
    if (!g_device_vtable) {
        g_device_vtable = vt;
        g_device_originals[0] = vt[kCreateSoundBuffer / 4];
        g_device_originals[1] = vt[kDuplicateSoundBuffer / 4];
        PatchSlot(vt + kCreateSoundBuffer / 4, reinterpret_cast<void*>(&HookCreateBuffer));
        PatchSlot(vt + kDuplicateSoundBuffer / 4, reinterpret_cast<void*>(&HookDuplicate));
    }
    Log("wav         DirectSoundCreate: device %p, vtable %p hooked", *out, static_cast<void*>(vt));
    return r;
}

// Label characters: letters, digits, - _ . ; anything else becomes _.
std::wstring Clean(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s)
        out += (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'-' || c == L'_' || c == L'.'
                   ? c : L'_';
    return out.empty() ? L"run" : out;
}

}  // namespace

bool WavDump_On() { return g_on; }

void WavDump_Start() {
    wchar_t dir[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_WAV_DIR", dir, MAX_PATH);
    if (n == 0) return;
    if (n >= MAX_PATH) Fatal("BOF3X_WAV_DIR: the path is %lu characters, over MAX_PATH", static_cast<unsigned long>(n));
    InitializeCriticalSection(&g_lock);
    g_dir = dir;
    while (!g_dir.empty() && (g_dir.back() == L'\\' || g_dir.back() == L'/')) g_dir.pop_back();
    wchar_t label[128];
    const DWORD ln = GetEnvironmentVariableW(L"BOF3X_WAV_LABEL", label, 128);
    if (ln > 0 && ln < 128) {
        g_label = Clean(label);
    } else {
        wchar_t recipe[MAX_PATH];
        const DWORD rn = GetEnvironmentVariableW(L"BOF3X_INPUT", recipe, MAX_PATH);
        std::wstring stem = L"play";
        if (rn > 0 && rn < MAX_PATH) {
            stem = recipe;
            const std::size_t slash = stem.find_last_of(L"\\/");
            if (slash != std::wstring::npos) stem = stem.substr(slash + 1);
            const std::size_t dot = stem.find_last_of(L'.');
            if (dot != std::wstring::npos) stem = stem.substr(0, dot);
        }
        g_label = Clean(stem);
    }
    // The pace the logic frames are clocked at: Capcom's WinMain's, or ours
    // (DIV-0047, and BOF3X_FRAME_MS as win_main.cpp reads it - it refuses a
    // bad value itself).
    if (!IsEnabled(addr::Game_WinMain)) {
        g_frame_ms = kCapcomFrameMs;
    } else {
        char text[32];
        const DWORD fn = GetEnvironmentVariableA("BOF3X_FRAME_MS", text, sizeof text);
        if (fn > 0 && fn < sizeof text) {
            char* end = nullptr;
            const double v = std::strtod(text, &end);
            if (end && *end == '\0' && v >= 1.0 && v <= 1000.0) g_frame_ms = v;
        }
    }
    char speed[8];
    if (GetEnvironmentVariableA("BOF3X_SPEED", speed, sizeof speed) > 0 && std::strcmp(speed, "1") != 0)
        Log("wav         BOF3X_SPEED=%s: the logic frames outrun the device, so the music's queue runs dry (counted as "
            "underrun samples); effects are mixed as ever", speed);

    void** const slot = reinterpret_cast<void**>(static_cast<std::uintptr_t>(Imp_DirectSoundCreate_addr));
    HMODULE dsound = GetModuleHandleA("dsound.dll");
    void* const real = dsound ? reinterpret_cast<void*>(GetProcAddress(dsound, MAKEINTRESOURCEA(1))) : nullptr;
    if (!real) Fatal("wav: DSOUND.dll is not loaded, or has no DirectSoundCreate (#1)");
    if (*slot != real)
        Fatal("wav: the import slot 0x%08X holds %p, not DSOUND.dll's DirectSoundCreate %p", static_cast<unsigned>(Imp_DirectSoundCreate_addr),
              *slot, real);
    g_real_create = reinterpret_cast<long (__stdcall*)(const void*, void**, void*)>(real);
    PatchSlot(slot, reinterpret_cast<void*>(&HookDirectSoundCreate));
    g_on = true;
    Log("wav         BOF3X_WAV_DIR %s, label %s: DirectSoundCreate's slot 0x%08X hooked; a logic frame is %.4f ms "
        "(%s WinMain)", Utf8Path(g_dir.c_str()).text, Utf8Path(g_label.c_str()).text, static_cast<unsigned>(Imp_DirectSoundCreate_addr),
        g_frame_ms, g_frame_ms == kCapcomFrameMs ? "Capcom's" : "our");
}

void WavDump_Tick() {
    if (!g_on) return;
    Guard g;
    AdvanceNow();
}

void WavDump_Stop() {
    if (!g_on) return;
    // ExitProcess has ended every other thread by now; one may have died
    // holding the lock, so it is only tried.
    if (!TryEnterCriticalSection(&g_lock)) {
        Log("wav         the lock is held at exit: the file stays as last flushed");
        return;
    }
    if (g_started) {
        AdvanceNow();
        Flush();
    }
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
        Log("wav         %s closed: %u frames, %llu samples (%.2f s); %u buffers made, %u of them streams; %llu samples "
            "the music's queue ran dry, its lead at most %llu; %u effect plays",
            Utf8Path(g_path.c_str()).text, g_next_frame - g_first_frame, static_cast<unsigned long long>(g_data_bytes / 4),
            static_cast<double>(g_data_bytes / 4) / kRate, g_created, g_streams,
            static_cast<unsigned long long>(g_underrun_samples), static_cast<unsigned long long>(g_max_lead), g_plays);
    }
    LeaveCriticalSection(&g_lock);
}

}  // namespace bof3

// BOF3X_SHADOW=sound: a differential fuzz of the 23 sound functions against
// byte-copies of Capcom's, once at start-up (docs/sound.md section 4). Every
// call out of a copy is re-aimed at a recording stand-in - the calls between
// the 23 included, so each function is tested alone - and so are the three
// Win32 imports, by moving the copies' absolute operands onto slots of our
// own. DirectSound is a set of fake COM objects (a device, six buffers, two
// notification objects) whose methods record their arguments and answer
// from a hash, DSERR_BUFFERLOST included; the MP3 decoder is three fake
// handles whose frame size the stand-ins set. No real audio, file, event or
// heap is touched: this runs before the game's C runtime is up.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/sound_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

extern "C" int __cdecl Sound_PlayEffectEax(unsigned short id) __asm__("_Sound_PlayEffect");
extern "C" int __cdecl Music_PlayEax(unsigned int track, int frames) __asm__("_Music_Play");
extern "C" int __cdecl Music_FadeOutStopEax(int frames) __asm__("_Music_FadeOutStop");
extern "C" int __cdecl Music_FadeInEax(int frames) __asm__("_Music_FadeIn");
extern "C" int __cdecl Music_FadeOutEax(int frames) __asm__("_Music_FadeOut");

namespace sound {
namespace {

unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t Dword(std::uint32_t address) {
    std::uint32_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void SetDword(std::uint32_t address, std::uint32_t v) { std::memcpy(At(address), &v, sizeof v); }

// --- The memory a round compares --------------------------------------------
// The channels, the six banks, the 0x460 bytes after them (read by a bank past
// 6) and the music's globals up to Music_FadeStep; the device block from
// Snd_BufferDesc to Music_Finished.
constexpr std::uint32_t kTablesAt = 0x6BC8C8, kTablesEnd = 0x6BDE64;
constexpr std::uint32_t kDeviceAt = 0x7DE378, kDeviceEnd = 0x7DE3E4;
constexpr unsigned kTables = kTablesEnd - kTablesAt, kDevice = kDeviceEnd - kDeviceAt;
constexpr unsigned kWave = 0x800, kDst = 0x280, kLockMem = 0x600, kHeap = 0x800, kLog = 40;

unsigned char g_wave[kWave];       // a wave, a file, a source to copy
unsigned char g_dst[kDst];         // Music_Decode's destination
unsigned char g_lock[kLockMem];    // what Lock hands out
unsigned char g_heap[kHeap];       // what Crt_malloc hands out
const unsigned char* g_out;        // Wave_FindData's out
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;
int g_decode_size;                 // the round's Music_Decode size, for the decoder stand-in's last frame
bool g_find_may_skip;              // Wave_FindData's stand-in may leave its out unwritten

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x632BE5ABu) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Bytes(const void* p, unsigned n) {
    std::uint32_t h = 2166136261u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 16777619u;
    return h;
}
std::uint32_t String(const char* s) { return Bytes(s, static_cast<unsigned>(std::strlen(s))); }

// --- DirectSound, faked -------------------------------------------------------
struct Fake { const void* const* vtable; std::uint32_t id; };
struct Frame { std::uint32_t a, b, bytes; };
struct Decoder { unsigned char head[0x14]; Frame* frame; };
Fake g_devices[2], g_buffers[6], g_notifies[2];
Decoder g_decoders[3];
Frame g_frames[3];

void* AnyBuffer(std::uint32_t h) { return &g_buffers[h % 6]; }

// A callee may change what its caller reads after it. Pointers the originals
// dereference without a test are only ever swapped for another fake.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h & 0x100) return;
    const std::uint32_t v = h >> 12;
    switch (h % 17) {
    case 0: Music_FileSize = static_cast<int>(v & 0x1FF); break;
    case 1: Music_FileLoops = static_cast<int>(v & 1); break;
    case 2: Music_File = At(v); break;
    case 3: Music_FadeCount = static_cast<int>(v % 3); break;
    case 4: Music_FadeStops = static_cast<int>(v & 1); break;
    case 5: SetDword(Address(&Music_Volume), h * 0x2545F491u); break;
    case 6: Music_Track = static_cast<unsigned char>(v); break;
    case 7: Music_LoadedTrack = static_cast<int>(v & 0xFF); break;
    case 8: Snd_Device = &g_devices[v & 1]; break;
    case 9: Music_Buffer = AnyBuffer(v); break;
    case 10: Music_Notify = &g_notifies[v & 1]; break;
    case 11: Music_Decoder = &g_decoders[v % 3]; break;
    case 12: Music_Loops = static_cast<int>(v & 1); break;
    case 13: Music_Finished = static_cast<int>(v & 1); break;
    case 14: Music_Events[v & 1] = At(h * 7); break;
    case 15: Sound_Channels[v % 23] = (v & 0x100) ? Address(AnyBuffer(v >> 9)) : 0; break;
    default: SetDword(Address(&Music_FadeStep), h * 0x9E3779B1u); break;
    }
}

std::uint32_t Id(const void* self) { return static_cast<const Fake*>(self)->id; }
long Answer(std::uint32_t h, bool lost) {
    switch (h % 10) {
    case 0: return lost ? kBufferLost : 0;
    case 1: return lost ? kBufferLost : static_cast<long>(0x80004005u);
    case 2: return static_cast<long>(0x88780078u);   // another DSERR
    default: return 0;
    }
}

long __stdcall FakeQuery(void* self, const void* iid, void** out) {
    Record(0x100, Id(self), Bytes(iid, 16));
    const std::uint32_t h = Hash();
    *out = &g_notifies[h & 1];
    Disturb();
    return Answer(h >> 4, false);
}
long __stdcall FakeAddRef(void* self) { Record(0x101, Id(self)); return 1; }
long __stdcall FakeRelease(void* self) { Record(0x102, Id(self)); Disturb(); return 0; }
long __stdcall FakeCreate(void* self, void* desc, void** out, void* outer) {
    const unsigned char* const d = static_cast<const unsigned char*>(desc);
    std::uint32_t format;
    std::memcpy(&format, d + 0x10, 4);
    Record(0x103, Id(self), Address(desc), Bytes(d, 0x24), format == Address(Snd_WaveFormat) ? Bytes(Snd_WaveFormat, 0x12) : 1);
    Record(0x103, Address(outer));
    const std::uint32_t h = Hash();
    const long r = Answer(h, false);
    if (r == 0 || h % 7 == 0) *out = AnyBuffer(h >> 8);   // a failing create may write it too
    Disturb();
    return r;
}
long __stdcall FakePositions(void* self, unsigned long n, const void* positions) {
    Record(0x104, Id(self), n, Bytes(positions, 16));
    Disturb();
    return Answer(Hash(), false);
}
long __stdcall FakeGetStatus(void* self, unsigned long* status) {
    Record(0x105, Id(self));
    const std::uint32_t h = Hash();
    *status = h % 3 == 0 ? (h >> 8) | 1 : h >> 8;
    Disturb();
    return 0;
}
long __stdcall FakeLock(void* self, unsigned long offset, unsigned long bytes, void** p1, unsigned long* n1, void** p2,
                        unsigned long* n2, unsigned long flags) {
    Record(0x106, Id(self), offset, bytes, flags);
    const std::uint32_t h = Hash();
    const long r = Answer(h, true);
    if (r == 0) {
        const unsigned long room = bytes > 0x200 ? 0x200 : bytes;
        const std::uint32_t c = (h >> 8) % 5;
        const unsigned long first = c < 2 ? room : c == 2 ? 0 : (h >> 11) % (room + 1);
        *p1 = g_lock + (h >> 16) % 0x100;
        *n1 = first;
        *p2 = g_lock + 0x300 + (h >> 20) % 0x100;
        *n2 = (h >> 24) % 3 == 0 ? (h >> 12) % 0x201 : room - first;
    }
    Disturb();
    return r;
}
long __stdcall FakeUnlock(void* self, void* p1, unsigned long n1, void* p2, unsigned long n2) {
    Record(0x107, Id(self), Address(p1) - Address(g_lock), n1, (Address(p2) - Address(g_lock)) ^ (n2 << 12));
    Disturb();
    return Answer(Hash(), false);
}
long __stdcall FakePlay(void* self, unsigned long a, unsigned long b, unsigned long flags) {
    Record(0x108, Id(self), a, b, flags);
    const std::uint32_t h = Hash();
    Disturb();
    return Answer(h, true);
}
long __stdcall FakeSetPosition(void* self, unsigned long v) { Record(0x109, Id(self), v); Disturb(); return Answer(Hash(), false); }
long __stdcall FakeSetVolume(void* self, unsigned long v) { Record(0x10A, Id(self), v); Disturb(); return Answer(Hash(), false); }
long __stdcall FakeSetFrequency(void* self, unsigned long v) { Record(0x10B, Id(self), v); Disturb(); return Answer(Hash(), false); }
long __stdcall FakeStop(void* self) { Record(0x10C, Id(self)); Disturb(); return 0; }
long __stdcall FakeRestore(void* self) { Record(0x10D, Id(self)); Disturb(); return Answer(Hash(), false); }
long __stdcall FakeUnexpected(void* self) {
    bof3::Fatal("sound: self-test reached a DirectSound method it does not fake (object %u)", (unsigned)Id(self));
}

const void* const kDeviceVtable[] = {reinterpret_cast<const void*>(&FakeQuery), reinterpret_cast<const void*>(&FakeAddRef),
                                     reinterpret_cast<const void*>(&FakeRelease), reinterpret_cast<const void*>(&FakeCreate),
                                     reinterpret_cast<const void*>(&FakeUnexpected)};
const void* const kNotifyVtable[] = {reinterpret_cast<const void*>(&FakeQuery), reinterpret_cast<const void*>(&FakeAddRef),
                                     reinterpret_cast<const void*>(&FakeRelease), reinterpret_cast<const void*>(&FakePositions)};
const void* kBufferVtable[21];

void BuildFakes() {
    for (auto& slot : kBufferVtable) slot = reinterpret_cast<const void*>(&FakeUnexpected);
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    kBufferVtable[kQueryInterface / 4] = f(&FakeQuery);
    kBufferVtable[1] = f(&FakeAddRef);
    kBufferVtable[kRelease / 4] = f(&FakeRelease);
    kBufferVtable[kGetStatus / 4] = f(&FakeGetStatus);
    kBufferVtable[kLock / 4] = f(&FakeLock);
    kBufferVtable[kPlay / 4] = f(&FakePlay);
    kBufferVtable[kSetCurrentPosition / 4] = f(&FakeSetPosition);
    kBufferVtable[kSetVolume / 4] = f(&FakeSetVolume);
    kBufferVtable[kSetFrequency / 4] = f(&FakeSetFrequency);
    kBufferVtable[kStop / 4] = f(&FakeStop);
    kBufferVtable[kUnlock / 4] = f(&FakeUnlock);
    kBufferVtable[kRestore / 4] = f(&FakeRestore);
    for (unsigned i = 0; i < 2; ++i) g_devices[i] = {kDeviceVtable, 0x10 + i};
    for (unsigned i = 0; i < 6; ++i) g_buffers[i] = {kBufferVtable, 0x20 + i};
    for (unsigned i = 0; i < 2; ++i) g_notifies[i] = {kNotifyVtable, 0x30 + i};
    for (unsigned i = 0; i < 3; ++i) {
        std::memset(&g_decoders[i], 0, sizeof g_decoders[i]);
        g_decoders[i].frame = &g_frames[i];
    }
}

// --- The stand-ins ------------------------------------------------------------
int __cdecl StubSprintf(char* dst, const char* fmt, ...) {
    unsigned n = 0;
    for (const char* p = fmt; *p; ++p)
        if (*p == '%') ++n;
    std::uint32_t a[2] = {};
    va_list args;
    va_start(args, fmt);
    for (unsigned i = 0; i < n && i < 2; ++i) a[i] = va_arg(args, std::uint32_t);
    va_end(args);
    Record(1, Address(fmt), n, a[0], a[1]);
    const std::uint32_t h = Hash() ^ a[0] ^ (a[1] * 3);
    static const char kHex[] = "0123456789ABCDEF";
    dst[0] = 'S';
    for (unsigned i = 0; i < 8; ++i) dst[1 + i] = kHex[(h >> (28 - 4 * i)) & 0xF];
    dst[9] = 0;
    Disturb();
    return 9;
}
void* __cdecl StubMalloc(unsigned n) {
    Record(2, n);
    Disturb();
    return g_heap + (Hash() % 0x400 & ~0xFu);
}
void __cdecl StubFree(void* p) { Record(3, Address(p)); Disturb(); }
int __cdecl StubFileOpen(const char* path, int a, int b) {
    Record(4, String(path), static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b));
    const std::uint32_t h = Hash();
    Disturb();
    return h % 3 == 0 ? -1 : static_cast<int>(h >> 28);
}
int __cdecl StubFileSize(int handle) {
    Record(5, static_cast<std::uint32_t>(handle));
    const std::uint32_t h = Hash();
    Disturb();
    return h % 4 ? static_cast<int>(h % 0x200) : static_cast<int>(h);
}
unsigned __cdecl StubFileRead(int handle, void* dst, unsigned n) {
    Record(6, static_cast<std::uint32_t>(handle), Address(dst), n);
    Disturb();
    return n;
}
int __cdecl StubFileClose(int handle) {   // File_Close is void; this leaves eax 0 behind it
    Record(7, static_cast<std::uint32_t>(handle));
    Disturb();
    return 0;
}
int __stdcall StubMp3Create(void** out) {
    Record(8);
    const std::uint32_t h = Hash();
    Disturb();
    if (h % 5 == 0) return static_cast<int>(h | 1);
    *out = &g_decoders[(h >> 8) % 3];
    return 0;
}
int __stdcall StubMp3MemoryIo(void* io) {
    Record(9);
    std::uint32_t h = Hash();
    for (unsigned i = 0; i < 0x2C; ++i) static_cast<unsigned char*>(io)[i] = static_cast<unsigned char>(h = h * 0x2C9277B5u + 1);
    return 0;
}
int __stdcall StubMp3SetIo(void* decoder, void* io) { Record(10, Address(decoder), Bytes(io, 0x2C)); Disturb(); return 0; }
int __stdcall StubMp3Open(void* decoder, const char* name, int mode) {
    Record(11, Address(decoder), String(name), static_cast<std::uint32_t>(mode));
    const std::uint32_t h = Hash();
    Disturb();
    return h % 5 == 0 ? static_cast<int>(h | 1) : 0;
}
int __stdcall StubMp3Start(void* decoder) {
    Record(12, Address(decoder));
    const std::uint32_t h = Hash();
    return h % 5 == 0 ? static_cast<int>(h | 1) : h % 7 == 0 ? -1 : 0;
}
// Decodes "a frame": a few bytes at out when out is inside g_dst, and every
// decoder's frame size set - at least 1, so a round always ends, and on the
// end of the stream no more than what is left of the round's size, so the
// zero fill after it stays inside.
int __stdcall StubMp3Decode(void* decoder, void* out) {
    const std::uint32_t filled = Address(out) - Address(g_dst);
    Record(13, Address(decoder), filled);
    const std::uint32_t h = Hash();
    Disturb();
    if (filled + 4 <= kDst) SetDword(Address(out), h * 0x01000193u);
    const std::uint32_t c = (h >> 8) % 20;
    int result = c < 12 ? 0 : c < 17 ? kEndOfStream : static_cast<int>(h | 0x80000000u);
    if (g_decode_size <= 0 && result == kEndOfStream) result = 0;
    std::uint32_t bytes = 1 + (h >> 12) % 0x40;
    if (result == kEndOfStream) {
        std::uint32_t left = static_cast<std::uint32_t>(g_decode_size) - filled;
        if (left == 0 || left > kDst) left = 1;   // only a broken caller gets here: keep the fuzz itself safe
        bytes = 1 + (h >> 12) % left;
    }
    for (Frame& f : g_frames) f.bytes = bytes;
    return result;
}
int __stdcall StubMp3Seek(void* decoder, int position) { Record(14, Address(decoder), static_cast<std::uint32_t>(position)); Disturb(); return 0; }
int __stdcall StubMp3Destroy(void* decoder) { Record(15, Address(decoder)); Disturb(); return 0; }
void* __stdcall StubCreateEvent(void* security, int manual, int initial, const char* name) {
    Record(16, Address(security), static_cast<std::uint32_t>(manual), static_cast<std::uint32_t>(initial), Address(name));
    const std::uint32_t h = Hash();
    Disturb();
    return At(h | 3);
}
int __stdcall StubCloseHandle(void* handle) { Record(17, Address(handle)); Disturb(); return 1; }
unsigned __stdcall StubMsgWait(unsigned count, const void* handles, int all, unsigned ms, unsigned mask) {
    Record(18, count, Address(handles), static_cast<std::uint32_t>(all), ms);
    Record(18, mask, Dword(Address(handles)), Dword(Address(handles) + 4));
    const std::uint32_t h = Hash();
    Disturb();
    static const unsigned kAnswers[] = {0, 1, 0, 1, 2, 0x102, 0xFFFFFFFFu, 0x80};
    return kAnswers[h % 8];
}
int __cdecl StubLoadFile(unsigned track) { Record(20, track); Disturb(); return static_cast<int>(Hash()); }
void __cdecl StubSetVolume(std::uint32_t bits) { Record(21, bits); Disturb(); }
void __cdecl StubStart(const void* file, unsigned size, int loops) {
    Record(22, Address(file), size, static_cast<std::uint32_t>(loops));
    Disturb();
}
int __cdecl StubFadeIn(int frames) { Record(23, static_cast<std::uint32_t>(frames)); Disturb(); return static_cast<int>(Hash()); }
void __cdecl StubPump() { Record(24); Disturb(); }
void __cdecl StubStop() { Record(25); Disturb(); }
unsigned __cdecl StubFindData(const unsigned char* wave, const unsigned char** data) {
    Record(26, Address(wave), data != nullptr);
    const std::uint32_t h = Hash();
    if (data && !(g_find_may_skip && h % 4 == 0)) *data = wave + (h >> 8) % 0x100;
    Disturb();
    return (h >> 16) % 0x201;
}
long __cdecl StubWrite(void* buffer, const void* src, unsigned offset, unsigned size) {
    Record(27, Address(buffer), Address(src), offset, size);
    Disturb();
    return static_cast<long>(Hash());
}
// Sound_PlayEffect's cue: its four words and their voice entries' buffers,
// which the two stand-ins under it may change - the original reads each word
// afresh, and the entry's buffer again after SndBuf_Play.
std::uint32_t g_cue, g_entries[4];
void DisturbCue() {
    const std::uint32_t h = Hash();
    if (!g_cue || h % 3 == 0) return;
    const std::uint32_t i = (h >> 4) & 3;
    if (h & 0x100) {
        if (g_entries[i]) SetDword(g_entries[i] + 4, Address(AnyBuffer(h >> 12)));
    } else {
        SetDword(g_cue + 4 * i, h * 0x2545F491u);
    }
}
void __cdecl StubPlay(std::uint32_t sample, void* buffer, unsigned frequency) {
    Record(28, sample, Address(buffer), frequency);
    DisturbCue();
    Disturb();
}
void __cdecl StubSndStop(void* buffer) { Record(29, Address(buffer)); DisturbCue(); Disturb(); }
void* __cdecl StubCreate(const unsigned char* wave) {
    Record(30, Address(wave));
    const std::uint32_t h = Hash();
    Disturb();
    return h % 4 == 0 ? nullptr : AnyBuffer(h >> 8);
}
void __cdecl StubRelease() { Record(31); Disturb(); }
void* __cdecl StubOpenDecoder(const void* file, unsigned size) {
    Record(32, Address(file), size);
    const std::uint32_t h = Hash();
    Disturb();
    return h % 4 == 0 ? nullptr : &g_decoders[(h >> 8) % 3];
}
void* __cdecl StubCreateBuffer() {
    Record(33);
    const std::uint32_t h = Hash();
    Disturb();
    return h % 4 == 0 ? nullptr : AnyBuffer(h >> 8);
}
long __cdecl StubSetNotify(void* buffer) { Record(34, Address(buffer)); Disturb(); return static_cast<long>(Hash()); }
void __cdecl StubDecode(unsigned char* dst, int size) { Record(35, Address(dst), static_cast<std::uint32_t>(size)); Disturb(); }

template <class To, class From>
To Cast(From f) { return reinterpret_cast<To>(reinterpret_cast<void*>(f)); }

const Callees kStubs = {
    StubSprintf, StubMalloc, StubFree, StubFileOpen, StubFileSize, StubFileRead, Cast<void (__cdecl*)(int)>(&StubFileClose),
    StubMp3Create, StubMp3MemoryIo, StubMp3SetIo, StubMp3Open, StubMp3Start, StubMp3Decode, StubMp3Seek, StubMp3Destroy,
    StubCreateEvent, StubCloseHandle, StubMsgWait,
    StubLoadFile, Cast<void (__cdecl*)(float)>(&StubSetVolume), StubStart, StubFadeIn, StubPump, StubStop, StubFindData,
    StubWrite, StubPlay, StubSndStop, StubCreate, StubRelease, StubOpenDecoder, StubCreateBuffer, StubSetNotify, StubDecode,
};

// The copies' import slots: the absolute operands of their Win32 calls moved here.
const void* g_slot_create_event = reinterpret_cast<const void*>(&StubCreateEvent);
const void* g_slot_close_handle = reinterpret_cast<const void*>(&StubCloseHandle);
const void* g_slot_msg_wait = reinterpret_cast<const void*>(&StubMsgWait);

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5B9380: return f(&StubSprintf);
    case 0x5B9660: return f(&StubMalloc);
    case 0x5B9577: return f(&StubFree);
    case 0x5A7380: return f(&StubFileOpen);
    case 0x5A74D0: return f(&StubFileSize);
    case 0x5A7470: return f(&StubFileRead);
    case 0x5A7510: return f(&StubFileClose);
    case 0x5ADF00: return f(&StubMp3Create);
    case 0x5B0D50: return f(&StubMp3MemoryIo);
    case 0x5AE160: return f(&StubMp3SetIo);
    case 0x5AE6A0: return f(&StubMp3Open);
    case 0x5AE820: return f(&StubMp3Start);
    case 0x5AFC40: return f(&StubMp3Decode);
    case 0x5AEBA0: return f(&StubMp3Seek);
    case 0x5B0630: return f(&StubMp3Destroy);
    case 0x587A20: return f(&StubLoadFile);
    case 0x5A6FB0: return f(&StubSetVolume);
    case 0x5A6CC0: return f(&StubStart);
    case 0x587BA0: return f(&StubFadeIn);
    case 0x5A7230: return f(&StubPump);
    case 0x5A7050: return f(&StubStop);
    case 0x5A6AA0: return f(&StubFindData);
    case 0x5A6AF0: return f(&StubWrite);
    case 0x5A6BB0: return f(&StubPlay);
    case 0x5A6C30: return f(&StubSndStop);
    case 0x5A69E0: return f(&StubCreate);
    case 0x5A70A0: return f(&StubRelease);
    case 0x5A6D60: return f(&StubOpenDecoder);
    case 0x5A6E60: return f(&StubCreateBuffer);
    case 0x5A6DF0: return f(&StubSetNotify);
    case 0x5A6F30: return f(&StubDecode);
    case 0x5B9550: return nullptr;   // the CRT's _ftol: pure x87, the copy keeps it
    default: bof3::Fatal("sound: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

// The copies and their calls out, by capstone 2026-09-22; every jump stays
// inside. Sizes are each body's extent to its last instruction.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
constexpr Call kPlayEffectCalls[] = {{0x7C, 0x5A6C30}, {0xAF, 0x5A6C30}, {0xD9, 0x5A6BB0}};
constexpr Call kLoadFileCalls[] = {{0x14, 0x5B9380}, {0x24, 0x5B9380}, {0x3C, 0x5A7380}, {0x54, 0x5A7380}, {0x77, 0x5B9577},
                                   {0x80, 0x5A74D0}, {0x8B, 0x5B9660}, {0x9E, 0x5A7470}, {0xA4, 0x5A7510}};
constexpr Call kPlayCalls[] = {{0x20, 0x587A20}, {0x34, 0x5A6FB0}, {0x4D, 0x5A6CC0}, {0x57, 0x587BA0}};
constexpr Call kTickCalls[] = {{0x0, 0x5A7230}, {0x26, 0x5A6FB0}, {0x44, 0x5A7050}};
constexpr Call kFromWaveCalls[] = {{0x11, 0x5A69E0}};
constexpr Call kCreateFromWaveCalls[] = {{0x3D, 0x5A6AA0}, {0xAC, 0x5A6AF0}};
constexpr Call kSndPlayCalls[] = {{0x50, 0x5A6AA0}, {0x5E, 0x5A6AF0}};
constexpr Call kStartCalls[] = {{0x16, 0x5A70A0}, {0x23, 0x5B9660}, {0x4B, 0x5A6D60}, {0x5C, 0x5A6E60}, {0x71, 0x5A6DF0}};
constexpr Call kOpenDecoderCalls[] = {{0x08, 0x5ADF00}, {0x1C, 0x5B0D50}, {0x2B, 0x5AE160}, {0x44, 0x5B9380}, {0x58, 0x5AE6A0},
                                      {0x6C, 0x5AE820}};
constexpr Call kCreateBufferCalls[] = {{0xA8, 0x5A6F30}, {0xBE, 0x5A6AF0}};
constexpr Call kDecodeCalls[] = {{0x1D, 0x5AFC40}, {0x43, 0x5AEBA0}};
constexpr Call kSetVolumeCalls[] = {{0x24, 0x5B9550}};
constexpr Call kReleaseCalls[] = {{0x45, 0x5B9577}, {0x8B, 0x5B0630}};
constexpr Call kPumpCalls[] = {{0x39, 0x5A7050}, {0x54, 0x5A6F30}, {0x7A, 0x5A6AF0}};

enum Fn : unsigned {
    kPlayEffect, kLoadFile, kPlay, kFadeOutStop, kFadeIn, kFadeOut, kTick, kFromWave, kCreateFromWave, kFindData, kWrite,
    kSndPlay, kSndStop, kSndRelease, kStart, kOpenDecoder, kSetNotify, kCreateBuffer, kDecode, kSetVolume, kStop, kRelease,
    kPump, kCount
};
const Clone kClones[kCount] = {
    {"Sound_PlayEffect", 0x587740, 0x119, kPlayEffectCalls, 3},
    {"Music_LoadFile", 0x587A20, 0xB8, kLoadFileCalls, 9},
    {"Music_Play", 0x587AE0, 0x60, kPlayCalls, 4},
    {"Music_FadeOutStop", 0x587B40, 0x38, nullptr, 0},
    {"Music_FadeIn", 0x587BA0, 0x36, nullptr, 0},
    {"Music_FadeOut", 0x587BE0, 0x32, nullptr, 0},
    {"Sound_Tick", 0x587C70, 0x51, kTickCalls, 3},
    {"SndBuf_FromWave", 0x5A69C0, 0x1A, kFromWaveCalls, 1},
    {"SndBuf_CreateFromWave", 0x5A69E0, 0xBC, kCreateFromWaveCalls, 2},
    {"Wave_FindData", 0x5A6AA0, 0x44, nullptr, 0},
    {"SndBuf_Write", 0x5A6AF0, 0xBB, nullptr, 0},
    {"SndBuf_Play", 0x5A6BB0, 0x75, kSndPlayCalls, 2},
    {"SndBuf_Stop", 0x5A6C30, 0x23, nullptr, 0},
    {"SndBuf_Release", 0x5A6C90, 0x29, nullptr, 0},
    {"Music_Start", 0x5A6CC0, 0x9F, kStartCalls, 5},
    {"Music_OpenDecoder", 0x5A6D60, 0x81, kOpenDecoderCalls, 6},
    {"Music_SetNotify", 0x5A6DF0, 0x70, nullptr, 0},
    {"Music_CreateBuffer", 0x5A6E60, 0xCC, kCreateBufferCalls, 2},
    {"Music_Decode", 0x5A6F30, 0x79, kDecodeCalls, 2},
    {"Music_SetVolume", 0x5A6FB0, 0x31, kSetVolumeCalls, 1},
    {"Music_Stop", 0x5A7050, 0x29, nullptr, 0},
    {"Music_Release", 0x5A70A0, 0x99, kReleaseCalls, 2},
    {"Music_Pump", 0x5A7230, 0x84, kPumpCalls, 3},
};
// The Win32 calls, through absolute operands: the offset of each disp32 in
// its copy, and the import slot it names.
struct ImportOperand { Fn fn; std::uint32_t offset, slot; const void* const* ours; };
const ImportOperand kImports[] = {
    {kSetNotify, 0x19, kImportCreateEvent, &g_slot_create_event},   // mov esi, [CreateEventA]
    {kRelease, 0x6B, kImportCloseHandle, &g_slot_close_handle},     // mov esi, [CloseHandle]
    {kPump, 0x25, kImportMsgWait, &g_slot_msg_wait},                // call [MsgWaitForMultipleObjects]
};

const void* Ours(unsigned k) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (k) {
    case kPlayEffect: return f(&Sound_PlayEffectEax);
    case kLoadFile: return f(&Music_LoadFile);
    case kPlay: return f(&Music_PlayEax);
    case kFadeOutStop: return f(&Music_FadeOutStopEax);
    case kFadeIn: return f(&Music_FadeInEax);
    case kFadeOut: return f(&Music_FadeOutEax);
    case kTick: return f(&Sound_Tick);
    case kFromWave: return f(&SndBuf_FromWave);
    case kCreateFromWave: return f(&SndBuf_CreateFromWave);
    case kFindData: return f(&Wave_FindData);
    case kWrite: return f(&SndBuf_Write);
    case kSndPlay: return f(&SndBuf_Play);
    case kSndStop: return f(&SndBuf_Stop);
    case kSndRelease: return f(&SndBuf_Release);
    case kStart: return f(&Music_Start);
    case kOpenDecoder: return f(&Music_OpenDecoder);
    case kSetNotify: return f(&Music_SetNotify);
    case kCreateBuffer: return f(&Music_CreateBuffer);
    case kDecode: return f(&Music_Decode);
    case kSetVolume: return f(&Music_SetVolume);
    case kStop: return f(&Music_Stop);
    case kRelease: return f(&Music_Release);
    default: return f(&Music_Pump);
    }
}
// Those whose eax a caller reads: the rest leave whatever they leave.
bool HasResult(unsigned k) {
    switch (k) {
    case kPlayEffect: case kLoadFile: case kPlay: case kFadeOutStop: case kFadeIn: case kFadeOut: case kFromWave:
    case kCreateFromWave: case kFindData: case kWrite: case kOpenDecoder: case kSetNotify: case kCreateBuffer: return true;
    default: return false;
    }
}

// --- The state --------------------------------------------------------------
struct State {
    unsigned char tables[kTables];
    unsigned char device[kDevice];
    std::uint32_t loaded, track;
    unsigned char wave[kWave], dst[kDst], lock[kLockMem], heap[kHeap];
    Frame frames[3];
    std::uint32_t out;
    std::uint32_t result, control;
    Entry log[kLog];
    std::uint32_t log_n;
};
static_assert(kTables % 4 == 0 && kDevice % 4 == 0, "State has no padding");

unsigned short ControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

void Capture(State& s) {
    std::memcpy(s.tables, At(kTablesAt), kTables);
    std::memcpy(s.device, At(kDeviceAt), kDevice);
    s.loaded = static_cast<std::uint32_t>(Music_LoadedTrack);
    s.track = Music_Track;
    std::memcpy(s.wave, g_wave, kWave);
    std::memcpy(s.dst, g_dst, kDst);
    std::memcpy(s.lock, g_lock, kLockMem);
    std::memcpy(s.heap, g_heap, kHeap);
    std::memcpy(s.frames, g_frames, sizeof s.frames);
    s.out = Address(g_out);
    s.control = ControlWord();
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    std::memcpy(At(kTablesAt), s.tables, kTables);
    std::memcpy(At(kDeviceAt), s.device, kDevice);
    Music_LoadedTrack = static_cast<int>(s.loaded);
    Music_Track = static_cast<unsigned char>(s.track);
    std::memcpy(g_wave, s.wave, kWave);
    std::memcpy(g_dst, s.dst, kDst);
    std::memcpy(g_lock, s.lock, kLockMem);
    std::memcpy(g_heap, s.heap, kHeap);
    std::memcpy(g_frames, s.frames, sizeof g_frames);
    g_out = At(s.out);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x51D0F00Du;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
std::uint32_t Pick(std::initializer_list<std::uint32_t> seeds) {
    const unsigned i = Next() % (seeds.size() + 1);
    return i < seeds.size() ? seeds.begin()[i] : Next();
}
std::uint32_t Stale(std::uint32_t low, unsigned bits) { return (Next() << bits) | low; }
std::uint32_t FloatBits(float f) {
    std::uint32_t v;
    std::memcpy(&v, &f, sizeof v);
    return v;
}
// A float as the music's volume and step hold them: the ends of the range,
// fractions, signed zeros, out of range, the specials, anything.
std::uint32_t Volume() {
    switch (Next() % 8) {
    case 0: return Pick({FloatBits(0.0f), FloatBits(127.0f), 0x80000000u, FloatBits(63.5f), FloatBits(1.0f), FloatBits(-1.0f)});
    case 1: return Pick({0x7F800000u, 0xFF800000u, 0x7FC00000u, 0x7FA00000u, 0x00000001u, FloatBits(1e10f), FloatBits(-1e10f)});
    case 2: return Next();
    default: return FloatBits(static_cast<float>(static_cast<int>(Next() % 40001) - 20000) / 128.0f);
    }
}
void* Maybe(void* p, unsigned in) { return Next() % in == 0 ? nullptr : p; }

// The chunks of a wave for Wave_FindData: tags one letter off d a t a, sizes
// that are odd, and the data chunk last.
void BuildWave() {
    const std::uint32_t fmt = Pick({0x10, 0x12, 0x14, 0x28, 0, 1}) % 0x40;
    SetDword(Address(g_wave) + 0x10, fmt);
    // The fmt chunk's body is a sled of small data chunks, 8 bytes each: a
    // walk that starts anywhere else in it ends there, by a count, not a fault.
    for (std::uint32_t i = 0x14; i + 8 <= 0x14 + fmt; i += 8) {
        std::memcpy(g_wave + i, "data", 4);
        SetDword(Address(g_wave) + i + 4, 0xD000 + i);
    }
    std::uint32_t at = 0x14 + fmt;
    static const char* const kTags[] = {"LIST", "fact", "dat ", "Data", "daTa", "datA", "xata", "dxta", "daxa", "atad", "DATA"};
    const unsigned chunks = Next() % 5;
    for (unsigned i = 0; i < chunks && at + 0x50 < kWave - 0x100; ++i) {
        std::memcpy(g_wave + at, kTags[Next() % (sizeof kTags / sizeof kTags[0])], 4);
        const std::uint32_t size = Next() % 0x40;
        SetDword(Address(g_wave) + at + 4, size);
        at += 8 + size;
    }
    std::memcpy(g_wave + at, "data", 4);
    SetDword(Address(g_wave) + at + 4, Pick({0, 1, 0x7FFFFFFF, 0x80000000u, 0x100}));
}

struct Args { std::uint32_t a[3]; };

Args Generate(unsigned k, State& input) {
    auto* words = reinterpret_cast<std::uint32_t*>(&input);
    for (unsigned i = 0; i < offsetof(State, result) / 4; ++i) words[i] = Next();
    g_seed = Next();
    g_find_may_skip = k == kSndPlay;
    Apply(input);
    // The channels, the music's globals, the device block.
    for (unsigned i = 0; i < 23; ++i) Sound_Channels[i] = Next() % 2 ? 0 : Next() % 3 ? Address(AnyBuffer(Next())) : Next();
    Music_FileSize = static_cast<int>(Next() % 4 ? Next() % 0x200 : Next());
    Music_FileLoops = static_cast<int>(Pick({0, 1}));
    Music_FadeCount = static_cast<int>(Pick({0, 1, 2, 0, 1}));
    Music_FadeStops = static_cast<int>(Pick({0, 1}));
    SetDword(Address(&Music_Volume), Volume());
    SetDword(Address(&Music_FadeStep), Volume());
    // Both creates run only under a test for the device (their callers').
    Snd_Device = k == kCreateBuffer || k == kCreateFromWave ? &g_devices[Next() & 1] : Maybe(&g_devices[Next() & 1], 8);
    Music_Buffer = Maybe(AnyBuffer(Next()), 5);
    Music_Notify = Maybe(&g_notifies[Next() & 1], 5);
    Music_Decoder = Maybe(&g_decoders[Next() % 3], 5);
    Music_Data = Maybe(At(Next()), 4);
    Music_Loops = static_cast<int>(Pick({0, 1}));
    Music_Finished = static_cast<int>(Pick({0, 1}));
    Music_LoadedTrack = static_cast<int>(Next() % 3 ? Next() % 0x100 : Next());
    Music_Track = static_cast<unsigned char>(Next());
    for (Frame& f : g_frames) f.bytes = 1 + Next() % 0x40;
    g_out = nullptr;
    g_cue = 0;
    std::memset(g_entries, 0, sizeof g_entries);
    Args x{};
    for (auto& a : x.a) a = Next();
    switch (k) {
    case kPlayEffect: {
        const std::uint32_t bank = Next() % 4 ? 1 + Next() % 6 : Pick({0, 7, 15, 1, 6});
        const std::uint32_t cue = Next() % 4 ? Next() % 24 : Pick({23, 24, 0x2F, 0xFF});
        x.a[0] = Stale((Next() & 0xF000) | bank << 8 | cue, 16);
        const std::uint32_t at = Address(Sound_Banks) + (bank - 1) * kBankBytes + cue * 16;
        if (at >= kTablesAt && at + 16 <= kTablesEnd) {
            g_cue = at;
            for (unsigned i = 0; i < 4; ++i) {
                std::uint32_t word;
                switch (Next() % 6) {
                case 0: word = 0; break;
                case 1: word = (Next() & 0xFFFF1F00u) | 0xFF; break;
                default: {
                    const std::uint32_t voice = Next() % 3 ? Next() % 64 : Pick({0x3F, 0x40, 0x7F, 0xFE});
                    const std::uint32_t channel = Next() % 2 ? Next() % 32 : Pick({0, 22, 23, 24, 31});
                    word = (Next() & 0xFFFF0000u) | (Next() % 2 ? 0x8000u : 0) | (Next() & 0x6000u) | channel << 8 | voice;
                    if (Next() % 8 == 0) word &= 0xFFFFu;
                }
                }
                SetDword(at + 4 * i, word);
                const std::uint32_t entry = Address(Sound_Banks) + (bank - 1) * kBankBytes + kVoices + (word & 0xFF) * 8;
                g_entries[i] = entry >= kTablesAt && entry + 8 <= kTablesEnd ? entry : 0;
            }
        }
        break;
    }
    case kLoadFile:
        x.a[0] = Pick({0, 1, 0x8D, 141, 999, 1000, 0xFF});
        break;
    case kPlay: {
        switch (Next() % 5) {
        case 0: x.a[0] = Music_Track; break;
        case 1: x.a[0] = static_cast<std::uint32_t>(Music_LoadedTrack); break;
        case 2: x.a[0] = Music_Track | 0x100u << (Next() % 24); break;
        case 3: x.a[0] = Next() & 0xFF; break;
        default: break;
        }
        if (Next() % 3 == 0) Music_LoadedTrack = static_cast<int>(x.a[0]);
        x.a[1] = Pick({0, 1, 8, 16, 0xFFFFFFFFu});
        break;
    }
    case kFadeOutStop: case kFadeIn: case kFadeOut:
        x.a[0] = Pick({0, 1, 2, 8, 10, 16, 0x7F, 0xFFFFFFFFu, 0x7FFFFFFF, 0x80000000u, 3});
        break;
    case kTick:
        break;
    case kFromWave: case kCreateFromWave:
        x.a[0] = Address(g_wave);
        break;
    case kFindData:
        BuildWave();
        x.a[0] = Address(g_wave);
        x.a[1] = Next() % 4 ? Address(&g_out) : 0;
        break;
    case kWrite: {
        // The offset (20 bits) and the size (at most 0x200) share x.a[2]: see Run.
        const std::uint32_t offset = Next() % 3 ? Next() % 0x24000 : Next() & 0xFFFFF;
        const std::uint32_t size = Pick({0, 1, 3, 4, 5, 0x100, 0x200}) % 0x201;
        x.a[0] = Address(AnyBuffer(Next()));
        x.a[1] = Address(g_wave);
        x.a[2] = offset << 12 | size;
        break;
    }
    case kSndPlay:
        x.a[0] = Next() % 4 ? (Address(g_wave) | (Next() % 2 ? 0x80000000u : 0)) : Next();
        x.a[1] = Address(Maybe(AnyBuffer(Next()), 6));
        break;
    case kSndStop: case kSndRelease:
        x.a[0] = Address(Maybe(AnyBuffer(Next()), 5));
        break;
    case kStart:
        x.a[0] = Address(g_wave);
        x.a[1] = Pick({0, 1, 4, 0x100, 0x200}) % 0x201;
        x.a[2] = Pick({0, 1});
        break;
    case kOpenDecoder:
        x.a[0] = Next() % 2 ? Address(g_wave) : Next();
        break;
    case kSetNotify:
        x.a[0] = Address(AnyBuffer(Next()));
        break;
    case kCreateBuffer:
        break;
    case kDecode:
        x.a[0] = Address(g_dst);
        x.a[1] = Pick({1, 2, 3, 4, 5, 0x40, 0x41, 0x100, 0x200});
        if (x.a[1] > 0x200 || x.a[1] == 0) x.a[1] = 1 + x.a[1] % 0x200;
        // A size of 0 or below: the signed test ends the loop after one frame
        // (the stand-in never ends the stream then: the zero fill would run
        // for gigabytes in both).
        if (Next() % 16 == 0) {
            static const std::uint32_t kNotPositive[] = {0, 0xFFFFFFFFu, 0x80000000u, 0xFFFFFF00u};
            x.a[1] = kNotPositive[Next() % 4];
        }
        g_decode_size = static_cast<int>(x.a[1]);
        break;
    case kSetVolume:
        x.a[0] = Volume();
        break;
    case kStop: case kRelease: case kPump:
        break;
    }
    return x;
}

using Fn0 = std::uint32_t (__cdecl*)();
using Fn1 = std::uint32_t (__cdecl*)(std::uint32_t);
using Fn2 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t);
using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);
using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

std::uint32_t Run(const void* fn, unsigned k, const Args& x) {
    void* const p = const_cast<void*>(fn);
    switch (k) {
    case kTick: case kCreateBuffer: case kStop: case kRelease: case kPump: return reinterpret_cast<Fn0>(p)();
    case kPlayEffect: case kLoadFile: case kFadeOutStop: case kFadeIn: case kFadeOut: case kFromWave: case kCreateFromWave:
    case kSndStop: case kSndRelease: case kSetNotify: case kSetVolume:
        return reinterpret_cast<Fn1>(p)(x.a[0]);
    case kPlay: case kFindData: case kOpenDecoder: case kDecode: return reinterpret_cast<Fn2>(p)(x.a[0], x.a[1]);
    case kSndPlay: case kStart: return reinterpret_cast<Fn3>(p)(x.a[0], x.a[1], x.a[2]);
    case kWrite: return reinterpret_cast<Fn4>(p)(x.a[0], x.a[1], x.a[2] >> 12, x.a[2] & 0xFFF);
    default: bof3::Fatal("sound: no runner for %u", k);
    }
}

void PatchImport(void* copy, const ImportOperand& op) {
    auto* code = static_cast<unsigned char*>(copy);
    std::uint32_t disp;
    std::memcpy(&disp, code + op.offset, sizeof disp);
    if (disp != op.slot) bof3::Fatal("sound: %s +0x%X does not name the import slot 0x%X", kClones[op.fn].name, (unsigned)op.offset, (unsigned)op.slot);
    disp = Address(op.ours);
    std::memcpy(code + op.offset, &disp, sizeof disp);
}

}  // namespace

void SelfTest() {
    // The floats ours reads from the image, as the disassembly says they are.
    if (Dword(kInverse127At) != 0x3C010204u || Dword(k10000At) != 0x461C4000u || Dword(k127At) != 0x42FE0000u)
        bof3::Fatal("sound: the volume constants are not the image's");
    BuildFakes();
    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[10];
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    for (const ImportOperand& op : kImports) PatchImport(clones[op.fn], op);

    constexpr unsigned kPerFunction = 2000;
    constexpr unsigned kRounds = kPerFunction * kCount;
    static State saved, input, theirs, ours;
    Capture(saved);
    const unsigned short cw_saved = ControlWord();
    g = kStubs;
    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    const unsigned short words[] = {0x027F, 0x037F, 0x007F};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        const Args x = Generate(k, input);
        Capture(input);
        const unsigned short cw = words[round % 5 == 0 ? 1 + (round / 5) % 2 : 0];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            SetControlWord(cw);
            State& out = pass ? ours : theirs;
            const std::uint32_t result = Run(pass ? Ours(k) : clones[k], k, x);
            Capture(out);
            out.result = HasResult(k) ? result : 0;
        }
        SetControlWord(cw_saved);
        calls += theirs.log_n;
        if (std::memcmp(&theirs, &ours, sizeof theirs) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      sound self-test MISMATCH: round %u, %s, result %08X / %08X, log %u / %u", round,
                          kClones[k].name, theirs.result, ours.result, theirs.log_n, ours.log_n);
        }
    }
    g = kOriginals;
    Apply(saved);
    SetControlWord(cw_saved);
    bof3::Log("shadow      sound self-test: %u rounds (%u per function, %u functions), %u calls to the stand-ins, %u MISMATCHES",
              kRounds, kPerFunction, (unsigned)kCount, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      sound self-test: %s %u of %u rounds differ", kClones[k].name, bad_per[k], kPerFunction);
    if (bad) bof3::Fatal("the sound layer differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace sound

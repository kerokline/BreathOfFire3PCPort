// BOF3X_SHADOW=sound_rest: a differential fuzz of group PS's nine functions
// against byte-copies of Capcom's, once at start-up (docs/sound-rest.md
// section 4). Every call out of a copy is re-aimed at a recorder - the jumps
// of the three thunks and Sound_PauseAll's tail included, so each function is
// tested alone - and ours reach the same recorders through SoundRest_g.
// DirectSound is a set of fake COM objects (three devices, six buffers) whose
// methods record their arguments and answer from a hash; DirectSoundCreate is
// a recorder that hands out a fake device. No real audio: this runs before
// the game's C runtime is up, and nothing it touches is left changed.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/sound_callees.h"
#include "game/sound_rest_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

extern "C" int __cdecl Sound_PauseAllEax(void) __asm__("_Sound_PauseAll");

// Calls fn(a0, a1) with ecx set: the thunks, Music_Halt and SndStream_IsPlaying
// read the ecx their caller left.
extern "C" std::uint32_t __cdecl SoundRestFuzz_Call(const void* fn, std::uint32_t ecx, std::uint32_t a0, std::uint32_t a1);
__asm__(
    ".globl _SoundRestFuzz_Call\n"
    "_SoundRestFuzz_Call:\n"
    "    pushl 16(%esp)\n"
    "    pushl 16(%esp)\n"
    "    movl 16(%esp), %ecx\n"
    "    movl 12(%esp), %eax\n"
    "    call *%eax\n"
    "    addl $8, %esp\n"
    "    ret\n");

// The recorders the thunks jump to: each hands SoundRestFuzz_Jumped its id,
// the first stack argument its caller pushed, and the ecx it arrived with -
// what a jump must pass on untouched - and answers in eax.
extern "C" std::uint32_t __cdecl SoundRestFuzz_Jumped(std::uint32_t id, std::uint32_t arg, std::uint32_t ecx);
extern "C" int __cdecl SoundRestFuzz_Halt(void);
extern "C" int __cdecl SoundRestFuzz_Resume(void);
extern "C" int __cdecl SoundRestFuzz_Playing(void);
__asm__(
    ".globl _SoundRestFuzz_Halt\n"
    "_SoundRestFuzz_Halt:\n"
    "    pushl %ecx\n"
    "    pushl 8(%esp)\n"
    "    pushl $0x41\n"
    "    call _SoundRestFuzz_Jumped\n"
    "    addl $12, %esp\n"
    "    ret\n"
    ".globl _SoundRestFuzz_Resume\n"
    "_SoundRestFuzz_Resume:\n"
    "    pushl %ecx\n"
    "    pushl 8(%esp)\n"
    "    pushl $0x42\n"
    "    call _SoundRestFuzz_Jumped\n"
    "    addl $12, %esp\n"
    "    ret\n"
    ".globl _SoundRestFuzz_Playing\n"
    "_SoundRestFuzz_Playing:\n"
    "    pushl %ecx\n"
    "    pushl 8(%esp)\n"
    "    pushl $0x43\n"
    "    call _SoundRestFuzz_Jumped\n"
    "    addl $12, %esp\n"
    "    ret\n");

namespace sound_rest {
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
// Sound_Channels; the stream kind; the device block from Snd_BufferDesc to
// Music_Finished (the description, the format, the device, the primary, the
// music's and the stream's buffers among them).
constexpr std::uint32_t kChannelsAt = 0x6BC8C8, kChannels = kChannelsEnd - kChannelsAt;
constexpr std::uint32_t kDeviceAt = 0x7DE378, kDeviceEnd = 0x7DE3E4, kDevice = kDeviceEnd - kDeviceAt;
constexpr unsigned kLog = 48;

struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;
unsigned g_k;   // the function under test

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

// --- DirectSound, faked -------------------------------------------------------
struct Fake { const void* const* vtable; std::uint32_t id; };
Fake g_devices[3], g_buffers[6];
void* AnyBuffer(std::uint32_t h) { return &g_buffers[h % 6]; }
void* AnyDevice(std::uint32_t h) { return &g_devices[h % 3]; }
std::uint32_t Id(const void* self) { return static_cast<const Fake*>(self)->id; }

// A callee may change what its caller reads after it. A pointer the originals
// dereference without a test (the device, the primary, the music's buffer
// once it was there) is only ever swapped for another fake.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h & 0x100) return;
    const std::uint32_t v = h >> 12;
    switch (h % 7) {
    case 0: Snd_Device = AnyDevice(v); break;
    case 1: Snd_Primary = AnyBuffer(v); break;
    case 2: if (Music_Buffer) Music_Buffer = AnyBuffer(v); break;
    case 3: SndStream_Buffer = (v & 8) ? AnyBuffer(v) : nullptr; break;
    case 4: SetDword(kStreamKind, (v & 3) ? 0 : v); break;
    default: Sound_Channels[v % 23] = (v & 0x200) ? Address(AnyBuffer(v >> 10)) : (v & 0x400) ? v : 0; break;
    }
}

long Answer(std::uint32_t h) {
    switch (h % 9) {
    case 0: return static_cast<long>(0x88780096u);   // DSERR_BUFFERLOST
    case 1: return static_cast<long>(0x80004005u);   // E_FAIL
    case 2: return 1;                                  // S_FALSE: a success the originals call a failure
    default: return 0;
    }
}

long __stdcall FakeRelease(void* self) { Record(0x102, Id(self)); Disturb(); return static_cast<long>(Hash() % 3); }
long __stdcall FakeCreate(void* self, void* desc, void** out, void* outer) {
    Record(0x103, Id(self), Bytes(desc, 0x24), Address(desc) ^ (Address(out) << 1), Address(outer));
    const std::uint32_t h = Hash();
    const long r = Answer(h);
    if (r == 0 || h % 5 == 0) *out = AnyBuffer(h >> 8);   // a failing create may write it too
    Disturb();
    return r;
}
long __stdcall FakeCooperate(void* self, void* hwnd, unsigned long level) {
    Record(0x118, Id(self), Address(hwnd), level);
    Disturb();
    return Answer(Hash());
}
long __stdcall FakeGetStatus(void* self, unsigned long* status) {
    Record(0x124, Id(self));
    const std::uint32_t h = Hash();
    Disturb();
    switch (h % 6) {
    case 0: return static_cast<long>(0x80004005u);     // fails, the out unwritten
    case 1: return 0;                                   // "succeeds", the out unwritten
    case 2: *status = h | 1; return 0;
    case 3: *status = (h & ~0x1FFu) | 0x100; return 0;  // bit 8, not bit 0
    default: *status = h >> 9; return Answer(h >> 4);
    }
}
long __stdcall FakePlay(void* self, unsigned long a, unsigned long b, unsigned long flags) {
    Record(0x130, Id(self), a, b, flags);
    Disturb();
    return Answer(Hash());
}
long __stdcall FakeSetFormat(void* self, const void* format) {
    Record(0x138, Id(self), Address(format), Bytes(format, 0x12));
    Disturb();
    return Answer(Hash());
}
long __stdcall FakeSetVolume(void* self, unsigned long v) { Record(0x13C, Id(self), v); Disturb(); return Answer(Hash()); }
long __stdcall FakeStop(void* self) { Record(0x148, Id(self)); Disturb(); return Answer(Hash()); }
long __stdcall FakeUnexpected(void* self) {
    bof3::Fatal("sound_rest: self-test reached a DirectSound method it does not fake (object %u)", (unsigned)Id(self));
}

const void* g_device_vtable[8];
const void* g_buffer_vtable[21];

void BuildFakes() {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    for (auto& slot : g_device_vtable) slot = f(&FakeUnexpected);
    for (auto& slot : g_buffer_vtable) slot = f(&FakeUnexpected);
    g_device_vtable[sound::kRelease / 4] = f(&FakeRelease);
    g_device_vtable[sound::kCreateSoundBuffer / 4] = f(&FakeCreate);
    g_device_vtable[kSetCooperativeLevel / 4] = f(&FakeCooperate);
    g_buffer_vtable[sound::kRelease / 4] = f(&FakeRelease);
    g_buffer_vtable[sound::kGetStatus / 4] = f(&FakeGetStatus);
    g_buffer_vtable[sound::kPlay / 4] = f(&FakePlay);
    g_buffer_vtable[kSetFormat / 4] = f(&FakeSetFormat);
    g_buffer_vtable[sound::kSetVolume / 4] = f(&FakeSetVolume);
    g_buffer_vtable[sound::kStop / 4] = f(&FakeStop);
    for (unsigned i = 0; i < 3; ++i) g_devices[i] = {g_device_vtable, 0x10 + i};
    for (unsigned i = 0; i < 6; ++i) g_buffers[i] = {g_buffer_vtable, 0x20 + i};
}

// --- The recorders --------------------------------------------------------------
void __cdecl StubSndStop(void* buffer) { Record(1, Address(buffer)); Disturb(); }
void __cdecl StubStreamStop() { Record(2); Disturb(); }
long __stdcall StubCreate(const void* guid, void** out, void* outer) {
    Record(3, Address(guid), Address(out), Address(outer));
    const std::uint32_t h = Hash();
    const long r = Answer(h);
    if (r == 0 || h % 4 == 0) *out = AnyDevice(h >> 8);   // a failing create may write it too
    Disturb();
    return r;
}

enum Fn : unsigned { kStopMusic, kResumeAll, kMusicPlaying, kPauseAll, kSetVolume, kHalt, kResume, kStreamPlaying, kInit, kCount };

}  // namespace
}  // namespace sound_rest

// Under Sound_PauseAll the tail jump's ecx is what the last callee left, which
// neither side sets (sound_rest.cpp): not recorded there.
std::uint32_t SoundRestFuzz_Jumped(std::uint32_t id, std::uint32_t arg, std::uint32_t ecx) {
    using namespace sound_rest;
    if (g_k == kPauseAll) Record(id);
    else Record(id, arg, ecx);
    Disturb();
    return Hash();
}

namespace sound_rest {
namespace {

const Callees kStubs = {
    StubSndStop, StubStreamStop, SoundRestFuzz_Halt, SoundRestFuzz_Resume, SoundRestFuzz_Playing, StubCreate,
};

// The copies and their calls out, by capstone 2026-10-05; every other jump
// stays inside. Sizes are each body's extent to its last instruction.
struct Call { std::uint32_t offset, target; const void* to; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
const Call kStopMusicCalls[] = {{0x0, 0x5A6FF0, reinterpret_cast<const void*>(&SoundRestFuzz_Halt)}};
const Call kResumeAllCalls[] = {{0x0, 0x5A7080, reinterpret_cast<const void*>(&SoundRestFuzz_Resume)}};
const Call kMusicPlayingCalls[] = {{0x0, 0x5A7020, reinterpret_cast<const void*>(&SoundRestFuzz_Playing)}};
const Call kPauseAllCalls[] = {{0xD, 0x5A6C30, reinterpret_cast<const void*>(&StubSndStop)},
                               {0x2A, 0x5A71C0, reinterpret_cast<const void*>(&StubStreamStop)},
                               {0x2F, 0x5A6FF0, reinterpret_cast<const void*>(&SoundRestFuzz_Halt)}};
const Call kSetVolumeCalls[] = {{0x22, 0x5B9550, nullptr}};   // the CRT's _ftol: pure x87, the copy keeps it
const Call kInitCalls[] = {{0x18, kDirectSoundCreate, reinterpret_cast<const void*>(&StubCreate)}};
const Clone kClones[kCount] = {
    {"Sound_StopMusic", 0x587B80, 0x5, kStopMusicCalls, 1},
    {"Sound_ResumeAll", 0x587B90, 0x5, kResumeAllCalls, 1},
    {"Sound_MusicPlaying", 0x587C20, 0x5, kMusicPlayingCalls, 1},
    {"Sound_PauseAll", 0x587C30, 0x34, kPauseAllCalls, 3},
    {"SndBuf_SetVolume", 0x5A6C60, 0x2F, kSetVolumeCalls, 1},
    {"Music_Halt", 0x5A6FF0, 0x29, nullptr, 0},
    {"Music_Resume", 0x5A7080, 0x16, nullptr, 0},
    {"SndStream_IsPlaying", 0x5A7200, 0x2A, nullptr, 0},
    {"Snd_Init", 0x5A6830, 0x145, kInitCalls, 1},
};

const void* Ours(unsigned k) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (k) {
    case kStopMusic: return f(&Sound_StopMusic);
    case kResumeAll: return f(&Sound_ResumeAll);
    case kMusicPlaying: return f(&Sound_MusicPlaying);
    case kPauseAll: return f(&Sound_PauseAllEax);
    case kSetVolume: return f(&SndBuf_SetVolume);
    case kHalt: return f(&Music_Halt);
    case kResume: return f(&Music_Resume);
    case kStreamPlaying: return f(&SndStream_IsPlaying);
    default: return f(&Snd_Init);
    }
}
// SndBuf_SetVolume leaves its caller's eax for a null buffer, and its one
// caller (ours) reads none: the rest answer in eax.
bool HasResult(unsigned k) { return k != kSetVolume; }

// --- The state --------------------------------------------------------------
struct State {
    unsigned char channels[kChannels];
    std::uint32_t kind;
    unsigned char device[kDevice];
    std::uint32_t result, control;
    Entry log[kLog];
    std::uint32_t log_n;
};

unsigned short ControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

void Capture(State& s) {
    std::memcpy(s.channels, At(kChannelsAt), kChannels);
    s.kind = Dword(kStreamKind);
    std::memcpy(s.device, At(kDeviceAt), kDevice);
    s.control = ControlWord();
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    std::memcpy(At(kChannelsAt), s.channels, kChannels);
    SetDword(kStreamKind, s.kind);
    std::memcpy(At(kDeviceAt), s.device, kDevice);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x50CA11EDu;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
std::uint32_t Pick(std::initializer_list<std::uint32_t> seeds) {
    const unsigned i = Next() % (seeds.size() + 1);
    return i < seeds.size() ? seeds.begin()[i] : Next();
}
void* Maybe(void* p, unsigned in) { return Next() % in == 0 ? nullptr : p; }

struct Args { std::uint32_t ecx, a[2]; };

Args Generate(unsigned k, State& input) {
    std::memset(&input, 0, sizeof input);
    for (unsigned i = 0; i < kChannels; ++i) input.channels[i] = static_cast<unsigned char>(Next());
    for (unsigned i = 0; i < kDevice; ++i) input.device[i] = static_cast<unsigned char>(Next());
    g_seed = Next();
    Apply(input);
    // The channels: none, a buffer, a stale word; the stream kind 0 most often.
    for (unsigned i = 0; i < 23; ++i)
        Sound_Channels[i] = Next() % 2 ? 0 : Next() % 4 ? Address(AnyBuffer(Next())) : Next();
    if (Next() % 6 == 0) for (unsigned i = 0; i < 23; ++i) Sound_Channels[i] = 0;
    SetDword(kStreamKind, Pick({0, 0, 0, 1, 2, 0x80000000u}));
    // The pointers: fakes or null; Snd_Init runs past its first test only
    // without a device.
    Snd_Device = k == kInit ? (Next() % 5 == 0 ? AnyDevice(Next()) : nullptr) : Maybe(AnyDevice(Next()), 3);
    Snd_Primary = Next() % 3 ? AnyBuffer(Next()) : Next() % 2 ? nullptr : At(Next());
    Music_Buffer = Maybe(AnyBuffer(Next()), 4);
    SndStream_Buffer = Maybe(AnyBuffer(Next()), 4);
    Args x{};
    // ecx with bit 0 set and clear, the low byte's other bits, anything.
    x.ecx = Pick({0, 1, 0x100, 0xFFFFFFFEu, 0xFFFFFFFFu, 0xFE, 0x101});
    x.a[0] = Next();
    x.a[1] = Next();
    switch (k) {
    case kSetVolume:
        x.a[0] = Address(Maybe(AnyBuffer(Next()), 5));
        // The levels: the ends of 0..127, past them, the sign, the dword's ends.
        x.a[1] = Next() % 3 ? Next() % 128
                            : Pick({0, 1, 63, 64, 126, 127, 128, 0xFFFFFFFFu, 0x7FFFFFFF, 0x80000000u, 0x1000, 0xFFFFFF81u});
        break;
    case kInit:
        x.a[0] = Pick({0, 0x10010, 0xFFFFFFFFu});   // the window handle
        break;
    default:
        break;
    }
    return x;
}

void Log(unsigned bad, unsigned round, unsigned k, const State& theirs, const State& ours) {
    if (bad > 12) return;
    bof3::Log("shadow      sound_rest self-test MISMATCH: round %u, %s, result %08X / %08X, log %u / %u", round, kClones[k].name,
              theirs.result, ours.result, theirs.log_n, ours.log_n);
}

}  // namespace

void SelfTest() {
    // The floats SndBuf_SetVolume reads from the image, as the disassembly says.
    if (Dword(sound::kInverse127At) != 0x3C010204u || Dword(sound::k10000At) != 0x461C4000u)
        bof3::Fatal("sound_rest: the volume constants are not the image's");
    BuildFakes();
    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[4];
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, c.calls[i].to, c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    constexpr unsigned kPerFunction = 3000;
    constexpr unsigned kRounds = kPerFunction * kCount;
    static State saved, input, theirs, ours;
    Capture(saved);
    const unsigned short cw_saved = ControlWord();
    const Callees saved_g = SoundRest_g;
    SoundRest_g = kStubs;
    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    const unsigned short words[] = {0x027F, 0x037F, 0x007F};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        g_k = k;
        const Args x = Generate(k, input);
        Capture(input);
        const unsigned short cw = words[round % 5 == 0 ? 1 + (round / 5) % 2 : 0];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            SetControlWord(cw);
            State& out = pass ? ours : theirs;
            const std::uint32_t result = SoundRestFuzz_Call(pass ? Ours(k) : clones[k], x.ecx, x.a[0], x.a[1]);
            Capture(out);
            out.result = HasResult(k) ? result : 0;
        }
        SetControlWord(cw_saved);
        calls += theirs.log_n;
        if (std::memcmp(&theirs, &ours, sizeof theirs) != 0) {
            ++bad_per[k];
            Log(++bad, round, k, theirs, ours);
        }
    }
    SoundRest_g = saved_g;
    Apply(saved);
    SetControlWord(cw_saved);
    bof3::Log("shadow      sound_rest self-test: %u rounds (%u per function, %u functions), %u calls to the stand-ins, "
              "%u MISMATCHES",
              kRounds, kPerFunction, (unsigned)kCount, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k])
            bof3::Log("shadow      sound_rest self-test: %s %u of %u rounds differ", kClones[k].name, bad_per[k], kPerFunction);
    if (bad) bof3::Fatal("the sound layer's last seven and its set-up differ from the original in %u of %u self-test rounds", bad,
                         kRounds);
}

}  // namespace sound_rest

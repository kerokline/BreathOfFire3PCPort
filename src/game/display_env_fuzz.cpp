// BOF3X_SHADOW=display_env: a differential fuzz of group S's eight functions
// against byte-copies of Capcom's, once at start-up (docs/display-env.md
// section 3).
//
// Each function alone: every relative call out of its copy is re-aimed at a
// recording stand-in, and ours is put on the same stand-ins through
// display_env::g. Then the two chains as trees: a copy of Gpu_PutDispEnv whose
// call reaches a copy of Gfx_Present whose call reaches a copy of
// Gfx_ClearPresent, against ours calling ours; Gpu_PutDrawEnv over
// D3d_SetBackColor likewise. DirectDraw and Direct3D are eight fake COM objects
// (surfaces, a viewport, a material) whose methods record their arguments -
// the stack-built DDBLTFX and D3DMATERIAL by their bytes - and answer with a
// chosen HRESULT, DDERR_SURFACELOST and its neighbours included; any other
// method ends the process naming its slot. Stand-ins and methods may disturb
// what their caller reads afterwards (the render flag, the surfaces, the
// isbg byte, the environment being copied, a bank's entries), always leaving
// the pointers the originals dereference untested valid. Crt_malloc and
// Crt_free are stand-ins too: this runs before BOF3.exe's C runtime is up.
//
// Compared per round: every region below, byte for byte, the log of calls,
// and the result of the two SetDef functions.
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/display_env_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace display_env {
namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

// --- Random numbers of our own ----------------------------------------------
U g_rand = 0x5A78605Au;
U Next() {
    U x = g_rand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g_rand = x;
}

// --- The memory a round compares ----------------------------------------------
constexpr unsigned kEnvBytes = 0x100, kHeapBytes = 0x2200, kPayloadBytes = 0x2400;
unsigned char g_env[kEnvBytes];           // the environments handed in, and SetDef's out
unsigned char g_heap[kHeapBytes];         // what the Crt_malloc stand-in hands out
unsigned char g_payload[kPayloadBytes];   // a kind-2 chunk

// Gpu_DispEnv and Gpu_DrawEnv with 0x10 bytes before (an env just below may
// be handed in) and Gfx_PixelFormat's first 0x10 after (one just above);
// the surfaces through the material; the material handle to the render flags;
// banks 0..7 with the channels (bank 0's record is 0x6BC5A4, bank 7's ends
// 0x6BE1C4).
constexpr U kEnvsAt = 0x7DECD0, kEnvsEnd = 0x7DED70;
constexpr U kComAt = 0x7CC338, kComEnd = 0x7CC35C;
constexpr U kFlagsAt = 0x6C3A40, kFlagsEnd = 0x6C3A50;
constexpr U kBanksAt = kBanks - kBankBytes, kBanksEnd = kBanks + 7 * kBankBytes;

struct Region { U at, bytes; };
Region g_regions[7];
unsigned g_n_regions;
constexpr unsigned kStateBytes = (kEnvsEnd - kEnvsAt) + (kComEnd - kComAt) + (kFlagsEnd - kFlagsAt) +
                                 (kBanksEnd - kBanksAt) + kEnvBytes + kHeapBytes + kPayloadBytes;

struct Entry { U what, a, b, c, d, e, f; };
constexpr unsigned kLog = 192;   // Snd_LoadBank makes up to 130 calls: every one is kept
struct State {
    unsigned char bytes[kStateBytes];
    Entry log[kLog];
    unsigned log_n;
    U result;
};
Entry g_log[kLog];
unsigned g_log_n;
U g_seed;

void Capture(State& s) {
    unsigned at = 0;
    for (unsigned i = 0; i < g_n_regions; ++i) {
        std::memcpy(s.bytes + at, At(g_regions[i].at), g_regions[i].bytes);
        at += g_regions[i].bytes;
    }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
    s.result = 0;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (unsigned i = 0; i < g_n_regions; ++i) {
        std::memcpy(At(g_regions[i].at), s.bytes + at, g_regions[i].bytes);
        at += g_regions[i].bytes;
    }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
// Where the first difference is, for the log line.
U FirstDifference(const State& a, const State& b) {
    unsigned at = 0;
    for (unsigned i = 0; i < g_n_regions; ++i) {
        for (U k = 0; k < g_regions[i].bytes; ++k)
            if (a.bytes[at + k] != b.bytes[at + k]) return g_regions[i].at + k;
        at += g_regions[i].bytes;
    }
    return 0;
}

U Hash(U salt = 0) {
    U h = (g_seed + g_log_n * 0x632BE5ABu + salt * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0, U e = 0, U f = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e, f};
    ++g_log_n;
}
U Bytes(const void* p, unsigned n) {
    U h = 2166136261u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 16777619u;
    return h;
}

// --- The COM objects, faked ---------------------------------------------------
struct Fake { const void* const* vtable; U id; };
constexpr unsigned kFakes = 6;   // 0..2 stand as surfaces, 3 and 4 as viewports; 5 is spare
Fake g_fakes[kFakes];
Fake g_materials[2];

U Id(const void* self) {
    for (unsigned i = 0; i < kFakes; ++i)
        if (self == &g_fakes[i]) return 1 + i;
    for (unsigned i = 0; i < 2; ++i)
        if (self == &g_materials[i]) return 0x11 + i;
    bof3::Fatal("display_env: a COM method was called on %p, which is no fake", self);
}
U IdOr(const void* p) {
    if (!p) return 0;
    for (unsigned i = 0; i < kFakes; ++i)
        if (p == &g_fakes[i]) return 0x100 + i;
    return Addr(p);
}

U g_env_at;        // the round's environment, which a callee may change under its caller
U g_record;        // the round's bank record

// A callee may change what its caller reads after it. The pointers the
// originals dereference without a test are only ever swapped for a fake.
void Disturb() {
    const U h = Hash(0xD157);
    if (h & 0x300) return;   // one call in four
    const U v = h >> 12;
    switch ((h >> 4) % 11) {
    case 0: PutLong(kPrimary, Addr(&g_fakes[v % 3])); break;
    case 1: PutLong(kBackBuffer, Addr(&g_fakes[v % 3])); break;
    case 2: PutLong(kViewport, Addr(&g_fakes[3 + v % 2])); break;
    case 3: At(kDrawEnvBackground)[0] = (v & 1) ? static_cast<unsigned char>(v >> 1) : 0; break;
    case 4: At(kRenderFlags)[0] ^= 1; break;
    case 5: At(kRenderFlags)[1] ^= 2; break;
    case 6: PutLong(kBackHandle, h * 0x9E3779B1u); break;
    case 7:
        if (g_env_at) At(g_env_at + v % 0x5C)[0] = static_cast<unsigned char>(v >> 8);
        break;
    case 8: At(kDispEnv + v % 0x14)[0] = static_cast<unsigned char>(v >> 8); break;
    case 9: At(kDrawEnv + 0x19 + v % 3)[0] = static_cast<unsigned char>(v >> 8); break;
    default: PutLong(kBackMaterial, (v & 3) ? Addr(&g_materials[v & 1]) : 0); break;
    }
}

long Answer(U salt) {
    const U h = Hash(salt);
    switch (h % 8) {
    case 0: case 1: case 2: return kSurfaceLost;
    case 3: return static_cast<long>(0x887601C1u);
    case 4: return static_cast<long>(0x887601C3u);
    case 5: return static_cast<long>(0x087601C2u);
    case 6: return static_cast<long>(h);
    default: return 0;
    }
}

long __stdcall FakeBlt(void* self, const void* dst, void* src, const void* src_rect, unsigned long flags, void* fx) {
    Record(0x114, Id(self), Addr(dst), IdOr(src), Addr(src_rect), flags, fx ? Bytes(fx, kBltFxBytes) : 0);
    const long r = Answer(0xB17);
    Disturb();
    return r;
}
long __stdcall FakeFlip(void* self, void* target, unsigned long flags) {
    Record(0x12C, Id(self), IdOr(target), flags);
    const long r = Answer(0xF11);
    Disturb();
    return r;
}
long __stdcall FakeRestore(void* self) {
    Record(0x16C, Id(self));
    Disturb();
    return Answer(0x4E5);
}
long __stdcall FakeSetBackground(void* self, unsigned long handle) {
    Record(0x120, Id(self), handle);
    Disturb();
    return Answer(0xB4C);
}
long __stdcall FakeClear(void* self, unsigned long count, const void* rects, unsigned long flags) {
    Record(0x130, Id(self), count, Addr(rects), flags);
    Disturb();
    return Answer(0xC1E);
}
long __stdcall FakeSetMaterial(void* self, void* material) {
    U diffuse_r, ambient_b;
    std::memcpy(&diffuse_r, static_cast<unsigned char*>(material) + 4, 4);
    std::memcpy(&ambient_b, static_cast<unsigned char*>(material) + 0x1C, 4);
    Record(0x10C, Id(self), Bytes(material, kMaterialBytes), diffuse_r, ambient_b);
    Disturb();
    return Answer(0x3A7);
}
template <int S> long __stdcall FakeUnexpected(void* self) {
    bof3::Fatal("display_env: self-test reached COM slot %d (+0x%X) of object %u, which nothing fakes", S, S * 4,
                (unsigned)Id(self));
}

// One vtable for all six: the slots these functions use do not collide
// between the interfaces (surface +0x14 / +0x2C / +0x6C, viewport +0x20 /
// +0x30, material +0x0C); the object's id in the log says which was called.
constexpr int kSlots = 32;
const void* g_vtable[kSlots];
template <int S> const void* Slot() {
    switch (S * 4) {
    case kSetMaterial: return reinterpret_cast<const void*>(&FakeSetMaterial);
    case kBlt: return reinterpret_cast<const void*>(&FakeBlt);
    case kSetBackground: return reinterpret_cast<const void*>(&FakeSetBackground);
    case kFlip: return reinterpret_cast<const void*>(&FakeFlip);
    case kClear: return reinterpret_cast<const void*>(&FakeClear);
    case kRestore: return reinterpret_cast<const void*>(&FakeRestore);
    default: return reinterpret_cast<const void*>(&FakeUnexpected<S>);
    }
}
template <int... S> void FillVtable(std::integer_sequence<int, S...>) { ((g_vtable[S] = Slot<S>()), ...); }

void BuildFakes() {
    FillVtable(std::make_integer_sequence<int, kSlots>{});
    for (unsigned i = 0; i < kFakes; ++i) g_fakes[i] = {g_vtable, 1 + i};
    for (unsigned i = 0; i < 2; ++i) g_materials[i] = {g_vtable, 0x11 + i};
}

// --- The stand-ins for the relative calls -------------------------------------
void __cdecl StubPresent() {
    Record(0x01);
    Disturb();
}
void __cdecl StubClearPresent(unsigned flags) {
    Record(0x02, flags);
    Disturb();
}
// The original pushes each byte in a dword whose upper bits are its caller's
// registers, and D3d_SetBackColor masks them off: only the bytes are compared.
void __cdecl StubSetBackColor(unsigned r, unsigned g, unsigned b) {
    Record(0x03, r & 0xFF, g & 0xFF, b & 0xFF);
    Disturb();
}

// A bank's callees may change the record under Snd_LoadBank: its data
// pointer (read again for each voice), an entry's buffer (read again after
// its release, for the channel scan), an entry still to come, a channel.
void DisturbBank() {
    const U h = Hash(0xBA4C);
    if (h % 3) return;
    const U v = h >> 8;
    switch ((h >> 2) % 4) {
    case 0: PutLong(g_record + kBankHeader, Addr(g_heap) + (v % 0x40) * 4); break;
    case 1: PutLong(g_record + kVoices + 4 + (v % 64) * 8, (v & 0x100) ? Long(kChannels + (v >> 9) % 23 * 4) : v); break;
    case 2: PutLong(g_record + kVoices + (v % 64) * 8, (v & 0x100) ? 0 : v * 0x10001u); break;
    default: PutLong(kChannels + (v % 23) * 4, Long(g_record + kVoices + 4 + ((v >> 5) % 64) * 8)); break;
    }
}
void* __cdecl StubMalloc(unsigned n) {
    Record(0x10, n);
    const U offset = (Hash(0x3A11) % 0x40) * 4 + (Hash(0x3A12) & 1);   // sometimes odd
    DisturbBank();
    return g_heap + offset;
}
void __cdecl StubFree(void* p) {
    Record(0x11, Addr(p));
    DisturbBank();
}
void __cdecl StubRelease(void* buffer) {
    Record(0x12, Addr(buffer));
    DisturbBank();
}
void* __cdecl StubFromWave(const unsigned char* wave) {
    Record(0x13, Addr(wave));
    const U h = Hash(0xF4F);
    DisturbBank();
    return (h & 7) ? At(h | 1) : nullptr;
}

const Callees kStubs = {StubPresent, StubClearPresent, StubSetBackColor, StubMalloc, StubFree, StubRelease, StubFromWave};
const Callees kTree = {Gfx_Present, Gfx_ClearPresent, D3d_SetBackColor, StubMalloc, StubFree, StubRelease, StubFromWave};

// --- The copies, by capstone 2026-09-23; every jump stays inside -----------
enum Kind : unsigned {
    kPutDisp, kPutDraw, kDefDisp, kDefDraw, kPresent, kClearPresent, kBackColor, kLoadBank, kPutDispTree, kPutDrawTree,
    kKinds
};
const char* const kNames[kKinds] = {"Gpu_PutDispEnv", "Gpu_PutDrawEnv", "Gpu_SetDefDispEnv", "Gpu_SetDefDrawEnv",
                                    "Gfx_Present", "Gfx_ClearPresent", "D3d_SetBackColor", "Snd_LoadBank",
                                    "Gpu_PutDispEnv tree", "Gpu_PutDrawEnv tree"};
void* g_clones[kKinds];

void* Clone(const char* name, U base, U size, std::initializer_list<bof3::CloneCall> calls) {
    return bof3::CloneOriginal(name, base, size, calls.begin(), static_cast<int>(calls.size()));
}
const void* F(auto fn) { return reinterpret_cast<const void*>(fn); }

void CloneAll() {
    g_clones[kPutDisp] = Clone("Gpu_PutDispEnv", 0x5A7860, 0x26, {{0x12, F(&StubPresent), 0x59EDF0}});
    g_clones[kPutDraw] = Clone("Gpu_PutDrawEnv", 0x5A7890, 0x4D, {{0x36, F(&StubSetBackColor), 0x5A5050}});
    g_clones[kDefDisp] = Clone("Gpu_SetDefDispEnv", 0x5A78E0, 0x28, {});
    g_clones[kDefDraw] = Clone("Gpu_SetDefDrawEnv", 0x5A7910, 0x4A, {});
    g_clones[kPresent] = Clone("Gfx_Present", 0x59EDF0, 0x60, {{0x59, F(&StubClearPresent), 0x59ECE0}});
    g_clones[kClearPresent] = Clone("Gfx_ClearPresent", 0x59ECE0, 0x107, {});
    g_clones[kBackColor] = Clone("D3d_SetBackColor", 0x5A5050, 0xE0, {});
    g_clones[kLoadBank] = Clone("Snd_LoadBank", 0x587CD0, 0xDE,
                                {{0x1F, F(&StubFree), 0x5B9577},
                                 {0x39, F(&StubRelease), 0x5A6C90},
                                 {0x81, F(&StubMalloc), 0x5B9660},
                                 {0xC9, F(&StubFromWave), 0x5A69C0}});
    // The trees: copies calling copies.
    void* const clear = Clone("Gfx_ClearPresent", 0x59ECE0, 0x107, {});
    void* const present = Clone("Gfx_Present", 0x59EDF0, 0x60, {{0x59, clear, 0x59ECE0}});
    g_clones[kPutDispTree] = Clone("Gpu_PutDispEnv", 0x5A7860, 0x26, {{0x12, present, 0x59EDF0}});
    void* const color = Clone("D3d_SetBackColor", 0x5A5050, 0xE0, {});
    g_clones[kPutDrawTree] = Clone("Gpu_PutDrawEnv", 0x5A7890, 0x4D, {{0x36, color, 0x5A5050}});
}

const void* Ours(unsigned k) {
    switch (k) {
    case kPutDisp: case kPutDispTree: return F(&Gpu_PutDispEnv);
    case kPutDraw: case kPutDrawTree: return F(&Gpu_PutDrawEnv);
    case kDefDisp: return F(&Gpu_SetDefDispEnv);
    case kDefDraw: return F(&Gpu_SetDefDrawEnv);
    case kPresent: return F(&Gfx_Present);
    case kClearPresent: return F(&Gfx_ClearPresent);
    case kBackColor: return F(&D3d_SetBackColor);
    case kLoadBank: return F(&Snd_LoadBank);
    default: bof3::Fatal("display_env: no function for kind %u", k);
    }
}

struct Args { U a[5]; };
using Fn0 = U (__cdecl*)();
using Fn1 = U (__cdecl*)(U);
using Fn3 = U (__cdecl*)(U, U, U);
using Fn5 = U (__cdecl*)(U, U, U, U, U);
U Run(const void* fn, unsigned k, const Args& x) {
    void* const p = const_cast<void*>(fn);
    switch (k) {
    case kPresent: return reinterpret_cast<Fn0>(p)();
    case kPutDisp: case kPutDraw: case kClearPresent: case kPutDispTree: case kPutDrawTree:
        return reinterpret_cast<Fn1>(p)(x.a[0]);
    case kBackColor: case kLoadBank: return reinterpret_cast<Fn3>(p)(x.a[0], x.a[1], x.a[2]);
    case kDefDisp: case kDefDraw: return reinterpret_cast<Fn5>(p)(x.a[0], x.a[1], x.a[2], x.a[3], x.a[4]);
    default: bof3::Fatal("display_env: no runner for kind %u", k);
    }
}
bool HasResult(unsigned k) { return k == kDefDisp || k == kDefDraw; }

unsigned short ControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

// --- A round's input ------------------------------------------------------------
U RandomByte(U odds_zero) { return Next() % odds_zero == 0 ? 0 : Next() & 0xFF; }

void RandomCom() {
    PutLong(kPrimary, Addr(&g_fakes[Next() % 3]));
    PutLong(kBackBuffer, Addr(&g_fakes[Next() % 3]));
    PutLong(kViewport, Addr(&g_fakes[3 + Next() % 2]));
    PutLong(kBackMaterial, Addr(&g_materials[Next() % 2]));
    PutLong(kBackHandle, Next());
    // Gfx_RenderFlags: bit 0 and bit 0x200 each half the time, the rest noise.
    U flags = Next() & ~0x201u;
    if (Next() & 1) flags |= 1;
    if (Next() & 1) flags |= 0x200;
    PutLong(kRenderFlags, flags);
    At(kDrawEnvBackground)[0] = static_cast<unsigned char>(RandomByte(2));
}

// An environment: usually a buffer of ours, now and then overlapping the
// global it is copied to, from below or above.
U PickEnv(U global) {
    switch (Next() % 8) {
    case 0: return global - 8;
    case 1: return global - 4;
    case 2: return global + 4;
    case 3: return global + 8;
    case 4: return global;
    default: return Addr(g_env) + (Next() % 0x20) * 4;
    }
}

Args Generate(unsigned k, unsigned* coverage) {
    for (U a = kEnvsAt; a < kEnvsEnd; ++a) At(a)[0] = static_cast<unsigned char>(Next());
    for (unsigned i = 0; i < kEnvBytes; ++i) g_env[i] = static_cast<unsigned char>(Next());
    RandomCom();
    g_env_at = 0;
    g_record = 0;
    Args x{};
    switch (k) {
    case kPutDisp: case kPutDispTree: {
        const U env = PickEnv(kDispEnv);
        g_env_at = env;
        // Half the rounds the same y: no present.
        if (Next() & 1) std::memcpy(At(kDispEnv + 2), At(env + 2), 2);
        coverage[0] += Long(kDispEnv) >> 16 == Long(env) >> 16;
        x.a[0] = env;
        break;
    }
    case kPutDraw: case kPutDrawTree: {
        const U env = PickEnv(kDrawEnv);
        g_env_at = env;
        // Each colour byte equal two times in three: all equal about 30%.
        for (U i = 0x19; i < 0x1C; ++i)
            if (Next() % 3) At(kDrawEnv + i)[0] = At(env + i)[0];
        if (Next() % 4 == 0) PutLong(kBackMaterial, 0);
        coverage[1] += At(kDrawEnv + 0x19)[0] == At(env + 0x19)[0] && At(kDrawEnv + 0x1A)[0] == At(env + 0x1A)[0] &&
                       At(kDrawEnv + 0x1B)[0] == At(env + 0x1B)[0];
        x.a[0] = env;
        break;
    }
    case kDefDisp: case kDefDraw:
        x.a[0] = Addr(g_env) + Next() % 0x80;
        for (int i = 1; i < 5; ++i) x.a[i] = Next();
        break;
    case kPresent:
        break;
    case kClearPresent:
        x.a[0] = Next();
        break;
    case kBackColor:
        for (int i = 0; i < 3; ++i) x.a[i] = Next();
        if (Next() % 6 == 0) x.a[Next() % 3] &= 0xFFFFFF00u;   // black
        if (Next() % 6 == 0) x.a[Next() % 3] |= 0xFF;          // white
        if (Next() % 6 == 0) PutLong(kBackMaterial, 0);
        if (Next() % 6 == 0) PutLong(kViewport, 0);
        break;
    case kLoadBank: {
        for (U a = kBanksAt; a < kBanksEnd; a += 4) PutLong(a, Next());
        const U r = Next() % 16;
        const U bank = r == 0 ? 0 : r == 1 ? 7 : 1 + r % 6;
        const U record = kBanks + (bank - 1) * kBankBytes;
        g_record = record;
        // A few buffer values, shared by the entries and the channels.
        U values[6];
        for (U& v : values) v = Next() | 1;
        for (U i = 0; i < 64; ++i) {
            PutLong(record + kVoices + 4 + i * 8, Next() % 3 == 0 ? 0 : values[Next() % 6]);
            PutLong(record + kVoices + i * 8, Next() % 3 == 0 ? 0 : Next());
        }
        for (U c = kChannels; c < kChannelsEnd; c += 4) PutLong(c, Next() % 2 ? values[Next() % 6] : Next());
        PutLong(record + kBankHeader, Next() % 4 == 0 ? 0 : Next());
        for (unsigned i = 0; i < kPayloadBytes; ++i) g_payload[i] = static_cast<unsigned char>(Next());
        for (U i = 0; i < 64; ++i)
            if (Next() % 3 == 0) std::memset(g_payload + kVoices + i * 8, 0, 4);
        coverage[2] += bank == 0 || bank == 7;
        x.a[0] = bank;
        x.a[1] = Addr(g_payload);
        x.a[2] = kBankHeader + Next() % 0x2000;
        break;
    }
    default: break;
    }
    for (unsigned i = 0; i < kHeapBytes; ++i) g_heap[i] = static_cast<unsigned char>(Next());
    return x;
}

}  // namespace

void SelfTest() {
    if (Long(kInverse255At) != 0x3B808081u) bof3::Fatal("display_env: the float at 0x5C4620 is not 1/255");
    BuildFakes();
    CloneAll();
    g_n_regions = 0;
    g_regions[g_n_regions++] = {kEnvsAt, kEnvsEnd - kEnvsAt};
    g_regions[g_n_regions++] = {kComAt, kComEnd - kComAt};
    g_regions[g_n_regions++] = {kFlagsAt, kFlagsEnd - kFlagsAt};
    g_regions[g_n_regions++] = {kBanksAt, kBanksEnd - kBanksAt};
    g_regions[g_n_regions++] = {Addr(g_env), kEnvBytes};
    g_regions[g_n_regions++] = {Addr(g_heap), kHeapBytes};
    g_regions[g_n_regions++] = {Addr(g_payload), kPayloadBytes};

    static State saved, input, theirs, ours;
    Capture(saved);
    const unsigned short cw_saved = ControlWord();
    const Callees g_saved = g;
    constexpr unsigned kPerKind = 4000;
    constexpr unsigned kRounds = kPerKind * kKinds;
    const unsigned short words[] = {0x027F, 0x007F, 0x037F};
    unsigned bad = 0, bad_per[kKinds] = {}, coverage[3] = {};
    unsigned counts[0x200] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kKinds;
        g_seed = round * 0x9E3779B9u + 0x7F4A7C15u;
        const Args x = Generate(k, coverage);
        Capture(input);
        const unsigned short cw = words[(round / kKinds) % 3];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g = (k == kPutDispTree || k == kPutDrawTree) ? kTree : kStubs;
            SetControlWord(cw);
            State& out = pass ? ours : theirs;
            const U result = Run(pass ? Ours(k) : g_clones[k], k, x);
            SetControlWord(cw_saved);
            Capture(out);
            out.result = HasResult(k) ? result : 0;
        }
        const unsigned n = theirs.log_n < kLog ? theirs.log_n : kLog;
        for (unsigned i = 0; i < n; ++i) ++counts[theirs.log[i].what & 0x1FF];
        if (std::memcmp(&theirs, &ours, sizeof theirs) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < n && std::memcmp(&theirs.log[first], &ours.log[first], sizeof(Entry)) == 0) ++first;
                const Entry& t = theirs.log[first < kLog ? first : 0];
                const Entry& o = ours.log[first < kLog ? first : 0];
                bof3::Log("shadow      display_env MISMATCH: round %u, %s, cw %04X, result %08X / %08X, log %u / %u, "
                          "memory at 0x%X, call %u: %X(%X %X %X %X) / %X(%X %X %X %X)",
                          round, kNames[k], cw, theirs.result, ours.result, theirs.log_n, ours.log_n,
                          (unsigned)FirstDifference(theirs, ours), first, t.what, t.a, t.b, t.c, t.d, o.what, o.a, o.b,
                          o.c, o.d);
            }
        }
    }
    g = g_saved;
    Apply(saved);
    SetControlWord(cw_saved);
    bof3::Log("shadow      display_env self-test: %u rounds (%u per kind, %u kinds), %u MISMATCHES; same y %u, same "
              "colour %u, bank 0 or 7 %u; calls: present %u, clear %u, colour %u, Blt %u, Flip %u, Restore %u, Clear %u, "
              "SetBackground %u, SetMaterial %u, free %u, release %u, malloc %u, wave %u",
              kRounds, kPerKind, (unsigned)kKinds, bad, coverage[0], coverage[1], coverage[2], counts[0x01],
              counts[0x02], counts[0x03], counts[0x114], counts[0x12C], counts[0x16C], counts[0x130], counts[0x120],
              counts[0x10C], counts[0x11], counts[0x12], counts[0x10], counts[0x13]);
    for (unsigned k = 0; k < kKinds; ++k)
        if (bad_per[k]) bof3::Log("shadow      display_env self-test: %s %u of %u rounds differ", kNames[k], bad_per[k], kPerKind);
    if (bad) bof3::Fatal("the display environments differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace display_env

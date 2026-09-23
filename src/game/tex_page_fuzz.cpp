// BOF3X_SHADOW=tex_page: a differential fuzz of the page-texture builders, their
// pixel converters and the DirectDraw surface helpers against byte-copies of
// Capcom's, once at start-up, on the fake DirectDraw of ddraw_fuzz.h.
// docs/tex-page.md section 5.
//
// The copies: the three surface helpers as one block (0x59F840..0x59F9AB; the
// two calls to Dd_InitSurfaceDesc stay inside it), the three converters as one
// block (0x5A9A59..0x5A9E1E; its two calls to Gfx_PackRgb 0x5AA79C go where the
// original called - nobody owns it), and each builder with its calls re-aimed:
// the surface helpers at the helper block's copy, the converters at recorders
// that log the call and run the converter block's copy, Gfx_ClutPixels at a
// stand-in. Ours runs on recorders that log alike and run ours, through
// tex_page::g. So a builder round compares the whole tree - Capcom's helpers
// and converters against ours - and the converters' arguments besides.
//
// Compared each round: the log (every COM call with its arguments, every
// converter call with where it writes, reads and looks up, every CLUT lookup),
// the descriptor each CreateSurface was handed, every byte of every surface's
// buffer, every byte of state either side could touch (Gfx_TexCache,
// Gfx_ClutRows, Gfx_TexCacheKey, Gfx_RenderFlags, the pixel-format records,
// Dd_StageSurface), and the return value. The fakes and the stand-ins disturb
// memory now and then after a call - the render flags, the key, the staging
// surface pointer, the entry's surface, mode byte and CLUT row generation - so
// every read the builders make is ordered against every call.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/ddraw_fuzz.h"
#include "game/tex_page_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace tex_page {
namespace {

using ddraw_fuzz::Record;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}

// --- random numbers of our own (the game's CRT is not up) -------------------------

U g_rand = 0x5A0080u;
U Next() {
    U x = g_rand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g_rand = x;
}
template <unsigned N> U Pick(const U (&values)[N]) { return values[Next() % N]; }

// --- the state -------------------------------------------------------------------

struct Region {
    U address, bytes;
};
const Region kRegions[] = {
    {kPageCache, 0x6000},       // Gfx_TexCache
    {kClutRows, 0x1000},        // Gfx_ClutRows
    {kTexKey, 8},               // Gfx_TexCacheKey
    {kRenderFlags, 4},          // Gfx_RenderFlags
    {0x7DED40, 0x120},          // the pixel-format records 0x7DED60.. and what a wrapped format index reaches below
    {kStage, 4},                // Dd_StageSurface
};
constexpr U kStateBytes = 0x6000 + 0x1000 + 8 + 4 + 0x120 + 4;
struct State {
    unsigned char bytes[kStateBytes];
};
State g_saved, g_start, g_theirs_state, g_ours_state;

void Capture(State& s) {
    U at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(s.bytes + at, At(r.address), r.bytes);
        at += r.bytes;
    }
}
void Restore(const State& s) {
    U at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(At(r.address), s.bytes + at, r.bytes);
        at += r.bytes;
    }
}
bool SameState(const State& a, const State& b, U* where) {
    U at = 0;
    for (const Region& r : kRegions) {
        for (U i = 0; i < r.bytes; ++i)
            if (a.bytes[at + i] != b.bytes[at + i]) {
                *where = r.address + i;
                return false;
            }
        at += r.bytes;
    }
    return true;
}

// The VRAM shadow, randomised once for the whole fuzz and put back after.
constexpr U kVramBytes = 0x100000;
unsigned char g_vram_saved[kVramBytes];

// The converted palettes Gfx_ClutPixels hands out: 64 of 32 bytes, each read
// up to 256 entries of 4 bytes on.
constexpr U kPaletteBytes = 64 * 32 + 1024;
alignas(4) unsigned char g_palettes[kPaletteBytes];

// --- the recorders ------------------------------------------------------------------

U g_round;
U g_entry;        // the round's cache entry: the one the build will take, or the one refreshed
U g_generation;   // the Gfx_ClutRows generation the round's CLUT names
void* g_surfaces[4];   // what MakeSurface made this round: stage A, stage B, the entry's surface, a spare

U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (ddraw_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

const U kWidths[] = {8, 16, 32, 64, 128, 256};           // every converter's
const U kHeights[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
const std::int32_t kKeyX[] = {0, 1, 2, 3, 7, 8, 63, 64, 127, 128, 255, -1, -2, -8};
const std::int32_t kKeyY[] = {0, 1, 2, 100, 255};

void RandomKey(U address, U h) {
    PutWord(address, static_cast<U>(kKeyX[h % 14]));
    PutWord(address + 2, static_cast<U>(kKeyY[(h >> 4) % 5]));
    PutWord(address + 4, kWidths[(h >> 8) % 6]);
    PutWord(address + 6, kHeights[(h >> 12) % 9]);
}

// What a callee may do to memory before the builder's next read: the render
// flags' two bits, the key, the staging surface pointer, the entry's surface,
// mode byte, CLUT word and generation. Always to values a converter can take.
void Disturb(U what) {
    const U h = Mix(what);
    if (h % 4) return;
    const U v = h >> 8;
    switch ((h >> 4) % 10) {
    case 0: At(kRenderFlags)[0] ^= 1; break;
    case 1: At(kRenderFlags)[0] ^= 0x20; break;
    case 2: RandomKey(kTexKey, v); break;
    case 3: PutLong(kStage, Addr(g_surfaces[v % 2])); break;
    case 4: PutLong(g_entry + 0x10, Addr(g_surfaces[v % 4])); break;
    case 5: At(g_entry)[1] = static_cast<unsigned char>(v % 4); break;
    case 6: PutWord(g_entry + 2, v); break;
    case 7: {
        // The generation of the row the round's CLUT names, or of the row the
        // entry's CLUT word names now - either may be read after this call.
        // Only inside the table: a CLUT of 0x8000 or more names a "row" in the
        // render flags or the page cache, which a write here would wreck.
        std::uint16_t word;
        std::memcpy(&word, At(g_entry + 2), 2);
        const U row = v & 1 ? g_generation : kClutRows + (word >> 6) * 8u;
        if (row >= kClutRows && row < kClutRows + 0x1000) PutLong(row, v * 0x10001u);
        break;
    }
    case 8: PutLong(g_entry + 0x14, v); break;
    default: RandomKey(g_entry + 8, v); break;
    }
}

void* __cdecl ClutStandIn(unsigned clut) {
    // Gfx_ClutPixels masks its argument to 16 bits first (0x5A04C5): what it
    // is given above them is not behaviour.
    Record(0x20, clut & 0xFFFF);
    Disturb(0x20);
    return g_palettes + ((clut & 0xFFFF) * 7 % 64) * 32;
}

ddraw_fuzz::Log g_theirs, g_ours;

// The fuzz stops at the first mismatch: a changed function that went on
// could fault where Capcom's never reads, and a control should be refused by
// the comparison, not by the fault.
[[noreturn]] void Refuse() { bof3::Fatal("the page textures differ from the original (stopped at the first mismatch)"); }

// True if our pass's last recorded call is the one Capcom's copy made at the
// same place. Ours runs a converter only then: a converter handed the wrong
// source or destination would read or write where the original never does.
bool SameAsTheirs() {
    const unsigned i = g_ours.n - 1;
    if (i >= ddraw_fuzz::kMaxCalls || i >= g_theirs.n) return false;
    const ddraw_fuzz::Call& a = g_ours.calls[i];
    const ddraw_fuzz::Call& b = g_theirs.calls[i];
    return a.what == b.what && std::memcmp(a.a, b.a, sizeof a.a) == 0;
}

void RecordConvert(U what, const void* dst, const void* src, const void* clut, int w, int h, int pitch) {
    U surface = 0xFFFFFFFFu, offset = Addr(dst);
    ddraw_fuzz::Locate(dst, &surface, &offset);
    Record(what, surface, offset, Addr(src) - kVram, clut ? Addr(clut) - Addr(g_palettes) : 0xFFFFFFFFu,
           static_cast<U>(w), static_cast<U>(h), static_cast<U>(pitch));
}

using Convert = void(__cdecl*)(void*, const void*, const void*, int, int, int);
using Convert16 = void(__cdecl*)(void*, const void*, int, int, int);
Convert g_their4, g_their8;
Convert16 g_their16;

template <int Ours> void __cdecl Record4(void* d, const void* s, const void* c, int w, int h, int pitch) {
    RecordConvert(0x10, d, s, c, w, h, pitch);
    if (!Ours || SameAsTheirs()) (Ours ? &::Tex_Convert4 : g_their4)(d, s, c, w, h, pitch);
    Disturb(0x10);
}
template <int Ours> void __cdecl Record8(void* d, const void* s, const void* c, int w, int h, int pitch) {
    RecordConvert(0x11, d, s, c, w, h, pitch);
    if (!Ours || SameAsTheirs()) (Ours ? &::Tex_Convert8 : g_their8)(d, s, c, w, h, pitch);
    Disturb(0x11);
}
template <int Ours> void __cdecl Record16(void* d, const void* s, int w, int h, int pitch) {
    RecordConvert(0x12, d, s, nullptr, w, h, pitch);
    if (!Ours || SameAsTheirs()) (Ours ? &::Tex_Convert16 : g_their16)(d, s, w, h, pitch);
    Disturb(0x12);
}

const Callees kOursRecorded = {Record4<1>, Record8<1>, Record16<1>, ClutStandIn};

// --- the copies ------------------------------------------------------------------------

constexpr U kHelpers = 0x59F840, kHelpersBytes = 0x16B;         // to the ret at 0x59F9AA
constexpr U kConverters = 0x5A9A59, kConvertersBytes = 0x3C5;   // to the ret at 0x5A9E1D
constexpr U kBuild = 0x5A0080, kBuildBytes = 0x434;             // to the ret at 0x5A04B3
constexpr U kRefresh = 0x5A0510, kRefreshBytes = 0x311;         // to the ret at 0x5A0820

using InitFn = void(__cdecl*)(void*);
using PlainFn = int(__cdecl*)(unsigned, unsigned, void**, unsigned);
using TextureFn = int(__cdecl*)(unsigned, unsigned, void**, void**, unsigned);
using BuildFn = unsigned long(__cdecl*)(int, int, int);
using RefreshFn = void(__cdecl*)(int, int);

struct Clones {
    InitFn init;
    PlainFn plain;
    TextureFn texture;
    BuildFn build;
    RefreshFn refresh;
} g_clone;

void MakeClones() {
    auto* helpers = static_cast<unsigned char*>(bof3::CloneOriginal("Dd_InitSurfaceDesc", kHelpers, kHelpersBytes));
    // Tex_Convert16's two calls of Gfx_PackRgb (0x5A9D82, 0x5A9DEE) go where
    // the original called: nobody owns it, so both sides of the converter
    // rounds are Capcom's packer against ours.
    const bof3::CloneCall pack[] = {{0x329, nullptr, 0x5AA79C}, {0x395, nullptr, 0x5AA79C}};
    auto* converters =
        static_cast<unsigned char*>(bof3::CloneOriginal("Tex_Convert4", kConverters, kConvertersBytes, pack, 2));
    if (!helpers || !converters) bof3::Fatal("tex_page: CloneOriginal returned null");
    g_clone.init = reinterpret_cast<InitFn>(helpers);
    g_clone.plain = reinterpret_cast<PlainFn>(helpers + 0x20);
    g_clone.texture = reinterpret_cast<TextureFn>(helpers + 0xC0);
    g_their4 = reinterpret_cast<Convert>(converters);
    g_their8 = reinterpret_cast<Convert>(converters + 0x1A1);
    g_their16 = reinterpret_cast<Convert16>(converters + 0x2D6);

    const void* init = helpers;
    const void* plain = helpers + 0x20;
    const void* texture = helpers + 0xC0;
    const void* c4 = reinterpret_cast<const void*>(&Record4<0>);
    const void* c8 = reinterpret_cast<const void*>(&Record8<0>);
    const void* c16 = reinterpret_cast<const void*>(&Record16<0>);
    const void* clut = reinterpret_cast<const void*>(&ClutStandIn);
    const bof3::CloneCall build[] = {
        {0x0AE, init, 0x59F840}, {0x0E1, plain, 0x59F860},  {0x151, c16, 0x5A9D2F}, {0x183, clut, 0x5A04C0},
        {0x1DD, c8, 0x5A9BFA},   {0x204, c4, 0x5A9A59},     {0x26A, texture, 0x59F900}, {0x2FF, c16, 0x5A9D2F},
        {0x324, clut, 0x5A04C0}, {0x380, c8, 0x5A9BFA},     {0x3A7, c4, 0x5A9A59},
    };
    const bof3::CloneCall refresh[] = {
        {0x0AF, init, 0x59F840}, {0x0FA, c16, 0x5A9D2F}, {0x156, c16, 0x5A9D2F}, {0x1A4, clut, 0x5A04C0},
        {0x1B0, init, 0x59F840}, {0x20A, c8, 0x5A9BFA},  {0x22D, c4, 0x5A9A59},   {0x28F, c8, 0x5A9BFA},
        {0x2B2, c4, 0x5A9A59},
    };
    g_clone.build = reinterpret_cast<BuildFn>(
        bof3::CloneOriginal("D3d_BuildPageTexture", kBuild, kBuildBytes, build, sizeof build / sizeof build[0]));
    g_clone.refresh = reinterpret_cast<RefreshFn>(
        bof3::CloneOriginal("D3d_RefreshPageTexture", kRefresh, kRefreshBytes, refresh, sizeof refresh / sizeof refresh[0]));
    if (!g_clone.build || !g_clone.refresh) bof3::Fatal("tex_page: CloneOriginal returned null");
}

// --- random state ---------------------------------------------------------------------------

const U kShifts[] = {0, 3, 8, 11, 16, 19, 24, 31, 32 + 3, 0xFFFFFF0Bu, 0x13, 0x0E, 0x0A};
const U kMasks[] = {0x1F, 0x3E0, 0x7C00, 0xF800, 0x7E0, 0xFF0000, 0xFF00, 0xFF, 0xFFFFFFFFu, 0, 0x8000, 0xF8F8F8};
const U kBytesPerPixel[] = {2, 4, 2, 4, 3, 0};

void RandomBytes(U address, U bytes) {
    for (U i = 0; i < bytes; i += 4) {
        const U v = Next();
        std::memcpy(At(address + i), &v, bytes - i < 4 ? bytes - i : 4);
    }
}

// The pixel-format records: random, with the fields the converters read
// seeded - the bytes per texel, the three shifts and masks.
void RandomFormats() {
    RandomBytes(0x7DED40, 0x120);
    At(kPixelFormat)[3] = static_cast<unsigned char>(Next() % 8 ? Pick(kBytesPerPixel) : Next());
    for (U i = 0; i < 3; ++i) {
        PutLong(kPixelFormat + 4 + 4 * i, Next() % 4 ? Pick(kShifts) : Next());
        PutLong(kPixelFormat + 0x10 + 4 * i, Next() % 4 ? Pick(kMasks) : Next());
    }
}

U BytesPerPixel() { return At(kPixelFormat)[3] == 4 ? 4 : 2; }

const U kPitches[] = {0, 512, 513, 514, 640, 1024, 1030, 1280, 2048, 4000, 256, 2, 3};

// --- the converters -------------------------------------------------------------------------

constexpr U kSourceBytes = 272 * 0x800;   // 256 rows read from up to 7 rows and 6 bytes in, and a margin
unsigned char* g_source;       // VirtualAlloc'd: 256 rows of 0x800
unsigned char* g_dst[2];       // the two passes' destinations
constexpr U kDstBytes = 0x110000;

U Span(U pitch) { return pitch * 256 + 1024 + 64; }

unsigned ConverterRounds(int which, unsigned rounds, unsigned* bad) {
    const U w4[] = {8, 16, 32, 64, 128, 256};
    const U w8[] = {4, 8, 16, 32, 64, 128, 256};
    const U w16[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
    unsigned wide = 0;
    for (unsigned r = 0; r < rounds; ++r) {
        RandomFormats();
        for (U i = 0; i < kPaletteBytes; i += 4) PutLong(Addr(g_palettes + i), Next());
        for (int i = 0; i < 64; ++i) {
            const U v = Next();
            std::memcpy(g_source + Next() % (kSourceBytes - 4), &v, 4);
        }
        const U w = which == 0 ? Pick(w4) : which == 1 ? Pick(w8) : Pick(w16);
        const U h = Pick(kHeights);
        U pitch = Pick(kPitches);
        if (pitch == 0) pitch = 256 * BytesPerPixel();
        const U src_offset = (Next() % 4) * 2 + (Next() % 2) * 0x800 * (Next() % 8);
        const unsigned char fill = static_cast<unsigned char>(Next());
        const U span = Span(pitch);
        std::memset(g_dst[0], fill, span);
        std::memset(g_dst[1], fill, span);
        if (BytesPerPixel() == 4) ++wide;
        const void* src = g_source + src_offset;
        const void* clut = g_palettes + (Next() % 64) * 32;
        if (which == 0) {
            g_their4(g_dst[0], src, clut, static_cast<int>(w), static_cast<int>(h), static_cast<int>(pitch));
            Tex_Convert4(g_dst[1], src, clut, static_cast<int>(w), static_cast<int>(h), static_cast<int>(pitch));
        } else if (which == 1) {
            g_their8(g_dst[0], src, clut, static_cast<int>(w), static_cast<int>(h), static_cast<int>(pitch));
            Tex_Convert8(g_dst[1], src, clut, static_cast<int>(w), static_cast<int>(h), static_cast<int>(pitch));
        } else {
            g_their16(g_dst[0], src, static_cast<int>(w), static_cast<int>(h), static_cast<int>(pitch));
            Tex_Convert16(g_dst[1], src, static_cast<int>(w), static_cast<int>(h), static_cast<int>(pitch));
        }
        if (std::memcmp(g_dst[0], g_dst[1], span) != 0) {
            U at = 0;
            while (g_dst[0][at] == g_dst[1][at]) ++at;
            if (*bad < 4)
                bof3::Log("shadow      tex_page MISMATCH: Tex_Convert%d round %u (w %u h %u pitch %u, %u bytes a texel): "
                          "byte 0x%X is 0x%02X, the original 0x%02X",
                          which == 0 ? 4 : which == 1 ? 8 : 16, r, (unsigned)w, (unsigned)h, (unsigned)pitch,
                          (unsigned)BytesPerPixel(), (unsigned)at, g_dst[1][at], g_dst[0][at]);
            ++*bad;
            Refuse();
        }
    }
    return wide;
}

// --- the surface helpers --------------------------------------------------------------------

const U kSizes[] = {0, 1, 2, 3, 4, 5, 255, 256, 257, 320, 1021, 0xFFFFFFFDu, 0x7FFFFFFFu};
const U kFormatIndices[] = {0, 1, 2, 0, 1, 0x04000000u, 0x04000001u, 0xFFFFFFFFu};


bool Compare(const char* name, unsigned r, U ret_ours, U ret_theirs, unsigned* bad) {
    char why[200] = "";
    U where = 0;
    bool same = ddraw_fuzz::SameLog(g_ours, g_theirs, why);
    if (same && !ddraw_fuzz::SamePixels(why)) same = false;
    if (same && !SameState(g_ours_state, g_theirs_state, &where)) {
        same = false;
        std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
    }
    if (same && ret_ours != ret_theirs) {
        same = false;
        std::snprintf(why, sizeof why, "returned 0x%X, the original 0x%X", (unsigned)ret_ours, (unsigned)ret_theirs);
    }
    if (!same) {
        if (*bad < 4) bof3::Log("shadow      tex_page MISMATCH: %s round %u: %s", name, r, why);
        ++*bad;
        Refuse();
    }
    return same;
}

void HelperRounds(unsigned rounds, unsigned* bad, unsigned* failed) {
    alignas(4) static unsigned char a[0x100], b[0x100];
    for (unsigned r = 0; r < rounds; ++r) {
        for (U i = 0; i < sizeof a; ++i) a[i] = b[i] = static_cast<unsigned char>(Next());
        g_clone.init(a + 0x40);
        Dd_InitSurfaceDesc(b + 0x40);
        if (std::memcmp(a, b, sizeof a) != 0) {
            if (*bad < 4) bof3::Log("shadow      tex_page MISMATCH: Dd_InitSurfaceDesc round %u", r);
            ++*bad;
            Refuse();
        }
    }
    for (unsigned r = 0; r < 2 * rounds; ++r) {
        g_round = r;
        const bool texture = r & 1;
        ddraw_fuzz::Reset(Next());
        RandomFormats();
        At(kRenderFlags)[0] = static_cast<unsigned char>(Next());
        if (Next() % 4 == 0) {
            ddraw_fuzz::FailAt(ddraw_fuzz::kDirectDrawCall + ddraw_fuzz::kCreateSurface, 0);
            ++*failed;
        }
        if (texture && Next() % 4 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kQueryInterface, 0);
        const U w = Pick(kSizes), h = Pick(kSizes), format = Pick(kFormatIndices);
        U pitches[1] = {Pick(kPitches)};
        ddraw_fuzz::SetPitches(pitches, 1);
        static U slots[2];
        const U junk0 = Next(), junk1 = Next();
        Capture(g_start);

        ddraw_fuzz::BeginPass(0);
        g_theirs.Clear();
        ddraw_fuzz::g_log = &g_theirs;
        slots[0] = junk0;
        slots[1] = junk1;
        U theirs = texture ? g_clone.texture(w, h, reinterpret_cast<void**>(&slots[0]), reinterpret_cast<void**>(&slots[1]), format)
                           : g_clone.plain(w, h, reinterpret_cast<void**>(&slots[0]), format);
        const U their_slots[2] = {slots[0], slots[1]};
        Capture(g_theirs_state);

        Restore(g_start);
        ddraw_fuzz::BeginPass(1);
        g_ours.Clear();
        ddraw_fuzz::g_log = &g_ours;
        slots[0] = junk0;
        slots[1] = junk1;
        U ours = texture ? Dd_CreateTextureSurface(w, h, reinterpret_cast<void**>(&slots[0]), reinterpret_cast<void**>(&slots[1]), format)
                         : Dd_CreatePlainSurface(w, h, reinterpret_cast<void**>(&slots[0]), format);
        Capture(g_ours_state);
        ddraw_fuzz::g_log = nullptr;
        if (slots[0] != their_slots[0] || slots[1] != their_slots[1]) ours ^= 0x80000000u;   // shows as the return
        Compare(texture ? "Dd_CreateTextureSurface" : "Dd_CreatePlainSurface", r, ours, theirs, bad);
    }
}

// --- the builders ------------------------------------------------------------------------------

const U kModes[] = {0, 1, 2, 3, 0, 1, 2, 0x100, 0x102, 0xFFFFFFFFu, 0x80000000u};
const U kModeBytes[] = {0, 1, 2, 3, 0, 1, 2, 0x80, 0xFF, 4};
const U kCluts[] = {0, 1, 0x3F, 0x40, 0x7FC0, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0xFFFFFFC0u};
const U kStates[] = {1, 2, 0x80, 0xFF};
const U kPages[] = {0, 0xF, 0x10, 0x1F};
const U kEdgeSlots[] = {0, 1, 31};
const U kFirsts[] = {0, 1, 31, 32, 32, 0xFF, 0xFF};   // 0xFF: anywhere

// A round's surfaces, the render flags, the formats, the palettes, the key,
// the cache. Returns the page.
void RandomWorld() {
    ddraw_fuzz::Reset(Next());
    RandomFormats();
    const U bpp = BytesPerPixel();
    g_surfaces[0] = ddraw_fuzz::MakeSurface(320, 256, bpp, Pick(kPitches));
    g_surfaces[1] = ddraw_fuzz::MakeSurface(320, 256, bpp, Pick(kPitches));
    g_surfaces[2] = ddraw_fuzz::MakeSurface(256, 256, bpp, Pick(kPitches));
    g_surfaces[3] = ddraw_fuzz::MakeSurface(256, 256, bpp, Pick(kPitches));
    U pitches[2] = {Pick(kPitches), Pick(kPitches)};
    ddraw_fuzz::SetPitches(pitches, 2);
    PutLong(kStage, Addr(g_surfaces[0]));
    At(kRenderFlags)[0] = static_cast<unsigned char>(Next());
    for (U i = 0; i < kPaletteBytes; i += 4) PutLong(Addr(g_palettes + i), Next());
    RandomBytes(kPageCache, 0x6000);
    RandomBytes(kClutRows, 0x1000);
    RandomKey(kTexKey, Next());
    if (Next() % 3 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock, 0);
    if (Next() % 6 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kDirectDrawCall + ddraw_fuzz::kCreateSurface, 0);
    if (Next() % 8 == 0) ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kQueryInterface, 0);
}

struct BuildCoverage {
    unsigned full, software, direct3d, direct, bits4, bits8, keyed, create_failed, lock_failed;
};

void BuildRounds(unsigned rounds, unsigned* bad, BuildCoverage* c) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r;
        RandomWorld();
        const U page = Next() % 3 == 0 ? Pick(kPages) : Next() % 32;
        const U mode = Pick(kModes);
        const U clut = Next() % 3 ? Next() & 0xFFFF : Pick(kCluts);
        // The page's first free entry: 0, 1, somewhere, 31, or none (32).
        U first = Pick(kFirsts);
        if (first == 0xFF) first = Next() % 32;
        g_generation = kClutRows + static_cast<U>(static_cast<std::int32_t>(clut) >> 6) * 8;
        for (U s = 0; s < 32; ++s) {
            const U e = kPageCache + (page * 32 + s) * kEntryBytes;
            At(e)[0] = static_cast<unsigned char>(s < first ? Pick(kStates) : s == first ? 0 : Next());
        }
        g_entry = kPageCache + (page * 32 + (first < 32 ? first : 0)) * kEntryBytes;
        if (first == 32) ++c->full;
        const bool software = At(kRenderFlags)[0] & 1;
        if (first < 32) {
            ++(software ? c->software : c->direct3d);
            if (mode & 2) ++c->direct;
            else if (mode) ++c->bits8;
            else ++c->bits4;
            if (!software && (At(kRenderFlags)[0] & 0x20)) ++c->keyed;
        }
        Capture(g_start);

        ddraw_fuzz::BeginPass(0);
        g_theirs.Clear();
        ddraw_fuzz::g_log = &g_theirs;
        const U theirs = g_clone.build(static_cast<int>(page), static_cast<int>(clut), static_cast<int>(mode));
        Capture(g_theirs_state);

        Restore(g_start);
        ddraw_fuzz::BeginPass(1);
        g_ours.Clear();
        ddraw_fuzz::g_log = &g_ours;
        g = kOursRecorded;
        const U ours = D3d_BuildPageTexture(static_cast<int>(page), static_cast<int>(clut), static_cast<int>(mode));
        g = kOriginals;
        Capture(g_ours_state);
        ddraw_fuzz::g_log = nullptr;
        for (unsigned i = 0; i < g_theirs.n && i < ddraw_fuzz::kMaxCalls; ++i) {
            if (g_theirs.calls[i].what == ddraw_fuzz::kDirectDrawCall + ddraw_fuzz::kCreateSurface && theirs == 0 &&
                i + 1 == g_theirs.n)
                ++c->create_failed;
            if (g_theirs.calls[i].what == ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock && theirs == 0 &&
                i + 1 == g_theirs.n)
                ++c->lock_failed;
        }
        Compare("D3d_BuildPageTexture", r, ours, theirs, bad);
    }
}

struct RefreshCoverage {
    unsigned software, direct3d, direct, bits4, bits8, lock_failed;
};

void RefreshRounds(unsigned rounds, unsigned* bad, RefreshCoverage* c) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r;
        RandomWorld();
        const U page = Next() % 3 == 0 ? Pick(kPages) : Next() % 32;
        const U slot = Next() % 3 == 0 ? Pick(kEdgeSlots) : Next() % 32;
        g_entry = kPageCache + (page * 32 + slot) * kEntryBytes;
        At(g_entry)[0] = static_cast<unsigned char>(Pick(kStates));
        At(g_entry)[1] = static_cast<unsigned char>(Pick(kModeBytes));
        PutWord(g_entry + 2, Next() % 3 ? Next() : Pick(kCluts));
        g_generation = kClutRows + (static_cast<U>(At(g_entry + 2)[0] | At(g_entry + 2)[1] << 8) >> 6) * 8;
        RandomKey(g_entry + 8, Next());
        PutLong(g_entry + 0x10, Addr(g_surfaces[2]));
        const bool software = At(kRenderFlags)[0] & 1;
        ++(software ? c->software : c->direct3d);
        const U mode = At(g_entry)[1];
        if (mode & 2) ++c->direct;
        else if (mode) ++c->bits8;
        else ++c->bits4;
        Capture(g_start);

        ddraw_fuzz::BeginPass(0);
        g_theirs.Clear();
        ddraw_fuzz::g_log = &g_theirs;
        g_clone.refresh(static_cast<int>(page), static_cast<int>(slot));
        Capture(g_theirs_state);

        Restore(g_start);
        ddraw_fuzz::BeginPass(1);
        g_ours.Clear();
        ddraw_fuzz::g_log = &g_ours;
        g = kOursRecorded;
        D3d_RefreshPageTexture(static_cast<int>(page), static_cast<int>(slot));
        g = kOriginals;
        Capture(g_ours_state);
        ddraw_fuzz::g_log = nullptr;
        for (unsigned i = 0; i < g_theirs.n && i < ddraw_fuzz::kMaxCalls; ++i)
            if (g_theirs.calls[i].what == ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock && i + 1 == g_theirs.n)
                ++c->lock_failed;
        Compare("D3d_RefreshPageTexture", r, 0, 0, bad);
    }
}

}  // namespace

void SelfTest() {
    MakeClones();
    std::memcpy(g_vram_saved, At(kVram), kVramBytes);
    Capture(g_saved);
    g_rand = 0x5A0080u;
    for (U i = 0; i < kVramBytes; i += 4) PutLong(kVram + i, Next());
    g_source = static_cast<unsigned char*>(VirtualAlloc(nullptr, kSourceBytes + 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_dst[0] = static_cast<unsigned char*>(VirtualAlloc(nullptr, kDstBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_dst[1] = static_cast<unsigned char*>(VirtualAlloc(nullptr, kDstBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!g_source || !g_dst[0] || !g_dst[1]) bof3::Fatal("tex_page: VirtualAlloc failed, error %lu", GetLastError());
    for (U i = 0; i < kSourceBytes; i += 4) {
        const U v = Next();
        std::memcpy(g_source + i, &v, 4);
    }

    unsigned bad = 0;
    constexpr unsigned kConvert4 = 2000, kConvert8 = 2000, kConvert16 = 1000;
    const unsigned wide4 = ConverterRounds(0, kConvert4, &bad);
    const unsigned wide8 = ConverterRounds(1, kConvert8, &bad);
    const unsigned wide16 = ConverterRounds(2, kConvert16, &bad);

    constexpr unsigned kHelperRounds = 2000;
    unsigned create_failed = 0;
    BuildCoverage bc = {};
    RefreshCoverage rc = {};
    constexpr unsigned kBuildRounds = 4000, kRefreshRounds = 4000;
    {
        const ddraw_fuzz::GlobalSwap dd(kDirectDraw, ddraw_fuzz::FakeDirectDraw());
        ddraw_fuzz::SetDisturb(nullptr);
        HelperRounds(kHelperRounds, &bad, &create_failed);
        ddraw_fuzz::SetDisturb(&Disturb);
        BuildRounds(kBuildRounds, &bad, &bc);
        RefreshRounds(kRefreshRounds, &bad, &rc);
        ddraw_fuzz::SetDisturb(nullptr);
    }
    Restore(g_saved);
    std::memcpy(At(kVram), g_vram_saved, kVramBytes);
    VirtualFree(g_source, 0, MEM_RELEASE);
    VirtualFree(g_dst[0], 0, MEM_RELEASE);
    VirtualFree(g_dst[1], 0, MEM_RELEASE);

    bof3::Log("shadow      tex_page self-test: Tex_Convert4 %u rounds (%u at 4 bytes a texel), Tex_Convert8 %u (%u), "
              "Tex_Convert16 %u (%u); Dd_InitSurfaceDesc %u, Dd_CreatePlainSurface and Dd_CreateTextureSurface %u "
              "(%u with CreateSurface failing)",
              kConvert4, wide4, kConvert8, wide8, kConvert16, wide16, kHelperRounds, 2 * kHelperRounds, create_failed);
    bof3::Log("shadow      tex_page self-test: D3d_BuildPageTexture %u rounds (page full %u, software %u, Direct3D %u, "
              "direct %u, 4-bit %u, 8-bit %u, colour-keyed %u, CreateSurface failed %u, Lock failed %u)",
              kBuildRounds, bc.full, bc.software, bc.direct3d, bc.direct, bc.bits4, bc.bits8, bc.keyed, bc.create_failed,
              bc.lock_failed);
    bof3::Log("shadow      tex_page self-test: D3d_RefreshPageTexture %u rounds (software %u, Direct3D %u, direct %u, "
              "4-bit %u, 8-bit %u, Lock failed %u)",
              kRefreshRounds, rc.software, rc.direct3d, rc.direct, rc.bits4, rc.bits8, rc.lock_failed);
    if (bad) bof3::Fatal("the page textures differ from the original in %u self-test rounds", bad);
}

}  // namespace tex_page

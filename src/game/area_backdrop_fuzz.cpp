// BOF3X_SHADOW=area_backdrop: the start-up differential fuzz of
// area_backdrop.cpp's four functions (docs/area-backdrop.md, "Checks").
//
// Each original is byte-copied with EVERY call re-aimed at a recording
// stand-in, and ours runs with the same stand-ins through area_backdrop::g.
// A round: random state with each branch's boundaries seeded, theirs, the
// same state again, ours; every byte of the state, the stand-ins' log (a
// count, a hash of every entry and the first 32 kept) compared. The
// stand-ins give back what the real callee would leave for the caller to
// read: a moved packet cursor - or, now and then, one that did not move, as
// a full pool's does - and the bytes the setters scribble.
//
// Widescreen_Live() is 0 while this runs (Widescreen_Inject comes after
// every fuzz in inject_all.cpp), so the faithful 0..320 quad is what is
// compared; the wide quad is the live check's.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_backdrop_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace area_backdrop {
namespace {

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* At(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetDword(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

std::uint32_t g_rng;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool OneIn(unsigned n) { return Next() % n == 0; }

// --- the stand-ins' log ----------------------------------------------------------

constexpr unsigned kKeep = 32, kIds = 16;
struct Log {
    std::uint32_t n, hash;
    std::uint32_t keep[kKeep][4];
    unsigned counts[kIds];
};
Log g_log;
std::uint32_t g_seed;

void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log.n < kKeep) {
        g_log.keep[g_log.n][0] = what;
        g_log.keep[g_log.n][1] = a;
        g_log.keep[g_log.n][2] = b;
        g_log.keep[g_log.n][3] = c;
    }
    for (const std::uint32_t v : {what, a, b, c}) g_log.hash = (g_log.hash ^ v) * 0x01000193u + 0x9E3779B9u;
    ++g_log.n;
    ++g_log.counts[what % kIds];
}
// Deterministic in the call's position: the same on both sides while the
// calls agree.
std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log.n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

// --- the stand-ins ---------------------------------------------------------------------

alignas(4) unsigned char g_packets[0x400];
std::uint32_t Packet(const void* p) { return Address(p) - Address(g_packets); }
constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;

void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(1, Packet(prim), static_cast<std::uint32_t>(dfe) | static_cast<std::uint32_t>(dtd) << 16, tpage);
    Record(1, static_cast<std::uint32_t>(tw));
    SetDword(prim + 4, 0xE8000000u | (tpage & 0xFFFFu));
    if (dfe) prim[6] |= 1;
    if (dtd) prim[6] |= 2;
    SetDword(prim + 8, static_cast<std::uint32_t>(tw));
}
// Both arguments are bytes to the real one (draw_emit.cpp): the original
// pushes immediates, so the whole dwords are logged. The cursor moves by the
// size, as the real one's does - or, one time in four, not at all, as when
// the pool is full: a cursor read before the commit then shows.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(2, slot, size, Packet(Gfx_PacketNext));
    if (Hash() % 4 != 0) Gfx_PacketNext += size & 0xFFu;
}
// Scribbles over every byte the caller stores after it, as the real one's
// code byte and 0.01 floats do over some of them.
unsigned char* __cdecl StubSetPolyG4(unsigned char* prim) {
    Record(3, Packet(prim));
    for (unsigned i = 4; i < 0x44; ++i) prim[i] = static_cast<unsigned char>(Hash() >> (i % 24));
    prim[7] = 0x38;
    for (unsigned z = 0x10; z <= 0x40; z += 0x10) SetDword(prim + z, kPointZeroOne);
    return prim;
}
void __cdecl StubSetSemi(unsigned char* prim, unsigned abe) {
    Record(4, Packet(prim), abe);
    prim[7] ^= 2;
}
// Only the low 16 bits reach the real one: the original pushes ax with
// stale bits above.
unsigned char __cdecl StubTest(unsigned long code) {
    Record(5, static_cast<std::uint32_t>(code) & 0xFFFFu);
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 8);
}
// The rect is the caller's local: its bytes are logged, not where it is.
unsigned char* __cdecl StubSetDrawMove(unsigned char* prim, const unsigned char* rect, unsigned long x,
                                       unsigned long y) {
    Record(6, Packet(prim), Dword(rect), Dword(rect + 4));
    Record(6, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    SetDword(prim + 4, 0xEC000000u);
    SetDword(prim + 8, Dword(rect));
    SetDword(prim + 0xC, Dword(rect + 4));
    SetDword(prim + 0x10, static_cast<std::uint32_t>(x));
    SetDword(prim + 0x14, static_cast<std::uint32_t>(y));
    return prim;
}

const Callees kStubs = {
    StubDrawMode, StubCommit, StubSetPolyG4, StubSetSemi,
    StubTest, StubSetDrawMove,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
        case 0x5A77C0: return f(kStubs.draw_mode);
        case 0x461E50: return f(kStubs.commit);
        case 0x5A7610: return f(kStubs.set_poly_g4);
        case 0x5A7780: return f(kStubs.set_semi);
        case 0x56FF00: return f(kStubs.test);
        case 0x5A7810: return f(kStubs.set_draw_move);
        default: bof3::Fatal("area_backdrop: no stand-in for a call to 0x%X", static_cast<unsigned>(target));
    }
}

// The copies and their calls, by capstone 2026-09-23 (every call in each
// function; no jump leaves any of them; AreaMap_SlotZones and
// WorldMap_PinSprite make no calls).
struct Call { std::uint32_t offset, target; };
constexpr Call kBackdropCalls[] = {{0x83, 0x5A77C0}, {0x8C, 0x461E50}, {0x98, 0x5A7610}, {0x9F, 0x5A7780},
                                   {0x13B, 0x461E50}};
constexpr Call kCycleCalls[] = {{0xD, 0x56FF00}, {0xCE, 0x5A7810}, {0xD7, 0x461E50}};

template <unsigned N>
void* Clone(const char* name, std::uint32_t base, std::uint32_t size, const Call (&calls)[N]) {
    bof3::CloneCall re_aimed[N];
    for (unsigned i = 0; i < N; ++i) re_aimed[i] = {calls[i].offset, StubFor(calls[i].target), calls[i].target};
    return bof3::CloneOriginal(name, base, size, re_aimed, static_cast<int>(N));
}

// --- the state and one round ----------------------------------------------------------

struct Region { unsigned char* at; unsigned size; };
template <class T> Region R(T& v) { return {reinterpret_cast<unsigned char*>(&v), sizeof v}; }
Region R(void* at, unsigned size) { return {static_cast<unsigned char*>(at), size}; }

constexpr unsigned kStateMax = 0x1000;
unsigned char g_saved[kStateMax], g_input[kStateMax], g_out[2][kStateMax];
Log g_logs[2];

unsigned Total(const Region* r, unsigned n) {
    unsigned total = 0;
    for (unsigned i = 0; i < n; ++i) total += r[i].size;
    if (total > kStateMax) bof3::Fatal("area_backdrop self-test: state of 0x%X bytes", total);
    return total;
}
void Capture(const Region* r, unsigned n, unsigned char* out) {
    for (unsigned i = 0; i < n; out += r[i].size, ++i) std::memcpy(out, r[i].at, r[i].size);
}
void Apply(const Region* r, unsigned n, const unsigned char* in) {
    for (unsigned i = 0; i < n; in += r[i].size, ++i) std::memcpy(r[i].at, in, r[i].size);
}
void Randomize(const Region* r, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        for (unsigned k = 0; k < r[i].size; ++k) r[i].at[k] = static_cast<unsigned char>(Next());
}

// Runs `theirs` then `ours` from the state as it stands; true if the state
// or the log differ. The copy runs under the game's x87 control word.
template <class Theirs, class Ours>
bool Pair(const char* name, unsigned round, const Region* r, unsigned n, Theirs&& theirs, Ours&& ours, unsigned& bad) {
    const unsigned total = Total(r, n);
    Capture(r, n, g_input);
    g_seed = Next();
    const unsigned short saved_word = GetControlWord();
    for (int pass = 0; pass < 2; ++pass) {
        Apply(r, n, g_input);
        std::memset(&g_log, 0, sizeof g_log);
        if (pass == 0) {
            SetControlWord(kGameControlWord);
            theirs();
            SetControlWord(saved_word);
        } else {
            ours();
        }
        Capture(r, n, g_out[pass]);
        g_logs[pass] = g_log;
    }
    const bool log_differs = g_logs[0].n != g_logs[1].n || g_logs[0].hash != g_logs[1].hash;
    const bool state_differs = std::memcmp(g_out[0], g_out[1], total) != 0;
    if (!log_differs && !state_differs) return false;
    if (++bad <= 8) {
        if (state_differs) {
            unsigned at = 0;
            while (g_out[0][at] == g_out[1][at]) ++at;
            unsigned region = 0, base = 0;
            while (at >= base + r[region].size) base += r[region++].size;
            bof3::Log("shadow      area_backdrop %s MISMATCH: round %u, state region %u (0x%08X) +0x%X: %02X / %02X",
                      name, round, region, static_cast<unsigned>(Address(r[region].at)), at - base, g_out[0][at],
                      g_out[1][at]);
        } else {
            unsigned at = 0;
            const unsigned kept = g_logs[0].n < kKeep ? g_logs[0].n : kKeep;
            while (at < kept && std::memcmp(g_logs[0].keep[at], g_logs[1].keep[at], 16) == 0) ++at;
            const std::uint32_t* a = at < kKeep ? g_logs[0].keep[at] : g_logs[0].keep[0];
            const std::uint32_t* b = at < kKeep ? g_logs[1].keep[at] : g_logs[1].keep[0];
            bof3::Log("shadow      area_backdrop %s MISMATCH: round %u, log of %u / %u calls, first difference at "
                      "entry %u: %X(%X, %X, %X) / %X(%X, %X, %X)",
                      name, round, g_logs[0].n, g_logs[1].n, at, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
        }
    }
    return true;
}

void Report(const char* name, unsigned rounds, unsigned bad, const char* detail) {
    bof3::Log("shadow      area_backdrop %s self-test: %u rounds (%s), %u MISMATCHES", name, rounds, detail, bad);
    if (bad) bof3::Fatal("%s differs from the original in %u of %u self-test rounds", name, bad, rounds);
}

alignas(4) unsigned char g_entry[0x400];

// --- AreaMap_DrawBackdrop -------------------------------------------------------------------

void SelfTestBackdrop(void (__cdecl* theirs)(const unsigned char*)) {
    constexpr unsigned kRounds = 60000;
    const Region r[] = {R(MapView_BuildFlags), R(&MapView_FocusX, 8), R(Cond_ByteFF), R(Gfx_PacketNext),
                        R(g_packets), R(g_entry)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x571BE001u;
    unsigned bad = 0, off = 0, outside = 0, drawn = 0, alternate = 0, held = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        if (OneIn(4)) MapView_BuildFlags = 0;
        // The focus at each bound of the entry's box, a cell past it, or
        // anywhere near; the low byte of the shifted difference random.
        const unsigned x0 = Next() % 0x100, z0 = Next() % 0x100;
        const unsigned x1 = OneIn(4) ? Next() % 0x100 : (x0 + Next() % 0x40) & 0xFFu;
        const unsigned z1 = OneIn(4) ? Next() % 0x100 : (z0 + Next() % 0x40) & 0xFFu;
        SetDword(g_entry + 4, x0 | z0 << 8 | x1 << 16 | z1 << 24);
        auto focus = [&](unsigned lo, unsigned hi) -> int {
            static const int kDelta[] = {0, 0, 1, -1};
            switch (Next() % 6) {
                case 0: return static_cast<int>(lo) + kDelta[Next() % 4];
                case 1: return static_cast<int>(hi) + kDelta[Next() % 4];
                case 2: return static_cast<int>(Next());
                case 3: return static_cast<int>(Next() % 0x120) - 0x10;
                default: return hi >= lo ? static_cast<int>(lo + Next() % (hi - lo + 1)) : static_cast<int>(Next() % 0x100);
            }
        };
        const int fx = focus(x0, x1), fz = focus(z0, z1);
        MapView_FocusX = static_cast<long>(0x7FFFu - (static_cast<std::uint32_t>(fx) << 8) - Next() % 0x100);
        MapView_FocusZ = static_cast<long>(0x8000u - (static_cast<std::uint32_t>(fz) << 8) - Next() % 0x100);
        if (OneIn(2)) Cond_ByteFF = 0;
        Gfx_PacketNext = g_packets + 4 * (Next() % 0x80);
        Pair("AreaMap_DrawBackdrop", round, r, n, [&] { theirs(g_entry); }, [&] { AreaMap_DrawBackdrop(g_entry); },
             bad);
        const bool committed = g_logs[0].counts[2] != 0;
        if (MapView_BuildFlags == 0) ++off;
        else if (!committed) ++outside;
        else {
            ++drawn;
            alternate += Cond_ByteFF != 0;
            // Entries 2 and 5 are the two commits; their fourth word the cursor.
            held += g_logs[0].counts[2] == 2 && g_logs[0].keep[2][3] == g_logs[0].keep[5][3];
        }
    }
    Apply(r, n, g_saved);
    char detail[256];
    std::snprintf(detail, sizeof detail,
                  "%u with the flags clear, %u with the focus outside the box, %u drawn - %u from the alternate "
                  "colour, %u with the cursor held by the first commit",
                  off, outside, drawn, alternate, held);
    Report("AreaMap_DrawBackdrop", kRounds, bad, detail);
}

// --- AreaMap_TextureCycle -------------------------------------------------------------------

void SelfTestTextureCycle(void (__cdecl* theirs)(const unsigned char*)) {
    constexpr unsigned kRounds = 60000;
    const Region r[] = {R(Frame_Counter), R(Gfx_PacketNext), R(g_packets), R(g_entry)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x571D3001u;
    unsigned bad = 0, refused = 0, moved = 0, first = 0, last = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        static const unsigned kPeriod[] = {0, 1, 2, 0xFE, 0xFF};
        const unsigned period = (OneIn(3) ? kPeriod[Next() % 5] : Next() % 0x100) + 1;
        g_entry[0] = static_cast<unsigned char>(period - 1);
        // Pairs with thresholds in order, each at or above the one before,
        // then one of 0xFF, which no frame exceeds (frame < period <= 256).
        const unsigned count = Next() % 7;
        unsigned t = 0, thresholds[8];
        unsigned char* p = g_entry + 8;
        for (unsigned k = 0; k < count; ++k, p += 8) {
            t += OneIn(4) ? 0 : Next() % 60;
            if (t > 0xFE) t = 0xFE;
            thresholds[k] = t;
            p[3] = static_cast<unsigned char>(t);
        }
        p[3] = 0xFF;
        thresholds[count] = 0xFF;
        // The frame at a threshold, a step either side, or anywhere.
        static const int kDelta[] = {0, 0, 1, -1};
        unsigned frame;
        if (OneIn(4)) frame = Next() % period;
        else {
            const int f = static_cast<int>(thresholds[Next() % (count + 1)]) + kDelta[Next() % 4];
            frame = static_cast<unsigned>(f < 0 ? 0 : f) % period;
        }
        Frame_Counter = frame + period * (Next() % 0x10000);
        Gfx_PacketNext = g_packets + 4 * (Next() % 0x80);
        Pair("AreaMap_TextureCycle", round, r, n, [&] { theirs(g_entry); }, [&] { AreaMap_TextureCycle(g_entry); },
             bad);
        if (g_logs[0].counts[6] == 0) ++refused;
        else {
            ++moved;
            // Which pair was taken: the source rect's second dword is B, the
            // dword after the pair's first; the first pair's B is at +12.
            first += frame <= thresholds[0];
            last += frame > (count ? thresholds[count - 1] : 0) && count != 0;
        }
    }
    Apply(r, n, g_saved);
    char detail[256];
    std::snprintf(detail, sizeof detail, "%u refused by the condition, %u moves - %u from the first pair, %u past every "
                  "threshold but the 0xFF",
                  refused, moved, first, last);
    Report("AreaMap_TextureCycle", kRounds, bad, detail);
}

// --- AreaMap_SlotZones ----------------------------------------------------------------------

constexpr std::uint32_t kFourthObject = 0x905DA0;

void SelfTestSlotZones(void (__cdecl* theirs)(const unsigned char*)) {
    constexpr unsigned kRounds = 40000;
    const Region r[] = {R(Draw_OtSlot), R(ObjTrio, 0x3E4), R(At(kFourthObject), 0x3C), R(g_entry)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x571E2001u;
    unsigned bad = 0, off = 0, empty = 0, zones = 0, set = 0, cleared = 0, fourth = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        static const unsigned char kSlot[] = {4, 4, 5, 6, 7};
        Draw_OtSlot = OneIn(3) ? static_cast<unsigned char>(Next()) : kSlot[Next() % 5];
        const unsigned count = OneIn(6) ? Next() % 2 : 1 + Next() % 6;
        g_entry[2] = static_cast<unsigned char>(count + 1);
        if (OneIn(8)) g_entry[2] = static_cast<unsigned char>(Next());
        // Zones with small boxes, so that a position seeded on a bound lands
        // on it; the sense bit random.
        for (unsigned k = 0; k < 8; ++k) {
            const unsigned x0 = Next() % 0x40, z0 = Next() % 0x40, w = Next() % 8, d = Next() % 8;
            SetDword(g_entry + 4 + k * 4, (x0 << 24) | (z0 << 16) | (w << 9) | (d << 2) | (Next() & 3u));
        }
        // Each object at a bound of some zone's box, a cell past it, or
        // anywhere; the fraction at the rounding edge or anywhere.
        auto place = [&](unsigned char* position) {
            const std::uint32_t z = Dword(g_entry + 4 + (Next() % 8) * 4);
            const int x0 = static_cast<int>(z >> 24), w = static_cast<int>((z >> 9) & 0x7Fu);
            const int z0 = static_cast<int>((z >> 16) & 0xFFu), d = static_cast<int>((z >> 2) & 0x7Fu);
            static const int kDelta[] = {0, 0, -1, 1};
            static const unsigned kFrac[] = {0, 0xFFFF, 0x7FFF, 0x8000};
            auto cell = [&](int lo, int hi) -> int {
                switch (Next() % 6) {
                    case 0: return lo + kDelta[Next() % 4];
                    case 1: return hi + kDelta[Next() % 4];
                    case 2: return static_cast<int>(Next() % 0x60) - 8;
                    case 3: return static_cast<int>(static_cast<short>(Next()));
                    default: return hi > lo ? lo + static_cast<int>(Next() % static_cast<unsigned>(hi - lo)) : lo;
                }
            };
            auto pack = [&](int c) {
                const unsigned frac = OneIn(2) ? kFrac[Next() % 4] : Next() % 0x10000;
                return (static_cast<std::uint32_t>(c) << 16) - 0x8000u + frac;
            };
            SetDword(position, pack(cell(x0, x0 + w)));
            SetDword(position + 4, pack(cell(z0, z0 + d)));
        };
        for (unsigned k = 0; k < 3; ++k) place(ObjTrio + k * 0x14C + 0x34);
        place(At(kFourthObject) + 0x34);
        const unsigned char fourth_before = At(kFourthObject)[0xB];
        Pair("AreaMap_SlotZones", round, r, n, [&] { theirs(g_entry); }, [&] { AreaMap_SlotZones(g_entry); }, bad);
        if (Draw_OtSlot == 4) { ++off; continue; }
        if (g_entry[2] <= 1) { ++empty; continue; }
        zones += g_entry[2] - 1u;
        for (unsigned k = 0; k < 3; ++k) {
            const unsigned at = 1 + k * 0x14C + 0x29;   // the slot byte in the captured state
            if (g_input[at] == g_out[0][at]) continue;
            if (g_out[0][at] == 4) ++set;
            else ++cleared;
        }
        fourth += At(kFourthObject)[0xB] != fourth_before;
    }
    Apply(r, n, g_saved);
    char detail[256];
    std::snprintf(detail, sizeof detail,
                  "%u with Draw_OtSlot 4, %u with no zones; %u zones run; slot bytes changed: %u to 4, %u to "
                  "Draw_OtSlot; %u fourth-object flags changed",
                  off, empty, zones, set, cleared, fourth);
    Report("AreaMap_SlotZones", kRounds, bad, detail);
}

// --- WorldMap_PinSprite ---------------------------------------------------------------------

alignas(4) unsigned char g_sprite[0xA4];

void SelfTestPinSprite(void (__cdecl* theirs)()) {
    constexpr unsigned kRounds = 4000;
    const Region r[] = {R(Sprite_Current), R(g_sprite)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    g_rng = 0x4112A001u;
    unsigned bad = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Randomize(r, n);
        Sprite_Current = g_sprite;
        Pair("WorldMap_PinSprite", round, r, n, theirs, [] { WorldMap_PinSprite(); }, bad);
    }
    Apply(r, n, g_saved);
    Report("WorldMap_PinSprite", kRounds, bad, "every byte of the sprite random");
}

}  // namespace

void SelfTest() {
    void* const backdrop = Clone("AreaMap_DrawBackdrop", bof3::addr::AreaMap_DrawBackdrop, 0x147, kBackdropCalls);
    void* const cycle = Clone("AreaMap_TextureCycle", bof3::addr::AreaMap_TextureCycle, 0xE5, kCycleCalls);
    void* const zones = bof3::CloneOriginal("AreaMap_SlotZones", bof3::addr::AreaMap_SlotZones, 0x1C7);
    void* const pin = bof3::CloneOriginal("WorldMap_PinSprite", bof3::addr::WorldMap_PinSprite, 0x18);
    g = kStubs;
    SelfTestBackdrop(reinterpret_cast<void(__cdecl*)(const unsigned char*)>(backdrop));
    SelfTestTextureCycle(reinterpret_cast<void(__cdecl*)(const unsigned char*)>(cycle));
    SelfTestSlotZones(reinterpret_cast<void(__cdecl*)(const unsigned char*)>(zones));
    SelfTestPinSprite(reinterpret_cast<void(__cdecl*)()>(pin));
    g = kOriginals;
}

}  // namespace area_backdrop

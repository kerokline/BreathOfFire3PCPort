// A field sprite's draw key and screen position, once a frame: originals
// 0x588F20 and 0x5890E0. docs/movement-script.md section 1b.
#include "game/sprite_screen.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// The callees, through pointers so that the start-up fuzz can stand recording
// functions in for them.
struct Callees {
    void (__cdecl* inherit_key)();
    long (__cdecl* project)(const short*, unsigned long*, long*);
    void (__cdecl* dropped)(unsigned char);
    void (__cdecl* overlay)();
};
const Callees kOriginals = {Sprite_InheritDrawKey, Gte_RotTransPers, Port_DroppedCall, Sprite_QueueOverlay};
Callees g = kOriginals;

// `fld dword` then `fstp dword`: exact, except that a signalling NaN comes
// back quiet (bit 22 set) - the only change the x87 makes to a float it moves.
std::uint32_t ThroughX87(std::uint32_t bits) {
    const bool nan = (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0;
    return nan ? bits | 0x00400000u : bits;
}
// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

}  // namespace

// original 0x5890E0: a sprite with +0x24 bit 7 goes onto the overlay list, not
// the draw list (PSX FUN_8014D3D4).
extern "C" void __cdecl Sprite_QueueOverlay(void) {
    const unsigned char n = Sprite_OverlayListCount;
    if (n >= 30) return;
    Sprite_OverlayListCount = static_cast<unsigned char>(n + 1);
    Sprite_OverlayList[n] = Sprite_Current;
    if (Sprite_Current[0] & 8) g.dropped(0);
}

// original 0x588F20: Sprite_Current's draw key, screen position and depth,
// and its place on the draw list (PSX FUN_8014D184, the sibling's
// Actor_UpdateScreenPos).
//
// As the original has it: the key is 16-bit arithmetic, stored before its
// range is tested; the float screen point is stored as the x87 moves it and
// truncated as the CRT's _ftol does (see Ftol16); the PSX's call for a sprite
// with flag bit 8 is Port_DroppedCall, a bare ret, on the PC.
extern "C" void __cdecl Sprite_UpdateScreen(void) {
    unsigned char* sprite = Sprite_Current;
    if (sprite[0x24] & 0x80) {
        g.overlay();
        return;
    }
    const unsigned x_whole = Word(sprite + 0x34) == 0, z_whole = Word(sprite + 0x38) == 0;
    const signed char adjust = Sprite_KeyAdjust[sprite[0x2B] * 3u + x_whole + z_whole];
    const auto key = static_cast<std::uint16_t>(adjust - x_whole - z_whole + Word(sprite + 0x3A) + Word(sprite + 0x36) -
                                                static_cast<std::uint16_t>(MapView_Origin[1]) -
                                                static_cast<std::uint16_t>(MapView_Origin[0]) + 2);
    SetWord(sprite + 0x32, key);
    sprite = Sprite_Current;
    const auto layer = static_cast<std::int16_t>(Word(sprite + 0x32));
    if (layer < 0 || layer > 0x36) {   // off the map's layers: hidden
        sprite[0] |= 0x80;
        return;
    }
    SetWord(sprite + 0x32, static_cast<unsigned>(layer & 0xFF) << 8);
    unsigned low;
    if (Draw_OtSlot == 4) {
        sprite = Sprite_Current;
        low = static_cast<unsigned>(Long(sprite + (Draw_SortOnX ? 0x34 : 0x38)) >> 15) & 0xFF;
    } else {
        sprite = Sprite_Current;
        low = ((Word(sprite + 0x3E) ^ 0xF01Fu) >> 5) & 0xFF;
    }
    SetWord(sprite + 0x32, Word(sprite + 0x32) | low);
    g.inherit_key();

    sprite = Sprite_Current;
    short vertex[4];
    vertex[0] = static_cast<short>((Long(sprite + 0x34) >> 9) - 0x4000);
    vertex[1] = static_cast<short>((Long(sprite + 0x38) >> 9) - 0x4000);
    vertex[2] = static_cast<short>(-(static_cast<std::int16_t>(Word(sprite + 0x3E)) / 2));
    unsigned long screen[2];
    long depth_cue;
    const long depth = g.project(vertex, screen, &depth_cue);
    SetLong(Sprite_Current + 0x60, depth);
    SetLong(Sprite_Current + 0x74, static_cast<std::int32_t>(ThroughX87(static_cast<std::uint32_t>(screen[0]))));
    SetWord(Sprite_Current + 0x2E, Ftol16(static_cast<std::uint32_t>(screen[0])));
    SetLong(Sprite_Current + 0x78, static_cast<std::int32_t>(ThroughX87(static_cast<std::uint32_t>(screen[1]))));
    SetWord(Sprite_Current + 0x30, Ftol16(static_cast<std::uint32_t>(screen[1])));

    const unsigned char n = Sprite_DrawListCount;
    if (n >= 0x28) return;
    unsigned char* const drawn = Sprite_Current;
    Sprite_DrawListCount = static_cast<unsigned char>(n + 1);
    Sprite_DrawList[n] = drawn;
    if (drawn[0] & 8) g.dropped(0);
}

namespace {

// --- BOF3X_SHADOW=sprite_screen: a differential fuzz, once at start-up ------
// Two byte-copies, their calls re-aimed at the recorders below - all but the
// one to the CRT's _ftol, which the copy keeps: it is a pure x87 conversion,
// and the one thing here whose semantics ours reproduces rather than calls.
// One round: a random sprite (or Sprite_Kind2) placed near the map view's
// origin so that its key usually lands in 0..0x36, the list counts at and
// around their limits, Draw_OtSlot 4 or not; the projection's stand-in
// returns screen floats that include NaNs (signalling and quiet), infinities,
// halves and values past 2^63. Theirs, then ours; the sprite, both lists and
// counts and the stand-ins' log compared.

constexpr unsigned kBuf = 0x100, kLog = 16;
unsigned char g_sprite[kBuf];
struct Entry { std::uint32_t what, a, b, c; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c};
    ++g_log_n;
}
void __cdecl StubInheritKey() {
    Record(1, Word(Sprite_Current + 0x32));
    const std::uint32_t h = Hash();
    if (h % 3 == 0) SetWord(Sprite_Current + 0x32, h >> 8);          // the inherited key
    if (h % 5 == 0) SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(Hash()));   // what the projection reads next
}
std::uint32_t Special() {
    static const std::uint32_t kFloats[] = {0x7F800001u, 0x7FC00000u, 0xFFA00000u, 0x7F800000u, 0xFF800000u,
                                            0x3F000000u, 0xBF000000u, 0x5F000000u, 0xDF000001u, 0x5EFFFFFFu,
                                            0x47000000u, 0xC7000080u, 0x00000000u, 0x80000000u};
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return kFloats[(h >> 4) % (sizeof kFloats / sizeof kFloats[0])];
    if (h % 3 == 1) {   // a float of a screen coordinate's size, fractions included
        const float v = static_cast<float>(static_cast<std::int32_t>(h >> 8) % 4096) / 8.0f;
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        return bits;
    }
    return Hash();
}
long __cdecl StubProject(const short* v, unsigned long* sxy, long* p) {
    Record(2, static_cast<std::uint16_t>(v[0]), static_cast<std::uint16_t>(v[1]), static_cast<std::uint16_t>(v[2]));
    sxy[0] = Special();
    ++g_log_n;   // a new hash for the second
    sxy[1] = Special();
    *p = static_cast<long>(Hash());
    return static_cast<long>(Hash());
}
void __cdecl StubDropped(unsigned char) { Record(3); }
void __cdecl StubOverlay() { Record(4); }
const Callees kStubs = {StubInheritKey, StubProject, StubDropped, StubOverlay};

constexpr std::uint32_t kUpdate = 0x588F20, kUpdateSize = 0x1C0, kOverlay = 0x5890E0, kOverlaySize = 0x2F;
constexpr std::uint32_t kFtol = 0x5B9550;

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(Sprite_Kind2), Sprite_Kind2_count},
    {Address(Sprite_DrawList), 40 * 4},
    {Address(&Sprite_DrawListCount), 1},
    {Address(Sprite_OverlayList), 30 * 4},
    {Address(&Sprite_OverlayListCount), 1},
    {Address(MapView_Origin), 4},
    {Address(&Draw_OtSlot), 1},
    {Address(&Draw_SortOnX), 1},
};
constexpr unsigned kRegionBytes = Sprite_Kind2_count + 160 + 1 + 120 + 1 + 4 + 1 + 1;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char sprite[kBuf];
    unsigned char* current;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.sprite, g_sprite, kBuf);
    s.current = Sprite_Current;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_sprite, s.sprite, kBuf);
    Sprite_Current = s.current;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x1B873593u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }

using Fn = void (__cdecl*)();

void SelfTest(Fn theirs_update, Fn theirs_overlay) {
    constexpr unsigned kRounds = 20000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("sprite_screen: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    Capture(saved);
    g = kStubs;
    unsigned bad = 0, in_range = 0, listed = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const bool overlay_only = round % 4 == 3;
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.current = Next() % 5 ? g_sprite : Sprite_Kind2;
        g_seed = Next();
        Apply(input);
        unsigned char* const s = Sprite_Current;
        // Near the view's origin: the whole words of x and z a few cells past it.
        if (Next() % 4) {
            SetWord(s + 0x36, static_cast<unsigned>(static_cast<std::uint16_t>(MapView_Origin[0]) + Next() % 0x20));
            SetWord(s + 0x3A, static_cast<unsigned>(static_cast<std::uint16_t>(MapView_Origin[1]) + Next() % 0x20));
        }
        if (Often()) SetWord(s + 0x34, 0);
        if (Often()) SetWord(s + 0x38, 0);
        if (Next() % 4) s[0x24] &= 0x7F;
        s[0x2B] = static_cast<unsigned char>(Next() % 4 ? Next() % 16 : Next());
        if (Often()) Draw_OtSlot = 4;
        Sprite_DrawListCount = static_cast<unsigned char>(Next() % 4 ? Next() % 40 : 38 + Next() % 5);
        Sprite_OverlayListCount = static_cast<unsigned char>(Next() % 4 ? Next() % 30 : 28 + Next() % 5);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            (pass ? (overlay_only ? &Sprite_QueueOverlay : &Sprite_UpdateScreen) : (overlay_only ? theirs_overlay : theirs_update))();
            Capture(pass ? our_out : their_out);
        }
        const auto key = static_cast<std::int16_t>(Word((their_out.current == g_sprite ? their_out.sprite : Sprite_Kind2) + 0x32));
        in_range += !overlay_only && !(their_out.current[0] & 0x80) ? 1 : 0;
        (void)key;
        listed += their_out.memory[Sprite_Kind2_count + 160] != input.memory[Sprite_Kind2_count + 160];
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      sprite_screen self-test MISMATCH: round %u, %s, log %u / %u", round,
                      overlay_only ? "Sprite_QueueOverlay" : "Sprite_UpdateScreen", their_out.log_n, our_out.log_n);
    }
    g = kOriginals;
    Apply(saved);
    bof3::Log("shadow      sprite_screen self-test: %u rounds, %u with the flag byte's bit 7 clear after, %u appended to "
              "Sprite_DrawList, %u MISMATCHES; the sprite, both lists and counts, the view origin and the stand-ins' log "
              "compared", kRounds, in_range, listed, bad);
    if (bad) bof3::Fatal("Sprite_UpdateScreen differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void SpriteScreen_Inject() {
    if (bof3::WantsShadow("sprite_screen")) {
        const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
        const bof3::CloneCall overlay_calls[] = {{0x29, f(&StubDropped)}};
        void* const overlay = bof3::CloneOriginal("Sprite_QueueOverlay", kOverlay, kOverlaySize, overlay_calls, 1);
        const bof3::CloneCall update_calls[] = {{0x0E, f(&StubOverlay)}, {0xE8, f(&StubInheritKey)}, {0x138, f(&StubProject)},
                                                {0x15A, nullptr}, {0x17A, nullptr}, {0x1B0, f(&StubDropped)}};
        void* const update = bof3::CloneOriginal("Sprite_UpdateScreen", kUpdate, kUpdateSize, update_calls, 6);
        (void)kFtol;
        SelfTest(reinterpret_cast<Fn>(update), reinterpret_cast<Fn>(overlay));
    }
    BOF3_INJECT(Sprite_UpdateScreen);
    BOF3_INJECT(Sprite_QueueOverlay);
}

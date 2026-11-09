// A field sprite's draw key and screen position, once a frame: originals
// 0x588F20 and 0x5890E0. docs/movement-script.md section 1b.
#include "game/sprite_screen.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/band_census.h"
#include "game/layering.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound_sp.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

// Sprite_Current, as rv::Current<records::SpriteObject>(), may be a Sprite_Objects
// record or another family whose first 0x80 bytes are a sprite object's (an
// ObjTrio member, the kind-2 object), so only that part is named.

std::uint32_t Address(const volatile void* p) { return Low32(p); }

const sprite_screen::Callees kOriginals = {Sprite_InheritDrawKey, Gte_RotTransPers, Port_DroppedCall, Sprite_QueueOverlay};
}  namespace sprite_screen { Callees g = kOriginals; }
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
const sprite_screen::Callees kStubs = {StubInheritKey, StubProject, StubDropped, StubOverlay};

constexpr ::bof3::image::CodeVa kUpdate{0x588F20};
constexpr ::bof3::image::CodeVa kOverlay{0x5890E0};
constexpr std::uint32_t kUpdateSize = 0x1C0, kOverlaySize = 0x2F;
constexpr ::bof3::image::CodeVa kFtol{0x5B9550};

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(rv::Bytes(&Sprite_Kind2)), sizeof(records::SpriteObject)},
    {Address(Sprite_DrawList), 40 * 4},
    {Address(&Sprite_DrawListCount), 1},
    {Address(Sprite_OverlayList), 30 * 4},
    {Address(&Sprite_OverlayListCount), 1},
    {Address(MapView_Origin), 4},
    {Address(&Draw_OtSlot), 1},
    {Address(&Draw_SortOnX), 1},
};
constexpr unsigned kRegionBytes = sizeof(records::SpriteObject) + 160 + 1 + 120 + 1 + 4 + 1 + 1;

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
    sprite_screen::g = kStubs;
    unsigned bad = 0, in_range = 0, listed = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const bool overlay_only = round % 4 == 3;
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.current = Next() % 5 ? g_sprite : rv::Bytes(&Sprite_Kind2);
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
        const auto key = static_cast<std::int16_t>(Word((their_out.current == g_sprite ? their_out.sprite : rv::Bytes(&Sprite_Kind2)) + 0x32));
        in_range += !overlay_only && !(their_out.current[0] & 0x80) ? 1 : 0;
        (void)key;
        listed += their_out.memory[sizeof(records::SpriteObject) + 160] != input.memory[sizeof(records::SpriteObject) + 160];
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      sprite_screen self-test MISMATCH: round %u, %s, log %u / %u", round,
                      overlay_only ? "Sprite_QueueOverlay" : "Sprite_UpdateScreen", their_out.log_n, our_out.log_n);
    }
    sprite_screen::g = kOriginals;
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
        void* const overlay = bof3::CloneOriginal("Sprite_QueueOverlay", kOverlay.v, kOverlaySize, overlay_calls, 1);
        const bof3::CloneCall update_calls[] = {{0x0E, f(&StubOverlay)}, {0xE8, f(&StubInheritKey)}, {0x138, f(&StubProject)},
                                                {0x15A, nullptr}, {0x17A, nullptr}, {0x1B0, f(&StubDropped)}};
        void* const update = bof3::CloneOriginal("Sprite_UpdateScreen", kUpdate.v, kUpdateSize, update_calls, 6);
        (void)kFtol;
        SelfTest(reinterpret_cast<Fn>(update), reinterpret_cast<Fn>(overlay));
    }
}

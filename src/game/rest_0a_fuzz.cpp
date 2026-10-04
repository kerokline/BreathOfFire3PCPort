// BOF3X_SHADOW=rest_0a: group R0A's seven helpers through the scenario harness
// (scenario_harness.h, used unchanged) as kCall helpers in field mode, once at
// start-up. docs/rest_0a.md section 3. BOF3X_R0A_ONLY=<name> runs the clones
// whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group R0A --clones --harness scenario
// (2026-10-04), each extent read again to its last instruction (capstone).
// Every callee is a stand-in of the group's own, registered before the
// harness's standard rows (the group's listing stands): each logs its
// arguments masked to what the callee reads and answers what the seven test -
// Sprite_ObjectAt and Field_EffectAhead 0xFF half the time, AreaMap_ByteAt the
// cell codes 0xF0..0xF8 and their neighbours, Effect_FindFree 0xFF a third of
// the time and else a slot 0..19, MapView_SlopeAt the scratch flag and a low
// word around 0x40, MapView_GroundAt a low word around the sprite's height -
// each with garbage above what the callers read.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_0a {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/band_rows.py --group R0A --clones --harness scenario, 2026-10-04.
constexpr sh::CallSite kCalls51C390[] = {{0x3B, 0x531CF0}, {0x53, 0x536700}, {0x6D, 0x536700}, {0x87, 0x536700}};
constexpr sh::CallSite kCalls51C6A0[] = {{0x65, 0x531C70}};
constexpr sh::CallSite kCalls51DD70[] = {{0x40, 0x531C70}};
constexpr sh::CallSite kCalls521510[] = {{0x61, 0x531C70}};
constexpr sh::CallSite kCalls522560[] = {{0x3E, 0x531CF0}, {0x4F, 0x530530}, {0x67, 0x536700}, {0x92, 0x536700}, {0xBB, 0x536700}};
constexpr sh::CallSite kCalls522FB0[] = {{0x1, 0x589810}, {0x58, 0x5720C0}};
constexpr sh::CallSite kCalls524DA0[] = {{0x29, 0x5725C0}, {0x43, 0x572570}, {0x7A, 0x5725C0}, {0x94, 0x572570}};

#define R0A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R0A_CALLS(a) a, R0A_N(a)
#define R0A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kCa = sh::Shape::kCall;
// The five that answer are read as al by every caller (docs/rest_0a.md section
// 2: 78 call sites, each `test al, al` or `cmp al, 0xFF` first); the two that
// do not answer are compared on the state and the log alone.
const sh::Clone kAll[] = {
    {"PartyAction_TargetAhead", 0x51C390, 0x9F, R0A_CALLS(kCalls51C390), nullptr, 0, nullptr, 0,
     R0A_FN(PartyAction_TargetAhead), 0xFFu, false, kCa, 0},
    {"PartyAction_MemberBeyondEffect", 0x51C6A0, 0x92, R0A_CALLS(kCalls51C6A0), nullptr, 0, nullptr, 0,
     R0A_FN(PartyAction_MemberBeyondEffect), 0xFFu, false, kCa, 0},
    {"PartyAction_MemberOnEffect", 0x51DD70, 0x6A, R0A_CALLS(kCalls51DD70), nullptr, 0, nullptr, 0,
     R0A_FN(PartyAction_MemberOnEffect), 0xFFu, false, kCa, 0},
    {"PartyAction_Kind30Ahead", 0x521510, 0xAA, R0A_CALLS(kCalls521510), nullptr, 0, nullptr, 0,
     R0A_FN(PartyAction_Kind30Ahead), 0xFFu, false, kCa, 0},
    {"PartyAction_BlockedAhead", 0x522560, 0xED, R0A_CALLS(kCalls522560), nullptr, 0, nullptr, 0,
     R0A_FN(PartyAction_BlockedAhead), 0xFFu, false, kCa, 0},
    {"Effect_SpawnAtCellHigh", 0x522FB0, 0x76, R0A_CALLS(kCalls522FB0), nullptr, 0, nullptr, 0,
     R0A_FN(Effect_SpawnAtCellHigh), 0, false, kCa, 0},
    {"PartyAction_SideProbes", 0x524DA0, 0xAF, R0A_CALLS(kCalls524DA0), nullptr, 0, nullptr, 0,
     R0A_FN(PartyAction_SideProbes), 0, false, kCa, 0},
};
#undef R0A_FN
#undef R0A_CALLS
#undef R0A_N

enum : unsigned { kTarget, kBeyond, kOn, kKind30, kBlocked, kSpawn, kSide, kCount };
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
constexpr U kSteps = 0x6697B0;   // Field_DirectionSteps: 8 rows of two longs
unsigned char* Steps() { return sh::Mem(kSteps); }
unsigned char* Effect(unsigned i) { return sh::EffectRecord(i); }

// --- the stand-ins' answers (Noise() and the state only: both passes the same) -----

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }
U WithAx(U answer, U ax) { return (answer & 0xFFFF0000u) | (ax & 0xFFFF); }

// Sprite_ObjectAt: none (0xFF) half the time, else an index 0..0x21 or any byte.
U FxObjectAt(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 4 ? (n >> 12) % 0x22 : n >> 16);
}
// Field_EffectAhead: none half the time, else a record 0..19 or any byte.
U FxEffectAhead(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 2 ? 0xFF : (n >> 8) % 4 ? (n >> 12) % 20 : n >> 16);
}
// AreaMap_ByteAt: each code the two map tests compare with and its neighbours.
U FxMapByte(const U*, U answer) {
    static const U kCodes[] = {0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xEF, 0x00, 0xFF, 0x72};
    const U n = sh::Noise();
    return WithAl(answer, n % 8 == 0 ? n >> 8 : kCodes[(n >> 3) % (sizeof kCodes / sizeof kCodes[0])]);
}
// Effect_FindFree: none a third of the time, else a slot 0..19 (what the real
// one hands out; the original and ours both write the record unchecked).
U FxFindFree(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 3 == 0 ? 0xFF : (n >> 4) % 20);
}
// MapView_SlopeAt: DamageScratch's flag byte 0 a third of the time, else not
// 0; the slope's low word on either side of 0x40 and at the s16 limits.
U FxSlope(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const flag = sh::Mem(bof3::addr::DamageScratch);
    if (sh::InRegions(flag, 1)) flag[0] = static_cast<unsigned char>(n % 3 == 0 ? 0 : (n >> 2) % 4 ? 1 : 1 + (n >> 4) % 0xFF);
    static const U kWords[] = {0x40, 0x41, 0x3F, 0, 0x7FFF, 0x8000, 0xFFFF, 0x8040, 0x0140, 0xFFC0, 0x1000};
    const U m = sh::Noise();
    return WithAx(answer, m % 5 == 0 ? m >> 8 : kWords[(m >> 3) % (sizeof kWords / sizeof kWords[0])]);
}
// MapView_GroundAt: a low word at, one either side of, or far from the
// sprite's height word +0x3E (read now: the same on both passes).
U FxGround(const U*, U answer) {
    const U n = sh::Noise();
    const unsigned char* const s = Sprite_Current;
    const U h = sh::InRegions(s + 0x3E, 2) ? Word(s + 0x3E) : 0;
    static const U kDelta[] = {0, 1, 0xFFFF, 2, 0xFFFE, 0x8000, 0x7FFF};
    return WithAx(answer, n % 5 == 0 ? n >> 8 : h + kDelta[(n >> 3) % (sizeof kDelta / sizeof kDelta[0])]);
}

// Masks by what each callee reads (symbols.toml's types, its evidence): the
// map cells as 16-bit words (AreaMap_ByteAt sign-extends both), the reach
// test's height as a word, every other argument whole. The record pointers the
// reach test is handed are logged by value (the same addresses on both passes:
// ObjTrio's and Effect_Objects' records).
#define R0A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu, kU16 = 0xFFFFu;
const sh::Callee kCallees[] = {
    // unsigned char (long x, long y, unsigned margin)
    {R0A_OURS(Sprite_ObjectAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxObjectAt},
    // unsigned char (short x, short y)
    {R0A_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxMapByte},
    // unsigned char (int x, int y, short z, int margin, const unsigned char *object): 0 a third of the time
    {R0A_OURS(Sprite_PointInReach), 5, {kW, kW, kU16, kW, kW}, sh::Answer::kFlag, 0, 0, {}},
    // unsigned char (void)
    {R0A_OURS(Field_EffectAhead), 0, {}, kG, 0, 0, {}, &FxEffectAhead},
    {R0A_OURS(Effect_FindFree), 0, {}, kG, 0, 0, {}, &FxFindFree},
    // long (long x, long y)
    {R0A_OURS(AreaMap_Elevation), 2, {kW, kW}, kG, 0, 0, {}},
    // long (long x, long y, unsigned long direction): 0x524DA0 pushes 3 and 5 whole
    {R0A_OURS(MapView_SlopeAt), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxSlope},
    // long (long x, long z)
    {R0A_OURS(MapView_GroundAt), 2, {kW, kW}, kG, 0, 0, {}, &FxGround},
};
#undef R0A_OURS

// Beyond field mode's standard regions (which hold Sprite_Current and the
// sprite records, ObjTrio, Field_State, Field_MemberCount, Effect_Objects and
// DamageScratch's first bytes): Field_DirectionSteps, which all seven read
// (0x524DA0 its rows 3 and 5 by address), so that the seed can put every row's
// boundaries in. .data, restored after the run as every region is.
const sh::Region kRegions[] = {
    {kSteps, 0x40},
};

// --- the seed -------------------------------------------------------------------------

// A 16.16 coordinate: a cell at 0, small, at the s16 and u16 limits (where the
// cell one on wraps) or random; a fraction 0 (often), a half, a quarter, 1,
// 0xFFFF or random.
U Coordinate() {
    const U cell = PickOf(0, 1, 2, sh::Next() % 0x80u, sh::Next() % 0x80u, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, sh::Next());
    const U frac = PickOf(0, 0, 0, 0x8000, 0x4000, 1, 0xFFFF, sh::Next());
    return (cell << 16) | (frac & 0xFFFF);
}
// A height word: around 0, the s16 limits, where + 0x200 wraps or crosses the
// sign, or random.
U Height() {
    return PickOf(0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x7E00, 0x7DFF, 0xFE00, 0xFDFF, 0xFFFF - 0x1FF, 0x40, sh::Next() % 0x400u,
                  sh::Next());
}

// Field_DirectionSteps: half the time the exe's shape (half a cell, 0x8000,
// signed per axis, at random per row), else each dword a boundary or random.
void SeedSteps() {
    unsigned char* const t = Steps();
    for (unsigned i = 0; i < 16; ++i) {
        const U v = sh::Half() ? PickOf(0, 0x8000, 0xFFFF8000u)
                               : PickOf(0, 0x8000, 0xFFFF8000u, 0x10000, 0xFFFF0000u, 0x4000, 1, 0xFFFFFFFFu, 0x7FFFFFFFu,
                                        0x80000000u, sh::Next());
        SetLong(t + 4 * i, static_cast<std::int32_t>(v));
    }
}

// Sprite_Current's direction: 0..7 mostly; 8..15 and any byte read the .data
// after the table (in place, the same on both passes).
unsigned char Direction() { return static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : PickOf(8, 9, 15, 0x80, 0xFF, sh::Next())); }

void SeedSprite() {
    unsigned char* const s = Sprite_Current;
    s[8] = Direction();
    SetLong(s + 0x34, static_cast<std::int32_t>(Coordinate()));
    SetLong(s + 0x38, static_cast<std::int32_t>(Coordinate()));
    SetWord(s + 0x3E, Height());
}

// The member count the two member loops walk to: 0, 1 (no member tested), 2,
// 3 (the three ObjTrio records), 4 and rarely any byte (records past ObjTrio's
// three, as the original computes them: the stand-in only logs the pointer).
void SeedCount() { Field_MemberCount = static_cast<unsigned char>(sh::Next() % 16 == 0 ? sh::Next() : PickOf(0, 1, 2, 2, 3, 3, 3, 4)); }

void SeedEffects(bool line_up) {
    const unsigned char* const s = Sprite_Current;
    for (unsigned i = 0; i < 20; ++i) {
        unsigned char* const e = Effect(i);
        e[0] = static_cast<unsigned char>(PickOf(0, 1, 1, 1, sh::Next() | 1));
        e[5] = static_cast<unsigned char>(PickOf(0x30, 0x30, 0x30, 0x17, 0x31, 0x2F, sh::Next()));
        SetLong(e + 0x34, static_cast<std::int32_t>(line_up && sh::Often() && sh::Half() ? static_cast<U>(Long(s + 0x34)) : Coordinate()));
        SetLong(e + 0x38, static_cast<std::int32_t>(line_up && sh::Often() && sh::Half() ? static_cast<U>(Long(s + 0x38)) : Coordinate()));
        SetWord(e + 0x3E, Height());
    }
}

void Seed(unsigned k) {
    SeedSteps();
    SeedSprite();
    switch (k) {
    case kBeyond:
    case kOn:
        SeedCount();
        SeedEffects(false);
        break;
    case kKind30: {
        unsigned char* const state = Field_State;
        state[0x89] = static_cast<unsigned char>(PickOf(2, 2, 2, 2, 0, 1, 3, sh::Next()));
        state[0x138] = static_cast<unsigned char>(PickOf(0, 0, 0, 2, 0xFE, 1, 3, sh::Next()));
        SeedEffects(true);
        break;
    }
    case kSide: Sprite_Current[0x2B] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next())); break;
    default: break;
    }
}

// The argument words, after the seed: the effect index's low byte 0..19 under
// random upper bytes (the callers push the dword a byte was stored into); the
// cell words at their boundaries under random upper halves, the fourth dword
// the callers push left random.
void Args(unsigned k, U* a) {
    switch (k) {
    case kBeyond:
    case kOn: a[0] = (a[0] & 0xFFFFFF00u) | (sh::Next() % 20); break;
    case kSpawn:
        a[1] = (a[1] & 0xFFFF0000u) | (PickOf(0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x40, sh::Next()) & 0xFFFF);
        a[2] = (a[2] & 0xFFFF0000u) | (PickOf(0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x40, sh::Next()) & 0xFFFF);
        break;
    default: break;
    }
}

// What the seven read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): Sprite_Current's direction,
// position, height and +0x2B; the member count; an effect record's position,
// height, in-use byte and kind; a row of Field_DirectionSteps; Field_State's
// two bytes (read once, before any call).
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 10) {
    case 0: s[8] = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) & 7); break;
    case 1: SetLong(s + (v & 1 ? 0x34 : 0x38), static_cast<std::int32_t>(v << 7)); break;
    case 2: SetWord(s + 0x3E, v >> 2); break;
    case 3: Field_MemberCount = static_cast<unsigned char>(v % 5); break;
    case 4: {
        unsigned char* const e = Effect(v % 20);
        static const unsigned kAt[] = {0x34, 0x38, 0x3E};
        const unsigned at = kAt[(v >> 5) % 3];   // from the hash: Next() is the seed's stream
        if (at == 0x3E) SetWord(e + at, v >> 5);
        else SetLong(e + at, static_cast<std::int32_t>(v << 3));
        break;
    }
    case 5: {
        unsigned char* const e = Effect(v % 20);
        if ((v >> 5) & 1) e[0] = static_cast<unsigned char>((v >> 6) & 1);
        else e[5] = static_cast<unsigned char>((v >> 6) & 1 ? 0x30 : v >> 7);
        break;
    }
    case 6: SetLong(Steps() + 4 * (v % 16), static_cast<std::int32_t>(v << 6)); break;
    case 7: {
        unsigned char* const state = Field_State;
        if (sh::InRegions(state + 0x89, 1) && sh::InRegions(state + 0x138, 1)) state[v & 1 ? 0x89 : 0x138] = static_cast<unsigned char>(v >> 1);
        break;
    }
    case 8: s[0x2B] = static_cast<unsigned char>(v); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R0A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R0A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_0a: BOF3X_R0A_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_0a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], nullptr, 0,
                   kRegions, sizeof kRegions / sizeof kRegions[0], [](unsigned k) { Seed(s_index[k]); }, &Disturb,
                   20000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_0a

// BOF3X_SHADOW=battle_odds: a differential fuzz of group CK's ten functions
// against byte-copies of Capcom's, once at start-up (docs/battle_odds.md
// section 3). Every call and tail jump out of a copy is re-aimed at a recording
// stand-in, and ours is put on the same stand-ins through battle_odds::g. The
// log is the vertex-block harness's (d3d_fuzz.h), used for its Record /
// SameLog only.
//
// Per round: the state a function reads, random with its boundaries seeded;
// Capcom's copy, then ours, from the same state and with the same eax and ecx
// on entry (two of the originals push a byte in a register whose upper bits
// are the entry's); the calls out with their arguments, the whole eax and
// every region either side could write compared.
//
// The stand-ins change, now and then, what the caller reads after the call -
// the banner pool's current entry, Sprite_Current, the result record's
// pointer, the round's flags, the window bytes stored after it - so a value
// read or a store made on the wrong side of a call shows.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_odds_callees.h"
#include "game/d3d_fuzz.h"
#include "hook/detour.h"
#include "hook/log.h"

// Calls fn() with eax and ecx set on entry; returns eax.
extern "C" __attribute__((naked)) std::uint32_t __cdecl BattleOdds_CallRegs(const void*, std::uint32_t,
                                                                             std::uint32_t) {
    asm("pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl 12(%ebp), %eax\n\t"
        "movl 16(%ebp), %ecx\n\t"
        "call *8(%ebp)\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "movl %ebp, %esp\n\t"
        "popl %ebp\n\t"
        "ret");
}

namespace battle_odds {
namespace {

using d3d_fuzz::Next;
using d3d_fuzz::Record;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U GetLong(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
U Pick(std::initializer_list<U> seeds) { return seeds.begin()[Next() % seeds.size()]; }
U PickH(U h, std::initializer_list<U> seeds) { return seeds.begin()[h % seeds.size()]; }
bool OneIn(U n) { return Next() % n == 0; }

// --- addresses ------------------------------------------------------------------------
const U kSprite = at::Sprite_CurrentAt();       // 0x937F88
const U kTints = at::TintRecordsAt();           // 0x7E0700
const U kScratch = bof3::addr::DamageScratch;   // 0x903850
constexpr U kWindows = 0x803160;                // WindowRecords 0..4
constexpr U kWindowBytes = 5 * 0x24;
constexpr U kTintBytes = 0xC00;

// Buffers of the fuzz's own.
unsigned char g_objects[0x200];   // effect objects Sprite_Current points at: 16 of 0x20
unsigned char g_results[0x80];    // result records

// --- regions --------------------------------------------------------------------------
struct Region {
    U at, size;
};
constexpr U kMaxState = 0x2000;
struct State {
    unsigned char bytes[kMaxState];
};
U RegionBytes(const Region* r, int n) {
    U total = 0;
    for (int i = 0; i < n; ++i) total += r[i].size;
    return total;
}
void Capture(const Region* r, int n, State& s) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        std::memcpy(s.bytes + at, At(r[i].at), r[i].size);
        at += r[i].size;
    }
}
void Restore(const Region* r, int n, const State& s) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        std::memcpy(At(r[i].at), s.bytes + at, r[i].size);
        at += r[i].size;
    }
}
bool FirstDifference(const Region* r, int n, const State& a, const State& b, U* where) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        for (U k = 0; k < r[i].size; ++k)
            if (a.bytes[at + k] != b.bytes[at + k]) {
                *where = r[i].at + k;
                return true;
            }
        at += r[i].size;
    }
    return false;
}

// Every game region any test touches, saved once and put back at the end.
const Region g_all[] = {
    {kBannerCurrent, 4}, {kBanners, 0x60}, {kBannerKinds, 1}, {kScratch, 1},       {kWindows, kWindowBytes},
    {kResult, 4},        {kTarget, 1},     {kActing, 4},      {kActorObject, 4},   {kFlagTarget, 1},
    {kRoundFlags, 4},    {kSprite, 4},     {kTints, kTintBytes},
};

// Every region a function could write, the game's and the fuzz's.
const Region g_writes[] = {
    {kBannerCurrent, 4}, {kBanners, 0x60},  {kBannerKinds, 1}, {kScratch, 1},
    {kWindows, kWindowBytes}, {kResult, 4}, {kRoundFlags, 4},  {kActorObject, 4},
    {kSprite, 4},        {kTints, kTintBytes}, {0, sizeof g_objects}, {0, sizeof g_results},
};
Region g_regions[sizeof g_writes / sizeof g_writes[0]];
constexpr int kRegions = static_cast<int>(sizeof g_writes / sizeof g_writes[0]);

// --- the stand-ins ----------------------------------------------------------------------

U g_round;
int g_test;
enum Test { kTestNone, kTestKind1, kTestKind2, kTestHalf, kTestTint, kTestSize, kTestFinish };

U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (d3d_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}
// A byte answer with noise above it: mostly 0 or 1, then 0x80 and 0xFF.
U ByteAnswer(U salt) {
    const U h = Mix(salt);
    const U k = (h >> 3) % 10;
    const U al = k < 4 ? 0 : k < 8 ? 1 : k == 8 ? 0x80 : 0xFF;
    return (h & 0xFFFFFF00u) | al;
}
U SomeObject(U h) { return Addr(g_objects) + (h % 16) * 0x20; }

// BattleBanner_NoneOfKind: the kind; it may move the current entry, change the
// kinds seen or the window bytes the handlers store after it.
U __cdecl StubNoneOfKind(U kind) {
    Record(1, kind, GetLong(At(kBannerCurrent)), At(kBannerKinds)[0]);
    const U h = Mix(1);
    switch ((h >> 4) % 5) {
    case 0: PutLong(At(kBannerCurrent), kBanners + ((h >> 8) % 8) * 0xC); break;
    case 1: At(kBannerKinds)[0] = static_cast<unsigned char>(h >> 12); break;
    case 2: At(kWindows + 3)[0] = static_cast<unsigned char>(h >> 16); At(kWindows + 4 * 0x24 + 3)[0] = static_cast<unsigned char>(h >> 20); break;
    default: break;
    }
    return ByteAnswer(2);
}
// Battle_CalcDamage reads its first two arguments' low bytes (battle_damage.cpp).
// Its answer's low word seeded at the shift's edges; it may move the result
// record, which the handler reads after it.
U __cdecl StubCalcDamage(U attacker, U target, U element) {
    Record(2, attacker & 0xFF, target & 0xFF, element);
    const U h = Mix(3);
    if (h % 4 == 0) PutLong(At(kResult), Addr(g_results) + ((h >> 8) % 0x38));
    const U low = (h >> 12) % 2 ? PickH(h >> 14, {0, 1, 2, 3, 0x7FFF, 0x8000, 0x8001, 0xFFFF, 0xFFFE, 9999, 0xD8F1})
                                : (h >> 14) & 0xFFFF;
    return (Mix(4) & 0xFFFF0000u) | low;
}
// BattleTask_FreeCurrent: what the caller did before its tail jump shows.
U __cdecl StubFreeTask() {
    Record(3, GetLong(At(kSprite)), GetLong(At(kRoundFlags)));
    return Mix(5);
}
// Sprite_ReleaseTint and Sprite_SetTint: the object; each may move
// Sprite_Current (read after both) or the actor's object (read before).
void DisturbSprite(U salt) {
    const U h = Mix(salt);
    switch ((h >> 4) % 4) {
    case 0: PutLong(At(kSprite), SomeObject(h >> 8)); break;
    case 1: PutLong(At(kActorObject), SomeObject(h >> 8)); break;
    default: break;
    }
}
U __cdecl StubReleaseTint(U sprite) {
    Record(4, sprite);
    DisturbSprite(6);
    return Mix(7);
}
U __cdecl StubSetTint(U sprite, U r, U gg, U b, U a) {
    Record(5, sprite, r & 0xFF, gg & 0xFF, b & 0xFF, a & 0xFF);
    DisturbSprite(8);
    return Mix(9);
}
// BattleActor_FxSize: the size in al; Sprite_Current may move.
U __cdecl StubFxSize() {
    Record(6, GetLong(At(kSprite)));
    DisturbSprite(10);
    return Mix(11);
}
// Battle_SetTargetFlag40 reads its argument's low byte (battle_sprites.cpp);
// it may change the round's flags, or'ed after it.
U __cdecl StubSetFlag40(U target) {
    Record(7, target & 0xFF, GetLong(At(kRoundFlags)));
    const U h = Mix(12);
    if (h % 3 == 0) At(kRoundFlags)[0] = static_cast<unsigned char>(h >> 8);
    return Mix(13);
}

const Callees kStandIns = {
    StubNoneOfKind, StubCalcDamage, StubFreeTask, StubReleaseTint, StubSetTint, StubFxSize, StubSetFlag40,
};

// --- the copies -------------------------------------------------------------------------
// Every E8 and E9 of each body, by capstone 2026-09-25 (docs/battle_odds.md
// section 1); every other jump stays inside.

const void* StubFor(U target) {
    switch (target) {
    case 0x44A830: return As<const void*>(&StubNoneOfKind);
    case 0x445CF0: return As<const void*>(&StubCalcDamage);
    case 0x4351F0: return As<const void*>(&StubFreeTask);
    case 0x454DC0: return As<const void*>(&StubReleaseTint);
    case 0x454CC0: return As<const void*>(&StubSetTint);
    case 0x4FC1F0: return As<const void*>(&StubFxSize);
    case 0x4530D0: return As<const void*>(&StubSetFlag40);
    default: bof3::Fatal("battle_odds: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

struct Site {
    U offset, target;
};
constexpr Site kKind1Sites[] = {{0x4C, 0x44A830}};
constexpr Site kKind2Sites[] = {{0x37, 0x44A830}};
constexpr Site kHalfSites[] = {{0x12, 0x445CF0}};
constexpr Site kFreeSites[] = {{0x0, 0x4351F0}};
constexpr Site kTintSites[] = {{0x21, 0x454DC0}, {0x2F, 0x454CC0}};
constexpr Site kSizeSites[] = {{0x0, 0x4FC1F0}};
constexpr Site kFinishSites[] = {{0x13, 0x4530D0}, {0x22, 0x4351F0}};

void* Clone(const char* name, U base, U size, const Site* sites, int n) {
    bof3::CloneCall calls[4];
    if (n > 4) bof3::Fatal("battle_odds: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) calls[i] = {sites[i].offset, StubFor(sites[i].target), sites[i].target};
    void* code = bof3::CloneOriginal(name, base, size, calls, n);
    if (!code) bof3::Fatal("battle_odds: CloneOriginal(%s) returned null", name);
    return code;
}
template <int N> void* Clone(const char* name, U base, U size, const Site (&sites)[N]) {
    return Clone(name, base, size, sites, N);
}

// --- the comparison ---------------------------------------------------------------------

d3d_fuzz::Log g_theirs, g_ours;
State g_start, g_after_theirs, g_after_ours;

struct Tally {
    const char* name;
    unsigned rounds, bad, calls, hits;   // hits: a coverage count each test defines
};

// One round: state already generated. Capcom's copy then ours from the same
// state and registers; the log, eax and every written region compared.
void Pass(Tally& t, const void* theirs, const void* ours) {
    const U eax = Next(), ecx = Next();
    Capture(g_regions, kRegions, g_start);
    g_theirs.Clear();
    d3d_fuzz::g_log = &g_theirs;
    const U ret_theirs = BattleOdds_CallRegs(theirs, eax, ecx);
    Capture(g_regions, kRegions, g_after_theirs);

    Restore(g_regions, kRegions, g_start);
    g_ours.Clear();
    d3d_fuzz::g_log = &g_ours;
    const U ret_ours = BattleOdds_CallRegs(ours, eax, ecx);
    Capture(g_regions, kRegions, g_after_ours);
    d3d_fuzz::g_log = nullptr;

    ++t.rounds;
    t.calls += g_theirs.n;
    char why[200] = "";
    bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why);
    if (same && ret_ours != ret_theirs) {
        std::snprintf(why, sizeof why, "the result %08X, the original %08X", (unsigned)ret_ours, (unsigned)ret_theirs);
        same = false;
    }
    U where = 0;
    if (same && FirstDifference(g_regions, kRegions, g_after_ours, g_after_theirs, &where)) {
        std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
        same = false;
    }
    if (!same) {
        if (t.bad < 4) bof3::Log("shadow      battle_odds MISMATCH: %s round %u: %s", t.name, t.rounds - 1, why);
        ++t.bad;
    }
}

void FillRandom(U at, U bytes) {
    for (U i = 0; i < bytes; ++i) At(at)[i] = static_cast<unsigned char>(Next());
}
bool Called(const d3d_fuzz::Log& log, U what) {
    for (unsigned c = 0; c < log.n && c < d3d_fuzz::kMaxCalls; ++c)
        if (log.calls[c].what == what) return true;
    return false;
}

// --- the tests --------------------------------------------------------------------------

// The pool, the current entry one of its eight; its timer at the edges.
void RandomBanner() {
    FillRandom(kBanners, 0x60);
    FillRandom(kWindows, kWindowBytes);
    At(kBannerKinds)[0] = static_cast<unsigned char>(Next() % 2 ? Pick({0, 1, 2, 3, 4, 0xFF}) : Next());
    At(kScratch)[0] = static_cast<unsigned char>(Next());
    const U entry = kBanners + (Next() % 8) * 0xC;
    PutLong(At(kBannerCurrent), entry);
    At(entry + 1)[0] = static_cast<unsigned char>(Next() % 3 ? Pick({1, 2, 0, 3, 4}) : Next());
    PutWord(At(entry + 8), Next() % 4 ? Pick({0, 1, 1, 1, 2, 0xFF, 0x100, 0xFE, 0x1FF, 0xFFFF, 0x8000}) : Next());
}

void FuzzKind(Tally& t, const void* clone, const void* ours, unsigned rounds, U salt, int test) {
    g_test = test;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = salt + r;
        RandomBanner();
        Pass(t, clone, ours);
        t.hits += Called(g_theirs, 1);
    }
    g_test = kTestNone;
}

void FuzzHeal(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x300000u + r;
        FillRandom(Addr(g_results), sizeof g_results);
        PutLong(At(kResult), Addr(g_results) + Next() % 0x38);
        Pass(t, clone, As<const void*>(&Effect_Heal20));
    }
}

void FuzzHalf(Tally& t, const void* clone, unsigned rounds) {
    g_test = kTestHalf;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x400000u + r;
        FillRandom(Addr(g_results), sizeof g_results);
        PutLong(At(kResult), Addr(g_results) + Next() % 0x38);
        PutLong(At(kActing), Next() % 2 ? (Next() & 0xFFFF0000u) | (Next() % 11) : Next());
        At(kTarget)[0] = static_cast<unsigned char>(Next() % 2 ? Next() % 11 : Next());
        Pass(t, clone, As<const void*>(&Effect_HalfAttack));
    }
    g_test = kTestNone;
}

void FuzzFree(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x500000u + r;
        PutLong(At(kSprite), Next());
        PutLong(At(kRoundFlags), Next());
        Pass(t, clone, As<const void*>(&BattleFx_FreeTask));
    }
}

// Effect objects with the fields the states test at their edges.
void RandomObjects() {
    FillRandom(Addr(g_objects), sizeof g_objects);
    for (U k = 0; k < 16; ++k) {
        unsigned char* const o = g_objects + k * 0x20;
        o[9] = static_cast<unsigned char>(Next() % 3 ? Pick({0, 1, 2, 0xFF, 8}) : Next());
        o[0xB] = static_cast<unsigned char>(Next() % 3 ? Pick({0, 1, 3, 4, 5, 6, 0x7F, 0x80, 0xFF}) : Next());
    }
}

void FuzzTint(Tally& t, const void* clone, unsigned rounds) {
    g_test = kTestTint;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x600000u + r;
        RandomObjects();
        PutLong(At(kSprite), SomeObject(Next()));
        PutLong(At(kActorObject), OneIn(4) ? Next() : SomeObject(Next()));
        Pass(t, clone, As<const void*>(&BattleFx_TintActor));
        t.hits += Called(g_theirs, 5);
    }
    g_test = kTestNone;
}

void FuzzBrighten(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x700000u + r;
        if (r % 8 == 0) FillRandom(kTints, kTintBytes);
        RandomObjects();
        U s = SomeObject(Next());
        if (OneIn(3)) {
            // The object inside the records: a byte the loop steps is its +0xA
            // (read again for each colour) or its +9.
            const U index = 1 + Next() % 200;   // from 1: the object from kTints + 4 on, inside the region
            const U byte = kTints + index * 12 + 2 + Next() % 3;
            s = byte - Pick({0xA, 0xA, 9, 1});
            At(s + 0xA)[0] = static_cast<unsigned char>(Next() % 2 ? index : Next());
            t.hits += 1;
        }
        PutLong(At(kSprite), s);
        Pass(t, clone, As<const void*>(&BattleFx_Brighten));
    }
}

void FuzzSize(Tally& t, const void* clone, unsigned rounds) {
    g_test = kTestSize;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x800000u + r;
        RandomObjects();
        PutLong(At(kSprite), SomeObject(Next()));
        Pass(t, clone, As<const void*>(&BattleFx_SetSize));
    }
    g_test = kTestNone;
}

void FuzzWait(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x900000u + r;
        RandomObjects();
        PutLong(At(kSprite), SomeObject(Next()));
        Pass(t, clone, As<const void*>(&BattleFx_WaitStep4));
        t.hits += At(GetLong(At(kSprite)) + 0xB)[0] <= 4;
    }
}

void FuzzFinish(Tally& t, const void* clone, unsigned rounds) {
    g_test = kTestFinish;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0xA00000u + r;
        RandomObjects();
        const U s = SomeObject(Next());
        if (Next() % 2) At(s + 0xB)[0] = 0;
        PutLong(At(kSprite), s);
        At(kFlagTarget)[0] = static_cast<unsigned char>(Next() % 2 ? Pick({0, 1, 2, 3, 0x40, 0x80, 0xC0}) : Next());
        PutLong(At(kRoundFlags), Next());
        Pass(t, clone, As<const void*>(&BattleFx_Finish));
        t.hits += Called(g_theirs, 7);
    }
    g_test = kTestNone;
}

}  // namespace

void SelfTest() {
    // The copies, before BattleOdds_Inject patches anything.
    const void* kind1 = Clone("BattleBanner_TickKind1", 0x44A740, 0x6D, kKind1Sites);
    const void* kind2 = Clone("BattleBanner_TickKind2", 0x44A7B0, 0x58, kKind2Sites);
    const void* heal = Clone("Effect_Heal20", 0x44C140, 0xC, nullptr, 0);
    const void* half = Clone("Effect_HalfAttack", 0x44C990, 0x28, kHalfSites);
    const void* free_task = Clone("BattleFx_FreeTask", 0x4AEE90, 0x5, kFreeSites);
    const void* tint = Clone("BattleFx_TintActor", 0x4B1E70, 0x54, kTintSites);
    const void* brighten = Clone("BattleFx_Brighten", 0x4B1ED0, 0x65, nullptr, 0);
    const void* size = Clone("BattleFx_SetSize", 0x4ED5C0, 0x17, kSizeSites);
    const void* wait = Clone("BattleFx_WaitStep4", 0x4EE8A0, 0x18, nullptr, 0);
    const void* finish = Clone("BattleFx_Finish", 0x4F7350, 0x28, kFinishSites);

    for (int i = 0; i < kRegions; ++i) g_regions[i] = g_writes[i];
    g_regions[kRegions - 2].at = Addr(g_objects);
    g_regions[kRegions - 1].at = Addr(g_results);

    const int n_all = static_cast<int>(sizeof g_all / sizeof g_all[0]);
    static State saved;
    if (RegionBytes(g_all, n_all) > kMaxState || RegionBytes(g_regions, kRegions) > kMaxState)
        bof3::Fatal("battle_odds: the saved regions outgrow the state buffer");
    Capture(g_all, n_all, saved);
    const Callees saved_callees = g;
    g = kStandIns;
    d3d_fuzz::Seed(0x434B434Bu);

    Tally tallies[] = {
        {"BattleBanner_TickKind1", 0, 0, 0, 0}, {"BattleBanner_TickKind2", 0, 0, 0, 0},
        {"Effect_Heal20", 0, 0, 0, 0},          {"Effect_HalfAttack", 0, 0, 0, 0},
        {"BattleFx_FreeTask", 0, 0, 0, 0},      {"BattleFx_TintActor", 0, 0, 0, 0},
        {"BattleFx_Brighten", 0, 0, 0, 0},      {"BattleFx_SetSize", 0, 0, 0, 0},
        {"BattleFx_WaitStep4", 0, 0, 0, 0},     {"BattleFx_Finish", 0, 0, 0, 0},
    };
    FuzzKind(tallies[0], kind1, As<const void*>(&BattleBanner_TickKind1), 10000, 0x100000u, kTestKind1);
    FuzzKind(tallies[1], kind2, As<const void*>(&BattleBanner_TickKind2), 10000, 0x200000u, kTestKind2);
    FuzzHeal(tallies[2], heal, 1000);
    FuzzHalf(tallies[3], half, 5000);
    FuzzFree(tallies[4], free_task, 1000);
    FuzzTint(tallies[5], tint, 5000);
    FuzzBrighten(tallies[6], brighten, 5000);
    FuzzSize(tallies[7], size, 3000);
    FuzzWait(tallies[8], wait, 3000);
    FuzzFinish(tallies[9], finish, 5000);

    g = saved_callees;
    Restore(g_all, n_all, saved);

    unsigned bad = 0, rounds = 0;
    for (const Tally& t : tallies) {
        bad += t.bad;
        rounds += t.rounds;
        bof3::Log("shadow      battle_odds self-test: %s %u rounds, %u calls out, %u covered, %u MISMATCHES", t.name,
                  t.rounds, t.calls, t.hits, t.bad);
    }
    bof3::Log("shadow      battle_odds self-test: %u rounds over 10 functions, %u MISMATCHES", rounds, bad);
    if (bad) bof3::Fatal("group CK's functions differ from the original in %u of %u self-test rounds", bad, rounds);
}

}  // namespace battle_odds

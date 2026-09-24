// BOF3X_SHADOW=battle_misc: a differential fuzz of group BF's twenty-seven
// functions against byte-copies of Capcom's, once at start-up
// (docs/battle_misc.md section 3). Every call out of a copy is re-aimed at a
// recording stand-in - the calls among the group's own functions included, so
// each function is tested alone - and ours is put on the same stand-ins through
// battle_misc::g. BattleBanner_Dispatch's call table is built from immediates on
// its stack, so its copy has those immediates re-aimed; EnemyAI_TurnCheck's jump
// table is moved into its copy. The log is the vertex-block harness's
// (d3d_fuzz.h), used for its Record / SameLog only.
//
// Per round: the state a function reads, random with its boundaries seeded;
// Capcom's copy, then ours, from the same state and with the same eax and ecx
// on entry (BattleMisc_CallRegs - two of the originals return a register they
// never wrote, and others pass stale register bits on); the calls out with
// their arguments, the result (masked to what the callers read) and every
// region either side could write compared.
//
// The stand-ins write what the real callees write where the caller reads it
// again (Str_CopyN's copy, which BattleBanner_ShowName's inline strcat scans),
// and now and then change something the caller reads after the call - the
// current sprite or enemy, a later object's flag or type byte, a later banner
// entry, a later AI row, the item cursor - so a value read on the wrong side of
// a call shows.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_misc_callees.h"
#include "game/d3d_fuzz.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

// Calls fn(a0..a5) with eax and ecx set on entry; returns eax.
extern "C" __attribute__((naked)) std::uint32_t __cdecl BattleMisc_CallRegs(const void*, std::uint32_t,
                                                                             std::uint32_t, std::uint32_t,
                                                                             std::uint32_t, std::uint32_t,
                                                                             std::uint32_t, std::uint32_t,
                                                                             std::uint32_t) {
    asm("pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl 40(%ebp)\n\t"
        "pushl 36(%ebp)\n\t"
        "pushl 32(%ebp)\n\t"
        "pushl 28(%ebp)\n\t"
        "pushl 24(%ebp)\n\t"
        "pushl 20(%ebp)\n\t"
        "movl 12(%ebp), %eax\n\t"
        "movl 16(%ebp), %ecx\n\t"
        "call *8(%ebp)\n\t"
        "movl %ebp, %esp\n\t"
        "popl %ebp\n\t"
        "ret");
}

namespace battle_misc {
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
bool OneIn(U n) { return Next() % n == 0; }
// A byte in the low bits, noise above it half the time.
U Stale(U low_byte) { return Next() % 2 ? (Next() & 0xFFFFFF00u) | (low_byte & 0xFF) : low_byte & 0xFF; }

// --- addresses ------------------------------------------------------------------------
const U kSprite = at::Sprite_CurrentAt();       // 0x937F88
const U kFieldState = at::Field_StateAt();      // 0x905D98
const U kParty = at::ObjTrioAt();               // 0x802D40
const U kObjects = at::Sprite_ObjectsAt();      // 0x7DEE80
const U kScratch = bof3::addr::DamageScratch;   // 0x903850
constexpr U kPartyBytes = 3 * kPartyStride;
constexpr U kEnemyBytes = 8 * kEnemyStride;
constexpr U kFuzzSlots = 48;                    // contexts the stand-in hands out: below the banner pool
constexpr U kContextBytes = kFuzzSlots * kContextStride;
constexpr U kObjectBytes = 30 * 0xA4;
constexpr U kFuzzScripts = 0x40;
constexpr U kScriptBytes = kFuzzScripts * 0x8C;

// Buffers of the fuzz's own.
unsigned char g_record[0x100];     // Field_State's record, the acting context, the command source
unsigned char g_items[0x200];      // an inventory page
unsigned char g_lists[0x200];      // the sound lists
unsigned char g_strings[0x200];    // message strings and the name suffix
unsigned char g_actor[0x100];      // BattleBanner_ShowName's actor
unsigned char g_enemy[0x200];      // a current enemy outside the objects
unsigned char g_str[0x200];        // Str_CopyN's

// --- regions --------------------------------------------------------------------------
struct Region {
    U at, size;
};
constexpr U kMaxState = 0x10000;
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
    {kPulseDown, 8},        {kFieldState, 4},        {kSprite, 4},           {kContexts, 0x2AC0},
    {kParty, kPartyBytes},  {kCurrentEnemy, 4},      {kSoundLists, 0x400},   {kActorBits, 2},
    {kItemWindow, 0x90},    {kItemWindowExtra, 3},   {kWindowCurrent, 4},    {kItemActor, 1},
    {kActingActor, 4},      {kActorPages, 0x300},    {kCommandSource, 4},    {kPartyPages, 3},
    {kBannerKinds, 1},      {kScratch, 1},           {kBannerText, 0x40},    {kMessages, 0x400},
    {kSuffixOn, 1},         {kAiScripts, kScriptBytes}, {kActingKind, 1},    {kObjects, kObjectBytes},
};

// --- the stand-ins ----------------------------------------------------------------------

U g_round;
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

// The current test's context, for the disturbances.
int g_test;           // which test is running (the stand-ins' disturbances depend on it)
U g_slot_low;         // the slots the context stand-in hands out: g_slot_low + (0 .. g_slot_span - 1)
U g_slot_span;
U g_script;           // EnemyAI_TurnCheck's script address

enum Test {
    kTestNone,
    kTestContextOne,
    kTestContexts,
    kTestCopies,
    kTestCanUse,
    kTestFree,
    kTestDispatch,
    kTestMessage,
    kTestName,
    kTestAi,
    kTestScreens,
};

// A valid object for Sprite_Current or the current enemy.
// From a hash, not the fuzz's generator: a stand-in runs once per side, and
// both sides must see the same disturbance.
U SomeEnemy(U h) { return kEnemyObjects + (h % 8) * kEnemyStride; }
U SomeObject(U h) {
    switch (h % 3) {
    case 0: return kParty + ((h >> 4) % 3) * kPartyStride;
    case 1: return SomeEnemy(h >> 4);
    default: return Addr(g_enemy) + ((h >> 4) % 4) * 4;
    }
}
U SomeEnemy() { return SomeEnemy(Next()); }
U SomeObject() { return SomeObject(Next()); }
U PickH(U h, std::initializer_list<U> seeds) { return seeds.begin()[h % seeds.size()]; }

U __cdecl StubAlloc(U kind, U mode) {
    Record(1, kind, mode, GetLong(At(kSprite)));
    const U h = Mix(1);
    if (g_test == kTestContextOne || g_test == kTestCopies) {
        if (h % 5 == 0) PutLong(At(kSprite), SomeObject(h >> 20));   // read after the call by InitActorContext
    }
    if (g_test == kTestContexts || g_test == kTestCopies) {
        // a later actor's flag bits
        const U k = (h >> 8) % 11;
        const U object = k < 3 ? kParty + k * kPartyStride : kEnemyObjects + (k - 3) * kEnemyStride;
        if (h % 4 == 1) At(object)[0] ^= 1;
        if (g_test == kTestCopies && h % 4 == 2) At(object + (k < 3 ? 0x134 : 0x114))[0] ^= 8;
    }
    const U slot = g_slot_low + (h >> 12) % (g_slot_span ? g_slot_span : 1);
    return (h & 0xFFFFFF00u) | slot;
}
U __cdecl StubActorOut(U actor) {
    Record(2, actor & 0xFF);
    const U h = Mix(2);
    // the object's +0x114 is read after the call
    const U index = actor & 0xFF;
    if (index >= 3 && index <= 10 && h % 5 == 0) At(kEnemyObjects + (index - 3) * kEnemyStride + 0x114)[0] ^= 8;
    return ByteAnswer(2);
}
U __cdecl StubTint(U sprite, U r, U gg, U b, U a) {
    Record(3, sprite, r, gg, b, a);
    return Mix(3);
}
U __cdecl StubEffect(U id) {
    Record(4, id);
    return Mix(4);
}
U __cdecl StubStream(U id) {
    Record(5, id);
    return Mix(5);
}
// 0x591E50: an inventory page - the fuzz's own buffer, or somewhere in the
// party records or the lists below them (so the pointer's upper half varies).
// It may change the actor byte and the cursor, both read after it.
U __cdecl StubItemList(U actor, U page, U flag) {
    Record(6, actor & 0xFF, page & 0xFF, flag & 0xFF);
    const U h = Mix(6);
    if (h % 5 == 0) At(kItemActor)[0] = static_cast<unsigned char>(h >> 8);
    if (h % 7 == 0) At(kItemWindowCursor)[0] = static_cast<unsigned char>(h >> 16);
    switch ((h >> 24) % 3) {
    case 0: return Addr(g_items) + (h >> 4) % 0x40;
    case 1: return kParty + (h >> 4) % 0x100;
    default: return 0x903A70 + (h >> 4) % 0x100;
    }
}
U __cdecl StubUsable(U mode, U actor, U item) {
    Record(7, mode, actor, item);
    return ByteAnswer(7);
}
U __cdecl StubFree() {
    Record(8, GetLong(At(kWindowCurrent)));
    const U h = Mix(8);
    if (h % 4 == 0) PutLong(At(kWindowCurrent), h);   // the loop keeps its own pointer
    return Mix(9);
}

// The AI's callees. Each may move the current enemy (read again after the
// test and between the calls), change a later row's opcode, the acting kind or
// the enemy's fields.
void DisturbAi(U salt) {
    const U h = Mix(salt);
    switch ((h >> 4) % 8) {
    case 0: PutLong(At(kCurrentEnemy), (h >> 8) % 2 ? SomeEnemy(h >> 12) : Addr(g_enemy)); break;
    case 1: At(g_script + ((h >> 8) % 4) * 16)[0] = static_cast<unsigned char>(PickH(h >> 12, {0, 5, 9, 0xA, 0x16, 0x21, 0x25, 0x30})); break;
    case 2: At(kActingKind)[0] = static_cast<unsigned char>(PickH(h >> 8, {0, 1, 4})); break;
    case 3: {
        unsigned char* const e = At(GetLong(At(kCurrentEnemy)));
        e[0x92] ^= static_cast<unsigned char>(h >> 16);
        PutWord(e + 0x108, h >> 12);
        break;
    }
    default: break;
    }
}
U __cdecl StubAiCondition(U mask) {
    Record(9, mask);
    DisturbAi(9);
    return ByteAnswer(10);
}
U __cdecl StubAiRowDone(U actor, U row) {
    Record(10, actor, row & 0xFF);
    DisturbAi(11);
    return ByteAnswer(12);
}
U __cdecl StubAiApply(U actor, U row) {
    Record(11, actor, row);
    DisturbAi(13);
    return Mix(14);
}
U __cdecl StubAiSetDone(U actor, U row, U flag) {
    Record(12, actor, row & 0xFF, flag);
    DisturbAi(15);
    return Mix(16);
}
U __cdecl StubAiFinish() {
    Record(13);
    return Mix(17);
}

// The screen updates: Sprite_Current recorded; a later object's type or flag,
// or Sprite_Current, may change (the walk sets it again for each object).
void DisturbObjects(U salt) {
    const U h = Mix(salt);
    const U object = kObjects + ((h >> 8) % 30) * 0xA4;
    switch ((h >> 4) % 6) {
    case 0: At(object)[0] ^= 1; break;
    case 1: At(object + 6)[0] = static_cast<unsigned char>(PickH(h >> 16, {7, 8, 9, 0xA, 6, 0})); break;
    case 2: PutLong(At(kSprite), h); break;
    default: break;
    }
}
U __cdecl StubScreenA() {
    Record(14, GetLong(At(kSprite)));
    DisturbObjects(18);
    return Mix(19);
}
U __cdecl StubScreenSlot() {
    Record(15, GetLong(At(kSprite)));
    DisturbObjects(20);
    return Mix(21);
}
// Battle_InitActorContext, under Battle_InitActorContexts: the pointers it was
// handed; a later object's flag may change.
U __cdecl StubInitContext() {
    Record(16, GetLong(At(kSprite)), GetLong(At(kFieldState)), GetLong(At(kCurrentEnemy)));
    const U h = Mix(22);
    const U k = (h >> 8) % 11;
    const U object = k < 3 ? kParty + k * kPartyStride : kEnemyObjects + (k - 3) * kEnemyStride;
    if (h % 3 == 0) At(object)[0] ^= 1;
    return Mix(23);
}
// Str_CopyN, as the real one: at most n & 0xFF bytes to the NUL, then a NUL;
// returns where the NUL went. It may flip the suffix switch or move the suffix,
// both read after it by BattleBanner_ShowName.
U __cdecl StubCopyN(U dst, U src, U n) {
    Record(17, dst, src, n);
    U d = dst, s = src;
    for (U k = 0; k < (n & 0xFF) && At(s)[0]; ++k) At(d++)[0] = At(s++)[0];
    At(d)[0] = 0;
    const U h = Mix(24);
    if (g_test == kTestName) {
        if (h % 5 == 0) At(kSuffixOn)[0] ^= 1;
        if (h % 7 == 0) PutLong(At(kNameSuffix), Addr(g_strings) + 0x100 + (h >> 8) % 0x40);
    }
    return (h % 3 == 0) ? Mix(25) : d;
}
U __cdecl StubBannerSet(U slot, U kind, U b2, U layer, U timer, U text) {
    Record(18, slot, kind, b2, layer, timer, text);
    return Mix(26);
}

// The dispatch's handlers: which one, the current entry and the scratch byte.
// Each may set bits of the kinds seen, end its entry, or change a later entry's
// kind (always below 5 - the original's table holds five), layer or activity.
void DisturbBanners(U salt) {
    const U h = Mix(salt);
    switch ((h >> 4) % 7) {
    case 0: At(kBannerKinds)[0] |= static_cast<unsigned char>(1u << ((h >> 8) % 3)); break;
    case 1: At(GetLong(At(kBannerCurrent)))[0] = 0; break;
    case 2: At(kBanners + ((h >> 8) % 8) * 0xC + 1)[0] = static_cast<unsigned char>((h >> 12) % 5); break;
    case 3: At(kBanners + ((h >> 8) % 8) * 0xC + 3)[0] = static_cast<unsigned char>((h >> 12) % 2); break;
    case 4: At(kBanners + ((h >> 8) % 8) * 0xC)[0] ^= 1; break;
    case 5: At(kScratch)[0] = static_cast<unsigned char>(h >> 16); break;
    default: break;
    }
}
U __cdecl StubBannerRet03() {
    Record(19, GetLong(At(kBannerCurrent)), At(kScratch)[0]);
    DisturbBanners(27);
    return Mix(28);
}
U __cdecl StubBannerKind1() {
    Record(20, GetLong(At(kBannerCurrent)), At(kScratch)[0]);
    DisturbBanners(29);
    return Mix(30);
}
U __cdecl StubBannerKind2() {
    Record(21, GetLong(At(kBannerCurrent)), At(kScratch)[0]);
    DisturbBanners(31);
    return Mix(32);
}
U __cdecl StubBannerRet4() {
    Record(22, GetLong(At(kBannerCurrent)), At(kScratch)[0]);
    DisturbBanners(33);
    return Mix(34);
}

const Callees kStandIns = {
    StubAlloc,
    StubActorOut,
    StubTint,
    StubEffect,
    StubStream,
    StubItemList,
    StubUsable,
    StubFree,
    StubAiCondition,
    StubAiRowDone,
    StubAiApply,
    StubAiSetDone,
    StubAiFinish,
    StubScreenA,
    StubScreenSlot,
    StubInitContext,
    StubCopyN,
    StubBannerSet,
    {StubBannerRet03, StubBannerKind1, StubBannerKind2, StubBannerRet03, StubBannerRet4},
};

// --- the copies -------------------------------------------------------------------------
// Every E8 of each body, by capstone 2026-09-23 (docs/battle_misc.md section 1);
// every jump stays inside.

const void* StubFor(U target) {
    switch (target) {
    case 0x435180: return As<const void*>(&StubAlloc);
    case 0x4456C0: return As<const void*>(&StubActorOut);
    case 0x454CC0: return As<const void*>(&StubTint);
    case 0x587740: return As<const void*>(&StubEffect);
    case 0x587910: return As<const void*>(&StubStream);
    case 0x591E50: return As<const void*>(&StubItemList);
    case 0x57DA70: return As<const void*>(&StubUsable);
    case 0x59E310: return As<const void*>(&StubFree);
    case 0x44B320: return As<const void*>(&StubAiCondition);
    case 0x44B2C0: return As<const void*>(&StubAiRowDone);
    case 0x44B3A0: return As<const void*>(&StubAiApply);
    case 0x44B2E0: return As<const void*>(&StubAiSetDone);
    case 0x44B920: return As<const void*>(&StubAiFinish);
    case 0x57B830: return As<const void*>(&StubScreenA);
    case 0x588F00: return As<const void*>(&StubScreenSlot);
    case 0x446BD0: return As<const void*>(&StubInitContext);
    case 0x5171A0: return As<const void*>(&StubCopyN);
    case 0x44A6E0: return As<const void*>(&StubBannerSet);
    default: bof3::Fatal("battle_misc: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

struct Site {
    U offset, target;
};
constexpr Site kCueSites[] = {{0x25, 0x587740}};
constexpr Site kTintSites[] = {{0x15, 0x454CC0}};
constexpr Site kContextSites[] = {{0x5, 0x435180}};
constexpr Site kContextsSites[] = {{0x1D, 0x446BD0}, {0x28, 0x435180}, {0x54, 0x446BD0}, {0x5F, 0x435180}};
constexpr Site kSoundSites[] = {{0x4B, 0x587910}};
constexpr Site kCopiesSites[] = {{0x3B, 0x435180}, {0x7E, 0x4456C0}, {0x97, 0x435180}};
constexpr Site kCanUseSites[] = {{0xF, 0x591E50}, {0x2E, 0x57DA70}};
constexpr Site kFreeSites[] = {{0x12, 0x59E310}};
constexpr Site kMessageSites[] = {{0x18, 0x5171A0}};
constexpr Site kNameSites[] = {{0x11, 0x5171A0}, {0x66, 0x44A6E0}};
constexpr Site kAiSites[] = {
    {0x4C, 0x44B320},  {0x6E, 0x44B320},  {0x90, 0x44B320},  {0xB2, 0x44B320},  {0xD4, 0x44B320},
    {0xF6, 0x44B320},  {0x118, 0x44B320}, {0x13D, 0x44B320}, {0x15F, 0x44B320}, {0x1C3, 0x44B2C0},
    {0x1DA, 0x44B3A0}, {0x201, 0x44B2C0}, {0x22A, 0x44B2C0}, {0x242, 0x44B3A0}, {0x258, 0x44B320},
    {0x26F, 0x44B3A0}, {0x27E, 0x44B320}, {0x296, 0x44B3A0}, {0x2A5, 0x44B320}, {0x2B9, 0x44B3A0},
    {0x2D8, 0x44B3A0}, {0x2FC, 0x44B2C0}, {0x310, 0x44B3A0}, {0x31E, 0x44B2E0}, {0x338, 0x44B920},
};
constexpr Site kScreenSites[] = {{0x28, 0x57B830}, {0x33, 0x588F00}};

void* Clone(const char* name, U base, U size, const Site* sites, int n) {
    bof3::CloneCall calls[32];
    if (n > 32) bof3::Fatal("battle_misc: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) calls[i] = {sites[i].offset, StubFor(sites[i].target), sites[i].target};
    void* code = bof3::CloneOriginal(name, base, size, calls, n);
    if (!code) bof3::Fatal("battle_misc: CloneOriginal(%s) returned null", name);
    return code;
}
template <int N> void* Clone(const char* name, U base, U size, const Site (&sites)[N]) {
    return Clone(name, base, size, sites, N);
}

// An immediate in a copy re-aimed: the dword at `offset` must hold `expected`.
void Reaim(void* copy, U offset, U expected, const void* target) {
    unsigned char* const at = static_cast<unsigned char*>(copy) + offset;
    if (GetLong(at) != expected)
        bof3::Fatal("battle_misc: the copy holds 0x%X at +0x%X, not 0x%X", (unsigned)GetLong(at), (unsigned)offset,
                    (unsigned)expected);
    PutLong(at, Addr(target));
}

// --- the comparison ---------------------------------------------------------------------

d3d_fuzz::Log g_theirs, g_ours;
State g_start, g_after_theirs, g_after_ours;

struct Tally {
    const char* name;
    unsigned rounds, bad, calls, hits;   // hits: a coverage count each test defines
};

struct Entry {
    U eax, ecx;
    U a[6];
};
Entry RandomEntry() {
    Entry e{Next(), Next(), {Next(), Next(), Next(), Next(), Next(), Next()}};
    return e;
}

// One round: state already generated. Runs Capcom's copy then ours from the
// same state, with the same registers and arguments, and compares the log,
// (result & mask) and the regions.
void Pass(Tally& t, const Region* rs, int nr, U mask, const void* theirs, const void* ours, const Entry& e) {
    Capture(rs, nr, g_start);
    g_theirs.Clear();
    d3d_fuzz::g_log = &g_theirs;
    const U ret_theirs = BattleMisc_CallRegs(theirs, e.eax, e.ecx, e.a[0], e.a[1], e.a[2], e.a[3], e.a[4], e.a[5]);
    Capture(rs, nr, g_after_theirs);

    Restore(rs, nr, g_start);
    g_ours.Clear();
    d3d_fuzz::g_log = &g_ours;
    const U ret_ours = BattleMisc_CallRegs(ours, e.eax, e.ecx, e.a[0], e.a[1], e.a[2], e.a[3], e.a[4], e.a[5]);
    Capture(rs, nr, g_after_ours);
    d3d_fuzz::g_log = nullptr;

    ++t.rounds;
    t.calls += g_theirs.n;
    char why[200] = "";
    bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why);
    if (same && (ret_ours & mask) != (ret_theirs & mask)) {
        std::snprintf(why, sizeof why, "the result %08X, the original %08X", (unsigned)ret_ours,
                      (unsigned)ret_theirs);
        same = false;
    }
    U where = 0;
    if (same && FirstDifference(rs, nr, g_after_ours, g_after_theirs, &where)) {
        std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
        same = false;
    }
    if (!same) {
        if (t.bad < 4) bof3::Log("shadow      battle_misc MISMATCH: %s round %u: %s", t.name, t.rounds - 1, why);
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

// A signed value near the interesting ones.
U Around(U v) { return v + static_cast<U>(static_cast<std::int32_t>(Next() % 5) - 2); }
U SignedSeed() {
    return Next() % 3 ? static_cast<U>(static_cast<std::int32_t>(Next() % 64) - 32)
                      : Pick({0, 1, 0xFFFFFFFFu, 0x7FFFFFFFu, 0x80000000u, 0x7FFFFFFEu, 0x80000001u, Next()});
}

// --- the tests --------------------------------------------------------------------------

void FuzzWrap(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x100000u + r;
        Entry e = RandomEntry();
        const U high = SignedSeed(), low = OneIn(4) ? SignedSeed() : Around(high - Next() % 16);
        e.a[0] = high;
        e.a[1] = low;
        switch (Next() % 4) {
        case 0: e.a[2] = Around(low); break;
        case 1: e.a[2] = Around(high); break;
        case 2: e.a[2] = SignedSeed(); break;
        default: e.a[2] = Next(); break;
        }
        Pass(t, nullptr, 0, 0xFFFFFFFFu, clone, As<const void*>(&Battle_WrapIndex), e);
        const auto v = static_cast<std::int32_t>(e.a[2]);
        t.hits += v >= static_cast<std::int32_t>(low) && v <= static_cast<std::int32_t>(high);
    }
}

void FuzzPulse(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kPulseDown, 8}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x200000u + r;
        FillRandom(kPulseDown, 8);
        At(kPulse)[0] = static_cast<unsigned char>(Next() % 2 ? Pick({0, 1, 2, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0xFE, 0xFF}) : Next());
        At(kPulseDown)[0] = static_cast<unsigned char>(Pick({0, 1, 1, 0, 2, 0xFF}));
        Pass(t, regions, 1, 0, clone, As<const void*>(&Battle_PulseStep), RandomEntry());
        t.hits += At(kPulseDown)[0] == 1;
    }
}

void FuzzCue(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x300000u + r;
        FillRandom(Addr(g_record), sizeof g_record);
        const U record = Next() % 3 ? Addr(g_record) + Next() % 0x40 : kParty + Next() % kPartyBytes;
        PutLong(At(kFieldState), record);
        PutWord(At(record + 0x2C), Next() % 2 ? Next() % 8 : Next());
        Entry e = RandomEntry();
        e.a[0] = Stale(Next() % 3 ? Next() % 8 : Next());
        Pass(t, nullptr, 0, 0xFFFFFFFFu, clone, As<const void*>(&Battle_PlayActorCue), e);
    }
}

void FuzzTint(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x400000u + r;
        PutLong(At(kSprite), Next());
        Entry e = RandomEntry();
        e.a[0] = Next() % 2 ? (Next() & ~0x80u) : (Next() | 0x80u);
        if (OneIn(4)) e.a[0] &= 0xFF;
        Pass(t, nullptr, 0, 0xFFFFFFFFu, clone, As<const void*>(&Battle_StatusTint), e);
        t.hits += (e.a[0] & 0x80) != 0;
    }
}

U Context(U slot) { return kContexts + (slot & 0xFF) * kContextStride; }

// A context slot plan for the stand-in: low, low + 1, .. (span of them).
void PlanSlots(U low, U span) {
    g_slot_low = low;
    g_slot_span = span;
}

void FuzzContextOne(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kContexts, kContextBytes}, {kSprite, 4}, {kParty, kPartyBytes}, {kEnemyObjects, kEnemyBytes},
                              {Addr(g_enemy), sizeof g_enemy}};
    g_test = kTestContextOne;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x500000u + r;
        if (r % 16 == 0) {
            FillRandom(kContexts, kContextBytes);
            FillRandom(kParty, kPartyBytes);
            FillRandom(kEnemyObjects, kEnemyBytes);
            FillRandom(Addr(g_enemy), sizeof g_enemy);
        }
        const U slot = Next() % kFuzzSlots;
        PlanSlots(slot, 1);
        // Sprite_Current an object, or overlapping the context it fills: its
        // +0x34.. read after the context's +0x80, +9 and the earlier dwords are
        // stored.
        U object;
        if (Next() % 3) object = SomeObject();
        else object = Context(slot) + Pick({0x4C, 0xFFFFFFFCu, 0xFFFFFFF8u, 0x48, 0x44, 0xFFFFFFD5u, 0x40});
        if (object + 0x40 > kContexts + kContextBytes && object >= kContexts && object < kContexts + kContextBytes)
            object = SomeObject();
        PutLong(At(kSprite), object);
        Pass(t, regions, 5, 0, clone, As<const void*>(&Battle_InitActorContext), RandomEntry());
        t.hits += object >= kContexts && object < kContexts + kContextBytes;
    }
    g_test = kTestNone;
}

void RandomActors(bool copies) {
    FillRandom(kParty, kPartyBytes);
    FillRandom(kEnemyObjects, kEnemyBytes);
    for (U i = 0; i < 3; ++i) {
        unsigned char* const o = At(kParty + i * kPartyStride);
        o[0] = static_cast<unsigned char>((o[0] & ~1u) | (Next() % 4 != 0));
        if (copies) o[0x134] = static_cast<unsigned char>((o[0x134] & ~8u) | (Next() % 4 == 0 ? 8 : 0));
    }
    for (U i = 0; i < 8; ++i) {
        unsigned char* const o = At(kEnemyObjects + i * kEnemyStride);
        o[0] = static_cast<unsigned char>((o[0] & ~1u) | (Next() % 2));
        if (copies) o[0x114] = static_cast<unsigned char>((o[0x114] & ~8u) | (Next() % 4 == 0 ? 8 : 0));
    }
}

void FuzzContexts(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kParty, kPartyBytes}, {kEnemyObjects, kEnemyBytes}, {kSprite, 4}, {kFieldState, 4}, {kCurrentEnemy, 4}};
    g_test = kTestContexts;
    PlanSlots(0, kFuzzSlots);
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x600000u + r;
        RandomActors(false);
        PutLong(At(kSprite), Next());
        PutLong(At(kFieldState), Next());
        PutLong(At(kCurrentEnemy), Next());
        Pass(t, regions, 5, 0, clone, As<const void*>(&Battle_InitActorContexts), RandomEntry());
    }
    g_test = kTestNone;
}

// Lists of (key << 16 | id) dwords ending in -1, the table's pointers into them.
void RandomLists(U key) {
    U at = 0;
    for (U list = 0; list < 12; ++list) {
        const U n = Next() % 6;
        for (U k = 0; k < n; ++k) {
            U hi = Next() % 3 ? Pick({key & 0xFF, key & 0xFF, (key + 1) & 0xFF, (key - 1) & 0xFF, 0, 0xFFFF}) : Next() & 0xFFFF;
            if (Next() % 4 == 0) hi = (key & 0xFF) | 0x100;   // the high half compared whole
            PutLong(g_lists + at, (hi << 16) | (Next() & 0xFFFF));
            at += 4;
        }
        PutLong(g_lists + at, 0xFFFFFFFFu);
        at += 4;
    }
    for (U s = 0; s < 256; ++s) {
        const U ptr = OneIn(8) ? 0 : Addr(g_lists) + 4 * (Next() % (at / 4));
        PutLong(At(kSoundLists + s * 4), ptr);
    }
}

void FuzzSound(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x700000u + r;
        Entry e = RandomEntry();
        e.a[0] = Stale(Next() % 2 ? Next() % 4 : Next());
        e.a[1] = Stale(Next());
        RandomLists(e.a[0]);
        Pass(t, nullptr, 0, 0xFF, clone, As<const void*>(&Battle_LoadSoundByKey), e);
        t.hits += Called(g_theirs, 5);
    }
}

void FuzzBit(Tally& t, const void* clone, const void* ours, unsigned rounds, U salt) {
    const Region regions[] = {{kActorBits, 2}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = salt + r;
        PutWord(At(kActorBits), Next() % 2 ? Next() : Pick({0, 0xFFFF, 0x8000, 1}));
        Entry e = RandomEntry();
        e.a[0] = Stale(Next() % 2 ? Next() % 11 : Pick({15, 16, 31, 32, 33, 0xFF, Next()}));
        Pass(t, regions, 1, 0xFFFFFFFFu, clone, ours, e);
    }
}

void FuzzCopies(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kContexts, kContextBytes}, {kParty, kPartyBytes}, {kEnemyObjects, kEnemyBytes}, {kSprite, 4}};
    g_test = kTestCopies;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x800000u + r;
        if (r % 8 == 0) FillRandom(kContexts, kContextBytes);
        RandomActors(true);
        PutLong(At(kSprite), Next());
        PlanSlots(Next() % 40, 1 + Next() % 8);
        Pass(t, regions, 4, 0xFFFFFFFFu, clone, As<const void*>(&Battle_SpawnActorCopies), RandomEntry());
        unsigned n = 0;
        for (unsigned c = 0; c < g_theirs.n && c < d3d_fuzz::kMaxCalls; ++c) n += g_theirs.calls[c].what == 1;
        t.hits += n;
    }
    g_test = kTestNone;
}

void FuzzCanUse(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kItemWindow, 0x24}, {kItemActor, 1}};
    g_test = kTestCanUse;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x900000u + r;
        FillRandom(kItemWindow, 0x24);
        FillRandom(Addr(g_items), sizeof g_items);
        At(kItemActor)[0] = static_cast<unsigned char>(Next() % 2 ? Next() % 3 : Next());
        At(kItemWindowCursor)[0] = static_cast<unsigned char>(Next() % 2 ? Next() % 0x40 : Next());
        Pass(t, regions, 2, 0xFF, clone, As<const void*>(&ItemMenu_CanUseSelected), RandomEntry());
    }
    g_test = kTestNone;
}

void FuzzSetupActor(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kItemWindow, 0x24}, {kItemWindowExtra, 3}, {kItemActor, 1}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0xA00000u + r;
        FillRandom(kItemWindow, 0x24);
        FillRandom(kItemWindowExtra, 3);
        FillRandom(Addr(g_record), sizeof g_record);
        if (r % 16 == 0) FillRandom(kActorPages, 0x300);
        // the acting context's byte +5: the fuzz's buffer, or inside record 16,
        // where the stores before each of its two reads can reach it
        U acting = Addr(g_record);
        if (OneIn(3)) acting = kItemWindow + Pick({0, 1, 2, 3, 4, 5, 6, 8, 9, 0xA, 0xB, 0xC, 0xF, 0x10}) - 5;
        else g_record[5] = static_cast<unsigned char>(Next() % 2 ? Next() % 3 : Next());
        PutLong(At(kActingActor), acting);
        const U source = Addr(g_record) + 0x40;
        PutLong(At(kCommandSource), source);
        PutLong(At(source + 0x10), Next() % 2 ? Pick({0, 2, 0x20000, 0x20002, 0xFFFFFFFFu, 0xFFFDFFFFu, 1}) : Next());
        Pass(t, regions, 3, 0, clone, As<const void*>(&ItemMenu_SetupForActor), RandomEntry());
    }
}

void FuzzTrue(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0xB00000u + r;
        Pass(t, nullptr, 0, 0xFF, clone, As<const void*>(&Battle_ReturnTrue), RandomEntry());
    }
}

void FuzzSetupParty(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kItemWindow, 0x24}, {kItemWindowExtra, 3}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0xC00000u + r;
        FillRandom(kItemWindow, 0x24);
        FillRandom(kItemWindowExtra, 3);
        FillRandom(kPartyPages, 3);
        Pass(t, regions, 2, 0xFFFFFFFFu, clone, As<const void*>(&ItemMenu_SetupForParty), RandomEntry());
    }
}

void FuzzFree(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kWindowCurrent, 4}};
    g_test = kTestFree;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0xD00000u + r;
        PutLong(At(kWindowCurrent), Next());
        Pass(t, regions, 1, 0xFFFFFFFFu, clone, As<const void*>(&ItemMenu_FreeWindows), RandomEntry());
    }
    g_test = kTestNone;
}

// The pool's eight entries: active mostly 0 or 1 (or any byte), kinds below 5
// (the original's table holds five), layers 0, 1 or other.
void RandomBanners(U bytes) {
    FillRandom(kBanners, bytes);
    for (U i = 0; i < 8; ++i) {
        unsigned char* const b = At(kBanners + i * 0xC);
        b[0] = static_cast<unsigned char>(Next() % 4 ? Next() % 2 : Next());
        b[1] = static_cast<unsigned char>(Next() % 5);
        b[3] = static_cast<unsigned char>(Next() % 4 ? Next() % 2 : Pick({2, 0xFF, Next()}));
    }
}

void FuzzDispatch(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kBannerCurrent, 4}, {kBanners, 0x60}, {kBannerKinds, 1}, {kScratch, 1}};
    g_test = kTestDispatch;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0xE00000u + r;
        RandomBanners(0x60);
        At(kBannerKinds)[0] = static_cast<unsigned char>(Next());
        At(kScratch)[0] = static_cast<unsigned char>(Next());
        PutLong(At(kBannerCurrent), Next());
        Pass(t, regions, 4, 0, clone, As<const void*>(&BattleBanner_Dispatch), RandomEntry());
        t.hits += g_theirs.n;
    }
    g_test = kTestNone;
}

void FuzzAdd(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kBanners, 0x60}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0xF00000u + r;
        RandomBanners(0x60);
        if (OneIn(4))   // every entry from 2 taken
            for (U i = 2; i < 8; ++i) At(kBanners + i * 0xC)[0] = static_cast<unsigned char>(1 + Next() % 0xFF);
        Entry e = RandomEntry();
        Pass(t, regions, 1, 0xFFFFFFFFu, clone, As<const void*>(&BattleBanner_Add), e);
    }
}

void FuzzSet(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kBanners, 0xC00}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1000000u + r;
        if (r % 8 == 0) FillRandom(kBanners, 0xC00);
        Entry e = RandomEntry();
        e.a[0] = Stale(Next() % 3 ? Next() % 8 : Next());
        Pass(t, regions, 1, 0xFFFFFFFFu, clone, As<const void*>(&BattleBanner_Set), e);
    }
}

void FuzzClearAll(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kBanners, 0x60}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1100000u + r;
        FillRandom(kBanners, 0x60);
        Pass(t, regions, 1, 0xFFFFFFFFu, clone, As<const void*>(&BattleBanner_ClearAll), RandomEntry());
    }
}

void FuzzNoneOfKind(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1200000u + r;
        FillRandom(kBanners, 0x60);
        const U kind = Next() % 2 ? Pick({1, 2, 4, 0}) : Next() & 0xFF;
        for (U i = 0; i < 8; ++i) {
            unsigned char* const b = At(kBanners + i * 0xC);
            b[0] = static_cast<unsigned char>(Next() % 3 ? Next() % 2 : Next());
            b[1] = static_cast<unsigned char>(Next() % 4 == 0 ? kind : Next() % 3 ? Pick({1, 2, 4, 0}) : Next());
        }
        Entry e = RandomEntry();
        e.a[0] = Stale(kind);
        Pass(t, nullptr, 0, 0xFF, clone, As<const void*>(&BattleBanner_NoneOfKind), e);
    }
}

void FuzzPush(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kQueueRead, 2}, {kQueue, 0x800}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1300000u + r;
        if (r % 8 == 0) FillRandom(kQueue, 0x800);
        At(kQueueWrite)[0] = static_cast<unsigned char>(Next() % 3 ? Next() % 16 : Pick({15, 16, 0x1F, 0xFF, Next()}));
        At(kQueueRead)[0] = static_cast<unsigned char>(Next());
        Pass(t, regions, 2, 0xFFFFFFFFu, clone, As<const void*>(&BattleQueue_Push), RandomEntry());
    }
}

void FuzzPending(Tally& t, const void* clone, unsigned rounds) {
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1400000u + r;
        const U a = Next() % 16;
        At(kQueueWrite)[0] = static_cast<unsigned char>(a);
        At(kQueueRead)[0] = static_cast<unsigned char>(Next() % 2 ? a : Next() % 3 ? (a + 1) & 0xF : Next());
        Pass(t, nullptr, 0, 0xFF, clone, As<const void*>(&BattleQueue_Pending), RandomEntry());
        t.hits += At(kQueueWrite)[0] != At(kQueueRead)[0];
    }
}

// Strings of 0..12 bytes in g_strings, the message table's entries pointing at them.
void RandomStrings() {
    for (U i = 0; i < sizeof g_strings; ++i) g_strings[i] = static_cast<unsigned char>(1 + Next() % 0xFF);
    for (U at = 0; at < sizeof g_strings; at += 1 + Next() % 13) g_strings[at] = 0;
    g_strings[sizeof g_strings - 1] = 0;
    for (U m = 0; m < 256; ++m) PutLong(At(kMessages + m * 4), Addr(g_strings) + Next() % 0x100);
}

void FuzzMessage(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kBanners, 0xC}, {kBannerText, 0x40}};
    g_test = kTestMessage;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1500000u + r;
        if (r % 16 == 0) RandomStrings();
        FillRandom(kBanners, 0xC);
        FillRandom(kBannerText, 0x40);
        Entry e = RandomEntry();
        e.a[0] = Stale(Next());
        Pass(t, regions, 2, 0xFFFFFFFFu, clone, As<const void*>(&BattleBanner_SetMessage), e);
    }
    g_test = kTestNone;
}

void FuzzName(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kBannerText, 0x40}, {kSuffixOn, 1}, {kNameSuffix, 4}};
    g_test = kTestName;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1600000u + r;
        if (r % 16 == 0) RandomStrings();
        FillRandom(Addr(g_actor), sizeof g_actor);
        // the name: 0..12 bytes then a NUL (or none within 8)
        const U len = Next() % 13;
        for (U k = 0; k < 16; ++k) g_actor[0x80 + k] = static_cast<unsigned char>(k < len ? 1 + Next() % 0xFF : k == len ? 0 : Next());
        FillRandom(kBannerText, 0x40);
        At(kSuffixOn)[0] = static_cast<unsigned char>(Next() % 2 ? Next() % 2 : Next());
        // the suffix: 0..9 bytes, in the second half of g_strings
        const U suffix = Addr(g_strings) + 0x100 + Next() % 0x40;
        const U slen = Next() % 10;
        for (U k = 0; k < slen; ++k) At(suffix + k)[0] = static_cast<unsigned char>(1 + Next() % 0xFF);
        At(suffix + slen)[0] = 0;
        PutLong(At(kNameSuffix), suffix);
        Entry e = RandomEntry();
        e.a[0] = Addr(g_actor);
        Pass(t, regions, 3, 0xFFFFFFFFu, clone, As<const void*>(&BattleBanner_ShowName), e);
        t.hits += At(kSuffixOn)[0] != 0;
    }
    g_test = kTestNone;
}

// The enemy and its script: fields the ops test seeded, rows' opcodes over the
// whole range with the table's edges.
void FuzzAi(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kCurrentEnemy, 4}, {kAiScripts, kScriptBytes}, {kActingKind, 1}, {kEnemyObjects, kEnemyBytes},
                              {Addr(g_enemy), sizeof g_enemy}};
    g_test = kTestAi;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1700000u + r;
        if (r % 32 == 0) {
            FillRandom(kEnemyObjects, kEnemyBytes);
            FillRandom(Addr(g_enemy), sizeof g_enemy);
            // a stand-in may move the current enemy: its script is read at the next row
            for (U i = 0; i < 8; ++i) At(kEnemyObjects + i * kEnemyStride + 0xF0)[0] = static_cast<unsigned char>(Next() % 16);
            for (U k = 0; k < 4; ++k) g_enemy[0xF0 + k * 4] = static_cast<unsigned char>(Next() % 16);
        }
        const U enemy = Next() % 3 ? SomeEnemy() : Addr(g_enemy);
        PutLong(At(kCurrentEnemy), enemy);
        unsigned char* const e = At(enemy);
        const U script = Next() % 4 ? Next() % 16 : Next() % kFuzzScripts;
        e[0xF0] = static_cast<unsigned char>(script);
        const U hp = Next() % 2 ? Pick({0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10}) : Next() & 0xFFFF;
        PutWord(e + 0x108, hp);
        PutWord(e + 0xA4, Next() % 2 ? (hp + static_cast<U>(static_cast<std::int32_t>(Next() % 3) - 1)) & 0xFFFF
                                    : Pick({0, 0x7FFF, 0x8000, 0xFFFF, Next() & 0xFFFF}));
        e[0x92] = static_cast<unsigned char>(Next() % 2 ? Pick({0, 8, 0x80, 0x88, 0x77, 0xF7}) : Next());
        e[0xAA] = static_cast<unsigned char>(Next() % 2 ? 0 : Next());
        At(kActingKind)[0] = static_cast<unsigned char>(Next() % 3 ? Pick({1, 4, 0, 2, 5}) : Next());
        // every script the enemies can name gets rows now and then
        g_script = kAiScripts + script * 0x8C;
        if (r % 16 == 0) FillRandom(kAiScripts, kScriptBytes);
        for (U i = 0; i < 4; ++i) {
            U op;
            switch (Next() % 4) {
            case 0: op = Next() % 9; break;
            case 1: op = Pick({9, 0xA, 0x16, 0x17, 0x18, 0x21, 0x22, 0x23, 0x24, 0x25}); break;
            case 2: op = Pick({0xB, 0x15, 0x19, 0x20, 0x26, 0x27, 0xFF, 0x80}); break;
            default: op = Next() % 0x28; break;
            }
            At(g_script + i * 16)[0] = static_cast<unsigned char>(op);
        }
        Pass(t, regions, 5, 0xFFFFFFFFu, clone, As<const void*>(&EnemyAI_TurnCheck), RandomEntry());
        t.hits += Called(g_theirs, 12);
    }
    g_test = kTestNone;
}

void FuzzCopyN(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{Addr(g_str), sizeof g_str}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1800000u + r;
        for (U i = 0; i < sizeof g_str; ++i) g_str[i] = static_cast<unsigned char>(1 + Next() % 0xFF);
        g_str[sizeof g_str - 1] = 0;
        const U src = Next() % 0x40;
        const U len = Next() % 3 ? Next() % 12 : Next() % 0x100;
        if (src + len < sizeof g_str) g_str[src + len] = 0;
        U dst;
        switch (Next() % 3) {
        case 0: dst = src + 1 + Next() % 4; break;             // overlapping, ahead: the copy feeds itself
        case 1: dst = src >= 4 ? src - 1 - Next() % 4 : 0x100; break;
        default: dst = 0x100 + Next() % 0x40; break;
        }
        Entry e = RandomEntry();
        e.a[0] = Addr(g_str) + dst;
        e.a[1] = Addr(g_str) + src;
        e.a[2] = Stale(Next() % 2 ? Pick({0, 1, 7, 8, 9, 12, 0xFF}) : Next());
        Pass(t, regions, 1, 0xFFFFFFFFu, clone, As<const void*>(&Str_CopyN), e);
    }
}

void FuzzScreens(Tally& t, const void* clone, unsigned rounds) {
    const Region regions[] = {{kObjects, kObjectBytes}, {kSprite, 4}};
    g_test = kTestScreens;
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x1900000u + r;
        if (r % 8 == 0) FillRandom(kObjects, kObjectBytes);
        for (U i = 0; i < 30; ++i) {
            unsigned char* const o = At(kObjects + i * 0xA4);
            o[0] = static_cast<unsigned char>((o[0] & ~1u) | (Next() % 3 != 0));
            o[6] = static_cast<unsigned char>(Next() % 2 ? Pick({6, 7, 8, 9, 0xA, 0xB, 0, 0xFF}) : Next());
        }
        PutLong(At(kSprite), Next());
        Pass(t, regions, 2, 0, clone, As<const void*>(&Sprite_UpdateObjectScreens), RandomEntry());
        t.hits += g_theirs.n;
    }
    g_test = kTestNone;
}

}  // namespace

void SelfTest() {
    // The copies, before BattleMisc_Inject patches anything.
    const void* wrap = Clone("Battle_WrapIndex", 0x4469F0, 0x1A, nullptr, 0);
    const void* pulse = Clone("Battle_PulseStep", 0x446A10, 0x34, nullptr, 0);
    const void* cue = Clone("Battle_PlayActorCue", 0x446A50, 0x2C, kCueSites);
    const void* tint = Clone("Battle_StatusTint", 0x446BB0, 0x1E, kTintSites);
    const void* context = Clone("Battle_InitActorContext", 0x446BD0, 0x54, kContextSites);
    const void* contexts = Clone("Battle_InitActorContexts", 0x446C30, 0x73, kContextsSites);
    const void* sound = Clone("Battle_LoadSoundByKey", 0x446E40, 0x59, kSoundSites);
    const void* set_bit = Clone("Battle_SetActorBit", 0x446FB0, 0x13, nullptr, 0);
    const void* clear_bit = Clone("Battle_ClearActorBit", 0x446FD0, 0x15, nullptr, 0);
    const void* copies = Clone("Battle_SpawnActorCopies", 0x446FF0, 0x11D, kCopiesSites);
    const void* can_use = Clone("ItemMenu_CanUseSelected", 0x447840, 0x3C, kCanUseSites);
    const void* setup_actor = Clone("ItemMenu_SetupForActor", 0x447E60, 0xD4, nullptr, 0);
    const void* is_true = Clone("Battle_ReturnTrue", 0x449E00, 0x3, nullptr, 0);
    const void* setup_party = Clone("ItemMenu_SetupForParty", 0x449E10, 0x7B, nullptr, 0);
    const void* free_windows = Clone("ItemMenu_FreeWindows", 0x449FE0, 0x20, kFreeSites);
    void* dispatch = Clone("BattleBanner_Dispatch", 0x44A5C0, 0x88, nullptr, 0);
    // Its table is built from immediates: eax (entries 0 and 3), then entries 1, 2 and 4.
    Reaim(dispatch, 0x4, kRetOnly, As<const void*>(&StubBannerRet03));
    Reaim(dispatch, 0x14, kBannerKind1, As<const void*>(&StubBannerKind1));
    Reaim(dispatch, 0x1C, kBannerKind2, As<const void*>(&StubBannerKind2));
    Reaim(dispatch, 0x28, kRetOnly, As<const void*>(&StubBannerRet4));
    const void* add = Clone("BattleBanner_Add", 0x44A650, 0x89, nullptr, 0);
    const void* set = Clone("BattleBanner_Set", 0x44A6E0, 0x5B, nullptr, 0);
    const void* clear_all = Clone("BattleBanner_ClearAll", 0x44A810, 0x1B, nullptr, 0);
    const void* none_of = Clone("BattleBanner_NoneOfKind", 0x44A830, 0x42, nullptr, 0);
    const void* push = Clone("BattleQueue_Push", 0x44A880, 0x37, nullptr, 0);
    const void* pending = Clone("BattleQueue_Pending", 0x44A8C0, 0x11, nullptr, 0);
    const void* message = Clone("BattleBanner_SetMessage", 0x44A8E0, 0x2B, kMessageSites);
    const void* name = Clone("BattleBanner_ShowName", 0x44A990, 0x6F, kNameSites);
    // EnemyAI_TurnCheck with its jump table (20 dwords at +0x344): the index
    // table after it (+0x394) is read by absolute address, from the original.
    void* ai = Clone("EnemyAI_TurnCheck", 0x44AAD0, 0x394, kAiSites);
    move_script::Relocate(ai, 0x44AAD0, 0x343, {0x46, 0x344, 20});
    const void* copy_n = Clone("Str_CopyN", 0x5171A0, 0x34, nullptr, 0);
    const void* screens = Clone("Sprite_UpdateObjectScreens", 0x517440, 0x44, kScreenSites);

    const int n_all = static_cast<int>(sizeof g_all / sizeof g_all[0]);
    static State saved;
    if (RegionBytes(g_all, n_all) > kMaxState) bof3::Fatal("battle_misc: the saved regions outgrow the state buffer");
    Capture(g_all, n_all, saved);
    const Callees saved_callees = g;
    g = kStandIns;
    d3d_fuzz::Seed(0x42464246u);

    Tally tallies[] = {
        {"Battle_WrapIndex", 0, 0, 0, 0},        {"Battle_PulseStep", 0, 0, 0, 0},
        {"Battle_PlayActorCue", 0, 0, 0, 0},     {"Battle_StatusTint", 0, 0, 0, 0},
        {"Battle_InitActorContext", 0, 0, 0, 0}, {"Battle_InitActorContexts", 0, 0, 0, 0},
        {"Battle_LoadSoundByKey", 0, 0, 0, 0},   {"Battle_SetActorBit", 0, 0, 0, 0},
        {"Battle_ClearActorBit", 0, 0, 0, 0},    {"Battle_SpawnActorCopies", 0, 0, 0, 0},
        {"ItemMenu_CanUseSelected", 0, 0, 0, 0}, {"ItemMenu_SetupForActor", 0, 0, 0, 0},
        {"Battle_ReturnTrue", 0, 0, 0, 0},       {"ItemMenu_SetupForParty", 0, 0, 0, 0},
        {"ItemMenu_FreeWindows", 0, 0, 0, 0},    {"BattleBanner_Dispatch", 0, 0, 0, 0},
        {"BattleBanner_Add", 0, 0, 0, 0},        {"BattleBanner_Set", 0, 0, 0, 0},
        {"BattleBanner_ClearAll", 0, 0, 0, 0},   {"BattleBanner_NoneOfKind", 0, 0, 0, 0},
        {"BattleQueue_Push", 0, 0, 0, 0},        {"BattleQueue_Pending", 0, 0, 0, 0},
        {"BattleBanner_SetMessage", 0, 0, 0, 0}, {"BattleBanner_ShowName", 0, 0, 0, 0},
        {"EnemyAI_TurnCheck", 0, 0, 0, 0},       {"Str_CopyN", 0, 0, 0, 0},
        {"Sprite_UpdateObjectScreens", 0, 0, 0, 0},
    };
    FuzzWrap(tallies[0], wrap, 10000);
    FuzzPulse(tallies[1], pulse, 3000);
    FuzzCue(tallies[2], cue, 3000);
    FuzzTint(tallies[3], tint, 3000);
    FuzzContextOne(tallies[4], context, 5000);
    FuzzContexts(tallies[5], contexts, 5000);
    FuzzSound(tallies[6], sound, 10000);
    FuzzBit(tallies[7], set_bit, As<const void*>(&Battle_SetActorBit), 3000, 0x1A00000u);
    FuzzBit(tallies[8], clear_bit, As<const void*>(&Battle_ClearActorBit), 3000, 0x1B00000u);
    FuzzCopies(tallies[9], copies, 5000);
    FuzzCanUse(tallies[10], can_use, 5000);
    FuzzSetupActor(tallies[11], setup_actor, 5000);
    FuzzTrue(tallies[12], is_true, 500);
    FuzzSetupParty(tallies[13], setup_party, 2000);
    FuzzFree(tallies[14], free_windows, 2000);
    FuzzDispatch(tallies[15], dispatch, 10000);
    FuzzAdd(tallies[16], add, 5000);
    FuzzSet(tallies[17], set, 5000);
    FuzzClearAll(tallies[18], clear_all, 1000);
    FuzzNoneOfKind(tallies[19], none_of, 5000);
    FuzzPush(tallies[20], push, 5000);
    FuzzPending(tallies[21], pending, 2000);
    FuzzMessage(tallies[22], message, 5000);
    FuzzName(tallies[23], name, 10000);
    FuzzAi(tallies[24], ai, 20000);
    FuzzCopyN(tallies[25], copy_n, 10000);
    FuzzScreens(tallies[26], screens, 5000);

    g = saved_callees;
    Restore(g_all, n_all, saved);

    unsigned bad = 0, rounds = 0;
    for (const Tally& t : tallies) {
        bad += t.bad;
        rounds += t.rounds;
        bof3::Log("shadow      battle_misc self-test: %s %u rounds, %u calls out, %u covered, %u MISMATCHES", t.name,
                  t.rounds, t.calls, t.hits, t.bad);
    }
    bof3::Log("shadow      battle_misc self-test: %u rounds over 27 functions, %u MISMATCHES", rounds, bad);
    if (bad) bof3::Fatal("group BF's functions differ from the original in %u of %u self-test rounds", bad, rounds);
}

}  // namespace battle_misc

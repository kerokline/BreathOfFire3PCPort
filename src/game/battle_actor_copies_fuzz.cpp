// BOF3X_SHADOW=battle_actor_copies: a differential fuzz of group CH, once at
// start-up. docs/battle_actor_copies.md section 4.
//
// Sixteen byte-copies, every call out re-aimed at a recording stand-in
// (bof3::CloneCall with `expected`); the five .data tables the stubs jump
// through (BattleTarget_Steps .. BattleItem_TargetSteps, twenty entries)
// pointed at recorders, so each stub reaches a recorder from its copy and from
// ours alike. One round: one function, random bytes in every region any of
// them touches, the two pointers aimed into buffers of our own, the indices
// put back inside the tables, each branch's boundaries seeded; theirs, then
// from the same state ours; the regions, the buffers and the stand-ins' log
// compared. Every stand-in logs a hash of the bytes the functions store, so a
// store moved across a call shows. Everything is put back afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_actor_copies_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_actor_copies {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U Address(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char& B(U address) { return At(address)[0]; }

// --- the buffers the two pointers are aimed into, and the list -------------

unsigned char g_cmd[0x40];    // 0x939FA0 points in here
unsigned char g_act[0x20];    // 0x939EC4 points in here, or at 0x939FA0's target
unsigned char g_list[0x200];  // Char_AbilityList's stand-in returns a pointer into it

void SetPtr(U address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }
unsigned char* Cmd() { return At(static_cast<U>(Long(At(at::kCommand)))); }

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 64;
struct Entry { U what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

U Hash() {
    U h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
U Fnv(U h, const unsigned char* p, unsigned n) {
    for (unsigned i = 0; i < n; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}
// Every byte the sixteen store, as it stands at the call.
U Watched() {
    U h = 0x811C9DC5u;
    h = Fnv(h, At(0x904AA0), 0x28);   // the phase bytes .. 0x904AC7
    h = Fnv(h, g_cmd, sizeof g_cmd);
    h = Fnv(h, g_act, sizeof g_act);
    h = Fnv(h, At(at::kRepeatLatch), 2);
    h = Fnv(h, At(at::kWindow4State), 1);
    h = Fnv(h, At(0x8033A0), 0x80);   // the item window's record and its cursor fields
    h = Fnv(h, At(at::kItemActor), 1);
    h = Fnv(h, At(0x93C2A0), 0xA0);   // the message queue
    h = Fnv(h, At(at::kActorPages), 0x300);
    return h;
}

// Each case moves a byte some function reads after a call, or again after one.
void Disturb() {
    const U h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 22) {
    case 0: B(at::kStep) = static_cast<unsigned char>(v % 6); break;
    case 1: B(at::kSub) = static_cast<unsigned char>(v % 5); break;
    case 2: SetPtr(at::kCommand, g_cmd + v % 0x10); break;
    case 3: SetPtr(at::kActing, v % 4 == 0 ? static_cast<const void*>(Cmd()) : g_act + v % 0x10); break;
    case 4: B(at::kItemPage) = static_cast<unsigned char>(v % 3 == 0 ? v : v % 5); break;
    case 5: B(at::kItemCursor) = static_cast<unsigned char>(v % 3 == 0 ? v : v % 12); break;
    case 6: B(at::kItemActor) = static_cast<unsigned char>(v); break;
    case 7: B(at::kQueueWrite) = static_cast<unsigned char>(v); break;
    case 8: SetWord(At(at::kItemScrollMotion), v % 2 ? 0 : w); break;
    case 9: SetWord(At(at::kPressed), w); break;
    case 10: SetWord(At(v % 2 ? at::kCancel : at::kConfirm), v % 3 == 0 ? 0 : 1u << (w % 16)); break;
    case 11: B(at::kEnemyCount) = static_cast<unsigned char>(v % 9); break;
    case 12: B(at::kPartyCount) = static_cast<unsigned char>(v % 5); break;
    case 13: Cmd()[0] = static_cast<unsigned char>(v); break;
    case 14: g_list[w % sizeof g_list] = static_cast<unsigned char>(v % 4 == 0 ? 0x97 : v); break;
    case 15: {
        // the flags of the item a list read would give, most of the time
        const unsigned id = g_list[w % sizeof g_list];
        B(at::kItemRecords + id * at::kItemRecordSize) = static_cast<unsigned char>(v);
        break;
    }
    case 16: SetWord(At(at::kItemScroll), v % 2 ? v % 9 : w); break;
    case 17: B(at::kEntryCount) = static_cast<unsigned char>(v); break;
    case 18: B(at::kItemOwner) = static_cast<unsigned char>(v); break;
    case 19: SetWord(Cmd() + 2, v % 3 == 0 ? 0x97 : v); break;
    case 20: SetLong(Cmd() + 0x10, static_cast<std::int32_t>(h * 0x9E3779B1u)); break;
    default: B(at::kItemState) = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    }
}

// --- the stand-ins ---------------------------------------------------------

// The twenty table entries.
template <unsigned N> void __cdecl StubHandler() {
    Record(N, B(at::kStep), B(at::kSub), Watched());
    Disturb();
}
U __cdecl StubSetMessage(U msg, U b2) { Record(1, msg, b2, Watched()); Disturb(); return Hash(); }
U __cdecl StubDefaultTarget(U actor) { Record(2, actor, Watched()); Disturb(); return Hash(); }
U __cdecl StubPrevTarget(U actor) { Record(3, actor, Watched()); Disturb(); return Hash(); }
// Inside low .. high, at its edges or past them, the value it was given, or
// anything; the upper bits of a byte result as often as not.
U __cdecl StubWrapIndex(U high, U low, U value) {
    Record(4, high, low, value, Watched());
    Disturb();
    const U h = Hash();
    const U picks[] = {low, high, value, high + 1, low - 1, 0x7F, 0x80, 0xFF, 0x100, h};
    const U r = picks[(h >> 4) % 10];
    return (h >> 8) % 2 ? r : (h & 0xFFFFFF00u) | (r & 0xFF);
}
// The d-pad bits each branch tests, alone and together, with the upper half
// any; now and then anything.
U __cdecl StubAutoRepeat(U pressed) {
    Record(5, pressed, Watched());
    Disturb();
    static const U kHeld[] = {0, 0x1000, 0x2000, 0x4000, 0x8000, 0x5000, 0x3000, 0x6000, 0x9000, 0xA000, 0xC000,
                              4, 8, 0xC, 0x1004, 0x2008, 0x8004, 0x7000, 0xF000, 0xA00C, 0x4008, 0x0F0F};
    const U h = Hash();
    if (h % 8 == 0) return h;
    return (h & 0xFFFF0000u) | kHeld[(h >> 8) % (sizeof kHeld / sizeof kHeld[0])];
}
// Sound_PlayEffect reads the u16.
U __cdecl StubPlayEffect(U id) { Record(6, id & 0xFFFF, Watched()); Disturb(); return Hash(); }
U __cdecl StubSetupForActor() { Record(7, Watched()); Disturb(); return Hash(); }
// Char_AbilityList reads the low byte of each argument (the original's carry
// stale upper bits): a list somewhere in our buffer, so a cursor of 0..255
// stays inside it.
U __cdecl StubAbilityList(U member, U type, U battle) {
    Record(8, member & 0xFF, type & 0xFF, battle & 0xFF, Watched());
    Disturb();
    return Address(g_list + (Hash() >> 8) % 0x100);
}
U __cdecl StubMsgSystem(U id) { Record(9, id, Watched()); Disturb(); return Hash(); }
// al zero or not, the upper bits any.
U AlEither() {
    const U h = Hash();
    return (h >> 3) % 2 ? (h & 0xFFFFFF00u) : (h | 1);
}
U __cdecl StubCanUse() { Record(10, Watched()); Disturb(); return AlEither(); }
U __cdecl StubReturnTrue() { Record(11, Watched()); Disturb(); return AlEither(); }
U __cdecl StubFreeWindows() { Record(12, Watched()); Disturb(); return Hash(); }

const void* StubFor(U target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x44A8E0: return f(&StubSetMessage);
    case 0x445730: return f(&StubDefaultTarget);
    case 0x4457F0: return f(&StubPrevTarget);
    case 0x4469F0: return f(&StubWrapIndex);
    case 0x461EB0: return f(&StubAutoRepeat);
    case 0x587740: return f(&StubPlayEffect);
    case 0x447E60: return f(&StubSetupForActor);
    case 0x591E50: return f(&StubAbilityList);
    case 0x497740: return f(&StubMsgSystem);
    case 0x447840: return f(&StubCanUse);
    case 0x449E00: return f(&StubReturnTrue);
    case 0x449FE0: return f(&StubFreeWindows);
    default: bof3::Fatal("battle_actor_copies: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

const Callees kStubs = {
    StubSetMessage, StubDefaultTarget, StubPrevTarget, StubWrapIndex, StubAutoRepeat, StubPlayEffect,
    StubSetupForActor, StubAbilityList, StubMsgSystem, StubCanUse, StubReturnTrue, StubFreeWindows,
};

// The five tables and the recorders their entries become.
using Handler = void (__cdecl*)();
struct Table { U at; unsigned count; const Handler* stubs; };
const Handler kTargetSteps[] = {&StubHandler<100>, &StubHandler<101>};
const Handler kTargetPicks[] = {&StubHandler<110>, &StubHandler<111>, &StubHandler<112>, &StubHandler<113>, &StubHandler<114>};
const Handler kItemSteps[] = {&StubHandler<120>, &StubHandler<121>, &StubHandler<122>,
                              &StubHandler<123>, &StubHandler<124>, &StubHandler<125>};
const Handler kItemOpen[] = {&StubHandler<130>, &StubHandler<131>};
const Handler kItemTargets[] = {&StubHandler<140>, &StubHandler<141>, &StubHandler<142>, &StubHandler<143>, &StubHandler<144>};
const Table kTables[] = {
    {Address(BattleTarget_Steps), BattleTarget_Steps_count, kTargetSteps},
    {Address(BattleTarget_Picks), BattleTarget_Picks_count, kTargetPicks},
    {Address(BattleItem_Steps), BattleItem_Steps_count, kItemSteps},
    {Address(BattleItem_OpenSteps), BattleItem_OpenSteps_count, kItemOpen},
    {Address(BattleItem_TargetSteps), BattleItem_TargetSteps_count, kItemTargets},
};
constexpr unsigned kTableEntries = 20;

// --- the copies ------------------------------------------------------------

struct Call { U offset, target; };
struct Clone {
    const char* name;
    U base, size;
    const Call* calls;
    int n_calls;
};

// Every E8 of each extent (capstone, 2026-09-25); BattleItem_Browse's two E9
// at +0x107 / +0x143 are jumps inside it and stay.
constexpr Call kPromptCalls[] = {{0x4, 0x44A8E0}};
constexpr Call kBeginCalls[] = {{0x2, 0x445730}};
constexpr Call kPickEnemyCalls[] = {{0x31, 0x461EB0}, {0x45, 0x445730}, {0x65, 0x587740}, {0x90, 0x4469F0}, {0x96, 0x445730},
                                    {0xA8, 0x587740}, {0xD0, 0x4469F0}, {0xD6, 0x4457F0}, {0xE8, 0x587740}};
constexpr Call kConfirmCalls[] = {{0x5, 0x587740}};
constexpr Call kOpenCalls[] = {{0x9, 0x447E60}};
constexpr Call kBrowseCalls[] = {{0x57, 0x461EB0}, {0x7F, 0x587740}, {0xAF, 0x587740}, {0x1E6, 0x587740}, {0x20C, 0x591E50},
                                 {0x230, 0x497740}, {0x2A3, 0x587740}, {0x2C6, 0x447840}, {0x2D4, 0x587740}, {0x2E3, 0x587740},
                                 {0x31F, 0x591E50}};
constexpr Call kChooseCalls[] = {{0xF, 0x591E50}};
constexpr Call kTargetBeginCalls[] = {{0x1B, 0x445730}, {0x41, 0x449E00}, {0x57, 0x445730}};
constexpr Call kItemEnemyCalls[] = {{0x31, 0x461EB0}, {0x47, 0x591E50}, {0x79, 0x445730}, {0x99, 0x587740}, {0xC4, 0x4469F0},
                                    {0xCA, 0x445730}, {0xDC, 0x587740}, {0x104, 0x4469F0}, {0x10A, 0x4457F0}, {0x11C, 0x587740}};
constexpr Call kItemPartyCalls[] = {{0x31, 0x461EB0}, {0x47, 0x591E50},  {0x79, 0x445730},  {0x99, 0x587740},  {0xAB, 0x449E00},
                                    {0xCF, 0x4469F0}, {0xFB, 0x4469F0},  {0x101, 0x445730}, {0x116, 0x587740}, {0x12A, 0x449E00},
                                    {0x14E, 0x4469F0}, {0x163, 0x587740}, {0x187, 0x4469F0}, {0x18D, 0x4457F0}, {0x1A2, 0x587740}};
constexpr Call kItemConfirmCalls[] = {{0x6, 0x587740}, {0x49, 0x449FE0}};

enum : unsigned {
    kTargetDispatch, kShowPrompt, kPickDispatch, kBegin, kPickEnemy, kConfirm, kItemDispatch, kOpenDispatch, kOpenWindow,
    kBrowse, kChoose, kTargetDispatch2, kTargetBegin, kItemPickEnemy, kItemPickParty, kItemConfirm, kCount
};

#define CH_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
#define CH_P(name, base, size) {name, base, size, nullptr, 0}
const Clone kClones[kCount] = {
    CH_P("BattleTarget_Dispatch", 0x447110, 0xE),
    CH_C("BattleTarget_ShowPrompt", 0x447120, 0x19, kPromptCalls),
    CH_P("BattleTarget_PickDispatch", 0x447140, 0x11),
    CH_C("BattleTarget_Begin", 0x447160, 0x2F, kBeginCalls),
    CH_C("BattleTarget_PickEnemy", 0x447190, 0xF2, kPickEnemyCalls),
    CH_C("BattleTarget_Confirm", 0x447390, 0x4D, kConfirmCalls),
    CH_P("BattleItem_Dispatch", 0x447430, 0xE),
    CH_P("BattleItem_OpenDispatch", 0x447440, 0x11),
    CH_C("BattleItem_OpenWindow", 0x447460, 0x4B, kOpenCalls),
    CH_C("BattleItem_Browse", 0x4474B0, 0x387, kBrowseCalls),
    CH_C("BattleItem_Choose", 0x4478B0, 0x90, kChooseCalls),
    CH_P("BattleItem_TargetDispatch", 0x447940, 0x11),
    CH_C("BattleItem_TargetBegin", 0x447960, 0x7F, kTargetBeginCalls),
    CH_C("BattleItem_PickEnemy", 0x4479E0, 0x126, kItemEnemyCalls),
    CH_C("BattleItem_PickParty", 0x447B10, 0x1AC, kItemPartyCalls),
    CH_C("BattleItem_Confirm", 0x447CC0, 0x69, kItemConfirmCalls),
};
#undef CH_C
#undef CH_P

// --- the state both passes start from --------------------------------------

struct Region { U at, size; };
const Region kRegions[] = {
    {0x904AA0, 0x40},             // the phase bytes, 0x904AAF, the counts, 0x904AC3
    {at::kRepeatLatch, 2},
    {at::kPressed, 2},
    {at::kConfirm, 4},            // and Field_CancelButtons
    {0x8031F0, 4},                // window record 4's first bytes (0x8031F3)
    {0x8033A0, 0x80},             // the item window's record 16 .. its cursor fields 0x803413
    {at::kItemActor, 1},
    {at::kActing, 4},
    {at::kCommand, 4},
    {0x93C2A0, 0xA0},             // the message queue's indices and entries
    {at::kActorPages, 0x300},     // 3 bytes for each of 256 owners
    {at::kItemRecords, 0x1800},   // constant data - random here, put back after: 256 item records
};
constexpr unsigned kRegionBytes = 0x40 + 2 + 2 + 4 + 4 + 0x80 + 1 + 4 + 4 + 0xA0 + 0x300 + 0x1800;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char cmd[sizeof g_cmd];
    unsigned char act[sizeof g_act];
    unsigned char list[sizeof g_list];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.cmd, g_cmd, sizeof g_cmd);
    std::memcpy(s.act, g_act, sizeof g_act);
    std::memcpy(s.list, g_list, sizeof g_list);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_cmd, s.cmd, sizeof g_cmd);
    std::memcpy(g_act, s.act, sizeof g_act);
    std::memcpy(g_list, s.list, sizeof g_list);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
unsigned Byte(const State& s, U address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}

U g_rng = 0x6A09E667u;
U Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
template <typename T, std::size_t N> T Pick(const T (&v)[N]) { return v[Next() % N]; }

// Random bytes put back inside what the buffers and tables hold.
void Fix() {
    SetPtr(at::kCommand, g_cmd + Next() % 0x10);
    SetPtr(at::kActing, Next() % 4 == 0 ? static_cast<const void*>(Cmd()) : g_act + Next() % 0x10);
    SetWord(Cmd() + 2, Next() % 5 == 0 ? 0x97 : Next() % 0x100);   // an item record inside the region
    for (unsigned char& b : g_list) if (Next() % 4 == 0) b = 0x97;
    B(at::kStep) = static_cast<unsigned char>(Next() % 6);
    B(at::kSub) = static_cast<unsigned char>(Next() % 5);
    if (Often()) B(at::kItemState) = 0;
    if (Half()) SetWord(At(at::kItemScrollMotion), 0);
}

// Buttons: none, the cancel set, the confirm set, both, anything.
void SeedButtons() {
    SetWord(At(at::kCancel), Half() ? 1u << (Next() % 16) : Next());
    SetWord(At(at::kConfirm), Half() ? 1u << (Next() % 16) : Next());
    const unsigned cancel = Word(At(at::kCancel)), confirm = Word(At(at::kConfirm));
    unsigned pressed;
    switch (Next() % 6) {
    case 0: pressed = 0; break;
    case 1: pressed = cancel; break;
    case 2: pressed = confirm & ~cancel; break;
    case 3: pressed = cancel | confirm; break;
    case 4: pressed = Next() & ~(cancel | confirm); break;
    default: pressed = Next(); break;
    }
    SetWord(At(at::kPressed), pressed);
}

void SeedTarget() {
    static const unsigned char kTargets[] = {0, 1, 2, 3, 4, 10, 0x7F, 0x80, 0x81, 0xFF, 0xFE};
    if (Often()) Cmd()[0] = Pick(kTargets);
    static const unsigned char kCounts[] = {0, 1, 2, 3, 4, 5, 8, 0xFF};
    if (Often()) B(at::kEnemyCount) = Pick(kCounts);
    if (Often()) B(at::kPartyCount) = Pick(kCounts);
}

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    SeedButtons();
    SeedTarget();
    switch (k) {
    case kTargetDispatch:
    case kItemDispatch:
        B(at::kStep) = static_cast<unsigned char>(Next() % (k == kTargetDispatch ? BattleTarget_Steps_count : BattleItem_Steps_count));
        break;
    case kPickDispatch:
        B(at::kSub) = static_cast<unsigned char>(Next() % BattleTarget_Picks_count);
        break;
    case kOpenDispatch:
        B(at::kSub) = static_cast<unsigned char>(Next() % BattleItem_OpenSteps_count);
        break;
    case kTargetDispatch2:
        B(at::kSub) = static_cast<unsigned char>(Next() % BattleItem_TargetSteps_count);
        break;
    case kOpenWindow:
    case kBrowse: {
        // the command's flags: bit 1 alone, with bit 17, neither, anything
        static const U kFlags[] = {2, 0x20002, 0, 0x20000, 0xFFFFFFFFu, 0xFFFDFFFFu};
        SetLong(Cmd() + 0x10, static_cast<std::int32_t>(Often() ? Pick(kFlags) : Next()));
        static const unsigned char kPages[] = {0, 1, 2, 3, 4, 0x7F, 0x80, 0xFF};
        B(at::kItemPage) = Pick(kPages);
        static const unsigned char kCursors[] = {0, 1, 2, 6, 7, 8, 9, 10, 0xFF, 0x80};
        if (Often()) B(at::kItemCursor) = Pick(kCursors);
        static const std::uint16_t kScrolls[] = {0, 1, 2, 3, 4, 6, 7, 8, 9, 0xFFFF, 0xFFFD, 0xFFFC, 0xFFFB, 0x7FFF, 0x8000};
        if (Often()) SetWord(At(at::kItemScroll), Pick(kScrolls));
        if (Often()) B(at::kItemState) = 0;
        if (Half()) SetWord(At(at::kItemScrollMotion), 0);
        break;
    }
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[150];
    unsigned paths[kCount];
    U seen[kCount][64];
    unsigned browse_cancel, browse_used, browse_refused, browse_97, page_wrap;
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    U path = 0x811C9DC5u;
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        if (out.log[i].what < 150) ++g_cover.logged[out.log[i].what];
        path = (path ^ out.log[i].what) * 0x01000193u;
        if (out.log[i].what == 6) path = (path ^ out.log[i].a) * 0x01000193u;
    }
    bool known = false;
    for (unsigned i = 0; i < g_cover.paths[k] && i < 64; ++i) known = known || g_cover.seen[k][i] == path;
    if (!known && g_cover.paths[k] < 64) g_cover.seen[k][g_cover.paths[k]++] = path;
    if (k == kBrowse) {
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
            if (out.log[i].what != 6) continue;
            if (out.log[i].a == 0x106) ++g_cover.browse_cancel;
            if (out.log[i].a == 0x103) ++g_cover.browse_used;
            if (out.log[i].a == 0x107) ++g_cover.browse_refused;
        }
        if (Byte(out, at::kPhase2) == 7 && Byte(in, at::kPhase2) != 7) ++g_cover.browse_97;
        const unsigned before = Byte(in, at::kItemPage), after = Byte(out, at::kItemPage);
        if ((before == 0 && after == 3) || (before == 3 && after == 0)) ++g_cover.page_wrap;
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_actor_copies: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    unsigned entries = 0;
    for (const Table& t : kTables) entries += t.count;
    if (entries != kTableEntries) bof3::Fatal("battle_actor_copies: the tables hold %u entries, not %u", entries, kTableEntries);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("battle_actor_copies: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&BattleTarget_Dispatch), reinterpret_cast<const void*>(&BattleTarget_ShowPrompt),
        reinterpret_cast<const void*>(&BattleTarget_PickDispatch), reinterpret_cast<const void*>(&BattleTarget_Begin),
        reinterpret_cast<const void*>(&BattleTarget_PickEnemy), reinterpret_cast<const void*>(&BattleTarget_Confirm),
        reinterpret_cast<const void*>(&BattleItem_Dispatch), reinterpret_cast<const void*>(&BattleItem_OpenDispatch),
        reinterpret_cast<const void*>(&BattleItem_OpenWindow), reinterpret_cast<const void*>(&BattleItem_Browse),
        reinterpret_cast<const void*>(&BattleItem_Choose), reinterpret_cast<const void*>(&BattleItem_TargetDispatch),
        reinterpret_cast<const void*>(&BattleItem_TargetBegin), reinterpret_cast<const void*>(&BattleItem_PickEnemy),
        reinterpret_cast<const void*>(&BattleItem_PickParty), reinterpret_cast<const void*>(&BattleItem_Confirm)};

    static State saved, input, their_out, our_out;
    U saved_tables[kTableEntries];
    {
        unsigned n = 0;
        for (const Table& t : kTables)
            for (unsigned i = 0; i < t.count; ++i) saved_tables[n++] = static_cast<U>(Long(At(t.at + 4 * i)));
    }
    Capture(saved);
    g = kStubs;
    for (const Table& t : kTables)
        for (unsigned i = 0; i < t.count; ++i) SetPtr(t.at + 4 * i, reinterpret_cast<const void*>(t.stubs[i]));

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const U v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.cmd) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.act) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.list) b = static_cast<unsigned char>(Next());
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? ours[k] : clones[k];
            reinterpret_cast<void (__cdecl*)()>(const_cast<void*>(fn))();
            Capture(out);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_actor_copies self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    {
        unsigned n = 0;
        for (const Table& t : kTables)
            for (unsigned i = 0; i < t.count; ++i) SetLong(At(t.at + 4 * i), static_cast<std::int32_t>(saved_tables[n++]));
    }
    Apply(saved);

    bof3::Log("shadow      battle_actor_copies self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the phase bytes, the input words, the item window, the message queue, the kept pages, the "
              "item records, the command and actor buffers, the list and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_actor_copies: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned entries_hit = 0;
    for (unsigned i = 100; i < 150; ++i) entries_hit += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      battle_actor_copies coverage: table entries %u of %u; paths per function %u %u %u %u %u %u %u %u "
              "%u %u %u %u %u %u %u %u (64 at most); browse: cancelled %u, used %u, refused %u, item 0x97 %u, page wrapped %u; "
              "calls: prompt %u, default target %u, previous target %u, wrap %u, repeat %u, cues %u, list %u, message %u, "
              "can use %u, return true %u, free %u",
              entries_hit, kTableEntries, c.paths[0], c.paths[1], c.paths[2], c.paths[3], c.paths[4], c.paths[5], c.paths[6],
              c.paths[7], c.paths[8], c.paths[9], c.paths[10], c.paths[11], c.paths[12], c.paths[13], c.paths[14], c.paths[15],
              c.browse_cancel, c.browse_used, c.browse_refused, c.browse_97, c.page_wrap, c.logged[1], c.logged[2],
              c.logged[3], c.logged[4], c.logged[5], c.logged[6], c.logged[8], c.logged[9], c.logged[10], c.logged[11],
              c.logged[12]);
    if (bad) bof3::Fatal("group CH (the target choice and the battle item window) differs from the original in %u self-test rounds", bad);
}

}  // namespace battle_actor_copies

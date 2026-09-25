// BOF3X_SHADOW=battle_actions: a differential fuzz of the action phases, once
// at start-up. docs/battle_actions.md section 4.
//
// Twenty-three byte-copies, every relative call out re-aimed at a recording
// stand-in (bof3::CloneCall with `expected`); the seven step tables' entries
// in .data and the event hook 0x904B6C pointed at recorders, which the copies
// and ours both read through memory. One round: one function, random bytes in
// every region any of them touches, the pointers and indices put back inside
// what they index, each branch's boundaries seeded; theirs, then from the
// same state ours; the regions, the text buffer and the stand-ins' log
// compared. Everything is put back afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_actions_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_actions {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }
unsigned char* PartyObj(unsigned i) { return At(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyObj(unsigned i) { return At(at::kEnemy + i * at::kEnemyStride); }
// The object of actor 0..10.
unsigned char* ActorObj(unsigned a) { return a < 3 ? PartyObj(a) : EnemyObj(a - 3); }
unsigned char* ActionOf(unsigned a) { return a < 3 ? PartyObj(a) + 0x124 : EnemyObj(a - 3) + 0x104; }

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

constexpr unsigned kText = 0x200;
unsigned char g_text[kText];   // what Msg_SystemPtr and Item_NamePtr answer into

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p), text = Address(g_text);
    if (at >= text && at < text + sizeof g_text) return 0x10000u + (at - text);
    return at;
}
std::uint32_t Bytes(const unsigned char* p, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}

// An action id inside the regions the fuzz fills: the high byte a category
// 0..3 (the item rows), the whole word below 0x400 (the ability records and
// rows are filled that far). The edges the code tests come often.
unsigned ActionId(std::uint32_t h) {
    static const unsigned kEdges[] = {0x97, 0x24, 0x25, 0x8C, 0, 0x96, 0x98, 0x23, 0x26, 0x8B, 0x8D, 0x124, 0x197};
    if (h % 3 == 0) return kEdges[(h >> 4) % (sizeof kEdges / sizeof kEdges[0])];
    return (h >> 8) & 0x3FF;
}
// An actor byte 0..10.
unsigned Actor(std::uint32_t h) { return h % 11; }

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the objects, indices inside the tables.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 22) {
    case 0: At(at::kSub)[0] = static_cast<unsigned char>(v % 6); break;
    case 1: At(at::kSub2)[0] = static_cast<unsigned char>(v % 3); break;
    case 2: At(at::kQueueAt)[0] = static_cast<unsigned char>(v % 0x17); break;
    case 3: At(at::kActor)[0] = static_cast<unsigned char>(Actor(v)); break;
    case 4: At(at::kActor + at::kBKind)[0] = static_cast<unsigned char>(v % 7); break;
    case 5: SetPtr(at::kActor + at::kBObject, ActorObj(Actor(w))); break;
    case 6: SetPtr(at::kActor + at::kBAction, ActionOf(Actor(w))); break;
    case 7: At(at::kTarget)[0] = static_cast<unsigned char>(Actor(v)); break;
    case 8: SetWord(At(at::kAbility), ActionId(h >> 6)); break;
    case 9: SetWord(At(at::kPending), v % 3 == 0 ? w : 0); break;
    case 10: At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] ^ (1u << (v % 8))); break;
    case 11: At(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(At(at::kRoundFlags + 1)[0] ^ (1u << (v % 8))); break;
    case 12: At(at::kEventBattle)[0] = static_cast<unsigned char>(v % 2 ? 0 : v | 1); break;
    case 13: At(at::kBattleEnd)[0] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 14: At(at::kMsgCount)[0] = static_cast<unsigned char>(v % 9); break;
    case 15: SetWord(At(at::kCancelButtons), h >> 16); break;
    case 16: SetWord(At(at::kInputHeld), h >> 16); break;
    case 17: {
        unsigned char* const o = ActorObj(Actor(w));
        const unsigned flags = Actor(w) < 3 ? 0x130 : 0x110;
        o[flags] = static_cast<unsigned char>(o[flags] ^ 0x40);
        o[1] = static_cast<unsigned char>(v % 3 == 0 ? 6 : v);
        break;
    }
    case 18: {
        const unsigned a = Actor(w);
        unsigned char* const o = ActorObj(a);
        if (v % 2) SetWord(o + (a < 3 ? 0x90 : 0x92), h >> 16);
        else SetWord(o + (a < 3 ? 0x9A : 0xA6), v);
        break;
    }
    case 19: At(at::kStep)[0] = static_cast<unsigned char>(v % 5); break;
    case 20: At(at::kCounter)[0] = static_cast<unsigned char>(v); break;
    default: At(at::kActor + at::kBKind)[0] = static_cast<unsigned char>(v % 2 ? 4 : 5); break;
    }
}

// --- the stand-ins ---------------------------------------------------------

// The step tables' entries.
template <unsigned N> void __cdecl StubStep() { Record(N, At(at::kSub)[0], At(at::kSub2)[0]); Disturb(); }
void __cdecl StubHook(int n) {
    Record(40, static_cast<std::uint32_t>(n));
    // BattleAction_EnterKind reads the kind after the hook, BattleAction_End
    // the battle end and the event byte
    const std::uint32_t h = Hash();
    if (h % 2) At(at::kActor + at::kBKind)[0] = static_cast<unsigned char>((h >> 8) % 7);
    if ((h >> 1) % 3 == 0) At(at::kBattleEnd)[0] = static_cast<unsigned char>((h >> 3) % 2);
    if ((h >> 2) % 3 == 0) At(at::kEventBattle)[0] = static_cast<unsigned char>((h >> 5) % 2);
    Disturb();
}
// The pick moves the target byte half the time - to a side or past 10 too,
// which only Battle_BeginAction reads, and it bounds them.
void __cdecl StubMemberAuto(unsigned m) {
    Record(41, m & 0xFF);
    static const unsigned char kTargets[] = {0, 1, 2, 3, 7, 10, 11, 0x40, 0x80, 0xC0, 0xFF};
    if (Hash() % 2) At(at::kTarget)[0] = kTargets[(Hash() >> 8) % sizeof kTargets];
    Disturb();
}
void __cdecl StubEnemyPick(unsigned e) {
    Record(42, e & 0xFF);
    static const unsigned char kTargets[] = {0, 2, 3, 9, 10, 11, 0x40, 0xC0};
    if (Hash() % 2) At(at::kTarget)[0] = kTargets[(Hash() >> 8) % sizeof kTargets];
    if (Hash() % 3 == 0) At(at::kQueueAt)[0] = static_cast<unsigned char>((Hash() >> 5) % 0x17);
    Disturb();
}
void __cdecl StubClearActing() {
    Record(43);
    if (Hash() % 2) At(at::kSub)[0] = static_cast<unsigned char>(Hash() % 2 ? 2 : (Hash() >> 7) % 6);
    if (Hash() % 3 == 0) At(at::kActor)[0] = static_cast<unsigned char>(Actor(Hash() >> 9));
    Disturb();
}
void __cdecl StubOpenWindow() { Record(44); Disturb(); }
const unsigned char* __cdecl StubMsg(unsigned id) {
    Record(45, id & 0xFFFF);
    Disturb();
    return g_text + (Hash() >> 5) % 0x100;
}
// The banner hashes what the name buffer holds when it is given it.
unsigned long __cdecl StubBanner(unsigned kind, unsigned b2, unsigned layer, unsigned timer, const char* text) {
    const unsigned char* const t = reinterpret_cast<const unsigned char*>(text);
    Record(46, (kind & 0xFF) | (b2 & 0xFF) << 8 | (layer & 0xFF) << 16 | (timer & 0xFF) << 24, Id(text),
           Address(text) == at::kNameBuf ? Bytes(t, 16) : 0);
    Disturb();
    return Hash();
}
unsigned long __cdecl StubSetBit(unsigned a) {
    Record(47, a & 31);
    // re-read after it: the actor, the object and record pointers, the id
    const std::uint32_t h = Hash();
    if (h % 3 == 0) At(at::kActor)[0] = static_cast<unsigned char>(Actor(h >> 8));
    if ((h >> 1) % 3 == 0) SetPtr(at::kActor + at::kBObject, ActorObj(Actor(h >> 12)));
    if ((h >> 2) % 3 == 0) SetPtr(at::kActor + at::kBAction, ActionOf(Actor(h >> 16)));
    if ((h >> 3) % 3 == 0) SetWord(At(at::kAbility), ActionId(h >> 7));
    Disturb();
    return Hash();
}
unsigned char __cdecl StubActionSuits() { Record(48); Disturb(); return static_cast<unsigned char>(Hash() % 3 == 0 ? Hash() >> 8 : 0); }
void __cdecl StubRemove(unsigned a) {
    Record(49, a & 0xFF);
    if (Hash() % 2) At(at::kActor)[0] = static_cast<unsigned char>(Actor(Hash() >> 8));
    Disturb();
}
char* __cdecl StubCopyN(char* dst, const char* src, unsigned n) {
    Record(50, Id(dst), Id(src), n & 0xFF);
    const std::uint32_t h = Hash();
    std::memcpy(dst + (h % 8), &h, sizeof h);
    Disturb();
    return dst;
}
// Costs at the AP's edges: the actor's AP (read by the caller before the
// call), one above and one below.
unsigned char __cdecl StubApCost(unsigned m, unsigned id, unsigned battle) {
    Record(51, m & 0xFF, id & 0xFF, battle & 0xFF);
    const unsigned a = At(at::kActor)[0];
    const unsigned ap = a < 11 ? Word(ActorObj(a) + (a < 3 ? 0x9A : 0xA6)) : 0;
    Disturb();
    const std::uint32_t h = Hash();
    const unsigned edge = (h % 3 == 0) ? ap : (h % 3 == 1) ? ap + 1 : ap - 1;
    return static_cast<unsigned char>(h % 5 == 0 ? h >> 8 : edge);
}
unsigned char __cdecl StubPickFlag8() { Record(52); Disturb(); return static_cast<unsigned char>(Hash() % 2 ? 0 : Hash() >> 8); }
// 0x42F9D0 picks the id again (word 0x904B80 and the record's +2) and
// answers a target 0..10 - a side would send BattleAction_AbilityCommit far
// past the enemy objects (docs/battle_actions.md section 3), on both sides.
unsigned char __cdecl StubPickTarget() {
    Record(53);
    const std::uint32_t h = Hash();
    if (h % 2) SetWord(At(at::kAbility), ActionId(h >> 3));
    Disturb();
    return static_cast<unsigned char>(Actor(Hash() >> 4));
}
void __cdecl StubLoadAbility(unsigned a) { Record(54, a & 0xFF); Disturb(); }
int __cdecl StubLoadDone() { Record(55); Disturb(); const std::uint32_t h = Hash(); return h % 2 ? 0 : static_cast<int>(h | 0x100); }
void __cdecl StubStartAbility(unsigned a, unsigned owner) { Record(56, a & 0xFF, owner); Disturb(); }
unsigned char __cdecl StubItemSuits() { Record(57); Disturb(); return static_cast<unsigned char>(Hash() % 3 == 0 ? Hash() >> 8 : 0); }
void __cdecl StubReturnItem(unsigned a) {
    Record(58, a & 0xFF);
    if (Hash() % 2) At(at::kActor)[0] = static_cast<unsigned char>(Actor(Hash() >> 8));
    Disturb();
}
unsigned char* __cdecl StubItemName(unsigned category, unsigned item) {
    Record(59, category & 0xFF, item & 0xFF);
    Disturb();
    return g_text + 0x100 + (Hash() >> 5) % 0xF0;
}
void __cdecl StubLoadItem(unsigned id) { Record(60, id & 0xFFFF); Disturb(); }
void __cdecl StubStartItem(unsigned id, unsigned owner) { Record(61, id & 0xFFFF, owner); Disturb(); }
unsigned char __cdecl StubAnyF0() {
    Record(62);
    if (Hash() % 3 == 0) SetWord(At(at::kPending), Hash() % 2 ? 0 : Hash() >> 16);
    Disturb();
    return static_cast<unsigned char>(Hash() % 3 == 0 ? Hash() >> 8 : 0);
}
void __cdecl StubLoadDat(int file) { Record(63, static_cast<std::uint32_t>(file)); Disturb(); }
void __cdecl StubClutRow(unsigned row) { Record(64, row & 0xFF); Disturb(); }
unsigned char __cdecl StubSettle() {
    Record(65);
    if (Hash() % 2) At(at::kSub)[0] = static_cast<unsigned char>(Hash() >> 9);
    Disturb();
    return static_cast<unsigned char>(Hash() % 2 ? 0 : Hash() >> 8);
}
unsigned long __cdecl StubQueuePush(unsigned a, unsigned b, unsigned long value) {
    Record(66, a & 0xFF, b & 0xFF, Id(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(value))));
    if (Hash() % 2) At(at::kMsgCount)[0] = static_cast<unsigned char>((Hash() >> 8) % 9);
    Disturb();
    return Hash();
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x453FA0: return f(&StubMemberAuto);
    case 0x435AB0: return f(&StubEnemyPick);
    case 0x4301B0: return f(&StubClearActing);
    case 0x444310: return f(&StubOpenWindow);
    case 0x497740: return f(&StubMsg);
    case 0x44A650: return f(&StubBanner);
    case 0x446FB0: return f(&StubSetBit);
    case 0x453210: return f(&StubActionSuits);
    case 0x446650: return f(&StubRemove);
    case 0x5171A0: return f(&StubCopyN);
    case 0x591DB0: return f(&StubApCost);
    case 0x4537A0: return f(&StubPickFlag8);
    case 0x42F9D0: return f(&StubPickTarget);
    case 0x4379D0: return f(&StubLoadAbility);
    case 0x454810: return f(&StubLoadDone);
    case 0x437930: return f(&StubStartAbility);
    case 0x4532A0: return f(&StubItemSuits);
    case 0x446EA0: return f(&StubReturnItem);
    case 0x591680: return f(&StubItemName);
    case 0x4377D0: return f(&StubLoadItem);
    case 0x437780: return f(&StubStartItem);
    case 0x453190: return f(&StubAnyF0);
    case 0x454590: return f(&StubLoadDat);
    case 0x4549F0: return f(&StubClutRow);
    case 0x453B10: return f(&StubSettle);
    case 0x44A880: return f(&StubQueuePush);
    default: bof3::Fatal("battle_actions: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

const Callees kStubs = {
    StubMemberAuto, StubEnemyPick, StubClearActing, StubOpenWindow, StubMsg, StubBanner, StubSetBit,
    StubActionSuits, StubRemove, StubCopyN, StubApCost, StubPickFlag8, StubPickTarget, StubLoadAbility,
    StubLoadDone, StubStartAbility, StubItemSuits, StubReturnItem, StubItemName, StubLoadItem, StubStartItem,
    StubAnyF0, StubLoadDat, StubClutRow, StubSettle, StubQueuePush,
};

// The seven step tables and a recorder per entry.
using Handler = void (__cdecl*)();
struct StepTable { std::uint32_t at; unsigned n; Handler stubs[6]; };
const StepTable kTables[] = {
    {at::kSteps, 5, {&StubStep<100>, &StubStep<101>, &StubStep<102>, &StubStep<103>, &StubStep<104>}},
    {at::kBeginSteps, 2, {&StubStep<105>, &StubStep<106>}},
    {at::kKindSteps, 6, {&StubStep<107>, &StubStep<108>, &StubStep<109>, &StubStep<110>, &StubStep<111>, &StubStep<112>}},
    {at::kAbilitySteps, 3, {&StubStep<113>, &StubStep<114>, &StubStep<115>}},
    {at::kItemSteps, 3, {&StubStep<116>, &StubStep<117>, &StubStep<118>}},
    {at::kEffectSteps, 3, {&StubStep<119>, &StubStep<120>, &StubStep<121>}},
    {at::kAfterSteps, 3, {&StubStep<122>, &StubStep<123>, &StubStep<124>}},
};
constexpr unsigned kTableCount = sizeof kTables / sizeof kTables[0];

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    int table;   // for a dispatch: its index in kTables, and the byte it reads; else -1
    std::uint32_t index_at;
};

constexpr Call kBeginCalls[] = {{0x54, 0x453FA0}, {0x61, 0x435AB0}};
constexpr Call kPlainCalls[] = {{0x0, 0x4301B0}, {0x62, 0x444310}, {0x69, 0x497740}, {0x77, 0x44A650}};
constexpr Call kOneCalls[] = {{0x6, 0x446FB0}};
constexpr Call kAbilityCheckCalls[] = {{0x12, 0x453210}, {0x22, 0x446650}, {0x95, 0x5171A0}, {0xA6, 0x44A650},
                                       {0x11B, 0x444310}, {0x122, 0x497740}, {0x12E, 0x44A650}, {0x16F, 0x591DB0},
                                       {0x189, 0x591DB0}, {0x1A7, 0x444310}, {0x1AE, 0x497740}, {0x1BA, 0x44A650},
                                       {0x1D7, 0x4537A0}};
constexpr Call kAbilityCommitCalls[] = {{0x30, 0x446FB0}, {0x5E, 0x42F9D0}, {0x134, 0x4379D0}};
constexpr Call kAbilityStartCalls[] = {{0x0, 0x454810}, {0x19, 0x437930}};
constexpr Call kItemCheckCalls[] = {{0x10, 0x4532A0}, {0x20, 0x446EA0}, {0x2B, 0x446650},
                                    {0x7E, 0x591680}, {0x8C, 0x5171A0}, {0x9E, 0x44A650}};
constexpr Call kItemCommitCalls[] = {{0x10, 0x446FB0}, {0x20, 0x4377D0}};
constexpr Call kItemStartCalls[] = {{0x0, 0x454810}, {0x1A, 0x437780}};
constexpr Call kWaitCalls[] = {{0x30, 0x446FB0}, {0x76, 0x446FB0}, {0x8F, 0x453190}};
constexpr Call kPreloadCalls[] = {{0x60, 0x454590}};
constexpr Call kLoadedCalls[] = {{0x0, 0x454810}, {0x36, 0x4549F0}};
constexpr Call kSettleCalls[] = {{0x13, 0x453B10}};
constexpr Call kMessagesCalls[] = {{0x6B, 0x497740}, {0x75, 0x44A880}};
constexpr Call kEndCalls[] = {{0x61, 0x4301B0}, {0x164, 0x44A650}};

enum : unsigned {
    kActionPhase, kBeginStep, kBeginAction, kEnterKind, kKindStep, kKindPlain, kKindOne, kAbilityStep,
    kAbilityCheck, kAbilityCommit, kAbilityStart, kItemStep, kItemCheck, kItemCommit, kItemStart,
    kEffectStep, kEffectWait, kEffectPreload, kEffectLoaded, kAfterStep, kAfterSettle, kEnemyMessages, kEnd,
    kCount
};

#define BA_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), -1, 0}
#define BA_P(name, base, size) {name, base, size, nullptr, 0, -1, 0}
#define BA_T(name, base, table, index) {name, base, 0xE, nullptr, 0, table, index}
const Clone kClones[kCount] = {
    {"Battle_ActionPhase", 0x42F220, 0x2C, nullptr, 0, 0, at::kStep},
    BA_T("BattleAction_BeginStep", 0x42F250, 1, at::kSub),
    BA_C("Battle_BeginAction", 0x42F260, 0x257, kBeginCalls),
    BA_P("BattleAction_EnterKind", 0x42F4C0, 0x3A),
    BA_T("BattleAction_KindStep", 0x42F500, 2, at::kSub),
    BA_C("BattleAction_KindPlain", 0x42F510, 0x96, kPlainCalls),
    BA_C("BattleAction_KindOne", 0x42F5B0, 0x2E, kOneCalls),
    BA_T("BattleAction_AbilityStep", 0x42F670, 3, at::kSub2),
    BA_C("BattleAction_AbilityCheck", 0x42F680, 0x1F6, kAbilityCheckCalls),
    BA_C("BattleAction_AbilityCommit", 0x42F880, 0x149, kAbilityCommitCalls),
    BA_C("BattleAction_AbilityStart", 0x42FAB0, 0x3C, kAbilityStartCalls),
    BA_T("BattleAction_ItemStep", 0x42FAF0, 4, at::kSub2),
    BA_C("BattleAction_ItemCheck", 0x42FB00, 0xC1, kItemCheckCalls),
    BA_C("BattleAction_ItemCommit", 0x42FBD0, 0x35, kItemCommitCalls),
    BA_C("BattleAction_ItemStart", 0x42FC10, 0x3D, kItemStartCalls),
    BA_T("BattleAction_EffectStep", 0x42FC50, 5, at::kSub),
    BA_C("BattleAction_EffectWait", 0x42FC60, 0xB5, kWaitCalls),
    BA_C("BattleAction_EffectPreload", 0x42FD20, 0x6F, kPreloadCalls),
    BA_C("BattleAction_EffectLoaded", 0x42FD90, 0x3D, kLoadedCalls),
    BA_T("BattleAction_AfterStep", 0x42FDD0, 6, at::kSub),
    BA_C("BattleAction_AfterSettle", 0x42FDE0, 0x38, kSettleCalls),
    BA_C("BattleAction_EnemyMessages", 0x42FF70, 0x9A, kMessagesCalls),
    BA_C("BattleAction_End", 0x430010, 0x195, kEndCalls),
};
#undef BA_C
#undef BA_P
#undef BA_T

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kParty, 0xE50},          // ObjTrio and on: the member formula reaches actor 10's +2, 0x8031F3 inside
    {at::kPhase, 0x160},          // the battle's globals 0x904AA0..0x904C00: the queue, both blocks, the hook
    {at::kText0, 0x40},           // Text_Records[0] and [1]
    {at::kNameBuf, 0x20},
    {at::kMsgBusy, 0x2350},       // 0x939F60..0x93C2B0: the message list, the stat copy, the enemies, 0x93C2A2
    {at::kCancelButtons, 2},
    {at::kInputHeld, 2},
    {at::kStandOffsets, 0x80},    // constant data from here on - random here, put back after
    {at::kAbilityRecords, 0x6000},// the action records, ids below 0x400
    {at::kAbilityRows, 0x8E8},    // the ability rows and the magic files
    {0x64B284, 0x1C0},            // the item rows 0..2
    {0x675ED8, 0x100},            // the item rows 3
    {at::kTextMiss, 0x18},        // the two banner texts' pointers
};
constexpr unsigned kRegionBytes = 0xE50 + 0x160 + 0x40 + 0x20 + 0x2350 + 2 + 2 + 0x80 + 0x6000 + 0x8E8 + 0x1C0 + 0x100 + 0x18;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char text[kText];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.text, g_text, sizeof g_text);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_text, s.text, sizeof g_text);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
unsigned Byte(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

// Random bytes put back inside what they index: the actor and target bytes,
// both blocks' pointers, every object's action id and stand index, the turn
// order, the step bytes, the message list, the hook.
void Fix() {
    for (unsigned a = 0; a < 11; ++a) {
        unsigned char* const o = ActorObj(a);
        SetWord(ActionOf(a) + 2, ActionId(Next()));
        if (a < 3) {
            o[8] = static_cast<unsigned char>(Next() % 4);
            o[0x89] = static_cast<unsigned char>(Next() % 8);
        }
    }
    At(at::kActor)[0] = static_cast<unsigned char>(Actor(Next()));
    At(at::kTarget)[0] = static_cast<unsigned char>(Actor(Next()));
    SetPtr(at::kActor + at::kBObject, ActorObj(Actor(Next())));
    SetPtr(at::kActor + at::kBAction, ActionOf(Actor(Next())));
    SetPtr(at::kTarget + at::kBObject, ActorObj(Actor(Next())));
    SetPtr(at::kTarget + at::kBAction, ActionOf(Actor(Next())));
    SetWord(At(at::kAbility), ActionId(Next()));
    for (unsigned i = 0; i < 0x16; ++i)
        At(at::kQueue + i)[0] = static_cast<unsigned char>(Half() ? 0xFF : Actor(Next()));
    At(at::kQueueEnd)[0] = static_cast<unsigned char>(Next() % 0x17);
    At(at::kQueueAt)[0] = static_cast<unsigned char>(Next() % 0x17);
    At(at::kStep)[0] = static_cast<unsigned char>(Next() % 5);
    At(at::kSub)[0] = static_cast<unsigned char>(Next() % 6);
    At(at::kSub2)[0] = static_cast<unsigned char>(Next() % 3);
    At(at::kMsgCount)[0] = static_cast<unsigned char>(Next() % 9);
    for (unsigned n = 0; n <= 8; ++n) At(at::kMsgList + n * 4)[0] = static_cast<unsigned char>(Next() % 8);
    SetPtr(at::kEventHook, reinterpret_cast<const void*>(&StubHook));
    if (Half()) At(at::kEventBattle)[0] = 0;
    if (Half()) At(at::kBattleEnd)[0] = 0;
    if (Half()) At(at::kMsgBusy)[0] = 0;
    if (Half()) SetWord(At(at::kPending), 0);
}

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    switch (k) {
    case kActionPhase:
    case kBeginStep:
    case kKindStep:
    case kAbilityStep:
    case kItemStep:
    case kEffectStep:
    case kAfterStep: {
        const Clone& c = kClones[k];
        At(c.index_at)[0] = static_cast<unsigned char>(Next() % kTables[c.table].n);
        if (k == kActionPhase && Half()) SetWord(At(at::kInputHeld), 0);
        if (k == kActionPhase && Half()) SetWord(At(at::kCancelButtons), 1u << (Next() % 16));
        break;
    }
    case kBeginAction: {
        const unsigned end = Next() % 0x17;
        At(at::kQueueEnd)[0] = static_cast<unsigned char>(end);
        At(at::kQueueAt)[0] = static_cast<unsigned char>(Often() && end ? Next() % end : Next() % 0x17);
        static const unsigned char kTargets[] = {0, 1, 2, 3, 5, 10, 11, 0x40, 0x80, 0xC0, 0xFF};
        At(at::kTarget)[0] = kTargets[Next() % sizeof kTargets];
        if (Half()) for (unsigned i = 0; i < 0x16; ++i) if (Often()) At(at::kQueue + i)[0] = 0xFF;
        break;
    }
    case kEnterKind:
        At(at::kEventBattle)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        break;
    case kKindPlain:
        At(at::kSub)[0] = static_cast<unsigned char>(Half() ? 2 : Next() % 6);
        At(at::kActor)[0] = static_cast<unsigned char>(Half() ? 3 + Next() % 8 : Next() % 3);
        break;
    case kAbilityCheck: {
        const unsigned a = At(at::kActor)[0];
        unsigned char* const o = ActorObj(a);
        const unsigned status = a < 3 ? 0x90 : 0x92;
        if (Half()) o[status] = static_cast<unsigned char>(o[status] & ~0x10u);
        if (Half()) {
            const unsigned id = Word(ActionOf(a) + 2);   // may be the one read, or not
            At(at::kAbilityRecords + id * 24 + 0x15)[0] = static_cast<unsigned char>(At(at::kAbilityRecords + id * 24 + 0x15)[0] | 4);
        }
        SetPtr(at::kActor + at::kBAction, ActionOf(Half() ? a : Actor(Next())));
        if (Half()) SetWord(ActionOf(a) + 2, 0x97);
        if (Often()) SetWord(o + (a < 3 ? 0x9A : 0xA6), Next() % 0x120);
        break;
    }
    case kAbilityCommit:
        At(at::kRoundFlags)[0] = static_cast<unsigned char>(Often() ? At(at::kRoundFlags)[0] | 0x20 : At(at::kRoundFlags)[0] & ~0x20u);
        if (Half()) {
            static const unsigned kIds[] = {0x24, 0x25, 0x8C};
            SetWord(At(at::kAbility), kIds[Next() % 3]);
        }
        break;
    case kEffectWait:
        for (unsigned a = 0; a < 11; ++a) {
            unsigned char* const o = ActorObj(a);
            if (Half()) o[1] = 6;
            if (Half()) o[a < 3 ? 0x130 : 0x110] = static_cast<unsigned char>(o[a < 3 ? 0x130 : 0x110] & ~0x40u);
        }
        if (Often()) At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] | 4);
        break;
    case kEffectPreload: {
        At(at::kActor + at::kBKind)[0] = static_cast<unsigned char>(Often() ? 4 + Next() % 2 : Next() % 7);
        if (Half()) {
            // the row this action reaches: its file none
            const unsigned kind = At(at::kActor + at::kBKind)[0];
            unsigned row = 0;
            if (kind == 4) row = At(at::kAbilityRows + Word(At(at::kAbility)))[0];
            else {
                const unsigned w = Word(move_script::At(static_cast<std::uint32_t>(Long(At(at::kActor + at::kBAction)))) + 2);
                const auto* const tables = reinterpret_cast<const unsigned char* const*>(static_cast<std::uintptr_t>(at::kItemRows));
                row = tables[w >> 8][w & 0xFF];
            }
            SetWord(At(at::kMagicFiles + row * 8), 0xFFFF);
        }
        break;
    }
    case kEffectLoaded:
        At(at::kEventBattle)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        break;
    case kAfterSettle:
    case kEnd:
        if (Often()) At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] | 4);
        if (Often()) SetWord(At(at::kPending), 0);
        if (k == kEnd && Half()) At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] | 0x40);
        if (k == kEnd && Half()) At(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(At(at::kRoundFlags + 1)[0] | 0x10);
        break;
    case kEnemyMessages:
        if (Often()) At(at::kMsgBusy)[0] = 0;
        if (Half()) At(at::kMsgCount)[0] = static_cast<unsigned char>(Half() ? 0 : 1);
        break;
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[130];
    unsigned begin_none, begin_found, target_block, suits, ability_banner, status_gate, ap_short, ap_ok, commit,
        swaps, battle_end, messages, preload;
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 130) ++g_cover.logged[out.log[i].what];
    switch (k) {
    case kBeginAction:
        if (Byte(out, at::kPhase) == 4 && Byte(in, at::kPhase) != 4) ++g_cover.begin_none;
        if (out.log_n) ++g_cover.begin_found;
        if (out.log_n && Byte(out, at::kTarget) <= 10 && Byte(in, at::kTarget + 8) != Byte(out, at::kTarget + 8)) ++g_cover.target_block;
        break;
    case kAbilityCheck:
    case kItemCheck:
        if (out.log_n >= 2 && (out.log[1].what == 49 || out.log[1].what == 58)) ++g_cover.suits;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
            if (out.log[i].what == 45 && out.log[i].a == 0x37) ++g_cover.status_gate;
            if (out.log[i].what == 45 && out.log[i].a == 0x36) ++g_cover.ap_short;
            if (out.log[i].what == 52) ++g_cover.ap_ok;
            if (out.log[i].what == 46 && (out.log[i].a & 0xFF) == 1) ++g_cover.ability_banner;
        }
        break;
    case kAbilityCommit:
        if (out.log_n) ++g_cover.commit;
        break;
    case kEnd:
        if (Byte(out, at::kSub) == 1) ++g_cover.swaps;
        if (Byte(out, at::kPhase) == 4 && Byte(out, at::kStep) == 2) ++g_cover.battle_end;
        break;
    case kEnemyMessages:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 66) ++g_cover.messages;
        break;
    case kEffectPreload:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 63) ++g_cover.preload;
        break;
    default:
        break;
    }
}

using Fn0 = void (__cdecl*)();

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_actions: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    // Each dispatch's operand names its table (read before anything moves).
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        if (c.table < 0) continue;
        const std::uint32_t disp_at = c.base + 10;   // ff 14 85 / ff 24 85 disp32 after 7 bytes of set-up
        std::uint32_t disp;
        std::memcpy(&disp, At(disp_at), sizeof disp);
        if (disp != kTables[c.table].at)
            bof3::Fatal("battle_actions: %s's dispatch reads 0x%X, not the table 0x%X", c.name, static_cast<unsigned>(disp),
                        static_cast<unsigned>(kTables[c.table].at));
    }

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("battle_actions: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Battle_ActionPhase), reinterpret_cast<const void*>(&BattleAction_BeginStep),
        reinterpret_cast<const void*>(&Battle_BeginAction), reinterpret_cast<const void*>(&BattleAction_EnterKind),
        reinterpret_cast<const void*>(&BattleAction_KindStep), reinterpret_cast<const void*>(&BattleAction_KindPlain),
        reinterpret_cast<const void*>(&BattleAction_KindOne), reinterpret_cast<const void*>(&BattleAction_AbilityStep),
        reinterpret_cast<const void*>(&BattleAction_AbilityCheck), reinterpret_cast<const void*>(&BattleAction_AbilityCommit),
        reinterpret_cast<const void*>(&BattleAction_AbilityStart), reinterpret_cast<const void*>(&BattleAction_ItemStep),
        reinterpret_cast<const void*>(&BattleAction_ItemCheck), reinterpret_cast<const void*>(&BattleAction_ItemCommit),
        reinterpret_cast<const void*>(&BattleAction_ItemStart), reinterpret_cast<const void*>(&BattleAction_EffectStep),
        reinterpret_cast<const void*>(&BattleAction_EffectWait), reinterpret_cast<const void*>(&BattleAction_EffectPreload),
        reinterpret_cast<const void*>(&BattleAction_EffectLoaded), reinterpret_cast<const void*>(&BattleAction_AfterStep),
        reinterpret_cast<const void*>(&BattleAction_AfterSettle), reinterpret_cast<const void*>(&BattleAction_EnemyMessages),
        reinterpret_cast<const void*>(&BattleAction_End)};

    static State saved, input, their_out, our_out;
    std::uint32_t saved_tables[kTableCount][6];
    for (unsigned t = 0; t < kTableCount; ++t) {
        std::memcpy(saved_tables[t], At(kTables[t].at), kTables[t].n * 4);
        for (unsigned i = 0; i < kTables[t].n; ++i) SetPtr(kTables[t].at + 4 * i, reinterpret_cast<const void*>(kTables[t].stubs[i]));
    }
    Capture(saved);
    g = kStubs;

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.text) b = static_cast<unsigned char>(Next());
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        // the step tables' entries are in no region: swapped once, for good
        Fix();
        g_seed = Next();
        Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? ours[k] : clones[k];
            reinterpret_cast<Fn0>(const_cast<void*>(fn))();
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
                bof3::Log("shadow      battle_actions self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    Apply(saved);
    for (unsigned t = 0; t < kTableCount; ++t) std::memcpy(At(kTables[t].at), saved_tables[t], kTables[t].n * 4);

    bof3::Log("shadow      battle_actions self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the party and enemy objects, the battle's globals, the text records, the tables, the text "
              "buffer and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_actions: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned entries = 0;
    for (unsigned i = 100; i < 125; ++i) entries += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      battle_actions coverage: step entries %u of 25, hook %u; begin: none left %u, found %u (target "
              "block %u); checks: turned away %u, name banners %u, status gate %u, AP short %u, AP enough %u; commits %u; "
              "preloads %u; messages %u; end: swaps %u, battle end %u",
              entries, c.logged[40], c.begin_none, c.begin_found, c.target_block, c.suits, c.ability_banner, c.status_gate,
              c.ap_short, c.ap_ok, c.commit, c.preload, c.messages, c.swaps, c.battle_end);
    if (bad) bof3::Fatal("the action phases differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_actions

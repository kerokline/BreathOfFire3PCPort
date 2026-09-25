// BOF3X_SHADOW=enemy_ai_ops: a differential fuzz of the enemy AI script ops,
// once at start-up. docs/enemy_ai_ops.md section 4.
//
// Twenty-two byte-copies, every call and tail jump out re-aimed at a
// recording stand-in (bof3::CloneCall with `expected`); the 46 code pointers
// of the op tables 0x64B1A0..0x64B254 swapped for recorders that each log
// their slot (so a dispatch through the wrong table, or by the wrong step
// byte, shows); every enemy object's +0xF4 hook pointed at a recorder, its
// +0xF8 cue table and the command's target byte (*0x939FA0) at buffers of
// our own. One round: one function, random bytes in every region any of
// them touches, the pointers and indices put back inside what the tables
// hold, each branch's boundaries seeded; theirs, then from the same state
// ours; the regions, the buffers and the stand-ins' log compared.
// Everything is put back afterwards.
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/enemy_ai_ops_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace enemy_ai_ops {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* EnemyObj(unsigned i) { return At(at::kEnemies + (i % at::kEnemyCount) * at::kEnemySize); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }
unsigned char* Current() { return At(static_cast<std::uint32_t>(Long(At(at::kEnemyCurrent)))); }

// --- the stand-ins' log and our buffers --------------------------------------

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

constexpr unsigned kTargets = 8;
unsigned char g_target[kTargets];   // *0x939FA0: the command's target byte
constexpr unsigned kCues = 24;
unsigned char g_cue[kCues];         // every enemy's +0xF8: the cue words +4 and +6

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
// Where the callee is: which object Sprite_Current and 0x939AD8 hold.
std::uint32_t Sc() { return Address(Sprite_Current); }
std::uint32_t En() { return static_cast<std::uint32_t>(Long(At(at::kEnemyCurrent))); }

// The step bytes inside the swapped block: +1 below 12, +2 below 6, +3 below 9.
void SmallSteps(unsigned char* e, std::uint32_t h) {
    e[1] = static_cast<unsigned char>(h % 12);
    e[2] = static_cast<unsigned char>((h >> 4) % 6);
    e[3] = static_cast<unsigned char>((h >> 8) % 9);
}
unsigned Actor(std::uint32_t h) { return h % 4 == 0 ? (h >> 2) % 64 : 3 + (h >> 2) % 8; }

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the arrays, indices inside the tables.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 24) {
    case 0: Sprite_Current = EnemyObj(v); break;
    case 1: SetPtr(at::kEnemyCurrent, EnemyObj(v)); break;
    case 2: SmallSteps(Sprite_Current, h >> 9); break;
    case 3: Sprite_Current[5] = static_cast<unsigned char>(Actor(h >> 9)); break;
    case 4: Sprite_Current[7] = static_cast<unsigned char>(v % 0x40); break;
    case 5: Current()[0x92 + w % 2] = static_cast<unsigned char>(v); break;
    case 6: Current()[0x10C] = static_cast<unsigned char>(v); break;
    case 7: SetWord(Current() + 0x108, v % 3 == 0 ? 0 : v % 3 == 1 ? 0x10000u - v : v); break;
    case 8: SetWord(Current() + 0x10A, v % 3 == 0 ? 0 : v % 3 == 1 ? 0x10000u - v : v); break;
    case 9: SetWord(Current() + 0xA4, v % 3 == 0 ? 0 : w); break;
    case 10: SetLong(Current() + 0x110, static_cast<std::int32_t>(h * 0x9E3779B1u)); break;
    case 11: {
        static const unsigned kBits[] = {0x40, 0x80, 0x1000, 0x2000};
        SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) ^ kBits[v % 4]);
        break;
    }
    case 12: At(at::kEventBattle)[0] = static_cast<unsigned char>(v % 2 ? 0 : v | 1); break;
    case 13: At(at::kTargeting)[0] = static_cast<unsigned char>(v % 2 ? 0 : v | 1); break;
    case 14: At(at::kActionKind)[0] = static_cast<unsigned char>(v % 7); break;
    case 15: SetWord(At(at::kMagicId), (w % 4) << 8 | v); break;
    case 16: At(at::kTarget)[0] = static_cast<unsigned char>(v); break;
    case 17: SetWord(At(at::kStatusAdd), w); break;
    case 18: SetPtr(at::kResult, EnemyObj(v) + 0x104); break;
    case 19: g_target[w % kTargets] = static_cast<unsigned char>(v); break;
    case 20: Sprite_Current[8] = static_cast<unsigned char>(v % 5); break;
    case 21: At(at::kTasks + Sprite_Current[5] * at::kTaskSize)[9] = static_cast<unsigned char>(v); break;
    case 22: At(at::kPulse)[0] = static_cast<unsigned char>(v); break;
    default: {
        const unsigned id = Word(At(at::kMagicId)) & 0xFF;
        At((w % 2 ? at::kAbilityFlags8 : at::kAbilityFlagsD) + id * 24)[0] ^= static_cast<unsigned char>(1u << (v % 8));
        break;
    }
    }
}

// --- the stand-ins ---------------------------------------------------------

// The 46 op-table slots 0x64B1A0..0x64B254.
constexpr std::uint32_t kBlock = at::kSteps;
constexpr unsigned kBlockEntries = 46;
template <unsigned N> void __cdecl StubSlot() { Record(200 + N, Sc(), En()); Disturb(); }
template <std::size_t... I> constexpr std::array<Handler, sizeof...(I)> SlotStubs(std::index_sequence<I...>) {
    return {&StubSlot<I>...};
}

// The callees. Each records what its callee reads of its arguments (the
// originals push bytes and words with other registers' upper bits), and the
// ones whose callers read something again after them move it half the time.
void __cdecl StubSetAnimation(unsigned a) {
    Record(1, a & 0xFF, Sc(), En());
    const std::uint32_t h = Hash();
    if (h % 2) SetPtr(at::kEnemyCurrent, EnemyObj(h >> 8));
    if ((h >> 1) % 2) Sprite_Current = EnemyObj(h >> 12);
    Disturb();
}
unsigned char __cdecl StubScriptTick() { Record(2, Sc(), En()); Disturb(); return static_cast<unsigned char>(Hash() >> 9); }
unsigned char __cdecl StubScriptTickOnce() {
    Record(3, Sc(), En());
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 ? 0 : h >> 9 | 1);
}
unsigned char __cdecl StubChance70() {
    Record(4, Sc(), En());
    if (Hash() % 2) SetWord(At(at::kMagicId), (Hash() >> 8) & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 ? 0 : (h >> 8) | 1);
}
void __cdecl StubDefeated() { Record(5, Sc(), En()); Disturb(); }
unsigned char __cdecl StubSpriteTickOnce() {
    Record(6, Sc(), En());
    if (Hash() % 2) SetPtr(at::kEnemyCurrent, EnemyObj(Hash() >> 8));
    Disturb();
    return static_cast<unsigned char>(Hash() >> 5);
}
unsigned long __cdecl StubStatusTint(unsigned status) { Record(7, status & 0xFFFF, Sc(), En()); Disturb(); return Hash(); }
unsigned char __cdecl StubSetTint(unsigned char* sprite, unsigned char r, unsigned char g_, unsigned char b, unsigned char a) {
    Record(8, Address(sprite), static_cast<std::uint32_t>(r) | g_ << 8 | b << 16, a);
    if (Hash() % 2) Sprite_Current = EnemyObj(Hash() >> 8);
    Disturb();
    return static_cast<unsigned char>(Hash() % 0x40);
}
void __cdecl StubTintRelease(unsigned char i) { Record(9, i, Sc()); Disturb(); }
void __cdecl StubReleaseTint(unsigned char* sprite) {
    Record(10, Address(sprite), En());
    if (Hash() % 2) Sprite_Current = EnemyObj(Hash() >> 8);
    Disturb();
}
// A delta at the pop-up / heal / miss edges, or anything; the result record
// pointer moved now and then (the original re-reads it for the store).
short __cdecl StubApplyDamage(unsigned attacker, unsigned target) {
    Record(11, attacker & 0xFF, target & 0xFF, Sc(), En());
    const std::uint32_t h = Hash();
    if (h % 3 == 0) SetPtr(at::kResult, EnemyObj(h >> 8) + 0x104);
    if ((h >> 2) % 2) SetPtr(at::kEnemyCurrent, EnemyObj(h >> 12));
    Disturb();
    static const short kDeltas[] = {0, 1, -1, 0x7FFF, -0x8000, 100, -100};
    const std::uint32_t k = Hash();
    return k % 3 == 0 ? static_cast<short>(k >> 16) : kDeltas[(k >> 4) % 7];
}
void __cdecl StubEffectApply() {
    Record(12, Sc(), En());
    if (Hash() % 2) SetPtr(at::kEnemyCurrent, EnemyObj(Hash() >> 8));
    Disturb();
}
unsigned __cdecl StubItemClass(unsigned category, unsigned id) {
    Record(13, category & 0xFF, id & 0xFF);
    if (Hash() % 2) SetPtr(at::kEnemyCurrent, EnemyObj(Hash() >> 8));
    Disturb();
    return Hash() >> 3;
}
void __cdecl StubDamagePopup(unsigned amount, unsigned actor) {
    Record(14, amount & 0xFFFF, actor & 0xFF);
    if (Hash() % 2) SetPtr(at::kEnemyCurrent, EnemyObj(Hash() >> 8));
    Disturb();
}
void __cdecl StubApPopup(unsigned amount, unsigned actor) {
    Record(15, amount & 0xFFFF, actor & 0xFF);
    if (Hash() % 2) SetPtr(at::kEnemyCurrent, EnemyObj(Hash() >> 8));
    Disturb();
}
// The turn check may change what the op tests next: the deltas, the HP.
unsigned long __cdecl StubTurnCheck() {
    Record(16, Sc(), En());
    const std::uint32_t h = Hash();
    if (h % 2) SetPtr(at::kEnemyCurrent, EnemyObj(h >> 8));
    if ((h >> 1) % 3 == 0) SetWord(Current() + 0xA4, (h >> 3) % 2 ? 0 : h >> 16);
    Disturb();
    return Hash();
}
void __cdecl StubPlayCue(unsigned id) { Record(17, id & 0xFFFF, Sc()); Disturb(); }
void __cdecl StubPlayEffect(unsigned short id) { Record(18, id, Sc()); Disturb(); }
void __cdecl StubHitSound() { Record(19, Sc(), En()); Disturb(); }
void __cdecl StubHitPopup() { Record(20, Sc(), En()); Disturb(); }
unsigned long __cdecl StubClearActorBit(unsigned actor) {
    Record(21, actor & 0xFF, Sc());
    const std::uint32_t h = Hash();
    if (h % 2) SetPtr(at::kEnemyCurrent, EnemyObj(h >> 8));
    if ((h >> 1) % 2) Sprite_Current = EnemyObj(h >> 12);
    Disturb();
    return Hash();
}
void __cdecl StubHook(int n) {
    Record(22, static_cast<std::uint32_t>(n), Sc(), En());
    if (Hash() % 2) SetPtr(at::kEnemyCurrent, EnemyObj(Hash() >> 8));
    Disturb();
}

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x4358D0: return f(&StubSetAnimation);
    case 0x436090: return f(&StubScriptTick);
    case 0x4360C0: return f(&StubScriptTickOnce);
    case 0x436B50: return f(&StubChance70);
    case 0x437470: return f(&StubDefeated);
    case 0x589410: return f(&StubSpriteTickOnce);
    case 0x446BB0: return f(&StubStatusTint);
    case 0x454CC0: return f(&StubSetTint);
    case 0x454D60: return f(&StubTintRelease);
    case 0x454DC0: return f(&StubReleaseTint);
    case 0x445A30: return f(&StubApplyDamage);
    case 0x44B9F0: return f(&StubEffectApply);
    case kItemClass: return f(&StubItemClass);
    case 0x453DA0: return f(&StubDamagePopup);
    case kApPopup: return f(&StubApPopup);
    case 0x44AAD0: return f(&StubTurnCheck);
    case kPlayCue: return f(&StubPlayCue);
    case 0x587740: return f(&StubPlayEffect);
    case 0x454380: return f(&StubHitSound);
    case 0x454410: return f(&StubHitPopup);
    case 0x446FD0: return f(&StubClearActorBit);
    default: bof3::Fatal("enemy_ai_ops: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

const Callees kStubs = {
    StubSetAnimation, StubScriptTick, StubScriptTickOnce, StubChance70, StubDefeated,
    StubSpriteTickOnce, StubStatusTint, StubSetTint, StubTintRelease, StubReleaseTint, StubApplyDamage,
    StubEffectApply, StubItemClass, StubDamagePopup, StubApPopup, StubTurnCheck, StubPlayCue, StubPlayEffect,
    StubHitSound, StubHitPopup, StubClearActorBit,
};

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
};

// The E8 / E9 of every call and tail jump out (capstone, 2026-09-25).
constexpr Call kBeginCalls[] = {{0x25, 0x4358D0}, {0x44, 0x446BB0}};
constexpr Call kScaleStartCalls[] = {{0xA, 0x4358D0}};
constexpr Call kScaleStepCalls[] = {{0x59, 0x436090}};
constexpr Call kIdleCalls[] = {{0x14, 0x4358D0}, {0x1C, 0x436090}};
constexpr Call kWaitCalls[] = {{0x121, 0x436090}};
constexpr Call kHighOnCalls[] = {{0x2A, 0x454CC0}};
constexpr Call kPulseCalls[] = {{0x5A, 0x454D60}};
constexpr Call kReceiveCalls[] = {
    {0x79, 0x44B9F0}, {0x8D, 0x445A30}, {0xF2, 0x4358D0}, {0x117, kItemClass}, {0x125, 0x4358D0}, {0x156, 0x4358D0},
    {0x17E, 0x453DA0}, {0x1A6, kApPopup}, {0x1C8, 0x44AAD0}, {0x203, 0x454CC0}, {0x22F, kPlayCue}, {0x253, 0x587740},
    {0x26E, 0x454380}, {0x273, 0x454410}, {0x298, 0x454380}, {0x29D, 0x454410}, {0x2AB, 0x436090}};
constexpr Call kWaitOnceCalls[] = {{0, 0x4360C0}};
constexpr Call kHitEndCalls[] = {{0, 0x589410}, {0x1A, 0x454DC0}, {0x7C, 0x436B50}, {0xAC, 0x436B50}, {0xDD, 0x446FD0}};
constexpr Call kFlashCalls[] = {{5, 0x587740}, {0x26, kPlayCue}, {0x34, 0x454DC0}, {0x51, 0x454CC0}};
constexpr Call kSquashCalls[] = {{0x2C, 0x437470}};
constexpr Call kPoseCalls[] = {{2, 0x4358D0}, {0x19, 0x436090}, {0x22, 0x4360C0}};

enum : unsigned {
    kStepDispatch, kBegin, kEnterDispatch, kScaleInDispatch, kScaleInStart, kScaleInStep, kIdle, kWait, kHighlightOn,
    kHighlightPulse, kActDispatch, kActBegin, kHitDispatch, kReceive, kWaitAnimOnce, kHitEnd, kDeathDispatch,
    kSwellStart, kSwell, kFlash, kSquash, kHitPose, kCount
};

#define EA_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
#define EA_P(name, base, size) {name, base, size, nullptr, 0}
const Clone kClones[kCount] = {
    EA_P("EnemyOp_StepDispatch", 0x4360F0, 0x12),
    EA_C("EnemyOp_Begin", 0x436110, 0x55, kBeginCalls),
    EA_P("EnemyOp_EnterDispatch", 0x436170, 0x12),
    EA_P("EnemyOp_ScaleInDispatch", 0x436190, 0x12),
    EA_C("EnemyOp_ScaleInStart", 0x4361B0, 0x59, kScaleStartCalls),
    EA_C("EnemyOp_ScaleInStep", 0x436210, 0x5E, kScaleStepCalls),
    EA_C("EnemyOp_Idle", 0x4363B0, 0x2A, kIdleCalls),
    EA_C("EnemyOp_Wait", 0x4363E0, 0x127, kWaitCalls),
    EA_C("EnemyOp_HighlightOn", 0x436510, 0x46, kHighOnCalls),
    EA_C("EnemyOp_HighlightPulse", 0x436560, 0x6C, kPulseCalls),
    EA_P("EnemyOp_ActDispatch", 0x4366E0, 0x12),
    EA_P("EnemyOp_ActBegin", 0x436700, 0x14),
    EA_P("EnemyOp_HitDispatch", 0x436720, 0x12),
    EA_C("EnemyOp_ReceiveAction", 0x436740, 0x2BC, kReceiveCalls),
    EA_C("EnemyOp_WaitAnimOnce", 0x436A00, 0x12, kWaitOnceCalls),
    EA_C("EnemyOp_HitEnd", 0x436A20, 0x12C, kHitEndCalls),
    EA_P("EnemyOp_DeathDispatch", 0x436D90, 0x12),
    EA_P("EnemyOp_DeathSwellStart", 0x436DB0, 0x44),
    EA_P("EnemyOp_DeathSwell", 0x436E00, 0x3E),
    EA_C("EnemyOp_DeathFlash", 0x436E40, 0x7D, kFlashCalls),
    EA_C("EnemyOp_DeathSquash", 0x436EC0, 0x32, kSquashCalls),
    EA_C("EnemyOp_HitPose", 0x437420, 0x28, kPoseCalls),
};
#undef EA_C
#undef EA_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kTasks, 0x22A0},        // the 48 slots (an actor's slot up to 63 runs into the enemy objects), the objects
    {0x937F88, 4},               // Sprite_Current
    {at::kEnemyCurrent, 4},
    {at::kStatBlock, 0x24},      // the 32 bytes copied in, the pointer 0x939FA0
    {at::kRoundFlags - 8, 0x260},  // the battle's globals 0x904AA0..0x904D00
    {at::kTints, 0x300},         // tint records 0..63
    {0x65C4D0, 0x1800},          // the ability records 0..255 (constant data - random here, put back after)
};
constexpr unsigned kRegionBytes = 0x22A0 + 4 + 4 + 0x24 + 0x260 + 0x300 + 0x1800;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char target[kTargets];
    unsigned char cue[kCues];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.target, g_target, sizeof g_target);
    std::memcpy(s.cue, g_cue, sizeof g_cue);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_target, s.target, sizeof g_target);
    std::memcpy(g_cue, s.cue, sizeof g_cue);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

// Random bytes put back inside what the tables and buffers hold.
void Fix() {
    Sprite_Current = EnemyObj(Next());
    SetPtr(at::kEnemyCurrent, Often() ? Sprite_Current : EnemyObj(Next()));
    for (unsigned i = 0; i < at::kEnemyCount; ++i) {
        unsigned char* const e = EnemyObj(i);
        SmallSteps(e, Next());
        e[5] = static_cast<unsigned char>(Actor(Next()));
        e[7] = static_cast<unsigned char>(Next() % 0x40);
        e[8] = static_cast<unsigned char>(Next() % 5);
        SetPtr(Address(e + 0xF4), reinterpret_cast<const void*>(&StubHook));
        SetPtr(Address(e + 0xF8), g_cue + i * 2);   // each its own offset: a cue read through the wrong enemy shows
    }
    SetPtr(at::kCommandSource, g_target + Next() % kTargets);
    SetPtr(at::kResult, EnemyObj(Next()) + 0x104);
    SetWord(At(at::kMagicId), (Half() ? (Next() % 4) << 8 : 0) | (Next() & 0xFF));
    At(at::kActionKind)[0] = static_cast<unsigned char>(Next() % 7);
    if (Half()) At(at::kEventBattle)[0] = 0;
    if (Half()) At(at::kTargeting)[0] = 0;
}

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    unsigned char* const e = Current();
    switch (k) {
    case kStepDispatch: s[1] = static_cast<unsigned char>(Often() ? Next() % 12 : Next() % kBlockEntries); break;
    case kEnterDispatch: s[2] = static_cast<unsigned char>(Often() ? Next() % 2 : Next() % 33); break;
    case kScaleInDispatch: s[3] = static_cast<unsigned char>(Often() ? Next() % 2 : Next() % 31); break;
    case kActDispatch: s[2] = static_cast<unsigned char>(Often() ? Next() % 6 : Next() % 23); break;
    case kHitDispatch: s[3] = static_cast<unsigned char>(Often() ? Next() % 3 : Next() % 17); break;
    case kDeathDispatch: s[3] = static_cast<unsigned char>(Often() ? Next() % 4 : Next() % 9); break;
    case kWait:
        s[2] = static_cast<unsigned char>(Next() % 2);
        [[fallthrough]];
    case kScaleInStep:
    case kSwell: {
        static const std::uint32_t kScale[] = {0, 0xFFFF, 0x10000, 0x10001, 0x13FFF, 0x14000, 0x14001,
                                               0x7FFFFFFF, 0x80000000u, 0xFFFFFFFFu};
        if (Often()) SetLong(s + 0x40, static_cast<std::int32_t>(kScale[Next() % 10]));
        if (k != kWait) break;
        if (Often()) e[0x92] = static_cast<unsigned char>(e[0x92] | 0x20);
        if (Half()) e[0x111] = static_cast<unsigned char>(e[0x111] & ~0x10u);
        static const unsigned char kBob[] = {0, 10, 11, 12, 13, 0xF6, 0xF5, 0xF4, 0xF3, 0x7F, 0x80};
        At(at::kTasks + s[5] * at::kTaskSize)[9] = kBob[Next() % 11];
        if (Half()) s[8] = static_cast<unsigned char>(Half() ? 1 : 3);
        break;
    }
    case kHighlightOn:
    case kHighlightPulse: {
        if (Often()) At(at::kTargeting)[0] = static_cast<unsigned char>(Next() | 1);
        unsigned char* const t = move_script::At(static_cast<std::uint32_t>(Long(At(at::kCommandSource))));
        const unsigned pick = Next() % 3;
        t[0] = static_cast<unsigned char>(pick == 0 ? s[5] : pick == 1 ? (Next() | 0x40) : Next() & ~0x40u);
        break;
    }
    case kReceive: {
        static const unsigned char kKinds[] = {0, 1, 1, 2, 3, 4, 4, 5, 5, 6};
        At(at::kActionKind)[0] = kKinds[Next() % 10];
        if (Often()) e[0x10C] = static_cast<unsigned char>(Next() % 4);
        static const std::uint16_t kDelta[] = {0, 0, 1, 0xFFFF, 0x7FFF, 0x8000, 100};
        SetWord(e + 0x108, kDelta[Next() % 7]);
        SetWord(e + 0x10A, Often() ? kDelta[Next() % 7] : Next());
        if (Half()) SetWord(e + 0xA4, 0);
        if (Often()) e[0x111] = static_cast<unsigned char>(e[0x111] & ~2u);
        if (Half()) SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & ~0x2000u);
        if (Half()) At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] | 0x80);
        if (Half()) At(at::kEventBattle)[0] = 0;
        if (Half()) SetWord(At(at::kMagicId), Next() & 0xFF);
        break;
    }
    case kHitEnd: {
        if (Often()) e[0x93] = static_cast<unsigned char>(e[0x93] & ~0x40u);
        if (Often()) At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] & ~0x40u);
        if (Often()) At(at::kTarget)[0] = static_cast<unsigned char>(At(at::kTarget)[0] & 0x3F);
        static const unsigned char kKinds[] = {1, 1, 4, 4, 4, 0, 5};
        At(at::kActionKind)[0] = kKinds[Next() % 7];
        if (Half()) SetWord(At(at::kMagicId), Next() & 0xFF);
        break;
    }
    case kSquash: {
        static const std::uint32_t kY[] = {0, 1, 0x1000, 0x2000, 0xFFFFFFFFu, 0x7FFFFFFF};
        static const std::uint32_t kV[] = {0, 0xFFFFFFFFu, 0xFFFFE000u, 0xFFFFF000u, 1, 0x80000000u};
        if (Often()) SetLong(s + 0x44, static_cast<std::int32_t>(kY[Next() % 6]));
        if (Often()) SetLong(s + 0x10, static_cast<std::int32_t>(kV[Next() % 6]));
        break;
    }
    case kHitPose:
        e[0x110] = static_cast<unsigned char>(e[0x110] & ~((Next() % 4) << 4));
        break;
    default:
        break;
    }
}

// What the rounds reached, from the original's side: per function, the
// rounds that called each stand-in (ids 1..22) and each table slot.
struct Coverage {
    unsigned stand_in[kCount][23];
    unsigned slots[kBlockEntries];
    unsigned quiet[kCount];   // rounds with no call at all
} g_cover;
void Cover(unsigned k, const State& out) {
    bool seen[256] = {};
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        const std::uint32_t w = out.log[i].what;
        if (w < 23 && !seen[w]) { seen[w] = true; ++g_cover.stand_in[k][w]; }
        if (w >= 200 && w < 200 + kBlockEntries && (k == kStepDispatch || k == kEnterDispatch || k == kScaleInDispatch ||
                                                    k == kActDispatch || k == kHitDispatch || k == kDeathDispatch || k == kWait))
            ++g_cover.slots[w - 200];
    }
    if (out.log_n == 0) ++g_cover.quiet[k];
}

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("enemy_ai_ops: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[24];
        if (c.n_calls > 24) bof3::Fatal("enemy_ai_ops: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&EnemyOp_StepDispatch), reinterpret_cast<const void*>(&EnemyOp_Begin),
        reinterpret_cast<const void*>(&EnemyOp_EnterDispatch), reinterpret_cast<const void*>(&EnemyOp_ScaleInDispatch),
        reinterpret_cast<const void*>(&EnemyOp_ScaleInStart), reinterpret_cast<const void*>(&EnemyOp_ScaleInStep),
        reinterpret_cast<const void*>(&EnemyOp_Idle), reinterpret_cast<const void*>(&EnemyOp_Wait),
        reinterpret_cast<const void*>(&EnemyOp_HighlightOn), reinterpret_cast<const void*>(&EnemyOp_HighlightPulse),
        reinterpret_cast<const void*>(&EnemyOp_ActDispatch), reinterpret_cast<const void*>(&EnemyOp_ActBegin),
        reinterpret_cast<const void*>(&EnemyOp_HitDispatch), reinterpret_cast<const void*>(&EnemyOp_ReceiveAction),
        reinterpret_cast<const void*>(&EnemyOp_WaitAnimOnce), reinterpret_cast<const void*>(&EnemyOp_HitEnd),
        reinterpret_cast<const void*>(&EnemyOp_DeathDispatch), reinterpret_cast<const void*>(&EnemyOp_DeathSwellStart),
        reinterpret_cast<const void*>(&EnemyOp_DeathSwell), reinterpret_cast<const void*>(&EnemyOp_DeathFlash),
        reinterpret_cast<const void*>(&EnemyOp_DeathSquash), reinterpret_cast<const void*>(&EnemyOp_HitPose)};

    static State saved, input, their_out, our_out;
    static constexpr auto kSlotStubs = SlotStubs(std::make_index_sequence<kBlockEntries>{});
    std::uint32_t saved_block[kBlockEntries];
    std::memcpy(saved_block, At(kBlock), sizeof saved_block);
    Capture(saved);
    g = kStubs;
    for (unsigned i = 0; i < kBlockEntries; ++i) SetPtr(kBlock + 4 * i, reinterpret_cast<const void*>(kSlotStubs[i]));

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.target) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.cue) b = static_cast<unsigned char>(Next());
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
            reinterpret_cast<Handler>(const_cast<void*>(fn))();
            Capture(out);
        }
        calls += their_out.log_n;
        Cover(k, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      enemy_ai_ops self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    std::memcpy(At(kBlock), saved_block, sizeof saved_block);
    Apply(saved);

    bof3::Log("shadow      enemy_ai_ops self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the task slots, the enemy objects, the battle's globals, the tint records, the ability "
              "records, the target and cue buffers and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      enemy_ai_ops: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned slots = 0;
    for (unsigned i = 0; i < kBlockEntries; ++i) slots += c.slots[i] ? 1u : 0u;
    bof3::Log("shadow      enemy_ai_ops coverage: op-table slots reached %u of %u", slots, kBlockEntries);
    for (unsigned k = 0; k < kCount; ++k) {
        char line[256];
        unsigned n = 0;
        for (unsigned w = 1; w < 23 && n + 16 < sizeof line; ++w)
            if (c.stand_in[k][w]) {
                const int wrote = std::snprintf(line + n, sizeof line - n, " %u:%u", w, c.stand_in[k][w]);
                if (wrote > 0) n += static_cast<unsigned>(wrote);
            }
        line[n] = 0;
        bof3::Log("shadow      enemy_ai_ops coverage: %s - no calls %u;%s", kClones[k].name, c.quiet[k], line);
    }
    if (bad) bof3::Fatal("the enemy AI script ops differ from the original in %u self-test rounds", bad);
}

}  // namespace enemy_ai_ops

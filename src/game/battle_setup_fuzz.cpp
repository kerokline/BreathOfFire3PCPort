// BOF3X_SHADOW=battle_setup: a differential fuzz of the battle set-up
// functions, once at start-up. docs/battle_setup.md section 5.
//
// Nineteen byte-copies, every call out re-aimed at a recording stand-in (none
// of them has a jump table). One round: one function, random bytes in every
// region any of them touches - the party's actor objects and the stretch of
// that array the enemies' counter is read from, the enemies' objects with the
// saved positions and the message queue below them, the battle globals at
// 0x904040.., Sprite_Current - then that function's branch boundaries seeded;
// theirs, then from the same state ours; the regions, the answer (at the
// width the original defines) and the stand-ins' log compared.
//
// The stand-ins are as loud as the real callees where the caller reads after
// the call: each one scribbles on a field some caller reads after it (the
// flags, statuses, counters, HP and AP, the agility words, the timer, the
// pending word, Sprite_Current); 0x446FB0's sets the pending bit as the real
// one does; 0x446540's writes the HP change word its callers zeroed; the
// status roll logs the counter it was given; Sprite_ReleaseTint's writes the
// status word its caller stores after it; Window_Alloc's the four window
// fields. The answers in al come with random upper bits, which the callers
// must not read (Battle_MarkFasterSide sums them, and must carry them).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_setup_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_setup {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x6C8E9CF5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }

constexpr unsigned kLog = 128;
struct Entry { std::uint32_t what, a, b, c, d, e, f; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;          // the stand-ins' own stream: the same on both passes

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0, std::uint32_t f = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e, f};
    ++g_log_n;
}

unsigned char* P(unsigned actor) { return At(at::Party(actor)); }
unsigned char* E(unsigned actor) { return At(at::Enemy(actor)); }
unsigned char* Obj(unsigned actor) { return actor <= 2 ? P(actor) : E(actor); }

// Every cell below is one some function reads again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const auto v = static_cast<unsigned char>(h >> 12);
    const unsigned w = h >> 20;
    const unsigned actor = (h >> 8) % 11;
    unsigned char* const o = Obj(actor);
    const bool party = actor <= 2;
    switch ((h >> 4) % 14) {
    case 0: o[party ? at::kPFlags : at::kEFlags] = v; break;
    case 1: o[(party ? at::kPFlags : at::kEFlags) + 1] = v; break;
    case 2: o[(party ? at::kPStatus : at::kEStatus) + (w & 1)] = v; break;
    case 3: P(actor)[at::kPCount4000 + (w & 1)] = static_cast<unsigned char>(v % 7); break;   // the party array, all 11
    case 4: o[party ? at::kPCounter : at::kECounter] = v; break;
    case 5: if (party) SetWord(o + at::kPHp + 2 * (w % 5), v * 3); else o[at::kECount4000] = static_cast<unsigned char>(v % 4); break;
    case 6: SetWord(o + (party ? at::kPAgi : at::kEAgi), v); break;
    case 7: At(at::kTimer)[0] = static_cast<unsigned char>(v % 3); break;
    case 8: SetWord(At(at::kPending), (w & 1) ? 0 : v); break;
    case 9: Sprite_Current = At(0x10000u + (h & 0xFFFF0u)); break;
    case 10: if (party) o[at::kPByteAE] = v; break;
    case 11: if (party) o[at::kPCharId] = static_cast<unsigned char>(v % 9); break;
    case 12: SetWord(o + (party ? at::kPHpChange : at::kEHpChange), w); break;
    default: o[party ? at::kPEffect : at::kEEffect] = v; break;
    }
}

// An answer in al with random upper bits: 0 one time in `odds`, or (Rarely)
// 1 one time in `odds`.
unsigned Answer(unsigned odds) { return (Hash() & ~0xFFu) | ((Hash() >> 7) % odds == 0 ? 0u : 1u); }
unsigned Rarely(unsigned odds) { return (Hash() & ~0xFFu) | ((Hash() >> 7) % odds == 0 ? 1u : 0u); }

// --- the stand-ins ---------------------------------------------------------

unsigned __cdecl StubAbsent(unsigned a) { Record(1, a & 0xFF); Disturb(); return Rarely(3); }
unsigned __cdecl StubCanAct(unsigned a) { Record(2, a & 0xFF); Disturb(); return Answer(4); }
void __cdecl StubSetPending(unsigned a) {
    Record(3, a & 0xFF);
    SetWord(At(at::kPending), Word(At(at::kPending)) | ((1u << (a & 31)) & 0xFFFF));
    Disturb();
}
void __cdecl StubMessage(unsigned a, unsigned b, unsigned c, unsigned d, const unsigned char* text) {
    Record(4, a & 0xFF, b & 0xFF, c & 0xFF, d & 0xFF, Address(text));
    Disturb();
}
void __cdecl StubMessageAt(unsigned s, unsigned a, unsigned b, unsigned c, unsigned d, const unsigned char* text) {
    Record(5, s & 0xFF, a & 0xFF, b & 0xFF, c & 0xFF, d & 0xFF, Address(text));
    Disturb();
}
void __cdecl StubStatusTint(unsigned s) {
    Record(6, s & 0xFF, Address(Sprite_Current));
    Sprite_Current = At(0x20000u + (Hash() & 0xFFF0u));
    Disturb();
}
void __cdecl StubPartyName(unsigned a) { Record(7, a & 0xFF); Disturb(); }
void __cdecl StubEnemyName(unsigned a) { Record(8, a & 0xFF); Disturb(); }
// The roll reads the actor's counter: log what it was given, then move it.
unsigned __cdecl StubWakeRoll(unsigned a) {
    const unsigned i = a & 0xFF;
    unsigned char* const counter = Obj(i) + (i <= 2 ? at::kPCounter : at::kECounter);
    Record(9, i, *counter);
    if (Hash() % 3 == 0) *counter = static_cast<unsigned char>(Hash() >> 9);
    Disturb();
    return Answer(2);
}
// Sets the HP change word the caller zeroed before it.
void __cdecl StubSetHpChange(unsigned a) {
    const unsigned i = a & 0xFF;
    unsigned char* const o = Obj(i);
    Record(10, i, Word(o + (i <= 2 ? at::kPHpChange : at::kEHpChange)), Word(o + (i <= 2 ? at::kPApChange : at::kEApChange)));
    SetWord(o + (i <= 2 ? at::kPHpChange : at::kEHpChange), Hash() >> 3);
    Disturb();
}
unsigned __cdecl StubEnemyOutpaces(unsigned a, unsigned average, unsigned highest) {
    Record(11, a & 0xFF, average & 0xFFFF, highest & 0xFFFF);
    Disturb();
    return Answer(2);
}
unsigned __cdecl StubReturnItem(unsigned slot, unsigned item) {
    Record(12, slot & 0xFF, item & 0xFFFF);
    Disturb();
    return Hash();
}
// Writes the status word its caller stores after it, and moves Sprite_Current,
// which the caller reads after it.
void __cdecl StubReleaseTint(unsigned char* object) {
    Record(13, Address(object));
    const std::uint32_t a = Address(object);
    if (a >= at::kParty && a < at::kParty + 3 * at::kPartyStride) SetWord(object + at::kPStatus, Hash());
    else SetWord(object + at::kEStatus, Hash());
    Sprite_Current = At(0x30000u + (Hash() & 0xFFF0u));
    Disturb();
}
// Msg_SystemPtr reads its id as a u16 (symbols.toml): Battle_OpeningMessage
// pushes it in ecx or edx over a 16-bit movzx, the upper half Rand's leftovers.
const unsigned char* __cdecl StubMsg(unsigned id) {
    Record(14, id & 0xFFFF);
    Disturb();
    return At(0x40000u + (Hash() & 0xFFFF0u));
}
unsigned __cdecl StubWindowAlloc(unsigned slot, unsigned kind) {
    Record(15, slot, kind);
    std::uint32_t h = Hash();
    for (unsigned i = 0; i < 6; ++i, h >>= 5) At(at::kMsgWindow)[i] = static_cast<unsigned char>(h);
    Disturb();
    return Hash();
}
int __cdecl StubRand() {
    Record(16);
    Disturb();
    return static_cast<int>(Hash() >> 1);
}
unsigned __cdecl StubSkipped(unsigned a) { Record(17, a & 0xFF); Disturb(); return Rarely(4); }
void __cdecl StubOpenWindow() { Record(18); Disturb(); }
unsigned __cdecl StubClearStatus(unsigned a, unsigned mask) {
    Record(19, a & 0xFF, mask & 0xFFFF);
    Disturb();
    return Hash();
}
unsigned __cdecl StubStanding(unsigned a) { Record(20, a & 0xFF); Disturb(); return Answer(3); }
unsigned __cdecl StubOutpaces(unsigned a, unsigned average, unsigned highest) {
    Record(21, a & 0xFF, average & 0xFFFF, highest & 0xFFFF);
    Disturb();
    return Answer(2);
}

const Callees kStubs = {
    StubAbsent,     StubCanAct,        StubSetPending, StubMessage, StubMessageAt,   StubStatusTint, StubPartyName,
    StubEnemyName,  StubWakeRoll,      StubSetHpChange, StubEnemyOutpaces, StubReturnItem, StubReleaseTint, StubMsg,
    StubWindowAlloc, StubRand,         StubSkipped,    StubOpenWindow, StubClearStatus, StubStanding,   StubOutpaces,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case kActorAbsent: return f(&StubAbsent);
    case kActorCanAct: return f(&StubCanAct);
    case kSetPending: return f(&StubSetPending);
    case kMessage: return f(&StubMessage);
    case kMessageAt: return f(&StubMessageAt);
    case kStatusTint: return f(&StubStatusTint);
    case kPartyName: return f(&StubPartyName);
    case kEnemyName: return f(&StubEnemyName);
    case kWakeRoll: return f(&StubWakeRoll);
    case kSetHpChange: return f(&StubSetHpChange);
    case kEnemyOutpaces: return f(&StubEnemyOutpaces);
    case kReturnItem: return f(&StubReturnItem);
    case 0x454DC0: return f(&StubReleaseTint);    // Sprite_ReleaseTint
    case 0x497740: return f(&StubMsg);            // Msg_SystemPtr
    case 0x59E2D0: return f(&StubWindowAlloc);    // Window_Alloc
    case 0x5B93D2: return f(&StubRand);           // Rand
    case 0x431030: return f(&StubSkipped);        // Battle_ActorSkipped
    case 0x444310: return f(&StubOpenWindow);     // Battle_OpenMsgWindow
    case 0x44F4B0: return f(&StubClearStatus);    // Battle_ClearStatus
    case 0x445550: return f(&StubStanding);       // Battle_ActorStanding
    case 0x4455C0: return f(&StubOutpaces);       // Battle_PartyOutpaces
    default: bof3::Fatal("battle_setup: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the nineteen copies (capstone, recursive descent, 2026-09-23: every jump
// internal, no jump table; the calls below are every call that leaves) ------

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    unsigned ret;               // the answer's width the original defines: 0 none, 1 al, 2 ax, 4 eax
    const void* ours;
};

constexpr Call kTick[] = {{0x19, 0x431030}, {0x7A, 0x431030}, {0xC8, 0x4456C0}, {0xFB, 0x4456C0}};
constexpr Call kExpire[] = {{0x1B, 0x431030}, {0x75, 0x446FB0}, {0xA5, 0x431030}, {0xF9, 0x446FB0},
                            {0x121, 0x444310}, {0x128, 0x497740}, {0x134, 0x44A650}};
constexpr Call kRestore[] = {{0x20, 0x431030}, {0x79, 0x44F4B0}, {0x9A, 0x444310}, {0xA9, 0x44A910},
                             {0xB0, 0x497740}, {0xBE, 0x44A650}, {0xD1, 0x497740}, {0xDF, 0x44A650}};
constexpr Call kPartyWake[] = {{0x20, 0x431030}, {0x36, 0x446CB0}, {0x79, 0x444310}, {0x88, 0x44A910},
                               {0x8F, 0x497740}, {0x9D, 0x44A650}, {0xB0, 0x497740}, {0xBE, 0x44A650}};
constexpr Call kEnemyWake[] = {{0x20, 0x431030}, {0x33, 0x446CB0}, {0x73, 0x444310}, {0x82, 0x44A960},
                               {0x89, 0x497740}, {0x97, 0x44A650}, {0xAA, 0x497740}, {0xB8, 0x44A650}};
constexpr Call kStatus80[] = {{0x1A, 0x431030}, {0x37, 0x446540}, {0x65, 0x446FB0}, {0x90, 0x431030}, {0xAA, 0x446540},
                              {0xD8, 0x446FB0}, {0xFA, 0x444310}, {0x101, 0x497740}, {0x10D, 0x44A650}};
constexpr Call kHpDrift[] = {{0x14, 0x431030}, {0x160, 0x446FB0}, {0x18D, 0x431030}, {0x1CC, 0x446FB0}};
constexpr Call kApUpkeep[] = {{0x14, 0x431030}, {0x69, 0x446FB0}, {0xB2, 0x446FB0}};
constexpr Call kSkipped[] = {{0x51, 0x4456C0}};
constexpr Call kFaster[] = {{0x27, 0x445550}, {0x88, 0x445980}, {0x9B, 0x4455C0},
                            {0xF1, 0x445550}, {0x14D, 0x445980}, {0x160, 0x445600}};
constexpr Call kWindow[] = {{0x4, 0x59E2D0}};
constexpr Call kReturn[] = {{0x6B, 0x446D90}};
constexpr Call kOpening[] = {{0x32, 0x5B93D2}, {0x43, 0x5B93D2}, {0x54, 0x5B93D2}, {0x63, 0x497740}, {0x76, 0x44A6E0}};
constexpr Call kClear[] = {{0x146, 0x454DC0}, {0x166, 0x454DC0}, {0x17F, 0x446BB0}};

#define BA_C(name, base, size, calls, ret) \
    {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret, reinterpret_cast<const void*>(&::name)}
#define BA_P(name, base, size, ret) {#name, base, size, nullptr, 0, ret, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    BA_P(Battle_ClearActingFlags, 0x4301B0, 0xF6, 0),
    BA_C(Battle_TickCounters, 0x4303D0, 0x123, kTick, 0),
    BA_C(BattleStep_Expire4000, 0x430640, 0x150, kExpire, 1),
    BA_C(BattleStep_Restore800, 0x430790, 0xF9, kRestore, 1),
    BA_C(BattleStep_PartyWake40, 0x430890, 0xD8, kPartyWake, 1),
    BA_C(BattleStep_EnemyWake40, 0x430970, 0xD2, kEnemyWake, 1),
    BA_C(BattleStep_PartyWake20, 0x430A50, 0xD8, kPartyWake, 1),
    BA_C(BattleStep_EnemyWake20, 0x430B30, 0xD2, kEnemyWake, 1),
    BA_C(BattleStep_Status80, 0x430C10, 0x127, kStatus80, 1),
    BA_C(BattleStep_HpDrift, 0x430D40, 0x1F5, kHpDrift, 1),
    BA_C(BattleStep_ApUpkeep, 0x430F40, 0xEB, kApUpkeep, 1),
    BA_C(Battle_ActorSkipped, 0x431030, 0x5F, kSkipped, 1),
    BA_C(Battle_MarkFasterSide, 0x4453C0, 0x190, kFaster, 1),
    BA_P(Battle_ActorStanding, 0x445550, 0x6A, 4),
    BA_P(Battle_PartyOutpaces, 0x4455C0, 0x3F, 4),
    BA_C(Battle_OpenMsgWindow, 0x444310, 0x2D, kWindow, 0),
    BA_C(Battle_ReturnQueuedItem, 0x446EA0, 0x75, kReturn, 0),
    BA_C(Battle_OpeningMessage, 0x44AA00, 0x82, kOpening, 0),
    BA_C(Battle_ClearStatus, 0x44F4B0, 0x194, kClear, 2),
};
#undef BA_C
#undef BA_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kActingK, kTickK, kExpireK, kRestoreK, kPWake40K, kEWake40K, kPWake20K, kEWake20K, kStatus80K, kHpDriftK, kApK,
    kSkippedK, kFasterK, kStandingK, kOutpacesK, kWindowK, kReturnK, kOpeningK, kClearK,
};
static_assert(kClearK + 1 == kCount, "the index enum follows kClones");

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
constexpr Region kRegions[] = {
    {0x802D40, 0xEC0},    // the party's objects, and the array on to actor 10 (BattleStep_Expire4000's enemy
                          // counters) with the window fields 0x803162
    {0x93A000, 0x22C0},   // the saved positions 0x93A034, the message queue 0x93B8E0, the enemies' objects
    {0x904040, 0xBC0},    // the battle globals 0x904060 .. 0x904B90, the item queue 0x904ACC
    {0x937F88, 4},        // Sprite_Current
};
constexpr unsigned kRegionBytes = 0xEC0 + 0x22C0 + 0xBC0 + 4;

struct State {
    unsigned char memory[kRegionBytes];
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned off = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + off, At(r.at), r.size); off += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned off = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + off, r.size); off += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

struct Args { std::uint32_t a[6]; };

std::uint32_t Byte(std::uint32_t low) { return Half() ? low : Garbage(0xFF, low & 0xFF); }
unsigned Pick(const unsigned* values, unsigned n) { return values[Next() % n]; }

// The fields the steps test, seeded towards their boundaries for every actor
// (the party array for all eleven: BattleStep_Expire4000 reads it for the
// enemies).
void SeedActors() {
    static const unsigned kSmall[] = {0, 1, 2, 3, 4, 5, 6, 0xFF};
    for (unsigned i = 0; i <= 10; ++i) {
        unsigned char* const p = P(i);
        if (Often()) p[at::kPCount4000] = static_cast<unsigned char>(Pick(kSmall, 8));
        if (Often()) p[at::kPCount800] = static_cast<unsigned char>(Pick(kSmall, 8));
        if (i > 2) {
            unsigned char* const e = E(i);
            if (Often()) e[at::kECount4000] = static_cast<unsigned char>(Pick(kSmall, 8));
            if (Half()) e[at::kECounter] = static_cast<unsigned char>(Pick(kSmall, 8));
            if (Half()) e[at::kPresent] |= 1;
        } else {
            if (Half()) p[at::kPCounter] = static_cast<unsigned char>(Pick(kSmall, 8));
            if (Half()) p[at::kPresent] |= 1;
        }
        unsigned char* const o = Obj(i);
        // the flags: bits 1, 2, 0x10, 0x20, 0x40, 0x80, 0x4000, 0x8000 each at even odds already; clear
        // them all at times so that a lone bit is met
        if (Next() % 4 == 0) SetLong(o + (i <= 2 ? at::kPFlags : at::kEFlags), static_cast<std::int32_t>(Next() & Next()));
        if (Next() % 4 == 0) SetWord(o + (i <= 2 ? at::kPStatus : at::kEStatus), Next() & Next() & Next());
    }
    if (Half()) At(at::kTimer)[0] = static_cast<unsigned char>(Next() % 3);
    if (Half()) SetWord(At(at::kPending), 0);
}

Args Seed(unsigned k) {
    Args x;
    for (auto& v : x.a) v = Next();
    Sprite_Current = At(0x50000u + (Next() & 0xFFF0u));
    SeedActors();
    switch (k) {
    case kActingK: {
        const unsigned actor = Next() % 11;
        std::uint32_t acting = Garbage(0xFFFF, actor | (Often() ? 4u << 8 : (Next() & 0xFF) << 8));
        SetLong(At(at::kActing), static_cast<std::int32_t>(acting));
        static const unsigned kAbilities[] = {0x27, 0xA3, 0x26, 0x28, 0xA2, 0x127, 0x10A3};
        if (Often()) SetWord(At(at::kAbility), Pick(kAbilities, 7));
        break;
    }
    case kHpDriftK:
        if (Half()) At(at::kHpDriftMode)[0] = 5;
        if (Half()) At(at::kField12)[0] = 0x12;
        for (unsigned i = 0; i <= 2; ++i) {
            unsigned char* const o = P(i);
            static const unsigned kIds[] = {0, 6, 7, 1, 5, 8};
            static const unsigned kMaxHp[] = {0, 1, 9, 10, 11, 19, 20, 29, 30, 999, 0x7FFF, 0xFFF5, 0xFFFF};
            if (Often()) o[at::kPCharId] = static_cast<unsigned char>(Pick(kIds, 6));
            if (Half()) o[at::kPWeapon] = 0x52;
            if (Half()) o[at::kPArmour3] = 0x1F;
            static const unsigned kAcc[] = {0x16, 0x17, 0x15, 0x18};
            if (Half()) o[at::kPAccessory] = static_cast<unsigned char>(Pick(kAcc, 4));
            if (Half()) o[at::kPAccessory + 1] = static_cast<unsigned char>(Pick(kAcc, 4));
            if (Often()) SetWord(o + at::kPMaxHp, Pick(kMaxHp, 13));
            if (Half()) o[at::kPStatus] = static_cast<unsigned char>(Next() & 0xFE);
            if (Half()) o[at::kPFlags] = static_cast<unsigned char>(Half() ? 2 : 0);
            if (Half()) {
                // HP next to what the weapon's term reaches: (max HP + 10) / 20 and the terms before it
                const unsigned n = (Word(o + at::kPMaxHp) + 10) / 20;
                SetWord(o + at::kPHp, n + (Next() % 5) - 2);
            } else if (Half()) {
                SetWord(o + at::kPHp, Next() % 3);
            }
        }
        for (unsigned i = 3; i <= 10; ++i)
            if (Half()) E(i)[at::kEStatus] = static_cast<unsigned char>((Next() & 0xFE) | (Half() ? 1 : 0));
        break;
    case kApK:
        if (Often()) {
            static const unsigned kCost[] = {0, 1, 2, 3, 0x7F, 0xFE, 0xFF};
            SetLong(At(at::kApCost), static_cast<std::int32_t>(Garbage(0xFF, Pick(kCost, 7))));
        }
        for (unsigned i = 0; i <= 2; ++i) {
            unsigned char* const o = P(i);
            if (Often()) o[at::kPFlags] = static_cast<unsigned char>((Next() & ~0x22u) | (Often() ? 2 : 0) | (Half() ? 0 : 0x20));
            const unsigned cost = ((Long(At(at::kApCost)) & 0xFF) + 1) / 2;
            if (Often()) SetWord(o + at::kPAp, cost + (Next() % 3) - 1);
        }
        break;
    case kRestoreK:
        for (unsigned i = 0; i <= 2; ++i) {
            unsigned char* const o = P(i);
            if (Often()) o[at::kPStatus + 1] = static_cast<unsigned char>(Next() | 8);
            if (Often()) o[at::kPCount800] = static_cast<unsigned char>(Half() ? 5 : 4 + Next() % 3);
        }
        break;
    case kExpireK:
        for (unsigned i = 0; i <= 10; ++i) {
            unsigned char* const o = Obj(i);
            if (Often()) o[(i <= 2 ? at::kPFlags : at::kEFlags) + 1] = static_cast<unsigned char>(Next() | 0x40);
            if (Often()) P(i)[at::kPCount4000] = static_cast<unsigned char>(Half() ? 3 : 2 + Next() % 3);
            if (i > 2 && Half()) E(i)[at::kECount4000] = static_cast<unsigned char>(Half() ? 3 : Next());
        }
        break;
    case kPWake40K:
    case kEWake40K:
    case kPWake20K:
    case kEWake20K:
    case kStatus80K: {
        const unsigned bit = k == kStatus80K ? 0x80 : (k == kPWake40K || k == kEWake40K) ? 0x40 : 0x20;
        for (unsigned i = 0; i <= 10; ++i) {
            unsigned char* const o = Obj(i);
            unsigned char* const s = o + (i <= 2 ? at::kPStatus : at::kEStatus);
            if (Often()) *s = static_cast<unsigned char>(Half() ? (*s | bit) : (*s & ~bit));
        }
        break;
    }
    case kSkippedK: {
        const unsigned actor = Next() % 11;
        x.a[0] = Byte(actor);
        if (Half()) At(at::kTimer)[0] = static_cast<unsigned char>(Half() ? 0 : 1 + Next() % 3);
        if (Half()) Obj(actor)[actor <= 2 ? at::kPFlags : at::kEFlags] ^= 0x10;
        break;
    }
    case kFasterK: {
        // agility words next to each other so that the average's double and the highest meet them
        const unsigned base = Next() % 0x60;
        for (unsigned i = 0; i <= 10; ++i)
            if (Often()) SetWord(Obj(i) + (i <= 2 ? at::kPAgi : at::kEAgi), Half() ? base + Next() % 8 : 2 * base + Next() % 5 - 2);
        if (Next() % 8 == 0)
            for (unsigned i = 0; i <= 10; ++i) SetWord(Obj(i) + (i <= 2 ? at::kPAgi : at::kEAgi), Half() ? 0 : 0x8000);
        break;
    }
    case kStandingK: {
        const unsigned actor = Next() % 11;
        x.a[0] = Byte(actor);
        unsigned char* const o = Obj(actor);
        if (Half()) o[at::kPresent] = static_cast<unsigned char>(Next() | 1);
        // each bit of either mask alone, and the one bit that tells them apart (0x800) more often
        static const unsigned kBits[] = {0x4944, 0x4144, 0x0800, 0x0800, 0x0100, 0x4000, 0x0004,
                                         0x0040, 0x0040, 0x0200, 0x1000, 0};
        if (Often()) SetWord(o + (actor <= 2 ? at::kPStatus : at::kEStatus), Pick(kBits, 12) & (Half() ? 0xFFFF : Next()));
        break;
    }
    case kOutpacesK: {
        const unsigned actor = Next() % 3;
        x.a[0] = Byte(actor);
        const unsigned agility = Half() ? 2 * (Next() % 0x100) : Next() % 0x200;   // even: twice the average meets it
        SetWord(P(actor) + at::kPAgi, Half() ? agility : Next());
        const unsigned half = agility / 2;
        x.a[1] = Garbage(0xFFFF, Half() ? half + Next() % 3 - 1 : Next() & 0xFFFF);
        x.a[2] = Garbage(0xFFFF, Half() ? agility + Next() % 3 - 1 : Next() & 0xFFFF);
        if (Next() % 8 == 0) x.a[1] = Garbage(0xFFFF, 0x8000 + Next() % 2);
        break;
    }
    case kReturnK: {
        const unsigned actor = Often() ? Next() % 3 : Half() ? 3 : Next() % 11;   // 3: the first past the members
        x.a[0] = Byte(actor);
        static const unsigned kFrom[] = {0, 1, 1, 2, 3, 8, 0x10, 0xFF};
        static const unsigned kTo[] = {0, 1, 2, 3, 4, 8, 9, 0x10, 0xFF};
        if (Often()) At(at::kQueueFrom)[0] = static_cast<unsigned char>(Pick(kFrom, 8));
        if (Often()) At(at::kQueueTo)[0] = static_cast<unsigned char>(Pick(kTo, 9));
        const unsigned from = static_cast<unsigned char>(At(at::kQueueFrom)[0] - 1), to = At(at::kQueueTo)[0];
        if (from < to && Often()) {
            // in the range: at its first, its last, or anywhere in it
            const unsigned r = Next() % 3;
            const unsigned slot = r == 0 ? from : r == 1 ? to - 1 : from + Next() % (to - from);
            At(at::kItemQueue + slot)[0] = static_cast<unsigned char>(actor);
        }
        if (Half()) At(at::kItemQueue + (to < 0xFF ? to : 0))[0] = static_cast<unsigned char>(actor);   // just past the end
        if (Half() && from > 0 && from < 0xFF) At(at::kItemQueue + from - 1)[0] = static_cast<unsigned char>(actor);   // before
        {
            // an enemy's too, in the party array past the members: a function that took it for a
            // member would read these
            unsigned char* const o = P(actor);
            if (Often()) o[at::kPQueuedKind] = static_cast<unsigned char>(Often() ? 5 : 4 + Next() % 3);
            if (Often()) o[at::kPQueuedItem + 1] = static_cast<unsigned char>(Often() ? 0 : Next() % 5);
        }
        break;
    }
    case kOpeningK: {
        static const unsigned kKinds[] = {0, 1, 2, 3, 4, 5, 0xFF, 0x100, 0xFFFFFFFFu, 0x80000001u};
        SetLong(At(at::kOpening), static_cast<std::int32_t>(Often() ? Pick(kKinds, 10) : Next()));
        break;
    }
    case kClearK: {
        const unsigned actor = Next() % 11;
        x.a[0] = Byte(actor);
        x.a[1] = Next();
        if (Half()) x.a[1] |= 0x20;
        if (Next() % 4 == 0) x.a[1] = 1u << (Next() % 16);
        unsigned char* const s = Obj(actor) + (actor <= 2 ? at::kPStatus : at::kEStatus);
        if (Half()) *s |= 0x20;
        break;
    }
    default: break;
    }
    return x;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[24];
    unsigned answers[kCount][2];   // al 0, al not 0
    unsigned single_lines, many_lines, restore_copies, queue_found, drift_marks;
} g_cover;

void Cover(unsigned k, const State& out, std::uint32_t result) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        const Entry& e = out.log[i];
        if (e.what < 24) ++g_cover.logged[e.what];
        if (e.what == 14 && (e.a == 0x25 || e.a == 0x28 || e.a == 0x2B)) ++g_cover.single_lines;
        if (e.what == 14 && (e.a == 0x26 || e.a == 0x27 || e.a == 0x29 || e.a == 0x2A || e.a == 0x2C)) ++g_cover.many_lines;
        if (k == kReturnK && e.what == 12) ++g_cover.queue_found;
        if (k == kHpDriftK && e.what == 3) ++g_cover.drift_marks;
    }
    ++g_cover.answers[k][(result & 0xFF) != 0 ? 1 : 0];
}

using Fn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_setup: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("battle_setup: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    static State saved, input, their_out, our_out;
    Capture(saved);
    g = kStubs;

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        g_seed = Next();
        const Args x = Seed(k);
        Capture(input);

        std::uint32_t result[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            const std::uint32_t r = reinterpret_cast<Fn>(const_cast<void*>(fn))(x.a[0], x.a[1], x.a[2], x.a[3], x.a[4], x.a[5]);
            Capture(out);
            const unsigned w = kClones[k].ret;
            result[pass] = w == 0 ? 0u : w == 1 ? (r & 0xFFu) : w == 2 ? (r & 0xFFFFu) : r;
            out.result = result[pass];
        }
        calls += their_out.log_n;
        Cover(k, their_out, result[0]);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_setup self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    Apply(saved);

    bof3::Log("shadow      battle_setup self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the party and enemy actor objects, the saved positions and message queue, the battle "
              "globals, Sprite_Current, the answer and the stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      battle_setup: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      battle_setup coverage: calls skipped %u, absent %u, pending %u, roll %u, HP change %u, "
              "clear status %u, standing %u, outpaces %u / %u, can act %u, window %u, lines %u (one actor %u, more %u), "
              "names %u / %u, tints %u / %u, item back %u, rand %u; answers 1: expire %u, restore %u, wakes %u %u %u %u, "
              "status80 %u, drift %u, upkeep %u, faster %u, standing %u, outpaces %u; drift marks %u",
              c.logged[17], c.logged[1], c.logged[3], c.logged[9], c.logged[10], c.logged[19], c.logged[20], c.logged[21],
              c.logged[11], c.logged[2], c.logged[18], c.logged[4] + c.logged[5], c.single_lines, c.many_lines, c.logged[7],
              c.logged[8], c.logged[13], c.logged[6], c.queue_found, c.logged[16], c.answers[kExpireK][1],
              c.answers[kRestoreK][1], c.answers[kPWake40K][1], c.answers[kEWake40K][1], c.answers[kPWake20K][1],
              c.answers[kEWake20K][1], c.answers[kStatus80K][1], c.answers[kHpDriftK][1], c.answers[kApK][1],
              c.answers[kFasterK][1], c.answers[kStandingK][1], c.answers[kOutpacesK][1], c.drift_marks);
    if (bad) bof3::Fatal("the battle set-up functions differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_setup

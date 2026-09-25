// BOF3X_SHADOW=battle_obj_states: a differential fuzz of the battle party
// objects' state handlers, once at start-up. docs/battle_obj_states.md
// section 4.
//
// Twenty-two byte-copies, every call and tail jump out re-aimed at a
// recording stand-in (bof3::CloneCall with `expected`); the seven table
// operands (`call / jmp [reg*4 + table]`, `jmp [0x64E048]`) re-aimed inside
// the copies at tables of recorders of our own, which g points ours at too.
// One round: one function, random bytes in every region any of them touches,
// the pointers and indices put back inside what they may reach, each branch's
// boundaries seeded; theirs, then from the same state ours; the regions, the
// target byte and the stand-ins' log compared. Everything is put back
// afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_obj_states_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_obj_states {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Member(unsigned i) { return At(at::kMembers + i * at::kMemberSize); }
unsigned char* Slot(unsigned i) { return At(at::kTasks + i * at::kTaskSize); }
unsigned char* S() { return Sprite_Current; }
unsigned char* F() { return Field_State; }

constexpr unsigned kTaskSlots = 48;   // Sprite_Current +5 kept inside them
constexpr unsigned kTintSlots = 64;   // +7 kept inside these records of MoveScript_TintRecords
constexpr unsigned kSkills = 256;     // 0x904B80 kept inside these records

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 64;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;
unsigned char g_target[16];   // what 0x939FA0 points into

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
// A pointer as the log keeps it: a member by number, else as it is.
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p);
    if (at >= at::kMembers && at < at::kMembers + 3 * at::kMemberSize) return 0x10000u + (at - at::kMembers);
    return at;
}
// What the callee sees of the object it runs on.
std::uint32_t Who() { return Id(S()) << 16 ^ Id(F()); }
std::uint32_t Obj() { return static_cast<std::uint32_t>(S()[1]) | S()[2] << 8 | S()[9] << 16 | S()[8] << 24; }

// Every byte below is one some handler reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the members, indices inside what they may
// reach.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 20) {
    case 0: Sprite_Current = Member(v % 3); break;
    case 1: Field_State = Member(v % 3); break;
    case 2: S()[1] = static_cast<unsigned char>(v); break;
    case 3: S()[2] = static_cast<unsigned char>(v); break;
    case 4: S()[9] = static_cast<unsigned char>(v % 3); break;
    case 5:
        if (w % 2) S()[5] = static_cast<unsigned char>(v % kTaskSlots);
        else S()[7] = static_cast<unsigned char>(v % kTintSlots);
        break;
    case 6: S()[8] = static_cast<unsigned char>(v % 4); break;
    case 7: F()[0x89] = static_cast<unsigned char>(v % 14); break;
    case 8: F()[0x125] = static_cast<unsigned char>(v % 2 ? 4 : v % 6); break;
    case 9: F()[0x134] = static_cast<unsigned char>(F()[0x134] ^ (1u << (v % 3))); break;
    case 10: SetLong(F() + 0x130, Long(F() + 0x130) ^ static_cast<std::int32_t>(1u << (9 + v % 4))); break;
    case 11: {
        static const unsigned char kBits[] = {4, 0x40, 0x80};
        if (w % 3 == 0) At(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(At(at::kRoundFlags + 1)[0] ^ (w % 2 ? 0x08 : 0x80));
        else At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] ^ kBits[v % 3]);
        break;
    }
    case 12: {
        static const unsigned char kPhases[] = {0, 1, 5};
        At(at::kPhase)[0] = kPhases[v % 3];
        break;
    }
    case 13: At(at::kTintOn)[0] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 14: g_target[w % sizeof g_target] = static_cast<unsigned char>(v % 3 == 0 ? S()[5] : v % 3 == 1 ? (v | 0x80) : v & 0x7F); break;
    case 15: {
        static const unsigned char kKinds[] = {4, 7, 8};
        At(at::kCastKind)[0] = v % 2 ? kKinds[w % 3] : static_cast<unsigned char>(v % 28);
        break;
    }
    case 16: SetWord(At(at::kSkillId), v % kSkills); break;
    case 17: At(at::kSoundSet + (w % 3 == 0 ? 0 : w % 3 == 1 ? at::kSwingTarget - at::kSoundSet : at::kSkillCost - at::kSoundSet))[0] =
                 static_cast<unsigned char>(v);
        break;
    case 18: Slot(S()[5] % kTaskSlots)[9] = static_cast<unsigned char>(static_cast<int>(v % 5) - 2 + (w % 2 ? 11 : -11)); break;
    default:
        if (w % 2) F()[0xBA] = static_cast<unsigned char>(v % 101);
        else SetWord(F() + 0x9A, h >> 16);
        break;
    }
}

// --- the stand-ins ---------------------------------------------------------

// A byte answer that is 0 a third of the time.
unsigned char Answer(unsigned shift) {
    const std::uint32_t h = Hash() >> shift;
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : (h >> 2) | 1);
}

template <unsigned N> void __cdecl StubHandler() { Record(100 + N, Who(), Obj()); Disturb(); }

unsigned __cdecl StubPickPose() { Record(1, Who(), Obj()); Disturb(); return Hash(); }
unsigned char __cdecl StubTick() { Record(2, Who(), Obj()); Disturb(); return Answer(3); }
unsigned char __cdecl StubTickOnce() { Record(3, Who(), Obj()); Disturb(); return Answer(5); }
long __cdecl StubElevation(long x, long y) {
    Record(4, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), Who());
    Disturb();
    return static_cast<long>(Hash());   // the whole dword: the caller keeps 16 bits
}
unsigned char __cdecl StubEnsure(unsigned char a) { Record(5, a, Who(), Obj()); Disturb(); return static_cast<unsigned char>(Hash()); }
unsigned char __cdecl StubSetTint(unsigned char* p, unsigned char r, unsigned char g_, unsigned char b, unsigned char a) {
    Record(6, Id(p), static_cast<std::uint32_t>(r) | g_ << 8 | b << 16 | static_cast<std::uint32_t>(a) << 24, Who());
    Disturb();
    return static_cast<unsigned char>(Hash() >> 3);
}
void __cdecl StubTintRelease(unsigned char i) { Record(7, i, Who()); Disturb(); }
unsigned char __cdecl StubRollPending() { Record(8, Who()); Disturb(); return Answer(7); }
// Values whose remainder lands on Field_State +0xBA or one either side, most
// of the time; negatives now and then (the original's idiv is signed).
int __cdecl StubRand() {
    Record(9, Who());
    const unsigned edge = F()[0xBA];
    Disturb();
    const std::uint32_t h = Hash();
    const int base = static_cast<int>((h >> 8) % 300) * 100;
    switch (h % 6) {
    case 0: return static_cast<int>(h >> 17);
    case 1: return -static_cast<int>(edge) - base;
    case 2: return base + static_cast<int>(edge) - 1;
    case 3: return base + static_cast<int>(edge) + 1;
    default: return base + static_cast<int>(edge);
    }
}
unsigned long __cdecl StubCue(unsigned cue) { Record(10, cue & 0xFF, Who()); Disturb(); return Hash(); }
unsigned char __cdecl StubTaskCreate(unsigned kind, unsigned parameter) {
    Record(11, kind & 0xFF, parameter & 0xFF, Who());
    Disturb();
    return static_cast<unsigned char>(Hash());
}
void __cdecl StubSetTarget(unsigned t) { Record(12, t & 0xFF, Who()); Disturb(); }
int __cdecl StubLoadDone() { Record(13, Who()); Disturb(); const std::uint32_t h = Hash(); return h % 3 == 0 ? 0 : static_cast<int>(h | 0x100); }
unsigned char __cdecl StubLoadSound(unsigned key, unsigned set) {
    Record(14, key & 0xFF, set & 0xFF, Who());
    Disturb();
    return Answer(9);
}
unsigned long __cdecl StubClearBit(unsigned actor) { Record(15, actor & 0xFF, Who()); Disturb(); return Hash(); }
void __cdecl StubEndAction() { Record(16, Who(), Obj()); Disturb(); }

// --- the tables of recorders -------------------------------------------------

constexpr unsigned kStubs = 64;
template <unsigned... I> struct Stubs {
    static constexpr Handler f[sizeof...(I)] = {&StubHandler<I>...};
};
template <unsigned N, unsigned... I> struct MakeStubs : MakeStubs<N - 1, N - 1, I...> {};
template <unsigned... I> struct MakeStubs<0, I...> { using type = Stubs<I...>; };
using AllStubs = MakeStubs<kStubs>::type;

// One 256-entry table a role (a byte indexes each), every role's entries a
// different rotation of the recorders, and one cell for 0x64E048.
enum : unsigned { kIdle, kByChar, kSpecial, kSwing, kCast, kCastDone, kTwelve, kRoles };
std::uint32_t g_tables[kRoles][256];

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x4412B0: return f(&StubPickPose);
    case 0x441180: return f(&StubTick);
    case 0x4411B0: return f(&StubTickOnce);
    case 0x5720C0: return f(&StubElevation);
    case 0x589330: return f(&StubEnsure);
    case 0x454CC0: return f(&StubSetTint);
    case 0x454D60: return f(&StubTintRelease);
    case 0x452BF0: return f(&StubRollPending);
    case 0x5B93D2: return f(&StubRand);
    case 0x446A50: return f(&StubCue);
    case 0x435180: return f(&StubTaskCreate);
    case 0x4530D0: return f(&StubSetTarget);
    case 0x454810: return f(&StubLoadDone);
    case 0x446E40: return f(&StubLoadSound);
    case 0x446FD0: return f(&StubClearBit);
    case 0x442DD0: return f(&StubEndAction);
    default: bof3::Fatal("battle_obj_states: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

Callees Stubbed() {
    Callees s = {
        g_tables[kIdle], g_tables[kByChar], g_tables[kSpecial], g_tables[kSwing], g_tables[kCast], g_tables[kCastDone],
        g_tables[kTwelve],
        StubEndAction,
        StubPickPose, StubTick, StubTickOnce, StubElevation, StubEnsure, StubSetTint, StubTintRelease, StubRollPending,
        StubRand, StubCue, StubTaskCreate, StubSetTarget, StubLoadDone, StubLoadSound, StubClearBit,
    };
    return s;
}

struct Call { std::uint32_t offset, target; };
// A `call / jmp [reg*4 + disp32]` or `jmp [disp32]` operand: its offset in
// the copy, the table it names, the role's table it is aimed at.
struct Operand { std::uint32_t offset, table; unsigned role; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    Operand operand[2];
    int n_operands;
};

constexpr Call kInitCalls[] = {{0x64, 0x5720C0}};
constexpr Call kStandCalls[] = {{0, 0x4412B0}, {5, 0x441180}};
constexpr Call kIdleCalls[] = {{0x228, 0x441180}};
constexpr Call kTintOnCalls[] = {{0x2A, 0x454CC0}};
constexpr Call kTintOffCalls[] = {{0x5A, 0x454D60}};
constexpr Call kAttackStartCalls[] = {{0x7B, 0x589330}, {0x83, 0x441180}};
constexpr Call kSwingCueCalls[] = {{0, 0x4411B0}, {0x1E, 0x452BF0}, {0x2B, 0x5B93D2}, {0x57, 0x446A50}, {0x61, 0x446A50}};
constexpr Call kSwingEndCalls[] = {{0, 0x4411B0}, {0x16, 0x435180}, {0x24, 0x4530D0}, {0x33, 0x442DD0}};
constexpr Call kCastStartCalls[] = {{0, 0x454810}, {0x32, 0x446E40}, {0x78, 0x446E40}, {0xF2, 0x589330}, {0x10C, 0x4411B0}};
constexpr Call kCastCueCalls[] = {{0xF, 0x446A50}, {0x38, 0x446A50}, {0x4F, 0x4411B0}};
constexpr Call kTickOnceAt0[] = {{0, 0x4411B0}};
constexpr Call kCastDoneBodyCalls[] = {{0xC9, 0x442DD0}};   // the second chunk's tail jump, 0x442C69
constexpr Call kCastDoneCalls[] = {{0x55, 0x4411B0}};
constexpr Call kPoseCalls[] = {{0, 0x454810}, {0x15, 0x589330}, {0x1D, 0x4411B0}};
constexpr Call kEndCalls[] = {{0x30, 0x446FD0}};

enum : unsigned {
    kInit, kStand, kIdleFn, kTintOn, kTintOff, kAttack, kAttackStart, kSwingFn, kSwingCue, kSwingEnd, kCastFn,
    kCastStart, kCastCue, kCastWait, kCastDoneFn, kCastDoneTick, kCastDoneFn2, kCastDoneWait, kTwelveFn, kTwelvePose,
    kTwelveWait, kEnd, kCount
};

#define BO_N(calls) calls, static_cast<int>(sizeof calls / sizeof calls[0])
const Clone kClones[kCount] = {
    {"BattleObj_StateInit", 0x441200, 0xA4, BO_N(kInitCalls), {}, 0},
    {"BattleObj_StateStand", 0x441550, 0x13, BO_N(kStandCalls), {}, 0},
    {"BattleObj_StateIdle", 0x441570, 0x22E, BO_N(kIdleCalls), {{0xF, at::kIdleSubs, kIdle}}, 1},
    {"BattleObj_IdleTintOn", 0x4417A0, 0x46, BO_N(kTintOnCalls), {}, 0},
    {"BattleObj_IdleTintOff", 0x4417F0, 0x6C, BO_N(kTintOffCalls), {}, 0},
    {"BattleObj_StateAttack", 0x441860, 0x23, nullptr, 0, {{0x10, at::kAttackSpecial, kSpecial}, {0x1F, at::kAttackByChar, kByChar}}, 2},
    {"BattleObj_AttackStart", 0x441890, 0x91, BO_N(kAttackStartCalls), {}, 0},
    {"BattleObj_StateSwing", 0x441930, 0x12, nullptr, 0, {{0xE, at::kSwingSubs, kSwing}}, 1},
    {"BattleObj_SwingCue", 0x441950, 0x72, BO_N(kSwingCueCalls), {}, 0},
    {"BattleObj_SwingEnd", 0x4419D0, 0x39, BO_N(kSwingEndCalls), {}, 0},
    {"BattleObj_StateCast", 0x4429E0, 0x12, nullptr, 0, {{0xE, at::kCastSubs, kCast}}, 1},
    {"BattleObj_CastStart", 0x442A00, 0x112, BO_N(kCastStartCalls), {}, 0},
    {"BattleObj_CastCue", 0x442B20, 0x54, BO_N(kCastCueCalls), {}, 0},
    {"BattleObj_CastWait", 0x442B80, 0x20, BO_N(kTickOnceAt0), {}, 0},
    // 0x442BA0..0x442BB6 and its second chunk 0x442C60..0x442C6E, and what
    // lies between them (never run in the copy): its internal jmp stays aimed
    {"BattleObj_StateCastDone", 0x442BA0, 0xD0, BO_N(kCastDoneBodyCalls), {{0xE, at::kCastDoneSubs, kCastDone}}, 1},
    {"BattleObj_CastDoneTick", 0x442BC0, 0xE, BO_N(kTickOnceAt0), {}, 0},
    {"BattleObj_CastDone", 0x442BD0, 0x64, BO_N(kCastDoneCalls), {}, 0},
    {"BattleObj_CastDoneWait", 0x442C50, 0xF, BO_N(kTickOnceAt0), {}, 0},
    {"BattleObj_State12", 0x442D60, 0x12, nullptr, 0, {{0xE, at::kState12Subs, kTwelve}}, 1},
    {"BattleObj_State12Pose", 0x442D80, 0x2B, BO_N(kPoseCalls), {}, 0},
    {"BattleObj_State12Wait", 0x442DB0, 0x1D, BO_N(kTickOnceAt0), {}, 0},
    {"BattleObj_EndAction", 0x442DD0, 0x62, BO_N(kEndCalls), {}, 0},
};
#undef BO_N

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kMembers, 3 * at::kMemberSize},   // ObjTrio: Sprite_Current and Field_State point here
    {at::kPhase, 0x100},                   // the battle's globals, 0x904AA0..0x904B9F
    {at::kTasks, kTaskSlots * at::kTaskSize},
    {0x937F88, 4},                         // Sprite_Current
    {0x905D98, 4},                         // Field_State
    {at::kTargetPtr, 4},
    {at::kHomeX, 8},
    {at::kTints, kTintSlots * 12},
    {at::kAttackCount, 0x28},              // constant data from here on - random here, put back after
    {at::kCastCount, 0x28},
    {at::kSkillWord, kSkills * 24},
};
constexpr unsigned kRegionBytes = 3 * 0x14C + 0x100 + kTaskSlots * 0x84 + 4 + 4 + 4 + 8 + kTintSlots * 12 + 0x28 + 0x28 + kSkills * 24;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char target[sizeof g_target];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.target, g_target, sizeof g_target);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_target, s.target, sizeof g_target);
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

// Random bytes put back inside what they may reach: the two object pointers
// (the same object three rounds in four, as BattleParty_RunStates sets them),
// every member's slot +5, tint +7 and pose +8, the target pointer, the skill.
void Fix() {
    const unsigned m = Next() % 3;
    Sprite_Current = Member(m);
    Field_State = Next() % 4 == 0 ? Member(Next() % 3) : Member(m);
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const o = Member(i);
        o[5] = static_cast<unsigned char>(Often() ? Next() % 3 : Next() % kTaskSlots);
        o[7] = static_cast<unsigned char>(Next() % kTintSlots);
        if (Often()) o[8] = static_cast<unsigned char>(Next() % 4);
    }
    SetLong(At(at::kTargetPtr), static_cast<std::int32_t>(Address(g_target + Next() % sizeof g_target)));
    SetWord(At(at::kSkillId), Next() % kSkills);
}

unsigned char* Target() { return move_script::At(static_cast<std::uint32_t>(Long(At(at::kTargetPtr)))); }

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    unsigned char* const s = S();
    unsigned char* const f = F();
    switch (k) {
    case kIdleFn: {
        static const unsigned char kPhases[] = {0, 1, 1, 5, 5, 2};
        At(at::kPhase)[0] = kPhases[Next() % 6];
        static const unsigned char kArgs[] = {0, 1, 2, 0xFF};
        At(at::kPhaseArg)[0] = kArgs[Next() % 4];
        At(at::kTargetMember)[0] = static_cast<unsigned char>(Next() % 3);
        if (Half()) f[0x89] = At(at::kMembers + At(at::kTargetMember)[0] * at::kMemberSize + 0x89)[0];
        if (Often()) f[0x90] = static_cast<unsigned char>(f[0x90] | 0x20);
        static const signed char kSteps[] = {10, 11, 12, 13, -10, -11, -12, -13, 0, 127, -128};
        Slot(s[5] % kTaskSlots)[9] = static_cast<unsigned char>(kSteps[Next() % 11]);
        if (Half()) SetLong(f + 0x130, Long(f + 0x130) & ~0x1000);
        break;
    }
    case kTintOn:
    case kTintOff: {
        At(at::kTintOn)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        const unsigned pick = Next() % 3;
        Target()[0] = static_cast<unsigned char>(pick == 0 ? s[5] : pick == 1 ? (Next() | 0x80) : Next() & 0x7F);
        break;
    }
    case kAttack:
        f[0x134] = static_cast<unsigned char>(Half() ? f[0x134] & ~1u : f[0x134] | 1);
        f[0x89] = static_cast<unsigned char>(Often() ? Next() % 12 : Next());
        break;
    case kAttackStart:
    case kCastStart:
    case kCastCue: {
        if (Half()) f[0x134] = static_cast<unsigned char>(f[0x134] ^ 2);
        static const unsigned char kKinds[] = {4, 7, 8, 3, 5, 6, 9, 0};
        At(at::kCastKind)[0] = Often() ? kKinds[Next() % 8] : static_cast<unsigned char>(Next() % 28);
        f[0x89] = static_cast<unsigned char>(Often() ? Next() % 12 : Next() % 0x24);
        if (Half()) f[0x125] = 4;
        static const unsigned char kCounts[] = {0, 1, 2};
        if (Often()) s[9] = kCounts[Next() % 3];
        break;
    }
    case kSwingCue: {
        static const unsigned char kCounts[] = {0, 1, 1, 2};
        s[9] = kCounts[Next() % 4];
        static const unsigned char kPercents[] = {0, 1, 50, 99, 100, 0xFF};
        if (Often()) f[0xBA] = kPercents[Next() % 6];
        break;
    }
    case kSwingEnd:
    case kCastDoneFn:
    case kEnd:
        if (Half()) At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] ^ (k == kEnd ? 0x40 : k == kSwingEnd ? 0x80 : 4));
        break;
    case kCastDoneFn2:
        if (Half()) f[0x125] = 4;
        break;
    case kSwingFn:
    case kCastFn:
    case kTwelveFn:
        if (Often()) s[2] = static_cast<unsigned char>(Next() % 5);
        break;
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[200];
    unsigned flips, homes, steps, colours_hit, colours_miss, tinted, released, rolled_hit, rolled_miss, cost, ended_kept;
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 200) ++g_cover.logged[out.log[i].what];
    const std::uint32_t fs = static_cast<std::uint32_t>(Byte(in, 0x905D98) | Byte(in, 0x905D99) << 8 | Byte(in, 0x905D9A) << 16 |
                                                        Byte(in, 0x905D9B) << 24);
    switch (k) {
    case kIdleFn:
        if ((Byte(in, fs + 0x131) ^ Byte(out, fs + 0x131)) & 8) ++g_cover.flips;
        if (Byte(in, at::kPhase) == 5) ++g_cover.homes;
        else if (Byte(in, fs + 0x90) & 0x20) ++g_cover.steps;
        if (Byte(in, at::kPhase) == 1 && Byte(in, at::kPhaseArg) > 1) {
            const std::uint32_t sc = static_cast<std::uint32_t>(Byte(in, 0x937F88) | Byte(in, 0x937F89) << 8 | Byte(in, 0x937F8A) << 16 |
                                                                Byte(in, 0x937F8B) << 24);
            ++(Byte(out, sc + 0x5D) == 0x78 ? g_cover.colours_hit : g_cover.colours_miss);
        }
        break;
    case kTintOn:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 6) ++g_cover.tinted;
        break;
    case kTintOff:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 7) ++g_cover.released;
        break;
    case kSwingCue:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 9) ++(((Byte(out, at::kRoundFlags) ^ Byte(in, at::kRoundFlags)) & 0x80) ? g_cover.rolled_hit : g_cover.rolled_miss);
        break;
    case kCastDoneFn2:
        if (Byte(in, fs + 0x125) == 4) ++g_cover.cost;
        break;
    case kEnd:
        if (Byte(in, at::kRoundFlags) & 0x40) ++g_cover.ended_kept;
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
        bof3::Fatal("battle_obj_states: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    for (unsigned role = 0; role < kRoles; ++role)
        for (unsigned i = 0; i < 256; ++i)
            g_tables[role][i] = Address(reinterpret_cast<const void*>(AllStubs::f[(i + 11 * role) % kStubs]));

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[8];
        if (c.n_calls > 8) bof3::Fatal("battle_obj_states: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        auto* const code = static_cast<unsigned char*>(clones[k]);
        for (int i = 0; i < c.n_operands; ++i) {
            const Operand& o = c.operand[i];
            std::uint32_t disp;
            std::memcpy(&disp, code + o.offset, 4);
            if (disp != o.table)
                bof3::Fatal("battle_obj_states: %s +0x%X holds 0x%X, not the table 0x%X", c.name, static_cast<unsigned>(o.offset),
                            static_cast<unsigned>(disp), static_cast<unsigned>(o.table));
            const std::uint32_t ours = Address(g_tables[o.role]);
            std::memcpy(code + o.offset, &ours, 4);
        }
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&BattleObj_StateInit), reinterpret_cast<const void*>(&BattleObj_StateStand),
        reinterpret_cast<const void*>(&BattleObj_StateIdle), reinterpret_cast<const void*>(&BattleObj_IdleTintOn),
        reinterpret_cast<const void*>(&BattleObj_IdleTintOff), reinterpret_cast<const void*>(&BattleObj_StateAttack),
        reinterpret_cast<const void*>(&BattleObj_AttackStart), reinterpret_cast<const void*>(&BattleObj_StateSwing),
        reinterpret_cast<const void*>(&BattleObj_SwingCue), reinterpret_cast<const void*>(&BattleObj_SwingEnd),
        reinterpret_cast<const void*>(&BattleObj_StateCast), reinterpret_cast<const void*>(&BattleObj_CastStart),
        reinterpret_cast<const void*>(&BattleObj_CastCue), reinterpret_cast<const void*>(&BattleObj_CastWait),
        reinterpret_cast<const void*>(&BattleObj_StateCastDone), reinterpret_cast<const void*>(&BattleObj_CastDoneTick),
        reinterpret_cast<const void*>(&BattleObj_CastDone), reinterpret_cast<const void*>(&BattleObj_CastDoneWait),
        reinterpret_cast<const void*>(&BattleObj_State12), reinterpret_cast<const void*>(&BattleObj_State12Pose),
        reinterpret_cast<const void*>(&BattleObj_State12Wait), reinterpret_cast<const void*>(&BattleObj_EndAction)};

    static State saved, input, their_out, our_out;
    Capture(saved);
    g = Stubbed();

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.target) b = static_cast<unsigned char>(Next());
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
                bof3::Log("shadow      battle_obj_states self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    Apply(saved);

    bof3::Log("shadow      battle_obj_states self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the party objects, the battle's globals, the task slots, the two object pointers, the target, "
              "the tint records, the byte tables, the skill records and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_obj_states: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned handlers = 0, handler_calls = 0;
    for (unsigned i = 100; i < 100 + kStubs; ++i) {
        handlers += c.logged[i] ? 1u : 0u;
        handler_calls += c.logged[i];
    }
    bof3::Log("shadow      battle_obj_states coverage: table entries %u of %u (%u calls); pick pose %u, ticks %u / once %u, "
              "heights %u, poses %u, tints %u, releases %u, pending %u, rands %u, cues %u, tasks %u, targets %u, loads %u, "
              "sounds %u, actor bits %u, action ends %u; idle: flips %u, homes %u, steps %u, colours %u / %u; tinted %u, "
              "released %u; rolls under %u over %u; costs %u; ends keeping the action %u",
              handlers, kStubs, handler_calls, c.logged[1], c.logged[2], c.logged[3], c.logged[4], c.logged[5], c.logged[6],
              c.logged[7], c.logged[8], c.logged[9], c.logged[10], c.logged[11], c.logged[12], c.logged[13], c.logged[14],
              c.logged[15], c.logged[16], c.flips, c.homes, c.steps, c.colours_hit, c.colours_miss, c.tinted, c.released,
              c.rolled_hit, c.rolled_miss, c.cost, c.ended_kept);
    if (bad) bof3::Fatal("the battle object states differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_obj_states

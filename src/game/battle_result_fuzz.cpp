// BOF3X_SHADOW=battle_result: a differential fuzz of the battle result, once
// at start-up. docs/battle_result.md section 4.
//
// Seventeen byte-copies, every call out re-aimed at a recording stand-in
// (bof3::CloneCall with `expected`); the two tables the window states build
// on their stacks re-aimed inside the copies (their immediates checked
// first); the .data step tables 0x64AFC0 and 0x64AFC8 pointed at recorders
// for the two dispatch stubs. One round: one function, random bytes in every
// region any of them touches, the pointers and indices put back inside what
// the tables hold, each branch's boundaries seeded; theirs, then from the
// same state ours; the regions and the stand-ins' log compared. Everything
// is put back afterwards.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_result_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_result {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }
std::uint32_t Dw(std::uint32_t address) { return static_cast<std::uint32_t>(Long(At(address))); }
unsigned char* WindowSlot(unsigned slot) { return At(at::kWindows + slot * at::kWindowSize); }
constexpr unsigned kWindowSlots = 0x16;
constexpr unsigned kMaxMembers = 8;

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 200;
struct Entry { std::uint32_t what, a, b, c, d, e; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

// Msg_SystemPtr's answers: a text of our own per call.
constexpr unsigned kMsg = 0x100;
unsigned char g_msg[kMsg];

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p), msg = Address(g_msg);
    if (at >= msg && at < msg + sizeof g_msg) return 0x10000u + (at - msg);
    return at;
}
std::uint32_t Bytes(const unsigned char* p, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}
// A text a stand-in was given: its address and its first 12 bytes, so that
// a text drawn before it was formatted, or formatted into the wrong record,
// shows.
std::uint32_t TextHash(const unsigned char* p) { return Bytes(p, 12); }

unsigned char* Member(unsigned slot) { return At(at::kMembers + slot * at::kMemberSize); }

// Set for the DrawExp rounds seeded with 14..16 slots, where the line's y
// byte wraps: the stand-ins' moves of the party count then keep it long.
bool g_long;
unsigned char PartyCount(unsigned v, unsigned short_mod) {
    return static_cast<unsigned char>(g_long ? 14 + v % 3 : v % short_mod);
}

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the arrays, indices inside the tables.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 22) {
    case 0: At(at::kStep)[0] = static_cast<unsigned char>(v); break;
    case 1: At(at::kPhase2)[0] = static_cast<unsigned char>(v); break;
    case 2: At(at::kMember)[0] = static_cast<unsigned char>(v % 5); break;
    case 3: At(at::kPartyCount)[0] = PartyCount(v, 5); break;
    case 4: At(at::kDropCount)[0] = static_cast<unsigned char>(v % 18); break;
    case 5: SetLong(At(at::kExpTotal), static_cast<std::int32_t>(v % 2 ? w % 100 : h * 0x9E3779B1u)); break;
    case 6: SetLong(At(at::kZennyTotal), static_cast<std::int32_t>(v % 2 ? w % 100 : h * 0x9E3779B1u)); break;
    case 7: SetWord(At(at::kTickStep), v % 2 ? w % 40 : w); break;
    case 8: Member(w % kMaxMembers)[9] = static_cast<unsigned char>(v); break;
    case 9: Member(w % kMaxMembers)[0xA] = static_cast<unsigned char>(v % 3 ? 0x62 + v % 3 : v); break;
    case 10: At(at::kPartySet)[0] = static_cast<unsigned char>(v); break;
    case 11: At(at::kBattleEnd)[0] = static_cast<unsigned char>(At(at::kBattleEnd)[0] ^ 0x20); break;
    case 12: WindowSlot(1)[w % 10] = static_cast<unsigned char>(v); break;
    case 13: WindowSlot(0x15)[w % 10] = static_cast<unsigned char>(v); break;
    case 14: SetWord(At(at::kDropItems + 2 * (w % 18)), h >> 16); break;
    case 15: At(at::kDropCounts + w % 18)[0] = static_cast<unsigned char>(v); break;
    case 16: SetWord(At(at::kBankIndex), h >> 16); break;
    case 17: SetLong(At(at::kPartyZenny), static_cast<std::int32_t>(h * 0x2545F491u)); break;
    case 18: SetPtr(at::kWindowCurrent, WindowSlot(w % kWindowSlots)); break;
    case 19: At(at::kText2 + w % 16)[0] = static_cast<unsigned char>(v); break;
    case 20: At(at::kText0 + w % 16)[0] = static_cast<unsigned char>(v); break;
    default: SetWord(At(at::kHeld + 4 * (w % 2)), v % 3 ? 0 : h >> 16); break;
    }
}

// --- the stand-ins ---------------------------------------------------------

// The six step entries of the two .data tables and the four stack-table
// entries of the window states.
template <unsigned N> void __cdecl StubHandler() {
    Record(N, At(at::kStep)[0], Id(At(Dw(at::kWindowCurrent))));
    Disturb();
}
// The counts and flags a caller branches on: the edges more often than not.
unsigned char SmallAnswer() {
    const std::uint32_t h = Hash();
    static const unsigned char kEdges[] = {0, 0, 1, 1, 2, 3, 0xFF, 0x80};
    return h % 4 == 0 ? static_cast<unsigned char>(h >> 8) : kEdges[(h >> 8) % 8];
}
unsigned char __cdecl StubCountMembers() {
    Record(40);
    if (Hash() % 2) SetLong(At(at::kExpTotal), static_cast<std::int32_t>(Hash() * 0x9E3779B1u));
    Disturb();
    return SmallAnswer();
}
unsigned char __cdecl StubZennyBonus() {
    Record(41);
    Disturb();
    return SmallAnswer();
}
void __cdecl StubAddExp(unsigned n) {
    Record(42, n);
    if (Hash() % 2) SetLong(At(at::kExpTotal), static_cast<std::int32_t>(Dw(at::kExpTotal) + (Hash() >> 28)));
    if (Hash() % 3 == 0) SetWord(At(at::kTickStep), Hash() >> 24);
    Disturb();
}
// 0x4469D0 reads its id as a byte; its answer is a byte.
unsigned char __cdecl StubRosterIndex(unsigned id) {
    Record(43, id & 0xFF);
    if (Hash() % 2) At(at::kMember)[0] = static_cast<unsigned char>(Hash() % 5);
    Disturb();
    return static_cast<unsigned char>(Hash() >> 5);
}
// 0x432170 reads its index as a byte and its second argument as a byte;
// the callers test the answer's word.
unsigned __cdecl StubLevelUpPending(unsigned index, unsigned b) {
    Record(44, index & 0xFF, b & 0xFF);
    const std::uint32_t h = Hash();
    if ((h >> 3) % 2) At(at::kPartyCount)[0] = PartyCount(h >> 8, 5);
    Disturb();
    // non-zero one call in three - half of those with a low byte of 0 -
    // and half of the zeros only in the low word
    if (h % 3 == 0) return (h >> 9) % 2 ? (h | 1) : ((h & 0xFFFF0000u) | 0x100u | (h & 0xFE00u));
    return h % 2 ? 0 : h & 0xFFFF0000u;
}
void __cdecl StubLevelUp(unsigned index) {
    Record(45, index & 0xFF);
    if (Hash() % 2) Member((Hash() >> 8) % kMaxMembers)[9] = static_cast<unsigned char>(Hash() >> 16);
    Disturb();
}
unsigned char __cdecl StubAddZenny(unsigned n, unsigned flag) {
    Record(46, n, flag & 0xFF);
    if (Hash() % 2) SetLong(At(at::kZennyTotal), static_cast<std::int32_t>(Dw(at::kZennyTotal) + (Hash() >> 28)));
    Disturb();
    return static_cast<unsigned char>(Hash());
}
// 0x5982D0 reads its four arguments as words (the original's fourth carries
// a register's upper half).
void __cdecl StubDrawFrame(int x, int y, int w, int h) {
    Record(47, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF,
           static_cast<std::uint32_t>(w) & 0xFFFF, static_cast<std::uint32_t>(h) & 0xFFFF);
    if (Hash() % 2) At(at::kPartyCount)[0] = PartyCount(Hash(), 6);
    Disturb();
}
// 0x598810 reads its slot as a byte; its answer is a dword, printed.
unsigned __cdecl StubExpToNext(unsigned slot) {
    Record(48, slot & 0xFF);
    Disturb();
    return Hash();
}
// A short text of the value into the buffer, so that the draws that read it
// see what was formatted.
int __cdecl StubSprintf(char* dst, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const unsigned value = va_arg(ap, unsigned);
    va_end(ap);
    Record(49, Id(dst), Address(fmt), value);
    const std::uint32_t h = Hash();
    unsigned n = 0;
    std::uint32_t v = value ^ (h & 0xF);
    do { dst[n++] = static_cast<char>('0' + v % 10); v /= 10; } while (v && n < 10);
    dst[n] = 0;
    Disturb();
    return static_cast<int>(n);
}
const unsigned char* __cdecl StubMsgSystem(unsigned id) {
    Record(50, id);
    const std::uint32_t h = Hash();
    unsigned char* const p = g_msg + (h % (kMsg - 16));
    for (unsigned i = 0; i < 12; ++i) p[i] = static_cast<unsigned char>(h >> (i * 2));
    if ((h >> 7) % 2) At(at::kPartySet)[0] = static_cast<unsigned char>(h >> 9);
    if ((h >> 8) % 2) SetLong(At(at::kZennyTotal), static_cast<std::int32_t>(h * 0x9E3779B1u % 200));
    Disturb();
    return p;
}
// PartySet_Select reads both as bytes.
void __cdecl StubPartySet(unsigned set, unsigned mode) {
    Record(51, set & 0xFF, mode & 0xFF);
    if (Hash() % 2) At(at::kStep)[0] = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}
// Window_Alloc as the real one: a free slot (byte +0 zero) taken - +0 1,
// +1 the kind, +2 and +3 0 - so that a store moved across it shows.
unsigned __cdecl StubWindowAlloc(unsigned slot, unsigned kind) {
    Record(52, slot & 0xFF, kind & 0xFF);
    if ((slot & 0xFF) < kWindowSlots) {
        unsigned char* const w = WindowSlot(slot & 0xFF);
        if (w[0] == 0) {
            w[0] = 1;
            w[1] = static_cast<unsigned char>(kind);
            w[2] = 0;
            w[3] = 0;
        }
        w[9] = static_cast<unsigned char>(Hash());
    }
    const std::uint32_t h = Hash();
    if ((h >> 3) % 2) At(at::kDropCount)[0] = static_cast<unsigned char>((h >> 8) % 18);
    if ((h >> 4) % 2) At(at::kPartyCount)[0] = PartyCount(h >> 12, 5);
    if ((h >> 5) % 2) SetLong(At(at::kExpTotal), static_cast<std::int32_t>((h >> 16) % 3 ? (h >> 20) % 64 : h));
    if ((h >> 6) % 2) SetLong(At(at::kZennyTotal), static_cast<std::int32_t>((h >> 16) % 3 ? (h >> 20) % 64 : h * 7));
    Disturb();
    return h;
}
int __cdecl StubFileLoadDone() {
    Record(53);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : static_cast<int>(h & 0xFFFF0000u);
}
void __cdecl StubSndLoadBank(unsigned index) {
    Record(54, index);
    Disturb();
}
// Inventory_Add reads its three arguments as bytes; the fourth is recorded whole.
unsigned char __cdecl StubInventoryAdd(unsigned category, unsigned item, unsigned count, unsigned d) {
    Record(55, category & 0xFF, item & 0xFF, count & 0xFF, d);
    if (Hash() % 2) At(at::kDropCount)[0] = static_cast<unsigned char>(Hash() % 18);
    Disturb();
    return static_cast<unsigned char>(Hash());
}
// Text_DrawAt / Text_DrawFont12 read x and y as words; the colour, the count
// and the text whole.
const unsigned char* __cdecl StubTextDrawAt(int x, int y, int color, int count, const unsigned char* text) {
    Record(56, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF,
           static_cast<std::uint32_t>(color), static_cast<std::uint32_t>(count), Id(text) ^ TextHash(text));
    if (Hash() % 2) At(at::kPartyCount)[0] = PartyCount(Hash(), 6);
    if (Hash() % 3 == 0) Member((Hash() >> 8) % kMaxMembers)[0xA] = static_cast<unsigned char>(Hash() % 2 ? 0x63 : Hash() >> 16);
    Disturb();
    return text;
}
void __cdecl StubTextDrawFont12(int x, int y, int colour, const unsigned char* text) {
    Record(57, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF,
           static_cast<std::uint32_t>(colour), Id(text) ^ TextHash(text));
    Disturb();
}
void __cdecl StubDrawMediumBox(int x, int y) {
    Record(58, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    if (Hash() % 2) SetLong(At(at::kPartyZenny), static_cast<std::int32_t>(Hash() * 0x2545F491u));
    Disturb();
}

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case kCountMembers: return f(&StubCountMembers);
    case kZennyBonus: return f(&StubZennyBonus);
    case kAddExp: return f(&StubAddExp);
    case kRosterIndex: return f(&StubRosterIndex);
    case kLevelUpPending: return f(&StubLevelUpPending);
    case kLevelUp: return f(&StubLevelUp);
    case kAddZenny: return f(&StubAddZenny);
    case kDrawFrame: return f(&StubDrawFrame);
    case kExpToNext: return f(&StubExpToNext);
    case 0x5B9380: return f(&StubSprintf);
    case 0x497740: return f(&StubMsgSystem);
    case 0x536AC0: return f(&StubPartySet);
    case 0x59E2D0: return f(&StubWindowAlloc);
    case 0x454810: return f(&StubFileLoadDone);
    case 0x454770: return f(&StubSndLoadBank);
    case 0x590BB0: return f(&StubInventoryAdd);
    case 0x516B30: return f(&StubTextDrawAt);
    case 0x516F60: return f(&StubTextDrawFont12);
    case 0x443870: return f(&StubDrawMediumBox);
    default: bof3::Fatal("battle_result: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

const Callees kStubs = {
    {&StubHandler<100>, &StubHandler<101>},
    {&StubHandler<102>, &StubHandler<103>},
    StubCountMembers, StubZennyBonus, StubAddExp, StubRosterIndex, StubLevelUpPending, StubLevelUp, StubAddZenny,
    StubDrawFrame, StubExpToNext, StubSprintf, StubMsgSystem, StubPartySet, StubWindowAlloc, StubFileLoadDone,
    StubSndLoadBank, StubInventoryAdd, StubTextDrawAt, StubTextDrawFont12, StubDrawMediumBox,
};
// The .data step tables the two dispatch stubs jump through: 0x64AFC0's two
// entries and 0x64AFC8's four.
constexpr unsigned kLevelUpEntries = 2, kRewardEntries = 4;
const Handler kLevelUpStubs[kLevelUpEntries] = {&StubHandler<110>, &StubHandler<111>};
const Handler kRewardStubs[kRewardEntries] = {&StubHandler<120>, &StubHandler<121>, &StubHandler<122>, &StubHandler<123>};

// The stack-built tables: the offset of each imm32 in the copy (capstone,
// 2026-09-25) and the handler it names.
struct Imm { std::uint32_t offset, value; };
constexpr Imm kExpImm[2] = {{0x0F, 0x5986F0}, {0x17, 0x5985A0}};
constexpr Imm kZennyImm[2] = {{0x0F, 0x5986F0}, {0x17, 0x598700}};

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
};

constexpr Call kSplitCalls[] = {{0xA, kCountMembers}, {0x42, 0x5B9380}, {0x49, 0x497740}, {0x5F, 0x536AC0}};
constexpr Call kOpenCalls[] = {{0x4, 0x59E2D0}};
constexpr Call kExpTickCalls[] = {{0xA, 0x454810}, {0x34, kAddExp}, {0x56, kAddExp}};
constexpr Call kExpDoneCalls[] = {{0x0, 0x454810}};
constexpr Call kFindCalls[] = {{0x26, kRosterIndex}, {0x41, kLevelUpPending}};
constexpr Call kSetupCalls[] = {{0x55, kRosterIndex}, {0x5E, kLevelUpPending}, {0x73, kRosterIndex}, {0x79, kLevelUp},
                                {0x90, kZennyBonus},  {0xB4, 0x59E2D0},        {0xD7, 0x5B9380},     {0xDE, 0x497740},
                                {0x123, 0x59E2D0},    {0x251, 0x454770}};
constexpr Call kZennyTickCalls[] = {{0xA, 0x454810}, {0x38, kAddZenny}, {0x5A, kAddZenny}};
constexpr Call kAwardCalls[] = {{0x1, 0x454810}, {0x3E, 0x590BB0}};
constexpr Call kDrawExpCalls[] = {{0x18, kDrawFrame}, {0x5E, 0x516B30}, {0x81, 0x5B9380}, {0x93, 0x516F60},
                                  {0x99, kExpToNext}, {0xA9, 0x5B9380}, {0xBB, 0x516F60}, {0xC2, 0x497740},
                                  {0xD5, 0x516B30}, {0xE1, 0x497740}, {0xF1, 0x516B30}};
constexpr Call kDrawZennyCalls[] = {{0x4, 0x443870}, {0x19, 0x5B9380}, {0x29, 0x516F60}, {0x41, 0x516B30}};

enum : unsigned {
    kSplitExp, kOpenExpWindow, kExpWaitHeld, kExpTick, kExpDone, kLevelUpStep, kFindLevelUp, kRewardStep, kSetup,
    kZennyWaitHeld, kZennyTick, kAwardDrops, kExpState, kDrawExp, kZennyState, kNextStep, kDrawZenny, kCount
};

#define BR_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
#define BR_P(name, base, size) {name, base, size, nullptr, 0}
// The extents are the disassembly's, to the last instruction (capstone,
// 2026-09-25) - shorter than the round's upper bounds, which run on into the
// padding.
const Clone kClones[kCount] = {
    BR_C("BattleResult_SplitExp", 0x431940, 0x6E, kSplitCalls),
    BR_C("BattleResult_OpenExpWindow", 0x431A20, 0x68, kOpenCalls),
    BR_P("BattleResult_ExpWaitHeld", 0x431A90, 0x11),
    BR_C("BattleResult_ExpTick", 0x431AB0, 0x75, kExpTickCalls),
    BR_C("BattleResult_ExpDone", 0x431B30, 0x24, kExpDoneCalls),
    BR_P("BattleResult_LevelUpStep", 0x431B60, 0x11),
    BR_C("BattleResult_FindLevelUp", 0x431B80, 0x8D, kFindCalls),
    BR_P("BattleResult_RewardStep", 0x431D50, 0x11),
    BR_C("BattleResult_Setup", 0x431D70, 0x26B, kSetupCalls),
    BR_P("BattleResult_ZennyWaitHeld", 0x432050, 0x1E),
    BR_C("BattleResult_ZennyTick", 0x432070, 0x79, kZennyTickCalls),
    BR_C("BattleResult_AwardDrops", 0x4320F0, 0x74, kAwardCalls),
    BR_P("BattleResultWin_ExpState", 0x598570, 0x26),
    BR_C("BattleResultWin_DrawExp", 0x5985A0, 0x11A, kDrawExpCalls),
    BR_P("BattleResultWin_ZennyState", 0x5986C0, 0x26),
    BR_P("BattleResultWin_NextStep", 0x5986F0, 0x9),
    BR_C("BattleResultWin_DrawZenny", 0x598700, 0x4A, kDrawZennyCalls),
};
#undef BR_C
#undef BR_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x904AA0, 0x460},                                  // the battle's globals, the drop list, the text records, 0x904EFC
    {at::kPartyZenny, 4},
    {at::kPartySet, 1},
    {at::kMsgCurrent, 4},
    {at::kHeld, 8},                                     // Input_Held .. Input_Pressed
    {at::kMembers, kMaxMembers * at::kMemberSize},      // the party slots' records, and past the third
    {at::kWindows, kWindowSlots * at::kWindowSize},     // the window records 0..0x15
    {at::kWindowCurrent, 4},
};
constexpr unsigned kRegionBytes = 0x460 + 4 + 1 + 4 + 8 + kMaxMembers * 0x14C + kWindowSlots * 36 + 4;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char msg[kMsg];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.msg, g_msg, sizeof g_msg);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_msg, s.msg, sizeof g_msg);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
unsigned ByteOf(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}
std::uint32_t DwordOf(const State& s, std::uint32_t address) {
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= (ByteOf(s, address + i) & 0xFF) << (8 * i);
    return v;
}

std::uint32_t g_rng = 0x2F6B1D35u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

// Random bytes put back inside what the tables and buffers hold: the window
// pointer, the counts, the text records' ends.
void Fix() {
    SetPtr(at::kWindowCurrent, WindowSlot(Next() % kWindowSlots));
    At(at::kPartyCount)[0] = static_cast<unsigned char>(Often() ? Next() % 5 : Next() % (kMaxMembers + 1));
    At(at::kDropCount)[0] = static_cast<unsigned char>(Often() ? Next() % 17 : Next() % 0x21);
    At(at::kMember)[0] = static_cast<unsigned char>(Next() % 5);
    for (unsigned i = 0; i < kMaxMembers; ++i)
        if (Half()) Member(i)[0xA] = static_cast<unsigned char>(Half() ? 0x63 : Next() % 0x63);
    if (Half()) SetWord(At(at::kHeld), 0);
    if (Half()) SetWord(At(at::kPressed), 0);
    if (Half()) At(at::kBattleEnd)[0] = static_cast<unsigned char>(At(at::kBattleEnd)[0] & ~0x20u);
    for (unsigned s = 0; s < kWindowSlots; ++s)
        if (Half()) WindowSlot(s)[0] = 0;
    At(at::kText0 + 0x1F)[0] = 0;
    At(at::kText2 + 0x1F)[0] = 0;
}

// Amounts at the tick's edges: 0, 1, 29 / 30 / 31, the step and one either
// side of it, and anything - negative differences included.
std::uint32_t Amount(std::uint32_t step) {
    static const std::uint32_t kEdges[] = {0, 1, 2, 29, 30, 31, 59, 60, 61, 899, 900, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
    const unsigned pick = Next() % 6;
    if (pick == 0) return Next();
    if (pick == 1) return step + (Next() % 3) - 1;
    if (pick == 2) return step + 0x80000000u + (Next() % 3) - 1;
    return kEdges[Next() % (sizeof kEdges / sizeof kEdges[0])];
}

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    g_long = false;
    switch (k) {
    case kSplitExp:
        SetLong(At(at::kExpTotal), static_cast<std::int32_t>(Half() ? 0 : Amount(Next() % 5)));
        break;
    case kOpenExpWindow:
    case kSetup:
        SetLong(At(at::kExpTotal), static_cast<std::int32_t>(Amount(30)));
        SetLong(At(at::kZennyTotal), static_cast<std::int32_t>(Amount(30)));
        if (k == kSetup) {
            if (Often()) SetWord(At(at::kHeld), Next() | 1);
            if (Half()) SetWord(At(at::kPressed), Half() ? 0 : Next() | 0x100);
            if (Half()) At(at::kBattleEnd)[0] = static_cast<unsigned char>(At(at::kBattleEnd)[0] | 0x20);
            if (Half()) WindowSlot(1)[0] = static_cast<unsigned char>(Next() | 1);
            if (Half()) WindowSlot(0x15)[0] = 0;
            // drop lists with equal items, and counts of 0, 1, 2 and past 16
            static const unsigned char kCounts[] = {0, 1, 2, 3, 5, 15, 16, 17, 0x20};
            if (Often()) At(at::kDropCount)[0] = kCounts[Next() % 9];
            for (unsigned i = 0; i < 0x20; ++i)
                if (Often()) SetWord(At(at::kDropItems + 2 * i), Half() ? Next() % 4 : (Next() % 4) << 8 | Next() % 4);
        }
        break;
    case kExpWaitHeld:
    case kZennyWaitHeld:
        SetWord(At(at::kHeld), Half() ? 0 : Half() ? 0x100 : Next());
        break;
    case kExpTick:
    case kZennyTick: {
        const std::uint32_t step = Often() ? Next() % 40 : Next();
        SetLong(At(at::kTickStep), static_cast<std::int32_t>(Half() ? step : (Next() & 0xFFFF0000u) | (step & 0xFFFF)));
        SetLong(At(k == kExpTick ? at::kExpTotal : at::kZennyTotal), static_cast<std::int32_t>(Amount(step & 0xFFFF)));
        if (Half()) SetWord(At(at::kHeld), Half() ? 0x100 : Next() | 1);
        break;
    }
    case kLevelUpStep:
        At(at::kStep)[0] = static_cast<unsigned char>(Next() % kLevelUpEntries);
        break;
    case kRewardStep:
        At(at::kStep)[0] = static_cast<unsigned char>(Next() % kRewardEntries);
        break;
    case kFindLevelUp:
        At(at::kMember)[0] = static_cast<unsigned char>(Next() % 5);
        break;
    case kAwardDrops: {
        static const unsigned char kCounts[] = {0, 1, 2, 3, 15, 16, 17};
        if (Often()) At(at::kDropCount)[0] = kCounts[Next() % 7];
        break;
    }
    case kExpState:
    case kZennyState:
        At(Dw(at::kWindowCurrent) + 3)[0] = static_cast<unsigned char>(Next() % 2);
        break;
    case kDrawExp:
        // the y byte wraps past 14 slots
        g_long = Next() % 8 == 0;
        if (g_long) At(at::kPartyCount)[0] = static_cast<unsigned char>(14 + Next() % 3);
        break;
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[130];
    unsigned split_zero, split_shared, tick_whole, tick_step, tick_rest, levelup_found, levelup_none, setup_ran,
        sorted_swaps, past16, bonus, awards, max_level, wrapped, dropped_window;
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 130) ++g_cover.logged[out.log[i].what];
    auto logged = [&](std::uint32_t what) {
        unsigned n = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) n += out.log[i].what == what;
        return n;
    };
    switch (k) {
    case kSplitExp:
        ++(DwordOf(out, at::kExpTotal) == 0 ? g_cover.split_zero : g_cover.split_shared);
        break;
    case kExpTick:
    case kZennyTick: {
        const std::uint32_t total = k == kExpTick ? at::kExpTotal : at::kZennyTotal;
        if (DwordOf(out, total) == 0 && DwordOf(in, total) != 0) {
            if (logged(53) && out.log_n >= 2) ++g_cover.tick_whole;
            else ++g_cover.tick_rest;
        } else if (out.log_n) {
            ++g_cover.tick_step;
        }
        break;
    }
    case kFindLevelUp:
        if ((ByteOf(out, at::kBattleEnd) & 0x20) && !(ByteOf(in, at::kBattleEnd) & 0x20)) ++g_cover.levelup_found;
        else if (ByteOf(out, at::kPhase2) != ByteOf(in, at::kPhase2)) ++g_cover.levelup_none;
        break;
    case kSetup: {
        if (!out.log_n) break;
        ++g_cover.setup_ran;
        g_cover.bonus += DwordOf(out, at::kZennyTotal) != DwordOf(in, at::kZennyTotal);
        unsigned allocs = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 52) ++allocs;
        if (allocs >= 2) ++g_cover.dropped_window;
        unsigned moved = 0;
        for (unsigned i = 0; i < 0x40; ++i) moved += ByteOf(out, at::kDropItems + i) != ByteOf(in, at::kDropItems + i);
        if (moved) ++g_cover.sorted_swaps;
        if (moved && ByteOf(in, at::kDropCount) > 16) ++g_cover.past16;
        break;
    }
    case kAwardDrops:
        g_cover.awards += logged(55);
        break;
    case kDrawExp: {
        g_cover.max_level += logged(50);
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 56 && out.log[i].a == 0x19 && out.log[i].b < 0x2C) { ++g_cover.wrapped; break; }
        break;
    }
    default:
        break;
    }
}

using Fn0 = void (__cdecl*)();

void PatchImms(void* copy, const char* name, const Imm* imms, unsigned n, const Handler* to) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (unsigned i = 0; i < n; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + imms[i].offset, sizeof had);
        if (had != imms[i].value)
            bof3::Fatal("battle_result: %s +0x%X holds 0x%X, not the handler 0x%X", name, static_cast<unsigned>(imms[i].offset),
                        static_cast<unsigned>(had), static_cast<unsigned>(imms[i].value));
        const std::uint32_t target = Address(reinterpret_cast<const void*>(to[i]));
        std::memcpy(code + imms[i].offset, &target, sizeof target);
    }
}

// The .data table entries, checked against what the original holds before
// they are swapped: a table someone moved is caught, not fuzzed.
void SwapTable(std::uint32_t table, const std::uint32_t* expected, const Handler* to, unsigned n, std::uint32_t* saved) {
    for (unsigned i = 0; i < n; ++i) {
        saved[i] = Dw(table + 4 * i);
        if (saved[i] != expected[i])
            bof3::Fatal("battle_result: 0x%X [%u] holds 0x%X, not 0x%X", static_cast<unsigned>(table), i,
                        static_cast<unsigned>(saved[i]), static_cast<unsigned>(expected[i]));
        SetPtr(table + 4 * i, reinterpret_cast<const void*>(to[i]));
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_result: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[12];
        if (c.n_calls > 12) bof3::Fatal("battle_result: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    PatchImms(clones[kExpState], "BattleResultWin_ExpState", kExpImm, 2, kStubs.exp_window);
    PatchImms(clones[kZennyState], "BattleResultWin_ZennyState", kZennyImm, 2, kStubs.zenny_window);

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&BattleResult_SplitExp), reinterpret_cast<const void*>(&BattleResult_OpenExpWindow),
        reinterpret_cast<const void*>(&BattleResult_ExpWaitHeld), reinterpret_cast<const void*>(&BattleResult_ExpTick),
        reinterpret_cast<const void*>(&BattleResult_ExpDone), reinterpret_cast<const void*>(&BattleResult_LevelUpStep),
        reinterpret_cast<const void*>(&BattleResult_FindLevelUp), reinterpret_cast<const void*>(&BattleResult_RewardStep),
        reinterpret_cast<const void*>(&BattleResult_Setup), reinterpret_cast<const void*>(&BattleResult_ZennyWaitHeld),
        reinterpret_cast<const void*>(&BattleResult_ZennyTick), reinterpret_cast<const void*>(&BattleResult_AwardDrops),
        reinterpret_cast<const void*>(&BattleResultWin_ExpState), reinterpret_cast<const void*>(&BattleResultWin_DrawExp),
        reinterpret_cast<const void*>(&BattleResultWin_ZennyState), reinterpret_cast<const void*>(&BattleResultWin_NextStep),
        reinterpret_cast<const void*>(&BattleResultWin_DrawZenny)};

    static State saved, input, their_out, our_out;
    static const std::uint32_t kLevelUpHad[kLevelUpEntries] = {0x431B80, 0x431C10};
    static const std::uint32_t kRewardHad[kRewardEntries] = {0x431D70, 0x432050, 0x432070, 0x4320F0};
    std::uint32_t saved_levelup[kLevelUpEntries], saved_reward[kRewardEntries];
    Capture(saved);
    g = kStubs;
    SwapTable(at::kLevelUpSteps, kLevelUpHad, kLevelUpStubs, kLevelUpEntries, saved_levelup);
    SwapTable(at::kRewardSteps, kRewardHad, kRewardStubs, kRewardEntries, saved_reward);

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.msg) b = static_cast<unsigned char>(Next());
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
                bof3::Log("shadow      battle_result self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    for (unsigned i = 0; i < kLevelUpEntries; ++i) SetLong(At(at::kLevelUpSteps + 4 * i), static_cast<std::int32_t>(saved_levelup[i]));
    for (unsigned i = 0; i < kRewardEntries; ++i) SetLong(At(at::kRewardSteps + 4 * i), static_cast<std::int32_t>(saved_reward[i]));
    Apply(saved);

    bof3::Log("shadow      battle_result self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the battle's globals, the drop list, the text records, the party records, the window "
              "records, the input words, Msg_SystemPtr's texts and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_result: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned steps = 0;
    for (unsigned i = 100; i < 104; ++i) steps += c.logged[i] ? 1u : 0u;
    for (unsigned i = 110; i < 112; ++i) steps += c.logged[i] ? 1u : 0u;
    for (unsigned i = 120; i < 124; ++i) steps += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      battle_result coverage: table entries %u of 10; split zero %u shared %u; ticks whole %u step %u "
              "rest %u; level-up found %u none %u, levelled %u; setups %u (bonus %u, drop window %u, sorted %u, past 16 "
              "%u); awards %u; party-set %u, sound banks %u, frames %u, lines at 99 or messages %u, y wrapped %u, "
              "zenny lines %u",
              steps, c.split_zero, c.split_shared, c.tick_whole, c.tick_step, c.tick_rest, c.levelup_found,
              c.levelup_none, c.logged[45], c.setup_ran, c.bonus, c.dropped_window, c.sorted_swaps, c.past16, c.awards,
              c.logged[51], c.logged[54], c.logged[47], c.max_level, c.wrapped, c.logged[58]);
    if (bad) bof3::Fatal("the battle result differs from the original in %u self-test rounds", bad);
}

}  // namespace battle_result

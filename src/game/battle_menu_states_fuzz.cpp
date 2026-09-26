// BOF3X_SHADOW=battle_menu_states: a differential fuzz of the battle menu
// states, once at start-up. docs/battle_menu_states.md section 4.
//
// Fifteen byte-copies, every call out re-aimed at a recording stand-in
// (bof3::CloneCall with `expected`); the four dispatch tables' 21 entries
// (0x64E44C..0x64E49C, contiguous) pointed at recorders, so that the
// dispatches - ours and the copies alike - reach a recorder, and an index
// past one table reaches the next one's recorder as the original's would.
// One round: one function, random bytes in every region any of them touches,
// the pointers and indices put back inside what they may hold, each branch's
// boundaries seeded; theirs, then from the same state ours; the regions, the
// two command records and the stand-ins' log compared. Everything is put back
// afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_menu_states_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_menu_states {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }
unsigned char* Member(unsigned i) { return At(at::kMembers + i * at::kMemberSize); }

constexpr std::uint32_t kPressed = 0x7E1BEC;   // Input_Pressed
constexpr std::uint32_t kConfirm = 0x90358E;   // Field_ConfirmButtons, then Field_CancelButtons
constexpr std::uint32_t kCancel = 0x903590;

// Two command records: 0x939FA0 points at one, and the stand-ins move it to
// the other, so that a pointer read before a call and used after shows.
constexpr unsigned kCmdSize = 0x20;
unsigned char g_cmd[2 * kCmdSize];

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

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
    const std::uint32_t at = Address(p);
    const std::uint32_t cmd = Address(g_cmd);
    if (at >= cmd && at < cmd + sizeof g_cmd) return 0x10000u + (at - cmd);
    return at;
}
unsigned char* CurrentCmd() { return move_script::At(static_cast<std::uint32_t>(Long(At(at::kCommandRecord)))); }

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside what they may point at, the category
// inside 0..3.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    unsigned char* const list = At(at::kList);
    switch ((h >> 4) % 20) {
    case 0: At(at::kState)[0] = static_cast<unsigned char>(v); break;
    case 1: At(at::kSubState)[0] = static_cast<unsigned char>(v); break;
    case 2: SetPtr(at::kCommandRecord, g_cmd + (v % 2) * kCmdSize); break;
    case 3: CurrentCmd()[0] = static_cast<unsigned char>(v); break;
    case 4: list[0xA] = static_cast<unsigned char>(v % 4); break;
    case 5: list[0xB] = static_cast<unsigned char>(v); break;
    case 6: list[0xC] = static_cast<unsigned char>(v); break;
    case 7: SetWord(At(kPressed), w); break;
    case 8: SetWord(At(v % 2 ? kConfirm : kCancel), w); break;
    case 9: At(at::kQueueWrite)[0] = static_cast<unsigned char>(v); break;
    case 10: At(v % 2 ? at::kPartyCount : at::kEnemyCount)[0] = static_cast<unsigned char>(w); break;
    case 11: if (v % 2) SetLong(list + 4, static_cast<std::int32_t>(h * 0x9E3779B1u)); else SetWord(list + 6, w); break;
    case 12: SetWord(list + 0x12, v % 3 == 0 ? w : 0); break;
    case 13: At(at::kCommandsChosen)[0] = static_cast<unsigned char>(v); break;
    case 14: SetPtr(at::kActing, Member(v % 3)); break;
    case 15: list[8] = static_cast<unsigned char>(v); break;
    case 16: list[3] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 17: At(at::kSubState + 1)[0] = static_cast<unsigned char>(v); break;
    case 18: SetWord(list + 0x10, w); break;
    default: {
        // the inventory byte at the cursor: an id or a count
        const unsigned cat = list[0xA] & 3;
        At((v % 2 ? 0x904354u : 0x904154u) + 0x80 * cat + list[0xC])[0] = static_cast<unsigned char>(w);
        break;
    }
    }
}

// --- the stand-ins ---------------------------------------------------------

// The stand-ins below that move a byte half the time move one their callers
// read again after the call: the command pointer after the target pickers,
// Battle_WrapIndex and Battle_ReturnTrue; the state after the banner and the
// set-up; the category and the sub-state after a cue.
void MoveCommand() { if (Hash() % 2) SetPtr(at::kCommandRecord, g_cmd + ((Hash() >> 8) % 2) * kCmdSize); }

// The 21 table entries.
template <unsigned N> void __cdecl StubHandler() { Record(N, At(at::kState)[0], At(at::kSubState)[0]); Disturb(); }

unsigned char __cdecl StubDefaultTarget(unsigned a) {
    Record(1, a, Id(CurrentCmd()));
    MoveCommand();
    Disturb();
    return static_cast<unsigned char>(Hash() >> 7);
}
unsigned char __cdecl StubPrevTarget(unsigned a) {
    Record(2, a, Id(CurrentCmd()));
    MoveCommand();
    Disturb();
    return static_cast<unsigned char>(Hash() >> 9);
}
long __cdecl StubWrap(long high, long low, long value) {
    Record(3, static_cast<std::uint32_t>(high), static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(value));
    MoveCommand();
    Disturb();
    const std::uint32_t h = Hash();
    return h % 2 ? static_cast<long>(value) : static_cast<long>(h);
}
unsigned long __cdecl StubBanner(unsigned msg, unsigned b2) {
    Record(4, msg, b2);
    if (Hash() % 2) At(at::kState)[0] = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
    return Hash();
}
// The auto-repeat's answer: one direction or button bit mostly, some pairs,
// nothing, and anything - the whole dword.
unsigned __cdecl StubAutoRepeat(unsigned pressed) {
    Record(5, pressed, Word(At(0x7E01B8)));
    static const std::uint32_t kBits[] = {0, 0, 0x1000, 0x2000, 0x4000, 0x8000, 4, 8, 0x5000, 0xA000, 0x3000,
                                          0x9000, 0x6000, 0x1004, 0x4008, 0xF00C, 0x10000, 0x80};
    Disturb();
    const std::uint32_t h = Hash();
    if (h % 8 == 0) return h;
    return kBits[(h >> 3) % (sizeof kBits / sizeof kBits[0])] | (h % 4 == 1 ? h & 0xFFFF0000u : 0);
}
void __cdecl StubSound(unsigned short id) {
    Record(6, id);
    const std::uint32_t h = Hash();
    if (h % 2) At(at::kList)[0xA] = static_cast<unsigned char>((h >> 8) % 4);
    if ((h >> 1) % 2) At(at::kSubState)[0] = static_cast<unsigned char>(h >> 16);
    Disturb();
}
const unsigned char* __cdecl StubSystemPtr(unsigned id) {
    Record(7, id);
    Disturb();
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(Hash()));
}
unsigned long __cdecl StubQueuePush(unsigned a, unsigned b, unsigned long value) {
    Record(8, a, b, static_cast<std::uint32_t>(value));
    Disturb();
    return Hash();
}
unsigned long __cdecl StubSetup() {
    Record(9);
    if (Hash() % 2) At(at::kState)[0] = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
    return Hash();
}
unsigned long __cdecl StubFree() { Record(10); Disturb(); return Hash(); }
// The three below read their arguments' low bytes (Item_HelpMessage and 0x591810 by
// `and eax, 0xFF`, Item_CanUse by its evidence), so they are recorded so.
unsigned __cdecl StubPrice(unsigned category, unsigned item) {
    Record(11, category & 0xFF, item & 0xFF);
    Disturb();
    return Hash();
}
unsigned char __cdecl StubCanUse(unsigned mode, unsigned member, unsigned category, unsigned item) {
    Record(12, mode & 0xFF, member & 0xFF, category & 0xFF, item & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h >> 8);
}
unsigned char __cdecl StubFlags(unsigned category, unsigned item) {
    Record(13, category & 0xFF, item & 0xFF);
    Disturb();
    return static_cast<unsigned char>(Hash() >> 5);
}
unsigned char __cdecl StubReturnTrue() {
    Record(14, Id(CurrentCmd()));
    MoveCommand();
    if (Hash() % 4 == 1) At(at::kPartyCount)[0] = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h >> 8);
}

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x445730: return f(&StubDefaultTarget);
    case 0x4457F0: return f(&StubPrevTarget);
    case 0x4469F0: return f(&StubWrap);
    case 0x44A8E0: return f(&StubBanner);
    case 0x461EB0: return f(&StubAutoRepeat);
    case 0x587740: return f(&StubSound);
    case 0x497740: return f(&StubSystemPtr);
    case 0x44A880: return f(&StubQueuePush);
    case 0x449E10: return f(&StubSetup);
    case 0x449FE0: return f(&StubFree);
    case 0x591C20: return f(&StubPrice);
    case 0x57D9A0: return f(&StubCanUse);
    case 0x591810: return f(&StubFlags);
    case 0x449E00: return f(&StubReturnTrue);
    default: bof3::Fatal("battle_menu_states: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

const Callees kStubs = {
    StubDefaultTarget, StubPrevTarget, StubWrap, StubBanner, StubAutoRepeat, StubSound, StubSystemPtr,
    StubQueuePush, StubSetup, StubFree, StubPrice, StubCanUse, StubFlags, StubReturnTrue,
};

// The four tables, contiguous: 4 + 10 + 2 + 5 entries from 0x64E44C.
constexpr std::uint32_t kTables = at::kAttackStates;
constexpr unsigned kTableEntries = 21;
const Handler kTableStubs[kTableEntries] = {
    &StubHandler<100>, &StubHandler<101>, &StubHandler<102>, &StubHandler<103>, &StubHandler<104>,
    &StubHandler<105>, &StubHandler<106>, &StubHandler<107>, &StubHandler<108>, &StubHandler<109>,
    &StubHandler<110>, &StubHandler<111>, &StubHandler<112>, &StubHandler<113>, &StubHandler<114>,
    &StubHandler<115>, &StubHandler<116>, &StubHandler<117>, &StubHandler<118>, &StubHandler<119>,
    &StubHandler<120>,
};
// What the tables hold (2026-09-25), checked before they are swapped.
constexpr std::uint32_t kTableHolds[kTableEntries] = {
    0x447FE0, 0x448020, 0x4480E0, 0x448140,                                                      // 0x64E44C
    0x448190, 0x448210, 0x448600, 0x448630, 0x4486C0, 0x448B80, 0x448C80, 0x4493E0, 0x449480, 0x4498F0,  // 0x64E45C
    0x4481B0, 0x4481E0,                                                                          // 0x64E484
    0x4486E0, 0x448780, 0x4488C0, 0x448A70, 0x448B40,                                            // 0x64E48C
};

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
};

// The call sites (capstone, 2026-09-25): the offset of each E8 in the body.
constexpr Call kBeginCalls[] = {{0x02, 0x445730}, {0x13, 0x44A8E0}};
constexpr Call kAtkPickCalls[] = {{0x2F, 0x461EB0}, {0x59, 0x4469F0}, {0x5F, 0x445730}, {0x71, 0x587740},
                                  {0x9B, 0x4469F0}, {0xA1, 0x4457F0}, {0xB3, 0x587740}};
constexpr Call kConfirmCalls[] = {{0x06, 0x587740}};
constexpr Call kPromptCalls[] = {{0x05, 0x497740}, {0x12, 0x44A880}};
constexpr Call kOpenCalls[] = {{0x09, 0x449E10}};
constexpr Call kListCalls[] = {{0x7D, 0x461EB0}, {0x91, 0x587740}, {0xCC, 0x587740}, {0x1E7, 0x587740},
                               {0x220, 0x591C20}, {0x226, 0x497740}, {0x254, 0x587740}, {0x30A, 0x587740},
                               {0x37D, 0x57D9A0}, {0x38E, 0x587740}, {0x3C1, 0x587740}};
constexpr Call kKindCalls[] = {{0x21, 0x591810}};
constexpr Call kTBeginCalls[] = {{0x21, 0x591810}, {0x2F, 0x445730}, {0x55, 0x449E00}, {0x6B, 0x445730}};
constexpr Call kItemPickCalls[] = {{0x31, 0x461EB0}, {0x64, 0x591810}, {0x70, 0x449E00}, {0x85, 0x445730},
                                   {0xA8, 0x587740}, {0xD3, 0x4469F0}, {0xD9, 0x445730}, {0xEB, 0x587740},
                                   {0x113, 0x4469F0}, {0x119, 0x4457F0}, {0x12B, 0x587740}};
constexpr Call kMemberCalls[] = {{0x31, 0x461EB0}, {0x64, 0x591810}, {0x72, 0x445730}, {0x92, 0x587740},
                                 {0xA4, 0x449E00}, {0xC8, 0x4469F0}, {0xF4, 0x4469F0}, {0xFA, 0x445730},
                                 {0x10F, 0x587740}, {0x123, 0x449E00}, {0x147, 0x4469F0}, {0x15C, 0x587740},
                                 {0x180, 0x4469F0}, {0x186, 0x4457F0}, {0x19B, 0x587740}};
constexpr Call kCommitCalls[] = {{0x07, 0x587740}, {0xAC, 0x449FE0}};

enum : unsigned {
    kAtkDispatch, kAtkBegin, kAtkPick, kAtkConfirm, kItemDispatch, kOpenDispatch, kPrompt, kOpenList, kList,
    kTargetKind, kTargetDispatch, kTargetBegin, kItemPick, kMemberPick, kCommit, kCount
};

#define BM_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
#define BM_P(name, base, size) {name, base, size, nullptr, 0}
const Clone kClones[kCount] = {
    BM_P("BattleAttackCmd_Dispatch", 0x447FD0, 0x0E),
    BM_C("BattleAttackCmd_Begin", 0x447FE0, 0x38, kBeginCalls),
    BM_C("BattleAttackCmd_PickEnemy", 0x448020, 0xBC, kAtkPickCalls),
    BM_C("BattleAttackCmd_Confirm", 0x4480E0, 0x59, kConfirmCalls),
    BM_P("BattleItemCmd_Dispatch", 0x448180, 0x0E),
    BM_P("BattleItemCmd_OpenDispatch", 0x448190, 0x11),
    BM_C("BattleItemCmd_QueuePrompt", 0x4481B0, 0x27, kPromptCalls),
    BM_C("BattleItemCmd_OpenList", 0x4481E0, 0x22, kOpenCalls),
    BM_C("BattleItemCmd_List", 0x448210, 0x3E2, kListCalls),
    BM_C("BattleItemCmd_TargetKind", 0x448630, 0x85, kKindCalls),
    BM_P("BattleItemCmd_TargetDispatch", 0x4486C0, 0x11),
    BM_C("BattleItemCmd_TargetBegin", 0x4486E0, 0x93, kTBeginCalls),
    BM_C("BattleItemCmd_PickEnemy", 0x448780, 0x135, kItemPickCalls),
    BM_C("BattleItemCmd_PickMember", 0x4488C0, 0x1A5, kMemberCalls),
    BM_C("BattleItemCmd_Commit", 0x448A70, 0xCD, kCommitCalls),
};
#undef BM_C
#undef BM_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x904AA0, 0x30},            // the battle's step bytes .. 0x904AC3
    {0x904154, 0x50C},           // the inventory's ids and counts, the saved list bytes 0x904605..7
    {0x8031A0, 0x2E0},           // window records 2..21
    {0x93C2A0, 0xA0},            // the message queue and its indices
    {at::kMembers, 0x3E4},       // ObjTrio
    {at::kCommandRecord, 4},
    {at::kActing, 4},
    {at::kRepeatLatch, 2},
    {kPressed, 2},
    {kConfirm, 4},               // Field_ConfirmButtons, Field_CancelButtons
};
constexpr unsigned kRegionBytes = 0x30 + 0x50C + 0x2E0 + 0xA0 + 0x3E4 + 4 + 4 + 2 + 2 + 4;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char cmd[sizeof g_cmd];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.cmd, g_cmd, sizeof g_cmd);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_cmd, s.cmd, sizeof g_cmd);
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

std::uint32_t g_rng = 0x5BD1E995u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

// Random bytes put back inside what they may hold: the two pointers, the
// members' actor bytes, the category.
void Fix() {
    SetPtr(at::kCommandRecord, g_cmd + (Next() % 2) * kCmdSize);
    SetPtr(at::kActing, Member(Next() % 3));
    for (unsigned i = 0; i < 3; ++i) Member(i)[5] = static_cast<unsigned char>(Next() % 3);
    At(at::kList)[0xA] = static_cast<unsigned char>(Next() % 4);
}

// The pad: most rounds neither the confirm nor the cancel word shares a bit
// with the pressed word (so the directions run); the rest one, the other or
// both.
void SeedPad() {
    const unsigned pressed = Word(At(kPressed));
    switch (Next() % 6) {
    case 0: SetWord(At(kCancel), pressed & (Next() | 1)); if (!(Word(At(kCancel)) & pressed)) SetWord(At(kCancel), pressed); break;
    case 1: SetWord(At(kCancel), Word(At(kCancel)) & ~pressed); SetWord(At(kConfirm), pressed | 1); SetWord(At(kPressed), pressed | 1); break;
    default:
        SetWord(At(kCancel), Word(At(kCancel)) & ~pressed);
        SetWord(At(kConfirm), Word(At(kConfirm)) & ~pressed);
        break;
    }
}

// The target byte at its wraps' edges.
unsigned char EdgeTarget() {
    static const unsigned char kTargets[] = {0, 1, 2, 3, 4, 5, 9, 10, 0x7F, 0x80, 0xFE, 0xFF};
    return Often() ? kTargets[Next() % (sizeof kTargets / sizeof kTargets[0])] : static_cast<unsigned char>(Next());
}

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    unsigned char* const list = At(at::kList);
    switch (k) {
    // the other step byte inside the tables too, so that an index read from
    // the wrong byte shows as a count, not a crash
    case kAtkDispatch:
        At(at::kState)[0] = static_cast<unsigned char>(Often() ? Next() % 4 : Next() % kTableEntries);
        At(at::kSubState)[0] = static_cast<unsigned char>(Next() % 4);
        break;
    case kItemDispatch:
        At(at::kState)[0] = static_cast<unsigned char>(Often() ? Next() % 10 : Next() % (kTableEntries - 4));
        At(at::kSubState)[0] = static_cast<unsigned char>(Next() % 5);
        break;
    case kOpenDispatch:
        At(at::kSubState)[0] = static_cast<unsigned char>(Often() ? Next() % 2 : Next() % (kTableEntries - 14));
        At(at::kState)[0] = static_cast<unsigned char>(Next() % 7);
        break;
    case kTargetDispatch:
        At(at::kSubState)[0] = static_cast<unsigned char>(Next() % 5);
        At(at::kState)[0] = static_cast<unsigned char>(Next() % 5);
        break;
    case kAtkPick:
    case kItemPick:
    case kMemberPick:
        SeedPad();
        CurrentCmd()[0] = EdgeTarget();
        At(at::kEnemyCount)[0] = static_cast<unsigned char>(Often() ? Next() % 6 : Next());
        At(at::kPartyCount)[0] = static_cast<unsigned char>(Often() ? Next() % 4 : Next());
        break;
    case kList: {
        if (Next() % 8 != 0) list[3] = 0;
        // the pad: a third each none, a cancel, a confirm (the list's last
        // tests run only with no scroll request pending)
        const unsigned pressed = Word(At(kPressed)) | 1;
        SetWord(At(kPressed), pressed);
        SetWord(At(kCancel), Word(At(kCancel)) & ~pressed);
        SetWord(At(kConfirm), Word(At(kConfirm)) & ~pressed);
        switch (Next() % 3) {
        case 0: SetWord(At(kCancel), pressed & (Next() | 1)); break;
        case 1: SetWord(At(kConfirm), pressed & (Next() | 1)); if (Half()) SetWord(At(kCancel), 0); break;
        default: break;
        }
        static const unsigned char kTops[] = {0, 0, 1, 6, 7, 8, 0x72, 0x73, 0x78, 0x79, 0x7A, 0xFF};
        const unsigned char top = kTops[Next() % (sizeof kTops / sizeof kTops[0])];
        list[0xB] = top;
        static const int kRows[] = {0, 0, 1, 2, 3, 5, 6, -1, 7, 8};
        switch (Next() % 6) {
        case 0: list[0xC] = static_cast<unsigned char>(Half() ? 0 : 1); if (Half()) list[0xB] = 0; break;
        case 1: list[0xC] = static_cast<unsigned char>(0x7E + Next() % 3); break;
        case 2: break;
        default: list[0xC] = static_cast<unsigned char>(top + kRows[Next() % 10]); break;
        }
        if (Next() % 8 != 0) SetWord(list + 0x12, 0);
        if (Half()) list[0xA] = static_cast<unsigned char>(Half() ? 0 : 3);
        break;
    }
    case kCommit: {
        static const unsigned char kCounts[] = {0, 1, 2, 3, 0xFF};
        const unsigned cat = list[0xA];
        At(0x904354u + 0x80 * cat + list[0xC])[0] = kCounts[Next() % 5];
        break;
    }
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[128];
    unsigned sounds[8];   // 0x100, 0x101, 0x103, 0x104, 0x106, 0x107, other
    unsigned list_above, list_scrolled, list_refused, list_confirmed, list_cancelled;
    unsigned spent, cleared, crossed;
    unsigned kinds[6];    // TargetKind: state 4 / 5 by 0x40, all 0xC0, 0x40, 0x80, self
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        const Entry& e = out.log[i];
        if (e.what < 128) ++g_cover.logged[e.what];
        if (e.what == 6) {
            static const unsigned kIds[] = {0x100, 0x101, 0x103, 0x104, 0x106, 0x107};
            unsigned s = 6;
            for (unsigned j = 0; j < 6; ++j) if (e.a == kIds[j]) s = j;
            ++g_cover.sounds[s];
        }
    }
    const unsigned state_out = ByteOf(out, at::kState);
    switch (k) {
    case kList: {
        bool refused = false, confirmed = false, cancelled = false;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
            if (out.log[i].what == 6 && out.log[i].a == 0x107) refused = true;
            if (out.log[i].what == 6 && out.log[i].a == 0x103) confirmed = true;
            if (out.log[i].what == 6 && out.log[i].a == 0x106) cancelled = true;
        }
        if (state_out == 6 && ByteOf(in, at::kState) != 6) ++g_cover.list_above;
        if (ByteOf(out, at::kList + 0x12) | ByteOf(out, at::kList + 0x13)) ++g_cover.list_scrolled;
        g_cover.list_refused += refused;
        g_cover.list_confirmed += confirmed;
        g_cover.list_cancelled += cancelled;
        break;
    }
    case kCommit: {
        const unsigned cat = ByteOf(in, at::kList + 0xA), row = ByteOf(in, at::kList + 0xC);
        const std::uint32_t count = 0x904354u + 0x80 * cat + row;
        if (ByteOf(out, count) == 0 && ByteOf(out, 0x904154u + 0x80 * cat + row) == 0) ++g_cover.cleared;
        else ++g_cover.spent;
        break;
    }
    case kTargetKind: {
        const unsigned sub = ByteOf(out, at::kSubState);
        if (state_out == 4 && sub != 3) ++g_cover.kinds[0];
        else if (state_out == 5) ++g_cover.kinds[1];
        else ++g_cover.kinds[2];
        break;
    }
    case kItemPick:
    case kMemberPick: {
        const unsigned sub_in = ByteOf(in, at::kSubState), sub_out = ByteOf(out, at::kSubState);
        if (sub_in != sub_out) ++g_cover.crossed;
        break;
    }
    default:
        break;
    }
}

using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_menu_states: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    for (unsigned i = 0; i < kTableEntries; ++i) {
        const auto had = static_cast<std::uint32_t>(Long(At(kTables + 4 * i)));
        if (had != kTableHolds[i])
            bof3::Fatal("battle_menu_states: table entry 0x%X holds 0x%X, not 0x%X", static_cast<unsigned>(kTables + 4 * i),
                        static_cast<unsigned>(had), static_cast<unsigned>(kTableHolds[i]));
    }

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("battle_menu_states: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&BattleAttackCmd_Dispatch), reinterpret_cast<const void*>(&BattleAttackCmd_Begin),
        reinterpret_cast<const void*>(&BattleAttackCmd_PickEnemy), reinterpret_cast<const void*>(&BattleAttackCmd_Confirm),
        reinterpret_cast<const void*>(&BattleItemCmd_Dispatch), reinterpret_cast<const void*>(&BattleItemCmd_OpenDispatch),
        reinterpret_cast<const void*>(&BattleItemCmd_QueuePrompt), reinterpret_cast<const void*>(&BattleItemCmd_OpenList),
        reinterpret_cast<const void*>(&BattleItemCmd_List), reinterpret_cast<const void*>(&BattleItemCmd_TargetKind),
        reinterpret_cast<const void*>(&BattleItemCmd_TargetDispatch), reinterpret_cast<const void*>(&BattleItemCmd_TargetBegin),
        reinterpret_cast<const void*>(&BattleItemCmd_PickEnemy), reinterpret_cast<const void*>(&BattleItemCmd_PickMember),
        reinterpret_cast<const void*>(&BattleItemCmd_Commit)};

    static State saved, input, their_out, our_out;
    std::uint32_t saved_tables[kTableEntries];
    std::memcpy(saved_tables, At(kTables), sizeof saved_tables);
    Capture(saved);
    g = kStubs;
    for (unsigned i = 0; i < kTableEntries; ++i) SetPtr(kTables + 4 * i, reinterpret_cast<const void*>(kTableStubs[i]));

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.cmd) b = static_cast<unsigned char>(Next());
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
            reinterpret_cast<Fn4>(const_cast<void*>(fn))(Next(), Next(), Next(), Next());
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
                bof3::Log("shadow      battle_menu_states self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    std::memcpy(At(kTables), saved_tables, sizeof saved_tables);
    Apply(saved);

    bof3::Log("shadow      battle_menu_states self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the step bytes, the inventory, the window records, the message queue, the party records, "
              "the pointers, the pad words, the two command records and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_menu_states: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned entries = 0;
    for (unsigned i = 100; i < 100 + kTableEntries; ++i) entries += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      battle_menu_states coverage: table entries %u of %u; default targets %u, previous %u, wraps %u, "
              "banners %u, repeats %u, prompts %u, set-ups %u, frees %u, help lines %u, use asks %u, flags %u, return-trues %u; "
              "cues 100 %u 101 %u 103 %u 104 %u 106 %u 107 %u other %u; list: above %u, scroll %u, refused %u, confirmed %u, "
              "cancelled %u; kinds: by 0x40 %u, state 5 %u, the rest %u; picks crossed or left %u; commit spent %u, cleared %u",
              entries, kTableEntries, c.logged[1], c.logged[2], c.logged[3], c.logged[4], c.logged[5], c.logged[8],
              c.logged[9], c.logged[10], c.logged[11], c.logged[12], c.logged[13], c.logged[14], c.sounds[0], c.sounds[1],
              c.sounds[2], c.sounds[3], c.sounds[4], c.sounds[5], c.sounds[6], c.list_above, c.list_scrolled,
              c.list_refused, c.list_confirmed, c.list_cancelled, c.kinds[0], c.kinds[1], c.kinds[2], c.crossed, c.spent,
              c.cleared);
    if (bad) bof3::Fatal("the battle menu states differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_menu_states

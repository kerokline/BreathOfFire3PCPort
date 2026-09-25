// BOF3X_SHADOW=battle_phases: a differential fuzz of the battle's frame and
// the phase table's first half, once at start-up. docs/battle_phases.md
// section 4.
//
// Eighteen byte-copies, every call and tail jump out re-aimed at a recording
// stand-in (bof3::CloneCall with `expected`); the two tables the originals
// build on their stacks re-aimed inside the copies (their immediates checked
// first); the three .data tables Battle_InputSteps, Battle_MenuSteps and
// Battle_CommitSteps and the event hook 0x904B6C pointed at recorders. One
// round: one function, random bytes in every region any of them touches, the
// pointers and indices put back inside what they index, each branch's
// boundaries seeded; theirs, then from the same state ours; the regions and
// the stand-ins' log compared. Everything is put back afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_phases.h"
#include "game/battle_phases_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_phases {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char& B(std::uint32_t address) { return *At(address); }
unsigned char* Member(unsigned i) { return At(at::kMembers + i * at::kMemberSize); }
unsigned char* EnemyObj(unsigned i) { return At(at::kEnemies + i * at::kEnemySize); }
unsigned char* Window(unsigned i) { return At(at::kWindows + i * at::kWindowSize); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }
unsigned char* MenuActor() { return At(static_cast<std::uint32_t>(Long(At(at::kMenuActor)))); }

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 160;
struct Entry { std::uint32_t what, a, b, c; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c};
    ++g_log_n;
}

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the arrays, indices inside the tables:
// the menu index 1..3 (the cancel path indexes the order by it less one,
// after a call), the sub-step 0..7 (the confirm dispatch's index, after a
// call).
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    unsigned char* const m = Member(w % 3);
    switch ((h >> 4) % 22) {
    case 0: B(at::kEventBattle) = static_cast<unsigned char>(v % 2 ? 0 : v % 3 ? 0x25 : v); break;
    case 1: {
        static const unsigned char kBits[] = {2, 8, 0x10};
        B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) ^ kBits[v % 3]);
        break;
    }
    case 2: B(at::kActorCount) = static_cast<unsigned char>(v % 6); break;
    case 3: B(at::kInitiative) = static_cast<unsigned char>(v % 4); break;
    case 4: B(at::kPhase + w % 3) = static_cast<unsigned char>(v % 8); break;
    case 5: B(at::kMenuIndex) = static_cast<unsigned char>(1 + v % 3); break;
    case 6: B(at::kCommand) = static_cast<unsigned char>(v % 8); break;
    case 7: B(at::kTapTimer) = static_cast<unsigned char>(v % 3 == 0 ? 0 : v % 9); B(at::kTapCommand) = static_cast<unsigned char>(w % 8); break;
    case 8: SetPtr(at::kMenuActor, Member(v % 3)); break;
    case 9: SetPtr(at::kMenuRecord, Member(v % 3) + 0x124); break;
    case 10:
        switch (v % 7) {
        case 0: m[0] = static_cast<unsigned char>(m[0] ^ 1); break;
        case 1: m[0x89] = static_cast<unsigned char>(w); break;
        case 2: m[8] = static_cast<unsigned char>(w); break;
        case 3: m[0x125] = static_cast<unsigned char>(w % 2 ? 5 : w); break;
        case 4: SetWord(m + 0x126, h >> 16); break;
        case 5: m[0x12E] = static_cast<unsigned char>(w); break;
        default: SetLong(m + 0x130, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(m + 0x130)) ^ 0x4000u)); break;
        }
        break;
    case 11: SetWord(At(at::kInputPressed), (h >> 16) & 0xF10C); break;
    case 12: SetWord(At(at::kInputHeld), (h >> 16) & 0xF10C); break;
    case 13: SetWord(At(at::kWindows + (w % 16) * at::kWindowSize + 4 + 2 * (v % 2)), h >> 16); break;
    case 14: Window(1 + w % 4)[3] = static_cast<unsigned char>(v % 3); break;
    case 15: B(at::kMessageCount) = static_cast<unsigned char>(v % 9); break;
    case 16: SetWord(At(at::kWaitFrames), v % 3); break;
    case 17: Sprite_Current = v % 2 ? Member(w % 3) : EnemyObj(w % 8); break;
    case 18: B(at::kFacing) = static_cast<unsigned char>(v); break;
    case 19: B(at::kAE9) = static_cast<unsigned char>(B(at::kAE9) ^ 2); B(at::kWindows) = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 20: B(at::kActorsIn) = static_cast<unsigned char>(v); break;
    default: {
        if (v % 2) m[0x90] = static_cast<unsigned char>(m[0x90] ^ 0x20);
        else SetLong(m + 0x134, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(m + 0x134)) ^ (v % 4 == 1 ? 2u : 0x4000u)));
        SetWord(At(at::kBattleMusic), h >> 16);
        break;
    }
    }
}

// --- the stand-ins ---------------------------------------------------------

// The void(void) callees, by a number of their own; the six stack-table
// handlers (100..), the .data tables' sixteen (120..).
template <unsigned N> void __cdecl StubV() { Record(N, Address(Sprite_Current)); Disturb(); }
unsigned __cdecl StubRunStates() { Record(2); Disturb(); return Hash(); }
unsigned long __cdecl StubBannerClearAll() { Record(20); Disturb(); return Hash(); }
unsigned char __cdecl StubActorIsOut(unsigned a) {
    Record(21, a & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 9);
}
void __cdecl StubClutRow(unsigned row) { Record(26, row); Disturb(); }
void __cdecl StubSetClutStp() { Record(27, Address(Sprite_Current)); Disturb(); }
void __cdecl StubLoadDat(int file) { Record(28, static_cast<std::uint32_t>(file)); Disturb(); }
// The event hook moves the step: the original increments it after the call.
void __cdecl StubEventHook(int n) {
    Record(29, static_cast<std::uint32_t>(n));
    if (Hash() % 2) B(at::kStep) = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}
// Mostly a record 0..15, now and then 0xFF (taken) - the upper bytes
// anything, which a caller that did not take the low byte would use.
unsigned __cdecl StubWindowAlloc(unsigned slot, unsigned kind) {
    Record(30, slot & 0xFF, kind & 0xFF);
    const unsigned n = g_log_n;
    Disturb();
    const std::uint32_t h = Hash();
    const unsigned r = (h & 0xFFFFFF00u) | (h % 5 == 0 ? 0xFFu : (h >> 8) % 16);
    if (n <= kLog) g_log[n - 1].c = r & 0xFF;
    return r;
}
const unsigned char* __cdecl StubMsgPtr(unsigned id) {
    Record(31, id & 0xFFFF);
    Disturb();
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(Hash() | 1));
}
unsigned long __cdecl StubQueuePush(unsigned a, unsigned b, unsigned long value) {
    Record(32, a, b, static_cast<std::uint32_t>(value));
    Disturb();
    return Hash();
}
void __cdecl StubOpenStatus(unsigned s) { Record(33, s); Disturb(); }
void __cdecl StubOpenSub1(unsigned s) { Record(35, s); Disturb(); }
unsigned char __cdecl StubQueuePending() { Record(36); Disturb(); return static_cast<unsigned char>(Hash() % 2 ? 0 : Hash() | 1); }
// 0 a third of the time; 0x100 now and then (a byte test would miss it).
int __cdecl StubLoadDone() {
    Record(37);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 3 == 0 ? 0 : h % 7 == 0 ? 0x100 : static_cast<int>(h | 1);
}
void __cdecl StubOpenSub2(unsigned s) { Record(38, s); Disturb(); }
void __cdecl StubOpenSub3(unsigned s) { Record(39, s); Disturb(); }
unsigned long __cdecl StubSpawnCopies() { Record(43); Disturb(); return Hash(); }
unsigned long __cdecl StubShowName(const unsigned char* actor) { Record(44, Address(actor)); Disturb(); return Hash(); }
void __cdecl StubSound(unsigned short id) { Record(45, id); Disturb(); }
// Battle_ReturnItem reads the slot's byte and the item's word.
unsigned char __cdecl StubReturnItem(unsigned slot, unsigned item) {
    Record(46, slot & 0xFF, item & 0xFFFF);
    Disturb();
    return static_cast<unsigned char>(Hash());
}
// The draws read the words of x and y (docs/battle_phases.md section 4).
void __cdecl StubDrawCross(int x, int y) {
    Record(47, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF);
    Disturb();
}
void __cdecl StubDrawLabel(unsigned k) { Record(48, k & 0xFF); Disturb(); }
void __cdecl StubDrawStatus(int x, int y) {
    Record(49, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF);
    Disturb();
}

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x42E400: return f(&StubV<1>);
    case 0x441100: return f(&StubRunStates);
    case 0x435830: return f(&StubV<3>);
    case 0x44A5C0: return f(&StubV<4>);
    case 0x56E6C0: return f(&StubV<5>);
    case 0x435110: return f(&StubV<6>);
    case 0x441140: return f(&StubV<7>);
    case 0x4358A0: return f(&StubV<8>);
    case 0x5173E0: return f(&StubV<9>);
    case 0x517440: return f(&StubV<10>);
    case 0x57B780: return f(&StubV<11>);
    case 0x494030: return f(&StubV<12>);
    case 0x455250: return f(&StubV<13>);
    case 0x454AD0: return f(&StubV<14>);
    case 0x59E230: return f(&StubV<15>);
    case 0x592F00: return f(&StubV<16>);
    case 0x44A810: return f(&StubBannerClearAll);
    case 0x4456C0: return f(&StubActorIsOut);
    case 0x494280: return f(&StubV<22>);
    case 0x435260: return f(&StubV<23>);
    case 0x446C30: return f(&StubV<24>);
    case 0x453C00: return f(&StubV<25>);
    case 0x4549F0: return f(&StubClutRow);
    case 0x4551A0: return f(&StubSetClutStp);
    case 0x454590: return f(&StubLoadDat);
    case 0x59E2D0: return f(&StubWindowAlloc);
    case 0x497740: return f(&StubMsgPtr);
    case 0x44A880: return f(&StubQueuePush);
    case 0x444230: return f(&StubOpenStatus);
    case 0x494A80: return f(&StubV<34>);
    case 0x444290: return f(&StubOpenSub1);
    case 0x44A8C0: return f(&StubQueuePending);
    case 0x454810: return f(&StubLoadDone);
    case 0x4442C0: return f(&StubOpenSub2);
    case 0x4442E0: return f(&StubOpenSub3);
    case 0x445680: return f(&StubV<40>);
    case 0x444F40: return f(&StubV<41>);
    case 0x446720: return f(&StubV<42>);
    case 0x446FF0: return f(&StubSpawnCopies);
    case 0x44A990: return f(&StubShowName);
    case 0x587740: return f(&StubSound);
    case 0x446D90: return f(&StubReturnItem);
    case 0x4432F0: return f(&StubDrawCross);
    case 0x4439A0: return f(&StubDrawLabel);
    case 0x442FA0: return f(&StubDrawStatus);
    case 0x446A10: return f(&StubV<50>);
    case 0x4450E0: return f(&StubV<51>);
    case 0x44AE90: return f(&StubV<52>);
    default: bof3::Fatal("battle_phases: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

const Callees kStubs = {
    &StubV<1>, &StubRunStates, &StubV<3>, &StubV<4>, &StubV<5>, &StubV<6>, &StubV<7>, &StubV<8>, &StubV<9>,
    &StubV<10>, &StubV<11>, &StubV<12>, &StubV<13>, &StubV<14>, &StubV<15>, &StubV<16>,
    {&StubV<100>, &StubV<101>},
    {&StubV<110>, &StubV<111>, &StubV<112>, &StubV<113>},
    StubBannerClearAll, StubActorIsOut, &StubV<22>, &StubV<23>, &StubV<24>, &StubV<25>, StubClutRow,
    StubSetClutStp, StubLoadDat,
    StubWindowAlloc, StubMsgPtr, StubQueuePush, StubOpenStatus, &StubV<34>, StubOpenSub1, StubQueuePending,
    StubLoadDone, StubOpenSub2, StubOpenSub3,
    &StubV<40>, &StubV<41>, &StubV<42>, StubSpawnCopies, StubShowName, StubSound, StubReturnItem,
    StubDrawCross, StubDrawLabel, StubDrawStatus, &StubV<50>,
    &StubV<51>, &StubV<52>,
};

// The three .data tables' entries while the fuzz runs.
const Handler kInputStubs[at::kInputStepCount] = {&StubV<120>, &StubV<121>, &StubV<122>, &StubV<123>, &StubV<124>};
const Handler kMenuStubs[at::kMenuStepCount] = {&StubV<130>, &StubV<131>, &StubV<132>, &StubV<133>,
                                                &StubV<134>, &StubV<135>, &StubV<136>, &StubV<137>};
const Handler kCommitStubs[at::kCommitStepCount] = {&StubV<140>, &StubV<141>, &StubV<142>};

// The stack-built tables: the offset of each imm32 in the copy (capstone,
// 2026-09-25) and the handler it names.
struct Imm { std::uint32_t offset, value; };
constexpr Imm kStartImm[2] = {{0x09, 0x42E4A0}, {0x16, 0x42E730}};
constexpr Imm kIntroImm[4] = {{0x09, 0x42E770}, {0x16, 0x42E8C0}, {0x1E, 0x42E8D0}, {0x26, 0x42E930}};

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
};

constexpr Call kFrameCalls[] = {{0x29, 0x42E400}, {0x2E, 0x441100}, {0x33, 0x435830}, {0x38, 0x44A5C0},
                                {0x3D, 0x56E6C0}, {0x42, 0x435110}, {0x47, 0x441140}, {0x4C, 0x4358A0},
                                {0x5A, 0x5173E0}, {0x61, 0x517440}, {0x66, 0x57B780}, {0x6B, 0x494030},
                                {0x70, 0x455250}, {0x75, 0x454AD0}, {0x7A, 0x59E230}, {0x7F, 0x592F00}};
constexpr Call kInitCalls[] = {{0x02, 0x44A810}, {0x21, 0x4456C0}, {0xE0, 0x494280}, {0xE5, 0x435260},
                               {0xEA, 0x446C30}, {0x16E, 0x453C00}, {0x1B6, 0x4549F0}, {0x1BD, 0x4549F0},
                               {0x1F1, 0x4456C0}, {0x20C, 0x4551A0}, {0x246, 0x454590}};
constexpr Call kOpenCalls[] = {{0x13, 0x59E2D0}, {0x59, 0x497740}, {0x63, 0x44A880}, {0x76, 0x497740},
                               {0x80, 0x44A880}, {0xA7, 0x59E2D0}, {0x127, 0x444230}, {0x12F, 0x494A80}};
constexpr Call kCrossCalls[] = {{0x02, 0x444290}};
constexpr Call kFinishCalls[] = {{0x12, 0x44A8C0}, {0x1B, 0x454810}, {0x33, 0x4442C0}, {0x3D, 0x4442E0}};
constexpr Call kRoundCalls[] = {{0x00, 0x445680}, {0x05, 0x444F40}, {0x1A, 0x446720}, {0x27, 0x446FF0}};
constexpr Call kNextCalls[] = {{0x01, 0x454810}, {0xC2, 0x44A990}};
constexpr Call kSelectCalls[] = {{0x39, 0x587740}, {0xBD, 0x446D90}, {0x116, 0x587740}, {0x127, 0x587740},
                                 {0x169, 0x587740}, {0x192, 0x4432F0}, {0x19E, 0x4439A0}, {0x1B2, 0x442FA0},
                                 {0x1F6, 0x587740}};
constexpr Call kConfirmCalls[] = {{0x00, 0x446A10}};
constexpr Call kCommitCalls[] = {{0x01, 0x4450E0}, {0x3C, 0x4456C0}, {0x63, 0x446D90}, {0x8B, 0x44AE90},
                                 {0xA0, 0x454590}};
constexpr Call kMessageCalls[] = {{0x6B, 0x497740}, {0x75, 0x44A880}};
constexpr Call kWaitCalls[] = {{0x00, 0x454810}};

enum : unsigned {
    kFrame, kStart, kInit, kIntro, kOpenWindows, kWaitWindow, kPrepareCross, kFinish, kInput, kRoundStart,
    kNextMember, kCommandSelect, kConfirmDispatch, kDefend, kCommit, kCommitRound, kQueueMessages, kWaitLoad,
    kCount
};

#define BP_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
#define BP_P(name, base, size) {name, base, size, nullptr, 0}
const Clone kClones[kCount] = {
    BP_C("Battle_Frame", 0x42E370, 0x84, kFrameCalls),
    BP_P("BattleStart_Dispatch", 0x42E470, 0x22),
    BP_C("Battle_Init", 0x42E4A0, 0x285, kInitCalls),
    BP_P("BattleIntro_Dispatch", 0x42E730, 0x32),
    BP_C("BattleIntro_OpenWindows", 0x42E770, 0x145, kOpenCalls),
    BP_P("BattleIntro_WaitWindow", 0x42E8C0, 0x10),
    BP_C("BattleIntro_PrepareCross", 0x42E8D0, 0x5F, kCrossCalls),
    BP_C("BattleIntro_Finish", 0x42E930, 0x5E, kFinishCalls),
    BP_P("BattleInput_Dispatch", 0x42E990, 0xE),
    BP_C("Battle_RoundStart", 0x42E9A0, 0x33, kRoundCalls),
    BP_C("BattleInput_NextMember", 0x42E9E0, 0xE9, kNextCalls),
    BP_C("BattleMenu_CommandSelect", 0x42EAD0, 0x2BB, kSelectCalls),
    BP_C("BattleMenu_ConfirmDispatch", 0x42EED0, 0x13, kConfirmCalls),
    BP_P("Cmd_ConfirmDefend", 0x42EEF0, 0x53),
    BP_P("BattleCommit_Dispatch", 0x42F070, 0xE),
    BP_C("Battle_CommitRound", 0x42F080, 0xB0, kCommitCalls),
    BP_C("BattleCommit_QueueMessages", 0x42F130, 0x92, kMessageCalls),
    BP_C("BattleCommit_WaitLoad", 0x42F1D0, 0x44, kWaitCalls),
};
#undef BP_C
#undef BP_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kMembers, 0x3E4},                                      // ObjTrio
    {at::kWindows, 0x240},                                      // windows 0..15
    {at::kWindows + 0xFF * at::kWindowSize, 8},                 // window 0xFF's, which Window_Alloc's 0xFF reaches
    {at::kClutFrom, 0x20},
    {at::kClutTo, 0x20},
    {at::kInputHeld, 8},                                        // Input_Held .. Input_Pressed
    {at::kConfirmButtons, 4},                                   // Field_ConfirmButtons, Field_CancelButtons
    {0x904A00, 0x300},                                          // the battle's globals, the hook, Text_Records[0]
    {at::kBattleMusic, 4},                                      // 0x904EFC, 0x904EFE
    {0x937F88, 4},                                              // Sprite_Current
    {0x939EC0, 0x500},                                          // 0x939EC4, 0x939F60, 0x939FA0, the message records
    {at::kB8E0, 0x9D0},                                         // 0x93B8E0, the eight enemy objects, 0x93C2A0..A2
    {at::kStatusOffsets, 0xA00},                                // constant, random here and put back: 0x64DF70, 0x64E2BC
};
constexpr unsigned kRegionBytes = 0x3E4 + 0x240 + 8 + 0x20 + 0x20 + 8 + 4 + 0x300 + 4 + 4 + 0x500 + 0x9D0 + 0xA00;

struct State {
    unsigned char memory[kRegionBytes];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
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

// Random bytes put back inside what they index: the menu pointers, the
// sprite, the order entries (members), the menu index, the message records'
// enemies, the event hook.
void Fix() {
    const unsigned actor = Next() % 3;
    SetPtr(at::kMenuActor, Member(actor));
    SetPtr(at::kMenuRecord, Member(Half() ? actor : Next() % 3) + 0x124);
    Sprite_Current = Half() ? Member(Next() % 3) : EnemyObj(Next() % 8);
    for (unsigned i = 0; i < 3; ++i) B(at::kEntryOrder + i) = static_cast<unsigned char>(Next() % 3);
    B(at::kMenuIndex) = static_cast<unsigned char>(Next() % 4);
    B(at::kMessageCount) = static_cast<unsigned char>(Next() % 9);
    for (unsigned n = 0; n <= 8; ++n) B(at::kMessages + 4 * n) = static_cast<unsigned char>(Next() % 8);
    SetPtr(at::kEventHook, reinterpret_cast<const void*>(&StubEventHook));
    if (Half()) B(at::kEventBattle) = 0;
    if (Half()) B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) & ~0x12u);
}

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    switch (k) {
    case kFrame:
        if (Half()) B(at::kWindows) = 0;
        if (Half()) B(at::kAE9) = static_cast<unsigned char>(B(at::kAE9) & ~2u);
        if (Half()) B(at::kPhase) = 5;
        break;
    case kStart:
        B(at::kStep) = static_cast<unsigned char>(Next() % 2);
        break;
    case kInit: {
        B(at::kActorCount) = static_cast<unsigned char>(Often() ? Next() % 5 : Next() % 16);
        for (unsigned i = 0; i < 3; ++i) {
            unsigned char* const p = Member(i);
            if (Often()) p[0] = static_cast<unsigned char>(p[0] | 1);
            const unsigned max = Next() & 0xFFFF;
            SetWord(p + 0xA0, max);
            SetWord(p + 0x98, (max >> 2) + (Next() % 3) - 1);
        }
        static const unsigned char kEvent[] = {0, 0, 0x25, 0x10, 1};
        B(at::kEventBattle) = kEvent[Next() % 5];
        break;
    }
    case kIntro:
        B(at::kSubStep) = static_cast<unsigned char>(Next() % 4);
        break;
    case kOpenWindows:
    case kFinish:
    case kPrepareCross: {
        static const unsigned char kInit[] = {0, 1, 2, 3, 0xFF};
        B(at::kInitiative) = kInit[Next() % 5];
        if (Half()) Window(2)[3] = 1;
        for (unsigned i = 0; i < 3; ++i) if (Half()) Member(i)[0] = static_cast<unsigned char>(Member(i)[0] | 1);
        break;
    }
    case kWaitWindow:
        if (Half()) Window(1)[3] = 1;
        break;
    case kInput:
        B(at::kStep) = static_cast<unsigned char>(Next() % at::kInputStepCount);
        break;
    case kRoundStart:
        if (Half()) B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) | 0x10);
        break;
    case kNextMember:
        for (unsigned i = 0; i < 3; ++i) {
            if (Next() % 4 == 0) B(at::kEntryOrder + i) = 0xFF;
            unsigned char* const p = Member(i);
            p[0x90] = static_cast<unsigned char>(Next() % 4 == 0 ? p[0x90] | 0x20 : p[0x90] & ~0x20u);
            std::uint32_t f = static_cast<std::uint32_t>(Long(p + 0x134)) & ~0x14001u;
            if (Next() % 6 == 0) f |= 1;
            if (Next() % 6 == 0) f |= 0x4000;
            if (Next() % 6 == 0) f |= 0x10000;
            SetLong(p + 0x134, static_cast<std::int32_t>(f));
        }
        break;
    case kCommandSelect: {
        static const unsigned char kTimer[] = {0, 0, 1, 2, 8};
        B(at::kTapTimer) = kTimer[Next() % 5];
        B(at::kTapCommand) = static_cast<unsigned char>(Next() % 8);
        B(at::kCommand) = static_cast<unsigned char>(Next() % 8);
        static const std::uint16_t kShape[] = {0x10, 0x20, 0x40, 0x80, 0x43, 0x30};
        const unsigned cancel = kShape[Next() % 6], confirm = kShape[Next() % 6];
        SetWord(At(at::kCancelButtons), cancel);
        SetWord(At(at::kConfirmButtons), confirm);
        static const std::uint16_t kDirs[] = {0x1000, 0x4000, 0x8000, 0x2000, 0x0004, 0x0008};
        unsigned pressed = 0, held = 0;
        switch (Next() % 6) {
        case 0: pressed = cancel; break;
        case 1: pressed = confirm; break;
        case 2: held = 0x100; break;
        case 3:
        case 4: {
            const unsigned d = Next() % 6;
            held = kDirs[d] | (Half() ? kDirs[Next() % 6] : 0u);
            if (Often()) pressed = kDirs[d];
            if (Half()) B(at::kTapCommand) = static_cast<unsigned char>(d + 1);
            if (Half() && B(at::kTapTimer) == 0) B(at::kTapTimer) = 8;
            break;
        }
        default: break;
        }
        if (Next() % 4 == 0) pressed |= Next() & 0xF1FF;
        if (Next() % 4 == 0) held |= Next() & 0xF1FF;
        SetWord(At(at::kInputPressed), pressed);
        SetWord(At(at::kInputHeld), held);
        unsigned char* const a = MenuActor();
        a[0x125] = static_cast<unsigned char>(Half() ? 5 : Next());
        SetLong(a + 0x130, static_cast<std::int32_t>(Half() ? Next() | 0x4000 : Next() & ~0x4000u));
        break;
    }
    case kConfirmDispatch:
        B(at::kSubStep) = static_cast<unsigned char>(Next() % at::kMenuStepCount);
        break;
    case kCommit:
        B(at::kStep) = static_cast<unsigned char>(Next() % at::kCommitStepCount);
        break;
    case kCommitRound:
        if (Half()) B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) | 0x10);
        for (unsigned i = 0; i < 3; ++i) {
            unsigned char* const p = Member(i);
            if (Often()) p[0x125] = 5;
            if (Half()) p[0x127] = 0;
            SetLong(p + 0x130, static_cast<std::int32_t>(Half() ? Next() | 0x4000 : Next() & ~0x4000u));
        }
        break;
    case kQueueMessages:
        if (Often()) B(at::kMessageBusy) = 0;
        if (Next() % 4 == 0) B(at::kMessageCount) = 0;
        break;
    case kWaitLoad: {
        if (Half()) B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) | 0x10);
        static const std::uint16_t kWait[] = {0, 0, 1, 2, 0xFFFF, 0x8000};
        SetWord(At(at::kWaitFrames), Often() ? kWait[Next() % 6] : Next());
        break;
    }
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[150];
    unsigned sounds[8];   // 0x101, 0x104, 0x106, 0x107
    unsigned dispatched, frame_held, low_hp_set, low_hp_cleared, member_found, none_left, auto_filled;
    unsigned chose_by_tap, tap_greyed, tap_started, cleared_command, window_ff, message_queued, waited, advanced;
} g_cover;
unsigned Count(const State& s, unsigned what) {
    unsigned n = 0;
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i) if (s.log[i].what == what) ++n;
    return n;
}
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        if (out.log[i].what < 150) ++g_cover.logged[out.log[i].what];
        if (out.log[i].what == 45) {
            const unsigned id = out.log[i].a;
            ++g_cover.sounds[id == 0x101 ? 0 : id == 0x104 ? 1 : id == 0x106 ? 2 : id == 0x107 ? 3 : 4];
        }
    }
    switch (k) {
    case kFrame:
        if (Count(out, 1)) ++g_cover.dispatched;
        else ++g_cover.frame_held;
        break;
    case kInit:
        for (unsigned i = 0; i < 3; ++i) {
            const std::uint32_t p = at::kMembers + i * at::kMemberSize;
            if (!(Byte(in, p) & 1)) continue;
            if (Byte(out, p + 0x91) & 0x20) ++g_cover.low_hp_set;
            else ++g_cover.low_hp_cleared;
        }
        break;
    case kOpenWindows:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 30 && out.log[i].a == 0x14 && out.log[i].c == 0xFF) { ++g_cover.window_ff; break; }
        break;
    case kRoundStart:
        if (Count(out, 42)) ++g_cover.auto_filled;
        break;
    case kNextMember:
        if (Count(out, 44)) ++g_cover.member_found;
        else if (Byte(out, at::kPhase) != Byte(in, at::kPhase)) ++g_cover.none_left;
        break;
    case kCommandSelect:
        if (Byte(out, at::kTapCommand) == 0 && Byte(in, at::kTapCommand) != 0 && Byte(out, at::kStep) != (Byte(in, at::kStep) + 2) % 256)
            ++g_cover.tap_greyed;
        if (Byte(out, at::kStep) == (Byte(in, at::kStep) + 2) % 256 && Byte(out, at::kTapTimer) == 0 && Count(out, 45) && out.log[0].a == 0x101)
            ++g_cover.chose_by_tap;
        if (Byte(out, at::kTapTimer) == 8) ++g_cover.tap_started;
        if (Byte(in, at::kCommand) != 0 && Byte(out, at::kCommand) == 0 && out.log_n == 0) ++g_cover.cleared_command;
        break;
    case kQueueMessages:
        if (Count(out, 32)) ++g_cover.message_queued;
        break;
    case kWaitLoad:
        if (Byte(out, at::kPhase) != Byte(in, at::kPhase)) ++g_cover.advanced;
        else if (Count(out, 37)) ++g_cover.waited;
        break;
    default:
        break;
    }
}

void PatchImms(void* copy, const char* name, const Imm* imms, unsigned n, const Handler* to) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (unsigned i = 0; i < n; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + imms[i].offset, sizeof had);
        if (had != imms[i].value)
            bof3::Fatal("battle_phases: %s +0x%X holds 0x%X, not the handler 0x%X", name, static_cast<unsigned>(imms[i].offset),
                        static_cast<unsigned>(had), static_cast<unsigned>(imms[i].value));
        const std::uint32_t target = Address(reinterpret_cast<const void*>(to[i]));
        std::memcpy(code + imms[i].offset, &target, sizeof target);
    }
}

// A .data table's entries checked against what the disassembly showed, and
// swapped for recorders.
void SwapTable(std::uint32_t table, const std::uint32_t* expected, const Handler* to, unsigned n, std::uint32_t* saved) {
    for (unsigned i = 0; i < n; ++i) {
        saved[i] = static_cast<std::uint32_t>(Long(At(table + 4 * i)));
        if (saved[i] != expected[i])
            bof3::Fatal("battle_phases: the table 0x%X entry %u is 0x%X, not 0x%X", static_cast<unsigned>(table), i,
                        static_cast<unsigned>(saved[i]), static_cast<unsigned>(expected[i]));
        SetPtr(table + 4 * i, reinterpret_cast<const void*>(to[i]));
    }
}
void RestoreTable(std::uint32_t table, const std::uint32_t* saved, unsigned n) {
    for (unsigned i = 0; i < n; ++i) SetLong(At(table + 4 * i), static_cast<std::int32_t>(saved[i]));
}
constexpr std::uint32_t kInputTargets[at::kInputStepCount] = {0x42E9A0, 0x42E9E0, 0x42EAD0, 0x42ED90, 0x42EED0};
constexpr std::uint32_t kMenuTargets[at::kMenuStepCount] = {0x447110, 0x447430, 0x448180, 0x447FD0,
                                                            0x42EEF0, 0x42EF50, 0x44A000, 0x44FF00};
constexpr std::uint32_t kCommitTargets[at::kCommitStepCount] = {0x42F080, 0x42F130, 0x42F1D0};

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_phases: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("battle_phases: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    PatchImms(clones[kStart], "BattleStart_Dispatch", kStartImm, 2, kStubs.start_steps);
    PatchImms(clones[kIntro], "BattleIntro_Dispatch", kIntroImm, 4, kStubs.intro_steps);

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Battle_Frame), reinterpret_cast<const void*>(&BattleStart_Dispatch),
        reinterpret_cast<const void*>(&Battle_Init), reinterpret_cast<const void*>(&BattleIntro_Dispatch),
        reinterpret_cast<const void*>(&BattleIntro_OpenWindows), reinterpret_cast<const void*>(&BattleIntro_WaitWindow),
        reinterpret_cast<const void*>(&BattleIntro_PrepareCross), reinterpret_cast<const void*>(&BattleIntro_Finish),
        reinterpret_cast<const void*>(&BattleInput_Dispatch), reinterpret_cast<const void*>(&Battle_RoundStart),
        reinterpret_cast<const void*>(&BattleInput_NextMember), reinterpret_cast<const void*>(&BattleMenu_CommandSelect),
        reinterpret_cast<const void*>(&BattleMenu_ConfirmDispatch), reinterpret_cast<const void*>(&Cmd_ConfirmDefend),
        reinterpret_cast<const void*>(&BattleCommit_Dispatch), reinterpret_cast<const void*>(&Battle_CommitRound),
        reinterpret_cast<const void*>(&BattleCommit_QueueMessages), reinterpret_cast<const void*>(&BattleCommit_WaitLoad)};

    static State saved, input, their_out, our_out;
    std::uint32_t saved_input[at::kInputStepCount], saved_menu[at::kMenuStepCount], saved_commit[at::kCommitStepCount];
    Capture(saved);
    SwapTable(at::kInputSteps, kInputTargets, kInputStubs, at::kInputStepCount, saved_input);
    SwapTable(at::kMenuSteps, kMenuTargets, kMenuStubs, at::kMenuStepCount, saved_menu);
    SwapTable(at::kCommitSteps, kCommitTargets, kCommitStubs, at::kCommitStepCount, saved_commit);
    g = kStubs;

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
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
                bof3::Log("shadow      battle_phases self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    RestoreTable(at::kInputSteps, saved_input, at::kInputStepCount);
    RestoreTable(at::kMenuSteps, saved_menu, at::kMenuStepCount);
    RestoreTable(at::kCommitSteps, saved_commit, at::kCommitStepCount);
    Apply(saved);

    bof3::Log("shadow      battle_phases self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the party records, the windows, the battle's globals, the menu pointers and messages, "
              "the enemy objects, the tables and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_phases: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned starts = 0, intros = 0, inputs = 0, menus = 0, commits = 0;
    for (unsigned i = 100; i < 102; ++i) starts += c.logged[i] ? 1u : 0u;
    for (unsigned i = 110; i < 114; ++i) intros += c.logged[i] ? 1u : 0u;
    for (unsigned i = 120; i < 125; ++i) inputs += c.logged[i] ? 1u : 0u;
    for (unsigned i = 130; i < 138; ++i) menus += c.logged[i] ? 1u : 0u;
    for (unsigned i = 140; i < 143; ++i) commits += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      battle_phases coverage: start steps %u of 2, intro steps %u of 4, input steps %u of 5, menu "
              "steps %u of 8, commit steps %u of 3; frame dispatched %u / held %u; init: actors tested %u, clut stp %u, "
              "event hook %u, music %u, low HP set %u / cleared %u; windows allocated %u (the first 0xFF %u), messages %u; "
              "sub-windows %u / %u / %u; auto filled %u; member found %u, none left %u; sounds 0x101 %u, 0x104 %u, "
              "0x106 %u, 0x107 %u; items returned %u; tap chose %u, greyed %u, started %u; command cleared %u; "
              "crosses %u; enemy messages %u; wait counted %u, advanced %u",
              starts, intros, inputs, menus, commits, c.dispatched, c.frame_held, c.logged[21], c.logged[27],
              c.logged[29], c.logged[28], c.low_hp_set, c.low_hp_cleared, c.logged[30], c.window_ff, c.logged[32],
              c.logged[35], c.logged[38], c.logged[39], c.auto_filled, c.member_found, c.none_left, c.sounds[0],
              c.sounds[1], c.sounds[2], c.sounds[3], c.logged[46], c.chose_by_tap, c.tap_greyed, c.tap_started,
              c.cleared_command, c.logged[47], c.message_queued, c.waited, c.advanced);
    if (bad) bof3::Fatal("the battle's frame and phases 0..2 differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_phases

void BattlePhases_Inject() {
    if (bof3::WantsShadow("battle_phases")) battle_phases::SelfTest();
    BOF3_INJECT(Battle_Frame);
    BOF3_INJECT(BattleStart_Dispatch);
    BOF3_INJECT(Battle_Init);
    BOF3_INJECT(BattleIntro_Dispatch);
    BOF3_INJECT(BattleIntro_OpenWindows);
    BOF3_INJECT(BattleIntro_WaitWindow);
    BOF3_INJECT(BattleIntro_PrepareCross);
    BOF3_INJECT(BattleIntro_Finish);
    BOF3_INJECT(BattleInput_Dispatch);
    BOF3_INJECT(Battle_RoundStart);
    BOF3_INJECT(BattleInput_NextMember);
    BOF3_INJECT(BattleMenu_CommandSelect);
    BOF3_INJECT(BattleMenu_ConfirmDispatch);
    BOF3_INJECT(Cmd_ConfirmDefend);
    BOF3_INJECT(BattleCommit_Dispatch);
    BOF3_INJECT(Battle_CommitRound);
    BOF3_INJECT(BattleCommit_QueueMessages);
    BOF3_INJECT(BattleCommit_WaitLoad);
}

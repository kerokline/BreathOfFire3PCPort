// BOF3X_SHADOW=battle_turn_steps: a differential fuzz of the per-turn steps,
// once at start-up. docs/battle_turn_steps.md section 5.
//
// Twenty byte-copies, every call out re-aimed at a recording stand-in; the two
// inline jump tables (0x4303B4, 0x4305FC) relocated into their copies; the
// eight .data dispatch tables' 29 entries and the three hooks 0x904B64 /
// 0x904B68 / 0x904B6C swapped for recorders (and put back). One round: one
// function, random bytes over every region any of them touches - the party's
// records and the window bytes after them, the character records and the
// battle globals, the second party array at 0x939AE0, the task slots and the
// enemies, Sprite_Current, the task slot 0xFF's owner, Game_Step,
// Input_Pressed - then the counts and indices put back inside their arrays and
// that function's branch boundaries seeded; theirs, then from the same state
// ours; the regions and the stand-ins' log compared.
//
// The stand-ins are loud: each logs its arguments with a hash of the watched
// state bytes as they were at the call (so a store moved across a call is
// seen), and three calls in four moves one of the cells some caller reads
// again after it (the state bytes, the member count and cursor, 0x904AE8,
// the round flags, the statuses, the flags, Music_Track, Input_Pressed ...).
// Answers in al come with random upper bits.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <cstring>

#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/battle_turn_steps_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_turn_steps {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x3B9D52C1u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, watch; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;          // the stand-ins' own stream: the same on both passes
// Task slot 0xFF's owner 0x9423FC is past the image (it ends at 0x93F000): the
// stand-in answers 0xFF only when a page could be committed there for the run.
bool g_slot_ff;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}

unsigned char* Member(unsigned i) { return At(at::kParty + i * at::kPartyStride); }
unsigned char& B(std::uint32_t address) { return *At(address); }

// The cells whose value at a call the log keeps: the state bytes, the counts,
// the flags bytes, the members' fields the steps write, Sprite_Current.
std::uint32_t Watch() {
    std::uint32_t h = 0x811C9DC5u;
    const auto mix = [&h](std::uint32_t v) { h = (h ^ v) * 0x01000193u; };
    for (std::uint32_t a = 0x904AA0; a < 0x904AC4; ++a) mix(B(a));
    for (std::uint32_t a : {0x904AE4u, 0x904AE5u, 0x904AE8u, 0x904AE9u, 0x904B7Au, 0x904B82u, 0x904B83u, 0x904B8Eu,
                            0x904B90u, 0x904B91u, 0x802D20u, 0x904131u, 0x66C7EAu, 0x66C7EBu})
        mix(B(a));
    for (unsigned i = 0; i < 5; ++i) {
        const unsigned char* const m = Member(i);
        for (unsigned o : {0x01u, 0x98u, 0x99u, 0x9Au, 0x9Bu, 0x9Cu, 0x131u, 0x134u, 0x135u}) mix(m[o]);
    }
    for (unsigned k = 0; k < 3; ++k) mix(B(at::kCopies + k * at::kPartyStride + at::kFlags));
    for (unsigned i = 0; i < 8; ++i) mix(B(at::kEnemy + i * at::kEnemyStride + at::kEFlags + 1));
    for (unsigned k = 0; k < 12; ++k) mix(B(at::kWindows + k * at::kWindowStride));
    mix(Address(Sprite_Current));
    return h;
}

void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, Watch()};
    ++g_log_n;
}

// Every cell below is one some function reads again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const auto v = static_cast<unsigned char>(h >> 12);
    const unsigned w = h >> 20;
    unsigned char* const m = Member((h >> 8) % 5);
    switch ((h >> 4) % 19) {
    case 0: B(at::kState1) = static_cast<unsigned char>(v % 4); break;
    case 1: B(at::kState2) = static_cast<unsigned char>(v % 15); break;
    case 2: B(at::kCursor) = static_cast<unsigned char>(v % 6); break;
    case 3: B(at::kMembers) = static_cast<unsigned char>(v % 5); break;
    case 4: B(at::kBattleEnd) = v; break;
    case 5: B(at::kRoundFlags) = v; break;
    case 6: B(at::kEventBattle) = (w & 1) ? 0 : v; break;
    case 7: m[at::kStatus + (w & 1)] = v; break;
    case 8: m[at::kFlags] = v; break;
    case 9: m[at::kCharId] = (w & 1) ? 4 : v; break;
    case 10: B(at::kMusicTrack) = (w & 1) ? 0xFF : v; break;
    case 11: B(at::kMusicFlags) = v; break;
    case 12: SetWord(At(at::kInput), (w & 1) ? 0 : v); break;
    case 13: m[at::kFlags2 + 1] = v; break;
    case 14: SetWord(At(at::kPending), (w & 1) ? 0 : v); break;
    case 15: B(at::kSideMode) = (w & 1) ? 2 : v; break;
    case 16: m[0] = v; break;
    case 17: Sprite_Current = At(0x10000u + (h & 0xFFFF0u)); break;
    default: B(at::kExtraRound) = v; break;
    }
}

// An answer in al with random upper bits: `one` of the time 1 in `odds`.
unsigned Answer(unsigned odds) { return (Hash() & ~0xFFu) | ((Hash() >> 7) % odds == 0 ? 1u : 0u); }

// --- the stand-ins ---------------------------------------------------------

unsigned __cdecl StubMarkFaster() {
    Record(1);
    Disturb();
    static const unsigned kAl[] = {0, 1, 2, 3, 0, 1, 2, 3, 4, 0xFF, 0x80};
    return (Hash() & ~0xFFu) | kAl[(Hash() >> 5) % 11];
}
void __cdecl StubTick() { Record(2); Disturb(); }
template <unsigned N> unsigned __cdecl StubStep() {
    Record(3 + N);
    if (Hash() % 3 == 0) B(at::kState2) = static_cast<unsigned char>((Hash() >> 8) % 15);
    Disturb();
    return Answer(3);
}
void __cdecl StubLoadDat(int file) {
    Record(12, static_cast<std::uint32_t>(file));
    if (Hash() % 3 == 0) B(at::kState1) = static_cast<unsigned char>((Hash() >> 8) % 4);
    Disturb();
}
void __cdecl StubClearCommands() { Record(13); Disturb(); }
unsigned __cdecl StubActorIsOut(unsigned a) {
    Record(14, a & 0xFF);
    if (Hash() % 4 == 0) B(at::kCursor) = static_cast<unsigned char>((Hash() >> 8) % 5);
    if (Hash() % 5 == 0) B(at::kMembers) = static_cast<unsigned char>((Hash() >> 12) % 5);
    Disturb();
    return Answer(4);
}
unsigned __cdecl StubTaskCreate(unsigned kind, unsigned parameter) {
    Record(15, kind, parameter);
    if (Hash() % 3 == 0) B(at::kCursor) = static_cast<unsigned char>((Hash() >> 8) % 5);
    Disturb();
    return (Hash() & ~0xFFu) | (g_slot_ff && (Hash() >> 9) % 8 == 0 ? 0xFFu : (Hash() >> 11) % 0x30);
}
// Moves the status word its caller re-reads.
unsigned __cdecl StubClearStatus(unsigned a, unsigned mask) {
    Record(16, a & 0xFF, mask & 0xFFFF);
    if ((a & 0xFF) < 5 && Hash() % 2) SetWord(Member(a & 0xFF) + at::kStatus, Hash() >> 5);
    Disturb();
    return Hash();
}
void __cdecl StubReleaseTint(unsigned char* object) { Record(17, Address(object)); Disturb(); }
void __cdecl StubReturnItem(unsigned a) {
    Record(18, a & 0xFF);
    if (Hash() % 3 == 0) B(at::kBattleEnd) = static_cast<unsigned char>(Hash() >> 8);
    if (Hash() % 4 == 0) B(at::kMembers) = static_cast<unsigned char>((Hash() >> 12) % 5);
    Disturb();
}
void __cdecl StubDroppedCall(unsigned b) { Record(19, b & 0xFF); Disturb(); }
// Moves Music_Track, which BattleEnd_ExitHook reads after it.
void __cdecl StubFadeOut(int frames) {
    Record(20, static_cast<std::uint32_t>(frames));
    if (Hash() % 3 == 0) B(at::kMusicTrack) = (Hash() & 0x10) ? 0xFF : static_cast<unsigned char>(Hash() >> 3);
    Disturb();
}
void __cdecl StubMusicPlay(unsigned track, int frames) { Record(21, track, static_cast<std::uint32_t>(frames)); Disturb(); }
void __cdecl StubOpenWindow() { Record(22); Disturb(); }
void __cdecl StubOpeningMessage() { Record(23); Disturb(); }
int __cdecl StubLoadDone() {
    Record(24);
    Disturb();
    const std::uint32_t h = Hash();
    switch ((h >> 3) % 4) {
    case 0: return 0;
    case 1: return static_cast<int>((h & 0xFFFFFF00u) | 0x100u);   // not 0, but al is
    default: return static_cast<int>(h | 1u);
    }
}
void __cdecl StubWriteBack(unsigned a) { Record(25, a); Disturb(); }
void __cdecl StubWindowReset() { Record(26); Disturb(); }
void __cdecl StubClearEnemies() { Record(27); Disturb(); }
void __cdecl StubTaskClearAll() { Record(28); Disturb(); }
// The record as the caller left it.
void __cdecl StubRecalc(unsigned char* record) {
    Record(29, Address(record), static_cast<std::uint32_t>(Word(record + 0x18)) | (record[0x1C] << 16) | (record[0x1E] << 24),
           static_cast<std::uint32_t>(Word(record + 0x10)) | (record[0xB] << 16));
    if (Hash() % 3 == 0) B(at::kMembers) = static_cast<unsigned char>((Hash() >> 12) % 5);
    Disturb();
}
void __cdecl StubReloadParty() { Record(30); Disturb(); }
unsigned __cdecl StubHookEvent(int n) {
    Record(31, static_cast<std::uint32_t>(n));
    Disturb();
    return (Hash() & ~0xFFu) | ((Hash() >> 6) % 3 == 0 ? 0xFFu : (Hash() >> 9) & 0xFE);
}
void __cdecl StubHookEnd() { Record(32); Disturb(); }
void __cdecl StubHookExit() { Record(33); Disturb(); }
// A dispatch table's slot: table t, entry s.
template <unsigned T, unsigned S> void __cdecl StubSlot() { Record(100 + T * 8 + S); Disturb(); }

const Callees kStubs = {
    StubMarkFaster,
    StubTick,
    {StubStep<0>, StubStep<1>, StubStep<2>, StubStep<3>, StubStep<4>, StubStep<5>, StubStep<6>, StubStep<7>, StubStep<8>},
    StubLoadDat,
    StubClearCommands,
    StubActorIsOut,
    StubTaskCreate,
    StubClearStatus,
    StubReleaseTint,
    StubReturnItem,
    StubDroppedCall,
    StubFadeOut,
    StubMusicPlay,
    StubOpenWindow,
    StubOpeningMessage,
    StubLoadDone,
    StubWriteBack,
    StubWindowReset,
    StubClearEnemies,
    StubTaskClearAll,
    StubRecalc,
    StubReloadParty,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x4453C0: return f(kStubs.mark_faster);      // Battle_MarkFasterSide
    case 0x4303D0: return f(kStubs.tick_counters);    // Battle_TickCounters
    case 0x430640: return f(kStubs.steps[0]);         // BattleStep_Expire4000
    case 0x430790: return f(kStubs.steps[1]);
    case 0x430890: return f(kStubs.steps[2]);
    case 0x430970: return f(kStubs.steps[3]);
    case 0x430A50: return f(kStubs.steps[4]);
    case 0x430B30: return f(kStubs.steps[5]);
    case 0x430C10: return f(kStubs.steps[6]);
    case 0x430D40: return f(kStubs.steps[7]);
    case 0x430F40: return f(kStubs.steps[8]);         // BattleStep_ApUpkeep
    case 0x454590: return f(kStubs.load_dat);         // LoadDatFile
    case 0x445680: return f(kStubs.clear_commands);   // Battle_ClearCommands
    case 0x4456C0: return f(kStubs.actor_is_out);     // Battle_ActorIsOut
    case 0x435180: return f(kStubs.task_create);      // BattleTask_Create
    case 0x44F4B0: return f(kStubs.clear_status);     // Battle_ClearStatus
    case 0x454DC0: return f(kStubs.release_tint);     // Sprite_ReleaseTint
    case 0x446EA0: return f(kStubs.return_item);      // Battle_ReturnQueuedItem
    case 0x4DF820: return f(kStubs.dropped_call);     // Port_DroppedCall
    case 0x587B40: return f(kStubs.fade_out);         // Music_FadeOutStop
    case 0x587AE0: return f(kStubs.music_play);       // Music_Play
    case 0x444310: return f(kStubs.open_window);      // Battle_OpenMsgWindow
    case 0x44AA00: return f(kStubs.opening_message);  // Battle_OpeningMessage
    case 0x454810: return f(kStubs.load_done);        // File_LoadDone
    case kWriteBackMember: return f(kStubs.write_back_member);
    case 0x59E330: return f(kStubs.window_reset);     // Window_ResetAll
    case kClearEnemies: return f(kStubs.clear_enemies);
    case 0x435260: return f(kStubs.task_clear_all);   // BattleTask_ClearAll
    case 0x590660: return f(kStubs.recalc_stats);     // Char_RecalcStats
    case kReloadParty: return f(kStubs.reload_party);
    default: bof3::Fatal("battle_turn_steps: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the dispatch tables' recorders ------------------------------------------

struct DataTable { std::uint32_t at; unsigned entries; std::uint32_t state; const void* slots[5]; };
#define CC_SLOTS(t) {reinterpret_cast<const void*>(&StubSlot<t, 0>), reinterpret_cast<const void*>(&StubSlot<t, 1>), \
                     reinterpret_cast<const void*>(&StubSlot<t, 2>), reinterpret_cast<const void*>(&StubSlot<t, 3>), \
                     reinterpret_cast<const void*>(&StubSlot<t, 4>)}
const DataTable kTables[] = {
    {at::kRoundEndSteps, 3, at::kState1, CC_SLOTS(0)},
    {at::kRoundEndFaster, 3, at::kState2, CC_SLOTS(1)},
    {at::kEndSteps, 5, at::kState1, CC_SLOTS(2)},
    {at::kEndTaskSteps, 3, at::kState2, CC_SLOTS(3)},
    {at::kEndWinSteps, 3, at::kState2, CC_SLOTS(4)},
    {at::kEndExitSteps, 4, at::kState2, CC_SLOTS(5)},
    {at::kEndResultSteps, 3, at::kState3, CC_SLOTS(6)},
    {at::kEndResultPages, 5, at::kState4, CC_SLOTS(7)},
};
#undef CC_SLOTS
constexpr unsigned kTableCount = sizeof kTables / sizeof kTables[0];
// What each table held at start-up, checked against the disassembly's reading.
constexpr std::uint32_t kTableHeld[kTableCount][5] = {
    {0x4302C0, 0x430510, 0x431090},
    {0x4302D0, 0x430500, 0x431090},
    {0x4311F0, 0x4314B0, 0x431540, 0x4315B0, 0x431760},
    {0x431200, 0x431220, 0x431320},
    {0x4314C0, 0x431520, 0x431910},
    {0x431770, 0x4317B0, 0x4317F0, 0x4318F0},
    {0x431920, 0x431B60, 0x431D50},
    {0x431940, 0x431A20, 0x431A90, 0x431AB0, 0x431B30},
};

// --- the twenty copies (capstone, 2026-09-25: every jump internal; the calls
// below are every relative call that leaves) ----------------------------------

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    move_script::Table table;   // an inline jump table, relocated into the copy
    int dispatch;               // the .data table a stub jumps through, or -1
    const void* ours;
};

constexpr Call kCheck[] = {{0x12, 0x4453C0}, {0xB6, 0x4303D0}};
constexpr Call kChain[] = {{0x1D, 0x430640}, {0x36, 0x430790}, {0x49, 0x430890}, {0x58, 0x430970}, {0x67, 0x430A50},
                           {0x76, 0x430B30}, {0x85, 0x430C10}, {0x9A, 0x430D40}, {0xAF, 0x430F40}};
constexpr Call kNext[] = {{0xDF, 0x454590}, {0xE9, 0x445680}, {0xFE, 0x4456C0}};
constexpr Call kStart[] = {{0x15, 0x4456C0}, {0xA6, 0x435180}};
constexpr Call kAwait[] = {{0xD8, 0x44F4B0}, {0xEF, 0x44F4B0}, {0xF5, 0x454DC0}, {0xFB, 0x446EA0},
                           {0x14C, 0x4DF820}, {0x162, 0x454590}, {0x179, 0x454590}};
constexpr Call kWin[] = {{0x3B, 0x587B40}, {0x47, 0x587AE0}, {0x4F, 0x444310}, {0x54, 0x44AA00}};
constexpr Call kAwaitInput[] = {{0x0, 0x454810}};
constexpr Call kWriteBack[] = {{0x2, 0x446A80}, {0x9, 0x446A80}, {0x10, 0x446A80}, {0x15, 0x59E330}, {0x1C, 0x454590}};
constexpr Call kExitHook[] = {{0x0, 0x454810}, {0x1A, 0x587B40}};
constexpr Call kFinish[] = {{0x3, 0x494E70}, {0x8, 0x435260}, {0x9E, 0x590660}, {0xBF, 0x446600}};

#define CC_C(name, base, size, calls) \
    {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), {}, -1, reinterpret_cast<const void*>(&::name)}
#define CC_T(name, base, size, calls, jmp, table, n) \
    {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), {jmp, table, n}, -1, reinterpret_cast<const void*>(&::name)}
#define CC_S(name, base, size, t) {#name, base, size, nullptr, 0, {}, t, reinterpret_cast<const void*>(&::name)}
#define CC_P(name, base, size) {#name, base, size, nullptr, 0, {}, -1, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    CC_S(BattleRoundEnd_Step, 0x4302B0, 0xE, 0),
    CC_S(BattleRoundEnd_FasterStep, 0x4302C0, 0xE, 1),
    CC_T(BattleRoundEnd_CheckFaster, 0x4302D0, 0xF4, kCheck, 0x24, 0xE4, 4),
    CC_P(BattleRoundEnd_FasterNext, 0x430500, 0x7),
    CC_T(BattleRoundEnd_StatusChain, 0x430510, 0x124, kChain, 0x13, 0xEC, 14),
    CC_C(BattleRoundEnd_NextRound, 0x431090, 0x147, kNext),
    CC_S(BattleEnd_Step, 0x4311E0, 0xE, 2),
    CC_S(BattleEnd_TaskStep, 0x4311F0, 0xE, 3),
    CC_P(BattleEnd_TasksBegin, 0x431200, 0x13),
    CC_C(BattleEnd_StartMemberTask, 0x431220, 0xF6, kStart),
    CC_C(BattleEnd_AwaitMemberTasks, 0x431320, 0x18C, kAwait),
    CC_S(BattleEnd_WinStep, 0x4314B0, 0xE, 4),
    CC_C(BattleEnd_WinBegin, 0x4314C0, 0x60, kWin),
    CC_C(BattleEnd_WinAwaitInput, 0x431520, 0x1A, kAwaitInput),
    CC_S(BattleEnd_ExitStep, 0x431760, 0xE, 5),
    CC_C(Battle_WriteBackParty, 0x431770, 0x31, kWriteBack),
    CC_C(BattleEnd_ExitHook, 0x4317B0, 0x39, kExitHook),
    CC_C(BattleEnd_Finish, 0x4317F0, 0xF9, kFinish),
    CC_S(BattleEnd_ResultStep, 0x431910, 0xE, 6),
    CC_S(BattleEnd_ResultPage, 0x431920, 0x11, 7),
};
#undef CC_C
#undef CC_T
#undef CC_S
#undef CC_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kRoundStepK, kFasterStepK, kCheckK, kFasterNextK, kChainK, kNextK, kEndStepK, kTaskStepK, kBeginK, kStartK, kAwaitK,
    kWinStepK, kWinK, kInputK, kExitStepK, kWriteBackK, kExitHookK, kFinishK, kResultStepK, kResultPageK,
};
static_assert(kResultPageK + 1 == kCount, "the index enum follows kClones");

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
constexpr Region kRegions[] = {
    {0x802D20, 0x840},    // 0x802D20, the party's records (and the array on to member 4), the window bytes
                          // 0x803184.., the busy dwords 0x803540..0x80355C
    {0x903A70, 0x1290},   // CharacterRecords (the index is kept to 0..9) and the battle globals to 0x904D00
    {0x939AE0, 0x3D0},    // the second party array's three +0x134 dwords
    {0x93A000, 0x22A0},   // the 48 task slots and the eight enemy objects
    {0x937F88, 4},        // Sprite_Current
    {0x9423FC, 4},        // task slot 0xFF's owner, where an unchecked 0xFF writes
    {0x66C7EA, 2},        // Game_Step
    {0x7E1BEC, 2},        // Input_Pressed
};
constexpr unsigned kRegionBytes = 0x840 + 0x1290 + 0x3D0 + 0x22A0 + 4 + 4 + 2 + 2;

struct State {
    unsigned char memory[kRegionBytes];
    Entry log[kLog];
    unsigned log_n;
};
bool Mapped(const Region& r) { return r.at != 0x9423FC || g_slot_ff; }
void Capture(State& s) {
    unsigned off = 0;
    for (const Region& r : kRegions) {
        if (Mapped(r)) std::memcpy(s.memory + off, At(r.at), r.size);
        off += r.size;
    }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned off = 0;
    for (const Region& r : kRegions) {
        if (Mapped(r)) std::memcpy(At(r.at), s.memory + off, r.size);
        off += r.size;
    }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

void SetPtr(std::uint32_t at, const void* p) { SetLong(At(at), static_cast<std::int32_t>(Address(p))); }

// What every round keeps inside its arrays: the member count (every loop
// runs to it), each member's CharacterRecord index, the hooks.
void Fix() {
    B(at::kMembers) = static_cast<unsigned char>(Next() % 5);
    for (unsigned i = 0; i < 5; ++i) Member(i)[at::kRecord] = static_cast<unsigned char>(Next() % 10);
    SetPtr(at::kHookEnd, reinterpret_cast<const void*>(&StubHookEnd));
    SetPtr(at::kHookExit, reinterpret_cast<const void*>(&StubHookExit));
    SetPtr(at::kHookEvent, reinterpret_cast<const void*>(&StubHookEvent));
}

unsigned Pick(const unsigned* values, unsigned n) { return values[Next() % n]; }

void Seed(unsigned k) {
    Sprite_Current = At(0x50000u + (Next() & 0xFFF0u));
    const Clone& c = kClones[k];
    if (c.dispatch >= 0) {
        const DataTable& t = kTables[c.dispatch];
        B(t.state) = static_cast<unsigned char>(Next() % t.entries);
        return;
    }
    const unsigned count = B(at::kMembers);
    switch (k) {
    case kCheckK: {
        if (Often()) B(at::kExtraRound) = 0;
        if (Often()) B(at::kTimer) = 0;
        if (Half()) B(at::kSideMode) = 2;
        break;
    }
    case kChainK: {
        static const unsigned kStates[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0xFF};
        B(at::kState2) = static_cast<unsigned char>(Pick(kStates, 16));
        if (Half()) SetWord(At(at::kPending), 0);
        else if (Half()) SetWord(At(at::kPending), 0x100u << (Next() % 8));
        if (Half()) B(at::kRoundFlags) |= 4;
        break;
    }
    case kNextK: {
        for (unsigned i = 0; i < 8; ++i) SetLong(At(at::kBusy + i * 4), 0);
        if (Next() % 4 == 0) {
            const unsigned i = Next() % 8;
            SetLong(At(at::kBusy + i * 4), static_cast<std::int32_t>(Half() ? 1u << (Next() % 32) : Next()));
        }
        if (Half()) B(at::kEventBattle) = 0;
        if (Half()) B(at::kBattleEnd) = 0;
        if (Half()) B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) ^ 0x10);
        break;
    }
    case kStartK: {
        static const unsigned kCursorAt[] = {0, 0, 1, 2, 3, 4, 5};
        B(at::kCursor) = static_cast<unsigned char>(Half() ? Next() % (count + 1) : Pick(kCursorAt, 7));
        for (unsigned i = 0; i < 5; ++i) {
            unsigned char* const m = Member(i);
            if (Often()) m[at::kFlags] = static_cast<unsigned char>((m[at::kFlags] & ~3u) | (Half() ? 0 : Next() % 4));
            if (Half()) m[at::kCharId] = 4;
            else if (Half()) m[at::kCharId] = static_cast<unsigned char>(Next() % 9);
            if (Half()) m[at::kFlags2 + 1] &= 0xDF;
        }
        break;
    }
    case kAwaitK: {
        for (unsigned i = 0; i < 5; ++i) {
            unsigned char* const m = Member(i);
            if (Often()) m[at::kFlags2 + 1] &= 0xDF;
            if (Half()) m[at::kStatus + 1] ^= 8;
        }
        if (Often()) B(at::kCursor) = static_cast<unsigned char>(Half() ? count : Next() % 6);
        static const unsigned kEnd[] = {0, 1, 2, 3, 0xFC, 0xFE, 0xFD, 0xFF};
        if (Often()) B(at::kBattleEnd) = static_cast<unsigned char>(Pick(kEnd, 8));
        if (Half()) B(at::kEventBattle) = 0;
        static const unsigned kStep[] = {0, 1, 2, 3};
        if (Half()) B(at::kState1) = static_cast<unsigned char>(Pick(kStep, 4));
        break;
    }
    case kWinK:
        for (unsigned i = 0; i < 3; ++i) Member(i)[0] = static_cast<unsigned char>(Member(i)[0] ^ (Half() ? 1 : 0));
        if (Half()) B(at::kMusicFlags) ^= 0x40;
        break;
    case kInputK:
    case kExitHookK:
        if (Half()) SetWord(At(at::kInput), 0);
        else if (Half()) SetWord(At(at::kInput), 0x100u << (Next() % 8));
        if (Half()) B(at::kMusicFlags) ^= 0x40;
        if (Half()) B(at::kMusicTrack) = 0xFF;
        break;
    case kFinishK:
        for (unsigned i = 0; i < 5; ++i) {
            unsigned char* const m = Member(i);
            if (Half()) m[at::kStatus + 1] ^= 0x40;
            unsigned char* const r = At(at::kRecords + m[at::kRecord] * at::kRecordStride);
            static const unsigned kCounts[] = {0, 3, 4, 4, 5, 5, 6, 0xFF};
            if (Often()) r[0x1E] = static_cast<unsigned char>(Pick(kCounts, 8));
        }
        break;
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[170];
    unsigned extra[4], chain_waits, chain_done, next_waits, next_end, next_round, started, no_slot, await_back, await_go,
        await_win, await_other, finish_raised;
} g_cover;

void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 170) ++g_cover.logged[out.log[i].what];
    const auto byte = [](const State& s, std::uint32_t address) -> unsigned {
        unsigned off = 0;
        for (const Region& r : kRegions) {
            if (address >= r.at && address < r.at + r.size) return s.memory[off + address - r.at];
            off += r.size;
        }
        return 0;
    };
    switch (k) {
    case kCheckK: ++g_cover.extra[byte(out, at::kExtraRound) & 3]; break;
    case kChainK:
        if (byte(out, at::kState1) != byte(in, at::kState1)) ++g_cover.chain_done;
        if (out.log_n == 0 && byte(out, at::kState2) != byte(in, at::kState2)) ++g_cover.chain_waits;
        break;
    case kNextK:
        if (byte(out, at::kPhase) == 5 && byte(in, at::kPhase) != 5) ++g_cover.next_end;
        else if (byte(out, at::kPhase) == 1 && byte(in, at::kPhase) != 1) ++g_cover.next_round;
        else if (byte(out, at::kPhase) == byte(in, at::kPhase)) ++g_cover.next_waits;
        break;
    case kStartK:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 15) ++g_cover.started;
        break;
    case kAwaitK:
        if (byte(out, at::kState2) + 1 == byte(in, at::kState2)) ++g_cover.await_back;
        if (byte(out, at::kState2) == 0 && byte(in, at::kState2) != 0) ++g_cover.await_go;
        if (out.log_n && byte(out, at::kState1) == 1) ++g_cover.await_win;
        if (out.log_n && byte(out, at::kState1) == 2) ++g_cover.await_other;
        break;
    case kFinishK:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 29) ++g_cover.finish_raised;
        break;
    default:
        break;
    }
}

using Fn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1500;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_turn_steps: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    for (unsigned t = 0; t < kTableCount; ++t)
        for (unsigned s = 0; s < kTables[t].entries; ++s)
            if (static_cast<std::uint32_t>(Long(At(kTables[t].at + 4 * s))) != kTableHeld[t][s])
                bof3::Fatal("battle_turn_steps: table 0x%X entry %u holds 0x%X, not 0x%X", static_cast<unsigned>(kTables[t].at), s,
                            static_cast<unsigned>(Long(At(kTables[t].at + 4 * s))), static_cast<unsigned>(kTableHeld[t][s]));

    // A page for task slot 0xFF's owner, where it lies: at start-up 0x940000.. is
    // reserved but not committed, so the page is committed for the run and
    // decommitted after (a free range would be reserved and released).
    MEMORY_BASIC_INFORMATION ff_info = {};
    bool ff_query = VirtualQuery(reinterpret_cast<void*>(0x9423FC), &ff_info, sizeof ff_info) == sizeof ff_info;
    const DWORD ff_was = ff_query ? ff_info.State : 0;
    void* ff_page = nullptr;
    if (ff_was == MEM_RESERVE)
        ff_page = VirtualAlloc(reinterpret_cast<void*>(0x942000), 0x1000, MEM_COMMIT, PAGE_READWRITE);
    else if (ff_was == MEM_FREE)
        ff_page = VirtualAlloc(reinterpret_cast<void*>(0x942000), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    ff_query = VirtualQuery(reinterpret_cast<void*>(0x9423FC), &ff_info, sizeof ff_info) == sizeof ff_info;
    const bool ff_writable = ff_query && ff_info.State == MEM_COMMIT &&
                             (ff_info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    g_slot_ff = ff_writable;
    bof3::Log("shadow      battle_turn_steps: 0x9423FC (task slot 0xFF's owner) was state 0x%lX, is %s, base %p",
              static_cast<unsigned long>(ff_was),
              ff_page ? "a page committed for the run" : ff_writable ? "already committed and writable" : "not writable",
              ff_query ? ff_info.AllocationBase : nullptr);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[12];
        if (c.n_calls > 12) bof3::Fatal("battle_turn_steps: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.table.entries) move_script::Relocate(clones[k], c.base, c.size, c.table);
    }

    static State saved, input, their_out, our_out;
    std::uint32_t saved_tables[kTableCount][5];
    for (unsigned t = 0; t < kTableCount; ++t)
        for (unsigned s = 0; s < kTables[t].entries; ++s) {
            saved_tables[t][s] = static_cast<std::uint32_t>(Long(At(kTables[t].at + 4 * s)));
            SetPtr(kTables[t].at + 4 * s, kTables[t].slots[s]);
        }
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
        Fix();
        g_seed = Next();
        Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            reinterpret_cast<Fn>(const_cast<void*>(fn))(0, 0, 0, 0);
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
                bof3::Log("shadow      battle_turn_steps self-test MISMATCH: round %u, %s, log %u / %u, first differing "
                          "state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    Apply(saved);
    for (unsigned t = 0; t < kTableCount; ++t)
        for (unsigned s = 0; s < kTables[t].entries; ++s)
            SetLong(At(kTables[t].at + 4 * s), static_cast<std::int32_t>(saved_tables[t][s]));
    if (ff_page) {
        if (ff_was == MEM_RESERVE) VirtualFree(ff_page, 0x1000, MEM_DECOMMIT);
        else VirtualFree(ff_page, 0, MEM_RELEASE);
    }

    bof3::Log("shadow      battle_turn_steps self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the party records and window bytes, the character records and battle globals, the "
              "second party array, the task slots and enemies, Sprite_Current, Game_Step, Input_Pressed and the "
              "stand-ins' log (with the watched state at each call) compared; task slot 0xFF %s",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad,
              g_slot_ff ? "answered" : "never answered (0x9423FC not writable)");
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_turn_steps: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned slots = 0, slots_all = 0;
    for (unsigned t = 0; t < kTableCount; ++t)
        for (unsigned s = 0; s < kTables[t].entries; ++s) {
            ++slots_all;
            slots += c.logged[100 + t * 8 + s] ? 1u : 0u;
        }
    unsigned steps = 0;
    for (unsigned i = 3; i < 12; ++i) steps += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      battle_turn_steps coverage: table slots %u of %u; extra round 0/1/2/3 %u %u %u %u, faster checks "
              "%u, ticks %u; chain steps %u of 9 (%u calls), waits passed %u, chains done %u; next round: waited %u, battle "
              "over %u, next round %u, event hooks %u, files %u, out tests %u; tasks started %u; await: back %u, done %u "
              "(win %u, other %u), status clears %u, tints %u, items %u, end hooks %u; music %u / %u, windows %u, "
              "messages %u, load waits %u; write-backs %u, exit hooks %u; finish: records raised %u, clears %u, reloads %u",
              slots, slots_all, c.extra[0], c.extra[1], c.extra[2], c.extra[3], c.logged[1], c.logged[2], steps,
              c.logged[3] + c.logged[4] + c.logged[5] + c.logged[6] + c.logged[7] + c.logged[8] + c.logged[9] +
                  c.logged[10] + c.logged[11],
              c.chain_waits, c.chain_done, c.next_waits, c.next_end, c.next_round, c.logged[31], c.logged[12], c.logged[14],
              c.started, c.await_back, c.await_go, c.await_win, c.await_other, c.logged[16], c.logged[17], c.logged[18],
              c.logged[32], c.logged[20], c.logged[21], c.logged[22], c.logged[23], c.logged[24], c.logged[25], c.logged[33],
              c.finish_raised, c.logged[27], c.logged[30]);
    if (bad) bof3::Fatal("the per-turn steps differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_turn_steps

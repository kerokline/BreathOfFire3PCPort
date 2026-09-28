// BOF3X_SHADOW=boss_sb: group BSB's 52 boss functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per kind (3, 4,
// 5, 8, 9, 10, 11) and per fight (4, 5, 6, 7, 13, 8, 9, 10).
// BOF3X_BSB_RUN=<run> runs one (the controls script's shortcut). docs/boss_sb.md
// section 3.
//
// The clone rows are tools/boss_rows.py's (--unit <unit> --clones,
// 2026-09-28), each read against the disassembly. The kinds' dispatchers are
// kDispatch with their state byte drawn below their table; their .data
// tables are DataTables (a recorder per entry), the +0xF4 hook tables with one
// argument word. The set-ups are the harness's kSetup, their hooks kEvent /
// kEnd / kExit; this group is the first to use kSetup.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sb.h"
#include "game/boss_sb_callees.h"
#include "game/move_script_bytes.h"

namespace boss_sb {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using move_script::SetWord;
using S = bh::Shape;

#define BSB_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BSB_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BSB_FN(name) reinterpret_cast<const void*>(&::name)

// A clone row: name, base, size, calls, jump tables, ours, answer, shape, and
// for a dispatcher its state byte and table size.
bh::Clone Row(const char* name, U base, U size, const bh::CallSite* calls, int n_calls, const bh::JumpTable* tables,
              int n_tables, const void* ours, U ret, S shape, std::uint8_t state_at = 1, std::uint8_t states = 0) {
    bh::Clone c{name, base, size, calls, n_calls, nullptr, 0, tables, n_tables, ours, ret, false, shape};
    c.state_at = state_at;
    c.states = states;
    return c;
}

// The cells outside the standard regions the group's functions write.
const bh::Region kRegions[] = {
    {at::kLeaderPick, 1}, {at::kScriptVar3, 1}, {at::kWindow4Flag, 1}, {at::kDrawPassFlags, 1}, {at::kMusicTrack, 1},
};

// ===========================================================================
// Seeds shared by the kinds and the set-ups
// ===========================================================================

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }

// The battle-end byte: its bits 0, 1, 2 and 3 each way.
void SeedBattleEnd() {
    Mem(at::kBattleEnd)[0] = Byte(bh::Often() ? BH_PICK(0, 1, 2, 3, 4, 5, 8, 9, 0xFC, 0xFD, 0xFE, 0xFF) : bh::Next());
}

// The event hooks' reads: the actor, the command kind, the command's id (the
// word +2 of [0x904B40], pointed into one of the harness's records), the
// round flags' bit 0x40, the target, 0x904AAD bit 0, the leader's HP, the
// battle-end byte's bit 2, the countdown 0x904AA5.
void SeedEvent() {
    Mem(at::kActor)[0] = Byte(bh::Often() ? 0 : BH_PICK(1, 2, 3, 0x80, 0x100));
    Mem(at::kCommandKind)[0] = Byte(bh::Often() ? 4 : BH_PICK(3, 5, 0x84, 0, 0x44));
    unsigned char* const command = bh::SpriteRecord(bh::Next()) + 4 * (bh::Next() % 16);
    bh::SetPointer(at::kCommand, command);
    SetWord(command + 2, bh::Often() ? 0x78 : bh::Half() ? BH_PICK(0x77, 0x79, 0x178, 0xF8, 0x7078, 0x178, 0x7078, 0) : bh::Next() & 0xFFFF);
    Mem(0x904AA8)[0] = Byte(bh::Half() ? bh::Next() | 0x40 : bh::Next() & ~0x40u);
    Mem(at::kTarget)[0] = Byte(bh::Half() ? 0 : BH_PICK(1, 3, 0x40, 0x80, 0xFF));
    Mem(at::kPoseBits)[0] = Byte(bh::Half() ? bh::Next() | 1 : bh::Next() & ~1u);
    SetWord(Mem(at::kLeaderHp), bh::Half() ? 0 : BH_PICK(1, 0x100, 0x8000, 0xFFFF));
    Mem(at::kBattleEnd)[0] = Byte(bh::Half() ? bh::Next() | 4 : bh::Next() & ~4u);
    Mem(at::kCountdown)[0] = Byte(bh::Often() ? BH_PICK(1, 1, 0, 2, 0x3C, 0x80, 0xFF) : bh::Next());
}

// An event hook's phase code: 0..6 two times in three (past 6 otherwise),
// with garbage above the byte half the time (the hook reads the low byte).
void EventArgs(U* a) {
    const U low = bh::Often() ? bh::Next() % 7 : 7 + bh::Next() % 249;
    a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | low;
}
// An enemy hook's word: 0..2 with garbage above the byte half the time.
void HookArgs(U* a) { a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Next() % 3); }

// What the exit and step functions read again after a call that the standard
// disturbance does not move: enemy 0 and 1's pose words and enemy 1's place
// (read after the spawn helpers), the fight byte at 5 (kind 3's step).
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 4) {
    case 0: Mem(at::kEnemy0Pose + (h >> 24) % 4)[0] = b; break;
    case 1: Mem(at::kEnemy1Pose + (h >> 24) % 4)[0] = b; break;
    case 2: Mem(at::kEnemy1Place + (h >> 24) % 12)[0] = b; break;
    default: Mem(at::kFight)[0] = h & 0x1000000 ? 5 : Byte(4 + (h >> 25) % 3); break;
    }
}

// ===========================================================================
// The kinds
// ===========================================================================

constexpr bh::CallSite kCallsTickAt38[] = {{0x38, 0x5893A0}};

// Kind 3: fight 5 (the one of its three fights its step compares against).
constexpr bh::CallSite kCalls438330[] = {{0x2, 0x4358D0}};
constexpr bh::CallSite kCalls438350[] = {{0x1F, 0x589590}, {0x6B, 0x5720C0}, {0x8C, 0x5891F0}, {0xC4, 0x5893A0}};
const bh::Clone kClonesK03[] = {
    Row("BossEngineer_Dispatch", 0x438290, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossEngineer_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossEngineer_Enter", 0x4382B0, 0x3D, kCallsTickAt38, 1, nullptr, 0, BSB_FN(BossEngineer_Enter), 0xFF, S::kState),
    Row("BossEngineer_ActDispatch", 0x4382F0, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossEngineer_ActDispatch), 0xFF, S::kDispatch, 2, 6),
    Row("BossEngineer_HitDispatch", 0x438310, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossEngineer_HitDispatch), 0xFF, S::kDispatch, 2, 3),
    Row("BossEngineer_HitStart", 0x438330, 0x1C, kCalls438330, BSB_N(kCalls438330), nullptr, 0, BSB_FN(BossEngineer_HitStart), 0, S::kState),
    Row("BossEngineer_HitStep", 0x438350, 0xC9, kCalls438350, BSB_N(kCalls438350), nullptr, 0, BSB_FN(BossEngineer_HitStep), 0xFF, S::kState),
    Row("BossEngineer_Hook", 0x438420, 0x10, nullptr, 0, nullptr, 0, BSB_FN(BossEngineer_Hook), 0xFF, S::kEnemyHook),
};
enum : unsigned { kK03Dispatch, kK03Enter, kK03Act, kK03Hit, kK03HitStart, kK03HitStep, kK03Hook };
static_assert(kK03Hook + 1 == BSB_COUNT(kClonesK03), "kind 3's seeding indices");
const bh::DataTable kTablesK03[] = {{0x64C984, 12}, {0x64C9B4, 6}, {0x64C9CC, 3}, {0x64C9D8, 3, 4, 1}};

void SeedK03(unsigned k) {
    unsigned char* const s = Sprite_Current;
    if (k != kK03HitStep) return;
    // the count at 1 (it reaches 0), at 0 (it wraps), and beside
    s[9] = Byte(bh::Often() ? BH_PICK(1, 1, 1, 0, 2, 0x3D) : bh::Next());
    Mem(at::kFight)[0] = Byte(bh::Half() ? 5 : BH_PICK(4, 6, 0x85, 0x15));
    // the shift count +5: the byte's bits 0..7, 8 and past, and the 5-bit mask
    s[5] = Byte(bh::Often() ? bh::Next() % 9 : BH_PICK(0x1F, 0x20, 0x21, 0x27, 0xFF, 0xE3));
}
void ArgsK03(unsigned k, U* a) {
    if (k == kK03Hook) HookArgs(a);
}

// Kinds 4 and 5: fight 7.
const bh::Clone kClonesK04[] = {
    Row("BossWorker_Dispatch", 0x438B30, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossWorker_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossWorker_Enter", 0x438B50, 0x3D, kCallsTickAt38, 1, nullptr, 0, BSB_FN(BossWorker_Enter), 0xFF, S::kState),
    Row("BossWorker_Hook", 0x438B90, 0x10, nullptr, 0, nullptr, 0, BSB_FN(BossWorker_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK04[] = {{0x64CA0C, 12}, {0x64CA3C, 3, 4, 1}};

constexpr bh::CallSite kCalls438BC0[] = {{0x42, 0x5893A0}};
const bh::Clone kClonesK05[] = {
    Row("BossOperator_Dispatch", 0x438BA0, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossOperator_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossOperator_Enter", 0x438BC0, 0x47, kCalls438BC0, BSB_N(kCalls438BC0), nullptr, 0, BSB_FN(BossOperator_Enter), 0xFF, S::kState),
    Row("BossOperator_Hook", 0x438C10, 0x10, nullptr, 0, nullptr, 0, BSB_FN(BossOperator_Hook), 0xFF, S::kEnemyHook),
    Row("BossOperator_HookHit", 0x438C20, 0xC, nullptr, 0, nullptr, 0, BSB_FN(BossOperator_HookHit), 0, S::kEnemyHook),
};
enum : unsigned { kK05Dispatch, kK05Enter, kK05Hook, kK05Hit };
static_assert(kK05Hit + 1 == BSB_COUNT(kClonesK05), "kind 5's seeding indices");
const bh::DataTable kTablesK05[] = {{0x64CA48, 12}, {0x64CA78, 3, 4, 1}};

void SeedK05(unsigned k) {
    // the target block: one of the harness's records, anywhere in it
    if (k == kK05Hit) bh::SetPointer(at::kTargetBlock, bh::SpriteRecord(bh::Next()) + 2 * (bh::Next() % 0x40));
}
// The kinds 4, 5 and 8..11 runs: every hook clone's word with garbage above it
// (the other clones take no argument, so the word is theirs to ignore).
void ArgsKind(unsigned, U* a) { HookArgs(a); }

// Kinds 8..11: fight 8 (the byte is not read by their code).
const bh::Clone kClonesK08[] = {
    Row("BossTorast_Dispatch", 0x438E50, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossTorast_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossTorast_Enter", 0x438E70, 0x3D, kCallsTickAt38, 1, nullptr, 0, BSB_FN(BossTorast_Enter), 0xFF, S::kState),
    Row("BossTorast_Hook", 0x4390E0, 0x10, nullptr, 0, nullptr, 0, BSB_FN(BossTorast_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK08[] = {{0x64CAA4, 12}, {0x64CB04, 3, 4, 1}};
const bh::Clone kClonesK09[] = {
    Row("BossKassen_Dispatch", 0x4390F0, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossKassen_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossKassen_Enter", 0x439110, 0x3D, kCallsTickAt38, 1, nullptr, 0, BSB_FN(BossKassen_Enter), 0xFF, S::kState),
    Row("BossKassen_Hook", 0x439150, 0x10, nullptr, 0, nullptr, 0, BSB_FN(BossKassen_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK09[] = {{0x64CB10, 12}, {0x64CB40, 3, 4, 1}};
const bh::Clone kClonesK10[] = {
    Row("BossGaltel_Dispatch", 0x439160, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossGaltel_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossGaltel_Enter", 0x439180, 0x3D, kCallsTickAt38, 1, nullptr, 0, BSB_FN(BossGaltel_Enter), 0xFF, S::kState),
    Row("BossGaltel_Hook", 0x4391C0, 0x10, nullptr, 0, nullptr, 0, BSB_FN(BossGaltel_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK10[] = {{0x64CB4C, 12}, {0x64CB7C, 3, 4, 1}};
const bh::Clone kClonesK11[] = {
    Row("BossDoksen_Dispatch", 0x4391D0, 0x12, nullptr, 0, nullptr, 0, BSB_FN(BossDoksen_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossDoksen_Enter", 0x4391F0, 0x3D, kCallsTickAt38, 1, nullptr, 0, BSB_FN(BossDoksen_Enter), 0xFF, S::kState),
    Row("BossDoksen_Hook", 0x439230, 0x10, nullptr, 0, nullptr, 0, BSB_FN(BossDoksen_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK11[] = {{0x64CB88, 12}, {0x64CBB8, 3, 4, 1}};

// ===========================================================================
// The set-ups
// ===========================================================================

// A set-up run's clones: which shape each is, for the shared seeds.
const bh::Clone* g_clones = nullptr;
void SeedSetup(unsigned k) {
    switch (g_clones[k].shape) {
    case S::kEvent: SeedEvent(); break;
    case S::kEnd:
        SeedBattleEnd();
        // the chapter's step at the wrap and beside
        Mem(at::kChapterStep)[0] = Byte(bh::Often() ? BH_PICK(0xFF, 0, 1, 0x31, 0x32) : bh::Next());
        break;
    case S::kExit: SeedBattleEnd(); break;
    default: break;
    }
}
void ArgsSetup(unsigned k, U* a) {
    if (g_clones[k].shape == S::kEvent) EventArgs(a);
}

constexpr bh::JumpTable kEventTable[] = {{0x15, 0xE8, 7}};   // the three kind-3 event hooks: the same offsets
constexpr bh::CallSite kCallsEndChapter[] = {{0x1C, 0x446E20}, {0x2F, 0x446E20}};
constexpr bh::CallSite kCallsExitPair[] = {{0xB, 0x4949D0}, {0x12, 0x4949D0}, {0x1B, 0x4949D0}, {0x29, 0x4949F0},
                                           {0x30, 0x494920}, {0x3C, 0x589590}, {0x56, 0x5891F0}, {0x7E, 0x4949D0},
                                           {0x8C, 0x4949F0}, {0x93, 0x494920}, {0x9F, 0x589590}, {0xB9, 0x5891F0}};
// Set-up 5's exit hook: a push ebx before its pairs moves each site by three.
constexpr bh::CallSite kCalls4387C0[] = {{0xB, 0x4949D0}, {0x12, 0x4949D0}, {0x1E, 0x4949D0}, {0x2C, 0x4949F0},
                                         {0x33, 0x494920}, {0x3F, 0x589590}, {0x59, 0x5891F0}, {0x81, 0x4949D0},
                                         {0x8F, 0x4949F0}, {0x96, 0x494920}, {0xA2, 0x589590}, {0xBA, 0x5891F0}};

const bh::Clone kClonesB04[] = {
    Row("Boss04_Setup", 0x438430, 0x1F, nullptr, 0, nullptr, 0, BSB_FN(Boss04_Setup), 0, S::kSetup),
    Row("Boss04_Event", 0x438450, 0x104, nullptr, 0, kEventTable, 1, BSB_FN(Boss04_Event), 0xFF, S::kEvent),
    Row("Boss04_Exit", 0x438560, 0xE3, kCallsExitPair, BSB_N(kCallsExitPair), nullptr, 0, BSB_FN(Boss04_Exit), 0, S::kExit),
    Row("Boss04_End", 0x4389E0, 0x34, kCallsEndChapter, BSB_N(kCallsEndChapter), nullptr, 0, BSB_FN(Boss04_End), 0, S::kEnd),
};
const bh::Clone kClonesB05[] = {
    Row("Boss05_Setup", 0x438650, 0x1F, nullptr, 0, nullptr, 0, BSB_FN(Boss05_Setup), 0, S::kSetup),
    Row("Boss05_Event", 0x438670, 0x104, nullptr, 0, kEventTable, 1, BSB_FN(Boss05_Event), 0xFF, S::kEvent),
    Row("Boss05_End", 0x438780, 0x34, kCallsEndChapter, BSB_N(kCallsEndChapter), nullptr, 0, BSB_FN(Boss05_End), 0, S::kEnd),
    Row("Boss05_Exit", 0x4387C0, 0xE5, kCalls4387C0, BSB_N(kCalls4387C0), nullptr, 0, BSB_FN(Boss05_Exit), 0, S::kExit),
};
const bh::Clone kClonesB06[] = {
    Row("Boss06_Setup", 0x4388B0, 0x1F, nullptr, 0, nullptr, 0, BSB_FN(Boss06_Setup), 0, S::kSetup),
    Row("Boss06_Event", 0x4388D0, 0x104, nullptr, 0, kEventTable, 1, BSB_FN(Boss06_Event), 0xFF, S::kEvent),
    Row("Boss06_Exit", 0x438A20, 0x10E, kCallsExitPair, BSB_N(kCallsExitPair), nullptr, 0, BSB_FN(Boss06_Exit), 0, S::kExit),
};

constexpr bh::CallSite kCalls438C50[] = {{0x96, 0x5171A0}, {0xA8, 0x44A650}, {0xC0, 0x5891F0}};
constexpr bh::JumpTable kTables438C50[] = {{0x16, 0x158, 7}};
constexpr bh::CallSite kCalls438DD0[] = {{0x4D, 0x446E20}};
constexpr bh::CallSite kCalls438E30[] = {{0x2, 0x4949D0}, {0x9, 0x4949D0}};
const bh::Clone kClonesB07[] = {
    Row("Boss07_Setup", 0x438C30, 0x1F, nullptr, 0, nullptr, 0, BSB_FN(Boss07_Setup), 0, S::kSetup),
    Row("Boss07_Event", 0x438C50, 0x174, kCalls438C50, BSB_N(kCalls438C50), kTables438C50, 1, BSB_FN(Boss07_Event), 0xFF, S::kEvent),
    Row("Boss07_End", 0x438DD0, 0x52, kCalls438DD0, BSB_N(kCalls438DD0), nullptr, 0, BSB_FN(Boss07_End), 0, S::kEnd),
    Row("Boss07_Exit", 0x438E30, 0x12, kCalls438E30, BSB_N(kCalls438E30), nullptr, 0, BSB_FN(Boss07_Exit), 0, S::kExit),
};

constexpr bh::CallSite kCalls43A000[] = {{0x10, 0x446DE0}, {0x15, 0x446E20}};
const bh::Clone kClonesB13[] = {
    Row("Boss13_Setup", 0x439FE0, 0x1F, nullptr, 0, nullptr, 0, BSB_FN(Boss13_Setup), 0, S::kSetup),
    Row("Boss13_End", 0x43A000, 0x22, kCalls43A000, BSB_N(kCalls43A000), nullptr, 0, BSB_FN(Boss13_End), 0, S::kEnd),
};

constexpr bh::CallSite kCallsEndVar[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
constexpr bh::CallSite kCalls439280[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x18, 0x589590}, {0x1F, 0x5891F0}};
constexpr bh::CallSite kCallsExitFlip[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x18, 0x589590}, {0x28, 0x5891F0}};
const bh::Clone kClonesB08[] = {
    Row("Boss08_Setup", 0x439240, 0x1F, nullptr, 0, nullptr, 0, BSB_FN(Boss08_Setup), 0, S::kSetup),
    Row("Boss08_End", 0x439260, 0x1A, kCallsEndVar, BSB_N(kCallsEndVar), nullptr, 0, BSB_FN(Boss08_End), 0, S::kEnd),
    Row("Boss08_Exit", 0x439280, 0x48, kCalls439280, BSB_N(kCalls439280), nullptr, 0, BSB_FN(Boss08_Exit), 0, S::kExit),
};
const bh::Clone kClonesB09[] = {
    Row("Boss09_Setup", 0x4392D0, 0x1F, nullptr, 0, nullptr, 0, BSB_FN(Boss09_Setup), 0, S::kSetup),
    Row("Boss09_End", 0x4392F0, 0x1A, kCallsEndVar, BSB_N(kCallsEndVar), nullptr, 0, BSB_FN(Boss09_End), 0, S::kEnd),
    Row("Boss09_Exit", 0x439310, 0x52, kCallsExitFlip, BSB_N(kCallsExitFlip), nullptr, 0, BSB_FN(Boss09_Exit), 0, S::kExit),
};
const bh::Clone kClonesB10[] = {
    Row("Boss10_Setup", 0x439370, 0x1F, nullptr, 0, nullptr, 0, BSB_FN(Boss10_Setup), 0, S::kSetup),
    Row("Boss10_End", 0x439390, 0x1A, kCallsEndVar, BSB_N(kCallsEndVar), nullptr, 0, BSB_FN(Boss10_End), 0, S::kEnd),
    Row("Boss10_Exit", 0x4393B0, 0x52, kCallsExitFlip, BSB_N(kCallsExitFlip), nullptr, 0, BSB_FN(Boss10_Exit), 0, S::kExit),
};

// BOF3X_BSB_RUN=<run> runs that one alone; unset, all of them.
bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BSB_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

constexpr unsigned kRounds = 8000;

void RunKind(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables,
             void (*seed)(unsigned), void (*args)(unsigned, U*), int fight, int kind) {
    if (!Wants(run)) return;
    bh::Group g{"boss_sb", clones, n, nullptr, 0, tables, n_tables, kRegions, BSB_COUNT(kRegions), seed, &Disturb, kRounds};
    g.args = args;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
}
void RunSetup(const char* run, const bh::Clone* clones, unsigned n, int fight) {
    if (!Wants(run)) return;
    g_clones = clones;
    bh::Group g{"boss_sb", clones, n, nullptr, 0, nullptr, 0, kRegions, BSB_COUNT(kRegions), &SeedSetup, &Disturb, kRounds};
    g.args = &ArgsSetup;
    g.fight = fight;
    bh::Run(g);
    g_clones = nullptr;
}

void NoSeed(unsigned) {}

}  // namespace

void SelfTest() {
    RunKind("k03", kClonesK03, BSB_COUNT(kClonesK03), kTablesK03, BSB_COUNT(kTablesK03), &SeedK03, &ArgsK03, 5, 3);
    RunKind("k04", kClonesK04, BSB_COUNT(kClonesK04), kTablesK04, BSB_COUNT(kTablesK04), &NoSeed, &ArgsKind, 7, 4);
    RunKind("k05", kClonesK05, BSB_COUNT(kClonesK05), kTablesK05, BSB_COUNT(kTablesK05), &SeedK05, &ArgsKind, 7, 5);
    RunKind("k08", kClonesK08, BSB_COUNT(kClonesK08), kTablesK08, BSB_COUNT(kTablesK08), &NoSeed, &ArgsKind, 8, 8);
    RunKind("k09", kClonesK09, BSB_COUNT(kClonesK09), kTablesK09, BSB_COUNT(kTablesK09), &NoSeed, &ArgsKind, 8, 9);
    RunKind("k10", kClonesK10, BSB_COUNT(kClonesK10), kTablesK10, BSB_COUNT(kTablesK10), &NoSeed, &ArgsKind, 8, 10);
    RunKind("k11", kClonesK11, BSB_COUNT(kClonesK11), kTablesK11, BSB_COUNT(kTablesK11), &NoSeed, &ArgsKind, 8, 11);
    RunSetup("b04", kClonesB04, BSB_COUNT(kClonesB04), 4);
    RunSetup("b05", kClonesB05, BSB_COUNT(kClonesB05), 5);
    RunSetup("b06", kClonesB06, BSB_COUNT(kClonesB06), 6);
    RunSetup("b07", kClonesB07, BSB_COUNT(kClonesB07), 7);
    RunSetup("b13", kClonesB13, BSB_COUNT(kClonesB13), 13);
    RunSetup("b08", kClonesB08, BSB_COUNT(kClonesB08), 8);
    RunSetup("b09", kClonesB09, BSB_COUNT(kClonesB09), 9);
    RunSetup("b10", kClonesB10, BSB_COUNT(kClonesB10), 10);
}

}  // namespace boss_sb

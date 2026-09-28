// BOF3X_SHADOW=boss_se: group BSE's 52 functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per fight and per
// kind - thirteen (BOF3X_BSE_RUN=b22|k28|b23|b30|k29|k55|b24|b48|k30|k31|k32|
// b25|b26 runs one). docs/boss_se.md section 3.
//
// The clone rows are tools/boss_rows.py's (--unit <unit> --clones,
// 2026-09-28), each read against the disassembly; the tables' entry counts are
// the code's (the dispatchers' state values, the next table's address), not
// the tool's extents. Every function is called the way its root calls it
// (Clone::shape): a set-up as Boss_SetupTable's jmp, a hook with its phase code
// or word, a dispatcher with its state byte drawn below its table.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_se.h"
#include "game/boss_se_callees.h"
#include "game/move_script_bytes.h"

namespace boss_se {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

// A clone row: name, base, size, calls, the ours, the answer's mask, the shape.
bh::Clone Row(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, U ret, S shape,
              const bh::JumpTable* tables = nullptr, int n_tables = 0) {
    return bh::Clone{name, base, size, calls, n, nullptr, 0, tables, n_tables, ours, ret, false, shape};
}
// A dispatcher: by Sprite_Current[at], its table's entries drawn each round.
bh::Clone Disp(const char* name, U base, const void* ours, std::uint8_t at, std::uint8_t states) {
    bh::Clone c{name, base, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, ours, 0, false, S::kDispatch};
    c.state_at = at;
    c.states = states;
    return c;
}

// ===========================================================================
// What every run shares
// ===========================================================================

// Port_DroppedCall sits in every kind's +1 table (entries 1 and 10): a bare
// ret. Capcom's dispatcher jumps to it with its caller's stack, ours calls it
// with none - its "argument" is whatever is there, never read: listed with no
// arguments so none is compared. Battle_CopyEnemyData and Sprite_PoseFromSet
// read the low byte of the id and of the animation (battle_sprites.cpp,
// field_hidden.cpp: Sprite_SetFrameQueueUpload takes frame & 0xFF), and the
// originals push a register whose upper bytes are a callee's leftovers.
// Battle_RemoveFromTurnOrder is louder than the real one: Boss26_End reads the
// member's +0x90 and +8 after it, and the next member's +0 - the effect moves
// them. The two engine functions nobody owns: 0x437450 (the sound, a word) and
// 0x4376A0 (the action's end).
std::uint32_t RemoveEffect(const std::uint32_t*, std::uint32_t answer) {
    const U n = bh::Noise();
    unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(n % 3));
    switch ((n >> 4) % 3) {
    case 0: p[0x91] ^= 0x40; break;
    case 1: p[8] = static_cast<unsigned char>(n >> 8); break;
    default: p[0] ^= 1; break;
    }
    return answer;
}

const bh::Callee kCallees[] = {
    {"Port_DroppedCall", ::bof3::addr::Port_DroppedCall, KeyOf(&::Port_DroppedCall), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"Battle_CopyEnemyData", ::bof3::addr::Battle_CopyEnemyData, KeyOf(&::Battle_CopyEnemyData), 2, {kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"Sprite_PoseFromSet", ::bof3::addr::Sprite_PoseFromSet, KeyOf(&::Sprite_PoseFromSet), 3, {kU8, kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"Battle_RemoveFromTurnOrder", ::bof3::addr::Battle_RemoveFromTurnOrder, KeyOf(&::Battle_RemoveFromTurnOrder), 1, {kU8},
     bh::Answer::kGarbage, 0, 0, {}, &RemoveEffect},
    {"0x437450", at::kEnemySound, at::kEnemySound, 1, {kU16}, bh::Answer::kGarbage, 0, 0},
    {"0x4376A0", at::kEnemyActEnd, at::kEnemyActEnd, 0, {}, bh::Answer::kGarbage, 0, 0},
};

// The cells beyond the harness's battle frame the 52 read or write.
const bh::Region kRegions[] = {
    {at::kScriptVar3, 4},        // movement-script variable 3
    {at::kCarryHp, 8},           // the three words kinds 30 and 31 carry in
    {0x904160, 0x10},            // the party list's tail past Cond_Flags' region (0x904065 + 0xFF)
    {at::kLeaderPick, 4},
    {at::kMsgMode, 4},
    {at::kSavedAp, 4},           // set-up 25's two saved bytes
    {0x939A00, 0xD0},            // the copy's count and bytes, set-up 26's sums (to 0x939AD0, the harness's)
    {0x929EC0, 4},               // Field_MemberCount
    {0x7E0918, 4},               // Draw_PassFlags
    {0x66C810, 4},               // MoveScript_WaitWordDA
};

// The clones of the run in progress, for Args; the code Boss25_Event's seed
// picked for the round (a seed plants, Args hands it over: an args hook's
// writes to memory would be lost, docs/boss_harness.md section 6).
const bh::Clone* g_clones = nullptr;
U g_code = 0;

unsigned char& B(U address) { return Mem(address)[0]; }
U Above() { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0; }

// An enemy hook's word 0..2 (the three callers') with garbage above half the
// time - the dispatchers mask it, their entries get it whole; the event hooks'
// codes, the ones each reads at twice the others' rate.
void Args(unsigned k, U* a) {
    const bh::Clone& c = g_clones[k];
    if (c.shape == S::kEnemyHook) {
        a[0] = Above() | (bh::Next() % 3);
    } else if (c.base == 0x43BF00) {
        a[0] = Above() | g_code;
    } else if (c.base == 0x43C230) {
        a[0] = Above() | BH_PICK(0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 0xFF, 0x80);
    }
}

// What the 52 read again after a call that the standard disturbance does not
// move: the script bits 0x904AAD (Boss25_Event, Boss26_Event after the task
// and the messages), the copy's count (Boss25_Exit after Transition_Start), a
// party member's +0x90 / +8 / +0 (Boss26_End after the turn-order call),
// the round's count 0x904AE2, variable 3.
void Disturb(U h) {
    const U v = h >> 16;
    switch ((h >> 8) % 5) {
    case 0: B(at::kScript) = static_cast<unsigned char>(v); break;
    case 1: B(at::kPartyCopyCount) = static_cast<unsigned char>(v); break;
    case 2: bh::PartyOf(static_cast<unsigned char>(v % 3))[0x91] ^= 0x40; break;
    case 3: B(at::kRoundSlot) = static_cast<unsigned char>(v); break;
    default: B(at::kScriptVar3) = static_cast<unsigned char>(v); break;
    }
}

unsigned char BattleEndByte() { return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 3, 0xFD, 0x82, 8, 0xA) : bh::Next()); }

bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BSE_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

// The two event hooks read the script bits 0x904AAD again after a task or a
// message: the standard disturbance reaches them only through Disturb's one
// case in eighty, so for set-ups 25 and 26 a quarter of the disturbances also
// flip one of its bits (Noise: the same on both passes).
void SettleScript() {
    const U n = bh::Noise();
    if (n % 4 == 0) B(at::kScript) ^= static_cast<unsigned char>(1u << ((n >> 8) % 8));
}

void RunGroup(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables,
              void (*seed)(unsigned), int fight, int kind, unsigned rounds = 6000, void (*settle)() = nullptr) {
    if (!Wants(run)) return;
    g_clones = clones;
    bh::Group g{"boss_se", clones, n, kCallees, BH_COUNT(kCallees), tables, n_tables, kRegions, BH_COUNT(kRegions), seed,
                &Disturb, rounds};
    g.args = &Args;
    g.settle = settle;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
    g_clones = nullptr;
}

// ===========================================================================
// Set-up 22
// ===========================================================================

constexpr bh::CallSite kCalls43B5D0[] = {{0x30, 0x446E20}};
constexpr bh::CallSite kCalls43B610[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x18, 0x589590}, {0x32, 0x5891F0}};
const bh::Clone kClonesB22[] = {
    Row("Boss22_Setup", 0x43B5B0, 0x1F, nullptr, 0, BH_FN(Boss22_Setup), 0, S::kSetup),
    Row("Boss22_End", 0x43B5D0, 0x35, kCalls43B5D0, BH_N(kCalls43B5D0), BH_FN(Boss22_End), 0, S::kEnd),
    Row("Boss22_Exit", 0x43B610, 0x3B, kCalls43B610, BH_N(kCalls43B610), BH_FN(Boss22_Exit), 0, S::kExit),
};
void SeedB22(unsigned k) {
    if (k == 1) {
        B(at::kBattleEnd) = BattleEndByte();
        B(at::kMusicFlags) = static_cast<unsigned char>(bh::Often() ? BH_PICK(0x40, 0, 0xFF, 0xBF) : bh::Next());
    }
}

// ===========================================================================
// Kind 28 (Bully) and set-up 23
// ===========================================================================

constexpr bh::CallSite kCalls43B670[] = {{0x8A, 0x5893A0}};
const bh::Clone kClonesK28[] = {
    Disp("BossBully_Dispatch", 0x43B650, BH_FN(BossBully_Dispatch), 1, 12),
    Row("BossBully_Enter", 0x43B670, 0x8F, kCalls43B670, BH_N(kCalls43B670), BH_FN(BossBully_Enter), 0xFF, S::kState),
    Row("BossBully_Hook", 0x43B700, 0x10, nullptr, 0, BH_FN(BossBully_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK28[] = {{Key(BossBully_States), 12}, {Key(BossBully_Hooks), 3, 4, 1}};
void SeedK28(unsigned k) {
    if (k == 1) Sprite_Current[5] = static_cast<unsigned char>(bh::Often() ? BH_PICK(3, 4, 3, 4, 2, 5, 0, 0x83) : bh::Next());
}

constexpr bh::CallSite kCalls43B750[] = {{0x2, 0x494A60}, {0x9, 0x494A60}, {0x10, 0x494A60}};
const bh::Clone kClonesB23[] = {
    Row("Boss23_Setup", 0x43B710, 0x1F, nullptr, 0, BH_FN(Boss23_Setup), 0, S::kSetup),
    Row("BossHook_ExitClearActors012", 0x43B750, 0x19, kCalls43B750, BH_N(kCalls43B750), BH_FN(BossHook_ExitClearActors012), 0, S::kExit),
};

// ===========================================================================
// Set-up 30
// ===========================================================================

constexpr bh::CallSite kCalls43CFA0[] = {{0x1C, 0x446DE0}, {0x29, 0x446E00}};
const bh::Clone kClonesB30[] = {
    Row("Boss30_Setup", 0x43CF80, 0x1F, nullptr, 0, BH_FN(Boss30_Setup), 0, S::kSetup),
    Row("Boss30_End", 0x43CFA0, 0x2E, kCalls43CFA0, BH_N(kCalls43CFA0), BH_FN(Boss30_End), 0, S::kEnd),
};
void SeedEnd(unsigned k) {
    if (k == 1) B(at::kBattleEnd) = BattleEndByte();
    if (k == 1 && bh::Half()) B(at::kScriptVar3) = static_cast<unsigned char>(BH_PICK(0, 0xFF, 0x7F, 0x80, 0xC));
}

// ===========================================================================
// Kinds 29 (Stallion) and 55 (Sample10), set-ups 24 and 48
// ===========================================================================

constexpr bh::CallSite kCalls43B790[] = {{0x38, 0x5893A0}};
const bh::Clone kClonesK29[] = {
    Disp("BossStallion_Dispatch", 0x43B770, BH_FN(BossStallion_Dispatch), 1, 12),
    Row("BossStallion_Enter", 0x43B790, 0x3D, kCalls43B790, BH_N(kCalls43B790), BH_FN(BossStallion_Enter), 0xFF, S::kState),
    Row("BossStallion_Hook", 0x43B7D0, 0x10, nullptr, 0, BH_FN(BossStallion_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK29[] = {{Key(BossStallion_States), 12}, {Key(BossStallion_Hooks), 3, 4, 1}};

constexpr bh::CallSite kCalls43B800[] = {{0x4C, 0x5893A0}};
const bh::Clone kClonesK55[] = {
    Disp("BossSample10_Dispatch", 0x43B7E0, BH_FN(BossSample10_Dispatch), 1, 12),
    Row("BossSample10_Enter", 0x43B800, 0x51, kCalls43B800, BH_N(kCalls43B800), BH_FN(BossSample10_Enter), 0xFF, S::kState),
    Row("BossSample10_Hook", 0x43B860, 0x10, nullptr, 0, BH_FN(BossSample10_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK55[] = {{Key(BossSample10_States), 12}, {Key(BossSample10_Hooks), 3, 4, 1}};

constexpr bh::CallSite kCalls43B890[] = {{0x1E, 0x446DE0}, {0x23, 0x446E00}};
constexpr bh::CallSite kCalls43B8C0[] = {{0x2, 0x494A60}};
const bh::Clone kClonesB24[] = {
    Row("Boss24_Setup", 0x43B870, 0x1F, nullptr, 0, BH_FN(Boss24_Setup), 0, S::kSetup),
    Row("Boss24_End", 0x43B890, 0x28, kCalls43B890, BH_N(kCalls43B890), BH_FN(Boss24_End), 0, S::kEnd),
    Row("Boss24_Exit", 0x43B8C0, 0x9, kCalls43B8C0, BH_N(kCalls43B8C0), BH_FN(Boss24_Exit), 0, S::kExit),
};
const bh::Clone kClonesB48[] = {
    Row("Boss48_Setup", 0x43B8D0, 0x1F, nullptr, 0, BH_FN(Boss48_Setup), 0, S::kSetup),
};

// ===========================================================================
// Kinds 30, 31 (Beyd) and 32 (Zig)
// ===========================================================================

constexpr bh::CallSite kCalls43B910[] = {{0x38, 0x5893A0}, {0x4C, 0x4948E0}, {0x54, 0x4946C0}};
constexpr bh::CallSite kCalls43BA30[] = {{0x2, 0x589330}, {0x13, 0x589410}, {0x18, 0x437470}, {0x58, 0x4976D0}};
constexpr bh::CallSite kCalls43BAD0[] = {{0x15, 0x4949D0}};
const bh::Clone kClonesK30[] = {
    Disp("BossBeyd_Dispatch", 0x43B8F0, BH_FN(BossBeyd_Dispatch), 1, 12),
    Row("BossBeyd_Enter", 0x43B910, 0xFE, kCalls43B910, BH_N(kCalls43B910), BH_FN(BossBeyd_Enter), 0, S::kState),
    Disp("BossBeyd_ActDispatch", 0x43BA10, BH_FN(BossBeyd_ActDispatch), 2, 6),
    Row("BossBeyd_Death", 0x43BA30, 0x5F, kCalls43BA30, BH_N(kCalls43BA30), BH_FN(BossBeyd_Death), 0, S::kState),
    Row("BossBeyd_Hook", 0x43BA90, 0x10, nullptr, 0, BH_FN(BossBeyd_Hook), 0, S::kEnemyHook),
    Row("BossBeyd_HookPick", 0x43BAA0, 0x27, nullptr, 0, BH_FN(BossBeyd_HookPick), 0, S::kEnemyHook),
    Row("BossBeyd_HookTick", 0x43BAD0, 0x2E, kCalls43BAD0, BH_N(kCalls43BAD0), BH_FN(BossBeyd_HookTick), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK30[] = {{Key(BossBeyd_States), 12}, {Key(BossBeyd_ActSubs), 6}, {Key(BossBeyd_Hooks), 3, 4, 1}};
unsigned char FightByte(unsigned char mine) {
    return static_cast<unsigned char>(bh::Often() ? (bh::Half() ? mine : BH_PICK(0x19, 0x1A, 0x10, 0x25, 0x18, 0x1B, 0x9A, 0)) : bh::Next());
}
void SeedK30(unsigned k) {
    switch (k) {
    case 1: B(at::kFight) = FightByte(0x1A); break;
    case 2:
        // the other state bytes inside the tables, so a dispatcher reading the
        // wrong byte lands on another entry rather than past its table
        if (bh::Often()) Sprite_Current[1] = static_cast<unsigned char>(bh::Next() % 6);
        if (bh::Often()) Sprite_Current[3] = static_cast<unsigned char>(bh::Next() % 6);
        break;
    case 3: B(at::kFight) = FightByte(0x19); break;
    case 5:
        B(at::kFight) = FightByte(0x19);
        B(at::kRoundSlot) = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 0x80, 0xFF) : bh::Next());
        break;
    case 6:
        B(at::kFlags) = static_cast<unsigned char>(bh::Half() ? bh::Next() | 2 : bh::Next());
        SetWord(Mem(0x66C810), bh::Often() ? BH_PICK(0, 0, 1, 0x100, 0x8000) : bh::Next());
        break;
    default: break;
    }
}

constexpr bh::CallSite kCalls43BB20[] = {{0x38, 0x5893A0}};
constexpr bh::CallSite kCalls43BC20[] = {{0x2, 0x589330}, {0x13, 0x589410}, {0x18, 0x437470}};
const bh::Clone kClonesK31[] = {
    Disp("BossBeyd2_Dispatch", 0x43BB00, BH_FN(BossBeyd2_Dispatch), 1, 12),
    Row("BossBeyd2_Enter", 0x43BB20, 0xDC, kCalls43BB20, BH_N(kCalls43BB20), BH_FN(BossBeyd2_Enter), 0, S::kState),
    Disp("BossBeyd2_ActDispatch", 0x43BC00, BH_FN(BossBeyd2_ActDispatch), 2, 6),
    Row("BossBeyd2_Death", 0x43BC20, 0x47, kCalls43BC20, BH_N(kCalls43BC20), BH_FN(BossBeyd2_Death), 0, S::kState),
    Row("BossBeyd2_Hook", 0x43BC70, 0x10, nullptr, 0, BH_FN(BossBeyd2_Hook), 0, S::kEnemyHook),
    Row("BossBeyd2_HookTarget4", 0x43BC80, 0x8, nullptr, 0, BH_FN(BossBeyd2_HookTarget4), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK31[] = {{Key(BossBeyd2_States), 12}, {Key(BossBeyd2_ActSubs), 6}, {Key(BossBeyd2_Hooks), 3, 4, 1}};
void SeedK31(unsigned k) {
    if (k == 2) {
        if (bh::Often()) Sprite_Current[1] = static_cast<unsigned char>(bh::Next() % 6);
        if (bh::Often()) Sprite_Current[3] = static_cast<unsigned char>(bh::Next() % 6);
    }
}

constexpr bh::CallSite kCalls43BCB0[] = {{0x42, 0x5893A0}};
constexpr bh::CallSite kCalls43BD00[] = {{0xF, 0x589330}, {0x2A, 0x4358D0}, {0x32, 0x436090}};
constexpr bh::CallSite kCalls43BD60[] = {{0x0, 0x436090}, {0x2E, 0x437450}};
constexpr bh::CallSite kCalls43BDA0[] = {{0x0, 0x436090}, {0xF, 0x4530D0}, {0x17, 0x5B93D2}, {0x37, 0x435180}, {0x46, 0x4376A0}};
constexpr bh::CallSite kCalls43BE10[] = {{0x2, 0x589330}, {0x13, 0x589410}, {0x18, 0x437470}};
const bh::Clone kClonesK32[] = {
    Disp("BossZig_Dispatch", 0x43BC90, BH_FN(BossZig_Dispatch), 1, 12),
    Row("BossZig_Enter", 0x43BCB0, 0x47, kCalls43BCB0, BH_N(kCalls43BCB0), BH_FN(BossZig_Enter), 0xFF, S::kState),
    Row("BossZig_Idle", 0x43BD00, 0x40, kCalls43BD00, BH_N(kCalls43BD00), BH_FN(BossZig_Idle), 0, S::kState),
    Disp("BossZig_Step5Dispatch", 0x43BD40, BH_FN(BossZig_Step5Dispatch), 2, 2),
    Row("BossZig_Step5Count", 0x43BD60, 0x3F, kCalls43BD60, BH_N(kCalls43BD60), BH_FN(BossZig_Step5Count), 0, S::kState),
    Row("BossZig_Step5Fire", 0x43BDA0, 0x4C, kCalls43BDA0, BH_N(kCalls43BDA0), BH_FN(BossZig_Step5Fire), 0, S::kState),
    Disp("BossZig_ActDispatch", 0x43BDF0, BH_FN(BossZig_ActDispatch), 2, 6),
    Row("BossZig_Death", 0x43BE10, 0x47, kCalls43BE10, BH_N(kCalls43BE10), BH_FN(BossZig_Death), 0, S::kState),
    Row("BossZig_Hook", 0x43BE60, 0x10, nullptr, 0, BH_FN(BossZig_Hook), 0, S::kEnemyHook),
    Row("BossZig_HookPick", 0x43BE70, 0x19, nullptr, 0, BH_FN(BossZig_HookPick), 0, S::kEnemyHook),
    Row("BossZig_HookHit", 0x43BE90, 0x8, nullptr, 0, BH_FN(BossZig_HookHit), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK32[] = {{Key(BossZig_States), 12},
                                    {Key(BossZig_Step5Subs), 2},
                                    {Key(BossZig_ActSubs), 6},
                                    {Key(BossZig_Hooks), 3, 4, 1}};
unsigned char ScriptBits() {
    return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 8, 0x20, 0x28, 0x10, 0x18, 0x38, 0xF7, 0xDF, 0xFF) : bh::Next());
}
void SeedK32(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case 2: {
        B(at::kScript) = ScriptBits();
        unsigned char* const e = bh::Pointer(bh::at::kEnemyCurrent);
        e[0x110] = static_cast<unsigned char>(bh::Half() ? e[0x110] | 2 : e[0x110] & ~2u);
        break;
    }
    case 3:
    case 6:
        if (bh::Often()) s[1] = static_cast<unsigned char>(bh::Next() % 2);
        if (bh::Often()) s[3] = static_cast<unsigned char>(bh::Next() % 2);
        break;
    case 4: s[9] = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 1, 0, 2, 0x80, 0x81, 0xFF) : bh::Next()); break;
    case 5:
        bh::SetRandHint(bh::Next() & 3);
        B(at::kScript) = ScriptBits();
        break;
    case 9: B(at::kScript) = ScriptBits(); break;
    default: break;
    }
}

// ===========================================================================
// Set-ups 25 and 26
// ===========================================================================

constexpr bh::CallSite kCalls43BF00[] = {{0x2C, 0x435180},  {0xD8, 0x4976D0},  {0x103, 0x4358D0},
                                         {0x12C, 0x4976D0}, {0x1C7, 0x4976D0}, {0x1E5, 0x4976D0},
                                         {0x1FE, 0x4976D0}, {0x238, 0x4976D0}, {0x25A, 0x4976D0}};
constexpr bh::CallSite kCalls43C180[] = {{0x24, 0x446E20}};
constexpr bh::CallSite kCalls43C1B0[] = {{0x9, 0x495040}};
const bh::Clone kClonesB25[] = {
    Row("Boss25_Setup", 0x43BEA0, 0x56, nullptr, 0, BH_FN(Boss25_Setup), 0, S::kSetup),
    Row("Boss25_Event", 0x43BF00, 0x27D, kCalls43BF00, BH_N(kCalls43BF00), BH_FN(Boss25_Event), 0xFF, S::kEvent),
    Row("Boss25_End", 0x43C180, 0x29, kCalls43C180, BH_N(kCalls43C180), BH_FN(Boss25_End), 0, S::kEnd),
    Row("Boss25_Exit", 0x43C1B0, 0x4A, kCalls43C1B0, BH_N(kCalls43C1B0), BH_FN(Boss25_Exit), 0, S::kExit),
};
// Boss25_Event's code 3 walks the script bits 0x904AAD in turn: the seed
// picks the step a round is aimed at (each bit before it set, it clear, the
// rest random) and the phase and step that step tests, or any; code 0 walks
// the round's count 0..7.
void SeedB25(unsigned k) {
    switch (k) {
    case 1: {
        const U pick = bh::Next() % 10;
        g_code = pick < 3 ? 0 : pick < 9 ? 3 : BH_PICK(1, 2, 4, 5, 6, 0xFF, 0x80);
        const auto bits = static_cast<unsigned char>(bh::Next());
        const U pick_step = bh::Next() % 9;
        const U step = pick_step >= 7 ? 5 : pick_step;
        switch (step) {
        case 0: B(at::kScript) = static_cast<unsigned char>(bits & ~0x20u); break;
        case 1: B(at::kScript) = static_cast<unsigned char>((bits | 0x20) & ~0x10u); break;
        case 2: B(at::kScript) = static_cast<unsigned char>((bits | 0x30) & ~1u); break;
        case 3: B(at::kScript) = static_cast<unsigned char>((bits | 0x31) & ~2u); break;
        case 4: B(at::kScript) = static_cast<unsigned char>((bits | 0x33) & ~4u); break;
        case 5: B(at::kScript) = static_cast<unsigned char>(bits | 0x37); break;
        default: B(at::kScript) = bits; break;
        }
        // the task's phase 1 for step 0, the order's phase 3 for step 1
        B(at::kPhase) = static_cast<unsigned char>(bh::Often() ? (step < 2 && bh::Often() ? 1 + 2 * step : BH_PICK(1, 3, 0, 2, 5))
                                                               : bh::Next());
        B(at::kStep) = static_cast<unsigned char>(bh::Often() ? 0 : bh::Next());
        B(at::kRoundSlot) = static_cast<unsigned char>(bh::Often() ? (step == 5 || bh::Half() ? BH_PICK(4, 6, 4, 6, 5, 3) : bh::Next() % 8)
                                                                   : bh::Next());
        // member 0's +1 / +2: the two pairs the last step tests, their crossings, or any
        static const U kPairs[][2] = {{6, 1}, {2, 0}, {6, 1}, {2, 0}, {6, 0}, {2, 1}, {3, 0}, {6, 2}, {1, 0}};
        const U* const pair = kPairs[bh::Next() % BH_COUNT(kPairs)];
        const bool any = !bh::Often();
        B(at::kMember0State1) = static_cast<unsigned char>(any ? bh::Next() : pair[0]);
        B(at::kMember0State2) = static_cast<unsigned char>(any ? bh::Next() : pair[1]);
        B(at::kFlags) = static_cast<unsigned char>(bh::Half() ? B(at::kFlags) | 0x40 : B(at::kFlags) & ~0x40u);
        break;
    }
    case 3:
        B(at::kPartyCopyCount) = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 3, 4, 5, 7, 8, 0x10, 0xFF) : bh::Next());
        break;
    default: break;
    }
}

constexpr bh::CallSite kCalls43C230[] = {{0x114, 0x435180}};
constexpr bh::JumpTable kTables43C230[] = {{0x15, 0x12C, 4}};
constexpr bh::CallSite kCalls43C370[] = {{0xF, 0x446650},  {0x4B, 0x589110}, {0x62, 0x446650}, {0x9E, 0x589110},
                                         {0xB5, 0x446650}, {0xF1, 0x589110}, {0x100, 0x446E20}};
constexpr bh::CallSite kCalls43E790[] = {{0x9, 0x495040}};
const bh::Clone kClonesB26[] = {
    Row("Boss26_Setup", 0x43C200, 0x30, nullptr, 0, BH_FN(Boss26_Setup), 0, S::kSetup),
    Row("Boss26_Event", 0x43C230, 0x13C, kCalls43C230, BH_N(kCalls43C230), BH_FN(Boss26_Event), 0xFF, S::kEvent, kTables43C230,
        BH_N(kTables43C230)),
    Row("Boss26_End", 0x43C370, 0x105, kCalls43C370, BH_N(kCalls43C370), BH_FN(Boss26_End), 0, S::kEnd),
    Row("BossHook_ExitTransition4", 0x43E790, 0x10, kCalls43E790, BH_N(kCalls43E790), BH_FN(BossHook_ExitTransition4), 0, S::kExit),
};
std::int32_t SignedWord() {
    return static_cast<std::int32_t>(bh::Often() ? BH_PICK(0, 1, 0xFFFF, 0x7FFF, 0x8000, 2, 0x100, 0xFF00) : bh::Next() & 0xFFFF);
}
void SeedB26(unsigned k) {
    switch (k) {
    case 1: {
        B(at::kActor) = static_cast<unsigned char>(bh::Often() ? 3 : BH_PICK(0, 1, 2, 4, 0x83));
        const unsigned char target =
            static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 3, 0, 1, 2, 3, 0x40, 0x80, 0xC0, 4, 7) : bh::Next());
        B(at::kTarget) = target;
        for (unsigned m = 0; m < 3; ++m) SetWord(Mem(at::kMember0Word128 + at::kPartyStride * m), static_cast<unsigned>(SignedWord()));
        SetWord(Mem(at::kEnemy0Word108), static_cast<unsigned>(SignedWord()));
        B(at::kEnemy0Flags) = static_cast<unsigned char>(bh::Half() ? B(at::kEnemy0Flags) | 2 : B(at::kEnemy0Flags) & ~2u);
        B(at::kRoundSlot) = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 1, 0, 2) : bh::Next());
        B(at::kEnemy0Flag8E) = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 1, 0, 2, 0x81) : bh::Next());
        SetLong(Mem(at::kTurn), static_cast<std::int32_t>(bh::Often() ? BH_PICK(0x15, 0x15, 0x14, 0x16, 0x115, 0x80000015u) : bh::Next()));
        B(at::kScript) = static_cast<unsigned char>(bh::Half() ? B(at::kScript) | 0x20 : B(at::kScript) & ~0x20u);
        B(at::kPhase) = static_cast<unsigned char>(bh::Often() ? 1 : bh::Next());
        break;
    }
    case 2:
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(m));
            p[0] = static_cast<unsigned char>(bh::Often() ? p[0] | 1 : p[0] & ~1u);
            if (bh::Often()) p[8] = static_cast<unsigned char>(BH_PICK(0, 1, 0xE3, 0xE4, 0xFC, 0xFB, 0x7F));
        }
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    RunGroup("b22", kClonesB22, BH_COUNT(kClonesB22), nullptr, 0, &SeedB22, 22, -1);
    RunGroup("k28", kClonesK28, BH_COUNT(kClonesK28), kTablesK28, BH_COUNT(kTablesK28), &SeedK28, 23, 28);
    RunGroup("b23", kClonesB23, BH_COUNT(kClonesB23), nullptr, 0, nullptr, 23, -1, 4000);
    RunGroup("b30", kClonesB30, BH_COUNT(kClonesB30), nullptr, 0, &SeedEnd, 30, -1);
    RunGroup("k29", kClonesK29, BH_COUNT(kClonesK29), kTablesK29, BH_COUNT(kTablesK29), nullptr, 24, 29);
    RunGroup("k55", kClonesK55, BH_COUNT(kClonesK55), kTablesK55, BH_COUNT(kTablesK55), nullptr, 48, 55);
    RunGroup("b24", kClonesB24, BH_COUNT(kClonesB24), nullptr, 0, &SeedEnd, 24, -1);
    RunGroup("b48", kClonesB48, BH_COUNT(kClonesB48), nullptr, 0, nullptr, 48, -1, 4000);
    RunGroup("k30", kClonesK30, BH_COUNT(kClonesK30), kTablesK30, BH_COUNT(kTablesK30), &SeedK30, 25, 30);
    RunGroup("k31", kClonesK31, BH_COUNT(kClonesK31), kTablesK31, BH_COUNT(kTablesK31), &SeedK31, 25, 31);
    RunGroup("k32", kClonesK32, BH_COUNT(kClonesK32), kTablesK32, BH_COUNT(kTablesK32), &SeedK32, 25, 32);
    RunGroup("b25", kClonesB25, BH_COUNT(kClonesB25), nullptr, 0, &SeedB25, 25, -1, 8000, &SettleScript);
    RunGroup("b26", kClonesB26, BH_COUNT(kClonesB26), nullptr, 0, &SeedB26, 26, -1, 8000, &SettleScript);
}

}  // namespace boss_se

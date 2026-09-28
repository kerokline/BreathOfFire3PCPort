// BOF3X_SHADOW=boss_sa: group BSA's 49 functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per kind (6, 7, 1,
// 2, 46, 39) and per fight (1, 2, 3, 39). docs/boss_sa.md section 3.
//
// The clone rows are tools/boss_rows.py's (--unit <U> --clones, 2026-09-28),
// each read against the disassembly (capstone): every extent runs to the
// function's last instruction (0x437CF0's includes its seven-entry jump
// table), every call site is the tool's. The tool's table extents are too long
// (it reads a table to the next address anything names): the counts below are
// the code's - each dispatcher's table runs to the next table its kind's code
// names (docs/boss_sa.md section 1).
//
// Every kind's tables are DataTables (their cells hold recorders while the
// Run lasts), every dispatcher a kDispatch with its state byte drawn below its
// table, every hook dispatcher and hook entry a kEnemyHook (the word 0..2,
// garbage above the byte half the time). The set-ups are kSetup, the hooks
// kEvent / kEnd / kExit - the harness reads the three hooks back after every
// call.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sa.h"
#include "game/boss_sa_callees.h"
#include "game/move_script_bytes.h"

namespace boss_sa {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using move_script::SetWord;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define SA_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define SA_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define SA_FN(name) reinterpret_cast<const void*>(&::name)

// A clone row: name, address, size, calls, ours, the answer's mask, shape;
// a dispatcher's state byte and entries.
bh::Clone Row(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, U ret, S shape,
              std::uint8_t state_at = 1, std::uint8_t states = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    c.state_at = state_at;
    c.states = states;
    return c;
}

// The callees the standard set lacks or records too coarsely, for every Run.
const bh::Callee kCallees[] = {
    // Port_DroppedCall is a bare ret (magic_s28.cpp) that sits in the kinds'
    // state tables (entries 1 and 10): reached through a dispatcher's jmp it
    // "reads" the stack word of the dispatcher's caller, which is nobody's
    // argument - logged with no arguments here, as a table entry
    {"Port_DroppedCall (a table entry)", 0x4DF820, KeyOf(&::Port_DroppedCall), 0, {}, bh::Answer::kGarbage, 0, 0},
    // the engine's unnamed three (boss_sa_callees.h): 0x437450 reads the word
    {"0x437450", at::kPlayCue, at::kPlayCue, 1, {0xFFFFu}, bh::Answer::kGarbage, 0, 0},
    {"0x4376A0", at::kTurnClose, at::kTurnClose, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x4376F0", at::kTurnChance, at::kTurnChance, 0, {}, bh::Answer::kGarbage, 0, 0},
};

// The cells the set-ups' hooks write beyond the standard regions: MoveScript
// counter 0, the music track, the window records' pass byte.
const bh::Region kRegions[] = {{at::kMoveVar3, 1}, {Key(&Music_Track), 1}, {at::kWindowPass, 1}};

unsigned char* Enemy() { return bh::Pointer(bh::at::kEnemyCurrent); }
unsigned char Byte(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<unsigned char>(bh::Next());
    return static_cast<unsigned char>(often.begin()[bh::Next() % often.size()]);
}

// The hook word: 0..2 (the harness's draw), garbage above the byte half the
// time (the dispatchers index by the low byte; the entries get the word whole).
void HookWord(U* a) {
    if (bh::Half()) a[0] = (a[0] & 0xFF) | (bh::Next() & 0xFFFFFF00u);
}

// HP against the kinds' two hook entries: the pick's 0xFFFF mark, the hit's
// quarter of the maximum (HP less the signed pending damage at, just above
// and just below it; the damage negative sometimes).
void SeedHp(bool quarter) {
    unsigned char* const e = Enemy();
    if (!quarter) {
        if (bh::Half()) SetWord(e + 0xA4, bh::Half() ? 0xFFFF : bh::Next() % 3 == 0 ? 0xFFFE : 0);
        return;
    }
    if (!bh::Often()) return;
    const U max = bh::Often() ? 1 + bh::Next() % 4000 : bh::Next() & 0xFFFF;
    const std::int32_t quarterhp = static_cast<std::int32_t>((max & 0xFFFF) >> 2);
    const std::int32_t dmg = bh::Half() ? static_cast<std::int32_t>(bh::Next() % 600) - 100 : static_cast<std::int16_t>(bh::Next());
    const std::int32_t left = quarterhp + static_cast<std::int32_t>(bh::Next() % 3) - 1;
    SetWord(e + 0xB0, max & 0xFFFF);
    SetWord(e + 0x108, static_cast<U>(dmg) & 0xFFFF);
    SetWord(e + 0xA4, static_cast<U>(left + static_cast<std::int16_t>(dmg)) & 0xFFFF);
}

// Kinds 1 and 39's end walk: MoveCmd_OpE9's answer is the standard kFlag;
// the field actor's index BossActor_Index answers 0xFF..29.

// ===========================================================================
// Kind 6 (Gary) and kind 7 (Mogu): fight 1
// ===========================================================================

constexpr bh::CallSite kCalls437A30[] = {{0x38, 0x5893A0}};
constexpr bh::CallSite kCalls437AB0[] = {{0x2, 0x4358D0}, {0xB, 0x572650}};
constexpr bh::CallSite kCalls437AD0[] = {{0x28, 0x5720C0}, {0x39, 0x589590}, {0x59, 0x5891F0}, {0x71, 0x572650}, {0x81, 0x5893A0}};
const bh::Clone kClonesGary[] = {
    Row("BossGary_Dispatch", 0x437A10, 0x12, nullptr, 0, SA_FN(BossGary_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossGary_Enter", 0x437A30, 0x3D, kCalls437A30, SA_N(kCalls437A30), SA_FN(BossGary_Enter), 0xFF, S::kState),
    Row("BossGary_ActDispatch", 0x437A70, 0x12, nullptr, 0, SA_FN(BossGary_ActDispatch), 0xFF, S::kDispatch, 2, 6),
    Row("BossGary_EndDispatch", 0x437A90, 0x12, nullptr, 0, SA_FN(BossGary_EndDispatch), 0xFF, S::kDispatch, 2, 3),
    Row("BossGary_EndStart", 0x437AB0, 0x1C, kCalls437AB0, SA_N(kCalls437AB0), SA_FN(BossGary_EndStart), 0, S::kState),
    Row("BossGary_EndAwait", 0x437AD0, 0x86, kCalls437AD0, SA_N(kCalls437AD0), SA_FN(BossGary_EndAwait), 0xFF, S::kState),
    Row("BossGary_Hook", 0x437B60, 0x10, nullptr, 0, SA_FN(BossGary_Hook), 0xFF, S::kEnemyHook),
};
// the hook table first: BareRet, in it, logs its word
const bh::DataTable kTablesGary[] = {
    {Key(BossGary_Hooks), 3, 4, 1}, {Key(BossGary_Steps), 12}, {Key(BossGary_ActSubs), 6}, {Key(BossGary_EndSteps), 3}};

constexpr bh::CallSite kCalls437B90[] = {{0x38, 0x5893A0}};
constexpr bh::CallSite kCalls437C10[] = {{0x2, 0x4358D0}};
constexpr bh::CallSite kCalls437C30[] = {{0x27, 0x5720C0}, {0x38, 0x589590}, {0x58, 0x5891F0}, {0x68, 0x5893A0}};
const bh::Clone kClonesMogu[] = {
    Row("BossMogu_Dispatch", 0x437B70, 0x12, nullptr, 0, SA_FN(BossMogu_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossMogu_Enter", 0x437B90, 0x3D, kCalls437B90, SA_N(kCalls437B90), SA_FN(BossMogu_Enter), 0xFF, S::kState),
    Row("BossMogu_ActDispatch", 0x437BD0, 0x12, nullptr, 0, SA_FN(BossMogu_ActDispatch), 0xFF, S::kDispatch, 2, 6),
    Row("BossMogu_EndDispatch", 0x437BF0, 0x12, nullptr, 0, SA_FN(BossMogu_EndDispatch), 0xFF, S::kDispatch, 2, 3),
    Row("BossMogu_EndStart", 0x437C10, 0x1C, kCalls437C10, SA_N(kCalls437C10), SA_FN(BossMogu_EndStart), 0, S::kState),
    Row("BossMogu_EndCount", 0x437C30, 0x6D, kCalls437C30, SA_N(kCalls437C30), SA_FN(BossMogu_EndCount), 0xFF, S::kState),
    Row("BossMogu_Hook", 0x437CB0, 0x10, nullptr, 0, SA_FN(BossMogu_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesMogu[] = {
    {Key(BossMogu_Hooks), 3, 4, 1}, {Key(BossMogu_Steps), 12}, {Key(BossMogu_ActSubs), 6}, {Key(BossMogu_EndSteps), 3}};
enum : unsigned { kDispatch, kEnter, kAct, kEnd, kEndStart, kEndStep, kHook };

// The count 0x904B7E: Gary's step waits for 0, Mogu's counts it down to 0.
void SeedCount(U end) {
    SetWord(Mem(at::kEndCount), bh::Often() ? (bh::Half() ? end : BH_PICK(0, 1, 2, 0xFFFF, 0x3C, 0x100)) : bh::Next() & 0xFFFF);
}
void SeedGary(unsigned k) {
    if (k == kEndStep) SeedCount(0);
    if (k == kEndStep || k == kEndStart) Mem(at::kBattleEnd)[0] = Byte({0, 1, 2, 4, 6, 0xFB});
}
void SeedMogu(unsigned k) {
    if (k == kEndStep) SeedCount(1);
}
void ArgsKind(unsigned k, U* a) {
    if (k == kHook) HookWord(a);
}

// ===========================================================================
// Fight 1's set-up and hooks
// ===========================================================================

constexpr bh::JumpTable kTables437CF0[] = {{0x15, 0xCC, 7}};
constexpr bh::CallSite kCalls437DE0[] = {{0x17, 0x446E20}, {0x22, 0x446E20}};
constexpr bh::CallSite kCalls437E10[] = {{0x3, 0x4949D0}, {0x11, 0x4949F0}, {0x18, 0x494920}, {0x24, 0x589590}, {0x3E, 0x5891F0},
                                         {0x66, 0x4949D0}, {0x74, 0x4949F0}, {0x7B, 0x494920}, {0x87, 0x589590}, {0x9E, 0x5891F0}};
const bh::Clone kClones01[] = {
    Row("Boss01_Setup", 0x437CD0, 0x1F, nullptr, 0, SA_FN(Boss01_Setup), 0, S::kSetup),
    {"Boss01_Event", 0x437CF0, 0xE8, nullptr, 0, nullptr, 0, kTables437CF0, SA_N(kTables437CF0), SA_FN(Boss01_Event), 0xFF, false, S::kEvent},
    Row("Boss01_End", 0x437DE0, 0x27, kCalls437DE0, SA_N(kCalls437DE0), SA_FN(Boss01_End), 0, S::kEnd),
    Row("Boss01_Exit", 0x437E10, 0xC9, kCalls437E10, SA_N(kCalls437E10), SA_FN(Boss01_Exit), 0, S::kExit),
};
enum : unsigned { kSetup, kEvent, kEndHook, kExit };

void Seed01(unsigned k) {
    switch (k) {
    case kEvent: {
        // phase 0: 0x904AA8 bit 0x40 and the target 0
        Mem(bh::at::kFlags)[0] = Byte({0x40, 0, 0xBF, 0xFF});
        if (bh::Half()) Mem(at::kTarget)[0] = 0;
        // phase 1: the actor 0, the kind 4, the action's id 0x78 at +2 of
        // [0x904B40] - a record in the compared state
        if (bh::Often()) Mem(at::kActor)[0] = 0;
        Mem(at::kActionKind)[0] = Byte({4, 4, 3, 5, 0x84});
        unsigned char* const record = bh::SpriteRecord(bh::Next()) + 4 * (bh::Next() % 16);
        bh::SetPointer(at::kAction, record);
        if (bh::Half()) SetWord(record + 2, bh::Half() ? 0x78 : BH_PICK(0x77, 0x79, 0x178, 0));
        // phase 5: 0x904AAD bit 0
        Mem(at::kEventFlags)[0] = Byte({0, 1, 0xFE, 0xFF});
        break;
    }
    case kEndHook:
        Mem(at::kBattleEnd)[0] = Byte({0, 1, 2, 3, 0xFE, 0x81});
        Mem(at::kChapterStep)[0] = Byte({0, 0xFF, 0x31});
        break;
    default:
        break;
    }
}
// The event hook's phase code: 0..6 (the harness's draw), sometimes 7 and
// above, garbage above the byte half the time (the hook switches on the low
// byte).
void Args01(unsigned k, U* a) {
    if (k != kEvent) return;
    if (bh::Next() % 8 == 0) a[0] = 7 + bh::Next() % 0xF9;
    if (bh::Half()) a[0] = (a[0] & 0xFF) | (bh::Next() & 0xFFFFFF00u);
}

// ===========================================================================
// Kind 1 (Nue, area 23), kind 2 (Nue, area 22), kind 46 (Sample 1)
// ===========================================================================

constexpr bh::CallSite kCalls437F00[] = {{0x38, 0x5893A0}};
constexpr bh::CallSite kCalls437F60[] = {{0x2, 0x5891F0}};
constexpr bh::CallSite kCalls437F80[] = {{0x0, 0x589410}, {0x19, 0x494980}, {0x34, 0x57C8E0}, {0x4A, 0x446FD0}, {0x5E, 0x589840}};
constexpr bh::CallSite kCalls438000[] = {{0x26, 0x587900}};
const bh::Clone kClonesNue[] = {
    Row("BossNue_Dispatch", 0x437EE0, 0x12, nullptr, 0, SA_FN(BossNue_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossNue_Enter", 0x437F00, 0x3D, kCalls437F00, SA_N(kCalls437F00), SA_FN(BossNue_Enter), 0xFF, S::kState),
    Row("BossNue_EndDispatch", 0x437F40, 0x12, nullptr, 0, SA_FN(BossNue_EndDispatch), 0xFF, S::kDispatch, 2, 2),
    Row("BossNue_EndPose", 0x437F60, 0x13, kCalls437F60, SA_N(kCalls437F60), SA_FN(BossNue_EndPose), 0, S::kState),
    Row("BossNue_EndMove", 0x437F80, 0x64, kCalls437F80, SA_N(kCalls437F80), SA_FN(BossNue_EndMove), 0, S::kState),
    Row("BossNue_Hook", 0x437FF0, 0x10, nullptr, 0, SA_FN(BossNue_Hook), 0xFF, S::kEnemyHook),
    Row("BossNue_HookPick", 0x438000, 0x2D, kCalls438000, SA_N(kCalls438000), SA_FN(BossNue_HookPick), 0, S::kEnemyHook),
    Row("BossNue_HookHit", 0x438030, 0x40, nullptr, 0, SA_FN(BossNue_HookHit), 0, S::kEnemyHook),
};
const bh::DataTable kTablesNue[] = {{Key(BossNue_Hooks), 3, 4, 1}, {Key(BossNue_Steps), 12}, {Key(BossNue_EndSteps), 2}};
enum : unsigned { kNueDispatch, kNueEnter, kNueEnd, kNuePose, kNueMove, kNueHook, kNuePick, kNueHit };

void SeedNue(unsigned k) {
    if (k == kNuePick) SeedHp(false);
    if (k == kNueHit) SeedHp(true);
    if (k == kNueMove) Mem(at::kBattleEnd)[0] = Byte({0, 2, 0xFD, 0xFF});
}
void ArgsNue(unsigned k, U* a) {
    if (k == kNueHook || k == kNuePick || k == kNueHit) HookWord(a);
}

constexpr bh::CallSite kCalls438090[] = {{0x38, 0x5893A0}};
const bh::Clone kClonesNue2[] = {
    Row("BossNue2_Dispatch", 0x438070, 0x12, nullptr, 0, SA_FN(BossNue2_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossNue2_Enter", 0x438090, 0x3D, kCalls438090, SA_N(kCalls438090), SA_FN(BossNue2_Enter), 0xFF, S::kState),
    Row("BossNue2_ActDispatch", 0x4380D0, 0x12, nullptr, 0, SA_FN(BossNue2_ActDispatch), 0xFF, S::kDispatch, 2, 6),
    Row("BossNue2_Hook", 0x4380F0, 0x10, nullptr, 0, SA_FN(BossNue2_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesNue2[] = {{Key(BossNue2_Hooks), 3, 4, 1}, {Key(BossNue2_Steps), 12}, {Key(BossNue2_ActSubs), 6}};

constexpr bh::CallSite kCalls438120[] = {{0x4C, 0x5893A0}};
const bh::Clone kClonesSample1[] = {
    Row("BossSample1_Dispatch", 0x438100, 0x12, nullptr, 0, SA_FN(BossSample1_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossSample1_Enter", 0x438120, 0x51, kCalls438120, SA_N(kCalls438120), SA_FN(BossSample1_Enter), 0xFF, S::kState),
    Row("BossSample1_Hook", 0x438180, 0x10, nullptr, 0, SA_FN(BossSample1_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesSample1[] = {{Key(BossSample1_Hooks), 3, 4, 1}, {Key(BossSample1_Steps), 12}};
// the hook is each group's last clone
void ArgsLastHook(unsigned k, U* a);
unsigned g_last_hook;
void ArgsLastHook(unsigned k, U* a) {
    if (k == g_last_hook) HookWord(a);
}

// ===========================================================================
// Fights 2, 3 and 39 (BOSS002)
// ===========================================================================

constexpr bh::CallSite kCalls4381B0[] = {{0x1B, 0x4976D0}};
constexpr bh::CallSite kCalls4381E0[] = {{0x9, 0x446E00}, {0x10, 0x494A60}, {0x26, 0x446DE0}};
const bh::Clone kClones02[] = {
    Row("Boss02_Setup", 0x438190, 0x1F, nullptr, 0, SA_FN(Boss02_Setup), 0, S::kSetup),
    Row("Boss02_Event", 0x4381B0, 0x26, kCalls4381B0, SA_N(kCalls4381B0), SA_FN(Boss02_Event), 0xFF, S::kEvent),
    Row("Boss02_End", 0x4381E0, 0x2B, kCalls4381E0, SA_N(kCalls4381E0), SA_FN(Boss02_End), 0, S::kEnd),
};
constexpr bh::CallSite kCalls438230[] = {{0x9, 0x446E00}, {0x17, 0x4949F0}, {0x2D, 0x446DE0}};
const bh::Clone kClones03[] = {
    Row("Boss03_Setup", 0x438210, 0x1F, nullptr, 0, SA_FN(Boss03_Setup), 0, S::kSetup),
    Row("Boss03_End", 0x438230, 0x32, kCalls438230, SA_N(kCalls438230), SA_FN(Boss03_End), 0, S::kEnd),
};
const bh::Clone kClones39[] = {
    Row("Boss39_Setup", 0x438270, 0x1F, nullptr, 0, SA_FN(Boss39_Setup), 0, S::kSetup),
};

// The end hooks' 0x904AE8 & 6 (none, bit 1, bit 2, both), the event hook's
// bit 1; the three cells they write, garbage before.
void SeedBoss002(unsigned) {
    Mem(at::kBattleEnd)[0] = Byte({0, 1, 2, 4, 6, 0xF9, 0xFB});
}
// Fight 2's event hook reads the phase code's low byte for 0: 0 a third of
// the time, with garbage above the byte half the time.
void Args02(unsigned k, U* a) {
    if (k != 1) return;
    if (bh::Next() % 3 == 0) a[0] = 0;
    if (bh::Half()) a[0] = (a[0] & 0xFF) | (bh::Next() & 0xFFFFFF00u);
}

// ===========================================================================
// Kind 39 (Weretigr, area 35; fight 33)
// ===========================================================================

constexpr bh::CallSite kCalls43D3F0[] = {{0x38, 0x5893A0}};
constexpr bh::CallSite kCalls43D450[] = {{0x7, 0x435180}};
constexpr bh::CallSite kCalls43D500[] = {{0x29, 0x437450}, {0x42, 0x437450}};
constexpr bh::CallSite kCalls43D560[] = {{0x11, 0x4376F0}, {0x16, 0x4376A0}};
constexpr bh::CallSite kCalls43D580[] = {{0x2, 0x5891F0}};
constexpr bh::CallSite kCalls43D5A0[] = {{0x0, 0x589410}, {0x19, 0x494980}, {0x34, 0x57C8E0}, {0x4A, 0x446FD0}, {0x7E, 0x589840}};
const bh::Clone kClonesWeretigr[] = {
    Row("BossWeretigr_Dispatch", 0x43D3D0, 0x12, nullptr, 0, SA_FN(BossWeretigr_Dispatch), 0xFF, S::kDispatch, 1, 12),
    Row("BossWeretigr_Enter", 0x43D3F0, 0x3D, kCalls43D3F0, SA_N(kCalls43D3F0), SA_FN(BossWeretigr_Enter), 0xFF, S::kState),
    Row("BossWeretigr_State4Dispatch", 0x43D430, 0x12, nullptr, 0, SA_FN(BossWeretigr_State4Dispatch), 0xFF, S::kDispatch, 2, 5),
    Row("BossWeretigr_State4Fx", 0x43D450, 0xAF, kCalls43D450, SA_N(kCalls43D450), SA_FN(BossWeretigr_State4Fx), 0, S::kState),
    Row("BossWeretigr_State4Cue", 0x43D500, 0x53, kCalls43D500, SA_N(kCalls43D500), SA_FN(BossWeretigr_State4Cue), 0, S::kState),
    Row("BossWeretigr_State4End", 0x43D560, 0x1C, kCalls43D560, SA_N(kCalls43D560), SA_FN(BossWeretigr_State4End), 0, S::kState),
    Row("BossWeretigr_EndPose", 0x43D580, 0x13, kCalls43D580, SA_N(kCalls43D580), SA_FN(BossWeretigr_EndPose), 0, S::kState),
    Row("BossWeretigr_EndMove", 0x43D5A0, 0x84, kCalls43D5A0, SA_N(kCalls43D5A0), SA_FN(BossWeretigr_EndMove), 0, S::kState),
    Row("BossWeretigr_Hook", 0x43D630, 0x10, nullptr, 0, SA_FN(BossWeretigr_Hook), 0xFF, S::kEnemyHook),
    Row("BossWeretigr_HookPick", 0x43D640, 0x22, nullptr, 0, SA_FN(BossWeretigr_HookPick), 0, S::kEnemyHook),
};
// the state table first here: BareRet is its entry 5, reached by a jmp with
// no argument of its own - so it logs none, and the hook table's other two
// entries log their word
const bh::DataTable kTablesWeretigr[] = {
    {Key(BossWeretigr_Steps), 12}, {Key(BossWeretigr_State4Steps), 5}, {Key(BossWeretigr_Hooks), 3, 4, 1}};
enum : unsigned { kWDispatch, kWEnter, kWState4, kWFx, kWCue, kWEnd, kWPose, kWMove, kWHook, kWPick };

void SeedWeretigr(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case kWFx:
        // the acting actor: an enemy 3..10 mostly (the harness draws 0..2)
        if (bh::Often()) Mem(at::kActor)[0] = static_cast<unsigned char>(3 + bh::Next() % 8);
        break;
    case kWCue:
        s[0xA] = Byte({1, 1, 0, 2, 0x10, 0xFF});
        break;
    case kWEnd:
        if (bh::Half()) s[0] &= 0xBF;
        break;
    case kWMove:
        Mem(at::kBattleEnd)[0] = Byte({0, 2, 0xFD, 0xFF});
        SetLong(Mem(at::kExp), static_cast<std::int32_t>(bh::Often() ? BH_PICK(0, 0xFFFF, 0xFFFFFFFFu, 0x7FFFFFFF) : bh::Next()));
        break;
    case kWPick:
        SeedHp(false);
        break;
    default:
        break;
    }
}
void ArgsWeretigr(unsigned k, U* a) {
    if (k == kWHook || k == kWPick) HookWord(a);
}

// BOF3X_BSA_RUN=<name> runs that one alone (the controls script's shortcut);
// unset, all ten run.
bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BSA_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

void RunKind(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables, void (*seed)(unsigned),
             void (*args)(unsigned, U*), int fight, int kind) {
    if (!Wants(run)) return;
    bh::Group g{"boss_sa", clones, n, kCallees, SA_COUNT(kCallees), tables, n_tables, kRegions, SA_COUNT(kRegions), seed, nullptr, 6000};
    g.args = args;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
}

void NoSeed(unsigned) {}

}  // namespace

void SelfTest() {
    RunKind("k6", kClonesGary, SA_COUNT(kClonesGary), kTablesGary, SA_COUNT(kTablesGary), &SeedGary, &ArgsKind, 1, 6);
    RunKind("k7", kClonesMogu, SA_COUNT(kClonesMogu), kTablesMogu, SA_COUNT(kTablesMogu), &SeedMogu, &ArgsKind, 1, 7);
    RunKind("b1", kClones01, SA_COUNT(kClones01), nullptr, 0, &Seed01, &Args01, 1, -1);
    // kinds 1 and 2: fights 2 and 3 (BOSS002, row 7 of areas 22 and 23 - the
    // tool leaves which is which; their code never reads 0x904AAA)
    RunKind("k1", kClonesNue, SA_COUNT(kClonesNue), kTablesNue, SA_COUNT(kTablesNue), &SeedNue, &ArgsNue, 3, 1);
    g_last_hook = SA_COUNT(kClonesNue2) - 1;
    RunKind("k2", kClonesNue2, SA_COUNT(kClonesNue2), kTablesNue2, SA_COUNT(kTablesNue2), &NoSeed, &ArgsLastHook, 2, 2);
    g_last_hook = SA_COUNT(kClonesSample1) - 1;
    RunKind("k46", kClonesSample1, SA_COUNT(kClonesSample1), kTablesSample1, SA_COUNT(kTablesSample1), &NoSeed, &ArgsLastHook, 39, 46);
    RunKind("b2", kClones02, SA_COUNT(kClones02), nullptr, 0, &SeedBoss002, &Args02, 2, -1);
    RunKind("b3", kClones03, SA_COUNT(kClones03), nullptr, 0, &SeedBoss002, nullptr, 3, -1);
    RunKind("b39", kClones39, SA_COUNT(kClones39), nullptr, 0, &SeedBoss002, nullptr, 39, -1);
    RunKind("k39", kClonesWeretigr, SA_COUNT(kClonesWeretigr), kTablesWeretigr, SA_COUNT(kTablesWeretigr), &SeedWeretigr, &ArgsWeretigr, 33, 39);
}

}  // namespace boss_sa

// BOF3X_SHADOW=boss_sj: group BSJ's 44 functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per unit - seven
// (BOF3X_BSJ_RUN=k59|b52|f4|k61|b54|b55|f5 runs one). docs/boss_sj.md
// section 3.
//
// The clone rows are tools/boss_rows.py's (--unit <unit> --clones,
// 2026-09-28), each read against the disassembly; one row changed:
// BossMyriaFx_Follow is 0x197 bytes, not the tool's 0x14A (0x44103A is its own
// branch, not a function), so its four calls to 0x441090 are all in it. The
// tables' entry counts are the code's (the dispatchers' state values, the
// next table's address), not the tool's extents. Every function is called
// the way its root calls it (Clone::shape): a set-up as Boss_SetupTable's
// jmp, a hook with its word, a kind's dispatcher with its state byte drawn
// below its table, the effect tasks as BattleTask_RunAll runs them (kTask:
// Sprite_Current a task slot) - the first group to use that shape.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sj.h"
#include "game/boss_sj_callees.h"
#include "game/move_script_bytes.h"

namespace boss_sj {
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
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

// A clone row: name, base, size, calls, the ours, the answer's mask, the shape.
bh::Clone Row(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, U ret, S shape,
              const bh::Imm* imms = nullptr, int n_imms = 0) {
    return bh::Clone{name, base, size, calls, n, imms, n_imms, nullptr, 0, ours, ret, false, shape};
}
// A kind's dispatcher: by Sprite_Current[at], its table's entries drawn each
// round; ours answers the entry's eax, as the original's jmp does.
bh::Clone Disp(const char* name, U base, const void* ours, std::uint8_t at, std::uint8_t states) {
    bh::Clone c{name, base, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, ours, 0xFF, false, S::kDispatch};
    c.state_at = at;
    c.states = states;
    return c;
}

// ===========================================================================
// What every run shares
// ===========================================================================

// Port_DroppedCall sits in both kinds' +1 tables (entries 1 and 10): a bare
// ret, listed with no arguments (docs/boss_sa.md section 3). Sprite_PoseFromSet
// reads the animation's low byte and Battle_RemoveFromTurnOrder the actor's
// (the originals push a register whose upper bytes are a callee's leftovers).
// Battle_RemoveFromTurnOrder is louder than the real one: the end hooks read
// the member's +0x90 and +8 after it, and the next member's +0 - the effect
// moves them (as boss_se_fuzz.cpp's). 0x441090 (nobody's) takes two whole
// words; BossMyriaFx_Follow, the group's own called directly by the task's
// states, is a kPhase recorder (it logs the sprite it ran for).
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
    {"Sprite_PoseFromSet", ::bof3::addr::Sprite_PoseFromSet, KeyOf(&::Sprite_PoseFromSet), 3, {kU8, kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"Battle_RemoveFromTurnOrder", ::bof3::addr::Battle_RemoveFromTurnOrder, KeyOf(&::Battle_RemoveFromTurnOrder), 1, {kU8},
     bh::Answer::kGarbage, 0, 0, {}, &RemoveEffect},
    {"0x441090", at::kRoundHigh, at::kRoundHigh, 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"BossMyriaFx_Follow", 0x440EF0, 0x440EF0, 0, {}, bh::Answer::kPhase, 0, 0},
};

// The cells beyond the harness's battle frame the 44 read or write.
const bh::Region kRegions[] = {
    {at::kScriptVar3, 4},        // movement-script variable 3 (set-up 54's end)
    {0x9035A4, 4},               // Field_ActiveMember (set-up 52's exit)
};

const bh::Clone* g_cur = nullptr;   // the Run's clones (for Seed and Args)

unsigned char& B(U address) { return Mem(address)[0]; }
U Above() { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0; }
unsigned char Byte(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<unsigned char>(bh::Next());
    const U* v = often.begin();
    return static_cast<unsigned char>(v[bh::Next() % often.size()]);
}
unsigned char* Owner() { return bh::Pointer(at::kOwner); }

// The words: an enemy hook's 0..2 (the three callers') with garbage above the
// byte half the time - the hook tables index by the low byte and hand the
// word on whole.
void Args(unsigned k, U* a) {
    if (g_cur[k].shape == S::kEnemyHook) a[0] = Above() | (bh::Next() % 3);
}

// The end hooks: the win bit (and garbage), each member's +0 bit 0 two times
// in three, its +8 where the byte add wraps.
void SeedEnd() {
    B(at::kBattleEnd) = Byte({0, 1, 2, 3, 0xFD, 0x82, 8, 0xA});
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(m));
        p[0] = static_cast<unsigned char>(bh::Often() ? p[0] | 1 : p[0] & ~1u);
        if (bh::Often()) p[8] = static_cast<unsigned char>(BH_PICK(0, 1, 0xE3, 0xE4, 0xFC, 0xFB, 0x7F));
        if (bh::Half()) p[0x91] ^= 0x40;
    }
}

// A dispatcher's other state bytes inside their tables, so a dispatcher reading
// the wrong byte lands on another entry (a count) rather than past its table
// (a Fatal); the byte the harness drew is left alone.
void OtherStates(unsigned at, unsigned n1, unsigned n2, unsigned n3) {
    unsigned char* const s = Sprite_Current;
    const unsigned n[4] = {0, n1, n2, n3};
    for (unsigned b = 1; b <= 3; ++b)
        if (b != at && n[b]) s[b] = static_cast<unsigned char>(bh::Next() % n[b]);
}

// --- kinds 59 and 61 ---------------------------------------------------------------

void SeedKind(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (g_cur[k].base) {
    case 0x43F7A0: OtherStates(1, 12, 6, 12); break;   // BossDLord_Dispatch: +2 inside the act table
    case 0x43F820: OtherStates(2, 12, 6, 6); break;    // BossDLord_ActDispatch: +1 and +3 inside tables
    case 0x43FBB0: OtherStates(1, 12, 12, 12); break;  // BossShroom_Dispatch
    case 0x43F8B0:                                     // BossDLord_HookFx: the three tests, the acting enemy
        s[1] = Byte({7, 7, 7, 6, 8, 0x87});
        s[2] = Byte({1, 1, 1, 0, 2, 0x81});
        s[9] = Byte({0, 0, 0, 1, 0xFF, 0x80});
        // an enemy 3..10 two times in three, else the harness's member 0..2
        // (whose object "0x93B960 - (3 - actor) * 0x128" is inside the task slots)
        if (bh::Often()) B(at::kActor) = static_cast<unsigned char>(3 + bh::Next() % 8);
        break;
    default: break;
    }
}

// What the kinds' functions read again after a call: the acting enemy
// (BossDLord_HookFx reads 0x904B34 after BattleTask_Create; the harness moves
// it only to a member).
void DisturbKind(U h) { B(at::kActor) = static_cast<unsigned char>(3 + (h >> 16) % 8); }

// --- the effect tasks --------------------------------------------------------------

// Myria's task: its +1 below the 7 states (the dispatcher and Follow index by
// it), +2 below the state's stack table; the pose word 0x904B7E below 10 (the
// ten-byte tables; ours aborts past them), its ends 0 and 9 often; the
// owner's +0xB and +1 (2 the "done" the waits test) at their boundaries;
// +0x48 set or not; the scales +0x40 / +0x44 at the round-up's cases (the
// sign, a zero low word).
void SeedTask(unsigned k) {
    unsigned char* const s = Sprite_Current;
    const U base = g_cur[k].base;
    s[1] = static_cast<unsigned char>(bh::Next() % 7);
    s[2] = static_cast<unsigned char>(bh::Next() % (base == 0x440DA0 ? 3 : base == 0x43FAE0 ? 3 : 2));
    if (base == 0x43FAC0) s[1] = 0;
    for (unsigned t = 0; t < 4; ++t) bh::TaskAt(t)[1] = static_cast<unsigned char>(bh::Next() % 7);
    SetWord(Mem(at::kPoseIndex), static_cast<unsigned>(bh::Often() ? BH_PICK(0, 9, 7, 8, 1, 2, 4, 5) : bh::Next() % 10));
    unsigned char* const o = Owner();
    o[0xB] = static_cast<unsigned char>(bh::Half() ? 0 : Byte({1, 0x80, 0xFF}));
    o[1] = Byte({2, 2, 2, 1, 3, 0x82});
    o[0x48] = static_cast<unsigned char>(bh::Half() ? 0 : Byte({1, 0x80}));
    SetLong(o + 0x40, static_cast<std::int32_t>(bh::Often() ? BH_PICK(0x10000, 0x18000, 0, 1, 0xFFFF0000u, 0xFFFFFFFFu, 0x7FFF0000, 0x8000) : bh::Next()));
    SetLong(o + 0x44, static_cast<std::int32_t>(bh::Often() ? BH_PICK(0x10000, 0x18000, 0, 1, 0xFFFF0000u, 0xFFFFFFFFu, 0x7FFF0000, 0x8000) : bh::Next()));
    // D>Lord's task: the count +9 at its ends, enemy 0's record index
    switch (base) {
    case 0x43FB60: s[9] = Byte({0, 0, 1, 2, 0xFF, 0xFF, 0xFE}); break;
    case 0x43FB10:
        B(at::kEnemy0Type) = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next());
        break;
    default: break;
    }
}

// What the tasks read again after a call: the owner's +1 and +0xB (the waits
// read the owner after the tick), the pose word (the entries read it after
// the bank call), the round flags' bit 2.
void DisturbTask(U h) {
    const U v = h >> 16;
    switch ((h >> 8) % 4) {
    case 0: Owner()[1] = static_cast<unsigned char>(v & 1 ? 2 : v >> 1); break;
    case 1: Owner()[0xB] = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 2: SetWord(Mem(at::kPoseIndex), v % 10); break;
    default: B(at::kFlags) = static_cast<unsigned char>(B(at::kFlags) ^ 4); break;
    }
}

// ===========================================================================
// The clone rows (tools/boss_rows.py --unit <UNIT> --clones)
// ===========================================================================

// K59, D>Lord
constexpr bh::CallSite kCalls43F7C0[] = {{0x4C, 0x5893A0}};
constexpr bh::CallSite kCalls43F840[] = {{0x2, 0x589330}, {0xA, 0x589410}, {0xF, 0x437470}};
constexpr bh::CallSite kCalls43F8B0[] = {{0x27, 0x435180}};
const bh::Clone kK59[] = {
    Disp("BossDLord_Dispatch", 0x43F7A0, BH_FN(BossDLord_Dispatch), 1, 12),
    Row("BossDLord_Enter", 0x43F7C0, 0x51, kCalls43F7C0, BH_N(kCalls43F7C0), BH_FN(BossDLord_Enter), 0xFF, S::kState),
    Disp("BossDLord_ActDispatch", 0x43F820, BH_FN(BossDLord_ActDispatch), 2, 6),
    Row("BossDLord_Death", 0x43F840, 0x52, kCalls43F840, BH_N(kCalls43F840), BH_FN(BossDLord_Death), 0, S::kState),
    Row("BossDLord_Hook", 0x43F8A0, 0x10, nullptr, 0, BH_FN(BossDLord_Hook), 0, S::kEnemyHook),
    Row("BossDLord_HookFx", 0x43F8B0, 0x9E, kCalls43F8B0, BH_N(kCalls43F8B0), BH_FN(BossDLord_HookFx), 0, S::kEnemyHook),
};
// the hook table first (its BareRet logs the hook's word); then the +1 and +2 tables
const bh::DataTable kTablesK59[] = {{0x64DC48, 3, 4, 1}, {0x64DC00, 12}, {0x64DC30, 6}};

// B52
constexpr bh::CallSite kCalls43F970[] = {{0x1C, 0x446650}, {0x58, 0x589110}, {0x6F, 0x446650}, {0xAB, 0x589110},
                                         {0xC2, 0x446650}, {0xFE, 0x589110}, {0x10D, 0x446DE0}, {0x112, 0x446E00}};
constexpr bh::CallSite kCalls43FA90[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x1A, 0x5891F0}};
const bh::Clone kB52[] = {
    Row("Boss52_Setup", 0x43F950, 0x1F, nullptr, 0, BH_FN(Boss52_Setup), 0, S::kSetup),
    Row("Boss52_End", 0x43F970, 0x117, kCalls43F970, BH_N(kCalls43F970), BH_FN(Boss52_End), 0, S::kEnd),
    Row("Boss52_Exit", 0x43FA90, 0x23, kCalls43FA90, BH_N(kCalls43FA90), BH_FN(Boss52_Exit), 0, S::kExit),
};

// F4, D>Lord's effect
constexpr bh::Imm kImms43FAE0[] = {{0xF, 0x43FB10}, {0x17, 0x43FB60}, {0x22, 0x4AEE90}};
constexpr bh::CallSite kCalls43FB10[] = {{0x30, 0x5891F0}, {0x38, 0x5893A0}, {0x3D, 0x5890E0}};
constexpr bh::CallSite kCalls43FB60[] = {{0x16, 0x587740}, {0x2E, 0x5893A0}, {0x33, 0x5890E0}};
const bh::Clone kF4[] = {
    Row("BossDLordFx_Dispatch", 0x43FAC0, 0x12, nullptr, 0, BH_FN(BossDLordFx_Dispatch), 0, S::kTask),
    Row("BossDLordFx_StepDispatch", 0x43FAE0, 0x2E, nullptr, 0, BH_FN(BossDLordFx_StepDispatch), 0, S::kTask, kImms43FAE0,
        BH_N(kImms43FAE0)),
    Row("BossDLordFx_Start", 0x43FB10, 0x4B, kCalls43FB10, BH_N(kCalls43FB10), BH_FN(BossDLordFx_Start), 0, S::kTask),
    Row("BossDLordFx_Count", 0x43FB60, 0x4A, kCalls43FB60, BH_N(kCalls43FB60), BH_FN(BossDLordFx_Count), 0, S::kTask),
};
const bh::DataTable kTablesF4[] = {{0x64DC54, 1}};

// K61, Shroom
constexpr bh::CallSite kCalls43FBD0[] = {{0x4C, 0x5893A0}};
const bh::Clone kK61[] = {
    Disp("BossShroom_Dispatch", 0x43FBB0, BH_FN(BossShroom_Dispatch), 1, 12),
    Row("BossShroom_Enter", 0x43FBD0, 0x51, kCalls43FBD0, BH_N(kCalls43FBD0), BH_FN(BossShroom_Enter), 0xFF, S::kState),
    Row("BossShroom_Hook", 0x43FC30, 0x10, nullptr, 0, BH_FN(BossShroom_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK61[] = {{0x64DCA0, 3, 4, 1}, {0x64DC70, 12}};

// B54, B55
constexpr bh::CallSite kCalls43FC60[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
const bh::Clone kB54[] = {
    Row("Boss54_Setup", 0x43FC40, 0x1F, nullptr, 0, BH_FN(Boss54_Setup), 0, S::kSetup),
    Row("Boss54_End", 0x43FC60, 0x1A, kCalls43FC60, BH_N(kCalls43FC60), BH_FN(Boss54_End), 0, S::kEnd),
};
constexpr bh::CallSite kCalls440700[] = {{0x1C, 0x446650}, {0x58, 0x589110}, {0x6F, 0x446650}, {0xAB, 0x589110},
                                         {0xC2, 0x446650}, {0xFE, 0x589110}, {0x10D, 0x446E20}, {0x11A, 0x446E00}};
const bh::Clone kB55[] = {
    Row("Boss55_Setup", 0x4406E0, 0x1F, nullptr, 0, BH_FN(Boss55_Setup), 0, S::kSetup),
    Row("Boss55_End", 0x440700, 0x11F, kCalls440700, BH_N(kCalls440700), BH_FN(Boss55_End), 0, S::kEnd),
};

// F5, Myria's effect
constexpr bh::Imm kImms440850[] = {{0xF, 0x440880}, {0x17, 0x4408C0}};
constexpr bh::CallSite kCalls440880[] = {{0x5, 0x589590}, {0x2A, 0x4408C0}};
constexpr bh::CallSite kCalls4408C0[] = {{0x47, 0x5891F0}, {0x4F, 0x436090}, {0x54, 0x5890E0}, {0x59, 0x440EF0}};
constexpr bh::Imm kImms440930[] = {{0xF, 0x440960}, {0x17, 0x4409A0}};
constexpr bh::CallSite kCalls440960[] = {{0x5, 0x589590}, {0x2A, 0x4409A0}};
constexpr bh::CallSite kCalls4409A0[] = {{0x49, 0x5891F0}, {0x51, 0x436090}, {0x56, 0x5890E0}, {0x5B, 0x440EF0}};
constexpr bh::Imm kImms440A10[] = {{0xF, 0x440A40}, {0x17, 0x440A80}};
constexpr bh::CallSite kCalls440A40[] = {{0x5, 0x589590}, {0x2A, 0x440A80}};
constexpr bh::CallSite kCalls440A80[] = {{0x49, 0x5891F0}, {0x51, 0x436090}, {0x56, 0x5890E0}, {0x5B, 0x440EF0}};
constexpr bh::Imm kImms440AF0[] = {{0xF, 0x440B20}, {0x17, 0x440BB0}};
constexpr bh::CallSite kCalls440B20[] = {{0x36, 0x589590}, {0x62, 0x5891F0}, {0x6A, 0x4360C0}, {0x6F, 0x5890E0}, {0x74, 0x440EF0}};
constexpr bh::CallSite kCalls440BB0[] = {{0x0, 0x436090}, {0x10, 0x4351F0}, {0x15, 0x5890E0}, {0x1A, 0x440EF0}};
constexpr bh::Imm kImms440BD0[] = {{0xF, 0x440C00}, {0x17, 0x440C90}};
constexpr bh::CallSite kCalls440C00[] = {{0x34, 0x589590}, {0x60, 0x5891F0}, {0x68, 0x4360C0}, {0x6D, 0x5890E0}, {0x72, 0x440EF0}};
constexpr bh::CallSite kCalls440C90[] = {{0x0, 0x4360C0}, {0x1F, 0x4351F0}, {0x24, 0x5890E0}, {0x29, 0x440EF0}};
constexpr bh::Imm kImms440CC0[] = {{0xF, 0x440CF0}, {0x17, 0x440D80}};
constexpr bh::CallSite kCalls440CF0[] = {{0x36, 0x589590}, {0x62, 0x5891F0}, {0x6A, 0x436090}, {0x6F, 0x5890E0}, {0x74, 0x440EF0}};
constexpr bh::CallSite kCalls440D80[] = {{0xB, 0x4351F0}, {0x10, 0x436090}, {0x15, 0x5890E0}, {0x1A, 0x440EF0}};
constexpr bh::Imm kImms440DA0[] = {{0xF, 0x440DD0}, {0x17, 0x440E60}, {0x22, 0x440ED0}};
constexpr bh::CallSite kCalls440DD0[] = {{0x36, 0x589590}, {0x62, 0x5891F0}, {0x6A, 0x436090}, {0x6F, 0x5890E0}, {0x74, 0x440EF0}};
constexpr bh::CallSite kCalls440E60[] = {{0x4B, 0x5891F0}, {0x5B, 0x436090}, {0x60, 0x5890E0}, {0x65, 0x440EF0}};
constexpr bh::CallSite kCalls440ED0[] = {{0x0, 0x4360C0}, {0x11, 0x4351F0}, {0x16, 0x5890E0}, {0x1B, 0x440EF0}};
// the tool's row stops at 0x44103A and refuses the je there; the function runs to 0x441087
constexpr bh::CallSite kCalls440EF0[] = {{0x117, 0x441090}, {0x13D, 0x441090}, {0x163, 0x441090}, {0x18A, 0x441090}};
#define BSJ_TASK(name, base, size, calls) Row(#name, base, size, calls, BH_N(calls), BH_FN(name), 0, S::kTask)
#define BSJ_STEPS(name, base, imms) Row(#name, base, sizeof(imms) == 3 * sizeof(bh::Imm) ? 0x2E : 0x26, nullptr, 0, BH_FN(name), 0, S::kTask, imms, BH_N(imms))
const bh::Clone kF5[] = {
    Row("BossMyriaFx_Dispatch", 0x440830, 0x1C, nullptr, 0, BH_FN(BossMyriaFx_Dispatch), 0, S::kTask),
    BSJ_STEPS(BossMyriaFx_State0, 0x440850, kImms440850),
    BSJ_TASK(BossMyriaFx_State0Enter, 0x440880, 0x38, kCalls440880),
    BSJ_TASK(BossMyriaFx_State0Loop, 0x4408C0, 0x62, kCalls4408C0),
    BSJ_STEPS(BossMyriaFx_State1, 0x440930, kImms440930),
    BSJ_TASK(BossMyriaFx_State1Enter, 0x440960, 0x38, kCalls440960),
    BSJ_TASK(BossMyriaFx_State1Loop, 0x4409A0, 0x64, kCalls4409A0),
    BSJ_STEPS(BossMyriaFx_State2, 0x440A10, kImms440A10),
    BSJ_TASK(BossMyriaFx_State2Enter, 0x440A40, 0x38, kCalls440A40),
    BSJ_TASK(BossMyriaFx_State2Loop, 0x440A80, 0x64, kCalls440A80),
    BSJ_STEPS(BossMyriaFx_State3, 0x440AF0, kImms440AF0),
    BSJ_TASK(BossMyriaFx_State3Enter, 0x440B20, 0x86, kCalls440B20),
    BSJ_TASK(BossMyriaFx_State3Wait, 0x440BB0, 0x1F, kCalls440BB0),
    BSJ_STEPS(BossMyriaFx_State4, 0x440BD0, kImms440BD0),
    BSJ_TASK(BossMyriaFx_State4Enter, 0x440C00, 0x84, kCalls440C00),
    BSJ_TASK(BossMyriaFx_State4Wait, 0x440C90, 0x2E, kCalls440C90),
    BSJ_STEPS(BossMyriaFx_State5, 0x440CC0, kImms440CC0),
    BSJ_TASK(BossMyriaFx_State5Enter, 0x440CF0, 0x86, kCalls440CF0),
    BSJ_TASK(BossMyriaFx_State5Wait, 0x440D80, 0x1F, kCalls440D80),
    BSJ_STEPS(BossMyriaFx_State6, 0x440DA0, kImms440DA0),
    BSJ_TASK(BossMyriaFx_State6Enter, 0x440DD0, 0x86, kCalls440DD0),
    BSJ_TASK(BossMyriaFx_State6Loop, 0x440E60, 0x6E, kCalls440E60),
    BSJ_TASK(BossMyriaFx_State6Wait, 0x440ED0, 0x20, kCalls440ED0),
    BSJ_TASK(BossMyriaFx_Follow, 0x440EF0, 0x197, kCalls440EF0),
};
#undef BSJ_TASK
#undef BSJ_STEPS
const bh::DataTable kTablesF5[] = {{0x64DDD0, 7}};

// BOF3X_BSJ_RUN=<unit, lower case> runs that one alone (the controls
// script's shortcut); unset, all seven run.
bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BSJ_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

void SeedEndOnly(unsigned) { SeedEnd(); }

void RunUnit(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables,
             void (*seed)(unsigned), void (*disturb)(U), int fight, int kind, unsigned span = 0) {
    if (!Wants(run)) return;
    g_cur = clones;
    bh::Group g{"boss_sj", clones, n, kCallees, BH_COUNT(kCallees), tables, n_tables, kRegions, BH_COUNT(kRegions),
                seed, disturb, 6000};
    g.args = &Args;
    g.fight = fight;
    g.kind = kind;
    g.phase_span = span;
    bh::Run(g);
    g_cur = nullptr;
}

}  // namespace

void SelfTest() {
    RunUnit("k59", kK59, BH_COUNT(kK59), kTablesK59, BH_COUNT(kTablesK59), &SeedKind, &DisturbKind, 52, 59);
    RunUnit("b52", kB52, BH_COUNT(kB52), nullptr, 0, &SeedEndOnly, nullptr, 52, -1);
    RunUnit("f4", kF4, BH_COUNT(kF4), kTablesF4, BH_COUNT(kTablesF4), &SeedTask, &DisturbTask, 52, -1, 7);
    RunUnit("k61", kK61, BH_COUNT(kK61), kTablesK61, BH_COUNT(kTablesK61), &SeedKind, nullptr, 54, 61);
    RunUnit("b54", kB54, BH_COUNT(kB54), nullptr, 0, &SeedEndOnly, nullptr, 54, -1);
    RunUnit("b55", kB55, BH_COUNT(kB55), nullptr, 0, &SeedEndOnly, nullptr, 55, -1);
    RunUnit("f5", kF5, BH_COUNT(kF5), kTablesF5, BH_COUNT(kTablesF5), &SeedTask, &DisturbTask, 55, -1, 7);
}

}  // namespace boss_sj

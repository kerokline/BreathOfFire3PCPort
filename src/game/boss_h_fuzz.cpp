// BOF3X_SHADOW=boss_h: the 20 shared boss helpers through the boss harness
// (boss_harness.h), once at start-up: three boss_harness::Run - kinds 8..11's
// death chain (fight 8, the kind byte seeded 8..11), the state helpers and
// the map callees, the hooks. docs/boss_h.md section 3.
//
// The clone rows are tools/boss_rows.py's (--unit H --clones, 2026-09-28),
// each read against the disassembly. A helper that is reached only through a
// unit's table is driven the way the unit reaches it (Clone::via): the unit's
// dispatcher is called with the state byte that selects the helper's entry
// and the helper planted in that entry - BossTorast_ActDispatch through each
// of kinds 8..11's dispatchers, the chain below it through its own
// dispatchers, the state helpers through kinds 6 and 21's, the enemy hook and
// Boss_Nop through their kinds' hook tables.
#include <cstddef>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_h.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"

namespace boss_h {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

// The .data tables and cells the helpers name (symbols.toml).
constexpr U kActSubs = bof3::addr::BossTorast_ActSubs;          // 0x64CAD4, 6
constexpr U kDeathSubs = bof3::addr::BossTorast_DeathSubs;      // 0x64CAEC, 2
constexpr U kDeathFxSteps = bof3::addr::BossTorast_DeathFxSteps;   // 0x64CAF4, 4

// ===========================================================================
// Kinds 8..11's death chain
// ===========================================================================

// clones: tools/boss_rows.py --unit H --clones
constexpr bh::CallSite kCalls437CA0[] = {{0x0, 0x5893A0}};
constexpr bh::CallSite kCalls438F10[] = {{0x19, 0x587740}};
constexpr bh::CallSite kCalls438F40[] = {{0x5F, 0x4449E0}};
constexpr bh::CallSite kCalls438FD0[] = {{0x9, 0x4394A0}, {0x3E, 0x587740}};
constexpr bh::CallSite kCalls439030[] = {{0xA, 0x4394A0}, {0x29, 0x589590}, {0x2F, 0x5891F0}, {0x37, 0x437470}};
constexpr bh::CallSite kCalls4394A0[] = {{0x23, 0x5A7A00}, {0x32, 0x5A7A50}, {0x5B, 0x5A79A0}, {0x73, 0x5A77C0},
                                         {0x7C, 0x461E50}, {0x99, 0x5A75F0}, {0x1E8, 0x5A7780}, {0x1F1, 0x461E50}};

// A clone driven through a dispatcher: its unit's dispatcher, the table cell
// the state selects, the state byte's offset and value.
bh::Clone Via(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret,
              std::uint8_t states_at, std::uint8_t states, U dispatcher, U cell, std::uint8_t at, std::uint8_t state) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    c.state_at = states_at;
    c.states = states;
    c.via = {dispatcher, cell, at, state};
    return c;
}

// Kinds 8..11's dispatchers (BossKind_Table[8..11], BSB's) and the cell of
// their +1 tables that state 6 selects.
constexpr U kKindDispatch[4] = {0x438E50, 0x4390F0, 0x439160, 0x4391D0};
constexpr U kKindState6[4] = {0x64CABC, 0x64CB28, 0x64CB64, 0x64CBA0};

const bh::Clone kClonesTorast[] = {
    Via("BossTorast_ActDispatch (kind 8)", 0x438EB0, 0x12, nullptr, 0, BH_FN(BossTorast_ActDispatch), S::kDispatch, 0, 2, 6,
        kKindDispatch[0], kKindState6[0], 1, 6),
    Via("BossTorast_ActDispatch (kind 9)", 0x438EB0, 0x12, nullptr, 0, BH_FN(BossTorast_ActDispatch), S::kDispatch, 0, 2, 6,
        kKindDispatch[1], kKindState6[1], 1, 6),
    Via("BossTorast_ActDispatch (kind 10)", 0x438EB0, 0x12, nullptr, 0, BH_FN(BossTorast_ActDispatch), S::kDispatch, 0, 2, 6,
        kKindDispatch[2], kKindState6[2], 1, 6),
    Via("BossTorast_ActDispatch (kind 11)", 0x438EB0, 0x12, nullptr, 0, BH_FN(BossTorast_ActDispatch), S::kDispatch, 0, 2, 6,
        kKindDispatch[3], kKindState6[3], 1, 6),
    Via("BossTorast_DeathDispatch", 0x438ED0, 0x12, nullptr, 0, BH_FN(BossTorast_DeathDispatch), S::kDispatch, 0, 3, 2,
        0x438EB0, kActSubs + 4 * 4, 2, 4),
    Via("BossTorast_DeathFxDispatch", 0x438EF0, 0x12, nullptr, 0, BH_FN(BossTorast_DeathFxDispatch), S::kDispatch, 0, 4, 4,
        0x438ED0, kDeathSubs + 4 * 0, 3, 0),
    Via("BossOp_ScriptTick", 0x437CA0, 0x5, kCalls437CA0, BH_N(kCalls437CA0), BH_FN(BossOp_ScriptTick), S::kState, 0xFF, 1, 0,
        0x438ED0, kDeathSubs + 4 * 1, 3, 1),
    Via("BossTorast_DeathFxStart", 0x438F10, 0x2A, kCalls438F10, BH_N(kCalls438F10), BH_FN(BossTorast_DeathFxStart), S::kState, 0, 1, 0,
        0x438EF0, kDeathFxSteps + 4 * 0, 4, 0),
    Via("BossTorast_DeathFxFlash", 0x438F40, 0x86, kCalls438F40, BH_N(kCalls438F40), BH_FN(BossTorast_DeathFxFlash), S::kState, 0, 1, 0,
        0x438EF0, kDeathFxSteps + 4 * 1, 4, 1),
    Via("BossTorast_DeathFxRingGrow", 0x438FD0, 0x55, kCalls438FD0, BH_N(kCalls438FD0), BH_FN(BossTorast_DeathFxRingGrow), S::kState, 0, 1,
        0, 0x438EF0, kDeathFxSteps + 4 * 2, 4, 2),
    Via("BossTorast_DeathFxRingShrink", 0x439030, 0xAE, kCalls439030, BH_N(kCalls439030), BH_FN(BossTorast_DeathFxRingShrink), S::kState, 0,
        1, 0, 0x438EF0, kDeathFxSteps + 4 * 3, 4, 3),
    {"BossTorast_DrawRing", 0x4394A0, 0x20F, kCalls4394A0, BH_N(kCalls4394A0), nullptr, 0, nullptr, 0, BH_FN(BossTorast_DrawRing), 0, false,
     S::kCallee},
};
enum : unsigned { kAct8, kAct9, kAct10, kAct11, kDeath, kDeathFx, kTick, kStart, kFlash, kGrow, kShrink, kRing };
static_assert(kRing + 1 == BH_COUNT(kClonesTorast), "the chain's seeding indices");

const bh::Callee kCalleesTorast[] = {
    {"BossTorast_DrawRing", 0x4394A0, KeyOf(&::BossTorast_DrawRing), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
};
// The three shared tables, their entries recorders (BossTorast_DeathDispatch
// and the steps among them: a clone driven through its dispatcher plants
// itself over its recorder for the pass).
const bh::DataTable kTablesTorast[] = {{kActSubs, 6}, {kDeathSubs, 2}, {kDeathFxSteps, 4}};

// A count and an interval: equal half the time, the halving sequence and its
// ends, or anything.
unsigned char Count() { return static_cast<unsigned char>(bh::Often() ? BH_PICK(0x40, 0x20, 0x10, 8, 4, 2, 1, 0, 0x41, 0x80) : bh::Next()); }
std::int32_t Speed() {
    return static_cast<std::int32_t>(bh::Often() ? BH_PICK(0, 1, 0x1FFF, 0x2000, 0x2001, 0x10000, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFF, 0xFFFFE000u)
                                                 : bh::Next());
}

void SeedTorast(unsigned k) {
    unsigned char* const s = Sprite_Current;
    unsigned char* const e = bh::Pointer(bh::at::kEnemyCurrent);
    // the kind whose colour is drawn: 8..11 mostly (the table's four), else any
    if (bh::Often()) e[0x100] = static_cast<unsigned char>(8 + bh::Next() % 4);
    switch (k) {
    case kFlash:
        s[0xA] = Count();
        s[9] = bh::Half() ? s[0xA] : Count();
        break;
    case kGrow:
        s[9] = static_cast<unsigned char>(bh::Often() ? BH_PICK(0x30, 0x2C, 0x34, 0, 4, 0xB0, 0x31) : bh::Next());
        break;
    case kShrink:
        s[9] = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 4, 8, 0x30, 1, 0x80) : bh::Next());
        SetLong(s + 0x40, Speed());
        SetLong(s + 0x44, Speed());
        break;
    default:
        break;
    }
}

// The ring's radius: its callers' values (0..0x30 in fours), with garbage
// above the byte.
void ArgsTorast(unsigned k, U* a) {
    if (k != kRing) return;
    a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Often() ? 4 * (bh::Next() % 13) : bh::Next() & 0xFF);
}

// ===========================================================================
// The state helpers and the map callees
// ===========================================================================

constexpr bh::CallSite kCalls43A720[] = {{0x0, 0x589410}};
constexpr bh::CallSite kCalls43B550[] = {{0x0, 0x589410}, {0x5, 0x437470}};
constexpr bh::JumpTable kTables43B130[] = {{0xF, 0x34, 7}};
constexpr bh::CallSite kCalls43B180[] = {{0x17, 0x57C0F0}, {0x28, 0x589330}, {0x38, 0x57C110}, {0x49, 0x589330},
                                         {0x52, 0x43B0D0}, {0x72, 0x57C0F0}, {0x83, 0x589330}, {0x92, 0x57C110},
                                         {0xA3, 0x589330}, {0xAC, 0x43B0D0}, {0xC2, 0x5720C0}, {0xDA, 0x5720C0}};

const bh::Clone kClonesOps[] = {
    // kind 21's dispatcher (0x43A660, BSD's) at state 1
    Via("BossOp_EnterTick", 0x43A720, 0x13, kCalls43A720, BH_N(kCalls43A720), BH_FN(BossOp_EnterTick), S::kState, 0, 1, 0,
        0x43A660, 0x64CF7C, 1, 1),
    // kind 6's +2 dispatcher (0x437A70, BSA's) at +2 = 4
    Via("BossOp_Death", 0x43B550, 0x48, kCalls43B550, BH_N(kCalls43B550), BH_FN(BossOp_Death), S::kState, 0, 1, 0, 0x437A70, 0x64C7F0,
        2, 4),
    // kind 21's hook table (0x43A740 by the hook's word, BSD's), entry 0
    Via("BossHook_RetargetMember0", 0x43A750, 0x11, nullptr, 0, BH_FN(BossHook_RetargetMember0), S::kEnemyHook, 0, 1, 0, 0x43A740,
        0x64CFA8, 0, 0),
    // kind 8's hook table (0x4390E0 by the hook's word, BSB's), entry 0
    Via("Boss_Nop", 0x437CC0, 0x1, nullptr, 0, BH_FN(Boss_Nop), S::kEnemyHook, 0, 1, 0, 0x4390E0, 0x64CB04, 0, 0),
    {"BossMap_SetCorners", 0x43B0D0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, BH_FN(BossMap_SetCorners), 0, false, S::kCallee},
    {"Boss_SetByLeaderId", 0x43B130, 0x50, nullptr, 0, nullptr, 0, kTables43B130, BH_N(kTables43B130), BH_FN(Boss_SetByLeaderId), 0, false,
     S::kCallee},
    {"BossMap_UpdateFromEnemies", 0x43B180, 0xFE, kCalls43B180, BH_N(kCalls43B180), nullptr, 0, nullptr, 0, BH_FN(BossMap_UpdateFromEnemies),
     0, false, S::kCallee},
};
enum : unsigned { kEnter, kOpDeath, kRetarget, kNop, kCorners, kLeaderId, kUpdate };
static_assert(kUpdate + 1 == BH_COUNT(kClonesOps), "the helpers' seeding indices");

const bh::Callee kCalleesOps[] = {
    {"BossMap_SetCorners", 0x43B0D0, KeyOf(&::BossMap_SetCorners), 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0},
};
// The loaded area block as far as a corner index of any width reaches
// (255 * 16 + 0xC8 dwords past AreaMap_Corners), and the byte
// Boss_SetByLeaderId writes.
const bh::Region kRegionsOps[] = {{at::kMapHeader, 0x4400}, {at::kLeaderPick, 1}};

unsigned char StatusByte() {
    return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 0x20, 0x40, 0x60, 0x21, 0x9F, 0xBF, 0xDF) : bh::Next());
}

void SeedOps(unsigned k) {
    switch (k) {
    case kEnter:
    case kOpDeath:
        break;
    case kRetarget:
        Mem(at::kLeaderTarget)[0] = static_cast<unsigned char>(bh::Often() ? BH_PICK(4, 4, 3, 5, 0x84, 0) : bh::Next());
        break;
    case kLeaderId:
        Mem(at::kLeaderId)[0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next());
        break;
    case kUpdate:
        Mem(at::kEnemy1Status)[1] = StatusByte();
        Mem(at::kEnemy2Status)[1] = StatusByte();
        Mem(at::kLeaderFlags)[0] = static_cast<unsigned char>(bh::Next());
        [[fallthrough]];
    case kCorners:
        // the grid's width: small, the area's own (a few dozen cells), or any
        if (bh::Often()) Mem(at::kMapHeader)[0] = static_cast<unsigned char>(bh::Half() ? 0x20 + bh::Next() % 0x40 : bh::Next() % 4);
        break;
    default:
        break;
    }
}

// BossMap_SetCorners' two indexes: its callers' (cell 0..1, value 0..2), with
// garbage above neither - they are dwords the original indexes by whole.
void ArgsOps(unsigned k, U* a) {
    if (k != kCorners) return;
    a[0] = bh::Next() % 2;
    a[1] = bh::Next() % 3;
}

// What the helpers read again after a call: the enemies' status bytes (read
// once, before the calls: moving them shows a re-read), the grid's width
// (read for each corner), the leader's flags and target.
void DisturbOps(U h) {
    switch ((h >> 8) % 5) {
    case 0: Mem(at::kEnemy1Status)[1] = static_cast<unsigned char>(h >> 16); break;
    case 1: Mem(at::kEnemy2Status)[1] = static_cast<unsigned char>(h >> 16); break;
    case 2: Mem(at::kMapHeader)[0] = static_cast<unsigned char>((h >> 16) % 0x40); break;
    case 3: Mem(at::kLeaderFlags)[0] = static_cast<unsigned char>(h >> 16); break;
    default: Mem(at::kLeaderTarget)[0] = static_cast<unsigned char>(h >> 16); break;
    }
}

// ===========================================================================
// The hooks
// ===========================================================================

constexpr bh::CallSite kCalls43A4A0[] = {{0x2, 0x494A60}};
constexpr bh::CallSite kCalls43EB60[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
constexpr bh::CallSite kCalls440820[] = {{0x2, 0x4949D0}};

const bh::Clone kClonesHooks[] = {
    {"BossHook_EventNone", 0x43C9F0, 0x3, nullptr, 0, nullptr, 0, nullptr, 0, BH_FN(BossHook_EventNone), 0xFF, false, S::kEvent},
    {"BossHook_EndPickWay", 0x43EB60, 0x1A, kCalls43EB60, BH_N(kCalls43EB60), nullptr, 0, nullptr, 0, BH_FN(BossHook_EndPickWay), 0, false,
     S::kEnd},
    {"BossHook_ExitClearActor0", 0x43A4A0, 0x9, kCalls43A4A0, BH_N(kCalls43A4A0), nullptr, 0, nullptr, 0, BH_FN(BossHook_ExitClearActor0), 0,
     false, S::kExit},
    {"BossHook_ExitActor0Bit40", 0x440820, 0x9, kCalls440820, BH_N(kCalls440820), nullptr, 0, nullptr, 0, BH_FN(BossHook_ExitActor0Bit40), 0,
     false, S::kExit},
};
enum : unsigned { kEventNone, kEndPick, kExitClear, kExitBit };
static_assert(kExitBit + 1 == BH_COUNT(kClonesHooks), "the hooks' seeding indices");

void SeedHooks(unsigned k) {
    if (k == kEndPick)
        Mem(at::kBattleEnd)[0] = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 3, 0xFD, 0x82) : bh::Next());
}

}  // namespace

void SelfTest() {
    {
        bh::Group g{"boss_h", kClonesTorast, BH_COUNT(kClonesTorast), kCalleesTorast, BH_COUNT(kCalleesTorast), kTablesTorast,
                    BH_COUNT(kTablesTorast), nullptr, 0, &SeedTorast, nullptr, 6000};
        g.args = &ArgsTorast;
        g.fight = 8;
        bh::Run(g);
    }
    {
        bh::Group g{"boss_h", kClonesOps, BH_COUNT(kClonesOps), kCalleesOps, BH_COUNT(kCalleesOps), nullptr, 0, kRegionsOps,
                    BH_COUNT(kRegionsOps), &SeedOps, &DisturbOps, 6000};
        g.args = &ArgsOps;
        g.fight = 18;
        bh::Run(g);
    }
    {
        bh::Group g{"boss_h", kClonesHooks, BH_COUNT(kClonesHooks), nullptr, 0, nullptr, 0, nullptr, 0, &SeedHooks, nullptr, 4000};
        bh::Run(g);
    }
}

}  // namespace boss_h

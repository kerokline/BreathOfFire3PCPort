// BOF3X_SHADOW=boss_sh: group BSH's 46 functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per unit -
// fourteen (BOF3X_BSH_RUN=k48|b34|b41|k41|k42|k54|b35|b47|k43|k50|b36|b43|f3|k44
// runs one). docs/boss_sh.md section 4.
//
// The clone rows are tools/boss_rows.py's (--unit <unit> --clones,
// 2026-09-28), each read against the disassembly; the tables' entry counts are
// the code's (the dispatchers' state values, the next table's address), not
// the tool's extents. Every function is called the way its root calls it
// (Clone::shape): a set-up as Boss_SetupTable's jmp, a hook with its phase code
// or word, a dispatcher with its state byte drawn below its table, the
// Angler's effect task (the kind-3 dispatcher's slot 3) as BattleTask_RunAll
// runs a slot (kTask).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sh.h"
#include "game/boss_sh_callees.h"
#include "game/move_script_bytes.h"

namespace boss_sh {
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

// A clone row: the tool's, with its shape, answer mask and (for a dispatcher)
// its state byte and table size.
bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0,
            std::uint8_t state_at = 1, std::uint8_t states = 0, const bh::Imm* imms = nullptr, int n_imms = 0) {
    bh::Clone c{name, base, size, calls, n, imms, n_imms, nullptr, 0, ours, ret, false, shape};
    c.state_at = state_at;
    c.states = states;
    return c;
}

// ===========================================================================
// What every run shares
// ===========================================================================

unsigned char& B(U address) { return Mem(address)[0]; }

// Battle_RemoveFromTurnOrder and Sprite_PoseFromSet are louder than the real
// ones: Boss34_End reads the member's +0x90 dword after the first and the
// next member's +0 after the second (and +8 of a member it has not reached) -
// the effect moves one of them in a random member.
std::uint32_t PartyEffect(const std::uint32_t*, std::uint32_t answer) {
    const U n = bh::Noise();
    unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(n % 3));
    switch ((n >> 4) % 3) {
    case 0: p[0x91] ^= 0x40; break;
    case 1: p[8] = static_cast<unsigned char>(n >> 8); break;
    default: p[0] ^= 1; break;
    }
    return answer;
}

// The group's callees: the two with louder effects (Sprite_PoseFromSet reads
// the animation's low byte - Sprite_SetFrameQueueUpload takes frame & 0xFF -
// and the original pushes a register whose upper bytes are leftovers), and the
// four engine functions nobody owns, with their arities (boss_sh_callees.h).
const bh::Callee kCallees[] = {
    {"Battle_RemoveFromTurnOrder", ::bof3::addr::Battle_RemoveFromTurnOrder, KeyOf(&::Battle_RemoveFromTurnOrder), 1, {kU8},
     bh::Answer::kGarbage, 0, 0, {}, &PartyEffect},
    {"Sprite_PoseFromSet", ::bof3::addr::Sprite_PoseFromSet, KeyOf(&::Sprite_PoseFromSet), 3, {kU8, kAll, kAll},
     bh::Answer::kGarbage, 0, 0, {}, &PartyEffect},
    {"0x446700", at::kOrderFront, at::kOrderFront, 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"0x437450", at::kEnemySound, at::kEnemySound, 1, {kU16}, bh::Answer::kGarbage, 0, 0},
    {"0x4376A0", at::kEnemyActEnd, at::kEnemyActEnd, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x4376F0", at::kEnemyActChance, at::kEnemyActChance, 0, {}, bh::Answer::kGarbage, 0, 0},
};

// The cells beyond the harness's battle frame the 46 read or write.
const bh::Region kRegions[] = {
    {at::kPickId, 4},        // the byte set-up 34 compares the members' +0x148 with
    {at::kPicked, 0x10},     // the member set-up 34 picked (read as a dword), and the 12 bytes kind 42 points at
    {0x66C810, 4},           // MoveScript_WaitWordDA
    {0x7E0918, 4},           // Draw_PassFlags
};

// The clones of the run in progress, for Args.
const bh::Clone* g_clones = nullptr;

U Above() { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0; }

// A dispatcher's other state bytes inside its table, so a dispatcher reading
// the wrong byte lands on another entry (a count) rather than past its table
// (a Fatal); never the byte the harness drew.
void OtherStates(unsigned drawn, unsigned below) {
    unsigned char* const s = Sprite_Current;
    for (unsigned b = 1; b <= 4; ++b)
        if (b != drawn) s[b] = static_cast<unsigned char>(bh::Next() % below);
}

// An enemy hook's word 0..2 (the three callers') with garbage above half the
// time - the hooks mask it, their entries get it whole; an event hook's code
// as its seed picked it (the seed aims the round's other inputs at that
// code's path; an args hook cannot write memory, docs/boss_harness.md section
// 6), with garbage above half the time; a dispatcher's word as drawn (handed
// on to the entry: Port_DroppedCall's recorder logs its byte).
U g_code = 0;
void Args(unsigned k, U* a) {
    const bh::Clone& c = g_clones[k];
    if (c.shape == S::kEnemyHook) {
        a[0] = Above() | (a[0] & 0xFF);
    } else if (c.shape == S::kEvent) {
        a[0] = Above() | g_code;
    }
}
// An event hook's code: `mine` two times in three, else any 0..6 or a byte past them.
U Code(U mine) { return bh::Often() ? mine : bh::Half() ? bh::Next() % 7 : bh::Next() & 0xFF; }

// What the 46 read again after a call that the standard disturbance does not
// move: set-up 34's picked member (after the first AbilityList_Add) and script
// bits, a party member's +0 / +8 / +0x91 (Boss34_End), the wait word and the
// id byte.
void Disturb(U h) {
    const U v = h >> 16;
    switch ((h >> 8) % 5) {
    case 0: B(at::kPicked) = static_cast<unsigned char>(v % 3); break;
    case 1: B(at::kScript) = static_cast<unsigned char>(v); break;
    case 2: bh::PartyOf(static_cast<unsigned char>(v % 3))[0] ^= 1; break;
    case 3: bh::PartyOf(static_cast<unsigned char>(v % 3))[0x91] ^= 0x40; break;
    default: SetWord(Mem(0x66C810), v & 1 ? 0 : v); break;
    }
}

bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BSH_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

void RunGroup(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables,
              void (*seed)(unsigned), int fight, int kind, unsigned rounds = 6000) {
    if (!Wants(run)) return;
    g_clones = clones;
    bh::Group g{"boss_sh", clones, n, kCallees, BH_COUNT(kCallees), tables, n_tables, kRegions, BH_COUNT(kRegions), seed,
                &Disturb, rounds};
    g.args = &Args;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
    g_clones = nullptr;
}

unsigned char EndByte() { return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 3, 0xFD, 0x82, 8, 0xA) : bh::Next()); }

// A kind's seed: a dispatcher's other state bytes inside its table.
void SeedKind(unsigned k) {
    const bh::Clone& c = g_clones[k];
    if (c.shape == S::kDispatch) OtherStates(c.state_at, c.states);
}

// The simple kinds' rows: the dispatcher by +1 (12), the entrance (its tail
// jmp to Sprite_ScriptTick at +0x4C), the hook.
constexpr bh::CallSite kCallsTick4C[] = {{0x4C, 0x5893A0}};

// ===========================================================================
// Kind 48 (Sample 3) and set-ups 34 and 41
// ===========================================================================

const bh::Clone kK48[] = {
    C("BossSample3_Dispatch", 0x43DEF0, 0x12, nullptr, 0, BH_FN(BossSample3_Dispatch), S::kDispatch, 0xFF, 1, 12),
    C("BossSample3_Enter", 0x43DF10, 0x51, kCallsTick4C, BH_N(kCallsTick4C), BH_FN(BossSample3_Enter), S::kState, 0xFF),
    C("BossSample3_Hook", 0x43DF70, 0x10, nullptr, 0, BH_FN(BossSample3_Hook), S::kEnemyHook, 0xFF),
};
const bh::DataTable kTablesK48[] = {{Key(BossSample3_Hooks), 3, 4, 1}, {Key(BossSample3_States), 12}};

constexpr bh::CallSite kCalls43DFD0[] = {{0x61, 0x446700}, {0xD2, 0x590C90}, {0xE3, 0x590C90}};
constexpr bh::CallSite kCalls43E0C0[] = {{0x1C, 0x446650}, {0x58, 0x589110}, {0x6F, 0x446650}, {0xAB, 0x589110}, {0xC2, 0x446650},
                                         {0xFE, 0x589110}, {0x117, 0x590C90}, {0x128, 0x590C90}, {0x137, 0x446DE0}, {0x143, 0x446E00}};
constexpr bh::CallSite kCalls43E210[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x18, 0x589590}, {0x1F, 0x5891F0}};
const bh::Clone kB34[] = {
    C("Boss34_Setup", 0x43DF80, 0x41, nullptr, 0, BH_FN(Boss34_Setup), S::kSetup),
    C("Boss34_Event", 0x43DFD0, 0xEE, kCalls43DFD0, BH_N(kCalls43DFD0), BH_FN(Boss34_Event), S::kEvent, 0xFF),
    C("Boss34_End", 0x43E0C0, 0x148, kCalls43E0C0, BH_N(kCalls43E0C0), BH_FN(Boss34_End), S::kEnd),
    C("Boss34_Exit", 0x43E210, 0x52, kCalls43E210, BH_N(kCalls43E210), BH_FN(Boss34_Exit), S::kExit),
};

// The picked member: 0..2 in the byte, the dword's upper bytes 0 (the only
// writer stores a byte into zeroed .data) or, now and then, not.
void SeedPicked() {
    const U low = bh::Often() ? bh::Next() % 3 : bh::Next() & 0xFF;
    SetLong(Mem(at::kPicked), static_cast<std::int32_t>(bh::Often() ? low : (bh::Next() & 0xFFFFFF00u) | low));
}

void SeedB34(unsigned k) {
    switch (k) {
    case 0: {
        // the id byte against each member's +0x148: equal half the time, near otherwise
        const auto id = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 5, 6, 0x80, 0xFF) : bh::Next());
        B(at::kPickId) = id;
        for (unsigned m = 0; m < 3; ++m)
            B(at::kPartyTag + m * at::kPartyStride) = static_cast<unsigned char>(bh::Half() ? id : id ^ (1u << (bh::Next() % 8)));
        if (bh::Half()) B(at::kPicked) = static_cast<unsigned char>(BH_PICK(0, 1, 2, 7, 0xFF));
        break;
    }
    case 1: {
        // code 5's tests (round-flag bit 15, script bit 0, the member's +0x91
        // bit 0x20) and code 1's (script bits 0 and 1, the actor): each passed
        // or failed on its own, aimed at the code the round is called with
        g_code = Code(bh::Half() ? 1 : 5);
        SeedPicked();
        const unsigned char picked = B(at::kPicked);
        B(at::kFlags + 1) = static_cast<unsigned char>(bh::Often() ? B(at::kFlags + 1) & 0x7F : B(at::kFlags + 1) | 0x80);
        const U bits = g_code == 1 ? (bh::Often() ? 1 : bh::Next() & 3) : (bh::Often() ? 0 : bh::Next() & 3);
        B(at::kScript) = static_cast<unsigned char>((bh::Next() & 0xFC) | bits);
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char& f = B(at::kPartyFlags91 + m * at::kPartyStride);
            f = static_cast<unsigned char>(bh::Half() ? f | 0x20 : f & ~0x20u);
        }
        B(at::kActor) = static_cast<unsigned char>(bh::Often() ? picked : BH_PICK(0, 1, 2, 3, 4));
        // the action record: one of the harness's records
        bh::SetPointer(at::kAction, bh::SpriteRecord(bh::Next()));
        break;
    }
    case 2:
        B(at::kBattleEnd) = EndByte();
        SeedPicked();
        B(at::kScript) = static_cast<unsigned char>(bh::Half() ? bh::Next() | 2 : bh::Next() & ~2u);
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(m));
            p[0] = static_cast<unsigned char>(bh::Often() ? p[0] | 1 : p[0] & ~1u);
            p[0x91] = static_cast<unsigned char>(bh::Half() ? p[0x91] | 0x40 : p[0x91] & ~0x40u);
            if (bh::Half()) p[8] = static_cast<unsigned char>(BH_PICK(0, 1, 0xE3, 0xE4, 0xFC, 0xFB, 0x7F));
        }
        break;
    default: break;
    }
}

const bh::Clone kB41[] = {
    C("Boss41_Setup", 0x43E270, 0x1F, nullptr, 0, BH_FN(Boss41_Setup), S::kSetup),
};

// ===========================================================================
// Kinds 41 (Gaist), 42 (Torch), 54 (Sample 9) and set-ups 35 and 47
// ===========================================================================

const bh::Clone kK41[] = {
    C("BossGaist_Dispatch", 0x43E540, 0x12, nullptr, 0, BH_FN(BossGaist_Dispatch), S::kDispatch, 0xFF, 1, 12),
    C("BossGaist_Enter", 0x43E560, 0x51, kCallsTick4C, BH_N(kCallsTick4C), BH_FN(BossGaist_Enter), S::kState, 0xFF),
    C("BossGaist_Hook", 0x43E5C0, 0x10, nullptr, 0, BH_FN(BossGaist_Hook), S::kEnemyHook, 0xFF),
    C("BossGaist_HookClearBit1", 0x43E5D0, 0x24, nullptr, 0, BH_FN(BossGaist_HookClearBit1), S::kEnemyHook),
};
const bh::DataTable kTablesK41[] = {{Key(BossGaist_Hooks), 3, 4, 1}, {Key(BossGaist_States), 12}};
void SeedK41(unsigned k) {
    SeedKind(k);
    if (k == 3) {
        B(at::kFlags) = static_cast<unsigned char>(bh::Half() ? bh::Next() | 2 : bh::Next() & ~2u);
        B(at::kFlags + 1) = static_cast<unsigned char>(bh::Next());
        SetWord(Mem(0x66C810), bh::Often() ? BH_PICK(0, 0, 1, 0x100, 0x8000) : bh::Next());
    }
}

const bh::Clone kK42[] = {
    C("BossTorch_Dispatch", 0x43E600, 0x12, nullptr, 0, BH_FN(BossTorch_Dispatch), S::kDispatch, 0xFF, 1, 12),
    C("BossTorch_Enter", 0x43E620, 0x51, kCallsTick4C, BH_N(kCallsTick4C), BH_FN(BossTorch_Enter), S::kState, 0xFF),
    C("BossTorch_Hook", 0x43E680, 0x10, nullptr, 0, BH_FN(BossTorch_Hook), S::kEnemyHook, 0xFF),
    C("BossTorch_HookTarget3", 0x43E690, 0x8, nullptr, 0, BH_FN(BossTorch_HookTarget3), S::kEnemyHook),
};
const bh::DataTable kTablesK42[] = {{Key(BossTorch_Hooks), 3, 4, 1}, {Key(BossTorch_States), 12}};

const bh::Clone kK54[] = {
    C("BossSample9_Dispatch", 0x43E6A0, 0x12, nullptr, 0, BH_FN(BossSample9_Dispatch), S::kDispatch, 0xFF, 1, 12),
    C("BossSample9_Enter", 0x43E6C0, 0x51, kCallsTick4C, BH_N(kCallsTick4C), BH_FN(BossSample9_Enter), S::kState, 0xFF),
    C("BossSample9_Hook", 0x43E720, 0x10, nullptr, 0, BH_FN(BossSample9_Hook), S::kEnemyHook, 0xFF),
};
const bh::DataTable kTablesK54[] = {{Key(BossSample9_Hooks), 3, 4, 1}, {Key(BossSample9_States), 12}};

constexpr bh::CallSite kCalls43E750[] = {{0xA, 0x4456C0}};
constexpr bh::CallSite kCalls43E770[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
const bh::Clone kB35[] = {
    C("Boss35_Setup", 0x43E730, 0x1F, nullptr, 0, BH_FN(Boss35_Setup), S::kSetup),
    C("Boss35_Event", 0x43E750, 0x20, kCalls43E750, BH_N(kCalls43E750), BH_FN(Boss35_Event), S::kEvent, 0xFF),
    C("Boss35_End", 0x43E770, 0x1A, kCalls43E770, BH_N(kCalls43E770), BH_FN(Boss35_End), S::kEnd),
};
void SeedEnd(unsigned k) {
    const bh::Clone& c = g_clones[k];
    if (c.shape == S::kEnd || c.shape == S::kEvent) B(at::kBattleEnd) = EndByte();
    if (c.shape == S::kEvent) g_code = Code(0);
}

const bh::Clone kB47[] = {
    C("Boss47_Setup", 0x43E7A0, 0x1F, nullptr, 0, BH_FN(Boss47_Setup), S::kSetup),
};

// ===========================================================================
// Kinds 43 (Angler) and 50 (Sample 5), set-ups 36 and 43, the effect task
// ===========================================================================

constexpr bh::CallSite kCalls43E860[] = {{0x2A, 0x4358D0}, {0x32, 0x436090}};
constexpr bh::CallSite kCalls43E8C0[] = {{0x28, 0x437450}, {0x55, 0x4530D0}, {0x5D, 0x4376F0}, {0x8A, 0x436090}};
constexpr bh::CallSite kCalls43E970[] = {{0x22, 0x436090}};
constexpr bh::CallSite kCalls43E9A0[] = {{0x0, 0x436090}, {0xC, 0x4376A0}};
constexpr bh::CallSite kCalls43E9D0[] = {{0x1C, 0x435180}};
const bh::Clone kK43[] = {
    C("BossAngler_Dispatch", 0x43E7C0, 0x12, nullptr, 0, BH_FN(BossAngler_Dispatch), S::kDispatch, 0xFF, 1, 12),
    C("BossAngler_Enter", 0x43E7E0, 0x51, kCallsTick4C, BH_N(kCallsTick4C), BH_FN(BossAngler_Enter), S::kState, 0xFF),
    C("BossAngler_AdvanceDispatch", 0x43E840, 0x12, nullptr, 0, BH_FN(BossAngler_AdvanceDispatch), S::kDispatch, 0xFF, 2, 2),
    C("BossAngler_AdvanceStart", 0x43E860, 0x5E, kCalls43E860, BH_N(kCalls43E860), BH_FN(BossAngler_AdvanceStart), S::kState),
    C("BossAngler_Advance", 0x43E8C0, 0x8F, kCalls43E8C0, BH_N(kCalls43E8C0), BH_FN(BossAngler_Advance), S::kState, 0xFF),
    C("BossAngler_RetreatDispatch", 0x43E950, 0x12, nullptr, 0, BH_FN(BossAngler_RetreatDispatch), S::kDispatch, 0xFF, 2, 2),
    C("BossAngler_Retreat", 0x43E970, 0x27, kCalls43E970, BH_N(kCalls43E970), BH_FN(BossAngler_Retreat), S::kState, 0xFF),
    C("BossAngler_RetreatEnd", 0x43E9A0, 0x11, kCalls43E9A0, BH_N(kCalls43E9A0), BH_FN(BossAngler_RetreatEnd), S::kState),
    C("BossAngler_Hook", 0x43E9C0, 0x10, nullptr, 0, BH_FN(BossAngler_Hook), S::kEnemyHook, 0xFF),
    C("BossAngler_HookSpawnFx", 0x43E9D0, 0x92, kCalls43E9D0, BH_N(kCalls43E9D0), BH_FN(BossAngler_HookSpawnFx), S::kEnemyHook),
};
enum : unsigned { kAnglerDispatch, kAnglerEnter, kAdvanceDispatch, kAdvanceStart, kAdvance, kRetreatDispatch, kRetreat, kRetreatEnd,
                  kAnglerHook, kSpawnFx };
static_assert(kSpawnFx + 1 == BH_COUNT(kK43), "kind 43's seeding indices");
const bh::DataTable kTablesK43[] = {{Key(BossAngler_Hooks), 3, 4, 1},
                                    {Key(BossAngler_States), 12},
                                    {Key(BossAngler_AdvanceSteps), 2},
                                    {Key(BossAngler_RetreatSteps), 2}};

// The coordinate +0x34, the step +0xC and the goal +0x18: the goal one step
// away half the time (the compare's equal side), one off, or any.
void SeedStep(int sign) {
    unsigned char* const s = Sprite_Current;
    const U at = bh::Often() ? BH_PICK(0, 0x30000, 0xFFFD0000u, 0x7FFFC000u, 0x80000000u) + (bh::Next() & 0x3C000) : bh::Next();
    const U step = bh::Often() ? BH_PICK(0x4000, 0x4000, 0, 1, 0xFFFFC000u) : bh::Next();
    SetLong(s + 0x34, static_cast<std::int32_t>(at));
    SetLong(s + 0xC, static_cast<std::int32_t>(step));
    const U next = sign > 0 ? at + step : at - step;
    const U pick = bh::Next() % 6;
    SetLong(s + 0x18, static_cast<std::int32_t>(pick < 3 ? next : pick == 3 ? next + 1 : pick == 4 ? next - 1 : bh::Next()));
}

void SeedK43(unsigned k) {
    SeedKind(k);
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case kAdvanceStart: {
        unsigned char* const e = bh::Pointer(bh::at::kEnemyCurrent);
        e[0xF0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next());
        break;
    }
    case kAdvance: {
        s[9] = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 1, 1, 0, 2, 0x80, 0x81) : bh::Next());
        SeedStep(1);
        // the sound word 0x939AD8's +0xF8 points at (the harness's record 0)
        if (bh::Half()) SetWord(bh::SpriteRecord(0), bh::Half() ? 0xFFFF : bh::Next());
        break;
    }
    case kRetreat: SeedStep(-1); break;
    case kSpawnFx: {
        // +1 == 7 and +2 == 1 two times in three, one of them off otherwise
        const U pick = bh::Next() % 9;
        s[1] = static_cast<unsigned char>(pick < 6 ? 7 : pick == 6 ? 6 : pick == 7 ? 7 : bh::Next());
        s[2] = static_cast<unsigned char>(pick < 6 ? 1 : pick == 6 ? 1 : pick == 7 ? BH_PICK(0, 2, 0x81) : bh::Next());
        B(bh::at::kActor) = static_cast<unsigned char>(bh::Often() ? 3 + bh::Next() % 8 : bh::Next() % 11);
        break;
    }
    default: break;
    }
}

const bh::Clone kK50[] = {
    C("BossSample5_Dispatch", 0x43EA70, 0x12, nullptr, 0, BH_FN(BossSample5_Dispatch), S::kDispatch, 0xFF, 1, 12),
    C("BossSample5_Enter", 0x43EA90, 0x51, kCallsTick4C, BH_N(kCallsTick4C), BH_FN(BossSample5_Enter), S::kState, 0xFF),
    C("BossSample5_Hook", 0x43EAF0, 0x10, nullptr, 0, BH_FN(BossSample5_Hook), S::kEnemyHook, 0xFF),
};
const bh::DataTable kTablesK50[] = {{Key(BossSample5_Hooks), 3, 4, 1}, {Key(BossSample5_States), 12}};

constexpr bh::CallSite kCalls43EB20[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
const bh::Clone kB36[] = {
    C("Boss36_Setup", 0x43EB00, 0x1F, nullptr, 0, BH_FN(Boss36_Setup), S::kSetup),
    C("Boss36_End", 0x43EB20, 0x1A, kCalls43EB20, BH_N(kCalls43EB20), BH_FN(Boss36_End), S::kEnd),
};
const bh::Clone kB43[] = {
    C("Boss43_Setup", 0x43EB40, 0x1F, nullptr, 0, BH_FN(Boss43_Setup), S::kSetup),
};

constexpr bh::Imm kImms43EBA0[] = {{0xF, 0x43EBD0}, {0x17, 0x43EC10}, {0x22, 0x4AEE90}};
constexpr bh::CallSite kCalls43EBD0[] = {{0x16, 0x5891F0}, {0x1E, 0x5893A0}, {0x23, 0x5890E0}};
constexpr bh::CallSite kCalls43EC10[] = {{0x0, 0x5893A0}, {0x5, 0x5890E0}};
const bh::Clone kF3[] = {
    C("BossAnglerFx_Dispatch", 0x43EB80, 0x12, nullptr, 0, BH_FN(BossAnglerFx_Dispatch), S::kTask, 0xFF),
    C("BossAnglerFx_Run", 0x43EBA0, 0x2E, nullptr, 0, BH_FN(BossAnglerFx_Run), S::kTask, 0xFF, 1, 0, kImms43EBA0, BH_N(kImms43EBA0)),
    C("BossAnglerFx_Start", 0x43EBD0, 0x31, kCalls43EBD0, BH_N(kCalls43EBD0), BH_FN(BossAnglerFx_Start), S::kTask),
    C("BattleFx_ScriptUntilDone", 0x43EC10, 0x1C, kCalls43EC10, BH_N(kCalls43EC10), BH_FN(BattleFx_ScriptUntilDone), S::kTask),
};
// The one-entry table with its word logged: the only entry takes none, so the
// dispatcher's hand-on is seen only there.
const bh::DataTable kTablesF3[] = {{Key(BossAnglerFx_States), 1, 4, 1}};
void SeedF3(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case 0:
        // +1 is 0 in every slot BossAngler_HookSpawnFx starts (one entry: a
        // byte past it crashes both sides); the others 0 or not
        s[1] = 0;
        for (unsigned b = 2; b <= 4; ++b) s[b] = static_cast<unsigned char>(bh::Half() ? 0 : bh::Next());
        break;
    case 1:
        s[2] = static_cast<unsigned char>(bh::Next() % 3);
        s[1] = static_cast<unsigned char>(bh::Next() % 3);
        s[3] = static_cast<unsigned char>(bh::Next() % 3);
        break;
    case 3: B(bh::at::kFlags) = static_cast<unsigned char>(bh::Half() ? bh::Next() | 4 : bh::Next() & ~4u); break;
    default: break;
    }
}

// ===========================================================================
// Kind 44 (Elder)
// ===========================================================================

const bh::Clone kK44[] = {
    C("BossElder_Dispatch", 0x43EC30, 0x12, nullptr, 0, BH_FN(BossElder_Dispatch), S::kDispatch, 0xFF, 1, 12),
    C("BossElder_Enter", 0x43EC50, 0x51, kCallsTick4C, BH_N(kCallsTick4C), BH_FN(BossElder_Enter), S::kState, 0xFF),
    C("BossElder_Hook", 0x43ECB0, 0x10, nullptr, 0, BH_FN(BossElder_Hook), S::kEnemyHook, 0xFF),
};
const bh::DataTable kTablesK44[] = {{Key(BossElder_Hooks), 3, 4, 1}, {Key(BossElder_States), 12}};

}  // namespace

void SelfTest() {
    RunGroup("k48", kK48, BH_COUNT(kK48), kTablesK48, BH_COUNT(kTablesK48), &SeedKind, 41, 48);
    RunGroup("b34", kB34, BH_COUNT(kB34), nullptr, 0, &SeedB34, 34, -1, 8000);
    RunGroup("b41", kB41, BH_COUNT(kB41), nullptr, 0, nullptr, 41, -1, 4000);
    RunGroup("k41", kK41, BH_COUNT(kK41), kTablesK41, BH_COUNT(kTablesK41), &SeedK41, 35, 41);
    RunGroup("k42", kK42, BH_COUNT(kK42), kTablesK42, BH_COUNT(kTablesK42), &SeedKind, 35, 42);
    RunGroup("k54", kK54, BH_COUNT(kK54), kTablesK54, BH_COUNT(kTablesK54), &SeedKind, 47, 54);
    RunGroup("b35", kB35, BH_COUNT(kB35), nullptr, 0, &SeedEnd, 35, -1);
    RunGroup("b47", kB47, BH_COUNT(kB47), nullptr, 0, nullptr, 47, -1, 4000);
    RunGroup("k43", kK43, BH_COUNT(kK43), kTablesK43, BH_COUNT(kTablesK43), &SeedK43, 36, 43, 8000);
    RunGroup("k50", kK50, BH_COUNT(kK50), kTablesK50, BH_COUNT(kTablesK50), &SeedKind, 43, 50);
    RunGroup("b36", kB36, BH_COUNT(kB36), nullptr, 0, &SeedEnd, 36, -1);
    RunGroup("b43", kB43, BH_COUNT(kB43), nullptr, 0, nullptr, 43, -1, 4000);
    RunGroup("f3", kF3, BH_COUNT(kF3), kTablesF3, BH_COUNT(kTablesF3), &SeedF3, 36, -1);
    RunGroup("k44", kK44, BH_COUNT(kK44), kTablesK44, BH_COUNT(kTablesK44), &SeedKind, 38, 44);
}

}  // namespace boss_sh

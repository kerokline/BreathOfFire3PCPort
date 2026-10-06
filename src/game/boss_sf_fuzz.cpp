// BOF3X_SHADOW=boss_sf: group BSF's 54 functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per unit - the
// set-ups 27 and 28, the kinds 33 and 62, and the two effect tasks FB8 and F2 -
// each with its fight id (Group::fight) and, for a kind, its kind
// (Group::kind). docs/boss_sf.md section 3. BOF3X_BSF_RUN=<unit> (B27, FB8,
// K33, B28, F2, K62) runs one.
//
// The clone rows are tools/boss_rows.py's (--unit <U> --clones, 2026-09-28),
// each read against the disassembly; the shapes are the plan's: a kind's
// dispatchers kDispatch with their state byte drawn below their table, its
// states kState (al the answer where they tail-jump to a tick), its +0xF4
// hook kEnemyHook, a set-up kSetup and its hooks kEnd / kExit / kEvent, an
// effect task's functions kTask (Sprite_Current a task slot; their state
// bytes seeded below their tables here, the harness draws only a kDispatch's),
// and Myria's two spawn helpers and state 8's check kCallee. Every .data table
// the dispatchers and hooks go through is a DataTable (its entries recorders
// while the Run lasts).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sf.h"
#include "game/boss_sf_callees.h"
#include "game/move_script_bytes.h"

namespace boss_sf {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BSF_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BSF_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BSF_FN(name) reinterpret_cast<const void*>(&::name)

// A clone row: the tool's, with its shape, answer mask and (for a
// dispatcher) its state byte and table size.
bh::Clone Row(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0,
              std::uint8_t state_at = 1, std::uint8_t states = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    c.state_at = state_at;
    c.states = states;
    return c;
}

unsigned char& B(U address) { return Mem(address)[0]; }
U Above() { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0; }

// ===========================================================================
// What every run shares
// ===========================================================================

// Battle_RemoveFromTurnOrder is louder than the real one: Boss27_End reads the
// member's +0x90 and +8 after it, and the next member's +0 - the effect moves
// them (boss_se_fuzz.cpp's, for the same loop in Boss26_End).
// (bh::TurnOrderEffect: the harness's since round eleven's cleanup folded this group's copy)

// Set-up 27's hooks read the script bits 0x904AAD again after a call
// (Boss27_Event's code 2 after Sprite_EnsureAnimation, Boss27_End after the
// party loop, Boss27_Exit between its spawn-helper calls): these callees'
// stand-ins flip one of its bits, louder than the real ones (which do not
// touch it), so a read taken before the call is refused whenever it is
// reached, not only when the disturbance happens to move the byte.
// BossActor_ClearBit40 keeps the standard stand-in's own effect (a field
// object's bit 0x40 flipped).
std::uint32_t ScriptEffect(const std::uint32_t*, std::uint32_t answer) {
    B(at::kScript) ^= static_cast<unsigned char>(1u << (bh::Noise() % 8));
    return answer;
}
std::uint32_t ClearBitEffect(const std::uint32_t* a, std::uint32_t answer) {
    bh::Object(bh::Noise() % 4)[0] ^= 0x40;
    return ScriptEffect(a, answer);
}

// AreaMap_Elevation answers the ground; the Gazer effect compares its signed
// low word + 0x200 / 0x400 / 0x1000 with the running sprite's +0x3E. A third
// of the time the answer puts that sum at +0x3E, one below or one above (the
// compare's boundary), with garbage in the high word (the code sign-extends
// the low word); else garbage.
std::uint32_t GroundEffect(const std::uint32_t*, std::uint32_t answer) {
    const U n = bh::Noise();
    if (n % 3 != 0) return answer;
    static const std::int32_t kSpans[] = {0x200, 0x400, 0x1000};
    const auto height = static_cast<std::int16_t>(Word(Sprite_Current + 0x3E));
    const std::int32_t ground = height - kSpans[(n >> 4) % 3] + static_cast<std::int32_t>((n >> 8) % 3) - 1;
    return (answer & 0xFFFF0000u) | (static_cast<U>(ground) & 0xFFFFu);
}

// Beyond the standard set: the four louder stand-ins above; Port_DroppedCall with no words (it sits in both
// kinds' +1 tables; reached through a dispatcher's jmp it "reads" the stack
// word of the dispatcher's caller - boss_sa.md section 3); Sprite_PoseFromSet
// and Battle_RemoveFromTurnOrder as set-up 26's loop has them (the pose a
// register whose upper bytes are a callee's leftovers: the low byte read);
// Crt_sprintf with the three words FB8 pushes (the standard four would log
// the original's stack above them); AreaMap_Elevation with its boundary
// effect; Battle_LoadSoundByKey reading its two bytes (the original pushes
// registers with leftovers above them); the engine functions nobody owns;
// Myria's two spawn helpers and state 8's check, the group's own called
// directly.
const bh::Callee kCallees[] = {
    {"Port_DroppedCall", ::bof3::addr::Port_DroppedCall, KeyOf(&::Port_DroppedCall), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"Sprite_PoseFromSet", ::bof3::addr::Sprite_PoseFromSet, KeyOf(&::Sprite_PoseFromSet), 3, {kU8, kAll, kAll}, bh::Answer::kGarbage, 0, 0, {},
     &ScriptEffect},
    {"Sprite_EnsureAnimation", ::bof3::addr::Sprite_EnsureAnimation, KeyOf(&::Sprite_EnsureAnimation), 1, {kU8}, bh::Answer::kFlag, 0, 0, {},
     &ScriptEffect},
    {"BossActor_CopyFrom", ::bof3::addr::BossActor_CopyFrom, KeyOf(&::BossActor_CopyFrom), 3, {kU8, kAll, kU8}, bh::Answer::kGarbage, 0, 0, {},
     &ScriptEffect},
    {"BossActor_ClearBit40", ::bof3::addr::BossActor_ClearBit40, KeyOf(&::BossActor_ClearBit40), 1, {kU8}, bh::Answer::kGarbage, 0, 0, {},
     &ClearBitEffect},
    {"Battle_RemoveFromTurnOrder", ::bof3::addr::Battle_RemoveFromTurnOrder, KeyOf(&::Battle_RemoveFromTurnOrder), 1, {kU8},
     bh::Answer::kGarbage, 0, 0, {}, &bh::TurnOrderEffect},
    {"Crt_sprintf", 0x5B9380, KeyOf(&::Crt_sprintf), 3, {kAll, kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"AreaMap_Elevation", ::bof3::addr::AreaMap_Elevation, KeyOf(&::AreaMap_Elevation), 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0, {},
     &GroundEffect},
    {"Battle_LoadSoundByKey", ::bof3::addr::Battle_LoadSoundByKey, KeyOf(&::Battle_LoadSoundByKey), 2, {kU8, kU8}, bh::Answer::kFlag, 0, 0},
    {"0x437450", at::kEnemySound, at::kEnemySound, 1, {kU16}, bh::Answer::kGarbage, 0, 0},
    {"0x4376A0", at::kEnemyActEnd, at::kEnemyActEnd, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x4376F0", at::kEnemyActChance, at::kEnemyActChance, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x454A80", at::kSlotsReleaseFor, at::kSlotsReleaseFor, 1, {kAll}, bh::Answer::kGarbage, 0, 0},
    {"0x455290", at::kSlotStart, at::kSlotStart, 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"BossMyria_SpawnFx", 0x440660, KeyOf(&::BossMyria_SpawnFx), 2, {kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"BossMyria_SpawnFxAndWait", 0x440630, KeyOf(&::BossMyria_SpawnFxAndWait), 3, {kU8, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"BossMyria_State8Check", 0x4405E0, KeyOf(&::BossMyria_State8Check), 0, {}, bh::Answer::kPhase, 0, 0},
};

// The cells beyond the harness's battle frame the 54 read or write.
const bh::Region kRegions[] = {
    {0x803430, 4},               // the window record byte 0x803433 the count waits on
    {at::kScriptVar3, 4},        // movement-script variable 3 (set-up 28's end)
};

// A dispatcher's other state bytes inside its table, so a dispatcher reading
// the wrong byte lands on another entry (a count) rather than past its table
// (a Fatal); never the byte the harness drew.
using bh::OtherStates;   // the harness's since round eleven's cleanup folded this group's copy (the same draws)

// A down-counter's byte: its ends, 1 (the step to 0) and any.
unsigned char Counter() { return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 1, 2, 6, 0x1E, 0xFF, 0x80) : bh::Next()); }

// An ability id: the three Myria's state 7 switches on, then ids whose record
// bytes have bit 2 (0x65C4D8) or bit 3 (0x65C4DD) set or clear as a coin
// says (the records are read in place), then any.
unsigned AbilityId() {
    const U pick = bh::Next() % 8;
    if (pick < 3) return pick == 0 ? 0x3A : pick == 1 ? 0x81 : 0x82;
    if (pick < 7) {
        const U base = pick & 1 ? at::kAbilityFlags8 : at::kAbilityFlags4;
        const unsigned char bit = pick & 1 ? 8 : 4;
        const bool want = bh::Half();
        for (unsigned tries = 0; tries < 64; ++tries) {
            const unsigned id = bh::Next() % 0x140;
            if (((Mem(base + id * at::kAbilityStride)[0] & bit) != 0) == want) return id;
        }
    }
    return bh::Half() ? bh::Next() % 0x140 : bh::Next() & 0xFFFF;
}

// What the 54 read again after a call that the standard disturbance does not
// move: the script bits 0x904AAD (Boss27_Event after Sprite_EnsureAnimation,
// Boss27_End after the loop, Boss27_Exit between its calls), a member's +0x91
// / +8 / +0 (Boss27_End), the ability 0x904B80 and the acting kind 0x904B35
// (Myria's state 7 after File_LoadDone and the sound load), Myria's wait word
// 0x904B7E, the window byte and 0x904AE9 (the count), the current enemy's
// +0x105 and the enemy data byte its +0xF0 names.
void Disturb(U h) {
    const U v = h >> 16;
    switch ((h >> 8) % 9) {
    case 0: B(at::kScript) = static_cast<unsigned char>(v); break;
    case 1: bh::PartyOf(static_cast<unsigned char>(v % 3))[0x91] ^= 0x40; break;
    case 2: SetWord(Mem(at::kAbility), v & 1 ? v >> 1 : (v >> 1) % 0x140); break;
    case 3: B(at::kActKind) = static_cast<unsigned char>(v & 1 ? 4 : v >> 1); break;
    case 4: SetWord(Mem(at::kMyriaWait), v & 1 ? (v >> 1) % 9 : v >> 1); break;
    case 5: B(at::kWindowUp) = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 6: B(at::kBanner) ^= 2; break;
    case 7: {
        unsigned char* const e = bh::Pointer(bh::at::kEnemyCurrent);
        e[0x105] = static_cast<unsigned char>(v & 1 ? 4 : v >> 1);
        break;
    }
    default: Mem(at::kEnemyData + (v % 8) * at::kEnemyDataStride)[0x8A + (v >> 8) % 2] = static_cast<unsigned char>(v >> 4); break;
    }
}

// Which unit the shadow name's run is limited to (BOF3X_BSF_RUN).
bool Wants(const char* unit) {
    const char* const only = std::getenv("BOF3X_BSF_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, unit) == 0;
}

// The clones of the run in progress, for Args.
const bh::Clone* g_clones = nullptr;

// An enemy hook's word 0..2 (the harness's draw) with garbage above the byte
// half the time - the hooks mask it, their entries get it whole; set-up 27's
// event code, 0 and 2 (the two it reads) at twice the others' rate; Myria's
// spawn helpers' words, their low bytes small or any with garbage above.
void Args(unsigned k, U* a) {
    const bh::Clone& c = g_clones[k];
    if (c.shape == S::kEnemyHook) {
        a[0] = Above() | (a[0] & 0xFF);
    } else if (c.base == 0x43C4A0) {
        a[0] = Above() | BH_PICK(0, 0, 0, 0, 0, 0, 2, 2, 2, 1, 3, 4, 5, 6, 0x80, 0xFF, 0x100);
    } else if (c.shape == S::kCallee) {
        for (unsigned i = 0; i < 3; ++i) a[i] = Above() | (bh::Half() ? bh::Next() % 8 : bh::Next() & 0xFF);
    }
}

void RunGroup(const char* unit, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables, void (*seed)(unsigned),
              int fight, int kind, unsigned rounds = 6000) {
    if (!Wants(unit)) return;
    g_clones = clones;
    bh::Group g{"boss_sf", clones, n, kCallees, BSF_COUNT(kCallees), tables, n_tables, kRegions, BSF_COUNT(kRegions), seed, &Disturb, rounds};
    g.args = &Args;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
    g_clones = nullptr;
}

// ===========================================================================
// Set-up 27
// ===========================================================================

constexpr bh::CallSite kCalls43C4A0[] = {{0x44, 0x589330}};
constexpr bh::CallSite kCalls43C5C0[] = {{0xF, 0x446650}, {0x4B, 0x589110}, {0x62, 0x446650}, {0x9E, 0x589110},
                                         {0xB5, 0x446650}, {0xF1, 0x589110}, {0x11F, 0x446E20}};
constexpr bh::CallSite kCalls43C6F0[] = {{0x12, 0x4949F0}, {0x1C, 0x4949D0}, {0x36, 0x4949F0}, {0x40, 0x4949D0}};
const bh::Clone kClonesB27[] = {
    Row("Boss27_Setup", 0x43C480, 0x1F, nullptr, 0, BSF_FN(Boss27_Setup), S::kSetup),
    Row("Boss27_Event", 0x43C4A0, 0x119, kCalls43C4A0, BSF_N(kCalls43C4A0), BSF_FN(Boss27_Event), S::kEvent, 0xFF),
    Row("Boss27_End", 0x43C5C0, 0x124, kCalls43C5C0, BSF_N(kCalls43C5C0), BSF_FN(Boss27_End), S::kEnd),
    Row("Boss27_Exit", 0x43C6F0, 0x47, kCalls43C6F0, BSF_N(kCalls43C6F0), BSF_FN(Boss27_Exit), S::kExit),
};

// The battle-end byte 0 half the time (the event hook's deep path needs it),
// else a single bit (bit 0 the one the deep path's test must not ignore, bit
// 2 the one code 2 tests) or others; the script bits with bit 3 clear half the
// time (the deep path's second test); the two enemies' +0x92 bit 0x4000
// (their +0x93 bit 6) each clear five times in six; the turn order's cursor
// small and the actor before it a member two times in three, else an enemy or
// none; the members' +0x130 low bits clear half the time.
void SeedB27(unsigned) {
    B(at::kBattleEnd) = static_cast<unsigned char>(bh::Half() ? 0 : bh::Often() ? BH_PICK(1, 1, 4, 2, 5, 0x80, 0xFB, 0xFF) : bh::Next());
    if (bh::Often()) B(at::kScript) = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 0x10, 0x20, 0x28, 0x18, 8, 1, 2, 4, 7) : bh::Next());
    if (bh::Half()) B(at::kScript) &= 0xF7;
    B(at::kEnemy0Status + 1) = static_cast<unsigned char>(bh::Next() % 6 ? B(at::kEnemy0Status + 1) & 0xBF : B(at::kEnemy0Status + 1) | 0x40);
    B(at::kEnemy1Status + 1) = static_cast<unsigned char>(bh::Next() % 6 ? B(at::kEnemy1Status + 1) & 0xBF : B(at::kEnemy1Status + 1) | 0x40);
    const unsigned cursor = bh::Often() ? bh::Next() % 12 : bh::Next() & 0xFF;
    B(at::kCursor) = static_cast<unsigned char>(cursor);
    if (cursor < 0xD5)
        B(at::kOrderBefore + cursor) =
            static_cast<unsigned char>(bh::Often() ? bh::Next() % 3 : bh::Often() ? BH_PICK(3, 4, 0xFF, 0x80) : bh::Next());
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(m));
        if (bh::Half()) p[0x130] = static_cast<unsigned char>(p[0x130] & 0xFC);
    }
}

// ===========================================================================
// FB8: fight 26's count task
// ===========================================================================

constexpr bh::CallSite kCalls43C780[] = {{0x4, 0x443870}, {0x17, 0x516B30}, {0x34, 0x5B9380}, {0x47, 0x516F60}, {0x5E, 0x516B30}, {0x89, 0x4351F0}};
const bh::Clone kClonesFB8[] = {
    Row("Boss26Fx_Dispatch", 0x43C740, 0x12, nullptr, 0, BSF_FN(Boss26Fx_Dispatch), S::kTask),
    Row("Boss26Fx_Wait", 0x43C760, 0x1B, nullptr, 0, BSF_FN(Boss26Fx_Wait), S::kTask),
    Row("Boss26Fx_DrawCount", 0x43C780, 0x8F, kCalls43C780, BSF_N(kCalls43C780), BSF_FN(Boss26Fx_DrawCount), S::kTask),
};
const bh::DataTable kTablesFB8[] = {{0x64D3E4, 2}};

// The dispatcher's +1 inside its two (the harness draws only a kDispatch's);
// the window byte and 0x904AE9 bit 1 clear half the time each; phase 5 a
// third; the turn counter from 0 past 0x15, or any.
void SeedFB8(unsigned k) {
    unsigned char* const s = Sprite_Current;
    s[1] = static_cast<unsigned char>(k == 0 ? bh::Next() % 2 : bh::Often() ? bh::Next() % 3 : bh::Next());
    B(at::kWindowUp) = static_cast<unsigned char>(bh::Half() ? 0 : bh::Next());
    B(at::kBanner) = static_cast<unsigned char>(bh::Half() ? B(at::kBanner) & 0xFD : B(at::kBanner) | 2);
    if (bh::Often()) B(at::kPhase) = static_cast<unsigned char>(bh::Half() ? 5 : BH_PICK(1, 2, 3, 4, 6));
    SetLong(Mem(at::kTurn), static_cast<std::int32_t>(bh::Often() ? bh::Next() % 0x18 : bh::Next()));
}

// ===========================================================================
// Kind 33 (Gazer) and set-up 28
// ===========================================================================

constexpr bh::CallSite kCallsTick38[] = {{0x38, 0x5893A0}};
constexpr bh::CallSite kCalls43C890[] = {{0x2A, 0x4358D0}};
constexpr bh::CallSite kCalls43C8D0[] = {{0x2A, 0x437450}, {0x32, 0x436090}, {0x3F, 0x435180}};
constexpr bh::CallSite kCalls43C970[] = {{0x2, 0x5891F0}, {0x13, 0x4530D0}};
constexpr bh::CallSite kCalls43C9A0[] = {{0x0, 0x436090}, {0x10, 0x4376F0}, {0x15, 0x4376A0}};
const bh::Clone kClonesK33[] = {
    Row("BossGazer_Dispatch", 0x43C810, 0x12, nullptr, 0, BSF_FN(BossGazer_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossGazer_Enter", 0x43C830, 0x3D, kCallsTick38, BSF_N(kCallsTick38), BSF_FN(BossGazer_Enter), S::kState, 0xFF),
    Row("BossGazer_State4Dispatch", 0x43C870, 0x12, nullptr, 0, BSF_FN(BossGazer_State4Dispatch), S::kDispatch, 0, 2, 3),
    Row("BossGazer_State4Start", 0x43C890, 0x3B, kCalls43C890, BSF_N(kCalls43C890), BSF_FN(BossGazer_State4Start), S::kState),
    Row("BossGazer_State4Wait", 0x43C8D0, 0x77, kCalls43C8D0, BSF_N(kCalls43C8D0), BSF_FN(BossGazer_State4Wait), S::kState),
    Row("BossGazer_State5Dispatch", 0x43C950, 0x12, nullptr, 0, BSF_FN(BossGazer_State5Dispatch), S::kDispatch, 0, 2, 2),
    Row("BossGazer_State5Start", 0x43C970, 0x24, kCalls43C970, BSF_N(kCalls43C970), BSF_FN(BossGazer_State5Start), S::kState),
    Row("BossGazer_State5Close", 0x43C9A0, 0x1B, kCalls43C9A0, BSF_N(kCalls43C9A0), BSF_FN(BossGazer_State5Close), S::kState),
    Row("BossGazer_Hook", 0x43C9C0, 0x10, nullptr, 0, BSF_FN(BossGazer_Hook), S::kEnemyHook),
};
// The hook table first: BareRet is in it and in State4Steps, and the first
// registration's word count stands (its entries log the forwarded word).
const bh::DataTable kTablesK33[] = {{0x64D448, 3, 4, 1}, {0x64D404, 12}, {0x64D434, 3}, {0x64D440, 2}};

// The enemy's +0xF0 (the enemy data record) 0..7 two times in three; +9 at
// the count's ends; a dispatcher's other state bytes inside its table.
void SeedKindCommon(const bh::Clone& c) {
    if (c.shape == S::kDispatch) OtherStates(c.state_at, c.states);
    unsigned char* const e = bh::Pointer(bh::at::kEnemyCurrent);
    if (bh::Often()) e[0xF0] = static_cast<unsigned char>(bh::Next() % 8);
    Sprite_Current[9] = Counter();
}
void SeedK33(unsigned k) { SeedKindCommon(kClonesK33[k]); }

constexpr bh::CallSite kCalls43CA00[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
const bh::Clone kClonesB28[] = {
    Row("Boss28_Setup", 0x43C9D0, 0x1F, nullptr, 0, BSF_FN(Boss28_Setup), S::kSetup),
    Row("Boss28_End", 0x43CA00, 0x1A, kCalls43CA00, BSF_N(kCalls43CA00), BSF_FN(Boss28_End), S::kEnd),
};
void SeedB28(unsigned) {
    B(at::kBattleEnd) = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 3, 0xFD, 0x82) : bh::Next());
}

// ===========================================================================
// F2: the Gazer's effect task
// ===========================================================================

constexpr bh::CallSite kCalls43CA40[] = {{0x1D, 0x588F20}};
constexpr bh::CallSite kCalls43CA70[] = {{0x2C, 0x589590}, {0x33, 0x5891F0}, {0x9B, 0x435180}};
constexpr bh::CallSite kCalls43CB50[] = {{0x2A, 0x5720C0}, {0x50, 0x5720C0}, {0x82, 0x587900}};
constexpr bh::CallSite kCalls43CC00[] = {{0x2A, 0x5720C0}, {0x50, 0x5720C0}};
constexpr bh::CallSite kCalls43CC90[] = {{0x2A, 0x5720C0}, {0x5B, 0x4351F0}};
constexpr bh::CallSite kCalls43CD00[] = {{0x1D, 0x588F20}};
constexpr bh::CallSite kCalls43CD30[] = {{0x2C, 0x589590}, {0x33, 0x5891F0}, {0x67, 0x5720C0}};
constexpr bh::CallSite kCalls43CDC0[] = {{0x12, 0x5893A0}};
const bh::Clone kClonesF2[] = {
    Row("BossGazerFx_Dispatch", 0x43CA20, 0x12, nullptr, 0, BSF_FN(BossGazerFx_Dispatch), S::kTask),
    Row("BossGazerFx_BounceDispatch", 0x43CA40, 0x23, kCalls43CA40, BSF_N(kCalls43CA40), BSF_FN(BossGazerFx_BounceDispatch), S::kTask),
    Row("BossGazerFx_BounceStart", 0x43CA70, 0xD4, kCalls43CA70, BSF_N(kCalls43CA70), BSF_FN(BossGazerFx_BounceStart), S::kTask),
    Row("BossGazerFx_Bounce", 0x43CB50, 0xAA, kCalls43CB50, BSF_N(kCalls43CB50), BSF_FN(BossGazerFx_Bounce), S::kTask),
    Row("BossGazerFx_BounceBack", 0x43CC00, 0x89, kCalls43CC00, BSF_N(kCalls43CC00), BSF_FN(BossGazerFx_BounceBack), S::kTask),
    Row("BossGazerFx_Leave", 0x43CC90, 0x61, kCalls43CC90, BSF_N(kCalls43CC90), BSF_FN(BossGazerFx_Leave), S::kTask),
    Row("BossGazerFx_MarkDispatch", 0x43CD00, 0x23, kCalls43CD00, BSF_N(kCalls43CD00), BSF_FN(BossGazerFx_MarkDispatch), S::kTask),
    Row("BossGazerFx_MarkStart", 0x43CD30, 0x82, kCalls43CD30, BSF_N(kCalls43CD30), BSF_FN(BossGazerFx_MarkStart), S::kTask),
    Row("BossGazerFx_MarkWait", 0x43CDC0, 0x17, kCalls43CDC0, BSF_N(kCalls43CDC0), BSF_FN(BossGazerFx_MarkWait), S::kTask, 0xFF),
};
enum : unsigned { kFxDispatch, kFxBounceDispatch, kFxBounceStart, kFxBounce, kFxBounceBack, kFxLeave, kFxMarkDispatch, kFxMarkStart, kFxMarkWait };
static_assert(kFxMarkWait + 1 == BSF_COUNT(kClonesF2), "F2's seeding indices");
const bh::DataTable kTablesF2[] = {{0x64D454, 2}, {0x64D45C, 4}, {0x64D46C, 3}};

// The three dispatchers' bytes inside their tables (and the others inside
// the smallest, 2); the bounce count +9 at its ends; the height +0x3E and the
// velocity words at signed boundaries; the owner's +0 bit 0 either way.
void SeedF2(unsigned k) {
    unsigned char* const s = Sprite_Current;
    for (unsigned b = 1; b <= 4; ++b) s[b] = static_cast<unsigned char>(bh::Next() % 2);
    if (k == kFxBounceDispatch) s[2] = static_cast<unsigned char>(bh::Next() % 4);
    if (k == kFxMarkDispatch) s[2] = static_cast<unsigned char>(bh::Next() % 3);
    s[9] = Counter();
    if (bh::Half()) SetWord(s + 0x3E, BH_PICK(0, 0x7FFF, 0x8000, 0xFFFF, 0x200, 0x1000, 0xF000));
    if (bh::Half()) SetLong(s + 0x14, static_cast<std::int32_t>(BH_PICK(0, 0x20, 0xFFFFFFE0u, 0x7FFFFFFFu, 0x80000000u, 0x10000)));
    if (bh::Half()) SetLong(s + 0x20, static_cast<std::int32_t>(BH_PICK(0x40, 0xFFFFFFC0u, 0xFFFFFFE0u, 0, 0x7FFFFFFFu)));
    unsigned char* const owner = bh::Pointer(at::kOwner);
    owner[0] = static_cast<unsigned char>(bh::Half() ? owner[0] | 1 : owner[0] & 0xFE);
}

// ===========================================================================
// Kind 62 (Myria)
// ===========================================================================

constexpr bh::CallSite kCalls4400A0[] = {{0x57, 0x4358D0}, {0x60, 0x440660}, {0x69, 0x440660}, {0x72, 0x440660}, {0x83, 0x455290}, {0x95, 0x5893A0}};
constexpr bh::CallSite kCalls440160[] = {{0x14, 0x4358D0}, {0x1C, 0x436090}};
constexpr bh::CallSite kCalls440190[] = {{0x9, 0x436090}};
constexpr bh::CallSite kCalls4401E0[] = {{0x6, 0x440630}};
constexpr bh::CallSite kCalls440220[] = {{0x28, 0x437450}};
constexpr bh::CallSite kCalls440280[] = {{0x6, 0x4530D0}};
constexpr bh::CallSite kCalls4402C0[] = {{0x28, 0x440630}};
constexpr bh::CallSite kCalls440310[] = {{0x0, 0x589410}, {0x5, 0x437470}, {0x10, 0x454A80}};
constexpr bh::CallSite kCalls440390[] = {{0x0, 0x454810}, {0x36, 0x446E40}, {0xC6, 0x440630}, {0xDD, 0x440630}, {0xE4, 0x4358D0}, {0xFE, 0x4360C0}};
constexpr bh::CallSite kCalls4404A0[] = {{0x2D, 0x587740}, {0x45, 0x4360C0}};
constexpr bh::CallSite kCallsTickOnce0[] = {{0x0, 0x4360C0}};
constexpr bh::CallSite kCalls440510[] = {{0x23, 0x4405E0}};
constexpr bh::CallSite kCalls440590[] = {{0x17, 0x4360C0}};
constexpr bh::CallSite kCalls4405C0[] = {{0x0, 0x4376A0}};
constexpr bh::CallSite kCalls4405E0[] = {{0x31, 0x4376A0}};
constexpr bh::CallSite kCalls440630[] = {{0xA, 0x440660}};
constexpr bh::CallSite kCalls440660[] = {{0x7, 0x435180}};
const bh::Clone kClonesK62[] = {
    Row("BossMyria_Dispatch", 0x440080, 0x12, nullptr, 0, BSF_FN(BossMyria_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossMyria_Enter", 0x4400A0, 0x9A, kCalls4400A0, BSF_N(kCalls4400A0), BSF_FN(BossMyria_Enter), S::kState, 0xFF),
    Row("BossMyria_IdleDispatch", 0x440140, 0x12, nullptr, 0, BSF_FN(BossMyria_IdleDispatch), S::kDispatch, 0, 2, 2),
    Row("BossMyria_IdleStart", 0x440160, 0x2A, kCalls440160, BSF_N(kCalls440160), BSF_FN(BossMyria_IdleStart), S::kState),
    Row("BossMyria_IdleEnd", 0x440190, 0x26, kCalls440190, BSF_N(kCalls440190), BSF_FN(BossMyria_IdleEnd), S::kState),
    Row("BossMyria_State4Dispatch", 0x4401C0, 0x12, nullptr, 0, BSF_FN(BossMyria_State4Dispatch), S::kDispatch, 0, 2, 2),
    Row("BossMyria_State4Start", 0x4401E0, 0x3F, kCalls4401E0, BSF_N(kCalls4401E0), BSF_FN(BossMyria_State4Start), S::kState),
    Row("BossMyria_State4Wait", 0x440220, 0x3A, kCalls440220, BSF_N(kCalls440220), BSF_FN(BossMyria_State4Wait), S::kState),
    Row("BossMyria_State5Dispatch", 0x440260, 0x12, nullptr, 0, BSF_FN(BossMyria_State5Dispatch), S::kDispatch, 0, 2, 2),
    Row("BossMyria_State5Start", 0x440280, 0x17, kCalls440280, BSF_N(kCalls440280), BSF_FN(BossMyria_State5Start), S::kState),
    Row("BossMyria_ActDispatch", 0x4402A0, 0x12, nullptr, 0, BSF_FN(BossMyria_ActDispatch), S::kDispatch, 0, 2, 6),
    Row("BossMyria_ActPick", 0x4402C0, 0x46, kCalls4402C0, BSF_N(kCalls4402C0), BSF_FN(BossMyria_ActPick), S::kState),
    Row("BossMyria_Death", 0x440310, 0x57, kCalls440310, BSF_N(kCalls440310), BSF_FN(BossMyria_Death), S::kState),
    Row("BossMyria_State7Dispatch", 0x440370, 0x12, nullptr, 0, BSF_FN(BossMyria_State7Dispatch), S::kDispatch, 0, 2, 3),
    Row("BossMyria_State7Start", 0x440390, 0x104, kCalls440390, BSF_N(kCalls440390), BSF_FN(BossMyria_State7Start), S::kState, 0xFF),
    Row("BossMyria_State7Count", 0x4404A0, 0x4A, kCalls4404A0, BSF_N(kCalls4404A0), BSF_FN(BossMyria_State7Count), S::kState, 0xFF),
    Row("BossMyria_State7End", 0x4404F0, 0x20, kCallsTickOnce0, BSF_N(kCallsTickOnce0), BSF_FN(BossMyria_State7End), S::kState),
    Row("BossMyria_State8Dispatch", 0x440510, 0x29, kCalls440510, BSF_N(kCalls440510), BSF_FN(BossMyria_State8Dispatch), S::kDispatch, 0, 2, 5),
    Row("BossMyria_State8Tick", 0x440540, 0xE, kCallsTickOnce0, BSF_N(kCallsTickOnce0), BSF_FN(BossMyria_State8Tick), S::kState),
    Row("BossMyria_State8Cost", 0x440550, 0x3E, kCallsTickOnce0, BSF_N(kCallsTickOnce0), BSF_FN(BossMyria_State8Cost), S::kState),
    Row("BossMyria_State8TickUnless", 0x440590, 0x1D, kCalls440590, BSF_N(kCalls440590), BSF_FN(BossMyria_State8TickUnless), S::kState, 0xFF),
    Row("BossMyria_State8Clear", 0x4405B0, 0xA, nullptr, 0, BSF_FN(BossMyria_State8Clear), S::kState),
    Row("BossMyria_State8Close", 0x4405C0, 0x19, kCalls4405C0, BSF_N(kCalls4405C0), BSF_FN(BossMyria_State8Close), S::kState),
    Row("BossMyria_State8Check", 0x4405E0, 0x36, kCalls4405E0, BSF_N(kCalls4405E0), BSF_FN(BossMyria_State8Check), S::kState),
    Row("BossMyria_Hook", 0x440620, 0x10, nullptr, 0, BSF_FN(BossMyria_Hook), S::kEnemyHook),
    Row("BossMyria_SpawnFxAndWait", 0x440630, 0x29, kCalls440630, BSF_N(kCalls440630), BSF_FN(BossMyria_SpawnFxAndWait), S::kCallee),
    Row("BossMyria_SpawnFx", 0x440660, 0x77, kCalls440660, BSF_N(kCalls440660), BSF_FN(BossMyria_SpawnFx), S::kCallee),
};
enum : unsigned {
    kMyDispatch, kMyEnter, kMyIdleDispatch, kMyIdleStart, kMyIdleEnd, kMy4Dispatch, kMy4Start, kMy4Wait, kMy5Dispatch, kMy5Start,
    kMyActDispatch, kMyActPick, kMyDeath, kMy7Dispatch, kMy7Start, kMy7Count, kMy7End, kMy8Dispatch, kMy8Tick, kMy8Cost,
    kMy8TickUnless, kMy8Clear, kMy8Close, kMy8Check, kMyHook, kMySpawnWait, kMySpawn
};
static_assert(kMySpawn + 1 == BSF_COUNT(kClonesK62), "kind 62's seeding indices");
const bh::DataTable kTablesK62[] = {{0x64DDC4, 3, 4, 1}, {0x64DD44, 12}, {0x64DD74, 2}, {0x64DD7C, 2}, {0x64DD84, 2},
                                    {0x64DD8C, 6},       {0x64DDA4, 3},  {0x64DDB0, 5}};

// Myria's cells: the ability id (the three state 7 switches on, and ids with
// each record bit set and clear), the acting kind 4 half the time, the wait
// word at 3, 5, 8 and others, the round flags' bit 2, the enemy's +0x105 at 4
// half the time, the fight byte 0 a sixth of the time (state 7's event-battle
// test), state 8's +1 at 8 two times in three (its dispatcher's tail).
void SeedK62(unsigned k) {
    const bh::Clone& c = kClonesK62[k];
    SeedKindCommon(c);
    SetWord(Mem(at::kAbility), AbilityId());
    B(at::kActKind) = static_cast<unsigned char>(bh::Half() ? 4 : BH_PICK(0, 1, 3, 5, 0x84, 0xFF));
    SetWord(Mem(at::kMyriaWait), bh::Often() ? BH_PICK(0, 3, 5, 8, 4, 6, 7, 0x103) : bh::Next());
    B(at::kRoundFlags) = static_cast<unsigned char>(bh::Half() ? B(at::kRoundFlags) | 4 : B(at::kRoundFlags) & 0xFB);
    unsigned char* const e = bh::Pointer(bh::at::kEnemyCurrent);
    e[0x105] = static_cast<unsigned char>(bh::Half() ? 4 : BH_PICK(0, 3, 5, 0x84, 0xFF));
    if (bh::Next() % 6 == 0) B(at::kFight) = 0;
    if (k == kMy8Dispatch && bh::Often()) Sprite_Current[1] = 8;
}

}  // namespace

void SelfTest() {
    RunGroup("B27", kClonesB27, BSF_COUNT(kClonesB27), nullptr, 0, &SeedB27, 27, -1);
    RunGroup("FB8", kClonesFB8, BSF_COUNT(kClonesFB8), kTablesFB8, BSF_COUNT(kTablesFB8), &SeedFB8, 26, -1);
    RunGroup("K33", kClonesK33, BSF_COUNT(kClonesK33), kTablesK33, BSF_COUNT(kTablesK33), &SeedK33, 28, 33);
    RunGroup("B28", kClonesB28, BSF_COUNT(kClonesB28), nullptr, 0, &SeedB28, 28, -1);
    RunGroup("F2", kClonesF2, BSF_COUNT(kClonesF2), kTablesF2, BSF_COUNT(kTablesF2), &SeedF2, 28, -1);
    RunGroup("K62", kClonesK62, BSF_COUNT(kClonesK62), kTablesK62, BSF_COUNT(kTablesK62), &SeedK62, 55, 62);
}

}  // namespace boss_sf

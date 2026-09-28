// BOF3X_SHADOW=boss_si: group BSI's 50 functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per kind (45, 51,
// 47, 60, 49, 52, 56, 57, 58), per fight (37, 38, 44, 40, 53, 42, 45, 49, 50,
// 51) and one for Arwan's effect task (the kind-3 dispatcher's slot 7) -
// twenty. BOF3X_BSI_RUN=<run> runs one (k45 .. k58, b37 .. b51, f7).
// docs/boss_si.md section 3.
//
// The clone rows are tools/boss_rows.py's (--unit <unit> --clones,
// 2026-09-28), each read against the disassembly; the tables' entry counts are
// the code's (the dispatchers' state values, the next table's address), not
// the tool's extents ("30 code entries" for kind 45's +1 table is 12, then its
// hook table's 3, then kind 51's table). Every function is called the way its
// root calls it (Clone::shape): a set-up as Boss_SetupTable's jmp, an end hook
// as BattleEnd_AwaitMemberTasks calls it, a hook with its word, a dispatcher
// with its state byte drawn below its table, the effect task's four as
// BattleTask_RunAll runs a slot (kTask - this group is its first user).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_si.h"
#include "game/boss_si_callees.h"
#include "game/move_script_bytes.h"

namespace boss_si {
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
constexpr U kU8 = 0xFFu;
#define BSI_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BSI_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BSI_FN(name) reinterpret_cast<const void*>(&::name)

// A clone row: name, base, size, calls, ours, the answer's mask, the shape.
bh::Clone Row(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, U ret, S shape,
              const bh::Imm* imms = nullptr, int n_imms = 0) {
    return bh::Clone{name, base, size, calls, n, imms, n_imms, nullptr, 0, ours, ret, false, shape};
}
// A dispatcher: by Sprite_Current[at], its table's entries drawn each round.
bh::Clone Disp(const char* name, U base, const void* ours, std::uint8_t at, std::uint8_t states) {
    bh::Clone c{name, base, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, ours, 0xFF, false, S::kDispatch};
    c.state_at = at;
    c.states = states;
    return c;
}

unsigned char& B(U address) { return Mem(address)[0]; }
unsigned char Byte(U v) { return static_cast<unsigned char>(v); }
U Above() { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0; }

// ===========================================================================
// What every run shares
// ===========================================================================

// MagicFx_CenterOnSide (ours, magic_lib.cpp) moves Sprite_Current's +0x34,
// +0x38 and +0x3E; set-up 38 reads +0x34 and +0x38 after it. Its stand-in is
// louder than the real one: half the time it writes Sprite_Current's +0x34 and
// +0x38 - an odd signed step from the fight's centre (the halving's rounding
// toward zero each way) or any dword - so a read before the call is told
// apart; and a quarter of the time it moves the centre too (read after it).
std::uint32_t CenterEffect(const std::uint32_t*, std::uint32_t answer) {
    const U n = bh::Noise();
    if (n & 1) {
        const U cx = static_cast<U>(Long(Mem(at::kCentreX)));
        const U cz = static_cast<U>(Long(Mem(at::kCentreZ)));
        static const U kSteps[] = {1, 0xFFFFFFFFu, 3, 0xFFFFFFFDu, 0x10001, 0xFFFEFFFFu, 0x7FFFFFFF, 0x80000001u};
        const U x = n & 2 ? cx + kSteps[(n >> 4) % 8] : bh::Noise();
        const U z = n & 4 ? cz + kSteps[(n >> 8) % 8] : bh::Noise();
        SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(x));
        SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(z));
    }
    if ((n >> 12) % 4 == 0) SetLong(Mem((n >> 14) & 1 ? at::kCentreX : at::kCentreZ), static_cast<std::int32_t>(bh::Noise()));
    return answer;
}

// Port_DroppedCall sits in every kind's +1 table (entries 1 and 10): a bare
// ret, listed with no arguments (docs/boss_sa.md section 3). Battle_CopyEnemyData
// reads the low byte of the slot and of the id (docs/boss_se.md section 3); the
// original pushes the id with EnemyData_FindByTag's upper bytes and the slot
// with them too. The two engine functions nobody owns, 0x4376F0 and 0x4376A0.
const bh::Callee kCallees[] = {
    {"Port_DroppedCall", ::bof3::addr::Port_DroppedCall, KeyOf(&::Port_DroppedCall), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"Battle_CopyEnemyData", ::bof3::addr::Battle_CopyEnemyData, KeyOf(&::Battle_CopyEnemyData), 2, {kU8, kU8},
     bh::Answer::kGarbage, 0, 0},
    {"MagicFx_CenterOnSide", ::bof3::addr::MagicFx_CenterOnSide, KeyOf(&::MagicFx_CenterOnSide), 0, {},
     bh::Answer::kGarbage, 0, 0, {}, &CenterEffect},
    {"0x4376F0", at::kTurnChance, at::kTurnChance, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x4376A0", at::kTurnClose, at::kTurnClose, 0, {}, bh::Answer::kGarbage, 0, 0},
};

// The cells beyond the harness's battle frame the 50 read or write.
const bh::Region kRegions[] = {
    {at::kScriptVar3, 4},      // the movement script's variables
    {at::kMusicTrack, 4},      // Music_Track
    {at::kCentreX, 8},         // the fight's centre
    {at::kKind2Z, 8},          // Field_Kind2Z, Field_Kind2X
};

// The clones of the run in progress (for the shared seeds), the byte their
// dispatchers read (the other state bytes are seeded inside the tables).
const bh::Clone* g_clones = nullptr;
unsigned g_small = 12;   // the other state bytes are drawn below this

// An enemy hook's word 0..2 with garbage above half the time - the dispatchers
// mask it, their entries get it whole.
void Args(unsigned k, U* a) {
    if (g_clones[k].shape == S::kEnemyHook) a[0] = Above() | (bh::Next() % 3);
}

// Every state byte +1..+4 but the one the clone's dispatcher reads, inside the
// tables (a dispatcher planted to read the wrong byte lands on an entry and is
// refused by a count, not a Fatal past its table).
void SeedStates(unsigned k) {
    const bh::Clone& c = g_clones[k];
    for (unsigned i = 1; i <= 4; ++i) {
        if (c.shape == S::kDispatch && i == c.state_at) continue;
        Sprite_Current[i] = Byte(bh::Next() % g_small);
    }
}

unsigned char BattleEndByte() {
    return Byte(bh::Often() ? BH_PICK(0, 1, 2, 3, 4, 6, 0xFD, 0x82, 8, 0xA, 0xFF) : bh::Next());
}

// What the 50 read again after a call that the standard disturbance does not
// move: the chapter's flag bits pointer (Ammonite after the tick), the current
// enemy's +5 (after EnemyData_FindByTag: the standard case 11 reaches it one
// time in 0x128), the task's +9 and +0xA, the owner's +1 / +2, the target.
void Disturb(U h) {
    const U v = h >> 16;
    switch ((h >> 8) % 5) {
    case 0: bh::SetPointer(0x929ED0, Mem(0x903F90 + 8 * (v % 40))); break;
    case 1: bh::Pointer(at::kCurrentEnemy)[5] = Byte(v); break;
    case 2: Sprite_Current[9 + (v & 1)] = Byte(v >> 1); break;
    case 3: bh::Pointer(at::kOwner)[1 + (v & 1)] = Byte(v >> 1); break;
    default: B(at::kTarget) = Byte(v); break;
    }
}

bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BSI_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

void RunGroup(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables,
              void (*seed)(unsigned), int fight, int kind, unsigned rounds = 6000, unsigned small = 12) {
    if (!Wants(run)) return;
    g_clones = clones;
    g_small = small;
    bh::Group g{"boss_si", clones, n, kCallees, BSI_COUNT(kCallees), tables, n_tables, kRegions, BSI_COUNT(kRegions),
                seed, &Disturb, rounds};
    g.args = &Args;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
    g_clones = nullptr;
}

// ===========================================================================
// The kinds
// ===========================================================================

constexpr bh::CallSite kCallsTick4C[] = {{0x4C, 0x5893A0}};

// The shape of eight of the nine kinds: a dispatcher by +1 (12), the entry
// (the stores and a tail tick), the hook (3, all BareRet).
void SeedKind(unsigned k) { SeedStates(k); }

// Kind 45 (Ammonite): its entry calls the tick, then tests the chapter's flag
// bit 2 and copies the enemy tagged 0x96 into the current enemy's slot.
constexpr bh::CallSite kCalls43ED20[] = {{0x4C, 0x5893A0}, {0x5A, 0x57C140}, {0x6B, 0x4948E0}, {0x7D, 0x4946C0}};
const bh::Clone kClonesK45[] = {
    Disp("BossAmmonite_Dispatch", 0x43ED00, BSI_FN(BossAmmonite_Dispatch), 1, 12),
    Row("BossAmmonite_Enter", 0x43ED20, 0x86, kCalls43ED20, BSI_N(kCalls43ED20), BSI_FN(BossAmmonite_Enter), 0, S::kState),
    Row("BossAmmonite_Hook", 0x43EDB0, 0x10, nullptr, 0, BSI_FN(BossAmmonite_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK45[] = {{0x64D990, 3, 4, 1}, {0x64D960, 12}};
void SeedK45(unsigned k) {
    SeedStates(k);
    // the slot byte +5 - 3: an enemy's 3..10, and the wrap below 3
    if (k == 1) bh::Pointer(at::kCurrentEnemy)[5] = Byte(bh::Often() ? 3 + bh::Next() % 8 : BH_PICK(0, 1, 2, 0xFF, 0x83));
}

const bh::Clone kClonesK51[] = {
    Disp("BossSample6_Dispatch", 0x43EDC0, BSI_FN(BossSample6_Dispatch), 1, 12),
    Row("BossSample6_Enter", 0x43EDE0, 0x51, kCallsTick4C, 1, BSI_FN(BossSample6_Enter), 0xFF, S::kState),
    Row("BossSample6_Hook", 0x43EE40, 0x10, nullptr, 0, BSI_FN(BossSample6_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK51[] = {{0x64D9CC, 3, 4, 1}, {0x64D99C, 12}};

const bh::Clone kClonesK47[] = {
    Disp("BossSample2_Dispatch", 0x43EF20, BSI_FN(BossSample2_Dispatch), 1, 12),
    Row("BossSample2_Enter", 0x43EF40, 0x51, kCallsTick4C, 1, BSI_FN(BossSample2_Enter), 0xFF, S::kState),
    Row("BossSample2_Hook", 0x43EFA0, 0x10, nullptr, 0, BSI_FN(BossSample2_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK47[] = {{0x64DA1C, 3, 4, 1}, {0x64D9EC, 12}};

const bh::Clone kClonesK60[] = {
    Disp("BossHugeSlug_Dispatch", 0x43EFB0, BSI_FN(BossHugeSlug_Dispatch), 1, 12),
    Row("BossHugeSlug_Enter", 0x43EFD0, 0x51, kCallsTick4C, 1, BSI_FN(BossHugeSlug_Enter), 0xFF, S::kState),
    Row("BossHugeSlug_Hook", 0x43F030, 0x10, nullptr, 0, BSI_FN(BossHugeSlug_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK60[] = {{0x64DA58, 3, 4, 1}, {0x64DA28, 12}};

const bh::Clone kClonesK49[] = {
    Disp("BossSample4_Dispatch", 0x43F0A0, BSI_FN(BossSample4_Dispatch), 1, 12),
    Row("BossSample4_Enter", 0x43F0C0, 0x51, kCallsTick4C, 1, BSI_FN(BossSample4_Enter), 0xFF, S::kState),
    Row("BossSample4_Hook", 0x43F120, 0x10, nullptr, 0, BSI_FN(BossSample4_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK49[] = {{0x64DAA8, 3, 4, 1}, {0x64DA78, 12}};

const bh::Clone kClonesK52[] = {
    Disp("BossSample7_Dispatch", 0x43F150, BSI_FN(BossSample7_Dispatch), 1, 12),
    Row("BossSample7_Enter", 0x43F170, 0x51, kCallsTick4C, 1, BSI_FN(BossSample7_Enter), 0xFF, S::kState),
    Row("BossSample7_Hook", 0x43F1D0, 0x10, nullptr, 0, BSI_FN(BossSample7_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK52[] = {{0x64DAF8, 3, 4, 1}, {0x64DAC8, 12}};

const bh::Clone kClonesK56[] = {
    Disp("BossManmo_Dispatch", 0x43F1E0, BSI_FN(BossManmo_Dispatch), 1, 12),
    Row("BossManmo_Enter", 0x43F200, 0x51, kCallsTick4C, 1, BSI_FN(BossManmo_Enter), 0xFF, S::kState),
    Row("BossManmo_Hook", 0x43F260, 0x10, nullptr, 0, BSI_FN(BossManmo_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK56[] = {{0x64DB34, 3, 4, 1}, {0x64DB04, 12}};

const bh::Clone kClonesK57[] = {
    Disp("BossChimera_Dispatch", 0x43F2D0, BSI_FN(BossChimera_Dispatch), 1, 12),
    Row("BossChimera_Enter", 0x43F2F0, 0x51, kCallsTick4C, 1, BSI_FN(BossChimera_Enter), 0xFF, S::kState),
    Row("BossChimera_Hook", 0x43F350, 0x10, nullptr, 0, BSI_FN(BossChimera_Hook), 0xFF, S::kEnemyHook),
};
const bh::DataTable kTablesK57[] = {{0x64DB88, 3, 4, 1}, {0x64DB58, 12}};

// Kind 58 (Arwan): the dispatcher, its entry, state 4's dispatcher by +2 (2)
// and step 0 (the task), state 5 (the wait), the hook. The other state bytes
// are drawn below 2 (the smaller table), so that a dispatcher reading the
// wrong one stays inside either table.
constexpr bh::CallSite kCalls43F520[] = {{0x2D, 0x435180}};
constexpr bh::CallSite kCalls43F580[] = {{0x9, 0x4376F0}, {0xE, 0x4376A0}};
const bh::Clone kClonesK58[] = {
    Disp("BossArwan_Dispatch", 0x43F480, BSI_FN(BossArwan_Dispatch), 1, 12),
    Row("BossArwan_Enter", 0x43F4A0, 0x51, kCallsTick4C, 1, BSI_FN(BossArwan_Enter), 0xFF, S::kState),
    Disp("BossArwan_State4Dispatch", 0x43F500, BSI_FN(BossArwan_State4Dispatch), 2, 2),
    Row("BossArwan_State4Fx", 0x43F520, 0x5E, kCalls43F520, BSI_N(kCalls43F520), BSI_FN(BossArwan_State4Fx), 0, S::kState),
    Row("BossArwan_State5Await", 0x43F580, 0x14, kCalls43F580, BSI_N(kCalls43F580), BSI_FN(BossArwan_State5Await), 0, S::kState),
    Row("BossArwan_Hook", 0x43F5A0, 0x10, nullptr, 0, BSI_FN(BossArwan_Hook), 0xFF, S::kEnemyHook),
};
enum : unsigned { kK58Dispatch, kK58Enter, kK58State4, kK58Fx, kK58Await, kK58Hook };
static_assert(kK58Hook + 1 == BSI_COUNT(kClonesK58), "kind 58's seeding indices");
const bh::DataTable kTablesK58[] = {{0x64DBE0, 3, 4, 1}, {0x64DBA8, 12}, {0x64DBD8, 2}};

// The enemy data index +0xF0: the area's eight records, and past them (a read
// the originals make as well; the bytes there are the exe's, the same on both
// passes).
unsigned char TypeByte() { return Byte(bh::Often() ? bh::Next() % 8 : bh::Next()); }

void SeedK58(unsigned k) {
    SeedStates(k);
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case kK58Fx:
        bh::Pointer(at::kCurrentEnemy)[0xF0] = TypeByte();
        // +2 up by one: its wrap
        if (bh::Half()) s[2] = Byte(BH_PICK(0, 1, 0xFF, 0x7F));
        break;
    case kK58Await: B(at::kFlags) = Byte(bh::Half() ? bh::Next() | 4 : bh::Next() & ~4u); break;
    default: break;
    }
}

// ===========================================================================
// The set-ups and their end hooks
// ===========================================================================

void SeedSetup(unsigned k) {
    switch (g_clones[k].shape) {
    case S::kEnd:
        B(at::kBattleEnd) = BattleEndByte();
        break;
    case S::kSetup:
        if (g_clones[k].base == 0x43EE50) {
            // set-up 38: enemy 7's place and the centre an odd signed step apart
            // (the halving toward zero each way), or anywhere
            unsigned char* const e7 = Mem(at::kEnemy7);
            const U cx = static_cast<U>(Long(Mem(at::kCentreX)));
            const U cz = static_cast<U>(Long(Mem(at::kCentreZ)));
            if (bh::Often()) SetLong(e7 + 0x34, static_cast<std::int32_t>(cx + BH_PICK(1, 0xFFFFFFFFu, 3, 0xFFFFFFFDu, 0x7FFFFFFF, 0x80000000u, 0)));
            if (bh::Often()) SetLong(e7 + 0x38, static_cast<std::int32_t>(cz + BH_PICK(1, 0xFFFFFFFFu, 3, 0xFFFFFFFDu, 0x7FFFFFFF, 0x80000000u, 0)));
        }
        break;
    default: break;
    }
}

constexpr bh::CallSite kCallsEndStep[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};

const bh::Clone kClonesB37[] = {
    Row("Boss37_Setup", 0x43ECC0, 0x1F, nullptr, 0, BSI_FN(Boss37_Setup), 0, S::kSetup),
    Row("Boss37_End", 0x43ECE0, 0x1A, kCallsEndStep, BSI_N(kCallsEndStep), BSI_FN(Boss37_End), 0, S::kEnd),
};
constexpr bh::CallSite kCalls43EE50[] = {{0x30, 0x4FC0E0}};
constexpr bh::CallSite kCalls43EED0[] = {{0x1E, 0x446DE0}, {0x23, 0x446E00}};
const bh::Clone kClonesB38[] = {
    Row("Boss38_Setup", 0x43EE50, 0x74, kCalls43EE50, BSI_N(kCalls43EE50), BSI_FN(Boss38_Setup), 0, S::kSetup),
    Row("Boss38_End", 0x43EED0, 0x28, kCalls43EED0, BSI_N(kCalls43EED0), BSI_FN(Boss38_End), 0, S::kEnd),
};
const bh::Clone kClonesB44[] = {Row("Boss44_Setup", 0x43EF00, 0x1F, nullptr, 0, BSI_FN(Boss44_Setup), 0, S::kSetup)};
const bh::Clone kClonesB40[] = {Row("Boss40_Setup", 0x43F040, 0x1F, nullptr, 0, BSI_FN(Boss40_Setup), 0, S::kSetup)};
const bh::Clone kClonesB53[] = {
    Row("Boss53_Setup", 0x43F060, 0x1F, nullptr, 0, BSI_FN(Boss53_Setup), 0, S::kSetup),
    Row("Boss53_End", 0x43F080, 0x1A, kCallsEndStep, BSI_N(kCallsEndStep), BSI_FN(Boss53_End), 0, S::kEnd),
};
const bh::Clone kClonesB42[] = {Row("Boss42_Setup", 0x43F130, 0x1F, nullptr, 0, BSI_FN(Boss42_Setup), 0, S::kSetup)};
const bh::Clone kClonesB45[] = {Row("Boss45_Setup", 0x43F270, 0x1F, nullptr, 0, BSI_FN(Boss45_Setup), 0, S::kSetup)};
const bh::Clone kClonesB49[] = {
    Row("Boss49_Setup", 0x43F290, 0x1F, nullptr, 0, BSI_FN(Boss49_Setup), 0, S::kSetup),
    Row("Boss49_End", 0x43F2B0, 0x1A, kCallsEndStep, BSI_N(kCallsEndStep), BSI_FN(Boss49_End), 0, S::kEnd),
};
constexpr bh::CallSite kCalls43F380[] = {{0x10, 0x446DE0}, {0x1D, 0x446E00}};
const bh::Clone kClonesB50[] = {
    Row("Boss50_Setup", 0x43F360, 0x1F, nullptr, 0, BSI_FN(Boss50_Setup), 0, S::kSetup),
    Row("Boss50_End", 0x43F380, 0x22, kCalls43F380, BSI_N(kCalls43F380), BSI_FN(Boss50_End), 0, S::kEnd),
};
const bh::Clone kClonesB51[] = {
    Row("Boss51_Setup", 0x43F5B0, 0x1F, nullptr, 0, BSI_FN(Boss51_Setup), 0, S::kSetup),
    Row("Boss51_End", 0x43F5D0, 0x1A, kCallsEndStep, BSI_N(kCallsEndStep), BSI_FN(Boss51_End), 0, S::kEnd),
};

// ===========================================================================
// Arwan's effect task (the kind-3 dispatcher's slot 7)
// ===========================================================================

constexpr bh::Imm kImms43F5F0[] = {{0xF, 0x43F630}, {0x17, 0x43F6C0}, {0x22, 0x43F750}, {0x2A, 0x43FE80}};
constexpr bh::CallSite kCalls43F630[] = {{0x76, 0x5891F0}};
constexpr bh::CallSite kCalls43F6C0[] = {{0x11, 0x589410}, {0x2C, 0x587740}, {0x6D, 0x4530D0}};
constexpr bh::CallSite kCalls43F750[] = {{0x11, 0x589410}, {0x1C, 0x5891F0}};
const bh::Clone kClonesF7[] = {
    Row("BossArwanFx_Dispatch", 0x43F5F0, 0x36, nullptr, 0, BSI_FN(BossArwanFx_Dispatch), 0, S::kTask, kImms43F5F0,
        BSI_N(kImms43F5F0)),
    Row("BossArwanFx_Start", 0x43F630, 0x86, kCalls43F630, BSI_N(kCalls43F630), BSI_FN(BossArwanFx_Start), 0, S::kTask),
    Row("BossArwanFx_Count", 0x43F6C0, 0x84, kCalls43F6C0, BSI_N(kCalls43F6C0), BSI_FN(BossArwanFx_Count), 0, S::kTask),
    Row("BossArwanFx_Finish", 0x43F750, 0x4C, kCalls43F750, BSI_N(kCalls43F750), BSI_FN(BossArwanFx_Finish), 0, S::kTask),
};
enum : unsigned { kF7Dispatch, kF7Start, kF7Count, kF7Finish };
static_assert(kF7Finish + 1 == BSI_COUNT(kClonesF7), "the task's seeding indices");

void SeedF7(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case kF7Dispatch: s[1] = Byte(bh::Next() % 4); break;
    case kF7Start:
        B(at::kEnemy0Type) = TypeByte();
        if (bh::Half()) s[1] = Byte(BH_PICK(0, 0xFF, 0x7F));
        break;
    case kF7Count:
        // the count +9: its stop 0xFF, its end 0, 1, 2; the wait +0xA: 0, 1, 2
        if (bh::Often()) s[9] = Byte(BH_PICK(0xFF, 0xFF, 0, 0, 1, 2, 0xFE, 0x2D));
        if (bh::Often()) s[0xA] = Byte(BH_PICK(0, 0, 1, 2, 0xFF, 0x2D));
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    RunGroup("k45", kClonesK45, BSI_COUNT(kClonesK45), kTablesK45, BSI_COUNT(kTablesK45), &SeedK45, 38, 45, 8000);
    RunGroup("k51", kClonesK51, BSI_COUNT(kClonesK51), kTablesK51, BSI_COUNT(kTablesK51), &SeedKind, 44, 51);
    RunGroup("k47", kClonesK47, BSI_COUNT(kClonesK47), kTablesK47, BSI_COUNT(kTablesK47), &SeedKind, 40, 47);
    RunGroup("k60", kClonesK60, BSI_COUNT(kClonesK60), kTablesK60, BSI_COUNT(kTablesK60), &SeedKind, 53, 60);
    RunGroup("k49", kClonesK49, BSI_COUNT(kClonesK49), kTablesK49, BSI_COUNT(kTablesK49), &SeedKind, 42, 49);
    RunGroup("k52", kClonesK52, BSI_COUNT(kClonesK52), kTablesK52, BSI_COUNT(kTablesK52), &SeedKind, 45, 52);
    RunGroup("k56", kClonesK56, BSI_COUNT(kClonesK56), kTablesK56, BSI_COUNT(kTablesK56), &SeedKind, 49, 56);
    RunGroup("k57", kClonesK57, BSI_COUNT(kClonesK57), kTablesK57, BSI_COUNT(kTablesK57), &SeedKind, 50, 57);
    RunGroup("k58", kClonesK58, BSI_COUNT(kClonesK58), kTablesK58, BSI_COUNT(kTablesK58), &SeedK58, 51, 58, 8000, 2);
    RunGroup("b37", kClonesB37, BSI_COUNT(kClonesB37), nullptr, 0, &SeedSetup, 37, -1);
    RunGroup("b38", kClonesB38, BSI_COUNT(kClonesB38), nullptr, 0, &SeedSetup, 38, -1, 8000);
    RunGroup("b44", kClonesB44, BSI_COUNT(kClonesB44), nullptr, 0, &SeedSetup, 44, -1);
    RunGroup("b40", kClonesB40, BSI_COUNT(kClonesB40), nullptr, 0, &SeedSetup, 40, -1);
    RunGroup("b53", kClonesB53, BSI_COUNT(kClonesB53), nullptr, 0, &SeedSetup, 53, -1);
    RunGroup("b42", kClonesB42, BSI_COUNT(kClonesB42), nullptr, 0, &SeedSetup, 42, -1);
    RunGroup("b45", kClonesB45, BSI_COUNT(kClonesB45), nullptr, 0, &SeedSetup, 45, -1);
    RunGroup("b49", kClonesB49, BSI_COUNT(kClonesB49), nullptr, 0, &SeedSetup, 49, -1);
    RunGroup("b50", kClonesB50, BSI_COUNT(kClonesB50), nullptr, 0, &SeedSetup, 50, -1);
    RunGroup("b51", kClonesB51, BSI_COUNT(kClonesB51), nullptr, 0, &SeedSetup, 51, -1);
    RunGroup("f7", kClonesF7, BSI_COUNT(kClonesF7), nullptr, 0, &SeedF7, 51, 58, 8000);
}

}  // namespace boss_si

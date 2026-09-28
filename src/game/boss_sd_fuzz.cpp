// BOF3X_SHADOW=boss_sd: group BSD's 52 functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per unit - the
// kinds 18, 21, 22, 23, 26, 24, 25, 27 and the set-ups 17..21, each with its
// fight id (Group::fight) and, for a kind, its kind (Group::kind).
// docs/boss_sd.md section 3. BOF3X_BSD_RUN=<unit> (K18, B17, ...) runs one.
//
// The clone rows are tools/boss_rows.py's (--unit <U> --clones, 2026-09-28),
// each read against the disassembly; the shapes are the plan's: a kind's
// dispatcher kDispatch with its state byte drawn below its table, its
// entrance kState (al the answer where it tail-jumps to Sprite_ScriptTick),
// its +0xF4 hook kEnemyHook, a set-up kSetup and its three hooks kEnd / kExit
// / kEvent. Every .data table the dispatchers and hooks jump through is a
// DataTable (its entries recorders while the Run lasts).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sd.h"
#include "game/boss_sd_callees.h"
#include "game/move_script_bytes.h"

namespace boss_sd {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using move_script::SetWord;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu;
#define BSD_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BSD_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BSD_FN(name) reinterpret_cast<const void*>(&::name)

// A clone row: the tool's, with its shape, answer mask and (for a
// dispatcher) its state byte and table size.
bh::Clone Row(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0,
              std::uint8_t state_at = 1, std::uint8_t states = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    c.state_at = state_at;
    c.states = states;
    return c;
}

// BH's three callees the set-ups and kind 26 call directly (not in the
// standard set).
const bh::Callee kCallees[] = {
    {"BossMap_UpdateFromEnemies", 0x43B180, KeyOf(&::BossMap_UpdateFromEnemies), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"Boss_SetByLeaderId", 0x43B130, KeyOf(&::Boss_SetByLeaderId), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"BossMap_SetCorners", 0x43B0D0, KeyOf(&::BossMap_SetCorners), 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0},
};

// The cells beyond the standard regions: the move-script counter the end
// hooks of set-ups 17 and 21 set, and the four words set-ups 18..20 keep for
// the area-79 kinds.
const bh::Region kRegions[] = {{at::kCounter0, 4}, {at::kKeptA6, 8}};

// --- seeding helpers -------------------------------------------------------------

unsigned char StatusByte() {
    return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 0x40, 0x20, 0x60, 0xBF, 0x41, 0xFF) : bh::Next());
}
unsigned char EndByte() { return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 3, 0xFD, 0x82, 0xFE) : bh::Next()); }

// A dispatcher's other state bytes inside their tables most of the time, so a
// dispatcher reading the wrong byte lands on another entry (a count) rather
// than past its table (a Fatal); never the byte the harness drew.
void OtherStates(unsigned drawn, unsigned below) {
    unsigned char* const s = Sprite_Current;
    if (!bh::Often()) return;
    for (unsigned b = 1; b <= 4; ++b)
        if (b != drawn) s[b] = static_cast<unsigned char>(bh::Next() % below);
}

// A hook's word: 0..2 (the harness's draw) with garbage above the byte half
// the time; an event hook's phase code, the ones the hook tests more often.
U HookWord(U drawn) { return (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (drawn & 0xFF); }
U PhaseWord(U low) { return (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | low; }

// The standard disturbance does not move these, and the functions read them
// after a call (or must be seen not to): 0x904AAD, the three enemies' status
// dwords and HP words, the kept words, the chapter's flag bits pointer,
// Sprite_Current's +5.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 8) {
    case 0: Mem(at::kWhoFell)[0] = b; break;
    case 1: Mem(at::kEnemy0Status)[1] = b; break;
    case 2: Mem(at::kEnemy1Status)[1] = b; break;
    case 3: Mem(at::kEnemy2Status)[1] = b; break;
    case 4: SetWord(Mem(h & 0x100 ? at::kEnemy1Hp : at::kEnemy0Hp), h >> 12); break;
    case 5: SetWord(Mem(at::kKeptA6 + 2 * ((h >> 12) % 4)), h >> 14); break;
    case 6: bh::SetPointer(at::kFlagBits, Mem(bh::at::kCondFlags + 8 * ((h >> 16) % 40))); break;
    default: Sprite_Current[5] = static_cast<unsigned char>(h & 0x200 ? 4 + ((h >> 16) & 1) : b); break;
    }
}

// Which unit the shadow name's run is limited to (BOF3X_BSD_RUN).
bool Wants(const char* unit) {
    const char* const only = std::getenv("BOF3X_BSD_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, unit) == 0;
}

void RunGroup(const char* unit, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables, void (*seed)(unsigned),
              void (*args)(unsigned, U*), int fight, int kind, unsigned phase_span = 0) {
    if (!Wants(unit)) return;
    bh::Group g{"boss_sd", clones, n, kCallees, BSD_COUNT(kCallees), tables, n_tables, kRegions, BSD_COUNT(kRegions), seed, &Disturb, 6000};
    g.phase_span = phase_span;
    g.args = args;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
}

// ===========================================================================
// The simple kinds: dispatcher, entrance, (action dispatcher,) hook
// ===========================================================================

constexpr bh::CallSite kCallsTick38[] = {{0x38, 0x5893A0}};   // the entrances' jmp Sprite_ScriptTick at +0x38

// Kind 18 (Mutant)
const bh::Clone kClonesK18[] = {
    Row("BossMutant_Dispatch", 0x43A590, 0x12, nullptr, 0, BSD_FN(BossMutant_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossMutant_Enter", 0x43A5B0, 0x3D, kCallsTick38, BSD_N(kCallsTick38), BSD_FN(BossMutant_Enter), S::kState, 0xFF),
    Row("BossMutant_ActDispatch", 0x43A5F0, 0x12, nullptr, 0, BSD_FN(BossMutant_ActDispatch), S::kDispatch, 0, 2, 6),
    Row("BossMutant_Hook", 0x43A610, 0x10, nullptr, 0, BSD_FN(BossMutant_Hook), S::kEnemyHook),
};
const bh::DataTable kTablesK18[] = {{0x64CEB0, 12}, {0x64CEE0, 6}, {0x64CEF8, 3, 4, 1}};

// Kind 24 (Emitai)
constexpr bh::CallSite kCalls43B300[] = {{0x2, 0x589330}, {0x13, 0x589410}, {0x18, 0x437470}};
const bh::Clone kClonesK24[] = {
    Row("BossEmitai_Dispatch", 0x43B280, 0x12, nullptr, 0, BSD_FN(BossEmitai_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossEmitai_Enter", 0x43B2A0, 0x3D, kCallsTick38, BSD_N(kCallsTick38), BSD_FN(BossEmitai_Enter), S::kState, 0xFF),
    Row("BossEmitai_ActDispatch", 0x43B2E0, 0x12, nullptr, 0, BSD_FN(BossEmitai_ActDispatch), S::kDispatch, 0, 2, 6),
    Row("BossEmitai_Death", 0x43B300, 0x5B, kCalls43B300, BSD_N(kCalls43B300), BSD_FN(BossEmitai_Death), S::kState),
    Row("BossEmitai_Hook", 0x43B360, 0x10, nullptr, 0, BSD_FN(BossEmitai_Hook), S::kEnemyHook),
};
const bh::DataTable kTablesK24[] = {{0x64D0C8, 12}, {0x64D0F8, 6}, {0x64D110, 3, 4, 1}};

// Kind 25 (Golem)
const bh::Clone kClonesK25[] = {
    Row("BossGolem_Dispatch", 0x43B370, 0x12, nullptr, 0, BSD_FN(BossGolem_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossGolem_Enter", 0x43B390, 0x3D, kCallsTick38, BSD_N(kCallsTick38), BSD_FN(BossGolem_Enter), S::kState, 0xFF),
    Row("BossGolem_Hook", 0x43B3D0, 0x10, nullptr, 0, BSD_FN(BossGolem_Hook), S::kEnemyHook),
};
const bh::DataTable kTablesK25[] = {{0x64D11C, 12}, {0x64D14C, 3, 4, 1}};

// Kind 27 (Garr)
constexpr bh::CallSite kCalls43B4D0[] = {{0x3D, 0x589590}, {0x4E, 0x5891F0}};
const bh::Clone kClonesK27[] = {
    Row("BossGarr_Dispatch", 0x43B4B0, 0x12, nullptr, 0, BSD_FN(BossGarr_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossGarr_Enter", 0x43B4D0, 0x57, kCalls43B4D0, BSD_N(kCalls43B4D0), BSD_FN(BossGarr_Enter), S::kState),
    Row("BossGarr_ActDispatch", 0x43B530, 0x12, nullptr, 0, BSD_FN(BossGarr_ActDispatch), S::kDispatch, 0, 2, 6),
    Row("BossGarr_Hook", 0x43B5A0, 0x10, nullptr, 0, BSD_FN(BossGarr_Hook), S::kEnemyHook),
};
const bh::DataTable kTablesK27[] = {{0x64D16C, 12}, {0x64D19C, 6}, {0x64D1B4, 3, 4, 1}};

// Kinds 21, 22, 23 (Claw, Cawer, Patrio)
constexpr bh::CallSite kCallsClaw[] = {{0x8, 0x57C140}, {0x46, 0x57C0F0}, {0x88, 0x589330}, {0x90, 0x5893A0}};
constexpr bh::CallSite kCalls43A860[] = {{0x8, 0x57C140}, {0x46, 0x57C0F0}, {0x86, 0x5893A0}};
const bh::Clone kClonesK21[] = {
    Row("BossClaw_Dispatch", 0x43A660, 0x12, nullptr, 0, BSD_FN(BossClaw_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossClaw_Enter", 0x43A680, 0x95, kCallsClaw, BSD_N(kCallsClaw), BSD_FN(BossClaw_Enter), S::kState, 0xFF),
    Row("BossClaw_Hook", 0x43A740, 0x10, nullptr, 0, BSD_FN(BossClaw_Hook), S::kEnemyHook),
};
const bh::DataTable kTablesK21[] = {{0x64CF78, 12}, {0x64CFA8, 3, 4, 1}};
const bh::Clone kClonesK22[] = {
    Row("BossCawer_Dispatch", 0x43A770, 0x12, nullptr, 0, BSD_FN(BossCawer_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossCawer_Enter", 0x43A790, 0x95, kCallsClaw, BSD_N(kCallsClaw), BSD_FN(BossCawer_Enter), S::kState, 0xFF),
    Row("BossCawer_Hook", 0x43A830, 0x10, nullptr, 0, BSD_FN(BossCawer_Hook), S::kEnemyHook),
};
const bh::DataTable kTablesK22[] = {{0x64CFB4, 12}, {0x64CFE4, 3, 4, 1}};
const bh::Clone kClonesK23[] = {
    Row("BossPatrio_Dispatch", 0x43A840, 0x12, nullptr, 0, BSD_FN(BossPatrio_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossPatrio_Enter", 0x43A860, 0x8B, kCalls43A860, BSD_N(kCalls43A860), BSD_FN(BossPatrio_Enter), S::kState, 0xFF),
    Row("BossPatrio_Hook", 0x43A8F0, 0x10, nullptr, 0, BSD_FN(BossPatrio_Hook), S::kEnemyHook),
};
const bh::DataTable kTablesK23[] = {{0x64CFF0, 12}, {0x64D020, 3, 4, 1}};

// The simple kinds' seed: the other state bytes inside the tables; the
// enemy's HP words (read back by kinds 21..23 from the kept words).
void SeedKind(unsigned k, const bh::Clone* clones) {
    const bh::Clone& c = clones[k];
    if (c.shape == S::kDispatch) OtherStates(c.state_at, c.states);
    unsigned char* const kept = Mem(at::kKeptA6);
    if (bh::Half()) SetWord(kept + 6, bh::Often() ? BH_PICK(0, 1, 0xFFFF, 0x7FFF, 0x8000) : bh::Next());
}
void SeedK18(unsigned k) { SeedKind(k, kClonesK18); }
void SeedK21(unsigned k) { SeedKind(k, kClonesK21); }
void SeedK22(unsigned k) { SeedKind(k, kClonesK22); }
void SeedK23(unsigned k) { SeedKind(k, kClonesK23); }
void SeedK24(unsigned k) { SeedKind(k, kClonesK24); }
void SeedK25(unsigned k) { SeedKind(k, kClonesK25); }
void SeedK27(unsigned k) { SeedKind(k, kClonesK27); }

// A kind's args: the hook's word with garbage above the byte; a dispatcher's
// first word (forwarded to the entry: Port_DroppedCall's recorder logs its
// byte) as drawn.
template <const bh::Clone* C> void ArgsKind(unsigned k, U* a) {
    if (C[k].shape == S::kEnemyHook) a[0] = HookWord(a[0]);
}

// ===========================================================================
// Kind 26 (Dodai 1 / 2)
// ===========================================================================

constexpr bh::CallSite kCalls43A900[] = {{0x14, 0x57C140}, {0x4C, 0x57C140}};
constexpr bh::CallSite kCalls43A990[] = {{0x8, 0x57C140}, {0x1D, 0x57C140}, {0xBB, 0x5893A0}};
constexpr bh::CallSite kCalls43AA70[] = {{0x2, 0x5891F0}, {0xA, 0x437470}, {0x22, 0x43B0D0}, {0x38, 0x5720C0}, {0x50, 0x5720C0}};
constexpr bh::CallSite kCalls43AB60[] = {{0xF, 0x5B93D2}, {0x1D, 0x587900}, {0x2B, 0x587900}};
const bh::Clone kClonesK26[] = {
    Row("BossDodai_Dispatch", 0x43A900, 0x8B, kCalls43A900, BSD_N(kCalls43A900), BSD_FN(BossDodai_Dispatch), S::kDispatch, 0, 1, 12),
    Row("BossDodai_Enter", 0x43A990, 0xC0, kCalls43A990, BSD_N(kCalls43A990), BSD_FN(BossDodai_Enter), S::kState, 0xFF),
    Row("BossDodai_ActDispatch", 0x43AA50, 0x12, nullptr, 0, BSD_FN(BossDodai_ActDispatch), S::kDispatch, 0, 2, 6),
    Row("BossDodai_Death", 0x43AA70, 0x88, kCalls43AA70, BSD_N(kCalls43AA70), BSD_FN(BossDodai_Death), S::kState),
    Row("BossDodai_HitPoseDispatch", 0x43AB00, 0x12, nullptr, 0, BSD_FN(BossDodai_HitPoseDispatch), S::kDispatch, 0, 2, 3),
    Row("BossDodai_HitShake", 0x43AB20, 0x1A, nullptr, 0, BSD_FN(BossDodai_HitShake), S::kState),
    Row("BossDodai_Hook", 0x43AB40, 0x10, nullptr, 0, BSD_FN(BossDodai_Hook), S::kEnemyHook),
    Row("BossHook_ActKindNone", 0x43AB50, 0x8, nullptr, 0, BSD_FN(BossHook_ActKindNone), S::kEnemyHook),
    Row("BossDodai_HitSound", 0x43AB60, 0x32, kCalls43AB60, BSD_N(kCalls43AB60), BSD_FN(BossDodai_HitSound), S::kEnemyHook),
};
enum : unsigned { kDodaiDispatch, kDodaiEnter, kDodaiAct, kDodaiDeath, kDodaiPose, kDodaiShake, kDodaiHook, kActKind, kHitSound };
static_assert(kHitSound + 1 == BSD_COUNT(kClonesK26), "kind 26's seeding indices");
const bh::DataTable kTablesK26[] = {{0x64D02C, 12}, {0x64D060, 6}, {0x64D078, 3}, {0x64D084, 3, 4, 1}};

void SeedK26(unsigned k) {
    const bh::Clone& c = kClonesK26[k];
    if (c.shape == S::kDispatch) OtherStates(c.state_at, c.states);
    unsigned char* const s = Sprite_Current;
    // the slot byte the dispatcher, the entrance and the death compare with 4 and 5
    s[5] = static_cast<unsigned char>(bh::Often() ? BH_PICK(4, 5, 4, 5, 3, 6, 0x84, 0) : bh::Next());
    switch (k) {
    case kDodaiDispatch:
        // the dispatcher reads Sprite_Current again after its Flags_Test,
        // and the disturbance may point it at another enemy: every enemy's
        // +1 inside the table (past it both sides jump through the dword after)
        for (unsigned i = 0; i < bh::at::kEnemyCount; ++i) bh::EnemyAt(i)[1] = static_cast<unsigned char>(bh::Next() % 12);
        break;
    case kHitSound: {
        // the signed word +0x108 against 0 (jle)
        unsigned char* const e = bh::Pointer(bh::at::kEnemyCurrent);
        SetWord(e + 0x108, bh::Often() ? BH_PICK(0, 1, 0xFFFF, 0x7FFF, 0x8000, 2) : bh::Next());
        break;
    }
    case kDodaiShake:
        // both parities of the frame counter, and a carry out of +0x38's low word
        if (bh::Half()) SetLong(s + 0x38, static_cast<std::int32_t>(bh::Often() ? BH_PICK(0xFFFFFE00u, 0x7FFFFE00u, 0xFE00, 0) : bh::Next()));
        break;
    default: break;
    }
}
void ArgsK26(unsigned k, U* a) {
    if (kClonesK26[k].shape == S::kEnemyHook) a[0] = HookWord(a[0]);
}

// ===========================================================================
// The set-ups
// ===========================================================================

constexpr bh::CallSite kCallsEndCounter[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};   // 0x43A640 and 0x43B730

// Set-up 17
const bh::Clone kClonesB17[] = {
    Row("Boss17_Setup", 0x43A620, 0x1F, nullptr, 0, BSD_FN(Boss17_Setup), S::kSetup),
    Row("Boss17_End", 0x43A640, 0x1A, kCallsEndCounter, BSD_N(kCallsEndCounter), BSD_FN(Boss17_End), S::kEnd),
};

// Set-ups 18, 19, 20: one body each, three times
constexpr bh::CallSite kCallsEvent[] = {{0xC, 0x43B180}, {0x7E, 0x43B180}};
constexpr bh::CallSite kCalls43AC50[] = {{0x1B, 0x446DE0}, {0x26, 0x446E00}, {0x36, 0x57C140}, {0x4B, 0x57C140},
                                         {0x57, 0x446E00}, {0x5E, 0x43B130}, {0x6C, 0x43B130}, {0x78, 0x446E20}};
constexpr bh::CallSite kCalls43AE10[] = {{0x1D, 0x446DE0}, {0x28, 0x446E00}, {0x38, 0x57C140}, {0x4D, 0x57C140},
                                         {0x59, 0x446E00}, {0x60, 0x43B130}, {0x6E, 0x43B130}, {0x7A, 0x446E20}};
constexpr bh::CallSite kCalls43AFD0[] = {{0x1D, 0x446DE0}, {0x28, 0x446E00}, {0x38, 0x57C140}, {0x4D, 0x57C140},
                                         {0x59, 0x446E00}, {0x70, 0x43B130}, {0x75, 0x446E20}};
constexpr bh::CallSite kCallsExit[] = {{0x13, 0x4949F0}, {0x1A, 0x4949D0}, {0x2B, 0x4949F0},
                                       {0x32, 0x4949D0}, {0x40, 0x4949F0}, {0x47, 0x4949D0}};
const bh::Clone kClonesB18[] = {
    Row("Boss18_Setup", 0x43ABA0, 0x1F, nullptr, 0, BSD_FN(Boss18_Setup), S::kSetup),
    Row("Boss18_Event", 0x43ABC0, 0x86, kCallsEvent, BSD_N(kCallsEvent), BSD_FN(Boss18_Event), S::kEvent, 0xFF),
    Row("Boss18_End", 0x43AC50, 0xB2, kCalls43AC50, BSD_N(kCalls43AC50), BSD_FN(Boss18_End), S::kEnd),
    Row("Boss18_Exit", 0x43AD10, 0x50, kCallsExit, BSD_N(kCallsExit), BSD_FN(Boss18_Exit), S::kExit),
};
const bh::Clone kClonesB19[] = {
    Row("Boss19_Setup", 0x43AD60, 0x1F, nullptr, 0, BSD_FN(Boss19_Setup), S::kSetup),
    Row("Boss19_Event", 0x43AD80, 0x86, kCallsEvent, BSD_N(kCallsEvent), BSD_FN(Boss19_Event), S::kEvent, 0xFF),
    Row("Boss19_End", 0x43AE10, 0xB4, kCalls43AE10, BSD_N(kCalls43AE10), BSD_FN(Boss19_End), S::kEnd),
    Row("Boss19_Exit", 0x43AED0, 0x50, kCallsExit, BSD_N(kCallsExit), BSD_FN(Boss19_Exit), S::kExit),
};
const bh::Clone kClonesB20[] = {
    Row("Boss20_Setup", 0x43AF20, 0x1F, nullptr, 0, BSD_FN(Boss20_Setup), S::kSetup),
    Row("Boss20_Event", 0x43AF40, 0x86, kCallsEvent, BSD_N(kCallsEvent), BSD_FN(Boss20_Event), S::kEvent, 0xFF),
    Row("Boss20_End", 0x43AFD0, 0xAF, kCalls43AFD0, BSD_N(kCalls43AFD0), BSD_FN(Boss20_End), S::kEnd),
    Row("Boss20_Exit", 0x43B080, 0x50, kCallsExit, BSD_N(kCallsExit), BSD_FN(Boss20_Exit), S::kExit),
};

// Set-up 21
constexpr bh::CallSite kCalls43B400[] = {{0x19, 0x4456C0}, {0x49, 0x4456C0}};
constexpr bh::CallSite kCalls43B480[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x19, 0x5891F0}, {0x20, 0x494A60}, {0x27, 0x494A60}};
const bh::Clone kClonesB21[] = {
    Row("Boss21_Setup", 0x43B3E0, 0x1F, nullptr, 0, BSD_FN(Boss21_Setup), S::kSetup),
    Row("Boss21_Event", 0x43B400, 0x71, kCalls43B400, BSD_N(kCalls43B400), BSD_FN(Boss21_Event), S::kEvent, 0xFF),
    Row("Boss21_Exit", 0x43B480, 0x30, kCalls43B480, BSD_N(kCalls43B480), BSD_FN(Boss21_Exit), S::kExit),
    Row("Boss21_End", 0x43B730, 0x1A, kCallsEndCounter, BSD_N(kCallsEndCounter), BSD_FN(Boss21_End), S::kEnd),
};

// The set-ups' seed: the battle-end byte and 0x904AAD at their bit values, the
// three enemies' +0x93 with and without 0x40, the kept words' sources.
void SeedSetup(unsigned) {
    Mem(at::kBattleEnd)[0] = EndByte();
    Mem(at::kWhoFell)[0] = EndByte();
    Mem(at::kEnemy0Status)[1] = StatusByte();
    Mem(at::kEnemy1Status)[1] = StatusByte();
    Mem(at::kEnemy2Status)[1] = StatusByte();
}

// An event hook's phase code: the harness's draw 0..6, the codes the hook
// tests (0 and 5; 3 for set-up 21) more often, with garbage above the byte.
template <const bh::Clone* C, U kTested1, U kTested2> void ArgsSetup(unsigned k, U* a) {
    if (C[k].shape != S::kEvent) return;
    const U low = bh::Half() ? (bh::Half() ? kTested1 : kTested2) : a[0] & 0xFF;
    a[0] = PhaseWord(low);
}

}  // namespace

void SelfTest() {
    RunGroup("K18", kClonesK18, BSD_COUNT(kClonesK18), kTablesK18, BSD_COUNT(kTablesK18), &SeedK18, &ArgsKind<kClonesK18>, 17, 18);
    RunGroup("B17", kClonesB17, BSD_COUNT(kClonesB17), nullptr, 0, &SeedSetup, nullptr, 17, -1);
    RunGroup("K21", kClonesK21, BSD_COUNT(kClonesK21), kTablesK21, BSD_COUNT(kTablesK21), &SeedK21, &ArgsKind<kClonesK21>, 18, 21);
    RunGroup("K22", kClonesK22, BSD_COUNT(kClonesK22), kTablesK22, BSD_COUNT(kTablesK22), &SeedK22, &ArgsKind<kClonesK22>, 18, 22);
    RunGroup("K23", kClonesK23, BSD_COUNT(kClonesK23), kTablesK23, BSD_COUNT(kTablesK23), &SeedK23, &ArgsKind<kClonesK23>, 18, 23);
    RunGroup("K26", kClonesK26, BSD_COUNT(kClonesK26), kTablesK26, BSD_COUNT(kTablesK26), &SeedK26, &ArgsK26, 18, 26, 12);
    RunGroup("K24", kClonesK24, BSD_COUNT(kClonesK24), kTablesK24, BSD_COUNT(kTablesK24), &SeedK24, &ArgsKind<kClonesK24>, 21, 24);
    RunGroup("B18", kClonesB18, BSD_COUNT(kClonesB18), nullptr, 0, &SeedSetup, &ArgsSetup<kClonesB18, 0, 5>, 18, -1);
    RunGroup("B19", kClonesB19, BSD_COUNT(kClonesB19), nullptr, 0, &SeedSetup, &ArgsSetup<kClonesB19, 0, 5>, 19, -1);
    RunGroup("B20", kClonesB20, BSD_COUNT(kClonesB20), nullptr, 0, &SeedSetup, &ArgsSetup<kClonesB20, 0, 5>, 20, -1);
    RunGroup("K25", kClonesK25, BSD_COUNT(kClonesK25), kTablesK25, BSD_COUNT(kTablesK25), &SeedK25, &ArgsKind<kClonesK25>, 21, 25);
    RunGroup("B21", kClonesB21, BSD_COUNT(kClonesB21), nullptr, 0, &SeedSetup, &ArgsSetup<kClonesB21, 3, 3>, 21, -1);
    RunGroup("K27", kClonesK27, BSD_COUNT(kClonesK27), kTablesK27, BSD_COUNT(kTablesK27), &SeedK27, &ArgsKind<kClonesK27>, 22, 27);
}

}  // namespace boss_sd

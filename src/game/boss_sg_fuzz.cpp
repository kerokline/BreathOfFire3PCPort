// BOF3X_SHADOW=boss_sg: group BSG's 53 functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per fight, kind and
// effect task - eleven (BOF3X_BSG_RUN=k34|b29|k35|k36|k37|b31|k38|b32|b33|f6|k40
// runs one). docs/boss_sg.md section 3.
//
// The clone rows are tools/boss_rows.py's (--unit <unit> --clones,
// 2026-09-28), each read against the disassembly; the tables' entry counts are
// the code's (the dispatchers' state values, the next table's address), not
// the tool's extents. Every function is called the way its root calls it
// (Clone::shape): a set-up as Boss_SetupTable's jmp, a hook with its word, a
// kind's dispatcher with its state byte drawn below its table, F6's functions
// as BattleTask_RunAll's task (kTask, Sprite_Current a task slot - the first
// group to use the shape), kind 40's quad as a callee with its turn.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sg.h"
#include "game/boss_sg_callees.h"
#include "game/move_script_bytes.h"

namespace boss_sg {
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
#define BSG_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BSG_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BSG_FN(name) reinterpret_cast<const void*>(&::name)

// A clone row: name, base, size, calls, the ours, the answer's mask, the shape.
bh::Clone Row(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, U ret, S shape) {
    return bh::Clone{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
}
// A kind's dispatcher: by Sprite_Current[at], its table's entries drawn each round.
bh::Clone Disp(const char* name, U base, const void* ours, std::uint8_t at, std::uint8_t states) {
    bh::Clone c{name, base, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, ours, 0, false, S::kDispatch};
    c.state_at = at;
    c.states = states;
    return c;
}

unsigned char& B(U address) { return Mem(address)[0]; }
U Above() { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0; }
U Picked(const U* values, unsigned n) { return bh::Often() ? bh::Pick(values, n) : bh::Next(); }
#define BSG_PICK(...) [] { static const U kV[] = {__VA_ARGS__}; return Picked(kV, sizeof kV / sizeof kV[0]); }()

// ===========================================================================
// What every run shares
// ===========================================================================

// BattleTask_Create answers "none" (0xFF) a third of the time - F6's strike
// tests for it - and a slot 0..47 otherwise (the standard listing never
// answers none).
std::uint32_t CreateEffect(const std::uint32_t*, std::uint32_t answer) {
    const U n = bh::Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 8) % 48);
}
// Battle_RemoveFromTurnOrder louder than the real one: set-up 32's end hook
// reads the member's +0x90 and +8 after it, and the next member's +0 - the
// effect moves them (BSE's Boss26_End, the same loop).
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
// BossActor_Clear louder than the real one: it also moves a byte of party
// member 0's +0x34 / +0x38, which set-up 33's end hook reads after it.
std::uint32_t ClearEffect(const std::uint32_t*, std::uint32_t answer) {
    const U n = bh::Noise();
    if (n % 2) Mem(n & 2 ? at::kMember0X : at::kMember0Z)[(n >> 2) % 4] ^= static_cast<unsigned char>(1 + (n >> 8) % 0xFF);
    return answer;
}
// Sound_PlayEffect louder than the real one: it also flips bit 2 of 0x904AAD
// half the time, which kind 40's flash reads again after it.
std::uint32_t SoundEffect(const std::uint32_t*, std::uint32_t answer) {
    if (bh::Noise() % 2) Mem(at::kScript)[0] ^= 4;
    return answer;
}
// The GTE stand-ins of kind 40's quad: a result from the inputs and the noise,
// written where the real callee writes (magic_s01_fuzz.cpp's shape), so the
// floats the draw computes from them are compared.
std::uint32_t RotMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* r = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* m = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[1]));
    for (unsigned i = 0; i < 9; ++i) m[i] = static_cast<short>(r[i % 3] * static_cast<int>(i + 1) + static_cast<int>(bh::Noise() & 0xFF));
    return answer;
}
std::uint32_t TransMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* m = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    const auto* v = reinterpret_cast<const long*>(static_cast<std::uintptr_t>(a[1]));
    for (unsigned i = 0; i < 3; ++i) {
        const long t = v[i] + static_cast<long>(bh::Noise() & 0xFFF);
        std::memcpy(m + 0x14 + 4 * i, &t, 4);
    }
    return answer;
}
std::uint32_t SetTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    bh::NoteBytes(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0])) + 0x14, 12);
    return answer;
}
std::uint32_t RotTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* v = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* t = reinterpret_cast<long*>(static_cast<std::uintptr_t>(a[1]));
    const U n = bh::Noise();
    t[0] = v[0] * 3 + static_cast<long>(n & 0x3FF) - 0x200;
    t[1] = v[1] * 5 - static_cast<long>((n >> 10) & 0x3FF);
    t[2] = v[2] * 7 + static_cast<long>(n >> 20);
    return answer;
}

// Beyond the standard set: the listings above; Sprite_PoseFromSet reading the
// animation's low byte (the original pushes a register whose upper bytes are
// leftovers - BSE's listing); kind 40's quad, called directly by two of the
// death's steps, reading only the low twelve bits of its turn (it builds the
// short turn << 4); the GTE calls with their stack pointers masked and the
// vectors by their bytes.
const bh::Callee kCallees[] = {
    {"BattleTask_Create", ::bof3::addr::BattleTask_Create, KeyOf(&::BattleTask_Create), 2, {kU8, kU8}, bh::Answer::kByte, 0, 47,
     {}, &CreateEffect},
    {"Battle_RemoveFromTurnOrder", ::bof3::addr::Battle_RemoveFromTurnOrder, KeyOf(&::Battle_RemoveFromTurnOrder), 1, {kU8},
     bh::Answer::kGarbage, 0, 0, {}, &RemoveEffect},
    {"BossActor_Clear", ::bof3::addr::BossActor_Clear, KeyOf(&::BossActor_Clear), 1, {kU8}, bh::Answer::kGarbage, 0, 0, {}, &ClearEffect},
    {"Sound_PlayEffect", ::bof3::addr::Sound_PlayEffect, KeyOf(&::Sound_PlayEffect), 1, {0xFFFF}, bh::Answer::kGarbage, 0, 0, {},
     &SoundEffect},
    {"Sprite_PoseFromSet", ::bof3::addr::Sprite_PoseFromSet, KeyOf(&::Sprite_PoseFromSet), 3, {kU8, kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"BossMikba_DrawQuad", 0x43E290, KeyOf(&::BossMikba_DrawQuad), 1, {0xFFF}, bh::Answer::kGarbage, 0, 0},
    {"Gte_RotMatrixYXZ", ::bof3::addr::Gte_RotMatrixYXZ, KeyOf(&::Gte_RotMatrixYXZ), 2, {0, 0}, bh::Answer::kGarbage, 0, 0, {6},
     &RotMatrixEffect},
    {"Gte_SetRotMatrix", ::bof3::addr::Gte_SetRotMatrix, KeyOf(&::Gte_SetRotMatrix), 1, {0}, bh::Answer::kGarbage, 0, 0, {18}},
    {"Gte_TransMatrix", ::bof3::addr::Gte_TransMatrix, KeyOf(&::Gte_TransMatrix), 2, {0, 0}, bh::Answer::kGarbage, 0, 0, {0, 12},
     &TransMatrixEffect},
    {"Gte_SetTransMatrix", ::bof3::addr::Gte_SetTransMatrix, KeyOf(&::Gte_SetTransMatrix), 1, {0}, bh::Answer::kGarbage, 0, 0, {},
     &SetTransEffect},
    {"Gte_RotTrans", ::bof3::addr::Gte_RotTrans, KeyOf(&::Gte_RotTrans), 2, {0, 0}, bh::Answer::kGarbage, 0, 0, {6}, &RotTransEffect},
};

// The cells beyond the harness's battle frame the 53 read or write
// (Music_Track 0x904131 lies in the standard Cond_Flags region).
const bh::Region kRegions[] = {
    {0x9035A4, 4},   // Field_ActiveMember (set-ups 31 and 32's exit hooks)
    {0x905E60, 8},   // Field_Kind2Z, Field_Kind2X (set-up 33's end hook, F6's rise)
};

// The clones of the run in progress, for Args.
const bh::Clone* g_clones = nullptr;

// An enemy hook's word 0..2 (the harness's draw) with garbage above half the
// time - the hooks mask it, their entries get it whole; kind 40's quad a whole
// word (it reads twelve bits).
void Args(unsigned k, U* a) {
    const bh::Clone& c = g_clones[k];
    if (c.shape == S::kEnemyHook) a[0] = Above() | (a[0] & 0xFF);
}

// What the 53 read again after a call that the standard disturbance does not
// move: 0x904AAD (kind 40's flash after the sound), party member 0's +0x34 /
// +0x38 (set-up 33 after BossActor_Clear), a member's +0x91 / +0 / +8 (set-up
// 32 after the turn-order call), Sprite_Current's +0xC / +0x10 / +0x18 / +0x1C
// (kind 40's quad after each RotTrans), the facing and Field_Kind2X / Z.
void Disturb(U h) {
    const U v = h >> 16;
    switch ((h >> 8) % 7) {
    case 0: B(at::kScript) = static_cast<unsigned char>(v); break;
    case 1: Mem(v & 1 ? at::kMember0X : at::kMember0Z)[(v >> 1) % 4] = static_cast<unsigned char>(v >> 4); break;
    case 2: bh::PartyOf(static_cast<unsigned char>(v % 3))[0x91] ^= 0x40; break;
    case 3: {
        static const unsigned kFields[] = {0xC, 0xD, 0x10, 0x13, 0x18, 0x1B, 0x1C, 0x1F};
        Sprite_Current[kFields[v % 8]] = static_cast<unsigned char>(v >> 3);
        break;
    }
    case 4: bh::PartyOf(static_cast<unsigned char>(v % 3))[v & 0x100 ? 0 : 8] ^= static_cast<unsigned char>(1 + (v >> 9) % 0x7F); break;
    case 5: B(at::kFacing) = static_cast<unsigned char>(v); break;
    default: Mem(0x905E60 + v % 8)[0] = static_cast<unsigned char>(v >> 3); break;
    }
}

// A dispatcher's other state bytes inside its own table, so a dispatcher
// reading the wrong byte lands on another entry (a count) rather than past
// its table (a Fatal); never the byte the harness drew.
void OtherStates(unsigned drawn, unsigned below) {
    unsigned char* const s = Sprite_Current;
    for (unsigned b = 1; b <= 4; ++b)
        if (b != drawn) s[b] = static_cast<unsigned char>(bh::Next() % below);
}

unsigned char EndByte() { return static_cast<unsigned char>(BSG_PICK(0, 1, 2, 3, 4, 6, 0xFD, 0xF9, 0x82, 0x84, 8, 0xA)); }

bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BSG_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

void RunGroup(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables,
              void (*seed)(unsigned), int fight, int kind, unsigned rounds = 6000, void (*settle)() = nullptr) {
    if (!Wants(run)) return;
    g_clones = clones;
    bh::Group g{"boss_sg", clones, n, kCallees, BSG_COUNT(kCallees), tables, n_tables, kRegions, BSG_COUNT(kRegions), seed,
                &Disturb, rounds};
    g.args = &Args;
    g.settle = settle;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
    g_clones = nullptr;
}

// ===========================================================================
// The simple kinds: dispatcher, entry, hook (34, 35, 36)
// ===========================================================================

constexpr bh::CallSite kCallsTick38[] = {{0x38, 0x5893A0}};   // the entries' jmp Sprite_ScriptTick at +0x38

void SeedSimple(unsigned k) {
    if (k == 0) OtherStates(1, 12);
}

const bh::Clone kClonesK34[] = {
    Disp("BossDolphin_Dispatch", 0x43CDE0, BSG_FN(BossDolphin_Dispatch), 1, 12),
    Row("BossDolphin_Enter", 0x43CE00, 0x3D, kCallsTick38, BSG_N(kCallsTick38), BSG_FN(BossDolphin_Enter), 0xFF, S::kState),
    Row("BossDolphin_Hook", 0x43CE40, 0x10, nullptr, 0, BSG_FN(BossDolphin_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK34[] = {{Key(BossDolphin_Hooks), 3, 4, 1}, {Key(BossDolphin_States), 12}};

const bh::Clone kClonesK35[] = {
    Disp("BossGisshan_Dispatch", 0x43CEA0, BSG_FN(BossGisshan_Dispatch), 1, 12),
    Row("BossGisshan_Enter", 0x43CEC0, 0x3D, kCallsTick38, BSG_N(kCallsTick38), BSG_FN(BossGisshan_Enter), 0xFF, S::kState),
    Row("BossGisshan_Hook", 0x43CF00, 0x10, nullptr, 0, BSG_FN(BossGisshan_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK35[] = {{Key(BossGisshan_Hooks), 3, 4, 1}, {Key(BossGisshan_States), 12}};

const bh::Clone kClonesK36[] = {
    Disp("BossScylla_Dispatch", 0x43CF10, BSG_FN(BossScylla_Dispatch), 1, 12),
    Row("BossScylla_Enter", 0x43CF30, 0x3D, kCallsTick38, BSG_N(kCallsTick38), BSG_FN(BossScylla_Enter), 0xFF, S::kState),
    Row("BossScylla_Hook", 0x43CF70, 0x10, nullptr, 0, BSG_FN(BossScylla_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK36[] = {{Key(BossScylla_Hooks), 3, 4, 1}, {Key(BossScylla_States), 12}};

// ===========================================================================
// Kinds 37 and 38: dispatcher, entry, action dispatcher, death, hook
// ===========================================================================

void SeedDeathKind(unsigned k) {
    if (k == 0) OtherStates(1, 12);
    if (k == 2) OtherStates(2, 6);
}

constexpr bh::CallSite kCalls43D050[] = {{0x2, 0x589330}, {0x13, 0x589410}, {0x18, 0x437470}};
const bh::Clone kClonesK37[] = {
    Disp("BossGarr2_Dispatch", 0x43CFD0, BSG_FN(BossGarr2_Dispatch), 1, 12),
    Row("BossGarr2_Enter", 0x43CFF0, 0x3D, kCallsTick38, BSG_N(kCallsTick38), BSG_FN(BossGarr2_Enter), 0xFF, S::kState),
    Disp("BossGarr2_ActDispatch", 0x43D030, BSG_FN(BossGarr2_ActDispatch), 2, 6),
    Row("BossGarr2_Death", 0x43D050, 0x5C, kCalls43D050, BSG_N(kCalls43D050), BSG_FN(BossGarr2_Death), 0, S::kState),
    Row("BossGarr2_Hook", 0x43D0B0, 0x10, nullptr, 0, BSG_FN(BossGarr2_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK37[] = {{Key(BossGarr2_Hooks), 3, 4, 1}, {Key(BossGarr2_States), 12}, {Key(BossGarr2_ActSubs), 6}};

constexpr bh::CallSite kCalls43D160[] = {{0x4C, 0x5893A0}};
constexpr bh::CallSite kCalls43D1E0[] = {{0x2, 0x589330}, {0xA, 0x589410}, {0xF, 0x437470}};
const bh::Clone kClonesK38[] = {
    Disp("BossDZombie_Dispatch", 0x43D140, BSG_FN(BossDZombie_Dispatch), 1, 12),
    Row("BossDZombie_Enter", 0x43D160, 0x51, kCalls43D160, BSG_N(kCalls43D160), BSG_FN(BossDZombie_Enter), 0xFF, S::kState),
    Disp("BossDZombie_ActDispatch", 0x43D1C0, BSG_FN(BossDZombie_ActDispatch), 2, 6),
    Row("BossDZombie_Death", 0x43D1E0, 0x52, kCalls43D1E0, BSG_N(kCalls43D1E0), BSG_FN(BossDZombie_Death), 0, S::kState),
    Row("BossDZombie_Hook", 0x43D240, 0x10, nullptr, 0, BSG_FN(BossDZombie_Hook), 0, S::kEnemyHook),
};
const bh::DataTable kTablesK38[] = {{Key(BossDZombie_Hooks), 3, 4, 1}, {Key(BossDZombie_States), 12}, {Key(BossDZombie_ActSubs), 6}};

// ===========================================================================
// Set-ups 29, 31, 32 and 33
// ===========================================================================

constexpr bh::CallSite kCalls43CE70[] = {{0x10, 0x446DE0}, {0x1D, 0x446E00}};
const bh::Clone kClonesB29[] = {
    Row("Boss29_Setup", 0x43CE50, 0x1F, nullptr, 0, BSG_FN(Boss29_Setup), 0, S::kSetup),
    Row("Boss29_End", 0x43CE70, 0x22, kCalls43CE70, BSG_N(kCalls43CE70), BSG_FN(Boss29_End), 0, S::kEnd),
};
void SeedEnd(unsigned k) {
    if (k != 1) return;
    B(at::kBattleEnd) = EndByte();
    B(at::kMusicFlags) = static_cast<unsigned char>(BSG_PICK(0x40, 0, 0xFF, 0xBF));
}

constexpr bh::CallSite kCalls43D0E0[] = {{0x10, 0x446DE0}, {0x1C, 0x446E00}};
constexpr bh::CallSite kCalls43D110[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x1A, 0x5891F0}};
const bh::Clone kClonesB31[] = {
    Row("Boss31_Setup", 0x43D0C0, 0x1F, nullptr, 0, BSG_FN(Boss31_Setup), 0, S::kSetup),
    Row("Boss31_End", 0x43D0E0, 0x21, kCalls43D0E0, BSG_N(kCalls43D0E0), BSG_FN(Boss31_End), 0, S::kEnd),
    Row("Boss31_Exit", 0x43D110, 0x2C, kCalls43D110, BSG_N(kCalls43D110), BSG_FN(Boss31_Exit), 0, S::kExit),
};

constexpr bh::CallSite kCalls43D270[] = {{0x1C, 0x446650}, {0x58, 0x589110}, {0x6F, 0x446650}, {0xAB, 0x589110},
                                         {0xC2, 0x446650}, {0xFE, 0x589110}, {0x120, 0x446DE0}, {0x125, 0x446E00}};
constexpr bh::CallSite kCalls43D3A0[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x1A, 0x5891F0}};
const bh::Clone kClonesB32[] = {
    Row("Boss32_Setup", 0x43D250, 0x1F, nullptr, 0, BSG_FN(Boss32_Setup), 0, S::kSetup),
    Row("Boss32_End", 0x43D270, 0x12A, kCalls43D270, BSG_N(kCalls43D270), BSG_FN(Boss32_End), 0, S::kEnd),
    Row("Boss32_Exit", 0x43D3A0, 0x23, kCalls43D3A0, BSG_N(kCalls43D3A0), BSG_FN(Boss32_Exit), 0, S::kExit),
};
void SeedB32(unsigned k) {
    if (k != 1) return;
    B(at::kBattleEnd) = static_cast<unsigned char>(bh::Often() ? B(at::kBattleEnd) | 2 : EndByte());
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(m));
        p[0] = static_cast<unsigned char>(bh::Often() ? p[0] | 1 : p[0] & ~1u);
        if (bh::Often()) p[8] = static_cast<unsigned char>(BSG_PICK(0, 1, 0xE3, 0xE4, 0xFB, 0xFC, 0x7F));
    }
}

constexpr bh::CallSite kCalls43D690[] = {{0x9, 0x446E00}, {0x10, 0x494A60}, {0x35, 0x446DE0}};
const bh::Clone kClonesB33[] = {
    Row("Boss33_Setup", 0x43D670, 0x1F, nullptr, 0, BSG_FN(Boss33_Setup), 0, S::kSetup),
    Row("Boss33_End", 0x43D690, 0x3A, kCalls43D690, BSG_N(kCalls43D690), BSG_FN(Boss33_End), 0, S::kEnd),
};
void SeedB33(unsigned k) {
    if (k == 1) B(at::kBattleEnd) = EndByte();
}

// ===========================================================================
// F6: Weretigr's turn and its trail (kTask: Sprite_Current a task slot)
// ===========================================================================

constexpr bh::CallSite kCalls43D6F0[] = {{0x1D, 0x588F20}};
constexpr bh::CallSite kCalls43D720[] = {{0x13, 0x5891F0}};
constexpr bh::CallSite kCalls43D750[] = {{0x0, 0x589410}, {0xB, 0x5891F0}};
constexpr bh::CallSite kCalls43D770[] = {{0xA3, 0x5891F0}, {0xB3, 0x589410}};   // +0x18 is a jmp to the shared tail at +0xB3
constexpr bh::CallSite kCalls43D830[] = {{0x5, 0x435180},  {0xB2, 0x4FB9F0}, {0xD7, 0x4FBBD0}, {0xE5, 0x5891F0}, {0x109, 0x4FB9F0},
                                         {0x12E, 0x4FBBD0}, {0x13C, 0x5891F0}, {0x148, 0x4530D0}, {0x158, 0x589410}};
constexpr bh::CallSite kCalls43D990[] = {{0x0, 0x589410}, {0xC1, 0x589410}};
constexpr bh::CallSite kCalls43DA60[] = {{0x5B, 0x589410}};
constexpr bh::CallSite kCalls43DAC0[] = {{0x39, 0x5891F0}, {0x47, 0x4351F0}};
constexpr bh::CallSite kCalls43DB10[] = {{0x1D, 0x588F20}};
constexpr bh::CallSite kCalls43DB40[] = {{0x52, 0x589410}};
constexpr bh::CallSite kCalls43DBA0[] = {{0x2C, 0x589410}, {0x45, 0x4351F0}};
const bh::Clone kClonesF6[] = {
    Row("BossWeretigrFx_Task", 0x43D6D0, 0x12, nullptr, 0, BSG_FN(BossWeretigrFx_Task), 0, S::kTask),
    Row("BossWeretigrFx_Main", 0x43D6F0, 0x23, kCalls43D6F0, BSG_N(kCalls43D6F0), BSG_FN(BossWeretigrFx_Main), 0, S::kTask),
    Row("BossWeretigrFx_Begin", 0x43D720, 0x2E, kCalls43D720, BSG_N(kCalls43D720), BSG_FN(BossWeretigrFx_Begin), 0, S::kTask),
    Row("BossWeretigrFx_AwaitPose", 0x43D750, 0x1C, kCalls43D750, BSG_N(kCalls43D750), BSG_FN(BossWeretigrFx_AwaitPose), 0, S::kTask),
    Row("BossWeretigrFx_Rise", 0x43D770, 0xB8, kCalls43D770, BSG_N(kCalls43D770), BSG_FN(BossWeretigrFx_Rise), 0xFF, S::kTask),
    Row("BossWeretigrFx_Strike", 0x43D830, 0x15F, kCalls43D830, BSG_N(kCalls43D830), BSG_FN(BossWeretigrFx_Strike), 0xFF, S::kTask),
    Row("BossWeretigrFx_Leap", 0x43D990, 0xC7, kCalls43D990, BSG_N(kCalls43D990), BSG_FN(BossWeretigrFx_Leap), 0xFF, S::kTask),
    Row("BossWeretigrFx_Fly", 0x43DA60, 0x60, kCalls43DA60, BSG_N(kCalls43DA60), BSG_FN(BossWeretigrFx_Fly), 0xFF, S::kTask),
    Row("BossWeretigrFx_Finish", 0x43DAC0, 0x4E, kCalls43DAC0, BSG_N(kCalls43DAC0), BSG_FN(BossWeretigrFx_Finish), 0, S::kTask),
    Row("BossWeretigrFx_Trail", 0x43DB10, 0x23, kCalls43DB10, BSG_N(kCalls43DB10), BSG_FN(BossWeretigrFx_Trail), 0, S::kTask),
    Row("BossWeretigrFx_TrailBegin", 0x43DB40, 0x57, kCalls43DB40, BSG_N(kCalls43DB40), BSG_FN(BossWeretigrFx_TrailBegin), 0xFF,
        S::kTask),
    Row("BossWeretigrFx_TrailFade", 0x43DBA0, 0x4B, kCalls43DBA0, BSG_N(kCalls43DBA0), BSG_FN(BossWeretigrFx_TrailFade), 0, S::kTask),
};
const bh::DataTable kTablesF6[] = {{Key(BossWeretigrFx_Steps), 2}, {Key(BossWeretigrFx_MainSteps), 7}, {Key(BossWeretigrFx_TrailSteps), 2}};

// F6 and kind 40 read Sprite_Current again after their calls (its +0, +2, +9,
// +0xB): the standard disturbance re-points it one call in 24, so a quarter
// of the disturbances also re-point it (at another of the four task slots,
// or enemies) and, for F6, a quarter flip its +0 between 0 and not (Noise:
// the same on both passes).
void SettleF6() {
    const U n = bh::Noise();
    if (n % 4 == 0) Sprite_Current = bh::TaskAt(n >> 8);
    else if (n % 4 == 1) Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] ? 0 : 1 + (n >> 8) % 0xFF);
}
void SettleK40() {
    const U n = bh::Noise();
    if (n % 4 == 0) Sprite_Current = bh::EnemyAt(n >> 8);
}

U Signed32() { return BSG_PICK(0, 1, 0xFFFFFFFFu, 0x7FFFFFFF, 0x80000000u, 0x1000000, 0xFF000000u, 0x4000000, 0x28, 0x10); }
unsigned char ActorByte() { return static_cast<unsigned char>(bh::Often() ? 3 + bh::Next() % 8 : bh::Next() % 3); }

void SeedF6(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case 0: s[1] = static_cast<unsigned char>(bh::Next() % 2); OtherStates(1, 2); break;
    case 1: s[2] = static_cast<unsigned char>(bh::Next() % 7); OtherStates(2, 7); break;
    case 9: s[2] = static_cast<unsigned char>(bh::Next() % 2); OtherStates(2, 2); break;
    case 4: {
        // the height against its goal: equal, one either side, the signed ends
        const U goal = Signed32();
        SetLong(s + 0x20, static_cast<std::int32_t>(goal));
        const U pick = bh::Next() % 5;
        SetLong(s + 0x3C, static_cast<std::int32_t>(pick == 0 ? goal : pick == 1 ? goal - 1 : pick == 2 ? goal + 1 : Signed32()));
        B(at::kTarget) = static_cast<unsigned char>(BSG_PICK(0, 1, 2, 3, 4, 10, 2, 3));
        B(at::kFacing) = static_cast<unsigned char>(BSG_PICK(0, 1, 2, 3, 0, 1, 2, 3, 0xFF, 0x80));
        break;
    }
    case 5:
        B(at::kTarget) = static_cast<unsigned char>(BSG_PICK(0, 1, 2, 3, 4, 10, 2, 3));
        s[0xB] = static_cast<unsigned char>(BSG_PICK(0, 0, 1, 0x10, 0xFF));
        break;
    case 6:
    case 8: B(at::kActor) = ActorByte(); break;
    case 7: {
        const U fall = bh::Next() & 0x7F;
        SetLong(s + 0x20, static_cast<std::int32_t>(-fall));
        if (bh::Often()) SetLong(s + 0x14, static_cast<std::int32_t>(-0x40 + fall));
        break;
    }
    case 11: s[9] = static_cast<unsigned char>(BSG_PICK(0, 1, 2, 0xFF, 0x80)); break;
    default: break;
    }
}

// ===========================================================================
// Kind 40 (Mikba): dispatcher, entry, two dispatchers, the death's six steps,
// hook, the quad
// ===========================================================================

constexpr bh::CallSite kCalls43DC10[] = {{0x38, 0x5893A0}};
constexpr bh::CallSite kCalls43DC90[] = {{0x0, 0x5893A0}};
constexpr bh::CallSite kCalls43DCC0[] = {{0x1E, 0x587740}, {0x3F, 0x4449E0}};
constexpr bh::CallSite kCalls43DD30[] = {{0x0, 0x589410}, {0x19, 0x461E10}, {0x8B, 0x587740}, {0x9B, 0x43E290}};
constexpr bh::CallSite kCalls43DDE0[] = {{0x53, 0x43E290}};
constexpr bh::CallSite kCalls43DE60[] = {{0x5, 0x589590}, {0x15, 0x5891F0}, {0x23, 0x5893A0}, {0x2D, 0x454590}};
constexpr bh::CallSite kCalls43DEA0[] = {{0x0, 0x5893A0}, {0x5, 0x454810}, {0xE, 0x437470}};
constexpr bh::CallSite kCalls43E290[] = {{0x15, 0x5A79A0},  {0x2B, 0x5A77C0},  {0x34, 0x461E50},  {0x39, 0x5A7B90},
                                         {0x5E, 0x5A80B0},  {0x68, 0x5A8DE0},  {0x83, 0x5A8100},  {0x8D, 0x5A8E00},
                                         {0xA5, 0x5A79A0},  {0xEF, 0x5A8200},  {0x15B, 0x5A8200}, {0x1C7, 0x5A8200},
                                         {0x231, 0x5A8200}, {0x289, 0x5A75D0}, {0x292, 0x461E50}, {0x29A, 0x5A7BC0}};
const bh::Clone kClonesK40[] = {
    Disp("BossMikba_Dispatch", 0x43DBF0, BSG_FN(BossMikba_Dispatch), 1, 12),
    Row("BossMikba_Enter", 0x43DC10, 0x3D, kCalls43DC10, BSG_N(kCalls43DC10), BSG_FN(BossMikba_Enter), 0xFF, S::kState),
    Disp("BossMikba_ActDispatch", 0x43DC50, BSG_FN(BossMikba_ActDispatch), 2, 6),
    Disp("BossMikba_DeathDispatch", 0x43DC70, BSG_FN(BossMikba_DeathDispatch), 3, 7),
    Row("BossMikba_DeathWait", 0x43DC90, 0x27, kCalls43DC90, BSG_N(kCalls43DC90), BSG_FN(BossMikba_DeathWait), 0, S::kState),
    Row("BossMikba_DeathFlash", 0x43DCC0, 0x66, kCalls43DCC0, BSG_N(kCalls43DCC0), BSG_FN(BossMikba_DeathFlash), 0, S::kState),
    Row("BossMikba_DeathOpen", 0x43DD30, 0xAC, kCalls43DD30, BSG_N(kCalls43DD30), BSG_FN(BossMikba_DeathOpen), 0, S::kState),
    Row("BossMikba_DeathSpread", 0x43DDE0, 0x7B, kCalls43DDE0, BSG_N(kCalls43DDE0), BSG_FN(BossMikba_DeathSpread), 0, S::kState),
    Row("BossMikba_DeathLoad", 0x43DE60, 0x3E, kCalls43DE60, BSG_N(kCalls43DE60), BSG_FN(BossMikba_DeathLoad), 0, S::kState),
    Row("BossMikba_DeathEnd", 0x43DEA0, 0x3E, kCalls43DEA0, BSG_N(kCalls43DEA0), BSG_FN(BossMikba_DeathEnd), 0, S::kState),
    Row("BossMikba_Hook", 0x43DEE0, 0x10, nullptr, 0, BSG_FN(BossMikba_Hook), 0, S::kEnemyHook),
    Row("BossMikba_DrawQuad", 0x43E290, 0x2A5, kCalls43E290, BSG_N(kCalls43E290), BSG_FN(BossMikba_DrawQuad), 0, S::kCallee),
};
const bh::DataTable kTablesK40[] = {
    {Key(BossMikba_Hooks), 3, 4, 1}, {Key(BossMikba_States), 12}, {Key(BossMikba_ActSubs), 6}, {Key(BossMikba_DeathSteps), 7}};

U Half32() { return BSG_PICK(0, 1, 2, 3, 0xFFFFFFFFu, 0xFFFFFFFDu, 0x7FFFFFFF, 0x80000000u, 0x80000001u, 0xBE, 0x68); }

void SeedK40(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case 0: OtherStates(1, 12); break;
    case 2: OtherStates(2, 6); break;
    case 3: OtherStates(3, 7); break;
    case 5: {
        const auto count = static_cast<unsigned char>(BSG_PICK(1, 0, 2, 0x40, 0x20, 0x80, 0xFF));
        s[9] = count;
        s[0xA] = static_cast<unsigned char>(bh::Half() ? count : BSG_PICK(1, 0, 2, 0x20, 0x40));
        B(at::kScript) = static_cast<unsigned char>(bh::Half() ? B(at::kScript) | 4 : B(at::kScript) & ~4u);
        break;
    }
    case 7:
        SetLong(s + 0xC, static_cast<std::int32_t>(BSG_PICK(0x28, 0x29, 0x27, 0x2A, 0, 0xFFFFFFFFu, 0x7FFFFFFF, 0x80000000u, 0xBE)));
        SetLong(s + 0x10, static_cast<std::int32_t>(BSG_PICK(0x28, 0x29, 0x27, 0x2A, 0, 0xFFFFFFFFu, 0x7FFFFFFF, 0x80000000u, 0x68)));
        s[0xA] = static_cast<unsigned char>(BSG_PICK(0x3F, 0x3E, 0x40, 0x41, 0xFF, 0, 1));
        break;
    case 11:
        SetLong(s + 0xC, static_cast<std::int32_t>(Half32()));
        SetLong(s + 0x10, static_cast<std::int32_t>(Half32()));
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    RunGroup("k34", kClonesK34, BSG_COUNT(kClonesK34), kTablesK34, BSG_COUNT(kTablesK34), &SeedSimple, 29, 34);
    RunGroup("b29", kClonesB29, BSG_COUNT(kClonesB29), nullptr, 0, &SeedEnd, 29, -1);
    RunGroup("k35", kClonesK35, BSG_COUNT(kClonesK35), kTablesK35, BSG_COUNT(kTablesK35), &SeedSimple, 30, 35);
    RunGroup("k36", kClonesK36, BSG_COUNT(kClonesK36), kTablesK36, BSG_COUNT(kTablesK36), &SeedSimple, 30, 36);
    RunGroup("k37", kClonesK37, BSG_COUNT(kClonesK37), kTablesK37, BSG_COUNT(kTablesK37), &SeedDeathKind, 31, 37);
    RunGroup("b31", kClonesB31, BSG_COUNT(kClonesB31), nullptr, 0, &SeedEnd, 31, -1);
    RunGroup("k38", kClonesK38, BSG_COUNT(kClonesK38), kTablesK38, BSG_COUNT(kTablesK38), &SeedDeathKind, 32, 38);
    RunGroup("b32", kClonesB32, BSG_COUNT(kClonesB32), nullptr, 0, &SeedB32, 32, -1);
    RunGroup("b33", kClonesB33, BSG_COUNT(kClonesB33), nullptr, 0, &SeedB33, 33, -1);
    RunGroup("f6", kClonesF6, BSG_COUNT(kClonesF6), kTablesF6, BSG_COUNT(kTablesF6), &SeedF6, 33, -1, 6000, &SettleF6);
    RunGroup("k40", kClonesK40, BSG_COUNT(kClonesK40), kTablesK40, BSG_COUNT(kTablesK40), &SeedK40, 34, 40, 6000, &SettleK40);
}

}  // namespace boss_sg

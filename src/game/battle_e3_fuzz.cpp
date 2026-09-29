// BOF3X_SHADOW=battle_e3: group BE3's 49 functions through the boss harness's
// engine frame (boss_harness.h, Group::engine; docs/boss_harness.md section
// 10), once at start-up: two boss_harness::Runs - "EO", the enemy side
// (EnemyOp_Steps 7..9, their sub-states, the action's end, the roll, the cue,
// the fixed-point helper), Sprite_Current an enemy; and "OBJ", the party
// objects' states 6, 10, 11 and 26 and the party helpers, Sprite_Current a
// member. docs/battle_e3.md section 4. BOF3X_BE3_RUN=EO or OBJ runs one.
//
// The clone rows are tools/band_rows.py's (--group BE3 --clones, 2026-09-29),
// each read against the disassembly; the shapes are section 10.3's: the enemy
// dispatchers kDispatch with their state byte drawn below their table, the
// enemy states kState (al the answer where every path tail-jumps to a tick),
// the enemy helpers kCallee; the party objects' dispatchers and states
// kMember (a dispatcher's byte drawn below its table), the party helpers
// kHelper. Every .data table a dispatcher goes through is a DataTable (its
// entries recorders while the Run lasts).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e3.h"
#include "game/battle_e3_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"

namespace battle_e3 {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BE3_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BE3_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BE3_FN(name) reinterpret_cast<const void*>(&::name)

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
unsigned char* Enemy() { return bh::Pointer(at::kCurrentEnemy); }
unsigned char* F() { return bh::Pointer(bh::at::kMemberCurrent); }
bool Flip() { return bh::Half(); }
// bit `mask` of *p set or clear as a coin says
void Bit(unsigned char* p, unsigned char mask) { *p = static_cast<unsigned char>(Flip() ? *p | mask : *p & ~mask); }
// A down-counter's byte: its ends, 1 (the step to 0) and any.
unsigned char Counter() { return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 1, 2, 5, 0x1E, 0xFF, 0x80) : bh::Next()); }

// ===========================================================================
// What both runs share
// ===========================================================================

// Battle_ApplyDamage and Effect_ApplyResult fill the result record 0x904B60
// names (BattleObj_HitReceive points it at Field_State + 0x124 and zeroes its
// +4 / +6 words and +8 byte first): its +8 bits (0 and 1 the two popups, 4),
// the second word +6, and - Effect_ApplyResult - the first word +4, which
// BattleObj_HitReceive stores from Battle_ApplyDamage's answer itself. These
// stand-ins do the same with drawn values (0, positive, negative), louder
// than the harness's quiet ones, after noting what the caller left there (its
// zeroing is compared, not wiped).
unsigned short Change(U n) {
    switch (n % 4) {
    case 0: return 0;
    case 1: return static_cast<unsigned short>(1 + (n >> 8) % 999);
    case 2: return static_cast<unsigned short>(0x10000 - 1 - (n >> 8) % 999);
    default: return static_cast<unsigned short>(n >> 12);
    }
}
void FillResult(bool first_word) {
    unsigned char* const r = bh::Pointer(at::kResult);
    bh::Note(Word(r + 4), Word(r + 6), r[8]);
    const U n = bh::Noise();
    if (first_word) SetWord(r + 4, Change(n));
    SetWord(r + 6, Change(n >> 3));
    r[8] = static_cast<unsigned char>((n >> 20) & 0x13);
}
U DamageEffect(const U*, U answer) {
    FillResult(false);
    return (answer & 0xFFFF0000u) | Change(bh::Noise());
}
U ResultEffect(const U*, U answer) {
    FillResult(true);
    return answer;
}

// Beyond the standard sets: the group's own functions its others call directly
// (called by name in ours; kPhase logs the sprite they ran for, the loss test
// answers a flag); the callees of the wave's other groups, raw; the standard
// callees this group's callers push with a register's leftovers above the
// byte they read (each callee read, docs/battle_e3.md section 3).
const bh::Callee kCallees[] = {
    {"Sound_PlayEffectUnlessNone", 0x437450, KeyOf(&::Sound_PlayEffectUnlessNone), 1, {kU16}, bh::Answer::kGarbage, 0, 0},
    {"EnemyOp_CastDoneCheck", 0x437230, KeyOf(&::EnemyOp_CastDoneCheck), 0, {}, bh::Answer::kPhase, 0, 0},
    {"EnemyOp_EndAction", 0x4376A0, KeyOf(&::EnemyOp_EndAction), 0, {}, bh::Answer::kPhase, 0, 0},
    {"BattleObj_HitPose", 0x441510, KeyOf(&::BattleObj_HitPose), 0, {}, bh::Answer::kPhase, 0, 0},
    {"BattleParty_RecalcStats", 0x442310, KeyOf(&::BattleParty_RecalcStats), 0, {}, bh::Answer::kPhase, 0, 0},
    {"BattleParty_AllDown", 0x442420, KeyOf(&::BattleParty_AllDown), 0, {}, bh::Answer::kFlag, 0, 0},
    {"0x446770", at::kTurnVelocityC, at::kTurnVelocityC, 1, {kAll}, bh::Answer::kGarbage, 0, 0},
    {"0x4467C0", at::kTurnVelocity18, at::kTurnVelocity18, 1, {kAll}, bh::Answer::kGarbage, 0, 0},
    {"0x446810", at::kRollB9, at::kRollB9, 0, {}, bh::Answer::kFlag, 0, 0},
    {"0x44A910", at::kNameToText, at::kNameToText, 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"0x44AA90", at::kBannerByPair, at::kBannerByPair, 2, {kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"0x44FDE0", at::kPass44FDE0, at::kPass44FDE0, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x453300", at::kPass453300, at::kPass453300, 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"0x453EB0", at::kSecondPopup, at::kSecondPopup, 2, {kU16, kU8}, bh::Answer::kGarbage, 0, 0},
    {"Battle_LoadSoundByKey", ::bof3::addr::Battle_LoadSoundByKey, KeyOf(&::Battle_LoadSoundByKey), 2, {kU8, kU8},
     bh::Answer::kFlag, 0, 0},
    {"Battle_SetDamagePopup", ::bof3::addr::Battle_SetDamagePopup, KeyOf(&::Battle_SetDamagePopup), 2, {kU16, kU8},
     bh::Answer::kGarbage, 0, 0},
    {"Battle_ApplyDamage", ::bof3::addr::Battle_ApplyDamage, KeyOf(&::Battle_ApplyDamage), 2, {kAll, kAll}, bh::Answer::kGarbage, 0,
     0, {}, &DamageEffect},
    {"Effect_ApplyResult", ::bof3::addr::Effect_ApplyResult, KeyOf(&::Effect_ApplyResult), 0, {}, bh::Answer::kGarbage, 0, 0, {},
     &ResultEffect},
    {"Battle_ReturnQueuedItem", ::bof3::addr::Battle_ReturnQueuedItem, KeyOf(&::Battle_ReturnQueuedItem), 1, {kU8},
     bh::Answer::kGarbage, 0, 0},
};

// The cells beyond the engine frame the 49 read or write: the 256 first
// ability records (read in place by 0x904B80, which the seeds keep below 256:
// their flag bits drawn with the rest), and CharacterRecords past the engine
// region's first 0xB4 bytes (ten records, by a member's +0x148, which the
// seeds keep below 10).
const bh::Region kRegions[] = {
    {at::kAbilityFlags0, 256 * at::kAbilityStride},
    {0x903B24, at::kCharRecords + 10 * at::kCharStride - 0x903B24},
};

// The ability id: below 256 (the records in the region), its flag bits the
// random fill's.
void SeedAbility() { SetWord(Mem(at::kAbility), bh::Often() ? bh::Next() % 256 : bh::Next() % 16); }

// Which run the shadow name's run is limited to (BOF3X_BE3_RUN).
bool Wants(const char* unit) {
    const char* const only = std::getenv("BOF3X_BE3_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, unit) == 0;
}

// The clones of the run in progress, for Args and the seeds.
const bh::Clone* g_clones = nullptr;

// ===========================================================================
// EO: the enemy side
// ===========================================================================

constexpr bh::CallSite kCalls437050[] = {{0x0, 0x454810}, {0x36, 0x446E40}, {0xA6, 0x4358D0}, {0xC0, 0x4360C0}};
constexpr bh::CallSite kCalls437120[] = {{0x26, 0x437450}, {0x3D, 0x587740}, {0x54, 0x4360C0}};
constexpr bh::CallSite kCalls437180[] = {{0x12, 0x437230}};
constexpr bh::CallSite kCalls4371A0[] = {{0x4C, 0x4360C0}};
constexpr bh::CallSite kCalls437200[] = {{0x17, 0x4360C0}};
constexpr bh::CallSite kCalls437230[] = {{0x9, 0x4376A0}};
constexpr bh::CallSite kCalls437260[] = {{0x1F, 0x4467C0}, {0x57, 0x4358D0}, {0x6B, 0x454CC0}, {0xA5, 0x587740}, {0xAD, 0x436090}};
constexpr bh::CallSite kCalls437320[] = {{0x8B, 0x436090}};
constexpr bh::CallSite kCalls4373C0[] = {{0x9, 0x446650}, {0x2D, 0x454DC0}, {0x3B, 0x446FD0}, {0x4A, 0x44A810}, {0x4F, 0x589840}};
constexpr bh::CallSite kCalls437450[] = {{0xB, 0x587740}};
constexpr bh::CallSite kCalls4376A0[] = {{0x1D, 0x446FD0}};
constexpr bh::CallSite kCalls4376F0[] = {{0xE, 0x5B93D2}, {0x22, 0x435180}};

const bh::Clone kClonesEO[] = {
    Row("EnemyOp_CastDispatch", 0x437030, 0x12, nullptr, 0, BE3_FN(EnemyOp_CastDispatch), S::kDispatch, 0, 2, 3),
    Row("EnemyOp_CastStart", 0x437050, 0xC6, kCalls437050, BE3_N(kCalls437050), BE3_FN(EnemyOp_CastStart), S::kState, 0xFF),
    Row("EnemyOp_CastCue", 0x437120, 0x59, kCalls437120, BE3_N(kCalls437120), BE3_FN(EnemyOp_CastCue), S::kState, 0xFF),
    Row("EnemyOp_CastDoneDispatch", 0x437180, 0x17, kCalls437180, BE3_N(kCalls437180), BE3_FN(EnemyOp_CastDoneDispatch), S::kDispatch, 0, 2, 3),
    Row("EnemyOp_CastDoneCost", 0x4371A0, 0x5A, kCalls4371A0, BE3_N(kCalls4371A0), BE3_FN(EnemyOp_CastDoneCost), S::kState),
    Row("EnemyOp_CastDoneTickUnless", 0x437200, 0x25, kCalls437200, BE3_N(kCalls437200), BE3_FN(EnemyOp_CastDoneTickUnless), S::kState),
    Row("EnemyOp_CastDoneCheck", 0x437230, 0xF, kCalls437230, BE3_N(kCalls437230), BE3_FN(EnemyOp_CastDoneCheck), S::kCallee),
    Row("EnemyOp_LeaveDispatch", 0x437240, 0x12, nullptr, 0, BE3_FN(EnemyOp_LeaveDispatch), S::kDispatch, 0, 2, 3),
    Row("EnemyOp_LeaveStart", 0x437260, 0xBC, kCalls437260, BE3_N(kCalls437260), BE3_FN(EnemyOp_LeaveStart), S::kState),
    Row("EnemyOp_LeaveStep", 0x437320, 0x94, kCalls437320, BE3_N(kCalls437320), BE3_FN(EnemyOp_LeaveStep), S::kState),
    Row("EnemyOp_LeaveEnd", 0x4373C0, 0x54, kCalls4373C0, BE3_N(kCalls4373C0), BE3_FN(EnemyOp_LeaveEnd), S::kState),
    Row("Sound_PlayEffectUnlessNone", 0x437450, 0x12, kCalls437450, BE3_N(kCalls437450), BE3_FN(Sound_PlayEffectUnlessNone), S::kCallee),
    Row("EnemyOp_EndAction", 0x4376A0, 0x50, kCalls4376A0, BE3_N(kCalls4376A0), BE3_FN(EnemyOp_EndAction), S::kCallee),
    Row("EnemyOp_RollBit80Task", 0x4376F0, 0x2B, kCalls4376F0, BE3_N(kCalls4376F0), BE3_FN(EnemyOp_RollBit80Task), S::kCallee),
    Row("Fixed_HighRoundUp", 0x441090, 0x1E, nullptr, 0, BE3_FN(Fixed_HighRoundUp), S::kCallee, 0xFFFF),
};
enum : unsigned {
    kCastDispatch, kCastStart, kCastCue, kCastDoneDispatch, kCastDoneCost, kCastDoneTickUnless, kCastDoneCheck, kLeaveDispatch,
    kLeaveStart, kLeaveStep, kLeaveEnd, kPlayUnlessNone, kEndAction, kRollBit80, kHighRoundUp
};
static_assert(kHighRoundUp + 1 == BE3_COUNT(kClonesEO), "the enemy side's seeding indices");
const bh::DataTable kTablesEO[] = {{0x64B250, 3}, {0x64B25C, 3}, {0x64B268, 3}};

// The enemy side's cells: the ability id, the fight byte 0 a third of the time
// (the event battle's tests), the current enemy's +0x105 at 4 half the time,
// its data record +0xF0 inside the eight, the counters +9 / +0xA at their
// ends, the cost, the enemies left at 1 and 0, the round flags' bits 2 and 6,
// the enemy's +0x90 bit 3; a dispatcher's other state bytes inside its table.
void SeedEO(unsigned k) {
    const bh::Clone& c = g_clones[k];
    if (c.shape == S::kDispatch) bh::OtherStates(c.state_at, c.states);
    SeedAbility();
    unsigned char* const e = Enemy();
    e[0xF0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next() % 8);
    e[0x105] = static_cast<unsigned char>(Flip() ? 4 : BH_PICK(0, 3, 5, 0x84, 0xFF));
    Bit(e + 0x90, 8);
    if (bh::Next() % 3 == 0) B(at::kFight) = 0;
    Sprite_Current[9] = Counter();
    Sprite_Current[0xA] = Counter();
    Bit(Mem(at::kRoundFlags), 4);
    Bit(Mem(at::kRoundFlags), 0x40);
    if (k == kCastCue && Flip()) Sprite_Current[9] = 0;
    if (k == kLeaveEnd) B(at::kEnemiesLeft) = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 1, 2, 0, 0xFF) : bh::Next());
    if (k == kCastDoneCost) B(at::kCost) = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 0xFF, 0x80) : bh::Next());
}

// A cue word 0xFFFF half the time (the one 0x437450 skips), its upper half
// garbage; the fixed-point helper's value with a zero low word half the time
// and a sign below, at or above 0.
void ArgsEO(unsigned k, U* a) {
    if (k == kPlayUnlessNone) {
        a[0] = (a[0] & 0xFFFF0000u) | (Flip() ? 0xFFFFu : bh::Next() & 0xFFFF);
    } else if (k == kHighRoundUp) {
        if (Flip()) a[0] &= 0xFFFF0000u;
        a[1] = BH_PICK(0, 1, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFFu, 0xFFFF0000u, 0x10000);
        if (Flip()) a[1] = bh::Next();
    }
}

// What the enemy side reads again after a call that the standard disturbance
// does not move: the ability id (kept inside the region), the current enemy's
// +0x105 and +0x90, the fight byte, the enemies left, the cost, the round
// flags' bits 2 and 6. Drawn from the hash only.
void DisturbEO(U h) {
    const U v = h >> 16;
    switch ((h >> 8) % 7) {
    case 0: SetWord(Mem(at::kAbility), v % 256); break;
    case 1: Enemy()[0x105] = static_cast<unsigned char>(v & 1 ? 4 : v >> 1); break;
    case 2: Enemy()[0x90] ^= 8; break;
    case 3: B(at::kFight) = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 4: B(at::kEnemiesLeft) = static_cast<unsigned char>(v & 1 ? 1 : v >> 1); break;
    case 5: B(at::kCost) = static_cast<unsigned char>(v); break;
    default: B(at::kRoundFlags) ^= static_cast<unsigned char>(v & 1 ? 4 : 0x40); break;
    }
}
// The current enemy's data record index inside the eight after any
// disturbance (the harness's case 11 moves any byte of the enemy but its hooks
// and kind): a record past them is read out of the area's block on both sides.
void SettleEO() {
    unsigned char* const e = Enemy();
    e[0xF0] &= 7;
}

// ===========================================================================
// OBJ: the party objects
// ===========================================================================

constexpr bh::CallSite kCalls441A90[] = {{0xBA, 0x44B9F0},  {0xCE, 0x445A30},  {0x135, 0x591810}, {0x15C, 0x441510}, {0x181, 0x453DA0},
                                         {0x1A9, 0x453EB0}, {0x20E, 0x446A50}, {0x21C, 0x454DC0}, {0x224, 0x446A50}, {0x248, 0x587900},
                                         {0x263, 0x454380}, {0x268, 0x454410}, {0x28D, 0x454380}, {0x292, 0x454410}, {0x2A0, 0x441180}};
constexpr bh::CallSite kCalls441D50[] = {{0x0, 0x589410}, {0x24, 0x4412B0}};
constexpr bh::CallSite kCalls441D80[] = {{0x0, 0x4411B0}, {0x5B, 0x446810}, {0x8B, 0x446810}, {0xD1, 0x446FD0}, {0xE5, 0x442420}};
constexpr bh::CallSite kCalls441ED0[] = {{0x47, 0x446770}, {0x56, 0x4411B0}, {0x60, 0x587900}};
constexpr bh::CallSite kCalls441F50[] = {{0x4A, 0x4411B0}};
constexpr bh::CallSite kCalls441FA0[] = {{0x28, 0x453DA0}, {0x3F, 0x435180}, {0xA6, 0x4411B0}};
constexpr bh::CallSite kCalls442050[] = {{0x1B, 0x4412B0}};
constexpr bh::CallSite kCalls4420A0[] = {{0xA, 0x446EA0},   {0x19, 0x446650},  {0x4C, 0x441180},  {0x85, 0x441180},
                                         {0xC3, 0x441180},  {0xFD, 0x441180},  {0x138, 0x441180}, {0x1BC, 0x453300},
                                         {0x1C4, 0x442420}, {0x212, 0x441180}, {0x21D, 0x446FD0}, {0x268, 0x441180}};
constexpr bh::CallSite kCalls442310[] = {{0x20, 0x590660}, {0xA9, 0x453C00}, {0xD9, 0x44FDE0}, {0xF2, 0x453300}};
constexpr bh::CallSite kCalls4424A0[] = {{0x5E, 0x435180}, {0x9C, 0x5891F0}, {0xA6, 0x587900}, {0xB7, 0x444310},
                                         {0xC5, 0x44A910}, {0xD9, 0x44AA90}, {0xEB, 0x441180}};
constexpr bh::CallSite kCalls4425A0[] = {{0x9, 0x446FD0}};
constexpr bh::CallSite kCalls442600[] = {{0x48, 0x442310}, {0x9B, 0x435180}, {0xD8, 0x5891F0}, {0xE2, 0x587900},
                                         {0xF3, 0x444310}, {0x10A, 0x497740}, {0x118, 0x44A650}, {0x12A, 0x441180}};
constexpr bh::CallSite kCalls442760[] = {{0x66, 0x435180}};
constexpr bh::CallSite kCalls442800[] = {{0x36, 0x446FD0}};
constexpr bh::CallSite kCalls442890[] = {{0x37, 0x446A50}, {0x48, 0x446A50}, {0x54, 0x587900},
                                         {0xAB, 0x453DA0}, {0xD3, 0x453EB0}, {0xDB, 0x441180}};
constexpr bh::CallSite kCalls442980[] = {{0x0, 0x441180}, {0x35, 0x446FD0}};
constexpr bh::CallSite kCalls442C40[] = {{0x0, 0x441180}};
constexpr bh::CallSite kCalls442C90[] = {{0x4, 0x435180}};
constexpr bh::CallSite kCalls442D30[] = {{0x0, 0x441510}, {0x14, 0x441180}, {0x1D, 0x4411B0}};
constexpr bh::CallSite kCalls442E60[] = {{0x9, 0x435180}};
constexpr bh::CallSite kCalls442F10[] = {{0x19, 0x452BF0}, {0x26, 0x5B93D2}, {0x52, 0x446A50}, {0x5C, 0x446A50}};
constexpr bh::CallSite kCalls442F80[] = {{0x11, 0x442DD0}};
constexpr bh::CallSite kCalls441510[] = {{0x1B, 0x589330}, {0x30, 0x589330}};

const bh::Clone kClonesOBJ[] = {
    Row("BattleObj_StateHit", 0x441A10, 0x12, nullptr, 0, BE3_FN(BattleObj_StateHit), S::kMember, 0, 2, 6),
    Row("BattleObj_HitEnter", 0x441A30, 0x3E, nullptr, 0, BE3_FN(BattleObj_HitEnter), S::kMember),
    Row("BattleObj_HitReceiveDispatch", 0x441A70, 0x12, nullptr, 0, BE3_FN(BattleObj_HitReceiveDispatch), S::kMember, 0, 3, 3),
    Row("BattleObj_HitReceive", 0x441A90, 0x2B1, kCalls441A90, BE3_N(kCalls441A90), BE3_FN(BattleObj_HitReceive), S::kMember),
    Row("BattleObj_HitWaitPose", 0x441D50, 0x2A, kCalls441D50, BE3_N(kCalls441D50), BE3_FN(BattleObj_HitWaitPose), S::kMember),
    Row("BattleObj_HitEnd", 0x441D80, 0x127, kCalls441D80, BE3_N(kCalls441D80), BE3_FN(BattleObj_HitEnd), S::kMember),
    Row("BattleObj_HitStepDispatch", 0x441EB0, 0x12, nullptr, 0, BE3_FN(BattleObj_HitStepDispatch), S::kMember, 0, 3, 5),
    Row("BattleObj_HitStepStart", 0x441ED0, 0x71, kCalls441ED0, BE3_N(kCalls441ED0), BE3_FN(BattleObj_HitStepStart), S::kMember),
    Row("BattleObj_HitStepOut", 0x441F50, 0x4F, kCalls441F50, BE3_N(kCalls441F50), BE3_FN(BattleObj_HitStepOut), S::kMember),
    Row("BattleObj_HitStepBack", 0x441FA0, 0xAD, kCalls441FA0, BE3_N(kCalls441FA0), BE3_FN(BattleObj_HitStepBack), S::kMember),
    Row("BattleObj_HitStepPose", 0x442050, 0x21, kCalls442050, BE3_N(kCalls442050), BE3_FN(BattleObj_HitStepPose), S::kMember),
    Row("BattleObj_FallDispatch", 0x442080, 0x12, nullptr, 0, BE3_FN(BattleObj_FallDispatch), S::kMember, 0, 3, 7),
    Row("BattleObj_Fall", 0x4420A0, 0x26F, kCalls4420A0, BE3_N(kCalls4420A0), BE3_FN(BattleObj_Fall), S::kMember),
    Row("BattleObj_Revive", 0x4424A0, 0xF2, kCalls4424A0, BE3_N(kCalls4424A0), BE3_FN(BattleObj_Revive), S::kMember),
    Row("BattleObj_ReviveEnd", 0x4425A0, 0x58, kCalls4425A0, BE3_N(kCalls4425A0), BE3_FN(BattleObj_ReviveEnd), S::kMember),
    Row("BattleObj_ReviveByEquip", 0x442600, 0x131, kCalls442600, BE3_N(kCalls442600), BE3_FN(BattleObj_ReviveByEquip), S::kMember),
    Row("BattleObj_FallTaskDispatch", 0x442740, 0x12, nullptr, 0, BE3_FN(BattleObj_FallTaskDispatch), S::kMember, 0, 4, 2),
    Row("BattleObj_FallTaskStart", 0x442760, 0x99, kCalls442760, BE3_N(kCalls442760), BE3_FN(BattleObj_FallTaskStart), S::kMember),
    Row("BattleObj_FallTaskWait", 0x442800, 0x66, kCalls442800, BE3_N(kCalls442800), BE3_FN(BattleObj_FallTaskWait), S::kMember),
    Row("BattleObj_HpDispatch", 0x442870, 0x12, nullptr, 0, BE3_FN(BattleObj_HpDispatch), S::kMember, 0, 3, 3),
    Row("BattleObj_HpApply", 0x442890, 0xE9, kCalls442890, BE3_N(kCalls442890), BE3_FN(BattleObj_HpApply), S::kMember),
    Row("BattleObj_HpEnd", 0x442980, 0x5B, kCalls442980, BE3_N(kCalls442980), BE3_FN(BattleObj_HpEnd), S::kMember),
    Row("BattleObj_CastDoneScript", 0x442C40, 0x5, kCalls442C40, BE3_N(kCalls442C40), BE3_FN(BattleObj_CastDoneScript), S::kMember),
    Row("BattleObj_State10", 0x442C70, 0x12, nullptr, 0, BE3_FN(BattleObj_State10), S::kMember, 0, 2, 2),
    Row("BattleObj_State10Task", 0x442C90, 0x78, kCalls442C90, BE3_N(kCalls442C90), BE3_FN(BattleObj_State10Task), S::kMember),
    Row("BattleObj_State10End", 0x442D10, 0x14, nullptr, 0, BE3_FN(BattleObj_State10End), S::kMember),
    Row("BattleObj_State11", 0x442D30, 0x23, kCalls442D30, BE3_N(kCalls442D30), BE3_FN(BattleObj_State11), S::kMember),
    Row("BattleObj_StateSpecial", 0x442E40, 0x12, nullptr, 0, BE3_FN(BattleObj_StateSpecial), S::kMember, 0, 2, 3),
    Row("BattleObj_SpecialStart", 0x442E60, 0xB0, kCalls442E60, BE3_N(kCalls442E60), BE3_FN(BattleObj_SpecialStart), S::kMember),
    Row("BattleObj_SpecialCue", 0x442F10, 0x6D, kCalls442F10, BE3_N(kCalls442F10), BE3_FN(BattleObj_SpecialCue), S::kMember),
    Row("BattleObj_SpecialWait", 0x442F80, 0x17, kCalls442F80, BE3_N(kCalls442F80), BE3_FN(BattleObj_SpecialWait), S::kMember),
    Row("BattleObj_HitPose", 0x441510, 0x37, kCalls441510, BE3_N(kCalls441510), BE3_FN(BattleObj_HitPose), S::kHelper),
    Row("BattleParty_RecalcStats", 0x442310, 0x10E, kCalls442310, BE3_N(kCalls442310), BE3_FN(BattleParty_RecalcStats), S::kHelper),
    Row("BattleParty_AllDown", 0x442420, 0x7F, nullptr, 0, BE3_FN(BattleParty_AllDown), S::kHelper, 0xFF),
};
enum : unsigned {
    kStateHit, kHitEnter, kHitReceiveDispatch, kHitReceive, kHitWaitPose, kHitEnd, kHitStepDispatch, kHitStepStart, kHitStepOut,
    kHitStepBack, kHitStepPose, kFallDispatch, kFall, kRevive, kReviveEnd, kReviveByEquip, kFallTaskDispatch, kFallTaskStart,
    kFallTaskWait, kHpDispatch, kHpApply, kHpEnd, kCastDoneScript, kState10, kState10Task, kState10End, kState11, kStateSpecial,
    kSpecialStart, kSpecialCue, kSpecialWait, kHitPose, kRecalcStats, kAllDown
};
static_assert(kAllDown + 1 == BE3_COUNT(kClonesOBJ), "the party side's seeding indices");
const bh::DataTable kTablesOBJ[] = {{0x64E07C, 6}, {0x64E094, 3}, {0x64E0A0, 5}, {0x64E0B4, 7},
                                    {0x64E0D0, 2}, {0x64E0D8, 3}, {0x64E12C, 2}, {0x64E13C, 3}};

// A member's field as the seeds set it: in the member Field_State points at,
// and - two times in three - in the other two as well, so that a stand-in
// moving Field_State to another member still reaches the branch under test.
template <typename Fn> void Members(Fn&& set) {
    unsigned char* const f = F();
    set(f);
    if (bh::Often())
        for (unsigned i = 0; i < at::kPartyMax; ++i)
            if (bh::PartyOf(static_cast<unsigned char>(i)) != f) set(bh::PartyOf(static_cast<unsigned char>(i)));
}
void MemberByte(unsigned off, unsigned char value) {
    Members([&](unsigned char* m) { m[off] = value; });
}
void MemberBit(unsigned off, unsigned char mask) {
    const bool on = Flip();
    Members([&](unsigned char* m) { m[off] = static_cast<unsigned char>(on ? m[off] | mask : m[off] & ~mask); });
}
void MemberWord(unsigned off, unsigned value) {
    Members([&](unsigned char* m) { SetWord(m + off, value); });
}

// A signed change against the HP it meets: equal, one either side, zero,
// negative, the s16 ends; else garbage.
void SeedChange() {
    const unsigned hp = bh::Often() ? BH_PICK(0, 1, 2, 100, 999, 0x7FFF, 0x8000, 0xFFFF) : bh::Next() & 0xFFFF;
    const unsigned pick = bh::Next() % 9;
    const unsigned change = pick == 0 ? hp : pick == 1 ? hp + 1 : pick == 2 ? hp - 1 : pick == 3 ? 0 : pick == 4 ? 0xFFFF - bh::Next() % 300
                          : pick == 5 ? 0x7FFF : pick == 6 ? 0x8000 : bh::Next() & 0xFFFF;
    MemberWord(0x98, hp);
    MemberWord(0x128, change & 0xFFFF);
    if (Flip()) MemberWord(0x12A, Flip() ? 0xFFFF - bh::Next() % 50 : 0);
    // the max HP against the result: one below it, at it, one above, or any
    const unsigned result = hp - change;
    const unsigned maxhp = bh::Often() ? result - 1 + bh::Next() % 3 : bh::Next();
    MemberWord(0xA0, maxhp & 0xFFFF);
}

// The party side's cells: every member's character +0x148 inside the ten
// records, the party count 0..3, the ability id; each function's branch
// boundaries on the member Field_State points at (and often the other two).
void SeedOBJ(unsigned k) {
    const bh::Clone& c = g_clones[k];
    for (unsigned i = 0; i < at::kPartyMax; ++i) bh::PartyOf(static_cast<unsigned char>(i))[0x148] = static_cast<unsigned char>(bh::Next() % 10);
    B(at::kPartyCount) = static_cast<unsigned char>(bh::Often() ? 3 : bh::Next() % 4);
    SeedAbility();
    if (c.states) {
        // the dispatched byte is the harness's; the others inside the state-6 tables
        for (unsigned b = 2; b <= 4; ++b)
            if (b != c.state_at) Sprite_Current[b] = static_cast<unsigned char>(bh::Next() % 3);
    }
    Sprite_Current[9] = Counter();
    Sprite_Current[0xA] = Counter();
    switch (k) {
    case kHitReceive:
        B(at::kActKind) = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 1, 4, 4, 5, 5, 0, 2, 3, 6) : bh::Next());
        MemberBit(0x12C, 1);
        MemberBit(0x12C, 2);
        SeedChange();
        if (bh::Next() % 3 == 0) {
            // the fall: HP 0 under a positive change
            MemberWord(0x98, 0);
            MemberWord(0x128, 1 + bh::Next() % 500);
        }
        MemberBit(0x134, 2);
        MemberBit(0x130, 1);
        MemberBit(0x131, 2);
        MemberBit(0x91, 8);
        B(at::kEvadeBoost) = static_cast<unsigned char>(bh::Often() ? BH_PICK(0x4A, 0x4B, 0x4C, 0, 0x64, 0xFF) : bh::Next());
        Bit(Mem(at::kRoundFlags + 1), 0x20);
        break;
    case kHitWaitPose:
    case kHitStepPose:
        MemberBit(0x90, 4);
        break;
    case kHitEnd:
        if (Flip()) Sprite_Current[2] = 3;
        MemberBit(0x91, 0x40);
        Bit(Mem(at::kRoundFlags), 0x40);
        B(at::kTarget) = static_cast<unsigned char>(bh::Often() ? bh::Next() % 11 : bh::Next());
        B(at::kActKind) = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 4, 4, 0, 5) : bh::Next());
        break;
    case kHitStepStart:
    case kHpEnd:
        MemberBit(0x91, 0x40);
        break;
    case kHitStepBack:
        if (Flip()) Sprite_Current[0xA] = 0;
        MemberBit(0x12C, 0x10);
        break;
    case kFall: {
        const unsigned path = bh::Next() % 6;
        MemberBit(0x130, 4);
        if (path != 0) MemberByte(0x130, static_cast<unsigned char>(F()[0x130] & ~4));
        MemberBit(0x134, 2);
        if (path > 1) MemberByte(0x134, static_cast<unsigned char>(F()[0x134] & ~2));
        MemberByte(0x96, path == 2 || bh::Next() % 8 == 0 ? 0x18 : 0x17);
        MemberByte(0x97, path == 3 || bh::Next() % 8 == 0 ? 0x18 : 0x19);
        MemberByte(0x95, path == 4 || bh::Next() % 8 == 0 ? 0x43 : 0x42);
        MemberBit(0x134, 1);
        B(at::kMembersUp) = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 1, 2, 3, 0) : bh::Next());
        Bit(Mem(at::kRoundFlags + 1), 0x40);
        if (Flip()) B(at::kFormActor) = Sprite_Current[5];
        break;
    }
    case kRevive:
    case kReviveByEquip:
        if (bh::Often()) Sprite_Current[9] = 1;
        if (k == kReviveByEquip) Sprite_Current[3] = static_cast<unsigned char>(BH_PICK(3, 5, 6, 3, 5, 6, 4, 0));
        B(at::kKindsSeen) = static_cast<unsigned char>(Flip() ? 0 : bh::Next());
        break;
    case kFallTaskStart:
        for (unsigned i = 0; i < at::kPartyMax; ++i) {
            unsigned char* const m = bh::PartyOf(static_cast<unsigned char>(i));
            m[0x131] = static_cast<unsigned char>(bh::Next() % 4 == 0 ? m[0x131] | 0x20 : m[0x131] & ~0x20);
        }
        MemberByte(0x89, static_cast<unsigned char>(Flip() ? 4 : bh::Next()));
        break;
    case kFallTaskWait:
        MemberBit(0x131, 0x20);
        break;
    case kHpApply:
        SeedChange();
        MemberBit(0x12C, 1);
        MemberBit(0x12C, 2);
        break;
    case kState11:
        MemberBit(0x130, 0x10);
        MemberBit(0x130, 0x20);
        break;
    case kSpecialCue:
        if (bh::Often()) Sprite_Current[9] = 1;
        MemberByte(0xBA, static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 50, 99, 100, 0xFF) : bh::Next()));
        break;
    case kSpecialWait:
        Bit(Sprite_Current, 0x40);
        break;
    case kHitPose:
        MemberBit(0x91, 8);
        break;
    case kAllDown:
        for (unsigned i = 0; i < at::kPartyMax; ++i) {
            unsigned char* const m = bh::PartyOf(static_cast<unsigned char>(i));
            if (bh::Often()) m[0] |= 1;
            SetWord(m + 0x90, bh::Often() ? BH_PICK(0x4000, 0x0004, 0x4004, 0x4000, 0, 0x8000) : bh::Next() & 0xFFFF);
            if (bh::Often()) m[0x130] &= ~4;
            if (bh::Often()) m[0x134] &= ~2;
            m[0x95] = static_cast<unsigned char>(bh::Next() % 6 == 0 ? 0x43 : 0x42);
            m[0x96] = static_cast<unsigned char>(bh::Next() % 6 == 0 ? 0x18 : 0x19);
            m[0x97] = static_cast<unsigned char>(bh::Next() % 6 == 0 ? 0x18 : 0x17);
        }
        break;
    default:
        break;
    }
}

// What the party side reads again after a call that the standard disturbance
// does not move: the member's (Field_State's) flags +0x90 / +0x91 / +0x12C /
// +0x130 / +0x131 / +0x134, its change +0x128 / +0x12A and HP +0x98, its
// equipment bytes +0x95..+0x97, the action's kind, the round flags, the
// message kinds 0x904AE9, the members up, the form actor, the evade byte, the
// ability id (inside the region), the party count (0..3). Drawn from the hash
// only.
void DisturbOBJ(U h) {
    const U v = h >> 16;
    unsigned char* const f = v & 0x8000 ? F() : bh::PartyOf(static_cast<unsigned char>(v % 3));
    switch ((h >> 8) % 12) {
    case 0: f[0x90 + (v & 1)] ^= static_cast<unsigned char>(1u << ((v >> 1) % 8)); break;
    case 1: f[0x12C] ^= static_cast<unsigned char>(1u << ((v >> 1) % 5)); break;
    case 2: f[0x130 + (v & 1)] ^= static_cast<unsigned char>(1u << ((v >> 1) % 8)); break;
    case 3: f[0x134] ^= static_cast<unsigned char>(1u << ((v >> 1) % 3)); break;
    case 4: f[0x128 + (v & 3)] = static_cast<unsigned char>(v >> 2); break;
    case 5: f[0x95 + (v % 3)] = static_cast<unsigned char>(v & 0x10 ? 0x18 : 0x43); break;
    case 6: B(at::kActKind) = static_cast<unsigned char>(v & 1 ? 1 + (v >> 1) % 5 : v >> 1); break;
    case 7: B(at::kRoundFlags + (v & 1)) ^= static_cast<unsigned char>(1u << ((v >> 1) % 8)); break;
    case 8: B(at::kKindsSeen) = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 9: B(v & 1 ? at::kMembersUp : at::kFormActor) = static_cast<unsigned char>(v & 2 ? (v >> 2) % 3 : v >> 2); break;
    case 10: SetWord(Mem(at::kAbility), v % 256); break;
    default: B(at::kPartyCount) = static_cast<unsigned char>(v % 4); break;
    }
}

// The args of a kHelper: none of the three party helpers takes any.
void ArgsOBJ(unsigned, U*) {}

void RunGroup(const char* unit, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables,
              void (*seed)(unsigned), void (*disturb)(U), void (*settle)(), void (*args)(unsigned, U*)) {
    if (!Wants(unit)) return;
    g_clones = clones;
    bh::Group g{"battle_e3", clones, n, kCallees, BE3_COUNT(kCallees), tables, n_tables, kRegions, BE3_COUNT(kRegions), seed, disturb, 6000};
    g.settle = settle;
    g.args = args;
    g.engine = true;
    bh::Run(g);
    g_clones = nullptr;
}

}  // namespace

void SelfTest() {
    RunGroup("EO", kClonesEO, BE3_COUNT(kClonesEO), kTablesEO, BE3_COUNT(kTablesEO), &SeedEO, &DisturbEO, &SettleEO, &ArgsEO);
    RunGroup("OBJ", kClonesOBJ, BE3_COUNT(kClonesOBJ), kTablesOBJ, BE3_COUNT(kTablesOBJ), &SeedOBJ, &DisturbOBJ, nullptr, &ArgsOBJ);
}

}  // namespace battle_e3

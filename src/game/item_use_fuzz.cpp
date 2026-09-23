// BOF3X_SHADOW=item_use: a differential fuzz of the field menu's item effects
// and the functions next to them, once at start-up. docs/item-use.md
// section 5.
//
// Fifty-nine byte-copies, every call out re-aimed at a recording stand-in,
// ItemUse_Handlers' 34 entries swapped for recorders and four entries of
// Area_Descriptors pointed at descriptors of our own whose +0x34 table holds
// 128 recorders. One round: one function, random bytes in every region any
// of them touches, then that function's branch boundaries seeded; theirs,
// then from the same state ours; the regions, the answer (at the width the
// original defines) and the stand-ins' log compared.
//
// The stand-ins are as loud as the real callees where the caller reads after
// the call: a heal or a status clear rewrites the record it names (the cure
// handler reads the status after its heal), the recompute rewrites the max HP
// (the scale handler reads it again), Party_Count answers anew each time (the
// party walks ask after every member), Flags_Test and Inventory_Add move the
// Faerie Tiara's cells, and every area choice moves the message box's words.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/item_use_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace item_use {
namespace {

using move_script::At;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x3C6EF372u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }

constexpr unsigned kLog = 64;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;   // the stand-ins' own stream: the same on both passes

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}

unsigned char* RecordOf(unsigned id, unsigned battle) {
    const unsigned i = id & 0xFF;
    return (battle & 0xFF) == 0 ? At(at::kCharRecords + i * at::kCharStride) : At(at::kWorkRecords + i * at::kWorkStride);
}

// Every cell below is one some function reads again, or stores, after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    switch ((h >> 4) % 14) {
    case 0: SetWord(At(at::kMessage), v % 3 == 0 ? 0xFFFF : h >> 16); break;
    case 1: At(at::kSubState)[0] = static_cast<unsigned char>(v); break;
    case 2: At(at::kWindow1State)[0] = static_cast<unsigned char>(v); break;
    case 3: At(at::kWindow0State)[0] = static_cast<unsigned char>(v); break;
    case 4: At(at::kColorHigh)[0] = static_cast<unsigned char>(v); break;
    case 5: Game_Step = static_cast<unsigned short>(h >> 16); break;
    case 6: At(at::kTiaraCount)[0] = static_cast<unsigned char>(v); break;
    case 7: At(at::kMenuState)[0] = static_cast<unsigned char>(v); break;
    case 8: At(at::kLeaderXZ + v % 8)[0] = static_cast<unsigned char>(h >> 20); break;
    case 9: Game_AreaNumber = static_cast<unsigned short>(v % 4); break;
    case 10: At(at::kPartyLists + v % 4)[0] = static_cast<unsigned char>((h >> 20) % 0x18); break;
    case 11: At(at::kSysCounter)[0] = static_cast<unsigned char>(v); break;
    default: {
        // a record's status, HP or max: any of the 16 records
        unsigned char* const r = RecordOf(v % 8, (h >> 20) & 1);
        static const unsigned kFields[] = {at::kStatus, at::kHp, at::kMaxHp, at::kAp, at::kMaxAp, at::kHpScale};
        r[kFields[(h >> 24) % 6] + ((h >> 28) & 1)] = static_cast<unsigned char>(h >> 8);
        break;
    }
    }
}

// --- the stand-ins ---------------------------------------------------------

// A heal rewrites the record it names (HP, and sometimes the status the cure
// handler reads after it) and answers anything in al.
unsigned char __cdecl StubHealHp(unsigned id, unsigned amount, unsigned battle) {
    Record(1, id, amount, battle);
    unsigned char* const r = RecordOf(id, battle);
    if (Hash() % 2) SetWord(r + at::kHp, Hash() >> 8);
    if (Hash() % 3 == 0) r[at::kStatus] = static_cast<unsigned char>(r[at::kStatus] ^ 0xA0);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 8);
}
unsigned char __cdecl StubHealAp(unsigned id, unsigned amount, unsigned battle) {
    Record(2, id, amount, battle);
    unsigned char* const r = RecordOf(id, battle);
    if (Hash() % 2) SetWord(r + at::kAp, Hash() >> 8);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 == 0 ? 0 : h >> 8);
}
unsigned char __cdecl StubClear(unsigned id, unsigned mask, unsigned battle) {
    Record(3, id, mask, battle);
    unsigned char* const r = RecordOf(id, battle);
    if (Hash() % 2) SetWord(r + at::kStatus, Word(r + at::kStatus) & ~mask);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 == 0 ? 0 : h >> 8);
}
unsigned char __cdecl StubCap999(unsigned short* stat, unsigned amount) {
    Record(4, Address(stat), amount);
    if (Hash() % 2) SetWord(reinterpret_cast<unsigned char*>(stat), Hash() >> 4);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 == 0 ? 0 : h >> 8);
}
unsigned char __cdecl StubCap99(unsigned char* stat, unsigned amount) {
    Record(5, Address(stat), amount);
    if (Hash() % 2) *stat = static_cast<unsigned char>(Hash() >> 4);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 == 0 ? 0 : h >> 8);
}
// Stat_AddClamped answers a word; the callers keep al only, so a quarter of
// the answers are non-zero with a zero low byte.
short __cdecl StubClamped(unsigned short* stat, unsigned delta) {
    Record(6, Address(stat), delta);
    if (Hash() % 2) SetWord(reinterpret_cast<unsigned char*>(stat), Hash() >> 4);
    Disturb();
    const std::uint32_t h = Hash();
    switch (h % 4) {
    case 0: return 0;
    case 1: return static_cast<short>((h >> 8) & 0xFF00);   // low byte 0
    default: return static_cast<short>(h >> 8);
    }
}
// Party_Count: its low byte 0..4 (the walks compare bytes), stale bits above
// half the time.
int __cdecl StubPartyCount(unsigned slot) {
    Record(7, slot);
    Disturb();
    const std::uint32_t h = Hash();
    const std::uint32_t count = (h >> 3) % 5;
    return static_cast<int>(h % 2 ? count : (h & 0xFFFFFF00u) | count);
}
// Rand: around the 30 % boundary, and a few the CRT's rand never answers
// (negative) so that signed and unsigned division tell apart.
int __cdecl StubRand() {
    Record(8);
    Disturb();
    static const int kValues[] = {29, 30, 31, 129, 130, 0, 99, 100, -1, -71, -70, -100, 0x7FFF, 32729, 32730};
    const std::uint32_t h = Hash();
    return h % 3 == 0 ? static_cast<int>(h >> 1) & 0x7FFF : kValues[(h >> 4) % 15];
}
// Char_RecalcStats rewrites the effective block the scale handler reads again.
void __cdecl StubRecalc(unsigned char* record) {
    Record(9, Address(record));
    if (Hash() % 4) SetWord(record + at::kMaxHp, Hash() >> 8);
    if (Hash() % 3 == 0) SetWord(record + at::kHp, Hash() >> 12);
    Disturb();
}
unsigned char __cdecl StubInventoryAdd(unsigned category, unsigned item, unsigned count, unsigned fourth) {
    Record(10, category, item, count, fourth);
    Disturb();
    return static_cast<unsigned char>(Hash());
}
// Flags_Test: set about one time in five, so that a walk runs to its end.
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned index) {
    Record(11, Address(bits), index);
    Disturb();
    return static_cast<unsigned char>(Hash() % 5 == 0 ? 1 : 0);
}
void __cdecl StubSystemChoice() {
    Record(12);
    Disturb();
}

template <unsigned N> unsigned char __cdecl StubItem(unsigned id, unsigned battle) {
    Record(100 + N, id, battle);
    Disturb();
    return static_cast<unsigned char>(Hash() >> 8);
}
template <unsigned N> void __cdecl StubArea() {
    Record(200 + N);
    Disturb();
}
template <std::size_t... I> std::array<Handler, sizeof...(I)> MakeItemStubs(std::index_sequence<I...>) {
    return {&StubItem<I>...};
}
template <std::size_t... I> std::array<Choice, sizeof...(I)> MakeAreaStubs(std::index_sequence<I...>) {
    return {&StubArea<I>...};
}

const Callees kStubs = {
    StubHealHp, StubHealAp, StubClear, StubCap999, StubCap99, StubClamped,
    StubPartyCount, StubRand, StubRecalc, StubInventoryAdd, StubFlagsTest, StubSystemChoice,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x590CE0: return f(&StubHealHp);
    case 0x590D70: return f(&StubHealAp);
    case 0x590F60: return f(&StubClear);
    case 0x590DE0: return f(&StubCap999);
    case 0x590E10: return f(&StubCap99);
    case 0x590E30: return f(&StubClamped);
    case 0x531BB0: return f(&StubPartyCount);
    case 0x5B93D2: return f(&StubRand);
    case 0x590660: return f(&StubRecalc);
    case 0x590BB0: return f(&StubInventoryAdd);
    case 0x57C140: return f(&StubFlagsTest);
    case 0x498A30: return f(&StubSystemChoice);
    default: bof3::Fatal("item_use: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the fifty-nine copies (capstone, 2026-09-22: every jump internal, no
// jump table; the calls below are every call that leaves) --------------------

struct Call { std::uint32_t offset, target; };
// ret: the answer's width the original defines - 0 none, 1 al, 2 ax, 4 eax.
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; unsigned ret; const void* ours; };

constexpr Call kHealHpAt0C[] = {{0xC, 0x590CE0}};
constexpr Call kHealApAt0C[] = {{0xC, 0x590D70}};
constexpr Call kClearAt0F[] = {{0xF, 0x590F60}};
constexpr Call kClearAt0C[] = {{0xC, 0x590F60}};
constexpr Call kPartyHeal[] = {{0x11, 0x531BB0}, {0x45, 0x590CE0}, {0x5C, 0x531BB0}};
constexpr Call kPartyHeal240[] = {{0x11, 0x531BB0}, {0x48, 0x590CE0}, {0x5F, 0x531BB0}};
constexpr Call kPartyCure[] = {{0x11, 0x531BB0}, {0x48, 0x590F60}, {0x5F, 0x531BB0}};
constexpr Call kHealCure[] = {{0xE, 0x590CE0}, {0x54, 0x5B93D2}, {0x71, 0x590F60}};
constexpr Call kRevive[] = {{0x45, 0x590F60}};
constexpr Call kRestore[] = {{0xA, 0x531BB0}, {0x42, 0x590F60}, {0x4D, 0x590CE0}, {0x60, 0x531BB0}};
constexpr Call kCap999[] = {{0x3D, 0x590DE0}, {0x4A, 0x590DE0}};
constexpr Call kClamped[] = {{0x3D, 0x590E30}, {0x4A, 0x590E30}};
constexpr Call kCap99[] = {{0x3D, 0x590E10}, {0x4A, 0x590E10}};
constexpr Call kScale[] = {{0x50, 0x590660}};
constexpr Call kTiara[] = {{0x9, 0x590BB0}, {0x31, 0x57C140}};
constexpr Call kCommit[] = {{0x2C, 0x498A30}};
constexpr Call kHealHpClear[] = {{0x7C, 0x590F60}};

#define IU_C(name, base, size, calls, ret) {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret, reinterpret_cast<const void*>(&::name)}
#define IU_P(name, base, size, ret) {#name, base, size, nullptr, 0, ret, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    IU_P(ItemUse_Dispatch, 0x497680, 0x44, 1),
    IU_P(ItemUse_None, 0x496CC0, 0x3, 1),
    IU_C(ItemUse_HealHp20, 0x496CD0, 0x1E, kHealHpAt0C, 1),
    IU_C(ItemUse_HealHp40, 0x496CF0, 0x1E, kHealHpAt0C, 1),
    IU_C(ItemUse_HealHp100, 0x496D10, 0x1E, kHealHpAt0C, 1),
    IU_C(ItemUse_HealHpFull, 0x496D30, 0x1E, kHealHpAt0C, 1),
    IU_C(ItemUse_PartyHealHp100, 0x496D50, 0x7B, kPartyHeal, 1),
    IU_C(ItemUse_HealAp20, 0x496DD0, 0x1E, kHealApAt0C, 1),
    IU_C(ItemUse_HealAp100, 0x496DF0, 0x1E, kHealApAt0C, 1),
    IU_C(ItemUse_HealHp5Cure, 0x496E10, 0x88, kHealCure, 1),
    IU_C(ItemUse_Cure80, 0x496EA0, 0x21, kClearAt0F, 1),
    IU_C(ItemUse_Cure08, 0x496ED0, 0x1E, kClearAt0C, 1),
    IU_C(ItemUse_Cure100, 0x496EF0, 0x21, kClearAt0F, 1),
    IU_C(ItemUse_CureA0, 0x496F20, 0x21, kClearAt0F, 1),
    IU_C(ItemUse_Revive, 0x496F50, 0x58, kRevive, 1),
    IU_C(ItemUse_PartyRestore, 0x496FB0, 0x81, kRestore, 1),
    IU_C(ItemUse_MaxHpUp, 0x497040, 0x62, kCap999, 1),
    IU_C(ItemUse_MaxApUp, 0x4970B0, 0x62, kCap999, 1),
    IU_C(ItemUse_AtkUp, 0x497120, 0x62, kClamped, 1),
    IU_C(ItemUse_DefUp, 0x497190, 0x62, kClamped, 1),
    IU_C(ItemUse_AgiUp, 0x497200, 0x62, kClamped, 1),
    IU_C(ItemUse_IntUp, 0x497270, 0x62, kClamped, 1),
    IU_C(ItemUse_Stat2EUp, 0x4972E0, 0x62, kCap99, 1),
    IU_C(ItemUse_HpScaleUp, 0x497350, 0x75, kScale, 1),
    IU_C(ItemUse_HealHp1, 0x4973D0, 0x1E, kHealHpAt0C, 1),
    IU_C(ItemUse_HealHp80, 0x4973F0, 0x1E, kHealHpAt0C, 1),
    IU_C(ItemUse_PartyHealHp80, 0x497410, 0x7B, kPartyHeal, 1),
    IU_C(ItemUse_PartyHealHp240, 0x497490, 0x7E, kPartyHeal240, 1),
    IU_C(ItemUse_PartyCure80, 0x497510, 0x7E, kPartyCure, 1),
    IU_C(ItemUse_HealAp40, 0x497590, 0x1E, kHealApAt0C, 1),
    IU_C(ItemUse_HealAp5, 0x4975B0, 0x1E, kHealApAt0C, 1),
    IU_C(ItemUse_HealHp5, 0x4975D0, 0x1E, kHealHpAt0C, 1),
    IU_C(ItemUse_FaerieTiara, 0x4975F0, 0x7C, kTiara, 1),
    IU_P(ItemUse_WaterJug, 0x497670, 0xA, 1),
    IU_C(Char_HealHp, 0x590CE0, 0x89, kHealHpClear, 1),
    IU_P(Char_HealAp, 0x590D70, 0x64, 1),
    IU_P(Stat_AddCap999, 0x590DE0, 0x27, 1),
    IU_P(Stat_AddCap99, 0x590E10, 0x1D, 1),
    IU_P(Stat_AddClamped, 0x590E30, 0x4F, 2),
    IU_P(Char_ClearStatus, 0x590F60, 0x56, 1),
    IU_C(MsgBox_ChoiceCommit, 0x4981C0, 0x6D, kCommit, 0),
    IU_C(MsgBox_MenuCommit, 0x4983C0, 0x4C, kCommit, 0),
    IU_P(MsgBox_SysChoice80, 0x498AD0, 0x2E, 0),
    IU_P(MsgBox_SysChoice81, 0x498B00, 0x2E, 0),
    IU_P(MsgBox_SysChoice82, 0x498B30, 0x2E, 0),
    IU_P(MsgBox_SysChoice83, 0x498B60, 0x2E, 0),
    IU_P(MsgBox_SysChoice84, 0x498B90, 0x2E, 0),
    IU_P(MsgBox_SysChoice85, 0x498BC0, 0x17, 0),
    IU_P(MsgBox_SysChoice86, 0x498BE0, 0x17, 0),
    IU_P(MsgBox_SysChoice87, 0x498C00, 0x17, 0),
    IU_P(MsgBox_SysChoice88, 0x498C20, 0x17, 0),
    IU_P(MsgBox_SysChoice89, 0x498C40, 0x17, 0),
    IU_P(MsgBox_SysChoice8A, 0x498C60, 0x17, 0),
    IU_P(MsgBox_SysChoice8B, 0x498C80, 0x17, 0),
    IU_P(MsgBox_SysChoice8C, 0x498CA0, 0x17, 0),
    IU_P(MsgBox_SysChoice8D, 0x498CC0, 0x17, 0),
    IU_P(MsgBox_SysChoice8E, 0x498CE0, 0x17, 0),
    IU_P(MsgBox_SysChoice8F, 0x498D00, 0x17, 0),
    IU_P(Input_AutoRepeat, 0x461EB0, 0x48, 4),
};
#undef IU_C
#undef IU_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

// Indices the seeding singles out.
enum : unsigned {
    kDispatch = 0, kHealCureK = 9, kReviveK = 14, kScaleK = 23, kTiaraK = 32,
    kHealHpK = 34, kHealApK, kCap999K, kCap99K, kClampedK, kClearK, kChoiceK, kMenuK,
    kSys80 = 42, kRepeatK = kSys80 + 16,
};

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kCharRecords, 8 * at::kCharStride},   // the eight persistent records
    {0x802D70, 0xAB0},                          // the leader's x / z, the working copies, the window records
    {0x7DEE40, 0x30},                           // MsgBoxState
    {0x929F00, 0x10},                           // 0x929F01, 0x929F0B
    {0x903840, 0x10},                           // 0x903848
    {at::kStoryFlags, 0x40},                    // the story flags, 0x90405F, the party lists 0x904062
    {at::kReturnPoint, 0x10},                   // the Faerie Tiara's copy
    {0x904EFC, 2},                              // Game_AreaNumber
    {0x905B60, 2},                              // 0x905B61
    {0x66C7E8, 4},                              // Game_Step
    {0x7E1BE0, 0x10},                           // the repeat countdown, Input_Held / Previous / Pressed
    {at::kRepeatLatch, 2},
};
constexpr unsigned kRegionBytes = 8 * at::kCharStride + 0xAB0 + 0x30 + 0x10 + 0x10 + 0x40 + 0x10 + 2 + 2 + 4 + 0x10 + 2;

struct State {
    unsigned char memory[kRegionBytes];
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

// The constant tables the dispatcher reads, randomised for the fuzz and put
// back after: each consumable's flags byte and its handler index, for all 256
// ids a byte can name.
unsigned char g_saved_flags[256], g_saved_index[256];
void* g_saved_handlers[34];
unsigned char* g_saved_desc[4];
unsigned char g_desc[4][0x40];
Choice g_area_table[128];

void RandomTables() {
    for (unsigned i = 0; i < 256; ++i) {
        At(at::kItemFlags + i * at::kItemStride)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        ItemUse_HandlerIndex[i] = static_cast<unsigned char>(Next() % 34);
    }
}

// Random bytes put back inside what the original's tables and our regions
// hold: the area index (our four descriptors), the party lists (the record
// map 0x66972C holds 0..7 at 0..0x17).
void Fix() {
    Game_AreaNumber = static_cast<unsigned short>(Next() % 4);
    for (unsigned i = 0; i < 8; ++i) At(at::kPartyLists + i)[0] = static_cast<unsigned char>(Next() % 0x18);
}

struct Args { std::uint32_t a[3]; };

// An id whose low byte names one of the eight records, stale bits above.
std::uint32_t Id() { return Half() ? Next() % 8 : Garbage(0xFF, Next() % 8); }
// The battle flag: its low byte 0 or not, stale bits above.
std::uint32_t Battle() {
    switch (Next() % 4) {
    case 0: return 0;
    case 1: return 1;
    case 2: return Garbage(0xFF, 0);
    default: return Garbage(0xFF, 1 + Next() % 0xFF);
    }
}
// A word field of a record, set near a boundary.
void SeedPair(unsigned char* r, unsigned value_at, unsigned max_at) {
    static const std::uint16_t kMax[] = {0, 1, 4, 7, 8, 100, 999, 1000, 0x7FFF, 0xFFFF};
    const std::uint16_t max = Often() ? kMax[Next() % 10] : static_cast<std::uint16_t>(Next());
    SetWord(r + max_at, max);
    switch (Next() % 7) {
    case 0: SetWord(r + value_at, max); break;
    case 1: SetWord(r + value_at, max - 1); break;
    case 2: SetWord(r + value_at, max + 1); break;
    case 3: SetWord(r + value_at, (max >> 2) - 1); break;
    case 4: SetWord(r + value_at, max >> 2); break;
    case 5: SetWord(r + value_at, 0xFFF0); break;
    default: break;
    }
}

Args Seed(unsigned k) {
    Args args;
    args.a[0] = Id();
    args.a[1] = Battle();
    args.a[2] = Next();
    unsigned char* const r = RecordOf(args.a[0], args.a[1]);
    static const std::uint32_t kAmounts[] = {0, 1, 5, 0x14, 0x64, 0xFFFF, 0x10000, 0x10005, 0xFFFFFFFFu, 0x8000};
    switch (k) {
    case kDispatch:
        args.a[1] = Half() ? Next() % 0x5C : Next();   // the item
        args.a[2] = Battle();
        break;
    case kHealCureK:
        if (Half()) r[at::kStatus] = static_cast<unsigned char>(Next() % 4 == 0 ? 0 : 0x20 << (Next() % 3));
        SeedPair(r, at::kHp, at::kMaxHp);
        break;
    case kReviveK:
        SetWord(r + at::kStatus, Half() ? 0x4000 : Next() & ~0x4000u);
        break;
    case kScaleK:
        SeedPair(r, at::kHp, at::kMaxHp);
        {
            static const unsigned char kScale[] = {0, 7, 8, 9, 10, 0x7F, 0x80, 0xFF};
            r[at::kHpScale] = kScale[Next() % 8];
        }
        break;
    case kTiaraK:
        // the ten flags 0xF6..0xFF (bytes +0x1E / +0x1F): none set, or one
        if (Half()) At(at::kStoryFlags + 0x1E)[0] = static_cast<unsigned char>(Half() ? 0 : 1u << (6 + Next() % 2));
        if (Half()) At(at::kStoryFlags + 0x1F)[0] = static_cast<unsigned char>(Half() ? 0 : 1u << (Next() % 8));
        break;
    case kHealHpK:
    case kHealApK:
        args.a[1] = Often() ? kAmounts[Next() % 10] : Next();
        args.a[2] = Battle();
        {
            unsigned char* const rr = RecordOf(args.a[0], args.a[2]);
            if (k == kHealHpK) SeedPair(rr, at::kHp, at::kMaxHp);
            else SeedPair(rr, at::kAp, at::kMaxAp);
        }
        break;
    case kCap999K:
    case kClampedK: {
        unsigned char* const p = RecordOf(args.a[0], args.a[1]) + 0x20 + 2 * (Next() % 0x10);
        static const std::uint16_t kValues[] = {0, 1, 998, 999, 1000, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE};
        if (Often()) SetWord(p, kValues[Next() % 9]);
        args.a[0] = Address(p);
        static const std::uint32_t kDeltas[] = {1, 0, 0xFFFF, 0x10000, 0x10001, 0x8000, 0x7FFF, 0xFFFFFFFFu, 2, 0x18000};
        args.a[1] = Often() ? kDeltas[Next() % 10] : Next();
        break;
    }
    case kCap99K: {
        unsigned char* const p = RecordOf(args.a[0], args.a[1]) + 0x2E + Next() % 0x30;
        static const unsigned char kValues[] = {0, 1, 98, 99, 100, 0x7F, 0x80, 0xFF, 0xFE};
        if (Often()) *p = kValues[Next() % 9];
        args.a[0] = Address(p);
        static const std::uint32_t kDeltas[] = {1, 0, 0xFF, 0x100, 0x101, 0x80, 0x7F, 0xFFFFFFFFu, 2};
        args.a[1] = Often() ? kDeltas[Next() % 9] : Next();
        break;
    }
    case kClearK: {
        static const std::uint32_t kMasks[] = {0x2000, 0xA0, 0x80, 0x4000, 0x10000, 0x12000, 0xFFFF0000u, 0xFFFFFFFFu};
        args.a[1] = Often() ? kMasks[Next() % 8] : Next();
        args.a[2] = Battle();
        unsigned char* const rr = RecordOf(args.a[0], args.a[2]);
        if (Half()) SetWord(rr + at::kStatus, Half() ? args.a[1] : ~args.a[1]);
        break;
    }
    case kChoiceK:
    case kMenuK:
        At(at::kChoiceId)[0] = static_cast<unsigned char>(Half() ? Next() % 0x80 : 0x80 | Next());
        if (Half()) At(at::kChoiceId)[0] = static_cast<unsigned char>(Half() ? 0x7F : 0x80);
        if (Half()) SetWord(At(at::kMessage), 0xFFFF);
        break;
    case kRepeatK: {
        static const std::uint32_t kPressed[] = {0, 0x1000, 0x4000, 0x5000, 0x10000, 0xFFFF0000u, 0x10};
        args.a[0] = Often() ? kPressed[Next() % 7] : Next();
        static const std::uint16_t kTimer[] = {0, 1, 2, 3, 0xC, 0xFFFF};
        if (Often()) SetWord(At(at::kRepeatTimer), kTimer[Next() % 6]);
        if (Half()) SetWord(At(at::kRepeatLatch), Word(reinterpret_cast<unsigned char*>(&Input_Held)) & (Half() ? 0x1000 : 0));
        break;
    }
    default:
        if (k >= kSys80 && k < kSys80 + 16) {
            static const unsigned char kCursor[] = {0, 1, 2, 0xFF, 0x80, 0x7F};
            At(at::kCursor)[0] = Often() ? kCursor[Next() % 6] : static_cast<unsigned char>(Next());
        }
        break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[400];
    unsigned answers[5];          // handler answers 0, 2, 3, 4, other
    unsigned heal_full, heal_clamped, heal_cleared, clamped_up, clamped_down, clear_hit;
    unsigned scale_up, scale_fill, commit_more, commit_done, sys_cursor01, repeat_fired, dispatch_refused;
} g_cover;

void Cover(unsigned k, const State& in, const State& out, std::uint32_t result) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 400) ++g_cover.logged[out.log[i].what];
    if (k <= kTiaraK + 1 && k >= 1) {
        const unsigned a = result & 0xFF;
        ++g_cover.answers[a == 0 ? 0 : a == 2 ? 1 : a == 3 ? 2 : a == 4 ? 3 : 4];
    }
    (void)in;
    switch (k) {
    case kDispatch: if ((result & 0xFF) == 2) ++g_cover.dispatch_refused; break;
    case kHealHpK:
        if (result & 0xFF) ++g_cover.heal_full;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 3) ++g_cover.heal_cleared;
        break;
    case kClampedK:
        if (static_cast<std::int16_t>(result) > 0) ++g_cover.clamped_up;
        if (static_cast<std::int16_t>(result) < 0) ++g_cover.clamped_down;
        break;
    case kClearK: if (result & 0xFF) ++g_cover.clear_hit; break;
    case kScaleK:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 9) ++g_cover.scale_up;
        if ((result & 0xFF) == 0) ++g_cover.scale_fill;
        break;
    case kRepeatK: if (result & 0xFFFF) ++g_cover.repeat_fired; break;
    default: break;
    }
}

using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("item_use: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[8];
        if (c.n_calls > 8) bof3::Fatal("item_use: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    static const auto kItemStubs = MakeItemStubs(std::make_index_sequence<34>{});
    static const auto kAreaStubs = MakeAreaStubs(std::make_index_sequence<128>{});

    // Save what the fuzz overwrites outside the regions, and swap the tables.
    static State saved, input, their_out, our_out;
    Capture(saved);
    for (unsigned i = 0; i < 256; ++i) {
        g_saved_flags[i] = At(at::kItemFlags + i * at::kItemStride)[0];
        g_saved_index[i] = ItemUse_HandlerIndex[i];
    }
    std::memcpy(g_saved_handlers, ItemUse_Handlers, sizeof g_saved_handlers);
    for (unsigned a = 0; a < 4; ++a) g_saved_desc[a] = Area_Descriptors[a];
    for (unsigned i = 0; i < 34; ++i) ItemUse_Handlers[i] = reinterpret_cast<void*>(kItemStubs[i]);
    for (unsigned i = 0; i < 128; ++i) g_area_table[i] = kAreaStubs[(i * 37) % 128];
    for (unsigned a = 0; a < 4; ++a) {
        std::memset(g_desc[a], 0xCC, sizeof g_desc[a]);
        const std::uint32_t table = Address(g_area_table + 32 * a);   // each area a different quarter
        std::memcpy(g_desc[a] + 0x34, &table, 4);
        Area_Descriptors[a] = g_desc[a];
    }
    // The areas' tables are 32 long and a choice id indexes up to 0x7F: the
    // four quarters are one array, so every id lands on a stand-in (area 3
    // with id 0x60 and up reads past the array - kept below 0x20 there).
    g = kStubs;

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        RandomTables();
        g_seed = Next();
        const Args args = Seed(k);
        if ((k == kChoiceK || k == kMenuK) && At(at::kChoiceId)[0] < 0x80)
            At(at::kChoiceId)[0] = static_cast<unsigned char>(At(at::kChoiceId)[0] % (0x80 - 0x20 * Game_AreaNumber));
        Capture(input);

        std::uint32_t result[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            const std::uint32_t r = reinterpret_cast<Fn3>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2]);
            Capture(out);
            const unsigned w = kClones[k].ret;
            result[pass] = w == 0 ? 0u : w == 1 ? (r & 0xFFu) : w == 2 ? (r & 0xFFFFu) : r;
            out.result = result[pass];
        }
        calls += their_out.log_n;
        Cover(k, input, their_out, result[0]);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      item_use self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    std::memcpy(ItemUse_Handlers, g_saved_handlers, sizeof g_saved_handlers);
    for (unsigned a = 0; a < 4; ++a) Area_Descriptors[a] = g_saved_desc[a];
    for (unsigned i = 0; i < 256; ++i) {
        At(at::kItemFlags + i * at::kItemStride)[0] = g_saved_flags[i];
        ItemUse_HandlerIndex[i] = g_saved_index[i];
    }
    Apply(saved);

    bof3::Log("shadow      item_use self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the eight records, the working copies and window records, MsgBoxState, the menu, "
              "story-flag, party and return-point bytes, Game_Step, the pad words, the answer and the stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      item_use: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned items = 0, areas = 0;
    for (unsigned i = 100; i < 134; ++i) items += c.logged[i] ? 1u : 0u;
    for (unsigned i = 200; i < 328; ++i) areas += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      item_use coverage: handler answers 0 %u, 2 %u, 3 %u, 4 %u, other %u; dispatched to %u of 34 "
              "entries, refused %u; area choices %u of 128, system choice %u; heals answered 1 %u (status cleared %u); "
              "clamped up %u, down %u; status cleared %u; scale raised %u, filled %u; repeat fired %u; "
              "party counts asked %u, rolls %u, flag tests %u, recomputes %u",
              c.answers[0], c.answers[1], c.answers[2], c.answers[3], c.answers[4], items, c.dispatch_refused, areas,
              c.logged[12], c.heal_full, c.heal_cleared, c.clamped_up, c.clamped_down, c.clear_hit, c.scale_up,
              c.scale_fill, c.repeat_fired, c.logged[7], c.logged[8], c.logged[11], c.logged[9]);
    if (bad) bof3::Fatal("the item effects differ from the original in %u self-test rounds", bad);
}

}  // namespace item_use

// BOF3X_SHADOW=char_stats: a differential fuzz of the stat, inventory and
// item-table functions and the two menu draws, once at start-up.
// docs/char-stats.md section 5.
//
// Twenty byte-copies, every call out re-aimed at a recording stand-in, the
// seven jump tables relocated in the copies, the trait-list pointer table
// 0x667548 swapped for lists of our own. One round: one function, random
// bytes in every region any of them touches, then that function's branch
// boundaries seeded; theirs, then from the same state ours; the regions, the
// answer (at the width the original defines) and the stand-ins' log compared.
//
// The stand-ins are as loud as the real callees where the caller reads after
// the call: the stat steps and the passes rewrite the record fields
// Char_RecalcStats reads after them (the scale, the base and effective max
// HP, HP, the halving bits, the percentages, the roster bonus) and the
// equipment bytes the passes must not read twice; the recompute rewrites the
// previews' copy by what the copy holds; the commit moves Gfx_PacketNext,
// which both draws read again after it; the primitive set-ups scribble on
// the fields their callers then write.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/char_stats_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace char_stats {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x2F6B1D93u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d, e; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;          // the stand-ins' own stream: the same on both passes
unsigned char* g_cur;          // the record the round is about: the stand-ins scribble on it

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}

constexpr std::uint32_t kRecordsEnd = at::kCharRecords + 8 * at::kCharStride;

// Every cell below is one some function reads again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const auto v = static_cast<unsigned char>(h >> 12);
    const unsigned w = h >> 20;
    switch ((h >> 4) % 10) {
    case 0: if (g_cur) g_cur[at::kHalveBits] = v; break;
    case 1: if (g_cur) g_cur[at::kHpScale] = static_cast<unsigned char>(v % 12); break;
    case 2: if (g_cur) g_cur[at::kBase + (w & 1)] = v; break;
    case 3: if (g_cur) g_cur[at::kMaxHp + (w & 1)] = v; break;
    case 4: if (g_cur) g_cur[at::kHp + (w & 1)] = v; break;
    case 5: if (g_cur) g_cur[at::kPercent + w % 5] = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 6: At(at::kRosterBonus + w % 40)[0] = v; break;
    case 7: if (g_cur) g_cur[at::kWeapon + w % 6] = v; break;
    case 8: At(at::kCharRecords + (w % 8) * at::kCharStride + at::kAtk + (v % 8))[0] = static_cast<unsigned char>(w); break;
    default: if (g_cur) g_cur[at::kAtk + w % 8] = v; break;
    }
}

// --- the stand-ins ---------------------------------------------------------

void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(1, Address(prim), static_cast<std::uint32_t>(dfe), static_cast<std::uint32_t>(dtd), tpage, tw);
    SetLong(prim + 4, static_cast<std::int32_t>(0xE8000000u | (tpage & 0xFFFF)));
    SetLong(prim + 8, static_cast<std::int32_t>(Hash()));
    Disturb();
}
// The commit moves Gfx_PacketNext by the size a quarter of the time not.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(2, slot, size);
    if (Hash() % 4) Gfx_PacketNext = Gfx_PacketNext + (size & 0xFF);
    Disturb();
}
void __cdecl StubPolyFT4(unsigned char* prim) {
    Record(3, Address(prim));
    prim[7] = 0x2C;
    const std::uint32_t h = Hash();
    for (unsigned off : {0x10u, 0x20u, 0x30u, 0x40u}) SetLong(prim + off, static_cast<std::int32_t>(h + off));
    // fields the caller writes after: a caller that skips one shows
    for (unsigned off : {0x04u, 0x08u, 0x0Cu, 0x14u, 0x18u, 0x1Cu, 0x24u, 0x28u, 0x2Cu, 0x34u, 0x38u, 0x3Cu, 0x44u})
        SetLong(prim + off, static_cast<std::int32_t>(h ^ (off * 0x01010101u)));
    Disturb();
}
void __cdecl StubSprt(unsigned char* prim) {
    Record(4, Address(prim));
    prim[7] = 0x64;
    const std::uint32_t h = Hash();
    SetLong(prim + 0x10, static_cast<std::int32_t>(h));
    for (unsigned off : {0x04u, 0x08u, 0x0Cu, 0x14u, 0x18u}) SetLong(prim + off, static_cast<std::int32_t>(h ^ (off * 0x01010101u)));
    Disturb();
}
// The stat steps read only the amount's low word (Stat_AddClamped) or low byte
// (the rest); the callers leave stale bits above, so only those are logged.
short __cdecl StubClamped(unsigned short* stat, unsigned delta) {
    Record(5, Address(stat), delta & 0xFFFF);
    if (Hash() % 2) SetWord(reinterpret_cast<unsigned char*>(stat), Hash() >> 4);
    Disturb();
    return static_cast<short>(Hash() >> 8);
}
unsigned char __cdecl StubCap99(unsigned char* stat, unsigned amount) {
    Record(6, Address(stat), amount & 0xFF);
    if (Hash() % 2) *stat = static_cast<unsigned char>(Hash() >> 4);
    Disturb();
    return static_cast<unsigned char>(Hash() >> 8);
}
unsigned char __cdecl StubResist(unsigned char* level, unsigned step) {
    Record(7, Address(level), step & 0xFF);
    if (Hash() % 2) *level = static_cast<unsigned char>(Hash() >> 4);
    Disturb();
    return static_cast<unsigned char>(Hash() >> 8);
}
unsigned char __cdecl StubCap100(unsigned char* value, unsigned amount) {
    Record(8, Address(value), amount & 0xFF);
    if (Hash() % 2) *value = static_cast<unsigned char>(Hash() >> 4);
    Disturb();
    return static_cast<unsigned char>(Hash() >> 8);
}
template <unsigned N> void __cdecl StubPass(unsigned char* record) {
    Record(N, Address(record));
    Disturb();
}
// The recompute: a record outside the regions (a preview's copy on the
// stack) is logged by a hash of its bytes, and its four stats moved by that
// hash - up, down, or left, so every mark shows.
void __cdecl StubRecalc(unsigned char* record) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < at::kRecordBytes; ++i) h = (h ^ record[i]) * 0x01000193u;
    const bool ours = Address(record) >= 0x903640 && Address(record) < kRecordsEnd;
    Record(13, ours ? Address(record) : 0xFFFFFFFFu, h);
    for (unsigned k = 0; k < 4; ++k) {
        unsigned char* const p = record + at::kAtk + 2 * k;
        switch ((h >> (4 * k)) % 4) {
        case 0: break;
        case 1: SetWord(p, Word(p) + 1); break;
        case 2: SetWord(p, Word(p) - 1); break;
        default: SetWord(p, h >> 16); break;
        }
    }
    Disturb();
}

const Callees kStubs = {
    StubDrawMode, StubCommit, StubPolyFT4, StubSprt, StubClamped, StubCap99, StubResist, StubCap100,
    StubPass<9>, StubPass<10>, StubPass<11>, StubPass<12>, StubRecalc,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5A77C0: return f(&StubDrawMode);
    case 0x461E50: return f(&StubCommit);
    case 0x5A75D0: return f(&StubPolyFT4);
    case 0x5A7710: return f(&StubSprt);
    case 0x590E30: return f(&StubClamped);
    case 0x590E10: return f(&StubCap99);
    case 0x590EE0: return f(&StubResist);
    case 0x590F30: return f(&StubCap100);
    case 0x590FC0: return f(&StubPass<9>);
    case 0x591190: return f(&StubPass<10>);
    case 0x591490: return f(&StubPass<11>);
    case 0x590800: return f(&StubPass<12>);
    case 0x590660: return f(&StubRecalc);
    default: bof3::Fatal("char_stats: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the twenty copies (capstone, 2026-09-23: every jump internal; the calls
// below are every call that leaves, the tables every jmp [reg*4 + table]) ---

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    move_script::Table table;   // entries 0: none
    unsigned ret;               // the answer's width the original defines: 0 none, 1 al, 2 ax, 4 eax
    const void* ours;
};

constexpr Call kIcon[] = {{0x83, 0x5A77C0}, {0x8F, 0x461E50}, {0x9B, 0x5A75D0}, {0x1C4, 0x461E50}};
constexpr Call kHand[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7710}, {0x82, 0x461E50}};
constexpr Call kRecalc[] = {{0x18, 0x590FC0}, {0x1E, 0x591190}, {0x24, 0x591490}, {0x2A, 0x590800}, {0x72, 0x590EE0},
                            {0x86, 0x590EE0}, {0x9A, 0x590EE0}, {0xAE, 0x590EE0}, {0xC2, 0x590EE0}, {0xD6, 0x590EE0},
                            {0xEA, 0x590EE0}, {0xFE, 0x590EE0}, {0x128, 0x590F30}, {0x13F, 0x590F30}, {0x155, 0x590F30},
                            {0x16C, 0x590F30}, {0x183, 0x590F30}};
constexpr Call kTraits[] = {{0x4C, 0x590EE0}, {0x5E, 0x590EE0}, {0x70, 0x590EE0}, {0x82, 0x590EE0}, {0x91, 0x590EE0},
                            {0xA0, 0x590EE0}, {0xAF, 0x590EE0}, {0xBE, 0x590EE0}, {0xCD, 0x590EE0}, {0x103, 0x590F30}};
constexpr Call kPreviewSlot[] = {{0xD3, 0x590660}};
constexpr Call kPreviewSet[] = {{0x9F, 0x590660}};
constexpr Call kWeapon[] = {{0x32, 0x590E30}, {0x46, 0x590E30}, {0x6F, 0x590F30}, {0x82, 0x590E30}, {0x8A, 0x590E30},
                            {0x95, 0x590E30}, {0xA8, 0x590E30}, {0xBB, 0x590E30}, {0xCE, 0x590EE0}, {0xE1, 0x590E30},
                            {0xEC, 0x590EE0}, {0xF7, 0x590EE0}, {0x10A, 0x590F30}, {0x11D, 0x590F30}, {0x128, 0x590F30},
                            {0x13B, 0x590EE0}, {0x14E, 0x590F30}};
constexpr Call kArmour[] = {{0x46, 0x590E30}, {0x5A, 0x590E30}, {0x83, 0x590EE0}, {0x93, 0x590EE0}, {0x9E, 0x590EE0},
                            {0xB1, 0x590EE0}, {0xBC, 0x590EE0}, {0xC7, 0x590EE0}, {0xDA, 0x590EE0}, {0xE5, 0x590EE0},
                            {0xF0, 0x590EE0}, {0xFB, 0x590EE0}, {0x119, 0x590EE0}, {0x124, 0x590EE0}, {0x12F, 0x590EE0},
                            {0x14A, 0x590EE0}, {0x15A, 0x590EE0}, {0x165, 0x590EE0}, {0x170, 0x590EE0}, {0x183, 0x590EE0},
                            {0x18E, 0x590EE0}, {0x199, 0x590EE0}, {0x1A4, 0x590EE0}, {0x1C1, 0x590EE0}, {0x1CC, 0x590EE0},
                            {0x1D7, 0x590EE0}, {0x1E7, 0x590EE0}, {0x1F4, 0x590EE0}, {0x201, 0x590EE0}, {0x216, 0x590EE0},
                            {0x223, 0x590EE0}, {0x22E, 0x590E30}, {0x23E, 0x590EE0}, {0x24B, 0x590E30}};
constexpr Call kAccessories[] = {{0x3C, 0x590E30}, {0x5D, 0x590E30}, {0x6D, 0x590E30}, {0x7A, 0x590E30}, {0x8A, 0x590E30},
                                 {0x9A, 0x590E10}, {0xF9, 0x590F30}, {0x106, 0x590F30}, {0x113, 0x590EE0}, {0x11E, 0x590EE0},
                                 {0x129, 0x590EE0}, {0x134, 0x590EE0}, {0x13F, 0x590EE0}, {0x14A, 0x590EE0}, {0x155, 0x590EE0},
                                 {0x160, 0x590EE0}, {0x16E, 0x590EE0}};

#define CS_C(name, base, size, calls, table, ret) \
    {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), table, ret, reinterpret_cast<const void*>(&::name)}
#define CS_P(name, base, size, table, ret) {#name, base, size, nullptr, 0, table, ret, reinterpret_cast<const void*>(&::name)}
constexpr move_script::Table kNoTable = {0, 0, 0};
const Clone kClones[] = {
    CS_C(Menu_DrawIcon, 0x5903F0, 0x1D2, kIcon, kNoTable, 0),
    CS_C(Menu_DrawHand, 0x5905D0, 0x8C, kHand, kNoTable, 0),
    CS_C(Char_RecalcStats, 0x590660, 0x1A0, kRecalc, kNoTable, 0),
    CS_C(Char_ApplyTraits, 0x590800, 0x154, kTraits, (move_script::Table{0x40, 0x11C, 14}), 0),
    CS_C(Equip_PreviewSlot, 0x590960, 0x144, kPreviewSlot, (move_script::Table{0x7E, 0x12C, 6}), 0),
    CS_C(Equip_PreviewSet, 0x590AB0, 0xF8, kPreviewSet, kNoTable, 0),
    CS_P(Inventory_Add, 0x590BB0, 0xDE, kNoTable, 1),
    CS_P(Stat_AddResist, 0x590EE0, 0x4A, kNoTable, 1),
    CS_P(Stat_AddCap100, 0x590F30, 0x23, kNoTable, 1),
    CS_C(Char_ApplyWeapon, 0x590FC0, 0x1C9, kWeapon, (move_script::Table{0x65, 0x15C, 11}), 0),
    CS_C(Char_ApplyArmour, 0x591190, 0x2F7, kArmour, (move_script::Table{0x79, 0x26C, 21}), 0),
    CS_C(Char_ApplyAccessories, 0x591490, 0x1E8, kAccessories, (move_script::Table{0x53, 0x18C, 23}), 0),
    CS_P(Item_NamePtr, 0x591680, 0x98, (move_script::Table{0x12, 0x88, 4}), 4),
    CS_P(Item_IconKind, 0x591720, 0x76, kNoTable, 4),
    CS_P(Item_EquipMask, 0x5917A0, 0x70, (move_script::Table{0x12, 0x60, 4}), 4),
    CS_P(KeyItem_Has, 0x5918E0, 0x1D, kNoTable, 1),
    CS_P(TextRecord_Set, 0x591940, 0x69, kNoTable, 0),
    CS_P(Inventory_Count, 0x5919B0, 0xC5, kNoTable, 2),
    CS_P(Inventory_CountUsed, 0x591A80, 0x3C, kNoTable, 1),
    CS_P(Item_Price, 0x591C20, 0x9C, (move_script::Table{0x12, 0x8C, 4}), 4),
};
#undef CS_C
#undef CS_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kIconK, kHandK, kRecalcK, kTraitsK, kPreviewSlotK, kPreviewSetK, kAddK, kResistK, kCap100K, kWeaponK, kArmourK,
    kAccessoriesK, kNameK, kIconKindK, kMaskK, kKeyHasK, kTextK, kCountK, kCountUsedK, kPriceK,
};
static_assert(kPriceK + 1 == kCount, "the index enum follows kClones");

// --- the state both passes start from --------------------------------------

unsigned char g_packets[0x400];   // Gfx_PacketNext points in here
unsigned char g_out[0x20];        // the previews' marks at 0, values at 8
unsigned char g_traits[0x100];    // the trait lists, even offsets, ended by a kind 0xE

struct Region { std::uint32_t at, size; };
constexpr unsigned kRegionCount = 6;
Region g_regions[kRegionCount];
constexpr unsigned kRegionBytes = 0x950 + 0x600 + 0x2000 + 4 + sizeof g_packets + sizeof g_out;

void MakeRegions() {
    g_regions[0] = {at::kRosterBonus, 0x950};    // the roster bonus, DamageScratch, the eight records
    g_regions[1] = {0x904000, 0x600};            // the extras 0x90412E..0x904130, the inventory lists, key items
    g_regions[2] = {at::kTextRecords, 0x2000};   // Text_Records, 256 entries
    g_regions[3] = {Address(&Gfx_PacketNext), 4};
    g_regions[4] = {Address(g_packets), sizeof g_packets};
    g_regions[5] = {Address(g_out), sizeof g_out};
}

struct State {
    unsigned char memory[kRegionBytes];
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned off = 0;
    for (const Region& r : g_regions) { std::memcpy(s.memory + off, At(r.at), r.size); off += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned off = 0;
    for (const Region& r : g_regions) { std::memcpy(At(r.at), s.memory + off, r.size); off += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

void* g_saved_traits[0xFF];

// The lists: pairs of random kinds 0..0xF (0xE ends), a final 0xE; each of the
// 255 pointers at an even offset, a few null.
void RandomTraits() {
    for (unsigned i = 0; i < sizeof g_traits; i += 2) {
        g_traits[i] = static_cast<unsigned char>(Next() % 0x10);
        g_traits[i + 1] = static_cast<unsigned char>(Next());
    }
    g_traits[sizeof g_traits - 2] = 0xE;
    for (unsigned t = 0; t < 0xFF; ++t) {
        const std::uint32_t p = Next() % 8 == 0 ? 0 : Address(g_traits) + 2 * (Next() % (sizeof g_traits / 2));
        SetLong(At(at::kTraitLists + 4 * t), static_cast<std::int32_t>(p));
    }
}

struct Args { std::uint32_t a[6]; };

std::uint32_t Byte(std::uint32_t low) { return Half() ? low : Garbage(0xFF, low & 0xFF); }
unsigned char* Rec(unsigned i) { return At(at::kCharRecords + (i % 8) * at::kCharStride); }

// A record for the passes and the recompute: one of the eight, or a stretch of
// the first region below them that no persistent record starts at.
unsigned char* AnyRecord() { return Often() ? Rec(Next()) : At(at::kRosterBonus + 0x40 + Next() % 0x300); }

constexpr unsigned char kWeaponIds[] = {0x0C, 0x0D, 0x0E, 0x15, 0x1A, 0x20, 0x25, 0x27, 0x28, 0x34, 0x41,
                                        0x43, 0x44, 0x4C, 0x4D, 0x4E, 0x00, 0xFF};
constexpr unsigned char kArmourIds[] = {0x0B, 0x0C, 0x0D, 0x0F, 0x11, 0x13, 0x14, 0x16, 0x18, 0x19, 0x1B, 0x1C, 0x1D, 0x1E,
                                        0x20, 0x28, 0x2C, 0x2D, 0x2F, 0x30, 0x31, 0x39, 0x3B, 0x3C, 0x3E, 0x40, 0x41, 0x42,
                                        0x43, 0x00, 0xFF};

void SeedRecord(unsigned char* r) {
    static const unsigned char kScale[] = {0, 1, 5, 9, 10, 0xFF};
    static const std::uint16_t kBase[] = {0, 1, 999, 0x7FFF, 0xFFFF};
    if (Often()) r[at::kHpScale] = kScale[Next() % 6];
    if (Half()) SetWord(r + at::kBase, kBase[Next() % 5]);
    if (Half()) r[at::kHalveBits] = static_cast<unsigned char>(Half() ? 0 : 1u << (Next() % 8));
    for (unsigned j = 0; j < 5; ++j)
        if (Half()) r[at::kPercent + j] = 0;
    if (Half()) SetWord(r + at::kHp, Word(r + at::kBase) + (Next() % 3) - 1);
}

Args Seed(unsigned k) {
    Args x;
    for (auto& v : x.a) v = Next();
    g_cur = nullptr;
    switch (k) {
    case kIconK:
    case kHandK: {
        Gfx_PacketNext = g_packets + 4 * (Next() % 0x40);
        if (k == kIconK) {
            // 0..20 the table, 24..91 the frame (return address, the argument
            // slots, the harness's extra arguments); 21..23 are undefined
            const unsigned r = Next() % 8;
            x.a[0] = Byte(r < 5 ? Next() % 21 : r == 5 ? 24 + Next() % 28 : r == 6 ? (Half() ? 28 : 48) + Next() % 4 : 52 + Next() % 40);
            if (Next() % 4 == 0) x.a[0] = Byte(Half() ? (Half() ? 11 : 12) : (Half() ? 19 : 20));
        }
        break;
    }
    case kRecalcK:
    case kTraitsK:
    case kWeaponK:
    case kArmourK:
    case kAccessoriesK: {
        unsigned char* const r = AnyRecord();
        g_cur = r;
        x.a[0] = Address(r);
        SeedRecord(r);
        if (k == kTraitsK) {
            RandomTraits();
            if (Next() % 6 == 0) r[at::kTrait] = 0xFF;
            else if (Half()) r[at::kTrait] = static_cast<unsigned char>(Next() % 8);
            if (Next() % 6 == 0) g_traits[0] = 0xE;
        }
        if (k == kWeaponK && Often()) r[at::kWeapon] = kWeaponIds[Next() % sizeof kWeaponIds];
        if (k == kArmourK)
            for (unsigned b = 0; b < 3; ++b)
                if (Often()) r[at::kArmour + b] = kArmourIds[Next() % sizeof kArmourIds];
        if (k == kAccessoriesK)
            for (unsigned b = 0; b < 2; ++b)
                if (Often()) r[at::kAccessory + b] = static_cast<unsigned char>(Half() ? Next() % 0x19 : (Half() ? 0xFF : 0x17));
        break;
    }
    case kPreviewSlotK:
    case kPreviewSetK: {
        const unsigned id = Next() % 8;
        g_cur = Rec(id);
        x.a[0] = Byte(id);
        if (k == kPreviewSlotK) {
            static const unsigned kSlots[] = {0, 1, 2, 3, 4, 5, 6, 0xFF};
            x.a[1] = Byte(Often() ? kSlots[Next() % 8] : Next());
            x.a[3] = Address(g_out);
            x.a[4] = Address(g_out + 8);
        } else {
            x.a[1] = 0x904000 + Next() % 0x5F0;
            x.a[2] = Address(g_out);
            x.a[3] = Address(g_out + 8);
        }
        break;
    }
    case kAddK: {
        const unsigned cat = Next() % 5;
        x.a[0] = Byte(cat);
        unsigned char* const ids = At(static_cast<std::uint32_t>(Long(At(at::kInvIds + 4 * cat))));
        unsigned char* const counts = cat == 4 ? nullptr : At(static_cast<std::uint32_t>(Long(At(at::kInvCounts + 4 * cat))));
        const unsigned slot = Next() % 0x80;
        static const unsigned char kCounts[] = {0, 1, 2, 0x62, 0x63, 0x64, 0x7F, 0x80, 0xFF};
        const unsigned how = Next() % 4;
        if (how == 0) x.a[1] = Byte(ids[slot]);                    // a stack that exists (or a free id 0)
        if (how == 1) x.a[1] = Byte(0);
        x.a[2] = Byte(Often() ? kCounts[Next() % 9] : Next());
        if (counts && Half()) counts[slot] = kCounts[Next() % 9];
        if (how == 0 && counts && Half()) {
            // the stack's sum at the cap: 99, 100 or 101
            ids[slot] = static_cast<unsigned char>(ids[slot] | 1);
            x.a[1] = Byte(ids[slot]);
            for (unsigned i = 0; i < slot; ++i)
                if (ids[i] == ids[slot]) ids[i] = static_cast<unsigned char>(ids[i] + 2);
            x.a[2] = Byte(1 + Next() % 0x63);
            counts[slot] = static_cast<unsigned char>(0x63 + Next() % 3 - (x.a[2] & 0xFF));
        }
        if (Next() % 3 == 0) {
            // a full list: no id 0 (and no count 0)
            for (unsigned i = 0; i < 0x80; ++i) {
                if (ids[i] == 0) ids[i] = static_cast<unsigned char>(1 + Next() % 0xFF);
                if (counts && counts[i] == 0) counts[i] = static_cast<unsigned char>(1 + Next() % 0xFF);
            }
            if (Half()) ids[Next() % 0x80] = 0;
            else if (counts && Half()) counts[Next() % 0x80] = 0;
        }
        break;
    }
    case kResistK:
    case kCap100K: {
        unsigned char* const p = Rec(Next()) + at::kResist + Next() % 0x10;
        static const unsigned char kResistValues[] = {0, 1, 2, 4, 5, 6, 7, 8, 0x7F, 0x80, 0xFA, 0xFF};
        static const unsigned char kCapValues[] = {0, 1, 0x63, 0x64, 0x65, 0x7F, 0x80, 0x9C, 0xE4, 0xFF};
        static const unsigned char kSteps[] = {0, 1, 2, 3, 5, 6, 7, 0x1B, 0x1C, 0x7F, 0x80, 0x9C, 0xFA, 0xFD, 0xFF};
        if (Often()) *p = k == kResistK ? kResistValues[Next() % 12] : kCapValues[Next() % 10];
        x.a[0] = Address(p);
        x.a[1] = Byte(Often() ? kSteps[Next() % 15] : Next());
        if (k == kResistK && Next() % 4 == 0) {
            // the refusals' edges: a level of 5..8 against a step of 0, one up, one down, or 0x7F
            static const unsigned char kStepEdges[] = {0, 1, 0xFF, 0x7F};
            *p = static_cast<unsigned char>(5 + Next() % 4);
            x.a[1] = Byte(kStepEdges[Next() % 4]);
        }
        break;
    }
    case kNameK:
    case kIconKindK:
    case kMaskK:
    case kPriceK:
        x.a[0] = Byte(Often() ? Next() % 7 : Next());
        break;
    case kKeyHasK:
        if (Often()) x.a[0] = Byte(At(at::kKeyItems)[Next() % 0x20]);
        break;
    case kTextK: {
        static const unsigned char kLengths[] = {0, 1, 2, 0x1E, 0x1F, 0x20, 0x21, 0xFF};
        x.a[0] = Byte(Next());
        x.a[1] = Byte(Often() ? kLengths[Next() % 8] : Next());
        const std::uint32_t dest = at::kTextRecords + (x.a[0] & 0xFF) * 0x20;
        switch (Next() % 4) {
        case 0: x.a[2] = dest - 1 - Next() % 3; break;           // overlapping below: the byte loop repeats
        case 1: x.a[2] = dest + 1 + Next() % 3; break;           // overlapping above
        default: x.a[2] = 0x904000 + Next() % 0x5E0; break;
        }
        break;
    }
    case kCountK: {
        const bool stock = Half();
        x.a[2] = stock ? (Half() ? 0 : Garbage(0xFF, 0)) : Byte(1 + Next() % 0xFF);
        const unsigned cat = stock ? Next() % 4 : (Often() ? 1 + Next() % 3 : Next() & 0xFF);
        x.a[0] = Byte(cat);
        switch (Next() % 4) {
        case 0:
            if (stock) x.a[1] = Byte(At(static_cast<std::uint32_t>(Long(At(at::kInvIds + 4 * cat))))[Next() % 0x80]);
            else {
                // a worn item: its wearer counted, the category its slot's
                unsigned char* const r = Rec(Next());
                const unsigned slot = Next() % 6;
                r[at::kFlags] = static_cast<unsigned char>(r[at::kFlags] | 1);
                x.a[1] = Byte(r[at::kWeapon + slot]);
                if (Often()) x.a[0] = Byte(slot == 0 ? 1 : slot < 4 ? 2 : 3);
            }
            break;
        case 1:
            x.a[1] = Byte(Half() ? At(at::kExtraAccessory)[0] : At(at::kExtraItem)[0]);
            break;
        default: break;
        }
        break;
    }
    case kCountUsedK:
        x.a[0] = Byte(Next() % 9);
        if (Half()) {
            // a list with its ids set, so the count reaches its length
            unsigned char* const ids = At(static_cast<std::uint32_t>(Long(At(at::kInvIds + 4 * (x.a[0] & 0xFF)))));
            for (unsigned i = 0; i < 0x80; ++i) ids[i] = static_cast<unsigned char>(Half() ? 0 : 1 + Next() % 0xFF);
        }
        break;
    default: break;
    }
    return x;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[16];
    unsigned answers[kCount][4];   // answer 0, 1, other, and (Inventory_Add) the scratch set
    unsigned icon_past, weapon_cases, armour_cases, accessory_cases, trait_calls, roster_calls, marks[4];
} g_cover;
bool g_seen_weapon[256];

void Cover(unsigned k, const Args& x, const State& in, const State& out, std::uint32_t result) {
    (void)in;
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 16) ++g_cover.logged[out.log[i].what];
    const unsigned a = result == 0 ? 0 : result == 1 ? 1 : 2;
    ++g_cover.answers[k][a];
    switch (k) {
    case kIconK: if ((x.a[0] & 0xFF) > 20) ++g_cover.icon_past; break;
    case kAddK: if (At(bof3::addr::DamageScratch)[0] == 1) ++g_cover.answers[k][3]; break;
    case kRecalcK:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 8) ++g_cover.roster_calls;
        break;
    case kTraitsK: g_cover.trait_calls += out.log_n; break;
    default: break;
    }
}

using Fn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                    std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                    std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

unsigned short ControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    MakeRegions();
    unsigned region_bytes = 0;
    for (const Region& r : g_regions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("char_stats: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[40];
        if (c.n_calls > 40) bof3::Fatal("char_stats: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.table.entries) move_script::Relocate(clones[k], c.base, c.size, c.table);
    }

    // Save what the fuzz overwrites outside the regions, and swap the tables.
    static State saved, input, their_out, our_out;
    Capture(saved);
    for (unsigned t = 0; t < 0xFF; ++t) g_saved_traits[t] = reinterpret_cast<void*>(Long(At(at::kTraitLists + 4 * t)));
    unsigned char* const saved_packet = Gfx_PacketNext;
    const unsigned short cw_saved = ControlWord();
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
        g_seed = Next();
        const Args x = Seed(k);
        if (k == kWeaponK) g_seen_weapon[At(x.a[0])[at::kWeapon]] = true;
        Capture(input);

        std::uint32_t result[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            SetControlWord(0x027F);
            const std::uint32_t r = reinterpret_cast<Fn>(const_cast<void*>(fn))(
                x.a[0], x.a[1], x.a[2], x.a[3], x.a[4], x.a[5], 0x13579BDFu, 0x2468ACE0u, 0x0F1E2D3Cu, 0x4B5A6978u,
                0x8796A5B4u, 0xC3D2E1F0u, 0x01234567u, 0x89ABCDEFu, 0xFEDCBA98u, 0x76543210u);
            SetControlWord(cw_saved);
            Capture(out);
            const unsigned w = kClones[k].ret;
            result[pass] = w == 0 ? 0u : w == 1 ? (r & 0xFFu) : w == 2 ? (r & 0xFFFFu) : r;
            out.result = result[pass];
        }
        calls += their_out.log_n;
        Cover(k, x, input, their_out, result[0]);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      char_stats self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    for (unsigned t = 0; t < 0xFF; ++t) SetLong(At(at::kTraitLists + 4 * t), static_cast<std::int32_t>(Address(g_saved_traits[t])));
    Apply(saved);
    Gfx_PacketNext = saved_packet;

    bof3::Log("shadow      char_stats self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the roster bonus and records, the inventory lists and key items, Text_Records, "
              "the packet buffer, the answer and the stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      char_stats: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned weapons = 0;
    for (bool s : g_seen_weapon) weapons += s ? 1u : 0u;
    bof3::Log("shadow      char_stats coverage: icons past the table %u; calls draw mode %u, commit %u, FT4 %u, SPRT %u, "
              "clamped %u, cap99 %u, resist %u, cap100 %u, passes %u / %u / %u / %u, recomputes %u; roster steps %u, "
              "trait calls %u; weapon ids %u; Inventory_Add 0 %u, 1 %u (new slot %u); resist 0 %u, 1 %u; cap100 0 %u, 1 %u; "
              "key item found %u; counts 0 %u, other %u",
              c.icon_past, c.logged[1], c.logged[2], c.logged[3], c.logged[4], c.logged[5], c.logged[6], c.logged[7],
              c.logged[8], c.logged[9], c.logged[10], c.logged[11], c.logged[12], c.logged[13], c.roster_calls, c.trait_calls,
              weapons, c.answers[kAddK][0], c.answers[kAddK][1], c.answers[kAddK][3], c.answers[kResistK][0],
              c.answers[kResistK][1], c.answers[kCap100K][0], c.answers[kCap100K][1], c.answers[kKeyHasK][1],
              c.answers[kCountK][0], c.answers[kCountK][1] + c.answers[kCountK][2]);
    if (bad) bof3::Fatal("the stat and inventory functions differ from the original in %u self-test rounds", bad);
}

}  // namespace char_stats

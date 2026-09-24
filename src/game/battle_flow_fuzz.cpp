// BOF3X_SHADOW=battle_flow: a differential fuzz of the battle task and the
// turn flow, once at start-up. docs/battle_flow.md section 4.
//
// Twenty-one byte-copies, every call and tail jump out re-aimed at a
// recording stand-in (bof3::CloneCall with `expected`); the two tables the
// originals build on their stacks re-aimed inside the copies (their
// immediates checked first); BattleEnemy_SetAnimation's jump table relocated
// into its copy; the enemy state table 0x64B084's first nine entries, the
// pause hook 0x904B6C and every enemy object's +0xF4 pointed at recorders,
// and the objects' +0xFC at an animation table of our own. One round: one
// function, random bytes in every region any of them touches, the pointers
// and indices put back inside what the tables hold, each branch's
// boundaries seeded; theirs, then from the same state ours; the regions, the
// primitive pool, the animation table, the packet cursor, the result and the
// stand-ins' log compared. Everything is put back afterwards.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_flow_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_flow {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Slot(unsigned i) { return At(at::kTasks + i * at::kTaskSize); }
unsigned char* EnemyObj(unsigned i) { return At(at::kEnemies + i * at::kEnemySize); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 160;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

constexpr unsigned kPool = 0x200;
unsigned char g_prim[kPool];
constexpr unsigned kAnim = 0x120;
unsigned char g_anim[kAnim];   // every enemy object's +0xFC table

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
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p);
    const std::uint32_t pool = Address(g_prim), anim = Address(g_anim);
    if (at >= pool && at < pool + sizeof g_prim) return 0x10000u + (at - pool);
    if (at >= anim && at < anim + sizeof g_anim) return 0x20000u + (at - anim);
    return at;
}
std::uint32_t Bytes(const unsigned char* p, unsigned from, unsigned to) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = from; i < to; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}
unsigned char SlotByte(const unsigned char& b) { return *reinterpret_cast<const volatile unsigned char*>(&b); }

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the arrays, indices inside the tables.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 20) {
    case 0: At(at::kPhase)[0] = static_cast<unsigned char>(v % 6); break;
    case 1: At(at::kPaused)[0] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 2: SetPtr(at::kEnemyCurrent, EnemyObj(v % 8)); break;
    case 3: Sprite_Current = EnemyObj(v % 8); break;
    case 4: SetPtr(at::kTaskCurrent, Slot(v % at::kTaskCount)); break;
    case 5: {
        unsigned char* const t = Slot(w % at::kTaskCount);
        t[0] = static_cast<unsigned char>(v % 3 == 0 ? 0 : v);
        t[6] = static_cast<unsigned char>(v % 4);
        break;
    }
    case 6: {
        unsigned char* const e = EnemyObj(w % 8);
        e[0] = static_cast<unsigned char>(v % 3 == 0 ? 0 : v);
        e[1] = static_cast<unsigned char>(v % 5 == 0 ? 0 : v);
        e[0x100] = static_cast<unsigned char>(v % 8);
        break;
    }
    case 7: g_anim[w % kAnim] = static_cast<unsigned char>(v); break;
    case 8: At(at::kDropCount)[0] = static_cast<unsigned char>(v % 17); break;
    case 9: {
        unsigned char* const e = move_script::At(static_cast<std::uint32_t>(Long(At(at::kEnemyCurrent))));
        const unsigned slot = 0xA8 + (w % 2) * 4;
        if (v % 2) SetWord(e + slot, v % 5);
        else e[slot + 2] = static_cast<unsigned char>(v % 8);
        break;
    }
    case 10: At(at::kEnemiesLeft)[0] = static_cast<unsigned char>(v % 3); break;
    case 11: At(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(At(at::kRoundFlags + 1)[0] ^ 0x40); break;
    case 12: At(at::kActorAt)[0] = static_cast<unsigned char>(v % 11); break;
    case 13: SetWord(At(at::kMagicId), v % 3 == 0 ? 0xA1 : v); break;
    case 14: {
        unsigned char* const s = move_script::At(static_cast<std::uint32_t>(Long(At(at::kTaskCurrent))));
        SetWord(s + 0x32 + 4 * (w % 3), h >> 16);
        break;
    }
    case 15: At(at::kNumberText + w % 8)[0] = static_cast<unsigned char>(v % 3 == 0 ? ' ' : v); break;
    case 16: {
        unsigned char* const e = move_script::At(static_cast<std::uint32_t>(Long(At(at::kEnemyCurrent))));
        if (v % 2) e[0x8C] = static_cast<unsigned char>(w);
        else SetLong(e + 0x110, static_cast<std::int32_t>(h * 0x9E3779B1u));
        break;
    }
    case 17: At(at::kAnimGate)[0] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 18: {
        unsigned char* const e = move_script::At(static_cast<std::uint32_t>(Long(At(at::kEnemyCurrent))));
        e[0x92 + w % 2] = static_cast<unsigned char>(v);
        break;
    }
    default: Sprite_Current[8] = static_cast<unsigned char>(v % 5); break;
    }
}

// --- the stand-ins ---------------------------------------------------------

// The six phases, the four task kinds, the nine enemy state entries.
template <unsigned N> void __cdecl StubHandler() { Record(N, Id(Sprite_Current), static_cast<std::uint32_t>(Long(At(at::kEnemyCurrent)))); Disturb(); }
void __cdecl StubPauseHook(int n) { Record(40, static_cast<std::uint32_t>(n)); Disturb(); }
void __cdecl StubEnemyHook(int n) { Record(41, static_cast<std::uint32_t>(n), Id(Sprite_Current)); Disturb(); }
void __cdecl StubUpdateScreen() { Record(42, Id(Sprite_Current), static_cast<std::uint32_t>(Long(At(at::kEnemyCurrent)))); Disturb(); }
unsigned char __cdecl StubEnsureAnimation(unsigned char a) {
    Record(43, SlotByte(a), Id(Sprite_Current));
    Disturb();
    return static_cast<unsigned char>(Hash() >> 7);
}
unsigned char __cdecl StubScriptTick() { Record(44); Disturb(); return static_cast<unsigned char>(Hash() >> 9); }
unsigned char __cdecl StubScriptTickOnce() { Record(45); Disturb(); return static_cast<unsigned char>(Hash() >> 11); }
// Values at the drop chances' and the 70 % test's edges, and anything.
int __cdecl StubRand() {
    static const int kEdges[] = {0, 1, 2, 3, 4, 7, 8, 0x1F, 0x20, 0x7F, 0x80, 0xFE, 0xFF, 0x100, 0x101, 0x103,
                                 69, 70, 71, 169, 170, 0x7FFF, -1, -70, -71, 0x1FF};
    Record(46);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 3 == 0 ? static_cast<int>(h >> 17) : kEdges[(h >> 8) % (sizeof kEdges / sizeof kEdges[0])];
}
void __cdecl StubRemove(unsigned a) { Record(47, a & 0xFF); Disturb(); }
void __cdecl StubRollDrops() { Record(48); Disturb(); }
void __cdecl StubSetFlag(unsigned n) { Record(49, n & 0xFFFF); Disturb(); }
void __cdecl StubReleaseTint(unsigned char* p) { Record(50, Id(p)); Disturb(); }
void __cdecl StubClearTurn(unsigned n) { Record(51, n & 0xFF); Disturb(); }
void __cdecl StubLoadDat(int file) { Record(52, static_cast<std::uint32_t>(file)); Disturb(); }
// An index inside the slots mostly, past the 48 now and then (still inside
// the enemy objects - the original's 0xFF writes outside the image).
unsigned char __cdecl StubTaskCreate(unsigned kind, unsigned parameter) {
    Record(53, kind & 0xFF, parameter & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 8 == 0 ? 0x30 + (h >> 8) % 0x10 : (h >> 8) % at::kTaskCount);
}
// Battle_DrawNumber reads the low words of x and y, the low byte of the
// CLUT row and the low word of the value.
void __cdecl StubDrawNumber(int x, int y, unsigned clut, unsigned value) {
    Record(54, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, clut & 0xFF, value & 0xFFFF);
    Disturb();
}
// "%3d" as the CRT prints it three rounds in four; otherwise 0..6 characters
// of anything - spaces, digits, other bytes - and for an empty string a few
// more after its NUL (the original draws the first character before any NUL
// test). The text ends by 0x904BA0 + 0x20, which the fuzz keeps zero.
int __cdecl StubSprintf(char* dst, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const unsigned value = va_arg(ap, unsigned);
    va_end(ap);
    Record(55, Id(dst), Address(fmt), value);
    const std::uint32_t h = Hash();
    unsigned n = 0;
    if (h % 4 != 0) {
        char digits[12];
        unsigned k = 0, v = value;
        do { digits[k++] = static_cast<char>('0' + v % 10); v /= 10; } while (v && k < 10);
        while (k < 3) digits[k++] = ' ';
        while (k) dst[n++] = digits[--k];
        dst[n] = 0;
    } else {
        const unsigned len = (h >> 4) % 7;
        for (unsigned i = 0; i < len; ++i) {
            const unsigned c = (h >> (8 + i * 3)) % 4;
            dst[n++] = static_cast<char>(c == 0 ? ' ' : c == 1 ? '0' + (h >> i) % 10 : 1 + (h >> (i + 5)) % 0xFF);
        }
        dst[n] = 0;
        if (len == 0) {
            const unsigned more = 1 + (h >> 20) % 3;
            for (unsigned i = 1; i <= more; ++i) dst[i] = static_cast<char>(1 + (h >> (i * 4)) % 0xFF);
            dst[more + 1] = 0;
        }
    }
    Disturb();
    return static_cast<int>(n);
}
unsigned __cdecl StubGetTPage(unsigned tp, unsigned abr, int x, int y) {
    Record(56, tp, abr, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return Hash();   // the whole dword: the caller keeps 16 bits
}
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(57, Id(prim), static_cast<std::uint32_t>(dfe) | static_cast<std::uint32_t>(dtd) << 16, tpage, static_cast<std::uint32_t>(tw));
    SetLong(prim + 4, static_cast<std::int32_t>(0xE1000000u | (tpage & 0xFFFFu)));
    Disturb();
}
// The commit moves the packet cursor as the real one does - wrapping inside
// our pool - so that a cursor read too early or too late shows.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(58, slot, size, Id(Gfx_PacketNext));
    Gfx_PacketNext = g_prim + (static_cast<unsigned>(Gfx_PacketNext - g_prim) + (size & 0xFF)) % 0x100u;
    Disturb();
}
unsigned __cdecl StubGetClut(int x, int y) {
    Record(59, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    Disturb();
    return Hash();
}
// What the caller stored before it, then its code byte over one of them.
void __cdecl StubSetSprt(unsigned char* prim) {
    Record(60, Id(prim), Bytes(prim, 4, 0x1C));
    prim[7] = 0x64;
}

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x588F20: return f(&StubUpdateScreen);
    case 0x589330: return f(&StubEnsureAnimation);
    case 0x5893A0: return f(&StubScriptTick);
    case 0x589410: return f(&StubScriptTickOnce);
    case 0x5B93D2: return f(&StubRand);
    case 0x446650: return f(&StubRemove);
    case 0x437580: return f(&StubRollDrops);
    case 0x494ED0: return f(&StubSetFlag);
    case 0x454DC0: return f(&StubReleaseTint);
    case 0x446FD0: return f(&StubClearTurn);
    case 0x454590: return f(&StubLoadDat);
    case 0x435180: return f(&StubTaskCreate);
    case 0x444480: return f(&StubDrawNumber);
    case 0x5B9380: return f(&StubSprintf);
    case 0x5A79A0: return f(&StubGetTPage);
    case 0x5A77C0: return f(&StubDrawMode);
    case 0x461E50: return f(&StubCommit);
    case 0x5A79E0: return f(&StubGetClut);
    case 0x5A7710: return f(&StubSetSprt);
    default: bof3::Fatal("battle_flow: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

const Callees kStubs = {
    {&StubHandler<100>, &StubHandler<101>, &StubHandler<102>, &StubHandler<103>, &StubHandler<104>, &StubHandler<105>},
    {&StubHandler<110>, &StubHandler<111>, &StubHandler<112>, &StubHandler<113>},
    StubTaskCreate, StubRollDrops, StubDrawNumber,
    StubUpdateScreen, StubEnsureAnimation, StubScriptTick, StubScriptTickOnce, StubRand,
    StubRemove, StubSetFlag, StubReleaseTint, StubClearTurn, StubLoadDat, StubSprintf,
    StubGetTPage, StubDrawMode, StubCommit, StubGetClut, StubSetSprt,
};
// The enemy state table's first nine entries (states 0..7, and 8 for the
// paused path's state 7).
constexpr unsigned kStates = 9;
const Handler kStateStubs[kStates] = {&StubHandler<120>, &StubHandler<121>, &StubHandler<122>, &StubHandler<123>,
                                      &StubHandler<124>, &StubHandler<125>, &StubHandler<126>, &StubHandler<127>,
                                      &StubHandler<128>};

// The stack-built tables: the offset of each imm32 in the copy (capstone,
// 2026-09-23) and the handler it names.
struct Imm { std::uint32_t offset, value; };
constexpr Imm kPhaseImm[6] = {{0x0C, 0x42E470}, {0x1B, 0x42E990}, {0x23, 0x42F070},
                              {0x2B, 0x42F220}, {0x33, 0x4302B0}, {0x3B, 0x4311E0}};
constexpr Imm kTaskImm[4] = {{0x08, 0x4352A0}, {0x10, 0x435350}, {0x18, 0x4378B0}, {0x20, 0x4357D0}};

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    unsigned ret;   // 0 nothing to compare, 1 the low byte
    move_script::Table table;
};

constexpr Call kUpdateCalls[] = {{0x1D, 0x588F20}};
constexpr Call kAnimCalls[] = {{0x39, 0x589330}, {0x7B, 0x589330}, {0xBD, 0x589330}, {0x101, 0x589330}};
constexpr Call kTickCalls[] = {{0x21, 0x5893A0}};
constexpr Call kTickOnceCalls[] = {{0x21, 0x589410}};
constexpr Call kChanceCalls[] = {{0x57, 0x5B93D2}};
constexpr Call kDefeatCalls[] = {{0x5D, 0x446650}, {0x65, 0x437580}, {0xB1, 0x494ED0}, {0xBC, 0x454DC0}, {0xDF, 0x446FD0}};
constexpr Call kDropCalls[] = {{0x56, 0x5B93D2}};
constexpr Call kItemMagicCalls[] = {{0x26, 0x435180}};
constexpr Call kItemLoadCalls[] = {{0x33, 0x454590}};
constexpr Call kAbilityMagicCalls[] = {{0x22, 0x435180}, {0x66, 0x435180}};
constexpr Call kAbilityLoadCalls[] = {{0x25, 0x454590}};
constexpr Call kDigitsCalls[] = {{0x66, 0x444480}};
constexpr Call kNumberCalls[] = {{0x1E, 0x5B9380}, {0x33, 0x5A79A0}, {0x4C, 0x5A77C0}, {0x55, 0x461E50},
                                 {0x8B, 0x5A79E0}, {0xD8, 0x5A7710}, {0xE1, 0x461E50}};
constexpr Call kLabelCalls[] = {{0xE, 0x5A79A0}, {0x26, 0x5A77C0}, {0x2F, 0x461E50},
                                {0x4D, 0x5A79E0}, {0x9F, 0x5A7710}, {0xA8, 0x461E50}};

enum : unsigned {
    kPhaseDispatch, kTaskRunAll, kTaskCreate, kTaskFree, kTaskClear, kEnemyRunAll, kEnemyScreen, kSetAnimation,
    kScriptTick, kScriptTickOnce, kChance70, kDefeated, kRollDrops, kItemMagic, kItemLoad, kAbilityMagic,
    kAbilityLoad, kDigits, kNumber, kLabel, kActorIsOut, kCount
};

#define BF_C(name, base, size, calls, ret) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret, {}}
#define BF_P(name, base, size, ret) {name, base, size, nullptr, 0, ret, {}}
const Clone kClones[kCount] = {
    BF_P("Battle_PhaseDispatch", 0x42E400, 0x62, 0),
    BF_P("BattleTask_RunAll", 0x435110, 0x61, 0),
    BF_P("BattleTask_Create", 0x435180, 0x6A, 1),
    BF_P("BattleTask_FreeCurrent", 0x4351F0, 0x65, 0),
    BF_P("BattleTask_ClearAll", 0x435260, 0x35, 0),
    BF_P("BattleEnemy_RunAll", 0x435830, 0x6A, 0),
    BF_C("BattleEnemy_UpdateScreenAll", 0x4358A0, 0x2E, kUpdateCalls, 0),
    {"BattleEnemy_SetAnimation", 0x4358D0, 0x144, kAnimCalls, 4, 0, {0x18, 0x134, 4}},
    BF_C("BattleEnemy_ScriptTick", 0x436090, 0x29, kTickCalls, 1),
    BF_C("BattleEnemy_ScriptTickOnce", 0x4360C0, 0x29, kTickOnceCalls, 1),
    BF_C("BattleEnemy_Chance70", 0x436B50, 0x6F, kChanceCalls, 1),
    BF_C("Battle_EnemyDefeated", 0x437470, 0x105, kDefeatCalls, 0),
    BF_C("Battle_RollDrops", 0x437580, 0x117, kDropCalls, 0),
    BF_C("Battle_StartItemMagic", 0x437780, 0x46, kItemMagicCalls, 0),
    BF_C("Magic_LoadForItem", 0x4377D0, 0x43, kItemLoadCalls, 0),
    BF_C("Battle_StartAbilityMagic", 0x437930, 0x9B, kAbilityMagicCalls, 0),
    BF_C("Magic_LoadForAbility", 0x4379D0, 0x35, kAbilityLoadCalls, 0),
    BF_C("BattleFx_RollingDigits", 0x432F10, 0x7A, kDigitsCalls, 0),
    BF_C("Battle_DrawNumber", 0x444480, 0x119, kNumberCalls, 0),
    BF_C("Battle_DrawLabel", 0x4445A0, 0xB2, kLabelCalls, 0),
    BF_P("Battle_ActorIsOut", 0x4456C0, 0x66, 1),
};
#undef BF_C
#undef BF_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kTasks, 0x22A0},        // the 48 slots, 0x93B8C4, 0x93B940, the eight enemy objects
    {0x937F88, 4},               // Sprite_Current
    {at::kEnemyCurrent, 4},
    {at::kPhase, 0x260},         // the battle's globals, the drop list, the pause hook, the text; to 0x904D00
    {at::kMembers, 0x3E4},       // ObjTrio
    {0x64B284, 0x1C0},           // constant data from here on - random here, put back after: the item rows 0..2
    {0x675ED8, 0x100},           // the item rows 3
    {at::kAbilityRows, 0x8E8},   // the ability rows and the magic files
    {0x65C4D0, 0x1800},          // the ability records' flag bytes
};
constexpr unsigned kRegionBytes = 0x22A0 + 4 + 4 + 0x260 + 0x3E4 + 0x1C0 + 0x100 + 0x8E8 + 0x1800;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char prim[kPool];
    unsigned char anim[kAnim];
    std::uint32_t packet;   // Gfx_PacketNext, as an offset into the pool
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.prim, g_prim, sizeof g_prim);
    std::memcpy(s.anim, g_anim, sizeof g_anim);
    s.packet = static_cast<std::uint32_t>(Gfx_PacketNext - g_prim);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_prim, s.prim, sizeof g_prim);
    std::memcpy(g_anim, s.anim, sizeof g_anim);
    Gfx_PacketNext = g_prim + s.packet;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
unsigned Byte(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}

std::uint32_t g_rng = 0x3C6EF372u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }

// Random bytes put back inside what the tables and buffers hold: the three
// current pointers, every slot's kind, every enemy's state, hook and table
// pointers, the phase index, the text's end.
void Fix() {
    SetPtr(at::kTaskCurrent, Slot(Next() % at::kTaskCount));
    Sprite_Current = EnemyObj(Next() % 8);
    SetPtr(at::kEnemyCurrent, EnemyObj(Next() % 8));
    for (unsigned i = 0; i < at::kTaskCount; ++i) Slot(i)[6] = static_cast<unsigned char>(Next() % 4);
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const e = EnemyObj(i);
        e[0x100] = static_cast<unsigned char>(Next() % 8);
        SetPtr(Address(e + 0xF4), reinterpret_cast<const void*>(&StubEnemyHook));
        SetPtr(Address(e + 0xFC), g_anim + i * 2);   // each its own offset: a table read through the wrong enemy shows
        if (Half()) e[8] = static_cast<unsigned char>(Next() % 5);
        e[0xAA] = static_cast<unsigned char>(Next() % 8);   // drop classes inside the chance table: a class
        e[0xAE] = static_cast<unsigned char>(Next() % 8);   // above 7 reads the original's stack (ours aborts)
    }
    At(at::kPhase)[0] = static_cast<unsigned char>(Next() % 6);
    SetPtr(at::kPauseHook, reinterpret_cast<const void*>(&StubPauseHook));
    SetWord(At(at::kMagicId), Next() & 0xFF);
    At(at::kNumberText + 0x20)[0] = 0;
    if (Half()) At(at::kPaused)[0] = 0;
    if (Half()) At(at::kAnimGate)[0] = 0;
}

struct Args { std::uint32_t a[4]; };

unsigned char* Current() { return move_script::At(static_cast<std::uint32_t>(Long(At(at::kEnemyCurrent)))); }

// Each branch's boundaries, on top of the random bytes.
Args Seed(unsigned k) {
    Args args;
    for (std::uint32_t& v : args.a) v = Next();
    switch (k) {
    case kPhaseDispatch:
        SetLong(At(at::kPhase), static_cast<std::int32_t>(Garbage(0xFF, Next() % 6)));
        break;
    case kTaskRunAll:
        for (unsigned i = 0; i < at::kTaskCount; ++i) {
            unsigned char* const t = Slot(i);
            t[0] = static_cast<unsigned char>(Half() ? 0 : Half() ? 0x40 : Next() | 1);
        }
        break;
    case kTaskCreate: {
        // the first free slot anywhere, or none
        const unsigned taken = Next() % 3 == 0 ? at::kTaskCount : Next() % at::kTaskCount;
        for (unsigned i = 0; i < at::kTaskCount; ++i) {
            unsigned char* const t = Slot(i);
            if (i < taken) t[0] = static_cast<unsigned char>(t[0] | 1);
            else if (Half()) t[0] = static_cast<unsigned char>(t[0] & ~1u);
        }
        args.a[0] = Garbage(0xFF, Next() % 4);
        args.a[1] = Garbage(0xFF, Next() & 0xFF);
        break;
    }
    case kEnemyRunAll:
    case kEnemyScreen:
        At(at::kPaused)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const e = EnemyObj(i);
            if (Half()) e[0] = 0;
            if (Half()) e[1] = 0;
        }
        break;
    case kSetAnimation:
        Sprite_Current[8] = static_cast<unsigned char>(Often() ? Next() % 4 : 4 + Next() % 0xFC);
        args.a[0] = Garbage(0xFF, Half() ? 0xFF - Next() % 2 : Next() % 0x100);
        break;
    case kScriptTick:
    case kScriptTickOnce:
    case kChance70: {
        unsigned char* const e = Current();
        if (Often()) e[0x92] = static_cast<unsigned char>(e[0x92] & ~0x64u);
        if (Often()) e[0x93] = static_cast<unsigned char>(e[0x93] & ~0x40u);
        if (Half()) At(at::kAnimGate)[0] = 0;
        if (Half()) e[0x114] = static_cast<unsigned char>(e[0x114] | 0x10);
        static const unsigned char kTurn[] = {0, 1, 2, 3, 4, 0xFF};
        At(at::kTurnGate)[0] = kTurn[Next() % 6];
        if (Half()) At(at::kFormation)[0] = 4;
        if (Half()) SetWord(At(at::kMagicId), Half() ? 0xA1 : 0x1A1);
        if (Often()) e[0x90] = static_cast<unsigned char>(e[0x90] | 2);
        if (Half()) e[0x111] = static_cast<unsigned char>(e[0x111] & 0x7F);
        if (k == kChance70 && Often()) {
            // every gate open, then at most one of them shut again: each
            // gate's edge, and the roll itself, in most rounds
            e[0x92] = static_cast<unsigned char>(e[0x92] & ~0x64u);
            e[0x93] = static_cast<unsigned char>(e[0x93] & ~0x40u);
            At(at::kTurnGate)[0] = static_cast<unsigned char>(Next() % 3);
            if (At(at::kFormation)[0] == 4 && Half()) At(at::kFormation)[0] = 5;
            else SetWord(At(at::kMagicId), Half() ? 0xA0 : 0x1A1);
            if (Half()) At(at::kAnimGate)[0] = 0;
            else e[0x114] = static_cast<unsigned char>(e[0x114] | 0x10);
            e[0x90] = static_cast<unsigned char>(e[0x90] | 2);
            if (Often()) e[0x111] = static_cast<unsigned char>(e[0x111] & 0x7F);
            switch (Next() % 12) {
            case 0: e[0x92] = static_cast<unsigned char>(e[0x92] | (Half() ? 4 : Half() ? 0x20 : 0x40)); break;
            case 1: e[0x93] = static_cast<unsigned char>(e[0x93] | 0x40); break;
            case 2: At(at::kTurnGate)[0] = 3; break;
            case 3: At(at::kFormation)[0] = 4; SetWord(At(at::kMagicId), 0xA1); break;
            case 4: At(at::kAnimGate)[0] = 1; e[0x114] = static_cast<unsigned char>(e[0x114] & ~0x10u); break;
            case 5: e[0x90] = static_cast<unsigned char>(e[0x90] & ~2u); break;
            default: break;
            }
        }
        break;
    }
    case kDefeated: {
        At(at::kEnemiesLeft)[0] = static_cast<unsigned char>(Next() % 3);
        if (Half()) At(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(At(at::kRoundFlags + 1)[0] | 0x40);
        if (Half()) At(at::kActorAt)[0] = Sprite_Current[5];
        if (Half()) SetLong(At(at::kExpTotal), static_cast<std::int32_t>(0xFFFFFFFFu - Next() % 0x100));
        break;
    }
    case kRollDrops: {
        unsigned char* const e = Current();
        for (unsigned slot = 0xA8; slot < 0xB0; slot += 4) {
            SetWord(e + slot, Next() % 4 == 0 ? 0 : Often() ? Next() % 5 : Next());
            e[slot + 2] = static_cast<unsigned char>(Next() % 4 == 0 ? 0 : Next() % 8);
        }
        static const unsigned char kCounts[] = {0, 0, 1, 2, 3, 5, 14, 15, 16};
        At(at::kDropCount)[0] = Often() ? kCounts[Next() % 9] : static_cast<unsigned char>(Next() % 17);
        for (unsigned i = 0; i < 16; ++i)
            if (Often()) SetWord(At(at::kDropItems + i * 2), Next() % 5);
        break;
    }
    case kItemMagic:
    case kItemLoad: {
        args.a[0] = Garbage(0xFFFF, (Next() % 4) << 8 | (Next() & 0xFF));
        if (Half()) {
            // the row this id reaches: its file none
            const unsigned w = args.a[0] & 0xFFFF;
            const auto* const tables = reinterpret_cast<const unsigned char* const*>(static_cast<std::uintptr_t>(at::kItemRows));
            SetWord(At(at::kMagicFiles + tables[w >> 8][w & 0xFF] * 8), 0xFFFF);
        }
        break;
    }
    case kAbilityMagic:
    case kAbilityLoad: {
        args.a[0] = Garbage(0xFF, Next() & 0xFF);
        if (Half()) SetWord(At(at::kMagicFiles + At(at::kAbilityRows)[args.a[0] & 0xFF] * 8), 0xFFFF);
        break;
    }
    case kDigits: {
        unsigned char* const s = move_script::At(static_cast<std::uint32_t>(Long(At(at::kTaskCurrent))));
        static const unsigned char kCount[] = {0xFF, 0, 1, 2, 3, 5, 0x80, 0xFE};
        s[0xA] = kCount[Next() % 8];
        static const std::uint16_t kPhases[] = {0, 1, 8, 9, 10, 0xFFFF, 0xFFF6, 0xFFF7, 0x7FFF, 0x8000};
        if (Often()) SetWord(s + 0x32, kPhases[Next() % 10]);
        break;
    }
    case kNumber:
        args.a[3] = Often() ? Garbage(0xFFFF, Next() % 1000) : Next();
        for (unsigned i = 0; i < 0x20; ++i) At(at::kNumberText + i)[0] = static_cast<unsigned char>(Next() % 5 == 0 ? 0 : Next());
        break;
    case kActorIsOut: {
        args.a[0] = Garbage(0xFF, Next() % 11);
        const unsigned a = args.a[0] & 0xFF;
        unsigned char* const r = a < 3 ? At(at::kMembers + a * at::kMemberSize) : EnemyObj(a - 3);
        if (Often()) r[0] = static_cast<unsigned char>(r[0] | 1);
        if (Half()) r[a < 3 ? 0x91 : 0x93] = static_cast<unsigned char>(r[a < 3 ? 0x91 : 0x93] & ~0x40u);
        break;
    }
    default:
        break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[140];
    unsigned ones[kCount], zeros[kCount];
    unsigned drops, merged, appended, battle_end, flag_cleared, created_none, second_task, spaces, texts;
    unsigned early[2], chance_rolled;
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 140) ++g_cover.logged[out.log[i].what];
    if (kClones[k].ret) ++(out.result & 0xFF ? g_cover.ones : g_cover.zeros)[k];
    switch (k) {
    case kScriptTick:
    case kScriptTickOnce:
        if (out.log_n == 0) ++g_cover.early[k - kScriptTick];
        break;
    case kChance70:
        if (out.log_n != 0) ++g_cover.chance_rolled;
        break;
    case kTaskCreate:
        if ((out.result & 0xFF) == 0xFF) ++g_cover.created_none;
        break;
    case kRollDrops: {
        const unsigned before = Byte(in, at::kDropCount), after = Byte(out, at::kDropCount);
        bool merged = false;
        for (unsigned i = 0; i < before && i < 16; ++i) if (Byte(out, at::kDropCounts + i) != Byte(in, at::kDropCounts + i)) merged = true;
        if (after != before || Byte(out, at::kDropCounts + before) == 1) ++g_cover.appended;
        if (merged) ++g_cover.merged;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 46) ++g_cover.drops;
        break;
    }
    case kDefeated:
        if ((Byte(out, at::kBattleEnd) & 2) && !(Byte(in, at::kBattleEnd) & 2)) ++g_cover.battle_end;
        if ((Byte(in, at::kRoundFlags + 1) & 0x40) && !(Byte(out, at::kRoundFlags + 1) & 0x40)) ++g_cover.flag_cleared;
        break;
    case kAbilityMagic:
        if (out.log_n >= 2 && out.log[1].what == 53) ++g_cover.second_task;
        break;
    case kNumber: {
        unsigned drawn = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 60) ++drawn;
        unsigned chars = 0;
        while (chars < 0x20 && Byte(out, at::kNumberText + chars) != 0) ++chars;
        if (drawn) ++g_cover.texts;
        for (unsigned i = 0; i < 8; ++i) if (Byte(out, at::kNumberText + i) == ' ') { ++g_cover.spaces; break; }
        break;
    }
    default:
        break;
    }
}

using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

void PatchImms(void* copy, const char* name, const Imm* imms, unsigned n, const Handler* to) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (unsigned i = 0; i < n; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + imms[i].offset, sizeof had);
        if (had != imms[i].value)
            bof3::Fatal("battle_flow: %s +0x%X holds 0x%X, not the handler 0x%X", name, static_cast<unsigned>(imms[i].offset),
                        static_cast<unsigned>(had), static_cast<unsigned>(imms[i].value));
        const std::uint32_t target = Address(reinterpret_cast<const void*>(to[i]));
        std::memcpy(code + imms[i].offset, &target, sizeof target);
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_flow: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[8];
        if (c.n_calls > 8) bof3::Fatal("battle_flow: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.table.entries) move_script::Relocate(clones[k], c.base, c.size, c.table);
    }
    PatchImms(clones[kPhaseDispatch], "Battle_PhaseDispatch", kPhaseImm, 6, kStubs.phases);
    PatchImms(clones[kTaskRunAll], "BattleTask_RunAll", kTaskImm, 4, kStubs.tasks);

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Battle_PhaseDispatch), reinterpret_cast<const void*>(&BattleTask_RunAll),
        reinterpret_cast<const void*>(&BattleTask_Create), reinterpret_cast<const void*>(&BattleTask_FreeCurrent),
        reinterpret_cast<const void*>(&BattleTask_ClearAll), reinterpret_cast<const void*>(&BattleEnemy_RunAll),
        reinterpret_cast<const void*>(&BattleEnemy_UpdateScreenAll), reinterpret_cast<const void*>(&BattleEnemy_SetAnimation),
        reinterpret_cast<const void*>(&BattleEnemy_ScriptTick), reinterpret_cast<const void*>(&BattleEnemy_ScriptTickOnce),
        reinterpret_cast<const void*>(&BattleEnemy_Chance70), reinterpret_cast<const void*>(&Battle_EnemyDefeated),
        reinterpret_cast<const void*>(&Battle_RollDrops), reinterpret_cast<const void*>(&Battle_StartItemMagic),
        reinterpret_cast<const void*>(&Magic_LoadForItem), reinterpret_cast<const void*>(&Battle_StartAbilityMagic),
        reinterpret_cast<const void*>(&Magic_LoadForAbility), reinterpret_cast<const void*>(&BattleFx_RollingDigits),
        reinterpret_cast<const void*>(&Battle_DrawNumber), reinterpret_cast<const void*>(&Battle_DrawLabel),
        reinterpret_cast<const void*>(&Battle_ActorIsOut)};

    static State saved, input, their_out, our_out;
    unsigned char* const saved_packet = Gfx_PacketNext;
    std::uint32_t saved_states[kStates];
    std::memcpy(saved_states, At(at::kEnemyStates), sizeof saved_states);
    Gfx_PacketNext = g_prim;
    Capture(saved);
    g = kStubs;
    for (unsigned i = 0; i < kStates; ++i) SetPtr(at::kEnemyStates + 4 * i, reinterpret_cast<const void*>(kStateStubs[i]));

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.prim) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.anim) b = static_cast<unsigned char>(Next());
        input.packet = Next() % 0x100u;
        input.result = 0;
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        const Args args = Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? ours[k] : clones[k];
            const std::uint32_t r = reinterpret_cast<Fn4>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2], args.a[3]);
            Capture(out);
            out.result = kClones[k].ret == 0 ? 0u : (r & 0xFFu);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_flow self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    std::memcpy(At(at::kEnemyStates), saved_states, sizeof saved_states);
    Apply(saved);
    Gfx_PacketNext = saved_packet;

    bof3::Log("shadow      battle_flow self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the task slots, the enemy objects, the battle's globals, the party records, the tables, "
              "the primitive pool, the animation table, the packet cursor, the result and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_flow: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned phases = 0, kinds = 0, states = 0;
    for (unsigned i = 100; i < 106; ++i) phases += c.logged[i] ? 1u : 0u;
    for (unsigned i = 110; i < 114; ++i) kinds += c.logged[i] ? 1u : 0u;
    for (unsigned i = 120; i < 129; ++i) states += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      battle_flow coverage: phases %u of 6, task kinds %u of 4, enemy state entries %u of 9, pause "
              "hook %u, enemy hook %u, screen updates %u, animations %u; ticks %u / %u (early 1: %u / %u rounds), chance 1 %u "
              "0 %u (rand reached in %u); created none %u; kills: battle end %u, flag cleared %u; drops rolled %u, merged %u, "
              "appended %u; files %u, second tasks %u; digits drawn %u; texts drawn %u (with a space %u), sprites %u; "
              "actor out %u in %u",
              phases, kinds, states, c.logged[40], c.logged[41], c.logged[42], c.logged[43], c.logged[44], c.logged[45],
              c.early[0], c.early[1], c.ones[kChance70], c.zeros[kChance70], c.chance_rolled,
              c.created_none, c.battle_end, c.flag_cleared, c.drops, c.merged, c.appended, c.logged[52], c.second_task,
              c.logged[54], c.texts, c.spaces, c.logged[60], c.ones[kActorIsOut], kPerFunction);
    if (bad) bof3::Fatal("the battle task and the turn flow differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_flow

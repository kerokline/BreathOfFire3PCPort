// BOF3X_SHADOW=event_objs: the start-up differential fuzz of event_objs.cpp's
// 22 functions (docs/event-objs.md, "The fuzz").
//
// Each original is byte-copied with EVERY call re-aimed at a recording
// stand-in - the calls between this module's own functions included, so each
// function is tested alone - and ours runs with the same stand-ins through
// event_objs::g. A round: random state with each branch's boundaries seeded,
// theirs, the same state again, ours; every byte of the state, the stand-ins'
// log (a count, a hash of every entry, the first 32 kept) and the result's
// low byte (every caller reads al; the originals leave the rest of eax
// holding whatever it held) compared. The stand-ins record only what the real
// callee reads of its arguments - the originals push bytes with stale upper
// bytes - give back what the real callee leaves for the caller to read
// (DamageScratch's slope byte, ax, al), and now and then disturb what the
// caller reads again after the call: Sprite_Current and Field_State (each
// swapped between two buffers), their fields, the flag bytes.
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/event_objs_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_objs {
namespace {

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* At(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetWord(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void SetLong(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }

std::uint32_t g_rng = 0x6B43A9B5u;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool OneIn(unsigned n) { return Next() % n == 0; }
unsigned char Pick(std::initializer_list<unsigned> seeds) {
    const unsigned i = Next() % (seeds.size() + 1);
    return static_cast<unsigned char>(i < seeds.size() ? seeds.begin()[i] : Next());
}
// A byte argument with stale upper bytes, as Capcom's callers push them.
std::uint32_t Stale(unsigned char b) { return (Next() & 0xFFFFFF00u) | b; }

// --- the buffers Sprite_Current and Field_State point at -------------------------
constexpr unsigned kSprite = 0x100, kField = 0x200;
unsigned char g_sprite[2][kSprite], g_field[2][kField];

// --- the stand-ins' log ----------------------------------------------------------
constexpr unsigned kKeep = 32, kIds = 32;
struct Log {
    std::uint32_t n, hash;
    std::uint32_t keep[kKeep][5];
};
Log g_log;
unsigned g_counts[kIds];
std::uint32_t g_seed;
unsigned char g_code;   // the code of the round: what AreaMap_ByteAt's stand-in answers most

void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log.n < kKeep) {
        g_log.keep[g_log.n][0] = what;
        g_log.keep[g_log.n][1] = a;
        g_log.keep[g_log.n][2] = b;
        g_log.keep[g_log.n][3] = c;
        g_log.keep[g_log.n][4] = d;
    }
    for (const std::uint32_t v : {what, a, b, c, d}) g_log.hash = (g_log.hash ^ v) * 0x01000193u + 0x9E3779B9u;
    ++g_log.n;
    ++g_counts[what % kIds];
}
// Deterministic in the call's position: the same on both sides while the
// calls agree.
std::uint32_t Hash(unsigned salt = 0) {
    std::uint32_t h = (g_seed + g_log.n * 0x10001u + salt * 0x3C6EF372u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

// What a callee may change that the caller reads again after it. Never +9 of
// either sprite: Field_JumpSetUp divides by it after its calls.
void Disturb() {
    const std::uint32_t h = Hash(7);
    if (h % 5 == 0) Sprite_Current = g_sprite[(h >> 3) & 1];
    if (h % 7 == 0) Field_State = g_field[(h >> 4) & 1];
    if (h % 11 == 0) Sprite_Current[5] = static_cast<unsigned char>(h >> 8);
    if (h % 13 == 0) Sprite_Current[8] = static_cast<unsigned char>(h >> 12);
    if (h % 17 == 0) Field_InputFlags = static_cast<unsigned char>(h >> 16);
    if (h % 19 == 0) Field_ScriptFlags = static_cast<unsigned short>(h >> 9);
    if (h % 23 == 0) Field_State[0x124] = static_cast<unsigned char>(h >> 20);
    if (h % 29 == 0) Field_State[0x148] = static_cast<unsigned char>(h >> 21);
    if (h % 31 == 0) Field_State[0x89] = static_cast<unsigned char>(h >> 22);
    if (h % 37 == 0) Field_ScriptFlags2 = static_cast<unsigned short>(h >> 5);
}

// --- the stand-ins -----------------------------------------------------------------
void __cdecl StubJumpSetUp() { Record(1); Disturb(); }
void __cdecl StubJumpCamera() { Record(2); }
void __cdecl StubJumpStart() { Record(3); }
// 0x5725C0: the slope byte in DamageScratch is what its callers test.
long __cdecl StubSlope(long x, long z, unsigned direction) {
    Record(4, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), direction & 0xFF);
    const std::uint32_t h = Hash();
    At(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 24);
    Disturb();
    return static_cast<long>(Hash(1));
}
// The ground near the object's height, either side of the 0x80 its callers test.
long __cdecl StubGround(long x, long z) {
    Record(5, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    const std::uint32_t h = Hash();
    const int deltas[] = {0x7F, 0x80, 0x81, 0, -1, -0x80, 0x7FFF, static_cast<short>(h >> 16)};
    const unsigned height = Word(Sprite_Current + 0x3E);
    Disturb();
    return static_cast<long>((h & 0xFFFF0000u) | ((height + static_cast<unsigned>(deltas[h % 8])) & 0xFFFFu));
}
unsigned char __cdecl StubShadeRaise(unsigned step) {
    Record(6, step & 0xFF);
    const std::uint32_t h = Hash();
    Disturb();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 24);
}
void __cdecl StubLoadPalette(unsigned short* dst, unsigned index) {
    Record(7, Address(dst), index);
    Disturb();
}
// The answer of a cells test: mostly no, a one or any non-zero byte.
unsigned char CellsAnswer(unsigned out_of) {
    const std::uint32_t h = Hash();
    if (h % out_of != 0) return 0;
    return static_cast<unsigned char>((h >> 8) % 2 ? 1 : ((h >> 16) | 1));
}
unsigned char __cdecl StubCellsAll(long x, long z, unsigned wide, unsigned code, unsigned mask) {
    Record(8, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), wide, (code & 0xFF) | (mask & 0xFF) << 8);
    const unsigned char r = CellsAnswer(5);
    Disturb();
    return r;
}
unsigned char __cdecl StubCellsAll4(long x, long z, unsigned code, unsigned mask) {
    Record(9, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), code & 0xFF, mask & 0xFF);
    return CellsAnswer(2);
}
unsigned char __cdecl StubCellsAllWide(long x, long z, unsigned code, unsigned mask) {
    Record(10, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), code & 0xFF, mask & 0xFF);
    return CellsAnswer(2);
}
unsigned char __cdecl StubCellsNone(long x, long z, unsigned wide, unsigned code, unsigned mask) {
    Record(11, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), wide, (code & 0xFF) | (mask & 0xFF) << 8);
    const unsigned char r = CellsAnswer(3);
    Disturb();
    return r;
}
unsigned char __cdecl StubCellsNone4(long x, long z, unsigned code, unsigned mask) {
    Record(12, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), code & 0xFF, mask & 0xFF);
    return CellsAnswer(2);
}
unsigned char __cdecl StubCellsNoneWide(long x, long z, unsigned code, unsigned mask) {
    Record(13, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), code & 0xFF, mask & 0xFF);
    return CellsAnswer(2);
}
// AreaMap_ByteAt reads both arguments as 16 bits. Mostly the round's code, or
// it with another low nibble, or a 0x2n cell.
unsigned char __cdecl StubByteAt(short x, short y) {
    Record(14, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y));
    const std::uint32_t h = Hash();
    switch (h % 8) {
    case 0: case 1: case 2: case 3: return g_code;
    case 4: return static_cast<unsigned char>(g_code ^ ((h >> 8) & 0xF));
    case 5: return static_cast<unsigned char>(0x20 | ((h >> 8) & 0xF));
    case 6: return static_cast<unsigned char>(g_code ^ ((h >> 8) & 0xF0));
    default: return static_cast<unsigned char>(h >> 24);
    }
}
void __cdecl StubFloorHurt(unsigned kind) { Record(15, kind & 0xFF); Disturb(); }
// Field_Bit80Tick reads Field_InputFlags and Field_State again after it.
void __cdecl StubFlash(unsigned n) {
    Record(16, n & 0xFF);
    const std::uint32_t h = Hash(3);
    if (h % 2) Field_State = g_field[(h >> 1) & 1];
    if (h % 3 == 0) Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ 1);
    Disturb();
}
// Party_Count: its callers read al.
int __cdecl StubPartyCount(unsigned slot) {
    Record(17, slot);
    const std::uint32_t h = Hash();
    const unsigned char counts[] = {0, 1, 2, 3, 3, 4, 6, 0x80, 0xFF, 1};
    return static_cast<int>((h & 0xFFFFFF00u) | counts[h % 10]);
}
// 0x537480: its callers test ax.
unsigned __cdecl StubHpLose(unsigned amount, unsigned member) {
    Record(18, amount, member & 0xFF);
    const std::uint32_t h = Hash();
    Disturb();
    return h % 2 ? (h & 0xFFFF0000u) : h;
}
void __cdecl StubHpGain(unsigned amount, unsigned member) { Record(19, amount, member & 0xFF); Disturb(); }
unsigned char __cdecl StubEffectFree() {
    Record(20);
    const std::uint32_t h = Hash();
    switch (h % 4) {
    case 0: return 0xFF;
    case 1: return static_cast<unsigned char>(h >> 8);
    default: return static_cast<unsigned char>((h >> 8) % 20);
    }
}
unsigned char __cdecl StubTileTurn(unsigned code, unsigned value, unsigned turn) {
    Record(21, code, value, turn);
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 ? 0 : h >> 24);
}
// 0x535610 reads its fourth argument as a word.
unsigned char __cdecl StubTurnProbe(long x, long z, unsigned zero, unsigned height) {
    Record(22, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), zero, height & 0xFFFF);
    const std::uint32_t h = Hash();
    Disturb();
    return static_cast<unsigned char>(h % 8 < 6 ? (h >> 24) | 1 : 0);
}
unsigned char __cdecl StubEquipCount(unsigned member, unsigned kind, unsigned value) {
    Record(23, member & 0xFF, kind & 0xFF, value & 0xFF);
    const std::uint32_t h = Hash();
    Disturb();
    return static_cast<unsigned char>(h % 2 ? 0 : h >> 24);
}

const Callees kStubs = {
    StubJumpSetUp, StubJumpCamera, StubJumpStart, StubSlope, StubGround, StubShadeRaise, StubLoadPalette,
    StubCellsAll, StubCellsAll4, StubCellsAllWide, StubCellsNone, StubCellsNone4, StubCellsNoneWide, StubByteAt,
    StubFloorHurt, StubFlash, StubPartyCount, StubHpLose, StubHpGain, StubEffectFree, StubTileTurn, StubTurnProbe,
    StubEquipCount,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x534610: return f(kStubs.jump_setup);
    case 0x534710: return f(kStubs.jump_camera);
    case 0x5345E0: return f(kStubs.jump_start);
    case kSlopeAt: return f(kStubs.slope_at);
    case 0x572570: return f(kStubs.ground_at);
    case 0x534800: return f(kStubs.shade_raise);
    case 0x5366A0: return f(kStubs.load_palette);
    case 0x535390: return f(kStubs.cells_all);
    case 0x5353E0: return f(kStubs.cells_all4);
    case kCellsAllWide: return f(kStubs.cells_all_wide);
    case 0x535C50: return f(kStubs.cells_none);
    case 0x535CA0: return f(kStubs.cells_none4);
    case kCellsNoneWide: return f(kStubs.cells_none_wide);
    case 0x536700: return f(kStubs.byte_at);
    case kFloorHurt: return f(kStubs.floor_hurt);
    case kFlash: return f(kStubs.flash);
    case 0x531BB0: return f(kStubs.party_count);
    case kHpLose: return f(kStubs.hp_lose);
    case kHpGain: return f(kStubs.hp_gain);
    case 0x589810: return f(kStubs.effect_free);
    case 0x535150: return f(kStubs.tile_turn);
    case kTurnProbe: return f(kStubs.turn_probe);
    case 0x535310: return f(kStubs.equip_count);
    default: bof3::Fatal("event_objs: no stand-in for a call to 0x%X", static_cast<unsigned>(target));
    }
}

// --- the copies -------------------------------------------------------------------
// Extents and calls out by capstone, 2026-09-23: each body to its last
// instruction; every jump stays inside; every call (and 0x5345E0's tail jmp)
// leaves and is re-aimed.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; unsigned args; bool boolean; };
constexpr Call kJumpStartCalls[] = {{0x0, 0x534610}, {0x24, 0x534710}};
constexpr Call kJumpSetUpCalls[] = {{0xBC, kSlopeAt}, {0xD0, 0x572570}};
constexpr Call kShadeStepCalls[] = {{0x5, 0x534800}, {0x2A, 0x5366A0}};
constexpr Call kTileCalls[] = {{0x2B, 0x535390}};
constexpr Call kFloorCalls[] = {{0x5A, 0x535390}, {0x8C, 0x535390}, {0xBF, 0x535390}, {0xF2, 0x535390},
                                {0x125, 0x535390}, {0x158, 0x535390}, {0x18B, 0x535390}, {0x1BB, 0x535390},
                                {0x1EB, 0x535390}, {0x202, kFloorHurt}, {0x212, kFlash}};
constexpr Call kBit80Calls[] = {{0x8, 0x531BB0}, {0x9A, kFlash}, {0xE3, kHpLose}, {0x129, kHpLose}, {0x13C, 0x589810}};
constexpr Call kTile8xCalls[] = {{0x19, 0x535150}};
constexpr Call kTileTurnCalls[] = {{0x1E, 0x535C50}, {0x86, kTurnProbe}};
constexpr Call kEquipTickCalls[] = {{0x10, 0x535310}, {0x2B, kHpGain}, {0x44, 0x535310},
                                    {0x5E, kHpGain}, {0x77, 0x535310}, {0x92, kHpGain}};
constexpr Call kCellsAllCalls[] = {{0x1C, 0x5353E0}, {0x39, kCellsAllWide}};
constexpr Call kCellsNoneCalls[] = {{0x1C, 0x535CA0}, {0x39, kCellsNoneWide}};
constexpr Call kCells4Calls[] = {{0xE, 0x536700}, {0x1C, 0x536700}, {0x28, 0x536700}, {0x33, 0x536700}};
constexpr Call kCheckCalls[] = {{0x28, kSlopeAt}, {0x3B, 0x572570}, {0x65, 0x5345E0}};

enum Fn : unsigned {
    kShadeBegin, kJumpStart, kJumpSetUp, kJumpCamera, kShadeStep, kShadeRaise, kTileD0, kTileA4, kFloorDamage,
    kBit80, kBit20, kTile89, kTileTurn, kTile8A, kEquipTick, kEquipCount, kCellsAll, kCellsAll4, kCellsNone,
    kCellsNone4, kJumpCheck, kApplyVelocity, kCount
};
#define CALLS(a) a, static_cast<int>(sizeof a / sizeof a[0])
const Clone kClones[kCount] = {
    {"Sprite_ShadeFadeBegin", 0x534590, 0x4C, nullptr, 0, 0, false},
    {"Field_JumpStart", 0x5345E0, 0x2A, CALLS(kJumpStartCalls), 0, false},
    {"Field_JumpSetUp", 0x534610, 0xF6, CALLS(kJumpSetUpCalls), 0, false},
    {"Field_JumpCamera", 0x534710, 0x79, nullptr, 0, 0, false},
    {"Sprite_ShadeFadeStep", 0x534790, 0x67, CALLS(kShadeStepCalls), 1, true},
    {"Sprite_ShadeRaise", 0x534800, 0x7E, nullptr, 0, 1, true},
    {"Field_TileD0", 0x534920, 0x62, CALLS(kTileCalls), 0, true},
    {"Field_TileA4", 0x534990, 0x62, CALLS(kTileCalls), 0, true},
    {"Field_FloorDamage", 0x534A00, 0x220, CALLS(kFloorCalls), 0, false},
    {"Field_Bit80Tick", 0x534F10, 0x1A4, CALLS(kBit80Calls), 0, false},
    {"Field_Bit20Tick", 0x5350C0, 0x56, nullptr, 0, 0, false},
    {"Field_Tile89", 0x535120, 0x27, CALLS(kTile8xCalls), 1, true},
    {"Field_TileTurn", 0x535150, 0xE7, CALLS(kTileTurnCalls), 3, true},
    {"Field_Tile8A", 0x535240, 0x27, CALLS(kTile8xCalls), 1, true},
    {"Field_EquipTick", 0x535270, 0x9B, CALLS(kEquipTickCalls), 0, false},
    {"Actor_EquipCount", 0x535310, 0x79, nullptr, 0, 3, true},
    {"AreaMap_CellsAll", 0x535390, 0x42, CALLS(kCellsAllCalls), 5, true},
    {"AreaMap_CellsAll4", 0x5353E0, 0xA2, CALLS(kCells4Calls), 4, true},
    {"AreaMap_CellsNone", 0x535C50, 0x42, CALLS(kCellsNoneCalls), 5, true},
    {"AreaMap_CellsNone4", 0x535CA0, 0xBE, CALLS(kCells4Calls), 4, true},
    {"Field_JumpCheckHeight", 0x535F50, 0x6D, CALLS(kCheckCalls), 0, false},
    {"Sprite_ApplyVelocity", 0x536670, 0x2E, nullptr, 0, 0, false},
};
#undef CALLS

const void* Ours(unsigned k) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (k) {
    case kShadeBegin: return f(&Sprite_ShadeFadeBegin);
    case kJumpStart: return f(&Field_JumpStart);
    case kJumpSetUp: return f(&Field_JumpSetUp);
    case kJumpCamera: return f(&Field_JumpCamera);
    case kShadeStep: return f(&Sprite_ShadeFadeStep);
    case kShadeRaise: return f(&Sprite_ShadeRaise);
    case kTileD0: return f(&Field_TileD0);
    case kTileA4: return f(&Field_TileA4);
    case kFloorDamage: return f(&Field_FloorDamage);
    case kBit80: return f(&Field_Bit80Tick);
    case kBit20: return f(&Field_Bit20Tick);
    case kTile89: return f(&Field_Tile89);
    case kTileTurn: return f(&Field_TileTurn);
    case kTile8A: return f(&Field_Tile8A);
    case kEquipTick: return f(&Field_EquipTick);
    case kEquipCount: return f(&Actor_EquipCount);
    case kCellsAll: return f(&AreaMap_CellsAll);
    case kCellsAll4: return f(&AreaMap_CellsAll4);
    case kCellsNone: return f(&AreaMap_CellsNone);
    case kCellsNone4: return f(&AreaMap_CellsNone4);
    case kJumpCheck: return f(&Field_JumpCheckHeight);
    default: return f(&Sprite_ApplyVelocity);
    }
}

using Fn0 = std::uint32_t (__cdecl*)();
using Fn1 = std::uint32_t (__cdecl*)(std::uint32_t);
using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);
using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using Fn5 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
struct Args { std::uint32_t a[5]; };

std::uint32_t Run(const void* fn, const Clone& c, const Args& x) {
    void* const p = const_cast<void*>(fn);
    std::uint32_t r = 0;
    switch (c.args) {
    case 0: r = reinterpret_cast<Fn0>(p)(); break;
    case 1: r = reinterpret_cast<Fn1>(p)(x.a[0]); break;
    case 3: r = reinterpret_cast<Fn3>(p)(x.a[0], x.a[1], x.a[2]); break;
    case 4: r = reinterpret_cast<Fn4>(p)(x.a[0], x.a[1], x.a[2], x.a[3]); break;
    default: r = reinterpret_cast<Fn5>(p)(x.a[0], x.a[1], x.a[2], x.a[3], x.a[4]); break;
    }
    return c.boolean ? r & 0xFF : 0;   // al for the answers; nothing for the voids
}

// --- the state a round compares ---------------------------------------------------
// One block from DamageScratch through the 256th actor record's equipment
// bytes: the slope byte, Field_ScriptFlags, every record an index byte can
// name (0x903A70 + 255 * 0xA4 + 0x18), the party list and its 256-byte reach,
// MoveScript_FAWord, Field_InputFlags, Field_ScriptFlags2, the Field_State
// pointer and Field_Kind2Z / X. Then Sprite_Current, MoveScript_F3Divisor and
// Gfx_ClutStripDirty; and, for the one function each that writes them, the
// 256 CLUTs of row 15 and 256 effect objects (a whole index byte's reach).
constexpr std::uint32_t kBlockAt = 0x903850, kBlockEnd = 0x903A70 + 255 * kRecordSize + 0x18;
constexpr unsigned kBlock = kBlockEnd - kBlockAt, kClutBytes = 256 * 64, kEffectBytes = 256 * 0x80;
constexpr std::uint32_t kGlobalsAt = 0x937F88;
constexpr std::uint32_t kEffectsAt = 0x7E11E0;   // Effect_Objects
constexpr unsigned kGlobals = 12;

struct State {
    unsigned char block[kBlock];
    unsigned char globals[kGlobals];
    unsigned char clut[kClutBytes];
    unsigned char effects[kEffectBytes];
    unsigned char sprite[2][kSprite], field[2][kField];
    std::uint32_t result;
    Log log;
};
void Capture(State& s, unsigned k) {
    std::memcpy(s.block, At(kBlockAt), kBlock);
    std::memcpy(s.globals, At(kGlobalsAt), kGlobals);
    if (k == kShadeBegin) std::memcpy(s.clut, At(kShadeClut), kClutBytes);
    if (k == kBit80) std::memcpy(s.effects, At(kEffectsAt), kEffectBytes);
    std::memcpy(s.sprite, g_sprite, sizeof g_sprite);
    std::memcpy(s.field, g_field, sizeof g_field);
    s.log = g_log;
}
void Apply(const State& s, unsigned k) {
    std::memcpy(At(kBlockAt), s.block, kBlock);
    std::memcpy(At(kGlobalsAt), s.globals, kGlobals);
    if (k == kShadeBegin) std::memcpy(At(kShadeClut), s.clut, kClutBytes);
    if (k == kBit80) std::memcpy(At(kEffectsAt), s.effects, kEffectBytes);
    std::memcpy(g_sprite, s.sprite, sizeof g_sprite);
    std::memcpy(g_field, s.field, sizeof g_field);
    std::memset(&g_log, 0, sizeof g_log);
}
// Everything a round compares; the CLUTs and the effect objects only for the
// function that writes them (the other rounds leave them stale).
bool Same(const State& a, const State& b, unsigned k) {
    if (std::memcmp(a.block, b.block, kBlock) || std::memcmp(a.globals, b.globals, kGlobals)) return false;
    if (std::memcmp(a.sprite, b.sprite, sizeof a.sprite) || std::memcmp(a.field, b.field, sizeof a.field)) return false;
    if (a.result != b.result || std::memcmp(&a.log, &b.log, sizeof a.log)) return false;
    if (k == kShadeBegin && std::memcmp(a.clut, b.clut, kClutBytes)) return false;
    if (k == kBit80 && std::memcmp(a.effects, b.effects, kEffectBytes)) return false;
    return true;
}

// A sprite object: the fields the 22 read, each branch's boundaries seeded.
void RandomSprite(unsigned char* s) {
    for (unsigned i = 0; i < kSprite; ++i) s[i] = static_cast<unsigned char>(Next());
    s[8] = Pick({0, 1, 2, 3, 4, 5, 6, 7, 2, 6, 8, 0x0A, 0xFE});
    s[9] = static_cast<unsigned char>(1 + Next() % 0xFF);   // never 0: see Disturb
    if (OneIn(2)) s[5] = 0;   // object 0: Field_JumpStart goes on to the camera
    for (unsigned at = 0x5D; at <= 0x5F; ++at) s[at] = Pick({0, 0x80, 0x80, 0xBC, 0xBF, 0xC0, 0xC0, 0xC1, 0x7F, 0xFF, 1});
    if (OneIn(3)) std::memset(s + 0x5D, 0x80, 3);
    if (OneIn(4)) std::memset(s + 0x5D, 0xC0, 3);
    if (OneIn(2)) s[0x70] = 0;
    // x and z whole (no fraction) now and then, for the cells' fraction tests.
    if (OneIn(3)) SetWord(s + 0x34, 0);
    if (OneIn(3)) SetWord(s + 0x38, 0);
    if (OneIn(4)) SetLong(s + 0x14, Next() % 5 - 2);
}

Args Generate(unsigned k, State& input) {
    g_seed = Next();
    // Sprites, Field_State and the globals around them.
    for (auto& s : g_sprite) RandomSprite(s);
    for (auto& f : g_field) {
        for (unsigned i = 0; i < kField; ++i) f[i] = static_cast<unsigned char>(Next());
        f[0x124] = Pick({0, 1, 2, 10, 0xFF});
        f[0x125] = Pick({0, 1, 2, 0x28, 0xFF});
        f[0x148] = static_cast<unsigned char>(OneIn(2) ? Next() % 8 : Next());
    }
    // Field_JumpSetUp divides by the speed and then by the frames the speed
    // leaves: only the table entries 1..16 (indices 1..5, 14, 15) may reach it.
    if (k == kJumpSetUp) {
        static constexpr unsigned char kSafe[] = {1, 2, 3, 4, 5, 14, 15};
        for (auto& f : g_field) f[0x128] = kSafe[Next() % sizeof kSafe];
    }
    // Field_Bit80Tick's countdown ends from 1.
    if (k == kBit80 && OneIn(2)) g_field[0][0x124] = 1;
    Sprite_Current = g_sprite[0];
    Field_State = g_field[0];
    unsigned char* const scratch = At(bof3::addr::DamageScratch);
    for (unsigned i = 0; i < 4; ++i) scratch[i] = static_cast<unsigned char>(OneIn(2) ? 0 : Next());
    Field_ScriptFlags = static_cast<unsigned short>(Next() & (OneIn(2) ? 0xFBFFu : 0xFFFFu));
    if (OneIn(3)) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFF7u);
    Field_ScriptFlags2 = static_cast<unsigned short>(Next() & (OneIn(2) ? 0xFFF7u : 0xFFFFu));
    Field_InputFlags = static_cast<unsigned char>(Next());
    MoveScript_F3Divisor = static_cast<short>(Next());
    Field_Kind2X = static_cast<long>(Next());
    Field_Kind2Z = static_cast<long>(Next());
    MoveScript_FAWord = static_cast<unsigned short>(Next());
    Gfx_ClutStripDirty = static_cast<unsigned char>(Next());
    // The records' state and equipment bytes, the codes the tests look for
    // seeded; the party list, mostly members 0..7.
    for (unsigned n = 0; n < 256; ++n) {
        unsigned char* const r = At(bof3::addr::CharacterRecords + n * kRecordSize);
        const std::uint32_t h = Next();
        r[0x10] = static_cast<unsigned char>((h & 0x5F) | (h % 3 == 0 ? 0x80 : 0) | (h % 5 == 0 ? 0x20 : 0));
        r[0x11] = static_cast<unsigned char>(h >> 8);
        for (unsigned i = 0x12; i < 0x18; ++i) r[i] = Pick({0x16, 0x17, 0x1F, 0x16, 0x17, 0x1F, 0});
    }
    for (unsigned i = 0; i < 256; ++i) At(kPartyList)[i] = static_cast<unsigned char>(OneIn(4) ? Next() : Next() % 8);
    if (k == kShadeBegin)
        for (const auto& s : g_sprite)
            for (unsigned i = 0; i < 64; ++i) At(kShadeClut + s[5] * 64u)[i] = static_cast<unsigned char>(Next());

    Args x{};
    for (auto& a : x.a) a = Next();
    g_code = static_cast<unsigned char>(Next());
    switch (k) {
    case kShadeStep: case kShadeRaise:
        x.a[0] = Stale(Pick({0, 1, 4, 8, 0x3F, 0x40, 0x41, 0x80, 0xFF}));
        break;
    case kTile89: case kTile8A:
        if (OneIn(2)) x.a[0] &= 0xFFFFFF00u;
        break;
    case kTileTurn:
        x.a[0] = OneIn(2) ? Stale(Pick({0x89, 0x8A})) : x.a[0];
        if (OneIn(2)) x.a[2] &= 0xFFFFFF00u;
        break;
    case kEquipCount:
        x.a[0] = Stale(static_cast<unsigned char>(OneIn(2) ? Next() % 24 : Next()));
        x.a[1] = Stale(Pick({0, 1, 2, 3, 4, 0xFF, 1, 2, 3}));
        x.a[2] = Stale(Pick({0x16, 0x17, 0x1F, 0}));
        break;
    case kCellsAll: case kCellsNone: case kCellsAll4: case kCellsNone4: {
        const bool wide = k == kCellsAll || k == kCellsNone;
        // The fractions: none, the low byte's or the high byte's bits alone, or any.
        for (unsigned i = 0; i < 2; ++i)
            if (!OneIn(4)) x.a[i] = (x.a[i] & 0xFFFF0000u) | Pick({0, 0, 1, 0x80, 0xFF}) << (OneIn(2) ? 8 : 0);
        if (OneIn(5)) x.a[0] = 0x0000FFFFu;
        const std::uint32_t code = Stale(Pick({0x20, 0x21, 0x01, 0x00, 0x30, 0x80, 0x88, 0xD0, 0xA4, 0x89}));
        const std::uint32_t mask = OneIn(2) ? Stale(0) : Next();
        if (wide) {
            x.a[2] = OneIn(2) ? 0 : Pick({1, 0xFF}) | (OneIn(3) ? 0x100u : 0) | (OneIn(4) ? Next() & 0xFFFFFF00u : 0);
            if (OneIn(6)) x.a[2] = 0x100;
            x.a[3] = code;
            x.a[4] = mask;
        } else {
            x.a[2] = code;
            x.a[3] = mask;
        }
        g_code = static_cast<unsigned char>(code);
        break;
    }
    default:
        break;
    }
    Capture(input, k);
    return x;
}

}  // namespace

void SelfTest() {
    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    constexpr unsigned kPer = 5000, kRounds = kPer * kCount;
    static State saved, input, theirs, ours;
    Capture(saved, kShadeBegin);
    std::memcpy(saved.effects, At(kEffectsAt), kEffectBytes);
    g = kStubs;
    unsigned bad = 0, bad_per[kCount] = {}, yes[kCount] = {}, calls = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        const Args x = Generate(k, input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, k);
            State& out = pass ? ours : theirs;
            const std::uint32_t result = Run(pass ? Ours(k) : clones[k], kClones[k], x);
            Capture(out, k);
            out.result = result;
        }
        calls += theirs.log.n;
        if (theirs.result) ++yes[k];
        if (!Same(theirs, ours, k)) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      event_objs self-test MISMATCH: round %u, %s, al %02X / %02X, log %u / %u", round,
                          kClones[k].name, theirs.result, ours.result, theirs.log.n, ours.log.n);
        }
    }
    g = kOriginals;
    Apply(saved, kShadeBegin);
    std::memcpy(At(kEffectsAt), saved.effects, kEffectBytes);
    bof3::Log("shadow      event_objs self-test: %u rounds (%u per function, 22 functions), %u calls to the stand-ins, "
              "%u MISMATCHES",
              kRounds, kPer, calls, bad);
    bof3::Log("shadow      event_objs coverage: al 1 in %u (ShadeFadeStep) %u (ShadeRaise) %u (TileD0) %u (Tile89) "
              "%u (TileTurn) %u (EquipCount) %u (CellsAll4) %u (CellsNone4) of %u; calls by stand-in: slope %u, ground %u, "
              "byte_at %u, floor_hurt %u, flash %u, hp_lose %u, effect_free %u, turn_probe %u, hp_gain %u, "
              "jump_start %u, jump_camera %u, load_palette %u, cells_all_wide %u, cells_none_wide %u",
              yes[kShadeStep], yes[kShadeRaise], yes[kTileD0], yes[kTile89], yes[kTileTurn], yes[kEquipCount],
              yes[kCellsAll4], yes[kCellsNone4], kPer, g_counts[4], g_counts[5], g_counts[14], g_counts[15],
              g_counts[16], g_counts[18], g_counts[20], g_counts[22], g_counts[19], g_counts[3], g_counts[2],
              g_counts[7], g_counts[10], g_counts[13]);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      event_objs self-test: %s %u of %u rounds differ", kClones[k].name, bad_per[k], kPer);
    if (bad) bof3::Fatal("the event script's object ops differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace event_objs

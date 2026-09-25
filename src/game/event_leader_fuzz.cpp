// BOF3X_SHADOW=event_leader: a differential fuzz of the leader's states, the
// step's landing and the way-blocked tests, once at start-up.
// docs/event_leader.md section 5.
//
// Twenty-seven byte-copies, every call and tail jump out re-aimed at a
// recording stand-in (bof3::CloneCall with `expected`); the five .data
// dispatch tables (Field_SwapSteps, Field_MenuSteps, Field_EncounterSteps,
// Field_PassageSteps, Field_ActionBySet) swapped for recorders, and
// Area_Descriptors' first eight entries pointed at descriptors of our own.
// One round: one function, random bytes in every region any of them touches,
// the pointers and indices put back inside what they index, each branch's
// boundaries seeded; theirs, then from the same state ours; the regions, the
// passage, name and exit records the stand-ins hand out, the result and the
// stand-ins' log compared. Everything is put back afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_leader_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_leader {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Member(unsigned i) { return ObjTrio + i * at::kMemberStride; }

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

// What the stand-ins hand out: the passage entry, an item's name, the exit
// records; the fake area descriptors and their script lists.
unsigned char g_passage[16];
unsigned char g_name[24];
constexpr unsigned kExits = 8;
unsigned char g_exits[kExits * 4];
constexpr unsigned kAreas = 8;
unsigned char g_desc[kAreas][8];
std::uint32_t g_scripts[0x1000 + kAreas];

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
// A pointer as an id that is the same on both passes: the three members,
// the stand-ins' buffers, else the address.
std::uint32_t Id(const void* p) {
    const std::uint32_t a = Address(p);
    const std::uint32_t trio = Address(ObjTrio);
    if (a >= trio && a < trio + 3 * at::kMemberStride) return 0x10000u + (a - trio);
    if (a >= Address(g_passage) && a < Address(g_passage) + sizeof g_passage) return 0x20000u + (a - Address(g_passage));
    return a;
}

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the party, indices inside what they index.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 22) {
    case 0: Sprite_Current = Member(v % 3); break;
    case 1: Field_State = Member(v % 3); break;
    case 2: Field_MemberCount = static_cast<unsigned char>(v % 4); break;
    case 3: {
        unsigned char* const sc = Sprite_Current;
        const unsigned k = w % 4;
        if (k == 0) sc[8] = static_cast<unsigned char>(v % 8);
        else if (k == 1) sc[9] = static_cast<unsigned char>(v % 3 == 0 ? 0 : v);
        else sc[k - 1] = static_cast<unsigned char>(v % 6);
        break;
    }
    case 4: Member(w % 3)[1] = static_cast<unsigned char>(v % 3 == 0 ? 3 : v % 3 == 1 ? 5 : v % 12); break;
    case 5: Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ (1u << (v % 8))); break;
    case 6: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ (1u << (v % 16))); break;
    case 7: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (1u << (v % 16))); break;
    case 8: Field_Request = static_cast<unsigned char>(v % 3 == 0 ? 0 : v % 3 == 1 ? 1 : v); break;
    case 9: SetLong(At(at::kTargetX + 4 * (w % 2)), static_cast<std::int32_t>(h * 0x9E3779B1u)); break;
    case 10: At(at::kScratch)[0] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 11: At(at::kPendingCount)[0] = static_cast<unsigned char>(v % 5); break;
    case 12: g_passage[4 + w % 3] = static_cast<unsigned char>(v % 4 == 0 ? 0xFF : v); break;
    case 13: {
        unsigned char* const fs = Field_State;
        const unsigned k = w % 3;
        if (k == 0) fs[0x128] = static_cast<unsigned char>(3 + v % 2);
        else if (k == 1) fs[0x137] = static_cast<unsigned char>(v % 2 ? 0 : v);
        else fs[0x138] = static_cast<unsigned char>(fs[0x138] ^ 1);
        break;
    }
    case 14: {
        const unsigned k = w % 3;
        if (k == 0) ObjTrio[0x138] = static_cast<unsigned char>(ObjTrio[0x138] ^ 1);
        else if (k == 1) ObjTrio[9] = static_cast<unsigned char>(v % 3 == 0 ? 0 : v % 3 == 1 ? 4 : v);
        else ObjTrio[8] = static_cast<unsigned char>(v % 8);
        break;
    }
    case 15: {
        unsigned char* const sc = Sprite_Current;
        if (w % 2) sc[0x70] = static_cast<unsigned char>(v % 2 ? 0 : v);
        else SetWord(sc + 0x3E, h >> 16);
        break;
    }
    case 16: Field_Kind2X = static_cast<long>(h * 0x2545F491u); break;
    case 17: Field_InputHeld = static_cast<unsigned short>(h >> 16); break;
    case 18: Member(w % 3)[0x5D] = static_cast<unsigned char>(v); break;
    case 19: Effect_Objects[(w % 32) * 0x80u] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 20: {
        unsigned char* const sc = Sprite_Current;
        SetLong(sc + 0x34 + 4 * (w % 2), static_cast<std::int32_t>(h * 0x2545F491u));
        break;
    }
    default: At(at::kPassageFlags + w % 32)[0] = static_cast<unsigned char>(v); break;
    }
}

// --- the stand-ins ---------------------------------------------------------

std::uint32_t Sc() { return Id(Sprite_Current); }
std::uint32_t Fs() { return Id(Field_State); }
// A byte answer: 0 with probability zeros / 8, else non-zero.
unsigned char Answer(unsigned zeros) {
    const std::uint32_t h = Hash();
    if (h % 8 < zeros) return 0;
    const unsigned char v = static_cast<unsigned char>(h >> 9);
    return v ? v : 1;
}

// The five dispatch tables' entries, the leader's own callees.
template <unsigned N> void __cdecl StubHandler() { Record(N, Sc(), Fs()); Disturb(); }
void __cdecl StubStepLands() { Record(1, Sc(), Fs()); Disturb(); }
unsigned char __cdecl StubStepTick() { Record(2, Sc()); Disturb(); return Answer(4); }
void __cdecl StubEquipTick() { Record(3, Sc()); Disturb(); }
void __cdecl StubBit20Tick() { Record(4, Sc()); Disturb(); }
void __cdecl StubFloorDamage() { Record(5, Sc()); Disturb(); }
unsigned char __cdecl StubTile89(unsigned t) { Record(6, t); Disturb(); return Answer(7); }
unsigned char __cdecl StubTile8A(unsigned t) { Record(7, t); Disturb(); return Answer(7); }
unsigned char __cdecl StubTileD0() { Record(8); Disturb(); return Answer(7); }
unsigned char __cdecl StubTileA4() { Record(9); Disturb(); return Answer(7); }
unsigned char __cdecl StubTestFB(short x, short z) { Record(10, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z)); Disturb(); return Answer(4); }
// The cell codes the callers test, often.
unsigned char __cdecl StubByteAt(short x, short z) {
    Record(11, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z), Sc());
    Disturb();
    static const unsigned char kCodes[] = {0xAF, 0xC0, 0xA0, 0xA1, 0xAE, 0x91, 0x26, 0x2C, 0x2D, 0xFF, 0, 0xB0};
    const std::uint32_t h = Hash();
    return h % 4 == 0 ? static_cast<unsigned char>(h >> 8) : kCodes[(h >> 8) % (sizeof kCodes)];
}
unsigned char __cdecl StubLeaderAnimation(unsigned a) { Record(12, a & 0xFF, Sc()); Disturb(); return Answer(4); }
void __cdecl StubChangeArea(unsigned area, int x, int z, unsigned flags) {
    Record(13, area & 0xFFFF, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), flags & 0xFF);
    Disturb();
}
unsigned char __cdecl StubLinkAt(unsigned x, unsigned z) { Record(14, x & 0xFFFF, z & 0xFFFF); Disturb(); return Answer(4); }
unsigned char __cdecl StubCellsAll(long x, long z, unsigned wide, unsigned code, unsigned mask) {
    Record(15, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), wide, (code & 0xFF) | (mask & 0xFF) << 8);
    Disturb();
    return Answer(6);
}
bool g_cells_mostly_none;
unsigned char __cdecl StubCellsNone(long x, long z, unsigned wide, unsigned code, unsigned mask) {
    Record(16, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), wide, (code & 0xFF) | (mask & 0xFF) << 8);
    Disturb();
    return g_cells_mostly_none ? static_cast<unsigned char>(Hash() % 9 != 0) : Answer(3);
}
unsigned char __cdecl StubEnsureAnimation(unsigned char a) { Record(17, a, Sc()); Disturb(); return Answer(4); }
void __cdecl StubJumpStart() { Record(18, Sc()); Disturb(); }
void __cdecl StubClearSteps() { Record(19, Sc()); Disturb(); }
unsigned char __cdecl StubEncounterDue() { Record(20); Disturb(); return Answer(4); }
unsigned char __cdecl StubEffectTest(unsigned pace) { Record(21, pace & 0xFF); Disturb(); return Answer(6); }
// The whole dword is tested: now and then only its upper bytes are set.
int __cdecl StubArriveHook(long x, long z) {
    Record(22, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    Disturb();
    const std::uint32_t h = Hash();
    return h % 4 == 0 ? static_cast<int>((h >> 8) << 8) : h % 4 == 1 ? static_cast<int>(h) : 0;
}
void __cdecl StubBit80Tick() { Record(23); Disturb(); }
void __cdecl StubLeaderWalk() { Record(24, Sc()); Disturb(); }
void __cdecl StubClearState(unsigned m) { Record(25, m & 0xFF); Disturb(); }
void __cdecl StubShadeBegin() { Record(26, Sc()); Disturb(); }
// Near the leader's height half the time: the 0x40 and 0xC0 edges.
long __cdecl StubGroundAt(long x, long z) {
    Record(27, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    Disturb();
    const std::uint32_t h = Hash();
    static const int kEdges[] = {0, 0x3F, 0x40, 0x41, -0x40, -0x41, 0xBF, 0xC0, 0xC1, -0xC0, -0xC1, 0x7FFF};
    if (h % 2) return static_cast<long>(h);
    return static_cast<long>((h & 0xFFFF0000u) | static_cast<std::uint16_t>(Word(At(at::kLeaderHeight)) + kEdges[(h >> 4) % 12]));
}
void __cdecl StubSetAnimation(unsigned char a) { Record(28, a, Sc()); Disturb(); }
void __cdecl StubSwapFields(unsigned a, unsigned b, unsigned keep) { Record(29, a & 0xFF, b & 0xFF, keep & 0xFF); Disturb(); }
void __cdecl StubMemberSprite(unsigned m, unsigned slot) { Record(30, m & 0xFF, slot & 0xFF, Sc()); Disturb(); }
void __cdecl StubLoadPalette(unsigned short* dst, unsigned index) { Record(31, Address(dst), index); Disturb(); }
void __cdecl StubSetElevation(int v) { Record(32, static_cast<std::uint32_t>(v) & 0xFFFF); Disturb(); }
void __cdecl StubRollInitiative() { Record(33); Disturb(); }
unsigned char __cdecl StubScriptTick() { Record(34, Sc()); Disturb(); return Answer(4); }
const unsigned char* __cdecl StubPassageAhead() { Record(35, Sc()); Disturb(); return g_passage; }
// The script fills the frame's buffer through Field_ActiveMember: +0x86 and
// the message word +0x88 (0xFFFF a third of the time).
void __cdecl StubScriptRun(const unsigned char* script) {
    Record(36, Address(script), Field_ActiveMember[0x86]);
    const std::uint32_t h = Hash();
    unsigned char* const b = Field_ActiveMember;
    b[0x86] = static_cast<unsigned char>(h >> 3);
    SetWord(b + 0x88, h % 3 == 0 ? 0xFFFFu : h >> 16);
    Disturb();
}
void __cdecl StubOpenScript(unsigned short id) { Record(37, id); Disturb(); }
void __cdecl StubOpenSystem(unsigned id) { Record(38, id); Disturb(); }
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned i) { Record(39, Address(bits), i & 0xFF); Disturb(); return static_cast<unsigned char>(Hash() % 3 == 0); }
void __cdecl StubFlagsSet(unsigned char* bits, unsigned i) { Record(40, Address(bits), i & 0xFF); Disturb(); }
void __cdecl StubPlayEffect(unsigned short id) { Record(41, id); Disturb(); }
unsigned char* __cdecl StubItemName(unsigned cat, unsigned item) {
    Record(42, cat & 0xFF, item & 0xFF);
    Disturb();
    return g_name + Hash() % 8;
}
unsigned char __cdecl StubInventoryAdd(unsigned cat, unsigned item, unsigned count) {
    Record(43, cat & 0xFF, item & 0xFF, count & 0xFF);
    Disturb();
    return static_cast<unsigned char>(Hash() % 3 != 0);
}
unsigned char __cdecl StubTestFC(short x, short z) { Record(44, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z)); Disturb(); return Answer(4); }
// Above and below 0x40 in the low word, whatever the upper.
long __cdecl StubSlopeAt(long x, long z, unsigned long d) {
    Record(45, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), static_cast<std::uint32_t>(d));
    Disturb();
    static const std::uint16_t kSlopes[] = {0x3F, 0x40, 0x41, 0, 0x8000, 0x7FFF, 0xFFC0};
    const std::uint32_t h = Hash();
    return static_cast<long>((h & 0xFFFF0000u) | kSlopes[(h >> 4) % 7]);
}
unsigned char __cdecl StubObjectAt(long x, long z, unsigned margin) {
    Record(46, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), margin, Word(Sprite_Current + 0x3E));
    Disturb();
    return Hash() % 2 ? 0xFF : static_cast<unsigned char>(Hash() >> 7);
}
unsigned char __cdecl StubPathClear(long x, long z, unsigned raised) {
    Record(47, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), raised & 0xFF);
    Disturb();
    return Answer(3);
}
void __cdecl StubGiveZenny(int n) { Record(48, static_cast<std::uint32_t>(n)); Disturb(); }
// A record that matches the leader's cell is planted at a random index - the
// original's search has no end - after the move below, with the cell's upper
// bytes cleared so that one can match.
const unsigned char* __cdecl StubAreaExits() {
    Record(49, Sc());
    Disturb();
    unsigned char* const sc = Sprite_Current;
    sc[0x37] = 0;
    sc[0x3B] = 0;
    const unsigned k = Hash() % kExits;
    g_exits[k * 4] = sc[0x36];
    g_exits[k * 4 + 1] = sc[0x3A];
    return g_exits;
}
unsigned char __cdecl StubWayBlockedWide(long x, long z, long ground) {
    Record(50, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), static_cast<std::uint32_t>(ground));
    Disturb();
    return Answer(4);
}
unsigned char __cdecl StubSpotFree(long x, long z, unsigned, unsigned raised) {
    Record(51, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), raised & 0xFF, Sc());
    Disturb();
    return Answer(4);
}
unsigned char __cdecl StubWayBlocked4(long x, long z, long ground) {
    Record(52, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), static_cast<std::uint32_t>(ground));
    Disturb();
    return Answer(4);
}
unsigned char __cdecl StubCellsBlock(long x, long z, unsigned wide) {
    Record(53, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), wide);
    Disturb();
    return Answer(6);
}
unsigned char __cdecl StubWayBlocked(long x, long z, unsigned raised, long ground) {
    Record(54, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), raised & 0xFF, static_cast<std::uint32_t>(ground));
    Disturb();
    return Answer(5);
}

const Callees kStubs = {
    StubStepLands, StubSpotFree, StubWayBlocked4, StubCellsBlock, StubWayBlocked,
    StubStepTick, StubEquipTick, StubBit20Tick, StubFloorDamage, StubTile89, StubTile8A, StubTileD0, StubTileA4,
    StubTestFB, StubByteAt, StubLeaderAnimation, StubChangeArea, StubLinkAt, StubCellsAll, StubCellsNone,
    StubEnsureAnimation, StubJumpStart, StubClearSteps, StubEncounterDue, StubEffectTest, StubArriveHook,
    StubBit80Tick, StubLeaderWalk, StubClearState, StubShadeBegin, StubGroundAt, StubSetAnimation, StubSwapFields,
    StubMemberSprite, StubLoadPalette, StubSetElevation, StubRollInitiative, StubScriptTick, StubPassageAhead,
    StubScriptRun, StubOpenScript, StubOpenSystem, StubFlagsTest, StubFlagsSet, StubPlayEffect, StubItemName,
    StubInventoryAdd, StubTestFC, StubSlopeAt, StubObjectAt,
    StubPathClear, StubGiveZenny, StubAreaExits, StubWayBlockedWide,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x52E580: return f(&StubStepLands);
    case 0x52E140: return f(&StubStepTick);
    case 0x535270: return f(&StubEquipTick);
    case 0x5350C0: return f(&StubBit20Tick);
    case 0x534A00: return f(&StubFloorDamage);
    case 0x535120: return f(&StubTile89);
    case 0x535240: return f(&StubTile8A);
    case 0x534920: return f(&StubTileD0);
    case 0x534990: return f(&StubTileA4);
    case 0x572650: return f(&StubTestFB);
    case 0x536700: return f(&StubByteAt);
    case 0x5305B0: return f(&StubLeaderAnimation);
    case 0x594E00: return f(&StubChangeArea);
    case 0x5951D0: return f(&StubLinkAt);
    case 0x535390: return f(&StubCellsAll);
    case 0x535C50: return f(&StubCellsNone);
    case 0x589330: return f(&StubEnsureAnimation);
    case 0x5345E0: return f(&StubJumpStart);
    case 0x536650: return f(&StubClearSteps);
    case 0x530030: return f(&StubEncounterDue);
    case 0x530860: return f(&StubEffectTest);
    case 0x56D750: return f(&StubArriveHook);
    case 0x534F10: return f(&StubBit80Tick);
    case 0x52DB90: return f(&StubLeaderWalk);
    case 0x536730: return f(&StubClearState);
    case 0x534590: return f(&StubShadeBegin);
    case 0x572570: return f(&StubGroundAt);
    case 0x5891F0: return f(&StubSetAnimation);
    case 0x5323E0: return f(&StubSwapFields);
    case 0x533BA0: return f(&StubMemberSprite);
    case 0x5366A0: return f(&StubLoadPalette);
    case 0x5725F0: return f(&StubSetElevation);
    case 0x532550: return f(&StubRollInitiative);
    case 0x5893A0: return f(&StubScriptTick);
    case 0x530600: return f(&StubPassageAhead);
    case 0x5797C0: return f(&StubScriptRun);
    case 0x4976D0: return f(&StubOpenScript);
    case 0x497710: return f(&StubOpenSystem);
    case 0x57C140: return f(&StubFlagsTest);
    case 0x57C0F0: return f(&StubFlagsSet);
    case 0x587740: return f(&StubPlayEffect);
    case 0x591680: return f(&StubItemName);
    case 0x590BB0: return f(&StubInventoryAdd);
    case 0x572790: return f(&StubTestFC);
    case 0x5725C0: return f(&StubSlopeAt);
    case 0x531CF0: return f(&StubObjectAt);
    case fn::kPathClear: return f(&StubPathClear);
    case fn::kGiveZenny: return f(&StubGiveZenny);
    case fn::kAreaExits: return f(&StubAreaExits);
    case fn::kWayBlockedWide: return f(&StubWayBlockedWide);
    case 0x52EBA0: return f(&StubSpotFree);
    case 0x535640: return f(&StubWayBlocked4);
    case 0x535730: return f(&StubCellsBlock);
    case 0x535610: return f(&StubWayBlocked);
    default: bof3::Fatal("event_leader: no stand-in for a call to 0x%X", static_cast<unsigned>(target));
    }
}

// The five .data tables: their entries become numbered recorders.
struct Swapped { std::uint32_t at; unsigned count, first; };
constexpr Swapped kTables[] = {
    {at::kSwapSteps, at::kSwapCount, 100}, {at::kMenuSteps, at::kMenuCount, 110},
    {at::kEncounterSteps, at::kEncounterCount, 115}, {at::kPassageSteps, at::kPassageStepCount, 120},
    {at::kActionBySet, at::kActionCount, 130},
};
constexpr unsigned kSwappedTotal = 5 + 2 + 2 + 4 + 19;
template <unsigned... N> struct Handlers { static constexpr Handler list[] = {&StubHandler<N>...}; };
using Swap = Handlers<100, 101, 102, 103, 104>;
using Menu = Handlers<110, 111>;
using Enc = Handlers<115, 116>;
using Pass = Handlers<120, 121, 122, 123>;
using Act = Handlers<130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148>;
const Handler* const kTableStubs[] = {Swap::list, Menu::list, Enc::list, Pass::list, Act::list};

// --- the copies ------------------------------------------------------------

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    Call calls[32];
    unsigned ret;   // 0 nothing to compare, 1 the low byte
};

enum : unsigned {
    kStepping, kStepLands, kSwapState, kSwapGather, kSpotFree, kSpinOut, kExchange, kSpinIn, kSwapEnd,
    kMenuState, kMenuOpen, kMenuWait, kEncState, kEncStart, kEncWait, kPassState, kPassOpen, kPassTake,
    kPassEnd, kActionState, kEncArea, kCellEvent, kExitCell, kWalkIn, kWayBlocked, kWayBlocked4, kCellsBlock,
    kCount
};

// Offsets of every E8 / E9 leaving each extent (capstone, 2026-09-25).
const Clone kClones[kCount] = {
    {"Field_LeaderStepping", 0x52E110, 0x28, {{0x19, 0x52E580}, {0x23, 0x52E140}}, 0},
    {"Field_LeaderStepLands", 0x52E580, 0x445, {{0x42, 0x535270}, {0x49, 0x5350C0}, {0x4E, 0x534A00}, {0x55, 0x535120}, {0x67, 0x535240}, {0x77, 0x534920}, {0x84, 0x534990}, {0x9C, 0x572650}, {0xC4, 0x536700}, {0xD9, 0x5305B0}, {0xFA, 0x594E00}, {0x12E, 0x5305B0}, {0x153, 0x536700}, {0x16E, 0x5951D0}, {0x195, 0x535390}, {0x1B4, 0x5951D0}, {0x1CA, 0x589330}, {0x255, 0x589330}, {0x271, 0x594E00}, {0x2A5, 0x535C50}, {0x2BB, 0x589330}, {0x2DE, 0x594E00}, {0x300, 0x5345E0}, {0x30D, 0x52E140}, {0x33E, 0x536650}, {0x367, 0x530030}, {0x386, 0x589330}, {0x3BE, 0x530860}, {0x3D7, 0x56D750}, {0x3E7, 0x534F10}, {0x410, 0x52DB90}, {0x423, 0x5305B0}}, 0},
    {"Field_SwapState", 0x52E9D0, 0x12, {}, 0},
    {"Field_SwapGather", 0x52E9F0, 0x1A4, {{0x88, 0x52EBA0}, {0xAB, 0x52EC20}, {0xCF, 0x52EBA0}, {0xEE, 0x52EBA0}, {0x10D, 0x52EBA0}, {0x122, 0x536730}, {0x133, 0x534590}}, 0},
    {"Field_SpotFree", 0x52EBA0, 0x7E, {{0xE, 0x572570}, {0x29, 0x535610}, {0x59, 0x572570}}, 1},
    {"Field_SwapSpinOut", 0x52ED80, 0xE2, {{0x2A, 0x5891F0}}, 0},
    {"Field_SwapExchange", 0x52EE70, 0x336, {{0x28, 0x5323E0}, {0x5F, 0x5323E0}, {0xAA, 0x533BA0}, {0x138, 0x534590}, {0x1B7, 0x52EBA0}, {0x1DA, 0x52EC20}, {0x201, 0x52EBA0}, {0x237, 0x572570}, {0x250, 0x5891F0}, {0x309, 0x52EBA0}}, 0},
    {"Field_SwapSpinIn", 0x52F1B0, 0x196, {{0x29, 0x5891F0}, {0x107, 0x5366A0}, {0x146, 0x536650}}, 0},
    {"Field_SwapEnd", 0x52F350, 0x38, {{0x1C, 0x5725F0}}, 0},
    {"Field_MenuState", 0x52F390, 0x12, {}, 0},
    {"Field_MenuOpen", 0x52F3B0, 0x11, {}, 0},
    {"Field_MenuWait", 0x52F3D0, 0x29, {}, 0},
    {"Field_EncounterState", 0x52F400, 0x12, {}, 0},
    {"Field_EncounterStart", 0x52F420, 0x62, {{0x9, 0x532550}, {0x2E, 0x536730}}, 0},
    {"Field_EncounterWait", 0x52F490, 0x5C, {}, 0},
    {"Field_PassageState", 0x52F5E0, 0x17, {{0x12, 0x5893A0}}, 0},
    {"Field_PassageOpen", 0x52F600, 0x155, {{0x7, 0x530600}, {0x20, 0x5305B0}, {0x35, 0x589330}, {0xA4, 0x5797C0}, {0xC6, 0x4976D0}, {0x114, 0x4976D0}, {0x122, 0x497710}, {0x136, 0x497710}}, 0},
    {"Field_PassageTake", 0x52F760, 0x13D, {{0xE, 0x530600}, {0x1E, 0x57C140}, {0x54, 0x57C0F0}, {0x61, 0x587740}, {0x72, 0x5307C0}, {0x96, 0x591680}, {0xCD, 0x590BB0}, {0xE6, 0x57C0F0}, {0xFE, 0x587740}, {0x105, 0x497710}, {0x122, 0x497710}}, 0},
    {"Field_PassageEnd", 0x52F8A0, 0x49, {{0x1B, 0x5305B0}, {0x2C, 0x589330}}, 0},
    {"Field_ActionState", 0x52FB60, 0x43, {{0x29, 0x589330}, {0x30, 0x536730}}, 0},
    {"Field_EncounterArea", 0x5317F0, 0x2E, {}, 0},
    {"Field_CellHasEvent", 0x531920, 0x2B, {{0xA, 0x536700}}, 1},
    {"Field_ExitFromCell", 0x531AF0, 0x6F, {{0xF, 0x536700}, {0x1C, 0x462AC0}}, 0},
    {"Field_PendingWalkIn", 0x533780, 0x130, {{0x1E, 0x536730}, {0x114, 0x572790}}, 0},
    {"Field_WayBlocked", 0x535610, 0x29, {{0x17, 0x535640}, {0x20, 0x535830}}, 1},
    {"Field_WayBlocked4", 0x535640, 0xE8, {{0xE, 0x535730}, {0x28, 0x5725C0}, {0x4D, 0x5725C0}, {0x77, 0x5725C0}, {0x95, 0x572570}, {0xCD, 0x531CF0}}, 1},
    {"Field_CellsBlock", 0x535730, 0xFA, {{0x27, 0x535C50}, {0x42, 0x535C50}, {0x6B, 0x535C50}, {0x85, 0x535C50}, {0x98, 0x535C50}, {0xB5, 0x535C50}, {0xCB, 0x535C50}, {0xE8, 0x535C50}}, 1},
};
unsigned Calls(const Clone& c) {
    unsigned n = 0;
    while (n < 32 && c.calls[n].offset != 0) ++n;
    return n;
}

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x802D40, 0x3E4},   // ObjTrio
    {0x7E091C, 8},       // the edge's x, z
    {0x802290, 4},       // the edge's area
    {0x7E11E0, 0x1000},  // Effect_Objects 0..31 (+0xB is kept inside them)
    {0x8CB580, 4},       // AreaMap_Header's size bytes
    {0x90384C, 0x18},    // the exit's z, the scratch, the target, the exit's x
    {0x903580, 4},       // the confirm button
    {0x9035A4, 4},       // Field_ActiveMember (normalised: see Capture)
    {0x9039A0, 4},       // Field_ScriptFlags
    {0x903A70, 0x520},   // the eight actor records
    {0x904060, 8},       // the party lists
    {0x90410C, 0x48},    // the passage flags, the party set, the count, the return point
    {0x904AA8, 4},       // 0x904AAA
    {0x904CE0, 0x10},    // Text_Records
    {0x904EE0, 0x20},    // 0x904EE0, the pending walk-in, Game_AreaNumber, MoveScript_FAWord
    {0x905B80, 0x10},    // Field_EdgeBits, the exit's kind
    {0x905BA0, 8},       // Field_InputFlags, Field_ScriptFlags2, Field_InputHeld
    {0x905D98, 4},       // Field_State
    {0x905E60, 8},       // Field_Kind2Z / X
    {0x929EC0, 4},       // Field_MemberCount
    {0x929F10, 0x10},    // Field_Kind2Hold, MapView_Elevation
    {0x937F80, 0x1C},    // the exit's area, Sprite_Current, MoveScript_F3Divisor, 0x937F98
    {0x66C7D8, 1},       // Field_Request
};
constexpr unsigned kRegionBytes = 0x3E4 + 8 + 4 + 0x1000 + 4 + 0x18 + 4 + 4 + 4 + 0x520 + 8 + 0x48 + 4 + 0x10 + 0x20 +
                                  0x10 + 8 + 4 + 8 + 4 + 0x10 + 0x1C + 1;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char passage[sizeof g_passage];
    unsigned char name[sizeof g_name];
    unsigned char exits[sizeof g_exits];
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
// Field_ActiveMember, once a function has pointed it at its own frame, is
// the one pointer the two passes cannot share: it becomes a marker.
constexpr std::uint32_t kFrameMarker = 0xF7A3E000u;
std::uint32_t g_active_in;
void Capture(State& s, bool normalise) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    if (normalise && Address(Field_ActiveMember) != g_active_in) {
        unsigned off = 0;
        for (const Region& r : kRegions) {
            if (r.at == 0x9035A4) { std::memcpy(s.memory + off, &kFrameMarker, 4); break; }
            off += r.size;
        }
    }
    std::memcpy(s.passage, g_passage, sizeof g_passage);
    std::memcpy(s.name, g_name, sizeof g_name);
    std::memcpy(s.exits, g_exits, sizeof g_exits);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_passage, s.passage, sizeof g_passage);
    std::memcpy(g_name, s.name, sizeof g_name);
    std::memcpy(g_exits, s.exits, sizeof g_exits);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x6C8E9CF5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }

// Random bytes put back inside what they index.
void Fix() {
    Sprite_Current = Member(Next() % 3);
    Field_State = Member(Next() % 3);
    Field_MemberCount = static_cast<unsigned char>(Next() % 4);
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const m = Member(i);
        m[0xB] = static_cast<unsigned char>(Half() ? 0xFF : Next() % 32);
        m[0x89] = static_cast<unsigned char>(Next() % 24);
        m[1] = static_cast<unsigned char>(Half() ? 3 : Next() % 12);
        m[2] = static_cast<unsigned char>(Next() % 6);
        if (Half()) m[8] = static_cast<unsigned char>(m[8] % 8);
    }
    Game_AreaNumber = static_cast<unsigned short>(Next() % kAreas);
    At(at::kPendingCount)[0] = static_cast<unsigned char>(Next() % 5);
    if (Half()) Field_Request = 0;
}

struct Args { std::uint32_t a[4]; };

// Each branch's boundaries, on top of the random bytes.
Args Seed(unsigned k) {
    Args args;
    for (std::uint32_t& v : args.a) v = Next();
    g_cells_mostly_none = k == kCellsBlock;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const fs = Field_State;
    switch (k) {
    case kStepping:
        if (Half()) sc[9] = 0;
        break;
    case kStepLands: {
        if (Half()) fs[0x128] = 4;
        static const unsigned char kRun[] = {0x0F, 0x10, 0x11, 0x12, 0};
        sc[7] = kRun[Next() % 5];
        Field_InputFlags = static_cast<unsigned char>(Field_InputFlags & ~3u);
        Field_InputFlags = static_cast<unsigned char>(Field_InputFlags | Next() % 4);
        if (Often()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x2060u);
        if (Often()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x100u);
        if (Half()) sc[0x70] = 0;
        if ((Field_InputFlags & 3) == 2 && Often()) {
            // on an edge the step lands on: 0x20000, or the header's size less 3
            const unsigned d = sc[8] % 8;
            sc[8] = static_cast<unsigned char>(d);
            const std::uint32_t n = sc[0x70];
            const std::uint32_t dx = static_cast<std::uint32_t>(Field_DirectionSteps[d * 2]) * n;
            const std::uint32_t dz = static_cast<std::uint32_t>(Field_DirectionSteps[d * 2 + 1]) * n;
            const std::uint32_t edge = Half() ? 0x20000u : (static_cast<std::uint32_t>(AreaMap_Header[Next() % 2]) - 3u) << 16;
            if (Half()) SetLong(sc + 0x34, static_cast<std::int32_t>(edge - dx));
            else SetLong(sc + 0x38, static_cast<std::int32_t>(edge - dz));
        }
        break;
    }
    case kSwapState:
        sc[2] = static_cast<unsigned char>(Next() % at::kSwapCount);
        break;
    case kMenuState:
        sc[2] = static_cast<unsigned char>(Next() % at::kMenuCount);
        break;
    case kEncState:
        sc[2] = static_cast<unsigned char>(Next() % at::kEncounterCount);
        break;
    case kPassState:
        sc[2] = static_cast<unsigned char>(Next() % at::kPassageStepCount);
        break;
    case kActionState:
        At(at::kPartySet)[0] = static_cast<unsigned char>((Next() % at::kActionCount) | (Half() ? 0x80 : 0));
        if (Half()) fs[0x137] = 0;
        break;
    case kSwapGather:
    case kExchange:
        if (Often()) Field_MemberCount = static_cast<unsigned char>(1 + Next() % 3);
        if (Half()) ObjTrio[0x138] = static_cast<unsigned char>(ObjTrio[0x138] & ~1u);
        for (unsigned i = 0; i < 3; ++i) Member(i)[1] = static_cast<unsigned char>(Often() ? 3 : Next() % 12);
        if (k == kExchange) {
            // the frames: the larger distance moved, >> 13, 0 or not
            const std::uint32_t lx = static_cast<std::uint32_t>(Long(At(at::kLeaderX)));
            const std::uint32_t lz = static_cast<std::uint32_t>(Long(At(at::kLeaderZ)));
            static const std::int32_t kMoved[] = {0, 0x1FFF, 0x2000, -0x2000, -0x1FFF, 0x1FFFFF, 0x7FFFFFFF, 0};
            if (Often()) Field_Kind2X = static_cast<long>(lx + static_cast<std::uint32_t>(kMoved[Next() % 8]));
            if (Often()) Field_Kind2Z = static_cast<long>(lz + static_cast<std::uint32_t>(kMoved[Next() % 8]));
        }
        break;
    case kSpotFree:
        args.a[3] = Garbage(0xFF, Half() ? 0 : Next() & 0xFF);
        break;
    case kSpinOut: {
        if (Often()) Field_MemberCount = static_cast<unsigned char>(2 + Next() % 2);
        const unsigned n = Next() % 5;
        sc[9] = static_cast<unsigned char>(n);   // +1 on entry
        for (unsigned i = 1; i < 3; ++i) {
            unsigned char* const m = Member(i);
            if (Often()) m[1] = 3;
            const int edge = -32 * static_cast<int>(n + 1);
            m[0x5D] = static_cast<unsigned char>(edge + static_cast<int>(Next() % 3) - 1);
        }
        if (Half()) ObjTrio[9] = 3;
        break;
    }
    case kSpinIn:
        if (Often()) Field_MemberCount = static_cast<unsigned char>(2 + Next() % 2);
        if (Often()) Sprite_Current = ObjTrio;
        Sprite_Current[9] = static_cast<unsigned char>(Next() % 3);
        for (unsigned i = 1; i < 3; ++i) if (Often()) Member(i)[1] = 3;
        break;
    case kSwapEnd:
        if (Half()) Field_Kind2Hold = 0;
        break;
    case kMenuWait:
        if (Half()) Field_Request = 1;
        break;
    case kEncStart:
        if (Half()) At(at::kEventBattle)[0] = 0;
        break;
    case kEncWait: {
        if (Half()) sc[0xB] = 0xFF;
        if (sc[0xB] != 0xFF && Half()) Effect_Objects[sc[0xB] * 0x80u] = 0;
        static const unsigned char kCounts[] = {0, 1, 2, 3, 3};
        Field_MemberCount = kCounts[Next() % 5];
        for (unsigned i = 1; i < 3; ++i) if (Often()) Member(i)[1] = 5;
        break;
    }
    case kPassOpen:
    case kPassTake: {
        static const unsigned char kKinds[] = {0, 1, 2, 3, 0xFF, 0xFF, 7};
        g_passage[6] = kKinds[Next() % 7];
        if (Often()) g_passage[5] = static_cast<unsigned char>(g_passage[5] | 0x80);
        if (Half()) g_passage[4] = 0xFF;
        if (k == kPassTake && Often()) Field_Request = 0;
        break;
    }
    case kPassEnd:
        if (Often()) Field_Request = 0;
        break;
    case kEncArea:
        if (Often()) Game_AreaNumber = Word(At(at::kEncounterAreas) + (Next() % 10) * 4);
        else Game_AreaNumber = static_cast<unsigned short>(Next());
        break;
    case kExitCell:
        break;
    case kWalkIn: {
        if (Half()) At(at::kPendingCount)[0] = 0;
        if (Often()) Field_MemberCount = static_cast<unsigned char>(1 + Next() % 3);
        static const std::int32_t kNear[] = {0, 0xFFFF, 0x10000, -0xFFFF, -0x10000, 0x10001, 0x40000};
        SetLong(At(at::kPendingX), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(At(at::kLeaderX))) +
                                                          static_cast<std::uint32_t>(kNear[Next() % 7])));
        SetLong(At(at::kPendingZ), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(At(at::kLeaderZ))) +
                                                          static_cast<std::uint32_t>(kNear[Next() % 7])));
        break;
    }
    case kWayBlocked:
        args.a[2] = Garbage(0xFF, Half() ? 0 : Next() & 0xFF);
        break;
    case kWayBlocked4:
        if (Half()) args.a[0] &= 0xFFFF0000u;
        if (Half()) args.a[1] &= 0xFFFF0000u;
        break;
    case kCellsBlock:
        if (Half()) args.a[2] = 0;
        break;
    default:
        break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[160];
    unsigned ones[kCount], zeros[kCount];
    unsigned frame_pointed;
} g_cover;
void Cover(unsigned k, const State& out, bool pointed) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 160) ++g_cover.logged[out.log[i].what];
    if (kClones[k].ret) ++(out.result & 0xFF ? g_cover.ones : g_cover.zeros)[k];
    if (pointed) ++g_cover.frame_pointed;
}

using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("event_leader: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        const unsigned n = Calls(c);
        bof3::CloneCall calls[32];
        for (unsigned i = 0; i < n; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, static_cast<int>(n));
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Field_LeaderStepping), reinterpret_cast<const void*>(&Field_LeaderStepLands),
        reinterpret_cast<const void*>(&Field_SwapState), reinterpret_cast<const void*>(&Field_SwapGather),
        reinterpret_cast<const void*>(&Field_SpotFree), reinterpret_cast<const void*>(&Field_SwapSpinOut),
        reinterpret_cast<const void*>(&Field_SwapExchange), reinterpret_cast<const void*>(&Field_SwapSpinIn),
        reinterpret_cast<const void*>(&Field_SwapEnd), reinterpret_cast<const void*>(&Field_MenuState),
        reinterpret_cast<const void*>(&Field_MenuOpen), reinterpret_cast<const void*>(&Field_MenuWait),
        reinterpret_cast<const void*>(&Field_EncounterState), reinterpret_cast<const void*>(&Field_EncounterStart),
        reinterpret_cast<const void*>(&Field_EncounterWait), reinterpret_cast<const void*>(&Field_PassageState),
        reinterpret_cast<const void*>(&Field_PassageOpen), reinterpret_cast<const void*>(&Field_PassageTake),
        reinterpret_cast<const void*>(&Field_PassageEnd), reinterpret_cast<const void*>(&Field_ActionState),
        reinterpret_cast<const void*>(&Field_EncounterArea), reinterpret_cast<const void*>(&Field_CellHasEvent),
        reinterpret_cast<const void*>(&Field_ExitFromCell), reinterpret_cast<const void*>(&Field_PendingWalkIn),
        reinterpret_cast<const void*>(&Field_WayBlocked), reinterpret_cast<const void*>(&Field_WayBlocked4),
        reinterpret_cast<const void*>(&Field_CellsBlock)};

    // Swap the tables and the first eight area descriptors in; put back after.
    std::uint32_t saved_tables[kSwappedTotal];
    {
        unsigned n = 0;
        for (unsigned t = 0; t < 5; ++t)
            for (unsigned i = 0; i < kTables[t].count; ++i) {
                std::uint32_t* const slot = reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(kTables[t].at + 4 * i));
                saved_tables[n++] = *slot;
                *slot = Address(reinterpret_cast<const void*>(kTableStubs[t][i]));
            }
    }
    unsigned char* saved_desc[kAreas];
    for (unsigned i = 0; i < 0x1000 + kAreas; ++i) g_scripts[i] = 0x51000000u + i * 0x10;
    for (unsigned a = 0; a < kAreas; ++a) {
        saved_desc[a] = Area_Descriptors[a];
        const std::uint32_t* const list = g_scripts + a;
        std::memcpy(g_desc[a] + 4, &list, sizeof list);
        Area_Descriptors[a] = g_desc[a];
    }

    static State saved, input, their_out, our_out;
    Capture(saved, false);
    g = kStubs;

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.passage) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.name) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.exits) b = static_cast<unsigned char>(Next());
        input.result = 0;
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        const Args args = Seed(k);
        Capture(input, false);
        g_active_in = Address(Field_ActiveMember);

        bool pointed = false;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? ours[k] : clones[k];
            const std::uint32_t r = reinterpret_cast<Fn4>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2], args.a[3]);
            if (pass == 0) pointed = Address(Field_ActiveMember) != g_active_in;
            Capture(out, true);
            out.result = kClones[k].ret == 0 ? 0u : (r & 0xFFu);
        }
        calls += their_out.log_n;
        Cover(k, their_out, pointed);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      event_leader self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    Apply(saved);
    for (unsigned a = 0; a < kAreas; ++a) Area_Descriptors[a] = saved_desc[a];
    {
        unsigned n = 0;
        for (unsigned t = 0; t < 5; ++t)
            for (unsigned i = 0; i < kTables[t].count; ++i)
                *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(kTables[t].at + 4 * i)) = saved_tables[n++];
    }

    bof3::Log("shadow      event_leader self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the party, the actor records, the field's globals, the scratch, the passage / name / exit "
              "records, the result and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      event_leader: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned handlers = 0;
    for (unsigned i = 100; i < 150; ++i) handlers += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      event_leader coverage: table entries %u of 32; lands: change area %u, link %u, cells %u / %u, "
              "jump %u, encounter %u, effect %u, arrive %u, walk %u; swap: spot %u, path %u, sprite %u, palette %u, "
              "elevation %u; passage: script %u (frame pointed %u), messages %u / %u, flags %u / %u, zenny %u, item %u / %u; "
              "walk-in %u; slopes %u, objects %u; spot free 1 %u 0 %u, event cell 1 %u 0 %u, way blocked 1 %u 0 %u, "
              "footprint 1 %u 0 %u, cells block 1 %u 0 %u",
              handlers, c.logged[13], c.logged[14], c.logged[15], c.logged[16], c.logged[18], c.logged[20], c.logged[21],
              c.logged[22], c.logged[24], c.logged[51], c.logged[47], c.logged[30], c.logged[31], c.logged[32], c.logged[36],
              c.frame_pointed, c.logged[37], c.logged[38], c.logged[39], c.logged[40], c.logged[48], c.logged[42],
              c.logged[43], c.logged[44], c.logged[45], c.logged[46], c.ones[kSpotFree], c.zeros[kSpotFree],
              c.ones[kCellEvent], c.zeros[kCellEvent], c.ones[kWayBlocked], c.zeros[kWayBlocked], c.ones[kWayBlocked4],
              c.zeros[kWayBlocked4], c.ones[kCellsBlock], c.zeros[kCellsBlock]);
    if (bad) bof3::Fatal("the event script's leader and steps differ from the original in %u self-test rounds", bad);
}

}  // namespace event_leader

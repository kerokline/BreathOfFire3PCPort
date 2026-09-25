// BOF3X_SHADOW=field_hidden: a differential fuzz of the fifteen field-side
// functions, once at start-up. docs/field_hidden.md section 4.
//
// Fifteen byte-copies, every call out re-aimed at a recording stand-in
// (bof3::CloneCall with `expected`); the two .data tables the dispatchers
// jump through (PartyAction5_Form0States 0x65FBC0, PartyAction5_Forms
// 0x65FC18) pointed at recorders. One round: one function, random bytes in
// every region any of them touches, the pointers and indices put back inside
// what the buffers hold, each branch's boundaries seeded; theirs, then from
// the same state ours; the regions, the fuzz's buffers, the result and the
// stand-ins' log compared. Everything is put back afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_hidden_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_hidden {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
constexpr std::uint32_t kMemberSize = 0x14C;
unsigned char* Member(unsigned i) { return ObjTrio + (i % 3) * kMemberSize; }
void SetPtr(unsigned char* at, const void* p) { SetLong(at, static_cast<std::int32_t>(Address(p))); }
unsigned char* Sc() { return Sprite_Current; }

// --- the fuzz's own buffers ---------------------------------------------------

constexpr unsigned kSrc = 0x400, kDst = 0x800;
unsigned char g_src[kSrc];    // every member's +0x50 points in here
unsigned char g_dst[kDst];    // Sprite_CopyFrames' buffer (or g_src, overlapping)
unsigned char g_name[0x40];   // Item_NamePtr's answer
unsigned char g_op[0x10];     // EventOp_Ex's op
unsigned char g_row[0x10];    // a row Encounter_FillSlots is given outside Encounter_Rows

// --- the stand-ins' log -------------------------------------------------------

constexpr unsigned kLog = 64;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

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
// Field_CellPickup's rounds lean its stand-ins towards the found paths: the
// cell 0xF2 or 0xF8, an effect object free, a Rand nibble of 13..15 or one
// with & 3 clear.
bool g_pickup;

// A byte answer: al as asked, the rest of eax anything.
std::uint32_t AlOf(unsigned al) { return (Hash() & ~0xFFu) | (al & 0xFF); }

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the arrays, indices inside the tables.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    unsigned char* const s = Sc();
    switch ((h >> 4) % 24) {
    case 0: Sprite_Current = Member(v); break;
    case 1: s[8] = static_cast<unsigned char>(v % 16); break;
    case 2: s[2] = static_cast<unsigned char>(v % 3); break;
    case 3: s[0xB] = static_cast<unsigned char>(v % 20); break;
    case 4: s[0xA] = static_cast<unsigned char>(v % 3); break;
    case 5: Field_Request = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 6: At(at::kScratch)[0] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 7: Field_State = Member(v); break;
    case 8: At(at::kLeader137)[0] = static_cast<unsigned char>(v % 2 ? 8 : v); break;
    case 9: At(at::kFacing)[0] = static_cast<unsigned char>(v % 3 ? v % 4 : v); break;
    case 10: At(at::kPlaced)[0] = static_cast<unsigned char>(v % 3); break;
    case 11: SetPtr(s + 0x50, g_src + w % 0x100); break;
    case 12: s[5] = static_cast<unsigned char>(v % 4); break;
    case 13: s[0x4B] = static_cast<unsigned char>(v); break;
    case 14: SetWord(s + 0x58, v % 4); break;
    case 15: SetWord(s + 0x2C, v % 3); break;
    case 16: SetWord(s + 0x3E, h >> 16); break;
    case 17: if (s[0xB] < 20) Effect_Objects[s[0xB] * 0x80u] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 18: Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ (v % 2 ? 2 : 4)); break;
    case 19: SetLong(At(at::kMemberPos + (w % 6) * 4), static_cast<std::int32_t>(h * 0x9E3779B1u)); break;
    case 20: At(at::kEnemies + (w % 8) * at::kEnemyStride)[0] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 21: SetLong(s + (v % 2 ? 0x34 : 0x38), static_cast<std::int32_t>(h * 0x2545F491u)); break;
    case 22: At(v % 2 ? at::kRows + 8 + (w % 8) * 9 : at::kSlotChance + w % 8)[0] = static_cast<unsigned char>(v % 17); break;
    default: g_src[w % kSrc] = static_cast<unsigned char>(v); break;
    }
}

// --- the stand-ins ------------------------------------------------------------

template <unsigned N> void __cdecl StubHandler() { Record(N, Address(Sc()), Sc()[2], Word(Sc() + 0x2C)); Disturb(); }

std::uint32_t __cdecl StubTick() { Record(1, Address(Sc())); Disturb(); return AlOf(Hash() % 2 ? 0 : Hash() >> 8); }
std::uint32_t __cdecl StubTickOnce() { Record(2, Address(Sc())); Disturb(); return AlOf(Hash() % 2 ? 0 : Hash() >> 8); }
std::uint32_t __cdecl StubEffectFree() {
    Record(3);
    Disturb();
    const std::uint32_t h = Hash();
    return AlOf(h % (g_pickup ? 8 : 4) == 0 ? 0xFF : (h >> 8) % 20);
}
void __cdecl StubSetAnimation(unsigned a) { Record(4, a & 0xFF, Address(Sc())); Disturb(); }
std::uint32_t __cdecl StubEnsureAnimation(unsigned a) { Record(5, a & 0xFF, Address(Sc())); Disturb(); return Hash(); }
std::uint32_t __cdecl StubBlocked() {
    Record(6, Address(Sc()), Sc()[8]);
    Disturb();
    return AlOf(Hash() % 2 ? 0 : 1 + (Hash() >> 9) % 0xFF);
}
// The slope: at the steepness test's edge (0x40 / 0x41 as a short, with and
// without upper bits), and anything; the scratch flag zero a third of the time.
long __cdecl StubSlope(long x, long z, unsigned long d) {
    Record(7, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), d & 0xFF);
    const std::uint32_t h = Hash();
    At(at::kScratch)[0] = static_cast<unsigned char>(h % 3 == 0 ? 0 : (h >> 4) | 1);
    Disturb();
    static const std::uint32_t kEdges[] = {0x40, 0x41, 0x3F, 0, 0x7FFF, 0x8000, 0x10040, 0xFFFF0041u, 0xFFFFFFFFu, 0x8041};
    const std::uint32_t k = Hash();
    return static_cast<long>(k % 5 == 0 ? k : kEdges[(k >> 8) % 10]);
}
// The ground: at Sprite_Current's +0x3E, one either side, or anything.
long __cdecl StubGround(long x, long z) {
    Record(8, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    const unsigned here = Word(Sc() + 0x3E);
    Disturb();
    const std::uint32_t h = Hash();
    const unsigned low = h % 4 == 0 ? h : here + (h >> 8) % 3 - 1;
    return static_cast<long>((h & 0xFFFF0000u) | (low & 0xFFFF));
}
void __cdecl StubPlay(unsigned id) { Record(9, id & 0xFFFF); Disturb(); }
std::uint32_t __cdecl StubObjectAt(long x, long z, unsigned margin) {
    Record(10, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), margin);
    Disturb();
    const std::uint32_t h = Hash();
    return AlOf(h % 4 == 0 ? 0xFF : (h >> 8) % 34);
}
std::uint32_t __cdecl StubCellPickup(unsigned x, unsigned z) {
    Record(11, x & 0xFFFF, z & 0xFFFF);
    Disturb();
    return AlOf(Hash() % 3 == 0 ? 1 + (Hash() >> 8) % 0xFF : 0);
}
std::uint32_t __cdecl StubByteAt(unsigned x, unsigned z) {
    Record(12, x & 0xFFFF, z & 0xFFFF);
    Disturb();
    static const unsigned char kCells[] = {0xF2, 0xF2, 0xF8, 0xF8, 0xF3, 0xF9, 0, 0xFF};
    const std::uint32_t h = Hash();
    if (g_pickup && h % 4 != 0) return AlOf((h >> 8) % 3 ? 0xF2 : 0xF8);
    return AlOf(h % 5 == 0 ? h >> 8 : kCells[(h >> 8) % 8]);
}
void __cdecl StubSpawn(unsigned k, unsigned x, unsigned z) { Record(13, k & 0xFF, x & 0xFFFF, z & 0xFFFF); Disturb(); }
// Rand: its low nibble at the draws' edges most of the time (0, 12..15 for
// the zenny, the bounds of the rows and slot chances), bits above anything.
int __cdecl StubRand() {
    Record(14);
    Disturb();
    static const unsigned char kNibbles[] = {0, 1, 12, 13, 14, 15, 15, 5, 7, 9, 3, 11};
    static const unsigned char kPickupNibbles[] = {13, 14, 15, 15, 12, 0};   // the find, then the tenfold's & 3
    const std::uint32_t h = Hash();
    const unsigned nib = g_pickup && h % 2 ? kPickupNibbles[(h >> 4) % 6] : h % 3 == 0 ? (h >> 4) & 0xF : kNibbles[(h >> 4) % 12];
    return static_cast<int>(((h >> 8) << 4 & 0x7FF0) | nib);
}
void __cdecl StubZenny(unsigned v) { Record(15, v); Disturb(); }
// Item_NamePtr: its name buffer, or Text_Records itself one dword on (the copy
// then reads what it just wrote, in order).
unsigned char* __cdecl StubItemName(unsigned c, unsigned i) {
    Record(16, c, i);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 4 == 0 ? At(bof3::addr::Text_Records + 4 * (1 + (h >> 4) % 2)) : g_name + (h >> 8) % 16;
}
std::uint32_t __cdecl StubInventoryAdd(unsigned c, unsigned i, unsigned n) {
    Record(17, c, i, n);
    Disturb();
    return AlOf(Hash() % 2 ? 0 : 1);
}
void __cdecl StubMsg(unsigned id) { Record(18, id); Disturb(); }
void __cdecl StubClearCell(unsigned x, unsigned z) { Record(19, x & 0xFFFF, z & 0xFFFF); Disturb(); }
// The frame upload moves the sprite's +0x50 half the time, and its +5.
unsigned char* __cdecl StubFrameUpload(unsigned frame) {
    Record(20, frame, Address(Sc()));
    const std::uint32_t h = Hash();
    if (h % 2) SetPtr(Sc() + 0x50, g_src + (h >> 8) % 0x100);
    if ((h >> 1) % 3 == 0) Sc()[5] = static_cast<unsigned char>((h >> 20) % 4);
    Disturb();
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(Hash()));
}
void __cdecl StubScriptStart(unsigned position) { Record(21, position & 0xFFFF, Address(Sc()), Sc()[0x4B]); Disturb(); }
void __cdecl StubCopyFrames(unsigned frame, unsigned char* buffer, unsigned size) {
    Record(22, frame, Address(buffer), size);
    const std::uint32_t h = Hash();
    if (h % 3 == 0) Sc()[0x4B] = static_cast<unsigned char>(h >> 8);
    if ((h >> 1) % 3 == 0) SetWord(Sc() + 0x58, (h >> 16) % 4);
    Disturb();
}
// The elevation scribbles on every effect object's +0x38 half the time: a
// position word stored after the call instead of before shows.
long __cdecl StubElevation(long x, long z) {
    Record(23, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    if (Hash() % 2)
        for (unsigned i = 0; i < 20; ++i) SetWord(Effect_Objects + i * 0x80u + 0x38, Hash() >> 16);
    Disturb();
    return static_cast<long>(Hash());
}
std::uint32_t __cdecl StubPickRow() {
    Record(24);
    Disturb();
    const std::uint32_t h = Hash();
    return AlOf(h % 5 == 0 ? h >> 8 : (h >> 8) % 8);
}
std::uint32_t __cdecl StubFillSlots(const unsigned char* row) {
    Record(25, Address(row));
    Disturb();
    const std::uint32_t h = Hash();
    return AlOf(h % 4 == 0 ? 0 : 1 + (h >> 8) % 8);
}
std::uint32_t __cdecl StubPlaceParty() { Record(26); Disturb(); return AlOf(Hash() % 5 == 0 ? 0 : 1 + Hash() % 3); }
std::uint32_t __cdecl StubPlaceEnemies() { Record(27); Disturb(); return AlOf(Hash() % 5 == 0 ? 0 : 1 + Hash() % 3); }
std::uint32_t __cdecl StubReachable() { Record(28); Disturb(); return AlOf(Hash() % 5 == 0 ? 0 : 1 + Hash() % 3); }
void __cdecl StubPush() { Record(29); }
void __cdecl StubPop() { Record(30); }
void __cdecl StubAim(unsigned facing) { Record(31, facing & 0xFF); Disturb(); }
std::uint32_t __cdecl StubPartyCount(unsigned slot) {
    Record(32, slot);
    Disturb();
    return AlOf((Hash() >> 8) % 4);
}
std::uint32_t __cdecl StubOnScreen(long x, long z, unsigned size, unsigned margin) {
    Record(33, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), size & 0xFF, margin & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return AlOf(h % 6 == 0 ? 0 : 1 + (h >> 8) % 0xFF);
}

const Callees kStubs = {
    StubTick, StubTickOnce, StubEffectFree, StubSetAnimation, StubEnsureAnimation, StubBlocked, StubSlope, StubGround,
    StubPlay, StubObjectAt, StubCellPickup, StubByteAt, StubSpawn, StubRand, StubZenny, StubItemName, StubInventoryAdd,
    StubMsg, StubClearCell, StubFrameUpload, StubScriptStart, StubCopyFrames, StubElevation, StubPickRow, StubFillSlots,
    StubPlaceParty, StubPlaceEnemies, StubReachable, StubPush, StubPop, StubAim, StubPartyCount, StubOnScreen,
};
const Handler kForm0Stubs[3] = {&StubHandler<50>, &StubHandler<51>, &StubHandler<52>};
const Handler kFormStubs[3] = {&StubHandler<53>, &StubHandler<54>, &StubHandler<55>};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5893A0: return f(&StubTick);
    case 0x589410: return f(&StubTickOnce);
    case 0x589810: return f(&StubEffectFree);
    case 0x5891F0: return f(&StubSetAnimation);
    case 0x589330: return f(&StubEnsureAnimation);
    case 0x51C390: return f(&StubBlocked);
    case 0x5725C0: return f(&StubSlope);
    case 0x572570: return f(&StubGround);
    case 0x587740: return f(&StubPlay);
    case 0x531CF0: return f(&StubObjectAt);
    case 0x51EBD0: return f(&StubCellPickup);
    case 0x536700: return f(&StubByteAt);
    case 0x524870: return f(&StubSpawn);
    case 0x5B93D2: return f(&StubRand);
    case 0x5307C0: return f(&StubZenny);
    case 0x591680: return f(&StubItemName);
    case 0x590BB0: return f(&StubInventoryAdd);
    case 0x497710: return f(&StubMsg);
    case 0x5728D0: return f(&StubClearCell);
    case 0x5894D0: return f(&StubFrameUpload);
    case 0x589350: return f(&StubScriptStart);
    case 0x589160: return f(&StubCopyFrames);
    case 0x5720C0: return f(&StubElevation);
    case 0x592570: return f(&StubPickRow);
    case 0x5925A0: return f(&StubFillSlots);
    case 0x5920E0: return f(&StubPlaceParty);
    case 0x592600: return f(&StubPlaceEnemies);
    case 0x592760: return f(&StubReachable);
    case 0x5A7B90: return f(&StubPush);
    case 0x5A7BC0: return f(&StubPop);
    case 0x592A30: return f(&StubAim);
    case 0x531BB0: return f(&StubPartyCount);
    case 0x5928F0: return f(&StubOnScreen);
    default: bof3::Fatal("field_hidden: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the copies ---------------------------------------------------------------

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    unsigned ret;   // 0 nothing to compare, 1 the low byte
};

// The E8 sites of each, capstone 2026-09-25 (every other transfer internal
// but the two dispatchers' jmp [abs]).
constexpr Call kEffectStateCalls[] = {{0x37, 0x5893A0}, {0x3E, 0x589810}, {0xE7, 0x5893A0}, {0xF1, 0x5893A0}, {0x10E, 0x5893A0}};
constexpr Call kFinishCalls[] = {{0x10, 0x589410}, {0x23, 0x5891F0}, {0x35, 0x5893A0}, {0x45, 0x589410}};
constexpr Call kBeginCalls[] = {{0x14, 0x51C390}, {0x2E, 0x51C390}, {0x77, 0x5725C0}, {0xA5, 0x589330}, {0xE2, 0x5725C0},
                                {0xFC, 0x572570}, {0x132, 0x5725C0}, {0x14C, 0x572570}, {0x172, 0x587740}, {0x18B, 0x589330}};
constexpr Call kResolveCalls[] = {{0x51, 0x531CF0}, {0x8F, 0x51EBD0}, {0xA5, 0x51EBD0}, {0xB9, 0x51EBD0}, {0xD2, 0x589410}};
constexpr Call kPickupCalls[] = {{0xC, 0x536700},  {0x18, 0x589810}, {0x29, 0x524870}, {0x31, 0x5B93D2}, {0x58, 0x5B93D2},
                                 {0x76, 0x5307C0}, {0x7F, 0x524870}, {0x96, 0x524870}, {0x9F, 0x591680}, {0xCF, 0x590BB0},
                                 {0xE0, 0x587740}, {0xE7, 0x497710}, {0xF3, 0x497710}, {0x10D, 0x5728D0}};
constexpr Call kPoseCalls[] = {{0x10, 0x589160}, {0x2B, 0x589350}, {0x3B, 0x589350}};
constexpr Call kCopyCalls[] = {{0x6, 0x5894D0}};
constexpr Call kAnimCalls[] = {{0x10, 0x589160}, {0x23, 0x589350}};
constexpr Call kExCalls[] = {{0x1, 0x589810}, {0x83, 0x5720C0}};
constexpr Call kPlaceCalls[] = {{0x53, 0x592570}, {0x6A, 0x5925A0}, {0x88, 0x5920E0}, {0x91, 0x592600}, {0x9A, 0x592760},
                                {0xA8, 0x5A7B90}, {0xB4, 0x592A30}, {0xC1, 0x531BB0}, {0x105, 0x5928F0}, {0x11D, 0x531BB0},
                                {0x169, 0x5928F0}, {0x195, 0x5A7BC0}, {0x19F, 0x5A7BC0}};
constexpr Call kRowCalls[] = {{0x1, 0x5B93D2}};
constexpr Call kSlotCalls[] = {{0x29, 0x5B93D2}};

enum : unsigned {
    kResume, kEffectState, kFinish, kForm0, kBegin, kResolve, kPickup, kByForm, kPose, kCopy, kAnim, kEx, kPlace,
    kPickRow, kFillSlots, kCount
};

#define FH_C(name, base, size, calls, ret) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret}
#define FH_P(name, base, size, ret) {name, base, size, nullptr, 0, ret}
const Clone kClones[kCount] = {
    FH_P("Member_ResumeUnlessHeld", 0x51BA60, 0x14, 0),
    FH_C("Member_EffectState", 0x51BBD0, 0x115, kEffectStateCalls, 0),
    FH_C("PartyAction_Finish", 0x51DA30, 0x66, kFinishCalls, 0),
    FH_P("PartyAction5_Form0", 0x51E910, 0x12, 0),
    FH_C("PartyAction5_Form0Begin", 0x51E930, 0x1B1, kBeginCalls, 0),
    FH_C("PartyAction5_Form0Resolve", 0x51EAF0, 0xDB, kResolveCalls, 0),
    FH_C("Field_CellPickup", 0x51EBD0, 0x11F, kPickupCalls, 1),
    FH_P("PartyAction5_ByForm", 0x51F1B0, 0x13, 0),
    FH_C("Sprite_PoseFromSet", 0x589110, 0x45, kPoseCalls, 0),
    FH_C("Sprite_CopyFrames", 0x589160, 0x59, kCopyCalls, 0),
    FH_C("Sprite_AnimFromSet", 0x5891C0, 0x2D, kAnimCalls, 0),
    FH_C("EventOp_Ex", 0x5898D0, 0x9F, kExCalls, 0),
    FH_C("Encounter_Place", 0x591F30, 0x1AB, kPlaceCalls, 1),
    FH_C("Encounter_PickRow", 0x592570, 0x2B, kRowCalls, 1),
    FH_C("Encounter_FillSlots", 0x5925A0, 0x5D, kSlotCalls, 1),
};
#undef FH_C
#undef FH_P

// --- the state both passes start from -----------------------------------------

struct Region { std::uint32_t at, size; };
constexpr Region kRegions[] = {
    {0x802D40, 0x3E4},        // ObjTrio: the members Sprite_Current and Field_State point at, +0x137
    {0x937F88, 4},            // Sprite_Current
    {0x905D98, 4},            // Field_State
    {0x905BA0, 8},            // Field_InputFlags 0x905BA2, Field_ScriptFlags2 0x905BA4
    {0x905E60, 8},            // Field_Kind2Z, Field_Kind2X
    {0x7E11E0, 0xA00},        // Effect_Objects, 20 of 0x80
    {0x7DEE80, 0x1338},       // Sprite_Objects
    {0x802000, 0x290},        // Sprite_ObjectsExtra
    {0x903780, 8},            // the fight's centre
    {0x903850, 0x10},         // the scratch bytes
    {0x66C7D8, 1},            // Field_Request
    {0x904CE0, 0x20},         // Text_Records' first record
    {0x904060, 0x10},         // the party lists
    {0x904AAC, 1},            // the battle's facing
    {0x6BDFF0, 0xA0},         // the scratch sprite and the placement's state to 0x6BE090
    {0x939F00, 0x60},         // the enemy slots
    {0x7E06E0, 0x18},         // the members' spots
    {0x8C5580, 0x2680},       // Encounter_Rows and the enemy kinds
    {0x669744, 0x18},         // constant data from here on - random here, put back after: MoveScript_EffectArg
    {0x6697B0, 0x80},         // Field_DirectionSteps and the 8 rows past it an odd direction can reach
    {0x669CB0, 8},            // Encounter_SlotChance
};
constexpr unsigned SumRegions() {
    unsigned n = 0;
    for (const Region& r : kRegions) n += r.size;
    return n;
}
constexpr unsigned kRegionBytes = SumRegions();

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char src[kSrc], dst[kDst], name[sizeof g_name], op[sizeof g_op], row[sizeof g_row];
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.src, g_src, sizeof g_src);
    std::memcpy(s.dst, g_dst, sizeof g_dst);
    std::memcpy(s.name, g_name, sizeof g_name);
    std::memcpy(s.op, g_op, sizeof g_op);
    std::memcpy(s.row, g_row, sizeof g_row);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_src, s.src, sizeof g_src);
    std::memcpy(g_dst, s.dst, sizeof g_dst);
    std::memcpy(g_name, s.name, sizeof g_name);
    std::memcpy(g_op, s.op, sizeof g_op);
    std::memcpy(g_row, s.row, sizeof g_row);
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

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }

// Random bytes put back inside what the buffers and tables hold.
void Fix() {
    Sprite_Current = Member(Next());
    Field_State = Member(Next());
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const m = Member(i);
        m[0xB] = static_cast<unsigned char>(Next() % 20);
        SetPtr(m + 0x50, g_src + Next() % 0x100);
        m[5] = static_cast<unsigned char>(Next() % 4);
        if (Often()) m[0x89] = static_cast<unsigned char>(Next() % 24);
    }
    for (unsigned k = 0; k < 8; ++k) At(at::kEnemies + k * at::kEnemyStride + 1)[0] = static_cast<unsigned char>(Next() % 0x40);
    if (Half()) Field_Request = 0;
}

struct Args { std::uint32_t a[4]; };

// Each branch's boundaries, on top of the random bytes.
Args Seed(unsigned k) {
    Args args;
    for (std::uint32_t& v : args.a) v = Next();
    unsigned char* const s = Sc();
    g_pickup = k == kPickup;
    switch (k) {
    case kResume:
        if (Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x400u);
        break;
    case kEffectState: {
        static const unsigned char kSub[] = {0, 0, 1, 1, 1, 2, 2, 3, 0xFF};
        s[2] = kSub[Next() % 9];
        if (Half()) At(at::kLeader137)[0] = 8;
        if (Half()) Effect_Objects[s[0xB] * 0x80u] = 0;
        break;
    }
    case kFinish: {
        static const unsigned char kSub[] = {0, 0, 1, 1, 2, 2, 3, 0xFF};
        s[0xB] = kSub[Next() % 8];
        break;
    }
    case kForm0:
        s[2] = static_cast<unsigned char>(Next() % 3);
        break;
    case kByForm:
        SetWord(s + 0x2C, Next() % 3);
        break;
    case kBegin:
        s[8] = static_cast<unsigned char>(Often() ? Next() % 8 : Next() % 16);
        break;
    case kResolve: {
        static const unsigned char kCount[] = {1, 1, 1, 1, 2, 0, 0xFF};
        s[0xA] = kCount[Next() % 7];
        s[8] = static_cast<unsigned char>(Often() ? Next() % 8 : Next() % 16);
        const unsigned d = s[8];
        // the point's x and z with no fraction, each half the time
        if (Half()) SetLong(s + 0x34, static_cast<std::int32_t>((Next() << 16) - 2u * static_cast<std::uint32_t>(Long(At(at::kDirSteps + d * 8)))));
        if (Half()) SetLong(s + 0x38, static_cast<std::int32_t>((Next() << 16) - 2u * static_cast<std::uint32_t>(Long(At(at::kDirSteps + d * 8 + 4)))));
        break;
    }
    case kPickup:
        if (Half()) Field_InputFlags = static_cast<unsigned char>(Field_InputFlags & ~6u);
        break;
    case kPose:
    case kAnim:
    case kCopy: {
        // the animation the sprite already has, half the time; a small size,
        // bits above the low word anything; the set our buffer, or g_src
        // itself a little on (the copy then reads what it wrote)
        const unsigned anim = Half() ? s[0x4B] : Next() & 0xFF;
        // (sizes up to 0x140 into our own buffer: bits 8..15 of the size count)
        const bool own = Often();
        const unsigned n = own && Next() % 4 == 0 ? Next() % 0x141 : Often() ? Next() % 0x40 : Next() % 0x80;
        const std::uint32_t size = Garbage(0xFFFF, Half() ? n : (Half() ? 0 : 1));
        unsigned char* const buffer = own ? g_dst : g_src + Next() % 0x100;
        static const std::uint16_t kPositions[] = {0, 1, 2, 3, 0xFFFF};
        SetWord(s + 0x58, kPositions[Next() % 5]);
        if (k == kAnim) {
            args.a[0] = Garbage(0xFF, anim);
            args.a[2] = Address(buffer);
            args.a[3] = size;
        } else {
            args.a[0] = Garbage(0xFF, anim);
            args.a[1] = Address(buffer);
            args.a[2] = size;
        }
        break;
    }
    case kEx:
        for (unsigned char& b : g_op) b = static_cast<unsigned char>(Next());
        if (Half()) g_op[2] = 0;
        if (Half()) g_op[4] = 0;
        args.a[0] = Address(g_op);
        break;
    case kPlace: {
        static const unsigned char kFacings[] = {0, 1, 2, 3, 4, 5, 0xFF, 0x80};
        args.a[0] = Garbage(0xFF, kFacings[Next() % 8]);
        args.a[1] = Garbage(0xFF, Half() ? 0 : 1 + Next() % 0xFF);
        At(at::kPlaced)[0] = static_cast<unsigned char>(Next() % 4);
        for (unsigned m = 0; m < 3; ++m) {
            static const unsigned char kIds[] = {2, 6, 0, 1, 3, 5, 7};
            At(at::kPartyList2 + m)[0] = kIds[Next() % 7];
        }
        for (unsigned e = 0; e < 8; ++e) At(at::kEnemies + e * at::kEnemyStride)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        break;
    }
    case kPickRow:
        for (unsigned r = 0; r < 8; ++r)
            At(at::kRows + 8 + r * 9)[0] = static_cast<unsigned char>(Often() ? Next() % 5 : Next());
        break;
    case kFillSlots: {
        unsigned char* const row = Half() ? At(at::kRows + (Next() % 8) * 9) : g_row;
        for (unsigned i = 0; i < 8; ++i)
            if (Next() % 4 == 0) row[i] = 0xFF;
        for (unsigned i = 0; i < 8; ++i)
            if (Often()) At(at::kSlotChance + i)[0] = static_cast<unsigned char>(Next() % 17);
        args.a[0] = Address(row);
        break;
    }
    default:
        break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[64];
    unsigned ones[kCount], zeros[kCount];
    unsigned sub[kCount][4];
    unsigned flagged, zenny_ten, name_overlap, copied, restarted, early_off;
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 64) ++g_cover.logged[out.log[i].what];
    if (kClones[k].ret) ++(out.result & 0xFF ? g_cover.ones : g_cover.zeros)[k];
    switch (k) {
    case kEffectState: {
        const unsigned sc = static_cast<unsigned>(Byte(in, 0x937F88) | Byte(in, 0x937F89) << 8 | Byte(in, 0x937F8A) << 16 | Byte(in, 0x937F8B) << 24);
        const unsigned b = Byte(in, sc + 2);
        ++g_cover.sub[k][b < 3 ? b : 3];
        break;
    }
    case kFinish: {
        const unsigned sc = static_cast<unsigned>(Byte(in, 0x937F88) | Byte(in, 0x937F89) << 8 | Byte(in, 0x937F8A) << 16 | Byte(in, 0x937F8B) << 24);
        const unsigned b = Byte(in, sc + 0xB);
        ++g_cover.sub[k][b < 3 ? b : 3];
        break;
    }
    case kBegin: {
        bool steep = true;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 9) steep = false;
        ++g_cover.sub[k][steep ? 0 : 1];
        break;
    }
    case kResolve:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 11) ++g_cover.sub[k][0];
        break;
    case kPickup:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
            if (out.log[i].what == 15) { ++g_cover.sub[k][0]; if (out.log[i].a >= 20) ++g_cover.zenny_ten; }
            if (out.log[i].what == 17) ++g_cover.sub[k][1];
        }
        break;
    case kPose:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 21 && out.log[i].a != 0) ++g_cover.restarted;
        break;
    case kCopy:
        if (std::memcmp(in.dst, out.dst, sizeof in.dst) != 0 || std::memcmp(in.src, out.src, sizeof in.src) != 0) ++g_cover.copied;
        break;
    case kPlace: {
        unsigned pops = 0, screens = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
            if (out.log[i].what == 30) ++pops;
            if (out.log[i].what == 33) ++screens;
        }
        if (pops) ++g_cover.sub[k][0];
        if (screens) ++g_cover.sub[k][1];
        if (Byte(out, at::kPlaced) != Byte(in, at::kPlaced)) ++g_cover.sub[k][2];
        break;
    }
    default:
        break;
    }
}

using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("field_hidden: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Member_ResumeUnlessHeld), reinterpret_cast<const void*>(&Member_EffectState),
        reinterpret_cast<const void*>(&PartyAction_Finish),      reinterpret_cast<const void*>(&PartyAction5_Form0),
        reinterpret_cast<const void*>(&PartyAction5_Form0Begin), reinterpret_cast<const void*>(&PartyAction5_Form0Resolve),
        reinterpret_cast<const void*>(&Field_CellPickup),        reinterpret_cast<const void*>(&PartyAction5_ByForm),
        reinterpret_cast<const void*>(&Sprite_PoseFromSet),      reinterpret_cast<const void*>(&Sprite_CopyFrames),
        reinterpret_cast<const void*>(&Sprite_AnimFromSet),      reinterpret_cast<const void*>(&EventOp_Ex),
        reinterpret_cast<const void*>(&Encounter_Place),         reinterpret_cast<const void*>(&Encounter_PickRow),
        reinterpret_cast<const void*>(&Encounter_FillSlots)};

    static State saved, input, their_out, our_out;
    std::uint32_t saved_form0[3], saved_forms[3];
    std::memcpy(saved_form0, At(at::kForm0States), sizeof saved_form0);
    std::memcpy(saved_forms, At(at::kForms), sizeof saved_forms);
    Capture(saved);
    g = kStubs;
    for (unsigned i = 0; i < 3; ++i) {
        SetPtr(At(at::kForm0States + 4 * i), reinterpret_cast<const void*>(kForm0Stubs[i]));
        SetPtr(At(at::kForms + 4 * i), reinterpret_cast<const void*>(kFormStubs[i]));
    }

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.src) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.dst) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.name) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.op) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.row) b = static_cast<unsigned char>(Next());
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
                bof3::Log("shadow      field_hidden self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    std::memcpy(At(at::kForm0States), saved_form0, sizeof saved_form0);
    std::memcpy(At(at::kForms), saved_forms, sizeof saved_forms);
    Apply(saved);

    bof3::Log("shadow      field_hidden self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the members, the effect and sprite objects, the field and placement globals, the enemy "
              "slots and rows, the tables, the fuzz's buffers, the result and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      field_hidden: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      field_hidden coverage: effect state sub-states %u/%u/%u/%u, finish %u/%u/%u/%u, form-0 states "
              "%u %u %u, forms %u %u %u; begin steep %u flat %u; resolve pickups %u; cell 1 %u 0 %u (zenny %u, tenfold %u, "
              "items %u); poses restarted %u; copies %u; place 1 %u 0 %u (popped %u, on-screen asked %u, slots dropped %u); "
              "rows 1+ %u 0 %u; slots 1+ %u 0 %u",
              c.sub[kEffectState][0], c.sub[kEffectState][1], c.sub[kEffectState][2], c.sub[kEffectState][3],
              c.sub[kFinish][0], c.sub[kFinish][1], c.sub[kFinish][2], c.sub[kFinish][3], c.logged[50], c.logged[51],
              c.logged[52], c.logged[53], c.logged[54], c.logged[55], c.sub[kBegin][0], c.sub[kBegin][1], c.sub[kResolve][0],
              c.ones[kPickup], c.zeros[kPickup], c.sub[kPickup][0], c.zenny_ten, c.sub[kPickup][1], c.restarted, c.copied,
              c.ones[kPlace], c.zeros[kPlace], c.sub[kPlace][0], c.sub[kPlace][1], c.sub[kPlace][2], c.ones[kPickRow],
              c.zeros[kPickRow], c.ones[kFillSlots], c.zeros[kFillSlots]);
    if (bad) bof3::Fatal("the field-side hidden functions differ from the original in %u self-test rounds", bad);
}

}  // namespace field_hidden

// BOF3X_SHADOW=member_sprites: the start-up differential fuzz of
// member_sprites.cpp's 21 functions (docs/member-sprites.md, section 5).
//
// Each original is byte-copied with EVERY call re-aimed at a recording
// stand-in - the calls between the 21 included, so each function is tested
// alone - and ours runs with the same stand-ins through member_sprites::g. The
// two dispatch tables (0x65F960, 0x65F99C) become tables of numbered
// stand-ins, in the copy's operand and in g alike; the three inline jump
// tables (Member_Follow's, Member_StepAhead's, Field_CellKind's) are
// relocated into their copies. A round: one function; random state with each
// branch's boundaries seeded and byte and word arguments given stale upper
// bytes; theirs, the same state again, ours; every byte of the state, the
// stand-ins' log (a count, a hash of every entry and the first 48 kept) and
// the result compared. The stand-ins give back what the caller reads - an
// answer, the object-ahead flag in cell 0, the direction byte, the aim point,
// the cells, the ground - and now and then disturb what the caller reads again
// after the call (Sprite_Current, Field_State, the objects' bytes, the cells,
// the flags, the counts).
//
// Never generated, because the original would fault, loop without end, or
// write outside the compared state: a Sprite_Current / Field_State outside the
// five objects, a state byte past its table (+1 >= 9, +2 >= 2), a walking
// speed of 0 (Field_State +0x128 of 0 or past the table's 6 entries - the
// division faults), an actor index +0x148 above 7, a member index +5 or +6
// above 3, and a catch-up distance of more than about 13 tries (the tries
// grow as the square of it, all of them stand-in calls).
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/member_sprites_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace member_sprites {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <class To, class From> To Cast(From f) { return reinterpret_cast<To>(reinterpret_cast<std::uintptr_t>(f)); }

constexpr unsigned kMemberBytes = 0x14C, kActorBytes = 0xA4, kActors = 8;

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool Half() { return Next() % 2 == 0; }
bool OneIn(unsigned n) { return Next() % n == 0; }
// A byte argument as a caller pushes it: a whole register, stale upper bytes.
std::uint32_t Stale(unsigned byte) { return (Next() & ~0xFFu) | (byte & 0xFF); }
std::uint32_t Stale16(unsigned word) { return (Next() & ~0xFFFFu) | (word & 0xFFFF); }

// --- the stand-ins' log ----------------------------------------------------------

constexpr unsigned kKeep = 48;
struct Log {
    std::uint32_t n, hash;
    std::uint32_t keep[kKeep][4];
};
Log g_log;
std::uint32_t g_seed;

void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log.n < kKeep) {
        g_log.keep[g_log.n][0] = what;
        g_log.keep[g_log.n][1] = a;
        g_log.keep[g_log.n][2] = b;
        g_log.keep[g_log.n][3] = c;
    }
    for (const std::uint32_t v : {what, a, b, c}) g_log.hash = (g_log.hash ^ v) * 0x01000193u + 0x9E3779B9u;
    ++g_log.n;
}
// Deterministic in the call's position: the same on both sides while the
// calls agree.
std::uint32_t Hash(std::uint32_t salt = 0) {
    std::uint32_t h = (g_seed + g_log.n * 0x10001u + salt * 0x3C6EF372u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

// --- the objects and the state ----------------------------------------------------

unsigned char g_scratch[0x150];   // an object that is not a party member
unsigned char g_out[4];           // Field_DirectionTo's destination when it is tested alone

unsigned char* Member(unsigned i) { return ObjTrio + i * kMemberBytes; }
unsigned char* Cells() { return At(at::kCells); }
// Sprite_Current may be any of the five, Field_State one of the four with +0x148.
unsigned char* SpriteChoice(unsigned v) {
    switch (v % 5) {
    case 0: return Member(0);
    case 1: return Member(1);
    case 2: return Member(2);
    case 3: return At(at::kObject905DA0);
    default: return g_scratch;
    }
}
unsigned char* StateChoice(unsigned v) { return v % 4 == 3 ? g_scratch : Member(v % 4); }

// The bytes the originals index by, kept where they are safe.
void FixObject(unsigned char* o, bool state) {
    o[1] = static_cast<unsigned char>(o[1] % 9);
    o[2] = static_cast<unsigned char>(o[2] % 2);
    o[5] = static_cast<unsigned char>(o[5] % 4);
    o[6] = static_cast<unsigned char>(o[6] % 4);
    if (state) {
        o[0x128] = static_cast<unsigned char>(o[0x128] % 5 + 1);
        o[0x148] = static_cast<unsigned char>(o[0x148] % kActors);
    }
}
void Fix() {
    for (unsigned i = 0; i < 3; ++i) FixObject(Member(i), true);
    FixObject(g_scratch, true);
    FixObject(At(at::kObject905DA0), false);
}

// A position near the leader's: 16.16, within about eight cells.
std::int32_t Near(std::int32_t base) { return base + static_cast<std::int32_t>(Next() % 0x100000) - 0x80000; }

// Now and then, something the caller reads again after a call.
void Disturb(std::uint32_t salt) {
    const std::uint32_t h = Hash(salt);
    unsigned char* const c = Sprite_Current;
    switch (h % 28) {
    case 0: ObjTrio[(h >> 8) % ObjTrio_count] = static_cast<unsigned char>(h >> 24); break;
    case 1: Sprite_Current = SpriteChoice(h >> 8); break;
    case 2: Field_State = StateChoice(h >> 8); break;
    case 3: Field_MemberCount = static_cast<unsigned char>((h >> 8) % 4); break;
    case 4: Field_ScriptFlags = static_cast<unsigned short>(h >> 8); break;
    case 5: Field_ScriptFlags2 = static_cast<unsigned short>(h >> 12); break;
    case 6: Field_InputFlags = static_cast<unsigned char>(h >> 8); break;
    case 7: Field_Request = static_cast<unsigned char>((h >> 8) % 3 == 0 ? 0 : h >> 16); break;
    case 8: At(at::kActorRecords)[(h >> 8) % (kActors * kActorBytes)] = static_cast<unsigned char>(h >> 24); break;
    case 9: g_scratch[(h >> 8) % sizeof g_scratch] = static_cast<unsigned char>(h >> 24); break;
    case 10: At(at::kObject905DA0)[(h >> 8) % 0xC8] = static_cast<unsigned char>(h >> 24); break;
    case 11: Cells()[(h >> 8) % 0x100] = static_cast<unsigned char>(h >> 24); break;
    case 12: c[8] = static_cast<unsigned char>((h >> 8) % 8); break;
    case 13: SetLong(c + 0xC, 0); SetLong(c + 0x10, 0); break;
    case 14: c[1] = 1; break;
    case 15: Draw_OtSlot = static_cast<unsigned char>(h >> 8); break;
    case 16: c[9] = static_cast<unsigned char>((h >> 8) % 3); break;
    case 17: Cells()[0] = static_cast<unsigned char>((h >> 8) % 2); break;
    default: break;
    }
    Fix();
}

// --- the stand-ins -------------------------------------------------------------------
// Ids: 1.. the callees in Callees order; 0x40 + n the dispatch tables' entries.

unsigned char Test(unsigned id, unsigned one_in) {
    Disturb(id);
    const std::uint32_t h = Hash(id * 10);
    return static_cast<unsigned char>(h % one_in == 0 ? 1 + (h >> 8) % 255 : 0);
}
// A value that sits on a > 0x40 boundary a good part of the time.
long Height(std::uint32_t salt) {
    const std::uint32_t h = Hash(salt);
    static const unsigned short kEdges[] = {0x3F, 0x40, 0x41, 0x8000, 0x7FFF, 0};
    const unsigned short low = h % 3 ? kEdges[(h >> 4) % 6] : static_cast<unsigned short>(h >> 8);
    return static_cast<long>((Hash(salt + 1) & 0xFFFF0000u) | low);
}

void __cdecl StubRestoreClut() { Record(1, Address(Sprite_Current)); Disturb(1); }
void __cdecl StubClearSteps() {
    Record(2, Address(Sprite_Current));
    Disturb(2);
    if (Hash(20) % 2) {
        SetLong(Sprite_Current + 0xC, 0);
        SetLong(Sprite_Current + 0x10, 0);
    }
}
long __cdecl StubGroundAt(long x, long z) {
    Record(3, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), Address(Sprite_Current));
    Disturb(3);
    const std::uint32_t h = Hash(30);
    return static_cast<long>(h % 2 ? h : (h & 0xFFFF0000u) | (h >> 8) % 0x800);
}
void __cdecl StubSetAnimation(unsigned animation) { Record(4, animation & 0xFF, Address(Sprite_Current)); Disturb(4); }
unsigned char __cdecl StubScriptTick() { Record(5, Address(Sprite_Current)); Disturb(5); return static_cast<unsigned char>(Hash(50)); }
unsigned char __cdecl StubTest535120(unsigned a) { Record(6, a & 0xFF); return Test(6, 6); }
unsigned char __cdecl StubTest535240(unsigned a) { Record(7, a & 0xFF); return Test(7, 6); }
unsigned char __cdecl StubEnsureAnimation(unsigned animation) {
    Record(8, animation & 0xFF, Address(Sprite_Current));
    Disturb(8);
    return static_cast<unsigned char>(Hash(80) % 2);
}
unsigned char __cdecl StubIdle() { Record(9, Address(Sprite_Current)); return Test(9, 5); }
// 0..6 each, and above.
unsigned char __cdecl StubFollowStep() {
    Record(10, Address(Sprite_Current));
    Disturb(10);
    const std::uint32_t h = Hash(100);
    return static_cast<unsigned char>(h % 10 < 8 ? h % 10 : h % 10 == 8 ? 0xFF : 7 + (h >> 8) % 249);
}
unsigned char __cdecl StubTest531DF0() { Record(11); return Test(11, 4); }
void __cdecl Stub534610() { Record(12, Address(Sprite_Current)); Disturb(12); }
void __cdecl Stub535F50() { Record(13, Address(Sprite_Current)); Disturb(13); }
void __cdecl Stub52E140() { Record(14, Address(Sprite_Current)); Disturb(14); }
void __cdecl StubWalkEnd() { Record(15, Address(Sprite_Current)); Disturb(15); }
void __cdecl Stub535270() { Record(16); Disturb(16); }
void __cdecl Stub5350C0() { Record(17); Disturb(17); }
void __cdecl Stub534F10() { Record(18); Disturb(18); }
void __cdecl Stub534A00() { Record(19); Disturb(19); }
unsigned char __cdecl StubTest534920() { Record(20); return Test(20, 6); }
// Member_WalkEnd reads +0xC, +0x10 and +1 after it.
void __cdecl StubFollow() {
    Record(21, Address(Sprite_Current));
    Disturb(21);
    const std::uint32_t h = Hash(210);
    if (h % 2) {
        SetLong(Sprite_Current + 0xC, 0);
        if (h % 4 != 1) SetLong(Sprite_Current + 0x10, 0);
        Sprite_Current[1] = static_cast<unsigned char>(h % 8 < 5 ? 1 : 2);
    }
}
// Member_FollowStep reads the position after it.
void __cdecl StubCatchUp(long x, long z) {
    Record(22, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), Address(Sprite_Current));
    Disturb(22);
    const std::uint32_t h = Hash(220);
    if (h % 3 == 0) {
        SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(x + static_cast<long>((h >> 4) % 0xE0000) - 0x70000));
        SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(z + static_cast<long>((h >> 12) % 0xE0000) - 0x70000));
    }
}
// The destination is Sprite_Current +8 or a stack byte (Member_CatchUp's,
// whose address differs between the copy and ours): only which is recorded.
void __cdecl StubDirectionTo(long x, long z, unsigned char* direction) {
    Record(23, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), direction == Sprite_Current + 8);
    Disturb(23);
    *direction = static_cast<unsigned char>(Hash(230) % 11);
}
// Member_FollowStep reads the aim point after it.
unsigned char __cdecl StubStepAhead() {
    Record(24, Address(Sprite_Current));
    Disturb(24);
    const std::uint32_t h = Hash(240);
    if (h % 2) {   // near the sprite, or on it (as far as it is: the caller's tie)
        const unsigned char* const c = Sprite_Current;
        const bool on = Hash(243) % 3 == 0;
        SetLong(At(at::kAimX), Long(c + 0x34) + (on ? 0 : static_cast<std::int32_t>(Hash(241) % 0x100000) - 0x80000));
        SetLong(At(at::kAimZ), Long(c + 0x38) + (on ? 0 : static_cast<std::int32_t>(Hash(242) % 0x100000) - 0x80000));
    }
    static const unsigned char kAnswers[] = {0, 1, 1, 1, 5, 6, 0xFF, 2};
    return kAnswers[(h >> 8) % 8];
}
unsigned char __cdecl StubBlockedAt(long x, long z, unsigned raised, long ground) {
    Record(25, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), (raised & 0xFF) ^ static_cast<std::uint32_t>(ground) << 8);
    Disturb(25);
    const std::uint32_t h = Hash(250);
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : 1 + (h >> 8) % 255);
}
unsigned char __cdecl StubCellAhead() {
    Record(26, Address(Sprite_Current));
    Disturb(26);
    const std::uint32_t h = Hash(260);
    return static_cast<unsigned char>(h % 3 == 0 ? 3 : h % 10);
}
// It leaves cell 0, which every caller reads.
long __cdecl StubObjectAt(long x, long z, unsigned dir) {
    Record(27, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), dir & 0xFF);
    Disturb(27);
    Cells()[0] = static_cast<unsigned char>(Hash(270) % 3 ? Hash(271) % 2 : Hash(272));
    return Height(273);
}
unsigned char __cdecl StubMapCell(unsigned x, unsigned z) {
    Record(28, x & 0xFFFF, z & 0xFFFF);
    Disturb(28);
    return static_cast<unsigned char>(Hash(280) % 2 ? 0 : Hash(281) % 16);
}
int __cdecl StubRand() { Record(29); Disturb(29); return static_cast<int>(Hash(290)); }
unsigned char __cdecl StubCellAheadRaised() { Record(30, Address(Sprite_Current)); Disturb(30); return static_cast<unsigned char>(Hash(300)); }
unsigned char __cdecl StubCellAheadFlat() { Record(31, Address(Sprite_Current)); Disturb(31); return static_cast<unsigned char>(Hash(310)); }
unsigned char ClassValue(std::uint32_t h) {
    static const unsigned char kClasses[] = {0, 0x10, 0x20, 0x70, 0xB0, 0xA0, 0xA1, 0xA2, 0xA3, 0xAF, 0x25, 0x10, 0x20, 0xB0};
    return h % 8 ? kClasses[(h >> 8) % 14] : static_cast<unsigned char>(h >> 16);
}
// It writes cells 1..5, which Field_CellClass reads.
void __cdecl StubReadCells(unsigned x, unsigned z) {
    Record(32, x & 0xFFFF, z & 0xFFFF, Address(Sprite_Current));
    Disturb(32);
    if (Hash(320) % 2)
        for (unsigned i = 1; i <= 5; ++i) Cells()[i] = ClassValue(Hash(321 + i));
}
unsigned char __cdecl StubCellClass(unsigned a, unsigned b, unsigned c) {
    Record(33, a & 0xFF, b & 0xFF, c & 0xFF);
    Disturb(33);
    return ClassValue(Hash(330));
}
void __cdecl StubTurnUnless(unsigned a, unsigned b, unsigned to) {
    Record(34, a & 0xFF, b & 0xFF, to & 0xFF);
    Disturb(34);
    if (Hash(340) % 2) Sprite_Current[8] = static_cast<unsigned char>(Hash(341) % 8);
}
unsigned char __cdecl StubCellSlope(unsigned cell) { Record(35, cell & 0xFF); Disturb(35); return static_cast<unsigned char>(Hash(350)); }
unsigned char __cdecl StubCellPairTurn(unsigned a, unsigned b, unsigned to, unsigned value) {
    Record(36, a & 0xFF, b & 0xFF, (to & 0xFF) | (value & 0xFF) << 8);
    Disturb(36);
    return static_cast<unsigned char>(Hash(360) % 4 == 0 ? 1 : 0);
}
unsigned char __cdecl StubCellKind(unsigned x, unsigned z, unsigned x0, unsigned z0) {
    Record(37, x & 0xFFFF, z & 0xFFFF, (x0 & 0xFFFF) | (z0 & 0xFFFF) << 16);
    Disturb(37);
    return ClassValue(Hash(370));
}
// Every high nibble, and the bytes Field_CellKind singles out.
unsigned char __cdecl StubByteAt(short x, short z) {
    Record(38, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z));
    Disturb(38);
    static const unsigned char kBytes[] = {0x11, 0x52, 0x70, 0xC0, 0xA0, 0xAE, 0xAF, 0xA2, 0xA3, 0xA1, 0x80, 0x20, 0x21, 0x31, 0xB3, 0xF0};
    const std::uint32_t h = Hash(380);
    // Field_CellKind's second call reads the cell stepped from: 0xC0 a third of the time.
    if (g_log.n % 2 == 0 && h % 3 == 0) return 0xC0;
    return h % 3 ? kBytes[(h >> 8) % 16] : static_cast<unsigned char>(h >> 16);
}
unsigned char __cdecl StubCellFacing(unsigned cell) { Record(39, cell & 0xFF); return Test(39, 4); }
unsigned char __cdecl StubObjectAhead(long x, long z) {
    Record(40, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    return Test(40, 3);
}
template <unsigned N> void __cdecl StubEntry() {
    Record(0x40 + N, Address(Sprite_Current), Sprite_Current[1], Sprite_Current[2]);
    Disturb(0x40 + N);
}

std::uint32_t g_member_states[9], g_control_states[2];
template <unsigned... N> void FillEntries(std::uint32_t* table) {
    const std::uint32_t entries[] = {Address(reinterpret_cast<const void*>(&StubEntry<N>))...};
    for (unsigned i = 0; i < sizeof...(N); ++i) table[i] = entries[i];
}

Callees Stubs() {
    Callees s{};
    s.restore_clut = StubRestoreClut;
    s.member_states = g_member_states;
    s.clear_steps = StubClearSteps;
    s.ground_at = StubGroundAt;
    s.set_animation = Cast<void (__cdecl*)(unsigned char)>(&StubSetAnimation);
    s.control_states = g_control_states;
    s.script_tick = StubScriptTick;
    s.test_535120 = StubTest535120;
    s.test_535240 = StubTest535240;
    s.ensure_animation = Cast<unsigned char (__cdecl*)(unsigned char)>(&StubEnsureAnimation);
    s.idle = StubIdle;
    s.follow_step = StubFollowStep;
    s.test_531df0 = StubTest531DF0;
    s.call_534610 = Stub534610;
    s.call_535f50 = Stub535F50;
    s.call_52e140 = Stub52E140;
    s.walk_end = StubWalkEnd;
    s.call_535270 = Stub535270;
    s.call_5350c0 = Stub5350C0;
    s.call_534f10 = Stub534F10;
    s.call_534a00 = Stub534A00;
    s.test_534920 = StubTest534920;
    s.follow = StubFollow;
    s.catch_up = StubCatchUp;
    s.direction_to = StubDirectionTo;
    s.step_ahead = StubStepAhead;
    s.blocked_at = StubBlockedAt;
    s.cell_ahead = StubCellAhead;
    s.object_at = StubObjectAt;
    s.map_cell = StubMapCell;
    s.rand = StubRand;
    s.cell_ahead_raised = StubCellAheadRaised;
    s.cell_ahead_flat = StubCellAheadFlat;
    s.read_cells = StubReadCells;
    s.cell_class = StubCellClass;
    s.turn_unless = StubTurnUnless;
    s.cell_slope = StubCellSlope;
    s.cell_pair_turn = StubCellPairTurn;
    s.cell_kind = StubCellKind;
    s.byte_at = StubByteAt;
    s.cell_facing = StubCellFacing;
    s.object_ahead = StubObjectAhead;
    return s;
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x534E50: return f(&StubRestoreClut);
    case 0x536650: return f(&StubClearSteps);
    case 0x572570: return f(&StubGroundAt);
    case 0x5891F0: return f(&StubSetAnimation);
    case 0x5893A0: return f(&StubScriptTick);
    case 0x535120: return f(&StubTest535120);
    case 0x535240: return f(&StubTest535240);
    case 0x589330: return f(&StubEnsureAnimation);
    case 0x51BDA0: return f(&StubIdle);
    case 0x51B050: return f(&StubFollowStep);
    case 0x531DF0: return f(&StubTest531DF0);
    case 0x534610: return f(&Stub534610);
    case 0x535F50: return f(&Stub535F50);
    case 0x52E140: return f(&Stub52E140);
    case 0x51AFE0: return f(&StubWalkEnd);
    case 0x535270: return f(&Stub535270);
    case 0x5350C0: return f(&Stub5350C0);
    case 0x534F10: return f(&Stub534F10);
    case 0x534A00: return f(&Stub534A00);
    case 0x534920: return f(&StubTest534920);
    case 0x51AD60: return f(&StubFollow);
    case 0x51B5D0: return f(&StubCatchUp);
    case 0x51B9D0: return f(&StubDirectionTo);
    case 0x51B430: return f(&StubStepAhead);
    case 0x535610: return f(&StubBlockedAt);
    case 0x526DB0: return f(&StubCellAhead);
    case 0x5725C0: return f(&StubObjectAt);
    case 0x592890: return f(&StubMapCell);
    case 0x5B93D2: return f(&StubRand);
    case 0x527640: return f(&StubCellAheadRaised);
    case 0x526DD0: return f(&StubCellAheadFlat);
    case 0x5282C0: return f(&StubReadCells);
    case 0x527470: return f(&StubCellClass);
    case 0x5280F0: return f(&StubTurnUnless);
    case 0x528070: return f(&StubCellSlope);
    case 0x528120: return f(&StubCellPairTurn);
    case 0x528370: return f(&StubCellKind);
    case 0x536700: return f(&StubByteAt);
    case 0x528730: return f(&StubCellFacing);
    case 0x528770: return f(&StubObjectAhead);
    default: bof3::Fatal("member_sprites: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

// --- the 21 copies ------------------------------------------------------------------
// Extents and calls out by capstone, 2026-09-23 (linear disassembly of each
// whole extent, inline tables skipped; every other transfer stays inside).
// `ret`: what the caller may read of eax - 0 nothing, 1 al.

struct Call { std::uint32_t offset, target; };
enum Kind : unsigned char {
    kMemberFrame, kStart, kControl, kFollow, kWalk, kWalkEnd, kFollowStep, kStepAhead, kCatchUp, kDirectionTo, kIdle,
    kCellAhead, kCellAheadFlat, kCellClass, kCellSlope, kTurnUnless, kCellPairTurn, kReadCells, kCellKind, kCellFacing,
    kObjectAhead, kFunctions
};
constexpr unsigned kMaxCalls = 44;
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const void* ours;
    unsigned ret;
    Call calls[kMaxCalls];
};
const auto P = [](auto f) { return reinterpret_cast<const void*>(f); };
const Clone kClones[kFunctions] = {
    {"Field_MemberFrame", 0x51AC50, 0x17, P(&Field_MemberFrame), 0, {{0x0, 0x534E50}}},
    {"Member_Start", 0x51AC70, 0xC5, P(&Member_Start), 0, {{0x1, 0x536650}, {0x45, 0x572570}, {0xA5, 0x5891F0}}},
    {"Member_Control", 0x51AD40, 0x17, P(&Member_Control), 0, {{0x12, 0x5893A0}}},
    {"Member_Follow", 0x51AD60, 0x260, P(&Member_Follow), 0,
     {{0x4D, 0x535120}, {0x5B, 0x535240}, {0x79, 0x589330}, {0x95, 0x51BDA0}, {0xA2, 0x51B050}, {0xBC, 0x531DF0},
      {0xCF, 0x589330}, {0x106, 0x534610}, {0x10B, 0x535F50}, {0x11D, 0x589330}, {0x157, 0x572570}, {0x168, 0x52E140},
      {0x184, 0x536650}, {0x193, 0x589330}, {0x1C9, 0x589330}, {0x208, 0x589330}}},
    {"Member_Walk", 0x51AFC0, 0x1C, P(&Member_Walk), 0, {{0xD, 0x51AFE0}, {0x17, 0x52E140}}},
    {"Member_WalkEnd", 0x51AFE0, 0x6D, P(&Member_WalkEnd), 0,
     {{0x0, 0x535270}, {0x5, 0x5350C0}, {0xA, 0x534F10}, {0xF, 0x534A00}, {0x16, 0x535120}, {0x24, 0x535240},
      {0x30, 0x534920}, {0x39, 0x536650}, {0x4A, 0x51AD60}}},
    {"Member_FollowStep", 0x51B050, 0x3DD, P(&Member_FollowStep), 1,
     {{0x17F, 0x51B5D0}, {0x24E, 0x51B9D0}, {0x256, 0x51B430}, {0x31B, 0x572570}, {0x32D, 0x535610}, {0x393, 0x572570}}},
    {"Member_StepAhead", 0x51B430, 0x198, P(&Member_StepAhead), 1, {{0xD, 0x526DB0}, {0x122, 0x5725C0}, {0x158, 0x572570}}},
    {"Member_CatchUp", 0x51B5D0, 0x3F8, P(&Member_CatchUp), 0,
     {{0x89, 0x572570}, {0x9B, 0x572570}, {0xE7, 0x51B9D0}, {0x142, 0x5725C0}, {0x159, 0x535610}, {0x1A5, 0x572570},
      {0x1BC, 0x535610}, {0x1DE, 0x572570}, {0x1F5, 0x535610}, {0x25F, 0x572570}, {0x276, 0x535610}, {0x298, 0x572570},
      {0x2AF, 0x535610}, {0x365, 0x592890}, {0x3DF, 0x572570}}},
    {"Field_DirectionTo", 0x51B9D0, 0x8E, P(&Field_DirectionTo), 0, {}},
    {"Member_Idle", 0x51BDA0, 0xE6, P(&Member_Idle), 1, {{0x8D, 0x5B93D2}, {0xBB, 0x589330}}},
    {"Field_CellAhead", 0x526DB0, 0x16, P(&Field_CellAhead), 1, {{0xC, 0x527640}, {0x11, 0x526DD0}}},
    {"Field_CellAheadFlat", 0x526DD0, 0x695, P(&Field_CellAheadFlat), 1,
     {{0x58, 0x5282C0}, {0x8E, 0x527470}, {0x9E, 0x527470}, {0xAE, 0x527470}, {0xF1, 0x5280F0}, {0x106, 0x5280F0},
      {0x14F, 0x528070}, {0x179, 0x528070}, {0x198, 0x5280F0}, {0x19F, 0x528070}, {0x1BE, 0x5280F0}, {0x1C5, 0x528070},
      {0x1F7, 0x5725C0}, {0x215, 0x5280F0}, {0x24E, 0x5725C0}, {0x274, 0x5280F0}, {0x295, 0x527470}, {0x2AC, 0x5280F0},
      {0x2C9, 0x5280F0}, {0x2F9, 0x527470}, {0x310, 0x5280F0}, {0x338, 0x5280F0}, {0x361, 0x5280F0}, {0x36B, 0x528070},
      {0x38D, 0x527470}, {0x3BC, 0x528070}, {0x3EC, 0x527470}, {0x42D, 0x528120}, {0x448, 0x528120}, {0x466, 0x528120},
      {0x484, 0x528120}, {0x4A2, 0x528120}, {0x4C0, 0x528120}, {0x4DA, 0x528070}, {0x508, 0x5725C0}, {0x547, 0x527470},
      {0x588, 0x528120}, {0x5A3, 0x528120}, {0x5C1, 0x528120}, {0x5DF, 0x528120}, {0x5FD, 0x528120}, {0x61B, 0x528120},
      {0x638, 0x528070}, {0x666, 0x5725C0}}},
    {"Field_CellClass", 0x527470, 0x1C1, P(&Field_CellClass), 1, {}},
    {"Field_CellSlope", 0x528070, 0x26, P(&Field_CellSlope), 1, {}},
    {"Field_TurnUnless", 0x5280F0, 0x2B, P(&Field_TurnUnless), 0, {}},
    {"Field_CellPairTurn", 0x528120, 0x68, P(&Field_CellPairTurn), 1, {}},
    {"Field_ReadCells", 0x5282C0, 0xA2, P(&Field_ReadCells), 0,
     {{0x1B, 0x528370}, {0x37, 0x528370}, {0x53, 0x528370}, {0x71, 0x528370}, {0x92, 0x528370}}},
    {"Field_CellKind", 0x528370, 0x3BD, P(&Field_CellKind), 1,
     {{0xB6, 0x536700}, {0xC9, 0x536700}, {0x11D, 0x528730}, {0x135, 0x528770}, {0x194, 0x528770}, {0x1AF, 0x528770},
      {0x1C7, 0x528770}, {0x1E2, 0x528770}}},
    {"Field_CellFacing", 0x528730, 0x3C, P(&Field_CellFacing), 1, {}},
    {"Field_ObjectAhead", 0x528770, 0x31, P(&Field_ObjectAhead), 1, {{0x13, 0x5725C0}}},
};

// A copy's `jmp/call [eax*4 + table]` re-aimed at `table`.
void AimTable(void* copy, const Clone& c, std::uint32_t at, std::uint8_t op, std::uint32_t original, const std::uint32_t* table) {
    auto* code = static_cast<std::uint8_t*>(copy);
    std::uint32_t disp;
    std::memcpy(&disp, code + at + 3, sizeof disp);
    if (code[at] != 0xFF || code[at + 1] != op || code[at + 2] != 0x85 || disp != original)
        bof3::Fatal("member_sprites: %s has no [eax*4 + 0x%X] at +0x%X", c.name, (unsigned)original, (unsigned)at);
    disp = Address(table);
    std::memcpy(code + at + 3, &disp, sizeof disp);
    FlushInstructionCache(GetCurrentProcess(), copy, c.size);
}

// --- the state compared -------------------------------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(ObjTrio), ObjTrio_count},
    {at::kCells, 0x100},
    {Address(&Field_ScriptFlags), 2},
    {at::kActorRecords, kActors * kActorBytes},
    {Address(&Field_InputFlags), 6},   // Field_InputFlags, a byte, Field_ScriptFlags2, Field_InputHeld
    {Address(&Field_State), 0xD0},     // Field_State, the object at 0x905DA0
    {Address(&Field_Request), 1},
    {Address(&Field_MemberCount), 1},
    {Address(&Draw_OtSlot), 1},
    {Address(&Sprite_Current), 4},
    {Address(g_scratch), sizeof g_scratch},
    {Address(g_out), sizeof g_out},
};
constexpr unsigned kStateBytes =
    ObjTrio_count + 0x100 + 2 + kActors * kActorBytes + 6 + 0xD0 + 1 + 1 + 1 + 4 + sizeof g_scratch + sizeof g_out;

struct State {
    unsigned char memory[kStateBytes];
    std::uint32_t result;
    Log log;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(s.memory + at, At(r.at), r.size);
        at += r.size;
    }
    std::memcpy(&s.log, &g_log, sizeof g_log);
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(At(r.at), s.memory + at, r.size);
        at += r.size;
    }
    std::memset(&g_log, 0, sizeof g_log);
}
bool Called(const State& s, std::uint32_t id) {
    for (unsigned i = 0; i < kKeep && i < s.log.n; ++i)
        if (s.log.keep[i][0] == id) return true;
    return false;
}
std::uint32_t FirstDifference(const State& a, const State& b) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        for (unsigned i = 0; i < r.size; ++i)
            if (a.memory[at + i] != b.memory[at + i]) return r.at + i;
        at += r.size;
    }
    return 0;
}

// --- the seeds ------------------------------------------------------------------------

// Positions near one another, steps small: the distances the followers test.
void SeedPositions() {
    const std::int32_t base_x = static_cast<std::int32_t>(Next() % 0x2000000), base_z = static_cast<std::int32_t>(Next() % 0x2000000);
    for (unsigned i = 0; i < 5; ++i) {
        unsigned char* const o = SpriteChoice(i);
        if (OneIn(8)) continue;
        SetLong(o + 0x34, Near(base_x) & (OneIn(3) ? ~0 : static_cast<std::int32_t>(0xFFFF0000u)));
        SetLong(o + 0x38, Near(base_z) & (OneIn(3) ? ~0 : static_cast<std::int32_t>(0xFFFF0000u)));
        static const std::int32_t kSteps[] = {0, 0, 0x8000, -0x8000, 0x4000, -0x4000, 0x10000, 0x2000};
        SetLong(o + 0xC, kSteps[Next() % 8]);
        SetLong(o + 0x10, kSteps[Next() % 8]);
        o[9] = static_cast<unsigned char>(Half() ? Next() % 4 : Next());
        o[8] = static_cast<unsigned char>(OneIn(8) ? Next() : Next() % 8);
        if (Half()) o[0x70] = 0;
    }
}
// A cell list for Field_CellClass: the classes, the edges of the tests.
void SeedCells() {
    unsigned char* const cells = Cells();
    for (unsigned i = 0; i < 0x10; ++i) cells[i] = ClassValue(Next());
    if (OneIn(3)) cells[Next() % 6] = 0x70;
    if (OneIn(3)) cells[1 + Next() % 5] = cells[1 + Next() % 5];
}

struct Args { std::uint32_t a, b, c, d; };

Args Seed(unsigned k) {
    Args x{Next(), Next(), Next(), Next()};
    unsigned char* const cur = Sprite_Current;
    switch (k) {
    case kFollow:
        Field_Request = static_cast<unsigned char>(OneIn(5) ? 9 : OneIn(2) ? 0 : Next());
        if (Next() % 4) Field_ScriptFlags2 &= 0xE3FF;
        if (Half()) ObjTrio[1] = static_cast<unsigned char>(OneIn(3) ? 0xA : Next() % 16);
        if (Half()) Field_State[0x89] = static_cast<unsigned char>(Half() ? 2 : Next() % 4);
        cur[8] = static_cast<unsigned char>(OneIn(8) ? Next() : Next() % 8);
        break;
    case kWalk:
        cur[9] = static_cast<unsigned char>(Half() ? 0 : Next() % 3);
        break;
    case kFollowStep:
    case kCatchUp: {
        SeedPositions();
        if (Next() % 4) cur[0] |= 0x80;
        if (Next() % 4) {
            Field_ScriptFlags &= 0xFFFE;
            Field_ScriptFlags2 &= 0xFFFE;
        }
        if (Next() % 4) Member(0)[0x138] &= 0xFE;
        for (unsigned i = 0; i < 3; ++i)
            if (Half()) Member(i)[0x137] = static_cast<unsigned char>(OneIn(3) ? 9 : Next() % 12);
        if (Half()) Member(1)[6] = 0;
        Field_MemberCount = static_cast<unsigned char>(Next() % 4);
        for (unsigned i = 0; i < 3; ++i) Member(i)[0x128] = static_cast<unsigned char>(Next() % 7);
        if (k == kCatchUp) {
            // x, z a few cells away: up to 13 tries (0x100000 is the first
            // distance that makes two).
            static const std::int32_t kFar[] = {0, 0x10000, 0xFFFFF, 0x100000, 0x10FFFF, 0x110000, 0x200000, 0x3F0000,
                                                0x80000, 0x6FFFF, 0x70000, 0x71000};
            const std::int32_t fx = kFar[Next() % 12] * (Half() ? 1 : -1) + static_cast<std::int32_t>(Next() % 0x100) - 0x80;
            const std::int32_t fz = kFar[Next() % 12] * (Half() ? 1 : -1) + static_cast<std::int32_t>(Next() % 0x100) - 0x80;
            x.a = static_cast<std::uint32_t>(Long(cur + 0x34) + fx);
            x.b = static_cast<std::uint32_t>(Long(cur + 0x38) + fz);
            if (Half()) {   // the leader near the sprite's start, or far
                SetLong(Member(0) + 0x34, Long(cur + 0x34) + static_cast<std::int32_t>(Next() % 0x100000) - 0x80000);
                SetLong(Member(0) + 0x38, Long(cur + 0x38) + static_cast<std::int32_t>(Next() % 0x100000) - 0x80000);
            }
        } else if (Half()) {   // already near the one followed
            const unsigned char* const t = Member(cur[6] % 3);
            SetLong(cur + 0x34, Long(t + 0x34) + static_cast<std::int32_t>(Next() % 0x40000) - 0x20000);
            SetLong(cur + 0x38, Long(t + 0x38) + static_cast<std::int32_t>(Next() % 0x40000) - 0x20000);
        }
        break;
    }
    case kStepAhead:
        if (Half()) cur[0x70] = 0;
        cur[8] = static_cast<unsigned char>(OneIn(6) ? Next() : Next() % 8);
        break;
    case kDirectionTo: {
        static const std::int32_t kD[] = {0, 0, 1, -1, 0x10000, -0x10000};
        x.a = static_cast<std::uint32_t>(Long(cur + 0x34) + (OneIn(5) ? static_cast<std::int32_t>(Next()) : kD[Next() % 6]));
        x.b = static_cast<std::uint32_t>(Long(cur + 0x38) + (OneIn(5) ? static_cast<std::int32_t>(Next()) : kD[Next() % 6]));
        x.c = Address(g_out + Next() % 4);
        break;
    }
    case kIdle:
        if (Half()) ObjTrio[0x137] = static_cast<unsigned char>(Next() % 3 + 8);
        Field_Request = static_cast<unsigned char>(Half() ? 0 : Next());
        if (Half()) Field_State[0x136] = static_cast<unsigned char>(0xEF + Next() % 4);
        cur[5] = static_cast<unsigned char>(Half() ? 1 + Next() % 2 : Next() % 4);
        for (unsigned i = 0; i < 3; ++i)
            if (Half()) Member(i)[0x137] = static_cast<unsigned char>(Half() ? 9 : Next());
        break;
    case kCellAhead:
        if (Half()) cur[0x70] = 0;
        break;
    case kCellAheadFlat: {
        if (Next() % 4) Field_ScriptFlags &= 0xFBFF;
        static const std::uint16_t kFractions[] = {0, 0, 0, 0x8000, 1, 0xFFFF};
        SetWord(cur + 0x34, kFractions[Next() % 6]);
        SetWord(cur + 0x38, kFractions[Next() % 6]);
        cur[8] = static_cast<unsigned char>(OneIn(8) ? Next() : Next() % 8);
        SeedCells();
        break;
    }
    case kCellClass: {
        SeedCells();
        static const unsigned char kIndices[] = {0, 1, 2, 3, 4, 5, 8, 9, 10};
        x.a = Stale(OneIn(8) ? Next() : kIndices[Next() % 9]);
        x.b = Stale(OneIn(3) ? 0 : OneIn(8) ? Next() : kIndices[Next() % 9]);
        x.c = Stale(OneIn(3) ? 0 : OneIn(8) ? Next() : kIndices[Next() % 9]);
        break;
    }
    case kCellSlope: {
        static const unsigned char kLow[] = {0, 1, 2, 3, 0xE, 0xF};
        x.a = Stale(Next() % 16);
        Cells()[x.a & 0xFF] = static_cast<unsigned char>((Next() & 0xF0) | kLow[Next() % 6]);
        break;
    }
    case kTurnUnless:
        x.a = Stale(Next() % 8);
        x.b = Stale(Next() % 8);
        cur[8] = static_cast<unsigned char>(Half() ? (Half() ? x.a : x.b) : Next() % 9);
        x.c = Stale(Next());
        break;
    case kCellPairTurn: {
        x.a = Stale(Next() % 16);
        x.b = Stale(Next() % 16);
        x.d = Stale(Next());
        if (Half()) Cells()[x.a & 0xFF] = static_cast<unsigned char>(x.d);
        if (Half()) Cells()[x.b & 0xFF] = static_cast<unsigned char>(x.d);
        x.c = Stale(Next());
        break;
    }
    case kReadCells:
    case kCellKind: {
        static const std::uint16_t kFractions[] = {0, 0, 0x8000, 1};
        SetWord(cur + 0x34, kFractions[Next() % 4]);
        SetWord(cur + 0x38, kFractions[Next() % 4]);
        if (Half()) cur[0x70] = 0;
        if (Half()) cur[5] = 0;
        if (OneIn(4)) Sprite_Current = At(at::kObject905DA0);
        x.a = Stale16(Next());
        x.b = Stale16(Next());
        x.c = Stale16(Half() ? x.a + Next() % 3 - 1 : Next());
        x.d = Stale16(Half() ? x.b + Next() % 3 - 1 : Next());
        break;
    }
    case kCellFacing:
        x.a = Stale(Half() ? 0xB0 | (Next() % 16) : Next());
        if (Half()) Sprite_Current[8] = static_cast<unsigned char>(Half() ? x.a & 0xF : Half() ? 3 : 5);
        break;
    case kObjectAhead:
        break;
    default:
        break;
    }
    Fix();
    return x;
}

std::uint32_t Run(const void* fn, const Args& x) {
    using Any = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
    return reinterpret_cast<Any>(const_cast<void*>(fn))(x.a, x.b, x.c, x.d);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 168000;
    unsigned bytes = 0;
    for (const Region& r : kRegions) bytes += r.size;
    if (bytes != kStateBytes) bof3::Fatal("member_sprites: regions are %u bytes, the state holds %u", bytes, kStateBytes);

    // The copies, every call re-aimed, the tables swapped or relocated.
    FillEntries<0, 1, 2, 3, 4, 5, 6, 7, 8>(g_member_states);
    FillEntries<16, 17>(g_control_states);
    void* theirs[kFunctions];
    for (unsigned k = 0; k < kFunctions; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[kMaxCalls];
        int n = 0;
        for (const Call& call : c.calls) {
            if (call.target == 0) break;
            calls[n++] = {call.offset, StubFor(call.target), call.target};
        }
        theirs[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, n);
    }
    AimTable(theirs[kMemberFrame], kClones[kMemberFrame], 0x10, 0x24, at::kMemberStates, g_member_states);
    AimTable(theirs[kControl], kClones[kControl], 0xB, 0x14, at::kControlStates, g_control_states);
    move_script::Relocate(theirs[kFollow], 0x51AD60, 0x260, {0xB8, 0x244, 7});
    move_script::Relocate(theirs[kStepAhead], 0x51B430, 0x198, {0x118, 0x178, 8});
    move_script::Relocate(theirs[kCellKind], 0x528370, 0x3BD, {0xFE, 0x2A8, 9});
    for (const Kind k : {kFollow, kStepAhead, kCellKind}) FlushInstructionCache(GetCurrentProcess(), theirs[k], kClones[k].size);

    static State saved, input, their_out, our_out;
    Capture(saved);
    g = Stubs();

    unsigned per[kFunctions] = {}, bad = 0, calls = 0;
    // Branch coverage, counted on the original's run.
    enum { kFollowWalk, kFollowHop, kStepNear, kStepLeaderJump, kStepTurnBack, kCatchKept, kCatchWiggle, kAheadObject,
           kIdleFidget, kDirNone, kBranches };
    unsigned branch[kBranches] = {};
    // The answers seen, per function returning one: a bit per value.
    std::uint32_t seen[kFunctions][8] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kFunctions;
        ++per[k];
        for (unsigned i = 0; i < kStateBytes; ++i) input.memory[i] = static_cast<unsigned char>(Next());
        Apply(input);
        Sprite_Current = SpriteChoice(Next());
        Field_State = StateChoice(Next());
        Fix();
        g_seed = Next();
        const Args args = Seed(k);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const std::uint32_t r = Run(pass ? kClones[k].ours : theirs[k], args);
            out.result = kClones[k].ret == 0 ? 0 : r & 0xFF;
            Capture(out);
        }
        calls += their_out.log.n;
        if (kClones[k].ret) seen[k][their_out.result >> 5 & 7] |= 1u << (their_out.result & 31);
        switch (k) {
        case kFollow:
            if (Called(their_out, 14)) ++branch[kFollowWalk];
            if (Called(their_out, 10) && !Called(their_out, 14)) ++branch[kFollowHop];
            break;
        case kFollowStep:
            if (their_out.result == 1 && their_out.log.n == 0) ++branch[kStepNear];
            if (their_out.result == 0xFF) ++branch[kStepLeaderJump];
            if (Called(their_out, 24) && their_out.result == 1) ++branch[kStepTurnBack];
            break;
        case kCatchUp:
            if (Called(their_out, 28)) ++branch[kCatchKept];
            if (their_out.log.n > 8) ++branch[kCatchWiggle];
            break;
        case kCellAheadFlat:
            if (Called(their_out, 27)) ++branch[kAheadObject];
            break;
        case kIdle:
            if (their_out.result == 1) ++branch[kIdleFidget];
            break;
        case kDirectionTo:
            if (std::memcmp(&their_out.memory[kStateBytes - sizeof g_out], &input.memory[kStateBytes - sizeof g_out], sizeof g_out) == 0)
                ++branch[kDirNone];
            break;
        default: break;
        }
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      member_sprites self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, first byte 0x%X",
                      round, kClones[k].name, (unsigned)their_out.log.n, (unsigned)our_out.log.n, (unsigned)their_out.result,
                      (unsigned)our_out.result, (unsigned)FirstDifference(their_out, our_out));
    }
    g = kOriginals;
    Apply(saved);
    auto count = [&](unsigned k) {
        unsigned n = 0;
        for (const std::uint32_t w : seen[k])
            for (std::uint32_t v = w; v; v &= v - 1) ++n;
        return n;
    };
    bof3::Log("shadow      member_sprites self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, %u MISMATCHES; "
              "the objects, cells, flags, actor records, the stand-ins' log and the result compared",
              kRounds, (unsigned)kFunctions, per[0], calls, bad);
    bof3::Log("shadow      member_sprites branches: the follower walked %u, asked and stood or hopped %u; the follow step near %u, jumped to the "
              "leader %u, turned back %u; the catch-up asked the map %u, tried sideways %u; the cell ahead asked for an object %u; "
              "fidgeted %u; no direction %u",
              branch[kFollowWalk], branch[kFollowHop], branch[kStepNear], branch[kStepLeaderJump], branch[kStepTurnBack],
              branch[kCatchKept], branch[kCatchWiggle], branch[kAheadObject], branch[kIdleFidget], branch[kDirNone]);
    bof3::Log("shadow      member_sprites answers seen: follow step %u, step ahead %u, idle %u, cell ahead flat %u, cell class %u, "
              "cell slope %u, pair turn %u, cell kind %u, facing %u, object ahead %u",
              count(kFollowStep), count(kStepAhead), count(kIdle), count(kCellAheadFlat), count(kCellClass), count(kCellSlope),
              count(kCellPairTurn), count(kCellKind), count(kCellFacing), count(kObjectAhead));
    if (bad) bof3::Fatal("the party members' sprites differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace member_sprites

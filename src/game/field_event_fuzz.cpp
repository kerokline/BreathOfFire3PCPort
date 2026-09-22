// BOF3X_SHADOW=field_event: the start-up differential fuzz of field_event.cpp's
// 31 functions (docs/field-event.md, section 5).
//
// Each original is byte-copied with EVERY call re-aimed at a recording
// stand-in - the calls between the 31 included, so each function is tested
// alone - and ours runs with the same stand-ins through field_event::g. The
// three dispatch tables (0x660918, 0x660954, 0x660B60) become tables of
// numbered stand-ins, in the copy's operand and in g alike; PartySet_Select's
// inline jump table is relocated into its copy. A round: one function; random
// state with each branch's boundaries seeded and byte arguments given stale
// upper bytes; theirs, the same state again, ours; every byte of the state,
// the stand-ins' log (a count, a hash of every entry and the first 48 kept)
// and the result compared. The stand-ins give back what the caller reads -
// counts, flags, a zone record, the ground - and now and then disturb what the
// caller reads again after the call (Sprite_Current, Field_State, the member
// count, the flags words, the party lists, the objects' bytes).
//
// Never generated, because the original would fault or write outside the
// compared state: a Sprite_Current / Field_State outside the five objects, a
// state byte past its table (+1 >= 15, +2 >= 3, 0x904EF1 >= 4), more than
// three members, a MoveScript_EffectState entry above 7 (the actor records
// compared are eight), a member id of 24 or more for Party_Join, a member
// index above 2 for Member_ClearState, a zone list with no record that
// matches (Area_ZoneAt would loop), a palette destination outside the 256
// bytes at 0x80D380, and 0x536A60's own loop (it never returns; its
// stand-in does).
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/field_event_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_event {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <class To, class From> To Cast(From f) { return reinterpret_cast<To>(reinterpret_cast<std::uintptr_t>(f)); }

constexpr unsigned kMemberBytes = 0x14C, kActorBytes = 0xA4, kActors = 8;

std::uint32_t g_rng = 0x6D2B79F5u;
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

unsigned char g_scratch[0x150];      // an object that is not a party member
unsigned char g_zones[8 * 8];        // Area_ZoneAt's list: seven records, the last one everything
unsigned char g_bank[8 + 256 * 4 + 8 * 64];   // Sprite_LoadPalette's bank: +4 an offset to 256 offsets
unsigned char g_zone_record[8];      // what the zone_at stand-in returns; +4 the zone

unsigned char* Member(unsigned i) { return ObjTrio + i * kMemberBytes; }
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
unsigned short AreaChoice(unsigned v) {
    switch (v % 5) {
    case 0: return 0xBB;
    case 1: return 0xBD;
    case 2: return 0xBC;
    default: return static_cast<unsigned short>(v / 5 % 0xC8);
    }
}
unsigned char MemberId(unsigned v) { return v % 26 >= 24 ? 0xFF : static_cast<unsigned char>(v % 26); }

// The state bytes the dispatchers index by, kept inside their tables.
void Fix() {
    for (unsigned i = 0; i < 5; ++i) {
        unsigned char* const o = SpriteChoice(i);
        o[1] = static_cast<unsigned char>(o[1] % 15);
        o[2] = static_cast<unsigned char>(o[2] % 3);
    }
    At(at::kPending)[1] = static_cast<unsigned char>(At(at::kPending)[1] % 4);
    Field_MemberCount = static_cast<unsigned char>(Field_MemberCount % 4);
    for (unsigned i = 0; i < MoveScript_EffectState_count; ++i) MoveScript_EffectState[i] = static_cast<unsigned char>(MoveScript_EffectState[i] % kActors);
    for (unsigned i = 0; i < 6; ++i) {
        unsigned char& id = At(at::kPartyLists)[i];
        if (id != 0xFF) id = MemberId(id);
    }
}

// Now and then, something the caller reads again after a call.
void Disturb(std::uint32_t salt) {
    const std::uint32_t h = Hash(salt);
    switch (h % 24) {
    case 0: ObjTrio[(h >> 8) % ObjTrio_count] = static_cast<unsigned char>(h >> 24); break;
    case 1: Sprite_Current = SpriteChoice(h >> 8); break;
    case 2: Field_State = StateChoice(h >> 8); break;
    case 3: Field_MemberCount = static_cast<unsigned char>((h >> 8) % 4); break;
    case 4: Field_ScriptFlags = static_cast<unsigned short>(h >> 8); break;
    case 5: Field_ScriptFlags2 = static_cast<unsigned short>(h >> 12); break;
    case 6: Field_InputFlags = static_cast<unsigned char>(h >> 8); break;
    case 7: Field_InputHeld = static_cast<unsigned short>((h >> 8) & 1 ? 0 : h >> 16); break;
    case 8: Field_Request = static_cast<unsigned char>((h >> 8) % 3 == 0 ? 6 : h >> 16); break;
    case 9: At(at::kPartyLists)[(h >> 8) % 6] = MemberId(h >> 16); break;
    case 10: *At(at::kPartySetCurrent) = static_cast<unsigned char>(h >> 8); break;
    case 11: MoveScript_EffectState[(h >> 8) % MoveScript_EffectState_count] = static_cast<unsigned char>(h >> 16); break;
    case 12: At(at::kActorRecords)[(h >> 8) % (kActors * kActorBytes)] = static_cast<unsigned char>(h >> 24); break;
    case 13: Game_AreaNumber = AreaChoice(h >> 8); break;
    case 14: g_scratch[(h >> 8) % sizeof g_scratch] = static_cast<unsigned char>(h >> 24); break;
    case 15: At(at::kObject905DA0)[(h >> 8) % kActorBytes] = static_cast<unsigned char>(h >> 24); break;
    case 16: MapView_HeightScale = static_cast<unsigned char>(h >> 8); break;
    case 17: SetLong(At(at::kTintSprites) + (h >> 8) % 32 * 12, static_cast<std::int32_t>(Address(SpriteChoice(h >> 16)))); break;
    default: break;
    }
    Fix();
}

// --- the stand-ins -------------------------------------------------------------------
// Ids: 1.. the callees in Callees order; 0x40 + n the dispatch tables' entries.

void __cdecl StubRestoreClut() { Record(1, Address(Sprite_Current)); Disturb(1); }
void __cdecl StubCopyInput() { Record(2, Address(Sprite_Current)); Disturb(2); }
void __cdecl StubClearSteps() { Record(3, Address(Sprite_Current)); Disturb(3); }
long __cdecl StubGroundAt(long x, long z) {
    Record(4, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), Address(Sprite_Current));
    Disturb(4);
    return static_cast<long>(Hash(40));
}
unsigned char __cdecl StubLeaderAnimation(unsigned animation) {
    Record(5, animation & 0xFF, Address(Sprite_Current));
    Disturb(5);
    return static_cast<unsigned char>(Hash(50) % 3);
}
unsigned char __cdecl StubScriptTick() { Record(6, Address(Sprite_Current)); Disturb(6); return static_cast<unsigned char>(Hash(60)); }
// The leader's tests: each true one time in six.
unsigned char Test(unsigned id) {
    Disturb(id);
    const std::uint32_t h = Hash(id * 10);
    return static_cast<unsigned char>(h % 6 == 0 ? 1 + (h >> 8) % 255 : 0);
}
unsigned char __cdecl StubTest535120(unsigned a) { Record(7, a & 0xFF); return Test(7); }
unsigned char __cdecl StubTest535240(unsigned a) { Record(8, a & 0xFF); return Test(8); }
unsigned char __cdecl StubTest5301F0() { Record(9); return Test(9); }
template <unsigned N> unsigned char __cdecl StubTestN() { Record(10 + N); return Test(10 + N); }
void __cdecl StubWalk() { Record(17, Address(Sprite_Current)); Disturb(17); }
const unsigned char* __cdecl StubZoneAt(unsigned x, unsigned z) {
    Record(18, x & 0xFF, z & 0xFF);
    Disturb(18);
    return g_zone_record;
}
// It may also move the zone record's byte +4, which the caller reads again after it.
int __cdecl StubRand() {
    Record(19);
    Disturb(19);
    if (Hash(191) % 4 == 0) g_zone_record[4] = static_cast<unsigned char>(Hash(192) % 16);
    return static_cast<int>(Hash(190));
}
int __cdecl StubPartyCount(unsigned slot) {
    Record(20, slot & 0xFF);
    Disturb(20);
    // Field_PartySetUp's leader-alone loop indexes MoveScript_EffectState (24
    // entries) by the first `count` ids of the party list, and an id of 0xFF
    // there reads the byte past the table (0x66982B, image data) and writes
    // the actor record it names - inside the eight compared records only while
    // that byte is 0. Keep the count within the ids that are real.
    const unsigned char* const list = At(at::kPartyLists);
    unsigned count = Hash(200) % 4;
    while (count > 0 && list[count - 1] == 0xFF) --count;
    return static_cast<int>(count);
}
unsigned char __cdecl StubHasItem(unsigned member, unsigned kind, unsigned item) {
    Record(21, member & 0xFF, kind & 0xFF, item & 0xFF);
    Disturb(21);
    const std::uint32_t h = Hash(210);
    return static_cast<unsigned char>(h % 4 == 0 ? 1 + (h >> 8) % 2 : 0);
}
unsigned char __cdecl StubSetAnimation(unsigned animation) {
    Record(22, animation & 0xFF, Address(Sprite_Current));
    Disturb(22);
    return static_cast<unsigned char>(Hash(220) % 2);
}
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned index) {
    Record(23, Address(bits), index);
    Disturb(23);
    return static_cast<unsigned char>(Hash(230) % 2);
}
void __cdecl StubSetBank(unsigned bank) { Record(24, bank & 0xFFFF, Address(Sprite_Current)); Disturb(24); }
void __cdecl StubMemberSprite(unsigned member, unsigned slot) { Record(25, member & 0xFF, slot & 0xFF, Address(Sprite_Current)); Disturb(25); }
void __cdecl StubPartyLoad(unsigned slot) { Record(26, slot & 0xFF); Disturb(26); }
void __cdecl StubReleaseTint(unsigned char* sprite) { Record(27, Address(sprite)); Disturb(27); }
void __cdecl StubLoadPalette(unsigned short* dst, unsigned index) { Record(28, Address(dst), index & 0xFF, Address(Sprite_Current)); Disturb(28); }
void __cdecl StubPosition(long x, long z, unsigned flags) {
    Record(29, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), flags & 0xFF);
    Disturb(29);
    if (Hash(290) % 3 == 0) SetLong(ObjTrio + 0x34 + (Hash(291) % 2) * 4, static_cast<std::int32_t>(Hash(292)));
}
// 0x533690 is unread: its whole dword is compared (Field_PartySetUp hands on the caller's).
void __cdecl StubPositionAlt(long x, long z, unsigned flags) {
    Record(30, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), flags);
    Disturb(30);
    if (Hash(300) % 3 == 0) SetLong(ObjTrio + 0x34 + (Hash(301) % 2) * 4, static_cast<std::int32_t>(Hash(302)));
}
void __cdecl StubClearActive() { Record(31); Disturb(31); }
void __cdecl StubLeaderFrame() { Record(32, Address(Sprite_Current), Address(Field_State)); Disturb(32); }
void __cdecl StubMemberFrame() { Record(33, Address(Sprite_Current), Address(Field_State)); Disturb(33); }
void __cdecl StubClearAll() { Record(34); Disturb(34); }
unsigned char __cdecl StubSetTint(unsigned char* sprite, unsigned r, unsigned gg, unsigned b, unsigned a) {
    Record(35, Address(sprite), (r & 0xFF) | (gg & 0xFF) << 8 | (b & 0xFF) << 16 | (a & 0xFF) << 24);
    Disturb(35);
    return static_cast<unsigned char>(Hash(350));
}
void __cdecl StubMemberTimers() { Record(36, Address(Field_State)); Disturb(36); }
void __cdecl StubJoinReset() { Record(37); Disturb(37); }
void __cdecl StubClearState(unsigned member) { Record(38, member & 0xFF); Disturb(38); }
void __cdecl StubLoadFirst(unsigned a, unsigned b, unsigned c) { Record(39, a & 0xFF, b & 0xFF, c & 0xFF); Disturb(39); }
void __cdecl StubLoadSecond(unsigned a, unsigned b, unsigned c, unsigned mode) {
    Record(40, a & 0xFF, b & 0xFF, (c & 0xFF) | (mode & 0xFF) << 8);
    Disturb(40);
}
// Done one time in two, and always after 40 calls: the wait loops end.
int __cdecl StubLoadDone() {
    Record(41);
    Disturb(41);
    return g_log.n > 40 || Hash(410) % 2 ? 1 + static_cast<int>(Hash(411) % 3) : 0;
}
void __cdecl StubSleep(int frames) { Record(42, static_cast<std::uint32_t>(frames)); Disturb(42); }
// The current set, the current byte, or anything.
unsigned char __cdecl StubFind(unsigned a, unsigned b, unsigned c) {
    Record(43, a & 0xFF, b & 0xFF, c & 0xFF);
    const unsigned char current = *At(at::kPartySetCurrent);
    Disturb(43);
    const std::uint32_t h = Hash(430);
    switch (h % 4) {
    case 0: return static_cast<unsigned char>(current & 0x7F);
    case 1: return current;
    default: return static_cast<unsigned char>(h >> 8);
    }
}
void __cdecl StubSelect(unsigned set, unsigned mode) { Record(44, set & 0xFF, mode & 0xFF); Disturb(44); }
void __cdecl StubSetError() { Record(45, Word(At(at::kFindIds)), At(at::kFindIds)[2]); }
void __cdecl StubLoadDat(int file) { Record(46, static_cast<std::uint32_t>(file)); Disturb(46); }
void __cdecl StubHeightCheck() {
    Record(47, Address(Sprite_Current));
    Disturb(47);
    if (Hash(470) % 2) MapView_HeightScale = static_cast<unsigned char>(Hash(471) % 2);
}
long __cdecl StubElevation(long x, long z) {
    Record(48, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), MapView_HeightScale);
    Disturb(48);
    return static_cast<long>(Hash(480));
}
void __cdecl StubTintRelease(unsigned index) { Record(49, index & 0xFF); Disturb(49); }
template <unsigned N> void __cdecl StubEntry() {
    Record(0x40 + N, Address(Sprite_Current), Sprite_Current[1], Sprite_Current[2]);
    Disturb(0x40 + N);
}

// The dispatch tables as the fuzz sees them: 15 + 3 + 4 numbered stand-ins.
std::uint32_t g_leader_states[15], g_control_states[3], g_pending_jumps[4];
template <unsigned... N> void FillEntries(std::uint32_t* table, unsigned base) {
    const std::uint32_t entries[] = {Address(reinterpret_cast<const void*>(&StubEntry<N>))...};
    for (unsigned i = 0; i < sizeof...(N); ++i) table[i] = entries[i];
    (void)base;
}

Callees Stubs() {
    Callees s{};
    s.restore_clut = StubRestoreClut;
    s.copy_input = StubCopyInput;
    s.leader_states = g_leader_states;
    s.clear_steps = StubClearSteps;
    s.ground_at = StubGroundAt;
    s.leader_animation = StubLeaderAnimation;
    s.control_states = g_control_states;
    s.script_tick = StubScriptTick;
    s.test_535120 = StubTest535120;
    s.test_535240 = StubTest535240;
    s.test_5301f0 = StubTest5301F0;
    s.tests[0] = StubTestN<0>;
    s.tests[1] = StubTestN<1>;
    s.tests[2] = StubTestN<2>;
    s.tests[3] = StubTestN<3>;
    s.tests[4] = StubTestN<4>;
    s.tests[5] = StubTestN<5>;
    s.tests[6] = StubTestN<6>;
    s.walk = StubWalk;
    s.zone_at = StubZoneAt;
    s.rand = StubRand;
    s.party_count = StubPartyCount;
    s.has_item = StubHasItem;
    s.set_animation = StubSetAnimation;
    s.flags_test = StubFlagsTest;
    s.set_bank = Cast<void (__cdecl*)(unsigned short)>(&StubSetBank);
    s.member_sprite = StubMemberSprite;
    s.party_load = StubPartyLoad;
    s.release_tint = StubReleaseTint;
    s.load_palette = StubLoadPalette;
    s.position = StubPosition;
    s.position_alt = StubPositionAlt;
    s.clear_active = StubClearActive;
    s.leader_frame = StubLeaderFrame;
    s.member_frame = StubMemberFrame;
    s.pending_jumps = g_pending_jumps;
    s.clear_all = StubClearAll;
    s.set_tint = Cast<unsigned char (__cdecl*)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char)>(&StubSetTint);
    s.member_timers = StubMemberTimers;
    s.join_reset = StubJoinReset;
    s.clear_state = StubClearState;
    s.load_first = StubLoadFirst;
    s.load_second = StubLoadSecond;
    s.load_done = StubLoadDone;
    s.sleep = StubSleep;
    s.find = StubFind;
    s.select = StubSelect;
    s.set_error = StubSetError;
    s.load_dat = StubLoadDat;
    s.height_check = StubHeightCheck;
    s.elevation = StubElevation;
    s.tint_release = Cast<void (__cdecl*)(unsigned char)>(&StubTintRelease);
    return s;
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x534E50: return f(&StubRestoreClut);
    case 0x531BD0: return f(&StubCopyInput);
    case 0x536650: return f(&StubClearSteps);
    case 0x572570: return f(&StubGroundAt);
    case 0x5305B0: return f(&StubLeaderAnimation);
    case 0x5893A0: return f(&StubScriptTick);
    case 0x535120: return f(&StubTest535120);
    case 0x535240: return f(&StubTest535240);
    case 0x5301F0: return f(&StubTest5301F0);
    case 0x531950: return f(&StubTestN<0>);
    case 0x530920: return f(&StubTestN<1>);
    case 0x5302C0: return f(&StubTestN<2>);
    case 0x530380: return f(&StubTestN<3>);
    case 0x530800: return f(&StubTestN<4>);
    case 0x5303E0: return f(&StubTestN<5>);
    case 0x530430: return f(&StubTestN<6>);
    case 0x530480: return f(&StubWalk);
    case 0x52FFD0: return f(&StubZoneAt);
    case 0x5B93D2: return f(&StubRand);
    case 0x531BB0: return f(&StubPartyCount);
    case 0x535310: return f(&StubHasItem);
    case 0x589330: return f(&StubSetAnimation);
    case 0x57C140: return f(&StubFlagsTest);
    case 0x589590: return f(&StubSetBank);
    case 0x533BA0: return f(&StubMemberSprite);
    case 0x533CE0: return f(&StubPartyLoad);
    case 0x454DC0: return f(&StubReleaseTint);
    case 0x5366A0: return f(&StubLoadPalette);
    case 0x533580: return f(&StubPosition);
    case 0x533690: return f(&StubPositionAlt);
    case 0x5367A0: return f(&StubClearActive);
    case 0x52D8F0: return f(&StubLeaderFrame);
    case 0x51AC50: return f(&StubMemberFrame);
    case 0x536760: return f(&StubClearAll);
    case 0x454CC0: return f(&StubSetTint);
    case 0x534EC0: return f(&StubMemberTimers);
    case 0x534010: return f(&StubJoinReset);
    case 0x536730: return f(&StubClearState);
    case 0x536850: return f(&StubLoadFirst);
    case 0x536890: return f(&StubLoadSecond);
    case 0x454810: return f(&StubLoadDone);
    case 0x5A9949: return f(&StubSleep);
    case 0x5368F0: return f(&StubFind);
    case 0x536AC0: return f(&StubSelect);
    case 0x536A60: return f(&StubSetError);
    case 0x454590: return f(&StubLoadDat);
    case 0x572590: return f(&StubHeightCheck);
    case 0x5720C0: return f(&StubElevation);
    case 0x454D60: return f(&StubTintRelease);
    default: bof3::Fatal("field_event: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

// --- the 31 copies ------------------------------------------------------------------
// Extents and calls out by capstone, 2026-09-22 (linear disassembly of each
// whole extent; every other transfer stays inside). `ret`: what the caller
// may read of eax - 0 nothing, 1 al, 2 eax.

struct Call { std::uint32_t offset, target; };
enum Kind : unsigned char {
    kLeaderFrame, kLeaderStart, kLeaderControl, kLeaderStand, kZoneCounterRoll, kZoneAt, kLeaderAnimation, kPartyCount,
    kContextReset, kPartySetUp, kPartyFirstFrame, kPartyPosition, kPendingJump, kMemberSprite, kPartyLoad, kPartyJoin,
    kJoinReset, kMemberTimers, kClearSteps, kLoadPalette, kClearState, kClearAll, kClearActive, kSetLoad, kSetLoadFirst,
    kSetLoadSecond, kSetFind, kSetSelect, kGroundAt, kHeightCheck, kReleaseTint, kFunctions
};
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const void* ours;
    unsigned ret;
    Call calls[13];
};
const auto P = [](auto f) { return reinterpret_cast<const void*>(f); };
const Clone kClones[kFunctions] = {
    {"Field_LeaderFrame", 0x52D8F0, 0x2C, P(&Field_LeaderFrame), 0, {{0x0, 0x534E50}, {0x15, 0x531BD0}}},
    {"Field_LeaderStart", 0x52D920, 0x124, P(&Field_LeaderStart), 0, {{0x1, 0x536650}, {0x5A, 0x572570}, {0x104, 0x5305B0}}},
    {"Field_LeaderControl", 0x52DA50, 0x17, P(&Field_LeaderControl), 0, {{0x12, 0x5893A0}}},
    {"Field_LeaderStand", 0x52DA70, 0x118, P(&Field_LeaderStand), 0,
     {{0x2A, 0x535120}, {0x3C, 0x535240}, {0x4C, 0x5301F0}, {0x91, 0x531950}, {0x9A, 0x530920}, {0xA3, 0x5302C0},
      {0xAC, 0x530380}, {0xB5, 0x530800}, {0xBE, 0x5303E0}, {0xC7, 0x530430}, {0xDA, 0x530480}, {0x106, 0x5305B0}}},
    {"Field_ZoneCounterRoll", 0x52FEB0, 0x120, P(&Field_ZoneCounterRoll), 0,
     {{0x26, 0x52FFD0}, {0x63, 0x5B93D2}, {0x82, 0x531BB0}, {0xA9, 0x535310}, {0xD9, 0x535310}}},
    {"Area_ZoneAt", 0x52FFD0, 0x52, P(&Area_ZoneAt), 2, {}},
    {"Field_LeaderAnimation", 0x5305B0, 0x43, P(&Field_LeaderAnimation), 1, {{0xE, 0x589330}, {0x2B, 0x589330}}},
    {"Party_Count", 0x531BB0, 0x20, P(&Party_Count), 2, {}},
    {"ScriptContext_Reset", 0x5322B0, 0x20, P(&ScriptContext_Reset), 0, {}},
    {"Field_PartySetUp", 0x533110, 0x388, P(&Field_PartySetUp), 0,
     {{0x4C, 0x57C140}, {0x5E, 0x531BB0}, {0x148, 0x589590}, {0x17C, 0x589590}, {0x1A9, 0x533BA0}, {0x1DE, 0x531BB0},
      {0x1F6, 0x533CE0}, {0x294, 0x533BA0}, {0x29E, 0x533BA0}, {0x2AA, 0x454DC0}, {0x2D3, 0x5366A0}, {0x332, 0x533580},
      {0x343, 0x533690}}},
    {"Field_PartyFirstFrame", 0x5334A0, 0xD3, P(&Field_PartyFirstFrame), 0, {{0xF, 0x5367A0}, {0x79, 0x52D8F0}, {0x80, 0x51AC50}}},
    {"Field_PartyPosition", 0x533580, 0x102, P(&Field_PartyPosition), 0, {{0xC0, 0x572570}}},
    {"Field_PendingJump", 0x533760, 0x18, P(&Field_PendingJump), 0, {}},
    {"Field_MemberSprite", 0x533BA0, 0x134, P(&Field_MemberSprite), 0, {{0x3C, 0x454DC0}, {0x128, 0x5366A0}}},
    {"Field_PartyLoad", 0x533CE0, 0x120, P(&Field_PartyLoad), 0, {{0xA, 0x536760}, {0x5E, 0x533BA0}, {0xF4, 0x454CC0}, {0xFE, 0x534EC0}}},
    {"Party_Join", 0x533EF0, 0x11A, P(&Party_Join), 1, {{0x2, 0x534010}, {0x67, 0x536730}, {0x94, 0x533BA0}}},
    {"Party_JoinReset", 0x534010, 0x12, P(&Party_JoinReset), 0, {}},
    {"Field_MemberTimers", 0x534EC0, 0x4B, P(&Field_MemberTimers), 0, {}},
    {"Sprite_ClearSteps", 0x536650, 0x1E, P(&Sprite_ClearSteps), 0, {}},
    {"Sprite_LoadPalette", 0x5366A0, 0x55, P(&Sprite_LoadPalette), 0, {}},
    {"Member_ClearState", 0x536730, 0x30, P(&Member_ClearState), 0, {}},
    {"Party_ClearAll", 0x536760, 0x33, P(&Party_ClearAll), 0, {{0x16, 0x536730}}},
    {"Party_ClearActive", 0x5367A0, 0x35, P(&Party_ClearActive), 0, {{0x18, 0x536730}}},
    {"PartySet_Load", 0x5367E0, 0x6C, P(&PartySet_Load), 0,
     {{0x1B, 0x536850}, {0x23, 0x454810}, {0x2E, 0x5A9949}, {0x36, 0x454810}, {0x43, 0x536890}, {0x4B, 0x454810},
      {0x5A, 0x5A9949}, {0x62, 0x454810}}},
    {"PartySet_LoadFirst", 0x536850, 0x38, P(&PartySet_LoadFirst), 0, {{0xF, 0x5368F0}, {0x2F, 0x536AC0}}},
    {"PartySet_LoadSecond", 0x536890, 0x54, P(&PartySet_LoadSecond), 0, {{0xF, 0x5368F0}, {0x4B, 0x536AC0}}},
    {"PartySet_Find", 0x5368F0, 0x16B, P(&PartySet_Find), 1, {{0x15C, 0x536A60}}},
    {"PartySet_Select", 0x536AC0, 0x94, P(&PartySet_Select), 0, {{0x40, 0x454590}, {0x54, 0x454590}, {0x68, 0x454590}, {0x7C, 0x454590}}},
    {"MapView_GroundAt", 0x572570, 0x1F, P(&MapView_GroundAt), 2, {{0x0, 0x572590}, {0xF, 0x5720C0}}},
    {"MapView_CheckHeightScale", 0x572590, 0x2E, P(&MapView_CheckHeightScale), 0, {}},
    {"Sprite_ReleaseTint", 0x454DC0, 0x2B, P(&Sprite_ReleaseTint), 0, {{0x13, 0x454D60}}},
};

// A copy's `jmp/call [reg*4 + table]` re-aimed at `table`.
void AimTable(void* copy, const Clone& c, std::uint32_t at, std::uint8_t op, std::uint32_t original, const std::uint32_t* table) {
    auto* code = static_cast<std::uint8_t*>(copy);
    std::uint32_t disp;
    std::memcpy(&disp, code + at + 3, sizeof disp);
    if (code[at] != 0xFF || code[at + 1] != op || code[at + 2] != 0x85 || disp != original)
        bof3::Fatal("field_event: %s has no [eax*4 + 0x%X] at +0x%X", c.name, (unsigned)original, (unsigned)at);
    disp = Address(table);
    std::memcpy(code + at + 3, &disp, sizeof disp);
    FlushInstructionCache(GetCurrentProcess(), copy, c.size);
}

// --- the state compared -------------------------------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(ObjTrio), ObjTrio_count},
    {at::kFindIds, 0x40},
    {Address(&Field_ScriptFlags), 2},
    {at::kActorRecords, kActors * kActorBytes},
    {at::kPartyLists, 6},
    {at::kPartySetCurrent, 1},
    {at::kJoinState, 9},
    {at::kPending, 0xE},   // the pending jump's two bytes .. Game_AreaNumber
    {at::kLeaderTimer, 1},
    {Address(&Field_InputFlags), 6},   // Field_InputFlags, a byte, Field_ScriptFlags2, Field_InputHeld
    {Address(&Field_State), 0xD0},     // Field_State, the object at 0x905DA0, Field_Kind2Z / X
    {Address(&Field_Request), 1},
    {Address(MoveScript_EffectState), MoveScript_EffectState_count},
    {Address(&Field_MemberCount), 1},
    {Address(&MapView_HeightScale), 1},
    {Address(&Draw_OtSlot), 1},
    {Address(&Sprite_Current), 4},
    {Address(&Gfx_ClutStripDirty), 1},
    {at::kPaletteBase, 0x100},
    {at::kPaletteBase + 0x4000, 0x100},
    {at::kTintSprites - 8, 32 * 12},
    {Address(g_scratch), sizeof g_scratch},
    {Address(g_zone_record), sizeof g_zone_record},
};
constexpr unsigned kStateBytes = ObjTrio_count + 0x40 + 2 + kActors * kActorBytes + 6 + 1 + 9 + 0xE + 1 + 6 + 0xD0 + 1 +
                                 MoveScript_EffectState_count + 1 + 1 + 1 + 4 + 1 + 0x100 + 0x100 + 32 * 12 + sizeof g_scratch + sizeof g_zone_record;

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
// Whether a stand-in with this id was called (the first 48 calls are kept).
bool Called(const State& s, std::uint32_t id) {
    for (unsigned i = 0; i < kKeep && i < s.log.n; ++i)
        if (s.log.keep[i][0] == id) return true;
    return false;
}
// The first differing byte, for the log.
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

// Seven records with random boxes (x0 <= x2, z0 <= z1 mostly), the last one
// everything; each +4 a zone.
void SeedZones() {
    for (unsigned r = 0; r < 8; ++r) {
        unsigned char* const rec = g_zones + r * 8;
        for (unsigned i = 0; i < 8; ++i) rec[i] = static_cast<unsigned char>(Next());
        if (r == 7) {
            rec[0] = 0;
            rec[1] = 0;
            rec[2] = 0xFF;
            rec[3] = 0xFF;
        } else if (!OneIn(8)) {
            if (rec[0] > rec[2]) std::swap(rec[0], rec[2]);
            if (rec[1] > rec[3]) std::swap(rec[1], rec[3]);
        }
    }
}
// A coordinate on or next to one of the boxes' edges.
unsigned char EdgeNear(unsigned axis) {
    const unsigned char* const rec = g_zones + Next() % 7 * 8;
    const int v = rec[axis + (Half() ? 0 : 2)] + static_cast<int>(Next() % 3) - 1;
    return static_cast<unsigned char>(v);
}
// A member id from the party-set table (so that PartySet_Find and
// Field_MemberSprite find it), or anything.
unsigned char TableId() {
    if (OneIn(5)) return OneIn(2) ? 0xFF : static_cast<unsigned char>(Next());
    return At(at::kPartySets)[Next() % (19 * 3)];
}

std::uint32_t g_zone_with_base, g_zone_without_base;   // zone indices, found at start

struct Args { std::uint32_t a, b, c, d; };

Args Seed(unsigned k) {
    Args x{Next(), Next(), Next(), Next()};
    unsigned char* const cur = Sprite_Current;
    switch (k) {
    case kLeaderFrame:
        *At(at::kLeaderTimer) = static_cast<unsigned char>(Half() ? 0 : Next() % 3);
        break;
    case kLeaderStart:
        if (Half()) Field_InputFlags &= 0xDF;
        if (Half()) Game_AreaNumber = 0xBD;
        break;
    case kLeaderStand:
        if (Next() % 4) Field_ScriptFlags &= 0xFEFF;
        if (Next() % 4) Field_ScriptFlags2 &= 0xFFBF;
        Field_Request = static_cast<unsigned char>(OneIn(4) ? 5 : Next() % 8);
        if (Half()) Field_InputHeld = 0;
        break;
    case kZoneCounterRoll: {
        if (Next() % 4) Field_ScriptFlags &= 0xFFDF;
        // Zones 1..6 have the table's large bases (120 plus a roll of up to 31
        // overflows a byte when doubled); above 15 the table runs into other data.
        g_zone_record[4] = static_cast<unsigned char>(OneIn(3) ? g_zone_without_base : Half() ? g_zone_with_base + Next() % 6 : Next());
        const unsigned base = At(at::kZoneBases)[g_zone_record[4] * 2u];
        std::uint16_t counter;
        switch (Next() % 6) {
        case 0: counter = 0; break;
        case 1: counter = static_cast<std::uint16_t>(base - 1); break;
        case 2: counter = static_cast<std::uint16_t>(base); break;
        case 3: counter = static_cast<std::uint16_t>(base + 1); break;
        default: counter = static_cast<std::uint16_t>(Next()); break;
        }
        SetWord(At(at::kZoneCounter), counter);
        x.a = Stale(Half() ? 0 : Next() % 3);
        break;
    }
    case kZoneAt: {
        SeedZones();
        x.a = Stale(EdgeNear(0));
        x.b = Stale(EdgeNear(1));
        break;
    }
    case kLeaderAnimation:
        x.a = Stale(Next());
        break;
    case kPartyCount: {
        const unsigned slot = Half() ? 0 : 1;
        const unsigned end = Next() % 4;
        for (unsigned i = 0; i < 3; ++i) At(at::kPartyLists)[slot * 3 + i] = i == end ? 0xFF : MemberId(Next() % 24);
        x.a = Stale(OneIn(8) ? Next() : slot);
        break;
    }
    case kContextReset:
        x.a = Address(g_scratch + Next() % 0x40);
        break;
    case kPartySetUp: {
        static const unsigned char kInputs[] = {0, 1, 8, 9, 0x40, 0x41, 0x48};
        Field_InputFlags = static_cast<unsigned char>((Next() & ~0x49u) | (Half() ? kInputs[Next() % 7] : Next() & 0x49));
        if (Half()) Field_ScriptFlags2 &= 0x7FFF;
        if (Half()) Field_ScriptFlags &= 0xF7FF;
        static const unsigned char kWho[] = {0, 7, 9, 1};
        At(at::kActorRecords)[9] = kWho[Next() % 4];
        x.c = Half() ? x.c & ~0x80u : x.c | 0x80u;
        if (Half()) x.c &= ~0x80u;
        break;
    }
    case kPartyFirstFrame:
        if (Half()) Field_ScriptFlags2 &= 0x7FFF;
        if (Half()) Field_InputFlags &= 0xF7;
        if (Half()) Field_ScriptFlags &= 0xFFF8;
        for (unsigned i = 0; i < 3; ++i) Member(i)[5] = static_cast<unsigned char>(Half() ? 0 : Next());
        break;
    case kPartyPosition:
        for (unsigned i = 0; i < 3; ++i)
            if (Half()) Member(i)[0x70] = 0;
        if (Half()) Field_ScriptFlags2 &= 0xBFFF;
        x.c = OneIn(4) ? x.c : (x.c & ~0xFFu) | Next() % 8;
        break;
    case kPendingJump:
        if (Half()) At(at::kPending)[0] = 0;
        break;
    case kMemberSprite: {
        *At(at::kPartySetCurrent) = static_cast<unsigned char>(Half() ? Next() % 19 | (Half() ? 0x80 : 0) : Next());
        const unsigned char* const row = At(at::kPartySets) + (*At(at::kPartySetCurrent) & 0x7Fu) * 3u;
        x.a = Stale(Half() ? row[Next() % 3] : Next());
        x.b = Stale(OneIn(4) ? Next() : Next() % 3);
        break;
    }
    case kPartyLoad:
        if (Half()) Field_Request = 6;
        x.a = Stale(Half() ? 0 : OneIn(4) ? Next() : 1);
        break;
    case kPartyJoin:
        Field_MemberCount = static_cast<unsigned char>(Half() ? 3 : Next() % 3);
        x.a = Stale(Next() % 24);
        break;
    case kMemberTimers:
        break;
    case kLoadPalette: {
        SetLong(cur + 0x4C, static_cast<std::int32_t>(Address(g_bank)));
        x.a = at::kPaletteBase + Next() % 0xC1;   // odd ones too: the twin's lost bit
        x.b = Stale(Next());
        break;
    }
    case kClearState:
        x.a = Stale(Next() % 3);
        break;
    case kSetLoad:
    case kSetLoadFirst:
    case kSetLoadSecond:
    case kSetFind: {
        x.a = Stale(TableId());
        x.b = Stale(TableId());
        x.c = Stale(TableId());
        if (k == kSetFind && Half()) {   // the current row, holding them or not
            const unsigned row = Next() % 19;
            const unsigned char* const r = At(at::kPartySets) + row * 3;
            *At(at::kPartySetCurrent) = static_cast<unsigned char>(row | (Half() ? 0x80 : 0));
            x.a = Stale(r[0]);
            x.b = Stale(Half() ? r[1] : 0xFF);
            x.c = Stale(Half() ? r[2] : OneIn(3) ? TableId() : 0xFF);
        } else {
            *At(at::kPartySetCurrent) = static_cast<unsigned char>(OneIn(4) ? 0xFF : Next());
        }
        x.d = Stale(Half() ? 0 : Next() % 5);
        break;
    }
    case kSetSelect:
        x.a = Stale(Next());
        x.b = Stale(OneIn(8) ? Next() : Next() % 5);
        break;
    case kHeightCheck:
        if (Half()) Sprite_Current = At(at::kObject905DA0);
        break;
    case kReleaseTint: {
        x.a = Address(SpriteChoice(Next()));
        for (unsigned i = 0; i < 32; ++i)
            if (OneIn(4)) SetLong(At(at::kTintSprites) + i * 12, static_cast<std::int32_t>(x.a));
        break;
    }
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
    constexpr unsigned kRounds = 186000;
    unsigned bytes = 0;
    for (const Region& r : kRegions) bytes += r.size;
    if (bytes != kStateBytes) bof3::Fatal("field_event: regions are %u bytes, the state holds %u", bytes, kStateBytes);

    // The copies, every call re-aimed, the tables swapped.
    FillEntries<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14>(g_leader_states, 0);
    FillEntries<16, 17, 18>(g_control_states, 0);
    FillEntries<20, 21, 22, 23>(g_pending_jumps, 0);
    void* theirs[kFunctions];
    for (unsigned k = 0; k < kFunctions; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[13];
        int n = 0;
        for (const Call& call : c.calls) {
            if (call.target == 0) break;
            calls[n++] = {call.offset, StubFor(call.target)};
        }
        theirs[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, n);
    }
    AimTable(theirs[kLeaderFrame], kClones[kLeaderFrame], 0x25, 0x24, at::kLeaderStates, g_leader_states);
    AimTable(theirs[kLeaderControl], kClones[kLeaderControl], 0xB, 0x14, at::kControlStates, g_control_states);
    AimTable(theirs[kPendingJump], kClones[kPendingJump], 0x10, 0x24, at::kPendingJumps, g_pending_jumps);
    move_script::Relocate(theirs[kSetSelect], 0x536AC0, 0x94, {0x31, 0x84, 4});
    FlushInstructionCache(GetCurrentProcess(), theirs[kSetSelect], 0x94);

    // A zone with a base and one without, from the exe's own table.
    g_zone_with_base = g_zone_without_base = 0x100;
    for (unsigned z = 0; z < 0x100; ++z) {
        const unsigned char base = At(at::kZoneBases)[z * 2];
        if (base != 0 && g_zone_with_base == 0x100) g_zone_with_base = z;
        if (base == 0 && g_zone_without_base == 0x100) g_zone_without_base = z;
    }
    if (g_zone_with_base == 0x100 || g_zone_without_base == 0x100) bof3::Fatal("field_event: no zone with and without a base");
    // The palette bank: +4 the offset of 256 offsets, each to one of 8 palettes.
    SetLong(g_bank + 4, 8);
    for (unsigned i = 0; i < 256; ++i) SetLong(g_bank + 8 + i * 4, static_cast<std::int32_t>(256 * 4 + (i % 8) * 64 + (i / 8 % 2)));
    for (unsigned i = 8 + 256 * 4; i < sizeof g_bank; ++i) g_bank[i] = static_cast<unsigned char>(i * 7 + 3);

    static State saved, input, their_out, our_out;
    Capture(saved);
    const std::uint32_t saved_zone_list = static_cast<std::uint32_t>(Long(At(at::kZoneLists)));
    g = Stubs();

    unsigned per[kFunctions] = {}, bad = 0, calls = 0;
    // Branch coverage, counted on the original's run: the zone counter's three
    // paths, the set-up's, the leader's walk, the first frame's members, and
    // the party set not found.
    enum { kZoneFlag, kZoneNoBase, kZoneRoll, kSetupLoad, kSetupMembers, kSetupPosition, kStandWalk,
           kFirstFrames, kFindMissing, kBranches };
    unsigned branch[kBranches] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kFunctions;
        ++per[k];
        for (unsigned i = 0; i < kStateBytes; ++i) input.memory[i] = static_cast<unsigned char>(Next());
        Apply(input);
        Sprite_Current = SpriteChoice(Next());
        Field_State = StateChoice(Next());
        Game_AreaNumber = AreaChoice(Next());
        if (Half()) Field_ScriptFlags2 &= 0x3FFF;
        for (unsigned i = 0; i < 32; ++i)
            if (OneIn(6)) SetLong(At(at::kTintSprites) + i * 12, static_cast<std::int32_t>(Address(SpriteChoice(Next()))));
        Fix();
        g_seed = Next();
        const Args args = Seed(k);
        // Area_ZoneAt's list for this round's area, put back after.
        const std::uint32_t list_at = at::kZoneLists + Game_AreaNumber * 4u;
        const std::uint32_t list_saved = static_cast<std::uint32_t>(Long(At(list_at)));
        SetLong(At(list_at), static_cast<std::int32_t>(Address(g_zones)));
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const std::uint32_t r = Run(pass ? kClones[k].ours : theirs[k], args);
            out.result = kClones[k].ret == 0 ? 0 : kClones[k].ret == 1 ? r & 0xFF : r;
            Capture(out);
        }
        SetLong(At(list_at), static_cast<std::int32_t>(list_saved));
        calls += their_out.log.n;
        switch (k) {
        case kZoneCounterRoll:
            if (their_out.log.n == 0) ++branch[kZoneFlag];
            else if (Called(their_out, 19)) ++branch[kZoneRoll];
            else ++branch[kZoneNoBase];
            break;
        case kPartySetUp:
            if (Called(their_out, 26)) ++branch[kSetupLoad];
            if (Called(their_out, 25)) ++branch[kSetupMembers];
            if (Called(their_out, 29) || Called(their_out, 30)) ++branch[kSetupPosition];
            break;
        case kLeaderStand:
            if (Called(their_out, 17)) ++branch[kStandWalk];
            break;
        case kPartyFirstFrame:
            if (Called(their_out, 32) || Called(their_out, 33)) ++branch[kFirstFrames];
            break;
        case kSetFind:
            if (Called(their_out, 45)) ++branch[kFindMissing];
            break;
        default: break;
        }
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      field_event self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, first byte 0x%X",
                      round, kClones[k].name, (unsigned)their_out.log.n, (unsigned)our_out.log.n, (unsigned)their_out.result,
                      (unsigned)our_out.result, (unsigned)FirstDifference(their_out, our_out));
    }
    g = kOriginals;
    Apply(saved);
    if (static_cast<std::uint32_t>(Long(At(at::kZoneLists))) != saved_zone_list) bof3::Fatal("field_event: the zone lists were not put back");
    bof3::Log("shadow      field_event self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, %u MISMATCHES; "
              "the objects, flags, party lists, actor records, palettes, tint records, the stand-ins' log and the result compared",
              kRounds, (unsigned)kFunctions, per[0], calls, bad);
    bof3::Log("shadow      field_event branches: the zone counter stopped by the flag %u / no base %u / rolled %u; the set-up loaded "
              "the party %u, set a member's sprite %u, placed them %u; the leader walked %u; the first frame ran a member's %u; "
              "the party set was not in the table %u",
              branch[kZoneFlag], branch[kZoneNoBase], branch[kZoneRoll], branch[kSetupLoad], branch[kSetupMembers],
              branch[kSetupPosition], branch[kStandWalk], branch[kFirstFrames],
              branch[kFindMissing]);
    if (bad) bof3::Fatal("the event script's field side differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace field_event

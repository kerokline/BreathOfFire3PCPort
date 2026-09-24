// BOF3X_SHADOW=inventory_ops: a differential fuzz of the encounter placement
// and the battle intro's party steps, once at start-up.
// docs/inventory_ops.md section 6.
//
// Twenty-five byte-copies, every call out re-aimed at a recording stand-in (no
// jump tables, and no jump leaves any of them - capstone, 2026-09-23). One
// round: one function, random bytes in every region any of them touches,
// then that function's branch boundaries seeded; theirs, then from the same
// state ours; the regions, the answer (at the width the original defines)
// and the stand-ins' log compared. The cell map 0x8C3D80 is only read (by
// AreaMap_CellNibble), so it is filled for that function's rounds and put
// back at the end, not compared.
//
// The stand-ins are as loud as the real callees where the caller reads after
// the call: each may move the centre, a member's spot or placed bytes, the
// order, the facing, the counts in memory, an enemy slot, the height sum,
// the camera words, Sprite_Current, Field_State, Field_MemberCount, the
// battle's facing and the party objects' fields - everything some function
// here reads again after a call. The ones answering a byte leave stale bits
// above al (they are typed to answer the whole eax and cast); the sign and
// the absolute value answer the true value (with stale high bits) three
// times in four, so that the walks reach their ends.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/inventory_ops_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace inventory_ops {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using u32 = std::uint32_t;

u32 Address(const void* p) { return static_cast<u32>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

u32 g_rng = 0x6A09E667u;
u32 Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
u32 Garbage(u32 low_bits, u32 value) { return (Next() & ~low_bits) | (value & low_bits); }
u32 Pick(std::initializer_list<u32> values) { return values.begin()[Next() % values.size()]; }

constexpr unsigned kLog = 160;
struct Entry { u32 what, a, b, c, d, e; };
Entry g_log[kLog];
unsigned g_log_n;
u32 g_seed;          // the stand-ins' own stream: the same on both passes
unsigned g_bias;     // this round's odds that a yes-or-no stand-in says yes, in quarters
unsigned g_hint;     // Encounter_CellFits's b, for the nibble stand-in
unsigned g_margin;   // Encounter_OnScreen's margin, for the projection stand-ins

u32 Hash(u32 salt) {
    u32 h = (g_seed + g_log_n * 0x2545F491u + salt * 0x9E3779B9u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
void Record(u32 what, u32 a = 0, u32 b = 0, u32 c = 0, u32 d = 0, u32 e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}

constexpr unsigned kObjects = 5;   // ObjTrio and the two slots after it that Encounter_RollInitiative walks
unsigned char* Obj(unsigned i) { return ObjTrio + at::kObjStride * i; }
unsigned char* EnemySlot(unsigned k) { return At(at::kEnemies + at::kEnemyStride * k); }
unsigned char* ActorRecord(unsigned i) { return At(at::kRecords + at::kRecordSize * i); }

// Where Sprite_Current may point: the five objects or the placement's
// scratch sprite. Field_State: the five objects.
unsigned char* AnySprite(u32 h) { return h % 6 == 5 ? At(at::kScratchSprite) : Obj(h % 6); }

// Every cell below is one some function reads again after a call. The
// invariants Tidy keeps are kept: pointers into the regions, at most five
// members, CLUT rows 0..31.
void Disturb() {
    const u32 h = Hash(1);
    if (h % 4 == 0) return;
    const u32 v = Hash(2);
    const unsigned w = h >> 8;
    switch ((h >> 2) % 20) {
    case 0: SetLong(At(at::kCentreX), static_cast<std::int32_t>(v)); break;
    case 1: SetLong(At(at::kCentreZ), static_cast<std::int32_t>(v)); break;
    case 2: SetLong(At(at::kMemberPos + 8 * (w % 4) + 4 * ((w >> 4) & 1)), static_cast<std::int32_t>(v)); break;
    case 3: At(at::kMembers + 4 * (w % 4) + ((w >> 4) & 1))[0] = static_cast<unsigned char>(v & 1); break;
    case 4: At(at::kOrder + w % 3)[0] = static_cast<unsigned char>(v % 4); break;
    case 5: At(at::kFacing)[0] = static_cast<unsigned char>(v % 8); break;
    case 6: At(at::kPlaced)[0] = static_cast<unsigned char>(v % 9); break;
    case 7: At(at::kEnemyCount)[0] = static_cast<unsigned char>(v % 9); break;
    case 8: {
        unsigned char* const e = EnemySlot(w % 8);
        switch ((w >> 4) % 3) {
        case 0: e[0] = static_cast<unsigned char>(v & 1); break;
        case 1: SetLong(e + 4, static_cast<std::int32_t>(v)); break;
        default: SetLong(e + 8, static_cast<std::int32_t>(v)); break;
        }
        break;
    }
    case 9: SetLong(At(at::kHeightAvg), static_cast<std::int32_t>(v)); break;
    case 10: Sprite_Current = AnySprite(v); break;
    case 11: Field_State = Obj(v % kObjects); break;
    case 12: Field_MemberCount = static_cast<unsigned char>(v % 6); break;
    case 13: At(at::kBattleFacing)[0] = static_cast<unsigned char>(v % 5); break;
    case 14: {
        unsigned char* const o = Obj(w % kObjects);
        switch ((w >> 4) % 8) {
        case 0: o[8] = static_cast<unsigned char>(v % 8); break;
        case 1: o[0xB] = static_cast<unsigned char>(v % 4); break;
        case 2: o[5] = static_cast<unsigned char>(v % 32); break;
        case 3: o[0x89] = static_cast<unsigned char>(v % 10); break;
        case 4: o[0x148] = static_cast<unsigned char>(v % 9); break;
        case 5: o[0x5D + v % 3] = static_cast<unsigned char>(v >> 8); break;
        case 6: SetLong(o + 0x34 + 4 * ((v >> 8) & 1), static_cast<std::int32_t>(v)); break;
        default: o[0x138] = static_cast<unsigned char>(v >> 8); break;
        }
        break;
    }
    case 15: At(at::kPartyList + w % 6)[0] = static_cast<unsigned char>(v % 12); break;
    case 16: SetWord(At(0x903800 + 2 * (w % 2)), v); break;   // Camera_ShiftX / ShiftY
    case 17: SetWord(At(0x903840), v); break;                  // Camera_Distance
    case 18: At(at::kBattleFlags)[0] = static_cast<unsigned char>(v); break;
    default: SetLong(At(0x905E60 + 4 * (w & 1)), static_cast<std::int32_t>(v)); break;   // Field_Kind2Z / X
    }
}

// A yes-or-no answer by this round's odds, with stale bits above al (and now
// and then a yes other than 1).
u32 Yes(u32 salt) {
    const u32 h = Hash(salt);
    const bool yes = (h >> 8) % 4 < g_bias;
    return (h & 0xFFFFFF00u) | (yes ? (h % 3 == 0 ? 1u + (h >> 4) % 0xFF : 1u) : 0u);
}

// A screen coordinate near the tests' edges.
float PickFloat(bool y, u32 h) {
    const float edge = y ? 240.0f : 320.0f;
    switch (h % 12) {
    case 0: return edge;
    case 1: return edge - 0.001f;
    case 2: return edge + 0.5f;
    case 3: return static_cast<float>(g_margin) + 8.0f;
    case 4: return static_cast<float>(g_margin) + 7.99f;
    case 5: return static_cast<float>(g_margin) + 8.01f;
    case 6: {
        const u32 nan = 0x7FC00000u | (h >> 12);
        float f;
        std::memcpy(&f, &nan, 4);
        return f;
    }
    case 7: return -1000.0f;
    default: return static_cast<float>(static_cast<int>((h >> 4) % 500) - 60) + static_cast<float>((h >> 16) & 0xFF) / 256.0f;
    }
}

// --- the stand-ins ---------------------------------------------------------
// Those answering a byte are typed to answer the whole eax; kStubs casts them.

u32 __cdecl StubPartyCount(unsigned slot) {
    Record(1, slot);
    Disturb();
    const u32 h = Hash(3);
    const unsigned n = (h >> 8) % 8 < 6 ? (h >> 12) % 4 : (h >> 12) % 9;
    return (h & 0xFFFFFF00u) | n;
}
long __cdecl StubElevation(long x, long y) {
    Record(2, static_cast<u32>(x), static_cast<u32>(y));
    Disturb();
    return static_cast<long>(Hash(4));
}
u32 __cdecl StubObjectAt(long x, long y, unsigned margin) {
    Record(3, static_cast<u32>(x), static_cast<u32>(y), margin & 0xFF);
    Disturb();
    const u32 h = Hash(5);
    return h % 3 == 0 ? h : (h | 0xFF);
}
void WritePoint(unsigned long* sxy, u32 h) {
    const float f[2] = {PickFloat(false, h), PickFloat(true, h >> 5)};
    std::memcpy(sxy, f, sizeof f);
}
long __cdecl StubRotTransPers(const short* v, unsigned long* sxy, long* p) {
    Record(4, static_cast<std::uint16_t>(v[0]), static_cast<std::uint16_t>(v[1]), static_cast<std::uint16_t>(v[2]));
    WritePoint(sxy, Hash(6));
    *p = static_cast<long>(Hash(7));
    Disturb();
    return static_cast<long>(Hash(8));
}
u32 Pack(const short* s, unsigned i) {
    return static_cast<std::uint16_t>(s[i]) | static_cast<u32>(static_cast<std::uint16_t>(s[i + 1])) << 16;
}
void RecordMatrix(u32 what, const short* m) { Record(what, Pack(m, 0), Pack(m, 2), Pack(m, 4), Pack(m, 6), static_cast<std::uint16_t>(m[8])); }
short* __cdecl StubRotMatrix(const short* angles, short* m) {
    Record(5, Address(angles));
    const u32 h = Hash(9);
    for (unsigned i = 0; i < 9; ++i) m[i] = static_cast<short>(h * (i + 3) >> 7);
    Disturb();
    return m;
}
void __cdecl StubApplyMatrix(const short* m, const short* v, long* out) {
    RecordMatrix(6, m);
    Record(7, static_cast<std::uint16_t>(v[0]), static_cast<std::uint16_t>(v[1]), static_cast<std::uint16_t>(v[2]));
    const u32 h = Hash(10);
    for (unsigned i = 0; i < 3; ++i) out[i] = static_cast<long>(h * (2 * i + 5));
    Disturb();
}
// The MATRIX: nine shorts, two bytes of padding nobody writes, three longs at +0x14.
void RecordTranslation(u32 what, const unsigned long* matrix) {
    const auto* const b = reinterpret_cast<const unsigned char*>(matrix);
    Record(what, static_cast<u32>(Long(b + 0x14)), static_cast<u32>(Long(b + 0x18)), static_cast<u32>(Long(b + 0x1C)));
}
void __cdecl StubSetRot(const unsigned long* matrix) {
    RecordMatrix(8, reinterpret_cast<const short*>(matrix));
    RecordTranslation(9, matrix);
    Disturb();
}
void __cdecl StubSetTrans(const unsigned long* matrix) {
    RecordMatrix(10, reinterpret_cast<const short*>(matrix));
    RecordTranslation(11, matrix);
    Disturb();
}
int __cdecl StubRand() {
    Record(12);
    Disturb();
    return static_cast<int>(Hash(11));
}
long __cdecl StubGroundAt(long x, long z) {
    Record(13, static_cast<u32>(x), static_cast<u32>(z));
    Disturb();
    return static_cast<long>(Hash(12));
}
u32 __cdecl StubEffectFree() {
    Record(14);
    Disturb();
    const u32 h = Hash(13);
    return (h & 0xFFFFFF00u) | (h % 5 == 0 ? 0xFFu : (h >> 8) % 20);
}
void __cdecl StubSetAnimFrom(unsigned a, unsigned b, unsigned char* buffer, unsigned size) {
    Record(15, a, b, Address(buffer), size, Address(Sprite_Current));
    Disturb();
}
u32 __cdecl StubScriptTick() {
    Record(16, Address(Sprite_Current));
    Disturb();
    return Hash(14);
}
void __cdecl StubShadeBegin() {
    Record(17, Address(Sprite_Current));
    Disturb();
}
u32 __cdecl StubShadeRaise(unsigned step) {
    Record(18, step & 0xFF, Address(Sprite_Current));
    Disturb();
    return Yes(15);
}
u32 __cdecl StubScriptOnce() {
    Record(19, Address(Sprite_Current));
    Disturb();
    return Yes(16);
}
void __cdecl StubSetAnimation(unsigned a) {
    Record(20, a & 0xFF, Address(Sprite_Current));
    Disturb();
}
u32 __cdecl StubPlaceMember(unsigned f, unsigned m, unsigned l, unsigned facing) {
    Record(21, f & 0xFF, m & 0xFF, l & 0xFF, facing & 0xFF);
    Disturb();
    return Yes(17);
}
u32 __cdecl StubMemberClear(unsigned m) {
    Record(22, m & 0xFF);
    Disturb();
    return Yes(18);
}
u32 __cdecl StubMemberStands(unsigned m) {
    Record(23, m & 0xFF);
    Disturb();
    return Yes(19);
}
u32 __cdecl StubEnemyClear(unsigned count, long x, long z, unsigned size) {
    Record(24, count & 0xFF, static_cast<u32>(x), static_cast<u32>(z), size & 0xFF);
    Disturb();
    return Yes(20);
}
u32 __cdecl StubNibble(unsigned x, unsigned z) {
    Record(25, x & 0xFFFF, z & 0xFFFF);
    Disturb();
    const u32 h = Hash(21);
    switch ((h >> 8) % 4) {
    case 0: return h >> 4;
    case 1: return (h & 0xFFFFFF00u) | (h >> 12) % 16;
    default: return (h & 0xFFFFFF00u) | ((g_hint + (h >> 12) % 7 - 1) & 0xFF);
    }
}
long __cdecl StubProject(long x, long z, long y, unsigned long* sxy) {
    Record(26, static_cast<u32>(x), static_cast<u32>(z), static_cast<u32>(y));
    WritePoint(sxy, Hash(22));
    Disturb();
    return static_cast<long>(Hash(23));
}
u32 __cdecl StubApart(long x0, long z0, long x1, long z1, unsigned s0, unsigned s1) {
    Record(27, static_cast<u32>(x0), static_cast<u32>(z0), static_cast<u32>(x1), static_cast<u32>(z1), (s0 & 0xFF) | (s1 & 0xFF) << 8);
    Disturb();
    return Yes(24);
}
u32 __cdecl StubCellFits(long x, long z, unsigned size) {
    Record(28, static_cast<u32>(x), static_cast<u32>(z), size & 0xFF);
    Disturb();
    return Yes(25);
}
u32 __cdecl StubPathClear(unsigned x0, unsigned z0, unsigned x1, unsigned z1) {
    Record(29, x0 & 0xFFFF, z0 & 0xFFFF, x1 & 0xFFFF, z1 & 0xFFFF);
    Disturb();
    return Yes(26);
}
// The two word helpers: the true answer three times in four, with stale high
// bits (Encounter_PathClear's original keeps the whole eax: lea ebp, [esi +
// eax]), else noise. The whole argument is logged: both sides compute it
// whole from the same arguments.
u32 __cdecl StubAbs(unsigned v) {
    Record(30, v);
    const u32 h = Hash(27);
    const auto s = static_cast<std::int16_t>(v);
    return h % 4 ? (h & 0xFFFF0000u) | static_cast<std::uint16_t>(s < 0 ? -s : s) : h;
}
u32 __cdecl StubSign(unsigned v) {
    Record(31, v);
    const u32 h = Hash(28);
    const auto s = static_cast<std::int16_t>(v);
    return h % 4 ? (h & 0xFFFF0000u) | static_cast<std::uint16_t>(s > 0 ? 1 : s < 0 ? -1 : 0) : h;
}
u32 __cdecl StubStepOpen(unsigned x0, unsigned z0, unsigned x1, unsigned z1) {
    Record(32, x0 & 0xFFFF, z0 & 0xFFFF, x1 & 0xFFFF, z1 & 0xFFFF);
    Disturb();
    return Yes(29);
}
u32 __cdecl StubTurnSense(unsigned t) {
    Record(33, t & 0xFF, Address(Sprite_Current));
    Disturb();
    return Hash(30);
}
u32 __cdecl StubShadeLower(unsigned step) {
    Record(34, step & 0xFF, Address(Sprite_Current));
    Disturb();
    return Yes(31);
}

// A stand-in answering the whole eax stood in for a callee typed to answer a
// byte or a word: deliberate, and sound for cdecl on x86 (the caller reads al
// or ax of the same register).
template <typename F, typename G>
F As(G g) { return reinterpret_cast<F>(reinterpret_cast<void (*)()>(g)); }

Callees Stubs() {
    Callees s = kOriginals;
    s.party_count = As<decltype(s.party_count)>(&StubPartyCount);
    s.elevation = &StubElevation;
    s.object_at = As<decltype(s.object_at)>(&StubObjectAt);
    s.rot_trans_pers = &StubRotTransPers;
    s.rot_matrix = &StubRotMatrix;
    s.apply_matrix = &StubApplyMatrix;
    s.set_rot_matrix = &StubSetRot;
    s.set_trans_matrix = &StubSetTrans;
    s.rand = &StubRand;
    s.ground_at = &StubGroundAt;
    s.effect_free = As<decltype(s.effect_free)>(&StubEffectFree);
    s.set_anim_from = &StubSetAnimFrom;
    s.script_tick = As<decltype(s.script_tick)>(&StubScriptTick);
    s.shade_begin = &StubShadeBegin;
    s.shade_raise = As<decltype(s.shade_raise)>(&StubShadeRaise);
    s.script_once = As<decltype(s.script_once)>(&StubScriptOnce);
    s.set_animation = As<decltype(s.set_animation)>(&StubSetAnimation);
    s.place_member = As<decltype(s.place_member)>(&StubPlaceMember);
    s.member_clear = As<decltype(s.member_clear)>(&StubMemberClear);
    s.member_stands = As<decltype(s.member_stands)>(&StubMemberStands);
    s.enemy_clear = As<decltype(s.enemy_clear)>(&StubEnemyClear);
    s.cell_nibble = As<decltype(s.cell_nibble)>(&StubNibble);
    s.project = &StubProject;
    s.apart = As<decltype(s.apart)>(&StubApart);
    s.cell_fits = As<decltype(s.cell_fits)>(&StubCellFits);
    s.path_clear = As<decltype(s.path_clear)>(&StubPathClear);
    s.short_abs = &StubAbs;
    s.short_sign = As<decltype(s.short_sign)>(&StubSign);
    s.step_open = As<decltype(s.step_open)>(&StubStepOpen);
    s.turn_sense = As<decltype(s.turn_sense)>(&StubTurnSense);
    s.shade_lower = As<decltype(s.shade_lower)>(&StubShadeLower);
    return s;
}

const void* StubFor(u32 target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x531BB0: return f(&StubPartyCount);
    case 0x5720C0: return f(&StubElevation);
    case 0x531CF0: return f(&StubObjectAt);
    case 0x5A8250: return f(&StubRotTransPers);
    case 0x5A8060: return f(&StubRotMatrix);
    case 0x5A7BF0: return f(&StubApplyMatrix);
    case 0x5A8DE0: return f(&StubSetRot);
    case 0x5A8E00: return f(&StubSetTrans);
    case 0x5B93D2: return f(&StubRand);
    case 0x572570: return f(&StubGroundAt);
    case 0x589810: return f(&StubEffectFree);
    case 0x5891C0: return f(&StubSetAnimFrom);
    case 0x5893A0: return f(&StubScriptTick);
    case 0x534590: return f(&StubShadeBegin);
    case 0x534800: return f(&StubShadeRaise);
    case 0x589410: return f(&StubScriptOnce);
    case 0x5891F0: return f(&StubSetAnimation);
    case 0x5922A0: return f(&StubPlaceMember);
    case 0x592400: return f(&StubMemberClear);
    case 0x5924E0: return f(&StubMemberStands);
    case 0x592800: return f(&StubEnemyClear);
    case 0x592890: return f(&StubNibble);
    case 0x5929D0: return f(&StubProject);
    case 0x592BD0: return f(&StubApart);
    case 0x592C30: return f(&StubCellFits);
    case 0x592CD0: return f(&StubPathClear);
    case 0x592E00: return f(&StubAbs);
    case 0x592E10: return f(&StubSign);
    case 0x592E30: return f(&StubStepOpen);
    case 0x52F570: return f(&StubTurnSense);
    case 0x534880: return f(&StubShadeLower);
    default: bof3::Fatal("inventory_ops: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the twenty-five copies (capstone, 2026-09-23: every jump internal, no
// jump table; the calls below are every call that leaves) --------------------

struct Call { u32 offset, target; };
struct Clone {
    const char* name;
    u32 base, size;
    const Call* calls;
    int n_calls;
    unsigned ret;   // the answer's width the original defines: 0 none, 1 al, 2 ax, 4 eax
    const void* ours;
};

constexpr Call kPlaceParty[] = {{0x10, 0x531BB0}, {0x113, 0x592C30}, {0x135, 0x592CD0}, {0x185, 0x5922A0}};
constexpr Call kPlaceMember[] = {{0xA2, 0x5B93D2}, {0xD9, 0x592400}, {0xEA, 0x5924E0}};
constexpr Call kMemberClear[] = {{0x7D, 0x592BD0}, {0xB8, 0x592CD0}};
constexpr Call kMemberStands[] = {{0x27, 0x5720C0}, {0x4C, 0x592C30}, {0x69, 0x531CF0}};
constexpr Call kPlaceEnemies[] = {{0xDC, 0x592C30}, {0xFC, 0x592800}};
constexpr Call kReachable[] = {{0x20, 0x531BB0}, {0x50, 0x592CD0}, {0x5F, 0x531BB0}};
constexpr Call kEnemyClear[] = {{0x54, 0x592BD0}};
constexpr Call kOnScreen[] = {{0x2C, 0x5720C0}, {0x3F, 0x5929D0}, {0x8D, 0x5720C0}, {0xA0, 0x5929D0}};
constexpr Call kProject[] = {{0x4B, 0x5A8250}};
constexpr Call kAimCamera[] = {{0x26, 0x5720C0}, {0x4E, 0x531BB0}, {0x74, 0x5720C0}, {0x93, 0x531BB0}, {0xB8, 0x5A8060},
                               {0x127, 0x5A7BF0}, {0x16E, 0x5A8060}, {0x178, 0x5A8DE0}, {0x182, 0x5A8E00}};
constexpr Call kCellFits[] = {{0x29, 0x592890}};
constexpr Call kPathClear[] = {{0x17, 0x592E10}, {0x2B, 0x592E10}, {0x35, 0x592E00},
                               {0x3E, 0x592E00}, {0x65, 0x592E30}, {0xA0, 0x592E30}};
constexpr Call kStepOpen[] = {{0x41, 0x592890}, {0x63, 0x592890}, {0x89, 0x592890},
                              {0x9B, 0x592890}, {0xA4, 0x592890}, {0xBC, 0x592890}};
constexpr Call kRollInit[] = {{0x1B, 0x5B93D2}, {0x55, 0x589810}};
constexpr Call kTurnSenseAll[] = {{0x29, 0x52F570}};
constexpr Call kPartyTurn[] = {{0x76, 0x5891C0}, {0xB6, 0x5891C0}, {0xE5, 0x5891C0}, {0xED, 0x5893A0}, {0x156, 0x534590}};
constexpr Call kToPlaces[] = {{0x7D, 0x534880}, {0x113, 0x572570}, {0x141, 0x534800}, {0x1B7, 0x5893A0}};
constexpr Call kAtPlaces[] = {{0x6F, 0x572570}, {0xBC, 0x5891F0}};
constexpr Call kScriptOnce[] = {{0x5C, 0x589410}};

#define BI_C(name, base, size, calls, ret) \
    {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret, reinterpret_cast<const void*>(&::name)}
#define BI_P(name, base, size, ret) {#name, base, size, nullptr, 0, ret, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    BI_C(Encounter_PlaceParty, 0x5920E0, 0x1B3, kPlaceParty, 1),
    BI_C(Encounter_PlaceMember, 0x5922A0, 0x157, kPlaceMember, 1),
    BI_C(Encounter_MemberClear, 0x592400, 0xD7, kMemberClear, 1),
    BI_C(Encounter_MemberStands, 0x5924E0, 0x82, kMemberStands, 1),
    BI_C(Encounter_PlaceEnemies, 0x592600, 0x153, kPlaceEnemies, 1),
    BI_C(Encounter_EnemiesReachable, 0x592760, 0x97, kReachable, 1),
    BI_C(Encounter_EnemyClear, 0x592800, 0x85, kEnemyClear, 1),
    BI_P(AreaMap_CellNibble, 0x592890, 0x5B, 1),
    BI_C(Encounter_OnScreen, 0x5928F0, 0xDE, kOnScreen, 1),
    BI_C(Encounter_Project, 0x5929D0, 0x54, kProject, 4),
    BI_C(Encounter_AimCamera, 0x592A30, 0x191, kAimCamera, 0),
    BI_P(Encounter_Apart, 0x592BD0, 0x5A, 1),
    BI_C(Encounter_CellFits, 0x592C30, 0x91, kCellFits, 1),
    BI_C(Encounter_PathClear, 0x592CD0, 0x125, kPathClear, 1),
    BI_P(Short_Abs, 0x592E00, 0xC, 4),
    BI_P(Short_Sign, 0x592E10, 0x1C, 2),
    BI_C(Encounter_StepOpen, 0x592E30, 0xCE, kStepOpen, 1),
    BI_P(Sprite_TurnSense, 0x52F570, 0x41, 1),
    BI_C(Encounter_RollInitiative, 0x532550, 0x108, kRollInit, 0),
    BI_C(Encounter_PartyTurnSense, 0x532660, 0x4F, kTurnSenseAll, 0),
    BI_C(Encounter_PartyTurn, 0x5326B0, 0x1A1, kPartyTurn, 1),
    BI_C(Encounter_PartyToPlaces, 0x532860, 0x202, kToPlaces, 1),
    BI_C(Encounter_PartyAtPlaces, 0x532A70, 0xE2, kAtPlaces, 0),
    BI_C(Encounter_PartyScriptOnce, 0x532B60, 0xAE, kScriptOnce, 1),
    BI_P(Sprite_ShadeLower, 0x534880, 0x92, 1),
};
#undef BI_C
#undef BI_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kPlacePartyK, kPlaceMemberK, kMemberClearK, kMemberStandsK, kPlaceEnemiesK, kReachableK, kEnemyClearK, kNibbleK,
    kOnScreenK, kProjectK, kAimCameraK, kApartK, kCellFitsK, kPathClearK, kAbsK, kSignK, kStepOpenK, kTurnSenseK,
    kRollInitK, kTurnSenseAllK, kPartyTurnK, kToPlacesK, kAtPlacesK, kScriptOnceK, kShadeLowerK,
};
static_assert(kShadeLowerK + 1 == kCount, "the index enum follows kClones");

// --- the state both passes start from --------------------------------------

unsigned char g_sxy[8];   // Encounter_Project's destination

struct Region { u32 at, size; };
constexpr Region kRegionList[] = {
    {at::kScratchSprite, 0x4A0},   // the scratch sprite and the placement's state 0x6BE070..0x6BE48F
    {at::kMemberPos, 0x800},       // 256 members' spots (a member index is a byte)
    {at::kHeightAvg, 0x12A0},      // the height sum and 256 enemy slots (a count is a byte)
    {at::kCentreX, 0x10},
    {at::kFormation, 0x10},        // the formation and both party lists
    {0x904AA0, 0x50},              // the battle's facing, the initiative, the battle flags
    {0x802D40, 0x680},             // ObjTrio and the two slots after it
    {0x7E11E0, 0xA00},             // Effect_Objects, 20 of them
    {at::kRecords, 0x5F0},         // the actor records 0..8
    {at::kShadeSource, 0x8C0},     // the shade source and the saved copies, CLUTs 0..31
    {at::kShadeClut, 0x800},
    {0x937F88, 0xC},               // Sprite_Current, Gfx_ClutStripDirty
    {0x905D98, 4},                 // Field_State
    {0x929EC0, 4},                 // Field_MemberCount
    {0x905E60, 8},                 // Field_Kind2Z / X
    {0x903800, 0x44},              // Camera_ShiftX / ShiftY .. Camera_Distance
    {0x92BF18, 4},                 // Draw_OtSlot
    {at::kKinds, 0x8C0},           // the enemy kinds 0..15
    {0x8CB580, 4},                 // AreaMap_Header's width and depth
    {0, sizeof g_sxy},             // g_sxy (its address filled in at start)
};
constexpr unsigned kRegionCount = sizeof kRegionList / sizeof kRegionList[0];
constexpr unsigned RegionBytes() {
    unsigned n = 0;
    for (const Region& r : kRegionList) n += r.size;
    return n;
}
constexpr unsigned kRegionBytes = RegionBytes();
Region g_regions[kRegionCount];

constexpr u32 kMapBytes = 0x8000;   // AreaMap_CellNibble's reach: a 16-bit cell index / 2
unsigned char g_saved_map[kMapBytes];

struct State {
    unsigned char memory[kRegionBytes];
    u32 result;
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

struct Args { u32 a[6]; };

u32 Byte(u32 low) { return Half() ? (low & 0xFF) : Garbage(0xFF, low); }
u32 Word16(u32 low) { return Half() ? (low & 0xFFFF) : Garbage(0xFFFF, low); }

// The invariants the stand-ins keep too - pointers into the regions, at most
// five members, CLUT rows 0..31 - and the likely values of the rest.
void Tidy() {
    Field_MemberCount = static_cast<unsigned char>(Next() % 6);
    Sprite_Current = AnySprite(Next());
    Field_State = Obj(Next() % kObjects);
    At(at::kScratchSprite)[5] = static_cast<unsigned char>(At(at::kScratchSprite)[5] % 32);
    for (unsigned i = 0; i < kObjects; ++i) {
        unsigned char* const o = Obj(i);
        o[5] = static_cast<unsigned char>(o[5] % 32);
        if (Next() % 8) o[0x148] = static_cast<unsigned char>(Next() % 9);
        if (Often()) o[0x89] = static_cast<unsigned char>(Next() % 10);
        if (Often()) o[8] = static_cast<unsigned char>(Next() % 8);
    }
    for (unsigned i = 0; i < 6; ++i)
        if (Next() % 8) At(at::kPartyList + i)[0] = static_cast<unsigned char>(Next() % 12);
    if (Often()) At(at::kBattleFacing)[0] = static_cast<unsigned char>(Next() % 4);
    if (Often()) At(at::kFacing)[0] = static_cast<unsigned char>(Next() % 4);
    if (Often()) At(at::kEnemyCount)[0] = static_cast<unsigned char>(Next() % 9);
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const e = EnemySlot(k);
        if (Often()) e[0] = static_cast<unsigned char>(Next() % 2);
        if (Often()) e[1] = static_cast<unsigned char>(Next() % 16);
    }
    for (unsigned m = 0; m < 4; ++m)
        if (Often()) At(at::kMembers + 4 * m)[1] = static_cast<unsigned char>(Next() % 2);
    for (unsigned k = 0; k < 16; ++k)
        if (Often()) At(at::kKinds + at::kKindStride * k)[0x86] = static_cast<unsigned char>(Next() % 4);
    g_bias = Next() % 5;
    g_hint = Next() % 16;
    g_margin = Next() & 0xFF;
}

// A 16-bit coordinate and a second one near it: equal, one off, a few off, or anywhere.
u32 Near(u32 base) {
    const auto d = static_cast<std::int32_t>(Pick({0, 0, 1, 0xFFFFFFFFu, 2, 0xFFFFFFFEu, 5, 0xFFFFFFFBu, Next() % 64, Next()}));
    return base + static_cast<u32>(d);
}

Args Seed(unsigned k) {
    Args x;
    for (auto& v : x.a) v = Next();
    Tidy();
    switch (k) {
    case kPlacePartyK:
        At(at::kFormation)[0] = static_cast<unsigned char>(Often() ? Next() % 4 : Next());
        for (unsigned i = 0; i < 3; ++i)
            At(at::kPartyList2 + i)[0] = static_cast<unsigned char>(Pick({2, 6, 0, 1, 3, 10, Next() % 24}));
        break;
    case kPlaceMemberK:
        x.a[0] = Byte(Often() ? Next() % 4 : Next());
        x.a[1] = Byte(Often() ? Next() % 3 : Next() % 0x20);
        x.a[2] = Byte(Often() ? Next() % 3 : Next());
        x.a[3] = Byte(Pick({0, 1, 2, 3, 4, 5, 6, 7, 0xFE, 0xFF, Next()}));
        break;
    case kMemberClearK:
    case kMemberStandsK:
        x.a[0] = Byte(Often() ? Next() % 4 : Next());
        for (unsigned m = 0; m < 3; ++m) At(at::kMembers + 4 * m)[0] = static_cast<unsigned char>(Next() % 2);
        break;
    case kPlaceEnemiesK:
        At(at::kPlaced)[0] = static_cast<unsigned char>(Next() % 9);
        At(at::kFacing)[0] = static_cast<unsigned char>(Pick({0, 1, 2, 3, 4, 7, 0xFF, Next()}));
        g_bias = Pick({0, 1, 1, 2, 4});
        break;
    case kEnemyClearK:
        x.a[0] = Byte(Pick({0, 1, 2, 3, 7, 8, 9, Next()}));
        x.a[3] = Byte(Often() ? Next() % 4 : Next());
        break;
    case kNibbleK: {
        for (u32 i = 0; i < kMapBytes; i += 4) SetLong(At(at::kCellMap + i), static_cast<std::int32_t>(Next()));
        const u32 w = Pick({0, 1, 2, 3, 16, 64, 200, 255, Next() & 0xFF});
        const u32 d = Pick({0, 1, 2, 3, 16, 64, 200, 255, Next() & 0xFF});
        AreaMap_Header[0] = static_cast<unsigned char>(w);
        AreaMap_Header[1] = static_cast<unsigned char>(d);
        x.a[0] = Word16(Pick({0, w - 1, w, w + 1, w ? Next() % w : 0, w ? Next() % w : 0, w ? Next() % w : 0, Next()}));
        x.a[1] = Word16(Pick({0, d - 1, d, d + 1, d ? Next() % d : 0, d ? Next() % d : 0, d ? Next() % d : 0, Next()}));
        break;
    }
    case kOnScreenK:
        x.a[2] = Byte(Often() ? Next() % 4 : Next());
        x.a[3] = Byte(g_margin);
        break;
    case kProjectK:
        x.a[2] = (Pick({0xFFFFu, 0xFFFEu, 0xFFFDu, 1, 0, 0x7FFF, 0x8000, 0x8001, Next()}) << 16) | (Next() & 0xFFFF);
        x.a[3] = Address(g_sxy);
        break;
    case kAimCameraK:
        EnemySlot(0)[0] = 1;   // a fight has somebody: the mean divides by the count
        x.a[0] = Byte(Often() ? Next() % 4 : Next());
        break;
    case kApartK: {
        x.a[4] = Byte(Pick({0, 1, 2, 3, 0x7F, 0xFF, Next()}));
        x.a[5] = Byte(Pick({0, 1, 2, 3, 0x7F, 0xFF, Next()}));
        const u32 sum = (((x.a[4] & 0xFF) + 1) << 15) + (((x.a[5] & 0xFF) + 1) << 15);
        if (Half()) x.a[0] = Pick({0x7FFFFFF0u, 0x80000000u, 0xFFFFFFF0u, Next()});
        if (Half()) x.a[1] = Pick({0x7FFFFFF0u, 0x80000000u, 0xFFFFFFF0u, Next()});
        const u32 edge = sum + Pick({0xFFFFFFFFu, 0, 1, Next() % 0x8000});
        x.a[2] = Often() ? (Half() ? x.a[0] + edge : x.a[0] - edge) : x.a[0] + (Next() % 0x40000);
        x.a[3] = Often() ? (Half() ? x.a[1] + edge : x.a[1] - edge) : x.a[1] - (Next() % 0x40000);
        break;
    }
    case kCellFitsK: {
        x.a[2] = Byte(Pick({0, 1, 2, 3, 0x3F, 0x40, 0x41, 0xFF, Next()}));
        g_hint = static_cast<unsigned char>(((x.a[2] & 0xFF) << 2) + 1);
        const u32 s = (x.a[2] & 0xFF) << 15;
        x.a[0] = s + (Next() << 16) + (Half() ? 0 : Next() & 0xFFFF);
        x.a[1] = s + (Next() << 16) + (Half() ? 0 : Next() & 0xFFFF);
        break;
    }
    case kPathClearK: {
        const u32 bx = Next() & 0xFFFF, bz = Next() & 0xFFFF;
        x.a[0] = Word16(bx);
        x.a[1] = Word16(bz);
        const u32 dx = Pick({0, 1, 0xFFFFFFFFu, 5, 0xFFFFFFFBu, 63, 64, 65, 0xFFFFFFC0u, 100, 127, 128, Next() % 16});
        const u32 dz = Pick({0, 1, 0xFFFFFFFFu, 5, 0xFFFFFFFBu, 63, 64, 65, 0xFFFFFFC0u, 0, Next() % 16});
        x.a[2] = Word16(bx + dx);
        x.a[3] = Word16(bz + dz);
        g_bias = Pick({1, 2, 3, 3, 4, 4});
        break;
    }
    case kAbsK:
    case kSignK:
        x.a[0] = Garbage(0xFFFF, Pick({0, 1, 2, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF, Next()}));
        break;
    case kStepOpenK: {
        const u32 a = Pick({0, 0x7FFF, 0x8000, 0xFFFF, Next()}), c = Pick({0, 0x7FFF, 0x8000, 0xFFFF, Next()});
        x.a[0] = Word16(a);
        x.a[1] = Word16(c);
        x.a[2] = Word16(Near(a));
        x.a[3] = Word16(Near(c));
        break;
    }
    case kTurnSenseK: {
        const u32 a = Pick({0, 1, 2, 3, 4, 5, 6, 7, 0xF8, 0xFC, 0xFF, Next()});
        x.a[0] = Byte(a);
        Sprite_Current[8] = static_cast<unsigned char>(
            Often() ? a + Pick({0xFFFFFFFFu, 0, 1, 2, 3, 4, 5, 8, 9, 12, 0xFFFFFFF8u, 0xFFFFFFF7u}) : Next());
        break;
    }
    case kRollInitK:
        for (unsigned i = 0; i < 9; ++i) ActorRecord(i)[0x38] = static_cast<unsigned char>(Pick({0, 50, 51, 99, 100, 101, 0xFF, Next()}));
        break;
    case kPartyTurnK:
        for (unsigned i = 0; i < kObjects; ++i) {
            unsigned char* const o = Obj(i);
            o[0xB] = static_cast<unsigned char>(Pick({0, 0, 1, 0xFF, 7}));
            if (Half()) o[8] = At(at::kFaceTable)[At(at::kBattleFacing)[0]];
        }
        for (unsigned i = 0; i < 9; ++i) SetWord(ActorRecord(i) + 0x10, Half() ? 0x4000 : Next() & ~0x4000u);
        At(at::kBattleFlags)[0] = static_cast<unsigned char>(Half() ? 4 : Next() & ~4u);
        break;
    case kToPlacesK:
        for (unsigned i = 0; i < kObjects; ++i) {
            unsigned char* const o = Obj(i);
            o[0xB] = static_cast<unsigned char>(Pick({0, 1, 2, 3}));
            if (Often()) o[0x89] = At(at::kPartyList2)[Next() % 3];
        }
        At(at::kBattleFlags)[0] = static_cast<unsigned char>(Next() % 4 == 0 ? 4 : Next() & ~4u);
        break;
    case kAtPlacesK:
        for (unsigned i = 0; i < 9; ++i) ActorRecord(i)[0x11] = static_cast<unsigned char>(Half() ? 0x40 : Next() & ~0x40u);
        break;
    case kScriptOnceK:
        for (unsigned i = 0; i < kObjects; ++i) Obj(i)[0xB] = static_cast<unsigned char>(Pick({0, 0, 1, 1, 2}));
        At(at::kBattleFlags)[0] = static_cast<unsigned char>(Next() % 4 == 0 ? 0x10 : Next() & ~0x10u);
        break;
    case kShadeLowerK:
        for (unsigned i = 0x5D; i <= 0x5F; ++i)
            Sprite_Current[i] = static_cast<unsigned char>(Pick({0x80, 0x80, 0x7F, 0x81, 0x88, 0x87, 0x00, 0xFF, 0xC0, Next()}));
        x.a[0] = Byte(Pick({8, 0, 1, 0x7F, 0x80, 0x81, 0xF8, 0xFF, Next()}));
        break;
    default: break;
    }
    return x;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[40];
    unsigned answers[kCount][3];   // answer 0, 1, other
    unsigned shape[kCount][6];     // per function, below
} g_cover;

unsigned Calls(const State& s, u32 what) {
    unsigned n = 0;
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i) n += s.log[i].what == what ? 1u : 0u;
    return n;
}
// Where a region's byte sits in State::memory.
unsigned Offset(u32 address) {
    unsigned off = 0;
    for (const Region& r : g_regions) {
        if (address >= r.at && address < r.at + r.size) return off + (address - r.at);
        off += r.size;
    }
    return 0;
}

// The shapes: CellNibble 0 out of range / even / odd cell; OnScreen the
// projections made (1..5); PathClear steps tried (0, 1..3, 4 and up);
// StepOpen nibbles read (0..3); RollInitiative the flag 0 / 1 / 2;
// PlaceEnemies slots given up (0, 1, 2 and up); PartyToPlaces members put
// at their spots / given their CLUT back; ShadeLower bytes left at 0x80 by it.
void Cover(unsigned k, const Args& x, const State& in, const State& out, u32 result) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 40) ++g_cover.logged[out.log[i].what];
    ++g_cover.answers[k][result == 0 ? 0 : result == 1 ? 1 : 2];
    unsigned* const s = g_cover.shape[k];
    switch (k) {
    case kNibbleK: {
        const unsigned w = in.memory[Offset(0x8CB580)], d = in.memory[Offset(0x8CB581)];
        const unsigned cx = x.a[0] & 0xFFFF, cz = x.a[1] & 0xFFFF;
        ++s[cx >= w || cz >= d ? 0 : ((w * cz + cx) & 1) + 1];
        break;
    }
    case kOnScreenK: { const unsigned n = Calls(out, 26); ++s[n < 6 ? n : 5]; break; }
    case kPathClearK: { const unsigned n = Calls(out, 32); ++s[n == 0 ? 0 : n < 4 ? 1 : 2]; break; }
    case kStepOpenK: { const unsigned n = Calls(out, 25); ++s[n < 6 ? n : 5]; break; }
    case kRollInitK: { const unsigned v = out.memory[Offset(at::kInitiative)]; ++s[v < 3 ? v : 3]; break; }
    case kPlaceEnemiesK: {
        unsigned n = 0;
        for (unsigned e = 0; e < 8; ++e) {
            const unsigned o = Offset(at::kEnemies + at::kEnemyStride * e);
            n += in.memory[o] != 0 && out.memory[o] == 0 ? 1u : 0u;
        }
        ++s[n < 2 ? n : 2];
        break;
    }
    case kToPlacesK:
        s[0] += Calls(out, 13);
        for (unsigned i = 0; i < kObjects; ++i) {
            const unsigned o = Offset(Address(Obj(i)) + 0xB);
            s[1] += in.memory[o] == 1 && out.memory[o] == 2 ? 1u : 0u;
        }
        break;
    case kShadeLowerK: {
        const u32 sc = Long(in.memory + Offset(0x937F88));
        for (unsigned i = 0x5D; i <= 0x5F; ++i) {
            const unsigned o = Offset(sc + i);
            s[0] += in.memory[o] != 0x80 && out.memory[o] == 0x80 ? 1u : 0u;
        }
        break;
    }
    default: break;
    }
}

using Fn = u32 (__cdecl*)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32);

unsigned short ControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    for (unsigned i = 0; i < kRegionCount; ++i) g_regions[i] = kRegionList[i];
    g_regions[kRegionCount - 1].at = Address(g_sxy);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[12];
        if (c.n_calls > 12) bof3::Fatal("inventory_ops: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    // Save what the fuzz overwrites, and stand the recorders in.
    static State saved, input, their_out, our_out;
    Capture(saved);
    std::memcpy(g_saved_map, At(at::kCellMap), kMapBytes);
    const unsigned short cw_saved = ControlWord();
    g = Stubs();

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const u32 v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        g_seed = Next();
        const Args x = Seed(k);
        Capture(input);

        u32 result[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            SetControlWord(0x027F);
            const u32 r = reinterpret_cast<Fn>(const_cast<void*>(fn))(x.a[0], x.a[1], x.a[2], x.a[3], x.a[4], x.a[5], 0x13579BDFu,
                                                                     0x2468ACE0u, 0x0F1E2D3Cu, 0x4B5A6978u, 0x8796A5B4u, 0xC3D2E1F0u,
                                                                     0x01234567u, 0x89ABCDEFu, 0xFEDCBA98u, 0x76543210u);
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
                bof3::Log("shadow      inventory_ops self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    Apply(saved);
    std::memcpy(At(at::kCellMap), g_saved_map, kMapBytes);

    bof3::Log("shadow      inventory_ops self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the placement's state, the members' spots, the enemy slots, the party objects, the effects, "
              "the shade CLUTs, the pointers and counts, the answer and the stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      inventory_ops: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    for (unsigned k = 0; k < kCount; ++k)
        bof3::Log("shadow      inventory_ops coverage: %-27s answers 0 %u, 1 %u, other %u; shape %u %u %u %u %u %u", kClones[k].name,
                  g_cover.answers[k][0], g_cover.answers[k][1], g_cover.answers[k][2], g_cover.shape[k][0], g_cover.shape[k][1],
                  g_cover.shape[k][2], g_cover.shape[k][3], g_cover.shape[k][4], g_cover.shape[k][5]);
    const unsigned* const l = g_cover.logged;
    bof3::Log("shadow      inventory_ops coverage: calls - count %u, elevation %u, object %u, rtp %u, rotmatrix %u, apply %u, "
              "setrot %u, settrans %u, rand %u, ground %u, effect %u, anim-from %u, tick %u, fade %u, raise %u, once %u, anim %u, "
              "place %u, clear %u, stands %u, enemy-clear %u, nibble %u, project %u, apart %u, fits %u, path %u, abs %u, sign %u, "
              "step %u, sense %u, lower %u",
              l[1], l[2], l[3], l[4], l[5], l[6], l[8], l[10], l[12], l[13], l[14], l[15], l[16], l[17], l[18], l[19], l[20], l[21],
              l[22], l[23], l[24], l[25], l[26], l[27], l[28], l[29], l[30], l[31], l[32], l[33], l[34]);
    if (bad) bof3::Fatal("the encounter placement and the intro's party steps differ from the original in %u self-test rounds", bad);
}

}  // namespace inventory_ops

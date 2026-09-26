// The field objects' blocked-ahead test: whether the point one step ahead of
// an object is taken by another object or a party member (Field_ObjectBlocked),
// and for Field_ObjectBlockedAhead also whether the map lets a sprite of its
// size through there. docs/field-blocked.md.
//
// A sprite object here: +8 its direction (0..7, bit 3 "moving"), +0x34 / +0x38
// its position (16.16, x then y), +0x3E its height (s16), +0x70 its size - the
// margin of the object tests, and for the map test how many steps ahead to
// look. The step per direction is Field_DirectionSteps (half a cell per axis).
//
// Every result these return is read as al alone by every caller (E8 scan of
// the exe, docs/field-blocked.md section 2), so ours return a byte; the
// originals leave other bits of eax as their last callee left them.
#include "game/field_blocked.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

constexpr unsigned kObjectSize = 0xA4, kObjects = 30, kExtra = 4;   // Sprite_Objects, Sprite_ObjectsExtra
constexpr unsigned kMemberSize = 0x14C;                             // ObjTrio's records
static_assert(kObjectSize * kObjects == Sprite_Objects_count);
static_assert(kObjectSize * kExtra == Sprite_ObjectsExtra_count);

// The high nibbles of an AreaMap_Bytes cell that stop a step, a bit each. The
// original looks the nibble up in a byte table at 0x518660 (0 or 2: open,
// 1: blocked) and jumps through 0x518654; FieldBlocked_Inject reads the mask
// from both (ReadCellTable, below) before it installs anything, so the list
// is the image's, not a copy of it (docs/exe-table-audit.md).
std::uint16_t ReadCellTable();
std::uint16_t BlockingNibbles() {
    static const std::uint16_t mask = ReadCellTable();   // read once, whoever asks first
    return mask;
}

// Only al of a byte result is meaningful: the originals (and the recorders
// standing in for them in the fuzz) leave the rest of eax undefined.
using ObjectFn = std::uint32_t (__cdecl*)(unsigned char*);
using PointFn = std::uint32_t (__cdecl*)(long, long, unsigned);   // x, y, a direction or a margin
unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }

// Every callee, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. Ours among them are tested alone.
struct Callees {
    ObjectFn map_ahead;                                              // Field_MapBlockedAhead
    PointFn object_at;                                               // Sprite_ObjectAt
    PointFn member_at;                                               // Party_MemberAt
    PointFn wide;                                                    // AreaMap_BlockedWide
    PointFn narrow;                                                  // AreaMap_BlockedNarrow (Capcom's)
    std::uint32_t (__cdecl* cell)(short, short);                     // AreaMap_CellBlocked
    std::uint32_t (__cdecl* steep)(long, long);                      // AreaMap_TooSteep
    std::uint32_t (__cdecl* byte_at)(short, short);                  // AreaMap_ByteAt
    long (__cdecl* slope)(long, long, unsigned long);                // AreaMap_Slope
    long (__cdecl* probe)(long, long);                               // Field_ProbeHeight
    long (__cdecl* elevation)(long, long);                           // AreaMap_Elevation
    // Sprite_PointInReach, with the height as the dword the originals pass
    // (it reads the low 16 bits).
    std::uint32_t (__cdecl* reach)(long, long, long, unsigned, const unsigned char*);
};
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
Callees Originals() {
    Callees c{};
    c.map_ahead = As<ObjectFn>(&Field_MapBlockedAhead);
    c.object_at = As<PointFn>(&Sprite_ObjectAt);
    c.member_at = As<PointFn>(&Party_MemberAt);
    c.wide = As<PointFn>(&AreaMap_BlockedWide);
    c.narrow = As<PointFn>(AreaMap_BlockedNarrow);
    c.cell = As<decltype(c.cell)>(&AreaMap_CellBlocked);
    c.steep = As<decltype(c.steep)>(&AreaMap_TooSteep);
    c.byte_at = As<decltype(c.byte_at)>(&AreaMap_ByteAt);
    c.slope = &AreaMap_Slope;
    c.probe = &Field_ProbeHeight;
    c.elevation = &AreaMap_Elevation;
    c.reach = As<decltype(c.reach)>(&Sprite_PointInReach);
    return c;
}
Callees g = Originals();

// The point `n` steps ahead in direction d & 7: 32-bit wrapping, as the
// originals' add and imul.
std::uint32_t StepX(unsigned d, std::uint32_t n) { return static_cast<std::uint32_t>(Field_DirectionSteps[(d & 7) * 2]) * n; }
std::uint32_t StepY(unsigned d, std::uint32_t n) { return static_cast<std::uint32_t>(Field_DirectionSteps[(d & 7) * 2 + 1]) * n; }
long Ahead(const unsigned char* object, unsigned at, std::uint32_t step) {
    return static_cast<long>(static_cast<std::uint32_t>(Long(object + at)) + step);
}

// Sprite_ObjectAt and Party_MemberAt: the first object of `count` from `first`
// (`size` apart) that is not Sprite_Current (re-read for each), is in use
// (byte +0) and - for the sprite lists - not skipped by +7 bit 0x80, and for
// which Sprite_PointInReach says (x, y, h) is within `margin` of it. The count
// is re-read after each object; `count_at` null means a fixed count.
unsigned char FirstInReach(unsigned char* first, unsigned size, unsigned count, const unsigned char* count_at, bool skip_flag7,
                           long x, long y, long h, unsigned margin, unsigned char base) {
    for (unsigned i = 0; i < (count_at ? *count_at : count); ++i) {
        const unsigned char* const object = first + i * size;
        if (object == Sprite_Current) continue;
        if (object[0] == 0) continue;
        if (skip_flag7 && (object[7] & 0x80)) continue;
        if (Al(g.reach(x, y, h, margin, object))) return static_cast<unsigned char>(base + i);
    }
    return 0xFF;
}

}  // namespace

// original 0x518080 (PSX FUN_801A2AEC): the point one step ahead of the object
// - its position plus the step of ITS direction, taken before any call - is
// blocked by the map (for a sprite of Sprite_Current's size and direction), by
// a sprite object, or by a party member. The size is re-read from
// Sprite_Current for each object test, as the original re-reads it.
extern "C" unsigned char __cdecl Field_ObjectBlockedAhead(unsigned char* object) {
    const unsigned d = object[8] & 7;
    const long x = Ahead(object, 0x34, StepX(d, 1));
    const long y = Ahead(object, 0x38, StepY(d, 1));
    if (Al(g.map_ahead(object))) return 1;
    if (Al(g.object_at(x, y, Sprite_Current[0x70])) != 0xFF) return 1;
    return Al(g.member_at(x, y, Sprite_Current[0x70])) != 0xFF ? 1 : 0;
}

// original 0x519670 (PSX FUN_801A4AD0): the same without the map.
extern "C" unsigned char __cdecl Field_ObjectBlocked(unsigned char* object) {
    const unsigned d = object[8] & 7;
    const long x = Ahead(object, 0x34, StepX(d, 1));
    const long y = Ahead(object, 0x38, StepY(d, 1));
    if (Al(g.object_at(x, y, Sprite_Current[0x70])) != 0xFF) return 1;
    return Al(g.member_at(x, y, Sprite_Current[0x70])) != 0xFF ? 1 : 0;
}

// original 0x518100 (PSX FUN_801A2BC8): the map test, (size + 1) steps ahead -
// the object's position, but Sprite_Current's direction and size. A sized
// sprite goes to AreaMap_BlockedWide, a size-0 one to AreaMap_BlockedNarrow
// (Capcom's, unreached by the attract cycle). The original passes the
// direction as a dword whose low byte alone it stored - the rest is whatever
// its stack slot held - and both callees mask it to a byte; ours passes 0
// above it.
extern "C" unsigned char __cdecl Field_MapBlockedAhead(unsigned char* object) {
    const unsigned d = Sprite_Current[8] & 7;
    const unsigned char size = Sprite_Current[0x70];
    const std::uint32_t n = size + 1u;
    const long x = Ahead(object, 0x34, StepX(d, n));
    const long y = Ahead(object, 0x38, StepY(d, n));
    return Al(size != 0 ? g.wide(x, y, d) : g.narrow(x, y, d));
}

// original 0x518180 (PSX FUN_801A2C64, not 0x801A2EE0 as psx_pair.py paired
// it): for a sized sprite, whether its leading edge at (x, y) can move in
// `direction`. Only the four axes (odd directions) are tested; any other byte
// is blocked. With X, Y the cell (the high words) and fx, fy the fractions:
//   1: row Y;                              cells X, X+1 and, if fx == 0, X-1
//   5: row Y+1 if fy != 0, else row Y;     the same cells
//   7: column X;                           cells Y, Y+1 and, if fy == 0, Y-1
//   3: column X+1 if fx != 0, else X;      the same cells
// then AreaMap_TooSteep at (x, y) and half a cell to either side across the
// direction (x +- 0x8000 for rows, y +- 0x8000 for columns). The results are
// summed as a byte (so 0x80 + 0x80 would be open - the callees return 0 or 1)
// and the sum's non-zero-ness returned. Cell coordinates are 16-bit: X + 1
// wraps as the original's low word does (the dwords it pushes carry the
// neighbouring argument's bytes above them; AreaMap_ByteAt reads 16 bits).
extern "C" unsigned char __cdecl AreaMap_BlockedWide(long x, long y, unsigned direction) {
    const auto ux = static_cast<std::uint32_t>(x), uy = static_cast<std::uint32_t>(y);
    const auto cx = static_cast<short>(ux >> 16), cy = static_cast<short>(uy >> 16);
    const bool fx = (ux & 0xFFFFu) != 0, fy = (uy & 0xFFFFu) != 0;
    const auto plus = [](short v, int by) { return static_cast<short>(static_cast<std::uint16_t>(v + by)); };
    unsigned char sum = 0;
    switch (direction & 0xFF) {
    case 1:
    case 5: {
        const short row = (direction & 0xFF) == 5 && fy ? plus(cy, 1) : cy;
        sum = static_cast<unsigned char>(sum + Al(g.cell(cx, row)));
        sum = static_cast<unsigned char>(sum + Al(g.cell(plus(cx, 1), row)));
        if (!fx) sum = static_cast<unsigned char>(sum + Al(g.cell(plus(cx, -1), row)));
        sum = static_cast<unsigned char>(sum + Al(g.steep(x, y)));
        sum = static_cast<unsigned char>(sum + Al(g.steep(static_cast<long>(ux + 0x8000u), y)));
        sum = static_cast<unsigned char>(sum + Al(g.steep(static_cast<long>(ux - 0x8000u), y)));
        break;
    }
    case 3:
    case 7: {
        const short column = (direction & 0xFF) == 3 && fx ? plus(cx, 1) : cx;
        sum = static_cast<unsigned char>(sum + Al(g.cell(column, cy)));
        sum = static_cast<unsigned char>(sum + Al(g.cell(column, plus(cy, 1))));
        if (!fy) sum = static_cast<unsigned char>(sum + Al(g.cell(column, plus(cy, -1))));
        sum = static_cast<unsigned char>(sum + Al(g.steep(x, y)));
        sum = static_cast<unsigned char>(sum + Al(g.steep(x, static_cast<long>(uy + 0x8000u))));
        sum = static_cast<unsigned char>(sum + Al(g.steep(x, static_cast<long>(uy - 0x8000u))));
        break;
    }
    default:
        return 1;   // 0, the diagonals and anything past 7, by the jump table's bound
    }
    return sum != 0 ? 1 : 0;
}

// original 0x518620 (PSX FUN_801A3100): whether the map cell (x, y) stops a
// step, by the high nibble of its AreaMap_Bytes byte.
extern "C" unsigned char __cdecl AreaMap_CellBlocked(short x, short y) {
    const unsigned nibble = (Al(g.byte_at(x, y)) >> 4) & 0xF;
    return (BlockingNibbles() >> nibble) & 1u;
}

// original 0x518760 (PSX FUN_801A31F4): whether the ground at (x, y) is too
// steep - AreaMap_Slope for Sprite_Current's direction, sloped (scratch byte
// 0, read after the call) and more than 0x40 in its low 16 bits, signed. The
// original's direction dword carries its caller's ecx above the byte;
// AreaMap_Slope reads the byte alone (and, the byte being 0..7, never indexes
// past its four corners).
extern "C" unsigned char __cdecl AreaMap_TooSteep(long x, long y) {
    const unsigned d = Sprite_Current[8] & 7;
    const long slope = g.slope(x, y, d);
    if (Scratch()[0] == 0) return 0;
    return static_cast<short>(slope) > 0x40 ? 1 : 0;
}

// original 0x531DB0 (PSX 0x801BECF0): the height to test a point at - the
// ground's there, unless it is more than 0x100 above or below Sprite_Current's
// own (s16 +0x3E, read after the call), then Sprite_Current's. 16 bits,
// zero-extended as the original's eax is.
extern "C" long __cdecl Field_ProbeHeight(long x, long y) {
    const long ground = g.elevation(x, y);
    const auto own = static_cast<short>(Word(Sprite_Current + 0x3E));
    const auto there = static_cast<short>(ground);
    int distance = own - there;
    if (distance < 0) distance = -distance;
    return static_cast<long>(static_cast<std::uint16_t>(distance > 0x100 ? own : there));
}

// original 0x531CF0 (PSX 0x801BEE0C): the sprite object at (x, y) - within
// `margin` by Sprite_PointInReach at the probe height - as its index in
// Sprite_Objects, or 0x1E + its index in Sprite_ObjectsExtra; 0xFF for none.
// Skips Sprite_Current, objects not in use, and those with +7 bit 0x80. The
// original stores the height over its own y argument slot (the calls take y
// from a register); cdecl gives the caller no claim on that slot and no
// caller reads it back (docs/field-blocked.md section 2), so ours does not.
extern "C" unsigned char __cdecl Sprite_ObjectAt(long x, long y, unsigned margin) {
    const long h = g.probe(x, y);
    const unsigned char found = FirstInReach(Sprite_Objects, kObjectSize, kObjects, nullptr, true, x, y, h, margin, 0);
    if (found != 0xFF) return found;
    return FirstInReach(Sprite_ObjectsExtra, kObjectSize, kExtra, nullptr, true, x, y, h, margin, 0x1E);
}

// original 0x531F10 (PSX 0x801BF0B8): the same over the party's records -
// Field_MemberCount of ObjTrio's, re-read after each - without the +7 test.
extern "C" unsigned char __cdecl Party_MemberAt(long x, long y, unsigned margin) {
    const long h = g.probe(x, y);
    return FirstInReach(ObjTrio, kMemberSize, 0, &Field_MemberCount, false, x, y, h, margin, 0);
}

namespace {

// --- BOF3X_SHADOW=field_blocked: a differential fuzz, once at start-up -------
// Nine byte-copies, every relative call re-aimed at a recorder, the two jump
// tables (0x518398, 0x518654) moved into their copies. One round: one
// function, the object lists, Sprite_Current (a buffer, or one of the listed
// objects or members) and the arguments random with each branch's boundaries
// seeded - fractions 0, 0x8000, 0xFFFF; every direction byte 0..8; sizes 0, 1,
// 0xFF; heights exactly 0x100 and 0x101 apart; objects out of use or flagged,
// Sprite_Current among them, the hit early, late, in the extra four or
// nowhere. Theirs, then ours from the same state; al (all of eax for
// Field_ProbeHeight), the recorders' log, the buffers, the three lists, the
// scratch bytes, Field_MemberCount and Sprite_Current compared.

constexpr unsigned kBuf = 0x100, kLog = 40;
unsigned char g_object[kBuf], g_sprite[kBuf];
unsigned char* g_current_object;
const unsigned char* g_hit;
struct Entry { std::uint32_t what, a, b, c, d, e; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash(std::uint32_t salt = 0) {
    std::uint32_t h = (g_seed + g_log_n * 0x61C88647u + salt) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0, std::uint32_t e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}

// Where Sprite_Current may point: the two buffers, any listed object, any member.
constexpr unsigned kCandidates = 2 + kObjects + kExtra + 3;
unsigned char* Candidate(unsigned i) {
    i %= kCandidates;
    if (i == 0) return g_sprite;
    if (i == 1) return g_object;
    i -= 2;
    if (i < kObjects) return Sprite_Objects + i * kObjectSize;
    i -= kObjects;
    if (i < kExtra) return Sprite_ObjectsExtra + i * kObjectSize;
    return ObjTrio + (i - kExtra) * kMemberSize;
}
std::uint32_t Id(const void* p) {
    for (unsigned i = 0; i < kCandidates; ++i)
        if (p == Candidate(i)) return 0x100 + i;
    return Address(p);
}

// A callee may change what the caller reads after it: Sprite_Current and its
// size, direction and height, the object's position and direction, the
// member count, an object's in-use byte or +7 flag.
void Disturb() {
    const std::uint32_t h = Hash(1);
    if (h % 5 == 0) Sprite_Current[0x70] = static_cast<unsigned char>(h >> 8);
    if (h % 7 == 0) Sprite_Current[8] = static_cast<unsigned char>(h >> 12);
    if (h % 11 == 0) SetWord(Sprite_Current + 0x3E, h >> 9);
    if (h % 13 == 0) SetLong(g_current_object + 0x34 + (h >> 20) % 2 * 4, static_cast<std::int32_t>(h * 0x2545F491u));
    if (h % 17 == 0) g_current_object[8] = static_cast<unsigned char>(h >> 16);
    if (h % 19 == 0) Sprite_Current = Candidate(h >> 5);
    if (h % 23 == 0) Field_MemberCount = static_cast<unsigned char>((h >> 7) % 5);
    if (h % 29 == 0) {
        unsigned char* const o = Candidate(2 + (h >> 11) % (kCandidates - 2));
        if ((h >> 3) % 2) o[0] = o[0] ? 0 : 1;
        else o[7] ^= 0x80;
    }
}

// A byte result: 0 most often (six zeros in a row is how the wide test says
// open), then 1, then the byte-sum and sign edges; noise above it. One round
// in eight every non-zero byte is 0x80, so that two of them sum to 0.
std::uint32_t ByteResult() {
    const std::uint32_t h = Hash(2);
    static const unsigned char kEdges[] = {0x80, 0xFF, 0x7F, 2};
    unsigned char v = h % 16 < 13 ? 0 : h % 16 == 13 ? 1 : h % 16 == 14 ? kEdges[(h >> 8) % 4] : static_cast<unsigned char>(h >> 24);
    if (v != 0 && (g_seed >> 3) % 8 == 0) v = 0x80;
    return (Hash(3) & 0xFFFFFF00u) | v;
}
std::uint32_t IndexResult() {   // an object test's answer: none most of the time
    const std::uint32_t h = Hash(4);
    static const unsigned char kValues[] = {0, 0x1D, 0x1E, 0x21, 0xFE, 0x7F};
    const unsigned char v = h % 16 < 11 ? 0xFF : h % 16 < 15 ? kValues[(h >> 8) % 6] : static_cast<unsigned char>(h >> 24);
    return (Hash(5) & 0xFFFFFF00u) | v;
}

std::uint32_t __cdecl StubMapAhead(unsigned char* o) { Record(1, Id(o)); Disturb(); return ByteResult(); }
std::uint32_t __cdecl StubObjectAt(long x, long y, unsigned m) { Record(2, x, y, m); Disturb(); return IndexResult(); }
std::uint32_t __cdecl StubMemberAt(long x, long y, unsigned m) { Record(3, x, y, m); Disturb(); return IndexResult(); }
std::uint32_t __cdecl StubWide(long x, long y, unsigned d) { Record(4, x, y, d & 0xFF); Disturb(); return ByteResult(); }
std::uint32_t __cdecl StubNarrow(long x, long y, unsigned d) { Record(5, x, y, d & 0xFF); Disturb(); return ByteResult(); }
std::uint32_t __cdecl StubCell(short x, short y) { Record(6, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)); Disturb(); return ByteResult(); }
std::uint32_t __cdecl StubSteep(long x, long y) { Record(7, x, y); Disturb(); return ByteResult(); }
std::uint32_t __cdecl StubByteAt(short x, short y) { Record(8, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)); return Hash(6); }
long __cdecl StubSlope(long x, long y, unsigned long d) {
    Record(9, x, y, d & 0xFF);
    Disturb();
    const std::uint32_t h = Hash(7);
    static const unsigned char kFlag[] = {0, 1, 1, 0x80};
    Scratch()[0] = h % 8 < 4 ? kFlag[h % 4] : static_cast<unsigned char>(h >> 24);
    static const std::uint16_t kLow[] = {0x3F, 0x40, 0x41, 0, 0x7FFF, 0x8000, 0xFFFF, 0x8040, 0x140};
    const std::uint32_t low = Hash(8) % 4 ? kLow[Hash(9) % 9] : Hash(9) & 0xFFFF;
    return static_cast<long>((Hash(10) & 0xFFFF0000u) | low);
}
long __cdecl StubProbe(long x, long y) { Record(10, x, y); Disturb(); return static_cast<long>(Hash(11)); }
long __cdecl StubElevation(long x, long y) {
    Record(11, x, y);
    Disturb();
    // Around Sprite_Current's height as it is now (after the disturbance).
    static const int kDelta[] = {0, 0xFF, 0x100, 0x101, -0xFF, -0x100, -0x101, 0x8000, 0x7FFF, 1};
    const std::uint32_t h = Hash(12);
    const int delta = h % 8 ? kDelta[h % 10] : static_cast<int>(h >> 16);
    const std::uint32_t low = static_cast<std::uint16_t>(Word(Sprite_Current + 0x3E) + delta);
    return static_cast<long>((Hash(13) & 0xFFFF0000u) | low);
}
std::uint32_t __cdecl StubReach(long x, long y, long h, unsigned margin, const unsigned char* o) {
    Record(12, Id(o), x, y, h, margin);
    Disturb();
    const bool yes = o == g_hit || Hash(14) % 61 == 0;
    return (Hash(15) & 0xFFFFFF00u) | (yes ? (Hash(16) >> 8 & 0xFF) | 1 : 0);
}

Callees Stubs() {
    Callees s{};
    s.map_ahead = StubMapAhead;
    s.object_at = As<PointFn>(&StubObjectAt);
    s.member_at = As<PointFn>(&StubMemberAt);
    s.wide = As<PointFn>(&StubWide);
    s.narrow = As<PointFn>(&StubNarrow);
    s.cell = StubCell;
    s.steep = StubSteep;
    s.byte_at = StubByteAt;
    s.slope = StubSlope;
    s.probe = StubProbe;
    s.elevation = StubElevation;
    s.reach = StubReach;
    return s;
}
const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x518100: return f(&StubMapAhead);
    case 0x531CF0: return f(&StubObjectAt);
    case 0x531F10: return f(&StubMemberAt);
    case 0x518180: return f(&StubWide);
    case 0x5183C0: return f(&StubNarrow);
    case 0x518620: return f(&StubCell);
    case 0x518760: return f(&StubSteep);
    case 0x536700: return f(&StubByteAt);
    case 0x5722D0: return f(&StubSlope);
    case 0x531DB0: return f(&StubProbe);
    case 0x5720C0: return f(&StubElevation);
    case 0x531C70: return f(&StubReach);
    default: bof3::Fatal("field_blocked: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The copies and their calls out, by capstone 2026-09-22 (every jump stays
// inside; the two `jmp [reg*4 + table]` are relocated below).
struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    Call calls[22];
    int n_calls;
};
const Clone kClones[] = {
    {"Field_ObjectBlockedAhead", 0x518080, 0x7A, {{0x26, 0x518100}, {0x48, 0x531CF0}, {0x6a, 0x531F10}}, 3},
    {"Field_ObjectBlocked", 0x519670, 0x68, {{0x36, 0x531CF0}, {0x58, 0x531F10}}, 2},
    {"Field_MapBlockedAhead", 0x518100, 0x7D, {{0x5d, 0x518180}, {0x6d, 0x5183C0}}, 2},
    // Code 0x218 bytes, then the 7-entry table at +0x218.
    {"AreaMap_BlockedWide", 0x518180, 0x234,
     {{0x5e, 0x518620}, {0x6c, 0x518620}, {0x8e, 0x518620}, {0x9e, 0x518620}, {0xb4, 0x518620}, {0xc0, 0x518760},
      {0xcd, 0x518760}, {0xf0, 0x518620}, {0xfe, 0x518620}, {0x119, 0x518620}, {0x127, 0x518620}, {0x13d, 0x518620},
      {0x149, 0x518760}, {0x156, 0x518760}, {0x17a, 0x518620}, {0x188, 0x518620}, {0x1aa, 0x518620}, {0x1ba, 0x518620},
      {0x1d0, 0x518620}, {0x1dc, 0x518760}, {0x1e9, 0x518760}, {0x1f6, 0x518760}}, 22},
    // Code 0x33 bytes, then the 3-entry table at +0x34; the byte table after
    // it (0x518660) is read in place, where it stays.
    {"AreaMap_CellBlocked", 0x518620, 0x40, {{0xa, 0x536700}}, 1},
    {"AreaMap_TooSteep", 0x518760, 0x3F, {{0x1f, 0x5722D0}}, 1},
    {"Sprite_ObjectAt", 0x531CF0, 0xB8, {{0xe, 0x531DB0}, {0x41, 0x531C70}, {0x87, 0x531C70}}, 3},
    {"Field_ProbeHeight", 0x531DB0, 0x3B, {{0xb, 0x5720C0}}, 1},
    {"Party_MemberAt", 0x531F10, 0x71, {{0xe, 0x531DB0}, {0x44, 0x531C70}}, 2},
};
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];
enum : unsigned { kAhead, kBlocked, kMapAhead, kWide, kCell, kSteep, kObjectAt, kProbe, kMemberAt };
static_assert(kMemberAt + 1 == kCount, "one enumerator per clone");

const void* Ours(unsigned k) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (k) {
    case kAhead: return f(&Field_ObjectBlockedAhead);
    case kBlocked: return f(&Field_ObjectBlocked);
    case kMapAhead: return f(&Field_MapBlockedAhead);
    case kWide: return f(&AreaMap_BlockedWide);
    case kCell: return f(&AreaMap_CellBlocked);
    case kSteep: return f(&AreaMap_TooSteep);
    case kObjectAt: return f(&Sprite_ObjectAt);
    case kProbe: return f(&Field_ProbeHeight);
    default: return f(&Party_MemberAt);
    }
}

struct State {
    unsigned char objects[Sprite_Objects_count], extra[Sprite_ObjectsExtra_count], members[kMemberSize * 3];
    unsigned char object[kBuf], sprite[kBuf];
    unsigned char scratch[4];
    unsigned char count;
    unsigned char* current;
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s, std::uint32_t result) {
    std::memcpy(s.objects, Sprite_Objects, sizeof s.objects);
    std::memcpy(s.extra, Sprite_ObjectsExtra, sizeof s.extra);
    std::memcpy(s.members, ObjTrio, sizeof s.members);
    std::memcpy(s.object, g_object, kBuf);
    std::memcpy(s.sprite, g_sprite, kBuf);
    std::memcpy(s.scratch, Scratch(), 4);
    s.count = Field_MemberCount;
    s.current = Sprite_Current;
    s.result = result;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    std::memcpy(Sprite_Objects, s.objects, sizeof s.objects);
    std::memcpy(Sprite_ObjectsExtra, s.extra, sizeof s.extra);
    std::memcpy(ObjTrio, s.members, sizeof s.members);
    std::memcpy(g_object, s.object, kBuf);
    std::memcpy(g_sprite, s.sprite, kBuf);
    std::memcpy(Scratch(), s.scratch, 4);
    Field_MemberCount = s.count;
    Sprite_Current = s.current;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x518080A5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }
template <typename T, unsigned N> T Pick(const T (&values)[N]) { return values[Next() % N]; }

// A 16.16 coordinate: the fraction 0 (on a cell boundary) half the time, else
// 0x8000, 0xFFFF, 1 or anything; the cell small or anything.
std::uint32_t Coordinate() {
    static const std::uint16_t kFraction[] = {0, 0, 0, 0, 0x8000, 0xFFFF, 1, 0x7FFF};
    const std::uint32_t whole = Next() % 4 ? Next() % 64 : Next() >> 16;
    const std::uint32_t fraction = Next() % 4 ? Pick(kFraction) : Next() & 0xFFFF;
    return whole << 16 | fraction;
}

struct Args { unsigned char* object; std::uint32_t a, b, c; };

// Sets the round's input: random bytes everywhere, then the boundaries.
Args Seed(unsigned k, State& input) {
    auto* bytes = reinterpret_cast<unsigned char*>(&input);
    for (unsigned i = 0; i < offsetof(State, current); ++i) bytes[i] = static_cast<unsigned char>(Next());
    input.current = Candidate(Next() % 4 ? Next() % 2 : Next());   // a buffer mostly, a listed object sometimes
    g_seed = Next();
    Apply(input);

    // The lists: in use or not, flagged or not; the hit early, late, extra,
    // a member, or nowhere.
    const unsigned out_of_use = 1 + Next() % 6, flagged = 1 + Next() % 6;
    for (unsigned i = 2; i < kCandidates; ++i) {
        unsigned char* const o = Candidate(i);
        if (Next() % out_of_use == 0) o[0] = 0;
        else if (Often()) o[0] = 1;
        if (Next() % flagged == 0) o[7] |= 0x80;
        else o[7] &= 0x7F;
    }
    static const unsigned char kCounts[] = {0, 1, 2, 3, 3, 3, 4};
    Field_MemberCount = Pick(kCounts);
    const unsigned hit = Next() % (kCandidates + 4);
    g_hit = hit < kCandidates ? Candidate(hit) : nullptr;
    if (k == kMemberAt && Next() % 2) g_hit = Candidate(2 + kObjects + kExtra + Next() % 3);   // a member, for the member test

    // Sprite_Current's size, direction, height.
    unsigned char* const s = Sprite_Current;
    static const unsigned char kSizes[] = {0, 0, 1, 2, 0xFF, 0x80};
    if (Next() % 4) s[0x70] = Pick(kSizes);
    if (Often()) s[8] &= 7;
    static const std::uint16_t kHeights[] = {0, 0x7FFF, 0x8000, 0xFFFF, 0x100, 0xFF00};
    if (Often()) SetWord(s + 0x3E, Pick(kHeights));

    // The object: the loops' Sprite_Current itself half the time.
    unsigned char* const object = Next() % 2 ? Sprite_Current : g_object;
    g_current_object = object;
    SetLong(object + 0x34, static_cast<std::int32_t>(Coordinate()));
    SetLong(object + 0x38, static_cast<std::int32_t>(Coordinate()));
    if (Often()) object[8] &= 7;

    Args args{object, Coordinate(), Coordinate(), 0};
    if (Next() % 8 == 0) args.a = Next();
    if (Next() % 8 == 0) args.b = Next();
    switch (k) {
    case kWide:   // every direction byte, noise above it
        static const unsigned char kAxes[] = {1, 3, 5, 7};
        args.c = Next() % 2 ? Pick(kAxes) : Next() % 2 ? Next() % 9 : Next() % 2 ? 0xFF : Next() & 0xFF;
        if (Next() % 2) args.c |= Next() & 0xFFFFFF00u;
        break;
    case kObjectAt:
    case kMemberAt:   // a byte margin mostly, as the callers pass it
        args.c = Next() % 4 ? Next() & 0xFF : Next();
        break;
    default:
        break;
    }
    return args;
}

using ObjectCall = std::uint32_t (__cdecl*)(unsigned char*);
using ThreeCall = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 45000;
    static State saved, input, their_out, our_out;
    Capture(saved, 0);
    unsigned char* const saved_object = g_current_object;
    g = Stubs();

    unsigned bad = 0, calls = 0, per[kCount] = {}, blocked[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        const Args args = Seed(k, input);
        Capture(input, 0);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g_current_object = args.object;
            void* const fn = pass ? const_cast<void*>(Ours(k)) : theirs[k];
            std::uint32_t result;
            if (k == kAhead || k == kBlocked || k == kMapAhead) result = reinterpret_cast<ObjectCall>(fn)(args.object);
            else result = reinterpret_cast<ThreeCall>(fn)(args.a, args.b, args.c);   // extra arguments are harmless in cdecl
            if (k != kProbe) result &= 0xFF;
            Capture(pass ? our_out : their_out, result);
        }
        calls += their_out.log_n;
        if (k == kObjectAt || k == kMemberAt ? their_out.result != 0xFF : k != kProbe && their_out.result != 0) ++blocked[k];
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      field_blocked self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X", round, kClones[k].name,
                      their_out.log_n, our_out.log_n, (unsigned)their_out.result, (unsigned)our_out.result);
    }
    g = Originals();
    Apply(saved);
    g_current_object = saved_object;
    bof3::Log("shadow      field_blocked self-test: %u rounds (%u per function, %u functions), %u calls to the stand-ins, %u MISMATCHES; "
              "the object, Sprite_Current's buffer, the three lists, scratch, Field_MemberCount, Sprite_Current, the result and "
              "the stand-ins' log compared", kRounds, per[0], kCount, calls, bad);
    bof3::Log("shadow      field_blocked self-test: blocked / found per function: ahead %u, blocked %u, map %u, wide %u, cell %u, "
              "steep %u, object %u, member %u", blocked[kAhead], blocked[kBlocked], blocked[kMapAhead], blocked[kWide], blocked[kCell],
              blocked[kSteep], blocked[kObjectAt], blocked[kMemberAt]);
    if (bad) bof3::Fatal("the blocked-ahead test differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// The original's cell table: for each high nibble, the byte at 0x518660 +
// nibble * 0x10 picks an entry of 0x518654, which is the `mov al, 1` at
// 0x51864D or the `xor al, al` at 0x518650. Anything else is a Fatal.
std::uint16_t ReadCellTable() {
    std::uint16_t mask = 0;
    for (unsigned nibble = 0; nibble < 16; ++nibble) {
        const unsigned char index = At(0x518660 + nibble * 0x10)[0];
        if (index > 2) bof3::Fatal("field_blocked: cell table entry %u is %u", nibble, index);
        std::uint32_t target;
        std::memcpy(&target, At(0x518654 + index * 4), sizeof target);
        if (target == 0x51864D) mask = static_cast<std::uint16_t>(mask | 1u << nibble);
        else if (target != 0x518650) bof3::Fatal("field_blocked: cell jump table entry %u is 0x%X", index, (unsigned)target);
    }
    return mask;
}

}  // namespace

void FieldBlocked_Inject() {
    BlockingNibbles();   // a table that is not the expected shape is a Fatal now, not mid-game
    if (bof3::WantsShadow("field_blocked")) {
        void* clones[kCount];
        for (unsigned k = 0; k < kCount; ++k) {
            const Clone& c = kClones[k];
            bof3::CloneCall calls[22];
            for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
            clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        }
        Relocate(clones[kWide], 0x518180, 0x234, {0x50, 0x218, 7});
        Relocate(clones[kCell], 0x518620, 0x40, {0x29, 0x34, 3});
        SelfTest(clones);
    }
    BOF3_INJECT(Field_ObjectBlockedAhead);
    BOF3_INJECT(Field_ObjectBlocked);
    BOF3_INJECT(Field_MapBlockedAhead);
    BOF3_INJECT(AreaMap_BlockedWide);
    BOF3_INJECT(AreaMap_CellBlocked);
    BOF3_INJECT(AreaMap_TooSteep);
    BOF3_INJECT(Sprite_ObjectAt);
    BOF3_INJECT(Field_ProbeHeight);
    BOF3_INJECT(Party_MemberAt);
}

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
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/code_bytes.h"

namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return Low32(p); }

constexpr unsigned kObjectSize = 0xA4, kObjects = 30, kExtra = 4;   // Sprite_Objects, Sprite_ObjectsExtra
constexpr unsigned kMemberSize = 0x14C;                             // ObjTrio's records
static_assert(kObjectSize * kObjects == (Sprite_Objects_count * sizeof(records::SpriteObject)));
static_assert(kObjectSize * kExtra == (Sprite_ObjectsExtra_count * sizeof(records::SpriteObject)));

// The high nibbles of an AreaMap_Bytes cell that stop a step, a bit each. The
// original looks the nibble up in a byte table at 0x518660 (0 or 2: open,
// 1: blocked) and jumps through 0x518654: the switch it was compiled from,
// the function's logic, ours since AreaMap_CellBlocked is (the disc-only
// round's rule for a table the compiler put in the code, docs/operand-store.md;
// it was read from the image until then, docs/exe-table-audit.md). Hosted,
// FieldBlocked_Inject registers CellTableIsOurs, which reads both tables and
// compares, for Operands_Check.
bool NibbleBlocks(unsigned nibble) {
    switch (nibble) {
    case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0xA: case 0xB: case 0xF: return true;
    default: return false;
    }
}

template <typename T, typename F> T AsFieldBlocked(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
field_blocked::CalleesFieldBlocked Originals() {
    field_blocked::CalleesFieldBlocked c{};
    c.map_ahead = AsFieldBlocked<field_blocked::ObjectFn>(&Field_MapBlockedAhead);
    c.object_at = AsFieldBlocked<field_blocked::PointFn>(&Sprite_ObjectAt);
    c.member_at = AsFieldBlocked<field_blocked::PointFn>(&Party_MemberAt);
    c.wide = AsFieldBlocked<field_blocked::PointFn>(&AreaMap_BlockedWide);
    c.narrow = AsFieldBlocked<field_blocked::PointFn>(AreaMap_BlockedNarrow);
    c.cell = AsFieldBlocked<decltype(c.cell)>(&AreaMap_CellBlocked);
    c.steep = AsFieldBlocked<decltype(c.steep)>(&AreaMap_TooSteep);
    c.byte_at = AsFieldBlocked<decltype(c.byte_at)>(&AreaMap_ByteAt);
    c.slope = &AreaMap_Slope;
    c.probe = &Field_ProbeHeight;
    c.elevation = &AreaMap_Elevation;
    c.reach = AsFieldBlocked<decltype(c.reach)>(&Sprite_PointInReach);
    return c;
}
}  namespace field_blocked { CalleesFieldBlocked gFieldBlocked = Originals(); }
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
    if (i < kObjects) return rv::Bytes(Sprite_Objects) + i * kObjectSize;
    i -= kObjects;
    if (i < kExtra) return rv::Bytes(Sprite_ObjectsExtra) + i * kObjectSize;
    return rv::Bytes(ObjTrio) + (i - kExtra) * kMemberSize;
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

field_blocked::CalleesFieldBlocked Stubs() {
    field_blocked::CalleesFieldBlocked s{};
    s.map_ahead = StubMapAhead;
    s.object_at = AsFieldBlocked<field_blocked::PointFn>(&StubObjectAt);
    s.member_at = AsFieldBlocked<field_blocked::PointFn>(&StubMemberAt);
    s.wide = AsFieldBlocked<field_blocked::PointFn>(&StubWide);
    s.narrow = AsFieldBlocked<field_blocked::PointFn>(&StubNarrow);
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
    unsigned char objects[(Sprite_Objects_count * sizeof(records::SpriteObject))], extra[(Sprite_ObjectsExtra_count * sizeof(records::SpriteObject))], members[kMemberSize * 3];
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
    field_blocked::gFieldBlocked = Stubs();

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
    field_blocked::gFieldBlocked = Originals();
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

// The hosted check of NibbleBlocks against the original's cell table: for
// each high nibble, the byte at 0x518660 + nibble * 0x10 picks an entry of
// 0x518654, which is the `mov al, 1` at 0x51864D or the `xor al, al` at
// 0x518650. Run hosted only (Operands_Check), where the code is mapped.
bool CellTableIsOurs() {
    const auto* const bytes = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(0x518660));
    const auto* const jumps = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(0x518654));
    for (unsigned nibble = 0; nibble < 16; ++nibble) {
        const unsigned char index = bytes[nibble * 0x10];
        if (index > 2) return false;
        std::uint32_t target;
        std::memcpy(&target, jumps + index * 4, sizeof target);
        if (target != (NibbleBlocks(nibble) ? 0x51864Du : 0x518650u)) return false;
    }
    return true;
}

}  // namespace

void FieldBlocked_Inject() {
    bof3::OperandRule("AreaMap_CellBlocked cell table", 0x518654, 0x10C, &CellTableIsOurs);
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
}

#include "game/sprite_find.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr unsigned kObjectSize = 0xA4;
constexpr unsigned kObjects = 30, kExtra = 4;
static_assert(kObjectSize * kObjects == Sprite_Objects_count);
static_assert(kObjectSize * kExtra == Sprite_ObjectsExtra_count);

// The fields of a sprite object this file reads; the rest is unread.
constexpr unsigned kFlags = 0x00;     // u8, bit 0: in use
constexpr unsigned kType = 0x06;      // u8
constexpr unsigned kHandle = 0x18;    // u8: the object this one is attached to (movement script F8 07)
constexpr unsigned kFlags24 = 0x24;   // u8, bit 0x40; bit 0x20: attached
constexpr unsigned kDrawKey = 0x32;   // u16: layer, then order within it (sprite-draw-order.md section 1)
constexpr unsigned kX = 0x34;         // s32
constexpr unsigned kY = 0x38;         // s32
constexpr unsigned kZ = 0x3E;         // s16
constexpr unsigned kRecord = 0x54;    // const u8*: +2 s8 reach, +3 bit 0x10 "not this one"

template <class T> T Get(const unsigned char* at, unsigned offset) {
    T v;
    std::memcpy(&v, at + offset, sizeof v);
    return v;
}
template <class T> void Put(unsigned char* at, unsigned offset, T v) { std::memcpy(at + offset, &v, sizeof v); }

// |a - b| as the original computes it: a 32-bit difference that may wrap, and
// an absolute value that leaves 0x80000000 negative.
std::int32_t WrappingDistance(std::int32_t a, std::int32_t b) {
    const std::uint32_t d = static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b);
    const std::uint32_t sign = d & 0x80000000u ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ sign) - sign);
}

std::int32_t ReachOf(const unsigned char* record) {
    const std::int32_t reach8 = static_cast<signed char>(record[2]);
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(reach8) << 15);
}

bool Qualifies(const unsigned char* object, const unsigned char* current) {
    if (!(object[kFlags] & 1)) return false;
    if (!(object[kFlags24] & 0x40)) return false;
    if (object[kType] != 0x0A) return false;
    if (object == current) return false;
    const auto* record = Get<const unsigned char*>(object, kRecord);
    if (record[3] & 0x10) return false;
    const std::int32_t reach = ReachOf(record);
    if (WrappingDistance(Get<std::int32_t>(current, kX), Get<std::int32_t>(object, kX)) > reach) return false;
    if (WrappingDistance(Get<std::int32_t>(current, kY), Get<std::int32_t>(object, kY)) > reach) return false;
    const int dz = Get<std::int16_t>(current, kZ) - Get<std::int16_t>(object, kZ);
    return (dz < 0 ? -dz : dz) <= 0x300;
}

// --- BOF3X_SHADOW=sprite_find: a differential fuzz, once at start-up -----------
// No calls, every jump internal: a byte-copy runs in place against the same
// arrays. The fuzz owns both arrays and Sprite_Current for its duration and
// puts them back. Objects are random but leaned towards qualifying, a field at
// a time, so that every test is the deciding one in some rounds; positions
// sit near the current object, a few at the wrap (0x80000000 apart) and at
// exactly the reach and one past it; the current object is in an array, or
// outside both.

using FindFn = unsigned char (__cdecl*)();

std::uint32_t g_rng = 0x165667B1u;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

unsigned char g_records[8][4];

std::int32_t Near(std::int32_t centre, std::int32_t reach) {
    const std::uint32_t c = static_cast<std::uint32_t>(centre), r = static_cast<std::uint32_t>(reach);
    switch (Rng() % 8) {
        case 0: return static_cast<std::int32_t>(c + 0x80000000u);
        case 1: return static_cast<std::int32_t>(c + r);
        case 2: return static_cast<std::int32_t>(c - r - 1u);
        case 3: return static_cast<std::int32_t>(Rng());
        default: return static_cast<std::int32_t>(c + Rng() % 0x30000u - 0x18000u);
    }
}

void Fill(unsigned char* object, unsigned likely, std::int32_t cx, std::int32_t cy, std::int16_t cz) {
    for (unsigned i = 0; i < kObjectSize; ++i) object[i] = static_cast<unsigned char>(Rng());
    if (Rng() % likely) object[kFlags] |= 1;
    if (Rng() % likely) object[kFlags24] |= 0x40;
    if (Rng() % likely) object[kType] = 0x0A;
    const unsigned char* record = g_records[Rng() % 8];
    Put<const unsigned char*>(object, kRecord, record);
    Put<std::int32_t>(object, kX, Near(cx, ReachOf(record)));
    Put<std::int32_t>(object, kY, Near(cy, ReachOf(record)));
    static const int kDz[] = {0, 0x300, 0x301, -0x300, -0x301, 0x100, -0x100, 0x7000};
    Put<std::int16_t>(object, kZ, static_cast<std::int16_t>(cz + kDz[Rng() % 8]));
}

void SelfTest(FindFn theirs) {
    constexpr unsigned kRounds = 8000;
    static unsigned char saved_a[Sprite_Objects_count], saved_b[Sprite_ObjectsExtra_count];
    static unsigned char outside[kObjectSize];
    std::memcpy(saved_a, Sprite_Objects, sizeof saved_a);
    std::memcpy(saved_b, Sprite_ObjectsExtra, sizeof saved_b);
    unsigned char* const saved_current = Sprite_Current;

    unsigned bad = 0, none = 0, first = 0, extra = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& r : g_records) {
            r[2] = static_cast<unsigned char>(Rng() % 6 == 0 ? Rng() : Rng() % 8);   // sometimes negative
            r[3] = static_cast<unsigned char>(Rng() % 5 == 0 ? Rng() | 0x10u : Rng() & ~0x10u);
        }
        const std::int32_t cx = static_cast<std::int32_t>(Rng()), cy = static_cast<std::int32_t>(Rng());
        const std::int16_t cz = static_cast<std::int16_t>(Rng());
        const unsigned likely = 1 + Rng() % 12;   // 1: nothing leans; 12: nearly everything does
        for (unsigned i = 0; i < kObjects; ++i) Fill(Sprite_Objects + i * kObjectSize, likely, cx, cy, cz);
        for (unsigned i = 0; i < kExtra; ++i) Fill(Sprite_ObjectsExtra + i * kObjectSize, likely, cx, cy, cz);

        const unsigned where = Rng() % (kObjects + kExtra + 4);
        unsigned char* const current = where < kObjects ? Sprite_Objects + where * kObjectSize
                                     : where < kObjects + kExtra ? Sprite_ObjectsExtra + (where - kObjects) * kObjectSize
                                     : outside;
        Put<std::int32_t>(current, kX, cx);
        Put<std::int32_t>(current, kY, cy);
        Put<std::int16_t>(current, kZ, cz);
        Sprite_Current = current;

        const unsigned char their_result = theirs();
        const unsigned char our_result = Sprite_FindNearby();
        if (our_result == 0xFF) ++none;
        else if (our_result < kObjects) ++first;
        else ++extra;
        if (their_result != our_result && ++bad <= 8)
            bof3::Log("shadow      Sprite_FindNearby self-test MISMATCH round %u: %u vs ours %u",
                      round, their_result, our_result);
    }
    std::memcpy(Sprite_Objects, saved_a, sizeof saved_a);
    std::memcpy(Sprite_ObjectsExtra, saved_b, sizeof saved_b);
    Sprite_Current = saved_current;
    bof3::Log("shadow      Sprite_FindNearby self-test: %u rounds (%u found in the 30, %u in the extra 4, %u none), "
              "%u MISMATCHES", kRounds, first, extra, none, bad);
    if (bad) bof3::Fatal("Sprite_FindNearby differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// The same for Sprite_PointInReach, on a fake object. The point is aimed at
// the object's centre plus an offset chosen against the reach: well inside,
// exactly at it, one short of it, one past it, 0x80000000 away, or anywhere;
// the same for z against reach << 7. Margins small, negative (so that the
// reach goes to zero and below), or anything. z carries noise above bit 15.
using PointFn = unsigned char (__cdecl*)(int, int, unsigned, int, const unsigned char*);

void SelfTestPointInReach(PointFn theirs) {
    constexpr unsigned kRounds = 20000;
    unsigned bad = 0, inside = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        unsigned char object[kObjectSize];
        for (auto& b : object) b = static_cast<unsigned char>(Rng());
        if (Rng() % 2) object[0x09] = static_cast<unsigned char>(Rng() % 4);
        if (Rng() % 2) { Put<std::int32_t>(object, 0x0C, static_cast<std::int32_t>(Rng() % 0x20000) - 0x10000);
                         Put<std::int32_t>(object, 0x10, static_cast<std::int32_t>(Rng() % 0x20000) - 0x10000); }
        const int margin = Rng() % 8 == 0 ? static_cast<int>(Rng()) : static_cast<int>(Rng() % 300) - 280;
        const std::uint32_t reach = object[0x70] + static_cast<std::uint32_t>(margin) + 2u;
        auto offset = [&](unsigned shift) -> std::uint32_t {
            const std::uint32_t limit = reach << shift;
            switch (Rng() % 8) {
                case 0: return limit;
                case 1: return limit - 1u;
                case 2: return limit + 1u;
                case 3: return 0u - limit;
                case 4: return 0x80000000u;
                case 5: return Rng();
                default: return limit ? Rng() % limit : 0u;
            }
        };
        const std::uint32_t steps = object[0x09];
        const int x = static_cast<int>(Get<std::uint32_t>(object, kX) + Get<std::uint32_t>(object, 0x0C) * steps + offset(15));
        const int y = static_cast<int>(Get<std::uint32_t>(object, kY) + Get<std::uint32_t>(object, 0x10) * steps + offset(15));
        const unsigned z = (Rng() & 0xFFFF0000u) |
            static_cast<std::uint16_t>(Get<std::uint16_t>(object, kZ) + (Rng() % 3 ? offset(7) % 0x4000 : offset(7)));

        const unsigned char their_result = theirs(x, y, z, margin, object);
        const unsigned char our_result =
            reinterpret_cast<PointFn>(reinterpret_cast<void*>(&Sprite_PointInReach))(x, y, z, margin, object);
        if (our_result) ++inside;
        if (their_result != our_result && ++bad <= 8)
            bof3::Log("shadow      Sprite_PointInReach self-test MISMATCH round %u: reach 0x%X: %u vs ours %u",
                      round, reach, their_result, our_result);
    }
    bof3::Log("shadow      Sprite_PointInReach self-test: %u rounds (%u within reach), %u MISMATCHES",
              kRounds, inside, bad);
    if (bad) bof3::Fatal("Sprite_PointInReach differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// Sprite_ObjectByHandle: every handle, against arrays whose type bytes are
// random but leaned towards 0x0A, so that the original's search - whose
// result is discarded - runs to every length, finds and fails to find.
using HandleFn = unsigned char (__cdecl*)(unsigned);

void SelfTestObjectByHandle(HandleFn theirs) {
    constexpr unsigned kLayouts = 64;
    static unsigned char saved[Sprite_Objects_count];
    std::memcpy(saved, Sprite_Objects, sizeof saved);
    unsigned bad = 0, calls = 0;
    for (unsigned layout = 0; layout < kLayouts; ++layout) {
        const unsigned lean = layout % 4;   // 0: no type-0x0A objects at all
        for (unsigned i = 0; i < kObjects; ++i)
            Sprite_Objects[i * kObjectSize + kType] =
                lean && Rng() % 4 < lean ? 0x0A : static_cast<unsigned char>(Rng() | 1u);
        for (unsigned handle = 0; handle < 0x100; ++handle, ++calls) {
            // Noise above the byte: the original reads only the low one.
            const unsigned arg = handle | (Rng() & 0xFFFFFF00u);
            const unsigned char their_result = theirs(arg);
            const unsigned char our_result = Sprite_ObjectByHandle(static_cast<unsigned char>(handle));
            if (their_result != our_result && ++bad <= 8)
                bof3::Log("shadow      Sprite_ObjectByHandle self-test MISMATCH handle %02X: %u vs ours %u",
                          handle, their_result, our_result);
        }
    }
    std::memcpy(Sprite_Objects, saved, sizeof saved);
    bof3::Log("shadow      Sprite_ObjectByHandle self-test: %u calls (every handle, %u layouts), %u MISMATCHES",
              calls, kLayouts, bad);
    if (bad) bof3::Fatal("Sprite_ObjectByHandle differs from the original in %u of %u self-test calls", bad, calls);
}

// Sprite_InheritDrawKey, against a clone whose two calls reach clones of
// Sprite_ObjectByHandle and Sprite_FindNearby. Objects as in SelfTest, the
// attached bit and the handle random, handles seeded at the boundaries of
// both numberings; every +0x32 word random. Both arrays and the object outside
// them are compared whole afterwards, so a stray store anywhere shows.
using InheritFn = void (__cdecl*)();

void SelfTestInheritDrawKey(InheritFn theirs) {
    constexpr unsigned kRounds = 8000;
    static unsigned char saved_a[Sprite_Objects_count], saved_b[Sprite_ObjectsExtra_count];
    static unsigned char start_a[Sprite_Objects_count], start_b[Sprite_ObjectsExtra_count];
    static unsigned char their_a[Sprite_Objects_count], their_b[Sprite_ObjectsExtra_count];
    static unsigned char outside[kObjectSize], start_out[kObjectSize], their_out[kObjectSize];
    std::memcpy(saved_a, Sprite_Objects, sizeof saved_a);
    std::memcpy(saved_b, Sprite_ObjectsExtra, sizeof saved_b);
    unsigned char* const saved_current = Sprite_Current;
    static const unsigned char kHandles[] = {0x00, 0x01, 0x03, 0x04, 0x05, 0x1D, 0x1E, 0x3F, 0x40, 0x7F,
                                             0x80, 0x81, 0x9D, 0x9E, 0xBF, 0xC0, 0xFF};

    unsigned bad = 0, attached = 0, near = 0, none = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& r : g_records) {
            r[2] = static_cast<unsigned char>(Rng() % 6 == 0 ? Rng() : Rng() % 8);
            r[3] = static_cast<unsigned char>(Rng() % 5 == 0 ? Rng() | 0x10u : Rng() & ~0x10u);
        }
        const std::int32_t cx = static_cast<std::int32_t>(Rng()), cy = static_cast<std::int32_t>(Rng());
        const std::int16_t cz = static_cast<std::int16_t>(Rng());
        const unsigned likely = 1 + Rng() % 12;
        for (unsigned i = 0; i < kObjects; ++i) Fill(Sprite_Objects + i * kObjectSize, likely, cx, cy, cz);
        for (unsigned i = 0; i < kExtra; ++i) Fill(Sprite_ObjectsExtra + i * kObjectSize, likely, cx, cy, cz);
        for (auto& b : outside) b = static_cast<unsigned char>(Rng());

        const unsigned where = Rng() % (kObjects + kExtra + 4);
        unsigned char* const current = where < kObjects ? Sprite_Objects + where * kObjectSize
                                     : where < kObjects + kExtra ? Sprite_ObjectsExtra + (where - kObjects) * kObjectSize
                                     : outside;
        Put<std::int32_t>(current, kX, cx);
        Put<std::int32_t>(current, kY, cy);
        Put<std::int16_t>(current, kZ, cz);
        // A handle whose number stays inside the 34 objects: a larger one reads
        // game memory past Sprite_ObjectsExtra, which is the same for both but
        // not ours to vary. Without bit 7 the number is (h & 0x3F) + 30, with
        // it h & 0x3F; out-of-range ones are folded back, keeping bits 6 and 7.
        unsigned char handle = kHandles[Rng() % sizeof kHandles];
        if (Rng() % 2) handle = static_cast<unsigned char>(Rng());
        const unsigned limit = handle & 0x80 ? kObjects + kExtra : kExtra;
        if ((handle & 0x3Fu) >= limit) handle = static_cast<unsigned char>((handle & 0xC0u) | ((handle & 0x3Fu) % limit));
        current[kHandle] = handle;
        if (Rng() % 2) current[kFlags24] |= 0x20; else current[kFlags24] &= ~0x20;
        Sprite_Current = current;

        std::memcpy(start_a, Sprite_Objects, sizeof start_a);
        std::memcpy(start_b, Sprite_ObjectsExtra, sizeof start_b);
        std::memcpy(start_out, outside, sizeof start_out);
        theirs();
        std::memcpy(their_a, Sprite_Objects, sizeof their_a);
        std::memcpy(their_b, Sprite_ObjectsExtra, sizeof their_b);
        std::memcpy(their_out, outside, sizeof their_out);
        std::memcpy(Sprite_Objects, start_a, sizeof start_a);
        std::memcpy(Sprite_ObjectsExtra, start_b, sizeof start_b);
        std::memcpy(outside, start_out, sizeof start_out);
        Sprite_Current = current;
        Sprite_InheritDrawKey();

        if (current[kFlags24] & 0x20) ++attached;
        else if (Sprite_FindNearby() == 0xFF) ++none;
        else ++near;
        const bool same = !std::memcmp(their_a, Sprite_Objects, sizeof their_a) &&
                          !std::memcmp(their_b, Sprite_ObjectsExtra, sizeof their_b) &&
                          !std::memcmp(their_out, outside, sizeof their_out);
        if (!same && ++bad <= 8)
            bof3::Log("shadow      Sprite_InheritDrawKey self-test MISMATCH round %u: handle %02X, attached %u, "
                      "+0x32 %04X vs ours %04X", round, handle, (current[kFlags24] & 0x20) ? 1u : 0u,
                      Get<std::uint16_t>(where < kObjects ? their_a + where * kObjectSize
                                         : where < kObjects + kExtra ? their_b + (where - kObjects) * kObjectSize
                                         : their_out, kDrawKey),
                      Get<std::uint16_t>(current, kDrawKey));
    }
    std::memcpy(Sprite_Objects, saved_a, sizeof saved_a);
    std::memcpy(Sprite_ObjectsExtra, saved_b, sizeof saved_b);
    Sprite_Current = saved_current;
    bof3::Log("shadow      Sprite_InheritDrawKey self-test: %u rounds (%u attached, %u near an object, %u neither), "
              "%u MISMATCHES", kRounds, attached, near, none, bad);
    if (bad) bof3::Fatal("Sprite_InheritDrawKey differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x589660. The number of the first sprite object of type 0x0A, other
// than the current one, that has the current one within its reach - the reach
// is the found object's, from the record at its +0x54 - or 0xFF. Objects are
// numbered 0..29 in Sprite_Objects and 30..33 in Sprite_ObjectsExtra, the
// numbering 0x57C0A0 uses too.
//
// As the original has it: x and y differences are 32-bit and wrap, and an
// absolute difference of 0x80000000 counts as within any reach; z is 16-bit
// and its limit is a fixed 0x300.
extern "C" unsigned char __cdecl Sprite_FindNearby(void) {
    const unsigned char* const current = Sprite_Current;
    for (unsigned i = 0; i < kObjects; ++i)
        if (Qualifies(Sprite_Objects + i * kObjectSize, current)) return static_cast<unsigned char>(i);
    for (unsigned i = 0; i < kExtra; ++i)
        if (Qualifies(Sprite_ObjectsExtra + i * kObjectSize, current)) return static_cast<unsigned char>(i + kObjects);
    return 0xFF;
}

// original 0x531C70. Whether the point (x, y, z) is within the object's reach -
// byte +0x70, widened by `margin` and 2 - of where the object is about to be:
// its position less dword +0xC (and +0x10) times byte +9.
//
// As the original has it: z is compared inclusively and x and y exclusively;
// everything wraps in 32 bits, and an absolute difference of 0x80000000 counts
// as within any reach.
extern "C" unsigned char __cdecl Sprite_PointInReach(int x, int y, short z, int margin, const unsigned char* object) {
    const std::uint32_t reach = static_cast<std::uint32_t>(object[0x70]) + static_cast<std::uint32_t>(margin) + 2u;
    const int dz = Get<std::int16_t>(object, kZ) - z;
    if ((dz < 0 ? -dz : dz) > static_cast<std::int32_t>(reach << 7)) return 0;
    const std::uint32_t steps = object[0x09];
    const std::int32_t limit = static_cast<std::int32_t>(reach << 15);
    const std::uint32_t ax = static_cast<std::uint32_t>(x) - Get<std::uint32_t>(object, 0x0C) * steps;
    if (WrappingDistance(static_cast<std::int32_t>(ax), Get<std::int32_t>(object, kX)) >= limit) return 0;
    const std::uint32_t ay = static_cast<std::uint32_t>(y) - Get<std::uint32_t>(object, 0x10) * steps;
    if (WrappingDistance(static_cast<std::int32_t>(ay), Get<std::int32_t>(object, kY)) >= limit) return 0;
    return 1;
}

// original 0x57C0A0. The object number an attachment handle names - the byte
// the movement-script command F8 07 stores at +0x18 - in Sprite_FindNearby's
// numbering: without bit 7, (handle & 0x3F) + 30, one of Sprite_ObjectsExtra;
// with it, handle & 0x3F.
//
// As the original has it: for a handle with bit 7 it walks the 30 objects
// looking for the (handle & 0x3F)-th of type 0x0A - and discards what it
// finds: the loop's index is in dl and never reaches al, so the result is
// handle & 0x3F whatever the objects are. The PlayStation's 0x8015BEE4 does
// the same (it returns the count of matches, which equals the number sought).
// The walk has no other effect - it writes its index into the argument's own
// stack slot, which both callers discard - so it is not repeated here. No
// shipped script attaches with a bit-7 handle (docs/known-defects.md D6), so
// the discarded search is never asked.
namespace {

unsigned char ObjectByHandle(unsigned char handle) {
    const unsigned char n = handle & 0x3F;
    return handle & 0x80 ? n : static_cast<unsigned char>(n + kObjects);
}

// --- BOF3X_SHADOW=sprite_find, live: every call in game runs the original's
// clone first, then ours from the same state, and the results are compared.
HandleFn g_handle_live = nullptr;
InheritFn g_inherit_live = nullptr;
struct LiveCounts { unsigned calls, bit7, mismatches; } g_live_handle, g_live_inherit;

void LiveReport(const char* name, const LiveCounts& c) {
    if (c.calls == 1 || c.calls % 4096 == 0)
        bof3::Log("shadow      %s live: %u calls (%u with bit 7), %u MISMATCHES", name, c.calls, c.bit7, c.mismatches);
}

}  // namespace

extern "C" unsigned char __cdecl Sprite_ObjectByHandle(unsigned char handle) {
    const unsigned char ours = ObjectByHandle(handle);
    if (g_handle_live) {
        const unsigned char theirs = g_handle_live(handle);
        LiveCounts& c = g_live_handle;
        ++c.calls;
        if (handle & 0x80) ++c.bit7;
        if (theirs != ours && ++c.mismatches <= 8)
            bof3::Log("shadow      Sprite_ObjectByHandle live MISMATCH: call %u, handle %02X: %u / %u", c.calls, handle,
                      theirs, ours);
        LiveReport("Sprite_ObjectByHandle", c);
    }
    return ours;
}

namespace {

// An object number as Sprite_InheritDrawKey turns it into an address: signed,
// 30 and above into Sprite_ObjectsExtra, and no bound on either side.
const unsigned char* ObjectByNumber(unsigned char number) {
    const int n = static_cast<signed char>(number);
    return n >= static_cast<int>(kObjects) ? Sprite_ObjectsExtra + (n - static_cast<int>(kObjects)) * static_cast<int>(kObjectSize)
                                           : Sprite_Objects + n * static_cast<int>(kObjectSize);
}

}  // namespace

// original 0x589770. The current object takes the draw key (+0x32: layer,
// then order within it) of the object it is attached to - bit 0x20 of +0x24,
// the handle at +0x18 - or else of the first type-0x0A object it is near
// (Sprite_FindNearby); near none, nothing changes. So a carried object, or
// one standing by another, sorts and layers with it.
//
// As the original has it: the number is compared signed, and on the attached
// path is not checked for 0xFF; neither can arise from Sprite_ObjectByHandle,
// whose results are 0..0x5D. A number past 33 reads past Sprite_ObjectsExtra.
namespace {

void InheritDrawKey() {
    unsigned char number;
    if (Sprite_Current[kFlags24] & 0x20) {
        number = ObjectByHandle(Sprite_Current[kHandle]);
    } else {
        number = Sprite_FindNearby();
        if (number == 0xFF) return;
    }
    Put<std::uint16_t>(Sprite_Current, kDrawKey, Get<std::uint16_t>(ObjectByNumber(number), kDrawKey));
}

}  // namespace

extern "C" void __cdecl Sprite_InheritDrawKey(void) {
    if (!g_inherit_live) return InheritDrawKey();
    // The one store either makes is the current object's +0x32.
    unsigned char* const current = Sprite_Current;
    const std::uint16_t before = Get<std::uint16_t>(current, kDrawKey);
    g_inherit_live();
    const std::uint16_t theirs = Get<std::uint16_t>(current, kDrawKey);
    Put<std::uint16_t>(current, kDrawKey, before);
    InheritDrawKey();
    const std::uint16_t ours = Get<std::uint16_t>(current, kDrawKey);
    LiveCounts& c = g_live_inherit;
    ++c.calls;
    if ((current[kFlags24] & 0x20) && (current[kHandle] & 0x80)) ++c.bit7;
    if (theirs != ours && ++c.mismatches <= 8)
        bof3::Log("shadow      Sprite_InheritDrawKey live MISMATCH: call %u, attached %u handle %02X: +0x32 %04X / %04X",
                  c.calls, (current[kFlags24] & 0x20) ? 1u : 0u, current[kHandle], theirs, ours);
    LiveReport("Sprite_InheritDrawKey", c);
}

void SpriteFind_Inject() {
    // 0x589660..0x589769, 0x531C70..0x531CEF and 0x57C0A0..0x57C0E9: no
    // calls, every jump internal (disasm 2026-09-20, 2026-09-21).
    // 0x589770..0x589807: every jump internal; two calls, the E8 at +0x0F to
    // 0x57C0A0 and at +0x53 to 0x589660, which the clone makes to their clones.
    if (bof3::WantsShadow("sprite_find")) {
        void* const find = bof3::CloneOriginal("Sprite_FindNearby", bof3::addr::Sprite_FindNearby, 0x10A);
        void* const handle = bof3::CloneOriginal("Sprite_ObjectByHandle", bof3::addr::Sprite_ObjectByHandle, 0x4A);
        const bof3::CloneCall calls[] = {{0x0F, handle}, {0x53, find}};
        void* const inherit =
            bof3::CloneOriginal("Sprite_InheritDrawKey", bof3::addr::Sprite_InheritDrawKey, 0x98, calls, 2);
        SelfTest(reinterpret_cast<FindFn>(find));
        SelfTestPointInReach(reinterpret_cast<PointFn>(
            bof3::CloneOriginal("Sprite_PointInReach", bof3::addr::Sprite_PointInReach, 0x80)));
        SelfTestObjectByHandle(reinterpret_cast<HandleFn>(handle));
        SelfTestInheritDrawKey(reinterpret_cast<InheritFn>(inherit));
        // Live from here on. The clones are the ones the fuzz used: the draw
        // key's calls reach the clones of the handle lookup and the search.
        g_handle_live = reinterpret_cast<HandleFn>(handle);
        g_inherit_live = reinterpret_cast<InheritFn>(inherit);
    }
    BOF3_INJECT(Sprite_FindNearby);
    BOF3_INJECT(Sprite_PointInReach);
    BOF3_INJECT(Sprite_ObjectByHandle);
    BOF3_INJECT(Sprite_InheritDrawKey);
}

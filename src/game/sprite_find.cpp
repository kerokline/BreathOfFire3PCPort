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
constexpr unsigned kFlags24 = 0x24;   // u8, bit 0x40
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

void SpriteFind_Inject() {
    // 0x589660..0x589769: no calls, every jump internal (disasm 2026-09-20).
    if (bof3::WantsShadow("sprite_find"))
        SelfTest(reinterpret_cast<FindFn>(
            bof3::CloneOriginal("Sprite_FindNearby", bof3::addr::Sprite_FindNearby, 0x10A)));
    BOF3_INJECT(Sprite_FindNearby);
}

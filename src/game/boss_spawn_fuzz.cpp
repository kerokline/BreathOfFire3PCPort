// BOF3X_SHADOW=boss_spawn: the six spawn helpers through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run, every function a
// kCallee (its tag and arguments drawn by Args, the tag planted by Seed).
// docs/boss_h.md section 5.
//
// The clone rows were read against the disassembly (capstone, 2026-09-28):
// the extents run to each function's last ret; the three writers call
// BossActor_Find at +5, which stands in as the harness's standard recorder
// (a field object in the compared state, never null - the original's null is
// the fault docs/boss_h.md section 6 describes).
#include <cstddef>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_spawn.h"
#include "game/move_script_bytes.h"

namespace boss_spawn {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
#define BS_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BS_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])

constexpr bh::CallSite kCallsFind[] = {{0x5, 0x494920}};
const bh::Clone kClones[] = {
    {"EnemyData_FindByTag", 0x4948E0, 0x3A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EnemyData_FindByTag), 0xFF, false, bh::Shape::kCallee},
    {"BossActor_Find", 0x494920, 0x5D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BossActor_Find), 0xFFFFFFFFu, false, bh::Shape::kCallee},
    {"BossActor_Index", 0x494980, 0x42, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BossActor_Index), 0xFF, false, bh::Shape::kCallee},
    {"BossActor_ClearBit40", 0x4949D0, 0x15, kCallsFind, BS_N(kCallsFind), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BossActor_ClearBit40), 0, false, bh::Shape::kCallee},
    {"BossActor_CopyFrom", 0x4949F0, 0x66, kCallsFind, BS_N(kCallsFind), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BossActor_CopyFrom), 0, false, bh::Shape::kCallee},
    {"BossActor_Clear", 0x494A60, 0x1E, kCallsFind, BS_N(kCallsFind), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BossActor_Clear), 0, false, bh::Shape::kCallee},
};
enum : unsigned { kData, kFind, kIndex, kBit40, kCopy, kClear };
static_assert(kClear + 1 == BS_COUNT(kClones), "the seeding indices");

// The round's tag, chosen by Seed and handed to Args (an args hook that
// writes memory is lost: docs/magic_harness.md section 5).
unsigned g_tag;

unsigned char Tag() { return static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 2, 6, 7, 0x10, 0xFF) : bh::Next()); }

void Seed(unsigned k) {
    g_tag = Tag();
    if (k == kData) {
        // the eight records' +0xC: the tag in none, one or several of them,
        // its neighbours in others
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const r = Mem(bh::at::kEnemyData + i * bh::at::kEnemyDataStride);
            const U pick = bh::Next() % 5;
            r[0xC] = static_cast<unsigned char>(pick == 0 ? g_tag : pick == 1 ? g_tag + 1 : pick == 2 ? g_tag - 1 : r[0xC]);
        }
        // a third of the rounds, no record carries it (the 0xFF answer)
        if (bh::Next() % 3 == 0)
            for (unsigned i = 0; i < 8; ++i) {
                unsigned char* const r = Mem(bh::at::kEnemyData + i * bh::at::kEnemyDataStride);
                if (r[0xC] == g_tag) r[0xC] ^= 0x80;
            }
        return;
    }
    // the field actors: some carry the tag with type 7, some the tag with
    // another type, some type 7 with another tag
    for (unsigned i = 0; i < bh::at::kObjectCount; ++i) {
        unsigned char* const o = bh::Object(i);
        switch (bh::Next() % 6) {
        case 0: o[6] = 7; o[0x9E] = static_cast<unsigned char>(g_tag); break;
        case 1: o[6] = static_cast<unsigned char>(bh::Half() ? 6 : 0x87); o[0x9E] = static_cast<unsigned char>(g_tag); break;
        case 2: o[6] = 7; o[0x9E] = static_cast<unsigned char>(g_tag ^ (1u << (bh::Next() % 8))); break;
        default: break;
        }
    }
    // a third of the rounds, no actor carries it (null, 0xFF - the finders'
    // other answer; the writers are handed the standard stand-in's object)
    if (bh::Next() % 3 == 0)
        for (unsigned i = 0; i < bh::at::kObjectCount; ++i) {
            unsigned char* const o = bh::Object(i);
            if (o[6] == 7 && o[0x9E] == g_tag) o[0x9E] ^= 0x80;
        }
}

void Args(unsigned k, U* a) {
    // the tag with garbage above it (the functions read its low byte)
    a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | g_tag;
    if (k != kCopy) return;
    // from: an enemy, a field object or a party record, all in the compared state
    switch (bh::Next() % 3) {
    case 0: a[1] = Key(bh::EnemyAt(bh::Next())); break;
    case 1: a[1] = Key(bh::Object(bh::Next())); break;
    default: a[1] = Key(bh::PartyOf(static_cast<unsigned char>(bh::Next()))); break;
    }
    a[2] = bh::Often() ? BH_PICK(0, 1, 2, 0x100, 0x101, 0xFF) : bh::Next();
}

}  // namespace

void SelfTest() {
    bh::Group g{"boss_spawn", kClones, BS_COUNT(kClones), nullptr, 0, nullptr, 0, nullptr, 0, &Seed, nullptr, 4000};
    g.args = &Args;
    bh::Run(g);
}

}  // namespace boss_spawn

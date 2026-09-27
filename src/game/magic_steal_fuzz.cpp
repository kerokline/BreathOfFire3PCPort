// BOF3X_SHADOW=magic_steal: Steal's overlay through the spell round's shared
// harness (magic_harness.h), once at start-up. docs/magic_steal.md section 3.
//
// This file is what a spell group writes: the clone table
// (tools/magic_rows.py --unit MAGIC216 --clones), the regions the harness
// does not hold already (the overlay's .data), and a seed per function.
// Everything else - the recorders, the state, the two passes, the
// comparison - is the harness's.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_steal.h"

namespace magic_steal {
namespace {

namespace mh = magic_harness;

// tools/magic_rows.py --unit MAGIC216 --clones, 2026-09-25 (capstone: every
// jump internal, no jump table; the task's three stack-table immediates, the
// roll's seven calls).
constexpr mh::Imm kImms4F50B0[] = {{0xF, 0x4F50E0}, {0x17, 0x4F5140}, {0x22, 0x4F52D0}};
constexpr mh::CallSite kCalls4F5140[] = {{0xA6, 0x5B93D2}, {0xEB, 0x590BB0}, {0xF9, 0x4B58F0}, {0x100, 0x497740},
                                         {0x10D, 0x44A880}, {0x165, 0x497740}, {0x172, 0x44A880}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"SkillSteal_Task", 0x4F50B0, 0x2E, nullptr, 0, kImms4F50B0, MH_N(kImms4F50B0), nullptr, 0,
     reinterpret_cast<const void*>(&::SkillSteal_Task)},
    {"SkillSteal_Start", 0x4F50E0, 0x56, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SkillSteal_Start)},
    {"SkillSteal_Roll", 0x4F5140, 0x18A, kCalls4F5140, MH_N(kCalls4F5140), nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::SkillSteal_Roll)},
};
#undef MH_N
enum : unsigned { kTask, kStart, kRoll };

constexpr std::uint32_t kRates = 0x65C204;   // SkillSteal_RateTable, 8 signed bytes
const mh::Region kRegions[] = {{kRates, 8}};

constexpr unsigned kEnemyItem = 0xA8, kEnemyRate = 0xAA, kEnemySpeed = 0xB8, kThiefSpeed = 0xA8;

// The table as the exe holds it, read at start-up (not written down here):
// random bytes make almost every rate too large or negative for a roll to
// meet.
unsigned char g_exe_rates[8];

void Seed(unsigned k) {
    unsigned char* const sc = Sprite_Current;
    switch (k) {
    case kTask:
        sc[1] = static_cast<unsigned char>(mh::Next() % 3);
        break;
    case kStart:
        break;
    case kRoll: {
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Often() ? 3 + mh::Next() % 8 : mh::Next() % 3);
        unsigned char* const e = mh::EnemyOf(mh::Mem(mh::at::kTarget)[0]);
        if (mh::Often()) e[kEnemyRate] = static_cast<unsigned char>(mh::Half() ? 1 + mh::Next() % 7 : mh::Next() % 8);
        if (mh::Often()) std::memcpy(mh::Mem(kRates), g_exe_rates, sizeof g_exe_rates);
        if (mh::Half()) {
            const unsigned item = mh::Half() ? 0 : mh::Next() & 0xFFFF;
            e[kEnemyItem] = static_cast<unsigned char>(item);
            e[kEnemyItem + 1] = static_cast<unsigned char>(item >> 8);
        }
        // The speed difference at a threshold or one below, and Rand aimed at
        // the rate times the tier the original takes for it: a threshold
        // moved by one changes the answer at exactly that roll.
        const int d = static_cast<int>(MH_PICK(49, 48, 29, 28, 19, 18, 9, 8, static_cast<std::uint32_t>(-10),
                                               static_cast<std::uint32_t>(-11), static_cast<std::uint32_t>(-20),
                                               static_cast<std::uint32_t>(-21), static_cast<std::uint32_t>(-30),
                                               static_cast<std::uint32_t>(-31), static_cast<std::uint32_t>(-50),
                                               static_cast<std::uint32_t>(-51), 0x3FFF, static_cast<std::uint32_t>(-0x4000)));
        unsigned tier = MH_PICK(4, 5, 6, 7, 8, 9, 10, 11, 12);
        if (mh::Next() % 4) {
            const unsigned speed = (mh::Next() & 0x3FFF) + 0x4000u;
            e[kEnemySpeed] = static_cast<unsigned char>(speed);
            e[kEnemySpeed + 1] = static_cast<unsigned char>(speed >> 8);
            unsigned char* const thief = mh::PartyOf(mh::Mem(mh::at::kActor)[0]);
            const unsigned t = static_cast<unsigned>(static_cast<int>(speed) + d);
            thief[kThiefSpeed] = static_cast<unsigned char>(t);
            thief[kThiefSpeed + 1] = static_cast<unsigned char>(t >> 8);
            tier = d >= 49 ? 12 : d >= 29 ? 11 : d >= 19 ? 10 : d >= 9 ? 9 : d >= -10 ? 8 : d >= -20 ? 7 : d >= -30 ? 6 : d >= -50 ? 5 : 4;
        }
        const unsigned rate =
            e[kEnemyRate] < 8 ? static_cast<unsigned>(static_cast<signed char>(mh::Mem(kRates)[e[kEnemyRate]])) : 0;
        const std::uint32_t hint = rate * tier;
        mh::SetRandHint(hint);
        // Half the time the roll lands on the compare exactly, or one below.
        if (mh::Half() && hint <= 0xFF) mh::SetRandFirst(static_cast<int>((hint - (mh::Half() ? 1u : 0u)) & 0xFF));
        break;
    }
    default:
        break;
    }
}

}  // namespace

void SelfTest() {
    std::memcpy(g_exe_rates, mh::Mem(kRates), sizeof g_exe_rates);
    const mh::Group group = {
        "magic_steal", kClones, sizeof kClones / sizeof kClones[0], nullptr, 0, nullptr, 0,
        kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, nullptr, 2000,
    };
    mh::Run(group);
}

}  // namespace magic_steal

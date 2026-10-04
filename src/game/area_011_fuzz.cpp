// BOF3X_SHADOW=area_011: area 11's three functions through the area round's
// shared harness (area_harness.h), once at start-up. docs/area_011.md
// section 3.
//
// The clone table was built by hand (capstone recursive descent of each
// root, 2026-09-27; every jump internal, no jump table, nothing refused), as
// round nine's group E built its before a tool printed one; tools/area_rows.py
// (group ART) should print the same for area 11 (docs/area_011.md section 5).
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_011.h"
#include "game/area_011_callees.h"
#include "game/area_harness.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_011 {
namespace {

namespace ah = area_harness;

// 0x401750: 0x1C bytes, no calls - handler 0
// 0x401770: 0x46 bytes - handler 1
constexpr ah::CallSite kCalls401770[] = {{0x30, 0x57CE10}};   // Effect_Spawn
// 0x4017C0: 0x79 bytes - the init
constexpr ah::CallSite kCalls4017C0[] = {{0x8, 0x57C140}, {0x1B, 0x57C140}};   // Flags_Test, twice
#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const ah::Clone kClones[] = {
    {"Area11_StepCameraDistance", 0x401750, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Area11_StepCameraDistance), 0, false, ah::Shape::kHandler},
    {"Area11_SpawnEffect", 0x401770, 0x46, kCalls401770, AH_N(kCalls401770), nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Area11_SpawnEffect), 0, false, ah::Shape::kHandler},
    {"Area11_DimBackdrop", 0x4017C0, 0x79, kCalls4017C0, AH_N(kCalls4017C0), nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Area11_DimBackdrop), 0, false, ah::Shape::kInit},
};
#undef AH_N
enum : unsigned { kStepCamera, kSpawnEffect, kDimBackdrop };

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// Effect_Spawn answers an Effect_Objects slot or 0xFF for none: the
// standard listing's kFlag never answers 0xFF, so it is listed here - 0xFE,
// 0xFF, 0, 1, 2 (0xFF a fifth of the time, 0xFE the byte beside it).
constexpr std::uint32_t kU8 = 0xFFu, kU16 = 0xFFFFu;
const ah::Callee kCallees[] = {
    {"Effect_Spawn", ::bof3::addr::Effect_Spawn, KeyOf(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02},
};

// The chain AreaMap_HeaderPass walks, built in the area block: 0..10 entries
// of kinds around 0x81 with steps of 1..3 dwords, a zero dword the end.
void Chain() {
    const unsigned base = 0x10 + ah::Next() % 0x300;
    AreaMap_EntryBase = static_cast<unsigned short>(base);
    unsigned char* p = AreaMap_Header + base * 4u;
    const unsigned count = ah::Next() % 11;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned step = 1 + ah::Next() % 3;
        const std::uint32_t kind = AH_PICK(0x81, 0x81, 0x80, 0x82, 0x01, 0x00, 0xC1, 0x41) << 24;
        const std::uint32_t low = ah::Half() ? ah::Next() & 0xFFFF : 0;
        move_script::SetLong(p, static_cast<std::int32_t>(kind | step << 16 | low));
        p += step * 4u;
    }
    move_script::SetLong(p, 0);
}

void Seed(unsigned k) {
    switch (k) {
    case kStepCamera:
        // the compare's two sides, the wrap at the bottom, anything
        if (ah::Often()) Camera_Distance = static_cast<short>(AH_PICK(0xEC00, 0xEBFF, 0xEC01, 0xEC60, 0x7FFF, 0x8000, 0));
        break;
    case kSpawnEffect:
        // a member id the eight-byte table holds, mostly
        if (ah::Often()) ah::Mem(at::kPartyThird)[0] = static_cast<unsigned char>(ah::Next() % 8);
        break;
    case kDimBackdrop: {
        // the two flags in all four ways (the recorder answers Flags_Test;
        // the bits are set for the record), and the chain
        unsigned char* const flags = ah::Mem(at::kInitFlags + 2);
        flags[0] = static_cast<unsigned char>((flags[0] & ~0x18u) | (ah::Next() & 0x18u));
        Chain();
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    ah::Group g{"area_011", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
                nullptr, 0, nullptr, 0, &Seed, nullptr, 4000};
    g.area = 11;
    ah::Run(g);
}

}  // namespace area_011

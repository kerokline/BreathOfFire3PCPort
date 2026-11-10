#include "game/sprite_order.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// --- BOF3X_SHADOW=sprite_order: a differential fuzz, once at start-up ----------
// Both functions are straight-line code with no calls, so a byte-copy of each
// runs anywhere. Ours against the copy, on the same input, results compared.

std::uint32_t g_rng = 0x9E3779B9u;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

using ListSwapFn = void (__cdecl*)(unsigned, unsigned);
using RecordSwapFn = void (__cdecl*)(unsigned long*, unsigned long*);

// The byte mask lets an index reach 255 of a 40-entry list, so the fuzz owns
// the 256 dwords from Sprite_DrawList for its duration and puts them back.
constexpr unsigned kReach = 256;

void SelfTestListSwap(ListSwapFn theirs) {
    constexpr unsigned kRounds = 4000;
    static unsigned char* saved[kReach], *input[kReach], *their_out[kReach];
    std::memcpy(saved, Sprite_DrawList, sizeof saved);
    unsigned bad = 0, same_index = 0, beyond = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& p : input) p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(Rng()));
        // The caller's arguments are whole registers with only the low byte
        // meant, so the upper 24 bits are noise here too.
        unsigned i = Rng(), j = Rng() % 8 == 0 ? (Rng() & ~0xFFu) | (i & 0xFF) : Rng();
        if (Rng() % 2) { i = (i & ~0xFFu) | (i & 0xFF) % 40; j = (j & ~0xFFu) | (j & 0xFF) % 40; }
        if ((i & 0xFF) == (j & 0xFF)) ++same_index;
        if ((i & 0xFF) >= 40 || (j & 0xFF) >= 40) ++beyond;

        std::memcpy(Sprite_DrawList, input, sizeof input);
        theirs(i, j);
        std::memcpy(their_out, Sprite_DrawList, sizeof their_out);
        std::memcpy(Sprite_DrawList, input, sizeof input);
        Sprite_DrawListSwap(i, j);
        if (std::memcmp(their_out, Sprite_DrawList, sizeof their_out) != 0 && ++bad <= 8)
            bof3::Log("shadow      Sprite_DrawListSwap self-test MISMATCH round %u: i 0x%08X j 0x%08X", round, i, j);
    }
    std::memcpy(Sprite_DrawList, saved, sizeof saved);
    bof3::Log("shadow      Sprite_DrawListSwap self-test: %u rounds (%u with i == j, %u reaching past entry 39), "
              "%u MISMATCHES; 256 entries compared", kRounds, same_index, beyond, bad);
    if (bad) bof3::Fatal("Sprite_DrawListSwap differs from the original in %u of %u self-test rounds", bad, kRounds);
}

void SelfTestRecordSwap(RecordSwapFn theirs) {
    constexpr unsigned kRounds = 4000;
    constexpr unsigned kWords = 16;
    unsigned long input[kWords], their_out[kWords], ours[kWords];
    unsigned bad = 0, overlapping = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& w : input) w = Rng();
        // The game only ever passes neighbours, 12 bytes apart. One round in
        // four here overlaps - by 0, 4 or 8 bytes - where the order of the
        // original's reads and writes is what decides the result.
        const unsigned a = Rng() % (kWords - 2);
        const unsigned b = Rng() % 4 == 0 ? a + Rng() % 5 - 2 : Rng() % (kWords - 2);
        if (b > kWords - 3) continue;
        if ((a > b ? a - b : b - a) < 3) ++overlapping;

        std::memcpy(their_out, input, sizeof input);
        theirs(their_out + a, their_out + b);
        std::memcpy(ours, input, sizeof input);
        Sprite_DrawRecordSwap(ours + a, ours + b);
        if (std::memcmp(their_out, ours, sizeof ours) != 0 && ++bad <= 8)
            bof3::Log("shadow      Sprite_DrawRecordSwap self-test MISMATCH round %u: a word %u, b word %u", round, a, b);
    }
    bof3::Log("shadow      Sprite_DrawRecordSwap self-test: %u rounds (%u overlapping), %u MISMATCHES",
              kRounds, overlapping, bad);
    if (bad) bof3::Fatal("Sprite_DrawRecordSwap differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void SpriteOrder_Inject() {
    // 0x593530..0x593565 and 0x593570..0x5935A4: no calls and no jumps at all
    // (disasm 2026-09-20).
    if (bof3::WantsShadow("sprite_order")) {
        SelfTestListSwap(reinterpret_cast<ListSwapFn>(
            bof3::CloneOriginal("Sprite_DrawListSwap", bof3::addr::Sprite_DrawListSwap, 0x36)));
        SelfTestRecordSwap(reinterpret_cast<RecordSwapFn>(
            bof3::CloneOriginal("Sprite_DrawRecordSwap", bof3::addr::Sprite_DrawRecordSwap, 0x35)));
    }
}

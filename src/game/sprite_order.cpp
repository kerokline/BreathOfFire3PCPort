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

// original 0x593530. Exchanges two entries of the sprite draw list, for the
// bubble sort at the head of 0x593060.
//
// As the original has it: only the low byte of each index counts - the caller
// pushes whole registers of which it set one byte - and nothing holds that
// byte to the list's 40 entries.
extern "C" void __cdecl Sprite_DrawListSwap(unsigned i, unsigned j) {
    i &= 0xFF;
    j &= 0xFF;
    unsigned char* const at_i = Sprite_DrawList[i];
    unsigned char* const at_j = Sprite_DrawList[j];
    Sprite_DrawList[j] = at_i;
    Sprite_DrawList[i] = at_j;
}

// original 0x593570. Exchanges two 12-byte draw records, for the per-layer
// bubble sort in 0x593060 - the hottest unnamed logic function of the attract
// run, 421,058 calls.
//
// As the original has it, in its order: all of b is read; then a is copied to
// b a dword at a time; then what was read goes to a. The game passes
// neighbouring records, which do not overlap; for records that do, this order
// is the result.
extern "C" void __cdecl Sprite_DrawRecordSwap(unsigned long* a, unsigned long* b) {
    const unsigned long b0 = b[0], b1 = b[1], b2 = b[2];
    b[0] = a[0];
    b[1] = a[1];
    b[2] = a[2];
    a[0] = b0;
    a[1] = b1;
    a[2] = b2;
}

void SpriteOrder_Inject() {
    // 0x593530..0x593565 and 0x593570..0x5935A4: no calls and no jumps at all
    // (disasm 2026-09-20).
    if (bof3::WantsShadow("sprite_order")) {
        SelfTestListSwap(reinterpret_cast<ListSwapFn>(
            bof3::CloneOriginal("Sprite_DrawListSwap", bof3::addr::Sprite_DrawListSwap, 0x36)));
        SelfTestRecordSwap(reinterpret_cast<RecordSwapFn>(
            bof3::CloneOriginal("Sprite_DrawRecordSwap", bof3::addr::Sprite_DrawRecordSwap, 0x35)));
    }
    BOF3_INJECT(Sprite_DrawListSwap);
    BOF3_INJECT(Sprite_DrawRecordSwap);
}

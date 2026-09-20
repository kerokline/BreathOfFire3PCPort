#include "game/prim.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// --- BOF3X_SHADOW=prim: every input there is, once at start-up -----------------
// Six instructions, no calls, no jumps: a byte-copy runs anywhere. All 256
// levels, each with random upper bits in the argument (the original reads one
// byte of it), on a random 16-byte buffer; the whole buffer compared.

using SetShadeFn = void (__cdecl*)(unsigned char*, unsigned);

void SelfTest(SetShadeFn theirs) {
    std::uint32_t rng = 0x85EBCA6Bu;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    unsigned bad = 0;
    for (unsigned level = 0; level < 256; ++level) {
        unsigned char input[16], their_out[16], ours[16];
        for (auto& b : input) b = static_cast<unsigned char>(next());
        const unsigned argument = (next() & ~0xFFu) | level;
        std::memcpy(their_out, input, sizeof input);
        theirs(their_out, argument);
        std::memcpy(ours, input, sizeof input);
        // Ours through the same signature the game's callers assume, so that
        // the noise above the low byte reaches it as it would in game.
        reinterpret_cast<SetShadeFn>(reinterpret_cast<void*>(&Prim_SetShade))(ours, argument);
        if (std::memcmp(their_out, ours, sizeof ours) != 0 && ++bad <= 8)
            bof3::Log("shadow      Prim_SetShade self-test MISMATCH at level %u", level);
    }
    bof3::Log("shadow      Prim_SetShade self-test: all 256 levels, %u MISMATCHES; 16 bytes compared", bad);
    if (bad) bof3::Fatal("Prim_SetShade differs from the original at %u of 256 levels", bad);
}

}  // namespace

// original 0x462A70. One level to bytes +4, +5 and +6 - where a PSX GPU
// primitive keeps r0, g0, b0 - leaving +7, the primitive's code, alone.
extern "C" void __cdecl Prim_SetShade(unsigned char* prim, unsigned char level) {
    prim[4] = level;
    prim[5] = level;
    prim[6] = level;
}

void Prim_Inject() {
    // 0x462A70..0x462A81: no calls, no jumps (disasm 2026-09-20).
    if (bof3::WantsShadow("prim"))
        SelfTest(reinterpret_cast<SetShadeFn>(
            bof3::CloneOriginal("Prim_SetShade", bof3::addr::Prim_SetShade, 0x12)));
    BOF3_INJECT(Prim_SetShade);
}

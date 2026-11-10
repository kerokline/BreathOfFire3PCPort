#include "game/prim.h"

#include <cstdint>
#include <cstring>
#include <utility>

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

// --- BOF3X_SHADOW=prim, second part: Prim_SetTexture ---------------------------
// A byte-copy of 0x572A00 with its nine calls left where they were - _ftol and
// Gpu_SetSemiTrans, still Capcom's while this runs (Prim_Inject comes before
// PsxGpu_Inject). One round: a random texture word, biased a third to each of
// its three sources; a random run of primitives; the area block's tables
// random. Theirs under each of four x87 control words in turn, ours from the
// same state, the primitives compared - five of them and a guard one.

using SetTextureFn = void (__cdecl*)(unsigned long, unsigned char*, int);

constexpr unsigned kTexPrimBytes = 0x48, kTexPrims = 6;
// What the table sources can reach: a u16 offset plus 0x7FF * 2 + 1, in dwords.
constexpr unsigned kAreaWindow = (0x10000u + 0x1000u) * 4u;

unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

void SelfTestTexture(SetTextureFn theirs) {
    // The game's word (psx_gte_float.cpp), 24 and 64 bits, and 64 bits
    // rounding toward zero: every value this function puts through the FPU
    // is a small integer, so none of them may make a difference.
    static const unsigned short kWords[] = {0x027F, 0x007F, 0x037F, 0x0F7F};
    constexpr unsigned kRounds = 64000;
    static unsigned char saved_area[kAreaWindow];
    std::memcpy(saved_area, AreaMap_Header, kAreaWindow);
    const unsigned short saved_word = GetControlWord();

    std::uint32_t rng = 0x572A0017u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    unsigned bad = 0, source[3] = {0, 0, 0}, turned = 0, flipped = 0, empty = 0, drawn = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        // The table: all of it random now and then, its eight offsets every
        // round - small, mostly, so that rounds meet the same entries again.
        if (round % 4096 == 0)
            for (unsigned i = 0; i < kAreaWindow; ++i) AreaMap_Header[i] = static_cast<unsigned char>(next());
        for (unsigned i = 0; i < 8; ++i) {
            const auto at = static_cast<unsigned short>(next() % 4 == 0 ? next() : next() % 0x400);
            std::memcpy(AreaMap_Header + 4 + 2 * i, &at, 2);
        }
        unsigned long texture = next();
        switch (next() % 3) {
            case 0: texture &= ~0xF00ul; ++source[0]; break;
            case 1: texture |= 0x800; ++source[1]; break;
            default: texture = (texture & ~0xF00ul) | ((1 + next() % 7) << 8); ++source[2]; break;
        }
        if (texture & 0x40000) ++turned;
        if (texture & 0x30000) ++flipped;
        int count = static_cast<int>(next() % 7) - 1;
        if (next() % 32 == 0) count = static_cast<int>(next() | 0x80000000u);
        const int limited = count > 5 ? 5 : count;   // never past the guard primitive
        if (limited <= 0) ++empty; else drawn += static_cast<unsigned>(limited);

        unsigned char input[kTexPrims * kTexPrimBytes], their_out[sizeof input], ours[sizeof input];
        for (auto& b : input) b = static_cast<unsigned char>(next());
        std::memcpy(their_out, input, sizeof input);
        SetControlWord(kWords[round % 4]);
        theirs(texture, their_out, limited);
        SetControlWord(saved_word);
        std::memcpy(ours, input, sizeof input);
        Prim_SetTexture(texture, ours, limited);
        if (std::memcmp(their_out, ours, sizeof ours) != 0 && ++bad <= 8) {
            unsigned at = 0;
            while (their_out[at] == ours[at]) ++at;
            bof3::Log("shadow      Prim_SetTexture self-test MISMATCH: round %u, word %04X, texture %08lX, count %d, "
                      "first difference at +0x%X (%02X / %02X)", round, kWords[round % 4], texture, limited, at,
                      their_out[at], ours[at]);
        }
    }
    std::memcpy(AreaMap_Header, saved_area, kAreaWindow);
    SetControlWord(saved_word);
    bof3::Log("shadow      Prim_SetTexture self-test: %u rounds, a quarter under each of control words 027F 007F 037F "
              "0F7F (%u from the grid, %u four corners from the table, %u a rectangle from the table; %u turned, %u "
              "flipped, %u empty, %u primitives drawn), %u MISMATCHES; six primitives compared",
              kRounds, source[0], source[1], source[2], turned, flipped, empty, drawn, bad);
    if (bad) bof3::Fatal("Prim_SetTexture differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void Prim_Inject() {
    // 0x462A70..0x462A81: no calls, no jumps (disasm 2026-09-20).
    if (bof3::WantsShadow("prim"))
        SelfTest(reinterpret_cast<SetShadeFn>(
            bof3::CloneOriginal("Prim_SetShade", bof3::addr::Prim_SetShade, 0x12)));
    if (bof3::WantsShadow("prim")) {
        // 0x572A00..0x572EC8: calls at these offsets, capstone 2026-09-21 -
        // eight to _ftol 0x5B9550, one to Gpu_SetSemiTrans 0x5A7780. No jump
        // leaves it.
        static const bof3::CloneCall kCalls[] = {{0x3E4, nullptr}, {0x3F1, nullptr}, {0x3FE, nullptr},
                                                 {0x40B, nullptr}, {0x418, nullptr}, {0x425, nullptr},
                                                 {0x432, nullptr}, {0x43F, nullptr}, {0x460, nullptr}};
        SelfTestTexture(reinterpret_cast<SetTextureFn>(
            bof3::CloneOriginal("Prim_SetTexture", bof3::addr::Prim_SetTexture, 0x4C9, kCalls, 9)));
    }
}

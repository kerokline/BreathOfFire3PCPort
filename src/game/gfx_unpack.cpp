#include "game/gfx_unpack.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The two packed forms a queued image upload can take (Gfx_FlushUploadQueue,
// record kinds 1 and 2). Both unpack w * h * 2 bytes into the scratch at
// Gfx_UnpackNext, hand that to Gfx_LoadImage, and then claim the space by
// advancing Gfx_UnpackNext to the next multiple of four.

// --- BOF3X_SHADOW=gfx_unpack: start-up differential fuzz against clones ---------
// (docs/SCAFFOLDING.md section 2.) The clones' call is re-aimed at the original
// Gfx_LoadImage address, which is ours by now, so both sides upload alike.

namespace {

using Fn = void (__cdecl*)(const void*, const short*);

std::uint32_t g_rng = 0x1F2E3D4Cu;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
int RngIn(int lo, int hi) { return lo + static_cast<int>(Rng() % static_cast<unsigned>(hi - lo + 1)); }

// A valid LZSS stream of at least `out` bytes: a match is only emitted if every
// window byte it reads is defined when it is read - one of the 0x1EE zeroes, or
// written earlier, by this very match included (the overlapping case).
unsigned g_matches, g_overlapping, g_literals;
void WriteLzss(std::uint8_t* at, int out) {
    bool defined[0x200];
    for (unsigned i = 0; i < 0x200; ++i) defined[i] = i < 0x1EE;
    unsigned r = 0x1EE;
    while (out > 0) {
        std::uint8_t* flags = at++;
        *flags = 0;
        for (int bit = 0; bit < 8; ++bit) {
            const unsigned len = 3 + Rng() % 16;
            // aim a third of the matches just behind the write position
            const unsigned offset = Rng() % 3 == 0 ? (r - 1 - Rng() % 8) & 0x1FF : Rng() & 0x1FF;
            bool ok = Rng() % 3 == 0;
            bool trial[0x200];
            std::memcpy(trial, defined, sizeof trial);
            bool overlap = false;
            for (unsigned k = 0, w = r; ok && k < len; ++k, w = (w + 1) & 0x1FF) {
                const unsigned from = (offset + k) & 0x1FF;
                ok = trial[from];
                overlap = overlap || (!defined[from] && trial[from]) || ((from - r) & 0x1FF) < k;
                trial[w] = true;
            }
            if (ok) {
                std::memcpy(defined, trial, sizeof trial);
                r = (r + len) & 0x1FF;
                *at++ = static_cast<std::uint8_t>(offset);
                *at++ = static_cast<std::uint8_t>((offset >> 4 & 0xF0) | (len - 3));
                out -= static_cast<int>(len);
                ++g_matches;
                g_overlapping += overlap;
            } else {
                *flags |= static_cast<std::uint8_t>(1u << bit);
                *at++ = static_cast<std::uint8_t>(Rng());
                defined[r] = true;
                r = (r + 1) & 0x1FF;
                out -= 1;
                ++g_literals;
            }
        }
    }
}

constexpr unsigned kShadowBytes = 64 * 0x800, kScratchBytes = 0x4000;
std::uint8_t g_packed[0x4000];
std::uint8_t g_shadow_theirs[kShadowBytes], g_scratch_theirs[kScratchBytes];

unsigned Fuzz(const char* name, Fn theirs, Fn ours, bool lzss) {
    constexpr unsigned kRounds = 2000;
    auto* shadow = reinterpret_cast<std::uint8_t*>(Gfx_VramShadow);
    unsigned bad = 0, negative = 0, both_negative = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& b : g_packed) b = static_cast<std::uint8_t>(Rng());
        int w = RngIn(0, 40), h = RngIn(0, 40);
        if (Rng() % 10 == 0) { w = -RngIn(1, 20); h = -RngIn(1, 20); ++both_negative; }   // a positive total all the same
        else if (!lzss && Rng() % 10 == 0) { h = -RngIn(1, 40); ++negative; }              // a negative one; it would hang the LZSS
        if (lzss) WriteLzss(g_packed + 4, w * h * 2 + 18);
        const short rect[4] = {static_cast<short>(RngIn(40, 900)), static_cast<short>(RngIn(20, 23)),
                               static_cast<short>(w), static_cast<short>(h)};
        unsigned char* next_theirs = nullptr;
        for (int pass = 0; pass < 2; ++pass) {
            std::memset(shadow, 0, kShadowBytes);
            std::memset(Gfx_UnpackScratch, 0, kScratchBytes);
            Gfx_UnpackNext = Gfx_UnpackScratch + 0x1000 + (round & 3);   // room to move back; unaligned too
            (pass ? ours : theirs)(g_packed, rect);
            if (pass == 0) {
                std::memcpy(g_shadow_theirs, shadow, kShadowBytes);
                std::memcpy(g_scratch_theirs, Gfx_UnpackScratch, kScratchBytes);
                next_theirs = Gfx_UnpackNext;
            }
        }
        if ((std::memcmp(g_shadow_theirs, shadow, kShadowBytes) != 0 ||
             std::memcmp(g_scratch_theirs, Gfx_UnpackScratch, kScratchBytes) != 0 || next_theirs != Gfx_UnpackNext) &&
            ++bad <= 8)
            bof3::Log("shadow      %s self-test MISMATCH round %u: w %d h %d, next %p vs ours %p", name, round, w, h,
                      (void*)next_theirs, (void*)Gfx_UnpackNext);
    }
    std::memset(shadow, 0, kShadowBytes);
    std::memset(Gfx_UnpackScratch, 0, kScratchBytes);
    Gfx_UnpackNext = nullptr;
    bof3::Log("shadow      %s self-test: %u rounds (%u with both of w, h negative, %u with a negative total), "
              "%u MISMATCHES; shadow rows, unpack scratch and Gfx_UnpackNext compared",
              name, kRounds, both_negative, negative, bad);
    return bad;
}

}  // namespace

void GfxUnpack_Inject() {
    if (bof3::WantsShadow("gfx_unpack")) {
        // Disasm 2026-09-19: every jump in both is internal; each has one
        // relative call, to Gfx_LoadImage.
        const bof3::CloneCall packed5_calls[] = {{0x82, nullptr}};
        auto* packed5 = bof3::CloneOriginal("Gfx_UploadPacked5", bof3::addr::Gfx_UploadPacked5, 0xAE, packed5_calls, 1);
        const bof3::CloneCall lzss_calls[] = {{0x119, nullptr}};
        auto* lzss = bof3::CloneOriginal("Gfx_UploadLzss", bof3::addr::Gfx_UploadLzss, 0x148, lzss_calls, 1);
        unsigned bad = Fuzz("Gfx_UploadPacked5", reinterpret_cast<Fn>(packed5), &Gfx_UploadPacked5, false);
        bad += Fuzz("Gfx_UploadLzss", reinterpret_cast<Fn>(lzss), &Gfx_UploadLzss, true);
        bof3::Log("shadow      Gfx_UploadLzss streams held %u literals and %u matches, %u of them overlapping "
                  "their own output", g_literals, g_matches, g_overlapping);
        if (bad) bof3::Fatal("gfx_unpack differs from the original in %u self-test rounds", bad);
    }
}

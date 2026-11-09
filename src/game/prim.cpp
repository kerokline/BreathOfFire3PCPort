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

// original 0x462A70. One level to bytes +4, +5 and +6 - where a PSX GPU
// primitive keeps r0, g0, b0 - leaving +7, the primitive's code, alone.
extern "C" void __cdecl Prim_SetShade(unsigned char* prim, unsigned char level) {
    prim[4] = level;
    prim[5] = level;
    prim[6] = level;
}

// original 0x572A00. Texture a run of `count` POLY_FT4s of 0x48 bytes from one
// packed word - the texture of a cell of the field view's map, and of much
// else (91 call sites). The word, bit by bit:
//
//   0..11   where the four (u, v) come from - below
//   12, 13  the semi-transparency mode, into the tpage
//   15      semi-transparency on (Gpu_SetSemiTrans)
//   16      mirror left to right;  17  top to bottom;  18  a quarter turn
//   19..23  the shade: r = g = b = (word >> 16) & 0xF8
//   24..27  the palette: a 4-bit texture's CLUT column, an 8-bit one's row
//   28, 29  the texture page, x = 320 + 128n, y = 256
//   31      set: a 4-bit texture; clear: 8-bit
//
// The corners, in the POLY_FT4's order (top left, top right, bottom left,
// bottom right):
//   - bits 8..11 clear: a 16-texel cell of a 16 x 16 grid, bits 0..3 its
//     column and 4..7 its row, the corners 15 apart;
//   - bit 11 set: two dwords of the area block (AreaMap_Header) at dword
//     u16[+6 + 4n] plus twice bits 0..10 - the four corners a byte each;
//   - otherwise: one dword at dword u16[+4 + 4n] plus bits 0..7 - the top
//     left's u and v, then a width and a height.
// The original takes each dword's top byte signed. Only a coordinate's low
// byte reaches the primitive, and that is the same either way, width added
// or not; the fuzz cannot see it (a control that drops the sign passes), so
// ours keeps the sign only to read like the original.
//
// The original goes through x87: each coordinate an integer to a float, the
// rectangle's far edges one float add, each result back through _ftol. Every
// such value is an integer of magnitude under 400, so every step is exact at
// any precision and in any rounding mode, and _ftol truncates: integers are
// the same arithmetic. The fuzz runs theirs under four control words to hold
// that claim to account. Its locals also hold four floats it never writes,
// swapped along with the corners by the turn and the mirrors: stale stack,
// and none of it reaches the primitive.
//
// Kept as the original has it: a count of 0 or less draws nothing; the turn
// comes first, then the mirrors.
extern "C" void __cdecl Prim_SetTexture(unsigned long texture, unsigned char* prim, int count) {
    const auto word = static_cast<std::uint32_t>(texture);
    const unsigned page = (word >> 28) & 3;
    const bool eight_bit = (word & 0x80000000u) == 0;
    const unsigned char* area = AreaMap_Header;
    auto u16_at = [area](unsigned at) { std::uint16_t x; std::memcpy(&x, area + at, 2); return unsigned{x}; };
    auto u32_at = [area](unsigned at) { std::uint32_t x; std::memcpy(&x, area + at, 4); return x; };
    int u[4], v[4];
    if ((word & 0xF00) == 0) {
        u[0] = u[2] = static_cast<int>((word & 0xF) << 4);
        v[0] = v[1] = static_cast<int>(word & 0xF0);
        u[1] = u[3] = u[0] + 15;
        v[2] = v[3] = v[0] + 15;
    } else if (word & 0x800) {
        const unsigned at = (u16_at(6 + 4 * page) + (word & 0x7FF) * 2) * 4;
        const std::uint32_t first = u32_at(at), second = u32_at(at + 4);
        u[0] = static_cast<std::int32_t>(first) >> 24;
        v[0] = static_cast<int>((first >> 16) & 0xFF);
        u[1] = static_cast<int>((first >> 8) & 0xFF);
        v[1] = static_cast<int>(first & 0xFF);
        u[2] = static_cast<std::int32_t>(second) >> 24;
        v[2] = static_cast<int>((second >> 16) & 0xFF);
        u[3] = static_cast<int>((second >> 8) & 0xFF);
        v[3] = static_cast<int>(second & 0xFF);
    } else {
        const std::uint32_t cell = u32_at((u16_at(4 + 4 * page) + (word & 0xFF)) * 4);
        u[0] = u[2] = static_cast<std::int32_t>(cell) >> 24;
        v[0] = v[1] = static_cast<int>((cell >> 16) & 0xFF);
        u[1] = u[3] = u[0] + static_cast<int>((cell >> 8) & 0xFF);
        v[2] = v[3] = v[0] + static_cast<int>(cell & 0xFF);
    }
    if (word & 0x40000) {   // corners 0, 1, 2, 3 take old 2, 0, 3, 1
        const int ou[4] = {u[0], u[1], u[2], u[3]}, ov[4] = {v[0], v[1], v[2], v[3]};
        static const int kFrom[4] = {2, 0, 3, 1};
        for (int i = 0; i < 4; ++i) u[i] = ou[kFrom[i]], v[i] = ov[kFrom[i]];
    }
    auto swap = [&u, &v](int a, int b) { std::swap(u[a], u[b]); std::swap(v[a], v[b]); };
    if (word & 0x10000) swap(0, 1), swap(2, 3);
    if (word & 0x20000) swap(0, 2), swap(1, 3);

    const auto shade = static_cast<unsigned char>((word >> 16) & 0xF8);
    const unsigned palette = (word >> 24) & 0xF;
    // getClut(x, y) is y << 6 | x >> 4: an 8-bit texture's palette is row
    // 0x1E3 + n at x 0, a 4-bit one's is x 16n of row 0x1E3.
    const auto clut = static_cast<std::uint16_t>(eight_bit ? 0x78C0 + (palette << 6) : 0x78C0 | palette);
    const auto tpage = static_cast<std::uint16_t>(
        ((((word & 0x3000) | 0x800) >> 1 | (((page << 7) + 0x140) & 0x3C0)) >> 6) | (eight_bit ? 0x80u : 0u));
    const unsigned semi_trans = (word >> 15) & 1;
    for (; count > 0; --count, prim += kTexPrimBytes) {
        Gpu_SetSemiTrans(prim, semi_trans);
        prim[4] = prim[5] = prim[6] = shade;
        std::memcpy(prim + 0x16, &clut, 2);
        std::memcpy(prim + 0x26, &tpage, 2);
        for (int i = 0; i < 4; ++i) {
            prim[0x14 + 0x10 * i] = static_cast<unsigned char>(u[i]);
            prim[0x15 + 0x10 * i] = static_cast<unsigned char>(v[i]);
        }
    }
}

void Prim_Inject() {
    // 0x462A70..0x462A81: no calls, no jumps (disasm 2026-09-20).
    if (bof3::WantsShadow("prim"))
        SelfTest(reinterpret_cast<SetShadeFn>(
            bof3::CloneOriginal("Prim_SetShade", bof3::addr::Prim_SetShade, 0x12)));
    BOF3_INJECT(Prim_SetShade);
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
    BOF3_INJECT(Prim_SetTexture);
}

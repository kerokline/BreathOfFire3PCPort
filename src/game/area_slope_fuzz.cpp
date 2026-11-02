// BOF3X_SHADOW=area_slope: a differential fuzz of AreaMap_Slope against a
// byte-copy of Capcom's, once at start-up (area_slope.cpp; docs/kind2-object.md
// section 7). Its own TU since the 64-bit build round (docs/x64-asm-policy.md
// 8 unit 8): CallFramed lays an i686 stack out for the call, and every
// *_fuzz.cpp is left out of the 64-bit build (CMakeLists.txt).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_slope {

using SlopeFn = long (__cdecl*)(long, long, unsigned long);

namespace {

using namespace move_script;

// --- BOF3X_SHADOW=area_slope: a differential fuzz, once at start-up ---------
// No calls, every jump internal: a byte-copy runs against the same globals.
// Both are called through CallFramed, which lays a 160-byte block out as the
// stack at the call - x, y, the direction dword, then 148 bytes of "caller's
// frame" - and copies it back, so that the bytes an unmasked corner index
// reads past the corners (the direction dword's own) are inputs, and the
// caller's frame a compared output. The three argument slots are not
// compared: the original scribbles on them (its scratch), ours reads them as
// parameters and leaves them (area_slope.cpp; the 64-bit build round, B2).
// One round: a point in or near a random grid, a direction byte 0..15 (16 and
// up reads the caller's stack in the original and is a Fatal in ours); the
// area window, height base and scale re-drawn every 256 rounds - random, or a
// constant fill (every cell flat) with a few bytes disturbed, so that the
// flat tests' ties are common. Theirs, then ours from the same state; eax,
// the block and the scratch bytes compared.

constexpr unsigned kBlock = 160;

// long CallFramed(const void* fn, unsigned char* block): calls fn with esp at
// a copy of block (so block[0..11] are its three arguments), then copies the
// stack back into block.
__attribute__((naked)) long __cdecl CallFramed(const void*, unsigned char*) {
    __asm__ volatile(
        "push %esi\n\t"
        "push %edi\n\t"
        "push %ebp\n\t"
        "mov %esp, %ebp\n\t"
        "sub $160, %esp\n\t"
        "cld\n\t"
        "mov 20(%ebp), %esi\n\t"
        "mov %esp, %edi\n\t"
        "mov $40, %ecx\n\t"
        "rep movsl\n\t"
        "call *16(%ebp)\n\t"
        "cld\n\t"
        "mov %esp, %esi\n\t"
        "mov 20(%ebp), %edi\n\t"
        "mov $40, %ecx\n\t"
        "rep movsl\n\t"
        "mov %ebp, %esp\n\t"
        "pop %ebp\n\t"
        "pop %edi\n\t"
        "pop %esi\n\t"
        "ret\n\t");
}

std::uint32_t g_rng = 0x5722D001u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }

constexpr unsigned kAreaWindow = 0x2000;   // as map_cells' fuzz: a grid of up to 40 x 40, all inside it

// Three kinds of area: random bytes; one byte everywhere (every cell flat);
// or every corner dword the same four bytes with two pairs equal and heights
// scaled to 0 - where one half's two edges are level (a == d, b == e) but not
// with each other (a != b), which is what tells the flat test's pairing apart.
// The last two with a few bytes disturbed.
void RandomArea() {
    const unsigned kind = Next() % 3;
    const auto fill = static_cast<unsigned char>(Next());
    unsigned char pattern[4];
    for (auto& p : pattern) p = static_cast<unsigned char>(Next());
    if (Next() % 2) { pattern[1] = pattern[0]; pattern[3] = pattern[2]; }   // level along x
    else { pattern[2] = pattern[0]; pattern[3] = pattern[1]; }              // level along y
    for (unsigned i = 0; i < kAreaWindow; ++i)
        AreaMap_Header[i] = kind == 0 ? static_cast<unsigned char>(Next()) : kind == 1 ? fill : pattern[(i - 0x30) & 3];
    if (kind != 0) {
        const unsigned disturbed = Next() % 64;
        for (unsigned i = 0; i < disturbed; ++i) AreaMap_Header[0x30 + Next() % (kAreaWindow - 0x30)] = static_cast<unsigned char>(Next());
    }
    AreaMap_Header[0] = static_cast<unsigned char>(Next() % 16 == 0 ? 0 : 1 + Next() % 40);
    AreaMap_Header[1] = static_cast<unsigned char>(Next() % 16 == 0 ? 0 : 1 + Next() % 40);
    AreaMap_HeightBase = static_cast<unsigned short>(Next() % 0x200);
    static const unsigned char kScale[] = {0, 1, 2, 0x7F, 0x80, 0xFF};
    MapView_HeightScale = static_cast<unsigned char>(kind == 2 ? 0 : Next() % 2 ? kScale[Next() % 6] : Next());
}
std::uint32_t RandomCoordinate(unsigned side) {
    std::uint32_t whole;
    if (Next() % 8 == 0) whole = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<short>(Next())));
    else whole = static_cast<std::uint32_t>(static_cast<std::int32_t>(Next() % (side + 4)) - 2);
    return (whole << 16) | (Next() & 0xFFFFu);
}

}  // namespace

void SelfTest(SlopeFn theirs) {
    constexpr unsigned kRounds = 131072;
    static unsigned char saved[kAreaWindow];
    std::memcpy(saved, AreaMap_Header, kAreaWindow);
    const unsigned char saved_scale = MapView_HeightScale;
    const unsigned short saved_base = AreaMap_HeightBase;
    unsigned char saved_scratch[4];
    std::memcpy(saved_scratch, Scratch(), 4);
    unsigned bad = 0, outside = 0, whole = 0, edge_flat = 0, edge = 0, four_flat = 0, four = 0, past = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        if (round % 256 == 0) RandomArea();
        alignas(4) unsigned char input[kBlock], their_block[kBlock], our_block[kBlock];
        for (auto& b : input) b = static_cast<unsigned char>(Next());
        const std::uint32_t x = RandomCoordinate(AreaMap_Header[0]), y = RandomCoordinate(AreaMap_Header[1]);
        std::memcpy(input, &x, 4);
        std::memcpy(input + 4, &y, 4);
        input[8] = static_cast<unsigned char>(Next() % 16);
        unsigned char scratch_in[4];
        for (auto& b : scratch_in) b = static_cast<unsigned char>(Next());

        std::memcpy(their_block, input, kBlock);
        std::memcpy(Scratch(), scratch_in, 4);
        const long their = CallFramed(reinterpret_cast<const void*>(theirs), their_block);
        unsigned char their_scratch[4];
        std::memcpy(their_scratch, Scratch(), 4);
        std::memcpy(our_block, input, kBlock);
        std::memcpy(Scratch(), scratch_in, 4);
        const long our = CallFramed(reinterpret_cast<const void*>(&AreaMap_Slope), our_block);

        const auto cx = static_cast<short>(x >> 16), cy = static_cast<short>(y >> 16);
        if (cx < 0 || cy < 0 || cx >= AreaMap_Header[0] || cy >= AreaMap_Header[1]) ++outside;
        else if (!(x & 0x8000) && !(y & 0x8000)) ++whole;
        else if (!(x & 0x8000) != !(y & 0x8000)) ++(their_scratch[0] ? edge : edge_flat);
        else if (their_scratch[0] == 0) ++four_flat;
        else {
            ++four;
            if (input[8] >= 8) ++past;
        }
        if ((their != our || std::memcmp(their_block + 12, our_block + 12, kBlock - 12) != 0 ||
             std::memcmp(their_scratch, Scratch(), 4) != 0) &&
            ++bad <= 8)
            bof3::Log("shadow      area_slope self-test MISMATCH: round %u, (%08X, %08X, %02X), %08lX / %08lX, flag %u / %u", round,
                      (unsigned)x, (unsigned)y, input[8], their, our, their_scratch[0], Scratch()[0]);
    }
    std::memcpy(AreaMap_Header, saved, kAreaWindow);
    MapView_HeightScale = saved_scale;
    AreaMap_HeightBase = saved_base;
    std::memcpy(Scratch(), saved_scratch, 4);
    bof3::Log("shadow      area_slope self-test: %u rounds (%u outside the grid; whole cells %u; one half %u flat / %u sloped; "
              "both halves %u flat / %u sloped, %u of them with a corner index past the four), %u MISMATCHES; eax, scratch "
              "bytes and the 148 bytes of caller's frame compared",
              kRounds, outside, whole, edge_flat, edge, four_flat, four, past, bad);
    if (bad) bof3::Fatal("AreaMap_Slope differs from the original in %u of %u self-test rounds", bad, kRounds);
}


}  // namespace area_slope

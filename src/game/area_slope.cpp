// The ground's slope under a point: original 0x5722D0. docs/kind2-object.md
// section 7.
#include "game/area_slope.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

int S8(unsigned char v) { return static_cast<signed char>(v); }
std::int32_t Abs(std::int32_t v) { return v < 0 ? -v : v; }   // never INT_MIN here: sums of a few bytes

}  // namespace

// original 0x5722D0: how steep the ground is at (x, y), 16.16, and whether it
// is sloped at all - scratch byte 0 (DamageScratch, the PSX scratchpad's first
// byte) is 1 for sloped, 0 for flat or outside the grid. Its three callers
// that were read (0x46B611, 0x518784, MoveCmd_Move 0x578CC3) test that byte
// straight after the call. The same grid and cell lookup as AreaMap_Elevation;
// per half-cell quadrant:
//   whole cell (bits 15 of x and y clear): flat, and the elevation formula;
//   one half: the two edge sums a + b and d + e of AreaMap_Elevation - flat
//     when b == e and a == d (then (b + a) << 4), else |(a + b) - (d + e)| << 4;
//   both halves: the four corners p0..p3 - flat when all four are equal (then
//     p0 << 5), else by the third argument's low byte n, i = n >> 1:
//     |p[i] - p[(i - 2) & 3]| << 5, or for odd n the larger of p[i] and
//     p[(i + 1) & 3] in place of p[i].
//
// As the original has it:
//  - It uses its caller's argument slots as scratch: x and y become
//    x & 0xFFFF8000 and y & 0xFFFF8000 inside the grid, and y's four bytes
//    then hold b, d, e (from byte 1) or p0..p3. So the arguments are read and
//    written where the caller pushed them (through the frame address), never
//    as copies.
//  - p[i] is not masked: an n of 8 or more reads past the four corners - the
//    third argument's own bytes for 8..15, the caller's stack from 16. Kept.
//  - Outside the grid the return is AreaMap_Header's top half with ax 0, as
//    AreaMap_Elevation's.
//  - Heights are the low byte of height x MapView_HeightScale (imul r/m8), and
//    corner sums wrap at 8 bits before they are compared or widened.
extern "C" long __cdecl AreaMap_Slope(long x, long y, unsigned long direction) {
    (void)x;
    (void)y;
    (void)direction;
    // [ebp + 8]: x, y, direction, then the caller's frame. The named
    // parameters are never used, so no copy of them can go stale.
    volatile unsigned char* const args = static_cast<volatile unsigned char*>(__builtin_frame_address(0)) + 8;
    const auto get32 = [args](unsigned at) {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(args[at + i]) << (8 * i);
        return v;
    };
    const auto put32 = [args](unsigned at, std::uint32_t v) {
        for (unsigned i = 0; i < 4; ++i) args[at + i] = static_cast<unsigned char>(v >> (8 * i));
    };
    std::uint32_t header;
    std::memcpy(&header, AreaMap_Header, 4);
    const std::uint32_t ax = get32(0), ay = get32(4);
    const auto cx = static_cast<short>(ax >> 16), cy = static_cast<short>(ay >> 16);
    const unsigned width = header & 0xFFu;
    if (cy >= static_cast<short>((header >> 8) & 0xFFu) || cx >= static_cast<short>(width) || cy < 0 || cx < 0) {
        Scratch()[0] = 0;
        return static_cast<long>(header & 0xFFFF0000u);
    }
    const std::uint32_t xm = ax & 0xFFFF8000u, ym = ay & 0xFFFF8000u;
    put32(4, ym);
    put32(0, xm);
    const int cell = cx + cy * static_cast<int>(width);
    const unsigned char* const h = AreaMap_Header + static_cast<unsigned>(AreaMap_HeightBase) * 4u + cell;
    const unsigned char* const c = reinterpret_cast<const unsigned char*>(&AreaMap_Corners) + cell * 4;
    const unsigned scale = MapView_HeightScale;
    if (((xm | ym) & 0xFFFFu) == 0) {   // the whole cell: flat
        Scratch()[0] = 0;
        return static_cast<long>((static_cast<std::uint32_t>(S8(c[2]) + S8(h[0]) * static_cast<int>(scale) * 2 + S8(c[1]))) << 4);
    }
    const auto height = [&](unsigned at) { return static_cast<unsigned char>(h[at] * scale); };
    if (((xm ^ ym) & 0xFFFFu) != 0) {   // one half: the two edges
        const unsigned char here = height(0);
        unsigned char a, b, d, e;
        if (xm & 0x8000u) {
            a = static_cast<unsigned char>(c[1] + here);
            b = static_cast<unsigned char>(c[3] + here);
            const unsigned char next = height(1);
            d = static_cast<unsigned char>(c[4] + next);
            e = static_cast<unsigned char>(c[6] + next);
        } else {
            a = static_cast<unsigned char>(c[2] + here);
            b = static_cast<unsigned char>(c[3] + here);
            const unsigned char next = height(width);
            d = static_cast<unsigned char>(c[4 * width] + next);
            e = static_cast<unsigned char>(c[4 * width + 1] + next);
        }
        args[5] = b;
        args[6] = d;
        args[7] = e;
        if (b == e && a == d) {
            Scratch()[0] = 0;
            return static_cast<long>(static_cast<std::uint32_t>(S8(b) + S8(a)) << 4);
        }
        Scratch()[0] = 1;
        return static_cast<long>(static_cast<std::uint32_t>(Abs(S8(b) - S8(e) - S8(d) + S8(a))) << 4);
    }
    // Both halves: the four corners, in y's slot.
    args[4] = static_cast<unsigned char>(c[3] + height(0));
    args[5] = static_cast<unsigned char>(c[6] + height(1));
    args[6] = static_cast<unsigned char>(c[4 * width + 4] + height(width + 1));
    args[7] = static_cast<unsigned char>(c[4 * width + 1] + height(width));
    const unsigned char p0 = args[4];
    if (p0 == args[7] && p0 == args[6] && p0 == args[5]) {
        Scratch()[0] = 0;
        return static_cast<long>(static_cast<std::uint32_t>(S8(p0)) << 5);
    }
    Scratch()[0] = 1;
    const unsigned n = get32(8) & 0xFFu;
    const unsigned i = n >> 1;
    int high = S8(args[4 + i]);   // unmasked, as the original's
    if (n & 1) {
        const int next = S8(args[4 + ((i + 1) & 3)]);
        if (high < next) high = next;
    }
    return static_cast<long>(static_cast<std::uint32_t>(Abs(high - S8(args[4 + ((i - 2) & 3)]))) << 5);
}

namespace {

// --- BOF3X_SHADOW=area_slope: a differential fuzz, once at start-up ---------
// No calls, every jump internal: a byte-copy runs against the same globals.
// Both are called through CallFramed, which lays a 160-byte block out as the
// stack at the call - x, y, the direction dword, then 148 bytes of "caller's
// frame" - and copies it back, so that the argument slots the function
// scribbles on and the bytes an unmasked corner index reads past them are
// both inputs and compared outputs. One round: a point in or near a random
// grid, a direction byte (0..15 mostly, any byte one round in eight); the
// area window, height base and scale re-drawn every 256 rounds - random, or a
// constant fill (every cell flat) with a few bytes disturbed, so that the
// flat tests' ties are common. Theirs, then ours from the same state; eax,
// the block and the scratch bytes compared.

constexpr unsigned kBlock = 160;
using SlopeFn = long (__cdecl*)(long, long, unsigned long);

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
        input[8] = static_cast<unsigned char>(Next() % 8 ? Next() % 16 : Next());
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
        if ((their != our || std::memcmp(their_block, our_block, kBlock) != 0 || std::memcmp(their_scratch, Scratch(), 4) != 0) &&
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
              "bytes and the 160 bytes of argument slots and caller's frame compared",
              kRounds, outside, whole, edge_flat, edge, four_flat, four, past, bad);
    if (bad) bof3::Fatal("AreaMap_Slope differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void AreaSlope_Inject() {
    // 0x5722D0..0x57256F: no calls, every jump internal (capstone, 2026-09-22).
    if (bof3::WantsShadow("area_slope"))
        SelfTest(reinterpret_cast<SlopeFn>(bof3::CloneOriginal("AreaMap_Slope", bof3::addr::AreaMap_Slope, 0x2A0)));
    BOF3_INJECT(AreaMap_Slope);
}

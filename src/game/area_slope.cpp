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

// BOF3X_SHADOW=area_slope: the start-up fuzz, area_slope_fuzz.cpp (an i686
// TU, as every *_fuzz.cpp: its framed call lays out an i686 stack).
namespace area_slope {
using SlopeFn = long (__cdecl*)(long, long, unsigned long);
void SelfTest(SlopeFn theirs);
}  // namespace area_slope

// original 0x5722D0: how steep the ground is at (x, y), 16.16, and whether it
// is sloped at all - scratch byte 0 (SharedScratch, the PSX scratchpad's first
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

void AreaSlope_Inject() {
    // 0x5722D0..0x57256F: no calls, every jump internal (capstone, 2026-09-22).
    if (bof3::WantsShadow("area_slope"))
        area_slope::SelfTest(reinterpret_cast<area_slope::SlopeFn>(bof3::CloneOriginal("AreaMap_Slope", bof3::addr::AreaMap_Slope, 0x2A0)));
    BOF3_INJECT(AreaMap_Slope);
}

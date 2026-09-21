#include "game/gfx_sprite_uv.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// DIV-0010: a sprite's far texture edge is one texel short.
//
// The three Direct3D sprite handlers - D3d_DrawSprt 0x5A2300, D3d_DrawSprt8
// 0x5A2520, D3d_DrawSprt16 0x5A2710 - read both texture edges of their quad
// from one float table, 0x7CA9E0: tc[i] = (i + 0.512) / 256, 256 entries,
// built by 0x5A5160. The near edge is tc[u]. The far edge is tc[u + w - 1]
// (operand 0x7CA9DC, index u + w), tc[u + 7] (0x7CA9FC) and tc[u + 15]
// (0x7CAA1C) - while the quad is the full w, 8 or 16 pixels wide. So w - 1
// texels are stretched over w pixels, and at the 2x scale the last row and
// column of every sprite get one screen pixel instead of two: the clipped
// numerals of known-defects.md D1.
//
// This is NOT a reimplementation. The handlers end in COM calls and nothing in
// the project can check a drawn surface yet (HANDOFF "The Direct3D end"), so
// what runs is the original's own code, byte for byte, with the two far-edge
// operands in each copy re-aimed at a table of ours. The handlers are
// straight-line code - no jump at all, six relative calls each, all re-aimed
// at where the original called.
//
// What the far edge has to be. The port draws with bilinear filtering (seen:
// the title logo, smooth at 2x), and its sprites are cells of shared texture
// pages, so a sample past the centre of the last texel blends in a
// neighbouring cell. That is what the original's numbers are for: near edge
// at the centre of texel u, far edge at the centre of texel u + w - 1. The
// slip is WHERE the far value is reached: a vertex value is reached at the
// vertex, one pixel past the last one drawn, so the last pixel samples only
// u + 7.07 of 7.5 for an 8 pixel sprite at 2x - the last texel gets 0.7 of a
// screen row where the first gets 1.6. (First try, 2026-09-20: far edge
// u + w with a small inset. It drew seams through the title logo - the
// neighbour blending in. Kept here because it is the obvious fix and wrong.)
//
// Ours: the last PIXEL samples the centre of the last texel. With the
// rasteriser's pixel centres on whole numbers (the Direct3D 6 rule, and what
// the original's near edge assumes), that is a far vertex value of
//
//   near + (w - 1) * n / (n - 1),   n = w * scale screen pixels
//
// which for scale 2 is u + w - 0.5 + (w - 1) / (2w - 1) + 0.012: the last term
// runs from 0.4667 at w = 8 to 0.5 for a wide sprite. One table cannot hold a
// function of u and w both, so ours uses the w = 8 value throughout,
// ours[j] = (j - 1/30 + 0.012) / 256 indexed by j = u + w: exact for the 8
// pixel font, short of the last texel's centre by under 0.03 of a texel for
// anything wider (never past it, so never a seam), and past it by 0.03 for a
// sprite 4 wide. The scale is 2.0 in every mode seen (0x7C9F4C / 0x7C9F48);
// at another scale this is still between the original and exact.
//
// The near edge is the original's, untouched, as is the table at 0x7CA9E0.
//
// An edge the original never defined: its table has 256 entries and SPRT
// indexes it with u + w - 1, a 16-bit w, unchecked - past 255 it reads
// whatever follows. Ours is linear to 1,024 and the index is not checked
// either.

namespace {

constexpr std::uint32_t kTableFarSprt = 0x7CA9DC;   // tc[(u + w) - 1]
constexpr std::uint32_t kTableFar8 = 0x7CA9FC;      // tc[u + 7]
constexpr std::uint32_t kTableFar16 = 0x7CAA1C;     // tc[u + 15]

constexpr int kEntries = 1024;
constexpr float kFarInset = 0.012f - 1.0f / 30.0f;
float g_tc[kEntries];

struct Handler {
    const char* name;
    std::uint32_t original, size;
    std::uint32_t calls[6];         // offsets of the E8 bytes
    std::uint32_t far_disp[2];      // offsets of the disp32 of the far edge's `fld [reg*4 + table]`
    std::uint32_t far_operand;      // the table address the original's far operand holds
    int far_extent;                 // ours + this is what replaces it: 0 where the index is u + w, else w
};

// Offsets from the disassembly of 2026-09-20 (capstone, entry to ret; no
// jump in any of the three).
constexpr Handler kHandlers[] = {
    {"D3d_DrawSprt", bof3::addr::D3d_DrawSprt, 0x211,
     {0x38, 0x1BB, 0x1C2, 0x1C9, 0x1E1, 0x1E8}, {0xD2, 0x130}, kTableFarSprt, 0},
    {"D3d_DrawSprt8", bof3::addr::D3d_DrawSprt8, 0x1EB,
     {0x35, 0x198, 0x19F, 0x1A6, 0x1BE, 0x1C5}, {0xBD, 0x11D}, kTableFar8, 8},
    {"D3d_DrawSprt16", bof3::addr::D3d_DrawSprt16, 0x1EB,
     {0x35, 0x198, 0x19F, 0x1A6, 0x1BE, 0x1C5}, {0xBD, 0x11D}, kTableFar16, 16},
};

void Reaim(const Handler& h, std::uint8_t* code, std::uint32_t at, std::uint32_t expected, const float* to) {
    std::uint32_t disp;
    std::memcpy(&disp, code + at, sizeof disp);
    // The three bytes before a disp32 here are D9 04 <sib>: fld dword [reg*4 + disp32].
    if (disp != expected || code[at - 3] != 0xD9 || code[at - 2] != 0x04)
        bof3::Fatal("%s: no `fld [reg*4 + 0x%X]` operand at +0x%X (found 0x%X)", h.name,
                    (unsigned)expected, (unsigned)at, (unsigned)disp);
    disp = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(to));
    std::memcpy(code + at, &disp, sizeof disp);
}

}  // namespace

void GfxSpriteUv_Inject() {
    for (int i = 0; i < kEntries; ++i) g_tc[i] = (static_cast<float>(i) + kFarInset) / 256.0f;
    for (const Handler& h : kHandlers) {
        bof3::CloneCall calls[6];
        for (int i = 0; i < 6; ++i) calls[i] = {h.calls[i], nullptr};
        auto* code = static_cast<std::uint8_t*>(bof3::CloneOriginal(h.name, h.original, h.size, calls, 6));
        // The original's index is u + w for SPRT (its operand is one entry
        // back) and u for the fixed sizes (its operand is w - 1 entries on);
        // ours is ours[u + w] for all three.
        for (std::uint32_t at : h.far_disp) Reaim(h, code, at, h.far_operand, g_tc + h.far_extent);
        FlushInstructionCache(GetCurrentProcess(), code, h.size);
        bof3::Inject(h.name, h.original, code);
    }
    bof3::Log("DIV-0010    sprite far texture edge: last pixel on the last texel's centre, table %p", static_cast<void*>(g_tc));
}

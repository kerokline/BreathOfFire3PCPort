// The three Direct3D sprite handlers, read to the last instruction with
// capstone against bof3/BOF3.exe (docs/sprt-draw.md). All are the PC port's
// own: the PlayStation drew SPRT, SPRT_8 and SPRT_16 on its GPU, so there is
// no PSX twin to read them against.
//
//   D3d_DrawSprt    0x5A2300..0x5A2510 (0x211)  code 0x64, SPRT
//   D3d_DrawSprt8   0x5A2520..0x5A270A (0x1EB)  code 0x74, SPRT_8
//   D3d_DrawSprt16  0x5A2710..0x5A28FA (0x1EB)  code 0x7C, SPRT_16
//
// Faithful but for DIVERGENCE DIV-0010, the far texture edge. The arithmetic
// is x87 in the original and x87 here, instruction for instruction (the X87*
// helpers below), as in d3d_draw.cpp: each fadd, fmul and fdiv rounds to the
// control word's precision and a float that passes through fld / fstp comes
// out with a signalling NaN quieted.
//
// DIV-0010: a sprite's far texture edge is one texel short.
//
// The handlers read both texture edges of their quad from one float table,
// D3d_TexCoords 0x7CA9E0: tc[i] = (i + 0.512) / 256, 256 entries, built by
// 0x5A5160. The near edge is tc[u]. The far edge is tc[u + w - 1] (operand
// 0x7CA9DC, index u + w), tc[u + 7] (0x7CA9FC) and tc[u + 15] (0x7CAA1C) -
// while the quad is the full w, 8 or 16 pixels wide. So w - 1 texels are
// stretched over w pixels, and at the 2x scale the last row and column of
// every sprite get one screen pixel instead of two: the clipped numerals of
// known-defects.md D1.
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
// g_far[j] = (j - 1/30 + 0.012) / 256 indexed by j = u + w: exact for the 8
// pixel font, short of the last texel's centre by under 0.03 of a texel for
// anything wider (never past it, so never a seam), and past it by 0.03 for a
// sprite 4 wide. The scale is 2.0 in every mode seen (0x7C9F4C / 0x7C9F48);
// at another scale this is still between the original and exact.
//
// The near edge is the original's, untouched, as is the table at 0x7CA9E0.
//
// The switch. The handlers read the far edge at g_far_base + 4j. That base is
// Capcom's 0x7CA9DC until SprtDraw_Inject patches it to g_far through
// PatchBytes under the name "SpriteFarEdge" - so BOF3X_ORIGINAL=SpriteFarEdge
// runs our three with Capcom's texture edges, and BOF3X_ORIGINAL=D3d_DrawSprt
// (or 8, 16) runs Capcom's handler itself, as it always has.
//
// An edge the original never defined: its table has 256 entries and SPRT
// indexes it with u + w - 1, a 16-bit w, unchecked - past 255 it reads
// whatever follows (docs/known-defects.md D37, kept). Until 2026-09-23 ours
// was a table of 1,024 read the same way, unchecked, so past 1,024 it read our
// own dll's memory; g_far now has an entry for every index u + w can make.
#include "game/sprt_draw.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/sprt_draw_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sprt_draw {


float g_far[kFarEntries];
U g_far_base = ::bof3::Img(kCapcomFarBase);   // a place: the handlers read the edge there

// Exactly the expression of the re-aimed copies that carried DIV-0010 from
// 2026-09-20 (src/game/gfx_sprite_uv.cpp, now gone): (float(j) + inset) / 256
// in float. The sum is exact before its one rounding under any precision
// (j has 17 bits, the inset 24 and an exponent of -6), and / 256 is exact, so
// the table does not depend on the x87 control word or on SSE against x87.
//
// Pinned, because the fuzz cannot see the table's values (ours and the
// re-aimed copies read the same one): the inset's bits, and an FNV-1a of the
// first 1,024 entries' bytes - the whole of the table the copies had - both
// computed independently in numpy float32 on 2026-09-23.
void FillFarTable() {
    constexpr float kFarInset = 0.012f - 1.0f / 30.0f;
    static_assert(std::bit_cast<U>(kFarInset) == 0xBCAEC33Fu, "DIV-0010's inset is not the one of 2026-09-20");
    for (U j = 0; j < kFarEntries; ++j) g_far[j] = (static_cast<float>(j) + kFarInset) / 256.0f;
    U hash = 0x811C9DC5u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(g_far);
    for (U i = 0; i < 1024 * 4; ++i) hash = (hash ^ bytes[i]) * 0x01000193u;
    if (hash != 0xF06E2C4Bu) bof3::Fatal("DIV-0010: the far-edge table is not the one of 2026-09-20 (FNV %08X)", hash);
}

// The same derivation at scale k (display-overhaul.md 4b): for the 8 pixel
// sprite the far vertex value is j - 0.488 + 7 / (8k - 1), an inset of
// 0.012 - (8k - 15) / (2 (8k - 1)) - at k = 2 exactly the pinned 0.012 - 1/30,
// so k = 2 keeps FillFarTable's table and its checks. Wider sprites stay short
// of their last texel's centre by under 0.03 of a texel at every k, as at 2.
void SetFarTableScale(U k) {
    if (k == 2) return FillFarTable();
    const float inset = static_cast<float>(0.012 - (8.0 * k - 15.0) / (2.0 * (8.0 * k - 1.0)));
    for (U j = 0; j < kFarEntries; ++j) g_far[j] = (static_cast<float>(j) + inset) / 256.0f;
    bof3::Log("DIV-0010    far texture edge for scale %u: inset %.6f", k, static_cast<double>(inset));
}

namespace {

// --- the x87 sequences, as the original executes them ------------------------

}  // namespace
}  // namespace sprt_draw

void SprtDraw_SetScale(unsigned k) { sprt_draw::SetFarTableScale(k); }

void SprtDraw_Inject() {
    using namespace sprt_draw;
    // The table first: the fuzz's DIV-0010 half reads it.
    FillFarTable();
    // The copies are of Capcom's own bytes: nothing patches the three but
    // BOF3_INJECT below (their callers reach them through the draw's jump
    // table), so this module's place in inject_all.cpp does not matter.
    if (bof3::WantsShadow("sprt_draw")) SelfTest();

    // DIVERGENCE DIV-0010: the far texture edge from our table. Patched into
    // our own base so that it has a name of its own for BOF3X_ORIGINAL.
    const U was = ::bof3::Img(kCapcomFarBase);
    const U is = Low32(g_far);
    std::uint8_t was_bytes[4], is_bytes[4];
    std::memcpy(was_bytes, &was, 4);
    std::memcpy(is_bytes, &is, 4);
    bof3::PatchBytes("SpriteFarEdge", Low32(&g_far_base), was_bytes,
                     is_bytes, 4);
    bof3::Log("DIV-0010    our sprite handlers' far texture edge: %s", g_far_base == ::bof3::Img(kCapcomFarBase)
                                                                          ? "Capcom's, tc[u + w - 1]"
                                                                          : "the last pixel on the last texel's centre");
}

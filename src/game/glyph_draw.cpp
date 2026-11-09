// The glyph handler and its texture lookup, read to the last instruction with
// capstone against bof3/BOF3.exe (docs/glyph-draw.md sections 1-3). Both are
// the PC port's own: the PlayStation drew these as GPU primitives, so there is
// no PSX twin to read them against.
//
//   D3d_DrawGlyph      0x5A2900..0x5A2BB3 (0x2B4 bytes), straight-line
//   Font_GlyphTexture  0x5A2BC0..0x5A2C96 (0xD7 bytes)
//
// DIVERGENCE DIV-0025 lives in D3d_DrawGlyph: the texture coordinates sample
// texel centres, (2u + 0.5) / 32, where Capcom's sample texel edges, 2u / 32
// (docs/known-defects.md D17). The offset is g_texel_inset, 0 until
// GlyphDraw_Inject sets it through PatchBytes under the name
// "GlyphTexelCentres" - so BOF3X_ORIGINAL=GlyphTexelCentres runs our function
// with Capcom's arithmetic, and BOF3X_ORIGINAL=D3d_DrawGlyph runs Capcom's.
#include "game/glyph_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/glyph_draw_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace glyph_draw {

const Callees kOriginals = {
    D3d_PrimColor,
    Font_GlyphTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(kRetOnly.v))),
    D3d_SetBlend,
    D3d_SetShadeMode,
    Font_BuildGlyphTexture,
};
Callees g = kOriginals;

float g_texel_inset = 0.0f;

// DIV-0025, amended 2026-09-27: the inset in texture texels that puts a
// screen pixel's sample at its own centre. The quad is 12 game units times
// `scale` pixels wide and shows 24 texels (each PSX texel doubled), so a
// pixel is 2 / scale texels and half of one is 1 / scale: 0.5 at scale 2
// (the 2026-09-22 value, exact there and wrong elsewhere - at scale 4 every
// odd pixel sampled a texel edge and the quad's two triangles rounded it
// differently, the owner's crooked stroke bottoms). Plus 1/256 of a texel
// so that an odd scale, whose pixel centres also land on edges (scale 3:
// pixel 1 at texel 1.0), samples the same side in both triangles. The
// fuzz's SameWithInset recomputes exactly this.
double TexelInset(double scale) {
    if (!(scale > 0.0)) scale = scale < 0.0 ? -scale : 1.0;   // a negative or NaN scale still draws
    return 1.0 / scale + 1.0 / 256.0;
}

}  // namespace glyph_draw

void GlyphDraw_Inject() {
    if (bof3::WantsShadow("glyph_draw")) glyph_draw::SelfTest();

    // DIVERGENCE DIV-0025: glyphs sample texel centres. Patched into our own
    // constant so that it has a name of its own for BOF3X_ORIGINAL.
    static const float was = 0.0f, is = 0.5f;
    std::uint8_t was_bytes[4], is_bytes[4];
    std::memcpy(was_bytes, &was, 4);
    std::memcpy(is_bytes, &is, 4);
    bof3::PatchBytes("GlyphTexelCentres",
                     Low32(&glyph_draw::g_texel_inset),
                     was_bytes, is_bytes, 4);
    bof3::Log("DIV-0025    glyph texels: (2u + 1/scale + 1/256) / 32, %s (0.5 at every scale until 2026-09-27)",
              glyph_draw::g_texel_inset != 0.0f ? "on" : "off");
}

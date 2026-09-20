#include "game/gfx_image.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"

// original 0x59EA70. The PC counterpart of the PSX LoadImage(RECT*, u_long*):
// copies h rows of w 16-bit cells into the 1024 x 512 shadow of PSX VRAM, then
// throws away every cached texture built from the cells it overwrote.
//
// As the original has it:
//   - only the upper bounds are checked. A negative x or y passes and writes
//     before the rectangle it names; a negative w reaches the copy as a huge
//     length. Nothing in the shipped data does either.
//   - a rect that fails the check is dropped whole, with no invalidation.
//   - h is read from the rect again on every row, and a rect with h <= 0 that
//     passes the check copies nothing but still invalidates.
extern "C" void __cdecl Gfx_LoadImage(const short* rect, const void* pixels) {
    const int x = rect[0], w = rect[2];
    if (x + w > 0x400) return;
    const int y = rect[1];
    if (y + rect[3] > 0x200) return;

    auto* dst = reinterpret_cast<std::uint8_t*>(Gfx_VramShadow) + ((y << 10) + x) * 2;
    auto* src = static_cast<const std::uint8_t*>(pixels);
    const std::uint32_t row = static_cast<std::uint32_t>(w + w);
    for (int n = 0; n < rect[3]; ++n) {
        std::memcpy(dst, src, row);
        dst += 0x800;
        src += row;
    }

    Gfx_InvalidateTextures(rect, 0);
}

// original 0x5A6800. Takes ownership of a malloc'd glyph table - the port's
// Chinese font, 288 bytes a glyph - and frees the one it replaces. The size is
// passed by LoadDatFile and never read.
extern "C" void __cdecl Font_SetGlyphData(void* owned_glyphs, unsigned /*size_unused*/) {
    if (Font_GlyphData != nullptr) Crt_free(Font_GlyphData);
    Font_GlyphData = owned_glyphs;
}

void GfxImage_Inject() {
    BOF3_INJECT(Gfx_LoadImage);
    BOF3_INJECT(Font_SetGlyphData);
}

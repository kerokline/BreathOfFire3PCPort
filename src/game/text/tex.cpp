// Moved here by tools/move_funcs.py from tex_cells.cpp, tex_page.cpp (docs/source-layout-round.md).
#include "game/text/tex.h"

#include <cstdint>
#include <cstring>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/tex_cells_callees.h"
#include "game/tex_page.h"
#include "game/tex_page_callees.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "render/render_shim.h"

using namespace tex_cells;

using namespace tex_page;

namespace tex_cells {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Word(U address) { return Word(At(address)); }

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}

void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

}  // namespace

}  // namespace tex_cells

namespace tex_page {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

// Gfx_PackRgb 0x5AA79C, which Tex_Convert16 calls per texel with the three
// channels in eax, ebx, edx: each shifted right by the dword at 0x7DED64 /
// 0x7DED68 / 0x7DED6C (`shr reg, cl` - the count's low five bits) and masked
// by 0x7DED70 / 0x7DED74 / 0x7DED78, OR-ed. Nothing writes those six dwords
// while a conversion runs, so they are read once per call here.
struct Packer {
    U shift_r, shift_g, shift_b, mask_r, mask_g, mask_b;
    Packer()
        : shift_r(At(::bof3::Img(Gfx_PixelFormat_addr) + 4)[0] & 31u), shift_g(At(::bof3::Img(Gfx_PixelFormat_addr) + 8)[0] & 31u),
          shift_b(At(::bof3::Img(Gfx_PixelFormat_addr) + 0xC)[0] & 31u), mask_r(Gfx_PixelFormat[4]),
          mask_g(Gfx_PixelFormat[5]), mask_b(Gfx_PixelFormat[6]) {}
    U operator()(U r, U g, U b) const {
        return ((r >> shift_r) & mask_r) | ((g >> shift_g) & mask_g) | ((b >> shift_b) & mask_b);
    }
};

// The three converters share one shape, from the original's loops: the w x h
// block of VRAM at src (rows 0x800 bytes apart) is TILED over 256 x 256 texels
// of the destination - each source row repeated across until 256 texels are
// written, rows repeated down until 256 rows are - row by row at `pitch`.
// Every count is the original's: `across` and `total` start at 0x100 and lose
// w (or h) per tile and stop only at exactly 0; the texel loop steps 8 (4-bit),
// 4 (8-bit) or 1 texels and stops only at exactly 0. So w and h must divide
// 256, and w be a multiple of the step, or the original runs on through memory;
// ours does the same arithmetic and would do the same. The destination is 2
// bytes a texel, or 4 when Gfx_PixelFormat's byte +3 is 4 (read once, on entry).
template <int Bits> void ConvertIndexed(void* dst, const void* src, const void* clut, int w, int h, int pitch) {
    constexpr U kPerLong = 32 / Bits;
    constexpr U kMask = (1u << Bits) - 1;
    const bool wide = At(::bof3::Img(Gfx_PixelFormat_addr))[3] == 4;
    const auto* table = static_cast<const unsigned char*>(clut);
    auto* d = static_cast<unsigned char*>(dst);
    U total = 0x100;
    do {
        const auto* s = static_cast<const unsigned char*>(src);
        U rows = static_cast<U>(h);
        total -= static_cast<U>(h);
        do {
            unsigned char* const row = d;
            U across = 0x100;
            do {
                const unsigned char* p = s;
                U n = static_cast<U>(w);
                across -= static_cast<U>(w);
                do {
                    const U v = Long(p);
                    if (wide) {
                        for (U k = 0; k < kPerLong; ++k) PutLong(d + 4 * k, Long(table + 4 * ((v >> (Bits * k)) & kMask)));
                        d += 4 * kPerLong;
                    } else {
                        for (U k = 0; k < kPerLong; ++k) PutWord(d + 2 * k, Word(table + 2 * ((v >> (Bits * k)) & kMask)));
                        d += 2 * kPerLong;
                    }
                    p += 4;
                    n -= kPerLong;
                } while (n != 0);
            } while (across != 0);
            d = row + pitch;
            s += 0x800;
        } while (--rows != 0);
    } while (total != 0);
}

}  // namespace

}  // namespace tex_page

// 0x5A9A59. 4-bit texels: each dword of VRAM is eight texels, low nibble first,
// each looked up in the CLUT (16-bit or 32-bit entries, as the destination).
void Tex_Convert4(void* dst, const void* src, const void* clut, int w, int h, int pitch) {
    ConvertIndexed<4>(dst, src, clut, w, h, pitch);
}

// 0x5A9BFA. 8-bit texels: each dword four texels, low byte first.
void Tex_Convert8(void* dst, const void* src, const void* clut, int w, int h, int pitch) {
    ConvertIndexed<8>(dst, src, clut, w, h, pitch);
}

// 0x5A9D2F. 15-bit direct texels: red bits 0..4, green 5..9, blue 10..14 of
// each VRAM cell, each moved to bits 19..23 and handed to Gfx_PackRgb; the
// PSX's bit 15 (semi-transparency) is dropped, and a cell of 0 - transparent on
// the PSX - is packed like any other (Gfx_ConvertRow's palettes keep it 0 and
// force other cells opaque; this does neither). The texel loop is a `loop`:
// it stops only when the count reaches exactly 0.
//
// DIVERGENCE DIV-0159 (issue #111, known-defects D333): Capcom's never sets
// Gfx_PixelFormat's opaque bits (dword 7). Under a page format with an alpha
// bit - ours is A1R5G5B5, opaque bits 0x8000 - every texel of a direct-colour
// page then has alpha 0 and the colour-key discard drops it, so Amalgam's melt
// (DIV-0142's capture, drawn from (0x340, 0x100) in 15-bit) draws nothing.
// Ours, with g_direct_opaque set: Gfx_ConvertRow's rule - a texel that packs
// non-zero takes the opaque bits, a non-zero cell that packs to 0 becomes
// 1 | opaque bits, and a cell of 0 stays 0, transparent as on the PSX.
//
// DIVERGENCE DIV-0160 (docs/stp-blend.md): while the textures carry bit 15
// (render::StpTexels, armed only with DIV-0159), a cell with bit 15 set takes
// no opaque bits - its colour with alpha clear, as Gfx_ConvertRow writes it.
void Tex_Convert16(void* dst, const void* src, int w, int h, int pitch) {
    const bool wide = tex_page::At(::bof3::Img(Gfx_PixelFormat_addr))[3] == 4;
    const Packer pack;
    const tex_page::U opaque = g_direct_opaque != 0 ? static_cast<tex_page::U>(Gfx_PixelFormat[7]) : 0;
    const bool stp = render::StpTexels();   // DIV-0160
    auto* d = static_cast<unsigned char*>(dst);
    tex_page::U total = 0x100;
    do {
        const auto* s = static_cast<const unsigned char*>(src);
        tex_page::U rows = static_cast<tex_page::U>(h);
        total -= static_cast<tex_page::U>(h);
        do {
            unsigned char* const row = d;
            tex_page::U across = 0x100;
            do {
                const unsigned char* p = s;
                tex_page::U n = static_cast<tex_page::U>(w);
                across -= static_cast<tex_page::U>(w);
                do {
                    const tex_page::U v = tex_page::Word(p);
                    tex_page::U texel = pack((v & 0x1F) << 19, (v & 0x3E0) << 14, (v & 0x7C00) << 9);
                    if (g_direct_opaque != 0) {   // DIV-0159
                        const tex_page::U bits = stp && (v & 0x8000) ? 0 : opaque;   // DIV-0160
                        if (texel != 0) texel |= bits;
                        else if (v != 0) texel = 1 | bits;
                    }
                    if (wide) {
                        tex_page::PutLong(d, texel);
                        d += 4;
                    } else {
                        tex_page::PutWord(d, texel);
                        d += 2;
                    }
                    p += 2;
                } while (--n != 0);
            } while (across != 0);
            d = row + pitch;
            s += 0x800;
        } while (--rows != 0);
    } while (total != 0);
}

// 0x5A9E1E. One 24 x 24 glyph of 4-bit texels (288 bytes, 12 a row) to dst:
// each dword eight texels low nibble first, each looked up in the 16-entry
// palette and written whatever it is (no transparency here); rows `pitch`
// apart. 16-bit entries, or 32-bit when byte 0x7DED63 is 4 (read once on
// entry). The original's four-texel tail (0x5A9EEB, 0x5A9FE7) is dead: its
// column count starts at 24 and steps 8.
void Font_UnpackGlyph(void* dst, const void* src, const void* palette, int pitch) {
    const bool wide = tex_cells::At(::bof3::Img(Gfx_PixelFormat_addr))[3] == 4;
    const auto* table = static_cast<const unsigned char*>(palette);
    const auto* s = static_cast<const unsigned char*>(src);
    auto* d = static_cast<unsigned char*>(dst);
    for (int row = 0; row < 0x18; ++row) {
        unsigned char* const start = d;
        for (int k = 0; k < 3; ++k) {
            const tex_cells::U v = tex_cells::Long(s);
            s += 4;
            for (tex_cells::U t = 0; t < 8; ++t) {
                const tex_cells::U index = (v >> (4 * t)) & 0xF;
                if (wide) tex_cells::PutLong(d + 4 * t, tex_cells::Long(table + 4 * index));
                else tex_cells::PutWord(d + 2 * t, tex_cells::Word(table + 2 * index));
            }
            d += wide ? 0x20 : 0x10;
        }
        d = start + pitch;
    }
}

void Tex_Inject() {
    BOF3_INJECT(Tex_Convert4);
    BOF3_INJECT(Tex_Convert8);
    BOF3_INJECT(Tex_Convert16);
    BOF3_INJECT(Font_UnpackGlyph);
}

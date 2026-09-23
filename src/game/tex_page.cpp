// The page textures and the DirectDraw surface helpers, read to the last
// instruction with capstone against bof3/BOF3.exe (docs/tex-page.md). All are
// the PC port's own: the PlayStation sampled VRAM on its GPU, so there is no
// PSX twin. Faithful: no divergence.
//
//   Dd_InitSurfaceDesc       0x59F840..0x59F85E (0x1F)
//   Dd_CreatePlainSurface    0x59F860..0x59F8FF (0xA0)
//   Dd_CreateTextureSurface  0x59F900..0x59F9AA (0xAB)
//   D3d_BuildPageTexture     0x5A0080..0x5A04B3 (0x434)
//   D3d_RefreshPageTexture   0x5A0510..0x5A0820 (0x311)
//   Tex_Convert4             0x5A9A59..0x5A9BF9 (0x1A1)  hand-written assembly, ebp frame
//   Tex_Convert8             0x5A9BFA..0x5A9D2E (0x135)  likewise
//   Tex_Convert16            0x5A9D2F..0x5A9E1D (0xEF)   likewise; calls Gfx_PackRgb 0x5AA79C per texel
//
// The DirectDraw is DirectX 6's: an IDirectDraw4 at Dd_DirectDraw, surfaces
// described by a DDSURFACEDESC2 (0x7C bytes), textures reached by
// QueryInterface for IID_IDirect3DTexture2. The COM calls here go through the
// vtables exactly as the original's do, one call each, in its order.
#include "game/tex_page.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/tex_page_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace tex_page {

const Callees kOriginals = {
    Tex_Convert4,
    Tex_Convert8,
    Tex_Convert16,
    Gfx_ClutPixels,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
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
std::int32_t Short(U address) {
    std::int16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

// One COM call: slot `offset` of the object's vtable, stdcall, `this` first.
template <typename... A> long Com(U object, U offset, A... args) {
    void* o = reinterpret_cast<void*>(static_cast<std::uintptr_t>(object));
    auto fn = reinterpret_cast<long(__stdcall*)(void*, A...)>((*reinterpret_cast<void***>(o))[offset / 4]);
    return fn(o, args...);
}

// IDirectDrawSurface4 and IDirectDraw4 slots, as byte offsets.
constexpr U kQueryInterface = 0x00, kBlt = 0x14, kCreateSurface = 0x18, kLock = 0x64, kSetColorKey = 0x74,
            kUnlock = 0x80;
constexpr U kLockWait = 1;             // DDLOCK_WAIT
constexpr U kBltWait = 0x1000000;      // DDBLT_WAIT
constexpr U kSrcBltKey = 8;            // DDCKEY_SRCBLT
constexpr void* kNoRect = nullptr;     // Lock's RECT *: the whole surface

// A DDSURFACEDESC2 on the stack, as the original keeps one: dwSize +0,
// dwFlags +4, dwHeight +8, dwWidth +0xC, lPitch +0x10, lpSurface +0x24, the
// DDPIXELFORMAT +0x48, ddsCaps +0x68 (dwCaps, dwCaps2 +0x6C, ...),
// dwTextureStage +0x78.
struct alignas(4) Desc {
    unsigned char b[0x7C];
    U Pitch() const { return Long(b + 0x10); }
    void* Surface() const { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(Long(b + 0x24))); }
};

// The rectangle every Blt of a page names on both sides: {0, 0, 256, 256}.
struct Rect {
    std::int32_t l, t, r, b;
};

// Gfx_PackRgb 0x5AA79C, which Tex_Convert16 calls per texel with the three
// channels in eax, ebx, edx: each shifted right by the dword at 0x7DED64 /
// 0x7DED68 / 0x7DED6C (`shr reg, cl` - the count's low five bits) and masked
// by 0x7DED70 / 0x7DED74 / 0x7DED78, OR-ed. Nothing writes those six dwords
// while a conversion runs, so they are read once per call here.
struct Packer {
    U shift_r, shift_g, shift_b, mask_r, mask_g, mask_b;
    Packer()
        : shift_r(At(kPixelFormat + 4)[0] & 31u), shift_g(At(kPixelFormat + 8)[0] & 31u),
          shift_b(At(kPixelFormat + 0xC)[0] & 31u), mask_r(Long(kPixelFormat + 0x10)),
          mask_g(Long(kPixelFormat + 0x14)), mask_b(Long(kPixelFormat + 0x18)) {}
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
    const bool wide = At(kPixelFormat)[3] == 4;
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

using namespace tex_page;

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
void Tex_Convert16(void* dst, const void* src, int w, int h, int pitch) {
    const bool wide = At(kPixelFormat)[3] == 4;
    const Packer pack;
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
                    const U v = Word(p);
                    const U texel = pack((v & 0x1F) << 19, (v & 0x3E0) << 14, (v & 0x7C00) << 9);
                    if (wide) {
                        PutLong(d, texel);
                        d += 4;
                    } else {
                        PutWord(d, texel);
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

// 0x59F840. Zeroes a DDSURFACEDESC2 and sets its dwSize (0x7C) and its pixel
// format's dwSize (0x20, at +0x48). The original leaves eax 0; no caller reads
// it (each overwrites eax before any use: 0x59F86F, 0x59F90F, 0x5A0133,
// 0x5A05C4, 0x5A06C5, and the rest read memory into it next).
void Dd_InitSurfaceDesc(void* desc) {
    auto* d = static_cast<unsigned char*>(desc);
    std::memset(d, 0, 0x7C);
    PutLong(d, 0x7C);
    PutLong(d + 0x48, 0x20);
}

// 0x59F860. A plain surface of width and height each rounded up to a multiple
// of 4, caps 0x840 (OFFSCREENPLAIN | SYSTEMMEMORY) under Gfx_RenderFlags bit 0
// and 0x1800 (TEXTURE | SYSTEMMEMORY) otherwise, flags 0x1007 (CAPS | HEIGHT |
// WIDTH | PIXELFORMAT), the pixel format the 0x20 bytes at 0x7DED80 +
// format * 0x40. Returns 1 if IDirectDraw4::CreateSurface returned DD_OK, else 0.
int Dd_CreatePlainSurface(unsigned width, unsigned height, void** surface, unsigned format) {
    Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    const U caps = (At(kRenderFlags)[0] & 1) ? 0x840u : 0x1800u;
    PutLong(desc.b + 0xC, (width + 3) & ~3u);
    PutLong(desc.b + 8, (height + 3) & ~3u);
    PutLong(desc.b + 0x68, caps);
    PutLong(desc.b + 4, 0x1007);
    PutLong(desc.b + 0x6C, 0);
    std::memcpy(desc.b + 0x48, At(kFormats + (format << 6)), 0x20);
    const U dd = Long(kDirectDraw);
    return Com(dd, kCreateSurface, desc.b, surface, 0u) == 0 ? 1 : 0;
}

// 0x59F900. A managed texture: width and height as given, flags 0x101007
// (... | TEXTURESTAGE), caps 0x1000 (TEXTURE), caps2 0x10 (TEXTUREMANAGE),
// texture stage 0, the pixel format as above. On failure returns 0 and does
// nothing more; on success asks the new surface (read back from *surface) for
// IID_IDirect3DTexture2 into *texture and returns 1 whatever that answers.
int Dd_CreateTextureSurface(unsigned width, unsigned height, void** surface, void** texture, unsigned format) {
    Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    PutLong(desc.b + 0xC, width);
    PutLong(desc.b + 4, 0x101007);
    PutLong(desc.b + 0x68, 0x1000);
    PutLong(desc.b + 0x6C, 0x10);
    PutLong(desc.b + 0x78, 0);
    PutLong(desc.b + 8, height);
    std::memcpy(desc.b + 0x48, At(kFormats + (format << 6)), 0x20);
    const U dd = Long(kDirectDraw);
    if (Com(dd, kCreateSurface, desc.b, surface, 0u) != 0) return 0;
    Com(Addr(*surface), kQueryInterface, At(kTexture2Iid), texture);
    return 1;
}

namespace tex_page {
namespace {

// Where the page's texels start in the VRAM shadow: the page's column
// ((page & 0xF) * 64 cells = * 0x80 bytes) and row block ((page & 0x10) * 16
// rows), moved by the key's y and by its x in the colour mode's units - half a
// byte a texel for mode 0, a byte for 1, two for 2, nothing for any other mode.
U Source(U page, U mode, std::int32_t key_x, std::int32_t key_y) {
    const U y = ((page & 0x10) << 4) + static_cast<U>(key_y);
    U x = (page & 0xF) << 7;
    switch (mode) {
    case 0: x += static_cast<U>(key_x >> 1); break;
    case 1: x += static_cast<U>(key_x); break;
    case 2: x += static_cast<U>(key_x) * 2; break;
    default: break;
    }
    return kVram + (y << 11) + x;
}

U Generation(U row_address) { return Long(row_address); }

}  // namespace
}  // namespace tex_page

// 0x5A0080. Builds the first free entry (state byte 0) of Gfx_TexCache page
// `page` from VRAM: 256 x 256 texels of the window Gfx_TexCacheKey (x, y, w, h)
// names, in colour mode `mode`, through CLUT `clut` unless mode bit 1 (direct).
//
// None free of the page's 32: returns 0. Under Gfx_RenderFlags bit 0 (the
// software surfaces) a 256 x 256 plain surface into the entry's +0x10, locked
// and filled directly; returns the slot, the entry's +0x14 zeroed. Otherwise a
// managed 256 x 256 texture into +0x10 / +0x14, colour-keyed on 0 under
// Gfx_RenderFlags bit 5, filled by a Blt of {0, 0, 256, 256} from the staging
// surface Dd_StageSurface, which is locked and filled; returns the texture (+0x14,
// read after the Blt). A surface that cannot be made, or a Lock that fails,
// returns 0 with the entry left free - and the surface just made left in it,
// unreleased (known-defects D30). On success: state 1, byte +1 the mode's low
// byte, +8 the key's 8 bytes (read last); unless direct, +2 the CLUT's low 16
// bits and +4 its row's generation (Gfx_ClutRows at (clut sar 6) * 8, read
// after the Unlock).
//
// Two tests of `mode` disagree for values beyond 3, as the original's do: the
// x offset and the direct test (`test al, 2`) read the low bits, but 4-bit
// against 8-bit is the whole dword against 0. The callers pass 0..3 (tpage
// bits 7..8).
unsigned long D3d_BuildPageTexture(int page, int clut, int mode) {
    const U p = static_cast<U>(page);
    const U m = static_cast<U>(mode);
    U slot = 0;
    for (U e = kPageCache + ((p * 3) << 8); slot < 0x20; ++slot, e += kEntryBytes)
        if (At(e)[0] == 0) break;
    if (slot == 0x20) return 0;
    const U src = Source(p, m, Short(kTexKey), Short(kTexKey + 2));
    Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    const U entry = kPageCache + ((p << 5) + slot) * kEntryBytes;
    const U surface_slot = entry + 0x10;
    U result;
    if (At(kRenderFlags)[0] & 1) {
        if (!Dd_CreatePlainSurface(0x100, 0x100, reinterpret_cast<void**>(At(surface_slot)), 0)) return 0;
        const void* table = nullptr;
        if (!(m & 2)) table = g.clut_pixels(static_cast<unsigned>(clut));
        if (Com(Long(surface_slot), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return 0;
        if (m & 2) g.convert16(desc.Surface(), At(src), Short(kTexKey + 4), Short(kTexKey + 6), static_cast<int>(desc.Pitch()));
        else if (m != 0) g.convert8(desc.Surface(), At(src), table, Short(kTexKey + 4), Short(kTexKey + 6), static_cast<int>(desc.Pitch()));
        else g.convert4(desc.Surface(), At(src), table, Short(kTexKey + 4), Short(kTexKey + 6), static_cast<int>(desc.Pitch()));
        Com(Long(surface_slot), kUnlock, 0u);
        if (!(m & 2)) {
            PutWord(At(entry + 2), static_cast<U>(clut));
            PutLong(entry + 4, Generation(kClutRows + static_cast<U>(clut >> 6) * 8));
        }
        PutLong(entry + 0x14, 0);
        result = slot;
    } else {
        if (!Dd_CreateTextureSurface(0x100, 0x100, reinterpret_cast<void**>(At(surface_slot)),
                                     reinterpret_cast<void**>(At(entry + 0x14)), 0))
            return 0;
        if (At(kRenderFlags)[0] & 0x20) {
            U key[2] = {0, 0};   // DDCOLORKEY {0, 0}
            Com(Long(surface_slot), kSetColorKey, kSrcBltKey, key);
        }
        const void* table = nullptr;
        if (!(m & 2)) table = g.clut_pixels(static_cast<unsigned>(clut));
        if (Com(Long(kStage), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return 0;
        if (m & 2) g.convert16(desc.Surface(), At(src), Short(kTexKey + 4), Short(kTexKey + 6), static_cast<int>(desc.Pitch()));
        else if (m != 0) g.convert8(desc.Surface(), At(src), table, Short(kTexKey + 4), Short(kTexKey + 6), static_cast<int>(desc.Pitch()));
        else g.convert4(desc.Surface(), At(src), table, Short(kTexKey + 4), Short(kTexKey + 6), static_cast<int>(desc.Pitch()));
        Com(Long(kStage), kUnlock, 0u);
        if (!(m & 2)) {
            PutWord(At(entry + 2), static_cast<U>(clut));
            PutLong(entry + 4, Generation(kClutRows + static_cast<U>(clut >> 6) * 8));
        }
        Rect rect = {0, 0, 0x100, 0x100};
        Com(Long(surface_slot), kBlt, &rect, Long(kStage), &rect, kBltWait, 0u);
        result = Long(entry + 0x14);
    }
    At(entry)[0] = 1;
    At(entry)[1] = static_cast<unsigned char>(m);
    PutLong(entry + 8, Long(kTexKey));
    PutLong(entry + 0xC, Long(kTexKey + 4));
    return result;
}

// 0x5A0510. Re-fills entry `slot` of page `page` from VRAM in place - the same
// surfaces, the same key and mode, as D3d_BuildPageTexture filled them - for
// an entry in state 2 (its CLUT's generation moved, or its VRAM was written
// under it). The state byte is set to 1 FIRST, before anything can fail; a
// failed Lock returns with nothing more done. The mode byte is re-read after
// the Lock for 4-bit against 8-bit (`test al, al` - the byte, where the build
// tested the dword). Unless direct: Gfx_ClutPixels of the entry's CLUT word
// (the original pushes it with the upper half of ecx left over from the x
// arithmetic; Gfx_ClutPixels masks it to 16 bits at 0x5A04C5, so it is passed
// clean here), and after the fill +4 the generation of row (clut >> 6),
// unsigned this time. Software: lock the entry's own surface. Direct3D: the
// staging surface, then a Blt of {0, 0, 256, 256} into the entry's surface.
void D3d_RefreshPageTexture(int page, int slot) {
    const U p = static_cast<U>(page);
    const U entry = kPageCache + ((p << 5) + static_cast<U>(slot)) * kEntryBytes;
    At(entry)[0] = 1;
    const U mode = At(entry)[1];
    const U src = Source(p, mode, Short(entry + 8), Short(entry + 0xA));
    Desc desc;
    Rect rect = {0, 0, 0x100, 0x100};
    if (mode & 2) {
        Dd_InitSurfaceDesc(desc.b);
        if (At(kRenderFlags)[0] & 1) {
            if (Com(Long(entry + 0x10), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return;
            g.convert16(desc.Surface(), At(src), Short(entry + 0xC), Short(entry + 0xE), static_cast<int>(desc.Pitch()));
            Com(Long(entry + 0x10), kUnlock, 0u);
            return;
        }
        if (Com(Long(kStage), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return;
        g.convert16(desc.Surface(), At(src), Short(entry + 0xC), Short(entry + 0xE), static_cast<int>(desc.Pitch()));
        Com(Long(kStage), kUnlock, 0u);
        Com(Long(entry + 0x10), kBlt, &rect, Long(kStage), &rect, kBltWait, 0u);
        return;
    }
    const void* table = g.clut_pixels(Word(At(entry + 2)));
    Dd_InitSurfaceDesc(desc.b);
    const bool software = (At(kRenderFlags)[0] & 1) != 0;
    const U locked = software ? entry + 0x10 : kStage;
    if (Com(Long(locked), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return;
    if (At(entry)[1] != 0)
        g.convert8(desc.Surface(), At(src), table, Short(entry + 0xC), Short(entry + 0xE), static_cast<int>(desc.Pitch()));
    else
        g.convert4(desc.Surface(), At(src), table, Short(entry + 0xC), Short(entry + 0xE), static_cast<int>(desc.Pitch()));
    Com(Long(locked), kUnlock, 0u);
    if (!software) Com(Long(entry + 0x10), kBlt, &rect, Long(kStage), &rect, kBltWait, 0u);
    PutLong(entry + 4, Generation(kClutRows + (Word(At(entry + 2)) >> 6) * 8));
}

void TexPage_Inject() {
    if (bof3::WantsShadow("tex_page")) tex_page::SelfTest();
    BOF3_INJECT(Dd_InitSurfaceDesc);
    BOF3_INJECT(Dd_CreatePlainSurface);
    BOF3_INJECT(Dd_CreateTextureSurface);
    BOF3_INJECT(D3d_BuildPageTexture);
    BOF3_INJECT(D3d_RefreshPageTexture);
    BOF3_INJECT(Tex_Convert4);
    BOF3_INJECT(Tex_Convert8);
    BOF3_INJECT(Tex_Convert16);
}

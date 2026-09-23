// The glyph and cell textures, read to the last instruction with capstone
// against bof3/BOF3.exe (docs/tex-cells.md). All are the PC port's own: the
// PlayStation drew glyphs and sprite cells straight from VRAM on its GPU, so
// there is no PSX twin. Faithful: no divergence.
//
//   D3d_FitTextureSize      0x59F9B0..0x59FA49 (0x9A)
//   Font_BuildGlyphTexture  0x5A2CA0..0x5A2E68 (0x1C9)
//   Dd_ClearSurface         0x5A2E70..0x5A2EAB (0x3C)
//   D3d_BuildCellTexture    0x5A32B0..0x5A378A (0x4DB)
//   D3d_FreeCellTexture     0x5A3790..0x5A37CD (0x3E)
//   D3d_RefreshCellTexture  0x5A37D0..0x5A3A59 (0x28A)
//   Font_UnpackGlyph        0x5A9E1E..0x5AA03D (0x220)  hand-written assembly, ebp frame
//   Cell_Unpack4            0x5AA03E..0x5AA1DF (0x1A2)  likewise
//   Cell_Unpack8            0x5AA1E0..0x5AA2F5 (0x116)  likewise
//   Cell_Unpack4Flip        0x5AA2F6..0x5AA4AD (0x1B8)  likewise
//   Cell_Unpack8Flip        0x5AA4AE..0x5AA5D5 (0x128)  likewise
//
// The COM calls go through the vtables exactly as the original's do, one call
// each, in its order; every pointer the original re-reads from memory is
// re-read here at the same point.
#include "game/tex_cells.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/tex_cells_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace tex_cells {

const Callees kOriginals = {
    Font_UnpackGlyph, Cell_Unpack4, Cell_Unpack8, Cell_Unpack4Flip, Cell_Unpack8Flip, Gfx_ClutPixels,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
void* Ptr(U address) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)); }
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
U Word(U address) { return Word(At(address)); }
std::int32_t Short(U address) {
    std::int16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
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

// One COM call: slot `offset` of the object's vtable, stdcall, `this` first.
template <typename... A> long Com(U object, U offset, A... args) {
    void* o = Ptr(object);
    auto fn = reinterpret_cast<long(__stdcall*)(void*, A...)>((*reinterpret_cast<void***>(o))[offset / 4]);
    return fn(o, args...);
}

// IDirectDrawSurface4 byte offsets; the device's two.
constexpr U kRelease = 0x08, kBlt = 0x14, kLock = 0x64, kSetColorKey = 0x74, kUnlock = 0x80;
constexpr U kBeginScene = 0x24, kEndScene = 0x28;   // IDirect3DDevice3
constexpr U kLockWait = 1;                           // DDLOCK_WAIT
constexpr U kBltWait = 0x1000000;                    // DDBLT_WAIT
constexpr U kBltColorFillWait = 0x1000400;           // DDBLT_COLORFILL | DDBLT_WAIT
constexpr U kSrcBltKey = 8;                          // DDCKEY_SRCBLT
void* const kNoRect = nullptr;

// A DDSURFACEDESC2 on the stack: lPitch +0x10, lpSurface +0x24.
struct alignas(4) Desc {
    unsigned char b[0x7C];
    U Pitch() const { return Long(b + 0x10); }
    U Surface() const { return Long(b + 0x24); }
};

struct Rect {
    std::int32_t l, t, r, b;
};

// The cell unpackers' shared shape (0x5AA03E, 0x5AA1E0, 0x5AA2F6, 0x5AA4AE):
// h rows of w texels from src (rows 0x800 bytes apart) to dst (rows `pitch`
// apart), each texel looked up in the palette and written only if the entry
// is non-zero - 0 is transparent, the destination keeps what it had (tested
// on the 16-bit entry, or the whole 32-bit one). Four-bit texels low nibble
// first, eight-bit low byte first; the Flip ones start at the row's last
// dword (src + w / 2 - 4, unsigned, or src + w - 4) and walk back, each
// dword's texels high first - the row mirrored. The texel count steps 8 or 4
// and stops only at exactly 0, and the row count is a `dec` / `jne`: w must
// be a non-zero multiple of the step and h non-zero, or the original runs on
// through memory; ours does the same arithmetic. Byte 0x7DED63 == 4 (read once
// on entry) makes the destination 32-bit.
template <int Bits, bool Flip> void CellUnpack(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    constexpr U kPerLong = 32 / Bits;
    constexpr U kMask = (1u << Bits) - 1;
    const bool wide = At(kPixelFormat)[3] == 4;
    const auto* table = static_cast<const unsigned char*>(palette);
    const U width = static_cast<U>(w);
    U s = Addr(src);
    if (Flip) s += (Bits == 4 ? width >> 1 : width) - 4;
    auto* d = static_cast<unsigned char*>(dst);
    U rows = static_cast<U>(h);
    do {
        U p = s;
        unsigned char* q = d;
        U n = width;
        do {
            const U v = Long(p);
            for (U k = 0; k < kPerLong; ++k) {
                const U index = (v >> (Bits * (Flip ? kPerLong - 1 - k : k))) & kMask;
                if (wide) {
                    const U c = Long(table + 4 * index);
                    if (c != 0) PutLong(q + 4 * k, c);
                } else {
                    const U c = Word(table + 2 * index);
                    if (c != 0) PutWord(q + 2 * k, c);
                }
            }
            p = Flip ? p - 4 : p + 4;
            q += (wide ? 4 : 2) * kPerLong;
            n -= kPerLong;
        } while (n != 0);
        s += 0x800;
        d += pitch;
    } while (--rows != 0);
}

// The Direct3D build's height for a texture narrower than the cells' extent:
// `fild` the texture width, `fidiv` the used width, `fimul` the used height,
// then the CRT's _ftol 0x5B9550 - round-toward-zero set in a copy of the
// control word for one `fistp` to 64 bits (0x5A36FA..0x5A3713). Rounded at the
// control word's precision, as the original; the caller keeps the low 16 bits.
U ScaledHeight(std::int32_t texture_width, std::int32_t used_width, std::int32_t used_height) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "fildl %[tw]\n\t"
        "fidivl %[uw]\n\t"
        "fimull %[uh]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [tw] "m"(texture_width), [uw] "m"(used_width), [uh] "m"(used_height)
        : "eax", "st");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// One pass over the cell records [first, first + count) onto the locked
// staging surface: D3d_BuildCellTexture's loop (0x5A33BA..0x5A351E) and
// D3d_RefreshCellTexture's (0x5A385A..0x5A3944), which differ only in the
// extent and checksum the build keeps. Each record: s16 x +0, s8 y +2, the
// size byte +3 (w = low nibble * 8, h = high nibble * 8), the tpage word +4
// (bits 0..3 the page column, 4 the page row, 7..8 the colour mode - 0 4-bit,
// anything else 8-bit - and 9 mirrored), u +6, v +7. The texels start at the
// page's VRAM plus v rows and u bytes (u / 2 for 4-bit); they go to (x + 160,
// y + 128) of the stage, 160 and 128 its centre, at the pitch and the depth
// (byte 0x7DED63, read per record) as they are then - nothing bounds either.
// Every field is read before the record's unpacker runs; the build's checksum
// adds the record's two dwords after it.
struct Extent {
    std::int32_t left, top, right, bottom;
    U checksum;
};
void DrawCells(U first, U count, const void* palette, const Desc& desc, Extent* extent) {
    if (static_cast<std::int32_t>(first) >= static_cast<std::int32_t>(first + count)) return;
    U record = kCellTable + first * 8;
    U n = count;
    do {
        const unsigned char* r = At(record);
        const std::int32_t x = Short(record);
        const std::int32_t y = static_cast<std::int8_t>(r[2]);
        const U tpage = Word(r + 4);
        const U mode = tpage & 0x180;
        const U u = mode != 0 ? r[6] : static_cast<U>(r[6]) >> 1;
        const U w = (r[3] & 0xFu) << 3;
        const U h = (static_cast<U>(r[3]) >> 1) & 0x78;
        const U src = kVram + (((((tpage & 0x10) << 4) + r[7]) << 11) + u) + ((tpage & 0xF) << 7);
        const U bpp = At(kPixelFormat)[3];
        const U dst = desc.Surface() + static_cast<U>(x + 0xA0) * bpp + static_cast<U>(y + 0x80) * desc.Pitch();
        const bool flip = (Word(r + 4) & 0x200) != 0;
        const Unpack unpack = flip ? (mode != 0 ? g.unpack8_flip : g.unpack4_flip) : (mode != 0 ? g.unpack8 : g.unpack4);
        unpack(Ptr(dst), Ptr(src), palette, static_cast<int>(w), static_cast<int>(h), static_cast<int>(desc.Pitch()));
        if (extent) {
            if (extent->left > x) extent->left = x;
            if (extent->right < x + static_cast<std::int32_t>(w)) extent->right = x + static_cast<std::int32_t>(w);
            if (extent->top > y) extent->top = y;
            if (extent->bottom < y + static_cast<std::int32_t>(h)) extent->bottom = y + static_cast<std::int32_t>(h);
            extent->checksum += Long(record);
            extent->checksum += Long(record + 4);
        }
        record += 8;
    } while (--n != 0);
}

}  // namespace
}  // namespace tex_cells

using namespace tex_cells;

// 0x5A9E1E. One 24 x 24 glyph of 4-bit texels (288 bytes, 12 a row) to dst:
// each dword eight texels low nibble first, each looked up in the 16-entry
// palette and written whatever it is (no transparency here); rows `pitch`
// apart. 16-bit entries, or 32-bit when byte 0x7DED63 is 4 (read once on
// entry). The original's four-texel tail (0x5A9EEB, 0x5A9FE7) is dead: its
// column count starts at 24 and steps 8.
void Font_UnpackGlyph(void* dst, const void* src, const void* palette, int pitch) {
    const bool wide = At(kPixelFormat)[3] == 4;
    const auto* table = static_cast<const unsigned char*>(palette);
    const auto* s = static_cast<const unsigned char*>(src);
    auto* d = static_cast<unsigned char*>(dst);
    for (int row = 0; row < 0x18; ++row) {
        unsigned char* const start = d;
        for (int k = 0; k < 3; ++k) {
            const U v = Long(s);
            s += 4;
            for (U t = 0; t < 8; ++t) {
                const U index = (v >> (4 * t)) & 0xF;
                if (wide) PutLong(d + 4 * t, Long(table + 4 * index));
                else PutWord(d + 2 * t, Word(table + 2 * index));
            }
            d += wide ? 0x20 : 0x10;
        }
        d = start + pitch;
    }
}

// 0x5AA03E, 0x5AA1E0, 0x5AA2F6, 0x5AA4AE: see CellUnpack.
void Cell_Unpack4(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    CellUnpack<4, false>(dst, src, palette, w, h, pitch);
}
void Cell_Unpack8(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    CellUnpack<8, false>(dst, src, palette, w, h, pitch);
}
void Cell_Unpack4Flip(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    CellUnpack<4, true>(dst, src, palette, w, h, pitch);
}
void Cell_Unpack8Flip(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    CellUnpack<8, true>(dst, src, palette, w, h, pitch);
}

// 0x59F9B0. The texture size the device takes for a w x h image, from
// D3d_DeviceDesc: under D3DPTEXTURECAPS_POW2 (bit 1 of the caps' low byte,
// read once) each side is the device's minimum (1 for 0) doubled until it is
// not below the side - signed compares, so a side above 0x40000000 never ends
// and a negative one keeps the minimum; else the sides as given. Each then
// clamped to the device's maximum (unsigned), and under SQUAREONLY (bit 5)
// both made the larger (signed). *out_w is written before *out_h. Callers
// D3d_AfterDraw 0x59F603 and D3d_BuildCellTexture; neither reads eax after.
void D3d_FitTextureSize(unsigned w, unsigned h, unsigned* out_w, unsigned* out_h) {
    const U caps = At(kTexCaps)[0];
    U tw = w, th = h;
    if (caps & 2) {
        tw = Long(kMinTexWidth);
        if (tw == 0) tw = 1;
        while (static_cast<std::int32_t>(tw) < static_cast<std::int32_t>(w)) tw <<= 1;
        th = Long(kMinTexHeight);
        if (th == 0) th = 1;
        while (static_cast<std::int32_t>(th) < static_cast<std::int32_t>(h)) th <<= 1;
    }
    const U max_w = Long(kMaxTexWidth);
    if (tw > max_w) tw = max_w;
    const U max_h = Long(kMaxTexHeight);
    if (th > max_h) th = max_h;
    if ((caps & 0x20) && tw != th) {
        const U side = static_cast<std::int32_t>(tw) > static_cast<std::int32_t>(th) ? tw : th;
        *out_w = side;
        *out_h = side;
        return;
    }
    *out_w = tw;
    *out_h = th;
}

// 0x5A2E70. Fills the whole of `surface` with colour 0: Blt(NULL, NULL, NULL,
// DDBLT_COLORFILL | DDBLT_WAIT, &fx), fx a zeroed DDBLTFX of 0x64 bytes with
// its dwSize set. Returns what the Blt returned; no caller reads it (0x5A2D38,
// 0x5A3339, 0x5A37E3, and the two in the Direct3D set-up, 0x5A55FF and
// 0x5A56F4, each `add esp, 4` and move on).
long Dd_ClearSurface(void* surface) {
    alignas(4) unsigned char fx[0x64] = {};
    PutLong(fx, 0x64);
    return Com(Addr(surface), kBlt, kNoRect, kNoRect, kNoRect, kBltColorFillWait, static_cast<void*>(fx));
}

// 0x5A3790. Empties D3d_CellTexCache entry `slot`: Release of the texture
// (+0x24) if any, then of the surface (+0x20, read after that Release) if
// any, then all 0x28 bytes zeroed. No bound on `slot`. Callers
// D3d_BuildCellTexture and the Direct3D teardown 0x5A6760 (all 128).
void D3d_FreeCellTexture(int slot) {
    const U entry = kCellCache + static_cast<U>(slot) * kCellEntryBytes;
    const U texture = Long(entry + 0x24);
    if (texture != 0) Com(texture, kRelease);
    const U surface = Long(entry + 0x20);
    if (surface != 0) Com(surface, kRelease);
    std::memset(At(entry), 0, kCellEntryBytes);
}

// 0x5A2CA0. Builds Font_TexCache entry `slot` for `glyph` in `clut`: the 16
// colours from Gfx_ClutPixels(clut) (first, before anything else is read), the
// glyph at Font_GlyphData + glyph * 0x120 (the pointer read after that call).
//
// Software surfaces (Gfx_RenderFlags bit 0): if the entry has no surface (+8),
// a 32 x 32 plain one is made into +8, cleared to 0 and +0xC zeroed; the
// entry's surface is locked and the glyph unpacked into it. Direct3D: if no
// surface, a 32 x 32 texture into +8 / +0xC, colour-keyed on 0 under flag bit
// 5 (re-read after the create); the staging surface is locked, the glyph
// unpacked into it, and {0, 0, 24, 24} Blt from it into the entry's surface.
// An existing surface is reused as it is - not cleared, not keyed. A failed
// create or Lock returns at once, the entry's glyph, CLUT and generation not
// written; otherwise +0 the glyph's low 16 bits, +2 the CLUT's, +4 the
// generation of Gfx_ClutRows at (clut sar 6) * 8 - signed, on the whole
// argument. Every surface pointer is re-read from the entry or from
// Dd_StageSurface at each use, as the original's are.
void Font_BuildGlyphTexture(int slot, unsigned glyph, unsigned clut) {
    const void* palette = g.clut_pixels(clut);
    const U src = Long(kGlyphData) + glyph * 0x120u;
    Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    const U entry = kFontCache + static_cast<U>(slot) * kFontEntryBytes;
    const U surface_slot = entry + 8;
    if (At(kRenderFlags)[0] & 1) {
        if (Long(surface_slot) == 0) {
            if (!Dd_CreatePlainSurface(0x20, 0x20, reinterpret_cast<void**>(At(surface_slot)), 0)) return;
            Dd_ClearSurface(Ptr(Long(surface_slot)));
            PutLong(entry + 0xC, 0);
        }
        if (Com(Long(surface_slot), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return;
        g.unpack_glyph(Ptr(desc.Surface()), Ptr(src), palette, static_cast<int>(desc.Pitch()));
        Com(Long(surface_slot), kUnlock, 0u);
    } else {
        if (Long(surface_slot) == 0) {
            if (!Dd_CreateTextureSurface(0x20, 0x20, reinterpret_cast<void**>(At(surface_slot)),
                                         reinterpret_cast<void**>(At(entry + 0xC)), 0))
                return;
            if (At(kRenderFlags)[0] & 0x20) {
                U key[2] = {0, 0};   // DDCOLORKEY {0, 0}
                Com(Long(surface_slot), kSetColorKey, kSrcBltKey, key);
            }
        }
        if (Com(Long(kStage), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return;
        g.unpack_glyph(Ptr(desc.Surface()), Ptr(src), palette, static_cast<int>(desc.Pitch()));
        Com(Long(kStage), kUnlock, 0u);
        Rect rect = {0, 0, 0x18, 0x18};
        const U surface = Long(surface_slot);
        const U stage = Long(kStage);
        Com(surface, kBlt, &rect, stage, &rect, kBltWait, 0u);
    }
    PutWord(entry, glyph);
    PutWord(entry + 2, clut);
    PutLong(entry + 4, Long(kClutRows + static_cast<U>(static_cast<std::int32_t>(clut) >> 6) * 8));
}

// 0x5A32B0. Composes the `count` SpriteCell records from `first` into
// D3d_CellTexCache entry `slot`. D3d_CellTexture passes its flags & 0x800 as
// `backdrop`.
//
// 1. The staging surface's canvas: unless Gfx_RenderFlags bit 0 (read once,
//    on entry), with `backdrop` non-zero and a surface at Dd_CellBackdrop
//    0x6BEA00, a Blt of that surface's {0, 0, 320, 240} to the stage's {0, 8,
//    320, 248}; otherwise Dd_ClearSurface of the stage.
// 2. Lock the stage (a failure returns at once, the entry untouched); the
//    palette Gfx_ClutPixels(clut); every record drawn (DrawCells), keeping the
//    extent - from (0, 0), so it always holds the origin - and the 32-bit sum
//    of the records' dwords; Unlock.
// 3. D3d_FreeCellTexture(slot), then the entry: extent s16 +8 left, +0xA
//    right, +0xC top, +0xE bottom; used size +0 right - left, +2 bottom - top;
//    +0x10 the CLUT's low 16 bits, +0x12 the count's; +0x18 the sum; +0x1C the
//    generation of Gfx_ClutRows at (clut shr 6) * 8 - unsigned here.
// 4. The flag byte re-read. Software: a plain surface of the used size into
//    +0x20 (a failure returns), a Blt of the stage's {left + 160, top + 128,
//    right + 160, bottom + 128} to its {0, 0, used w, used h}, and +4 / +6
//    the used size (re-read) rounded up to 4 in 16 bits. Direct3D: device
//    EndScene; D3d_FitTextureSize of the used size (re-read); a texture of
//    that size into +0x20 / +0x24 - a failure returns WITHOUT BeginScene
//    (known-defects D31); if the texture is narrower than the used width
//    (signed), the used height becomes texture width / used width * used
//    height through the x87 and _ftol and the used width the texture's, both
//    stored and the Blt's destination made {0, 0, those}; the same Blt into
//    +0x20; +4 / +6 the texture's size; device BeginScene. Only the width is
//    tested: a texture shorter than the used height gets a destination taller
//    than itself.
void D3d_BuildCellTexture(int slot, unsigned first, unsigned count, unsigned clut, unsigned backdrop) {
    const U flags = At(kRenderFlags)[0];
    U from_surface = 0;
    if (!(flags & 1) && backdrop != 0 && (from_surface = Long(kBackdrop)) != 0) {
        Rect to = {0, 8, 0x140, 0xF8};
        Rect from = {0, 0, 0x140, 0xF0};
        Com(Long(kStage), kBlt, &to, from_surface, &from, kBltWait, 0u);
    } else {
        Dd_ClearSurface(Ptr(Long(kStage)));
    }
    Extent extent = {0, 0, 0, 0, 0};
    Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    if (Com(Long(kStage), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return;
    const void* palette = g.clut_pixels(clut);
    DrawCells(first, count, palette, desc, &extent);
    Com(Long(kStage), kUnlock, 0u);
    D3d_FreeCellTexture(slot);

    const U entry = kCellCache + static_cast<U>(slot) * kCellEntryBytes;
    PutWord(entry + 8, static_cast<U>(extent.left));
    PutWord(entry + 0xA, static_cast<U>(extent.right));
    PutWord(entry + 0xC, static_cast<U>(extent.top));
    PutWord(entry + 0xE, static_cast<U>(extent.bottom));
    PutWord(entry + 0, static_cast<U>(extent.right - extent.left));
    PutWord(entry + 2, static_cast<U>(extent.bottom - extent.top));
    PutWord(entry + 0x12, count);
    PutWord(entry + 0x10, clut);
    PutLong(entry + 0x18, extent.checksum);
    PutLong(entry + 0x1C, Long(kClutRows + (clut >> 6) * 8));
    Rect from = {extent.left + 0xA0, extent.top + 0x80, extent.right + 0xA0, extent.bottom + 0x80};
    const U flags_now = At(kRenderFlags)[0];
    Rect to = {0, 0, static_cast<std::int32_t>(Word(entry + 0)), static_cast<std::int32_t>(Word(entry + 2))};
    if (flags_now & 1) {
        if (!Dd_CreatePlainSurface(static_cast<unsigned>(to.r), static_cast<unsigned>(to.b),
                                   reinterpret_cast<void**>(At(entry + 0x20)), 0))
            return;
        const U surface = Long(entry + 0x20);
        const U stage = Long(kStage);
        Com(surface, kBlt, &to, stage, &from, kBltWait, 0u);
        PutWord(entry + 4, (Word(entry + 0) + 3) & 0xFFFC);
        PutWord(entry + 6, (Word(entry + 2) + 3) & 0xFFFC);
        return;
    }
    Com(Long(kDevice), kEndScene);
    unsigned texture_w, texture_h;
    D3d_FitTextureSize(Word(entry + 0), Word(entry + 2), &texture_w, &texture_h);
    if (!Dd_CreateTextureSurface(texture_w, texture_h, reinterpret_cast<void**>(At(entry + 0x20)),
                                 reinterpret_cast<void**>(At(entry + 0x24)), 0))
        return;
    const U used_w = Word(entry + 0);
    if (static_cast<std::int32_t>(texture_w) < static_cast<std::int32_t>(used_w)) {
        const U used_h = Word(entry + 2);
        PutWord(entry + 2, ScaledHeight(static_cast<std::int32_t>(texture_w), static_cast<std::int32_t>(used_w),
                                        static_cast<std::int32_t>(used_h)));
        PutWord(entry + 0, texture_w);
        to.r = static_cast<std::int32_t>(texture_w & 0xFFFF);
        to.b = static_cast<std::int32_t>(Word(entry + 2));
    }
    const U surface = Long(entry + 0x20);
    const U stage = Long(kStage);
    Com(surface, kBlt, &to, stage, &from, kBltWait, 0u);
    PutWord(entry + 4, texture_w);
    PutWord(entry + 6, texture_h);
    Com(Long(kDevice), kBeginScene);
}

// 0x5A37D0. Re-draws D3d_CellTexCache entry `slot`'s cells for a changed
// palette, into the surface it already has: the stage cleared (always -
// never the backdrop, whatever the build had), locked (a failure returns at
// once), the palette Gfx_ClutPixels(clut), the records drawn as the build
// draws them (no extent, no checksum), unlocked; +0x1C the generation of
// Gfx_ClutRows at (clut shr 6) * 8. Then the Blt the build made, from the
// entry's stored extent (s16 +8..+0xE, read after that store) to {0, 0, +0,
// +2}: under Gfx_RenderFlags bit 0 (read after the Unlock) at once, else
// between device EndScene and BeginScene with the entry's surface read after
// the EndScene. The surface must exist: nothing tests +0x20 (D31).
void D3d_RefreshCellTexture(int slot, unsigned first, unsigned count, unsigned clut) {
    Dd_ClearSurface(Ptr(Long(kStage)));
    Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    if (Com(Long(kStage), kLock, kNoRect, desc.b, kLockWait, 0u) != 0) return;
    const void* palette = g.clut_pixels(clut);
    DrawCells(first, count, palette, desc, nullptr);
    Com(Long(kStage), kUnlock, 0u);
    const U entry = kCellCache + static_cast<U>(slot) * kCellEntryBytes;
    PutLong(entry + 0x1C, Long(kClutRows + (clut >> 6) * 8));
    Rect from = {Short(entry + 8) + 0xA0, Short(entry + 0xC) + 0x80, Short(entry + 0xA) + 0xA0,
                 Short(entry + 0xE) + 0x80};
    const U flags = At(kRenderFlags)[0];
    Rect to = {0, 0, static_cast<std::int32_t>(Word(entry + 0)), static_cast<std::int32_t>(Word(entry + 2))};
    if (flags & 1) {
        const U surface = Long(entry + 0x20);
        const U stage = Long(kStage);
        Com(surface, kBlt, &to, stage, &from, kBltWait, 0u);
        return;
    }
    Com(Long(kDevice), kEndScene);
    const U surface = Long(entry + 0x20);
    const U stage = Long(kStage);
    Com(surface, kBlt, &to, stage, &from, kBltWait, 0u);
    Com(Long(kDevice), kBeginScene);
}

void TexCells_Inject() {
    if (bof3::WantsShadow("tex_cells")) tex_cells::SelfTest();
    BOF3_INJECT(D3d_FitTextureSize);
    BOF3_INJECT(Font_BuildGlyphTexture);
    BOF3_INJECT(Dd_ClearSurface);
    BOF3_INJECT(D3d_BuildCellTexture);
    BOF3_INJECT(D3d_FreeCellTexture);
    BOF3_INJECT(D3d_RefreshCellTexture);
    BOF3_INJECT(Font_UnpackGlyph);
    BOF3_INJECT(Cell_Unpack4);
    BOF3_INJECT(Cell_Unpack8);
    BOF3_INJECT(Cell_Unpack4Flip);
    BOF3_INJECT(Cell_Unpack8Flip);
}

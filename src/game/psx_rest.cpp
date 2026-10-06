// The PSX library layer's last seventeen - group PL of the platform round's
// step 2 (docs/platform-layers-plan.md section 4, docs/platform-read-pass.md
// section 2). Each function read to its last instruction with capstone against
// bof3/BOF3.exe, 2026-10-05 (docs/psx-rest.md):
//
//   Gpu_SetPolyF3      0x5A7570  0x17    Display_Teardown        0x5A6380  0x122
//   Gpu_SetPolyFT3     0x5A7590  0x17    Gfx_FreeClutRows        0x5A64B0  0x2C
//   Gpu_SetLineG4      0x5A76F0  0x1A    Gfx_ReleaseTexCache     0x5A64E0  0x55
//   Gpu_SetTexWindow   0x5A7840  0x13    Font_ReleaseTexCache    0x5A6540  0x46
//   Gte_SquareRoot0    0x5A7A90  0xB     Display_ReleaseOrphans  0x5A6590  0x5F
//   Gte_ApplyMatrixSV  0x5A7C70  0x7E    Display_ReleaseBackdrop 0x5A65F0  0x55
//   Display_TextOut    0x5A66B0  0x6D    D3d_FreeCellTextures    0x5A6760  0x17
//   Display_ErrorBox   0x5A6720  0x39    D3d_ReleaseAfterDraw    0x5A6650  0x3E
//   Sound_Shutdown     0x5A6980  0x3D
//
// Faithful: no divergence. Every call out goes through psx_rest::g, so the
// start-up fuzz can stand recorders in for the callees; every COM call goes
// through the object's vtable and every Win32 call through the game's import
// slot, as the original makes it.
#include "game/psx_rest.h"

#include <emmintrin.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/psx_rest_callees.h"
#include "hook/detour.h"

namespace psx_rest {
namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, std::uint16_t v) { std::memcpy(At(address), &v, sizeof v); }
void* Ptr(U address) { return At(Long(address)); }

// `rep stosd` of zeros: n dwords from address, first to last.
void ZeroDwords(U address, U n) {
    for (U i = 0; i < n; ++i) PutLong(address + i * 4, 0);
}

// One COM pointer released through its vtable's +8 if it is not null. The
// caller says whether the slot is zeroed after (inside the test, as the
// originals do it).
void ReleaseIf(U slot, bool zero) {
    void* const object = Ptr(slot);
    if (!object) return;
    Method<ComCall>(object, kRelease)(object);
    if (zero) PutLong(slot, 0);
}

// Win32 through the game's own import slots, as the originals call it.
template <class Fn>
Fn Import(U slot) {
    Fn fn;
    std::memcpy(&fn, At(slot), sizeof fn);
    return fn;
}
unsigned long __stdcall ImportSetTextColor(void* hdc, unsigned long color) {
    return Import<unsigned long (__stdcall*)(void*, unsigned long)>(kImportSetTextColor)(hdc, color);
}
int __stdcall ImportSetBkMode(void* hdc, int mode) {
    return Import<int (__stdcall*)(void*, int)>(kImportSetBkMode)(hdc, mode);
}
int __stdcall ImportTextOutA(void* hdc, int x, int y, const char* text, int n) {
    return Import<int (__stdcall*)(void*, int, int, const char*, int)>(kImportTextOutA)(hdc, x, y, text, n);
}
int __stdcall ImportMessageBoxA(void* hwnd, const char* text, const char* caption, unsigned type) {
    return Import<int (__stdcall*)(void*, const char*, const char*, unsigned)>(kImportMessageBoxA)(hwnd, text, caption,
                                                                                                  type);
}

}  // namespace

const Callees kOriginals = {
    Gfx_FreeClutRows, Gfx_ReleaseTexCache, Font_ReleaseTexCache, Display_ReleaseOrphans, Display_ReleaseBackdrop,
    D3d_FreeCellTextures, D3d_ReleaseAfterDraw,
    Crt_free, D3d_FreeCellTexture, SndStream_Stop, Music_Release,
    ImportSetTextColor, ImportSetBkMode, ImportTextOutA, ImportMessageBoxA,
};
Callees g = kOriginals;

}  // namespace psx_rest

using namespace psx_rest;

// --- The five libgpu setters and the texture window --------------------------

// original 0x5A7570. libgpu SetPolyF3 by its code: byte +7 = 0x20, and 0.01 to
// the three vertices' depth floats at a 0xC stride. The size byte +3 is left
// alone, as in every setter of the port's (docs/psx-library-layer.md section 1).
extern "C" void __cdecl Gpu_SetPolyF3(unsigned char* prim) {
    prim[7] = 0x20;
    for (U at = 0x10; at <= 0x28; at += 0xC) std::memcpy(prim + at, &kPointZeroOne, 4);
}

// original 0x5A7590. SetPolyFT3: code 0x24, the depths at a 0x10 stride.
extern "C" void __cdecl Gpu_SetPolyFT3(unsigned char* prim) {
    prim[7] = 0x24;
    for (U at = 0x10; at <= 0x30; at += 0x10) std::memcpy(prim + at, &kPointZeroOne, 4);
}

// original 0x5A76F0. SetLineG4: code 0x5C, four depths at a 0x10 stride.
extern "C" void __cdecl Gpu_SetLineG4(unsigned char* prim) {
    prim[7] = 0x5C;
    for (U at = 0x10; at <= 0x40; at += 0x10) std::memcpy(prim + at, &kPointZeroOne, 4);
}

// original 0x5A7840. libgpu SetTexWindow(DR_TWIN *, RECT *) as the port has it:
// code 0xF0 in the top byte of +4 (the three bytes below it zero), and at +8
// the RECT's ADDRESS - not the PSX's packed 0xE2 word. Gfx_DrawOTag's case for
// 0xF0 copies the rect from that pointer into Gfx_TexCacheKey
// (docs/d3d-draw.md section 3), so the rect must outlive the frame's draw.
extern "C" void __cdecl Gpu_SetTexWindow(unsigned char* prim, const unsigned char* rect) {
    PutLong(static_cast<U>(reinterpret_cast<std::uintptr_t>(prim + 4)), kTexWindowCode);
    const U address = static_cast<U>(reinterpret_cast<std::uintptr_t>(rect));
    std::memcpy(prim + 8, &address, 4);
}

// --- The two libgte functions -------------------------------------------------

// original 0x5A7A90. libgte SquareRoot0: fild, fsqrt, and a tail jmp to the
// CRT's _ftol 0x5B9550 - the root truncated toward zero, eax the low dword.
// Under the game's control word 0x027F fsqrt is the correctly rounded double
// root (docs/psx-library-layer.md section 3), so sqrtsd is the same: an
// integer below 2^31 has a root whose distance to the next integer is far
// above a double's last bit, so the truncation is the integer square root.
// A negative n gives the NaN, and _ftol of a NaN is the integer indefinite
// 0x8000000000000000, whose low dword is 0.
extern "C" long __cdecl Gte_SquareRoot0(long n) {
    if (n < 0) return 0;
    const double root = _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(static_cast<double>(n))));
    return static_cast<long>(static_cast<std::uint32_t>(static_cast<std::int64_t>(root)));
}

// original 0x5A7C70. libgte ApplyMatrixSV(MATRIX *, SVECTOR *, SVECTOR *): the
// rotation times an SVECTOR, each row three products summed in 32 bits (they
// wrap), >> 12 arithmetic, stored as s16. Every read before the first store;
// the stores z, x, y as the original makes them.
extern "C" void __cdecl Gte_ApplyMatrixSV(const short* matrix, const short* vector, short* out) {
    const std::int32_t x = vector[0], y = vector[1], z = vector[2];
    std::int32_t row[3];
    for (int r = 0; r < 3; ++r) {
        const std::uint32_t sum = static_cast<std::uint32_t>(matrix[3 * r] * x) +
                                  static_cast<std::uint32_t>(matrix[3 * r + 1] * y) +
                                  static_cast<std::uint32_t>(matrix[3 * r + 2] * z);
        row[r] = static_cast<std::int32_t>(sum) >> 12;
    }
    out[2] = static_cast<short>(row[2]);
    out[0] = static_cast<short>(row[0]);
    out[1] = static_cast<short>(row[1]);
}

// --- The teardown's seven -----------------------------------------------------

// original 0x5A64B0. Every Gfx_ClutRows row buffer (the pointer of each of the
// 512 pairs) to Crt_free, and the pair - the generation counter with it -
// zeroed. A pair whose pointer is null is left as it is.
extern "C" void __cdecl Gfx_FreeClutRows() {
    for (U at = kClutRows + 4; at < kClutRowsEnd + 4; at += 8) {
        void* const row = Ptr(at);
        if (!row) continue;
        g.free(row);
        PutLong(at - 4, 0);
        PutLong(at, 0);
    }
}

// original 0x5A64E0. Each of Gfx_TexCache's 32 pages from its first entry to
// its first entry whose state byte is 0: the texture at +0x14 released, then
// the surface at +0x10 - read after that Release - and the 0x18 bytes zeroed.
extern "C" void __cdecl Gfx_ReleaseTexCache() {
    for (U page = kTexCache; page < kTexCacheEnd; page += kTexPageBytes) {
        for (U i = 0; i < 0x20; ++i) {
            const U entry = page + i * kTexEntryBytes;
            if (!At(entry)[0]) break;
            ReleaseIf(entry + 0x14, false);
            ReleaseIf(entry + 0x10, false);
            ZeroDwords(entry, 6);
        }
    }
}

// original 0x5A6540. All 128 of Font_TexCache: the texture +0xC released, then
// the surface +8 (read after), the entry's five dwords zeroed and its glyph
// word +0 set back to 0xFFFF - the set-up's empty value.
extern "C" void __cdecl Font_ReleaseTexCache() {
    for (U entry = kFontCache; entry < kFontCacheEnd; entry += kFontEntryBytes) {
        ReleaseIf(entry + 0xC, false);
        ReleaseIf(entry + 8, false);
        ZeroDwords(entry, 5);
        PutWord(entry, 0xFFFF);
    }
}

// original 0x5A6590. Six COM pointers at 0x7CAE20..0x7CAE37 that no other
// instruction of the image names: +0x10 then +8, +0x14 then +0xC, +4, +0 -
// each released and zeroed when it is not null (docs/psx-rest.md section 2).
extern "C" void __cdecl Display_ReleaseOrphans() {
    for (U at = kOrphans + 8; at < kOrphans + 0x10; at += 4) {
        ReleaseIf(at + 8, true);
        ReleaseIf(at, true);
    }
    ReleaseIf(kOrphans + 4, true);
    ReleaseIf(kOrphans, true);
}

// original 0x5A65F0. The backdrop block: 0x6BEA0C then 0x6BEA04, 0x6BEA10 then
// 0x6BEA08, then Dd_CellBackdrop 0x6BEA00 released (none zeroed by itself);
// then the seven dwords from 0x6BE9F8 zeroed, and 0x6BE9FC = 0x12345678,
// which nothing reads.
extern "C" void __cdecl Display_ReleaseBackdrop() {
    for (U at = kBackdropPairs; at < kBackdropPairs + 8; at += 4) {
        ReleaseIf(at + 8, false);
        ReleaseIf(at, false);
    }
    ReleaseIf(kBackdrop, false);
    ZeroDwords(kBackdropBlock, 7);
    PutLong(kBackdropMagic, kMagic);
}

// original 0x5A6760. D3d_FreeCellTexture of every D3d_CellTexCache slot,
// 0 to 127.
extern "C" void __cdecl D3d_FreeCellTextures() {
    for (U slot = 0; slot < kCellSlots; ++slot) g.free_cell_texture(static_cast<int>(slot));
}

// original 0x5A6650. D3d_AfterDraw's three surfaces - the texture, its
// surface, the capture surface - released, then its seven dwords zeroed.
extern "C" void __cdecl D3d_ReleaseAfterDraw() {
    ReleaseIf(kAfterDrawTexture, false);
    ReleaseIf(kAfterDrawTexSurface, false);
    ReleaseIf(kAfterDrawSurface, false);
    ZeroDwords(kAfterDraw, 7);
}

// --- The teardown, the text, the error box, the sound -------------------------

// original 0x5A6380. The seven above, in the original's order (the cell
// textures before the after-draw block), then each DirectX 6 slot released and
// zeroed: the material, the viewport, the device, the IDirect3D3, the clipper,
// the Z-buffer, the stage, the back buffer, the primary. Then DirectDraw: in
// fullscreen (Gfx_RenderFlags bit 0x100, read after every release before it)
// SetCooperativeLevel(hwnd, DDSCL_NORMAL) on the object as first read, then
// RestoreDisplayMode and Release each on the slot read again; Release; zero.
extern "C" void __cdecl Display_Teardown(void* hwnd) {
    g.free_clut_rows();
    g.release_tex_cache();
    g.release_font_cache();
    g.release_orphans();
    g.release_backdrop();
    g.free_cell_textures();
    g.release_after_draw();
    ReleaseIf(kBackMaterial, true);
    ReleaseIf(kViewport, true);
    ReleaseIf(kDevice, true);
    ReleaseIf(kDirect3D, true);
    ReleaseIf(kClipper, true);
    ReleaseIf(kZBuffer, true);
    ReleaseIf(kStage, true);
    ReleaseIf(kBackBuffer, true);
    ReleaseIf(kPrimary, true);
    void* dd = Ptr(kDirectDraw);
    if (!dd) return;
    if (Long(kRenderFlags) & 0x100) {
        Method<ComCoop>(dd, kSetCooperativeLevel)(dd, hwnd, kDdsclNormal);
        dd = Ptr(kDirectDraw);
        Method<ComCall>(dd, kRestoreDisplayMode)(dd);
        dd = Ptr(kDirectDraw);
    }
    Method<ComCall>(dd, kRelease)(dd);
    PutLong(kDirectDraw, 0);
}

// original 0x5A66B0. GDI text on the back buffer: GetDC on DDraw_BackBuffer as
// read at entry, and nothing more if that fails; else SetTextColor white,
// SetBkMode TRANSPARENT, the text's length counted after them, TextOutA at
// (x, y), and ReleaseDC on DDraw_BackBuffer read again. Nothing reads the
// HRESULT left in eax (the one caller, WinMain's overlay, is ours).
extern "C" void __cdecl Display_TextOut(int x, int y, const char* text) {
    void* const surface = Ptr(kBackBuffer);
    void* hdc = nullptr;   // the original's is the slot `push ecx` made: GetDC writes it (docs/psx-rest.md section 3)
    if (Method<ComGetDC>(surface, kGetDC)(surface, &hdc)) return;
    g.set_text_color(hdc, kWhite);
    g.set_bk_mode(hdc, kTransparent);
    const int n = static_cast<int>(std::strlen(text));
    g.text_out(hdc, x, y, text, n);
    void* const again = Ptr(kBackBuffer);
    Method<ComReleaseDC>(again, kReleaseDC)(again, hdc);
}

// original 0x5A6720. MessageBoxA(NULL, text, the image's caption, MB_ICONHAND)
// with the text from the error table: a code below 100 (signed) indexes
// 0x66B6A8, any other 0x66B568 - which puts 100..103 at the four entries
// after the first table's twenty. Nothing checks the code; nothing reads the
// box's answer.
extern "C" void __cdecl Display_ErrorBox(int code) {
    const U base = code < 100 ? kErrorText : kErrorTextHigh;
    const char* const text = reinterpret_cast<const char*>(Ptr(base + static_cast<U>(code) * 4));
    g.message_box(nullptr, text, reinterpret_cast<const char*>(At(kErrorCaption)), kIconHand);
}

// original 0x5A6980. SndStream_Stop, Music_Release, then the primary sound
// buffer and the DirectSound object - both the real DirectSound's, made by
// the sound set-up 0x5A6830 - each released and zeroed when not null.
extern "C" void __cdecl Sound_Shutdown() {
    g.stream_stop();
    g.music_release();
    ReleaseIf(kSndPrimary, true);
    ReleaseIf(kSndDevice, true);
}

void PsxRest_Inject() {
    if (bof3::WantsShadow("psx_rest")) psx_rest::SelfTest();
    BOF3_INJECT(Gpu_SetPolyF3);
    BOF3_INJECT(Gpu_SetPolyFT3);
    BOF3_INJECT(Gpu_SetLineG4);
    BOF3_INJECT(Gpu_SetTexWindow);
    BOF3_INJECT(Gte_SquareRoot0);
    BOF3_INJECT(Gte_ApplyMatrixSV);
    BOF3_INJECT(Gfx_FreeClutRows);
    BOF3_INJECT(Gfx_ReleaseTexCache);
    BOF3_INJECT(Font_ReleaseTexCache);
    BOF3_INJECT(Display_ReleaseOrphans);
    BOF3_INJECT(Display_ReleaseBackdrop);
    BOF3_INJECT(D3d_FreeCellTextures);
    BOF3_INJECT(D3d_ReleaseAfterDraw);
    BOF3_INJECT(Display_Teardown);
    BOF3_INJECT(Display_TextOut);
    BOF3_INJECT(Display_ErrorBox);
    BOF3_INJECT(Sound_Shutdown);
}

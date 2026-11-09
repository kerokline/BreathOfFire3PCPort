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
void* Ptr(U address) { return At(Long(address)); }

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
    std::uint32_t word;   // the import slot is a 32-bit word: Windows' function on i686, its thunk on x86-64 (design 4)
    std::memcpy(&word, At(slot), sizeof word);
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(word));
}
unsigned long __stdcall ImportSetTextColor(void* hdc, unsigned long color) {
    return Import<unsigned long (__stdcall*)(void*, unsigned long)>(::bof3::Img(PsxRest_ImportSetTextColor_addr))(hdc, color);
}
int __stdcall ImportSetBkMode(void* hdc, int mode) {
    return Import<int (__stdcall*)(void*, int)>(::bof3::Img(PsxRest_ImportSetBkMode_addr))(hdc, mode);
}
int __stdcall ImportTextOutA(void* hdc, int x, int y, const char* text, int n) {
    return Import<int (__stdcall*)(void*, int, int, const char*, int)>(::bof3::Img(PsxRest_ImportTextOutA_addr))(hdc, x, y, text, n);
}
int __stdcall ImportMessageBoxA(void* hwnd, const char* text, const char* caption, unsigned type) {
    return Import<int (__stdcall*)(void*, const char*, const char*, unsigned)>(::bof3::Img(PsxRest_ImportMessageBoxA_addr))(hwnd, text, caption,
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

// --- The two libgte functions -------------------------------------------------

// --- The teardown's seven -----------------------------------------------------

// --- The teardown, the text, the error box, the sound -------------------------

void PsxRest_Inject() {
    if (bof3::WantsShadow("psx_rest")) psx_rest::SelfTest();
}

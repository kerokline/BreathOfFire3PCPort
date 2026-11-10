// The page textures and the DirectDraw surface helpers, read to the last
// instruction with capstone against bof3/BOF3.exe (docs/tex-page.md). All are
// the PC port's own: the PlayStation sampled VRAM on its GPU, so there is no
// PSX twin. Faithful, but for DIV-0159 in Tex_Convert16.
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

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/tex_page_callees.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "render/render_shim.h"

namespace tex_page {

// g_direct_opaque (DIV-0159) is in text/tex.cpp, beside Tex_Convert16, which reads it.

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return Low32(p); }
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

}  // namespace
}  // namespace tex_page

using namespace tex_page;

void TexPage_Inject() {
    if (bof3::WantsShadow("tex_page")) tex_page::SelfTest();
}

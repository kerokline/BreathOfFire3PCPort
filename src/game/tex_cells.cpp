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

using namespace tex_cells;

void TexCells_Inject() {
    if (bof3::WantsShadow("tex_cells")) tex_cells::SelfTest();
}

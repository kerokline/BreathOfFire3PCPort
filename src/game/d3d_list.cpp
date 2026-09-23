// The draw: the port's DrawOTag, 0x59EE50..0x59F29F (0x450 bytes, then its two
// jump tables and their byte index tables to 0x59F518), read to the last
// instruction with capstone against bof3/BOF3.exe (docs/d3d-draw.md section 1).
// Faithful: no divergence. Its handlers are reached through the same addresses
// the original calls, so the ones that are ours (this group's, D3d_DrawGlyph,
// DIV-0010's sprite copies) run as they do under Capcom's draw.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/d3d_list_callees.h"
#include "hook/log.h"

namespace d3d_list {

namespace {
using Handler = long(__cdecl*)(const unsigned char*);
Handler H(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(address)); }
}  // namespace

// The handlers of each table, by entry, as the original's case blocks call them.
const Callees kOriginals = {
    {
        // the software surfaces, first table 0x59F2A0
        H(0x5A3A60), H(0x5A3B60), H(0x5A3D70), H(0x5A3E90), H(0x5A3FB0), H(0x5A40C0), H(0x5A41A0),
        H(0x5A42E0), H(kRetOnly), H(kRetOnly), H(kRetOnly), H(kRetOnly), H(kRetOnly), H(kRetOnly),
        H(0x5A4400), H(0x5A45B0), H(0x5A4500), H(0x5A4900), H(0x5A46E0), H(0x5A47F0), H(0x5A4C40),
    },
    {
        // Direct3D, second table 0x59F3D8
        H(0x59FA50), H(0x59FDB0), H(0x5A0AB0), H(0x5A0C40), H(0x5A0E80), H(0x5A1050), H(0x5A1290),
        H(0x5A14C0), H(0x5A17A0), H(0x5A1A00), H(0x5A1D10), H(0x5A18B0), H(0x5A1B50), H(0x5A1EA0),
        H(0x5A20D0), H(0x5A2300), H(0x5A2220), H(0x5A2900), H(0x5A2520), H(0x5A2710), H(0x5A2EB0),
    },
    Gfx_MoveImage,
    D3d_SetAlphaModulate,
    D3d_AfterDraw,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
U Word(U address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}

void Scene(U offset) {   // 0x24 BeginScene, 0x28 EndScene; the device re-read each time
    void** device = *reinterpret_cast<void** volatile*>(At(kDevice));
    reinterpret_cast<long(__stdcall*)(void*)>((*reinterpret_cast<void***>(device))[offset / 4])(device);
}

// Codes 0xE0..0xE3 and 0xE8..0xEB, both tables: the PSX draw mode. Byte +6 bit
// 0 to Gfx_DrawEnable (which the NEXT draw tests on entry), bit 1 to 0x7DED16,
// the tpage word +4 to Gfx_DrawTpage, and the 8 bytes a non-null +8 points at to
// Gfx_TexCacheKey.
void DrawMode(U p) {
    At(kDrawEnable)[0] = At(p + 6)[0] & 1;
    At(kDrawEnable - 1)[0] = (At(p + 6)[0] >> 1) & 1;
    PutWord(kDrawTpage, Word(p + 4));
    const U key = Long(p + 8);
    if (key != 0) {
        PutLong(kTexKey, Long(key));
        PutLong(kTexKey + 4, Long(key + 4));
    }
}
// Codes 0xF0..0xF3: Gfx_TexCacheKey from the 8 bytes +8 points at - not
// checked for null, as the original does not.
void TexKey(U p) {
    const U key = Long(p + 8);
    PutLong(kTexKey, Long(key));
    PutLong(kTexKey + 4, Long(key + 4));
}
// Codes 0xEC..0xEF: a VRAM move, the rect at +8, the destination +0x10, +0x14.
void MoveImage(U p) {
    g.move_image(reinterpret_cast<short*>(At(p + 8)), static_cast<int>(Long(p + 0x10)),
                 static_cast<int>(Long(p + 0x14)));
}

// One primitive: (code & 0xFC) - 0x20, unsigned, through the byte table to an
// entry. Entries 0..20 are handlers; the software table's 21..23 and the
// Direct3D table's 21..24 are the cases above; the last entry of each is none.
void Dispatch(U p, bool software) {
    const U index = (At(p + 7)[0] & 0xFCu) - 0x20u;
    if (index > (software ? 0xD0u : 0xD4u)) return;
    const U entry = At((software ? kSoftBytes : kD3dBytes) + index)[0];
    if (entry < kHandlers) {
        (software ? g.soft : g.d3d)[entry](At(p));
        return;
    }
    switch (entry) {
    case 21: DrawMode(p); return;
    case 22: MoveImage(p); return;
    case 23: TexKey(p); return;
    case 24:
        if (!software) g.alpha_modulate(At(p + 8)[0] ^ 1u);
        return;
    case 25:
        if (!software) return;
        break;
    default: break;
    }
    bof3::Fatal("Gfx_DrawOTag: byte table entry %u at code 0x%02X", (unsigned)entry, (unsigned)At(p + 7)[0]);
}

}  // namespace
}  // namespace d3d_list

using namespace d3d_list;

// 0x59EE50, called once a rendered frame from WinMain (0x4FCE6F) with the
// ordering table. Nothing at all unless Gfx_DrawEnable. Gfx_RenderFlags bit 0
// (read once) picks the software surfaces' table; otherwise the walk is inside
// BeginScene / EndScene and uses the Direct3D table. The walk: from the table
// pointer, while the link word is not -1, follow it - a link with its top byte
// set is masked to 24 bits and the node it reaches is NOT drawn (known-defects
// D4), a clean one is drawn - and read the drawn node's link after its handler.
// Then every glyph texture and every cell texture is marked unused, and if
// anything set the word 0x7CADEA during the draw (it is cleared on entry),
// D3d_AfterDraw 0x59F580 runs (a tail jump in the original; it reads no
// argument).
void Gfx_DrawOTag(unsigned long* ot) {
    if (At(kDrawEnable)[0] == 0) return;
    if (Word(kAfterDrawFlag) != 0) PutWord(kAfterDrawFlag, 0);
    const bool software = (At(kRenderFlags)[0] & 1) != 0;
    if (!software) Scene(0x24);
    U node = static_cast<U>(reinterpret_cast<std::uintptr_t>(ot));
    while (Long(node) != 0xFFFFFFFFu) {
        U p = Long(node);
        if (p & 0xFF000000u) p &= 0xFFFFFFu;
        else Dispatch(p, software);
        node = p;
    }
    if (!software) Scene(0x28);
    for (U a = kFontInUse; a < kFontInUseEnd; a += 0x14) PutWord(a, 0);
    for (U a = kCellInUse; a < kCellInUseEnd; a += 0x28) PutWord(a, 0);
    if (Word(kAfterDrawFlag) != 0) g.after_draw();
}

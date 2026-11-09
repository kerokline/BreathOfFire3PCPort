// Moved here by tools/move_funcs.py from battle_items.cpp, field_misc.cpp, menu_windows.cpp, psx_gpu.cpp, psx_rest.cpp, scena_sx2.cpp (docs/source-layout-round.md).
#include "game/platform/gpu.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle_items_callees.h"
#include "game/d3d_lines.h"
#include "game/field_misc.h"
#include "game/field_misc_callees.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/map_rim.h"
#include "game/menu_verbs.h"
#include "game/menu_windows.h"
#include "game/menu_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/psx_gpu.h"
#include "game/psx_rest_callees.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sx2_callees.h"
#include "game/scenario_harness.h"
#include "game/shoulder_glyphs.h"
#include "game/sound.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = scena_sx2::at;

}  // namespace

using namespace battle_items;

namespace {

using namespace field_misc;

}  // namespace

using namespace menu_windows;

using namespace psx_rest;

namespace {

// What the setters leave at +0x10 and every 0x10 after it: the float 0.01, as
// psx_gpu.cpp's do (docs/psx-library-layer.md section 1).
constexpr std::uint32_t kPointZeroOneBattleItems = 0x3C23D70Au;

void PutDword(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }

void PutLongFieldMisc(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void Put32(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }

// What the setters leave at +0x10 and on: the float 0.01, as its bits. The
// PSX primitives have no such field; the port's are wider, and unread.
constexpr std::uint32_t kPointZeroOnePsxGpu = 0x3C23D70Au;

constexpr std::uint32_t kLinkFlag = 0x80000000u;

using move_script::SetLong;

}  // namespace

// original 0x5A7540. Puts prim at the head of the list ot heads: the old head
// goes to link_out - prim's own link, for the game's callers - and the new
// head carries bit 31 of what prim's first dword held. Both reads first.
extern "C" void __cdecl Gpu_AddPrim(unsigned long* ot, unsigned long* prim, unsigned long* link_out) {
    const unsigned long flag = *prim & kLinkFlag;
    const unsigned long head = *ot;
    *link_out = head;
    *ot = flag | Low32(prim);
}

// original 0x5A7560. One store: the list's last link now names `item`. The
// caller makes `item` the new tail. 892,889 calls in the attract run.
extern "C" void __cdecl Gpu_LinkPrim(unsigned long* tail, unsigned long item) { *tail = item; }

// original 0x5A7570. libgpu SetPolyF3 by its code: byte +7 = 0x20, and 0.01 to
// the three vertices' depth floats at a 0xC stride. The size byte +3 is left
// alone, as in every setter of the port's (docs/psx-library-layer.md section 1).
extern "C" void __cdecl Gpu_SetPolyF3(unsigned char* prim) {
    prim[7] = 0x20;
    for (psx_rest::U at = 0x10; at <= 0x28; at += 0xC) std::memcpy(prim + at, &kPointZeroOne, 4);
}

// original 0x5A7590. SetPolyFT3: code 0x24, the depths at a 0x10 stride.
extern "C" void __cdecl Gpu_SetPolyFT3(unsigned char* prim) {
    prim[7] = 0x24;
    for (psx_rest::U at = 0x10; at <= 0x30; at += 0x10) std::memcpy(prim + at, &kPointZeroOne, 4);
}

// original 0x5A75B0 (PSX SetPolyF4 0x8017B31C): code 0x28, z at +0x10 +0x1C +0x28 +0x34.
extern "C" unsigned char* __cdecl Gpu_SetPolyF4(unsigned char* prim) {
    prim[7] = 0x28;
    for (field_misc::U z = 0x10; z <= 0x34; z += 0xC) PutLongFieldMisc(prim + z, 0x3C23D70A);
    return prim;
}

// originals 0x5A75D0, 0x5A7650, 0x5A7710, 0x5A7760, 0x5A7770. The primitive's
// GPU code to byte +7, and for three of them 0.01 to a float per vertex.
extern "C" void __cdecl Gpu_SetPolyFT4(unsigned char* prim) {
    prim[7] = 0x2C;
    for (unsigned at = 0x10; at <= 0x40; at += 0x10) PutDword(prim + at, kPointZeroOnePsxGpu);
}

// original 0x5A75F0 (PSX SetPolyG3 0x8017B2F4): code 0x30, 0.01 per vertex.
extern "C" void __cdecl Gpu_SetPolyG3(unsigned char* prim) {
    prim[7] = 0x30;
    for (unsigned k = 0x10; k <= 0x30; k += 0x10) PutDword(prim + k, kPointZeroOneBattleItems);
}

// original 0x5A7610 (PSX SetPolyG4 0x8017B344): code 0x38, z at +0x10 +0x20 +0x30 +0x40.
extern "C" unsigned char* __cdecl Gpu_SetPolyG4(unsigned char* prim) {
    prim[7] = 0x38;
    for (field_misc::U z = 0x10; z <= 0x40; z += 0x10) PutLongFieldMisc(prim + z, 0x3C23D70A);
    return prim;
}

// originals 0x5A7630, 0x5A7690, 0x5A7740 - the three the window layer wants
// (docs/window-task.md): a shaded textured quad at a 0x14-byte vertex stride,
// a four-point line at 0xC, and a tile with one vertex.
extern "C" void __cdecl Gpu_SetPolyGT4(unsigned char* prim) {
    prim[7] = 0x3C;
    for (unsigned at = 0x10; at <= 0x4C; at += 0x14) PutDword(prim + at, kPointZeroOnePsxGpu);
}

extern "C" void __cdecl Gpu_SetLineF2(unsigned char* prim) {
    prim[7] = 0x40;
    PutDword(prim + 0x10, kPointZeroOnePsxGpu);
    PutDword(prim + 0x1C, kPointZeroOnePsxGpu);
}

// original 0x5A7670 (PSX SetLineF3 0x8017B420): code 0x48, z at +0x10 +0x1C
// +0x28. The PSX also writes the polyline's terminator 0x55555555 at +0x14;
// the PC's primitive has its third corner there and no terminator.
extern "C" unsigned char* __cdecl Gpu_SetLineF3(unsigned char* prim) {
    prim[7] = 0x48;
    for (field_misc::U z = 0x10; z <= 0x28; z += 0xC) PutLongFieldMisc(prim + z, 0x3C23D70A);
    return prim;
}

extern "C" void __cdecl Gpu_SetLineF4(unsigned char* prim) {
    prim[7] = 0x4C;
    for (unsigned at = 0x10; at <= 0x34; at += 0xC) PutDword(prim + at, kPointZeroOnePsxGpu);
}

// original 0x5A76B0 (PSX SetLineG2 0x8017B40C): code 0x50, two vertices.
extern "C" void __cdecl Gpu_SetLineG2(unsigned char* prim) {
    prim[7] = 0x50;
    PutDword(prim + 0x10, kPointZeroOneBattleItems);
    PutDword(prim + 0x20, kPointZeroOneBattleItems);
}

// original 0x5A76D0 (PSX SetLineG3 0x8017B440): code 0x58, three vertices.
// The PSX's also stores the polyline's terminator 0x55555555; the port's
// primitive has none.
extern "C" void __cdecl Gpu_SetLineG3(unsigned char* prim) {
    prim[7] = 0x58;
    for (unsigned k = 0x10; k <= 0x30; k += 0x10) PutDword(prim + k, kPointZeroOneBattleItems);
}

// original 0x5A76F0. SetLineG4: code 0x5C, four depths at a 0x10 stride.
extern "C" void __cdecl Gpu_SetLineG4(unsigned char* prim) {
    prim[7] = 0x5C;
    for (psx_rest::U at = 0x10; at <= 0x40; at += 0x10) std::memcpy(prim + at, &kPointZeroOne, 4);
}

extern "C" void __cdecl Gpu_SetSprt(unsigned char* prim) {
    prim[7] = 0x64;
    PutDword(prim + 0x10, kPointZeroOnePsxGpu);
}

// original 0x5A7720 (PSX 0x8017B36C, the sibling's SetSprt8): code 0x74, SPRT_8,
// and the float 0.01 to +0x10.
extern "C" void __cdecl Gpu_SetSprt8(unsigned char* prim) {
    prim[7] = 0x74;
    Put32(prim + 0x10, 0x3C23D70Au);
}

// original 0x5A7730: a primitive's code byte +7 0x7C (the PSX GPU's SPRT_16)
// and the float 0.01 to +0x10 - Gpu_SetSprt / Gpu_SetSprt8's shape. eax is
// left the primitive; no caller reads it. The PSX SetSprt16 0x8017B380 (the
// sibling's Psy-Q signature match; pairs_propagated "call-anchored").
extern "C" void __cdecl Gpu_SetSprt16(unsigned char* prim) {
    prim[7] = 0x7C;
    SetLong(prim + 0x10, static_cast<std::int32_t>(scena_sx2::at::kDepth));
}

extern "C" void __cdecl Gpu_SetTile(unsigned char* prim) {
    prim[7] = 0x60;
    PutDword(prim + 0x10, kPointZeroOnePsxGpu);
}

// original 0x5A7750 (PSX SetTile1 0x8017B3A8): code 0x68, one vertex.
extern "C" void __cdecl Gpu_SetTile1(unsigned char* prim) {
    prim[7] = 0x68;
    PutDword(prim + 0x10, kPointZeroOneBattleItems);
}

extern "C" void __cdecl Gpu_SetCode6C(unsigned char* prim) { prim[7] = 0x6C; }

extern "C" void __cdecl Gpu_SetCode84(unsigned char* prim) { prim[7] = 0x84; }

// originals 0x5A7780, 0x5A77A0. Bits 1 and 0 of the GPU code, from bit 0 of the
// argument's low byte.
extern "C" void __cdecl Gpu_SetSemiTrans(unsigned char* prim, unsigned abe) {
    prim[7] = static_cast<unsigned char>((abe & 1) ? prim[7] | 2 : prim[7] & 0xFD);
}

extern "C" void __cdecl Gpu_SetShadeTex(unsigned char* prim, unsigned tge) {
    prim[7] = static_cast<unsigned char>((tge & 1) ? prim[7] | 1 : prim[7] & 0xFE);
}

// original 0x5A77C0. A draw-mode primitive: 0xE8000000 with the texture page
// in the low word and the two flags in bits 16 and 17 - not the PSX GPU's
// 0xE1 packet - and the texture window word after it.
extern "C" void __cdecl Gpu_SetDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    PutDword(prim + 4, 0xE8000000u | (tpage & 0xFFFFu));
    if (dfe) prim[6] |= 1;
    if (dtd) prim[6] |= 2;
    PutDword(prim + 8, tw);
}

void Gpu_Inject() {
    BOF3_INJECT(Gpu_AddPrim);
    BOF3_INJECT(Gpu_LinkPrim);
    BOF3_INJECT(Gpu_SetPolyF3);
    BOF3_INJECT(Gpu_SetPolyFT3);
    BOF3_INJECT(Gpu_SetPolyF4);
    BOF3_INJECT(Gpu_SetPolyFT4);
    BOF3_INJECT(Gpu_SetPolyG3);
    BOF3_INJECT(Gpu_SetPolyG4);
    BOF3_INJECT(Gpu_SetPolyGT4);
    BOF3_INJECT(Gpu_SetLineF2);
    BOF3_INJECT(Gpu_SetLineF3);
    BOF3_INJECT(Gpu_SetLineF4);
    BOF3_INJECT(Gpu_SetLineG2);
    BOF3_INJECT(Gpu_SetLineG3);
    BOF3_INJECT(Gpu_SetLineG4);
    BOF3_INJECT(Gpu_SetSprt);
    BOF3_INJECT(Gpu_SetSprt8);
    BOF3_INJECT(Gpu_SetSprt16);
    BOF3_INJECT(Gpu_SetTile);
    BOF3_INJECT(Gpu_SetTile1);
    BOF3_INJECT(Gpu_SetCode6C);
    BOF3_INJECT(Gpu_SetCode84);
    BOF3_INJECT(Gpu_SetSemiTrans);
    BOF3_INJECT(Gpu_SetShadeTex);
    BOF3_INJECT(Gpu_SetDrawMode);
}

// Moved here by tools/move_funcs.py from magic_s06.cpp (docs/source-layout-round.md).
#include "game/magic/magic_008.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using move_script::SetLong;

unsigned char* Sc() { return Sprite_Current; }

// A draw-mode packet (tpage `tpage`, dithered) committed to layer `layer`.
void DrawMode(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

constexpr std::uint32_t kF0 = 0;             // 0.0f

constexpr std::uint32_t kF120 = 0x42F00000;  // 120.0f

constexpr std::uint32_t kF359 = 0x43B38000;  // 359.0f

void PutU32(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(v)); }

void PutRgb(unsigned char* at, unsigned char v) { at[0] = at[1] = at[2] = v; }

}  // namespace

#define S06_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4A29C0: a flash over the screen, two semi-transparent gouraud
// quads (x 0..319, the wide frame's under DIV-0041; y 0..120 white at the
// top to shade +9 x 16, then 120..359 back to white), between draw modes tpage 0x35 and 0x15 on layer 1. The shade
// is read once, before the first quad.
S06_EXPORT void __cdecl Magic008_DrawFlash(void) {
    DrawMode(0x35, 1);
    unsigned char* p = Gfx_PacketNext;
    const unsigned char shade = static_cast<unsigned char>(Sc()[9] << 4);
    // DIV-0041: the flash over the wide frame, x Widescreen_FillX()..319 + the
    // columns (the PSP's -32..351); 0..319 while the fills are unarmed.
    const std::uint32_t left = std::bit_cast<std::uint32_t>(Widescreen_FillX());
    const std::uint32_t right = std::bit_cast<std::uint32_t>(Widescreen_FillRight(319.0f));
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutU32(p + 8, left);
    PutU32(p + 0xC, kF0);
    PutU32(p + 0x18, right);
    PutU32(p + 0x1C, kF0);
    PutU32(p + 0x28, left);
    PutU32(p + 0x2C, kF120);
    PutU32(p + 0x38, right);
    PutU32(p + 0x3C, kF120);
    PutRgb(p + 4, 0x80);
    PutRgb(p + 0x14, 0x80);
    PutRgb(p + 0x24, shade);
    PutRgb(p + 0x34, shade);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutU32(p + 8, left);
    PutU32(p + 0xC, kF120);
    PutU32(p + 0x18, right);
    PutU32(p + 0x1C, kF120);
    PutU32(p + 0x28, left);
    PutU32(p + 0x2C, kF359);
    PutU32(p + 0x38, right);
    PutU32(p + 0x3C, kF359);
    PutRgb(p + 4, shade);
    PutRgb(p + 0x14, shade);
    PutRgb(p + 0x24, 0x80);
    PutRgb(p + 0x34, 0x80);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    DrawMode(0x15, 1);
}

void Magic008_Inject() {
    BOF3_INJECT(Magic008_DrawFlash);
}

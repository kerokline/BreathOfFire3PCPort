// Moved here by tools/move_funcs.py from area_w3c.cpp (docs/source-layout-round.md).
#include "game/area/area_132.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3c.h"
#include "game/area_w3c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3c::at;

}  // namespace

namespace {

using move_script::SetLong;

using U = std::uint32_t;

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// A float stored as the originals' `mov dword` of its bits.
void SetFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// Area 132's effects at a fixed cell: a slot from Effect_FindFree, none
// nothing; else its z +0x38 and x +0x34 set (and read back), +0 = 1, +5 =
// kind, the extra bytes `set` (+1 and +0xB, each when not negative), then
// +0x3C = (the ground at (x, z), sign-extended, + `lift`) << 16, and +0x34
// moved by `shift` (read again after the call; only when `shift` is not 0).
// Returns nothing; the caller's slot byte is not read after.
void EffectAtCell(unsigned char kind, std::int32_t x, std::int32_t z, int b1, int bB, std::int32_t lift, std::int32_t shift) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    tE->z = z;
    const std::int32_t zz = tE->z;
    tE->x = x;
    const std::int32_t xx = tE->x;
    e[0] = 1;
    e[5] = kind;
    if (b1 >= 0) e[1] = static_cast<unsigned char>(b1);
    if (bB >= 0) e[0xB] = static_cast<unsigned char>(bB);
    const long ground = AH_CALL(AreaMap_Elevation)(xx, zz);
    const U y = static_cast<U>(static_cast<std::int32_t>(static_cast<short>(ground)) + lift);
    if (shift != 0) {
        const U moved = static_cast<U>(tE->x) + static_cast<U>(shift);
        tE->height = static_cast<std::int32_t>(y << 16);
        tE->x = static_cast<std::int32_t>(moved);
    } else {
        tE->height = static_cast<std::int32_t>(y << 16);
    }
}

}  // namespace

// original 0x41D450 (+0x3C[0], PSX 0x801F3A24): the running object's +8 bit 4:
// animation 0x40, else 0x41.
extern "C" void __cdecl Area132_AnimByBit4(void) { AH_CALL(Sprite_SetAnimation)((Sprite_Current[8] & 4) ? 0x40 : 0x41); }

// original 0x41D470 (+0x3C[1], PSX 0x801F3A64): an effect record of kind 0x73
// at (0x2A0000, 0x1D0000), +1 = 0, on the ground.
extern "C" void __cdecl Area132_Effect73(void) { EffectAtCell(0x73, 0x2A0000, 0x1D0000, 0, -1, 0, 0); }

// original 0x41D4D0 (+0x3C[2], PSX 0x801F3AEC): two records of kind 0x73, at
// (0x2E0000, 0x1C0000) with +1 = 1, +0xB = 1 and at (0x2F0000, 0x1C0000) with
// +1 = 1, +0xB = 0; each 0x200 above the ground and moved 0x8000 back in x.
extern "C" void __cdecl Area132_Effect73Pair(void) {
    EffectAtCell(0x73, 0x2E0000, 0x1C0000, 1, 1, 0x200, -0x8000);
    EffectAtCell(0x73, 0x2F0000, 0x1C0000, 1, 0, 0x200, -0x8000);
}

// original 0x41D5B0 (+0x3C[3], PSX 0x801F3C1C): an effect record of kind 0x74
// at (0x2B0000, 0x280000), on the ground (+1 left as it was).
extern "C" void __cdecl Area132_Effect74(void) { EffectAtCell(0x74, 0x2B0000, 0x280000, -1, -1, 0, 0); }

// original 0x41D610 (+0x3C[4], PSX 0x801F3CA0): Cond_ByteFE = 0 (the gradient
// state below releases its record once it reads 0).
extern "C" void __cdecl Area132_ClearFE(void) { Cond_ByteFE = 0; }

// original 0x41D620 (EffectKind18_States[95], 0x6541E8; Sprite_Current the
// effect record): Cond_ByteFE 0: Effect_Release. Then, with Draw_PassFlags bit
// 2: Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, 0x95, 0), Gfx_CommitPrim(7, 0xC);
// at Gfx_PacketNext (read again) a POLY_G4: Gpu_SetPolyG4, Gpu_SetSemiTrans(0),
// corners (0, 0), (320, 0), (0, 200), (320, 200) as floats, the top two
// vertices' colour (0, 0xC8, 0xFF) and the bottom two's (0, 0, 0x20);
// Gfx_CommitPrim(7, 0x44). A screen-wide gradient.
extern "C" void __cdecl Area132_EffectGradient(void) {
    if (Cond_ByteFE == 0) AH_CALL(Effect_Release)();
    if ((Draw_PassFlags & 4) == 0) return;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    AH_CALL(Gfx_CommitPrim)(7, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyG4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetFloat(p + 0x2C, 200.0f);
    SetFloat(p + 0x3C, 200.0f);
    p[0x15] = 0xC8;
    p[0x05] = 0xC8;
    p[0x16] = 0xFF;
    p[0x06] = 0xFF;
    // DIV-0041: the left corners at -53 and the right at 373 under the wide
    // picture (widescreen.h: 0 until every self-test has run).
    SetFloat(p + 0x08, Widescreen_FillX());
    SetLong(p + 0x0C, 0);
    SetFloat(p + 0x18, 320.0f + static_cast<float>(Widescreen_Fill()));
    SetLong(p + 0x1C, 0);
    SetFloat(p + 0x28, Widescreen_FillX());
    SetFloat(p + 0x38, 320.0f + static_cast<float>(Widescreen_Fill()));
    p[0x14] = 0;
    p[0x04] = 0;
    p[0x34] = 0;
    p[0x24] = 0;
    p[0x35] = 0;
    p[0x25] = 0;
    p[0x36] = 0x20;
    p[0x26] = 0x20;
    AH_CALL(Gfx_CommitPrim)(7, 0x44);
}

void Area132_Inject() {
    BOF3_INJECT(Area132_AnimByBit4);
    BOF3_INJECT(Area132_Effect73);
    BOF3_INJECT(Area132_Effect73Pair);
    BOF3_INJECT(Area132_Effect74);
    BOF3_INJECT(Area132_ClearFE);
    BOF3_INJECT(Area132_EffectGradient);
}

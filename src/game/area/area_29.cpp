// Moved here by tools/move_funcs.py from worldmap_area.cpp (docs/source-layout-round.md).
#include "game/area/area_29.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/worldmap_area_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace worldmap_area;

namespace worldmap_area {

using move_script::At;

using move_script::Long;

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace worldmap_area

namespace {

using U = std::uint32_t;

}  // namespace

// worldmap_area's callee seam, moved here from worldmap_area.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace worldmap_area {
namespace {

template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Rand,
    AreaMap_Elevation,
    Fn<void (__cdecl*)(int, int, int)>(::bof3::Code(kSetCell.v)),
    ScriptFlags_Set40,
    Fn<void (__cdecl*)()>(::bof3::Code(kFlagsClear40.v)),
    Msg_OpenScript,
    AreaMap_ByteAt,
    Sprite_SetAnimation,
    WorldMap_PinSprite,
    Sprite_QueueOverlay,
    Effect_Release,
    WorldMap_FrameStep,
    WorldMapHud_BoxStep,
    WorldMap_DrawHud,
    WorldMap_RecordIndex,
    Sprite_SetAnimationBank,
    Sprite_ScriptTick,
    Sprite_UpdateScreen,
    Gpu_SetPolyFT4,
    Gpu_SetShadeTex,
    Gpu_SetSemiTrans,
    As<long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*, long*,
                       long*)>(&Gte_RotTransPers4),
    Gte_PrimDepths4_10,
    Prim_SetTexture,
    Gfx_CommitPrim,
    MapView_ItemHalfAt,
    MapView_LinkPrimAt,
};
Callees g = kOriginals;

}  // namespace worldmap_area

// original 0x4037B0 (area 29's descriptor init, +0x40 of 0x5EE270, called by
// Area_Enter; PSX 0x801F2D2C, the sibling's names/area_records.toml): a five-
// byte `jmp 0x4037C0`, then the body. Rand() & 0x3F picks one of eight field
// objects by the chances at 0x5EE268 (16 12 12 8 8 4 2 2: they sum to 64, so
// the "none" index 8 is not reached with this table); the other seven of the
// first eight Sprite_Objects records (0xA4 bytes) get byte +0 = 0. The kept
// one is put at one of eight cells, Rand() & 7 into the (x, z) byte pairs at
// 0x5EE258: +0x34 = x << 16, +0x38 = z << 16, and +0x3E the word of
// AreaMap_Elevation(+0x34 as stored, z << 16). Last, whether or not one was
// kept, Field_StepCount = the low word of (the leader's dword +0x134) - 5.
//
// As the original has it: the roll and the subtraction are byte-wide; the
// second Rand's low byte is taken (`mov [esp + 4], al`, then & 0xFF).
extern "C" void __cdecl Area29_PickFieldObject(void) {
    unsigned roll = static_cast<unsigned>(g.rand()) & 0x3F;
    unsigned keep = 0;
    for (; keep < 8; ++keep) {
        const unsigned chance = Area29_Weights[keep];
        if (roll < chance) break;
        roll = (roll - chance) & 0xFF;
    }
    for (unsigned i = 0; Sprite_Objects_addr + i * at::kFieldObjectSize < at::kFieldObjectsEnd; ++i)
        if (i != keep) At(::bof3::Img(Sprite_Objects_addr) + i * at::kFieldObjectSize)[0] = 0;
    if (keep < 8) {
        const unsigned cell = static_cast<unsigned>(g.rand()) & 7;
        records::SpriteObject* const o = rv::As<records::SpriteObject>(At(::bof3::Img(Sprite_Objects_addr) + keep * at::kFieldObjectSize));
        const U x = static_cast<std::uint32_t>(Area29_Cells[cell * 2]) << 16;
        const U z = static_cast<std::uint32_t>(Area29_Cells[cell * 2 + 1]) << 16;
        o->x = static_cast<std::int32_t>(x);
        o->z = static_cast<std::int32_t>(z);
        const long height = g.elevation(o->x, static_cast<long>(z));
        o->height_whole = static_cast<unsigned>(height);
    }
    Field_StepCount = static_cast<unsigned short>(Long(At(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

void Area29_Inject() {
    BOF3_INJECT(Area29_PickFieldObject);
}

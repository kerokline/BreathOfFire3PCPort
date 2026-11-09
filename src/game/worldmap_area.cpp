// Group DA of the eighth round (docs/takeover-queue-round8.md, docs/worldmap_area.md):
// thirty functions the world-map and combat routes enter only through pointers.
// Each was read to its last instruction with capstone against bof3/BOF3.exe on
// 2026-09-25; every one is a faithful replacement, so no DIVERGENCE.md entry
// is owed. The start-up fuzz is worldmap_area_fuzz.cpp.
//
//   Area29_PickFieldObject    0x4037B0 (0xCB)   area 29's descriptor init (+0x40), PSX 0x801F2D2C
//   Area33_ClearCellsA        0x403CE0 (0x5F)   area 33's handler 0 (Area33_Handlers), PSX 0x801F2C38
//   Area33_ClearCellsB        0x403D40 (0x25)   area 33's handler 1, PSX 0x801F2CD0
//   WorldMap33_PlaceMessage   0x403D70 (0x87)   WorldMap_FieldHooks entry 1
//   WorldMap33_PlateRun       0x403E00 (0xD6)   world-map record 1 +0x0 (effect kind 0 on the Yraall map)
//   WorldMap33_PlateShow      0x403EE0 (0x142)  WorldMap33_PlateStates 1
//   WorldMap33_PlateGrow      0x404030 (0x41)   ... 2
//   WorldMap33_PlateHold      0x404080 (0x58)   ... 3
//   WorldMap33_PlateShrink    0x4040E0 (0x50)   ... 4
//   WorldMapHud_Run           0x404130 (0x12)   world-map record 1 +0xC (effect kind 0x58)
//   WorldMapHud_Frame         0x404150 (0xA)    WorldMapHud_States 1
//   WorldMapHud_BoxStep       0x404230 (0x12)   WorldMapHud_Frame's tail jump
//   WorldMapHud_BoxSlideIn    0x404250 (0x62)   WorldMapHud_BoxStates 1
//   WorldMapHud_BoxHold       0x4042C0 (0x6E)   ... 2
//   WorldMapHud_BoxSlideOut   0x404330 (0x57)   ... 3
//   WorldMap33_DrawDrift      0x4048E0 (0x462)  world-map record 1 +0x10
//   WorldMap_FrameWait        0x411310 (0x27)   WorldMap_FrameStep's state 0, shared by eleven tables
//   WorldMapHud_BoxWait       0x414BB0 (0x38)   WorldMapHud_BoxStates 0, shared by eleven
//   WorldMapHud_Start         0x419110 (0x1D)   WorldMapHud_States 0, shared by eleven
//   WorldMap_RecordIndex      0x462A90 (0x26)   the world-map record of Game_AreaNumber
//   EffectKind00_WorldMap     0x462AE0 (0x1A)   Effect_KindHandlers 0x00
//   EffectKind58_WorldMap     0x462B40 (0x1A)   Effect_KindHandlers 0x58
//   WorldMap_RecordHook10     0x462B80 (0x1A)   EffectKind18_States 2 and 3
//   EffectKind06_Run          0x469BB0 (0x12)   Effect_KindHandlers 0x06
//   EffectKind06_Start        0x469BD0 (0xD7)   EffectKind06_States 0, PSX 0x8019B404
//   EffectKind06_Tick         0x469CB0 (0x12)   EffectKind06_States 1, PSX 0x8019B544
//   EffectKind06_Blink        0x469CD0 (0x3D)   EffectKind06_Ticks 0..2, 4, PSX 0x8019B588
//   EffectKind06_End          0x469DB0 (0x22)   EffectKind06_States 2, PSX 0x8019B7DC
//   EffectKind18_Run          0x46D830 (0x12)   Effect_KindHandlers 0x18
//   EffectKind18_Start        0x46D850 (0x32)   EffectKind18_States 0
//
// Every call goes through worldmap_area::g (worldmap_area_callees.h); the
// dispatches through .data tables read the table in place, as the originals
// do (unchecked), so the fuzz can swap the entries.
#include "game/worldmap_area.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/worldmap_area_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "hook/detour.h"
#include "hook/log.h"

namespace worldmap_area {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
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

using namespace worldmap_area;

namespace {

using U = std::uint32_t;

// The object being run, re-read wherever the original re-reads it.
unsigned char* Cur() { return Sprite_Current; }

// A handler read from a .data table in place, as the originals' `call` /
// `jmp [index * 4 + table]` do: the index is not checked.
Handler EntryWorldmapArea(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(worldmap_area::Long(worldmap_area::At(address))))); }

// The region box's leave test the HUD machine's states share (0x404274 ..
// 0x404297 and its copies): the map's mode byte set while the object's +0xB
// is, or Field_Request 2, or bit 8 of Field_ScriptFlags (read as a dword,
// `test ch, 1`).
bool BoxLeaves(const unsigned char* o) {
    if (AreaW0b_MapMode != 0 && o[0xB] != 0) return true;
    if (Field_Request == 2) return true;
    return (Field_ScriptFlags & 0x100) != 0;
}

}  // namespace

// ===========================================================================
// Area 29 and area 33's handlers
// ===========================================================================

// ===========================================================================
// The place plate (effect kind 0 on the Yraall map, world-map record 1 +0)
// ===========================================================================

// ===========================================================================
// The map's HUD task (effect kind 0x58 on the Yraall map, record 1 +0xC)
// ===========================================================================

// ===========================================================================
// The drift layer (world-map record 1 +0x10, through WorldMap_RecordHook10)
// ===========================================================================

// ===========================================================================
// The world-map records and the effect kinds that go through them
// ===========================================================================

// ===========================================================================
// Effect kind 6 (the combat route's) and kind 0x18
// ===========================================================================

// ===========================================================================

void WorldmapArea_Inject() {
    if (bof3::WantsShadow("worldmap_area")) worldmap_area::SelfTest();
}

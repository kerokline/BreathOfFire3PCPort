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

// original 0x462A90 (4,560 calls on the world-map route): the index of the
// first of the eleven 0x1C-byte WorldMap_Records whose area byte +0x18 is
// Game_AreaNumber (u16 against the zero-extended byte), or 11 when none -
// the whole eax. The records are the eleven world maps (areas 16, 33, 45, 65,
// 87, 88, 104, 115, 121, 151, 152), each with five code pointers and a data
// pointer; the callers take the index's low byte.
extern "C" unsigned __cdecl WorldMap_RecordIndex(void) {
    const unsigned area = Game_AreaNumber;
    unsigned index = 0;
    for (U a = ::bof3::Img(WorldMap_Records_addr) + 0x18; a < ::bof3::Img(at::kRecordAreasEnd); a += at::kRecordSize, ++index)
        if (At(a)[0] == area) break;
    return index;
}

namespace {
// A world-map record's code pointer at `slot`, by WorldMap_RecordIndex's low
// byte, read in place. Index 11 (no world map) reads the dwords after the
// eleventh record - EffectKind01_States 0x653A44 of EffectKind01_Run 0x462BA0 (0x462BC0, 0x462BF0,
// 0x462E70, 0x462EB0, EffectKind07_Hold, ...) - as the original does; record 6 (area
// 104) holds nulls at +4, +8 and +0x14.
Handler RecordEntry(U slot) {
    const U index = g.record_index() & 0xFF;
    return EntryWorldmapArea(::bof3::Img(WorldMap_Records_addr) + index * at::kRecordSize + slot);
}
}  // namespace

// original 0x462AE0 (Effect_KindHandlers 0x00): a tail jump to the world-map
// record's +0 (WorldMap33_PlateRun on the Yraall map).
extern "C" void __cdecl EffectKind00_WorldMap(void) { RecordEntry(0)(); }

// original 0x462B40 (Effect_KindHandlers 0x58): the record's +0xC
// (WorldMapHud_Run on the Yraall map).
extern "C" void __cdecl EffectKind58_WorldMap(void) { RecordEntry(0xC)(); }

// original 0x462B80 (EffectKind18_States 2 and 3): the record's +0x10
// (WorldMap33_DrawDrift on the Yraall map).
extern "C" void __cdecl WorldMap_RecordHook10(void) { RecordEntry(0x10)(); }

// ===========================================================================
// Effect kind 6 (the combat route's) and kind 0x18
// ===========================================================================

// original 0x469BB0 (Effect_KindHandlers 0x06): EffectKind06_States 0x653EC8
// by +1 - _Start, _Tick, _End.
extern "C" void __cdecl EffectKind06_Run(void) { EntryWorldmapArea(::bof3::Img(EffectKind06_States_addr) + rv::As<records::EffectObject>(Cur())->state * 4u)(); }

// original 0x469BD0 (EffectKind06_States 0 and 0x653EF4's entry 0; PSX
// 0x8019B404): Sprite_SetAnimationBank(0x18); +0x48 = 0; the word +0x2E +=
// the word +0xC, +0x30 -= the word +0x10; +0x24 = 0, +0x2A = 0, +0x29 = 2.
// Variant +6 == 3: byte +0 |= 0x20, +0x5C = 1, +0x5F / +0x5E / +0x5D = 0xF8,
// +9 = 0x14; any other: +0x5C and the three = 0, +9 = 0x10. Then
// Sprite_SetAnimation(EffectKind06_Anims[+6] - 0 0 2 1 3 4, unchecked) and
// +1 = 1.
extern "C" void __cdecl EffectKind06_Start(void) {
    g.set_bank(0x18);
    Cur()[0x48] = 0;
    rv::As<records::EffectObject>(Cur())->screen_x = static_cast<std::uint16_t>(rv::As<records::EffectObject>(Cur())->screen_x) + worldmap_area::Word(Cur() + 0xC);
    rv::As<records::EffectObject>(Cur())->screen_y = static_cast<std::uint16_t>(rv::As<records::EffectObject>(Cur())->screen_y) - worldmap_area::Word(Cur() + 0x10);
    Cur()[0x24] = 0;
    Cur()[0x2A] = 0;
    Cur()[0x29] = 2;
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    if (o->param == 3) {
        o->flags = static_cast<unsigned char>(o->flags | 0x20);
        Cur()[0x5C] = 1;
        rv::As<records::EffectObject>(Cur())->shade[2] = 0xF8;
        rv::As<records::EffectObject>(Cur())->shade[1] = 0xF8;
        rv::As<records::EffectObject>(Cur())->shade[0] = 0xF8;
        rv::As<records::EffectObject>(Cur())->timer = 0x14;
    } else {
        rv::Bytes(o)[0x5C] = 0;
        rv::As<records::EffectObject>(Cur())->shade[2] = 0;
        rv::As<records::EffectObject>(Cur())->shade[1] = 0;
        rv::As<records::EffectObject>(Cur())->shade[0] = 0;
        rv::As<records::EffectObject>(Cur())->timer = 0x10;
    }
    g.set_animation(worldmap_area::At(::bof3::Img(EffectKind06_Anims_addr) + rv::As<records::EffectObject>(Cur())->param)[0]);
    rv::As<records::EffectObject>(Cur())->state = 1;
}

// original 0x469CB0 (EffectKind06_States 1; PSX 0x8019B544): EffectKind06_Ticks
// 0x653EDC by the variant +6 - _Blink for 0, 1, 2 and 4, 0x469D40 for 3,
// 0x469D10 for 5 (EffectKind06_Fade, EffectKind06_PlayOnce: field_c1.cpp).
extern "C" void __cdecl EffectKind06_Tick(void) { EntryWorldmapArea(::bof3::Img(EffectKind06_Ticks_addr) + rv::As<records::EffectObject>(Cur())->param * 4u)(); }

// original 0x469CD0 (PSX 0x8019B588): +9 - 1; at 0 +1 + 1 and nothing more.
// Otherwise Sprite_ScriptTick, and - only while bit 2 of +9 is set, so it
// blinks - a tail jump to Sprite_QueueOverlay when the kind +5 is 6, else to
// Sprite_UpdateScreen. Sprite_ScriptTick's answer is not read.
extern "C" void __cdecl EffectKind06_Blink(void) {
    rv::As<records::EffectObject>(Cur())->timer = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->timer - 1);
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (o->timer == 0) {
        o->state = static_cast<unsigned char>(o->state + 1);
        return;
    }
    g.script_tick();
    o = rv::As<records::EffectObject>(Cur());
    if ((o->timer & 4) == 0) return;
    if (o->kind == 6) g.queue_overlay();
    else g.update_screen();
}

// original 0x469DB0 (EffectKind06_States 2; PSX 0x8019B7DC): +0x5F, +0x5E,
// +0x5D = 0, then a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind06_End(void) {
    rv::As<records::EffectObject>(Cur())->shade[2] = 0;
    rv::As<records::EffectObject>(Cur())->shade[1] = 0;
    rv::As<records::EffectObject>(Cur())->shade[0] = 0;
    g.effect_release();
}

// original 0x46D830 (Effect_KindHandlers 0x18): EffectKind18_States 0x65406C
// by +1.
extern "C" void __cdecl EffectKind18_Run(void) { EntryWorldmapArea(::bof3::Img(EffectKind18_States_addr) + rv::As<records::EffectObject>(Cur())->state * 4u)(); }

// original 0x46D850 (EffectKind18_States 0): +1 = +0xB (the effect's sub-kind
// is its next state), +2 = +3 = +4 = +9 = 0.
extern "C" void __cdecl EffectKind18_Start(void) {
    rv::As<records::EffectObject>(Cur())->state = Cur()[0xB];
    rv::As<records::EffectObject>(Cur())->step = 0;
    Cur()[3] = 0;
    Cur()[4] = 0;
    rv::As<records::EffectObject>(Cur())->timer = 0;
}

// ===========================================================================

void WorldmapArea_Inject() {
    if (bof3::WantsShadow("worldmap_area")) worldmap_area::SelfTest();
    BOF3_INJECT(WorldMap_RecordIndex);
    BOF3_INJECT(EffectKind00_WorldMap);
    BOF3_INJECT(EffectKind58_WorldMap);
    BOF3_INJECT(WorldMap_RecordHook10);
    BOF3_INJECT(EffectKind06_Run);
    BOF3_INJECT(EffectKind06_Start);
    BOF3_INJECT(EffectKind06_Tick);
    BOF3_INJECT(EffectKind06_Blink);
    BOF3_INJECT(EffectKind06_End);
    BOF3_INJECT(EffectKind18_Run);
    BOF3_INJECT(EffectKind18_Start);
}

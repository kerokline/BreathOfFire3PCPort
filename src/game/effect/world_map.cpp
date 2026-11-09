// Moved here by tools/move_funcs.py from effect_1a.cpp, rest_4f.cpp, worldmap_area.cpp (docs/source-layout-round.md).
#include "game/effect/world_map.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/config_text.h"
#include "game/effect/world_map_callees.h"
#include "game/effect_1a.h"
#include "game/effect_1a_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/fishing_text.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4f.h"
#include "game/rest_4f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "game/worldmap_area.h"
#include "game/worldmap_area_callees.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = world_map::at;

}  // namespace

using namespace worldmap_area;

namespace {

using U = std::uint32_t;

using move_script::At;

U L(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U L(U a) { return L(At(a)); }

using Handler = void (__cdecl*)();

// A world map record's handler at `slot`, by WorldMap_RecordIndex's low byte;
// index 11 ("none") reads the dwords after the eleventh record, which are
// kind 1's and kind 7's state tables - the original's read, kept.
void RecordHandler(const char* who, U slot) {
    const U index = SH_CALL(WorldMap_RecordIndex)() & 0xFF;
    if (index >= effect_1a::at::kRecordCount)
        bof3::Fatal("%s: WorldMap_RecordIndex answered %u, past the twelve rows (docs/effect_1a.md section 6)", who,
                    (unsigned)index);
    const U handler = L(slot + index * effect_1a::at::kRecordSize);
    if (handler == 0)
        bof3::Fatal("%s: world map record %u has no handler at +0x%X (area 104's record) - the original jumps to 0 "
                    "(docs/effect_1a.md section 6)",
                    who, (unsigned)index, (unsigned)(slot - ::bof3::Img(0x653910)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(handler))();
}

using move_script::Long;

U LRest4f(U a) { return static_cast<U>(Long(move_script::At(a))); }

}  // namespace

namespace worldmap_area {

using move_script::At;

using move_script::Long;

}  // namespace worldmap_area

namespace {

// A handler read from a .data table in place, as the originals' `call` /
// `jmp [index * 4 + table]` do: the index is not checked.
Handler EntryWorldmapArea(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(worldmap_area::Long(worldmap_area::At(address))))); }

// A world-map record's code pointer at `slot`, by WorldMap_RecordIndex's low
// byte, read in place. Index 11 (no world map) reads the dwords after the
// eleventh record - EffectKind01_States 0x653A44 of EffectKind01_Run 0x462BA0 (0x462BC0, 0x462BF0,
// 0x462E70, 0x462EB0, EffectKind07_Hold, ...) - as the original does; record 6 (area
// 104) holds nulls at +4, +8 and +0x14.
Handler RecordEntry(U slot) {
    const U index = g.record_index() & 0xFF;
    return EntryWorldmapArea(::bof3::Img(WorldMap_Records_addr) + index * worldmap_area::at::kRecordSize + slot);
}

}  // namespace

// original 0x462A90 (4,560 calls on the world-map route): the index of the
// first of the eleven 0x1C-byte WorldMap_Records whose area byte +0x18 is
// Game_AreaNumber (u16 against the zero-extended byte), or 11 when none -
// the whole eax. The records are the eleven world maps (areas 16, 33, 45, 65,
// 87, 88, 104, 115, 121, 151, 152), each with five code pointers and a data
// pointer; the callers take the index's low byte.
extern "C" unsigned __cdecl WorldMap_RecordIndex(void) {
    const unsigned area = Game_AreaNumber;
    unsigned index = 0;
    for (U a = ::bof3::Img(WorldMap_Records_addr) + 0x18; a < ::bof3::Img(worldmap_area::at::kRecordAreasEnd); a += worldmap_area::at::kRecordSize, ++index)
        if (At(a)[0] == area) break;
    return index;
}

// original 0x462AC0 (called by Field_ExitFromCell): WorldMap_Records' +0x14 of
// the record WorldMap_RecordIndex's low byte names - the area's exit records
// (x, z, area, kind). Index 11 ("none") reads the dword after the eleventh
// record, EffectKind07_States[3] (a code pointer, as effect_1a.cpp's dispatchers
// read their +4 / +8 there); ours aborts past 11. Area 104's record holds 0.
extern "C" const unsigned char* __cdecl WorldMap_ExitRecords(void) {
    const U index = SH_CALL(WorldMap_RecordIndex)() & 0xFF;
    if (index >= rest_4f::at::kRecordCount)
        bof3::Fatal("WorldMap_ExitRecords: WorldMap_RecordIndex answered %u, past the twelve rows (docs/rest_4f.md "
                    "section 7)",
                    (unsigned)index);
    return At(LRest4f(::bof3::Img(rest_4f::at::kExitRecords.v) + index * rest_4f::at::kRecordSize));
}

// original 0x462AE0 (Effect_KindHandlers 0x00): a tail jump to the world-map
// record's +0 (WorldMap33_PlateRun on the Yraall map).
extern "C" void __cdecl EffectKind00_WorldMap(void) { RecordEntry(0)(); }

// original 0x462B00 (Effect_KindHandlers 0xE): the world map record's +4
// handler by WorldMap_RecordIndex's low byte (Area33_Record04Run on Yraall's).
extern "C" void __cdecl EffectKind0E_WorldMap(void) { RecordHandler("EffectKind0E_WorldMap", ::bof3::Img(WorldMap_Records_addr) + 0x4); }

// original 0x462B20 (Effect_KindHandlers 0x16): the record's +8 handler.
extern "C" void __cdecl EffectKind16_WorldMap(void) { RecordHandler("EffectKind16_WorldMap", ::bof3::Img(WorldMap_Records_addr) + 0x8); }

// original 0x462B40 (Effect_KindHandlers 0x58): the record's +0xC
// (WorldMapHud_Run on the Yraall map).
extern "C" void __cdecl EffectKind58_WorldMap(void) { RecordEntry(0xC)(); }

// original 0x462B60 (Effect_KindHandlers 0x5C): Area104_Kind5CRun in area
// 0x68, Area121_Kind5CRun anywhere else (both tail jumps).
extern "C" void __cdecl EffectKind5C_Run(void) {
    if (Game_AreaNumber == 0x68)
        SH_CALL(Area104_Kind5CRun)();
    else
        SH_CALL(Area121_Kind5CRun)();
}

// original 0x462B80 (EffectKind18_States 2 and 3): the record's +0x10
// (WorldMap33_DrawDrift on the Yraall map).
extern "C" void __cdecl WorldMap_RecordHook10(void) { RecordEntry(0x10)(); }

void EffectWorldMapFromEffect1a_Inject() {
    BOF3_INJECT(EffectKind0E_WorldMap);
    BOF3_INJECT(EffectKind16_WorldMap);
    BOF3_INJECT(EffectKind5C_Run);
}

void EffectWorldMapFromRest4f_Inject() {
    BOF3_INJECT(WorldMap_ExitRecords);
}

void EffectWorldMapFromWorldmapArea_Inject() {
    BOF3_INJECT(WorldMap_RecordIndex);
    BOF3_INJECT(EffectKind00_WorldMap);
    BOF3_INJECT(EffectKind58_WorldMap);
    BOF3_INJECT(WorldMap_RecordHook10);
}

// Moved here by tools/move_funcs.py from worldmap_area.cpp (docs/source-layout-round.md).
#include "game/area/world_map_hud.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/worldmap_area.h"
#include "game/worldmap_area_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace worldmap_area;

namespace worldmap_area {

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace worldmap_area

namespace {

// The object being run, re-read wherever the original re-reads it.
unsigned char* Cur() { return Sprite_Current; }

}  // namespace

// original 0x419110 (WorldMapHud_States 0; the same entry in the eleven
// world-map copies' tables): +9 = 0, +0xB = 0, +1 + 1.
extern "C" void __cdecl WorldMapHud_Start(void) {
    rv::As<records::EffectObject>(Cur())->timer = 0;
    Cur()[0xB] = 0;
    rv::As<records::EffectObject>(Cur())->state = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->state + 1);
}

void WorldMapHud_Inject() {
    BOF3_INJECT(WorldMapHud_Start);
}

// Moved here by tools/move_funcs.py from area_w1b.cpp (docs/source-layout-round.md).
#include "game/area/area_47.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1b::at;

}  // namespace

namespace {

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

}  // namespace

// original 0x408F60 (area 47's handler 0, Area47_Handlers 0x5F8324):
// Sprite_Current +0x34 and Field_Kind2X = 0x120000, +0x38 and Field_Kind2Z =
// 0x240000; Field_ViewReset; the word +0x3E = 0x1220 and
// MapView_SetElevation(that word, read back - pushed with Field_ViewReset's
// edx above it, which MapView_SetElevation never reads: ours passes the word
// sign-extended); Camera_Angles[0] = 0xFB76, Camera_Angles[2] = 0xF8.
extern "C" void __cdecl Area47_PlaceAtStart(void) {
    rv::Current<records::SpriteObject>()->x = 0x120000;
    Field_Kind2X = 0x120000;
    rv::Current<records::SpriteObject>()->z = 0x240000;
    Field_Kind2Z = 0x240000;
    AH_CALL(Field_ViewReset)();
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x1220);
    AH_CALL(MapView_SetElevation)(S16(Cur() + 0x3E));
    Camera_Angles[0] = static_cast<short>(0xFB76);
    Camera_Angles[2] = 0xF8;
}

// original 0x408FC0 (area 47's handler 1): Effect_FindFree; a slot: its
// record +0 = 1, kind +5 = 0x3D.
extern "C" void __cdecl Area47_SpawnEffect3D(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x3D;
}

void Area47_Inject() {
    BOF3_INJECT(Area47_PlaceAtStart);
    BOF3_INJECT(Area47_SpawnEffect3D);
}

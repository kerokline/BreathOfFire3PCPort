// The area's link list and the drop-in party placement - five functions of
// the field overlay's PC twin, each read to its last instruction against the
// PSX (docs/area-entry.md section 1 has the pairs). The start-up fuzz is
// area_entry_fuzz.cpp.
#include "game/area_entry.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/area_entry_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"

namespace area_entry {

const Callees kOriginals = {
    Flags_Test,         Area_ClassifyPending, Area_PickMusic,     Kind2_Place,       Member_ClearState, ScriptContext_Reset,
    Sprite_LoadPalette, Field_MemberSprite,   Party_SetUpMembers, Party_SwapMembers, ObjTrio_SwapFields,
};
Callees g = kOriginals;

}  // namespace area_entry

namespace {

using namespace move_script;
using namespace area_entry;

unsigned char* Ptr(const unsigned char* at) { return move_script::At(static_cast<std::uint32_t>(move_script::Long(at))); }
unsigned char* Descriptor() { return Area_Descriptors[Game_AreaNumber]; }

}  // namespace

void AreaEntry_Inject() {
}

// The movement commands and the party's moves - sixteen functions the movement
// script's interpreter (move_script.cpp, move_groups.cpp) and the field object
// update (field_objects.cpp) call, and the demo's party reset. Each is the PSX
// twin's where one was read (docs/move-cmds.md section 1 has the pairs). The
// start-up fuzz is move_cmds_fuzz.cpp.
#include "game/move_cmds.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_cmds_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace move_cmds {

const Callees kOriginals = {
    AreaMap_Elevation, Math_Ratan2, ScriptContext_Reset, MapView_SetElevation, MoveCmd_AttachOffset,
    MoveScript_ObjectKind, MoveCmd_Detach, MoveCmd_AttachMove, MoveCmd_HandlePosition, PartySet_Load, Party_Join,
    AreaMap_Slope,
};
Callees g = kOriginals;

}  // namespace move_cmds

namespace {

using namespace move_script;
using namespace move_cmds;

std::uint32_t AddressMoveCmds(const volatile void* p) { return Low32(p); }
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

// Sprite_Current may be a Sprite_Objects record or another family whose
// first 0x80 bytes are a sprite object's (an ObjTrio member, the kind-2
// object), so only that part is named: rv::Current<records::SpriteHead>()
// (C2 audit frs F3; typed-data-round.md 3, "C2 audit as run").

}  // namespace

void MoveCmds_Inject() {
    if (bof3::WantsShadow("move_cmds")) move_cmds::SelfTest();
}

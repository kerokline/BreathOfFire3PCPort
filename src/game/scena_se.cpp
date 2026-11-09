// The engine-side helpers the scenario chapters share - round ten, group SE,
// taken with the scenario harness (scenario_harness.h). docs/scena_se.md has
// each function, who calls it, and why three of the nine addresses the plan
// listed are not here.
//
//   Field_StartEventBattle  0x4410B0  53 call sites in 14 chapters
//   Scena08_PartyJoin784    0x519F70  chapter 8's call table A entry 5
//   Effect_SpawnAtCell      0x524870  the cell pickups' effect (18 copies call it)
//   EventObj_Face           0x579D70  the end of every event-script placement
//   EventOp_0x              0x57A010  event op 0x / Fx, and 19 direct callers
//   Party_AddToLists        0x591CC0  chapter 6's party change
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. The unchecked indexes the
// originals make (a negative placement count, a member id past
// Character_IndexById's 24) are reproduced, as event_script.cpp's
// placements and field_event.cpp's Party_Join reproduce them (latent
// defects, docs/scena_se.md section 6).
#include "game/scena_se.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_se_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_se::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record: ObjectScenaSe(n) is a Sprite_Objects record.
// rv::As<records::EffectObject>(p) is given effect objects (Effect_Objects).

}  // namespace

void ScenaSe_Inject() {
    if (bof3::WantsShadow("scena_se")) scena_se::SelfTest();
}

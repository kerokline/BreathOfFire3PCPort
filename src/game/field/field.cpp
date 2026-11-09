// Moved here by tools/move_funcs.py from scena_se.cpp (docs/source-layout-round.md).
#include "game/field/field.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_se.h"
#include "game/scena_se_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_se::at;

}  // namespace

namespace {

using move_script::At;

}  // namespace

// original 0x4410B0: an event battle begins. Field_ScriptFlags2 bit 12 (the
// encounter bit the leader's state 5 clears, docs/event_leader.md), the event
// battle byte, the leader's state 5 with its sub-state bytes +2..+4 cleared,
// the battle's flags byte from the event battle's 4-byte record
// (EventBattle_Records[id], its +0) and the opening kind 0. The id is the
// argument's low byte, unchecked against the table (the original reads it
// the same way). No PSX twin paired.
extern "C" void __cdecl Field_StartEventBattle(unsigned id_arg) {
    const unsigned char id = static_cast<unsigned char>(id_arg);
    At(::bof3::Img(at::kScriptFlags2High))[0] = static_cast<unsigned char>(At(::bof3::Img(at::kScriptFlags2High))[0] | 0x10);
    EventBattle_Id = id;
    unsigned char* const leader = At(::bof3::Img(ObjTrio_addr));
    leader[1] = 5;
    const unsigned char flags = At(::bof3::Img(bof3::addr::EventBattle_Records))[id * 4u];
    leader[2] = 0;
    leader[3] = 0;
    leader[4] = 0;
    EventBattle_Flags = flags;
    BattlePhases_Initiative = 0;
}

void Field_Inject() {
    BOF3_INJECT(Field_StartEventBattle);
}

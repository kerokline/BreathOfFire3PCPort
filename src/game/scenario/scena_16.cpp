// Moved here by tools/move_funcs.py from move_cmds.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_16.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_cmds_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

using namespace move_cmds;

}  // namespace

// original 0x519890: chapter 16's call-table entry 0 (Scenario_CallA(0) from
// Scena16_Start): PartySet_Load(0xA, 0xFF, 0xFF, 0), both party lists emptied
// (0xFF), bits 0..2 of the flag byte before them set, Field_MemberCount 0,
// then Party_Join(0xA). No PSX twin read.
// As the original has it: the flag byte is read after PartySet_Load and
// written after the lists. The original returns Party_Join's al; Scenario_CallA's
// callers never read eax (symbols.toml there).
extern "C" void __cdecl Scena16_PartyReset(void) {
    move_cmds::g.partyset_load(0xA, 0xFF, 0xFF, 0);
    const auto flags = static_cast<unsigned char>(Cond_Flags[0xd1] | 7);
    for (unsigned i = 0; i < 6; ++i) Cond_Flags[0xd2 + i] = 0xFF;
    Cond_Flags[0xd1] = flags;
    Field_MemberCount = 0;
    move_cmds::g.party_join(0xA);
}

void Scena16_Inject() {
    BOF3_INJECT(Scena16_PartyReset);
}

// Group TWO of the platform round (step 3, 2026-10-06; docs/game-last.md): the
// two Capcom functions the hidden-start scan found left in game code
// (docs/hidden-start-scan.md section 4.1). Each read with capstone to its last
// instruction (2026-10-06):
//
//   Item_UseFlags       0x591810  (category, item) -> the item's flag byte; a
//                                 4-entry jump table 0x591888, no calls
//   ItemTrade_Dispatch  0x593950  the trade screen's states by the byte 0x93985C,
//                                 a tail jump through ItemTrade_States, unchecked
//
// Neither calls anything, so nothing here goes through the harness but the
// state handlers ItemTrade_Dispatch reaches through its table, which it reads
// in place (the fuzz swaps the cells for recorders). No divergence: each is a
// faithful replacement. Where the original jumps past ItemTrade_States' three
// entries, ours aborts with a message (docs/game-last.md section 4).
#include "game/game_last.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/game_last_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = game_last::at;
using U = std::uint32_t;

}  // namespace

void GameLast_Inject() {
    if (bof3::WantsShadow("game_last")) game_last::SelfTest();
}

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
using move_script::At;
using Handler = void(__cdecl*)();

}  // namespace

// original 0x591810 (PSX 0x80166918): by the category's low byte through the
// 4-entry table 0x591888 (dec; cmp 3; ja default):
//   1  the byte at 0x657461 + 28 * id (a weapon's +0x11)
//   2  the byte at 0x657D79 + 26 * id (armour's +0x11)
//   3  the byte at 0x658461 + 24 * id (an accessory's +0x11)
//   4  0 (xor al, al over the category less 1, 3: eax 0)
//   any other (0, 5..255)  the byte at 0x656B38 + 22 * id (a consumable's flags' low byte)
// id is the item's low byte. The whole eax as the original leaves it - its
// callers read al: for 1 and 3 the byte is loaded over the id (below 0x100),
// for 2 over id * 13 (lea edx, [eax+eax*2]; lea eax, [eax+edx*4]) and for the
// default over id * 11 (lea edx, [eax+eax*4]; lea eax, [eax+edx*2]), so their
// upper bytes are that product's.
extern "C" unsigned __cdecl Item_UseFlags(unsigned category, unsigned item) {
    const unsigned id = item & 0xFF;
    switch (category & 0xFF) {
    case 1: return At(at::kWeaponFlags + id * at::kWeaponStride)[0];
    case 2: return ((id * 13) & ~0xFFu) | At(at::kArmourFlags + id * at::kArmourStride)[0];
    case 3: return At(at::kAccessoryFlags + id * at::kAccessoryStride)[0];
    case 4: return 0;
    default: return ((id * 11) & ~0xFFu) | At(at::kConsumableFlags + id * at::kConsumableStride)[0];
    }
}

// original 0x593950 (GameMode8_TradeStep's tail jump 0x52CF30): xor eax, eax;
// mov al, [0x93985C]; jmp [ItemTrade_States + eax * 4] - unchecked. The three
// states are ItemTrade_Open, ItemTrade_Run, ItemTrade_Leave (all ours); the
// byte is written 0, 1, +1 and -1 and nothing else (docs/game-last.md section
// 1.2). Past the three the original jumps through ItemTrade_OpenSteps' cells
// (ItemTrade_OpenStart for 3); ours aborts with a message. The handler is the
// table's cell read in place - in the game Capcom's address or the jump Inject
// put there to ours, a recorder while the fuzz runs ours.
extern "C" void __cdecl ItemTrade_Dispatch(void) {
    const unsigned state = At(at::kTradeState)[0];
    if (state >= ItemTrade_States_count)
        bof3::Fatal("ItemTrade_Dispatch: the trade state 0x93985C is %u, past the %u entries of ItemTrade_States 0x66A470 - "
                    "the original jumps through the dword after (docs/game-last.md section 4)",
                    state, ItemTrade_States_count);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ItemTrade_States[state]))();
}

void GameLast_Inject() {
    if (bof3::WantsShadow("game_last")) game_last::SelfTest();
    BOF3_INJECT(Item_UseFlags);
    BOF3_INJECT(ItemTrade_Dispatch);
}

// The platform round's step 3, group TWO: the two Capcom functions the
// hidden-start scan found left in game code (docs/hidden-start-scan.md section
// 4.1) - Item_UseFlags 0x591810 and ItemTrade_Dispatch 0x593950. Two
// functions. docs/game-last.md.
//
// What they are, by the code:
//   - Item_UseFlags(category, item): the item's flag byte, one table a
//     category (Item_EquipMask's shape one byte on); the battle item menu's
//     states, BattleObj_HitReceive, the AI, the auto-target and Item_CanUse
//     read it;
//   - ItemTrade_Dispatch(): the trade screen's state dispatcher, the byte
//     0x93985C through ItemTrade_States; GameMode8_TradeStep's tail jump.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void GameLast_Inject();

namespace game_last {
// BOF3X_SHADOW=game_last: the start-up fuzz, game_last_fuzz.cpp. Clones the
// two originals before GameLast_Inject patches them.
void SelfTest();
}  // namespace game_last

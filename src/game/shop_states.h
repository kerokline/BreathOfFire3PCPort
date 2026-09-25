// The shop overlay's first table (originals 0x57F500..0x5801A0): the
// dispatcher that Game_Mode 7's step 1 jumps to, the mode's begin and end,
// and the inn - its prompt, the night and the in-game save menu that the inn
// and the save point share - through the tables 0x663E40 and
// 0x663F4C..0x663FB7. docs/shop_states.md.
#pragma once

void ShopStates_Inject();

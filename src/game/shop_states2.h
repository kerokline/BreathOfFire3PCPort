// The shop's buy and sell states (originals 0x5818B0..0x582EA3, the PSX
// SHOP.EMI compiled into the exe, reached through ShopTrade_States 0x664118
// and the step tables after it), the shop's close 0x584F70, and task 0's
// title loop with its mode 0 (0x588E70, 0x588EB0). docs/shop_states2.md.
#pragma once

void ShopStates2_Inject();

// The field menu and the list draws (round eight, group DH): the field menu's
// state machine and its top bar (originals 0x589970..0x58A0D0, the PSX
// START.EMI's code compiled into the exe), and record handler 6 of
// Field_RunTaskRecords - the top bar's five windows - with the slide steps
// the menu and shop windows share (0x599B50..0x59A6A7). docs/menu_lists.md.
#pragma once

void MenuLists_Inject();

// The battle menu states (originals 0x447FD0..0x448B3C, pointer-reached and
// entered by a tail jump from the command step 0x42EED0): the attack
// command's target pick (0x447FD0 and the four states of 0x64E44C), and the
// item command's states - the list's opening, the list itself, the item's
// target kind, the target picks and the commit (0x448180 and the tables
// 0x64E45C, 0x64E484, 0x64E48C). docs/battle_menu_states.md.
#pragma once

void BattleMenuStates_Inject();

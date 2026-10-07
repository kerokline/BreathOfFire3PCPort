#pragma once

// DIVERGENCE DIV-0059: the item and skill lists' title centred on its real
// width under a Latin language overlay. src/game/battle_draw.cpp has the
// switch (g_list_title_centre, patched at inject under the name
// BattleListTitleCentre); every list that draws such a title in the
// 0x99-wide box asks here.

// The x of the title within its window: the original's 6 * (13 - n), n the
// count the original took (characters or bytes, as each draw has it), or
// under DIV-0059 the same centre, x + 78, less half the width the pen covers.
int ListTitle_X(const unsigned char* label, int n);

// Whether DIV-0059 is on: a Latin overlay, after BattleDraw_Inject's patch. A
// list whose title has another centre than x + 78 (the master list's x + 0x3A,
// src/game/rest_2h.cpp) asks this and centres on its own.
bool ListTitle_Centring();

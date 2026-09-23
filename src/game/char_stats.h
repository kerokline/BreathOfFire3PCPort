// Stats and inventory: the stat recompute Char_RecalcStats 0x590660 and its
// four passes (weapon, armour, accessories, the per-character trait list),
// the two byte steps under them, the equipment previews, the inventory's add
// and counts, the item-table getters, a text record's copy, and two menu
// draws (the icon quad 0x5903F0 and the pointing hand 0x5905D0), all in
// 0x5903F0..0x591CAB. docs/char-stats.md.
#pragma once

void CharStats_Inject();

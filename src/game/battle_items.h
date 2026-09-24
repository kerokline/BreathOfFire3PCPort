// The battle's item and ability effects and the helpers under them: the
// Healing Herb sparkles (0x4B8FE0..0x4B9900, MAGIC070.EMI's), three shared
// effect draws (0x4AD6F0, 0x4C5150, 0x4C54F0) and the actor-matrix push
// 0x4B7D40, the battle actor's small helpers (0x4FB830..0x4FC1F0), four PSX
// primitive setters (0x5A75F0, 0x5A76B0, 0x5A76D0, 0x5A7750) and the SND
// stream's play and stop (0x5A7140, 0x5A71C0). docs/battle_items.md.
#pragma once

void BattleItems_Inject();

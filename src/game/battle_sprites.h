// Battle actors' flags and targets, the damage and hit pop-ups, the formation's
// stat changes, the clut map behind the sprite tints, and the encounter's
// enemy set-up with the enemies' name windows: the battle engine's functions
// at 0x452BF0..0x4551A0 and 0x494280..0x49503C, and two small field-side
// helpers (Sprite_UpdateScreenSlot 0x588F00, Sound_PlayById 0x587900).
// docs/battle_sprites.md.
#pragma once

void BattleSprites_Inject();

namespace battle_sprites {

// The start-up fuzz, BOF3X_SHADOW=battle_sprites. Called by
// BattleSprites_Inject before anything is patched.
void SelfTest();

}  // namespace battle_sprites

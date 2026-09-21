// DIV-0022: the game's clock counts from the game's own start, not from the
// last full boot of Windows (docs/known-defects.md D5).
#pragma once

void GameClock_Inject();

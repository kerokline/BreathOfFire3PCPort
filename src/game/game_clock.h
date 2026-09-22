// DIV-0022: the game's clock counts from the game's own start, not from the
// last full boot of Windows (docs/known-defects.md D5).
#pragma once

void GameClock_Inject();

// Tooling: stop and restart the clock, so that a pause of the game thread -
// a frozen `shot` of a recipe (docs/input-script.md) - leaves the frame
// deadline no debt to replay. Return false when the clock is not ours
// (BOF3X_ORIGINAL=Game_Clock), when a pause is repaid as usual.
bool GameClock_Pause();
void GameClock_Resume();

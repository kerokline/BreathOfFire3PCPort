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

// DIV-0047 / DIV-0048: milliseconds since the game started as a double from
// QueryPerformanceCounter, for our WinMain's frame deadline. The tick slot
// above steps 15.6 ms at a time (GetTickCount's granularity), which at a
// period under that let several deadlines pass in one step and drew one
// frame of them (2026-09-24, the 4x and 1 ms runs: 64 drawn a second at
// any period). Paused and resumed with the tick clock, so a frozen shot
// leaves this deadline no debt either.
double GameClock_NowMs();

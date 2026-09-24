// Cheats (docs/cheats.md): the EXP and zenny multipliers, DIV-0045, and the
// steal roll that always passes, DIV-0046. Each is a launcher setting carried
// by an environment variable, off when the variable is unset.
#pragma once

#include <cstdint>

// Reads BOF3X_EXP / BOF3X_ZENNY / BOF3X_STEAL and patches the two steal rolls
// when asked. Called once from InjectAll.
void Cheats_Inject();

// 1 unless BOF3X_EXP / BOF3X_ZENNY was set: what Battle_EnemyDefeated
// multiplies a fallen enemy's yield by before adding it to the battle total.
std::uint32_t Cheats_ExpMultiplier();
std::uint32_t Cheats_ZennyMultiplier();

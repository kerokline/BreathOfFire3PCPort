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

// DIV-0046 on Pilfer's roll, which is ours since round eight (Steal_Start,
// src/game/magic_fx_reached.cpp): the mask the random byte takes - 0xFF, or
// 0 when the cheat's patch is in Capcom's body at 0x4B5691 (BOF3X_STEAL=1 and
// not BOF3X_ORIGINAL=Cheat_StealAlways). Read from those bytes after the
// patch, so ours rolls exactly as the patched original would.
std::uint32_t Cheats_PilferRollMask();

// The same for Steal's roll, ours since round nine (SkillSteal_Roll,
// src/game/magic_steal.cpp): the byte at 0x4F51EF after the patch.
std::uint32_t Cheats_StealRollMask();

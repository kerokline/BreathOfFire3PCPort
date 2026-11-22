// Cheats (docs/cheats.md): the EXP and zenny multipliers, DIV-0045, the
// steal roll that always passes, DIV-0046, every enemy's information
// shown, DIV-0187, the minigames that cannot be failed, DIV-0188, and the
// skill that is always learned, DIV-0226, and the faerie village's speed,
// DIV-0243. Each
// is a launcher setting carried by an environment variable, off when the
// variable is unset.
#pragma once

#include <cstdint>

// Reads BOF3X_EXP / BOF3X_ZENNY / BOF3X_STEAL / BOF3X_ENEMY_INFO /
// BOF3X_NO_FAIL / BOF3X_LEARN / BOF3X_FAERIE and patches the two steal rolls when asked. Called once from
// InjectAll.
void Cheats_Inject();

// DIV-0243: the faerie village's speed, BOF3X_FAERIE=n, 0..10, 1 when unset.
// The village counts battles (Game_BattleCount, 0x904134) from stamps it
// keeps of that count. With the speed n each battle since a stamp counts as n
// village battles; the count and the stamps stay in real battles, so the save
// is as it was (docs/cheats.md section 9).
std::uint32_t Cheats_FaerieSpeed();

// The village battles since `stamp`: (clock - stamp) x the speed, unsigned as
// the original's subtraction is. At 1 it is the original's difference.
std::uint32_t Cheats_FaerieElapsed(std::uint32_t clock, std::uint32_t stamp);

// The real battles that hold `span` village battles, rounded up: what a
// stamp moves by when the village spends `span`, and how far behind the
// clock a stamp is kept. At 1, and at 0 (where nothing is ever spent), `span`.
std::uint32_t Cheats_FaerieSpan(std::uint32_t span);

// A shop's level steps `q`, a quotient of the elapsed battles, with the level
// they are added to. The original adds them in a byte, so 256 steps add
// nothing; a speed above 1 brings that within a short absence, so there the
// steps stop at what the byte holds (the caller's cap then applies). At 0 and
// 1, `q` as it came.
std::uint32_t Cheats_FaerieSteps(std::uint32_t q, std::uint32_t level);

// DIV-0226: true under BOF3X_LEARN=1. Battle_MemberRollByAction
// (src/game/battle/dragon_cmd.cpp) then answers 1 whenever its other tests
// pass, whatever Rand gives: a member that watches an enemy learns the skill
// the enemy uses, if the skill can be learned, the party does not know it
// and the member's list has room. The coin flip for a skill that cannot be
// learned (Battle_MemberCoinFlip) is not touched. False when unset.
bool Cheats_LearnAlways();

// DIV-0188: true under BOF3X_NO_FAIL=1. A minigame's own code then skips
// the step that judges the player's attempt a failure, one site at a time
// as each is reached and verified (docs/cheats.md section 7): the rope pull
// (Area75_PhasePlay, src/game/area_w1f.cpp). Nothing else of a minigame
// changes - the player still plays it, they just cannot lose it. False when
// unset.
bool Cheats_MinigameNoFail();

// DIV-0187: true under BOF3X_ENEMY_INFO=1. Battle_SetupEnemy
// (src/game/battle_sprites.cpp) and Battle_PlaceBossActor
// (src/game/rest_3g.cpp) then give every enemy record's byte +0x8F the value
// 1 - the byte the original takes from the save's "kind defeated before" bit
// (Battle_EnemyKindFlag), 0 for a boss - so the HP gauge in the target and
// status banners and Identify's stats panel show for every enemy, bosses
// too. The bit set itself is not touched. False when unset.
bool Cheats_EnemyInfoShown();

// 1 unless BOF3X_EXP / BOF3X_ZENNY was set: what Battle_EnemyDefeated
// multiplies a fallen enemy's yield by before adding it to the battle total
// (0..10). The EXP one also scales the two boss hooks that write the EXP
// total themselves, Boss16_End and BossWeretigr_EndMove.
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

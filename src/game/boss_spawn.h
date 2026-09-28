// The boss set-ups' spawn helpers: six small engine functions at
// 0x4948E0..0x494A7D that every boss set-up and hook calls with a tag - find
// the field actor carrying it (Sprite_Objects, type 7, +0x9E the tag), clear
// one of its bits, copy an enemy's pose or place onto it, clear its state
// bytes; and find the area's enemy data record by its byte +0xC. Round eleven
// group BH took them as a module of their own (docs/boss_h.md section 5),
// fuzzed through the boss harness (boss_harness.h).
#pragma once

void BossSpawn_Inject();

namespace boss_spawn {
// BOF3X_SHADOW=boss_spawn: the start-up fuzz, boss_spawn_fuzz.cpp - one
// boss_harness::Run. Clones every original before BossSpawn_Inject patches it.
void SelfTest();
}  // namespace boss_spawn

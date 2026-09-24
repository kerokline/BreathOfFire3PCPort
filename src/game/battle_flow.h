// The battle task and the turn flow (originals 0x42E400, 0x432F10,
// 0x435110..0x4360E9, 0x436B50, 0x437470..0x437A05, 0x444480..0x444652,
// 0x4456C0): the battle's per-frame phase dispatch, the 48 battle-task slots
// (run, create, free, clear), the eight enemy objects' state dispatch, their
// animation helpers, an enemy's defeat and its drops, the magic loaders for
// abilities and items, the rolling-digit and number draws, and the "actor is
// out" test. docs/battle_flow.md.
#pragma once

void BattleFlow_Inject();

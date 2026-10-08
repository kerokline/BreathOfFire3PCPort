# The enemies' AI data, and a check against an outside description

**Status:** IN PROGRESS (2026-10-03). The engine side is compared and agrees.
Seven enemies' records are compared by hand and agree; the other 168 distinct
records are not compared yet. Nothing live was run.

The outside description is second_advent's *Enemy Database* for the
PlayStation release
(<https://gamefaqs.gamespot.com/ps/196817-breath-of-fire-iii/faqs/57525>,
v4, 2011). Its author asks that it not be reproduced, so **nothing of it is
copied here or anywhere in the repository**: this note names its sections and
says where our code agrees. Read the guide itself beside
`tools/enemy_ai.py`'s output.

## 1. Where the data is

Not in `BOF3.exe`. Each `AREAnnn.DAT` has a kind-0 chunk of tag `0xC2000`,
`0x4A8` bytes, which loads at `0x8C5580`: `Encounter_Rows` (8 x 9 bytes), then
the area's eight enemy data records of `0x8C` bytes at `0x8C55C8`. The four AI
rows are at record `+0x38` (the `0x8C5600` every `EnemyAI_*` function indexes).
The record's fields are in `tools/enemy_ai.py`'s header, each from the code
that reads it (`Battle_CopyEnemyData`, `EnemyAI_ApplyAction`,
`BattleEnemy_PickAction`).

`python tools/enemy_ai.py --game bof3 [--area N | --name TEXT | --conditions]`
prints the records in our code's own words. Its output is game data
(CLAUDE.md rule 1): terminal or `analysis/` only.

## 2. The engine against the guide's "AI Layout" section

Every point the guide makes about the mechanism has a counterpart in code
that is ours and fuzzed (2026-10-03, read side by side):

| The guide's term | Ours |
|---|---|
| five sections, the first unconditional | the record's own odds index `+0xE` and ability list `+0x1C`, then four rows |
| the pre-turn conditions (21) | `EnemyAI_ChooseActions`' 21 cases `0x0B..0x15`, `0x19..0x20`, `0x26`, `0x27`; a failing condition unmarks the row, so it can fire again |
| the post-attack conditions (19) | `EnemyAI_TurnCheck`'s cases `0..0xA`, `0x16..0x18`, `0x21..0x25`; the "repeatable" ones are the cases that apply without marking the row done |
| the death element only through a skill | `EnemyAI_CondElement`: an attack is not tested against mask bit 8 |
| the seven change types | `EnemyAI_ApplyAction`'s kinds 1..7 |
| the modifier as tenths, level by its square | `EnemyAI_ScaleStat`: `x * f / 10`, case 6 by `f * f` |
| every resistance but death | `EnemyAI_SetAttrByte` skips `+0xE6` |
| the attack type, in quarters | `BattleEnemy_PickAction`: four 2-bit kinds of `0x65563C[+0x8E]` by `Rand() & 3` |
| the skill rates, in eighths | the same function: one of eight bytes `+0x9C` by `Rand() & 7` |
| a later section wins | rows apply in order 0..3, each overwriting `+0x8E` and `+0x9C..` |
| an unused section | condition byte `0x63` (`EnemyAI_OtherRowsDone` answers 0 for one) |

All 1,792 rows of the 448 records (175 distinct) carry a condition our two
functions have a case for, or `0x63` (`enemy_ai.py --conditions`).

**One point to measure.** For condition `0x25` the guide's condition list and
its version notes word the threshold differently. Ours compares the HP word
`+0xA4` with the amount `+0x108` as Capcom's code does; whether `+0xA4` is
already reduced when `EnemyAI_TurnCheck` runs was not read here.

## 3. Records compared

Tar Man, Ripper, Gonghead, Goblin, Eye Goo, Mage Goo and BossGbln (areas 8
and 51): conditions, change kinds, masks, modifiers, odds and abilities all
agree with the guide's entries.

- The stat order at `+0x28..` (strength, defence, agility, intelligence), the
  element of each mask bit, `+0x96` / `+0x94` as experience / zenny and
  `+0xAA` / `+0xAE` as the steal / drop rates are named from this agreement:
  hypotheses, not read from the binary.
- The sibling's `names/abilities.toml` is keyed one below the byte in an
  ability list (byte 0 is an empty place). The tool subtracts one. Which
  side is off by one was not settled.

## 4. Open

- The remaining records, and the bosses': a boss kind's own state table can
  override what its record says ([`boss_h.md`](boss_h.md)), so a boss needs
  its hook read as well as its rows.
- What the data says is not what a fight does until it is watched: a live
  check of one "hit" row (the owner's Tar Man and Frost) would close the
  loop from record to behaviour. 2026-10-07: the Volt's row explained the
  owner's 78-EXP fight to the number, and the once-only rows were found to
  fire once per *session* on the port, not once per fight - DIV-0082,
  [`trigger-mode-enemies.md`](trigger-mode-enemies.md).

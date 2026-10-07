# The trigger-mode enemies: the Volt's EXP bonus, and why the port lost it

**Status:** DONE in code (DIV-0082, 2026-10-07); the owner's live fight owed.

The owner's report (2026-10-06 night, `analysis/shots/owner_reports/
volt_fight_menu_1006.webp` and `volt_fight_result_78exp_1006.webp`): a field
fight against three Volts and one Thunder; the Thunder's first move was its
own lightning on the whole enemy side; Nina fell; the result screen said
"You gain 78 EXP!" with 78 on each row. The owner's recollection, since borne
out by the data: a Volt hit by an electric attack changes mode and gives
extra EXP. First reported 2026-10-02 as "enemies whose mode changes on a
trigger do not change" (tar men, volts), held for a route.

## 1. The mechanism, as the code and the data have it

Everything below is ours since rounds seven, eight, twelve and fourteen and
fuzzed against Capcom's bytes; the PSX twins were read beside it on
2026-10-07 and agree line for line.

- **The data.** An area's enemy records (`tools/enemy_ai.py`,
  [`enemy_ai_data.md`](enemy_ai_data.md)) carry four AI rows of 16 bytes at
  `+0x38`. The Volt (areas 27, 28, 56, 63, 76; base EXP 28 at record `+0x16`,
  zenny 6) has row 1: condition `0x02` - "hit: element mask 0x004" - change
  kind 6 with mask 2 and value 30: the EXP word scaled by 30/10, message
  `0x82`. The Thunder's EXP is 16. Other once-only EXP rows: Lava Man
  (`0x21`, x1.6), Vulcan (`0x00`, x5), Vampire (`0x10`, x0.5), and the zenny
  rows of the goos, the Gazer and the Big Bulb (`enemy_ai.py` lists them;
  game data, not reproduced here).
- **The test.** `EnemyAI_TurnCheck` `0x44AAD0` (`battle_misc.cpp`) walks the
  enemy's four rows; opcodes 0..8 call `EnemyAI_CondElement` `0x44B320`
  (`battle_e5.cpp`) with masks 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x80.
  `CondElement`: 0 unless the enemy's damage word `+0x108` is not 0; for an
  ability (acting kind `0x904B35` = 4) the ability's element word
  (`0x65C4DC` + 24 x id) against the mask, **whoever cast it** - the Thunder's
  own lightning counts; for an attack (kind 1) by a member 0..2, the weapon's
  element byte. Jolt, Lightning and Myollnir all carry bit 4 (`0x1404`).
- **The change.** A row whose test passes and whose done bit is clear
  (`EnemyAI_RowDone` `0x44AC80`: byte `+0xF1` bit `row`) goes through
  `EnemyAI_ApplyAction` `0x44B3A0` - kind 6: `+0x96` (the EXP the reward
  path reads) times value / 10, capped at 0xFFFF - and `EnemyAI_SetRowDone`
  marks the bit. Opcodes `0x21..0x24` apply without marking (the "every
  time" rows).
- **When.** `EnemyOp_ReceiveAction` `0x436740` ([`enemy_ai_ops.md`](enemy_ai_ops.md)
  section 2, "the hit taken", step 4) runs `EnemyAI_TurnCheck` after the
  damage is applied and the pop-ups shown, **before** step 5 detects the
  kill. A Volt killed by the lightning is tripled first, then dies.
- **The reward.** `Battle_EnemyDefeated` `0x437470` (`battle_flow.cpp`) adds
  the object's `+0x96` to the EXP total `0x904AEC` (times DIV-0045's
  multiplier); `BattleResult_SplitExp` `0x431940` divides the total by the
  count of eligible members, rounded up ([`battle_result.md`](battle_result.md)).

So: 84 + 28 + 28 + 16 = 156, halved over Ryu and Peco = 78. Exactly one of
the three Volts fired its row.

## 2. Why one of three: the done byte is never cleared on the PC

`+0xF1` is written by `EnemyAI_SetRowDone` alone. At spawn,
`Battle_CopyEnemyData` `0x4946C0` (`battle_sprites.cpp`) clears a fixed list
(`+0x10D`, `+0x110..`, `+0x114..`, `+0x118..`, `+0x11C..`, `+0x122..+0x125`)
and sets `+0xF0`; `Battle_PlaceBossActor` `0x494570` clears the first 0x80
bytes and the same list; `BattleEnemy_ClearStates` `0x494E70` zeroes bytes
`+0..+4` at a fight's end. The objects at `0x93B960` are `.bss`. A slot
whose Volt fired row 1 in an earlier fight hands the bit to the next enemy
spawned there: the Volt in the one slot still clear paid out, the other two
were already "done". The 2026-10-02 "never happens" is the same thing after
a few fights.

**The PlayStation twin** (`0x800A9148`) clears the same list and not
`+0xE1` (its `+0xF1`) either. The difference is where the objects live:
`0x801EB5A0` (stride 0x118) is inside the BATTLE.EMI#3 game-mode image,
`0x801D0C00..0x801ED93F` (118,080 bytes, md5 `8a80230e...` as the sibling's
`names/overlays.toml` has it), which the game loads from disc over the field
overlay at every battle entry; the image's bytes at all eight objects are
zero (BATTLE.EMI pulled off the sibling's disc image and split with its
`tools/emi.py`, 2026-10-07). Every PSX fight starts with the byte clear by
reload; the port, with one static copy, keeps it.

**Verdict:** the port's change. Not ours (the clone comparisons of every
function above pass; `battle_e5.md` control 31 is kind 6 itself), not
Capcom's design. Repaired as DIV-0082: `Battle_CopyEnemyData` also clears
`+0xF1`. The same repair covers the Tar Man's frost row and every once-only
hit row.

## 3. The check owed

A live fight, twice in one session, against a Volt group with a thunder hit
on every Volt (the Thunder's lightning does it; so does Nina's Jolt on each):
every Volt should yield 84 both times, and the message `0x82` should show
for each. Before the fix the second fight pays only the slots the first did
not touch. `BOF3X_ORIGINAL='*'` is not the A/B here - the original has the
bug - a fresh launch is.

## 4. Other state of the same shape

Anything the PlayStation resets by reloading an overlay image and the port
keeps in static memory is a candidate for the same defect. A read-only
survey (2026-10-07, an agent over the sibling's decomps and our spawn code)
found no second confirmed case. What it established:

- **The game-mode images.** BATTLE.EMI#3 `0x801D0C00..0x801ED93F`, START
  to `0x801ED861`, SHOP to `0x801E619F`, all at the one base; BATL_END apart
  at `0x801EEC00..0x801F0BA8`. BATL_END's decomp stores nothing into its own
  image (its state is the `0x801462xx` globals, PC `0x904Axx`, which
  `Battle_Init` resets, `battle_phases.cpp`); the party's persistent copies
  (`0x80145E8C`) are boot RAM, not an image, and the two `Battle_Init`s clear
  the same fields.
- **Mutable data inside BATTLE's image** besides the enemy objects: the
  current-enemy and menu-actor pointers, the banner pool, the turn-order
  scratch, the message ring, the task slots and the stat copies - each
  reset by `Battle_Init` on both platforms or rewritten before it is read
  (`battle_phases.cpp:167`, `:201`).
- **The enemy object's bytes**, by writer at spawn: `Battle_SetupEnemy`
  writes `+0..+8`, `+0xC..+0x3B` (less `+0x9..+0xB`, `+0x2A`), `+0x3E`,
  `+0x48`, `+0x4B`, `+0x5C..+0x5F`, `+0x70`, `+0x8F`, `+0x9A`;
  `Battle_CopyEnemyData` the working record `+0x80..+0xDB`, `+0xDF..+0xE7`,
  `+0xF0`, `+0xF1` (DIV-0082), `+0x100`, `+0x10D`, `+0x110..+0x11F`,
  `+0x122..+0x125`; `Battle_SetEnemyOffset` `+0xF2..+0xF3`. Read but not
  written at spawn: `+0xF4` / `+0xF8` (the event-battle hook and cue
  pointers, written by the boss Enter states, `boss_sa.md`; read by
  `EnemyOp_CastCue` only in an event battle - a leftover pointer in a
  non-boss event fight would be a wrong cue, a null one a fault; not seen),
  and `+0xDC..+0xDE`, `+0xE8..+0xEF` (base attribute bytes with no writer
  anywhere; the image holds zero there too - the whole eight objects are
  zero in the file - so the port's zero `.bss` matches).
- **The enemy message list** `0x939FC0` (count `0x93C2A2`,
  `EnemyAI_ApplyAction` writes, `BattleAction_EnemyMessages` reads): neither
  `Battle_Init` nor the round reset touches the count; its PSX home was not
  resolved. A fight ending between a queue and its commit would show an old
  message under a new enemy's name once. Not seen; left as a note.
- **Every way out of a battle** should pass `BattleEnemy_ClearStates`
  (`BattleEnd_Finish`, `BattleLoss_ResetParty`, `BattleLoss_Restart`,
  `Escape_Leave` are the four callers); not every exit path was walked.
- SHOP writes globals inside its own image (`0x801E5F84..0x801E6198`); not
  mapped to the PC.

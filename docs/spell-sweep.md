# The spell sweep: every ability cast in a battle that cannot end

**Status:** MEASURED (2026-10-26: built and run - the three sides on the
PC install, the member and enemy sides on a disc-built install before and
after the twin-read placements, sections 4 to 7; the enemy side's Howling
failure explained and the enemy side re-run on both installs the same
evening, sections 6.2 and 6.3; open: the Super Combo text row, the owner's
eye; 2026-10-27: the enemy and item sides on the disc-only installs,
section 10, and the sweep by enemy kind, section 11). The owner's idea the
same day: "a
harness where an enemy casts all the spells and abilities in the game, and
the party has infinite life, just to check the render / functionality of
the spells"; and "treat it like normal combat by altering / injecting ids
into the character skill list - that way targets are assigned like normal.
We could do the same for combat-useable items."

## 0. Why

Issue #149 (2026-10-25) was a `game-value` gap row - `BoneDanceShake_Counts`,
three beat lengths a disc-built install zeroes - that the install-gap census
had listed as NEEDS FIX since 2026-10-18 and the read watch
([`install-gap-reads.md`](install-gap-reads.md)) had never seen read, because
no route fights a D.Zombie. "Not read on eleven routes" was taken for
harmless. About sixteen `game-value` rows of the same shape remain on the US
build (`WardMote_Tilts`, `TornadoMote_Phases`, `ReviveHalo_MoteCounts`,
`AirRaidImage_ShadeSteps`, the `EffectKind18Sub*` states, `BossGaist_Anims`,
...; [`exe-gap-round.md`](exe-gap-round.md) 1), each reached only by the
one spell, effect or scene that reads it. The sweep is the coverage answer:
cast every ability under a disc-built install with `BOF3X_GAPWATCH` on, and
a gap read while zero is flagged by the watch itself, no eye needed. The
same run gives the render check the 46 spell group docs (`magic_s*.md`,
`magic_c*.md`) each list as owed ("the owner casting them, with a save that
has them or DIV-0045's cheat").

## 1. What it is

`tools/spell_sweep.py`. A **harness, not a route**: every recipe it writes
pokes the game's memory (the `poke` step of
[`input-script.md`](input-script.md) 3, "an experiment's tool: a recipe with
a poke is not a player's route"), so nothing here is a player's path and no
recording of it is a reference. It writes recipes and drives
`tools/input_run.py` over them, one chunk of abilities per launch, reading
the launcher's `bof3x.log` for each ability's end and going on past a
failure at the id after it (`run`).

The fight is the `combat` route's (`tools/recipes/combat.txt`, the owner's
2026-09-23 recording: the `combat` save, a few steps, a random encounter),
cut at the recipe frame its field mode `0x66C7E8` became 5 - **frame 501**,
measured by a probe run with a `peek` after every `wait` - and finished by
`until 0x66C7E8 u16 == 5`. Three members (Ryu, Teepo, Rei), one enemy
(Gooey, enemy 0: `+0x8E` 1, flag word `+0x90` 0xE0, status `+0x92` 4, HP
48; the prologue's peeks, logged on every run).

**The battle cannot end.** Every enemy slot's HP (`0x93B960 + k * 0x128 +
0xA4`) is poked to 0xFFFF before each cast: `Battle_ApplyDamage` leaves an
enemy at 0xFFFF alone (Capcom's own rule, [`battle_damage.md`](battle_damage.md)),
so nothing the party casts kills it. Every member's working record
(`0x802DC0 + m * 0x14C`, the battle side of `Char_AbilityList`) gets HP and
max HP 999, AP and max AP 999 and DEF 999 before each cast, so the enemy's
turn costs nothing that matters and no cast runs out of AP.

### 1.1 The member side (`--side member`)

Per ability id, the owner's injection: the id is poked into **the first
slot of each of the member's four ability lists** (`+0x60`, `+0x6A`,
`+0x74`, `+0x7E` of the working record, the lists `Char_AbilityList`
returns by type; the window pages between the four and remembers the page
per actor, so row 0 of whichever page it opens on is the ability). Then the
ordinary battle menu does the rest, as in play: the cross's **up** command
(`Battle_MenuSteps[1]`, `0x447430`, the ability window - named
`BattleItem_*` in the source, IDEAS I51), confirmed with up held, as
`battle_commands.txt` does; a shot of the list (`aNNN_list`); confirm on row
0; then `seek circle` until the command step `0x904AA1` leaves the confirm
dispatch (4) - one more confirm when the ability asks for a target
(`BattleItem_Choose` step 4), none when it picks its own (step 5). So the
target is chosen as it is in play, the default of the ability's record
flags. The other two crosses Defend (right, confirmed with right held,
`Cmd_ConfirmDefend`, no target).

The cross comes up for the members in turn order, so the caster is whoever
acts first - Rei, in this fight. Every member holds the id, so it does not
matter which.

`Skill_CanUse` mode 2 refuses five abilities unless a byte agrees
(`battle_window_draw.cpp`): the sweep pokes AP 0 for `0x8C` and `+0x1E` 0
for `0x14`; `0x15` and `0x3E` want four zero bytes the fight has; `0x97`
wants no event battle.

### 1.2 The enemy side (`--side enemy`)

Enemy 0's eight ability bytes (`+0x9C..+0xA3`, the list
`BattleEnemy_PickAction` draws from by `Rand() & 7`) are all poked to the
id; its action-kind row (`0x65563C[+0x8E]`, four 2-bit kinds chosen by
`Rand() & 3`) is poked to 0xFF so every roll is kind 3, which becomes kind 4
- an ability - while the flag word `+0x90` has bit 0x20 (Gooey's 0xE0 has
it; the sweep sets it anyway). The three crosses Defend; the enemy's turn
plays the ability at its own target choice (`BattleEnemy_PickTarget` by the
kind). Three more pokes and one change of step came from the Howling case
(section 6.2, 2026-10-26 evening): the EX round's gate `0x904B7A` is cleared
at the cross (the route's three members outpace Gooey every round, so
`Battle_MarkFasterSide` gave the party every other round alone and Gooey
cast on half the rounds of the first runs); the members Defend by a `seek
right` with a two-frame hold and four-frame gap (right tapped twice within
eight frames chooses the command the cross points at,
`BattleMenu_CommandSelect`'s tap rule), one seek for however many crosses
the round has, since a cast's status takes members out of the next round's
cross; and after the shots each member's low status byte (`+0x90`) is poked
to 0, so a sleep (0x20, Howling's and Bone Dance's) does not leave the fight
running by itself through the wake rolls - the high byte keeps Ovum's egg
(0x800), which has its own five rounds. This is how the 30 enemy-only abilities are reached: a member casting
one hits a path Capcom's code never took - Nue Stomp (id 1) from a member
calls `BattleEnemy_SetAnimationOf` with the member's actor, which the
original runs on a record outside the eight enemies and ours aborts
([`rest_3a.md`](rest_3a.md) 7.7) - the sweep's first trial.

### 1.3 The shots and the log

Once the input phase ends (`0x904AA0` leaves 1) the recipe takes six shots
30 frames apart (`aNNN_0..5`; 45 on the first member-side runs, whose later shots caught the next round's cross - a cast and the round's two Defends and the enemy's turn are over within about 130 frames), the game writing its own frames
(`BOF3X_SHOT_DIR`); with `BOF3X_EFFECTLOG=1` every effect object's first run
goes to the log, and the driver records each ability's effect handlers
between its marks. `results_<side>.tsv` in the root: id, name, section,
side, status (`done`, `failed` with the log's line, `short`, `not reached`),
shots, effect handlers, frames, chunk.

### 1.4 The names

The ids are the game's list bytes. The names are the archival sibling's
`names/abilities.toml` read **one id down**: the game's byte N is the
sibling's id N - 1 (the list drew *Nue Stomp* for byte 1; the sibling's id 0
is Nue Stomp and its id 1 Gambit; [`magic_c2.md`](magic_c2.md) 1 reads the
same table "one id down" for Bone Dance, 0x39 = 57). The PC's
`Ability_Records` (`0x65C4C8`, 24 bytes a record) has an empty record 0 in
front of the PSX table's 227, which is the shift. The sibling's gloss
sections sort the 227 bytes: Skills 68, Attack type spells 44, Assist type
spells 24, Heal type spells 22 (the 158 a member casts), Enemy only skills
30, Dragon spells 21 (the dragon forms', not in the ability window), Unused
/ dummy 11, unlabelled 7.

## 2. Running it

```
python tools/spell_sweep.py run --route tools/recipes/combat.txt --cut 501 --members 3 \
    --root C:\Users\kerok\bof3x-runs\spellsweep\pc --launcher <ini-less launcher dir>\bof3x-launcher.exe \
    --side member --sections member --chunk 10 --speed 4 --env BOF3X_EFFECTLOG=1
python tools/spell_sweep.py run ... --side enemy --sections "Enemy only skills" --enemy-flags 0xE0
python tools/spell_sweep.py names [--sections member]
python tools/spell_sweep.py recipes ... --out DIR         # the recipes alone
```

A disc-built install: `--game <install's game dir> --env BOF3X_EXEIMAGE=<its cache>
--env BOF3X_OPT=none --env BOF3X_LAYERING=0 --env BOF3X_GAPWATCH=<dir>
--env BOF3X_GAPWATCH_CLASSES=gap,unread,language`, the install prepared by
`abc_run.py prepare` ([`abc-harness.md`](abc-harness.md)); then
`tools/gapwatch.py report <dir> --dll <that run's bof3x.dll>`.

`--sibling DIR` before the subcommand names the archival sibling's checkout
when the tool runs from a worktree that has none beside it (the other row
tools' option).

An A/B on one cast: `--ids 68 --chunk 1`, `--poke "0x93BA1F u8 1"` (an
`input_run` poke added to every cast after the sweep's own) and `--peek
"0x93BA1F u8 e_fire"` (a peek logged after the shots), both repeatable; the
same run with `--env BOF3X_ORIGINAL=<the functions>` is Capcom's code on the
same fight - **through `bof3x-launcher-dev.exe`**, since the release
launcher refuses the hosted-only switches with a dialog. Added 2026-10-26
for the Foretell question (section 4).

`--dead M[,M]` makes those members fallen before every cast (status
`0x4000`, HP 0: the hit path's own death write) for the revives (section 9);
`--target-ups N` presses up on the target cursor before the confirm (the
default puts Rei's cursor on the first fallen member).

The launcher folder must hold no `bof3x.ini` (the play ini's cheats and
options would change the fight; [`abc-harness.md`](abc-harness.md)). One
game at a time: the driver reads the launcher folder's `bof3x.log`. About
400 recipe frames an ability, under two seconds at speed 4; a chunk of ten
with its launch and route about a minute.

## 3. The first trial (2026-10-26, the PC install)

Six abilities 1..6 as one chunk. The window opened on row 0 *Nue Stomp*
(help "Jump attack (random damage)", page HEAL, the gauges 999 / 999), the
row confirmed, the target confirmed by the seek's one press, the two Defends
taken, the shots started - and the cast aborted ours: `FATAL:
BattleEnemy_SetAnimationOf: actor 2 is no enemy (3..10)`. Nue Stomp is an
enemy skill, Rei was the caster (actor 2): section 1.2's reason for the
enemy side, and the numbering of section 1.4.

## 4. The member side on the PC install (2026-10-26, 158 ids)

First pass, chunks of ten, speed 4, 108 s a chunk: **150 done**, four
`failed` and four never reached. The four failures were all the same thing,
and not the failing id's: the cast *before* it left a message open that
waits for a button - Sanctuary (58) "All auxiliary magic dispelled!",
Weretiger (64, Rei's change), Foretell (68) "Terrible luck. Lucky color:"
and Accession (151, refused by `Skill_CanUse`: its row greyed, the seek's
three confirms gave up) - and the recipe pressed nothing while it waited
for the next input phase, so the wait timed out on battle phase 3 after
9,000 frames. Since then the wait for the input phase is a `seek circle`
(a confirm every twelve frames, 120 at most), and the driver no longer
drops the id after a failure (it did: 60, 66, 70 and 152 had no row). The
eleven ids were re-run (section 4.1).

**For the owner's eye** (`<root>\pc\run_member_*\aNNN_K.png`, six a
cast, the list shot `aNNN_list.png`):

- Foretell (68): the message reads "Terrible luck.Lucky color:" and then a
  blank box where the colour's word should be (`a068_5.png`). Whether the
  PC's own text has the word, or the box is the game's, is not settled
  here. **Settled 2026-10-26, not a defect:** the box is the game's grey
  tile. The lucky colour is never a word - `Foretell_DrawTiles` draws one
  8 x 12 tile per element the enemy is weak to (its resistance class at
  most 1, holy at most 3) after the colon, and one 0x60 grey tile when
  there is none ([`magic_s15.md`](magic_s15.md) 1). The route's Gooey
  (area 20 slot 4) is class 2 in every element and 5 in holy, so grey; the
  en-US overlay's five messages are the US disc's own bytes word for word,
  "Terrible luck." with no space before "Lucky" on both. The grade is the
  fight's, not the pokes': HP 999 / 999 is the same full-HP band the fight
  starts in, and the party's levels 1 / 1 / 5 against the Gooey's 45 give
  the lowest level band (5) less the three-against-one bias (12): -7,
  "Terrible luck". Capcom's own fourteen Foretell functions
  (`BOF3X_ORIGINAL`, the developer launcher) draw the identical frame
  (384 grey pixels at the same place), and with the Gooey's fire and holy
  bytes poked weak (`--poke "0x93BA1F u8 1" --poke "0x93BA24 u8 3"`) ours
  and Capcom's both draw a red and a white tile. Harness root
  `C:\Users\kerok\bof3x-runs\ss1026\fore` (`pc_ours`, `pc_orig`,
  `pc_ours_weak`, `pc_orig_weak`, `pc_levels`).
- Super Combo (3): the prompt box at the field's centre reads "GO!!" on
  the PC (`a003_1.png`) - and is **blank on the disc-built install**
  (section 6).

### 4.1 The re-run

The four lost ids (60, 66, 70, 152) and the three stalled-after ids (59,
65, 69) all `done`; Accession (151) `refused` again - `Skill_CanUse` greys
it in this fight. Accession is the dragon screen, which is tested elsewhere
(the owner, 2026-10-26): skipped by design, not an open row. **The member
side ends 157 done of 158, 1 skipped (Accession), 0 failed**, 357 recipe frames a cast on
average (`results_member.tsv`). The report (`spell_sweep.py report`)
rebuilds the table from the chunk logs, a re-run's `done` row winning.

## 5. The enemy side (2026-10-26, 48 ids: the 30 enemy-only, the 11 unused, the 7 unlabelled)

The first run went through all 48 in six minutes with every row `done` -
and cast nothing: the next round's cross was back within 30 frames of the
input phase's end on every id (`run_enemy_*\aNNN_1.png`). **The combat
route's Gooey is paralysed** (the owner, the same afternoon: it is Gooey's
own logic - it paralyses itself unless the party is several levels above
its own, about 49 - and a paralysis cure would also lift it; its status
word `+0x92` reads 4 at the fight's start), so it skips every turn. The
enemy side now clears `+0x92` before each cast, pokes the kind row of the
enemy's own `+0x8E` (`--enemy-odds`) rather than re-pointing `+0x8E`,
leaves the enemy's HP alone (the members only Defend on this side), and
logs the enemy's command kind (`+0x105`) and ability (`+0x106`) after the
shots.

**The second run** (the status cleared): Gooey casts - the banner names
the skill, Bone Dance (57) lands its "Miss!" marks over Rei
(`run_enemy_033\a057_1.png`), Astral Warp and the rest of 17..32 all run
through. **47 done, 1 failed** of 48 after the re-runs; two things in it:

- **Nue Stomp (1) stalls the battle**: the banner shows, Gooey leaves its
  spot, and phase `0x904AA0` stays 3 through 120 confirm presses. The
  skill waits on the Nue's own jump animation, which a Gooey's sprite does
  not have: the harness's limit (section 8), not a port defect. (The first
  pass charged it to Chlorine (9), whose block began with the wait; since
  then each block ends with its own wait, and 1 and 9 were re-run.)
- **Paralyzer (139) crashes ours** - `CRASH 0: exception 0xC0000005 at
  bof3x.dll+0x346377`, `Paralyzer_Start` (`magic_engine.cpp`), reading
  `0x00000000`: the documented latent defect of row 123
  ([`magic_engine.md`](magic_engine.md) 6) - the cue table pointer `+0xF8`
  only an event battle's set-up writes, read unchecked, in Capcom's code
  the same. The sweep reached it from an ordinary battle, as predicted;
  nothing new, now seen.

The enemy's command kind (`+0x105`) peeked after the shots read 0 on
every id - the record is cleared by then (`EnemyOp_CastDoneCost`); the peek
was in the wrong place and said nothing. Since 2026-10-26 evening the enemy
side waits, once the input phase ends, for `+0x105` to read 4
(`BattleEnemy_PickAction` writes it at the enemy's turn, after the members'
Defends, 21 frames later in this fight) and peeks the kind and the ability
there, before the shots; a timeout of that wait (300 frames) is "the enemy
did not cast", which the first runs could not tell from a cast. The shots
therefore start at the pick, about 20 frames later than before.

## 6. The disc-built run with the gap watch

The US disc + PSP-EU install (`abc_run.py prepare --set us-disc --only
psx-us-psp`, root `C:\Users\kerok\bof3x-runs\spellsweep_disc`), the exe
image from its cache, `BOF3X_GAPWATCH` with `gap,unread,language`. **A
run of a disc-built install from `input_run.py` needs `BOF3X_CACHE=<the
cache>` as well as `BOF3X_EXEIMAGE`** - without it the game stops at its
eighth frame (the first attempt lost 25 minutes to five timed-out chunks;
`abc_run.py run_one` sets both).

**The first chunk's report** (`gapwatch.py report`, abilities 2..12, 4,500
latches): 4 of 255 ranges read, two before any write - the empty save
slot (`Encounter_SlotChance+0x1C`, the known owner's call) and
**`Effect1b_ToggleText+0xE` `0x66A096..0x66A0D8`, class `language`, read
90 times before any write by `SuperCombo_DrawText` (`0x49B420`) from
`SuperCombo_DrawBox` and `SuperCombo_Prompt1`**: Super Combo's prompt
text. The shot agrees: the box at the field's centre that reads "GO!!" on
the PC is empty on the disc-built install (`probe_gw\a003_1.png` against
`pc\run_member_002\a003_1.png`). A bone-dance-shaped row, found by the
sweep's third cast. `0x66A094`'s layout under a Western layer was already
an open item of [`exe-gap-round.md`](exe-gap-round.md) 3
([`install-gap-labels.md`](install-gap-labels.md) 4.3, 8); this is what it
costs in play.

### 6.1 The full run, all three sides (the image of `main` 25d12620, before the twin reads)

Member 156 done, 1 failed (65 Pilfer - the wait after Weretiger, section
8), 1 skipped (Accession); enemy 47 done, 1 failed (54 Howling: the input
phase never ended after the three Defends, 900 frames - not seen on the PC,
not yet explained); item 55 done, 36 refused, the same 36 as the PC's. 64
runs, every chunk's watch file joined (`gw_report.tsv`): **9 of 255 ranges
read in some run, 7 of them before any write.** Each, with its shot:

| Range | Class | Read by | In | What the shot shows |
|---|---|---|---|---|
| `AirRaidImage_ShadeSteps` `0x65A698` | gap, game-value | `AirRaidImage_Bounce` from `KickImage_Task` | Air Raid (16), 108 reads | **the kicked image's rising flame is missing** - the PC's `a016_1.png` has it at the top left, the disc's does not (shade steps 0) |
| `WardMote_Tilts` `0x65A918` | gap, game-value | `WardMote_PushMatrix` from `WardMote_Launch` | WardOfLight (43), Resist (153); 512 reads | the motes draw; a tilt of 0 is not told from one frame |
| `Effect1b_ToggleText+0xE` `0x66A096` | language | `SuperCombo_DrawText` | Super Combo (3), 90 reads | **"GO!!" blank** (section 6) |
| `Encounter_SlotChance+0x1C` | gap, language-zero | the load screen | every run | the empty save slot, the known owner's call |
| `Battle_CommandLabels` `0x669D28` | language | `BattleWin_DrawCommandLabel` | every enemy and item run | the labels draw (`Atk` on every cross shot): the layer's write, which the watch cannot see on a parked page ([`install-gap-reads.md`](install-gap-reads.md) 9.3); the member runs do not list it, so the read-before-write is the blind spot |
| `Effect1b_ToggleText+0x60` `0x66A0E8` | language | `GeneWin_DrawChoices` | the Accession block (chunk 144) | the gene window's choices - the row CW showed written by the layer on Western installs (9.3) |
| `Menu_Verbs+0xF4` `0x66A31C` | language | `Text_DrawSmall` | chunk 047 (Identify in it) | written by the layer (`DIV-0018: 22 menu verbs` in the install's log), unseen |
| `Magic080_TextPointers+0x18` `0x66A3E0` | language | `Text_CharCount` from `Identify_DrawEnemy` | Identify (60) | the panel is the PC's to the pixel (`a060_3.png` on both: WEAK POINT, the `?` rows, EXP, ITEM) |

So two real zeros in play on the before-image - Air Raid's flame and Super
Combo's prompt - plus Ward's tilts, unjudged by eye. The first two of the
three are `gap` rows the twin-read pass placed the same afternoon
([`exe-immediate-places.md`](exe-immediate-places.md) 7, branch
`worktree-agent-ada98af7cd3367ec1`: `AirRaidImage_ShadeSteps` from
`MAGIC015.EMI#0`, `WardMote_Tilts` from `MAGIC043.EMI#3`); the prompt text
is a `language` row for the layer (LA's kind), not placed by that pass.

### 6.2 The after picture

The install rebuilt from the twin-read branch's recipes (`c_uspsp2` /
`g_uspsp2`, `python <worktree>/tools/importer.py build` with the same two
discs, then `install --base`), the member and enemy sides again (`pc2`,
`gw2`): member 157 done, Accession skipped - Weretiger's aftermath cleared
this time; enemy 47 done, **Howling (54) failed the same way again** (the
input phase never ends after the three Defends; the log shows the battle
and common sound banks loading during the Defends, as if an action had
begun; the PC's two runs of 54 passed) - open. The watch now guards **245
ranges** (255 before: the placed rows are gone) and reads **6 of them in
some run**, none a `gap` row but the empty save slot: `AirRaidImage_ShadeSteps`
and `WardMote_Tilts` are no longer in the list at all. The shots agree
for Air Raid: at the same recipe frame (`a016_2.png`) the before-image
draws Rei's kicked image washed out to white at the field's centre, the
after-image draws it in its colours. (The PC's frames land earlier in the
effect - its `a016_1.png` has the flame rising at the top left where the
disc runs show nothing yet - so the PC is not a frame-exact reference for
this one; the two disc runs are.) `Effect1b_ToggleText+0xE` (Super Combo's
"GO!!") is still read while zero: the `language` row stands, for a layer
entry.

**Fixed (2026-10-26, label group 24, DIV-0064 amended; D393):** the four
texts come from `MAGIC003.EMI`'s twin of `SuperCombo_DrawText` (`BOOT.BIN`'s
on a PSP), written in place behind `0x66A0D8`
([`install-gap-labels.md`](install-gap-labels.md) 4.5). `loc_build.py selftest` (the reader on synthetic code: the US chain found, a stride-6 chain refused); the reader over the seven discs (`combo prompts 4` on each: the US words on six, the French first text on `psx-fr`); `importer.py check` and `exe_tables.py check` 0 errors; the US + PSP-EU disc-built install rebuilt from this branch (root `C:\Users\kerok\bof3x-runs\ss1026\sc`) and a PC install copy with an `en-US` layer from it, the sweep's ability 3 alone on each, the install's log `DIV-0064: 4 of 4 combo prompts` on both: the disc install's `a003_1.png` reads READY, `a003_2.png` GO!! and `a003_5.png` O HIT COMBO!! where the before-image's box was empty; the PC copy under `en-US` draws the same frames to the pixel, and under `original` its own bytes (the full-width spaces, text 0 wider). The gap watch (`gap,unread,language`, 900 latches) still lists `0x66A096..0x66A0D8` read 720 times "before a write" with writes 0 - the parked-page blind spot of [`install-gap-reads.md`](install-gap-reads.md) 9.3 (page `0x66A000` is parked on most latches, so the layer's load-time writes are not seen; the row is written by the log line and the shot). Not captured: the French first text under `fr-FR`.

**Howling: explained** (2026-10-26 evening, D391 in
[`known-defects.md`](known-defects.md); nothing of the port's or Capcom's).
Three things of the harness's, read off the two logs and then off a probe
recipe with a peek set after every step (`C:/Users/kerok/bof3x-runs/ss1026/howl/probe*`):

1. **Gooey cast on every other round.** After each of its casts the EX-round
   byte `0x904B7A` was set (`Battle_MarkFasterSide`: the route's three
   members outpace Gooey) and `Battle_BuildTurnOrder` admitted only the
   marked members (`battle_damage.cpp`'s `Gated`): the turn order read
   `02 01 00 FF` where a casting round's reads `02 01 00 03`. The datlog
   shows it as every second round loading no `MAGICnnn.DAT`. So half the
   enemy-side `done` rows of sections 5 and 6 had cast nothing - which half
   depended on the chunk's parity: on the PC's second run Ovum (52) fell on
   an EX round and Howling (54) on a casting one; on the disc runs the other
   way round. The PC "passed" Howling because it cast it after an uncast
   Ovum; the disc "failed" it because it had cast Ovum the round before.
2. **Ovum's egg took Rei out of the cross.** Ovum's target gets status
   0x800 (`BattleStep_Restore800` lifts it after five rounds: HP and AP to
   the maximum, `Battle_ClearStatus`); `Battle_BuildEntryOrder` leaves such a
   member out, so the round after had two crosses, not three. The recipe's
   third Defend block then pressed into the *next* round's first cross, and
   its `until 0x904AA0 != 1` waited for the input phase to end while the
   game waited at the second cross for a press that never came: "the input
   phase never ends after the three Defends", 900 frames. The sound banks
   "loading during the Defends" were the commit's `BATL_SE.DAT`
   (`Battle_CommitRound`'s `load_dat(0xD1)`) one Defend early.
3. **Howling itself puts Ryu and Teepo to sleep** (status `+0x90` 0x20, the
   bit `BattleInput_NextMember` skips; Bone Dance sets the same), so once it
   is really cast the party has no cross until `BattleStep_PartyWake20`'s
   roll clears it - 750 frames on the probe, four rounds of Gooey casting
   Howling again, longer than the 480-frame wait after the shots. The PC's
   second run woke them the next round.

The fix is the harness's (section 1.2): the EX-round byte cleared at the
cross, one `seek right` Defend for however many crosses the round has, the
party's low status byte cleared after the shots, and the cast confirmed by
the pick (section 5's peek). With it Ovum, Howling and Bone Dance cast in
one chunk on both installs, each with its `MAGICnnn.DAT` load and a kind-4
peek, and Howling draws on the disc install as on the PC (the members
stretched tall, the egg with them). The whole enemy side was re-run on
both installs after the change (section 6.3).

### 6.3 The enemy side re-run with the fix (2026-10-26 evening)

The enemy side (48 ids: the 30 enemy-only, the 11 unused, the 7 unlabelled)
run again on the US-disc + PSP-EU install (`abc_run.py prepare --set us-disc
--only psx-us-psp`, root `C:/Users/kerok/bof3x-runs/ss1026/howl`, the main
checkout's importer at 5bd84b98) and on the PC install, from one ini-less
launcher copy, chunks of ten at speed 4, about seven minutes a side. **Both
installs read the same row for row: 47 done, 1 failed** - Paralyzer (139),
the known crash of row 123 ([`magic_engine.md`](magic_engine.md) 6) - with
Nue Stomp (1) stalling the fight as before (section 5) and Chlorine (9) and
Syphon (142) re-run after them. The difference from the first runs is what
`done` now means: **every one of the 48 ids was picked by Gooey** (the
`enemy0_command_kind = 0x4` peek before the shots, 48 on each install, each
with its `MAGICnnn.DAT` load in the datlog), where the first runs had cast
on every other round. Howling (54): `done` on both, 219 recipe frames, the
members stretched tall on the disc install's shots as on the PC's.

The gap watch over the disc run's seven chunks (`gapwatch.py report`,
16,200 latches): 3 of 255 ranges read, 1 before any write - the empty save
slot (`Encounter_SlotChance+0x1C`, the known owner's call) - and
`Battle_CommandLabels` and `Task_StackTop` as every run before. No `gap`
row is read by any enemy-only skill now that each of them is really cast.

## 7. Items (`--side item`)

The owner's second half, built the same way: category 0's first id and
count (`0x904154` / `0x904354`, the lists the pointers at `0x656B00` /
`0x656B14` name; `char_stats.cpp`) are poked to the item and 9, the
window's saved page, top and cursor (`0x904605..7`) to 0, and the cross's
**down** command opens the item window (`BattleItemCmd_*`,
[`battle_menu_states.md`](battle_menu_states.md)); row 0 confirmed, then
the target as on the member side. The 91 consumables (the sibling's
`names/items.toml`, the same ids: the list drew *Green Apple* x 9 at row 0
for id 1). `Item_CanUse` mode 2 (flags bit 1) greys what battle refuses; a
greyed row fails the seek and the driver goes on.

**The PC run: 55 done, 36 refused, 0 failed.** The refused are the
window's own: the stat shards and foods (25..31), the vases, rags, cups,
beads, books, paintings and icons (34..55), Mackerel (77), the Parts
A..H (78..85), the Faerie Tiara, Skill Ink, Flower Jewel and Water Jug -
every one greyed by `Item_CanUse` mode 2, so the confirm kept the row and
the seek gave up (`results_item.tsv`, status `refused`).

## 8. Limits

- A forced cast is not a player's cast: the caster is whoever's cross comes
  first (or enemy 0), not the character the ability belongs to, so an
  effect that reads its caster's sprite may draw with another's frames, and
  an enemy skill that waits on its own enemy's animation stalls the battle
  from a Gooey (Nue Stomp, section 5). Zeros and hangs it still finds. The
  sweep by enemy kind (section 11, 2026-10-27) casts 26 of the 33 enemy
  skills an enemy carries from that enemy; seven boss-only ones have no
  route yet.
- After Weretiger (64) the fight does not come back to a cross for 1,440
  frames of confirms on either install (Rei's changed form acts on its own);
  the driver restarts the chunk at the next id. Likewise after a skill that
  ends the battle, if any did (none seen: the enemy's 0xFFFF HP holds).
- Stale shots: a re-run writes into the same `run_<tag>` folder and does
  not clear it; read the file times.
- Dragon spells (21) are the dragon forms' and need the Dragon command and
  a gene set; not swept.
- One fight, one enemy kind, one area: effects that read the enemy's family
  or the field are seen in one setting.

## 9. The revives over a fallen member (2026-10-26 evening)

The twin read placed `ReviveHalo_MoteCounts` the same day, and no sweep run
had ever read it (section 6's `gw_report.tsv`: 0 reads): Raise Dead and
Resurrect spawn the halo's motes over a fallen member, and the sweep's party
never falls. So `--dead`: before each cast the member's working-record status
word is poked to `0x4000` and its HP to 0, the two words the hit path writes
outright at a member's death (`battle_e3.cpp`); the command gate (mask
`0x4964`) then leaves the member out of the cross, the game draws it lying
down from the next frame (the pose is the state machine's, not the hit
sequence's, so the poke is enough), and the other crosses Defend by the
enemy side's one `seek right` (`defend_all`), since their count has changed.
The target cursor starts on the caster (`Battle_DefaultTarget`, the first
not-out member from the caster upward) and the party side wraps the index
without skipping the fallen, so the recipe presses up `(dead - 2) mod 3`
times - Rei casts first in this fight - as a player moves it. After the
shots the fallen member's HP and status word are peeked.

The run: the US-disc + PSP-EU install rebuilt from `fix/sweep-wave` (the twin
reads and label group 24 in; root `C:\Users\kerok\bof3x-runs\ss1026\rv`,
`disc` / `disc_item` with the gap watch in `gw`) and the PC install (`pc` /
`pc_item`), Ryu (member 0) fallen, Rei casting, one id a chunk. Which
abilities revive was taken from the code, not the names: `ItemUse_Revive`
is the one field item handler that clears `0x4000` (Ammonia,
[`item-use.md`](item-use.md)), and MAGIC077 serves both Raise Dead rows and
both Resurrect rows ([`magic_s17.md`](magic_s17.md)); Benediction
(MAGIC169, [`magic_s35.md`](magic_s35.md)) was cast as the owner's guess.

| Id | Name | Disc install | PC install | Ryu after (HP / status) |
|---|---|---|---|---|
| 76 | Raise Dead | done, 289 frames | done, 289 | 250 / 0 |
| 77 | Resurrect | done | done | 999 / 0 |
| 180 | Raise Dead (type 3 row) | done | done | 250 / 0 |
| 181 | Resurrect (type 3 row) | done | done | 999 / 0 |
| 169 | Benediction | done, 277 | done, 277 | **0 / 0x6000 - not a revive** |
| item 14 | Ammonia | done | done | 1 / 0x2000 |

The shots (`a076_2..5.png`): Ryu lying at his spot, the Raise Dead banner,
Rei's cast glow, the halo's motes rising, then Ryu standing with 250 and a
green popup; **the disc install's frames are the PC's to the pixel** for
every row, so the placed row draws as the PC's data did. The gap watch over
the six runs lists 3 of 245 ranges read, only the empty save slot before a
write (the placed row is no longer a range it guards; its evidence is the
shots). Benediction raises the statue over the party and brightens the
living members (`a169_2`, `a169_4`); Ryu stays fallen with `0x2000` added to
his status, so it is a party effect on the living, not a revive - what its
`0x2000` is was not read. Two things for the owner's eye, the same on both
installs and so ours either way: the green popup over Ryu reads **543** where
his HP after Raise Dead is 250 (`a076_5.png`); and Ammonia leaves the status
word `0x2000` with HP 1 where the spells leave 0. Neither is a disc-install
difference; whether the PlayStation shows the same number is the next
question for either.

### 9.2 Status bit 0x2000 (2026-10-26 night, a read of the code)

Bit `0x2000` of the status word - the member's `0x802DC0 + m * 0x14C` +0x10
(the party record's +0x90, `records::PartyMember::status`) and the enemy's
`0x93B9E0 + ..` +0x92 (`records::BattleEnemy::status`, the object's +0x12) -
is the **low-HP mark: set while HP is below a quarter of max HP, cleared
otherwise**. It is not a revive mark, not a buff, not an exclusion, and it
is not the round flags' `0x2000` (`EnemyOp_HitEnd` turns round flag 0x40
into 0x2000 in the round word `0x904AA8`, which `battle_e3.cpp:516`
reads) nor the second flags dword's (+0x130 / +0x134, the task-in-flight bit `BattleEnd_AwaitMemberTasks` waits on, `battle_turn_steps.cpp:364`).
The sibling's read of the PSX setup (`names/functions.toml`,
Battle_Setup's evidence line) names it the same, independently: "sets
status 0x2000 (low HP) when HP < max/4 else clears it".

Who writes it (every site is an HP-against-quarter test; none else writes
the bit):

| Site | Record | Writes |
|---|---|---|
| `BattleWin_MemberGaugeStates` `0x5975D0` (kind 5, every frame a member's gauge window steps) | member +0x90 | `LowFlag`: `or byte [+1], 0x20` while HP < max / 4, `and word, 0xDFFF` otherwise (`battle_win_states.cpp:125`, 229) |
| `BattleWin_EnemyGaugeStates` `0x597320` (kind 4) | enemy object +0x12 = record +0x92 | the same `LowFlag` (`battle_win_states.cpp:203`) |
| the battle setup (`battle_phases.cpp:233`) | member +0x90 | set when HP < max / 4, else cleared, once at battle start; the other counters zeroed beside it |
| `Char_HealHp` `0x590CE0` (`item_use.cpp:302`) | the field record +0x10 or the working record by the battle flag | `Char_ClearStatus(id, 0x2000)` when HP after the heal is at or above max / 4; sets nothing |
| `Field_FloorHurt` (`field_e2.cpp:260`) | both the field record +0x10 and Field_State +0x90 | set when HP after the floor's damage is at or below max / 4 |
| `Menu_DrawMemberStatus` (`map_field_objects.cpp:457`) | the field record +0x10 | set below a quarter, cleared otherwise, each draw of the status window |

Who reads it: `Escape_Chance` `0x44A520` (`battle_e4.cpp:1513`) adds 8
to the escape chance once when any member not out has the bit; the enemy
AI's condition `0x0E` (`EnemyAI_ChooseActions`, `battle_damage.cpp:925`) holds
when another enemy not out has it - "an ally is in danger"; the gauge and
status windows colour the HP value 4 (the warning colour) on it
(`battle_win_states.md`, `battle_windows.md`); and `Battle_ActorCanCommand`'s
mask `0x4964` leaves it out, so a member in danger still commands.
`Battle_ClearStatus` `0x44F4B0` clears only the bits its mask names (0x4000,
0x800, 0x80, 0x40, 0x10, 8, 4, 2, 1, and 0x20), so the round-end steps'
`clear_status(status & 0xBF5F)` never touch it; and the hit path's death
write (`battle_e3.cpp:506`, 929: `status = 0x4000`, HP 0) drops it, which
is the `0x4000` the poke copies.

So the table in section 9 reads as the arithmetic says, with Ryu's max HP
999 (Resurrect's 999 is a full heal) and the quarter 249:

- **Ammonia, 1 / 0x2000**: `ItemUse_Revive` writes HP 1 and
  `Char_ClearStatus(0x4000)`; `Char_ClearStatus` does not clear 0x2000, and
  the next frame's `BattleWin_MemberGaugeStates` sets it, 1 < 249. In play
  the revived member stands with a red HP figure.
- **Raise Dead 250 / 0, Resurrect 999 / 0**: MAGIC077's revive leaves HP at or
  above the quarter, and the gauge step clears the bit the frame after.
- **Benediction, 0 / 0x6000**: Benediction added nothing. The poke wrote
  `0x4000` with HP 0; 0 < 249, so the first gauge step after it or'ed
  `0x2000` in - the `0x6000` is the poke's `0x4000` plus the mark, and any
  cast over the poked member that neither revives nor heals would peek the
  same. (The revive rows read 0 and not 0x2000 for the same reason in
  reverse.) Benediction's own effect on the living party is still what
  `magic_s35.md` reads it as; this section only settles the bit.

Not measured, and not needed: a `--peek` of the word per frame would show
the `0x4000 -> 0x6000` step on the first gauge frame after the poke; the
sites above are the only writers of the bit in `src/game`, so the reading
stands without it.

### 9.3 The PSP-only install (2026-10-26 night, D394)

The same six casts on an install built from the PSP-EU disc alone
(`importer.py build --source <PSP-EU iso> --lang en-150`, then `install
--base`; root `C:\Users\kerok\bof3x-runs\ss1026\rv\pspeu`, the gap watch
in `gw_pspeu`): the halo rose **without its motes** (`a076_4.png` against the
PC install's), and the gap watch reported `ReviveHalo_MoteCounts`
`0x65B1D4..0x65B1D7` (`gap`, `game-value`) read 5 times before any write by
`ReviveHalo_Spawn` `0x4BD380`. Section 9's "placed" was the US disc's: on
the PSPs `exe-gaps.tsv` still listed three of the four bytes as a gap, and
the twin read's note "the PSPs fill it"
([`exe-immediate-places.md`](exe-immediate-places.md) 7.3) was a misread of
that narrower range.

Why ([`exe-immediate-places.md`](exe-immediate-places.md) 7.6): the PSP
discs carry `BMAGIC/MAGIC077.EMI` with the twin at the same address
`0x801EF4A8` and its count table at `0x801F049C` equal to the PC's four bytes,
and `build_twins` finds the twin by its body on both PSPs; but the PSPs'
`BOOT.BIN` map segment for `ReviveMote_Types` begins one byte early, on the
zero low byte of a pointer word before the table, so the free region was three
bytes, two non-zero, under `find_imm`'s four-byte floor. The fix is a floor
rule: the reader symbol's bytes past the region that another range already
holds equal to the PC count when the twin's table holds them too; the row
places on PSP-JP and PSP-EU (DIV-0101 amended). The two neighbouring "the
PSPs fill it" notes are true: `AirRaidImage_ShadeSteps`' four bytes and
`WardMote_Tilts`' first byte are the PSPs' `BOOT.BIN` map's (equal to the
PC's), the tilts' other seven an `immref` row.

The check: the PSP-only install rebuilt from `fix/sweep-psp-halo` (root
`C:\Users\kerok\bof3x-runs\ss1026\psp`), abilities 76 and 77 with Ryu
fallen. The gap watch over the two runs: 3 of 219 ranges read (220 before:
the row is no longer one it guards), `ReviveHalo_MoteCounts` gone from the
report, the one read before a write the empty save slot as in section 9. The
compare against the PC install's section-9 runs (`abc_compare.py`,
`cmp_076` / `cmp_077`): `a076_0..5` and `a077_0..5` **differ nowhere** -
the halo frames `a076_3` / `a076_4` that differed in the halo region before
(boxes `176,28..288,108` and `132,8..320,120`, 22,842 and 37,432 pixels)
are the PC's to the pixel, the motes rising over Ryu; only the list shot's
two small boxes (`224,72..244,92`, `224,180..244,200`: the PSP's own icons)
stay, as on every PSP-built install. Ryu stands after with 250 and 999 as
in the table.

**Simoon's dusts (D245, the same night).** The full member sweep on the
PSP-only install (`full_pspeu`) drew Simoon's dome and fan without its two
dust sprites: the PSX-only install's dense shots (`simoon_psxus`, 40 shots
three frames apart) show them rising inside the dome over `a103_10..18`,
the PSP-only install's (`simoon_pspeu`) none, the compare's boxes growing
from `40,136..96,160` to `0,76..148,188`. The row is `SimoonDust_Offsets`,
the one placed-but-wrong symbol `psp-image.md` 1 had named: the PSP map
bridged a jump table into it, and `SimoonDust_Wait` sent the dusts 1.7
million units away. Fixed in `disc_image` (the relocated-word rule,
[`exe-immediate-places.md`](exe-immediate-places.md) 7.7; DIV-0101
amended), the table then `psxmap`'s from the PSP disc's `MAGIC103.EMI#3`.
The check, the PSP-only install rebuilt again (`g_pspeu2`), Simoon alone
with the dense shots: `a103_0..39` **differ nowhere** against
`simoon_psxus` (before: `a103_10..18`, up to 48,298 pixels), the dusts in
the dome from `a103_10`; the list shot's two PSP icon boxes as ever
(`cmp_simoon`, root `C:\Users\kerok\bof3x-runs\ss1026\psp`).

### 9.4 The three installs, the whole member side (2026-10-26 night)

The owner's ask behind the revives: "compare a psx only / psp only build
with the pc based builds to make sure none of the resurrection spells /
effects were broken on those builds." So the revive set over a fallen Ryu
on four installs, then the **whole member side** (158 ids, chunks of ten,
`--confirm circle` - after the load the battle confirms with circle on
every install, the release's cross is the title and load menu's only) on
the PC install, a US-PSX-only install and a PSP-EU-only install, each
built from this branch before the PSP fixes of 9.3, with the gap watch on
the two disc-only ones; roots `C:\Users\kerok\bof3x-runs\ss1026\rv`
(`full_pc`, `full_psxus`, `full_pspeu`, `gwfull_*`, the compares
`cmp3_run_member_*`, `cmp_full_px_*`, `cmp_simoon_dense`).

**The revives over a fallen Ryu** (`--dead 0`, the four installs):

| Install | Raise Dead 76 / 180 | Resurrect 77 / 181 | Ammonia 14 | Frames against the PC (7 a cast) |
|---|---|---|---|---|
| PC | 250 | 999 | 1 | - |
| US disc + PSP-EU (the players' shape) | 250 | 999 | 1 | identical |
| US disc alone | 250 | 999 | 1 | identical |
| PSP-EU alone | 250 | 999 | 1 | **the halo without its motes** (9.3, D394), fixed |

**The whole member side**: 157 done of 158 on each of the three installs,
the same three non-casts everywhere (Pilfer after Weretiger, Accession by
design, Snooze after it). 1,100 frames a pair through `abc_compare.py`:

| Pair | Frames differing | What |
|---|---|---|
| PC against US-PSX-only | 3 | Super Combo's count line (`a003_3..5`): the play install's cache predates label group 24, so it draws the PC's full-width spaces where the layer's text has the disc's half spaces (section 6.2, the owner's eye) |
| US-PSX-only against PSP-EU-only | 164 | the list shots (the PSP's own icons, two 20 x 20 boxes, every cast); Fire Breath and Ice Breath's banner word (the EU English layer spells them as two words); **Simoon's two dusts missing on the PSP-EU install** (`a103_2`, `a205_2`; the dense shots `simoon_*`: nine frames, 30 to 54 after the cast, the haze growing over the enemy on the PSX install and nothing on the PSP's) - D245, the PSP map's bridged hole, fixed (9.3) |
| PC against PSP-EU-only | 167 | the two rows above together |

The gap watch on the two disc-only installs over the whole member side
(18 runs each, 245 / 220 ranges): no `game-value` row read while zero;
the `language` rows listed before a write are the parked-page blind spot
of [`install-gap-reads.md`](install-gap-reads.md) 9.3 (Super Combo's
prompts, the gene window's choices, the menu verbs, Identify's labels, all
drawn by the layer in the shots) and the empty save slot. So on a
disc-only install the member side's effects are the PC's but for the two
PSP rows the sweep found and this branch places. Not swept here: the enemy
side and the item side on the disc-only installs (the enemy side ran on
the US + PSP install in 6.3), and a PSP-JP install.

## 10. The enemy and item sides on the disc-only installs (2026-10-27)

Section 9.4 compared the whole member side across the PC install, a
US-PSX-only install and a PSP-EU-only install; this is the same for the
other two sides. The enemy side (48 ids, `--sections "Enemy only
skills,Unused/dummy skills,none" --enemy-flags 0xE0`) and the item side
(`--side item`, the 91 consumables) on the PC install (the main checkout's
`bof3`, `--lang en-US`), a US-PSX-only install (`importer.py build` from the
US cue, `install --base`, `en-US`) and a PSP-EU-only install (the PSP-EU iso,
`en-150`), both built from `away1027/sweep` (main `e84aaf6d` plus the
`--kind` option), with `BOF3.CFG` copied in, the gap watch on the two
disc-only ones (`gap,unread,language`); the `combat` route cut at 501, three
members, chunks of ten, speed 4, `--confirm circle`, `BOF3X_EFFECTLOG=1`. Root
`C:\Users\kerok\bof3x-runs\away1027\sweep` (`pc_enemy`, `psxus_enemy`,
`pspeu_enemy`, the `_item` three, `gw_*`, the compares `cmp_*`; the chain
`chain_part1.sh`). **A disc-only install's run wants `BOF3X_RECIPE_SAVES`**
naming the main checkout's `tools/recipe_saves` when the tool runs from a
worktree: `recipe_saves.py` finds the main checkout's saves only for a game
folder that is the main checkout's (the first attempt's two disc runs ended
at once, "no recipe save").

| Side | PC | US-PSX-only | PSP-EU-only |
|---|---|---|---|
| enemy | 48 done, 0 failed | 48 done | 48 done |
| item | 55 done, 36 refused | 55 done, the same 36 refused | 55 done, the same 36 refused |

On the enemy side every id was picked (`enemy0_command_kind = 0x4` and the
id at `+0x106`, 48 of 48 on each install), no until timed out, no crash:
**Paralyzer (139) casts** where sections 5 and 6.3 crashed on it, DIV-0183
(PR #156) in. Nue Stomp (1) still leaves the fight stalled after its six
shots, as from every Gooey (section 11 settles it), and Chlorine (9) is
re-run in the next chunk. The recipe frames a cast took are the same id for
id on all three installs, on both sides.

**The pictures** (`abc_compare.py`, tolerance 4, 16 pixels; 288 enemy frames
and 546 item frames a pair):

| Pair | Enemy frames differing | Item frames differing |
|---|---|---|
| PC against US-PSX-only | 0 | 0 |
| US-PSX-only against PSP-EU-only | 1: UtmostAttack's banner (`a116_1`, box `112,44..212,60`) reads "Vortex Wave" under the EU layer | 91: every list shot, the PSP's two icon boxes (`224,72..240,92`, `224,180..240,200`) and nothing else |
| PC against PSP-EU-only | 1, the same banner | 91, the same list shots |

So nothing but the two expected rows of 9.4 - the EU layer's words and the
PSP's icons. No missing effect, zero, hang or stuck phase on either side.

**The gap watch** (`gapwatch.py report`, 6 enemy and 40 item runs on each
disc-only install; 245 ranges on the US-PSX-only image, 219 on the
PSP-EU-only): 3 ranges read in some run, 1 before any write, on all four
reports - the empty save slot (`Encounter_SlotChance+0x1C`, the known
owner's call), with `Battle_CommandLabels` (the parked-page blind spot,
[`install-gap-reads.md`](install-gap-reads.md) 9.3) and `Task_StackTop` as
on every run before. No `game-value` row is read by an enemy skill or a
consumable on these installs. With 9.4, all three sides are now swept on
the PC, the US-PSX-only, the PSP-EU-only and the US + PSP-EU installs; not
swept: a PSP-JP install and the Western PSX discs other than the US one.

The PC runs also wrote agent stp's bit-15 census (`BOF3X_STPCENSUS`, one file
a chunk, `census\`; [`stp-census.md`](stp-census.md)): 46 files, and every
row with opaque-only or mixed draws is a page-15 text palette (CLUT `7800`
in area 0's load screen and area 29's item window, `7807` in the item
window) - no other row, the rope's included, since the fight has none.

## 11. The sweep by enemy kind (2026-10-27)

Section 8's first limit: the enemy-only skills were cast by the combat
route's Gooey, so a skill that plays its own enemy's animation stalled
(Nue Stomp) and an effect that reads its caster drew with the Gooey's. This
casts each one from an enemy that has it.

### 11.1 How a fight's enemy is chosen (measured)

Read in our code and confirmed on the runs below:

- An area's enemies are its own: the area file's kind-0 chunk of tag
  `0xC2000` (`0x4A8` bytes) loads at `0x8C5580` with the area - eight
  encounter rows of 9 bytes (`Encounter_Rows`: eight slot kinds, `0xFF`
  none, `+8` the weight), then eight enemy data records of `0x8C` at
  `0x8C55C8` (`records::EnemyData`; `tools/enemy_ai.py` reads the same
  bytes off the player's `AREAnnn.DAT`). The sprites come at the battle's
  start from the area's own `ENEMYnnn.DAT` (the combat run's datlog:
  `ENEMY029.DAT` after `BATTLE.DAT`, in area 29).
- A random encounter: `Field_EncounterDue` (`0x530030`) when the leader's
  step count `Field_StepCount` (`0x905B80`) reaches the threshold at
  `ObjTrio + 0x134` in a zone whose `+4` is set; then `Encounter_Place`
  (`0x591F30`) - `Encounter_PickRow` by weight, `Encounter_FillSlots`
  (slot k gets row[k], active when `(Rand & 0xF) + 1 <=
  Encounter_SlotChance[k]`, `0x669CB0`) into `Encounter_Slots`
  (`0x939F00`, 12 bytes a slot: used, the kind, x, z), which
  `Battle_InitEnemies` (`0x4942C0`) sets up as enemies 0.. at the battle's
  start (`Battle_SetupEnemy`, `Battle_CopyEnemyData`).
- So **poking `+0x8E` in a running fight** changes the odds row only: the
  sprite, animation bank (`+0x1A` of the data) and AI rows are the slot's.
  **A poke before the set-up** does it: the rows poked to the wanted slot
  alone and the threshold passed, in the wanted area.

### 11.2 The tool: `--kind AREA:SLOT --at X:Z:FLAGS`

`spell_sweep.py run --side enemy --kind A:S --at X:Z:F` (`kind_head`): the
combat route kept to frame 371 (`--field-cut`, the party standing in area
29's field before the route's own walk), the recipe's `goto A X Z F`
(`Field_ChangeArea` through the original's entry, the door sweep's step;
`X:Z:F` a link's arrival in that area, from a door-sweep `steps.json`), a
settle, then pokes: the eight rows to `[S, 0xFF x 7, weight 2]`, the slot's
four AI rows off (condition `0x63`, so the pick is the base list the sweep
pokes - an Egg Gang's row cast ability 69 on the first try), slot 0's chance
16, the threshold 1 and the step count `0x100`; the leader walks (`--walk`,
16 frames a direction) until the field mode is 5; 20 frames into the battle
the members' HP and DEF are poked 999 (a GntOrc's first strike ended the
combat save's party, levels 1..5, before the first cross). The prologue
peeks the area and enemy 0's slot (`+0xF0`), so a run says which enemy it
fought. `--enemy-odds` / `--enemy-flags` take the kind's own odds index
(`+0x0E`) and flag word (`+0x12`) from its area record.

**Only a kind in a random-encounter row works this way.** The census (below)
found every enemy-only skill's non-encounter carrier to be a boss kind,
animation bank `0xFFFF` - an event battle's, set up by `Boss_SetupTable`.
`--kind 22:1` (area 22's Nue) set up as a random encounter crashes ours in
the sprite draw (`0x5935B0`'s replacement, `sprite_draw.cpp`, reading
`0x00000000`, mode 5 step 5; `probe_k22_1`, dump in that folder): a state
the game never makes, the harness's misuse, not a defect. Those kinds are
reached by a route that fights them, cut at its battle (`--route R --cut F`
as for `combat`; the cut is the last frame the game mode `0x66C7E8` is not 5
before the log's `boss encounter:` line, measured by a copy of the route
with a mode peek after every line, `away1027\sweep\mp`).

### 11.3 The census: who carries what

Every area's eight records (`AREAnnn.DAT` of the PC install, `enemy_ai.py`'s
reader), each slot's base list (when its odds row has an ability kind) and
its AI rows' lists, for the 48 enemy-side ids (game data: the table is in
the root, `kind_census.json`, not here). **33 of the 48 have a carrier**;
15 are in no enemy's lists at all - the 11 `Unused/dummy` (25, 80,
111..113, 145..148, 213, 214), the second Barrier (188), the second Depress
(211) and 127, 225 - so a cast by "an enemy that has it" does not exist for
them, and section 5's Gooey casts stand as their only ones. Of the 33, 21
are carried by a random-encounter kind (one chosen per id with a link
arrival in its area); the other 12 only by boss kinds: Nue Stomp (the Nues, areas 22 / 23), Astral Warp (Amalgam),
Bone Dance and RottenBreath (D.Zombie), UtmostAttack (Stallion), and - with
no route of the owner's reaching them - Head Cracker (Shroom, area 119),
Holocaust and Venom (Myria, 198), Corona (Gaist, 120), Main Cannon
(Ammonite, 134), Paralyzer (Chimera, 197) and Eye Beam (Gazer, 77, an AI
row's).

### 11.4 The runs

The PC install and the US-PSX-only install of section 10 (gap watch on),
one run a kind, chunks of the ids it carries. Random-encounter kinds by
`--kind`; boss kinds by `cutsceneAndNue` (cut 4300, the area-23 Nue, boss
id 2), `ropeAndAmalgam` (cut 4902, boss id 12), `minecartToZombieDragon`
(cut 5207, boss id 32) and `stallion` (cut 10573, its second fight, boss id
24). Root `C:\Users\kerok\bof3x-runs\away1027\sweep` (`bykind_pc`,
`bykind_us`, `kind_table.txt`, the compares `cmpk_*`, the gap watch
`gw_kind_psxus`, the chains `chain_kind.sh`, `chain_retry*.sh`).

**Every carried skill but the seven boss-only ones without a route is cast
by its own kind on both installs** - 26 ids, Chlorine counted once though
two kinds cast it: the prologue's area and slot are the carrier's, the pick
peek `enemy0_ability` is the id (kind 4), six shots each, no stall, no
crash, no timeout.

| Id | Skill | Cast by (area / slot) | How |
|---|---|---|---|
| 1 | Nue Stomp | Nue (23 / 2) | `cutsceneAndNue` |
| 9 | Chlorine | Curr (22 / 0); and the Nue | `--kind`; the Nue route |
| 17 | Astral Warp | Amalgam (28 / 0) | `ropeAndAmalgam` |
| 18 | Giant Growth | Gonghead (95 / 2) | `--kind` |
| 23, 37 | Bad back, MagicShuffle | Codger (111 / 0) | `--kind` |
| 24 | Watch Enemy | Thrasher (127 / 1) | `--kind` |
| 27 | Hypnotize | Sleepy (105 / 1) | `--kind` |
| 28 | Devour | Man Trap (95 / 1) | `--kind` |
| 30 | Molasses | VileWeed (141 / 1) | `--kind` |
| 31 | Tarbaby | Tar Man (51 / 3) | `--kind` |
| 32 | Absorb | Audrey (127 / 2) | `--kind` (an AI row's list in the data) |
| 33 | Healing Herb | Guard (27 / 1) | `--kind` (an AI row's) |
| 34 | Steroids | BarbBulb (105 / 0) | `--kind` |
| 35 | Magic Belt | GntOrc (139 / 4) | `--kind` |
| 50 | Pollen | Big Bulb (117 / 4) | `--kind` |
| 52 | Ovum | Egg Gang (139 / 2) | `--kind` |
| 54 | Howling | Cerebus (82 / 2) | `--kind` |
| 57, 81 | Bone Dance, RottenBreath | D.Zombie (108 / 3) | `minecartToZombieDragon` |
| 61 | Target | Tricker (35 / 0) | `--kind` (an AI row's) |
| 86 | Barrier | DeathBot (167 / 0) | `--kind` |
| 109 | Depress | Spectre (77 / 4) | `--kind` |
| 114 | Fire Whip | Berserkr (146 / 2) | `--kind` |
| 116 | UtmostAttack | Stallion (67 / 1) | `stallion`, second fight |
| 142 | Syphon | BloodBat (77 / 1) | `--kind` |

(The slot an event battle's enemy takes is the boss set-up's, not the area
record's the census names: the Amalgam fought as slot 0 of area 28, the
D.Zombie as slot 3 of area 108, the Stallion as slot 1 of area 67.)

**Nue Stomp, answered: no stall from a Nue.** From the area-23 Nue the
banner shows, the Nue leaves its spot, lands on the party and returns
(`bykind_pc\r_nue\run_enemy_001\a001_0..5.png`), and the fight is back at
the cross four confirms later (recipe frame 4619, 240 frames after the
pick) - on the PC and the US-PSX-only install alike. The stall of sections
5, 6.3 and 10 is the Gooey's missing animation (the harness's), as section
5 supposed: nothing of the port's or Capcom's.

**The pictures, PC against US-PSX-only** (`abc_compare.py`, the same root
name on both sides): **0 frames differ in every one of the 24 runs**, the
route fights included. (A first pass of eleven kinds differed because the
tool changed between its PC and US halves - the start-of-battle HP poke
shifted the frames; re-run as `*_v2` with one tool, they are identical.)

**The gap watch** over the US-PSX-only by-kind runs (24 runs, 245 ranges):
7 read, 5 before a write - the empty save slot, `Battle_CommandLabels` and
`Menu_Verbs+0xF4` (the parked-page blind spot), and `Equip_SlotIcons+0xC0`,
`+0xDC`, `+0xF8` read by `MenuList_IconWheel` in the `stallion` route's
field menu (rows [`install-gap-reads.md`](install-gap-reads.md) lists read
on all 77 of its runs, which the census marks HARMLESS, the layer's). **No
`game-value` row is read by any enemy skill cast by its own kind.**

**What it took to reach the cast** (the field part is the fragile one): the
first arrival and the square walk (`up,left,down,right`) gave an encounter
in 9 of the 20 kinds; the others timed out in the field (no encounter in
3,000 frames: the arrival's zone, a wall, or a placement that failed - not
told apart) and were tried again from the area's arrivals with a longer
walk (`up x3, left x3, down x3, right x3`). Two walks went through a link
into the next area and fought its own rows (138 into 139, 76 into 77) - the
prologue's area peek shows it - so those two kinds were taken in the area
they reached (Egg Gang 139 / 2, BloodBat 77 / 1). Area 3's Gonghead met
nothing from four arrivals; area 95's did.

**Not reached**: the stallion route's first fight (boss id 23, area 67 slot
0, cut 1939): its enemy made no ability pick within 300 frames of the
Defends on either install - not read further. The seven boss-only skills
without a route (above), and the 15 ids no enemy carries.

### 11.5 The recipe for one kind

```
python tools/spell_sweep.py run --route tools/recipes/combat.txt --cut 501 --members 3 \
    --side enemy --kind 22:0 --at 0x600000:0x448000:7 --ids 9 --enemy-odds 6 --enemy-flags 0xE0 \
    --root <root> --launcher <ini-less launcher dir>\bof3x-launcher.exe --speed 4 --confirm circle
```

`--at` from a door-sweep `steps.json` (an `arr_x`, `arr_z` and the door's
`flags` whose `arr_area` is the area), `--enemy-odds` / `--enemy-flags` from
the kind's area record (`tools/enemy_ai.py --area A` reads the file; the two
bytes are `+0x0E` and `+0x12`). A boss kind: the route that fights it and
its cut, no `--kind`. From a worktree with a disc-only install,
`BOF3X_RECIPE_SAVES` names the main checkout's `tools/recipe_saves`
(section 10).

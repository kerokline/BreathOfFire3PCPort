# The spell sweep: every ability cast in a battle that cannot end

**Status:** MEASURED (2026-10-26: built and run - the three sides on the
PC install, the member and enemy sides on a disc-built install before and
after the twin-read placements, sections 4 to 7; open: Howling on the disc
install, the Super Combo text row, the owner's eye). The owner's idea the
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
kind). This is how the 30 enemy-only abilities are reached: a member casting
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

An A/B on one cast: `--ids 68 --chunk 1`, `--poke "0x93BA1F u8 1"` (an
`input_run` poke added to every cast after the sweep's own) and `--peek
"0x93BA1F u8 e_fire"` (a peek logged after the shots), both repeatable; the
same run with `--env BOF3X_ORIGINAL=<the functions>` is Capcom's code on the
same fight. `--sibling DIR` names the archival checkout whose `names/` give
the ids (the default is the checkout's parent, which a worktree lacks).
Added 2026-10-26 for the Foretell question (section 4).

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

The enemy's command kind (`+0x105`) peeked after the shots reads 0 on
every id - the record is cleared by then; the peek is in the wrong place
and says nothing.

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
  enemy-only skills want a sweep by enemy kind - each cast by an enemy that
  has it - which this does not do.
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

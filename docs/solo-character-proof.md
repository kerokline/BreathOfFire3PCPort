# The solo-character round's proof batch: the off-proof, the solo routes, the boss sweep with a team of one

**Status:** BUILT (2026-11-12, group RT of [`solo-character-round.md`](solo-character-round.md) 4 "RT";
branch `solo/rt` off the round's stage A tip `10646687`). Three runners and a shared module under
`tools/`; each smoke-run on the stage A tip, where the mode stamps the save's mark and changes no battle
or field yet (section 6). The full ten-route off-proof is the coordinator's, on the merged branch, on a
quiet, hands-off machine (round doc 5). The recordings the owner owes are section 5.

Tooling only: no `src/` change, no ledger entry. Everything a run writes - launcher copies, the game
folder, saves, crafted saves, state hashes, logs, frames - stays in the harness root, outside the tree
(CLAUDE.md rule 1).

## 1. The root and the shared module

`tools/solo_harness.py` holds what the three runners share. A root is prepared once per runner (each
`prepare` takes the build directory and makes the root's own copies):

| In the root | What |
|---|---|
| `Lbase\`, `Lcand\` | `bof3x-launcher.exe`, `bof3x-launcher-dev.exe`, `bof3x.exe`, `bof3x.dll` of one build; **no `bof3x.ini`** (a play ini's cheats fail tests), removed again before every run |
| `g\` | `BOF3.exe`, `BOF3.CFG`, `capcom.avi`, `logos.avi` copied from `--game-src` (default the main checkout's `bof3\`); `DAT`, `BGM`, `SND` junctioned to it, read only. The slot files the runs swap live here, not in the owner's game |
| `saves\` | the recipe saves copied from `--saves-src` (default the main checkout's `tools/recipe_saves`), handed to the game as `BOF3X_RECIPE_SAVES`; the boss sweep's crafts are written here too |
| `STOP` | a file of this name ends a batch before its next run |

Every run goes through `tools/input_run.py` with the exe host, `--no-front`, `--slot0-wait 2400`, and an
environment with **every inherited `BOF3X_` variable removed** before the runner sets its own. Runs are
sequential: before and after each one the runner waits until no `bof3x`, `BOF3` or launcher process whose
path lies under the root is alive (a launcher call returns before the game ends). A game is only ever
ended by `input_run.py`, by its own pid. Roots belong under `D:\bof3x-runs` (an excluded folder).

## 2. The off-proof: `tools/solo_proof.py`

What it proves: with the solo behaviours off, the candidate build's memory is the base build's, frame
for frame, on the ten state-hash routes; and with `SoloCharacter` on but no champion, or on with a
champion on a route that stays in chapter 0, the mode stands down and the memory is still the base's.

The method is the 2026-11-12 proof chain's (`D:\bof3x-runs\de1112\proof\chain.sh`; [`state-hash.md`](state-hash.md)
6): the references are a **pair of base-build runs recorded fresh** (the `orig4` pairs are stale,
[`HANDOFF.md`](HANDOFF.md) 14b); the candidate is run once and `tools/statehash.py check REF REFB NEW`
compares it where the pair agrees. A route that differs gets a second candidate run checked against the
same pair; if that one is identical the first is reported as noise, and if both differ the base is
checked against the candidate pair (`recheck_*`) to say whether the base lies inside the candidate's own
spread. Every run: `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the skip list of the runner's tree
(`tools/statehash_skip.txt`), `--slot0-hold` on the shop route, speed 1. A `cache_base\` or `cache_cand\`
folder in the root, if one is made, is handed over as `BOF3X_CACHE` (none is needed on the PC install).

```
python tools/solo_proof.py prepare --root D:\bof3x-runs\<run>\proof --base-build <main at the merge-base>\build --cand-build <merged tip>\build
python tools/solo_proof.py all     --root D:\bof3x-runs\<run>\proof            # refs, then off, on-none, on-champ:ryu
python tools/solo_proof.py report  --root D:\bof3x-runs\<run>\proof
```

or step by step: `refs` (the pair per route; kept on a second call unless `--again`), `check --variant
off`, `check --variant on-none`, `check --variant on-champ:<champion>`; `--routes` narrows any of them,
`chapters` prints each route's save chapter.

| Variant | The candidate is run with | A pass |
|---|---|---|
| `off` | no `BOF3X_ON`, no `BOF3X_SOLO` (the behaviours' default) | `identical` on all ten |
| `on-none` | `BOF3X_ON=SoloCharacter`, `BOF3X_SOLO` unset: nothing stamps the mark (`Solo_Stamp` needs a launcher champion) and the mode is not active | `identical` on all ten |
| `on-champ:NAME` | `BOF3X_ON=SoloCharacter`, `BOF3X_SOLO=NAME`, on the routes whose save is chapter 0 (the mode stands down through the intro, round doc 6a item 3). The run's log is read for the stamp line: a route that reaches chapter 1 and stamps is reported "does not qualify" | `identical` with no stamp line |

**Which routes qualify for `on-champ`** (the chapter byte, block `+0x00`, of each route's save, read by
`chapters`): only `whelpBoss`'s save is chapter 0 (`whelp_boss`); the others' are chapter 1 (`combat`,
`shop`, `caughFish`, `cutsceneAndNue`, `worldMapAndAreaTransition`), 3 (`masterAndManillo`) or 14
(`field_menu`, `menu_screens`, `dragonTransform`). **And `whelpBoss` does not qualify either, measured:**
the route reaches chapter 1 at recipe frame 3,315 and the game stamps the mark there (section 6), so
**no state-hash route stays in chapter 0 throughout**. `on-champ` on `whelpBoss` is still worth its run:
up to tick 3,314 it is the chapter-0 stand-down shown, and the one page that differs from 3,315 on
(`0x904000`, the mark's) is the stamp. `--all-routes` runs the variant on all ten: there the mark is
stamped, the mode is active and, once CB and FS are merged, the battles change, so a difference is
reported and not judged. A stamped run is not repeated.

Output: `<root>\sh\cn_<route>_<side>.sh` (sides `ref`, `refb`, `off`, `off2`, `onnone`, `onryu` ...),
each run's `.runlog` (input_run's output) and `.log` (the game's), `check_*.txt`, `recheck_*.txt`,
`diff_<route>_ref_refb.txt` (how far the pair is from itself); `<root>\results.tsv` one row a route and
variant, appended (the verdict, the first difference: page count, first tick and the first pages with
their symbols; the stamp line); `<root>\proof.log` the batch's lines.

**Run time** (the 2026-11-12 chain's per-run times, speed 1): combat 100 s, shop 117 s, caughFish 142 s,
field_menu 64 s, menu_screens 74 s, dragonTransform 157 s, cutsceneAndNue 263 s, masterAndManillo 197 s,
worldMapAndAreaTransition 84 s, whelpBoss 452 s - **1,650 s, about 28 minutes, one run of all ten**, plus
some 15 s a run of start-up and waits. `all` is two reference runs, `off`, `on-none` (ten runs each) and
`on-champ` (whelpBoss): 41 runs, **about 2 h 10 min** with no repeat; each differing route adds one run.
A pass of `all`: `off` and `on-none` `identical` on all ten; `on-champ` on `whelpBoss` `differs ...
(stamped: route does not qualify)` with the one page `0x904000` from the stamp's tick (3,315 at stage A)
and nothing before it - a stamped route is reported in its row and does not set the exit code; the
coordinator reads that row's first difference (one page, from the stamp's tick).

## 3. The battle routes with a team of one: `tools/solo_routes.py`

What it shows: whether each battle route still plays to its end with one member fighting, and what the
party and the champion's record are before and after. Each route is run plain once (no behaviour) and
then once per champion (`BOF3X_ON=SoloCharacter`, `BOF3X_SOLO=<name>`; `--if-present` adds
`SoloChampionIfPresent`), from a copy of the route with watches put after its header (a watch takes no
frame, so the route plays as recorded): the field mode, the fight byte, the party block `0x904060`
(formation, both lists), `Field_MemberCount`, the party set, the mark `0x904A21`, the chapter, and each
record's id, level, joined bits, EXP, HP and AP. Speed 4 by default (the recipe is frame-indexed), `en-US`
as the routes were recorded.

```
python tools/solo_routes.py prepare    --root D:\bof3x-runs\<run>\rt --build <merged tip>\build
python tools/solo_routes.py run        --root D:\bof3x-runs\<run>\rt --champions ryu nina garr rei momo peco
python tools/solo_routes.py robustness --root D:\bof3x-runs\<run>\rt
python tools/solo_routes.py report     --root D:\bof3x-runs\<run>\rt
```

The routes (default): `combat`, `combatGroup`, `combat_exp`, `dragonTransform`, `gameover`,
`quick_combat`; `--routes` takes any recipe name. Verdicts, per run:

| Verdict | From |
|---|---|
| ran to its end, in step with the plain run | the recipe's `done`, and the battles (each stay at field mode 5, first and last frame) and the end mode are the plain run's |
| desynced | the recipe ran to its end, but the battles or the end mode differ from the plain run's - with a team of one, a recorded route's later presses land on other menus |
| failed | the recipe's own check gave up (`input FAILED`) |
| hung | `input_run.py`'s `--minutes` ran out |
| crashed | a `FATAL` or `CRASH` line in the game's log, or the game gone before the recipe's end |

"Before" is the value at the first battle's first frame, "after" the run's last. Output:
`<root>\routes\` (the watched copies, `<route>_<side>.runlog` / `.log`, frames),
`<root>\routes_results.tsv` and `.md` (the latest run of each route and side); `summarise` rebuilds the
rows from the logs. Run time: about a minute a run at speed 4 (section 6), so six routes with the plain
side and six champions is 42 runs, **about 45 minutes**.

**Input robustness** (`robustness`, from the plain run's battle windows and the route's own presses)
- a route that presses only confirm buttons in battle still plays when the menus change; one that moves
a cursor in battle picks something else once the turn order is different:

| Route | Frames | Battle (field mode 5, plain run) | Presses in battle: confirm / other | Other buttons | Verdict |
|---|--:|---|--:|---|---|
| `combat` | 2,620 | 404 to the end | 26 / 29 | the cross's four directions | not input-robust |
| `combatGroup` | 622 | 611 to the end | 0 / 0 | - | input-robust, but only because it ends 11 frames into the battle: it shows the start, not a fight |
| `combat_exp` | 3,764 | 404..2,647 | 26 / 29 | the four directions | not input-robust |
| `dragonTransform` | 4,329 | 646..4,228 | 17 / 52 | the four directions, triangle | not input-robust in general - **but its save's party is already one member** (record 0 alone, id 7, `Field_MemberCount` 1, measured), so with `BOF3X_SOLO=ryu` CB has nothing to change and the route is expected to stay in step |
| `gameover` | 3,081 | 338 to the end | 39 / 60 | the four directions, triangle | not input-robust |
| `quick_combat` | 2,920 | 404 to the end | 27 / 31 | the four directions, F6, F7 | not input-robust (it is `combat.txt` with the quicksave keys; `tools/quicksave_check.py make` rebuilds it from a new combat recording) |

So every recorded fight but `dragonTransform` with `ryu` is expected to desync once CB fights with one
member; `solo_routes.py run` reports where (the battles' frames against the plain run's), and the routes
of section 5 replace them.

## 4. The boss sweep with a team of one: `tools/solo_boss_sweep.py`

What it shows: per boss fight, whether it starts, ends, hangs or crashes with one member, the mode on.
The fights are the boss sweep's pre-fight saves ([`boss-sweep.md`](boss-sweep.md) 6): the scene-started
fights 28 (the owner's `midEyeEncounter`), 38 (`bf38c`), 50 (`bf50c`), 54 (`bf54b`), 55 (`bf55b`), staged
as 7.3 staged them, and chapter 15's sample fights 39..48 (`bf39`..`bf48`, staged with `--scene` at the
entry's own start - the 2026-10-28 sweep did not play them, so their plain rows are new too). `prepare`
makes the crafts with `tools/chapter_jump.py make` into the root's `saves\`. The recipe is
`tools/spell_sweep.py`'s fight recipe (the route to the cut, the fight's watches, the party's HP and DEF
999, three rounds of Defend where 7.3 had them, the kill, the return) with the party watches of section 3
put beside the fight's (62 of the op's 64 watches with four enemies).

The crafted saves hold no mark: the game stamps the launcher's champion at the load's first area entry
(`Solo_Stamp` in `Area_Enter`; all of these saves are chapter 9 to 15), so the sweep runs from the saves
as they are.

```
python tools/solo_boss_sweep.py prepare --root D:\bof3x-runs\<run>\boss --build <merged tip>\build
python tools/solo_boss_sweep.py list
python tools/solo_boss_sweep.py run     --root D:\bof3x-runs\<run>\boss --champions ryu nina garr rei momo peco
python tools/solo_boss_sweep.py report  --root D:\bof3x-runs\<run>\boss
```

Verdicts: `started, ended (win | loss | by the set-up's script)`, `started, hung (no return to the
field)`, `did not start` (with the recipe's failing line), `crashed`. A fight that does not start or
hangs with one member is a finding for CB, or a line in the entry's "what stays the original's" - never a
skip. The table also gives the party block and the member count at the battle's first frame, the mark,
and the champion's record at the start and the end. `--members` (default 3) is how many working records
the party pokes cover, as `spell_sweep.py`'s default; with CB merged a one-member battle reads only the
first. Output: `<root>\boss\` (the recipes, frames, logs), `<root>\boss_results.tsv` and `.md`. Run time:
about a minute and a half a fight at speed 4 (section 6): fifteen fights, the plain side and six
champions, 105 runs, **about 2 h 30 min**; `--fights` narrows it.

Added 2026-11-12 evening, at the owner's word ("the zombiedragon save, which is a 2 man team/fight"):

- **Fight 32, the D.Zombie**, a route-reached fight: `minecartToZombieDragon.txt` kept to its cut 5230,
  then `battle 32`, `--after 1200 --after-confirm`, as boss-sweep.md 7.3's row. A table entry with
  `route=` uses the recorded route (and its own `# save`) in place of the probe; the other fights are
  unchanged. The route's save has a party of two (first list `2 7`, second `7 2`, count 2).
- **The route fights an earlier battle** on the way to the cut (battle 372). With `BOF3X_SOLO` set the
  champion fights it alone and the recorded presses no longer finish it as recorded, so the route reaches
  the boss out of step. **`--mark-at-cut`** is the way to the boss in step without a new recording (the
  coordinator's): `BOF3X_ON=SoloCharacter` with **no** `BOF3X_SOLO` through the route (the mark stays 0,
  the mode is inactive, the route replays as recorded through battle 372), then at the cut the mark
  `0x904A21` is poked to the champion's record + 1 before `battle 32`; sides `mark_<champion>`. A
  harness's poke, not a player's path.
- **Solo sides wait for the cross** with `until` where `spell_sweep.py`'s fight recipe has `seek <confirm>
  phase == 1`: with one member a confirm pressed as the cross opens enters the command and phase 1 ends
  two frames later, between the seek's checks, so the seek gave up while the battle ran on (fight 32 with
  ryu, momo, peco under the seek: "hung", the battle in fact still turning). `--seek-cross` keeps the
  seek. Plain sides are unchanged.
- The table gained `joined` (the records with `+0xB` bit 0 at the call), `party_before` (at the call),
  `party_in_battle` (60 frames into mode 5, after CB's write), `party_after_return`, and the game's own
  `DIV-0228` arm and restore lines (`solo_battle`, `solo_restore`).

## 5. Recordings owed by the owner

Recorded on the merged branch (CB, FS, MS, LV in), with the mode on: the launcher's Solo character page
with the mode's box ticked and the champion chosen (or `BOF3X_ON=SoloCharacter BOF3X_SOLO=<name>` with
the developer launcher), `BOF3X_RECORD=<path>`, `en-US`, the play folder's slot loaded as below. A route
recorded with the mode on stamps its save's mark at the first area entry, so it replays only with the
same champion: the champion goes into the route's header comment beside `# save`, and
`solo_routes.py run --routes <name> --champions <the same>` plays it. Each file goes to
`tools/recipes/` (a route is the owner's inputs, not game data), its save is the existing recipe save
named.

What the saves hold, measured from the watches of section 6 (the records' joined bit, `+0xB` bit 0, at
the first battle's first frame):

- `combat` (chapter 1): records 0, 3 and 4 joined (record 0 also "must stay"); the party lists hold ids
  0, 4, 3. Of the six champions only `ryu` (record 0) and `rei` (record 4) have joined; a champion of
  the other four fights from a record the story has not reached ("champion always", round doc 6a item 2;
  record 1 at level 5, record 2 at 13, record 5 at 10 there).
- `adult_ryu` (chapter 14): all six champions' records joined (0, 1, 2, 4, 5, 6); the party is record 0
  alone (id 7).

| # | Route to record | Save | Champion | What to do | About |
|--:|---|---|---|---|---|
| 1 | `soloCombat` | `combat` | `rei` (joined, not the walker: CB's "won by the champion alone with a different walker") | the encounter `combat.txt` walks into; win it with the champion alone; confirm through the result windows back to the field (as `combat_exp` does) | 1 min |
| 2 | `soloCombatAbsent` | `combat` | `nina` (not joined in chapter 1: "champion always" fighting from an unjoined record, CB's gate in round doc 6a item 2) | the same encounter, won or lost, through to the field or the GAME OVER | 1 min |
| 3 | `soloGameover` | `combat` | `rei` | the same encounter, lost with the champion alone (Defend until the champion falls), through GAME OVER to the title | 1 min |
| 4 | `soloFlee` | `combat` | `rei` | the same encounter, Escape (R1) until it works, then a few steps on the field | 40 s |
| 5 | `soloSwap` (the field route for the L swap) | `adult_ryu` | `ryu` (so the walker changes while the champion stays) | on the field: L once a second until every joined character has walked (all six champions' records are joined there), a door into the next area and back, the members screen opened (the camp's or the field menu's) and closed | 1 to 2 min |
| 6 | `soloSwapRei` | `adult_ryu` | `rei` | the same as 5 with a champion who is not record 0: the battle that follows any walker, if an encounter comes - otherwise as 5 | 1 to 2 min |

Not owed as recordings:

- **`quick_combat`'s solo form**: `tools/quicksave_check.py make` builds it from route 1 (the take and the
  restore frame chosen again inside one turn of the new fight, as `quick_combat.txt`'s header explains
  for frame 900).
- **`dragonTransform` with `ryu`**: the existing route, its party already one member (section 3).
- **A champion's dragon form with another walker** cannot be recorded in chapter 14 from `adult_ryu`
  (the party is record 0 alone): it wants a save where record 0 is not the walker; route 5's end (a
  walker other than record 0) saved in slot and fought from would be it, on the owner's word.
- **The boss fights**: section 4 stages them from saves; no recording.

## 6. As run on the stage A tip (2026-11-12)

Root `D:\bof3x-runs\solo1112\RT\root` (`proof\`, `rt\`, `boss\`); the base build `main` `ca6aacc6`
(the round's merge-base, a detached worktree `RT\base`), the candidate the stage A tip `10646687`, both
i686. Four other agents were compiling on the machine throughout, so timing-sensitive pages could have
differed for that reason alone; none did.

**The off-proof subset** (`refs`, `check --variant off`, `check --variant on-none` on `combat` and
`field_menu`):

| Route | ref / refb (the base against itself) | `off` | `on-none` |
|---|---|---|---|
| `combat` | identical on all 2,561 ticks | identical on all 2,561 compared ticks | identical |
| `field_menu` | identical on all 1,409 ticks | identical on all 1,409 | identical |

Every run `done` at the route's last frame (2,620 and 1,454), no Fatal; the base logs `behaviours: 0 of
10 on`, the candidate `1 of 12 on` under `on-none` (the two new switches counted). No repeat was needed.

**`on-champ`** (`refs` and `check --variant on-champ:ryu` on `whelpBoss`): the pair identical on all
13,185 ticks; the candidate with `BOF3X_SOLO=ryu` identical up to tick 3,314, then one page, `0x904000`
(it holds the mark `0x904A21`), from tick 3,315 to the end - the frame the log's stamp line names
(`... stamped into this game's save mark, chapter 1`, right after the load of `AREA009.DAT`): the route
leaves chapter 0 there. A repeat run (made before a stamped run stopped being repeated) differed the same
way. **The control** (`check --variant on-champ:ryu --routes combat --all-routes`): one page,
`0x904000`, from tick 337, the stamp at the save's first area entry - so the check sees a one-byte write
of the mode, and nothing else of stage A shows on the hash.

The state hash's own `statehash   watch:` notes (a word that happens to hold an address of ours) differ
in number between the base's and the candidate's logs (20 and 26 on `combat`): the DLLs are laid out
differently, so other `.data` words collide with function addresses; a note, not a difference.

**The battle routes** (`solo_routes.py run --champions ryu`, speed 4): all six `ran to its end, in step
with the plain run` - the expected smoke result, since stage A changes no battle. The champion's mark
read 0 before the first area entry and 1 (record 0 + 1) after it in every solo run, and the log carried
`DIV-0228 SoloCharacter on, BOF3X_SOLO=ryu (record 0)` and the stamp line (`... stamped into this game's
save mark, chapter 1`): the launcher hands `BOF3X_SOLO` from the environment to the game with no ini.
Party lists, count and the champion's record were the plain run's (the `combat` save: formation 0,
first list `0 4 3`, second `0 3 4`, count 3; `adult_ryu`: first and second list `7`, count 1). A run took 42 to 114 s.

**The boss sweep** (`solo_boss_sweep.py run --champions ryu`, plain and solo, all fifteen fights): every
fight `started, ended (win)` on both sides, at the same start and return frames on both. Fights 39..48 and
50 (donor `adult_ryu`) and 28 / 55, 38 / 54 (donors with three members) all staged; 25 to 45 s a run. The
champion's HP after reads 999: the harness's party pokes, not the game's. Note for the coordinator: with
`ryu` the sample fights and the Chimera change nothing even with CB in - their donor's party is record 0
alone - so the sweep wants the other champions (`--champions nina garr rei momo peco`) to test a team of
one built from a record that is not walking.

## 6a. Fight 32 on `solo/round` `c982fc84` (2026-11-12 evening)

The combat half, the mark and the level-1 start on the game's own party; root
`D:\bof3x-runs\solo1112\RT\root\dz`, speed 4. **In the save at the call, records 0 and 2 have joined
(ryu, garr); records 1, 4, 5, 6 (nina, rei, momo, peco) have not.** Before every battle: formation 0,
first list `2 7`, second `7 2`, count 2, set `8B`.

**The boss, reached in step (`--mark-at-cut`)**, every champion:

| Side | In battle (second list, count) | The game's line | Verdict | After the return |
|---|---|---|---|---|
| plain | `7 2`, 2 | - | started, ended (win) | first `7 2`, second `7 2`, count 2, set `8B` |
| mark_ryu | `7`, 1 | battle 373 fought by the champion alone (record 0, id 0x07); set kept | started, ended (win) | the plain run's; `party put back (restored mask 0x1F)` |
| mark_nina | `8`, 1 | record 1, id 0x08; set row changed, the battle file loaded | started, ended (win) | the plain run's; mask `0x3F` |
| mark_garr | `2`, 1 | record 2, id 0x02; set kept | started, ended (win) | the plain run's; mask `0x1F` |
| mark_rei | `4`, 1 | record 4, id 0x04 (level 5, unjoined); set row changed | started, ended (win) | the plain run's; mask `0x3F` |
| mark_momo | `5`, 1 | record 5, id 0x05; set row changed | started, ended (win) | the plain run's; mask `0x3F` |
| mark_peco | `6`, 1 | record 6, id 0x06; set row changed | started, ended (win) | the plain run's; mask `0x3F` |

Every restore line reads `second list 07 02 FF count 2 formation 0 set 8B; first list 02 07 FF`, and
the field after the return (first `7 2`, second `7 2`, count 2, set `8B`) is the plain run's in all six.
The win is the harness's kill (the HP poke), the champion's HP 999 after it the party poke.

**The route's first fight, battle 372** (sides `solo_<champion>`, `BOF3X_SOLO` set from the start): the
champion fought it alone in all six (the arm line, second list `07 02 FF` count 2 saved). Five
returned with `party put back` (ryu, garr mask `0x1F`, set kept; nina, momo, peco mask `0x3F`, the set
row changed and loaded). **rei (record 4, level 5, unjoined) lost it**: `battle 372's saved party
dropped at GameMode_Start (the battle did not return to the field)`, the game back at its start, so
the route never reached the boss (`battle 32 refused: no tagged actor ... area 31`). After battle 372
the route is out of step, as the owner saw. Of the five that went on, garr and nina won the boss;
ryu, momo and peco "hung" under `spell_sweep.py`'s seek, which the `until` change above answers (the
battle was still turning: enemy states and phases changing to the recipe's last frame).

**combatGroup with garr** (`D:\bof3x-runs\solo1112\RT\root\cg`): an earlier run with the now-removed
map fold ended `game exited rc 1` as the battle loaded. On `c982fc84` it did not recur in four runs:
the route once (`done` at 622) and three times with 900 more frames after its end (`done` at 1,522),
each with `battle 4 fought by the champion alone (record 2 ...)`, the battle file loaded (record 2
unjoined in chapter 1), no Fatal, no CRASH line, the battle still on at the end.

## 6b. The unusual boss encounters (2026-11-12 night)

Build: the `bx`, `cx`, `gx` rows on `solo/round` `59137505` (before the step-7 restore fix `a6000629`);
the walk-in check on `94a61619`. Roots `D:\bof3x-runs\solo1112\RT\root\{bx,cx,gx,mikba}`. All boss sides
`--mark-at-cut` (the mode on, no `BOF3X_SOLO`, the mark poked at the cut). Solo sides wait for the cross
with `until` (section 4). The harness now passes `--slot0-shared` (the root's own game folder; the owner
was playing from `game\` beside it) and drops a stale `bof3x.log` before each run: rows written between
21:22 and 21:24 had read the previous run's log while input_run refused the slot swap, and were rerun.

| Fight | Plain | The six champions | Notes |
|---|---|---|---|
| 2 Nue | win by the enemy's escape | all six: the same ending, party after and scene after as plain | nina crashed once (nvwgf2um.dll, after `put back` mask `0x3F`, mode 5 step 7): the known driver class; the rerun passed |
| 7 Worker, Operator (chapter 0) | ended by the set-up's script | ryu: no arm line (`sl_chapter` 0), as plain | the stand-down's evidence |
| 8 kind 8 | win | all six as plain | nina crashed once, fight 2's class; the rerun passed |
| 12 Amalgam | win | all six as plain | |
| 13 Balio, Sunder (the story's loss) | loss | all six: loss, the party after as plain, the story goes on to area 3 as plain's (modes 2 1 2), **no GAME OVER** | the champion alone falls before any cross opens; the harness's wait for the cross then times out after the return (the recipe FAILS there, the game does not) |
| 16 Balio, Sunder, Nina | win | ryu (already alone), nina, garr as plain; **rei, momo, peco crash, every time** | **finding**, below |
| 23 Bullies | win | all six as plain | |
| 24 Stallion | win | all six as plain | rei once `FATAL: render: Present failed, HRESULT 0x887A0005` after `put back` mask `0x3F` (the driver class); the rerun passed |
| 29 Dolphin | win | all six as plain | |
| 35 Gaist, Torch | win by an escape | garr as plain; ryu (already alone), nina, rei, momo, peco "hung" | the harness's `until` for the cross: phase 3 waits on presses for 1,800 frames, which plain's seek presses through. Not a game result; owed again |
| 25 Beyd (set-up 25, craft `bfs25`) | started (fight 0x19, mode 5 at 444) | ryu: `already has the champion alone` (SortParty's list); the other five **arm** | before CB's Beyd stand-down; the kill never sees a step 2 (a minigame), so every side ends "hung" in the harness |
| 34 (craft `bfs34`) | did not start | did not start | a smoke run of set-up 34 only, NOT the Mikba result: the donor's two-member party, and Scene11's step 7 party prompt (`Request(6)`, field mode 7) never closed by the probe's presses. The walk-in save below replaces it |
| 22 Garr (craft `bfs22`) | did not start | did not start | the scene stays in a talk (mode 4) for 600 frames: the craft does not reach the call |
| 31 Garr, Angel Tower (craft `bfs31`) | crashed at the area's load | crashed | `Sprite_LoadPalette` at area 0x55's load (mode 1) on plain too: the craft's state, not the mode's |

**Fight 16 with a champion whose party-set row lacks id 1.** Battle 88: the arm line `set row changed`
(rows `0x0D` (4, 5, 7) for rei, `0x10` (5, 6, 7) for momo, `0x12` (6, 7, 8) for peco), then `CRASH 0:
exception 0xC0000005 at bof3x.dll+0x10D613` in mode 5 step 5, reading `0x05D04B62`; the DLL's symbols
put the pc in `Sprite_LoadPalette`, with `Field_MemberSprite` and `Boss16_Event` on the stack. Read in
`src/game/boss/boss_nina.cpp`: `Boss16_Event` code 2 calls `Field_MemberSprite(1, 1)` to dress enemy 2 as
character id 1, and a member sprite comes from the loaded party set's row. Plain's row and the row the
mode keeps for ryu, nina, garr (row 0, ids 0, 1, 2) hold id 1; the rows CB loads for rei, momo, peco do
not. Rerun for rei: the same pc. Deterministic: the set-up reads a character by id from the party set.

### The Mikba walk-in save (`tools/solo_walkin.py`)

Fight 34 is started by chapter 8's run 0xB, `Scena08_Scene11` (`scena_08_2.cpp`), which
`Scena08_ArriveHook` starts on area 0x2B (43) with chapter flag 0x14 set and 0x15 clear, at z 0xF0000 or
more and x cells 0x1A..0x26. Its step 0 runs `DropIn(0)` and `PartySet_LoadFirst(7, 8, 4)`; step 7
`Scena08_SetUpRecord4`, the party prompt (`Request(6)`), must-stay on records 0 and 4, record 4's HP 0
and status `| 0x60`; step 9 `DropIn(2)` and the placement; step 0xA the fight once counter 0 is 0x1A.
Flag 0x14 is set by run 10 (`Scena08_Scene10`, started by area 0x15's entry with 0x13 set and 0x14
clear). So the latest point before the game builds the fight's party is area 43 with 0x14 set, before the
hook's cells. `mikba43`: donor `minecartToZombieDragon`, chapter 8, run 0 (state 2, as the owner's saves
hold it), area 43 at (0x110000, 0x170000) flags 0x84 (where Scene11 itself puts the party in area 43
after the fight), chapter flags 0x0..0x3 (the donor's) and 0xF, 0x10, 0x11, 0x13, 0x14 set, 0x15 clear;
lists `07 08 04` both, party set `0x8F` (row 0x0F, ids 4, 7, 8), records 0, 1, 4 joined (the owner's ask;
record 2 joined as the donor has it, 3, 5, 6 not), the solo mark 0. Chapter 8's flags 0x4..0xE are the
donor's (clear): the areas whose entries test them (1, 2, 3, 0xC, 0xD, 0x12, 0x23) may start earlier
chapter-8 scenes if walked into. Loaded headless (`BOF3X_SOLO=nina`): the field (mode 2) in area 43 with
three walkers (count 3, lists `07 04 08` after the load's own sort), chapter 8, the stamp line, no Fatal.

### 6c. The owner's routes: fight 31 (`preGarrFight`) and fight 34 (`mikbaFight`)

**Fight 31, the Angel Tower Garr fight** (root `D:\bof3x-runs\solo1112\RT\root\gf`; the straight replay and
the `f31_*` rows on `953de965`, the same `src/` as `205f1e12`; the `f131_*` rows on `2d745a5f`). The
straight replay, mode off, loads slot 0 (chapter 7, areas 83 then 85) and stays in step to its end: the
dismissal at frames 324..1069 (first list `0 1 2` count 3, then `2 0`, count 2 at 684, then `0` alone,
count 1 at 1069; record 2's joined bit cleared at 1069), the fight byte 0x1F at 4064 (`boss encounter: id
31`), the return at 6764, the chapter byte 7 -> 8 at 9428 with area 32 loaded, the party `0A` at 11340
and `07` at 12002. The recording ends with F9 twice: the pause, then the return to the title (the
records read 0 at 14415), and the next F9 at the title ends the program, which `input_run.py` reports as
"FAILED ... the game exited": the owner's quit, not a fault. From the code: chapter 7's
`Scenario_CallB(1)`, called by run 6's step 0xB before the fight, is `Scena07_Leave2Out1562`
(`docs/scena_calls.md`): `Party_Remove(2)`, then the joined bit of ids 1, 5, 6 and 2 cleared - the
dismissal; run 6 itself writes no record. Record 2 keeps EXP 15,859 through the chapter change in the
plain replay (its HP 152 -> 178 at 9428).

| Side | Arm line | Verdict | After the return |
|---|---|---|---|
| plain (the sweep's kill) | - | started, ended (win, enemies defeated) | first and second `0`, count 1, set `80`; the scene's talk, then mode 2 |
| garr | battle 306 fought by the champion alone (record 2, id 0x02, **joined 0**); set kept (`80`) | the same; record 2 EXP 15,859 -> 16,259, the result window "You gain 400 EXP!", "Garr ... EXP to next level 22" | `put back` mask `0x1F`; the same as plain |
| ryu | battle 306 already has the champion (id 0x00) alone | the same | as plain |
| nina | record 1 (joined 0, HP 1 before) | the same | mask `0x1F`; as plain |
| rei | record 4 (joined 0, level 5) | the same; record 4 level 5 -> 8 | mask `0x3F`; as plain |

**The scene after the fight** (`f131_*`: the route's own presses replayed after the return, aligned on
the return's area reload, which comes 34 frames after the return on every side): plain, garr and ryu each
reload area 85, open the talk 84 frames after the return and close it 696..700 frames later, as the
owner's run does (6,848..7,545). After the talk the owner walks; the replayed walk does not bring the next
talk (the owner's at 8,022) on any side, plain included, so the chapter change is not reached in the sweep
- only the straight replay reaches it (9,428). Record 2 on the garr side: EXP counted up 15,859 -> 16,259
in the result window, HP 999 (the harness's poke), joined 0; in plain record 2 is untouched by the fight.
So a champion-garr fight leaves record 2 with 400 more EXP than the story gives (plain carries 15,859 into
chapter 8); its HP is the battle's write-back.

No `a frame of no pieces` line in any fight-31 log. **Pictures** (garr side): the camera frames the
enemy only, in plain as with every champion - the party member is off screen and shows in the status box
(`Garr AP 37 HP 999`). The boss Garr is drawn in its own colours (`f31_mark_garr\w004170.png`); the
champion's critical hit flashes it pale with "Critical" (`t004593.png`); it then kneels in its own colours
and the result window follows (`t004650.png`). Plain's boss falls flat after a normal hit instead; both
are the hit's reaction, no garbage or wrong palette on either side.

### Fight 34, the Mikba route (`mikbaFight`)

A constructed state the owner walked through: the save is `mikba43.DAT` edited by the owner - the
entrance moved to area 43 at (0x238000, 0x20000) and Garr in Rei's place (lists `07 08 02`, set `8A`;
records 0, 1, 2, 4 joined, record 0 must-stay; read from the file). The scene, its party prompt, the fight
and the second prompt are the game's own; the chapter flags around them are not a real save's.

Plain, mode off (`2d745a5f`, root `D:\bof3x-runs\solo1112\RT\root\mk`): first field at 255; the party
prompt (field mode 7) at 2804..2854, after which the lists are `07 08 04` and record 4 is level 19, HP 0,
status `0x6000`, joined and must-stay (Scene11's step 7); the fight byte 0x22 at 3548 (`boss encounter:
id 34`), mode 5 from 3549; the return at 14158, where record 4 is HP 1, status `0x2000` (the battle's own
write-back) and record 0 HP 77, status `0x0080`; the second prompt at 19128, record 4's must-stay cleared
at 19127; F9 twice at the end (the owner's quit, as above). With `BOF3X_SOLO` set from the start (CB's
fight rules merged): rei logs `the champion's record 4 has 0 HP; battle 372 is fought by the game's own
party`; nina, ryu and garr log `battle 372 is event battle 0x22, fight 34 ... the champion is record N,
not 4: fought by the game's own party`. **All four replay identically to plain**: the 171 changes of the
field mode, fight byte, chapter, both lists, the count, the set and records 0, 1, 2 and 4, frame for
frame, the second prompt included. So no champion leaves record 4 at 0 HP where plain does not: on this
build no champion fights fight 34 alone.

### 6d. The final table (2026-11-13)

Every boss row on one build, `solo/round` `a3bcacba` (CB's step-7 restore fix and fight rules in), root
`D:\bof3x-runs\solo1112\RT\root\bx`, `--mark-at-cut` on the champion sides, the sweep's kill ending each
fight. Solo sides now wait for the cross with the same seek pressed one frame on, one frame off (a check
every second frame sees the two frames phase 1 lasts after a command is entered, and the battle's
messages still get their confirms); a row that does not return says from the phase watches whether the
battle was still turning at the end. In brackets: what the mode did with the party-set row.

| Fight | plain | ryu | nina | garr | rei | momo | peco |
|---|---|---|---|---|---|---|---|
| 2 Nue | escape | escape (kept) | escape (row changed) | escape (row changed) | escape (kept) | escape (row changed) | escape (row changed) |
| 8 | win | win (kept) | win (row changed) | win (row changed) | win (kept) | win (row changed) | win (row changed) |
| 12 Amalgam | win | win (kept) | win (row changed) | win (row changed) | win (kept) | win (row changed) | win (row changed) |
| 13 Balio, Sunder | loss | loss (kept) | loss (row changed) | loss (row changed) | loss (kept) | loss (row changed) | loss (row changed) |
| 16 Balio, Sunder, Nina | win | win (already alone) | win (kept) | win (kept) | win (**stands down**: no row holds id 4 with id 1) | win (row changed) | win (row changed) |
| 23 Bullies | win | win (kept) | win (kept) | win (row changed) | win (row changed) | win (row changed) | win (kept) |
| 24 Stallion | win | win (kept) | win (kept) | win (kept) | win (row changed) | win (row changed) | win (row changed) |
| 29 Dolphin | win | win (kept) | win (kept) | win (row changed) | win (row changed) | win (kept) | win (row changed) |
| 35 Gaist | escape | escape (already alone) | escape (row changed) | escape (kept) | escape (kept) | escape (row changed) | not ended, the battle still turning (row changed) |
| 46 (sample) | win | - | - | not ended, the battle still turning (row changed) | - | - | - |

Every ended row: the same kind of ending as plain, the party after the return as plain's, the scene
after the return as plain's (fight 13: the loss carries the story on to area 3, as plain's). Restore
masks `0x1F` (set kept), `0x3F` (row changed), `0x15` / `0x35` on fight 16. **No crash and no `a frame of
no pieces` line in any of the 68 runs**; the three that hit the driver crash on `59137505` (fight 2 nina,
fight 8 nina, fight 24 rei), three runs each on this build: 9 of 9 ended, no crash, mask `0x3F` each time.

The two rows that do not end are the harness's way of ending a fight, not a fault of the mode: on fight
35 the boss's HP climbs back each turn (0x17C to 0x3C9 over 9,000 frames) after peco's kill round and the
escape the other sides reach never comes; on fight 46 (five enemies, `Air Raid`) garr's attacks swing
(the frames show them) but no enemy's HP falls after the kill poke over 8,000 frames, where adult Ryu
(plain) and, in the coordinator's runs, rei and nina win it. Fight 46 garr has to start with
`BOF3X_SOLO` set (`solo_garr`): its fight starts before the cut, so a mark poked there arms nothing (the
`mark_garr` row is the game's own party).

**The owner on these two rows (2026-11-13), how the fights play, not measured here:** fight 35 - "gaist is healed by his torches so if peco couldn't get through that the fight would go on indefinitely"; fight 46 - "I think that is maybe the chicken fight - air raid cancels a persons moves, so if you got hit too many times by air raid, and you were too slow to move first, it could reasonably cancel your move several turns in a row". Both read as the game's own rules meeting one slow or weak fighter, which fits the watches (the boss's HP climbing back; swings with no HP falling). Fight 46's enemy name is not read from the data.

**The other fights, for one table:**

| Fight | Build | Result |
|---|---|---|
| 7 (chapter 0) | `59137505` | ryu: no arm line, as plain (the stand-down through the intro) |
| 25 Beyd (craft) | `59137505` | the five champions arm (before CB's Beyd stand-down; not rerun since, as asked) |
| 31 Angel Tower Garr | `953de965` / `2d745a5f` | all sides win as plain (section 6c) |
| 32 D.Zombie | `c982fc84` | all six win, party put back as plain (section 6a) |
| 34 Mikba (owner's route) | `2d745a5f` | every champion stands down; identical to plain (section 6c) |
| 22 Garr, chapter 5 | - | no craft reaches it |

**Two Garrs, the last question: does record 2's extra EXP survive into chapter 8?** Read from the code,
not run: `Scena08_Start` (`src/game/scenario/scena_08_2.cpp`) sets the joined bit on all eight records,
calls `Party_HealJoined` (`0x533E50`), then clears the joined bit on records 1..7; it writes no EXP and no
level. So with champion garr record 2 carries its 16,259 EXP (plain: 15,859) into chapter 8, and the
battle's HP write-back is replaced by the heal - in the straight replay record 2's HP goes 152 -> 178 at
the chapter change (frame 9,428), which is that heal. The route with `BOF3X_SOLO=garr` from the start was
not run: the owner's battle presses would not line up with a different fighter's turns.

**Argued from the code only** (no run shows it): the chapter-8 heal above; what `Party_DropIn(1)` lists
on fight 31's area; the Beyd stand-down's effect on fight 25 on the current build; what fight 46 garr's
attacks do to five flying enemies (the frames show the swings, not why no HP falls); the ending fight 35
expects when its boss is never reduced; record 4's dragon form and revive inside the Mikba battle (the
record watches see the save block's records, not the battle's working copies).

## 7. Not done here

- **The full ten-route off-proof** on the merged branch, the machine quiet (the coordinator's, round
  doc 5): only `combat`, `field_menu` and `whelpBoss` ran here, on the stage A tip.
- **The solo routes and the boss sweep with the combat half in**: here they could only show that the
  harness runs and that stage A changes nothing (every row in step with its plain run). With CB merged
  the recorded routes are expected to desync (section 3) and the boss rows become the findings.
- **The other five champions** were not run (`ryu` only, the smoke test); `--champions` takes them.
- **`--if-present`** (`SoloChampionIfPresent`) is wired but was not run.
- **The 64-bit build** is not covered by these runners (`prepare` copies the i686 build directory's four
  files; an x86_64 build directory works the same way, not tried).
- **The routes of section 5** are the owner's to record.
- **A README row** for this doc (not in RT's files).

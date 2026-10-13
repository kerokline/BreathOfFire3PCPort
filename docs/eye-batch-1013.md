# The owner's eye batch of 2026-10-13: seven questions, captured once

**Status:** MEASURED (2026-10-13, away day, branch `away/1013-captures` off `d0d9162a`; live
runs in the harness root `C:\Users\kerok\abv`, built that morning by `abc_run.py prepare`;
not seen by the owner). The pictures are in the main checkout's
`analysis/shots/eye-1013/<question>/` (game-derived, never in the tree), each folder with a
`README.md`: the question, the installs and frames, what the record says, what to decide.
This doc is the index and what the frames already say; **the owner's eye decides**.

The visual checks the owner must make were scattered over owner-review.md, HANDOFF and four
round docs. This batch put each in front of the same harness once, so they can be looked at in
one sitting. Every sheet is the game's own pixels (the render target sampled back to 320 x 240,
or 426 x 240 wide) at 2x, nearest neighbour, labelled per row with the frame and scene.

## 1. How it was made

- **Build:** this branch (= `d0d9162a` plus the three tools below), `cmake --preset i686 &&
  cmake --build build`. Headless self-test from the harness's launcher copy
  (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*,-field_c1'`, the PC install copy with the `en-150`
  cache): exit 0, 1,072 lines at `0 MISMATCHES`, `inject: 10088 ours`, `self-test only: done`
  (`'*'` alone stops at `field_c1` under a Latin overlay, the known trap); then the exe host's
  self-test once (exit 0) before any recording.
- **Installs** (`abc_run.py prepare --only pc-en150,psx-eu,psp-eu,pc-enus,pc-ja,psx-us`, 2,598 s,
  every step exit 0): the eu triple of `recipes/abc-installs.toml`, the US pair, and two scratch
  entries (`_scripts/installs_extra.toml` in the eye folder): `pc-ja` (PC + the Japanese disc,
  `ja-JP`) and `*-wide` copies of the eu triple with `BOF3X_WIDE=1`.
- **Runs:** `abc_run.py run` per route (every run `BOF3X_OPT=none`, `BOF3X_LAYERING=0`,
  `--no-front`, a timed frame every 30 recipe frames unless said): 57 game runs (route x
  install) in 28 run folders, every one `done` at its last recipe frame, 8,555 s of game wall
  time. One run hit the stale `recipe done` trap
  (`eu2_gameover`, done after 1 s with no frames) and was re-run alone.
- **Shots** were added only with `tools/recipe_shots.py` in scratch copies of recipes
  (`shop_inn.txt`: a shot every 10 frames 2700..3300; `shop_inn_long.txt` the same plus a
  `wait 3600`); no recipe in the tree changed.
- **New tools** (committed): `tools/eye_sheet.py` (the sheets; `wide` sets a wide capture's
  middle against the narrow one), `tools/launcher_host/capture_page.py` (the launcher page and
  Jukebox by `PrintWindow`), `tools/wide_seams.py` (finds a fill left at 320 columns in wide
  captures). The small measuring scripts the READMEs cite are in the eye folder's `_scripts/`.

## 2. The questions

| # | Folder | Captured | What the frames say (measured) |
|---|---|---|---|
| 1 | `q1-d256-psp-title` | the title, PSP-EU / PSX-EU / PC, frames 300..600; the PSP logo at 4x | The PSP's logo is drawn in two pieces that do not join (a seam at about game x 218, the F of FIRE cut), its right wing reaches x 314, and a second, cut `TM` sits under the first. 35,479..37,698 pixels PSP / PSX from 420. The scroll before it is identical (0 pixels). |
| 2 | `q2-psp-standins-logo-launcher` | the town (area tiles), battle lists (UI page), dragonTransform (Ryu's forms), the logo movie (three caches' `CAPCOM.AVI`, every 0.5 s), the launcher page and Jukebox on four caches | No tile holes in the town; an interior corner strip where the PSX has black. `L` / `R` for the PSX's `L1` / `R1` at the battle scroll bars. Ryu's forms **identical** PSP / PSX in all sampled frames. The PSP logo movie's wordmark is about a third taller (421 x 80 against 428 x 60 at 640 x 480). The launcher: two clipped lines (the Snap note, every cache; the Data reason, the owner's cache). |
| 3 | `q3-wide-mode` | attract (PC, PSP-EU), combat, gameover, shop, cutsceneAndNue, bossAndFlash, whelpBoss wide and narrow | Message boxes move to the edges as designed; no fill left at 320 columns in 2,044 wide frames. Scenes 4..7, 9, 10 of psp-widescreen-a.md 6 are on no recipe; the flashes (1..3) were not caught. **Found:** wide changes the `Rand` sequence (whelpBoss parts from recipe frame 285, its fight goes another way); area 23's sunset sky stays blue in wide; battle flash phases differ (section 3). |
| 4 | `q4-menu-boxes-and-psp-config` | field_menu wide / narrow on three installs; config_screen on the eu triple and the US pair | The time and money boxes: unchanged in wide (0 pixels in the middle, every install). PSP-EU Config: labels and values blank (D246). **Found:** on PSX-only installs the Config values are blank too (section 3). |
| 5 | `q5-fishing-ticker` | caughFish on the eu triple, the ticker band every 30 frames 1050..1800 | The disc ticker runs one message at a time (`Wind reel...` then an empty band, then `Move Rod...Directional button`), the PC's packs them; the disc's circle glyph against the PC's red ring; PSP-EU says `buttons`. **Found:** the fishing equip screen's `EQUIP`, `ROD`, `GUIDE` headings blank on both disc-only installs. |
| 6 | `q6-older-checks` | world map (compass, sky bands) narrow and wide on three installs; gameover; the town wide; config_screen under en-US / ja-JP | The needle draws on all three. DIV-0065's labels beside their values (0 pixels between installs). The world map, field menu, town and game over: wide middle = narrow. Under ja-JP the Config grid is the PC's Chinese with a Japanese help line. |
| 7 | `q7-d255-inn-black-psx-only` | the inn (shots every 10 frames through the black), the same plus 3,600 frames of no input, the camp | **PSX-EU never comes back from the black**: 106 of 106 frames black from 2670 to 6720 (`OYASUMI.DAT` `open ... FAILED`); the camp the same from 2610 to the route's end. PC and PSP-EU return after the jingle. Not fixed. |

## 3. Found on the way (for the coordinator to number; nothing numbered here)

1. **D255 is a hang, not a longer wait** (Q7). On the PSX-EU-only install the inn's and the
   camp's black screen never ends while music plays: 4,050 frames, the last 3,600 with no
   input, `Frame_Counter` still counting, no `CRASH`. By the code (read, not measured):
   `Sound_LoadStream` `0x587910` returns at the failed open with the stream kind stored as 0,
   and the wait needs `Sound_StreamDone` `0x587A00` = `Music_IsPlaying() ^ 1`, so it waits for
   the area's song, which the failed load never stopped. Twenty more source files call
   `Sound_StreamDone`; any wait on a jingle a PSX-only install lacks would hang the same way.
   This sharpens the owner's open call: on a PSX-only install one of its two answers is needed
   for the inn and the camp to be leavable. (Also: on every install the inn's black lasts
   until the jingle has played in real time, so a shot freezing the game shortens it in frames.)
2. **The Config screen's option values are blank on every disc-built image** (Q4). PSX-EU and
   PSX-US draw the labels but no `Fast Med. Slow` / `1 2 3 4` / `Stereo Mono` / `Off On`
   (1,169 pixels at `163,74-267,142` against the PC, every row). The row tables beside the
   records, `0x653808` (row to first record) and `0x653810` (row to count) - `00 03 07 0b 0d
   0f` / `03 04 04 02 02` in `BOF3.exe` and the PC cache's image - are zero in `c_psxeu`,
   `c_psxus` and `c_pspeu` (`_scripts/cfgbytes.py`). `recipes/exe-gaps.tsv` classes
   `0x6536F8..0x653830` (`ConfigScreen_OpenStates+0x24`) `language` for every disc build, but the
   language layer writes only the 17 records (`0x6536F8..0x653808`, `config_text.cpp`), not the
   two tables. A gap of the disc-built image (DIV-0101's class); config_text.cpp says the tables
   are byte-identical to the PlayStation's, so a twin fill from the disc is the likely shape.
3. **The wide picture changes the random sequence** (Q3). `randlog` narrow against wide: equal
   for combat, gameover, shop, cutsceneAndNue, bossAndFlash; parted for the attract at recipe
   frame 4437 and whelpBoss at 285 (wide calls `Rand` 8 more times a frame). The visible cause
   is area 2's steam: a vent outside the narrow view, inside the wide band, is drawn and makes
   puffs. In whelpBoss the fight then differs (`Operator defends` against `Whelp Breath`) and
   the route parts at about 1080. DIV-0041 is written as the picture only; this is the owner's
   call (allow it, or keep the band's emitters off `Rand`).
4. **Area 23's sunset sky does not change colour in the wide picture** (Q3). Narrow at game
   (160, 10): `(61, 86, 244)` at 390, `(255, 86, 0)` at 660; wide: `(0, 191, 239)` and
   `(22, 191, 239)`, across the whole width, rows 0..183. Not traced to its function.
5. **Battle sprites' flash phases differ in wide** (Q3, Q6): hit-flash silhouettes and a boss's
   tint at the same tick, once the command cross (gameover `t000810`). The narrow run repeats
   exactly; two wide runs differ from each other at 1830..1890 only.
6. **The fishing equip screen's headings are blank on disc-only installs** (Q5): `EQUIP`, `ROD`,
   `GUIDE` drawn on the PC, blank on PSX-EU and PSP-EU (718 pixels, `58,72-241,148`). Not looked
   into.
7. **The launcher page clips two lines** (Q2): the note under `Snap the picture...` ends at
   "stretches to the" on every cache; the Data suite's reason runs out of its box with the
   owner's `pc-plus-us` cache.
8. **Under ja-JP the Config grid is the PC's Chinese** (Q6), the help line Japanese.

None of these is fixed here, and no ledger entry is owed by this branch: the tools are
instrumentation and change no game behaviour.

## 4. What this batch could not see

- Sound: the jingle's length, the logo's audio, the ticker's sound are not captured.
- Anything between two timed frames (30 recipe frames apart, 15 on the title and world map):
  the spell and death flashes of Q3 items 1..3 were on the routes but fell between frames.
  A window of close shots (`recipe_shots.py --from --to --every 2`) round a known cast is the
  way to catch one.
- The scenes no recipe reaches (Q3 items 4..7, 9, 10: areas 80, 189, 104 / 121, 172, the
  COMMU02 reveal, SCE15EF; the SCENA17 ending), the Beyd-Zig duel and Myria (owner-review's
  PSP play items), and the PSP's dial page beyond the one world-map scene.
- `campingFishing` on PSX-EU never reaches the fishing spot (finding 1), so Q5 rests on
  caughFish there.

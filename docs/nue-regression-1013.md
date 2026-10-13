# The cutsceneAndNue state-hash signal of 2026-10-13: not a regression

**Status:** MEASURED (2026-10-13, the owner away; agent nue, branch
`away/1013-nue` off `platform/more-hardening` `7fbf2e66`, built in the
worktree; the hosted log says `inject: 10087 ours`). Every run below is under the
session scratchpad `.../scratchpad/nue/` (`runs/`, the state-hash files in
`sh/`, the scripts `run1.sh`, `chain1.sh`..`chain4.sh`, `chk.sh`,
`hashed_diff.py`); the re-recorded pair is in `analysis/statehash/`
(game-derived, never in the tree - rule 1).

**Verdict.** Nothing merged since 2026-10-10 moved Capcom's state on this
route. The signal is two causes, both now measured to the byte: page
`0x905000` is **the save list's staging**, which went stale under the
reference pair when save slot 6 was rewritten at 07:10 this morning (the
2026-10-10 trap, H1; Capcom's own code differs from the old pair the same
way); the music pages are **DIV-0092**, the ledgered area-music fallback
(H2), which fires twice on this route - at the load (the recipe save holds no
track) and after the Nue event battle. With the pair re-recorded and DIV-0092
switched off, ours at the integration tip is **identical to Capcom's on every
one of 7,361 compared ticks** (`h2E`). No residue; no bisect was needed.

## 1. The signal

[`d56-encounter-bounds.md`](d56-encounter-bounds.md) 3 and 6: a build off
`d0d9162a` checked against `cn_cutsceneAndNue_orig2` / `_orig2b` (recorded
2026-10-10 17:30 / 17:35) differed on `0x905000` from tick 136 to the end
(7,225 ticks) and on `0x904000` / `0x665000` / `0x6BD000` (`Music_Track`,
`Music_LoadedTrack`, `Music_File*`) over ticks 204..1,032, in both hosts,
D56's flag on or off, where the 2026-10-10 run `cn_cutsceneAndNue_cu1010x`
had been identical. Reproduced here first: `statehash.py check orig2 orig2b
d56/sh/cn_cutsceneAndNue_on_b.sh` gives exactly those pages, and also the
sound page `0x7DE000` over 3..2,886 (2,534 ticks).

## 2. The conditions of every run

`tools/input_run.py tools/recipes/cutsceneAndNue.txt --no-front --game
<main>/bof3 --launcher <scratch>/nue/L/bof3x-launcher.exe --slot0-wait 2400
--minutes 40 --env BOF3X_STATEHASH=<file>`, with `BOF3X_OPT=none
BOF3X_LAYERING=0 BOF3X_STATEHASH_SKIP=tools/statehash_skip.txt` (173 ranges)
exported; `L/` an ini-less copy of the worktree's `bof3x-launcher.exe`,
`bof3x.dll`, `bof3x.exe`; no language overlay (the `cn_` pairs' setting).
`--original` as named per run; `input_run.py` picks the hosted `BOF3.exe`
with `--original` and the exe host (`bof3x.exe`) without, unless `--host` is
given. The machine was not quiet: D56's `balioAndSunder_2_on_b` game (pid
35388, from `d56/L2`, started 08:47; its runner logged `end: game exited` but
the game ran on, still appending to its state-hash file, 34 MB by 11:25) ran
throughout, and other agents' games from about 11:40.

| Run | Side | Host | Ticks | When |
|---|---|---|--:|---|
| `h1orig` | `--original "*,-Game_Clock"` | hosted | 7,361 | 10:48 |
| `rrA`, `rrB` | `--original "*,-Game_Clock"` (the new pair) | hosted | 7,425 each | 10:54, 10:58 |
| `rrC` | the same, dumps at 135, 136, 4300 | hosted | 7,361 | 11:13 |
| `h2`, `h2D`, `h2E` | `--original AreaTrackFallback` (DIV-0092 off); D dumps 204, 205, 7400; E dumps 1100, 2000, 5000 | hosted | 7,425 / 7,425 / 7,361 | 11:03, 11:23, 11:33 |
| `tip`, `tipD`, `tipE` | ours as built; D, E dumped as above (D with `BOF3X_MUSIC_LOG=1`) | exe | 7,425 / 7,425 / 7,361 | 11:07, 11:18, 11:29 |
| `tipH` | ours as built | hosted | 7,361 | 11:39 |
| `h2X` | DIV-0092 off | exe | - | 11:44: `FATAL: BOF3X_ORIGINAL is set ... bof3x.exe does not map` (as designed; the switch is hosted-only) |

The tick counts differ only by where the runner's kill fell against the
64-tick flush; every run reached `recipe done at recipe frame 7421`.

## 3. H1: the save list's staging - confirmed

**Capcom's own side against the old pair** (`check orig2 orig2b h1orig`):
`0x905000` from tick 136 to 7,360, 7,225 ticks - the same page, first tick
and count as ours. So the pair, not the build, had moved.

**The bytes.** `statehash.py bytes rrC.sh.135.bin rrC.sh.136.bin`: at tick
136 the game fills thirteen records of 0x1C bytes from `0x905BC0`, one per
save slot (`BISLPS00..0C`): the slot's index at +0x29 (`00`..`0c`), the
leader's name (`52 79 75` "Ryu") at +0, level and place bytes beside it.
`0x905BC6` is the field [`state-hash.md`](state-hash.md) 5 already named as
"slot 0's save summary" (`Save_ReadSummaries`); `symbols.toml` labels the
region `Draw_SortOnX+0x18..`, which is a neighbour's name, not this table's.
Against the 2026-10-05 drill dump of Capcom's side
(`analysis/statehash/drill_cutsceneAndNue_orig.sh.4300.bin`) today's tick
4300 differs on `0x905000` in exactly the records of the slots rewritten
since: slot 6 (`0x905C68`, "Nina" then, "Ryu" now), slot 8 (`0x905CC2`) and
the new slots 9..C (`0x905CD8..0x905D25`), and on `0x92A030..0x92A075`
(`Save_Directory`, the new slots' file names `BISLPS0A`..). Since the
2026-10-10 pair only `BISLPS06.DAT` changed (modified 2026-10-13 07:10:28;
slots 1..5 and 7..C are older than 10-10 17:30, and slot 0 holds the recipe
save on Capcom's side throughout), so slot 6's record is the difference.

**It is not this route's alone.** D56's own `combat` runs straddle the
rewrite: `cn_combat_on_a` (finished 06:59) checks clean against
`cn_combat_orig2`, `cn_combat_on_b` (07:46) differs on `0x905000` from tick
226, its save-list read, over 2,335 ticks. Every `cn_<route>_orig2` pair
whose route opens the save list is stale the same way (section 6).

**Re-recorded** (state-hash.md 6's procedure): `rrA` and `rrB` installed as
`analysis/statehash/cn_cutsceneAndNue_orig2` / `_orig2b` (`.sh`, `.log`,
`.runlog`), the 2026-10-10 pair kept as `cn_cutsceneAndNue_orig2_pre1013` /
`_orig2b_pre1013`. The new pair is hashed under the current list (173
ranges, `BOF3SH3`, flags 3), so runs under that list are compared on every
page, the three the old pair's 168-range list left out included. The two
agree but on six pages at isolated ticks (`diff rrA rrB`: the sound page
`0x7DE000` on 23 ticks, `0x7C9000` `D3d_Scale*` / `Font_TexCache` on 8,
`0x7CA000` on 1, VRAM shadow `0x7BB000` / `0x7C7000` and `0x937000` on 5
over 2,625..3,137 - the loaded machine's noise class of state-hash.md 4),
which `check` takes out tick by tick.

## 4. H2: DIV-0092's area-music fallback - confirmed

**Ours with the divergence off against the new pair** (`check orig2 orig2b
h2E`): **identical on all 7,361 compared ticks.** The repeats: `h2` -
seven pages at tick 3 only (the VRAM shadow `0x7BB000..0x7BF000`, the sound
page, `0x937000`: the start-up class, which Capcom's own `rrC` and `h1orig`
show at ticks 3..4 against the same pair); `h2D` - three pages at ticks 2,977
and 5,578 only (load noise).

**Ours as built** (`check orig2 orig2b tip`; `tipD`, `tipE` and the hosted
`tipH` the same): exactly four pages -

| Page | Ticks | Count |
|---|---|--:|
| `0x904000` (`Music_Track`) | 204..1,032 and 7,395..end | 859 |
| `0x665000` (`Music_LoadedTrack`) | 205..1,032 and 7,395..end | 858 |
| `0x6BD000` (`Music_File*`, `Music_FadeStops`) | 205..1,032 and 7,395..end | 858 |
| `0x7DE000` (`Snd_BufferDesc`, `Snd_WaveFormat`, `Music_Loops`) | 205..2,736 and 7,395..end | 2,555 |

(The old pair stopped at tick 7,360, so D56 saw only the first span.) `diff
h2 tip` from 2,967 to 7,394: identical on all 4,428 ticks.

**The bytes** (`hashed_diff.py`, the skip list applied to the raw dumps, off
= `h2D`/`h2E`, on = `tipD`/`tipE`):

- **Tick 204** - `0x904CD0`, the area's track byte: off `ff`, on `1b`.
  `GameMode_Start` (`0x495840`, `src/game/mode_flow.cpp` 349) copies the save
  block's track over the one `Field_ChangeArea` chose; `nue.DAT` holds `0xFF`
  (it was saved inside a scripted silence, DIVERGENCE.md DIV-0092's
  second amendment), and DIV-0092 case 2 keeps the zone's track 27 instead.
- **Tick 205** - `Music_Track` `0x904131` `ff`/`1b`, `Music_LoadedTrack`
  `0x6653BC` `8d`/`1b`, `Music_FileSize` `0x6BDE48` and `Music_FileLoops`
  `0x6BDE4C`, `Music_FadeStops` `0x6BDE58` `01`/`00`, `Music_Loops`
  `0x7DE3DC` `00`/`01`. The music log of `tipD` says why:
  `f204 Music_Play(27, 8) track 255 loaded 141` - `Area_Enter` plays 27 on
  the load; `h2D`'s first music after the title is the script's
  `f1032 Music_Play(27, 8)`, where `tipD`'s same call finds 27 already
  loaded. From 1,033 the two match again.
- **Ticks 1,100 and 2,000** - only `0x7DE37C..0x7DE3AC`, `Snd_BufferDesc`
  and `Snd_WaveFormat`, the last DirectSound buffer's description: off
  44,100 Hz stereo (`02`, `44ac`, `10b102`, `04`: the stream created at
  1,032), on 22,050 Hz mono (a sound effect's buffer: ours' 1,032 call
  re-played a loaded track and created none). The next stream start on both
  sides, `f2736 Music_Start(..., 107416, 0)`, writes the same description;
  tick 5,000 is identical.
- **Tick 7,400** - `Music_Track` `ff`/`1b`, `0x904CD0` `1d`/`1b`,
  `Music_LoadedTrack` `a5`/`1b`, `Music_File*`, `Music_FadeStops`, the buffer
  description again. `tipD`'s log: `DIV-0092    event battle 2 over: the
  zone's track 27 from its top (the current byte held 29), no resume`, then
  `f7394 Music_Play(27, 10)`; `h2D`'s last music is `f7374 Music_Stop`, the
  field silent to the end - the defect DIV-0092 repairs.

So every byte of the music pages is DIV-0092 doing what its entry says, at
the two places its entry names, and nothing else.

**The hosts.** `tip` (exe) against `tipH` (hosted): identical but two ticks
(1,573: `0x7CA000` and the sound page; 1,696: the sound page), noise that
`tip` alone has against every other run. The exe host refuses
`BOF3X_ORIGINAL` loudly (`h2X`), so on it the divergence cannot be switched
off; there the check is the hosted build of the same tree, as
[`state3-build.md`](state3-build.md) 5.1 already does across the cutover.

**`Rand`.** The logs' `randlog` lines of `rrA` (Capcom), `tip` and `h2` are
identical on all 7,422 frames (frame 3,661: 112 on every side; frame 7,420:
204). D271 (the all-Capcom side of `nueRandomEncounter.txt` one draw ahead at
frame 3,661, under `'*,-Game_Clock,-Game_WndProc'`) does not reproduce on
this route under `'*,-Game_Clock'`, so it is not this signal's cause; it
stays open as its own entry.

## 5. What was added to the method

No skip range: DIV-0092's bytes are game state (the track bytes, the loaded
file), and a range over them would blind every route to the music.
[`state-hash.md`](state-hash.md) 6 already says how a ledgered divergence
that changes state is checked - with the divergence switched off - and now
carries a "known and not skipped" row for DIV-0092 with the pages, the ticks
and this route's command, beside DIV-0073's, plus the new pair in its
reference list.

## 6. What is left, and for whom

1. **The other nine `cn_<route>_orig2` pairs** are stale on `0x905000` (and
   possibly `0x92A000`) from each route's first save-list read, measured on
   `combat` (section 3). Re-recording them (two Capcom-side runs each,
   about 45 minutes on a quiet machine) is the coordinator's call: any agent
   whose recipe writes a slot today (slot 6 is `balioAndSunder_2`'s, by
   design) stales them again. A durable answer - the runner staging a fixed
   save list rather than the owner's - is a design change to
   `input_run.py`, not made here.
2. **Other routes DIV-0092 touches.** Only cutsceneAndNue's pair is known to
   meet it. D56's logs show `bossAndFlash` ending an event battle (`event
   battle 8 over: the zone's track 33`), a route without a `cn_` pair; a
   random-encounter route that returns to the field would meet case 1 as
   well. Check with `--original AreaTrackFallback` when one reports music
   pages.
3. **The orphaned game** pid 35388 (D56's `balioAndSunder_2_on_b`, launcher
   `d56/L2`) was still running at 11:45, not killed here (not this agent's
   run).

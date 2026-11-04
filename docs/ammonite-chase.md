# The Ammonite chase: was it slowed down on the PSP?

**Status:** MEASURED (2026-11-03). The question is the owner's: a persistent
player rumour says the chase before the Ammonite fight (area 134, chapter 12)
was made slower, or easier, in the PSP release. **The answer from the code is
no.** The PSP runs the same rules, the same constants and the same random
tables as the PlayStation discs and the PC, word for word where a compiler
keeps them. Section 4 lists what this reading does not cover.

## 1. The save and the route

`tools/recipes/ammoniteBoss.txt` (`# save ammoniteBoss`, the owner's save of
2026-11-03 23:25, recorded with `BOF3X_RECORD`, en-US). Replayed with the
scenario counters, the step byte and the area watched (root
`D:\bof3x-runs\amm1103`, recipe `amm_w.txt`, the closing F9 presses left out):

| Recipe frame | What the watch shows |
|---|---|
| 270 / 351 | the area loads (0x89, then 0x86) |
| 640..2221 | chapter 12 run 9, steps 0xA..0x11: the scene before the chase |
| 2222 | step 0x12: effect kinds `0x82`, `0x83` and `0x84` placed; `0x903849` (the push count) 3 |
| 2223..2239 | `0x90384A` up one each frame, 0 to 0x10, with the button held |
| 2240 | `0x90384A` 0x80: the party is caught |
| 2267 / 2325 | step 0x1E, then 0x1F and 0x20: the fight starts from the caught side |
| 7548 | the fight is over, the scene goes on (steps 0x21..0x2D) |

In this route the button was held from the first frame of the chase, and the
party was caught 18 frames in.

## 2. The rules, as the code has them

The PC's code is `scena_sc12.cpp` (run 9, `0x561170`, step 0x12) and
`effect_3d.cpp` / `effect_4a.cpp` (kinds `0x82`..`0x84`); the comments there
cite each original address. Read together:

- Two objects (Sprite_Objects records 0 and 1) push toward the party. Each push
  is a count of steps (`0x903849`, 3, 4 or 5) of one cell each (four adds of
  0x4000 to x), with nudges in z of 2, 4 and 2 adds and two eight-frame holds.
- Kind `0x84` picks each push count from a 16-byte table by `Rand & 0xF`
  (`EffectKind84_PushCounts`, `0x654C68`), and each pause after a push from a
  second one (`EffectKind84_Waits`, `0x654C78`: 30, 60 or 40 frames).
- The scene's step 0x12 moves the party (`Party_DropIn(7)`,
  `Kind2_Place(3)`) on any frame that `Input_Held` is one of the three words
  0x3000, 0x6000 or 0x2000.
- While a push is under way (`0x903849` not 0), every frame one of those words
  is held adds one to `0x90384A`. When it was already 0x10, the count becomes
  0x80 and the party is caught. The scene's step 0x12 sets the count back to
  0 on every frame none of the three words is held (`SetCounter(2, 0)`), so
  the limit is on one unbroken hold, not on the chase as a whole (section 5
  measured it).
- The party wins when the third member's x reaches 0x340000 (0x34 cells): step
  0x14, and the fight starts from there.

So a hold of 17 frames or more while the pushers move is caught; a hold that
is let go sooner starts again from 0. Tapping the button gets past this.

### 2.1 The cycle, measured

The positions watched every frame (pushers' x `0x7DEEB4` / `0x7DEF58`, the
leader's `0x802D74`, the third member's `0x80300C`, in cells) on four
variants of the route (`d_hold`, `d_mash1`, `d_mash3`, `d_obey` in the run
root) give the cycle:

1. The chase waits for the first press of right. That press starts the first
   attack.
2. **The sweep.** Both pushers move forward together at half a cell a frame
   (the party moves an eighth of a cell a frame), until one is level with the
   leader (`Check`: its x at or past the leader's).
3. **The strike.** At the leader, the pusher swings in z (the 2 / 4 / 2
   nudges with two eight-frame holds), about 22 frames, staying where it is.
4. **The pause.** The attack count goes to 0 and kind `0x84` waits 30, 40 or
   60 frames. The hold counter does not count here: hold freely.
5. When the pause begins, a pusher still at or past the leader's x catches the
   party (`EffectKind82_Wait` to state 0x16, `EffectKind82_End`: `0x903849`
   0xFE, step 0x1E). Otherwise the next attack starts at 1.

The party is caught in two ways: a pusher still level with the leader when the
pause begins, or a hold of 17 frames without a break during a sweep or strike.
To escape, the party must move *during* the strike, to get ahead of the pusher
before the pause, but in short taps. Standing still does not work: `d_obey`
pressed right for 10 frames to start the chase, then nothing. The pushers
swept from 27 to the leader's 32 and struck. At 2263, when the pause began,
they were still level with it: caught, step 0x1E.

| Variant | 1st attack: pushers / leader at the pause | Result |
|---|---|---|
| held (the owner's) | - | caught by the hold count at 2240, mid-sweep |
| `d_obey` (still after the start) | 32 / 32 | caught at 2263 |
| `d_mash3` (3 on, 3 off) | 33 / 35.1 | safe; 2nd attack 44 / 45.9 safe; the line at 2381, in a pause |
| `d_mash1` (1 on, 1 off) | 33 / 35.1 | safe; the line at 2363, in the 2nd strike |

## 3. The comparison

The two tables first, as bytes, searched for in every held disc image
(`D:\bof3x-runs\discs`, files located by their ISO 9660 directories):

| Build | Where the 32 bytes are | Equal to the PC's |
|---|---|---|
| PC `BOF3.exe` | `.data` `0x654C68` | - |
| PSX JP | `BIN/WORLD03/AREA134.EMI`, section 13 (`0x801F2C00`), `0x801F5724` | yes |
| PSX US | the same file and offset | yes |
| PSP JP | `PSP_GAME/SYSDIR/BOOT.BIN`, `0x2E1160`; also in `USRDIR/JPN/WORLD03/AREA134.EMI` | yes |
| PSP EU | `BOOT.BIN`, file offset `0x2E09F8`; also in `USRDIR/USA/WORLD03/AREA134.EMI` | yes |

Then the code, disassembled with capstone:

- **PSX JP against PSX US.** Section 13 of `AREA134.EMI` differs in 251 words,
  and every one is an address half moved by 4 (the US build's globals sit four
  bytes later). Section 12 is identical. The chase code is the same.
- **PSX against the PC.** Kind `0x84`'s states at `0x801F3E8C` (the pick) and
  `0x801F3F78` (the count) test the same line (`0x33FFFF < x`), the same three
  held words, the same `< 0x10` before 0x80, and index the same tables.
- **PSP JP against PSX.** The PSP's `BOOT.BIN` is its own native compile
  (`psp-code-diff.md`), so the words differ, but the constants a compiler keeps
  are equal. Kind `0x84` at `0x4EE24` / `0x4EEF0` (`lui 0x34; slt` for the
  line, `slti 0x10`, 0x3000 / 0x6000 / 0x2000, the tables at `0x2E1160` /
  `0x2E1170`). The pushers at `0x4E560..0x4ED38`: the same 0x4000 steps, four
  a cell, the same 2 / 4 / 2 nudges, the same 8-frame holds and the count to
  0xF. The scene's step 0x12 at `0x2A8934`: the same held words, then calls
  with 7 and 3 (`Party_DropIn`, `Kind2_Place`), 0x80 to step 0x13, and the
  spawns of `0x83` and `0x84` just before it.
- **PSP EU against PSP JP.** The same 744-word block (`0x4E344` in the EU
  ELF): 186 words differ, all of them address halves and call targets. No
  `ori`/`slti`/`sltiu` constant and no 0x4000 step differs.
- The PSP's `USRDIR/JPN/WORLD03/AREA134.EMI` section 13 is the PSX JP's byte
  for byte, and the USA folder's is the same.

The US-disc decompilation the project reads for PlayStation twins does not
cover `WORLD03` or the chapter 12 scenario, so this comparison was made from
the disc code directly.

## 4. What this does not cover

- **Frame rate.** The rules are counted in frames. If the PSP ran the field
  slower in real time, the chase would feel slower with the same code. The
  PSP's frame pacing was not measured here.
- **Rand.** Each build has its own `Rand` (PSX `0x8017ED4C`, PSP `0xA2CF0`).
  The tables are equal, but the sequence of draws may not be.
- **The party's step.** `Party_DropIn(7)` / `Kind2_Place(3)` move the party
  by a move script and the engine's speed table. Section 12 of the area file is
  equal on the PSX and PSP; the engine's speed table was not compared.

If the rumour has a cause, it is one of these three, not the chase's own
rules.

## 5. Tapping instead of holding (measured)

Player advice online says tapping right works better than holding it. The
owner's route held right for 84 frames from recipe frame 2214. Three variants
(`mash1.txt`, `mash2.txt`, `mash3.txt` in the run root) keep the route up to
that frame, then tap right for 400 frames: 1 frame on and 1 off, 2 and 2,
3 and 3 (30, 15 and 10 presses a second). The third member's x
(`0x80300C`, ObjTrio 2's +0x34) and the counters were watched every frame.

| Taps | Highest held count | Line reached (step 0x14) | Then | The fight starts from |
|---|---|---|---|---|
| held (the owner's) | 0x10, then 0x80 at 2240 | no | step 0x1E at 2267 | the caught branch |
| 1 on, 1 off | 1 | 2363 (x 0x348000) | step 0x1E at 2365 | the caught branch |
| 2 on, 2 off | 2 | 2363 | step 0x15, then 0x1E at 2372 | the caught branch |
| 3 on, 3 off | 3 | 2381 | steps 0x15, 0x16 at 2608 (`Place(0x348000, 0x258000)`) | the escape branch: the camera goes on past the line before the fight |

With taps the count never gets near 0x10: it goes back to 0 on each release.
All three tapping variants reach the line, where the held route never does.
At 30 and 15 taps a second, the scene still goes to step 0x1E just after the
line, which `EffectKind83_Finish` (the pushers' state 23) sets when a pusher
reaches the leader. Only the 10-a-second variant ends on the escape branch.
Tap speed is not the cause (section 2.1 measured it). The 30-a-second run
crosses the line at 2363 while a pusher is still striking (`0x903849` 3, the
pusher at 46, the leader at 48.4). `PastLine` sets `0x903849` 0xFF and step
0x14. Then the striking pusher's `Again` sees 0xFF, goes to state 0x17, and
`EffectKind83_Finish` writes step 0x1E over the win, two frames later. The
10-a-second run happens to cross during a pause, when no pusher is striking.
So the line must be crossed between attacks, not during one.

What the two branches change in the fight is not read here.

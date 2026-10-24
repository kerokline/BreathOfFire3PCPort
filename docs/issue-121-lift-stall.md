# Issue 121: the lift scene stalls on instant text (ScrubOasis's report)

**Status:** FIXED (2026-10-24; reproduced on the owner's `liftScene` recording with a one-frame mash and with
random stick taps, the cause read off per-frame peeks, closed by DIV-0158's amendment - the pacing floor - and
recorded as D357; the fix built and replayed, section 5)

[Issue #121](https://github.com/kerokline/BreathOfFire3PCPort/issues/121): "Frozen at the mountain lift
cutscene. Gave it a couple tries. Couldn't get past it." The reporter's screenshot is at the lift's start
platform. Later comments: the freeze happens on the 10/22 build and on main `5a4637f2` (built 2026-10-24
14:00 UTC); **it goes away when Msg Speed is set to Medium**, with the launcher's instant text on
(`text_speed=1`, DIV-0158). His `bof3x.ini` is attached to the issue (`bof3x.txt`); his `sources.toml`: the
US PSX disc and the PSP-EU disc, no PC install.

## 1. The scene

Area 99 (やまごや in the sibling's `names/places.toml`), story chapter 3, **run 7** (`Scena03_Scene7`
`0x544040`, `src/game/scena_sc3.cpp`). Its steps wait on the script counter `C0` (`0x903848`) with exact
tests: step 4 `C0 == 7`, 5 `0x12`, 6 `0x16`, 7 `0x18`, 8 `0x27`, 0xB `0x2F`, 0xD `0x34`; step 0xD ends the
run. The area's movement scripts advance `C0` between messages: a dialogue object's script does `A2` (count
up) then `E3 nn` (open message `nn` through `MoveScript_SetTurnTarget`, `Field_Request` 2, `Game_Mode` 4 while
the box is up), and waits on the others' counts with `A1 nn` (`MoveScript_CounterTest`: stop while unequal).
The lift itself is the **kind-2 object** (`Sprite_Kind2` `0x7E0940`, the camera focus, `docs/kind2-object.md`),
running the area's `+0x1C` script 0 (`0x618CB4`) through `Kind2_Script`; it waits on the same counts.

What instant text changes (DIV-0158, `MsgBox_StatePrint`): on Fast a page prints whole in one frame, stopping
only at a page end, break, choice, timed wait or pause; Medium prints at Fast's delays. On the original's Fast a
glyph is a frame: the speed table `0x658E94` is `00 00 | 01 02 | 01 00` by Msg Speed, held or not, and a 0
delay leaves the printer in state 0 for the next glyph next frame. The shout near the start (Nina's, the owner
confirms, `C0` 0x13) is the scene's one message-box effect, a **grow, kind 2** (code `0x0F`); it outlives a
box closed early but nothing waits on it - a distraction, not the cause.

## 2. The reproduction

All on the reporter's install (`rep`: US PSX + PSP-EU, `install --base`, en-US), instant text, Msg Speed Fast
(`poke 0x903A58 u8 0` after the load), DIV-0071's layering on and the default layers (nothing forced), x1,
the owner's `liftScene` save (recorded 2026-10-24 for this scene: `tools/recipe_saves/liftScene.DAT` +
`tools/recipes/liftScene.txt`, loaded with `--save liftScene`). Root `C:\Users\kerok\bof3x-runs\i121y`.

| Route | Presses | Result |
|---|---|---|
| `ls_fast` | the owner's recording | completes (`C0` to 0x29 where the recording ends) |
| `ls_med` | the same, Msg Speed Medium | completes, one frame off here and there |
| `ls_x2` | the same at x2 (`key f1` at the load) | completes |
| `ls_mash11` | circle one frame on, one off, from the load | **stalls at `C0` 0x12** |
| `ls_noise` | the owner's presses plus a random direction tap on a third of the frames | **stalls at `C0` 0x12** |

The stall frame (`ls_mash11/t003000`) is the reporter's screenshot: Ryu and Garr on the start platform, Nina
at the console, no box. The game is not hung - the field draws, the party stands - the scene is.

## 3. The cause, frame by frame

Per-frame peeks (`expand.py` writes them into the route; `trace.py` reads the log): `C0`, the run and step
(`0x8034E4` u16), the box (`0x7DEE40` u32: state, sub-state, effect kind, phase), its flags (`0x7DEE44`),
`Field_Request` (`0x66C7D8`), `Game_Mode` (`0x66C7E8`), the kind-2 object's run byte, state, script position
(`+0x8A`) and context (`+0x80`), the view hold `Field_Kind2Hold` (`0x929F12`), and the ten field objects' and
three party objects' contexts.

The lift's script, from `0x618CB4`: `F4 06 00 01`, `E8 01`, **`A1 08`**, `FA 04 0A 06 00` (a ten-step timed
move, which `Kind2_Script` turns into state 1 - `Kind2_Move` - until the view's scroll lets go of
`Field_Kind2Hold`, `AreaMap_Frame`), `E8 02`, **`A1 0B`**, `FA 05 0A 06 00`, `E8 03`, `A1 10`, ..., **`A1 12`**,
`FA 00 0E 06 00`, `5F 5F`, **`A2`**, `A1 18`, ...

| | owner's presses (`ls_ctl_k2`) | the mash (`ls_mash_k2`) |
|---|---|---|
| `C0` becomes 8 | f1447 | f1016 |
| the lift's move starts (state 1, hold 1) | f1448 | f1017 |
| `C0` 9, 0xA, 0xB, 0xC | f1569, f1598, **f1650**, f1689 | f1060, f1089, **f1116**, f1155 |
| the hold clears, the lift reaches `A1 0B` | f1607 / f1608 (`C0` 0xA) | f1176 / f1177 (`C0` **0xC**) |
| the lift passes `A1 0B` | f1651 | never |

The move's hold lasts 160 frames in both runs. With the owner's presses `C0` reaches 0xB 202 frames after 8 and
the lift is already waiting; with the mash `C0` is 0xB from +100 to +139 and gone when the lift looks. From then
on nothing counts: the dialogue objects reach `A1 18` (script 11 `0x618744`, position 0x32) after the message at
`C0` 0x12, the scene's step 6 waits for 0x16, and the `A2` that would make 0x13 is the lift's, two ops past the
`A1 12` it never reaches. The three messages between 8 and 0xB are pages of 0x24 + 0x20, 0x20 and 0x29 glyphs:
on Fast they alone take 141 frames to print, so on the PlayStation and the unmodified port `C0` cannot leave 0xB
within 160 frames of reaching 8, whatever the player does. The instant page can.

Why the first traces missed it: every rhythm tried on the `genmelAndLift` route closed each page a few frames
after its print at the earliest (the 2-held / 2-released mash included), which with the window's open and close
kept `C0` at 0xB long enough; the one-frame mash and the stick taps close pages the frame they print.

## 4. The fix: the pacing floor (DIV-0158, amended)

An instant page closes no earlier than it could on Fast. `MsgBox_StatePrint` counts the glyphs an instant
page prints (`g_instant_page`, reset with the box) and, at the page's stop code - its end `0x00` outside a
substitution, a break `0x02`, a pause `0x0B`, a choice `0x14` or a timed wait `0x16` - leaves the code where it
is and goes to state 1 for that many frames first (at most 255); then state 0 takes the code as before. The
page is drawn whole from its first frame; the arrow, the choice and the close come when Fast's would. A stop
code inside a script's own `0x10` span takes no floor (the original's instant case). Medium is untouched.

Not changed: the script's exact waits. An `A1` of "at least" would touch every area's scripts for one scene's
race, and counters are reset (`A0`) between scenes.

## 5. Verification

- Without the floor: section 2's table.
- With it (`L5`, the build of this branch): the mash route (`fx_mash`) **completes the scene** - `C0` to 0x30,
  run 7's step 0xD at f4362 - and the trace shows the floor: a page of 0x1A glyphs printed at f1213 sits in state
  1 to f1239, then takes its break. The owner's recording (`fx_owner`) keeps advancing but falls behind the
  recording (`C0` 0x16 at its end instead of 0x29): a press that landed inside a page's floor is not a press
  edge the box sees, and a recording does not press again where a player would. **A route recorded on instant
  text before the floor does not replay 1:1 with it** (`genmelAndLift`, `liftScene`); routes recorded with
  `text_speed=0` are untouched.
- `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'` with `BOF3X_TEXT_SPEED` 0 and 1 (the developer launcher, `L6`, the PC
  install): both exit 0.

## 6. The instruments

Under `C:\Users\kerok\bof3x-runs\i121y` (Defender-excluded; harness roots are per session - make your own):
`installs.toml` (a copy of `i121x`'s, the `rep` install), `expand.py` (a recorded route to one line a frame with
the peeks above, a `poke` at a frame, `--from` to start the peeks late), `trace.py` (the log to one row a frame,
printed on change), the routes `ls_*.txt` and their runs, `run_c.sh` / `run_fix.sh` (one run: install copy,
launcher copy), `L5` the fixed build's launcher. The earlier instruments stay in `i121x` (the `genmelAndLift`
route with its hand-added F1, `lift_*.txt`).

## 7. Related, fixed alongside (PR #129)

- #118 (D355): the Skill Notes count's total read off our stack. Ours.
- #120 (D356, DIV-0177): the area's song lost after a jingle (the Flier's `ITEM01`). Capcom's port; restored.

## 8. The console flash (the second report; DIV-0178, D358)

The reporter's "chrysm explosion was upside down", and the owner's stills from a PlayStation let's play against
ours: there the five beams from the console rise to vertical, wide then narrow, always up; ours start flat, rise,
then fold over the horizon and point down before they narrow. The beams are effect kind 0x22's five kind-0x21 arms
(`docs/effect_1d.md`), 15 units long once grown. Every function on the chain matches its PlayStation twin
instruction for instruction (`EffectKind21_ArmPoints` and the five states in `SCENA03.EMI`, `RotMatrixY` / `Z`,
`csin` / `ccos`, the projection, the camera load; `rays_orig2` hosts Capcom's PC arm functions and draws the same
flip) - except `Gte_ApplyMatrixLV`: the port's multiplies in 32 bits, the PlayStation's (`0x80179390`) splits each
component into `v >> 15` and `v & 0x7FFF` and never wraps. The arm's point heights peeked each frame (`campeek`,
`project.py`) go +3163 at f2502 to -699 at f2508 of the owner's recording: `0x1000 x 0xF0000` crossing 2^31.
DIV-0178 makes the products 64-bit (switch `ApplyMatrixLVExact`, armed after the `psx_gte` fuzz). Capture
`rays_fix` (the save with a press every 40 frames, since the owner's recording no longer replays under the floor;
shots every 6 frames, 2700..2880): the beams rise from flat, fan wide, narrow upward into one vertical beam and
fold - the owner's stills' shape, always up.

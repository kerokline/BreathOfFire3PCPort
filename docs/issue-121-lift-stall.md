# Issue 121: the lift scene stalls on instant text (ScrubOasis's report)

**Status:** IN PROGRESS (2026-10-24; not reproduced here - every run below completes the scene)

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
run. The area's event script advances `C0` between messages. Sunder's shout near the start (`C0` 0x13) is the
scene's one message-box effect: a **grow, kind 2** (code `0x0F`; `MsgBox_EffectGrow` / `GrowStart` /
`GrowStep`, `src/game/msgbox.cpp`).

What instant text changes (DIV-0158, `MsgBox_StatePrint`): on Fast a page prints whole in one frame, stopping
only at a page end, break, choice, timed wait or pause; Medium prints at Fast's delays. Measured on the shout
(the per-frame peeks of section 3): the page completes at f14175 with the box waiting at state 2
and the grow starting; a press at f14177 closes the box while the grow is still in its first phase; the grow
runs on a closed box for 324 frames until the next message resets it. On Medium the grow plays while the text
prints. Nothing waits on the grow in any run here, so this is the one known Fast-only difference in the scene,
not a demonstrated cause.

## 2. What has been ruled out (each run completes run 7: run 0, `C0` back to 0)

| What | Runs | Root |
|---|---|---|
| Builds | today's `2f55de72`; the 10/22 evening build `9a4cda2e` (alpha.5 + DIV-0158, the reporter's "latest"); alpha.4 + DIV-0158 (branch `test/alpha4-fasttext`, local) | `i121x`, `i121b`, `i121c` |
| Installs | PC + US-disc layer; US PSX alone; PSP-EU alone; **the reporter's: US PSX + PSP-EU** (`rep` in `i121x/installs.toml`) | `i121x` |
| Msg Speed | Fast (the owner's recording was already Fast: `0x903A58` read 0 before the console) and Medium | `i121x/sp_*` |
| Press rhythm on Fast | the recording's own presses; circle every 20 frames; every 9 frames (3 held); a mash (2 held, 2 released) | `i121x/sp_trace`, `rh_*` |
| Settings | his ini against the owner's: only `display`, `satpixie.shadow_mask`, `ruby_ui`, `ffmpeg` differ - none reach game logic | - |

The per-frame traces show `C0` stepping by one every time - no skipped count (`rh_mash`, `rh_p9`,
`sp_trace`, `rh_rep`). A circle **held** from before the console never advances anything (no press edge), as
in the original - not the bug.

Caveats on the runs: the harness forces `BOF3X_LAYERING=0` and `BOF3X_OPT=none` (a player has DIV-0071's
layering on and the default layers), and every route here inherits the owner's **F1 x2** (below). The
reporter played at x1 at first and at x2 later, and froze both times, per his comments.

## 3. The instruments

All under `C:\Users\kerok\bof3x-runs\i121x` (Defender-excluded; harness roots are per session - make your own
with `tools/abc_run.py prepare --root ... --installs <copy of i121x/installs.toml>`):

- **The route** `tools/recipes/genmelAndLift.txt` (main checkout, untracked as of 2026-10-24; save
  `tools/recipe_saves/genmelAndLift.DAT`, area 69, chapter 3). Recorded at **x2**: the owner pressed F1 at
  `Frame_Counter` 316, which the recorder of that day did not keep, so the route carries a hand-added `key f1`
  at frame 316. Without it every replay parts at the Flier jingle (f5840): jingles play in wall time, so a
  scene waiting on one sees half the frames at x1. It needs the owner's ini settings: `BOF3X_TEXT_SPEED=1`,
  `BOF3X_STEAL=1` (`cheat.steal=1`). Area 99 loads at f9246; the lift scene is f12800..17220; area 71 at f17400.
- **The lift traces** `i121x/lift_*.txt`: the route to f13840 (run 7 step 5, after the console), then a
  `poke 0x903A58 u8 0/1` (Msg Speed), then a press rhythm written frame by frame with `peek` lines for `C0`
  (`0x903848`), the run and step (`0x8034E4` u16) and the box (`0x7DEE40` u32: state, sub-state, effect
  kind, effect phase). Parse the log's `peek ... recipe frame N` lines.
- **A field save before the scene does not work**: an F12 quicksave taken at the console loads without the
  scene's state (Nina alone on the platform; `i121x/saves/liftStart.DAT`, kept as the example). Use the route.
- `tools/abc_run.py` with `--installs i121x/installs.toml --set i121` (PC, PSX-US, PSP-EU) or `--only rep`.

## 4. What to do next

1. **Get the reporter's save** from just before the console (or before the lift's area) and whether he plays
   on keyboard or pad. Load it under `BOF3X_RECORD` on Fast and play the scene by hand - the recorder writes
   F-keys since `tools/recorder-fkeys` (local, uncommitted 2026-10-24) - then replay with the per-frame peeks.
2. **Lift the harness's forced settings**: rerun `lift_mash_fx.txt` on `rep` with `BOF3X_LAYERING` unset
   (DIV-0071 on) and the default layers, and at x1 (insert a `key f1` at f13840 to drop back from x2).
3. **Read the event script's waits** for area 99 run 7's messages (the event VM's op that waits on a message,
   and what it reads of the box - state, sub-state, `Msgbox_StepCount`, flag 4 / 2) against what an instant
   page can skip: a page that completes and is closed within the frames another waiter samples once.
4. **Compare the PlayStation** for the same messages: a stall that needs a timing the PSX could never produce
   is ours (DIV-0158) to fix, by giving the instant page the frame the waiter needs.

## 5. Related, fixed alongside (PR #129)

- #118 (D355): the Skill Notes count's total read off our stack. Ours.
- #120 (D356, DIV-0177): the area's song lost after a jingle (the Flier's `ITEM01`). Capcom's port; restored.

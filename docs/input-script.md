# Scripted input: walking the game to a screen unattended

**Status:** WORKING (verified 2026-09-21; extended 2026-09-23 with recorded
routes, §5a, and 2026-09-24 with captures the game writes itself, §3)

The owner asked for a way for an agent to reach menus and screens while nobody
is at the keyboard, and to capture them. This is it: a **recipe** of pad
presses played *inside* the game by `bof3x.dll`, counted in the game's own
frames, and a driver that launches the game with one and saves the window at
each `shot`. Built 2026-09-21. It reached the title menu, every row of the
Config screen, a save's load, the field and every slot of the field menu's top
bar on its first day, and it produced the DIV-0010 A/B the handoff had been
waiting on for the owner (§5).

## 1. How to run one

```
python tools/input_run.py tools/recipes/config_screen.txt --out analysis/shots/config --lang en
python tools/input_run.py tools/recipes/field_menu.txt --out analysis/shots/menu --lang en --original "D3d_DrawSprt"
```

`--lang` sets `BOF3X_LANG`, `--original` sets `BOF3X_ORIGINAL`, `--env K=V`
sets anything else. Output: `OUT/NAME.png` for every `shot NAME`; `mark`,
`peek` and `until` lines echoed; exit 0 when the recipe says `done`. Since
2026-09-24 **the game writes each shot itself** (`BOF3X_SHOT_DIR`, set to
`--out`; `render::SaveFrame`): the render target as drawn, at the target's
size - the window's client under DIV-0042 - before the present's scaling and
any look, whatever covers the window (§3). The runner converts it to PNG and
falls back to grabbing the window's client area from the screen only when
the file is missing. **`--no-front`** leaves the window's z-order alone:
DIV-0033 keeps the game running unfocused, so a recipe can play while the
machine is in use. Without it the runner still pulls the game to the front
every half second, as `attract_run.py` does, and the keyboard and mouse are
best left alone.

(As first written, 2026-09-21: the game had to be windowed, the shot was a
screen grab of the 640 x 480 client, and the machine had to be left alone
for the run.) Shots land in `analysis/`, which is gitignored: they are
pictures of Capcom's game and are never committed.

Routes can also be **recorded by playing** (`BOF3X_RECORD`, §5a) and played
back as recipes; the owner's `shop.txt` was the first.

Recipes in `tools/recipes/`:

| Recipe | What it reaches | Frames |
|---|---|--:|
| `title_timeline.txt` | no input; a shot every 60 frames from launch | 601 |
| `config_screen.txt` | title menu, then Config with the cursor on each of its seven rows | 838 |
| `field_menu.txt` | save 5 loaded, the field, the menu, each top-bar slot | ~1,400 |
| `battle_commands.txt` | NEW GAME, the opening scene's dialogue, its scripted battle; each command of the cross held, the skill list opened, the skill used | ~3,200 |
| `menu_screens.txt` | save 5, into Items, Ability, Equipment, Tactics and Status and out again, each checked on the menu state | ~1,700 |
| `field_view.txt` | save 5 loaded to its field; four shots of the view, 45 frames apart | 1,400 |
| `camera_rotate.txt` | save 5 loaded to its field; R1 held with left, then right (60 frames each, the view turns about 31 degrees and springs back on release), R1 alone, left alone, L1 with left - the camera branch of `AreaMap_Frame` (docs/map-layers.md) | 1,850 |
| `attract_cycle.txt` | no input; the attract sequence, a shot every 200 frames from 1,530 to 12,330 (each shot holds 30 frames) | 14,150 |
| `shop.txt` / `shop_ab.txt` | recorded (§5a): save 3, the item and weapon shops, the inn, the save menu; `_ab` has a shot every 90 frames | 3,157 |
| `worldMapAndAreaTransition.txt` / `_ab.txt` | recorded by the owner 2026-09-23: save 3, out of the town to the world map (Yraall Region, two place plates), into an area, the map again, two adjacent areas ([`world-map.md`](world-map.md)); `_ab` has a shot every 60 frames | 2,143 |
| `combat.txt` / `combat_ab.txt` | recorded by the owner 2026-09-23: slot 0 (an F12 save in the field), a few steps, a random encounter, the fight - the command cross, a spell, a heal, a steal attempt, the item list - to "You won the battle!"; `_ab` has a shot every 60 frames. **Deterministic**: two playbacks under our build, 43 of 43 captures identical, the encounter at the same frame and the same damage every turn (`analysis/shots/combat_a`, `combat_b`) - the exe imports no `srand` or `time`, so the CRT's `rand` starts from its fixed seed, and the encounter test counts steps ([`event-ops.md`](event-ops.md), `Field_EncounterDue`). The battle start the handoff was waiting for. | 2,620 |
| `worldmap_sliver.txt` | the route above cut at frame 1260 with one shot on the map - a one-minute check of the compass needle (DIV-0044) | 1,260 |

## 2. How it works

`src/hook/input_script.cpp`. With `BOF3X_INPUT` set to a recipe file, one call
is re-aimed: WinMain's call of `Input_Latch` at `0x4FCDDE`. Ours calls
Capcom's latch - the devices are polled and re-acquired exactly as they would
be - and then, while the recipe plays, overwrites pad 1's three words:
`Input_Held` `0x7E1BE8`, `Input_Previous` `0x7E1BEA`, and `Input_Pressed`
`0x7E1BEC` as `(previous ^ held) & held`. **The real keyboard is ignored
until the recipe ends**, then handed back.

Two facts from the disassembly decided the shape (both in `symbols.toml`,
2026-09-21):

- **The latch is not once a frame.** WinMain calls it on every pass of its
  message loop, and while the window is inactive the loop spins there without
  running a frame (`0x4FCE16` jumps back). So the recipe does not count latch
  calls: it advances when `Frame_Counter` `0x937F94` changes - incremented at
  `0x4FCFE5` once per frame run - and writes the three words from its own
  previous and current word, so a latch repeated inside one frame rewrites the
  same values instead of losing the edge in `Input_Pressed`.
- **The bit layout is the PlayStation pad's**, read out of `Pad_Read`
  `0x5A9700` and the default key table `0x66C648`: `0x1` L2, `0x2` R2, `0x4`
  L1, `0x8` R1, `0x10` triangle, `0x20` circle, `0x40` cross, `0x80` square,
  `0x100` select, `0x800` start, `0x1000` up, `0x2000` right, `0x4000` down,
  `0x8000` left. That settles the question `Input_Held`'s note had left open,
  and makes `Field_CopyInput`'s exchanged bits the four directions by
  measurement rather than analogy.

Recipe frame 0 is `Frame_Counter` 0 - the first frame of the main loop, after
the logo videos - and the game is deterministic from launch
([`attract-mode.md`](attract-mode.md)), so a recipe lands on the same frames
every run: `field_menu.txt` met its two `until`s at frames 800 and 1,112 on
two runs out of two.

**Why it is not a divergence.** Nothing is patched unless `BOF3X_INPUT` is set,
and then nothing of Capcom's is replaced - the latch runs, and only its output
is overwritten. It is instrumentation, like the call tracer: no ledger entry,
and it cannot move the frame hash of a run that does not set it.

## 3. The recipe language

One step per line; `#` starts a comment; buttons join with `+`
(`cross+right`).

| Step | Does |
|---|---|
| `set hold N` / `set gap N` | frames a press holds / is released after (defaults 4 and 8; apply to later presses) |
| `wait N` | N frames, nothing held |
| `press BUTTONS [xK]` | K times: hold, then release |
| `hold BUTTONS N` | N frames held, no release after |
| `seek BUTTONS ADDR TYPE OP VALUE [max K]` | press BUTTONS until the condition holds, checking before each press; K presses (default 16) without it FAILS the recipe. For cursors that remember where they were: `seek right 0x929F05 u8 == 0` |
| `until ADDR TYPE OP VALUE [timeout N]` | nothing held until true, checked once a frame. TYPE `u8`/`u16`/`u32`; OP `==` `!=` `&` (any bit) `!&` (no bit). A timeout (default 3,600) FAILS the recipe and hands the pad back |
| `shot NAME [N [BUTTONS]]` | hold BUTTONS (default nothing) for N frames (default 30) - the battle's command cross shows a command only while its direction is held - then **freeze** the game until the driver has the picture (below). The picture is the frame presented as the hold ends: one exact frame, the same on every run |
| `peek ADDR TYPE [LABEL]` | log a value |
| `mark TEXT` | log a line |
| `end` | stop |

**Frozen shots** (2026-09-21). `input_run.py` sets `BOF3X_SHOT_WAIT`. At the
end of a shot's hold the latch - the top of WinMain's loop, where the last
frame built has just been presented and the next has not begun - logs `input
shot NAME recipe frame F frozen` and waits on the event
`Local\bof3x_shot_<pid>` until the driver has grabbed the window and set it (3 s
at most: Windows ghosts a window that pumps nothing for 5). The DIV-0022 clock
stops meanwhile (`GameClock_Pause`), so the frame deadline has no debt to
replay and the frames after a shot are presented as any others. Before this,
the driver grabbed 0.4 s after the log line while the game ran on: two runs
of `attract_cycle.txt` then differed in 34 of 55 shots, the mine-cart scene a
camera step apart and the title's prompt at another point of its blink.
Frozen, two runs of all ours agree in **55 of 55** (`analysis/shots/frozen_a`,
`frozen_b`). Without `BOF3X_SHOT_WAIT` - a recipe run by hand - a shot is
logged as its hold starts and nothing waits. With `BOF3X_ORIGINAL=Game_Clock`
the freeze still works, but the pause is replayed unrendered afterwards, and
the log says so.

**The game writes its own captures** (2026-09-24). `input_run.py` sets
`BOF3X_SHOT_DIR` to its `--out` directory, and at each shot the DLL writes
`NAME.bmp` there itself - the render target read back through a staging
copy (`render::SaveFrame`, `src/render/render_d3d11.cpp`): the frame as the
game drew it, at the target's size, before the present's scaling and any
look, whatever covers the window. The runner converts it to `NAME.png` and
falls back to a screen grab only when the file is missing. With `--no-front`
the runner never touches the window's z-order: DIV-0033 keeps the game
running unfocused, so a recipe can play while the machine is in use. Checked
on the Config recipe: 8 of 8 shots "from the game" with the window in the
background, `analysis/shots/capture_check`. **F11** writes the same frame by
hand, `bof3x-frame-<Frame_Counter>.bmp` beside the DLL, "Frame saved" on
screen. Captures made this way are the target's size (the window's client
under DIV-0042), so an A/B needs both sides made the same way.

BUTTONS is a PSX button name (`up down left right cross circle square triangle
l1 l2 r1 r2 start select`) or **`@ADDR`**, the u16 at that address when the
step starts. That exists for the field's button assignments, which are part of
a save (§4): `press @0x903584` opens the menu whichever shape the loaded save
gave it. A parse error, or an address that is not mapped, stops the game at
start-up with the line number (`Fatal`).

Anything worth waiting on is better as `until` than as `wait`: a wait is only
as good as the load time it was measured against. The ones known so far:

| Address | Type | Meaning |
|---|---|---|
| `0x66C7E8` | u16 | field task mode: 2 field, 3 menu, 4 seen during a talk, **5 battle**, 11 after Start in save 5's area ([`menu-screens.md`](menu-screens.md) §1; mode 5 is what the mode-2 handler's request 3 sets, `0x495A4A`, and what the new game's battle showed) |
| `0x929F00` | u8 | menu state, 1 = top bar ([`menu-screens.md`](menu-screens.md) §1) |
| `0x929F05` | u8 | the menu's top-bar cursor, from 0: Items, Ability, Equipment, Tactics, Status, Config, Camp. **Remembered between openings** - seek it, do not count presses |
| `0x903584` / `0x90358E` / `0x903590` | u16 | the save's menu button / confirm buttons / cancel buttons - use as `@ADDR` |
| `0x937F94` | u32 | `Frame_Counter` |

## 4. What the game was measured to do with it

Measured with these recipes, 2026-09-21 - facts about this build with the
owner's saves, not claims about Breath of Fire III in general:

- The title logo is up by frame 420 and takes Start by 480.
- The title menu opens with its cursor on LOAD GAME (saves present); down
  from there is CONFIG, and down again wraps to NEW GAME. **Circle confirms**
  there: circle on NEW GAME faded out and started a new game.
- The load screen lists slots 0-5, three to a page; down x5 from slot 0 is
  slot 5; circle, circle loads it ("Load game? Yes" then "Loading complete"),
  and one more circle leaves for the field.
- **The field menu button is save data.** `0x41C0A0` / `0x42B400` test
  `Input_Pressed` against the word `Field_MenuButton` `0x903584`; with save 5
  loaded it held `0x80`, square, and square opened the menu. The owner:
  shape buttons are assigned per save, and save 5 came from a US save whose
  assignment differs from saves 0-3; saves 0 and 1 are from before the menu
  is available, and save 2 is at camp. Bit `0x40` of `0x9039A2` blocks the
  menu (clear in save 5).
- In save 5's area Start put the field in mode 11 through `0x536B60`, a
  handler of its own, and did not open the menu.
- **A new game reaches a battle deterministically** (owner's suggestion): up
  from LOAD GAME is NEW GAME; circle advances the opening scene's dialogue;
  the scripted battle begins at recipe frame 2,824 on every run so far
  (`seek circle 0x66C7E8 u16 == 5`). **The command cross is hold-to-choose**:
  a direction shows its command only while held - left Watch, right Defend,
  down Item, up Skill - and release snaps back to Attack, so a command is
  chosen with `hold up+circle`. Circle on Attack goes to target selection.
  L1 shows Charge and R1 Escape (owner: Escape is selectable but the intro
  fight cannot be fled); **L2 and R2 confirm**, like circle - holding L2
  chose Attack.
- **Confirm and cancel are save data too**: with save 5 loaded
  `Field_ConfirmButtons` `0x90358E` held `0x43` (cross, L2, R2) and
  `Field_CancelButtons` `0x903590` `0x10` (triangle) - a US layout, where the
  title and load screens, before any save is loaded, confirm with circle.
  Cross entered each menu screen and triangle left it; circle did not enter.

## 5. The first harvest

From `analysis/shots/`, all local:

- **The title menu in game** (DIV-0014, [`USER_CHECKS.md`](USER_CHECKS.md) 6):
  `config/title_menu.png`. NEW GAME, LOAD GAME, CONFIG in English, the
  selected row brighter. The glow question is still the owner's to judge.
- **Config, every row** (DIV-0015..0017): `config/config_row1..7.png`. The
  two buttons above the panel are still Chinese, as the handoff says.
- **DIV-0010, the A/B**: `field_menu/` against `field_menu_orig/` (run with
  `--original "D3d_DrawSprt,D3d_DrawSprt8,D3d_DrawSprt16"`), cropped in
  `div0010_ab_x3.png`. With Capcom's handlers the bottom row of every HP / AP
  numeral is cut off and the portrait frame has a seam under it; with ours
  the numerals are whole. This is the first time the fix has been seen in the
  menu, which is where it matters.
- The field menu's top bar, each slot: `field_menu/menu_top0..6.png`.
- A save-point diary's English prompt ("Do you want to make a record of your
  journey so far?"), met by accident in save 0.

## 5a. Recording a recipe by playing (2026-09-23)

`BOF3X_RECORD=<path>` before the launcher turns the same latch round: the
player plays, and each frame's pad word is written to `<path>` as `hold` /
`wait` lines. The word is sampled at the first latch of a new frame and held
for the rest of that frame, as playback holds a recipe's - so the game sees
the same inputs recording and playing back, edge for edge; a tap shorter than
a frame is lost to both. F12 (read from the game's own DirectInput key bytes
at `0x7DE828`, DIK `0x58` - `GetAsyncKeyState` saw nothing on the first
recording) writes a one-frame `shot` in place of its frame.
`tools/recipe_shots.py` adds shots every N frames to a recording after the
fact, again without moving a frame. Record in the language you will play back
in: text timing differs.

The first recording is the owner's `shop.txt`, from save 3: the item shop
(buy, sell), a found Molotov, the weapon shop with equipping, walking and
running, the inn's Rest and the save menu (not saved - no file written), 3,157
frames. `analysis/validate_shop.sh` plays `shop_ab.txt` (35 shots) in English
twice: the original side `BOF3X_ORIGINAL=*` less the language machinery
(`LoadDatFile`, `MsgBox_DrawChar`, `Msg_SystemPtr`, `ConfigText`,
`MenuVerbs`, `TitleMenu_Widths`, `Text_DrawString`, `Text_DrawImmediate`),
the DIV-0022 clock and the DIV-0004 fix; our side with every pixel-changing
divergence off. **35 of 35 captures identical, none black**, with 583 ours
(rounds one to four and groups S and T of the fifth). Two things learned on
the way: `BOF3X_ORIGINAL` now takes `-NAME` to exclude one name from `*`,
and `*` no longer switches off the input hook itself - it did, and the
all-original side sat in the attract sequence with no input. And
`Text_DrawImmediate` belongs in the language set: without it the inn's Yes /
No is spaced at Capcom's 12 px, DIV-0006's rule, the one capture that
differed before it was added.

## 6. Limits and next steps

- **No branching.** A recipe cannot choose on a value; `until` and `@ADDR` are
  the only reads. Enough for walking menus.
- **No input during the logo videos**: they play before the main loop, which
  the recipe's frames start after.
- ~~The driver grabs the screen, so the window must be on screen and in front;
  it is, for the run. A grab from the Direct3D surface itself is
  [`IDEAS.md`](IDEAS.md) I14's question, not this tool's.~~ Since 2026-09-24
  the DLL writes the render target itself (§3, `BOF3X_SHOT_DIR`) and
  `--no-front` leaves the window where it is; the screen grab is only the
  fallback. Hashing frames for comparison (I14's levels) is still open.
- Pad 2 is never touched.
- Worth writing next: a recipe per screen the handoff owes a look at - the
  Items and Ability lists (clipping, [`dialogue-localisation.md`](dialogue-localisation.md)
  §6), Status, Equipment - each a `field_menu.txt` with a few more presses.

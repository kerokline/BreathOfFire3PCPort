# Controls: what the port reads, what the Config panel's columns mean, and a plan for modern pads

**Status:** DONE (2026-09-24) - steps 1 to 3 of §6 built and confirmed by
the owner (DIV-0050, DIV-0051); step 4, the in-game binding screen, deferred
to [`IDEAS.md`](IDEAS.md) I20 at the owner's word. The owner's
decisions are in §5.

## 1. What the 2001 port actually does (read 2026-09-24)

All from `tools/pe_disasm.py` / `tools/pe_xref.py` over the addresses named;
the pad bit layout is [`input-script.md`](input-script.md) §2's.

**Controllers were always allowed - through DirectInput 7's joystick path.**
`DInput_Init` `0x5A94C0` calls `DirectInputCreateA(.., 0x700, ..)`, enumerates
`DIDEVTYPE_JOYSTICK` attached devices and keeps the **first** one
(`0x5A9620` creates it into `0x7DE930` and stops the enumeration), sets
`c_dfDIJoystick`, exclusive + foreground, a -1000..1000 range on axes X, Y, Z
and Rz, and acquires it. `Pad_Read` `0x5A9700` then maps it digitally:

| Joystick | Pad bit |
|---|---|
| X > 500 / < -500 | right `0x2000` / left `0x8000` |
| Y > 500 / < -500 | down `0x4000` / up `0x1000` |
| buttons 0, 1, 2, 3 | cross `0x40`, square `0x80`, triangle `0x10`, circle `0x20` |
| buttons 5, 6, 7, 8, 9 (only if `0x7DE938` is 1) | R2 `0x2`, L1 `0x4`, R1 `0x8`, start `0x800`, select `0x100` |

Button 4 is never read, **L2 is reachable from no joystick button**, and the
**POV hat (the d-pad of every modern pad) is never read**. `0x7DE938` is
written to 1 at `0x5A9674`, in the enumeration callback, so it means only
"a joystick was found". On an Xbox-class pad today, which DirectInput exposes
as A B X Y LB RB Back Start LS RS on buttons 0..9, that gives A = cross,
B = square, X = triangle, Y = circle, RB = R2, Back = L1, Start = R1, left
stick click = start, right stick click = select, no L2, and a dead d-pad. It
"works" in the sense that the game moves.

**The keyboard is a 32-entry table of `(u16 DIK scancode, u16 pad bits)`** at
`0x7DE7A8`, ended by a key of 0, applied over the DirectInput keyboard's 256
key bytes at `0x7DE828` (non-exclusive, background - read even unfocused,
which DIV-0033 compensates for). The table is filled by `Cfg_Load`
`0x4FD030` from **lines 3 and up of `BOF3.CFG`**, two integers a line through
`sscanf`, when the file has more than two lines (`0x5A9860` copies the
array); otherwise the default at `0x66C648` (`0x5A9880`). So any key *can* be
bound in the original, by editing the file - there is just no UI for it, and
nothing on screen follows it (§2). The default has 24 entries, 8 free:

```
 0 0xC8 up   1 0xD0 down   2 0xCB left   3 0xCD right       (arrows)
 4 0x2C Z triangle   5 0x2D X cross   6 0x2E C square   7 0x2F V circle
 8 0x1E A L1   9 0x10 Q L2   10 0x1F S R1   11 0x11 W R2
12 0x1C Enter start   13 0x36 right Shift select
14 0x01 Esc cross   15 0x39 Space circle
16..19 keypad 8 2 4 6 the directions; 20..23 keypad 9 1 7 3 the diagonals
```

**The default table is a rule of the engine** since the disc-only round
(the owner's call, 2026-10-11): `input::DefaultKeys()` in
`src/input/bindings.cpp` names each entry by its `DIK_` key and its
PlayStation button(s), in the order above - so a disc-built install, which
has no `BOF3.exe` to read `0x66C648` from, has the same defaults. The
launcher compares the rule with the file's table when the file is there (a
refusal on a difference) and otherwise uses the rule; the engine writes it
to `0x66C648` at start, after the same comparison, and `Cfg_SetDefaultKeys`
copies the rule ([`exe-import-engine.md`](exe-import-engine.md) 2.2).

The word `Pad_Read` returns is the joystick's OR the keyboard's; `Input_Latch`
`0x4FC6A0` (sole caller) turns it into `Input_Held` / `Input_Previous` /
`Input_Pressed`. That is the whole input path: **one leaf function with no
arguments whose sole caller is ours already** (`win_main.cpp`).

## 2. The Config panel's Controller row: two layers, and what each column is

The row opens a six-line sub-panel (`0x461A50`, rows drawn by `0x461AF0`).
Each line is one **action** - Speak, Move, Action, Menu, View, Change - and
what the game stores for it is a **PlayStation pad word**, six of them at
`0x903580..0x903590`, which are **save data** (`Field_MenuButton`,
`Field_CancelButtons` in `symbols.toml`). `Init` writes the defaults at
`0x460FDE`: Speak `0x23` (circle, with L2 and R2 always ORed in at
`0x461485` / `0x46152D`), Move `0x80` square, Action `0x40` cross, Menu
`0x10` triangle, View `0x8` R1, Change `0x4` L1. The line-to-slot table is
`0x6536F0` = `00 01 06 02 03 04`.

**How the sub-panel rebinds (`0x461459..0x46153B`):** with a line
highlighted, press *any* button other than L2 / R2 (`and ecx, 0xFC`); the
code finds the line that button currently belongs to, gives *that* line the
highlighted line's old button, and gives the highlighted line the pressed
one - a **swap of PlayStation buttons between actions**. This is the
PlayStation release's own feature, kept intact. The game's logic tests
`Input_Pressed` against these words, never against physical keys. A
PlayStation save with swapped buttons carries the swap into the PC.

**The two columns right of the name** (`0x461AF0` draws two boxed cells at
`row x + 0x3F` and `+ 0x53`, then `0x461C00(x, y, word)` fills them):

- **Left cell, white: the keyboard key.** A *hard-coded* one-byte string per
  pad bit - `V` (`0x65382C`) for circle, `C` `Z` `X` (`0x653820..`) for
  square / triangle / cross, `S` `A` (`0x653818..`) for R1 / L1. These are
  the *default* table's letters. They do not read the live table, so a
  `BOF3.CFG` that rebinds keys leaves the panel lying. (A latent defect of
  the 2001 port; worth a `known-defects.md` entry when this is built.)
- **Right cell, coloured: the PlayStation button icon**, drawn as a glyph
  through the text draw `0x516B30` in colour 2 or 3 - the one-byte codes
  `b` `c` `d` `X` `f` `g` (`0x66A2E0..`) name the glyphs (code - `0x26`) that
  in the shipped Chinese table are the circle, cross, triangle, square, L1
  and R1 pictures. **The English overlay writes the US disc's letters over
  those slots** (DIV-0006's doubled font), which is why the owner's capture
  shows a red `b`, a pink `X`, a blue `c`, a green `d`, and `g` / `f`
  (`analysis/shots/ctrl_fix2/controller_panel.png`, and the owner's
  screenshot 2026-09-24). The PlayStation screen has only this column. The
  US atlas carries the four shapes three times (the 8 x 12 set around row
  `0x54`, the small set at `0xE7`, another at `0xA8`) and `L1 R1 L2 R2
  START SELECT` labels on row `0x15` (`analysis/font/us_sheet0.png`), so the
  icons can come back from the disc the way the letters did. (That the
  Chinese cells at those six indices are the icons is inferred from the
  codes and the capture, not yet rendered from `FIRST.DAT`; `font_pc.py`
  can settle it.)

So "the second column I don't quite understand" is the PC team's addition:
which keyboard key *defaults* to that PlayStation button. Their column is
the right idea, done statically.

### 2.1 The default field buttons are a rule of the engine (2026-10-12)

The six words are the first of **nine pad words** at `0x903580..0x903592`
(word 2 `Field_MenuButton`, 7 `Field_ConfirmButtons`, 8
`Field_CancelButtons`). `Boot_Task` (`0x496B60`, `src/game/mode_flow.cpp`)
copies their defaults from `0x656AEC` (18 bytes, `0x496BB8..0x496C25`).
No disc carries that range in the PC's shape. A disc-built image of a
Western disc therefore had zeros there (`recipes/exe-gaps.tsv`'s
`game-value` row, read without running in the gate's attract pair). So a
New Game on a disc-only install would probably have started with every
action unbound (not played). Loading a save restores the words. Since the
disc-only round, the defaults are **a rule of the engine**, the same way
`Key_TableDefault` is (section 1). `FieldButtonDefaults` in
`src/hook/exe_own.cpp` names each word's button by `input::Action`
(`src/input/bindings.h`), plus a zero padding halfword. It is one of the
engine's own regions (`0x656AEC..0x656B00`; [`exe-engine-own.md`](exe-engine-own.md)
2). It is written at map time and verified byte for byte against
`BOF3.exe`'s `.data` at every start when the file is there (a Fatal on a
difference).

| Word | Action | Default button |
|---|---|---|
| 0 | Speak | circle, with L2 and R2 |
| 1 | Move | square |
| 2 | Menu (`Field_MenuButton`) | triangle |
| 3 | View | R1 |
| 4 | Change | L1 |
| 5 | none of the Config screen's lines; no instruction reads it alone (`LoadMenu_Apply` and the save writer move it with its neighbours) | select |
| 6 | Action | cross |
| 7 | confirm (`Field_ConfirmButtons`) | circle, with L2 and R2 |
| 8 | cancel (`Field_CancelButtons`) | cross |

Each part of the rule rests on a measurement:

- **The PC's own code agrees with the rule.** The Config screen's `Init` (`0x460FDE..0x46100B`)
  writes words 0, 1, 6, 2, 3, 4 as immediates, each equal to the rule.
  `LoadMenu_Apply`'s Start-held reset (`0x588629..0x588670`) writes all
  nine as immediates. Its only difference is circle alone in words 0 and 7,
  with no L2 or R2. The Config screen's rebinding clears L2 and R2 from
  words 0 and 7 and ORs them back in (`0x461485` / `0x46148C`, `0x46152D` /
  `0x461534`). That is where "with L2 and R2" comes from.
- **The PlayStation has the same defaults, in its own executable.** The JP
  `SLPS_009.90`'s `Boot_Init` (`0x8014E974`; the sibling's
  `symbols.toml`) copies nine halfwords from `0x8017FEE4` to `0x80145AB0`.
  Read from the sibling's `disc/SLPS_009.90`, they equal the rule. So they
  are a Capcom game value (the Japanese release's default assignment), not
  the port's.
- **The US release's assignment is a different one.** The US disc's
  `SLUS_004.22` copies them from `0x8017F56C` (`lui 0x8018` / `addiu -0xA94`
  at `0x8014EB24`, its `Boot_Init`). Read there, in the same order, it gives
  cross for Speak and confirm (with L2 and R2), circle for Move, square for
  Menu, triangle for Action and cancel, and the same R1, L1 and select.
  This fits the `symbols.toml` measurement of save 5 (`Field_MenuButton`
  square, confirm cross with L2 and R2, cancel triangle). The PC port took
  the Japanese assignment. The engine's rule is the PC's, because it must
  equal `BOF3.exe` when the file is present. So a US-disc-only install
  starts with the Japanese buttons, as the PC does. Offering the US
  release's assignment on a Western install would be a divergence, and is
  for the owner to decide.

Verified with headless self-tests on 2026-10-12. Each exited 0 with
`self-test only: done`.

- The exe host with `BOF3.exe`, and with `analysis/cache/pc-plus-us`: `exe_own: 18 regions,
  20886 bytes ... each equal to BOF3.exe's .data`.
- The real install with a fresh US `base/exe/` (`exe_tables.py build`): the
  range is classed `own`, and 455 `gap` ranges are left (456 before).
  `627132 of 638976 bytes equal` (627,123 before, so the nine non-zero
  bytes), and every class that must equal does.
- The disc-only scratch install with no `BOF3.exe`, with the IN group's
  cache and with the fresh one: `inject: 10088 ours`. The IN cache was built
  before this change, so its `data.toml` still logs the range as a `gap`, but
  the fill writes over it. A cache rebuilt from this recipe classes it `own`.
- Hosted, `BOF3X_SHADOW=exe_own,shell,mode_flow`: `exe_own self-test: a fill
  into a zeroed image writes 16904 non-zero bytes in 18 regions, each the
  PC's`, and `mode_flow` and `shell` report 0 mismatches.

## 3. What is wrong today, in one list

1. Modern pads: d-pad dead, L2 unreachable, button order scrambled, and
   an XInput pad's two triggers arrive on one DirectInput Z axis, which the
   port ranges but never reads.
2. Only the first attached joystick; no hot-plug (enumeration once, at
   `Game_Init`).
3. Keyboard rebinding exists only by editing `BOF3.CFG`; the panel's key
   column is hard-coded to the defaults.
4. Under `BOF3X_LANG=en` the icon column draws letters.
5. Two layers with one UI: the panel edits the PlayStation-button layer and
   shows a static shadow of the physical one.
6. *(Fixed 2026-10-21, DIV-0152.)* No pad reset. The Western discs return to the
   title when Start and Select are held for 60 frames. The Japanese disc, and so the
   port, had only F9 twice, a keyboard reset. Now Start and Select held for a second
   do what F9's second press does (`BOF3X_ORIGINAL=PadReset` for the original).

## 4. Proposal

### 4.1 One seam: take over `Pad_Read`

`Pad_Read` is cdecl, no arguments, returns the pad word, and its sole caller
is ours. A reimplementation (`src/game/pad_read.cpp`, one DIV entry) returns
`keyboard | pad` from:

- **Keyboard:** the same 256 DirectInput key bytes at `0x7DE828` (keep
  `DInput_Init`'s keyboard device; `BOF3X_RECORD`'s F12 reads those bytes)
  through a table **we** own - the `BOF3.CFG` lines 3+ format kept as the
  storage so a hand-edited file still works, plus the launcher UI over it.
- **Pad:** XInput 1.4 (`XInputGetState`, ships in Windows 8+, no
  redistributable, ~40 lines): d-pad and left stick both as directions
  (stick with a radial dead zone, the original's 50 % threshold as the
  default), triggers as L2 / R2 above a threshold, every button and both
  stick clicks bindable. Poll all four slots, take the OR (or the first
  connected - owner's call); connection state re-checked at a low rate so a
  pad plugged in mid-game is picked up. **Skip `DInput_Init`'s joystick
  enumeration when XInput is on**, or a pad is read twice.
- **DirectInput pads stay as the fallback** for non-XInput hardware, with two
  fixes: read the POV hat as the d-pad, and make the button order a table
  rather than the hard-coded 0..3 / 5..9.

Default XInput map, subject to §5: A cross, B circle, X square, Y triangle,
LB L1, RB R1, LT L2, RT R2, Start start, Back select, d-pad and left stick
the directions. (Nintendo-layout owners will want A / B swapped - a launcher
checkbox, "confirm on the east button".)

### 4.2 Storage: the physical map in `bof3x.ini`, the PlayStation map where it is

Keep the two layers, because the lower one is game logic and save data:

- **Physical layer** (key / pad button -> PlayStation bit): `bof3x.ini`
  `[keys]` / `[pad]`, read by the DLL at start and re-read on the panel's
  changes; the launcher gets a Controls tab that edits the same file.
- **PlayStation layer** (action -> PlayStation bit): untouched, stays in the
  save, `Init` still resets it.

### 4.3 The panel: the PSX swap table back as it was, and a binding area beside it (owner's decision, 2026-09-24)

**The Controller row stays the PlayStation's.** Its sub-panel keeps the
swap semantics untouched and goes back to the PlayStation's *one* column:
the button icon, from the disc's cells. The PC team's hard-coded key
letters (`0x461C00`'s left cell) go, since a live binding area makes them
redundant and they were wrong whenever `BOF3.CFG` was edited. That is one
ledger entry: the icon cells imported, the key cell dropped, the frame back
to its PlayStation width (DIV-0026 widened it for the two cells and the
English names; re-measure with one cell).

**A seventh row, "Key Config" (working name), opens the binding area.** The
row tables `0x653808` / `0x653810` are 8 entries with two unused (rows 6
and 7 hold 0 options) and records 15 and 16 are unused, so the data has room;
the panel draw `0x461710` loops six rows and opens a frame `0x21` x `0x0D`
cells, both to grow by one; the caption is system-pool message `0xBC + row`
(`0x460E5B`), so the seventh caption is a pool line of ours in the English
overlay and a Chinese one for the port's own text. The cursor byte
`0x929F02` runs 0..5 and the row handler at `0x460E8F..` dispatches on it.

**The binding area is a screen we draw**, through the game's own pieces
(`Menu_DrawFrame` of DIV-0011, the small UI font `0x516E70`, the cursor
hand), not Capcom's row code: one line per PlayStation input, fourteen -
up, down, left, right, circle, cross, triangle, square, L1, L2, R1, R2,
start, select - each with **two cells: keyboard, pad**. Fourteen lines at
the sub-panel's 16 px pitch is 224 of 480 px, so it fits as one column
beside the main panel where the swap sub-panel sits, or as two columns of
seven if the owner prefers it short. Highlight a line, press: a scancode
fills the keyboard cell, an XInput button the pad cell - **the device the
press came from decides the cell**, which is the owner's "depending on
whether the key was sent from a controller or a keyboard". Press the same
input on the same line again to clear it; the cancel button leaves. `Init`
on this screen restores the default table (`0x66C648`'s keys and the XInput
default of §4.1); the main panel's `Init` does not touch it, since the
original's `Init` never touched `BOF3.CFG` either.

Bindings write straight to the physical layer (§4.2) and take effect on the
next frame, because `Pad_Read` reads the live table.

**Cells are our own overlay, not glyphs.** Key names and pad glyphs are
drawn by the Direct3D 11 backend at the cell's rectangle from a small
texture of ours (letters, `Ent` `Sp` `RSh` `Esc` and the arrows for keys;
A B X Y LB RB LT RT Start Back and the stick directions for XInput, the
PlayStation shapes for a DualShock-class pad if that is ever detected).
That keeps the glyph table's ten free slots for the PlayStation icons the
Controller row needs.

### 4.4 Later, not now

- Button prompts in dialogue and tutorials use the same icon glyphs; with a
  pad detected they could switch to that pad's glyphs. Needs a census of
  where the icon codes appear in the pools (`loc_build.py` can find them).
- Rumble: not known whether the PlayStation release used the DualShock's
  motors - **not a claim, a question for the owner**.
- Steam Input / DualSense: XInput covers Xbox pads natively and everything
  Steam or DS4Windows presents as one. Native DualSense would need
  `Windows.Gaming.Input` or SDL3 (zlib licence - compatible with
  [`LICENSING.md`](LICENSING.md), but a dependency the project has so far
  avoided); design the pad reader behind one small interface so either can
  replace XInput without touching the panel.

## 5. Decisions

**Settled by the owner, 2026-09-24:** the Controller row keeps the
PlayStation's swap table and its one icon column; the physical bindings
(keyboard and XInput) get their own area in the Config screen, a seventh
row. Both layers live: the physical map feeds the pad word, the save's swap
words decide what the word does.

The seventh row was later deferred (§6 step 4, [`IDEAS.md`](IDEAS.md) I20);
the physical bindings live in the launcher's Controls dialog instead.

**Still open** (as written; all four since settled, noted 2026-09-24):

1. Pad API: XInput now, behind an interface (recommended), or SDL3 from the
   start for DualSense and the community glyph database. **Settled: SDL3**
   (the owner, DIV-0050's rationale).
2. Several pads: OR them all, or first connected. **Settled: one pad, the
   first connected, re-opened on hot-plug** (DIV-0050).
3. Default XInput layout: Xbox positional (A cross) or a Nintendo-style
   toggle from the start. **Settled: positional by default, with a toggle
   from the start** - `BOF3X_PAD_LAYOUT=positional|nintendo|auto`,
   `pad.layout=` in the ini (DIV-0050).
4. The binding area's shape: one column of fourteen or two of seven.
   **Moved to I20** with the in-game binding screen (§6 step 4); the
   launcher's Controls dialog has fourteen rows.

## 6. Order of work

1. **Built 2026-09-24, DIV-0050:** `DInput_Init`, `Pad_Read` and
   `DInput_Shutdown` ours (`src/game/pad_read.cpp`): the keyboard path as the
   original, the pad through SDL3 (fetched and built static at configure
   time, [`THIRD_PARTY.md`](THIRD_PARTY.md) §2), one pad, hot-plug,
   `BOF3X_PAD_LAYOUT=positional|nintendo|auto`. The keyboard path has a
   shadow check (`BOF3X_SHADOW=pad_read`): a clone of the original with both
   device pointers null, 20,000 random key states and tables. Live: the recipe
   run and `tools/key_probe.py` (four scancodes at the window, each the
   expected bit in `Input_Held`) pass - DIV-0050's verification line. No pad
   was attached when it was built, so the pad side is unexercised until the
   owner plugs one in; the log line `pad: opened <name>` is what to look for.
   **The owner's Xbox Series X pad opened the same day.** Their close from
   the Config screen then found two faults of ours, fixed and recorded in
   DIV-0050's verification: the present's read of a destroyed window's
   client rectangle (on `main` too), and SDL's HIDAPI discovery taking the
   `WM_QUIT` - `WM_DESTROY` now sets `Game_QuitFlag`. `tools/close_probe.py`
   is the check. **Confirmed by the owner in play with the pad, 2026-09-24.**
   **Since 2026-11-18, DIV-0239** (the Linux round's wave 2, group R5; the
   owner's call): the keyboard is SDL3's too. With SDL's window (DIV-0238)
   and our `Pad_Read`, `DInput_Init` creates no DirectInput object and each
   latch fills the same 256 bytes at `Key_State` from SDL's keyboard state
   through `src/input/key_sdl.cpp`'s table (the DIK numbers, so `BOF3.CFG`'s
   lines and the `key.NAME=` lines mean what they meant); the table at
   `Key_Table` is applied over them as before. The bytes are all 0 while the
   window is not in front (DirectInput's were filled in the background).
   DirectInput's keyboard is left on Windows only, under
   `BOF3X_OFF=WindowSdl`, `BOF3X_ORIGINAL=Pad_Read` or `DInput_Init`, and
   `BOF3X_OFF=KeyboardSdl` (which, with SDL's window, also fills SDL's bytes
   and logs each key the two keep apart for two latches).
2. **Built 2026-09-24:** the physical map in `bof3x.ini` as `key.NAME=action`
   and `pad.NAME=action` lines (`src/input/bindings.h`, shared by the launcher
   and the DLL), handed to the DLL as `BOF3X_KEYS` / `BOF3X_PAD` when they
   differ from the default - `BOF3X_KEYS` rewrites the game's own 32-entry
   table at `Key_Table`, so the original's loop reads it; unset, that table
   stands as `Cfg_Load` filled it. The launcher's **Controls...** dialog:
   fourteen rows, an action each, two key boxes and two pad boxes, the
   face-button layout below (by position, Nintendo, automatic), Defaults.
   A key naming two actions at once (the keypad's diagonals) has no box and
   is kept unless a box takes its key. **Verified:** `tools/ini_probe.py` -
   a test ini with Z = circle, Up = down, Enter = select, Q = R2,
   Numpad9 = up+right and X unbound, through the launcher to the game: every
   word as bound, 6 of 6, and the log `BOF3X_KEYS - 5 items understood, 5
   keys in the table`, `BOF3X_PAD - 3 items understood, 2 inputs bound,
   layout nintendo`, the pad opened with that layout; the dialog seen
   populated with the defaults (screenshot, 2026-09-24). **Later the same
   day, at the owner's ask:** the cells became buttons with a press-to-bind
   capture (a keyboard hook for keys, SDL for the pad through
   `src/input/pad_sdl.cpp`, now shared with the DLL), and the launcher's
   dialogs take the pad as navigation - d-pad, cross, circle, start -
   through the same bindings ([`launcher-settings.md`](launcher-settings.md)
   §3). Verified: a probe clicked the Up key cell and pressed Y, then Home
   into the second cell (an extended key, DIK 0xC7), then Escape on the
   Cross cell: Y, Home, and X unchanged, and the shadow check and
   `tools/ini_probe.py` still pass with the shared reader. **Confirmed by the
   owner with their Xbox pad, 2026-09-24:** a pad press registers in a cell
   ("works great") and the dialogs can be walked with the controller alone.
3. **Built 2026-09-24, DIV-0051:** the Controller row back to one icon
   column. What the read found on the way: the port never drew the
   PlayStation's shapes - its glyphs at those codes are circled numerals
   and an X (`analysis/shots/ctrl_cn/controller_panel.png`, the Chinese
   screen). The atlas holds the shapes at three sizes - 8 x 8 (the UI set,
   what the PlayStation panel's measure suggested), 8 x 12 (the dialogue
   set, narrower drawings) and 12 x 12 - and the owner chose 12 x 12 by
   eye against their PlayStation screenshot; L1 and R1 are composed of the
   dialogue capital and the 8 x 8 serifed 1, the owner having spotted the
   serif no single atlas cell has. Four builds, the captures
   `analysis/shots/ctrl_icons` .. `ctrl_icons4` (DIV-0051's verification
   line), then a fifth with the icon half a glyph right and the box half a
   glyph shorter, and a sixth with the frame a cell past the box again, at
   the owner's word (`ctrl_icons5`, `ctrl_icons6`) - the sixth is the one
   built, **confirmed by the owner: "perfect"**.
4. **Deferred** (the owner, 2026-09-24: "I like this dual approach - config
   menu for mapping controllers to PSX buttons, in-game PSX button mapping
   - for now"): the seventh row and the binding screen of §4.3 are
   [`IDEAS.md`](IDEAS.md) I20.

## 7. The function keys (2026-11-18)

**Status: BUILT, host-tested; the page and the keys in play are not yet seen
by the owner.** The owner asked for the function keys on a page of their own,
rebindable, on 2026-11-18, and the same day widened it: each action takes a
keyboard key and a pad input "just like the others", and a key or pad input
that does two things is shown in red. Ledger entry: written at the merge
(the code says `DIVERGENCE (function keys; numbered at the merge)`).

### 7.1 What a function key did before

`Game_WndProc`'s key half (`WinGame_Key`, `src/game/platform/magic_080.cpp`)
compared each `WM_KEYDOWN`'s virtual key with fixed keys, in this order:

| Key | Action | Whose |
|---|---|---|
| F5 | the quick menu (`QuickMenu_Key`; it takes every key while open) | ours, the quicksave round |
| F9 | pause; pressed again, back to the title (at the title: the game stops). Any other key ends a pause | Capcom's |
| F8 | full screen or window | Capcom's, DIV-0032 |
| `BOF3X_RUBY_KEY` (F4) | the row above the text shown or hidden (`MsgBox_ReadingRowKey`) | ours, DIV-0155 |
| F6 / F7 | quicksave / quick-load of `BISLPS10.DAT` | ours, the quicksave round |
| F12 | `Save_QuickWrite`, an ordinary save to the first slot, and "Save OK" | Capcom's |
| F11 | the picture to `bof3x-frame-<n>.bmp` beside `bof3x.dll` | tooling |
| F1 | double speed | ours, DIV-0048 |

Other places that read a function key, unchanged: the recorder
(`BOF3X_RECORD`, `InputScript_RecordKey`: F12 is its shot, F1..F11 recorded
as `key` steps), the input recipes' `key f6` names
(`src/hook/input_script.cpp`), the movie player (any key but Alt and F10
skips, `src/game/fmv_play.cpp`), and the window procedures, which swallow
F10 and Alt as `WM_SYSKEYDOWN` (`game_window_win32.cpp`) - so neither ever
reaches `WinGame_Key` on Windows. The pause lines name F9: they are
Capcom's text and its translations (DIV-0038), left as they are.

### 7.2 What it does now

- **One table** (`src/input/function_keys.h`), shared by the launcher and
  the DLL: nine actions - code names `speed`, `reading_rows`, `quick_menu`,
  `quicksave`, `quickload`, `window`, `pause`, `picture`, `save_write` - each
  with one key and one pad input, either of them none. Defaults: the keys
  above, no pad input.
- **Any key, any pad input.** A key is the number the Controls cells store
  (the set-1 scan code, the extended flag as bit 7). The game finds a key
  message's number with `FnKeyOfMessage`: F1..F12 by their virtual key, as
  before (and as an input recipe's key, which has no scan code, needs);
  every other key by the scan code and extended flag of the lParam - what the
  capture's keyboard hook reads from the same lParam. Off Windows the SDL
  window builds those lParam bits from `key_sdl.h`'s number
  (`FnMessageBitsOfDik`), so a key reads the same there. One pair reads
  as the capture stores it, not as DirectInput numbers it: Pause is 0x45 and
  NumLock 0xC5 (the capture's swap, R5's note; not changed here).
- **Double bindings are allowed.** A key that is also a game button's key
  does both when pressed: the action through `WinGame_Key`, the button
  through the key bytes. Two actions on one key: the first in the old order
  above (`kFnOrder`) runs alone - which is also how `BOF3X_RUBY_KEY` on F5,
  F8 or F9 behaved before. A pad input bound to an action and a game button
  does both.
- **The pad.** Once a pass of WinMain's loop, after the message step,
  `FunctionKeys_PadPressed` reads each bound pad input and fires an action
  on its press edge (never while held, no repeat) through the same code as
  its key (`WinGame_Action`): the pause, the quick menu's rule (every key is
  the menu's while it is open; its own action closes it), the pause ending on
  any other action, the repeat rules (a pad press is never a repeat). Nothing
  fires while the window is not in front (`App_Active` and
  `win_main::g_foreground`); an input held then counts once released. With no
  pad input bound nothing is polled: the loop is as before. The loop runs in
  play, paused and with the quick menu open; a movie has its own loop, so
  neither keys nor pad presses start an action during a movie (as before for
  keys). Under Capcom's WinMain (`BOF3X_ORIGINAL=Game_WinMain`) the pad
  starts no action. Every action works from the pad; none is greyed.
- **F10 and Alt** are refused by the page's capture: Windows sends them as
  `WM_SYSKEYDOWN`, which never reaches the handler.
- **The quick menu** closes with its own action's key or pad input.

### 7.3 Storage and transport

- `bof3x.ini`: `fkey.ACTION=KEY` (a key name as `key.` lines take, `0x1A`
  hex for an unnamed one, or `none`) and `fpad.ACTION=INPUT` (a pad input's
  name, or `none`). Per action, not `key.NAME=`: one key must be able to hold
  a game button and an action at once. The first such line starts from the
  defaults; each is applied in order; a line not understood is said on
  stderr and left out. Written - every action, key then pad - only once the
  ini had such a line or the page was changed (`Config::fkeys_named`).
- `BOF3X_FKEYS`: the same items comma-separated, sent only when the table
  differs from what the game builds without it (the defaults with
  `ruby_key=`'s key for the row above the text), so an ini that never named a
  function key sends nothing new. An environment's `BOF3X_FKEYS` wins. The
  game logs each item it does not understand (`fkeys       BOF3X_FKEYS: ... -
  left out`) and a count, and leaves the item out: as `BOF3X_KEYS`, which
  only counts, it does not stop.
- **`BOF3X_RUBY_KEY`** still moves the row above the text (f1..f12 or none;
  another value stops the game, as before). The game applies it to the
  defaults, then `BOF3X_FKEYS` over it, so `BOF3X_FKEYS` wins when both are
  set. The launcher keeps `ruby_key=` in step: when the page puts the row on
  an F key or none, `ruby_key=` says so; on another key `ruby_key=` keeps its
  value and `BOF3X_FKEYS` carries the key.
- **The harness keeps the default layout:** under `BOF3X_INPUT` or
  `BOF3X_RECORD` the game does not read `BOF3X_FKEYS` (one log line says so),
  since recipes name the default keys and the recorder uses F12.
  `BOF3X_RUBY_KEY` is read as before.

### 7.4 The page

The Controls tab's third sub-page, **Function keys**: one row an action, its
label, a Keyboard cell and a Pad cell, and a note for three rows. Click a
cell and press a key or a pad button (Escape cancels, Clear empties it), as
on the other two pages. Nothing is taken from another cell. A cell whose key
or pad input does two or more things - on any of the three pages - has red
text (`FnRedCells`; drawn by the page's custom draw with the button's theme),
redrawn at once; the line under the rows says what red means. Defaults puts
this page back to its defaults. The Settings page's Display box names the
window action's key ("Full screen (F8 changes to a window)"), or no key.

The pad inputs: SDL 3's whole button list since this page - `misc1`,
`rpaddle1`, `lpaddle1`, `rpaddle2`, `lpaddle2`, `touchpad`, `misc2`..`misc6`
appended to `PadInput` (`input/bindings.h`) - bindable on all three pages; an
old ini's names read as before. On a pad with few spare buttons most pad
bindings are double and red, by design; L2 and R2 keep their game rule
(outside the Config's permutation, ORed into Speak and confirm).

**Tests:** `tools/fkeys_host` (the table, the text forms, bad items, the
key message of every producer, the order, the red cells, the pad inputs) and
`tools/launcher_host`'s `TestFnKeys` (the ini lines, `BOF3X_FKEYS`,
`ruby_key=`).

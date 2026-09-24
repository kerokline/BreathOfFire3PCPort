# Controls: what the port reads, what the Config panel's columns mean, and a plan for modern pads

**Status:** IN PROGRESS (2026-09-24) - the read is done; step 1 of §6 is
built and confirmed in play (DIV-0050, `src/game/pad_read.cpp`), the rest is not. The owner's
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

**Still open:**

1. Pad API: XInput now, behind an interface (recommended), or SDL3 from the
   start for DualSense and the community glyph database.
2. Several pads: OR them all, or first connected.
3. Default XInput layout: Xbox positional (A cross) or a Nintendo-style
   toggle from the start.
4. The binding area's shape: one column of fourteen or two of seven.

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
   populated with the defaults (screenshot, 2026-09-24). Not yet checked in
   play: a pad map changed from the dialog.
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
4. The seventh row and the binding screen (§4.3).

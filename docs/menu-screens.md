# The field menu: where it lives and what is wrong with it

**Status:** IN PROGRESS (2026-09-20)

Started from the owner's report that the PC menu differs from the PlayStation
one in many small ways. Everything in §1 and §2 was measured on the owner's
running game with read-only sampling (`tools/mem_watch.py`,
`tools/task_stacks.py`) while the owner walked the menu and said what they
did; the log is `analysis/memwatch/menu_state.tsv` (gitignored). §3 is static
reading, and for the Config frame a read of the PlayStation side from the
owner's disc. Two fixes came of it, DIV-0010 and DIV-0011; neither has been
seen in the menu yet.

## 1. How the menu runs

- The field task's loop is `0x495800`: `call [0x656A44 + 4 * mode]`, then
  `Task_Sleep(1)`. Mode is `u16 0x66C7E8`, step `u16 0x66C7EA`. Mode 2 is the
  field, **mode 3 the menu**: step 0 `0x495BC0` loads DAT `0x31E`, step 1 is
  `0x5172F0`, step 2 `0x495C50` reloads the area's file (`0x2C2 + n`).
- Because the menu runs inside one per-frame call and returns, **a sleeping
  task's stack never shows it** - `task_stacks.py` sees only `0x495750`. The
  state bytes are what can be watched.
- `0x5172F0` = `0x56D690` (the event-script dispatcher through `0x662C80`,
  index `s8 0x8034E0` - NOT the menu; it held 5 through field and menu alike),
  then **`0x589970`, the menu**, then `0x59E230`.
- `0x589970`: `jmp [0x6672B4 + 4 * u8 0x929F00]`.

| State | Handler | Screen (owner's walk) |
|---|---|---|
| 0 | `0x589990` | set-up; runs `0x590660` per party member |
| 1 | `0x589B60` | top bar; Camp (slot 6) is handled here - step 1, counter runs below zero, mode leaves 3 |
| 2 | `0x58AAE0` | Items (step 2 top level, 3 list, 4 close) |
| 3 | `0x58D7C0` | Ability (only its exit seen: steps 9, 8) |
| 4 | `0x58C2C0` | Equipment (2 top, 3 character picker - shared by "equip" and "optimize" - 5 equip screen, 7 close) |
| 5 | `0x58F080` | Tactics (1 top, 2 formation, 3 members, 4 close) |
| 6 | `0x58A4C0` | Status (2 rest, 3 change character, 4 close; opens with a count of 8, not 5) |
| 7 | `0x5902E0` | Config: step 1 is `jmp 0x460CB0`, its own machine on `0x929F02` through `0x6536C0` (2 = running, `0x461070`) |
| 8 | `0x5902D0` | four entries, all the bare `ret` at `0x437CC0` |

State block at `0x929F00`: `+0` state, `+1` step, `+2` sub-state (Config,
Tactics), `+4` open/close countdown, `+5` the current cursor (top-bar slot,
counted from 0; Config reuses it for its row and restores it), `+6` the
character shown by Status. List cursors are elsewhere, unfound.

Since 2026-09-21 an input recipe walks all of this unattended
([`input-script.md`](input-script.md), `tools/recipes/menu_screens.txt`).
Measured on the way: the top-bar cursor `+5` survives closing and reopening
the menu, so a recipe seeks it rather than counting presses; the menu opens
on `Field_MenuButton` `0x903584` and screens are entered and left on the
save's own confirm and cancel words, `0x90358E` / `0x903590` - all three
per save (save 5: square, cross, triangle). The buttons above each screen's
panel are DIV-0018 ([`config-screen.md`](config-screen.md) §8).

## 2. Pieces named by what they were seen or read to do

- `0x575690 (u8 kind)`: the menu backdrop - SPRTs tiled 240 wide in rows 32
  apart, tile rectangles from `0x663874`, patterns from `0x663920`, kind from
  the Config "Background" byte `0x903A5B`. It writes sprite positions as
  **floats** (`fild` / `fstp` into the primitive at `+8` / `+0xC`): the PC
  team's own primitive format, not libgpu's.
- The panel and text draws sit in `0x573560`-`0x5763F0` (a dozen callers of
  `Text_DrawAt`), and the Config screen in `0x460C40`-`0x461E10`, next to
  five of HANDOFF's seven unread per-character `Text_DrawAt` callers.
- `0x590660`..`0x592400` hold no draw calls; by position and callers, stat
  code.

## 3. The defects the owner reported (2026-09-20, screenshots against the PSX)

All present in the 2001 release, so they are bugs to fix, each with a ledger
entry when fixed.

1. **Config's panel has no frame. Fixed as DIV-0011, confirmed by the owner.**
   `0x461710` opens with `0x4DF820(x, y, 0x21, 0x0D)` and the controller
   sub-panel `0x461A50` with `0x4DF820(x, y, 0x0C, 0x0F)`, and **`0x4DF820` is
   a bare `ret`** with 25 call sites of 0, 1 and 4 arguments - several empty
   functions folded into one. The PlayStation's counterpart was found by
   searching the Japanese disc for the argument pair (`addiu a2, 0x21` /
   `addiu a3, 0x0D`): `STATUS.EMI`, the same function at `0x801E2548`, calling
   **`0x801DF56C`** - five sprites under 8 x 8 texture windows and four
   corner pieces through `0x801B02A0(x, y, id, 1)`. The PC's `0x801B02A0` is
   **`Menu_DrawPiece` `0x57D860`**, and its flagged rectangle table
   `0x663C8C` still holds all nine pieces. Ours is
   `src/game/menu_frame.cpp`; the decode is its comment. The same search
   (`START.EMI` has the pair too) is the way to find the next one.
2. **The reserve list on "change party members" has no frame. Same cause,
   same fix (DIV-0011), seen drawn in the owner's session.** The PlayStation's list is `0x801EA99C(obj)`:
   the frame `0x12` by `0x15` at the object's `+4` / `+6` (it slides in from
   x `0x140` to `0x96`, `0x20` a frame), then an entry a member. The PC has
   the list twice, `0x59AA80(obj)` and `0x581300(x, y)`, each opening with
   the empty call (`0x59AA98`, `0x581313`); the entry body is `0x573560`. An
   earlier pass here missed both sites by judging argument counts from the
   instruction AFTER the call - the stack is cleaned several instructions
   later. Read the pushes before it.
3. **The screen title ("Ability") has no box and sits left of centre.**
   Unread; `0x574AB0` is called first by every Config frame and is the
   candidate.
4. **Numerals lose their bottom row** - [`known-defects.md`](known-defects.md)
   D1, now seen on every menu screen, not only Equipment. **Fixed as DIV-0010, unseen in the menu:**
   the D3D sprite handlers reach their far texture value one pixel past the
   last pixel drawn. Not a menu bug - every sprite in the game has it.
5. **Text has a glow the PlayStation's has not** (owner, same day). Two
   causes, both measured: bilinear filtering under an alpha test of "above 8
   of 255" draws a grey fringe round every glyph (DIV-0012, an opt-in point
   filter), and the PC team brightened the white text CLUT - the disc's
   shadow shade 4 became 12 (DIV-0013, restored by the English overlay).

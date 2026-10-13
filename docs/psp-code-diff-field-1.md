# The PSP code diff, stage B: field, menus, items and system, rows 1..454

**Status:** DONE (2026-10-12): stage B of group CD of the PSP-only round
([`psp-only-round.md`](psp-only-round.md) 3), share CD2. This doc reads rows
1..454 of [`psp-code-diff.md`](psp-code-diff.md) section 7's table "Field,
menus, items and system: 908 differing pairs": the named functions first, then
the rest by score. Rows 455..908 are CD3's, and the battle half is CD1's. It is
read-only: nothing in `src/` or the importer changed. The one tool added,
`tools/psp_pair_read.py`, sits beside `psp_pair.py` and does not change it.
Nothing here has been seen in play, and every claim about what the player sees
is marked as a guess.

## 0. The answer

Each of the 454 differing pairs is classified as one of five things (section 3):

| class | rows | what it means here |
|---|---|---|
| **encoding** | 264 | the same behaviour, compiled differently, or a normaliser defect (section 2) |
| **platform** | 127 | the PSP's renderer, widescreen, sound, save, file I/O, input, or its replacements for the Psy-Q library |
| **content** | **33** | a game-logic or data-use change: 11 single changes and 22 hooks of one PSP-only mode (section 1) |
| mispair | 25 | the two functions are not twins; most are a PSX extent that holds several functions (section 2.3) |
| unresolved | 5 | read, but not decided (section 3) |

- **No content change touches the priority data.** No consumer of
  `exp_table`, `RYUD`, Ryu's form data or the arenas is in these rows.
  - Three readers of the consumables table are here. They read the same
    records at the same fields on both builds: row 217 (COMMU00 `801efa50`),
    row 263 (AREA030 `801e105c`) and row 346 (START `801d31a8`).
  - Rows 37 and 346 call the item-87 test (GAME.EMI `801b0494`, PSP
    `9a80c`) with the same arguments on both sides.
  - The only text that touches button labels is row 32, the glyph renderer
    (`8014f708`, PSP `9978`). The PSP adds text control codes `0x1b..0x24`
    that draw button glyphs. That is platform input, and it is not the battle
    button-label page.
- **The PSP adds a mode of its own.** It is launched from a menu choice the PSX
  does not have (section 1.2). Twenty-two of these rows, nearly all in
  AREA030, branch on its flag. Whenever the mode is not running, each of
  them does what the PSX does.
- **Eleven single content changes**, all identical on PSP-EU:
  - two event triggers resized or re-conditioned (rows 71, 178);
  - a party placement added for map 92 (row 41);
  - a tint overlay skipped at one spot of area 70 (row 119);
  - a frame counter that now saturates (row 312);
  - a shop prompt simplified (row 231);
  - five drawing changes: a logo fade (row 100), a wider caption graphic (rows
    108 and 253), uncoloured status abbreviations (row 352), and a rebuilt
    scenario effect (row 372).
- **The normaliser has three defects.** Section 2 measures them: a jal's delay
  slot, overlay calls into GAME.EMI resolved to the boot image's zeros, and
  .bss "functions" of the boot image that took a twin by caller vote.
  Correcting the first two takes 58 of these 454 rows out of `differs` (209 of
  the 1,225 in both halves); all 58 read as encoding or platform.

## 1. The content changes

Evidence gives instruction addresses: PSX-JP addresses, then the PSP-JP twin's.
"EU" is the PSP-EU twin compared with PSP-JP by relocation-masked words
(`eucheck`, section 4): 0 differing words means the same code on both PSP
releases. Suites follow [`psp-only-round.md`](psp-only-round.md): graphics,
audio, data.

### 1.1 Single changes

| row | PSX (file) | PSP-JP / EU | what changes | suite | evidence (PSX / PSP) |
|---|---|---|---|---|---|
| 41 | `801de8fc` SHOP#0 | `1fb418` / `1fb154` (EU same) | After its map-0x85 case, the PSP adds a case for map id 92 (`0x5c`). It puts every party member at a fixed position (`0x190000`, `0x3c0000`), takes the height from `80155238`'s twin, and recentres the camera through `801548a4`'s twin. The PSX has no map-92 case. Guess: where the party stands on leaving a shop into map 92 was fixed. | data | `801dec08..801decd8` / `1fb70c..1fb790` |
| 71 | `801fe2a0` SCENA08#0 | `29533c` / `295178` (EU same) | A trigger in area `0x2b`, gated on flag `0x14` set, flag `0x15` clear and x in `0x20..0x25`. The PSX needs y ≥ `0xF0000`, the PSP y ≥ `0xE0000` (both test the second argument). The zone is one tile deeper on the PSP. Guess: the event fires a tile earlier. | data | `801fe628..801fe634` (`lui 0xe; ori 0xffff; slt`) / `295784..295790` (`lui 0xe; slt`) |
| 178 | `801fd67c` SCENA07#0 | `291a1c` / `291858` (EU same) | Area `0x69`'s trigger. The PSX fires (`8015ca78`, then `Msg_OpenScript(1)`) when x is 7..11 and y is 28..30. The PSP fires when a state byte is not 0, 1 or 2 and y ≥ 30, and ignores x. Guess: the event's zone moved, or it was made conditional. | data | `801fd854..801fd884` / `291c5c..291c90`, `291d4c` |
| 119 | `80159108` SLPS (the field tint overlay) | `115ec` / `11480` (EU same) | The full-frame quad is widened to `-32..352`, which is widescreen. The PSP also skips the overlay when the area id is 70 and the two party-position halfwords are in 40..43 and 24..27. Guess: a fix for one spot of one area. | graphics | `80159190` / `11678..116c8` |
| 312 | `801994e8` GAME#0 | `7f08` / `887c` (EU same) | The frame counter at `0x80145038` goes up on every call on the PSX. The PSP stops it at 999,999,999 (`0x3b9ac9ff`). Guess: a play-time count capped for display, reached after about 193 days at 60 Hz. | data | `801994f8..80199508` / `7f14..7f30` |
| 231 | `801d47bc` SHOP#0 | `1f28dc` / `1f25c4` (EU same) | The shop's confirm prompt. The PSX opens system message `0x85 +` the byte at `801e6020` and handles yes or no in place. The PSP always opens message `0x9d` (`0x85 + 0x18`) and hands input to the PSP-only `1f2350`. It is the same prompt only if that byte is always `0x18`, which was not established (low confidence). | data | `801d47f8`, `801d4858` / `1f28fc`, `1f2918` |
| 100 | `801d1a88` GAME#1 | `6220` / `6b94` (EU same) | A logo fade step. The PSX fades `0x80143c26` by 4 and `0x80143c28` by 2 per frame; the PSP fades both by 2. Two draw positions move: `(26,24)` to `(61,31)` and `(-6,28)` to `(-9,18)`. Guess: the PSP's logo art differs. | graphics | `801d1ac0`, `801d1b8c`, `801d1ba4` / `6254`, `62f0`, `6308` |
| 108, 253 | `801f9238`, `801f91a8` SCENA17#0 | `2b85f8` / `2b8448`, `2b856c` / `2b83bc` (EU same) | One graphic, drawn for the text code `@`, is 88 px wide on the PSX and 105 on the PSP (height 18). Row 253 draws it; row 108 advances x past it by the same 88 or 105. Guess: a wider caption image redrawn for the PSP. | graphics | `801f91f8` / `2b85c0`; `801f9348` / `2b8708` |
| 352 | `801e2f78` START#8 | `c0d98` / `c0b8c` (EU same) | The status flags' text. The PSX draws flags `0x20/0x40/0x10/0x80` in colours 2/1/6/5 and gives flag `0x08` a second glyph at `(x+12, y+4)`. The PSP draws them all in colour 0, moves flags `0x04/0x08` to x+5, and drops the second glyph. Guess: the status abbreviations lose their colours. | graphics | `801e3024`, `801e3050`, `801e307c`, `801e30a4`, `801e3134` / `c0df4`, `c0e2c`, `c0eb0`, `c0f00` |
| 372 | `801fa38c` SCENA07#0 | `3a08c` / `39e60` (EU same) | The PSX transforms a 482-vertex mesh and draws 512 gouraud quads, coloured from two tables. The PSP draws a 63-quad strip with a colour that fades down from `0x40`. Guess: a scenario effect rebuilt or simplified (low: it could be a mispair, though the opening calls match). | graphics | `801fa43c`, `801fa5a0..801fa628` / `3a104`, `3a1b4..3a200` |

### 1.2 The PSP-only mode and its 22 hooks

- **The flag.** PSP `0x2100fc` returns the byte at .bss raw `0x11d48c`.
  - It is set to 1 in exactly one place, `0x20fce4`.
  - It is cleared at `0x20fec0` and `0x20ff44`.
  - Measured with `psp_elf.py <JP BOOT.BIN> refs 0x11d48c`.
- **The setter.** `0x20fc80` is a main loop of its own. It initialises a
  24-entry table of `0x24`-byte records (seeded with 999, 9999 and 99), sets
  the flag, and while the flag stays set it calls the handler in `[0x11d490]`,
  then `0x2109b4`, then `0xdec(1)`.
- **The launch.** `0x20fc80` is installed as the program by `0xec8`, the call
  `Main_FrameLoop`'s twin uses to install `Boot_Init`'s. The install is at
  `0xc2330`, case 2 of a 5-way menu switch at `0xc22c0`, in START-region code
  that no PSX function pairs with.
- **The code region.** The code at `0x20f000`.. is PSP-only: the last paired
  function before it is SHISU's.
- **The second test.** `0x210108` and `0x210158` test the word at
  `0x11b310 + 0x10`, a block that the module's `0x20fe70` clears (`0x3bc`
  bytes) when it starts.
- **A second PSP-only flag.** `0x21f418` (raw `0xc4478`) is read in row 125.
  CD3 reports that it is set by `0x20f4d4`; this doc did not trace it.

That the mode runs inside AREA030 is a fact of the code: 19 of the 22 rows are
AREA030 functions. AREA030 is the fishing area, by the sibling's
`names/places.toml`. **What the mode is in play is a guess**: a PSP-only
fishing mode with its own records table, entered from a menu.

Every row below behaves as the PSX does whenever the flag is clear. Suite:
data (a new mode).

| row | PSX (file) | PSP-JP | while the mode runs |
|---|---|---|---|
| 33 | `801ded34` AREA030#4 | `e6ac4` | clears actor `+0xb`, sets a flag byte, skips the step (also gated on `210158`) |
| 92 | `801d2dc0` AREA030#4 | `1e4d0` | alternate draw positions, 36 px further left |
| 109 | `801daa5c` AREA030#4 | `e479c` | an extra exit trigger |
| 110 | `801dc9bc` AREA030#4 | `e4fdc` | the PSP-only `e5384` (324 instructions) replaces the PSX input handler |
| 125 | `801a0aa8` GAME#0 | `8538c` | (this flag or the second one) skips setting `0x8014686e = 1` and the call to `801a8834` |
| 135 | `801da320` AREA030#4 | `e4590` | its own state branch: calls the PSP-only `21cd0c` and `21cad8`, sets field 3 to `0xe` |
| 191 | `801d3820` AREA030#4 | `19e98` | window at x -16, text at x -7; value 10 calls `210e44` |
| 192 | `801d7c58` AREA030#4 | `1daa8` | columns 44 px left, plus a fourth column |
| 194, 195 | `801dce38`, `801dd6bc` AREA030#4 | `e16fc`, `e1954` | text at x -7; `210158` can force the cancel or exit path |
| 257 | `801d3f54` AREA030#4 | `1a6ac` | a slide threshold goes from 29 to -7, its reset from 39 to 3 |
| 259 | `801da0d8` AREA030#4 | `df364` | skips a window animation, registers `21d260` |
| 260 | `801db1c0` AREA030#4 | `e4bdc` | an extra path (`e32cc`, `2101e4`) |
| 261, 262 | `801dcd74`, `801dd824` AREA030#4 | `e15ec`, `e1ad0` | window x from 29 to -7 (262 adds an input gate on `210158`) |
| 395 | `801d3a50` AREA030#4 | `1a20c` | window x from `0x125` to `0x101` |
| 396, 397, 398 | `801d469c`, `801d5064`, `801d5590` AREA030#4 | `1b178`, `1bbe0`, `1c108` | a fourth menu row (cursor 3 sets state `0x11`; the PSX handles cursors 1 and 2), and window x 36 px left. The cursor-3 case is not itself gated, so it is reachable only when the menu offers a fourth row |
| 399 | `801d9d24` AREA030#4 | `de418` | stores two PSP-only getters' results to a PSP-only struct and sets its flag |
| 400 | `801d9ec8` AREA030#4 | `df08c` | with `210158`: an early exit that sets `0x80144121 = 7` without reading the pad |
| 402 | `801dd9f8` AREA030#4 | `e1ce4` | with `210158`: takes the pad-`0x40` path as if the button were pressed |

### 1.3 Platform rows a reader may still care about

These are classed platform, but they change what the game does, not only how
it is drawn:

- **Row 18, `Rand`** (`8017ed4c`, PSP `a2cf0`). The PSX calls the BIOS
  `rand`. The PSP inlines an LCG (`seed * 0x343fd + 0x269ec3`, `>> 16 & 0x7fff`).
  So **the random sequence differs**, and any route or test that depends on
  the PSX's random draws will not replay on the PSP.
- **Row 160, `Field_EncounterDue`** (GAME `801b4ea8`, PSP `cd080`). The PSP
  drops the PSX's check that postpones an encounter while the VSync count at
  `0x80143efc` is above `0xf0`. The 7/10 step is the same on both. This
  agrees with [`psp-stallion.md`](psp-stallion.md) 5.3.
- **Row 29, the options menu** (START `801e1ac0`, PSP `c12b4`). The PSX row
  that toggles `0x8014494d` and calls two sound-library functions (a
  sound-output option, a guess) becomes a row that opens the PSP's key
  configuration. Help messages from row 5 on shift by one.
- **Row 331** (`801d1688`, PSP `5f20`). The PSX's second sprite draw at
  `(x+240, y+112)` is absent. Guess: the PSP draws the image in one piece.
  Classed platform at medium confidence; it could be content.

## 2. What the normaliser still holds (for CD1, CD3 and the next run)

### 2.1 Two defects, measured

```
python tools/psp_pair_read.py rescore --psp-jp <JP iso> --psp-eu <EU iso> --diff <scratch>/diff.json --out <scratch>/rescore.json
```

The command rebuilds `psp_pair.py`'s pairing (the same pairs; with no fix, it
reproduces stage A's score on all 1,225 rows). It then rescores each row with
these corrections:

- **`ds`, the delay slot.** `extract` clobbers a jal's caller-saved registers
  before it reads the delay slot. An address half formed in the slot (PSP
  `addiu` with a LO16 relocation, PSX `addiu off(base)`) is then counted as a
  value: `Main_FrameLoop`'s `0x4e1c` is `Boot_Init`'s twin's address. CD1
  reported the same.
- **`res`, the resident module.** `Psx.resolve` tries the boot range
  (`0x8011cd40..0x801f3004`) before the resident module. The boot image is
  zero from `0x801a0000` on (81,917 of 81,920 words), so every overlay call
  into GAME.EMI (`0x80196800..0x801ce0e4`) becomes an unpaired `('boot', addr)`
  callee. The PSP twin's call to the paired GAME.EMI twin then looks added. The
  item-87 test is the clearest case: START `801d31a8` calls `801b0494` on the
  PSX, and its twin calls `9a80c`.

| rows | `differs` | leave `differs` with `ds` | with `res` | with both |
|---|---|---|---|---|
| field 1..454 (this doc) | 454 | 25 | 41 | **58** |
| field 455..908 (CD3) | 454 | 67 | 56 | 112 |
| battle (CD1) | 317 | 17 | 21 | 39 |

The readers classed all 58 of this doc's rows as encoding (52) or platform (6).
Section 4's table gives each row's score with both corrections.

### 2.2 The boot image's .bss as functions

Six rows here have a PSX key in the boot image at a GAME.EMI or overlay
address: rows 60, 86, 154, 155, 156 and 299. Each has 0 instructions, and its
PSP twin came by caller vote. The real PSX function is the overlay's at the
same address, and `res` would have pointed the votes there. Diffed against
it:

```
python tools/psp_pair_read.py phantom --psp-jp <JP iso> --psp-eu <EU iso> 801b1e3c:cbc68 801af3f0:9ac74 801b0390:9a70c 801bf1a8:d856c 801f03c0:107c88 801af214:99f00
```

| row | real PSX function | vs the PSP twin |
|---|---|---|
| 60 | GAME `801b1e3c` (298 instructions) | score 0 (verdict platform) |
| 86 | GAME `801af3f0` | score 1. The pairing gives this function `212268` instead, which looks wrong |
| 154 | GAME `801b0390` | score 1 |
| 155 | GAME `801bf1a8` | score 1 (the `0x140` stride) |
| 156 | COMMU00 `801f03c0` | score 4, not read |
| 299 | GAME `801af214` | score 0 |

Across both halves, 26 pairs have such a key (`diff.json`, PSX key `boot:` at
`0x80196800` or above).

### 2.3 Function extents and pairs found wrong

**Several functions inside one PSX extent.** Stage A section 3 warned about
this: a jump table whose cases run past a return with no prologue after it
merges two or more functions. The PSP twin then matches only one of them.
Found in rows 29, 30, 33, 34, 36, 43, 44, 57, 72, 80, 81, 106, 133, 134, 175,
225, 239, 240, 241 and 316. Where the readers found the right part:

- row 43: PSX `801f7f08`
- row 44: PSX `801f9834`
- row 57: PSX `801f98a4`
- row 72: PSX `801fcd80`
- row 80: PSX `801fc45c`
- row 81: PSX `801f8ce4`
- row 106: PSX `801fa98c`
- row 133: PSX `801f87c4`, and `801f8564`'s twin is `284120`
- row 240: PSX `801f8eb0`
- row 241: PSX `801fb914`

Two PSX keys are data tables (rows 114, 143). Two start mid-function (rows 77,
139), and row 175's starts at a branch target (the function starts at
`801f71fc`).

**Mispairs.** The rest of the 25:

- **Library twins:**
  - `SetDrawMode` with `9c7b0`, which is the PSP's float `rsin`, `rcos`'s
    sibling (row 16);
  - `get_alarm` with `a2d60`, an allocate-and-copy routine (row 17);
  - `printf`'s twin `2c575c` is used as `memcpy(dst, src, 32)`;
  - `GetGraphType`'s twin `9fd54` receives `SetDrawMode`'s arguments, so it
    is probably that function's replacement.
- **Swapped pairs:**
  - GAME `801ba5d8` / `801bb670` with `d2a48` / `d15d0` (rows 40, 46);
  - GAME `801a2c64` / `801a2ee0` with `88704` / `882d8` (row 314; CD3 row 518
    has the same);
  - AREA037 `801f3508` and AREA068 `801f3930` (rows 408, 419). Their true
    twins are `228b60` and `228b08`, both identical, so gap alignment shifted
    by one. **No content difference there**: the 3-to-6 / message-96
    difference CD3 relayed is this shift. `801f3930` stores 3 and `0xffff` on
    both builds (`psp_elf.py dis 0x228af0 0x228b60`).
- **Probable pairing errors:**
  - boot `8015df44` is probably the true twin of `952f0`, which is paired with
    `8015de28`;
  - boot `80167628`, the tile-attribute getter, is unpaired, while its twin
    `dce04` is paired with the setter GAME `801a5bc8`;
  - GAME `8019701c` and `80197070` are unpaired, but they sit at the call sites
    of `3100` and `3154`, which are `0x54` apart as the PSX pair is.
- **Others:** rows 35, 55, 128, 207 (the exec routine with a pad reader), 444
  and 451. No twin was found.

## 3. Method

1. Regenerate stage A's data:

   ```
   python tools/psp_pair.py diff --psp-jp <JP iso> --psp-eu <EU iso> --out <scratch>/diff.json --md <scratch>/table.md
   ```

   The 908 field rows of the result are byte-identical to the committed
   table (`diff`).
2. Dump every row's annotated disassembly and the aligned diff of the two
   functions' call and constant streams:

   ```
   python tools/psp_pair_read.py dump --psp-jp <JP iso> --psp-eu <EU iso> --diff <scratch>/diff.json --half field --rows 1-454 --out <scratch>/rows/
   ```

   The aligned diff sets address halves aside and processes delay slots
   first.
3. Ten readers each read a contiguous share of rows against one brief: the
   classes, the encoding tags, the bias towards encoding, and the rule that
   content needs an instruction pair that behaves differently. Two notes went
   out mid-read: the `res` defect, with a map of every GAME.EMI and BATTLE.EMI
   function to its PSP twin, and the PSP-only mode.
4. This doc's author:
   - read every content row's two listings;
   - checked rows 41, 71, 119, 178, 312, 408 and 419 instruction by instruction;
   - found the mode's setter and launch with `psp_elf.py refs` / `dis` on the
     JP `BOOT.BIN` (extracted to scratch);
   - re-diffed the six boot-.bss rows (`phantom`);
   - compared each content row's PSP-JP and PSP-EU twins by relocation-masked
     words (`eucheck`, a scratch script over `psp_pair.Psp`).

**Tags.** encoding:

| tag | rows | what |
|---|---|---|
| `E-fold` | 85 | a constant folded or split differently; a range test as `(x-K) < W` against two signed bounds; a structure offset added to a base |
| `E-resolve` | 60 | section 2.1 `res` |
| `E-switch` | 28 | a compare chain against a jump table |
| `E-other` | 28 | |
| `E-div` | 16 | a reciprocal against `div` |
| `E-inline` | 13 | |
| `E-loop` | 12 | a rescaled induction variable |
| `E-bits` | 10 | |
| `E-ds` | 7 | |
| `E-phantom` | 5 | section 2.2 |

platform:

| tag | rows | what |
|---|---|---|
| `P-gpu` | 79 | mostly three changes: the `GetGraphType` texture-page branch dropped, the draw-mode packet grown from 12 to 16 bytes, and the CLUT and texture-page words re-encoded |
| `P-ws` | 18 | among them the field tile grid, which grows from 28x56 to 33x58 cells (66-byte rows) in about ten rows, and the map renderer's set-up (row 53) |
| `P-lib` | 12 | |
| `P-other` | 6 | |
| `P-save` | 4 | |
| `P-sound` | 4 | |
| `P-file` | 3 | |
| `P-input` | 1 | |

**Unresolved (5):**

- Row 65: the PSX extent stops at its jump table.
- Row 102 (START): PSP calls `2167b8` / `2167ec` where the PSX slides a panel.
- Row 132 (SCENA01): the PSP twin has branches for areas 0, 5 and 8 that the
  PSX function lacks; perhaps merged from another scenario function.
- Row 156: section 2.2.
- Row 353 (START): the PSP always sets menu state 4. Perhaps the same menu
  that launches the PSP-only mode.

## 4. Per-row classification

Columns: the row number in `psp-code-diff.md`'s field table, the PSX function
(the sibling's name where it has one), the PSP-JP twin, stage A's score (in
brackets, with both section 2.1 corrections), the class and tag, the reader's
confidence, a note, and the instruction addresses the note rests on. Notes are
cut at 260 characters.

| # | PSX (file) | PSP-JP | score (fixed) | class | tag | conf. | note | evidence |
|---|---|---|---|---|---|---|---|---|
| 1 | `8015096c` SLPS_009.90 MsgBox_Step | `b78c` | 16 (16) | encoding | E-switch | high | MsgBox_Step: PSX opcode compare chain vs PSP jump table bounded at 30; message-table base 0x80010000/+4 becomes base+0x10000/+0x10004 (E-fold); every PSX case (state 6 via 164-byte char record, state 0x11 via offset table, 32-byte table) is present on PSP w... | psx 801509f8 / psp b864; psx 80150cdc / psp ba68; psx 80150df4 / psp bbd4; psx 80150c70 / psp ba1c |
| 2 | `8014e974` SLPS_009.90 Boot_Init | `4e1c` | 10 (10) | platform | P-other | medium | Boot_Init: PSX inline memset of 0x10b0 bytes becomes a memset call of 0x30b0 (larger PSP state block), Ss reverb/volume calls replaced by PSP sound init, file-load wait loop goes through the PSP platform layer | psx 8014e9f8 / psp 4ed8; psx 8014e9c8 / psp 4ea8; psx 8014e990 / psp 4e44 |
| 3 | `8014c3c8` SLPS_009.90 Actor_Task | `1c20` | 9 (9) | platform | P-gpu | medium | Actor_Task: offscreen render target moves (PSX DrawArea x+0x340 and y+16/+256 double-buffer vs PSP x+0x120 only), tpage bits built with 0x3ff masks and SetSemiTrans; the screen culls are the same range ([-64,384] x [-64,304]) written as two signed compares | psx 8014c5b0 / psp 1ddc; psx 8014c4e0 / psp 1d08; psx 8014cc04 / psp 2320 |
| 4 | `8014aac8` SLPS_009.90 Main_FrameLoop | `280` | 8 (6) | platform | P-other | high | Main_FrameLoop: PSX DrawSyncCallback/DrawSync/__main and SE key polling replaced by PSP platform frame calls (vsync, display flip, input poll); game-task calls unchanged | psx 8014aaf8 / psp 2a4; psx 8014abd8 / psp 374; psx 8014ab68 / psp 310 |
| 5 | `80178d40` SLPS_009.90 rcos | `9c760` | 7 (7) | platform | P-lib | high | rcos: Psy-Q 4096-entry table lookup replaced by float cosf(x*2pi/4096)*4096 on PSP; twin plausible (same role, rsin sibling at 9c7b0) | psx 80178d4c / psp 9c76c |
| 6 | `801ef96c` ETC/SHOP.EMI#8 Card_Open (label) | `98cf8` | 6 (6) | platform | P-save | high | Card_Open (label): memory-card device name 'bu00:'/'bu10:' built inline from immediates instead of copied from .data, and Psy-Q open() replaced by a PSP savedata/file call; memory-card function | psx 801efa10 / psp 98d88; psx 801ef9c0 / psp 98d4c |
| 7 | `8017aab0` SLPS_009.90 ratan2 | `9c864` | 5 (5) | platform | P-lib | high | ratan2: Psy-Q integer table atan2 replaced by soft-float double atan2 calls on PSP; twin plausible | psx 8017aaf0 / psp 9c878 |
| 8 | `8014af98` SLPS_009.90 Packet_FrameReset | `778` | 4 (4) | platform | P-gpu | medium | Packet_FrameReset: packet/OT list count 56 -> 58 (two more ordering layers) and per-frame packet buffer at base+0x18000+frame*0x9000 on the PSP renderer | psx 8014b038 / psp 810; psx 8014b044 / psp 780 |
| 9 | `8015ad34` SLPS_009.90 Text_DrawImmediate | `14848` | 4 (4) | encoding | E-switch | high | Text_DrawImmediate: same 10-case jump table and same control codes (0x12,0x13,0x15 consume a byte on both: PSX (c-18)<2 or c==21); message base 0x80010000/+4 becomes base+0x10000/+0x10004 | psx 8015aec4 / psp 14a74; psx 8015ae70 / psp 149fc |
| 10 | `80165434` SLPS_009.90 Char_RecalcStats | `b9cb8` | 4 (4) | encoding | E-div | high | Char_RecalcStats: divide by 10 as reciprocal 0x66666667 vs div; 32-byte lwl/lwr block copy vs lhu/sh; tail-merged call sites duplicated; same callees and offsets | psx 801654fc / psp b9da0; psx 8016544c / psp b9cc8 |
| 11 | `8017917c` SLPS_009.90 VectorNormalS | `9db74` | 4 (4) | platform | P-lib | high | VectorNormalS: GTE-based Psy-Q routine replaced by soft-float sqrt/divide calls; twin plausible | psx 8017917c / psp 9dbd4 |
| 12 | `80151f4c` SLPS_009.90 Font_MapGlyph | `bfd0` | 3 (3) | platform | P-gpu | medium | Font_MapGlyph: Packet_Commit size 12 -> 16 and an added GetTPage/GetGraphType call for the PSP renderer; remaining differences are encoding (compare-chain order, /12 of (x<<4)+192 vs (x+12)<<4) | psx 80151f9c / psp c034; psx 80152c98 / psp c7e4 |
| 13 | `8015e908` SLPS_009.90 SE_Play | `95b30` | 3 (3) | platform | P-sound | high | SE_Play: 639-instruction SPU voice driver replaced by an 8-instruction wrapper into the PSP sound layer; twin plausible | psx 8015e908 / psp 95b3c |
| 14 | `801794f0` SLPS_009.90 PushMatrix | `9d460` | 3 (1) | platform | P-lib | high | PushMatrix: GTE matrix-stack depth check (640) replaced by a software stack (32-byte copy, pointer += 0x20); note: PSP 2c575c is paired to printf but is used as memcpy(dst,src,32) here | psx 801794fc / psp 9d47c |
| 15 | `8017a0b0` SLPS_009.90 RotMatrix | `9cdb0` | 3 (0) | platform | P-lib | high | RotMatrix: Psy-Q sin/cos table lookups replaced by calls to the PSP rcos (9c760) and rsin (9c7b0) plus matrix multiplies; twin plausible | psx 8017a0d8 / psp 9cdec |
| 16 | `8017cc50` SLPS_009.90 SetDrawMode | `9c7b0` | 3 (3) | mispair |  | high | SetDrawMode vs PSP 9c7b0: the PSP function is float sin (rsin, the twin of rcos at 9c760: x*2pi/4096, sinf, *4096), not a draw-mode packet setter; true twin not found (look in the PSP GPU platform layer near 9fc9c..9fdac) | psx 8017cc78 / psp 9c7d8 |
| 17 | `8017e6a4` SLPS_009.90 get_alarm | `a2d60` | 3 (3) | mispair |  | high | get_alarm (VSync watchdog with printf/SetIntrMask) vs PSP a2d60 (table-indexed allocate-and-copy of two buffers with free); different purpose; true twin not found | psx 8017e6ac / psp a2d94 |
| 18 | `8017ed4c` SLPS_009.90 Rand | `a2cf0` | 3 (3) | platform | P-lib | high | Rand: PSX BIOS A0 jump stub vs PSP inline LCG (seed*0x343fd+0x269ec3, >>16 &0x7fff); twin plausible; the random sequence differs from PSX | psx 8017ed4c / psp a2cfc |
| 19 | `801c0114` ETC/GAME.EMI#0 Encounter_PlaceParty | `d8d68` | 2 (2) | encoding | E-fold | high | Encounter_PlaceParty: the lost 320 is the 0x140 actor stride kept as an offset on PSX and as a pointer on PSP (stride present on both); 0x8002d600/0x80031600 as base+offset; not widescreen | psx 801c03b8 / psp d9018; psx 801c01c0 / psp d8e78 |
| 20 | `801c23f8` ETC/GAME.EMI#0 Battle_InitPartyContexts | `d7c20` | 2 (2) | encoding | E-loop | high | Battle_InitPartyContexts: 164-byte record copy unrolled by 16 on PSX vs 20x8-byte loop on PSP (index*164 both), 0x10000 hoisted to a register, 0x140 actor stride on both; not widescreen | psx 801c2520 / psp d7d50; psx 801c24d4 / psp d7cd4 |
| 21 | `8014d184` SLPS_009.90 Actor_UpdateScreenPos | `2450` | 2 (2) | platform | P-gpu | medium | Actor_UpdateScreenPos: OT-layer bound 55 -> 57 matches the PSP's 58 layers (row 8); the rest is the same depth computation inlined | psx 8014d274 / psp 24f8 |
| 22 | `8016c4e4` SLPS_009.90 SsSepReplay | `9e628` | 2 (2) | platform | P-sound | high | SsSepReplay: Psy-Q _SsSndSetReplayMode replaced by PSP sequence state handling (seq*0x58c record, PSP OS call); twin plausible | psx 8016c4f8 / psp 9e640 |
| 23 | `8017b420` SLPS_009.90 SetLineF3 | `9fcd8` | 2 (2) | platform | P-lib | high | SetLineF3: PSP setter writes only the code byte 0x48 and drops the PSX tag length 5 and the 0x55555555 terminator word; twin plausible | psx 8017b428 / psp 9fcd8 |
| 24 | `8017b460` SLPS_009.90 SetLineF4 | `9fcf0` | 2 (2) | platform | P-lib | high | SetLineF4: same as SetLineF3 with code 0x4c, PSX length 6 dropped; twin plausible | psx 8017b468 / psp 9fcf0 |
| 25 | `8017ecdc` SLPS_009.90 strcat | `2c57d0` | 2 (2) | platform | P-lib | high | strcat: PSX BIOS A0 0x15 stub vs PSP compiled strcat (22 instructions); twin plausible | psx 8017ecdc / psp 2c57d0 |
| 26 | `8017ed1c` SLPS_009.90 strncpy | `2c58f8` | 2 (2) | platform | P-lib | high | strncpy: PSX BIOS A0 0x1a stub vs PSP compiled strncpy (24 instructions); twin plausible | psx 8017ed1c / psp 2c58f8 |
| 27 | `8017ed2c` SLPS_009.90 strlen | `2c5870` | 2 (2) | platform | P-lib | high | strlen: PSX BIOS A0 0x1b stub vs PSP compiled strlen (9 instructions); twin plausible | psx 8017ed2c / psp 2c5870 |
| 28 | `8017ed6c` SLPS_009.90 sprintf | `2c54c4` | 2 (2) | platform | P-lib | high | sprintf: PSX BIOS stub vs PSP newlib-style sprintf (FILE-on-stack with flags 0x208, size 0x7fffffff, calls the vfprintf core); twin plausible | psx 8017ed6c / psp 2c54e0 |
| 29 | `801e1ac0` ETC/START.EMI#8 | `c12b4` | 23 (23) | platform | P-other | medium | Options menu (START): the PSX row that toggles 0x8014494d and calls two sound-library functions (a sound-output option, a guess) is replaced by a row that opens the PSP key configuration (state 5, SE 0x104, call 99274); help text ids from row 5 on shift by... | psx 801e1d24 / psp c1510, c158c |
| 30 | `801f9f18` SCENARIO/SCENA13.EMI#0 | `2ab8b4` | 23 (31) | encoding | E-other | high | Sibling boundary: the PSX range holds two functions; the first (801f9f18..801fa840) has the same call stream and constants as PSP 2ab8b4 (only sign/andi forms and address halves differ); the second (801fa844..) is a separate function whose twin is elsewhere... | psx 801fa83c / psp 2ac024; psx 801fa644 / psp 2abe64 |
| 31 | `801a9b84` ETC/GAME.EMI#0 | `8d788` | 22 (22) | encoding | E-switch | high | Area dispatch: same areas (0x28,0x30,0x6f,0x94,0xa7,0xa9,0xab,0xad) and same handlers; compare tree vs range checks plus jump table, PSP passes a0/a1 through; handlers are overlay-resident on PSX (unresolved labels) | psx 801a9bdc / psp 8d868; psx 801a9c24 / psp 8d7c8 |
| 32 | `8014f708` SLPS_009.90 | `9978` | 21 (22) | platform | P-input | medium | Glyph/text renderer: PSP extends the control-code table to 0x25 (jump table) adding codes 0x1b..0x24 that draw button glyphs (0xaf..0xb2, 0x66, 0xd8) and an extra glyph drawer (942c); PSX handles 0..7, 0x12..0x15, 0xff; the rest is encoding (div by 21 as re... | psx 8014f7a0 / psp 9a40; psp 9dfc; psp 9d08; psp 9db4 |
| 33 | `801ded34` WORLD00/AREA030.EMI#4 | `e6ac4` | 20 (20) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 2100fc, with 210158): while that mode runs, the PSP clears actor+0xb, sets a flag byte and skips the step; the PSX path otherwise. What the mode is in play is a guess (it runs in AREA030, the fishing area) | psp e6cf0..e6d1c (no PSX counterpart before 801df5e0) |
| 34 | `801a8cf8` ETC/GAME.EMI#0 | `8db10` | 19 (19) | encoding | E-other | high | Sibling boundary: the PSX range holds three functions; the third (801a9030..801a92fc) has the same call stream and constants as PSP 8db10; the first two are separate functions | psx 801a9030 / psp 8db10; psx 801a92f8 / psp 8ddd0 |
| 35 | `801f7db4` SCENARIO/SCENA09.EMI#0 | `2965a8` | 19 (19) | mispair |  | medium | SCENA09 event: PSX tests area 0x45 then 0x31 with flags 0/2/3 and a 3-case switch; PSP 2965a8 tests area 0x25/0x27 with flags 0x2e/0x2f/0x30 and a 12-case switch; same callee kinds (801c2e34/2de8/7014 are GAME.EMI, E-resolve) but a different scene; true twi... | psx 801f7dbc / psp 2965c4; psx 801f7e88 / psp 296798 |
| 36 | `801fa1f0` SCENARIO/SCENA09.EMI#0 | `298d6c` | 18 (33) | encoding | E-switch | medium | SCENA09 event state machine (PSX range holds 5 functions; twin is the last, 801fae0c..801fbc70, 0x97-case jump table): case bodies reordered and shared tails (j 801fb8b8 etc.) duplicated on PSP, so call positions shift; constants differ only in address halv... | psx 801fae1c / psp 298d6c; psx 801faec0 / psp 298e24 |
| 37 | `801dd52c` ETC/SHOP.EMI#0 | `1f9eac` | 17 (7) | encoding | E-resolve | medium | SHOP.EMI list draw: missing PSX calls are GAME.EMI functions mislabelled (801b0494->9a80c, 801b07f8->9a940, 801b02a0->9a620, 801b0d08->9ab6c, 801b0a14->9b770); one call site duplicated from a tail merge; PRIORITY: calls the item-87 test (801b0494 / 9a80c) w... | psx 801dd5e8 / psp 1f9fe4; psx 801dd7fc / psp 1fa1fc |
| 38 | `801f3800` WORLD04/AREA189.EMI#13 | `7d3ec` | 17 (18) | platform | P-ws | high | AREA189 overlay effect: full-screen quads move from x 0..320 to x -32..352 (and offsets -0x20/-0x120), the PSX double GetGraphType check collapses to one call; widescreen | psx 801f38c0 / psp 7d474; psx 801f3d6c / psp 7d4d0; psp 7d788 |
| 39 | `801d7090` ETC/SHOP.EMI#0 | `1f3a3c` | 16 (14) | platform | P-save | medium | SHOP.EMI save: PSX builds the memory-card header string (date digits /10, sprintf); PSP twin is a savedata dialog step (PLAT calls 2162c4/991a0/99274, sprintf of a different format); the PSX role is replaced by the PSP save flow | psx 801d7200 / psp 1f3acc; psp 1f3ab4 |
| 40 | `801ba5d8` ETC/GAME.EMI#0 | `d2a48` | 15 (15) | mispair |  | low | GAME.EMI state handler vs PSP d2a48: PSP calls 801bb270, 801bccc4, 801bc1b0, 801bc864, 801bc9ac (never called by the PSX function) and lacks its 801bcbc0 and 80155a34 calls; likely the twin of a neighbouring PSX state handler; true twin not found | psx 801ba690 / psp d2b8c; psx 801ba90c / psp d2bd0 |
| 41 | `801de8fc` ETC/SHOP.EMI#0 | `1fb418` | 15 (11) | content |  | medium | PSP adds a block after the map-0x85 handling: when the map id (PSX 0x80143f00) is 0x5c (92) every party member is placed at fixed x=0x190000 y=0x3c0000 with the height from 80155238 (PSP f5c0) and the camera is recentred via 801548a4; PSX has no map-92 case... | psx 801dec08..801decd8 / psp 1fb70c..1fb790 (psp 1fb70c ori 0x5c, 1fb734 lui 0x19, 1fb738 lui 0x3c, 1fb748 jal f5c0) |
| 42 | `801e100c` ETC/SHOP.EMI#0 | `1fd418` | 15 (7) | encoding | E-fold | high | Shop window frame/tile drawing: (i+5)*8 and (i+11)*8 folded to i*8+40/+88, base+0xb1a field read as +0x7a on another base, PSX LIB tile/box drawers are the PSP 9a620/9aa74/9ab6c/9b770 twins (normaliser PLAT), loop shapes differ only. | psx 801e1348 / psp 1fd754; psx 801e1070 / psp 1fd48c; psx 801e15bc / psp 1fd9b8 |
| 43 | `801f7cc8` SCENARIO/SCENA11.EMI#0 | `2a2d14` | 15 (13) | mispair |  | high | PSX range 801f7cc8..801f82d8 is two functions (switch on 0x80146871 with 0x11 cases ends at 801f7f04; second starts 801f7f08 with 0x33 cases); PSP 2a2d14 is the twin of PSX 801f7f08 and matches it case for case (Inventory_Add 0x23/0x56, Msg_OpenScript 0x42,... | psx 801f7cd8 sltiu 0x11, 801f7f1c sltiu 0x33 / psp 2a2d2c..2a2d88 (bounds 0x15/0x32/0x33) |
| 44 | `801f9320` SCENARIO/SCENA14.EMI#0 | `2af928` | 15 (17) | mispair |  | high | PSX range 801f9320..801fa904 is two functions (first switch 0x22 cases ends 801f9830; second starts 801f9834 with 0x4a cases); PSP 2af928 (sltiu 0x4a) is the twin of PSX 801f9834 and the remaining chunks are lui/ori vs lui/addiu-0x8000 splits of the same ca... | psx 801f9330 sltiu 0x22, 801f9840 sltiu 0x4a / psp 2af940 sltiu 0x4a; psx 801fa028 / psp 2b0020 |
| 45 | `801a0848` ETC/GAME.EMI#0 | `2508c` | 14 (14) | encoding | E-other | high | Same per-map dispatch (0x31/0x75/0x76/0xa9/0xab) and party flag loop; the five overlay callees are 'unresolved' on PSX so the normaliser counts them as PSP-only calls; srav-and-1 vs and-with-(1<<i) and the hoisted 0x80146250 reload are compiler choices. | psx 801a0874..801a08d4 / psp 250b8..25128; psx 801a0910 / psp 25160 |
| 46 | `801bb670` ETC/GAME.EMI#0 | `d15d0` | 14 (14) | mispair |  | high | Swapped with PSX 801ba5d8 (table row: 801ba5d8<->d2a48 has exactly the mirror call/const difference): PSP d15d0 calls d137c(=801bcbc0) with 2 args and d2314(=801baf00), returns early when both +0x34 and +0x38 are set, whereas PSX 801bb670 calls 801bccc4 (4... | psx 801bb7c8 jal 801bccc4, 801bb820 jal 801bb270 / psp d1698 jal d137c, d1708 jal d2314 |
| 47 | `801dc694` ETC/SHOP.EMI#0 | `1f920c` | 14 (7) | encoding | E-fold | high | Shop window drawing like row 42: (i+5)*8 folded to i*8+0x28, PSX LIB drawers are the PSP 9a620/9a80c/9a940/9ab6c/9b770 twins, PSX tiles 0x32/0x33 present on both sides (psx 801dcd1c/801dcd3c, psp 1f98f8/1f9918). | psx 801dcadc / psp 1f96ac; psx 801dcd1c / psp 1f98f8 |
| 48 | `801e104c` ETC/START.EMI#8 | `1f6bcc` | 13 (11) | platform | P-gpu | medium | Start-menu panel draw: PSP drops the PSX GetGraphType check before SetDrawMode (PSP 9fd54 takes the SetDrawMode args 0,0,0xf,0), Packet_Commit packet size 0xc becomes 0x10; the baf40/bb5e8 vs 1f54e4/1f5b8c calls are the same START.EMI text helpers (PSX over... | psx 801e10c0..801e1108 (addiu a1,0xc) / psp 1f6c4c..1f6c5c (ori a1,0x10); psx 801e1180 / psp 1f6ce0 |
| 49 | `801fda6c` SCENARIO/SCENA01.EMI#0 | `276660` | 13 (12) | encoding | E-fold | high | Map trigger boxes: PSX tests (pos-K) unsigned <= W, PSP tests pos<K and pos>K+W signed; spot-checked boxes are identical (0x150000..0x168000, 0x130000..0x158000, 0xc0000..0xd8000, 0x1f8000..0x200000, 0x510000..0x520000); xori 6 vs xori 2;xori 4 and andi 0xf... | psx 801fdb08..801fdb44 / psp 276738..276778; psx 801fdd7c / psp 276ad4; psx 801fdc88 / psp 2769a0 |
| 50 | `801fc91c` SCENARIO/SCENA09.EMI#0 | `29aecc` | 13 (13) | encoding | E-fold | high | Same map trigger-box pattern as row 49 (SCENA09): unsigned offset-range tests vs signed bound pairs with identical boxes (spot-checked 0x200000..0x238000, 0x410000..0x418000, 0x270000..0x298000, 0x400000/0x408000), duplicated PSP epilogues. | psx 801fc970..801fc998 / psp 29af24..29af64; psx 801fca0c / psp 29aff0; psx 801fca8c / psp 29b098 |
| 51 | `801fa9c8` SCENARIO/SCENA14.EMI#0 | `2b06f8` | 13 (13) | encoding | E-fold | high | Same SCENA14 event script: the four PSX 'unpaired' overlay calls are PSP 25610c/25621c/256384/25647c twins, LIB 801c0e2c/801c8a4c/801c2de8 are d6f20/143f8c/d8270, lui/ori vs lui/addiu-0x8000 coordinate splits (0x1c8000), the 8-member copy loop has the same... | psx 801faa48 / psp 2b0784; psx 801faf68..801fb060 / psp 2b0c2c..2b0c78; psx 801fb27c / psp 2b0e64 |
| 52 | `801d66a0` WORLD00/AREA030.EMI#4 | `1cf24` | 13 (13) | encoding | E-div | high | Area-30 tile grid draw: PSX keeps x/y in 16.16 (0x840000/0x860000/0x9b0000/0x9e0000, steps 0xc0000/0xd0000, then sra 16), PSP keeps plain ints (0x84/0x86/0x9b/0x9e, steps 12/13); divide by 12 via 0xaaaaaaab multiply vs div; andi 0xfe vs 0xff on an even valu... | psx 801d6828..801d6844 / psp 1d048..1d098; psx 801d670c / psp 1cf98; psx 801d68d8 / psp 1d0f4 |
| 53 | `801548a4` SLPS_009.90 | `e560` | 13 (13) | platform | P-ws | high | Field map renderer init widened for the PSP screen: primitive pool 0x400 -> 0x800, tile grid 28x56 -> 33x58 (loop bounds 0x1c/0x38 -> 0x21/0x3a, row stride 0x70 -> 0x84), camera tile origin x-24 -> x-27 and y+3 -> y+5, visible extents 0x1b/0x37 -> 0x20/0x39... | psx 80154958 / psp e614; psx 80154bd8,80154be8,80154bf0 / psp e82c,e83c,e844; psx 80154a7c,80154a94 / psp e6f0,e700;... |
| 54 | `801d49d8` ETC/COMMU01.EMI#7 | `10c6f4` | 12 (12) | encoding | E-inline | medium | PSX copies a 9-halfword local table from overlay data 801d0c18 with lwl/lwr, PSP stores the values as immediates (0x47,0x54,0x95,0x9a,0x4e,0x93,0x58,0x6c,0x5d; the PSX data bytes were not compared); 2a+44 vs 2(a-9)+62 and index (a-1)*8+0x801457a4 vs a*8+bas... | psx 801d49e8..801d4a30 / psp 10c6f8..10c740; psx 801d4a54 / psp 10c770..10c778; psx 801d4a68 / psp 10c78c |
| 55 | `801d4bcc` ETC/SHOP.EMI#0 | `1f29a8` | 12 (12) | mispair |  | medium | Gap-aligned SHOP state handler with different structure: PSX draws a window with Msg_SystemPtr(0x85+x), calls 801d6f10/801d7218 and sets the step byte to 0xc with SE_Play 0x107, PSP calls three unpaired helpers, draws message 0x1d7/0x1d8 and steps the same... | psx 801d4bfc,801d4c6c,801d4cdc / psp 1f29c4..1f2a14, 1f2a34..1f2a90 |
| 56 | `801eaf20` ETC/START.EMI#8 | `c7de8` | 12 (5) | encoding | E-fold | high | Start-menu window drawing of the same family as rows 42/47: (i+5)*8 folded to +40, PSX LIB drawers are the PSP 9a1b8/9a620/9ab6c/9b770 twins, 0x12-entry bitmask loop over 0x80145544 identical, scheduling only. | psx 801eaf58..801eaf90 / psp c7e3c..c7e64; psx 801eaffc / psp c7ed0 |
| 57 | `801f9384` SCENARIO/SCENA05.EMI#0 | `284ea4` | 12 (29) | mispair |  | high | PSX range 801f9384..801fc2f0 is five functions (switches of 0x13, 5, 0x67, 0x1f, 0x11 cases starting 801f9384, 801f96e0, 801f98a4, 801fb66c, 801fbdbc); PSP 284ea4 (sltiu 0x67) is the twin of PSX 801f98a4 only; twin of 801f9384 itself not found; the 801f98a4... | psx 801f9390 sltiu 0x13, 801f98b4 sltiu 0x67 / psp 284ebc sltiu 0x67 |
| 58 | `801565fc` SLPS_009.90 | `101bc` | 12 (12) | platform | P-ws | high | Field tile-grid buffer widened from 56 rows x 28 columns (56-byte rows) to 58 x 33 (66-byte rows) for the 384-wide frame, and the PSX OT-link masks (0xff000000/0xffffff) give way to the PSP's packet dispatch loop. | psx 801566a8 801566b4 80156700 80156718 / psp 00010250 00010258 000102a4 000102ac |
| 59 | `801595e0` SLPS_009.90 | `11ae8` | 12 (12) | platform | P-ws | high | Same tile-grid widening: the clear loop runs 58 rows x 33 columns with 66-byte stride where the PSX ran 56 x 28 with 56-byte stride; bounds 56/28 become 58/33. | psx 801595f0 8015960c 80159614 80159680 / psp 00011b28 00011b38 00011b40 |
| 60 | `801b1e3c` SLPS_009.90 | `cbc68` | 12 (12) | encoding | E-phantom | high | The PSX key is the boot image's zero-filled .bss at a GAME.EMI address (0 instructions; the twin came by caller vote). Diffed against GAME.EMI 801b1e3c instead, the pair scores 0 (verdict platform: platform calls only) | psp_pair_read.py phantom: GAME.EMI#0 801b1e3c n=298 vs psp cbc68 n=391, score 0 |
| 61 | `801d6f10` ETC/SHOP.EMI#0 | `1f3778` | 11 (10) | platform | P-save | high | PSX scans the memory-card directory (24-byte entries, size>>13 blocks, strncmp on names, 15-block free count); PSP replaces it with a savedata utility state machine (PLAT 2162c4/99298, error codes 0x80110301..0x80110307, SE 0x107). | psx 801d6f30 801d6f84 801d6fc0 801d7050 / psp 001f37f0 001f3880 001f38c4 001f38dc |
| 62 | `801e4478` ETC/SHOP.EMI#0 | `2008a8` | 11 (4) | encoding | E-resolve | high | The PSX 'LIB unpaired' draw calls are GAME.EMI 801afa80/801b02a0/801b0a14 (map twins 9a1b8/9a620/9b770), which the PSP calls 1:1 (rescore2 both=4). What is left is the 801af3f0 pairing conflict (map 212268, call sites 9ac74) and const 40 = (i+5)*8 folded. | psx 801e45c0 801e48c0 801e4ab0 / psp 002009e4 00200d00 00200efc |
| 63 | `800f6084` WORLD00/AREA030.EMI#5 | `e8290` | 11 (11) | encoding | E-fold | high | PSX keeps coordinates pre-shifted (0x4f0000/0x510000/0x520000/0x540000 stepped by 0xd0000); PSP holds 79/81/82/84 stepped by 13 and shifts later; the 0x1008e base offset is a relocated table base; e9234 call duplicated into branches. | psx 800f6114 800f6120 800f6384 / psp 000e8328 000e8340 000e85a8 |
| 64 | `801f30b8` WORLD01/AREA040.EMI#13 | `22a5fc` | 11 (11) | platform | P-ws | high | Tile lookup in AREA040 overlay copies the boot routine: grid bounds 56/28 become 58/33 with 66-byte rows (same widescreen tile-grid widening as rows 58/59). | psx 801f30e4 801f3100 801f313c / psp 0022a638 0022a65c 0022a6b0 |
| 65 | `801f36b0` WORLD03/AREA120.EMI#13 | `33588` | 11 (13) | unresolved |  | low | The PSX extent stops at the 5-way jump table (23 of the function's instructions), so the case bodies were never compared; the prologue matches (callee 801b0e10->9c42c, state array 0x800e4800, a 5-way switch on the byte at +2), which suggests encoding (E-swi... | psx 801f36c4 801f36fc 801f3718 / psp 000335b4 00033604 00033614 |
| 66 | `80155b38` SLPS_009.90 | `edc0` | 11 (11) | platform | P-ws | high | Same tile-grid widening as rows 58/64: bounds 56/28 become 58/33, 66-byte rows. | psx 80155b64 80155b80 80155bbc / psp 0000edfc 0000ee24 0000ee78 |
| 67 | `80155cd4` SLPS_009.90 | `ef9c` | 11 (11) | platform | P-ws | high | Same tile-grid widening as rows 58/64: bounds 56/28 become 58/33, 66-byte rows. | psx 80155d00 80155d1c 80155d58 / psp 0000efd8 0000f000 0000f054 |
| 68 | `801d86d8` ETC/SHOP.EMI#0 | `1f4cb0` | 10 (6) | encoding | E-ds | high | The PSP-only constants are sprintf-format address halves in delay slots, and the sprintf calls are only reordered (ds=4). What is left is the 801af3f0 pairing conflict (map twin 212268 against call-site twin 9ac74), not behaviour. | psx 801d87b0 801d8824 / psp 001f4d7c 001f4de4 |
| 69 | `801d144c` ETC/SISYOU.EMI#0 | `119fa0` | 10 (6) | encoding | E-ds | high | Twin of row 68 in SISYOU.EMI: the PSP-only constants are delay-slot address halves, and the same 801af3f0 pairing conflict is left over. | psx 801d14fc / psp 00119fa0 |
| 70 | `801de474` ETC/START.EMI#8 | `bdcd4` | 10 (12) | platform | P-gpu | medium | Packet_Commit length 12 becomes 16 and the PSX GetGraphType double-probe becomes one PLAT 9fd54 call with draw-mode args; the rest is encoding (unpaired PSX LIB draw calls, (i+11)*8 folded to 88). | psx 801de6d4 801de6e8 801de718 / psp 000bdf98 000bdfa4 |
| 71 | `801fe2a0` SCENARIO/SCENA08.EMI#0 | `29533c` | 10 (10) | content |  | medium | SCENA08 trigger (area 0x2b, flag 0x14 set, flag 0x15 clear, x tile 0x20..0x25): PSX requires y > 0xeffff (y >= 15.0) and PSP requires y >= 0xe0000 (14.0), so the scene-0xb trigger zone is one tile larger in y; guess: the event fires a tile earlier. | psx 801fe628 801fe62c 801fe630 801fe634 / psp 00295784 00295788 0029578c 00295790 |
| 72 | `801fc900` SCENARIO/SCENA10.EMI#0 | `2a0694` | 10 (10) | mispair |  | high | The PSX extent covers two functions: 801fc900 (6-case switch on 0x80146871, Msg_OpenScript, Flag_Clear) ends at 801fcd7c; the PSP function is the twin of the next one, PSX 801fcd80 (26-case switch), and that part matches as encoding; the twin of 801fc900 wa... | psx 801fc910 801fcd78 801fcd80 801fcd8c / psp 002a06ac |
| 73 | `801f2c90` WORLD01/AREA060.EMI#13 | `68534` | 10 (11) | platform | P-ws | high | Overlay quad laid out for the 384 frame (x -32 and 352 replace 0 and 320), and the GPU tpage/clut words change (0x7900/0x7980 to 0x400/0x600, GetGraphType probe to a PLAT call, Packet_Commit 12 to 16). | psx 801f2d4c 801f2dd0 801f2d24 / psp 000685c0 000685cc 0006861c 000685a0 |
| 74 | `801f676c` WORLD02/AREA104.EMI#14 | `245a9c` | 10 (10) | platform | P-ws | high | Sprite x coordinates shifted +32 (300/210/255 to 332/242/287, table x values +32 at load) and tpage 0x7bc6 to 0xf60 with the GetGraphType probe removed. | psx 801f67fc 801f6804 801f67dc / psp 00245b10 00245b18 00245b8c 00245adc |
| 75 | `80155e74` SLPS_009.90 | `f17c` | 10 (10) | platform | P-ws | high | Same tile-grid widening as rows 58/64 (56/28 to 58/33, 66-byte rows); the PSP also clears a row byte through a multiply on the row width. | psx 80155edc 80155ef8 80155f34 / psp 0000f1f0 0000f214 0000f224 |
| 76 | `801d125c` ETC/COMMU03.EMI#6 | `117ce8` | 9 (9) | platform | P-sound | high | COMMU03 sound display polls SpuGetKeyStatus over 24 voices on PSX and 32 on PSP; UV/tpage words also change (0x98/0x9c/0xd8 +8 to 0x99/0xd9 +7, 0x780b dropped). | psx 801d1508 801d1594 801d162c 801d1648 / psp 00117f84 00117fdc 001180bc |
| 77 | `801d63c8` ETC/START.EMI#8 | `b3cb0` | 9 (9) | encoding | E-switch | medium | The PSX key begins mid-function (the last case of a 6-way jump table, slot +0x13); the PSP function holds the whole switch as a compare chain over slots +0xe..+0x13; the remaining calls (Inventory_Add, Char_RecalcStats, Menu_EquipConfirm, SE ids) match. | psx 801d63c8 801d6514 801d6520 / psp 000b3d38 000b3dc4 000b3ecc 000b3ed8 |
| 78 | `801e588c` ETC/START.EMI#8 | `c29a8` | 9 (9) | platform | P-other | low | PSX runs a cursor box (801e6be8 with 0x20,0x30) and a yes/no routine (801dda08) and then sets menu states 0xa/0xe; PSP calls the platform routine 1f24d0 instead and steps a counter by its 0..3 result; reads as a platform dialog replacement, but the state ha... | psx 801e58ec 801e58f4 801e5918 / psp 000c2a04 000c2a10 |
| 79 | `801cf1a4` PLCHAR/PLP678.EMI#0 | `ed9dc` | 9 (5) | encoding | E-resolve | high | The PSX 'unpaired' calls are GAME.EMI 801bee0c/801b6adc (map twins d5c50/caaec), matching the PSP (rescore2 both=5). The attribute test (a+16)&0xff<2, ==0xf4, (a+10)&0xff<2 is the same set {0xf0,0xf1,0xf4,0xf6,0xf7} as the PSP compare chain (E-fold). | psx 801cf200 801cf218 801cf244 801cf258 801cf260 / psp 000eda48 000eda68 000eda88 000eda94 000edaa4 |
| 80 | `801fbca4` SCENARIO/SCENA08.EMI#0 | `294008` | 9 (20) | mispair |  | high | The PSX extent covers three functions (801fbca4 with a 9-case switch, 801fc21c with a 16-case switch, 801fc45c with a 28-case switch); the PSP function is the twin of PSX 801fc45c (sltiu 0x1c); the twins of the other two were not found. | psx 801fbcb4 801fc228 801fc468 / psp 00294020 |
| 81 | `801f88dc` SCENARIO/SCENA14.EMI#0 | `2aefb4` | 9 (27) | mispair |  | high | The PSX extent covers two functions: 801f88dc (13-case switch) ends at 801f8ce0; the PSP function is the twin of PSX 801f8ce4 (sltiu 0x19); the twin of 801f88dc was not found. | psx 801f88ec 801f8cdc 801f8cf4 / psp 002aefcc |
| 82 | `801f4608` WORLD02/AREA104.EMI#14 | `2452ec` | 9 (1) | encoding | E-resolve | high | The PSX 'LIB unpaired' calls are GAME.EMI 801b766c/801b7250/801bef00/801c34c8/801c5c40/801b1df4 (map twins cb39c/cafb4/d5d5c/d9b34/db61c/c8830), as the PSP calls them (rescore2 both=1, minor). Also PSX abs() is inline (bgez/negu) where PSP calls library abs... | psx 801f46e0 801f4708 801f480c 801f4980 / psp 0024539c 002453bc 002454a8 002455dc |
| 83 | `801f4b4c` WORLD03/AREA121.EMI#14 | `250248` | 9 (1) | encoding | E-resolve | high | Copy of row 82 in AREA121.EMI: the same GAME.EMI resolve defect (rescore2 both=1, minor) and the same inline-abs versus library-call difference. | psx 801f4c24 / psp 00250248 |
| 84 | `801f53a4` WORLD04/AREA170.EMI#13 | `260f28` | 9 (9) | encoding | E-fold | medium | The trigger coordinates are identical but split differently (PSX lui X ori 0x8000, PSP lui X+1 addiu -0x8000) with the two coordinate registers swapped; checked 0x708000/0x738000/0x208000/0x218000 against 0x900000, plus the 0x25 stores. | psx 801f53cc 801f55e0 801f5638 / psp 00260f60 00261240 00261270 |
| 85 | `801f3030` WORLD04/AREA196.EMI#13 | `7f958` | 9 (9) | platform | P-gpu | high | Tpage/clut words change (0x7940/0x7900/0x78ca to 0x500/0x400/0x3a0, GetGraphType probes removed); the 0x100 vs 0x1000 angle steps are E-loop (PSP keeps angle*16 and shifts right by 4); rsin is inlined; 0x1194 is folded. | psx 801f3314 801f3480 801f37ec / psp 0007fd1c 0007fe34 00080138 0007fa5c |
| 86 | `801af3f0` SLPS_009.90 | `9ac74` | 9 (10) | encoding | E-phantom | high | As row 60: against GAME.EMI 801af3f0 the PSP twin 9ac74 scores 1 (minor: one 0xfffc mask); the pairing gives 801af3f0's own class the PSP function 212268, which looks wrong | psp_pair_read.py phantom: GAME.EMI#0 801af3f0 n=345 vs psp 9ac74 n=334, score 1 |
| 87 | `801d62ec` ETC/SHOP.EMI#0 | `1f2f9c` | 8 (2) | encoding | E-resolve | high | The PSX 'LIB unpaired' calls are GAME.EMI 801beb74/801c23f8/801c224c/801a4e58 (map twins d5990/Battle_InitPartyContexts/Battle_InitPartyMember/88d7c), matching the PSP sequence (rescore2 both=2). 0x650 is a field offset on the PSP flag base, and 0xffffa190... | psx 801d6414 801d6424 801d647c 801d6574 / psp 001f310c 001f3118 001f3168 001f3220 |
| 88 | `801d7218` ETC/SHOP.EMI#0 | `1f3d0c` | 8 (8) | platform | P-save | high | PSX reads each memory-card save header (801eec30, 0xea0 bytes) into the slot list; PSP builds the slot summary from live game state for PSP savedata (strncpy name, party bytes, Flag_Test 0x92, 0xca0 buffer). | psx 801d7290 801d72b4 / psp 001f3e14 001f3e7c |
| 89 | `801dcdfc` ETC/SHOP.EMI#0 | `1f99b8` | 8 (4) | encoding | E-resolve | high | The PSX 'LIB unpaired' calls are GAME.EMI 801b05b0/801b0928/801b02a0 (map twins 9b540/9aa74/9a620), called 1:1 on the PSP (rescore2 both=4). What is left is the 801af3f0 pairing conflict and const 40 = (i+5)*8 folded. | psx 801dcea0 801dcf84 801dd110 / psp 001f9a84 001f9b44 001f9cd0 |
| 90 | `801e36ac` ETC/SHOP.EMI#0 | `200220` | 8 (3) | platform | P-gpu | high | Shop item-info window: DR_MODE packet committed as 12 bytes on PSX vs 16 on PSP (Packet_Commit a1 0xc->0x10) after SetDrawMode; tile loops rescaled ((i+1)*8 vs i*8+8) and sprintf format addresses in delay slots are encoding; the twin 801af3f0 call is a GAME... | psx 801e38ac / psp 200438; psx 801e38bc / psp 200450 |
| 91 | `801d2afc` ETC/START.EMI#8 | `b0270` | 8 (8) | encoding | E-fold | high | START screen row-colour setup: PSX indexes 36-byte records at (i+4) and (i+12) from an absolute base, PSP at i*36+0x93 and +0x1b3 (difference 288 = 8*36), same values 5/3 and 7/5; 801beb74 is a GAME.EMI call twinned to d5990 (E-resolve). | psx 801d2b50 / psp b02dc; psx 801d2ba0 / psp b02ec |
| 92 | `801d2dc0` WORLD00/AREA030.EMI#4 | `1e4d0` | 8 (8) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 2100fc): while that mode runs, draw positions are shifted by -36; the PSX path otherwise. The mode's meaning in play is a guess | psx 801d2df0 / psp 1e504,1e53c; psx 801d2f40 / psp 1e6b4,1e6c4; psx 801d315c / psp 1e94c,1e95c |
| 93 | `801f2d44` WORLD00/AREA037.EMI#13 | `6aa98` | 8 (8) | platform | P-gpu | medium | PSX picks tpage 0x225/0x95 and 0x2b/0x1b by GetGraphType, PSP fixed 0x95/0x1b; CLUT 0x78cf -> 0x3f0; DR_MODE 12 -> 16 bytes; PSP also folds vertex colours above 128 to 384-2v (renderer colour-range handling, guess); rcos masks 0xe00 vs 0xfff equal after <<9... | psx 801f2dc4 / psp 6aaf0; psx 801f2ef4 / psp 6abec; psx 801f2f88 / psp 6acbc-6ad20 |
| 94 | `801f3b84` WORLD03/AREA120.EMI#13 | `84524` | 8 (8) | encoding | E-bits | high | PSX tests (word & 0xffff00) == 0xNN0M00 constants, PSP tests byte1 and byte2 separately; all six (8,2)->4 (6,1)->6 (6,2)->6 (6,3)->5 (6,4)->3 (3,1) mappings identical. | psx 801f3be8 / psp 84578-8458c; psx 801f3cc4 / psp 84654-84660 |
| 95 | `801f3304` WORLD03/AREA143.EMI#13 | `7e370` | 8 (8) | encoding | E-fold | high | PSX subtracts 24..152 from the pre-increment value, PSP subtracts 32..160 from the post-increment (+8) value: identical; PSX 80197070 (GAME.EMI, unpaired in map) vs PSP 3154 is the same 12-instruction tail call per placement (medium on that). | psx 801f3340,801f3358 / psp 7e384,7e3a0; psx 801f3548 / psp 7e57c |
| 96 | `801d3288` ETC/COMMU02.EMI#8 | `11560c` | 7 (3) | encoding | E-switch | high | Digit-count switch (case 2/3/4 with <10/<100/<1000) compiled as compare chain with tail-duplicated 8015002c calls on PSP; delay-slot ADDR constants; 801af3f0 call is E-resolve. | psx 801d3338 / psp 1156b0-1156e4; psx 801d34f4 / psp 11572c |
| 97 | `801d1048` ETC/COMMU05.EMI#7 | `118edc` | 7 (5) | encoding | E-fold | high | PSX reads halfwords from the fixed buffer 0x80010072/74 etc., PSP from the loaded buffer pointer + 0x10072/74 (relocated load area); same calls and branches. | psx 801d1208,801d1224 / psp 1190bc-1190c8,1190ec-1190f8 |
| 98 | `8019a9f8` ETC/GAME.EMI#0 | `15140` | 7 (7) | encoding | E-loop | high | Window-frame tile loops rescaled ((i+k)*8 vs i*8+8k) and unrolled differently; 801af3f0/801b020c/801b0390 are GAME.EMI calls (E-resolve); all window/tile arguments (0x99,0x9a,0x14,8,0x20/0x80,9,0x74) match. | psx 8019ac30 / psp 153f4; psx 8019b0b4 / psp 15844 |
| 99 | `801be38c` ETC/GAME.EMI#0 | `d4134` | 7 (7) | encoding | E-fold | high | Masked word compare (& 0xffff00 == 0x10a00) split into byte compares (10,1); 0x18000 = 0x20000-0x8000 built differently; abs() inlined on PSX vs PLAT call 2c553c; 320-byte stride same. | psx 801be3b4-801be3d0 / psp d417c-d4194; psx 801be448 / psp d4200 |
| 100 | `801d1a88` ETC/GAME.EMI#1 | `6220` | 7 (7) | content |  | medium | GAME.EMI#1 logo/fade step: PSX fades 0x80143c26 by 4 per frame and 0x80143c28 by 2, PSP both by 2; draw positions change (801d1664 at 26,24 -> 61,31; 801d1494 at -6,28 -> -9,18). Guess: the PSP's title/logo art differs in size and placement. | psx 801d1ac0 / psp 6254; psx 801d1b8c-801d1b90 / psp 62f0,62f8; psx 801d1ba4-801d1ba8 / psp 6308,6310 |
| 101 | `801d3320` ETC/SHISU.EMI#0 | `209ec4` | 7 (3) | encoding | E-loop | high | Window border tile loops ((i+5)*8 vs i*8+40), unrolled pieces; 801b07f8/801b02a0 are GAME.EMI calls twinned to 9a940/9a620 (E-resolve). | psx 801d3584 / psp 20a0f0; psx 801d3470 / psp 209fec |
| 102 | `801e55f4` ETC/START.EMI#8 | `21686c` | 7 (7) | unresolved |  | medium | START.EMI panel step: PSX plays SE 0x102 only when counter==4, calls 801df09c, slides a panel (32-80*n) and counts down before advancing the phase; PSP plays SE 0x102 unconditionally, sets a block of state bytes and calls unpaired 2167b8/2167ec, advancing t... | psx 801e5604 / psp 216894; psx 801e5664 / psp 2168e8; psx 801e5678 / psp 216900 |
| 103 | `801cf420` PLCHAR/PLP267.EMI#0 | `ed4a8` | 7 (5) | encoding | E-switch | high | PSX tests (t+16)&0xff<2 (0xf0/0xf1), t==0xf4, (t+10)&0xff<2 (0xf6/0xf7); PSP tests 0xf0,0xf1,0xf4,0xf6,0xf7 explicitly: same sets. 80167628 vs dce04 is the same tile-attribute query. Gives item 0x29 (not item 87). | psx 801cf454-801cf474 / psp ed4e8-ed508,ed57c-ed588 |
| 104 | `801cec18` PLCHAR/PLP349.EMI#0 | `f2aa4` | 7 (5) | encoding | E-switch | high | Identical (normalised) to row 103 in PLP349: range checks vs explicit 0xf0/0xf1/0xf4/0xf6/0xf7 compares. | psx 801cec4c / psp f2ae0 |
| 105 | `801cf224` PLCHAR/PLP567.EMI#0 | `f4b10` | 7 (5) | encoding | E-switch | high | Identical (normalised) to row 103 in PLP567. | psx 801cf258 / psp f4b4c |
| 106 | `801fa6bc` SCENARIO/SCENA10.EMI#0 | `29ec58` | 7 (17) | encoding | E-other | medium | PSX range 801fa6bc..801fc220 spans two functions: an 11-case one ending at 801fa984 and the 51-case one starting at 801fa98c that is the twin; the first is the deleted chunk; the rest is field-offset/ADDR/delay-slot encoding and GAME.EMI calls (E-resolve).... | psx 801fa6c8,801fa984,801fa99c / psp 29ec70 |
| 107 | `801fcbac` SCENARIO/SCENA15.EMI#0 | `2b4964` | 7 (6) | platform | P-file | high | Scenario state machine: PSX File_LoadRequest (unpaired) replaced by PSP 970fc (psx 8016345c) and PSP adds a File_LoadDone wait before the countdown state proceeds; extra 2b2320 calls are tail duplication of PSX's shared 801fdb84 call (encoding). | psx 801fcc28 / psp 2b49e4; psx 801fd348 / psp 2b4fe0 |
| 108 | `801f9238` SCENARIO/SCENA17.EMI#0 | `2b85f8` | 7 (7) | content |  | medium | SCENA17 text drawer: after the '@' glyph (drawn by 801f91a8) x advances 88 on PSX and 105 on PSP; rest is switch-to-jump-table, tpage 0xaf/0x3f and DR_MODE 12->16 (platform). Guess: the special graphic drawn for '@' is wider on PSP. | psx 801f9348 / psp 2b8708 |
| 109 | `801daa5c` WORLD00/AREA030.EMI#4 | `e479c` | 7 (7) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 2100fc): while that mode runs, the PSP adds an exit trigger; the PSX path otherwise. The mode's meaning in play is a guess | psx 801daaa4 / psp e47ec-e4810 |
| 110 | `801dc9bc` WORLD00/AREA030.EMI#4 | `e4fdc` | 7 (7) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 2100fc): while that mode runs, the PSP-only handler e5384 (324 instructions) replaces the PSX input handler; the PSX path otherwise. The mode's meaning in play is a guess | psx 801dc9d4 / psp e4fec,e5024 |
| 111 | `800f6748` WORLD00/AREA030.EMI#5 | `e892c` | 7 (7) | encoding | E-loop | high | Fishing result panel: row positions as induction variables (0x580000/0x5c0000 += 0xd0000 vs 88/92 += 13) and the fixed 0x80010090 buffer relocated to base+0x10090; same calls. | psx 800f6810,800f6a4c / psp e89fc,e8c4c |
| 112 | `801f34a0` WORLD01/AREA047.EMI#13 | `68b00` | 7 (7) | platform | P-gpu | medium | Same effect template as row 93 (AREA047): GetGraphType tpage choice removed, CLUT 0x78cf -> 0x3f0, DR_MODE 12 -> 16, colour fold above 128 on PSP. | psx 801f352c / psp 68b54; psx 801f3648 / psp 68c38; psx 801f37b4 / psp 68d20-68d84 |
| 113 | `801f4258` WORLD01/AREA048.EMI#13 | `63628` | 7 (8) | platform | P-gpu | high | Tpage 0x2a5/0xb5 choice removed, DR_MODE 12 -> 16 bytes in 801564c4 calls; cull ranges (-60..381, -150..301), 1125 multiplier and (b-128)<<7 are E-fold. | psx 801f43ac / psp 63750; psx 801f475c / psp 63978 |
| 114 | `801f2d4c` WORLD01/AREA064.EMI#13 | `23326c` | 7 (7) | mispair |  | high | PSX range is a data table (.word entries, no prologue), not code; PSP 23326c is a Rand-driven weighted picker. True PSX twin not found (likely code after this table in AREA064#13). | psx 801f2d4c / psp 23327c |
| 115 | `801f4e3c` WORLD02/AREA104.EMI#14 | `2439cc` | 7 (2) | encoding | E-resolve | high | All PSX LIB-unpaired calls are GAME.EMI functions twinned to the PSP calls (801c4590->da9ac, 801c3be4->da15c, 801a1050->84ed0, 801a89ac->8c904, 801c4294->da74c); tile codes 0xaf/0xc0 identical; rescore2 both=2. | psx 801f4e44,801f4f1c / psp 2439d8,243a88 |
| 116 | `801f5380` WORLD03/AREA121.EMI#14 | `24ee60` | 7 (2) | encoding | E-resolve | high | Identical (normalised) to row 115 in AREA121. | psx 801f5388 / psp 24ee6c |
| 117 | `801563ac` SLPS_009.90 | `f344` | 7 (7) | platform | P-ws | low | Field cell lookup ring buffer: wrap modulus 56 on PSX vs 58 on PSP, PSP also wraps a 33-wide second axis (diff range 66, row stride 132 bytes); PSX range is cut at 20 instructions so the second axis cannot be compared. Guess: buffer widened for the 384-px v... | psx 801563c8,801563e4 / psp f364,f388; psp f398,f3cc |
| 118 | `80156ee8` SLPS_009.90 | `11c50` | 7 (7) | platform | P-gpu | high | CLUT 0x78c0/(b>>4) -> 0x300+b, tpage choice removed; cull -80..400 and (v-0x81)<<7 are E-fold. | psx 80157190 / psp 11f2c; psx 80157058 / psp 11d9c |
| 119 | `80159108` SLPS_009.90 | `115ec` | 7 (8) | content |  | medium | Field colour-tint overlay: full-screen quad widened 0..320 -> -32..352 (P-ws), and PSP adds a skip when the u16 at PSP .bss 0x100000-0xf0 equals 70 and two positions lie in 40..43 and 24..27. Guess: a location-specific suppression of the tint (possibly a fi... | psp 11678-116c8 (no PSX counterpart, psx 80159190); psx 80159200 / psp 11710-11718 |
| 120 | `8015b1d8` SLPS_009.90 | `8ba70` | 7 (6) | encoding | E-resolve | high | PSX calls 801a27a8/801a28d0/801a4a10 shown as LIB unpaired are GAME.EMI functions twinned to 86148 (Script_ShowMessage)/8627c/87894; 116 is the +0x74 field offset. | psx 8015b270,8015b280,8015b2b0 / psp 8bb04,8bb14,8bb38 |
| 121 | `80164ec8` SLPS_009.90 | `b9aac` | 7 (6) | platform | P-gpu | high | CLUT 0x7800/(b&0x3f) -> b<<4, tpage choice removed, DR_MODE 12 -> 16; PSX lwl/lwr copy of a 21-byte table vs PSP printf-style call (E-inline); UV arithmetic (16v-72/-56) matches mod 256. | psx 8016517c / psp b9c78; psx 80164fec / psp b9b54 |
| 122 | `8016571c` SLPS_009.90 | `b8048` | 7 (7) | encoding | E-inline | high | Character record copied with an unrolled 16-byte memcpy on PSX vs a 20-iteration loop on PSP, and a 6-case jump table vs compare chain (E-switch); Char_RecalcStats call unchanged. | psx 801657a8-801657cc / psp b80cc-b80d8; psx 801657e4 / psp b8124 |
| 123 | `80168288` SLPS_009.90 | `ddda8` | 7 (6) | platform | P-gpu | medium | Sprite UVs shifted by one texel (v 0x9b/0xa7 -> 0x9c/0xa8), CLUT 0x7802 -> 0x20, tpage choice removed, DR_MODE 12 -> 16; geometry (width 13, y-27) identical. Guess: PSP texture atlas placement. | psx 801683d8,801683e0 / psp ddf18,ddf38; psx 80168450 / psp ddf6c |
| 124 | `801d0ebc` ETC/COMMU01.EMI#7 | `10a5f0` | 6 (6) | encoding | E-div | high | PSX divides by 5 with 0x66666667 and computes 54*(n-9) by shifts; PSP uses div by 5 and 54*n-486, the same value; the trailing 801d28a4 call is duplicated into both branches on the PSP. | psx 801d1098..801d10fc / psp 10a79c..10a7f4 |
| 125 | `801a0aa8` ETC/GAME.EMI#0 | `8538c` | 6 (6) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 0x2100fc, plus a second PSP-only mode flag at raw 0xc4478 via 0x21f418): while either mode runs the PSP skips setting 0x8014686e=1 and the 801a8834 call; the PSX path otherwise (guess: the modes are PSP-only minigame loops);... | psx 801a0de8..801a0df4 / psp 85650..85670 |
| 126 | `801a9d38` ETC/GAME.EMI#0 | `8f2b0` | 6 (6) | encoding | E-switch | high | The same 13-case switch on (byte & 0xf0): PSX compare tree, PSP compare chain plus jump table for 0x80..0xd0; the same four callees. | psx 801a9d80..801a9f64 / psp 8f2f8..8f4e0 |
| 127 | `801d2308` ETC/SHISU.EMI#0 | `2096e8` | 6 (6) | encoding | E-inline | high | Identical call sequence; the PSP unrolls a struct copy into lh/sh pairs and replaces SetLightMatrix/NormalColorCol with one-instruction GTE stubs; added constants are the copy offsets. | psx 801d2428 801d26f4 / psp 209804 209864..209904 209a84 |
| 128 | `801d473c` ETC/SHOP.EMI#0 | `1f2674` | 6 (6) | mispair |  | high | Not twins: PSX sets 0x801e6020=0x17, opens a window via 801dab48 and bumps a byte; PSP opens a window, calls 1f3778 (psx 801d6f10) and branches on its result; PSP SHOP code around 1f2674..1f28d8 is restructured, true twin not found. | psx 801d4740..801d47a8 / psp 1f2688..1f2720 |
| 129 | `801d9230` ETC/SHOP.EMI#0 | `1f74c8` | 6 (5) | encoding | E-fold | high | Same sprintf/801501c0/801d9fc0 sequence; the PSP hoists the x+0x4e / y+0x39 coordinates into registers and addresses the shop state base-relative (w1+150, w4+152). | psx 801d94d4..801d9508 / psp 1f74f8..1f7788 |
| 130 | `801e313c` ETC/SHOP.EMI#0 | `1ffd68` | 6 (3) | platform | P-gpu | medium | Packet_Commit after SetDrawMode takes a packet length of 12 on the PSX and 16 on the PSP (the DR_MODE packet size of the PSP renderer); the rest is encoding (a 20- and 6-iteration draw loop and the 801b02a0 calls re-sequenced, a resident GAME.EMI copy of 80... | psx 801e3418..801e3428 / psp 200068..200078 |
| 131 | `801e2424` ETC/START.EMI#8 | `c0488` | 6 (3) | encoding | E-loop | high | The 20-record clear loop counts down by 8 from 19 on the PSX and up from 0 to 20 on the PSP; the 801df09c and window-open calls are the same. | psx 801e246c..801e2480 / psp c04d4..c04f0 |
| 132 | `801f83c0` SCENARIO/SCENA01.EMI#0 | `272664` | 6 (6) | unresolved |  | low | The PSP twin is 138 instructions longer: it adds branches for areas 0, 5 and 8 (McNeil Village, Rei's hideout, Cedar Woods by the sibling's places.toml) with Flag_Test 0x3a/0x15, a file load, and +0xa00 to a halfword, before the area 9.. code the PSX functi... | psx 801f83c0..801f83cc / psp 272674..2728cc |
| 133 | `801f8564` SCENARIO/SCENA05.EMI#0 | `284360` | 6 (16) | mispair |  | high | PSX extent 801f8564..801f8b8c spans two functions (the first a 22-case switch on 0x80146871, the second a 36-case one at 801f87c4); PSP 284360 is the twin of the second (801f87c4); the twin of 801f8564 is PSP 284120 (sltiu 0x16 at 284138). | psx 801f8570 801f87d0 / psp 284138 284378 |
| 134 | `801fb270` SCENARIO/SCENA08.EMI#0 | `293674` | 6 (10) | encoding | E-other | medium | The matched function is the same (0x158000 as 0x160000-0x8000, field stores reordered, File_LoadRequest twin 970fc, 801c0e2c/801c8a4c resolve to d6f20/143f8c); the PSX extent runs past its end into the next function 801fb8a0 (an 11-case switch with Zenny_Ad... | psx 801fb3bc 801fb8a0 801fb8ac / psp 2937cc 2922c4 |
| 135 | `801da320` WORLD00/AREA030.EMI#4 | `e4590` | 6 (6) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 0x2100fc): when the mode is set the PSP runs its own branch (a state switch on raw 0x11b6cc, bumps byte +0x75, calls PSP-only 21cd0c/21cad8, sets field 3 to 0xe) and then the shared 801e1734; the PSX path otherwise; area 30... | psx 801da320 801da414 / psp e459c..e4668 e471c |
| 136 | `800f63e4` WORLD00/AREA030.EMI#5 | `e85fc` | 6 (2) | encoding | E-resolve | high | Same calls in the same order; PSX jal targets 801e0cd8/801e0e24/801d79e4 are AREA030#4 functions mislabelled LIB/unpaired (PSP e345c/e3500/1d8b4 are their twins); loop of 10 unrolled differently. | psx 800f6458 800f6470 / psp e8664 e8678 |
| 137 | `800f6a9c` WORLD00/AREA030.EMI#5 | `e8cac` | 6 (2) | encoding | E-resolve | high | Same as row 136: identical call sequence once the AREA030#4 targets 801e0cd8/801e0e24/801d79e4 are resolved to e345c/e3500/1d8b4. | psx 800f6b0c..800f6c44 / psp e8d0c..e8e24 |
| 138 | `800f6ef8` WORLD00/AREA030.EMI#5 | `e90b0` | 6 (2) | encoding | E-resolve | high | Same as row 136: identical call sequence once the AREA030#4 targets are resolved. | psx 800f6f64..800f707c / psp e910c..e9208 |
| 139 | `801f35d0` WORLD01/AREA044.EMI#13 | `22cb38` | 6 (6) | mispair |  | medium | PSX key starts mid-function (first instruction andi $s0,$s0,0xff, no prologue) and covers only part of the code; PSP 22cb38 is the whole function (a 21-case jump table on 0x14($s0) with the same Flag_Test 0x16 / SE_Play 0x201/0x20e/0x202 cases); the true PS... | psx 801f35d0 / psp 22cb38..22cb90 |
| 140 | `801f37c0` WORLD02/AREA085.EMI#13 | `23983c` | 6 (4) | encoding | E-fold | high | PSX compares the halfword 0x80146870 with 0xc06; PSP compares its two bytes with 6 and 0xc; 801c224c is a GAME.EMI call (resolve label) = PSP d7a78. | psx 801f37c8..801f37d0 / psp 23983c..239864 |
| 141 | `801f3490` WORLD02/AREA102.EMI#13 | `6b9b0` | 6 (0) | encoding | E-resolve | high | Rescore with the GAME.EMI resolve fix is 0; the PSX calls are GAME.EMI functions mislabelled LIB. | psx 801f34e8 / psp 6b9b0 |
| 142 | `801f595c` WORLD03/AREA121.EMI#14 | `243f04` | 6 (6) | platform | P-ws | high | Sprite x 228 becomes 260 (+32, the 384-wide frame); the PSP also drops the GetGraphType branch, insets the UVs (u+1, v 0xd9, size 7 vs 8) and inlines the 4-byte table as constants (P-gpu side changes). | psx 801f59dc 801f59ec 801f5a2c / psp 243f64 243fb4 |
| 143 | `801f2c74` WORLD03/AREA127.EMI#13 | `250adc` | 6 (6) | mispair |  | high | The PSX key is a data table, not code (.word entries, no prologue); PSP 250adc is real code (stores 0xb at +0x7e and 0xf at 0x80146860); true twin not found. | psx 801f2c74..801f2cd0 / psp 250af8..250b18 |
| 144 | `801f3460` WORLD03/AREA149.EMI#13 | `25aa24` | 6 (6) | encoding | E-switch | high | PSX compare chain vs PSP 13-entry jump table; the 0x77ff/0x63ff toggle stores 0x63ff/0x77ff on the PSX and 0x77ff-0x1400/0x63ff+0x1400 on the PSP, and +/-0x140000 by register sign on both: equal. | psx 801f34c8..801f34d8 801f35b8..801f35c8 / psp 25aa74..25aa8c 25ab60..25ab78 |
| 145 | `801f31dc` WORLD04/AREA170.EMI#13 | `4c254` | 6 (6) | encoding | E-fold | high | PSX y = base+0x29/0x2d/0x31 and +0x50, PSP y = mid-4/mid/mid+4 with mid = base+0x2d and (base+0xa)+0x46; the colour 200 is a register on the PSX and immediates on the PSP. | psx 801f339c..801f33d4 801f34f4 / psp 4c440..4c460 4c564 |
| 146 | `801f3768` WORLD04/AREA197.EMI#13 | `7f324` | 6 (6) | platform | P-gpu | high | PSX picks tpage 0x3b or 0xab by GetGraphType and CLUT 0x78ca; PSP uses tpage 0x3b and CLUT 0x3a0 (its renderer's VRAM layout); the rest is loop rescaling (*0x80 counters) and abs as a call. | psx 801f3920..801f3968 / psp 7f500..7f510 |
| 147 | `801f36b0` WORLD04/AREA198.EMI#13 | `26eacc` | 6 (3) | encoding | E-div | high | PSX (Rand%30-6)*32 by reciprocal multiply; PSP (Rand%30)*32-192 by div; the %14 and %25 rolls the same. | psx 801f371c..801f3764 / psp 26eb44..26eb68 |
| 148 | `801f2e80` WORLD04/AREA199.EMI#13 | `8311c` | 6 (7) | platform | P-gpu | high | PSX branches on GetGraphType for tpage 0x99/0x229, commits a 12-byte DR_MODE and builds CLUTs as 0x79c0 / (n+0x1e5)<<6; PSP uses tpage 0x99, a 16-byte DR_MODE and CLUTs 0x700 / (n<<8)+0x500. | psx 801f2ed4..801f2f20 801f3354 / psp 83184..83194 833e0 |
| 149 | `8014d5d0` SLPS_009.90 | `283c` | 6 (6) | encoding | E-div | high | PSX divides by 80 with 0x66666667>>5, PSP with div 80; 801c224c is a GAME.EMI call (Battle_InitPartyMember on the PSP); 8014dafc vs 2ce4 (twin of 8014db0c) is a call-target pairing artifact. | psx 8014d6dc..8014d704 8014d75c / psp 2934..2954 298c |
| 150 | `80153950` SLPS_009.90 | `cf8c` | 6 (6) | platform | P-ws | medium | Per-row loop over 28 columns on the PSX and 33 on the PSP (start compare 27 vs 32), with the wrap counter 55 vs 57; more tiles for the wider frame. | psx 80153960 801539dc 80153a04 / psp cfac d028 d038 |
| 151 | `8015601c` SLPS_009.90 | `fdfc` | 6 (6) | platform | P-gpu | medium | PSX branches on GetGraphType to build the tpage and uses CLUT 0x1e3 and u1=u0/15; PSP builds one tpage, CLUT base 0x140 masked 0x3ff, u1=u0+14; the 0x80104000 table is base+0x104000 on the PSP (encoding). | psx 80156258 80156270..801562dc 8015618c / psp 10098 100a4 ffb0 |
| 152 | `80158464` SLPS_009.90 | `12d7c` | 6 (6) | encoding | E-fold | high | PSX culls with unsigned (x+60)<441 and (y+150)<451; PSP with signed -60<=x<381 and -150<=y<301: the same ranges; 75*15 folded to 0x465. | psx 80158504..8015852c 801585c4..801585d8 / psp 12e48..12e70 12f04 |
| 153 | `80167da0` SLPS_009.90 | `dd9b0` | 6 (6) | encoding | E-resolve | high | Same logic (state 4 or 3 by the xor of the input test with byte 0x80144952, then 801c34c8/801c5c40/801c67ac); the PSX calls are GAME.EMI functions mislabelled LIB. | psx 8016807c..801680d8 / psp ddcd4..ddd2c |
| 154 | `801b0390` SLPS_009.90 | `9a70c` | 6 (6) | encoding | E-phantom | high | As row 60: against GAME.EMI 801b0390 the PSP twin 9a70c scores 1 (minor: one constant 12) | psp_pair_read.py phantom: n=63 vs 63, score 1 |
| 155 | `801bf1a8` SLPS_009.90 | `d856c` | 6 (6) | encoding | E-phantom | high | As row 60: against GAME.EMI 801bf1a8 the PSP twin d856c scores 1 (minor: the 0x140 actor stride, as rows 19 and 20) | psp_pair_read.py phantom: n=216 vs 266, score 1 |
| 156 | `801f03c0` SLPS_009.90 | `107c88` | 6 (6) | unresolved | E-phantom | low | As row 60, but the address belongs to COMMU00.EMI#0 801f03c0 (n=84): against it the PSP twin 107c88 scores 4 (PSP-only constants 160, 20, 25, 6). That pair was not read | psp_pair_read.py phantom: COMMU00.EMI#0 801f03c0 n=84 vs psp 107c88 n=89, differs 4 |
| 157 | `801d258c` ETC/BATE.EMI#0 | `201ebc` | 5 (5) | encoding | E-div | high | PSX divides by 20 with 0xcccccccd>>4 and tests <50 then (n-50)<30; PSP uses divu 20 and <50 / <80; the 6-entry clear loop and Stat_AddClamped calls are the same. | psx 801d25d4..801d2644 / psp 201edc 201f14..201f64 |
| 158 | `801d1278` ETC/COMMU05.EMI#7 | `11915c` | 5 (3) | encoding | E-fold | high | PSX addresses 0x8011006e/0x80110076 absolutely, PSP as base+0x1006e/0x10076; PSX divides by 3 with 0x55555556, PSP with div 3. | psx 801d13d4..801d13dc / psp 119180..119188 119200 1192dc |
| 159 | `801b31e0` ETC/GAME.EMI#0 | `ccbe0` | 5 (5) | encoding | E-other | medium | The PSP duplicates the 801bf918(0,2,1) call into two tails where the PSX shares one; the record loop (stride 0x140, -2 offsets) and an 8-byte copy loop are re-expressed; same callees. | psx 801b3220..801b3290 / psp ccc34..ccca4 ccd78..ccda8 |
| 160 | `801b4ea8` ETC/GAME.EMI#0 | `cd080` | 5 (5) | platform | P-other | high | Field_EncounterDue: the PSX refuses an encounter when 0x80143efc (VSync(1) from the DrawSync callback) is above 0xf0 unless 0x80143f02 bit 0; the PSP has no such test (no DrawSync callback); the 7/10 is the same (0x66666667>>2 vs div 10); see docs/psp-stall... | psx 801b4fac..801b4fc0 / psp cd118..cd128 |
| 161 | `801b766c` ETC/GAME.EMI#0 | `cb39c` | 5 (5) | encoding | E-fold | high | PSX tests (n+0x60)&0xff<2, PSP n==0xa1 or n==0xa0; 0xefff as -0x1001; 801a0a30 is a GAME.EMI call; 80167628 vs dce04 is a duplicate-function pairing. | psx 801b789c..801b78a4 / psp cb5e4..cb5f4 |
| 162 | `801bc2b0` ETC/GAME.EMI#0 | `d37f8` | 5 (5) | encoding | E-switch | high | The same switch on (b & 0xf0) as a compare tree on the PSX and a chain on the PSP, with the same 801bc6e8/801bc758 calls; 80167628 (boot) vs dce04 (twin of GAME.EMI 801a5bc8) is a duplicate-function pairing. | psx 801bc3c8..801bc454 / psp d391c..d3964 |
| 163 | `801c0f94` ETC/GAME.EMI#0 | `d90bc` | 5 (5) | encoding | E-switch | high | Tail-duplicated 8014dc30 calls (0x52/0xcb/0x27a/0x47/0x1fd), an 0xa0-byte copy as a 20-iteration loop, the members' bit set/clear the same; 80197718 is a GAME.EMI call (PSP 3dfc). | psx 801c11dc..801c1270 801c1348..801c1360 / psp d92c4..d9350 d942c..d9444 |
| 164 | `801c3010` ETC/GAME.EMI#0 | `d986c` | 5 (5) | encoding | E-inline | medium | PSX copies a 10-byte stack table from 0x80196fe0 with lwl/lwr, PSP stores it as immediates (20/40/60); loop bound 480 by 8 vs 60 by 1. | psx 801c3028..801c3060 801c3154 / psp d9870..d9898 d9980 |
| 165 | `801d8a10` ETC/SHOP.EMI#0 | `1f4fac` | 5 (1) | encoding | E-fold | high | Rescore with both fixes is 1 (minor); shop record pointers computed the same, 801afa80 resolved to 9a1b8. | psx 801d8b84 / psp 1f5100 |
| 166 | `801d8c34` ETC/SHOP.EMI#0 | `1f51c0` | 5 (4) | encoding | E-fold | high | Shop row x offset: PSX picks s1+0x1e or s1+0x24 on a flag, PSP adds 0 or 6 then +0x1e (same value); the window-draw call goes to PSP 9ac74 (PLAT) where the resident map pairs 801af3f0 with 212268, same call site and arguments. | psx 801d8cc4..801d8cdc / psp 1f524c..1f5258 |
| 167 | `801def00` ETC/SHOP.EMI#0 | `1fb7dc` | 5 (8) | encoding | E-fold | medium | Shop list: PSX offsets 0x5d/0x1f from a0 are 0x56/0x18 from a0+7 on PSP (same base); the 0xa0 mask test is split into 0x80 then 0x20; branches on a constant-zero debug local (colour 7) are removed; the a3 argument of 801d9fc0 is a zeroed local on PSP, where... | psx 801def50 801def6c 801def80 801df0a8 801df06c / psp 1fb838 1fb880 1fba14 1fb844 |
| 168 | `801e15f0` ETC/SHOP.EMI#0 | `1fd9f0` | 5 (4) | encoding | E-resolve | high | The PSX calls into GAME.EMI 801b0928/801b02a0 are mislabelled LIB unpaired; the resident map pairs them with the PSP 9aa74/9a620 it calls; (i+5)*8 is folded as 8i+40 and loop bounds are the same. | psx 801e1704 801e18a0 801e18b8 / psp 1fdb08 1fdcc0 1fdcd4 |
| 169 | `801d1784` ETC/SISYOU.EMI#0 | `11b6fc` | 5 (4) | platform | P-gpu | medium | Weapon-shop row: the same restructuring as row 167, plus the GetGraphType-dependent draw mode replaced by the PSP's platform call and the Packet_Commit size raised from 0xc to 0x10 (DR_MODE packet length); bit 0x2000 replaces srl 0xb + andi 4. | psx 801d1a78 801d1ab8 801d193c / psp 11ba58 11ba68 11b918 |
| 170 | `801d2598` ETC/START.EMI#8 | `afd74` | 5 (5) | encoding | E-fold | high | A flag-set loop over 0x24-byte records: the PSX addresses them absolutely from 0x1b0 plus the base, the PSP through a base pointer; 801beb74 is GAME.EMI (twin d5990), not unpaired. | psx 801d2780 801d27ac 801d2784 / psp aff34 aff54 |
| 171 | `801d6a0c` ETC/START.EMI#8 | `b3f68` | 5 (5) | encoding | E-fold | high | There are no event differences; the struct field offsets 0xf/0x11 are the same on both sides and the normaliser misread the base registers. | psx 801d6a8c 801d6a9c / psp b3fb4 b3fc0 |
| 172 | `801e1770` ETC/START.EMI#8 | `c0ff0` | 5 (5) | encoding | E-bits | medium | PSX masks 48v+0x76 with 0xfffe and the PSP with 0xffff (the value is always even); the 0x1000/0x4000 button tests are reordered; PSX 801ddf28 (unresolved) and PSP bd880 (twin of 801f3da0) are called with the same arguments. | psx 801e1828 801e1878 801e17ac / psp c10a0 c10f0 c103c |
| 173 | `801e7ca0` ETC/START.EMI#8 | `c4af4` | 5 (5) | encoding | E-switch | high | The ranges are the same: PSX (c-70)<6 and (c+82)&0xff<6 are the PSP's 70<=c<76 and 174<=c<180, and the case values 0x73/0xd7/0x50/0xa8 map to 8/7/9. | psx 801e7d40 801e7d54 / psp c4b84 c4bdc |
| 174 | `801f89f0` SCENARIO/SCENA00.EMI#0 | `16de8` | 5 (3) | encoding | E-resolve | high | The same six word copies into the object at 0x1f800044 (PSP: through a base pointer); 801ad1dc is GAME.EMI with twin 8fc28. | psx 801f8a08..801f8a58 / psp 16e08..16e6c |
| 175 | `801f7450` SCENARIO/SCENA02.EMI#0 | `279778` | 5 (5) | encoding | E-other | high | Function-boundary defect: the real PSX function starts at 801f71fc (overlay disassembly: area 0, flag 0x37/0x19, area 3 with 801c2e34 and 801fd6fc(3,4), area 5) and runs through 801f7450, which is a branch target inside it; the PSP 279778 holds the whole fu... | psx 801f71fc 801f7220 801f72a0 801f7450 / psp 279778 2797d4 27983c 2799a4 |
| 176 | `801fd780` SCENARIO/SCENA02.EMI#0 | `27cc90` | 5 (6) | encoding | E-fold | medium | The trigger-box range checks are written as x-K<N on PSX and as two-bound compares on PSP (0x558000/0x568000 and so on); the calls are the same and no call or flag is added. | psx 801fd780.. / psp 27cc90.. |
| 177 | `801f747c` SCENARIO/SCENA04.EMI#0 | `2b810` | 5 (5) | encoding | E-fold | high | A table init: lui+ori 0x8000 on PSX is lui(K+1)+addiu -0x8000 on PSP; all five 0x20-byte records were checked field by field and are equal. | psx 801f7484..801f75b0 / psp 2b814..2b8e8 |
| 178 | `801fd67c` SCENARIO/SCENA07.EMI#0 | `291a1c` | 5 (5) | content |  | medium | Area 0x69 trigger: the PSX fires (8015ca78 and Msg_OpenScript(1)) when x is 7..11 and y is 28..30; the PSP fires when the state byte 3f90+8 is not 0, 1 or 2 and y>=30, ignoring x (this is the only SCENA overlay with an 0x4a/0x52/0x55/0x69/0xaf handler, so t... | psx 801fd854..801fd884 / psp 291c5c..291c90 291d4c |
| 179 | `801fba70` SCENARIO/SCENA08.EMI#0 | `29231c` | 5 (3) | encoding | E-fold | high | lui/ori 0x8000 is written as lui/addiu -0x8000 with equal values (0x3d8000, 0xd8000, 0x138000); 801c26b0 is GAME.EMI with twin d7ef0. | psx 801fbb80 801fbbc8 801fbc30 / psp 292424 292480 2924d4 |
| 180 | `801f763c` SCENARIO/SCENA13.EMI#0 | `4f534` | 5 (1) | encoding | E-resolve | high | The calls 801a5d5c/8019751c are GAME.EMI (twins 89cec/3c5c); stride 0x98 is computed as 19v*8 vs 17v*8+16v, and -0x10000/+0x10000 is the same subu/addu (rescore2 both=1). | psx 801f7790 801f77cc 801f78e0 / psp 4f610 4f62c 4f6fc |
| 181 | `801f7028` SCENARIO/SCENA14.EMI#0 | `2ad968` | 5 (9) | encoding | E-resolve | medium | The PSX LIB-unpaired calls are GAME.EMI functions whose map twins the PSP calls (801c2de8->d8270 and similar); the next area id is preloaded in delay slots; the fixed-point constants are lui/ori vs lui/addiu. | psx 801f707c 801f7284 801f73c8 / psp 2ad9d4 2adb9c 2adcac |
| 182 | `801fb3bc` SCENARIO/SCENA14.EMI#0 | `2ad67c` | 5 (5) | encoding | E-other | medium | Tail duplication of Msg_OpenScript; the callees 801f326c/801f37ac sit in other overlays and are the PSP twins 26cc60/26c230; the PSP returns an explicit 0 where the PSX leaves v0=2 (a void-style return; only matters if the table dispatcher reads it). | psx 801fb3d4 801fb3f0 801fb3f8 / psp 2ad698 2ad6a4 2ad6d8 |
| 183 | `801fb414` SCENARIO/SCENA14.EMI#0 | `2ad6e8` | 5 (5) | encoding | E-other | medium | Same as row 182 with argument 4 (tail duplication; explicit return 0 vs leftover v0=2). | psx 801fb42c 801fb450 / psp 2ad704 2ad744 |
| 184 | `801fb46c` SCENARIO/SCENA14.EMI#0 | `2ad754` | 5 (5) | encoding | E-other | medium | Same as row 182 with argument 2 (tail duplication; explicit return 0 vs leftover v0=2). | psx 801fb484 801fb4a8 / psp 2ad770 2ad7b0 |
| 185 | `801fb4c4` SCENARIO/SCENA14.EMI#0 | `2ad7c0` | 5 (5) | encoding | E-other | medium | Same as row 182 with argument 5 (tail duplication; explicit return 0 vs leftover v0=2). | psx 801fb4dc 801fb500 / psp 2ad7dc 2ad81c |
| 186 | `801fb51c` SCENARIO/SCENA14.EMI#0 | `2ad82c` | 5 (5) | encoding | E-other | medium | Same as row 182 with argument 6 (tail duplication; explicit return 0 vs leftover v0=2). | psx 801fb534 801fb558 / psp 2ad848 2ad888 |
| 187 | `801f7aec` SCENARIO/SCENA15.EMI#0 | `51020` | 5 (5) | platform | P-gpu | medium | An offscreen tile inside DrawArea (0x340,0x100): the PSX places it at obj.x+0x334 and obj.y+240*(buf^1)-21 (VRAM double buffer); the PSP's renderer uses obj.x+0x114 and obj.y-37 with no buffer term. | psx 801f7c2c 801f7c5c..801f7c6c / psp 51120 51138 |
| 188 | `801fddc0` SCENARIO/SCENA15.EMI#0 | `2b2518` | 5 (5) | encoding | E-other | medium | Same pattern as row 182 (tail duplication; other-overlay callees 26cc60/26c230; explicit return 0). | psx 801fddec 801fde08 / psp 2b2544 2b2584 |
| 189 | `801f8f38` SCENARIO/SCENA17.EMI#0 | `2b83c0` | 5 (5) | platform | P-gpu | medium | The PSX draws two black letterbox tiles (0,0,320,30) and (0,210,320,30) with Packet_Commit; the PSP calls the platform helper a24cc(0x20,0x1e,0x140,0xb4), likely with a widescreen x offset of 32. | psx 801f8f58..801f8fcc / psp 2b83c4..2b83d8 |
| 190 | `801f33a8` WORLD00/AREA027.EMI#13 | `608e0` | 5 (4) | platform | P-gpu | medium | The GetGraphType-chosen tpage (0x225/0x95) is fixed at 0x95; MapView_LinkPrimAt links the DR_MODE packet with size 0xc on PSX and 0x10 on PSP (packet length); (0x468000>>9)-0x4000 is folded to -0x1cc0. | psx 801f33c4 801f3420 801f3438 / psp 60910 60940 60944 |
| 191 | `801d3820` WORLD00/AREA030.EMI#4 | `19e98` | 5 (5) | content | C-mode | medium | PSP-only mode hook (0x11d48c): when it is set the window is drawn at x=-16 (vs 20) and the text at x=-7, and value 10 calls the PSP-only 210e44(-7,20,0,0x9f); the PSX path otherwise. Guess: a PSP-only mode inside AREA030 (fishing area). | psx 801d3824 801d387c / psp 19ea4 19eb4 19ef8 |
| 192 | `801d7c58` WORLD00/AREA030.EMI#4 | `1daa8` | 5 (5) | content | C-mode | medium | PSP-only mode hook (0x11d48c): when it is set the columns shift x-44 and a fourth column (bit 3) is drawn through PLAT 212c88; the PSX path otherwise. Guess: a PSP-only mode inside AREA030 (fishing area). | psx 801d7cb4 801d7dfc / psp 1dadc 1db1c 1dc78 1dcb4 |
| 193 | `801d978c` WORLD00/AREA030.EMI#4 | `203ac` | 5 (5) | platform | P-gpu | medium | Glyph draw: the tpage chosen by GetGraphType (0x8f/0x2f) becomes fixed 0x2f/0x3f, CLUT 0x7800 is dropped, and the code/21 division by reciprocal (0x30c30c31) becomes div. | psx 801d97ec 801d9820 801d9b58 / psp 203e4 20430 |
| 194 | `801dce38` WORLD00/AREA030.EMI#4 | `e16fc` | 5 (5) | content | C-mode | medium | PSP-only mode hook (0x11d48c): when it is set the text x is -7 (vs 29) and 0x210158 can force the cancel path (SE 0x106, flag 0x80); the PSX path otherwise. Guess: a PSP-only mode inside AREA030 (fishing area). | psx 801dce38 801dcf7c / psp e1710 e1724 e1738 |
| 195 | `801dd6bc` WORLD00/AREA030.EMI#4 | `e1954` | 5 (5) | content | C-mode | medium | PSP-only mode hook (0x11d48c): when it is set the text x is -7 (vs 29) and 0x210158 can skip to the exit path; the PSX path otherwise. Guess: a PSP-only mode inside AREA030 (fishing area). | psx 801dd6c0 801dd700 / psp e1964 e1978 e19d4 |
| 196 | `801e0450` WORLD00/AREA030.EMI#4 | `e2c5c` | 5 (5) | encoding | E-fold | high | The clamps x in [0x1a0000,0x3e0000] and y in [0x110000,0x320000] are written as <=K-1 vs <K (0x19ffff/0x1a0000, 0x10ffff/0x110000, 0x13ffff/0x140000); the inline abs becomes a call to abs (801c5c40 twin). | psx 801e04fc 801e0530 801e05a4 801e05d4 / psp e2d90 e2dbc e2e20 e2e40 |
| 197 | `801f2dcc` WORLD01/AREA049.EMI#14 | `230140` | 5 (1) | encoding | E-resolve | high | The calls 801c3530/801c3708/801b1df4 are GAME.EMI (twins d9b88/d9d1c/c8830); the 0xdfff mask matches -0x2001; rescore2 both=1. | psx 801f302c 801f303c 801f30e0 801f2e30 / psp 2301a4 |
| 198 | `801f3258` WORLD02/AREA088.EMI#13 | `4d4c4` | 5 (5) | platform | P-gpu | medium | The draw mode is set through the PSP platform call instead of GetTPage/SetDrawMode, and Packet_Commit grows from 0xc to 0x10 (DR_MODE length); the rest is an inlined 0x24 packet copy and the same rsin/rcos math; 801f3600 (unresolved) maps to PSP 4d09c. | psx 801f32c0 801f32cc 801f32e8 / psp 4d51c 4d528 4d558 |
| 199 | `801f2ed4` WORLD02/AREA091.EMI#13 | `23e220` | 5 (5) | platform | P-gpu | medium | The GetGraphType tpage (0xae/0x3e) is fixed at 0x3e, and MapView_LinkPrimAt's DR_MODE size is 0xc on PSX and 0x10 on PSP; the fixed-point constants are lui/addiu folds. | psx 801f2f6c 801f2fd8 / psp 23e2e8 23e308 |
| 200 | `801f2d9c` WORLD02/AREA105.EMI#13 | `7b7c8` | 5 (6) | platform | P-gpu | medium | The tpage (0x1ab/0x7b via GetGraphType) is fixed at 0x7b and CLUT 0x78c6 becomes 0x360 (PSP CLUT id); the UV math (b-0x80)*128-0x40 is folded to 128b-0x4040 (equal). | psx 801f2e9c 801f2ebc 801f2ecc / psp 7b8ac 7b8b0 7b8d0 |
| 201 | `801f2c60` WORLD03/AREA117.EMI#13 | `640ec` | 5 (3) | encoding | E-inline | medium | The inline abs(x-K) becomes calls to abs (801c5c40 twin) with the same constants 0x28000/0x150000/0x160000; the base offsets are folded. | psx 801f2d58 801f2d90 801f2db8 / psp 641ec 6420c 64228 64244 |
| 202 | `801f3884` WORLD03/AREA117.EMI#13 | `64e20` | 5 (5) | encoding | E-fold | high | The constants are folded and equal: (b-128)*128+64 = 128b-0x3fc0, (b-128)*128+s-8 = 128b+s-0x4008, and v/2-(s-64)+8 = v/2-s+0x48. | psx 801f38c0 801f38f8 801f3978 / psp 64e4c 64e74 64f70 |
| 203 | `801f60f4` WORLD03/AREA121.EMI#14 | `245664` | 5 (4) | platform | P-gpu | medium | The tpage chosen by GetGraphType (0xab/0x3b) is fixed at 0x3b and CLUT 0x78ca becomes 0x3a0; the UV 0xfe is computed as 0xe0+0x1e (equal). | psx 801f628c 801f62f4 801f62dc / psp 2457d0 2457d4 2457dc |
| 204 | `801f3050` WORLD03/AREA140.EMI#13 | `711a0` | 5 (5) | platform | P-gpu | medium | The GetGraphType tpage (0x225/0x95) is fixed at 0x95 and Packet_Commit's DR_MODE size goes from 0xc to 0x10; the coordinate constants are folds ((b-128)*128+64 = 128b-0x3fc0). | psx 801f3064 801f30b0 801f30e8 / psp 711b8 711dc 71210 |
| 205 | `801f35e4` WORLD03/AREA141.EMI#13 | `25581c` | 5 (1) | encoding | E-resolve | high | The calls 801c6df4/801acba8 are GAME.EMI (twins 106fec/8f78c); x+0xffbb0000 is written as x-0x450000; rescore2 both=1. | psx 801f35f8 801f3654 801f3670 / psp 255830 255834 |
| 206 | `801f4150` WORLD04/AREA189.EMI#13 | `7db38` | 5 (5) | platform | P-gpu | medium | The tpage chosen by GetGraphType (0x327/0x2a7) is fixed, the CLUT 0x7a00 becomes 0x800, and the UV/CLUT constants are re-expressed (0x600, +0x100). | psx 801f41e0 801f4214 801f4344 / psp 7dbd4 7dc1c 7dd24 |
| 207 | `8014ae58` SLPS_009.90 | `570` | 5 (5) | mispair |  | high | The PSX function is the exec/shutdown routine (StopCallback, PadStop, StopCARD, Exec, ClearOTag/DrawOTag, SetDispMask); PSP 570 is a pad reader (sceCtrl STUB, analog-to-button mapping). True twin not found; the PSX routine likely has no PSP counterpart. | psx 8014ae8c..8014af1c / psp 5ac 604..6c4 |
| 208 | `8014dff0` SLPS_009.90 | `330c` | 5 (5) | platform | P-file | high | A CD file read: CdSearchFile/CdControl/CdPosToInt/CdIntToPos/CdControlB are replaced by the PSP's file stubs (9e2f4..9e32c); the 0x88-byte directory-entry copy is the same (17x8). | psx 8014e020 8014e034 8014e0a8 8014e0c4 / psp 334c 3360 33b0 33cc |
| 209 | `8014fd78` SLPS_009.90 | `a050` | 5 (5) | platform | P-gpu | medium | Text glyph draw: the CLUT 0x7800/n becomes n<<4 (PSP CLUT index); the signed mod-32 is idiom-rewritten; -0x58 vs +0xa8 on a byte store is equal; the duplicate 955c call for chars 8..11 has the same arguments. | psx 8014fdc4 8014fe7c 8014ff9c / psp a0b0 a168 a1fc a218 a2e4 |
| 210 | `801532a0` SLPS_009.90 | `d57c` | 5 (5) | encoding | E-other | medium | World-map camera: the same calls and order, heavily rescheduled; the area 0xbd branch calls the other-overlay function 801f2c04 on PSX and PSP 7c608 (twin of 9d00fd:801b0e10); the 0x7fff00/0x10401e constants are folds of the PSX's -0x8000 masks and its 8010... | psx 801532c0 80153354 801534bc / psp d5b4 d648 d740 |
| 211 | `80153858` SLPS_009.90 | `ce94` | 5 (5) | platform | P-ws | high | World-map tile ring enlarged: the wrap column 27 becomes 32, columns 28 become 33, and rows 56 become 58 (more tiles drawn for the wider PSP view). | psx 8015386c 801538dc 801538ec / psp ceac cf20 cf30 |
| 212 | `80153a6c` SLPS_009.90 | `d0a8` | 5 (5) | platform | P-ws | high | The same tile-ring enlargement as row 211 (0x1b/0x1c/0x37 become 0x20/0x21/0x3a). | psx 80153a7c 80153a84 80153b10 / psp d0c4 d114 d154 |
| 213 | `80155154` SLPS_009.90 | `d48c` | 5 (5) | platform | P-ws | high | The same tile-ring enlargement as row 211 (0x1b/0x1c/0x37 become 0x20/0x21/0x39/0x3a). | psx 80155170 80155178 801551e8 / psp d4b8 d528 d538 |
| 214 | `80157474` SLPS_009.90 | `1204c` | 5 (5) | encoding | E-div | high | The divide by 10 through 0x66666667>>2 becomes div; the inline abs becomes an abs call; the loop counters are rescaled ((7-i)*8+7 vs s2-=8); the coordinate bases are folded (-0x4080/-0x4000). | psx 8015752c 801575e8 80157640 / psp 12164 12174 12158 120f4 |
| 215 | `8015d6f0` SLPS_009.90 | `94cec` | 5 (3) | platform | P-sound | high | Sound init: SsInit/SsSetTableSize/SsSetTickMode/SsUtSetReverbType/SsSetReservedVoice are replaced by PSP sound-layer calls (2bfc14, 2bfc60, 0x500 volumes, STUB). | psx 8015d6f8 8015d70c 8015d734 / psp 94d18 94d4c 94d58 94da0 |
| 216 | `801658e8` SLPS_009.90 | `b8200` | 5 (5) | encoding | E-inline | high | A party record (0x80144964 base) copied into a local: a 0xa0+4-byte lw/sw block loop on PSX, a 20-iteration loop on PSP; the stride and field offsets are re-expressed. | psx 8016592c 80165974..801659a8 / psp b8208 b8284 |
| 217 | `801efa50` ETC/COMMU00.EMI#0 | `109a2c` | 4 (4) | encoding | E-switch | high | Faerie-village production tick: per-type item tables (14/20/18/16-byte records) are read at the same field offsets (+8/+0xc, +9/+0x12, +9/+0x10, +9/+0xe); /20 by reciprocal becomes div; the switch on type is reordered. PRIORITY: reads the consumables table... | psx 801efb48 801efb54 801efce8 / psp 109b2c 109b34 109c20 |
| 218 | `801d2110` ETC/COMMU01.EMI#7 | `10c8d8` | 4 (4) | encoding | E-loop | high | Clear loop counts by 8 to 480 on PSX and by 1 to 60 on PSP; row offset is 52*n on both (PSX 13n*4, PSP 17n*4-16n); 0xfffc mask on a multiple of 4 equals 0xffff; PSP duplicates the 801651b4 tail call per branch. | psx 801d2224 / psp 10c9bc; psx 801d2440 / psp 10cb2c; psx 801d24ac / psp 10cbcc |
| 219 | `801d3038` ETC/COMMU01.EMI#7 | `10d5b0` | 4 (3) | platform | P-gpu | medium | GetGraphType/SetDrawMode pair replaced by the PSP platform call and Packet_Commit length 12 -> 16; the rest (menu layout, div-by-5 reciprocal, case 5 test in s3) is equivalent. | psx 801d30f0 / psp 10d684; psx 801d3130 / psp 10d694; psx 801d3430 / psp 10da80 |
| 220 | `801d2ce0` ETC/COMMU02.EMI#8 | `10fb48` | 4 (5) | platform | P-gpu | high | Sprite packet: GetGraphType/SetDrawMode replaced, Packet_Commit 12 -> 16, CLUT words 0x7bc8/0x7bc9 -> 0xf80/0xf90 in the PSP's own encoding. | psx 801d2d48 / psp 10fb90; psx 801d2d94 / psp 10fbd8; psx 801d2dc4 / psp 10fc0c |
| 221 | `801d6cf0` ETC/COMMU02.EMI#8 | `112cf8` | 4 (4) | encoding | E-other | high | Same loop; PSX window call 801d8a38 (unpaired) and PSP 1162f8 (paired to another overlay's copy) take identical args; 0xfff8 mask on 16-8k is a no-op; stride 0xa4 present on both. | psx 801d6d4c / psp 112d54; psx 801d6dfc / psp 112e00 |
| 222 | `801d7214` ETC/COMMU02.EMI#8 | `113198` | 4 (4) | encoding | E-other | high | As row 221 (same window-call pairing artefact); Msg_OpenScript(0xf2) present on both sides. | psx 801d7270 / psp 1131fc; psx 801d73a8 / psp 11333c |
| 223 | `801d7c64` ETC/COMMU02.EMI#8 | `113c38` | 4 (4) | encoding | E-other | high | As row 221; Msg_OpenScript(0xf7) present on both sides. | psx 801d7cc0 / psp 113c94; psx 801d7d88 / psp 113d50 |
| 224 | `801a3100` ETC/GAME.EMI#0 | `865fc` | 4 (4) | encoding | E-switch | medium | Same returned set (1 for 0x10..0x50, 0xa0, 0xb0, 0xf0; 0 otherwise) as a compare tree vs a linear chain; callee differs in label only (PSX boot 80167628 unpaired, PSP dce04 = twin of GAME.EMI 801a5bc8), same args, not proven identical. | psx 801a3114 / psp 86618; psx 801a3124 / psp 86634 |
| 225 | `801ae734` ETC/GAME.EMI#0 | `93628` | 4 (4) | encoding | E-div | high | PSX range 801ae734..801aeac0 spans four functions (the Msg_OpenSystem/0xfeff code is in 801ae90c and 801ae9f0); the first function matches, record index /0x98 by reciprocal multiply on PSX vs div on PSP; the PSP's always-true flag test before 8e308 is equiv... | psx 801ae81c / psp 93720; psx 801ae774 / psp 9368c |
| 226 | `801afa80` ETC/GAME.EMI#0 | `9a1b8` | 4 (5) | platform | P-gpu | medium | Sprite: GetGraphType/SetDrawMode replaced, Packet_Commit 12 -> 16, tpage words 0x780c/0x780e -> 0xc0/0xe0 in PSP encoding. | psx 801afae8 / psp 9a210; psx 801afb50 / psp 9a270 |
| 227 | `801b0e10` ETC/GAME.EMI#0 | `7c608` | 4 (4) | platform | P-other | medium | Camera matrix setup: PSP precomputes angles from globals, wraps half-angles to 15 bits (equivalent for 4096-period angles) and adds a change detector that calls 7cbe0 (n=353, only weakly paired to a widescreen-flagged AREA189 function) for 3 frames after th... | psx 801b0e78 / psp 7c70c; psp 7c674; psp 7c770 |
| 228 | `801bfba8` ETC/GAME.EMI#0 | `d635c` | 4 (4) | encoding | E-resolve | medium | Same loop over 5 records stride 0x140; PSX GAME.EMI call 8019701c (mislabelled LIB) equals PSP 3100. | psx 801bfc68 / psp d643c |
| 229 | `801bfe54` ETC/GAME.EMI#0 | `d8aa0` | 4 (4) | encoding | E-fold | high | PSX 0x8002d600/ori 0x800 addresses become a pointer base plus 0x2d600/0xf0800 on PSP; record stride 0x140 on both. | psx 801bffb4 / psp d8c0c; psx 801c0018 / psp d8c98 |
| 230 | `801c70d8` ETC/GAME.EMI#0 | `143c90` | 4 (3) | encoding | E-fold | medium | 0x800e4000 becomes base+0xe4000; PSP callees 142bd0 and 142c50 sit where PSX calls 801c7a74 and 801c7ae4 (same roles: index, then flag), so the call difference is pairing; PopMatrix tail duplicated. | psx 801c7178 / psp 143d28; psx 801c7170 / psp 143d30; psx 801c7198 / psp 143d48 |
| 231 | `801d47bc` ETC/SHOP.EMI#0 | `1f28dc` | 4 (4) | content |  | low | Shop confirm prompt: PSX opens system message 0x85 + byte 801e6020 and handles the yes/no in place (SE 0x107 on cancel); PSP always opens message 0x9d and delegates input to PSP-only helper 1f2350; guess: same prompt if that byte is 0x18, otherwise the PSP... | psx 801d47f8 / psp 1f28fc; psx 801d4858 / psp 1f2918 |
| 232 | `801da2a8` ETC/SHOP.EMI#0 | `1f5c94` | 4 (2) | encoding | E-ds | high | No differing chunks; constants are relocated address halves in delay slots (both score 2, resolve fix accounts for the call). | (no chunk) |
| 233 | `801dbeb0` ETC/SHOP.EMI#0 | `bc1c4` | 4 (4) | platform | P-gpu | medium | Packet_Commit 12 -> 16 with the GetGraphType call replaced; the frame-tile loop is the same once GAME.EMI 801b02a0 (mislabelled LIB) is read as PSP 9a620 (E-resolve). | psx 801dbf60 / psp bc280; psx 801dbf7c / psp bc2a0 |
| 234 | `801e1d38` ETC/SHOP.EMI#0 | `1fc578` | 4 (2) | encoding | E-ds | high | No differing chunks; constants are relocated address halves (both score 2). | (no chunk) |
| 235 | `801d0dd4` ETC/SISYOU.EMI#0 | `11996c` | 4 (4) | encoding | E-other | medium | Same menu logic and SE ids; row y 0x36*n+0x3e same; cursor call goes to a parameterised duplicate (PSP 11a41c, extra args) where PSX calls 801d1cc8; window call pairing artefact as row 221. | psx 801d102c / psp 119ba8; psx 801d104c / psp 119bc4 |
| 236 | `801d10ac` ETC/SISYOU.EMI#0 | `119c2c` | 4 (4) | encoding | E-other | medium | As row 235. | psx 801d1260 / psp 119db4; psx 801d1280 / psp 119dd8 |
| 237 | `801d6734` ETC/START.EMI#8 | `b2c50` | 4 (4) | encoding | E-fold | medium | Equal event streams (ratio 1.00); only field and global addressing differ. | (no chunk) |
| 238 | `801f9fb8` SCENARIO/SCENA00.EMI#0 | `27016c` | 4 (5) | encoding | E-fold | high | Camera z test 0x4400 then +0x1e00 (PSP) vs store 0x6200 (PSX), and -0x1e0000 vs +0xffe20000: same values. | psx 801fa1a0 / psp 2702a4; psx 801fa1b4 / psp 2702b4 |
| 239 | `801f7c38` SCENARIO/SCENA02.EMI#0 | `279ec4` | 4 (6) | encoding | E-resolve | medium | First function (36 states) matches PSP 279ec4; PSX range also holds the next function 801f8070 (9 states), source of the deleted constants; 0xb0 xor equals 0x10^0x80^0x20; GAME.EMI 801c26b0 = PSP d7ef0. | psx 801f7db8 / psp 27a004; psx 801f7f1c / psp 27a170 |
| 240 | `801f8cac` SCENARIO/SCENA05.EMI#0 | `2846c4` | 4 (13) | mispair |  | medium | PSX range is two functions, 801f8cac (8 states) and 801f8eb0 (13 states); PSP 2846c4 (13 states) is the twin of 801f8eb0, which matches after GAME.EMI resolve (801c7014=107174, 801c2e34=d82b8); twin of 801f8cac not found in this row. | psx 801f8eb8 / psp 2846dc; psx 801f9014 / psp 28482c |
| 241 | `801fb6dc` SCENARIO/SCENA07.EMI#0 | `290734` | 4 (10) | mispair |  | medium | PSX range is two functions, 801fb6dc (7 states) and 801fb914 (13 states); PSP 290734 is the twin of 801fb914 (GAME.EMI 801c7014/801c0e2c/801c8a4c = 107174/d6f20/143f8c; 28f688 call is PSX's shared tail at 801fc000); twin of 801fb6dc not found here. | psx 801fb920 / psp 29074c; psx 801fc000 / psp 290acc |
| 242 | `801fc2d0` SCENARIO/SCENA07.EMI#0 | `290b90` | 4 (3) | encoding | E-resolve | high | All call differences are GAME.EMI targets mislabelled LIB (801c2e34, 801c0e2c, 801c8a4c, 801a8900 = d82b8, d6f20, 143f8c, 8c84c); 0x258000 is 0x260000-0x8000. | psx 801fc324 / psp 290be8; psx 801fc67c / psp 290ee4 |
| 243 | `801f823c` SCENARIO/SCENA08.EMI#0 | `3d1e8` | 4 (4) | encoding | E-fold | high | GT4 UVs: PSX fp-(t0-0x40) and +0x41 vs PSP s3-a2+0x40/+0x41, same values. | psx 801f83b0 / psp 3d344; psx 801f84a0 / psp 3d420 |
| 244 | `801fa3e4` SCENARIO/SCENA08.EMI#0 | `292c10` | 4 (5) | encoding | E-resolve | high | GAME.EMI 801c2de8 = PSP d8270; scenario constants folded; 0x80145fcc store is base 0x103f90+0x140. | psx 801fa438 / psp 292c88; psx 801fa6d0 / psp 292e28 |
| 245 | `801fdb14` SCENARIO/SCENA10.EMI#0 | `2a0e74` | 4 (5) | encoding | E-fold | medium | Trigger ranges: PSX v-5<2 plus ==4 vs PSP ==6/5/4; unsigned x-K<N vs andi+slti; 0x458000 as 0x460000-0x8000; PSX 801fe2b0 vs PSP helper 2a0e18 in the call slot. | psx 801fdc04 / psp 2a0f9c; psx 801fdc28 / psp 2a0fc0 |
| 246 | `801fc890` SCENARIO/SCENA12.EMI#0 | `2a8f1c` | 4 (4) | encoding | E-fold | high | Every trigger box checked: PSX (x-K) unsigned <= W vs PSP two signed bounds give the same ranges (e.g. 0x1f8000..0x208000, 0x248000..0x270000, 0x368000..0x370000). | psx 801fc8d0 / psp 2a8f68; psx 801fcbe4 / psp 2a936c |
| 247 | `801fbbc4` SCENARIO/SCENA13.EMI#0 | `2acef4` | 4 (0) | encoding | E-resolve | high | GAME.EMI 801c2874/801c2628 (mislabelled LIB) = PSP d8084/d7e34; both score 0. | psx 801fbc64 / psp 2acf78 |
| 248 | `801fbdd8` SCENARIO/SCENA13.EMI#0 | `2ad0c8` | 4 (0) | encoding | E-resolve | high | As row 247. | psx 801fbe70 / psp 2ad150 |
| 249 | `801fbe10` SCENARIO/SCENA14.EMI#0 | `2b1784` | 4 (0) | encoding | E-resolve | high | As row 247. | psx 801fbef0 / psp 2b1838 |
| 250 | `801fc130` SCENARIO/SCENA14.EMI#0 | `2b1a58` | 4 (0) | encoding | E-resolve | high | As row 247. | psx 801fc1c4 / psp 2b1aec |
| 251 | `801fc1d8` SCENARIO/SCENA15.EMI#0 | `2b428c` | 4 (4) | encoding | E-resolve | high | GAME.EMI 801ade9c/801c2de8 = PSP 92dc8/d8270; actor positions 0x2f8000/0x278000 as lui-0x8000; 0x8015e974 as a relocated address. | psx 801fc898 / psp 2b47dc; psx 801fc3f0 / psp 2b4424 |
| 252 | `801fe3c8` SCENARIO/SCENA15.EMI#0 | `2b5a90` | 4 (0) | encoding | E-resolve | high | As row 247; 80167800 present on both. | psx 801fe474 / psp 2b5b28 |
| 253 | `801f91a8` SCENARIO/SCENA17.EMI#0 | `2b856c` | 4 (4) | content |  | medium | Sprite width 88 -> 105 px (height 18, u 0, v 0x40); CLUT word also re-encoded; guess: a PSP-redrawn caption image in SCENA17 that is wider. | psx 801f91f8 / psp 2b85c0; psx 801f91e8 / psp 2b85ac |
| 254 | `801f4768` WORLD00/AREA016.EMI#13 | `22440c` | 4 (4) | platform | P-gpu | high | tpage chosen by GetGraphType on PSX (0x12b or 0x5b) is fixed 0x5b on PSP; CLUT 0x78cb -> 0x3b0; abs via PLAT 2c553c; RotTransPers4 copy loop. | psx 801f4a98 / psp 22472c; psx 801f4a90 / psp 224724 |
| 255 | `801f2c74` WORLD00/AREA027.EMI#13 | `2eaac` | 4 (4) | encoding | E-div | high | Divide by 3 by reciprocal 0x55555556 vs div; 0x800e4800 as base+0xe4800; stride 0x98 same. | psx 801f2cb4 / psp 2eafc; psx 801f2c7c / psp 2eabc |
| 256 | `801d2c30` WORLD00/AREA030.EMI#4 | `197d4` | 4 (4) | platform | P-gpu | high | tpage chosen by GetGraphType (0x229 or 0x99) fixed 0x99 on PSP; CLUT 0x7a40 -> 0x900; UVs identical (u1 0x28/0x38 as u0+-0x10). | psx 801d2c7c / psp 19814; psx 801d2c9c / psp 19818 |
| 257 | `801d3f54` WORLD00/AREA030.EMI#4 | `1a6ac` | 4 (4) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 2100fc): while that mode runs, a slide threshold changes from 29 to -7 and its reset from 39 to 3; the PSX path otherwise. The mode's meaning in play is a guess | psp 1a6c4; psx 801d3f80 / psp 1a6fc; psx 801d3ff4 / psp 1a758 |
| 258 | `801d82f4` WORLD00/AREA030.EMI#4 | `1de84` | 4 (4) | platform | P-gpu | high | tpage chosen by GetGraphType (0x2d or 0x1d) fixed on PSP; CLUT 0x7887 -> 0x270; UVs identical. | psx 801d8338 / psp 1decc; psx 801d8360 / psp 1ded0 |
| 259 | `801da0d8` WORLD00/AREA030.EMI#4 | `df364` | 4 (3) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 2100fc): while that mode runs, an animation is skipped; the PSX path otherwise. The mode's meaning in play is a guess | psp df374; psp df388; psx 801da164 / psp df3a0 |
| 260 | `801db1c0` WORLD00/AREA030.EMI#4 | `e4bdc` | 4 (4) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 2100fc): while that mode runs, an extra path runs; the PSX path otherwise. The mode's meaning in play is a guess | psp e4d00; psp e4d34 |
| 261 | `801dcd74` WORLD00/AREA030.EMI#4 | `e15ec` | 4 (4) | content | C-mode | medium | PSP-only mode hook (0x11d48c via 2100fc): while that mode runs, a window's x moves from 29 to -7; the PSX path otherwise. The mode's meaning in play is a guess | psp e15fc; psx 801dcde8 / psp e1614 |
| 262 | `801dd824` WORLD00/AREA030.EMI#4 | `e1ad0` | 4 (4) | content | C-mode | medium | As row 261 (window x 29 to -7 under the PSP-only mode); the PSX path otherwise | psp e1ae0; psp e1b4c |
| 263 | `801e105c` WORLD00/AREA030.EMI#4 | `e371c` | 4 (4) | encoding | E-inline | medium | Priority: reads the consumables table (PSX 0x801c995c + (id+56)*14, PSP base+0x310 + id*14, same record); local 6-byte string copy inlined as byte stores; special-case name copy is 5 bytes on PSX vs 8 on PSP (JP string, not game logic). | psx 801e11c4 / psp e38b0; psx 801e1090 / psp e3730; psx 801e1180 / psp e389c |
| 264 | `800f5e5c` WORLD00/AREA030.EMI#5 | `e80e0` | 4 (5) | platform | P-ws | high | Backdrop drawn as two 160-wide quads from x 0 on PSX and two 192-wide quads from x -32 on PSP (384 logical frame); tpage/GetGraphType and packet length re-encoded. | psx 800f5ef4 / psp e8184; psx 800f5f68 / psp e820c |
| 265 | `801f469c` WORLD00/AREA033.EMI#13 | `227d38` | 4 (4) | platform | P-gpu | high | Same function as row 254 in another overlay: tpage by GetGraphType (0x12b/0x5b) fixed on PSP, CLUT 0x78cb -> 0x3b0. | (as row 254) |
| 266 | `801f4768` WORLD01/AREA045.EMI#13 | `22ea50` | 4 (4) | platform | P-gpu | high | PSX picks tpage 0x5b/0x12b via two GetGraphType calls and sets CLUT 0x78cb; PSP hard-codes tpage 0x5b and CLUT 0x3b0 (its own VRAM layout); inline abs becomes the PSP abs helper; UV math and OT adds match | psx 801f4a90..801f4abc / psp 22ed68..22ed78 |
| 267 | `801f3ae4` WORLD01/AREA048.EMI#13 | `62f74` | 4 (3) | encoding | E-fold | high | PSX ORs (((x>>5)&0x38)+0x50)<<16 into the tpage word only when (w&0xf80000)==0 via a branch; PSP hoists the value and multiplies it by the (w&0xf80000)==0 flag; same result | psx 801f3cac..801f3ccc / psp 63020..63070, 6315c..63184 |
| 268 | `80117e78` WORLD01/AREA049.EMI#13 | `6c210` | 4 (4) | platform | P-gpu | high | Same sprite pattern: PSX chooses tpage 0x3b/0xab with GetGraphType and CLUT 0x78c5, PSP fixes tpage 0x3b and CLUT 0x350; (x-0x80)<<7 vs x<<7-0x4000 and /3 by reciprocal are encoding | psx 80118088..801180d0 / psp 6c418..6c428 |
| 269 | `801f35a0` WORLD01/AREA052.EMI#13 | `23150c` | 4 (4) | encoding | E-fold | medium | PSX uses literal 0x11/0x29/7/8 that the PSP derives from the (checked-equal) position words as (pos-0x8000)>>16; tile get/set calls sit in the same places with the same args (PSX getter 80167628 is boot-unpaired, PSP calls dce04 at that site) | psx 801f35ac..801f3604, 801f36e0 / psp 23151c..231578, 231668 |
| 270 | `801f47a0` WORLD01/AREA065.EMI#13 | `234de8` | 4 (4) | platform | P-gpu | high | Same AREA sprite function as row 266 (identical chunk structure): PSX picks tpage 0x5b/0x12b via GetGraphType and sets CLUT 0x78cb, PSP hard-codes tpage 0x5b and its own CLUT 0x3b0; inline abs becomes the PSP abs helper; the rest matches | as row 266 (tpage/CLUT stores) |
| 271 | `801f54b8` WORLD02/AREA077.EMI#14 | `611bc` | 4 (3) | encoding | E-fold | high | Quad corners x-0x4040/x-0x3f40, y-0x4030/y-0x3fd0 on PSX vs (x-0x4000)-0x40/+0xc0 and (y-0x4000)-/+0x30 on PSP: same values split differently | psx 801f5500..801f550c / psp 611f0..6125c |
| 272 | `801f4190` WORLD02/AREA082.EMI#13 | `6dbdc` | 4 (4) | encoding | E-div | high | /10000 by reciprocal (0x68db8bad) vs div 0x2710; range test (y-0xa6040) unsigned <= 0x53020 vs two signed compares against 0xa6040/0xf9060, and > 0xf905f vs >= 0xf9060: same ranges | psx 801f419c..801f41dc, 801f4224..801f424c, 801f4288..801f4294 / psp 6dbf0..6dc24, 6dc78..6dc94 |
| 273 | `801f4798` WORLD02/AREA087.EMI#13 | `23ba4c` | 4 (4) | platform | P-gpu | high | Same AREA sprite function as row 266 (identical chunk structure): PSX picks tpage 0x5b/0x12b via GetGraphType and sets CLUT 0x78cb, PSP hard-codes tpage 0x5b and its own CLUT 0x3b0; inline abs becomes the PSP abs helper; the rest matches | as row 266 |
| 274 | `801f52c8` WORLD02/AREA088.EMI#13 | `23d9cc` | 4 (4) | platform | P-gpu | high | Same AREA sprite function as row 266 (identical chunk structure): PSX picks tpage 0x5b/0x12b via GetGraphType and sets CLUT 0x78cb, PSP hard-codes tpage 0x5b and its own CLUT 0x3b0; inline abs becomes the PSP abs helper; the rest matches | as row 266 |
| 275 | `801f3194` WORLD02/AREA102.EMI#13 | `6b6e4` | 4 (4) | encoding | E-fold | high | Screen-bounds test (v+20)&0xffff < 361 unsigned vs v >= -20 && v < 341 signed: same range [-20,340]; (x-128)<<7 vs x<<7-0x4000 | psx 801f3218..801f3248 / psp 6b758..6b788 |
| 276 | `801f2cb0` WORLD02/AREA103.EMI#13 | `302a0` | 4 (5) | platform | P-gpu | high | DrawMode packet committed as 12 bytes on PSX vs 16 on PSP (Packet_Commit size); the loop (stride 0x4bc, y -= 0x70000, clamp 0x20/0x80) is the same with the induction re-expressed | psx 801f2d04..801f2d08 / psp 302f8..302fc |
| 277 | `801f5174` WORLD02/AREA103.EMI#13 | `6a478` | 4 (4) | encoding | E-loop | high | PSP counts the depth term down by 8 from 63-t4 where PSX recomputes (7-i)*8+7-t4 each iteration; PSX also computes a dead masked value into s7 (read only by itself) that the PSP drops | psx 801f5328..801f537c / psp 6a540..6a55c, 6a604..6a624, 6a674 |
| 278 | `801f6360` WORLD02/AREA104.EMI#14 | `24486c` | 4 (4) | platform | P-ws | high | Centre x 250 on PSX vs 282 (+32) on PSP: widescreen offset; PSX also picks tpage 0x3e/0xae by GetGraphType (PSP fixed 0x3e) and commits DrawMode as 12 vs 16 bytes | psx 801f63bc / psp 2448d0; psx 801f63d0..801f641c / psp 2448f4..24490c |
| 279 | `801f3940` WORLD02/AREA106.EMI#13 | `245ea8` | 4 (4) | encoding | E-fold | high | Position constants built as lui+ori 0x8000 on PSX vs lui(+1) then -0x8000 on PSP: same values 0x128000/0xb8000/0x238000/0x118000/0x1f8000 | psx 801f3964..801f3a2c / psp 245edc..245f9c |
| 280 | `801f2f50` WORLD03/AREA114.EMI#13 | `2499d8` | 4 (4) | encoding | E-switch | high | PSX key 801f2f50 is a case label inside 801f2e48, which dispatches on byte 0x801454e8 through a 13-entry jump table; PSP 2499d8 is the whole function as a compare chain (cases 10/11/12, else 1) with the same flag 0x96/0x97 tests and Inventory_Add calls (ite... | psx 801f2e48..801f2e84 (read from the overlay capture) / psp 2499e8..249a30 |
| 281 | `801f4768` WORLD03/AREA115.EMI#13 | `24b5f8` | 4 (4) | platform | P-gpu | high | Same AREA sprite function as row 266 (identical chunk structure): PSX picks tpage 0x5b/0x12b via GetGraphType and sets CLUT 0x78cb, PSP hard-codes tpage 0x5b and its own CLUT 0x3b0; inline abs becomes the PSP abs helper; the rest matches | as row 266 |
| 282 | `801f2f5c` WORLD03/AREA117.EMI#13 | `64404` | 4 (4) | encoding | E-fold | high | abs(X-0x28000)>0x18000 // (abs(Y-0x150000)>0x18000 && abs(Y-0x160000)>0x18000) on both sides; PSP re-tests X on one path (tail duplication) and splits the constants with lui/addiu | psx 801f2f60..801f2fd0 / psp 64418..64490 |
| 283 | `801f2edc` WORLD03/AREA119.EMI#13 | `669e8` | 4 (4) | encoding | E-fold | high | Same box test around (0x428000, 0x3e0000/0x3f0000) with tolerances 0x8000/0x10000; PSX adds negated constants, PSP subtracts positive ones through the abs helper; 62*i index computed as 64i-i-i | psx 801f2f04..801f2fe4 / psp 66aa4..66b14 |
| 284 | `801f2d20` WORLD03/AREA135.EMI#14 | `3eb3c` | 4 (2) | encoding | E-resolve | high | PSX call 801ad1dc is the GAME.EMI function whose twin is PSP 8fc28 (mislabelled LIB/unpaired); colour ((v&0xff)-480)*2 vs 2*(v&0xff)-960 gives the same byte; PSP duplicates the tail | psx 801f2dec, 801f2f9c / psp 3ebfc, 3edbc |
| 285 | `801f5a44` WORLD03/AREA135.EMI#14 | `2537c4` | 4 (1) | encoding | E-resolve | high | PSX call 801a7ca0 is GAME.EMI (twin 8b200, mislabelled); x==4//(x-6)<2u vs x==4//x==6//x==7 compare chain; record stride 116 computed two ways | psx 801f5a78, 801f5ac4..801f5acc / psp 253800, 253838..253850 |
| 286 | `801f3f84` WORLD03/AREA139.EMI#13 | `70aa4` | 4 (5) | platform | P-gpu | high | PSX chooses tpage 0x225/0x95 with GetGraphType (PSP fixes 0x95) and passes DrawMode size 12 vs 16 to the OT-add helper 801564c4; quad corner math re-split ((x-128)<<7/64 vs x<<7-0x3fc0) | psx 801f3f98..801f3ffc / psp 70abc, 70af4 |
| 287 | `801f46d4` WORLD03/AREA151.EMI#13 | `25c974` | 4 (4) | platform | P-gpu | high | Same AREA sprite function as row 266 (identical chunk structure): PSX picks tpage 0x5b/0x12b via GetGraphType and sets CLUT 0x78cb, PSP hard-codes tpage 0x5b and its own CLUT 0x3b0; inline abs becomes the PSP abs helper; the rest matches | as row 266 |
| 288 | `801f4628` WORLD04/AREA152.EMI#13 | `25e7a0` | 4 (4) | platform | P-gpu | high | Same AREA sprite function as row 266 (identical chunk structure): PSX picks tpage 0x5b/0x12b via GetGraphType and sets CLUT 0x78cb, PSP hard-codes tpage 0x5b and its own CLUT 0x3b0; inline abs becomes the PSP abs helper; the rest matches | as row 266 |
| 289 | `801f31d0` WORLD04/AREA172.EMI#13 | `55d74` | 4 (4) | encoding | E-other | medium | Two overlay-local calls (PSX 801f3fac/801f3984, PSP 5688c/56500) are unpaired on both sides but sit at the same point with no args; 0x800e48c0 buffer addressed as pointer+0xe48c0 on PSP; loop identical | psx 801f3218..801f3220 / psp 55dbc..55dc4; psx 801f3250 / psp 55dec..55df4 |
| 290 | `801f2dac` WORLD04/AREA173.EMI#14 | `81a20` | 4 (2) | encoding | E-fold | high | Map buffer at fixed 0x80104000 on PSX vs pointer+0x104000 on PSP; 194*w row stride and &~0xff/(i+0xe6) tile write identical | psx 801f2df8..801f2e74 / psp 81aa4..81b2c |
| 291 | `801f4de0` WORLD04/AREA189.EMI#13 | `26b1d0` | 4 (1) | encoding | E-resolve | high | PSX calls 801beb74 and 801c48ec are GAME.EMI functions (twins d5990, dacb4) mislabelled LIB/unpaired; the stride 0x140 is present on both sides (normaliser noise) | psx 801f4ee0, 801f4f08, 801f4f28 / psp 26b2c0, 26b2ec, 26b304 |
| 292 | `801f2dc8` WORLD04/AREA193.EMI#13 | `26d44c` | 4 (4) | encoding | E-inline | high | PSX copies a 12-byte local table from .rodata 801f2c04 with lwl/lwr; PSP stores the same six halfwords (checked in the capture) as immediates | psx 801f2dd4..801f2e08 / psp 26d450..26d48c |
| 293 | `801f41f4` WORLD04/AREA197.EMI#13 | `26e1e0` | 4 (4) | encoding | E-fold | high | Struct passed by value: PSP spills args and reads halves; tile tests (y==0x708000/0x738000 with x>>16 in 3..5; x==0x28000/0x58000/0xd8000 with y>>16 from 0x71) identical, lui+ori vs lui-0x8000 | psx 801f420c..801f42c4 / psp 26e20c..26e33c |
| 294 | `80154ef4` SLPS_009.90 | `eabc` | 4 (4) | encoding | E-fold | high | Map buffer 0x80104000 fixed vs pointer+0x104000; (w>>14)&0x3c vs ((w>>16)&15)<<2 identical | psx 80154fa4..80154fa8 / psp ebd0..ebe0 |
| 295 | `801564c4` SLPS_009.90 | `f478` | 4 (4) | platform | P-gpu | medium | OT-add helper: PSX links with 24-bit pointers (&0xffffff, &0xff000000) and accepts depth 0..55 unsigned; PSP stores full pointers, checks the packet buffer against its own bound and accepts depth 0..57 (bound 56 to 58), so two more depth slots are drawn on... | psx 80156514 / psp f4d8..f4dc; psx 80156570..80156598 / psp f54c..f568 |
| 296 | `801581b8` SLPS_009.90 | `12a8c` | 4 (4) | encoding | E-switch | high | Map-patch dispatcher: type 0/1/2/default by beq chain on PSX vs bgtz/slti chain on PSP, case 0 falling into case 1 on both; (w>>14)&0x3c vs ((w>>16)&15)<<2; map buffer fixed vs pointer+0x104000 | psx 80158230..8015824c, 801582b8..801582bc / psp 12b24..12b80, 12c2c..12c38 |
| 297 | `80161e44` SLPS_009.90 | `96b08` | 4 (4) | encoding | E-div | high | Positional sound pan and volume: /100 and /480 by reciprocal multiply on PSX vs div on PSP; 44*(y+64) vs 44y+0xb00; csqrt (LIB) vs PSP 9c800; the voice-attr writes to 0x18/0x1a match | psx 80161e90..80161eac, 80162034..801620a4 / psp 96b5c..96b6c, 96cc4..96d64 |
| 298 | `80166150` SLPS_009.90 | `b8a80` | 4 (4) | encoding | E-switch | medium | Equip-stat applier (20-byte records at 0x801c9f30, ids 13..77): PSX unsigned (id-13)<65 jump table vs PSP 13<=id<78 jump table; every case body matches (Stat_AddClamped/80165fe4/80166084 with the same offsets and values); jump-table contents not compared; n... | psx 801661a8..801661cc / psp b8ae8..b8b0c |
| 299 | `801af214` SLPS_009.90 | `99f00` | 4 (4) | encoding | E-phantom | high | As row 60: against GAME.EMI 801af214 the PSP twin 99f00 scores 0 (same) | psp_pair_read.py phantom: n=38 vs 44, score 0 |
| 300 | `801ef214` ETC/COMMU00.EMI#0 | `1077bc` | 3 (3) | encoding | E-loop | high | Loop counter steps by 8 to 480 on PSX vs by 1 to 60 on PSP | psx 801ef2bc..801ef2c4 / psp 10783c..10784c |
| 301 | `801ef398` ETC/COMMU00.EMI#0 | `1078e0` | 3 (3) | encoding | E-loop | high | Loop counter steps by 8 to 480 on PSX vs by 1 to 60 on PSP | psx 801ef440..801ef448 / psp 107964..107970 |
| 302 | `801d1158` ETC/COMMU01.EMI#7 | `10a86c` | 3 (3) | encoding | E-fold | high | y = (a-9)*54 + 7q + 68 on PSX vs 54a + 7q - 418 on PSP: identical | psx 801d12a4..801d12d0 / psp 10a984..10a9cc |
| 303 | `801d3f08` ETC/COMMU01.EMI#7 | `10bd54` | 3 (4) | platform | P-gpu | high | PSX picks tpage 0x2e/0x1e by GetGraphType with a 12-byte DrawMode, PSP fixes 0x1e and 16 bytes; CLUT 0x7840/(t>>4) on PSX vs t+0x100 from a PSP table; /10 digits by reciprocal vs div | psx 801d3f28..801d3f74, 801d4010 / psp 10bd84..10bd9c, 10be10 |
| 304 | `801d0fc8` ETC/COMMU02.EMI#8 | `1152e4` | 3 (3) | encoding | E-div | high | Decimal digit split /100 and /10 by reciprocal multiply (0x51eb851f, 0x66666667) vs div; clamp 0..100 identical | psx 801d0fc8..801d1060 / psp 1152f8..115374 |
| 305 | `801d1894` ETC/COMMU02.EMI#8 | `10e784` | 3 (3) | encoding | E-loop | high | Row y kept as y<<16 (start 0x300000, step 0x200000) on PSX vs plain 48 step 32 on PSP | psx 801d18bc, 801d18f4 / psp 10e7bc, 10e7fc |
| 306 | `801d316c` ETC/COMMU02.EMI#8 | `10ff48` | 3 (2) | encoding | E-ds | high | Only relocated address halves in delay slots differ (delay-slot rescore 0) | psx 801d3210 / psp 10ffec |
| 307 | `801d5e4c` ETC/COMMU02.EMI#8 | `111e2c` | 3 (3) | encoding | E-fold | high | Window helper linked once on PSP (1162f8) vs an overlay-local copy 801d8a38 on PSX, same args; (16-8k)&0xfff8 vs &0xffff equal since the value is a multiple of 8 | psx 801d5e84..801d5e90 / psp 111e64..111e78 |
| 308 | `801d6288` ETC/COMMU02.EMI#8 | `112238` | 3 (3) | encoding | E-fold | high | Same as row 307: local helper copy vs shared PSP helper, mask 0xfff8 vs 0xffff on a multiple of 8 | psx 801d62c0..801d62cc / psp 112270..112280 |
| 309 | `801d6bbc` ETC/COMMU02.EMI#8 | `112bb0` | 3 (3) | encoding | E-fold | high | Same as row 307; the message 0xf7 tail is identical | psx 801d6bf4..801d6c00 / psp 112be8..112bfc |
| 310 | `801d9c44` ETC/COMMU02.EMI#8 | `116a80` | 3 (2) | platform | P-gpu | medium | Window frame: PSX sets tpage 0x2d/0x1d by GetGraphType and a 12-byte DrawMode before each sprite, PSP fixes 0x1d and 16 bytes and loops what PSX unrolls; 801b02a0 is GAME.EMI (twin 9a620, mislabelled) and (n-2)*8 vs 8n-16 match | psx 801d9c8c..801d9cdc / psp 116a80 area; psx 801da13c / psp 116e60 |
| 311 | `801d16f4` ETC/COMMU05.EMI#7 | `1195d0` | 3 (3) | platform | P-gpu | high | PSX tpage 0x2d/0x1d by GetGraphType and 12-byte DrawMode vs PSP fixed 0x1d and 16; (b<<3)&0xf0 vs ((b&0xfe)<<3)&0xff identical | psx 801d1724..801d1788 / psp 1195fc..119630; psx 801d183c / psp 1196c8..1196dc |
| 312 | `801994e8` ETC/GAME.EMI#0 | `7f08` | 3 (3) | content |  | medium | Frame counter at 0x80145038 is incremented every call on PSX but only while below 999,999,999 on PSP, so it saturates (guess: a play-time count capped for display; about 193 days at 60 Hz to reach) | psx 801994f8..80199508 / psp 7f14..7f30 |
| 313 | `8019ec74` ETC/GAME.EMI#0 | `23b1c` | 3 (3) | encoding | E-fold | high | Buffer 0x800e4d40 fixed vs pointer+0xe4d40; per-record x/y/z += 2v or v under the same test; 8 records of 0x20 identical | psx 8019ec94..8019ed54 / psp 23b3c..23be0 |
| 314 | `801a2ee0` ETC/GAME.EMI#0 | `882d8` | 3 (3) | mispair |  | high | Collision probe: PSP 882d8 sums f(x),f(x+1),[f(x-1) when aligned] and g(pos),g(pos+0x8000),g(pos-0x8000), which is exactly PSX 801a2c64 (read from the GAME capture); PSX 801a2ee0 uses floor/+0x8000 through wrapper 801a3244 and should pair with PSP 88704 (th... | psx 801a2cd0..801a2d40 / psp 88340..883ec; psx 801a2f74..801a2f90 |
| 315 | `801a4418` ETC/GAME.EMI#0 | `873e4` | 3 (3) | encoding | E-fold | high | Copies three words from a 152-byte record (0x80146884 table); absolute addressing on PSX vs base+0x34/0x38/0x3c on PSP; stride 152 identical | psx 801a4468..801a44bc / psp 87420..87484 |
| 316 | `801adf2c` ETC/GAME.EMI#0 | `92e54` | 3 (3) | encoding | E-switch | high | PSX range runs past the function end (801ae0e8) into three following functions; within the function: jump table on rand&15 and index /152 by reciprocal vs div; PSP adds a (obj+8 != g)^4 test that is never zero, so 8015c4b4 is always called as on PSX | psx 801adf5c..801adf70, 801ae004..801ae028 / psp 92e8c..92eb4, 92f40..92f48 |
| 317 | `801aeac0` ETC/GAME.EMI#0 | `9399c` | 3 (3) | encoding | E-resolve | high | PSX call 8019701c is GAME.EMI (mislabelled); PSP calls 3100; record writes via base+0x34/0x38/0x3c, 116-byte stride computed two ways | psx 801aeacc / psp 939b0; psx 801aebe0..801aec70 / psp 93a8c..93b00 |
| 318 | `801b0d08` ETC/GAME.EMI#0 | `9ab6c` | 3 (3) | encoding | E-div | high | (n+4) mod 12 by reciprocal 0x2aaaaaab vs div 12 | psx 801b0d44..801b0d50 / psp 9ab98..9aba4 |
| 319 | `801b1560` ETC/GAME.EMI#0 | `cb6f4` | 3 (3) | encoding | E-switch | medium | Result of 801b1e3c dispatched by beq chain (0..6, 0xff) on PSX vs range test plus jump table on PSP; tail with 801b703c duplicated on PSP; flag mask &0xcfff identical; boot getter 80167628 (unpaired) at the site where PSP calls dce04 | psx 801b16cc..801b1728, 801b183c / psp cb7b0..cb7c8, cb96c |
| 320 | `801b65b8` ETC/GAME.EMI#0 | `ca6a4` | 3 (3) | encoding | E-fold | high | Member loop from record 1 (+0x140) on both; PSX indexes with 0x140 from a fixed base, PSP with pointer+0x140 | psx 801b6608..801b666c / psp ca6e8..ca734 |
| 321 | `801b80cc` ETC/GAME.EMI#0 | `ce9fc` | 3 (3) | encoding | E-fold | high | Same as row 315 for the 0x80147a54 table (152-byte records), base+0x34/0x38/0x3c on PSP | psx 801b8108..801b8184 / psp cea48..ceab4 |
| 322 | `801bdd58` ETC/GAME.EMI#0 | `d53f8` | 3 (3) | encoding | E-fold | medium | Step count ((d-0xf0000)>>16)+1 when d>0xfffff vs d>=0x100000; inline abs vs helper; probe offsets +-0x10000 and loop bound s5-1 (from 0) vs s5 (from -1); final &~0x7fff snap identical; not every probe path traced | psx 801bde1c..801bde40, 801bdf74..801be024 / psp d54c0..d54ec, d5554..d575c |
| 323 | `801c1750` ETC/GAME.EMI#0 | `d7228` | 3 (3) | encoding | E-bits | high | Direction table index (d<<2)&0x3f8 vs (d>>1)<<3 on d&0xff; 0xbf vs -65 mask; member stride 0x140 identical | psx 801c1794..801c1798, 801c18a8 / psp d7274..d7278, d728c |
| 324 | `801c1904` ETC/GAME.EMI#0 | `d736c` | 3 (3) | encoding | E-bits | high | Same index form as row 323; double step, optional extra step and party-member scan for flag +0x70 identical | psx 801c1918..801c1a54 / psp d7370..d7448 |
| 325 | `801c1aac` ETC/GAME.EMI#0 | `d7498` | 3 (3) | encoding | E-inline | high | PSX inlines abs(a-b)>0xffff (negu + slt 0xffff,x); PSP calls the PLAT abs (twin 801c5c40) and tests x<0x10000; andi 0xbf vs and -65 is the same mask; 0x140 is the same 320-byte stride on both. | psx 801c1b2c..801c1b44 / psp d7514..d7524 |
| 326 | `801c1e40` ETC/GAME.EMI#0 | `d77b4` | 3 (3) | encoding | E-fold | high | Masks 0xfff7/0xfdf7 vs -9/-513 applied to the reloaded halfword: same bits cleared. | psx 801c1e9c 801c1eb0 / psp d7800 d7834 |
| 327 | `801c26b0` ETC/GAME.EMI#0 | `d7ef0` | 3 (3) | encoding | E-inline | high | 164-byte record copy: PSX 0xa0-bound 16-byte loop plus tail word, PSP 20x8-byte loop plus tail word; same 0x140 destination stride. | psx 801c27c8 801c27f8 / psp d7fb8 d8004 |
| 328 | `801c388c` ETC/GAME.EMI#0 | `d9e70` | 3 (3) | encoding | E-other | high | Final test that bytes +0x5d..+0x5f all equal -64: PSX one masked word compare against 0xc0c0c0xx, PSP three byte compares; the clamp logic before it is identical. | psx 801c3968..801c3988 / psp d9f08..d9f2c |
| 329 | `801c7bac` ETC/GAME.EMI#0 | `142d28` | 3 (3) | encoding | E-fold | high | PSX absolute 0x800e4048 table becomes PSP base pointer (0xd0790) + 0xe4048; 0x50000 present on both (lui a1,5 at psp 142dfc). | psx 801c7bf4 801c7c0c / psp 142db4..142dc4 |
| 330 | `801d0e54` ETC/GAME.EMI#1 | `583c` | 3 (0) | encoding | E-resolve | high | All calls present on both after resolving 8019711c->31c0 and 801a870c->8c694 into GAME.EMI; SoundSet_Layout is called later on PSP (reordered); rescore2 both=0. | psx 801d0e88 801d0e90 801d0ec0 / psp 586c 588c 58b4 |
| 331 | `801d1664` ETC/GAME.EMI#1 | `5ee4` | 3 (3) | platform | P-gpu | medium | Drops the GetGraphType hi-res tpage branch (0x2ab vs 0xbb), DR_MODE packet 12->16 bytes, and the PSX's second sprite draw at (x+240,y+112) is absent on PSP; guess: the PSP renderer draws the image in one piece where the PSX split it across texture pages. | psx 801d1688..801d16b0 801d16d4 801d170c..801d173c / psp 5f20 5f38 5f4c |
| 332 | `801d1880` ETC/GAME.EMI#1 | `6078` | 3 (3) | platform | P-gpu | high | x*0x66666667>>40 is x/640 (E-div, PSP uses div 0x280); remaining difference is the dropped GetGraphType 0x200 tpage branch and DR_MODE packet 12->16. | psx 801d18e4..801d1904 801d19ac..801d19d8 801d1a04 / psp 60f4 6184 61a0 |
| 333 | `801d3af4` ETC/SHOP.EMI#0 | `1f12f8` | 3 (3) | encoding | E-bits | high | y & 0xfffe on PSX is a no-op (y = 18-20n is always even); window call 801dab48 vs PSP twin of 801f3da0 with identical args (0x14,0x12,0x118,0x13). | psx 801d3b3c 801d3b38 / psp 1f1320 1f1340 |
| 334 | `801d3ba8` ETC/SHOP.EMI#0 | `1f139c` | 3 (2) | encoding | E-other | high | Only the window callee differs by label (PSX 801dab48 unpaired vs PSP twin of 801f3da0) with the same arguments; rescore both=2. | psx 801d3bcc / psp 1f13cc |
| 335 | `801d60c0` ETC/SHOP.EMI#0 | `1f2d7c` | 3 (3) | encoding | E-bits | high | Same as row 333: & 0xfffe on an always-even y, window callee label only. | psx 801d6104 801d6108 / psp 1f2dc8 |
| 336 | `801d8054` ETC/SHOP.EMI#0 | `1f4a8c` | 3 (3) | encoding | E-bits | high | & 0xfffe on always-even values (0x10-2k, 0x3e+2k); 801beb74 resolves to PSP d5990; tail-merged calls duplicated on PSP. | psx 801d8098 801d8140 801d8168 / psp 1f4ae8 1f4b70 |
| 337 | `801d8270` ETC/SHOP.EMI#0 | `1f7060` | 3 (3) | encoding | E-switch | high | (flags&0xa0)==0xa0 test becomes nested 0x80/0x20 tests with the same four outcomes (string 5760/5768 <-> -79db/-79d6 mapping checked); 0x2000 set/clear and s5=7/4/2 identical; coordinate offsets precomputed. | psx 801d83bc..801d8478 801d8480..801d84f4 / psp 1f71e4..1f72f8 1f7300..1f735c |
| 338 | `801dc18c` ETC/SHOP.EMI#0 | `be740` | 3 (2) | platform | P-gpu | high | Same GetTPage/SetSprt/GetClut sequence; RECTs built in the packet instead of on the stack and DR_MODE packet length 12->16; 801b02a0 resolves to 9a620 (E-resolve). | psx 801dc220 801dc5e4 / psp be7f4 bebcc |
| 339 | `801dd304` ETC/SHOP.EMI#0 | `bc448` | 3 (1) | platform | P-gpu | high | Only residue after resolving 801b02a0->9a620 is the DR_MODE packet length 12->16; rescore2 both=1. | psx 801dd370 801dd3c0 / psp bc4c0 bc51c |
| 340 | `801de1b8` ETC/SHOP.EMI#0 | `1fa6d0` | 3 (3) | encoding | E-other | medium | PSP keeps the counting loops but drops the stores to the 3x2-byte array at PSX 0x801e617c/617d (dead-store elimination of a file-static never read on PSP); worth checking who reads 0x801e617c on PSX. | psx 801de298 801de2c4 801de2fc / psp 1fa784..1fa7cc |
| 341 | `801ded58` ETC/SHOP.EMI#0 | `1faa34` | 3 (3) | encoding | E-bits | high | & 0xfffe on always-even y (0x3e+2k, 0x12-8n); 0x1a-40n identical; window callee label only. | psx 801dedbc 801deecc / psp 1faaf8 1fab38 |
| 342 | `801df59c` ETC/SHOP.EMI#0 | `1fae40` | 3 (3) | encoding | E-bits | high | Same pattern as row 341 (even-y mask, precomputed offsets, window callee label). | psx 801df644 / psp 1faf1c 1faf28 |
| 343 | `801e26c0` ETC/SHOP.EMI#0 | `1fe520` | 3 (3) | encoding | E-switch | high | v==0x36 // (v-0x49)<2 // v==0x52 vs explicit 0x36/0x49/0x4a/0x52 compares; window callee label only. | psx 801e2788..801e27a8 / psp 1fe5e0..1fe600 |
| 344 | `801d21c4` ETC/START.EMI#8 | `af3d8` | 3 (3) | encoding | E-fold | high | Same predicate: PSX (v+0x60)&0xff<2 on a 0xf0-masked value vs PSP !=0xa0 && !=0xa1; 0xaf and 0x91 tests identical; callee 80167628 vs twin of 801a5bc8 with same args. | psx 801d21ec..801d2214 / psp af3fc..af424 |
| 345 | `801d2cb0` ETC/START.EMI#8 | `b03ac` | 3 (3) | encoding | E-fold | high | Index+4 / index+12 times 36 folded into PSP field offsets 0x93/0x1b3; same values 6/4/8 stored. | psx 801d2d04 801d2d54 / psp b0418..b0428 |
| 346 | `801d31a8` ETC/START.EMI#8 | `b1ba0` | 3 (0) | encoding | E-resolve | high | PRIORITY: calls 801b0494 (resolves to PSP 9a80c, the item-87 test); behaviour same, PSP duplicates the SE_Play(0x107) tail into each branch; rescore2 both=0. | psx 801d3554 801d3624 801d3664 / psp b1fc4 b20ac b2048 b20e0 |
| 347 | `801d7824` ETC/START.EMI#8 | `b53e8` | 3 (1) | encoding | E-resolve | high | 801b05b0 resolves to 9b540; SE_Play(0x107) tail duplicated; 468 is a struct offset (s1+0x1d4); rescore2 both=1. | psx 801d7b10 801d7cb4 / psp b56b0 b5838 b5770 |
| 348 | `801d9ac8` ETC/START.EMI#8 | `b6d40` | 3 (3) | encoding | E-fold | medium | Index+12/+11 times 36 folded into field offsets (0x685, 0x1410 are address/offset folds); SE_Play(0x102) block and 6-entry loop match. | psx 801d9c00 801d9c4c 801d9e78..801d9f30 / psp b6f90 b7004..b7098 |
| 349 | `801db650` ETC/START.EMI#8 | `bcabc` | 3 (3) | encoding | E-switch | high | START.EMI copy of row 337: same nested 0x80/0x20 test rewrite and precomputed offsets. | psx 801db79c..801db854 / psp bcc40..bcd5c |
| 350 | `801e1670` ETC/START.EMI#8 | `c01f4` | 3 (3) | encoding | E-bits | high | & 0xfff0 on 0x10-16n (always a multiple of 16) is a no-op; window callee label only. | psx 801e16c8 801e16d4 / psp c0248 c024c |
| 351 | `801e2d04` ETC/START.EMI#8 | `c0c84` | 3 (4) | encoding | E-other | medium | 126-byte local table copy inlined on PSX vs memcpy on PSP (E-inline); PSX also calls Msg_SystemPtr(0xca+n or 0xd0) and discards the result, absent on PSP; no effect if that call is pure. | psx 801e2d48..801e2e10 801e2edc..801e2f00 / psp c0cb4 |
| 352 | `801e2f78` ETC/START.EMI#8 | `c0d98` | 3 (3) | content |  | medium | Status-icon text: PSX draws flags 0x20/0x40/0x10/0x80 in colours 2/1/6/5 at x+4 and flag 0x08 with a second glyph via 8014fd78 at (x+12,y+4); PSP draws all in colour 0, flags 0x04/0x08 at x+5, and drops the second glyph (guess: status abbreviations in the m... | psx 801e3024 801e3050 801e307c 801e30a4 801e3134 / psp c0df4 c0e2c c0eb0 c0f00 |
| 353 | `801e4430` ETC/START.EMI#8 | `c24b4` | 3 (3) | unresolved |  | low | PSX sets state 2 or 3 from flag 0x801ed76c and else calls 801e7588 then 801e6d08(0); PSP always sets 4 and calls the twin of 801e6f80; cannot tell renumbered states from a changed menu flow without the dispatcher. | psx 801e4454..801e4464 801e4470 801e4478 / psp c24d0 c24e8 |
| 354 | `801ce8ac` PLCHAR/PLP278.EMI#0 | `ea6ec` | 3 (1) | encoding | E-resolve | high | 801bed50 resolves to d5b40; 0x74 stride present on both; rescore2 both=1. | psx 801ce980 801ce9e8 / psp ea7b0 ea7f0 |
| 355 | `801d1b10` SCENARIO/SCE15EF1.EMI#0 | `44990` | 3 (1) | platform | P-gpu | high | Only DR_MODE packet length 12->16 remains after resolving 801b0fd0/801b0efc to 9c5e4/9c504; rescore2 both=1. | psx 801d1b6c 801d1b84 801d1b90 / psp 449f0 44a08 44a14 |
| 356 | `801f7670` SCENARIO/SCENA00.EMI#0 | `1741c` | 3 (3) | encoding | E-inline | medium | PSX copies a 3-byte colour from rodata 0x801f6c04, PSP stores immediates 0xc0; 0x138000 and 0xb0000 present on both (lui 0x14 - 0x8000, lui 0xb). | psx 801f7684..801f76a0 801f77b0..801f77c4 / psp 17420 17538..17544 |
| 357 | `801f8d90` SCENARIO/SCENA00.EMI#0 | `178b8` | 3 (2) | platform | P-gpu | medium | Packet lengths change (0xc->0x10, 0x28 on both for the second), inline abs replaced by PLAT abs, 801ad1dc resolves to 8fc28; graph-type branch dropped. | psx 801f8e48 801f8e60 801f904c / psp 1797c 17998 17b64 |
| 358 | `801fb744` SCENARIO/SCENA00.EMI#0 | `26f558` | 3 (3) | encoding | E-switch | high | Compare-chain order differs; 0x2d, 0x1a8000/0x158000 (lui+addiu -0x8000), 0xff7f vs -0x81 are the same values on both. | psx 801fb774 801fb7e8 801fb810 801fb8dc / psp 26f5b8 26f600 26f628 26f6c4 |
| 359 | `801fcec0` SCENARIO/SCENA01.EMI#0 | `276150` | 3 (1) | encoding | E-other | medium | Tail duplication on PSP; 801c26b0 resolves to d7ef0; PSX boot 8015df44 (unpaired) is consistently replaced by PSP 952f0 (labelled twin of 8015de28) here and in rows 36/239/240/376, so 952f0 is probably the real twin of 8015df44. | psx 801fcf68 801fd1ac 801fd188 / psp 276214 276480 2761f8 |
| 360 | `801fc044` SCENARIO/SCENA02.EMI#0 | `278298` | 3 (3) | encoding | E-fold | high | x+(-0x40000) and x+-0x18000/+0x18000 range tests become x-0x40000 and x -/+ (lui 2 - 0x8000); same 152-byte record stride. | psx 801fc070 801fc09c..801fc0c8 / psp 2782dc 278300..27831c |
| 361 | `801fc1a4` SCENARIO/SCENA02.EMI#0 | `2783e0` | 3 (3) | encoding | E-fold | high | Same range-test folding as row 360. | psx 801fc1f4..801fc208 / psp 278434..278440 |
| 362 | `801fc3b4` SCENARIO/SCENA02.EMI#0 | `2785c0` | 3 (3) | encoding | E-fold | high | Same range-test folding as row 360 (0xfffb8000 vs lui 5 - 0x8000). | psx 801fc3cc 801fc414 / psp 278608 |
| 363 | `801fc5d0` SCENARIO/SCENA02.EMI#0 | `2787a4` | 3 (3) | encoding | E-fold | high | Same range-test folding as row 360; event diff shows no differing chunks. | psx 801fc5d0 / psp 2787a4 |
| 364 | `801f7e6c` SCENARIO/SCENA03.EMI#0 | `2954c` | 3 (3) | encoding | E-fold | medium | Struct copied via base+0xc on PSP (fields 52/56/60 become +0/+4/+8 off a moved base, 0x58 -> 0x4c); same 0x21/6 values stored. | psx 801f7ef0..801f7f10 / psp 295dc..29620 |
| 365 | `801fc9dc` SCENARIO/SCENA03.EMI#0 | `280cbc` | 3 (3) | encoding | E-fold | medium | 0x3a8000/0x28000/0x4c8000 written as lui+1 minus 0x8000; (v-0x3c)<2 and (v-0x1f)<2 identical; callee 801fcc9c vs twin of 801f9fd0 with same arg 0x12. | psx 801fca0c..801fca5c 801fca88 / psp 280ce8..280d70 280d40 |
| 366 | `801fcdb4` SCENARIO/SCENA03.EMI#0 | `28110c` | 3 (0) | encoding | E-resolve | high | 801a8ae0 resolves to 8ca44; only the address constant differs; rescore2 both=0. | psx 801fcdcc / psp 28112c |
| 367 | `801f6d44` SCENARIO/SCENA04.EMI#0 | `2b408` | 3 (3) | encoding | E-fold | high | Stores rebased to base+0xc then +0x18 on PSP; 0x568000/0x128000 written as lui+1 - 0x8000. | psx 801f6d48..801f6d80 / psp 2b418..2b454 |
| 368 | `801fa010` SCENARIO/SCENA04.EMI#0 | `282a84` | 3 (0) | encoding | E-resolve | high | 801a8ae0 resolves to 8ca44; rescore2 both=0. | psx 801fa028 / psp 282aa4 |
| 369 | `801fcd68` SCENARIO/SCENA05.EMI#0 | `288364` | 3 (3) | encoding | E-fold | high | Unsigned range tests (x-K)<=W become signed lower/upper bound pairs with identical ranges (e.g. 0x210000..0x258000, 0x930000..0x948000); PSP duplicates tails. | psx 801fcdd0..801fcde4 801fce28..801fce3c / psp 2883d8..2883f0 288474..28848c |
| 370 | `801f88b8` SCENARIO/SCENA07.EMI#0 | `38688` | 3 (3) | encoding | E-resolve | high | 801b0efc/801b0fd0 resolve to 9c504/9c5e4; 24-byte record shift loop restructured. | psx 801f8984 801f89a0 / psp 38758 38774 |
| 371 | `801f8be8` SCENARIO/SCENA07.EMI#0 | `3894c` | 3 (3) | platform | P-gpu | medium | DR_MODE packet 12->16; field offsets rebased (+0x12/+0x2a vs +0x18); polygon loop and rcos/rsin use match. | psx 801f8c5c 801f8c40 / psp 389b0 389d0 |
| 372 | `801fa38c` SCENARIO/SCENA07.EMI#0 | `3a08c` | 3 (6) | content |  | low | PSX transforms a 482-vertex mesh and draws 512 gouraud quads coloured from tables at 0x800e6ff4/0x800e7ff4; PSP draws a 63-quad strip from base+0xe4800 with a procedural colour fade (0x40 down) (guess: a scenario effect rebuilt or simplified; could also be... | psx 801fa43c 801fa5a0..801fa628 / psp 3a104..3a10c 3a1b4..3a200 |
| 373 | `801f8d98` SCENARIO/SCENA08.EMI#0 | `3d86c` | 3 (3) | encoding | E-resolve | high | SCENA08 copy of row 370. | psx 801f8d98 / psp 3d86c |
| 374 | `801f90c8` SCENARIO/SCENA08.EMI#0 | `3db30` | 3 (3) | platform | P-gpu | medium | SCENA08 copy of row 371: DR_MODE packet 12->16, rebased field offsets. | psx 801f90c8 / psp 3db30 |
| 375 | `801fd704` SCENARIO/SCENA08.EMI#0 | `294acc` | 3 (5) | encoding | E-resolve | high | 801c2de8/801c0e2c/801c8a4c/801a8900 resolve to d8270/d6f20/143f8c/8c84c; 0xfd vs -3 same mask; remaining 0xf0000 is a split constant. | psx 801fd8d0 801fdbb4 801fdbe4 801fdf58 / psp 294c78 294ea8 294ed0 2951b0 |
| 376 | `801f9498` SCENARIO/SCENA09.EMI#0 | `29789c` | 3 (3) | encoding | E-other | medium | Tail duplication (Msg_OpenScript, 8e7e0 copies); 801c26b0 resolves; PSX 8015df44 replaced by PSP 952f0 as in row 359. | psx 801f9498 / psp 29789c |
| 377 | `801f9fc4` SCENARIO/SCENA10.EMI#0 | `29c300` | 3 (0) | encoding | E-resolve | high | 801a5d5c resolves to 89cec; the !=0xff guard exists on both; rescore2 both=0. | psx 801f9fe8 801f9ff8 / psp 29c324 29c32c |
| 378 | `801fe5b0` SCENARIO/SCENA10.EMI#0 | `2a1664` | 3 (1) | encoding | E-resolve | high | andi 0xfe vs and -2 same; 801c2628 resolves to d7e34; rescore2 both=1. | psx 801fe5e4 801fe5fc / psp 2a1698 2a16ac |
| 379 | `801f75ec` SCENARIO/SCENA12.EMI#0 | `4b154` | 3 (2) | encoding | E-other | high | Only the window callee label differs (PSX 801f765c vs PSP twin of 801f3da0); rescore2 both=2. | psx 801f760c / psp 4b170 |
| 380 | `801fb464` SCENARIO/SCENA13.EMI#0 | `2ac754` | 3 (3) | encoding | E-switch | medium | (x+-6)<2 range tests become ==6//==7; range constants split lui+1 - 0x8000; tail duplication; Flag_Test and 8e810 calls moved, not added. | psx 801fb464 / psp 2ac754 |
| 381 | `801f924c` SCENARIO/SCENA14.EMI#0 | `2ad4b4` | 3 (3) | encoding | E-fold | high | Stores 0x63ff after testing ==0x77ff; PSP stores x-0x1400 after the same test (0x77ff-0x1400 = 0x63ff). | psx 801f9258 801f9268 / psp 2ad4d0 2ad4d4 |
| 382 | `801f71b0` SCENARIO/SCENA15.EMI#0 | `26f0c` | 3 (6) | platform | P-gpu | medium | (x-11)&0xff<17 and (x-8)<16 become signed bound pairs 11..27 and 8..23 (E-fold); 801b0efc resolves to 9c504; the remaining difference is DR_MODE packet 12->16. | psx 801f73c4..801f73dc 801f7484 / psp 270e4..270f4 27180..2718c |
| 383 | `801f7e2c` SCENARIO/SCENA15.EMI#0 | `512dc` | 3 (3) | encoding | E-fold | high | PSX masks the word at +0x5c with 0xffff00 and compares to 0x307000; PSP tests bytes +0x5d==0x70 and +0x5e==0x30; andi 0xf7 vs and -9 is the same bit clear | psx 801f7f14..801f7f2c / psp 51390..513a8; psx 801f7ef0 / psp 5137c |
| 384 | `801fdb84` SCENARIO/SCENA15.EMI#0 | `2b2320` | 3 (2) | encoding | E-div | high | Rand()%38+42 then /2 on both sides: PSX by reciprocal 0x6bca1af3 (the 19 is 38/2 in the multiply-back), PSP by div 0x26 | psx 801fdbac..801fdbf0 / psp 2b2350..2b236c |
| 385 | `801f8818` SCENARIO/SCENA17.EMI#0 | `2b812c` | 3 (2) | encoding | E-ds | high | same two calls 8015d8ac and 8014b848(ptr); 0x4e1c is the PSP address low half in the jal delay slot | psx 801f8864..801f8874 / psp 2b8174..2b8184 |
| 386 | `801f8b58` SCENARIO/SCENA17.EMI#0 | `2b8abc` | 3 (3) | platform | P-gpu | medium | PSX picks tpage 0x100/0x40 by GetGraphType and commits a 12-byte DR_MODE, PSP fixed 0x40 and 16 bytes; primitive code bytes 5/6 dropped; loop counter by 0x80 on PSP (E-loop) | psx 801f8c34..801f8c98 / psp 2b8b8c..2b8bac; psx 801f8d2c,801f8e54 / psp 2b8d80 |
| 387 | `801f8ff4` SCENARIO/SCENA17.EMI#0 | `2b83e8` | 3 (3) | platform | P-gpu | medium | GetGraphType tpage choice (0x2ad/0xbd) removed, DR_MODE packet 12->16 and sprite packet 20->32, CLUT 0x7b40 vs 0xd00 (VRAM layout of the PSP renderer) | psx 801f902c..801f9078,801f90b8 / psp 2b843c..2b844c,2b8488 |
| 388 | `801f954c` SCENARIO/SCENA17.EMI#0 | `2b8908` | 3 (1) | platform | P-file | medium | PSX File_LoadRequest(0x266) 801629cc (unpaired) vs PSP 970fc (twin of PSX 8016345c), same file id and same File_LoadDone poll; 801c26b0 pairs with d7ef0 (resolve fix) | psx 801f9630 / psp 2b89f0; psx 801f958c / psp 2b8968 |
| 389 | `801f96c4` SCENARIO/SCENA17.EMI#0 | `2b8a80` | 3 (2) | encoding | E-ds | high | same calls 8015d8ac and 8014b848(ptr); 0x4e1c is a PSP address low half in a jal delay slot | psx 801f96ec..801f96fc / psp 2b8a9c..2b8aac |
| 390 | `801f2cec` WORLD00/AREA013.EMI#13 | `22258c` | 3 (1) | encoding | E-resolve | high | 0x1c8000 vs 0x1d0000-0x8000 is the same bound; PSX inline abs vs PSP call to abs 2c553c; PSX GAME.EMI call 801acba8 is PSP 8f78c (resolve defect) | psx 801f2d04,801f2d1c..801f2d3c,801f2d70 / psp 2225a4,2225c8,222600 |
| 391 | `801f3b00` WORLD00/AREA016.EMI#13 | `22408c` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding | psx 801f3b30..801f3b7c / psp 2240dc..2240fc; psx 801f3bac / psp 224124; psx 801f3bb8 / psp 224158 |
| 392 | `801f31cc` WORLD00/AREA027.EMI#13 | `606b4` | 3 (1) | encoding | E-fold | high | coordinates 0x468000/0x478000/0x498000 built as lui+addiu -0x8000 on PSP and spilled to stack; all calls and values match | psx 801f3240..801f3294 / psp 60700..60798 |
| 393 | `801d2108` WORLD00/AREA030.EMI#4 | `18dd8` | 3 (3) | encoding | E-div | high | /255 and /15 by reciprocal (0x80808081, 0x88888889) on PSX vs div on PSP; bounds 0x14ffff/0x3bffff vs slt 0x150000/0x3c0000; andi 0xdf vs and -33 | psx 801d2174..801d2180 / psp 18e64,18e84; psx 801d2220 / psp 18ee0 |
| 394 | `801d23f0` WORLD00/AREA030.EMI#4 | `19050` | 3 (3) | encoding | E-fold | high | bounds expressed as <=0x3bffff/0x14ffff vs <0x3c0000/0x150000; PSP duplicates the Actor_UpdateScreenPos tail call that PSX reaches by a jump | psx 801d23fc,801d2428 / psp 19064,19088; psx 801d25d4 / psp 19120,19218 |
| 395 | `801d3a50` WORLD00/AREA030.EMI#4 | `1a20c` | 3 (3) | content | C-mode | high | PSP-only mode hook (0x11d48c): window x 0x125 becomes 0x101 (-36) while the mode runs; the PSX path otherwise; what the mode is in play (a PSP-only fishing mode) is a guess; rest is E-fold | psx 801d3c0c / psp 1a21c..1a230,1a39c |
| 396 | `801d469c` WORLD00/AREA030.EMI#4 | `1b178` | 3 (3) | content | C-mode | medium | PSP adds a fourth fishing-menu row: cursor (+6)==3 sets state +1 to 0x11, PSX only handles cursor 1 (advance) and 2 (state 0xc); also window x 0x14 -> -0x10 under the PSP-only mode hook (0x11d48c); the cursor==3 case itself is not gated (reachable only if t... | psx 801d4880..801d488c / psp 1b370..1b378; psx 801d46d8 / psp 1b18c..1b1a8 |
| 397 | `801d5064` WORLD00/AREA030.EMI#4 | `1bbe0` | 3 (3) | content | C-mode | medium | same fishing-menu fourth row as row 396: cursor==3 -> state 0x11 added on PSP (PSX: 0 -> 2, 2 -> advance), window x -16 under the PSP-only mode hook (0x11d48c); the cursor==3 case itself is not gated (reachable only if the menu offers a fourth row, presumab... | psx 801d52c8..801d52dc / psp 1be58..1be60; psx 801d5084 / psp 1bbf4..1bc10 |
| 398 | `801d5590` WORLD00/AREA030.EMI#4 | `1c108` | 3 (3) | content | C-mode | medium | same fishing-menu fourth row: cursor==3 -> state 0x11 added on PSP (PSX: 0 -> 2, 2 -> 6), window x -16 under the PSP-only mode hook (0x11d48c); the cursor==3 case itself is not gated (reachable only if the menu offers a fourth row, presumably in that mode:... | psx 801d56f4..801d56fc / psp 1c294..1c29c; psx 801d55cc / psp 1c118..1c130 |
| 399 | `801d9d24` WORLD00/AREA030.EMI#4 | `de418` | 3 (2) | content | C-mode | high | PSP-only mode hook (0x11d48c): stores the results of PSP-only getters 21cd0c/21cad8 to a PSP-only struct (+0x3b4/+0x3b8) and sets its +0x18 flag; the PSX path otherwise; mode meaning in play is a guess | psx 801d9d74 / psp de460..de488 |
| 400 | `801d9ec8` WORLD00/AREA030.EMI#4 | `df08c` | 3 (3) | content | C-mode | high | PSP-only mode hook (0x11d48c, plus 210158 on 0x11b310+0x10): early exit that sets 0x80144121=7 and clears state +3/+4 without reading the pad; the PSX path otherwise (pad switch itself is E-switch); mode meaning in play is a guess | psx 801d9ec8 / psp df0a0..df168 |
| 401 | `801da80c` WORLD00/AREA030.EMI#4 | `df98c` | 3 (3) | encoding | E-fold | high | PSX range test x-0x60001 <u 0xefffe vs PSP 0x60000<x<0x150000: same range | psx 801da920..801da934 / psp dfa74..dfa8c |
| 402 | `801dd9f8` WORLD00/AREA030.EMI#4 | `e1ce4` | 3 (3) | content | C-mode | high | PSP-only mode hook (0x11d48c, plus 210158): takes the pad-0x40 path (0x80150000-0x6cd6 = 0x40, 0x3c0000 store, state +3 = 1) as if the button were pressed; the PSX path otherwise; line loop rewritten (E-loop); mode meaning in play is a guess | psx 801dda38..801dda90 / psp e1d20..e1d9c; psx 801ddb0c / psp e1de0 |
| 403 | `801e1378` WORLD00/AREA030.EMI#4 | `e3a14` | 3 (0) | encoding | E-inline | high | PSX copies two initialised string tables (0x34 and 0x1a bytes) with lwl/lwr loops, PSP calls a memcpy-like helper (annotated printf); sprintf format address in delay slot; field offsets are stack layout | psx 801e13b4..801e1408 / psp e3a50,e3a64; psx 801e15f8 / psp e3b94 |
| 404 | `801e17b0` WORLD00/AREA030.EMI#4 | `e3d3c` | 3 (5) | encoding | E-fold | high | PSX reads halfwords +0x7a/+0x7c/+0x7e of a base+0x10000 pointer, PSP adds 0x1007a etc. to the 0x790 base; all calls match in order | psx 801e19bc / psp e3ec4..e3f58 |
| 405 | `800f550c` WORLD00/AREA030.EMI#5 | `e93bc` | 3 (3) | encoding | E-switch | high | pad compare chain re-ordered: 0x1000->+10, 0x2000->+1, 0x4000->-10, 0x8000->-1 on both; clamp 1..99 same; andi 0xbf vs -65 | psx 800f553c..800f5590 / psp e9414..e9450; psx 800f55b8..800f55d0 / psp e9468..e9480 |
| 406 | `800f5828` WORLD00/AREA030.EMI#5 | `e7aac` | 3 (3) | encoding | E-other | high | branch/tail layout differs; PSX has the same SE_Play(0x106) and SE_Play(0x105) paths, andi 0xbf vs -65, 0x10094 is base+0x10000+0x94 on PSP | psx 800f58c4,800f58ec / psp e7b50,e7b6c; psx 800f5a70 / psp e7cd4 |
| 407 | `801f3a34` WORLD00/AREA033.EMI#13 | `2279b8` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding (same... | psx 801f3a64..801f3ab0 / psp 227a08..227a28 |
| 408 | `801f3508` WORLD00/AREA037.EMI#13 | `228c78` | 3 (3) | mispair | M-swap | high | AREA037 #13 helper (if byte 0x80150c3 is set: message index 0x60 and 0x801d4291 = 6, else 0xffff). Its true twin is PSP 228b60, the twin the table gives row 419, and that pair is identical; the paired 228c78 stores 0x88 or 0x86 and is another area copy of t... | psx 801f3514..801f352c / psp 228b6c..228b88 |
| 409 | `801f2c04` WORLD01/AREA040.EMI#13 | `2297d8` | 3 (0) | encoding | E-resolve | high | PSX GAME.EMI call 801a5d5c is PSP 89cec (resolve defect); 0x5024 is the overlay table low half in the delay slot | psx 801f2c38 / psp 229808 |
| 410 | `801f4288` WORLD01/AREA041.EMI#13 | `22aeb8` | 3 (1) | encoding | E-fold | high | table index base shifted by -0x3b on PSP and Flag_Set block expressed as base+0x650; same flag, same SE 0x201 | psx 801f4294..801f42a8 / psp 22aec4..22aee4 |
| 411 | `801f5070` WORLD01/AREA042.EMI#13 | `22b5d4` | 3 (1) | encoding | E-resolve | high | PSX GAME.EMI calls 801a8338 and 8019701c are PSP 8b6c8 and 3100 (resolve defect); 0x108000 as lui 0x11 -0x8000 | psx 801f513c,801f5158 / psp 22b698,22b6b0; psx 801f51a4 / psp 22b708 |
| 412 | `801f3b00` WORLD01/AREA045.EMI#13 | `22e6d0` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding (same... | as row 391 |
| 413 | `801f37f4` WORLD01/AREA048.EMI#13 | `62cd0` | 3 (0) | encoding | E-fold | high | 0x858000/0x878000 built as lui 0x86/0x88 + addiu -0x8000 | psx 801f37fc,801f3814 / psp 62cd4,62cec |
| 414 | `801f3874` WORLD01/AREA048.EMI#13 | `62d34` | 3 (1) | encoding | E-fold | high | n*16 written as (n<<12)/256 on PSP, (n+0x80)*16 as ((n<<12)+0x80000)/256; a2=1/16*(n>=49) same; constants are lui/addiu splits | psx 801f3890..801f38d4 / psp 62d48..62dcc |
| 415 | `801f3944` WORLD01/AREA048.EMI#13 | `62e18` | 3 (0) | encoding | E-fold | high | 0x858000/0x878000 as lui+addiu -0x8000; otherwise identical | psx 801f394c,801f3964 / psp 62e1c,62e34 |
| 416 | `801f3aa4` WORLD01/AREA048.EMI#13 | `62f38` | 3 (0) | encoding | E-fold | high | 0x848000/0x888000 as lui 0x85/0x89 + addiu -0x8000 | psx 801f3ab0,801f3ac4 / psp 62f3c,62f54 |
| 417 | `801f3414` WORLD01/AREA052.EMI#13 | `230a24` | 3 (1) | encoding | E-resolve | high | PSX GAME.EMI calls 801a5bc8 and 801acba8 resolve to PSP 89a0c and 8f78c; boot 80167628 pairs with dce04 as in row 391; byte +0x19 read as (word+0x18 & 0xff00)>>8 | psx 801f3484..801f3488 / psp 230a94..230aa4; psx 801f3578 / psp 230b74 |
| 418 | `801f3b38` WORLD01/AREA065.EMI#13 | `234a68` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding (same... | as row 391 |
| 419 | `801f3930` WORLD01/AREA068.EMI#13 | `228b60` | 3 (3) | mispair | M-swap | high | AREA068 #13 helper (if the byte is set, 0x801d4292 = 3; then 0xffff). Its true twin is PSP 228b08 (sb 3 to raw 0x11b6dd, then 0xffff), identical to the PSX. The neighbour 228b34 holds the 5 variant (PSX 801f3960). The paired 228b60 is row 408s twin, so stag... | psx 801f393c..801f3954 / psp 228b10..228b30 (psp_elf.py dis 0x228af0 0x228b60) |
| 420 | `801f32fc` WORLD01/AREA075.EMI#13 | `236a8c` | 3 (1) | encoding | E-resolve | high | PSX GAME.EMI call 801a5d5c is PSP 89cec; 0x30000 hoisted into a register | psx 801f3358 / psp 236b00 |
| 421 | `801f3d24` WORLD02/AREA077.EMI#14 | `39bd8` | 3 (4) | encoding | E-resolve | medium | PSX GAME.EMI 80197070 pairs with PSP 3154 (3100/3154 sit 0x54 apart like 8019701c/80197070); 0x800e5502 is base(0x790)+0xe4800+0xd02; local callees 801f4220/801f43c8 unpaired vs 3a640/3a08c in the same slots | psx 801f3d54 / psp 39c20; psx 801f3dec / psp 39ca0; psx 801f3d9c,801f3da4 / psp 39c5c,39c64 |
| 422 | `801f3e48` WORLD02/AREA077.EMI#14 | `39cfc` | 3 (3) | encoding | E-fold | high | 0x738000/0xb8000 as lui+addiu -0x8000; 0x800e5504 as base+0xe5504, stride 0x2c loop same | psx 801f3e4c..801f3e60 / psp 39d0c..39d24; psx 801f3ec8 / psp 39d6c |
| 423 | `801f3f08` WORLD02/AREA077.EMI#14 | `39dc8` | 3 (5) | platform | P-gpu | medium | DR_MODE packet 12->16 on PSP; otherwise E-fold (0x800e5504 as base+0xe5504) and E-resolve (80197070 -> 3154) | psx 801f3f64 / psp 39e14; psx 801f4010 / psp 39ebc |
| 424 | `801f3264` WORLD02/AREA082.EMI#13 | `28e8c` | 3 (3) | platform | P-gpu | medium | DR_MODE packet 12->16; local callee 801f2f50 (stack args) vs 28f38 (register args t0/t1) is the same draw call | psx 801f32a0 / psp 28ed0; psx 801f32ec / psp 28f20 |
| 425 | `801f3aac` WORLD02/AREA082.EMI#13 | `2ccac` | 3 (3) | encoding | E-fold | high | loop over 128 records of 20 bytes at 0x800e4800 (PSP base 0x790+0xe4800); local callee unpaired on PSX | psx 801f3ad8..801f3b38 / psp 2cce0..2cd34 |
| 426 | `801f373c` WORLD02/AREA085.EMI#13 | `2397b8` | 3 (1) | encoding | E-resolve | high | PSX GAME.EMI call 801c6df4 is PSP 106fec; -0x160000 folded differently | psx 801f3768 / psp 2397ec |
| 427 | `801f3b34` WORLD02/AREA087.EMI#13 | `23b6cc` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding (same... | as row 391 |
| 428 | `801f2ce4` WORLD02/AREA088.EMI#13 | `4ce0c` | 3 (2) | encoding | E-fold | high | 0x800e4800 as base(0x790)+0xe4800; field +9 via addiu; same SE and calls | psx 801f2d10..801f2d24 / psp 4ce44..4ce64 |
| 429 | `801f2f28` WORLD02/AREA088.EMI#13 | `4d200` | 3 (3) | encoding | E-resolve | medium | PSX GAME.EMI calls 801b0efc/801b0fd0 are PSP 9c504/9c5e4; record shift loop and 0x300 list compaction re-scheduled; local 801f35b0 vs 4d048 | psx 801f2ff4,801f3010 / psp 4d2d0,4d2ec; psx 801f3200 / psp 4d464 |
| 430 | `801f4664` WORLD02/AREA088.EMI#13 | `23d64c` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding (same... | as row 391 |
| 431 | `801f2c48` WORLD02/AREA098.EMI#13 | `428b4` | 3 (2) | platform | P-gpu | medium | PSX 8014e458 (twin 3708) replaced by PSP-only a2c2c (43 insns) with a VRAM rect 0x340,0x100,0x80,0x100; party record copy (0x80145e8c + 320*n) is E-inline; 80197070 -> 3154 E-resolve | psx 801f2cc8 / psp 42944; psx 801f2e4c / psp 42ab4 |
| 432 | `801f42e8` WORLD02/AREA098.EMI#13 | `43dbc` | 3 (5) | platform | P-gpu | medium | DR_MODE packet 12->16; 0x800e4800 list as base+0xe4800, stride 0x38 loop same | psx 801f4354 / psp 43e14; psx 801f4454 / psp 43f50 |
| 433 | `801f48e8` WORLD02/AREA098.EMI#13 | `44f9c` | 3 (3) | encoding | E-resolve | high | PSX GAME.EMI 801b0efc is PSP 9c504 (resolve defect); -0x10000/-0x4000 steps folded | psx 801f4934,801f4970 / psp 44ff8,4502c; psx 801f4b74 / psp 45210 |
| 434 | `801f4dbc` WORLD02/AREA098.EMI#13 | `4438c` | 3 (3) | encoding | E-div | high | Rand()%768 by reciprocal 0x2aaaaaab on PSX, div 0x300 on PSP; -0x20000 as lui 0xfffe vs subu lui 2 | psx 801f4e24..801f4e54 / psp 44408..44410 |
| 435 | `801f5308` WORLD02/AREA098.EMI#13 | `448f0` | 3 (5) | encoding | E-fold | high | 8 records of 24 bytes at 0x800e4800 via base+0xe4800; local callee unpaired on PSX | psx 801f532c..801f5384 / psp 4490c..44970 |
| 436 | `801f2f44` WORLD02/AREA102.EMI#13 | `6b4a0` | 3 (3) | encoding | E-loop | medium | (x-128)<<7 strength-reduced to (x<<7)-0x4000, loop invariants hoisted (0x380000, 0x400000); division forms differ; no game value found to differ | psx 801f2fb0..801f2fc4 / psp 6b4c4..6b4e0; psx 801f3058..801f3098 / psp 6b5c4..6b618 |
| 437 | `801f36b4` WORLD02/AREA102.EMI#13 | `6bbb8` | 3 (3) | encoding | E-loop | medium | same strength reduction as row 436 (sibling function in AREA102) | as row 436 |
| 438 | `801f2e5c` WORLD02/AREA103.EMI#13 | `3043c` | 3 (4) | platform | P-gpu | medium | DR_MODE packet 12->16; 0x800e4800 records (stride 0x4bc) via base+0xe4800 | psx 801f2eb0 / psp 30488; psx 801f2f34 / psp 30520 |
| 439 | `801f3bc4` WORLD02/AREA104.EMI#14 | `244c6c` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding (same... | as row 391 |
| 440 | `801f66c8` WORLD02/AREA104.EMI#14 | `244bd8` | 3 (3) | encoding | E-switch | high | tile attribute -> class switch (0x50,0x10->2; 0x40->1; 0,0xaf,0xc0->0; else 3) identical as compare tree vs flat chain; 80167628 pairs with dce04 | psx 801f66e4..801f6758 / psp 244bf4..244c5c |
| 441 | `801f35f4` WORLD02/AREA111.EMI#13 | `2479b8` | 3 (1) | encoding | E-resolve | medium | PSX GAME.EMI 801ad768 is PSP 8fd64; PSP derives the character index by (ptr-base)/320 where PSX indexes directly; nibble table lookup identical | psx 801f3700 / psp 247a94; psx 801f360c..801f3630 / psp 2479c4..247a48 |
| 442 | `801f39e0` WORLD02/AREA111.EMI#13 | `247cf4` | 3 (1) | encoding | E-resolve | medium | as row 441 (801ad768 -> 8fd64, index by /320) | psx 801f3b48 / psp 247e48 |
| 443 | `801f2ea8` WORLD02/AREA112.EMI#13 | `6ce48` | 3 (3) | encoding | E-fold | medium | (x!=2 ? 0x11 : 0) as -(x!=2)&0x11 vs n+16n; /32 rounding same; 0x80104000 buffer via base+0x104000 | psx 801f2f60..801f2f6c / psp 6cefc..6cf10; psx 801f31bc / psp 6d0c0 |
| 444 | `801f2dd8` WORLD03/AREA114.EMI#13 | `249854` | 3 (3) | mispair |  | medium | PSX: if 80164890() and mode!=2, call 80162764/8015ca48 and set +0x7c=9; PSP: calls 8015ca78, sets 0x801448e7=0x3e and +0x7c=5; different callees and values; twin not found | psx 801f2dd8..801f2e34 / psp 249860..249880 |
| 445 | `801f3b04` WORLD03/AREA115.EMI#13 | `24b278` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding (same... | as row 391 |
| 446 | `801f2e08` WORLD03/AREA117.EMI#13 | `64298` | 3 (3) | encoding | E-inline | high | PSX inline abs() of x-0x28000 etc., PSP calls abs 2c553c; bounds 0x150000/0x160000 same | psx 801f2e48..801f2e70 / psp 642f8..64324 |
| 447 | `801f381c` WORLD03/AREA118.EMI#13 | `661a8` | 3 (4) | platform | P-gpu | medium | GetGraphType tpage (0x225/0x95) removed and DR_MODE 12->16; (x-128)<<7/0x40 folded to (x<<7)-0x3fc0 | psx 801f3844..801f3890 / psp 661dc..661fc; psx 801f38d4 / psp 66238 |
| 448 | `801f3e7c` WORLD03/AREA120.EMI#13 | `847f8` | 3 (3) | encoding | E-fold | high | word & 0xffff00 == 0x030600 vs bytes +1==6 and +2==3; PSX inline abs vs PSP abs call | psx 801f3e80..801f3ea8 / psp 8480c..8482c |
| 449 | `801f3f5c` WORLD03/AREA120.EMI#13 | `848f4` | 3 (3) | platform | P-gpu | medium | GetGraphType tpage (0x225/0x95) removed; 801564c4 argument 0xc vs 0x10 (packet size); (x-128)<<7+0x10 folded | psx 801f40ac..801f40dc,801f410c / psp 84a34,84a50 |
| 450 | `801f3b18` WORLD03/AREA121.EMI#14 | `24fbc8` | 3 (4) | platform | P-gpu | medium | PSX picks the DR_MODE tpage by GetGraphType (two Psy-Q calls) and commits a 12-byte packet; PSP uses one fixed tpage and a 16-byte packet; boot 80167628 pairs with PSP dce04 consistently (tile attribute getter); rest is compare-chain/loop re-encoding (same... | as row 391 |
| 451 | `801f2cac` WORLD03/AREA133.EMI#13 | `26d328` | 3 (3) | mispair |  | medium | PSX clears 0x8014865b, calls 8015ca78, sets 0x80146871=30 and 0x80146870=4; PSP sets message id from a table indexed by 0x801490c3 and only on 1 calls 8015ca78 and sets 0x80146870=1; twin not found | psx 801f2cac..801f2cd8 / psp 26d328..26d374 |
| 452 | `801f49ec` WORLD03/AREA135.EMI#14 | `253048` | 3 (1) | encoding | E-resolve | high | PSX GAME.EMI 801acba8 is PSP 8f78c; -0xc8000 vs +0xd0000-0x8000 same | psx 801f4ab0 / psp 2530ec; psx 801f49fc / psp 253088 |
| 453 | `801f4c18` WORLD03/AREA135.EMI#14 | `253208` | 3 (1) | encoding | E-resolve | high | 801acba8 -> 8f78c; -0x1c8000 as subu of 0x1d0000-0x8000 | psx 801f4cc0 / psp 25328c; psx 801f4c38 / psp 253224 |
| 454 | `801f4d0c` WORLD03/AREA135.EMI#14 | `2532d0` | 3 (1) | encoding | E-resolve | high | 801acba8 -> 8f78c; 0x148000 folded | psx 801f4d60 / psp 25331c |

## 5. Not verified

- **Nothing was seen in play.** Every "guess" above is one. What the PSP-only
  mode is, where map 92 and areas `0x2b`, `0x69` and 70 are, and whether the
  wider graphic or the uncoloured status text shows at all are for the owner
  or a later read of the data.
- **The readers' calls rest on their notes.** This doc's author re-read the
  content rows, the mode's setter and launch, and rows 408 and 419. The other
  encoding and platform rows were not re-read; their confidence column is the
  reader's. The share read before the mid-read notes (rows 41..57) did not
  apply the `res` correction, so a few of its encoding rows may carry another
  tag than `E-resolve`.
- **The mode's own code** (`0x20f000`..`0x220000`, about 64 KiB of PSP-only
  functions) was not read beyond its loop and its menu entry. The second
  PSP-only flag (`0x21f418`, raw `0xc4478`) and its setter were not traced.
- **The 25 mispairs and 5 unresolved rows** are not diffed against their true
  twins, except the six of section 2.2, rows 408 and 419, and the parts the
  readers named in section 2.3. A content change may hide in a PSX function
  whose twin was not found: the leading functions of the merged extents,
  rows 35, 55, 128, 207, 444 and 451.
- **Row 231's message byte** (`801e6020`): it was not checked whether the PSX
  ever sets it to anything but `0x18`. If it does not, the row is encoding.
- **Row 372** may be a mispair whose prologue happens to match.
- **PSP-EU** was compared only for the content rows' functions (all identical
  to PSP-JP by masked words); the PSX side is JP only, as in stage A.
- **The `res` correction's own precision:** it resolves only into the calling
  file's other sections and the resident module, and only to a known function
  start. Calls into other overlays stay as before.

## 6. For the other files (the coordinator's)

**Folded in 2026-10-12** (the coordinator's pass, branch `psp/ledger`): The README row is in.

- **`docs/README.md`, one row:** `psp-code-diff-field-1.md` | Stage B of the
  PSP code diff, field half rows 1..454 (2026-10-12): 264 encoding, 127
  platform, 33 content, 25 mispairs, 5 unresolved. The content is 11 single
  changes (two event triggers, a map-92 party placement, an area-70 tint skip,
  a capped frame counter, a shop prompt, five drawing changes) and 22 hooks of
  a PSP-only mode launched from a new menu choice and run in AREA030. Nothing
  touches `exp_table`, `RYUD`, consumable 87 or the button-label page. Also
  measures three normaliser defects and adds `tools/psp_pair_read.py`.
- **No ledger entry.** These are the PSP's differences, not ours. A
  PSP-content option that adopted one would want its own entry then.
- **For SI and IM:** the consumables' readers here (rows 217, 263, 346) read
  the same records and fields on both builds. The item-87 test is reached
  through the same calls.
- **For CD1 and CD3:** section 2. The `res` defect hides added calls into
  GAME.EMI or BATTLE.EMI, and the `ds` defect makes addresses look like
  values. `psp_pair_read.py rescore` gives every row of both halves its score
  with both corrected.

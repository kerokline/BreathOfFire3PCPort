# The PSP code diff, stage B: field, menus, items and system, rows 455..908

**Status:** MEASURED (2026-10-12) - stage B of group CD of the PSP-only round
([`psp-code-diff.md`](psp-code-diff.md), whose section 7 table this reads),
group CD3: the lower-scoring half of the field table (rows 455..908, scores 2
and 3; CD2 has rows 1..454, CD1 the battle half). Read-only: nothing in
`src/`, the importer or `tools/psp_pair.py` changed. Nothing here has been
seen in play; every sentence about what the player would see is a guess.

## 0. The answer

Of the 454 rows: **9 content, 85 platform, 360 encoding**.

| class | rows | how decided |
|---|---|---|
| content | 9 | all read |
| platform | 85 | 30 read, 55 by rule (a GPU or video-mode signature, below) |
| encoding | 360 | 90 read, 270 by rule (every difference re-matched; below) |

The content changes, all present the same way in PSP-EU (checked by
disassembling the EU twins of 733, 677 and 576 with `eudis.py`):

| # | PSX (file) | PSP-JP / EU | what changes | suite | evidence |
|---|---|---|---|---|---|
| 733 | `801f3960` WORLD01/AREA068 #13 | `228c08` / `228a20` | When the message-box byte `0x801490C3` is set, the PSX writes 5 to the master-state byte `0x801D4292` (a SISYOU address: `gref.py 4292 b6dc` finds its stores in SISYOU and in this overlay) and resets the message index `0x801490A4` to `0xFFFF`; the PSP writes **6** and sets the message index to **116**, then returns. Its neighbour `801f3930` (CD2's row) does the same with 3 -> 6 and index 96. Which line the player then sees is the owner's to say. | data | disassembly, row 733 (section 2.1) |
| 675, 686, 687, 688 | `801d1cf0`, `801da1e8`, `801daf98`, `801dbc2c` WORLD00/AREA030 #4 | `1e220`, `df4a8`, `dfc50`, `e0440` | AREA030 (`names/places.toml`: a fishing place) tests PSP-only state through `0x2100fc` (returns the byte at `.bss 0x11d48c`), `0x210158` and `0x21f418`, and runs added branches when it is set. The byte is cleared in `Boot_Init`'s twin and by two PSP-only functions, and set by the 490-instruction PSP-only `0x20f4d4`, which no code calls directly and which calls the item code (`0x9a80c`, the item-87 function's twin, and `0xb8d08`). | data | `pspref.py addr 11d48c`, `pspref.py call 2100fc` (76 call sites, most in AREA030's twins) |
| 677 | `801d4200` AREA030 #4 | `1a920` / `1a7a4` | The same byte switches the last argument of a text-draw call (`801d978c`'s twin) from 29 to -7. | graphics | row 677 |
| 685 | `801d9c24` AREA030 #4 | `de2dc` | The area's init clears more state on the PSP, including `0x11b30a`, which only PSP-added AREA030 code reads (`pspstate.py`): the init of the addition above. Probable, not traced further. | data | row 685 |
| 678 | `801d6144` AREA030 #4 | `1cafc` | A draw position argument moves by one: 228 -> 229 (`801d1818`'s twin, the first argument). | graphics | `ctx.py 678 psx 0x78` |
| 576 | `801e6d98` ETC/START #8 | `c3298` / `c3090` | A title-screen label sprite: the PSX picks its width from two values by the index argument; the PSP copies a 20-byte `.rodata` table and takes (v, width) for five indices from it. Five label widths where the PSX has two reads as different label art (guess). Packet lengths also differ (platform). | graphics | row 576; the table read in scratch, not reproduced |

None of the nine is in [`DIVERGENCE.md`](DIVERGENCE.md)'s sense ours: they
are what the PSP did, for the round to adopt or not.

**The priority data.** Only one of the priority readers is in this share:
COMMU04's consumables reader `801d12c8` (row 506) is **encoding** - its one
scored difference, an `andi 0xfffe` on the PSX, masks a value
(`0x12 - 12n`) that is always even. No row here tests item 87, reads
`exp_table`, the button-label page or `RYUD`. Rows 460 and 461 (AREA151/152)
test the tile code `0xAE` (the value the item-87 code compares against) the
same way on both builds. One PSP-only function outside the table, `0x20f4d4`
(above), calls the item-87 function's twin `0x9a80c`; what it does with it is
not read.

**What the rows mostly are.** Stage A's normaliser misses a few compiler
habits and one defect of its own, which together explain almost every row:

- **156 of the 454 rows call a function outside their overlay (GAME.EMI,
  mostly) that stage A resolved to an empty boot-EXE key.** `Psx.resolve` tries the boot range
  (`code_lo..code_hi`, which reaches past `0x801EEC30` because the sibling's
  `functions.tsv` lists such addresses) before the resident module, so an
  overlay's call into GAME.EMI lands on a zero-filled `('boot', addr)`
  function that has no twin. Rows 482-484 and 905-908 *are* such keys,
  paired by caller vote; against the GAME.EMI copy at the same address each
  re-matches clean (`pairchk.py 9d00 <addr> <psp>`), except 908, whose
  address holds no field-family code in the captures. Not fixed here (a fix
  changes the resolve order, so it is not additive); measured by
  `explain.py` comparing calls by address.
- **The flag array through a base.** The PSP forms `Flag_Test`'s first
  argument as `0x100000 - 0x5e70` (the twin of `0x801448D4`) plus `0x650`,
  `0x5f8`, `0x5c8` or `0xc77` in the jal's delay slot: CD1's finding (the
  relocated low half read as a constant). 24 rows.
- **Callee twins stage A did not pair** (42 rows): overlay helpers, the
  sequence-play wrapper `8015dcf0` = `95118`, the scratchpad-pointer clear
  `80197070` = `3154`; each checked with `pairchk.py` (same constants,
  similar size).
- **Library twins mispaired.** `SetDrawMode` `0x8017cc50` is paired with
  `0x9c7b0`, which is `rsin`'s twin (PSX rows calling `rsin` call `9c7b0` on
  the PSP; it holds pi, 4096.0 and 1/4096 as floats); `GetGraphType`
  `0x8017bc2c` is paired with `0x9fd54`, which builds a GPU command word
  (`lui 0xe400`) and reads no video mode; PSX `SetDrawMode` calls line up
  with PSP calls to it. The tile getter `80167628` is
  paired with nothing and its twin `dce04` with the setter `801a5bc8`.
- **Three mispairs of game functions.** Row 518: stage A swapped GAME.EMI
  `801a2c64` and `801a2ee0` (PSP `882d8` / `88704`); swapped back, both
  re-match clean, and the caller `86548` (`801a2bc8`'s twin) picks them the
  PSX way (byte `+0x70` set: `882d8`). Row 728: the PSX side is data
  decoded as code. Row 758: the PSX side is a jump-table case body (no
  prologue) and the PSP side is the twin of `801f2cd8`.
- **Function boundaries.** A PSX `jr v0` jump table ends the function before
  its case bodies (rows 549, 739, 808), and two PSX functions are one on the
  PSP (rows 869, 872..883).
- **Compiler forms**: constants the PSP computes from one it holds
  (`0x77ff = 0x63ff + 0x1400`), `lui x / ori 0x8000` against
  `lui x+1 / addiu -0x8000` across a delay slot, byte masks as `andi 0xfe`
  against `and -2`, `(v - 6) < 2` against `v == 6 || v == 7`, a halfword
  compare split into two bytes, divisions by a constant (PSX reciprocal
  multiply, PSP `div`), struct copies (`lwl/lwr` against `memcpy`), loop
  strength reduction.

**Platform**, by kind: the GPU (CLUT words `0x78xx`/`0x7bxx` stored at a
primitive's `+0xe` become small PSP texture ids, 11 rows; `Packet_Commit`'s
length argument, 46; the 24-bit ordering-table links of the packet
allocator, row 862; `ClearImage` / `LoadImage`), the video-mode test
(`GetGraphType`, PAL/NTSC: the PSP keeps one branch, 42 rows), sound
(`Ss*` calls become no-ops, rows 512, 900), file I/O (`File_LoadRequest`,
`CdRead`; rows 510, 511, 871), the GTE lighting path (row 476), a second
draw pass replacing a colour boost in SHISU (rows 546, 547, section 2.3),
and **widescreen**:

| # | PSX | PSP-JP / EU | what | in `psp-widescreen.md` 3a? |
|---|---|---|---|---|
| 466, 467 | `801f4de0`, `801f4e68` WORLD04/AREA172 #13 | `262594`, `26260c` | a 64x32 sprite drawn at (220, 200) on the PSX at (256, 190) - inside the 384 x 216 window of 2.4 | no |
| 478 | `80153758` boot, beside `MapView_Build` | `cd98` / `cc40` | the terrain strip ring: loop bound 56 -> 58 entries, row wrap 28 -> 33 rows, entry stride 112 -> 132 bytes | no (3a has the cull, not the buffer) |
| 567, 568 | `801e3b70`, `801e4130` ETC/SHOP | `1ff1ac`, `1ff474` | a left clamp moved 32 px (-170 -> -202, -120 -> -152): the -32 visible edge | no |
| 657, 658, 750, 861 | SCENA17, AREA082, AREA189 | | the full-frame `(-32, 0, 384/352, 240)` rectangle, and the presentation rectangle `(0, 12, 384, 216)` set through `a24cc` | yes (items 6, 7) |

## 1. Method and commands

Scratch scripts in
`<scratchpad>/psp_cd3/` (not in the tree; they print addresses, counts and
disassembly only):

| script | what it does |
|---|---|
| `build_rows.py` | reruns `psp_pair.build` and stage A's diff loop (unchanged code), sorts the field `differs` rows as `write_md` does, keeps 455..908 (first and last rows match the doc's), and pickles both sides' words, consts, calls |
| `cd3dis.py <row>` / `fdis.py` / `sbs.py` | side-by-side capstone disassembly, calls annotated with the sibling's names and twins |
| `explain.py` | a tolerant second pass: each one-sided constant against the other side's full value set (every immediate; every `lui` half joined with every later low half on that register; shift amounts), allowing +-1, negation, low 16 / 8 bits and scaling by 2^1..2^5; PSP constants carried by a relocated `addiu`/`ori` dropped (CD1); calls compared **by address** through any space's twin |
| `classify.py` | rows whose every difference `explain.py` settles are "rule"; the rest (80) were read |
| `listrict.py`, `oddctx.py` | a strict pass the tolerant one cannot hide: load-immediate values on one side only with no exact match anywhere on the other (+-1 allowed only against a compare) - 137 rows, each looked at in context; this is what found rows 733, 678 and 576, which `explain.py` had re-matched |
| `pairchk.py`, `pairs_batch*.py` | `explain.py` on any (PSX, PSP) pair: the callee twins and the zero-fill keys |
| `pspref.py`, `pspstate.py`, `stubs.py`, `gref.py`, `eudis.py` | PSP references to an address or a call target; my rows touching the PSP-only state; import stubs by module; PSX global writers and their twins; PSP-EU disassembly |
| `final.py` | the classes: the manual readings (section 3's `read` rows) override the rule, which is platform if the PSX side calls `GetGraphType` or `File_LoadRequest`/`Cd*`/`Ss*`/`ClearImage`/`LoadImage`/lighting, builds a CLUT word `0x78xx..0x7fxx` stored at `+0xe`, passes `Packet_Commit` the length 12, or the PSP adds `-32`/`352`/`384` where the PSX had 320; encoding otherwise |

```
python <scratch>/psp_cd3/build_rows.py          # about 2 minutes
python <scratch>/psp_cd3/final.py               # classes, table.md
python <scratch>/psp_cd3/cd3dis.py 733          # any row, by its number here
```

**How the quick decisions were made.** A row is encoding by rule only when
(a) the tolerant pass re-matches every constant, (b) the calls agree by
address or differ only in a Psy-Q call on the PSX side, a PSP runtime helper
(`abs` `2c553c`, `memcpy` `2c575c`, draw-mode `9fd54`, `sin` `9c7b0`) or a
callee twin, and (c) the strict pass found nothing it did not explain. The
strict pass's 137 rows were each looked at; the patterns above cover them.

## 2. Notes on the content rows

### 2.1 AREA068 (row 733)

PSX `801f3960`: `lb 0x801490C3; beqz; [li 5; sb 0x801D4292]; li 0xffff;
sh 0x801490A4`. PSP `228c08`: base `0x100210` (the twin of `0x8014909C`,
the box state byte of `TEXT_ENGINE.md`), `lb +0x27`; if set: `sh 0x74, +8`,
`sb 6, 0x11b6dc`, return; else `sh 0xffff, +8`. `0x801490A4` is the message
index the sibling's `MsgBox_Replay` re-reads (`symbols.toml`), so on the PSP
a set box byte leads to message 116 of the area block rather than none - a
guess at the effect; the owner's eye decides. Stage A had scored the row 2
(the 5 -> 6 was cancelled by its +-1 rule); `listrict.py` caught it.

### 2.2 AREA030 (rows 675, 677, 678, 685..688)

The PSP-only byte `0x11d48c` gates added branches in at least five AREA030
functions here and in 76 call sites overall; `0x210158` (true when the word at `0x11b320` is zero) and `0x21f418` (byte `0xc4478`) are two more PSP-only reads.
`0x20f4d4`, the byte's only setter, is reached through no direct call and no
`.data` pointer (searched); it does not reach the `sceNet*` imports within
four calls (`stubs.py`), so this is not shown to be the ad-hoc layer. What
the addition is in play is the owner's to say.

### 2.3 SHISU (rows 546, 547, platform)

The PSX adds `3k` (clamped to 255) to two colour bytes and subtracts from the
third before drawing (`801d2308`) and restores them after; the PSP keeps only
the subtraction, draws, then calls the PSP-only 230-instruction `20934c`
(a model draw with its own matrix set-up) with `0x40` and, for two kinds,
`x >> 2`. A rendering substitute; whether it looks the same is not checked.

## 3. Per-row classification

`how`: `read` (disassembly read, both sides) or `rule` (section 1). Row
numbers are the section 7 table's, counted from its first row.

| # | PSX (file) | PSP-JP | class | how | why |
|---|---|---|---|---|---|
| 455 | `801f4230` WORLD03/AREA141.EMI#13 | `255f50` | encoding | read | GAME.EMI call resolved to the boot zero-fill; bound 0xc000 vs 0xbfff |
| 456 | `801f4508` WORLD03/AREA141.EMI#13 | `25621c` | encoding | read | scratchpad 0x1f800000 store becomes a .bss store; GAME.EMI call resolve |
| 457 | `801f4764` WORLD03/AREA141.EMI#13 | `25647c` | encoding | rule | constants re-match tolerantly |
| 458 | `801f3b24` WORLD03/AREA144.EMI#13 | `257594` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 459 | `801f32d4` WORLD03/AREA149.EMI#13 | `25a7f4` | encoding | read | 0x77ff = 0x63ff + 0x1400 computed; 0xffec0000 = -(0x14 << 16) |
| 460 | `801f3a70` WORLD03/AREA151.EMI#13 | `25c5f4` | encoding | read | (v + 0x60) & 0xff < 2 against v == 0xa0 / 0xa1; getter 80167628 = dce04 (stage A pairs dce04 with the setter 801a5bc8) |
| 461 | `801f39c4` WORLD04/AREA152.EMI#13 | `25e420` | encoding | read | as row 460 |
| 462 | `801f4158` WORLD04/AREA168.EMI#13 | `7532c` | platform | rule | video-mode test (GetGraphType) dropped |
| 463 | `801f390c` WORLD04/AREA169.EMI#13 | `260438` | encoding | rule | constants re-match tolerantly |
| 464 | `801f40f0` WORLD04/AREA170.EMI#13 | `734d4` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 465 | `801f3070` WORLD04/AREA171.EMI#13 | `7402c` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 466 | `801f4de0` WORLD04/AREA172.EMI#13 | `262594` | platform | read | widescreen layout: a 64x32 sprite moves from (220, 200) to (256, 190) - inside the PSP crop (psp-widescreen 2.4); not in its 3a list |
| 467 | `801f4e68` WORLD04/AREA172.EMI#13 | `26260c` | platform | read | as row 466 (the same sprite call) |
| 468 | `801f45e0` WORLD04/AREA173.EMI#14 | `81eac` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 469 | `801f3378` WORLD04/AREA189.EMI#13 | `7c7a0` | platform | rule | video-mode test (GetGraphType) dropped |
| 470 | `801f43e8` WORLD04/AREA189.EMI#13 | `7dd68` | platform | rule | video-mode test (GetGraphType) dropped |
| 471 | `801f518c` WORLD04/AREA189.EMI#13 | `26b3f4` | encoding | rule | constants re-match tolerantly |
| 472 | `801f5600` WORLD04/AREA189.EMI#13 | `26b678` | encoding | read | callee twins missed (801f5f20 = 26b01c, 801f6088 = 26b170); masks |
| 473 | `801f3ab0` WORLD04/AREA191.EMI#13 | `26c568` | encoding | rule | constants re-match tolerantly |
| 474 | `801f2c48` WORLD04/AREA197.EMI#13 | `53198` | encoding | read | callee twins missed (801f30e4 = 535e4, 801f3474 = 53928, slot-clear helpers) |
| 475 | `801f400c` WORLD04/AREA199.EMI#13 | `83b18` | platform | rule | CLUT word becomes a PSP texture id; Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 476 | `8014bb64` SLPS_009.90 | `1540` | platform | read | GTE lighting (SetLightMatrix, NormalColorCol) through the PSP twin 8c1d0; Packet_Commit length |
| 477 | `8014fce8` SLPS_009.90 | `95e0` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 478 | `80153758` SLPS_009.90 | `cd98` | platform | read | widescreen: the terrain strip ring grows from 56 to 58 entries and 28 to 33 rows (loop bound 0x38 -> 0x3a, wrap 0x1b -> 0x20, stride 112 -> 132); beside MapView_Build, not in psp-widescreen 3a |
| 479 | `80155578` SLPS_009.90 | `f988` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 480 | `80156938` SLPS_009.90 | `104b8` | encoding | rule | constants re-match tolerantly |
| 481 | `80157e14` SLPS_009.90 | `12748` | encoding | rule | constants re-match tolerantly |
| 482 | `801a0a30` SLPS_009.90 | `84dcc` | encoding | read | PSX side is the boot zero-fill at a GAME.EMI address; against GAME.EMI 801a0a30 the PSP code is the same |
| 483 | `801b020c` SLPS_009.90 | `9a580` | encoding | read | zero-fill key; against GAME.EMI 801b020c the same (text-code compare hoisted) |
| 484 | `801c2ab4` SLPS_009.90 | `d965c` | encoding | read | zero-fill key; GAME.EMI 801c2ab4 re-matches clean |
| 485 | `801eeef0` ETC/COMMU00.EMI#0 | `109488` | encoding | rule | constants re-match tolerantly |
| 486 | `801ef110` ETC/COMMU00.EMI#0 | `1076ec` | encoding | rule | constants re-match tolerantly |
| 487 | `801f0524` ETC/COMMU00.EMI#0 | `107df8` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 488 | `801f1bb8` ETC/COMMU00.EMI#0 | `10a17c` | encoding | read | struct copy (lwl/lwr) becomes a 12-byte memcpy |
| 489 | `801d1778` ETC/COMMU01.EMI#7 | `10adfc` | encoding | rule | callee twin missed; constants re-match tolerantly; globals unmapped (data map gap) |
| 490 | `801d1a94` ETC/COMMU01.EMI#7 | `10b0f4` | encoding | rule | callee twin missed; constants re-match tolerantly; globals unmapped (data map gap) |
| 491 | `801d3a54` ETC/COMMU01.EMI#7 | `10bca8` | encoding | rule | constants re-match tolerantly |
| 492 | `801d4484` ETC/COMMU01.EMI#7 | `10c1e4` | encoding | rule | constants re-match tolerantly |
| 493 | `801d15ec` ETC/COMMU02.EMI#8 | `10e514` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 494 | `801d17bc` ETC/COMMU02.EMI#8 | `10e6a4` | encoding | rule | constants re-match tolerantly |
| 495 | `801d20c0` ETC/COMMU02.EMI#8 | `10ef18` | encoding | rule | constants re-match tolerantly |
| 496 | `801d2ec4` ETC/COMMU02.EMI#8 | `10fd10` | platform | read | CLUT word 0x7887 becomes a PSP texture id 0x270 |
| 497 | `801d5188` ETC/COMMU02.EMI#8 | `115c9c` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 498 | `801d5ae4` ETC/COMMU02.EMI#8 | `111b08` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 499 | `801d7e5c` ETC/COMMU02.EMI#8 | `107bc0` | encoding | rule | constants re-match tolerantly |
| 500 | `801d8124` ETC/COMMU02.EMI#8 | `1140a4` | encoding | rule | callee twin missed |
| 501 | `801d90d0` ETC/COMMU02.EMI#8 | `114844` | encoding | rule | constants re-match tolerantly |
| 502 | `801d94c8` ETC/COMMU02.EMI#8 | `11674c` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 503 | `801daae4` ETC/COMMU02.EMI#8 | `115138` | encoding | rule | globals unmapped (data map gap) |
| 504 | `801d1708` ETC/COMMU03.EMI#6 | `117b24` | platform | rule | video-mode test (GetGraphType) dropped |
| 505 | `801d0d4c` ETC/COMMU04.EMI#6 | `1184e0` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 506 | `801d12c8` ETC/COMMU04.EMI#6 | `1188f8` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 507 | `801d0f50` ETC/COMMU05.EMI#7 | `118e14` | platform | read | CLUT word 0x7849 becomes 0x190; video-mode test dropped |
| 508 | `801d1498` ETC/COMMU05.EMI#7 | `11937c` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 509 | `801d1640` ETC/COMMU05.EMI#7 | `119520` | platform | read | CLUT word 0x7883 becomes 0x230 |
| 510 | `801986ec` ETC/GAME.EMI#0 | `8b9c` | platform | read | file I/O: File_LoadRequest (CD) becomes the PSP loader 970fc |
| 511 | `80198c94` ETC/GAME.EMI#0 | `74d0` | platform | read | file I/O: File_LoadRequest becomes 970fc |
| 512 | `80198f48` ETC/GAME.EMI#0 | `78ac` | platform | read | sound: SsSepSetVol wrapper becomes the PSP sound call 951f0 |
| 513 | `8019bb64` ETC/GAME.EMI#0 | `20d14` | encoding | read | scratchpad-pointer clear 80197070 = 3154; flag base +0x650 |
| 514 | `8019bbb8` ETC/GAME.EMI#0 | `20d68` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 515 | `8019cbac` ETC/GAME.EMI#0 | `25590` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 516 | `8019d4f8` ETC/GAME.EMI#0 | `22898` | encoding | read | tile getter 80167628 = dce04 |
| 517 | `8019dfd4` ETC/GAME.EMI#0 | `233b0` | encoding | read | as row 513 |
| 518 | `801a2c64` ETC/GAME.EMI#0 | `88704` | encoding | read | MISPAIR: stage A swapped 801a2c64 and 801a2ee0 (PSP 882d8 / 88704); swapped back both re-match clean, and the caller 86548 picks them the PSX way |
| 519 | `801a4088` ETC/GAME.EMI#0 | `88b24` | encoding | rule | constants re-match tolerantly |
| 520 | `801a952c` ETC/GAME.EMI#0 | `8cf68` | encoding | rule | constants re-match tolerantly |
| 521 | `801a9a98` ETC/GAME.EMI#0 | `8d698` | encoding | read | tile getter 80167628 = dce04 |
| 522 | `801aa944` ETC/GAME.EMI#0 | `90d18` | encoding | rule | constants re-match tolerantly |
| 523 | `801adac0` ETC/GAME.EMI#0 | `90058` | encoding | rule | constants re-match tolerantly |
| 524 | `801adcd8` ETC/GAME.EMI#0 | `9020c` | encoding | rule | constants re-match tolerantly |
| 525 | `801ae4dc` ETC/GAME.EMI#0 | `933e8` | encoding | rule | constants re-match tolerantly |
| 526 | `801b6444` ETC/GAME.EMI#0 | `ca568` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 527 | `801b69ac` ETC/GAME.EMI#0 | `ca9d4` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 528 | `801b7410` ETC/GAME.EMI#0 | `cb180` | encoding | rule | constants re-match tolerantly |
| 529 | `801b7468` ETC/GAME.EMI#0 | `cb1c4` | encoding | rule | constants re-match tolerantly |
| 530 | `801bd758` ETC/GAME.EMI#0 | `d4de0` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 531 | `801beab0` ETC/GAME.EMI#0 | `d58dc` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 532 | `801bebbc` ETC/GAME.EMI#0 | `d59d4` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 533 | `801c0440` ETC/GAME.EMI#0 | `d65e4` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 534 | `801c08d8` ETC/GAME.EMI#0 | `d6a78` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap); PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 535 | `801c0b2c` ETC/GAME.EMI#0 | `d6c88` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 536 | `801c0e2c` ETC/GAME.EMI#0 | `d6f20` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 537 | `801c2e80` ETC/GAME.EMI#0 | `d8300` | encoding | rule | constants re-match tolerantly |
| 538 | `801c2f44` ETC/GAME.EMI#0 | `d8394` | encoding | rule | constants re-match tolerantly |
| 539 | `801c3998` ETC/GAME.EMI#0 | `d9f3c` | encoding | rule | constants re-match tolerantly |
| 540 | `801c4084` ETC/GAME.EMI#0 | `da4e0` | encoding | read | an 8-byte rect table copied from .rodata on the PSX is built from immediates on the PSP (values checked equal) |
| 541 | `801c7880` ETC/GAME.EMI#0 | `1429b4` | encoding | rule | globals unmapped (data map gap) |
| 542 | `801c7ed8` ETC/GAME.EMI#0 | `14302c` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 543 | `801d104c` ETC/GAME.EMI#1 | `5a64` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 544 | `801d1a20` ETC/SHISU.EMI#0 | `208000` | encoding | rule | globals unmapped (data map gap) |
| 545 | `801d1b58` ETC/SHISU.EMI#0 | `20811c` | encoding | rule | globals unmapped (data map gap) |
| 546 | `801d2978` ETC/SHISU.EMI#0 | `208578` | platform | read | the model colour boost (+3k on two colour bytes, clamped) becomes a second PSP draw pass 20934c(obj, 0x40 or x >> 2) |
| 547 | `801d2adc` ETC/SHISU.EMI#0 | `20869c` | platform | read | as row 546 |
| 548 | `801d1154` ETC/SHOP.EMI#0 | `1eee58` | encoding | rule | constants re-match tolerantly |
| 549 | `801d23ec` ETC/SHOP.EMI#0 | `1ef890` | encoding | read | jump table vs compare chain: the PSX case bodies (Inventory_Remove / _Add) lie outside the bound |
| 550 | `801d3664` ETC/SHOP.EMI#0 | `1f0474` | encoding | rule | constants re-match tolerantly |
| 551 | `801d3698` ETC/SHOP.EMI#0 | `1f04a4` | encoding | rule | callee twin missed; constants re-match tolerantly |
| 552 | `801d3cac` ETC/SHOP.EMI#0 | `1f14ac` | encoding | rule | callee twin missed |
| 553 | `801d3d94` ETC/SHOP.EMI#0 | `1f158c` | encoding | rule | callee twin missed |
| 554 | `801d3fe4` ETC/SHOP.EMI#0 | `1f17c8` | encoding | rule | callee twin missed |
| 555 | `801d409c` ETC/SHOP.EMI#0 | `1f1888` | encoding | rule | callee twin missed |
| 556 | `801d41fc` ETC/SHOP.EMI#0 | `1f19c0` | encoding | rule | callee twin missed |
| 557 | `801d431c` ETC/SHOP.EMI#0 | `1f1ac0` | encoding | rule | callee twin missed |
| 558 | `801d447c` ETC/SHOP.EMI#0 | `1f1c10` | encoding | rule | callee twin missed |
| 559 | `801d457c` ETC/SHOP.EMI#0 | `1f1cfc` | encoding | rule | callee twin missed |
| 560 | `801d4624` ETC/SHOP.EMI#0 | `1f1db4` | encoding | rule | callee twin missed |
| 561 | `801d5da8` ETC/SHOP.EMI#0 | `1f2c18` | encoding | rule | callee twin missed |
| 562 | `801d6028` ETC/SHOP.EMI#0 | `1f2cf8` | encoding | rule | callee twin missed |
| 563 | `801d76c8` ETC/SHOP.EMI#0 | `1f423c` | encoding | read | 12 = the 11 stored before, plus one |
| 564 | `801d95d0` ETC/SHOP.EMI#0 | `bb1cc` | encoding | rule | calls compared by address agree |
| 565 | `801da430` ETC/SHOP.EMI#0 | `1f5df4` | encoding | rule | constants re-match tolerantly |
| 566 | `801e2dbc` ETC/SHOP.EMI#0 | `1fec10` | encoding | rule | callee twin missed |
| 567 | `801e3b70` ETC/SHOP.EMI#0 | `1ff1ac` | platform | read | widescreen: a left clamp moves 32 px (-170 -> -202), the -32 visible edge |
| 568 | `801e4130` ETC/SHOP.EMI#0 | `1ff474` | platform | read | widescreen: left clamp -120 -> -152 |
| 569 | `801e4aec` ETC/SHOP.EMI#0 | `1ff77c` | encoding | rule | constants re-match tolerantly |
| 570 | `801dab08` ETC/START.EMI#8 | `b75b0` | encoding | rule | constants re-match tolerantly |
| 571 | `801dad00` ETC/START.EMI#8 | `b6c08` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 572 | `801e15d4` ETC/START.EMI#8 | `c0160` | encoding | rule | callee twin missed |
| 573 | `801e6308` ETC/START.EMI#8 | `c3104` | encoding | rule | constants re-match tolerantly |
| 574 | `801e6418` ETC/START.EMI#8 | `c3fd0` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 575 | `801e6be8` ETC/START.EMI#8 | `1f33a0` | encoding | rule | callee twin missed; constants re-match tolerantly |
| 576 | `801e6d98` ETC/START.EMI#8 | `c3298` | content | read | START title: the label sprite takes (v, width) from a five-entry .rodata table on the PSP where the PSX picks one of two widths; Packet_Commit length |
| 577 | `801e6f80` ETC/START.EMI#8 | `c3480` | encoding | rule | callee twin missed |
| 578 | `801e8008` ETC/START.EMI#8 | `c4eb8` | encoding | rule | constants re-match tolerantly |
| 579 | `801e94d4` ETC/START.EMI#8 | `c63e0` | encoding | rule | callee twin missed |
| 580 | `801d1370` SCENARIO/SCE15EF0.EMI#0 | `57bfc` | platform | rule | Packet_Commit length argument |
| 581 | `801d106c` SCENARIO/SCE15EF1.EMI#0 | `59dac` | encoding | rule | constants re-match tolerantly |
| 582 | `801d147c` SCENARIO/SCE15EF1.EMI#0 | `43f74` | platform | rule | Packet_Commit length argument |
| 583 | `801d21b4` SCENARIO/SCE15EF1.EMI#0 | `5add8` | platform | read | ClearImage (VRAM) becomes the PSP clear a2c2c; scratchpad clear 80197070 = 3154 |
| 584 | `801d2e30` SCENARIO/SCE15EF1.EMI#0 | `5b960` | platform | read | as row 583 |
| 585 | `801d1218` SCENARIO/SCE15EF2.EMI#0 | `5c910` | platform | rule | Packet_Commit length argument |
| 586 | `801d1624` SCENARIO/SCE15EF2.EMI#0 | `5cba0` | platform | rule | Packet_Commit length argument |
| 587 | `801d10ac` SCENARIO/SCE15EF3.EMI#0 | `5d690` | encoding | rule | constants re-match tolerantly |
| 588 | `801d1a60` SCENARIO/SCE15EF3.EMI#0 | `5dfd8` | encoding | read | callee twin missed (801d171c = 5dc4c) |
| 589 | `801d1ff8` SCENARIO/SCE15EF3.EMI#0 | `28f38` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 590 | `801f6e48` SCENARIO/SCENA00.EMI#0 | `17188` | encoding | rule | calls compared by address agree |
| 591 | `801f7134` SCENARIO/SCENA00.EMI#0 | `15bcc` | platform | read | CLUT word 0x7a80 becomes 0xa00; video-mode test dropped |
| 592 | `801f7484` SCENARIO/SCENA00.EMI#0 | `15e70` | encoding | rule | constants re-match tolerantly |
| 593 | `801f8360` SCENARIO/SCENA00.EMI#0 | `1763c` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 594 | `801fb924` SCENARIO/SCENA00.EMI#0 | `2711e4` | encoding | read | 0x3000 = 0x5000 - 0x2000 computed |
| 595 | `801f7dcc` SCENARIO/SCENA01.EMI#0 | `60484` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 596 | `801fbf94` SCENARIO/SCENA02.EMI#0 | `2781f8` | encoding | rule | constants re-match tolerantly |
| 597 | `801fc308` SCENARIO/SCENA02.EMI#0 | `278524` | encoding | rule | constants re-match tolerantly |
| 598 | `801fc518` SCENARIO/SCENA02.EMI#0 | `278700` | encoding | rule | constants re-match tolerantly |
| 599 | `801fe1c0` SCENARIO/SCENA02.EMI#0 | `27dc1c` | encoding | rule | calls compared by address agree |
| 600 | `801fc704` SCENARIO/SCENA03.EMI#0 | `2809d4` | encoding | rule | constants re-match tolerantly |
| 601 | `801fcb4c` SCENARIO/SCENA03.EMI#0 | `280e70` | encoding | read | callee twin missed (801fcc9c = 280fdc) |
| 602 | `801fcefc` SCENARIO/SCENA03.EMI#0 | `281248` | encoding | rule | calls compared by address agree |
| 603 | `801f7cf0` SCENARIO/SCENA04.EMI#0 | `2d594` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 604 | `801f7554` SCENARIO/SCENA05.EMI#0 | `2a540` | encoding | rule | constants re-match tolerantly |
| 605 | `801fd178` SCENARIO/SCENA05.EMI#0 | `2881f4` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 606 | `801fd208` SCENARIO/SCENA05.EMI#0 | `2882ac` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 607 | `801fd4d8` SCENARIO/SCENA05.EMI#0 | `288b60` | encoding | rule | calls compared by address agree |
| 608 | `801fd580` SCENARIO/SCENA05.EMI#0 | `288bf8` | encoding | rule | calls compared by address agree |
| 609 | `801fd800` SCENARIO/SCENA06.EMI#0 | `28e880` | encoding | rule | constants re-match tolerantly |
| 610 | `801fe254` SCENARIO/SCENA06.EMI#0 | `28f17c` | encoding | rule | calls compared by address agree |
| 611 | `801fe284` SCENARIO/SCENA06.EMI#0 | `28f1a8` | encoding | rule | calls compared by address agree |
| 612 | `801fe2b4` SCENARIO/SCENA06.EMI#0 | `28f1d4` | encoding | rule | calls compared by address agree |
| 613 | `801fe334` SCENARIO/SCENA06.EMI#0 | `28f248` | encoding | rule | calls compared by address agree |
| 614 | `801fe38c` SCENARIO/SCENA06.EMI#0 | `28f298` | encoding | rule | calls compared by address agree |
| 615 | `801f7ddc` SCENARIO/SCENA07.EMI#0 | `37458` | encoding | read | callee twins missed (801fa38c = 398a4, 80197070 = 3154) |
| 616 | `801f8358` SCENARIO/SCENA07.EMI#0 | `379d0` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 617 | `801f85d8` SCENARIO/SCENA07.EMI#0 | `37c68` | platform | rule | Packet_Commit length argument |
| 618 | `801f9330` SCENARIO/SCENA07.EMI#0 | `38c94` | platform | rule | Packet_Commit length argument |
| 619 | `801f9efc` SCENARIO/SCENA07.EMI#0 | `38570` | platform | rule | Packet_Commit length argument |
| 620 | `801fd524` SCENARIO/SCENA07.EMI#0 | `28fb80` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 621 | `801fd5a0` SCENARIO/SCENA07.EMI#0 | `28fc00` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 622 | `801f77b4` SCENARIO/SCENA08.EMI#0 | `3c064` | encoding | rule | calls compared by address agree |
| 623 | `801f7e14` SCENARIO/SCENA08.EMI#0 | `3cfc0` | platform | rule | Packet_Commit length argument |
| 624 | `801f8964` SCENARIO/SCENA08.EMI#0 | `3784c` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 625 | `801f8bf8` SCENARIO/SCENA08.EMI#0 | `3cc64` | platform | rule | Packet_Commit length argument |
| 626 | `801f9ce0` SCENARIO/SCENA08.EMI#0 | `42294` | encoding | rule | constants re-match tolerantly |
| 627 | `801f9e50` SCENARIO/SCENA08.EMI#0 | `42428` | encoding | rule | constants re-match tolerantly |
| 628 | `801f9ec4` SCENARIO/SCENA08.EMI#0 | `424a0` | encoding | rule | constants re-match tolerantly |
| 629 | `801f9f8c` SCENARIO/SCENA08.EMI#0 | `42554` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 630 | `801f76d0` SCENARIO/SCENA09.EMI#0 | `483b0` | encoding | rule | constants re-match tolerantly |
| 631 | `801f9354` SCENARIO/SCENA09.EMI#0 | `295dd4` | encoding | rule | constants re-match tolerantly |
| 632 | `801fbd6c` SCENARIO/SCENA09.EMI#0 | `299b38` | encoding | rule | constants re-match tolerantly |
| 633 | `801fe12c` SCENARIO/SCENA09.EMI#0 | `276f94` | encoding | rule | calls compared by address agree |
| 634 | `801fe154` SCENARIO/SCENA09.EMI#0 | `29bf9c` | encoding | rule | calls compared by address agree |
| 635 | `801fe184` SCENARIO/SCENA09.EMI#0 | `28f200` | encoding | rule | calls compared by address agree |
| 636 | `801fe1ac` SCENARIO/SCENA09.EMI#0 | `29bfec` | encoding | rule | calls compared by address agree |
| 637 | `801fe1dc` SCENARIO/SCENA09.EMI#0 | `29c018` | encoding | rule | calls compared by address agree |
| 638 | `801fe20c` SCENARIO/SCENA09.EMI#0 | `29c044` | encoding | rule | calls compared by address agree |
| 639 | `801fe234` SCENARIO/SCENA09.EMI#0 | `29c068` | encoding | rule | calls compared by address agree |
| 640 | `801fe30c` SCENARIO/SCENA09.EMI#0 | `288bb0` | encoding | rule | calls compared by address agree |
| 641 | `801fe334` SCENARIO/SCENA09.EMI#0 | `288bd4` | encoding | rule | calls compared by address agree |
| 642 | `801fe35c` SCENARIO/SCENA09.EMI#0 | `29c16c` | encoding | rule | calls compared by address agree |
| 643 | `801f719c` SCENARIO/SCENA10.EMI#0 | `41098` | encoding | rule | constants re-match tolerantly |
| 644 | `801f7ad4` SCENARIO/SCENA10.EMI#0 | `41994` | encoding | rule | constants re-match tolerantly |
| 645 | `801f7fa0` SCENARIO/SCENA10.EMI#0 | `29ccc0` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 646 | `801fc280` SCENARIO/SCENA10.EMI#0 | `29ff00` | encoding | read | flag base + 0xc77 |
| 647 | `801fe00c` SCENARIO/SCENA10.EMI#0 | `2a0b14` | encoding | rule | constants re-match tolerantly |
| 648 | `801fe360` SCENARIO/SCENA10.EMI#0 | `2a144c` | encoding | rule | calls compared by address agree |
| 649 | `801fe63c` SCENARIO/SCENA10.EMI#0 | `2a16e4` | encoding | rule | calls compared by address agree |
| 650 | `801f7178` SCENARIO/SCENA11.EMI#0 | `5ede4` | encoding | rule | constants re-match tolerantly |
| 651 | `801f7200` SCENARIO/SCENA11.EMI#0 | `5ee60` | encoding | rule | constants re-match tolerantly |
| 652 | `801f88b0` SCENARIO/SCENA12.EMI#0 | `2a4c78` | encoding | rule | constants re-match tolerantly |
| 653 | `801f7dbc` SCENARIO/SCENA13.EMI#0 | `2aa204` | encoding | rule | calls compared by address agree |
| 654 | `801fb84c` SCENARIO/SCENA14.EMI#0 | `2b1160` | encoding | rule | constants re-match tolerantly |
| 655 | `801fa6bc` SCENARIO/SCENA15.EMI#0 | `2b1d40` | encoding | rule | calls compared by address agree |
| 656 | `801f77fc` SCENARIO/SCENA17.EMI#0 | `2b7200` | encoding | rule | calls compared by address agree |
| 657 | `801f8204` SCENARIO/SCENA17.EMI#0 | `2b7b58` | platform | read | PSP sets its presentation rectangle (0, 12, 384, 216) through a24cc (psp-widescreen 2.4) |
| 658 | `801f8914` SCENARIO/SCENA17.EMI#0 | `2b8214` | platform | read | full-frame rectangle (0, 0, 320, 240) -> (-32, 0, 384, 240); video-mode test dropped |
| 659 | `801f9104` SCENARIO/SCENA17.EMI#0 | `2b84cc` | encoding | rule | constants re-match tolerantly |
| 660 | `801f3c2c` WORLD00/AREA008.EMI#13 | `3fe24` | encoding | rule | constants re-match tolerantly |
| 661 | `801f3d10` WORLD00/AREA008.EMI#13 | `3feec` | encoding | rule | constants re-match tolerantly |
| 662 | `801f2cdc` WORLD00/AREA010.EMI#13 | `222294` | encoding | rule | constants re-match tolerantly |
| 663 | `801f2cc4` WORLD00/AREA011.EMI#13 | `2223b8` | encoding | rule | constants re-match tolerantly |
| 664 | `801f2d88` WORLD00/AREA013.EMI#13 | `222618` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 665 | `801f39d8` WORLD00/AREA016.EMI#13 | `22348c` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 666 | `801f42f8` WORLD00/AREA016.EMI#13 | `223998` | encoding | rule | constants re-match tolerantly |
| 667 | `801f2df8` WORLD00/AREA024.EMI#14 | `27a70` | encoding | rule | calls compared by address agree |
| 668 | `801f3370` WORLD00/AREA026.EMI#13 | `225570` | encoding | rule | constants re-match tolerantly |
| 669 | `801f2d5c` WORLD00/AREA027.EMI#13 | `2eb80` | encoding | read | scratchpad-pointer clear 80197070 = 3154 |
| 670 | `801f3164` WORLD00/AREA027.EMI#13 | `60658` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 671 | `801f2d3c` WORLD00/AREA028.EMI#13 | `322c8` | encoding | read | -64 - v and 192 - v stored as a byte |
| 672 | `801f3060` WORLD00/AREA028.EMI#13 | `321b8` | platform | rule | Packet_Commit length argument |
| 673 | `801d1818` WORLD00/AREA030.EMI#4 | `18700` | platform | read | CLUT word 0x7a40 becomes 0x900 |
| 674 | `801d18cc` WORLD00/AREA030.EMI#4 | `187b4` | platform | read | CLUT word 0x7a40 becomes 0x900 |
| 675 | `801d1cf0` WORLD00/AREA030.EMI#4 | `1e220` | content | read | AREA030: a PSP-only state byte (0x11d48c through 2100fc) selects a PSP-added path |
| 676 | `801d2adc` WORLD00/AREA030.EMI#4 | `196c4` | encoding | read | scratchpad-pointer clear 80197070 = 3154 |
| 677 | `801d4200` WORLD00/AREA030.EMI#4 | `1a920` | content | read | AREA030: the same PSP-only byte switches a draw argument from 29 to -7 |
| 678 | `801d6144` WORLD00/AREA030.EMI#4 | `1cafc` | content | read | AREA030: a draw position argument 228 -> 229 (0xe4 -> 0xe5) |
| 679 | `801d6f7c` WORLD00/AREA030.EMI#4 | `1ea64` | encoding | rule | constants re-match tolerantly |
| 680 | `801d732c` WORLD00/AREA030.EMI#4 | `1ed64` | encoding | rule | constants re-match tolerantly |
| 681 | `801d79e4` WORLD00/AREA030.EMI#4 | `1d8b4` | platform | read | CLUT word 0x7887 becomes 0x270 |
| 682 | `801d7bac` WORLD00/AREA030.EMI#4 | `1d9f4` | encoding | rule | constants re-match tolerantly |
| 683 | `801d7e34` WORLD00/AREA030.EMI#4 | `1dd58` | encoding | rule | constants re-match tolerantly |
| 684 | `801d8410` WORLD00/AREA030.EMI#4 | `1f4e8` | encoding | rule | constants re-match tolerantly |
| 685 | `801d9c24` WORLD00/AREA030.EMI#4 | `de2dc` | content | read | AREA030 init: the PSP clears more state, including 0x11b30a which only PSP-added AREA030 code reads (probable part of the AREA030 addition) |
| 686 | `801da1e8` WORLD00/AREA030.EMI#4 | `df4a8` | content | read | AREA030: PSP-only byte (2100fc) gates an added branch |
| 687 | `801daf98` WORLD00/AREA030.EMI#4 | `dfc50` | content | read | AREA030: PSP-only byte (2100fc) gates an added branch |
| 688 | `801dbc2c` WORLD00/AREA030.EMI#4 | `e0440` | content | read | AREA030: PSP-only bytes (2100fc, 210158, 21f418) gate added branches |
| 689 | `801de218` WORLD00/AREA030.EMI#4 | `e5f3c` | encoding | rule | constants re-match tolerantly |
| 690 | `801e0b14` WORLD00/AREA030.EMI#4 | `e32cc` | encoding | rule | constants re-match tolerantly |
| 691 | `801e0cd8` WORLD00/AREA030.EMI#4 | `e345c` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 692 | `801e0fa4` WORLD00/AREA030.EMI#4 | `e364c` | encoding | rule | constants re-match tolerantly |
| 693 | `801e1a24` WORLD00/AREA030.EMI#4 | `e3fa4` | encoding | rule | constants re-match tolerantly |
| 694 | `800f5be8` WORLD00/AREA030.EMI#5 | `e7e70` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 695 | `800f6c74` WORLD00/AREA030.EMI#5 | `e8e50` | encoding | rule | constants re-match tolerantly |
| 696 | `801f2d18` WORLD00/AREA032.EMI#13 | `3b064` | encoding | rule | callee twin missed |
| 697 | `801f2dac` WORLD00/AREA032.EMI#13 | `3b0ec` | encoding | rule | callee twin missed |
| 698 | `801f3920` WORLD00/AREA032.EMI#13 | `225efc` | encoding | rule | constants re-match tolerantly |
| 699 | `801f390c` WORLD00/AREA033.EMI#13 | `227078` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 700 | `801f422c` WORLD00/AREA033.EMI#13 | `227584` | encoding | rule | constants re-match tolerantly |
| 701 | `801f31f0` WORLD01/AREA041.EMI#13 | `6213c` | encoding | read | flag base + 0x5c8 |
| 702 | `801f34ac` WORLD01/AREA041.EMI#13 | `62750` | encoding | rule | constants re-match tolerantly |
| 703 | `801f2e78` WORLD01/AREA042.EMI#13 | `2ace0` | platform | rule | Packet_Commit length argument |
| 704 | `801f3988` WORLD01/AREA042.EMI#13 | `63d0c` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 705 | `801f3a0c` WORLD01/AREA042.EMI#13 | `63d90` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 706 | `801f3b24` WORLD01/AREA042.EMI#13 | `63e84` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 707 | `801f3e80` WORLD01/AREA042.EMI#13 | `22afe8` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 708 | `801f2fbc` WORLD01/AREA043.EMI#13 | `22c2f8` | encoding | rule | calls compared by address agree |
| 709 | `801f39d8` WORLD01/AREA045.EMI#13 | `22dad0` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 710 | `801f42f8` WORLD01/AREA045.EMI#13 | `22dfdc` | encoding | rule | constants re-match tolerantly |
| 711 | `801f2db4` WORLD01/AREA047.EMI#13 | `30d68` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 712 | `801f2f60` WORLD01/AREA047.EMI#13 | `31170` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 713 | `801f2c48` WORLD01/AREA048.EMI#13 | `2bd4c` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 714 | `801f2d64` WORLD01/AREA048.EMI#13 | `2be94` | encoding | rule | constants re-match tolerantly |
| 715 | `801f302c` WORLD01/AREA048.EMI#13 | `2c18c` | platform | rule | Packet_Commit length argument |
| 716 | `801f4804` WORLD01/AREA048.EMI#13 | `22f2f8` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 717 | `801f4c28` WORLD01/AREA048.EMI#13 | `22f6f0` | encoding | rule | constants re-match tolerantly |
| 718 | `801170d8` WORLD01/AREA049.EMI#13 | `47e98` | encoding | rule | calls compared by address agree |
| 719 | `80117410` WORLD01/AREA049.EMI#13 | `47918` | platform | rule | Packet_Commit length argument |
| 720 | `80117900` WORLD01/AREA049.EMI#13 | `47d8c` | platform | rule | Packet_Commit length argument |
| 721 | `80117c58` WORLD01/AREA049.EMI#13 | `6c050` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 722 | `801f2d00` WORLD01/AREA051.EMI#13 | `2307b8` | encoding | rule | constants re-match tolerantly |
| 723 | `801f2ec8` WORLD01/AREA052.EMI#13 | `36154` | platform | rule | Packet_Commit length argument |
| 724 | `801f3a14` WORLD01/AREA052.EMI#13 | `230de4` | encoding | read | tile getter 80167628 = dce04 |
| 725 | `801f3e14` WORLD01/AREA052.EMI#13 | `2311dc` | encoding | rule | constants re-match tolerantly |
| 726 | `801f4268` WORLD01/AREA052.EMI#13 | `2312d0` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 727 | `801f43b0` WORLD01/AREA052.EMI#13 | `231414` | encoding | read | tile getter 80167628 = dce04 |
| 728 | `801f2dd4` WORLD01/AREA064.EMI#13 | `233370` | encoding | read | MISPAIR: the PSX side is not code (data words decoded as lb/nop); the PSP twin of 801f2d4c is called |
| 729 | `801f3a10` WORLD01/AREA065.EMI#13 | `233e68` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 730 | `801f4330` WORLD01/AREA065.EMI#13 | `234374` | encoding | rule | constants re-match tolerantly |
| 731 | `801f2c48` WORLD01/AREA067.EMI#14 | `2d940` | encoding | rule | constants re-match tolerantly |
| 732 | `801f3f00` WORLD01/AREA067.EMI#14 | `235440` | encoding | rule | constants re-match tolerantly |
| 733 | `801f3960` WORLD01/AREA068.EMI#13 | `228c08` | content | read | AREA068: with a message box open, the PSP writes 6 (PSX 5) to the master-state byte (SISYOU 0x801d4292) and sets the message index 0x801490a4 to 116 (PSX 0xffff) |
| 734 | `801f4874` WORLD01/AREA075.EMI#13 | `2376cc` | encoding | rule | callee twin missed; constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 735 | `801f2d0c` WORLD02/AREA077.EMI#14 | `348b0` | encoding | rule | constants re-match tolerantly |
| 736 | `801f2ec0` WORLD02/AREA077.EMI#14 | `34a54` | encoding | rule | constants re-match tolerantly |
| 737 | `801f462c` WORLD02/AREA077.EMI#14 | `3a238` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 738 | `801f5bd8` WORLD02/AREA077.EMI#14 | `2386b4` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 739 | `801f2d20` WORLD02/AREA079.EMI#13 | `238f58` | encoding | rule | constants re-match tolerantly |
| 740 | `801f352c` WORLD02/AREA080.EMI#13 | `344c4` | platform | rule | Packet_Commit length argument |
| 741 | `801f2c94` WORLD02/AREA081.EMI#13 | `23951c` | encoding | rule | constants re-match tolerantly |
| 742 | `801f3408` WORLD02/AREA082.EMI#13 | `2c988` | encoding | rule | callee twin missed |
| 743 | `801f3ba0` WORLD02/AREA082.EMI#13 | `6d660` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 744 | `801f4008` WORLD02/AREA082.EMI#13 | `6da64` | encoding | rule | constants re-match tolerantly |
| 745 | `801f4334` WORLD02/AREA082.EMI#13 | `6dd50` | encoding | read | scratchpad-pointer clear 80197070 = 3154 |
| 746 | `801f4430` WORLD02/AREA082.EMI#13 | `6e868` | encoding | rule | constants re-match tolerantly |
| 747 | `801f4c90` WORLD02/AREA082.EMI#13 | `6e0ac` | encoding | rule | constants re-match tolerantly |
| 748 | `801f5124` WORLD02/AREA082.EMI#13 | `6e470` | encoding | rule | constants re-match tolerantly |
| 749 | `801f51bc` WORLD02/AREA082.EMI#13 | `6e4f0` | encoding | rule | constants re-match tolerantly |
| 750 | `801f5424` WORLD02/AREA082.EMI#13 | `6e734` | platform | read | full-frame rectangle -> (-32, 0, 352, 240); video-mode test dropped |
| 751 | `801f34f8` WORLD02/AREA086.EMI#13 | `239e78` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap); PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 752 | `801f3a0c` WORLD02/AREA087.EMI#13 | `23aad8` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 753 | `801f4328` WORLD02/AREA087.EMI#13 | `23afd8` | encoding | rule | constants re-match tolerantly |
| 754 | `801f453c` WORLD02/AREA088.EMI#13 | `23ca58` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 755 | `801f4e58` WORLD02/AREA088.EMI#13 | `23cf58` | encoding | rule | constants re-match tolerantly |
| 756 | `801f312c` WORLD02/AREA091.EMI#13 | `23e46c` | encoding | rule | constants re-match tolerantly |
| 757 | `801f3228` WORLD02/AREA091.EMI#13 | `23e554` | encoding | rule | constants re-match tolerantly |
| 758 | `801f2c88` WORLD02/AREA092.EMI#13 | `238e68` | encoding | read | MISPAIR: the PSX side is a jump-table case body (no prologue, falls through); the PSP side is the twin of 801f2cd8 |
| 759 | `801f3460` WORLD02/AREA094.EMI#13 | `240368` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 760 | `801f2f3c` WORLD02/AREA095.EMI#13 | `67e54` | encoding | rule | constants re-match tolerantly |
| 761 | `801f2ecc` WORLD02/AREA096.EMI#13 | `68290` | encoding | rule | constants re-match tolerantly |
| 762 | `801f3878` WORLD02/AREA098.EMI#13 | `433b4` | encoding | rule | callee twin missed |
| 763 | `801f4bc0` WORLD02/AREA098.EMI#13 | `44180` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 764 | `801f4e7c` WORLD02/AREA098.EMI#13 | `44438` | platform | rule | Packet_Commit length argument |
| 765 | `801f3608` WORLD02/AREA100.EMI#13 | `241960` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 766 | `801f352c` WORLD02/AREA103.EMI#13 | `3060c` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 767 | `801f53f0` WORLD02/AREA103.EMI#13 | `69e14` | platform | rule | video-mode test (GetGraphType) dropped |
| 768 | `801f3a9c` WORLD02/AREA104.EMI#14 | `2431ec` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 769 | `801f4ae4` WORLD02/AREA104.EMI#14 | `243680` | encoding | rule | callee twin missed |
| 770 | `801f6564` WORLD02/AREA104.EMI#14 | `244a3c` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 771 | `801f2c48` WORLD02/AREA105.EMI#13 | `7b688` | encoding | read | halfword compare 0x020c split into byte compares 0x0c, 2 |
| 772 | `801f32b8` WORLD02/AREA105.EMI#13 | `245de4` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 773 | `801f33d4` WORLD02/AREA106.EMI#13 | `7c134` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 774 | `801f3cd4` WORLD02/AREA106.EMI#13 | `2460dc` | encoding | rule | constants re-match tolerantly |
| 775 | `801f2d40` WORLD02/AREA108.EMI#14 | `2f8cc` | encoding | rule | calls compared by address agree |
| 776 | `801f3a14` WORLD02/AREA108.EMI#14 | `457f8` | encoding | read | callee twin missed (801f3ba0 = 45938; its own diff: SetDrawMode twin) |
| 777 | `801f3a6c` WORLD02/AREA108.EMI#14 | `45848` | encoding | read | as row 776 |
| 778 | `801f3f34` WORLD02/AREA108.EMI#14 | `6af18` | encoding | read | scratchpad-pointer clear 80197070 = 3154 |
| 779 | `801f4c28` WORLD02/AREA108.EMI#14 | `2468bc` | encoding | rule | constants re-match tolerantly |
| 780 | `801f3558` WORLD02/AREA112.EMI#13 | `248b2c` | encoding | read | lui 0x46 / ori 0x8000 against lui 0x47 / addiu -0x8000 across a delay slot |
| 781 | `801f3944` WORLD02/AREA112.EMI#13 | `248f20` | encoding | read | (v - 6) < 2 against v == 6 / 7 |
| 782 | `801f39dc` WORLD03/AREA115.EMI#13 | `24a684` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 783 | `801f42f8` WORLD03/AREA115.EMI#13 | `24ab84` | encoding | rule | constants re-match tolerantly |
| 784 | `801f314c` WORLD03/AREA117.EMI#13 | `6460c` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 785 | `801f33f8` WORLD03/AREA117.EMI#13 | `64854` | encoding | read | flag base + 0x5f8 |
| 786 | `801f3590` WORLD03/AREA117.EMI#13 | `649c4` | encoding | read | flag base + 0x5f8 |
| 787 | `801f364c` WORLD03/AREA117.EMI#13 | `64a70` | encoding | read | flag base + 0x5f8 |
| 788 | `801f4150` WORLD03/AREA118.EMI#13 | `65818` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 789 | `801f4340` WORLD03/AREA118.EMI#13 | `659e0` | encoding | read | flag base + 0x5f8 |
| 790 | `801f43fc` WORLD03/AREA118.EMI#13 | `65a8c` | encoding | read | flag base + 0x5f8 |
| 791 | `801f4634` WORLD03/AREA118.EMI#13 | `6675c` | encoding | rule | constants re-match tolerantly |
| 792 | `801f301c` WORLD03/AREA119.EMI#13 | `66b58` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 793 | `801f3324` WORLD03/AREA119.EMI#13 | `66ea8` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 794 | `801f3608` WORLD03/AREA119.EMI#13 | `67118` | encoding | read | scratchpad-pointer clear 80197070 = 3154 |
| 795 | `801f369c` WORLD03/AREA119.EMI#13 | `6770c` | platform | read | CLUT word 0x78ca becomes 0x3a0; video-mode test dropped |
| 796 | `801f3010` WORLD03/AREA120.EMI#13 | `32ed4` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 797 | `801f3d94` WORLD03/AREA120.EMI#13 | `84718` | encoding | rule | constants re-match tolerantly |
| 798 | `801f39f0` WORLD03/AREA121.EMI#14 | `24e0b0` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 799 | `801f430c` WORLD03/AREA121.EMI#14 | `24e5b0` | encoding | rule | constants re-match tolerantly |
| 800 | `801f5028` WORLD03/AREA121.EMI#14 | `24eb14` | encoding | rule | callee twin missed |
| 801 | `801f5aa0` WORLD03/AREA121.EMI#14 | `244020` | platform | rule | CLUT word becomes a PSP texture id; Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 802 | `801f2fc4` WORLD03/AREA125.EMI#13 | `540fc` | encoding | rule | calls compared by address agree |
| 803 | `801f31d0` WORLD03/AREA128.EMI#13 | `250d4c` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 804 | `801f3760` WORLD03/AREA130.EMI#13 | `250ff4` | encoding | read | callee twin missed (8015dcf0 = 95118, the sequence-play wrapper) |
| 805 | `801f37b8` WORLD03/AREA130.EMI#13 | `251044` | encoding | read | as row 804 |
| 806 | `801f2f54` WORLD03/AREA131.EMI#13 | `251a70` | encoding | rule | callee twin missed; constants re-match tolerantly |
| 807 | `801f3560` WORLD03/AREA132.EMI#13 | `4a198` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 808 | `801f2c1c` WORLD03/AREA133.EMI#13 | `251efc` | encoding | read | jump-table case (store 0x1e / 4) inlined by the PSP; PSX case body checked equal |
| 809 | `801f40c8` WORLD03/AREA134.EMI#13 | `4f018` | encoding | read | scratchpad-pointer clear 80197070 = 3154 |
| 810 | `801f45a0` WORLD03/AREA134.EMI#13 | `54668` | encoding | rule | constants re-match tolerantly |
| 811 | `801f46ec` WORLD03/AREA134.EMI#13 | `54b60` | encoding | rule | calls compared by address agree |
| 812 | `801f3abc` WORLD03/AREA135.EMI#14 | `3e730` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 813 | `801f46ac` WORLD03/AREA135.EMI#14 | `252d34` | encoding | rule | constants re-match tolerantly |
| 814 | `801f4e7c` WORLD03/AREA135.EMI#14 | `253410` | encoding | rule | constants re-match tolerantly |
| 815 | `801f60e0` WORLD03/AREA135.EMI#14 | `253e10` | encoding | rule | constants re-match tolerantly |
| 816 | `801f3018` WORLD03/AREA136.EMI#13 | `254960` | encoding | rule | constants re-match tolerantly |
| 817 | `801f3288` WORLD03/AREA139.EMI#13 | `48d24` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 818 | `801f4364` WORLD03/AREA139.EMI#13 | `254d8c` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 819 | `801f3950` WORLD03/AREA140.EMI#13 | `71774` | platform | rule | video-mode test (GetGraphType) dropped |
| 820 | `801f3c44` WORLD03/AREA140.EMI#13 | `719dc` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap); PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 821 | `801f4380` WORLD03/AREA140.EMI#13 | `25554c` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 822 | `801f3740` WORLD03/AREA141.EMI#13 | `255950` | encoding | rule | constants re-match tolerantly |
| 823 | `801f390c` WORLD03/AREA141.EMI#13 | `255acc` | encoding | rule | constants re-match tolerantly |
| 824 | `801f2cb0` WORLD03/AREA142.EMI#13 | `6c7f8` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 825 | `801f30a0` WORLD03/AREA143.EMI#13 | `4dccc` | encoding | read | callee twin missed (801f2d7c = 4d964) |
| 826 | `801f3584` WORLD03/AREA143.EMI#13 | `7e5b4` | platform | read | CLUT word 0x7940 becomes 0x500; video-mode test dropped |
| 827 | `801f3a20` WORLD03/AREA144.EMI#13 | `25749c` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 828 | `801f332c` WORLD03/AREA145.EMI#14 | `31a54` | platform | rule | Packet_Commit length argument |
| 829 | `801f43dc` WORLD03/AREA145.EMI#14 | `257e10` | encoding | rule | constants re-match tolerantly |
| 830 | `801f4da4` WORLD03/AREA145.EMI#14 | `2580f4` | encoding | read | (x >> 7) & 0xc against ((x >> 9) & 3) << 2; lui/ori across a delay slot |
| 831 | `801f361c` WORLD03/AREA148.EMI#13 | `6fbe8` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 832 | `801f3bd0` WORLD03/AREA148.EMI#13 | `2592f0` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 833 | `801f4798` WORLD03/AREA148.EMI#13 | `259e70` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 834 | `801f2cd4` WORLD03/AREA149.EMI#13 | `52fc0` | encoding | read | callee twin missed (801f2de0 = 53084) |
| 835 | `801f3948` WORLD03/AREA151.EMI#13 | `25ba74` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 836 | `801f4264` WORLD03/AREA151.EMI#13 | `25bf74` | encoding | rule | constants re-match tolerantly |
| 837 | `801f389c` WORLD04/AREA152.EMI#13 | `25daec` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 838 | `801f41b8` WORLD04/AREA152.EMI#13 | `25dfec` | encoding | rule | constants re-match tolerantly |
| 839 | `801f2c04` WORLD04/AREA155.EMI#13 | `25edac` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 840 | `801f31bc` WORLD04/AREA167.EMI#13 | `72744` | platform | rule | video-mode test (GetGraphType) dropped |
| 841 | `801f4080` WORLD04/AREA167.EMI#13 | `25f34c` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 842 | `801f2e10` WORLD04/AREA168.EMI#13 | `51fa0` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 843 | `801f3914` WORLD04/AREA168.EMI#13 | `74f78` | platform | rule | video-mode test (GetGraphType) dropped |
| 844 | `801f3e18` WORLD04/AREA168.EMI#13 | `747dc` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 845 | `801f3fbc` WORLD04/AREA168.EMI#13 | `7497c` | encoding | rule | constants re-match tolerantly; PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 846 | `801f3524` WORLD04/AREA169.EMI#13 | `2600e4` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 847 | `801f3994` WORLD04/AREA170.EMI#13 | `4c60c` | encoding | rule | constants re-match tolerantly |
| 848 | `801f4640` WORLD04/AREA170.EMI#13 | `73848` | platform | rule | video-mode test (GetGraphType) dropped |
| 849 | `801f3764` WORLD04/AREA171.EMI#13 | `26151c` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 850 | `801f32e8` WORLD04/AREA172.EMI#13 | `55e64` | encoding | read | callee twins missed (801f3984 = 56500, 801f3fac = 5688c) |
| 851 | `801f36c0` WORLD04/AREA172.EMI#13 | `56228` | platform | rule | Packet_Commit length argument |
| 852 | `801f4324` WORLD04/AREA172.EMI#13 | `261c44` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 853 | `801f47f0` WORLD04/AREA172.EMI#13 | `262048` | encoding | rule | constants re-match tolerantly |
| 854 | `801f33c4` WORLD04/AREA173.EMI#14 | `80980` | encoding | read | flag base + 0x650 (CD1: relocated low half) |
| 855 | `801f3fbc` WORLD04/AREA173.EMI#14 | `81424` | platform | rule | video-mode test (GetGraphType) dropped |
| 856 | `801f4a4c` WORLD04/AREA173.EMI#14 | `2629f4` | encoding | rule | constants re-match tolerantly |
| 857 | `801f2fec` WORLD04/AREA174.EMI#13 | `50a4c` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0); Psy-Q call on the PSX side (rsin/rcos, SetDrawMode, GetGraphType) only |
| 858 | `801f2e58` WORLD04/AREA186.EMI#13 | `269ef0` | encoding | rule | constants re-match tolerantly |
| 859 | `801f3368` WORLD04/AREA188.EMI#13 | `5ef78` | encoding | rule | callee twin missed |
| 860 | `801f37cc` WORLD04/AREA188.EMI#13 | `5f390` | encoding | read | callee twins missed (801f3adc = 5fb8c, 801f3de0 = 5f6c0, 801f3e7c = 5f750, 801f44b4 = 5fae0) |
| 861 | `801f3514` WORLD04/AREA189.EMI#13 | `7d188` | platform | read | full-frame rectangle -> (-32, 0, 384, 240) |
| 862 | `801f497c` WORLD04/AREA189.EMI#13 | `7c8fc` | platform | read | GPU packet allocator: 24-bit ordering-table links (0xffffff / 0xff000000) become full pointers |
| 863 | `801f4a04` WORLD04/AREA189.EMI#13 | `7c980` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 864 | `801f2dbc` WORLD04/AREA191.EMI#13 | `5531c` | encoding | read | callee twin missed (801f2e34 = 5538c) |
| 865 | `801f2ea0` WORLD04/AREA193.EMI#13 | `26d5fc` | encoding | rule | calls compared by address agree |
| 866 | `801f3524` WORLD04/AREA198.EMI#13 | `26e970` | encoding | rule | PSP runtime call (abs 2c553c, memcpy 2c575c, draw-mode 9fd54, sin 9c7b0) |
| 867 | `8014b8f8` SLPS_009.90 | `f64` | encoding | rule | constants re-match tolerantly |
| 868 | `8014ce90` SLPS_009.90 | `1300` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 869 | `8014d440` SLPS_009.90 | `26f0` | encoding | read | function boundary: PSX 8014dafc is the 3-instruction head of 8014db0c (PSP 2ce4) |
| 870 | `8014db0c` SLPS_009.90 | `2ce4` | encoding | rule | constants re-match tolerantly |
| 871 | `8014df9c` SLPS_009.90 | `32bc` | platform | read | file I/O: CdRead / CdReadSync become PSP no-ops |
| 872 | `8014ecac` SLPS_009.90 | `64f4` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 873 | `8014ecd4` SLPS_009.90 | `6518` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 874 | `8014ecfc` SLPS_009.90 | `653c` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 875 | `8014ed24` SLPS_009.90 | `6560` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 876 | `8014ed4c` SLPS_009.90 | `6584` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 877 | `8014ed74` SLPS_009.90 | `65a8` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 878 | `8014ed9c` SLPS_009.90 | `65cc` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 879 | `8014edc4` SLPS_009.90 | `65f0` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 880 | `8014f028` SLPS_009.90 | `6854` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 881 | `8014f068` SLPS_009.90 | `6890` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 882 | `8014f090` SLPS_009.90 | `68b4` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 883 | `8014f2f4` SLPS_009.90 | `6b18` | encoding | read | function boundary: PSX 8014f3d0 (12 instr.) + 8014f414 = PSP 6bd4 |
| 884 | `8015002c` SLPS_009.90 | `9638` | encoding | rule | constants re-match tolerantly |
| 885 | `80154780` SLPS_009.90 | `d1c0` | encoding | rule | constants re-match tolerantly |
| 886 | `80155238` SLPS_009.90 | `f5c0` | encoding | rule | constants re-match tolerantly |
| 887 | `80155a94` SLPS_009.90 | `ed08` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 888 | `80156844` SLPS_009.90 | `103fc` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 889 | `80157308` SLPS_009.90 | `1090c` | encoding | rule | constants re-match tolerantly |
| 890 | `801576d4` SLPS_009.90 | `10a6c` | encoding | rule | constants re-match tolerantly |
| 891 | `80157b38` SLPS_009.90 | `12484` | platform | rule | Packet_Commit length argument; video-mode test (GetGraphType) dropped |
| 892 | `80158100` SLPS_009.90 | `10d80` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 893 | `8015815c` SLPS_009.90 | `10df0` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 894 | `801587a0` SLPS_009.90 | `13068` | encoding | rule | constants re-match tolerantly |
| 895 | `80158a54` SLPS_009.90 | `10e60` | encoding | rule | constants re-match tolerantly |
| 896 | `80158b60` SLPS_009.90 | `10f90` | encoding | rule | constants re-match tolerantly |
| 897 | `801593fc` SLPS_009.90 | `11938` | encoding | rule | constants re-match tolerantly; globals unmapped (data map gap) |
| 898 | `8015c890` SLPS_009.90 | `8e6a8` | encoding | read | division by 360 (PSP div, PSX reciprocal multiply) |
| 899 | `8015d140` SLPS_009.90 | `8edc0` | encoding | rule | constants re-match tolerantly |
| 900 | `8015d860` SLPS_009.90 | `94e94` | platform | read | sound: SsStart2 / SsSetSerialAttr / SsSetSerialVol become PSP no-ops |
| 901 | `80162610` SLPS_009.90 | `95c6c` | encoding | read | callee twin missed (8015dcf0 = 95118) |
| 902 | `80163d68` SLPS_009.90 | `97c08` | platform | read | VRAM upload (LoadImage) becomes the PSP chunked upload; similarity-weak pair |
| 903 | `80165dcc` SLPS_009.90 | `b86c4` | encoding | rule | globals unmapped (data map gap) |
| 904 | `8016776c` SLPS_009.90 | `dcf40` | encoding | rule | constants re-match tolerantly |
| 905 | `801afba0` SLPS_009.90 | `9a2c0` | encoding | read | zero-fill key; GAME.EMI 801afba0 re-matches clean |
| 906 | `801afd54` SLPS_009.90 | `9b1ac` | encoding | read | zero-fill key; GAME.EMI 801afd54 re-matches clean |
| 907 | `801bf0b8` SLPS_009.90 | `d5edc` | encoding | read | zero-fill key; GAME.EMI 801bf0b8 re-matches clean |
| 908 | `801eec30` SLPS_009.90 | `2111f8` | encoding | read | zero-fill key with no PSX code in the field captures (0x801eec30 is MAGIC081 code); SHOP/START call an uncaptured section there |

## 4. Not verified

- **The 270 encoding and 55 platform rows decided by rule were not read
  instruction by instruction.** The tolerant pass allows +-1 and scaling, so
  a threshold moved by one (a `slti` bound 20 -> 21) or a value doubled
  would pass it; the strict pass catches changed load-immediates but not
  changed compare bounds or changed field offsets (offsets are not compared
  at all: the PSP's bases move them). Rows 733 and 678 are the kind this
  misses; others like them may remain.
- **Jump-table case bodies** outside the PSX bound were checked for row 808
  only; rows 549 and 739 assume theirs match.
- **The zero-fill rows** (482-484, 905-907) were compared with the GAME.EMI
  copy by the tolerant pass only; row 908's PSX code is not in the captures.
- **AREA030's PSP-only state**: what sets it in play, what `0x20f4d4` is, and
  whether `0x11b30a`/`0x11b320`/`0xc4478` belong to the same feature, are
  not traced. Not seen in play.
- **Row 733's effect** (message 116 replayed) is inferred from the
  sibling's text-engine notes; not seen.
- **Row 576**: only the table's shape (five entries, v and width) was read;
  which title-menu items it serves is a guess.
- **The PSX side is JP only** (the sibling's captures). A content row could
  be a JP -> US revision the PSP inherited rather than a PSP change; no US
  overlay was read.
- **The widescreen rows** are classed from the constants; none was rendered.
- **PSP-EU** was checked for rows 733, 677 and 576 only; the other rows'
  EU twins come from `psp_pair`'s JP -> EU map.

## 5. For the other files (the coordinator's)

- **`docs/README.md`, one row:** `psp-code-diff-field-2.md` | Stage B of
  the PSP code diff, field half rows 455..908 (2026-10-12): 9 content, 85
  platform, 360 encoding. Content: AREA068's message-box handler (master
  state 5 -> 6, message index -> 116), AREA030's PSP-only state gating
  added branches, a title label table, a 1-px move. Stage A's resolve sends
  overlay calls into GAME.EMI to empty boot keys (156 of 454 rows); three
  mispairs and two library mispairs named.
- **No ledger entry**: a reading, not a behaviour change. If the round
  adopts row 733 or the AREA030 addition, that adoption needs one (DIV
  letter for CD).
- **For `psp-widescreen.md` 3a (its owner's call):** four sites the PSP
  changed that 3a does not list - AREA172's sprite (rows 466, 467), the
  terrain strip ring beside `MapView_Build` (row 478), SHOP's left clamps
  (rows 567, 568).
- **For whoever next runs `psp_pair.py`:** resolve an overlay's call through
  its own file and the resident module before the boot range (or bound the
  boot range by the boot EXE's real end); pair `rsin` with `0x9c7b0`,
  `SetDrawMode` with `0x9fd54`, `80167628` with `dce04`; swap the
  `801a2c64` / `801a2ee0` pair. CD1 owns the tool on its branch; nothing
  here changed it.
- **For SI and IM:** nothing in this share stops our code running on the
  PSP's consumables or item 87; the one consumables reader here (COMMU04
  `801d12c8`) is encoding.

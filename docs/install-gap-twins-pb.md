# The PSP's `BOOT.BIN` as a twin's file (install-gap round, group PB)

**Status:** MEASURED (2026-10-18; branch `install-gap/pb` off `06a209bd`).
Headless only: the recipes regenerated over the seven held discs in the
group's worktree (not committed), `tools/exe_scan.py` on both PSP discs, the
`psp-jp` and `psp-eu` caches built and the exe host's self-test from each
(harness root `C:\Users\kerok\g_pb`). Nothing played, nothing rendered, not
seen by the owner. The coordinator folds this into TW's
`docs/install-gap-twins.md` at the merge
([`install-gap-round.md`](install-gap-round.md) 2, PB). Addresses, symbol
names and counts only (CLAUDE.md rule 1); the session scripts (`probe2.py`,
`probe3.py`, `psprefs.py`, `pspquad.py`, `psxrefs.py`, `gapcount.py`) stay in
the session scratchpad `pb/`.

## 1. What was built: `BOOT.BIN`'s reader (`tools/psp_exe.py`)

[`exe-image-twins-1013.md`](exe-image-twins-1013.md) 6 named the class: on
the PSP discs many remaining runs are held by one overlay **and** by
`BOOT.BIN`, and the PSP's native code reads `BOOT.BIN`'s copy. `twin_place`
took the first equal bytes in the file, which is safe for a 64-byte run and
not for a four-byte one. Built:

- **`Elf.text_refs`**: the ELF's `.text` hi16 / lo16 relocation pairs
  settled, `{address: [instructions that read it]}`. A relocation's info is
  its type, the program header of its place and the one of its address
  (bits 0..7, 8..15, 16..23); the image is unrelocated at 0, so a pair's
  value is the address. A lo16 pairs with the latest hi16 whose `lui` wrote
  its base register, in the same header, at most 0x400 bytes before it: the
  PSP's compiler interleaves pairs, and on PSP-EU the list order alone pairs
  13,903 of 72,346 lo16s with a `lui` of another register; by register,
  71,474 pair and the rest have no hi16 (`probe2.py`, `pspquad.py`).
  `Elf.pointer_words`: the `.rodata` and `.data` words a `R_MIPS_32` names.
- **`boot_place(elf, want, lo)`**: among the copies of the run in `.rodata`
  and `.data`, the one a relocated pair reads **into** (not merely near) and
  whose place agrees with the PC's pointer words before `lo` in its
  `symbols.toml` symbol (each a relocated word in the copy's place too).
  Exactly one, or no row (the reason printed).
- **A `twin` that names `BOOT.BIN`** is placed by `boot_place`, PSP builds
  only; a sixth field (n-th copy) or a PSX build is refused loudly (`fail`).
  `check` refuses a `BOOT.BIN` twin row on a PSX build.
- **The rule `margins`** (section 3): `Sprite_ScreenMargins` from the copy
  the PSP's cull reads, its x margins less 32.

**The worked example and the six rows before it.** `ItemUse_HandlerIndex+0xD0`
and the five other `BOOT.BIN` twins already in `TWINS`
(`EffectKind2A_States+0x10`, the four `MapCell_Upright*` tables) are placed
by the reader at the same addresses as before on both discs - `recipes/exe-psp.tsv`'s
existing rows are byte-identical after the regeneration - each now with its
readers printed (`ItemUse_HandlerIndex+0xD0`: PSP-EU `0x2DEDB8`, read at
`0xC7E4`, `0xC840`, ... ; PSP-JP `0x2DF5C8`).

## 2. The rows (for `TWINS`, stage B)

Not in the committed `TWINS` (group TW owns the list this stage); tested in
the worktree and handed back for stage B to append after TW merges. Every
row's bytes are `BOF3.exe`'s ("0 not BOF3.exe's" on both PSP builds).

| Range | Symbol | PSP-JP place / readers | PSP-EU place / readers | Other copies on the disc |
|---|---|---|---|---|
| `0x663B20..0x663B28` | the default light pair (`EventScript_OpLengths+0x14`; `Area_Enter`'s `kDefaultLight`) | `0x2E5B78` / `0x855BC`, `0x855C0` | `0x2E5390` / `0x852F4`, `0x852F8` | `BMAGIC/MAGIC095.EMI#3` once |
| `0x66978C..0x6697A2` | `MoveScript_EffectArg+0x48`, a leader climbing move script | `0x2E54E6` / `0xCF3F0`, `0xDBA7C`, `0xDBFFC` | `0x2E4CFE` / `0xCF104`, `0xDB794`, `0xDBD14` | none |
| `0x6697A4..0x6697B0` | `MoveScript_EffectArg+0x60`, the next script's head (to the gap's end) | `0x2E54FC` / `0xDBAA4`, `0xDBCD4` | `0x2E4D14` / `0xDB7BC`, `0xDB9EC` | none |
| `0x65E2DC..0x65E2E0` | `EffectKind18Sub1C_States+0x14` | `0x2E202C` / `0x6B270` | `0x2E1844` / `0x6AFA4` | `WORLD02/AREA102.EMI#13` |
| `0x65E2F4..0x65E2F8` | `EffectKind18Sub1D_States+0x14` | `0x2E2044` / `0x6B820` | `0x2E185C` / `0x6B554` | `WORLD02/AREA102.EMI#13` |
| `0x65EBD4..0x65EBD8` | `EffectKind18Sub33_States+0x14` | `0x2E290C` / `0x78E50` | `0x2E2124` / `0x78B88` | `WORLD04/AREA164.EMI#13` |
| `0x65F578..0x65F57C` | `EffectKind18Sub64_States+0x1C` | `0x2E32BC` / `0x8442C` | `0x2E2AD4` / `0x84164` | `WORLD03/AREA120.EMI#13` |

Each `EffectKind18Sub*` copy is the only one in `BOOT.BIN`, and the
code-pointer words before it (five for `1C`, `1D`, `33`, seven for `64`,
the states' handlers) are relocated words at the same offsets: the copy is
the table's own tail, not four equal bytes elsewhere. The PSX builds keep
these four as gaps (OF-E's owning-function rows, round staging 3): no PSX
disc file holds them where its overlay reads them.

**The light pair.** The gap row was `0x663B1C..0x663B28`; its PC bytes
`0x663B1C..0x663B20` are zero, and the pair the readers address starts at
`0x663B20`, so the row is the pair's eight bytes. `MAGIC095` holds the
twelve bytes too, but no code of the PSP reads an overlay's copy - the
twin names `BOOT.BIN`, whose copy one PSP function reads twice (the
`addiu` / `addu` pair above; which function it twins on the PC was not
traced).

**`MoveScript_EffectArg+0x45`** (the census: 1 of 3 windows). Not one run
on the PSP disc: the PSP packs the move scripts without the PC's (and the
PSX's) alignment padding. At the PSP-EU map's displacement (`exe_maps/psp-eu.tsv`
places `0x669750..0x669789` from `0x2E4CC5`) the PC's three zero bytes at
`0x669789..0x66978C` are absent on the PSP, and so are the two at
`0x6697A2..0x6697A4`; the two scripts between are verbatim, each where the
PSP's code points (`psprefs.py`). So the row is two twins, the scripts
only; the five padding bytes are zero on the PC and stay zero. With both,
the gap row is gone.

Ready to append (stage B), after TW's own rows:

```python
    # The PSP's BOOT.BIN copies its own code reads (docs/install-gap-twins.md, PB):
    # each placed by boot_place (a relocated pair into the copy, the symbol's
    # pointer words in place), PSP builds only. The light pair is MAGIC095's too;
    # MoveScript_EffectArg's two scripts sit without the PC's padding on the PSP.
    (0x663B20, 0x663B28, "BOOT.BIN", "the default light pair (Area_Enter's kDefaultLight, Light_Angles)", PSP),
    (0x66978C, 0x6697A2, "BOOT.BIN", "MoveScript_EffectArg+0x48: a leader climbing move script", PSP),
    (0x6697A4, 0x6697B0, "BOOT.BIN", "MoveScript_EffectArg+0x60: the next move script's head", PSP),
    (0x65E2DC, 0x65E2E0, "BOOT.BIN", "EffectKind18Sub1C_States+0x14", PSP),
    (0x65E2F4, 0x65E2F8, "BOOT.BIN", "EffectKind18Sub1D_States+0x14", PSP),
    (0x65EBD4, 0x65EBD8, "BOOT.BIN", "EffectKind18Sub33_States+0x14", PSP),
    (0x65F578, 0x65F57C, "BOOT.BIN", "EffectKind18Sub64_States+0x1C", PSP),
```

## 3. `Sprite_ScreenMargins`: not a twin, the PSP's 16:9 framing undone

`0x663B28..0x663B30` is on no file of either PSP disc verbatim, nor within
two bytes of it anywhere in `BOOT.BIN`'s `.rodata` / `.data`; its second
entry's four bytes occur in no file (`probe4.py`, `probe5.py`). The reader
settles why. The PSX reads the table from its boot EXE (PSX-JP
`0x80182AEC`, four `lbu` at `0x8015B588..0x8015B5E8`, `psxrefs.py`).
`BOOT.BIN`'s eight bytes after the light pair (PSP-EU `0x2E5398`, PSP-JP
`0x2E5B80`) are read by one relocated pair, PSP-EU `0x8BD1C` (`addiu
s1, 0x5398`), in the twin of `Sprite_ProjectA`: the same load-vector /
perspective / store-xy triple as the other culls
([`psp-widescreen.md`](psp-widescreen.md) 3), then entry 0 or 1 by the
record's byte 3 bit 3, `x > 0x140 + [0]`, `x < -[1]`, `y > 0xF0 + [2]`,
`y < -[3]`, and the 40-entry draw list (`0x8BE3C..0x8BEBC`) - the PC's
function. Its bytes are the PC's with **both x margins of each entry 32
larger** and the y margins equal: the PSP's 32 columns either side of its
384-wide view ([`psp-widescreen.md`](psp-widescreen.md) 2.4), the class
DIV-0103's amendment undoes for `MsgBox_PlacementTable` (`unwide`) and the
dialogue box tables (`boxxy`). (That doc's 3b said the sprite culls were
left alone; this one table was not - its margins moved, the code did not.)

**Built:** the rule `margins`, PSP builds: `margins_place` finds the copy as
`boot_place` finds a twin's (the PC's bytes with the x margins + 32, a
relocated pair into it), and `build_fill` writes x - 32, y as read; refused
loudly if a margin would go under zero, and a row is written only when the
result equals `BOF3.exe`'s. Both PSP builds: one row, 8 bytes, "0 not
BOF3.exe's". The rule is in the committed code, so the coordinator's
regeneration after this branch merges writes the two `margins` rows; the
seven twins per build wait for stage B's `TWINS` lines. DIV-0041's wide modes keep reading the PC's margins
(DIVERGENCE's ultrawide-culls entry), so this is what our engine expects.

## 4. Checks

- **Regeneration** (`g_pb/regen.sh`, `regen2.sh`: `psp_exe.py recipe`,
  `exe_tables.py recipe`, `exe_own.py measure`, `exe_tables.py gaps`,
  `exe_tables.py recipe`, the seven discs): 16 new `exe-psp.tsv` rows (seven
  `twin` and one `margins` per PSP build), every one "0 not BOF3.exe's"; no
  existing row moved. `exe-gaps.tsv` loses exactly the seven PSP rows per
  PSP build the round named and gains none; the PSX builds unchanged.
  `exe_tables.py check` 0 errors, `psp_exe.py check` OK.
- **Gaps** (`gapcount.py`: rows / bytes / PC non-zero bytes, against
  `06a209bd`'s file): PSP-JP 92 / 1,069 / 628 to **85 / 994 / 575**,
  PSP-EU 93 / 1,079 / 634 to **86 / 1,004 / 581**.
- **`tools/exe_scan.py`** on both PSP discs (`g_pb/exe_scan`): none of the
  rows' symbols is listed with a byte that is not the PC's; `missing/gap`
  575 (PSP-JP) and 581 (PSP-EU), the gap file's non-zero counts.
- **The exe host, headless**, from each cache (section 5).

## 5. The self-tests

The caches built from this branch's regenerated recipes with the group's
rows applied (`importer.py build --source <iso> --lang en-150` / `ja-JP`,
then `install --base` into an empty directory; `g_pb/c_psp*`, `g_pb/g_psp*`).
In both `base/exe/data.bin`, every byte of `0x663B1C..0x663B30`,
`0x669789..0x6697B0` and the four `EffectKind18Sub*` rows equals
`BOF3.exe`'s (`cachecmp.py`). The exe host from an ini-less copy of this
branch's build (`BOF3X_SELFTEST_ONLY=1 BOF3X_EXEIMAGE=<cache>
BOF3X_CACHE=<cache> BOF3X_LANG=<tag>`):

| Install | Exit | `inject:` | `BOF3X_EXEIMAGE gap` | `BOF3X_EXEIMAGE_STRICT=1` |
|---|---|---|---|---|
| PSP-EU only (`en-150`) | 0, "self-test only: done" | 10088 ours | 86 ranges, 1,004 bytes | exit 3 at `0x604958` `Area65_DriftUV` (port-relaid, the owner's) |
| PSP-JP only (`ja-JP`) | 0, "self-test only: done" | 10087 ours | 85 ranges, 994 bytes | exit 3 at `0x604958` `Area65_DriftUV` |

The gap totals are the regenerated `exe-gaps.tsv`'s. None of the rows of
section 2 or 3 is in either log's gap list. (The one function fewer under
`ja-JP` was not looked into; this branch changes nothing in `src/`.)

## 6. For the ledger (drafts; the coordinator numbers)

- **DIV-A (amends DIV-0103's 16:9 amendment):** `Sprite_ScreenMargins` on a
  PSP-built image. *Original:* a PSP-only image left the eight bytes zero
  (no source), so every sprite outside the bare 320 x 240 frame was flagged
  off-screen and kept off `Sprite_DrawList`; the PSP itself culls with each
  x margin 32 wider. *New:* the PSP's table read back to the 4:3 frame (x -
  32, y copied), equal to `BOF3.exe`'s and a PSX-built image's. *Why:* the
  same rule as `unwide` / `boxxy`: the PSP's base without its re-framing,
  wide anchoring being DIV-0041's in code. *Reversible:* drop the recipe
  row.
- **DIV-0101 (amended in place, as the 2026-10-13 twins were):** the seven
  `BOOT.BIN` twin rows of section 2, once stage B appends them, and
  `boot_place` as how a `BOOT.BIN` twin's copy is chosen. No new number.

No defect was observed (no route reaches these bytes in this group's
headless work); none is described.

## 7. Not PB's, not done

`Char_WhelpSlot+0x4` (one byte, no safe twin) and `ConfigMenu_Steps+0xC`
(RD's reader first) are untouched. The PSP's other one-overlay-and-`BOOT.BIN`
runs of [`exe-image-twins-1013.md`](exe-image-twins-1013.md) 6
(`Area104_PanelTris+0x30`, `BossGaist_Anims+0x2`, ...) are TW's / OF's
rows by the round's split; with `boot_place` in, any of them can be a
`BOOT.BIN` twin on the PSP builds by one `TWINS` line.

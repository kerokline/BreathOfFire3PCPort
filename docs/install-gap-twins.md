# The install-gap round's PlayStation twins (TW), and the pieced rule

**Status:** MEASURED (2026-10-18, group TW of the install-gap round,
[`install-gap-round.md`](install-gap-round.md) section 2; branch
`install-gap/tw` off `06a209bd`). Headless only: the recipes regenerated over
the seven held discs in the group's worktree (not committed), `exe_scan.py`
over the seven, one PSX-US and one PSP-EU cache rebuilt and the exe host's
self-test from them. Nothing played, nothing rendered, not seen by the owner.
Every byte the rows fill is `BOF3.exe`'s; the scratch scripts (`search.py`,
`survey.py`, `pieces.py`, `atdisp.py`, `infile.py`, `nbr.py`, `kinds.py`,
`gapcount.py`) and their outputs stay in the harness root `C:\Users\kerok\g_tw`
(derived from the player's files, never in the tree; rule 1).

The census's P2 item 11 and P1 item 7 ([`install-gap-census.md`](install-gap-census.md)
8): the `disc-unplaced` rows whose 16-byte windows the disc search found. Section
1 is the rows, 2 the pieced rule, 3 what was not placed, 4 the measurements.
Group PB adds its `BOOT.BIN` rows as section 5.

## 1. The rows

Method. For each census row and build, every file of the disc
(`psp_exe.sources_of`) searched for the PC's run (`survey.py`); where no file
held it whole, the longest runs a file holds (`pieces.py`), then the place
those runs share (`atdisp.py`: one displacement, which bytes equal the PC's
there). The reader: the `lui` + load / store / `addiu` pairs of one register
into the table in the file's own code (`infile.py`, the 1013 method), the
effect kind's handler from the PSX `Effect_KindHandlers` (`0x801C8C68`) read
in the candidate file (`kinds.py`), or the PSX-US map's segments either side
(`nbr.py`). A range ends where the disc's own bytes begin; "full" means every
byte the PC has non-zero in the census row is inside it.

| Range | Symbol | File | Copies / readers | Choice | Builds | Covers |
|---|---|---|---|---|---|---|
| `0x62034C..0x6203A4` | `Area115_Sprites` | `WORLD03/AREA115.EMI#13` | 1 / 4 | eleven world-map overlays hold the 88 bytes; the area's own overlay, as `Area16_Buttons` .. `Area88_*` took theirs | all 7 | full |
| `0x6203A4..0x6203BC` | `Area115_Buttons` | `WORLD03/AREA115.EMI#13` | 1 / 8 | as above, the next 0x58 bytes | all 7 | full |
| `0x62390C..0x623910` | `Area121_Cells+0x26C` | `WORLD03/AREA121.EMI#13` | 1 in the file / 0 | 72 files hold the four bytes; AREA121's one copy is at `0x80117004`, the four bytes before the PSX-US map's next segment (`0x623910..` from `AREA121.EMI#13` at `0x80117008`, one displacement) | PSX 5 | full |
| `0x635344..0x635364` | `Area146_EffectStates+0x1230` (area 148's attach vectors) | `WORLD03/AREA148.EMI#13` | 1 / 15 | group RD's reader ([`install-gap-readers.md`](install-gap-readers.md) 4.2): `Area_Descriptors[148]` +0x0C's table at `0x635340`, read by `MoveCmd_AttachOffset`; the table + 4 (`0x801F60B4` US / JP / PSP, `0x801F60B8` EU / FR / DE). 36 of 36 bytes equal on all seven. Also covers the census's game-value row `+0x124C` | all 7 | full, both rows |
| `0x63CA48..0x63CB03` | `Area169_Rects+0xC`, the three dial grids | `WORLD04/AREA170.EMI#13` | 1 / 3 | P1 item 7: section 2 | PSX 5 | full |
| `0x63D5D8..0x63D5DC` | `Area169_Rects+0xB9C` (area 170's attach vectors) | `WORLD04/AREA170.EMI#13` | 3 / - | RD 4.2: `Area_Descriptors[170]` +0x0C's table at `0x801F6448`, + 0x10; AREA170 holds the four bytes three times (`0x801F6310`, `0x801F6382`, `0x801F6458` on all seven), the third the table's (`TWINS`' sixth field 2). The table's 20 bytes are equal on all seven | all 7 | full |
| `0x64D478..0x64D484` | `BossDolphin_Anims` | `BOSS/BOSS029.EMI#16` | 1 / 1 | BOSS012, 013 (twice), 021 and 029 hold it; the PSX-US map's segments either side are BOSS028's and BOSS030's | all 7 | full |
| `0x64D798..0x64D7A0` | `BossGaist_Sounds` | `BOSS/BOSS035.EMI#16` | 1 / 2 | BOSS035, 042, 047 hold it; the map's segments either side are BOSS034's and BOSS036's | all 7 | full |
| `0x653FA8..0x653FB9` | `EffectKind3C_Op` | `SCENARIO/SCENA03.EMI#0` | 1 / 1 | one file; the three bytes after it are the disc's, zero on the PC | US, EU, FR | full |
| `0x6543F0..0x654400` | `EffectKind33_SignX` | `WORLD00/AREA026.EMI#13` | 1 / 1 | AREA026 and AREA134 hold the 32 bytes of SignX + SignZ (the PC's `EffectKind99_States+0x10` holds the same 32). PSX `Effect_KindHandlers[0x33]` = `0x801F2C04`, which in AREA026 loads the state table 0x10 before the copy; `[0x99]` = `0x801F455C` does the same in AREA134. So kind 0x33's tables are AREA026's | all 7 | full |
| `0x654400..0x654410` | `EffectKind33_SignZ` | `WORLD00/AREA026.EMI#13` | 1 / 1 | as above | all 7 | full |
| `0x654D88..0x654D99` | `EffectKind86_States+0x14`, `EffectKind86_Spawn`'s 17-byte EventOp operand | `SCENARIO/SCENA13.EMI#0` | 1 / 2 | one file. The census row `0x654D84..0x654D9C`: every byte the disc holds otherwise is zero on the PC, on every build (`atdisp.py`), which is why the 16-byte windows missed it | all 7 | full |
| `0x6550E0..0x655100` | `EffectKind99_States+0x10` | `WORLD03/AREA134.EMI#13` | 1 / 3 | kind 0x99's handler (above) | all 7 | full |
| `0x655208..0x655210` | `EffectKindA0_States+0x24` | `SCENARIO/SCE15EF1.EMI#0` | 1 / 21 | AREA098 holds it too; kind 0xA0's handler `0x801D0C04` in SCE15EF1 loads `0x801D3918`, the copy - 0x24 (the PC's offset); `+0x30` came from the same file in 1013 | all 7 | full |
| `0x65B8F4..0x65B918` | `Fx106_ShardTables` | `BMAGIC/MAGIC106.EMI#3` | 1 / 5 | MAGIC063 holds it too; the map's segments either side are MAGIC105's and MAGIC109's | PSX 5 | full |
| `0x65C1A8..0x65C1B8` | `MagicBallOrb_Shades`, pairs 0..7 | `BMAGIC/MAGIC172.EMI#3` | 1 / 2 | Magic Ball is MAGIC172 ([`magic_s36.md`](magic_s36.md)); the section ends after the 16 bytes | all 7 | 16 of 20 (section 3) |
| `0x65C21C..0x65C224` | `AuraBreathDome_V` | `BMAGIC/MAGIC218.EMI#3` | 1 / 2 | MAGIC131 holds it too; Aura Breath is MAGIC218 (magic_s36.md) | all 7 | full |
| `0x65DE5E..0x65DE76` | `EffectKind18_08_States+0x8A` | `WORLD02/AREA109.EMI#13` | 1 / 4 | 1013's EU row's file and place; the last two bytes are the disc's, zero on the PC | JP, US, FR, DE | full |
| `0x65F438..0x65F44E` | `EffectKind18Sub5D_States+0x14` | `WORLD04/AREA174.EMI#13` | 1 / 7 | one file; the last two bytes as above | JP, US, FR, DE | full |
| `0x66ADBC..0x66ADD6` | `Fish_Kinds+0x72C` (PSX-JP) | `ETC/GAME.EMI#0` | 1 / 35 | the existing all-builds row `0x66ADBC..0x66ADD8` finds no copy on PSX-JP: its last two bytes are the disc's own (zero on the PC) | PSX-JP | full |

## 2. The pieced rule (`TWINS`' seventh field)

**Area 169's panel (P1 item 7).** The census found no copy of the 189 bytes
`0x63CA46..0x63CB03` on any PSX disc and asked for a pieced rule. Measured:
the first two bytes are zero on the PC (alignment before the grids, RD 4.2),
and **the other 187 are one verbatim run in `WORLD04/AREA170.EMI#13`** at
`0x801F58D0` on all five PSX discs - the dial panel is area 170's (its code
reads it: three `lui` / imm pairs, `0x801F3328`, `0x801F3A3C`, `0x801F48E0`
on PSX-US). `AREA169.EMI#13`'s section ends where the PSX-US map's previous
segment does (`0x63CA46`). So the P1 row is one plain twin; the PSP builds
already place it.

**The rule, for the rows that need it.** Three rows hold the PC's bytes at
one place with some of the disc's own bytes between them, so no single run
is BOF3.exe's:

- a `TWINS` entry may carry a seventh field, a tuple of segments inside its
  `[lo, hi)`;
- `twin_place` then finds the place as a disc address of `lo` where **every**
  segment equals BOF3.exe's bytes at the same displacement (the longest
  segment's copies, each checked for the rest in the same section; the sixth
  field still picks the n-th such place);
- `find_rows` writes **one `twin` recipe row per segment**, each at its own
  disc address. The recipe's format, `build_fill` and `check` are unchanged,
  and `fill`'s other-file rule - D268's "a twin takes back a byte a map
  segment of another file placed with another value", D267's MeteorStrike
  lifts - applies to each segment as to any twin row;
- `psp_exe.py check` refuses a segment outside its run, empty or out of
  order.

The bytes between segments are the disc's own value and stay unfilled (zero,
a `gap` the next `exe_tables.py gaps` lists with what is left).

| Run | File, place | Segments | Builds | Left (PC non-zero bytes) |
|---|---|---|---|---|
| `EffectKind66_States+0x18` `0x654770..0x654798` | `WORLD03/AREA135.EMI#14` at `0x801F6230` (1 copy of the long segment, 25 readers) | `0x654772..0x654778`, `0x65477A..0x654780`, `0x654782..0x654798` | Western PSX | three u16 fields `+0x18`, `+0x20`, `+0x28` (3): the Western discs' own, all four alike (JP equals the PC) |
| `EffectKind5B_States+0xC` `0x654858..0x654876` | `SCENARIO/SCENA10.EMI#0` at `0x801FE7CC` (22 readers) | `0x654858..0x65486C`, `0x65486E..0x654870`, `0x654872..0x654874` | Western PSX | three u16 fields `0x65486C`, `0x654870`, `0x654874` (4): the Western discs' own (JP equals the PC whole, and places it) |
| `CommuMusic_CloseSteps+0x28` `0x653104..0x653150` | `ETC/COMMU03.EMI#6` at `0x801D0C04`, the place the PSX-US map takes `0x653104..0x653144` from | `0x65310C..0x65311C`, `0x65313C..0x653142` | FR, DE | the music player's other labels (FR / DE's own words), and the list glyphs `0x653144..` (no disc copy; the census's game-value part) |

The segments are whole fields: a u16 the disc holds equal, or a whole label
the FR / DE disc kept in the PC's words. A field the disc holds otherwise is
left whole, so no value is half one release's and half another's.

## 3. What was not placed, and why

| Row | Builds | Evidence | Where it goes |
|---|---|---|---|
| `Area124_Weights+0x50` `0x62660C..0x626614` | EU, DE | Area 125's weights: the PSX-US map takes `0x62660D..` from `WORLD03/AREA125.EMI#13` at one displacement, and EU / DE's maps the same from `0x626614`. At that place EU holds 5 and DE 4 of the 8 bytes equal to the PC's (US 7, the eighth placed). The PC's 8 bytes occur only at the head of section 13 of seven unrelated area overlays (AREA020, 029, 063, 064, 072, 073, 110) with no reader into them. EU / DE gave area 125 other weights | a regional value: the European discs' own (rule 6), an owner's call like `MasterWin_Requirements+0x44`; not a twin |
| `MagicBallOrb_Shades+0x10` `0x65C1B8..0x65C1BC` | all 7 | MAGIC172's section ends after pairs 0..7; pairs 8 and 9 (4 bytes, 2 of them non-zero on the PC) are on no disc in that place, and `MagicBallOrb_DrawDisc` indexes by +0xB unchecked | stage B (OF-E): which index the PSX code can reach |
| the three u16 fields of `EffectKind66_States` and of `EffectKind5B_States` | Western PSX | section 2: the Western discs hold other values there | a regional value (the West's re-laid numbers; the board / menu kind 0x5D-0x5E's): the Western disc's own or the PC's, an owner's call |
| `CommuMusic_CloseSteps` FR / DE labels | FR, DE | section 2: FR / DE's own words | group LA's language layer |
| `CommuMusic_CloseSteps+0x68` (glyphs) | JP, US, EU, PSP | no file holds them (the US map's segment ends at `0x653144`; the disc's bytes after it are code) | P3, stage B |

**For stage B.** `EffectKind86_States` (census P2 item 13, OF-E's first row)
is placed whole on all seven builds by section 1's operand row, so OF-E has
nothing left of it. `Area146_EffectStates`' game-value row and
`Area169_Rects`' game-value row (OF-A's list) are placed too, as RD's attach
vectors. No ledger entry is drafted: every byte placed is BOF3.exe's; the
regional rows of the table above are the owner's to call.

## 4. Measurements

**The regeneration** (`regen.sh` in the harness root, the round's order:
`psp_exe.py recipe`, `exe_tables.py recipe`, `exe_own.py measure`,
`exe_tables.py gaps`, `exe_tables.py recipe`, over the seven discs; not
committed, the coordinator regenerates). `recipes/exe-psp.tsv` gains 146
`twin` rows, every one "0 not BOF3.exe's": PSX-JP 19, PSX-US 25, PSX-EU 23,
PSX-FR 27, PSX-DE 26, PSP-JP 13, PSP-EU 13. `psp_exe.py check` OK,
`exe_tables.py check` 0 errors. Side effects in the other recipes:

- `exe-layout.tsv` loses two `data` words, `0x635358` and `0x63535C`, and
  `exe-pointers.tsv` the run there: they are pointer-shaped bytes of area
  148's vectors 4 and 5 (RD 4.2), now placed equal on every build, so the
  pointer decision drops them. The image's bytes there are the same either way.
- `exe-places.tsv` gains `0x6543E8..0x6543F0` from `WORLD04/AREA170.EMI#13`
  on the five PSX builds, found by `exe_tables.py recipe` beside the newly
  filled `EffectKind33_SignX` (eight bytes of `EffectKind33_States`); a place
  row is kept only when its bytes are the PC's.

`recipes/exe-gaps.tsv`'s `gap` rows, ranges / bytes / the PC's non-zero
bytes (`gapcount.py`):

| Build | `06a209bd` | this branch |
|---|---|---|
| PSX-JP | 104 / 1,486 / 797 | 85 / 899 / 422 |
| PSX-US | 122 / 2,089 / 1,320 | 107 / 1,452 / 909 |
| PSX-EU | 118 / 2,120 / 1,348 | 105 / 1,533 / 971 |
| PSX-FR | 122 / 2,269 / 1,460 | 109 / 1,610 / 1,030 |
| PSX-DE | 125 / 2,297 / 1,474 | 113 / 1,658 / 1,057 |
| PSP-JP | 92 / 1,069 / 628 | 79 / 790 / 417 |
| PSP-EU | 93 / 1,079 / 634 | 80 / 800 / 423 |

What is left of this group's rows in the regenerated file: the three u16
fields each of `EffectKind66_States` and `EffectKind5B_States` (Western),
`MagicBallOrb_Shades+0x10` (all seven, 2 non-zero bytes), `Area124_Weights+0x50`
(EU, DE), `CommuMusic_CloseSteps`' FR / DE labels and the glyphs `+0x68`
(section 3). Every other row of section 1 is gone from it on every build.

**`tools/exe_scan.py`** over the seven discs (`C:\Users\kerok\g_tw\scan`):
`missing/gap` 422 (JP), 909 (US), 971 (EU), 1,030 (FR), 1,057 (DE), 417
(PSP-JP), 423 (PSP-EU) - the table's non-zero column, byte for byte. Of this
group's symbols only the rows named above have a `missing` or `differ` count;
the placed ones are all `equal`.

**The exe host, headless**, from a PSX-US cache and a PSP-EU cache built on
this branch (`importer.py build --source <disc> --lang en-US / en-150`,
`install --base`, an ini-less launcher copy, `BOF3X_SELFTEST_ONLY=1
BOF3X_EXEIMAGE=<cache> BOF3X_CACHE=<cache>`):

| Install | Exit | `inject:` | `BOF3X_EXEIMAGE gap` |
|---|---|---|---|
| PSX-US only | 0, "self-test only: done" | 10088 ours | 107 ranges, 1,452 bytes (the regenerated file's PSX-US rows) |
| PSP-EU only | 0, "self-test only: done" | 10088 ours | 80 ranges, 800 bytes |

No `src/` change, so the build's other self-tests stand. Not done: no live
run, no capture (world map 115's legend frame and area 170's dial panel are
the two rows that draw; the end batch's item 4).

## 5. PB: `BOOT.BIN` twins

Group PB, folded in at the merge (2026-10-18, from `install-gap/pb`). Measured headless in PB's worktree; nothing played. Its seven `BOOT.BIN` rows are in `TWINS` since the TW merge; its section 2 below says "stage B", which the coordinator brought forward.

### 5.1 What was built: `BOOT.BIN`'s reader (`tools/psp_exe.py`)

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

### 5.2 The rows (for `TWINS`, stage B)

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

### 5.3 `Sprite_ScreenMargins`: not a twin, the PSP's 16:9 framing undone

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

### 5.4 Checks

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

### 5.5 The self-tests

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

### 5.6 For the ledger (drafts; the coordinator numbers)

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

### 5.7 Not PB's, not done

`Char_WhelpSlot+0x4` (one byte, no safe twin) and `ConfigMenu_Steps+0xC`
(RD's reader first) are untouched. The PSP's other one-overlay-and-`BOOT.BIN`
runs of [`exe-image-twins-1013.md`](exe-image-twins-1013.md) 6
(`Area104_PanelTris+0x30`, `BossGaist_Anims+0x2`, ...) are TW's / OF's
rows by the round's split; with `boot_place` in, any of them can be a
`BOOT.BIN` twin on the PSP builds by one `TWINS` line.


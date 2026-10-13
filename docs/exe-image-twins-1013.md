# The disc-built image's one-file runs, and PSP-EU's own layout (2026-10-13)

**Status:** MEASURED (2026-10-13, the owner away; agent `twins`, branch
`away/1013-twins` off `d0d9162a`). Headless only: the recipes regenerated
over the seven held discs, the exe host's self-test from a PSX-EU-only and a
PSP-EU-only install built for this in the session scratch (`c_psxeu` /
`g_psxeu`, `c_pspeu` / `g_pspeu`, `importer.py build --lang en-150` and
`install --base`). Nothing played, nothing rendered, not seen by the owner.
Every byte filled is `BOF3.exe`'s; no image lost a byte that was. The scratch
scripts (`unplaced.py`, `mipsref.py`, `rowbuilds.py`, `gapcount.py`,
`imgcmp.py`, `exediff.py`, `regen.sh`, `selftests.sh`) and their outputs stay
in the session scratchpad `twins/` - derived from the player's files, never in
the tree (rule 1).

HANDOFF item 2's two leftovers of the disc-built image
([`exe-import-engine.md`](exe-import-engine.md),
[`visual-findings-1012.md`](visual-findings-1012.md) 9 and 10,
[`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md), DIV-0101):

1. the twelve disc-unplaced runs one overlay alone carries (9's last item),
   drafted as `TWINS` rows on 2026-10-12 and never committed;
2. `psxmap` reading PSP files at PSX-JP's addresses (10.2) and what is left
   on PSP-EU's image (10.4).

Three commits, each with its own regeneration so each can be taken or
dropped alone:

| Commit | What | Recipe rows added |
|---|---|---|
| `f99a1224` | section 2: thirteen `twin` rows, the twelve and `ItemUse_HandlerIndex+0xD0` | 76 (all "0 not BOF3.exe's") |
| `767a3590` | section 3: `pspmap`, `psxmap` at the PSP disc's own place | 4 (PSP-EU) |
| `e84ef241` | section 4: 26 `twin` rows for the game-value runs of the same shape | 141 (all "0 not BOF3.exe's") |

## 1. How the candidates were found

`unplaced.py BUILD` (scratch; the 2026-10-12 tool of the same name, widened):
for each `gap` row of `recipes/exe-gaps.tsv` of class `disc-unplaced` or
`game-value`, at least six bytes, how many of the build's files (`psp_exe.
sources_of`: the boot EXE or `BOOT.BIN` and every EMI section) hold the PC's
whole run, and how often. Run at `d0d9162a` on all seven builds
(`up_before_<build>.txt`); the rows held by **one file** grouped by symbol
(`cands.py`, `cands_before.txt`): 13 of class `disc-unplaced`, 29 of class
`game-value`.

The reader. For each candidate, `mipsref.py` (scratch) lists every copy of
the run in its file and, per copy, the `lui` + load / store / `addiu` pairs of
the same register (within 24 instructions) in that file's sections whose
address falls in the table that holds the copy (from the symbol's start, the
copy minus the run's offset into it, to the copy's end) - the overlay's own
code reading the copy (`refs_before.txt`, PSX-US and PSX-EU; PSP-EU for the
`BOOT.BIN` rows). The builds. `rowbuilds.py` (scratch, `rb_du.txt`,
`rb_gv.txt`): the builds whose gap (at `d0d9162a`) the range overlaps and
where `twin_place` finds the run; a row's fifth field is that set (`None`
when all seven), so no row is written where the build places the bytes
already or holds other bytes.

## 2. The thirteen disc-unplaced rows (`f99a1224`)

The twelve of `visual-findings-1012.md` 9 regenerated at today's tip - all
twelve are still gaps and still one file's - and `ItemUse_HandlerIndex+0xD0`
from 10.4's list (`BOOT.BIN` holds the 64 bytes once on both PSP discs).

| Range | Symbol | File | Copies | Readers (lui/imm pairs into the table, US) | Builds | Disc copy, PSX-EU / PSP-EU |
|---|---|---|---|---|---|---|
| `0x5F6307..0x5F6314` | `Area42_Placements+0x27` | `WORLD01/AREA042.EMI#13` | 1 | 2 | all seven | `0x801F2C03` / same |
| `0x609470..0x609487` | `Area72_Weights+0x50` (area 73's) | `WORLD01/AREA073.EMI#13` | 1 | 0 (see below) | PSX | `0x801F2D4C` / - |
| `0x616DDC..0x616DE4` | `Area94_EffectArgsD` | `WORLD02/AREA094.EMI#13` | 3 | 3 / 2 / **1** | all seven, the third copy | `0x801F5364` / same |
| `0x64D158..0x64D16A` | `BossGolem_Hooks+0xC` | `BOSS/BOSS022.EMI#13` | 1 | 2 | all seven | `0x800C1B18` / same |
| `0x653F88..0x653F98` | `EffectKind36_Frames` | `SCENARIO/SCENA03.EMI#0` | 1 | 4 | all seven | `0x801FCF40` / same |
| `0x65436C..0x6543AC` | `EffectKind2C_States+0xC` | `SCENARIO/SCENA04.EMI#0` | 1 | 3 | all seven | `0x801FA1A4` / same |
| `0x6544EC..0x65450C` | `EffectKind44_SparkColours` | `WORLD02/AREA080.EMI#13` | 1 | 2 | all seven | `0x801F3DC4` / same |
| `0x6545EC..0x6545FC` | `EffectKind49_V3States+0xC` | `SCENARIO/SCENA07.EMI#0` | 2 | 5 / 2 | all seven, the first copy | `0x801FDD7C` / `0x801FDD60` |
| `0x65460C..0x65461C` | `EffectKind49_V4States+0x10` | `SCENARIO/SCENA07.EMI#0` | 2 | 5 / 3 | all seven, the second copy | `0x801FDD9C` / `0x801FDD80` |
| `0x654F28..0x654F38` | `EffectKind8A_States+0x24` | `WORLD01/AREA068.EMI#13` | 1 | 2 | PSX | `0x801F3FE4` / - |
| `0x658F08..0x658F48` | `ItemUse_HandlerIndex+0xD0` | `BOOT.BIN` | 1 | 21 (PSP-EU) | PSP | - / `0x2DEDB8` |
| `0x65DE5F..0x65DE78` | `EffectKind18_08_States+0x8B` | `WORLD02/AREA109.EMI#13` | 1 | 4 | PSX-EU only | `0x801F2E8F` / - |
| `0x65EC54..0x65EC64` | `EffectKind18Sub3B_States+0x14` | `WORLD04/AREA169.EMI#13` | 1 | 6 | all seven | `0x801F3AA4` / same |

The choices where a file holds a run more than once (`TWINS`' sixth field):

- **`Area94_EffectArgsD`**: AREA094 holds the eight bytes three times. The
  PSX-US map's segment just before it (`exe_maps/psx-us.tsv`: `0x616DB4..
  0x616DDC` from `AREA094.EMI#13` at `0x801F533C`) continues to
  `0x801F5364`, the third copy; its one reader is at `0x801F326C`, inside the
  area's handler `+0x3C[8]` (`0x801F3230`), which `symbols.toml` has as the
  `Area94_EffectArgsD` reader ("kind 3, record 0, Area94_EffectArgsD"). The
  other two copies are read at `0x801F2F0C..0x801F300C` (handlers 0..2,
  `_ArgsA`) and `0x801F3154`, `0x801F31D4` (handlers 5, 6, `_ArgsC`).
- **`EffectKind49_V3/V4`**: the two 16-byte runs are equal, and SCENA07 holds
  them twice, 0x20 apart - as the PC's two runs are (`0x6545EC`, `0x65460C`).
  V3 takes the first copy, V4 the second; the readers split the same way
  (`0x801F73E8..0x801F7530` the first table, `0x801F7618..0x801F7890` the
  second). On the PSP discs the copies are at `0x801FDD60` / `0x801FDD80`,
  same order and readers.

`Area72_Weights+0x50`: 23 bytes held once, by area 73's overlay, with no
`lui`/imm pair into the table - the reader indexes it from a base the scan
does not follow. The 23 bytes are BOF3.exe's run verbatim and occur in no
other file; kept on that.

Regenerated (`regen.sh r1`: `exe_tables.py recipe`, `psp_exe.py recipe`,
`exe_tables.py recipe`, `exe_own.py measure`, `exe_tables.py gaps`, the
seven discs; `regen_r1.log`): 76 new `twin` rows, every one "0 not
BOF3.exe's". Side effects: six `exe-layout.tsv` `number` words inside the
filled ranges (`0x653F8C..0x653F94`, `0x6544F0`, `0x654504`, `0x654508`) are
filled by the rows now and leave the layout; one `exe-places.tsv` row
(`0x64D16A..0x64D1C0`, `BOSS034.EMI#16`) recounts its agree / key columns
(62 / 18 to 58 / 17) beside the new Golem row.

## 3. `pspmap`: psxmap at the PSP disc's own place (`767a3590`)

**The class 10.2 named.** `psxmap` fills a PSP image's unfilled runs from the
PSP disc's same file at PSX-JP's address. PSP-JP's files share PSX-JP's
layout; PSP-EU's do not everywhere (`GAME.EMI` 0x1000 earlier from some
point, 10.2). Measured: `probe_eu.py` / `probe_all.py` (scratch) take every
PSP gap row inside a segment of any of the five PSX maps (with
`exe-places.tsv`), and look for the run in the PSP disc's same file beside
that PSX disc's own 16 bytes before or after it.

| Build | Gap rows inside a PSX segment | At PSX-JP's place already | Elsewhere, one place by context | Not in the file | No PSX segment |
|---|---|---|---|---|---|
| PSP-JP | 6 (all the boot EXE's: no PSP file) | - | 0 | - | 240 |
| PSP-EU | 11 | 0 | **4** | 1 | 240 |

The other four PSX maps place nothing more on PSP-EU than PSX-JP's does
(`pa_psp-eu.txt`). So a second map keyed on PSP-EU's own layout places four
runs, and PSP-JP needs none.

**Built.** `psxmap_rows` keeps reading at PSX-JP's place; where the bytes there
are not BOF3.exe's, `own_place` looks for the run in the PSP disc's file by
the PSX-JP disc's own 16 bytes either side of it (a side with fewer than
four distinct values is not used): exactly one place holding BOF3.exe's run
beside a context, or nothing. The row is rule `pspmap` (the place is the PSP
disc's own address; `build_fill` reads it as `psxmap`). The recipe now needs
the PSX-JP disc beside the PSP discs and refuses without it (it is always
run over all seven). A first attempt keyed on the PC's own bytes of the
segment found none: the four segments are short (one is the run itself) or
full of pointer words.

| Range | Symbol | File | PSX-JP place | PSP-EU place | Bytes (PC non-zero) |
|---|---|---|---|---|---|
| `0x65401C..0x654028` | `EffectKind34_V0Areas` | `ETC/GAME.EMI#0` | `0x801C9184` | `0x801C8184` (-0x1000) | 12 (9) |
| `0x662CD8..0x662CE8` | `Scenario_Hooks+0x58` | `ETC/GAME.EMI#0` | `0x801C94A4` | `0x801C84A4` (-0x1000) | 16 (2) |
| `0x662DE8..0x662DF0` | `Field_ModeTailKinds+0x100` | `ETC/GAME.EMI#0` | `0x801C95B4` | `0x801C85B4` (-0x1000) | 8 (2) |
| `0x66B1DA..0x66B1F0` | `MenuList_GeneWinSteps+0xE` | `BATTLE/BATTLE.EMI#15` | `0x800B3CC6` | `0x800B6CC6` (+0x3000) | 22 (13) |

(`regen_r2.log`: "psp-eu pspmap: 4 runs, 58 bytes (26 of them non-zero on
the PC)", "psp-jp pspmap: 0 runs".) Three of them are in 10.4's list as "no
safe twin" (short runs, many copies): the context settles which copy.

## 4. The game-value rows (`e84ef241`)

The same finding for the `game-value` class (the D263 class: a value code
reads). Of 29 one-file candidates, 26 rows; left out
`Area154_ChoicePairs+0x18C8` (AREA067 holds it three times, no reader of any
copy), `EffectKind18Sub38_States+0x10` (its eight bytes are the start of the
Sub35 run's copy in AREA166 and the only reader there is Sub35's) and
`ConfigMenu_Steps+0xC` (`BOOT.BIN` once, no reader found).

| Range | Symbol | File | Readers | Builds |
|---|---|---|---|---|
| `0x617EBC..0x617EC8` | `Area96_Handlers+0x194` (area 98's) | `WORLD02/AREA098.EMI#13` | 15 | all |
| `0x63FF40..0x63FF48` | `Area172_EffectStates+0x100C` (area 173's) | `WORLD04/AREA173.EMI#14` | 24 | all |
| `0x654308..0x654310` | `EffectKind28_States+0x10` | `WORLD01/AREA042.EMI#13` | 2 | all |
| `0x654344..0x65434C` | `EffectKind2A_States+0x10` | `BOOT.BIN` | 274 (weak: `lui 0x2E` pairs are common in the ELF) | PSP |
| `0x6544A8..0x6544B0` | `EffectKind6B_Frame` | `WORLD03/AREA120.EMI#13` | 8 | all |
| `0x654750..0x654758` | `EffectKind56_States+0x10` | `WORLD03/AREA135.EMI#14` | 17 | US, EU, FR, DE |
| `0x654AB0..0x654AB8` | `EffectKind6F_States+0x10` | `WORLD03/AREA139.EMI#13` | 3 | PSX |
| `0x655214..0x655220` | `EffectKindA0_States+0x30` | `SCENARIO/SCE15EF1.EMI#0` | 21 | all |
| `0x65A5EC..0x65A5F4` | `MindSword_BladeRadii` | `BMAGIC/MAGIC006.EMI#3` | 1 | all |
| `0x65ABF9..0x65AC00` | `WeretigerStreak_Offsets+0x1` | `BMAGIC/MAGIC064.EMI#3` | 2 | all |
| `0x65AC10..0x65AC20` | `WeretigerImage_UV` | `BMAGIC/MAGIC064.EMI#3` | 2 | all |
| `0x65B7F5..0x65B808` | `SimoonDust_Offsets+0x5` | `BMAGIC/MAGIC103.EMI#3` | 3 | US, EU, FR |
| `0x65BC7C..0x65BC84` | `VenomRing_ScreenY` | `BMAGIC/MAGIC130.EMI#3` | 1 | all |
| `0x65C309..0x65C318` | `TempestGust_Angles+0x1` | `BMAGIC/MAGIC223.EMI#3` | 2 | all |
| `0x65E010..0x65E018` | `EffectKind18Sub0E_States+0x18` | `WORLD03/AREA117.EMI#13` | 7 | all |
| `0x65E02C..0x65E034` | `EffectKind18Sub13_States+0x14` | `WORLD03/AREA117.EMI#13` | 3 | all |
| `0x65E8A4..0x65E8B0` | `EffectKind18Sub27_States+0x14` | `WORLD03/AREA139.EMI#13` | 5 | PSX |
| `0x65E940..0x65E948` | `EffectKind18Sub42_States+0x78` | `WORLD03/AREA140.EMI#13` | 19 | PSX |
| `0x65EB44..0x65EB50` | `EffectKind18Sub2E_States+0x14` | `WORLD04/AREA158.EMI#13` | 3 | EU |
| `0x65EB64..0x65EB6C` | `EffectKind18Sub2F_States+0x14` | `WORLD04/AREA160.EMI#13` | 3 | EU |
| `0x65EB80..0x65EB88` | `EffectKind18Sub30_States+0x14` | `WORLD04/AREA161.EMI#13` | 3 | US, FR |
| `0x65EBB8..0x65EBC0` | `EffectKind18Sub32_States+0x14` | `WORLD04/AREA163.EMI#13` | 3 | FR |
| `0x65EBEC..0x65EBF4` | `EffectKind18Sub34_States+0x14` | `WORLD04/AREA165.EMI#13` | 3 | all |
| `0x65EC1C..0x65EC28` | `EffectKind18Sub35_States+0x14` | `WORLD04/AREA166.EMI#13` | 3 | all |
| `0x65EC74..0x65EC80` | `EffectKind18Sub40_States+0x10` | `WORLD02/AREA105.EMI#13` | 8 | all |
| `0x660D5C..0x660D68` | `Scena00_ObjectHooks+0x4` | `SCENARIO/SCENA02.EMI#0` | 2 | all |

`regen_r3.log`: 141 rows, every one "0 not BOF3.exe's". Side effects: six
more `exe-layout.tsv` `number` words filled by the rows (`0x6544AC`,
`0x655214`, `0x65AC14`, `0x65AC1C`, `0x65BC7C`, `0x65BC80`), and ten new
`exe-places.tsv` rows (`0x660D8C..0x660DE4`, `ETC/SHOP.EMI#0`, the five PSX
builds) found by `exe_tables.py recipe` once the twin beside them is filled;
no image byte changes there (`imgcmp.py`).

## 5. Gaps before and after, per image

`recipes/exe-gaps.tsv`'s `gap` rows: ranges / bytes / bytes the PC has
non-zero (`gapcount.py`, against `d0d9162a`'s file):

| Build | `d0d9162a` | after `f99a1224` | after `767a3590` | after `e84ef241` |
|---|---|---|---|---|
| PSX-JP | 261 / 2,522 / 1,449 | 250 / 2,284 / 1,317 | same | 231 / 2,094 / 1,199 |
| PSX-US | 282 / 3,313 / 2,104 | 271 / 3,076 / 1,972 | same | 249 / 2,851 / 1,840 |
| PSX-EU | 282 / 3,301 / 2,087 | 270 / 3,039 / 1,942 | same | 247 / 2,802 / 1,802 |
| PSX-FR | 284 / 3,400 / 2,165 | 273 / 3,163 / 2,033 | same | 250 / 2,930 / 1,897 |
| PSX-DE | 284 / 3,394 / 2,165 | 273 / 3,157 / 2,033 | same | 253 / 2,959 / 1,911 |
| PSP-JP | 246 / 2,099 / 1,250 | 236 / 1,841 / 1,129 | same | 219 / 1,673 / 1,030 |
| PSP-EU | 251 / 2,167 / 1,282 | 241 / 1,909 / 1,161 | 237 / 1,851 / 1,135 | 220 / 1,683 / 1,036 |

The exe host, headless (`selftests.sh`: `BOF3X_SELFTEST_ONLY=1
BOF3X_CACHE=<cache> BOF3X_EXEIMAGE=<image> BOF3X_LANG=en-150`, a fresh
ini-less launcher copy per run, this branch's build; the "before" image is
the cache's own, built at `d0d9162a`, the others `exe_tables.py build` from
the disc at each commit):

| Install | Image | Exit | `inject:` | `BOF3X_EXEIMAGE gap` | `BOF3X_EXEIMAGE_STRICT=1` |
|---|---|---|---|---|---|
| PSX-EU only | `d0d9162a` | 0 | 10088 ours | 282 ranges, 3,301 bytes | exit 3 at `0x5EC5EC` `Area26_EffectKinds+0x4` |
| PSX-EU only | `f99a1224` | 0 | 10088 | 270, 3,039 | exit 3, same first gap |
| PSX-EU only | `767a3590` | 0 | 10088 | 270, 3,039 (image byte-identical to the one before) | exit 3, same |
| PSX-EU only | `e84ef241` | 0 | 10088 | 247, 2,802 | exit 3, same |
| PSP-EU only | `d0d9162a` | 0 | 10088 | 251, 2,167 | exit 3 at `0x5F6308` `Area42_Placements+0x28` |
| PSP-EU only | `f99a1224` | 0 | 10088 | 241, 1,909 | exit 3 at `0x604958` `Area65_DriftUV` (port-relaid) |
| PSP-EU only | `767a3590` | 0 | 10088 | 237, 1,851 | exit 3, same |
| PSP-EU only | `e84ef241` | 0 | 10088 | 220, 1,683 | exit 3, same |

Every run "self-test only: done". The images against `BOF3.exe`'s `.data`
(`imgcmp.py`): PSX-EU 613,447 bytes equal to 613,592 to 613,732 of 638,976,
PSP-EU 612,069 to 612,190 to 612,216 to 612,315; **0 bytes lost** at every
step, every changed byte now the PC's. The checks at the tip: `psp_exe.py
check` OK, `exe_tables.py check` 0 errors, `importer.py check` ("742
files, 3582 chunks, 0 error(s)", its other parts 0 errors), `ledger_check.py`
114 entries 0 errors. No `src/` change, so the build's
other self-tests stand.

## 6. What is left on PSP-EU's image (10.4 re-measured)

`exediff.py` (scratch): where the PSX-EU image equals `BOF3.exe`'s `.data`
and the PSP-EU image does not, runs merged across up to four bytes. At
`d0d9162a` 42 ranges / 2,363 bytes; at `e84ef241` **39 / 2,336**
(`ed_before.txt`, `ed_r3.txt`). Gone: `ItemUse_HandlerIndex+0xD0` (section
2), `EffectKind34_V0Areas`, `Scenario_Hooks+0x58`, `Field_ModeTailKinds+0x102`,
`MenuList_GeneWinSteps+0xE` (section 3). New in the list: the two EU-only
rows of section 4 (`EffectKind18Sub2E/2F_States+0x14`) - PSX-EU gained them,
the PSP-EU disc does not hold those bytes. The list now:

| What | Bytes | Why it stays |
|---|---|---|
| `Math_SinTable+0x2` | 1,996 | engine-own: the engine fills it at start (`ExeOwn_Fill`), whatever the image holds |
| `Char_ExpTable+0x13A8..` (19 runs) | 251 | DIV-0104: the PSP's rebalanced level table, the PSP's own data |
| `SimoonDust_Offsets` | 18 | `psp-image.md` 1: the PSP's own values |
| `EffectKind18Sub2E_States+0x14`, `Sub2F+0x14` | 14 | the PSP-EU disc does not carry the PC's bytes (`rowbuilds.py`: found on PSX-EU only) |
| `MoveScript_EffectArg+0x48` | 27 | not on the PSP disc whole (`disc-unplaced`); the PSX builds place it from the boot EXE |
| `EventScript_OpLengths+0x15`, `+0x1B` / `Sprite_ScreenMargins` | 10 | `game-value`; `BOOT.BIN` and `MAGIC095` both hold the 12 bytes (`up_after_psp-eu.txt`): no single reader's file, not followed |
| `ConfigMenu_Steps+0xC` | 5 | `BOOT.BIN` once, but no reader found by the scan - left for a reader |
| `Char_WhelpSlot+0x4` | 1 | `game-value`, a single byte: no safe twin |
| single bytes of five areas' choice / message tables (`Area04_Choices+0x38`, `Area05_MessagesB+0x64C`, `Area08_MessageFormat+0x9FC..`, `Area100_Choices+0x38`, `Area116_Choices+0x48`) and `Area07_Handlers+0x577`, `Area25_Handlers+0xA77`, `Title_Sprites+0x68`, `NameTable_Consumables+0x78A`, `Scena17_EndSteps+0x7DC` | 14 | placed by the map from the PSP's own file, whose byte differs (the PSP's data or text); `Title_Sprites` is DIV-0103's kept PSP title |

**A class this suggests, not followed.** On the PSP discs many remaining runs
are held by exactly one overlay **and** by `BOOT.BIN` (`up_after_psp-eu.txt`:
`EffectKind18Sub1C/1D/33/64_States`, four bytes each; `Area104_PanelTris+0x30`;
`BossGaist_Anims+0x2`; `CommuHiLo_DealSteps+0x14`, ...). The PSP's native
code reads `BOOT.BIN`'s copy, so a `twin` there wants `BOOT.BIN`'s reader
settled first (the ELF's relocations, as `msets` does) - and most of them are
under six bytes, where `unplaced.py` did not look.

## 7. For the ledger

DIV-0101 is amended in place (no new number): the thirteen and 26 runs as
more `twin` rows of the existing class, and `pspmap` as a new way of finding
a run's place on a PSP disc (the bytes are still the disc's, verbatim, each
`BOF3.exe`'s). No defect was observed for any of these runs (no route reaches
a known symptom); none is numbered here.

Not done: no live run (the brief is headless); the PSP builds' `BOOT.BIN`
second copies (section 6); `ConfigMenu_Steps+0xC` and `Area72_Weights+0x50`
want a reader found by a better scan than lui/imm pairs (the second is
filled anyway on its byte evidence).

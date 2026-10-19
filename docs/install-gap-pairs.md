# Pairing the effect, magic and boss readers (install-gap round, group FP)

**Status:** MEASURED (2026-10-18, install-gap round stage B, group FP, branch
`install-gap/fp` off `beba2d60`). Pairs derived on the psx-jp disc and fed to
`symbols.toml` at the `hypothesis` tier. The recipes were regenerated over the
seven held discs in the group's worktree but **not committed**: the coordinator
regenerates. Built; nothing played. Addresses, symbol names and counts only
(CLAUDE.md rule 1). The scratch scripts and their outputs stay in the harness
root `C:\Users\kerok\g_fp`.

**Second pass** (2026-10-19, branch `pairs/unpaired` off `19180e63`, section 8):
the unpaired rows' readers, seeded by hand from three engine tables (the boss
kinds, the area descriptors, the scenario hooks). 385 more `psx` pairs and 125
`psx_overlay` fields at the `hypothesis` tier; four more `immref` rows on
psx-jp and two on each other build (8.1), measured in scratch. Recipes **not**
committed (the coordinator's). Nothing built or played. Scratch in
`C:\Users\kerok\g_pairs`. Independently re-derived on 2026-10-19 (end of 8).

The problem ([`exe-immediate-places.md`](exe-immediate-places.md) 0 and 6). The
`immref` kind places a P3 row through the PSX twin of a function that reads it.
OT's code-pointer pairs reached the readers of only 11 of 66 P3 rows. They
reached none of the effect, magic and boss state functions, because no placed
word pairs those functions: their tables are themselves unplaced. This doc
pairs them another way (section 1), measures the pairs (section 2), feeds them
(section 3), and places what they reach (section 4).

## 0. What came out

1. **2,734 new `psx` pairs and 410 `psx_overlay` fields** were written to
   `symbols.toml` by `tools/exe_imm.py pair --write`. That makes 5,206
   functions with `psx`, up from 2,472 after OT, and 4,471 with `psx_overlay`.
   Seven methods produced them, every pair through OT's fingerprint check
   (section 1).
2. **Precision.** 662 derivations hit a `psx` that symbols.toml already held
   and agreed with it. 4 differed, and in at least 1 of those 4 the old entry
   is the wrong one (section 2). On the held-out run, with only the boot
   executable's evidence tier as seeds, the result was 268 agreements and 1
   difference. A rejected variant, table slots accepted by position alone, was
   right only about half the time (48 to 24), so nothing it produced is written.
3. **Reach.** Section 3 of [`install-gap-round.md`](install-gap-round.md) lists
   the stage-B rows. 63 rows remain in `exe-gaps.tsv`. Before this work, **10**
   of them had a reader with a psx-jp twin. Now **38** do. OF-E went from 6 of 43
   to 34 of 43, and OF-A stays at 4 of 20 (section 5).
4. **Placed** (the committed generator, regenerated): **17 new `immref`
   rows** on psx-jp, 16 on each Western PSX build and 10 on each PSP build. On
   top of OT's 7 that makes 24 / 23 / 11 rows. They include
   `ChlorineCloud_Offsets` on all seven builds, from the same function CW found
   by hand. Section 4.
5. **`EffectKind2A_States+0x10`** is paired (its reader's twin is in
   `WORLD01/AREA048.EMI#13`). All six of its non-zero bytes are on the disc,
   but the generator refuses the row. The PC holds two trailing zero bytes of
   alignment, and in the same place the PSX holds its next data. **A one-line
   change to `exe_tables.find_imm`** (PT's file, described exactly in section
   6) places it on all five PSX builds. The same change places five more rows.
   One of them is an owner-call row that must be excluded.
6. **`MasterWin_Requirements+0x44` is not a requirement list** (section 7).
   It is a six-character ASCII string that follows the 17-pointer table. No
   instruction operand or pointer word addresses it, and RW's watch saw it
   unread on the master route. JP's `SHOP.EMI` holds it and no Western
   `SHOP.EMI` holds it at all. There is no Western list to read here. It is an
   `unread` region, not P1. No ledger entry is drafted.

## 1. The methods (`tools/exe_imm.py pair`)

Seeds are every symbols.toml `psx` resolvable to a section:
- the one `psx_overlay` names;
- the boot executable below `0x80196800`;
- `GAME.EMI#0`;
- else **located**: the one section holding the address whose function there
  passes the gate. `GAME.EMI#1` and `BATTLE.EMI#3` are tried first. Several
  sections count only where their bodies are one body.

Every candidate pair then passes **the gate**, OT's `pair_agrees`: the call and
constant fingerprint, with the boot executable's evidence-tier pairs as the call
map. A table slot or call position may instead pass the **shape check**. That
check needs equal call lists, aligned position by position, with two or more
calls agreeing or new and none contradicting a known pair. Calls to code the
PSX family keeps resident resolve by context: an overlay's own file first, then
`BATTLE.EMI#15` / `#3` for battle families, then the boot executable, then
`GAME.EMI#0`, then `GAME.EMI#1` for field families.

| Method | What it pairs | New pairs |
|---|---|---:|
| `magic` | `Magic_Rows` (PC, 151 rows of file id and entry) against its copy in `BATTLE/BATTLE.EMI#15`, found by the PC's ids less 0x105 at the same 8-byte stride. Row N's entry is paired with row N's, in the file the id names. File ids are the disc's `BIN/` files in LBA order, which equals the sibling's `file_ids.json` 883 of 883 on psx-jp | 133 |
| `kinds` | `Effect_KindHandlers` (187 entries) against its psx copy (`GAME.EMI`, the `psx` symbols.toml holds), kind N's handler to kind N's. Most handlers sit at overlay addresses that up to 201 files share (`0x801F2C04` in every area overlay). For those, the file whose state table agrees best wins (`score_dispatch`: each slot already paired the same way, or passing the gate or the shape check; contradictions count double; a tie only between identical bodies) | 454 |
| `tables` | A paired dispatcher's state table against the table its twin's lui pair addresses, state N to state N. The PC table can be a `.data` jump table or a stack table of `mov [esp+4i], imm32` (the magic tasks). A slot in another overlay chooses its file as `kinds` does | 586 |
| `calls` | Two paired functions' call lists of equal length: the n-th call to the n-th call, each callee through the gate | 336 |
| `tablesearch` | An unpaired PC dispatcher's table, found among every run of PSX code words by the entries already paired: three or more and three quarters agree, none contradicts. Its twin is the function whose lui pair addresses that run. This is how `BattleMagicFx_Dispatch` (kind-1 battle tasks, 110 entries) found `BATTLE.EMI#3` | 114 |
| `order` | Between two pairs adjacent in PC order whose twins are in one section in the same order, with as many functions on both sides (12 at most): position by position, through the gate | 268 |
| `order-align` | Inside a magic, boss, scenario or area overlay, where the counts differ, or where the next pair is another file's (then the walk runs to the end of the section's code): the order-keeping alignment. A pair is kept only where it is the one gate pass of both its row and its column | 876 |

2,767 pairs derived; 2,734 written (33 functions already had a `psx` the
seeding could not place in a section: 28 equal the derivation, 5 do not, section
2.3). Commands:

    python tools/exe_imm.py pair --game bof3 --disc <psx-jp cue> --held-out      # the precision run
    python tools/exe_imm.py pair --game bof3 --disc <psx-jp cue> --out <tsv> --write

`--held-out` never writes. A run takes about two minutes. It needs capstone, as
OT's `feed` does.

## 2. Precision

### 2.1 Against the `psx` symbols.toml already held (the production run)

Here a derivation of a function that is already a seed is compared, not used.
"Evidence" means a `psx` with no `psx_status` (the earlier rounds' readings).
"Feed" means OT's code-pointer pairs.

| Method | Evidence agree | Evidence differ | Feed agree | Feed differ | Other |
|---|---:|---:|---:|---:|---|
| `tables` | 148 | 2 | 514 | 2 | 2 + 1 same address in another file, 2 a copy |
| `kinds` | 0 | 1 | 0 | 0 | |

The other methods only reach functions with no `psx`. The four differences are
listed in 2.3.

### 2.2 Held out (seeds: the boot executable's evidence tier only, 250 pairs)

Every other `psx` here is a reference the methods do not see.

| Method | Evidence agree | Evidence differ | Feed agree |
|---|---:|---:|---:|
| `calls` | 26 | 0 | 0 |
| `kinds` | 5 | 1 | 103 |
| `order` | 9 | 0 | 0 |
| `order-align` | 4 | 0 | 18 |
| `tables` | 7 | 0 | 95 (+1 a copy) |
| `tablesearch` | 1 | 0 | 0 |

Total: 268 agree, 1 differs, and the one difference is the entry's error
(2.3). Consistency between methods is a check that needs no reference: a
function derived twice gave the same twin, or an identical body in another
file, in every case on the production run but 3 `tables` re-derivations (of
1,716).

**Rejected:** accepting a table slot or an order position whose gate fails
only for want of a call to compare. In the production run it gave 48 evidence
agreements against 24 differences, and 31 feed differences. Most were a
function the PSX links a copy of into each magic overlay, which is no error.
But the table shifts were real. Nothing of that tier is in the written pairs.

### 2.3 The differences (left as they are; not written)

| PC | Entry's `psx` | Derived | By | Reading |
|---|---|---|---|---|
| `0x469E30` `Effect_CameraTurn` | `0x8019BC3C` | `0x8019B8BC` (`GAME.EMI#0`) | `kinds` (kind 0x13), and `tables` (`Effect_RunObjects`' table) | The entry is wrong. `0x8019BC3C` is kind 0x31's handler (PC `EffectKind31_Run` `0x46A020`). The two dispatchers are word for word alike but for their tables, and the entry's own state twins (`CameraTurn_Start` `0x8019B90C`, `_Step`, `_End`) are the slots of `0x8019B8BC`'s table. Suggest `psx = 0x8019B8BC` |
| `0x4975B0` `ItemUse_HealAp5` | `0x801E8AB8` | `0x801E8860` (`START.EMI#8`) | `tables` (state 24 of `0x497680`'s `0x658DB0`) | Not read; the other slots of that table agree |
| `0x44C2A0` `EffectSlot17_RaiseDown` | `0x8009B034` (feed) | `0x8009AF74` | `tables` (state 17 of `0x44B9F0`'s `0x64E73C`) | `0x44B9F0`'s pairs are a known trouble spot (HANDOFF: `pairs_propagated.json`'s `0x445A30` / `0x44B9F0`); not read |
| `0x44C7D0` `EffectSlot25_StatMod2` | `0x8009B79C` (feed) | `0x8009B1DC` | as above | as above |

The 5 entries with a `psx` the seeding could not place (2,767 against 2,734):

- `0x528CD0` `LeaderPanel_S1Choose` (`0x801DA068`, derived `0x801DA1E8`, `order`)
  and `0x5295F0` `LeaderPanel_S4Run` (`0x801DB03C`, derived `0x801DB1C0`,
  `tables`). Both derivations equal OT's `differs` pairs (OT 3). Two methods
  that share nothing agree against the entry, which OT already suspected was
  the one wrong.
- `0x402D80` `Area19_CameraIn`, `0x416DE0` `Area108_PlaceAtCellB` (derived in
  `AREA135`) and `0x42C200` `Area192_Init`, all by `order-align`. Not read.

Correcting any `psx` an earlier round wrote is the coordinator's or the owner's
call. The two `LeaderPanel` rows and `Effect_CameraTurn` are the ones the
evidence supports.

## 3. The feed

`tools/exe_imm.py pair --write` (`pair_write`) writes the following, after the
entry's `name` (or `psx`) line, keyed by `pc` as OT's `write_feed` does:

- **A new pair of a gate-accepted method, on a `[[func]]` with no `psx`:**
  `psx`, `psx_overlay` (not for the boot executable), `psx_status =
  "hypothesis"`, and `psx_evidence`. The evidence names the method and the
  table, slot or neighbours.
- **A seed `locate` found a file for,** or a `GAME.EMI` address whose function
  passes the gate: `psx_overlay` alone. `exe_tables.sym_twins` otherwise reads
  such a `psx` in the boot image, where the bytes are not the function's. 410
  of these were written. This is what gives `GameMode3_Run` / `_Leave` a
  readable twin.

The header's `psx_status` line names the new source. Checks:

- `tomllib` loads it: 10,100 functions, 5,206 `psx`, 4,471 `psx_overlay`,
  3,678 `hypothesis`.
- `gen_symbols.py` gives 10,088 ours.
- `cmake --build build` links.
- `python tools/exe_imm.py selftest` reports 0 problems.

## 4. What the pairs place (`exe-imm.tsv`, regenerated, not committed)

The full chain of [`install-gap-round.md`](install-gap-round.md) 5 ran in the
worktree (`C:\Users\kerok\g_fp\regen.sh`). `find_imm`'s counts after it:

| Build | rows (OT) | rows now | regions with a twinned reader |
|---|---:|---:|---:|
| psx-jp | 7 | 24 | 112 |
| psx-us, -eu-en, -fr | 7 | 23 | 110..111 |
| psx-de | 7 | 23 | 113 |
| psp-jp, psp-eu | 1 | 11 | 95 |

All rows are `immref`. The new ones, by row:

| Row | Range | Twin file | Builds |
|---|---|---|---|
| `EffectKind27_States+0xC` | `0x6542EC..0x6542F0` | `WORLD01/AREA042.EMI#13` | all 7 |
| `EffectKind29_States+0x18` | `0x654328..0x65432C` | `WORLD01/AREA048.EMI#13` | all 7 |
| `EffectKind65_ShakeSteps` | `0x6549E8..0x6549EC` | `WORLD02/AREA107.EMI#13` | all 7 |
| `EffectKind6D_States+0x48` | `0x654A9C..0x654AA0` | `WORLD03/AREA139.EMI#13` | PSX 5 |
| `EffectKind9A_Branches` | `0x655168..0x655170` | `WORLD03/AREA119.EMI#13` | PSX 5 |
| **`ChlorineCloud_Offsets`** | `0x65A608..0x65A62C` | `BMAGIC/MAGIC009.EMI#3`, function `0x801EF198` | all 7 |
| `SnapWave_FacingPhase` | `0x65A678..0x65A67C` | `BMAGIC/MAGIC013.EMI#3` | all 7 |
| `Magic020Slash_Animations` | `0x65A6DC..0x65A6E5` | `BMAGIC/MAGIC020.EMI#3` | all 7 |
| `Magic087_RisePhases+0x8` | `0x65B528..0x65B52C` | `BMAGIC/MAGIC087.EMI#3` | all 7 |
| `WhelpBreath_GlowAngles` | `0x65BB24..0x65BB28` | `BMAGIC/MAGIC120.EMI#3` | all 7 |
| `InkPuff_Offsets+0x11` | `0x65BE21..0x65BE34` | `BMAGIC/MAGIC145.EMI#3` | all 7 |
| `InkInkPuff_Offsets` | `0x65BE50..0x65BE74` | `BMAGIC/MAGIC146.EMI#3` | all 7 |
| `EffectKind18Sub1C_States+0x14` | `0x65E2DC..0x65E2E0` | `WORLD02/AREA102.EMI#13` | PSX 5 |
| `EffectKind18Sub1D_States+0x14` | `0x65E2F4..0x65E2F8` | `WORLD02/AREA102.EMI#13` | PSX 5 |
| `EffectKind18Sub33_States+0x14` | `0x65EBD4..0x65EBD8` | `WORLD04/AREA164.EMI#13` | PSX 5 |
| `EffectKind18Sub37_States+0x10` | `0x65EC04..0x65EC08` | `WORLD04/AREA165.EMI#13` | psx-jp (the twin's body is not found exactly once in the other builds' copy) |
| `EffectKind18Sub64_States+0x1C` | `0x65F578..0x65F57C` | `WORLD03/AREA120.EMI#13` | PSX 5 |

Each row rebuilds the PC's bytes exactly on the build it names. That is
`find_imm`'s rule, and `exe_tables.py check` decodes them all against BOF3.exe
(section 4.1). The PSP's four `EffectKind18Sub*` rows are PB's `BOOT.BIN`
twins, already placed. Where a stage-A twin fills a range, the twin keeps it:
`imm` rows only fill bytes nothing else did.

**`ChlorineCloud_Offsets`, CW's row.** The pairing reached `ChlorineCloud_Start`
(`0x49DAD0`) by `order-align` inside `MAGIC009` (between `Chlorine_End` and the
next magic unit). The twin is `0x801EF198`, the function CW found by hand from
the steps table. CW's note asked whether the matcher follows an indexed
`lui` / `addu` / `lw` chain. It does: `exe_imm.scan` keeps a lui-plus-index
register as an address base, and this row's `insns` (`6C+74`) is that chain. The
search missed the row only because the function had no twin. Once PT's `TWINS`
row from the same file merges, the twin fills the bytes first and this
`immref` row drops out of the recipe on its own.

### 4.1 Checks after the regeneration

- `exe_tables.py check --game bof3` with the seven `--disc`s decodes every row
  on its disc against BOF3.exe: 0 errors.
- `importer.py check`: 0 errors.
- The regeneration changes `exe-imm.tsv` (+101 rows), `exe.toml` (its hashes
  and counts) and `exe-gaps.tsv` (-101 rows, the placed ones). Every other
  recipe comes out byte-identical. The recipes were checked out again before
  the commit.
- The exe host's self-test from the owner's install (`BOF3X_SELFTEST_ONLY=1`,
  a copy of `bof3x.exe` and `bof3x.dll`): `self-test only: done`, exit 0.
- `gen_symbols.py`'s ours count is unchanged (10,088): `psx` fields generate
  nothing.

## 5. Reach, row by row (psx-jp twins)

Readers are `exe_tables.text_readers` (every .text operand from the row's data
symbol, 0x400 below at most), as `find_imm` takes them. "Twinned" means a reader
has a `psx`.

| | rows | with a reader | twinned before | twinned now |
|---|---:|---:|---:|---:|
| OF-A (areas, scenario) | 20 | 15 | 4 | 4 |
| OF-E (effects, magic, bosses) | 43 | 42 | 6 | 34 |
| all | 63 | 57 | 10 | 38 |

**Twinned but not placed, and why** (measured with the twins' lui pairs; counts
only):

- `EffectKind2A_States+0x10`, `Area118_SpawnKinds`, `BreathMote_Angles`
  (Western), `EffectKind18Sub2F_States+0x14` and `Sub32_States+0x14` all fail
  for one reason: every non-zero byte is the disc's at the twin's address, but
  the PC's trailing zeros are the disc's next data. Section 6.
- `EffectKind75_States+0xC`: 3 of 3 bytes equal through the twins of
  `EffectKind7B_Pulse` / `7C_Pulse` (`AREA188`), under `find_imm`'s four-byte
  floor.
- `BoneDanceShake_Counts`: all 3 non-zero bytes equal (`MAGIC057`). The 4th
  byte is zero on the PC and the disc's next byte. Three compared bytes are
  under the floor even with the section 6 change.
- `Area141_CellEntries+0x7C`: all 15 non-zero bytes equal (`AREA141`, through
  `Area141_PlaceOne0x`'s twin). But the PC holds four zero bytes before the
  record that the PSX does not, so the run is offset. A rule that starts the row
  at the reader's operand would place it, but it makes `ChlorineCloud_Offsets`
  and `InkInkPuff_Offsets` ambiguous, so it is not proposed.
- `EffectKind18Sub31_States+0x14`: 4 of 8 bytes equal (4 of 6 non-zero).
  `Sub2E`: 1 to 2 of 10 non-zero. `Sub30`: 1 of 6. `AuraBreathDome_Height`:
  3 of 8. The twin's addressed bytes differ. Whether these are the same rows
  with other values, or another copy of the sub-program (several area overlays
  hold each), is not settled. So they are not written as findings.
- `CommuMusic_CloseSteps` (+0x28, +0x40, +0x66, +0x68): the twins' lui pairs
  address nothing that fits.
- `TornadoMote_Phases`: the one twinned reader (`TornadoMote_Task`) addresses
  nothing that fits. The other two readers (`Identify_Draw*`) are unpaired.
- `GameMode3_Steps+0xC`: `GameMode3_Run` / `_Leave` now carry
  `psx_overlay = "GAME.EMI#0"`. The regeneration places nothing: the two bytes
  are under the floor.

**Unpaired readers, and why:**

- **Bosses** (`BossDolphin_Sounds`, `BossGaist_Anims+0x2`, `BossTorch_Sounds`).
  Their `*_Enter` functions are state 0 of the boss kinds' tables, reached
  through `BattleEnemy_RunAll`'s `call [0x64B084 + kind * 4]`. None of that
  table's 64 entries has a twin, so `tablesearch` has no anchor. One boss
  kind's table paired by hand (a reading of `BattleEnemy_RunAll`'s twin in
  `BATTLE.EMI`) would let `tables` and `order-align` run.
- **`ReviveHalo_MoteCounts`, `WardMote_Tilts`**: the magic units' entries are
  paired, but the gate fails for these readers or their neighbours.
- **`EffectKind18Sub21`, `Sub38`, `Sub45`**: kind 0x18's slots for these
  sub-programs choose no file (no candidate scores 2).
- **OF-A areas and scenario** (`Area91`, `96`, `115`, `121`, `135`, `154`,
  `172`, `196`, `Scena02..04`, `LinkedObjectD_Amounts`): no method here seeds
  area or scenario code. The cheap next method is the area descriptor table
  (`psx_pair.py areas`' pairing of `0x667590` with the boot executable's
  `0x801802EC`, each descriptor's `+0x3C` handlers and `+0x40` init). It feeds
  `order-align` in every area overlay; not built here.
- `Area96_Handlers+0x184`, `Area154_ChoicePairs` (two rows),
  `Area172_EffectStates+0xFF0`, `Area196_Keys1+0x658` and
  `AirRaidImage_ShadeSteps` have no reader at all by `text_readers`.

## 6. For PT: the one change to `exe_tables.find_imm`

In `find_imm`'s `immref` loop, the byte comparison currently reads:

    if blob2[o] != pc[x - DATA_LO]:
        ok = False
        break

The proposed version:

    if blob2[o] != pc[x - DATA_LO]:
        if x >= lo and not any(pc[x - DATA_LO:hi - DATA_LO]):
            end = x          # the PC's tail is zero (alignment); the disc's next data begins
            break
        ok = False
        break

A region's tail of bytes that the PC holds zero then ends the row where the
disc's bytes first differ. The row stops short and the PC's zeros stay unfilled,
which in the image is the PC's own value. This is TW's "a range ends where the
disc's own bytes begin" ([`install-gap-twins.md`](install-gap-twins.md) 1),
applied to `immref`. The four-byte and two-non-zero floors are unchanged.

Measured with the change applied in scratch (`trimpatch.py`), over the same
pairs, it adds these rows:

| Row | Range placed | Builds |
|---|---|---|
| **`EffectKind2A_States+0x10`** | `0x654344..0x65434A` (the six non-zero bytes), `WORLD01/AREA048.EMI#13` | PSX 5 |
| `Area118_SpawnKinds` | `0x621610..0x62161C` | PSX 5 |
| `BreathMote_Angles` | `0x65BB78..0x65BB7F` | US, EU, DE |
| `EffectKind18Sub2F_States+0x14` | `0x65EB64..0x65EB6A` | JP, US, FR, DE, both PSPs |
| `EffectKind18Sub32_States+0x14` | `0x65EBB8..0x65EBBE` | JP, US, EU, DE, both PSPs |
| `Area104_PlateAnims` | `0x61B4F0..0x61B4F4` | all 7 - **exclude**: an OWNER CALL row (DIV-0055's plates, `port-relaid`, census 4). It needs an `exe_own.py` region or a kind filter in `imm_regions` before the change lands |

On psx-jp this takes the row count from 24 to 29 and the bytes from 92 to 114.

## 7. `MasterWin_Requirements+0x44` (P1 item 8): a finding, no list

- `MasterWin_Requirements` (`0x66B424`) is 17 pointers, `0x44` bytes. The
  census row `0x66B468..0x66B46E` is the six bytes after the table. symbols.toml's
  own evidence calls them ASCII. They are a six-letter word followed by two zero
  bytes. The census credited `MasterWin_Available` as the reader, but that was
  `text_readers`' 0x400 reach: the function's operand is the table's start.
- **No reader:** no .text operand lies in `0x66B428..0x66B46F` but the table
  base. No 4-byte word anywhere in .data or .text equals `0x0066B468`. RW's
  gap watch saw it "not read, not written" on `masterAndManillo` on all four
  Western builds ([`install-gap-reads.md`](install-gap-reads.md)).
- **The discs:** `MasterWin_Available`'s twin is `ETC/SHOP.EMI#0`
  `0x801E4AEC` on psx-jp, and its body is found once in each Western
  `SHOP.EMI`. On psx-jp and both PSPs, the 6 bytes after its table equal the
  PC's. On the four Western builds, all 74 bytes at the twin's table differ,
  because the table's pointers move with the Western layout. The six-byte word
  is in no Western `SHOP.EMI` section at all. The master requirement lists
  themselves (`0x66B3D0..`) are not in `exe-gaps.tsv` on any build.
- **So:** there is no Western requirement list at this row to read. The row is
  an `unread` string, a JP-side leftover the Western discs dropped. The census's
  "a master's availability test reads a zero list" does not hold. Suggested
  status: `unread`, by an `exe_own.py` `REGIONS` row (PT's file). No ledger
  draft: nothing an install gets differs.

## 8. The unpaired rows, second pass (2026-10-19)

Section 5 left two groups with no twinned reader: the three boss rows, and the
OF-A area and scenario rows. This pass seeds `exe_imm.py pair` from three engine
tables, by a scratch script (`C:\Users\kerok\g_pairs\run.py`). The script
builds the `Pairing`, adds the seeds, runs the seven methods unchanged, and
writes through `pair_write`. `tools/` is unchanged. All counts are psx-jp's.

### 8.1 Results

| | before | after |
|---|---:|---:|
| `symbols.toml` functions with `psx` | 5,206 | 5,591 |
| with `psx_overlay` | 4,471 | 4,980 |
| `psx_status = "hypothesis"` | 3,678 | 4,063 |
| psx-jp `exe-gaps.tsv` rows with a twinned reader (of 251) | 87 | 93 |
| `find_imm` regions with a twinned reader (of 265) | 107 | 113 |
| `find_imm` `immref` rows placed / bytes | 24 / 103 | 28 / 127 |

The placement counts come from a scratch run of `exe_tables.recipe_imm` over
psx-jp alone (`g_pairs\place.py`), with the committed places and pointers.

The four new rows:

| Row | Range | Twin |
|---|---|---|
| `BossDolphin_Sounds` | `0x64D484..0x64D48C` | `BOSS/BOSS029.EMI#16` `0x800C1848` (`BossDolphin_Enter`) |
| `BossGaist_Anims+0x2` | `0x64D78E..0x64D798` | `BOSS/BOSS035.EMI#16` `0x800C1860` (`BossGaist_Enter`) |
| `BossTorch_Sounds` | `0x64D7A0..0x64D7A8` | `BOSS/BOSS035.EMI#16` `0x800C1994` (`BossTorch_Enter`) |
| `Scena04_Quake` + `Scena04_TremorSteps` | `0x660FFC..0x661004` | `SCENARIO/SCENA04.EMI#0` `0x801F8680` (`Scena04_Scene1`) |

That settles 5 of the 17 listed rows on psx-jp. One more, `Area135_EffectKinds`,
gains a twinned reader but does not place (8.5).

The other six builds reach only two of the four rows through `build_twins`:
`BossDolphin_Sounds` and the `Scena04` row. `BossGaist_Anims+0x2` and
`BossTorch_Sounds` place on **psx-jp only**. On each other build the masked
body of the Enter twin matches three functions in `BOSS035.EMI#16`
(`0x800C1860`, `0x800C1994`, `0x800C1A98`), and `build_twins` needs exactly
one match. The bytes at the psx-jp addresses are equal on all six builds
(10/10, 8/8): the twin is missing, not the data. `immref` rows per build, from
the verification's regeneration in scratch:

| Build | before | after |
|---|---:|---:|
| psx-jp | 24 | 28 |
| psx-us | 24 | 26 |
| psx-eu-en | 23 | 25 |
| psx-fr | 22 | 24 |
| psx-de | 24 | 26 |
| psp-jp, psp-eu (each) | 12 | 14 |

A follow-up, **not applied**: in `exe_tables.build_twins`, when `find_body`
returns several hits and one of them is the psx-jp address in the same file
and section, take that hit. Measured by hand, that would place the two rows on
all six builds. It needs its own regeneration and check before landing.

### 8.2 The boss kinds, by hand

- **The table.** `BattleEnemy_States` (`0x64B084`, 64 entries) has its twin at
  `BATTLE/BATTLE.EMI#3` `0x801EB208`. Entry 0 there is `EnemyOp_StepDispatch`'s
  `psx` (`0x801E3140`), and entry 1 is null on both sides. The three
  entries symbols.toml already held (kinds 23, 24 and 27, `BOSS018.EMI#16`)
  equal the table's words. One function addresses the table:
  `BATTLE.EMI#3` `0x801E20F4`, `BattleEnemy_RunAll`'s twin. Its gate fails (no
  call to compare), so it is not written. The same 64 words are in
  `BATTLE2.EMI#3` and in 40 `BOSS*.EMI` files.
- **The file.** A boss kind's entry is an address at `0x800C1xxx` that 18 to 40
  boss files hold, so the address alone does not name a file. The file comes from
  round eleven's boss-id evidence (`Boss29_Setup`, `Boss35_Setup`, by
  `tools/boss_rows.py --disc`). That evidence is independent of these rows'
  bytes. The bytes then agree with it:
  - **kind 35, Dolphin, `BOSS029`.** `0x800C1804`. Of the 40 candidate files,
    BOSS029 is the only one whose state 0 addresses bytes equal to both
    `BossDolphin_Anims` (not a gap row) and `_Sounds`.
  - **kind 42, Gaist, `BOSS035`.** `0x800C181C`. Only BOSS035 and BOSS047 (one
    image, by section md5) hold both `BossGaist_Anims` and `_Sounds`.
  - **kind 43, Torch, `BOSS035`.** `0x800C1950`. Only BOSS035, BOSS047 and
    BOSS038 hold a dispatcher there with a table, and of those only BOSS035 and
    BOSS047 hold `BossTorch_Sounds`.
- **State 0.** Each `*_Enter` is paired as state 0 of the twin dispatcher's
  table. The gate refuses it: it makes no call, and its constants are object
  offsets. The PSX `*_Enter`'s lui pairs are what `find_imm` reads.
- **`tables` and `order-align` then.** `tables` reads all three twin tables:
  `0x800C19BC` in BOSS029, and `0x800C1D2C` / `0x800C1D68` in BOSS035. It adds
  nothing for these kinds. State 0 fails the gate. State 1 is
  `Port_DroppedCall` on the PC. States 2..11 are the resident `EnemyOp_*`
  functions in `BATTLE.EMI#3`, already held or refused. `order-align` does run
  inside BOSS035. It pairs `BossGaist_Hook` (`0x43E5C0` with `0x800C18C4`,
  which is also the hook word the twin `BossGaist_Enter` stores) and
  `BossGaist_HookClearBit1` (`0x43E5D0` with `0x800C190C`). Dolphin has no
  paired neighbour after it in BOSS029, so `order-align` adds nothing there.
- **The other kinds** were not fed. `choose()` alone, the `kinds` method's file
  choice, picks a file for 22 of the 61 non-null kinds, and the three that
  were already held agree. It sends kinds 37, 54 and 56 to one address in BOSS014 among
  ties, so a `bosskinds` method needs the boss-id evidence as its file
  choice, not `choose()`.

### 8.3 The area descriptors and the scenario tables

- **Areas** (`Area_Descriptors` `0x667590` against the boot table
  `0x801802EC`). Area *k*'s descriptor is in `AREA`*kkk*`.EMI`, in the
  section that holds it (`#13`, or `#14` in some files). All 200 areas agree on
  handler count and on whether there is an init. That gives 741 slots, and 4
  slots fall outside every section.
- **Precision of the slots against symbols.toml** (before the filter below):
  - 390 agree;
  - 39 are a copy (the same body in another area's file: the PC keeps one
    function where the PSX has one per area);
  - 15 are the same address in another file;
  - 7 differ.

  The filter has two rules:
  - an area is dropped whole where one of its slots contradicts a held pair:
    areas 34, 39, 41, 60, 75, 99 and 108, 59 slots;
  - a PC function is dropped where its slots name different bodies: 5 slots.

  After the filter, 3 differences remain. Each is two identical bodies in the
  one file (AREA098 handler 5, AREA134 handler 13, AREA148 handler 5).
- **Scenario** (`Scenario_Hooks` / `_CallATables` / `_CallBTables`
  against `GAME.EMI#0` `0x801C944C` / `0x801CDC4C` / `0x801CDC9C`). For each
  chapter, the table's slots go to that chapter's `SCENAnn.EMI#0`. That gives
  247 slots. 12 hit a held pair and all 12 agree. 66 are copies of a shared
  PC function.
- **Held-out run.** The seeds were the boot executable's evidence tier (250)
  plus these table seeds. The run found 3,911 new pairs, and 3,308 of them hit a
  held `psx`:
  - 3,262 agree;
  - 23 are a copy;
  - 19 are the same address in another file;
  - **4 differ (0.12%)**.

  The four are one area slot (AREA148, an identical body), two cross-file
  copies from `order-align`, and one `order` pair in `GAME.EMI`. This compares
  with section 2.2's 268 to 1.

  **Not fully independent at the seed level.** The run's seed filter drops
  area and chapter groups by consulting the held `psx` pairs, the same pairs it
  then scores against. The seed-level held-out figures therefore lean on the
  references that check them. The derived pairs' figures are unaffected in
  kind.
- **Precision on the bytes.** A pair agrees when an 8-byte `.data` operand of
  the PC function (two non-zero bytes, pointer words wild) appears at an address
  one of the twin's lui pairs forms. Results:
  - the 336 table seeds that were new to this run (6 boss, 167 area, 163
    scenario): 29 testable, 28 agree;
  - all the pairs the seeds account for: about 38 testable, 34 agree. The 4
    differences are false tests. In `Scena02_StepHook`, `Scena03_Scene3`,
    `Scena07_Scene3` and `Scena13_Run3` the only PC "operand" is a round
    constant (`0x5E0000`, `0x5F0000` or `0x600000`) that falls in `.data`'s
    range but is not a data reference. All four pass the gate strongly. Real
    tests give 34 of 34;
  - for reference, the 3,678 `hypothesis` pairs already held: 311 testable,
    261 agree (84%).

  Most functions have no testable operand.
- **Three of a plain rerun's pairs are refuted.** On today's symbols.toml,
  `exe_imm.py pair` with no seeds derives 99 new pairs, from merges since FP.
  Three of them conflict with a descriptor slot: `0x42C200` (AREA192 init),
  `0x402D80` (AREA019 handler 1) and `0x41D810` (AREA008 handler 6). No
  existing `symbols.toml` value changes:
  - `0x42C200` and `0x402D80` keep their held `psx` (`0x801F3424`,
    `0x801F2C2C`) and gain a `psx_overlay` (`AREA192.EMI#13`,
    `AREA019.EMI#13`). The plain run's `order` / `order-align` derivations, a
    neighbour of the address the engine's own pointer gives, are what the
    descriptor refutes; they are not written;
  - `0x41D810` is unchanged. It is shared by area 8 (handler 6) and area 134
    (handler 3), and its held `psx` is area 134's.

### 8.4 What was written

`pair_write` wrote only what the seeds account for: the 514 pairs new against a
run with no seeds, and the three above. It skipped the 99 other pairs of a
plain rerun and its 25 `located` overlays, which a plain `pair --write` gives.
Two fields went in:

- `psx` + `psx_overlay` + `psx_status = "hypothesis"` + `psx_evidence` on
  385 functions that had no `psx`;
- `psx_overlay` alone on 125 functions whose existing `psx` (with no overlay)
  a seed or a derivation confirms (110 area, 15 scenario). `sym_twins` reads
  these in the right file now.

These were not written: 7 functions whose held `psx` names another
area's copy, all small shared camera helpers.

The seeds' `psx_evidence` reads "table seed 2026-10-19 (docs/install-gap-pairs.md
8; a scratch seeding of exe_imm.py pair, not one of its methods)". The derived
pairs' evidence ends "from section 8's table seeds".

Checks:
- `tomllib` loads 10,100 functions.
- `gen_symbols.py` gives 10,088 ours, unchanged.
- `exe_imm.py selftest` reports 0 problems.

The 385 written, by method: the seeds (6 boss, 46 area, 147 scenario), then
tables 120, order-align 44, calls 13, order 5 and tablesearch 4.

**82 of the 199 written seeds fail the fingerprint gate** (`pair_agrees`) and
rest on the engine tables' slot-for-slot evidence alone. That is not only the
boss `*_Enter` functions of 8.2: area and scenario seeds are among them. Every
derived pair passes its method's gate, except 4 `tablesearch` pairs and 2
`tables` shape passes, which is by design.

### 8.5 The rows left

| Row | Why |
|---|---|
| `Area96_Handlers+0x184`, `Area154_ChoicePairs+0x18C8` / `+0x19B8`, `Area172_EffectStates+0xFF0`, `Area196_Keys1+0x658` | No reader by `text_readers` (section 5). No pairing reaches a row nothing reads. |
| `Area115_Directions` | Reader `Area115_Record8Place` (`0x419680`). Area 115's descriptor has no handlers and no init on either side, and nothing in AREA115 is paired. |
| `Area121_BandHeights` | Reader `Area121_DrawTopBand` (`0x41C790`). Its PC neighbours' pairs are another file's copy (`Area121_RingRise` with AREA104), so `order` has no bound inside AREA121. |
| `Area135_EffectKinds` | **Twinned** (`Area135_SpawnKind1AtLeader`, handler 19, `AREA135.EMI#14` `0x801F50B0`), not placed. `find_imm`'s region is `0x62C164..0x62C175`. It begins with four PC alignment zeros (`0x62C164..0x62C167`) that are non-zero on the disc before the twin's address (`AREA135.EMI#14` `0x801F626C`); from `0x62C168`, 12 of 13 bytes are equal. That is `Area141_CellEntries`' shape (section 5), a run offset by leading zeros, not a trailing-zero tail. Section 6's zero-tail rule is already in `find_imm` on main and does not reach it. |
| `Scena02_EffectX` | Readers `Scena02_PlaceEffect70` / `68` are still unpaired after the run. Why the order walks miss them was not measured. |
| `Scena03_Bob4`, `Scena03_Bob2` | Readers `Scena03_BobParty4` / `2`. They lie between two paired SCENA03 functions (`0x801FC1D0`, `0x801FC46C`) with exactly two PSX functions between (`0x801FC3B4`, `0x801FC410`), whose lui pairs address two words 4 apart, as the PC's operands `0x660F94` / `0x660F98` are. The gate refuses them (no call; the constants are offsets). A position-only rule would place them. Not written, because section 1 found position alone right only about half the time for table slots. |
| `LinkedObjectD_Amounts` | Reader `LinkedObjectD_ShowAmount` (`0x537CE0`). No function in the `LinkedObject*` group is paired, and which overlay holds their twins is not established. |

`Area91_RingTurns`, listed in section 5, was already out of `exe-gaps.tsv`
(PT 6.7). The area seeds twin its reader `0x412590` too.

Section 8 was independently re-derived on 2026-10-19: all 385 pairs and 125
`psx_overlay` fields reproduced, and the recipes regenerated in scratch over the
seven discs matched the committed ones byte for byte apart from the new rows.

## For the other files

- **`install-gap-census.md` section 8** (the coordinator's):
  - the rows in section 4 are placed;
  - `ChlorineCloud_Offsets` is placed both by `immref` and by CW / PT's twin;
  - P1 item 8 is not a list (section 7);
  - P2 `EffectKind2A_States` waits on section 6's change.
- **PT:** section 6 (`find_imm`), section 7 (`REGIONS`), and the
  `Area104_PlateAnims` exclusion.
- **The owner or the coordinator:** the `psx` corrections in 2.3.
- **Section 8, for the coordinator:**
  - regenerate the recipes. Expect the four `immref` rows of 8.1 on psx-jp;
  - census 8.5: the boss rows and `Scena04_Quake` / `_TremorSteps` move
    to FIXED once they place (`BossGaist_Anims+0x2` and `BossTorch_Sounds`
    on psx-jp only, 8.1). `Area135_EffectKinds` moves to "twinned,
    not placed";
  - a plain `exe_imm.py pair --write` still has 99 pairs and 25 overlays to
    give (8.4).

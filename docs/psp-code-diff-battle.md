# The PSP code diff, battle and effects half: what the 317 differing pairs are

**Status:** IN PROGRESS (2026-10-12) - stage B of group CD (CD1) of the
PSP-only round: the round's priority questions answered, and the 317
differing pairs of [`psp-code-diff.md`](psp-code-diff.md) 7's "Battle and
effects" table classed. Read-only: nothing in `src/` or the importer changed.
Nothing here has been seen in play. The field half is CD2's and CD3's.

## 0. The content changes found

| # | Where (PSX-JP; PSP-JP / PSP-EU) | What the PSP does that the PSX does not | Suite | Evidence |
|---|---|---|---|---|
| C1 | `BATTLE.EMI#3` `0x801d43bc` (the turn step, row 33) and `0x801d653c` (the round start, row 213); PSP-JP `0x11df2c` / `0x11f2f4`, PSP-EU `0x11db9c` / `0x11ef64` | Both check the battle-end byte (PSX `0x80146324`, the PC's `0x904AE8`) and end the battle when it is non-zero. The PSP inserts one test first: **with boss set-up 27** (`0x801462E6` = `0x1B`), **end bit 0 set and bit 2 clear, the battle goes on** as if nothing had ended. Bit 0 is "the loss" in the boss end hooks' reading (`symbols.toml`, fight 1's end hook); set-up 27's event hook (`Boss27_Event`, [`boss_sf.md`](boss_sf.md)) is what sets bit 2. So in that fight the PSP no longer ends on a party loss unless the event hook has set bit 2 | data (a code member, `psp=`; DIV-0108) | read side by side, both functions, both PSP releases |
| C2 | `BATTLE.EMI#15` `0x800a8f4c` (an enemy's entry, row 47); PSP-JP `0x141414`, PSP-EU `0x1411e0` | After the enemy's set-up (`0x800a9148`, PSP `0x141ba0`) and before the next call, the PSP adds: **in area 198 (`0xC6`) with boss set-up 55 (`0x37`), the enemy record's `+0x98` is set to 791 (`0x317`)**. `+0x98` is the actor's agility (`src/game/battle_damage.cpp`: the turn order sorts on it). **Corrected 2026-10-12 (PB, [`psp-battle-changes.md`](psp-battle-changes.md) 2.2):** `+0x98` is the PSP's offset of the enemy's first item word, ours `+0xA8` (the item a steal takes and the first drop rolls), not agility; 0x317 is accessory 0x17, "Ivory Charm" by the sibling's `names/items.toml`. The area is PSX `0x80143F00` by the learned data map; the constant 791 occurs in no PSX function and in this one PSP function | data (a code member, `psp=`; DIV-0108) | read side by side; `791` searched in every function of both sides |

What the fight in area 198 under set-up 55, and set-up 27's fight, are in play
is the owner's to say: this doc does not name them. Neither change has a PSX
counterpart in the captures. `BOSS027` is not among the sibling's captured
overlays, so set-up 27's own hooks were compared only through the PC's reading
(`boss_sf.md`), not PSX against PSP.

Graphics (widescreen) changes found that [`psp-widescreen.md`](psp-widescreen.md)
3a does not list:
- **a full-screen effect mesh widened**: MAGIC064 `0x801f103c` (PSP
  `0x178834`) and MAGIC102 `0x801eec80` (PSP `0x19de14`) loop over 28 x 56
  cells on the PSX and 33 x 58 on the PSP (strides `0x70` and `0x38` against
  `0x84` and `0x42`); the helper `BATTLE.EMI#15` `0x800ac020` (PSP `0x1487d4`)
  bounds a column by 56 against 58;
- **a sprite cull widened**: MAGIC105 `0x801efcf0` (PSP `0x1a3340`) keeps x
  in `[-64, 384]` on the PSX and `[-96, 416]` on the PSP (y unchanged);
  psp-widescreen.md 3b says the sprite culls were left, and this one was not;
- the 3a item 6 rectangle (`-32` / `351` or `383` for `0` / `319`) in ten
  more effect functions (rows 134, 163, 177, 185, 245, 278, 280, 281, 294,
  301).

## 1. The round's priority questions

Reported to the coordinator as each was answered, 2026-10-12.

1. **`RYUD` and the 15 arenas: our code runs the PSP's bytes.**
   - `RYUD00..03` section 3 is one 0x1B000-byte bank, byte-identical to arena
     member 7's bank. Each file lands at one of the four places that bank can
     occupy in an arena loaded at `0x80033800`: `+0x3F00C`, `+0x2400C`,
     `+0x3600C`, `+0x1B00C`. `RYUD10..13` are the same bank cut short. So
     `RYUD` reloads that bank in place, and no code holds its address.
   - The readers go through the sprite's bank pointer `+0x4C` and the bank's
     own offsets ([`arena-edits.md`](arena-edits.md) 1):
     - `Sprite_ScriptStart`, `Tick`, `TickOnce` and `Step` (PSX `0x8014D804`,
       `0x8014D86C`, `0x8014D9E0`, `0x8014DA8C`) are `same`.
     - `Sprite_SetFrameQueueUpload` (PSX `0x8014DAFC` + `0x8014DB0C`, PSP-JP
       `0x2CE4`) is the same logic, read side by side. Stage A paired its
       4-instruction entry with PSP `0xA540C`: a mispair.
     - `Battle_InitPartyMember` (GAME.EMI `0x801C224C`) is `minor`.
   - The PSP's P8 bank moves its header offsets, changes one of 16 images (12
     bytes longer) and lengthens the scripts by 4. The bank keeps its fixed
     size.
   - Not verified: that the changed image decodes in our renderer.
2. **The button-label page: art only.**
   - `0x1A080200` is a type-3 image: VRAM (832, 256), 32 KB. The GPU reads
     it, by texture page and UV. Our code does not read it as data.
   - 25 functions pass (832, 256). Their twins are `same`, `minor` or
     `widescreen`, or differ only in the draw packet's size.
   - `BATTLE.EMI#15` `0x800aecdc` / PSP `0x14d460`, read in full: the same
     40 x 24 cells, CLUT and table indexing on both sides.
3. **The two `BATL_END` `exp_table` readers are encoding.**
   - `0x801ef144` and `0x801efe60` match their twins step for step.
   - The PSP's `0x7608` / `0x7644` / `0x7648` are the low halves of
     relocated format-string addresses. The diff counted them as constants
     because of a defect: it clobbered registers before a call's delay slot.
     That defect is fixed here (section 3).

## 2. The method

- **Read**: the pair was disassembled and compared instruction by instruction,
  or at every differing constant and call (scratch `rd.py`, `ctx.py`,
  `sbs.py`), 46 rows.
- **Rerun**: after the tool fixes of section 3, the same PSX function pairs
  with the same PSP function and the diff says `same` or `minor` (one
  difference): 86 rows. Each of the defects fixed was read on a row first:
  - the delay slot (row 3 of the priority answers);
  - the resident GAME.EMI: rows 246 and 39, where the PSX call lands in
    GAME.EMI;
  - a reciprocal shared by two divisors: row 22, `x / 25` and `x / 100`.
- **Pattern**: the row has the same difference as a read row (named in its
  line), 82 rows. The patterns, each read at least once:
  - the enemy index folded into an address (`id - 3`, times `0x118`);
  - a struct copied in another word size or loop count;
  - a PSX-RAM address (`0xE4048` is `0x800E4048` in the PSP's EMI buffer);
  - a range test rewritten (`addiu` + `sltiu` against two compares);
  - a reciprocal against `div`;
  - the draw-mode packet's 12 bytes against 16;
  - one case run of a jump-table switch: the PSX row starts after
    `jr $v0`, and the PSP twin holds the whole switch;
  - the widescreen rectangle.
- **Calls**: the call lists align one to one and only unmapped globals
  differ: 6 rows.
- **Scan**: the rest (97 rows). A scratch `cmpscan.py` lists, per row, the
  immediates each side compares a value against (`li` + `beq` / `bne`,
  `slti` / `sltiu`). It keeps the non-trivial ones found on one side only:
  C1 and C2 both show up this way (`0x1B`; `0xC6`, `0x37`). Every row it
  flagged was read. The 97 rows it found with the same compare set on both
  sides are classed encoding without a read. **A content change that only
  alters a stored value, with no new compare, could hide among them.** This
  is the main limit of this half.

A scan of every battle pair for one-sided reads of the boss set-up byte or the
area (scratch `special.py`) found C1 and C2 again. Its other hits were the
compiler repeating a load in a branch-likely delay slot.

## 3. The tool changes (`tools/psp_pair.py`)

- **A call clobbers the caller-saved registers after its delay slot**, not
  before. An address formed in the delay slot had been counted as a constant.
- **The boot EXE's code range** is now taken from `functions.tsv` without its
  `low` rows. Those rows are blobs at `0x8011CD40`..`0x801F2C04`. They had
  stretched the range over GAME.EMI, so an overlay's call into GAME.EMI was
  resolved to the boot EXE. CD3 found the same and proposed reordering the
  resolve; this fixes the cause, and the resolve also tries GAME.EMI after
  BATTLE.EMI for the battle families.
- **The diff's constants**: two divisors whose ratio is a power of two match,
  because one reciprocal serves both. A PSP constant whose PSX-RAM address the
  PSX function references is dropped.

Measured on PSP-JP after the changes (`pair`, without `--check-eu`):
- 9,692 of 12,904 PSX functions paired. The PSX count is lower than stage
  A's 13,134 because the blobs no longer yield starts.
- 141 of the 175 named game functions paired.
- `diff`: battle 276 `differs` and field 672, against stage A's 317 and 908.

**`psp-code-diff.md`'s tables were not regenerated**, so CD2's and CD3's row
numbers stand. This doc's rows are that table's rows.

## 4. The tallies

| class | rows |
|---|---|
| content | 3 (C1 twice, C2) |
| widescreen | 14 |
| platform | 5 |
| encoding | 292 |
| unsettled | 3 (rows 38, 76, 108) |

The unsettled rows:
- **Row 38** (MAGIC057): the PSP twin lies in the boot range and stores
  other values. Probably a mispair.
- **Row 76** (MAGIC060): a jump table and a test against `0x83` on the PSX,
  not matched case for case.
- **Row 108** (BOSS004): a seven-case switch split differently on each side.

## 5. Not verified

- The 97 scan rows (section 2), and every row classed by pattern, were not
  read.
- C1 and C2 have not been seen in play. Their fights are not identified in
  this doc.
- Set-up 27's own hooks on the PSP were not compared: the sibling has no
  capture of `BOSS027`.
- Whether C1 repairs a PSX fault (a loss that ends a fight that should
  continue) or changes intended behaviour is the owner's call. The PSX twin
  of `Boss27_Event` would settle what the original did
  ([`psx-twin-check.md`](psx-twin-check.md)). It is not in the captures.
- The remaining CD3 cautions apply here too: library mispairs (SetDrawMode
  and GetGraphType paired with the wrong PSP functions) and switches cut at
  their case labels.

## 6. For the other files (the coordinator's)

**Folded in 2026-10-12** (the coordinator's pass, branch `psp/ledger`): The README row, owner-review (C1 and C2, answered: DIV-0108) and the `psp-widescreen.md` 3a rows are in; C2's reading is corrected in section 0 (PB).

- **`docs/README.md`, one row:** `psp-code-diff-battle.md` | The PSP code
  diff's battle and effects half, stage B (2026-10-12): the 317 differing
  pairs classed. Two content changes:
  - boss set-up 27 does not end on a loss without its event bit;
  - area 198 with set-up 55 sets an enemy's first item word to 0x317 (read as agility here first; corrected by PB).

  Also four widescreen changes psp-widescreen.md 3a lacks (an effect mesh, a
  sprite cull). The priority answers are in section 1: `RYUD`, the arenas,
  the button page and `exp_table` all run on our code.
- **`docs/owner-review.md`, one entry:** C1 and C2. Which fights they are, and
  whether either belongs in the data suite as PSP content (default off, per
  the unified-data plan) or is a repair to take as Intent. C1 first: it may
  be a fix.
- **`docs/psp-widescreen.md` 3a**, rows to add: the effect mesh (MAGIC064,
  MAGIC102, `0x800ac020`) and the MAGIC105 sprite cull. CD3 reports three
  more in the field half.
- **No ledger entry**: nothing of ours changed. If the owner takes C1 or C2,
  that is a DIV then.

## 7. The rows

`row` is the row of `psp-code-diff.md` 7's battle table. The evidence column
uses the labels of section 2.

| row | PSX (file) | name | PSP-JP | class | evidence | what |
|---|---|---|---|---|---|---|
| 1 | `801d1228` BATTLE/BATTLE.EMI#3 | Battle_Init | `00120130` | encoding | calls | call lists align (File_LoadRequest to the PSP's loader 0x970fc: platform); globals the data map lacks |
| 2 | `800a79ac` BATTLE/BATTLE.EMI#15 | Formation_ApplyStatMods | `0013ae88` | encoding | read | party-record fields as base + offset (stride 0x140) on the PSP |
| 3 | `801dc00c` BATTLE/BATTLE.EMI#3 | Battle_CalcDamage | `0012764c` | encoding | read | the 8-byte variance table copied from 0x801D0C7C on the PSX is built inline on the PSP (bytes equal); enemy index (id - 3) folded into the address |
| 4 | `800987f0` BATTLE/BATTLE.EMI#15 | Escape_Failed | `0012c79c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 5 | `801d1e84` BATTLE/BATTLE.EMI#3 | BattleMenu_DirectionHold (label) | `001205a0` | encoding | calls | battle-local inventory-add copy against the boot Inventory_Add; pair via a label (similarity) |
| 6 | `801e3b8c` BATTLE/BATTLE.EMI#3 | Battle_ResolveAction_Enemy | `00140e24` | encoding | read | call lists align; a byte read as (halfword & 0xFF00) >> 8 on the PSP; fields as base + offset |
| 7 | `801daab4` BATTLE/BATTLE.EMI#3 | Battle_BuildTurnOrder | `00126ee0` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 8 | `801dc704` BATTLE/BATTLE.EMI#3 | Battle_HitCheck_PartyTarget | `001251e4` | encoding | read | enemy index folded; a 0xEF bit-clear the PSP does by insert |
| 9 | `801dc85c` BATTLE/BATTLE.EMI#3 | Battle_HitCheck_EnemyTarget | `001253a0` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 10 | `801dcd18` BATTLE/BATTLE.EMI#3 | Battle_ScaleDamage | `00125868` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 11 | `801ef34c` BATTLE/BATL_END.EMI#0 | BattleResult_Setup (label) | `0014a3a0` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 12 | `80095418` BATTLE/BATTLE.EMI#15 | ItemMenu_Confirm | `0012b4fc` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 13 | `80098278` BATTLE/BATTLE.EMI#15 | Escape_Roll | `0012c284` | encoding | read | 0x140 / 0x348 are the member stride and base, address arithmetic on the PSP |
| 14 | `80098450` BATTLE/BATTLE.EMI#15 | Escape_Begin | `0012c498` | encoding | read | battle-local inventory-add copy (0x801dde44, same logic as PSP 0x126768) called on the PSX, the boot Inventory_Add on the PSP; File_LoadRequest is platform |
| 15 | `801d7e80` BATTLE/BATTLE.EMI#3 | BattleBanner_DrawFrame | `00121b38` | platform | read | RGB555 decode the same; the PSX's 12-byte draw-mode packet against the PSP's 16 |
| 16 | `801d8060` BATTLE/BATTLE.EMI#3 | BattleMenu_TargetCursor | `00121d28` | platform | pattern | as row 15 (the same code shape) |
| 17 | `801dfc68` BATTLE/BATTLE.EMI#3 | Battle_ResolveAction_Party | `0013daec` | encoding | pattern | as row 6 (the party twin of the same routine) |
| 18 | `80099954` BATTLE/BATTLE.EMI#15 | EnemyAI_ApplyAction | `0012e750` | encoding | read | one case run of a jump-table switch on the PSX (head 0x80099954, with row 200); chance / 10000 by reciprocal on the PSX (read as 125), by div on the PSP |
| 19 | `8009a158` BATTLE/BATTLE.EMI#15 | Effect_ApplyResult (label) | `001339f0` | encoding | pattern | the enemy-index fold of row 3 (-3 / 0xFFFD / 0x10000) |
| 20 | `801dbb40` BATTLE/BATTLE.EMI#3 | Battle_ApplyDamage | `00127314` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 21 | `801dcaa0` BATTLE/BATTLE.EMI#3 | Battle_BaseDamage | `00125624` | encoding | read | enemy index (id - 3) times 0x118 folded into the address offset on the PSP |
| 22 | `801dfa14` BATTLE/BATTLE.EMI#3 | Battle_SwingCue_Step | `0013ba28` | encoding | read | rand() % 100 by reciprocal on the PSX, by div on the PSP (the reciprocal reads as /25 to the normaliser) |
| 23 | `801e1fa8` BATTLE/BATTLE.EMI#3 | Battle_SwingCue_Step2 | `0013d3ac` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 24 | `8017b480` SLPS_009.90 | SetLineG4 | `0009fcfc` | platform | read | SetLineG4, a Psy-Q library macro (packet length 9, code 0x5C, the 0x55555555 terminator); the PSP's 3-instruction twin builds its own packet |
| 25 | `800abc84` BATTLE/BATTLE.EMI#15 |  | `001483cc` | encoding | read | 0x20-byte stats copy word-wise on the PSX, halfword-wise on the PSP; the same check call |
| 26 | `800b0a20` BATTLE/BATTLE.EMI#15 |  | `0014f9dc` | encoding | read | x = (i + 5) * 8 on the PSX, i * 8 + 40 on the PSP; call lists align (GAME.EMI menu helpers and their twins) |
| 27 | `800c223c` BOSS/BOSS025.EMI#13 |  | `001e64e0` | encoding | read | the PSX row is case code of a six-case jump-table switch (0x800c2208..0x800c2634, 246 instructions against the PSP's 233); Msg_OpenScript 0x1B..0x1F and the 0x46 store are in the PSX's next piece |
| 28 | `800af4f0` BATTLE/BATTLE.EMI#15 |  | `0014e578` | encoding | read | as row 26 (+88 = (i + 11) * 8) |
| 29 | `801f103c` BMAGIC/MAGIC064.EMI#3 |  | `00178834` | widescreen | read | a full-screen effect mesh widened: loops of 28 x 56 cells (stride 0x70) on the PSX, 33 x 58 (stride 0x84) on the PSP; not in psp-widescreen.md 3a |
| 30 | `801eec80` BMAGIC/MAGIC102.EMI#3 |  | `0019de14` | widescreen | read | the same mesh in MAGIC102 (rows of 28 by 56 two-byte cells widened to 33 by 58); the boss-id switch (0x1C, 0x23, 0x2F, 0x34) the same, as a jump table on the PSP |
| 31 | `800afbc8` BATTLE/BATTLE.EMI#15 |  | `0014eca8` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 32 | `800b025c` BATTLE/BATTLE.EMI#15 |  | `0014f25c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 33 | `801d43bc` BATTLE/BATTLE.EMI#3 |  | `0011df2c` | CONTENT | read | boss set-up 27 keeps fighting on a loss: see the content table (with row 213) |
| 34 | `801ef484` BMAGIC/MAGIC151.EMI#3 |  | `001c70e0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 35 | `8009de6c` BATTLE/BATTLE.EMI#15 |  | `001343a8` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 36 | `801efcf0` BMAGIC/MAGIC105.EMI#3 |  | `001a3340` | widescreen | read | a sprite cull widened from x in [-64, 384] to [-96, 416] (y unchanged); not in psp-widescreen.md 3a, whose 3b says the sprite culls were left |
| 37 | `800a0170` BATTLE/BATTLE.EMI#15 |  | `00134790` | encoding | read | status masks 0x840 / 0x864 / 0x80C / 0x814 / 0x804 tested as one andi each on the PSX, bit by bit on the PSP; the same branches |
| 38 | `801ef900` BMAGIC/MAGIC057.EMI#3 |  | `00017c84` | unsettled | read | PSP twin 0x17c84 sits in the boot range and stores other values (0x15, 0x39, 0xC8, 0xF0, 0x104 against the PSX's 0x1D, 0xA0): probably a mispair (similarity-weak) |
| 39 | `800c2220` BOSS/BOSS013.EMI#16 |  | `001e2ac4` | encoding | read | the PSX side is one case of a six-case jump-table switch (0x800c2138..0x800c2348, split at the case labels); the PSP twin is the whole switch, Msg_OpenScript(0x27) in case 1 on both |
| 40 | `800c2bc8` BOSS/BOSS025.EMI#13 |  | `001e6c14` | encoding | pattern | one case run of a jump-table switch (head 0x800c2b98, 101 instructions against the PSP's 99) |
| 41 | `801eed50` BMAGIC/MAGIC068.EMI#3 |  | `0017cb7c` | encoding | read | message ranges 71..90 / 51..70 / 31..50 as addiu + sltiu on the PSX, two compares on the PSP; the same message numbers |
| 42 | `801f0290` BATTLE/BATL_END.EMI#0 |  | `0014bdb0` | platform | pattern | as row 15 (BATL_END copy) |
| 43 | `8009770c` BATTLE/BATTLE.EMI#15 |  | `0012a610` | encoding | calls | call lists align one to one; globals the data map lacks |
| 44 | `8009bae0` BATTLE/BATTLE.EMI#15 |  | `0012f6bc` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 45 | `8009cae8` BATTLE/BATTLE.EMI#15 |  | `0013056c` | encoding | calls | call lists align one to one; globals the data map lacks |
| 46 | `8009ed24` BATTLE/BATTLE.EMI#15 |  | `00131d14` | encoding | calls | call lists align one to one; globals the data map lacks |
| 47 | `800a8f4c` BATTLE/BATTLE.EMI#15 |  | `00141414` | CONTENT | read | area 198 with boss set-up 55: the enemy's first item word set to 0x317 (first read as agility; corrected by PB): see the content table |
| 48 | `801e0af8` BATTLE/BATTLE.EMI#3 |  | `0013c108` | encoding | calls | call lists align one to one; globals the data map lacks |
| 49 | `801ef818` BMAGIC/MAGIC144.EMI#3 |  | `001925d8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 50 | `800a09a0` BATTLE/BATTLE.EMI#15 |  | `00133058` | encoding | read | stat / 5 by reciprocal on the PSX, stat * 100 / 500 on the PSP; chance product / 10000 the same |
| 51 | `800a0af4` BATTLE/BATTLE.EMI#15 |  | `001331a0` | encoding | pattern | as row 50 |
| 52 | `800a0c24` BATTLE/BATTLE.EMI#15 |  | `001332c8` | encoding | pattern | as row 50 |
| 53 | `800a0d78` BATTLE/BATTLE.EMI#15 |  | `00133410` | encoding | pattern | as row 50 |
| 54 | `800a7ff0` BATTLE/BATTLE.EMI#15 |  | `0013b068` | encoding | read | struct offsets and the same call sequence (similarity-weak) |
| 55 | `800ae99c` BATTLE/BATTLE.EMI#15 |  | `0014d170` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 56 | `801eed04` BMAGIC/MAGIC092.EMI#3 |  | `00192178` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 57 | `801efd7c` BMAGIC/MAGIC102.EMI#3 |  | `0019db00` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 58 | `801f0778` BMAGIC/MAGIC102.EMI#3 |  | `0019f354` | encoding | read | culls x in [-60, 380], y in [-150, 300] on both (addiu + sltiu against two slti) |
| 59 | `801ef290` BMAGIC/MAGIC107.EMI#3 |  | `001a47e8` | platform | read | GTE and draw-mode code (tpage 0x25 / 0x15, packet length 12): the PSP's draw layer |
| 60 | `801ef544` BMAGIC/MAGIC107.EMI#3 |  | `001a4a84` | encoding | pattern | effect arithmetic (0x500 a fixed-point constant folded); draw calls on both |
| 61 | `80093b7c` BATTLE/BATTLE.EMI#15 |  | `00127d60` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 62 | `80093cc8` BATTLE/BATTLE.EMI#15 |  | `00127ef8` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 63 | `80095be0` BATTLE/BATTLE.EMI#15 |  | `00129388` | encoding | read | struct offsets; the boot item helper called directly |
| 64 | `80095d8c` BATTLE/BATTLE.EMI#15 |  | `00129584` | encoding | pattern | as row 63 |
| 65 | `80097be0` BATTLE/BATTLE.EMI#15 |  | `0012aa90` | encoding | read | a party record copied field by field |
| 66 | `8009c9d0` BATTLE/BATTLE.EMI#15 |  | `00130464` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 67 | `8009dc18` BATTLE/BATTLE.EMI#15 |  | `001313e4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 68 | `800a13c0` BATTLE/BATTLE.EMI#15 |  | `00134e0c` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 69 | `800a9148` BATTLE/BATTLE.EMI#15 |  | `00141ba0` | encoding | read | the enemy set-up called from row 47 at the same place; field copies unrolled on the PSX |
| 70 | `800ad61c` BATTLE/BATTLE.EMI#15 |  | `0014c5c4` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 71 | `801d74dc` BATTLE/BATTLE.EMI#3 |  | `0012309c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 72 | `801f0550` BMAGIC/MAGIC003.EMI#3 |  | `00153b4c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 73 | `801eef8c` BMAGIC/MAGIC004.EMI#3 |  | `00154074` | encoding | read | the range 133..136 as addiu + sltiu on the PSX, two compares on the PSP; the same SE 0x101 |
| 74 | `801ef174` BMAGIC/MAGIC060.EMI#0 |  | `00172e58` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 75 | `801ef5f4` BMAGIC/MAGIC060.EMI#0 |  | `00173ca0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 76 | `801efc84` BMAGIC/MAGIC060.EMI#0 |  | `00173f84` | unsettled | read | the PSX's MAGIC060 0x801efc84 tests byte +0xB against 0x83 and runs a jump table; the PSP twin (153 against 231 instructions) was not matched case for case |
| 77 | `801ef580` BMAGIC/MAGIC087.EMI#3 |  | `0018ef00` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 78 | `801f02c8` BMAGIC/MAGIC172.EMI#3 |  | `001d1844` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 79 | `801ef0a8` BATTLE/BATL_OVR.EMI#0 |  | `0014aa1c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair platform |
| 80 | `8009bd6c` BATTLE/BATTLE.EMI#15 |  | `0012f8f4` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 81 | `8009bfc0` BATTLE/BATTLE.EMI#15 |  | `0012fb40` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 82 | `8009c054` BATTLE/BATTLE.EMI#15 |  | `0012fbd8` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 83 | `8009f820` BATTLE/BATTLE.EMI#15 |  | `001325e8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 84 | `800a0080` BATTLE/BATTLE.EMI#15 |  | `00132e70` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 85 | `800a0680` BATTLE/BATTLE.EMI#15 |  | `00134b88` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 86 | `800a3d7c` BATTLE/BATTLE.EMI#15 |  | `001379b8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 87 | `800ac020` BATTLE/BATTLE.EMI#15 |  | `001487d4` | widescreen | read | the mesh helper's bound 56 (0x38) raised to 58 (0x3A), as rows 29 and 30 |
| 88 | `801ddc7c` BATTLE/BATTLE.EMI#3 |  | `001265d0` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 89 | `801e722c` BATTLE/BATTLE.EMI#3 |  | `00145714` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 90 | `801eecc0` BMAGIC/MAGIC020.EMI#3 |  | `0015c9fc` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 91 | `801ef4e0` BMAGIC/MAGIC044.EMI#3 |  | `00166cac` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 92 | `801efa4c` BMAGIC/MAGIC044.EMI#3 |  | `0016714c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 93 | `801f04ec` BMAGIC/MAGIC056.EMI#3 |  | `00187904` | encoding | pattern | colour / texture arithmetic (-128, 28, 0x4000000) of an effect; PSP twin half the size: the draw layer |
| 94 | `801ef454` BMAGIC/MAGIC058.EMI#3 |  | `001708c0` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 95 | `801efa9c` BMAGIC/MAGIC060.EMI#0 |  | `001734a0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 96 | `801ef984` BMAGIC/MAGIC066.EMI#3 |  | `0017a380` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 97 | `801f0274` BMAGIC/MAGIC069.EMI#3 |  | `0017df70` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 98 | `801efbec` BMAGIC/MAGIC079.EMI#3 |  | `0018788c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 99 | `801eef74` BMAGIC/MAGIC091.EMI#3 |  | `0016d3a4` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 100 | `801ef804` BMAGIC/MAGIC095.EMI#3 |  | `00195b18` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 101 | `801eec5c` BMAGIC/MAGIC097.EMI#3 |  | `001996b8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 102 | `801eec5c` BMAGIC/MAGIC098.EMI#3 |  | `001983c8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 103 | `801ef1d8` BMAGIC/MAGIC103.EMI#3 |  | `0019ffe8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 104 | `801ef370` BMAGIC/MAGIC104.EMI#3 |  | `001a130c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 105 | `801efc44` BMAGIC/MAGIC167.EMI#3 |  | `001cd364` | encoding | read | a clamp (slti 0x11) the PSP does by min / max; draw calls on both |
| 106 | `801f05e8` BMAGIC/MAGIC172.EMI#3 |  | `001d1ad0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 107 | `801eeeec` BMAGIC/MAGIC173.EMI#3 |  | `001d2878` | encoding | read | the same cases 0x61 and 0x6C as a compare chain on the PSP |
| 108 | `800c1fe8` BOSS/BOSS004.EMI#13 |  | `001e0390` | unsettled | read | a seven-case jump-table switch on the PSX (186 instructions) against an 80-instruction PSP twin: case bodies split differently, not matched case for case |
| 109 | `800c217c` BOSS/BOSS018.EMI#16 |  | `001e3fd0` | encoding | pattern | one case run of a jump-table switch (head 0x800c214c); not every case compared |
| 110 | `800c1e34` BOSS/BOSS025.EMI#13 |  | `001e5e1c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 111 | `800c3088` BOSS/BOSS025.EMI#13 |  | `001e63d4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 112 | `800c2310` BOSS/BOSS033.EMI#16 |  | `001e8d10` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 113 | `800c18fc` BOSS/BOSS036.EMI#16 |  | `001ea4e8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 114 | `800c1b6c` BOSS/BOSS036.EMI#16 |  | `001ea748` | encoding | read | (word & 0xFFFF00) == 0x10700 on the PSX, byte compares 7 and 1 on the PSP |
| 115 | `800c19cc` BOSS/BOSS052.EMI#16 |  | `001ec544` | encoding | pattern | as row 114 (BOSS052) |
| 116 | `800c1dc8` BOSS/BOSS055.EMI#16 |  | `001ed730` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 117 | `801efe60` BATTLE/BATL_END.EMI#0 |  | `0014b97c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 118 | `801f093c` BATTLE/BATL_END.EMI#0 |  | `0014b774` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 119 | `8009aa74` BATTLE/BATTLE.EMI#15 |  | `0012ec88` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 120 | `8009ab64` BATTLE/BATTLE.EMI#15 |  | `0012ed88` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 121 | `8009bc00` BATTLE/BATTLE.EMI#15 |  | `0012f7cc` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 122 | `8009c16c` BATTLE/BATTLE.EMI#15 |  | `0012fce4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 123 | `8009c834` BATTLE/BATTLE.EMI#15 |  | `00130308` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 124 | `8009d138` BATTLE/BATTLE.EMI#15 |  | `00130ac4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 125 | `8009d3dc` BATTLE/BATTLE.EMI#15 |  | `00130d38` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 126 | `8009d610` BATTLE/BATTLE.EMI#15 |  | `00130f04` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 127 | `8009f538` BATTLE/BATTLE.EMI#15 |  | `0013236c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 128 | `800a1204` BATTLE/BATTLE.EMI#15 |  | `00133840` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 129 | `800a45fc` BATTLE/BATTLE.EMI#15 |  | `001362a8` | encoding | pattern | only globals or field offsets differ (the data map, base + offset addressing) |
| 130 | `800a573c` BATTLE/BATTLE.EMI#15 |  | `00138a0c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 131 | `800a84a0` BATTLE/BATTLE.EMI#15 |  | `0013a268` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 132 | `800a8840` BATTLE/BATTLE.EMI#15 |  | `0013a52c` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 133 | `800a9894` BATTLE/BATTLE.EMI#15 |  | `00141e54` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 134 | `800abf3c` BATTLE/BATTLE.EMI#15 |  | `001486fc` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 135 | `800ac73c` BATTLE/BATTLE.EMI#15 |  | `00148f28` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 136 | `800accc4` BATTLE/BATTLE.EMI#15 |  | `001494ac` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 137 | `800acec0` BATTLE/BATTLE.EMI#15 |  | `00149624` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 138 | `800acf80` BATTLE/BATTLE.EMI#15 |  | `001496e0` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 139 | `800adec4` BATTLE/BATTLE.EMI#15 |  | `0014d704` | encoding | pattern | only globals or field offsets differ (the data map, base + offset addressing) |
| 140 | `800ae650` BATTLE/BATTLE.EMI#15 |  | `0014daf4` | encoding | pattern | only globals or field offsets differ (the data map, base + offset addressing) |
| 141 | `800b1160` BATTLE/BATTLE.EMI#15 |  | `0014e3d4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 142 | `801d1820` BATTLE/BATTLE.EMI#3 |  | `0011c504` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 143 | `801d360c` BATTLE/BATTLE.EMI#3 |  | `001210c4` | encoding | read | action id in {0x24, 0x25} or 0x8C on both (a range test on the PSX) |
| 144 | `801d8240` BATTLE/BATTLE.EMI#3 |  | `00121f18` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 145 | `801d8420` BATTLE/BATTLE.EMI#3 |  | `00123800` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 146 | `801da7a4` BATTLE/BATTLE.EMI#3 |  | `00126c5c` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 147 | `801ddab8` BATTLE/BATTLE.EMI#3 |  | `00126434` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 148 | `801e61fc` BATTLE/BATTLE.EMI#3 |  | `00144804` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 149 | `801e7ad8` BATTLE/BATTLE.EMI#3 |  | `0014632c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 150 | `801e85bc` BATTLE/BATTLE.EMI#3 |  | `001469e0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 151 | `801e8d10` BATTLE/BATTLE.EMI#3 |  | `00146104` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 152 | `801efec8` BMAGIC/MAGIC003.EMI#3 |  | `00153360` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 153 | `801eec98` BMAGIC/MAGIC004.EMI#3 |  | `00153df0` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 154 | `801ef118` BMAGIC/MAGIC006.EMI#3 |  | `0015508c` | encoding | pattern | the 0x601..0x9FF range test of row 158 |
| 155 | `801eec98` BMAGIC/MAGIC009.EMI#3 |  | `001565a8` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 156 | `801eec98` BMAGIC/MAGIC017.EMI#3 |  | `0015b93c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 157 | `801eed30` BMAGIC/MAGIC021.EMI#3 |  | `0015fbe4` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 158 | `801ef138` BMAGIC/MAGIC044.EMI#3 |  | `001663f0` | encoding | read | the range test 0x601 <= v < 0xA00 as addiu + sltiu on the PSX, two compares on the PSP |
| 159 | `801ef3b8` BMAGIC/MAGIC046.EMI#3 |  | `00168c8c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 160 | `801eeeb0` BMAGIC/MAGIC057.EMI#3 |  | `0016f6d0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 161 | `801eec90` BMAGIC/MAGIC065.EMI#0 |  | `001799b0` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 162 | `801eec94` BMAGIC/MAGIC067.EMI#3 |  | `0017bab0` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 163 | `801f00c4` BMAGIC/MAGIC067.EMI#3 |  | `0017b774` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 164 | `801eec70` BMAGIC/MAGIC075.EMI#3 |  | `001842a8` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 165 | `801ef3a0` BMAGIC/MAGIC082.EMI#3 |  | `0018989c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 166 | `801f0e74` BMAGIC/MAGIC083.EMI#3 |  | `0018ce50` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 167 | `801f0488` BMAGIC/MAGIC102.EMI#3 |  | `0019f024` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 168 | `801efe34` BMAGIC/MAGIC104.EMI#3 |  | `001a1c70` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 169 | `801ef944` BMAGIC/MAGIC113.EMI#3 |  | `001a9dd0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 170 | `801efda0` BMAGIC/MAGIC113.EMI#3 |  | `001aa5a8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 171 | `801ef6a8` BMAGIC/MAGIC114.EMI#3 |  | `001ab254` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 172 | `801eec80` BMAGIC/MAGIC115.EMI#3 |  | `001ad044` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 173 | `801f037c` BMAGIC/MAGIC126.EMI#3 |  | `001b87f8` | encoding | pattern | the 0x601..0x9FF range test of row 158 |
| 174 | `801ef058` BMAGIC/MAGIC129.EMI#3 |  | `001baa5c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 175 | `801eec88` BMAGIC/MAGIC132.EMI#3 |  | `001bec04` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 176 | `801eec7c` BMAGIC/MAGIC137.EMI#3 |  | `001bfae4` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 177 | `801efdf8` BMAGIC/MAGIC137.EMI#3 |  | `001c0330` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 178 | `801eec90` BMAGIC/MAGIC154.EMI#3 |  | `001c8af8` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 179 | `801ef210` BMAGIC/MAGIC172.EMI#3 |  | `001d0678` | encoding | pattern | the 0x601..0x9FF range test of row 158 |
| 180 | `801ef148` BMAGIC/MAGIC173.EMI#3 |  | `001d2ad8` | encoding | pattern | the 0x601..0x9FF range test of row 158 |
| 181 | `801efa34` BMAGIC/MAGIC173.EMI#3 |  | `001d3164` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 182 | `801ef048` BMAGIC/MAGIC213.EMI#3 |  | `001d3b98` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 183 | `801ef3cc` BMAGIC/MAGIC219.EMI#3 |  | `001d558c` | encoding | pattern | the 0x601..0x9FF range test of row 158 |
| 184 | `801eed18` BMAGIC/MAGIC220.EMI#3 |  | `001d6c34` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 185 | `801ef1e8` BMAGIC/MAGIC223.EMI#3 |  | `001d94f4` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 186 | `801efca0` BMAGIC/MAGIC225.EMI#3 |  | `001da76c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 187 | `800c1e7c` BOSS/BOSS002.EMI#16 |  | `001dfab0` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 188 | `800c1940` BOSS/BOSS004.EMI#13 |  | `001e510c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 189 | `800c19c0` BOSS/BOSS008.EMI#16 |  | `001e0f84` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair widescreen |
| 190 | `800c1f24` BOSS/BOSS013.EMI#16 |  | `001e28a0` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 191 | `800c1b40` BOSS/BOSS021.EMI#16 |  | `001e4dec` | encoding | pattern | one case run of a jump-table switch (head 0x800c1b08); not every case compared |
| 192 | `800c18e4` BOSS/BOSS028.EMI#16 |  | `001e7088` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 193 | `800c20cc` BOSS/BOSS033.EMI#16 |  | `001e8b18` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 194 | `800c1e0c` BOSS/BOSS054.EMI#16 |  | `001ed018` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 195 | `800c18a8` BOSS/BOSS055.EMI#16 |  | `001ed26c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 196 | `801ef144` BATTLE/BATL_END.EMI#0 |  | `00149f70` | encoding | pattern | only globals or field offsets differ (the data map, base + offset addressing) |
| 197 | `801eec7c` BATTLE/BATL_OVR.EMI#0 |  | `0014af60` | encoding | pattern | only globals or field offsets differ (the data map, base + offset addressing) |
| 198 | `80094e88` BATTLE/BATTLE.EMI#15 |  | `00128c0c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 199 | `8009975c` BATTLE/BATTLE.EMI#15 |  | `0012d30c` | encoding | read | 0xE4048 is PSX address 0x800E4048, an offset into the PSP's PSX-RAM buffer |
| 200 | `80099c7c` BATTLE/BATTLE.EMI#15 |  | `0012ea28` | encoding | pattern | the other case run of row 18's switch |
| 201 | `8009af28` BATTLE/BATTLE.EMI#15 |  | `0012f108` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 202 | `8009bee0` BATTLE/BATTLE.EMI#15 |  | `0012fa60` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 203 | `800a2298` BATTLE/BATTLE.EMI#15 |  | `00136ad4` | encoding | read | the same flow; 0x103 is SE_Play's argument on both |
| 204 | `800a5ec0` BATTLE/BATTLE.EMI#15 |  | `0013a730` | encoding | read | the same cases 0x3F, 0x1B, 0x44 in another order |
| 205 | `800a8c10` BATTLE/BATTLE.EMI#15 |  | `00141948` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 206 | `800a9fd8` BATTLE/BATTLE.EMI#15 |  | `001423d4` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 207 | `800aa21c` BATTLE/BATTLE.EMI#15 |  | `00146f30` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 208 | `800aa404` BATTLE/BATTLE.EMI#15 |  | `001470cc` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 209 | `800ad3c0` BATTLE/BATTLE.EMI#15 |  | `0014c368` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 210 | `800aecdc` BATTLE/BATTLE.EMI#15 |  | `0014d460` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 211 | `801d32fc` BATTLE/BATTLE.EMI#3 |  | `00120ddc` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 212 | `801d4b08` BATTLE/BATTLE.EMI#3 |  | `0011e434` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 213 | `801d653c` BATTLE/BATTLE.EMI#3 |  | `0011f2f4` | CONTENT | read | boss set-up 27 keeps fighting on a loss: the round-start check (with row 33) |
| 214 | `801d6c90` BATTLE/BATTLE.EMI#3 |  | `0011f98c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 215 | `801d6df0` BATTLE/BATTLE.EMI#3 |  | `0011fabc` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 216 | `801d6ef8` BATTLE/BATTLE.EMI#3 |  | `0011fbb4` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 217 | `801d7ce0` BATTLE/BATTLE.EMI#3 |  | `001219a8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 218 | `801d8ab4` BATTLE/BATTLE.EMI#3 |  | `00123e8c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 219 | `801d8dc8` BATTLE/BATTLE.EMI#3 |  | `00124184` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 220 | `801d94a4` BATTLE/BATTLE.EMI#3 |  | `001222a8` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 221 | `801da66c` BATTLE/BATTLE.EMI#3 |  | `00122fb4` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 222 | `801db020` BATTLE/BATTLE.EMI#3 |  | `00124818` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 223 | `801db3ac` BATTLE/BATTLE.EMI#3 |  | `00124b0c` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 224 | `801dcec0` BATTLE/BATTLE.EMI#3 |  | `001259dc` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 225 | `801dd410` BATTLE/BATTLE.EMI#3 |  | `00125e78` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 226 | `801ddfb4` BATTLE/BATTLE.EMI#3 |  | `001268b8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 227 | `801de074` BATTLE/BATTLE.EMI#3 |  | `00126974` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 228 | `801de0c0` BATTLE/BATTLE.EMI#3 |  | `001269b0` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 229 | `801e42c0` BATTLE/BATTLE.EMI#3 |  | `0013f744` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 230 | `801e4794` BATTLE/BATTLE.EMI#3 |  | `0013fb74` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 231 | `801e63f4` BATTLE/BATTLE.EMI#3 |  | `001449e0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 232 | `801e6538` BATTLE/BATTLE.EMI#3 |  | `00144b14` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 233 | `801ef78c` BMAGIC/MAGIC001.EMI#0 |  | `00150ac4` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 234 | `801f014c` BMAGIC/MAGIC001.EMI#0 |  | `001512f4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 235 | `801eec78` BMAGIC/MAGIC002.EMI#0 |  | `0015165c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 236 | `801ef1fc` BMAGIC/MAGIC002.EMI#0 |  | `00151990` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 237 | `801efd4c` BMAGIC/MAGIC003.EMI#3 |  | `00153228` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 238 | `801f03f0` BMAGIC/MAGIC003.EMI#3 |  | `001535b8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 239 | `801ef490` BMAGIC/MAGIC004.EMI#3 |  | `001544ac` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 240 | `801efaf4` BMAGIC/MAGIC004.EMI#3 |  | `001549b8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 241 | `801efad4` BMAGIC/MAGIC008.EMI#0 |  | `0015cff8` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 242 | `801f05fc` BMAGIC/MAGIC008.EMI#0 |  | `0015d9cc` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 243 | `801ef2b4` BMAGIC/MAGIC015.EMI#0 |  | `00159698` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 244 | `801ef6b0` BMAGIC/MAGIC015.EMI#0 |  | `00159a00` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 245 | `801ef348` BMAGIC/MAGIC021.EMI#3 |  | `0015462c` | widescreen | read | a full-screen rectangle x 0..319 on the PSX, -32..351 (width 0x15F) on the PSP: psp-widescreen.md 3a item 6; the 12 is the draw-mode packet |
| 246 | `801ef134` BMAGIC/MAGIC039.EMI#3 |  | `00161a9c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 247 | `801eee14` BMAGIC/MAGIC041.EMI#3 |  | `00163958` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 248 | `801eeef0` BMAGIC/MAGIC043.EMI#3 |  | `00165608` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 249 | `801efc48` BMAGIC/MAGIC046.EMI#3 |  | `00169480` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 250 | `801eec70` BMAGIC/MAGIC048.EMI#3 |  | `00169708` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 251 | `801eecbc` BMAGIC/MAGIC052.EMI#3 |  | `0016adf4` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 252 | `801ef4c0` BMAGIC/MAGIC056.EMI#3 |  | `0016ed58` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 253 | `801ef134` BMAGIC/MAGIC057.EMI#3 |  | `0016f940` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 254 | `801eed0c` BMAGIC/MAGIC059.EMI#3 |  | `00171c88` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 255 | `801f0544` BMAGIC/MAGIC059.EMI#3 |  | `00171b9c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 256 | `801ef2dc` BMAGIC/MAGIC060.EMI#0 |  | `00172fa0` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 257 | `801ef8e4` BMAGIC/MAGIC060.EMI#0 |  | `001732fc` | encoding | pattern | colour arithmetic of an effect (0xFF clamp by min / max on the PSP) |
| 258 | `801f07c8` BMAGIC/MAGIC060.EMI#0 |  | `00173a78` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 259 | `801ef8f4` BMAGIC/MAGIC064.EMI#3 |  | `00178b7c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 260 | `801f04a0` BMAGIC/MAGIC064.EMI#3 |  | `00178318` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 261 | `801eeff8` BMAGIC/MAGIC066.EMI#3 |  | `0017a078` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 262 | `801ef2dc` BMAGIC/MAGIC073.EMI#3 |  | `00181eac` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 263 | `801ef1dc` BMAGIC/MAGIC083.EMI#3 |  | `0018b300` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 264 | `801eee44` BMAGIC/MAGIC086.EMI#3 |  | `0018cff8` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 265 | `801eed04` BMAGIC/MAGIC087.EMI#3 |  | `0018ec34` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 266 | `801eeef4` BMAGIC/MAGIC088.EMI#3 |  | `0018f67c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 267 | `801ef1f0` BMAGIC/MAGIC092.EMI#3 |  | `001917d4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 268 | `801eed04` BMAGIC/MAGIC093.EMI#3 |  | `001939b4` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 269 | `801ef108` BMAGIC/MAGIC093.EMI#3 |  | `00192e50` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 270 | `801eef7c` BMAGIC/MAGIC094.EMI#3 |  | `0019474c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 271 | `801eee60` BMAGIC/MAGIC095.EMI#3 |  | `00195228` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 272 | `801ef1fc` BMAGIC/MAGIC095.EMI#3 |  | `00195648` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 273 | `801ef1a4` BMAGIC/MAGIC097.EMI#3 |  | `00197e58` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 274 | `801ef1b4` BMAGIC/MAGIC098.EMI#3 |  | `00199150` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 275 | `801eec88` BMAGIC/MAGIC099.EMI#3 |  | `0019a23c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 276 | `801ef1dc` BMAGIC/MAGIC100.EMI#3 |  | `0019c08c` | encoding | pattern | a range test (0x401..0xBFF) as row 158 |
| 277 | `801ef2d4` BMAGIC/MAGIC108.EMI#3 |  | `001a5f38` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 278 | `801ef148` BMAGIC/MAGIC110.EMI#3 |  | `001a7e2c` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 279 | `801eec7c` BMAGIC/MAGIC111.EMI#3 |  | `001a8770` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 280 | `801ef330` BMAGIC/MAGIC111.EMI#3 |  | `001a8f10` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 281 | `801ef970` BMAGIC/MAGIC111.EMI#3 |  | `001a8e30` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 282 | `801ef280` BMAGIC/MAGIC114.EMI#3 |  | `001abc70` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 283 | `801efa80` BMAGIC/MAGIC114.EMI#3 |  | `001ac048` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 284 | `801f0004` BMAGIC/MAGIC114.EMI#3 |  | `001ac368` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 285 | `801eec90` BMAGIC/MAGIC117.EMI#0 |  | `001ae0fc` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 286 | `801ef06c` BMAGIC/MAGIC117.EMI#0 |  | `00157050` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 287 | `801ef44c` BMAGIC/MAGIC120.EMI#3 |  | `001b0684` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 288 | `801efd20` BMAGIC/MAGIC120.EMI#3 |  | `001af9b8` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 289 | `801f0274` BMAGIC/MAGIC120.EMI#3 |  | `001afe28` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 290 | `801f08ac` BMAGIC/MAGIC120.EMI#3 |  | `001b00e4` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 291 | `801efba8` BMAGIC/MAGIC122.EMI#3 |  | `001b3b98` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 292 | `801ef960` BMAGIC/MAGIC125.EMI#3 |  | `001b6f88` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 293 | `801ef000` BMAGIC/MAGIC131.EMI#3 |  | `001bc554` | encoding | pattern | only globals or field offsets differ (the data map, base + offset addressing) |
| 294 | `801f030c` BMAGIC/MAGIC131.EMI#3 |  | `001bd620` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 295 | `801eef78` BMAGIC/MAGIC143.EMI#3 |  | `001c18d4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 296 | `801eed04` BMAGIC/MAGIC144.EMI#3 |  | `001c26bc` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 297 | `801ef078` BMAGIC/MAGIC144.EMI#3 |  | `001c29bc` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 298 | `801eecb0` BMAGIC/MAGIC151.EMI#3 |  | `001c5b0c` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 299 | `801ef104` BMAGIC/MAGIC154.EMI#3 |  | `001c8ed4` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 300 | `801eec84` BMAGIC/MAGIC161.EMI#3 |  | `001ca504` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 301 | `801ef1dc` BMAGIC/MAGIC161.EMI#3 |  | `001ca990` | widescreen | pattern | the full-screen rectangle widened to -32..351 / 383 as row 245 |
| 302 | `801eefa0` BMAGIC/MAGIC162.EMI#3 |  | `00174504` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 303 | `801eee88` BMAGIC/MAGIC167.EMI#3 |  | `00184924` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 304 | `801ef444` BMAGIC/MAGIC169.EMI#3 |  | `001cf6b4` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 305 | `801ef4ec` BMAGIC/MAGIC173.EMI#3 |  | `001d2cfc` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 306 | `801ef23c` BMAGIC/MAGIC213.EMI#3 |  | `001d3dd0` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 307 | `801eecdc` BMAGIC/MAGIC216.EMI#0 |  | `001d42b4` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 308 | `801eed18` BMAGIC/MAGIC223.EMI#3 |  | `001d9a7c` | encoding | scan | the same compare set on both sides (compare scan); the differences are offsets, struct copies, packet sizes or folded indices |
| 309 | `800c1dd4` BOSS/BOSS002.EMI#16 |  | `001dfa00` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 310 | `800c2f74` BOSS/BOSS025.EMI#13 |  | `001e62e0` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair same |
| 311 | `800c18e4` BOSS/BOSS033.EMI#16 |  | `001e8534` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 312 | `800c1db0` BOSS/BOSS033.EMI#16 |  | `001e8840` | encoding | rerun | the delay-slot, resident-module and reciprocal fixes of this round make the pair minor |
| 313 | `800c1f60` BOSS/BOSS034.EMI#16 |  | `001e9730` | encoding | read | one case of a six-case jump-table switch (head 0x800c1f30); that case reads the same (flags, the 0x40 stores, two calls of 0x80165bcc / PSP 0xb8480); the PSP's other cases not compared one by one |
| 314 | `800c1bb4` BOSS/BOSS035.EMI#16 |  | `001ea244` | encoding | pattern | one case run of a jump-table switch (head 0x800c1b84); not every case compared |
| 315 | `800c1900` BOSS/BOSS051.EMI#16 |  | `001ebf00` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 316 | `800c1b14` BOSS/BOSS051.EMI#16 |  | `001ec154` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |
| 317 | `800c1df8` BOSS/BOSS052.EMI#16 |  | `001ec778` | encoding | pattern | the enemy-index fold, reciprocal division, struct copy count or PSX-RAM address of rows 3, 21, 50, 90, 199 |

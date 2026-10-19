# The install-gap round, group CW: sources for the open live rows

**Status:** MEASURED (2026-10-18, branch `install-gap/cw` off `beba2d60`, docs
only). The two `gap` rows RW found read in play while zero, each given a
source on the player's disc by the PSX-twin method, and the rows checked
through the regeneration chain in this worktree with local edits to
`tools/psp_exe.py` that were then reverted (the rows are PT's to write). RW's
`language` rows read by reading their readers and RW's own run files; no game
was run. Nothing regenerated is committed.

Group CW of [`install-gap-round.md`](install-gap-round.md) 3a. The rows come
from [`install-gap-reads.md`](install-gap-reads.md) 0 and 3: of the ten read
while zero, two had no source after stage A - `ChlorineCloud_Offsets` and the
PSPs' `Char_WhelpSlot+0x3`. The method is [`psx-twin-check.md`](psx-twin-check.md)'s.
Addresses, sizes, counts and symbol names only (CLAUDE.md rule 1).

## 0. What came out

1. **`ChlorineCloud_Offsets` `0x65A608..0x65A62C` is a twin row**, every build:
   `BMAGIC/MAGIC009.EMI` section 3 holds the 36 bytes at `0x801EF8B0`
   (section offset `0xCB0`, the section's last 36 bytes) on all five PSX discs
   and in both PSP discs' copies of the file, equal to `BOF3.exe`'s. The
   twin of `ChlorineCloud_Start` is `0x801EF198` in that section, and it reads
   the table there with `lui $at, 0x801F` and `lw -0x750 / -0x74C / -0x748($at)`.
   **The PSX holds the PC's values**: no regional difference, no finding (1).
2. **`Char_WhelpSlot+0x3` `0x669739..0x66973C` (PSP) is a `BOOT.BIN` twin
   of the whole member-flag table**, `0x669738..0x669743`. The PSP's own
   `Field_MemberSprite` (psp-jp `0xD7B74`, psp-eu `0xD788C`) reads it at
   `BOOT.BIN` `0x2E5497` / `0x2E4CAF` + member, one byte below where the map
   put it. The three gap bytes alone match 503 places in `BOOT.BIN`. The
   11-byte table matches one place that a relocated pair reads, so
   `boot_place` takes it. **PB's "no safe twin" was about the run's length,
   not the source** (2).
3. **The `language` rows RW counted as read before a write are written on
   every Western install.** The watch recorded no write anywhere on page
   `0x66A000`, in any of its 77 runs. On that page the Western layer writes
   in place after the watch arms: the run logs show `DIV-0018: 22 menu verbs`,
   the zenny unit, the gene tabs, the status words and the menu stats. The
   reads come hundreds of frames later. The A/B/C sweep of 2026-10-13 compared
   PC+en-US with PSX-US disc-only on `menu_screens`, `field_menu` and
   `dragonTransform`, and saw no difference in these words. So on a Western
   disc-only install **these are not visible defects**. Under `ja-JP`
   (psx-jp, psp-jp) nothing writes them: there they are **D280** (blank) and,
   for the fishing label and lines, **D279** (glyph `0xFFDA`) - visible, known,
   and the owner's call (3).

## 1. `ChlorineCloud_Offsets`

**The PC side.** `ChlorineCloud_Start` `0x49DAD0` (ours in `magic_s03.cpp`;
[`magic_s03.md`](magic_s03.md) 1, 4) reads three `(dx, dz, dy)` dword triples
at `0x65A608 + 12 * +0xB`. `ChlorineChild_Kinds` `0x65A5F4` (2 words) and
`ChlorineCloud_Steps` `0x65A5FC` (3 words) come just before the table, and
`BlitzBolt_TaskTable` `0x65A62C` just after it. On every disc-built image the
36 bytes are a `gap` (`recipes/exe-gaps.tsv`, `game-value`, all seven builds), so
each of Chlorine's three clouds starts at the source with no offset. RW saw
the read on psx-jp, psx-us, psx-eu-en and psp-eu (`cutsceneAndNue`, the Nue
fight, area 0x17).

**The twin.** MAGIC009 is Chlorine's overlay (`magic_s03.md`'s table). On the
PSX it is `BIN/BMAGIC/MAGIC009.EMI`. Its section 3 is the code and data
(`emi.py list`: `0x801EEC00`, 3,284 bytes). The five PSX files differ from
each other (from byte 102 on), but each section 3 holds `BOF3.exe`'s 36 bytes
at section offset `0xCB0` (a scratch search, `g_cw\s\chl.py`). The words
before them have the PC's layout, as code pointers into this section:

| PSX-JP address | What | PC |
|---|---|---|
| `0x801EF89C` | two words: `0x801EF120` (the cloud's run), `0x801EF6B0` (the copy's task) | `ChlorineChild_Kinds` `0x65A5F4` |
| `0x801EF8A4` | three words: `0x801EF198`, `0x801EF2D4`, `0x801EF340` | `ChlorineCloud_Steps` `0x65A5FC` (`_Start`, `_Grow`, MAGIC040's `0x4A5D50`) |
| `0x801EF8B0` | the 36 bytes | `ChlorineCloud_Offsets` `0x65A608` |

The steps table's first entry is the twin of `ChlorineCloud_Start`:
`0x801EF198..0x801EF2D4`, read to its `jr $ra` (`0x801EF2CC`). Its order of
operations is the PC's:

- `+9` is counted down, and the function returns while it is non-zero.
- The source's `+8` is copied.
- The offset index `+0xB` is scaled by 12 (`sll 1; addu; sll 2`) and added to
  `lui $at, 0x801F`.
- dx is loaded by `lw -0x750($at)` (`0x801EF8B0`) into `+0xC`, and dz by
  `lw -0x74C($at)` into `+0x10`.
- The turn is called (`jal 0x801DD318`, the PC's `0x446770`).
- `+0x34` and `+0x38` are set to the source's plus the offsets.
- dy is loaded by `lw -0x748($at)` and added to the source's `+0x3C`.
- `jal 0x800AC68C` (`BattleActor_UpdateScreenXY`).
- `+0xA` is set to 0 and `+2` counts on.

The third step entry is local on the PSX (`0x801EF340`, inside the section).
On the PC it is MAGIC040's `0x4A5D50`, the shared body `magic_s03.md` 3
describes. That entry is a code pointer, not part of this row.

**The PSP.** Both PSP discs carry `BMAGIC/MAGIC009.EMI`. The twin rule finds the
run there at the same place (section 3, `0x801EF8B0`, 0 bytes not
`BOF3.exe`'s). `BOOT.BIN` holds the 36 bytes three times, and a relocated
pair reads each copy (`boot_place`: "3 copies, 3 read"), so a `BOOT.BIN` row
would be refused as ambiguous. All three copies equal the PC's bytes,
so the PSP's native code reads the same values whichever copy is Chlorine's.
MindSword_BladeRadii (`0x65A5EC`, MAGIC006) already takes the overlay file on
both PSPs (`TWINS`); this row does the same.

**Which kind.** It is a run in the effect's overlay, so it is a `TWINS` row.
An `immref` would also fit: the twin's `lui` and `lw` address the bytes in
their own section. But `ChlorineCloud_Start` has no `psx` field, so OT's
search had no twin for it, and the load is indexed (`addu $at, $at, $v0`
between the `lui` and the `lw`). A `TWINS` row takes precedence over
`exe-imm.tsv` anyway (`exe-immediate-places.md` 1.1). It is not computed: the
values are a table.

**Not a finding.** The PSX values equal the PC's on every disc. Rule 6 is
not engaged and no ledger entry is needed: this is the disc giving the PC's
own bytes back, DIV-0101's class.

**Checked** in this worktree, with the row added to `TWINS` and then
reverted (section 4): `psp_exe.py recipe` wrote
`twin 0x65A608..0x65A62C from BMAGIC/MAGIC009.EMI#3 0x801EF8B0: 36 bytes, 0 not
BOF3.exe's` for each of the seven builds.

## 2. `Char_WhelpSlot+0x3` (PSP)

**The PC side.** `Field_MemberSprite` `0x533BA0` (psx `0x801C224C`;
`symbols.toml`, `field_event.cpp`) sets up a member's sprite. Among other
fields it writes `+0x2B = 0x669738[member]` and the low byte of `+0x70 =
(+0x2B != 0)`. `field_event_callees.h` calls the table `kMemberFlags` and
`event_ops_callees.h` calls it `kMemberSlot`: a byte per member id, eleven
used. `Char_WhelpSlot` `0x669736` is the byte before it, with one pad byte at
`0x669737`. The gap row is the table's entries 1..3, three bytes, one of them
non-zero on the PC. With it zero, that member's sprite gets flag 0. What that
looks like in play is not measured. RW saw the read on six routes on both PSPs.
The PSX builds place the whole stretch from the boot EXE (`exe_maps`: one run,
`0x66758D..0x669848`).

**The PSP's reader.** `BOOT.BIN`'s relocated pairs into the table's
neighbourhood (`Elf.text_refs`, `g_cw\s\whelp.py`) name two readers of
`0x2E5497` on psp-jp (`0xCD1AC`, `0xD7B74`) and two of `0x2E4CAF` on psp-eu
(`0xCCEC0`, `0xD788C`). `0xD7B74` is in the PSP's `Field_MemberSprite`, read
from `0xD7B14` (`g_cw\s\whelp2.py`, capstone). The stores to `+0x24` (1),
`+0x27` (slot + 0x78), `+0x28` (2), `+0x25` (5) and `+0x26` (slot * 0x50) are
the PC's. Then `lui $a0, 0x2E; addiu $a0, $a0, 0x5497; addu` the member;
`lbu`; `sb` to `+0x2B`; and `+0x70`'s low byte from `sltu $zero, +0x2B`. So
the PSP's table starts at `0x2E5497`, right after the PSP's `Char_WhelpSlot`
byte: the PSP has no pad byte. The table is 11 bytes there (the next table,
read at `0x2E54A2`, follows at once), against 12 on the PC.

**Why the map left three bytes.** `exe_maps/psp-*.tsv` carries the
displacement from before the table through `0x669739`, which puts PC
`0x669738` at the PSP's entry 1, not entry 0. Its next segment starts at
`0x66973C` with the displacement of the table after it. The bytes in both
segments are equal (zeros), but they are one entry off. The three bytes
between, PC entries 1..3, differ at that displacement, so the map stops there.

**The twin.** `boot_place` on the gap's three bytes: 503 copies, 15 read by a
relocated pair - refused. On `0x669738..0x66973C` (4 bytes): 122 copies, 8
read - refused. On the whole table `0x669738..0x669743` (11 bytes): **one copy
read, at `0x2E5497` (psp-jp) / `0x2E4CAF` (psp-eu), by the two readers
above**. On `0x669738..0x669744` (with the PC's pad byte): 0 read.
`g_cw\s\probe_bp.py` holds the probe. So the source is the whole 11-byte
table. The row overlaps the map's bytes at `0x669738` and
`0x66973C..0x669743`, which are equal, and puts them at the displacement the
PSP's own reader uses.

**`+0x4` against `+0x3`.** The 1013 table ([`exe-image-twins-1013.md`](exe-image-twins-1013.md) 6)
listed `Char_WhelpSlot+0x4`, one byte, "no safe twin". Today's gap is
`+0x3`, three bytes, because the map changed since. Neither run is safe as
a byte search, at 1 byte or at 3. The safe twin is the table from its base,
read by the reader's own pair. **The PSP's table equals the PC's in all
eleven entries**: no regional difference.

**Checked** as in section 1: `psp-jp twin 0x669738..0x669743: BOOT.BIN
0x2E5497, read at 0xCD1AC 0xD7B74`; psp-eu `0x2E4CAF`, `0xCCEC0 0xD788C`; and
`11 bytes, 0 not BOF3.exe's` on both.

## 3. The `language` rows read "before a write"

RW ([`install-gap-reads.md`](install-gap-reads.md) 0 item 2, 3) lists three
`language` ranges read before any write on all seven installs:
`Menu_Verbs`, `Menu_Verbs+0xF4` and `CommuName_RecordNames+0x144`. It lists
five more on the two Japanese builds. Answered here by the readers, the
layers' contents and RW's own files.

### 3.1 The Western installs: written, and the watch did not see it

- **The watch saw no write on page `0x66A000`.** A tally of RW's 77 files
  (`g_rw\gw`, `g_cw\s\gwpage.py`) gives 0 writes on any range of that page on
  any build, against 165 on page `0x669000` on each Western build
  (`Battle_CommandLabels` and the name slots, written by the layer's
  `memcpy`). The page is the busiest in the watch: 524 of 1,740 latches
  parked on psx-us `menu_screens` (`gapwatch.py pages`). The debug registers
  hold only `gap` words, so a `language` write on a parked page is not
  recorded. The same layer load also changes page `0x66A000`'s protection:
  `PauseText`'s four 4-byte patches at `0x66A418..0x66A44C` go through
  `VirtualProtect` (`detour.cpp`). Which of the two hid the writes is not
  measured.
- **The layer writes them.** The run log of RW's psx-us attract
  (`g_rw\runs\cost_on_attract\psx-us\bof3x.log`) has `gapwatch: 324 ranges ...
  guarded` and then, at the first `FIRST.DAT` from the en-US layer:
  `DIV-0018: 22 menu verbs`, `DIV-0064: 2 of 2 status words`, `4 of 4 menu stats`,
  `5 of 5 item types`, `1 of 1 zenny unit`, `3 of 3 gene tabs` (and the rest
  of the groups). The en-US and en-150 layers carry kinds 8 and 15 (a scan of
  `c_psxus\loc` and `c_pspeu\loc`). The first reads RW gives come hundreds of
  frames later: psx-us `menu_screens` frames 1,108..1,662.
- **The readers read what the layer wrote.** RW's report, split by build
  (`gapwatch.py report` on each build's files):

| Row | Reader on psx-us, psp-eu | The bytes it reads | Written by |
|---|---|---|---|
| `Menu_Verbs` `0x66A228..0x66A2FC` | `Text_CharCount`, `Text_DrawString` from `Menu_DrawButtonRow` `0x574890` and `MenuList_ButtonRow` | the 22 verb slots through `0x6637E4` | DIV-0018 kind 8, in place (`ApplySlots`: the slot cleared, the string copied) |
| `Menu_Verbs+0xF4` `0x66A31C..0x66A32C` | `Text_DrawString` through `Crt_sprintf` | the zenny unit `0x66A31C`, a `%s` | DIV-0064 group 11, in place |
| `CommuName_RecordNames+0x144` `0x66A0E8..0x66A164` | `Text_DrawString` from `GeneWin_DrawChoices`, `Menu_DrawBox`, `Text_DrawAt` | the gene tabs `0x66A14C..` (through `0x66A164`), the status words `0x66A0E8`, the menu stats `0x66A0F8` | DIV-0064 groups 12, 1, 2, in place |

  The rest of each range is either repointed (group 3, the item types at
  `0x66A120`: the tables name our buffers) or read by nothing these readers
  reach. That covers the six glyph strings `0x66A2E0..` behind `0x66A2FC`
  (under a Latin layer the Config cell and the fishing label draw DIV-0051's
  icons) and `0x66A320..0x66A32C` (behind `0x66A32C`, kind 0x0F's grid text,
  on no route).
- **The capture already exists.** The A/B/C sweep of the `us` set
  ([`abc-sweep-1013-us.md`](abc-sweep-1013-us.md) 0) compared PC+en-US with
  PSX-US disc-only, frame by frame, on `menu_screens`, `field_menu` and
  `dragonTransform`. The only differences in those screens were the recipe
  save's name glyph and a colon's blink. The verbs, the money unit, the
  stats and the gene tabs drew the same. No new capture was taken.

**Verdict, Western (psx-us, psx-eu-en, psx-fr, psx-de, psp-eu):** not a
defect. RW's "before a write" for these three rows is the watch's blind spot on
page `0x66A000`. For I43 step 4 they should be read as written, not as zero.
The psx-eu-en, psx-fr and psx-de layers carry kinds 7..12, the 19 kind-15
chunks and kind 16 as en-US does (`c_psx*\loc`). The French and German
words are LA's ([`install-gap-labels.md`](install-gap-labels.md)).

### 3.2 The Japanese installs: nothing writes them

The `ja-JP` layers carry kinds 5, 6, 13 and 14, and on psx-jp one kind-15
chunk (tag 15, the Identify words); psp-jp has no kind 15 (`c_psxjp\loc`,
`c_pspjp\loc`). Neither has kinds 7..12 or kind 15's other groups (D280:
`loc_build.py` builds those for Latin donors only). So under `ja-JP` the slots stay zero, and each reader draws an empty
string, or for kind 0x0F a NUL as a glyph:

| Row | Reader (psx-jp, psp-jp) | Draws | Defect |
|---|---|---|---|
| `Menu_Verbs` | `Menu_DrawButtonRow` / `MenuList_ButtonRow`; and `EffectKind0F_DrawGlyph` from `EffectKind0F_LineNext` (the fishing banner's label, the one-character string behind `0x66A2FC`) | four blank buttons; the label's NUL becomes glyph `0 - 0x26 = 0xFFDA` (`effect_1b.cpp`'s one-byte branch) | D280 (captured, [`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 3.2); the label is D279's glyph by a second path |
| `Menu_Verbs+0xF4` | `Crt_sprintf` `%s` | no unit after the money | D280 (captured: `field_menu`, blank) |
| `CommuName_RecordNames+0x144` | `GeneWin_DrawChoices`, `BattleMenu_DrawItemList`, `Menu_DrawBox` | blank gene tabs, item-list title, stat labels | D280 (captured) |
| `Battle_CommandLabels` `0x669D28..0x669D60` | `BattleWin_DrawCommandLabel` | a blank label box | D280 (captured) |
| `Battle_CommandLabelPointers+0x1C` `0x669D7C..0x669DE0` | `BattleBanner_ShowName`, `Str_CopyN` from `BattleBanner_SetMessage`, `BattleTarget_ShowPrompt` | an empty battle banner (the kind 12 messages, DIV-0052) | D280's scope (kinds 7..12); the banner itself not in its captured table |
| `CommuName_RecordNames+0x1C` `0x669FC0..0x66A088` | `EffectKind0F_DrawGlyph` from `EffectKind0F_LineType` | the fishing lines' NUL as glyph `0xFFDA` | **D279** (garbage; a PSP-JP crash one run in five) |
| `CommuName_RecordNames+0x1CC` `0x66A170..0x66A228` (psx-jp) | `BattleMenu_DrawSkillList` | a blank skill-type title in battle | D280 (captured) |
| `Encounter_SlotChance+0x24` `0x669CD4..0x669CE8` | `FieldPanel_DrawKindRow` from `Field_MembersFrame` | the name slots `0x669CD8` (the fish merchant) and `0x669CE0` (the whelp's reset name), blank | D280's scope (kinds 10, 11; the Western layer's `CharNames_ApplyMerchant` / `CharNames_Apply` write them) |

All of these are visible on a Japanese disc-only install, all are known, and all
wait on the owner's call in D280 (a `ja-JP` layer of kinds 7..16 from the Japan
disc). The one addition is a second path to D279's glyph `0xFFDA`: the fishing
label's one-character string under `ja-JP`, as well as its lines. D279's fix
should cover both.

## 4. Checks run

- `g_cw\s\regen.py` is `g_coord\regen.sh`'s chain in Python, run in this
  worktree with the two rows added to `TWINS`: `psp_exe.py recipe`,
  `exe_tables.py recipe`, `exe_own.py measure` (with `analysis/pc_xref.json`
  copied in), `exe_tables.py gaps`, `exe_tables.py recipe` again, then the
  three checks. All ended 0:
  - `exe_tables check`: 0 errors.
  - `psp_exe check`: OK.
  - `importer check`: 742 files, 3,582 chunks, 0 errors.

  Against the base's committed recipes:
  - `exe-gaps.tsv` loses exactly the nine `gap` rows these two rows close
    (647 to 638): `ChlorineCloud_Offsets` on all seven builds and
    `Char_WhelpSlot+0x3` on both PSPs.
  - `exe-psp.tsv` gains nine `twin` rows.
  - `exe-layout.tsv` loses one `number` word, `0x65A61C`, inside the table
    (the twin now fills it).
  - `exe.toml`'s hashes and counts move with these changes.

  No other recipe changed.
- The edit to `tools/psp_exe.py` and every regenerated recipe were reverted
  (`git checkout -- tools/psp_exe.py recipes/`). The branch's one commit is
  this document and its `README.md` row.
- Scratch, all game-derived output under `C:\Users\kerok\g_cw`: `x\<build>\MAGIC009.EMI`
  and its disassembly, the probes in `s\`, `regen1.log`.

## 5. Handed on

- **PT** (`tools/psp_exe.py` `TWINS`), two rows at the end of the list:

  ```python
      # CW (docs/install-gap-live-rows.md): the two live rows RW found read while zero.
      (0x65A608, 0x65A62C, "BMAGIC/MAGIC009.EMI", "ChlorineCloud_Offsets (ChlorineCloud_Start's twin 0x801EF198 reads it at 0x801EF8B0)"),
      (0x669738, 0x669743, "BOOT.BIN", "Char_WhelpSlot+0x2: the member flag bytes (Field_MemberSprite's kMemberFlags, to sprite +0x2B)", PSP),
  ```

  The second row starts at the table's base, not at the gap, on purpose (2).
- **FP** (optional, not needed for the fill): `ChlorineCloud_Start` `0x49DAD0`
  `psx = 0x801EF198` in `BMAGIC/MAGIC009.EMI` section 3. The pairing is
  evidenced by the steps table at `0x801EF8A4`, whose entry 0 it is, and by
  the order of operations in section 1. It is a pair for `symbols.toml`. It
  is not a second source of the row, since `TWINS` takes precedence.
- **The coordinator:** after the merge, census 8: `ChlorineCloud_Offsets`
  NEEDS FIX P3 becomes fixed by `twin` (all 7); `Char_WhelpSlot` (PSP) becomes
  fixed by `twin` (`BOOT.BIN`). RW's three Western `language` rows are
  written, not zero (3.1). The JP rows are D280's and D279's, with D279 gaining
  the label path (3.2). No ledger draft: both rows are the disc's bytes equal
  to the PC's (DIV-0101's class), and no behaviour is chosen.
- **RW's instrument** (for whoever owns `gapwatch.cpp` next): a `language`
  range on a page parked or re-protected at the layer's load reports "read
  before a write" though written. A write-only debug register on the
  language ranges' first words, or the layer's `Log` lines joined to the
  file, would settle it per run.

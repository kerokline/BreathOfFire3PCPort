# The PSP code diff: the sibling's PSX functions paired with `BOOT.BIN`, and where they differ

**Status:** IN PROGRESS (2026-10-12) - stage A of group CD of the PSP-only round
([`psp-only-round.md`](psp-only-round.md) 3) done: the pairing tool, its
coverage and precision measured, the pairs diffed with the widescreen changes
and the platform layer set aside, and the differing pairs tabled by half
(section 7). Stage B (reading each differing pair: content, encoding or
platform) has not started. Read-only: nothing in `src/` or the importer
changed. Nothing here has been seen in play.

## 0. The answer so far

- **The pairing works across the two compilers.** `tools/psp_pair.py pair`
  pairs **9,624 of 13,134** PSX functions (73.3 %) with PSP-JP, and **139 of
  the 175** functions the sibling names as game code (79.4 %). The 15 pairs
  that [`psp-stallion.md`](psp-stallion.md) 3.1 and
  [`psp-widescreen.md`](psp-widescreen.md) 3 established by hand come out the
  same, on JP and on EU (section 3).
- **The diff, after normalisation,** puts 8,345 pair classes in five verdicts
  (section 5): 5,409 `same`, 1,041 `minor` (one scored difference), **1,225
  `differs`** (317 battle and effects, 908 field, menus, items and system), 34
  `widescreen`, 636 `platform`. `differs` is a list for stage B, not a finding:
  the normaliser removes the compilers' known habits, and what is left still
  mixes content with encoding (section 4.3).
- **The priority consumers** (section 6): no consumer of the priority data
  shows a content difference that stage A can name. Item 87's special case
  (a test of the item id against 87, then of a call's result against `0xAE`) is
  in both builds. `exp_table`'s six paired readers: `Char_LevelUp` `minor`,
  two `same`, one `platform`, and two `BATL_END` readers `differs` at scores 2
  and 3, on PSP-side constants that look like structure offsets. The
  consumables' twelve: four `differs` at scores 2 to 4. **`RYUD`'s readers and
  the button-label page's readers are not found by this method**: no PSX
  function names either load address, so they read through a pointer or a
  texture page that stage B must follow. The arenas' load area has two
  paired readers, both `minor`.

## 1. Sources and commands

| Input | Where |
|---|---|
| PSX code | the sibling's `disc/SLPS_009.90` (JP boot EXE) and `analysis/overlay_captures_all.json` (405 overlay sections, the logo EXE left out) |
| PSX names | the sibling's `symbols.toml` (556, all boot EXE) and `names/functions.toml` (121, in 8 overlays): 677, the corpus `overlay-transfer-feasibility.md` counts |
| PSX starts | the sibling's `analysis/functions.tsv` (boot EXE), the captures' header and engine entry lists, jal targets, stored prologue pointers (the tool's docstring) |
| PSP code | `PSP_GAME/SYSDIR/BOOT.BIN` read from the player's `(PSP) (Japan).iso` and `(PSP) (Europe).iso` through `tools/psx_disc.py`; its `.rel.*` sections (type `0x700000A0`) say which instruction halves are addresses |

```
python tools/psp_pair.py pair --psp-jp "CDImage/Breath of Fire III (PSP) (Japan).iso" \
    --psp-eu "CDImage/Breath of Fire III (PSP) (Europe).iso" --check-eu --out <scratch>/pairs.json
python tools/psp_pair.py diff --psp-jp ... --psp-eu ... --out <scratch>/diff.json --md <scratch>/table.md
```

`pair` takes about four minutes with `--check-eu`, `diff` about two and a
half. Both write addresses, counts, constants and the sibling's names only
(rule 1); the JSON stays in scratch. Section 7 is `diff --md`'s output as it
stood at this doc's commit.

## 2. The pairing

The PSP's ELF is a native compile of the PSX boot EXE and every overlay by
another compiler (`psp-stallion.md` 3.1), so no PSX word matches a PSP word as
it stands: registers, delay slots, `li` forms (`addiu` against `ori`), constant
reloads and multiplications (the PSX shifts and adds) all differ. Each function
is reduced to what a compiler keeps: its non-address constants, its field
offsets, its callees and code pointers in order of first use, and the data
addresses it touches. The matcher runs in rounds until nothing moves:

1. **seed**: a unique, equal multiset of constants and field offsets (766 pairs);
2. **data map**: paired functions' global lists aligned, two agreeing votes
   (a PSX address to a PSP `.data` / `.bss` address);
3. **callee alignment**: a pair's callee lists aligned through known pairs;
4. **caller vote**: an unpaired callee whose paired callers' twins all call
   one unpaired PSP function;
5. **gaps**: the PSP links each family of overlays (`BATTLE`, `BMAGIC`,
   `BOSS`, `ETC`, `PLCHAR`, `SCENARIO`, the `WORLD`s) in the PSX's file order
   (measured: the median PSP address of each file's pairs rises with the file
   name inside every family), so a family's pairs in that order are anchors;
   between two anchors, equal counts pair by position and others by an
   order-keeping alignment of the similarity matrix;
6. **similarity**: a rarity-weighted multiset similarity, mutual best with a
   margin; last, `similarity-weak` (0.3 and 1.3 times the runner-up), tagged
   in every table so a reader can weigh it.

Equal copies are one class on each side (the same words at the same load
address on the PSX; the same unrelocated words on the PSP): `AREA` overlays
repeat helpers, and 1,279 PSX functions are such copies. PSP-EU follows from
PSP-JP by unique relocation-masked words (7,283) and then by order between
them (13,178 of 13,627 in all).

### 2.1 Coverage (`pair`, 2026-10-12)

| | count | paired | |
|---|---|---|---|
| PSX functions (boot EXE and 405 captures) | 13,134 | 9,624 | 73.3 % |
| PSP-JP functions (jal targets, `.data` code pointers, the code after each return) | 13,627 | 8,345 | a twin of one PSX class (a PSX function and its copies) |
| sibling names, game code | 175 | **139** | 79.4 %; 24 of the 677 names are labels inside a function (the captures' case labels) and pair through it |
| sibling names, Psy-Q library and BIOS thunks | 502 | 72 | expected near zero, since the PSP replaces the library: these twins are mostly by caller vote (the PSP function every paired caller's twin calls), and the diff counts them as the PSP's platform layer (section 4) |

The 36 named game functions left unpaired: the memory card (`Card_*`, ten,
the PSP saves through its own utility), `Save_BuildImage` / `Save_Commit`,
the sound cue tables (`SE_CueSetup_Bank0..6`, `SE_PollKeyStatus`), the CD
layer (`File_LoadRequest`, `File_LBA`), `MsgBox_Render`,
`MsgBox_StateDispatch`, and twelve battle functions (`EnemyAI_TurnCheck`,
`EnemyAI_ChooseActions`, `Battle_BeginAction`, `Battle_ActorFrameUpdate`,
`Battle_FrameTask`, `Battle_State_Nop`, `HUD_GaugesTick`,
`BattleBanner_Hold`, `BattleBanner_SlideOut`, and three names that are no
function start here: `Examine_ActiveTick`, `Battle_MemberStatusTick`,
`Examine_EnemyMove_Begin`). Most of the first group is the platform layer
the PSP rewrote, so no twin is expected; the battle ones are unpaired, not
known to be absent.

## 3. Precision

- **The hand-made pairs.** `psp-stallion.md` 3.1's `BOSS024` table and
  `psp-widescreen.md` 3's `MapView_Build` (`0x80153b8c`, EU `0xd840`),
  `Actor_Task` (`0x8014c3c8`, EU `0x258c`) and the `0x80161e44` twin (EU
  `0x96884`): all 15 that the tool pairs come out as those docs have them, the
  EU addresses included (`BossStallion_Enter` `0x1E54F8` / EU `0x1E51D4`,
  `Boss24_Setup` `0x1E5694` / `0x1E5370`, `Boss24_End` `0x1E5708` /
  `0x1E53E4`, `Boss24_Exit` `0x1E576C` / `0x1E5448`, `Boss48_Setup`
  `0x1E5788` / `0x1E5464`, the hooks and bare returns between). Boss 48's end
  and exit stay unpaired. Checked with a scratch script over `pairs.json`.
- **Callee agreement.** Of the 3,303 pairs where both sides have two or more
  paired callees, 26 (0.8 %) share fewer than half of them; by method,
  `signature` 0 of 252, `gap-align` 6 of 1,259, `similarity` 6 of 983,
  `callee-align` 7 of 486, `gap-order` 5 of 243, `similarity-weak` 2 of 52,
  `caller-vote` 0 of 28. Those 26 are the likeliest mispairs.
- **JP against EU.** An independent PSX-to-PSP-EU pairing agrees with
  PSX-to-JP-to-EU on 9,379 of 9,383 functions (99.96 %). This is a
  consistency check, not a precision one: both runs share the method.
- **One mispair found and fixed by reading.** A first diff paired GAME.EMI's
  `0x801b0494` (the item-87 test) with PSP `0x9b540`, and reported the 87
  test missing on the PSP. The disassembly showed the PSX function had been
  run on into its neighbour: a jump table set the end-of-function rule aside.
  A prologue after a return now ends a function past a jump table, and the
  pair is `0x9a80c`, which holds the same test. A reader in stage B should
  treat a size flag (section 7's `size` note) as a reason to check the
  boundaries first.

## 4. The diff

### 4.1 What is compared

For each pair class (copies once), `diff_pair` compares, as sets:

- **constants**, after setting aside the trivial ones (`0`, `1`, `-1`, `2`,
  `3`, `4`, `8`, `0x10`, `0xFF`, `0xFFFF`), multipliers, a PSP constant that is
  a PSX RAM offset (the PSP keeps EMI sections at their PSX offsets in one
  buffer: `0x33800` is `0x80033800`), and the constant arguments of a platform
  call on either side; a constant matches its value plus or minus one, its
  negation, and its own low 16 bits (or 8, for a negative): `0xBF` and `-65`
  clear the same bit. Masks (`2^k - 1`) and powers of two are listed, never
  scored, because Allegrex's `ext`, `ins`, `seb`, `min` and `max` take them as
  fields or drop them;
- **callees**, paired ones as a set; a difference only in small callees (40
  instructions or fewer, which one compiler inlines) is listed as `inline`,
  not scored;
- **globals**, scored only when each side has a mapped address the other
  lacks;
- **field offsets**, listed, never scored: the PSP's base registers move them.

### 4.2 Verdicts

| verdict | rule |
|---|---|
| `same` | score 0 |
| `minor` | score 1 |
| `differs` | score 2 or more |
| `widescreen` | a PSP-added constant from `psp-widescreen.md` 3a (`-96`, `417`, `-231`, `552`, `-81`, `402`, `-32`, `384`, `352`, `113`, `258`) and otherwise score under 2, or with GTE use |
| `platform` | a PSX function using the GTE that differs, or a difference only in calls where one side calls the platform (the PSX's Psy-Q library from `0x80168C58` on; the PSP's system stubs, the library's PSP twins, and unpaired PSP functions calling into either) |

The 15 `BOSS024` pairs, which `psp-stallion.md` reads as identical, come out
14 `same` and 1 `minor` (`Boss24_End`: one unmapped global on each side).

### 4.3 What `differs` still holds

Stage A's normaliser is measured on its noise, not proved complete. The most
frequent constants still left in `differs` rows are small: on the PSX side
`-3` (77 rows), `12` (54), `320` (44), `5`, `-128`; on the PSP side `6`
(37), `0x650` (36), `14`, `56`, `32` (rows counted over `diff.json`'s detail
column).
They read as compare bounds and argument values the two compilers fold
differently, structure offsets the PSP adds to a base it already holds, and
PSX GPU-packet lengths the PSP builds through a call: a reading of a handful
of rows, not a measurement. The 44 rows that lose `320` without gaining a
widescreen constant are worth a look against `psp-widescreen.md` 3.
Rows with high scores also include function pairs of very different size
(flagged), where the PSP inlined callees or the boundaries differ. Expect
most `differs` rows to be encoding; the score orders them for reading.

## 5. The tallies (`diff`, 2026-10-12)

| half | same | minor | differs | widescreen | platform | pair classes |
|---|---|---|---|---|---|---|
| battle and effects (`BATTLE`, `BMAGIC`, `BOSS`; boot functions with a battle name or only battle callers) | 2,309 | 393 | **317** | 7 | 263 | 3,289 |
| field, menus, items and system (the rest) | 3,100 | 648 | **908** | 27 | 373 | 5,056 |

Of the 137 named game pairs: battle 47 `same`, 6 `minor`, **23 `differs`**,
9 `platform`; field 29 `same`, 9 `minor`, **13 `differs`**, 1 `platform`.
Of the 1,225 `differs`, 384 score 4 or more (102 battle, 282 field).

The `widescreen` rows are the 3a item 6 sites (`-32` with `384` or `352`
added, `320` removed) in the battle, magic, shop, scenario and area overlays,
the boot frame function (`0x8014f5c0`, EU `0x76d8`) and `MapView_Build`.

## 6. The priority consumers

The round asked first about the consumers of `RYUD`, Ryu's form data, the
arenas, the button-label page, `exp_table` and consumable 87
([`psp-only-round.md`](psp-only-round.md) 3 and 4). A consumer here is a
paired PSX function with a data address inside the item (PSX-JP addresses):

| data | PSX range | source of the range |
|---|---|---|
| `exp_table` | `0x801CC068`, 5,544 bytes (GAME.EMI) | `tables.toml` |
| consumables | `0x801C995C`, 92 records of 14 bytes (GAME.EMI); record 87 at `+0x4C2` | `tools/psp_stallion.py` `JP_TABLES` |
| `RYUD` form data | `0x8007280C`, `0x1B000` bytes (section 3 of `BPLCHAR/RYUD00`) | `region_diff.Build` sections, this round |
| the arenas' load area | `0x80033800` up to `RYUD`'s (sections 3 and 7 of `BPLD012`, `BRTD457`) | the same |
| the button-label page | the type-3 image section at `0x1A080200` (BATTLE.EMI #4): a VRAM image, read by texture page, not by address | `region_diff.Build` |

Findings:

- **Consumable 87.** GAME.EMI `0x801b0494` (PSP-JP `0x9a80c`, EU `0x9a588`)
  tests the record's byte `+8` bit 0, then the item id against 87, then
  the result of `0x80167628` on two position words against `0xAE`; the PSP
  twin keeps all three (`0x9a8b0`..`0x9a8d8`: `andi 1`, `ori 0x57`,
  `ori 0xae`), and the diff says `same`. The PSP's data
  change to item 87 (one flag bit, `exe-import.md` 3) therefore changes which
  branch this code takes, on both builds alike; our code running on the PSP's
  record does what the PSP does here. Not verified: whether any other reader
  tests id 87 without referencing the table.
- **`exp_table`.** Six paired readers: `Char_LevelUp` (`minor`: globals only),
  SHOP's `0x801daa30` and BATL_END's `0x801ef92c` (`same`), BATL_END's
  `0x801f0a78` (`platform`), and BATL_END's `0x801ef144` and `0x801efe60`
  (`differs`, scores 2 and 3: the PSP adds constants `0x7608`, `0x7644`,
  `0x7648`, `256` and one call). Stage B reads those two first. The PSP's
  change is to the table's data (roster index 6), which no code change is
  needed to read.
- **Consumables.** Twelve paired readers: seven `same`, one `minor`, four
  `differs` at scores 2 to 4 (`COMMU00` `0x801efa50`, `COMMU04` `0x801d12c8`,
  `START` `0x801d31a8`, `AREA030` `0x801e105c`).
- **`RYUD` and the button-label page: no reader found by address.** No PSX
  function forms an address in `RYUD`'s range or loads the image's page by
  address (`0x80070000..0x80090000` searched: only fixed-point constants and
  sound-library sizes). The form data is reached through a pointer set when
  the section loads; the page through a texture-page word. Stage B finds them
  from the load (`File_LoadRequest` with the `RYUD` file ids, or the section
  table) and from the battle menu's draw code. Until then, **whether our code
  runs on the PSP's `RYUD` is unanswered by this group**.
- **The arenas.** Two paired readers of the load area, both `minor`: MAGIC131's
  `0x801ef6ac` and `Battle_InitPartyMember` (GAME.EMI `0x801c224c`).

## 7. The tables

Generated by `psp_pair.py diff --md`. Columns: the PSX function (address,
file, and copies of it elsewhere), the sibling's name (or the label inside
it), the PSP-JP and PSP-EU twins (`-`: no EU mapping), how it was paired, the
instruction counts (with a `size` flag outside 0.6x..1.7x), the score, and
the differences (`const -[PSX only] +[PSP only]`, `w<width><offset>` fields,
`calls -[...] +[...]` by PSX name or PSP address, `unpaired PSX/PSP`
callee counts, globals). Named functions first, then by score.

#### The priority data's paired readers (every verdict)

| data | PSX (file) | name | PSP-JP | PSP-EU | verdict | score | differs in |
|---|---|---|---|---|---|---|---|
| exp_table | `801ef144` BATTLE/BATL_END.EMI#0 |  | `00149f70` | `00149d30` | differs | 2 | const -[] +[0x7608]; calls -[] +[12cfa0]; globals 6 only PSX, 1 only PSP |
| exp_table | `801ef92c` BATTLE/BATL_END.EMI#0 |  | `0014a674` | `0014a434` | same | 0 | field -[w1+2 w4+16] +[w1+278 w1+279 w1+280 w1+281 w1+282] |
| exp_table | `801efe60` BATTLE/BATL_END.EMI#0 |  | `0014b97c` | `-` | differs | 3 | const -[] +[256 0x7644 0x7648]; globals 5 only PSX, 1 only PSP |
| exp_table | `801f0a78` BATTLE/BATL_END.EMI#0 |  | `0014b8b8` | `0014b678` | platform | 0 | field -[] +[w1+121]; calls -[] +[CharId_ToRosterIndex] |
| exp_table | `801daa30` ETC/SHOP.EMI#0 |  | `000bbe24` | `000bbc98` | same | 0 |  |
| exp_table | `801aedd4` ETC/GAME.EMI#0 | Char_LevelUp | `00099c88` | `00099a04` | minor | 1 | globals 18 only PSX, 1 only PSP |
| consumables | `801ef4e0` BMAGIC/MAGIC060.EMI#0 |  | `00173188` | `-` | minor | 1 | const -[] +[0x4df8] |
| consumables | `801eef98` BMAGIC/MAGIC216.EMI#0 |  | `001798d0` | `-` | same | 0 |  |
| consumables | `801da988` ETC/SHOP.EMI#0 |  | `000bbd80` | `-` | same | 0 |  |
| consumables | `801efa50` ETC/COMMU00.EMI#0 |  | `00109a2c` | `-` | differs | 4 | const -[5] +[9]; field -[] +[w1+3792 w1+8 w1+9 w2+0 w2+12 w2+14 w2+16 w2+18 ...]; globals 10 only PSX, 2 only PSP |
| consumables | `801d12c8` ETC/COMMU04.EMI#6 |  | `001188f8` | `-` | differs | 2 | const -[0xfffe] +[]; globals 2 only PSX, 1 only PSP |
| consumables | `801b0494` ETC/GAME.EMI#0 |  | `0009a80c` | `0009a588` | same | 0 | field -[] +[w2+54]; calls -[] +[dce04] unpaired 1/0 |
| consumables | `801d31a8` ETC/START.EMI#8 |  | `000b1ba0` | `000b1960` | differs | 3 | const -[-8] +[]; field -[] +[w1+756 w1+9291 w2+16 w2+4 w4+0]; calls -[] +[9a80c] unpaired 1/1 |
| consumables | `801e105c` WORLD00/AREA030.EMI#4 |  | `000e371c` | `000e342c` | differs | 4 | const -[56] +[104 224 66]; field -[w1+0 w1+1] +[w1+32 w1+33] |
| consumables | `80166720` SLPS_009.90 | Item_NamePtr | `000b8c54` | `-` | same | 0 |  |
| consumables | `801667d4` SLPS_009.90 |  | `000b8d08` | `-` | same | 0 |  |
| consumables | `80166918` SLPS_009.90 |  | `000b8e58` | `-` | same | 0 |  |
| consumables | `80167058` SLPS_009.90 |  | `000b9560` | `-` | same | 0 |  |
| arena load area | `801ef6ac` BMAGIC/MAGIC131.EMI#3 |  | `001bcb30` | `001bc87c` | minor | 1 | field -[] +[w2+0 w2+66]; globals 1 only PSX, 1 only PSP |
| arena load area | `801c224c` ETC/GAME.EMI#0 | Battle_InitPartyMember | `000d7a78` | `000d7790` | minor | 1 | field -[] +[w4+0]; calls -[] +[] unpaired 1/0; globals 1 only PSX, 1 only PSP |
| the constant 87 | `801b0494` ETC/GAME.EMI#0 |  | `0009a80c` | `0009a588` | same | 0 | field -[] +[w2+54]; calls -[] +[dce04] unpaired 1/0 |

#### Battle and effects: 317 differing pairs

| PSX (file) | name | PSP-JP | PSP-EU | paired by | instr. PSX/PSP | score | differs in |
|---|---|---|---|---|---|---|---|
| `801d1228` BATTLE/BATTLE.EMI#3 | Battle_Init | `00120130` | `0011fda0` | similarity | 338/278 | 14 | field -[] +[w2+480 w2+720]; calls -[] +[970fc] unpaired 2/0; globals 17 only PSX, 13 only PSP |
| `800a79ac` BATTLE/BATTLE.EMI#15 | Formation_ApplyStatMods | `0013ae88` | `0013abd4` | caller-vote | 123/114 | 7 | const -[] +[0x3f90 186 5 506 826]; field -[] +[w2+186 w2+506 w2+826]; globals 5 only PSX, 2 only PSP |
| `801dc00c` BATTLE/BATTLE.EMI#3 | Battle_CalcDamage | `0012764c` | `001272d4` | callee-align | 431/383 | 7 | const -[-3 25 0xfffd 0x10000] +[50 60 70]; globals 5 only PSX, 1 only PSP |
| `800987f0` BATTLE/BATTLE.EMI#15 | Escape_Failed | `0012c79c` | `0012c45c` | similarity | 74/66 | 6 | calls -[] +[122264 1268b8 12cc34 Battle_BuildTurnOrder Battle_RemoveFromTurnOrder] |
| `801d1e84` BATTLE/BATTLE.EMI#3 | BattleMenu_DirectionHold (label) | `001205a0` | `00120210` | similarity | 228/260 | 6 | const -[-2 -4] +[444]; field -[w1+290] +[w1+1 w1+2 w1+20 w1+5 w1+6 w2+1808]; calls -[126768] +[Inventory_Add]; globals 13 only PSX, 1 only PSP |
| `801e3b8c` BATTLE/BATTLE.EMI#3 | Battle_ResolveAction_Enemy | `00140e24` | `-` | similarity-weak | 249/244 | 6 | const -[] +[160 0xff00]; field -[w4+160 w4+163 w4+164 w4+167 w4+168 w4+171 w4+172 w4+175 ...] +[w1+10 w1+149 w2+10 w2+12 w2+14 w2+16 w2+18 w2+2 ...]; calls -... |
| `801daab4` BATTLE/BATTLE.EMI#3 | Battle_BuildTurnOrder | `00126ee0` | `-` | callee-align | 321/268 | 4 | const -[-3 25] +[100]; field -[] +[w1+9 w2+0]; globals 5 only PSX, 1 only PSP |
| `801dc704` BATTLE/BATTLE.EMI#3 | Battle_HitCheck_PartyTarget | `001251e4` | `00124e64` | gap-order | 80/109 | 4 | const -[-3 239 25] +[100]; field -[] +[w1+27] |
| `801dc85c` BATTLE/BATTLE.EMI#3 | Battle_HitCheck_EnemyTarget | `001253a0` | `00125020` | gap-order | 133/150 | 4 | const -[-3 239 25] +[100]; field -[] +[w1-588] |
| `801dcd18` BATTLE/BATTLE.EMI#3 | Battle_ScaleDamage | `00125868` | `001254e8` | gap-align | 100/89 | 4 | const -[-3 125 25] +[100] |
| `801ef34c` BATTLE/BATL_END.EMI#0 | BattleResult_Setup (label) | `0014a3a0` | `0014a160` | similarity | 198/178 | 3 | const -[15] +[0x7608]; calls -[] +[970fc CharId_ToRosterIndex Char_LevelUp] unpaired 1/0 |
| `80095418` BATTLE/BATTLE.EMI#15 | ItemMenu_Confirm | `0012b4fc` | `0012b1bc` | similarity | 317/317 | 3 | const -[] +[0xfffff9d0]; field -[] +[w1+10 w1+12 w1+579 w1+586 w1+587 w1+588 w1+684 w1+8 ...]; calls -[] +[9a80c] |
| `80098278` BATTLE/BATTLE.EMI#15 | Escape_Roll | `0012c284` | `0012bf44` | gap-align | 94/118 | 3 | const -[320 840] +[]; field -[] +[w1+16 w1+17 w1+19 w1+3 w1+68 w2+168]; globals 5 only PSX, 1 only PSP |
| `80098450` BATTLE/BATTLE.EMI#15 | Escape_Begin | `0012c498` | `0012c158` | similarity | 143/126 | 3 | const -[] +[80]; calls -[] +[12cc34 970fc Inventory_Add] unpaired 1/0 |
| `801d7e80` BATTLE/BATTLE.EMI#3 | BattleBanner_DrawFrame | `00121b38` | `001217b8` | callee-align | 119/124 | 3 | const -[12 248 32] +[]; field -[] +[w2+40 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801d8060` BATTLE/BATTLE.EMI#3 | BattleMenu_TargetCursor | `00121d28` | `001219a8` | gap-order | 119/124 | 3 | const -[12 248 32] +[]; field -[] +[w2+40 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801dfc68` BATTLE/BATTLE.EMI#3 | Battle_ResolveAction_Party | `0013daec` | `-` | similarity-weak | 245/252 | 3 | const -[] +[144 0xff00]; field -[w4+144 w4+147 w4+148 w4+151 w4+152 w4+155 w4+156 w4+159 ...] +[w1+149 w2+0 w2+10 w2+12 w2+14 w2+16 w2+18 w2+2 ...]; globals ... |
| `80099954` BATTLE/BATTLE.EMI#15 | EnemyAI_ApplyAction | `0012e750` | `0012e424` | similarity | 177/178 | 2 | const -[125] +[0x2710]; field -[w1+130] +[w2+130] |
| `8009a158` BATTLE/BATTLE.EMI#15 | Effect_ApplyResult (label) | `001339f0` | `00133714` | similarity | 498/329 | 2 | const -[-3 0xfffd 0x10000] +[]; field -[] +[w4+0] |
| `801dbb40` BATTLE/BATTLE.EMI#3 | Battle_ApplyDamage | `00127314` | `00126f9c` | similarity | 297/204 | 2 | const -[-3] +[72] |
| `801dcaa0` BATTLE/BATTLE.EMI#3 | Battle_BaseDamage | `00125624` | `001252a4` | gap-align | 106/108 | 2 | const -[-3] +[]; globals 1 only PSX, 1 only PSP |
| `801dfa14` BATTLE/BATTLE.EMI#3 | Battle_SwingCue_Step | `0013ba28` | `0013b77c` | gap-order | 54/45 | 2 | const -[25] +[100]; field -[] +[w2+8] |
| `801e1fa8` BATTLE/BATTLE.EMI#3 | Battle_SwingCue_Step2 | `0013d3ac` | `0013d120` | gap-align | 53/44 | 2 | const -[25] +[100]; field -[] +[w2+8] |
| `8017b480` SLPS_009.90 | SetLineG4 | `0009fcfc` | `0009f9d0` | gap-align | 8/3 (size 0.4x) | 2 | const -[0x55555555 9] +[]; field -[w1+3 w4+36] +[] |
| `800abc84` BATTLE/BATTLE.EMI#15 |  | `001483cc` | `00148190` | caller-vote | 90/124 | 20 | field -[] +[w2-11664 w4+192]; globals 30 only PSX, 20 only PSP |
| `800b0a20` BATTLE/BATTLE.EMI#15 |  | `0014f9dc` | `-` | similarity | 457/490 | 17 | const -[11] +[0x4608 0x4614 0x7660 40 88]; field -[] +[w1+122 w1+1669 w4+0]; calls -[] +[9a620 9a80c 9a940 9ab6c 9b770]; globals 4 only PSX, 1 only PSP |
| `800c223c` BOSS/BOSS025.EMI#13 |  | `001e64e0` | `001e61c8` | similarity-weak | 147/233 | 17 | const -[0x600 0xffff00 0x10600] +[27 28 29 30 31 5 70]; calls -[] +[13e400] unpaired 2/0; globals 12 only PSX, 5 only PSP |
| `800af4f0` BATTLE/BATTLE.EMI#15 |  | `0014e578` | `-` | similarity | 425/460 | 14 | const -[5] +[40 88]; field -[] +[w1+6 w4+0]; calls -[] +[9a620 9a80c 9a940 9ab6c 9b770]; globals 1 only PSX, 1 only PSP |
| `801f103c` BMAGIC/MAGIC064.EMI#3 |  | `00178834` | `00178574` | caller-vote | 145/145 | 14 | const -[-24 0x3ff 0x400 112 28 40 55] +[0x10401e 132 0x800 32 33 5 58]; field -[] +[w2-7472 w2+0]; calls -[] +[d2ac] unpaired 1/0; globals 6 only PSX, 2 only... |
| `801eec80` BMAGIC/MAGIC102.EMI#3 |  | `0019de14` | `0019db60` | similarity-weak | 312/317 | 14 | const -[120 320 35 36 360 47 56] +[-28 0x104024 0x11d0 58 66]; field -[] +[w1+0 w1+10 w1+5 w2+0 w2+62 w4-4 w4+0 w4+52 ...]; globals 20 only PSX, 2 only PSP |
| `800afbc8` BATTLE/BATTLE.EMI#15 |  | `0014eca8` | `0014ec10` | similarity | 360/365 | 13 | const -[5] +[40]; field -[w1+16] +[w1+122 w1+6 w4+0 w4+16]; calls -[] +[9a620 9aa74 9ab6c 9b540 9b770]; globals 1 only PSX, 1 only PSP |
| `800b025c` BATTLE/BATTLE.EMI#15 |  | `0014f25c` | `0014f1c4` | gap-align | 474/480 | 13 | const -[11] +[0x4644 24 88]; field -[w2+64] +[w1+14 w1+15 w1+16 w1+1669 w1+17 w1+18 w1+19 w1+32 ...]; calls -[] +[9a1b8 9a620 9a80c bb534] unpaired 1/0; glob... |
| `801d43bc` BATTLE/BATTLE.EMI#3 |  | `0011df2c` | `0011db9c` | gap-order | 263/208 | 13 | const -[-3] +[0xfffff7ff 0xffffdfff 27 48]; field -[] +[w1+1 w1+10 w1+148 w1+149 w1+2 w2-18568 w2+8 w4+148 ...]; globals 41 only PSX, 8 only PSP |
| `801ef484` BMAGIC/MAGIC151.EMI#3 |  | `001c70e0` | `001c6e2c` | similarity | 285/223 | 12 | const -[254] +[40]; field -[] +[w2+128]; calls -[] +[1263f8 1268b8 126a28 Battle_ClearActorBit Battle_InitPartyMember Battle_RemoveFromTurnOrder]; globals 16... |
| `8009de6c` BATTLE/BATTLE.EMI#15 |  | `001343a8` | `001340cc` | similarity | 310/246 | 11 | const -[-3] +[10 0xe4048]; field -[] +[w2+32]; globals 10 only PSX, 8 only PSP |
| `801efcf0` BMAGIC/MAGIC105.EMI#3 |  | `001a3340` | `001a308c` | gap-align | 180/166 | 10 | const -[-128 157 369 449 45 480 496 557] +[-64 -96 305 32 0x1000 417]; field -[w1-1] +[w1+3] |
| `800a0170` BATTLE/BATTLE.EMI#15 |  | `00134790` | `001344b4` | caller-vote | 268/253 | 9 | const -[0x804 0x80c 0x814 0x840 0x864] +[]; field -[w2+0] +[w2-710]; calls -[] +[1263f8 1268b8 Battle_RemoveFromTurnOrder] |
| `801ef900` BMAGIC/MAGIC057.EMI#3 |  | `00017c84` | `00017b08` | similarity-weak | 70/59 | 9 | const -[160 29] +[200 21 240 260 57]; field -[w1+2 w1+42 w2+44 w2+62 w4+52 w4+56] +[w1+1 w2+46 w2+48 w4+16]; calls -[281c f5c0] +[] unpaired 0/1 |
| `800c2220` BOSS/BOSS013.EMI#16 |  | `001e2ac4` | `001e27a0` | gap-align | 62/115 (size 1.8x) | 9 | const -[] +[0xffffa250 39 5 6 0x2000]; calls -[] +[122264 12cc34 Battle_InitPartyMember Msg_OpenScript]; globals 3 only PSX, 4 only PSP |
| `800c2bc8` BOSS/BOSS025.EMI#13 |  | `001e6c14` | `001e68fc` | similarity | 65/99 | 9 | const -[215] +[-17 -33 -9 12 5 6]; calls -[] +[29ec] unpaired 1/0; globals 2 only PSX, 2 only PSP |
| `801eed50` BMAGIC/MAGIC068.EMI#3 |  | `0017cb7c` | `0017c8c8` | similarity | 399/359 | 8 | const -[-31 -51 -71 320] +[50]; field -[] +[w1+175 w1+176 w1+177 w1+178 w1+179 w1+180 w1+7]; globals 10 only PSX, 3 only PSP |
| `801f0290` BATTLE/BATL_END.EMI#0 |  | `0014bdb0` | `0014bd2c` | callee-align | 168/174 | 7 | const -[248 32] +[]; field -[] +[w2+40]; calls -[] +[122898 122dc4 122ebc] |
| `8009770c` BATTLE/BATTLE.EMI#15 |  | `0012a610` | `0012a2c8` | gap-order | 218/224 | 7 | field -[w4+0] +[w4+16]; globals 10 only PSX, 7 only PSP |
| `8009bae0` BATTLE/BATTLE.EMI#15 |  | `0012f6bc` | `0012f394` | similarity | 64/66 | 7 | const -[-3] +[]; calls -[] +[1268b8 Battle_CalcDamage Battle_RemoveFromTurnOrder] |
| `8009cae8` BATTLE/BATTLE.EMI#15 |  | `0013056c` | `00130248` | gap-order | 170/165 | 7 | globals 9 only PSX, 7 only PSP |
| `8009ed24` BATTLE/BATTLE.EMI#15 |  | `00131d14` | `00131a00` | gap-order | 213/181 | 7 | globals 9 only PSX, 7 only PSP |
| `800a8f4c` BATTLE/BATTLE.EMI#15 |  | `00141414` | `001411e0` | similarity | 120/116 | 7 | const -[112] +[14 198 55 791]; field -[w4+8] +[]; globals 5 only PSX, 2 only PSP |
| `801e0af8` BATTLE/BATTLE.EMI#3 |  | `0013c108` | `0013be6c` | caller-vote | 159/159 | 7 | globals 9 only PSX, 7 only PSP |
| `801ef818` BMAGIC/MAGIC144.EMI#3 +1 copies |  | `001925d8` | `00192324` | callee-align | 445/438 | 7 | const -[-128 -3 -4] +[14 24 320 6 64] |
| `800a09a0` BATTLE/BATTLE.EMI#15 |  | `00133058` | `-` | callee-align | 78/74 | 6 | const -[25 5 625] +[100 0x2710 500] |
| `800a0af4` BATTLE/BATTLE.EMI#15 |  | `001331a0` | `00132ea4` | callee-align | 71/66 | 6 | const -[25 5 625] +[100 0x2710 500] |
| `800a0c24` BATTLE/BATTLE.EMI#15 |  | `001332c8` | `-` | similarity | 78/74 | 6 | const -[25 5 625] +[100 0x2710 500] |
| `800a0d78` BATTLE/BATTLE.EMI#15 |  | `00133410` | `0013311c` | callee-align | 71/66 | 6 | const -[25 5 625] +[100 0x2710 500] |
| `800a7ff0` BATTLE/BATTLE.EMI#15 |  | `0013b068` | `-` | similarity-weak | 180/194 | 6 | const -[0x14000] +[6]; field -[] +[w1+0 w1+1 w1+164 w1+17 w1+19 w2+2]; calls -[] +[124ce8 1262fc 13a3c8] unpaired 1/0 |
| `800ae99c` BATTLE/BATTLE.EMI#15 |  | `0014d170` | `-` | similarity | 207/188 | 6 | const -[5] +[0x45c9 0x45d5 40]; calls -[] +[9a620] |
| `801eed04` BMAGIC/MAGIC092.EMI#3 |  | `00192178` | `00191ec4` | callee-align | 167/137 | 6 | const -[120 280 320] +[]; field -[] +[w2+0]; globals 7 only PSX, 3 only PSP |
| `801efd7c` BMAGIC/MAGIC102.EMI#3 |  | `0019db00` | `0019d84c` | gap-align | 83/100 | 6 | const -[28 56] +[0x104024 33 58 66]; field -[] +[w2+0 w4-4 w4+0]; calls -[] +[144110] |
| `801f0778` BMAGIC/MAGIC102.EMI#3 |  | `0019f354` | `0019f0a0` | gap-align | 194/181 | 6 | const -[-128 441 451] +[0xffffc000 301 381]; field -[w1+10 w1+11 w1+6 w1+7 w1+8 w1+9 w2+0 w4+12] +[w1+2 w4+0 w4+4] |
| `801ef290` BMAGIC/MAGIC107.EMI#3 |  | `001a47e8` | `001a4534` | callee-align | 164/166 | 6 | const -[-8 145 37 5] +[-16 129 256] |
| `801ef544` BMAGIC/MAGIC107.EMI#3 |  | `001a4a84` | `001a47d0` | callee-align | 284/290 | 6 | const -[-8 0x500 129 37] +[-16 241 256]; field -[] +[w4+12] |
| `80093b7c` BATTLE/BATTLE.EMI#15 |  | `00127d60` | `001279e8` | gap-order | 77/102 | 5 | const -[] +[-624]; field -[] +[w1+4 w4-18712]; calls -[] +[124ce8 124e1c 1261b0] |
| `80093cc8` BATTLE/BATTLE.EMI#15 |  | `00127ef8` | `00127b80` | gap-order | 77/102 | 5 | const -[] +[-624]; field -[] +[w1+4 w4-18712]; calls -[] +[124ce8 124e1c 1261b0] |
| `80095be0` BATTLE/BATTLE.EMI#15 |  | `00129388` | `00129040` | gap-order | 98/126 | 5 | const -[] +[0xffff9c90]; field -[] +[w1+586 w1+588 w4-18712]; calls -[] +[124ce8 124e1c 1261b0] |
| `80095d8c` BATTLE/BATTLE.EMI#15 |  | `00129584` | `0012923c` | gap-order | 117/138 | 5 | const -[] +[0xffff9c90]; field -[] +[w1+4 w1+586 w1+588]; calls -[] +[124ce8 124e1c 1261b0] |
| `80097be0` BATTLE/BATTLE.EMI#15 |  | `0012aa90` | `0012a748` | gap-order | 204/209 | 5 | const -[] +[116 144 176 28 60]; field -[w4+0] +[w1+14 w1+15 w1+16 w1+17 w1+18 w1+19 w1+316 w2+0 ...] |
| `8009c9d0` BATTLE/BATTLE.EMI#15 |  | `00130464` | `00130140` | gap-order | 48/51 | 5 | const -[-3 25] +[100]; calls -[] +[Battle_CalcDamage] |
| `8009dc18` BATTLE/BATTLE.EMI#15 |  | `001313e4` | `001310c8` | gap-order | 59/58 | 5 | const -[0x2710 5] +[10]; calls -[] +[Battle_CalcDamage] |
| `800a13c0` BATTLE/BATTLE.EMI#15 |  | `00134e0c` | `00134b30` | callee-align | 123/87 | 5 | const -[] +[10]; globals 4 only PSX, 5 only PSP |
| `800a9148` BATTLE/BATTLE.EMI#15 |  | `00141ba0` | `0014196c` | caller-vote | 310/172 (size 0.6x) | 5 | const -[] +[44 48 0xe4048]; field -[] +[w1+0 w1+10 w1+120 w1+121 w1+122 w1+123 w1+124 w1+125 ...]; globals 64 only PSX, 2 only PSP |
| `800ad61c` BATTLE/BATTLE.EMI#15 |  | `0014c5c4` | `0014c540` | gap-align | 180/172 | 5 | const -[12] +[-16 -8]; field -[] +[w4+2000]; calls -[] +[9a620] |
| `801d74dc` BATTLE/BATTLE.EMI#3 |  | `0012309c` | `00122d1c` | callee-align | 324/315 | 5 | const -[12 248 254 32] +[0x2000]; field -[] +[w1+16 w1+479 w1+480 w1+481 w1+482 w2+128 w2+136 w2+138 ...]; globals 9 only PSX, 2 only PSP |
| `801f0550` BMAGIC/MAGIC003.EMI#3 |  | `00153b4c` | `00153aac` | callee-align | 150/139 | 5 | const -[-54 -78 5] +[-12 10] |
| `801eef8c` BMAGIC/MAGIC004.EMI#3 |  | `00154074` | `00153fd4` | gap-align | 96/96 | 5 | const -[-133] +[132 137]; calls -[] +[3c5c] |
| `801ef174` BMAGIC/MAGIC060.EMI#0 |  | `00172e58` | `00172db8` | gap-align | 87/82 | 5 | const -[] +[0x4de0 0x4de5 0x4de9 0x4deb 0x4df0] |
| `801ef5f4` BMAGIC/MAGIC060.EMI#0 |  | `00173ca0` | `-` | callee-align | 182/176 | 5 | const -[6] +[-504 24 36 72] |
| `801efc84` BMAGIC/MAGIC060.EMI#0 |  | `00173f84` | `-` | callee-align | 231/153 | 5 | const -[131] +[5 6]; field -[w1+0 w1+1 w1+11 w1+2 w1+9] +[]; calls -[1737fc] +[] |
| `801ef580` BMAGIC/MAGIC087.EMI#3 |  | `0018ef00` | `0018ec4c` | similarity | 227/220 | 5 | const -[-240 97] +[-4 -8 337] |
| `801f02c8` BMAGIC/MAGIC172.EMI#3 |  | `001d1844` | `001d1590` | gap-align | 177/163 | 5 | const -[-12 -13 12] +[-72 -78] |
| `801ef0a8` BATTLE/BATL_OVR.EMI#0 |  | `0014aa1c` | `0014a7dc` | similarity | 46/42 | 4 | const -[] +[0x4e1c]; field -[] +[w1+4]; calls -[94edc ec8] +[144170]; globals 1 only PSX, 1 only PSP |
| `8009bd6c` BATTLE/BATTLE.EMI#15 |  | `0012f8f4` | `0012f5cc` | gap-order | 67/68 | 4 | const -[-3] +[]; calls -[] +[Battle_CalcDamage]; globals 3 only PSX, 1 only PSP |
| `8009bfc0` BATTLE/BATTLE.EMI#15 |  | `0012fb40` | `0012f81c` | gap-order | 36/38 | 4 | const -[-3] +[]; calls -[] +[Battle_CalcDamage]; globals 2 only PSX, 1 only PSP |
| `8009c054` BATTLE/BATTLE.EMI#15 |  | `0012fbd8` | `0012f8b4` | gap-order | 67/67 | 4 | const -[-3] +[]; calls -[] +[Battle_CalcDamage]; globals 2 only PSX, 1 only PSP |
| `8009f820` BATTLE/BATTLE.EMI#15 |  | `001325e8` | `-` | similarity | 139/137 | 4 | const -[-3 25 625 0x10000] +[0x2710] |
| `800a0080` BATTLE/BATTLE.EMI#15 |  | `00132e70` | `-` | callee-align | 58/54 | 4 | const -[-3 25 625] +[0x2710] |
| `800a0680` BATTLE/BATTLE.EMI#15 |  | `00134b88` | `001348ac` | caller-vote | 174/161 | 4 | const -[0xfffd] +[]; calls -[] +[1263f8]; globals 4 only PSX, 3 only PSP |
| `800a3d7c` BATTLE/BATTLE.EMI#15 |  | `001379b8` | `001376dc` | callee-align | 485/420 | 4 | const -[-10] +[40]; field -[] +[w1+0 w1+1 w1+2 w1+26 w1+3 w1+4 w2+60]; calls -[] +[126974 1269b0]; globals 12 only PSX, 2 only PSP |
| `800ac020` BATTLE/BATTLE.EMI#15 |  | `001487d4` | `-` | gap-order | 106/105 | 4 | const -[0xffffff 0xff000000 56] +[0x8fcc 58]; field -[] +[w4+2000] |
| `801ddc7c` BATTLE/BATTLE.EMI#3 |  | `001265d0` | `00126250` | gap-order | 108/99 | 4 | const -[-3 25 625] +[100]; field -[] +[w1+0] |
| `801e722c` BATTLE/BATTLE.EMI#3 |  | `00145714` | `001454d0` | gap-order | 90/104 | 4 | const -[] +[52 56 60 64 68]; field -[w4+52 w4+56 w4+60 w4+64 w4+68] +[w4+0] |
| `801eecc0` BMAGIC/MAGIC020.EMI#3 |  | `0015c9fc` | `0015c95c` | gap-align | 255/150 (size 0.6x) | 4 | const -[-3] +[14]; calls -[1493dc] +[] |
| `801ef4e0` BMAGIC/MAGIC044.EMI#3 |  | `00166cac` | `00166c0c` | callee-align | 323/296 | 4 | const -[12 6] +[103 205]; field -[] +[w2+0 w2+2] |
| `801efa4c` BMAGIC/MAGIC044.EMI#3 |  | `0016714c` | `001670ac` | callee-align | 493/425 | 4 | const -[6] +[-8 103 137]; field -[] +[w2+0 w2+2 w2+4 w2+6] |
| `801f04ec` BMAGIC/MAGIC056.EMI#3 |  | `00187904` | `00187650` | similarity-weak | 70/35 (size 0.5x) | 4 | const -[-128 -16 28] +[0xffffc000 0x4000000]; field -[w1+8 w1+93 w1+94 w1+95] +[w1+10 w1+11 w1+9]; calls -[281c Actor_AnimTick] +[] |
| `801ef454` BMAGIC/MAGIC058.EMI#3 |  | `001708c0` | `00170820` | gap-align | 86/81 | 4 | const -[-3] +[]; globals 3 only PSX, 3 only PSP |
| `801efa9c` BMAGIC/MAGIC060.EMI#0 |  | `001734a0` | `00173724` | gap-align | 117/116 | 4 | const -[6] +[10 72 84] |
| `801ef984` BMAGIC/MAGIC066.EMI#3 |  | `0017a380` | `0017a0cc` | gap-align | 184/150 | 4 | const -[280 320] +[]; globals 3 only PSX, 2 only PSP |
| `801f0274` BMAGIC/MAGIC069.EMI#3 |  | `0017df70` | `0017dcbc` | gap-align | 138/123 | 4 | const -[280 320] +[]; globals 4 only PSX, 2 only PSP |
| `801efbec` BMAGIC/MAGIC079.EMI#3 |  | `0018788c` | `001875d8` | gap-align | 22/24 | 4 | calls -[18863c] +[187f1c] |
| `801eef74` BMAGIC/MAGIC091.EMI#3 +1 copies |  | `0016d3a4` | `0016d304` | gap-align | 22/24 | 4 | calls -[190ed4] +[16d870] |
| `801ef804` BMAGIC/MAGIC095.EMI#3 |  | `00195b18` | `00195864` | callee-align | 353/301 | 4 | const -[-4 41] +[-192 215 512]; field -[] +[w1+8] |
| `801eec5c` BMAGIC/MAGIC097.EMI#3 |  | `001996b8` | `00199404` | similarity | 171/129 | 4 | const -[280 320] +[]; globals 5 only PSX, 2 only PSP |
| `801eec5c` BMAGIC/MAGIC098.EMI#3 |  | `001983c8` | `00198114` | callee-align | 171/129 | 4 | const -[280 320] +[]; globals 5 only PSX, 2 only PSP |
| `801ef1d8` BMAGIC/MAGIC103.EMI#3 |  | `0019ffe8` | `0019fd34` | callee-align | 346/325 | 4 | const -[40 64] +[-88 192 0xc00]; field -[w1+12 w1+13 w1+16 w1+17 w1+18 w1+24 w1+25 w1+28 ...] +[] |
| `801ef370` BMAGIC/MAGIC104.EMI#3 |  | `001a130c` | `001a1058` | callee-align | 403/421 | 4 | const -[-32 -48] +[120 256 320 336] |
| `801efc44` BMAGIC/MAGIC167.EMI#3 |  | `001cd364` | `001cd0b0` | gap-align | 579/532 | 4 | const -[17] +[-3 180 45] |
| `801f05e8` BMAGIC/MAGIC172.EMI#3 |  | `001d1ad0` | `001d181c` | gap-align | 177/163 | 4 | const -[-8 -9] +[-48 -54] |
| `801eeeec` BMAGIC/MAGIC173.EMI#3 |  | `001d2878` | `001d25c4` | similarity | 137/151 | 4 | const -[-3 0x20000 0x30000 0x38000 0x10000] +[98]; calls -[] +[125d70] |
| `800c1fe8` BOSS/BOSS004.EMI#13 |  | `001e0390` | `001e006c` | gap-align | 186/80 (size 0.4x) | 4 | const -[-3 120 128 50 64 7] +[5]; field -[w1+0 w2+2] +[w4+52 w4+56 w4+60] |
| `800c217c` BOSS/BOSS018.EMI#16 +2 copies |  | `001e3fd0` | `001e3cac` | gap-align | 40/56 | 4 | const -[] +[5 6]; field -[] +[w2+690]; calls -[] +[1e49d0] |
| `800c1e34` BOSS/BOSS025.EMI#13 |  | `001e5e1c` | `001e5af8` | gap-order | 29/31 | 4 | const -[0x2800 0x800] +[32]; calls -[] +[13e400 13eb48]; globals 1 only PSX, 1 only PSP |
| `800c3088` BOSS/BOSS025.EMI#13 |  | `001e63d4` | `-` | similarity | 59/65 | 4 | const -[] +[0x7798]; calls -[] +[121f18 144110]; globals 2 only PSX, 1 only PSP |
| `800c2310` BOSS/BOSS033.EMI#16 |  | `001e8d10` | `001e89f8` | gap-order | 68/68 | 4 | const -[-3 15] +[]; globals 3 only PSX, 3 only PSP |
| `800c18fc` BOSS/BOSS036.EMI#16 |  | `001ea4e8` | `001ea1d0` | gap-align | 31/38 | 4 | const -[0x30000] +[0xe4048]; field -[] +[w1+134]; calls -[] +[13e400 13eb48] |
| `800c1b6c` BOSS/BOSS036.EMI#16 |  | `001ea748` | `001ea430` | gap-order | 73/55 | 4 | const -[-3 0xffff00 0x10700] +[14 7]; field -[w4+0] +[w1+1 w1+2] |
| `800c19cc` BOSS/BOSS052.EMI#16 |  | `001ec544` | `001ec22c` | gap-align | 76/58 | 4 | const -[-3 0xffff00 0x10700] +[14 7]; field -[w4+0] +[w1+1 w1+2] |
| `800c1dc8` BOSS/BOSS055.EMI#16 |  | `001ed730` | `-` | gap-align | 99/122 | 4 | const -[] +[131 0xe4048]; field -[] +[w1+135 w4+5968]; calls -[] +[126830 13e400 13ebc0] |
| `801efe60` BATTLE/BATL_END.EMI#0 |  | `0014b97c` | `-` | similarity | 247/269 | 3 | const -[] +[256 0x7644 0x7648]; globals 5 only PSX, 1 only PSP |
| `801f093c` BATTLE/BATL_END.EMI#0 |  | `0014b774` | `0014b534` | similarity | 77/81 | 3 | const -[] +[0x763c]; calls -[] +[9a1b8] |
| `8009aa74` BATTLE/BATTLE.EMI#15 |  | `0012ec88` | `0012e95c` | gap-order | 54/63 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `8009ab64` BATTLE/BATTLE.EMI#15 |  | `0012ed88` | `0012ea5c` | gap-order | 42/41 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `8009bc00` BATTLE/BATTLE.EMI#15 |  | `0012f7cc` | `0012f4a4` | gap-order | 82/74 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `8009c16c` BATTLE/BATTLE.EMI#15 |  | `0012fce4` | `0012f9c0` | gap-order | 38/40 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `8009c834` BATTLE/BATTLE.EMI#15 |  | `00130308` | `0012ffe4` | gap-order | 54/49 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `8009d138` BATTLE/BATTLE.EMI#15 |  | `00130ac4` | `001307a8` | gap-order | 43/44 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `8009d3dc` BATTLE/BATTLE.EMI#15 |  | `00130d38` | `00130a1c` | gap-order | 69/66 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `8009d610` BATTLE/BATTLE.EMI#15 |  | `00130f04` | `00130be8` | gap-order | 85/75 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `8009f538` BATTLE/BATTLE.EMI#15 |  | `0013236c` | `00132058` | similarity | 44/44 | 3 | const -[-3] +[]; calls -[] +[Battle_CalcDamage] |
| `800a1204` BATTLE/BATTLE.EMI#15 |  | `00133840` | `-` | similarity | 100/102 | 3 | const -[-3 625 0x10000] +[0x2710] |
| `800a45fc` BATTLE/BATTLE.EMI#15 |  | `001362a8` | `00135fcc` | callee-align | 134/105 | 3 | globals 3 only PSX, 3 only PSP |
| `800a573c` BATTLE/BATTLE.EMI#15 |  | `00138a0c` | `00138730` | similarity | 136/127 | 3 | const -[112 0x10000] +[14]; field -[w4+12 w4+8] +[]; calls -[] +[13b560]; globals 7 only PSX, 1 only PSP |
| `800a84a0` BATTLE/BATTLE.EMI#15 |  | `0013a268` | `00139fb4` | gap-align | 64/50 | 3 | const -[] +[20]; calls -[] +[124ce8] |
| `800a8840` BATTLE/BATTLE.EMI#15 |  | `0013a52c` | `0013a278` | similarity | 151/129 | 3 | const -[-3] +[]; field -[] +[w4-11816]; globals 7 only PSX, 2 only PSP |
| `800a9894` BATTLE/BATTLE.EMI#15 |  | `00141e54` | `00141c20` | similarity | 370/351 | 3 | const -[-21] +[-13 13]; field -[w1+2 w4+0 w4+3 w4+4 w4+7] +[w1+124 w1+18 w2+16 w2+18 w2+22] |
| `800abf3c` BATTLE/BATTLE.EMI#15 |  | `001486fc` | `001484c0` | gap-order | 54/54 | 3 | const -[319] +[-32 351 383] |
| `800ac73c` BATTLE/BATTLE.EMI#15 |  | `00148f28` | `00148ce8` | callee-align | 51/46 | 3 | const -[-3] +[]; calls -[] +[3c5c] |
| `800accc4` BATTLE/BATTLE.EMI#15 |  | `001494ac` | `0014926c` | callee-align | 113/90 | 3 | const -[280 320] +[]; globals 1 only PSX, 2 only PSP |
| `800acec0` BATTLE/BATTLE.EMI#15 |  | `00149624` | `001493e4` | caller-vote | 41/46 | 3 | const -[-3] +[0xe4048]; field -[] +[w1+134]; globals 2 only PSX, 1 only PSP |
| `800acf80` BATTLE/BATTLE.EMI#15 |  | `001496e0` | `001494a0` | gap-order | 41/46 | 3 | const -[-3] +[0xe4048]; field -[] +[w1+135]; globals 2 only PSX, 1 only PSP |
| `800adec4` BATTLE/BATTLE.EMI#15 |  | `0014d704` | `0014d678` | callee-align | 236/250 | 3 | field -[w1+18 w1+20] +[w1+0 w1+3 w1+4 w2+138 w2+20]; calls -[] +[9b770]; globals 7 only PSX, 1 only PSP |
| `800ae650` BATTLE/BATTLE.EMI#15 |  | `0014daf4` | `0014da68` | similarity | 196/210 | 3 | field -[w1+18 w1+20] +[w1+0 w1+3 w1+4 w2+138 w2+20]; calls -[] +[9b770]; globals 6 only PSX, 1 only PSP |
| `800b1160` BATTLE/BATTLE.EMI#15 |  | `0014e3d4` | `-` | similarity | 101/105 | 3 | const -[58] +[0x46c8 0x46cc] |
| `801d1820` BATTLE/BATTLE.EMI#3 |  | `0011c504` | `0011c15c` | similarity | 159/123 | 3 | const -[254] +[]; calls -[] +[] unpaired 1/0; globals 4 only PSX, 2 only PSP |
| `801d360c` BATTLE/BATTLE.EMI#3 |  | `001210c4` | `00120d44` | similarity | 126/97 | 3 | const -[-3] +[37]; globals 6 only PSX, 1 only PSP |
| `801d8240` BATTLE/BATTLE.EMI#3 |  | `00121f18` | `00121b98` | gap-order | 119/124 | 3 | const -[12 248 32] +[]; field -[] +[w2+40 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801d8420` BATTLE/BATTLE.EMI#3 |  | `00123800` | `00123480` | similarity | 143/147 | 3 | const -[12 248 32] +[]; field -[] +[w2+40 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801da7a4` BATTLE/BATTLE.EMI#3 |  | `00126c5c` | `001268dc` | callee-align | 178/161 | 3 | const -[-3] +[]; field -[w1+2 w4+0 w4+3] +[w2+16 w2+18]; globals 3 only PSX, 2 only PSP |
| `801ddab8` BATTLE/BATTLE.EMI#3 |  | `00126434` | `001260b4` | gap-order | 34/35 | 3 | const -[] +[52 56 60]; field -[w4+52 w4+56 w4+60] +[w4+0]; calls -[] +[] unpaired 1/0 |
| `801e61fc` BATTLE/BATTLE.EMI#3 |  | `00144804` | `001445b8` | gap-align | 63/51 | 3 | const -[0x66660000 5] +[10] |
| `801e7ad8` BATTLE/BATTLE.EMI#3 |  | `0014632c` | `001460e8` | similarity | 554/425 | 3 | const -[13] +[40]; field -[] +[w2+128 w4+52]; calls -[] +[] unpaired 1/0; globals 14 only PSX, 1 only PSP |
| `801e85bc` BATTLE/BATTLE.EMI#3 |  | `001469e0` | `0014679c` | callee-align | 367/299 | 3 | const -[13] +[40]; field -[] +[w2+128 w4+52]; globals 9 only PSX, 1 only PSP |
| `801e8d10` BATTLE/BATTLE.EMI#3 |  | `00146104` | `00145ec0` | callee-align | 101/96 | 3 | const -[-3] +[0xe4048]; field -[] +[w1+1 w1+131 w4-11800]; globals 1 only PSX, 1 only PSP |
| `801efec8` BMAGIC/MAGIC003.EMI#3 |  | `00153360` | `001532c0` | callee-align | 104/101 | 3 | const -[-3] +[14 5] |
| `801eec98` BMAGIC/MAGIC004.EMI#3 |  | `00153df0` | `00153d50` | gap-align | 181/160 | 3 | const -[-3] +[14]; field -[] +[w2+0]; globals 13 only PSX, 1 only PSP |
| `801ef118` BMAGIC/MAGIC006.EMI#3 |  | `0015508c` | `00154fec` | gap-align | 80/89 | 3 | const -[0xfffff9ff 0x3ff 0x20000] +[0x600 0xa00] |
| `801eec98` BMAGIC/MAGIC009.EMI#3 |  | `001565a8` | `00156508` | gap-align | 150/147 | 3 | const -[-3] +[14]; field -[] +[w2+64 w4+5968]; globals 13 only PSX, 1 only PSP |
| `801eec98` BMAGIC/MAGIC017.EMI#3 |  | `0015b93c` | `0015b89c` | gap-align | 168/157 | 3 | const -[-3 254] +[14] |
| `801eed30` BMAGIC/MAGIC021.EMI#3 |  | `0015fbe4` | `0015fb44` | gap-align | 124/107 | 3 | const -[-3 120] +[14]; field -[] +[w4+5968] |
| `801ef138` BMAGIC/MAGIC044.EMI#3 |  | `001663f0` | `00166350` | gap-align | 102/104 | 3 | const -[0xfffff9ff 0x3ff] +[0x600 0xa00] |
| `801ef3b8` BMAGIC/MAGIC046.EMI#3 |  | `00168c8c` | `00168bec` | gap-align | 92/103 | 3 | const -[-3 115 0x10000] +[17]; calls -[] +[125d70] |
| `801eeeb0` BMAGIC/MAGIC057.EMI#3 |  | `0016f6d0` | `0016f630` | gap-order | 55/59 | 3 | const -[] +[52 56 60]; field -[w4+52 w4+56 w4+60] +[w4+0] |
| `801eec90` BMAGIC/MAGIC065.EMI#0 |  | `001799b0` | `001796fc` | similarity | 232/187 | 3 | const -[-3] +[14]; field -[] +[w1-686]; globals 11 only PSX, 1 only PSP |
| `801eec94` BMAGIC/MAGIC067.EMI#3 |  | `0017bab0` | `0017b7fc` | similarity | 166/152 | 3 | const -[-3] +[14]; field -[] +[w2+0 w2+32]; globals 10 only PSX, 1 only PSP |
| `801f00c4` BMAGIC/MAGIC067.EMI#3 |  | `0017b774` | `0017b4c0` | gap-align | 108/92 | 3 | const -[319] +[-32 351 383]; field -[] +[w4+5968] |
| `801eec70` BMAGIC/MAGIC075.EMI#3 |  | `001842a8` | `00183ff4` | gap-align | 89/89 | 3 | field -[] +[w2+0]; calls -[] +[3c5c]; globals 3 only PSX, 1 only PSP |
| `801ef3a0` BMAGIC/MAGIC082.EMI#3 |  | `0018989c` | `001895e8` | gap-align | 69/56 | 3 | const -[185 268] +[83]; field -[] +[w2+224] |
| `801f0e74` BMAGIC/MAGIC083.EMI#3 |  | `0018ce50` | `0018cb9c` | gap-order | 42/30 | 3 | const -[0xffff8030 0xa040 0x2020] +[]; field -[w1-16192 w1-2236 w1-24416 w1-2724 w1-2776 w1-292 w1-3336 w1-360 ...] +[w1+1 w4+0] |
| `801f0488` BMAGIC/MAGIC102.EMI#3 |  | `0019f024` | `0019ed60` | gap-align | 178/201 | 3 | const -[-128 0xffc] +[0xffffc000]; field -[w1+2 w1+6 w1+7 w2+0 w2+2 w2+4] +[w2+80 w2+82 w2+84] |
| `801efe34` BMAGIC/MAGIC104.EMI#3 |  | `001a1c70` | `001a19bc` | callee-align | 227/210 | 3 | const -[-8] +[256 34 49] |
| `801ef944` BMAGIC/MAGIC113.EMI#3 |  | `001a9dd0` | `001a9b1c` | gap-order | 164/147 | 3 | const -[-16 -56 -86] +[128]; field -[] +[w2+17552] |
| `801efda0` BMAGIC/MAGIC113.EMI#3 |  | `001aa5a8` | `001aa2f4` | callee-align | 212/195 | 3 | const -[-16 -56 -86 7] +[]; field -[] +[w1+0 w1+1] |
| `801ef6a8` BMAGIC/MAGIC114.EMI#3 |  | `001ab254` | `001aafa0` | gap-align | 50/51 | 3 | const -[-2] +[-256 0xffffe190] |
| `801eec80` BMAGIC/MAGIC115.EMI#3 |  | `001ad044` | `001acd90` | gap-align | 81/81 | 3 | field -[] +[w2+0]; calls -[] +[3c5c]; globals 1 only PSX, 1 only PSP |
| `801f037c` BMAGIC/MAGIC126.EMI#3 |  | `001b87f8` | `001b8544` | gap-align | 61/65 | 3 | const -[0xfffff9ff 0x3ff] +[0x600 0xa00] |
| `801ef058` BMAGIC/MAGIC129.EMI#3 |  | `001baa5c` | `001ba7a8` | similarity | 185/179 | 3 | const -[-3 15 0x10000] +[]; globals 2 only PSX, 3 only PSP |
| `801eec88` BMAGIC/MAGIC132.EMI#3 |  | `001bec04` | `001be950` | gap-align | 76/74 | 3 | field -[] +[w2+0]; calls -[] +[3c5c]; globals 2 only PSX, 1 only PSP |
| `801eec7c` BMAGIC/MAGIC137.EMI#3 |  | `001bfae4` | `001bf830` | gap-align | 156/130 | 3 | const -[-3] +[14]; field -[] +[w2+0]; globals 10 only PSX, 1 only PSP |
| `801efdf8` BMAGIC/MAGIC137.EMI#3 |  | `001c0330` | `001c007c` | gap-align | 103/88 | 3 | const -[319] +[-32 351 383] |
| `801eec90` BMAGIC/MAGIC154.EMI#3 |  | `001c8af8` | `001c8844` | gap-align | 122/112 | 3 | const -[-3] +[14]; field -[] +[w2+0]; globals 12 only PSX, 1 only PSP |
| `801ef210` BMAGIC/MAGIC172.EMI#3 |  | `001d0678` | `001d03c4` | gap-align | 87/100 | 3 | const -[0xfffff9ff 0x3ff] +[0x600 0xa00]; field -[] +[w1+164] |
| `801ef148` BMAGIC/MAGIC173.EMI#3 |  | `001d2ad8` | `001d2824` | similarity | 142/134 | 3 | const -[0xfffff9ff 0x3ff] +[0x600 0xa00] |
| `801efa34` BMAGIC/MAGIC173.EMI#3 |  | `001d3164` | `001d2eb0` | callee-align | 497/428 | 3 | const -[] +[-16 109 145]; field -[] +[w2+0 w2+2 w2+4 w2+6] |
| `801ef048` BMAGIC/MAGIC213.EMI#3 |  | `001d3b98` | `001d38e4` | gap-align | 109/100 | 3 | calls -[] +[3c5c]; globals 3 only PSX, 1 only PSP |
| `801ef3cc` BMAGIC/MAGIC219.EMI#3 |  | `001d558c` | `001d52e4` | gap-align | 77/76 | 3 | const -[0xfffff9ff 0x3ff] +[0x600 0xa00] |
| `801eed18` BMAGIC/MAGIC220.EMI#3 |  | `001d6c34` | `001d698c` | similarity | 98/113 | 3 | const -[120] +[]; field -[] +[w2+0]; globals 3 only PSX, 2 only PSP |
| `801ef1e8` BMAGIC/MAGIC223.EMI#3 |  | `001d94f4` | `001d924c` | gap-align | 107/90 | 3 | const -[319] +[-32 351 383]; field -[] +[w4+5968] |
| `801efca0` BMAGIC/MAGIC225.EMI#3 |  | `001da76c` | `001da4c4` | gap-align | 76/61 | 3 | const -[319 37] +[-32 351] |
| `800c1e7c` BOSS/BOSS002.EMI#16 |  | `001dfab0` | `001df78c` | gap-align | 23/30 | 3 | const -[6] +[0xffffe730]; field -[] +[w1+1873]; calls -[] +[1267d0 1267f0]; globals 2 only PSX, 1 only PSP |
| `800c1940` BOSS/BOSS004.EMI#13 +5 copies |  | `001e510c` | `001e4de8` | gap-order | 27/12 (size 0.5x) | 3 | const -[191 0x1000] +[]; field -[w1+0 w1+1 w1+2 w1+3 w4+256] +[]; calls -[2bd0] +[] |
| `800c19c0` BOSS/BOSS008.EMI#16 |  | `001e0f84` | `001e0c60` | gap-align | 53/63 | 3 | const -[-8] +[-32]; calls -[] +[122b60] |
| `800c1f24` BOSS/BOSS013.EMI#16 |  | `001e28a0` | `001e257c` | gap-align | 53/53 | 3 | const -[] +[0xffffa250]; calls -[] +[122264 12cc34 Battle_InitPartyMember] |
| `800c1b40` BOSS/BOSS021.EMI#16 |  | `001e4dec` | `001e4ac8` | gap-align | 44/59 | 3 | const -[-3] +[5 6] |
| `800c18e4` BOSS/BOSS028.EMI#16 |  | `001e7088` | `001e6d70` | gap-align | 24/29 | 3 | const -[] +[0xe4048]; field -[] +[w1+134]; calls -[] +[13e400] |
| `800c20cc` BOSS/BOSS033.EMI#16 |  | `001e8b18` | `001e8800` | gap-order | 134/123 | 3 | const -[112 0x10000] +[14]; field -[w4+12 w4+8] +[]; globals 7 only PSX, 1 only PSP |
| `800c1e0c` BOSS/BOSS054.EMI#16 |  | `001ed018` | `001ecd00` | gap-align | 91/86 | 3 | const -[-3] +[]; field -[] +[w1+164]; globals 2 only PSX, 2 only PSP |
| `800c18a8` BOSS/BOSS055.EMI#16 |  | `001ed26c` | `001ecf54` | gap-align | 44/52 | 3 | const -[] +[0x7cfc]; calls -[] +[13e400] |
| `801ef144` BATTLE/BATL_END.EMI#0 |  | `00149f70` | `00149d30` | similarity | 108/100 | 2 | const -[] +[0x7608]; calls -[] +[12cfa0]; globals 6 only PSX, 1 only PSP |
| `801eec7c` BATTLE/BATL_OVR.EMI#0 |  | `0014af60` | `0014ad20` | similarity | 135/108 | 2 | calls -[] +[144170]; globals 4 only PSX, 2 only PSP |
| `80094e88` BATTLE/BATTLE.EMI#15 |  | `00128c0c` | `001288c4` | callee-align | 72/64 | 2 | const -[0x20002] +[]; globals 9 only PSX, 1 only PSP |
| `8009975c` BATTLE/BATTLE.EMI#15 |  | `0012d30c` | `0012cfd8` | gap-align | 52/63 | 2 | const -[] +[0xe4048]; field -[] +[w1+52]; globals 2 only PSX, 1 only PSP |
| `80099c7c` BATTLE/BATTLE.EMI#15 |  | `0012ea28` | `0012e6fc` | similarity | 172/140 | 2 | const -[125] +[0x2710] |
| `8009af28` BATTLE/BATTLE.EMI#15 |  | `0012f108` | `0012ede0` | similarity | 17/22 | 2 | const -[104] +[232] |
| `8009bee0` BATTLE/BATTLE.EMI#15 |  | `0012fa60` | `0012f738` | similarity | 33/32 | 2 | field -[w2+0] +[]; calls -[] +[Battle_CalcDamage] |
| `800a2298` BATTLE/BATTLE.EMI#15 |  | `00136ad4` | `001367f8` | similarity-weak | 276/307 | 2 | const -[69] +[6]; field -[] +[w1+216 w1+228 w1+231 w1+4 w1+6 w1+7 w2+4]; calls -[] +[12ccb0] |
| `800a5ec0` BATTLE/BATTLE.EMI#15 |  | `0013a730` | `0013a47c` | caller-vote | 208/177 | 2 | const -[-3 64] +[12]; field -[w1+0] +[w1+164] |
| `800a8c10` BATTLE/BATTLE.EMI#15 |  | `00141948` | `00141714` | similarity | 147/150 | 2 | const -[] +[0xe4048]; field -[] +[w2+22]; globals 1 only PSX, 1 only PSP |
| `800a9fd8` BATTLE/BATTLE.EMI#15 |  | `001423d4` | `001421a0` | caller-vote | 105/84 | 2 | const -[] +[0xe4048]; field -[] +[w1+116 w1+117 w1+118 w1+119]; globals 2 only PSX, 1 only PSP |
| `800aa21c` BATTLE/BATTLE.EMI#15 |  | `00146f30` | `00146cec` | similarity | 75/62 | 2 | const -[0x1fe0] +[6] |
| `800aa404` BATTLE/BATTLE.EMI#15 |  | `001470cc` | `00146e88` | callee-align | 55/46 | 2 | const -[0x1fe0] +[6] |
| `800ad3c0` BATTLE/BATTLE.EMI#15 |  | `0014c368` | `0014c2e4` | gap-align | 148/149 | 2 | const -[] +[0x45a8 0x7650] |
| `800aecdc` BATTLE/BATTLE.EMI#15 |  | `0014d460` | `0014d3d4` | similarity | 84/73 | 2 | const -[12] +[6] |
| `801d32fc` BATTLE/BATTLE.EMI#3 |  | `00120ddc` | `-` | similarity | 170/181 | 2 | const -[-3] +[]; field -[] +[w1-624 w1+1 w1+148 w1+2 w1+216 w1+232 w1+3 w1+5 ...]; globals 9 only PSX, 1 only PSP |
| `801d4b08` BATTLE/BATTLE.EMI#3 |  | `0011e434` | `0011e0a4` | gap-order | 121/114 | 2 | const -[-3 0xffff0000] +[]; field -[] +[w1+1 w1+2 w1+218 w4+240] |
| `801d653c` BATTLE/BATTLE.EMI#3 |  | `0011f2f4` | `0011ef64` | gap-order | 122/121 | 2 | const -[] +[27]; field -[] +[w1-624 w1+1 w1+2 w1+20 w1+28 w1+68 w2+8]; calls -[] +[970fc] unpaired 2/0 |
| `801d6c90` BATTLE/BATTLE.EMI#3 |  | `0011f98c` | `0011f5fc` | similarity | 43/40 | 2 | calls -[] +[95118] unpaired 1/0 |
| `801d6df0` BATTLE/BATTLE.EMI#3 |  | `0011fabc` | `0011f72c` | similarity | 31/31 | 2 | calls -[] +[95118] unpaired 1/0 |
| `801d6ef8` BATTLE/BATTLE.EMI#3 |  | `0011fbb4` | `0011f824` | gap-order | 109/122 | 2 | const -[] +[0x3f90]; field -[] +[w1+16 w1+2 w2+128]; globals 2 only PSX, 1 only PSP |
| `801d7ce0` BATTLE/BATTLE.EMI#3 |  | `001219a8` | `00121628` | gap-align | 102/100 | 2 | const -[0x7800 63] +[9]; field -[w1+0 w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 ...] +[w1+32] |
| `801d8ab4` BATTLE/BATTLE.EMI#3 |  | `00123e8c` | `00123b0c` | similarity | 196/189 | 2 | const -[248 32] +[]; field -[] +[w2+40 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801d8dc8` BATTLE/BATTLE.EMI#3 |  | `00124184` | `00123e04` | similarity | 314/290 | 2 | const -[248 32] +[0x7474 0x2000]; field -[] +[w2+40] |
| `801d94a4` BATTLE/BATTLE.EMI#3 |  | `001222a8` | `00121f28` | gap-order | 104/111 | 2 | const -[] +[0x7468 0x746c]; field -[w1+12 w1+13 w1+4 w1+5 w1+6 w2+10 w2+14 w2+16 ...] +[w4+2000] |
| `801da66c` BATTLE/BATTLE.EMI#3 |  | `00122fb4` | `00122c34` | gap-align | 71/57 | 2 | const -[-3 5] +[] |
| `801db020` BATTLE/BATTLE.EMI#3 |  | `00124818` | `00124498` | gap-align | 158/127 | 2 | const -[-3] +[]; globals 2 only PSX, 1 only PSP |
| `801db3ac` BATTLE/BATTLE.EMI#3 |  | `00124b0c` | `0012478c` | gap-align | 19/20 | 2 | const -[-3] +[]; globals 1 only PSX, 1 only PSP |
| `801dcec0` BATTLE/BATTLE.EMI#3 |  | `001259dc` | `0012565c` | gap-align | 50/46 | 2 | const -[-3] +[10] |
| `801dd410` BATTLE/BATTLE.EMI#3 |  | `00125e78` | `00125af8` | gap-align | 71/64 | 2 | const -[25] +[100]; field -[] +[w1+238 w2+8 w4+17424] |
| `801ddfb4` BATTLE/BATTLE.EMI#3 |  | `001268b8` | `00126538` | gap-order | 41/45 | 2 | calls -[126768] +[Inventory_Add] |
| `801de074` BATTLE/BATTLE.EMI#3 |  | `00126974` | `001265f4` | gap-align | 17/13 | 2 | const -[25] +[100] |
| `801de0c0` BATTLE/BATTLE.EMI#3 |  | `001269b0` | `00126630` | gap-align | 17/13 | 2 | const -[25] +[100] |
| `801e42c0` BATTLE/BATTLE.EMI#3 |  | `0013f744` | `0013f4e0` | gap-align | 61/49 | 2 | const -[25] +[100]; field -[] +[w1+238 w2+224] |
| `801e4794` BATTLE/BATTLE.EMI#3 |  | `0013fb74` | `0013f910` | signature | 32/36 | 2 | calls -[] +[3c5c] |
| `801e63f4` BATTLE/BATTLE.EMI#3 |  | `001449e0` | `00144794` | gap-align | 75/77 | 2 | const -[12 0xfc00] +[-32]; field -[] +[w4+2000] |
| `801e6538` BATTLE/BATTLE.EMI#3 |  | `00144b14` | `001448c8` | gap-align | 70/73 | 2 | const -[12 0xfc00] +[-32]; field -[] +[w4+2000] |
| `801ef78c` BMAGIC/MAGIC001.EMI#0 |  | `00150ac4` | `00150a24` | gap-align | 149/129 | 2 | const -[-3] +[14] |
| `801f014c` BMAGIC/MAGIC001.EMI#0 |  | `001512f4` | `00151254` | gap-align | 51/60 | 2 | const -[] +[-624 0xffffe190]; field -[] +[w4+172] |
| `801eec78` BMAGIC/MAGIC002.EMI#0 |  | `0015165c` | `001515bc` | gap-align | 88/55 | 2 | const -[5] +[10] |
| `801ef1fc` BMAGIC/MAGIC002.EMI#0 |  | `00151990` | `001518f0` | gap-align | 636/551 | 2 | const -[12] +[0xfffff6c3]; field -[w1+0 w2+0 w2+2 w2+48 w2+50 w2+98] +[w1+112 w1+124 w1+125 w1+136 w1+148 w1+149 w2+52 w2+54 ...] |
| `801efd4c` BMAGIC/MAGIC003.EMI#3 |  | `00153228` | `00153188` | callee-align | 91/78 | 2 | const -[-3] +[14] |
| `801f03f0` BMAGIC/MAGIC003.EMI#3 |  | `001535b8` | `00153518` | gap-align | 85/76 | 2 | const -[15] +[180 192] |
| `801ef490` BMAGIC/MAGIC004.EMI#3 |  | `001544ac` | `0015440c` | gap-align | 128/95 | 2 | const -[137 157] +[]; field -[] +[w4+5968] |
| `801efaf4` BMAGIC/MAGIC004.EMI#3 |  | `001549b8` | `00154918` | gap-align | 39/40 | 2 | const -[12] +[]; globals 1 only PSX, 1 only PSP |
| `801efad4` BMAGIC/MAGIC008.EMI#0 |  | `0015cff8` | `0015cf58` | similarity-weak | 108/92 | 2 | const -[-3] +[14]; field -[] +[w4+5968] |
| `801f05fc` BMAGIC/MAGIC008.EMI#0 |  | `0015d9cc` | `0015d92c` | signature | 64/61 | 2 | const -[0x28000 0x18000] +[] |
| `801ef2b4` BMAGIC/MAGIC015.EMI#0 |  | `00159698` | `001595f8` | gap-align | 170/155 | 2 | const -[-3] +[14] |
| `801ef6b0` BMAGIC/MAGIC015.EMI#0 |  | `00159a00` | `00159960` | gap-align | 150/130 | 2 | const -[-3] +[14] |
| `801ef348` BMAGIC/MAGIC021.EMI#3 +2 copies |  | `0015462c` | `0015458c` | gap-align | 119/105 | 2 | const -[319] +[-32 351] |
| `801ef134` BMAGIC/MAGIC039.EMI#3 |  | `00161a9c` | `001619fc` | signature | 39/37 | 2 | calls -[] +[3c5c] |
| `801eee14` BMAGIC/MAGIC041.EMI#3 |  | `00163958` | `001638b8` | gap-align | 33/32 | 2 | calls -[] +[3c5c] |
| `801eeef0` BMAGIC/MAGIC043.EMI#3 |  | `00165608` | `00165568` | gap-align | 37/36 | 2 | calls -[] +[3c5c] |
| `801efc48` BMAGIC/MAGIC046.EMI#3 |  | `00169480` | `001693e0` | callee-align | 152/140 | 2 | const -[6] +[512 60]; field -[] +[w4+2000] |
| `801eec70` BMAGIC/MAGIC048.EMI#3 |  | `00169708` | `00169668` | gap-align | 75/66 | 2 | const -[-3] +[-24] |
| `801eecbc` BMAGIC/MAGIC052.EMI#3 |  | `0016adf4` | `0016ad54` | gap-align | 82/77 | 2 | const -[] +[10 9]; field -[] +[w1+0] |
| `801ef4c0` BMAGIC/MAGIC056.EMI#3 |  | `0016ed58` | `0016ecb8` | callee-align | 325/308 | 2 | const -[64] +[128 0xc00 96]; field -[w1+12 w1+13 w1+16 w1+17 w1+18 w1+24 w1+25 w1+28 ...] +[] |
| `801ef134` BMAGIC/MAGIC057.EMI#3 |  | `0016f940` | `0016f8a0` | gap-align | 146/123 | 2 | const -[0x20000 0x30000 0xfffd0000] +[] |
| `801eed0c` BMAGIC/MAGIC059.EMI#3 |  | `00171c88` | `00171be8` | callee-align | 247/209 | 2 | const -[120 0x40000] +[]; field -[] +[w2+0]; calls -[] +[125d70]; globals 8 only PSX, 1 only PSP |
| `801f0544` BMAGIC/MAGIC059.EMI#3 |  | `00171b9c` | `00171afc` | callee-align | 63/59 | 2 | const -[280 320] +[]; field -[] +[w4+5968] |
| `801ef2dc` BMAGIC/MAGIC060.EMI#0 |  | `00172fa0` | `-` | gap-align | 125/122 | 2 | const -[] +[0x4deb 0x4df8] |
| `801ef8e4` BMAGIC/MAGIC060.EMI#0 |  | `001732fc` | `001733b8` | gap-align | 106/105 | 2 | const -[-64] +[-512]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[] |
| `801f07c8` BMAGIC/MAGIC060.EMI#0 |  | `00173a78` | `00173cfc` | gap-align | 29/26 | 2 | calls -[] +[3c5c] |
| `801ef8f4` BMAGIC/MAGIC064.EMI#3 |  | `00178b7c` | `001788bc` | similarity-weak | 446/300 | 2 | const -[] +[14]; field -[] +[w1+0 w1+1 w1+11 w1+2 w1+36 w1+39 w1+5 w1+6 ...]; calls -[3708] +[1263f8] unpaired 0/1; globals 13 only PSX, 1 only PSP |
| `801f04a0` BMAGIC/MAGIC064.EMI#3 |  | `00178318` | `00178058` | signature | 61/58 | 2 | const -[0x19800 0x32000] +[] |
| `801eeff8` BMAGIC/MAGIC066.EMI#3 |  | `0017a078` | `00179dc4` | gap-align | 18/17 | 2 | const -[0xffff4000] +[0xc000] |
| `801ef2dc` BMAGIC/MAGIC073.EMI#3 +1 copies |  | `00181eac` | `00181bf8` | signature | 54/51 | 2 | calls -[] +[3c5c] |
| `801ef1dc` BMAGIC/MAGIC083.EMI#3 |  | `0018b300` | `0018b04c` | gap-align | 58/53 | 2 | calls -[] +[3c5c] |
| `801eee44` BMAGIC/MAGIC086.EMI#3 |  | `0018cff8` | `0018cd44` | signature | 34/34 | 2 | calls -[] +[3c5c] |
| `801eed04` BMAGIC/MAGIC087.EMI#3 |  | `0018ec34` | `0018e980` | callee-align | 203/178 | 2 | const -[] +[0xe4048]; field -[] +[w1+131 w2+0]; globals 8 only PSX, 1 only PSP |
| `801eeef4` BMAGIC/MAGIC088.EMI#3 |  | `0018f67c` | `0018f3c8` | signature | 39/38 | 2 | calls -[] +[3c5c] |
| `801ef1f0` BMAGIC/MAGIC092.EMI#3 |  | `001917d4` | `00191520` | gap-align | 74/71 | 2 | field -[] +[w4+5968]; calls -[] +[3c5c] |
| `801eed04` BMAGIC/MAGIC093.EMI#3 |  | `001939b4` | `00193700` | callee-align | 190/154 | 2 | const -[120] +[]; field -[] +[w1+3 w2+0 w4+116]; globals 8 only PSX, 1 only PSP |
| `801ef108` BMAGIC/MAGIC093.EMI#3 |  | `00192e50` | `00192b9c` | gap-align | 73/70 | 2 | field -[] +[w4+5968]; calls -[] +[3c5c] |
| `801eef7c` BMAGIC/MAGIC094.EMI#3 |  | `0019474c` | `00194498` | signature | 54/55 | 2 | calls -[] +[3c5c] |
| `801eee60` BMAGIC/MAGIC095.EMI#3 |  | `00195228` | `00194f74` | signature | 56/59 | 2 | calls -[] +[3c5c] |
| `801ef1fc` BMAGIC/MAGIC095.EMI#3 |  | `00195648` | `00195394` | callee-align | 356/308 | 2 | const -[41] +[215 512]; field -[] +[w1+8] |
| `801ef1a4` BMAGIC/MAGIC097.EMI#3 |  | `00197e58` | `00197ba4` | gap-align | 81/76 | 2 | calls -[] +[3c5c] |
| `801ef1b4` BMAGIC/MAGIC098.EMI#3 |  | `00199150` | `00198e9c` | gap-align | 81/76 | 2 | calls -[] +[3c5c] |
| `801eec88` BMAGIC/MAGIC099.EMI#3 |  | `0019a23c` | `00199f88` | gap-align | 117/100 | 2 | calls -[] +[3c5c] |
| `801ef1dc` BMAGIC/MAGIC100.EMI#3 |  | `0019c08c` | `0019bdd8` | gap-align | 81/99 | 2 | const -[0xfffffbff 0x7ff] +[0x400 0xc00] |
| `801ef2d4` BMAGIC/MAGIC108.EMI#3 |  | `001a5f38` | `001a5c84` | callee-align | 172/161 | 2 | const -[-96 37 64 96] +[256 384]; field -[] +[w2+17552] |
| `801ef148` BMAGIC/MAGIC110.EMI#3 |  | `001a7e2c` | `001a7b78` | callee-align | 148/134 | 2 | const -[319] +[-32 351] |
| `801eec7c` BMAGIC/MAGIC111.EMI#3 |  | `001a8770` | `001a84bc` | gap-align | 170/145 | 2 | const -[-3] +[14] |
| `801ef330` BMAGIC/MAGIC111.EMI#3 |  | `001a8f10` | `001a8c5c` | callee-align | 282/247 | 2 | const -[319] +[-32 351] |
| `801ef970` BMAGIC/MAGIC111.EMI#3 |  | `001a8e30` | `001a8b7c` | similarity | 71/56 | 2 | const -[319] +[-32 351] |
| `801ef280` BMAGIC/MAGIC114.EMI#3 |  | `001abc70` | `001ab9bc` | callee-align | 224/246 | 2 | const -[-3 -64] +[] |
| `801efa80` BMAGIC/MAGIC114.EMI#3 |  | `001ac048` | `001abd94` | callee-align | 206/200 | 2 | const -[0xa0000 0xa8000] +[]; calls -[] +[125d70] |
| `801f0004` BMAGIC/MAGIC114.EMI#3 |  | `001ac368` | `001ac0b4` | callee-align | 291/296 | 2 | const -[47 0x80000] +[188]; calls -[] +[125d70] |
| `801eec90` BMAGIC/MAGIC117.EMI#0 |  | `001ae0fc` | `001ade48` | signature | 33/29 | 2 | calls -[] +[3c5c] |
| `801ef06c` BMAGIC/MAGIC117.EMI#0 +1 copies |  | `00157050` | `00156fb0` | gap-align | 29/26 | 2 | calls -[] +[3c5c] |
| `801ef44c` BMAGIC/MAGIC120.EMI#3 |  | `001b0684` | `001b03d0` | callee-align | 503/684 | 2 | const -[0xfc0] +[32 36]; field -[] +[w1+14 w1+20 w1+21 w1+22 w2+16 w2+18 w2+24] |
| `801efd20` BMAGIC/MAGIC120.EMI#3 |  | `001af9b8` | `001af704` | gap-align | 39/40 | 2 | const -[12] +[]; globals 1 only PSX, 1 only PSP |
| `801f0274` BMAGIC/MAGIC120.EMI#3 |  | `001afe28` | `001afb74` | gap-align | 125/104 | 2 | const -[280] +[]; calls -[] +[125d70]; globals 3 only PSX, 1 only PSP |
| `801f08ac` BMAGIC/MAGIC120.EMI#3 |  | `001b00e4` | `001afe30` | gap-align | 43/39 | 2 | const -[280] +[]; globals 2 only PSX, 1 only PSP |
| `801efba8` BMAGIC/MAGIC122.EMI#3 |  | `001b3b98` | `001b38e4` | gap-align | 416/348 | 2 | const -[] +[-14 6]; field -[] +[w4+2000] |
| `801ef960` BMAGIC/MAGIC125.EMI#3 |  | `001b6f88` | `001b6cd4` | gap-order | 75/77 | 2 | const -[0x7f8] +[6] |
| `801ef000` BMAGIC/MAGIC131.EMI#3 |  | `001bc554` | `001bc2a0` | gap-align | 65/59 | 2 | field -[] +[w2+0]; globals 3 only PSX, 2 only PSP |
| `801f030c` BMAGIC/MAGIC131.EMI#3 |  | `001bd620` | `001bd36c` | gap-align | 71/56 | 2 | const -[319] +[-32 351] |
| `801eef78` BMAGIC/MAGIC143.EMI#3 |  | `001c18d4` | `001c1620` | signature | 46/42 | 2 | calls -[] +[3c5c] |
| `801eed04` BMAGIC/MAGIC144.EMI#3 |  | `001c26bc` | `001c2408` | gap-align | 81/79 | 2 | const -[120] +[]; field -[] +[w2+0]; globals 3 only PSX, 1 only PSP |
| `801ef078` BMAGIC/MAGIC144.EMI#3 |  | `001c29bc` | `001c2708` | gap-align | 34/33 | 2 | calls -[] +[3c5c] |
| `801eecb0` BMAGIC/MAGIC151.EMI#3 |  | `001c5b0c` | `001c5858` | gap-align | 66/70 | 2 | const -[] +[448]; field -[] +[w4+5968]; calls -[] +[12cc34]; globals 1 only PSX, 1 only PSP |
| `801ef104` BMAGIC/MAGIC154.EMI#3 |  | `001c8ed4` | `001c8c20` | gap-align | 48/50 | 2 | const -[12] +[]; globals 1 only PSX, 1 only PSP |
| `801eec84` BMAGIC/MAGIC161.EMI#3 |  | `001ca504` | `001ca250` | gap-align | 126/111 | 2 | const -[-3] +[14]; field -[] +[w4+5968] |
| `801ef1dc` BMAGIC/MAGIC161.EMI#3 |  | `001ca990` | `001ca6dc` | gap-align | 127/115 | 2 | const -[319] +[-32 351] |
| `801eefa0` BMAGIC/MAGIC162.EMI#3 +6 copies |  | `00174504` | `0017423c` | gap-align | 37/36 | 2 | calls -[] +[3c5c] |
| `801eee88` BMAGIC/MAGIC167.EMI#3 +1 copies |  | `00184924` | `00184670` | gap-align | 37/35 | 2 | calls -[] +[3c5c] |
| `801ef444` BMAGIC/MAGIC169.EMI#3 |  | `001cf6b4` | `001cf400` | signature | 42/40 | 2 | calls -[] +[3c5c] |
| `801ef4ec` BMAGIC/MAGIC173.EMI#3 |  | `001d2cfc` | `001d2a48` | callee-align | 313/282 | 2 | const -[12] +[109] |
| `801ef23c` BMAGIC/MAGIC213.EMI#3 |  | `001d3dd0` | `001d3b1c` | gap-align | 52/50 | 2 | field -[w1+9] +[w1+4]; calls -[] +[139508 3d84] |
| `801eecdc` BMAGIC/MAGIC216.EMI#0 |  | `001d42b4` | `001d4000` | gap-align | 143/151 | 2 | const -[-3] +[]; field -[] +[w1-686]; globals 2 only PSX, 1 only PSP |
| `801eed18` BMAGIC/MAGIC223.EMI#3 |  | `001d9a7c` | `001d97d4` | similarity | 133/139 | 2 | const -[120] +[]; field -[] +[w2+0 w2+32]; globals 2 only PSX, 1 only PSP |
| `800c1dd4` BOSS/BOSS002.EMI#16 |  | `001dfa00` | `001df6dc` | gap-align | 20/26 | 2 | const -[6] +[]; field -[] +[w1+1873]; calls -[] +[1267d0 1267f0]; globals 1 only PSX, 1 only PSP |
| `800c2f74` BOSS/BOSS025.EMI#13 |  | `001e62e0` | `001e5fbc` | similarity | 29/30 | 2 | const -[] +[0xffffe730 280] |
| `800c18e4` BOSS/BOSS033.EMI#16 |  | `001e8534` | `001e821c` | gap-align | 88/65 | 2 | const -[-3] +[14] |
| `800c1db0` BOSS/BOSS033.EMI#16 |  | `001e8840` | `001e8528` | gap-align | 25/31 | 2 | const -[6] +[]; field -[] +[w1+5]; calls -[] +[1267d0 1267f0]; globals 1 only PSX, 1 only PSP |
| `800c1f60` BOSS/BOSS034.EMI#16 |  | `001e9730` | `001e9418` | gap-align | 43/78 (size 1.8x) | 2 | const -[] +[0x8000 5 6 0x2000]; field -[w1+0] +[w1+13]; calls -[] +[125cd4] unpaired 1/0 |
| `800c1bb4` BOSS/BOSS035.EMI#16 |  | `001ea244` | `001e9f2c` | gap-align | 9/27 (size 2.8x) | 2 | const -[] +[5 6] |
| `800c1900` BOSS/BOSS051.EMI#16 |  | `001ebf00` | `001ebbe8` | gap-align | 32/37 | 2 | const -[] +[0xe4048]; field -[] +[w1+134]; globals 1 only PSX, 1 only PSP |
| `800c1b14` BOSS/BOSS051.EMI#16 |  | `001ec154` | `001ebe3c` | gap-align | 50/53 | 2 | const -[] +[0xe4048]; field -[] +[w1+134]; globals 1 only PSX, 1 only PSP |
| `800c1df8` BOSS/BOSS052.EMI#16 |  | `001ec778` | `001ec460` | gap-align | 28/33 | 2 | const -[] +[0xe4048]; field -[] +[w1+134]; globals 1 only PSX, 1 only PSP |

#### Field, menus, items and system: 908 differing pairs

| PSX (file) | name | PSP-JP | PSP-EU | paired by | instr. PSX/PSP | score | differs in |
|---|---|---|---|---|---|---|---|
| `8015096c` SLPS_009.90 | MsgBox_Step | `0000b78c` | `-` | similarity | 328/337 | 16 | const -[10 11 12 13 18 20 22 9] +[30 0x10004]; field -[] +[w1+120 w1+121 w4+0]; globals 6 only PSX, 6 only PSP |
| `8014e974` SLPS_009.90 | Boot_Init | `00004e1c` | `00005784` | gap-align | 97/100 | 10 | const -[0x10b0] +[0x30b0 0xe3804]; field -[] +[w4-11312]; calls -[94cec dec e34] +[970fc] unpaired 1/6; globals 5 only PSX, 1 only PSP |
| `8014c3c8` SLPS_009.90 | Actor_Task | `00001c20` | `0000258c` | callee-align | 520/509 | 9 | const -[127 0x3c000 369 449 96] +[-64 0x3ff 20 288 305 32 385]; field -[w1-1 w1-2 w1-3 w4+0 w4+20 w4+23 w4+24 w4+27 ...] +[w1+1 w1+2 w1+3 w4+2000 w4+5968]; c... |
| `8014aac8` SLPS_009.90 | Main_FrameLoop | `00000280` | `00000bf0` | similarity | 67/57 | 8 | const -[112] +[0x4e1c]; calls -[570 977f0 c48] +[] unpaired 1/4 |
| `80178d40` SLPS_009.90 | rcos | `0009c760` | `0009c4e8` | caller-vote | 35/20 (size 0.6x) | 7 | const -[0x400 0x401 0x801 0xc00 0xc01 0xfff] +[0x40490fdb 0x45800000 0x3a000000]; calls -[] +[] unpaired 0/1 |
| `801ef96c` ETC/SHOP.EMI#8 | Card_Open (label) | `00098cf8` | `00098a74` | similarity | 109/108 | 6 | const -[0x1e000] +[0x6964 0x7562 58 0x30307562 0x30310000]; field -[] +[w2+4 w4+0] |
| `8017aab0` SLPS_009.90 | ratan2 | `0009c864` | `0009c5ec` | caller-vote | 82/37 (size 0.5x) | 5 | const -[0x400 0x800 0x7fe00000] +[]; calls -[] +[] unpaired 0/5 |
| `8014af98` SLPS_009.90 | Packet_FrameReset | `00000778` | `-` | gap-order | 52/52 | 4 | const -[48 56] +[58]; globals 4 only PSX, 1 only PSP |
| `8015ad34` SLPS_009.90 | Text_DrawImmediate | `00014848` | `000146d4` | caller-vote | 126/160 | 4 | const -[] +[19 0x10004]; field -[] +[w2+0 w4+0]; globals 3 only PSX, 2 only PSP |
| `80165434` SLPS_009.90 | Char_RecalcStats | `000b9cb8` | `000b9ae8` | caller-vote | 165/199 | 4 | const -[] +[10 28 60]; field -[w4+28 w4+31 w4+32 w4+35 w4+36 w4+39 w4+40 w4+43 ...] +[w2+0 w2+10 w2+12 w2+14 w2+16 w2+18 w2+2 w2+22 ...]; globals 5 only PSX,... |
| `8017917c` SLPS_009.90 | VectorNormalS | `0009db74` | `0009d8b4` | caller-vote | 5/87 (size 14.7x) | 4 | field -[] +[w2+0 w2+2 w2+4]; calls -[] +[] unpaired 0/5 |
| `80151f4c` SLPS_009.90 | Font_MapGlyph | `0000bfd0` | `-` | gap-align | 874/576 | 3 | const -[192 20 6] +[] |
| `8015e908` SLPS_009.90 | SE_Play | `00095b30` | `000958ac` | caller-vote | 639/8 (size 0.0x) | 3 | const -[127 128 0xf00 0x17ff 0xf000] +[] |
| `801794f0` SLPS_009.90 | PushMatrix | `0009d460` | `0009d1a0` | caller-vote | 12/16 | 3 | const -[0x7200 640] +[0x790] |
| `8017a0b0` SLPS_009.90 | RotMatrix | `0009cdb0` | `-` | caller-vote | 129/85 | 3 | const -[0xfff] +[0xffffb310 0xffffb2f0 0xffffb2d0]; field -[w2+10 w2+12 w2+14 w2+16 w2+6 w2+8] +[] |
| `8017cc50` SLPS_009.90 | SetDrawMode | `0009c7b0` | `0009c538` | caller-vote | 21/20 | 3 | const -[] +[0x40490fdb 0x45800000 0x3a000000]; field -[w1+3 w4+4 w4+8] +[]; calls -[] +[] unpaired 0/1 |
| `8017e6a4` SLPS_009.90 | get_alarm | `000a2d60` | `000a2a2c` | similarity-weak | 79/65 | 3 | const -[0x401 0x1000000 0x800 0x2000000 63 0xf0000] +[12 7]; field -[] +[w2+10 w2+8] |
| `8017ed4c` SLPS_009.90 | Rand | `000a2cf0` | `000a29bc` | caller-vote | 3/13 (size 3.5x) | 3 | const -[160 47] +[0x269ec3 0x7fff] |
| `801c0114` ETC/GAME.EMI#0 | Encounter_PlaceParty | `000d8d68` | `000d8a80` | callee-align | 182/209 | 2 | const -[320] +[]; field -[] +[w2+0 w2+192 w4+0 w4+4]; globals 1 only PSX, 2 only PSP |
| `801c23f8` ETC/GAME.EMI#0 | Battle_InitPartyContexts | `000d7c20` | `000d7938` | gap-align | 131/130 | 2 | const -[320 0x10000] +[20]; field -[] +[w1+0] |
| `8014d184` SLPS_009.90 | Actor_UpdateScreenPos | `00002450` | `00002dbc` | callee-align | 130/139 | 2 | const -[55] +[57]; field -[] +[w1+5520 w4+5968] |
| `8016c4e4` SLPS_009.90 | SsSepReplay | `0009e628` | `0009e2fc` | caller-vote | 10/70 (size 6.5x) | 2 | calls -[] +[] unpaired 0/3 |
| `8017b420` SLPS_009.90 | SetLineF3 | `0009fcd8` | `0009f9ac` | gap-order | 8/3 (size 0.4x) | 2 | const -[0x55555555 5] +[]; field -[w1+3 w4+20] +[] |
| `8017b460` SLPS_009.90 | SetLineF4 | `0009fcf0` | `0009f9c4` | caller-vote | 8/3 (size 0.4x) | 2 | const -[0x55555555 6] +[]; field -[w1+3 w4+24] +[] |
| `8017ecdc` SLPS_009.90 | strcat | `002c57d0` | `002c5694` | caller-vote | 3/22 (size 5.8x) | 2 | const -[160 21] +[]; field -[] +[w1+0] |
| `8017ed1c` SLPS_009.90 | strncpy | `002c58f8` | `002c57bc` | caller-vote | 3/24 (size 6.2x) | 2 | const -[160 26] +[]; field -[] +[w1+0] |
| `8017ed2c` SLPS_009.90 | strlen | `002c5870` | `002c5734` | caller-vote | 3/9 (size 2.5x) | 2 | const -[160 27] +[]; field -[] +[w1+0] |
| `8017ed6c` SLPS_009.90 | sprintf | `002c54c4` | `002c5388` | caller-vote | 3/30 (size 7.8x) | 2 | const -[] +[112 0x7fffffff 520]; field -[] +[w1+0] |
| `801e1ac0` ETC/START.EMI#8 |  | `000c12b4` | `000c10a8` | similarity-weak | 536/255 (size 0.5x) | 23 | const -[-8 106 13 14 168 176 197 202 ...] +[0xfffff9d0 189 469]; field -[] +[w1+2 w1+4 w1+5 w2+16 w2+4]; calls -[6400 b7d64 c0b78] +[3ff74] unpaired 1/2 |
| `801f9f18` SCENARIO/SCENA13.EMI#0 |  | `002ab8b4` | `002ab6f0` | similarity | 948/461 (size 0.5x) | 23 | const -[0xfffff204 -748 -80 -902 100 111 134 135 ...] +[]; field -[] +[w2-944]; calls -[2a9d2c 951c0 9528c 95c6c e560] +[d8270] |
| `801a9b84` ETC/GAME.EMI#0 |  | `0008d788` | `0008d4e0` | gap-align | 61/82 | 22 | const -[169 170 171 49] +[-167 112 41]; calls -[] +[229de0 22f6f0 2486bc 259344 25f75c 260548 2619e4 262d0c] unpaired 8/0 |
| `8014f708` SLPS_009.90 |  | `00009978` | `-` | similarity-weak | 315/436 | 21 | const -[20 22 0x7800 5 6 7] +[102 175 176 177 178 216 27 28 ...]; calls -[] +[] unpaired 0/1 |
| `801ded34` WORLD00/AREA030.EMI#4 |  | `000e6ac4` | `000e67fc` | similarity-weak | 1092/724 | 20 | const -[0xffff8000 128 0x13ffff 0x150000 192 208 224 253 ...] +[117]; field -[w1+14 w1+2 w1+5 w2+52 w2+56 w4+20] +[w1-6064 w1-6383 w1+29 w1+32 w2+16 w2+18 w2... |
| `801a8cf8` ETC/GAME.EMI#0 |  | `0008db10` | `0008d868` | similarity | 320/166 (size 0.5x) | 19 | const -[108 0x110000 127 129 133 30 32 0x50000 ...] +[]; field -[w1+0] +[w1+5904]; calls -[3554 97fc8 File_LoadDone Flag_Clear dd108] +[] unpaired 3/0 |
| `801f7db4` SCENARIO/SCENA09.EMI#0 |  | `002965a8` | `002963e4` | similarity-weak | 353/610 (size 1.7x) | 19 | const -[] +[-7 110 12 0xa80 37 39 41 46 ...]; field -[] +[w1+1 w1+2 w1+3 w1+5904 w2-240 w4+60]; calls -[] +[107174 d8270 d82b8] |
| `801fa1f0` SCENARIO/SCENA09.EMI#0 |  | `00298d6c` | `00298ba8` | similarity | 1498/838 (size 0.6x) | 18 | const -[14 154 18 23 33 36 38 44 ...] +[0x650]; field -[w1+0 w1+11 w2+46 w2+48] +[w1+2 w1+5904]; calls -[97fc8 SE_Play] +[107174 952f0 d7ef0 d8270 d82b8] unp... |
| `801dd52c` ETC/SHOP.EMI#0 |  | `001f9eac` | `-` | gap-align | 457/490 | 17 | const -[11] +[0xffff81a8 0xffff819c 0x781c 40 88]; field -[] +[w1+122 w1+1666 w4+0]; calls -[] +[9a620 9a80c 9a940 9ab6c 9b770]; globals 2 only PSX, 1 only PSP |
| `801f3800` WORLD04/AREA189.EMI#13 |  | `0007d3ec` | `0007d124` | similarity | 568/463 | 17 | const -[-256 -512 0x7900 320 486 5 679] +[0xffffd800 -288 -32 0xffffec00 -72 10 0x400 0x600 ...]; field -[] +[w2+17552]; globals 6 only PSX, 3 only PSP |
| `801d7090` ETC/SHOP.EMI#0 |  | `001f3a3c` | `001f3728` | gap-order | 93/171 (size 1.8x) | 16 | const -[0x3fc 5 64] +[0x4690 0x80110380 0x80110382 252 263 0x77a4 32 48 ...]; field -[] +[w1+14347 w1+21 w1+49 w1+77 w2+14354 w4+14356]; calls -[] +[1f33a0 1... |
| `801ba5d8` ETC/GAME.EMI#0 |  | `000d2a48` | `000d275c` | callee-align | 533/846 | 15 | const -[0x101000 161 163 0xffff00 65] +[]; field -[] +[w1+10 w1+11 w1+2 w1+4 w4+5968]; calls -[d137c ecc4] +[d0e40 d1050 d11a4 d1470 d2674] |
| `801de8fc` ETC/SHOP.EMI#0 |  | `001fb418` | `001fb154` | similarity | 227/237 | 15 | const -[-121 0xfffffaae 0xfffffa0e 320] +[20 0x3c0000 92]; field -[w4+0 w4+12 w4+4 w4+8] +[w4+336]; calls -[] +[Battle_InitPartyContexts Battle_InitPartyMemb... |
| `801e100c` ETC/SHOP.EMI#0 |  | `001fd418` | `001fd184` | callee-align | 374/374 | 15 | const -[11 5] +[0xffff88ac 0xffff88a0 0x7874 40 88]; field -[] +[w1+122]; calls -[] +[9a620 9aa74 9ab6c 9b770] |
| `801f7cc8` SCENARIO/SCENA11.EMI#0 |  | `002a2d14` | `002a2b50` | similarity | 347/215 | 15 | const -[11 12 17 36 53 0xb0000 77] +[0xffffa250 0x3f90 20 22]; field -[] +[w1+121 w1+8 w4+60]; calls -[Flag_Set b9138] +[da4e0] |
| `801f9320` SCENARIO/SCENA14.EMI#0 |  | `002af928` | `002af764` | similarity | 1232/867 | 15 | const -[-792 -88 112 142 29 33 336 0xfff7 ...] +[0xffff8000 0x3214 0x321c 0x650]; field -[w1+0] +[w1+16272 w1+2 w1+4 w1+44 w1+5 w1+5904 w2+6]; calls -[2ad3f4... |
| `801a0848` ETC/GAME.EMI#0 |  | `0002508c` | `00024ea0` | gap-align | 66/66 | 14 | const -[320] +[]; calls -[] +[230140 24c5c0 24caa0 2601bc 2615f4] unpaired 4/0 |
| `801bb670` ETC/GAME.EMI#0 |  | `000d15d0` | `000d12e4` | callee-align | 665/829 | 14 | const -[] +[161 163 65]; field -[] +[w1-1712 w1+10 w1+9 w4+5968]; calls -[d0e40 d1050 d11a4 d1470 d2674] +[d137c ecc4]; globals 3 only PSX, 1 only PSP |
| `801dc694` ETC/SHOP.EMI#0 |  | `001f920c` | `-` | gap-align | 460/491 | 14 | const -[5] +[40 88]; field -[] +[w1+6 w4+0]; calls -[] +[9a620 9a80c 9a940 9ab6c 9b770]; globals 1 only PSX, 1 only PSP |
| `801e104c` ETC/START.EMI#8 |  | `001f6bcc` | `001f68fc` | callee-align | 183/176 | 13 | const -[] +[0xffff8298 0x77e8 0x77ec]; calls -[baf40 bb5e8] +[1f54e4 1f5b8c 9a620] |
| `801fda6c` SCENARIO/SCENA01.EMI#0 |  | `00276660` | `0027649c` | similarity-weak | 342/491 | 13 | const -[-28 -6 0xffe60000 0xffe80000 0xffeb0000 0xffed0000 0xffef0000 0xfff38000 ...] +[29]; field -[] +[w2-240 w4-26160] |
| `801fc91c` SCENARIO/SCENA09.EMI#0 |  | `0029aecc` | `0029ad08` | similarity-weak | 547/789 | 13 | const -[-5 0xfff10000 0xfff60000 0xfff70000 0xfffa0000 0xfffe0000 0x10000] +[0x418000 0x438000 0x448000 0x498000 0x4e8000 6 0x5c8000]; field -[] +[w1-26288 w... |
| `801fa9c8` SCENARIO/SCENA14.EMI#0 |  | `002b06f8` | `002b0534` | similarity | 552/504 | 13 | const -[] +[0x280000]; field -[w1+0] +[w1-26288 w1+1873]; calls -[97fc8 dec] +[143f8c 25610c 25621c 256384 25647c 8c84c d6f20 d8270] unpaired 4/0; globals 9 ... |
| `801d66a0` WORLD00/AREA030.EMI#4 |  | `0001cf24` | `-` | gap-order | 200/194 | 13 | const -[0x9b0000 0x9e0000 254 0xc0000 0xd0000 0x840000 0x860000] +[12 13 132 134 155 158]; field -[] +[w1+0] |
| `801548a4` SLPS_009.90 |  | `0000e560` | `0000e408` | callee-align | 235/217 | 13 | const -[-24 0x3ff 0x400 112 28 40 55] +[0x104014 0x10401e 132 0x800 32 33 5 58]; field -[] +[w2-7472 w2+0]; calls -[] +[] unpaired 0/1; globals 14 only PSX, ... |
| `801d49d8` ETC/COMMU01.EMI#7 |  | `0010c6f4` | `0010c424` | caller-vote | 55/57 | 12 | const -[44] +[-9 108 147 149 154 62 71 78 ...]; field -[] +[w1+3785] |
| `801d4bcc` ETC/SHOP.EMI#0 |  | `001f29a8` | `001f2690` | gap-align | 69/58 | 12 | const -[12 133 263 5 76 98] +[471 472]; calls -[1f3778 1f3d0c 93d8 Msg_SystemPtr SE_Play] +[] unpaired 2/3 |
| `801eaf20` ETC/START.EMI#8 |  | `000c7de8` | `-` | similarity | 347/340 | 12 | const -[5] +[0xffffcf4c 0xffffcf40 40]; calls -[] +[9a1b8 9a620 9ab6c 9b770] |
| `801f9384` SCENARIO/SCENA05.EMI#0 |  | `00284ea4` | `00284ce0` | similarity | 2665/2172 | 12 | const -[106 131 200 41 42 43 44 45 ...] +[]; field -[w1+0] +[w1-26288 w1+1 w1+1666 w1+4 w1+5 w2-1520 w4+52 w4+56]; calls -[9508c 950c0 95c6c Flag_Clear] +[10... |
| `801565fc` SLPS_009.90 |  | `000101bc` | `00010060` | gap-order | 140/144 | 12 | const -[-28 -56 0xffffff 27 28 0xff000000 56] +[-33 -58 0x104024 32 33 58]; field -[] +[w2+0 w4-4]; globals 5 only PSX, 1 only PSP |
| `801595e0` SLPS_009.90 |  | `00011ae8` | `0001196c` | caller-vote | 81/89 | 12 | const -[-28 -56 0x7fefc000 27 28 56] +[-33 -58 0x104024 33 58 66]; field -[] +[w2+0 w4+0] |
| `801b1e3c` SLPS_009.90 |  | `000cbc68` | `000cb97c` | caller-vote | 0/391 (size 392.0x) | 12 | const -[] +[129 192 32 320 5 6 65 9 ...]; field -[] +[w1-1712 w1+0 w1+1 w1+284 w1+44 w1+8 w1+9 w2+0 ...]; calls -[] +[29ec 84ed0 8c864 SE_Play c886c d0df8 ec... |
| `801d6f10` ETC/SHOP.EMI#0 |  | `001f3778` | `001f3464` | gap-order | 91/168 (size 1.8x) | 11 | const -[15 0x1fff] +[0x80110302 0x80110307 252 263 32 48 60]; field -[] +[w1+14347 w1+21 w1+49 w1+77 w2+14354 w4+14356]; calls -[] +[1f33a0 SE_Play] unpaired... |
| `801e4478` ETC/SHOP.EMI#0 |  | `002008a8` | `-` | similarity | 406/417 | 11 | const -[] +[0xffff898c 0xffff8980 40]; field -[w1+0 w1+32] +[w1+122 w1+36]; calls -[] +[9a1b8 9a620 9ab6c 9b770] |
| `800f6084` WORLD00/AREA030.EMI#5 |  | `000e8290` | `000e7fb0` | callee-align | 203/219 | 11 | const -[0x4f0000 0x510000 0x520000 0x540000 0xd0000] +[13 0x1008e 79 81 82 84]; field -[] +[w1+1 w2+0] |
| `801f30b8` WORLD01/AREA040.EMI#13 |  | `0022a5fc` | `0022a414` | caller-vote | 93/108 | 11 | const -[-28 -56 28 56] +[-33 -58 0x104024 0x104028 33 58 66]; field -[] +[w2+0 w4-4 w4+0] |
| `801f36b0` WORLD03/AREA120.EMI#13 |  | `00033588` | `000333a4` | caller-vote | 23/219 (size 9.2x) | 11 | const -[] +[0xfffffc00 0xffffd400 -128 0xfffff400 -384 0x400 127 0x1800000 ...]; field -[] +[w1+0 w1+1 w1+2 w1+3 w1+6 w2+8 w4+12 w4+16 ...]; calls -[] +[33130] |
| `80155b38` SLPS_009.90 |  | `0000edc0` | `0000ec68` | caller-vote | 97/118 | 11 | const -[-28 -56 28 56] +[-33 -58 0x104024 0x104028 33 58 66]; field -[] +[w2+0 w4-4 w4+0] |
| `80155cd4` SLPS_009.90 |  | `0000ef9c` | `0000ee44` | gap-order | 98/119 | 11 | const -[-28 -56 28 56] +[-33 -58 0x104024 0x104028 33 58 66]; field -[] +[w2+0 w4-4 w4+0] |
| `801d86d8` ETC/SHOP.EMI#0 |  | `001f4cb0` | `001f49ac` | gap-order | 205/191 | 10 | const -[] +[0xffff84ec 0xffff84e8 0xffff84e3 0xffff84de 0xffff80b8 0xffff8085]; globals 4 only PSX, 4 only PSP |
| `801d144c` ETC/SISYOU.EMI#0 |  | `00119fa0` | `00119bf0` | callee-align | 205/191 | 10 | const -[] +[-150 -154 -159 -164 -528 -579]; globals 4 only PSX, 4 only PSP |
| `801de474` ETC/START.EMI#8 |  | `000bdcd4` | `-` | callee-align | 517/472 | 10 | const -[-2 11] +[0xffffc282 24 83 88]; field -[w4+16] +[w1+32 w2+10 w2+36 w2+4 w2+6 w4+44]; calls -[] +[9a1b8 9a620] |
| `801fe2a0` SCENARIO/SCENA08.EMI#0 |  | `0029533c` | `00295178` | similarity | 240/317 | 10 | const -[-6 0x110000 0x130000 0x160000 0x1b0000 0x1f0000 0x38000 0x70000 ...] +[7]; field -[] +[w1+4 w1+5 w2-240 w2+6 w4-26160] |
| `801fc900` SCENARIO/SCENA10.EMI#0 |  | `002a0694` | `002a04d0` | similarity | 534/273 (size 0.5x) | 10 | const -[-682 -7 -948 30 512 53 6 64 ...] +[0xffff8000]; field -[] +[w2-1520]; calls -[29c400 Flag_Clear] +[143f8c d6f20 d82b8] |
| `801f2c90` WORLD01/AREA060.EMI#13 |  | `00068534` | `00068268` | similarity | 362/348 | 10 | const -[0x7900 0x7980 320 0xffd30000 553] +[-32 -4 0x400 0x104030 0x600 180 352]; field -[] +[w1+0 w1+1 w1+16384 w4+0]; globals 2 only PSX, 1 only PSP |
| `801f676c` WORLD02/AREA104.EMI#14 |  | `00245a9c` | `002458d8` | gap-align | 124/115 | 10 | const -[12 210 300 0x7bc6 46 9] +[242 287 332 0xf60]; field -[w1+3] +[w2+198] |
| `80155e74` SLPS_009.90 |  | `0000f17c` | `0000f024` | gap-align | 93/112 | 10 | const -[-28 -56 28 56] +[-33 -58 0x104024 33 58 66]; field -[] +[w1+16384 w2+0 w4-4 w4+0] |
| `801d125c` ETC/COMMU03.EMI#6 |  | `00117ce8` | `-` | similarity | 292/280 | 9 | const -[156 24 0x780b 75] +[176 0x73fc 0x7400 300 0x104e2 7]; field -[w4+24] +[w2+0 w4+32] |
| `801d63c8` ETC/START.EMI#8 |  | `000b3cb0` | `000b3a7c` | similarity | 114/167 | 9 | const -[19] +[]; field -[w1+0] +[]; calls -[] +[bbffc]; globals 6 only PSX, 10 only PSP |
| `801e588c` ETC/START.EMI#8 |  | `000c29a8` | `000c27a8` | similarity | 54/50 | 9 | const -[10 14 32 48] +[]; calls -[1f33a0 bba28] +[3ff74] unpaired 1/0 |
| `801cf1a4` PLCHAR/PLP678.EMI#0 +8 copies |  | `000ed9dc` | `000ed6fc` | caller-vote | 104/99 | 9 | const -[10] +[240 241 246 247]; calls -[] +[caaec d5c50 dce04] unpaired 1/0 |
| `801fbca4` SCENARIO/SCENA08.EMI#0 |  | `00294008` | `00293e44` | similarity | 1144/550 (size 0.5x) | 9 | const -[-792 180 30 0xfff7 9 90 95] +[0xffff8000 152]; field -[w1+0 w1+6 w4+24 w4+28] +[w1-26288 w1+1670 w1+18773]; calls -[95d5c 95db0 9716c 97fc8] +[107174... |
| `801f88dc` SCENARIO/SCENA14.EMI#0 |  | `002aefb4` | `002aedf0` | similarity-weak | 531/292 (size 0.6x) | 9 | const -[10 27 36 5 6 0xfff7 7 9] +[0x650]; calls -[8e7e0 8e810 dec] +[]; globals 5 only PSX, 1 only PSP |
| `801f4608` WORLD02/AREA104.EMI#14 |  | `002452ec` | `00245128` | callee-align | 200/201 | 9 | field -[] +[w4+5968]; calls -[] +[c8830 cafb4 cb39c d5d5c d9b34 db61c]; globals 2 only PSX, 1 only PSP |
| `801f4b4c` WORLD03/AREA121.EMI#14 |  | `00250248` | `00250084` | callee-align | 200/201 | 9 | field -[] +[w4+5968]; calls -[] +[c8830 cafb4 cb39c d5d5c d9b34 db61c]; globals 2 only PSX, 1 only PSP |
| `801f53a4` WORLD04/AREA170.EMI#13 |  | `00260f28` | `00260d64` | similarity | 179/226 | 9 | const -[0x28000 0x208000 0x8000 0x50000 0x58000 0x80000 0x88000 0xb0000 ...] +[]; field -[] +[w1+19 w1+20] |
| `801f3030` WORLD04/AREA196.EMI#13 |  | `0007f958` | `0007f690` | callee-align | 477/523 | 9 | const -[171 256 0x78ca 0x7900 0x7940 677 805] +[0x400 128 0x500 0x1000 0x11940 928]; field -[w4+20 w4+32 w4+44 w4+8] +[w1+0 w1+1 w1+8 w2+12 w2+17552 w2+2 w2+... |
| `801af3f0` SLPS_009.90 |  | `0009ac74` | `0009a9f0` | caller-vote | 0/334 (size 335.0x) | 9 | const -[] +[-4 -40 -8 128 172 240 256 32 ...]; field -[] +[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...]; calls -[] +[Packet_Commit] |
| `801d62ec` ETC/SHOP.EMI#0 |  | `001f2f9c` | `001f2c84` | gap-order | 158/175 | 8 | const -[] +[0xffffa190 0x650]; field -[] +[w1+1873 w2-240 w4+336]; calls -[] +[88d7c Battle_InitPartyContexts Battle_InitPartyMember d5990] |
| `801d7218` ETC/SHOP.EMI#0 |  | `001f3d0c` | `001f39f8` | gap-order | 75/134 (size 1.8x) | 8 | const -[-48 0xea0] +[11 12 127 146 9]; calls -[] +[Flag_Test] unpaired 0/1; globals 2 only PSX, 26 only PSP |
| `801dcdfc` ETC/SHOP.EMI#0 |  | `001f99b8` | `-` | gap-align | 307/317 | 8 | const -[] +[40]; field -[] +[w1+1666 w1+6 w4+0]; calls -[] +[9a620 9aa74 9b540]; globals 2 only PSX, 1 only PSP |
| `801e36ac` ETC/SHOP.EMI#0 |  | `00200220` | `001fff08` | similarity | 224/241 | 8 | const -[12] +[0x7880 0x7888 0x7890]; field -[] +[w4+2000]; calls -[] +[9a1b8 9a620] |
| `801d2afc` ETC/START.EMI#8 |  | `000b0270` | `000b0030` | similarity | 107/79 | 8 | const -[12] +[]; calls -[] +[d5990]; globals 11 only PSX, 7 only PSP |
| `801d2dc0` WORLD00/AREA030.EMI#4 |  | `0001e4d0` | `0001e298` | similarity | 270/347 | 8 | const -[248] +[100 108 137 149 192 193 99]; calls -[] +[] unpaired 0/1 |
| `801f2d44` WORLD00/AREA037.EMI#13 |  | `0006aa98` | `0006a7cc` | similarity | 268/286 | 8 | const -[0x78cf 0xe00 43] +[-192 -2 0xffffe190 0x3f0 129] |
| `801f3b84` WORLD03/AREA120.EMI#13 |  | `00084524` | `0008425c` | similarity | 90/88 | 8 | const -[0x20600 0x20800 0xff0000 0xffff00 0x30600 0x40600 0x10300 0x10600] +[]; field -[w4+0] +[w1+1] |
| `801f3304` WORLD03/AREA143.EMI#13 |  | `0007e370` | `0007e0a8` | similarity | 153/145 | 8 | const -[-120 -152 -24 -56 -88] +[-128 -160 -32 -64 -96]; field -[] +[w1-7024 w4+5968]; calls -[] +[3154] |
| `801d3288` ETC/COMMU02.EMI#8 |  | `0011560c` | `0011531c` | callee-align | 167/180 | 7 | const -[] +[0xfffff960 0xfffff95c 0x7380 0x7388 0x7390 0x7398 5] |
| `801d1048` ETC/COMMU05.EMI#7 |  | `00118edc` | `-` | callee-align | 137/159 | 7 | const -[] +[0xfffffa8c 0x7410 0x1006e 0x10070 0x10072 0x10074]; field -[] +[w2+0]; globals 1 only PSX, 2 only PSP |
| `8019a9f8` ETC/GAME.EMI#0 |  | `00015140` | `-` | gap-align | 432/463 | 7 | const -[5] +[40 88]; field -[] +[w1+6 w4+0]; calls -[212268] +[] unpaired 2/0; globals 1 only PSX, 1 only PSP |
| `801be38c` ETC/GAME.EMI#0 |  | `000d4134` | `000d3e48` | gap-align | 104/116 | 7 | const -[0x20000 0xffff00 0x10a00 0x18000] +[0xffffa190 10]; field -[] +[w1+1868 w4+17424]; globals 2 only PSX, 2 only PSP |
| `801d1a88` ETC/GAME.EMI#1 |  | `00006220` | `00006b94` | similarity | 79/67 | 7 | const -[-6 24 26 28] +[-9 18 31 61]; calls -[] +[] unpaired 1/0 |
| `801d3320` ETC/SHISU.EMI#0 |  | `00209ec4` | `-` | similarity | 272/261 | 7 | const -[] +[0xffff955c 0xffff9550 40]; calls -[] +[9a620 9a940] |
| `801e55f4` ETC/START.EMI#8 |  | `0021686c` | `0021658c` | similarity | 44/42 | 7 | const -[32 48] +[6]; calls -[1f33a0 bbffc] +[] unpaired 1/3 |
| `801cf420` PLCHAR/PLP267.EMI#0 |  | `000ed4a8` | `000ed1c8` | gap-align | 126/151 | 7 | const -[10] +[240 241 246 247]; field -[] +[w4+5968]; calls -[] +[da4e0 dce04] unpaired 1/0 |
| `801cec18` PLCHAR/PLP349.EMI#0 |  | `000f2aa4` | `000f27c4` | gap-align | 126/151 | 7 | const -[10] +[240 241 246 247]; field -[] +[w4+5968]; calls -[] +[da4e0 dce04] unpaired 1/0 |
| `801cf224` PLCHAR/PLP567.EMI#0 |  | `000f4b10` | `000f4830` | gap-align | 126/151 | 7 | const -[10] +[240 241 246 247]; field -[] +[w4+5968]; calls -[] +[da4e0 dce04] unpaired 1/0 |
| `801fa6bc` SCENARIO/SCENA10.EMI#0 |  | `0029ec58` | `0029ea94` | similarity | 1578/1163 | 7 | const -[-5 -640 0x110000 0x1a0000 0x1f0000 0x80000] +[]; field -[w1+0] +[w1-26288 w2-1520 w4+336 w4+56]; calls -[29c510 951c0] +[107174 95118 d8270 d82b8] un... |
| `801fcbac` SCENARIO/SCENA15.EMI#0 |  | `002b4964` | `002b47a0` | similarity | 897/824 | 7 | const -[] +[0x650]; field -[w1+0] +[w1+1 w1+18794]; calls -[] +[143f8c 25a7f4 8c84c 970fc d6f20] unpaired 2/0; globals 5 only PSX, 1 only PSP |
| `801f9238` SCENARIO/SCENA17.EMI#0 |  | `002b85f8` | `002b8448` | callee-align | 94/103 | 7 | const -[26 33 34 35 88] +[-32 105 91] |
| `801daa5c` WORLD00/AREA030.EMI#4 |  | `000e479c` | `000e44ac` | similarity-weak | 245/251 | 7 | const -[0x110000 0x11ffff 0x14ffff 0x150000 0x7fff 0x3e0000 0x80000] +[0x3e8000]; field -[] +[w1-1712 w1+1 w1+11 w1+233 w1+349 w1+9763 w2+1808 w2+62 ...]; ca... |
| `801dc9bc` WORLD00/AREA030.EMI#4 |  | `000e4fdc` | `000e4cec` | similarity | 184/232 | 7 | const -[-3] +[0x100a0 0x100c0 0x100de]; field -[] +[w2+0 w4+1936 w4+5968]; calls -[] +[] unpaired 0/2; globals 4 only PSX, 2 only PSP |
| `800f6748` WORLD00/AREA030.EMI#5 |  | `000e892c` | `000e864c` | callee-align | 207/224 | 7 | const -[0x580000 0x5c0000 0xd0000] +[0x10090 0x10092 88 92]; field -[] +[w2+0] |
| `801f34a0` WORLD01/AREA047.EMI#13 |  | `00068b00` | `00068834` | callee-align | 263/282 | 7 | const -[0x78cf 0xe00 43] +[0xffffe190 0x3f0 129 256 38] |
| `801f4258` WORLD01/AREA048.EMI#13 |  | `00063628` | `0006335c` | callee-align | 299/218 | 7 | const -[-128 441 451 80] +[0xffffc000 301 381]; field -[] +[w2+2 w4+2000] |
| `801f2d4c` WORLD01/AREA064.EMI#13 |  | `0023326c` | `00233094` | gap-order | 34/63 (size 1.8x) | 7 | const -[0x2800 0x5100 0xb25 0x201f] +[-5 63 7]; field -[w1+0 w1+11704 w1+3] +[]; calls -[] +[f5c0] unpaired 1/0 |
| `801f4e3c` WORLD02/AREA104.EMI#14 |  | `002439cc` | `00243800` | gap-align | 96/106 | 7 | field -[] +[w1+80 w2-1456 w4+17424 w4+5968]; calls -[] +[84ed0 8c904 da15c da74c da9ac dce04] unpaired 1/0; globals 6 only PSX, 1 only PSP |
| `801f5380` WORLD03/AREA121.EMI#14 |  | `0024ee60` | `0024ec9c` | gap-align | 96/106 | 7 | field -[] +[w1+80 w2-1456 w4+17424 w4+5968]; calls -[] +[84ed0 8c904 da15c da74c da9ac dce04] unpaired 1/0; globals 6 only PSX, 1 only PSP |
| `801563ac` SLPS_009.90 |  | `0000f344` | `0000f1ec` | caller-vote | 20/52 (size 2.5x) | 7 | const -[-56 56] +[-33 -58 33 0xfff 58 66] |
| `80156ee8` SLPS_009.90 |  | `00011c50` | `00011ad4` | similarity | 245/253 | 7 | const -[-129 0x78c0 43 481] +[0xffffc000 401 768]; field -[w1+3] +[w1+0 w2+0 w2+12 w2+16 w2+18 w2+2 w2+20 w2+28 ...] |
| `80159108` SLPS_009.90 |  | `000115ec` | `00011480` | gap-align | 113/135 | 7 | const -[248 320] +[-32 24 28 352 40 44 70]; field -[w2+2] +[] |
| `8015b1d8` SLPS_009.90 |  | `0008ba70` | `0008b7bc` | gap-order | 76/76 | 7 | const -[] +[116]; calls -[] +[8627c 87894 Script_ShowMessage] |
| `80164ec8` SLPS_009.90 |  | `000b9aac` | `000b98dc` | similarity | 178/131 | 7 | const -[-12 -120 120 0x7800 63] +[0xffffc0f0 -56 -72]; field -[w1+24] +[w1+32] |
| `8016571c` SLPS_009.90 |  | `000b8048` | `-` | gap-order | 107/109 | 7 | const -[] +[20 5]; field -[w4+0 w4+12 w4+168 w4+184 w4+4 w4+8] +[w4+16 w4+180 w4+196 w4+20]; globals 8 only PSX, 5 only PSP |
| `80168288` SLPS_009.90 |  | `000ddda8` | `000ddac0` | callee-align | 299/299 | 7 | const -[167 0x7802 6 0xffe5 7] +[-3 0xffffe190 13]; field -[w1+3] +[w4+1744] |
| `801d0ebc` ETC/COMMU01.EMI#7 |  | `0010a5f0` | `0010a324` | similarity | 152/152 | 6 | const -[-9 0x66660000 0x6667] +[-486 5]; globals 3 only PSX, 1 only PSP |
| `801a0aa8` ETC/GAME.EMI#0 |  | `0008538c` | `000850c4` | callee-align | 261/283 | 6 | const -[18 22 0xee7f] +[0x650 0x5b58 32]; field -[w4+32 w4+35 w4+36 w4+39] +[w1-1648 w1+1 w1+144 w1+17744 w1+1873 w1+2 w2+0 w2+2 ...]; calls -[dec] +[970fc] ... |
| `801a9d38` ETC/GAME.EMI#0 |  | `0008f2b0` | `0008f008` | gap-align | 137/147 | 6 | const -[129 144 49 81] +[-128 209] |
| `801d2308` ETC/SHISU.EMI#0 |  | `002096e8` | `00209410` | similarity | 272/282 | 6 | const -[] +[12 20 28 34 36]; field -[w2-10 w2-12 w2-14 w2-16 w2-18 w2-2 w2-20 w2-22 ...] +[w2+2 w2+30 w2+4 w2+6]; calls -[] +[9c594] unpaired 0/2 |
| `801d473c` ETC/SHOP.EMI#0 |  | `001f2674` | `001f235c` | gap-align | 31/45 | 6 | const -[23] +[0x80110300 0x80110302 464]; field -[] +[w1+2]; calls -[] +[1f3778] unpaired 1/0 |
| `801d9230` ETC/SHOP.EMI#0 |  | `001f74c8` | `001f71fc` | callee-align | 229/215 | 6 | const -[] +[0x7814]; field -[] +[w1+150 w4+152]; globals 6 only PSX, 5 only PSP |
| `801e313c` ETC/SHOP.EMI#0 |  | `001ffd68` | `001ffa50` | similarity | 283/302 | 6 | const -[12] +[0x7888]; field -[] +[w4+2000]; calls -[] +[9a1b8 9a620] |
| `801e2424` ETC/START.EMI#8 |  | `000c0488` | `000c0308` | similarity | 51/57 | 6 | const -[] +[0x6e64]; calls -[ec8] +[3ff74 8c694] unpaired 1/0; globals 2 only PSX, 1 only PSP |
| `801f83c0` SCENARIO/SCENA01.EMI#0 |  | `00272664` | `002724a0` | similarity | 410/548 | 6 | const -[] +[21 25 0xa00 5 58 6]; field -[w1+0] +[w1-26288 w1+1 w1+2 w1+3 w1+5904 w2-240]; calls -[] +[95bfc File_LoadDone d8270] |
| `801f8564` SCENARIO/SCENA05.EMI#0 |  | `00284360` | `0028419c` | similarity | 355/210 (size 0.6x) | 6 | const -[11 15 20 21 22 7] +[]; field -[w1+0] +[w2-1520]; calls -[] +[107174 95118 d8270] unpaired 1/0 |
| `801fb270` SCENARIO/SCENA08.EMI#0 |  | `00293674` | `002934b0` | similarity | 452/342 | 6 | const -[128 0xbb8 36] +[0x420000]; field -[w1+0] +[w1+2 w2-1520]; calls -[Zenny_Add dec] +[143f8c 9170 970fc d6f20] unpaired 1/0; globals 3 only PSX, 1 only PSP |
| `801da320` WORLD00/AREA030.EMI#4 |  | `000e4590` | `000e42a0` | similarity | 73/125 (size 1.7x) | 6 | const -[0xfffe] +[0xffffe850 14 0x10080]; field -[] +[w1+117 w1+12 w1+4 w2+0 w4+5968]; calls -[] +[] unpaired 0/3 |
| `800f63e4` WORLD00/AREA030.EMI#5 |  | `000e85fc` | `000e831c` | callee-align | 118/107 | 6 | calls -[] +[1d8b4 e345c e3500] |
| `800f6a9c` WORLD00/AREA030.EMI#5 |  | `000e8cac` | `000e89cc` | callee-align | 117/105 | 6 | calls -[] +[1d8b4 e345c e3500] |
| `800f6ef8` WORLD00/AREA030.EMI#5 |  | `000e90b0` | `000e8dd0` | gap-order | 108/97 | 6 | calls -[] +[1d8b4 e345c e3500] |
| `801f35d0` WORLD01/AREA044.EMI#13 |  | `0022cb38` | `0022c960` | gap-align | 63/151 (size 2.4x) | 6 | const -[] +[10 21 240 48 7]; calls -[] +[22c7a4 8e7e0] unpaired 1/0 |
| `801f37c0` WORLD02/AREA085.EMI#13 |  | `0023983c` | `00239670` | gap-align | 58/65 | 6 | const -[0xc06] +[12 6]; field -[] +[w1-1648]; calls -[] +[Battle_InitPartyMember]; globals 1 only PSX, 1 only PSP |
| `801f3490` WORLD02/AREA102.EMI#13 |  | `0006b9b0` | `0006b6e4` | similarity | 89/98 | 6 | calls -[] +[da378 da4e0 dacb4] |
| `801f595c` WORLD03/AREA121.EMI#14 +1 copies |  | `00243f04` | `00243d38` | gap-align | 77/71 | 6 | const -[228 0x7800] +[152 176 224 260 7]; field -[w1+0 w1+1] +[w1+16 w1+17] |
| `801f2c74` WORLD03/AREA127.EMI#13 |  | `00250adc` | `00250918` | gap-order | 24/21 | 6 | const -[0x2f00 0x3200 0x4e00 0x4f00 0x5680] +[11 15]; field -[] +[w2+126]; calls -[] +[] unpaired 1/0 |
| `801f3460` WORLD03/AREA149.EMI#13 |  | `0025aa24` | `0025a860` | similarity | 146/135 | 6 | const -[10 0x140000 0xffec0000] +[0xffffec00 13 0x1400]; field -[w4+0] +[w2-24368] |
| `801f31dc` WORLD04/AREA170.EMI#13 |  | `0004c254` | `0004c03c` | similarity | 236/238 | 6 | const -[41 49 80 9] +[-4 70] |
| `801f3768` WORLD04/AREA197.EMI#13 |  | `0007f324` | `0007f05c` | similarity | 148/159 | 6 | const -[0xfffff880 171 0x78ca] +[0xffffc000 0x3880 928] |
| `801f36b0` WORLD04/AREA198.EMI#13 |  | `0026eacc` | `0026e908` | gap-align | 98/76 | 6 | const -[-6 7] +[-192 30 0x1488]; calls -[] +[89cec] |
| `801f2e80` WORLD04/AREA199.EMI#13 |  | `0008311c` | `00082e54` | similarity | 299/195 | 6 | const -[14 0x79c0 485 544] +[0x500 0x700] |
| `8014d5d0` SLPS_009.90 |  | `0000283c` | `000031a8` | callee-align | 107/107 | 6 | const -[5] +[80]; field -[w1+100] +[]; calls -[a540c] +[2ce4 Battle_InitPartyMember] |
| `80153950` SLPS_009.90 |  | `0000cf8c` | `0000ce34` | gap-align | 67/71 | 6 | const -[-2 27 28 55] +[32 33 57] |
| `8015601c` SLPS_009.90 |  | `0000fdfc` | `0000fca0` | caller-vote | 208/238 | 6 | const -[0x20000 0x40000 32 384 483 0x10000 96] +[0x3ff 0x104004 0x104006 14 768]; field -[] +[w2+0 w4+0] |
| `80158464` SLPS_009.90 |  | `00012d7c` | `00012c08` | similarity-weak | 184/174 | 6 | const -[-128 441 451] +[0xffffc000 301 381]; field -[w1+10 w1+11 w1+6 w1+7 w1+8 w1+9 w2+0 w4+12] +[w1-24496 w1+2 w4+0] |
| `80167da0` SLPS_009.90 |  | `000dd9b0` | `000dd6c8` | callee-align | 220/250 | 6 | const -[0x60000] +[]; field -[] +[w1+44 w2-1456 w2+2 w2+31824 w4+17424 w4+5968]; calls -[] +[ca9d4 d9b34 db61c dc1f0]; globals 4 only PSX, 1 only PSP |
| `801b0390` SLPS_009.90 |  | `0009a70c` | `0009a488` | caller-vote | 0/63 (size 64.0x) | 6 | const -[] +[832 896]; field -[] +[w1+0 w1+1 w1+2]; calls -[] +[9a620 Packet_Commit] |
| `801bf1a8` SLPS_009.90 |  | `000d856c` | `000d8284` | caller-vote | 0/266 (size 267.0x) | 6 | const -[] +[128 15 192 256 32 0x8000 48]; field -[] +[w1+0 w1+4 w4+0 w4+20]; calls -[] +[107174 d5ff8 d61a4] |
| `801f03c0` SLPS_009.90 |  | `00107c88` | `001079a8` | caller-vote | 0/89 (size 90.0x) | 6 | const -[] +[15 155 159 160 20 25 31 6]; field -[] +[w1+0] |
| `801d258c` ETC/BATE.EMI#0 |  | `00201ebc` | `00201ba4` | similarity | 80/79 | 5 | const -[-50 30 5] +[20]; globals 3 only PSX, 1 only PSP |
| `801d1278` ETC/COMMU05.EMI#7 |  | `0011915c` | `-` | callee-align | 134/133 | 5 | const -[] +[0xfffffa8e 0x7410 0x1006e 0x10076]; field -[] +[w2+0]; globals 3 only PSX, 1 only PSP |
| `801b31e0` ETC/GAME.EMI#0 |  | `000ccbe0` | `000cc8f4` | similarity | 282/296 | 5 | const -[-2 320] +[20]; field -[w1+112 w4+12 w4+8] +[w1-1 w1-2 w1+1666 w4+112]; globals 8 only PSX, 2 only PSP |
| `801b4ea8` ETC/GAME.EMI#0 |  | `000cd080` | `000ccd94` | callee-align | 148/139 | 5 | const -[241 320 5] +[10]; calls -[] +[dce04] unpaired 1/0; globals 2 only PSX, 1 only PSP |
| `801b766c` ETC/GAME.EMI#0 |  | `000cb39c` | `000cb0b0` | gap-align | 145/165 | 5 | const -[96] +[0xfffff9d0 0xffffa190 161]; field -[] +[w1+144 w1+19 w1+80 w2+12 w4+5968]; calls -[] +[dce04] unpaired 2/0 |
| `801bc2b0` ETC/GAME.EMI#0 |  | `000d37f8` | `000d350c` | callee-align | 246/337 | 5 | const -[145 209 0xffff0000 49 81] +[]; field -[w1+112] +[w1+11 w4+112 w4+17424 w4+5968]; calls -[] +[dce04] unpaired 1/0 |
| `801c0f94` ETC/GAME.EMI#0 |  | `000d90bc` | `000d8dd4` | callee-align | 347/358 | 5 | const -[320 0xf0000] +[0x3f90 20]; field -[] +[w1+0 w1+121 w1+43 w2-1520 w4+112]; calls -[] +[] unpaired 1/0; globals 14 only PSX, 1 only PSP |
| `801c3010` ETC/GAME.EMI#0 |  | `000d986c` | `000d9584` | callee-align | 154/140 | 5 | const -[480] +[20 40 60]; globals 3 only PSX, 1 only PSP |
| `801d8a10` ETC/SHOP.EMI#0 |  | `001f4fac` | `-` | gap-order | 136/133 | 5 | const -[] +[0xffff84f8 28]; field -[w1+14 w1+15 w1+16 w1+17 w1+18 w1+19 w4+0] +[w4+32]; calls -[] +[9a1b8]; globals 1 only PSX, 6 only PSP |
| `801d8c34` ETC/SHOP.EMI#0 |  | `001f51c0` | `001f4ed8` | gap-order | 111/101 | 5 | const -[36] +[0xffff81e4 0x77cc 0x77d0 6] |
| `801def00` ETC/SHOP.EMI#0 |  | `001fb7dc` | `001fb518` | callee-align | 263/281 | 5 | const -[160 31 93] +[24 86]; field -[w4+12] +[w1+0 w1+122]; globals 1 only PSX, 1 only PSP |
| `801e15f0` ETC/SHOP.EMI#0 |  | `001fd9f0` | `-` | similarity | 275/291 | 5 | const -[] +[40]; calls -[] +[9a620 9aa74] |
| `801d1784` ETC/SISYOU.EMI#0 |  | `0011b6fc` | `0011b34c` | similarity | 226/248 | 5 | const -[160] +[-720 0x7418 65 70 0x2000] |
| `801d2598` ETC/START.EMI#8 |  | `000afd74` | `000afb34` | similarity | 145/131 | 5 | const -[36 432] +[]; calls -[] +[d5990]; globals 4 only PSX, 3 only PSP |
| `801d6a0c` ETC/START.EMI#8 |  | `000b3f68` | `-` | callee-align | 208/199 | 5 | const -[15 17] +[]; field -[w1+14] +[w1+4 w1+5 w4+4 w4+8]; globals 6 only PSX, 4 only PSP |
| `801e1770` ETC/START.EMI#8 |  | `000c0ff0` | `000c0de4` | similarity | 196/174 | 5 | const -[0xfffe] +[0x1410]; field -[] +[w2+12]; calls -[] +[3ff74] unpaired 1/0; globals 4 only PSX, 1 only PSP |
| `801e7ca0` ETC/START.EMI#8 |  | `000c4af4` | `000c4818` | callee-align | 99/107 | 5 | const -[6 82] +[174 180 76]; field -[] +[w2+22] |
| `801f89f0` SCENARIO/SCENA00.EMI#0 |  | `00016de8` | `00016c6c` | gap-order | 45/58 | 5 | const -[] +[52 56 60]; field -[w1+28 w4+60] +[w4+0 w4+28]; calls -[] +[8fc28] |
| `801f7450` SCENARIO/SCENA02.EMI#0 |  | `00279778` | `002795b4` | similarity | 357/456 | 5 | const -[-22] +[23 25 32 55 6]; field -[] +[w1+1 w1+2 w1+3 w1+5904 w4-26160]; calls -[dec] +[2796fc d82b8] |
| `801fd780` SCENARIO/SCENA02.EMI#0 |  | `0027cc90` | `0027cacc` | similarity-weak | 585/943 | 5 | const -[-3 0x4ffff 0xfff70000 0x80000 0x90000 0x10000 0xf0000] +[]; field -[] +[w1-26288 w1+1 w1+2 w1+3 w1+4 w2-240 w4-26160] |
| `801f747c` SCENARIO/SCENA04.EMI#0 |  | `0002b810` | `0002b634` | signature | 79/55 | 5 | const -[0x118000 0x138000 0x158000 0x160000 0xf0000] +[] |
| `801fd67c` SCENARIO/SCENA07.EMI#0 |  | `00291a1c` | `00291858` | similarity | 164/212 | 5 | const -[-28 -5 -7 0x1c8000] +[30]; field -[] +[w1-26288 w2-240]; calls -[] +[291914] unpaired 1/0 |
| `801fba70` SCENARIO/SCENA08.EMI#0 |  | `0029231c` | `00292158` | similarity | 125/129 | 5 | const -[] +[0xffff8000 5]; field -[] +[w2-1520]; calls -[] +[d7ef0]; globals 1 only PSX, 1 only PSP |
| `801f763c` SCENARIO/SCENA13.EMI#0 |  | `0004f534` | `0004f318` | similarity | 183/156 | 5 | const -[0xffff0000 0x10000] +[]; field -[] +[w1+0]; calls -[] +[3c5c 89cec] |
| `801f7028` SCENARIO/SCENA14.EMI#0 |  | `002ad968` | `002ad7a4` | similarity | 538/502 | 5 | const -[0xd8000] +[]; field -[] +[w2+4 w4+336]; calls -[] +[25628c d6f20 d8270 d82b8] unpaired 1/0 |
| `801fb3bc` SCENARIO/SCENA14.EMI#0 |  | `002ad67c` | `002ad4b8` | gap-align | 21/26 | 5 | calls -[] +[26c230 26cc60] unpaired 2/0 |
| `801fb414` SCENARIO/SCENA14.EMI#0 |  | `002ad6e8` | `002ad524` | gap-align | 21/26 | 5 | calls -[] +[26c230 26cc60] unpaired 2/0 |
| `801fb46c` SCENARIO/SCENA14.EMI#0 |  | `002ad754` | `002ad590` | gap-align | 21/26 | 5 | calls -[] +[26c230 26cc60] unpaired 2/0 |
| `801fb4c4` SCENARIO/SCENA14.EMI#0 |  | `002ad7c0` | `002ad5fc` | similarity | 21/26 | 5 | calls -[] +[26c230 26cc60] unpaired 2/0 |
| `801fb51c` SCENARIO/SCENA14.EMI#0 |  | `002ad82c` | `002ad668` | similarity | 21/26 | 5 | calls -[] +[26c230 26cc60] unpaired 2/0 |
| `801f7aec` SCENARIO/SCENA15.EMI#0 |  | `00051020` | `00050e14` | similarity | 131/115 | 5 | const -[-21 820] +[-37 20 276]; field -[w4+0 w4+20 w4+23 w4+24 w4+27 w4+3 w4+4 w4+7] +[] |
| `801fddc0` SCENARIO/SCENA15.EMI#0 |  | `002b2518` | `002b2354` | similarity | 25/29 | 5 | calls -[] +[26c230 26cc60] unpaired 2/0 |
| `801f8f38` SCENARIO/SCENA17.EMI#0 |  | `002b83c0` | `002b8210` | caller-vote | 46/10 (size 0.2x) | 5 | const -[210 96] +[180 32]; field -[w1+3 w1+4 w1+5 w1+6 w1+7 w2+10 w2+12 w2+14 ...] +[]; calls -[Packet_Commit] +[] unpaired 0/1 |
| `801f33a8` WORLD00/AREA027.EMI#13 |  | `000608e0` | `00060614` | similarity | 51/45 | 5 | const -[0xffffc000 12] +[0xffffe3c0 0xffffe340 0x13500000] |
| `801d3820` WORLD00/AREA030.EMI#4 |  | `00019e98` | `00019d2c` | gap-align | 28/63 (size 2.2x) | 5 | const -[] +[-16 -7 10 159]; field -[] +[w4+5968]; calls -[] +[] unpaired 0/2 |
| `801d7c58` WORLD00/AREA030.EMI#4 |  | `0001daa8` | `-` | callee-align | 117/169 | 5 | const -[] +[-44 129 144 158]; field -[w1+0] +[w1+32]; calls -[] +[] unpaired 0/2 |
| `801d978c` WORLD00/AREA030.EMI#4 |  | `000203ac` | `-` | similarity | 246/192 | 5 | const -[143 175 0x7800 0xc31 0x30c30000] +[] |
| `801dce38` WORLD00/AREA030.EMI#4 |  | `000e16fc` | `000e140c` | gap-align | 120/147 | 5 | const -[] +[-7 0x100a2 0x100a4]; field -[] +[w1+1870 w1+697 w1+703 w2+0 w2+740]; calls -[] +[] unpaired 0/2; globals 4 only PSX, 1 only PSP |
| `801dd6bc` WORLD00/AREA030.EMI#4 |  | `000e1954` | `000e1664` | gap-align | 78/90 | 5 | const -[] +[0xfffff9d0 -7 0x100c2]; field -[] +[w2+0 w2+16]; calls -[] +[] unpaired 0/2; globals 2 only PSX, 1 only PSP |
| `801e0450` WORLD00/AREA030.EMI#4 |  | `000e2c5c` | `000e296c` | gap-align | 139/144 | 5 | const -[0x10ffff 0x110000 0x130000 0x19ffff 0x1a0000 0x80000] +[] |
| `801f2dcc` WORLD01/AREA049.EMI#14 |  | `00230140` | `0022ff68` | similarity-weak | 204/234 | 5 | const -[] +[0x3f90]; field -[] +[w1+44 w1+5 w2-1520 w4+5968]; calls -[] +[c8830 d9b88 d9d1c] |
| `801f3258` WORLD02/AREA088.EMI#13 |  | `0004d4c4` | `0004d2b4` | callee-align | 198/210 | 5 | const -[12 18 42] +[]; field -[w2-2 w2+0 w2+2 w2+4 w4-2] +[w4+16 w4+20 w4+24 w4+28 w4+32]; calls -[] +[37ee4] unpaired 1/0 |
| `801f2ed4` WORLD02/AREA091.EMI#13 |  | `0023e220` | `0023e054` | gap-align | 145/146 | 5 | const -[0x1a0000 174 0x1f8000 6] +[0xffffe190]; field -[w1+3] +[] |
| `801f2d9c` WORLD02/AREA105.EMI#13 |  | `0007b7c8` | `0007b500` | similarity | 224/172 | 5 | const -[-128 -129 0x78c6 427 64] +[864] |
| `801f2c60` WORLD03/AREA117.EMI#13 |  | `000640ec` | `00063e20` | similarity-weak | 98/104 | 5 | const -[0xffea0000 0xffeb0000 0xfffd8000 0x10000] +[0x5f8]; field -[] +[w1+16384 w4-7600 w4+1936]; globals 2 only PSX, 1 only PSP |
| `801f3884` WORLD03/AREA117.EMI#13 |  | `00064e20` | `00064b54` | similarity | 155/138 | 5 | const -[-128 -64] +[0xffffc040 0xffffc008 72] |
| `801f60f4` WORLD03/AREA121.EMI#14 +1 copies |  | `00245664` | `002454a0` | callee-align | 158/138 | 5 | const -[171 254 0x78ca] +[0xffffe190 928] |
| `801f3050` WORLD03/AREA140.EMI#13 |  | `000711a0` | `00070ed8` | callee-align | 160/135 | 5 | const -[-127 -128] +[0xffffc040 0xffffbfc0 0x1780000] |
| `801f35e4` WORLD03/AREA141.EMI#13 |  | `0025581c` | `00255658` | gap-align | 34/32 | 5 | const -[0xffbb0000] +[]; field -[] +[w4-25520]; calls -[] +[106fec 8f78c] |
| `801f4150` WORLD04/AREA189.EMI#13 |  | `0007db38` | `0007d870` | similarity | 157/140 | 5 | const -[127 0x7a00 486 679 807] +[0x600 0x800 256] |
| `8014ae58` SLPS_009.90 |  | `00000570` | `00000ee0` | gap-order | 49/99 (size 2.0x) | 5 | const -[0x400] +[-128 -64 128 0x4000 0x800 256 32 0x8000 ...]; calls -[330c 3708] +[] unpaired 1/0 |
| `8014dff0` SLPS_009.90 |  | `0000330c` | `00003c78` | gap-order | 74/79 | 5 | const -[] +[17]; field -[w4+12 w4+8] +[]; calls -[] +[] unpaired 0/5 |
| `8014fd78` SLPS_009.90 |  | `0000a050` | `-` | callee-align | 162/196 | 5 | const -[-60 0x7800 480 95] +[12 31] |
| `801532a0` SLPS_009.90 |  | `0000d57c` | `0000d424` | caller-vote | 273/253 | 5 | const -[] +[0x10401e 0x7fff00]; field -[] +[w1-880 w1+784 w2-7152 w2-7216 w2-7856 w2+0 w2+2 w2+4 ...]; calls -[] +[7c608]; globals 18 only PSX, 1 only PSP |
| `80153858` SLPS_009.90 |  | `0000ce94` | `0000cd3c` | callee-align | 60/62 | 5 | const -[27 28 55] +[32 33 58] |
| `80153a6c` SLPS_009.90 |  | `0000d0a8` | `0000cf50` | callee-align | 68/70 | 5 | const -[27 28 55] +[32 33 58] |
| `80155154` SLPS_009.90 |  | `0000d48c` | `0000d334` | gap-align | 53/59 | 5 | const -[27 28 55] +[32 33 58] |
| `80157474` SLPS_009.90 |  | `0001204c` | `00011ed0` | similarity | 148/143 | 5 | const -[-129 5] +[0xffffc000 -8 10] |
| `8015d6f0` SLPS_009.90 |  | `00094cec` | `00094a64` | gap-align | 87/105 | 5 | const -[] +[0xffff9910 0xffff88d0]; field -[] +[w1+6 w2+10]; calls -[] +[] unpaired 1/5 |
| `801658e8` SLPS_009.90 |  | `000b8200` | `-` | gap-order | 100/99 | 5 | const -[] +[20]; field -[w4+0 w4+12 w4+168 w4+184 w4+4 w4+8] +[w4+16 w4+180 w4+196 w4+20]; globals 8 only PSX, 4 only PSP |
| `801efa50` ETC/COMMU00.EMI#0 |  | `00109a2c` | `-` | similarity | 248/204 | 4 | const -[5] +[9]; field -[] +[w1+3792 w1+8 w1+9 w2+0 w2+12 w2+14 w2+16 w2+18 ...]; globals 10 only PSX, 2 only PSP |
| `801d2110` ETC/COMMU01.EMI#7 |  | `0010c8d8` | `0010c608` | similarity | 370/394 | 4 | const -[480 0xfffc] +[60]; field -[] +[w1+21 w2+16]; globals 7 only PSX, 1 only PSP |
| `801d3038` ETC/COMMU01.EMI#7 |  | `0010d5b0` | `-` | similarity | 335/431 | 4 | const -[] +[0xfffff430 0x7378 0x737c]; field -[] +[w1-175 w2+0]; globals 2 only PSX, 1 only PSP |
| `801d2ce0` ETC/COMMU02.EMI#8 |  | `0010fb48` | `0010f8e0` | callee-align | 68/62 | 4 | const -[0x7bc8 0x7bc9] +[0xf80 0xf90] |
| `801d6cf0` ETC/COMMU02.EMI#8 |  | `00112cf8` | `00112aa4` | similarity | 81/80 | 4 | const -[164 0xfff8] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801d7214` ETC/COMMU02.EMI#8 |  | `00113198` | `-` | similarity | 114/122 | 4 | const -[164 0xfff8] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801d7c64` ETC/COMMU02.EMI#8 |  | `00113c38` | `001139c0` | similarity | 87/88 | 4 | const -[164 0xfff8] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801a3100` ETC/GAME.EMI#0 |  | `000865fc` | `0008633c` | gap-align | 59/40 | 4 | const -[161 193 33 65] +[]; calls -[] +[dce04] unpaired 1/0 |
| `801ae734` ETC/GAME.EMI#0 |  | `00093628` | `0009339c` | gap-align | 200/119 (size 0.6x) | 4 | const -[254 5 0xfeff] +[152]; field -[w1+11 w1+116] +[]; calls -[Msg_OpenSystem] +[] |
| `801afa80` ETC/GAME.EMI#0 |  | `0009a1b8` | `00099f34` | callee-align | 69/66 | 4 | const -[0x780c 0x780e] +[192 224] |
| `801b0e10` ETC/GAME.EMI#0 |  | `0007c608` | `0007c340` | similarity | 58/100 (size 1.7x) | 4 | const -[] +[0xffffc000 0x4000 0x7fff 0x800000]; calls -[] +[7cbe0]; globals 1 only PSX, 6 only PSP |
| `801bfba8` ETC/GAME.EMI#0 |  | `000d635c` | `000d6074` | gap-align | 123/115 | 4 | const -[320] +[]; field -[] +[w1+0 w1+68]; calls -[] +[] unpaired 1/0; globals 3 only PSX, 3 only PSP |
| `801bfe54` ETC/GAME.EMI#0 |  | `000d8aa0` | `000d87b8` | callee-align | 159/175 | 4 | const -[0x800 320] +[-624]; field -[] +[w1-1648 w1+5 w1+69 w2+0 w2+192]; globals 2 only PSX, 2 only PSP |
| `801c70d8` ETC/GAME.EMI#0 |  | `00143c90` | `00143a44` | callee-align | 158/178 | 4 | const -[] +[0xe4048]; field -[] +[w1+12 w1+130 w1+131 w1+1669 w1+9934 w4+1936 w4+9936]; calls -[] +[] unpaired 1/0; globals 9 only PSX, 3 only PSP |
| `801d47bc` ETC/SHOP.EMI#0 |  | `001f28dc` | `001f25c4` | gap-align | 104/49 (size 0.5x) | 4 | const -[133 263] +[157 464]; field -[] +[w1+11 w1+2]; calls -[SE_Play] +[] unpaired 1/0 |
| `801da2a8` ETC/SHOP.EMI#0 |  | `001f5c94` | `001f59c8` | callee-align | 97/88 | 4 | const -[] +[0xffff8537 0xffff80e5 0xffff80d0 0x77dc] |
| `801dbeb0` ETC/SHOP.EMI#0 +3 copies |  | `000bc1c4` | `000bc038` | callee-align | 182/161 | 4 | const -[12] +[32 48]; field -[] +[w4+2000]; calls -[] +[9a620] |
| `801e1d38` ETC/SHOP.EMI#0 |  | `001fc578` | `001fc2cc` | gap-align | 98/90 | 4 | const -[] +[0xffff8940 0xffff88f1 0xffff88dc 0x7870] |
| `801d0dd4` ETC/SISYOU.EMI#0 |  | `0011996c` | `001195c4` | similarity | 167/172 | 4 | const -[0xfffe] +[]; field -[] +[w2+0 w2+16]; calls -[] +[3ff74] unpaired 1/0; globals 1 only PSX, 2 only PSP |
| `801d10ac` ETC/SISYOU.EMI#0 |  | `00119c2c` | `-` | similarity | 138/140 | 4 | const -[0xfffe] +[]; field -[] +[w2+0 w2+16]; calls -[] +[3ff74] unpaired 1/0; globals 1 only PSX, 2 only PSP |
| `801d6734` ETC/START.EMI#8 |  | `000b2c50` | `-` | callee-align | 168/172 | 4 | const -[15 17] +[]; field -[w1+14] +[w1+4 w1+5]; globals 5 only PSX, 3 only PSP |
| `801f9fb8` SCENARIO/SCENA00.EMI#0 |  | `0027016c` | `0026ffa8` | similarity | 163/121 | 4 | const -[0x6200 0xffe20000] +[0xffff9950 0x1e00]; field -[w4+0] +[w1+2 w4+56] |
| `801f7c38` SCENARIO/SCENA02.EMI#0 |  | `00279ec4` | `00279d00` | similarity | 346/225 | 4 | const -[176 9] +[]; field -[] +[w1+4 w1+5 w1+5904 w2-1520]; calls -[97fc8] +[952f0 d7ef0] unpaired 1/0 |
| `801f8cac` SCENARIO/SCENA05.EMI#0 |  | `002846c4` | `00284500` | similarity-weak | 264/168 | 4 | const -[14 15 30 31 40 63 7] +[0x239950]; field -[w1+0] +[w1+2 w2-1520]; calls -[6400 9716c 97fc8 Flag_Clear] +[107174 d82b8] unpaired 1/0 |
| `801fb6dc` SCENARIO/SCENA07.EMI#0 |  | `00290734` | `00290570` | similarity | 556/266 (size 0.5x) | 4 | const -[254 31 0xfff7] +[]; field -[w1+0 w2+0] +[w1-26288]; calls -[6400 95d5c 95db0 9716c 97fc8] +[107174 143f8c d6f20] |
| `801fc2d0` SCENARIO/SCENA07.EMI#0 |  | `00290b90` | `002909cc` | similarity | 738/635 | 4 | const -[0x170000 0x10000] +[]; field -[w1+0] +[w1+1873 w4+336]; calls -[] +[143f8c 8c84c d6f20 d82b8]; globals 3 only PSX, 1 only PSP |
| `801f823c` SCENARIO/SCENA08.EMI#0 |  | `0003d1e8` | `0003cfcc` | callee-align | 186/190 | 4 | const -[-127 -128] +[193 65]; field -[] +[w2+0 w4+2000] |
| `801fa3e4` SCENARIO/SCENA08.EMI#0 |  | `00292c10` | `00292a4c` | similarity | 427/360 | 4 | const -[0x140000 0x1e8000 0xa8000 0xd0000] +[]; field -[] +[w4+336]; calls -[dec] +[d8270] |
| `801fdb14` SCENARIO/SCENA10.EMI#0 |  | `002a0e74` | `002a0cb0` | similarity | 282/343 | 4 | const -[-5 0x118000 0x40000 0x58000] +[6]; field -[] +[w1-26288 w1+17744 w2-240 w4-26160]; calls -[] +[291914] unpaired 1/0 |
| `801fc890` SCENARIO/SCENA12.EMI#0 |  | `002a8f1c` | `002a8d58` | similarity-weak | 226/318 | 4 | const -[] +[0x3f90 0x368000 0x3f8000 0x488000]; field -[] +[w2-240 w4-26160 w4+60] |
| `801fbbc4` SCENARIO/SCENA13.EMI#0 |  | `002acef4` | `002acd30` | gap-align | 47/41 | 4 | field -[] +[w1+48]; calls -[] +[d7e34 d8084] |
| `801fbdd8` SCENARIO/SCENA13.EMI#0 |  | `002ad0c8` | `002acf04` | gap-align | 46/41 | 4 | field -[w1+0] +[w1+16]; calls -[] +[d7e34 d8084] |
| `801fbe10` SCENARIO/SCENA14.EMI#0 |  | `002b1784` | `002b15c0` | gap-align | 61/52 | 4 | field -[w1+0] +[w1+16]; calls -[] +[d7e34 d8084] |
| `801fc130` SCENARIO/SCENA14.EMI#0 |  | `002b1a58` | `002b1894` | gap-align | 44/45 | 4 | field -[] +[w1+48]; calls -[] +[d7e34 d8084] |
| `801fc1d8` SCENARIO/SCENA15.EMI#0 |  | `002b428c` | `002b40c8` | similarity | 474/417 | 4 | const -[0x270000 0x8000] +[0x4e1c]; field -[] +[w4-26160]; calls -[94edc ec8] +[92dc8 d8270] |
| `801fe3c8` SCENARIO/SCENA15.EMI#0 |  | `002b5a90` | `002b58cc` | gap-align | 51/46 | 4 | field -[w1+0] +[w1+16]; calls -[] +[d7e34 d8084] |
| `801f91a8` SCENARIO/SCENA17.EMI#0 |  | `002b856c` | `002b83bc` | gap-order | 35/35 | 4 | const -[0x7b85 88] +[105 0xe50] |
| `801f4768` WORLD00/AREA016.EMI#13 |  | `0022440c` | `00224224` | similarity | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 4 only PSX, 1 only PSP |
| `801f2c74` WORLD00/AREA027.EMI#13 |  | `0002eaac` | `0002e8c4` | similarity | 55/53 | 4 | const -[0x20000 0xb0000 0xc0000] +[152]; field -[] +[w2+20 w4+0 w4+16 w4+4 w4+8]; globals 1 only PSX, 1 only PSP |
| `801d2c30` WORLD00/AREA030.EMI#4 |  | `000197d4` | `00019668` | gap-order | 77/74 | 4 | const -[0x7a40 553] +[-16 0x900] |
| `801d3f54` WORLD00/AREA030.EMI#4 |  | `0001a6ac` | `-` | gap-align | 152/154 | 4 | const -[254 39] +[-7 10]; field -[] +[w4+5968]; calls -[] +[3154] unpaired 0/1 |
| `801d82f4` WORLD00/AREA030.EMI#4 |  | `0001de84` | `0001dc4c` | callee-align | 68/61 | 4 | const -[0x7887 45 96] +[624] |
| `801da0d8` WORLD00/AREA030.EMI#4 |  | `000df364` | `000df074` | gap-order | 60/78 | 4 | const -[0xfffe] +[0xffffb310 0x10080]; field -[] +[w2+0 w4+936]; calls -[] +[] unpaired 0/2 |
| `801db1c0` WORLD00/AREA030.EMI#4 |  | `000e4bdc` | `000e48ec` | similarity | 229/247 | 4 | const -[] +[-2]; field -[] +[w1-6384 w1+9763 w1+9776 w4-6392 w4+28]; calls -[] +[e32cc] unpaired 0/2; globals 5 only PSX, 2 only PSP |
| `801dcd74` WORLD00/AREA030.EMI#4 |  | `000e15ec` | `000e12fc` | gap-align | 45/67 | 4 | const -[-3] +[-7 0x100a2 0x100a4]; field -[] +[w2+0]; calls -[] +[] unpaired 0/1 |
| `801dd824` WORLD00/AREA030.EMI#4 |  | `000e1ad0` | `000e17e0` | gap-align | 70/85 | 4 | const -[] +[-7 0x100e0]; field -[] +[w2+0]; calls -[] +[] unpaired 0/2; globals 1 only PSX, 1 only PSP |
| `801e105c` WORLD00/AREA030.EMI#4 |  | `000e371c` | `000e342c` | callee-align | 191/190 | 4 | const -[56] +[104 224 66]; field -[w1+0 w1+1] +[w1+32 w1+33] |
| `800f5e5c` WORLD00/AREA030.EMI#5 |  | `000e80e0` | `000e7e00` | callee-align | 131/108 | 4 | const -[0x7a80 96] +[-32 192 0xa00]; field -[] +[w4+2000] |
| `801f469c` WORLD00/AREA033.EMI#13 |  | `00227d38` | `00227b50` | gap-align | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 3 only PSX, 1 only PSP |
| `801f4768` WORLD01/AREA045.EMI#13 |  | `0022ea50` | `0022e878` | gap-align | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 3 only PSX, 1 only PSP |
| `801f3ae4` WORLD01/AREA048.EMI#13 |  | `00062f74` | `00062ca8` | callee-align | 155/159 | 4 | const -[56 0xb8000] +[0xffffe190 127 248]; field -[] +[w1+0 w1+1 w1+2] |
| `80117e78` WORLD01/AREA049.EMI#13 |  | `0006c210` | `0006bf44` | similarity | 178/175 | 4 | const -[-128 171 0x78c5 64] +[848]; field -[] +[w1+2 w1+3] |
| `801f35a0` WORLD01/AREA052.EMI#13 |  | `0023150c` | `00231334` | similarity | 129/131 | 4 | const -[0x110000 41 42 0x78000 0x80000 7] +[]; calls -[] +[dce04] unpaired 1/0 |
| `801f47a0` WORLD01/AREA065.EMI#13 |  | `00234de8` | `00234c10` | gap-align | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 6 only PSX, 1 only PSP |
| `801f54b8` WORLD02/AREA077.EMI#14 |  | `000611bc` | `00060ef0` | callee-align | 82/77 | 4 | const -[0xffffc030 0xffffbfd0] +[0xffffc000 0x21800000] |
| `801f4190` WORLD02/AREA082.EMI#13 |  | `0006dbdc` | `0006d910` | similarity | 97/88 | 4 | const -[0x53020 625] +[0x2710 0xa6040] |
| `801f4798` WORLD02/AREA087.EMI#13 |  | `0023ba4c` | `0023b880` | gap-align | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 3 only PSX, 1 only PSP |
| `801f52c8` WORLD02/AREA088.EMI#13 |  | `0023d9cc` | `0023d800` | gap-align | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 5 only PSX, 1 only PSP |
| `801f3194` WORLD02/AREA102.EMI#13 |  | `0006b6e4` | `0006b418` | callee-align | 49/51 | 4 | const -[-128 361] +[0xffffc000 341] |
| `801f2cb0` WORLD02/AREA103.EMI#13 |  | `000302a0` | `000300c8` | similarity | 98/101 | 4 | const -[12 0xfff90000] +[0x4bc]; field -[] +[w2+16 w2+18 w2+20 w4+2000 w4+8]; globals 1 only PSX, 1 only PSP |
| `801f5174` WORLD02/AREA103.EMI#13 |  | `0006a478` | `0006a1ac` | similarity | 155/140 | 4 | const -[-128 127 0xf80000] +[0xffffc000 -8] |
| `801f6360` WORLD02/AREA104.EMI#14 |  | `0024486c` | `002446a8` | gap-align | 127/116 | 4 | const -[12 250] +[170 282]; field -[w1+3] +[] |
| `801f3940` WORLD02/AREA106.EMI#13 |  | `00245ea8` | `00245ce4` | gap-align | 70/76 | 4 | const -[0x118000 0x128000 0x1f8000 0xb8000] +[] |
| `801f2f50` WORLD03/AREA114.EMI#13 |  | `002499d8` | `00249814` | gap-align | 51/126 (size 2.4x) | 4 | const -[] +[11 12 13 6] |
| `801f4768` WORLD03/AREA115.EMI#13 |  | `0024b5f8` | `0024b434` | gap-align | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 3 only PSX, 1 only PSP |
| `801f2f5c` WORLD03/AREA117.EMI#13 |  | `00064404` | `00064138` | similarity-weak | 42/55 | 4 | const -[0xffea0000 0xffeb0000 0xfffd8000 0x18000] +[] |
| `801f2edc` WORLD03/AREA119.EMI#13 |  | `000669e8` | `0006671c` | similarity | 73/89 | 4 | const -[0xffbd0000 0xffc10000 0xffc20000 0x10000] +[0x428000]; field -[] +[w1+16384] |
| `801f2d20` WORLD03/AREA135.EMI#14 |  | `0003eb3c` | `0003e920` | similarity | 173/184 | 4 | const -[-480] +[-960]; field -[w1+26] +[w4+20 w4+28]; calls -[] +[8fc28] |
| `801f5a44` WORLD03/AREA135.EMI#14 |  | `002537c4` | `00253600` | gap-align | 87/70 | 4 | const -[-6] +[0x7b6c 7]; calls -[] +[8b200] |
| `801f3f84` WORLD03/AREA139.EMI#13 |  | `00070aa4` | `000707dc` | similarity | 211/179 | 4 | const -[-127 -128] +[0xffffc040 0xffffbfc0]; field -[] +[w2+17552] |
| `801f46d4` WORLD03/AREA151.EMI#13 |  | `0025c974` | `0025c7b0` | gap-align | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 3 only PSX, 1 only PSP |
| `801f4628` WORLD04/AREA152.EMI#13 |  | `0025e7a0` | `0025e5dc` | gap-align | 355/333 | 4 | const -[299 0x78cb] +[944]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w1+16385 w4+0]; globals 6 only PSX, 1 only PSP |
| `801f31d0` WORLD04/AREA172.EMI#13 |  | `00055d74` | `00055b64` | similarity | 57/56 | 4 | const -[] +[44 6 0xe48c0]; field -[w1+6] +[w1+0 w2+42]; calls -[] +[] unpaired 2/0 |
| `801f2dac` WORLD04/AREA173.EMI#14 |  | `00081a20` | `00081758` | similarity-weak | 78/102 | 4 | const -[] +[0x104002 0x650 34]; field -[] +[w1+16384 w1+16385 w2+0 w4+0]; globals 4 only PSX, 1 only PSP |
| `801f4de0` WORLD04/AREA189.EMI#13 |  | `0026b1d0` | `0026b00c` | similarity | 139/137 | 4 | const -[320] +[]; field -[] +[w2+15952]; calls -[] +[d5990 dacb4 dc1d0] unpaired 1/0; globals 1 only PSX, 1 only PSP |
| `801f2dc8` WORLD04/AREA193.EMI#13 |  | `0026d44c` | `0026d288` | gap-align | 27/26 | 4 | const -[] +[12 14 29 30] |
| `801f41f4` WORLD04/AREA197.EMI#13 |  | `0026e1e0` | `0026e01c` | gap-align | 70/102 | 4 | const -[0x28000 0x8000 0x50000 0x58000 0xd0000] +[] |
| `80154ef4` SLPS_009.90 |  | `0000eabc` | `0000e964` | caller-vote | 71/94 | 4 | const -[60] +[0x104002 0x104028 12 15]; field -[] +[w1+16384 w1+16385 w2+0 w4+0 w4+4] |
| `801564c4` SLPS_009.90 |  | `0000f478` | `-` | caller-vote | 59/63 | 4 | const -[0xffffff 0xff000000 56] +[0x8fcc 58] |
| `801581b8` SLPS_009.90 |  | `00012a8c` | `00012918` | caller-vote | 163/188 | 4 | const -[60] +[0x104002 0x104030 12]; field -[w1+1 w2+2] +[w1+16384 w1+16385] |
| `80161e44` SLPS_009.90 |  | `00096b08` | `00096884` | caller-vote | 216/207 | 4 | const -[15 0x3fff 25 43 64] +[0xb00 480]; field -[] +[w1+121 w4+5968]; calls -[] +[] unpaired 0/1 |
| `80166150` SLPS_009.90 |  | `000b8a80` | `000b8868` | callee-align | 86/105 | 4 | const -[32 36 65] +[13 78] |
| `801af214` SLPS_009.90 |  | `00099f00` | `00099c7c` | caller-vote | 0/44 (size 45.0x) | 4 | const -[] +[140 40]; field -[] +[w1+4 w1+5 w1+6 w2+10 w2+12 w2+14 w2+8]; calls -[] +[Packet_Commit] |
| `801ef214` ETC/COMMU00.EMI#0 |  | `001077bc` | `001074dc` | similarity | 83/72 | 3 | const -[480 9] +[60] |
| `801ef398` ETC/COMMU00.EMI#0 |  | `001078e0` | `00107600` | similarity | 88/75 | 3 | const -[480 9] +[60] |
| `801d1158` ETC/COMMU01.EMI#7 |  | `0010a86c` | `0010a5a0` | similarity | 104/103 | 3 | const -[-9 68] +[-418]; field -[] +[w1+21 w2+14] |
| `801d3f08` ETC/COMMU01.EMI#7 |  | `0010bd54` | `0010ba88` | callee-align | 75/59 | 3 | const -[0x7840 5] +[10 256] |
| `801d0fc8` ETC/COMMU02.EMI#8 |  | `001152e4` | `-` | similarity | 229/196 | 3 | const -[25 5] +[0xfffff9d0]; field -[] +[w1+0 w1+9886 w1+9890 w2+14 w2+16] |
| `801d1894` ETC/COMMU02.EMI#8 |  | `0010e784` | `0010e4a8` | similarity | 56/60 | 3 | const -[0x200000 0x300000] +[32 48]; globals 2 only PSX, 1 only PSP |
| `801d316c` ETC/COMMU02.EMI#8 |  | `0010ff48` | `0010fce0` | callee-align | 70/67 | 3 | const -[] +[0xfffff792 0xfffff78c 0x7380] |
| `801d5e4c` ETC/COMMU02.EMI#8 |  | `00111e2c` | `00111bfc` | similarity | 53/59 | 3 | const -[0xfff8] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801d6288` ETC/COMMU02.EMI#8 |  | `00112238` | `-` | similarity | 52/55 | 3 | const -[0xfff8] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801d6bbc` ETC/COMMU02.EMI#8 |  | `00112bb0` | `0011295c` | similarity | 59/67 | 3 | const -[0xfff8] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801d9c44` ETC/COMMU02.EMI#8 |  | `00116a80` | `00116764` | callee-align | 349/287 | 3 | const -[] +[-16]; field -[] +[w2+0 w2+2 w2+4 w2+6]; calls -[] +[9a620] |
| `801d16f4` ETC/COMMU05.EMI#7 |  | `001195d0` | `00119228` | callee-align | 137/125 | 3 | const -[12 240] +[254]; field -[] +[w1+0 w1+1] |
| `801994e8` ETC/GAME.EMI#0 |  | `00007f08` | `0000887c` | gap-align | 51/55 | 3 | const -[] +[0x3b9ac9ff]; field -[w4+0] +[]; calls -[] +[e33b8 e5f3c] |
| `8019ec74` ETC/GAME.EMI#0 |  | `00023b1c` | `00023930` | gap-order | 78/72 | 3 | const -[] +[32 0xe4d40]; field -[] +[w1+0 w2+2 w4+0 w4+16 w4+20 w4+24 w4+4 w4+8]; calls -[7c608] +[] |
| `801a2ee0` ETC/GAME.EMI#0 |  | `000882d8` | `00088024` | similarity | 135/266 (size 2.0x) | 3 | const -[0xffff0000 5 7] +[0xffff8000]; calls -[8670c] +[] |
| `801a4418` ETC/GAME.EMI#0 |  | `000873e4` | `00087124` | gap-align | 81/85 | 3 | const -[] +[52 56 60]; field -[w1+24 w1+28 w4+60] +[w4+0 w4+24 w4+28] |
| `801adf2c` ETC/GAME.EMI#0 |  | `00092e54` | `00092bc8` | gap-align | 208/107 (size 0.5x) | 3 | const -[254 0xfeff] +[152]; field -[w1+11 w1+116] +[]; calls -[Msg_OpenSystem] +[] |
| `801aeac0` ETC/GAME.EMI#0 |  | `0009399c` | `00093710` | similarity | 105/101 | 3 | const -[] +[52 56 60]; field -[w4+52 w4+56 w4+60] +[w4+0 w4+12]; calls -[] +[] unpaired 1/0 |
| `801b0d08` ETC/GAME.EMI#0 |  | `0009ab6c` | `0009a8e8` | callee-align | 62/66 | 3 | const -[6 0x2aaa0000] +[12] |
| `801b1560` ETC/GAME.EMI#0 |  | `000cb6f4` | `000cb408` | callee-align | 363/320 | 3 | const -[0x190000 6] +[]; field -[] +[w4+17424 w4+5968]; calls -[] +[dce04] unpaired 2/0 |
| `801b65b8` ETC/GAME.EMI#0 |  | `000ca6a4` | `000ca3b8` | gap-align | 42/39 | 3 | const -[320] +[]; globals 2 only PSX, 2 only PSP |
| `801b80cc` ETC/GAME.EMI#0 |  | `000ce9fc` | `000ce710` | gap-align | 92/98 | 3 | const -[] +[52 56 60]; field -[w1+28 w4+60] +[w4+0 w4+28] |
| `801bdd58` ETC/GAME.EMI#0 |  | `000d53f8` | `000d510c` | similarity | 298/311 | 3 | const -[0xfffff 0x60000 0xfff10000 0xffff0000 0x10000] +[]; field -[w1+112] +[w4+0 w4+112 w4+4] |
| `801c1750` ETC/GAME.EMI#0 |  | `000d7228` | `000d6f40` | gap-align | 97/81 | 3 | const -[0x3f8 320] +[]; globals 3 only PSX, 1 only PSP |
| `801c1904` ETC/GAME.EMI#0 |  | `000d736c` | `000d7084` | gap-align | 77/56 | 3 | const -[0x3f8 320] +[]; globals 1 only PSX, 1 only PSP |
| `801c1aac` ETC/GAME.EMI#0 |  | `000d7498` | `000d71b0` | gap-order | 124/107 | 3 | const -[320] +[]; field -[w1+0] +[]; globals 7 only PSX, 2 only PSP |
| `801c1e40` ETC/GAME.EMI#0 |  | `000d77b4` | `000d74cc` | callee-align | 80/68 | 3 | const -[0xffe7 0xfff7] +[-513] |
| `801c26b0` ETC/GAME.EMI#0 |  | `000d7ef0` | `000d7c08` | gap-align | 93/84 | 3 | const -[164 320] +[20] |
| `801c388c` ETC/GAME.EMI#0 |  | `000d9e70` | `000d9b88` | gap-order | 56/48 | 3 | const -[-256 0xc0c00000 0xc0c0c000] +[]; field -[w4+92] +[w4+5968] |
| `801c7bac` ETC/GAME.EMI#0 |  | `00142d28` | `00142af4` | gap-align | 127/132 | 3 | const -[0x50000] +[0xe4048]; field -[] +[w1+130 w4+0 w4+4]; globals 6 only PSX, 1 only PSP |
| `801d0e54` ETC/GAME.EMI#1 |  | `0000583c` | `000061b0` | similarity | 39/41 | 3 | const -[] +[0x6e64]; calls -[] +[31c0 8c694] |
| `801d1664` ETC/GAME.EMI#1 |  | `00005ee4` | `00006858` | callee-align | 61/38 | 3 | const -[112 12 240] +[] |
| `801d1880` ETC/GAME.EMI#1 |  | `00006078` | `000069ec` | similarity | 116/102 | 3 | const -[12 5 512] +[640] |
| `801d3af4` ETC/SHOP.EMI#0 |  | `001f12f8` | `001f0fe0` | similarity | 42/39 | 3 | const -[0xfffe] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801d3ba8` ETC/SHOP.EMI#0 |  | `001f139c` | `001f1084` | similarity | 62/67 | 3 | const -[] +[0x77a0]; calls -[] +[3ff74] unpaired 1/0 |
| `801d60c0` ETC/SHOP.EMI#0 |  | `001f2d7c` | `001f2a64` | similarity | 50/49 | 3 | const -[0xfffe] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801d8054` ETC/SHOP.EMI#0 |  | `001f4a8c` | `001f4788` | callee-align | 85/88 | 3 | const -[0xfffe] +[]; calls -[] +[3ff74 d5990] unpaired 1/0 |
| `801d8270` ETC/SHOP.EMI#0 |  | `001f7060` | `001f6d94` | callee-align | 264/281 | 3 | const -[160] +[0x77c8 0x7ff8] |
| `801dc18c` ETC/SHOP.EMI#0 +3 copies |  | `000be740` | `000be5d4` | callee-align | 321/330 | 3 | const -[] +[-16]; field -[] +[w1+12 w1+13 w1+4 w1+5 w1+6 w2+0 w2+10 w2+14 ...]; calls -[] +[9a620] |
| `801dd304` ETC/SHOP.EMI#0 +3 copies |  | `000bc448` | `000bc2bc` | callee-align | 137/120 | 3 | const -[12] +[]; field -[] +[w4+2000]; calls -[] +[9a620] |
| `801de1b8` ETC/SHOP.EMI#0 |  | `001fa6d0` | `001fa40c` | gap-align | 104/82 | 3 | const -[164 6] +[]; calls -[] +[d5990]; globals 3 only PSX, 1 only PSP |
| `801ded58` ETC/SHOP.EMI#0 |  | `001faa34` | `001fa770` | similarity | 104/110 | 3 | const -[0xfffe] +[]; calls -[] +[3ff74 d5990] unpaired 1/0 |
| `801df59c` ETC/SHOP.EMI#0 |  | `001fae40` | `001fab7c` | similarity | 148/158 | 3 | const -[0xfffe] +[]; field -[] +[w1+4 w2+0 w2+2]; calls -[] +[3ff74 d5990] unpaired 1/0 |
| `801e26c0` ETC/SHOP.EMI#0 |  | `001fe520` | `001fe294` | gap-align | 72/77 | 3 | const -[] +[74]; calls -[] +[3ff74] unpaired 1/0 |
| `801d21c4` ETC/START.EMI#8 |  | `000af3d8` | `000af198` | callee-align | 22/23 | 3 | const -[96] +[160 161]; field -[] +[w2+54]; calls -[] +[dce04] unpaired 1/0 |
| `801d2cb0` ETC/START.EMI#8 |  | `000b03ac` | `000b016c` | similarity | 60/47 | 3 | const -[12] +[]; calls -[] +[d5990]; globals 2 only PSX, 2 only PSP |
| `801d31a8` ETC/START.EMI#8 |  | `000b1ba0` | `000b1960` | similarity | 298/354 | 3 | const -[-8] +[]; field -[] +[w1+756 w1+9291 w2+16 w2+4 w4+0]; calls -[] +[9a80c] unpaired 1/1 |
| `801d7824` ETC/START.EMI#8 |  | `000b53e8` | `-` | similarity | 311/306 | 3 | const -[] +[468]; field -[] +[w1+1 w1+10 w1+11 w1+12 w1+147 w1+1666 w1+2 w1+3 ...]; calls -[] +[9b540 d5990] |
| `801d9ac8` ETC/START.EMI#8 |  | `000b6d40` | `000b6b48` | similarity | 391/321 | 3 | const -[12] +[0x685 0x1410]; field -[w1+0] +[w1+1664 w2+14] |
| `801db650` ETC/START.EMI#8 |  | `000bcabc` | `000bc934` | callee-align | 264/281 | 3 | const -[160] +[0xffffc108 0x71a8] |
| `801e1670` ETC/START.EMI#8 |  | `000c01f4` | `000c0074` | similarity | 59/59 | 3 | const -[0xfff0] +[]; calls -[] +[3ff74] unpaired 1/0 |
| `801e2d04` ETC/START.EMI#8 |  | `000c0c84` | `-` | similarity-weak | 151/69 (size 0.5x) | 3 | const -[202 208] +[0xffffc9c4]; field -[w1+12 w1+13 w4+0 w4+11 w4+12 w4+15 w4+3 w4+4 ...] +[]; calls -[Msg_SystemPtr] +[] |
| `801e2f78` ETC/START.EMI#8 |  | `000c0d98` | `000c0b8c` | similarity | 174/144 | 3 | const -[] +[0xffffca42]; calls -[a050] +[] |
| `801e4430` ETC/START.EMI#8 |  | `000c24b4` | `000c22b4` | similarity | 21/17 | 3 | calls -[] +[c3480] unpaired 2/0 |
| `801ce8ac` PLCHAR/PLP278.EMI#0 +7 copies |  | `000ea6ec` | `000ea40c` | gap-align | 82/85 | 3 | const -[116] +[]; calls -[] +[d5b40] |
| `801d1b10` SCENARIO/SCE15EF1.EMI#0 |  | `00044990` | `00044760` | callee-align | 111/113 | 3 | const -[12] +[]; calls -[] +[9c504 9c5e4] |
| `801f7670` SCENARIO/SCENA00.EMI#0 |  | `0001741c` | `000172a0` | similarity | 142/135 | 3 | const -[0x138000 0xb0000] +[192] |
| `801f8d90` SCENARIO/SCENA00.EMI#0 |  | `000178b8` | `0001773c` | gap-align | 230/226 | 3 | field -[w1+16] +[w4+16 w4+2000 w4+28]; calls -[] +[8fc28]; globals 3 only PSX, 1 only PSP |
| `801fb744` SCENARIO/SCENA00.EMI#0 |  | `0026f558` | `0026f394` | similarity | 106/107 | 3 | const -[0x158000 0x1a8000] +[0xffff9950]; field -[] +[w1+2] |
| `801fcec0` SCENARIO/SCENA01.EMI#0 |  | `00276150` | `00275f8c` | similarity | 212/257 | 3 | const -[0x50000] +[]; field -[] +[w1+2 w1+3 w1+5904 w2+12]; calls -[97fc8] +[952f0 d7ef0] unpaired 1/0 |
| `801fc044` SCENARIO/SCENA02.EMI#0 |  | `00278298` | `002780d4` | callee-align | 41/37 | 3 | const -[0xfffc0000 0xfffe8000 0x18000] +[] |
| `801fc1a4` SCENARIO/SCENA02.EMI#0 |  | `002783e0` | `0027821c` | callee-align | 43/39 | 3 | const -[0xfffc0000 0xfffe8000 0x10000 0x18000] +[] |
| `801fc3b4` SCENARIO/SCENA02.EMI#0 |  | `002785c0` | `002783fc` | callee-align | 43/38 | 3 | const -[0xfffb8000 0xfffe8000 0x18000] +[] |
| `801fc5d0` SCENARIO/SCENA02.EMI#0 |  | `002787a4` | `002785e0` | similarity | 41/38 | 3 | const -[0x30000 0xffff0000 0x18000] +[] |
| `801f7e6c` SCENARIO/SCENA03.EMI#0 |  | `0002954c` | `00029368` | gap-order | 62/76 | 3 | const -[] +[52 56 60]; field -[w4+52 w4+56 w4+60] +[] |
| `801fc9dc` SCENARIO/SCENA03.EMI#0 |  | `00280cbc` | `00280af8` | similarity | 54/73 | 3 | const -[0x28000 0x4c0000] +[0xfffffb90]; field -[] +[w1+4 w1+5] |
| `801fcdb4` SCENARIO/SCENA03.EMI#0 |  | `0028110c` | `00280f48` | gap-order | 22/29 | 3 | const -[] +[0x2740]; calls -[] +[8ca44] |
| `801f6d44` SCENARIO/SCENA04.EMI#0 |  | `0002b408` | `0002b22c` | gap-align | 34/36 | 3 | const -[0x128000] +[12 24]; field -[w4+12 w4+28 w4+32 w4+36 w4+40 w4+44 w4+52 w4+56] +[w4+0 w4+4 w4+8] |
| `801fa010` SCENARIO/SCENA04.EMI#0 |  | `00282a84` | `002828c0` | gap-order | 22/29 | 3 | const -[] +[0x2798]; calls -[] +[8ca44] |
| `801fcd68` SCENARIO/SCENA05.EMI#0 |  | `00288364` | `002881a0` | similarity-weak | 216/361 | 3 | const -[0xc8000 0x18000] +[0x930000]; field -[] +[w2-240 w4-26160] |
| `801f88b8` SCENARIO/SCENA07.EMI#0 |  | `00038688` | `00038478` | callee-align | 195/177 | 3 | const -[] +[24]; field -[w4+24 w4+28 w4+32 w4+36 w4+40 w4+44] +[w2+40 w2+42 w2+766]; calls -[] +[9c504 9c5e4] |
| `801f8be8` SCENARIO/SCENA07.EMI#0 |  | `0003894c` | `0003873c` | callee-align | 198/210 | 3 | const -[12 18 42] +[]; field -[w2-2 w2+0 w2+2 w2+4 w4-2] +[w4+16 w4+20 w4+24 w4+28 w4+32] |
| `801fa38c` SCENARIO/SCENA07.EMI#0 |  | `0003a08c` | `00039e60` | gap-align | 159/106 | 3 | const -[0x2000000 482 512] +[32 52 63 64]; field -[w4+52 w4+56 w4+60] +[]; calls -[] +[9c504]; globals 1 only PSX, 1 only PSP |
| `801f8d98` SCENARIO/SCENA08.EMI#0 |  | `0003d86c` | `0003d650` | gap-align | 195/177 | 3 | const -[] +[24]; field -[w4+24 w4+28 w4+32 w4+36 w4+40 w4+44] +[w2+40 w2+42 w2+766]; calls -[] +[9c504 9c5e4] |
| `801f90c8` SCENARIO/SCENA08.EMI#0 |  | `0003db30` | `0003d914` | gap-align | 198/210 | 3 | const -[12 18 42] +[]; field -[w2-2 w2+0 w2+2 w2+4 w4-2] +[w4+16 w4+20 w4+24 w4+28 w4+32] |
| `801fd704` SCENARIO/SCENA08.EMI#0 |  | `00294acc` | `00294908` | similarity | 482/427 | 3 | const -[0xf0000] +[]; field -[w1+0] +[w1-26288 w4+336]; calls -[97fc8] +[143f8c 8c84c d6f20 d8270] |
| `801f9498` SCENARIO/SCENA09.EMI#0 |  | `0029789c` | `002976d8` | similarity | 677/639 | 3 | const -[] +[0xffff8000]; field -[w1+0] +[w1-26288 w1+5904]; calls -[97fc8] +[952f0 d7ef0] unpaired 1/0 |
| `801f9fc4` SCENARIO/SCENA10.EMI#0 |  | `0029c300` | `0029c13c` | gap-align | 15/15 | 3 | const -[] +[0x2e88]; calls -[] +[89cec] |
| `801fe5b0` SCENARIO/SCENA10.EMI#0 |  | `002a1664` | `002a14a0` | gap-align | 22/22 | 3 | const -[254] +[]; calls -[] +[d7e34] |
| `801f75ec` SCENARIO/SCENA12.EMI#0 |  | `0004b154` | `0004af3c` | similarity | 27/27 | 3 | const -[] +[0xdc0]; calls -[] +[3ff74] unpaired 1/0 |
| `801fb464` SCENARIO/SCENA13.EMI#0 |  | `002ac754` | `002ac590` | similarity | 325/452 | 3 | const -[0x148000 0x28000 0x38000] +[]; field -[] +[w1+121 w1+17744 w2-240 w4-26160] |
| `801f924c` SCENARIO/SCENA14.EMI#0 |  | `002ad4b4` | `002ad2f0` | gap-align | 51/46 | 3 | const -[0x140000 0x63ff] +[0xffffec00]; field -[w4+0] +[w2-24368] |
| `801f71b0` SCENARIO/SCENA15.EMI#0 |  | `00026f0c` | `00026d18` | callee-align | 202/203 | 3 | const -[17] +[24 28]; calls -[] +[9c504] |
| `801f7e2c` SCENARIO/SCENA15.EMI#0 |  | `000512dc` | `000510d0` | similarity | 65/60 | 3 | const -[0xff0000 0xffff00 0x307000] +[]; field -[w4+92] +[] |
| `801fdb84` SCENARIO/SCENA15.EMI#0 |  | `002b2320` | `002b215c` | gap-order | 33/27 | 3 | const -[19] +[38]; field -[] +[w1+1]; globals 1 only PSX, 1 only PSP |
| `801f8818` SCENARIO/SCENA17.EMI#0 |  | `002b812c` | `002b7f7c` | similarity | 22/24 | 3 | const -[] +[0x4e1c]; calls -[94edc ec8] +[] |
| `801f8b58` SCENARIO/SCENA17.EMI#0 |  | `002b8abc` | `002b890c` | similarity | 241/226 | 3 | const -[5 6] +[0xffffe190 128]; field -[w1+3] +[] |
| `801f8ff4` SCENARIO/SCENA17.EMI#0 |  | `002b83e8` | `002b8238` | similarity | 65/57 | 3 | const -[12 0x7b40] +[0xd00]; field -[w1+3] +[] |
| `801f954c` SCENARIO/SCENA17.EMI#0 |  | `002b8908` | `002b8758` | similarity | 77/81 | 3 | const -[0xf0000] +[]; field -[] +[w2-240]; calls -[dec] +[970fc d7ef0] unpaired 1/0 |
| `801f96c4` SCENARIO/SCENA17.EMI#0 |  | `002b8a80` | `002b88d0` | gap-align | 17/15 | 3 | const -[] +[0x4e1c]; field -[] +[w1+3]; calls -[94edc ec8] +[] |
| `801f2cec` WORLD00/AREA013.EMI#13 |  | `0022258c` | `002223a4` | similarity | 32/33 | 3 | const -[0x1c8000 0x7fff] +[]; calls -[] +[8f78c] |
| `801f3b00` WORLD00/AREA016.EMI#13 |  | `0022408c` | `00223ea4` | similarity | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f31cc` WORLD00/AREA027.EMI#13 |  | `000606b4` | `000603e8` | similarity | 53/65 | 3 | const -[] +[0x13500000 0x13510000 0x470000] |
| `801d2108` WORLD00/AREA030.EMI#4 |  | `00018dd8` | `00018c6c` | gap-align | 167/155 | 3 | const -[0x14ffff 0x150000 0x3bffff] +[] |
| `801d23f0` WORLD00/AREA030.EMI#4 |  | `00019050` | `00018ee4` | gap-align | 107/116 | 3 | const -[0x14ffff 0x150000 0x3bffff] +[]; field -[] +[w4+4 w4+5968] |
| `801d3a50` WORLD00/AREA030.EMI#4 |  | `0001a20c` | `0001a0a0` | gap-align | 120/131 | 3 | const -[254] +[-12 257]; calls -[] +[] unpaired 0/1 |
| `801d469c` WORLD00/AREA030.EMI#4 |  | `0001b178` | `0001affc` | gap-align | 112/134 | 3 | const -[0xfffe] +[-16 17]; calls -[] +[] unpaired 0/1 |
| `801d5064` WORLD00/AREA030.EMI#4 |  | `0001bbe0` | `0001ba64` | gap-align | 142/166 | 3 | const -[0xfffe] +[-16 17]; calls -[] +[] unpaired 0/1 |
| `801d5590` WORLD00/AREA030.EMI#4 |  | `0001c108` | `0001bf8c` | gap-align | 83/106 | 3 | const -[0xfffe] +[-16 17]; calls -[] +[] unpaired 0/1 |
| `801d9d24` WORLD00/AREA030.EMI#4 |  | `000de418` | `000de130` | gap-align | 18/32 (size 1.7x) | 3 | const -[] +[0xffffb310]; field -[] +[w4+24 w4+948 w4+952]; calls -[] +[] unpaired 0/3 |
| `801d9ec8` WORLD00/AREA030.EMI#4 |  | `000df08c` | `-` | gap-order | 94/131 | 3 | const -[17] +[0xffffe850]; field -[] +[w1+117 w1+349 w1+4 w1+9766]; calls -[] +[] unpaired 0/2 |
| `801da80c` WORLD00/AREA030.EMI#4 |  | `000df98c` | `000df69c` | gap-align | 73/71 | 3 | const -[0x110000 0xfff9ffff 0xefffe] +[]; field -[] +[w4+16 w4+20 w4+52 w4+5968] |
| `801dd9f8` WORLD00/AREA030.EMI#4 |  | `000e1ce4` | `000e19f4` | gap-align | 111/119 | 3 | const -[0x3e0000 0xa0000] +[]; field -[w1+4 w1+5 w1+6 w2+10 w2+12 w2+14 w2+8] +[]; calls -[] +[] unpaired 0/2 |
| `801e1378` WORLD00/AREA030.EMI#4 |  | `000e3a14` | `000e3724` | gap-order | 184/127 | 3 | const -[] +[0xffffdea2 0xffffde6d 0x72e4]; field -[w1+0 w1+1 w1+2 w1+3 w2+56 w4+0 w4+11 w4+12 ...] +[w1+32 w1+33 w1+34 w1+35 w2+84] |
| `801e17b0` WORLD00/AREA030.EMI#4 |  | `000e3d3c` | `000e3a4c` | gap-order | 156/154 | 3 | const -[] +[0x1007a 0x1007c 0x1007e]; field -[] +[w2+0] |
| `800f550c` WORLD00/AREA030.EMI#5 |  | `000e93bc` | `000e90dc` | similarity | 179/184 | 3 | const -[0x2001] +[0x1009c]; field -[] +[w2+0 w2+4]; calls -[] +[1e18c]; globals 1 only PSX, 1 only PSP |
| `800f5828` WORLD00/AREA030.EMI#5 |  | `000e7aac` | `-` | gap-align | 157/168 | 3 | const -[] +[0x1008c 0x10094]; field -[] +[w1+1 w1+9783 w2+0]; calls -[] +[1e18c]; globals 1 only PSX, 1 only PSP |
| `801f3a34` WORLD00/AREA033.EMI#13 |  | `002279b8` | `002277d0` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f3508` WORLD00/AREA037.EMI#13 +1 copies |  | `00228c78` | `00228a90` | gap-align | 14/13 | 3 | const -[96] +[134 136] |
| `801f2c04` WORLD01/AREA040.EMI#13 |  | `002297d8` | `002295f0` | gap-align | 21/23 | 3 | const -[] +[0x5024]; field -[] +[w4-25520]; calls -[] +[89cec] |
| `801f4288` WORLD01/AREA041.EMI#13 |  | `0022aeb8` | `0022acd0` | gap-align | 14/18 | 3 | const -[] +[-59 0x650]; globals 1 only PSX, 1 only PSP |
| `801f5070` WORLD01/AREA042.EMI#13 |  | `0022b5d4` | `0022b3ec` | similarity | 135/89 | 3 | const -[0x108000] +[]; calls -[] +[8b6c8] |
| `801f3b00` WORLD01/AREA045.EMI#13 |  | `0022e6d0` | `0022e4f8` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f37f4` WORLD01/AREA048.EMI#13 |  | `00062cd0` | `00062a04` | similarity-weak | 26/25 | 3 | const -[0x878000] +[0x860000 0x880000] |
| `801f3874` WORLD01/AREA048.EMI#13 |  | `00062d34` | `00062a68` | similarity | 43/56 | 3 | const -[128 17 0x858000] +[0x880000] |
| `801f3944` WORLD01/AREA048.EMI#13 |  | `00062e18` | `00062b4c` | similarity | 31/29 | 3 | const -[0x878000] +[0x860000 0x880000] |
| `801f3aa4` WORLD01/AREA048.EMI#13 |  | `00062f38` | `00062c6c` | similarity-weak | 15/15 | 3 | const -[0x888000] +[0x850000 0x890000] |
| `801f3414` WORLD01/AREA052.EMI#13 |  | `00230a24` | `0023084c` | gap-align | 92/93 | 3 | const -[] +[0xff00]; field -[w1+24 w1+25] +[]; calls -[] +[8f78c dce04] unpaired 1/0 |
| `801f3b38` WORLD01/AREA065.EMI#13 |  | `00234a68` | `00234890` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f3930` WORLD01/AREA068.EMI#13 +6 copies |  | `00228b60` | `00228978` | gap-align | 10/13 | 3 | const -[] +[6 96]; globals 1 only PSX, 1 only PSP |
| `801f32fc` WORLD01/AREA075.EMI#13 |  | `00236a8c` | `002368b4` | gap-align | 49/60 | 3 | const -[0x30000] +[]; calls -[] +[89cec] |
| `801f3d24` WORLD02/AREA077.EMI#14 |  | `00039bd8` | `000399ac` | similarity | 42/53 | 3 | field -[] +[w1+3330]; calls -[] +[3154 3a08c] unpaired 2/0 |
| `801f3e48` WORLD02/AREA077.EMI#14 |  | `00039cfc` | `00039ad0` | similarity | 45/50 | 3 | const -[0xb8000] +[44 0xe5504] |
| `801f3f08` WORLD02/AREA077.EMI#14 |  | `00039dc8` | `00039b9c` | similarity | 65/64 | 3 | const -[] +[44 0xe5504]; field -[] +[w2+36 w2+42 w4+2000]; calls -[] +[3154]; globals 1 only PSX, 1 only PSP |
| `801f3264` WORLD02/AREA082.EMI#13 |  | `00028e8c` | `00028ca8` | similarity-weak | 36/43 | 3 | const -[12] +[]; field -[w2+100 w2+104 w2+108] +[w4+100 w4+104 w4+108 w4+2000]; calls -[] +[28f38] unpaired 1/0 |
| `801f3aac` WORLD02/AREA082.EMI#13 |  | `0002ccac` | `0002cacc` | caller-vote | 41/41 | 3 | const -[] +[20]; field -[] +[w1+0 w2+2 w4+12]; calls -[] +[263dc] unpaired 1/0 |
| `801f373c` WORLD02/AREA085.EMI#13 |  | `002397b8` | `002395ec` | gap-align | 32/33 | 3 | const -[0xffea0000] +[]; calls -[] +[106fec] |
| `801f3b34` WORLD02/AREA087.EMI#13 |  | `0023b6cc` | `0023b500` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f2ce4` WORLD02/AREA088.EMI#13 |  | `0004ce0c` | `0004cbfc` | similarity | 34/44 | 3 | const -[0x170000 0x4800 0x10000] +[9]; field -[] +[w1+0] |
| `801f2f28` WORLD02/AREA088.EMI#13 |  | `0004d200` | `0004cff0` | similarity | 195/177 | 3 | const -[] +[24]; field -[w4+24 w4+28 w4+32 w4+36 w4+40 w4+44] +[w2+40 w2+42 w2+766]; calls -[] +[37e90 9c504 9c5e4] unpaired 1/0 |
| `801f4664` WORLD02/AREA088.EMI#13 |  | `0023d64c` | `0023d480` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f2c48` WORLD02/AREA098.EMI#13 |  | `000428b4` | `00042684` | similarity | 135/141 | 3 | const -[] +[14]; field -[w4+12 w4+4 w4+8] +[w1+726 w4+5968]; calls -[3708] +[3154] unpaired 0/1; globals 6 only PSX, 2 only PSP |
| `801f42e8` WORLD02/AREA098.EMI#13 |  | `00043dbc` | `00043b8c` | caller-vote | 101/109 | 3 | const -[12] +[56]; field -[] +[w1+0 w1+1 w1+2 w1+3 w1+4 w4+12 w4+16 w4+2000 ...]; globals 2 only PSX, 1 only PSP |
| `801f48e8` WORLD02/AREA098.EMI#13 +1 copies |  | `00044f9c` | `00044d74` | callee-align | 170/177 | 3 | const -[0xffff0000] +[0xfffffc00 0xfffff000]; calls -[] +[9c504] |
| `801f4dbc` WORLD02/AREA098.EMI#13 |  | `0004438c` | `0004415c` | callee-align | 47/43 | 3 | const -[0xfffe0000 6] +[768] |
| `801f5308` WORLD02/AREA098.EMI#13 |  | `000448f0` | `000446c0` | caller-vote | 42/40 | 3 | const -[] +[24]; field -[] +[w1+0 w1+2 w2+20]; calls -[] +[44990] unpaired 1/0 |
| `801f2f44` WORLD02/AREA102.EMI#13 |  | `0006b4a0` | `0006b1d4` | callee-align | 143/143 | 3 | const -[-128 15] +[0xffffc000 0x400000 56] |
| `801f36b4` WORLD02/AREA102.EMI#13 |  | `0006bbb8` | `0006b8ec` | similarity | 153/154 | 3 | const -[-128 15] +[0xffffc000 0x400000 56] |
| `801f2e5c` WORLD02/AREA103.EMI#13 |  | `0003043c` | `00030264` | similarity | 73/73 | 3 | const -[12] +[0x4bc]; field -[] +[w2+16 w2+20 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801f3bc4` WORLD02/AREA104.EMI#14 |  | `00244c6c` | `00244aa8` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f66c8` WORLD02/AREA104.EMI#14 |  | `00244bd8` | `00244a14` | gap-align | 39/35 | 3 | const -[17 176 81] +[]; calls -[] +[dce04] unpaired 1/0 |
| `801f35f4` WORLD02/AREA111.EMI#13 |  | `002479b8` | `002477f4` | gap-align | 90/81 | 3 | const -[] +[320]; calls -[] +[8fd64] |
| `801f39e0` WORLD02/AREA111.EMI#13 |  | `00247cf4` | `00247b30` | gap-align | 115/110 | 3 | const -[] +[320]; field -[w1+24] +[w4+24]; calls -[] +[8fd64] |
| `801f2ea8` WORLD02/AREA112.EMI#13 |  | `0006ce48` | `0006cb7c` | similarity-weak | 209/197 | 3 | const -[17 31] +[0x10402a 0x104030]; field -[] +[w1+1 w1+16384 w1+3 w2+0 w4+0] |
| `801f2dd8` WORLD03/AREA114.EMI#13 |  | `00249854` | `00249690` | gap-align | 19/17 | 3 | const -[9] +[5 62]; calls -[8e7e0 95db0 9716c] +[8e810] |
| `801f3b04` WORLD03/AREA115.EMI#13 |  | `0024b278` | `0024b0b4` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f2e08` WORLD03/AREA117.EMI#13 |  | `00064298` | `00063fcc` | callee-align | 52/68 | 3 | const -[0xffea0000 0xffeb0000 0xfffd8000 0x10000] +[] |
| `801f381c` WORLD03/AREA118.EMI#13 |  | `000661a8` | `00065edc` | similarity | 239/192 | 3 | const -[-127 -128] +[0xffffbfc0]; field -[] +[w2+17552] |
| `801f3e7c` WORLD03/AREA120.EMI#13 |  | `000847f8` | `00084530` | similarity | 50/63 | 3 | const -[0xffff00 0x30600] +[6]; field -[w4+0] +[w1+1 w4+5968] |
| `801f3f5c` WORLD03/AREA120.EMI#13 |  | `000848f4` | `0008462c` | similarity | 216/183 | 3 | const -[-128 192] +[0xffffc010]; field -[w4+0 w4+40] +[] |
| `801f3b18` WORLD03/AREA121.EMI#14 |  | `0024fbc8` | `0024fa04` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f2cac` WORLD03/AREA133.EMI#13 |  | `0026d328` | `0026d164` | similarity-weak | 11/23 (size 2.0x) | 3 | const -[30] +[]; globals 2 only PSX, 2 only PSP |
| `801f49ec` WORLD03/AREA135.EMI#14 |  | `00253048` | `00252e84` | gap-order | 57/57 | 3 | const -[0xfff38000] +[]; calls -[] +[8f78c] |
| `801f4c18` WORLD03/AREA135.EMI#14 |  | `00253208` | `00253044` | gap-order | 50/50 | 3 | const -[0xffe38000] +[]; calls -[] +[8f78c] |
| `801f4d0c` WORLD03/AREA135.EMI#14 |  | `002532d0` | `0025310c` | signature | 29/29 | 3 | const -[0x148000] +[]; calls -[] +[8f78c] |
| `801f4230` WORLD03/AREA141.EMI#13 |  | `00255f50` | `00255d8c` | gap-align | 42/46 | 3 | const -[] +[0xbfff]; calls -[] +[d7ef0] |
| `801f4508` WORLD03/AREA141.EMI#13 |  | `0025621c` | `00256058` | gap-align | 28/27 | 3 | const -[] +[0xffff9698]; calls -[] +[8ad1c] |
| `801f4764` WORLD03/AREA141.EMI#13 |  | `0025647c` | `002562b8` | gap-align | 25/24 | 3 | const -[] +[0xffff96ea]; calls -[] +[89cec] |
| `801f3b24` WORLD03/AREA144.EMI#13 |  | `00257594` | `002573d0` | gap-align | 31/28 | 3 | const -[0x1c0000] +[]; calls -[] +[8f78c] |
| `801f32d4` WORLD03/AREA149.EMI#13 |  | `0025a7f4` | `0025a630` | gap-align | 93/81 | 3 | const -[0x77ff 0xffec0000] +[0x1400]; field -[w4+0] +[w2-24368] |
| `801f3a70` WORLD03/AREA151.EMI#13 |  | `0025c5f4` | `0025c430` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f39c4` WORLD04/AREA152.EMI#13 |  | `0025e420` | `0025e25c` | callee-align | 226/224 | 3 | const -[96] +[160 161]; field -[] +[w2+12 w2+2]; calls -[] +[dce04] unpaired 1/0 |
| `801f4158` WORLD04/AREA168.EMI#13 |  | `0007532c` | `00075064` | similarity | 206/173 | 3 | const -[-128 12] +[0xffffc000] |
| `801f390c` WORLD04/AREA169.EMI#13 |  | `00260438` | `00260274` | similarity | 58/61 | 3 | const -[] +[0x1410]; field -[] +[w1+44]; calls -[] +[d7ef0] |
| `801f40f0` WORLD04/AREA170.EMI#13 |  | `000734d4` | `0007320c` | similarity | 242/221 | 3 | const -[-127 -128] +[0xffffbfc0]; field -[] +[w1+0] |
| `801f3070` WORLD04/AREA171.EMI#13 |  | `0007402c` | `00073d64` | similarity | 210/197 | 3 | const -[-127 -128] +[0xffffbfc0]; field -[] +[w1+0] |
| `801f4de0` WORLD04/AREA172.EMI#13 |  | `00262594` | `002623d0` | gap-align | 29/30 | 3 | const -[200 220] +[190 256] |
| `801f4e68` WORLD04/AREA172.EMI#13 |  | `0026260c` | `00262448` | gap-order | 8/8 | 3 | const -[200 220] +[190 256] |
| `801f45e0` WORLD04/AREA173.EMI#14 |  | `00081eac` | `00081be4` | similarity | 175/172 | 3 | const -[-127 -128] +[0xffffbfc0]; field -[] +[w1+0] |
| `801f3378` WORLD04/AREA189.EMI#13 |  | `0007c7a0` | `0007c4d8` | similarity | 47/40 | 3 | const -[483 549] +[768] |
| `801f43e8` WORLD04/AREA189.EMI#13 |  | `0007dd68` | `0007daa0` | similarity | 339/315 | 3 | const -[-512 6] +[0xffffec00] |
| `801f518c` WORLD04/AREA189.EMI#13 |  | `0026b3f4` | `0026b230` | similarity | 123/149 | 3 | const -[-16 5] +[26]; field -[] +[w1+44 w4+5968] |
| `801f5600` WORLD04/AREA189.EMI#13 |  | `0026b678` | `0026b4b4` | callee-align | 384/378 | 3 | const -[] +[0x650]; field -[w1+0] +[w1+1907 w2+15952 w4+5968]; calls -[] +[2ad43c 8c904 dc1d0] unpaired 3/1; globals 5 only PSX, 1 only PSP |
| `801f3ab0` WORLD04/AREA191.EMI#13 +1 copies |  | `0026c568` | `0026c3a4` | gap-order | 71/60 | 3 | const -[164 320] +[20] |
| `801f2c48` WORLD04/AREA197.EMI#13 |  | `00053198` | `00052f8c` | signature | 83/85 | 3 | const -[0x20000 0x50000 0xa0000 0xd0000] +[] |
| `801f400c` WORLD04/AREA199.EMI#13 |  | `00083b18` | `00083850` | similarity-weak | 252/218 | 3 | const -[0x78c0 427] +[768]; field -[w1+12 w1+13 w1+20 w1+21 w1+28 w1+29 w1+36 w1+37 ...] +[w4+32 w4+40] |
| `8014bb64` SLPS_009.90 |  | `00001540` | `00001eac` | callee-align | 472/436 | 3 | const -[12 0xffffff 55] +[57]; field -[] +[w4+32 w4+36 w4+4]; calls -[] +[] unpaired 1/0 |
| `8014fce8` SLPS_009.90 |  | `000095e0` | `00009e24` | callee-align | 34/22 | 3 | const -[12 143 768] +[] |
| `80153758` SLPS_009.90 |  | `0000cd98` | `0000cc40` | callee-align | 61/63 | 3 | const -[27 55] +[32 58] |
| `80155578` SLPS_009.90 |  | `0000f988` | `0000f82c` | caller-vote | 147/285 (size 1.9x) | 3 | const -[] +[0x10402a 0x104030]; field -[] +[w1-1712 w1+0 w1+16 w1+16384 w1+16385 w2+0 w4+0]; calls -[] +[] unpaired 2/0 |
| `80156938` SLPS_009.90 |  | `000104b8` | `0001035c` | gap-align | 91/115 | 3 | const -[248] +[0xffffa190 0x5b0 31] |
| `80157e14` SLPS_009.90 |  | `00012748` | `000125c4` | similarity | 177/205 | 3 | const -[-128 0xffc] +[0xffffc000]; field -[w1+2 w1+6 w1+7 w2+0 w2+2 w2+4] +[w2+80 w2+82 w2+84] |
| `801a0a30` SLPS_009.90 |  | `00084dcc` | `00084b04` | caller-vote | 0/30 (size 31.0x) | 3 | const -[] +[5]; field -[] +[w1+80]; calls -[] +[85158 851e8] |
| `801b020c` SLPS_009.90 |  | `0009a580` | `0009a2fc` | caller-vote | 0/25 (size 26.0x) | 3 | const -[] +[18 19 21]; field -[] +[w1+0] |
| `801c2ab4` SLPS_009.90 |  | `000d965c` | `000d9374` | caller-vote | 0/132 (size 133.0x) | 3 | const -[] +[120 40 80]; field -[] +[w1+38 w1+39 w1+5 w1+75]; calls -[] +[d8040 dce48] |
| `801eeef0` ETC/COMMU00.EMI#0 |  | `00109488` | `001091b0` | similarity | 118/100 | 2 | const -[5] +[10] |
| `801ef110` ETC/COMMU00.EMI#0 |  | `001076ec` | `0010740c` | similarity | 58/51 | 2 | const -[480] +[60] |
| `801f0524` ETC/COMMU00.EMI#0 |  | `00107df8` | `00107b18` | similarity | 111/126 | 2 | const -[152] +[]; globals 3 only PSX, 1 only PSP |
| `801f1bb8` ETC/COMMU00.EMI#0 |  | `0010a17c` | `00109eb0` | similarity | 159/127 | 2 | const -[17] +[0xfffff228]; field -[] +[w1+16] |
| `801d1778` ETC/COMMU01.EMI#7 |  | `0010adfc` | `0010ab30` | similarity | 176/181 | 2 | const -[0xfffc] +[]; field -[] +[w2+14]; globals 4 only PSX, 1 only PSP |
| `801d1a94` ETC/COMMU01.EMI#7 |  | `0010b0f4` | `0010ae28` | similarity | 214/220 | 2 | const -[0xfffc] +[]; globals 5 only PSX, 1 only PSP |
| `801d3a54` ETC/COMMU01.EMI#7 |  | `0010bca8` | `0010b9dc` | caller-vote | 17/18 | 2 | const -[480] +[60] |
| `801d4484` ETC/COMMU01.EMI#7 |  | `0010c1e4` | `0010bf18` | similarity | 23/25 | 2 | const -[-4] +[-36] |
| `801d15ec` ETC/COMMU02.EMI#8 |  | `0010e514` | `0010e238` | similarity | 104/96 | 2 | const -[-16 0x100000 0x200000] +[32]; globals 1 only PSX, 1 only PSP |
| `801d17bc` ETC/COMMU02.EMI#8 |  | `0010e6a4` | `0010e3c8` | similarity | 52/56 | 2 | const -[0x200000 0x300000] +[32 48] |
| `801d20c0` ETC/COMMU02.EMI#8 |  | `0010ef18` | `-` | similarity | 86/93 | 2 | const -[0x100000 0x200000 25] +[100 32]; field -[] +[w4+9832] |
| `801d2ec4` ETC/COMMU02.EMI#8 |  | `0010fd10` | `0010faa8` | caller-vote | 49/45 | 2 | const -[0x7887] +[624] |
| `801d5188` ETC/COMMU02.EMI#8 |  | `00115c9c` | `001159ac` | similarity | 330/259 | 2 | const -[] +[19]; field -[] +[w1-1 w1+0 w1+1 w1+2 w1+40 w1+46 w2+512]; globals 8 only PSX, 1 only PSP |
| `801d5ae4` ETC/COMMU02.EMI#8 |  | `00111b08` | `001118d8` | caller-vote | 132/127 | 2 | const -[] +[19]; field -[] +[w1+0 w2+454 w2+512]; globals 4 only PSX, 1 only PSP |
| `801d7e5c` ETC/COMMU02.EMI#8 |  | `00107bc0` | `001078e0` | caller-vote | 12/14 | 2 | const -[480] +[60] |
| `801d8124` ETC/COMMU02.EMI#8 |  | `001140a4` | `00113e2c` | similarity | 29/31 | 2 | field -[] +[w2+0]; calls -[] +[3ff74] unpaired 1/0 |
| `801d90d0` ETC/COMMU02.EMI#8 |  | `00114844` | `001145cc` | callee-align | 77/76 | 2 | const -[] +[0xfffff590 0x266c] |
| `801d94c8` ETC/COMMU02.EMI#8 |  | `0011674c` | `-` | callee-align | 201/198 | 2 | const -[] +[0xfffff9ca 48] |
| `801daae4` ETC/COMMU02.EMI#8 |  | `00115138` | `00114e50` | similarity | 103/105 | 2 | globals 4 only PSX, 2 only PSP |
| `801d1708` ETC/COMMU03.EMI#6 |  | `00117b24` | `00117700` | similarity | 95/107 | 2 | const -[12 0x7800 63] +[31] |
| `801d0d4c` ETC/COMMU04.EMI#6 |  | `001184e0` | `001180cc` | similarity | 265/258 | 2 | const -[0xfffe] +[]; field -[] +[w2+14]; globals 10 only PSX, 1 only PSP |
| `801d12c8` ETC/COMMU04.EMI#6 |  | `001188f8` | `-` | similarity | 144/126 | 2 | const -[0xfffe] +[]; globals 2 only PSX, 1 only PSP |
| `801d0f50` ETC/COMMU05.EMI#7 |  | `00118e14` | `00118a0c` | caller-vote | 60/50 | 2 | const -[0x7849] +[400] |
| `801d1498` ETC/COMMU05.EMI#7 |  | `0011937c` | `00118fd4` | callee-align | 99/104 | 2 | const -[] +[0x101d8]; field -[] +[w2+0 w2+2]; globals 5 only PSX, 1 only PSP |
| `801d1640` ETC/COMMU05.EMI#7 |  | `00119520` | `00119178` | similarity | 44/44 | 2 | const -[0x7883] +[560] |
| `801986ec` ETC/GAME.EMI#0 |  | `00008b9c` | `00009510` | similarity | 159/150 | 2 | const -[0x160000] +[]; field -[w1+0] +[w1+1 w1+8 w1+80]; calls -[dec] +[970fc] unpaired 2/0 |
| `80198c94` ETC/GAME.EMI#0 |  | `000074d0` | `00007e44` | gap-align | 97/186 (size 1.9x) | 2 | const -[0x560] +[43]; field -[] +[w1+2 w2+0]; calls -[] +[970fc] unpaired 1/0 |
| `80198f48` ETC/GAME.EMI#0 |  | `000078ac` | `00008220` | gap-align | 123/202 | 2 | const -[0x560] +[43]; field -[] +[w4+336]; calls -[952f0] +[] |
| `8019bb64` ETC/GAME.EMI#0 |  | `00020d14` | `00020b28` | gap-order | 17/20 | 2 | const -[] +[0x650]; calls -[] +[3154] unpaired 1/0; globals 1 only PSX, 1 only PSP |
| `8019bbb8` ETC/GAME.EMI#0 |  | `00020d68` | `00020b7c` | gap-order | 32/30 | 2 | const -[] +[0x650]; calls -[] +[] unpaired 1/0; globals 1 only PSX, 1 only PSP |
| `8019cbac` ETC/GAME.EMI#0 |  | `00025590` | `000253a4` | similarity | 131/148 | 2 | const -[] +[52 56]; field -[] +[w4+0] |
| `8019d4f8` ETC/GAME.EMI#0 |  | `00022898` | `000226ac` | gap-align | 74/113 | 2 | const -[0xffff0000] +[0x104002]; field -[] +[w1+16384 w2+0]; calls -[] +[dce04] unpaired 1/0 |
| `8019dfd4` ETC/GAME.EMI#0 |  | `000233b0` | `000231c4` | gap-align | 25/28 | 2 | const -[] +[0x650]; calls -[] +[3154] unpaired 1/0; globals 1 only PSX, 1 only PSP |
| `801a2c64` ETC/GAME.EMI#0 |  | `00088704` | `00088450` | callee-align | 157/263 | 2 | const -[5 7] +[0xffff0000]; calls -[] +[8670c] |
| `801a4088` ETC/GAME.EMI#0 |  | `00088b24` | `00088870` | callee-align | 208/149 | 2 | const -[] +[152 6] |
| `801a952c` ETC/GAME.EMI#0 |  | `0008cf68` | `0008ccc0` | gap-align | 93/94 | 2 | const -[0x160000] +[0x1410]; field -[] +[w1+44 w2-1456]; calls -[] +[] unpaired 1/0 |
| `801a9a98` ETC/GAME.EMI#0 |  | `0008d698` | `0008d3f0` | gap-align | 58/58 | 2 | const -[0x107fff] +[]; calls -[] +[dce04] unpaired 2/0 |
| `801aa944` ETC/GAME.EMI#0 |  | `00090d18` | `00090a8c` | similarity | 254/286 | 2 | const -[207] +[223]; field -[] +[w4+5968] |
| `801adac0` ETC/GAME.EMI#0 |  | `00090058` | `0008fdcc` | gap-align | 53/50 | 2 | const -[-256 0x80808000] +[]; field -[w4+92] +[] |
| `801adcd8` ETC/GAME.EMI#0 |  | `0009020c` | `0008ff80` | gap-align | 63/62 | 2 | const -[-256 0xc0c0c000] +[]; field -[w4+92] +[] |
| `801ae4dc` ETC/GAME.EMI#0 |  | `000933e8` | `0009315c` | gap-align | 136/107 | 2 | const -[254] +[152]; field -[w1+116 w4+16] +[w1+8] |
| `801b6444` ETC/GAME.EMI#0 |  | `000ca568` | `000ca27c` | gap-align | 80/74 | 2 | const -[320] +[]; globals 1 only PSX, 1 only PSP |
| `801b69ac` ETC/GAME.EMI#0 |  | `000ca9d4` | `000ca6e8` | gap-order | 73/69 | 2 | const -[0x2001] +[]; field -[] +[w4+5968]; globals 1 only PSX, 1 only PSP |
| `801b7410` ETC/GAME.EMI#0 |  | `000cb180` | `000cae94` | gap-align | 17/16 | 2 | const -[40] +[10] |
| `801b7468` ETC/GAME.EMI#0 |  | `000cb1c4` | `000caed8` | gap-align | 95/89 | 2 | const -[80] +[10]; field -[] +[w2+2 w2+4 w2+54 w2+6]; calls -[] +[] unpaired 1/0 |
| `801bd758` ETC/GAME.EMI#0 |  | `000d4de0` | `000d4af4` | similarity-weak | 341/384 | 2 | const -[0x20000 0x60000 0x18000] +[]; field -[w1+112] +[w1-1648 w1+9 w4+112 w4+12 w4+16 w4+17424 w4+5968] |
| `801beab0` ETC/GAME.EMI#0 |  | `000d58dc` | `000d55f0` | gap-align | 44/43 | 2 | const -[-320] +[]; globals 1 only PSX, 1 only PSP |
| `801bebbc` ETC/GAME.EMI#0 |  | `000d59d4` | `000d56e8` | gap-align | 66/62 | 2 | const -[320] +[]; globals 1 only PSX, 2 only PSP |
| `801c0440` ETC/GAME.EMI#0 |  | `000d65e4` | `000d62fc` | gap-align | 80/87 | 2 | const -[320] +[]; globals 1 only PSX, 2 only PSP |
| `801c08d8` ETC/GAME.EMI#0 |  | `000d6a78` | `000d6790` | gap-align | 111/98 | 2 | const -[320] +[]; globals 2 only PSX, 1 only PSP |
| `801c0b2c` ETC/GAME.EMI#0 |  | `000d6c88` | `000d69a0` | gap-align | 171/163 | 2 | const -[320] +[]; field -[] +[w1+31760]; calls -[] +[] unpaired 1/0; globals 1 only PSX, 1 only PSP |
| `801c0e2c` ETC/GAME.EMI#0 |  | `000d6f20` | `000d6c38` | gap-align | 84/94 | 2 | const -[320] +[]; field -[] +[w1+12]; globals 1 only PSX, 1 only PSP |
| `801c2e80` ETC/GAME.EMI#0 |  | `000d8300` | `000d8018` | gap-align | 37/35 | 2 | const -[480] +[60] |
| `801c2f44` ETC/GAME.EMI#0 |  | `000d8394` | `000d80ac` | gap-order | 41/34 | 2 | const -[480] +[60] |
| `801c3998` ETC/GAME.EMI#0 |  | `000d9f3c` | `000d9c54` | gap-order | 62/57 | 2 | const -[-256 0x80808000] +[]; field -[w4+92] +[] |
| `801c4084` ETC/GAME.EMI#0 |  | `000da4e0` | `000da1f8` | gap-order | 48/60 | 2 | const -[] +[0x4000 0x4010 528]; field -[] +[w2+16] |
| `801c7880` ETC/GAME.EMI#0 |  | `001429b4` | `00142780` | gap-order | 77/78 | 2 | globals 5 only PSX, 2 only PSP |
| `801c7ed8` ETC/GAME.EMI#0 |  | `0014302c` | `00142df8` | gap-order | 59/69 | 2 | const -[] +[0xe4048]; field -[] +[w1+130]; globals 6 only PSX, 1 only PSP |
| `801d104c` ETC/GAME.EMI#1 |  | `00005a64` | `000063d8` | similarity | 44/45 | 2 | const -[0x20000 0xffff00] +[]; globals 1 only PSX, 2 only PSP |
| `801d1a20` ETC/SHISU.EMI#0 |  | `00208000` | `00207d28` | similarity | 69/70 | 2 | globals 6 only PSX, 2 only PSP |
| `801d1b58` ETC/SHISU.EMI#0 |  | `0020811c` | `00207e44` | similarity-weak | 77/73 | 2 | globals 6 only PSX, 2 only PSP |
| `801d2978` ETC/SHISU.EMI#0 |  | `00208578` | `002082a0` | caller-vote | 87/73 | 2 | globals 4 only PSX, 2 only PSP |
| `801d2adc` ETC/SHISU.EMI#0 |  | `0020869c` | `002083c4` | caller-vote | 73/71 | 2 | globals 6 only PSX, 2 only PSP |
| `801d1154` ETC/SHOP.EMI#0 |  | `001eee58` | `001eeb58` | similarity | 55/62 | 2 | const -[] +[11 0x37fc] |
| `801d23ec` ETC/SHOP.EMI#0 |  | `001ef890` | `001ef580` | gap-align | 162/146 | 2 | const -[26] +[13]; field -[] +[w2+4]; calls -[] +[d5990] unpaired 1/1 |
| `801d3664` ETC/SHOP.EMI#0 |  | `001f0474` | `001f015c` | gap-order | 11/9 | 2 | const -[25] +[100] |
| `801d3698` ETC/SHOP.EMI#0 |  | `001f04a4` | `001f018c` | gap-order | 33/33 | 2 | const -[12] +[55] |
| `801d3cac` ETC/SHOP.EMI#0 |  | `001f14ac` | `001f1194` | similarity | 53/55 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d3d94` ETC/SHOP.EMI#0 |  | `001f158c` | `001f1274` | similarity | 135/138 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d3fe4` ETC/SHOP.EMI#0 |  | `001f17c8` | `001f14b0` | similarity | 42/46 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d409c` ETC/SHOP.EMI#0 |  | `001f1888` | `001f1570` | similarity | 67/63 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d41fc` ETC/SHOP.EMI#0 |  | `001f19c0` | `001f16a8` | similarity | 69/63 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d431c` ETC/SHOP.EMI#0 |  | `001f1ac0` | `001f17a8` | similarity | 35/38 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d447c` ETC/SHOP.EMI#0 |  | `001f1c10` | `001f18f8` | similarity | 60/58 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d457c` ETC/SHOP.EMI#0 |  | `001f1cfc` | `001f19e4` | similarity | 37/43 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d4624` ETC/SHOP.EMI#0 |  | `001f1db4` | `001f1a9c` | gap-align | 33/30 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d5da8` ETC/SHOP.EMI#0 |  | `001f2c18` | `001f2900` | gap-align | 53/54 | 2 | field -[w1+0] +[w1+1 w1+2 w1+4]; calls -[] +[3ff74] unpaired 1/0 |
| `801d6028` ETC/SHOP.EMI#0 |  | `001f2cf8` | `001f29e0` | similarity | 33/33 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d76c8` ETC/SHOP.EMI#0 |  | `001f423c` | `001f3f28` | similarity | 156/146 | 2 | const -[12 0xfffe] +[] |
| `801d95d0` ETC/SHOP.EMI#0 +3 copies |  | `000bb1cc` | `-` | similarity | 168/159 | 2 | calls -[] +[9a1b8] |
| `801da430` ETC/SHOP.EMI#0 |  | `001f5df4` | `-` | gap-order | 124/118 | 2 | const -[] +[0xffff80f1 0xffff80e5] |
| `801e2dbc` ETC/SHOP.EMI#0 |  | `001fec10` | `001fe984` | similarity | 171/185 | 2 | field -[] +[w4+1656]; calls -[] +[9a1b8] |
| `801e3b70` ETC/SHOP.EMI#0 |  | `001ff1ac` | `001fef20` | gap-align | 13/14 | 2 | const -[-170] +[-202] |
| `801e4130` ETC/SHOP.EMI#0 |  | `001ff474` | `001ff1e8` | gap-align | 13/14 | 2 | const -[-120] +[-152] |
| `801e4aec` ETC/SHOP.EMI#0 |  | `001ff77c` | `001ff4f0` | gap-align | 62/87 | 2 | const -[] +[0xffffa190 0xffff8afc 15]; field -[] +[w1+1665 w4+0] |
| `801dab08` ETC/START.EMI#8 |  | `000b75b0` | `000b73b8` | similarity | 122/124 | 2 | const -[320] +[245]; calls -[] +[d5990] |
| `801dad00` ETC/START.EMI#8 |  | `000b6c08` | `000b6a10` | similarity | 131/66 (size 0.5x) | 2 | const -[164] +[]; calls -[] +[d5990]; globals 8 only PSX, 1 only PSP |
| `801e15d4` ETC/START.EMI#8 |  | `000c0160` | `000bffe0` | similarity | 35/37 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801e6308` ETC/START.EMI#8 |  | `000c3104` | `-` | similarity | 67/71 | 2 | const -[] +[0xffffccc4 0xffffcb80] |
| `801e6418` ETC/START.EMI#8 |  | `000c3fd0` | `-` | callee-align | 200/196 | 2 | const -[] +[0xffffcdd0 48] |
| `801e6be8` ETC/START.EMI#8 |  | `001f33a0` | `001f3088` | similarity | 70/82 | 2 | const -[] +[28 56]; field -[] +[w1+21] |
| `801e6d98` ETC/START.EMI#8 |  | `000c3298` | `000c3090` | similarity | 121/122 | 2 | const -[128 144] +[0xffffcd71]; field -[w1+24] +[w1+32 w1+33 w1+34 w1+35] |
| `801e6f80` ETC/START.EMI#8 |  | `000c3480` | `000c3278` | similarity | 72/73 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801e8008` ETC/START.EMI#8 |  | `000c4eb8` | `000c4bdc` | similarity | 68/55 | 2 | const -[25] +[100] |
| `801e94d4` ETC/START.EMI#8 |  | `000c63e0` | `-` | similarity | 58/61 | 2 | calls -[] +[3ff74] unpaired 1/0 |
| `801d1370` SCENARIO/SCE15EF0.EMI#0 |  | `00057bfc` | `000579fc` | callee-align | 115/119 | 2 | calls -[] +[9c504 9c5e4] |
| `801d106c` SCENARIO/SCE15EF1.EMI#0 |  | `00059dac` | `00059ac0` | similarity | 125/124 | 2 | const -[0x2f7fff] +[6]; field -[w1+6] +[w1+0] |
| `801d147c` SCENARIO/SCE15EF1.EMI#0 +1 copies |  | `00043f74` | `00043d44` | callee-align | 122/130 | 2 | field -[w1+12 w1+13 w1+14 w1+20 w1+21 w1+22 w1+4 w1+5 ...] +[]; calls -[] +[9c504 9c5e4] |
| `801d21b4` SCENARIO/SCE15EF1.EMI#0 |  | `0005add8` | `0005aaf8` | similarity | 109/120 | 2 | const -[112] +[14]; field -[w4+12 w4+8] +[w1+2574 w4+5968]; calls -[3708] +[3154] unpaired 0/1 |
| `801d2e30` SCENARIO/SCE15EF1.EMI#0 |  | `0005b960` | `0005b680` | similarity | 98/108 | 2 | const -[112] +[14]; field -[w4+12 w4+8] +[w1+2574 w4+5968]; calls -[3708] +[3154] unpaired 0/1 |
| `801d1218` SCENARIO/SCE15EF2.EMI#0 |  | `0005c910` | `0005c638` | gap-align | 122/130 | 2 | field -[w1+12 w1+13 w1+14 w1+20 w1+21 w1+22 w1+4 w1+5 ...] +[]; calls -[] +[9c504 9c5e4] |
| `801d1624` SCENARIO/SCE15EF2.EMI#0 |  | `0005cba0` | `0005c8c8` | gap-align | 83/85 | 2 | const -[] +[6]; field -[] +[w1+0 w1+1 w1+2 w1+3 w2+4 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801d10ac` SCENARIO/SCE15EF3.EMI#0 |  | `0005d690` | `0005d3b8` | similarity | 69/72 | 2 | const -[0x188000 0xff000000] +[]; calls -[] +[9c504] |
| `801d1a60` SCENARIO/SCE15EF3.EMI#0 |  | `0005dfd8` | `0005dcfc` | similarity | 77/81 | 2 | const -[0xffff0000] +[20] |
| `801d1ff8` SCENARIO/SCE15EF3.EMI#0 |  | `00028f38` | `00028d54` | callee-align | 183/183 | 2 | calls -[] +[9c504 9c5e4] |
| `801f6e48` SCENARIO/SCENA00.EMI#0 |  | `00017188` | `0001700c` | similarity | 168/165 | 2 | field -[w1+26] +[w4+28]; calls -[] +[8fc28] |
| `801f7134` SCENARIO/SCENA00.EMI#0 |  | `00015bcc` | `00015a50` | callee-align | 69/58 | 2 | const -[0x7a80] +[0xa00] |
| `801f7484` SCENARIO/SCENA00.EMI#0 |  | `00015e70` | `00015cf4` | gap-align | 25/27 | 2 | const -[0xf8000 0xb0000] +[] |
| `801f8360` SCENARIO/SCENA00.EMI#0 |  | `0001763c` | `000174c0` | callee-align | 141/157 | 2 | const -[45] +[256 360] |
| `801fb924` SCENARIO/SCENA00.EMI#0 |  | `002711e4` | `00271020` | similarity | 303/271 | 2 | const -[0x3000] +[0xffffe000]; field -[w4+0] +[w1+8 w4+56]; calls -[] +[8c84c] |
| `801f7dcc` SCENARIO/SCENA01.EMI#0 |  | `00060484` | `000601b8` | callee-align | 66/55 | 2 | const -[319] +[-32 352 6] |
| `801fbf94` SCENARIO/SCENA02.EMI#0 |  | `002781f8` | `00278034` | similarity | 39/37 | 2 | const -[0x50000 0x18000] +[] |
| `801fc308` SCENARIO/SCENA02.EMI#0 |  | `00278524` | `00278360` | callee-align | 39/36 | 2 | const -[0xfffb0000 0xffff0000 0x10000] +[] |
| `801fc518` SCENARIO/SCENA02.EMI#0 |  | `00278700` | `0027853c` | similarity | 41/38 | 2 | const -[0x28000 0x40000 0xfffe0000] +[] |
| `801fe1c0` SCENARIO/SCENA02.EMI#0 |  | `0027dc1c` | `0027da58` | similarity-weak | 38/33 | 2 | field -[] +[w1+151]; calls -[] +[d7e34] |
| `801fc704` SCENARIO/SCENA03.EMI#0 |  | `002809d4` | `00280810` | similarity | 47/58 | 2 | const -[42 52] +[] |
| `801fcb4c` SCENARIO/SCENA03.EMI#0 |  | `00280e70` | `00280cac` | similarity | 80/85 | 2 | const -[0xa8000 0xb8000] +[]; field -[] +[w1+4 w1+5 w4-26160] |
| `801fcefc` SCENARIO/SCENA03.EMI#0 +1 copies |  | `00281248` | `00281084` | gap-align | 8/8 | 2 | calls -[] +[d7e34] |
| `801f7cf0` SCENARIO/SCENA04.EMI#0 |  | `0002d594` | `-` | gap-align | 111/124 | 2 | calls -[] +[9c504 9c5e4] |
| `801f7554` SCENARIO/SCENA05.EMI#0 |  | `0002a540` | `0002a364` | similarity | 48/56 | 2 | const -[] +[52 56]; field -[] +[w4+0] |
| `801fd178` SCENARIO/SCENA05.EMI#0 |  | `002881f4` | `00288030` | similarity | 34/44 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801fd208` SCENARIO/SCENA05.EMI#0 |  | `002882ac` | `002880e8` | similarity | 34/44 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801fd4d8` SCENARIO/SCENA05.EMI#0 |  | `00288b60` | `0028899c` | gap-order | 10/10 | 2 | calls -[] +[d7e34] |
| `801fd580` SCENARIO/SCENA05.EMI#0 |  | `00288bf8` | `00288a34` | gap-align | 8/8 | 2 | calls -[] +[d7e34] |
| `801fd800` SCENARIO/SCENA06.EMI#0 |  | `0028e880` | `0028e6bc` | similarity-weak | 294/415 | 2 | const -[0xb8000 0x18000] +[]; field -[] +[w2-240 w4-26160] |
| `801fe254` SCENARIO/SCENA06.EMI#0 |  | `0028f17c` | `0028efb8` | gap-order | 10/10 | 2 | calls -[] +[d7e34] |
| `801fe284` SCENARIO/SCENA06.EMI#0 |  | `0028f1a8` | `0028efe4` | gap-order | 10/10 | 2 | calls -[] +[d7e34] |
| `801fe2b4` SCENARIO/SCENA06.EMI#0 |  | `0028f1d4` | `0028f010` | gap-order | 10/10 | 2 | calls -[] +[d7e34] |
| `801fe334` SCENARIO/SCENA06.EMI#0 |  | `0028f248` | `0028f084` | gap-order | 10/10 | 2 | calls -[] +[d7e34] |
| `801fe38c` SCENARIO/SCENA06.EMI#0 |  | `0028f298` | `0028f0d4` | similarity | 10/10 | 2 | calls -[] +[d7e34] |
| `801f7ddc` SCENARIO/SCENA07.EMI#0 |  | `00037458` | `00037268` | gap-align | 13/13 | 2 | calls -[3a08c] +[3154] |
| `801f8358` SCENARIO/SCENA07.EMI#0 |  | `000379d0` | `000377e0` | gap-order | 14/17 | 2 | const -[] +[28]; field -[] +[w1+0]; globals 2 only PSX, 1 only PSP |
| `801f85d8` SCENARIO/SCENA07.EMI#0 |  | `00037c68` | `00037a78` | gap-order | 53/57 | 2 | const -[] +[28]; field -[] +[w1+0 w1+1 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801f9330` SCENARIO/SCENA07.EMI#0 |  | `00038c94` | `-` | callee-align | 277/245 | 2 | const -[0x410 0x430] +[]; field -[w1+0 w2+1296 w4+0 w4+1300 w4+1304 w4+1308 w4+3 w4+4 ...] +[w1+1072 w2+0 w2+144 w2+146 w2+2]; calls -[] +[9c504] |
| `801f9efc` SCENARIO/SCENA07.EMI#0 |  | `00038570` | `00038360` | caller-vote | 69/69 | 2 | const -[0x100000] +[32 0xe515c]; field -[] +[w1+0 w1+2 w4+12 w4+20 w4+2000 w4+24]; globals 1 only PSX, 1 only PSP |
| `801fd524` SCENARIO/SCENA07.EMI#0 |  | `0028fb80` | `0028f9bc` | gap-order | 29/31 | 2 | const -[] +[0x5e0]; globals 1 only PSX, 1 only PSP |
| `801fd5a0` SCENARIO/SCENA07.EMI#0 |  | `0028fc00` | `0028fa3c` | gap-order | 26/32 | 2 | const -[] +[0x5e0]; globals 1 only PSX, 1 only PSP |
| `801f77b4` SCENARIO/SCENA08.EMI#0 |  | `0003c064` | `0003be48` | gap-order | 90/94 | 2 | field -[w4+16] +[w4+0]; calls -[] +[9c504 9c5e4] |
| `801f7e14` SCENARIO/SCENA08.EMI#0 |  | `0003cfc0` | `0003cda4` | caller-vote | 124/137 | 2 | const -[] +[40]; field -[] +[w1+0 w1+1 w1+2 w1+3 w4+0 w4+12 w4+20 w4+2000 ...]; globals 1 only PSX, 1 only PSP |
| `801f8964` SCENARIO/SCENA08.EMI#0 +1 copies |  | `0003784c` | `0003765c` | callee-align | 95/97 | 2 | calls -[] +[9c504 9c5e4] |
| `801f8bf8` SCENARIO/SCENA08.EMI#0 |  | `0003cc64` | `0003ca48` | callee-align | 53/57 | 2 | const -[] +[28]; field -[] +[w1+0 w1+1 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801f9ce0` SCENARIO/SCENA08.EMI#0 |  | `00042294` | `0004206c` | signature | 19/23 | 2 | const -[0x190000 0xf0000] +[] |
| `801f9e50` SCENARIO/SCENA08.EMI#0 |  | `00042428` | `00042200` | gap-align | 26/29 | 2 | const -[0xfffe0000 0x98000] +[] |
| `801f9ec4` SCENARIO/SCENA08.EMI#0 |  | `000424a0` | `00042278` | gap-align | 23/25 | 2 | const -[0xfffe0000 0x98000] +[] |
| `801f9f8c` SCENARIO/SCENA08.EMI#0 |  | `00042554` | `-` | callee-align | 129/132 | 2 | calls -[] +[9c504 9c5e4] |
| `801f76d0` SCENARIO/SCENA09.EMI#0 |  | `000483b0` | `00048194` | similarity-weak | 122/134 | 2 | const -[] +[0x328000 40]; field -[] +[w1+0 w1+1 w1+2 w1+3 w1+4 w4+12 w4+16 w4+24 ...] |
| `801f9354` SCENARIO/SCENA09.EMI#0 |  | `00295dd4` | `00295c10` | gap-align | 70/70 | 2 | const -[] +[0xffff9950 0x1410]; field -[] +[w1-26288 w1+1 w1+2 w1+3 w1+44] |
| `801fbd6c` SCENARIO/SCENA09.EMI#0 |  | `00299b38` | `00299974` | similarity | 489/340 | 2 | const -[0x1f0000 0x1f8000 0x8000] +[]; field -[] +[w1-26288 w4+336 w4+52]; calls -[] +[8c84c] |
| `801fe12c` SCENARIO/SCENA09.EMI#0 +4 copies |  | `00276f94` | `00276dd0` | gap-align | 8/8 | 2 | calls -[] +[d7e34] |
| `801fe154` SCENARIO/SCENA09.EMI#0 |  | `0029bf9c` | `0029bdd8` | gap-align | 10/10 | 2 | calls -[] +[d7e34] |
| `801fe184` SCENARIO/SCENA09.EMI#0 +3 copies |  | `0028f200` | `0028f03c` | gap-order | 8/8 | 2 | calls -[] +[d7e34] |
| `801fe1ac` SCENARIO/SCENA09.EMI#0 |  | `0029bfec` | `0029be28` | gap-align | 10/10 | 2 | calls -[] +[d7e34] |
| `801fe1dc` SCENARIO/SCENA09.EMI#0 |  | `0029c018` | `0029be54` | gap-align | 10/10 | 2 | calls -[] +[d7e34] |
| `801fe20c` SCENARIO/SCENA09.EMI#0 +2 copies |  | `0029c044` | `0029be80` | gap-align | 8/8 | 2 | calls -[] +[d7e34] |
| `801fe234` SCENARIO/SCENA09.EMI#0 +1 copies |  | `0029c068` | `0029bea4` | gap-align | 49/45 | 2 | field -[w1+0] +[w1+16]; calls -[] +[d7e34] |
| `801fe30c` SCENARIO/SCENA09.EMI#0 +5 copies |  | `00288bb0` | `002889ec` | similarity | 8/8 | 2 | calls -[] +[d7e34] |
| `801fe334` SCENARIO/SCENA09.EMI#0 +4 copies |  | `00288bd4` | `00288a10` | similarity | 8/8 | 2 | calls -[] +[d7e34] |
| `801fe35c` SCENARIO/SCENA09.EMI#0 |  | `0029c16c` | `0029bfa8` | gap-align | 8/8 | 2 | calls -[] +[d7e34] |
| `801f719c` SCENARIO/SCENA10.EMI#0 |  | `00041098` | `00040e68` | similarity | 132/130 | 2 | const -[] +[0xda8 0xdac]; field -[] +[w2+1808] |
| `801f7ad4` SCENARIO/SCENA10.EMI#0 |  | `00041994` | `00041764` | similarity | 87/95 | 2 | const -[98] +[90] |
| `801f7fa0` SCENARIO/SCENA10.EMI#0 |  | `0029ccc0` | `0029cafc` | similarity | 323/296 | 2 | const -[] +[0x650]; calls -[] +[107174 d8270]; globals 2 only PSX, 1 only PSP |
| `801fc280` SCENARIO/SCENA10.EMI#0 |  | `0029ff00` | `0029fd3c` | similarity | 321/293 | 2 | const -[] +[0xc77]; field -[] +[w1+400]; calls -[] +[107174 8c84c d8270 d82b8]; globals 3 only PSX, 1 only PSP |
| `801fe00c` SCENARIO/SCENA10.EMI#0 |  | `002a0b14` | `002a0950` | similarity | 153/181 | 2 | const -[0x29ffff] +[0x290000]; field -[] +[w1+17744 w2-240] |
| `801fe360` SCENARIO/SCENA10.EMI#0 |  | `002a144c` | `002a1288` | gap-align | 37/41 | 2 | field -[] +[w1+121]; calls -[] +[d8084] |
| `801fe63c` SCENARIO/SCENA10.EMI#0 |  | `002a16e4` | `002a1520` | gap-align | 41/37 | 2 | field -[w1+0] +[w1+16]; calls -[] +[d7e34] |
| `801f7178` SCENARIO/SCENA11.EMI#0 |  | `0005ede4` | `0005eb08` | similarity | 28/31 | 2 | const -[244] +[240] |
| `801f7200` SCENARIO/SCENA11.EMI#0 |  | `0005ee60` | `0005eb84` | similarity | 28/31 | 2 | const -[236] +[240] |
| `801f88b0` SCENARIO/SCENA12.EMI#0 |  | `002a4c78` | `002a4ab4` | gap-align | 34/40 | 2 | const -[] +[0xffff9950 12]; field -[w1+0] +[w1+1] |
| `801f7dbc` SCENARIO/SCENA13.EMI#0 |  | `002aa204` | `002aa040` | similarity | 388/330 | 2 | field -[] +[w2-944]; calls -[] +[256e4c d8270] unpaired 1/0 |
| `801fb84c` SCENARIO/SCENA14.EMI#0 |  | `002b1160` | `002b0f9c` | similarity | 281/324 | 2 | const -[0x1d7fff 0xb7fff] +[]; field -[] +[w1+17744 w2-240] |
| `801fa6bc` SCENARIO/SCENA15.EMI#0 |  | `002b1d40` | `002b1b7c` | similarity | 95/70 | 2 | field -[] +[w1-26288 w4+336]; calls -[] +[d6f20] |
| `801f77fc` SCENARIO/SCENA17.EMI#0 |  | `002b7200` | `002b7050` | similarity | 52/54 | 2 | calls -[] +[85824] |
| `801f8204` SCENARIO/SCENA17.EMI#0 |  | `002b7b58` | `002b79a8` | gap-order | 12/19 | 2 | const -[] +[12 216 384]; calls -[] +[] unpaired 0/1 |
| `801f8914` SCENARIO/SCENA17.EMI#0 |  | `002b8214` | `002b8064` | similarity | 138/107 | 2 | const -[12 320] +[-32 384]; field -[w1+3] +[] |
| `801f9104` SCENARIO/SCENA17.EMI#0 |  | `002b84cc` | `002b831c` | similarity | 40/40 | 2 | const -[0x7b80 63] +[0xe00] |
| `801f3c2c` WORLD00/AREA008.EMI#13 |  | `0003fe24` | `-` | callee-align | 56/50 | 2 | const -[15] +[30 0xd98] |
| `801f3d10` WORLD00/AREA008.EMI#13 |  | `0003feec` | `-` | callee-align | 35/34 | 2 | const -[] +[0xcfc 0xda4] |
| `801f2cdc` WORLD00/AREA010.EMI#13 |  | `00222294` | `002220ac` | similarity | 27/32 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w4+0] |
| `801f2cc4` WORLD00/AREA011.EMI#13 |  | `002223b8` | `002221d0` | similarity | 40/47 | 2 | const -[0x3fc] +[0x104022]; field -[] +[w2+0 w4+0 w4+4] |
| `801f2d88` WORLD00/AREA013.EMI#13 |  | `00222618` | `00222430` | similarity | 28/28 | 2 | const -[0x7fff] +[]; calls -[] +[8f78c] |
| `801f39d8` WORLD00/AREA016.EMI#13 |  | `0022348c` | `002232a4` | similarity | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f42f8` WORLD00/AREA016.EMI#13 |  | `00223998` | `002237b0` | similarity | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f2df8` WORLD00/AREA024.EMI#14 |  | `00027a70` | `-` | signature | 116/113 | 2 | calls -[] +[9c504 9c5e4] |
| `801f3370` WORLD00/AREA026.EMI#13 |  | `00225570` | `00225388` | similarity | 34/33 | 2 | const -[0x68000 0x70000] +[] |
| `801f2d5c` WORLD00/AREA027.EMI#13 |  | `0002eb80` | `0002e998` | similarity-weak | 51/49 | 2 | const -[] +[152]; field -[] +[w2+20 w4+16 w4+8]; calls -[] +[3154]; globals 1 only PSX, 1 only PSP |
| `801f3164` WORLD00/AREA027.EMI#13 |  | `00060658` | `0006038c` | similarity | 22/23 | 2 | const -[] +[0x5c0]; globals 1 only PSX, 1 only PSP |
| `801f2d3c` WORLD00/AREA028.EMI#13 |  | `000322c8` | `000320f0` | callee-align | 125/110 | 2 | const -[-128 511] +[0xffffa000]; field -[] +[w2+0 w2+2] |
| `801f3060` WORLD00/AREA028.EMI#13 |  | `000321b8` | `00031fe0` | similarity | 63/64 | 2 | const -[12] +[]; field -[] +[w4+2000]; globals 1 only PSX, 1 only PSP |
| `801d1818` WORLD00/AREA030.EMI#4 |  | `00018700` | `00018584` | gap-order | 44/45 | 2 | const -[0x7a40] +[0x900] |
| `801d18cc` WORLD00/AREA030.EMI#4 |  | `000187b4` | `00018638` | gap-order | 35/35 | 2 | const -[0x7a40] +[0x900] |
| `801d1cf0` WORLD00/AREA030.EMI#4 |  | `0001e220` | `0001dfe8` | similarity | 130/161 | 2 | const -[] +[100 99]; calls -[] +[] unpaired 0/1 |
| `801d2adc` WORLD00/AREA030.EMI#4 |  | `000196c4` | `00019558` | callee-align | 54/51 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w4+0]; calls -[] +[3154] |
| `801d4200` WORLD00/AREA030.EMI#4 |  | `0001a920` | `0001a7a4` | gap-align | 40/44 | 2 | const -[254] +[-7]; calls -[] +[3154] unpaired 0/1 |
| `801d6144` WORLD00/AREA030.EMI#4 |  | `0001cafc` | `0001c980` | gap-order | 157/141 | 2 | const -[] +[229 6]; field -[] +[w1+0 w1+1] |
| `801d6f7c` WORLD00/AREA030.EMI#4 |  | `0001ea64` | `0001e82c` | similarity | 198/188 | 2 | const -[] +[0xd78 6]; field -[w1+6] +[w1+0 w1+1] |
| `801d732c` WORLD00/AREA030.EMI#4 |  | `0001ed64` | `0001eb2c` | callee-align | 261/276 | 2 | const -[-25] +[0xd80]; field -[w1+20] +[w1+122 w2+0] |
| `801d79e4` WORLD00/AREA030.EMI#4 |  | `0001d8b4` | `0001d718` | callee-align | 106/80 | 2 | const -[0x7887 32] +[624] |
| `801d7bac` WORLD00/AREA030.EMI#4 |  | `0001d9f4` | `0001d858` | callee-align | 42/45 | 2 | const -[] +[0xd78 0xd7c] |
| `801d7e34` WORLD00/AREA030.EMI#4 |  | `0001dd58` | `0001db20` | gap-order | 81/75 | 2 | const -[252 254] +[] |
| `801d8410` WORLD00/AREA030.EMI#4 |  | `0001f4e8` | `-` | callee-align | 310/305 | 2 | const -[254] +[837]; field -[w1+56] +[w1+2292 w1+2804 w1+4] |
| `801d9c24` WORLD00/AREA030.EMI#4 |  | `000de2dc` | `000ddff4` | similarity | 38/62 | 2 | field -[] +[w4+5968]; globals 2 only PSX, 2 only PSP |
| `801da1e8` WORLD00/AREA030.EMI#4 |  | `000df4a8` | `000df1b8` | similarity | 68/89 | 2 | const -[0xfffe] +[0x10080]; field -[] +[w1+14 w2+0]; calls -[] +[] unpaired 0/1 |
| `801daf98` WORLD00/AREA030.EMI#4 |  | `000dfc50` | `000df960` | gap-align | 40/53 | 2 | const -[] +[62]; calls -[] +[] unpaired 0/1; globals 1 only PSX, 2 only PSP |
| `801dbc2c` WORLD00/AREA030.EMI#4 |  | `000e0440` | `000e0150` | gap-align | 101/151 | 2 | field -[] +[w1+12]; calls -[] +[] unpaired 0/3 |
| `801de218` WORLD00/AREA030.EMI#4 |  | `000e5f3c` | `000e5c74` | similarity | 171/172 | 2 | const -[5] +[10]; field -[] +[w1+0] |
| `801e0b14` WORLD00/AREA030.EMI#4 |  | `000e32cc` | `000e2fdc` | caller-vote | 40/26 | 2 | const -[5] +[10] |
| `801e0cd8` WORLD00/AREA030.EMI#4 |  | `000e345c` | `000e316c` | caller-vote | 81/41 (size 0.5x) | 2 | const -[12 768] +[] |
| `801e0fa4` WORLD00/AREA030.EMI#4 |  | `000e364c` | `000e335c` | callee-align | 45/52 | 2 | const -[491] +[0xb00] |
| `801e1a24` WORLD00/AREA030.EMI#4 |  | `000e3fa4` | `000e3cb4` | callee-align | 144/151 | 2 | const -[] +[0x10078 0x10082]; field -[] +[w2+0] |
| `800f5be8` WORLD00/AREA030.EMI#5 |  | `000e7e70` | `-` | gap-align | 101/103 | 2 | const -[] +[0x1008a]; field -[] +[w2+0]; calls -[] +[1e18c]; globals 1 only PSX, 1 only PSP |
| `800f6c74` WORLD00/AREA030.EMI#5 |  | `000e8e50` | `000e8b70` | gap-order | 153/152 | 2 | const -[] +[0x10096 0x10098]; field -[] +[w1+1 w2+0] |
| `801f2d18` WORLD00/AREA032.EMI#13 |  | `0003b064` | `0003ae48` | similarity | 30/34 | 2 | calls -[] +[29f5c] unpaired 1/0 |
| `801f2dac` WORLD00/AREA032.EMI#13 |  | `0003b0ec` | `0003aed0` | signature | 38/41 | 2 | calls -[] +[29f5c] unpaired 1/0 |
| `801f3920` WORLD00/AREA032.EMI#13 |  | `00225efc` | `00225d14` | gap-order | 71/73 | 2 | const -[-256 0xc0c0c000] +[]; field -[w4+92] +[w4-26096]; calls -[] +[3d84] |
| `801f390c` WORLD00/AREA033.EMI#13 |  | `00227078` | `00226e90` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f422c` WORLD00/AREA033.EMI#13 |  | `00227584` | `0022739c` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f31f0` WORLD01/AREA041.EMI#13 |  | `0006213c` | `00061e70` | similarity | 35/34 | 2 | const -[] +[0x5c8]; globals 1 only PSX, 1 only PSP |
| `801f34ac` WORLD01/AREA041.EMI#13 |  | `00062750` | `00062484` | similarity | 136/161 | 2 | const -[0xba009124] +[0x45ff6edc 7]; field -[] +[w1+0 w1+1] |
| `801f2e78` WORLD01/AREA042.EMI#13 |  | `0002ace0` | `0002ab04` | callee-align | 138/139 | 2 | calls -[] +[9c504 9c5e4] |
| `801f3988` WORLD01/AREA042.EMI#13 |  | `00063d0c` | `00063a40` | similarity | 30/32 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f3a0c` WORLD01/AREA042.EMI#13 |  | `00063d90` | `00063ac4` | similarity | 25/27 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f3b24` WORLD01/AREA042.EMI#13 |  | `00063e84` | `00063bb8` | similarity | 41/40 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f3e80` WORLD01/AREA042.EMI#13 |  | `0022afe8` | `0022ae00` | gap-align | 13/14 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f2fbc` WORLD01/AREA043.EMI#13 |  | `0022c2f8` | `0022c120` | signature | 43/45 | 2 | calls -[] +[3c5c] |
| `801f39d8` WORLD01/AREA045.EMI#13 |  | `0022dad0` | `0022d8f8` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f42f8` WORLD01/AREA045.EMI#13 |  | `0022dfdc` | `0022de04` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f2db4` WORLD01/AREA047.EMI#13 |  | `00030d68` | `00030b90` | callee-align | 105/106 | 2 | calls -[] +[9c504 9c5e4] |
| `801f2f60` WORLD01/AREA047.EMI#13 |  | `00031170` | `00030f98` | similarity | 149/156 | 2 | calls -[] +[9c504 9c5e4] |
| `801f2c48` WORLD01/AREA048.EMI#13 |  | `0002bd4c` | `0002bb70` | similarity-weak | 22/26 | 2 | const -[] +[0x650]; field -[] +[w4+5968]; globals 1 only PSX, 1 only PSP |
| `801f2d64` WORLD01/AREA048.EMI#13 |  | `0002be94` | `0002bcb8` | similarity | 75/90 | 2 | const -[0x158000 0x1000000 0xd8000] +[256] |
| `801f302c` WORLD01/AREA048.EMI#13 |  | `0002c18c` | `0002bfb0` | similarity | 138/139 | 2 | calls -[] +[9c504 9c5e4] |
| `801f4804` WORLD01/AREA048.EMI#13 |  | `0022f2f8` | `0022f120` | gap-align | 21/25 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f4c28` WORLD01/AREA048.EMI#13 |  | `0022f6f0` | `0022f518` | gap-align | 61/65 | 2 | const -[0x58000 0xa8000] +[]; field -[] +[w1+21] |
| `801170d8` WORLD01/AREA049.EMI#13 |  | `00047e98` | `-` | signature | 124/117 | 2 | calls -[] +[9c504 9c5e4] |
| `80117410` WORLD01/AREA049.EMI#13 |  | `00047918` | `00047700` | similarity | 57/59 | 2 | const -[12] +[]; field -[] +[w4+2000]; globals 1 only PSX, 1 only PSP |
| `80117900` WORLD01/AREA049.EMI#13 |  | `00047d8c` | `00047b74` | similarity | 68/65 | 2 | const -[12] +[]; field -[] +[w4+2000]; globals 1 only PSX, 1 only PSP |
| `80117c58` WORLD01/AREA049.EMI#13 |  | `0006c050` | `0006bd84` | similarity | 92/81 | 2 | const -[] +[0x5f8]; globals 1 only PSX, 1 only PSP |
| `801f2d00` WORLD01/AREA051.EMI#13 |  | `002307b8` | `002305e0` | gap-align | 41/48 | 2 | const -[0x3fc] +[0x104022]; field -[] +[w2+0 w4+0 w4+4] |
| `801f2ec8` WORLD01/AREA052.EMI#13 |  | `00036154` | `00035f64` | similarity | 107/110 | 2 | calls -[] +[9c504 9c5e4] |
| `801f3a14` WORLD01/AREA052.EMI#13 |  | `00230de4` | `00230c0c` | gap-align | 98/99 | 2 | field -[w1+24 w1+28] +[]; calls -[] +[8f78c dce04] unpaired 1/0 |
| `801f3e14` WORLD01/AREA052.EMI#13 |  | `002311dc` | `00231004` | gap-align | 65/59 | 2 | const -[5] +[0x650]; field -[] +[w1+3] |
| `801f4268` WORLD01/AREA052.EMI#13 |  | `002312d0` | `002310f8` | gap-align | 9/10 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f43b0` WORLD01/AREA052.EMI#13 |  | `00231414` | `0023123c` | gap-align | 62/62 | 2 | calls -[] +[8f78c dce04] unpaired 1/0 |
| `801f2dd4` WORLD01/AREA064.EMI#13 |  | `00233370` | `00233198` | gap-order | 5/6 | 2 | field -[w1+11564 w1+11596 w1+11704 w1+11708] +[]; calls -[] +[23326c] |
| `801f3a10` WORLD01/AREA065.EMI#13 |  | `00233e68` | `00233c90` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f4330` WORLD01/AREA065.EMI#13 |  | `00234374` | `0023419c` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f2c48` WORLD01/AREA067.EMI#14 |  | `0002d940` | `0002d758` | similarity | 82/82 | 2 | const -[0x1c0000] +[12]; field -[w2+12 w2+14 w2+16 w2+18] +[w2+0 w2+2 w2+4 w2+6 w4+392]; calls -[] +[9c504] |
| `801f3f00` WORLD01/AREA067.EMI#14 |  | `00235440` | `00235268` | gap-align | 27/32 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w4+0] |
| `801f3960` WORLD01/AREA068.EMI#13 +6 copies |  | `00228c08` | `00228a20` | gap-align | 10/13 | 2 | const -[] +[116]; globals 1 only PSX, 1 only PSP |
| `801f4874` WORLD01/AREA075.EMI#13 |  | `002376cc` | `-` | similarity | 109/97 | 2 | const -[25] +[100] |
| `801f2d0c` WORLD02/AREA077.EMI#14 |  | `000348b0` | `000346c0` | similarity | 97/104 | 2 | const -[0x180200] +[0x710]; field -[w4+48] +[w2+14 w2+4] |
| `801f2ec0` WORLD02/AREA077.EMI#14 |  | `00034a54` | `00034864` | similarity | 93/102 | 2 | const -[9] +[37]; field -[] +[w4+5968] |
| `801f462c` WORLD02/AREA077.EMI#14 |  | `0003a238` | `0003a00c` | callee-align | 101/103 | 2 | calls -[] +[9c504 9c5e4] |
| `801f5bd8` WORLD02/AREA077.EMI#14 |  | `002386b4` | `002384e8` | gap-order | 9/10 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f2d20` WORLD02/AREA079.EMI#13 |  | `00238f58` | `00238d8c` | gap-align | 15/17 | 2 | const -[6] +[20] |
| `801f352c` WORLD02/AREA080.EMI#13 |  | `000344c4` | `-` | callee-align | 169/184 | 2 | const -[] +[68 84]; field -[w4+4 w4+64 w4+67 w4+68 w4+7 w4+71 w4+80 w4+83 ...] +[w1+0 w1+1 w1+2 w1+3 w2+0 w2+2 w2+68 w2+70 ...]; calls -[] +[9c504] |
| `801f2c94` WORLD02/AREA081.EMI#13 |  | `0023951c` | `00239350` | gap-align | 27/32 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w4+0] |
| `801f3408` WORLD02/AREA082.EMI#13 |  | `0002c988` | `0002c7ac` | similarity | 20/20 | 2 | calls -[] +[264c4] unpaired 1/0 |
| `801f3ba0` WORLD02/AREA082.EMI#13 |  | `0006d660` | `0006d394` | similarity | 52/61 | 2 | const -[] +[0x600]; globals 1 only PSX, 1 only PSP |
| `801f4008` WORLD02/AREA082.EMI#13 |  | `0006da64` | `0006d798` | similarity | 88/91 | 2 | const -[-8 0x4b0000] +[-32] |
| `801f4334` WORLD02/AREA082.EMI#13 |  | `0006dd50` | `0006da84` | similarity | 57/55 | 2 | const -[625] +[0x2710]; field -[w1+0] +[w1-26288]; calls -[] +[3154] |
| `801f4430` WORLD02/AREA082.EMI#13 |  | `0006e868` | `0006e5a0` | callee-align | 157/163 | 2 | const -[] +[-32 0xffffe190 0x400 0x13010000]; field -[] +[w1+0 w1+1] |
| `801f4c90` WORLD02/AREA082.EMI#13 |  | `0006e0ac` | `0006dde0` | similarity | 49/46 | 2 | const -[] +[100 24] |
| `801f5124` WORLD02/AREA082.EMI#13 |  | `0006e470` | `0006e1a4` | similarity | 34/31 | 2 | const -[625] +[0x2710] |
| `801f51bc` WORLD02/AREA082.EMI#13 |  | `0006e4f0` | `0006e224` | similarity | 41/38 | 2 | const -[625] +[0x2710] |
| `801f5424` WORLD02/AREA082.EMI#13 |  | `0006e734` | `0006e46c` | callee-align | 87/77 | 2 | const -[248 320] +[-32 352] |
| `801f34f8` WORLD02/AREA086.EMI#13 +1 copies |  | `00239e78` | `00239cac` | callee-align | 59/78 | 2 | const -[320] +[]; globals 1 only PSX, 1 only PSP |
| `801f3a0c` WORLD02/AREA087.EMI#13 |  | `0023aad8` | `0023a90c` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f4328` WORLD02/AREA087.EMI#13 |  | `0023afd8` | `0023ae0c` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f453c` WORLD02/AREA088.EMI#13 |  | `0023ca58` | `0023c88c` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f4e58` WORLD02/AREA088.EMI#13 |  | `0023cf58` | `0023cd8c` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f312c` WORLD02/AREA091.EMI#13 |  | `0023e46c` | `0023e2a0` | gap-align | 56/58 | 2 | const -[0x1a0000 0x1f8000] +[] |
| `801f3228` WORLD02/AREA091.EMI#13 |  | `0023e554` | `0023e388` | gap-align | 59/75 | 2 | const -[0x1a0000 0x1f8000] +[]; field -[] +[w2+0] |
| `801f2c88` WORLD02/AREA092.EMI#13 |  | `00238e68` | `00238c9c` | similarity | 15/36 (size 2.3x) | 2 | const -[20] +[14] |
| `801f3460` WORLD02/AREA094.EMI#13 |  | `00240368` | `0024019c` | gap-align | 35/43 | 2 | const -[] +[0x104028]; field -[] +[w2+0 w4+0]; globals 2 only PSX, 1 only PSP |
| `801f2f3c` WORLD02/AREA095.EMI#13 |  | `00067e54` | `00067b88` | similarity | 105/105 | 2 | const -[-128 -64] +[] |
| `801f2ecc` WORLD02/AREA096.EMI#13 |  | `00068290` | `00067fc4` | callee-align | 105/105 | 2 | const -[-128 -64] +[] |
| `801f3878` WORLD02/AREA098.EMI#13 |  | `000433b4` | `00043184` | similarity | 33/32 | 2 | calls -[] +[43f74] unpaired 1/0 |
| `801f4bc0` WORLD02/AREA098.EMI#13 |  | `00044180` | `00043f50` | callee-align | 95/97 | 2 | calls -[] +[9c504 9c5e4] |
| `801f4e7c` WORLD02/AREA098.EMI#13 |  | `00044438` | `00044208` | callee-align | 53/57 | 2 | const -[] +[28]; field -[] +[w1+0 w1+1 w4+2000]; globals 1 only PSX, 1 only PSP |
| `801f3608` WORLD02/AREA100.EMI#13 |  | `00241960` | `00241794` | gap-align | 9/10 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f352c` WORLD02/AREA103.EMI#13 |  | `0003060c` | `00030434` | similarity | 104/105 | 2 | calls -[] +[9c504 9c5e4] |
| `801f53f0` WORLD02/AREA103.EMI#13 |  | `00069e14` | `00069b48` | similarity | 126/119 | 2 | const -[-128] +[0xffffc000] |
| `801f3a9c` WORLD02/AREA104.EMI#14 |  | `002431ec` | `00243020` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f4ae4` WORLD02/AREA104.EMI#14 |  | `00243680` | `002434b4` | gap-align | 47/62 | 2 | calls -[] +[d5c50] |
| `801f6564` WORLD02/AREA104.EMI#14 |  | `00244a3c` | `00244878` | gap-align | 88/103 | 2 | const -[0x20000 0x60000] +[]; field -[] +[w1+0 w2+0 w2+192 w2+194 w2+196 w2+198]; globals 2 only PSX, 1 only PSP |
| `801f2c48` WORLD02/AREA105.EMI#13 |  | `0007b688` | `0007b3c0` | similarity | 16/20 | 2 | const -[524] +[12] |
| `801f32b8` WORLD02/AREA105.EMI#13 |  | `00245de4` | `00245c20` | gap-align | 24/27 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f33d4` WORLD02/AREA106.EMI#13 |  | `0007c134` | `0007be6c` | similarity | 316/309 | 2 | const -[] +[-16 0x400 0x440 64]; field -[] +[w2+2] |
| `801f3cd4` WORLD02/AREA106.EMI#13 |  | `002460dc` | `00245f18` | gap-align | 91/117 | 2 | const -[0x188000 0x8000 0xa0000] +[] |
| `801f2d40` WORLD02/AREA108.EMI#14 |  | `0002f8cc` | `-` | signature | 116/113 | 2 | calls -[] +[9c504 9c5e4] |
| `801f3a14` WORLD02/AREA108.EMI#14 |  | `000457f8` | `000455e0` | signature | 19/18 | 2 | calls -[] +[45938] unpaired 1/0 |
| `801f3a6c` WORLD02/AREA108.EMI#14 |  | `00045848` | `00045630` | similarity | 32/29 | 2 | field -[] +[w4+5968]; calls -[] +[45938] unpaired 1/0 |
| `801f3f34` WORLD02/AREA108.EMI#14 |  | `0006af18` | `0006ac4c` | similarity | 40/41 | 2 | const -[] +[0x650]; calls -[] +[3154]; globals 1 only PSX, 1 only PSP |
| `801f4c28` WORLD02/AREA108.EMI#14 |  | `002468bc` | `002466f8` | gap-align | 63/62 | 2 | const -[-256 0xd0d0d000] +[]; field -[w4+92] +[]; calls -[] +[3d84] |
| `801f3558` WORLD02/AREA112.EMI#13 |  | `00248b2c` | `00248968` | gap-align | 54/59 | 2 | const -[0x458000] +[0x470000] |
| `801f3944` WORLD02/AREA112.EMI#13 |  | `00248f20` | `00248d5c` | gap-align | 50/51 | 2 | const -[-6] +[7]; globals 1 only PSX, 2 only PSP |
| `801f39dc` WORLD03/AREA115.EMI#13 |  | `0024a684` | `0024a4c0` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f42f8` WORLD03/AREA115.EMI#13 |  | `0024ab84` | `0024a9c0` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f314c` WORLD03/AREA117.EMI#13 |  | `0006460c` | `00064340` | similarity | 150/131 | 2 | const -[0x18000] +[0x25500000] |
| `801f33f8` WORLD03/AREA117.EMI#13 |  | `00064854` | `00064588` | similarity | 45/47 | 2 | const -[] +[0x5f8]; globals 3 only PSX, 1 only PSP |
| `801f3590` WORLD03/AREA117.EMI#13 |  | `000649c4` | `000646f8` | similarity | 39/42 | 2 | const -[] +[0x5f8]; globals 1 only PSX, 1 only PSP |
| `801f364c` WORLD03/AREA117.EMI#13 |  | `00064a70` | `000647a4` | callee-align | 27/30 | 2 | const -[] +[0x5f8]; globals 1 only PSX, 1 only PSP |
| `801f4150` WORLD03/AREA118.EMI#13 |  | `00065818` | `0006554c` | similarity | 67/69 | 2 | const -[] +[0x5f8]; globals 4 only PSX, 1 only PSP |
| `801f4340` WORLD03/AREA118.EMI#13 |  | `000659e0` | `00065714` | similarity | 39/42 | 2 | const -[] +[0x5f8]; globals 1 only PSX, 1 only PSP |
| `801f43fc` WORLD03/AREA118.EMI#13 |  | `00065a8c` | `000657c0` | callee-align | 27/30 | 2 | const -[] +[0x5f8]; globals 1 only PSX, 1 only PSP |
| `801f4634` WORLD03/AREA118.EMI#13 |  | `0006675c` | `00066490` | similarity | 167/148 | 2 | const -[-128] +[0xffffc008] |
| `801f301c` WORLD03/AREA119.EMI#13 |  | `00066b58` | `0006688c` | callee-align | 52/68 | 2 | const -[0xffc10000 0xffc20000 0x10000] +[] |
| `801f3324` WORLD03/AREA119.EMI#13 |  | `00066ea8` | `00066bdc` | similarity | 150/131 | 2 | const -[0x418000] +[0x25500000] |
| `801f3608` WORLD03/AREA119.EMI#13 |  | `00067118` | `00066e4c` | similarity | 31/30 | 2 | const -[-88] +[480]; calls -[] +[3154] |
| `801f369c` WORLD03/AREA119.EMI#13 |  | `0006770c` | `00067440` | similarity | 268/275 | 2 | const -[0x78ca] +[928]; field -[] +[w2+2] |
| `801f3010` WORLD03/AREA120.EMI#13 |  | `00032ed4` | `00032cfc` | similarity-weak | 125/121 | 2 | const -[] +[12]; field -[] +[w2+0 w2+10 w2+2 w2+4 w2+6 w2+8]; calls -[] +[9c504]; globals 3 only PSX, 1 only PSP |
| `801f3d94` WORLD03/AREA120.EMI#13 |  | `00084718` | `00084450` | similarity | 22/22 | 2 | const -[0x20800 0xffff00] +[]; field -[w4+0] +[w1+1] |
| `801f39f0` WORLD03/AREA121.EMI#14 |  | `0024e0b0` | `0024deec` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f430c` WORLD03/AREA121.EMI#14 |  | `0024e5b0` | `0024e3ec` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f5028` WORLD03/AREA121.EMI#14 |  | `0024eb14` | `0024e950` | gap-align | 47/62 | 2 | calls -[] +[d5c50] |
| `801f5aa0` WORLD03/AREA121.EMI#14 +1 copies |  | `00244020` | `00243e54` | gap-align | 58/50 | 2 | const -[0x7b80] +[0xe00] |
| `801f2fc4` WORLD03/AREA125.EMI#13 |  | `000540fc` | `-` | signature | 120/117 | 2 | calls -[] +[9c504 9c5e4] |
| `801f31d0` WORLD03/AREA128.EMI#13 |  | `00250d4c` | `00250b88` | gap-align | 60/71 | 2 | const -[] +[0x104028]; field -[] +[w2+0 w4+0]; globals 2 only PSX, 1 only PSP |
| `801f3760` WORLD03/AREA130.EMI#13 |  | `00250ff4` | `00250e30` | gap-align | 10/10 | 2 | calls -[] +[95118] unpaired 1/0 |
| `801f37b8` WORLD03/AREA130.EMI#13 |  | `00251044` | `00250e80` | gap-align | 13/14 | 2 | calls -[] +[95118] unpaired 1/0 |
| `801f2f54` WORLD03/AREA131.EMI#13 |  | `00251a70` | `002518ac` | similarity | 59/62 | 2 | const -[22] +[0x1410]; field -[] +[w1+44 w2-1520] |
| `801f3560` WORLD03/AREA132.EMI#13 |  | `0004a198` | `-` | similarity | 143/146 | 2 | calls -[] +[9c504 9c5e4] |
| `801f2c1c` WORLD03/AREA133.EMI#13 |  | `00251efc` | `00251d38` | gap-align | 30/44 | 2 | const -[] +[30]; field -[] +[w1+15]; calls -[] +[8e810]; globals 1 only PSX, 4 only PSP |
| `801f40c8` WORLD03/AREA134.EMI#13 |  | `0004f018` | `0004edfc` | similarity | 53/50 | 2 | const -[0x33ffff] +[9]; field -[w1+9] +[w1+0]; calls -[] +[3154] |
| `801f45a0` WORLD03/AREA134.EMI#13 |  | `00054668` | `00054454` | similarity | 30/32 | 2 | const -[0xffff0000] +[12]; field -[w4+12 w4+20 w4+28] +[w4+0 w4+4 w4+8] |
| `801f46ec` WORLD03/AREA134.EMI#13 |  | `00054b60` | `-` | signature | 112/104 | 2 | calls -[] +[9c504 9c5e4] |
| `801f3abc` WORLD03/AREA135.EMI#14 |  | `0003e730` | `0003e514` | similarity | 24/26 | 2 | const -[] +[0x650]; field -[] +[w4+5968]; globals 1 only PSX, 1 only PSP |
| `801f46ac` WORLD03/AREA135.EMI#14 |  | `00252d34` | `00252b70` | gap-order | 87/92 | 2 | const -[0xfffe0000 0xfffe8000] +[] |
| `801f4e7c` WORLD03/AREA135.EMI#14 |  | `00253410` | `0025324c` | signature | 19/18 | 2 | const -[0x128000 0x168000] +[] |
| `801f60e0` WORLD03/AREA135.EMI#14 |  | `00253e10` | `00253c4c` | gap-order | 29/32 | 2 | const -[] +[45 56]; field -[] +[w2+58] |
| `801f3018` WORLD03/AREA136.EMI#13 |  | `00254960` | `0025479c` | gap-order | 10/15 | 2 | const -[5] +[103] |
| `801f3288` WORLD03/AREA139.EMI#13 |  | `00048d24` | `00048b08` | similarity | 149/147 | 2 | calls -[] +[9c504 9c5e4] |
| `801f4364` WORLD03/AREA139.EMI#13 |  | `00254d8c` | `00254bc8` | gap-align | 44/43 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f3950` WORLD03/AREA140.EMI#13 |  | `00071774` | `000714ac` | callee-align | 184/154 | 2 | const -[-128] +[6]; field -[] +[w1+0 w1+1 w1+2 w2+0] |
| `801f3c44` WORLD03/AREA140.EMI#13 |  | `000719dc` | `00071714` | callee-align | 58/79 | 2 | const -[320] +[]; globals 1 only PSX, 1 only PSP |
| `801f4380` WORLD03/AREA140.EMI#13 |  | `0025554c` | `00255388` | callee-align | 169/152 | 2 | const -[6] +[]; field -[] +[w1+0 w1+1 w1+19 w1+20 w1+3 w1+4 w1+9 w4+12 ...]; globals 7 only PSX, 1 only PSP |
| `801f3740` WORLD03/AREA141.EMI#13 |  | `00255950` | `0025578c` | callee-align | 53/48 | 2 | const -[-256 0x80808000] +[]; field -[w4+92] +[w4-25520] |
| `801f390c` WORLD03/AREA141.EMI#13 |  | `00255acc` | `00255908` | gap-align | 63/60 | 2 | const -[-256 0xc0c0c000] +[]; field -[w4+92] +[w4-26096] |
| `801f2cb0` WORLD03/AREA142.EMI#13 |  | `0006c7f8` | `0006c52c` | similarity-weak | 275/255 | 2 | const -[] +[0x900 256 512 768]; field -[w1+32 w1+33 w1+34 w1+40 w1+41 w1+42 w1+48 w1+49 ...] +[w1+28 w1+29 w1+30 w2+32 w2+34] |
| `801f30a0` WORLD03/AREA143.EMI#13 |  | `0004dccc` | `0004dab0` | callee-align | 77/81 | 2 | const -[0xffff0000] +[20] |
| `801f3584` WORLD03/AREA143.EMI#13 |  | `0007e5b4` | `0007e2ec` | similarity | 251/258 | 2 | const -[0x7940 544] +[]; field -[] +[w2+0 w2+16 w2+18 w2+2 w2+20 w2+4] |
| `801f3a20` WORLD03/AREA144.EMI#13 |  | `0025749c` | `002572d8` | gap-align | 34/37 | 2 | field -[] +[w4-25520 w4+5968]; calls -[] +[8f78c] |
| `801f332c` WORLD03/AREA145.EMI#14 |  | `00031a54` | `0003187c` | similarity | 102/104 | 2 | field -[] +[w4+2000]; calls -[] +[9c504 9c5e4] |
| `801f43dc` WORLD03/AREA145.EMI#14 |  | `00257e10` | `00257c4c` | gap-align | 68/77 | 2 | const -[-6 0x98000] +[7] |
| `801f4da4` WORLD03/AREA145.EMI#14 |  | `002580f4` | `00257f30` | gap-align | 74/79 | 2 | const -[12] +[0x640000]; field -[] +[w1+2] |
| `801f361c` WORLD03/AREA148.EMI#13 |  | `0006fbe8` | `0006f920` | similarity | 30/35 | 2 | const -[] +[0x650]; field -[] +[w4+5968]; globals 1 only PSX, 1 only PSP |
| `801f3bd0` WORLD03/AREA148.EMI#13 |  | `002592f0` | `0025912c` | gap-align | 19/21 | 2 | const -[] +[0x650]; field -[] +[w2+8]; globals 1 only PSX, 1 only PSP |
| `801f4798` WORLD03/AREA148.EMI#13 |  | `00259e70` | `00259cac` | callee-align | 163/180 | 2 | calls -[] +[9c504 9c5e4] |
| `801f2cd4` WORLD03/AREA149.EMI#13 |  | `00052fc0` | `00052db4` | similarity | 46/38 | 2 | field -[] +[w1-880 w2-944]; calls -[] +[45938] unpaired 1/0 |
| `801f3948` WORLD03/AREA151.EMI#13 |  | `0025ba74` | `0025b8b0` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f4264` WORLD03/AREA151.EMI#13 |  | `0025bf74` | `0025bdb0` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f389c` WORLD04/AREA152.EMI#13 |  | `0025daec` | `0025d928` | gap-align | 70/57 | 2 | const -[0x7b80] +[0xe00] |
| `801f41b8` WORLD04/AREA152.EMI#13 |  | `0025dfec` | `0025de28` | gap-align | 122/144 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w1+0 w4+0] |
| `801f2c04` WORLD04/AREA155.EMI#13 |  | `0025edac` | `0025ebe8` | gap-align | 46/50 | 2 | const -[] +[0x618]; globals 1 only PSX, 1 only PSP |
| `801f31bc` WORLD04/AREA167.EMI#13 |  | `00072744` | `0007247c` | callee-align | 251/228 | 2 | const -[-128 127] +[0xffffbfc0]; field -[] +[w1+0] |
| `801f4080` WORLD04/AREA167.EMI#13 |  | `0025f34c` | `0025f188` | gap-align | 23/27 | 2 | const -[] +[0x650]; field -[w1+0] +[w4-25520]; globals 2 only PSX, 1 only PSP |
| `801f2e10` WORLD04/AREA168.EMI#13 |  | `00051fa0` | `00051d94` | similarity | 130/135 | 2 | calls -[] +[9c504 9c5e4] |
| `801f3914` WORLD04/AREA168.EMI#13 |  | `00074f78` | `00074cb0` | callee-align | 264/237 | 2 | const -[-128 127] +[0xffffbfc0]; field -[] +[w1+0] |
| `801f3e18` WORLD04/AREA168.EMI#13 |  | `000747dc` | `00074514` | similarity | 67/80 | 2 | const -[320 0x10000 0x18000] +[] |
| `801f3fbc` WORLD04/AREA168.EMI#13 |  | `0007497c` | `000746b4` | similarity | 61/73 | 2 | const -[0x20000 0x28000 320] +[] |
| `801f3524` WORLD04/AREA169.EMI#13 |  | `002600e4` | `0025ff20` | gap-align | 13/14 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f3994` WORLD04/AREA170.EMI#13 |  | `0004c60c` | `0004c3f4` | callee-align | 142/163 | 2 | const -[] +[0x104002 44]; field -[] +[w1+0 w1+16384 w1+16385 w2+0 w4+0] |
| `801f4640` WORLD04/AREA170.EMI#13 |  | `00073848` | `00073580` | similarity | 189/174 | 2 | const -[-768 384] +[-16]; field -[] +[w4+0] |
| `801f3764` WORLD04/AREA171.EMI#13 |  | `0026151c` | `00261358` | gap-align | 13/14 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f32e8` WORLD04/AREA172.EMI#13 |  | `00055e64` | `00055c54` | similarity | 27/30 | 2 | const -[] +[6]; field -[w1+6] +[w1+0]; calls -[] +[] unpaired 2/0 |
| `801f36c0` WORLD04/AREA172.EMI#13 |  | `00056228` | `00056018` | callee-align | 122/130 | 2 | field -[w1+12 w1+13 w1+14 w1+20 w1+21 w1+22 w1+4 w1+5 ...] +[]; calls -[] +[9c504 9c5e4] |
| `801f4324` WORLD04/AREA172.EMI#13 |  | `00261c44` | `00261a80` | gap-align | 9/10 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f47f0` WORLD04/AREA172.EMI#13 |  | `00262048` | `00261e84` | similarity | 63/60 | 2 | const -[-256 0xc0c0c000] +[]; field -[w4+92] +[w4-26096] |
| `801f33c4` WORLD04/AREA173.EMI#14 |  | `00080980` | `000806b8` | similarity | 22/24 | 2 | const -[] +[0x650]; globals 1 only PSX, 1 only PSP |
| `801f3fbc` WORLD04/AREA173.EMI#14 |  | `00081424` | `0008115c` | similarity | 160/148 | 2 | const -[-128 -64] +[]; field -[] +[w2+0 w2+2 w2+4 w4+0] |
| `801f4a4c` WORLD04/AREA173.EMI#14 |  | `002629f4` | `00262830` | gap-align | 20/27 | 2 | const -[0x168000] +[0x7a0000]; field -[] +[w4+5968] |
| `801f2fec` WORLD04/AREA174.EMI#13 |  | `00050a4c` | `-` | similarity | 191/197 | 2 | calls -[] +[9c504 9c5e4] |
| `801f2e58` WORLD04/AREA186.EMI#13 |  | `00269ef0` | `00269d2c` | gap-align | 49/44 | 2 | const -[0x48000 0xa0000] +[]; field -[] +[w1+4679 w4+4612 w4+4616]; calls -[] +[107174] |
| `801f3368` WORLD04/AREA188.EMI#13 |  | `0005ef78` | `0005ec9c` | similarity | 33/32 | 2 | calls -[] +[43f74] unpaired 1/0 |
| `801f37cc` WORLD04/AREA188.EMI#13 |  | `0005f390` | `0005f0b4` | similarity | 48/47 | 2 | const -[] +[6]; field -[w1+6] +[w1+0]; calls -[] +[] unpaired 2/0 |
| `801f3514` WORLD04/AREA189.EMI#13 |  | `0007d188` | `0007cec0` | similarity | 165/151 | 2 | const -[0xff0000 320] +[-32 384] |
| `801f497c` WORLD04/AREA189.EMI#13 |  | `0007c8fc` | `0007c634` | caller-vote | 33/32 | 2 | const -[0xffffff 0xff000000] +[0x8fcc] |
| `801f4a04` WORLD04/AREA189.EMI#13 |  | `0007c980` | `0007c6b8` | callee-align | 88/97 | 2 | const -[0x10000] +[0x104030]; field -[] +[w4+0]; globals 2 only PSX, 1 only PSP |
| `801f2dbc` WORLD04/AREA191.EMI#13 |  | `0005531c` | `0005510c` | similarity | 19/20 | 2 | calls -[] +[45938] unpaired 1/0 |
| `801f2ea0` WORLD04/AREA193.EMI#13 |  | `0026d5fc` | `0026d438` | similarity | 155/148 | 2 | calls -[97fc8] +[d7ef0] |
| `801f3524` WORLD04/AREA198.EMI#13 |  | `0026e970` | `0026e7ac` | gap-align | 27/25 | 2 | calls -[] +[8f78c] |
| `8014b8f8` SLPS_009.90 |  | `00000f64` | `000018d0` | gap-order | 17/19 | 2 | const -[-2 14 7] +[]; field -[w1+112 w2+96] +[w1+44 w2+28] |
| `8014ce90` SLPS_009.90 |  | `00001300` | `00001c6c` | gap-align | 154/144 | 2 | const -[-128 96] +[0x3ff 32] |
| `8014d440` SLPS_009.90 |  | `000026f0` | `0000305c` | gap-order | 36/32 | 2 | calls -[a540c] +[2ce4] |
| `8014db0c` SLPS_009.90 |  | `00002ce4` | `00003650` | gap-align | 67/64 | 2 | const -[-16] +[0xfffffc00]; field -[] +[w2+40 w4+76] |
| `8014df9c` SLPS_009.90 |  | `000032bc` | `00003c28` | gap-align | 19/19 | 2 | const -[0xffe] +[]; calls -[] +[] unpaired 0/2 |
| `8014ecac` SLPS_009.90 |  | `000064f4` | `00006e68` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014ecd4` SLPS_009.90 |  | `00006518` | `00006e8c` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014ecfc` SLPS_009.90 |  | `0000653c` | `00006eb0` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014ed24` SLPS_009.90 |  | `00006560` | `00006ed4` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014ed4c` SLPS_009.90 |  | `00006584` | `00006ef8` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014ed74` SLPS_009.90 |  | `000065a8` | `00006f1c` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014ed9c` SLPS_009.90 |  | `000065cc` | `00006f40` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014edc4` SLPS_009.90 |  | `000065f0` | `00006f64` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014f028` SLPS_009.90 |  | `00006854` | `000071c8` | gap-align | 15/15 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014f068` SLPS_009.90 |  | `00006890` | `00007204` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014f090` SLPS_009.90 |  | `000068b4` | `00007228` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8014f2f4` SLPS_009.90 |  | `00006b18` | `0000748c` | gap-align | 9/9 | 2 | calls -[] +[6bd4] unpaired 1/0 |
| `8015002c` SLPS_009.90 |  | `00009638` | `-` | caller-vote | 96/96 | 2 | const -[0x7800 63] +[21] |
| `80154780` SLPS_009.90 |  | `0000d1c0` | `0000d068` | gap-align | 27/21 | 2 | const -[56] +[58] |
| `80155238` SLPS_009.90 |  | `0000f5c0` | `0000f464` | caller-vote | 190/241 | 2 | const -[] +[0x10402a 0x104030]; field -[w1+1] +[w1+16384 w1+16385 w2+0 w4+0] |
| `80155a94` SLPS_009.90 |  | `0000ed08` | `0000ebb0` | gap-order | 22/22 | 2 | const -[] +[0x10401e]; field -[] +[w2+0]; globals 3 only PSX, 1 only PSP |
| `80156844` SLPS_009.90 |  | `000103fc` | `000102a0` | gap-align | 57/47 | 2 | const -[12 0xffffff 0xff000000] +[] |
| `80157308` SLPS_009.90 |  | `0001090c` | `000107a8` | gap-align | 89/88 | 2 | const -[64] +[0x104030 128 0xb1800000]; field -[] +[w1+16384] |
| `801576d4` SLPS_009.90 |  | `00010a6c` | `-` | gap-align | 117/135 | 2 | const -[-128] +[0xffffc000]; field -[w1+0 w1+1 w1+3 w2+0 w2+2 w2+4] +[w2+56 w2+58 w2+60] |
| `80157b38` SLPS_009.90 |  | `00012484` | `-` | similarity | 164/176 | 2 | const -[-128] +[0xffffc000]; field -[w1+19 w1+2 w1+3 w2+0 w2+2 w2+4] +[w2+56 w2+58 w2+60 w4+16] |
| `80158100` SLPS_009.90 |  | `00010d80` | `00010c14` | gap-align | 22/28 | 2 | const -[] +[0x104028]; field -[] +[w2+0]; globals 1 only PSX, 1 only PSP |
| `8015815c` SLPS_009.90 |  | `00010df0` | `00010c84` | gap-align | 22/28 | 2 | const -[] +[0x104028]; field -[] +[w2+0]; globals 1 only PSX, 1 only PSP |
| `801587a0` SLPS_009.90 |  | `00013068` | `00012ef4` | similarity-weak | 167/222 | 2 | const -[128] +[-8 0x104030]; field -[w1+3] +[w1+16384] |
| `80158a54` SLPS_009.90 |  | `00010e60` | `00010cf4` | caller-vote | 53/73 | 2 | const -[] +[0x104020 0x104028]; field -[] +[w2+0 w4+0] |
| `80158b60` SLPS_009.90 |  | `00010f90` | `00010e24` | caller-vote | 59/74 | 2 | const -[480] +[0x104022]; field -[] +[w2+0 w4+0] |
| `801593fc` SLPS_009.90 |  | `00011938` | `000117bc` | gap-align | 110/107 | 2 | const -[320] +[]; field -[w1+2] +[]; globals 5 only PSX, 1 only PSP |
| `8015c890` SLPS_009.90 |  | `0008e6a8` | `0008e400` | gap-align | 101/69 | 2 | const -[45] +[360]; field -[w4+0] +[w4+12 w4+8] |
| `8015d140` SLPS_009.90 |  | `0008edc0` | `0008eb18` | gap-order | 105/110 | 2 | const -[] +[52 56]; field -[w4+52 w4+56] +[w4+0 w4+272 w4+336 w4+5968] |
| `8015d860` SLPS_009.90 |  | `00094e94` | `00094c0c` | gap-align | 17/17 | 2 | calls -[] +[] unpaired 0/3 |
| `80162610` SLPS_009.90 |  | `00095c6c` | `000959e8` | callee-align | 43/38 | 2 | calls -[] +[95118] unpaired 1/0 |
| `80163d68` SLPS_009.90 |  | `00097c08` | `00097984` | similarity-weak | 38/84 (size 2.2x) | 2 | const -[] +[0xfffff800 0x800 0x801 31] |
| `80165dcc` SLPS_009.90 |  | `000b86c4` | `000b84ac` | gap-order | 34/40 | 2 | globals 2 only PSX, 3 only PSP |
| `8016776c` SLPS_009.90 |  | `000dcf40` | `000dcc58` | similarity | 36/29 | 2 | const -[] +[0x72b8 0x72c8]; calls -[dec] +[] |
| `801afba0` SLPS_009.90 |  | `0009a2c0` | `0009a03c` | caller-vote | 0/119 (size 120.0x) | 2 | const -[] +[31]; field -[] +[w2+40]; calls -[] +[9a49c] |
| `801afd54` SLPS_009.90 |  | `0009b1ac` | `0009af28` | caller-vote | 0/228 (size 229.0x) | 2 | const -[] +[31]; field -[] +[w2+32 w2+40]; calls -[] +[9a49c] |
| `801bf0b8` SLPS_009.90 |  | `000d5edc` | `000d5bf4` | caller-vote | 0/71 (size 72.0x) | 2 | calls -[] +[d5ad8 d5b40] |
| `801eec30` SLPS_009.90 |  | `002111f8` | `00210f58` | caller-vote | 0/27 (size 28.0x) | 2 | calls -[] +[3ff74] |


## 8. Not verified

- **Every `differs` row is unread.** The verdicts are a normaliser's, checked
  on the `BOSS024` module and on a dozen rows read while tuning it; stage B
  says which are content.
- **Pair precision beyond the 15 hand-made pairs** rests on the callee
  agreement (0.8 % contradicted) and on reading a few rows. Pairs tagged
  `similarity-weak` (52 testable, 2 contradicted) and rows with a `size`
  flag deserve the reader's check before anything else.
- **The 3,510 unpaired PSX functions** (26.7 %) are not diffed. They include
  36 named ones (section 2.1) and whatever the PSP dropped, merged or split.
  The 5,282 PSP functions that are no twin likewise (copies of a twin among
  them); some are the PSP's own platform layer.
- **The PSX side is JP only** (the sibling's captures). PSP-EU's columns come
  from PSP-JP by masked words and order; no US or European PSX code was read.
- **`RYUD`'s and the button-label page's readers** are not found (section 6).
- **The half split** is by family and, for the boot EXE, by name prefix or
  battle-only callers; unnamed boot functions with mixed callers go to the
  field half.
- **Function boundaries** come from rules (section 2): a jump table whose
  cases run past a return without a prologue after it still merges two
  functions on either side.

## 9. For the other files (the coordinator's)

- **`docs/README.md`, one row:** `psp-code-diff.md` | The PSP-only round's
  code diff, stage A (2026-10-12): `tools/psp_pair.py` pairs 9,624 of 13,134
  PSX functions (139 of 175 named game functions) with `BOOT.BIN` across the
  two compilers, verified on the 15 hand-made pairs; the diff, normalised for
  the compilers' habits, leaves 1,225 `differs` (317 battle and effects, 908
  field and system) for stage B. Item 87's special case is in both builds;
  `RYUD`'s and the button page's readers are not found by address.
- **No ledger entry**: a list, not a behaviour change.
- **For SI and IM:** nothing found that stops our code running on the PSP's
  `exp_table` or consumable 87. `RYUD` and the arenas are open (section 6).
  IM's table readers in the ELF: `pairs.json` maps every paired PSX function
  to its PSP-JP and PSP-EU twins; a PSX reader of a table found by IM gives
  its PSP reader directly.

READY FOR STAGE B.

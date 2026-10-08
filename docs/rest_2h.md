# Group R2H: window kinds - the shop set's 11..18, the battle menu's 3..5, the gene list - the masters' windows, and three strays

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave two, on the
round branch's tip `fa583bc`. **36 functions ours** (`src/game/rest_2h.cpp`,
`rest_2h.h`, `rest_2h_callees.h`, shadow name `rest_2h`): the cut's 34 rows
for R2H (`analysis/round14_cut.tsv`), the start in their span no list had
(`0x59C810`, band_rows' "code no list has") and `0x59E160`, game code the
catalogue filed under the renderer by its address range (section 5). Each
read to its last instruction with capstone and fuzzed through the scenario
harness's **field** mode (used unchanged): 144,000 rounds, **0 mismatches**;
84 controls planted, 83 refused by a count and one an equivalent mutant with its near variant refused (section 6). Eleven `.data` tables named. No PSX twin is paired for any of
them (`analysis/pairs_propagated.json`). Fuzz only; the camp route's reach is
section 9.

**What the band is.** Window kinds, almost all: a record of `WindowRecords`
(`0x803160`, 22 of 0x24) is the dword at `0x905B84` while
`Field_RunTaskRecords` runs it, `+2` its kind, `+3` its step, `+4` / `+6` x /
y (s16), `+8..` the kind's bytes. Record handler 7 jumps through
`Window_Handler7KindTable` (DI's `Window_Handler7Kinds`, ours since round
eight, [`menu_draw_helpers.md`](menu_draw_helpers.md)), handler 8 through
`Window_Handler8KindTable`, handler 6 through `MenuList_Kinds` (DH's
`MenuList_Run`). DI took kinds 0..10 of handler 7 and 0..2 of handler 8;
**this group takes kinds 11..18 and 3..5**, `MenuList_Kinds[20]`, the
draws they call, their slides, and three strays at the band's ends. A "run"
calls its step through a `.data` step table by `+3`, then draws the record
(re-read); a "slide" moves x or y by a constant and, past a bound (signed
16-bit), stores the bound and sets `+3` to 0. What the windows show comes
from the code and from `docs/HANDOFF.md`'s note on the owner's
`campingFishing.txt` (the camp's skill note, party choice and masters); the
names say what the code does - "master" and "pupil" for 0x59C2C0's list of 17
system messages and the records whose `+0x1F` is its pick, "gene" for the 18
bits of `0x904650` other docs already call so ([`field_o.md`](field_o.md)'s
`EventCond_Gene`, [`battle_e5.md`](battle_e5.md)).

| Function | Entry | Bytes | Reached by | What |
|---|---|--:|---|---|
| `MenuList_ReserveWinDraw` | `0x59AA80` | 0x35D | E8 from `0x59AA30` (`MenuList_Kinds[19]`, R2G's) | the reserve list for an object: DIV-0011's frame call, then per id of the list `+0x20` points at (count `+0xA`) a member's panel - `PartyForm_DrawReserve`'s twin (`field_s.cpp`) with a dimmed form for a record with `+0xB` bit 1 |
| `MenuList_GeneWinRun` | `0x59ADE0` | 0x20 | `MenuList_Kinds[20]` | step (`MenuList_GeneWinSteps`, 3), then the draw |
| `MenuList_GeneWinDraw` | `0x59AE00` | 0x415 | E8 (the run) | the list `0x6BE0B0` from the 18 gene bits; a scrolling list of system messages `0x4171 + entry` with an 8 x 8 icon, the `+0xB` row lit; `+0xC` = the picked entry; title, frame, scroll bar |
| `ShopWin_SharedListRun` | `0x59BE50` | 0x20 | handler 7 kind 11 | step (`ShopWin_SharedListSteps`, 5), `SharedList_DrawList` (FS's) |
| `ShopWin_SharedListSlideTo28` | `0x59BE70` | 0x28 | SharedListSteps[3] | x + 0x20 to 0x28 |
| `ShopWin_SharedMemberRun` | `0x59BEA0` | 0x20 | kind 12 | step (5), `SharedList_DrawMember` (FS's) |
| `ShopWin_MemberStatusRun` | `0x59BEC0` | 0x32 | kind 13 | step (3), `Menu_DrawMemberStatus(+4, +6, +0xA, 0)` |
| `ShopWin_RowMenuRun` | `0x59BF00` | 0xD | kind 14 | `ShopWin_DrawRowMenu(record)`, no step |
| `ShopWin_DrawRowMenu` | `0x59BF10` | 0x1FE | E8 (the run) | a box of `0x66B37C` rows: title `0x66B36C[0]`, row i the text `0x66B36C[0x66B37D[i]]`, `+0xB` lit (7 / 2), `+0xA` lit (7 / 0) |
| `ShopWin_MasterListRun` | `0x59C110` | 0x20 | kind 15 | step (`ShopWin_MasterListSteps`, 4), `MasterWin_DrawList` |
| `MasterWin_SlideOut` | `0x59C130` | 0x28 | MasterListSteps[1] | x - 0x20 to -120 - **DIV-0041's bound** (section 2) |
| `MasterWin_SlideTo64` | `0x59C160` | 0x28 | MasterListSteps[2] | x + 0x20 to 0x64 |
| `ShopWin_MasterCaptionRun` | `0x59C190` | 0x20 | kind 16 | step (3), `MasterWin_DrawCaption` |
| `MasterWin_CaptionSlideTo8C` | `0x59C1B0` | 0x28 | MasterCaptionSteps[2] | x - 0x20 to 0x8C |
| `ShopWin_PupilsRun` | `0x59C1E0` | 0x20 | kind 17 | step (3), `MasterWin_DrawPupils` |
| `MasterWin_PupilsSlideDown` | `0x59C200` | 0x28 | PupilsSteps[1] | y + 0x10 to 0xF0 |
| `MasterWin_PupilsSlideUp` | `0x59C230` | 0x28 | PupilsSteps[2] | y - 0x10 to 0x80 |
| `ShopWin_ItemCountRun` | `0x59C260` | 0x2A | kind 18 | step (3), `SharedList_DrawItemCount(+4, +6)` |
| `ShopWin_ItemCountSlideToD2` | `0x59C290` | 0x28 | ItemCountSteps[2] | x - 0x20 to 0xD2 |
| `MasterWin_DrawList` | `0x59C2C0` | 0x4BE | E8 (the run) | a 17-byte stack list from the bits of `0x904657..`; `+0xD` = the bit of the `+0xB` entry (0xFF none); rows: `MasterWin_Available(entry - 1)`, system message `0x110 + entry`, beside it the mark `0x66A2D8` (available; the byte `t`, a star in the shipped font - DIV-0064's sixth group since 2026-10-07) or the dot icon; `+0xB` lit, `+0xC` in colour 2; title, frame, scroll bar over the stack list |
| `MasterWin_Available` | `0x59C780` | 0x8C | E8 (DrawList) | al: entries 0xB..0xE by a jump table (bits 3, 4, 5 of `0x904061`, story flag 0x6C); others: every skill of `MasterWin_Requirements[index]` known |
| `MasterWin_SkillKnown` | `0x59C810` | 0x58 | E8 (Available) | al: the byte in records 0..6's ten ability slots (`+0x7E`) or the shared list `0x904574` |
| `MasterWin_DrawCaption` | `0x59C870` | 0x73 | E8 (the run) | box, border, system message `0x100 + +0xA` |
| `MasterWin_DrawPupils` | `0x59C8F0` | 0x10D | E8 (the run) | portraits of records 0..7 with `+0xB` bit 0 whose `+0x1F` is window 1's `+0xD` (three to a row), the label box `0x66A1F8` (not under a Latin overlay: DIV-0083) |
| `MasterWin_DrawPortrait` | `0x59CA00` | 0xF8 | E8 (Pupils) | a 0x24 x 0x28 sprite from the cell `0x66B470[index]` (index 4 is 0xB from chapter 8 on), grey by the shade |
| `BattleMenuWin_ItemListSlideLeft` | `0x59CB90` | 0x26 | `BattleMenuWin_ItemListSteps[3]` | x - 0x20; below 0x53: **0x52** (D88's pair) |
| `BattleMenuWin_EquipRun` | `0x59CC10` | 0x47 | handler 8 kind 3 | step (`BattleMenuWin_EquipSteps`, 4), `BattleEquipWin_Draw` (BE7's) of the member `0x66972C[0x904065[+0xC]]` |
| `BattleMenuWin_EquipSlideTo62` | `0x59CC60` | 0x28 | EquipSteps[3] | x + 0x20 to 0x62 |
| `BattleMenuWin_EquipItemsRun` | `0x59CC90` | 0x20 | kind 4 | step (`BattleMenuWin_EquipItemsSteps`, 4), `BattleEquipWin_DrawItems` |
| `BattleMenuWin_EquipItemsSlideTo98` | `0x59CCB0` | 0x28 | EquipItemsSteps[1] | x + 0x20 to 0x98 |
| `BattleMenuWin_VerbPairRun` | `0x59CCE0` | 0x1C | kind 5 | `Menu_DrawVerbPair(+4, +6, +0xB)`, no step |
| `BattleEquipWin_DrawBar` | `0x59DB70` | 0x76 | E8 from `BattleEquipWin_Draw` (BE7's, raw) and `0x585DC0` (R2C's) | one 8 x 8 sprite of a stat bar |
| `BattleEquipWin_DrawItems` | `0x59DBF0` | 0x568 | E8 (the run) | `+8` the category by the slot `+9`; the category's items `Item_CanUse(4, member, ..)` allows (the armour's by its icon kind too) into `0x6BE0C4` / `0x6BE144`; a scrolling `Menu_DrawItemRow` list, the category title through **DIV-0059's site**, the count "n / 128" |
| `Menu_DrawVerbPair` | `0x59E160` | 0xCB | E8 (kind 5) | two buttons, verbs `0x66A228` and `0x66A240`, lit / dimmed by the selection |
| `DInput_EnumJoystick` | `0x5A9620` | 0x64 | the original `DInput_Init`'s `push 0x5A9620` (stdcall callback) | CreateDevice, QueryInterface, a case-blind product-name compare - section 1.1 |
| `Cfg_SetKeyTable` | `0x5A9860` | 0x15 | E8 from `Cfg_Load` (Capcom's) | 32 dwords into `Key_Table` |

### 1.1 The details read

- **The runs re-read the record** (`0x905B84`) after the step and pass it
  (or its words) to the draw; ours reads it volatile after every call.
  `ShopWin_MemberStatusRun` pushes a fifth word 0 after
  `Menu_DrawMemberStatus`'s four; ours passes it too (the callee reads four).
- **`MenuList_ReserveWinDraw`** reads `+4`, `+6`, `+0xA` and `+0x20` once
  after the frame call and the count `+0xA` again at each row's end. Per
  row: `rec = CharacterRecords + 0xA4 * 0x66972C[id]` (both bytes unbounded,
  read in place); `dim` = `rec +0xB` bit 1. Box `(X, Y, 0x7D, 0x30, 0x80 +
  dim, style)`; portrait `(X + 0x56, Y, +9, dim ? 1 : (+0x10 >> 6) & 2)`; name
  `(X + 0x14, Y + 1, dim ? 7 : 0, 5)`; `Menu_DrawTile16(X + 2, Y + 0x15, 3,
  dim)`, the level, the status text (colour `dim ? 7 : 1`), the tile `(.., 0,
  dim)`, HP (7 dim; else 4 on `+0x11` bit 5, 2 at 1 or less) and its maximum
  (7 dim; else 4 on `+0x1E`), the tile `(.., 1, dim)`, AP (7 dim; else 4 at a
  quarter of `+0x22` or less, 2 at 0) and its maximum (7 dim, else 0). The
  colours and the dim flag go out in dwords whose upper three bytes are the
  stack's; each callee reads the byte.
- **The scrolling lists** (`MenuList_GeneWinDraw`, `MasterWin_DrawList`,
  `BattleEquipWin_DrawItems`): `Menu_ListScroll(+0xA, &offset, &moving,
  state)` with the offset written into the low byte of the original's own
  argument slot and `moving` into a stack dword (ours: two locals; the callee
  writes both on every path, `menu_windows.cpp`); the rows run from the
  answered top while the index is below `top + moving + 9` (`+ 7` rows for
  the item list) at `y + offset + 0x1A + 13 row`, the lit row first drawn in
  colour 7 (dim 1) and the rest two units up. The gene list **ends** at its
  first empty entry; the masters' list **skips** one; the item list skips
  one and lights the row whose `+0xA` (re-read) `+ r` is `+0xB`.
- **`MasterWin_Available`**: index `& 0xFF`; `0xB..0xE` through the jump
  table `0x59C7FC` (the clone's `JumpTable`); the others walk
  `MasterWin_Requirements[index]` to its 0xFF. al only; the rest of eax is
  not set (`ret_mask` 0xFF).
- **`MasterWin_DrawPortrait`**: index 4 is 0xB when `Cond_ByteFA` (s8) is 8
  or more (the index is the caller's argument slot, rewritten); the cell
  pointer is computed before the calls and read after them; x and y their
  words as unsigned floats; the packet cursor re-read after `Gfx_CommitPrim`.
- **`BattleEquipWin_DrawBar`** reads the packet cursor once, before
  `Gpu_SetSprt8`, and its x / y as unsigned words (the original stores
  `x & 0xFFFF` into its own argument slots and `fild`s them).
- **`BattleEquipWin_DrawItems`**: `+8` = 2 for slot `+9` 1..3, 3 for 4 or 5,
  else 1 (a jump table, `0x59E144`); per item of the category (128): the
  member `0x66972C[0x904065[+0xC]]` re-read each item, `Item_CanUse(4,
  member, +8, id)`, and for the armour (`+8` 2, re-read) `Item_IconKind(2,
  id) - 2 == +9 - 1`; then the counts and the ids zero-filled to 128 (the
  counts first). The count `"n / 128"` is sprintf'd with four words (the
  stand-in logs three: the fourth is a constant).
- **`DInput_EnumJoystick`** (stdcall, `ret 8`): `IDirectInput::CreateDevice(
  DInput_Object, &instance->guidInstance, &DInput_Joystick, 0)` (vtable +0xC);
  non-zero: answer 1 (DIENUM_CONTINUE). Else `QueryInterface(DInput_Joystick
  re-read, the IID at 0x5C4718, &DInput_Joystick2)`, then `_stricmp(instance
  + 0x12C, 0x66C7B0)` (the C runtime's, `0x5C2B40`) 0: `DInput_JoystickFound
  = 1`; answer 0 (DIENUM_STOP). Ours makes the same two COM calls through the
  vtables (`dinput.h`) and calls the C runtime's `_stricmp` by address.
  DIV-0050's `DInput_Init` (ours) never enumerates, so this runs only with
  `BOF3X_ORIGINAL=DInput_Init`.
- **`Cfg_SetKeyTable`**: `rep movsd`, 0x20 dwords forward; ours copies dword
  by dword forward.

## 2. Divergence and the patches inside the band

No divergence of this group's own. Three of other modules live inside these
bodies, each kept working for ours:

- **DIV-0011** (`menu_frame.cpp`): `MenuFrame_Inject` re-aims the call at
  `0x59AA98` (the first of `MenuList_ReserveWinDraw`, to the empty
  `0x4DF820`) at `Menu_DrawFrame`. Ours calls whatever that site reaches,
  read from its rel32 at each call (`SiteTarget(0x59AA98)`), as `field_s.cpp`
  does for `PartyForm_DrawReserve`'s `0x581313`: with the divergence on, the
  frame; with `BOF3X_ORIGINAL=Menu_DrawFrame`, the empty function.
- **DIV-0059** (`battle_draw.cpp`): under a Latin overlay `BattleDraw_Inject`
  re-aims the title draw at `0x59DEFA` (inside `BattleEquipWin_DrawItems`) from
  `Text_DrawAt` to `ListTitle_DrawAt`. Ours calls what `0x59DEFA` reaches the
  same way. The fuzz ran without a language overlay (the site reaching
  `Text_DrawAt`); with one, the fuzz file keys a row of its own by the site
  (`ListTitle_DrawAt (DIV-0059)`), as for DIV-0011's frame.
- **DIV-0041** (`widescreen.cpp` `kSlides`): the imm32 at `0x59C136` - the
  -120 of `MasterWin_SlideOut`'s `mov ecx, imm32` - is widened by the columns
  under `BOF3X_WIDE=1`. Ours reads that word at every call
  (`rest_2h::g_master_bound`, `0x59C136` outside the fuzz) as the original's
  `cmp word [eax + 4], cx` does; `Rest2H_Inject` refuses to start unless
  `0x59C135` is still the `mov ecx` opcode (0xB9) and logs the bound
  (`rest_2h: MasterWin_SlideOut's bound -120`, -173 wide). The fuzz copies
  `0x59C130` itself, seeds the copy's immediate (-120, -173, 0, -1, 0x7FFF,
  -0x8000, random) and aims ours at it (control C27).
- **Language overlays** (DIV-0064, `labels.cpp`): the category titles' table
  `0x66B5C4` (read by `BattleEquipWin_DrawItems`) is one of the tables whose
  words DIV-0064 retargets; ours reads the word in place at each call.
  `0x66A1F0` / `0x66A1F8` (the masters' two headers,
  [`yes-no-prompts.md`](yes-no-prompts.md) section 5) and `0x66B1F0` are read
  in place too. No other DIV, cheat or `widescreen.cpp` entry names an
  address of the 36 (grep of `DIVERGENCE.md`, `cheats.cpp`, `widescreen.cpp`,
  `labels.cpp`, `lang_*.cpp`, `yes_no*.cpp`, 2026-10-04).
- **DIV-0050** (`pad_read.cpp`): `DInput_EnumJoystick` is the callback the
  replaced `DInput_Init` no longer passes; nothing patches it.

**No full-frame fill** (a 320 x 240 TILE or quad) is drawn in the band.

## 3. Arguments, answers, the tables

**Upper halves.** The originals push coordinates made by `mov cx, [..]` and
bytes made by `mov cl, [..]` over whatever the register held (a record
pointer's upper half, a callee's leftover, the stack's), and `edi` / `ebx`
y's moved by `add edi, 0xFFFE` (a 32-bit add of 0xFFFE, two lower in the word
and one higher above it). Every callee reads only the low word of a
coordinate and the low byte of a byte (`menu-windows.md` section 2, the field
standard's masks, `Menu_DrawTile16` / `MasterWin_DrawPortrait` read here), so
ours passes them zero-extended and the fuzz masks accordingly.
**Answers**: `MasterWin_Available` and `MasterWin_SkillKnown` answer in al
(`ret_mask` 0xFF); `DInput_EnumJoystick` in eax (0xFFFFFFFF).

**The tables** (`symbols.toml` `[[data]]`, `ctype` `unsigned long`; each
count the run of handler words up to the next table or data, read by hand,
2026-10-04; band_rows' "13 code entries" for `0x66B2D8` and the like ran on
into the following tables):

| Address | Name | Count | Read by (by `+3`, unchecked) |
|---|---|--:|---|
| `0x66B1CC` | `MenuList_GeneWinSteps` | 3 | `MenuList_GeneWinRun` (`0x66B1D8` after it is a piece list) |
| `0x66B2D8` | `ShopWin_SharedListSteps` | 5 | `ShopWin_SharedListRun` |
| `0x66B2EC` | `ShopWin_SharedMemberSteps` | 5 | `ShopWin_SharedMemberRun` |
| `0x66B300` | `ShopWin_MemberStatusSteps` | 3 | `ShopWin_MemberStatusRun` |
| `0x66B384` | `ShopWin_MasterListSteps` | 4 | `ShopWin_MasterListRun` |
| `0x66B394` | `ShopWin_MasterCaptionSteps` | 3 | `ShopWin_MasterCaptionRun` |
| `0x66B3A0` | `ShopWin_PupilsSteps` | 3 | `ShopWin_PupilsRun` |
| `0x66B3AC` | `ShopWin_ItemCountSteps` | 3 | `ShopWin_ItemCountRun` |
| `0x66B56C` | `BattleMenuWin_EquipSteps` | 4 | `BattleMenuWin_EquipRun` |
| `0x66B57C` | `BattleMenuWin_EquipItemsSteps` | 4 | `BattleMenuWin_EquipItemsRun` |
| `0x66B424` | `MasterWin_Requirements` | 17 | `MasterWin_Available` (pointers to 0xFF-ended skill lists) |

The step tables hold, besides this group's slides, `BareRet` and the shared
slides `MenuSlide_RightOff` / `_RightTo17` / `MenuWin_SlideOutLeft` (ours) and
`0x596920` (R2F's), `0x59A960`, `0x59A9E0`, `0x59AA50`, `0x59A610` (R2G's) -
all reached by what the table holds, none called by address. Other `.data`
the bodies read in place (not named): the piece lists, the row menu's
`0x66B36C` / `0x66B37C` / `0x66B37D`, the portraits' cells `0x66B470`, the
format strings, the two status strings, the verbs.

## 4. The fuzz (`rest_2h_fuzz.cpp`)

36 clones, 4,000 rounds each, field mode (`g.field`). 20 `kState` (the runs
and slides, the record the dword at `0x905B84`), 16 `kCall` (the draws handed
a window record as a[0]; `MasterWin_Available` / `SkillKnown` a byte with
garbage above it; the portrait, the bar and the verb pair their words;
`Cfg_SetKeyTable` a scratch buffer; the callback an instance buffer).
`BOF3X_R2H_ONLY=<name>` runs the clones whose name contains it.

**Four copies are the fuzz file's own**, handed to the harness as a six-byte
`jmp [copy]` (field_s's way): `0x59AA80` and `0x59DBF0`, whose sites DIV-0011
and DIV-0059 re-aim before this inject (each site aimed at the recorder for
where it reaches now - the harness's `CloneOriginal` would refuse it); the
`0x59DBF0` copy's jump table relocated; `0x59C130`, whose immediate the seed
moves; `0x5A9620`, a stdcall callback, behind a fifteen-byte cdecl adaptor
(`push [esp+8]` twice, `call [copy]`, `ret`). Every site of a copy calls a
trampoline into `sh::StandIn` of its callee's address (or the site's target),
so the copies log exactly as the harness's.

**The callees**: the ten of the group called by `E8`, FS's three
`SharedList_*`, BE7's `BattleEquipWin_Draw` (member byte, x / y words, set,
flags byte, record), `Menu_DrawMemberStatus` with its fifth word,
`Menu_DrawTile16`, `Gpu_SetSprt8`, `Crt_sprintf` (three words, the format a
string; its stand-in writes up to seven digits and a NUL, so the
`Text_DrawFont8` after it hashes something), `_stricmp` (both strings
hashed; a `kFlag`), and the two COM methods: `g_com`'s fake `IDirectInput`
and two devices, whose stdcall methods log through `sh::Record` under the
call sites' addresses (`0x5A9638`, `0x5A965A`) and answer from the log
(CreateDevice fails one time in three, else writes one of the two devices).
DIV-0011's and DIV-0059's rows are keyed at start-up by where their sites
reach. The rest is the field standard set.

**Regions** beyond field mode's: `WindowRecords`, `CharacterRecords` past the
standard `0x903A94`, the inventory lists `0x904160..0x904560`, `0x6BE0A0` +
0x1C0 (the gene list, the item list and its counts, and past them - the rows
read past by `top + r`), `Key_Table`, the four DirectInput globals, the
instance buffer and the reserve ids. 27,924 bytes, 46 regions, 282 stand-ins.

**The seed**: the current record and the record a draw is handed (half the
time the same), each with `+3` inside its run's table, x / y at the slides'
arrivals +/- 0, 1, 2, 0x10, 0x20, 0x40, `+8` the scroll states (0, 0x10,
0x11, 0x14, 0xF0, 0xF4, 0xF5), `+9` the slots 0..6, **`+0xA` 1..6** (the
lists' top; the masters' list's rows stay inside its 17 bytes, section 7),
`+0xB` / `+0xC` / `+0xD` near the lists; records 0..7's `+9`, `+0xB`,
`+0x10`, `+0x11`, HP at 0 / 1 / 2, `+0x1E`, `+0x1F` 0..3 against window 1's
`+0xD`, AP at its maximum's quarter +/- 1; the party lists with ids that have
a record; the gene bits (rarely all eighteen); the masters' bits; `0x904061`;
a master's required skills planted in the slots or the shared list; the
chapter at 7 / 8 / 9 / negative for the portrait; the inventory with gaps;
the fake COM objects and the instance.

**The disturbance** (the group's case, from its hash only): `0x905B84`
repointed (the runs re-read it), the current record's x / y, the window
colour, the handed record's `+0xB`, `+8`, `+9`, `+0xC`, x / y, window 1's
`+0xD`, a record's `+0xB` or `+0x1F`, an item id of the list, a byte of the
second party list. Never `+0xA` (section 7).

**Result** (this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=rest_2h`, exit
0): 144,000 rounds, 1,959,489 calls to the stand-ins, **0 mismatches** (the first run, before the last seed changes, passed too).
Every step-table handler reached (842..11,333 calls); the thinnest callees
`Flags_Test` 795, `MasterWin_SkillKnown` 2,118, `_stricmp` 2,642.

**Under `'*'`** (this worktree, after the rebinding): exit 0, `inject: 9025
ours, 0 left original`, 1,026 self-test lines of 0 mismatches and none other
(among them `rest_2h` and DI's `menu_draw_helpers`, whose table expectations
were rebound, and BE7's `battle_e7`, whose `kDrawBar` was); the same with
`BOF3X_WIDE=1`: exit 0, 1,026, 9,025 ours, the log's bound -173. Each passed
on its first run. `tools/ledger_check.py`: 72 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **One start no list had**, `0x59C810` (band_rows: "code no list has" in
  `0x59C780`'s span; the catalogue's `0x59C780` 0xE8 ran over it): a function
  of its own with its frame and `ret`, called by `MasterWin_Available`.
  Taken.
- **One start taken from outside the cut**: `0x59E160`, catalogued "1 Library
  layer / Renderer" by its address range, is a menu draw (two boxes, two
  verbs of `Menu_Verbs`, the selection) called only from this group's
  `0x59CCE0`. It is game code in no group; taken as `Menu_DrawVerbPair`.
  Its neighbours `0x59E4F0`, `0x59E930`, ... carry the same range label and
  were not read (section 8).
- **Extents**: band_rows' read sizes are the code's; the cut's are the
  catalogue's with padding, except `0x59C780` (cut 232, read 0x8C: the
  catalogue ran over `0x59C810`). The hidden starts are entries by address
  (table cells; `0x5A9620` a pushed callback). **No case, no shared tail,
  nothing dropped.** `0x59C780` and `0x59DBF0` carry jump tables inside their
  extents (`JumpTable`s / relocated).
- **The labels**: "Table Window_Handler7KindTable / 8KindTable /
  MenuList_Kinds" were right for the runs; the "BattleMenuWin_ItemListSteps"
  row `0x59CB90` is that table's step 3; "Boot: party state, items and saves"
  (`0x59BE70`) is a slide; the `hypothesis` rows `0x59DB70` (battle_e7) and
  `0x5A9860` (Windows shell) are the stat bar and `Cfg_Load`'s key copy;
  `0x5A9620` ("pad_read") is the joystick callback.

## 6. Controls

`r2h/controls.py` (scratch): each plant replaces a string that occurs once in
`rest_2h.cpp`, rebuilds, runs the self-test on the clones whose name contains
the filter, restores and rebuilds. **84 planted, 83 refused** (every one exit 3, by a count of mismatching
rounds), every function at least one. **Not refused: C40** (the masters'
list's empty entry ending the rows instead of being skipped) - an
equivalent mutant: the list is built compact (entries, then zeros), and its
rows never pass its 17 bytes (ours aborts there), so after the first empty
entry every row is empty either way; its near variant C84 (entry 1 taken
for empty) is refused. **The first run** (before the last two seed changes)
left three more unrefused: C11 (the gene list's empty entry skipped - the
same compactness, until the seed put the gene list's top past 9 with all
eighteen set and bytes past the list, so the rows read past it), C69 (the
lit item row's second draw not re-reading its id - refused once the
disturbance moved the lit row's id and count for the item list), and
C83 / C84 added then. **The thinnest**: C69 (2 rounds), C83 (3), C45 (3:
one byte of the shared list's 128th), C19 and C80 (10: the record pointer
moved during the step), C74 (43). The table is the last run of each.

| # | Function | Plant | Refused (rounds of 4,000) |
|---|---|---|--:|
| C01 | `MenuList_ReserveWinDraw` | dim on +0xB bit 0 | 2,349 |
| C02 | `MenuList_ReserveWinDraw` | the box's flags 0x82 dim | 2,380 |
| C03 | `MenuList_ReserveWinDraw` | the status colour 2 undimmed | 1,855 |
| C04 | `MenuList_ReserveWinDraw` | HP colour 2 below 1 | 615 |
| C05 | `MenuList_ReserveWinDraw` | three rows at most | 1,294 |
| C06 | `MenuList_ReserveWinDraw` | AP's half for its quarter | 422 |
| C07 | `MenuList_ReserveWinDraw` | the frame 0x13 wide | 4,000 |
| C08 | `MenuList_ReserveWinDraw` | status A and B swapped on the frame bit | 1,387 |
| C09 | `MenuList_ReserveWinDraw` | the name 6 characters | 3,345 |
| C10 | `MenuList_GeneWinDraw` | 17 genes | 869 |
| C11 | `MenuList_GeneWinDraw` | an empty entry skipped, not the end | 1,404 |
| C12 | `MenuList_GeneWinDraw` | the lit row's message 0x4172 + | 1,405 |
| C13 | `MenuList_GeneWinDraw` | +0xC by +0xA | 2,729 |
| C14 | `MenuList_GeneWinDraw` | the scroll bar's total 0x11 | 4,000 |
| C15 | `MenuList_GeneWinDraw` | rows to top + moving + 8 | 938 |
| C16 | `MenuList_GeneWinDraw` | the icon three below | 3,520 |
| C17 | `MenuList_GeneWinRun` | the caption's steps | 1,375 |
| C18 | `ShopWin_SharedListSlideTo28` | the bound 0x29 | 217 |
| C19 | `ShopWin_SharedListRun` | the record read before the step | 10 |
| C20 | `ShopWin_SharedMemberRun` | draws the list | 4,000 |
| C21 | `ShopWin_MemberStatusRun` | highlight 1 | 4,000 |
| C22 | `ShopWin_DrawRowMenu` | the box 16 n + 0x18 high | 4,000 |
| C23 | `ShopWin_DrawRowMenu` | the +0xA row's second draw in colour 1 | 1,225 |
| C24 | `ShopWin_DrawRowMenu` | the row's text index one on | 4,000 |
| C25 | `ShopWin_DrawRowMenu` | the last pieces 8 higher | 4,000 |
| C26 | `ShopWin_MasterListRun` | draws the caption | 4,000 |
| C27 | `MasterWin_SlideOut` | the constant -120, the immediate ignored | 2,165 |
| C28 | `MasterWin_SlideTo64` | 0x10 a frame | 2,718 |
| C29 | `MasterWin_CaptionSlideTo8C` | the test 0x8D | 311 |
| C30 | `MasterWin_PupilsSlideDown` | x for y | 4,000 |
| C31 | `MasterWin_PupilsSlideUp` | the test 0x7F | 225 |
| C32 | `ShopWin_ItemCountRun` | x and y swapped | 3,892 |
| C33 | `ShopWin_ItemCountSlideToD2` | stores 0xD1 | 1,683 |
| C34 | `MasterWin_DrawList` | 16 masters | 2,015 |
| C35 | `MasterWin_DrawList` | +0xD 0xFE when none | 1,482 |
| C36 | `MasterWin_DrawList` | the picked colour 4 | 1,579 |
| C37 | `MasterWin_DrawList` | the name 0x111 + | 3,819 |
| C38 | `MasterWin_DrawList` | Available of the entry, not entry - 1 | 3,819 |
| C39 | `MasterWin_DrawList` | the title at + 0x3B | 4,000 |
| C40 | `MasterWin_DrawList` | an empty entry ends the rows | **not refused** |
| C41 | `MasterWin_Available` | flag 0x6D | 740 |
| C42 | `MasterWin_Available` | 0xD on bit 5 | 365 |
| C43 | `MasterWin_Available` | an empty list 0 | 284 |
| C44 | `MasterWin_SkillKnown` | six records | 171 |
| C45 | `MasterWin_SkillKnown` | 127 of the shared list | 3 |
| C46 | `MasterWin_DrawCaption` | message 0x101 + | 4,000 |
| C47 | `MasterWin_DrawPupils` | +0xB bit 1 | 2,558 |
| C48 | `MasterWin_DrawPupils` | a new row at 0x4A | 275 |
| C49 | `MasterWin_DrawPortrait` | index 4 to 0xA | 343 |
| C50 | `MasterWin_DrawPortrait` | from chapter 9 | 140 |
| C51 | `MasterWin_DrawPortrait` | shade 1 is 0x31 | 963 |
| C52 | `MasterWin_DrawPortrait` | the CLUT row 0x1DF | 4,000 |
| C53 | `BattleMenuWin_ItemListSlideLeft` | stores 0x53 (the 'fix') | 1,680 |
| C54 | `BattleMenuWin_EquipRun` | the member from the first party list | 3,097 |
| C55 | `BattleMenuWin_EquipRun` | flags from +0xC | 3,805 |
| C56 | `BattleMenuWin_EquipSlideTo62` | to 0x60 | 2,544 |
| C57 | `BattleMenuWin_EquipItemsRun` | the equip window's steps | 3,038 |
| C58 | `BattleMenuWin_EquipItemsSlideTo98` | the test 0x97 | 311 |
| C59 | `BattleMenuWin_VerbPairRun` | +0xA selected | 3,650 |
| C60 | `BattleEquipWin_DrawBar` | u and v swapped | 3,855 |
| C61 | `BattleEquipWin_DrawBar` | semi-transparency 1 | 4,000 |
| C62 | `BattleEquipWin_DrawBar` | x signed | 1,979 |
| C63 | `BattleEquipWin_DrawItems` | slot 4 the armour | 515 |
| C64 | `BattleEquipWin_DrawItems` | Item_CanUse mode 3 | 4,000 |
| C65 | `BattleEquipWin_DrawItems` | the armour's kind +9 + 2 | 1,756 |
| C66 | `BattleEquipWin_DrawItems` | the counts not cleared | 4,000 |
| C67 | `BattleEquipWin_DrawItems` | the lit row by r alone | 710 |
| C68 | `BattleEquipWin_DrawItems` | rows to moving + 8 | 1,767 |
| C69 | `BattleEquipWin_DrawItems` | the second draw's id not re-read | 2 |
| C70 | `BattleEquipWin_DrawItems` | the count one more | 4,000 |
| C71 | `BattleEquipWin_DrawItems` | the title by +9 | 3,490 |
| C72 | `Menu_DrawVerbPair` | the second verb dim on 0 | 1,616 |
| C73 | `Menu_DrawVerbPair` | the second button 0x2F on | 4,000 |
| C74 | `Menu_DrawVerbPair` | the colour not re-read | 43 |
| C75 | `DInput_EnumJoystick` | a failed device stops | 1,310 |
| C76 | `DInput_EnumJoystick` | found = 2 | 449 |
| C77 | `DInput_EnumJoystick` | the name at +0x128 | 2,690 |
| C78 | `Cfg_SetKeyTable` | 31 dwords | 4,000 |
| C79 | `ShopWin_PupilsRun` | the master list's steps | 2,677 |
| C80 | `ShopWin_MasterCaptionRun` | the record read before the step | 10 |
| C81 | `ShopWin_RowMenuRun` | the caption drawn | 4,000 |
| C82 | `MenuList_ReserveWinDraw` | the count read once | 339 |
| C83 | `BattleEquipWin_DrawItems` | the top read once for the lit row | 3 |
| C84 | `MasterWin_DrawList` | entry 1 skipped as empty | 86 |

## 7. Latent defects and ranges (Capcom's, described, not fixed)

- **The step tables are unbounded** (all ten runs: `call [T + 4 * +3]`); ours
  aborts past each table's count, where the original calls the next table's or
  data's word. No step store of the band's own goes past them (each slide
  writes 0); a step set elsewhere past a table is not shown to happen.
- **`MasterWin_DrawList`'s rows read its 17-byte stack list by `top + row`
  unbounded**: past it the original reads the rest of its own frame (three
  bytes of a local, then its return address and argument) as entries.
  `Menu_ListScroll` keeps `top + moving + 8` below 17 for a top of 8 or less
  (it answers the top less one while scrolling down, `menu_windows.cpp`), and
  the screen's own states move `+0xA` (not this group's). **Ours aborts** at a
  row past 16 with a message; reaching it would need a top of 9 or more. No
  ledger entry is needed unless the owner's play reaches it.
- **`MasterWin_Available` indexes `MasterWin_Requirements` by its argument
  unbounded** (17 pointers, then ASCII); its only caller passes 0..16. Ours
  aborts past 16.
- **`MenuList_GeneWinDraw` reads its 18-byte list past its end** when the top
  passes 9 (in place, `0x6BE0C2..`, the item list's bytes); with all eighteen
  genes set and a top past 9 the rows read whatever follows until a 0.
  Faithful (ours reads the same bytes).
- **`BattleEquipWin_DrawItems` reads the ids and counts by `top + r`** past
  their 128 bytes in place; faithful.
- **`BattleMenuWin_ItemListSlideLeft` tests 0x53 and stores 0x52** - the
  pair `BattleMenuWin_ItemListSlideRight` has (known-defects.md D88, DI's);
  an x landing exactly on 0x53 stays there a frame, then snaps to 0x52. Kept; control C53 plants the "fix" and is refused.
- **The reserve list's member ids and the portraits' index** are unbounded
  bytes into `0x66972C` / `0x66B470`, read in place (inside `.data`).

## 8. Calls across groups, inbound, and code in the band not taken

**Out of the group, raw**: none to a group of this round. `_stricmp`
`0x5C2B40` (the C runtime, part 0) by address; everything else is ours and
called by name.

**Inbound** (for the rebinding pass): `0x59AA30` (`MenuList_Kinds[19]`, R2G's)
calls `MenuList_ReserveWinDraw` by `E8`; `0x585DC0` (R2C's) and
`BattleEquipWin_Draw` (BE7's, ours) call `BattleEquipWin_DrawBar`; `Cfg_Load`
(Capcom's) calls `Cfg_SetKeyTable`; the original `DInput_Init` pushes
`DInput_EnumJoystick`. Through tables: `Window_Handler7KindTable` [11..18],
`Window_Handler8KindTable` [3..5], `MenuList_Kinds` [20],
`BattleMenuWin_ItemListSteps` [3], and the group's own step tables.

**Harness rows naming the group's addresses**: `boss_harness.cpp`'s standard
row `{"0x59DB70", 0x59DB70, 0x59DB70, 6, {kAll, kAll, kU8, kU8, kU16, kU8}}`
lists `BattleEquipWin_DrawBar` as Capcom's. It still registers (the key is
the address, inside the image) and BE7's `BH_AT(at::kDrawBar)` keeps reaching
it; its x / y masks are wider than the function reads (it reads their words:
`and eax, 0xFFFF`), which only makes the row stricter. For the coordinator's
fold: `kU16` for both, and `SH_OURS`-style once ours (not edited here).

**Code in the band in no group, not taken**: the catalogue's part-1
"Renderer" rows by range between `0x59E160` and `0x5A4C40` (`0x59E4F0`,
`0x59E930`, `0x59EE50`'s three hidden, `0x59F520`, ... 32 starts) and the
part-0 platform set-up rows `0x5A5BC0..0x5A62C0`; and the PSX-library,
sound and shell rows to `0x5A7C70`. `0x59E160` shows the range label can
hide game code; the others were not read (round fourteen leaves parts 0 and
1 to IDEAS.md I8 / I12).

## 9. The live route

`analysis/round14_cut.tsv`'s reach column marks six rows: the five hidden
starts in `Shop_DrawSellDetail`'s catalogue extent (`0x59BE50`, `0x59BE70`,
`0x59BEA0`, `0x59BEC0`, `0x59BF00`) and `0x5A9620` - each an upper bound (its
host was entered). `HANDOFF.md`'s trace of `campingFishing.txt` (2026-09-30,
plain `BOF3X_CALLTRACE`) entered `Window_Handler7KindTable`'s kinds "at
`0x59C110`.." in the camp: the masters' windows (kinds 15..17 and their
draws) and the camp's other kinds. The shop route (`shop.txt`) and the
combat route reach handler 8's kinds by DI's queue. **No trace was run by
this group**; the coordinator's route A/Bs and the state hash after the merge
are the live check: `campingFishing.txt` for the masters and the skill-note
windows, a battle's equipment window for `BattleEquipWin_DrawItems` /
`DrawBar` / the verb pair, the field menu for the gene and reserve lists.
`DInput_EnumJoystick` is reached only with `BOF3X_ORIGINAL=DInput_Init`.

## 10. The rebinding

`band_rows.py --refs --group R2H` and `grep -rn -i` of the 36 over
`src/game`: 32 raw references to 14 of them.

| File | Change |
|---|---|
| `battle_e7_callees.h` | `kDrawBar = bof3::addr::BattleEquipWin_DrawBar` (value unchanged; BE7's fuzz keys on it) |
| `menu_draw_helpers_fuzz.cpp` | the expected words of `Window_Handler7KindTable` [11..18], `Window_Handler8KindTable` [3..5] and `BattleMenuWin_ItemListSteps` [3] by name (values unchanged) |

**Left raw, on purpose**: `boss_harness.cpp`'s row (a harness: not this
group's to edit; section 8); `battle_e7_fuzz.cpp`'s `CallSite` table and its
`"0x59DB70"` row (the disassembly's target; the row keys on the constant);
`menu_frame.cpp`'s `kReserveListObjSite` `0x59AA98` and `battle_draw.cpp`'s
`kTitleCallB` `0x59DEFA` (call sites, not functions); comments in
`field_s.cpp`, `field_o.cpp`, `menu_frame.cpp`, `menu_draw_helpers.cpp`,
`battle_e7.cpp`. No file of another group of this round refers to the 36.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-04) for the 24 that had no
line: `0059ADE0 20`, `0059BE50 20`, `0059BE70 28`, `0059BEA0 20`,
`0059BEC0 32`, `0059BF00 D`, `0059C110 20`, `0059C130 28`, `0059C160 28`,
`0059C190 20`, `0059C1B0 28`, `0059C1E0 20`, `0059C200 28`, `0059C230 28`,
`0059C260 2A`, `0059C290 28`, `0059C810 58`, `0059CB90 26`, `0059CC10 47`,
`0059CC60 28`, `0059CC90 20`, `0059CCB0 28`, `0059CCE0 1C`, `005A9620 64`.
**Left as they were** (twelve existing lines at the group's own entries):
`0059C2C0 4BE`, `0059C870 73`, `0059C8F0 10D`, `0059DB70 76`, `0059E160 CB`,
`005A9860 15` agree with the read; **longer than the read, covering a
function of the group's** (for the coordinator's split): `0059AA80 380` (read
0x35D; covers `0x59ADE0`), `0059AE00 420` (0x415), `0059BF10 3A8` (0x1FE;
covers `0x59C110..0x59C290`), `0059C780 E8` (0x8C; covers `0x59C810`),
`0059CA00 100` (0xF8), `0059DBF0 56D` (0x568); and the host `005A94C0 1C4`
(`DInput_Init`; covers `0x5A9620`). `0059BBC0 282` (`Shop_DrawSellDetail`)
ends at `0x59BE42`, before the five starts the cut calls hidden in it.

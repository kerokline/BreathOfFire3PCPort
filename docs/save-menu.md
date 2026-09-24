# Save, load, the inn and the sound stream (group X)

**Status:** IN PROGRESS (2026-09-23 - group X of the sixth parallel round:
44 functions ours in [`src/game/save_menu.cpp`](../src/game/save_menu.cpp),
faithful, fuzzed against Capcom's at start-up (`BOF3X_SHADOW=save_menu`,
66,000 rounds, 0 mismatches) and controlled headless (63 controls, 61
refused by comparison, 2 changes that change nothing); no divergence, one
candidate defect (D38). Through the batch `ab26` - the shop A/B 35 of 35
([`takeover-queue-round6.md`](takeover-queue-round6.md) status))

The sixth round's queue ([`takeover-queue-round6.md`](takeover-queue-round6.md))
gave group X 23 functions the owner's shop route reaches: the save files,
the inn, the save menu's pieces, the shop's price helpers, the title menu's
draws and the sound stream. It also asked where the code after
`Snd_LoadBank` `0x587CD0` belongs - group S found that function `0xDE` bytes
long ([`display-env.md`](display-env.md)), and the catalogue had given it
everything to `0x588E6F`. **That code is task 0's mode 1, the title and load
flow: 25 functions, 21 of them in no list** - reached only through three jump
tables in `.data`. They are clearly this cluster's (the load menu), so this
group took them too.

Read with capstone against `bof3/BOF3.exe`, each function to its last
instruction, PSX twins from `analysis/pairs_propagated.json` where it has one
(overlays `62002f85` and `b8cc1561`, and the boot EXE), read side by side.

## 1. What they are

Entries and sizes (the extents read; for `analysis/calltrace/entries_logic.txt`):

| Entry | Size | Name | Shop route calls | PSX twin |
|---|--:|---|--:|---|
| `0x454770` | `0x9A` | `Snd_LoadBankFile` | 2 | - |
| `0x454820` | `0x45` | `Save_ReadFile` | 15 | - |
| `0x4548B0` | `0xA1` | `Save_ListFiles` | 2 | - |
| `0x4549F0` | `0x2E` | `Gfx_ClutStripCopyRow` | 6 | `0x8014E294` |
| `0x580630` | `0xB1` | `Party_RestoreAll` | 1 | `0x801D68E0` |
| `0x5808E0` | `0x88` | `SaveMenu_DrawChoices` | 134 | `0x801D6D7C` |
| `0x580970` | `0x43` | `Menu_DrawBlackScreen` | 207 | `0x801DF800` |
| `0x583020` | `0xA3` | `Shop_PriceRate` | 2 | `0x801D3570` |
| `0x5830D0` | `0x23` | `Shop_ScalePrice` | 8,007 | `0x801D3664` |
| `0x583100` | `0x40` | `Shop_SellPrice` | 116 | - |
| `0x583140` | `0xCB` | `Shop_InitWindows` | 2 | - |
| `0x583210` | `0x138` | `Shop_Equip` (code `0x122`, then its 5-entry jump table) | 1 | - |
| `0x588880` | `0x4D` | `TitleMenu_DrawRows` | 39 | - |
| `0x5888D0` | `0x1E2` | `TitleMenu_DrawRow` | 117 | - |
| `0x588AC0` | `0xDE` | `NewGame_RollGrowth` | 8 | `0x801E7190` |
| `0x588BA0` | `0x56` | `NewGame_RollSum` | 40 | `0x801E70DC` |
| `0x588C00` | `0x83` | `TitleMenu_DrawFrame` | 43 | - |
| `0x588C90` | `0x8E` | `TitleMenu_DrawPiece` | 172 | - |
| `0x588D20` | `0x9D` | `SaveMenu_DrawSlots` | 190 | two (`0x801D67C0`, `0x801DDF6C`) |
| `0x588DC0` | `0xB0` | `Save_ReadSummaries` | 2 | - |
| `0x587910` | `0xE3` | `Sound_LoadStream` | 1 | (`0x80164090`, a CD stream) |
| `0x587A00` | `0x1B` | `Sound_StreamDone` | 56 | `0x80164890` |
| `0x5A7020` | `0x2A` | `Music_IsPlaying` | 56 | - |
| `0x587DB0` | `0xF` | `TitleFlow_Step` | untraced | - |
| `0x587DC0` | `0xD4` | `TitleFlow_Begin` | untraced | - |
| `0x587EA0` | `0xE` | `TitleFlow_Menu` | untraced | - |
| `0x587EB0` | `0x19` | `TitleMenu_Open` | untraced | - |
| `0x587ED0` | `0x1A` | `TitleMenu_OpenWait` | untraced | - |
| `0x587EF0` | `0x19` | `TitleMenu_FrameUp` | untraced | - |
| `0x587F10` | `0x28` | `TitleMenu_RowsStart` | untraced | - |
| `0x587F40` | `0x59` | `TitleMenu_RowsFadeIn` | untraced | - |
| `0x587FA0` | `0xF6` | `TitleMenu_Choose` | untraced | - |
| `0x5880A0` | `0x31` | `TitleMenu_Leave` | untraced | - |
| `0x5880E0` | `0x48` | `TitleFlow_NewGame` | untraced | - |
| `0x588130` | `0xE` | `TitleFlow_Load` | untraced | - |
| `0x588140` | `0x4A` | `LoadMenu_List` | untraced | - |
| `0x588190` | `0x5C` | `LoadMenu_Open` | untraced | - |
| `0x5881F0` | `0x149` | `LoadMenu_Choose` | untraced | - |
| `0x588340` | `0x72` | `LoadMenu_Confirm` | untraced | - |
| `0x5883C0` | `0xD7` | `LoadMenu_Read` | untraced | - |
| `0x5884A0` | `0xB3` | `LoadMenu_Error` | untraced | - |
| `0x588560` | `0x66` | `LoadMenu_Loaded` | untraced | - |
| `0x5885D0` | `0x222` | `LoadMenu_Apply` | untraced | - |
| `0x588800` | `0x79` | `TitleFlow_EnterGame` | untraced | `0x801E6228` |

"Untraced": no entry list held them, so the all-calls trace has no count.
That the route reaches them is by its callees' callers in
`recipe_shop/bof3x.callcounts.tsv`: `TitleMenu_DrawRows` from `0x587F28`,
`0x587F4C`, `0x587FAD`, `0x5880CF` (the title menu's states 3, 4, 5 and 6),
`Save_ListFiles` from `0x587E70` (`TitleFlow_Begin`) and `0x58815B`
(`LoadMenu_List`), `Save_ReadFile` 14 times from `0x588E41` (the summaries)
and once from `0x588423` (`LoadMenu_Read`), `Save_ReadSummaries` from
`0x588164` and from the in-game save menu's `0x57FDE5`,
`NewGame_RollSum` 40 times (8 characters x 5 from `TitleFlow_EnterGame`).
The route loads save 3 from the title, so it runs the title menu and the
load menu's whole path; not `TitleFlow_NewGame` and not `LoadMenu_Error`.

Every size matches `pe_funcs.py`'s except `Shop_Equip` (it said `0x150`, into
the padding after the table). No listed function turned out to be a case
label.

### Where the code after Snd_LoadBank goes

Task 0's body `0x588E88` calls `[0x667294 + 4 Game_Mode]`; mode 1 is
`TitleFlow_Step` `0x587DB0`, `jmp [0x6671F4 + 4 Game_Step]` over six steps:

| Step | Function |
|--:|---|
| 0 | `TitleFlow_Begin` - the field made ready for the backdrop, `Save_ListFiles`, the cursor on LOAD GAME when saves exist |
| 1 | `TitleFlow_Menu` - `jmp [0x66720C + 4 state]`, the title menu's states 0..6 (`0x587EB0`..`0x5880A0`) |
| 2 | `TitleFlow_NewGame` |
| 3 | `TitleFlow_Load` - `jmp [0x667228 + 4 state]`, the load menu's states 0..7 (`0x588140`..`0x5885D0`) |
| 4 | `0x460CB0`, the config screen - not ours, not this group's |
| 5 | `TitleFlow_EnterGame` - task 0 restarted at `0x495800`, the game's body |

The state byte is `0x6BDF86` for both menus; `0x667228` is `0x66720C`'s
eighth entry, so the table of fifteen holds both. Nothing checks either index.

### The data

- `0x6BDF84..0x6BDF96`: the error message byte, the cursor (s8), the state,
  "something to load" (`0x6BDF8E`, three title rows instead of two), the row
  brightnesses `0x6BDF90[3]`, the apply countdown `0x6BDF94`, "Start was held"
  `0x6BDF96` (the default pad map).
- `0x929ED4` the load error, 1 no save, 2 a failed read or checksum;
  `0x929F04` the menus' frame counter; `0x929F0B` Menu_YesNo's answer.
- `0x9036D4` the save slot under the cursor, `0x8034D0` the first of the three
  slots shown; `0x905BC0` the sixteen `0x1C`-byte summaries, `+0x15` the
  directory index or `0xFF`.
- The game block `0x9039E0` (`0x10B0` bytes, the PSX layout of
  [`save-interchange.md`](save-interchange.md)), its pad map at `+0xCBC`
  (`0x90469C`), copied to the live map `0x903580..0x903591` on a load.
- `0x6BDE40` the last stream's kind (`id >> 12`), `0x6BDE44` its file image.

## 2. How each works, quirks kept

The code comments hold each function's full reading; what matters beyond
the obvious:

- **`Save_ListFiles`** keeps both defects of [`save-files.md`](save-files.md)
  §2: no `_findclose`, and no bound - a seventeenth match writes on past
  `Save_Directory` towards `Save_Staging`. DIV-0002's `Save_WriteFile` (ours
  since 2026-09-19) now calls ours.
- **`Save_ReadSummaries`** does not check `Save_ReadFile`'s answer: a file that
  will not open leaves the summary whatever `Save_Staging` held.
- **`Snd_LoadBankFile`** is `LoadDatFile`'s chunk walk that hands on only kind
  2, the sound banks.
- **`Sound_LoadStream`** reads its id as a **dword**: `0x446E8B` pushes
  `(ecx & 0xFFFF) + 0x1000`, so a kind of 16 is possible. The `symbols.toml`
  signature was `unsigned short`, which would have dropped that bit; it is
  `unsigned` now (and `move_groups.cpp`'s pointer and stand-in with it). Kind 0
  plays through `Music_Start` at volume 127 once, dropping a fade in progress
  (`Music_FadeCount` 0, `Music_Stop`, `Music_Track` `0xFF`); any other kind goes
  to `0x5A7140`, a one-shot WAV player that is not taken.
- **Two uninitialised locals, kept.** `Music_IsPlaying` tests the low byte of
  the slot its entry's `push ecx` made, which `GetStatus` fills - unless it
  fails. `LoadMenu_Error` takes its targets for an error other than 1 or 2
  from the high byte of the same kind of slot. Both are ours as **naked
  entries** that pass the caller's `ecx` to the body, so the value tested is
  the one the original would test. For `LoadMenu_Error` that `ecx` is now left
  by our `TitleFlow_Load`, not Capcom's; only unstored error values read it.
- **Register leftovers in arguments.** `SaveMenu_DrawChoices` builds its row
  index with `movzx si, bl` over `x`, so each text line's `y` carries
  `20 * (x & 0xFFFF0000)`, and the hand's row with `movzx ax, byte` over the
  last `Text_DrawAt` result's upper half; `SaveMenu_DrawSlots` passes the
  first slot's dword with only its low byte advanced. All kept (all vanish in
  a 16-bit `y`, which is what the PSX twin passes). Where the original pushes a
  byte in a register it never cleared (the window style and colour bytes, the
  party set, `Sprite_SetAnimation`'s byte, a growth roll's bytes), ours
  zero-extends; each callee reads the byte (`0x575690` masks at `0x575718`;
  the rest by their own `symbols.toml` entries or group Y's reading).
- **`TitleMenu_DrawRow` and DIV-0014.** The row widths are three `mov byte
  [esp + 0x1x], w` immediates at `0x5888E4`..`0x5888EE`, which DIV-0014
  ([`title-menu.md`](title-menu.md)) patches for the English sheet. Ours reads
  them from the image at `0x5888E8`, `0x5888ED`, `0x5888F2`, so the patch keeps
  its effect with the code ours and `BOF3X_ORIGINAL=TitleMenu_Widths` still
  switches it off. A row index of 3 or more, which reads past the three bytes
  into the stack in the original and which nothing passes, aborts in ours.
- **`NewGame_RollGrowth`** writes four mapped roll sums into `0x903640 + 5 m`
  and then clears all five bytes: only its `Rand` calls survive. The PSX twin
  does exactly the same. `TitleFlow_EnterGame` runs it for all eight
  characters **after a load too**, so loading a save consumes 40 roll sums of
  `Rand`.
- **`TitleFlow_NewGame`** clears the first dword of each 8-byte record of
  `Cond_Flags` (`0x903F90..0x90402F`) - `0x929ED0` is `Cond_Flags + 8 n` too.
- **`LoadMenu_Read`** zeroes `0x1C00` bytes of `Save_Staging`, reads the whole
  file, zeroes the checksum word, and **copies the block into the live game
  block while summing it** - before it knows the sum is right. See D38.
- **The dispatchers jump.** `TitleFlow_Step`, `TitleFlow_Menu` and
  `TitleFlow_Load` end in `[[clang::musttail]]`: the built object has
  `jmpl *0x6671f4(,%eax,4)` and the two like it, the original's own
  instruction, so a step function still returns to task 0's loop and a
  step that is Capcom's (`0x460CB0`) still sees task 0's return address.
  Every other function has `disable_tail_calls` (a tail jump into Capcom's
  code would show in the frame hash, HANDOFF Traps).
- **`LoadMenu_Apply`** is a coroutine step: three `File_LoadDone` /
  `Task_Sleep(1)` waits, which never loop on the PC (`File_LoadDone` is 1).
  `TitleFlow_EnterGame` ends in `0x5A9976`, which restarts task 0 and does
  not return.

## 3. The fuzz

`save_menu_fuzz.cpp`: each of the 44 alone, 1,500 rounds each, under control
words `0x027F`, `0x007F` and `0x037F` in turn. Every relative call out of a
copy goes to a recording stand-in, and ours to the same through
`save_menu::g`, the calls between this module's own functions included. The
three dispatch tables in `.data` hold recorders during the test;
`Shop_Equip`'s in-body table and its `jmp` operand are moved onto the copy.
`Music_IsPlaying`'s buffer is a fake COM object whose `GetStatus` fails a third
of the time without writing; `Music_IsPlaying` and `LoadMenu_Error` are
called with a chosen `ecx`. Nothing reaches a file or the CRT: `File_*`,
`_findfirst` / `_findnext`, `Crt_sprintf`, `Crt_malloc` / `Crt_free` and
`Rand` are stand-ins.

Stand-ins answer from a hash of the round and the call's index and give their
callers what those read afterwards: `Gfx_CommitPrim` and the piece draw move
`Gfx_PacketNext`; the recalculation writes the maxima the inn copies; the
inventory removal rewrites the slot byte `Shop_Equip` reads back;
`Field_MemberSprite` moves `Sprite_Current` and `Field_State`;
`Save_ReadFile` fills `Save_Staging` with a right checksum half the time; a
sound, a transition, a slot draw, `Menu_YesNo` or the title frame may move
the state byte, the answer, the slot or the first slot; `PartySet_Select`
rewrites the party set byte; `File_LoadDone` answers 0 a few times first.
Arguments the original pushes with leftover upper bits are logged as the
bytes the callees read.

Seeded boundaries: the cursor at -1, 0, 2, 3, `0x7F`, `0x80`; brightnesses
around `0x40` and `0x80`; the counter at 0, 1, `0xF0`; slots 0, 1, 14, 15, 16,
`0x100`, -1 and the first slot two below; hex digits and their neighbours
(`/ : @ G ` ` g`) at a directory name's byte 7; items `0x2A`, `0x2B`, `0x36`,
`0x37`; rates 0, 1, 100, `0xFFFF`; prices around multiples of 100; window
bytes 5..8; the inn byte `0x1D..0x1F`; stream kinds up to 19; file sizes
negative, tiny and large, with a well-formed chunk list; up to 20 directory
matches with names up to 39 bytes.

Compared per round: every region the function touches, byte for byte (the
game block, `Save_Staging` and `Save_Directory`, the summaries, the menus'
bytes, the pad map, the CLUT strip and source, the shop's window records,
`Field_Members`' bytes, the packet buffer), the log of calls, and the result
where there is one (`al` only for `NewGame_RollSum`).

Result: **66,000 rounds, 0 mismatches**, alone and with `BOF3X_SHADOW='*'`
(627 functions ours).

## 4. Negative controls

Sixty-three, planted one at a time behind a temporary switch and all removed
(the committed `save_menu.cpp` has none):

| # | Change | Refused in |
|--:|---|---|
| 1 | bank chunks of kind 3, not 2 | 769 / 1,500 |
| 2 | `Save_ReadFile` answers 1 | 1,206 |
| 3 | the name copied without its NUL | 268 |
| 4 | the CLUT dirty flag 2 | 1,500 |
| 5 | the inn byte cleared below `0x1F` | 66 |
| 6 | the text rows without `x`'s upper half | 495 |
| 7 | the hand's row without the leftover | 1,499 |
| 8 | the black tile 239 tall | 1,500 |
| 9 | rate 70 for window 6 or 8 | 298 |
| 10 | the price divided by 101 | 883 |
| 11 | the times-50 items end at `0x35` | 97 |
| 12 | slot 5 of kind 2 | 204 |
| 13 | row 2 at `0x91` | 991 |
| 14 | the glow unbrightened | 760 |
| 15 | four rolls, not five | 1,500 |
| 16 | the roll's modulo unsigned | 661 |
| 17 | a piece's v from `+3` | 1,054 |
| 18 | the slot cursor never lit | 1,020 |
| 19 | the slot argument without its upper bytes | 346 (after seeding a first slot up to `0xFFF` and just below 0: 0 before) |
| 20 | `g` parsed as a hex digit (16) | **not refused - changes nothing**: 16 is never a slot |
| 61 | `` ` `` parsed as a hex digit (9) | 654 |
| 21 | `Music_Track` `0xFE` | 160 |
| 22 | the stream id read as a word | 105 (after seeding kinds up to 19: 0 before) |
| 23 | the voice answer not flipped | 731 |
| 24 | `GetStatus`'s slot starts 0, not `ecx` | 179 |
| 25 | the step table one on | 1,500 |
| 26 | the cursor always 0 | 993 |
| 27 | `Sprite_Current` not read again | 381 |
| 28 | the rows held at `0x41` | 430 |
| 29 | the down wrap unsigned | 77 |
| 30 | the lit row clamped above `0x7F`, not `0x80` | **not refused - changes nothing**: a row at `0x80` is set to `0x80` |
| 31 | `Game_Step` = cursor + 3 | 801 |
| 32 | `Cond_Flags` cleared every 4 bytes | 1,500 |
| 33 | no saves: state 6 | 500 |
| 34 | the slot compared whole, not its low byte | 115 |
| 35 | the first slot's follow unsigned | 62 |
| 36 | the checksum compared as dwords | 756 |
| 37 | `Field_ScriptFlags` only on a good sum | 358 |
| 38 | the error's local from the wrong byte of `ecx` | 869 |
| 39 | no default pad map | 324 |
| 40 | the `Cond_Flags` index unsigned | 311 |
| 41 | back to the title on row 1 | 286 |
| 42 | 31 frames before applying | 1,500 |
| 43 | the open wait ends at 1 | 754 |
| 44 | two brightness bytes cleared, not four | 1,500 |
| 45 | "no" keeps the state | 32 |
| 46 | one window word `0x67` | 1,500 |
| 47 | the old slot byte read before the removal | 742 (after the stand-in rewrote that byte: 4 before) |
| 48 | HP copied before the recalculation | 1,500 |
| 49 | the title menu table one back | 1,500 |
| 50 | the load menu table one back | 1,500 |
| 51 | no sound on an empty slot | 392 |
| 52 | the state read before the transition | 97 |
| 53 | the loaded game applied at countdown 1 | 918 |
| 54 | the piece's CLUT shifted by 5 | 1,440 |
| 55 | `y` not narrowed to s16 | 593 |
| 56 | the second flag not tested | 991 |
| 57 | the first slot follows past `+ 3` | 38 |
| 58 | no chunk walk for files of `0x20` bytes or less | 60 (after seeding tiny files: 1 before) |
| 59 | the old stream not freed | 783 |
| 62 | the draw mode's page not masked to 16 bits | 1,500 |
| 64 | `Save_ListFiles` bounded at 16 | 291 |

Every behaviour-changing control failed by comparison; two refused by a
fault in their first form (49 and 50 stepped past the recorder tables) were
rewritten to stay inside and then refused by count. Four needed better
seeding first (19, 22, 47, 58), recorded above.

## 5. What none of this reached

- **Live.** Nothing here has run in the game yet; the shop A/B
  (`analysis/validate_shop.sh`) after the merge reaches every function but
  `TitleFlow_NewGame` and `LoadMenu_Error`. The attract sequence reaches
  none of the 23 listed (the queue is the route less what attract reaches),
  and it presses nothing, so it never leaves `Title_Task` for the title menu
  task `0x588E70` whose mode 1 the other 21 are (`Title_CheckStart` wants
  Start).
  **`entries_logic.txt` needs the 21 new entries** (table above) before a
  frame-hash run - they were never in it.
- Fuzz only: `TitleFlow_NewGame`, `LoadMenu_Error` (and its uninitialised
  path), `Sound_LoadStream`'s kinds above 0 (`0x5A7140`) and above 15,
  `Save_ListFiles` past sixteen files, `Music_IsPlaying` with a failing
  `GetStatus`, `TitleMenu_DrawRow` beyond row 2 (ours aborts; nothing calls it
  so).
- **Real files.** No self-test opens one; `Save_ReadFile`, `Save_ListFiles`,
  `Snd_LoadBankFile` and `Sound_LoadStream` meet the disk first in the A/B.
- The call order into `Gpu_GetTPage` before `Gfx_PacketNext` is read (ours
  sequences it explicitly) is not observable to the fuzz: no stand-in of
  `GetTPage` moves the packet, as the real one does not.
- Not taken, next to these: `0x5806F0` (the save block builder, not reached by
  the route - it saved nothing) and `0x5809C0` (the window procedure's save,
  which writes a file); `0x5A7140` / `0x5A7200` (the one-shot stream).

## 6. Facts about other groups' functions

- `0x575690` (group Y) masks its argument to a byte (`and eax, 0xFF` at
  `0x575718`). `0x574AB0`'s fifth argument and `0x57CF60`'s sixth are bytes
  the original pushes in registers with leftover upper bits (`0x5881A1`,
  `0x5808F1`); Y should confirm they read only the byte.
- `0x5749F0` (Y) returns a price its callers mask to 16 bits.
- `0x576960` (called by `SaveMenu_DrawSlots`, 190 calls in the route) and
  `0x591B60` (the inventory removal `Shop_Equip` calls, `(kind, item, 1, 0)`)
  are real entries - `E8` targets - that no catalogue row lists: each is folded
  into a neighbour's extent. `Inventory_Add` (W) is called with a fourth dword
  it does not read and an item byte in `cl` over leftover bits.
- `0x5918E0` (W) is `Shop_PriceRate`'s shop-kind test, called with 1; PSX
  `0x80166AD0`.
- `Snd_LoadBank` ends at `0x587DAD`; `0x587DB0` is `TitleFlow_Step`.

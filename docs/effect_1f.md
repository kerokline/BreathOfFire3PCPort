# Group E1F: a menu on effect record 6, a sprite pass, game mode 8's steps, the UI sprite helpers

**Status:** MEASURED (2026-09-29) - round thirteen, wave one
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9 and 10),
on the round branch's tip `1bb41df`. **15 functions ours**
(`src/game/effect_1f.cpp`, the helpers' declarations in
`src/game/effect_1f.h`, shadow name `effect_1f`): the cut's fifteen rows for
E1F (`analysis/round13_cut.tsv`), each read to its last instruction with
capstone and fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8), used unchanged:
120,000 rounds, 0 mismatches (in this worktree). 92 controls planted one at a time: 90 refused by a count, 1 refused only by a fault with a near variant refused by a count, 1 equivalent mutant with a near variant refused (section 6a). Fuzz only: no
recorded route enters any of them (section 9). No divergence.

| Function | Entry | Bytes | Reached by | PSX twin |
|---|---|--:|---|---|
| `ChoiceMenu_Run` | `0x52A6C0` | 0xE | cell `0x6602EC` (entry 3 of E1E's `0x52A420` table `0x6602E0`) | - |
| `ExtraSlots_Step` | `0x52A6D0` | 0x12 | `ChoiceMenu_Choices[0]` | - |
| `ExtraSlots_Enter` | `0x52A6F0` | 0x8C | `ExtraSlots_Steps[0]` | - |
| `ExtraSlots_PickSlot` | `0x52A780` | 0x16F | `ExtraSlots_Steps[1]` | - |
| `ExtraSlots_PickItem` | `0x52A8F0` | 0x4A4 | `ExtraSlots_Steps[2]` | `0x801DD050` (call-disputed) |
| `Sprite_UpdateScreenScaled` | `0x52CD50` | 0x9A | calls from `Sprite_UpdateAllScaled`, E1A's `0x4656C0`; a tail `jmp` from E1A's `0x465840` | `0x801E08A4` |
| `Sprite_UpdateAllScaled` | `0x52CDF0` | 0x26 | calls from `GameMode8_Frame`, `GameMode8_WaitFrame` | `0x801E0A0C` |
| `Effect_ResetFirstSeven` | `0x52CE20` | 0x3C | a call from E1E's `0x52A370` | `0x801E0A88` |
| `FieldPanel_KindPoints` | `0x52CE60` | 0x63 | calls from `FieldPanel_KindTotal`, FE1's `FieldPanel_DrawKindRow` | `0x801E0B14` |
| `FieldPanel_KindTotal` | `0x52CED0` | 0x2C | a call from FE1's `FieldPanel_DrawTotal` | `0x801E0BC4` |
| `GameMode8_Frame` | `0x52CF00` | 0x23 | game mode 8's step table `0x656AB8` entry 1; a call from its entry 0 `0x496440` | `0x801E0C28` |
| `GameMode8_TradeStep` | `0x52CF30` | 0x5 | `0x656AB8` entry 3 | `0x801E0C78` |
| `GameMode8_WaitFrame` | `0x52CF40` | 0x19 | two calls from mode 8's step 2 `0x4964E0` | `0x801E0C98` |
| `UiSprite_SetMode` | `0x52CF60` | 0x79 | 31 `E8` sites (effect states of E1A, E1B, E1G; FE1's panels) | `0x801E0CD8` |
| `UiSprite_Draw` | `0x52CFE0` | 0x9C | 100 `E8` sites (the same callers) | `0x801E0E24` |

The twins are the cut's (`analysis/pairs_propagated.json`); the sibling names
none of them (`names/*.toml`, `symbols.toml`: no entry at any of the eleven
addresses), so every name here is from what the PC code does. The words
"menu", "slot", "kind", "trade" name the code's shape, not a play-tested fact:
no Breath of Fire III gameplay fact is stated here from memory.

**Not all of this is effect code** (the brief's addendum asks for it to be
said): `GameMode8_Frame`, `_TradeStep` and `_WaitFrame` are a game mode's
steps; `FieldPanel_KindPoints` / `_KindTotal` are FE1's panel arithmetic;
`Sprite_UpdateAllScaled` walks the `Sprite_Objects` pool. They are part-5
rows the coordinator cut into this group, not `hypothesis` rows of the
labelling pass, and all fifteen are taken; the coordinator may move the
non-effect ones' docs and names to their own rounds' families without
touching the code (section 7 lists who reaches each).

## 1. What each function does

Every function's comment in `effect_1f.cpp` is the full read; the
`symbols.toml` evidence strings cite it. **Effect record 6** below is
`Effect_Objects` record 6, `0x7E14E0` (0x80 bytes): the menu keeps its state
there, whatever `Sprite_Current` is when it runs.

### The menu on effect record 6 (`0x52A6C0..0x52AD93`)

The chain above it is E1E's: `0x52A420` (entry 5 of the table at `0x660200`,
itself reached through a pointer at `0x64899C` in area data) jumps by
`Sprite_Current +3` through `0x6602E0` - entries `0x52A440`, `0x52A480`,
`0x52A4A0`, **`0x52A6C0`**, `0x52AF30`, five before `ChoiceMenu_Choices`
begins. `0x52A4A0` (E1E) steps and wraps record 6 `+6` between 0 and 2 and
draws three labels from `MessagePools`; confirming moves `Sprite_Current +3`
to 3, which is `ChoiceMenu_Run`.

- **`ChoiceMenu_Run` `0x52A6C0`**: `jmp [ChoiceMenu_Choices + 4 * record 6
  +6]` - `ExtraSlots_Step`, `0x52ADA0`, `0x52AE80` (the other two choices, in
  no cut). Unchecked; ours aborts past the three.
- **`ExtraSlots_Step` `0x52A6D0`**: `jmp [ExtraSlots_Steps + 4 *
  Sprite_Current +4]` - the three steps below. Unchecked; ours aborts past
  them. The cell `0x6602F4` is also what `Field_FormActions[13]` `0x522B40`
  (`jmp [0x65FEF4 + 4 * word +0x2C]`) reaches with a word of 0x100: whether
  anything sets that word to 0x100 is not measured.
- **`ExtraSlots_Enter` `0x52A6F0`**: when record 6 `+1` is 3 or 4 (the two
  slots; record 6 is an effect object whose state picks what it shows), its
  `+7` loses bit 7, the hand goes to `(0x1C, 0x58 + 16 * (+7 & 0x7F))` and
  `Sprite_Current +4` steps to 1. Then the slot's label: `Text_DrawAt(0x1D,
  0x14, 0, 0xFF, MessagePools + word)`, the word `0x803622` for `+7` bit 0
  clear, `0x803624` set.
- **`ExtraSlots_PickSlot` `0x52A780`**: `Input_AutoRepeat(Input_Pressed &
  0x5000)`; then `Input_Pressed` against `Field_CancelButtons` (sound 0x106;
  `+7 |= 0x80`, record 6 `+0x2C` = 0xFFFF, `Sprite_Current +3` down and `+4`
  = 0 - back to the choice), `Field_ConfirmButtons` (0x103, `+7 |= 2`, `+4`
  up), or the repeat's bit 12 or 14 (0x100, `+7 ^= 1`, record 6 `+1 ^= 7` -
  3 and 4 swap -, `+4` down, so `ExtraSlots_Enter` runs again). The label;
  unless `+7` bit 7, record 6 `+0x2C` = the slot's byte + 4 (`0x904130` for
  bit 0 clear, `0x90412E` for set; 0xFFFF when it is 0); the hand at `(0x1C,
  0x58 + 16 * bit 0)`.
- **`ExtraSlots_PickItem` `0x52A8F0`**: the list for the chosen slot, over
  category 3's inventory ids (`0x9042D4`, 128 bytes) filtered by the kind
  byte `+0x12` of each id's 24-byte `NameTable_Accessories` record. Cancel:
  0x106, `+7 &= 0xFD`, `Sprite_Current +4` down (back to `PickSlot`).
  - `+7` bit 0 clear, kind 0xB, the slot `0x904130` (char_stats.cpp's "extra
    accessory"): the label `0x803626`; confirm: the id at s16 `+0x36`, when
    of kind 0xB, `Inventory_Remove(3, id, 1)`, the slot's old id back
    (`Inventory_Add(3, old, 1)`, when not 0), the slot = id; then 0x103 and
    back as cancel. Else the repeat's bit 12 moves the cursor dword `+0xC`
    up (not below 0), bit 14 down (below `+0x18 - 1`), each with 0x100. The
    id at `+0x36`: kind 0xB -> `+0x2C` = id + 4, else 0xFFFF; the hand at
    `(0xA8, 13 * +0xC + 0x5A)`.
  - bit 0 set, kind 0xA, the slot `0x90412E` ("extra item", its count
    `0x90412F`): the label `0x803628`; nothing moves while record 6 `+0xA` is
    not 0 (a scroll animation's count, by the writes of 3 to it). Confirm:
    the id at s16 `+0x3A`, when of kind 0xA and not already the slot's,
    swapped in as above with `0x90412F` = 1; then the list's kind-0xA count
    n (a loop over all 128), and when `+0x1C` is at least n and the scroll
    word `+0x38` is past n - 9 and not 0, the scroll back by one; 0x103 and
    back. Else, with the cursor dword `+0x10` over nine rows: bit 12 up (at
    0, the scroll up with `+8` = 4, `+0xA` = 3), bit 14 down (below `+0x1C`;
    at 8, the scroll down while scroll + 8 is below `+0x1C`, with `+8` = 0,
    `+0xA` = 3); `Input_Pressed` bit 2 the scroll up by 9 (at the top, to 0
    and then the cursor to 0), bit 3 the scroll down by 9 while `+0x1C`
    passes 8 (held at `+0x1C - 8`; unmoved, the cursor to 8), else the cursor
    to `+0x1C`. Each move 0x100. The id at `+0x3A`: kind 0xA -> `+0x2C` = id
    + 4, else 0xFFFF; the hand at `(0xA8, 13 * +0x10 + 0x5A)`.

### The sprite pass (`0x52CD50`, `0x52CDF0`)

- **`Sprite_UpdateScreenScaled` `0x52CD50`**: `Sprite_UpdateScreen` with
  `+0x3C` held at 0 (the dword saved, zeroed, and put back on the record
  current after the call); then on the record current now, `+0x40` and
  `+0x44` = `0x4650000 idiv d` with the low byte cleared (`and al, 0`), d =
  `+0x60` when above 0, `+0x60 - 2 * s16 +0x3E` when below; both 0 at 0. The
  second is computed again from the cells, read through `Sprite_Current`
  again. eax: the quotient, or at 0 `Sprite_Current` as read after the call.
- **`Sprite_UpdateAllScaled` `0x52CDF0`**: the 30 `Sprite_Objects` records
  (0xA4 each, to `0x7E01B8`) whose `+0` is not 0, each made `Sprite_Current`
  and scaled; `Sprite_Current` is left at the last.

### The effect records' reset (`0x52CE20`)

- **`Effect_ResetFirstSeven`**: bytes `+1..+4` of records 0..6 to 0, record
  by record; `Effect_ReleaseAt(7)` .. `(19)`; record 2 `+1` = 2. E1E's
  `0x52A370` calls it when the menu's area opens.

### The kind points (`0x52CE60`, `0x52CED0`)

- **`FieldPanel_KindPoints(kind, count)`**: the 36-byte record `kind & 0xFF`
  at `0x66A6A0` (FE1 counted from `0x66A6AC`, the same fields at `+3` / `+6`):
  the count's low word at or above the threshold byte `+0xF` answers the
  points word `+0x12` - with the count's high word above it in eax, as the
  original loads the argument whole -; below it, `(count * 10 idiv
  threshold) * points / 10` (the last by the `0x66666667` multiply; every
  value is small and not negative, so it is the plain quotient). No divide
  by 0: below a threshold of 0 cannot happen.
- **`FieldPanel_KindTotal`**: the sum over the 32 count bytes `0x9040EC`
  that are not 0; `ax` the sum's low word.

### Game mode 8's steps (`0x52CF00`, `0x52CF30`, `0x52CF40`)

`GameMode_Handlers[8]` `0x496430` jumps by `Game_Step` through `0x656AB8`
(mode 7's `GameMode_ShopSteps` ends at `0x656AB8`): `0x496440`,
`GameMode8_Frame`, `0x4964E0`, `GameMode8_TradeStep`, `0x4965D0`, ... Step
0 loads (`0x52B480`, `0x495040(1)`, the area and the draw), calls
`GameMode8_Frame` once and steps on; step 2 raises the party set's bit 7,
loads `0x2C2 + set` and draws `GameMode8_WaitFrame` while two loads finish.

- **`GameMode8_Frame`**: `Field_MembersFrame`, `0x52B6C0` (catalogue part 7,
  in no cut), `AreaMap_Frame`, `Party_UpdateScreens`,
  `Sprite_UpdateAllScaled`, `Effect_RunObjects`, tail `Field_DrawFrame`.
- **`GameMode8_TradeStep`**: a tail `jmp 0x593950`, the dispatcher by the
  byte `0x93985C` of FE2's `ItemTrade_*` steps (EKH's section 8.9; in no cut).
- **`GameMode8_WaitFrame`**: `GameMode8_Frame` without its first two calls.

### The UI sprite helpers (`0x52CF60`, `0x52CFE0`)

- **`UiSprite_SetMode(index, slot)`**: the 16-byte record `index & 0xFF` of
  `UiSprite_Modes` `0x660394` (four dwords, libgpu `getTPage`'s colour mode,
  blend, page x and page y); `Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, tpage,
  0)` with tpage `((d0 & 3) << 7) | ((d4 & 3) << 5) | ((dC & 0x200) << 2) |
  ((dC & 0x100) >> 4) | ((d8 sar 6) & 0xF)`; `Gfx_CommitPrim(slot, 0xC)`.
- **`UiSprite_Draw(sprite, slot, x, y)`**: at `Gfx_PacketNext` (read once):
  `Gpu_SetSprt`; the colour 0x80 x 3; s16 x and y as floats at `+8` /
  `+0xC`; from the 16-byte record `sprite & 0xFF` of `UiSprite_Sheet`
  `0x660438`: the CLUT word `+0x16` = `(word +4 << 6) | ((dword +0 sar 4) &
  0x3F)`, the size words `+0x18` / `+0x1A` from `+8` / `+0xA`, u, v `+0x14` /
  `+0x15` from `+0xC` / `+0xD`; `Gfx_CommitPrim(slot, 0x1C)`. Answers the
  primitive (callers write through it: FE1's `FieldPanel_DrawKindIcon`).

## 2. Calling convention, arguments, answers

The state handlers, the dispatchers, the sprite pass, the reset and mode 8's
steps take nothing. The helpers are cdecl:

| Function | Reads of its arguments | Answer (compared) |
|---|---|---|
| `FieldPanel_KindPoints` | the kind's byte (`and eax, 0xFF`), the count's word (`cmp ax`, `and 0xFFFF`) - and its high word into the answer on one path | eax whole (`ret_mask` 0xFFFFFFFF) |
| `FieldPanel_KindTotal` | - | ax (0xFFFF: above it the last callee's eax) |
| `UiSprite_SetMode` | the index's byte; the slot whole (to `Gfx_CommitPrim`) | none (eax the commit's; no caller reads it) |
| `UiSprite_Draw` | the sprite's byte; the slot whole; x, y as s16 (`movsx`) | eax whole |
| `Sprite_UpdateScreenScaled` | - | eax whole (defined on every path; its callers drop it) |

**Pushed with leftovers**: `FieldPanel_KindTotal` pushes the count from
`movzx ax, al` (eax's high word left) and `ExtraSlots_PickItem` pushes the
item for `Inventory_Remove` / `Inventory_Add` as `ebx` / `eax` over a byte
load. Ours passes the byte; the fuzz compares those arguments at the width
the callees read (section 3). `Menu_DrawHand`'s y is built over eax's high
word in `ExtraSlots_Enter` and `_PickSlot`; the standard row compares its low
word, which is all `Menu_DrawHand` reads (FS's row).

## 3. The fuzz (`effect_1f_fuzz.cpp`)

The clone table is `band_rows.py --group E1F --clones --harness scenario`,
every extent confirmed by the reading (the tool's and the cut's agree but for
padding: section 5). Effect mode (`g.effect`); the state handlers and
dispatchers `kEffect`, the helpers `kCall`, mode 8's steps and the sprite
pass `kState`. 8,000 rounds a function. `BOF3X_E1F_ONLY=<name>` runs the
clones whose name contains it.

- **Tables**: `ChoiceMenu_Choices` (3) and `ExtraSlots_Steps` (3) as
  `DataTable`s - their entries recorders on both sides; every entry reached
  (about 2,000 each).
- **Callees re-listed**: `Sprite_UpdateScreenScaled`,
  `Sprite_UpdateAllScaled` (`kPhase`: their recorders log `Sprite_Current`);
  `FieldPanel_KindPoints` (byte, word); `Inventory_Remove` / `_Add` (three
  bytes, `kFlag`, and half the time a byte of the id list moved - the real
  ones rewrite it, and `PickItem` counts it after them); `Gfx_CommitPrim` (the
  primitive at the cursor noted as it stands when committed, then the cursor
  moved - a field written after the commit shows); `Sprite_UpdateScreen`
  (notes `Sprite_Current` and its `+0x3C` at the call); `Input_AutoRepeat`
  (a quarter garbage, else none, all or some of the bits handed - the "no
  repeat" paths run).
- **Region**: the id list `0x9042D4` (0x80). Effect mode's standard set
  holds the rest (records, sprites, input and buttons, `MessagePools`, the
  save block with `0x9040EC` and the slots).
- **Seeds**: record 6 `+6` below 3, `Sprite_Current +4` below 3 (the
  dispatchers); for the menu, record 6 `+1` 3 / 4 / others, `+7` on its bit
  boundaries, `+0xA` 0 two times in three, the dwords `+0xC`, `+0x10`,
  `+0x18`, `+0x1C` on 0, 1, 7..10, 16, 17, -1 and random, the scroll word on
  0, 1, 8..10, 0x7F, 0x8000, 0xFFFF, the s16 indexes mostly inside the 128
  and sometimes just past either end; the list from the ids whose kind is
  0xA or 0xB (read from the image once) or random; the slots empty, random or
  the id under the cursor; the cancel and confirm masks on two different
  bits two times in three, `Input_Pressed` on either, both, the page bits,
  the repeat bits or random. For the sprite pass, `Sprite_Current` half the
  time a sprite record, `+0x60` across 0, +-1, +-2, the dividend, the
  limits and `2 * s16 +0x3E` +-1, kept off a divisor of 0 (the seed and the
  `settle` after each disturbance). For the helpers, the kind below 32 two
  times in three and the count on the threshold, one either side, 0, 0xFFFF;
  the mode index below 10 and the sprite below 74 two times in three.
- **Disturbance** (the group's case, from its hash only): record 6's bytes
  and dwords, the scroll word and the indexes, `Input_Pressed`, a button
  mask, a slot byte, a list byte, a count byte, `Sprite_Current +3` / `+4`,
  `+0x3C` / `+0x60`.

**Result, in this worktree**: `BOF3X_SHADOW=effect_1f` headless, exit 0: **120,000 rounds over 15 functions (8,000 each), 571,167 calls to the stand-ins, 0 mismatches**; 24,884 bytes of state in 46 regions. Coverage: every table entry reached (2,589..2,745 each), `Inventory_Remove` 356, `Inventory_Add` 197, `Sprite_UpdateScreenScaled` 120,135 (as a recorder), `FieldPanel_KindPoints` 128,018, `Effect_ReleaseAt` 104,000. `BOF3X_SHADOW='*'`: exit 0, 680 self-test lines, every one 0 mismatches, `inject: 6910 ours, 0 left original`; the same with `BOF3X_WIDE=1`: exit 3 at `battle_e7` (`GeneWin_ListSlideOut` 503 rounds, 1,832 in all: its slide bound reads DIV-0041's widened operand; not this group's code, and the main checkout holds another session's uncommitted `battle_e7` / `widescreen.h` edits), before `effect_1f` runs - `BOF3X_SHADOW=effect_1f BOF3X_WIDE=1` alone: exit 0, the same 571,167 calls, 0 mismatches (none of the fifteen has a widened operand).

## 4. Divergence

None. Each function is a faithful replacement. Where the original jumps
through a state table unchecked (`ChoiceMenu_Run`, `ExtraSlots_Step`) ours
aborts past the table, and where it divides by 0 (`Sprite_UpdateScreenScaled`)
ours aborts - the round-nine precedent (the original faults or jumps wild at
the same point), no ledger entry. The unchecked **reads** of the image's data
tables are not aborted: they fault on neither side and ours reads the same
bytes in place (section 6).

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` reads all fifteen as the cut lists them; six
  differ from the catalogue's sizes by padding only (`0x52A6C0` 0xE of 16,
  `0x52A6D0` 0x12 of 32, `0x52A6F0` 0x8C of 144, `0x52A780` 0x16F of 368,
  `0x52A8F0` 0x4A4 of 1,200, `0x52CF00` 0x23 of 48). No start dropped, merged
  or added: no case, no shared tail, no code the cut does not list in the
  band. `0x52CF30` is hidden after `0x52CF00` (its host) but reached by its
  own cell: a function.
- **The cut's `unit_desc` column is wrong for most rows** and should not be
  carried into names: "Fn_522740", "Fn_522B40", "Fn_522B60
  (Field_ActionBySet[13])" were attributed through word-indexed dispatchers
  with an index of 0x100 or more (`0x65FEF4 + 0x400` is `0x6602F4`) - the real
  byte-indexed ones are E1E's `0x52A420` and this group's two dispatchers;
  "GameMode_Shop (GameMode_Handlers[7])" is mode **8**'s table `0x656AB8`,
  which lies just past `GameMode_ShopSteps` (`symbols.toml` already said
  so); "Fn_464E40; Fn_464F20 kind 0x3" and "Fn_466080 kind 0xF" are callers,
  not dispatchers. "No dispatcher found" for `0x52A8F0` and `0x52CE20`:
  section 1 has both.
- **The band `0x52A6C0..0x52CFE0` holds 23 catalogue rows the cut leaves
  out** (`0x52ADA0` .. `0x52C7C0`, part 7 "Unlabelled", hosts `0x5298A0` and
  `0x52B6C0`): the other two choices of `ChoiceMenu_Choices`, the table at
  `0x66030C`, and `0x52B6C0`'s mode-8 panel code. Not this group's (the cut
  owns); they are in no group of this round - for the coordinator.

## 6. Latent defects (Capcom's, described, not fixed)

- **L1 Unchecked dispatchers**: `ChoiceMenu_Run` by record 6 `+6` (three
  entries, `ExtraSlots_Steps` follows), `ExtraSlots_Step` by `+4` (three,
  the table `0x66030C` follows). `0x52A4A0` keeps `+6` in 0..2 and the steps
  keep `+4` in 0..2 by their own increments, so play should not reach it.
  Ours aborts.
- **L2 `Sprite_UpdateScreenScaled` divides by `+0x60 - 2 * s16 +0x3E`** when
  `+0x60` is below 0, which is 0 when the two meet; the original faults.
  Whether any sprite reaches it is not measured. Ours aborts.
- **L3 Reads by unchecked indexes, no fault**: `UiSprite_SetMode` by a byte
  into ten records (`UiSprite_Sheet` follows at `+0xA4`), `UiSprite_Draw`
  into 74 (`0x6608D8`'s bytes follow) - every constant index at the 131
  call sites is inside (0..9 and 0..0x48); `FieldPanel_KindPoints` by a byte
  into the kinds' records (32 counted by `FieldPanel_KindTotal`);
  `ExtraSlots_PickItem` by the s16 words `+0x36` / `+0x3A` into the 128-byte
  id list, and by an id into `NameTable_Accessories`. All read the image or
  the save block in place, the same on both sides.
- **L4 `ExtraSlots_PickItem`'s two slots are not symmetric**: the kind-0xB
  slot (`0x904130`) takes the id under the cursor even when it is already
  the slot's - `Inventory_Remove(3, id, 1)` then `Inventory_Add(3, id, 1)`,
  a net nothing if both succeed -, where the kind-0xA slot checks
  `0x90412E != id` first; and neither checks `Inventory_Remove`'s answer
  before filling the slot. Whether a failed remove can happen from this
  list (every id shown is in the inventory) is not measured.

## 6a. Controls

`scratchpad/e1f/controls.py` (the session scratchpad, not committed): each
plant anchored on a unique string of `effect_1f.cpp`, rebuilt, run alone
(`BOF3X_E1F_ONLY`), restored, and rebuilt at the end. Run against the final
fuzz (8,000 rounds); the count is the mismatching rounds.

| Function | Controls | Refused (rounds of 8,000) |
|---|---|---|
| `ChoiceMenu_Run` | C1 index + 1 | 8,000 |
| `ExtraSlots_Step` | C2 index + 1 | 8,000 |
| `ExtraSlots_Enter` | C3 state 4 dropped, C4 mask 0x3F, C5 hand y + 1, C6 +4 by 2 | 1,834; 1,316; 3,642; 3,642 |
| `ExtraSlots_PickSlot` | C7 sound, C8 bit 6, C9 +3 by 2, C10 +4 = 1, C11 bit 2, C12 repeat mask, C13 `^ 6`, C14 + 3, C15 slots swapped, C16 bit 6 test, C17 hand << 3, C18 `Input_Pressed` read before the repeat call | 2,583; 1,990; 2,583; 2,583; 1,073; 455; 1,180; 1,671; 2,360; 1,930; 2,922; 117 |
| `ExtraSlots_PickItem` | C19..C53 and C35b: kinds, counts, slot writes, cursor and scroll bounds, the loop bound, sounds, labels, the pull-back, the early `Input_Pressed` read | every one refused but C35, from 4 (C29, the pull-back's `count - 9`) and 9 (C51, the loop's 0x80) to 3,976 |
| `Sprite_UpdateScreenScaled` | C54 +0x3C held at 1, C55 put back + 1, C56 mask, C57 lift << 2, C58 `far > 1`, C59 eax at 0, C60 put back on the record before the call | 7,982; 8,000; 2,914; 2,088; 730; 533; 303 |
| `Sprite_UpdateAllScaled` | C61 in-use byte +1, C62 29 records | 8,000; 3,990 |
| `Effect_ResetFirstSeven` | C63 six records, C64 +5 for +4, C65 from 8, C66 state 3 | 8,000 each |
| `FieldPanel_KindPoints` | C67 threshold +0xE, C68 `>`, C68b `low + 1 >=`, C69 high word dropped, C70 `* 9`, C71 `/ 9` | 2,369; a fault; 630; 5,930; 678; 778 |
| `FieldPanel_KindTotal` | C72 31 kinds, C73 kind + 1 | 4,000; 8,000 |
| `GameMode8_*` | C74 first two calls swapped, C75 the sprite pass dropped, C76 the other raw callee | 8,000 each |
| `UiSprite_SetMode` | C77 `d0 & 7`, C78 `dC & 0x300`, C79 `& 0x70`, C80 dfe 1, C81 size 0xD | 277; 3,929; 5,139; 8,000; 8,000 |
| `UiSprite_Draw` | C82 colour, C83 x for y, C84 `& 0x1F`, C85 height from +8, C86 v from +0xC, C87 `Gpu_SetSprt` after the colour, C88 commit before the last byte, C89 x not narrowed to s16, C90 answer + 1 | 8,000; 8,000; 754; 5,306; 7,678; 7,957; 7,317; 8,000; 8,000 |

**Not refused by a count, two**: **C35** (`scrolled >= 0` -> `> 0`) is an
equivalent mutant - at a scrolled word of exactly 0 (an old word of 9) both
branches write 0 and play 0x100 - and its near variant **C35b** (`>= -1`)
is refused (27). **C68** (`low >= threshold` -> `>`) faults: at a threshold
and count of 0 the mutant divides by 0 (exit 0xC0000005), which proves less;
**C68b** (`low + 1 >= threshold`) is refused by a count (630). Before the
final seeds (the list's kind count and the cursors' boundaries seeded, 6,000
rounds) C29 and C51 were not refused; the seeds are the fix, not the plant.

## 7. Calls across groups

**Out**: none to another group of this round (`band_rows.py --edges`: E1F
calls no cut row but its own). `0x52B6C0` and `0x593950` are called raw
(`effect_1f_callees.h`): both in no cut. Everything else by name.

**In** (for the rebinding pass; `band_rows.py --refs`, the `E8` scan):

| Callee | Callers outside the group |
|---|---|
| `UiSprite_Draw` | 100 sites: E1A (`0x464BA0`, `0x464F80`, `0x4652D0`, `0x465AB0`, `0x465E50`, ...), E1B (`0x4678C0`, `0x467B10`, ...), E1G, FE1's eight panels (ours, raw through `field_e1_callees.h`, now rebound) |
| `UiSprite_SetMode` | 31 sites: the same families |
| `Sprite_UpdateScreenScaled` | E1A `0x4656C0` (call), `0x465840` (tail `jmp`) |
| `Effect_ResetFirstSeven` | E1E `0x52A370` |
| `ChoiceMenu_Run` | E1E `0x52A420`'s table `0x6602E0` (cell, no rebinding) |
| `ExtraSlots_Step` | `ChoiceMenu_Choices` (ours); `Field_FormActions[13]` `0x522B40`'s cell by word 0x100 (Capcom's, cell) |
| `FieldPanel_KindPoints`, `_KindTotal` | FE1's `FieldPanel_DrawKindRow`, `FieldPanel_DrawTotal` (ours, rebound) |
| `GameMode8_*` | mode 8's table `0x656AB8` and `0x496440`, `0x4964E0` (Capcom's) |

## 8. The rebinding

`band_rows.py --refs --group E1F` found 66 raw references to four of the
fifteen, in three files.

- **Rebound** (the value unchanged, one line each): `field_e1_callees.h`'s
  `kDrawMode` = `bof3::addr::UiSprite_SetMode`, `kDrawSprite` =
  `bof3::addr::UiSprite_Draw`, `kKindPoints` =
  `bof3::addr::FieldPanel_KindPoints`, `kKindTotal` =
  `bof3::addr::FieldPanel_KindTotal`, and the `#include "bof3/symbols.gen.h"`
  it lacked. `field_e1.cpp` still calls through `SH_AT` with them, so FE1's
  fuzz keys stand.
- **Left raw, on purpose**: `field_e1_fuzz.cpp`'s `CallSite` tables (the
  disassembly's targets) and its rows `"0x52CFE0"` / `"0x52CED0"` keyed on
  the rebound constants; the comments that call them "nobody's".
- **For the coordinator** (a harness: not this group's to edit):
  `scenario_harness.cpp`'s `kField` rows `"0x52CFE0"`, `"0x52CF60"`,
  `"0x52CE60"`, `"0x52CED0"` (FE1's, lines 544, 559, 668, 669 at `1bb41df`)
  and `kEffectOverrides`' `FX_RAW(0x52CFE0)`, `FX_RAW(0x52CF60)` (lines 885,
  886) - still valid keys (address = key), stale as "E1F's, raw". The wave's
  groups that call `0x52CFE0` / `0x52CF60` raw (E1A, E1B, E1G) can call
  `SH_CALL(UiSprite_Draw)` / `SH_CALL(UiSprite_SetMode)` once this merges -
  but then the key is ours, and those two `FX_RAW` rows no longer serve them:
  **a `FX_OURS` row each is wanted in `kEffectOverrides`** when the first
  group calls them by name.
- **State tables named** (`[[data]]`): `ChoiceMenu_Choices` `0x6602F4` (3),
  `ExtraSlots_Steps` `0x660300` (3), and the two record tables the helpers
  read, `UiSprite_Modes` `0x660394` (160 bytes) and `UiSprite_Sheet`
  `0x660438` (1,184). Each count from the next table any code indexes
  (`0x660300`, `0x66030C`, the sheet at `+0xA4`, `0x6608D8`), checked by
  hand.

## 9. The live route

The catalogue's reach columns (`analysis/remaining_catalog.tsv`: attract,
shop, worldmap, combat) are empty for all fifteen; the first-call traces
`reach_dragon` and `reach_whelp` enter none, and no other trace under
`analysis/calltrace` names them. **Fuzz only.** Game mode 8's steps and the
menu want a route the owner records through the place that opens them; which
place that is is the owner's to say.

## 10. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29): `0052A6C0 E`, `0052A6D0
12`, `0052A6F0 8C`, `0052A780 16F`, `0052A8F0 4A4`, `0052CF00 23` (the
existing `0052CF00 35` swallowed `0x52CF30`), `0052CF30 5`. The other eight
were there with these extents.

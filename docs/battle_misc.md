# The small battle helpers (group BF)

**Status:** IN PROGRESS (2026-09-24) - twenty-seven functions ours in
[`src/game/battle_misc.cpp`](../src/game/battle_misc.cpp). Each was read to
its last instruction, and against its PSX twin where one is paired. Each is
fuzzed against Capcom's at start-up (`BOF3X_SHADOW=battle_misc`, 147,500
rounds, 0 mismatches; with `'*'`, exit 0 and 827 injects). There are 92
negative controls: 90 were refused by a comparison and 2 are changes that
change nothing (section 3.2). No divergence. No defect that the game reaches.
**Not yet through a live check.** The combat A/B runs centrally after the
merge (section 5).

This is group BF of the seventh parallel round
([`takeover-queue-round7.md`](takeover-queue-round7.md)). The functions are
the battle engine's small helpers that the owner's combat route reaches:
- the banner pool and the message queue
- the actor bits and sound cues
- the actor contexts and their copies
- the item window's record
- the enemy AI's per-turn row check
- a bounded string copy
- the battle's object screen pass

Every claim about the binary comes from capstone over `bof3/BOF3.exe` on
2026-09-23:
- recursive descent from each entry, jump tables followed (the scratchpad's
  `bf/reach.py`)
- E8/E9 call sites and what each does with `eax` next (`bf/callers.py`)
- the pushes before each banner call (`bf/kinds.py`)

The PSX side comes from capstone over the sibling's overlay captures
(`analysis/overlay_captures_all.json`). Section `8a80230e..` is
`BATTLE.EMI#3`, loaded at `0x801D0C00`, and section `4065db04..` is
`BATTLE.EMI#15`, loaded at `0x80093800`. Its names are from the sibling's
`names/functions.toml`. The call counts are the combat route's
(`analysis/combat_catalog.md`), and **all twenty-seven are reached by it**.
The attract sequence, the shop route and the world-map route reach none of
them.

## 1. The functions

| PC | name | size | PSX twin | combat calls | what |
|---|---|---|---|--:|---|
| `0x4469F0` | `Battle_WrapIndex` | `0x1A` | none paired | 4 | wrap-around index: below `low` gives `high`, above `high` gives `low`; signed dwords |
| `0x446A10` | `Battle_PulseStep` | `0x34` | `0x801DD7C8` | 619 | the menu cursor's pulse: byte `0x904AC8` +/- 2 between 0 and `0x1F`, direction `0x904AC4` |
| `0x446A50` | `Battle_PlayActorCue` | `0x2C` | `0x801DD820` `Battle_PlayActorCue` | 11 | `Sound_PlayEffect(0x64E3BC[kind + cue * 3])`, kind the word `+0x2C` of `*Field_State` |
| `0x446BB0` | `Battle_StatusTint` | `0x1E` | `0x801DDA7C` | 4 | status bit 7: `Sprite_SetTint(Sprite_Current, -6, -10, 0, 0)`; else a bare `ret` |
| `0x446BD0` | `Battle_InitActorContext` | `0x54` | `0x801DDAB8` | 4 | a context (`0x93A000 + slot * 0x84`) for `Sprite_Current` |
| `0x446C30` | `Battle_InitActorContexts` | `0x73` | `0x801DDB44` | 1 | every actor's context, in actor order |
| `0x446E40` | `Battle_LoadSoundByKey` | `0x59` | none paired | 3 | a SND file through a per-set key list (`0x6565BC`) |
| `0x446FB0` | `Battle_SetActorBit` | `0x13` | `0x801DE158` | 17 | `0x904B82 \|= 1 << (n & 31)` |
| `0x446FD0` | `Battle_ClearActorBit` | `0x15` | `0x801DE178` `Battle_ClearActorBit` | 16 | `0x904B82 &= ~(1 << (n & 31))` |
| `0x446FF0` | `Battle_SpawnActorCopies` | `0x11D` | `0x801DE19C` | 4 | a kind-7 context holding a copy of every actor able to act |
| `0x447840` | `ItemMenu_CanUseSelected` | `0x3C` | none paired | 3 | can the item under the item window's cursor be used (1.4) |
| `0x447E60` | `ItemMenu_SetupForActor` | `0xD4` | none paired | 3 | window record 16 for the acting actor's inventory |
| `0x449E00` | `Battle_ReturnTrue` | `0x3` | `0x80097F44` | 6 | `mov al, 1; ret` (1.5) |
| `0x449E10` | `ItemMenu_SetupForParty` | `0x7B` | `0x80097F4C` | 1 | window record 16 for the party's list |
| `0x449FE0` | `ItemMenu_FreeWindows` | `0x20` | `0x800981D4` | 4 | window records 16..19 freed |
| `0x44A5C0` | `BattleBanner_Dispatch` | `0x88` | `0x801DE404` `BattleBanner_Dispatch` | 2,091 | each active banner entry's tick (1.1) |
| `0x44A650` | `BattleBanner_Add` | `0x89` | `0x801DE528` | 4 | the first free entry from 2 to 7 filled |
| `0x44A6E0` | `BattleBanner_Set` | `0x5B` | `0x801DE5D4` | 13 | entry `slot & 0xFF` filled, no bound |
| `0x44A810` | `BattleBanner_ClearAll` | `0x1B` | `0x801DE7CC` | 1 | bytes +0..+2 of the eight entries zeroed |
| `0x44A830` | `BattleBanner_NoneOfKind` | `0x42` | `0x801DE820` `BattleBanner_NoneOfKind` | 4 | al 1 when no active entry has the kind |
| `0x44A880` | `BattleQueue_Push` | `0x37` | `0x801DE888` | 5 | the 16-entry message queue at `0x93C2C0` (1.2) |
| `0x44A8C0` | `BattleQueue_Pending` | `0x11` | `0x801DE8F4` | 1 | write index != read index |
| `0x44A8E0` | `BattleBanner_SetMessage` | `0x2B` | `0x801DE914` | 7 | message text into banner 0 (1.2) |
| `0x44A990` | `BattleBanner_ShowName` | `0x6F` | `0x801DEA20` | 12 | an enemy's name (and suffix) as banner 0 (1.2) |
| `0x44AAD0` | `EnemyAI_TurnCheck` | `0x343` + tables `0x50` + `0x26` | `0x80098BB0` `EnemyAI_TurnCheck` | 8 | the current enemy's four per-turn AI rows (1.3) |
| `0x5171A0` | `Str_CopyN` | `0x34` | boot `0x8015030C` | 23 | at most n bytes to the NUL, then a NUL; eax the NUL's address |
| `0x517440` | `Sprite_UpdateObjectScreens` | `0x44` | `0x801A1624` (callers tier, `GAME.EMI`) | 2,091 | the screen update of each type-7+ object |

Sizes run to the byte after the last reachable instruction. The catalogue's
sizes (`combat_catalog.md`) are right, or right less alignment padding, for
all but one. **`0x44AAD0` is `0x343` bytes of code, not the catalogue's
176.** Its jump table (20 dwords at `0x44AE14`) and its index bytes (38 at
`0x44AE64`) follow the code.

**For `entries_logic.txt`**, the sizes are the code extents in the table.
`0x449E00` is 3 bytes: the tracer needs its size, and `0x449E03..0x449E0F`
is `nop` padding that the detour's jump overwrites.

The PSX pairs come from `analysis/pairs_propagated.json`. Each was checked
against a read of the twin.
- **Pairs with a wrong section.** The file gives five of them the section
  `boot` beside the overlay one. For `0x44AAD0` it gives `boot` alone, but
  the sibling names it in `BATTLE.EMI#15`.
- **`0x446A10`.** Its tier there is `call`, and the twin matches branch for
  branch.
- **`0x44A5C0`.** The file has no pair for it. The twin is the sibling's
  `BattleBanner_Dispatch`: the body is at `0x801DE440`, and the entry is
  `0x801DE404`, which builds the table.
- **Three functions have no twin to be found.** They are `0x4469F0`,
  `0x446E40` and `0x447840` (and `0x447E60`: nothing paired in its
  neighbourhood either). `0x446E40` loads a SND file, where the PSX streams
  from the CD. The other three are item-window code that the port rewrote:
  its item window is a record in the window task, `0x803160 + 16 * 0x24`.

### 1.1 The banner pool

The banner pool has 8 entries of `0xC` bytes at `0x93B8E0`. The PSX pool is
at `0x801EB460`.

| offset | field |
|---|---|
| +0 | active |
| +1 | kind |
| +2 | a byte |
| +3 | layer, 0 or 1 |
| +4 | text |
| word +8 | timer, `0xFF` for forever |
| +0xA | 0 |

`BattleBanner_Dispatch` runs once a frame from the battle frame at `0x42E3A8`.
1. It sets `0x904AE9` to 0. This byte is the mask of kinds seen this frame.
2. It walks layer 0, then layer 1. For each entry:
   1. The entry becomes `0x93B8C0` before anything is tested.
   2. The entry is skipped unless it is active, on this layer, and its kind
      shares no bit with the mask. The mask is read again for each entry.
   3. `DamageScratch` gets the entry's index. The PSX puts this index in
      scratchpad byte `0x1F800000`.
   4. The kind byte, read again, indexes a five-entry table that the
      original builds on its stack from immediates:

| kind | handler |
|---|---|
| 0, 3, 4 | `0x437CC0` (a bare `ret`) |
| 1 | `0x44A740` |
| 2 | `0x44A7B0` |

The PSX table at `0x801D0C84` has empty functions for kinds 0, 3 and 4 too,
so the dispatch behaves the same on both.

The two handlers are unlisted and unread beyond what they show:
- **Kind 1** counts the timer down, deactivates the entry at 0, then calls
  `BattleBanner_NoneOfKind(1)`.
- **Kind 2** does the same, but calls `NoneOfKind(4)`. This matches the
  PSX's kind-2 handler `0x801DE71C`.

**There is no bound on the kind.** Index 5 of the stack table is the
original's own return address, so ours aborts loudly on a kind of 5 or more
(`CLAUDE.md` rule 4, as `Field_RunTaskRecords` does).
- Of the 31 `BattleBanner_Add` sites, 29 push the constant 1 or 2; two
  (`0x430174` and `0x44A418`) push `ebx`, which was not traced back.
- All four `BattleBanner_Set` sites push 1 or 2.

`BattleBanner_Add` looks for a free entry from 2 to 7. When it finds one, it
returns eax = index * 12. When all are taken it writes nothing and returns
`0x15`, the `lea` of the last index (7 * 3). Several of its 31 callers
overwrite only al and return, so ours returns the whole register.
`BattleBanner_Set` fills entry `slot & 0xFF` with no bound.

### 1.2 The queue, the message and the name

**The message queue.** `BattleQueue_Push(a, b, value)` writes entry
`0x93C2A1` of the queue at `0x93C2C0`, where each entry holds a byte, a byte
and a dword. The write index is a byte with no bound, and is stored back as
`(index + 1) & 0xF`. `BattleQueue_Pending` compares the write index with the
read index `0x93C2A0`.

**The message.** `BattleBanner_SetMessage` copies 8 bytes of the string
`0x669DE0[msg]` into `0x904EC0`, then sets banner 0's byte +2.
- The PSX copies from a table of 8-byte strings at `0x801EAFB0 + msg * 8`.
- The PC's table holds string pointers. This is the table the English
  overlay can repoint.

**The name.** `BattleBanner_ShowName(actor)`:
1. copies 8 bytes from `actor + 0x80`: the enemy object's name, the 16
   bytes the port prepended to the working record
   ([`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md) §1);
2. when `0x904B7A` is set, appends the string that `0x669DE4` points at
   (message 1). The append is inline: `repne scasb` twice, then `rep movsd`
   and `rep movsb`;
3. makes it banner 0 through `BattleBanner_Set(0, 1, 1, 0, 0xFF, text)`.

The PSX copies 5 bytes from `+0x74` and appends `0x801EAFB8` under
`0x801463B6`.

### 1.3 `EnemyAI_TurnCheck`

This is the sibling's hypothesis name, confirmed here opcode for opcode.

**The rows.** For row i = 0..3, the row is 16 bytes at `0x8C5600 + script *
0x8C + i * 16`. The script is byte `+0xF0` of the current enemy
`0x939AD8`, read at the row's start. The PSX stride is `0x88`. The row's
byte 0 is its opcode, and the dispatch goes through the index bytes
`0x44AE64` and the jump table `0x44AE14`:

| opcode | test | then |
|---|---|---|
| 0..8 | `0x44B320(mask)`, masks 1, 2, 4, 8, `0x10`, `0x20`, `0x40`, **`0x100`, `0x80`** | fire once, the enemy read again after the test |
| 9 | acting kind `0x904B35` is 4 and word `+0x108` is non-zero | fire once |
| `0xA` | acting kind is 1 and word `+0x108` is non-zero | fire once |
| `0x16` / `0x17` | byte `+0x92` bit 3 / bit 7 | fire once |
| `0x18` | byte `+0xAA` is zero | fire once |
| `0x21`..`0x23` | `0x44B320(1 / 2 / 4)` | `0x44B3A0` only |
| `0x24` | acting kind is 1 and word `+0x108` is non-zero | `0x44B3A0` only |
| `0x25` | u16 `+0xA4` is not above s16 `+0x108` | fire once |
| other (above `0x25` by the range check) | nothing | |

**"Fire once"** is three calls:
1. `0x44B2C0(enemy, i)`, the row-done bit. If it is set, the row stops here.
2. `0x44B3A0(enemy, row)`.
3. `0x44B2E0(enemy, i, 1)`.

The enemy handed to the second and third calls is read again before each.
Then comes `0x44B920`, whose eax is returned.

**The row index.** The original keeps it in its pushed `ecx` slot, so the
upper bytes of the dword it passes on are the caller's `ecx`. Both callees
read only the low byte.

### 1.4 The item window

**`ItemMenu_CanUseSelected`** makes two calls.
1. It calls `0x591E50(actor 0x929F06, page 0x8033AB, 1)`. This is group
   BD's function, and it returns a pointer to the actor's inventory page: a
   party record's `+0x6A`, `+0x74`, `+0x7E` or `+0x60`, by page.
2. It calls `0x57DA70(2, actor, the page's byte at cursor 0x8033AC)`. The
   actor is read again after the first call.

Its al is 1 when `0x57DA70` answers al 0. The two dwords of the second call
carry the page pointer's upper half above their bytes. This comes from
`movzx ax` and then `mov al` on the returned register, and ours reproduces
it. The name is a hypothesis: the only caller plays cue `0x107` when the
answer is 1.

**`ItemMenu_SetupForActor` and `ItemMenu_SetupForParty`** fill window
record 16 (`0x8033A0`). Its type is 1 and its handler is 8. They also set
the bytes `0x80340C..E` and the item window's actor `0x929F06`. The
`ForActor` version reads the actor from byte +5 of `*0x939EC4`. It reads it
twice, once after the first four stores and once after all of record 16's
stores, and the fuzz aliases it into the record to check both reads.

### 1.5 `0x449E00` is a function, not a stub

`0x449E00` is three bytes: `mov al, 1; ret`. It is a complete function.
- The PSX twin `0x80097F44` is `jr ra` with `v0 = 1`.
- On the PSX, `Item_TargetSetup` calls it as `0x80097F44() ? 0 : ..`, a
  predicate the port never made false.
- All 13 of its PC callers test al.

Ours returns 1 as `unsigned char`.

## 2. Registers the originals leave, and what ours returns

Where a caller could read more than al, ours returns the original's whole
eax. Each case was found from the E8 scan (`bf/callers.py`), and each is
checked by the fuzz's full-width result compare and by a control.

| function | eax returned | why a caller can read it |
|---|---|---|
| `Battle_WrapIndex` | the whole value | callers push the whole value on |
| `Battle_SetActorBit` / `Battle_ClearActorBit` | `1 << n` / its complement | `0x42FBE0` pushes eax on with a word over its low half |
| `Battle_PlayActorCue` | `Sound_PlayEffect`'s eax | `0x4FC079` returns it |
| `Battle_StatusTint` | `Sprite_SetTint`'s eax; **on the no-tint path, the caller's own** | `0x44F40E` / `0x44F446` return it; ours is a naked `testb` / `jne` / `ret` |
| `Battle_SpawnActorCopies` | the last actor's context pointer, or `0x4456C0`'s eax, with al 11 | its caller, a battle-task state at `0x42E9C7`, returns it |
| `ItemMenu_SetupForParty` | 0 | |
| `ItemMenu_FreeWindows` | `Window_FreeCurrent`'s eax | two tail jumps |
| `BattleBanner_Add` / `_Set` | the entry offset, or `0x15` when none is free | callers overwrite only al |
| `BattleBanner_ClearAll` | `0x93B941` | |
| `BattleQueue_Push` | the entry offset | `0x42FF2F` returns it |
| `BattleBanner_SetMessage` / `Str_CopyN` | the NUL's address | four tail returns |
| `BattleBanner_ShowName` | `BattleBanner_Set`'s eax | |
| `EnemyAI_TurnCheck` | `0x44B920`'s eax | |

**Al only.** `Battle_LoadSoundByKey`, `ItemMenu_CanUseSelected`,
`Battle_ReturnTrue`, `BattleBanner_NoneOfKind` and `BattleQueue_Pending`
return al alone: every caller tests al. The remaining five are `void`, since
their callers reload eax.

**The arguments.** Ours passes the arguments as the original's registers
leave them where a callee could read the upper bits:
- `Battle_PlayActorCue`: the cue id in `ecx`, carrying the record pointer's
  upper half.
- `ItemMenu_CanUseSelected`: its second call.

Stale upper bits that the callee masks off are recorded masked. These are
the entry `eax` and `ecx` handed to `0x591E50`, the stack garbage above the
index given to `0x4456C0`, and `EnemyAI_TurnCheck`'s row index.

## 3. The fuzz

`BOF3X_SHADOW=battle_misc` is one start-up run of about a second.

**The copies.** It makes 27 byte-copies. Every relative call is re-aimed at
a recording stand-in; this includes the calls among the group's own
functions, so each function is tested alone. Two copies need more than
that:
- `BattleBanner_Dispatch`'s four table immediates (`+4`, `+0x14`, `+0x1C`,
  `+0x28`) are re-aimed in its copy.
- `EnemyAI_TurnCheck`'s jump table is relocated into its copy (`0x394`
  bytes copied).

**One round.** Each round builds the state the function reads. Capcom's copy
and ours then run from that same state, with the same `eax` and `ecx` on
entry and the same six argument dwords, through a naked call helper
(`BattleMisc_CallRegs`). The two runs are compared on:
- the calls out, with their arguments;
- the result, masked as in section 2;
- every region either run could write.

**What the stand-ins do.** They write what the real callee writes where the
caller reads it again: `Str_CopyN`'s copy, which the inline `strcat` scans.
Now and then they also change what the caller reads after the call:
- `Sprite_Current`
- the current enemy
- a later actor's flag or skip bit
- a later object's type or flag
- a later banner's kind, layer or activity, the kinds mask, or the scratch
  byte
- a later AI row's opcode, the acting kind, or the enemy's fields
- the item cursor or actor
- the name suffix switch or pointer

Every such disturbance is drawn from a hash of the round and the call's
position, never from the fuzz's generator. A stand-in runs once per side,
and a generator draw inside one made the two sides diverge on the first
build: 16,180 false mismatches.

**What the fuzz seeds, per function:**
- **Wrap:** `low` and `high` near each other, and near 0, the `int`
  extremes and ±1. The value is at, and ±1 from, both bounds.
- **Pulse:** the counter at 0, 1, 2, `0x1D`..`0x21`, `0xFE` and `0xFF`. The
  flag at 0, 1, 2 and `0xFF`.
- **Tint:** the status with and without bit 7, and with noise above it.
  The entry eax is random, so the no-tint path's register is compared.
- **Contexts:** slots 0..47, with `Sprite_Current` sometimes inside the
  context being filled, 4 bytes before it or `0x40` to `0x4C` into it. This
  lets the read-after-store order show.
- **Copies:** the party and enemy flags and skip bits. A quarter of the
  rounds use slots 50..79, whose contexts lie over the enemy objects being
  copied, so the copy's direction shows.
- **Sound lists:** keys equal to the key, key ±1, 0, `0xFFFF`, and the key
  with bit 8 set (the high half is compared whole). Also null lists and
  empty lists.
- **Bits:** actors 0..10, 15, 16, 31, 32, 33 and `0xFF`, with noise above.
- **Item window:** cursor bytes above the page. The acting context's byte
  +5 inside record 16. The command flags 0, 2, `0x20000`, `0x20002`, all
  ones, and all but bit 17.
- **Banners:** active values 0, 1 and any byte. Kinds 0..4. Layers 0, 1, 2,
  `0xFF` and any byte. Every entry from 2 taken, one round in four. Set
  slots up to `0xFF`.
- **Queue:** the write index at 15, 16, `0x1F`, `0xFF` and any byte.
- **Name:** names of 0 to 12 bytes, with or without a NUL within 8.
  Suffixes of 0 to 9 bytes. The switch as 0, 1 or any byte.
- **AI:** every opcode from 0 to `0x27`, plus `0x80` and `0xFF`. Scripts
  0..63. HP at 0, 1, `0x7FFF`, `0x8000` and `0xFFFF`, with `+0xA4` at ±1 of
  it and at the sign cases. The acting kind as 0, 1, 2, 4, 5 or any byte.
  The current enemy is an enemy object or a buffer of the fuzz's.
- **`Str_CopyN`:** n at 0, 1, 7, 8, 9, 12, `0xFF` or any value, with noise
  above. Destinations 1 to 4 bytes ahead of the source (the copy feeds
  itself), behind it, or apart from it.
- **Screens:** types 0, 6..`0xB`, `0xFF` and any byte, present or not.

**The result.** Run on 2026-09-24, it passed alone and with `'*'`: exit 0,
827 injects, no Fatal. **147,500 rounds, 0 mismatches.** Per function, as
rounds / calls out / covered:

| function | rounds | calls out | covered |
|---|--:|--:|---|
| `Battle_WrapIndex` | 10,000 | | 2,593 in range |
| `Battle_PulseStep` | 3,000 | | 2,289 falling |
| `Battle_PlayActorCue` | 3,000 | 3,000 | |
| `Battle_StatusTint` | 3,000 | | 1,506 tinted |
| `Battle_InitActorContext` | 5,000 | | 1,652 aliased |
| `Battle_InitActorContexts` | 5,000 | 55,000 | |
| `Battle_LoadSoundByKey` | 10,000 | | 2,113 loaded |
| `Battle_SetActorBit` | 3,000 | | |
| `Battle_ClearActorBit` | 3,000 | | |
| `Battle_SpawnActorCopies` | 5,000 | 58,463 | 1,245 with overlapping slots |
| `ItemMenu_CanUseSelected` | 5,000 | 10,000 | |
| `ItemMenu_SetupForActor` | 5,000 | | |
| `Battle_ReturnTrue` | 500 | | |
| `ItemMenu_SetupForParty` | 2,000 | | |
| `ItemMenu_FreeWindows` | 2,000 | 8,000 | |
| `BattleBanner_Dispatch` | 10,000 | 35,026 handler calls | |
| `BattleBanner_Add` | 5,000 | | |
| `BattleBanner_Set` | 5,000 | | |
| `BattleBanner_ClearAll` | 1,000 | | |
| `BattleBanner_NoneOfKind` | 5,000 | | |
| `BattleQueue_Push` | 5,000 | | |
| `BattleQueue_Pending` | 2,000 | | 1,014 pending |
| `BattleBanner_SetMessage` | 5,000 | | |
| `BattleBanner_ShowName` | 10,000 | | 7,494 with the suffix switch on |
| `EnemyAI_TurnCheck` | 20,000 | 89,369 | 6,612 with a row marked done |
| `Str_CopyN` | 10,000 | | |
| `Sprite_UpdateObjectScreens` | 5,000 | 76,981 updates | |

### 3.1 Negative controls

Each control was planted alone in ours by the scratchpad's `bf/runner.py`,
from `bf/controls.py`: edit, build, headless self-test, restore. The count
is mismatching rounds per function. Every refusal is by comparison, none by
a fault or a hang.

**Controls per function:** Wrap 3, Pulse 3, Cue 3, Tint 3, Context 3,
Contexts 3, Sound 4, SetBit 2, ClearBit 2, Copies 7, CanUse 4, SetupActor
4, ReturnTrue 1, SetupParty 2, Free 2, Dispatch 5, Add 3 (one shared with
Set), Set 2, ClearAll 2, NoneOfKind 2, Push 3, Pending 1, Message 3, Name 5,
AI 12, CopyN 4, Screens 4.

**Z7 and the fuzz fix.** Z7 (copy the object backwards) was not refused on
the first run: no context overlapped an object. The copies fuzz then gained
the overlapping slots, Z1 to Z7 were run again, and Z7 was refused. The
Z counts below are from that second run.

| # | control | refused |
|---|---|---|
92
| W1 | wrap: below low tested with <= | Battle_WrapIndex 565 |
| W2 | wrap: unsigned compares | Battle_WrapIndex 3,678 |
| W3 | wrap: above high tested with >= | Battle_WrapIndex 427 |
| P1 | pulse: turn at > 0x1F | Battle_PulseStep 122 |
| P2 | pulse: down on any non-zero flag | Battle_PulseStep 260 |
| P3 | pulse: 0 leaves the flag | Battle_PulseStep 122 |
| C1 | cue: row stride 2 | Battle_PlayActorCue 2,469 |
| C2 | cue: id without the pointer upper half | Battle_PlayActorCue 3,000 |
| C3 | cue: returns 0 | Battle_PlayActorCue 3,000 |
| T1 | tint: bit 6 | Battle_StatusTint 1,477 |
| T2 | tint: no-tint path clears eax | Battle_StatusTint 1,494 |
| T3 | tint: green -11 | Battle_StatusTint 1,506 |
| X1 | context: object read before the call | Battle_InitActorContext 941 |
| X2 | context: +8 cleared, not +9 | Battle_InitActorContext 4,992 |
| X3 | context: +0x80 stored last | Battle_InitActorContext 547 |
| Y1 | contexts: Field_State not set | Battle_InitActorContexts 4,914 |
| Y2 | contexts: an absent party member takes no slot | Battle_InitActorContexts 3,046 |
| Y3 | contexts: enemies present on bit 1 | Battle_InitActorContexts 4,980 |
| L1 | sound: key compared on 16 bits | Battle_LoadSoundByKey 1,079 |
| L2 | sound: id + 0x2000 | Battle_LoadSoundByKey 2,113 |
| L3 | sound: a null list answers 0 | Battle_LoadSoundByKey 1,228 |
| L4 | sound: the high half shifted arithmetically (changes nothing?) | not refused: a change that changes nothing (section 3.2) |
| S1 | setbit: shift masked with 15 | Battle_SetActorBit 754 |
| S2 | setbit: returns 0 | Battle_SetActorBit 3,000 |
| R1 | clearbit: shift masked with 7 | Battle_ClearActorBit 1,408 |
| R2 | clearbit: returns the bit | Battle_ClearActorBit 3,000 |
| Z1 | copies: party skip on bit 2 | Battle_SpawnActorCopies 3,708 |
| Z2 | copies: Sprite_Current set before the call | Battle_SpawnActorCopies 4,961 |
| Z3 | copies: al 10 at the end | Battle_SpawnActorCopies 5,000 |
| Z4 | copies: +0x29 = 2 | Battle_SpawnActorCopies 4,961 |
| Z5 | copies: party is index 0..1 | Battle_SpawnActorCopies 5,000 |
| Z6 | copies: 0x4456C0 eax kept as a byte | Battle_SpawnActorCopies 3,740 |
| Z7 | copies: the object copied backwards | Battle_SpawnActorCopies 99 |
| U1 | canuse: actor read once | ItemMenu_CanUseSelected 1,015 |
| U2 | canuse: arguments without the list upper half | ItemMenu_CanUseSelected 5,000 |
| U3 | canuse: whole eax tested | ItemMenu_CanUseSelected 2,001 |
| U4 | canuse: cursor read before the call | ItemMenu_CanUseSelected 479 |
| A1 | setupactor: actor not read again | ItemMenu_SetupForActor 1,007 |
| A2 | setupactor: bit 16 instead of 17 | ItemMenu_SetupForActor 1,325 |
| A3 | setupactor: +0xD = 0xFE | ItemMenu_SetupForActor 5,000 |
| A4 | setupactor: actor read before the first stores | ItemMenu_SetupForActor 470 |
| V1 | true: returns 2 | Battle_ReturnTrue 500 |
| Q1 | setupparty: word +4 0xFF57 | ItemMenu_SetupForParty 2,000 |
| Q2 | setupparty: returns 1 | ItemMenu_SetupForParty 2,000 |
| F1 | free: three records | ItemMenu_FreeWindows 2,000 |
| F2 | free: the first call's eax | ItemMenu_FreeWindows 2,000 |
| D1 | dispatch: kinds not reset | BattleBanner_Dispatch 9,951 |
| D2 | dispatch: scratch not set | BattleBanner_Dispatch 9,933 |
| D3 | dispatch: current set only for dispatched entries | BattleBanner_Dispatch 7,536 |
| D4 | dispatch: layer tested with > | BattleBanner_Dispatch 7,800 |
| D5 | dispatch: kinds read once per layer | BattleBanner_Dispatch 1,186 |
| B1 | add: from entry 1 | BattleBanner_Add 1,855 |
| B2 | add: none free returns 0 | BattleBanner_Add 1,423 |
| B3 | add/set: timer word whole | BattleBanner_Add 3,561, BattleBanner_Set 4,987 |
| E1 | set: slot masked to 3 bits | BattleBanner_Set 1,630 |
| E2 | set: eax unmasked | BattleBanner_Set 2,542 |
| G1 | clearall: +2 kept | BattleBanner_ClearAll 1,000 |
| G2 | clearall: returns 0 | BattleBanner_ClearAll 1,000 |
| N1 | noneofkind: inactive entries count | BattleBanner_NoneOfKind 545 |
| N2 | noneofkind: kind whole | BattleBanner_NoneOfKind 2,090 |
| H1 | push: wraps at 8 | BattleQueue_Push 1,853 |
| H2 | push: eax index * 4 | BattleQueue_Push 4,796 |
| H3 | push: +1 gets a | BattleQueue_Push 4,978 |
| K1 | pending: write above read | BattleQueue_Pending 923 |
| M1 | message: index masked to 7 bits | BattleBanner_SetMessage 2,553 |
| M2 | message: b2 to +3 | BattleBanner_SetMessage 5,000 |
| M3 | message: returns 0 | BattleBanner_SetMessage 5,000 |
| O1 | name: the NUL not appended | BattleBanner_ShowName 6,669 |
| O2 | name: from +0x7F | BattleBanner_ShowName 10,000 |
| O3 | name: suffix always | BattleBanner_ShowName 2,236 |
| O4 | name: suffix pointer read before the copy | BattleBanner_ShowName 1,072 |
| O5 | name: returns 0 | BattleBanner_ShowName 10,000 |
| I1 | ai: masks 7 and 8 swapped | EnemyAI_TurnCheck 4,675 |
| I2 | ai: ops 0..8 fire with the enemy of the row start | EnemyAI_TurnCheck 1,373 |
| I3 | ai: apply handed the enemy of the test | EnemyAI_TurnCheck 737 |
| I4 | ai: 0x25 compares unsigned | EnemyAI_TurnCheck 848 |
| I5 | ai: 0x24 fires once | EnemyAI_TurnCheck 332 |
| I6 | ai: op 9 on acting kind 1 | EnemyAI_TurnCheck 829 |
| I7 | ai: the PSX script stride 0x88 | EnemyAI_TurnCheck 17,503 |
| I8 | ai: returns 0 | EnemyAI_TurnCheck 20,000 |
| I9 | ai: 0x18 on a non-zero byte | EnemyAI_TurnCheck 2,344 |
| I10 | ai: acting kind read once | EnemyAI_TurnCheck 399 |
| I11 | ai: 0x21..0x23 with the loop-top enemy | EnemyAI_TurnCheck 420 |
| I12 | ai: 0x16 on bit 2 | EnemyAI_TurnCheck 1,419 |
| J1 | copyn: one byte more | Str_CopyN 5,146 |
| J2 | copyn: returns dst | Str_CopyN 8,743 |
| J3 | copyn: the NUL copied and on | Str_CopyN 4,661 |
| J4 | copyn: memmove on overlap | Str_CopyN 2,363 |
| US1 | screens: type 6 in, 7 out | Sprite_UpdateObjectScreens 4,719 |
| US2 | screens: type 9 updated too | Sprite_UpdateObjectScreens 3,689 |
| US3 | screens: Sprite_Current set only for updated objects | Sprite_UpdateObjectScreens 2,260 |
| US4 | screens: type re-read after the update (changes nothing?) | not refused: a change that changes nothing (section 3.2) |

### 3.2 The two that change nothing

- **L4: the key's high half shifted arithmetically.** The original shifts
  logically (`shr eax, 0x10`). The shift happens only after the dword has
  been found not to be -1, and it is compared with `key & 0xFF`. The two
  shifts differ only when bit 31 is set, and then both results are above
  `0xFF`. No input can tell them apart.
- **US4: the type byte read again before the slot update.** The re-read
  happens on the `else` side, before any call is made. The byte cannot have
  changed.

## 4. What the fuzz does not reach

- **The real callees.** These are the other groups' functions (`0x435180`,
  `0x4456C0`, `Sprite_SetTint`, `0x591E50`, `0x57DA70`,
  `Sprite_UpdateScreenSlot`), the unnamed AI helpers (`0x44B320`,
  `0x44B2C0`, `0x44B3A0`, `0x44B2E0`, `0x44B920`), the banner handlers
  `0x44A740` / `0x44A7B0`, and ours in other modules (`Sound_PlayEffect`,
  `Sound_LoadStream`, `Window_FreeCurrent`, `Sprite_UpdateScreenA`). Here
  they are recorders.
- **A banner kind of 5 or more** where the dispatch would call it. The
  original calls through its own stack frame; ours aborts. The fuzz keeps
  kinds below 5.
- **Context slots above 82, and slots from 48 to 49.** These reach the
  banner pool and beyond (0x93A000 + slot * 0x84, no bound). They are the
  same arithmetic on both sides.
- **Real battle data.** The AI scripts, sound lists, cue table rows and
  names are the fuzz's own. The cue table itself is read from the image.

## 5. The live check

**Owed after the merge: the combat A/B** (`analysis/validate_combat.sh`),
which reaches all twenty-seven (the call counts are in section 1).
- **What would show on screen.** The banners: the enemy's name, which is
  `BattleBanner_ShowName`, and the messages. Also the item window and the
  menu cursor's pulse.
- **What only the memory checks see.** The AI rows and the contexts.
- **`entries_logic.txt`** wants the 27 entries at the section 1 sizes.

No DIV entry; no behaviour change.

## 6. Found on the way

- **Unbounded indices, all as the PSX has them.** None is reached with a bad
  value by anything read here, so none is written up as a defect:
  - the banner dispatch's kind: its stack table holds five, and kind 5 is
    the return address;
  - `BattleBanner_Set`'s slot;
  - `BattleQueue_Push`'s write index, which is only ever stored masked to 4
    bits;
  - the context slot from `0x435180`;
  - the sound-list set.
- **Kind 2's handler waits on kind 4.** It calls `NoneOfKind(4)`, not
  `NoneOfKind(2)`. The PSX's does the same (`0x801DE780`: `a0 = 4`). This
  looks deliberate or copied; it is kept as it is.
- **`Field_State` (`0x905D98`) is the current party actor's record in
  battle.** This is the PSX `0x8014624C`, which
  `Battle_InitActorContexts` sets for each party member. `0x939AD8` is the
  enemy's twin (PSX `0x801EB458`). The name `Field_State` (a hypothesis) is
  left as it is, and this is said, not renamed.
- **Other groups' functions, as seen from here:**
  - `0x435180` (BB) takes (0, kind) and returns a context slot in al.
    Kind 6 is an actor context, 7 a copy, and 0 a filler.
  - `0x4456C0` (BB) returns al 1 when the actor is absent or has status
    bit `0x40`: party `+0x91` / enemy `+0x93` of the object.
  - `0x591E50` (BD) returns an actor's inventory page by page number.
  - `0x44B2C0` (BE) is the sibling's `EnemyAI_RowDone`, bit `row` of the
    enemy's `+0xF1`. (Its `+0xE1` is on the PSX: the enemy's `+0x10`
    prepend.) `0x44B2E0` is `EnemyAI_SetRowDone`, `0x44B320`
    `EnemyAI_CondPartyFlag` and `0x44B3A0` `EnemyAI_ApplyAction`.
- **`analysis/pairs_propagated.json`** puts five overlay functions in `boot`
  as well (section 1).
- **A trap for fuzz authors.** In an anonymous namespace, an `enum`
  constant named like a file-scope constant (`kContexts`) silently shadows
  it. The first build wrote the contexts' random bytes to address 2. Name
  enum constants with a prefix.

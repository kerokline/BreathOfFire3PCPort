# Group FE1: the field engine's 0x52D080..0x533BA0 - the panel draws, four leader states, the cells around a sprite, the party's placements, the pending jump

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3, [`takeover-queue-round12.md`](takeover-queue-round12.md)), wave two, on
the round branch's tip `61be26e`. **45 functions ours**
(`src/game/field_e1.cpp`, shadow name `field_e1`): the cut table's 45 rows
for FE1 (`analysis/round12_cut.tsv`), none added, none dropped. Each read to
its last instruction with capstone and fuzzed through the scenario harness in
field mode ([`scenario_harness.md`](scenario_harness.md) section 7) without
edits to it: 270,000 rounds, 0 mismatches. CONTROLS_SUMMARY Fuzz-only except
the three both recorded routes enter (section 9).

The cut calls the band "event script, first seven runs"; what it holds is
wider:

| Part | Functions | Reached through |
|---|--:|---|
| The panel draws: two sprites, a kind's icon and row, the total and its rank, a message, a shade, two windows, a blink; an inventory test | 10 | E8 / E9 from Capcom's panel states `0x466BE0..0x4670C0` and `0x528A90..0x52A420` (not ours, not in the cut) |
| The way from the leader to a point | 1 | `Field_SwapGather`, `Field_SwapExchange` (ours) |
| Leader states 6, 8 (the jump, with 13 steps), 11 (a field object's content, 2 steps), 12; a passage's step 3; a party action's step | 21 | `Field_LeaderStates` `0x660918`, their own tables, `Field_PassageSteps[3]`, 48 cells of the PartyAction run tables |
| The zenny found | 1 | 21 E8 sites (17 in Capcom's `0x51C270..0x5251C6`, `Field_CellPickup`, `Field_PassageTake`, `Field_ContentTake`, FC2's `0x46D180`) |
| The cells around a sprite, the gateway exit | 3 | `Field_CellAround`, `Field_LeaderCellEvent`, `FieldMenu_TopBarInput` (ours) |
| The party's placements: at the slots, one script tick each, by the second list, the leader set off | 4 | Capcom's field-mode code at `0x496130..0x4961F9`; `Field_PartySetUp` (ours) |
| The pending jump's members | 5 | `Field_PendingJumps` `0x660B60` entries 1..3 (the table `Field_PendingJump`, ours, jumps through) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`evidence` for the reading, the names by shape): "panel", "kind", "rank",
"jump", "content", "gateway" name the code's shape, not a play-tested fact.
No Breath of Fire III gameplay fact is stated here from memory. One that
touches this band is on record: the owner identified the 16-byte name
`FieldPanel_DrawKindRow` copies for kind 0x16 (`0x669CD8`) as Manillo's, the
fish merchant ([`DIVERGENCE.md`](DIVERGENCE.md), the name overlay entry, which
cites `0x52D1EC` - an instruction of `FieldPanel_DrawKindRow`, `0x52D140 +
0xAC`). Ours reads that slot in place, so the overlay's patch of it holds.

## 1. What each function does

Every function's comment in `field_e1.cpp` is the full read and each
`symbols.toml` `evidence` string cites it; this section is the map. SC is
`Sprite_Current`, FS `Field_State`; both are read again at every use, as the
original reads `[0x937F88]` / `[0x905D98]`.

### 1.1 The panel draws (callers Capcom's)

The draw helpers are the engine's `0x52CF60` (a draw-mode primitive from the
16-byte records `0x660394`) and `0x52CFE0` (a sprite primitive from the
16-byte records `0x660438` at x, y; it answers the primitive), and
`0x468950` (a textured quad) - all by raw address, nobody's.

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `FieldPanel_DrawHeader` | `0x52D080` | 0x38 | mode (0, 1); sprites 0 at (x + 8, y), 1 at (x + 0x108, y) |
| `FieldPanel_DrawKindIcon` | `0x52D0C0` | 0x7B | mode (1, 1); sprite 2 at (x, y); unless the kind is 0xFF its CLUT word, u and v from the kind |
| `FieldPanel_DrawKindRow` | `0x52D140` | 0x1DD | a row: sprites 3, 4, 5 by the row; the kind's name to the text scratch `0x904BA0` (0x16: `0x669CD8`; 0xFF: eight '?'; else the 22-byte records `0x656FF8`); the count and `0x52CE60(kind, count)` printed |
| `FieldPanel_DrawTotal` | `0x52D320` | 0x23D | the total `0x52CED0` against 13 thresholds the code builds on its stack; the rank to `0x9045F4`; the rank's sprite and one or two sprites 0x48; the total printed |
| `FieldPanel_DrawMessage` | `0x52D560` | 0x5B | sprite 0xC; the script pool's message `id` unless 0xFFFF |
| `FieldPanel_DrawShade` | `0x52D5C0` | 0x4D | a half-transparent 320 x 240 tile, colour 0x20 |
| `FieldPanel_DrawBox3` | `0x52D610` | 0x13B | `Menu_DrawBox` (0x64 x 0x38, the style `0x903A5A`), the sprite frame, three pool messages by `0x8035FA` / `FC` / `FE` |
| `FieldPanel_DrawBox2` | `0x52D750` | 0x126 | the same wider (0xB3 x 0x42), two messages by `0x8035F8` / `0x803602` |
| `Inventory_Holds38To4DAt99` | `0x52D880` | 0x39 | al 1 when 22 or more slots of the consumables' list hold one of 0x38..0x4D at 99 |
| `FieldPanel_DrawBlink` | `0x52D8C0` | 0x2C | while `0x939A28` and `Frame_Counter` bit 3, sprite 0x47 at (0x1A, 0x5C) |

The rank thresholds and the rank records are immediates of
`FieldPanel_DrawTotal`'s code (stored byte by byte into its frame), so ours
holds them as its own constants; no `.data` table is copied.

### 1.2 The way and the leader's states

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `Field_PathClear` | `0x52EC20` | 0x15D | al: from the leader's x toward x in half units at z, then from its z toward z at x; each step's ground within 0x40 of the leader's height and not `Field_WayBlocked` |
| `Field_FormActionState` | `0x52F4F0` | 0x73 | state 6: input held ends it; else `Field_FormActions[0x90412C & 0x7F]`; once FS `+0x137` is 0, the pose, member 0 cleared, state 1, `Field_LeaderStand` |
| `PartyAction_ScriptEnd` | `0x52F5C0` | 0x16 | `Sprite_ScriptTickOnce`; at its end FS `+0x137` = 0 |
| `Field_PassageTrigger` | `0x52F8F0` | 0x53 | passage step 3: a stack record (`+0x86`, `+0x88` from FS `+0x12A` / `+0x12C`) to `Field_ObjectTrigger` unless 0xFF; step 2 |
| `Field_JumpState` | `0x52F950` | 0x12 | state 8: `jmp Field_JumpSteps[+2]` |
| `Field_JumpBegin` | `0x52F970` | 0xF | step 0: FE2's `0x535FC0` (a pose), step 2 |
| `Field_JumpOut` | `0x52F980` | 0x12 | step 1: `jmp Field_JumpOutSteps[+3]` |
| `Field_JumpOut0` .. `Field_JumpOut4` | `0x52F9A0` .. `0x52FA40` | 0x39, 0x13 x 3, 0x1D | FE2's `0x535FE0` (with `Field_JumpSetUp`), `0x536050`, `0x5360C0`, `0x536130`, `0x536170`; `+3` on, `Field_JumpCamera` in 0 unless `+5` or a script flag's bit 3; 4 ends in step 2 |
| `Field_JumpAir` | `0x52FA60` | 0x55 | step 2: by `Field_InputHeld` bit 12 / 14 / the button map `0x903580`, FE2's `0x5364D0` / `0x536550` / `0x5365D0`; `MapView_SetElevation(+0x3E)` and `Field_JumpCamera` unless bit 3 |
| `Field_JumpIn` | `0x52FAC0` | 0x12 | step 3: `jmp Field_JumpInSteps[+3]` |
| `Field_JumpIn0` .. `Field_JumpIn3` | `0x52FAE0` .. `0x52FB50` | 0xF, 0x13, 0x38, 0x5 | FE2's `0x536290`, `0x5362D0`, `0x5363C0`, `0x536440` (a tail jump) |
| `Field_ContentState` | `0x52FBB0` | 0x17 | state 11: `Field_ContentSteps[+2]`, then `Sprite_ScriptTick` |
| `Field_ContentTake` | `0x52FBD0` | 0x1B4 | the field object FS `+0x139` names: its flag (`+5`, bank `0x9040CC`) set - message 1; `+0x18` 0xFF - zenny `+0x19 * 40`; else an item: name, `Inventory_Add`, message 2 or 3; its animation 1 unless `+0xB` bit 0; `Field_Request` 2 |
| `Field_ContentEnd` | `0x52FD90` | 0xF7 | after message 3 the object back to animation 0; else removed (`+0xB` bit 0) or its cell set to 0x10; state 0xC with `Field_InputFlags` bit 5, else the pose and state 1 |
| `Field_AreaRunState` | `0x52FE90` | 0x14 | state 12: area 0x68 `Area104_LeaderRun`, else `Area121_LeaderRun` |
| `Field_GiveZenny` | `0x5307C0` | 0x3A | sound 0x106, the amount printed into `Text_Records`, message 5, `Field_Request` 2, `Zenny_Add(amount, 0)` - the sibling's `Field_GiveZenny` (PSX `0x801B6E50`), read the same |

### 1.3 The cells, the exit, the placements, the pending jump

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `Field_CellAroundLarge` | `0x531120` | 0x420 | al: for a sprite off the cell grid (`+0x34` or `+0x38` not 0), rows of two or three cells ahead by its facing searched for the code (`0x903850` / `0x903852`); a match turns an even facing along the row; the both-fractions shape then tries `Field_CellAroundSide` two facings either side |
| `Field_CellAroundSide` | `0x531540` | 0x116 | al: two cells on one side (x for 1 / 5, z for 7 / 3); a match sets the facing |
| `Field_GatewayExit` | `0x531820` | 0xF9 | al: the area's exit from `Field_GatewayExits` (area 0xBD: `Field_GatewayExits189` by `Cond_ByteFF`), set pending: area `0x937F82`, kind 4 `0x905B88`, x `0x903860`, z `0x90384C` |
| `Party_PlaceAtSlots` | `0x532C10` | 0xF6 | members to the slots `0x7E06E0`; the camera (`Field_Kind2X` / `Z`) to the leader with the height per frame in `MoveScript_FAWord` |
| `Party_ScriptTicks` | `0x532D10` | 0x38 | each member current and `Sprite_ScriptTick` |
| `Party_PlacesByList` | `0x532D50` | 0x17A | x and z reassigned by the second list `0x904065`, grounds; then the drop-in or each member's formation pose |
| `Field_LeaderPlaceOffset` | `0x533690` | 0xCF | the leader at (x, z) set off by twice (thrice) `Field_PlaceOffsets[direction >> 1]`, once more for a later member with `+0x70` |
| `Field_PendingJumpTurn` | `0x5338B0` | 0x33 | pending jump 1: `Field_PendingRelease(2)` facing 5 / 3, else (3) |
| `Field_PendingJumpKind4` | `0x5338F0` | 0x9 | pending jump 2: `Field_PendingRelease(4)` |
| `Field_PendingDrop` | `0x533900` | 0x92 | pending jump 3: the next member (`Field_PendingNext`) posed, 0x7D0 above its ground, falling (`+0x20` = -8), state 2 / 3 / 3 |
| `Field_PendingNext` | `0x5339A0` | 0xA3 | al: member `0x904EF2` made current once the one before stands (state 1); the flags' bits cleared, the jump over at the count |
| `Field_PendingRelease` | `0x533A50` | 0x150 | al 0 / 1 / 2: member `0x904EF2` released into the kind's four state bytes (`Field_PendingStates`) once the one before stands or its bits say so |

**Tables named** (`[[data]]`, counts to the next table): `Field_JumpSteps`
`0x66099C` (4), `Field_JumpOutSteps` `0x6609AC` (5), `Field_JumpInSteps`
`0x6609C0` (4), `Field_ContentSteps` `0x660A1C` (2), `Field_FormActions`
`0x660A44` (19), `Field_GatewayExits` `0x660AB8` (10 records),
`Field_GatewayExits189` `0x660B08` (2), `Field_PlaceOffsets` `0x660B40` (4
pairs), `Field_PendingStates` `0x660B70` (5 records).

## 2. Divergence

None: every function is a faithful replacement, so no `DIVERGENCE.md` entry
is owed. Where Capcom's code would jump through a table entry that is not
code or walk its stack past a table, ours aborts with a message (the round
nine rule; section 7 lists each). One place ours cannot reproduce what
Capcom's leaves: `Field_PassageTrigger`'s stack record (section 7, L3) - ours
zeroes the 0xA4 bytes Capcom's leaves as the frame held; nothing of ours
reads them, and what the event hooks read of that record is FE2's to settle.

## 3. The arguments pushed with leftovers

Where the original pushes a byte or word in a register whose upper bytes are
the caller's or a callee's leftovers, ours passes the value and the fuzz
lists the callee with the width its code reads (each read, capstone
2026-09-29) - the brief's "masks" note, FH's stand-ins' first use:

| Callee | Masks | The read |
|---|---|---|
| `0x52CFE0` | byte, whole, word, word | `+0x34` `and eax, 0xFF`; the slot handed whole to `Gfx_CommitPrim`; `+0x1F` / `+0x11` `movsx` of x and y. Pushed from `movzx cx, byte` / `lea` over the caller's `esi` (`FieldPanel_DrawKindRow`, `FieldPanel_DrawTotal`) |
| `Text_DrawAt` | word, word, byte, byte, the text (16 bytes hashed) | the pen words; `0x516B70`'s colour and count bytes (BE7's reading) |
| `Menu_DrawBox` | the colour a byte | `colour & 0xFF`; the style byte pushed as `eax` over the entry's `eax` |
| `Sprite_EnsureAnimation` | byte | `Sprite_SetAnimationAt` reads `dl` (the precedent: `area_w1f`, `boss_sf`, `magic_s14`) |
| `Item_NamePtr`, `Inventory_Add` | bytes | the category and id low bytes; `Field_ContentTake` pushes stack dwords whose upper three bytes it never wrote |
| `AreaMap_SetByte` | word, word, byte | `s16` x and z, the value's byte |
| `MapView_SetElevation` | word | only the low 16 bits reach its sum and the sign extension |
| `Field_CellHasEvent` | word, word | `AreaMap_ByteAt`'s words |
| `Crt_sprintf` | three words, the format hashed | the standard row lists four; at the three-argument calls here the fourth is the caller's frame |

## 4. The fuzz (`field_e1_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=field_e1`, `Group::field`, 6,000 rounds a
function. Shapes: the 21 leader-state and jump functions `kSprite` (SC and
FS an `ObjTrio` record two times in three - they run on the leader), the
helpers with arguments or an answer `kCall` (`ret_mask 0xFF` on the seven
that answer al: `Inventory_Holds38To4DAt99`, `Field_PathClear`,
`Field_CellAroundLarge`, `Field_CellAroundSide`, `Field_GatewayExit`,
`Field_PendingNext`, `Field_PendingRelease`), the rest `kState`. The five
dispatch tables are `DataTable`s; `sprite_span` is not used (the four
dispatchers' tables differ, 4, 5, 4, 2): the seed draws each one's byte
below its own table (section 7.9's advice).

**Callees beyond the standard set:** the re-listings of section 3; the
group's own called by E8, by name (`Field_GiveZenny`,
`Field_CellAroundSide`, `Field_PendingNext` - which makes a member current
when it answers, as the real one does - and `Field_PendingRelease`); FE2's
fourteen by raw address (section 8). Louder stand-ins where the caller reads
back or a branch needs it: `0x52CFE0` answers the packet cursor and moves it
0x1C on (the real one commits); `0x52CED0` answers near the thresholds
`FieldPanel_DrawTotal` compares, never 0xFFFF (section 7, L1);
`MapView_GroundAt` answers within 0x48 of the leader's height two times in
three and `Field_WayBlocked` open seven in eight, so `Field_PathClear` walks
its steps; `AreaMap_ByteAt` answers the searched code a third of the time;
`Item_NamePtr` a name in the harness's text buffer; `Field_ObjectTrigger`
notes the four bytes `+0x86..+0x89` of the record it is handed (the pointer
itself is the caller's frame).

**Regions beyond field mode's:** the inventory's lists past `0x904160`
(0x280), the message cells `0x7DEE20` (0x60), `0x903860` (0x10),
`0x937F80` (8), the slot positions `0x7E06E0` (0x20), `0x904AA0` (0x50: the
formation `0x904AAC`, the bits `0x904AE5`), `0x92BF18`, `0x904EF0`,
`Text_Records` (0x40), `0x939A28`, `0x7E1BE0` (`Cond_ByteFF`).

**Seeds:** the member count 0..3 (1..3 two times in three), the pending
member below the count, SC `+8` 0..7 mostly (to 11), the party set below 19
with bit 7 half the time; per function: the 22 ids at 99 placed then one
count or id moved off (the 21 / 22 boundary), the blink byte, the leader's
height, input held 0 half the time, `Field_Request` 0 / 1 / 2, `+0x12A`
0xFF, each dispatcher's byte below its table, `+5` and the script flags'
bit 3, the held bits 12 / 14 and the button map, the content object's `+5`,
`+0x18` (0xFF half the time), `+0xB`, its fractions 0, the message word 3,
`Field_InputFlags` bit 5, area 0x68, the sprite's fractions 0 or not (the
three shapes of `Field_CellAroundLarge`), the gateway's flags, party set
0xC, area 0xBD or a listed area, `Cond_ByteFF`, the leader's id and the
second list from the members' ids, the camera near the slot, the members'
`+0x70`, the pending members' state `+1` 1 and facings 5 / 3. **Arguments:**
the kind 0xFF / 0x16 / small, the row below 4, the message id or 0xFFFF,
`Field_PathClear`'s point within six half units of the leader, the zenny
amount, the searched code, the side's direction 1 / 5 / 7 / 3, the pending
kind 2..4.

**Disturbance** (the group's case 14, from the hash only): FS `+0x137`,
`+0x139` and the content object's `+5` / `+0xB`, the member count, the
pending member, the leader's x or z and height, SC and FS to a member, SC
`+8`, the message word, a member's state `+1`.

Result (this worktree, 2026-09-29), `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=field_e1`, exit 0:

    field_e1 self-test: 270000 rounds over 45 functions (6000 each), 933908 calls to the stand-ins, 0 MISMATCHES; 23520 bytes of state (47 regions) and the stand-ins' log compared

Every recorder was called and every table entry reached (the coverage lines;
`Field_FormActions`' nineteen 140..182 times each).

STAR_RESULT

## 5. What the cut and the tool said, settled

- **The tool's extents are right for all 45**; 22 cut sizes differ by
  padding only (`0x52EC20` is 0x15D, not the catalogue's 0x160; the hidden
  starts' "cut" sizes run to the next 16-byte boundary).
- **Hidden starts:** `pc_hidden` puts `0x52F4F0` in `0x52EC20`, eighteen in
  `Sprite_TurnSense` `0x52F570` (ours) and three in `Field_PendingJump`
  `0x533760` (ours). None is inside its host's code: `Sprite_TurnSense` is
  0x41 bytes (`inventory_ops.cpp`), `Field_PendingJump` 0x18
  (`field_event.cpp`), `0x52EC20` ends at `0x52ED7D`. Each start is reached
  by address (a `.data` table) or by a call, so each is a function of its
  own; `entries_logic.txt`'s `0052F570 70` line covered `0x52F5C0`, fixed by
  adding `0052F570 41` (section 11).
- **Nothing in the band that the cut does not list**, and no cut start is a
  case of another function.
- **Scenario_harness_fh's thirteen** include two of this group's: `0x52F980`
  (`Field_JumpOut`) and `0x52D880` (`Inventory_Holds38To4DAt99`), copied
  from the image by its self-test - so `FieldE1_Inject` runs after
  `ScenarioHarnessFh_Inject` in `inject_all.cpp` (before `DrawPool_Grow`).
- **The brief's "every function is called from code that is ours"** does not
  hold for eighteen: the ten panel functions (their callers `0x466BE0..` and
  `0x528A90..` are in no group of the cut) and `Party_PlaceAtSlots`,
  `Party_ScriptTicks`, `Party_PlacesByList` (`0x496130..0x4961F9`, a field
  function outside `GameMode_Field`'s 0x1BC bytes - the tool names its host
  `GameMode_Field` from the catalogue extent). Listed for the coordinator
  (section 8).

## 6. Controls

CONTROLS_TABLE

## 7. Latent defects (Capcom's, described, not fixed)

- **L1 `FieldPanel_DrawTotal`: a total of 0xFFFF walks the stack.** The last
  threshold is 0xFFFF and the loop continues while the total is at or above
  the threshold, so a total of 0xFFFF passes all thirteen and goes on
  comparing the frame's bytes as words (none can exceed it) until it leaves
  the stack. The total is `0x52CED0`'s sum of 32 kinds' points
  (`0x52CE60`: a record's word when the count reaches its byte `+3`, else a
  share of it) - whether ordinary play can reach 0xFFFF depends on the
  records' words, not read here. Ours aborts with a message.
- **L2 `Field_FormActionState`: `Field_FormActions` indexed unchecked** by
  `0x90412C & 0x7F`: the table has 19 entries and `Field_EncounterAreas`
  (data) follows, so a set 19..127 jumps into data. Ours aborts. The same
  index reaches `Field_ActionBySet` (event_leader.md D59's class).
- **L3 `Field_PassageTrigger`: a record of the frame's bytes.** Of the 0xA4
  bytes handed to `Field_ObjectTrigger` only `+0x86..+0x89` are written;
  `Field_ObjectTrigger` tests `+0x89` bit 6 (the word's high byte, defined)
  and hands the record to `0x56E020` or the chapter's hook slot 1, which may
  read more of it. Ours zeroes the record (section 2); what the hooks read is
  FE2's and the chapters' to say.
- **L4 `Party_PlaceAtSlots`: no member with the leader's id** reads the
  record after the last member (with three, `0x803164..`, past `ObjTrio`)
  for the camera's x, z. `Party_PlacesByList` likewise takes the word past
  the saved ones (`0x903850 + 4 * count`) for a member whose id is in none
  of the list's slots.
- **L5 the member count is trusted**: `Party_PlaceAtSlots`,
  `Party_PlacesByList`, `Field_LeaderPlaceOffset` and the pending jump index
  `ObjTrio` by it and by `0x904EF2` unchecked; above three they write past
  `ObjTrio` (the class every field group meets; reproduced).
- **L6 `Field_ContentTake`'s category and id** reach `Item_NamePtr` and
  `Inventory_Add` as stack dwords whose upper bytes it never wrote; both
  read the low byte only, so it is harmless (listed for the record).
- **L7 the dispatch bytes** (`Field_JumpState` `+2`, `Field_JumpOut` /
  `Field_JumpIn` `+3`, `Field_ContentState` `+2`) are unchecked; past their
  tables they run on into the next tables' code (to `0x660A24`), then data.
  Ours reads in place and aborts at a word that is not code.
- **`Field_PendingRelease` / `Field_PendingNext` shift by the member number
  mod 32**; at 0 the "bit n - 1" is bit 31 of a 16-bit word, never set. Not
  a defect, a shape.

## 8. Calls across groups

**Out, raw** (`field_e1_callees.h`; the coordinator rebinds after FE2
merges):

| Address | Owner | Called by |
|---|---|---|
| `0x56D6B0` `Field_ObjectTrigger` | FE2 | `Field_PassageTrigger` |
| `0x535FC0`, `0x535FE0`, `0x536050`, `0x5360C0`, `0x536130`, `0x536170` | FE2 | `Field_JumpBegin`, `Field_JumpOut0..4` |
| `0x5364D0`, `0x536550`, `0x5365D0` | FE2 | `Field_JumpAir` |
| `0x536290`, `0x5362D0`, `0x5363C0`, `0x536440` | FE2 | `Field_JumpIn0..3` |
| `0x52CF60`, `0x52CFE0`, `0x52CE60`, `0x52CED0`, `0x468950` | nobody (engine rows) | the panel draws |

**In, from outside the group** (for the rebinding pass): ours -
`Field_SwapGather` / `Field_SwapExchange` and `Field_PassageTake`
(event_leader: `kPathClear`, `kGiveZenny`), `Field_CellPickup` and the
field-hidden fuzz (field_hidden: `kFoundZenny`), `Field_CellAround` and
`Field_LeaderCellEvent` (event_ops: `kCellAroundLarge`, `kExitGateway`),
`FieldMenu_TopBarInput` (menu_lists: `kExitGateway`), `Field_PartySetUp`
(field_event: `position_alt`), `Field_LeaderFrame` / `Field_PendingJump` /
`Field_PassageState` through their tables (read in place: no rebinding);
this wave - FC2's `0x46D180` calls `Field_GiveZenny` (FH's 7.6); Capcom's -
the panel states `0x466BE0..0x4670C0` and `0x528A90..0x52A420`, the
field-mode code at `0x496130`, `0x496135`, `0x49617E`, `0x4961F9`, the
PartyAction run tables `0x65F998..` (`PartyAction_ScriptEnd`, 48 cells), the
seventeen Capcom sites in `0x51C270..0x5251C6` that call `Field_GiveZenny`.

## 9. The live route

Of the 45, **three are entered by both recorded routes**
(`analysis/calltrace/reach_whelp` and `reach_dragon`, all original,
2026-09-29 at `979a567`): `Party_PlaceAtSlots` (frames 1076 / 4195),
`Party_ScriptTicks` (1076 / 4195), `Party_PlacesByList` (1077 / 4227), each
called from `0x496130..0x496183` - the coordinator's frame-hash A/B covers
exactly these. The other 42 are fuzz-only: no recorded route opens a panel,
jumps, takes a field object's content, finds zenny, walks off-grid past a
code cell, leaves by a gateway or runs a pending jump.

## 10. The rebinding

REBINDING

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's list (checked first, no duplicates): the 24
hidden starts with the extents above, `0052EC20 15D` (the existing line said
0x160, through padding), and `0052F570 41` (`Sprite_TurnSense`'s own extent;
its `70` line covered `0x52F5C0`). The other 20 had lines already, each the
tool's extent.

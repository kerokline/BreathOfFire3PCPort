# Field-side hidden functions: two member states, a party action, poses, Ex and the encounter's driver

**Status:** IN PROGRESS (2026-09-25) - fifteen functions ours
(`src/game/field_hidden.cpp`, shadow name `field_hidden`), each fuzzed
headless against a copy of Capcom's with every call re-aimed at a recorder:
30,000 rounds, 0 mismatches. 83 negative controls were planted: 81 were refused by a count (exit 3), each in
the one function it touches, and two were changes that change nothing. Not yet through a live check: the
coordinator's worldmap and combat A/B after the merge.

Group DE of the eighth round ([`takeover-queue-round8.md`](takeover-queue-round8.md)).
Every function is a faithful replacement, so no `DIVERGENCE.md` entry is
owed. Two latent defects of the original are described (section 7) and kept.

## 1. How they are reached

The fifteen fall into four families; eight tables are named for them.

**Two party-member states.** `Field_MemberFrame` (`0x51AC50`, ours,
`member_sprites.cpp`) jumps through a 9-entry table at `0x65F960` by
`Sprite_Current +1`. That table is now **`Member_States`**. Entry 3 is
`0x51BA60` and entry 7 is `0x51BBD0`. `pe_funcs.py` folded both into
`Field_DirectionTo`, the function before them; neither is part of it.

**A party action.** The party sets' field actions form a three-level
dispatch:

1. `0x52FB60` (group DC's) calls `[0x6609D0 + (0x90412C & 0x7F) * 4]`,
   that is, one handler per party set.
2. Each set's handler jumps through a forms table by the u16 `+0x2C`.
3. Each form's handler jumps through a states table by `+2`.

This group has set 5's handler and one of its forms:

| Level | Address | What it is |
|---|---|---|
| Set 5 (entry 5 of `0x6609D0`) | `0x51F1B0` | `PartyAction5_ByForm`: jumps through **`PartyAction5_Forms`** `0x65FC18` (3 entries: `0x51E910`, `0x51ED10`, `0x5226D0`) |
| Form 0 | `0x51E910` | `PartyAction5_Form0`: jumps through **`PartyAction5_Form0States`** `0x65FBC0` (3 entries) |
| State 0 | `0x51E930` | `PartyAction5_Form0Begin` |
| State 1 | `0x51EAF0` | `PartyAction5_Form0Resolve` |
| State 2 | `0x51DA30` | `PartyAction_Finish`, shared |
| Called by state 1 | `0x51EBD0` | `Field_CellPickup` |

Sets 0, 1 and 2 have the same shape. Their dispatchers are `0x51C760`,
`0x51CC20` and `0x51D730`; their forms tables are `0x65FA00`, `0x65FA5C` and
`0x65FAD4`. Their form-0 state tables are named here as well, because
`PartyAction_Finish` is reached through them:

- **`PartyAction0_Form0States`** `0x65F9C0`;
- **`PartyAction1_Form0States`** `0x65FA18`;
- **`PartyAction2_Form0States`** `0x65FA74`.

A scan of `.data` finds `0x51DA30` in 18 dwords. In the four tables named
here it is entry 2; the other 14 tables were not read.

**The pose helpers.** `0x589160` copies a sprite's frames into a buffer.
`0x589110` and `0x5891C0` are its two callers: the battle objects' poses
(22 call sites, round seven's `battle_windows.cpp`) and the battle intro's
party steps (`inventory_ops.cpp`). Round seven named all three "missed by
the cut" ([`inventory_ops.md`](inventory_ops.md) section 5).

**The event op Ex** `0x5898D0`. It was named already (`EventOp_Ex`, a
`hypothesis` since 2026-09-22) and is taken now.

**The encounter placement's driver.** `0x591F30` (`Encounter_Place`) is
called by `Field_EncounterDue` (`event_ops.cpp`, as `(4, 0)`), and in turn
calls `0x592570` (`Encounter_PickRow`), `0x5925A0` (`Encounter_FillSlots`)
and round seven's placement functions. Two tables are named for it:

- **`Encounter_Rows`** `0x8C5580` (bss, loaded with the area);
- **`Encounter_SlotChance`** `0x669CB0`.

## 2. What is ours

| Function | Address | Bytes | What it does |
|---|---|--:|---|
| `Member_ResumeUnlessHeld` | `0x51BA60` | 0x14 | `+1 = 1` unless bit 10 of `Field_ScriptFlags2` |
| `Member_EffectState` | `0x51BBD0` | 0x115 | By `+2`: 0 waits for the leader's `+0x137` to leave 8, then back to state 1; 1 starts an effect object (kind 6, the member's `+0x2E` / `+0x30`, `MoveScript_EffectArg[Field_State +0x89]`); 2 waits for it to end. `Sprite_ScriptTick` every frame |
| `PartyAction_Finish` | `0x51DA30` | 0x66 | By `+0xB`: tick once, or tick once then `Sprite_SetAnimation(+8)`, or tick until `Field_Request` clears; then `+0x2B = 0`, `Field_State +0x137 = 0` |
| `PartyAction5_Form0` | `0x51E910` | 0x12 | `jmp [0x65FBC0 + +2 * 4]` |
| `PartyAction5_Form0Begin` | `0x51E930` | 0x1B1 | An even facing turned one eighth back; with nothing there (`0x51C390`), one eighth on instead; with nothing there either, back again - so it ends on a diagonal with something ahead, the one turned back tried first (and kept when neither has); a steep slope ahead gives animation `(d-1)/2 + 0x46` and goes straight to state 2; else `+0x2B = 1`, the two side probes, sound `0x100 + form`, animation `(d-1)/2 + 0x42`, `+0xA = 5` |
| `PartyAction5_Form0Resolve` | `0x51EAF0` | 0xDB | After 5 frames: the object two steps ahead gets `+0x80` bit 0; `Field_CellPickup` on that cell, else on the next cell in x, then in z (only across a fraction); state 2 |
| `Field_CellPickup` | `0x51EBD0` | 0x11F | Cell `0xF2`: an effect, 3 in 16 zenny (2, or 5; ×10 under `Field_InputFlags & 6` one time in four). Cell `0xF8`: item `0x56` of category 0 into the inventory with its message. Both clear the cell and answer 1 |
| `PartyAction5_ByForm` | `0x51F1B0` | 0x13 | `jmp [0x65FC18 + u16 +0x2C * 4]` |
| `Sprite_PoseFromSet` | `0x589110` | 0x45 | Copy the frames; the same animation restarts its script at `+0x58 - 2`, another starts it at 0 |
| `Sprite_CopyFrames` | `0x589160` | 0x59 | `Sprite_SetFrameQueueUpload`, then `size & 0xFFFF` bytes from `+0x50` to `buffer + +5 * size`, and `+0x50` pointed there |
| `Sprite_AnimFromSet` | `0x5891C0` | 0x2D | Copy the frames, `+0x4B` = the animation, `Sprite_ScriptStart(position)` |
| `EventOp_Ex` | `0x5898D0` | 0x9F | An effect object from the op's 7 bytes: kind, cell and half-cell of x and z, its ground, `+0xB` |
| `Encounter_Place` | `0x591F30` | 0x1AB | The facing, the centre, a row and its slots, the placement, then every member and enemy on screen (section 5) |
| `Encounter_PickRow` | `0x592570` | 0x2B | A row by weight: `(Rand & 0xF) + 1` against a byte running sum |
| `Encounter_FillSlots` | `0x5925A0` | 0x5D | Eight slots from a row: a kind not `0xFF`, active by `(Rand & 0xF) + 1 <= Encounter_SlotChance[k]` |

Every `evidence` field in `symbols.toml` gives the extent read, the calls and
the callers. The sizes are to the last instruction, capstone, 2026-09-25.

**The two dispatchers read their `.data` tables in place with the index
unchecked** (known-defects.md D59), as the original does. This follows the precedent of the CJ
group's `FxRing_Phases` ([`magic_fx_reached.md`](magic_fx_reached.md)). An
index past the table reads the next table's dwords, the same on both sides.
That is not a stack table, so there is nothing to abort.

**Callers ignore the state handlers' `eax`.** `Field_MembersFrame` and
`0x52FB60` both ignore it, so the handlers are `void`. `Sprite_PoseFromSet`'s
`eax` is `Sprite_ScriptStart`'s. Round seven's `BattleObj_PickPose` passes it
on, and it was never a value there.

## 3. Found on the way

- **`PartyAction5_Form0Resolve` passes the cells with uninitialised upper
  halves.** It writes the point's two dwords into its locals and reads the
  cell back from `+0xA` and `+2`, so the upper 16 bits of each argument
  come from its own uninitialised stack. Every callee reads only the low
  word:
  - `AreaMap_ByteAt` takes `short`;
  - `0x524870` uses `movsx word`;
  - `0x5728D0` uses `movsx`.

  Ours passes the cell zero-extended. The recorders compare the low words.
- **`Encounter_Place` reuses its argument slots.** It writes each member's
  "wide" flag into the low byte of its own first argument's slot and pushes
  that whole dword as `Encounter_OnScreen`'s size, so the upper bytes are
  the caller's. It also keeps the member counter in the second argument's
  slot. `Field_CellPickup` likewise keeps the zenny amount in its `z` slot.
  No caller reads any of these slots again. Ours passes the same dword
  (`facing & 0xFFFFFF00 | wide`); `Encounter_OnScreen` reads the byte.
- **An odd facing is not masked.** In `PartyAction5_Form0Begin`, an odd
  `+8` is kept as it is and indexes `Field_DirectionSteps` unchecked. `+8`
  of 8 or more reads past the 8 rows. `PartyAction5_Form0Resolve` indexes
  it the same way (known-defects.md D64).
- **`Inventory_Add` is pushed a fourth dword, 0**, by `Field_CellPickup`'s
  `0xF8` path. It reads three.
- **A store that changes nothing.** `Member_EffectState` sub-state 0 stores
  `+2 = 0` into a byte it has just read as 0. Negative control E2 (section
  4) found it.
- The sound `Field_CellPickup` plays for an item, `0x106`, is also the first
  thing `0x5307C0` plays for zenny.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=field_hidden`
(`src/game/field_hidden_fuzz.cpp`). It makes 15 byte-copies:

- Every call out is re-aimed at a recorder, using `CloneCall` with
  `expected`.
- Both tables, `PartyAction5_Form0States` and `PartyAction5_Forms`, have
  their three entries swapped for recorders, and are put back afterwards.

**Each round** starts from random bytes over these regions (about 17 KB):

- ObjTrio;
- `Sprite_Current` and `Field_State`;
- the field flags and `Field_Kind2X` / `Z`;
- the 20 effect objects;
- `Sprite_Objects` and `Sprite_ObjectsExtra`;
- the scratch bytes and the fight's centre;
- `Field_Request`;
- `Text_Records`' first record;
- the party lists;
- the placement's state and scratch sprite;
- the enemy slots and the members' spots;
- `Encounter_Rows` and the enemy kinds;
- `MoveScript_EffectArg`, `Field_DirectionSteps` and 8 rows past it;
- `Encounter_SlotChance`.

The fuzz also has buffers of its own: frame sources, a copy buffer, an item
name, an op and a row.

The pointers and indices are then put back inside their arrays, and each
branch's boundaries are seeded:

- the flag bit;
- sub-states 0..3 and `0xFF`;
- the leader at 8;
- an effect ended;
- forms and states 0..2;
- odd and even facings, and facings above 7;
- the slope at `0x40` / `0x41`, as a short and with upper bits;
- ground equal to `+0x3E` and one either side of it;
- points with and without a fraction;
- cells `0xF2` / `0xF8`;
- `Rand` nibbles 0, 12..15;
- the input flags' bits 1 and 2;
- the animation equal or not;
- sizes 0, 1 and small, with upper bits;
- a copy onto its own source;
- `op[2]` / `op[4]` zero;
- facings 0..5, `0x80` and `0xFF`, with upper bits;
- `keep_centre` zero;
- member ids 2 and 6;
- placed counts 0..3;
- row weights that wrap;
- `0xFF` kinds;
- slot chances 0..16.

**The recorders are not quiet.** Each one moves a byte that its caller reads
again after the call:

- `Sprite_SetFrameQueueUpload` moves the sprite's `+0x50` and `+5`.
- `Sprite_CopyFrames` moves `+0x4B` and `+0x58`.
- `MapView_SlopeAt` sets the scratch flag.
- `AreaMap_Elevation` scribbles on every effect's `+0x38`.
- `Item_NamePtr` answers `Text_Records` itself a dword or two on, one time in
  four.

A general disturber also moves one of 24 watched bytes three calls in four.
They include:

- `Sprite_Current` and `Field_State`;
- `+8`, `+2`, `+0xB`, `+0xA`, `+5`, `+0x4B`, `+0x2C`, `+0x3E`, `+0x50` and
  the position;
- `Field_Request`, the scratch flag and the leader's `+0x137`;
- the facing, the placed count, the members' spots and an enemy slot;
- a row weight or slot chance.

Every byte answer comes with the rest of `eax` random, so a missing `al`
mask shows.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=field_hidden`, exit 0:

    field_hidden self-test: 30000 rounds over 15 functions (2000 each), 84679 calls to the stand-ins, 0 MISMATCHES
    field_hidden coverage: effect state sub-states 410/664/480/446, finish 490/516/477/517, form-0 states
      642 709 649, forms 702 666 632; begin steep 445 flat 1555; resolve pickups 1889; cell 1 1698 0 302
      (zenny 490, tenfold 61, items 550); poses restarted 528; copies 1518; place 1 472 0 1528 (popped 1526,
      on-screen asked 1526, slots dropped 806); rows 1+ 1152 0 848; slots 1+ 1989 0 11

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 1277 ours` and no
mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`DE/controls.py` in the session scratchpad)
planted each one alone: replace, build, self-test, revert. The table gives
the rounds that refused each one, of 2,000.

| | Planted | Refused in |
|---|---|---|
| R1 | ResumeUnlessHeld: bit 0x200, not 0x400 | 986 |
| R2 | ResumeUnlessHeld: state 2, not 1 | 1496 |
| E1 | EffectState: waits while the leader is 9, not 8 | 200 |
| E2 | EffectState: sub-state 0 keeps +2 | not refused: changes nothing (section 3) |
| E3 | EffectState: effect kind 7 | 499 |
| E4 | EffectState: the effect argument sign-extended | 224 |
| E5 | EffectState: sub-state 2 waits for a non-zero +0 | 470 |
| E6 | EffectState: no tick above sub-state 2 | 446 |
| E7 | EffectState: +0x30 from the member's +0x2E | 499 |
| E8 | EffectState: no effect slot leaves +2 | 156 |
| F1 | Finish: sub-state 0 ends on a zero tick | 490 |
| F2 | Finish: the animation +9 | 247 |
| F3 | Finish: the request test inverted | 994 |
| F4 | Finish: +0x137 of Sprite_Current, not Field_State | 489 |
| F5 | Finish: sub-state 1 goes to 3 | 247 |
| B1 | ByForm: form 2 read as 0 | 632 |
| G1 | Form0: state 2 read as 0 | 649 |
| N1 | Begin: an odd direction turned | 2000 |
| N2 | Begin: the first turn forward | 979 |
| N3 | Begin: steep above 0x41 | 233 |
| N4 | Begin: the slope compared unsigned | 478 |
| N5 | Begin: a steep slope moves +2 by one | 445 |
| N6 | Begin: the side probe clears at equal ground | 126 |
| N7 | Begin: sound 0x101 + form | 1555 |
| N8 | Begin: the steep animation 0x45 | 445 |
| N9 | Begin: (d - 1) / 2 as an arithmetic shift | 18 |
| N10 | Begin: the second side probe in direction 4 | 1555 |
| N11 | Begin: the scratch flag read before the slope | 457 |
| N12 | Begin: +0xA = 4 | 1555 |
| N13 | Begin: +0x2B not set before the probes | 1286 |
| V1 | Resolve: the x fraction taken from z | 438 |
| V2 | Resolve: objects below 0x1F in the first array | 11 |
| V3 | Resolve: bit 1 of the object's +0x80 | 545 |
| V4 | Resolve: z tried even when x found | 77 |
| V5 | Resolve: one step ahead, not two | 1154 |
| V6 | Resolve: the second array from 0x1D | 86 |
| V7 | Resolve: +2 moved every frame | 830 |
| C1 | CellPickup: zenny from 0xC | 114 |
| C2 | CellPickup: 5 from 0xE | 119 |
| C3 | CellPickup: tenfold on bit 1 only | 88 |
| C4 | CellPickup: tenfold on Rand & 1 | 39 |
| C5 | CellPickup: 12 bytes of the name | 550 |
| C6 | CellPickup: the two messages swapped | 550 |
| C7 | CellPickup: Field_Request = 1 | 534 |
| C8 | CellPickup: the zenny cell leaves +0xB | 938 |
| C9 | CellPickup: the cell not cleared | 1698 |
| C10 | CellPickup: the second effect kind 2 | 490 |
| C11 | CellPickup: the name copied backwards | 155 |
| C12 | CellPickup: the effect-free test dropped | 128 |
| P1 | PoseFromSet: restart at +0x58 - 1 | 663 |
| P2 | PoseFromSet: the whole animation dword compared | 528 |
| P3 | PoseFromSet: +0x4B read before the copy | 377 |
| K1 | CopyFrames: size & 0xFF | 36 |
| K2 | CopyFrames: +5 read as its bit 0 | 769 |
| K3 | CopyFrames: +0x50 left | 1997 |
| K4 | CopyFrames: copied last byte first | 74 |
| K5 | CopyFrames: the offset read before the upload | 409 |
| A1 | AnimFromSet: position 0 | 2000 |
| A2 | AnimFromSet: +0x4B not set | 1347 |
| X1 | EventOp_Ex: the z half from op[5] | 764 |
| X2 | EventOp_Ex: +0xB = op[5] | 1506 |
| X3 | EventOp_Ex: the elevation asked at (z, x) | 1507 |
| X4 | EventOp_Ex: the words stored after the call | 766 |
| L1 | Place: facing rotated above 4 only | 228 |
| L2 | Place: the centre to the whole cell | 491 |
| L3 | Place: keep_centre inverted | 2000 |
| L4 | Place: no early answer on no enemies | 474 |
| L5 | Place: wide for id 7, not 6 | 410 |
| L6 | Place: the members' margin 0x10 | 1142 |
| L7 | Place: the placed count kept | 599 |
| L8 | Place: the answer whatever is placed | 178 |
| L9 | Place: the camera aimed with the facing as first set | 189 |
| L10 | Place: the reachability asked even when placing failed | 549 |
| L11 | Place: the party count asked once | 948 |
| L12 | Place: the wide flag with a clean dword | not refused: changes nothing (the callee reads the byte) |
| W1 | PickRow: the roll without + 1 | 188 |
| W2 | PickRow: below the sum, not at most | 182 |
| W3 | PickRow: the last row when none | 9 |
| W4 | PickRow: the sum as an int | 8 |
| S1 | FillSlots: active below the chance only | 437 |
| S2 | FillSlots: the chance read before the draw | 9 |
| S3 | FillSlots: a 0xFF kind keeps its +0 | 1793 |
| S4 | FillSlots: +1 stored after the draw | 2000 |

The two not refused are not gaps. E2 removes a store of the value the byte
already holds. L12 passes `Encounter_OnScreen` a clean dword in place of the
first argument's slot, and the callee reads only its low byte.

The thin ones are thin by their branch's odds, not by a missing seed:

- N9, 18: the sign only matters at direction 0 on the steep path.
- V2, 11: object index 0x1E.
- W3 / W4, 9 / 8: no row reached, or a wrap before the roll.
- S2, 9: a chance moved during the draw.

## 5. The encounter placement, end to end

Round seven took the placement's parts
([`inventory_ops.md`](inventory_ops.md)). This group takes the driver that
calls them, so the whole path is now ours.

`Field_EncounterDue` calls `Encounter_Place(4, 0)`, which does the
following in order:

1. **The facing.** An argument of 4 or more rotates the facing byte
   `0x6BE070` one on (`& 3`). This is how every field encounter is made.
2. **The centre.** `0x903780` / `0x903784` are set to `Field_Kind2X` /
   `Field_Kind2Z`, to the half cell.
3. **The row and the slots.** `Encounter_PickRow` rolls a row from
   `Encounter_Rows` by weight. `Encounter_FillSlots` then fills the eight
   slots from it. Slots 0..2 always come (chance 16 of 16) unless their kind
   is `0xFF`. Slots 3..7 come 10, 8, 6, 4 and 2 times in 16.
4. **No enemies.** If no slot came, the answer is 0.
5. **The placement.** `0x904AAC` is set to the facing. Then
   `Encounter_PlaceParty`, `Encounter_PlaceEnemies` and
   `Encounter_EnemiesReachable` run, each only if the one before answered
   yes. A no from any of them makes the answer 0, but the rest still runs.
6. **On screen.** Inside a pushed GTE matrix:
   - `Encounter_AimCamera` aims the camera.
   - Every member must be `Encounter_OnScreen`, with ids 2 and 6 "wide".
     One off screen ends it with 0.
   - An enemy off screen is dropped, and the placed count goes one down.
   - A count of 0 answers 0.

## 6. For `analysis/calltrace/entries_logic.txt`

Seven lines are appended to the main checkout's copy, under a `group DE`
comment:

    0051BA60 14
    0051BBD0 115
    0051DA30 66
    0051E910 12
    0051E930 1B1
    0051EAF0 DB
    0051F1B0 13

The other eight were already listed, seven with the sizes read here
(`00589110 45`, `00589160 59`, `005891C0 2D`, `005898D0 9F`, `00591F30 1AB`,
`00592570 2B`, `005925A0 5D`). The eighth is wrong:

| Line now | Should be |
|---|---|
| `0051EBD0 360` | `0051EBD0 11F` |

The line as it stands runs over the unowned `0x51ECF0..` (the next form's
dispatcher and states).

Three host lines overlap ours. All three hosts are no group's:

- `0051D4E0 5B6` runs over `0x51DA30`;
- `0051E6C0 50B` runs over `0x51E910..0x51EBCA`;
- `0051EF30 57B` runs over `0x51F1B0`.

The merger should judge them as the earlier rounds' were.

## 7. Defects found (Capcom's, latent, kept)

Numbered D82 and D83 in [`known-defects.md`](known-defects.md).

- **A zenny cell dug with every effect object busy is cleared for
  nothing** (D82). In `Field_CellPickup`'s `0xF2` path, the zenny roll only
  happens once `Effect_FindFree` answers a slot. With all 20 objects taken,
  the path skips the roll and the `+0xB` store, but still clears the cell
  (`0x5728D0`) and answers 1. The chance is lost for good. The `0xF8` item
  path does not ask for an effect, so it cannot lose its item this way.
  Reaching it needs 20 live effect objects at the moment of the dig, which
  no route here does.
- **The row weights are summed in a byte** (D83). Weights over 255 in total wrap,
  and a later row can then be picked for a roll the earlier rows should
  have covered. The rows come from the area's data. Whether any area's
  weights sum past 16, let alone 255, was not checked.

## 8. What the fuzz did not reach, and what is left

- **Not reached:**
  - negative object indices from `Sprite_ObjectAt`, which the original
    would write below `Sprite_Objects`;
  - an effect index `+0xB` of 20 or more in `Member_EffectState`
    sub-state 2;
  - forms and states past the three-entry tables, which would call through
    the next table's dwords;
  - `Sprite_CopyFrames` onto the sprite's own `+0x50`.

  All four are faithful by construction: the same arithmetic, the index
  unchecked. They are not exercised, because the writes would land outside
  what the fuzz restores.
- **The routes:** by the round's trace, the worldmap route reaches the
  member states, the party action, `EventOp_Ex` and the encounter. The
  combat route reaches the pose helpers and the encounter (83 calls to each
  of its three). The coordinator's A/B after the merge is their live check.
  The `0xF8` item cell, the tenfold zenny, a party action on a steep slope
  and a no-enemy row are fuzz only, as far as this group knows.
- **Left original:** nothing. All fifteen are taken.

## 9. Other groups' addresses

Nothing here was bound, renamed or given an `impl`.

- **`0x6609D0`** is the party-set action table that DC's `0x52FB60` calls
  through, by `0x90412C & 0x7F`, unchecked below 128 (known-defects.md D59).
  - Its first twelve entries: `0x51C760` `0x51CC20` `0x51D730` `0x51DEB0`
    `0x51E8D0` `0x51F1B0` `0x51FA90` `0x520140` `0x520800` `0x521340`
    `0x521A70` `0x5220C0`.
  - It is DC's to name. This group names only the tables below it.
  - `0x52FB60` then calls `Sprite_EnsureAnimation(+8)`, `0x536730(0)` and
    sets `+1 = 1` when `Field_State +0x137` is 0.
- **`0x591F30` is misnamed by its caller.** `event_ops_callees.h` calls it
  `kPartyVisible`, "u8(u8, u8): moves Sprite_Current". It is the whole
  encounter placement (`Encounter_Place`). The raw address there is still
  right. A rename is event_ops' to make.
- **Three raw addresses in earlier modules are now ours:**
  - `battle_windows_callees.h`'s `kPoseFrom` is `Sprite_PoseFromSet`;
  - `inventory_ops_callees.h`'s `kSetAnimFrom` is `Sprite_AnimFromSet`;
  - `event_ops`' `kPartyVisible` is `Encounter_Place`.

  All three are correct as they stand.
- **`member_sprites_callees.h`'s `kMemberStates` is now named**
  `Member_States`. Its entries 2 (`0x525370`, DB's), 4 (`0x51BA80`),
  5 (`0x437CC0`), 6 (`0x51BAA0`) and 8 (`0x51BCF0`) are no group's here
  except DB's.
- **Nobody's callees**, called by raw address:
  - `0x51C390`, the party action's "something ahead" test (an object two
    steps on, or a `0xF2` cell there or across the fraction);
  - `0x524870`, an effect of kind `0x34` at a cell;
  - `0x5307C0`, the zenny found;
  - `0x5728D0`, a map cell cleared.
- **No address in another group looks misfiled.** DB's `0x525370` is
  `Member_States` entry 2 and the leader's too (`0x660920`), a field state
  under `0x525150`, as DB lists it.

# The movement commands and the party's moves (group L)

**Status:** IN PROGRESS (2026-09-22) - fifteen functions ours in
[`src/game/move_cmds.cpp`](../src/game/move_cmds.cpp), each read to its last
instruction and against its PSX twin where one is paired, fuzzed against
Capcom's at start-up (`BOF3X_SHADOW=move_cmds`, 0 mismatches) with 76
negative controls, 74 refused by a count and 2 that change nothing. **Through the live
batch `ab24`.** No divergence; two latent defects written down
([`known-defects.md`](known-defects.md) D14, D15).

**2026-10-10: a sixteenth, `MoveCmd_Move` `0x578C10`** - the last function
of the game's own code that was still Capcom's (section 8). Fuzzed (0
mismatches, 14 controls refused), self-tested narrow and wide; not live-run,
no state-hash run.

**The live batch, 2026-09-22 (`ab24`, `analysis/validate_ab24.sh`):** the whole third round - groups H, J, K, L and M, 137 functions, 466 ours - checked at once, original against ours: the field, new-game, field-menu and menu-screens capture pairs identical (4, 9, 7, 5 of each), the 9-minute attract 55 of 55 (and 55 of 55 against `ab22`'s ours), the same attract in English 55 of 55, the oracle identical at all 7,478 compared frames, the memory dump identical, and the frame hash identical on all 10,063 frames (`ab24_orig` / `ab24_oursb`, beside an original-vs-original pair identical on 10,062).

The third parallel round's group L ([`takeover-queue-round3.md`](takeover-queue-round3.md)):
the movement script's callees that were still Capcom's after
[`movement-script.md`](movement-script.md) - every caller there is ours - and
the demo's party reset. All fifteen are reached by the attract cycle (call
counts are `hidden_b`'s, `analysis/calltrace/hidden_b/bof3x.callcounts.tsv`).

## 1. The functions

| PC | name | bytes | PSX twin | calls (`hidden_b`) | what |
|---|---|---|---|---|---|
| `0x5190A0` | `Party_MoveMember` | `0x1F9` | `FUN_801A4088` (gap44, read on both) | 663 | a party member's move: the velocity record (op `D4`'s) of its slot filled - two tilts from the ground's slope one step ahead, and a turn |
| `0x5A7A70` | `Math_Ratan2` | `0x1B` | `0x8017AAB0` (call-anchored) | 438, all from `Party_MoveMember` | `fpatan` * 2048 / 3.14, truncated by the CRT's `_ftol` |
| `0x518B00` | `Sprite_SetPoseWait` | `0x16` | `FUN_801A3764` (gap44) | 83 | `[3] = [1]`, `[1] = 5` |
| `0x573400` | `MoveCmd_MoveKind2` | `0xED` | `FUN_801C6DF4` (call) | 41 | the kind-2 object's move |
| `0x5734F0` | `Kind2_Place` | `0x66` | `FUN_801C7014` (call-anchored) | 12 | the kind-2 object placed and grounded |
| `0x578C10` | `MoveCmd_Move` (2026-10-10, section 8) | `0xF5` | `FUN_801ACBA8` (call; not re-read) | not measured | a step's move of any other object: count, velocity, the rise to the ground |
| `0x578D10` | `MoveCmd_OpF7` | `0xA4` | `FUN_801ACD68` (call) | 6 | op `F7`'s timed move to a point on the ground |
| `0x578DC0` | `MoveCmd_HandlePosition` | `0xEA` | `FUN_801ACF30` (read on both) | 6 | where a handle's object stands, plus an attachment offset |
| `0x5792A0` | `MoveCmd_Attach` | `0xE8` | `FUN_801AD768` (call) | 12 | op `F8`, attach ([`movement-script.md`](movement-script.md) §2) |
| `0x579390` | `MoveCmd_Detach` (new name) | `0x1D` | `FUN_801ACEE0` | 6 | `+0x24` bit 5 off; `+2` off for the kind 1 |
| `0x5793B0` | `MoveCmd_AttachMove` (new name) | `0x92` | `FUN_801AD0A0` | 6 | the timed move to where the attachment puts the sprite |
| `0x579450` | `MoveScript_FindLabel` | `0x78` | `FUN_801AD8F0` (read on both) | 2,448 | a label's position |
| `0x57C310` | `MoveScript_Variable` | `0xD0` with its table | `0x8015C260` (gap9) | 2,448 | the flow ops' variables |
| `0x57C840` | `MoveScript_ObjectKind` | `0x52` | `0x8015CB38` (call) | 172 | what `Sprite_Current` is |
| `0x57CDC0` | `Sprite_ScriptPeek` | `0x4E` | `0x8015D48C` (gap4) | 2,808 | the frame script's position after its next step |
| `0x519890` | `Scena16_PartyReset` (new name) | `0x57` | none paired | 3 | chapter 16's call-table entry 0: the party emptied, member `0xA` joined |

PSX side read from the sibling's `analysis/ghidra/GAME_EMI0_80196800_decomp/`;
pairings from `analysis/pairs_propagated.json`. Every one is its twin term for
term but `Kind2_Place`, below. `symbols.toml`'s group L block has each one's
evidence; three of the old entries were wrong or thin:
`MoveScript_Variable`'s 13 and 14 were "unread" (13 is the constant 1, 14 is
case 0 again), `MoveScript_FindLabel`'s `0E` / `0F` test is mask `2` (bit 1,
not bit 2), and `MoveCmd_OpF7`'s body was unread.

Callers not ours: `Math_Ratan2` has 70 call sites, 68 of them outside
`Party_MoveMember` (battle and effect code the attract never reaches);
`Kind2_Place` has about 130 (overlay code); `MoveCmd_MoveKind2`,
`MoveCmd_Attach`, `MoveCmd_OpF7`, `Party_MoveMember` and
`MoveScript_ObjectKind` a few each in overlay code (`0x408EB0`, `0x417510`,
`0x41D430`, `0x578FA0`, `0x57C8E0`...). They reach ours through the patched
entry, like every other takeover.

## 2. What is kept as Capcom had it

- **`Math_Ratan2` is x87 in ours too.** `fpatan` works at 64 bits whatever the
  precision control says, then two `fmul` by doubles each round to the control
  word's precision (53 bits in game, `psx_gte_float.cpp`), then `_ftol`: a copy
  of the control word with round-toward-zero for one `fistp` to 64 bits, and
  the word put back. Ours is that sequence in inline assembly, so it rounds as
  the original under ANY control word - the fuzz checks three. Pi is taken as
  3.14 (the double at `0x5C4650` is `1 / 3.14`): a right angle is 1024.5
  before truncation, 1024 after; the PSX's integer `ratan2` has no such
  factor. `_ftol` leaves the high dword in `edx`; ours does not, and no caller
  reads it (all 70 sites use `eax`).
- **`Party_MoveMember`**: the slot is `(object - 0x802000) / 0xA4` with its
  low byte taken signed (4 when negative; a quotient of `0x100 + n` is slot
  `n`), and the object pointer is never dereferenced. The zeroed tilt axis
  still gets a velocity: from its value on entry to its value on entry, 0 -
  the targets are the sprite's `+0x64` / `+0x68` read before either is zeroed.
  The record's counts are `n << 1` and `n << 2` as bytes (wrapping for `n` of
  `0x40` and up). Directions 5 and 6 turn through `+0x1000` when the sprite's
  angle is past direction 3's. Any direction byte indexes
  `Field_DirectionSteps` and `Sprite_DirectionAngles` (callers pass 0..7).
- **`MoveCmd_MoveKind2`**: the divisor is a 16-bit product; every product of
  the step wraps; `MoveScript_F3Divisor` and the steps byte are read again
  after the ground; a speed of 0 (index 0, or past `Field_MoveSpeeds`) or 0
  steps divides by zero, as the original faults.
- **`Kind2_Place`**: `+0x85` is read after `ScriptContext_Reset` (which clears
  it). **The one difference from the PSX:** `MapView_SetElevation` gets
  `AreaMap_Elevation`'s whole dword, where the PSX passes it as a short; the
  high half can be non-zero (outside the map), and
  [`map_view.cpp`](../src/game/map_view.cpp) shows it cannot matter there.
- **`MoveCmd_OpF7`** asks for the ground even when nothing moves (the
  context's `+4` clear), and returns `Sprite_Current[8]` as found in `al` -
  `MoveScript_GroupF` clears `eax` straight after (`0x577AEA`); ours is `void`.
- **`MoveCmd_HandlePosition`** keeps `Sprite_Current` in the dword `0x92A0C0`
  (the PSX's scratch `0x1F800010`; `symbols.toml` also calls it
  `MapView_CornerPtr` for `MapView_Build`'s use) and reads it back from there.
  Without bit 7 a handle is `Sprite_ObjectsExtra + handle * 0xA4` - all seven
  bits; with it, the COUNT of type-`0x0A` objects before the `(h & 0x3F)`-th
  (D6's search; the total when there is none), as on the PSX. **`out[3]`** is
  the fourth dword of its own 16-byte buffer, which `MoveCmd_AttachOffset`
  never writes: an uninitialised stack dword in the original and on the PSX,
  **0 in ours**. Neither caller reads it (op `84` copies three dwords,
  `MoveCmd_AttachMove`'s copy is dead), and the fuzz does not compare it.
- **`MoveCmd_Attach`**: for the object kind 1 it detaches when `+2` is 1 (not
  7) and marks the attachment by `+2 = 1`; `+1` is read back after the store.
- **`MoveCmd_AttachMove`** divides by the count SIGN-extended (`movsx`, the
  PSX's `(int)(char)` too): D14. It reads the position through the pointer
  `MoveCmd_HandlePosition` returns.
- **`MoveScript_FindLabel`**: 16-bit positions; no bound (a missing label or
  an op of length 0 on the way never returns).
- **`MoveScript_ObjectKind`** returns the pointer itself for anything that is
  none of the four kinds.
- **`Sprite_ScriptPeek`**'s sums wrap at 16 bits.
- **`Scena16_PartyReset`** reads the flag byte `0x904061` after
  `PartySet_Load` and writes it after the lists; it returns `Party_Join`'s
  `al`, which `Scenario_CallA`'s callers never read.

## 3. The fuzz

`BOF3X_SHADOW=move_cmds` ([`move_cmds_fuzz.cpp`](../src/game/move_cmds_fuzz.cpp)),
at start-up, before the game's C runtime. Each of the fifteen is byte-copied
with every call re-aimed at a recording stand-in (`CloneCall` with the callee
the disassembly showed) - the calls among the fifteen included, so each is
tested alone - and `MoveScript_Variable`'s jump table relocated into its copy.
`Math_Ratan2`'s copy keeps its tail jump to the real `_ftol` (plain x87).

- **28,000 rounds**, 2,000 per function for fourteen of them: random state
  (`Sprite_Kind2`, all 30 `Sprite_Objects`, the four `Sprite_ObjectsExtra`, the
  party records, `MoveScript_F3Divisor`, `Game_AreaNumber` with
  `MoveScript_FAWord`, `Field_Kind2X` / `Z`, `0x92A0C0`, the flag byte and
  both party lists, `Field_MemberCount`, `0x8034E0..0x8034F1`, the counters,
  `0x802DC9`, `0x904030`, the flag pointer `0x929ED0`, a sprite, a context,
  both out buffers, `Sprite_Current`), theirs, the same state again, ours;
  every byte of it, the result and the stand-ins' log compared. **0
  mismatches.** Byte arguments are given stale upper bytes, as Capcom's
  callers push them.
- **Math_Ratan2, 20,000 rounds** under the control words `0x027F` (the
  game's), `0x037F` and `0x007F`: signed zeros, both infinities, quiet and
  signalling NaNs, denormals, `FLT_MAX`, the `(short, 128.0)` pairs
  `Party_MoveMember` passes, integers to ±1000 and random bit patterns; the
  result and the control word after compared. **0 mismatches.**
- Seeded boundaries: the party slot inside, just below (quotient 0), below
  (negative), `0x100 + n` and `0x80..0xFF`; record bit 0; a velocity summing
  to zero; frames 0, 1, `0x40`, `0x80`, `0xFF`; the sprite's angle at
  direction 3's ±1; directions 2 / 6 and past 7; a stale `F3` divisor; the
  context's `+4`; frames 0 / 1 / `0x7F` / `0x80` / `0xFF` for `F7` and the
  attach move; handles with bit 7 and small indices over objects of type
  `0x0A` a third of the time, and `0x40..0x7F` without it; `+1` / `+2` at 7
  and 1; mode 7; position bit `0x4000`; variables 0..15 and past; every edge
  of `MoveScript_ObjectKind`'s five ranges and its neighbour; the peek's tick
  byte, `position / 2` equal to and one off the length byte, entries `0x7F` /
  `0x80`.
- The stand-ins give the caller what it reads and disturb what it writes or
  reads again: the ground with a high half of 0, sign-extended or random; the
  context reset writes `+0x83..+0x85` (so a store moved before it shows); the
  handle's position comes back through the returned pointer, and one call in
  four it is a different buffer; the party calls rewrite the flag byte, the
  lists and the count.

**Not generated, on purpose:** the inputs that fault on both sides
(`MoveCmd_MoveKind2`'s speed 0 or 0 steps, `MoveCmd_AttachMove`'s
`0x80000000 / -1`); a label search that wraps past `0xFFFF`; label bytes of
even length (36 have length 0 and would hang the original on a desynced walk;
the rest are excluded so that a desynced control ends on the sled below).

**The label search's generator was changed by its controls.** Its first form
put random bytes in operands, and three controls (`L1`, `L2`, `L4`) were
refused only by a hang: an out-of-step walk met a length-0 byte. Now every
operand byte is an op of non-zero length, the label is followed by a sled of
24 `0A label` pairs, and the label byte is an op of odd length - so an
out-of-step walk that lands on a label byte steps to the other parity, meets
a `0A`, and ends at a position the fuzz compares. All three are now refused
by count. Likewise `K2` was first refused by a fault (the stale divisor it
read was 0); the divisor is now seeded non-zero, and `K2` is refused by count.

## 4. Negative controls

One planted bug each, `build` then `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=move_cmds` (the script is the scratchpad's `controls_L.py`; not
committed). **76 controls, 74 refused by a count, 2 not refused because they
change nothing.** Mismatching rounds of the function's 2,000 (of 20,000 for
`R`):

| | control | refused |
|---|---|---|
| R1 | `Math_Ratan2`: rounding to nearest, not toward zero | 9,381 |
| R2 | 1 / pi for 1 / 3.14 | 7,367 |
| R3 | x and y swapped | 18,133 |
| R4 | a 32-bit `fistp` (a NaN gives `0x80000000`, not 0) | 1,616 |
| R5 | the control word not put back | 20,000 |
| P1 | `Party_MoveMember`: a negative slot is 3 | 279 |
| P2 | record bit 1 for bit 0 | 1,016 |
| P3 | the velocity test on `+0xC` alone | 464 |
| P4 | direction 2 unflipped for 3 | 178 |
| P5 | bit 2 of the direction for bit 1 | 503 |
| P6 | count `+1 = n << 2` | 1,146 |
| P7 | `+8` divided by 4n | 878 |
| P8 | direction 2's angle for 3's | 79 |
| P9 | the long turn on `<=` | 28 |
| P10 | `+0x64` not zeroed | 481 |
| P11 | the turn masks `0x1FFF` | 683 |
| P12 | frames 0 leaves `+0x6C` | 160 |
| P13 | the x tilt starts from `+0x68` | 843 |
| P14 | the slot by unsigned division | 160 |
| P15 | the rise kept as 32 bits | **not refused** - only its low 16 bits reach the short passed on |
| P16 | the whole quotient as the slot | 368 |
| W1 | `Sprite_SetPoseWait`: pose 4 | 2,000 |
| W2 | its two stores swapped | 1,992 |
| K1 | `MoveCmd_MoveKind2`: only direction 2 halves | 338 |
| K2 | `MoveScript_F3Divisor` not stored | 1,983 |
| K3 | `Field_Kind2X` / `Z` swapped | 2,000 |
| K4 | the rise shifted 15 | 2,000 |
| K5 | `MoveScript_FAWord` not cleared | 2,000 |
| K6 | the height unsigned | **not refused** - it moves the rise by `0x10000`, which the `<< 16` shifts out |
| Q1 | `Kind2_Place`: `+0x85` set before the reset | 1,000 |
| Q2 | speed index 2 | 2,000 |
| Q3 | the PSX's short to `MapView_SetElevation` | 1,006 |
| Q4 | `+1 = 0` | 2,000 |
| Q5 | the script byte stored before the reset | 1,991 |
| F1 | `MoveCmd_OpF7`: `+0xA = n + 2` | 965 |
| F2 | flag `0x20` | 723 |
| F3 | the context test inverted | 2,000 |
| F4 | the rise unsigned | 468 |
| F5 | the ground only when moving | 1,035 |
| H1 | `MoveCmd_HandlePosition`: the index, not the count | 539 |
| H2 | six bits without bit 7 | 131 |
| H3 | `Sprite_Current` not put back | 2,000 |
| H4 | `+0x3E` unsigned | 540 |
| H5 | `0x92A0C0` not written | 2,000 |
| A1 | `MoveCmd_Attach`: kind 1 detaches on `+2 == 7` | 285 |
| A2 | context `+7` not set | 201 |
| A3 | the kind-1 path advances 2 | 411 |
| A4 | no detach on `+1 == 7` | 441 |
| A5 | kind 1 writes `+2 = 7` | 367 |
| A6 | `s` to `+0x18` too | 597 |
| D1 | `MoveCmd_Detach`: bit 4 for bit 5 | 1,514 |
| D2 | kind 2 for kind 1 | 1,042 |
| M1 | `MoveCmd_AttachMove`: the count unsigned | 1,020 |
| M2 | the buffer, not the returned pointer | 501 |
| M3 | handle and offset swapped | 1,995 |
| M4 | `+9` not written | 2,000 |
| L1 | `MoveScript_FindLabel`: `F8 07` steps 4 | 65 |
| L2 | `0E` / `0F` by mask 4 | 96 |
| L3 | bit `0x8000` asks | 974 |
| L4 | `0F` by the table | 36 |
| V1 | `MoveScript_Variable`: 14 is `0xFF` | 114 |
| V2 | 13 is 0 | 120 |
| V3 | 9 is two bits | 31 |
| V4 | 0 unsigned | 97 |
| V5 | 7 unsigned | 43 |
| O1 | `MoveScript_ObjectKind`: `Sprite_Objects` to its end inclusive | 91 |
| O2 | anything else 0 | 1,042 |
| O3 | the third `ObjTrio` object missing | 98 |
| O4 | `Sprite_ObjectsExtra` from its second byte | 117 |
| E1 | `Sprite_ScriptPeek`: `> 0x80` | 132 |
| E2 | 0 at the end | 315 |
| E3 | the whole position compared | 315 |
| E4 | tick byte 0 ticks | 369 |
| T1 | `Scena16_PartyReset`: the flag byte read before `PartySet_Load` | 947 |
| T2 | `| 3` | 988 |
| T3 | five list bytes | 1,876 |
| T4 | `Field_MemberCount` kept | 1,990 |
| T5 | member 9 joins | 2,000 |

## 5. The other harnesses' stand-ins

Checked, none changed: `field_objects.cpp` stands in for `Sprite_SetPoseWait`
and `Party_MoveMember` (after the latter its caller only clears a bit; after
the former the update reads `+1` again in its pose loop, but only as a value
it copies - a stand-in that leaves it hides nothing the caller undoes);
`move_script.cpp`'s stand-ins for `MoveScript_ObjectKind`,
`MoveScript_FindLabel`, `MoveScript_Variable`, `Sprite_ScriptPeek`,
`MoveCmd_OpF7`, `MoveCmd_Attach` and `MoveCmd_MoveKind2` move the position or
the fields the interpreter reads again (noisier than the real ones where they
differ); `move_groups.cpp`'s for the kind, the peek and
`MoveCmd_HandlePosition`; `event_script` and `field_modes` for `Kind2_Place`.
All still self-test with `BOF3X_SHADOW='*'` (exit 0).

## 6. What the live batch should watch, and what no check reaches

- Every function is reached by the attract cycle, most in scenario 16: the
  party reset 3 times (the demo's start), the attach with its detach and move
  6 times each, op `F7` 6 times. The frame hash and the oracle cover them.
- **`Math_Ratan2` is the one with callers the attract does not reach**: 68
  battle and effect call sites. The batch's new-game capture pair (its
  scripted battle) is the only check that reaches any of them; the fuzz is
  what stands behind the rest.
- Not reached by any check but the fuzz: `MoveCmd_HandlePosition` with bit 7
  (no shipped script, D6), `MoveCmd_AttachMove` with a count of `0x80` or more
  (none shipped, D14), and the object kinds other than 0..3 in
  `MoveScript_ObjectKind`.

## 7. For `analysis/calltrace/entries_logic.txt`

Owned ranges, `start size` in hex as that file has them. Fourteen are in the
list already (from `entries.txt`); against it: **`00519890 57` is new** (a
hidden function, reached only through chapter 16's call table), and **two
sizes change** - `005190A0` was `559` (`pe_funcs.py` ran it on through
`Field_ObjectFollow` `0x5192A0` and beyond), `0057C310` was `1AF` (past its
jump table into `0x57C3E0`'s neighbours). The other twelve are as listed.

```
00518B00 16
005190A0 1F9
00519890 57
00573400 ED
00578C10 F5
005734F0 66
00578D10 A4
00578DC0 EA
005792A0 E8
00579390 1D
005793B0 92
00579450 78
0057C310 D0
0057C840 52
0057CDC0 4E
005A7A70 1B
```

`0057C310`'s `D0` includes its jump table (`0x57C3A4..0x57C3DF`); the code
alone is `0x91`.

## 8. `MoveCmd_Move`, taken 2026-10-10

Until this date `MoveCmd_Move` `0x578C10` was the one function of the game's
own code our code still called as Capcom's: `symbols.toml` had its
`[[func]]` (the prologue read in round three) without `impl`, so every
`AH_CALL(MoveCmd_Move)` / `SH_CALL(MoveCmd_Move)` - nine area modules and
`EffectKind32_Throw` (field_c1) - and the callee tables of the movement
script's interpreter (`move_script.cpp`) and the field objects
(`field_objects.cpp`, `object_kinds.cpp`) bound to Capcom's address. It was
missing from `analysis/pc_funcs.json` (that list misses starts it did not
walk to), had no detour, and `inject: N ours` did not count it, since that
counts `impl`s only. `tools/owned_sweep.py` (8.2) is the check that lists
such a function.

**The read** (`python tools/pe_disasm.py 0x578C10:90`, capstone to the last
instruction): `0x578C10..0x578D04`, `0xF5` bytes, then nops to `MoveCmd_OpF7`
at `0x578D10`. Cdecl, `(object, direction)`; two calls, `0x578CBE`
`AreaMap_Slope` and `0x578CD2` `AreaMap_Elevation`; every jump internal. The
PSX twin is `FUN_801ACBA8` (`psx_pair` call; not re-read for this).

1. `speed = Field_MoveSpeeds[object +4]`, the byte unmasked.
2. `Sprite_Current +9 = 0x20` for directions 2 and 6 (the direction's low
   byte), else `0x10`; then `+9 = +9 / speed` (`idiv`; a speed of 0 faults).
3. `+0xC = MoveCmd_F9Steps[direction * 2] * speed`, `+0x10 =
   MoveCmd_F9Steps[direction * 2 + 1] * speed` (`imul`, wrapping; the
   direction unmasked, `(direction & 0xFF) << 3` bytes). `0x6696DC` was
   `move_cmds_callees.h`'s unnamed `kKind2Steps`; round twelve had already
   named it `MoveCmd_F9Steps`, and both moves now use that name.
4. The landing: `x = +0xC * +9 + +0x34`, `z = +0x10 * +9 + +0x38`.
5. Unless bit 7 of `object +0`: `ground = AreaMap_Slope(x, z, direction)`,
   and when it leaves `DamageScratch`'s byte non-zero (sloped ground,
   [`kind2-object.md`](kind2-object.md) section 7) `ground =
   AreaMap_Elevation(x, z)` instead; then `+0x14 = ((s16) ground - (s16)
   +0x3E) / +9` (`idiv`), both re-read through `Sprite_Current` after the
   calls. On flat ground the answer is `AreaMap_Slope`'s own return, the
   flat case's height.
6. `Sprite_Current +0 |= 0x10`, always. `eax` is left holding
   `Sprite_Current`; no caller reads it (`void`).

`Field_JumpSetUp` `0x534610` is the same shape with other callees
(`0x5725C0`, `MapView_GroundAt`), and `MoveCmd_MoveKind2` is the kind-2
object's version.

**Ours** (`move_cmds.cpp`) is the same term for term. Kept as the original
has it: the speed index and the direction unmasked; the signed divisions;
the 16-bit ground and height; `+9` and `+0x3E` read again after the calls;
`object +0` read after the velocity is stored. **Two differences, neither
reachable by a valid input:** a divisor of 0 - a speed of 0 (table entry 0),
or a frame count of 0 (a speed above `0x10`, `0x20` on the diagonals, which
only an index past the table gives) - aborts by name where the original
raises the divide fault (the owner's ruling of 2026-09-26, no DIVERGENCE
entry: [`takeover-queue-round9.md`](takeover-queue-round9.md)); and the
original pushes its whole direction dword to `AreaMap_Slope` where ours
passes the byte, which is all `AreaMap_Slope` reads (`area_slope.cpp`).

**The harnesses.** `MoveCmd_Move` now binds to ours, so the stand-in rows that
named it as Capcom's were switched to key by ours with Capcom's address
kept: `area_w0a_fuzz.cpp` (`KeyOf(&::MoveCmd_Move)`), the `W1C`, `W1F`,
`W2D`, `W3D`, `W3E`, `W3F`, `W4E`, `W4F` rows (`*_THEIRS` to `*_OURS`),
`field_c1_fuzz.cpp` (`FC1_OURS`) and `scenario_harness.cpp` (`FIELD_OURS`).
The clone-based fuzzes (`field_objects`, `object_kinds`, the area clones'
`CallSite` rows) re-aim Capcom's call sites by address and needed nothing.

### 8.1 The fuzz and its controls

`move_cmds_fuzz.cpp` clones `0x578C10` for `0xF5` bytes with both calls
re-aimed: `0xAE` to a new `AreaMap_Slope` stand-in, `0xC2` to the existing
elevation one. The slope stand-in records `(x, z, direction & 0xFF)`,
answers a high half of 0, sign-extended or random, leaves `DamageScratch`'s
byte 0, 1 or another non-zero byte (a third of the time each), and a third
of the time each rewrites `+9` (never to 0) and `+0x3E`, so that a read
moved before the call shows. `DamageScratch`'s four bytes joined the
compared state. Inputs: the object is the fuzz's context buffer, bit 7 of
`+0` half the time; the speed index 1..5 three times in four, else any byte
whose table byte divides (non-zero, and with the ground asked one that
leaves a count); directions 0..7, 2 and 6 again, 8, 9, `0x82`, `0xFF` and
random, with stale upper bytes. Rounds rose from 28,000 to 30,000 to keep
2,000 a function.

**Result:** `move_cmds self-test: 30000 rounds (2000 per function, 15
functions), 26765 calls to the stand-ins, 0 MISMATCHES; Math_Ratan2 20000
rounds under three control words, 0 MISMATCHES`.

**Controls**, one planted bug each in `MoveCmd_Move`, built and run with
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=move_cmds` from an ini-less copy (the
scratchpad's `controls.py`, not committed). **14 planted, 14 refused**;
mismatching rounds of `MoveCmd_Move`'s 2,000:

| | control | refused |
|---|---|---|
| M1 | only direction 2 doubles | 239 |
| M2 | `+9` not divided by the speed | 1,395 |
| M3 | the x unit is the z one | 1,745 |
| M4 | bit 6 of `object +0` for bit 7 | mismatches from round 29 (12 logged), then ours' abort on a count of 0 ends the run before the summary |
| M5 | the elevation always, never the slope's own answer | 363 |
| M6 | the scratch byte compared with 1, not 0 | 322 |
| M7 | the height unsigned | 525 |
| M8 | `+9` not read again after the calls | 322 |
| M9 | flag `0x20` | 1,496 |
| M10 | the direction masked `& 7` for the units | 729 |
| M11 | the rise divided unsigned | 449 |
| M12 | the ground's whole dword, not its low 16 bits | 516 |
| M13 | the landing one frame's step | 695 |
| M14 | the slope asked with `direction & 7` | 384 |

M6 and M8 are refused only because the stand-in draws a scratch byte other
than 0 and 1 and rewrites `+9`: without those draws they would pass.

### 8.2 Verification

Built in the worktree (`cmake --preset i686 && cmake --build build`), run from
an ini-less copy of the launcher and DLL:
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' bof3x-launcher.exe --game .../bof3 --no-config`.

- narrow: exit 0, `inject: 10087 ours, 0 left original by BOF3X_ORIGINAL` (one more than 10,086), no mismatch in any group.
- `BOF3X_WIDE=1` (`patch ON Widescreen` in the log): exit 0, the same `inject:` line, no mismatch.

`python tools/owned_sweep.py` after the takeover - every harness call macro
(`AH_` / `BH_` / `MH_` / `SH_CALL`), every row of an array typed
`*::CallSite`, and every bare use of the name of a `[[func]]` without `impl`:

```
owned_sweep: 15346 harness call macros (6 through a pointer), 28469 CallSite rows; 10 callee(s) not ours
  0x5adf00 Mp3_Create .. 0x5b0d50 Mp3_MemoryIo: [[func]] without impl (eight, sound.cpp)
  0x5b9550 Crt_ftol: [[func]] without impl (21 file(s))
  0x5bad64 Crt_GetPtd: [[func]] without impl (2 file(s))
```

The decoder's eight are DIV-0091's, reached only under `BOF3X_MP3=original`;
`Crt_ftol` is a clone's callee in 21 fuzzes (ours inline it). **`Crt_GetPtd`
`0x5BAD64` is one more than expected**: `crt_rest.cpp`'s `Rand` seed
instrument for the map_cells live check calls it, as its `symbols.toml`
entry records - a known use, reported, not taken here. The six "through a
pointer" are macros over a local function pointer or a template parameter
(`effect_2a.cpp`, `magic_s16.cpp`, `magic_s18.cpp`, `rest_1e.cpp`).

**Not verified:** no live route was run (the owner was playing on the
machine), and no state-hash A/B run (`BOF3X_STATEHASH`, `tools/statehash.py`)
was made; which recorded routes reach `MoveCmd_Move` was not measured. The
PSX twin was not re-read.

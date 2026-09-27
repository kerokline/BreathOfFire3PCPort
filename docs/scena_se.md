# The scenario chapters' shared engine-side helpers (group SE)

**Status:** IN PROGRESS (2026-09-27) - stage A: six functions read to their
last instruction and written (`src/game/scena_se.cpp`, shadow name
`scena_se`), their fuzz file written against the scenario harness's
contract; not built, not fuzzed, no controls yet (the harness is group SCH's,
not merged). Three of the nine addresses the round listed are not taken
(section 1).

Group SE of round ten's first wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §1: "eight
helpers the banks share with the field engine", the analogue of the spell
round's group L).

## Stage A done - what stage B has to do

Written at stage A (branch `phase-3/round10-se`):
`src/game/scena_se.cpp`, `scena_se.h`, `scena_se_callees.h`,
`scena_se_fuzz.cpp`; six `symbols.toml` entries with `impl` (two edited in
place, four new) and `[[data]] EventBattle_Records`; five lines in the main
checkout's `entries_logic.txt`. Both `.cpp` files pass
`i686-w64-mingw32-clang++ -fsyntax-only -m32 -std=c++20` against a private
copy of `magic_harness.h` renamed to `scenario_harness` / `SH_`, with the one
field SCH's tool prints added (`Clone::shape`, `Shape::kCallEntry`).

**The branch does not link at stage A**: `event_script.cpp` names
`EventOp_0x` and `EventObj_Face`, which `impl` now binds to ours, and the
module is not in the build yet. Stage B fixes that by registering it.

Stage B, after merging `phase-3/capture-round-ten` at SCH's harness SHA:

1. Register the module: `src/game/scena_se.cpp` and `scena_se_fuzz.cpp` at
   the end of `CMakeLists.txt`'s list, `ScenaSe_Inject();` (and its
   `#include "game/scena_se.h"`) at the end of `inject_all.cpp`.
2. Check the fuzz file against SCH's real header: the `Clone` field order
   after `ours` (`ret_mask`, `calm`, `shape`), the name of the call-table
   shape (`Shape::kCallEntry` as `scenario_rows.py` prints it), and what a
   `kCallEntry` clone is called with (Scena08_PartyJoin784 reads no word; the
   group's `Args` leaves its ten words random). If SCH's standard regions
   cover some of this group's (Sprite_Current, ObjTrio, the member count,
   Field_ScriptFlags2), drop the duplicates from `kRegions` - overlap is
   harmless but double-counts bytes.
3. Build, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_se`, to 0 mismatches;
   read the coverage line against section 4's expectations (every callee
   reached; Effect_FindFree's 0xFF branch; EventOp_0x's early return).
4. Plant and refuse the controls of section 5 (script: plant, rebuild, run,
   restore, rebuild), fill the table, then `BOF3X_SHADOW='*'`.
5. `tools/ledger_check.py` (0 errors), this status header, the README row's
   counts.

## 1. The nine addresses, read

The plan's list came from the scenario walk's frontier
([`scenario-roots.md`](scenario-roots.md), `analysis/scenario_roots.json`
`via`). Read to the last instruction (capstone recursive descent; SCH's
`tools/scenario_rows.py --unit SE --clones` gives the same nine extents),
and each caller found by an `E8` / `E9` scan of `.text` and a dword scan of
the image:

| Address | Bytes | Callers | What it is | Taken |
|---|--:|---|---|---|
| `0x4410B0` | 0x43 | 53 `E8` sites in 36 functions of **14 chapters** (0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15) | **a chapter helper**: `Field_StartEventBattle` | yes |
| `0x519F70` | 0x24 | chapter 8's call table A entry 5; the tail `jmp` of entry 6 (`0x519FA0`) | **a chapter's call-table entry** outside the band: `Scena08_PartyJoin784` | yes |
| `0x591CC0` | 0xE5 | one `E8`, chapter 6 (`0x54D053`) | **a chapter helper** (engine-side): `Party_AddToLists` | yes |
| `0x57A010` | 0x1C2 | `EventScript_Op`'s default; 19 `E8`: 16 engine (area and world-map placements), 3 chapter code (chapters 6/7, 9/10, 15) | **an engine function the chapters call**: `EventOp_0x`, the event script's op 0x / Fx | yes (below) |
| `0x579D70` | 0x3C | eight placements' last call; one chapter site (chapter 3, `0x544977`) | **an engine function the chapters call**: `EventObj_Face` | yes (below) |
| `0x524870` | 0x76 | three calls in each of the 18 cell-pickup copies | **engine**, reached only through `0x520000` | yes: small, a leaf, nobody's |
| `0x520000` | 0x11F | three `E8` from `0x51FF20` | **engine**: one of 18 copies of `Field_CellPickup` | **no** |
| `0x508000` | 0x3F | entry 18 of the `.data` table `0x65E710` (21 handlers, `0x507640..0x508670`) | **engine**: one state of an object kind's machine | **no** |
| `0x5080A0` | 0x305 | `0x507DB0`, `0x507F10`, `0x508000` (twice), `0x508040` (twice) | **engine**: that machine's two-quad draw | **no** |

**Four of the nine are not chapter helpers at all; the walk reached them
through a coordinate.** The chapters push 16.16 positions as immediates, and
`0x520000` and `0x508000` are also such positions: `push 0x520000` at
`0x53A3B1`, `0x54BE5D`, `0x568E3F` (the x or z of `Field_ChangeArea` or
`0x56ADF0`), `cmp edi, 0x520000` at `0x53DBDD` / `0x54EBA6`, `cmp ebp,
0x520000` at `0x55BE29`; `push 0x508000` at `0x5638AE` (`Field_ChangeArea`).
`scenario_roots.py` counted a `.text`-valued immediate as a code reference,
so `0x520000` (and through it `0x524870`) and `0x508000` (and through it
`0x5080A0`) joined the closure. No chapter calls any of the four. This is a
fourth walker lesson for [`scenario-roots.md`](scenario-roots.md) §3: an
immediate that is a round 16.16 number is a coordinate first.

- **`0x520000` is left to the party-action round.** It is byte for byte
  `Field_CellPickup` (`0x51EBD0`, ours, [`field_hidden.md`](field_hidden.md))
  but for the multiplier of the found zenny (`mov cl, 0x14` where
  `Field_CellPickup` has `0xA`). There are 18 such copies at
  `0x51C270..0x525150`, one per party set's action (`Field_ActionBySet`,
  [`event_leader.md`](event_leader.md)): seven with 10 (`0x51C270`,
  `0x51CA60`, `0x51CFB0`, `0x51DAA0`, `0x51E1B0`, `0x51EBD0`, `0x51F4B0`) and
  eleven with 20 (`0x520000`, `0x5206C0`, `0x521200`, `0x5218C0`,
  `0x521F80`, `0x522A20`, `0x523390`, `0x523810`, `0x5242B0`, `0x524750`,
  `0x525150`) - an instruction-by-instruction comparison of all 18
  (2026-09-27). The 17 left are one body over a multiplier, as group S16
  wrote four sparkle copies once; taking one here would split the family.
- **`0x508000` and `0x5080A0` are left to their unit's round.** `0x508000`
  is entry 18 of a 21-entry handler table at `0x65E710` read at `0x50762E`
  (entries `0x507640..0x508670`); `0x5080A0` draws two textured quads for
  four of those handlers. The unit is about 22 functions that nobody owns
  and no route reaches; it is a unit of its own.
- **`0x524870` is taken**: a 118-byte leaf (two callees, both ours), called
  by all 18 pickup copies and nothing else, in no one's band. Taking it now
  is the pickup round's shared helper done first.
- **`EventOp_0x` and `EventObj_Face` are taken** though they belong to the
  event script's unit ([`event-script.md`](event-script.md) §1): both are
  small and self-contained (every callee ours), their template `EventOp_1x`
  is ours, and the chapters call them directly. The ten placement ops still
  Capcom's (3x..Ex) stay the event script's. A later fold may move these two
  into `event_script.cpp`.

## 2. The functions

| Function | Address | Bytes | Call shape | Args | Answers |
|---|---|--:|---|---|---|
| `Field_StartEventBattle` | `0x4410B0` | 0x43 | a direct callee of chapter code | 1 word (a byte used) | nothing read |
| `Scena08_PartyJoin784` | `0x519F70` | 0x24 | call-table entry (`Scenario_CallA`, chapter 8 A[5]) | none read | nothing read |
| `Effect_SpawnAtCell` | `0x524870` | 0x76 | a direct callee | 3 words (a byte, two words used) | nothing read |
| `EventObj_Face` | `0x579D70` | 0x3C | a direct callee | none | nothing read |
| `EventOp_0x` | `0x57A010` | 0x1C2 | `EventScript_Op`'s jump table default, and direct | 1 word: the op | nothing read |
| `Party_AddToLists` | `0x591CC0` | 0xE5 | a direct callee | 1 word (a byte used) | al: 1 placed, 0 not |

What each does (the `evidence` fields have the addresses):

- **`Field_StartEventBattle(id)`** starts an event battle from the field:
  `Field_ScriptFlags2` bit 12 (the encounter bit, which the leader's state 5
  clears when it hands over - [`event_leader.md`](event_leader.md)), the
  event battle byte `0x904AAA`, the leader (ObjTrio's first record) to state
  5 with its sub-state bytes cleared, the battle's flags byte `0x904AE5`
  from the event battle's record `EventBattle_Records[id] +0`, and
  `0x904AE4` 0. Each chapter call is followed by the chapter's next state
  (`0x8034E5`) or a return. The name says what the code sets; which battles
  the ids are is not read here.
- **`Scena08_PartyJoin784`** empties the party count and joins members 7,
  8 and 4 by `Party_Join`, then reloads the members' palettes (`0x533E00`).
  Entry 6 of the same table (`0x519FA0`) loads party set (7, 8, 4, 0) first
  and jumps here. The name is the ids; who they are is not read here.
- **`Effect_SpawnAtCell(state, x, z)`** puts an effect object of kind
  `0x34` at a cell: `Effect_FindFree`'s slot, in use, kind, state, the cell
  as 16.16, the ground there plus 0x100.
- **`EventObj_Face`** ends a placement: the object's pose byte `+0x4B`
  reset, then with its flags' bit 3 an animation (`+0x2A` from bit 4), else
  a facing.
- **`EventOp_0x(op)`** is `EventOp_1x`'s placement (`event_script.cpp`'s
  `Place`) plus object `+0x83` from `op[0x10]`.
- **`Party_AddToLists(member)`** is `Party_Join`'s list half without a
  sprite: the record's joined bits, then a free slot of the two lists, or
  with three members the first slot whose member lacks record bit 1 and
  stands in both lists.

## 3. Who calls each (for the rebinding pass)

Every chapter group calls these by raw address this wave. By band
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3):

| Function | Call sites by band |
|---|---|
| `Field_StartEventBattle` `0x4410B0` | SC0 5 (`0x538B10`, `0x538F90`, `0x5391B0`); SC1 2 (`0x53B130`, `0x53B5F0`); SC2a 5, SC2b 3 (`0x53ED70` .. `0x540EA0`); SC3 1 (`0x542F60`); SC5 11 (`0x546B80` .. `0x549A80`); SC6 6 (`0x54C480`, `0x54C910`, `0x54CF00`, `0x54D4F0`); SC7 6 (`0x54F8A0` .. `0x553070`); SC9a 2 (`0x5556D0`, `0x555B80`); SC9b 2 (`0x559970`, `0x55AB00`); SC12 2 (`0x561170`); SC13 5 (`0x563A20` .. `0x5673C0`); SC15 2 (`0x568980`, `0x56A0E0`) |
| `EventObj_Face` `0x579D70` | SC3 1 (`0x544830`); `event_ops_callees.h` `kFaceObject`; `event_script.cpp` by name |
| `EventOp_0x` `0x57A010` | SC7 1 (`0x5508C0`), SC9b 1 (`0x5594D0`), SC15 1 (`0x569730`); 16 engine sites; `event_script.cpp` by name |
| `Party_AddToLists` `0x591CC0` | SC6 1 (`0x54CF00`) |
| `Scena08_PartyJoin784` `0x519F70` | chapter 8's call table A (SCH names it); `0x519FA0` (nobody's) |
| `Effect_SpawnAtCell` `0x524870` | `field_hidden_callees.h` `kSpawnAtCell`; the 17 pickup copies |

## 4. The fuzz (`scena_se_fuzz.cpp`)

Written against the scenario harness's contract (`magic_harness`'s API one
for one, `SH_`), not yet run. The clone table is `scenario_rows.py --unit SE
--clones` with the three untaken dropped. Every callee is listed (the
group's listing stands over the standard set): `Effect_FindFree` answers a
byte `0xFF..0x13` (none, or slots 0..19); `EventObj_SetFlags` is logged by
the flags byte it reads (`deref` 1); `Sprite_SetAnimationBank` by the
bank's 16 bits; `0x533E00` by address.

- **Regions**: `Field_ScriptFlags2`, the battle bytes `0x904AA0..0x904B50`,
  the leader's head, `Effect_Objects` (20 x 0x80), `Sprite_Objects` (30 x
  0xA4), the `Sprite_Current` and `Field_ActiveMember` cells, the count and
  bank words `0x903850`, both party lists, `Field_MemberCount`,
  `CharacterRecords` 0..7 (`MoveScript_EffectState`'s 24 entries name
  records 0..7), and a 32-byte op buffer of the fuzz's own.
- **Seed**: both pointer cells at a sprite object; the count 0..29, and for
  `EventOp_0x` 30 or 31 a third of the time; `op[3]` bit 7 both ways; the
  member count 0..4; list ids below 24, the second list often the first's in
  another order (so the three-member search finds a slot); each record's
  bit 1 half the time; `EventObj_Face`'s `+7` bit 3 both ways.
- **Args**: `EventOp_0x` gets the op buffer; `Party_AddToLists` an id below
  24 with garbage above; the rest random words (the originals mask them).
- **Disturbance** (from the hash only): the count 0..29, `Sprite_Current`,
  one op byte, the bank word - what `EventOp_0x` reads again after its
  calls.

Stage B records the counts here.

## 5. Controls (planned; stage B plants them)

| # | Function | Mutant | Expected |
|---|---|---|---|
| C1 | `Field_StartEventBattle` | `\| 0x08` for `\| 0x10` | refused (region) |
| C2 | | leader state 4 | refused |
| C3 | | the record's `+1` byte for `+0` | refused |
| C4 | | `0x904AE4` left alone | refused |
| C5 | `Scena08_PartyJoin784` | members 7, 4, 8 | refused (log) |
| C6 | | the palette reload dropped | refused |
| C7 | | the count left alone | refused |
| C8 | `Effect_SpawnAtCell` | no early return on 0xFF | refused |
| C9 | | x zero-extended, not sign-extended | refused |
| C10 | | `+ 0xFF` for `+ 0x100` | refused |
| C11 | | kind 0x35 | refused |
| C12 | `EventObj_Face` | bit 5 for bit 4 into `+0x2A` | refused |
| C13 | | the two branches swapped | refused |
| C14 | `EventOp_0x` | `> 30` for `>= 30` | refused (count 30 seeded) |
| C15 | | `+0x83` from `op[0x11]` | refused |
| C16 | | `op[3]` read before `EventObj_SetFlags` | refused by the disturbance |
| C17 | | the count read once at the top | refused by the disturbance |
| C18 | | the bank from `op[2] << 8 \| op[1]` | refused |
| C19 | | the count's `+ 1` dropped | refused |
| C20 | `Party_AddToLists` | `n <= 3` | refused |
| C21 | | bit 0 for bit 1 in the search | refused |
| C22 | | the inner loop stops at 2 | refused |
| C23 | | al 1 on none | refused (`ret_mask`) |
| C24 | | `\|= 1` for `\|= 3` | refused |

## 6. Latent defects (described, not fixed)

- **`EventOp_0x` places before the table on a negative count.** The count
  word is compared signed (`cmp ax, 0x1E; jge`), so 0x8000..0xFFFF passes
  and indexes `Sprite_Objects` backwards. `EventOp_1x` / `2x` share it
  (`event_script.cpp`'s `Place`). Nothing seen sets the count negative.
- **`Party_AddToLists` indexes `MoveScript_EffectState` by a whole byte.**
  An id of 24 or more reads the bytes after its 24 entries as a record
  index and ORs 3 into whatever lies at `CharacterRecords` + 0xA4 x that
  byte; an empty list slot (`0xFF`) in the three-member search reads one
  too (read only). `Party_Join` has the first (field_event.cpp's comment).
- **`Effect_SpawnAtCell` uses `Effect_FindFree`'s slot unchecked** - safe
  because FindFree answers 0..19 or 0xFF.
- **`Field_StartEventBattle` reads `EventBattle_Records` by a whole byte**,
  the table's length unknown.

## 7. Found on the way

- **The chapter call tables' entries live outside every band.** 100 of the
  call tables' distinct entries sit at `0x519890..0x51AC20` (the scenario
  walk's roots, read off `0x660B84` / `0x660BD4`), beside the field engine,
  not in `0x537F20..`. Two are ours (`Scena16_PartyReset`, now
  `Scena08_PartyJoin784`); **98 are in no wave-one group's band**, including
  `0x519FA0` next to this group's. They are small (most 0x10..0x60 bytes)
  and are a group of their own, or each chapter group's by its table - a
  cut for the coordinator.
- **`0x533E00`** (the members' palettes reloaded, `Scena08_PartyJoin784`'s
  tail) and **`0x519FA0`** are nobody's.
- **`entries_logic.txt` has three extents too long** for addresses this
  group does not take: `00520000 6BB` is 0x11F, `005080A0 6E7` is 0x305
  (and `00524870 33C`, fixed by this group's `76` line).
- `event_ops_callees.h`'s `kFaceObject` and `field_hidden_callees.h`'s
  `kSpawnAtCell` are now ours (`EventObj_Face`, `Effect_SpawnAtCell`); the
  raw addresses stay right until the rebinding pass.

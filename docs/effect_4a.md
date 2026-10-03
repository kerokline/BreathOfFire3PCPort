# Group E4A: effect kinds 0x82 (states 11..23) and 0x83..0x87

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..14),
wave four, from the round branch's tip `89c25e1`. **51 functions ours**
(`src/game/effect_4a.cpp`, shadow name `effect_4a`): the cut table's 48 rows
for E4A (`analysis/round13_cut.tsv`, the band `0x433640..0x489020`) and three
starts no list of the cut has - kind 0x83's state 2 `0x4883D0`, kind 0x85's
draw tail `0x488B90` and kind 0x86's dispatcher `0x488BE0` (section 5). Each
read to its last instruction with capstone and fuzzed through the scenario
harness in effect mode ([`scenario_harness.md`](scenario_harness.md) section
8) without edits to it: @ROUNDS@ rounds, 0 mismatches; @CONTROLS@. **Fuzz
only**: no recorded route enters any of the 50 effect functions (section 9).

Every row is effect code. One is also battle code: **`0x433640`
(`Sprite_StateRestart`, a `hypothesis` row) is a one-line state** - `+1 = 0` -
that `EffectKind83_States[12]` and the stack table of the battle's
`BattleFx_ActorWatch` (`0x433460`, its state 2) both name: the linker kept one
copy of identical code, which sits inside the battle module's run. It is
effect code by its table, taken whole, fuzzed in effect mode, and named for
what it does on whatever `Sprite_Current` is (section 5).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x82: after E3D's pushes, `Sprite_Objects` record 0 nudged in z (+0x8000, -0x10000, +0x8000 with eight-frame holds), counted, then restarted from state 0 or ended | 8 (+3 shared) | `EffectKind82_States` `0x654C88` (E3D's, 24): entries 11..23; 14, 16, 23 are kind 0x83's functions, 19 `BareRet`, 21 `Effect_StateRelease` |
| 0x83: kind 0x82 again for record 1 - pushed a cell at a time by `0x903849`'s count while short of the leader, nudged, restarted or ended | 20 | `Effect_KindHandlers[0x83]` (`0x65555C`), `EffectKind83_States` `0x654CE8` (24): entry 0 E3D's `EffectKind82_Start`, 12 `Sprite_StateRestart`, 19 / 20 `BareRet`, 21 / 22 `Effect_StateRelease` |
| 0x84: the count's driver - waits for one of three held-button words, picks the push count, counts the presses, ends at a line or on 0xFE | 5 | `[0x84]` (`0x655560`), `EffectKind84_States` `0x654D48` (5; entry 4 `Effect_StateRelease`) |
| 0x85: a run of messages 0x33..0x36 under a full-screen tile that brightens between them, then a wait on the counter (`Area144_SpawnEffect85` spawns it) | 7 | `[0x85]` (`0x655564`), `EffectKind85_States` `0x654D60` (5) |
| 0x86: two free sprites placed by `EventOp_0x`, a cell apart in z, slid together, brightened, then freed | 5 | `[0x86]` (`0x655568`), `EffectKind86_States` `0x654D74` (4) |
| 0x87: E4B's records set up and run to their end, the draw pass flags kept and put back | 6 | `[0x87]` (`0x65556C`), `EffectKind87_States` `0x654E88` (9; 4..7 `0x492750`) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the five dispatchers `evidence`). "Push", "nudge", "hold",
"mash", "brighten", "slide", "tint" name the code's shape - the cells it steps
- not a play-tested fact: what the game shows with these kinds and where was
not traced (section 9; the owner's word, as the brief says). The only spawner
`src/game` shows is area 144's `Area144_SpawnEffect85` (kind 0x85, `+6` the
active member's `Sprite_Objects` index); the others are spawned by code not
yet ours or by event scripts.

## 1. What each function does

Ours is `src/game/effect_4a.cpp`; every function carries its original's
address and a one-paragraph reading. In outline:

### 1.1 Kind 0x82, states 11..23 (E3D's `EffectKind82_Run` `0x487C10`)

E3D's states 2..10 push record 0 a cell at a time and check it against the
leader; a check that finds it there sends `+1` to 0xD. State 11
(`_Check11`) is the last check (0xD, `+9` 0; else 0xC). State 12
(`_Restart`) sets `+1` 0 - back to `EffectKind82_Start` - and clears
`0x903849` when it is 0xFF or `0x90384A` is 0x80. States 13..18 nudge record 0
in z: `_Nudge13` (`Sprite_Current` pointed at the record for two adds of
0x4000 to `+0x38`, then `+1` 0xE, `Sprite_Current` put back, `+9` 8),
`_Hold14` (`EffectKind83_Hold14`, shared: `+9` down, at 0 `+1` 0xF),
`_Nudge15` (four adds of -0x4000, `+9` 8), `_Hold16` (shared, to 0x11),
`_Nudge17` (two adds, `+1` 0x12), `_Count18` (`+2` up; below 0xF to state
0x14, else back to 0xD). State 19 is `BareRet`, 20 (`_Again`) ends to 0x17
(`+2` 0) when `0x903849` is 0xFF or `0x90384A` 0x80, else clears `0x903849`
and restarts (`+1`, `+2` 0). State 21 is `Effect_StateRelease`. State 22
(`_End`, where E3D's `_Wait` sends 0x16) sets `Field_State` to the third
member's object (`ObjTrio` + 0x298) and ends: its x at 0x34 cells or past
(signed) writes `0x903849` 0xFF and the step `0x8034E5` 0x14, else 0xFE and
0x1E, `0x90384A` / `0x90384B` 0 either way, then a tail `jmp` to
`Effect_Release`. State 23 (`EffectKind83_Finish`, shared) stores the leader's
x less record 0's into `+0x34`, the step 0x1E, and releases.

### 1.2 Kind 0x83 (`EffectKind83_Run` `0x488220`)

Kind 0x82's code again for `Sprite_Objects` record 1 (`0x7DEF24`): `_Wait`
is `EffectKind82_Wait` byte for byte but its own copy (`+1` 0x16 when record
**0** has reached the leader - section 7; 0x17 when `0x90384A` is 0x80; the
switch on `0x903849` through MSVC's byte table `0x4882C4` and jump table
`0x4882B0` inside the extent: 3 to state 6, 4 to 4, 5 to 2, 0xFF releases);
`_Push2`..`_Push10` four adds of 0x4000 to record 1's `+0x34`; `_Check3`..
`_Check11` against record 1's x; `Sprite_StateRestart` at 12; `_Nudge13`,
`_Nudge15`, `_Nudge17` on record 1's z; `_Hold14`, `_Hold16`; `_Count18`
(`+2` up; 0xF or more `+1` 0xD, else `_Again`'s code in line - this kind's
entry 20 is `BareRet`); 0x16 is `Effect_StateRelease`, 0x17 `_Finish` (record
0's x again).

### 1.3 Kind 0x84 (`EffectKind84_Run` `0x4887E0`)

`_WaitPress`: `+1` 1 when `Input_Held` (a word) is exactly 0x3000, 0x6000 or
0x2000. `_Pick`, `_Mash` and `_Pause` open alike: `0x903849` 0xFE calls
`Effect_Release` **and goes on** (section 7); then `Field_State` the third
member's object, whose x at 0x34 cells or past writes `0x903849` 0xFF,
`0x90384A` / `B` 0, the step 0x14 and releases. Past that, `_Pick` sets
`0x903849` from `EffectKind84_PushCounts[Rand & 0xF]` and `+1` 2; `_Mash`,
once `0x903849` is 0 (the pushes done), sets `+9` from
`EffectKind84_Waits[Rand & 0xF]` and `+1` 3, and otherwise counts each frame
a counting word is held into `0x90384A` - at the seventeenth, 0x80 and the
release; `_Pause` counts `+9` down and at 0 clears the word `0x8034E6` and
goes back to `_Pick`. Entry 4 is `Effect_StateRelease`, which no state stores.

### 1.4 Kind 0x85 (`EffectKind85_Run` `0x4889E0`)

`_Start`: the tile's colour `+0x5D..+0x5F` 0, the message `+0xB` 0x33,
`Msg_OpenScript(+0xB)`, `Field_Request` 2, `+1` 1. `_WaitMessage`: when the
message has closed (`Field_Request` not 2) `+9` 0x1E, `+1` 2. `_Brighten`:
`+9` down; while not 0 each colour byte up 2; at 0 `+9` 0x1E, `+1` 3.
`_NextMessage`: `+9` down; at 0 `+0xB` up - at 0x37 the colour 0xFF, `+1` 4
and the counter `0x903848` up one, else the next message and back to state 1.
`_Wait`: the counter at 0x35 releases the record. States 1..4 each end with a
call of E4D's `0x48CA90` (a full-screen tile coloured by `+0x5D..+0x5F`) and a
tail `jmp` to `_ShowObjects`: `Sprite_Objects[+6]` and the leader each get
`+0x29` 2 and `Sprite_UpdateScreen` with `Sprite_Current` on them, then
`Sprite_Current` is put back.

### 1.5 Kind 0x86 (`EffectKind86_Run` `0x488BE0`)

`_Spawn`: two `Sprite_FindFree` slots into `+3` and `+4`, each marked in use
as found (none: nothing this frame; the first unmarked when the second is
none); each placed by `EventOp_0x(0x654D88)` with the count word `0x903850`
the slot (the op moves `Sprite_Current`; the state keeps its record in `esi`
and puts it back); the first's z a cell less, the second's a cell more, `+0`
bit 5 and `+0x5C` 1 on both, tinted (`Sprite_SetTint(first, 0xF, 0, 0, 1)`,
`(second, 0, 0, 0xF, 1)`), `+9` 0x20, `+1` 1. `_Slide`: 32 frames of 0x800
towards each other, then `+9` 0xF, `+1` 2. `_Tint`: 15 frames of `+0x5D..+0x5F`
up 8 on both, then the counter `0x903848` up one, `+1` 3. `_End`: both freed
(`+0` 0), `Sprite_ReleaseTint` on each (`+4` read again after the first
call), `Effect_Release`.

### 1.6 Kind 0x87 (`EffectKind87_Run` `0x488F60`)

`_Start`: `+6` the low byte of `Draw_PassFlags`, which becomes 0x1B; E4B's
`0x489030`; `0x676280` 0; `+1` up. `_Step1`: when E4B's `0x489220` answers
0 (al), E4B's `0x4891F0`, `Sound_PlayEffect(0x203)`, `+1` up. `_Step2`: when
`0x489220` answers 0, `+1` up. `_Restore`: `Draw_PassFlags`' low byte from
`+6`, `+1` up. States 4..7 are `0x492750` (`+1` up; a catalog row no group
holds). `_End` (8): `Effect_Release`, then the word `0x8034E6` 0.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed. `DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables (grepped 2026-10-03). Where the original indexes past
a table ours aborts with a `Fatal` naming the function (the round-nine rule;
nothing in the fuzz reaches it): the five dispatchers past their tables, and
`Sprite_Objects` by an effect record's byte (`+3` / `+4` in kind 0x86, `+6` in
kind 0x85's `_ShowObjects`) past its 30 records.

## 3. The tables and the arguments

**The tables** (`symbols.toml` `[[data]]`): each the table's own length to the
next table a dispatcher indexes or to the first dword that is not code,
checked by hand against what the states store into `+1` (the tool's runs
counted on into the next table, the addendum's warning):

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind83_States` `0x654CE8` | 24 | `0x654D48`, kind 0x84's (the tool's run said 29) |
| `EffectKind84_States` `0x654D48` | 5 | a null dword at `0x654D5C` (entry 4 `Effect_StateRelease`, stored by no state) |
| `EffectKind85_States` `0x654D60` | 5 | `0x654D74`, kind 0x86's (the tool's run of nine counted it in) |
| `EffectKind86_States` `0x654D74` | 4 | a null dword at `0x654D84` |
| `EffectKind87_States` `0x654E88` | 9 | `0x654EAC`, kind 0x88's (`0x4896A0` indexes it; the tool said 28) |
| `EffectKind84_PushCounts` `0x654C68` | 16 bytes | read by `Rand & 0xF` |
| `EffectKind84_Waits` `0x654C78` | 16 bytes | read by `Rand & 0xF` |

`EffectKind82_States` is E3D's; eight of its entries (11..23 less `BareRet`,
`Effect_StateRelease` and the three shared with kind 0x83) are ours now.

**Arguments**: `Msg_OpenScript` is pushed `movzx cx` of a byte over a leftover
upper half (the standard row's `kU16` mask is what it reads); every other
argument is a whole immediate or a pointer. Nothing re-listed for a mask.

## 4. The fuzz (`effect_4a_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_4a`, effect mode (`g.effect`; kinds
0x82..0x87, each clone its own), 4,000 rounds a function
(`BOF3X_E4A_ONLY=<name>` runs the clones whose name holds it,
`BOF3X_E4A_ROUNDS` sets the rounds). All 51 clones are `kEffect`; the five
dispatchers' `state_span` is their table's length (section 3). The five tables
are `DataTable`s, swapped for recorders on both sides (E3D's
`EffectKind82_Start`, `Sprite_StateRestart`, `BareRet`,
`Effect_StateRelease` and `0x492750` among the entries). `0x488240`'s clone
moves MSVC's jump table (`JumpTable{0x58, 0x70, 5}`) and reads the byte table
in place. **Regions** beyond effect mode's standard ones: `0x676280` (4, kind
0x87's flag). Everything else is standard: the records, `Sprite_Current`,
`Sprite_Objects` (all 30), `ObjTrio` (the leader and the third member), the
counters `0x903848..0x90384B` and the count word `0x903850`, the chapter
bytes (the step `0x8034E5`, the word `0x8034E6`), `Field_State`,
`Field_Request`, `Input_Held`, `Draw_PassFlags`.

**Callees**: the effect-standard rows for `Effect_Release` (clears `+0..+4`),
`Sprite_FindFree` (a slot 0..29 or 0xFF a third of the time),
`Sprite_UpdateScreen` (logs `Sprite_Current` and its 0x80 bytes),
`Sprite_ReleaseTint`; the field-standard `Sprite_SetTint`; the standard
`Msg_OpenScript`, `Sound_PlayEffect`, `Rand`. **Listed in the group**:
`EffectKind85_ShowObjects` (the tail, `kPhase`); E4D's `0x48CA90` noting
`Sprite_Current` and its three colour bytes; E4B's `0x489030` and `0x4891F0`
(`kPhase`) and `0x489220` (`kFlag`, al). **Re-listed**: `EventOp_0x` - the
field-standard row leaves `Sprite_Current` alone; the real op points it at the
object it places and steps the count word, and so does this stand-in, so a
`_Spawn` that read `Sprite_Current` after the op rather than the record it
kept would be refused (control @C_NOTBACK@).

**Seeds** (every round, after the harness's fill): all 20 records' `+3`, `+4`,
`+6` below 30 (past them both sides write past `Sprite_Objects`, outside the
regions - section 7); the counter at 0x35, 0x34, 0x36, 0; `0x903849` at 3, 4,
5, 0xFF, 0xFE, 0, 6, 2; `0x90384A` at 0x80, 0, 0x10, 0xF, 0x11, 0x81; `+9` at
0, 1, 2, 8, 0x1E, 0xFF; `+2` at 0xE, 0xF, 0x10, 0, 0xFF; the message `+0xB`
at 0x36, 0x37, 0x35, 0x33, 0xFF; records 0 and 1's x at, one past, one short
of, a cell either side of and far from the leader's; the third member's x at
0x340000, one either side, 0, the most negative, -1; `Input_Held` at the three
counting words, 0x2001, 0x7000, 0x1000, 0; `Field_Request` at 2, 0, 1, 3 (each
value list with a random draw beside it). **Disturbance** (the group's, from
the hash only): `+9`, `+3` / `+4` / `+6` (below 30), `0x903849`, the third
member's x about the line, `+0xB`, `Input_Held`, the counter, `Field_Request`.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_4a`,
exit 0): @RESULT@

**Every shadow** (this worktree, no `bof3x.ini`): @STAR@

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 48 to the byte (3,632 bytes against the
  cut's 4,095; 45 differ by padding only), with one difference by code:
  `0x488240`'s cut size 464 runs over `0x4883D0`, a state of its own
  (`EffectKind83_States[2]`, "code no list has"). `0x488240`'s 0x181 bytes
  hold MSVC's switch tables after its last `ret`, as E3D's `_Wait` does.
- **`0x488B70`'s extent**: the tool read 0x68 bytes, through the `jmp
  0x488B90` and on to `0x488BD7`. `0x488B90` has its own frame (`push esi`)
  and `ret` and is the tail of four states (wave two's rule): taken as
  `EffectKind85_ShowObjects`, `0x488B70` cut to 0x18 bytes and the tail a call
  in ours.
- **Added `0x488BE0`**: kind 0x86's dispatcher (`Effect_KindHandlers[0x86]`),
  a catalog row ("Table Effect_KindHandlers") in no group of the cut, inside
  the band; its table's four states are rows of this group which the cut's
  unit column filed under kind 0x85. Taken (the addendum's rule).
- **Hidden starts**: all 48 of the cut's and two of the three added
  (`0x4883D0` is not hidden), each an entry by address - a cell of
  `Effect_KindHandlers` or of a kind's table - none a case or a shared tail.
  Their recorded host `0x487BF0` is E3D's `EffectKind81_FindFreeDrop`, whose
  `entries_logic.txt` line E3D cut to 0x1A; `0x433640`'s host `0x432F10` is
  `BattleFx_RollingDigits`, which ends at `0x43363E` - no host contains our
  code as a fall-through.
- **The `hypothesis` rows** (`0x433640`, `0x488220`, `0x4887E0`, `0x488F60`):
  three dispatchers and the shared one-line state - effect code, taken.
- **Not taken, in no group**: `0x492750` (`EffectKind87_States[4..7]` and
  `EffectKind80_States[4..7]`: `+1` up one; catalog part 6) - outside the band,
  the coordinator's to place.
- **The cut's `unit` / `label` columns**: "Fn_487C10 kind 0x82" for kind
  0x83's and 0x84's rows and "Fn_4889E0 kind 0x85" for kind 0x86's are the
  catalog's guesses; the code says the kinds above.

## 6. Controls

Planted in `effect_4a.cpp` one at a time by a script (scratch `controls.py`:
a unique anchor replaced, rebuild, run under `BOF3X_E4A_ONLY=<filter>`,
restore, rebuild; never with a commit in between). Counts are rounds refused
of 4,000 per function run, in this worktree; every refused run exited 3. At
least one plant a function, several for the larger; each dispatcher swapped
onto another table of at least its length.

@CONTROLS_TABLE@

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x83's `_Wait` checks record 0, not record 1.** Every other state
  of kind 0x83 pushes and checks `Sprite_Objects` record 1; its `_Wait` is
  kind 0x82's code copied whole and still compares record 0's x with the
  leader's for its 0x16 exit (and `_Finish`, shared, stores record 0's
  distance). With both kinds alive the 0x16 exit follows record 0.
- **`0x903849` 0xFE releases the record and runs on.** Kind 0x84's three
  states call `Effect_Release` on 0xFE and then carry on with the record they
  have just freed: the line check, `Rand`, and the writes of `+1` / `+9` land
  in a record whose `+0` is now 0 (the next `Effect_FindFree` may hand it out
  with those bytes set). `_Mash`'s seventeenth press and the line do the same
  after their own release only where the code returns at once - they do.
- **Kind 0x85's `_Wait` draws after its release**: at counter 0x35 it frees
  the record, then still calls the tile and `_ShowObjects`, reading `+6` and
  the colour of the freed record that frame.
- **The sprite indexes are unchecked.** Kind 0x86's `+3` / `+4` come from
  `Sprite_FindFree` (0..29, 0xFF checked in `_Spawn` only); kind 0x85's `+6`
  from `Area144_SpawnEffect85`, which takes `(Field_ActiveMember -
  Sprite_Objects) / 0xA4` without a check. A record whose bytes are past 29
  writes past `Sprite_Objects` into the draw records at `0x7E01C0`. Ours
  aborts.
- **`_Count18` can never reach 0xF in play**: `EffectKind82_Start` and
  `_Again` set `+2` 0 on every pass, so `+2` is 1 when `_Count18` reads it;
  the "0xF or more, back to 0xD" exit is dead unless `+2` is written
  elsewhere (nothing in the band does).
- **Kind 0x84's entry 4 (`Effect_StateRelease`) is unreachable**: no state
  stores 4.
- **The five dispatchers do not bound `+1`.** Every writer in the band stays
  inside its table (kinds 0x82 / 0x83's 0x16 / 0x17 among their 24); ours
  aborts past any of them.

## 8. Calls across groups

**Outbound, raw** (`band_rows.py --edges`: E4A to E4B 4, to E4D 4; both of
this wave, called by address through `effect_4a_callees.h` until they merge):

| Address | Owner | Called by |
|---|---|---|
| `0x48CA90` (the tile) | E4D | `EffectKind85_WaitMessage`, `_Brighten`, `_NextMessage`, `_Wait` |
| `0x489030` | E4B | `EffectKind87_Start` |
| `0x489220` (al) | E4B | `EffectKind87_Step1`, `_Step2` |
| `0x4891F0` | E4B | `EffectKind87_Step1` |

By address, Capcom's and in no group: `0x492750` through kind 0x87's table.
By name, already ours: `Effect_Release`, `Sprite_FindFree`, `EventOp_0x`,
`Sprite_SetTint`, `Sprite_ReleaseTint`, `Sprite_UpdateScreen`,
`Msg_OpenScript`, `Sound_PlayEffect`, `Rand`; through the tables E3D's
`EffectKind82_Start`, `Effect_StateRelease` (FC1) and `BareRet`.

**Inbound from outside the group** (for the rebinding pass):
- `Effect_RunObjects` reaches the five kinds through `Effect_KindHandlers`
  (read in place); E3D's `EffectKind82_Run` reaches the eight kind-0x82 states
  and the three shared through `EffectKind82_States`.
- `BattleFx_ActorWatch` `0x433460` (ours, `battle_fx_tasks`) reaches
  `Sprite_StateRestart` through its stack table - rebound (section 10).
- `Area144_SpawnEffect85` (`area_w3f`) writes kind 0x85 into a record; no
  call.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for every row of the band but one: `0x433640`'s combat
column holds -98 (the battle actor watch's path, `BattleFx_ActorWatch`'s
state 2; no first-call trace file names it). No first-call trace names any of
the 51 (`analysis/calltrace/reach_dragon`, `reach_whelp` and today's
`reach_balioAndSunder_1_1003`, `_2_1003`, `reach_bossAndFlash_1003`,
`reach_dragonGene_1003`: every file grepped for the addresses). **Fuzz
only** for the 50 effect functions. No live run was made (the brief). A
recorded walk through area 144's scene that spawns kind 0x85 would let the
coordinator's frame-hash A/B cover it; which scenes show kinds 0x82..0x84,
0x86 and 0x87 is the owner's to say.

## 10. The rebinding

`grep -rn -i` of the 51 addresses and the seven tables' in `src/game`
(`band_rows.py --refs` found four references to one function, `0x433640`):

- **Rebound**: `battle_fx_tasks.cpp`'s actor-watch handler table
  `H(0x433640)` now reads `H(bof3::addr::Sprite_StateRestart)` and
  `battle_fx_tasks_fuzz.cpp`'s `kWatchImm` entry `{0x1E, 0x433640}` reads
  `{0x1E, bof3::addr::Sprite_StateRestart}` (the values unchanged, so the
  battle fuzz's keys stand); the comments in `battle_fx_tasks.cpp` and
  `battle_fx_tasks_callees.h` name it.
- **Left raw**: none of ours. E3D's `symbols.toml` evidence for
  `EffectKind82_States` / `EffectKind82_Start` names `0x488020..0x4887B0` and
  `0x488220` by address (text, E3D's).

No raw reference to an E4A address sits in a file another group of this round
is writing.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03, a commented block): 51
lines, the read extents of the 51 (none was listed before). `00488B70` is
0x18 bytes; its tail `00488B90` (0x48) has its own line. No host line needed
fixing: `00487BF0` was cut to 0x1A by E3D, and `00432F10 280` ends before
`0x433640`.

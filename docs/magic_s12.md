# Group S12: Identify and Celerity (MAGIC060, MAGIC062)

**Status:** IN PROGRESS (2026-09-27). All 44 functions are ours
(`src/game/magic_s12.cpp`, shadow name `magic_s12`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 88,000 rounds. 346 of 348 negative controls refused by a count (exit 3); 2 equivalent, recorded with their reasons and a refused near variant each. Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fourth spell wave, group S12
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Read one id down | Extent | Functions |
|--:|---|---|---|---|--:|
| 39 | MAGIC060 | 060 | Identify | `0x4B0D50..0x4B1CAA` | 28 |
| 95 | MAGIC062 | 062 | Celerity | `0x4B1CB0..0x4B2F34` | 16 |

The extents are `tools/magic_rows.py --unit MAGIC060 / MAGIC062 --clones`
(capstone recursive descent; one jump table, in `Identify_DrawElements`, six
entries by its `cmp 5`, moved into the copy by the harness; nothing
`REFUSED`). MAGIC062's extent holds two functions that were ours already,
round eight's `BattleFx_TintActor` (`0x4B1E70`) and `BattleFx_Brighten`
(`0x4B1ED0`); the 44 are the rest, none found inside or missing from the
extents. 8,119 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. What the code does
fits the labels (MAGIC060 prints a panel about the target; MAGIC062 moves four
stats), but that is reading, not a measurement of play.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Identify (MAGIC060).**
  - `Identify_Task`, the kind-2 task: a five-entry stack table by +1.
  - `Identify_Start` makes two children (kind 1, parameter 0x12): the dim
    (+1 0) and the disc (+1 1); runs `Identify_Roll`; +0xB 0.
  - `Identify_Roll` compares the acting member's byte +0x8A (party record by
    the actor byte `0x904B34`) with the target enemy's word +0x98: a chance of
    16 in 16 at a gap of 15 or more, then 14 / 12 / 10 / 8 / 6 / 4 / 2 as the
    gap falls below 15 / 10 / 5 / 0 / -5 / -10 / -15, against `Rand & 0xF`.
    An enemy kind already marked shows (+4 1); else a hit on an enemy whose
    byte +0x8F is not 0 marks its kind and shows; else +4 0. The kind is the
    enemy's byte +0x8C, a bit of the 256-bit array at `0x9040A8`
    (`Identify_MarkSeen`, `Identify_WasSeen`): once identified, a kind shows
    every time after, whatever the roll.
  - `Identify_WaitOpen` waits for the disc to report (+0xB 2), then makes the
    third child (+1 2, the target's tint) and sets +9 0x10.
  - `Identify_ShowTimed` draws the panel for 16 frames, then
    `Identify_ShowUntilInput` draws it until the word `Input_Pressed` is not
    0 and sets +0xB 0x83 - the children's cue to close.
  - `Identify_End` waits for +0xB 0x80 (each of the three children takes one
    off as it ends), then the effect-done bit and free.
  - The panel: `Identify_DrawMember` for a party target (the member's name,
    then question marks for every value), `Identify_DrawEnemy` for an enemy
    (its name; for an enemy whose +0x8F is not 0 the six element glyphs
    `Identify_DrawElements`, the words +0x96 and +0x94 printed through
    `Crt_sprintf` with their labels, else question marks); then two item names
    (`Identify_DrawItem`, the words +0xA8 and +0xAC: the item in the low byte,
    the table - weapons, armour, accessories, else consumables - in the high
    byte), each shown only when the roll showed (+4), else question marks.
    Every text is centred by `0xA0 - 6 x Text_CharCount`.
  - `Identify_DrawElements` draws six 12 x 12 glyphs 0x14 apart, each bright
    (0x80) when the enemy's resistance byte (+0xBF, +0xC0, +0xC1, +0xC3, +0xC2,
    +0xC4) is 1 or less (the sixth: 3 or less), else dim (0x30).
  - The children (`IdentifyChild_Task`, three kinds by +1 through
    `IdentifyChild_Kinds`): the **dim** (`IdentifyDim_*`: +9 up to 0x10, a
    semi-transparent tile over the whole screen, grey +9; the owner's +0xB 1
    when it is in; closes on 0x83 by counting +9 down, `MagicFx_CountDownRelease`),
    the **disc** (`IdentifyDisc_*`: waits for the dim, grows +9 to 0x1E - one
    grey semi-transparent quad on screen round (0xA0, 0x78), radius
    3 x (+9 + 6), turning with +9 - sets the owner's +0xB 2; closes by +9
    down 2, `MagicFx_CountDown2Release`), and the **tint**
    (`IdentifyTint_*`: the target's sprite given a black tint record, its
    colour pulsed up to 8 and down to 0 on odd frames until the owner's +0xB
    has bit 0x80, then the tint released and the target flashed).
  - `MagicFx_CountDownRelease` (`0x4B1740`) is a child's generic last phase,
    in sixteen files' tables: +9 down, at 0 the owner's +0xB down and free.
    `MagicFx_CountDown2Release` (`0x4B18B0`, also reached from MAGIC111 and
    MAGIC172) the same by 2.
- **Celerity (MAGIC062)** is group S18's Buff (MAGIC082) with its own
  colours, no kind lookup and a stat change of its own:
  - `Celerity_Task`: a six-entry stack table - `Celerity_Start`, round
    eight's `BattleFx_TintActor` and `BattleFx_Brighten`, S18's
    `Buff_WaitChildren`, `Celerity_Apply`, `Celerity_End`.
  - `Celerity_Start`: the target sprite's facing and position to the task;
    five children (kind 1, parameter 0x4A): a ring (+1 0) and four sparks
    (+1 1, +4 0..3, +0xB 0 / 8 / 0x10 / 0x18); CLUT row 26's first 32 words
    restored; sound 0x100.
  - `Celerity_Apply`: the tint faded; at 0 the tint released, the target
    flashed, and for each of the four `MagicFx_BuffStats` bytes
    `Celerity_ApplyStat` and a popup child (kind 1, 0x48; +4 the index).
  - `Celerity_ApplyStat(stat)`: the target's result record (a member's
    +0x124, an enemy's +0x104) into `0x904B60`, its +4 / +6 / +8 cleared, the
    byte +0x14 + stat moved by the signed step at `NameTable_Abilities` record
    +3 of the ability word `0x904B80`, clamped to 100 / -100 (a clamp returns
    without the engine's `0x453300`, which is told of every other change).
  - `Celerity_End`: once the children have ended, the done bit and the byte
    `0x90465C` 5.
  - The ring (`CelerityRing_Run`: BarrierRing_Grow, BuffRing_Wait,
    `MagicFx_CountDownRelease`): a flat disc of sixteen triangles, radius
    0x80, centre grey +9 x 5 and a rim of three random shades
    ((`Rand & 3` + 3) x +9 per channel), and a band of sixteen quads out to
    0x100 in the same shades, under the actor matrix.
  - The sparks (`CeleritySpark_Run`: MagicFx_WaitA, ShieldSpark_Place, then
    `_Rise`, `_Fall`, `_Spin`, `_Orbit`) are BuffSpike_Arc, _Arc2, _Spin and
    _Orbit instruction for instruction but their two callees; `_Draw` is
    BuffSpike_Draw but its colour tables; `_DrawDisc` is
    MagicFx_DrawDiscRadius with the centre coloured by
    `CeleritySpark_DiscColors[+4]`.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the project's
two precedents:

- an index past any of the nine dispatch tables (two stack tables, seven
  `.data` tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3);
- `CeleritySpark_Draw` aborts where the original would store a depth outside
  its four-entry stack array (+9 read again between the two stores; S18's
  `BuffSpike_Draw`, the same code).

`Identify_DrawEnemy` passes `Identify_DrawItem` the item word with the
previous callee's upper half above it, as the original's `mov ax` leaves it,
and `Identify_DrawItem` passes that dword on as Text_DrawAt's count with the
name's length in its low byte; both are reproduced (the count is then huge,
so the name is drawn to its NUL). The x of every centred text is computed from
Text_CharCount's byte alone: the original's has the callee's upper half above
it, but `Text_DrawAt` stores x as a word, so the draw is the same.

## 3. Calls to other units

None by raw address into another spell group's unit. The one unnamed callee is
the engine's `0x453300` (a stat change for the target, as
`MagicFx_ApplyBuff`'s `0x44F650` calls it), in no group.

The `.data` tables name functions already ours, called through the table in
place: `MagicFx_ClearCount9` (S30), `BarrierRing_Grow` and `ShieldSpark_Place`
(S19), `BuffRing_Wait` and `Buff_WaitChildren` (S18), `MagicFx_WaitA` (S17).
By name: `BattleFx_TintActor`, `BattleFx_Brighten`, `MagicFx_PushActorMatrix`,
`BattleActor_UpdateScreenXY`, `MagicFx_LinkByDepth`, the text, GTE, GPU and
sprite library.

**For the coordinator:** `0x4B1740` is now `MagicFx_CountDownRelease` and
`0x4B18B0` `MagicFx_CountDown2Release`. Groups C3 (both), S03, S08 and S31
hold `0x4B1740` as a raw constant, S29 in its callee notes; S17, S18, S20, S25
and S30 name it only in comments (their tables are read in place). They work
as they are and can take the names.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `IdentifyChild_Kinds` | `0x65AAD0` | 3 |
| `IdentifyDim_Steps` | `0x65AADC` | 4 |
| `IdentifyDisc_Steps` | `0x65AAEC` | 4 |
| `IdentifyTint_Steps` | `0x65AAFC` | 3 |
| `CeleritySpark_ColorsUp` | `0x65AB0C` | 48 bytes |
| `CeleritySpark_ColorsDown` | `0x65AB3C` | 48 bytes |
| `CeleritySpark_DiscColors` | `0x65AB6C` | 12 bytes |
| `CelerityChild_Kinds` | `0x65AB78` | 2 |
| `CelerityRing_Steps` | `0x65AB80` | 3 |
| `CeleritySpark_Steps` | `0x65AB8C` | 6 |

Each count is where the next table starts (the dump of `0x65AAB0..0x65ABAC`,
2026-09-26): MAGIC060's texts (two labels, a mark, a row of marks) and its
number format sit between the tables at `0x65AAB0..0x65AACF` and `0x65AB08`;
MAGIC063's first table follows at `0x65ABA4`. The panel's other texts are the
engine's (`0x66A3E0`, `0x66A3E8`, `0x66A31C`), and the six element glyph codes
are reached through the pointer table `0x66A398`; none is named here.

## 5. The fuzz

`BOF3X_SHADOW=magic_s12` runs `magic_harness::Run` over the 44 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s12_fuzz.cpp`:

- **Callees** (37 listed; the standard set supplies the rest):
  - `Text_DrawAt` (x and y masked to words, as it stores them) and
    `Text_CharCount` (answering a byte 0..0x14) log the text they are given -
    its bytes to the NUL, and the pointer unless it lies in the caller's stack
    (the panel prints its numbers into a stack buffer, which differs between
    the clone's frame and ours);
  - `Crt_sprintf` runs for real on both sides (`kThrough`), so the printed
    number is what `Text_DrawAt` logs;
  - `Gfx_CommitPrim` and `MapView_LinkPrimAt` log each primitive's bytes and
    move `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's own;
    `MagicFx_LinkByDepth` logs its depths (`deref` 16) and the primitives it
    links;
  - the projections log their SVECTORs through `deref` (6 bytes each);
  - `Identify_WasSeen` answers as a flag; `Identify_DrawItem` answers garbage,
    whose upper half `Identify_DrawEnemy` passes on;
  - this group's own functions called directly, by address.
- **Clones:** `Identify_WasSeen` compares al (`ret_mask 0xFF`),
  `Identify_DrawItem` the whole of eax; `CeleritySpark_Draw` runs `calm` (it
  indexes its stack by +9 read again, S18's case).
- **Tables:** the seven `.data` tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `DamageScratch`; `Prim_VertexScratch`; `MoveScript_TintRecords`;
  the CLUT strip's 0x40 bytes from and to; `0x904B50..0x904B8F` (the result
  record pointer and the ability word); `0x90465C`; the seen bits
  `0x9040A8`; `Input_Pressed`. 22,906 bytes of state in 19 regions.
- **Seed:** each dispatcher inside its table; each count one step before and
  at its threshold; the owner's +0xB at each wait's value and either side;
  for the roll, the level gap at both sides of each of its seven thresholds
  and `Rand`'s first answer either side of the chance; for the panel, a party
  target for `Identify_DrawMember` (one of the five members the regions hold:
  it zeroes and restores a record byte, and a zero written outside the
  regions outlives the round - measured, 9 false mismatches before the seed
  kept it in) and an enemy target for `Identify_DrawEnemy` (its caller sends
  it nothing else; a party target indexes the enemies 253..255, outside the
  image's data, a fault on both sides); the resistance bytes 0..4; the tint
  record's red at 8 and 0 either side; `Celerity_ApplyStat`'s stat below
  0x10 and its cell so that it plus the ability's step lands at 99..101 or
  -99..-101; the spark spin at 3, 4, 5 and extremes (S18's).
- **Args:** `Identify_DrawItem` categories 0..4 and 0x80 with the item word's
  high byte the category half the time; `Identify_DrawElements` its caller's
  (0x66, 0x54) half the time; `CeleritySpark_DrawDisc` its callers' radii.
- **Disturb** (the group's case): `Gfx_PacketNext`, a scratch byte, a vertex
  word, a tint byte, `Input_Pressed`, a dword of the seen bits, and the
  target enemy's byte +0x8F (0 half the time). The last was added after the
  first controls run: control E26 (`Identify_DrawEnemy` reading +0x8F before
  a `Text_CharCount` call instead of after it) stood, because the harness's
  own disturbance writes one random byte of the target's record and almost
  never makes that one change between 0 and not 0. With it E26 is refused,
  and every control was run again on the changed fuzz (the table below).

Result in this worktree (2026-09-27):

    shadow      magic_s12 self-test: 88000 rounds over 44 functions (2000 each), 1062956 calls to the stand-ins,
                0 MISMATCHES; 22906 bytes of state (19 regions) and the stand-ins' log compared

Every callee listed and every handler the tables and immediates name was
called by the originals (coverage line in `build/bof3x.log`).
`BOF3X_SHADOW='*'`: exit 0 (1,063,818 stand-in calls for this group in that
run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

348 plants, each put in `magic_s12.cpp` one at a time by a script (not committed: `controls.py` and `run_controls.py` in `C:/Users/kerok/AppData/Local/Temp/claude/C--Users-kerok-Documents-GitHub-BreathOfFire3PCPort/9e63839e-3d62-41b0-af67-04c4e6eecc8c/scratchpad/s12/`, S31's pattern; plants of several edits are lists) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s12`, and restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches). **346 of 348 refused**, each by exit 3 with its mismatch counts in the table (two runs: the first, before the +0x8F disturbance, left E26 standing; this table is the second, on the committed fuzz). The I-, R-, M-, W-, D-, E-, T-, L- controls are Identify's task, roll, seen bits and panel; C-, F-, K-, V-, X-, Q-, N-, B- its children; G-, S-, A-, Z-, P- Celerity's task and stats; H-, RD-, BD-, SR-, SF-, SY-, SP-, O-, DR-, DD- its ring and sparks.

The thinnest (fewest rounds to the first difference):

- **T8** (DrawItem: the category's low word): 1
- **R9** (Roll: below -4): 4
- **E26** (DrawEnemy: +0x8F read before text 1 is counted): 4
- **I21** (ShowUntilInput: the input's low byte only): 5
- **R11** (Roll: below -16): 5
- **R5** (Roll: 15 inclusive): 6

Not refused:

- **D16** (DrawCentredCounted: the count's low word): equivalent: Text_CharCount is declared returning unsigned char, so the `& 0xFF` the plant drops was already a no-op in the source; its near variant D16b (`& 0x0F`) is refused
- **S20** (Start: the source read again for x): equivalent: no call lies between the entry's read of the source pointer and this one, so reading it again cannot differ; its near variant S20b (read after the ring's create) is refused

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| I1 | Identify_Task: entries 0/1 swapped | Identify_Task 801 |
| I2 | Identify_Task: entries 2/3 swapped | Identify_Task 775 |
| I3 | CallPhase: the phase from +2 (both tasks) | Identify_Task 1591, Celerity_Task 1682 |
| I4 | Start: first child parameter 0x13 | Identify_Start 2000 |
| I5 | Start: first child +1 1 | Identify_Start 1961 |
| I6 | Start: second child +1 2 | Identify_Start 2000 |
| I7 | Start: no roll | Identify_Start 2000 |
| I8 | Start: +0xB 1 | Identify_Start 2000 |
| I9 | Start: second child owned by itself | Identify_Start 1951 |
| I10 | WaitOpen: at 3 | Identify_WaitOpen 1482 |
| I11 | WaitOpen: child +1 3 | Identify_WaitOpen 998 |
| I12 | WaitOpen: +9 0x11 | Identify_WaitOpen 998 |
| I13 | WaitOpen: +9 on the child | Identify_WaitOpen 975 |
| I14 | WaitOpen: parameter 0x13 | Identify_WaitOpen 998 |
| I15 | DrawPanel: member below 4 | Identify_ShowTimed 126, Identify_ShowUntilInput 140 |
| I16 | DrawPanel: member / enemy swapped | Identify_ShowTimed 2000, Identify_ShowUntilInput 2000 |
| I17 | ShowTimed: +9 not down | Identify_ShowTimed 2000 |
| I18 | ShowTimed: on at 1 | Identify_ShowTimed 980 |
| I19 | ShowUntilInput: the test inverted | Identify_ShowUntilInput 2000 |
| I20 | ShowUntilInput: +0xB 0x84 | Identify_ShowUntilInput 1014 |
| I21 | ShowUntilInput: the input's low byte only | Identify_ShowUntilInput 5 |
| I22 | End: at 0x81 | Identify_End 1493 |
| I23 | End: done bit 3 | Identify_End 700 |
| I24 | ShowUntilInput: the panel after the test | Identify_ShowUntilInput 987 |
| R1 | Roll: enemy word +0x9A | Identify_Roll 112 |
| R2 | Roll: actor byte +0x8B | Identify_Roll 105 |
| R3 | Roll: the gap reversed | Identify_Roll 215 |
| R4 | Roll: top chance 15 | Identify_Roll 24 |
| R5 | Roll: 15 inclusive | Identify_Roll 6 |
| R6 | Roll: below 11 | Identify_Roll 8 |
| R7 | Roll: chance 11 below 5 | Identify_Roll 14 |
| R8 | Roll: 0 inclusive | Identify_Roll 8 |
| R9 | Roll: below -4 | Identify_Roll 4 |
| R10 | Roll: chance 5 below -10 | Identify_Roll 14 |
| R11 | Roll: below -16 | Identify_Roll 5 |
| R12 | Roll: hit at the chance | Identify_Roll 91 |
| R13 | Roll: Rand & 0x1F | Identify_Roll 92 |
| R14 | Roll: seen shows 2 | Identify_Roll 1340 |
| R15 | Roll: the hit ignored | Identify_Roll 187 |
| R16 | Roll: +0x8E | Identify_Roll 133 |
| R17 | Roll: not marked | Identify_Roll 158 |
| R18 | Roll: a miss shows | Identify_Roll 502 |
| R19 | Roll: WasSeen before Rand | Identify_Roll 2000 |
| R20 | Roll: +0x8F of the first target (not read again) | Identify_Roll 16 |
| R21 | Roll: seen tested whole | Identify_Roll 1340 |
| M1 | MarkSeen: kind +0x8D | Identify_MarkSeen 1522 |
| M2 | MarkSeen: dword by kind >> 4 | Identify_MarkSeen 1301 |
| M3 | MarkSeen: bit kind & 15 | Identify_MarkSeen 714 |
| M4 | MarkSeen: xor | Identify_MarkSeen 968 |
| M5 | MarkSeen: target - 2 | Identify_MarkSeen 1495 |
| W1 | WasSeen: bit kind & 30 | Identify_WasSeen 486 |
| W2 | WasSeen: inverted | Identify_WasSeen 2000 |
| W3 | WasSeen: kind +0x8D | Identify_WasSeen 979 |
| W4 | WasSeen: answers 2 | Identify_WasSeen 980 |
| W5 | WasSeen: the next dword | Identify_WasSeen 1030 |
| D1 | DrawMember: +0x88 zeroed and kept | Identify_DrawMember 2000 |
| D2 | DrawMember: not zeroed | Identify_DrawMember 1919 |
| D3 | DrawMember: put back without reading the target again | Identify_DrawMember 160 |
| D4 | DrawMember: put back before the draw | Identify_DrawMember 1919 |
| D5 | DrawMember: name y 0x31 | Identify_DrawMember 2000 |
| D6 | DrawMember: name count 6 | Identify_DrawMember 2000 |
| D7 | DrawMember: name at +0x81 | Identify_DrawMember 2000 |
| D8 | DrawMember: text 1 at y 0x45 | Identify_DrawMember 2000 |
| D9 | DrawMember: marks at x 0x71 | Identify_DrawMember 2000 |
| D10 | DrawMember: label 1 count 4 | Identify_DrawMember 2000 |
| D11 | DrawMember: a mark for label 2 | Identify_DrawMember 2000 |
| D12 | DrawMember: text 2 at y 0x8B | Identify_DrawMember 2000 |
| D13 | DrawMember: last marks at y 0xAD | Identify_DrawMember 2000 |
| D14 | DrawCentredCounted: centred on 0x9F | Identify_DrawMember 2000 |
| D15 | DrawCentred: five a character | Identify_DrawMember 1911, Identify_DrawEnemy 1899 |
| D16 | DrawCentredCounted: the count's low word | equivalent: Text_CharCount is declared returning unsigned char, so the `& 0xFF` the plant drops was already a no-op in the source; its near variant D16b (`& 0x0F`) is refused |
| D16b | DrawCentredCounted: the count's low 4 bits (D16's near variant) | Identify_DrawMember 804 |
| D17 | DrawMember: marks count 7 | Identify_DrawMember 2000 |
| E1 | DrawEnemy: record by target - 2 | Identify_DrawEnemy 2000 |
| E3 | DrawEnemy: +0x8E | Identify_DrawEnemy 1044 |
| E4 | DrawEnemy: the test inverted | Identify_DrawEnemy 2000 |
| E5 | DrawEnemy: elements at x 0x67 | Identify_DrawEnemy 959 |
| E6 | DrawEnemy: elements at y 0x55 | Identify_DrawEnemy 959 |
| E7 | DrawEnemy: first number +0x94 | Identify_DrawEnemy 959 |
| E8 | DrawEnemy: second number +0x96 | Identify_DrawEnemy 959 |
| E9 | DrawEnemy: number at x 0x71 | Identify_DrawEnemy 959 |
| E10 | DrawEnemy: second number count 6 | Identify_DrawEnemy 959 |
| E11 | DrawEnemy: label 1 at y 0x65 | Identify_DrawEnemy 959 |
| E12 | DrawEnemy: name at y 0x2F (identified) | Identify_DrawEnemy 959 |
| E13 | DrawEnemy: text 1 count + 1 (identified) | Identify_DrawEnemy 959 |
| E14 | DrawEnemy: text 1 centred on 0xA1 (not identified) | Identify_DrawEnemy 1041 |
| E15 | DrawEnemy: label 1 for label 2 (not identified) | Identify_DrawEnemy 1041 |
| E16 | DrawEnemy: text 2 at y 0x8B | Identify_DrawEnemy 2000 |
| E17 | DrawEnemy: first item +0xAA | Identify_DrawEnemy 2000 |
| E18 | DrawEnemy: second item +0xAE | Identify_DrawEnemy 2000 |
| E19 | DrawEnemy: first item without the upper half | Identify_DrawEnemy 2000 |
| E20 | DrawEnemy: second item without the upper half | Identify_DrawEnemy 2000 |
| E21 | DrawEnemy: category from the low byte | Identify_DrawEnemy 1993 |
| E22 | DrawEnemy: first item at y 0x99 | Identify_DrawEnemy 2000 |
| E23 | DrawEnemy: second item at y 0xAD | Identify_DrawEnemy 2000 |
| E24 | DrawEnemy: first number a byte | Identify_DrawEnemy 953 |
| E25 | DrawEnemy: the format one byte on | Identify_DrawEnemy 959 |
| E26 | DrawEnemy: +0x8F read before text 1 is counted | Identify_DrawEnemy 4 |
| E27 | DrawEnemy: second item's upper half from the text | Identify_DrawEnemy 2000 |
| T1 | DrawItem: marks when +4 is 1 | Identify_DrawItem 1328 |
| T2 | DrawItem: marks count 9 | Identify_DrawItem 1151 |
| T3 | DrawItem: weapons stride 27 | Identify_DrawItem 94 |
| T4 | DrawItem: armour stride 25 | Identify_DrawItem 91 |
| T5 | DrawItem: accessories stride 23 | Identify_DrawItem 98 |
| T6 | DrawItem: consumables stride 21 | Identify_DrawItem 564 |
| T7 | DrawItem: categories 1 and 2 swapped | Identify_DrawItem 185 |
| T8 | DrawItem: the category's low word | Identify_DrawItem 1 |
| T9 | DrawItem: item & 0x7F | Identify_DrawItem 426 |
| T10 | DrawItem: the count alone | Identify_DrawItem 849 |
| T11 | DrawItem: the count over the category | Identify_DrawItem 849 |
| T12 | DrawItem: centred on 0xA1 | Identify_DrawItem 849 |
| T13 | DrawItem: answers nothing | Identify_DrawItem 849 |
| T14 | DrawItem: category 3 as consumables | Identify_DrawItem 98 |
| L1 | DrawElements: resistances 4 and 5 swapped | Identify_DrawElements 777 |
| L2 | DrawElements: weak at 2 | Identify_DrawElements 1040 |
| L3 | DrawElements: the sixth weak at 2 | Identify_DrawElements 261 |
| L4 | DrawElements: dim 0x31 | Identify_DrawElements 1999 |
| L5 | DrawElements: the scratch dim 0x31 | Identify_DrawElements 1999 |
| L6 | DrawElements: bright 0x81 | Identify_DrawElements 1801 |
| L7 | DrawElements: bottom y + 0xD | Identify_DrawElements 2000 |
| L8 | DrawElements: right x + 0xD | Identify_DrawElements 2000 |
| L9 | DrawElements: step 0x15 | Identify_DrawElements 2000 |
| L10 | DrawElements: glyph byte whole | Identify_DrawElements 2000 |
| L11 | DrawElements: neighbouring glyph | Identify_DrawElements 2000 |
| L12 | DrawElements: clut row 0x1E1 | Identify_DrawElements 2000 |
| L13 | DrawElements: u 0xD | Identify_DrawElements 2000 |
| L14 | DrawElements: committed 0x24 | Identify_DrawElements 2000 |
| L15 | DrawElements: record by target - 2 | Identify_DrawElements 1825 |
| L16 | DrawElements: the target read again per glyph | Identify_DrawElements 683 |
| L17 | DrawElements: five glyphs | Identify_DrawElements 2000 |
| L18 | DrawElements: top y + 1 | Identify_DrawElements 2000 |
| C1 | IdentifyChild_Task: by +2 | IdentifyChild_Task 1361 |
| C2 | IdentifyDim_Run: drawn with +2 0 | IdentifyDim_Run 284 |
| C3 | IdentifyDim_Run: drawn with +0 0 | IdentifyDim_Run 738 |
| C4 | IdentifyDisc_Run: drawn with +2 0 | IdentifyDisc_Run 263 |
| C5 | IdentifyDisc_Run: draws the dim | IdentifyDisc_Run 742 |
| C6 | IdentifyTint_Task: by +1 | IdentifyTint_Task 1320 |
| C7 | IdentifyDim_Run: the step by +1 | IdentifyDim_Run 1486 |
| F1 | FadeIn: at 0x11 | IdentifyDim_FadeIn 991 |
| F2 | FadeIn: owner 2 | IdentifyDim_FadeIn 446 |
| F3 | FadeIn: +1 on | IdentifyDim_FadeIn 446 |
| K1 | CountDownRelease: the owner up | MagicFx_CountDownRelease 477 |
| K2 | CountDownRelease: at 1 | MagicFx_CountDownRelease 1005 |
| K3 | CountDownRelease: not freed | MagicFx_CountDownRelease 479 |
| V1 | DimDraw: tpage 0x56 | IdentifyDim_Draw 2000 |
| V2 | DimDraw: the tile on layer 2 | IdentifyDim_Draw 2000 |
| V3 | DimDraw: height 239.0 | IdentifyDim_Draw 2000 |
| V4 | DimDraw: width 322.0 | IdentifyDim_Draw 2000 |
| V5 | DimDraw: opaque | IdentifyDim_Draw 2000 |
| V6 | DimDraw: grey from +0xA | IdentifyDim_Draw 1995 |
| V7 | DimDraw: green + 1 | IdentifyDim_Draw 2000 |
| V8 | DimDraw: last mode 0x16 | IdentifyDim_Draw 2000 |
| V9 | DimDraw: the scratch not written | IdentifyDim_Draw 2000 |
| X1 | WaitDim: at 2 | IdentifyDisc_WaitDim 1462 |
| X2 | WaitDim: +9 1 | IdentifyDisc_WaitDim 953 |
| X3 | Grow: at 0x1F | IdentifyDisc_Grow 1020 |
| X4 | Grow: owner 3 | IdentifyDisc_Grow 505 |
| X5 | WaitClose: at 0x84 | IdentifyFx_WaitClose 1459 |
| X6 | WaitClose: from 0x83 up | IdentifyFx_WaitClose 490 |
| X7 | CountDown2Release: by 1 | MagicFx_CountDown2Release 2000 |
| X8 | CountDown2Release: the owner kept | MagicFx_CountDown2Release 824 |
| Q1 | DiscDraw: radius x 2 | IdentifyDisc_Draw 2000 |
| Q2 | DiscDraw: radius + 7 | IdentifyDisc_Draw 2000 |
| Q3 | DiscDraw: second corner + 0xF | IdentifyDisc_Draw 2000 |
| Q4 | DiscDraw: third corner - 3 | IdentifyDisc_Draw 2000 |
| Q5 | DiscDraw: angle mask 0x3F | IdentifyDisc_Draw 1756 |
| Q6 | DiscDraw: angle << 6 | IdentifyDisc_Draw 2000 |
| Q7 | DiscDraw: the cosine's angle not read back | IdentifyDisc_Draw 15 |
| Q8 | DiscDraw: the radius not read back for x | IdentifyDisc_Draw 452 |
| Q9 | DiscDraw: centre x 0x9F | IdentifyDisc_Draw 2000 |
| Q10 | DiscDraw: centre y 0x79 | IdentifyDisc_Draw 2000 |
| Q11 | DiscDraw: grey / 4 | IdentifyDisc_Draw 1955 |
| Q12 | DiscDraw: grey x 5 | IdentifyDisc_Draw 1977 |
| Q13 | DiscDraw: committed 0x40 | IdentifyDisc_Draw 2000 |
| Q14 | DiscDraw: last corner unshaded | IdentifyDisc_Draw 2000 |
| Q15 | DiscDraw: +9 read once | IdentifyDisc_Draw 380 |
| Q16 | DiscDraw: y by the sine | IdentifyDisc_Draw 2000 |
| N1 | TintStart: blue 1 | IdentifyTint_Start 2000 |
| N2 | TintStart: alpha 0 | IdentifyTint_Start 2000 |
| N3 | TintStart: the record into +0xA | IdentifyTint_Start 1999 |
| N4 | TintStart: +9 1 | IdentifyTint_Start 2000 |
| N5 | TintStart: not released | IdentifyTint_Start 2000 |
| N6 | TintStart: the actor's sprite tinted | IdentifyTint_Start 2000 |
| B1 | Brighten: bit 1 of the frame | IdentifyTint_Brighten 1001 |
| B2 | Brighten: green not up | IdentifyTint_Brighten 972 |
| B3 | Brighten: at 9 | IdentifyTint_Brighten 969 |
| B4 | Brighten: red at stride 11 | IdentifyTint_Brighten 969 |
| B5 | Brighten: green tested | IdentifyTint_Brighten 584 |
| N7 | Dim: red not down | IdentifyTint_Dim 1031 |
| N8 | Dim: back to step 0 | IdentifyTint_Dim 703 |
| N9 | Dim: at 1 | IdentifyTint_Dim 937 |
| N10 | Dim: bit 0x40 | IdentifyTint_Dim 999 |
| N11 | Dim: the actor flashed | IdentifyTint_Dim 426 |
| N12 | Dim: the owner up | IdentifyTint_Dim 456 |
| N13 | Dim: bit 1 of the frame | IdentifyTint_Dim 1012 |
| G1 | Celerity_Task: entries 2/3 swapped | Celerity_Task 646 |
| G2 | Celerity_Task: entries 4/5 swapped | Celerity_Task 671 |
| S1 | Start: facing from +9 | Celerity_Start 1916 |
| S2 | Start: z from +0x3C | Celerity_Start 2000 |
| S3 | Start: +9 0x11 | Celerity_Start 1889 |
| S4 | Start: +0xB 1 | Celerity_Start 1770 |
| S5 | Start: ring parameter 0x4B | Celerity_Start 2000 |
| S6 | Start: ring +9 1 | Celerity_Start 1845 |
| S7 | Start: ring z from x | Celerity_Start 2000 |
| S8 | Start: ring not counted | Celerity_Start 1784 |
| S9 | Start: spark +1 2 | Celerity_Start 2000 |
| S10 | Start: spark +4 its +0xB | Celerity_Start 2000 |
| S11 | Start: spark +0xB + 1 | Celerity_Start 2000 |
| S12 | Start: spark +9 without + 1 | Celerity_Start 2000 |
| S13 | Start: spark +0xA the task's +0xA | Celerity_Start 1993 |
| S14 | Start: three sparks | Celerity_Start 2000 |
| S15 | Start: 15 + 15 CLUT words | Celerity_Start 2000 |
| S16 | Start: second CLUT half from +0x22 | Celerity_Start 2000 |
| S17 | Start: dirty 2 | Celerity_Start 2000 |
| S18 | Start: sound 0x101 | Celerity_Start 2000 |
| S19 | Start: spark owned by itself | Celerity_Start 2000 |
| S20 | Start: the source read again for x | equivalent: no call lies between the entry's read of the source pointer and this one, so reading it again cannot differ; its near variant S20b (read after the ring's create) is refused |
| S20b | Start: the ring's x from the source, read after the create (S20's near variant) | Celerity_Start 115 |
| A1 | Apply: red at stride 11 | Celerity_Apply 1987 |
| A2 | Apply: blue not down | Celerity_Apply 2000 |
| A3 | Apply: +9 not down | Celerity_Apply 2000 |
| A4 | Apply: at 1 | Celerity_Apply 1001 |
| A5 | Apply: the actor flashed | Celerity_Apply 446 |
| A6 | Apply: the next stat | Celerity_Apply 480 |
| A7 | Apply: popup parameter 0x49 | Celerity_Apply 480 |
| A8 | Apply: popup +4 i + 1 | Celerity_Apply 480 |
| A9 | Apply: popup +9 5i | Celerity_Apply 480 |
| A10 | Apply: popup +0xA 12i + 2 | Celerity_Apply 480 |
| A11 | Apply: popups not counted | Celerity_Apply 480 |
| A12 | Apply: three stats | Celerity_Apply 480 |
| A13 | Apply: phase not on | Celerity_Apply 480 |
| A14 | Apply: popup before the stat | Celerity_Apply 480 |
| A15 | Apply: the tint not released | Celerity_Apply 480 |
| Z1 | End: byte 6 | Celerity_End 1019 |
| Z2 | End: at 1 | Celerity_End 1022 |
| Z3 | End: no done bit | Celerity_End 485 |
| P1 | ApplyStat: a member below 4 | Celerity_ApplyStat 172 |
| P2 | ApplyStat: member record + 0x120 | Celerity_ApplyStat 551 |
| P3 | ApplyStat: enemy record + 0x100 | Celerity_ApplyStat 1449 |
| P4 | ApplyStat: +4 not cleared | Celerity_ApplyStat 2000 |
| P5 | ApplyStat: +8 1 | Celerity_ApplyStat 2000 |
| P6 | ApplyStat: the step two bytes on | Celerity_ApplyStat 1568 |
| P7 | ApplyStat: records of 0x14 | Celerity_ApplyStat 1074 |
| P8 | ApplyStat: the cell + 0x15 | Celerity_ApplyStat 1293 |
| P9 | ApplyStat: 100 clamped | Celerity_ApplyStat 221 |
| P10 | ApplyStat: clamp 99 | Celerity_ApplyStat 429 |
| P11 | ApplyStat: -100 clamped | Celerity_ApplyStat 146 |
| P12 | ApplyStat: clamp -99 | Celerity_ApplyStat 178 |
| P13 | ApplyStat: no stat change call | Celerity_ApplyStat 1393 |
| P14 | ApplyStat: the call on a clamp too | Celerity_ApplyStat 429 |
| P15 | ApplyStat: the change told for the actor | Celerity_ApplyStat 1258 |
| P16 | ApplyStat: the cell unsigned | Celerity_ApplyStat 824 |
| P17 | ApplyStat: the stat's low nibble | Celerity_ApplyStat 341 |
| H1 | CelerityChild_Task: by +2 | CelerityChild_Task 1042 |
| H2 | Ring_Run: drawn with +0 0 | CelerityRing_Run 962 |
| H3 | Ring_Run: band before disc | CelerityRing_Run 1038 |
| H4 | Ring_Run: the step by +1 | CelerityRing_Run 1359 |
| H5 | Ring_Run: not popped | CelerityRing_Run 1038 |
| RD1 | RingDisc: tpage 0x36 | CelerityRing_DrawDisc 2000 |
| RD2 | RingDisc: centre x 4 | CelerityRing_DrawDisc 1986 |
| RD3 | RingDisc: Rand & 7 | CelerityRing_DrawDisc 1712 |
| RD4 | RingDisc: rim + 2 | CelerityRing_DrawDisc 1991 |
| RD5 | RingDisc: first radius << 8 | CelerityRing_DrawDisc 1998 |
| RD6 | RingDisc: fifteen triangles | CelerityRing_DrawDisc 2000 |
| RD7 | RingDisc: steps of 0x80 | CelerityRing_DrawDisc 2000 |
| RD8 | RingDisc: centre z 1 | CelerityRing_DrawDisc 2000 |
| RD9 | RingDisc: opaque | CelerityRing_DrawDisc 2000 |
| RD10 | RingDisc: quad depths | CelerityRing_DrawDisc 2000 |
| RD11 | RingDisc: rim green from blue | CelerityRing_DrawDisc 1530 |
| RD12 | RingDisc: committed 0x30 | CelerityRing_DrawDisc 2000 |
| RD13 | RingDisc: centre from byte 7 | CelerityRing_DrawDisc 1989 |
| RD14 | RingDisc: last mode layer 4 | CelerityRing_DrawDisc 2000 |
| RD15 | RingDisc: +9 not read again after Rand | CelerityRing_DrawDisc 196 |
| BD1 | RingBand: inner radius << 6 | CelerityRing_DrawBand 1995 |
| BD2 | RingBand: outer radius << 7 | CelerityRing_DrawBand 1994 |
| BD3 | RingBand: outer copied from inner | CelerityRing_DrawBand 2000 |
| BD4 | RingBand: outer z 1 | CelerityRing_DrawBand 2000 |
| BD5 | RingBand: inner colour on the outer edge | CelerityRing_DrawBand 2000 |
| BD6 | RingBand: outer 2 | CelerityRing_DrawBand 2000 |
| BD7 | RingBand: committed 0x40 | CelerityRing_DrawBand 2000 |
| BD8 | RingBand: seventeen quads | CelerityRing_DrawBand 2000 |
| BD9 | RingBand: projected as three | CelerityRing_DrawBand 2000 |
| BD10 | RingBand: tpage 0x34 | CelerityRing_DrawBand 2000 |
| SR1 | Spark_Run: by +1 | CeleritySpark_Run 1644 |
| SR2 | Rise: disc 0x19 | CeleritySpark_Rise 2000 |
| SR3 | Rise: speed -15 | CeleritySpark_Rise 418 |
| SR4 | Rise: acceleration 5 | CeleritySpark_Rise 418 |
| SR5 | Rise: +0xA 9 | CeleritySpark_Rise 418 |
| SR6 | Fall: 0x1D - +0xB | CeleritySpark_Fall 397 |
| SR7 | Fall: disc 0x20 | CeleritySpark_Fall 2000 |
| SF1 | SparkFrame: Rand & 7 | CeleritySpark_Rise 1006, CeleritySpark_Fall 1019, CeleritySpark_Orbit 1002 |
| SF2 | SparkFrame: drawn before the matrix | CeleritySpark_Rise 2000, CeleritySpark_Fall 2000, CeleritySpark_Orbit 2000 |
| SY1 | SparkFly: bit 1 of the frame | CeleritySpark_Rise 1209, CeleritySpark_Fall 1236 |
| SY2 | SparkFly: speed less the acceleration | CeleritySpark_Rise 2000, CeleritySpark_Fall 2000 |
| SY3 | SparkFly: height by +0x16 | CeleritySpark_Rise 2000, CeleritySpark_Fall 2000 |
| SY4 | SparkFly: at 1 | CeleritySpark_Rise 862, CeleritySpark_Fall 813 |
| SP1 | Spin: wide below 6 | CeleritySpark_Spin 63 |
| SP2 | Spin: wide 0x41 | CeleritySpark_Spin 237 |
| SP3 | Spin: up every fourth | CeleritySpark_Spin 82 |
| SP4 | Spin: up to 5 | CeleritySpark_Spin 303 |
| SP5 | Spin: the angle less the spin | CeleritySpark_Spin 1822 |
| SP6 | Spin: sound for the second | CeleritySpark_Spin 93 |
| SP7 | Spin: sound 0x102 | CeleritySpark_Spin 39 |
| SP8 | Spin: +0x14 9 | CeleritySpark_Spin 141 |
| SP9 | Spin: +0x20 7 | CeleritySpark_Spin 141 |
| SP10 | Spin: +0x10 16 | CeleritySpark_Spin 141 |
| SP11 | Spin: spin up 2 | CeleritySpark_Spin 317 |
| SP12 | Spin: at spin 3 | CeleritySpark_Spin 531 |
| O1 | Orbit: +0x10 up 3 | CeleritySpark_Orbit 2000 |
| O2 | Orbit: +0xB up 1 | CeleritySpark_Orbit 2000 |
| O3 | Orbit: angle mask 0x3F | CeleritySpark_Orbit 1002 |
| O4 | Orbit: radius + 0xB1 | CeleritySpark_Orbit 1997 |
| O5 | Orbit: x sar 4 | CeleritySpark_Orbit 2000 |
| O6 | Orbit: z from the owner's x | CeleritySpark_Orbit 2000 |
| O7 | Orbit: at 0x11 | CeleritySpark_Orbit 729 |
| O8 | Orbit: disc 0x21 | CeleritySpark_Orbit 2000 |
| O9 | Orbit: the spin into +0xB | CeleritySpark_Orbit 1992 |
| DR1 | SparkDraw: apex 0x51 | CeleritySpark_Draw 2000 |
| DR2 | SparkDraw: radius 0x31 | CeleritySpark_Draw 2000 |
| DR3 | SparkDraw: second angle + 7 | CeleritySpark_Draw 2000 |
| DR4 | SparkDraw: colour row i + 1 | CeleritySpark_Draw 2000 |
| DR5 | SparkDraw: blue from green | CeleritySpark_Draw 1680 |
| DR6 | SparkDraw: semi-transparent | CeleritySpark_Draw 2000 |
| DR7 | SparkDraw: packet step 0x30 | CeleritySpark_Draw 2000 |
| DR8 | SparkDraw: three a row | CeleritySpark_Draw 2000 |
| DR9 | SparkDraw: first row linked as three | CeleritySpark_Draw 2000 |
| DR10 | SparkDraw: second row bias 3 | CeleritySpark_Draw 2000 |
| DR11 | SparkDraw: the rows' tables swapped | CeleritySpark_Draw 2000 |
| DR12 | SparkDraw: second link x / z swapped | CeleritySpark_Draw 2000 |
| DR13 | SparkDraw: tpage 0x35 | CeleritySpark_Draw 2000 |
| DR14 | SparkDraw: second row not turned down | CeleritySpark_Draw 2000 |
| DR15 | SparkDraw: centre y 1 | CeleritySpark_Draw 2000 |
| DR16 | SparkDraw: index word + 1 | CeleritySpark_Draw 2000 |
| DR17 | SparkDraw: first link at the task's z twice | CeleritySpark_Draw 2000 |
| DD1 | SparkDisc: radius a byte | CeleritySpark_DrawDisc 628 |
| DD2 | SparkDisc: colours by 4 x +4 | CeleritySpark_DrawDisc 1661 |
| DD3 | SparkDisc: green from blue | CeleritySpark_DrawDisc 1573 |
| DD4 | SparkDisc: steps of 0x100 | CeleritySpark_DrawDisc 2000 |
| DD5 | SparkDisc: centre y from +0x2E | CeleritySpark_DrawDisc 2000 |
| DD6 | SparkDisc: rim 2 | CeleritySpark_DrawDisc 2000 |
| DD7 | SparkDisc: triangle linked 0x30 | CeleritySpark_DrawDisc 2000 |
| DD8 | SparkDisc: triangle dy 3 | CeleritySpark_DrawDisc 2000 |
| DD9 | SparkDisc: mode linked 0x10 | CeleritySpark_DrawDisc 2000 |
| DD10 | SparkDisc: first rim x sar 11 | CeleritySpark_DrawDisc 2000 |
| DD11 | SparkDisc: second rim y by the sine | CeleritySpark_DrawDisc 2000 |
| DD12 | SparkDisc: seven triangles | CeleritySpark_DrawDisc 2000 |
| DD13 | SparkDisc: opaque | CeleritySpark_DrawDisc 2000 |
| DD14 | SparkDisc: centre red from green | CeleritySpark_DrawDisc 1289 |

## 7. What nothing reached

- **What the spells look like**, and whether the labels read one id down are
  right: nothing recorded casts them. DIV-0045's cheats can put the skills in
  a list for the owner's eye.
- **The panel's texts.** The fuzz logs what `Text_DrawAt` is given, not what
  the glyphs look like; `0x66A3E0` and `0x66A3E8` are the engine's and were
  not read (they may be filled at run time: the target's and the ability's
  names are candidates, unmeasured).
- **The item category.** The high byte of the words +0xA8 / +0xAC is read as
  the table (1 weapons, 2 armour, 3 accessories, else consumables); that the
  enemy records hold those values there is not measured.
- **`0x453300`** is Capcom's and unread beyond its caller; the recorder logs
  the target it is given.

## 8. Latent defects (described, not fixed)

- **Identify on a party member reads the "enemy" below the enemy records.**
  `Identify_Start` runs the roll whatever the target; for a member (0..2) the
  roll's enemy record is index -3..-1 - the tail of the battle task slots
  (`0x93B960 - 3 x 0x128`). Its "level" (+0x98) is whatever a task left there,
  and if its +0x8F is not 0 and the roll hits, `Identify_MarkSeen` sets the
  seen bit of the "kind" at its +0x8C: casting on a member can mark an
  arbitrary enemy kind identified. By reading; whether the skill can target a
  member is not measured. Faithful in ours.
- **The dispatch tables are unbounded** (the two stack tables by +1, the seven
  `.data` tables by +1 / +2): ours aborts past each.
- **`BattleTask_Create`'s "none free" (0xFF) is never tested** by
  `Identify_Start`, `Identify_WaitOpen`, `Celerity_Start` or `Celerity_Apply`:
  slot 255 is written 0x84 x 255 bytes past the slot table. Faithful.
- **`CeleritySpark_Draw`'s stack index** (S18's `BuffSpike_Draw`): +9 read
  again after the GTE calls indexes a four-entry array; nothing moves +9 in
  the game. Ours aborts outside 0..3.
- **`Identify_DrawItem` reads past the name tables** for an item byte past a
  table's records (92 consumables, 83 weapons, ...), into the next table: a
  wrong name, not a fault. **`Identify_DrawMember`** restores the name byte
  through the target read again after two calls; nothing moves the target
  between them in the game.
- **`MagicFx_CountDown2Release` with an odd +9** wraps through 0xFF and ends
  128 frames later; `IdentifyDisc_Grow` leaves +9 at 0x1E (even), so not by
  this spell.

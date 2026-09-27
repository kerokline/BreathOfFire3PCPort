# Group S12: Identify and Celerity (MAGIC060, MAGIC062)

**Status:** IN PROGRESS (2026-09-27). All 44 functions are ours
(`src/game/magic_s12.cpp`, shadow name `magic_s12`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 88,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
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
  word, a tint byte, `Input_Pressed`, a dword of the seen bits.

Result in this worktree (2026-09-27):

    shadow      magic_s12 self-test: 88000 rounds over 44 functions (2000 each), 1062954 calls to the stand-ins,
                0 MISMATCHES; 22906 bytes of state (19 regions) and the stand-ins' log compared

Every callee listed and every handler the tables and immediates name was
called by the originals (coverage line in `build/bof3x.log`).
`BOF3X_SHADOW='*'`: exit 0 (1,063,818 stand-in calls for this group in that
run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

CONTROLS_TEXT

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

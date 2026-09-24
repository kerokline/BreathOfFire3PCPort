# The encounter placement and the battle intro's party steps

**Status:** IN PROGRESS (2026-09-24) - 25 functions ours
(`src/game/inventory_ops.cpp`, shadow name `inventory_ops`), each fuzzed
headless against a byte copy of Capcom's with every call re-aimed at a
recorder: **25 x 2,000 rounds, about 420,000 stand-in calls, 0 mismatches**;
**80 negative controls, 76 refused by a count, 4 not refused - each a change that changes nothing** (section 6). No divergence. **Not yet through the live
check** - the combat A/B (`analysis/validate_combat.sh`) runs centrally after
the merge.

Group BI of the seventh round ([`takeover-queue-round7.md`](takeover-queue-round7.md)),
"inventory operations and the event-script bits on the way". Read, it is
neither. The seventeen `0x5920E0..0x592E30` are **the random encounter's
placement on the field map** - where the party and the enemies stand for a
fight that happens where the party walked, and whether it fits on screen -
all under `0x591F30`, which `Field_EncounterDue`'s caller asks at
`0x53013C`. The other eight are **the battle intro's party steps**: the
initiative roll, the party turning to face the fight, the shade fade out,
the walk to the placed spots and the fade back, called from the intro's
state machine at `0x495E90` (a jump table `0x656A84` by the word
`0x66C7EA`), plus two sprite helpers. The file and shadow names stay as the
round gave them. That this is a battle set-up is read from the callers
(`Field_EncounterDue`, the battle's own intro); the owner has not confirmed
what is on screen at each step, so every name that says what a step *means*
is `hypothesis` (game facts are the owner's).

## 1. The functions

Bytes are each body to its last instruction (capstone, 2026-09-23); none has
a jump table and no jump leaves any of them. "Route" is the call count in
`analysis/combat_catalog.md` (the owner's combat route, `recipe_combat`).
PSX twins are `GAME.EMI` section 0 (the sibling's
`analysis/ghidra/GAME_EMI0_80196800_decomp/`), from
`analysis/pairs_propagated.json` and read side by side.

| PC | Name | Bytes | PSX | Route | What |
|---|---|--:|---|--:|---|
| `0x5920E0` | `Encounter_PlaceParty` | 0x1B3 | `801C73A8` | 1 | sort, recentre, place each member |
| `0x5922A0` | `Encounter_PlaceMember` | 0x157 | `801C7650` | 3 | one member's spot, 16 tries |
| `0x592400` | `Encounter_MemberClear` | 0xD7 | `801C7880` | 3 | apart from the others, reached from the centre |
| `0x5924E0` | `Encounter_MemberStands` | 0x82 | `801C79C0` | 3 | fits the cell map, no object there |
| `0x592600` | `Encounter_PlaceEnemies` | 0x153 | `801C7BAC` | 1 | each enemy slot, 49 jitters |
| `0x592760` | `Encounter_EnemiesReachable` | 0x97 | `801C7DC4` | 1 | each enemy reached by some member |
| `0x592800` | `Encounter_EnemyClear` | 0x85 | `801C7ED8` | 1 | apart from the enemies before it |
| `0x592890` | `AreaMap_CellNibble` | 0x5B | `801C7FCC` | 17 | a nibble of the cell map `0x8C3D80` |
| `0x5928F0` | `Encounter_OnScreen` | 0xDE | `801C805C` | 4 | four corners on screen, a margin above |
| `0x5929D0` | `Encounter_Project` | 0x54 | `801C81BC` | 20 | a ground point's screen point |
| `0x592A30` | `Encounter_AimCamera` | 0x191 | `801C823C` | 1 | the mean height, the camera matrix |
| `0x592BD0` | `Encounter_Apart` | 0x5A | `801C8490` | 3 | two sizes apart in x or z |
| `0x592C30` | `Encounter_CellFits` | 0x91 | `801C8510` | 4 | a size class against the nibble |
| `0x592CD0` | `Encounter_PathClear` | 0x125 | `801C881C` | 4 | a stepped walk, both ways |
| `0x592E00` | `Short_Abs` | 0xC | `801C8180` | 8 | absolute value of a word |
| `0x592E10` | `Short_Sign` | 0x1C | `801C81A4` | 8 | sign of a word |
| `0x592E30` | `Encounter_StepOpen` | 0xCE | `801C8600` | 13 | one step open by the nibbles |
| `0x52F570` | `Sprite_TurnSense` | 0x41 | `801B3CF8` | 3 | which way to turn to a direction |
| `0x532550` | `Encounter_RollInitiative` | 0x108 | `801BFBA8` | 1 | who notices, `0x904AE4` 0 / 1 / 2 |
| `0x532660` | `Encounter_PartyTurnSense` | 0x4F | `801BFDC8` | 1 | each member's turn sense |
| `0x5326B0` | `Encounter_PartyTurn` | 0x1A1 | `801BFE54` | 6 | a frame of turning; then the fade begins |
| `0x532860` | `Encounter_PartyToPlaces` | 0x202 | `801C0114` | 16 | a frame of fade out, move, fade in |
| `0x532A70` | `Encounter_PartyAtPlaces` | 0xE2 | none paired | 1 | everyone at the spots, the battle pose |
| `0x532B60` | `Encounter_PartyScriptOnce` | 0xAE | none paired | 42 | a frame of each member's script |
| `0x534880` | `Sprite_ShadeLower` | 0x92 | `801C3998` | 24 | `Sprite_ShadeRaise`'s mirror |

Every one is reached by the combat route (it is the catalogue's). Differences
from the PSX are of form - stride `0x140` there, `0x14C` here, the
depth written where the PC leaves it - except the two in section 5.

**For `entries_logic.txt`**: the catalogue's sizes are right but one:
`0x52F570` is `0x41`, not 0x50 (`pe_funcs.py` ran into the padding).

## 2. The addresses

Named and used by name: `Sprite_Current`, `Field_State`,
`Field_MemberCount`, `ObjTrio`, `Effect_Objects`, `Draw_OtSlot`,
`Gfx_ClutStripDirty`, `Field_Kind2X` / `Z`, `Camera_Angles`,
`Camera_ShiftX` / `ShiftY` / `Distance`, `AreaMap_Header`. The rest are
constants in `inventory_ops_callees.h` (so no other group can bind one):

| Address | What |
|---|---|
| `0x6BE070` | the formation's facing 0..3 (PSX `0x801CE054`) |
| `0x6BE071` | the enemies placed / reached, counted in memory |
| `0x6BE074` | the members' placing order, 3 bytes |
| `0x6BE078` | 4 bytes per member: +0 placed, +1 "wide" (its actor record 2 or 6) |
| `0x6BE084` | the enemy count `0x5925A0` chose |
| `0x6BDFF0` | a scratch sprite `0x591F30` makes `Sprite_Current` (+0x3C written) |
| `0x903780` / `0x903784` | the fight's centre, 16.16 (PSX `0x80143F24`) |
| `0x7E06E0` | each member's spot, x and z (PSX `0x80143F2C`) |
| `0x939F00` | 8 enemy slots of 12 bytes: +0 active, +1 kind, +4 x, +8 z |
| `0x939860` | the mean ground height, low 5 bits cleared |
| `0x8C55C8` | the enemy kinds, 0x8C bytes: +0x86 size class, +0x87 margin |
| `0x8C3D80` | the cell map, a nibble per cell, `AreaMap_Header` bytes 0 / 1 its extent |
| `0x6698B0`, `0x669B60`, `0x669AF8`, `0x669C80`, `0x669CB8` | the placement's tables: party offsets by formation, enemy offsets by count, 49 jitters, 6 recentres, camera offsets by facing |
| `0x904060`, `0x904062`, `0x904065` | the formation byte; the party list; the second list the placement reads |
| `0x66972C` | a member id's actor record (symbols.toml's `MoveScript_EffectState`) |
| `0x904AAC`, `0x904AE4`, `0x904AE5` | the battle's facing; the initiative; the intro's flags (bit 2 skips the fade, bit 4 the script step) |
| `0x660B3C`, `0x660B1C`, `0x660B24` | per facing: the direction to face, the centre's cell step; per member id: the alert effect's two bytes |
| `0x80D380`, `0x80D440`, `0x811380` | the shade CLUTs: source row, the saved copy, `Gfx_ClutStrip` row 15 |

## 3. The placement

`0x591F30` (not ours) picks the enemies (`0x592570`, `0x5925A0`), sets the
facing, then asks, in order: `Encounter_PlaceParty`,
`Encounter_PlaceEnemies`, `Encounter_EnemiesReachable`; then aims the camera
and checks every member and enemy with `Encounter_OnScreen`.

- **The party** (`Encounter_PlaceParty`): the wide members first (a bubble
  sort of n passes on the order bytes, read from memory, so it runs on into
  the member bytes past three); if the sort moved any, the centre is retried
  at six offsets until one fits and is reachable - and ends on the sixth
  when none is. Each member (`Encounter_PlaceMember`) starts at the
  formation's offset turned to the facing, then walks eight steps along one
  axis (a Rand bit picks it) and eight along the other, half a cell at a
  time, until `Encounter_MemberClear` (apart from the placed members,
  reached from the centre) and `Encounter_MemberStands` (fits the cell map,
  no object there).
- **The enemies** (`Encounter_PlaceEnemies`): per active slot an offset from
  a triangle table by (count, placed so far), five cells out, turned to the
  facing, then up to 49 jitters; a slot that never fits is dropped.
  `Encounter_EnemiesReachable` then drops those no member can walk to.
- **The geometry.** `AreaMap_CellNibble` reads the map's nibble (0 off the
  map). `Encounter_CellFits` wants it at least `4 * size + 1` (`b`),
  anything from `b + 4` fits, and between the corner's place in its cell
  decides (on both edges `b`; off the z edge `b + 2` or `b + 3`; off the x
  edge `b + 1` or `b + 3`). `Encounter_StepOpen` reads the nibble as a
  passage class: along x 2 or more but not 3, along z 3 or more, a diagonal 5
  or more (one sign) or by its far cell (the other). `Encounter_PathClear`
  walks x then z per turn within a byte budget of `|dx| + |dz|`, and tries
  the other direction if that fails. What the nibble values *are* (heights,
  walls, water) is not read here.
- **On screen** (`Encounter_OnScreen`): the four corners of the footprint
  projected (`Encounter_Project`, `Gte_RotTransPers`), each below x 320 and
  y 240, then the first corner's y less the margin at least 8.

## 4. The intro's party steps

In the intro's order: `Encounter_RollInitiative` (via `0x52F429`) rolls for
five slots - the three members and two past them - `r` in 0..100; a member
whose actor record's byte `+0x38` is at least `r` notices (an alert effect,
kind 6, over it), a slot past the party counts on `r <= 50`; all five gives
`0x904AE4 = 1`, none `2`, else `0`. `Encounter_PartyTurnSense` sets each
member's turn sense to the fight's direction; `Encounter_PartyTurn` turns
them a step a frame (a member whose record has bit 14 of `+0x10` just takes
its pose), then saves each member's CLUT and starts the shade fade;
`Encounter_PartyToPlaces` lowers each shade (`Sprite_ShadeLower`, to
`0x80`), moves the member to its placed spot, and raises the shade back
(`Sprite_ShadeRaise`), restoring the CLUT; `Encounter_PartyAtPlaces` puts
everyone on the spots in the battle pose (animation = facing, + 0x1C when
bit 6 of the record's `+0x11`); `Encounter_PartyScriptOnce` runs each
member's script until all have finished. What the numbers mean on screen
(which of 1 and 2 is the ambush) is not read and is the owner's to say.

## 5. Found on the way

- **The recentre checks the path from (Z, Z)** (`Encounter_PlaceParty`,
  `0x592215`): it passes the centre's z cell as both the start's x and z of
  `Encounter_PathClear`. The PSX does the same (`FUN_801C73A8` passes
  `_DAT_80143f2a` twice), so it is Capcom's, not the port's; and when none of
  the six recentres passes, the centre still moves to the sixth. Latent in
  the sense that it only runs when the sort moved a wide member. Kept.
- **`Encounter_OnScreen` lost its lower bounds in the port.** The PSX
  compares the projected point as unsigned shorts against `0x13F` / `0xEF`,
  so a corner left of or above the screen fails; the PC compares floats
  against 320.0 / 240.0 only, so such a corner passes. A fight placed partly
  off the left or top edge is possible on the PC. Kept (faithful); a fix
  would be a ledgered divergence.
- **`Encounter_AimCamera` divides by the number of things averaged with no
  guard** (the PSX traps the same); there is always somebody in a fight.
- **`Sprite_ShadeLower` wraps above 127**: a shade byte of `0x7F` less a
  negative step becomes `0x80` and more; the callers step by 8 and 4.
- **"All five noticed" needs the two slots past the party to roll 50 or
  less** (`Encounter_RollInitiative` walks five over a three-member
  `ObjTrio`, as on the PSX): a party that notices everything still gets
  `0x904AE4 = 1` only a quarter of the time. Whether that is intended is the
  owner's call; kept.
- Two functions (`0x532A70`, `0x532B60`) search the first party list for the
  member's id and never use the answer: a dead read, not reproduced.
- `AreaMap_CellNibble` was called through a raw address by
  `member_sprites` (`0x51B935`); it is named now, and that call site is fine
  as it is.
- For others (said, not bound): `0x591F30` (the placement's driver),
  `0x592570` (picks an encounter row from the 9-byte table `0x8C5588` by
  Rand) and `0x5925A0` (fills the enemy slots from the row), all nobody's
  and pointer-less by name; `0x5891C0` (next round) is `(animation, 0,
  buffer, size)`: `0x589160` copies from the sprite's `+0x50`, the sprite's
  `+0x4B` becomes the animation, then `0x589350`.

## 6. The fuzz, and the controls

`BOF3X_SHADOW=inventory_ops`, at start-up: 25 copies, every call re-aimed at
a recorder (`bof3::CloneCall` with the expected callee). 2,000 rounds per
function: random bytes over every region any of them touches (the placement
state and scratch sprite, 256 members' spots and enemy slots, the centre,
the party lists, the intro's bytes, five `ObjTrio` slots, 20 effect
objects, nine actor records, the shade CLUTs, the pointers and counts, the
camera words, 16 enemy kinds, `AreaMap_Header`), the cell map filled for the
nibble's rounds; then each function's boundaries seeded: member and facing
bytes 0..7 / `0xFE` / `0xFF` with stale bits above, map extents 0 / 1 / 255
and coordinates at the width and depth and one either side, words
`0x7FFF` / `0x8000` / `0xFFFF`, steps equal / one off / across the budget of
128, separations at the sum and one either side (and across the 32-bit
wrap), fractions 0 or not, projected points at 320 / 240 exactly and at the
margin + 8, NaNs, shade bytes `0x80` / `0x7F` / `0x81` with steps `0x7F` /
`0x80` / `0xFF`, record bytes 50 / 51 / 100 / 101, the intro's flag bits.
Theirs, then ours from the same state, under x87 control word `0x027F`; the
regions, the answer at the original's width and the recorders' log compared.

The recorders move, at random, everything some function reads again after a
call (the centre, spots, placed and wide bytes, order, facing, counts, enemy
slots, the height sum, `Sprite_Current`, `Field_State`, `Field_MemberCount`,
the battle's facing, the objects' fields, the camera words). Those answering
a byte leave stale bits above `al`; the sign and absolute value answer
truly three times in four with stale high words, so the walks arrive.

Result (2026-09-24):

    shadow      inventory_ops self-test: 50000 rounds over 25 functions (2000 each), 421666 calls to the stand-ins, 0 MISMATCHES; the placement's state, the members' spots, the enemy slots, the party objects, the effects, the shade CLUTs, the pointers and counts, the answer and the stand-ins' log compared

Coverage, from the original's side ("shape": `AreaMap_CellNibble` off the
map / even / odd cell; `Encounter_OnScreen` projections made 1..5 (6 means
all five); `Encounter_PathClear` steps tried 0 / 1..3 / 4 and up;
`Encounter_StepOpen` nibbles read 0..3; `Encounter_RollInitiative` the flag
0 / 1 / 2; `Encounter_PlaceEnemies` slots given up 0 / 1 / 2+;
`Encounter_PartyToPlaces` members moved / CLUTs restored;
`Sprite_ShadeLower` bytes it brought to 0x80):

    Encounter_PlaceParty        answers 0 976, 1 1024, other 0; shape 0 0 0 0 0 0
    Encounter_PlaceMember       answers 0 561, 1 1439, other 0; shape 0 0 0 0 0 0
    Encounter_MemberClear       answers 0 1249, 1 751, other 0; shape 0 0 0 0 0 0
    Encounter_MemberStands      answers 0 1324, 1 676, other 0; shape 0 0 0 0 0 0
    Encounter_PlaceEnemies      answers 0 53, 1 1947, other 0; shape 1168 340 492 0 0 0
    Encounter_EnemiesReachable  answers 0 270, 1 1730, other 0; shape 0 0 0 0 0 0
    Encounter_EnemyClear        answers 0 1099, 1 901, other 0; shape 0 0 0 0 0 0
    AreaMap_CellNibble          answers 0 1434, 1 29, other 537; shape 1401 320 279 0 0 0
    Encounter_OnScreen          answers 0 1903, 1 97, other 0; shape 0 1005 484 233 129 149
    Encounter_Project           answers 0 0, 1 0, other 2000; shape 0 0 0 0 0 0
    Encounter_AimCamera         answers 0 2000, 1 0, other 0; shape 0 0 0 0 0 0
    Encounter_Apart             answers 0 609, 1 1391, other 0; shape 0 0 0 0 0 0
    Encounter_CellFits          answers 0 1146, 1 854, other 0; shape 0 0 0 0 0 0
    Encounter_PathClear         answers 0 1556, 1 444, other 0; shape 418 329 1253 0 0 0
    Short_Abs                   answers 0 0, 1 0, other 2000; shape 0 0 0 0 0 0
    Short_Sign                  answers 0 227, 1 754, other 1019; shape 0 0 0 0 0 0
    Encounter_StepOpen          answers 0 217, 1 1783, other 0; shape 87 1287 558 68 0 0
    Sprite_TurnSense            answers 0 0, 1 1595, other 405; shape 0 0 0 0 0 0
    Encounter_RollInitiative    answers 0 2000, 1 0, other 0; shape 1725 235 40 0 0 0
    Encounter_PartyTurnSense    answers 0 2000, 1 0, other 0; shape 0 0 0 0 0 0
    Encounter_PartyTurn         answers 0 1451, 1 549, other 0; shape 0 0 0 0 0 0
    Encounter_PartyToPlaces     answers 0 1051, 1 949, other 0; shape 475 415 0 0 0 0
    Encounter_PartyAtPlaces     answers 0 2000, 1 0, other 0; shape 0 0 0 0 0 0
    Encounter_PartyScriptOnce   answers 0 850, 1 1150, other 0; shape 0 0 0 0 0 0
    Sprite_ShadeLower           answers 0 1834, 1 166, other 0; shape 908 0 0 0 0 0
    calls - count 23838, elevation 20051, object 978, rtp 2000, rotmatrix 4000, apply 2000, setrot 2000, settrans 2000, rand 14695, ground 5395, effect 3516, anim-from 4014, tick 6165, fade 214, raise 864, once 1487, anim 4920, place 2718, clear 13547, stands 2839, enemy-clear 22036, nibble 4607, project 3933, apart 9620, fits 137652, path 13045, abs 7418, sign 7418, step 48410, sense 4845, lower 954

`BOF3X_SHADOW='*'`: exit 0, 314 "0 MISMATCHES" lines, no Fatal, 825 ours.

**Negative controls**, planted one at a time by a script (not committed:
apply, build, run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=inventory_ops`,
restore), each build's output read:

| | Planted | Refused in (rounds of 2,000) |
|---|---|--:|
| I0 | Nibble: x compared as a byte | 44 |
| I1 | Nibble: z against the width | 353 |
| I2 | Nibble: odd cell high nibble | 560 |
| I3 | Nibble: 32-bit index | not refused - a change that changes nothing: width and depth are bytes, so width * z + x (both in range) is at most 65,024 and never wraps 16 bits |
| I4 | Abs: 16-bit answer | 1,015 |
| I5 | Sign: 0 as 1 | 227 |
| I6 | StepOpen: along x 3 refused dropped | 22 |
| I7 | StepOpen: along z needs 2 | 11 |
| I8 | StepOpen: diagonal 4 | 32 |
| I9 | StepOpen: no ordering on equal x | 143 |
| I10 | StepOpen: far cell not read again | 68 |
| I11 | PathClear: budget sign ignored | 984 |
| I12 | PathClear: x step costs 2 | 531 |
| I13 | PathClear: no second pass | 1,709 |
| I14 | PathClear: stop at budget 0 | 264 |
| I15 | CellFits: fractions swapped | 172 |
| I16 | CellFits: b + 4 exclusive | 118 |
| I17 | CellFits: off both allowed | 70 |
| I18 | Apart: strict | 87 |
| I19 | Apart: z not tested | 321 |
| I20 | Project: y rounded down | 687 |
| I21 | Project: x - 0x3FFF | 2,000 |
| I22 | OnScreen: x edge inclusive | 220 |
| I23 | OnScreen: lower bound added | 316 |
| I24 | OnScreen: margin 7 | 11 |
| I25 | OnScreen: turn the other way | 995 |
| I26 | MemberClear: skips k == member from the masked byte | 794 |
| I27 | MemberClear: placed not tested | 1,319 |
| I28 | MemberClear: spot held for the path | 7 |
| I29 | MemberStands: sprite held | 53 |
| I30 | MemberStands: object 0 accepted | 301 |
| I31 | PlaceMember: step signs not negated | 591 |
| I32 | PlaceMember: turn until 0 | 1,120 |
| I33 | PlaceMember: axis by r only | 399 |
| I34 | PlaceMember: member masked for callees | not refused - a change that changes nothing: both callees use only the low byte of the member |
| I35 | PlaceMember: 7 tries | 664 |
| I36 | PlaceParty: sort ascending | 715 |
| I37 | PlaceParty: wide only 2 | 778 |
| I38 | PlaceParty: path from (x, z) | 420 |
| I39 | PlaceParty: centre kept when none fits | 561 |
| I40 | PlaceParty: facing read once | 46 |
| I41 | EnemyClear: inactive tested too | 873 |
| I42 | EnemyClear: kind margin +0x87 | 1,488 |
| I43 | PlaceEnemies: no +5 cells | 2,000 |
| I44 | PlaceEnemies: 48 jitters | 553 |
| I45 | PlaceEnemies: spot not read back | 76 |
| I46 | PlaceEnemies: triangle c * c | 1,697 |
| I47 | Reachable: count asked once | 1,403 |
| I48 | Reachable: enemy words +4 / +8 | 1,996 |
| I49 | AimCamera: low 4 bits cleared | 993 |
| I50 | AimCamera: distance + 0x1190 | 2,000 |
| I51 | AimCamera: rotation not made again | 2,000 |
| I52 | AimCamera: members not averaged | 1,467 |
| I53 | TurnSense: +4 exclusive | 104 |
| I54 | TurnSense: no +8 | 8 |
| I55 | ShadeLower: clamp at -127 | not refused - a change that changes nothing: at a difference of exactly -128 the clamp and the low byte are both 0x80 |
| I56 | ShadeLower: 0x80 bytes lowered too | 517 |
| I57 | ShadeLower: step unsigned | 954 |
| I58 | Roll: redraw above 99 | 85 |
| I59 | Roll: past the party at 51 | 5 |
| I60 | Roll: record byte +0x39 | 1,014 |
| I61 | Roll: no free effect not counted | 152 |
| I62 | Roll: none gives 0 | 40 |
| I63 | TurnSenseAll: sprite held | 146 |
| I64 | PartyTurn: bit 13 | 1,297 |
| I65 | PartyTurn: tick after bit 14 too | 1,077 |
| I66 | PartyTurn: step & 3 | 492 |
| I67 | PartyTurn: count held | 157 |
| I68 | PartyTurn: fade flag bit 3 | 176 |
| I69 | PartyTurn: shade 0xBF | 127 |
| I70 | ToPlaces: first list | 272 |
| I71 | ToPlaces: not found is spot 0 | 131 |
| I72 | ToPlaces: sprite id not Field_State | 15 |
| I73 | ToPlaces: side step z from x | 1,255 |
| I74 | ToPlaces: dirty not set | 339 |
| I75 | ToPlaces: all 2 ignores last count | 73 |
| I76 | AtPlaces: +0x1B | 1,215 |
| I77 | AtPlaces: z via object | not refused - a change that changes nothing: Sprite_Current was set to the object on the line before, with no call between |
| I78 | ScriptOnce: flag bit 2 | 1,068 |
| I79 | ScriptOnce: all 1 test is 2 | 295 |

A first run had I12 as "the x step costs no budget": refused by a **hang**
(the walk never ends when every step is granted), which proves less than a
count and left that self-test's process spinning (section 8). Its counting
twin "the x step costs 2" is I12 above. The thinnest by count: I59 (5
rounds: a slot past the party with r of exactly 51), I28 (7: a recorder
moving the member's spot between the separations and the path), I54 (8: a
direction below the target), I7 and I24 (11), I72 (15).

## 7. For the batch check

All 25 on the `--original` list; each on the trace list with the sizes in
section 1. The combat route reaches all of them (section 1's counts); what
it cannot say is which branches ran - the fuzz is the only check of the
fallbacks (the sixth recentre, a dropped enemy slot, a path tried from the
other end, the bit-14 pose, the intro's skip bits).

## 8. A self-test left running

The hang-refused control of section 6 (2026-09-24 06:21) left its game
process spinning: the harness timed out and ended the launcher, not the
game (pid 37036 by its start time; its parent had exited). It holds this
worktree's `build/bof3x.dll` (renamed aside to
`build/bof3x.dll.held-by-hung-selftest` so the link could proceed) and
`build/bof3x.log`, so the later runs used a copy in `build-ctl/`. Agents do
not end processes; it is the coordinator's to end by pid. A control script
should end the game by the pid the launcher prints, not only the launcher.

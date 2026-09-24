# The battle windows' draw helpers

**Status:** IN PROGRESS (2026-09-23) - nineteen functions ours
(`src/game/battle_window_draw.cpp`, shadow name `battle_window_draw`), each
fuzzed headless against a copy of Capcom's with every call re-aimed at a
recorder: 38,000 rounds, 0 mismatches; 62 negative controls, 60 refused by a
count, two not refused because they change nothing. `BOF3X_SHADOW='*'` passes.
**Not yet through the live check** - the combat A/B
(`analysis/validate_combat.sh`) runs centrally after the merge (section 5).

Group BD of the seventh parallel round
([`takeover-queue-round7.md`](takeover-queue-round7.md)): the openers of
battle windows 1..4, the primitives the battle window handlers of group BC
(`0x442FA0`, `0x443870`, `0x443B10`, `0x443D90`, `0x443F60`) draw with, the
test that keeps one enemy name per kind, and five ability helpers the battle's
skill list (`0x59D200`, group BJ) and group BF's `0x447840` / `0x446FF0` call.
Faithful: no divergence, no `DIVERGENCE.md` entry owed.

## 1. The functions

Every extent measured by recursive descent from the entry (a scratch capstone
walker, 2026-09-23); all nineteen agree with `pe_funcs.py`'s sizes in the
round's catalogue, and every jump in each stays inside it (no jump table). The
PSX twin of each was read (`BATTLE.EMI` section `8a80230e...`, `GAME.EMI`
`9d00fd19...` and the boot EXE `SLPS_009.90`, from the sibling's overlay
captures); the first fourteen are `analysis/pairs_propagated.json`'s pairs,
the last five were found for this doc by shape (the constants `0x3E` / `0x8C`
/ `0x97`, `0x1A` / `0x19` with record bytes `+0x12` / `+0x13`, `and 0x1FF`
with a scan to 10) and read instruction for instruction.

| PC | Name | Bytes | PSX twin | What it does |
|---|---|--:|---|---|
| `0x444230` | `BattleWin_OpenStatus` | 0x51 | `0x801D92D4` | window 1 (handler 3, sub-kind 0): x from `0x64E307` by the party size (`0x904AB0`'s low byte), y 0xF0 - the party's status panel, which `0x442FA0` slides up to 0xC8 |
| `0x444290` | `BattleWin_OpenSub1` | 0x2F | `0x801D9358` | window 2, sub-kind 1, at (0x88, 0x58) |
| `0x4442C0` | `BattleWin_OpenSub2` | 0x1D | `0x801D93B4` | window 3, sub-kind 2, its place left as it was |
| `0x4442E0` | `BattleWin_OpenSub3` | 0x2F | `0x801D93F8` | window 4, sub-kind 3, at (0x7C, 0x2A) |
| `0x444340` | `BattleWin_DrawNumber` | 0x132 | `0x801D94A4` | `%4d` (or `:` for 0xFFFF) in the 6 x 5 digit strip at v 0xD8 |
| `0x4447B0` | `BattleWin_DrawQuadF4` | 0x14F | `0x801D9A84` | a flat POLY_F4 of a shape table (`0x64E148`) in the window colour |
| `0x444900` | `BattleWin_DrawTile` | 0xDB | `0x801D9C50` | a TILE of a size table (`0x64E268`) in the window colour |
| `0x4449E0` | `BattleWin_DrawTileRgb` | 0xA8 | `0x801D9D8C` | the same tile in a given 15-bit colour |
| `0x444A90` | `BattleWin_DrawBar` | 0x1A9 | `0x801D9E6C` | a bar 7 high: body (two looks) and end cap, two POLY_FT4 |
| `0x444C40` | `BattleWin_DrawCell16` | 0xA0 | `0x801DA048` | a 16 x 8 cell of the battle strip at v 0xD8, under abr 1 |
| `0x444CE0` | `BattleWin_DrawLine` | 0x70 | `0x801DA3D8` | an opaque LINE_F2 |
| `0x444D50` | `BattleWin_DrawLineHalf` | 0xAB | `0x801DA484` | a LINE_F2 under abr 0 |
| `0x444E00` | `BattleWin_DrawLineAdd` | 0xAB | `0x801DA578` | a LINE_F2 under abr 1 |
| `0x444EB0` | `BattleWin_FirstOfKind` | 0x84 | `0x801DA66C` | 0 when a window above already shows an enemy of this one's kind |
| `0x57DA70` | `Skill_CanUse` | 0x176 | `0x801B05B0` (GAME) | whether a member may use an ability, field or battle |
| `0x57DC90` | `Menu_DrawSkillRow` | 0x7F | `0x801B0928` (GAME) | one skill-list row: icon, name, `%2d` cost |
| `0x5918A0` | `Skill_FlagIndex` | 0x3A | `0x80166A74` (boot) | the lowest set bit of the ability's `u16_4 & 0x1FF`, from 1 |
| `0x591DB0` | `Skill_ApCost` | 0x9D | `0x80167398` (boot) | an ability's AP cost, halved or three-quartered by the record's `+0x16` / `+0x17` |
| `0x591E50` | `Char_AbilityList` | 0x67 | `0x80167464` (boot) | a member's 10-slot ability list of a type |

Four names are hypotheses (`symbols.toml` says which): which window sub-kinds
1..3 are is group BC's to read, and `BattleWin_FirstOfKind`'s "kind" is the
use of enemy byte `+0xC`, not a read of what writes it.

**Read against the PlayStation.** The fourteen battle helpers are the PSX's
store for store; the PC stores floats where the PSX stores halfwords (the
port's own primitive format, [`menu-screens.md`](menu-screens.md) section 2),
so the PC adds without the PSX's 16-bit wrap. `BattleWin_DrawQuadF4`,
`BattleWin_DrawTile`, `BattleWin_DrawLineHalf` and `BattleWin_DrawLineAdd` are
the sibling's "rect/line draws" under `BattleBanner_DrawFrame` (`0x801D7E80`,
PC `0x443610`) and `BattleMenu_TargetCursor` (`0x801D8060`, PC `0x443740`),
with the colour `0x8002BA08[0x8014494E << 6 | 0x20]` - the PC's CLUT shadow
`0x80B788` at row (colour * 2 + 1). `Skill_CanUse` tests `0x904660..63` and
`0x90465C..5F` byte by byte where the PSX loads one word each; the rest of the
five is the PSX's, with the PC's records 4 bytes higher (the ability lists
`+0x60..+0x7E` against `+0x5C..+0x7A`, the AP at `+0x1A` against `+0x16`) and
its working records at stride 0x14C against 0x140.

## 2. What they do, and the data

The ability table `0x65C4C8` is 0x18 bytes a record: the name in 16 bytes,
then the PSX record's eight (the sibling's `names/abilities.toml`): `b0` at
`+0x10` (bit 0 usable in the field, bit 1 in battle, by `Skill_CanUse`), the
cost `b2` at `+0x12` (`Skill_ApCost`), `u16_4` at `+0x14` (`Skill_FlagIndex`;
`0x400` with a member's `+0x10` bit 4 forbids the ability in battle). The
member records are `CharacterRecords` (`0x903A70`, 0xA4) - reached through the
party list `0x904062` and `0x66972C` by `Skill_CanUse` mode 1 and
`Char_AbilityList`, directly by `Skill_ApCost` - or the working records inside
`ObjTrio` at `0x802DC0`, stride 0x14C, in battle.

Each function's comment in `battle_window_draw.cpp` is the full read. The
points that matter:

- **Order.** Where the original reads memory after a call, ours reads it
  after the same call: `BattleWin_OpenStatus` reads the party size after
  `Window_Alloc`; `BattleWin_DrawNumber` changes the character in the print
  buffer before the CLUT call and reads it back after; `Skill_CanUse` reads
  the AP before `Skill_ApCost` in battle and after it in the field
  (controls 2, 15, 39 and 48 say each is seen).
- **Upper halves.** `Skill_CanUse` hands `Skill_ApCost` the member and id
  dwords whole, and in the field the member dword with its low byte replaced
  by the record (the argument's own stack slot) - ours rebuilds that dword
  exactly. `Menu_DrawSkillRow` pushes the icon with the caller's upper bits;
  `Menu_DrawIcon8` reads its low byte.
- **Floats.** Every coordinate is an int loaded with `fild` and stored as a
  float, a 16-bit value plus at most two bytes: exact, so a plain conversion.
  `BattleWin_DrawBar` copies its cap's corners as bits out of its argument
  slots; the same values.
- **Kept quirks** (each named in the comment on ours): the openers write
  their fields whether or not `Window_Alloc` found the record free;
  `BattleWin_DrawNumber`'s byte index and next-character end test;
  `BattleWin_DrawTileRgb`'s red from bits 10..14 (the PSX's order);
  `BattleWin_DrawCell16`'s unsigned coordinates; `Skill_FlagIndex`'s
  unreachable answer 10.

**Two latent defects of Capcom's**, kept, for the coordinator to number in
[`known-defects.md`](known-defects.md):

1. `BattleWin_DrawCell16` takes x and y **unsigned** (`and 0xFFFF`, then
   `fild`) where every other helper here sign-extends, so a cell at a
   coordinate below 0 lands 65,536 pixels away instead of just off-screen
   left or top. The PSX stores halfwords, where the two readings are the same:
   the port introduced it. Whether any caller (`0x442FA0`, `0x443F60`, group
   BC) passes a negative coordinate is unread; control 29 shows the difference
   is real when one does.
2. `BattleWin_FirstOfKind` looks up every window 5..12 above the enemy's own as
   an enemy (`+0xA - 3`), so a window there holding a party actor (0..2) reads
   a "kind" byte from before the enemy records (`0x93B674` for actor 0, inside
   the previous data) and can hide an enemy's name if that byte happens to
   equal the enemy's. The PSX has the same shape. Harmless unless party actors
   use those windows, which is BC's to say.

Neither is a divergence here; both stay as the original has them.

## 3. The fuzz (`BOF3X_SHADOW=battle_window_draw`, `battle_window_draw_fuzz.cpp`)

Nineteen byte-copies, every call out re-aimed at a recording stand-in (each
checked against the callee the disassembly showed), for ours through
`battle_window_draw::g` alike. 2,000 rounds a function: random bytes in
sixteen game regions - the 22 window records, the packet cursor, four working
records, ten character records, the window colour, the party lists, the flag
words, `0x904AA0..BF`, the print buffer, `0x66972C`, the whole ability table,
the CLUT shadow around the rows seeded, the enemy records -3..7, the shape and
size tables (around the `:` format, which a random byte could turn into a
conversion reading an argument never passed), the skill icons - and in our own
pool and names; then each branch's boundaries seeded; the copy, then ours,
both under x87 control word 0x027F; everything compared, with the answer at
the original's width (al for `BattleWin_FirstOfKind`, `Skill_CanUse`,
`Skill_FlagIndex`, `Skill_ApCost`; eax for `Char_AbilityList`) and the
stand-ins' log.

The stand-ins do what callers read back: `Window_Alloc` claims a free record
as the real one does (so an opener that wrote its state before the call is
seen), the commit moves the packet cursor and stamps the primitive, the
setters write their codes and z floats, the draw mode its words, `sprintf` a
small `%Nd` formatter of its own (the self-test runs before the game's CRT);
the AP-cost stand-in moves the AP of the record it was handed one call in
three; and most disturb a cell some caller reads again after the call (the
party size, the colour, a print-buffer character, the records' AP and flag
bytes, the ability table's bytes, the flag words, `0x904AAA`).

Results (`build/bof3x.log`): 38,000 rounds, 136,576 stand-in calls, 0
mismatches; `BOF3X_SHADOW='*'` passes (314 self-test lines, all 0). Coverage:
`BattleWin_FirstOfKind` 0 in 735 rounds, 1 in 1,265; `Skill_CanUse` no 1,087,
yes 913; `Skill_FlagIndex` 0 in 232, 1 in 836, 9 in 65; `Skill_ApCost` halved
596, three quarters 488, whole 916; openers on a taken record 3,973; the `:`
number 284.

**Negative controls** (planted one at a time through a temporary switch, all
removed; rounds refused of 2,000):

| # | Planted | Refused in |
|---|---|---|
| 1 | status x from the table one entry on | 1,977 |
| 2 | status party size read before `Window_Alloc` | 129 |
| 3 | status +9 not cleared | 1,988 |
| 4 | sub-kind 1 written as 2 | 2,000 |
| 5 | window 3's x written | 2,000 |
| 6 | window 4's y 0x2B | 2,000 |
| 7 | window 2's state written before `Window_Alloc` | 985 |
| 8 | number's 0xFFFF test on the whole dword | 284 |
| 9 | number's character not written back | 1,989 |
| 10 | number steps 6 | 1,729 |
| 11 | number's x not cut to 16 bits | 1,796 |
| 12 | number drawn only if the buffer is not empty | **not refused** |
| 13 | number's u from c * 6 - 0x4F | 2,000 |
| 14 | number's spaces not stepped | 695 |
| 15 | number's character read before the CLUT call | 63 |
| 16 | quad's shade row a 9-bit value | 523 |
| 17 | shade green from bits 6..10 | 981 quad, 1,017 tile |
| 18 | window colour unsigned | 463 quad, 504 tile |
| 19 | quad's last dy signed | 743 |
| 20 | tile w and h swapped | 1,968 |
| 21 | tile always semi-transparent | 1,672 |
| 22 | RGB tile red and blue swapped | 1,929 |
| 23 | RGB tile y unsigned | 294 |
| 24 | bar's look on the whole dword | 445 |
| 25 | bar body's right u 0x98 | 672 |
| 26 | bar cap from x, not x + w | 1,611 |
| 27 | bar 8 high | 2,000 |
| 28 | bar cap's CLUT 0xE0 | 2,000 |
| 29 | cell x signed | 279 |
| 30 | cell under abr 0 | 2,000 |
| 31 | opaque line's y1 from y0 | 1,992 |
| 32 | blended lines not semi-transparent | 2,000 each |
| 33 | added line under abr 0 | 2,000 |
| 34 | first-of-kind counts state-3 windows | 142 |
| 35 | first-of-kind goes on when found in window 12 | 333 |
| 36 | first-of-kind's kind from enemy - 3 | 486 |
| 37 | first-of-kind tests its own window too | 414 |
| 38 | first-of-kind's actor enemy + 2 | 1,096 |
| 39 | can-use battle AP read after the cost | 41 |
| 40 | can-use AP below or equal | 52 |
| 41 | ability 0x14 while below 6 | 11 |
| 42 | ability 0x15 without its fourth byte | 8 |
| 43 | ability 0x8C at a cost of 0, not AP 0 | 13 |
| 44 | ability 0x97 against 0x26 | 17 |
| 45 | field use on b0 bit 1 | 15 |
| 46 | battle ban without the ability's 0x400 | 66 |
| 47 | id 0 test on the whole dword | 53 |
| 48 | can-use field AP read before the cost | 18 |
| 49 | can-use field record by the member, not the lists | 222 |
| 50 | skill row's name at y + 2 | 2,000 |
| 51 | skill row's cost not cut to a byte | 2,000 |
| 52 | skill row's cost at x + 0x6C | 2,000 |
| 53 | flag index over 0x3FF | 125 |
| 54 | flag index from 0 | 1,768 |
| 55 | AP cost halved rounding down | 155 |
| 56 | AP cost three quarters as (3c + 3) >> 2 | **not refused** |
| 57 | AP cost 0x19 on `+0x16` only | 159 |
| 58 | AP cost 0x1A on `+0x16` only | 248 |
| 59 | AP cost's battle test on the whole dword | 146 |
| 60 | ability list type 3 at +0x7F | 161 |
| 61 | battle ability list through the party list | 1,158 |
| 62 | ability list default +0x5C | 1,324 |

Both not refused are **changes that change nothing**: 12 needs an empty
`%4d` or `:` - neither can be, and the formats are kept out of the random
regions; 56 is the same ceiling of 3c / 4 for every c (0..255) as the
original's "plus one when the low two bits are set". Controls 39..49 were
first refused in 0..9 rounds (39, 42, 44 not at all): the battle's special
abilities were rarely reached with the tests before them passed, and the cost
stand-in never moved the AP. Seeding half the rounds onto them and giving the
stand-in that side effect brought all eleven in.

**Not tested by the fuzz:** reads of `.rdata` tables the rounds cannot
randomise (the three formats); `BattleWin_FirstOfKind` with enemy indexes past
7 - their kind bytes run off `.data` (`0x93D6EC`) from index 7 and off the
image (`0x93F000`) from index 12, so they are never seeded; in game the
enemies are few.

## 4. What the other groups' functions do, as seen from here

Said, not bound (the round's rule):

- The battle task `0x42E400` (BB) opens window 1 with state 0 once a battle
  (`0x42E897`), window 2 with 0, and windows 3 / 4 with 1 or 2 by byte
  `0x904AE4` and with 2 (`0x42E963`, `0x42E96D`).
- Handler `0x442FA0` (BC, reached through `0x597066`) moves window 1's y from
  0xF0 to 0xC8 by 4 a frame; `0x443D90` draws a number with
  `BattleWin_DrawNumber` and then asks `BattleWin_FirstOfKind` (`0x443EB2`)
  whether to go on.
- The skill list `0x59D200` (BJ) calls `Skill_CanUse(window +8, 0x929F06,
  id)`, then `Skill_ApCost(0x929F06, id, 1)` and `Skill_FlagIndex(id)`, and
  hands `Menu_DrawSkillRow` the ability record `0x65C4C8 + id * 0x18` itself as
  the name.
- `0x447840` and `0x446FF0` (BF) take a member's ability lists through
  `Char_AbilityList`; `0x447840` also calls `Skill_CanUse`.

**Chinese text, for the localisation work:** only `Menu_DrawSkillRow` draws
text - the ability's name, the 16-byte GBK field at `0x65C4C8 + id * 0x18`,
handed in by `0x59D200` and drawn through `Text_DrawAt`. Nothing here touches
the turn counter's labels at `0x669D10` / `0x669D18` or draws the target banner
`攻 击` or an enemy name (`BattleWin_FirstOfKind` only decides whether the
name is shown). `BattleWin_DrawNumber` draws digits from a texture strip, and
`BattleWin_DrawCell16` 16 x 8 cells of the battle strip at v 0xD8 on page
(0x3C0, 0): if any battle label is a picture rather than text, it would be one
of those cells - not established here.

## 5. What the combat route reaches

The combat route (`tools/recipes/combat.txt`, `analysis/combat_catalog.md`)
reaches all nineteen: from 1 call each of the four openers to 27,052 of
`BattleWin_DrawLineAdd`; `Skill_CanUse` 204, `Menu_DrawSkillRow` 402,
`Skill_ApCost` 408, `Char_AbilityList` 828. All nineteen are in
`analysis/calltrace/entries_logic.txt` with these sizes. Within the functions,
what the route is unlikely to reach and so is fuzz only: `Skill_CanUse`'s
field mode (the field menu's skill list, not the battle), the five special
abilities unless the party knows them, `Skill_ApCost`'s halving and three
quarters unless a member wears the `0x1A` / `0x19` items, `BattleWin_DrawNumber`'s
`:`, and `BattleWin_FirstOfKind` with a party actor above the enemy. The live
A/B after the merge is the check of the whole; nothing here has been seen
drawn by ours yet.

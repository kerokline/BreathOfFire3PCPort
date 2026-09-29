# Group E1B: effect kind 0xF's list and child states, the panels, kinds 0x11, 0x12, 0x14, 0x92

**Status:** MEASURED (2026-09-29) - round thirteen wave one
([`takeover-queue-round13.md`](takeover-queue-round13.md) section 10), from
the round branch's tip `1bb41df`. **48 functions ours**
(`src/game/effect_1b.cpp`, raw addresses in `src/game/effect_1b_callees.h`,
fuzz in `src/game/effect_1b_fuzz.cpp`, shadow name `effect_1b`): every row the
cut `analysis/round13_cut.tsv` gives group E1B, each read to its last
instruction with capstone, none dropped, none added (the band holds no code
the cut does not list). Fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8), used unchanged:
**288,000 rounds, 0 mismatches**; **48 of 48 controls refused by a count**.
Seven state tables named. No divergence; no ledger entry. Live coverage:
**none** - fuzz only (section 6).

## 1. What the band is

`0x4672F0..0x46A5F2`: effect kind 0xF's states 26..40 (the kind's dispatcher
`0x466080`, `Effect_KindHandlers[0xF]`, and its states 0..25 are E1A's; it
jumps through the 41-entry run `0x653C28` by `Sprite_Current +1`), the helpers
those states and E1A's states call, and four whole handlers of
`Effect_KindHandlers` that sit after ours `EffectKind32_Arc` (FC1's; the
catalog's host extent swallowed them - `EffectKind32_Arc` ends at `0x46A287`,
so nothing of ours contained this code).

What reaches each (`band_rows.py --group E1B`, then read by hand):

| Rows | Reached by |
|---|---|
| `0x4672F0`, `0x4673B0`, `0x467400`, `0x4674F0` | kind 0xF's table `0x653C28` entries 26..29 |
| `0x467520`, `0x4677F0`, `0x467A60`, `0x467D60`, `0x468020`, `0x468320` | entries 30..35 of the same table **and** `EffectKind0F_Children` `0x653CA0` (the same six cells), which `EffectKind0F_Child` dispatches by `+2` |
| the sub-kinds' steps | their step tables (`+3`), section 3; `0x467540..0x467760` are also kind 0xF's entries 36..40 |
| `0x468040` | `EffectKind0F_Child4Steps[0]` and a call from `EffectKind0F_Child5Start` |
| `0x468210` | calls from `_Child4Move`, `_Child5Move` |
| `0x468560..0x469AD0` | calls: this group's states and E1A's (`0x4660D0..0x4670C0`), FE1's `FieldPanel_DrawBox3` / `_DrawBox2` (`0x468950`), FE2's `ItemTrade_*` states (`0x469750`) |
| `0x46A3E0`, `0x46A450`, `0x46A500`, `0x46A5E0` | `Effect_KindHandlers[0x92]`, `[0x11]`, `[0x12]`, `[0x14]` |

**Not every row is effect code in the narrow sense**: the helpers
`0x468560..0x469AD0` are panel and window draws called with arguments (kCall),
two of them shared with the field panels (FE1, FE2) - hence the `Panel_`
prefix on those. None is anything but effect or panel code: no row goes back
to the coordinator. The one `hypothesis` row (`0x46A5E0`) is kind 0x14's
dispatcher, as its label guessed.

Who spawns kind 0xF, 0x11, 0x12, 0x92 is not in code: no immediate store of
those kinds to a record's `+5` exists in `.text` (a byte-pattern scan for
`mov byte [reg + 5], imm` and absolute stores into `Effect_Objects`, scratch
`spawn.py`); they come from movement scripts (`Effect_Spawn` /
`Effect_SpawnAt`, ops `0x90..0x9D`). What the game shows with them is not
stated here (the brief's rule): the code draws panels of MessagePools lines,
three toggles, lists of the accessories of NameTable_Accessories categories
0xA and 0xB with counts, two equipped accessories with icons, gauges and
counters; the strings it prints (`0x653EA0..0x653EC4`, `0x66A070..`) are the
image's and not copied here.

## 2. The functions

Names are the reading's (`EffectKindNN_<Verb>`, `Panel_` for the two shared
window helpers). The PSX twins `analysis/pairs_propagated.json` gives
(`0x801D53B4..0x801D978C`) are AREA overlay copies, unnamed in the sibling:
cited in each `symbols.toml` evidence string, no name taken from them.
`src/game/effect_1b.cpp` carries a comment per function with the full read;
`symbols.toml` the same in each evidence string. In brief:

- **Kind 0xF states 26..28** (`_ListOpen`, `_ListShow`, `_ListClose`): a
  window (`Panel_DrawWindow(0x14, 0x12, 0x118, 0x13, 0)`), the toggles at
  `(0x58, 0x28)` (lit by `1 << +6` once the leader's `ObjTrio +3` is 3 or
  more) and the message list at `(0x27, 0x40)`, sliding in by `+9` (state 26,
  down to 0, then a kind-0x1A record in state 0x10 and `+1` up) or out (state
  28, up to 4, then `+1` 0 / 2 / 6 by `+0xB` and `+6`).
- **State 29, `_Child`**: while `Effect_Objects` record 6 has `+8` 0 and `+1`
  0xE, `EffectKind0F_Children[+2]`; else `Effect_Release`. Records 6 and 3 are
  special to kind 0xF: the dispatcher `0x466080` tests record 3 and
  `_DrawGlyph` takes its `y` from record 3's `+0x30`.
- **Sub-kinds 0..5** (`_Child0.._Child5` dispatch `+3`; section 3 for the
  tables): each places the record as a sprite (bank, `+0x2E` / `+0x30` screen
  place, tint bytes) and walks an animation script: sub-kind 0 two
  kind-0x1A children and a script of (animation, frames); 1 a value bounced
  0..0x60 and a count printed; 2 a gauge 0..0x18, a fade and a blink out;
  3 a four-row grid of marks, the lit one stepped by `+9 / 12`; 4 and 5 a
  cursor moved by (dx, dy) scripts inside `0x39..0x79 x 0x41..0x94` with its
  panel (`_DrawCursorPanel`), 5 also counting `+0xB` to 0x3C.
- **The panels**: `_DrawMessageList` (a window of nine or ten MessagePools
  lines by the id table `0x653E00` from `+0x14`, scrolled by `+8`; the title by
  `0x653DF4[+0x3C]`), `_DrawListFrame`, `_DrawCountHeader`, `_DrawToggles` /
  `_DrawToggle`, `_DrawItemsB` / `_DrawItemsA` (the accessories of category
  0xB / 0xA among the 128 inventory ids `0x9042D4`, counts `0x9044D4`),
  `_DrawScrollMark`, `_DrawEquipped` (`0x904130`, `0x90412E` with E1G's icon
  helper), `_DrawTwinFrame`, `_DrawItemFrame`, `_DrawGlyph` (a code-0x6C glyph,
  [`glyph-draw.md`](glyph-draw.md)).
- **The window**: `Panel_DrawWindow` = `Panel_DrawWindowBevel` (two
  semi-transparent quads and a tile in the style's colour, `0x80B7A8 + style *
  0x40`) + `Panel_DrawWindowEdges` (two three-point lines); `Panel_DrawEdgeQuad`
  (a textured quad from five 10-byte records `0x653E6C`).
- **Kinds 0x11 and 0x12**: a screen-wide black semi-transparent quad; a
  0xC8-to-black gradient over the top 100 lines. **Kind 0x92**: a sprite object
  (`Sprite_Objects[+0xB]`) moved by the record's step, released with it.
  **Kind 0x14**: `EffectKind14_States[+1]` (FC1's states).

**Stack reuse**: `_DrawToggles` keeps its three bits in its first argument's
slot, `_DrawEquipped` its `x + 0x2A` in its second's, `_DrawItemsA` a height in
its first's, and the bevel / edges most of their locals in theirs - the
callers' pushed copies, which no caller reads again: ours uses locals.
**Upper halves**: several states build a coordinate over a pointer's or a
callee's upper half (`movzx ax, byte [reg + 9]`); where the upper half is the
record pointer ours reproduces it, where it is a callee's `eax` ours passes the
low bits and the recorders compare what the callee reads (section 5).

## 3. The tables

Named in `symbols.toml` as `[[data]]`, each count the table's own length (the
next table starts right after; read by hand):

| Table | Count | Dispatcher, index |
|---|--:|---|
| `EffectKind0F_Children` `0x653CA0` | 6 | `EffectKind0F_Child` `0x4674F0`, `+2` (also kind 0xF's entries 30..35) |
| `EffectKind0F_Child0Steps` `0x653CB8` | 5 | `EffectKind0F_Child0`, `+3` (also entries 36..40) |
| `EffectKind0F_Child1Steps` `0x653CE4` | 2 | `_Child1`, `+3` |
| `EffectKind0F_Child2Steps` `0x653CF8` | 4 | `_Child2`, `+3` |
| `EffectKind0F_Child3Steps` `0x653D2C` | 2 | `_Child3`, `+3` |
| `EffectKind0F_Child4Steps` `0x653D54` | 2 | `_Child4`, `+3` |
| `EffectKind0F_Child5Steps` `0x653DA4` | 2 | `_Child5`, `+3` |

Kind 0xF's own table `0x653C28` (41 entries, `BareRet` at 0 and 13) is left
for E1A, whose dispatcher reads it. The byte scripts after each step table
(`0x653CCC`, `0x653CEC`, `0x653D08`, the grid `0x653D34`, the move scripts
`0x653D5C` / `0x653DAC`), the id tables `0x653DF4` / `0x653E00`, the quad
records `0x653E6C` and the string pointers `0x66A088`, `0x66A32C` are read in
place, their sizes in `effect_1b_callees.h`; not named.

## 4. The fuzz

`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_1b`, in this worktree: **288,000
rounds over 48 functions (6,000 each), 2,413,734 calls to the stand-ins, 0
mismatches**; 26,828 bytes of state in 49 regions; exit 0. Every entry of the
eight tables reached (the handler lines of the coverage: `0x467520..0x468320`
about 440 each, the steps 1,190..3,060, `EffectKind14_States`' three about
2,000); `Effect_FindFree` 13,272, `Effect_Release` 7,338.

- **Shapes**: states, sub-kind steps and the four kind handlers `kEffect` with
  their kind (0xF, 0x92, 0x11, 0x12, 0x14); `EffectKind0F_Child` `sub_span` 6,
  `EffectKind14_Run` `state_span` 3; the `+3` dispatchers get `+3` below their
  table from the seed. The panels and windows `kCall` (`_DrawGlyph`'s text a
  scratch pointer, `ArgAt(1, kScratch)`).
- **The seed** puts every index byte inside its table **in all 20 records**
  (the animation positions `+0xA`, the grid row `+0xB` and `+9`, the list's
  `+0x3C` and `+0x14`, the equipped panel's message `+0x2C`), since the
  harness's disturbance moves `Sprite_Current` among them after a call and the
  originals read the new record; the accessory ids below 52, the style byte
  below 16 (the colour table's region), record 6's `+1` / `+8` for `_Child`
  only (record 6's `+1` is an index of `EffectKind14_Run`'s table otherwise),
  and each branch's boundaries (the counters at their compares, the cursor at
  its clamps, the gauge at 0x18 and the sign bit).
- **Disturb** (hash only): the leader's `+4`, an id or a count, the style
  byte, a bit of `+7`, the item cursor `+0xC` / `+0x10`, `+6`, the equipped
  ids, and record 6's bytes for `_Child`.
- **Stand-ins**: `Effect_FindFree` re-listed so the three callers that write
  its record unchecked never see 0xFF (section 7), the rest see it a quarter of
  the time; `Text_DrawAt` with the widths it reads; the group's own callees
  (fourteen of them, recorders) with theirs; E1A's and E1G's raw.
- **Regions** beyond effect mode's: the ids and counts (`0x9042D4`,
  `0x9044D4`, 0x80 each), MessagePools to `0x803980`, the style colours
  `0x80B7A8` + 0x400.

**`BOF3X_SHADOW='*'`** (this worktree's build, no ini: narrow): exit 0,
`inject: 6943 ours, 0 left original` (6,895 at the base plus these 48), 980
lines of `0 MISMATCHES`, `effect_1b`'s the same 288,000 rounds (2,414,210 calls,
another stream after the other shadows). **With `BOF3X_WIDE=1`**: `effect_1b`
alone exit 0, 0 mismatches (none of its 48 holds a widescreen operand); `'*'`
exit 3 on `battle_e7` (`GeneWin_ListSlideOut`, `_List2SlideOut`,
`_List3SlideOut`, 1,832 rounds) - the known build-directory / DIV-0041 fault of
[`takeover-queue-round13.md`](takeover-queue-round13.md) section 11, in files
this group does not touch; reported, not fixed.

**Controls** (scratch `controls.py`: all 48 mutants planted on unique anchors,
one build, each function's fuzz run alone with `BOF3X_E1B_ONLY=<name>`, the
source restored and rebuilt): **48 of 48 refused by a count** (exit 3), from
95 rounds (`_ListClose`: `+1` = 7 for 6) to 6,000 (33 of them). One per
function: a dispatcher indexing `+3 + 1`, a placement off by one, a clamp or
compare bound moved by one, a spawned byte changed, a primitive's byte or
float changed, a call's argument changed, a call dropped
(`_Child0Tick`'s script tick), a colour source changed (`_DrawEquipped`'s
`+7 & 3`). No equivalent mutant.

## 5. The masks

Re-listed with the width the callee reads, each read by hand:
`Text_DrawAt` x, y words, colour and count bytes (FE1's reading; the colours
here come from byte registers over garbage); the group's own `Panel_*` /
`_Draw*` x, y words (each passes them on only to word readers), heights,
`which`, bits and flags bytes; E1A's `0x465120` x, y, value words
(`movsx word`), `0x465D90` x, y words and a flag byte; E1G's `0x594D50` item
and category bytes (`test al, al`, `Item_IconKind`'s `& 0xFF`). The E1F rows
(`0x52CFE0`, `0x52CF60`) are EKH's effect-mode re-listings, unchanged.

## 6. Live coverage

None. The catalog's reach columns (`analysis/remaining_catalog.tsv`: attract,
shop, worldmap, combat) are empty for all 48 rows, and no call-count file
under `analysis/calltrace` (157 of them, the recorded routes and A/B runs)
names any of the 48 entries. Fuzz only, until a recorded walk shows these
kinds; the coordinator's frame-hash A/B covers it then.

## 7. Latent defects (described, not fixed) and where ours aborts

- **`Effect_FindFree` unchecked**: `_ListOpen` (the call at `0x46736A`), `_Child2Gauge`
  (`0x467B8B`) and `_DrawMessageList` (`0x468626`) write the answer's record
  without testing for 0xFF (none free): with all 20 records busy the original
  writes a kind-0x1A record at `0x7E91E0`, past the pool. Ours aborts with a
  message. Ordinary play needs 20 live effects at that moment; not seen.
- **Indexes past a table - ours aborts, the original reads or jumps through
  what follows**: every `+2` / `+3` / `+1` dispatcher past its table's length;
  the animation scripts past their end; `Panel_DrawEdgeQuad`'s `which` past 5;
  `_DrawMessageList`'s title id past 6 and line index past 0x36; an accessory
  id past NameTable_Accessories' 52 (`_DrawItemsA` / `_DrawItemsB`);
  `EffectKind92_Follow`'s sprite index past 30 (the original writes past
  `Sprite_Objects`). None is reached from the states' own starts.
- **Reads before a script**: the steps read "the record before `+0xA`"; with
  `+0xA` 0 and `+9` not 0 the original reads up to four bytes in front of the
  script - the high bytes of the step table's last pointer, image constants.
  A record's own start never gets there (`+9` 0 advances `+0xA` first), but a
  record left mid-step does, so ours reads the same bytes rather than abort.
- **A no-op store**: `_DrawCountHeader` compares the printed number's first
  byte with 0x20 and writes 0x20 over it - no change (perhaps meant to blank
  a leading character).
- **Strides**: `_Child3Grid` takes the lit mark's string at `0x66A32C + mark *
  8` and the others' at `mark * 4`; with the image's marks (0, 1) the lit one
  reads entry 0 or 2 of three.

## 8. Calls across groups and the rebinding

**Raw calls into other groups of this round** (in `effect_1b_callees.h`): E1F
`0x52CF60`, `0x52CFE0`; E1A `0x465120`, `0x464DA0`, `0x464E40`, `0x464EC0`,
`0x4652D0`, `0x465D90`, `0x465E50`; E1G `0x594D50`.

**Inbound from outside the group**: E1A's kind-0xF states call
`Panel_DrawWindow`, `_DrawToggles`, `_DrawItemsB`, `_DrawItemsA`,
`_DrawEquipped`, `_DrawCountHeader`, `_DrawGlyph` (by `E8`; raw in their
clones until the rebinding); ours `FieldPanel_DrawBox3` / `_DrawBox2` (FE1)
call `Panel_DrawEdgeQuad`, ours FE2 `ItemTrade_*` states `Panel_DrawWindow`;
`Effect_KindHandlers[0x11]`, `[0x12]`, `[0x14]`, `[0x92]` and kind 0xF's table
`0x653C28` (E1A's dispatcher) hold the rest, read in place.

**Rebound** (the round-ten form, the value unchanged): `field_e1_callees.h`
`kDrawQuad` -> `bof3::addr::Panel_DrawEdgeQuad`; `field_e2_callees.h`
`kTradeBox` -> `bof3::addr::Panel_DrawWindow`. **Left raw on purpose**: the
fuzz files' `CallSite` tables (`field_e1_fuzz.cpp`, `field_e2_fuzz.cpp`: the
keys) and comments. **For the coordinator** (harness files, not this group's
to edit): `scenario_harness.cpp` `kField`'s rows `"0x468950"` and `"0x469750"`
and `kEffectOverrides`' `FX_RAW(0x469750)`, `FX_RAW(0x468AC0)` - still valid
by address; ours calling by name keys on the name, so any group calling these
by name lists them itself (as E1B does).

**DIVERGENCE.md, cheats.cpp, widescreen.cpp**: none names an address in the
band; no byte patched inside the 48.

`tools/ledger_check.py`: 0 errors. `analysis/calltrace/entries_logic.txt`
(main checkout): 33 extents appended (15 were already exact), among them
the smaller ones that cut E1A's host `0x466260` (`1DD2`) and the lines
`0x468040 1C5`, `0x468210 348`, `0x469AD0 E0` down to the functions read.

# The fishing minigame's text - where it lives, and DIV-0069

**Status:** MEASURED (2026-10-03, fix wave group FL, headless only) - the
strings and draws read from the disc, the exe and our source; the fix built
and fuzzed; **not yet seen in game** (section 6 is the coordinator's live
check). The fishing code itself (`0x52AF80..0x52CD47`) is Capcom's and
round fourteen's; everything changed here is in draws that were already ours.
Section 8 (2026-10-17, the button round's IC-F, built and headless only)
draws the banner's labels through the Config's `σ`: ledger draft D, amending
DIV-0069.

## 1. What the owner saw (HANDOFF item `0000`)

Under the English overlay, at the fishing spot (`tools/recipes/campingFishing.txt`,
`analysis/shots/camping/`, the owner's `analysis/shots/owner_catalogue/`):

- the banner across the top window still Chinese at every step
  (`fishing_banner.png`; frames 3120, 4080, 4320, 4560, 4800), with `aa`
  and coloured `b`, `c`, `d` after the lines;
- the equip menu (`fishing_equip_menu.webp`, frame 3360): the rod and lure
  names cut at eight letters (`Wooden R`, `Heavy Ca`, `Old Popp`), the
  three tabs Chinese, and a thin frame line right of the EQUIP and GUIDE
  boxes with a short mark under EQUIP;
- the data page's `?????????` / `NO DATA` box (frame 3600).

## 2. Where each piece lives (read 2026-10-03)

The machine is the leader's state 9 (`LeaderPanel_*`, E1E's,
[`effect_1e.md`](effect_1e.md) - which named it by shape: it is the fishing
spot) driving effect records 0..6; the windows are effect kind 0xF
([`effect_1a.md`](effect_1a.md) states 0..12, [`effect_1b.md`](effect_1b.md)
26..40). All of it ours since round thirteen's wave one. None of the
Chinese is GBK: it is the port's own two-byte glyph codes, so a byte search
for the characters finds nothing; the strings were found through the draws'
operands and read by rendering the shipped glyph table (scratch).

| What | Where | Read by | Before this entry |
|---|---|---|---|
| The banner's thirteen lines | strings `0x669FC0..0x66A06D`, through 8-byte records at `0x653B98` (pointer, label byte `+4`, pause byte `+5`); which lines a window cycles: the row table `0x653C04` (nine rows of three line indexes and a spawn byte) | `EffectKind0F_Choose`, `_Title`, `_TitleClose`, `_LineStart`, `_LineType`, `_LineNext`, `_LineScroll` (pe_xref: `0x4661D0`, `0x466210`, `0x4662D3`..`0x46643B` - all ours) | Chinese under every overlay |
| A line's button label | `0x66A2FC[label]` - one-byte strings `b`, `c`, `d` at `0x66A2DC..` = glyphs `0x3C..0x3E`, the port's circled numerals 1, 2, 3; colours `0x653C00[label]` = 2, 1, 6 | the same states; and the Config screen's `0x461C00`, which reads entries 0..5 as circle, cross, triangle, square, L1, R1 with the same colours 2, 1, 6 (DIV-0051) | the overlay paints those single-byte slots with letters: `b`, `c`, `d` |
| The two dots after a line | byte `0x61` (glyph `0x3B`, the port's two-dot ellipsis) in the strings | - | painted `a`: the `aa` |
| The three tabs | 8-byte slots `0x66A070`, `0x66A078`, `0x66A080` behind the table `0x66A088` | `EffectKind0F_DrawToggles` `0x468AC0` only (x `+0xA / 0x35 / 0x6A`, counts 2, 3, 2) | Chinese |
| The panel titles `ROD`, `LURE`, `EQUIP`, `GUIDE` | `0x653EA0..0x653EB2`, plain ASCII in the exe | `_DrawItemsB`, `_DrawItemsA`, `_DrawEquipped` | already English (Capcom left them) |
| The help line over the menu ("Equip rod") | `MessagePools` + a word (`0x803620`/`40`/`5E`, `0x803622`..`28`) | `LeaderPanel_S9Menu`, `ExtraSlots_*` | already English: the area's script pool, converted with the area |
| Accessory names | `NameTable_Accessories` (DIV-0008 writes the US names) | `Text_DrawAt(.., 8, name)`: `_DrawItemsB` `0x468C50`, `_DrawItemsA` `0x468F00`, `_DrawEquipped` `0x469210` (fourteen calls), `EffectKind03_ShowName` `0x465230` (the cast's panel) | count 8 - **the cut**: `Text_DrawString` counts characters, and the PC's names were four two-byte glyphs |

Not a tenth verb set: DIV-0018's 22 verbs are `0x66A228` behind `0x6637E4`;
the tabs are their own table, read by one draw.

**The data page's box** is not Chinese: `NO DATA` is artwork, and the
`?????????` run draws as Latin `?` under the overlay. The US module holds
its own unknown-fish placeholder, twelve `?` right after Manillo's name
(`0x801E28D0`). Nothing changed; which code draws the run was not traced
(the data page is `ChoiceMenu_Choices[1]` `0x52ADA0`'s and record 6's,
Capcom's help-line draw and ours' child draws).

## 3. The disc's side

The fishing module every fishing area carries (US `BIN/WORLD00/AREA030.EMI`,
the section loaded at `0x801D0C00`; `AREA089`, `AREA129` the same bytes)
has the same machine, built for 8-unit letters:

- **The lines**: 12-byte records (a count, a pointer, the PC's label and
  pause bytes) at `0x801E1E78..`, ending twelve bytes before the row table,
  which equals the PC's `0x653C04` byte for byte (US `0x801E1F20`; French
  and German at their own offsets, same shape). The count sometimes
  includes the NUL and once runs into the next string with none.
- **The labels**: three 3-byte entries at `0x801E1F14` - codes `0x81`,
  `0x82`, `0x83` (the US font's cross, triangle, square) in colours 1, 6, 5:
  the US release's buttons, not the port's.
- **The cadence**: `LineStart`'s twin sets `+9` = 4 (`0x801D3A28`), the
  typing step sets 4 again (`0x801D3AEC`), the flip cursor is
  `8 - 2 * left` wide at `2 * left + 0x11D` (`0x801D3BF8..0x801D3C08`), the
  label flip `8 - 2 * wait` at `0x125 - 2 * (4 - wait)` (`0x801D3C9C..`):
  the PC's 6 / 12 / `0x119` for its 12-unit glyphs.
- **The tabs**: fixed-width strings right after the edge-quad records the
  PC has at `0x653E6C`, drawn by `lui / addiu` of their address with
  `addiu $a3, $zero, n` before it - US `Gear` `Data` `Rule` at `x + 6 + 0x30 i`,
  count 4 (`0x801D7CAC..`); German count 4; French count 7, `x + 6` and
  `x + 0x3E` (its boxes are wider).

## 4. What ours does now (DIV-0069)

[`DIVERGENCE.md`](DIVERGENCE.md) DIV-0069 is the record. In short:

- `loc_build.py` (`convert_fishing`, chunk kind 16, written into
  `FIRST.DAT`'s overlay): tag 1 the thirteen lines, tag 2 the three tabs,
  found as section 3 says - by the PC's row table and edge-quad records,
  the tabs' width from the module's own code. US, French and German all
  convert (13 lines, 3 tabs each).
- `src/game/fishing_text.cpp`: `FishingText_Apply` copies them into buffers
  of ours (64 bytes a line, 16 a tab) and re-aims the record pointers and
  the tab table, each entry checked first.
- Under a Latin overlay, armed in `InjectAll` after every self-test
  (`FishingText_Arm`): kind 0xF types each character after half its own
  advance (4 frames for an 8-unit letter, 6 for a 12-unit glyph); the flip
  cursor is the character's advance wide with its right edge at `0x125`;
  the leaving character moves the line on by its advance (kept even); the
  label follows the real pen width and is DIV-0051's icon (circle, cross,
  triangle) for the port's numeral; the tabs are centred in their boxes by
  their width, to the NUL; names are drawn to 12 characters. Each of these
  gives the original's number for 12-unit glyphs.
- One known approximation: the leaving character's flip (`0x469AD0` with
  bit 7) shows the right `2 * left` units of the 12-unit cell, which for an
  8-unit letter is mostly its blank right margin - the letter leaves a
  frame or two sooner than the US one does. Left as is.

## 5. The stray frame lines (Capcom's; not fixed)

[`known-defects.md`](known-defects.md) `D198`: the lines are the right
column of the side quads `Panel_DrawEdgeQuad` builds (their heights 0x1E,
0x40, 0x68 are the lines' measured heights), half a game pixel wide at
scale 4: the port's `D3d_TexCoords` `(i + 0.512) / 256` with the
PlayStation's `u + w` far edge samples the texel past the quad above scale
1. Neither the language nor the wide picture: the cure is renderer-wide and
the owner's call.

## 6. For the coordinator's live check

Prerequisites, at the merged tip: the build; **the English overlay rebuilt**
so it carries chunk kind 16 - `python tools/loc_build.py all --disc
"CDImage/Breath of Fire III (USA).cue" --game bof3` must print
`fishing: 13 lines, 3 tabs`.

**The route** is the owner's `tools/recipes/campingFishing.txt` (`# save
camping`, recorded `BOF3X_LANG=en BOF3X_FILTER=point`). A shots copy:

    python tools/recipe_shots.py tools/recipes/campingFishing.txt --every 30 --out analysis/campingFishing_fl.txt
    python tools/input_run.py analysis/campingFishing_fl.txt --out analysis/shots/fishing_fl_en --lang en --env BOF3X_FILTER=point --minutes 6 --no-front

(`--every 30` keeps the nine named shots; 175 more, every half second -
the banner's lines take about a second each.) Frames to look at, with a
right and a wrong picture:

| Frame | Right | Wrong |
|---|---|---|
| `fishing_banner` (3120) and every 30 to 3330 | the top window types the disc's "set rod and lure" line, each letter landing where the flip ends at the window's right edge, then a **green triangle** icon right after the line's three dots; the row's other lines "casting" with a **red circle** and "quit fishing" with a **blue cross** | Chinese; `aa`; a `b` / `c` / `d`; letters appearing left of the flip; the icon a word's width past the dots |
| `fishing_equip` (3360) | tabs `Gear` `Data` `Rule`, each centred in its box; the lure list and the EQUIP panel with whole names (`Wooden Rod`; `Heavy Ca` and `Old Popp` whole), clear of the `x 5` counts | names cut at 8; Chinese tabs |
| `fishing_data` (3600) | `Data` lit; the `?????????` / `NO DATA` box as before | - (unchanged by design) |
| `fishing_action` (4080) | top: "wind reel" and a red circle; the bottom panel's name whole | `Wooden R` in the bottom panel |
| `fishing_vs` (4560), 4800 | the lost-catch line in English, no label (its label byte is `0xFF`) | Chinese |

**Replay caveat:** the banner's lines now take fewer frames under English.
Nothing in the fishing logic was found to wait on them (they are spawned
kind-0xF records, not record 3, and draw no `Rand`), but they hold effect
slots for different spans. If the route's later frames drift, compare the
run's `randlog` lines with a run at the pre-merge tip before believing
anything past the cast; the shots at 3120..3600 (the menu) come before the
cast.

**The stray line** (`D198`): frame 3360 with `--env BOF3X_SCALE=1` (the
line should be gone) and with `wide=0` in the launcher's ini (it should
stay). Zoom right of the EQUIP box, game x 156..162.

**Also owed the owner's eye:** `caughFish.txt`'s catch (the catch lines
and the cast panel's name), the French build's tabs (`Données` is seven
letters over a 40-unit box) and the German build.

## 7. Questions for the owner

- The button icons: the US disc labels the three lines with cross,
  triangle, square (its own button layout); ours draws the port's buttons,
  circle, cross, triangle, which is what the PC's numerals and the Config
  screen say. Which do you want?
- The tabs read `Gear` / `Data` / `Rule` on the US disc (you read the
  Chinese as Equip / Data / Guide). The disc's words are used.

## 8. The labels through σ (button round group IC-F, ledger draft D)

**Status:** BUILT (2026-10-17, branch `buttons/icf`), headless only. The
round is [`button-unification-round.md`](button-unification-round.md) 3,
row IC-F. `σ` is SG's ([`button-sigma.md`](button-sigma.md)).

### 8.1 Which test each label names

Read from the image on 2026-10-17 (a scratch PE reader over `0x653B98`,
`0x653C00`, `0x653C04`). Seven of the thirteen texts carry a label. Which
stage shows which row is in our source: the rows are record 3's mode `+6`
(`LeaderPanel_Effect3Mode` `0x52B2A0`, `src/game/rest_1g.cpp`), and the
callers name the stage.

| Row (mode) | Set by | Texts (line byte) | The test the label names |
|---|---|---|---|
| 0 | stage 1, both items set (`LeaderPanel_Stage1Steps[0]` `0x5289C0`, mode 0) | 0 (0), 2 (1), 1 (2) | stage 1's `0x20` / `0x40` / `0x10` (`LeaderPanel_S1Buttons` `0x528A90`); text 0's `0x20` leads to stage 2, which casts on **word 7** (`LeaderPanel_S2Wait` `0x528FA0`) |
| 6 | stage 1, an item missing (`0x5289C0`, mode 6) | 12 (2), 11 (0), 2 (1) | stage 1's `0x10` / `0x20` / `0x40`; here `0x20` goes to the gear menu (stage 9), not the cast |
| 1 | stage 3 (`LeaderPanel_S3Run` `0x5291D0`, mode 1) | 3 (0), 4 (none), 5 (1) | the reel's `0x20` and the leave's `0x40`, hard-coded |
| 2 | stage 4 (`LeaderPanel_S4Music` `0x5295A0`, mode 2) | 3 (0), 4 (none) | the reel's `0x20`, hard-coded |
| 3, 4, 5 | stages 5, 6, 7 | 8, 9, 10 (none) | no label |

So **text 0 is the cast's line**: the only label whose press is also a
Config word's. Every other label names a hard-coded constant, which group
HT turns into `Sigma(constant)` after this merges.

### 8.2 What is drawn

`FishingText_LabelOf(text, line)` (`src/game/fishing_text.cpp`) answers an
entry of `0x66A2FC` and a pen. The kind-0xF line states (`LineNext`
`0x466460`, `LineScroll` `0x4665E0`, `LineFade` `0x466750`, in
`src/game/effect_1a.cpp`) draw through one helper, `Label`, at their five
label draws.

- **The button.** The line byte names Japan's button: 0 circle `0x20`,
  1 cross `0x40`, 2 triangle `0x10`. The label shows `Sigma` of it. Text 0
  shows the first of the six buttons in word 7 (`0x90358E`), in the Config
  cell's order. Under draft A word 7 is Speak's word, so this equals
  `Sigma(circle)`. The self-test checks that over all 720 assignments.
- **The entry.** `0x66A2FC`'s order, as the Config cell `0x461C00` reads it
  under its tests: circle, cross, triangle, square, L1, R1 (its loads of
  `0x66A2FC..0x66A310`, read 2026-10-17 with capstone).
- **The pen, by the shape.** Circle 2, cross 1, triangle 6 (`0x653C00`),
  square 5, L1 and R1 0: the pens `0x461C00` pushes for the same six.
- **Under a Latin layer**, the disc's shape: DIV-0051's icon for the entry
  (`FishingText_Label`, now six entries).
- **On the PC's own text**, the port's stand-in: the `0x66A2FC` string for
  the entry, a circled numeral or the port's `X`, in that pen. No new art,
  as the owner chose for DT-2 (section 5.1 of the round doc).

With `σ` the identity, every label is the original's: entry = the line
byte, pen = `0x653C00[line]`.

**Off** (`BOF3X_ORIGINAL=FishingLabelSigma`, and before `FishingText_Arm`):
entry = the line byte and pen = `0x653C00[line]`, which are the original's
reads. So effect_1a's fuzz compares Capcom's draws.

**A word 7 with none of the six buttons** is refused with a Fatal that names
the remedy (hold Start while the save loads, or the switch). Under draft A it
cannot happen, because SG refuses such words at the writers. It can happen
only with `ButtonSigma` off and a hand-edited save.

### 8.3 Verification

- **Build:** no errors, no new warnings.
- **Headless, narrow** (`BOF3X_SELFTEST_ONLY=1
  BOF3X_SHADOW=fishing_text,sigma,effect_1a`, an ini-less copy of the dev
  launcher under `C:\Users\kerok\b_icf`, the PC install): exit 0.
  - `fishing_text self-test: the label through sigma over 7 labelled lines,
    each release's defaults, the stand-in swap and 720 assignments (10920
    checks); 0 MISMATCHES`.
  - `effect_1a self-test: 210000 rounds over 70 functions ... 0
    MISMATCHES`, with `0x466460`, `0x4665E0` and `0x466750` covered.
  - `patch ON FishingLabelSigma`.
- **What the self-test checks:**
  - the image's `0x653C00` pens equal the table's;
  - text 0's line byte is circle;
  - at Japan's defaults every label is the original's (entry, pen and
    icon);
  - at the West's defaults: circle, cross and triangle show cross,
    triangle and square, and the cast shows cross;
  - at PSP-EU's: cross, circle, square, and the cast cross;
  - **the stand-in swap** (Speak <-> Action, View <-> Change, words 7 / 8
    by draft A): circle lines show cross in pen 1, cross lines show circle
    in pen 2, triangle stays, and the cast shows cross;
  - all 720 assignments: each label is `Sigma(its button)` in its shape's
    pen, and the cast's equals `Sigma(circle)`.
- **Headless, everything** (`BOF3X_SHADOW='*'`): see the round's hand-back.
- **Not seen in game.** RC's fishing route (`caughFish`, area 30) in its
  identity and swapped runs is the evidence. The identity run's banner must
  equal the reference. The swapped run's must show, for example, the cast's
  line with a cross icon (blue under a Latin layer, the circled 2 on the PC's
  text) and the quit line with a circle.

### 8.4 Between this merge and HT's

Until HT lands, stage 1 and the reel still test Japan's constants. After a
swap, the labels then name the swapped buttons while the old ones still
work. The cast is split: stage 1 still wants circle, and stage 2's confirm
already wants the swapped button, because word 7 moves with draft A. The same
holds on a Western disc-only install at its own defaults (SG's section 7).
The labels state what the round intends. HT makes the tests agree.

### 8.5 Ledger draft D (amends DIV-0069)

- **Date:** 2026-10-17
- **Subsystem:** field text, the fishing spot's banner labels
  (`src/game/fishing_text.cpp` `FishingText_LabelOf`; the label draws of
  effect kind 0xF's states 10..12 in `src/game/effect_1a.cpp`). **Amends
  DIV-0069**: its label "for the button the port's numeral names" becomes
  the label for that button through `σ`.
- **Tier:** Intent. The owner, 2026-10-17: unify every button flow (the
  round's answer 1). DIV-0069's owner's word waited on exactly this ("until
  the banner can draw the icons of the bindings in force", IDEAS I29).
- **Original behaviour:** each labelled line of the banner (seven of the
  thirteen records at `0x653B98`) draws a fixed label by its line byte. That
  is `0x66A2FC[line]`, the port's circled 1, 2 or 3, in pen
  `0x653C00[line]` (2, 1, 6). It names circle, cross or triangle whatever
  the Config says. DIV-0069 draws DIV-0051's icon for the same fixed button
  under a Latin layer.
- **New behaviour:** the label names the button the line's test is on now.
  - For the cast's line (text 0), that is word 7's button. For every other
    labelled line, it is `Sigma` of the line byte's button.
  - It is drawn as that button's own label, in that button's own pen
    (circle 2, cross 1, triangle 6, square 5, L1 / R1 0, as the Config cell
    `0x461C00`).
  - Under a Latin layer it is DIV-0051's shape. On the PC's own text it is
    the port's stand-in, `0x66A2FC[entry]`.
  - At the identity Config, nothing changes.
- **Rationale:** the round's answer 1. An icon that follows the Config is
  true in the fishing too, once HT moves the tests.
- **Also in the PSX version?** The PlayStation's labels are fixed too: the
  US module's are its own release's buttons (section 3). Following the
  Config is ours.
- **Verification:** section 8.3. RC's `caughFish` area-30 route, identity
  and swapped, is owed.
- **Reversible?** `BOF3X_ORIGINAL=FishingLabelSigma`.

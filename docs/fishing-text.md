# The fishing minigame's text - where it lives, and DIV-0069

**Status:** MEASURED (2026-10-03, fix wave group FL, headless only) - the
strings and draws read from the disc, the exe and our source; the fix built
and fuzzed; **not yet seen in game** (section 6 is the coordinator's live
check). The fishing code itself (`0x52AF80..0x52CD47`) is Capcom's and
round fourteen's; everything changed here is in draws that were already ours.
Section 8 (2026-10-17, the button round's IC-F, built and headless only)
draws the banner's labels through the Config's `σ`: DIV-0136, amending
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

## 8. The labels through σ (button round group IC-F, DIV-0136)

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
  cell's order. Under DIV-0133 word 7 is Speak's word, so this equals
  `Sigma(circle)`. The self-test checks that over all 720 assignments.
- **The entry.** `0x66A2FC`'s order, as the Config cell `0x461C00` reads it
  under its tests: circle, cross, triangle, square, L1, R1 (its loads of
  `0x66A2FC..0x66A310`, read 2026-10-17 with capstone).
- **The pen.** A button that `σ` leaves where it was keeps the original's
  draw: the line byte's label in the pen the image holds at `0x653C00`
  (section 8.6). Only a button that moved takes its shape's pen: circle 2,
  cross 1, triangle 6, square 5, L1 and R1 0, the pens `0x461C00` pushes for
  the same six. This is the owner's rule of 2026-10-17, the one DT-2 uses.
  Since D313 (section 8.8) "where it was" is where the image's release had
  it: on a Western or PSP-EU image the line byte's button is that release's
  (`M_R` of Japan's), and the label is `SigmaFrom(R, it)`.
- **Under a Latin layer**, the disc's shape: DIV-0051's icon for the entry
  (`FishingText_Label`, now six entries).
- **On the PC's own text**, the port's stand-in: the `0x66A2FC` string for
  the entry, a circled numeral or the port's `X`, in that pen. No new art,
  as the owner chose for DT-2 (section 5.1 of the round doc).

With `σ` the identity, every label is the original's: entry = the line
byte, pen = `0x653C00[line]`, on every install kind.

**Off** (`BOF3X_ORIGINAL=FishingLabelSigma`, and before `FishingText_Arm`):
entry = the line byte and pen = `0x653C00[line]`, which are the original's
reads. So effect_1a's fuzz compares Capcom's draws.

**A word 7 with none of the six buttons** is refused with a Fatal that names
the remedy (hold Start while the save loads, or the switch). Under DIV-0133 it
cannot happen, because SG refuses such words at the writers. It can happen
only with `ButtonSigma` off and a hand-edited save.

### 8.3 Verification

- **Build:** no errors, no new warnings.
- **Headless, narrow** (`BOF3X_SELFTEST_ONLY=1
  BOF3X_SHADOW=fishing_text,sigma,effect_1a`, an ini-less copy of the dev
  launcher under `C:\Users\kerok\b_icf`, the PC install): exit 0.
  - `fishing_text self-test: the label through sigma over 7 labelled lines
    on two images (this one, pens 2 1 6; a disc-built image's gap zeros),
    Japan's words as the original's draw, each release's defaults, the
    stand-in swap and 1440 assignments (30523 checks); 0 MISMATCHES`
    (`buttons/icf2`; the first build's line read 720 assignments and 10920
    checks).
  - `effect_1a self-test: 210000 rounds over 70 functions ... 0
    MISMATCHES`, with `0x466460`, `0x4665E0` and `0x466750` covered.
  - `patch ON FishingLabelSigma`.
- **What the self-test checks**, on two images: the running one, and a
  disc-built one as `exe_tables.py` writes it (every line byte 0, every pen
  0; section 8.6):
  - text 0's line byte is circle;
  - at Japan's words (the PC's start, and the camping save every route
    loads, on every install kind) every label is the original's: entry, the
    image's pen, and icon;
  - a button that stays keeps the image's pen; a moved one takes its
    shape's pen;
  - at the West's defaults: circle, cross and triangle show cross,
    triangle and square, and the cast shows cross;
  - at PSP-EU's: cross, circle, square, and the cast cross;
  - **the stand-in swap** (Speak <-> Action, View <-> Change, words 7 / 8
    by DIV-0133): circle lines show cross in pen 1, cross lines show circle
    in pen 2, triangle stays, and the cast shows cross;
  - all 720 assignments: each label is `Sigma(its button)` in its shape's
    pen, and the cast's equals `Sigma(circle)`.
- **Headless, everything** (`BOF3X_SHADOW='*'`): exit 0; see 8.7.
- **RC's fishing route on `psx-us`, identity:** see 8.7.

### 8.4 Between this merge and HT's

This held until HT merged into `buttons/round`. Before that, stage 1 and the
reel still tested Japan's constants, so after a swap the labels named the
swapped buttons while the old ones still worked. HT closed it.

### 8.5 DIV-0136 (amends DIV-0069)

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
  - A button that `σ` leaves where it was draws exactly as the original
    reads it: the line byte's label in `0x653C00`'s pen. A moved button is
    drawn as its own label in its own pen (circle 2, cross 1, triangle 6,
    square 5, L1 / R1 0, as the Config cell `0x461C00`). This is the owner's
    rule of 2026-10-17: recolour only when the button moves.
  - Under a Latin layer it is DIV-0051's shape. On the PC's own text it is
    the port's stand-in, `0x66A2FC[entry]`.
  - At the identity Config, nothing changes.
  - **Amended 2026-10-17 (D313, section 8.8):** the label is read in the
    terms of the release the running `.data` was built from (the build of
    the disc-built image the exe host mapped, `ExeImage_MappedBuild`;
    `BOF3.exe`'s own, hosted or a `pc-zh` image, is Japan's). The line byte names
    the line's label entry on every release; the entry is that release's
    button, `M_R` of Japan's (`P` on the PlayStation West: cross, triangle,
    square; `S` on PSP-EU: cross, circle, square). The label is
    `SigmaFrom(R, that button)`, which is `Sigma` of Japan's, or word 7's
    for the cast. "Unmoved" means the release's own button: then the label
    draws as that release's disc draws it, its button in the image's pen,
    which since D313 is the disc module's own (West 1, 6, 5; PSP-EU 1, 2,
    5). So a Western install at its own defaults draws the US banner's
    labels, and at Japan's words (the camping save) draws Japan's buttons
    in their own pens. Without the amendment it drew an unmoved circle in
    the West's pen for cross.
- **Rationale:** the round's answer 1. An icon that follows the Config is
  true in the fishing too, once HT moves the tests.
- **Also in the PSX version?** The PlayStation's labels are fixed too: the
  US module's are its own release's buttons (section 3). Following the
  Config is ours.
- **Verification:** sections 8.3 and 8.7.
- **Reversible?** `BOF3X_ORIGINAL=FishingLabelSigma`.

### 8.6 Where a disc install's white label comes from (2026-10-17, `buttons/icf2`)

RC's validation (`button-routes.md` 9.1) found that on `psx-us` and `psp-eu`
the reel line's label drew white before the round and red after it, with the
Config untouched. The white is not the disc module's colour. It is a gap in
the disc-built image:

- **The pen.** `0x653C00..0x653C02` sit in the range `0x653BFC..0x653C03`,
  which `recipes/exe-gaps.tsv` classes `gap` / `game-value` for every disc
  build. In `RC`'s `c_psxus` and `c_pspeu` `base/exe/data.toml` it is
  `[0x653BFC, 0x653C03, "gap", ...]`, and `data.bin` holds zero there. So
  the original's read `0x653C00[line]` gives pen 0, which is white. The map
  resumes at `0x653C03` (the row table, from `AREA030.EMI`).
- **The line bytes.** The label and pause bytes of all thirteen records
  (`0x653B9C`, `0x653BA4`, ... `+4..+7` of each) are `gap` too. On a
  disc-built image every line's label byte is 0 (circle) and every pause
  byte is 0. Read from both `data.bin`s on 2026-10-17.
- **The disc module's own labels** are not read by anything: three entries
  at US `0x801E1F14`, its own buttons in colours 1, 6, 5 (section 3).

The owner's rule (2026-10-17): a label whose button `σ` leaves where it was
draws exactly as the original reads it, glyph and colour. Only a moved
button takes its shape's colour. `Resolve` now does this. An unmoved button
gives entry = the line byte and pen = the image's `0x653C00[line]`. On the
PC that is 2, 1 or 6. On a disc install it is 0, which reproduces the white.

**Fixed since as D313 (section 8.8):** the records and pens come from the
disc's fishing module, and the label resolves in the image's release's
terms. What this paragraph found, as it stood: on a disc-built
image the banner's line bytes are all 0. So every line of the thirteen is
labelled circle, including the quit, gear and no-label lines, and every
line's pause is 0 where the PC's is 12 or 1. A pause of 0 is decremented to
255 by `LineNext` `0x466460` before it is tested. The fix belongs to
`exe_tables.py`: map the disc module's 12-byte records' label and pause
bytes (section 3, the PC's bytes) and the pens. It is not a draw change. Until
then, `σ` resolves every disc-install label from circle. The identity bar
holds, but after a swap a disc install's quit line would show `σ(circle)`.
RC's route shows only the reel line, which is circle on both.

### 8.7 Verification of the follow-up

- **Build:** clean.
- **Headless, narrow** (`fishing_text,sigma,effect_1a`): exit 0, and the
  self-test line of 8.3.
- **Headless, everything** (`BOF3X_SHADOW='*'`, an ini-less copy in
  `C:\Users\kerok\b_icf\launcher`):
  - 1,084 lines report `0 MISMATCHES` and none any other count;
  - `inject: 10087 ours, 0 left original by BOF3X_ORIGINAL`, then
    `self-test only: done`.
  - Defender quarantined that copy's `bof3x-launcher-dev.exe` at 19:47
    (events 1116 / 1117, `Trojan:Win32/Bearfoos.A!ml`) while it waited. The
    game ran to the end and wrote the log.
- **RC's fishing route on `psx-us`** (own root `C:\Users\kerok\b_icf`,
  `abc_run.py prepare --only psx-us`, then `run --route
  tools/recipes/sigma_fishing.txt --wide 0`, exe host):
  - **identity** (`icf2`) against RC's pre-round reference
    `b_rc\runs\id_sigma_fishing\psx-us`: **49 / 49 frames pixel-equal**.
    Against the round's `r_id`: 42 / 49. The seven are the reel label, as
    RC found.
  - **swapped** (`icf2sw`, `sigma_fishing_sw.txt`, save `camping_sw`)
    against RC's `r_sw_sigma_fishing_sw\psx-us`: **49 / 49 equal**. The reel
    label is a blue cross (pen 1), which moved, so it takes its shape's
    colour. The area 16 legend trades as before.

### 8.8 D313: the line records and pens from the disc (2026-10-17, `fix/fishing-records-disc`)

[`known-defects.md`](known-defects.md) D313 is the record. Section 8.6
found the defect; this is the fix and its measurements.

**The correspondence, measured** (a scratch reader over the seven discs in
`C:\Users\kerok\b_d246\discs` and `BOF3.exe`, 2026-10-17). Each fishing
module (`WORLD00/AREA030.EMI` section 4 on a PSX disc; `AREA089` and
`AREA129` carry the same bytes; `BOOT.BIN` on a PSP disc, where its fishing
code lives) holds the thirteen records as 12 bytes: a count, the string
pointer, then at `+8..+11` the PC record's `+4..+7` (label byte, pause byte,
two zero bytes). These four bytes equal `BOF3.exe`'s for **all 13 records on
all seven builds**. So the PC's 8-byte record is the disc's with the count
dropped. Nothing else in the PC record is a gap: its `+0` is the string
pointer, which `exe-layout.tsv` fills. Right after record 12 come three
3-byte label entries, then the PC's row table `0x653C04`, 12 bytes after the
entries' start on a PSX disc and 9 in `BOOT.BIN`. Each entry is the label's
text (its button's code and a NUL) with its pen at `+2`:

| Build | Records | Entries (code, pen) |
|---|---|---|
| psx-jp | `0x801E1EA0` | `0x1508` 2, `0x1509` 1, `0x150A` 6: circle, cross, triangle |
| psx-us | `0x801E1E78` | `0x81` 1, `0x82` 6, `0x83` 5: cross, triangle, square |
| psx-eu-en | `0x801E1E98` | as US |
| psx-fr | `0x801E212C` | as US |
| psx-de | `0x801E1E74` | as US |
| psp-jp | `BOOT.BIN` `0x2DFF00` | as psx-jp |
| psp-eu | `BOOT.BIN` `0x2DF728` | `0x81` 1, `0x80` 2, `0x83` 5: cross, circle, square |

(PSP-EU's own `AREA030.EMI` still holds Japan's entries. Its code is in
`BOOT.BIN`, so the rule reads that.) The US module's `LineNext` twin reads the
entry this way: `lbu` of the record's label byte at `0x801E1E80 + 12 *
line`, then the text `0x801E1F14 + 3 * label` and the pen `lbu 0x1F16(3 *
label)` (`0x801D3CE4..0x801D3D0C`, capstone). Its `LineScroll` and `LineFade`
twins do the same (`0x801D3EAC..`, `0x801D40CC..`, `0x801D4150..`).

**Which release's terms the label is in.** The line byte is the same on every
release. It names the line's *action's* entry, and the entry is the release's
own button for that action: Japan's on the Japanese builds, `P` of Japan's
on the PlayStation West, `S` on PSP-EU. These are `sigma.h`'s `M_R`. The
pens follow the button's shape on every disc (circle 2, cross 1, triangle 6,
square 5), so a pen is the release's button's. The PC's `0x66A2FC` and the
Config cell are in Japan's order. So on a Western image the byte is read
as `M_R(Japan's button)`, and its label through the Config is
`SigmaFrom(R, M_R(Japan's))` = `Sigma(Japan's)`. **`FishingText_LabelOf` did
need the translation** for the pen, not the button. The button it named was
already `Sigma(Japan's)`. But "unmoved" compared against Japan's button, so
with the disc's pens a Western install at Japan's words would draw an
unmoved red-pen slot as a circle in the West's cross pen (blue). `Resolve`
now takes the image's release:

- `own = M_R(Japan's button)`;
- the label's button is `SigmaFrom(R, own)`, or word 7's first for the cast;
- when that is `own`, the label is `own`'s entry in the image's pen, the
  release's disc draw. Otherwise it is the button's entry in its shape's pen.

The release is the running `.data`'s: the build of the disc-built image the
exe host mapped (`ExeImage_MappedBuild`, new in `src/hook/exe_image.cpp`), else
Japan's (`BOF3.exe`'s own, hosted, or a `pc-zh` image, byte-identical). On
the PC and every Japanese build nothing changes.

| Install, Config | Before D313 | Now |
|---|---|---|
| PC, any | unchanged | unchanged |
| psx-us, Japan's words (the camping save) | circle, pen 0 (white); every line labelled; 256-frame pauses | circle, red (a moved button: the West's slot is cross) |
| psx-us, the swap | blue cross (moved) | blue cross: the West's own button, the disc's pen 1 |
| psx-us, the West's defaults | cross / triangle / square, shapes' pens | the same pixels, now as the disc's own draw |

**Off** (`BOF3X_ORIGINAL=FishingLabelSigma`): the original's reads,
`0x66A2FC[line]` in `0x653C00[line]`. On a Western image that is the PC's
glyph for Japan's button in the West's pen, a circle in blue. That is
Capcom's PC code over the West's data, and it is left as such.

**The rule** (`tools/psp_exe.py`, rules `fishrec` and `fishpen`; called by
`exe_tables.disc_image` for every disc build). `fishrec` finds the records
by `BOF3.exe`'s four bytes at `+4` (the disc's at `+8` of 12) in the module,
as `faerie` does. It copies them record for record into
`0x653B98..0x653C00` (52 bytes; the pointer words are left to the layout).
`fishpen` copies the three pens from the entries after the records it found,
into `0x653C00..0x653C03`. It is kept only when the PC's row table follows
within 12 bytes, and only beside its `fishrec` row. Its bytes are
`BOF3.exe`'s on psx-jp and psp-jp, and the release's own elsewhere (the
recipe prints "3 not BOF3.exe's", which is the point). `psp_exe.py check`
holds both rows to their ranges.

**The pacing.** `LineType` puts the pause byte in `+9` when a line is typed.
`LineNext` decrements it before it tests it, so a 0 runs 256 frames before the
column moves on and the next line spawns. The PC's pause is 12, or 1 for
text 12. A zero label byte also made every no-label line flip a label in
(6 frames) and fade one out. Measured on `fish_dense` (the fishing route
with 400 frames added and a shot every 5 from 800; `C:\Users\kerok\b_fish\
pacing.py`, the typing edge lit or dark). The before run is the new code over
the same image with the 55 bytes zeroed again (`make_before.py`):

| Run | Typing-edge spans from the reel (frame 1050) on |
|---|---|
| psx-us, before | 1050..1110, 1225..1280, 1535..1660: two lines, then about 255 frames of nothing |
| psx-us, now | 1050..1110, 1120..1210, 1225..1280, 1290..1415, 1420..1475, 1485..1610, 1615..1670: each line follows 5..10 frames after the last |
| pc | 1050..1085, 1095..1210, 1225..1255, 1265..1365, 1375..1475, ...: the same 5..10-frame follow (its Chinese lines type at 12 units) |

In the before run the rod-steering line (label `0xFF` on
the PC) ends in a red circle. Now it has none.

**Checks:**

- `psp_exe.py recipe` over the seven discs: `fishrec` "52 bytes, 0 not
  BOF3.exe's" on all seven; `fishpen` 0 on psx-jp / psp-jp and 3 on the
  other five. Then `exe_tables.py recipe`, `exe_own.py measure`,
  `exe_tables.py gaps`: `exe-gaps.tsv` loses exactly the 13
  `EffectKind0D_States` rows on each build (91), nothing else.
  `exe-psp.tsv` +14. `exe.toml` moves the seven hashes and counts. Layout,
  pointers and places are unchanged.
- `importer.py check`, `exe_tables.py check`, `psp_exe.py check`,
  `exe_own.py check`: 0 errors. `importer.py stale` on a cache built before:
  `stale exe  tools/psp_exe.py changed (build_fill)`.
- Headless, everything (`BOF3X_SHADOW='*'`, an ini-less launcher copy in
  `C:\Users\kerok\b_fish\st`, the PC install): 1,084 lines `0 MISMATCHES`
  and none any other count; `fishing_text self-test: ... 7 labelled lines on
  four images (this one, in Japan's terms, pens 2 1 6; ...) ... 2880
  assignments (81259 checks); 0 MISMATCHES`; `inject: 10087 ours, 0 left
  original`; `self-test only: done`. A hosted run cannot map a disc image,
  so the West's terms are tested by the synthetic images and the live run.
  (A narrow hosted run with `BOF3X_EXEIMAGE` naming the psx-us cache only
  compares that image: `.data` stays the PC's.) The fishing_text self-test now runs four images: this one,
  in its release's terms, plus Japan's, the West's and PSP-EU's, each with
  its own pens by rule. It checks that this image's pens are its release's
  and every pause is non-zero (the defect's two signatures), that each
  release's defaults draw that release's own labels, and the other Configs
  and 4 x 720 assignments.
- Live, `psx-us` (`C:\Users\kerok\b_fish`, `abc_run.py prepare --only
  psx-us,pc`, `--wide 0`): `sigma_fishing.txt` and its `_sw`, the dense
  route on psx-us and pc, its swap twin, a West-defaults twin
  (`fish_dense_west.txt`, save `camping_west`: the camping save's words
  mapped by `P`, the route's presses renamed by `P` from frame 340), and the
  before run. The log reads `labels in the PlayStation West's (P) terms (the
  running .data is the psx-us-built image)`. Against RC's round references
  (`b_rc\runs\r_id_sigma_fishing`, `r_sw_sigma_fishing_sw`, psx-us), both
  routes are 43 / 49 frames pixel-equal. The six are `f01140` and `t001140`
  .. `t001320`, the reel banner. On the identity route that is the red circle
  where it was white, plus the next line following. On the swap the blue
  cross is unchanged and the next line follows. Sheets are in
  `C:\Users\kerok\b_fish\banner_sheet.png` and `banner_sheet2.png`.

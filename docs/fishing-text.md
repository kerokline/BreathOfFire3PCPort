# The fishing minigame's text - where it lives, and DIV-0069

**Status:** MEASURED (2026-10-03, fix wave group FL, headless only) - the
strings and draws read from the disc, the exe and our source; the fix built
and fuzzed; **not yet seen in game** (section 6 is the coordinator's live
check). The fishing code itself (`0x52AF80..0x52CD47`) is Capcom's and
round fourteen's; everything changed here is in draws that were already ours.

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

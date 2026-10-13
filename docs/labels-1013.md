# The exe's labels, 2026-10-13: the Identify panel's words, Beyd, fight 26's counter, the hi-lo choices

**Status:** IN PROGRESS (2026-10-13, branch `away/1013-labels`; built and
self-tested, the Identify panel captured in five languages; Beyd, fight 26
and the hi-lo game reached by no recipe - section 4)

The owner's calls on [`exe-chinese-remaining.md`](exe-chinese-remaining.md)
(2026-10-13): restore the Identify panel's weakness heading the PlayStation's
way, fix the French and German item word, and build three new label groups -
Beyd's name, fight 26's turn counter, the hi-lo game's choices. All four are
DIV-0064's machinery (`src/game/labels.cpp`, chunk kind 15 of
`tools/loc_build.py`); the ledger entries are written `DIV-XXXX` for the
coordinator to number. No game data is in this doc: addresses, counts, and
the discs' own short words as earlier docs quote them.

## 0. What came out

1. **The Identify panel draws the discs' small words** (DIV-XXXX, an amendment
   of DIV-0064 group 15): `WEAK` `POINT` on the weakness row and the item word
   above the items at the disc routine's own places, the EXP label drawn whole,
   under every PlayStation overlay including ja-JP. The French and German
   layers now draw `OBJET` / `OBJEKT` (and the German `EP`) where they drew the
   PC's English (section 1.3, a defect for the coordinator to number).
2. **The Western discs draw `WEAK POINT` on the party member's panel only** -
   their enemy panel has no weakness row at all, no heading and no element
   glyphs; the Japanese disc draws both. The PC keeps the row (and its six
   Chinese element glyphs) on the enemy panel; ours labels it as the Japanese
   disc does. Dropping it under a Western layer is the owner's call (1.2).
3. **Three new groups** (DIV-XXXX each): 17 Beyd's name (`BATE.EMI`, by the
   copy of 5 two calls after the sound `0x102`), 18 fight 26's turn counter
   (`BOSS025.EMI`: the words, the count's and the words' x, the singular's
   test - the layout follows the disc), 19 the hi-lo choices (`COMMU02.EMI`,
   centred as the discs place them, drawn whole). All four groups now live in
   buffers of ours read by our own code: **no `.data` byte changes under an
   overlay**, so the overlay state-hash references stand.
4. `tools/text_scan.py --ledger` still exits 0: 424 strings, **cover 145, left
   9** (was 141 / 13); with `--singles` 504, cover 155, left 23.
5. **Not reached in game:** no committed recipe enters fight 26, Beyd's
   equipment screen or the hi-lo game; a probe from the owner's `preBeyd` save
   did not get there (section 4). Their draws are checked by the builder's
   reports, the DLL's log, the self-tests and reading.

## 1. The Identify panel (group 15 amended)

### 1.1 What every disc's MAGIC060 draws

`MAGIC060.EMI` (`BIN/BMAGIC/`), one section at `0x801EEC00`, read with
capstone (the session's `mdis.py` / `grepdis.py` over `psx_disc.Disc` and
`exe_twins.emi_sections`). The task's draw step picks the panel by the target
byte `0x80146384` (`sltiu 3`): the member panel `0x801EF174`, the enemy panel
`0x801EF2DC` (US). The member panel's calls, US:

| Call at | Draw | Text | (x, y) |
|---|---|---|---|
| `0x801EF1D8` | `0x801EF590` (12 px) | the member's name | (`0x70`, `0x30`) |
| `0x801EF20C` | `0x801EF76C` (8 px) | `WEAK` | (`0x5A`, `0x44`) |
| `0x801EF220` | `0x801EF76C` | `POINT` | (`0x80`, `0x44`) |
| `0x801EF238` | `0x801EF590` | the marks row | (`0x70`, `0x50`) |
| `0x801EF264` | `0x801EF590` | `EXP` | (`0xB0`, `0x64`) |
| `0x801EF29C` | `0x801EF76C` | `ITEM` | (`0x5A`, `0x8A`) |

`0x801EF76C` is the small draw: per letter one 8 x 8 quad from (x, y), the
cell `u = (c - 0x40) * 8`, `v = 8`, the pen 8 on - so the coordinates are the
glyphs' top-left. The PC's panel draws on the same rows (`Identify_DrawMember`:
the heading at `0x44`, the marks at `0x50`, EXP at (`0xB0`, `0x64`), the items'
heading at `0x8A`).

The words per disc, as `loc_build.py`'s `identify_words` reads them (the first
call at each of those four places whose `$a0` is a string; session script
`try_readers.py`):

| Disc | weakness heading | item word | EXP label |
|---|---|---|---|
| US, European, Japanese | `WEAK` `POINT` | `ITEM` | `EXP` |
| French | `WEAK` `POINT` | `OBJET` | `EXP` |
| German | `WEAK` `POINT` | `OBJEKT` | `EP` |
| PSP-EU, PSP-JP (`MAGIC060.EMI` is the Japanese build's) | `WEAK` `POINT` | `ITEM` | `EXP` |

`BOF3.exe` carries the Japanese disc's four words in `.data` unread:
`WEAK` `0x65AAA8`, `EXP` `0x65AAB0` (read: the push at `0x4B1150`, and
`0x4B12B4` / `0x4B1375` on the enemy panel), `ITEM` `0x65AAB4`, `POINT`
`0x65AABC` (a dump of `0x65AA90..0x65AB20`, session script). They are group
15's shipped slots 2..5: a word the builder does not find, or an overlay built
before today, draws them.

### 1.2 The enemy panel: the Western discs dropped the weakness row

`WEAK` is loaded once in the US, European, French and German routines and
three times in the Japanese (a scan of every `lui` / `addiu` pair). The US
enemy panel `0x801EF2DC` draws the name, the EXP and zenny lines (values or
marks), `ITEM` and the two items - **no weakness heading, no marks row, no
element glyphs**. The Japanese `0x801EF2F0..` draws `WEAK` `POINT` in both of
its branches, the six element glyphs (`0x801EFC84` at (`0x66`, `0x50`), call at
`0x801EF36C`) for an identified enemy and the marks row for the rest. The PC
port's enemy panel is the Japanese one (its elements at (`0x66`, `0x54`)).

So "the PlayStation's way" splits: on a party member both agree with what we
draw now; on an enemy, the Western discs show nothing on that row. We draw
`WEAK POINT` over the PC's row on both panels (the Japanese disc's layout,
which the PC's row is) and leave the six element glyphs as they are - Chinese
glyphs at `0x66A380..` drawn as quads by glyph code, which no layer repaints
(`exe-chinese-remaining.md` 3). **For the owner:** keep the row (as now), or
drop the enemy's weakness row under a Western layer as the Western discs do.
The capture of section 1.4 is an enemy panel (`Fly Man`, unidentified: the
marks row).

### 1.3 Defect: the French and German layers drew the English `ITEM`

Since DIV-0064 group 15 (2026-10-10) every Latin layer drew the PC's `ITEM`
(`0x65AAB4`) above the items, where the French disc has `OBJET` and the
German `OBJEKT`; and the German disc's EXP label is `EP` where every layer
drew the PC's `EXP` (`0x65AAB0`). The evidence is the base's code
(`magic_s12.cpp` at `0a6ecc1b`: `Text(kItemWord)` and `Text(kLabel1)` under
any written group 15, whatever the language) and the en-US `before` capture
of 1.4, which draws that `ITEM`; no French or German capture was made with
the base build. Fixed by the builder sending the disc's words (group 15's
slots 4 and 5): `OBJET`, `OBJEKT` and `EP` in the fr-FR and de-DE captures of
1.4. The coordinator numbers it.

### 1.4 Captures

`tools/recipes/identify.txt` with a shot every 30 frames from frame 600
(`recipe_shots.py --every 30 --from 600`, `--save identify`, `BOF3X_OPT=none`,
`--no-front`, from an ini-less launcher copy and a scratch game directory
whose `DAT/` is hard-linked to `bof3/`'s shipped containers, the overlays
built into it by this branch's `loc_build.py all`). The panel is up at frame
900. Measured on the frames (rows / columns of pixels at or above 150, game
units, session script `rows.py`):

| Run | What frame 900 shows |
|---|---|
| `identify_before_en` (the branch's base, en-US) | no heading; `ITEM` from row `0x8E`, x `0x5B` (drawn at (`0x59`, `0x8E`)) |
| `identify_after_en-US` | `WEAK POINT` rows `0x44..0x4A`, x `0x5A..0xA2`; `ITEM` from row `0x8A`; `EXP` |
| `identify_after_fr-FR` | `WEAK POINT`; `OBJET`; `EXP`; the name `H-volant` |
| `identify_after_de-DE` | `WEAK POINT`; `OBJEKT`; `EP` at x `0xB0..0xBE` |
| `identify_after_ja-JP` | `WEAK POINT` and `ITEM` small; the Japanese name whole; no Chinese heading |
| `identify_after_original` | the port's panel: `弱点`, `持有物`, `EXP` - as shipped |

The sheet sent with this branch: the session's `cmp_identify_enemy.png`
(crops of the five overlay runs side by side).

## 2. Fight 26's turn counter (group 18)

`BOSS025.EMI` section 13 at `0x800C1800` holds the routine at `0x800C3088` on
every Western disc (and `BOSS027.EMI` the same; `turn_counter` reads both
identically). It draws the box (`0x801D8270(0x6B, 0x10)`), formats `%2d` of
`0x15 - turn` (the turn `0x801463D0`, the PC's `0x904B90`), draws it with the
12 px numerals and then one string with `Text_DrawAt`:

| Disc | count at | words at | plural | singular | singular when |
|---|---|---|---|---|---|
| US (`0x800C30C8..0x800C30F4`) | `0x70` | `0x80` | `turns left` | none | never |
| European (`0x800C30E4..0x800C311C`) | `0x70` | `0x80` | `turns left` | `turn  left` | turn `== 0x14` (`beq`) |
| French | `0x94` | `0x70` | `Rest   tours` | `Rest   tour` | turn `>= 0x14` (`sltiu`) |
| German | `0x70` | `0x80` | `turns left` | `turn  left` | turn `>= 0x14` (`sltiu`) |

The French spaces leave the count's room between the words; the German disc
has the English words. The PC (`Boss26Fx_DrawCount` `0x43C780`) draws a word
at `0x7A`, the count at `0x92` and a word at `0xAA`. The layer's group 18 is
three strings: the plural, the singular (the plural again for the US), and
the layout as four raw bytes - count x, words x, the test (`n`, `=`, `>`) and
its value (`p 80 n 01`, `p 80 = 14`, `94 p > 14`, `p 80 > 14` as the builder
printed them). Under a written group the draw is the disc's; with no overlay
the PC's.

## 3. The hi-lo game's choices (group 19)

The three choices are 4-byte slots right before the 72 bytes of sprite
rectangles the PC has at `0x652D04` (group 14's anchor): US / European
`HIGH` (no NUL) `LOW` `SET`, French `Haut` `Bas` `Mise`, German `Hoch` `Tief`
`Setz` (`COMMU02.EMI` `0x801DAC54` US); PSP-EU's `BOOT.BIN` has the US three
the same way. The routine (US `0x801D2AF0..0x801D2CD4`) draws the `0x2A` x
`0x14` box and then the label at y + 3, at x + 6 for a four-letter word and
x + `0xA` for a three-letter one (US: label 0 at + 6; French: all but label 1;
German: all at + 6) - the box's middle for 8 px letters, which ours computes
as x + (`0x2C` - the pen width) / 2.

The count it passes is not one number: US and European 4 in mode 0 and 3 in
modes 1 and 2; French 4 in modes 0 and 2, 3 in mode 1; German 4 in all.
By the PC's `Text_DrawAt`, whose count limits the characters, the count 3
would draw `HIG` - so either the PlayStation's draw counts differently or the
US disc cuts the word in the highlighted modes; not settled (no capture of
the PlayStation's game). Ours draws every word whole (`0xFF`).

## 4. What no recipe reaches

- **Fight 26 and Beyd's name.** The battle extra and its count are fight 26
  (`BOSS025`, area 92; `boss_se.md`). No committed recipe enters area 92. The
  owner's save `tools/recipe_saves/preBeyd.DAT` loads at a harbour with the
  party beside two people; a probe (the load opening of `whelpBoss.txt`, then
  standing, then up-left and the circle button every 30 frames for 1,500
  frames, shots every 60) changed nothing on screen. Crafting the route needs
  the owner's knowledge of where the duel starts; not cheap, not done.
- **The hi-lo game.** `fairyVillage.txt` (78 shots every 60 frames, under
  this branch, en-US) visits the board, its lists and two shops - not the
  game. DIV-0064's own entry had it as "no route" too.

What stands for them: the builder's reports and the log's `DIV-0064: 1 of 1
guest name`, `3 of 3 turn counter`, `3 of 3 hi-lo choices` (en-US, fr-FR,
de-DE runs of section 1.4); the self-tests (their draws are fuzzed against
Capcom's before an overlay loads); and the code read in sections 2 and 3.

## 5. Checks run

- `python tools/loc_build.py selftest`: ok, with a new case - the three MIPS
  readers on synthetic code (no game data), the guest reader refusing a copy
  with no sound before it.
- `loc_build.py all --disc <cue> --game <scratch game>` for the US, European,
  French, German and Japanese discs: `identify words 6, guest name 1, turn
  counter 3, hi-lo choices 3` on the four Western discs, `identify words 6` on
  the Japanese; PSP-EU through `convert_labels_psp` (a session script, nothing
  written): `identify words 6`, `hi-lo choices 3`.
- `tools/text_scan.py --all --ledger` with the five PSX discs: exit 0 (section 0).
- `tools/ledger_check.py`: the four new entries fail only on their unnumbered
  `DIV-XXXX` ID lines (4 errors, by design until the coordinator numbers them).
- `tools/exe_tables.py check`: 0 errors.
- Self-tests: see section 6.

## 6. Self-tests and the disc-only installs

(filled in below as the runs finish)

## 7. Left, and for the owner

- **The owner:** the enemy panel's weakness row under a Western layer (1.2);
  the hi-lo count 3 question (3) if a PlayStation capture is to hand; the
  four groups in play.
- **Not built:** groups 17 and 18 from a PSP disc (its `BATE` / `BOSS025`
  EMIs are the Japanese build's; PSP-EU's `BOOT.BIN` carries `WhelpBeyd` and
  `turns left` / `turn  left` packed, but the layout lives in its Allegrex
  code, not read); a Japanese layer's groups 17..19 (the names would be
  Japanese text: `Beyd`'s katakana is over the 5-byte copy).
- `tools/exe_tables.py`'s `LANGUAGE_ZERO` still lists `0x669CE8` as a language
  address no layer reaches: true of the byte (ours reads a buffer), so left.

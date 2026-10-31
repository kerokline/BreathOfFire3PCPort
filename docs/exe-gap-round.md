# The exe-gap round: the disc-built image's last differences, and STRICT as the default

**Status:** DONE, PR at the owner's word (2026-10-31: launched at the owner's
word late morning, five groups merged on `exe-gap/round` the same day, the
end batch run unattended in the afternoon and evening and passed whole
(section 9a); DIV-0196 the owner's rule on the Config labels after it;
unpushed. Section 4a has each group as run, section 7 the owner's calls.
Framed 2026-10-20, re-measured and staged 2026-10-31 morning, briefs under
`C:\Users\kerok\bof3x-runs\eg1031`.) It also carried HANDOFF's "disc-only
and PSP polish" item (group AU).

## 0. What the round is for

A disc-only install builds `BOF3.exe`'s `.data` from the player's discs
([`exe-import-engine.md`](exe-import-engine.md)). Every byte the PC has
non-zero that no disc fills and the engine does not compute is a **`gap`**
range in `recipes/exe-gaps.tsv`. The image leaves it zero and logs it, and
`BOF3X_EXEIMAGE_STRICT=1` refuses an image that has one.

The round's end is **I43 step 4: STRICT on by default for disc-only
installs**, so a *new* gap fails loudly at start instead of drawing wrong.
[`install-gap-reads.md`](install-gap-reads.md) 9.7 says why that cannot be
switched on today: three blockers, below. The round removes them.

## 1. Where it stands (re-measured 2026-10-31, `main` `43e98773`)

`gap` rows per build in `recipes/exe-gaps.tsv` (last regenerated 2026-10-27,
the descriptor-table rule and the faerie operand), as ranges / bytes, counted
from the file (`how = gap`; the `port-relaid` and `disc-unplaced` classes
have no row left):

| Build | game-value | language-zero | total |
|---|--:|--:|--:|
| psx-us | 45 / 650 | 2 / 10 | 47 / 660 |
| psx-eu-en | 46 / 693 | 2 / 10 | 48 / 703 |
| psx-fr | 49 / 728 | 2 / 10 | 51 / 738 |
| psx-de | 49 / 728 | 2 / 10 | 51 / 738 |
| psx-jp | 15 / 228 | 5 / 51 | 20 / 279 |
| psp-jp | 16 / 216 | 5 / 51 | 21 / 267 |
| psp-eu | 18 / 227 | 2 / 10 | 20 / 237 |

Against 2026-10-20's 49..74 rows a build (the table this section held
then): the away day's rules, issue #149's placements, the 32 hand-read twins
of 2026-10-26 and the descriptor-table rule took the rest. **71 distinct
ranges** over the seven builds, **14 on all seven**, and they fall into a
few families, which is what makes the round small enough to cut:

| Family | Ranges | Builds | What they are | The answer this round writes |
|---|--:|---|---|---|
| `Scena17_EndSteps+0xD4..+0xA10` | 35 | the four Western PSX | the Western ending roll's step table and strings | **accepted unread**: DIV-0148 carries the roll as kind 17 of the language layer, the engine reads it from there; nothing reads these bytes on a Western image (section 3) |
| `Equip_SlotIcons+0x50..+0x14C` | 10 | all 7 | the ten formation names, 16 bytes of each record | **layer**: kind 15 group 10 writes them under a Latin layer (DIV-0064); blank under `ja-JP` until JA (D280) |
| `CommuMusic_LabelC+0xC..+0x4C` | 6 | mixed (DE/FR 3, US 1, EU 1, JP+PSP 1) | bytes among the faerie music-player labels no twin placed (the census's `CommuMusic_CloseSteps+0x28..+0x68` rows) | **PL** for `+0x4C` (the reader `CommuMusic_DrawList`); the FR / DE label rows are **pending**: the owner's open question, [`install-gap-labels.md`](install-gap-labels.md) 4.4 |
| `ConfigScreen_OpenStates+0x141` | 2 | all 7 | the bytes after D282's row tables | **PL**: reader, then place or `unread` |
| `EffectKind66_States+0x18/+0x20/+0x28`, `Rest3e_Windows+0x14/+0x18/+0x1C` | 6 | the four Western PSX | the Western u16 fields (`install-gap-twins.md` 2, 3) | **port-own**: the PC's, which both PSP discs hold too (section 7, measured 2026-10-31) |
| `TornadoMote_Phases+0x10` (PSX) / `+0x15` (PSP) | 2 | all 7 | the Identify panel's ASCII `WEAK` / `EXP` labels (the `?` marks are the engine's since D278) | **PL**: a label group or the engine's own string |
| `Char_DefaultRecords`, `+0x47C` | 2 | all 7; psp-eu | the default names' fields | **layer** (DIV-0020); the psp-eu tail **PL** |
| `WardMote_Tilts+0x8` | 1 | all 7 | four bytes past the placed tilts, no reader found | **accepted unread** or **PL** (a reader) |
| `Area121_BandHeights+0x1`, `LinkedObjectD_Amounts` | 2 | the PSPs; psp-eu | bytes under every `find_imm` floor | **PL**: the floor, or a hand-read twin |
| `Encounter_SlotChance+0x1C` (`0x669CCC`) | 1 | all 7 | the empty save slot's text | **unread** once AU draws the slot blank on every install (section 7) |
| `BattleE1_GuestHeadOut+0x8`, `Effect1b_ToggleText+0x10`, `CommuRank_Modes+0x8C` | 3 | the two Japanese builds | a name copied at the extra-equip screen, the chop's count word and the faerie record 0 name, all written by a Latin layer on the other builds | **JA** (D280) |
| `Effect1b_ToggleText+0xC` (`0x66A094`) | 1 | all 7 | the count's one-glyph word | **layer**: DIV-0064 group 25 (2026-10-30) |

`exe_tables.py check` 0 errors at this tip. Every row still has a status in
[`install-gap-census.md`](install-gap-census.md) 8, written 2026-10-17..18;
the table above is the status today and AC rewrites section 8 from it.

## 2. The three blockers (install-gap-reads.md 9.7, brought up to date)

1. **STRICT refuses every `gap` range, read or not.** With 20..51 per build
   (section 1) it would refuse all seven installs at start
   (`src/hook/exe_image.cpp`, the `strict` lambda and the loop under it).
   The step means "a new gap fails loudly", so the switch has to tell an
   accounted row from a new one: group AC writes the list, group ST makes
   the engine read it.
2. **The owner's rows**: ~~D266~~ (done 2026-10-20), the empty save slot and
   the Western u16 fields - both answered 2026-10-31 (section 7): the slot
   blank everywhere (AU), the fields the PC's (port-own). The `pending`
   answer stays available for a later owner's row.
3. **Coverage.** "Not read" rests on eleven routes (19 areas, four battles);
   48 of the 88 unread rows were sampled, before the placements since. psp-jp
   stopped at five routes on D321's Fatal; **D321 is fixed since 2026-10-18**,
   so psp-jp is covered by this round's end batch (section 9).

Also in the way of a Japanese disc-only default, though not STRICT's:
**D280**, the `ja-JP` layer's kinds 7..15 - group JA.

## 3. The owner's rows

**Answered** (both 2026-10-13, "the away day" in
[`owner-review.md`](owner-review.md); the 2026-10-18 section listed D266
again as a call, which it no longer was). **D266 built 2026-10-20** with
option 1 below (`fix/d266-title-mark`, DIV-0101's `titlemark` rule; a US
disc-only title draws its TM whole); D280 not built:

- **D266, the Western title mark** (`Title_Sprites+0xE`, `0x65385E`, the
  four Western PSX builds). Only the **width byte** is missing. The mark's
  art is already there: a disc-only install draws the disc's own title and
  glyph pages (DIV-0114), and a Western disc's glyph page holds the ™ at
  (u 96, v 60). The ™ is 12 wide where the PC's ® is 10, so no segment of
  the PC's table matches the disc's and the image keeps 0, which draws the
  mark zero wide. Options for the byte (_2026-10-20: built as option 1's
  source but as its own `titlemark` rule in `tools/psp_exe.py`, not a
  `twin` row - `recipes/exe-psp.tsv` carries one `titlemark` row per
  Western build_):
  - **a `twin` row from the disc's `GAME.EMI#1`** (the owner's yes): each
    Western disc carries the whole `Title_Sprites` table at the same address
    (`titleraw.py`, [`disc-data-inventory.md`](disc-data-inventory.md) 4.1),
    so the disc's own 12 is read, as D261's rows were made
    ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3). **Preferred.**
  - an engine rule measuring the mark's width off its tile in the glyph page
    the install already has (`exe_own`'s pattern). Same answer from the
    player's disc, more code, and only worth it if the twin cannot be found.
  - the PC's 10: wrong for the ™ (clips its last two columns), and a copied
    value; not an option.
- **D280, a `ja-JP` layer for kinds 7..15** (kind 16 done with D279,
  DIV-0069 amended). The method that worked is in D280's entry: find each
  table by its reader on the Japan disc (`lui_sites`), counts in characters.
  The largest single piece of the round.

**Answered 2026-10-31** (section 7 has the answers; the lines kept for the pointers):

- **The empty save slot** (`Encounter_SlotChance+0x1C`, `0x669CCC`, all
  seven builds, read on every load screen): the staged localisation PR,
  [`name-entry-restoration.md`](name-entry-restoration.md) section 8 first.
- ~~**The Western ending roll**~~ - **answered and built 2026-10-20**: "let
  western installs use their own roll" (DIV-0148, D323). The roll differs by
  release, not language (EU, FR and DE byte-identical; the US its own; the
  PC = Japan). It cannot live in `.data` (its line table is pointer words,
  which must stay the PC's), so a Western language layer carries it as
  kind 17 and the engine reads its lines from there - the roll follows the
  language, on a PC install too (the owner's second call the same day). Its `Scena17_EndSteps` gap rows stay
  in `exe-gaps.tsv` - unread on a Western image now, so section 4 C's
  "accepted unread, with the evidence" is their answer.
- **`Area124_Weights+0x50`** (EU, DE), **the Western u16 fields of
  `EffectKind66_States` / `EffectKind5B_States`**: the disc's numbers or the
  PC's ([`install-gap-twins.md`](install-gap-twins.md) 2, 3).
- **`0x66A094`'s layout** under a Western layer
  ([`install-gap-labels.md`](install-gap-labels.md) 4.3, 8) - **done
  2026-10-30** as DIV-0064's group 25, on the owner's catch in play.
- ~~**`Area65_DriftUV`** and the other `port-relaid` rows~~ - no
  `port-relaid` row is left in `exe-gaps.tsv` (2026-10-31).

## 4. The groups (cut 2026-10-31)

Six groups, two stages. Stage A's four run in parallel, each in its own
worktree off the round branch; stage B's two after stage A merges. Every
group reads `brief_common.md` first; the briefs are under
`C:\Users\kerok\bof3x-runs\eg1031\` (section 10).

### Stage A

**JA - D280, a `ja-JP` layer for kinds 7..15** (`tools/loc_build.py`,
`src/game/labels.cpp`, `src/game/config_text.cpp` and the readers each kind
feeds; the largest group, its own). The Japan disc's text for the Config
screen's labels and options (kind 7), the menu verbs (8), the battle
commands and skill-type title (9), New Game's default names (10), the
merchant lines (11), kind 12, and the label groups of kind 15 - found by
their readers on the Japanese disc as D279's fix found the fishing module's
(`lui_sites`), not by the Latin anchors; each draw checked for the
`c - 0x26` branch (DIV-0144), the `0xFF` space, character counts and
NUL-less records (D280's four lessons). On the PC under `ja-JP` the port's
Chinese becomes Japanese; on psx-jp and psp-jp the blanks fill. The three
Japanese-only gap rows of section 1 are this group's. Ledger: DIV-0015 /
0018 / 0019 / 0020 / 0064 extended by amendment, drafted lettered. Checks:
`loc_build.py all --discs` over the Japan disc and PSP-JP, `BOF3X_SHADOW=*`
headless, and headless captures of the Config screen, the field menu, a
battle and the New Game names on psx-jp, psp-jp and the PC under `ja-JP`
(the harness root `eg1031\ja`). If a kind cannot be read from the disc, the
report says which and why, and the kind stays as it is - no PC bytes into
code (the owner's stance, 2026-10-10).

**PL - placement with no owner** (`tools/exe_tables.py`, `tools/exe_own.py`,
`tools/psp_exe.py`, `symbols.toml`'s `psx` fields; recipes regenerated but
**not committed** - the coordinator regenerates at the merge). The rows of
section 1 marked PL: `CommuMusic_LabelC`'s six, `ConfigScreen_OpenStates+0x141`,
`TornadoMote_Phases+0x10 / +0x15`, `Char_DefaultRecords+0x47C` (psp-eu),
`WardMote_Tilts+0x8`, `Area121_BandHeights+0x1`, `LinkedObjectD_Amounts`;
then the PSP-EU remainder of [`exe-image-twins-1013.md`](exe-image-twins-1013.md)
6 where a reader is found (the overlay-and-`BOOT.BIN` class, `ConfigMenu_Steps+0xC`)
and [`install-gap-pairs.md`](install-gap-pairs.md) 8.5's leftovers if any
still show in a scan. Each row ends **placed** (a twin, a rule in
`exe_own.py`, an `immref`), or **not placed with the reason** (no reader; a
reader no disc file holds), which AC turns into an accepted row. Budget:
`exe_tables.py`'s placement rounds are 8 and the European builds use 7
(HANDOFF traps). Checks: `exe_tables.py check` 0 errors, `exe_scan.py` over
the seven discs, `data_diff.py run` over the seven discs for any recipe
change (data-diff.md 5), `BOF3X_SHADOW=*` headless.

**AC - the accounting** (reads, the new recipe file, `tools/exe_scan.py`,
`docs/install-gap-census.md` 8). Writes `recipes/exe-gaps-accepted.tsv`:
one row per `gap` row of `exe-gaps.tsv` - `build`, `lo`, `hi`, `symbol`,
`answer`, `evidence` - where `answer` is one of **layer** (a language layer
writes it: the kind and group), **port-own** (the engine supplies it: the
function), **unread** (no reader, or a reader no route reaches: the
disassembly or the read-watch file that shows it), **pending** (the owner's,
with the owner-review pointer), and `evidence` is a doc section or a
`symbols.toml` entry, never a byte. `exe_scan.py --accepted` fails on a
`gap` row with no accepted row and on an accepted row with no `gap` row,
so the list cannot drift; the file's shape is generated, its answers are the
hand's. Section 8 of the census rewritten as "the rows today" from the list
(its 2026-10-18 statuses move to a dated subsection). Runs in parallel with
PL: rows PL places are dropped from the list at PL's merge (the check names
them).

**AU - the disc-only leftovers** (`tools/importer.py`, the music tools,
`src/hook`'s hosted checks). (a) **D320**: on every PSX-only and PSP-only
install a lost fight restarts the fight's music because song 166 is missing;
the PlayStation plays the battle bundle's sub-song 2, which has `BGM082`'s
sub 2's shape (table song 86), and the owner's names call 166 the alternate
of 86 - so the importer writes 166 from that sub-song for a disc-only
install, the loss tune heard in the end batch (headless: the file exists
and `Music_Play(0xA6)` opens it; the ear is the owner's). A ledger entry if
the tune is not byte-for-byte the PC's (it will not be: the PC's is an MP3
of Capcom's mix). (b) [`disc-only-round-review.md`](disc-only-round-review.md)
2's notes: the Config screen's and `yes_no_layout`'s hosted checks compare
the fields the readers use (`g_ops`, `g_stops_moved`), `exe_image.cpp`
reads `BOF3X_EXEIMAGE` with the wide API, `bss_numbers`' halfword test says
what it tests, `under_jmp` entries get a log line. (c) The four stale
Chinese pairs (DIV-0092) are the coordinator's to re-record in the end
batch, not AU's. (c) **The empty save slot drawn blank on the PC install**
(`Encounter_SlotChance+0x1C`, `Menu_DrawSaveSlot`): the owner's call of
2026-10-31, as the US disc draws it; a ledger entry with a switch. (d)
**D257, the PAL banks** (the owner's call of 2026-10-31): the importer
accepts the eight sample-swapped area banks a European PSX disc builds
(`region-diff.md` 8.4, `sound-import.md` 7 item 1) for a European
disc-only install; a ledger entry. **Not AU's:** the PSP-EU title's layout
(D256, the owner's).

### Stage B (after JA, PL, AC and AU merge)

**ST - STRICT narrowed, then on** (`src/hook/exe_image.cpp`, `tools/exe_tables.py`'s
image writer, `src/launcher`, DIV-0101 amended). The image's range list
(`base/exe/`'s ranges, read by `ExeImage_Source`) carries the accepted
answer beside each `gap` row - the importer writes it from
`exe-gaps-accepted.tsv` - and the engine refuses a `gap` with none:
`Fatal` with the range, the symbol and "rebuild the install", the launcher's
stale-piece message for a refused image as `FilledSince()`'s. Then the
default flips: strict on for a disc-built image, `BOF3X_EXEIMAGE_STRICT=0`
(ini `off=`, the registry regenerated) for the old leniency; a PC install
unchanged. The negative control is in the self-test: a planted range refused.
The ledger: DIV-0101 amended, the `Switch:` / `Default:` fields updated.

**CV - coverage, the coordinator's end batch** (section 9): the read watch
over the eleven routes on all seven installs, psp-jp included, under this
tip; the attract; the A/B/C routes; the state hash. Not an agent.

### 4a. As run (2026-10-31, Opus agents in `eg1031\wt_<group>`, merged in the order AC, AU, PL, JA, ST)

Stage A's four ran in parallel from `657d5a89` (the round tip after the
owner's four calls, section 7) and reported within the day; ST ran off
`1e735bba` (AC, AU and PL merged) while JA was still running, and merged
after it. The order planned (PL first) was not kept: AC and AU were ready
first, and AC's check pruned the rows PL placed at PL's merge instead. After
every merge: `exe_tables.py check`, `ledger_check.py`, `exe_scan.py
--accepted`, the registry and switches regenerated, the build, and the
whole hosted shadow suite from an ini-less copy (`eg1031\gate\`): exit 0, no
mismatch, after AC + AU and after PL; JA's and ST's are the end batch's.
The reports: `C:\Users\kerok\bof3x-runs\eg1031\<group>\REPORT.md`.

**AC (merged `9ed06325`).** `recipes/exe-gaps-accepted.tsv`, one row per
`gap` row with `answer` (layer / port-own / unread / pending) and
`evidence`; `tools/exe_scan.py --accepted` fails on a gap with no row, a
row with no gap, a duplicate, a bad answer or an empty evidence, and runs in
CI (`checks.yml`); census section 8 rewritten as the rows today, the
2026-10-18 statuses kept whole below it. 258 rows at its tip; **221 after
PL's placements** (100 / 24 / 125 / 9 by answer before; after PL and JA:
91 layer, 0 port-own, 121 unread, 9 pending). It found section 1's two
errors (35 roll ranges, not 37; the music-player label rows are the census's
`CommuMusic_CloseSteps` rows and still the owner's question), that "the
PC's numbers" had no supplier on a Western-PSX-only install, and the
Identify words' probable read-while-zero on the Japanese and PSP builds.

**AU (merged `136ddd03`, numbered `6b43bb45`).** DIV-0192: `base/bgm/166.DAT`
written for every disc cache from sub-song 2 of `BGMBAT00.EMI` through its
bank (D320 fixed; a PC install with a disc cache now plays the disc's loss
tune, `music=pc` the MP3). DIV-0193 `SaveSlotBlank`: an empty save slot
draws no text on every install, as the US disc; `BOF3X_OFF=SaveSlotBlank`
the port's string. DIV-0194: a European disc-only install takes its disc's
own eight area banks through the new `recipes/pal-banks.toml` (written by
`importer.py pal-banks` from the three European discs, identical on all
three; D257 fixed; 741 containers, area 112 loaded from a crafted save with
the PAL bank's hash). The review notes: the Config and `yes_no_layout`
hosted checks compare the fields the readers use, `BOF3X_EXEIMAGE` read
wide, `bss_numbers`' docstring, an `under_jmp` log line; `fmv_install`
already DIV-0099's, `check_relaid` tooling. Shadow suite 1,325 s, exit 0.

**PL (merged `7dd4eb4a`, numbered `1e735bba`).** Placed:
`CommuMusic_LabelC+0x4C` on psx-jp / psp-jp / psp-eu (a new `imm_strings`
piece rule: the PlayStation lays the three glyph strings in the other
order); `Area121_BandHeights+0x1` on the PSPs (the section-tail floor);
`LinkedObjectD_Amounts` on psp-eu (a `BOOT.BIN` twin read at its first
byte); the six Western u16 fields as `regional` twins on the four Western
PSX builds (**DIV-0195**, section 7: area 135's three panel pieces 56 px
right of the PC's, kind 0x5E's frame 1 px narrower, its menu box 84 px
right and 158 px narrower, 60 / 118 on FR). The Identify words
(`TornadoMote_Phases+0x10 / +0x15`) are the engine's own
(`exe_own.cpp IdentifyWords`, DIV-0101 amended): AC's psp-eu worry did
not hold (its layer draws all four words), but psp-jp drew no EXP label
and now does. Not placed, with the reason: the West's own `CommuMusic`
text (pending / layer), `ConfigScreen_OpenStates+0x144` (the PC keyboard's
default key names, `port-platform`, accounted unread),
`Char_DefaultRecords+0x47C` (the layer's), `WardMote_Tilts+0x8` (the
PlayStation's table is followed by zero padding too: unread). Gap rows
47 / 48 / 51 / 51 / 20 / 21 / 20 to **40 / 41 / 44 / 44 / 18 / 18 / 16**
(US, EU-en, FR, DE, JP, PSP-JP, PSP-EU); `data_diff.py run` 0 errors.

**JA (merged `6eb88ea9`, folded `c3ece5bb`).** `loc_build.py convert_exe_ja`
reads the Japan disc's own tables where each module's code reads them
(`BOOT.BIN` holds 180 of the 188 words for PSP-JP; the 8 it lacks are the
Config rows the PSP's own Config screen has not): kind 7 (6 labels, 17
options, 6 controller names), 8 (22 verbs), 9 (7 commands), 10 (8 names;
character 2 takes a pair code, which reaches the save), 11, 12 (12 battle
messages), 15 (24 groups; the second status word left as shipped, 9 bytes
for an 8-byte slot). DLL: group 8 and group 20 (kind 0x5E's title) in
buffers of ours, a new group 26 (fight 26's two turn words), the
BattleExtra name copy whole beyond five bytes. Nine amendments (DIV-0015 /
0018 / 0019 / 0020 / 0052 / 0057 / 0064 / 0118 / 0120), no new number.
psp-jp's Identify panel takes group 15 from the UMD's `MAGIC060.EMI` as
PSP-EU's does. L1 / R1 keep DIV-0051's cells. Latin overlays rebuilt
byte-identical. **Seen by the owner on the four sheets, 2026-10-31
afternoon: "those all look great"**; then, on the Config labels past the
panel, the owner's rule the same afternoon, **DIV-0196** (`ConfigLabelJapanese`):
the label column left-aligned at the row box's own x under a Japanese
layer (two glyphs in, then one, then none, by three looks at the headless
captures: the longest label's large text ran off the right at the first
two); the `sub` of the width at both paths made nops and the anchor 0,
ours reading the operand (`field_c1.cpp`), the `field_c1` shadow rows at
0 mismatches under `ja-JP`. The pair code in a saved name stays a note.

**ST (merged `3c00b19b`, folded `2b27a3c0`).** `write_exe` writes an
`accepted = [[answer, lo, hi], ...]` list into the image's `data.toml` from
the accepted recipe (the range rows unchanged: five readers unpack four
fields); the engine, on a disc-built image, logs an answered gap with its
answer and refuses an unanswered one or an image with no list ("built by
an older release ... rebuild the install"), `BOF3X_EXEIMAGE_STRICT=0` runs
them, `=1` keeps refusing every gap (DIV-0101's `Switch:` value, which the
presets send), another value a Fatal; `exe_image STRICT self-test` in the
exe host (8 checks on a synthetic list, 9 with an image mapped: the planted
copy; ST's report said 10, its logs 9); the
launcher's stale check names the `exe` step for an older install. Seven
installs start under the default (40 / 41 / 44 / 44 / 18 / 18 / 16
accepted, 0 not), an older image is refused, a planted gap is refused by
name, the attract on psx-us runs to its end. The preset reference tables
had no rows for DIV-0192..0195 (stale since AU): regenerated, the
as-shipped count 79. **Decisions taken:** no `off=` name for the switch
(a rebuild is the player's way out; `=0` is a developer's); every existing
disc-built install - the harness roots and the owner's - is rebuilt by the
launcher's stale prompt or `importer.py build`.

## 5. Order and method

Stage A's four in parallel off the round branch's tip; merge **PL** first
(it changes the gap list), regenerate the recipes and run `data_diff.py`,
then **AC** (its check against the regenerated list prunes the rows PL
placed), then **JA** and **AU** in either order. After every merge: the
build, `exe_tables.py check`, `BOF3X_SHADOW='*'` headless (the gate that
missed a static-init fault once skipped it, HANDOFF traps), `exe_scan.py`
over the seven discs. Then **ST** alone, then the end batch. The method is
the install-gap round's: agents in worktrees, `git reset --hard` onto the
round tip first, one merge at a time, the coordinator numbers.

## 6. Done when

- `exe-gaps-accepted.tsv` covers every `gap` row of the regenerated
  `exe-gaps.tsv` with an answer and a pointer, no row NEEDS FIX, and
  `exe_scan.py --accepted` is a CI check;
- the read watch over the routes and the attract on all seven installs reads
  no `gap` range while zero, or every range it reads is `pending` with the
  owner's pointer;
- STRICT on by default for disc-built images, all seven installs starting
  under it, the planted gap refused (the negative control), the ten routes
  hash-equal to their `cn_<route>_orig4` pairs on the PC install;
- psx-jp and psp-jp draw Japanese where they drew nothing (JA), the loss
  tune plays on a disc-only install (AU, by the file; the ear is the owner's).

## 7. For the owner

**Answered 2026-10-31 morning**, before the launch:

- **The empty save slot** (`0x669CCC`): "blank it everywhere for
  consistency" - the PC install draws an empty slot as the US disc does,
  nothing; group AU's item (c), a ledger entry. The name-entry questions of
  [`name-entry-restoration.md`](name-entry-restoration.md) 8 stay with the
  localisation PR.
- **The Western u16 fields** of `EffectKind66_States+0x18/+0x20/+0x28` and
  `Rest3e_Windows+0x14/+0x18/+0x1C`: the owner asked whether the PSP keeps
  the Western layout. **Measured 2026-10-31** (five images built in memory
  by `exe_tables.build_from` from `D:\bof3x-discs\paths.json`, the six
  fields compared with `BOF3.exe`'s `.data`, relations only): the PC,
  psx-jp, psp-jp **and psp-eu** hold the same six values; psx-us and
  psx-eu-en leave them zero (the Western PSX discs hold other numbers
  there, `install-gap-twins.md` 2). Capcom's PSP Western release kept the
  Japanese numbers, so the PC's are consistent with it: **the PC's** - the
  rows are accounted **port-own** with this measurement as the evidence,
  no twin placed. (The owner's standing answer, reversible by a `twin` row
  if the Western PSX layout is ever wanted.)
- **D257, the PAL banks**: "let's take the pal banks" - group AU's item (d),
  the importer accepts the eight sample-swapped banks on a European
  disc-only install, a ledger entry, D257 fixed.
- **D280's layer**: "let's do it now" - JA runs in stage A.

**Found by AC at the accounting (2026-10-31 midday), the coordinator's calls:**

- **"The PC's numbers" has no supplier on a Western-PSX-only install**: no
  engine function fills the six u16 fields, the PSP discs are not in such
  an install, and no PC bytes go into code - so today the fields are zero
  there. The coordinator's call: **the Western disc's own values on the four
  Western PSX builds** (PL, a twin per field, a ledger entry as a regional
  value, DIV-0195); an install with a PSP disc follows the recipe's
  precedence. **Confirmed by the owner, 2026-10-31 afternoon**: "using the
  PSX bytes for a psx-only build instead of saving hardcoded values is
  fine". This is the "disc's numbers" option of the
  original question, chosen because the only alternative without the PC's
  bytes is zero.
- **The Identify panel's words on psx-jp, psp-jp and psp-eu**: those discs'
  `MAGIC060.EMI` holds no Identify words, so label group 15's slots 2..5 fall
  back to the `TornadoMote_Phases` range, a gap there: the panel probably
  draws no `WEAK` / `EXP` / `ITEM` / `POINT`. PL verifies and fixes it as
  D278's marks were fixed (the engine's own word when the slot is empty).
- **Nine `pending` rows remain**: the FR / DE music-player labels
  (`CommuMusic_LabelC+0xC / +0x24 / +0x2E / +0x4A`), the owner's open
  question of 2026-10-18 ([`install-gap-labels.md`](install-gap-labels.md)
  4.4: a raw-byte label group after a capture of the texture page on each
  install kind). ST's narrowed STRICT accepts a pending row.

**Still the owner's**, after the merge: a listen to the loss tune (AU, D320)
and the PAL banks on a European disc-only install; the eye on DIV-0195's
regional boxes on a Western PSX-only install; the word on a fresh
`cn_<route>_orig4` re-record with the current saves (section 9a item 4);
the nine pending rows (the FR / DE music-player labels). JA's sheets and
DIV-0196 were seen and approved the same day.

## 8. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/loc_build.py` (the `ja-JP` kinds), `src/game/labels.cpp`, `src/game/config_text.cpp`, the readers JA's kinds feed | JA |
| `tools/exe_tables.py` (placement, `immref`), `tools/exe_own.py`, `tools/psp_exe.py`, `symbols.toml` `psx` fields | PL (ST edits the image writer after PL merged) |
| `recipes/exe-gaps-accepted.tsv`, `tools/exe_scan.py`, `docs/install-gap-census.md` 8 | AC |
| `tools/importer.py` (song 166), the music tools, the hosted checks of `src/hook` named in AU | AU |
| `src/hook/exe_image.cpp`, `src/launcher` (the strict message, `off=`), `src/launcher/ledger_registry.h` regenerated | ST |
| `recipes/exe*.tsv`, `recipes/exe.toml`, `recipes/exe-psp.tsv`, the data-diff recipes | the coordinator only, regenerated after each merge |
| `docs/DIVERGENCE.md`, `docs/known-defects.md` numbers | the coordinator at the merges |
| `docs/README.md` rows | each group adds its own; keep both sides at a merge |

Merge order: PL, AC, JA, AU, then ST. Numbering: groups draft lettered
ledger entries and describe defects without numbers; the coordinator numbers
from the first free after DIV-0191 and D398 (2026-10-31).
Every commit `git commit -s`; no game data in any file, report or commit
message (rule 1): addresses, symbol names, counts and file names only.

## 9. The end batch (the coordinator; live, unattended)

From a harness root under `C:\Users\kerok\bof3x-runs\eg1031\` (the seven
disc builds of `D:\bof3x-runs\paths.json`, `abc_run.py prepare --set`, a
launcher copy `L\` with the round's DLL; the machine hands-off, the game
pauses on focus loss):

1. The self-tests headless on the exe host and the hosted shadow suite
   (`BOF3X_SELFTEST_ONLY=1`, `BOF3X_SHADOW='*'`, from an ini-less launcher
   dir), with STRICT's default on; the planted-gap control.
2. The read watch ([`install-gap-reads.md`](install-gap-reads.md) 1-2):
   `BOF3X_GAPWATCH=<root>\gw`, `BOF3X_GAPWATCH_CLASSES=gap`, the attract and
   the ten routes on all seven installs, psp-jp's eleven now that D321 is
   fixed; the owner's newer recordings as routes where they have a save. The
   file read against the accepted list: a `gap` read while zero that is not
   `pending` is a finding for a fix before the default stays on.
3. The A/B/C attract and routes on the seven installs under STRICT by default
   (every install must start), the four stale Chinese pairs re-recorded
   (DIV-0092).
4. The state hash: the ten routes on the PC install against
   `cn_<route>_orig4` / `_orig4b` (`BOF3X_OPT=none`, the skip list), equal.
5. Captures for the owner: JA's screens on psx-jp and the PC under `ja-JP`,
   the loss tune's log line on a disc-only install.

### 9a. As run (2026-10-31 afternoon and evening, an Opus agent at the round tip `2b27a3c0`; `end\REPORT.md`)

**Every step passed; nothing needed a fix before the default stayed on.**
Root `C:\Users\kerok\bof3x-runs\eg1031\end\`, the seven disc-only installs
rebuilt with this tip's importer (1,859 s), the PC install a copy of the
tooling install with junctions, the launcher copy `L\` ini-less, one game at
a time, the chain waiting for the coordinator's gate before it started.

1. **Self-tests.** The exe host on the PC install: the STRICT self-test 8
   checks right; on all seven disc-only installs 9 (the planted copy of the
   mapped `data.toml` with one answer taken out refused and the range named),
   0 FATAL, and every install "N accepted, 0 not" at 18 / 40 / 41 / 44 / 44 /
   18 / 16 (JP, US, EU-en, FR, DE, PSP-JP, PSP-EU). The hosted shadow suite
   on the PC install: exit 0 in 1,173 s, every mismatch line 0, 10,087 ours.
2. **The seven installs under the default.** The attract and the ten routes
   on each, 77 runs, every one to `recipe done` with exit 0 and no FATAL,
   393,120 latches watched under the read watch. 76 of 77 area sequences
   follow psx-us; the exception psx-fr `worldMapAndAreaTransition` is the
   known one (install-gap-reads.md 9.1), and psp-jp `combat` and
   `dragonTransform` now follow the US sequence where 9.1 had them part.
3. **The read watch:** 0 of 60 ranges read in any run. The only range
   written is `Char_DefaultRecords` by the layer, read by nothing. Two edge
   hits were checked and are accesses that end before the range
   (`EffectKind0F_DrawToggles`' pointer reads below `0x66A094`,
   `CharNames_Apply`'s writes below `0x669CE8`). The nine `pending` rows were
   not exercised: no route opens the faerie music player.
4. **The state hash on the PC install:** the ten routes under the pool-fold
   proof's conditions. Against `cn_<route>_orig4` every route differs on the
   standing signature, not the round (`0x5FD000` from tick 0 is DIV-0176
   SunderLoop; the save-list pages are the owner's slot 6 saved after orig4;
   the DIV-0092 and DIV-0154 pages are ledgered); against the pool-fold
   proof's own 2026-10-29 runs three routes are identical on every tick and
   the other seven differ only on the known noise pages (sound, the VRAM
   shadow, two windows, the D3D page), the two widest re-run with the windows
   moved or gone. **No PC-install difference on any game page.** A fresh
   orig4 re-record with the current saves would make "equal" literal (an
   item for the owner, section 7).
5. **Captures:** the lost fight on psx-eu-en logs `DIV-0087 track 166 from
   the cache: base\bgm\166.DAT, bank BGMBAT00, looping`
   (`end\runs\cap_gameover_r2\psx-eu-en\`); the PC install's load list draws
   no text in its empty slots under `SaveSlotBlank` (`end\s5\slot_pc\load_list.png`).

Findings, none needing a fix: `gapwatch.py`'s `edge` counter counts any
access starting up to 7 bytes below a range whatever its width (a tooling
note); ST's report's "10 checks" is 9. Not run: the four stale Chinese pairs
of DIV-0092; the owner's newer recordings as extra routes; A/B/C picture
compares (no frames at `--every 100000`). Durations: the routes 3 h 57 min,
the hash 42 min.

**After the batch:** DIV-0196 (the Config labels under `ja-JP`, section 4a
under JA) landed on the round branch with its own proof - the `field_c1`
and `config_text` shadow rows under `ja-JP` at 0 mismatches and the
`config_screen` captures on the three Japanese installs - and the owner's
approval of the captures; the batch's installs predate it, which touches no
disc-built image.

## 10. The brief, per group

Written 2026-10-31 under `C:\Users\kerok\bof3x-runs\eg1031\`: `brief_common.md`
(the rules, the worktree, the report), then `brief_JA.md`, `brief_PL.md`,
`brief_AC.md`, `brief_AU.md`, `brief_ST.md`; `launch.sh` makes the round
branch's worktrees (`wt_<group>`) and harness folders, `merge.sh <group>`
is the coordinator's merge with the regeneration chain and the gate after it.
Each brief says, in its own words: the goal; the rows by symbol and range
(never the bytes); the files it owns and must not touch; the docs to read
first; its harness root; the checks that mean done; the report's shape. A
usage cut resumes with a message naming the next step; only the hand-back
report means merge.

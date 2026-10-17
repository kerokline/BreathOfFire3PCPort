# The button unification round: staging

**Status:** DRAFT (staged 2026-10-17 against `origin/main` `8382691d`; nothing
launched, nothing built beyond issue #78's D307.)

The design is [`key-icons-by-mapping.md`](key-icons-by-mapping.md) 5 as
re-read in [`button-roles-by-release.md`](button-roles-by-release.md) 7,
with the owner's three answers of 2026-10-17 (that doc, 8). This document
stages the build as parallel groups whose acceptance evidence is
**screenshots**: every site is a draw, and the test for each is a capture
with the Config untouched (nothing may change) and a capture with the Config
swapped (every icon shows the swapped button, and the swapped button is the
one that works).

## 0. What the round builds, in one paragraph

One permutation, `σ`, read from the save's six Config words against the
install's release defaults: for each line, its default button -> its button
now. Then:

1. **every icon** is drawn as `σ(button)`, the button taken in Japan's terms
   (an icon from release R's text or art is `M_R^-1(b)` first: `P` for the
   PlayStation West, `S` for PSP-EU, identity for JP, PSP-JP and the PC);
2. **every hard-coded test** of Japan's bit `b` tests `σ(b)`: the fishing's
   stage 1, the minigames (the rope pull, the chop, the well, the antenna,
   the ram, the crane), the dock's parts menu, the name entry's;
3. **confirm and cancel follow `σ`** (the owner's answer 1): menus confirm on
   whatever Speak is on and cancel on whatever Action is on, so words 7 and 8
   are no longer a third flow;
4. **the starting Config is the source disc's** (answer 2): an install starts
   with the defaults of the release it was built from, JP first, then the
   PlayStation West, then the PSP, when several sources are present;
5. **PSP-EU's two honest texts** (answer 3): the well (area 68) and the ram
   (area 121) translate by `P`, the rest of PSP-EU's by `S`.

With `σ` the identity, nothing a player sees or presses changes. That is the
regression check, and it is why the whole round can be verified without an
owner at the keyboard. PlayStation shapes only: no keyboard key names, no
Xbox or Nintendo letters, no physical-layer glyphs (section 6).

## 1. Why screenshots verify it

Every site in [`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2 is a
draw on a screen a recipe reaches or can reach, and every test in
[`button-roles-by-release.md`](button-roles-by-release.md) 3 is a press a
recipe makes. So one instrument serves every group: a recipe that opens the
Config, **swaps Speak with Action and View with Change**, saves, and then
visits a site. Two runs per site per install kind:

| Run | Config | Expected |
|---|---|---|
| identity | untouched | every frame equal to the pre-round reference (`statehash.py check` on the hash routes; the attract's 55 shots equal) |
| swapped | Speak <-> Action, View <-> Change | the icon at the site is the swapped button's; the press the recipe makes with the swapped button has the original's effect (the next frames equal the identity run's from the press on) |

The owner judges the swapped captures by eye from the app
(contact sheets, one per site and install kind), and answers the open
questions of section 5 the same way. Group RC builds the instrument first
and alone (section 2), so every other group's hand-back can cite its
captures.

## 2. Stage A (three groups, in parallel)

Each group works in its own worktree off `main`, commits with `git commit -s`,
self-tests headless, and ends with a hand-back report. Harness roots are per
group (`C:\Users\kerok\b_<group>`), never shared (two runners in one root
kill each other's games, Traps).

### SG: the core, `σ` and the defaults (`src/input/`, `src/game/shell.cpp`; merges first)

- **`σ` as a table.** A new `src/input/sigma.{h,cpp}`: `Sigma(bit)` and
  `SigmaInverse(bit)` over the six face and shoulder bits, rebuilt whenever
  a Config word changes (the rebind at `0x461459..0x46153B`, `LoadMenu_Apply`
  `0x58860E..`, the boot defaults `0x496C25`; all three are ours). The
  defaults it is read against are the install's release's
  ([`button-roles-by-release.md`](button-roles-by-release.md) 2: JP, `P`,
  `S`), which the cache's `manifest.toml` already names by source.
- **The starting Config** (answer 2): `Shell_MapDefaultKeys`'s rule in
  `shell.cpp` and the boot defaults take the release's nine words, JP then
  the PlayStation West then the PSP when several sources are present. The
  reading of answer 2 that a PC install with a Western language layer starts
  with Japan's buttons is the one built; it is also section 5's first
  question to the owner.
- **Words 7 and 8** (answer 1): written as the Speak line's word and the
  Action line's word at the three writers above. No reader of
  `Field_ConfirmButtons` changes (16 in `battle_e4.cpp`, 11 in `rest_2d.cpp`,
  8 in `battle_menu_states.cpp`, 7 in `rest_4f.cpp`, the rest), because the
  words they read now carry `σ`.
- **Self-test** (`BOF3X_SHADOW`): with every release's defaults, `σ` is the
  identity and words 7 / 8 equal the originals'; a swap gives a permutation
  (closure, inverse); a hand-edited save with a non-permutation (two lines on
  one button) is refused loudly, not guessed at (rule 4).
- **Ledger draft A** (amends DIV-0051's icon column and names words 7 / 8
  as Speak's and Action's) and **draft B** (the starting Config by source).
- Output: `docs/button-sigma.md` (the table, the three writers, the
  defaults by source, the self-test).

### RC: the routes and the reference captures (`tools/recipes/`, no `src/`)

- `tools/recipes/sigma_swap.txt`: from the title, open the Config, swap
  Speak <-> Action and View <-> Change, save to a slot the recipes do not
  use (slot 6 is `balioAndSunder_2`'s; pick another and say which), exit.
  Built with `tools/recipe_shots.py` for every shot line (never by hand: a
  hand-inserted shot shifts every later input a frame).
- One site route per row of section 3, each in an identity and a swapped
  variant (`--save` the swapped slot's save): the world map's legend
  (`worldMapAndAreaTransition`, `caughFish` area 16), the fishing banner and
  stage 1's presses (`caughFish` area 30), the Config panel
  (`config_controller`), the item lists' corner (`fairyVillage`, `shop`,
  `menu_screens`), and **the sites with no route today**: the dock (area
  128), the rope pull (area 75), the chop (area 8), the well (68), the
  antenna (111), the ram (121), the crane (135), the name entry. Each by a
  chapter-jump save (the chapter byte, state 0 and the `+0x70` checksum,
  as `ending17.txt` reached the dune) and a short walk; a `# save` header
  on every recipe (the harness refuses one without).
- The references: every route run once on each install kind, identity
  Config, before any other group merges: PC (`bof3`), a Western PSX
  disc-only install (US), a PSP-EU-only install, and the PC with the `en-US`
  layer. Frames to a scratch folder outside the tree; the state hash
  recorded for the hash routes.
- Output: the recipes, `tools/sigma_sheets.py` (a contact sheet per site:
  identity beside swapped, one row per install kind), and
  `docs/button-routes.md` (which frame shows which icon, and the press each
  route makes).

### DT-1: the dialogue's icons as button codes, the build half (`tools/loc_build.py`)

The largest piece and the only one that fixes a defect present today under
every Western layer (the text names the US buttons over the PC's code,
key-icons 9). Split in two so its build half runs in stage A:

- A text icon becomes a **button code** in the layer's output: a control
  that names Japan's bit, not a glyph. PC Chinese by its colour cue (circled
  1 in colour 25 = circle, `X` in colour 4 = cross, circled 3 in colour 1 =
  circle in area 1 and square in area 121, the census of key-icons 2.6); the
  US / EU / FR / DE icon glyphs `0x80..0x83` by `P`; PSP-EU's by `S` except
  areas 68 and 121 by `P` (answer 3).
- The areas: 8, 30, 89, 129, 68, 75, 111, 121, 135, and the PC's area 1.
  Not buttons: area 78's crosses, the faerie village's times sign.
- `loc_build.py` writes the control; the draw half (DT-2) resolves it. Until
  DT-2 merges the control must still draw *something* honest: DT-1 emits the
  release's own glyph beside the code so an unresolved control draws as
  today, and DT-2 removes that. No stub.
- Check: `loc_build.py all --discs`; a dump of each area's text with the
  codes counted (31 PSX mentions, 32 under `en-US`, the PSP-EU set) against
  the census; `importer.py check`.
- Output: `docs/button-dialogue.md` 1-3.

## 3. Stage B (after SG merges; six groups in parallel, by file)

Every group below asks `Sigma()` and nothing else of SG. Captures through
RC's routes are each group's acceptance evidence.

| Group | Site | What changes | Files | Route (RC) |
|---|---|---|---|---|
| **IC-W** | the world map's legend | the eleven `*_DrawFrame` copies draw the key glyph for `σ(word 0)` / `σ(word 6)` through one helper; the PC's port art (the `SPACE` / `X` / `ENTER` labels) replaced by the disc's words with a live glyph where a disc is present, and by our own label text on a PC-only install (section 5, question 2); D307's repaint of a disc's baked icons stays | `world_map.cpp`, the `area_w*.cpp` frame copies, `tools/importer.py` (the dial page) | world map |
| **IC-F** | the fishing banner's labels | the kind-0xF line records' label byte and `FishingText_Label` resolve through `σ`: stage 1's lines by their constants, the cast's confirm by word 7 | `fishing_text.cpp`, the kind-0xF line states | fishing |
| **IC-D** | the dock's parts-menu legend | the three baked icons on the area page blanked (the importer, as D307) and `σ(circle / cross / triangle)` drawn at their places in `EffectKind5E_DrawPanel`; on a Western disc the page's `P` icons likewise | `rest_3e.cpp`, `tools/importer.py` (area 128's page) | dock |
| **IC-L** | the item lists' L1 / R1 corner | pieces `0x15`, `0x18`, `0x2E`, `0x33` drawn without their baked label and the glyph for `σ(L1)` / `σ(R1)` drawn in `Menu_DrawPiece`, one place for the ten lists; the PSP page's `L` / `R` tiles then only art | `menu_windows` (the piece draw), `tools/importer.py` (the UI page) | item lists |
| **HT** | the hard-coded tests | first, the input census regenerated (`input_census.py EXE pc_funcs.json symbols.toml OUT`, key-icons 1; the 2026-10-13 `input_census.tsv` is not in `analysis/`, so this run commits it there); then every face or shoulder constant test in the list, `Input_Pressed & b` to `& Sigma(b)` (`0x74`'s four bits one by one), in: the fishing (`LeaderPanel_S1Buttons` `0x528A90`, the reel and catch `0x20` / `0x40` / `0x60` / `0xC0`), the rope pull (`area_w1f`), the dock (`EffectKind5E_Pick` `0x47F610`), the chop, the well, the antenna, the ram, the crane, the name entry, the item lists' `0x500C` / `0xF00C`; HT splits itself into two worktrees by file family once the census is in (field minigames and fishing; everything else) if the count asks for it | the files the census names; never a file an IC group owns | every site route's press |
| **DT-2** | the dialogue's draw half | `MsgBox_Step` / `Text_DrawString` resolve DT-1's button code to the glyph of `σ(bit)` at draw time and drop the fallback glyph; a glyph wider than the one it replaces re-flows the line as the layer's other widths do | `msgbox.cpp`, `text_draw.cpp`, `loc_build.py` (DT-1's fallback removed) | the nine areas |

The Config panel itself (`Config_DrawControllerCell`) needs no group: it
draws the word's own button, which under `σ` is already the truth. RC's
`config_controller` route proves it in both runs.

Each group's self-test: with `σ` the identity the site's frames equal the
reference (a fuzz with stand-in words for the draw helpers; the headless
`BOF3X_SHADOW` run for the tests); with a stand-in swap, the expected glyph
or mask. Each group's hand-back cites its RC captures, identity and swapped,
on the four install kinds.

## 4. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `src/input/sigma.*`, `shell.cpp`'s defaults, the three Config-word writers | SG |
| `tools/recipes/sigma_*.txt`, `tools/sigma_sheets.py`, the chapter-jump saves | RC |
| `tools/loc_build.py` | DT-1, then DT-2 (one after the other, never at once) |
| `tools/importer.py` page repaints | one commit per page, IC-W (the dial page), IC-D (area 128's page), IC-L (the UI page); merged in that order, each rebased on the last |
| `world_map.cpp`, `area_w*.cpp` frame draws | IC-W |
| `fishing_text.cpp`, the kind-0xF line states | IC-F |
| `rest_3e.cpp` | IC-D |
| the piece draw | IC-L |
| every file with a hard-coded test | HT; where such a file is also an IC group's (the fishing module, `rest_3e.cpp`), HT's change lands after that group's, rebased |
| `msgbox.cpp`, `text_draw.cpp` | DT-2 |
| `docs/README.md` rows | each group its own; keep both sides |

Merge order: **SG** first (everything asks it), RC and DT-1 in either order;
then IC-W, IC-F, IC-D, IC-L, DT-2, HT, with HT last since it touches the
most files. After every merge: the build, `BOF3X_SHADOW='*'` headless, and
RC's identity routes against the references.

Numbering: drafts lettered by group; the coordinator numbers at the merges
from the first free entry (**`DIV-0131` is taken** by PR #81; the
fishing-panel duck in flight on 2026-10-17 must be renumbered before it
lands). The drafts expected: A (SG: `σ`, amends DIV-0051; words 7 / 8),
B (SG: the starting Config by source), C (IC-W: the legend from `σ`, the
port's keyboard art replaced; amends DIV-0130), D (IC-F: amends DIV-0069),
E (IC-D), F (IC-L), G (HT: the hard-coded tests through `σ`), H (DT: the
dialogue's button codes; amends the DIV-0005 family). Defects from the
first free after D311. Every commit `git commit -s`; no dialogue line
quoted in any doc, the scene described instead (rule 1).

## 5. What the owner answers by chat, with a capture beside each

1. **A PC install with a Western layer starts with Japan's buttons** (the
   reading of answer 2 in [`button-roles-by-release.md`](button-roles-by-release.md) 8).
   Confirm, or the layer's release counts as a source. SG builds the
   reading; a one-line change either way.
2. **The world map's labels on a PC-only install**: the port's `SPACE` /
   `X` / `ENTER` art goes (PlayStation icons only); what stands in its
   place is our own label text (Talk / Act / Camp, in the layer's words) or
   nothing but the glyphs. IC-W captures both at the identity Config.
3. **The item lists' corner on the PSP page**: the PSP's `L` / `R` tiles
   stay as art under the glyph, or the glyph alone. IC-L captures both.
4. **PSP-EU's code at the well and the ram** (not read): HT reads it; if the
   code follows `S` and the text `P`, the text is wrong on the PSP too, and
   the owner says whether DT keeps answer 3 or follows the code.
5. **The swapped-Config contact sheets**, one per site and install kind: the
   round's acceptance is the owner's "yes" on each, from the app.

## 6. Not in this round

- Keyboard key names, Xbox and Nintendo letters, the last-device flag and
  the physical layer's glyphs (key-icons 5's `GlyphFor` beyond the
  PlayStation shapes): a later round; the owner asked for PlayStation icons
  only.
- Option B, a "US button layout" launcher preset: with `σ` and the starting
  Config by source it is a preset of the existing Controls dialog at most, and
  a ledger line only if it becomes a default.
- The battle command cross, `Menu_ButtonSets`, the F9 lines: not sites
  (key-icons 2.7).
- The name entry's legend: cut by the port; its restoration is IDEAS I34 and
  the staged localisation PR, where HT's name-entry tests then apply.

## 7. The brief, per group

Each brief is written from this document and names: the goal; the files it
owns and must not touch; the docs to read first
([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2 and 5,
[`button-roles-by-release.md`](button-roles-by-release.md) 1, 2, 7 and 8,
[`controls.md`](controls.md) 2, [`world-map-hud.md`](world-map-hud.md) 1.2
for IC-W, [`fishing-text.md`](fishing-text.md) 2 for IC-F,
[`rest_3e.md`](rest_3e.md) for IC-D, [`input-script.md`](input-script.md)
and the chapter-jump method for RC); its harness root; the RC routes that
are its evidence; the checks that mean done; the hand-back report's shape
(what was built, the captures' paths outside the tree, the drafts lettered,
what was not reached). Rules 1, 4 and 7 are restated in every brief. A usage
cut resumes with a message naming the next step; only the hand-back report
means merge.

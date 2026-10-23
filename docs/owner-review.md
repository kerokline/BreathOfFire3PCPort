# Owner review list - things that want the owner's eye or ear

**Status:** IN PROGRESS (2026-10-06; rewritten through 2026-10-22, the reading-row round's section at the top) - started the morning after the platform
round merged (PR #42), from the calls [`platform-round.md`](platform-round.md)
section 4 and [`round-14-cleanup.md`](round-14-cleanup.md) section 5
gathered. One list, so the questions only the owner can settle in play are
not scattered across round docs. **Rewrite, do not append**: an item leaves
when the owner has looked and said, and its answer goes where it belongs
(the ledger, the defect list, the round doc). Agents add here; the owner
strikes.

Each item says what to do at the machine, what to look or listen for, and
where the reading behind it is. Nothing here is a divergence until it is in
[`DIVERGENCE.md`](DIVERGENCE.md).

## 2026-10-22, the reading-row round (built)

Built on `localization/second-script` ([`reading-row-round.md`](reading-row-round.md);
its section 6 holds the calls; the contract is [`reading-row.md`](reading-row.md) 1).
Ledgered as DIV-0155 (the reading row, furigana), DIV-0156 (a second language)
and DIV-0157 (mods); the round's findings are known-defects D326..D330.

### Decided by the owner (2026-10-22, chat)

- **Call 2, furigana ships as the mod zip** (first "build it in", then
  "lets deliver it as a zip just so we can work on the first mod
  implementation in this round"): the staged shape stands. `data/furigana/`
  is the zip's source, `tools/make_mod.py furigana` builds it, `release.yml`
  attaches it, the Mods tab installs it, `setup` builds
  `ruby-furigana-ja-JP` from the installed mod. The point is the mod path
  proven end to end in this round.

### Still open (round doc section 6, and from stage A's hand-backs)

- **The opening scene shows no second script, by rule** (found 2026-10-22
  when the owner's first install showed nothing on a new game): the whelp
  scene's dialogue in AREA024 is one message of eight timed pages with the
  offset pen and an effect preset (`0x0D 0x0E 0x0F 0x16`), and the
  builders leave such a message plain **whole** (reading-row-second.md 2;
  134 effect messages and 409 non-box ones per pair). The next message on
  from the scene is composed. **Decided 2026-10-22 ("let's add an explicit
  stop to the text; it will make the new game not comparable to before, but
  it makes it better for study"):** under a reading-row layer a timed page
  (`0x16`) becomes an explicit confirm stop in the composed copy, and the
  offset-pen and effect pages are composed too; the original message is
  untouched (the side chunk), so with no layer nothing changes. Group OS,
  launched the same day.
- **Pages are re-flowed, rows never** (owner, 2026-10-22, off a capture of
  the opening where the re-break ran the name row into the speech and the
  row reached the frame: "in the original script, the name gets its own
  row - just go ahead and do a page break instead of fitting it onto one
  page. That leads to more pages of text, but less re-flow of sentences"):
  the second-language mode keeps every authored row break like the
  furigana mode and sends a third text row to a new page; about 3,300 more
  presses over the Japanese script, no re-broken row. Group NR, launched
  the same day; D332 falls with it.
- **The second-script dropdown hidden by default** (owner, 2026-10-22):
  *Row above the text* leaves the Settings page unless a toggle shows it;
  the toggle sits on the Mods page behind a sub-row of two buttons under
  the top row's Mods button, *Data* (the mods list as today) and *Extras*
  (the toggle) - "just to reserve that slot on the top row"; it turns on by
  hand or on its own when the furigana mod is installed. Group LX, launched
  the same day.
- **A first install needs two steps** the launcher does not say yet
  (the saves now carry themselves: D331, `carry_saves` in `setup`, from
  the previous game folder and from the home folder's old layout): the
  furigana zip must be built
  (`tools/make_mod.py furigana`, `build
eleaseurigana-0.1.0.zip`) and
  added on the Mods tab, and `language=ja-JP` chosen for its entry.

- **From ML:** should a mod also build into a cache named by `cache=`, or
  only the home's own (as now)? Should the dropdown's furigana entry read
  "Japanese (Furigana)" plain or with "(mod 0.1.0)"? And the one that
  gates a release: **SudachiPy and its dictionary are not bundled** in the
  release's Python, so a player's `setup` with the furigana mod installed
  stops at the loud error until they are (about 70 MB more in the release,
  or the builder run on the owner's machine) - `tools/make_release.py`
  `NOT_BUNDLED` says so.
- **Decided 2026-10-22, a follow-on group after RR lands ("lets have that
  as a followup after RR"): the readings lexicon and the second opinion.**
  SudachiPy stays off the player's machine: the owner's machine runs the
  builder once and writes `data/furigana/lexicon.toml`, the sorted word
  list (dictionary form, reading) the builder met - a dictionary excerpt
  (SudachiDict, Apache 2.0), never ordered or keyed by slot, so no sentence
  can be told from it (the rule 1 reading the owner took in chat); the
  player's builder segments by longest match against it, and the owner's
  build records every place the two segmenters disagree as a `next` /
  `prev` rule in the sidecar. Beside it a `--compare` mode places readings
  with fugashi + UniDic as well and writes the disagreements by surface and
  context, most frequent first, for the owner to adjudicate into
  `readings.toml` (call 4's proofreading, made cheap). **And a round of
  agents** (owner, 2026-10-22): the review list sharded by area to
  subagents that give the kana of every token of every message as
  structured output, with a flag where the segmentation looks wrong; a
  script diffs them against Sudachi's into the disagreement list by
  surface and context; a second pass proposes the sidecar rule for each
  line and the owner approves. The script goes through the API; the owner
  is fine with that (the script is published in both languages already;
  the rule is about the repo carrying none of Capcom's material), and every
  derived file - shards, kana, diffs - stays in the harness root, only the
  sidecar rules and the lexicon in the tree. Measured already:
  Sudachi core against full differs in 253 of 27,976 messages, nearly all
  compound width, not readings; the power kanji's on-reading (3,857
  placements) is the pools' stat labels, the dialogue reads it the kun way
  in 240 of 245. Suzume (libraz) was looked at and set aside: it tokenises
  and tags but gives no readings, and ships no Windows wheel.
- **From FB:** 4,559 area pages split for a third row, each an extra
  confirm press, as accepted.
- **The agent pass ran (2026-10-22 afternoon,
  [`readings-audit.md`](readings-audit.md)); for the owner to decide:**
  the sheet `C:\Users\kerok\bof3x-runs\rr_audit\proposals.md` and the
  fragment `candidate_readings.toml` beside it - 178 candidate rules, each
  with the proposing agent's reason and its measured reach. (a) Approve
  the plain rules of the audit doc's 3.1 (他 ほか, 様 さま after a name,
  金 かね, 都 みやこ, 村 むら, 身体 からだ, 明日 あした, 開く あ-) into
  `readings.toml`. (b) The OVERREACH bin: 来る, 長, 実, 者, 戦 and both 方
  rules are wrong as plain rules and want `next` / `prev` lists or more
  surfaces; 石化 せっか, 前世 ぜんせ, 宝物 たからもの, 前金 まえきん are
  right and only look wide because the pools repeat them. (c) Thirteen
  owner calls: **賢樹** (builder たけき, agents けんじゅ; 21 placements),
  **外海** (そとうみ / がいかい; the builder has そとめ in 20 of 45),
  **海人**, 剣 as a lone label, 生命 as いのち, 盗人 ぬすびと / ぬすっと,
  魚 in the chant, 大星. (d) Whether the compounds Sudachi splits (大武会
  and 33 more, no sidecar rule can reach them) get a multi-token rule in
  the builder or a `--dict full` measurement first. **Decided 2026-10-23:
  大武会 = だいぶかい** (unattested in the setting book, the Taiwanese
  guide or the script; kan-on ブ as in 武道会; entered in `readings.toml`,
  effective with the multi-token rule). The setting book's kana index also
  gives ぬすっとこうもり for the bat, a data point for the 盗人 call -
  **taken: 盗人 = ぬすっと (2026-10-23)**, a plain rule over its 12 dialogue
  lines. **Then (2026-10-23, chat): 賢樹 けんじゅ, 外海 がいかい for all
  45, 大星 おおぼし, 魚 うお in the chant, 生命 いのち in the one threat, 剣
  stays けん** - all in `readings.toml`, each build-measured (audit doc 3.4).
  海人 かいじん closed the thirteen. Found on the way: `prev` cannot see across a
  row break, so a word that opens a row cannot be keyed on the row above
  (the chant's first 魚); a builder follow-on beside the multi-token rule.

- **1, decided 2026-10-22 off PR's sheets:** the gap is **even, "2px top
  2px bottom"** - a reading's ink 2 units from the text above it and 2
  units from its own text (PR's gap 2 was 4 above and 1 below; RR sets the
  pen y that gives 2 and 2 and shows it on a capture). The Latin reading
  row stays at **1/2**: "plenty legible", 2/3 "not worth it".
- **F4 hides and shows the second script** (owner, 2026-10-22, for
  language study): the ruby glyphs' emission only is suppressed, the
  re-flow and the extra space stay; the binding is optional
  (`BOF3X_RUBY_KEY`, default F4, `none` to free the key; `ruby_key=` in the
  ini at the merge) because the F keys are running out - F2, F3, F4 were
  the free ones.
- **The capture scene** for the end batch and the owner's play: the
  dolphin fight (`tools/recipes/dolphinBoss.txt`), "a large chunk of
  uninterrupted text".
- 3: a reading revealed with its word; 5: choice options plain; 6: every
  held pair built by `setup`, all in the dropdown; 7: SL's four as SB drafted
  them - taken as built ([`reading-row-round.md`](reading-row-round.md) 6).

### To play (owed)

Sheets, 4:3 and 16:9, point filter, in
`C:\Users\kerok\bof3x-runs\rr_rr\shots\sheets\` (never in the tree):
`furigana_*`, `study_key_F4_*`, `second_en-US_over_ja-JP_*`, `zoom_gap_*`,
`dolphin_furigana_*`. In the launcher: Language *Japanese*, *Row above the
text* the furigana entry (or English for the second language).

- **The new game's opening with stops** (`C:\Users\kerok\bof3x-runs\rr_os\sheets\opening_furigana.png`,
  `opening_en-US_over_ja-JP.png`): every timed box holds until a press; the
  stop is the plain end-of-message wait with no arrow - say if you want an
  arrow there (a renderer change). D332 is on the English-over-Japanese
  sheet: a speaker's name run into its speech, one row touching the frame.
- **The opening with rows kept** (`C:\Users\kerok\bof3x-runs\rr_nr\sheets\opening_en-US_over_ja-JP_rows_kept.png`):
  the name on its own row, three of the seven boxes now two pages. One
  thing NR noticed for your eye: on a two-row page (a name, then one line
  of speech) the balance rule can put a few English words above the name
  row; changing that is an amendment to the sharing rule, your call.
- **The Mods page's Data / Extras sub-row and the hidden dropdown**
  (`C:\Users\kerok\bof3x-runs\r_ruby\lx\shots\`): the toggle off by
  default, on when the furigana zip is added; Settings closes the gap.
- **The dolphin fight with furigana** (`dolphin_furigana_*.png`; your
  capture scene): the coast scene's long dialogue. Look at the gap - built as
  1.5 units of clear space over and under each reading, the nearest even
  spacing to "2 and 2" on the unit grid (`zoom_gap_*.png`; 2.5 and 2.5 is the
  other even choice, one table to change) - and at how a reading appears with
  its word. Played by recipe, `tools/recipes/dolphinBoss.txt` needs one more
  press under the layer (the Fairy Tear prompt's first message is a split
  page, D329); in your own play it is just one more confirm.
- **F4** (`study_key_F4_*.png`): hides the readings and shows them again, the
  layout unchanged; `ruby_key=` in `bof3x.ini` moves it (`f1`..`f12`) or
  frees it (`none`).
- **English over Japanese** (`second_en-US_over_ja-JP_*.png`): Language
  *Japanese*, *Row above the text* *English*; a speaker's name drawn small in
  the English row (`0x13`). Japanese over English is not captured: worth one
  look under Language *English*.
- **The Mods tab's install of the zip**: Add zip... with
  `furigana-<version>.zip` (`tools/make_mod.py furigana`), the row building
  the layer, then the entry in *Row above the text*. The build needs
  SudachiPy in the Python the launcher runs the tools with; the release's
  runtime does not carry it (the release gate above), so from a release
  folder the tab shows the loud error rather than a layer.
- Also seen on the sheets, for your eye: the nue save's party names drawn as
  wrong glyphs under Japanese (D326, the recipe save's, not the row's); the
  next-page arrow on a one-pair page's lower edge (D327, the original's
  rule); readings to proofread, for example a compound split as the
  dictionary splits it (D328).

## 2026-10-21, the wide-ring round

Built on `wide-ring/round` ([`wide-ring-round.md`](wide-ring-round.md); its
section 6 holds every call, items 1..12, all answered in chat on
2026-10-21). Nothing is left to decide; **what is owed is play** of each
change. The sheets are in `C:\Users\kerok\bof3x-runs\r_ring\<group>\sheets\`
(never in the tree).

### Decided by the owner (2026-10-21, built)

- **The ring** (DIV-0041's 2026-10-21 RG amendment, IDEAS I37,
  [`wide-ring.md`](wide-ring.md)): the PSP's 33 x 58 at 16:9 and 21:9,
  Capcom's 28 x 56 at 4:3; the inset kept; the rim draw-only, sprites in
  the grown rows hidden; D295's rule kept; the walk today's plus one; a
  quickload across widths rebuilds the rim, `MapView_Redraw` 2. D292 narrowed
  to kind 0x8D's first frames, the remainder accepted.
- **The two sweeps** (DIV-0041's SW amendment, IDEAS I38,
  [`wide-sweeps.md`](wide-sweeps.md)): the shade's right-anchored map and the
  banner's faster slide in the bands, accepted off the sheets.
- **The table faerie** (DIV-0151, D300, [`faerie-fix.md`](faerie-fix.md)):
  its look and the sides' second draw, accepted off the sheets.
- **The pillarbox** (DIV-0041's PB / PX amendment,
  [`narrow-pillarbox.md`](narrow-pillarbox.md) 10..11): only areas 31, 90 and
  198 framed; menus opened there stay framed; the camps, 98, 149 and 186
  unframed.
- **Area 194's first-visit crash** ([`area194-entry.md`](area194-entry.md)):
  the crafted save's (Capcom's `Leave` on an empty party, unreachable in
  play); no guard.
- **Garr over the camp's fire** (DIV-0071's third amendment, D301,
  [`garr-fire.md`](garr-fire.md)): area 192's camp; the flames over his arm
  and weapon as Capcom draws them; the wider reach accepted on the owner's
  PlayStation check.

### To play (owed)

- **The ring:** any field scene at 16:9 and 21:9 with the camera pulled out
  or turned over low ground (the side edges should not notch); a quicksave
  taken at one width and loaded at another (`BOF3X_WIDE_RING=0` for the
  2026-10-13 picture).
- **The sweeps:** the fishing minigame's *READY!* banner (`caughFish`) and the
  bad ending's *Fin* wipe (area 172) at 16:9 and 21:9.
- **The table faerie:** `dolphinBoss`, recipe frames about 960..2,850: the
  gold faerie whole over the table, no shadow on the plank's front.
- **The pillarbox, the three-area build:** areas 31 (the lift shaft), 90 (at
  21:9, framed at 426; its row tips and a menu opened there) and 198 at both
  wide widths; `BOF3X_PILLARBOX=0` for the full picture.
- **Garr at the fire:** the camps of area 53 (chapter 6) and 192: his arm
  and weapon under the flames, as on the PlayStation.

## 2026-10-21, docs/own-evidence-fixes

Three changes the owner asked for, built and not yet played:

- **DIV-0152, the pad reset.** In the field, hold Start and Select for a second.
  It should go back to the title, as on a Western PlayStation disc, and the log
  should show a `DIV-0152` line. The recipe `tools/recipes/field_padreset.txt`
  does this from `adult_ryu`; it was not run, because another session's chain
  held the game.
- **DIV-0153, the Holy Mantle in area 83** (D325). Walk area 83 with the Mantle
  on: fights should come rarely, about 240 or more steps (more when walking).
  Before the change they mostly came within 2 to 46 steps.
- **DIV-0154, the extra round** (D63). In a fight where the third member is
  marked faster early on, and then no longer qualifies while member 1 still
  does, the third member should not be asked for a command in the next extra
  round. D63 has the outline.

## 2026-10-20, typed-data stage C

`typed/c` (`main` 61023304 merged in;
[`typed-data-stage-c.md`](typed-data-stage-c.md), the round doc's section 3).
No behaviour change, the machine code identical to `main` (the smoke run
passed 2026-10-20: the round doc's section 3). The C4 audit's two names, answered
2026-10-20 (reports A and B in `C:\Users\kerok\bof3x-runs\c_typed\audit\c4\`):

- **`rest_4a`'s `kEnemyHp`**: renamed `kEnemyIntel` ("go ahead with the
  rename"), matching `records.h`'s `BattleEnemy::intel` at `+0xBA`; no
  behaviour change. IDEAS I49 is the later check of what the test means.
- **`Save_Directory`'s count**: 16 kept ("I agree on the 16 save slot
  cap"); the overrun stays D322's, bounded by DIV-0146.

## 2026-10-20, answered at the machine

- **The quicksave across languages**: the original Chinese against any
  translation refused before a byte is written ("another language", PR #96,
  [`quicksave.md`](quicksave.md) 4b); one translation into another kept,
  IDEAS I47 to review it. Live: en-US into fr-FR loads, en-US into the
  original and the original into en-US refused, the game playing on.
- **The ending's mirrored bands** (DIV-0127): seen, accepted as they are -
  "ok for ultrawide (known hack view), and it's not noticeable on regular
  wide" ([`ending-backdrop.md`](ending-backdrop.md) 6).
- **D266, the Western title mark**: "read the 12" - built on
  `fix/d266-title-mark` (DIV-0101's `titlemark` rule); a US disc-only title
  draws its TM whole (`analysis/shots/d266-title-mark-1020/`).
- **The ending roll** (`Scena17_EndSteps`): "let western installs use their
  own roll" - DIV-0148 on the same branch; the US and PAL discs each roll
  their release's credits (`analysis/shots/ending-roll-1020/`). D323 was the
  patchwork before. Then "let's have the roll track with localization": the
  roll is the language layer's, so a PC install under en-US rolls the US
  credits too, the original text and ja-JP the Japanese ones.

## 2026-10-18, the install-gap round

`install-gap/round` (`0baf26f7`), its end docs on `install-gap/end-docs`
([`install-gap-round.md`](install-gap-round.md); the census's statuses,
[`install-gap-census.md`](install-gap-census.md) 8; DIV-0101, DIV-0103,
DIV-0069 and DIV-0064 amended, D319 and D320). Each call below is one line, with
the doc that has the reading.

- **Keep DIV-0064's amendment?** (LA's DIV-B) It changes the PC install under
  every Latin layer. Each faerie shows its own name (D289's fix), and the French
  and German layers show their discs' words for the equip and experience panel
  labels, the shared list's title, the gene list's title and kind 0x5E's menu
  title. Built and in place, the confirmation pending: [`install-gap-labels.md`](install-gap-labels.md) 8.
- ~~**`Title_Sprites+0xE`** (D266, the Western title mark)~~ - answered
  2026-10-13 and built 2026-10-20 (the section above).
- **`Encounter_SlotChance+0x1C`** (the empty save slot `0x669CCC`): read on
  every load screen of all seven installs; the staged localisation PR, its
  section 8 first: [`name-entry-restoration.md`](name-entry-restoration.md), census 8.2.
- **`Area124_Weights+0x50`** (EU, DE): the European discs give area 125 other
  weights. Take the disc's own, or the PC's (which no disc has a reader for)?
  [`install-gap-twins.md`](install-gap-twins.md) 3.
- **The Western u16 fields of `EffectKind66_States` and `EffectKind5B_States`**
  (three each, Western PSX): the Western discs' own numbers or the PC's (JP
  equals the PC)? [`install-gap-twins.md`](install-gap-twins.md) 2 and 3.
- **`0x66A094`'s layout** under a Western layer: draw nothing (as now on a
  disc-only install), or take the West's narrower box and the count's x?
  [`install-gap-labels.md`](install-gap-labels.md) 4.3 and 8.
- **The FR / DE music-player labels** (`CommuMusic_CloseSteps+0x28` / `+0x40` /
  `+0x66`): a raw-byte label group, after a capture of the texture page on each
  install kind? [`install-gap-labels.md`](install-gap-labels.md) 4.4.
- **D310's format `0x654900..0x654904`:** keep the engine's own string, or take
  the disc's copy that `immref` finds in `SCENARIO/SCENA10.EMI` on PSX-US (a
  DIV-0101 amendment)? [`exe-immediate-places.md`](exe-immediate-places.md) 4.
- **The two `LeaderPanel` `psx` entries** `0x528CD0` (`LeaderPanel_S1Choose`)
  and `0x5295F0` (`LeaderPanel_S4Run`): FP's and OT's methods share nothing and
  both derive another twin than the entry's. Correct them?
  [`install-gap-pairs.md`](install-gap-pairs.md) 2.3, [`exe-immediate-places.md`](exe-immediate-places.md) 3.
- **A Defender exclusion** for the build and harness folders: Windows Defender
  quarantines every local launcher build, so the round's live end batch (the
  A/B/C caches, the attract gate, the hash routes, the captures) has not run.
  This is a machine setting only you can make:
  [`install-gap-round.md`](install-gap-round.md) 5, [`install-gap-reads.md`](install-gap-reads.md) 8.
- ~~**D321, PSP-JP-only item lists stop at start** ([`known-defects.md`](known-defects.md) D321): the L1 / R1 corner has no Japanese source - blank, the Latin icons, or part of the ja-JP layer (D280)?~~
  **Decided 2026-10-18 and built:** a capital `L` and `R` from the `ja-JP`
  font (DIV-0138 amended, `install-gap/d321`); psp-jp's five item-list
  routes now run to the end. Owed: your eye on the corner
  (`C:\Users\kerok\g_d321\shots\pspjp_*_corner_*`).

## 2026-10-17, the button unification round

`buttons/round`, its end on `buttons/round-end`
([`button-unification-round.md`](button-unification-round.md); DIV-0133..0141,
D312..D315). The captures and saves are game-derived, outside the tree, under
the harness root `C:\Users\kerok\b_rc`. **That root was deleted 2026-10-21**
(HANDOFF "Pick up here" 1): the saves and sheets named below are gone. Before
the checks, prepare a new root under `C:\Users\kerok\bof3x-runs\` and
regenerate them with [`button-routes.md`](button-routes.md)'s commands
(`abc_run.py prepare --set sigma`, `sigma_saves.py`, `sigma_sheets.py`, the
new root in place of `b_rc`).

### Decided by the owner (2026-10-17, built)

- **Unify every button flow** (answer 1): menus confirm on Speak's button and
  cancel on Action's (DIV-0133); every icon and hard-coded test asks the one
  table (DIV-0135..0140). Confirm follows Speak even onto a shoulder.
- **An install starts with its source's Config** (answer 2), and **a PC
  install with a Western layer or disc starts with Japan's** - the PC's own
  data counts as a JP source (DIV-0134). This answers the earlier question
  of the US release's button assignment (BD, 2026-10-12): a US disc-only
  install now starts with the US one.
- **PSP-EU's well and ram text translated by `P`** (answer 3; DIV-0140). HT
  read the PSP-EU code: the well tests `P` (D315, a release quirk; ours
  follows the Config), and the ram reads the Move word, so PSP-EU's own ram
  text is wrong on its disc (D314, Capcom's).
- **The world map's legend:** the port's `SPACE` / `X` / `ENTER` art replaced
  by the glyphs alone, no label text; the pictograms, the PC-only `START`
  and the spacing stay; a withheld row on a PC-only install dimmed as the
  discs dim theirs (DIV-0135).
- **The item lists' corner on a PSP install:** one code path, the PSP's `L`
  / `R` blanked, the same `L1` / `R1` drawn (DIV-0138).
- **L2 / R2 stay outside `σ`**, ORed into Speak and confirm as the original
  rebind does; no L2 / R2 glyph anywhere.
- **The PC's own text:** the colour swap - the port's stand-in, glyph and
  colour, of the button now; no new art (DIV-0140).
- **The held tests (HT-2):** `SuperCombo_ReadButton` keeps its own buttons;
  the music room, the battle cross's L1 / R1 and the text's hold-to-speed go
  through `σ`; the command boxes stay where they are (DIV-0139).
- **The fishing labels:** recolour only when the button moves (DIV-0136).
- **The PSP-EU Config screen** (D246): the PSP's words, four one-word names
  of ours where it has none (DIV-0141).

### To do at the machine

- **Record the minigame starts** from RC's placed saves,
  `C:\Users\kerok\b_rc\saves\` (deleted 2026-10-21; regenerate first, see
  above): `chop8` (the chop, area 8), `well68`
  (the well), `rope75` (the rope pull), `tower112` (into area 112, for the
  antenna in 111), `ram121` (the ram), `dock128` (the dock's parts menu),
  `crane135` (the crane) - each with `BOF3X_RECORD`, from the load to the
  start ([`button-routes.md`](button-routes.md) 5 says where each save
  stands). These are the only live evidence the dock's legend, the
  minigames' tests and the dialogue's button mentions lack.

### To look at

- **The contact sheets**, `C:\Users\kerok\b_rc\sheets_round\` (deleted
  2026-10-21; regenerate first, see above)
  (`worldmap`, `fishing`, `config`, `items`): identity beside swapped, a row
  per install kind (PC, PSX-US, PSP-EU, PC + en-US). Look for: the legend's
  rows trading, the PC-only stand-ins and START, the reel label, the Config
  panel's column, the list corner's tiles (the 8 px face glyph if a face
  button lands there), PSP-EU's `L1` / `R1`.
- **Confirm on a shoulder** (answer 1 literally): with Speak on R1, every
  menu confirms on R1 - fine, or not.

### To decide

- **`Scena19_Run0`'s R1** ([`button-tests.md`](button-tests.md) 2.4): left as
  it is; you do not place that section yet.
- **D314:** record more than the defect (PSP-EU's own ram text), or leave it.

## 2026-10-17, stale installs, the gap census's fixes, the PSP-only title

`install/recipe-version`. Captures are in session `6467ad33`'s scratchpad
(game-derived, never in the tree); the scratch homes are `C:\Users\kerok\rv`
(PSX-US + PSP-EU) and `rvp` (PSP-EU only), the launcher folder `rvL`.

### Approved by the owner (struck, 2026-10-17)

- **The PSP-only title's layout** (DIV-0131, D256): the mock-up, "much
  better"; one layout at every width, 4:3 cropping the wingtips.
- **Staleness covers the exe image and the cache recipe, piece by piece**, and
  a stale install is **prompted, then rebuilt** ([`install-staleness.md`](install-staleness.md)).

### To look at

- **The stale-install prompt** in a real start: make a piece stale (a row of
  `recipes/exe-psp.tsv` in a source checkout), start that checkout's launcher on
  a home it built; the box names the pieces; yes rebuilds (the unchanged pieces
  taken from the cache), no starts nothing. Every install built before this
  branch is judged stale once.
- **The PSP-only title in play** at 4:3, 16:9 and 21:9 (captured, DIV-0131).
- **On a disc-only install** (D310, D311): area 75's counters and area 104's
  countdown draw digits; the faerie village board's rows draw; a level-up that
  teaches an ability names it (Western discs); the Identify panel's `?` marks
  (D278); the field menu's play-time colon; the hi-lo game pays out.

### To decide

- **The PSP's copyright line** on a PSP-only title: its sprite cuts the PSP's
  `,2005` (as it always has); widen it to the PSP's row, or leave it.
- **The census's owner items** ([`install-gap-census.md`](install-gap-census.md) 8):
  the event battles' track words first (all seven builds ask for track 0).

## 2026-10-14, the ultrawide round

Gathered at the round's fold-in ([`ultrawide-round.md`](ultrawide-round.md)
5b). The pictures are game-derived, in the main checkout's `analysis/shots/`
(never in the tree).

### Approved by the owner (struck, 2026-10-14)

- **The lighthouse's rays** (DIV-0126, D243): the rays fan out of the globe
  again.
- **Depress's swirl and the fishing pedestal** (DIV-0125): "That restoration
  looks perfect"; the pedestal shows the field's ground.
- **Case zero's walk** (DIV-0123): the pair leave by the top and wait out of
  view, in the original's frames.
- **The campfire over Garr** (DIV-0071's 2026-10-14 amendment, D301), seen in
  a replay.
- **The bulb's filament** at the lighthouse's pedestal (DIV-0071's 2026-10-13
  amendment): the owner likes its look.

### Decided by the owner, to build (2026-10-15)

- **The faerie through the table** (D300): fix this scene narrowly -
  built 2026-10-21 as DIV-0151 (the wide-ring round, above).
- **The narrow areas:** a per-scene pillarbox - built 2026-10-21 for areas
  31, 90 and 198 (the wide-ring round, above).
- **RG, the ring for far cameras** (D292): built 2026-10-21 (the wide-ring
  round, above).
- **The breath mote and the column's sound** (D293, D294): **leave both** at
  Capcom's bounds (struck; recorded in the defects).
- **The two sweeps** (IDEAS I38): mapped onto the wide picture - built
  2026-10-21 (the wide-ring round, above).
- **The 14 unmovable 16:9 band sites** ([`wide-staging.md`](wide-staging.md)
  3): **a policy per kind, not site by site** - leave visible the stationary
  speakers (areas 26, 39, 94, 188), the walks that end in the band (areas 60,
  143, 172) and area 7's mid-route point. What is left is a look: census 3's
  parked and still sheets, flag only what looks wrong (item under "To look
  at").

### To look at

- **The faster slide** (DIV-0041, SL; not yet seen): in the wide modes a menu
  or shop box slides out faster (up to about twice Capcom's speed at 21:9) and
  is gone on Capcom's frame. A menu with the time and money boxes, a shop, a
  gene screen. Also whether a box sliding *in* is seen whole in the 21:9 band
  on its first frames (D296) - fine, or to fix?
- **The 21:9 left message box** ([`ultrawide-census-1.md`](ultrawide-census-1.md)
  6.1): a left-placed box draws whole at the picture's left edge (game x -104),
  the 16:9 placement moved by the columns - right at 21:9, or nearer the
  middle?
- **The terrain margin at 21:9** ([`ultrawide-frame.md`](ultrawide-frame.md)):
  the cull margin is `c + 47`, 167 at 120 columns, a rule from the value tuned
  by eye at 53 - any corner popping at the edges as the camera turns?
- **Census 3's band sheets** (the policy above leaves them visible): the
  parked and still rows in scenes ([`ultrawide-census-3.md`](ultrawide-census-3.md)
  4.2; `parked-scene_w1_*.png`, `still-scene_w1_*.png` in
  `analysis/shots/uw_census3/sheets/`) - flag any that look wrong.
- **Two `dolphinBoss` plant frames** (f03060, f00720;
  [`campfire-and-crystal.md`](campfire-and-crystal.md) 5): plants in front of a
  character are drawn in front again after the campfire's amendment - right?

### To decide

- **The gene crystal** (D242, Capcom's order; accepted for now): CF's
  candidate rule ([`campfire-and-crystal.md`](campfire-and-crystal.md) 6) is
  there if the cut ever bothers you.
- **The patch cells** (D295, measured in
  [`ultrawide-patch-cells.md`](ultrawide-patch-cells.md); your word: leave for
  now) - only if one is ever seen changing early in the band (section 4 there
  lists the scenes).
- **The six other effects the GTE fix changes** (DIV-0126; kinds 0x1E, 0x1F,
  0x21, 0x64, 0x72, 0x97, 0xA0): their shards and debris now turn by a true
  rotation; no route reaches them - look when one does.
- **Effect kind 0x51's grab** (DIV-0125; areas 34 and 108 at given chapter
  counters): its column should now show the scene behind it warped, a heat
  shimmer - unseen, no recipe.

## 2026-10-13, the away day

Gathered from the day's fifteen merges on `platform/more-hardening`; each item
wants one line back. The pictures and sounds are game-derived, in the main
checkout's `analysis/` (never in the tree).

### Decided by the owner, to build (2026-10-15)

- **Ryu's byte, D288**: **take the disc's**; on a PC-only install clear the
  bit by rule (`lzss_flag`, verified by structure) -
  [`arena-render-1013.md`](arena-render-1013.md) 6's draft DIV.
- **Inventory Q7, the 27A row's discs**: **yes** - the 27A loop from any PSX
  disc (all five decode identical).
- **Member 1's battle bank** (`BPLD012` / `015` / `016`): **take the disc's**
  (the added part draws nothing; seen on the render sheet).
- **Member 2's field bank** (the eight `PL` arenas and `START`): **the split** -
  keep the PC's nudges (a repair of Capcom's art, a DIV) and drop script 42's
  added part by the structural rule of [`arena-render-1013.md`](arena-render-1013.md)
  6; a disc-only install keeps the disc's art. Decided off the render sheets.
- **D255's fix**: **both** - the disc's XA stream (`S_XA00.STR`) as the
  jingle on a PSX-only install, and a failed jingle open ending the wait at
  once (a DIV), so no missing file can hang it again. **Built** 2026-10-16:
  DIV-0129, `fix/psx-only-1016`.
- **The Western weakness row**: **keep the row and the element glyphs on every
  language** (as now). The owner's `radiationRoom.txt` recipe (2026-10-15)
  reaches an enemy panel with the row, for the A/B.
- **The US title layout, D277**: **draw the US release's three-piece notice
  on a US-built image** (a DIV).
- **The Western title mark, D266**: **yes** (built 2026-10-20, PR #98) - the width read
  from the disc's `GAME.EMI#1`, built as its own `titlemark` rule in
  `tools/psp_exe.py` rather than a `twin` row.
- **A ja-JP layer, D280**: **yes, all of kinds 7..16** from the Japan disc
  (DIV-0015 / 0018 / 0019 / 0020 / 0064 / 0069 extended).
- **The fishing crash, D279**: **both** - kind 16 through the ja-JP layer, and
  the scroller declining an empty line (a code DIV). **Built** 2026-10-18:
  DIV-0144 and DIV-0069's amendment, `install-gap/d279`.
- **Inventory Q6, the wrap clicks**: **clamp now** in the decoder when the
  banks are derived (a DIV; the 76 banks no longer equal the PC's).
- **Set 8's verb**: **confirmed** - keep `Quit` (DIV-0101 amended).
- **The key icons** ([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 10):
  (1) **icons that tell the truth first**, the US release's icons as a layout
  preset after; (2) the Western text's icons **translated** to the PC's
  buttons; (3) the world map's labels as the disc's words with live glyphs -
  **a launcher option**, not the default; (4) keyboard glyphs as **key names**
  in the game's own lettering; (5) **the reached sites first** (world map,
  fishing, Config), the dialogue after. **Built 2026-10-17** for the
  PlayStation shapes by the button unification round (DIV-0133..0140; the
  section at the top); key names and a US preset are a later round's.
- **`combatGroup` and `sortScreens`'s saves**: `combatGroup` takes `combat`
  (the owner's assumption). `sortScreens`' own save is not among the recipe
  saves: replayed on `vaccinationAndFireScene`, `bridgeWalk` and
  `ninaWalkBehindBlock` (all three hold its 11,957 zenny), its sort menus match
  the 2026-10-07 frames, but its first run's play time (08:20) and its Camp
  step (a camp room at about frame 1770) match none. The owner: attach
  `vaccinationAndFireScene` - the sort menus are what the route is for. Its
  frames from about 1770 on are a town, not the first run's camp.
- **`radiationRoom`'s save**: `tools/recipe_saves/radiationRoom.DAT`; the
  recipe carries `# save radiationRoom`.

### To look at

- **The eye batch, `analysis/shots/eye-1013/q1..q7`** (one folder a
  question, each with a `README.md`; [`eye-batch-1013.md`](eye-batch-1013.md) 2):
  - q1, D256: the PSP title's logo in two pieces with a seam near x 218 and a
    second, cut ™ - keep the PSP's title on a PSP base, or draw a PlayStation
    title there?
  - q2: the PSP logo movie's wordmark about a third taller than the
    PlayStation's - fine, or wrong? (and the launcher's two clipped lines,
    D287 - fine to fix?)
  - q3: the wide picture - anything wrong besides D284 and D285? (D283 is
    fixed by the random split, 2026-10-13.)
  - q4: PSP-EU's Config screen blank (D246) - decided and built 2026-10-17
    (DIV-0141).
  - q5: the fishing ticker - the disc's one message at a time against the
    PC's packed band; the disc's circle glyph against the PC's red ring; PSP's
    "buttons" - which style?
  - q6: under ja-JP the Config grid is the port's Chinese (D280) - see "to
    decide"; the rest equal narrow / wide.
  - q7, D255: PSX-EU never comes back from the inn's black (now a hang) -
    see "to decide".
- **The arena sheets, `analysis/shots/arena-render-1013/`** (`README.md`,
  `RYUD.md`; [`arena-render-1013.md`](arena-render-1013.md) 2): member 2's
  nudges and slits (`PL*_seams.png`), member 1's added part (`BPLD*`), Ryu's
  byte (`RYUD_image10.png`) - the three decisions below.
- **The D56 capture, `analysis/shots/d56_before` / `d56_after`**, frame 500
  ([`d56-encounter-bounds.md`](d56-encounter-bounds.md) 4): an enemy off the
  left edge before, refused after (DIV-0116, on by default) - right?
- **The labels sheet, `analysis/shots/labels_1013/identify_sheet_1013.png`**
  ([`labels-1013.md`](labels-1013.md) 1.4): `WEAK POINT`, `ITEM` / `OBJET` /
  `OBJEKT`, `EP` on the Identify panel (DIV-0117) - placed right?
- **The Japanese world map**, `C:\Users\kerok\abj\runs\jp1013_worldMapAndAreaTransition`
  frames 1140..1770 (D281, [`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 3.3):
  PSX-JP draws it about a third closer than the PC and PSP-JP. **The owner's
  note (2026-10-13):** does not remember the Japanese release's map being
  closer, but it is subtle enough that they would not; since the PC port and
  the PSP both draw the wide view, the zoom may be a change Capcom made after
  the initial Japanese release (the European disc drew the wide view in the
  1012 sweep). Recorded as the owner's hypothesis; the PSX twin read (JP
  against US `GAME.EMI`'s world-map camera set-up) is the check - anything to
  add?
- **Area 65's forest canopy** (`C:\Users\kerok\abu\runsဓ_fairyVillage`, the
  PC's `AREA065` art against both discs', [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md)
  3.3) and **bossAndFlash's three boss-effect frames** 3480, 4530,
  4800..4830 that differ between the installs on both runs
  (`abu\runsဓ_bossAndFlash`, `u1013r_bossAndFlash`; that doc 9) -
  anything visibly wrong?

### To listen to

- **`analysis/listen-1013/`** (each folder's `README.txt` says what and when;
  [`wav-dump.md`](wav-dump.md) 4):
  - a, `a_random_encounter_resume`: the area's song resuming after a random
    fight (DIV-0092) against silence (Capcom's rule) - right, as the
    PlayStation?
  - b, `b_loop_rows`: loops `064`, `076` and `037` with DIV-0081's table on
    against off - the joins clean?
  - c, `c_inn_jingle_psx_only`: the inn on the PC against a US-disc-only
    install (D255) - which jingle source, if any?

### To decide

- **The hi-lo count** ([`labels-1013.md`](labels-1013.md) 3): **a route
  first** (the owner, 2026-10-15) - a recorded route to the hi-lo game, and one
  to fight 26 / Beyd's duel, so both can be seen against the PlayStation before
  deciding. Where each starts is the owner's to say or record.
- **Resume or restart after a random encounter.** From the `cutsceneAndNue`
  save in mednafen (the US disc's port 1 card, slot 1, placed beside the
  sibling's saves), after the Nue fight: walk into a random encounter and
  listen to whether the area's song comes back where it left off or from its
  start. The Nue boss restarted it (the owner, 2026-10-11); DIV-0092's
  amendment says the PlayStation resumes. Event battles restart the area's
  song since #54 (DIV-0092's second amendment, heard); this listen confirms
  the random-encounter resume itself.

## To listen to (the music investigation, 2026-10-06)

- **The listening set is ready**: `analysis/bgm/listen/README.txt` says the order
  ([`bgm-comparison.md`](bgm-comparison.md) section 9). Play each pair disc,
  then MP3, then disc: `141_*` the title music (brightness and room), `153_*`
  the battle theme (the replayed 7.4 s intro and the cut at 23.6 s), `000_*`
  the town theme (a rhythm hiccup at 17.3 s), then `000_seam.wav` and
  `153_seam.wav` (the PC's loop join on its own, at 8 s). For each: same,
  slightly different or clearly different, and in what. **Then the decision
  the plan waits on** (section 10 there): the disc's music as an option, a
  measured loop-point table for the MP3s as the cheap fix, or both; and the
  decoder swap's target is now known (MPEG-1 Layer III, 44.1 kHz, 128 kbit/s
  CBR, plain stereo, no tags). **The owner heard it (2026-10-06 night):**
  quality close, the seams very noticeable - **the loop fix is built as
  DIV-0081** (13 tracks today; the rest measured tomorrow). Next for the ear:
  `analysis/bgm/listen/153_loop_fixed.wav`, the battle theme spliced the
  engine's way (the join at 23.6 s), and then the game itself with
  `BOF3X_MUSIC_LOOPS` on against `=0`. **One decision:** the town theme
  `000`'s file is 0.44 s shorter than one loop, so no correct loop exists in
  it; the ways on are to accept the rewind for that song, a stretched loop,
  or samples from outside the file (a render is audio derived from the
  game's data and ships nowhere, rule 1; a re-encode the player makes would
  be their own). Undecided.

- **DIV-0081's second cut (2026-10-10, `catchup/music-tooling`,
  [`music-open-ends.md`](music-open-ends.md) 3):** four shipped rows looped
  at a period the sequence does not have - `064` and `076` dropped the last
  of their 16 bars each pass, `131` 2.9 s, `070` 20 ms. The table is now 39
  rows: listen first to `064` or `076` (should now play its whole body before
  the join) and one new row (`037`, `084`, `109`, `130`) with
  `BOF3X_MUSIC_LOOPS` on against `=0`; `131` and `070` rewind as the
  original now. DIV-0081's near-full call (a slip a pass, or the rewind) has numbers now
  (`music-open-ends.md` 5: the rewind is within -41..+7 ms of the disc's
  period on all 23; a slip loop scores worse on most).

## To review in play

**State 3, `bof3x.exe` (PR #53, 2026-10-12):** the icon generated from the whelp
portrait (DIV-0095, [`exe-icon.md`](exe-icon.md)), the window and its DPI scale
(DIV-0094, DIV-0036 amended) - watched only through the end batch's disc-only
installs so far, never looked at for themselves.

**The disc-only round (2026-10-12, [`disc-only-round.md`](disc-only-round.md)) - a build from the US disc alone, nothing of the PC's:**

- **A key press in `bof3x.exe`.** RD found the keyboard has been dead in the
  exe host since state 3 (Capcom's DirectInput object table lay in `.text`,
  `int3` there; the SDK's `c_dfDIKeyboard` replaces it). Start `bof3x.exe`
  through the launcher and press any bound key on the title screen; the
  pad was never affected.
- **The disc-only install itself** ([`disc-only-install.md`](disc-only-install.md)):
  `importer.py build --source "<US cue>" --lang en-US --out <cache>`,
  `install --base` into an empty directory, the launcher with `cache=` and
  no `BOF3.exe` there. Play the title, a field, a fight, a menu, a save.
  Known gaps to look for (DIV-0100's doc, section 4: 3,358 bytes of Capcom's
  game values no disc carries were zero at the round's start; after the PSP
  round's IM2 and VF, PSX-US 285 ranges and 1,141 game-value bytes): the wrong effect file on a spell or
  item (`Magic_Rows`), the dragon-form sets, the equipment and experience
  panels' numbers, the ending roll (each release's own since 2026-10-20, DIV-0148; the first two and parts of the panels' and the event battles' tables filled by rule since 2026-10-12, DIV-0101 amended). What you see decides the first item under
  "Decisions".
- **DIV-0096, the eleven arenas as the disc's:** member 2's field sprites and
  member 1's battle sprite may differ by a part in some animations; walk and
  fight with member 2 in the party on the disc-only install.
- **DIV-0097, the disc's own images:** the four area pages (`AREA104`, `127`,
  `134`, `164`), the faerie village sheet, and the French and German sheets
  under a FR-only or DE-only cache.
- **DIV-0099, the Capcom logo from the disc:** with ffmpeg on the machine the
  movie plays at start from the converted AVI - look (30 fps, doubled 320 x
  240) and listen (44.1 kHz from the disc's XA); nobody has played it in the
  game.

**The PSP-only round (2026-10-12, [`psp-only-round.md`](psp-only-round.md)) - a build from a PSP disc alone, and the PSP's content as suites beside a PlayStation disc:**

*Answered (2026-10-12 afternoon, `psp-only-round.md` 7.1; recorded where they belong):*
- **The game-value gaps: fill them** - the four largest (`Magic_Rows`' effect files, the dragon set files,
  `EventBattle_Records`' battle files, the formation wheel's spots) are filled by rule on every disc build, PSX
  too (DIV-0101 amended, [`disc-gap-fills.md`](disc-gap-fills.md)); the rest stays open under "Decisions".
- **C1** is the duel between Beyd and Zig (chapter 6 run 9, event battle 0x1B, area 92) and **C2** is
  set-up 55, Myria, in area 198 - **corrected by PB**: the PSP sets Myria's first item word (the PSP's `+0x98`,
  ours `+0xA8`: the item a steal takes and the first drop rolls) to item 0x317, "Ivory Charm" by the sibling's
  names, not an agility; the PSX holds consumable 0x17, "Napalm", there (same index, category changed; IDEAS P17). The owner, 2026-10-12: keep it a toggle. Both are members of the launcher's data suite, on by default under a PSP base
  (DIV-0108, [`psp-battle-changes.md`](psp-battle-changes.md)).
- **The suites' membership** as LT proposed: the name layers and P8 under data, the button-label page left out
  beside a PSX base (DIV-0107).
- **The PSP's music:** the restored stereo width is fine; no narrow-pan bank form (DIV-0105).
- **The dropped tone:** the rule kept, the PSP's one tone a box on the Jukebox, the restored chord the default
  (DIV-0103, DIV-0113).
- **The PSP-EU Config screen:** decided 2026-10-17, built as DIV-0141 (D246).
- **The PSP-only mode** (CD2 / CD3: a new menu choice and 22 hooks run in AREA030, probably fishing): left until
  you are back (below).

*Answered the same night (2026-10-12; recorded where they belong):*
- **Widescreen: option (a) only** ([`psp-widescreen-placements.md`](psp-widescreen-placements.md) 6.1, built by
  WA, DIV-0041 amended): the PSP's per-element choices at our 53 columns; no crop, so no "PSP framing" setting.
  **The message boxes and the areas 104 / 121 HUD anchored** to the edges in wide mode (this reverses your
  "the dialogue boxes seem ok" of 2026-09-23); **the ending keeps its letterbox**; **the tile ring** grown as the
  PSP did is wanted later (IDEAS I37, the inset approach for now); **the A8 bars widened** ("If psp widened the a8
  bar, let's widen it too" - the hold of 2026-10-04 lifted); **WA's reveal kept** (both primitives widened: "I
  assume some psp draws will leave 16 row bands since we are not cropping the same"), and from it the rule: where a
  PSP widening relied on its crop, ours widens over the full height.
- **Myria's item:** the PlayStation holds "Napalm" (consumable 0x17), the PSP "Ivory Charm" (accessory 0x17) - keep
  it a toggle (`myria-item`, DIV-0108 amended).
- **The disc's data preferred** where the PC install and a disc both carry a chunk (prompted by the PC title's (R),
  DIV-0114): the disc-data preference inventory is next ([`HANDOFF.md`](HANDOFF.md)).

*What the end batch and the harness put in front of you* ([`psp-end-batch.md`](psp-end-batch.md),
[`abc-harness.md`](abc-harness.md)): **a contact sheet** of the PSP-only install against the PSX disc-only install
(`psp_eb/contact_sheet.png` in the round's scratch), **launcher captures** of the options page on the three caches
and the Jukebox (`psp_eb/lcap/`), and AB's PC / PSX-EU / PSP-EU picture comparisons (`report_vf2_*.md`). The
visual findings you reported from the live runs are fixed as gaps of every disc-built image (D251 the battle
framing, D252 the equip icons and number formats, D253 the areas 16 / 45 crash), not yet seen by you since.

- **The eye.** The PSP stand-ins' look as a PSP-only base: the PSP's area tiles (653 blanked - look for holes),
  its UI page with the shoulder-button labels, its dial and title pages, Ryu's forms (DIV-0103). The PSP's logo
  movie converted at import (Cinepak SSIM 0.868 against the direct decode, 30000/1001 fps, the PSP's frame
  scaled back to 4:3; DIV-0106). The launcher page: the three suites, their greyed reasons and wording, the
  layout (LT: 560 x 309 dialog units; two older notes clipped). The Jukebox: the PSP column, and the chord box
  and its wording ("Battle chord as the PSP plays it (one tone)").
- **The ear.** The PSP's music from a PSP-only cache or the `psp-music` layer through the Jukebox: the 16
  note-changed songs, 22 (128 to 126 bpm, and the one bank the inverse cannot split: 16.8 dB) and 157 (tempo),
  31 (its setup); the effects and voices from ATRAC3plus, and the 11 jingles re-encoded (DIV-0105, DIV-0106).
  The chord one tone against two, in a fight with one of the 16 containers' characters (`BPLD` 015, 025, 056,
  257, 457, 567, 578; DIV-0113).
- **Play.** From a PSP-only install with the defaults: **the Beyd-Zig duel, lost** - with `duel-loss` on, a
  party loss lasts until `Boss27_Event`'s other path sets bit 2, so it may stall (a scripted end the PSP meant,
  or a stall; the PlayStation's twin is uncaptured, so C1 may also be a repair - DIV-0108's tier waits on this);
  **Myria** in area 198 - steal from her, or see the drop, with `myria-item` on and off; a battle with magic
  (the filled `Magic_Rows`); a level-up of roster index 6 (the PSP's table, DIV-0104 / DIV-0111).
- **Still open (the follow-up round, [`HANDOFF.md`](HANDOFF.md)).**
  - **D56's repair** (built 2026-10-13 as DIV-0116, [`d56-encounter-bounds.md`](d56-encounter-bounds.md)): a
    fight no longer starts with a combatant off the left or top edge. Look at `analysis/shots/d56_before` and
    `d56_after` frame 500 (a crafted camera shift: the Gooey off screen, then refused); in play, nothing to see
    unless an encounter would have placed an enemy off the left - none of nine battle routes does.
  - **The save slot number's `"%X"`** (D254): done 2026-10-16 after your report of odd characters on the empty
    slots and behind the named ones - the engine's own two-byte format; the slots are numbered, the empty ones blank.
  - **The jingles on a PSX-only install** (D255: the inn's screen stays black longer without `OYASUMI`): the
    disc's XA as the jingle, or the failed stream ending the wait.
  - **The PSP title's framing** (D256): look at a PSP-EU title capture.
  - **The field menu's time and money boxes** in wide mode: you check on the PSP; unchanged until then.
  - **The PAL banks** (D257): take a PAL disc's eight area banks for a PAL disc-only install, or not.
  - **The PSP-only mode** (probably fishing): adopt it as a layer of ours, or leave it.

0. **The game from the cache** (DIV-0089, 2026-10-10,
   [`cache-read.md`](cache-read.md) section 9). **At the machine:** with
   `BOF3X_CACHE=<a cache built with the PC's DAT>` (or the ini's `cache=`)
   and nothing installed in `DAT\`, then with `BOF3X_CACHE_DATA=0` for
   comparison: the title, a town, a fight, the menus (whatever draws from
   `FIRST.DAT`'s images - the one container whose order matters; which
   screen shows the overlapped tiles is not read), an inn (an `SND\`
   wave), English from the cache's `loc/en-US`, and `psp-art` on Stallion if
   a save reaches area 67. **Look for** any difference at all; the log's
   `DIV-0089` lines say what was read from where. Also the one-root call
   (section 6 there): keep `BOF3X_CACHE` as the one root, or the cache for
   the music only.

1. **Music coming back after a pause** (`Sound_ResumeAll`,
   [`sound-rest.md`](sound-rest.md) section 3 item 2). Read from the code,
   never heard: `Sound_ResumeAll` plays whatever music buffer is loaded,
   whether or not it was playing, and a stopping fade leaves the buffer in
   place. **At the machine:** reach a scene whose music fades to silence
   (an area change through the field mode's `Music_FadeOutStop`, or a
   scenario step that fades), then while it is silent press F9 (pause) and
   any key (resume). Also: Alt-Tab away and back during the silence. **Listen
   for** the faded track returning. Run it on ours and under
   `BOF3X_ORIGINAL='*'` - the same on both is Capcom's, and a fix is a ledger
   entry. The scripted resumes (the inn's night, the flag-message fanfare,
   area 113's reward, area 135's box, chapter 11 step 47, areas 57 / 108 /
   145's shared handler) stop the music themselves first and are not the
   case.
2. **The layering fix in play** (DIV-0071, HANDOFF item 0000000): a town,
   stairs, a bridge, followers close behind, with the default on.
3. **TILE_1 drawn as one point** ([`d3d-rest.md`](d3d-rest.md) D-a). Shown to
   the owner 2026-10-06 from the `whelpBoss` route's frame 11880 (the dream
   scene, Deis in the light pillar: the drifting specks are TILE_1s, each one
   screen pixel where the PlayStation's covered four at this scale). A paint
   mock of the 2x2 was sent; the owner: "go ahead and make it larger".
   **Built the same day as DIV-0077** (on by default, `BOF3X_TILE1=0` the
   point). Owed the owner's eye in play: the dream scene after the whelp
   fight, or any Kaiser cast.

4. **Lines drawn one screen pixel wide** (the owner's note, 2026-10-06: the
   fishing gauge's thin orange bar "looks like it's supposed to be a bit
   thicker"). Measured the same day on the `caughFish` route's frame 1680
   (`analysis/shots/fish_1006_every60/f01680.png`, the PLAYER vs FISH gauge
   at 4x): the dark red line under the green bar and the gauge's grey centre
   mark are each one screen pixel, at a window scale of about 3.3. They are
   PlayStation LINE primitives (`LINE_F2` here; `D3d_DrawLineF2` / `_F4` in
   `d3d_draw.cpp`, `_G4` in `d3d_rest.cpp` draw a `LINESTRIP`, which Direct3D
   rasterises one pixel wide at any scale) - **the TILE_1 class again
   (DIV-0077), for lines**. A fix is each segment as a quad of the scale's
   width (every line in the game: the fishing gauge, the fishing grey lines,
   magic trails). The owner (2026-10-06): "all elements should scale" - **built
   the same day as DIV-0079** (one quad per segment, square ends, smooth
   diagonals; on by default, `BOF3X_LINES=0` the strip). Owed the owner's eye
   in play: the fishing gauge and the grey lines, a magic trail if one is
   cast.

## Reported by the owner, for a cleanup session

- **Battle music runs on after a fight, the area's music does not resume**
  (the owner, 2026-10-10 evening: "our game doesn't 'end' songs where I
  expect (compared to psx). The battle music will continue after battle
  ends, and the area music won't resume"). Catalogued as
  [`known-defects.md`](known-defects.md) D241 with where to start (the PSX
  twin's end-of-battle `Music_Play`, then the PC's path; whether the
  cache's music or the MP3s was playing). Not looked at. Needs a route or
  the configuration: which music source, and whether `BOF3X_ORIGINAL='*'`
  does the same.

- **The sort screens, the formation screen and the camp's Skill Notes**
  (the owner, 2026-10-07 morning, `tools/recipes/sortScreens.txt`: "a few
  other localization needed spots"). **Done the same morning**, DIV-0064's
  groups 7..11 and DIV-0084: the item and ability sort menus (`SORT`,
  `ManualSort`, `NormalItem`, `CombatItem`, `High AP`, `Low AP`, and the
  equipment sort's `Power` / `Defence` / `Kind`, which the route does not
  open), the camp's `SORT` / `LOOK`, `Ink`, the formation names `Normal` /
  `Attack` / `Defense` centred as the US draws them, and the zenny unit's
  Z (the letter `s` on the owner's Items screen was the overlay's repaint
  of the port's coin glyph). Replayed from the route,
  `analysis/shots/sortScreens2` frames 330, 420, 720, 1440, 1980. The
  owner confirmed the armour and weapon screens' sorts the same morning.
  **Owed the owner's eye in play:** the French and German builds' words
  (`Tri=man`, `ManSort`, `Encre`, `Tinte`...).

- **Unrendered squares at the wide view's outer edges** - **fixed 2026-10-07**
  (the owner, 2026-10-06 night; `analysis/shots/owner_reports/bridge_left_edge_unrendered_1006.webp`).
  Not a cull: the view's cell *inset* (`map_layers.cpp` `Inset()`) trims
  each ring row to what 320 columns need, and the wide picture's extra
  columns were never walked. Lowered by three columns a side under the wide
  view (DIV-0041's entry, `BOF3X_WIDE_INSET`); the `bridgeWalk` route wide
  shows the sea at both edges on every frame. The owner (2026-10-07): "the
  sea tiles stepping is probably correct now". Owed the owner's eye on the
  canyon bridge of the first screenshot, and on the attract sequence's map.

- **The Volt's EXP bonus did not appear** (the owner, 2026-10-06 night;
  screenshots `analysis/shots/owner_reports/volt_fight_menu_1006.webp` and
  `volt_fight_result_78exp_1006.webp`): a field fight against three Volts and
  one Thunder, Nina died, the result screen "You gain 78 EXP!" with 78 on each
  of the three rows. The owner's reading - their recollection of the game's
  rules, to be verified against the code: a Volt hit by an electric attack
  should change mode and give extra EXP; a Thunder gives about 16; expected
  about 84 x 3 + 16 = 308 with the bonus, 156 without, and 78 x 2 = 156. The
  first report (2026-10-02, HANDOFF item 00000) was held for a route; this is
  its substitute. **Staged for tomorrow** as a read of the reward path and
  the mode-change op against the PSX twin, then a route for the owner to
  record (brief `plat2/brief_volt.md` in the session-7d0c9683 scratchpad);
  not launched tonight (the PC goes off).

- **The Skill Notes prompt's hand a word short of `Yes`** (the owner,
  2026-10-06 night; screenshot
  `analysis/shots/owner_reports/skill_notes_record_yes_no_1006.webp`: the
  skill menu's Note tab, "Record in Skill Notes?  Yes No" on the top line,
  the hand pointing at the gap after the question mark, a word's width left
  of `Yes`). The owner's reading: the same yes / no issue as the other
  screens - another chooser `Menu_YesNo` does not reach, the fifth after
  [`yes-no-prompts.md`](yes-no-prompts.md) section 2's four. A cleanup
  session, by that doc's method: find the prompt's `Menu_DrawHand` site
  (the E8 scan's 44 sites; the skill menu's Note tab, so `0x9398D2`-style
  answer byte and stops to read), measure the English `Yes` / `No` from the
  line as section 3 does, give it DIV-0027's gap under a Latin overlay only,
  amend DIV-0027's list, capture before and after for the owner. Check the
  other skill-menu prompts (the Note tab's forget / overwrite, if any) while
  there, since they will share the site.

- **The master list's Chinese header and its marks** (the owner, 2026-10-06
  night; our screenshot
  `analysis/shots/owner_reports/master_list_header_marks_1006.webp`, a web
  capture of the US PlayStation screen beside it,
  `master_list_web_reference_1006.png`). Two things on one screen, the camp's
  "View master's profile" list:
  1. **The list's title is still 师匠** under the English overlay. The US
     screen's word is **`MSTR`** - this answers half of
     [`yes-no-prompts.md`](yes-no-prompts.md) section 5's question to the
     owner (the portrait box's 弟子 label, `0x66A1F8`, is still unanswered:
     the web capture does not show that box). The slot is `0x66A1F0`, 8
     bytes, drawn by `MasterWin_DrawList` `0x59C2C0` (ours, `rest_2h.cpp`)
     through `0x57D800`. Section 5 found no `MSTR` on the US disc as text, so
     the four letters would be ours to write - DIV-0064's way, a kind-15
     group into the slot, the word recorded as authored from the owner's
     reference, not read from a disc.
  2. **The mark beside a completed master is a cross (`†`), where the US
     screen shows a star (`★`), and a dot for the others.** The owner's
     reading: crosses where stars should be. `MasterWin_DrawList` draws the
     mark from `0x66A2D8` (`rest_2h.md`'s row: "the mark `0x66A2D8`
     (available) or an icon"), a `.data` string whose Chinese glyph the
     Latin font maps to `†`. The fix is the same shape as the header's: find
     what glyph the US overlay's font has for the star (the web capture shows
     the US font draws one), and write the code into the slot under a Latin
     overlay only; or, if the mark is an icon on the US side, draw it as one.
     **The dot is already right:** a second capture
     (`master_list_unfinished_dot_1006.webp`, Fahl unfinished) shows the
     unfinished mark as a small dot under our overlay, as on the US screen -
     so only the completed mark's glyph is wrong, and `rest_2h.md`'s "or an
     icon" is the dot. Amend DIV-0064 with both.

  **Done 2026-10-07 (DIV-0064 amended, DIV-0059 extended):** both are the
  US disc's own - `SHOP.EMI` has `MSTR` and the star beside each other, so
  nothing was authored. The cross was ours: the mark is the byte `t`, the
  shipped font's `t` slot is a star, and the overlay's letter repaint wrote
  a t over it. `MSTR` is centred on its box in
  `analysis/shots/master_labels/masters.png` (the camp route); **the star
  is owed the owner's eye** - no committed save has a completed master.
  The pupils box's 弟子: the owner's US capture (the same morning) shows the
  PlayStation draws no label box there, so DIV-0083 leaves the port's out
  under a Latin overlay (`analysis/shots/master_labels2/masters.png`).

- **Two routes with visual glitches, recorded by the owner** (2026-10-06
  night, `d1c5dcf1`: `tools/recipes/ninaWalkBehindBlock.txt` and
  `tools/recipes/bridgeWalk.txt`, their saves imported by the owner as
  `tools/recipe_saves/ninaWalkBehindBlock.DAT` and `bridgeWalk.DAT`; replay
  with `--save NAME`). Read 2026-10-07 with a capture every 60 frames
  (`tools/recipe_shots.py --every 60`), A/B against `BOF3X_LAYERING=0`,
  `BOF3X_WIDE=0` and `--original '*'`:
  1. **Nina through a crate - DIV-0071's rule, fixed.** Identical with the
     layering off, wrong with it on (frames 360 and 540): the rule guarded
     the feet only, and the crate's raised top covered her body, not her
     feet, so it was crossed. Now nothing but floor is crossed where it
     reaches the sprite at all (the entry's "Refined 2026-10-07"); the
     route's captures identical to the switch off, the fix's own routes
     re-run. **Owed the owner's eye in play** with the default on.
  2. **The bridge scene** (area 41, `DAT\AREA041.DAT`), each cause apart:
     - the sea's stair-stepped edges: the cell inset, **fixed** (above);
     - **Garr behind the deck and the near railing: Capcom's order**, the
       same under `--original '*'` and with the layering off (the rule is
       held off there by the railing's cell records, as the entry says).
       The owner (2026-10-07): fine as it is, the railing has holes;
     - **the sky stopping short at the left, and the stretched sliver at
       the right: fixed.** The sky is `EffectKind18Sub15_Draw`'s (the area's
       own effect, `effect_5c.cpp`): a 320-wide haze band, three scrolling
       cloud strips clipped to 0..320 and four gradients; a strip starting
       past 320 was drawn backwards into the band. Widened to the picture's
       edges under DIV-0041 (its entry, "The sea bridge's sky"). The owner
       on the capture (2026-10-07): "Sky looks perfect";
     - **the "waterfall": fixed and seen, 2026-10-08** (DIV-0085, D239):
       the deck's edge cells gained a south step from the sky effect and
       their cliff read the next tile's sea texture; both faces now take the
       cliff word. The owner, the bridge wide, walked several times: "I
       can't notice anything off about the deck edges", the streaks gone.
  The third capture (`sprite_crop_2135_1006.png`) is a close-up of one
  of these and needs no separate reading.

## Decisions the measurements raised

- **The Capcom game values no disc carries** (2026-10-12, NB and IN of the
  disc-only round; [`exe-engine-own.md`](exe-engine-own.md),
  [`disc-only-install.md`](disc-only-install.md) 4): JP 1,622 / US 2,135
  bytes in 456 ranges - `Effect_HpDamageVariance+0xB8` (468), `Magic_Rows+0x4`
  (300), `Equip_SlotIcons+0x50` (131), `EventBattle_Records+0xE0` (82) the
  largest - exist in no shape on any disc. Rule 1 says they are not copied,
  so a disc-built image maps them as zero (logged; `BOF3X_EXEIMAGE_STRICT=1`
  refuses). The choices: accept the gaps as the disc-only build's (a ledger
  entry naming what they break), derive each by a rule where one exists
  (none read yet), or require the PC's exe for those tables (a cache built
  with it carries them). Your call after the play above. **Answered in part
  (2026-10-12, the PSP-only round): "fill them"** - the four largest are
  filled by rule from every disc (DIV-0101 amended,
  [`disc-gap-fills.md`](disc-gap-fills.md)); what is left (the formation names, the long tail of section 3
  there) is still this call. The event battles' track half was filled by
  rule on 2026-10-17 after players reported it (issue #83, D316): each
  boss file's own music through the disc's song table.
- **The faerie village's label group writes names one record late** (LG,
  2026-10-12; [`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4; DIV-0064's
  group 16): the PC record is name first at `0x653200`, `labels.cpp` writes
  name r to `0x653214+20r`, so record 0 is never named and the "unnamed"
  record 59 is the area-limit table. A `labels.cpp` + `loc_build.py` fix
  changes the PC path's output: a ledger entry if you say fix it. (Whether
  a player sees it: the village's member list - look there first.)
- **The porting house's logo movie** (`LOGOS.AVI`, the second of `Fmv_Play`'s
  two calls; no disc counterpart): you asked 2026-10-12 whether there is a
  reason to keep it; none technical was found (it is the porting house's
  branding, not Capcom's game; the sibling's fidelity invariant is not ours).
  Drop it from our build by default with a ledger entry, or keep it on the
  PC-install path?
- **Six doubtful `.bss` words CP holds as pointers** (24 bytes,
  `exe-import-engine.md` 7.2) and **DIV-0055's 94 plate bytes** with no
  transform (the PSX source unmeasured): small, listed so they are not lost.

- **34 French and 11 German messages the PC build leaves Chinese** (2026-10-10,
  [`loc-build-disc-only.md`](loc-build-disc-only.md), the disc-only build's
  open question 1): where a disc's slot table is shorter than 256 entries,
  `loc_build.py`'s PC path measures a slot's offset against the PC's table
  size, so the disc's first messages look out of range and the Chinese stays.
  The disc-only path reads them right. Fixing the PC path changes today's
  fr-FR / de-DE output (a ledger entry, the layers rebuilt); the owner says
  whether, and whether to look at one of them first (the doc names the areas).
- **What the engine holds as layout for a disc-only build** (2026-10-10,
  [`exe-import-engine.md`](exe-import-engine.md) section 5, extending step 8's
  call 4): the 9,142 code pointers, the 117 pooled zero objects past the last
  named table, and the 147 `.bss` / 63 `.rdata` pointer words can come from no
  disc. Hold them in the engine (as `exe-pointers.tsv` already holds addresses,
  never bytes), or stop at "the PC install only" for them? Not blocking: the
  PC-sourced build is byte-identical either way.
- **Decided 2026-10-10: minimp3 replaces Capcom's MP3 decoder** ("I agree
  with using minimp3 unless there is a known decoding bug that has been
  resolved" - none: [`mp3-decoder-choice.md`](mp3-decoder-choice.md), 42 of
  the decoders' 57 items token-identical, the 15 others casts and naming but
  for dr_mp3 keeping the bit reservoir on a `pcm == NULL` call). Recorded in
  [`platform-layers-plan.md`](platform-layers-plan.md) 2.4; nothing to hear
  until it is built, then the ledger entry's PCM bound and the loop seams.

- **`symbols.toml` quotes table values** ([`exe-tables-by-build.md`](exe-tables-by-build.md)
  section 7, 2026-10-08): about 25 `[[data]]` evidence strings carry six or
  more of a table's numbers in a row (`Battle_DamageVarianceTable`,
  `Steal_RateTable`, `Field_MoveSpeeds`, `WorldMap_Records`, the CLUT tables
  and others listed there), older than the step that found them. Rule 1 says
  values from the game stay out of the repository; the fix is to replace the
  quoted numbers with what they mean and how they were read. **Done
  2026-10-08** (the owner: the values are public knowledge, recreated in
  FAQs, taken from the sibling before the discs verified them - sunset them):
  25 entries reworded in the tree, the addresses, counts, strides, readers
  and PSX cross-references kept, the value runs replaced by their shape
  (eleven are tables of game numbers, the rest engine constants - CLUT
  strides, op lengths, direction angles, cell codes). The seven address
  lists the regex also caught (`Gte_Vertices`, `BattleObj_StateTable`,
  `MenuList_Kinds`, `Scena00_Runs` and three more) are function and data
  addresses, not values, and stay. **Not done, the owner's call:** a history
  rewrite - the strings stay in `git log`. Recommended against: the figures
  are public, the last rewrite cost every cited hash (CLAUDE.md rule 7), and
  the licensing path rests on the tree's engine / data split, not the
  history's. Also open: the same kind of value runs in `docs/` (known-defects,
  the group docs, `importer.md`'s list of the eleven world-map areas) were
  not scanned.

- **Step 3's four calls** ([`importer-transforms.md`](importer-transforms.md)
  "The owner's calls", 2026-10-08): (1) a disc-only player's dial page - the
  PlayStation's buttons as built, or a keyboard-legend layer independent of
  the PC's art; (2) the 40 French and 54 German enemy names over 8 bytes -
  Chinese on the PC today, blank over the cache: abbreviate in `loc_build.py`
  or widen DIV-0053's draw; (3) the 14 port-edited arenas - read what the
  edits fix, or accept the disc's decode with a ledger entry; (4) `AREA004`
  from a Western disc alone brings the re-texture (DIV-0080's note) - accept
  as a by-source difference, or refuse. Nothing to look at until the engine
  reads the cache.

- **Step 8's four calls** ([`exe-import.md`](exe-import.md) "The owner's
  calls", 2026-10-08): (1) `recipes/exe-pointers.tsv` in the repo - 3,134
  lines of addresses and counts from the executable, no values; kept under
  rule 1's reading, say if it should be generated on the player's machine
  instead; (2) blank the PC-source image's six name tables and move the
  Chinese names to `loc/zh-CN/`, so `base/exe/` is one thing from every
  source; (3) a PSP-only `base/exe/` carries the PSP's level table and
  consumable 87 and no `sin_table` - accept and ledger, or require a PSX disc
  or the PC; (4) the data pointers from a disc - held by the engine, or a
  rebuild transform (93 % exact; recommended).

- **Decided 2026-10-08: the disc's music by default** ("default to disc-delivered
  music if available; there doesn't seem to be a compelling reason to use the
  PC-delivered music unless you absolutely have to"). Recorded in
  [`unified-data-plan.md`](unified-data-plan.md) section 6; step 9 moves up,
  the loop table stays as the PC-only fallback. What it asks next: the
  listening session that gates step 9 (`bgm-comparison.md` 10), and whether
  the 11 jingles (step 6's call 3) follow the same rule - from `S_XA00.STR`
  on the disc, by the engine playing a kind-0 stream from PCM.

- **Step 6's three calls** ([`sound-import.md`](sound-import.md) section 6,
  2026-10-08): (1) the PAL discs' 8 swapped area banks - refused by hash
  today, so a PAL-only player lacks 8 containers' banks; accept as that
  build's? (2) the port's converter wraps where the SPU clamps - 9 sample
  values in 5 sounds, a full-scale click on the PC (`AREA000`'s bank holds
  one: a listen first); clamping is a DIV and those banks stop being the
  PC's bytes; (3) the 11 jingles for a disc-only player - an MP3 encoder in
  the import (licensing), the engine playing kind-0 streams from WAV (`PURE`
  / `KARA` 47 MB each, streamed), or PC-only.

- **Step 4's six calls** ([`opt-layers.md`](opt-layers.md) "The owner's
  calls", 2026-10-08): `psp-art` one layer or two (two chosen: P6's ten rows,
  `psp-tiles` the rest); the PSP map bands a layer (chosen) or a rule; what
  the launcher offers (recommended: a "PSP extras" group of boxes); the
  `en-150` names layer the 8 renames only (chosen) or PSP-EU's whole tables;
  the PSP content left out (logo, title page, button labels, `SCENA17`, P8,
  the level table, consumable 87); a preset with layers on. **To look at once
  built:** Stallion with `psp-art` on and off (fight 24, area 67; area 166's
  fight 48), ability 116's banner under `psp-names-en-150`, `AREA128`'s dock
  wide with `psp-maps`, a few `psp-tiles` areas.

- **The world map's gauge words in French and German** ([`importer.md`](importer.md)
  section 3, 2026-10-08): the port's dial page (the keyboard legend, kept for
  every language) restyled the ENGINE / OVER HEAT gauge frames with the words
  in English, as JP and US have them. The French and German discs translate
  them (MOTEUR / SURCHAUFFE) in 7-row strips inside the frames, where the
  port's swirl overlaps the first letters. Splicing the disc's word rows into
  the port's page is possible with a rectangle diff (no tool yet), as a
  ledgered divergence under the French and German overlays only. **Wants the
  owner's eye on a render first**, then a yes or no. Seen on the way, and settled
  the same day: four areas (`AREA104`, `127`, `134`, `164`) whose plate pages
  the US disc left in Japanese are **not a gap** ([`importer.md`](importer.md)
  section 3).

- **The base tree's version of the Western data rows** ([`region-diff.md`](region-diff.md),
  2026-10-06): every build after JP - US, FR, DE and both PSP discs - changes
  `AREA004` section 8 (992 bytes) and one cue byte in 65 dragon and Ryu sound
  banks; only the PC, built from JP, lacks them (two more kinds are Western-
  or FR/DE-only). Should the cache's `base/` keep JP's rows, or take the later
  version as a ledgered divergence? **Read 2026-10-06** (section 8 there):
  `AREA004` is Dauna Mine's minecart area; its section 8 walls 72 cells that
  fill gaps between wall stubs JP already placed (along the raised strip's
  east edge with a doorway kept, the north end of a column on its west side,
  and the corridor's bottom edge) plus a 30-cell texture fix, and section 10 closes the same cells
  to battle placement - **a collision bug fix**, taken by every later build
  (the PSP took the walls but not the placement half). The cue byte is a PSX
  sound-priority fix the PC's sound code never reads; the PAL sample swap is
  regional with no clear purpose (keep JP/US). The owner's rule: a bug fix is
  worth keeping as the default. Two ways to build it: an overlay chunk from
  the player's own later disc (needs that disc), or our own fix in code - the
  72 cells blocked and the 8 placement cells closed by coordinate, a
  divergence with the later discs as precedent, for every player. Area 4 is
  in the attract demo, so the state hash's reference runs want the switch off.
  The owner chose code; **built as DIV-0080** (`BOF3X_AREA4_WALLS`, the table
  proven against the US and German discs offline). **Changed 2026-10-10** at
  the owner's asking (no Capcom table in our code, if we can): a rule from the
  JP map's own data was looked for and **does not exist** - 58 other open
  cells in area 4 have the same local heights and neighbours as the 72
  ([`region-diff.md`](region-diff.md) 10.1) - so the table is gone and the
  walls are the **`area4-walls` layer** from the player's own US, European,
  French or German disc. **On by default since 2026-10-10** (the owner's
  word): a build with a Western disc builds it, `install` installs it, and
  the launcher plays it whenever it is installed and the ini's `opt=` is
  empty; `opt=none` turns it off (DIV-0080). **Without a Western disc, area
  4 is the shipped open map.** **Owed the
  owner's eye:** with the layer on, in the minecart area, walk the raised
  strip's east edge and the corridor's bottom edge - blocked with the layer,
  open without it.
- **Stallion's PSP recolour as an option** ([`psp-stallion.md`](psp-stallion.md)):
  palettes only (two rows in areas 67 and 166, plus the three variants), the
  fight's code identical. A toggle is a palette layer from the player's own
  PSP disc (about a day, plus a second overlay prefix in `LoadDatFile`) and
  the renamed attack hours on top through the name table. The agent
  recommends an option, not a default. **The renders are in
  `analysis/stallion/`** (`AREA067_stallion_cells_jp_left_psp_right.png`,
  `AREA166_palettes0to3_jp_left_psp_right.png`) for the owner to look at.
  **The owner recorded the route the same night**: `tools/recipes/stallion.txt`
  (956 lines, the pre-fight, the transform scene and the fight; `# save
  stallion`, imported from their slot 0), so the live check exists once the
  option is built. **The Holy Mantle lead is refuted**: nothing
  to toggle.

## Seen by the owner (struck)

- **DIV-0076, the save slot's summary** - confirmed 2026-10-06: a save with
  another member leading shows Ryu's level and name together on the load
  screen.

## Debt 3: measured by a log line, review the log

- `PartyAction_SpawnKind1B` `0x5252B0` now logs `debt3` when
  `Effect_FindFree` answers 0xFF (added 2026-10-06, `rest_1f.cpp`; behaviour
  unchanged). After a play session, grep `bof3x.log` for `debt3`. A hit means
  the wait polled a packet-pool byte ([`rest_1d.md`](rest_1d.md) L1); the
  owner then decides whether the action should end at once with no effect
  placed (a ledger entry) or stay as Capcom's.

## Parked, documented, revisit if ever seen

- **The `0x91` camp cells** ([`rest_2d.md`](rest_2d.md) L1,
  [`psx-twin-check.md`](psx-twin-check.md)): 91 real cells where the camp
  opens though the compare says refuse; Capcom's on both machines. The owner
  (2026-10-06): leave it, revisit later. A fix is a one-line divergence.
- **The one-texel POLY_FT3 colour** ([`d3d-rest.md`](d3d-rest.md) D-b): a
  triangle whose corners share one texel is flattened to a colour read from
  the PSX offsets of a primitive this port widened to floats, then the wrong
  nibble. No route reaches it, no builder of one found. The owner
  (2026-10-06): leave it documented; review if a miscoloured speck is ever
  spotted.

## Decided and built, not yet seen live

- **The launcher's Music, Cache folder and PSP extras boxes** (2026-10-10,
  [`launcher-settings.md`](launcher-settings.md) 3.1; the owner's call 3 of
  [`opt-layers.md`](opt-layers.md)): driven by a script, never by hand - a
  look at the dialog's layout at the owner's DPI, Browse... for a cache
  folder, and the PSP boxes with the PSP layers installed.

- **`Cfg_Load`'s key-line overrun** ([`shell.md`](shell.md) section 5): the
  owner (2026-10-06) wanted overrun protection; **built the same day as
  DIV-0078** (a zero-filled 32-entry table of our own, lines past it ignored;
  `shell` self-test 0 mismatches with the switch off). Visible only with a
  hand-edited `BOF3.CFG`; a check is to add 25 key lines and see the game
  still start.

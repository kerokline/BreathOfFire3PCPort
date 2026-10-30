# The exe-gap round: the disc-built image's last differences, and STRICT as the default

**Status:** DRAFT (framed 2026-10-20 at the owner's word, not staged: no
groups cut, no briefs, nothing launched; the same day stage 0, D266 and the
ending roll were done on `fix/d266-title-mark`, sections 1 and 3). The idea is
[`IDEAS.md`](IDEAS.md) I43, steps 3 and 4. The rounds before it are the
install-gap round ([`install-gap-round.md`](install-gap-round.md), PR #91)
and the second pairing pass ([`install-gap-pairs.md`](install-gap-pairs.md)
8, merged with PR #94).

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

## 1. Where it stands (measured 2026-10-20, `main` `84e43c32`)

`gap` ranges per build in `recipes/exe-gaps.tsv`, as ranges / bytes:

| Build | game-value | port-relaid | language-zero | disc-unplaced | total |
|---|--:|--:|--:|--:|--:|
| psx-us | 66 / 1,065 | 8 / 97 | 2 / 10 | 0 | 76 / 1,172 |
| psx-eu-en | 63 / 1,110 | 8 / 97 | 2 / 10 | 2 / 45 | 75 / 1,262 |
| psx-fr | 65 / 1,151 | 8 / 97 | 2 / 10 | 1 / 37 | 76 / 1,295 |
| psx-de | 66 / 1,162 | 8 / 97 | 2 / 10 | 2 / 45 | 78 / 1,314 |
| psx-jp | 45 / 525 | 8 / 97 | 5 / 51 | 0 | 58 / 673 |
| psp-jp | 42 / 430 | 7 / 67 | 5 / 51 | 0 | 54 / 548 |
| psp-eu | 43 / 437 | 7 / 70 | 2 / 10 | 0 | 52 / 517 |

44 ranges are gaps on all seven builds. **This file predated the second
pairing pass**: PR #94 carried its new rows into the recipes' sources but did
not regenerate the recipes. **Stage 0 is done** on `fix/d266-title-mark`
(2026-10-20, the recipes regenerated over the seven discs with D266's rule):
`exe-imm.tsv` + 16 `immref` rows, and the `gap` rows per build now

| Build | before | after |
|---|--:|--:|
| psx-us | 76 | 72 |
| psx-eu-en | 75 | 71 |
| psx-fr | 76 | 72 |
| psx-de | 78 | 74 |
| psx-jp | 58 | 53 |
| psp-jp | 54 | 51 |
| psp-eu | 52 | 49 |

none new (`exe_tables.py check` 0 errors).

Every row already has a status in
[`install-gap-census.md`](install-gap-census.md) 8 (FIXED, HARMLESS,
NEEDS FIX, OWNER CALL). The read watch
([`install-gap-reads.md`](install-gap-reads.md) 9) found that, over the
attract and the ten routes on all seven installs, play reads only two `gap`
ranges while zero: the owner's two (section 3).

## 2. The three blockers (install-gap-reads.md 9.7, brought up to date)

1. **STRICT refuses every `gap` range, read or not.** With 49..74 per build
   (after stage 0, section 1)
   it would refuse all seven installs at start. The step means "a new gap
   fails loudly", so the switch has to tell an accounted row from a new one.
2. **The owner's two read rows**, ~~D266~~ (done 2026-10-20, section 3) and
   the empty save slot (section 3).
3. **Coverage.** "Not read" rests on eleven routes (19 areas, four battles);
   48 of the 88 unread rows were sampled. psp-jp stopped at five routes on
   D321's Fatal; **D321 is fixed since 2026-10-18**, so psp-jp can be
   covered now.

Also in the way of a Japanese disc-only default, though not STRICT's:
**D280**, the `ja-JP` layer's kinds 7..15 (the exe's own strings read as
zero on psx-jp and psp-jp).

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

**Open, each with its doc:**

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
- **`Area65_DriftUV`** and the other `port-relaid` rows (7..8 per build),
  which the owner holds.

## 4. The work, by kind

Not yet cut into groups. Each kind is a candidate group or stage.

**Stage 0 - regenerate and recount** (mechanical, first; **done 2026-10-20**, section 1). The seven discs'
recipes after PR #94 (`psp_exe.py recipe`, `exe_tables.py recipe`,
`exe_own.py measure`, `exe_tables.py gaps`; HANDOFF "How to run things"),
`tools/exe_scan.py` clean, section 1's table again. Expected: four new
`immref` rows on psx-jp, two on each other build.

**A - the answered owner rows.** ~~D266's twin (small)~~ (done 2026-10-20,
the `titlemark` rule, section 3); D280's kinds 7..15 (large, its own group
or two). Each with its ledger amendment.

**B - placement with no owner** (the tools' reach):
- the rows [`install-gap-pairs.md`](install-gap-pairs.md) 8.5 leaves:
  `Area115_Directions`, `Area121_BandHeights`, `Area135_EffectKinds` (a run
  offset by leading zeros, `Area141_CellEntries`' shape: one `find_imm`
  rule for both), `Scena02_EffectX`, `Scena03_Bob4` / `Bob2`,
  `LinkedObjectD_Amounts`;
- `build_twins`' tie-break (`BossGaist_Anims+0x2`, `BossTorch_Sounds` place
  on psx-jp only);
- the rows under `find_imm`'s four-byte floor (`EffectKind75_States+0xC`,
  `BoneDanceShake_Counts`, `GameMode3_Steps+0xC`) and the `EffectKind18Sub*`
  rows whose twins' bytes differ;
- census 8.4..8.6's NEEDS FIX items (song 166 / D320 is not an exe row and
  stays with the audio list).

**C - the unread rows, accounted.** Every remaining `gap` row gets one of
four answers, written next to it: **placed**, **a rule** (`exe_own`),
**the port's own** (the engine supplies it), or **accepted unread** (no
reader, or a reader no route reaches, with the evidence). The
"no reader" rows (`Area96_Handlers+0x184`, the `Area154_ChoicePairs` /
`Area172_EffectStates` / `Area196_Keys1` tails) are the first of the last
kind. No PC bytes copied into code, whatever the answer (the owner's stance,
2026-10-10).

**D - STRICT narrowed, then on.** A reviewed list beside `exe-gaps.tsv`
(for example `recipes/exe-gaps-accepted.tsv`: build, range, answer, the doc
that settled it), generated rows checked against it by `exe_scan.py`, and
`exe_image.cpp` refusing only a `gap` range **not** on the list. Then the
default flips for disc-only installs, with the launcher's message for a
refused image (rebuild the install, as `FilledSince()`'s does). A code
change and a DIV-0101 amendment.

**E - coverage.** psp-jp's eleven routes under the read watch now that D321
is fixed; the owner's newer recordings (the radiation room, the minigame
starts once recorded) as routes; the attract. The unsampled 40 of 88 unread
rows sampled. A clean watch is necessary for the default, not sufficient:
C's "accepted unread" answers carry the rest.

## 5. Order and method

Stage 0 (done 2026-10-20), then A (~~D266 first~~, done 2026-10-20; D280)
and B side by side, then C
over what is left, then E's runs on the result, then D last. The method is
the install-gap round's: agents in worktrees by method, one merge at a time,
`exe_scan.py` and the recipes' regeneration after every merge, the
read-watch batch and the A/B/C attract and routes as the end batch
([`abc-harness.md`](abc-harness.md); roots under `D:\bof3x-runs`).

## 6. Done when

- `exe-gaps.tsv` regenerated, every row on the reviewed list with an answer
  and a pointer, no NEEDS FIX left;
- the watch over the routes and the attract on all seven installs reads no
  `gap` range while zero;
- STRICT on by default for disc-only installs, all seven starting under it,
  and a planted new gap refused at start (the negative control).

## 7. For the owner, before staging

- The open calls in section 3 (the ending roll is answered).
- Whether D280's `ja-JP` layer is this round's or its own (it is mostly
  language work, and the Japanese builds' default could wait for it).

# σ, the Config as one permutation, and the starting Config by source

**Status:** BUILT (2026-10-17, group SG of
[`button-unification-round.md`](button-unification-round.md) 2, branch
`buttons/sg`). Headless self-tests pass. Not played. Ledger drafts A and B
(section 6) are lettered; the coordinator numbers them at the merge.

The design is [`button-roles-by-release.md`](button-roles-by-release.md) 7,
with the owner's three answers of 2026-10-17 (that doc, 8). The owner
confirmed the reading of answer 2 the same day: **a PC install with a Western
layer or a Western disc starts with Japan's buttons**, because the PC's own
data counts as a JP source.

## 1. What is built

- `src/input/sigma.{h,cpp}` holds the table `σ`. It also holds the releases'
  maps, the release's starting words, and the install's release read from
  the cache manifest.
- The three writers of the Config words call it after their own stores:
  `Boot_Task` (`src/game/mode_flow.cpp`), `LoadMenu_Apply`
  (`src/game/save_menu.cpp`) and the rebind in `ConfigScreen_Controller`
  (`src/game/rest_4f.cpp`).
- `sigma::Arm` runs in `inject_all.cpp` after every module's self-test. It
  runs the self-test, reads the release and sets the two switches. Before
  `Arm` the writers do exactly what Capcom's do, so the shadows of
  `mode_flow`, `save_menu` and `rest_4f` still compare Capcom's stores.

No reader of `Field_ConfirmButtons` / `Field_CancelButtons` changes. No icon
and no hard-coded test asks `σ` yet: the stage-B groups do that.

## 2. The table

The game's code is Japan's on every install, so `σ` is kept in Japan's terms.
For each of the six lines, Japan's default button maps to the button the line
is on now:

| Line (word) | Japan's default | `Sigma(that button)` |
|---|---|---|
| Speak (0) | circle `0x20` | word 0's button, L2 / R2 set aside |
| Move (1) | square `0x80` | word 1 |
| Action (6) | cross `0x40` | word 6 |
| Menu (2) | triangle `0x10` | word 2 |
| View (3) | R1 `0x08` | word 3 |
| Change (4) | L1 `0x04` | word 4 |

The API:

- **`Sigma(bits)`** and **`SigmaInverse(bits)`** take one bit or a mask.
  Each of the six bits is mapped; start, select, the d-pad, L2 and R2 pass
  through unchanged. A hard-coded test of Japan's bit `b` asks `Sigma(b)`,
  and so does an icon from the PC's own text or art.
- **`SigmaFrom(R, bits)`** is `Sigma(M_R^-1(bits))`, for an icon from release
  R's text or art. `M_R` is the identity for JP, PSP-JP and the PC, `P` for
  the PlayStation West, and `S` for PSP-EU. Answer 3 is the caller's to
  apply: PSP-EU's well (area 68) and ram (area 121) pass `kWest`.
- **`InstallRelease()`** is the release `Arm` read.

**Why Japan's terms, not the release's.** The round doc says that `σ` is
"read against the install's release defaults", and that it is the identity
at those defaults. It also says that a hard-coded test of Japan's bit `b`
tests `σ(b)`, and that an icon from R's text is `σ(M_R^-1(b))`. On a JP-based
install the three statements agree. On a Western disc-only install they do
not.

That install now starts with the West's Config (draft B), with Speak on
cross. The PlayStation US release tests `P(b)` wherever Japan's tests `b`:
the 44 changed tests of `button-roles-by-release.md` 3. So the unified flow
needs `Sigma` to equal `P` there, which means a table relative to Japan's
defaults. The release-relative `σ` of the round doc is `SigmaFrom(R, ·)`,
which is the identity at R's defaults. The self-test checks both views
(section 5).

**Refusal.** Each line's word, with word 0's L2 / R2 set aside, must hold
exactly one of the six bits, and the six words must all differ. If not, the
writer stops with a Fatal that names the writer, the line, the word and its
value, and the remedy. After a load, the remedy is to hold Start while the
save loads (`LoadMenu_Apply`'s reset), or to use
`BOF3X_ORIGINAL=ButtonSigma`.

The rebind can only swap, so it always leaves a permutation. The PlayStation
saves are the same. Only a hand-edited save can be refused, or a save from
the disc-only zeros of [`controls.md`](controls.md) 2.1 (fixed 2026-10-12).

Under `BOF3X_ORIGINAL=ButtonSigma`, and before `Arm`, `Sigma` is the
identity, which is the original's flow. When `ButtonSigma` is on, asking
`Sigma` before any writer has built the table is a Fatal. Nothing in the
game can ask before `Boot_Task`.

## 3. The defaults by source

**The release's nine words.** These are Japan's rule (`FieldButtonDefaults`
in `src/hook/exe_own.cpp`, equal to `BOF3.exe`'s `0x656AEC`) with each line's
button mapped by `M_R`. The values are not copied from the discs. Both maps
are stated as rules in `sigma.cpp`: `P` circle -> cross -> triangle ->
square -> circle, and `S` circle <-> cross, triangle <-> square.

| Word | Japan (`pc-zh`, `psx-jp`, `psp-jp`) | PlayStation West (`psx-us`, `psx-eu-en`, `psx-fr`, `psx-de`) | PSP-EU (`psp-eu`) |
|---|---|---|---|
| 0 Speak | circle + L2 R2 | cross + L2 R2 | cross + L2 R2 |
| 1 Move | square | circle | triangle |
| 2 Menu | triangle | square | square |
| 3 View, 4 Change | R1, L1 | R1, L1 | R1, L1 |
| 5 | select | select | select |
| 6 Action | cross | triangle | circle |
| 7 confirm | = word 0 | = word 0 | = word 0 |
| 8 cancel | = word 6 | = word 6 | = word 6 |

**Checked against the discs.** On 2026-10-17 the nine halfwords were read
from both PSP `BOOT.BIN`s at the offsets in `button-roles-by-release.md` 2,
with a scratch ISO reader and nothing kept. PSP-EU's are the right-hand
column exactly, with L2 / R2 in words 0 and 7. PSP-JP's equal Japan's. The
JP and US columns rest on [`controls.md`](controls.md) 2.1's reading of
`SLPS_009.90` and `SLUS_004.22`. EU, FR and DE rest on
`button-roles-by-release.md` 2.

**The Start-held reset** in `LoadMenu_Apply` uses the same buttons with no L2
/ R2 in words 0 and 7, as the PC's reset `0x588629..0x588670` writes
Japan's.

**The release of an install.** `Arm` reads `BOF3X_CACHE`'s `manifest.toml`
through the shared reader (`common/cache_manifest.h`) and takes its
`[[source]]` builds. The order is answer 2's:

1. **Japan**, if any source is `pc-zh` or `psx-jp`. The PC counts as a JP
   source, which is the owner's confirmed reading:
   `kPcIsJapanSource = true` in `sigma.cpp` is the one line to flip.
2. **The PlayStation West**, if any source is `psx-us`, `psx-eu-en`,
   `psx-fr` or `psx-de`.
3. **The PSP**: `psp-jp` (Japan's map) before `psp-eu`.

With no cache named, or a cache without a manifest, the install is a PC
install, so Japan. A manifest the reader refuses, a manifest with no
sources, or a source id this table does not know is a Fatal, because the
release cannot be guessed. The owner's play install (`pc-zh` and all seven
discs) is Japan. A Western disc-only install is the West. A PSP-EU-only
install is PSP-EU.

**Where the starting words are written.** `Boot_Task` first copies the
eighteen bytes of `0x656AEC`, as before. For a non-Japan release it then
writes the release's nine words over them. The `0x656AEC` region itself is
unchanged. It is the engine's own and is verified byte for byte against
`BOF3.exe` (`exe-engine-own.md` 2), so it must stay the PC's.

**`Shell_MapDefaultKeys` is not changed.** The round doc names it beside the
boot defaults. It writes `Key_TableDefault`, the keyboard's map from keys to
PlayStation bits, which is the physical layer. It does not map lines to
buttons. The release's Config already moves Speak to cross, so mapping the
keys by `P` as well would apply the release twice: the key that sends cross
would then send something else. The keyboard and the pad keep sending the
same PlayStation buttons on every install, and the Config decides which line
each button serves, as on the PlayStation.

## 4. The three writers

| Writer | Original | Ours | What runs after its stores |
|---|---|---|---|
| the boot defaults | `Boot_Task` `0x496B60`, the copy `0x496BB8..0x496C25` | `mode_flow.cpp` | `BootDefaultsWritten`: the release's words (B), then A |
| the load | `LoadMenu_Apply` `0x5885D0`, the stores `0x58860E..` and the Start-held reset `0x588629..0x588670` | `save_menu.cpp` | `LoadApplied(reset)`: the reset's words by release when Start was held (B), then A |
| the rebind | `ConfigScreen_Controller` `0x4613B0`, the swap `0x461459..0x46153B` | `rest_4f.cpp` | `Rebound`: A |

Here **A** means that word 7 is set to word 0 (Speak's, with its L2 / R2),
word 8 is set to word 6 (Action's), and the table is rebuilt. No other store
to words 7 or 8 exists in `.text` (`button-roles-by-release.md` 4).

At every release's defaults, word 7 already equals word 0 and word 8 already
equals word 6, so A changes nothing until a line is swapped. The rebind's
own L2 / R2 handling of word 7 (`0x46148C`, `0x461534`) still runs, and its
result is then overwritten by word 0.

## 5. The self-test (`BOF3X_SHADOW=sigma`)

The self-test is pure: no game state is read except the `0x656AEC` region.
It checks the following:

1. **Each release's defaults, and its Start-held reset** (6 tables).
   - Japan's words equal `0x656AEC`'s eighteen bytes.
   - The West's and PSP-EU's words are as the table above states them by
     name.
   - Words 7 / 8 equal Speak's / Action's, so A writes the originals.
   - L2 / R2 are in word 0 at boot and absent after a reset.
   - `σ` is a permutation, `σ` equals `M_R`, `SigmaFrom(R)` is the
     identity, and `M_R^-1` undoes `M_R`.
2. **Every Config the rebind can make**: all 720 assignments of the six
   lines to the six bits, from each release's words (2,160 tables).
   - Each is a permutation: onto the six bits, `SigmaInverse(Sigma(b)) = b`
     and `Sigma(SigmaInverse(b)) = b`.
   - Bits outside the six pass through (masks in steps of `0x107`).
   - Each line's Japanese button maps to the line's button now.
3. **The 15 swaps of two lines from each release's defaults** (45 tables).
   The two images trade places and nothing else moves.
4. **Eleven non-permutations are refused, each with a reason**: two lines on
   one button (three ways), a line on nothing (two), on two buttons, on
   select, on start, on L2, on up, and word 0 holding only L2 / R2. In the
   game the writer's Fatal carries that same reason.
5. **The release by source** over 13 source lists, including an empty list
   and an unknown id, which are refused.

The run ends in a Fatal on any mismatch, or unless the counts are 2,211
tables and 11 refusals.

## 6. Ledger drafts

### Draft A: the Config's six lines as one permutation; confirm on Speak's button, cancel on Action's

- **Date:** 2026-10-17
- **Subsystem:** input, the Config words (`src/input/sigma.{h,cpp}`; the
  writers `Boot_Task` `0x496B60`, `LoadMenu_Apply` `0x5885D0`,
  `ConfigScreen_Controller` `0x4613B0`). **Amends DIV-0051**: the
  controller panel's icon column was the first site keyed on the mapping.
  From here every icon and every hard-coded test is meant to ask one table,
  `σ`, and DIV-0051's column is that table's first reader. Words 7 and 8 are
  named here as Speak's and Action's.
- **Tier:** Intent. The owner, 2026-10-17: unify every button flow (answer
  1, `button-roles-by-release.md` 8).
- **Original behaviour:** the Config screen swaps buttons between its six
  lines' words only. `Field_ConfirmButtons` (word 7) and
  `Field_CancelButtons` (word 8), which every menu tests, are written only by
  the boot defaults and the load. So a player who moves Speak off circle
  still confirms every menu with circle. The same holds on the PlayStation.
- **New behaviour:** after each of the three writers, word 7 is Speak's word
  (with its L2 / R2) and word 8 is Action's. Menus confirm on whatever
  Speak is on and cancel on whatever Action is on.
  - The six words are read as a permutation `σ` of the six face and shoulder
    bits, in Japan's terms. The stage-B sites ask it through `Sigma` and
    `SigmaFrom`.
  - Six words that are not one button each (a hand-edited save) are refused
    with a Fatal that names the remedy. They are not guessed at.
  - At every release's defaults nothing changes: words 7 / 8 equal the
    originals and `σ` is the release's own map.
- **Rationale:** the owner's answer 1. One flow, so that an icon that
  follows the Config tells the truth in the menus too.
- **Also in the PSX version?** The PlayStation has the same third flow on
  every release, so this is a change to Capcom's design, not a port repair.
- **Verification:** `BOF3X_SHADOW=sigma` (section 5), and the shadows of
  `mode_flow`, `save_menu` and `rest_4f`, which compare Capcom's stores
  before `Arm` (section 8).
- **Reversible?** `BOF3X_ORIGINAL=ButtonSigma`. A save written under A
  keeps its words 7 / 8 as saved. Holding Start while it loads writes the
  defaults.

### Draft B: the starting Config by source

- **Date:** 2026-10-17
- **Subsystem:** the boot defaults (`Boot_Task` `0x496B60`, after its copy
  of `0x656AEC`) and `LoadMenu_Apply`'s Start-held reset
  (`0x588629..0x588670`). The release comes from the cache manifest's
  sources (`src/input/sigma.cpp`).
- **Tier:** Intent. The owner's answer 2 (2026-10-17): an install starts
  with the defaults of the release it was built from.
- **Original behaviour:** the PC starts every New Game with Japan's buttons:
  Speak and confirm on circle, Action and cancel on cross, Move square, Menu
  triangle. Since the disc-only round (`controls.md` 2.1), a disc-only
  install starts the same way.
- **New behaviour:** the nine words are the release's, with sources taken in
  the order JP, then the PlayStation West, then the PSP.
  - **The PC's own data counts as a JP source**, confirmed by the owner on
    2026-10-17: a PC install with a Western layer or a Western disc starts
    with Japan's buttons, so the owner's install and every PC install are
    unchanged.
  - A Western disc-only install starts with the West's buttons: Speak and
    confirm on cross, Move circle, Menu square, Action and cancel on
    triangle.
  - A PSP-EU-only install starts with PSP-EU's: Speak and confirm on cross,
    Move triangle, Menu square, Action and cancel on circle.
  - The Start-held reset writes the same buttons without L2 / R2 in words 0
    and 7, as the PC's reset does for Japan's.
- **Rationale:** the owner's answer 2. The text and art of a disc-only
  install are its release's, so its buttons should be too.
- **Also in the PSX version?** Each PlayStation release starts with its own
  defaults. This brings that back for disc-only installs, which the PC never
  had.
- **Verification:** the self-test's release-by-source cases (section 5).
  The live start of a disc-only install is owed (section 8).
- **Reversible?** `BOF3X_ORIGINAL=StartConfigBySource`. For the other
  reading of answer 2, `kPcIsJapanSource` in `sigma.cpp` is a one-line
  change.

## 7. What is not reached, and open points

- **A Western or PSP-EU disc-only install between this merge and HT's.**
  Draft B moves Speak and Action, and draft A moves confirm and cancel with
  them. But the hard-coded tests (fishing stage 1, the minigames, the dock)
  still test Japan's bits until HT asks `Sigma`. In that interval such an
  install has a split flow: Speak on cross, but the fishing casts on circle.
  The coordinator may want B's switch to default OFF until HT merges.
  Either way it is one byte (`g_by_source`).
- **Confirm on a shoulder.** Answer 1 taken literally: if Speak is put on
  R1, every menu confirms on R1. That is what "menus confirm on whatever
  Speak is on" says. Worth a line to the owner beside the swapped captures.
- **Not reached live: a disc-only start.** No Western or PSP-EU disc-only
  install was started. The release-by-source path is checked only by the
  self-test's 13 source lists and by the shared manifest reader. A
  manifest-only scratch cache cannot stand in for one, because
  `dat_cache` refuses it at start-up (DIV-0089, "no whole [cache] assets
  table") before `sigma::Arm` reads it. The first New Game on such an
  install should show Speak on cross in the Config panel. That is owed, and
  RC's swapped routes on the four install kinds cover it.

## 8. Verification

- **Build:** `cmake --build build`, no errors and no new warnings.
- **Headless, narrow** (`BOF3X_SELFTEST_ONLY=1
  BOF3X_SHADOW=sigma,mode_flow,save_menu,rest_4f,exe_own`, an ini-less copy
  of the launcher, the PC install): exit 0. The log shows:
  - `sigma self-test: 2211 tables ... 11 non-permutations refused; the
    release by source over 13 cases; 0 MISMATCHES`;
  - `mode_flow` 41,000 rounds, `save_menu` 66,000 and `rest_4f` 208,000, each
    0 mismatches;
  - `patch ON ButtonSigma`, `patch ON StartConfigBySource`, and the release
    line "Japan's ... no cache named: the PC's own data".
- **Headless, everything** (`BOF3X_SHADOW='*'`, the same copy):
  - `self-test only: done` and `inject: 10087 ours, 0 left original`;
  - 1,078 self-test lines report `0 MISMATCHES`, and none reports any other
    count.

  Windows Defender then quarantined this build's `bof3x-launcher.exe` and
  `bof3x-launcher-dev.exe` as `Trojan:Win32/Bearfoos.A!ml`: events 1116 /
  1117 at 14:47..14:48, in the scratch copy and in `build/`. The launcher
  process was killed while it waited. The game ran on to the end of its
  self-tests (pid 11652), and the log above is that game's. The launcher
  was relinked for the live runs below. No Defender setting was touched.
- **The state hash, identity Config** (`combat.txt`, the hosted build on
  both sides, a scratch copy of the PC install, `--lang en-US --wide 0`,
  `BOF3X_LAYERING=0`, `BOF3X_OPT=none`, the 173-range skip list):
  - two runs with both switches on (`on`, `onb`);
  - two runs with `--original ButtonSigma,StartConfigBySource` (`off`,
    `offb`);
  - `statehash.py check on onb off` and `check on onb offb`: **identical on
    all 2,561 compared ticks**;
  - `diff off onb`: identical on every tick.

  `on` and `onb` disagree with each other at tick 3 alone, on seven pages:
  the VRAM shadow, the sound page and `0x937000`. That is boot noise, and
  `off` against `on` shows the same seven. The route boots (`Boot_Task`)
  and loads a save (`LoadMenu_Apply`), so two of the three writers ran with
  the table rebuilt and words 7 / 8 rewritten: no state changed.
- **A swap, live** (a scratch recipe outside the tree: title, Config,
  Controller, cross pressed on Speak's line; the same four settings, both
  sides). `diff off on` gives **one page, `0x903000`, from tick 731 (the
  press) to the end**. The nine words in the raw dumps at tick 800:
  - off: Speak `0x43`, Action `0x20`, confirm `0x23`, cancel `0x40`. This
    is the original: the swap leaves confirm on circle and cancel on cross.
  - on: Speak `0x43`, Action `0x20`, confirm `0x43`, cancel `0x20`. Confirm
    is on cross, as Speak is, and cancel on circle, as Action is.

  At tick 700, before the press, both sides hold Japan's nine words.

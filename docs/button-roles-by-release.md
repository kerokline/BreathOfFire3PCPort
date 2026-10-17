# Button roles by release: is each release's button use internally consistent?

**Status:** MEASURED (2026-10-17, the owner's request after their answers to
[`key-icons-by-mapping.md`](key-icons-by-mapping.md) 10). Static only. The
scripts are in the session scratchpad (`keys/relscan.py`, `pspscan.py`,
`texticons.py`). They read the player's discs and print addresses, masks and
counts, never game data (rule 1). Nothing is built.

The owner (2026-10-17): unify every button flow. Check that each release's
button uses, dialogue and art are internally consistent with its own default
mapping, write the map per release (JP accept = circle, English = cross, ...),
and then let every icon point through the map to what the Config holds now
("all circles become Xs, all triangles become squares"). PlayStation icons
only, no keyboard keys.

## 1. The answer in short

- **Every PlayStation release is internally consistent.** US, EU, FR and DE
  share one map from Japan's buttons (`P`: circle -> cross, cross -> triangle,
  triangle -> square, square -> circle). Their defaults, all 44 changed
  hard-coded tests and all 31 dialogue mentions follow it. No test and no
  mention goes another way.
- **PSP-EU uses a different map** (`S`: circle <-> cross, triangle <-> square).
  Its defaults and the fishing's stage-1 test follow `S`. Two dialogue
  mentions do not: the well (area 68) and the ram (area 121) keep the
  PlayStation West's icons. Whether PSP-EU's code at those two follows its
  text or `S` is not settled (section 5).
- **PSP-JP is Japan's map** (its defaults equal JP's).
- **The menus' confirm and cancel never follow the Config** on any release:
  the Config swaps only its six lines' words. Words 7 and 8 are written only
  by the boot defaults and the load path (section 4). This is a third flow,
  with the minigames' and the fishing's hard-coded tests, that a Config swap
  leaves behind.

## 2. The defaults (each boot executable's nine pad words)

Found by shape in each boot executable or `BOOT.BIN`: nine halfwords with R1,
L1 and select in words 3, 4 and 5, word 7 equal to word 0 (with L2 / R2), and
word 8 equal to word 6.

| Role (word) | JP: `SLPS_009.90`, PSP-JP `BOOT.BIN`, the PC | US `SLUS_004.22`, EU `SLES_013.04`, FR `SLES_013.19`, DE `SLES_013.20` | PSP-EU `BOOT.BIN` |
|---|---|---|---|
| Speak (0) and confirm (7) | circle | cross | cross |
| Move (1) | square | circle | triangle |
| Menu (2) | triangle | square | square |
| Action (6) and cancel (8) | cross | triangle | circle |
| View (3), Change (4) | R1, L1 | R1, L1 | R1, L1 |

Places: JP `+0xECEE4`, US `+0xE956C`, EU / FR / DE `+0xE9A38`, PSP-JP
`BOOT.BIN +0x2DF320`, PSP-EU `+0x2DEB08` (file offsets). JP and US agree with
[`controls.md`](controls.md) 2.1's reading of the two boot executables.

The maps from Japan's buttons, as permutations of the face bits:

| JP button | `P` (PlayStation West) | `S` (PSP-EU) |
|---|---|---|
| circle `0x20` | cross `0x40` | cross `0x40` |
| cross `0x40` | triangle `0x10` | circle `0x20` |
| triangle `0x10` | square `0x80` | square `0x80` |
| square `0x80` | circle `0x20` | triangle `0x10` |

## 3. The hard-coded tests

**PlayStation** (`relscan.py`): every EMI section a release shares with JP,
each pad test (an `andi` within four instructions of a `lhu` of the pad
words) compared position by position, the map learned from the data rather
than assumed. For each of US, EU, FR and DE: 139 tests compared; 94 equal
with no face bit; **44 changed, every one by `P`**; one other, `GAME.EMI#1`
`0x800` -> `0x9FF` (start, not a face button, as
[`key-icons-by-mapping.md`](key-icons-by-mapping.md) 3 found for the US).
Four sections differ in count in all four: the name entry (`COMMU02`,
`COMMU02B`, `START`, `STATUS`) drops one triangle test, the rework the PC cut.
The four Western discs give the same 44 changes. This repeats the 10-13 US
measurement and extends it to EU, FR and DE.

**PSP** (`pspscan.py`): the code is in `BOOT.BIN` (the PSP overlays hold no
MIPS pad tests), with no fixed pad addresses to pin a test on. So every
`andi` with a face bit in PSP-JP's and PSP-EU's `.text` was aligned
(difflib on the masks' non-face structure): 2,024 aligned, 42 changed by `S`,
8 not. Seven of the eight sit where the alignment is broken (their
neighbours' address deltas jump by hundreds of bytes); the eighth is off by
one (JP `0x22055C` `andi 0x80` matches EU `0x22036C` `andi 0x80` exactly). The
fishing's stage-1 mask decides it: PSP-JP `0x74` becomes PSP-EU `0xE4`, which
is `S` (`P` would give `0xD4`). Weaker than the PlayStation's measurement,
since the count includes masks that are not pad tests.

## 4. Confirm and cancel ignore the Config

The Config's rebind (`0x461459..0x46153B`, [`controls.md`](controls.md) 2)
swaps buttons between the six lines' words (slots `00 01 06 02 03 04`) and
touches word 7 only to mask and restore L2 / R2 (`0x46148C`, `0x461534`).
Every store to word 7 or 8 in `BOF3.exe`'s `.text` (capstone over the whole
section): those two, the boot defaults (`0x496C25`) and `LoadMenu_Apply`
(`0x58860E`, `0x588661`, `0x58866A`). So a player who swaps Speak to
triangle still confirms every menu with circle. The same holds on the
PlayStation, whose defaults the PC copies.

## 5. Dialogue

**PlayStation** (`texticons.py`): an icon right after a colour control
(`{05:cc}` then a byte `0x80..0x83`, circle, cross, triangle, square), every
area text block (the EMI section loaded at `0x10000`). US, EU, FR and DE are
identical: nine areas, 31 mentions. Against the PC's own text (JP's buttons;
the colour-cued census of [`key-icons-by-mapping.md`](key-icons-by-mapping.md)
2.6), area by area:

| Area | Scene | PC (JP) | US / EU / FR / DE | By `P` |
|---|---|---|---|---|
| 8 | Bunyan's wood chop | cross x2 | triangle x2 | yes |
| 30, 89, 129 | fishing help | circle x5 each | cross x5 each | yes |
| 68 | the well | cross x2, circle x2 | triangle x2, cross x2 | yes |
| 75 | the rope pull | circle x3 | cross x3 | yes |
| 111 | the antenna | circle x1 | cross x1 | yes |
| 121 | ramming the Black Ship | square x1 | circle x1 | yes |
| 135 | the crane | circle x6 | cross x6 | yes |

The PC has one more mention (area 1) that no Western disc carries.

**PSP-EU** (its overlays under `USRDIR/USA/`, the same census, bytes `0x80..0x83`
read with the PlayStation's meaning): area 8 circle x2, the fishing cross x5
each, the rope cross x3, the antenna cross x1, the crane cross x6 - all by
`S`. **Two are not**: the well (68) triangle x2 and cross x2 (`S` gives circle
and cross), and the ram (121) circle x1 (`S` gives triangle). Both equal the
PlayStation West's text, as if left from it. Area 92's one `0xFF` after a
colour control is not an icon.

`{04:xx}`, which Bunyan's lines also use ("{04:03} sets the log and {04:00}
cuts it"), is a party member's name, in ordinary dialogue in most areas, not
a button.

## 6. Art

Not re-rendered here. From earlier measurements: the US dial page's labels
carry cross / triangle (Enter / Guide) and its dock legend tests and shows
`P` of Japan's ([`key-icons-by-mapping.md`](key-icons-by-mapping.md) 2.2,
2.4); issue #78's capture (a Western disc) shows the same cross / triangle.
EU, FR, DE and the PSP pages were not read pixel by pixel.

## 7. The map, and how icons would follow the Config

The game's code is Japan's on every install. Every hard-coded test and every
save word is in Japan's bits. So:

1. **An icon's button in Japan's terms.** An icon from release R's art or
   text names R's button `b`; in Japan's terms it is `M_R^-1(b)` (`M_R` = `P`
   for the PlayStation West, `S` for PSP-EU, identity for JP, PSP-JP and the
   PC). The PC's own icons are already Japan's.
2. **The Config as a permutation.** The six lines hold a permutation of the
   default buttons (a swap keeps it one). So the save defines `σ`: for each
   line, its default button -> its button now. Unchanged Config: `σ` is
   the identity.
3. **Every icon drawn as `σ(button)`.** "All circles become Xs" when Speak
   is on X.
4. **Every flow the same way** (the owner's "unify"): a hard-coded test of
   Japan's bit `b` tests `σ(b)`, and confirm and cancel become `σ(circle)`
   and `σ(cross)`. Then the fishing, the minigames and the menus follow the
   Config as the field does, and every icon tells the truth.

A behaviour change (a ledger entry): with `σ` the identity, nothing a player
sees or presses changes.

## 8. The owner's answers (2026-10-17)

1. **Confirm and cancel follow `σ`** too: menus confirm on whatever Speak is
   on, cancel on whatever Action is on (section 7.4 as written).
2. **The starting Config is the source disc's**: an install starts with the
   defaults of the release it was built from; with several sources, the
   order is JP, then the PlayStation West, then the PSP. Read here (not yet
   confirmed by the owner): the PC's own data counts as a JP source (Japan's
   code and defaults), so a PC install with a Western language layer starts
   with Japan's buttons and a Western disc-only install with the West's.
3. **PSP-EU's text is honest**: the well (68) and the ram (121) use the
   PlayStation West's buttons there, so their icons translate by `P`, the
   rest of PSP-EU's by `S`. PSP-EU's code at those two is not read.

Not built. Issue #78 (a Western disc install's world-map legend) is built
first and alone: the disc's baked icons are blanked so the live glyph - the
Config's button, as on the PlayStation - is the only one drawn.

# The A/B/C sweep of the `us` set, 2026-10-13: ten routes, the PC against the US disc alone

**Status:** MEASURED (2026-10-13 morning, the owner away; branch
`away/1013-us` off `d0d9162a`). Every claim below is from the runs, dumps
or tool output named with it; the frames, logs, heat maps, dumps and
reports stay under `C:\Users\kerok\abu` (game-derived, rule 1). Not seen by
the owner.

The question ([`abc-sweep-1012.md`](abc-sweep-1012.md) 6 left it): is the
`us` set ([`abc-harness.md`](abc-harness.md) 2: `pc-enus`, the PC install
with an `en-US` cache built from the PC's files and the US disc, against
`psx-us`, the US PlayStation disc alone) otherwise stable at today's build,
what does it show that the eu triple did not, and where do the two
us-specific open items stand: the load screen's `"%X"` (D254) and the inn's
black screen on a PSX-only install (D255).

## 0. What came out

- **All twenty runs ended `done`, no `CRASH` or `FATAL` line in any log**
  (`runs/u1013_*/*/bof3x.log`). No route parts: the scene key never splits,
  no frame is missing.
- **One defect new to this set, US only: the title's copyright line** (3.1).
  The disc-only install takes the US disc's `Title_Sprites` rectangles, whose
  copyright pieces are laid out for the US title's two lines, and draws them
  at the PC's (Japanese layout's) one-line places: `1997` runs into `ALL`
  and the US's second line is never drawn. Every route's title frames.
- **One gap of the disc-built image, filled the twin way:** the faerie
  ranked lists' page counter format `"%d/%d"` (`0x6531F4`), zero on every
  disc build, read verbatim from `ETC/COMMU05.EMI` (4). The board's page
  counter now reads `1/2` as the PC's (proof run 4.2: fairyVillage's
  unexplained frames 90 to 83).
- **One trait-keyed known difference added:** `pc-map-legend`, the world
  map's legend panel (the port's keyboard legend against the disc's
  buttons): worldMapAndAreaTransition's unexplained frames 25 to 6 (5).
- **The two open items, captured, unchanged:** D254 (the slots' `) 16`) on
  every route's load screen; D255 (the inn's screen black from recipe frame
  2880 to the route's end, `SND\OYASUMI.DAT` `FAILED`) (6). D255 is not
  fixed here.
- **The eu sweep's fixes hold on the US disc:** the field Equip icons
  (D262), the gene window's `AP` label (D263), the village board's help line
  and area limits (D260, D261 - US's map placed those already): no
  difference at those places (3.4).
- **The state hash** (A-B, with the US cache's gate-skip list): outside the
  VRAM shadow and the CLUT strip, only the sound page `0x7DE000` (gone on a
  repeat: the machine) and, on the attract, the draw page `0x802000` from
  tick 1310 - repeated and read to the byte: pointers into area 4's loaded
  data, which differs between the PC's and the US disc's chunks (7).
- **The harness's stale-log trap met once and fixed:** `input_run.py`
  removes the launcher's old `bof3x.log` before a run (8).

## 1. Build, root, method

- **Build:** this branch's `build/` (the tree at `d0d9162a`), copied by
  `abc_run.py prepare` into the ini-less `C:\Users\kerok\abu\L`: SHA-1
  prefixes `bof3x.dll` `75FA072C97F9`, `bof3x-launcher.exe` `C5C1DFD60066`,
  `bof3x.exe` `F0DB819224F4` (the last equal to the eu sweep's). Every run
  logged `inject: 10088 ours`.
- **Root:** `C:\Users\kerok\abu`, new. `python tools/abc_run.py prepare
  --set us --root C:/Users/kerok/abu`: the PC cache 290 s, the US cache
  522 s, the install 19 s (917 s in all).
- **Self-tests from `abu\L`, headless:** the hosted `BOF3X_SHADOW='*'` on
  `abu\g_pc`, exit 0, `inject: 10087 ours`, 1,072 `0 MISMATCHES` lines and
  none other; the exe host on `abu\g_psxus` (`BOF3X_EXEIMAGE` = the US
  cache, `en-US`), exit 0, `inject: 10088 ours`, `place 730 ranges 6845
  bytes`, `gap 282 ranges 3313 bytes`.
- **Warm-up:** the exe host's first run from a fresh launcher copy differs
  at tick 3 (HANDOFF, traps), so one combat run of `psx-us` alone (tag
  `warm`) went first and was not compared.
- **Runs:** `python tools/abc_run.py run --set us --tag u1013 --route R
  --root C:/Users/kerok/abu --statehash --statehash-skip
  C:/Users/kerok/abu/c_psxus/base/exe/gate-skip.txt`, one game at a time,
  `--every 30`. The skip list is `exe_tables.py gate-skip --cache
  abu/c_psxus` (the committed list plus 1,047 ranges, 46,710 bytes).
  Reports `report_u1013_<route>.md`, frames `runs/u1013_<route>/`. Other
  agents ran games beside these all morning; the machine was not quiet.
- **fairyVillage** ran from a scratch copy with `# save fairyVillage` added
  as a comment (the eu sweep's trap; no frame moves). Unlike the eu triple,
  this set plays every route under the language it was recorded in
  (`en-US`), so the PC side is the owner's own route.
- **cutsceneAndNue's PSX-US side** first "ended" in 15 s with 5 timed
  frames (8); it was run again alone (`--only psx-us --tag u1013`, 273 s,
  247 frames) and every route compared again from the frames on disk
  (`abc_run.py compare`) once `pc-map-legend` was in.

## 2. The sweep, route by route

A = `pc-enus`, B = `psx-us`. Frames compared / differing / unexplained /
first differing tick / first unexplained tick / where the scene key parts.
The eu sweep's A-B (`pc-en150` against `psx-eu`) beside it.

| Route | A-B (us) | Runs (A, B wall) | eu A-B |
|---|---|---|---|
| attract | 366 / 66 / 66 / 420 / 420 / - | done, 385 + 383 s | 366 / 61 / 31 |
| combat | 87 / 7 / 7 / 90 / 90 / - | done, 113 + 98 s | 87 / 6 / 3 |
| shop | 105 / 16 / 16 / 90 / 90 / - | done, 123 + 115 s | 105 / 16 / 13 |
| cutsceneAndNue | 252 / 18 / 18 / 30 / 30 / - | done, 270 + 273 s (B's re-run) | 252 / 18 / 15 |
| worldMapAndAreaTransition | 71 / 25 / 6 / 90 / 90 / - | done, 89 + 82 s | 71 / 25 / 22 |
| menu_screens | 64 / 36 / 36 / 420 / 420 / - | done, 80 + 72 s | 64 / 36 / 33 |
| field_menu | 55 / 27 / 27 / 420 / 420 / - | done, 71 + 63 s | 55 / 27 / 24 |
| dragonTransform | 145 / 118 / 118 / 60 / 60 / - | done, 163 + 156 s | 145 / 118 / 116 |
| bossAndFlash | 184 / 16 / 16 / 60 / 60 / - | done, 203 + 203 s | 184 / 17 / 14 |
| fairyVillage | 157 / 91 / 90 / 60 / 60 / - | done, 185 + 195 s | 157 / 131 / 128, parts at 3600 |

The us set's unexplained count is the eu's plus the title frames (2 or 3
a route, 30 on the attract) on shop, cutsceneAndNue, menu_screens, field_menu
and dragonTransform exactly, and within the timing frames
elsewhere (3.1: the eu set explains its title by `pc-title-mark`; here the
copyright line stays unexplained in the same frames). Its first
unexplained tick is therefore the title's.

The unexplained differences, each opened and looked at (crops of both
sides, the heat maps), by kind. *Data* a changed texture or text, *timing*
the same picture at another moment. Descriptions are of what is drawn.

**On every route:**
- The title (mode 0, area 0; the attract's 420..1290 and its second title
  6120..7020): the copyright line `20,208-300,224`, about 8,400 pixels
  (**3.1**), beside the PC's (R) mark (explained, `pc-title-mark`).
- The load screen (mode 1): three regions at `56,56..128,184`, the slot
  number and the empty slots' text (**D254**, 6.1).

**Beyond those, per route:**
- **combat:** `t001140`, a battle frame: Ryu's attack pose and a heal
  effect a frame apart, the `Max!` pop-up not yet up on PSX-US; `t001110`
  and `t001170` equal (timing).
- **shop:** the inn, 2880..3150, up to 80% of the frame (**D255**, 6.2).
- **cutsceneAndNue:** `t005370`, a glow's phase (timing, as on eu); the
  save menu 7020..7350 (D254).
- **worldMapAndAreaTransition:** after `pc-map-legend`, only the title and
  the load screen.
- **menu_screens / field_menu:** Ryu's name, the save's Chinese glyph `龙`
  on the PC and blank on the disc's font (16 x 16 at `116,64`, `36,64`,
  `200,64`, `28,20` by screen; data: the recipe save's name text, as on
  eu); the play time's colon at `68,24-72,36` in some frames (timing: its
  blink).
- **dragonTransform:** the same name glyph in the battle window
  (`116,204-128,216`) and the gene window (`156,44-168,60`), 101 frames of
  990..3990 (data, the save).
- **bossAndFlash:** `t002520`, `t003150`, `t003480`, `t004530`,
  `t004800`..`t004830`: a boss effect's phase (a dome, a creature's pose)
  a frame apart, each single and equal on both sides of it (timing; eu had
  three such frames). The repeat (7) differs at 3480, 4530 and 4800..4830
  again and not at 2520 or 3150: those three recur run to run, so they are
  not the machine's load; why the effect's phase differs there between the
  installs was not looked into (listed in 9).
- **fairyVillage:** (a) area 65's first visit, 300..600: the legend panel
  merged with a low-amplitude region over the forest canopy
  (`64,16-268,68`, `0,20-156,96`): the same regions and pixel counts as the
  eu sweep's (`report_n1012_fairyVillage.md` A-B, 360..600); the PC's
  `AREA065` base chunks 1, 2, 4 and 5 differ from the US disc's (the
  caches' `manifest.toml`), data of the port's area, not looked into
  further; (b) the board's page counter `236,44-264,52` 810..930: `1/2` on
  the PC, a stale `3` on PSX-US (**4**, filled); (c) 1350..3270: the food
  counter (84 on the PC, 34 on PSX-US), the faerie pop-up's stat bars
  (drawn on the PC, black on PSX-US, `t001710`) and a flying creature's
  place a frame apart: the faerie stats `0x653210 + 20 r` still zero on the US
  image, the eu fixes doc's 3.5 finding, now seen on US too; (d) 300..330
  and 630: transitions a frame apart (timing).
- **attract:** `t008490`, `t008550` a dialogue line typed one glyph apart;
  `t009810` a 246-pixel corner labelled *moved +7,+6*, and `t010590`,
  `t010860` the whole frame (a camera and light phase): timing, the attract
  is not frame-exact there (`abc-harness.md` 8: 2 of 366 on a PSP repeat).

## 3. Findings

### 3.1 The US title's copyright line drawn on the Japanese layout (new; US disc only)

`t000120` of combat (and the title frames of every route): the PC draws
`© CAPCOM CO.,LTD.1997 ALL RIGHTS RESERVED.` on one line at y 208; PSX-US
draws `© CAPCOM CO.,LTD.1997ALL RIGHTS RESERVED.`, the `1997` running into
`ALL` (crop `runs/u1013_combat/*/t000120.png` `0,180-320,240` at 2x).

**Cause, measured.** `Title_DrawSetA` (`0x462820`, `title_states.cpp`)
draws `Title_Sprites` (`0x653850`, entries of u, v, w, h, clut) entry 8 at
(0x14, 0xD0) and entry 9 at (0xA4, 0xD0) - 0x14 + 0x90, entry 8's width on
the PC. On the US image the table is mapped from the US disc
(`data.toml`: `0x65385F..0x653910 map ETC/GAME.EMI#1@0x801D1C7B`), and two
entries differ from `BOF3.exe` (a scratch byte compare of `c_psxus`'s
`data.bin`): entry 8 `w` 0xA0 where the PC has 0x90, entry 9 `v` 0x30
where the PC has 0x20. The PSX-EU image equals the PC's there (only the
known `+0xE` gap byte, DIV-0114). The US disc's twin of `Title_DrawSetA`
(`ETC/GAME.EMI#1`, the US disc's file read with capstone; found by sprite
1's placement `li a0, 0x106` / `li a1, 0x82` at `0x801D1330`, then calls to
`0x801D17D8` with x, y, index in `a0..a2` through `0x801D1464`) draws three
pieces on two lines: entry 8 at
(0x0C, 0xC8), entry 0x13 at (0x0C, 0xD4), entry 9 at (0xAC, 0xD4); the US
table has 20 entries (entry 19: u 0, v 0x20, w 0xA0, h 0x10), the PC's 17.
So the disc-only install cuts the US art with the US rectangles and places
them where the Japanese layout's code puts its two pieces: entry 8 (160
wide) overlaps entry 9 by 16 pixels on one line, and entry 0x13, the US
second line's first piece, is never drawn.

Classification: data and code together - the US release's own title
layout, which our code (the PC's, from the Japanese layout) does not have.
Not a gap and not a twin fill: the bytes are the US disc's own. Two
fixes are open, both the owner's call (a DIV either way): draw the US
layout on a US-built image (the three placements and entry 19, by build),
or keep the PC's two rectangles for this page (but the US page's rows hold
the US text, so the PC's rectangles would cut it wrongly). For numbering.

### 3.2 The world map's legend

Area 33 (worldMapAndAreaTransition, 1110..1770) and area 65
(fairyVillage, 330..600, 4650): the legend panel `64,20-124,56` draws
`SPACE / X / ENTER` with icons on the PC and the disc's buttons with
`Enter / Guide / Camp` on PSX-US (crop `t001140` at 4x). This is the
port's own art, which no disc carries (`importer-transforms.md` 5, the
dial page: "a note, not a divergence"); `pc-map-legend` (5) explains it.

### 3.3 Area 65's forest canopy

See 2, fairyVillage (a). Small differences (mean |d| about 1) over the
forest in the same frames on eu and us; the PC's `AREA065` chunks differ
from both discs'. Data, the port's area art; for the owner's eye with the
other port-art differences.

### 3.4 The eu sweep's fixes on the US disc

- `menu_screens`' `equip` shot: the six slot icons equal on both sides; the
  frame's only regions are the name glyph (D262 holds).
- `dragonTransform`: no region at the gene window's `AP` label
  `200,76-224,88` (D263 holds).
- `fairyVillage`: the board draws its two house slots at 1350 on both
  sides, and the route runs to its end with no crash and no parting (D260;
  US's map already placed the area limits, D261's row is PSX-JP and PSX-EU
  only). Played under `en-US` the route's moments fall elsewhere than the
  eu sweep's; at 1710 a faerie's pop-up is up, its stat bars drawn on the
  PC and black on PSX-US (the stats gap, 2 (c)).

## 4. The fill: the ranked lists' page counter format

### 4.1 The row

`0x6531F4..0x6531F9`, `"%d/%d"` and its NUL: `CommuRank_Show`'s
`kPageFormat` (`rest_4e.cpp`), passed to `sprintf` with the page and the
count. A `gap language-zero CommuRank_Modes+0x80` on all seven builds
(`recipes/exe-gaps.tsv`; `exe_tables.py`'s `LANGUAGE_ZERO` lists it, and
`loc-build-disc-only.md` 5.4 calls it "the engine's own by meaning").
The US image showed what a zero format draws: a stale `3` where the PC
draws `1/2` (`t000810`, `236,44-264,52` crop at 4x; the eu fixes doc saw
the same region on PSX-EU and PSP-EU).

**The disc carries it verbatim in its reader's overlay.** A scan of the US
disc's `ETC/COMMU*.EMI` for the five bytes finds one copy, in
`COMMU05.EMI` section 7 (code and data at `0x801D0C00`) at `0x801D0C04`,
followed by `"%2d"`. Its reader, capstone over that section: `lui a1,
0x801D; addiu a1, a1, 0xC04` at `0x801D0EAC`, then `jal 0x8017E3F4` with
`a0` the buffer `0x80145AD4`, `a2` the page byte `0x801D19F4` + 1 and `a3`
the count - the PC's `CommuRank_Show` shape. So the row is a `twin`
(`tools/psp_exe.py` `TWINS`), every build.

`psp_exe.py recipe` over the seven discs (`regen.log` in the session's
scratch): `<build> twin 0x6531F4..0x6531F9 from ETC/COMMU05.EMI#7
0x801D0C04: 5 bytes, 0 not BOF3.exe's` on psx-jp, psx-us, psx-eu-en,
psx-fr, psx-de, psp-jp and psp-eu.

### 4.2 Checks and the proof run

| Check | Result |
|---|---|
| `exe_tables.py recipe`, `psp_exe.py recipe`, `exe_tables.py recipe`, `exe_own.py measure --out <scratch>`, `exe_tables.py gaps`, the seven discs | `exe-gaps.tsv` loses the seven `0x6531F4` rows and nothing else; `exe-psp.tsv` gains the seven twin rows; `exe.toml`'s identical counts move; `exe-layout.tsv`, `exe-pointers.tsv`, `exe-places.tsv` unchanged |
| `importer.py check`, `exe_tables.py check`, `psp_exe.py check`, `ledger_check.py` | 0 errors / OK / 114 entries, 0 errors, 2 notes |
| `abc_run.py prepare --set us` (both caches rebuilt with these tools: 261 s, 325 s), the exe host's self-test on `abu\g_psxus` | `c_psxus` `data.toml`: `0x6531F4..0x6531F9 place ETC/COMMU05.EMI#7:twin@0x801D0C04`; self-test exit 0, `inject: 10088 ours`, `place 731 ranges 6850 bytes` (was 730 / 6845), `gap 281 ranges 3308 bytes` (was 282 / 3313) |
| fairyVillage, tag `ufx` (`report_ufx_fairyVillage.md`) | both sides `done` at 4719, no `CRASH`; A-B differing 91 to **84**, unexplained 90 to **83**; the page counter span 810..930 gone: `t000810` reads `1/2` on both sides (crop `236,44-264,52` at 4x); the rest as in 2 (the title, the load screen, area 65, the faerie stats from 1350) |

No `src/` change, so no shadow self-test beyond section 1's.

## 5. The known entry: `pc-map-legend`

`recipes/abc-known.toml`: `pair_trait = "pc"`, `areas = [33, 65]` (the
areas a route has shown it in), `box = [64, 16, 124, 60]`; ledger "none:
the port's own art" with `importer-transforms.md` 5's note. Measured by
`abc_run.py compare` on this sweep's frames: worldMapAndAreaTransition A-B
unexplained 25 to 6 (19 frames explained, 1110..1770), fairyVillage 91 to
90 (the frames at 330 and 4650; at 360..600 the legend region merges with
the canopy region of 3.3 into one box larger than the rule's, and stays
unexplained, as it should). It names no ledger entry because none exists:
the port's art is simply absent with the port. Other world-map areas get
added when a route shows them.

Not added, on purpose: the name glyph of the recipe saves (2, menu_screens)
- a property of the owner's saves, not of an install; and the title's
copyright line - a defect (3.1).

## 6. The two open items

### 6.1 D254, the load screen's `"%X"`

On every route's load screen (combat 240..300, menu_screens 570..720,
cutsceneAndNue's save menu 7020..7350): PSX-US draws each slot's number
blank and each empty slot as `) 16` (the stale tail of slot 0's `00:16`),
the PC (with the `en-US` layer) the slot numbers and the port's Chinese
empty-record word (crop `runs/u1013_combat/*/t000270.png`, `40,40-200,200`
at 2x). The same as on PSX-EU; still open, the owner's call. The disc
carries no `"%X"` to twin (`known-defects.md` D254).

### 6.2 D255, the inn on a PSX-only install

`shop`, PSX-US: the inn's rest opens `SND\OYASUMI.DAT` between recipe frames
2640 and 2670 and the log has `open ... FAILED SND\OYASUMI.DAT`
(`runs/u1013_shop/psx-us/bof3x.log`); the PC opens it from slot 0. Both
sides go black through 2850. From 2880 the PC is back in the inn (`Good
morning... Save?`, then the inn menu with `117Z` at 3150) while PSX-US stays
black to the route's last frame, 3150, ten timed frames at up to 80% of the
frame. Not fixed here (another agent renders the jingle's audio).

## 7. The state hash

Per route, `statehash.py diff --align recipe` A-B (`compare/statehash_A-B.txt`),
pages outside the VRAM shadow and the CLUT strip:

| Route | Pages (beyond VRAM / CLUT) |
|---|---|
| combat | `0x7DE000` (the sound page) on 9 ticks |
| shop, cutsceneAndNue, field_menu, dragonTransform | none |
| worldMapAndAreaTransition | `0x7DE000` and `0x937000` at tick 4 only |
| menu_screens | `0x7DE000` at tick 344 |
| bossAndFlash | `0x7DE000` on 74 ticks (2..3382) |
| fairyVillage | `0x7DE000` on 17 ticks |
| attract | `0x802000` from tick 1310 to the end |

`0x7DE000` is the sound device's page (`Snd_BufferDesc`, `Music_Events`),
the noise `state3-build.md` 5.2 and the eu sweep record. **Repeated** on
the route where it was largest: bossAndFlash again (tag `u1013r`, after
the proof's cache rebuild and a fresh gate-skip list, 1,046 ranges) - 70
pages, none outside the VRAM shadow and the CLUT strip, no `0x7DE000` on
any tick. The first run's sound page was the loaded machine. **The attract's `0x802000`**, repeated: a
second pair of runs of `wait 1500` from the same installs
(`input_run.py ... --no-save --lang en-US`, `BOF3X_STATEHASH_DUMP=1320,1400`,
`abu\dump\`) differs on the same page from tick 1310, and `statehash.py
bytes` on the tick-1400 dumps names it: `DrawTable+0x0` (32 bytes),
`Gfx_FrameNodes+0x500` and `+0x508` (two bytes each) and
`PartyWorkingRecords+0x3C` (10 bytes) - words pointing into the loaded
area data, `0x160` bytes apart between the sides. The attract is in area 4
from 1310, and the PC's `AREA004` base chunks 6 and 7 differ from the US
disc's (the caches' manifests), so the data after them sits elsewhere.
Data, not game state. No game-state page differs on any route.

## 8. The harness: the stale-log trap, fixed

cutsceneAndNue's first PSX-US run ended `done at recipe frame 7421` in 15 s
with 5 timed frames: `input_run.py` read the `recipe done` and `shot` lines
the PC run had left in `abu\L\bof3x.log` before the new game rewrote it,
released the "shots" and ended the game at frame ~163 (its own log, copied
after, stops there). The trap `abc-harness.md` 9 records. **Fixed in
`tools/input_run.py`:** the launcher's log is removed before the launch;
if it cannot be removed, reading starts at its end and restarts from 0 once
the file shrinks. Every run after the change (the re-run, the dump pair,
the proof run) followed a finished run in the same folder and read only
its own log.

A smaller trap, not fixed: `prepare` rebuilding a cache (here after the
twin row changed the tools' hash) deletes `base/exe/gate-skip.txt` with
it, and the next `--statehash-skip` run stops at once with `FATAL:
BOF3X_STATEHASH_SKIP: cannot open ...`. Run `exe_tables.py gate-skip` again
after any `prepare` that rebuilt a disc cache.

## 9. What is left

- 3.1, the US title layout: the owner's call between the two fixes (or
  leave it); for numbering.
- The faerie stats `0x653210 + 20 r` on the US image (2, fairyVillage (c)):
  the eu fixes doc's open finding, the same on US; COMMU00's nine-byte
  records would need a new rule shape, not a twin.
- 3.3, area 65's canopy, and the other port-art differences: the owner's
  eye.
- D254 and D255 stay open; D255's audio is another agent's.
- bossAndFlash's three boss-effect frames (3480, 4530, 4800..4830) that
  differ between the installs on both runs: not read.
- Not run here: caughFish and masterAndManillo on the us set (the other
  agent ran them on 2026-10-12 night, `abx\runs\us_*`).

## 10. For the other files

Not edited here; the coordinator folds them in.

- **`docs/known-defects.md`**: 3.1 for a number (the US title's copyright
  pieces placed on the Japanese layout, entry 0x13 never drawn); D254 and
  D255 seen again on PSX-US (6); the faerie stats' entry, when numbered,
  covers PSX-US too.
- **`docs/DIVERGENCE.md`**, DIV-0101 (the gaps entry), amendment: "Amended
  2026-10-13 (the us set's sweep, [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md)
  4): the faerie ranked lists' page counter format `0x6531F4` is no longer
  zero on a disc-built image, read verbatim from `ETC/COMMU05.EMI#7` (every
  build, `twin`); every filled byte equals `BOF3.exe`'s. Verification: that
  doc 4.2."
- **`docs/loc-build-disc-only.md`** 5.4: `0x6531F4` is placed on every
  build now (by `twin`), so the list of "language addresses that stay zero"
  is seven; `exe_tables.py`'s `LANGUAGE_ZERO` keeps the address, as it kept
  `0x6536B0` after D261 (a placed byte is not a gap).
- **`docs/abc-harness.md`** 9: the stale-log trap is fixed by
  `input_run.py` (8); the trap line can say so, and the gate-skip list a
  cache rebuild deletes (8) is a line for the traps. Section 5's table gains
  `pc-map-legend`.

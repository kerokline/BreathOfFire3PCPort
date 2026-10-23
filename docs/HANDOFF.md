# Handoff — next session

**Status:** IN PROGRESS (2026-10-23: PR #112, Amalgam's melt (DIV-0159), open; the bit-15 round staged on
`render/stp-blend`, item 00. 2026-10-22: the reading-row round built on `localization/second-script`, its end on
`reading-row/end`)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

**10,088 functions are ours in the exe host, 10,087 in a hosted run** (the game's own code is all ours since PR #43).
**The reading-row round is built** on `localization/second-script` (off `main` `abf915cc`, its end batch on
`reading-row/end`; [`STATUS.md`](STATUS.md) has the paragraph; item 6a below). **The wide-ring round is on `main`**
(PR #104; item 0). Before it `main` was `dc921a0c`: since `19180e63` it took the typed-data round's stages A and B (PR #94: 50,943 raw data sites
to 27,394, the machine code identical; `tools/data_census.py` the CI ratchet), the quicksave round (PR #95: F6 / F7 /
the F5 menu, DIV-0145..0147, D322), the quicksave's language guard (PR #96: the original Chinese against a
translation refused before a byte is written; one translation into another kept, IDEAS I47), D266 and DIV-0148
(PR #98), the day's docs (PR #99), stage C (PR #100) and the reading-row round's staging (PR #101, item 6a).
**2026-10-21** was a cleanup: every worktree but the main checkout removed and every branch but `main` deleted,
local and on `origin`, each found merged by `git cherry` first; the owner deleted the older harness roots in the
user folder (`ab*`, `b_*`, `g_*`, `q_*`, `v_*`, `b1` and the rest), so a doc naming one names a run that is gone.
**2026-10-20** ([`exe-gap-round.md`](exe-gap-round.md) frames what is left of
the disc-built image): D266's title mark from the disc (DIV-0101's `titlemark` rule), each Western release's own
staff roll following the language (DIV-0148, D323), the recipes regenerated with the second pairing pass's rows, the
ten state-hash pairs re-recorded (`cn_<route>_orig3` / `_orig3b`) and every difference of ours against them traced
to a ledgered change. **Layers built before DIV-0148 carry no roll**: rebuild a Western language (`importer.py
build`, or `loc_build.py all --discs` for an install's `DAT\<tag>.*.DAT`) to get the release's credits. **The
typed-data round's stage C is on `main`** (PR #100)
([`typed-data-stage-c.md`](typed-data-stage-c.md); each step as run in [`typed-data-round.md`](typed-data-round.md)
3): C0 one shared views header, C1 the types (the shared 0x80-byte `SpriteHead`, renames, views, new structs), C2 the
sites they unlock by seven groups, C3 the module-only entries and a naming pass, C4 the tables as `records::` arrays,
the scratch block renamed `SharedScratch` with its per-function names as typed per-file aliases (the owner's calls),
the generated `GameData` struct (0x37B000 bytes, 2,965 members) and the strict CI rule (`data_census.py --strict`,
27,327 exceptions). Family audits after C2 and C4, each with a fix branch. The census reads 27,389, a wider count
(the `<Name>_addr` and `rv::Bytes` spellings now count), not a regression, every site with a reason; **the machine
code is identical to `main`'s throughout** (`tools/codegen_same.py`, 640 objects, `0 differ` at every merge).
Quicksave owed: the owner's play, their call on drawing F6 / F7's confirmation (logged only, as F12's is, DIV-0032),
and the leftovers in [`quicksave.md`](quicksave.md) (a live screen grab at the take, the cross-reboot base, the
hosted cross form).

## Pick up here

00. **The bit-15 round: semi-transparent primitives blend only texels with bit 15** (branch `render/stp-blend`, off
   `main` `9a4cda2e`). **Step 1, the census, is done** ([`stp-census.md`](stp-census.md), `BOF3X_STPCENSUS`): five
   routes, 123,467 semi-transparent textured draws, 34,858 the PSX would draw opaque - the rope and the page-15 text
   palettes (CLUTs `7800` / `7807`, additive, on dark backings) and nothing else; **no primitive mixes the two
   kinds**; modes 0 and 2, every GT4 and every cell sprite unchanged. CLUT row 480 measured on the twin's savestates:
   bit 15 clear there too, so the text is Capcom's port as the rope is (section 4). Before step 3: census the routes it
   did not reach (section 5). Found 2026-10-23 on the owner's route `ropeAndAmalgam.txt` (save
   `ropeAndAmalgam`), recipe frame ~300, the McNeil rooftop (area 28): the rope over the black sky draws at about a
   quarter of its PSX brightness (median (20, 16, 6) against the twin's (72, 56, 24)).
   **Established** (runs in `C:\Users\kerok\bof3x-runs\amalgam_111`, the twin's in `C:\Users\kerok\bof3x-runs\rope_psx`):
   - The rope is 12 map-cell quads, records at cell (0x27, 0x16..0x21), condition `0x0216`, texture word
     `0xF360B0DD`, drawn by `MapCell_DrawQuads` `0x570020` (PSX `0x80156ABC`). Both machines draw them as `0x2E`
     (semi-transparent), colour `0x60`, blend mode 3 (background + a quarter of the texel). Flag `0x16` and the map
     patch at (0x27, 0x15) come from `Scena02_EnterArea` area 0x1C (`src/game/scena_sc2.cpp`).
   - The PSX (the sibling's runtime, `gpu_frame_27440.json`): the rope's CLUT (48, 483) entries 1..7 have bit 15
     clear, so the GPU draws them opaque and the blend never applies.
   - The PC: `Gfx_ConvertRow` (`0x59EBB0`, `src/game/gfx_clut.cpp`) drops bit 15, and `D3d_SetBlend` (`0x59FCA0`,
     `src/game/d3d_draw.cpp`) blends the whole primitive. The US PSX disc's data in our engine draws the same dim
     rope, so the data is not the cause. Capcom's port, then: a DIV, not a data fix.

   **The plan** (agreed with the owner 2026-10-23; a round of its own because it changes every semi-transparent
   textured draw):
   1. A census: log every semi-transparent textured primitive whose texels include bit-15-clear ones, over several
      routes (attract, combat, an effects route, `ropeAndAmalgam`), to size the change before building it.
   2. Carry bit 15 into the textures: the palette path (`Gfx_ConvertRow`) and the direct-colour path
      (`Tex_Convert16`, which DIV-0159 / PR #112 now makes opaque but for 0). The A1R5G5B5 page format has one alpha
      bit, already transparency, so this likely wants a 32-bit page format in the backend with three alpha levels
      (transparent, solid, blended) - `render/render_shim.cpp` `CreateSurface`, `render_d3d11.cpp` `Convert` and the
      pixel shader.
   3. Draw by texel: for a semi-transparent textured primitive, bit-15-clear texels opaque, bit-15 texels blended
      (two passes, or dual-source blending in the D3D11 backend).
   4. A DIV entry and a switch, the existing fuzzes (`tex_page`, `d3d_draw`, `gfx_clut`), A/B frames on the census
      routes, then the owner's eye on the rope and a few effects.

0. **The wide-ring round is merged** (PR #104); the owner's play of each change
   ([`owner-review.md`](owner-review.md) 2026-10-21: the ring at a pulled-out camera and a quickload across widths;
   the READY! banner and the *Fin* wipe; the table faerie in `dolphinBoss`; areas 31, 90, 198 framed; Garr at the
   camps of areas 53 and 192). Every call is answered (round doc section 6); the end batch's measured gate is its
   section 9. Left open by design: D292's kind 0x8D first frames (accepted), area 19's camera-out (no save reaches
   it, [`wide-ring.md`](wide-ring.md) 8), the D300 rule's broadening (when a second example is seen). The round's
   runs: `C:\Users\kerok\bof3x-runs\r_ring\<group>`.

0a. **The typed-data round is done through stage C** (PR #100; [`typed-data-stage-c.md`](typed-data-stage-c.md),
   each step as run in [`typed-data-round.md`](typed-data-round.md) 3). Next in that line is I44's stage 3, the
   relocatable image (the round doc's section 7, not staged), and IDEAS I49 (what `Battle_AutoTargetCheck`'s `+0xBA`
   test means, after `kEnemyHp` became `kEnemyIntel`). **The method that held** (stage C's section 1):
   `tools/codegen_same.py` against a baseline build of the base commit after every batch, `0 differ`; after every
   typing pass a read-only family audit by other agents, because the gate cannot see a pointer typed as the wrong
   struct; build before calling anything done. A new typing pass builds its own baseline worktree (the old ones were
   removed 2026-10-21). Stage C's scratch: `C:\Users\kerok\bof3x-runs\c_typed` (the coordinator's log `RESUME.md`,
   the briefs, the audit reports `audit\`); stages A and B's scratch is deleted.

1. **The owner's checks** (each in [`owner-review.md`](owner-review.md), newest first):
   - **The reading-row round** (item 6a): play of the furigana row, F4, a second language and the Mods tab's
     install; nothing to decide unless the gap reads wrong in play.
   - **The wide-ring round** (item 0): play of the five changes; nothing to decide.
   - **Quicksaves** (DIV-0145..0147): not played yet; whether F6 / F7 get an on-screen line (logged only today, as
     F12's is, DIV-0032); IDEAS I47 (one translation's quicksave loaded under another) when they want it.
   - **From the button round (PR #85):** record the minigame starts with `BOF3X_RECORD`
     ([`button-routes.md`](button-routes.md) 5) and look at the contact sheets; `Scena19_Run0`'s R1. **The root
     `C:\Users\kerok\b_rc` that held the placed saves and the sheets was deleted 2026-10-21**: before the owner's
     check, re-prepare a root under `C:\Users\kerok\bof3x-runs\` and regenerate them (`button-routes.md`'s commands,
     `abc_run.py prepare --set sigma`, `sigma_saves.py`, `sigma_sheets.py`, with the new root). **From PR #81:** the stale-install prompt in a real start, the PSP-only title in play, D310.
   - The ultrawide round's and the away day's looks, listens and calls (owner-review's 2026-10-14 and 2026-10-13
     sections); Peco's EXP table (item 9).

2. **The exe-gap round** ([`exe-gap-round.md`](exe-gap-round.md), framed, not staged): what is left between a
   disc-built image and `BOF3X_EXEIMAGE_STRICT=1` as the default - 49..74 `gap` rows per build, STRICT narrowed to a
   reviewed list, D280's `ja-JP` kinds 7..15 (answered yes, not built), the owner's open rows (the empty save slot
   first), psp-jp's read-watch coverage now that D321 is fixed. Stage 0 and D266 are done (PR #98). Staging it is the
   owner's word.

3. **The state hash after 2026-10-20** ([`state-hash.md`](state-hash.md) 6): **the ten route pairs re-recorded 2026-10-20** as `cn_<route>_orig3` /
   `_orig3b` (main's build `84e43c32`, 173 ranges, the same conditions as the `orig2` pairs) and ours checked against
   each (`cn_<route>_a1020`): combat and shop identical on every tick; the rest differ only by ledgered changes,
   each confirmed by a hosted run of ours with the change switched off - DIV-0092 (the area's music after a load:
   field_menu and dragonTransform then identical; cutsceneAndNue and menu_screens the same four pages), the button
   round's glyphs into VRAM (caughFish by DIV-0138 alone, a bisect over 19 switches; worldMapAndAreaTransition by
   DIV-0135 alone; masterAndManillo by the glyph set together, not by DIV-0138 or DIV-0140 alone - which one is
   open), and on whelpBoss DIV-0092 with DIV-0143 then one page, `Sound_Banks`' buffer pointers (heap addresses, raw
   dumps at tick 10,000: the `Sound_Channels` class, a skip-list candidate). A few VRAM and sound pages at tick 3
   came in one run of five and are noise. Scripts and checks: `C:\Users\kerok\bof3x-runs\away1020`. **Stale at the tip since 2026-10-22**: nine routes fail
   against them at the reading-row round's base tip (with PR #105) and at its end alike (D330); re-record before using them as a gate
   (item 6a).

4. **Open technical items, no owner needed:**
   - **The ultrawide round's own** ([`ultrawide-round.md`](ultrawide-round.md) 5b and the group docs): the six
     `# save` headers D298 names; the band census's two follow-ups (D299) and a `+0x83` column naming a census row's
     script; a log line for case zero's north steps; a `gameover` hash at 1 and 2 against narrow for D285; the four
     CPU readers of the grab (D297); `inject_all.cpp`'s comments at `MenuDrawHelpers_Inject`, `Rest2G_Inject`,
     `Rest2H_Inject` and `Rest2F_Inject` ([`ultrawide-slides.md`](ultrawide-slides.md) 9).
   - **The all-calls tracer stalls** a hosted run before any recipe frame (D276,
     [`hidden-start-scan.md`](hidden-start-scan.md) 8): bisect the DLL.
   - **The JP world map's camera** (D281, [`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 3.3).
   - **Groups 17 and 18 from PSP-EU and ja-JP** ([`labels-1013.md`](labels-1013.md) 7): not built.
   - **The `_pre1013` entry lists** in `analysis/`: remove once nothing reads them.
   - D270's look owed; D284 and D285 not traced; D287 (the launcher's clipped lines); DIV-0064's amendment pending
     the owner's confirmation.

5. **The follow-up round: disc-only and PSP polish** (not staged): PSP-EU's image's 39 ranges
   ([`exe-image-twins-1013.md`](exe-image-twins-1013.md) 6); [`disc-only-round.md`](disc-only-round.md) 5; D246 and
   D257, the owner's. D320 (song 166 on disc-only installs) is open.

6. **The staged localisation PR**: the empty save slot blank as the US disc draws it (`0x669CCC`) and IDEAS I34, the
   name entry back ([`name-entry-restoration.md`](name-entry-restoration.md); its section 8 is the owner's first).
   The exe-gap round needs its answer too (item 2).

6a. **The reading-row round is built** ([`reading-row-round.md`](reading-row-round.md), status and "as built" at
   its top; issue #97; IDEAS I21 and I50) on `localization/second-script`, the end batch's fold on
   `reading-row/end`: **PR next** at the owner's word. The dialogue box draws an optional reading row over each text
   row from kind-18 side chunks - furigana from the player's `ja-JP` layer (DIV-0155, the readings shipped as the
   first mod zip, DIV-0157) or any other held language (DIV-0156, every ordered pair built by `setup`); two row
   pairs a page at pen y 2, 9, 21, 28; F4 hides the readings (`ruby_key=` / `BOF3X_RUBY_KEY`, `none` frees it);
   chosen in the launcher's *Row above the text*. **The afternoon's three changes, all merged:** under a layer a
   timed box stops for a confirm press and the offset-pen / effect pages are composed, so the new game's opening
   shows the second script (group OS; DIV-0155 / DIV-0156 amended; sheets
   `C:\Users\kerok\bof3x-runs\rr_os\sheets\`); the dropdown is hidden unless the Mods page's *Extras*
   sub-button's toggle (`ruby_ui=`, on by itself when a reading-row mod is added) shows it, and Settings closes the
   gap (group LX); `setup` carries an install's saves across a re-run (D331 fixed - before it a re-run removed them
   with the previous game folder); and the second mode re-flows pages, never rows (group NR: every authored row
   kept, a third text row to a new page, a write-time check that no composed row is wider than its authored one,
   D332 fixed; about 3,700 more pages on a Japanese main, the owner's trade). Findings D326..D332. What is next:
   - **The owner's call:** the next-page arrow at a timed stop (today it is the plain end-of-message wait, no
     arrow; drawing one is a renderer change and a DIV amendment, OS's hand-back) and D332 (English over
     Japanese: the opening's speaker names are glyphs, not an insert, so the re-break runs a name into its speech
     and one row reaches the frame).
   - **The owner's play** of the sheets' scenes ([`owner-review.md`](owner-review.md) 2026-10-22, "to play"): the
     dolphin fight with furigana, F4, English over Japanese, the Mods tab's install of the zip. Sheets in
     `C:\Users\kerok\bof3x-runs\rr_rr\shots\sheets\`.
   - **The gap**, only if the owner reads it otherwise in play: built as 1.5 units of clear space over and under a
     reading (the nearest even spacing to "2 and 2" on the unit grid; 2.5 / 2.5 at pitch 21 is the other even
     choice), one table, `msgbox.cpp` `kRowTable` ([`reading-row.md`](reading-row.md) 3.3, RR's hand-back 4).
   - **The readings follow-on group** (decided 2026-10-22, owner-review): `data/furigana/lexicon.toml` from the
     owner's machine so a player's build needs no SudachiPy; `--compare` with fugashi + UniDic; an agent pass over
     the review list sharded by area, diffed against Sudachi, each disagreement proposed as a sidecar rule for the
     owner to approve; derived files in the harness root only (D328).
   - **The release gate:** SudachiPy and its dictionary are not bundled (`tools/make_release.py` `NOT_BUNDLED`), so
     a player's `setup` with the furigana mod installed stops loudly until the lexicon lands or the owner decides to
     bundle (about 70 MB). Do not release the mod zip as playable before one of the two.
   - **Re-record the `orig3` state-hash pairs** at the tip (D330, [`state-hash.md`](state-hash.md) 6): stale for
     every build since PR #105's recipes and the changes before it; the round's gate was ours against the base
     tip's runs (`C:\Users\kerok\bof3x-runs\rr_rr\statehash\`).
   - Not reached: AREA014's scarecrow and AREA150's prompt (no route or save), the faerie village's tutorials (no
     recipe), Japanese over English on a capture; a page whose text rows are all empty draws no readings (none
     seen; a rule is the contract's call, RR's hand-back 6). Hand-backs:
     `C:\Users\kerok\bof3x-runs\r_ruby\{fb,ml,pr,rr,sb,sl,os,lx,nr}\HANDBACK.md`.

7. **Work that needs no owner** (each doc's own list is the authority):
   [`exe-import-engine.md`](exe-import-engine.md) 5's remainder; [`cache-read.md`](cache-read.md) 10; the music's
   long-fade row ([`music-open-ends.md`](music-open-ends.md) 4) and `SsUtKeyOnV`; the loop rows `084`, `109`, `130`
   ([`wav-dump.md`](wav-dump.md) 5); the exe's remaining Chinese
   ([`exe-chinese-remaining.md`](exe-chinese-remaining.md) 3 and 7).

8. **Small, left over:** [`round-14-cleanup.md`](round-14-cleanup.md) 4's last paragraph; the 61 run-time raw calls
   ([`round-13-cleanup.md`](round-13-cleanup.md) 1.3); E2E's 19 unreachable names
   ([`round-13-review.md`](round-13-review.md) 3); the PSP's eleven map-band changes; Capcom's type-1 routine;
   DIV-0041's terrain margin at the bridge's edge cells; `Sprite_FlashClut`'s row mask; seven older hosts in
   `entries_logic.txt`; `importer.py verify`'s `opt/area4-walls` note on a US-only cache; `dragonMenu`'s frame 720.

9. **Revisit: the PSP's Peco level table is a repair, not a rebalance** (read 2026-10-16; owner's call). DIV-0104 /
   DIV-0111 and their docs call roster index 6's 249 differing `Char_ExpTable` bytes a PSP balance change; decoded
   row by row they are **one row inserted** (the PSP adds a level-36 row; the PSX's last row is all zero), Capcom's
   defect by the PSX-twin rule, which the PSP fixed. **To do:** (a) the owner decides whether it becomes a
   Sensible-tier repair on every install; (b) reword those entries; (c) name the six gain stats; (d) see Peco 97 -> 99
   in play. A second gap (41 -> 42 on the PSP) and Peco's empty level-1 ability are in the same reading (session
   9c821506's scratchpad).

10. **After the first signed release** (`v0.1.0-alpha.3`): submit the binaries' hashes as false positives to
   Microsoft and Avira; pin jsign's jar by hash ([`releases/README.md`](releases/README.md) "Signing").

**Older open items** (one line each; the pointer holds the detail):

- **The owner's older checks** (move to `owner-review.md`): DIV-0063 in game; the compass needle (DIV-0044) and the
  sky's bands (DIV-0041); the wide game over in a fight, the trees at the field's edges, a held pad input across a
  video's start (DIV-0066); the Config screen under an overlay; French, German and Japanese in game; DIV-0064's
  labels; [`USER_CHECKS.md`](USER_CHECKS.md)'s list.
- **The layering (DIV-0071):** on by default; a launcher key not built
  ([`sprite-draw-order.md`](sprite-draw-order.md) 19).
- **The fix wave's questions** ([`yes-no-prompts.md`](yes-no-prompts.md) 6); D198 is Capcom's renderer, not fixed.
- **Round eleven's open set-ups** ([`round-11-cleanup.md`](round-11-cleanup.md)); [`round-10-cleanup.md`](round-10-cleanup.md)'s
  remaining items.
- **Localisation** ([`dialogue-localisation.md`](dialogue-localisation.md) 6 and 8); furigana is IDEAS I21.
- **Widescreen's debts** ([`widescreen.md`](widescreen.md) 4, 5): the sky `0x571C85`, the message-box table, the corner
  pop at margin 100.
- **[`new-code-audit.md`](new-code-audit.md)**: eight bugs confirmed by reading, ten unchecked reports, six
  questions; A1 and A2 the owner's. The lifter's first run ([`lifter-feasibility.md`](lifter-feasibility.md) 7).
- **`analysis/pairs_propagated.json`**'s three wrong pairs ([`battle_damage.md`](battle_damage.md)); then
  [`attract-remaining.md`](attract-remaining.md) 5.1.
- **The first receipt** ([`STATUS.md`](STATUS.md) open decisions).
- **IDEAS:** I13; I15's look toggle key and I19; I45 shrink the quicksave files; I46 what loading one trusts; I47
  quick-loading across translations.
- **The display overhaul's owed checks** ([`display-overhaul.md`](display-overhaul.md) 5,
  [`window-modes.md`](window-modes.md) 6).
- **Stage 2's regression check** ([`attract-mode.md`](attract-mode.md) 6).
- **The asset path** ([`asset-loading-path.md`](asset-loading-path.md) 4).
- **Obligations** ([`STATUS.md`](STATUS.md)): contact TheRealBiggs; write findings back to the sibling.

## How to run things

_Verified 2026-09-24; the disc-only, A/B/C, state-hash, WAV and save lines 2026-10-12..13._

- **Build:** `cmake --preset i686 && cmake --build build` - llvm-mingw's `i686-w64-mingw32-clang++` (the `retcomm`
  toolchain under `~/.local/share`), **not** MSYS2's. The first configure fetches SDL3 (network).
- **Run:** `build/bof3x-launcher.exe --game bof3 --no-config` from a script (`--no-config` skips the settings dialog,
  [`launcher-settings.md`](launcher-settings.md) §4); log in `build/bof3x.log`. `BOF3X_ORIGINAL=NAME` / `=*` /
  `*,-NAME` for original behaviour. Nothing mode-sets (DIV-0032); the game runs unfocused (DIV-0033), so
  `attract_run.py --no-front` leaves the desktop alone for all-ours runs; `--launcher DIR/bof3x-launcher.exe` runs a
  copy with its own `bof3x.ini` (or none: self-tests and references want an ini-less copy, Traps). End a run with
  `taskkill //F //IM BOF3.exe` only for a game you started (the runners do exactly that now,
  [`world-map.md`](world-map.md) §6); batches run detached.
- **Self-tests, headless:** `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' build/bof3x-launcher.exe --game <dir> --no-config` (a hosted run: the player's launcher hands it to `bof3x-launcher-dev.exe` beside it since 2026-10-15, so a clean launcher copy takes that file too, [`launcher-settings.md`](launcher-settings.md) 2):
  exit 0 passed, 3 a Fatal ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2). Shadow names per module are in each module's
  doc; comma-separated lists work.
- **A disc-only install** ([`disc-only-install.md`](disc-only-install.md)): `python tools/importer.py build --source
  "CDImage/Breath of Fire III (USA).cue" --lang en-US --out <cache>` (about 3.5 minutes with the movie; ffmpeg on the
  PATH or `BOF3X_FFMPEG`), `python tools/importer.py install --base --cache <cache> --game <empty dir>`, then the
  launcher with `cache=<cache>` in its ini (or `BOF3X_CACHE`) and `--game <that dir>`: no `BOF3.exe` needed,
  `BOF3X_EXEIMAGE` set from the cache by the launcher. The exe host's self-test from such an install:
  `BOF3X_SELFTEST_ONLY=1 BOF3X_EXEIMAGE=<cache> BOF3X_CACHE=<cache> BOF3X_LANG=en-US <copy>/bof3x-launcher.exe --game
  <dir> --no-config`. The gate: `python tools/exe_tables.py gate-skip --cache <cache> --game bof3` writes
  `<cache>/base/exe/gate-skip.txt`; two attract runs under it (`attract_run.py ... --lang en-US`, one from the
  disc-only install, one from the real install with `analysis/cache/pc-plus-us`) and `statehash.py diff`.
- **The A/B/C harness** ([`abc-harness.md`](abc-harness.md)): `python tools/abc_run.py prepare --set eu --root <root>`
  builds each install's cache and game directory and the ini-less launcher copy `<root>/L`; `python tools/abc_run.py
  run --set eu --tag T --route R --root <root>` plays a recipe on every install and writes `report_T_R.md`;
  `abc_compare.py --side A=<run> --side B=<run>` compares two builds' runs of one install. The roots are
  game-derived, never in the tree (the sets' roots below).
- **The WAV dump** ([`wav-dump.md`](wav-dump.md) 2): `BOF3X_WAV_DIR=<dir>` writes the game's audio mix to
  `<dir>\<n>_<label>.wav` (`BOF3X_WAV_LABEL`, default the recipe's name); `input_run.py --wav <dir> [--wav-label L]`
  sets both; `tools/wav_diff.py` lines two dumps up. The recipe step `music TRACK [FRAMES]` plays a track
  (instrumentation, like `poke`). The dump is at 44.1 kHz, not the device's format.
- **Which files a route loads:** `BOF3X_DATLOG=1` logs one line per file `LoadDatFile` walks, overlays included
  ([`asset-loading-path.md`](asset-loading-path.md) 5).
- **A recipe's save:** `input_run.py` (and `abc_run.py run`) take `--save NAME` (that `tools/recipe_saves` save in
  slot 0 on every side) or `--no-save` (slot 0 left alone); the harness refuses a recipe with no `# save` header
  unless one is given ([`abc-harness.md`](abc-harness.md) 3a). `input_run.py` removes the launcher directory's old
  `bof3x.log` before a launch (the stale `recipe done` trap, closed 2026-10-13).
- **The harness's sets and roots** ([`abc-harness.md`](abc-harness.md) 2): `--set eu`, `--set us`, `--set jp`; one
  agent per root, each session preparing its own under `C:\Users\kerok\bof3x-runs\` or `D:\bof3x-runs\` (both
  Defender-excluded; `--root` or `BOF3X_ABC_ROOT`, the default `~\abc`). The older roots named in the round docs
  (`ab`, `abu`, `abj`, `abv`, `abx`, `b_rc`) were deleted 2026-10-21.
- **The state hash** ([`state-hash.md`](state-hash.md)): from an ini-less launcher copy, `BOF3X_OPT=none
  BOF3X_LAYERING=0`, `tools/statehash_skip.txt`; `python tools/input_run.py tools/recipes/R.txt --no-front --game
  <main>/bof3 --launcher <copy>/bof3x-launcher.exe` with `BOF3X_STATEHASH`, then `statehash.py check cn_R_orig2.sh
  cn_R_orig2b.sh <run>` ([`abc-sweep-1012.md`](abc-sweep-1012.md) 6 is a worked example).
- **Oracle:** `attract_run.py --out ref.tsv --original "*,-Game_Clock"`, the same without `--original`, then
  `attract_diff.py ref.tsv new.tsv` ([`attract-mode.md`](attract-mode.md) §6).
- **Memory dump:** `attract_run.py --minutes 2.4` and within seconds `mem_dump.py --label X`; `mem_dump.py --compare A
  B`. Always two reference runs.
- **Frame hash:** `analysis/validate_wm1b.sh` is the set-up; rebuilding the exclusion list is
  [`call-trace.md`](call-trace.md) §6 (`calltrace.py wallclock --static 59E000-5A6000,5A9600-5AB000 --also
  5BC8E0,5BDA20`); `BOF3X_CALLTRACE_DETAIL=lo-hi` on both sides to see a differing frame.
- **Routes and captures:** `python tools/input_run.py tools/recipes/X.txt --out analysis/shots/X [--lang en-US]
  [--no-front]` - the game writes its own frames (`BOF3X_SHOT_DIR`, [`input-script.md`](input-script.md) §3); the
  owner records with `BOF3X_RECORD`. `--speed 8` runs a recipe fast (`BOF3X_SPEED`, DIV-0048's tooling note: same
  frames, same shots; compare the `randlog` against an x1 run once per route). Route A/Bs:
  `analysis/validate_combat.sh`, `validate_shop.sh`, the world map's in [`world-map.md`](world-map.md).
- **What the live runs cost:** `attract_run.py` and `input_run.py` append a line a run to `analysis/run_times.tsv`;
  `python tools/run_times.py [--since "2026-10-03"] [--by tool|what|side]` sums it, with frames / 60 beside the wall
  time.
- **Takeover recipe:** read to the last instruction; `symbols.toml` entry with evidence and `impl`; clone and fuzz
  under `BOF3X_SHADOW`, every call out re-aimed at a recording stand-in, boundaries seeded; plant a bug per behaviour
  and see it refused; live: oracle, dump, hash beside an original-vs-original pair; say in the doc what none of it
  reached. The details (clone ordering in `inject_all.cpp`, x87 `0x027F`, jump tables, MATRIX padding per DIV-0021)
  are in [`SCAFFOLDING.md`](SCAFFOLDING.md) and [`sprite-draw-order.md`](sprite-draw-order.md).
- **The parallel method** (rounds two to fourteen and the platform rounds): split by file or group, each group in its
  own worktree with its own shadow name; **before spawning, commit and register the boundary callees** in
  `symbols.toml`; agents self-test headless only; merge one branch at a time `--no-ff`, the build and
  `BOF3X_SHADOW='*'` after each, reading the build's output; resolve `symbols.toml` by entry, keyed on `pc`, then a
  `tomllib` check; `CMakeLists.txt`, `inject_all.cpp` and `docs/README.md` want both sides. Only an agent's hand-back
  report means merge; a usage cut resumes with a message naming its next step. One live batch after the merges.
- **A suspected bug against the PlayStation:** read the twin ([`psx-twin-check.md`](psx-twin-check.md)) - the
  sibling's `tools/disc_ls.py --extract` and `tools/emi.py list`, capstone MIPS32 LE; find it by the caller's
  constants, not the function's (GCC folds small compares). Same on the PSX = Capcom's, a DIV; different = the port's,
  a restoration.
- **Other:** `save_convert.py` (docstring; `cygpath -m` paths in Git Bash); `crash_report.py` after `CRASH` lines (or
  `llvm-symbolizer` on the launcher copy's `bof3x.dll`, ImageBase `0x10000000`); `loc_build.py all --discs`; `dat.py
  survey`; `verify_fixtures.py`; `ghidra_pc.py import`.

## In flight / uncommitted

- **PR #112 open** (branch `111-amalgam-death`, issue #111): D333 / DIV-0159, a direct-colour page's texels opaque
  but for 0, so Amalgam's melt draws; the owner's `ropeAndAmalgam.txt` route. Its runs:
  `C:\Users\kerok\bof3x-runs\amalgam_111` (also the A/B/C root `C:\Users\kerok\bof3x-runs\abc111`, set `us`, built
  with the discs at `D:\bof3x-runs\discs` through a scratch copy of `abc-installs.toml`) and Capcom's-code runs in
  `C:\Users\kerok\bof3x-runs\amalgam_111_orig`.
- **`render/stp-blend`**: item 00, step 1 (the census) built and run; runs in `C:\Users\kerok\bof3x-runs\stp_census`.
- The 2026-10-21 cleanup removed all 83 worktrees (every one clean but a typed B1 agent's untracked
  scratch) and deleted 145 local and 65 `origin` branches; each was classified merged with `git cherry` first, not an
  ancestry check (Traps). `main` is the only branch, local and remote.
- **Scratch kept, game-derived, never in the tree:** `C:\Users\kerok\bof3x-runs\c_typed` (stage C's log, briefs and
  audits), `C:\Users\kerok\bof3x-runs\r_ruby` (the reading-row round's briefs, wanted at launch),
  `C:\Users\kerok\bof3x-runs\rr_sl` (group SL's run) and `typedc-smoke` (stage C's smoke run).
- **2026-10-20's scratch, game-derived, never in the tree:** `C:\Users\kerok\bof3x-runs\away1020` (launcher copies
  `L\` (main's build) and `L2\` (the D266 branch's), the game folder `g\` hard-linked from `bof3\`, the runs, the
  checks, the chain scripts); `D:\bof3x-runs\away1020` (the US and EU disc-only caches `c_us`, `c_eu` and installs
  `g_us`, `g_eu`, built by the D266 branch); **`D:\bof3x-runs\discs`, the seven discs extracted** - `CDImage\` now
  holds them zipped, and `recipes/abc-installs.toml` still names `{main}/CDImage/*.cue` (Traps).
- **Older scratch:** the owner's scratch disc-only installs `C:\b3g` and `C:\b3p` (built before `wmrec`: rebuild
  or remove). The older A/B/C roots and `lbl13` are gone (2026-10-21).
- **Other sessions work in the main checkout**: check `git status` and the branch before a commit there, and never
  build in `build/` while its game runs.

Local only, gitignored, worth keeping:

- `bof3/BOF3.CFG`, `build/bof3x.ini` (the owner's; do not edit); the owner's saves `bof3/BISLPS*.DAT`; **the
  recipes' saves are `tools/recipe_saves/*.DAT`** ([`input-script.md`](input-script.md) §1a).
- `analysis/statehash/` - **the route pairs `cn_<route>_orig3` / `_orig3b` (2026-10-20)**, the older `_orig2` pairs,
  the overlay pairs `en_<route>_sh3a` / `_sh3b`, `attract_r14_orig*`; `analysis/calltrace/`;
  `analysis/attract/orig_a.tsv`; `analysis/memdump/clutref_*`.
- `analysis/cache/pc-plus-us`; `bof3/DAT/<tag>.*.DAT`; `analysis/font/`; `analysis/shots/` - every capture and sheet
  sent to the owner (2026-10-20: `d266-title-mark-1020/`, `ending-roll-1020/`).
- `CDImage/` - the owner's PSX discs (USA, Europe, Japan, Germany, France; also a Japanese demo disc) and two PSP
  images, zipped. Never commit.

## Traps already paid for

_One line each, with a pointer. Add when something costs more than an hour._

- **A scratch copy of a recipe loses its save** (2026-10-23): `recipe_shots.py` output made from a recipe with no
  `# save` header loads whatever slot 0 holds; `input_run` says `slot 0: left alone` in its first line. A rope
  measurement ran an hour in the owner's whelpBoss area before it was caught. Check that line, or pass `--save`.
- **The release launcher refuses the A/B switches** (2026-10-23): `BOF3X_ORIGINAL` / `BOF3X_SHADOW` want
  `bof3x-launcher-dev.exe` **and** `--host bof3`; with the release launcher it shows a modal dialog on the owner's
  screen and the run waits.

- **A recorded route splits pages under a reading-row layer** (2026-10-22, D329): every page the builder splits adds
  a confirm press, so a route recorded without the layer desyncs at the first one (`dolphinBoss.txt` under
  `ruby-furigana-ja-JP` answers the Fairy Tear prompt "no"). Routes, the state hash and the self-tests run with no
  layer; a run with one wants its own recording or a press per split page
  (`C:\Users\kerok\bof3x-runs\r_ruby\rr\recipes\dolphin_fix.txt`). Never change `tools/recipes/` for it.
- **`tools/mods.py`'s chunk walker and `tools/dat.py` must agree on header word 3** (2026-10-22, commit `5ad20bb2`):
  a kind-18 chunk carries the pool half (0 or 1) there, which `dat.py` refused before FB and `mods.py` walks on its
  own; a change to the container's header goes into both, or a layer reads in one and is refused by the other.

- **The launcher always sets `BOF3X_MUSIC` since DIV-0150** (2026-10-21): an empty `music=` is `psx`, so a PC + PSX
  cache rebuilt since then plays the disc's 11 streams from `base\psx-jingles\`, and a stream's length is what
  `Sound_StreamDone`'s waits read. A reference run on a PC install wants `--env BOF3X_MUSIC=seq` (the old order) or a
  re-record; caches built before DIV-0150 have no `psx-jingles` and play as before.

- **Capcom's WndProc freezes an unfocused game** (2026-10-13, [`wav-dump.md`](wav-dump.md) 4a): an all-original side
  run with `--no-front` made 2,404 frames in ten minutes. Such a side wants `--original '*,-Game_Clock,-Game_WndProc'`.
  **Paid again 2026-10-20:** the state-hash pairs' Capcom side (`*,-Game_Clock`, as section 4 of
  [`state-hash.md`](state-hash.md) records them) stops writing the moment any other game window takes the focus - a
  side run beside a reference chain cost 10 minutes until the window was activated by PID. Run nothing with a window
  beside a reference recording, or give the focus back (`WScript.Shell.AppActivate(<pid>)`). The hash is per tick, so
  a pause costs time, not data.
- **`CDImage\` holds the discs zipped** (found so 2026-10-20; when they were zipped is not recorded): every tool that takes `--disc` /
  `--source` wants a `.cue` / `.bin` or `.iso`, and `recipes/abc-installs.toml` names `{main}/CDImage/*.cue`. An
  extracted copy is at `D:\bof3x-runs\discs` (the seven, by Python's `zipfile`); point the tools there, or extract
  again into a root of your own.
- **A script named after a standard module shadows it** (2026-10-20): a `bisect.py` in a scratch root ran whenever a
  Python script started from that folder imported anything that imports `bisect` (Pillow does) - twice a second,
  untracked bisect chain, racing the real one on the same launcher folder and slot 0 of `bof3\`, holding the folder so three
  queued control runs failed at once (`bof3x.log cannot be removed: a game of this launcher folder is still running`).
  Before trusting a chain's results, list `python.exe` by command line and stop strays by exact PID, letting a running
  `input_run.py` end on its own so slot 0 comes back.
- **The ending's roll is not in `.data` under a Western language** (2026-10-20, DIV-0148): its lines come from the
  layer's kind 17. A layer built before that has none and rolls the Japanese credits on a PC install, D323's
  patchwork on a Western disc-only one. Rebuild the language.
- **`bof3\` is the tooling's install since 2026-10-15** (the owner plays from `game\`): its slot 0 held the recipe save
  `camping.DAT` after 2026-10-20's slot race. Recipes swap slot 0 there freely; the owner's saves are `game\`'s.
- **A title-start recipe with no `# save` lands on CONFIG in a harness folder** (2026-10-13,
  [`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md)): the recipe presses for a cursor that starts on LOAD GAME, which it
  does only when a save exists; the harness's game folders have none. Give it a save or a `--save`.
- **`abc_run.py prepare` rebuilding a cache deletes `base/exe/gate-skip.txt`** (2026-10-13,
  [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md) 8): the next `--statehash-skip` run stops at once with `FATAL:
  BOF3X_STATEHASH_SKIP: cannot open`. Run `exe_tables.py gate-skip` again after any `prepare` that rebuilt a cache.
- **A TaskStop'd regeneration's children keep running** (2026-10-13): stopping the background task that ran a
  recipe regeneration (`exe_tables.py` / `psp_exe.py recipe` chains) leaves its `python` children writing the
  recipes. Check `tasklist` for them and stop each by its exact PID before regenerating again.
- **A branch from before 2026-10-04 looks unmerged by an ancestry check** (2026-10-13): the DCO rewrite gave 185
  commits new hashes, so `git branch --merged` and `merge-base --is-ancestor` call an old branch unmerged though its
  content is in `main`. Use `git cherry main <branch>` (a `-` line is in `main` by patch) before deleting or merging
  one.
- **`abc_run.py prepare` fails with WinError 32 while the root's `L\` is in use** (2026-10-13): it recopies the
  launcher copy, `bof3x.exe` included, so a game running from `L\` blocks it. Let the run finish (or stop it by its
  exact PID) first.
- **A recipe without a `# save` header loads each install's own slot 0** (2026-10-13, `fairyVillage`): the harness's
  three sides can start from different saves. Since the tooling merge the harness refuses such a recipe without `--save` / `--no-save`. Add the header in a scratch copy ([`abc-harness.md`](abc-harness.md);
  [`abc-sweep-1012.md`](abc-sweep-1012.md) 1).
- **Two agents can run games at once only from separate harness roots** (2026-10-13): `kill_stale` ends any game
  started from its own launcher directory, so two runners sharing a root kill each other's games. Give each its own
  `--root` (tonight `ab` and `abx`); the machine is then not quiet, so a state-hash reading wants a repeat.
- **A round branch cut before a PR merged numbers its ledger against the wrong tip** (2026-10-12): `psp/round` sits on
  `98c17e8b`, and `main` took DIV-0102 after it, so numbering from the branch's DIV-0101 would have collided at the
  rebase. `psp/ledger` took `main`'s ledger first and numbered from DIV-0103. Check that `main` is an ancestor of the
  branch (`merge-base --is-ancestor`) before numbering.
- **`dialog_test.py` needs a `bof3x.exe` beside the launcher** since state 3, or the dialog never opens (LT placed a
  stand-in); and the Jukebox's `IDD_MUSIC` grew 36 dialog units for the chord box (TG), which moved its buttons.
- **`attract_run.py` overrides `BOF3X_LANG` with its own `--lang`, default `original`** (2026-10-12): the round's end
  batch set the language in the environment and both gate runs played under the port's Chinese - which a disc-only
  cache does not have - so the disc side stalled in area 4's text box and 349 pages differed. Pass `--lang en-US` to
  the tool. Since DIV-0102 such a start is refused loudly instead.
- **The exe host's first run from a fresh launcher copy differs at tick 3** (2026-10-12, round doc 4.5): DIV-0095's
  icon generation at the load screen (one more CLUT row, `bof3x.icon` and `bof3x.exe.new` written) puts five
  VRAM-shadow pages, `0x7DE000` and `0x937000` a tick apart from the pair; the second run in the same directory is
  identical on every tick. Run the exe host once from a fresh copy before recording with it.
- **MCI refuses a movie whose full path is about 160 characters** (error 304; the 8.3 form opens; the PC's own AVI has
  the same limit): keep a disc-only install's path short.
- **A disc-built image maps 2,183 bytes of Capcom's game values as zero** (DIV-0101, `BOF3X_EXEIMAGE_STRICT=1`
  refuses): a disc-only route that behaves oddly may be one of them (`recipes/exe-gaps.tsv`, the gap lines in the
  log); the two the attract reached are fixed (the button map, the story-flag seed). A third, found 2026-10-16: the
  forest sprite after the mine drawn in pieces (members 7 and 9's animation records, `0x669868`), fixed the same day
  by placing a region from the pointers into it (I43 step 1; EU gap ranges 185 to 177); settling the rest is
  [`IDEAS.md`](IDEAS.md) I43.
- **`'*'` under a Latin overlay stops at `field_c1`** (2026-10-10, the morning's trap read right): `BOF3X_LANG=en-US`
  (en-150, fr-FR, de-DE) re-aims `Config_DrawRowLabel + 0x9F` through the `ConfigText` patch and the clone check
  refuses it after 979 lines. `ja-JP` passes whole. The language self-test is `BOF3X_SHADOW='*,-field_c1'` (the list
  takes `-name`), and `field_c1` is covered by the `BOF3X_LANG=original` run beside it.
- **Every state-hash and attract run wants `BOF3X_OPT=none`** since the walls layer is installed (2026-10-10): the
  layer arms at start-up and the VRAM shadow and `Gfx_CurrentEnv` pages differ at tick 3 on every route, area 4 or
  not. The morning's item 1.1 said "`BOF3X_OPT` unset"; the traps' line was the right one.
- **The route pairs stage the owner's save list** (2026-10-10): a new or rewritten save slot changes `Save_Staging`'s
  page (`0x92A000`) and one beside it (`0x905000`) from the route's first save-list read, on Capcom's side as much as
  ours. Before blaming a tip, run Capcom's side once (`--original "*,-Game_Clock"`, never `'*'` alone: Capcom's clock
  is the wall clock and the play-time digits drift) and `check` it against the pair; if it differs the same way,
  re-record the pair (state-hash.md 6). The 2026-10-05 pairs went stale this way; `cn_<route>_orig2` / `_orig2b` are
  the current ones.
- **Killing a queued chain by a command-line pattern kills its waiters** (2026-10-10): chains that wait on each
  other's markers name each other in their launch commands when the script text was written and launched in one
  command. Write the script in one command, launch by path in another, and stop one by the exact PID of its `bash`,
  its runner and `BOF3.exe`. The owner walking up to the attract window means that run alone restarts.
- **Cloud sessions** (2026-10-08): `git commit -s` there signs as the environment's identity (Claude), not the owner -
  write the owner's `Signed-off-by` and the `Co-authored-by` lines by hand. Uploads go to scratch only (rule 1).
  `apt-get install g++-mingw-w64-i686` gives `-fsyntax-only` checks with `gen_symbols.py`'s header - not the project's
  llvm-mingw, so the owner's build is the check (PR #44's C++ was merged on that alone; the review found a
  Windows-only failure in it). Mednafen 1.29 from Ubuntu runs headless (`xvfb-run -a /usr/games/mednafen -sound.driver
  dummy -soundrecord`) and renders bit-identical to the owner's 1.32.1; background jobs die with the VM's restarts, so
  a keepalive script restarts long runs. Commits reach the owner as `git format-patch` files or a pushed `claude/...`
  branch.
- **A stopped background chain kept running** (2026-10-06): a `bash chain.sh` launched in the background and then
  stopped through the harness left its child shell alive; a second launch of the same chain ran beside it. Both waited
  on `tasklist` for the previous launcher, both slipped through the one-second gap between its narrow and wide runs,
  the second's `cmake --build` failed (`ninja: error: opening deps log: Permission denied`, the launcher exe
  unwritable) and left the **previous tip's DLL** in `build/` - so a live check that read `inject: 10079 ours` had
  validated the tip before. Read the `inject:` count against what the tip should report before trusting a chain's
  result, and give one chain the machine: check `tasklist` for a stray `bash` or launcher before starting another.
- **`BOF3X_SHADOW='*'` in the main checkout's `build/` fails under the owner's settings** (2026-10-06):
  `build/bof3x.ini` is the owner's (`language=en` then; a bare code stops the launcher since 2026-10-10, DIV-0005 -
  the owner's line wants `en-US`), the launcher exports it as `BOF3X_LANG`, and under English the `ConfigText` patch
  re-aims the call at `Config_DrawRowLabel + 0x9F` - which `field_c1`'s clone check reads (`FATAL: ... the site is
  re-aimed already, cannot clone`, exit 3, 978 of the 1,056 `MISMATCHES` lines reached). An environment variable wins
  over the ini: run every self-test with `BOF3X_LANG=original` (the agents' worktrees never saw it - their inis are
  defaults). Do not edit the owner's ini.
- **`BOF3X_OPT` changes the arena and VRAM** (2026-10-08, DIV-0086): a state-hash or attract reference run, and any
  `'*'` meant to match the record, want it unset; the launcher exports the ini's `opt=` as the language's `language=`
  is - a copy with no ini, as for `cheat.exp`. **Since 2026-10-10 an empty `opt=` (or no ini) plays the default
  `area4-walls` when it is installed** (DIV-0080), and area 4 is in the attract demo: a reference run from an install
  that has the layer wants `BOF3X_OPT=none`. And **a recipe recorded under `BOF3X_LANG=en` plays under `en-US`** since
  the tags (the 29 headers said `en`; they say `en-US` since 2026-10-10, comment lines only; `input-script.md`).
- **The bare language codes are retired** (2026-10-10, DIV-0005): `BOF3X_LANG=en` (`fr`, `de`, `ja`), an ini
  `language=en` or `--lang en` to any tool stops it with the tag to use - the launcher by a message box (which a
  scripted run would wait on; the runners check first), the DLL by a Fatal. Old commands in the round docs that say
  `--lang en` want `--lang en-US`.
- **`git am` of a cloud session's patches** (2026-10-08): it refuses a file that already exists untracked (a copy
  downloaded earlier) and a tracked file with uncommitted changes ("does not match index"), and a failed run leaves
  `.git/rebase-apply`, which blocks the next ("previous rebase directory still exists"). `git am --abort`, move the
  copy aside, `git stash`, apply, `git stash pop`. Never `git am -s`: the patches carry the sign-off.
- **A `shot` line is a frame of the route** (2026-09-30). `shot NAME 1 [BUTTONS]` holds its buttons for one frame; a
  shot inserted without taking that frame out of the run it splits puts every later input a frame late. An ad-hoc
  splitter did that to copies of the owner's recordings - menus and dialogue tolerated it, a fishing cast did not, and
  three committed recipes had to be repaired (`9689c71`). Use `tools/recipe_shots.py RECIPE --every N --out COPY`,
  which keeps the total; a hand-placed shot replaces a frame (decrement its neighbour).
- **Every commit wants its `Signed-off-by`** (`.github/workflows/dco.yml`, CONTRIBUTING.md): seven commits made by
  cloud sessions on 2026-10-01 and 2026-10-04 had none and PR #40's check failed. At the owner's word the branch was
  rewritten on 2026-10-04 (`git filter-branch --msg-filter` from `69e9d3c`, messages only: every tree identical) and
  force-pushed: **185 commits from the old `2ab349b` on have new hashes**, the docs' citations were rewritten to them,
  and the old-to-new map is `analysis/round13_signoff_sha_map.tsv`. The cloud sessions' SSH signatures on the
  rewritten commits are gone (a rewritten commit cannot keep one). The old local branches that pointed at the
  pre-rewrite commits were deleted on 2026-10-13. Commit with `git commit -s`; check `git log --format=%B <base>..HEAD`
  before opening a PR.
- **A route A/B's off-list and scratch ini go stale**: the three `validate_*.sh` scripts' `DIVS` lists lacked the four
  09-27 centring and layout divergences (DIV-0058..0061), and a scratch launcher ini copied from an older session had
  `cheat.steal=1` - the 2026-09-28 combat A/B "stole" and every banner differed by a few hundred pixels until both
  were fixed. After a DIV that moves pixels, add its `BOF3X_ORIGINAL` name to the scripts; check the ini's `cheat.*`
  lines before a combat run; and `validate_shop.sh` must pin `BOF3X_WIDE=0 BOF3X_SCALE=2` and a launcher copy like the
  other two or ours writes 1704 x 960 frames against 640 x 480 grabs (fixed in the local copy).
- **A spell fuzz's call counts depend on the build directory**: the harness stores pointers into our DLL in game
  memory, so a worktree and the main checkout take different branches (Steal: 9,278 against 9,850, 0 mismatches in
  both). Judge a merge by 0 mismatches and controls refused (round9 doc section 6).
- **Parallel agents share `analysis/calltrace/entries_logic.txt`**; one agent's script emptied it (round9 doc section
  10). Snapshots `entries_logic_0926_*_raw.txt` exist; after a wave, check every `impl` start has a line.
- **A token-pasting inject macro hides `BOF3_INJECT` lines from `ledger_check.py`** (S16's forty); write the injects
  out.
- **A usage cap cuts every agent at once**: tell them to commit early; a cut agent resumes with SendMessage, its
  worktree intact. Waves of about 350 functions fit one window.
- **Past 12.4 days of Windows uptime (counted from the last Restart; Fast Startup keeps it counting, only a Restart
  resets it) an all-original run stops pacing and, by the code, draws nothing** - `--original "*"` turns
  DIV-0022 / DIV-0047 off with the rest. Reference sides run `*,-Game_Clock` (the clock changes pace, never logic).
  Already past 6.2 days, all-original runs go at half speed (D5): hash verdicts stand (logic frames), wall times do
  not. Pace figures before 2026-09-21 are the 31.25 band.
- **An A/B original side that keeps a function ours can keep a dependency of it original.** `Text_DrawString`'s glyph
  guard was set only by `Font_SetGlyphData`, so with the English overlay the `*,KEEP` side trapped at the first battle
  banner (glyph `0xA6B` over `0xA00`), every run, tracer or not - found 2026-09-25 by the combat route, which no A/B
  had played since DIV-0052. Fixed in `LoadDatFile` (DIV-0016, amended). When a KEEP list keeps ours a function that
  reads state another of ours writes, keep the writer too, or set the state where both sides run.
- **An all-original side cannot write its own frames**: with `Display_Setup` original there is no Direct3D 11 device,
  `render::SaveFrame` returns false (`NOT saved:` in the log), and the runner falls back to the window grab at every
  frozen shot - the screen must be uncovered, and a covered one hung a run at its shot on 2026-09-25. A trace needs no
  captures: play the recorded route file itself (`combat.txt`), not its `_ab` twin.
- BSim produced one high-confidence wrong match; no BSim name exceeds `hypothesis` without a PC-side read
  ([`bsim-evaluation.md`](bsim-evaluation.md)).
- Indexing PSX overlays into the BSim database degrades published ranks unless done on a copy
  ([`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md)).
- Figures repeated across docs are not evidence - count them (the 29,036 case).
- A Python `'''` string or a bash heredoc eats backslashes (`\0` became two NUL bytes in `loc_build.py`), and a
  heredoc holding triple quotes or C++ apostrophes dies with "unexpected EOF". Write source with the editor tools;
  keep backslashes out of `symbols.toml` evidence strings.
- Capcom's WndProc freezes the game when its window is not in front and replays the missed time unrendered
  ([`windowed-mode.md`](windowed-mode.md)); ours does not (DIV-0033), so this bites under `BOF3X_BACKGROUND=0` or an
  all-original side - where **`attract_run.py --no-front` makes 0 logic frames** (`wm1`). Reference sides keep the
  foreground hold.
- A scripted run takes the language and filter from the owner's `build/bof3x.ini` unless the environment sets them: an
  English run against the Chinese reference looks exactly like a `LoadDatFile` regression. `attract_run.py` pins them;
  check the recording's second `#` line.
- A foreground-holding run sends **anything the owner types into the game**; an oracle or hash run on the owner's
  desktop needs keyboard and mouse left alone (lost runs 2026-09-19 and the wave-2 first start).
- A rebuild fails at link with "Permission denied" while a launched game holds `bof3x.dll`. Close the game.
- `pe_xref.py` indexes data memory operands only: "(no references)" for a function means nothing (use `callees` in
  `analysis/pc_funcs.json` or an E8/E9 scan), and `add eax, 0x803580` is invisible to it - walk the `imms` / `offs` /
  `globals_` lists ([`dialogue-localisation.md`](dialogue-localisation.md) §6).
- **The `clut` region is off by one palette row** in most dumps (row 506 all-original, 482 ours): `clutref_a` predates
  DIV-0022. A one-row `clut` difference with arena and VRAM identical is that
  ([`sprite-draw-order.md`](sprite-draw-order.md) §17); re-recording is two all-original dumps.
- `mem_dump.py`'s `clut` region and `attract_diff.py` are unreliable under `BOF3X_CALLTRACE_MODE=all`. Dumps and the
  oracle at full speed, the hash under the tracer, never mixed ([`asset-loading-path.md`](asset-loading-path.md) §2).
- One disagreeing oracle frame at a state change is a torn sample until a re-run says otherwise; a two-frame skew from
  ~3376 on is the `wm1_ours` kind. The frame hash is the arbiter ([`attract-mode.md`](attract-mode.md) §6).
- `--original "*"` unquoted in `$(...)` or `echo` is glob-expanded, and the `cp` after it saves the PREVIOUS run's
  callframes. Check the `inject:` line says what the run was meant to be.
- A fuzz of random bytes barely tests a comparison with a constant: seed the boundaries and let the negative control
  say whether you did ([`sprite-draw-order.md`](sprite-draw-order.md) §6).
- **A frame-hash difference means nothing without an original-vs-original pair**: wall-clock draw code appears as
  tracing gets faster; `wallclock --static` is the fix ([`call-trace.md`](call-trace.md) §6).
- **An owned function missing from `entries_logic.txt` breaks the hash** with every count equal (`ab21`: 23 functions,
  5,236 frames). The tracer reads a size only for an owned entry, so a wrong size matters once its function is taken
  over. Check the list before every batch.
- **The frame hash counts the CRT's `sscanf` by characters** (`ab26`, frame 5524): `Mp3_MemoryIo` scans the music
  buffer's address in hex, so a shorter address is four fewer calls. Extra `0x5BCF64` calls (`0x5BD989` / `0x5BD9C0`
  pairs under `0x5B9AA6`) at one frame are this.
- **The compiler's tail call at a task's top frame shows in the hash** (`ab24`, `Boot_Task`): fix with
  `__attribute__((disable_tail_calls))`; deeper tail calls are harmless. Found in two minutes with
  `BOF3X_CALLTRACE_DETAIL=1-1` and a `diff`.
- The optimiser can hide a wrong build from the fuzz (strict aliasing); `-fno-strict-aliasing` is project-wide - do
  not remove it ([`psx-library-layer.md`](psx-library-layer.md) §2).
- **clang 22.1.8 miscompiles** `(a & 0xFFFFFF00) | ((unsigned char*)a)[8]` at -O2 to a plain byte load. Write it as
  inline asm, as `event_ops.cpp`'s `DirectionInPointer` does ([`event-ops.md`](event-ops.md) §8).
- A negative control NOT refused is information: the quirk may be unobservable (`MapView_SetElevation`, `Gte_Rtpt`),
  the fuzz may be blind (`Gte_DepthRamp` needed seeded values), or the change may change nothing (six on 2026-09-22).
  Ask whether any input could tell them apart; seed it. A control refused by a HANG proves less than one refused by a
  count.
- **A stand-in quieter than the real callee hides what the caller undoes**: give it the side effects the caller reads
  or reverts ([`movement-script.md`](movement-script.md) §1b).
- A fault in a start-up self-test **hangs at start-up** with nothing after the `cloned` lines; `Get-Process BOF3 |
  select CPU` near zero is a fault. A harness restoring a block too far can fault the original's copy.
- **A self-test runs before `BOF3.exe`'s C runtime**: anything reaching `_getptd` (`Rand` does) ends the process with
  only "could not load the dll". Give the fuzz a stand-in ([`sprite-draw-order.md`](sprite-draw-order.md) §16).
- A clone of a function with a jump table runs its cases in the ORIGINAL body; relocate the entries and the `jmp
  [reg*4 + table]` operand, as `text_draw.cpp` does.
- A fuzz can generate the original's own trap: `Text_DrawString` executes `in al, dx` for a glyph above `0xA00`.
- Pairing EMI sections to DAT chunks by order or address mis-pairs 47 files; use `dat_census.align`
  ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2). A raw byte scan of the `DAT`s drowns in audio - walk the chunks, keep
  kind 0; the movement scripts are in the exe's `.data` ([`movement-script.md`](movement-script.md) §3).
- A recipe's `shot NAME 10` can back out before the grab; keep the default 30 on anything that changes
  ([`input-script.md`](input-script.md) §3).
- The field's buttons are save data (`0x903584` / `0x90358E` / `0x903590`, save 5 differs): press `@0x903584`, not a
  shape; `seek` the remembered top-bar cursor ([`input-script.md`](input-script.md) §4).
- Window grabs include Windows 11's rounded corners (mask 8 x 8 each), and a run not through `input_run.py` does not
  freeze for its shots. **A black screen cover makes grabbed captures black and every A/B "identical"** (`ab25`) -
  only the grab fallback now that the game writes its own frames; check captures are not black.
- **Agent worktrees start from `main`, not the current branch**: commit first, and have each agent `git merge-base
  --is-ancestor <commit> HEAD` and reset onto it.
- **`symbols.toml` does not merge by text**: git splices one group's block onto another's (twice in round two, again
  in round three). Rebuild by entry, three-way on `pc`, then check with `tomllib` that no address or name is bound
  twice. **Group names collide** across groups (W / Y, Z / V1): rename in the later group's files.
- **Our own scaffolding has ceilings, and they fail like hangs**: the `BOF3X_ORIGINAL` / `BOF3X_SHADOW` lists (2,048
  characters), the tracer's owned-function table (256, then 2,048 - hit again at 3,165 ours on 2026-09-27, 8,192 until
  2026-10-03, now 32,768 in `calltrace.cpp`) and the detour's owned table (4,096, hit at 4,097 on 2026-09-27, now 16,384 in `detour.cpp`)
  ended in a `Fatal` before the window. Read the log when a run is slow; a `Fatal` dialog blocks the runner until it
  is dismissed.
- **Do not build or self-test in the main checkout while a frame-hash reference side records**: the `orig` side
  recorded under the S32 merge's build made 9,675 logic frames against ~10,300 and four frames of 25,000..33,000 calls
  where its twin had 2 (`r9_orig_0927_loaded`). Agents' worktree self-tests ran under every side without harm.
- **A failed build leaves the previous `bof3x.dll`** and the self-tests pass on it. Read the build's output, not only
  the exit code.
- **A divergence that stretches time must not stretch what the game sees** (DIV-0028): keep the logical state on the
  original's schedule, let only the audible or visible part linger; the traced hash with the DIV on is the check.
- Check a patch's expected bytes against the image, not the disassembly in your head (`0x461A61`); dump the bytes
  first.
- **`BOF3X_ORIGINAL=*` switched off the input recipe**: instruments pass `instrument = true`. `*,-NAME` excludes one
  name (`validate_shop.sh`).
- **An A/B's two sides need the same divergences**, or every capture "differs" by sub-pixel sampling (the first
  backdrop A/B).
- **The game's task stacks lie inside the main thread's stack**: DXGI's `Present` ran off a 16 KB task stack. Anything
  heavier than a few hundred bytes of stack runs on its own fiber (`render_d3d11.cpp`, `RunOnFiber`). Three hours.
- `DrawState` is a Win32 macro and `pass` an HLSL keyword.
- The crash reporter's stack scan misses the faulting frame: use the exception stream's context (the scratch
  `dumpstack.py`; worth folding into `tools/crash_report.py`).
- **The tracer's single step is visible to `pushfd`**: a stray step reached the CRT's `__except` (`0x5BA154`) and
  `_exit(code)` - no dialog, no CRASH line. `calltrace.cpp` handles it now. **A silent exit whose stack holds
  `0x5BA15F` is an unhandled exception**: `BOF3X_EXITTRACE=1` logs it ([`window-modes.md`](window-modes.md) §4a).
- **`renderer=0` in `bof3x.ini` selects Capcom's software renderer** on an all-original side; reference runs pin
  `renderer=1` in a scratch ini.
- **Our code calls ours directly**: `BOF3X_ORIGINAL=NAME` switches only what Capcom's code calls; where it matters
  from our side, call through `bof3::orig::NAME`.

## Waiting on someone else

- **The owner:** [`owner-review.md`](owner-review.md) - its sections from the newest (item 1), then the older eye, ear and
  play checks and open calls. The owner has played a PSX-only install (the reports of 2026-10-16); nobody has
  played a PSP-only one.
- **TheRealBiggs** - not yet contacted ([`STATUS.md`](STATUS.md) obligations).

# Handoff — next session

**Status:** IN PROGRESS (2026-10-20: `main` is `b3498050` (PRs #94..#96 and #98 merged; `typed/c` has it merged); the typed-data round's stage C is built on `typed/c`, unpushed: the owner's smoke run, then its PR, item 0)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

**10,088 functions are ours in the exe host, 10,087 in a hosted run** (unchanged since PR #43: the game's own code is
all ours). `main` was `56d81b42` on 2026-10-19: since `19180e63` it took the typed-data round's stages A and B (PR #94: 50,943 raw
data sites to 27,394, the machine code identical; the census's CI ratchet; the install-gap round's second pairing
pass) and the quicksave round (PR #95: F6 / F7 / the F5 menu, DIV-0145..0147, D322; not yet played). Since then PR #96
(the quicksave's language guard, IDEAS I47) and PR #98 (D266's title mark, DIV-0148): `main` is `b3498050`; PR #99
(docs/handoff-1020) rewrites this file and STATUS for that day's other work. Before `19180e63`: the button round
(PR #85), the event battles' music (PR #86), Amalgam's melt (PR #87), Ryu's dream (PR #90), the install-gap round
(PR #91), the jukebox's recorded streams (PR #92). **The typed-data round's stage C is built on `typed/c`**
([`typed-data-stage-c.md`](typed-data-stage-c.md); each step as run in [`typed-data-round.md`](typed-data-round.md)
3): C0 one shared views header, C1 the types (the shared 0x80-byte `SpriteHead`, renames, views, new structs), C2 the
sites they unlock by seven groups, C3 the module-only entries and a naming pass, C4 the tables as `records::` arrays,
the scratch block renamed `SharedScratch` with its per-function names as typed per-file aliases (the owner's calls),
the generated `GameData` struct (0x37B000 bytes, 2,965 members) and the strict CI rule (`data_census.py --strict`,
27,327 exceptions). Family audits after C2 and C4, each with a fix branch. The census reads 27,390, a wider count
(the `<Name>_addr` and `rv::Bytes` spellings now count), not a regression, every site with a reason; **the machine
code is identical to `main`'s throughout** (`tools/codegen_same.py`, 640 objects, `0 differ` at every merge).
Quicksave owed: the owner's play, their call on drawing F6 / F7's confirmation (logged only, as F12's is, DIV-0032),
and the leftovers in [`quicksave.md`](quicksave.md) (a live screen grab at the take, the cross-reboot base, the
hosted cross form).

## Pick up here

0. **The typed-data round's stage C: the owner's smoke run, then its PR** from `typed/c` (tip `2a208d56` plus the
   coordinator's docs; [`typed-data-stage-c.md`](typed-data-stage-c.md) 8 call 2). On the owner's machine (agents may
   not run the game here): the headless `BOF3X_SHADOW='*'` self-test and one state-hash route, both from a plain copy
   of `build/` as the install-gap round's end batch ran them ([`install-gap-round.md`](install-gap-round.md) 6). The
   machine code is identical to `main` b3498050 object by object, so this is a formality, but it is the only check
   that runs the game. Then the PR: no behaviour change, no ledger entry; every non-merge commit signed off (read
   `git log --format=%B main..typed/c` first). `origin/main` b3498050 (PRs #96, #98) is merged into `typed/c`
   (2026-10-20; gate baseline `.claude/worktrees/typed-baseline-c2`, 0 differ; census 27,389; DIV-0148's one new raw
   site, `scena_sc15.cpp` RollEntry's `L(kRollLines + 4u * i)`, listed `gate`: `Scena17_RollLines[i]` changes
   `Scena17_ScrollRoll`). If PR #99 merges first, merge it too: STATUS, HANDOFF, owner-review, IDEAS and README will
   conflict - take #99's and re-apply the typed-data paragraphs. **Two calls for the owner**, from the C4 audit
   ([`owner-review.md`](owner-review.md) "2026-10-20, typed-data stage C"): `rest_4a`'s `kEnemyHp` against
   `records.h`'s `intel` at the same offset, and `Save_Directory`'s count. **The method that held** (stage C's
   section 1): `tools/codegen_same.py` against a baseline build after every batch, `0 differ`; after every typing
   pass a read-only family audit by other agents, because the gate cannot see a pointer typed as the wrong struct;
   build before calling anything done. Scratch of stage C, outside the tree: `C:\Users\kerok\c_typed` (the
   coordinator's log `RESUME.md`, the briefs `BRIEF_*.md`, the audit reports `audit\`, the C2 groups' and C4's
   scripts); of stages A and B: `C:\Users\kerok\b1\`, `C:\Users\kerok\v_typed\`, `C:\Users\kerok\v_pairs` and
   `g_pairs`. The worktrees are listed under "In flight".

   **Owed from the button round (PR #85), the owner:** (a) record the minigame starts from RC's placed saves,
   `C:\Users\kerok\b_rc\saves\` (`chop8`, `well68`, `rope75`, `tower112`, `ram121`, `dock128`, `crane135`: the six
   minigames and the dock's parts menu; [`button-routes.md`](button-routes.md) 5), with `BOF3X_RECORD`, then the
   identity and swapped runs of each - the only live evidence DIV-0137, DIV-0139's minigame tests and DIV-0140 lack;
   (b) look at the contact sheets, `C:\Users\kerok\b_rc\sheets_round\`; (c) `Scena19_Run0`'s R1, left as it is,
   undecided ([`button-tests.md`](button-tests.md) 2.4). [`owner-review.md`](owner-review.md) "2026-10-17, the button
   unification round" has the list. **Still owed from PR #81, the owner's eye:** the launcher's stale-install prompt
   in a real start; the PSP-only title in play (captured at 4:3, 16:9, 21:9); D310's screens and the hi-lo payouts in
   play.

1. **The owner's answers** ([`owner-review.md`](owner-review.md)): first "2026-10-14, the ultrawide round" - look at
   the faster slide (not yet seen), the 21:9 left message box, the terrain margin 167 at 21:9, two `dolphinBoss` plant
   frames; decide the remaining 16:9 band rows (census 3's list and ST's 14 sites the shape cannot move: move out,
   hide or leave), the faerie through the table (D300, Capcom's; FY's rule), the gene crystal (D242, Capcom's,
   accepted for now), the narrow areas 31 / 90 / 53, the patch cells (D295, leave unless seen), RG - the ring for far
   cameras (D292, I37), the six other effects the GTE fix changes, effect kind 0x51's grab, the mote and the column's
   sound (D293, D294), the two sweeps (I38). Then "2026-10-13, the away day": look (the eye batch q1..q7, the arena
   sheets, the D56 capture, the labels sheet, the Japanese world map), listen (`analysis/listen-1013` a / b / c), and
   decide (inventory Q6 / Q7; member 2's bank, Ryu's byte, member 1; the Western weakness row; the hi-lo count; the US
   title layout; a ja-JP layer for kinds 7..16 and the fishing crash; D255's fix; set 8 confirmed; the key icons'
   five; the `combatGroup` / `sortScreens` saves; the Western title mark). Each answer goes to the ledger or the
   defect list; then the rows to the policy mechanism (`recipes/prefer.toml`).
   Also: **the ending's backdrop** (`wide/ending-backdrop`, DIV-0127, D302, [`ending-backdrop.md`](ending-backdrop.md); unpushed, 2026-10-14): the owner saw the captures and approved; their eye on the mirrored bands in play, then the PR. The launcher page that uses the scene: built 2026-10-15 from the owner's mock-ups (`launcher/title-page`, [`launcher-settings.md`](launcher-settings.md) 2a; unpushed) - their eye on it, then the PR.

2. **The stale state-hash pairs** ([`nue-regression-1013.md`](nue-regression-1013.md)): the Nue signal of 2026-10-13
   was a stale pair (re-recorded as `cn_cutsceneAndNue_orig2` / `_orig2b`) and DIV-0092 by design. **Left:** the
   other nine `cn_<route>_orig2` pairs are stale the same way (save-slot records in page `0x905000`) - about 45
   minutes on a quiet machine to re-record, per [`state-hash.md`](state-hash.md) 6; any recipe that writes a slot
   stales them again (slot 6 is `balioAndSunder_2`'s by design), so a fixed staged save list in `input_run.py` is the
   durable fix (a design change, the owner's call). The ultrawide round compared wide against ours narrow from the same
   hour, which sidesteps this. `bossAndFlash` meets DIV-0092 too (event battle 8, track 33) and has no pair.

3. **Open technical items, no owner needed:**
   - **The ultrawide round's own** ([`ultrawide-round.md`](ultrawide-round.md) 5b and the group docs): the six
     `# save` headers D298 names (`bridgeWalk`, `fairyVillage`, `identify`, `ninaWalkBehindBlock`, `tarmanLogic`,
     `vaccinationAndFireScene` take the save of their own name, as census 3's scratch copies did); the band census's
     two follow-ups (D299: the `field_frame.cpp` hook line for type-`0x0A` sprites, the void channel at the renderer
     seam) and a `+0x83` column naming a census row's script; a log line for case zero's north steps (census 4 had
     to read them from dumps); a `gameover` hash at 1 and 2 against narrow for D285; the four CPU readers of the grab
     (D297, a GPU readback); `inject_all.cpp`'s comments at `MenuDrawHelpers_Inject`, `Rest2G_Inject`,
     `Rest2H_Inject` and `Rest2F_Inject` still say those modules read DIV-0041's widened slide bound back - since the
     faster slide they read Capcom's ([`ultrawide-slides.md`](ultrawide-slides.md) 9).
   - **The all-calls tracer stalls** a hosted run before any recipe frame (D276,
     [`hidden-start-scan.md`](hidden-start-scan.md) 8): `pc_hidden_reached.json` is still the 2026-09-21 reach, the
     199 new hidden starts untraced. Bisect the DLL.
   - **The image's map fault left:** D266 (the Western title mark's width, a `twin` from `GAME.EMI#1` - after the
     owner's yes; D267 and D268 fixed 2026-10-17).
   - **The US title layout** (D277, [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md) 3.1): waits on the owner's call.
   - **The JP world map's camera** (D281, [`abc-sweep-1013-jp.md`](abc-sweep-1013-jp.md) 3.3): the PSX twin read, JP
     against US `GAME.EMI`'s world-map camera set-up, and a raw dump of `0x905000`'s page at tick 1095. The owner's
     hypothesis: a change Capcom made after the first Japanese release (the PC and the PSP both draw the wide view).
   - **Groups 17 and 18 from PSP-EU and ja-JP** ([`labels-1013.md`](labels-1013.md) 7): not built (the layout lives in
     the PSP's Allegrex code; Japanese names exceed the 5-byte copy).
   - **The PSP ELF's second map** is not worth building (187 bytes, [`exe-chinese-remaining.md`](exe-chinese-remaining.md)
     5.4).
   - **The `_pre1013` entry lists** in `analysis/` (`pc_funcs_pre1013.json`, `pc_hidden_pre1013.json`, ...): kept
     beside the replaced ones ([`hidden-start-scan.md`](hidden-start-scan.md) 8); remove once nothing reads them.
   - D270 fixed by DIV-0121 - the owner's look owed, and whether such a field has another way out; D284 (area 23's
     sky in wide) and D285 (the flash phases, item above) not traced; D286 (the fishing equip headings) fixed, FR / DE too since the install-gap round
     ([`install-gap-labels.md`](install-gap-labels.md)); D287 (the launcher's clipped lines); D289 fixed by the same round (the owner's
     confirmation of DIV-0064's amendment pending).

4. **The install-gap round: merged as PR #91** ([`install-gap-round.md`](install-gap-round.md), its status line).
   Stage A (RD, PB, OT, TW, LA, RW) and stage B (PT, CW, FP) placed the census's P1 items 7 and 8, P2 items 11..13,
   the P4 rows and most of P3's reach: the `gap` rows went from 104..125 per PSX build to 58..78, and from 92 / 93 per
   PSP build to 54 / 52. **Its live end batch ran 2026-10-18 evening** (that doc's 6): the self-tests pass on every
   install kind, and the gate and the eleven routes show no game-state difference the round made. Defender stopped
   none of its launcher copies (plain copies of `build/`); detections that evening were all another agent's. The
   new placement kinds are DIV-0101's amendment; also DIV-0103, DIV-0069 and DIV-0064; D319 fixed, D320 open. **Left:**
   - **RW's watch re-run on the merged code** ([`install-gap-reads.md`](install-gap-reads.md) 8). Then I43 step 4,
     `BOF3X_EXEIMAGE_STRICT=1` as the default, once no row read in play is still zero except the owner's two
     (D266's title mark, the empty save slot; a DIV-0101 amendment);
   - **the unpaired rows: a second pass ran 2026-10-19 on `typed/b1`, merged in PR #94** ([`install-gap-pairs.md`](install-gap-pairs.md)
     8, independently re-derived): 385 new `psx` pairs and 125 `psx_overlay` fields; `BossDolphin_Sounds` and the
     `Scena04` row place on every build, `BossGaist_Anims+0x2` and `BossTorch_Sounds` on psx-jp only (the twin's body
     matches three functions in `BOSS035#16`; a `build_twins` tie-break is the follow-up). **The recipes were not
     regenerated in the branch**: regenerate them after the PR merges (expect four new `immref` rows on psx-jp, two on
     each other build). Still unplaced: `Area115_Directions`, `Area121_BandHeights`, `Area135_EffectKinds` (a run
     offset by leading zeros, `Area141_CellEntries`' shape), `Scena02_EffectX`, `Scena03_Bob4` / `Bob2`,
     `LinkedObjectD_Amounts`, and the five rows no code reads;
   - the rows under `find_imm`'s four-byte floor (`EffectKind75_States+0xC`, `BoneDanceShake_Counts`,
     `GameMode3_Steps+0xC`); the `EffectKind18Sub*` rows whose twins' bytes differ;
   - **the census's remaining NEEDS FIX items** ([`install-gap-census.md`](install-gap-census.md) 8.4..8.6): song 166
     on the disc-only installs (D320, P2), the importer's 165 rule after a render; `CommuMusic_CloseSteps+0x68`'s
     glyphs; P3's tail as listed there.

     The owner's calls are in [`owner-review.md`](owner-review.md) "2026-10-18, the install-gap round".
   Regenerate after any tool change (`psp_exe.py recipe`, `exe_tables.py recipe`, `exe_own.py measure`,
   `exe_tables.py gaps`, `exe_tables.py recipe`, the seven discs) and check with `tools/exe_scan.py`.

5. **The follow-up round: disc-only and PSP polish** (not staged). Done: D254, D255, D256, D282. Still open: what
   is left on PSP-EU's image ([`exe-image-twins-1013.md`](exe-image-twins-1013.md) 6: 39 ranges, the
   `BOOT.BIN`-reader class); the disc-only round's own leftovers ([`disc-only-round.md`](disc-only-round.md) 5); the
   PSP-EU Config screen (D246) and the PAL banks (D257), the owner's.

6. **The staged localisation PR**: the empty save slot blank as the US disc draws it (`0x669CCC`) and IDEAS I34, the
   name entry back ([`name-entry-restoration.md`](name-entry-restoration.md); its section 8 is the owner's to answer
   first). Not launched.

7. **Work that needs no owner** (each doc's own list is the authority):
   [`exe-import-engine.md`](exe-import-engine.md) 5's remainder (the effect state tables' twins); [`cache-read.md`](cache-read.md)
   10 (the MP3s in the cache, an ini key for `BOF3X_CACHE_DATA`); the music's long-fade row as a design
   ([`music-open-ends.md`](music-open-ends.md) 4) and `SsUtKeyOnV` read before the sound effects move; the loop rows
   `084`, `109`, `130` not rendered ([`wav-dump.md`](wav-dump.md) 5); the exe's remaining Chinese, the definite list
   in [`exe-chinese-remaining.md`](exe-chinese-remaining.md) 3 and 7's "left open". The three disc-only language
   gaps of the old item 6 are closed (that doc 6).

8. **Small, left over:** the controls still thin ([`round-14-cleanup.md`](round-14-cleanup.md) 4's last
   paragraph); the 61 run-time raw calls ([`round-13-cleanup.md`](round-13-cleanup.md) 1.3, a decision); E2E's 19
   `EffectKind48_State7..12_*` names, which E2D's three-state table makes unreachable
   ([`round-13-review.md`](round-13-review.md) 3); the PSP's eleven map-band changes unread (region-diff 6); Capcom's
   type-1 routine not located; DIV-0041's wider terrain margin as the creator of the bridge's edge cells, unmeasured;
   `Sprite_FlashClut`'s row mask; seven older hosts still covering an owned start in `entries_logic.txt`;
   `importer.py verify` on a US-only cache reports `opt/area4-walls` `AREA004.DAT` slot 6 "composed with the layer, not
   the recipe's"; `dragonMenu`'s frame 720 pop-up differs from its reference (together again at 780, probably
   timing).

9. **Revisit: the PSP's Peco level table is a repair, not a rebalance** (read 2026-10-16, chat only; owner's call).
   DIV-0104 / DIV-0111, [`exe-tables-by-build.md`](exe-tables-by-build.md) 4.4, [`psp-only-build.md`](psp-only-build.md)
   4 and [`opt-layers.md`](opt-layers.md) call roster index 6's 249 differing `Char_ExpTable` bytes a PSP balance
   change at Extension tier. Decoded row by row (PC `bof3/BOF3.exe` against `game/cache/opt/psp-tables/exe/place.bin`
   laid at its `place.toml` ranges), they are **one row inserted**: the PSP adds a level-36 row, and every PC / PSX
   row from 36 on is the PSP's row one level higher; the PSX's last row (level 99) is all zero, the only zero final
   row of the seven characters. Abilities (`+6` / `+7`) stay put. Roster index 6 is Peco (New Game's record order,
   DIVERGENCE.md DIV-0020's original behaviour). The inserted step is the ~1.1x ratio the curve keeps on both sides of it, where
   the PSX's 35 -> 36 step is about twice that. Consequences on the PSX / PC, by `Char_LevelUpGain`'s (`0x432170`)
   reading: Peco cannot sit at level 98 (the 98 and 99 sums are equal, so 97 goes to 99), he ends with one level's
   bundle of stat gains less than the PSP's, and from 36 to 97 each bundle comes a level early and cheaper. Every
   PSX build holds the same table, so by the PSX-twin rule it is Capcom's defect, which the PSP fixed. **To do:**
   (a) the owner decides whether it becomes a Sensible-tier repair on every install, not only an option
   (`psp-tables`) and a PSP-only base; (b) reword those entries from "rebalance" to "a missing row restored";
   (c) name the six gain stats (labels system messages `0xC, 0xD, 8, 9, 0xA, 0xB`, `battle_e7.cpp`'s
   `BattleResultWin_DrawLevelUp`; not checked); (d) see Peco level 97 -> 99 in play to confirm the skip.
   **A second gap the PSP left** (the same day): every character's EXP steps grow in fixed ratio phases (x2, x1.5,
   x1.25, then x1.125; Peco alone also has a x1.1 phase from 26 to 40), and nowhere is a step the square of its
   phase's ratio except Peco's two: 35 -> 36 on the PSX (x1.1 squared, the row the PSP restored) and 41 -> 42 on the
   PSP (x1.125 squared, also 40 -> 41 on the PSX), which no build fills. The EXP alone says a row is missing there;
   Peco's stat gains come in irregular spurts, so they cannot say whether the stats are missing too. Also: Peco's
   level-1 row is empty where the six others carry an ability at `+6`, the same on every build (design or slip,
   the owner's to say). The other six tables are equal on every build and show neither pattern. Scripts: session
   9c821506's scratchpad (`scan.py`, `phase.py`, `cad.py`).

10. **After the first signed release** (`v0.1.0-alpha.3`, signed by `release.yml` through Trusted Signing since PR #72):
   submit the binaries' hashes as false positives to Microsoft and Avira; jsign's jar is still pinned by version
   only, not by hash ([`releases/README.md`](releases/README.md) "Signing").

**Older open items** (one line each; the pointer holds the detail):

- **The owner's older checks** (move to `owner-review.md`): DIV-0063 in game (the gene with a partner down); the
  compass needle (DIV-0044) and the sky's bands (DIV-0041) on the world map; the wide game over in a fight, the
  trees at the field's edges, a held pad input across a video's start (DIV-0066); the Config screen under an
  overlay; French, German and Japanese in game (`AREA065`, German `AREA121`); DIV-0064's labels (status words,
  the battle's stat panel, the shops' `WEAPON` / `ARMOR`, the French pages); [`USER_CHECKS.md`](USER_CHECKS.md)'s
  list (item 8 among them).
- **The layering (DIV-0071):** on by default; a launcher key not built; why the owner's PlayStation emulator shots
  look less cut than the PC's is unmeasured ([`sprite-draw-order.md`](sprite-draw-order.md) 19).
- **The fix wave's questions** ([`yes-no-prompts.md`](yes-no-prompts.md) 6): whether `ItemTrade_Confirm`'s
  per-item prompt gets DIV-0027's stops; the two masters-screen headers' PlayStation text. The stray frame line
  at window scale 1 (D198) is Capcom's renderer, not fixed.
- **Round eleven's open set-ups** ([`round-11-cleanup.md`](round-11-cleanup.md)): D166 a fix candidate, D167 the
  owner's question; [`round-10-cleanup.md`](round-10-cleanup.md)'s remaining items (the `SH_CALL` form, the
  world-map body shared once, D143's five copies); the group docs' contradictions written into D89..D161, one read
  each.
- **Localisation** ([`dialogue-localisation.md`](dialogue-localisation.md) 6 and 8): Japanese, the exe's own
  strings still Chinese (Config, verbs, default names, battle labels); French / German, the message slots whose
  European pointers leave their block and the accented enemy names over 8 bytes; the turn counter, the Skill Ink
  label, the shop's master / apprentice words and the shop and tactics strings; the seven character-at-a-time
  `Text_DrawAt` callers still at 12 px. Furigana is IDEAS I21. The owner's decision: its own branch and effort.
- **Widescreen's debts** ([`widescreen.md`](widescreen.md) 4, 5): the state hash with `BOF3X_WIDE=1` and `=2` is done
  (the ultrawide round) and 3b's culls are classed ([`ultrawide-culls.md`](ultrawide-culls.md)); left: the sky
  `0x571C85`, the message-box table, the corner pop at margin 100.
- **[`new-code-audit.md`](new-code-audit.md)**: eight bugs confirmed by reading and not seen in game, ten unchecked
  reports, six questions; A1 and A2 the owner's. The lifter's first run on `BOF3.exe`
  ([`lifter-feasibility.md`](lifter-feasibility.md) 7).
- **`analysis/pairs_propagated.json`**: `0x445A30` / `0x44B9F0` swapped, `0x44F030` paired to the wrong twin
  ([`battle_damage.md`](battle_damage.md)); then [`attract-remaining.md`](attract-remaining.md) 5.1's divergence
  map (never the `call-disputed` tier) and the name import as `hypothesis`.
- **The first receipt** ([`STATUS.md`](STATUS.md) open decisions): the evidence exists (`attract_diff.py`,
  `mem_dump.py --compare`, the state hash, the route A/Bs).
- **IDEAS:** I13 save states (for what no route replays: bosses, event battles); I15's live look toggle key and
  I19 curvature; I18 closed by the owner (F12 unchanged; the quicksave is F6, DIV-0145); I45 shrink the quicksave files; I46 what loading one trusts.
- **The display overhaul's owed checks** ([`display-overhaul.md`](display-overhaul.md) 5,
  [`window-modes.md`](window-modes.md) 6): `rb1`'s edge pixels (`BOF3X_PIXEL_OFFSET`), SatPixie's tuning by hand,
  the title-bar drag under `BOF3X_BACKGROUND=0`, sprite edges at k = 3 / 6. Not built, loud if reached: a `Lock`
  of the primary or back buffer, sub-rectangle locks, depth / fog / lighting, the back buffer's `GetDC`.
- **Stage 2's regression check** ([`attract-mode.md`](attract-mode.md) 6): which of `MsgBox_Step`'s 23 control
  codes the attract's messages use; the draw-order list checked in the real game (I14 level 1,
  [`sprite-draw-order.md`](sprite-draw-order.md) 5, 6).
- **The asset path** ([`asset-loading-path.md`](asset-loading-path.md) 4): the 32 callers of `LoadDatFile`, the
  value-sequence search for the dropped PSX sections; for I1, `0x5806F0` and the options bytes at block `+0x78`.
- **Obligations** ([`STATUS.md`](STATUS.md)): contact TheRealBiggs; write findings back to the sibling (the `0x0C`
  answer, `Rand` not matching). Unknown though ours: what `SpriteCell_Add` `0x5A6790`'s 8-byte records are.

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
- **The harness's sets and roots** ([`abc-harness.md`](abc-harness.md) 2): `--set eu` (root `C:\Users\kerok\ab`),
  `--set us` (`abu`), `--set jp` (`abj`), the eye batch's scratch installs (`abv`), the 2026-10-12 re-run's `abx` -
  one agent per root.
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

- **The typed-data round's stage C** (2026-10-19..20), all under `.claude/worktrees/`: `typed-c` (`typed/c`, the
  PR's branch), `typed-baseline-c` (a detached `main` `56d81b42` build: the gate's reference until main was merged) and
  `typed-baseline-c2` (detached `b3498050`, the gate's reference now; keep it until the PR merges), `agent-c4` (`typed/c4`, merged), `agent-c4fix` (`typed/c4-fix`, merged), `audit-c4` (detached `f56ed9a8`,
  the C4 audit's read-only tree) and the older stage C agents' `agent-c0`, `agent-c1`, `agent-c2-<group>`,
  `agent-c2fix`, `agent-c3e`, `agent-c3n` (branches `typed/c0`, `typed/c1`, `typed/c2-<group>`, `typed/c2-fix`,
  `typed/c3-entries`, `typed/c3-naming`): all merged into `typed/c`; removable after the PR merges. Scratch:
  `C:\Users\kerok\c_typed`.
- **Stages A and B's worktrees** (merged as PR #94; `git cherry main` shows no commit missing from `main` on
  `typed/b1`, `typed/integration`, `typed/census`, `typed/records`, `typed/globals`, `pairs/unpaired`,
  `typed/b0-globals`, `typed/b1-audit-fix`): `typed-b1`, `typed-integration`, `typed-baseline` (the detached
  `19180e63` build, the gate's reference for A and B, no longer needed) and the stage A, fix, B0 and B1 agents'
  `agent-*` worktrees, plus the session-`eb87db8c` scratchpad's `sd` (`typed/records`). Removable at the owner's word.
- **The quicksave round's** `agent-a10bb1b1f874af08e` (`quicksave/typed`) and the baseline `C:\Users\kerok\q_typed\base`
  (detached `6422dee2`): merged in PR #95; removable.
- **The install-gap round's worktrees** (`agent-*` on `install-gap/*`, and `scratchpad/coord` of session
  `14ea0433`): merged in PR #91; removable.
- **`wide/ultrawide-round`** (a worktree in session `6d500aac`'s scratchpad `uw/`): merged as PR #60. Its
  group branches `wide/ba`, `cf`, `dp`, `eb1`, `eb2`, `eb2b`, `eb3`, `fy`, `grab`, `ld`, `lr`, `pd`, `rn`, `rn2`, `sl`,
  `ss`, `st`, `st2`, `st3`, `uc`, `uc2`, `uw` (most with a worktree under `.claude/worktrees/`): all merged;
  removable.
- **The fifteen away-day worktrees** under `.claude/worktrees/` and their branches `away/1013-*`: merged in PR #58;
  removable at the owner's word.
  Also there from 2026-10-12: `worktree-agent-ae288769dd06909be` (the D260..D263 fix agent's, merged in PR #57).
- **The junction `C:\Users\kerok\lbl13`** (the labels agent's): safe to delete.
- **Scratch outside the tree:** the A/B/C roots `C:\Users\kerok\ab`, `abj`, `abu`, `abv`, `abx` (game-derived, never
  in the tree); the owner's scratch disc-only installs `C:\b3g` (US) and `C:\b3p` (PSP-EU), built before `wmrec` -
  rebuild with the current tools or remove. A `balioAndSunder_2` run may still be running from the session scratch
  `d56/L2` (it restores slot 0 when done). Older: `music/event-battle-restart`'s scratch worktree (the
  session-`3186ef2f` scratchpad's `evb/`, an untracked `cfg.out` - look before removing); `claude/ecstatic-mccarthy-t8bg37`,
  the owner's to delete.
- **Other sessions work in the main checkout**: check `git status` and the branch before a commit there, and never
  build in `build/` while its game runs.

Local only, gitignored, worth keeping:

- `bof3/BOF3.CFG`, `build/bof3x.ini` (the owner's; do not edit); the owner's saves `bof3/BISLPS*.DAT`, theirs to
  play in; **the recipes' saves are `tools/recipe_saves/*.DAT`**, swapped into slot 0 for a run by `input_run.py`
  ([`input-script.md`](input-script.md) §1a). Gitignored, so a fresh clone re-imports them from the owner's slots.
- `analysis/statehash/` - the state-hash references (`cn_<route>_orig2` / `_orig2b`, the overlay pairs
  `en_<route>_sh3a` / `_sh3b`, `attract_r14_orig*`); `analysis/calltrace/` - the frame-hash references and the
  reach traces; `analysis/attract/orig_a.tsv` (the oracle's reference); `analysis/memdump/clutref_*` (its `clut`
  row stale - Traps).
- `analysis/cache/pc-plus-us` (the PC install, the US disc and both PSP images); `bof3/DAT/<tag>.*.DAT` (rebuilt by
  `loc_build.py all --discs`); `analysis/font/`; `analysis/shots/` - every capture and sheet sent to the owner.
- `CDImage/` - the owner's PSX discs (USA, Japan, Germany, France) and two PSP images. Never commit; extract to
  scratch.

## Traps already paid for

_One line each, with a pointer. Add when something costs more than an hour._

- **Capcom's WndProc freezes an unfocused game** (2026-10-13, [`wav-dump.md`](wav-dump.md) 4a): an all-original side
  run with `--no-front` made 2,404 frames in ten minutes. Such a side wants `--original '*,-Game_Clock,-Game_WndProc'`.
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

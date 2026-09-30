# Handoff — next session

**Status:** IN PROGRESS (2026-09-29, round twelve merged to `main` as PR #33, `d1b411c`; round thirteen on `phase-3/capture-round-thirteen` from it: waves one and two merged, 7,568 ours, paused before wave three)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

**7,568 functions are ours** (`inject: 7568 ours, 0 left original`, on `phase-3/capture-round-thirteen`; 6,891 on `main`);
`main` is round eleven (PR #30, `c4b0d32`, on round ten and its cleanup,
PR #28 and #29), and **round eleven's cleanup, the part a session without
the game can do, is on `claude/round-10-cleanup-handoff-qtwcrk`**
([`round-11-cleanup.md`](round-11-cleanup.md), item 0). **Round eleven, the
boss round, is complete** ([`takeover-queue-round11.md`](takeover-queue-round11.md)):
two waves, 531 functions in 11 groups, 5,706 -> 6,237, every BOSS
overlay's code ours through [`boss_harness.md`](boss_harness.md). Round
ten before it took every chapter bank and every area overlay
([`takeover-queue-round10.md`](takeover-queue-round10.md)); round nine
every spell. Every group 0 mismatches, every control refused or an
equivalent with a refused near variant; everything fuzz-only - the live
check per chapter, per area and per fight is the owner's. Merges are
verified in a detached worktree with its own build. Round eleven's debts
are [`round-11-cleanup.md`](round-11-cleanup.md) (item 0); round ten's are
[`round-10-cleanup.md`](round-10-cleanup.md) (item 0a). The rest is
[`STATUS.md`](STATUS.md)'s wave table; do not copy it here.

**The frame hash reference** is `analysis/calltrace/r9_orig` (twin
`r9_origb`, identical on all 10,317 frames; `analysis/validate_round9_hash.sh`,
reference sides `--original "*,-Game_Clock"`, `renderer=1`, windowed,
foreground held), re-recorded 2026-09-27 09:15 at 3,164 ours (`ed0cd6f`);
`r9_ours` (08:50, the same build) identical but frame 0, the set-up (as
since `rb1`). Wave five's 346 functions came after it and none is on the
attract path - every spell group is fuzz-only - so it stands for this
build until something on the attract path is taken (nothing in round
ten's six waves is). `r9_*_0926` and
`r9_orig_0927_loaded` (a side recorded under a concurrent build, four
frames of 25,000 calls) are history; `r8_*` and older too.

## Pick up here

0000. **2026-09-30, three fixes from the owner's `tools/recipes/gameover.txt`** (a fight with Rei's Equip window, the party
   lost, GAME OVER, the title): DIV-0064's second load (the fatal the owner hit), DIV-0065 (the Equip window's stat
   labels a row up - Capcom's own offset), the loss screen's black widened (DIV-0041). Self-test `'*'` 0 mismatches,
   the recipe to `done` on ours; the recipe plays `# save combat` (the owner: the same save as `combat.txt`).
   **Owed the owner's eye** in a fight of their own.

000. **Round thirteen, the effect engine: waves one and two are merged (7,568 ours); paused before wave three** at the
   owner's word (the usage cap). Branch `phase-3/capture-round-thirteen` from `main` `d1b411c`, with `main`'s PR #34
   merged in; nothing pushed. [`takeover-queue-round13.md`](takeover-queue-round13.md): section 10 the cut (35 groups
   E1A..E6D in six waves), 11 stage A, 12 wave one (278), 13 wave two (395). **Before wave three, in order:** the
   harness fold section 13 names (the quiet effect-standard rows five groups re-listed); the whelp route's frame hash
   (it enters functions of EGT, E1C and E2A; `live_batch12w2c.sh` in the session-`6ae930a8` scratchpad is the model);
   then `make_briefs.py <scratch> 3 <tip> band_edges_w2.txt` and four agents, E3A..E3D, 210 functions, E3B
   merged before E3C. Waves three and six are small enough to run together (406). The scripts are in the
   session-`56ff1eb2` scratchpad (`.../56ff1eb2-8c2d-4d5f-82f0-a85df7f2d489/scratchpad/round13/`): `runner13.sh
   <scratch>` merges the groups appended to `pending13.txt` one at a time and stops at a failure (`END` ends it;
   never edit `merge_group13.sh` while it runs), `verify_tip.sh` runs narrow and wide, `fold_names.py` points the
   harness rows at names. The main checkout's `build/bof3x.ini` has `wide=1` and a running game locks its DLL: verify
   in the verification worktree. The wave's agent worktrees and branches are merged and still present.
00. **Round twelve is complete** - [`takeover-queue-round12.md`](takeover-queue-round12.md) is the record: 654 functions
   in fourteen groups, 6,237 -> 6,891, the tip `0e51ec7` live-checked (its section 9: the attract hash and five routes
   identical but frame 0, the pictures at their baselines). On `phase-3/capture-round-twelve` from `main` `430f34b`,
   pushed 2026-09-29 and **merged as PR #33 (`d1b411c`)**. Next, in order: the round's debts (section 7 there: the
   mask and stand-in folds into both harnesses, the defects to number, the pointer scan `band_rows.py` lacks, the 33
   owned starts without an `entries_logic.txt` line); **round thirteen**, the effect engine
   ([`takeover-queue-round13.md`](takeover-queue-round13.md), planned by another session, starts from this tip). Owed
   by the owner: DIV-0063 in game (the gene with a partner down, and each failing pair), the Config screen under an
   overlay, the field recipes (section 6 there). The scripts and briefs are in the session-`6ae930a8` scratchpad
   (`.../6ae930a8-04f9-4a6b-9276-f65e6257f40f/scratchpad/`): `merge_group12.sh`, `queue2.sh`, `rebind_resolve.py`,
   `table_resolve.py`, `live_batch12w2*.sh`, `keep_display.py`, `reach_by_module.py`. **A recipe A/B is a frame hash
   first** (`live_batch12w2c.sh`: original twice, then ours): it needs no screen. The picture A/B's original side is a
   screen grab: the display on (`keep_display.py`) and nothing over the game - a browser window cost one pass. Round
   eleven's cleanup (item 0 below) has all its game-side checks now.
0. **Round eleven's cleanup** - the list is [`round-11-cleanup.md`](round-11-cleanup.md),
   its status header says what landed and what is left. **Landed
   2026-09-28 night on `claude/round-10-cleanup-handoff-qtwcrk`** (restarted
   from `main` at `c4b0d32`; a cloud session without the game files;
   verified by the i686 build, `ledger_check.py`, `gen_symbols.py`,
   `tables.py check`): the six harness fold-backs (section 1 there - the
   standard stand-ins louder for `0x446DE0`, `Msg_OpenScript`,
   `Battle_RemoveFromTurnOrder`, `Scenario_CallA`, `Battle_OpenMsgWindow`;
   `OtherStates` shared; `kTask`'s state draw; the disturbance's case 11
   below `phase_span`; the `DataTable` order logged; the dispatcher contract
   written), the rebinding of every raw constant with a name (195 in 28
   files, values unchanged), the defects D162..D173 (D166 set-up 25's HP /
   AP bytes **a fix candidate for the owner**, D167 set-ups 8..10's poses
   **the owner's question**), `boss_rows.py --no-write`. **First thing with
   the game on hand, at that branch's tip:** `BOF3X_SHADOW='*'` headless -
   **counts will move** for the groups the cleanup doc's section 1 names
   (every group but BSC and BSH for the louder `0x446DE0`, BSA and BSE for
   `Msg_OpenScript`, BSD K26 and BSJ f4 / f5 for the disturbance), 0
   mismatches is the bar, and a mismatch there is a re-read across a call
   to write into the group's doc; then `ledger_check.py`, the hash against
   `r9_orig` (frame 0 only), the combat A/B; then the PR. Then the owner's
   open set-ups (round doc section 4's last item), the recipe saves per
   fight, the local worktrees and the scratchpad's controls scripts (item
   6 there). The round itself: [`takeover-queue-round11.md`](takeover-queue-round11.md)
   - BH alone in stage A built the harness, BSA..BSE then BSF..BSJ ran in
   parallel, every merge verified by its own build and `'*'` in the
   detached worktree; 531 functions, 1,494 controls; the tip's live checks
   passed (section 8 there). The routine that ran both waves:
   `merge_group11.sh <group> <scratch>` (`MOD=boss_<group>`) and the
   filled briefs in the session-`08306a9f` scratchpad
   (`.../08306a9f-084c-46a9-903b-012248764b0f/scratchpad/`), the
   template `analysis/round11_wave1_brief.md`.
0a. **Round ten's debts** - the list is [`round-10-cleanup.md`](round-10-cleanup.md),
   its status header says what landed and what is left. **Landed
   2026-09-28 on `claude/round-10-cleanup-handoff-qtwcrk`** (a cloud
   session without the game files; verified by the i686 build,
   `ledger_check.py`, `gen_symbols.py`, `tables.py check`): the rebinding of
   every raw constant whose target is ours (234 in 41 files, the round-nine
   form - values unchanged, so the fuzz keys stand), the defects numbered
   D133..D161 with the wave docs' contradictions written into the entries
   for one read each (D133's three dispatcher policies, D135's record-255
   split, the rest listed under item 2 there), the harness docs' five
   notes, `area_rows.py --no-write`. Merged as PR #29 (`6b70e71`); **its
   game-side checks ran at that tip on 2026-09-28 afternoon and pass**
   (the cleanup doc's status header has the figures: `'*'` headless,
   `ledger_check.py`, the hash against `r9_orig` frame 0 only, the three
   route A/Bs at their round-eight baselines). Then the
   owner's order to choose among what is left: the `SH_CALL` form with the
   five `scenario_harness.cpp` rows moved to `SH_OURS` in one commit, the
   world-map body shared once (D143 names the five copies in ours), the
   small engine group for the callees nobody owns (item 3), the tool fixes
   that want the exe (item 4), the recipe saves per chapter and the live
   check per area, the scratchpad copies (item 7). The routine that ran
   six waves: a brief in `analysis/` (`round10_wave6_brief.md` is the
   latest), one Opus agent per group in a worktree, `merge_group10v.sh
   <group> <scratch>` (`MOD=<module>`; it merges in the main checkout, runs
   `keepboth.py` and `one_grow.py` - **the `one_grow.py` step belongs in
   the next round's merge script from the start**: `DrawPool_Grow` is the
   last inject line and keep-both doubles or misplaces it whenever a
   branch forked before a reorder - then builds and self-tests in the
   detached worktree `<old scratch>/verify`), one merge at a time in the
   background, about ten minutes each. Scratch: the merge scripts and waves
   four to six's `<group>/` controls scripts in
   `.../71e258cd-639f-4084-8bfa-60f9e4a9ffda/scratchpad/`; waves one to
   three's in `.../0eefe2a8-ba23-4625-9434-7c4f87a1456f/scratchpad/`
   (also `verify/` and `play/`). Every agent worktree and
   `phase-3/round10-*` branch is merged and removed; round ten is `main`
   (PR #28, `3e8d531`).
1. **Owed by the spell round** (round9 doc sections 6 to 12), the owner's
   order to choose:
   - **Rebinding and `known-defects.md` are done** (2026-09-27 afternoon):
     99 raw constants and 16 phase-table literals in 38 files now name
     their targets (round9 doc section 12 "Rebinding"; what stays raw and
     why is listed there - the unowned engine helpers `0x446770`,
     `0x4514A0`, `0x494060`, `0x494110`, `0x4941B0`, and the S23 / S24 /
     S29 `_callees.h` constants their fuzz keys on). The spell round's
     defects are D89..D132 (four common classes collapsed to one entry
     each, D89..D92; D132 is `Task_Create`'s slot from EA). Four places
     where the group docs contradict each other are written into the
     entries (tint table 32 or 256 records; whether ours reads a `.data`
     dispatch in place or aborts; slot 255's address; task `0x5B`) - each
     wants one read of the code to settle.
   - **The owner's eye**: boot, the title demo, entering a game, area
     changes, F9 (the scheduler, EA); and any spell cast - every spell group
     is fuzz-only, no route casts them. A cast from the recipe save
     (`tools/recipe_saves/adult_ryu`, `combat.txt`) would reach the first
     spell functions ever run live.
   - **34 owned functions have no `entries_logic.txt` line** (round9 doc
     section 10, re-checked after wave five: the same 34): 9 are the
     wall-clock exclusions, 25 to audit - the hash matched with them
     absent, so each is covered by a host extent or off the attract path;
     say which.
2. **Housekeeping.** Round nine is merged (PR #27). The controls scripts of every
   round-nine group live in session scratchpads, not in git: waves one to
   three and the other earlier groups in
   `C:/Users/kerok/AppData/Local/Temp/claude/C--Users-kerok-Documents-GitHub-BreathOfFire3PCPort/c6020f3e-435b-4b37-a18c-94d1c71f583a/scratchpad/<group>/`,
   the kFlag re-run (`r1`..`r8`) and wave four (`s09`..`s15`) in
   `.../9e63839e-3d62-41b0-af67-04c4e6eecc8c/scratchpad/`, wave five
   (`s32`..`s38`, plus `merge_group.sh`, `keepboth.py`, `r9_hash_refs.sh`)
   in `.../2837efe3-e486-4ce9-9f2f-bd50089ad35b/scratchpad/`. A Temp folder:
   copy them somewhere durable if they are to outlive a cleanup. About 20
   orphaned `tail -f | grep` watchers from the first session (`c6020f3e`)
   may still be running; harmless, stop them when that session is closed.
   The seven `phase-3/round9-s3N` branches and the agents' worktrees under
   `.claude/worktrees/` are merged and can go.
   **The parallel-wave routine, for the next queue** (it ran five waves
   without change): a brief in `analysis/` (gitignored) with the tip's SHA
   and the session's scratch path, one Opus agent per group line in a
   worktree, merge each as it reports: merge `--no-ff`; the conflicts are
   always both-appended in `CMakeLists.txt`, `inject_all.cpp`,
   `symbols.toml`, `docs/README.md` - keep both, the CMake list's `)` on its
   last line only, and in `symbols.toml` **repeat the shared `[[func]]`
   header** when a hunk starts inside an entry; `tomllib` parse and no
   duplicate `pc`; build; the group's shadow and `BOF3X_SHADOW='*'`
   headless; `ledger_check.py` 0 errors; `analysis/consolidate_entries.py`.
   `merge_group.sh <sNN> <dir holding keepboth.py>` does all but the last
   two (it expects the branch `phase-3/round9-<sNN>`). **Expect noise while
   agents run**: an agent polling its controls script notifies "finished,
   waiting on background work" many times before its report; only the
   report (a hand-back message) means merge. A usage cut shows as `failed`
   with the agent's last line - resume it with a message naming its next
   step. Waves of about 350 functions fit one usage window.
3. **The owner's eye on older rounds**: round seven and the world map (the
   compass needle, DIV-0044; the sky's bands, DIV-0041); a fight under full
   ownership - ask whether the 2026-09-24 combat-route play counts.
4. **The other queues**, the owner's order to choose: new routes (`menu_screens.txt`, a boss, an event
   battle), the MP3 decoder's replacement (round9 doc section 3). What
   earlier rounds left unowned is listed in each group doc ("left
   original", "in no group"); the named ones are round seven's `0x43B130`
   and the boss handlers at `0x656954`, round six's `0x5806F0` /
   `Save_QuickWrite`, and WinMain's run-once callees.
5. **Localisation: four languages and what they leave.** The owner's
   decision (2026-09-28 night): the open localisation items below are
   **their own branch and effort, after the area round finishes** - not
   folded into a takeover wave.
    Built 2026-09-24
   (DIV-0054..0057; [`dialogue-localisation.md`](dialogue-localisation.md)
   sections 6 and 9). Next, in the order they bite:
   - **The owner's look in game** at French, German and Japanese.
     `AREA065` (every Western disc) and German `AREA121` rearrange the
     world-map page, and are not yet seen.
   - **Japanese: the exe's own strings.** Config, verbs, default names,
     merchant, battle labels and messages (kinds 7..12) are still Chinese.
     Their converters read US layouts, and the Latin layout patches are
     skipped for `ja` (`Lang_FullWidth`).
   - **French / German:**
     - ~466 / ~273 message slots whose European pointers leave their block;
       which slots the PC's scripts reach is unread;
     - 37 / 54 accented enemy names over the banner's 8 bytes (pair codes
       or one-byte accents would fix it);
     - ~~the title art~~ built 2026-09-29 (DIV-0014's French and German
       paragraph): the discs' own two rows and the English CONFIG.
   - **Furigana** is idea I21, for its own branch.
6. **Localisation: the exe's remaining Chinese**
   ([`dialogue-localisation.md`](dialogue-localisation.md) §6 the open list,
   §8 the method and the chunk kinds; `BOF3X_TEXTLOG=1` gives a string's
   address). Located by round seven: the place plates want the US page
   section and `0x800D3800` per world map ([`world-map-hud.md`](world-map-hud.md));
   the target banner `攻 击` is `BattleWin_DrawCommandLabel` from
   `0x669D60`, patch point the `Text_DrawAt` call at `0x443AF5`
   ([`battle_windows.md`](battle_windows.md)); the enemy names, the banner
   messages and the EX suffix are done (DIV-0053, DIV-0052); the ability names are
   the 16-byte GBK field at `0x65C4C8 + id * 0x18`
   ([`battle_window_draw.md`](battle_window_draw.md)). **Built 2026-09-29
   on `loc/remaining-labels`, DIV-0064** (chunk kind 15, `src/game/labels.cpp`):
   the list headers, the Equip column's and the battle's stats, and the
   status words, from the US disc; the list titles' centring (DIV-0059)
   extended to every list; and D174 fixed on the way (`ConfigText_Inject`
   under `BOF3X_LANG=original`). Seen by capture: `ITEM`, `HEAL`, `Pwr Def
   Int Agl` (`analysis/shots/labels_en`). **Owed the owner's eye:** the
   status words on a poisoned member, a battle's stat panel and skill
   titles, the shops' `WEAPON` / `ARMOR`; the French weapon and skill
   pages (`ARMEMENT`, `CAPACITE` - the item and skill types are repointed
   into the DLL's buffers, so nothing is kept) and the German build. Still
   Chinese, with readers
   named in [`dialogue-localisation.md`](dialogue-localisation.md) §8: the
   turn counter, the Skill Ink count's label, the shop's master / apprentice
   words, and some fifty strings of the shop and tactics screens at
   `0x669E10..0x66A0B0` / `0x66A14C..0x66A1E0`. Captures:
   `tools/recipes/menu_screens.txt`,
   `battle_commands.txt`, `combat.txt`. Also from §6 there: the seven
   character-at-a-time `Text_DrawAt` callers still at 12 px (`0x45B490`,
   `0x45B5F0`, `0x460730`, `0x460920`, `0x466260`, `0x4B1090`, `0x4B11F0`)
   plus `0x4987E0` and the 8 px UI font `0x516E70`; text in artwork;
   a better upscale; German and French (10 glyph slots free). **Closed by
   the owner, 2026-09-29:** longer names - the port widened the fields
   for two-byte glyphs, not for more letters; the boxes on screen are the
   same size, so the US disc's 12-letter names stay. Saved names
   stay as they are (owner's decision).
7. **Widescreen's debts** ([`widescreen.md`](widescreen.md) §4, §5):
   the oracle and the frame hash once with `BOF3X_WIDE=1`; the attract A/B
   cropped to the middle 640 columns; the sprite and object culls (§3b's
   table), the sky `0x571C85`, the message-box table; whether any area
   change still shows a black centre with live bands; the corner pop with
   the margin at 100. A live `BOF3X_SHADOW=map_layers` under the wide view
   reports the cull's divergence by design.
8. **[`new-code-audit.md`](new-code-audit.md), with the game on hand.** The
   2026-09-25 readability audit of the code new to the game left eight bugs
   confirmed by reading and not yet seen in game, ten unchecked reports and
   six open questions. Each has its check. The two the owner decides on are
   A1 (a surface slot reused under a pending draw) and A2 (the Japanese
   overlay switches F9's lines to English). Also the lifter's first run on
   `BOF3.exe` ([`lifter-feasibility.md`](lifter-feasibility.md) §7), output
   to `analysis/`.

## Then

Ordered; reasoning lives in [`STATUS.md`](STATUS.md), not here.

9. **`analysis/pairs_propagated.json` errors**: `0x445A30` / `0x44B9F0`
   swapped, `0x44F030` paired to the wrong twin
   ([`battle_damage.md`](battle_damage.md)). Then the divergence map of
   [`attract-remaining.md`](attract-remaining.md) §5.1 (`psx_pair.py areas
   && fill && propagate`, five minutes; never use the `call-disputed` tier),
   scenario overlays, the name import as `hypothesis`.
10. **The first receipt** ([`STATUS.md`](STATUS.md) open decisions). CI
   compiles `src/` since 2026-09-25 (`.github/workflows/build.yml`, beside the
   ledger checks in `checks.yml`). The evidence a receipt records exists: `attract_diff.py`,
   `mem_dump.py --compare`, `calltrace.py frames`, the route A/Bs.
11. **[`IDEAS.md`](IDEAS.md) I13, save states** - the random encounter is
    deterministic now (`combat.txt`), so save states are for what no route
    replays: bosses and event battles.
12. **I15, the live look toggle** - the input path is read and ours
    (DIV-0050), so it wants only a key; I19 (curvature for SatPixie) beside
    it.
13. **I18, F12 before shipping** - `Save_QuickWrite` writes a normal save to
    slot 0 from anywhere, battles included; the owner keeps it for now and
    wants it disabled or a true quicksave before shipping. The recorder's
    F12 shot lands on top of it.
14. **The display overhaul's owed checks** ([`display-overhaul.md`](display-overhaul.md)
    §5, [`window-modes.md`](window-modes.md) §6): the edge pixels of `rb1`
    - `BOF3X_PIXEL_OFFSET=0.498046875` against the 27 differing captures of
    the 55-shot attract A/B (`validate_rb1.sh`, about 25 minutes); the
    owner's tuning of SatPixie (the Options dialog by hand, only tried by
    code; our own CRT look, DIV-0037, was withdrawn 2026-09-27 so its
    tuning and rescale checks are moot); the title-bar drag under
    `BOF3X_BACKGROUND=0`; sprite edges at k = 3 / 6 looked at closely.
    Not built, loud if reached: a `Lock` of the primary or back buffer
    (`Gfx_DrawOTag` logs the first request), sub-rectangle locks, depth /
    fog / lighting, the back buffer's `GetDC`. The set-up's pixel formats
    and caps `0xCCD` are this machine's HAL's; the backend takes any RGB
    masks.
15. **Stage 2's regression check**: which of `MsgBox_Step`'s 23 control
    codes the attract sequence's eight messages use (tracer detail mode);
    nothing covers `Msg_OpenSystem` ([`attract-mode.md`](attract-mode.md) §6).
    And a check of the list the draw-order pass builds in the real game -
    I14 level 1 - with a hand-written header for the sprite object (five
    files address it by offset; [`sprite-draw-order.md`](sprite-draw-order.md)
    §5, §6).
16. **The owner, in game** ([`USER_CHECKS.md`](USER_CHECKS.md)): with
    `BOF3X_LANG=en` - the choice lists at 8 px, item and ability menus for
    clipping, a pick-up, the masters' talk, a long area (`AREA090`,
    `175`-`185`, DIV-0007); USER_CHECKS 6's two-row title layout; a save and
    load through the fully-ours file layer, DIV-0003's failing case,
    DIV-0002's clean A/B; the confused-member reversal against
    `BOF3X_ORIGINAL=Field_CopyInput`; the reserve list's frame (DIV-0011,
    ~590 sprites a frame - if anything else on that screen vanishes, the
    packet pool is full); Ability (menu state 3) open, and the Items /
    Equipment list cursors (`mem_watch.py --seconds 600 929F00:16`); the
    VRAM movers `Gfx_MoveImage`, `Gfx_MoveCells`, `Gfx_UploadLzss`, which
    only a scroll or compressed picture reaches (optionally under
    `BOF3X_SHADOW=Gfx_InvalidateTextures`); DIV-0047 over a long session
    (the old bands began at 35 minutes); DIV-0039's window title (built,
    "seen at the next run" - confirm it was).
17. **Finish reading the asset path**: the 32 callers of `LoadDatFile`, the
    value-sequence search for the dropped PSX sections
    ([`asset-loading-path.md`](asset-loading-path.md) §4). For I1, the PC
    block builder `0x5806F0` and the options bytes at block `+0x78`;
    PC-to-PSX is still static only.
18. **Obligations** ([`STATUS.md`](STATUS.md)): contact TheRealBiggs; write
    findings back to the sibling (the `0x0C` answer, `Rand` not matching).
    Still unknown, though both are ours: what the 8-byte records of
    `SpriteCell_Add` `0x5A6790` (table `0x6BEA18`, read by `0x5A32B0`) are.

## How to run things

_Verified 2026-09-24._

- **Build:** `cmake --preset i686 && cmake --build build` - llvm-mingw's
  `i686-w64-mingw32-clang++` (the `retcomm` toolchain under `~/.local/share`),
  **not** MSYS2's. The first configure fetches SDL3 (network).
- **Run:** `build/bof3x-launcher.exe --game bof3 --no-config` from a script
  (`--no-config` skips the settings dialog, [`launcher-settings.md`](launcher-settings.md)
  §4); log in `build/bof3x.log`. `BOF3X_ORIGINAL=NAME` / `=*` / `*,-NAME`
  for original behaviour. Nothing mode-sets (DIV-0032); the game runs
  unfocused (DIV-0033), so `attract_run.py --no-front` leaves the desktop
  alone for all-ours runs; `--launcher DIR/bof3x-launcher.exe` runs a copy
  with its own `bof3x.ini`. End a run with `taskkill //F //IM BOF3.exe`
  only for a game you started (the runners do exactly that now,
  [`world-map.md`](world-map.md) §6); batches run detached.
- **Self-tests, headless:** `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'
  build/bof3x-launcher.exe --game <dir> --no-config` - exit 0 passed, 3 a
  Fatal ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2). Shadow names per module
  are in each module's doc; comma-separated lists work.
- **Oracle:** `attract_run.py --out ref.tsv --original "*,-Game_Clock"`, the
  same without `--original`, then `attract_diff.py ref.tsv new.tsv`
  ([`attract-mode.md`](attract-mode.md) §6).
- **Memory dump:** `attract_run.py --minutes 2.4` and within seconds
  `mem_dump.py --label X`; `mem_dump.py --compare A B`. Always two reference
  runs.
- **Frame hash:** `analysis/validate_wm1b.sh` is the set-up; rebuilding the
  exclusion list is [`call-trace.md`](call-trace.md) §6 (`calltrace.py
  wallclock --static 59E000-5A6000,5A9600-5AB000 --also 5BC8E0,5BDA20`);
  `BOF3X_CALLTRACE_DETAIL=lo-hi` on both sides to see a differing frame.
- **Routes and captures:** `python tools/input_run.py tools/recipes/X.txt
  --out analysis/shots/X [--lang en] [--no-front]` - the game writes its own
  frames (`BOF3X_SHOT_DIR`, [`input-script.md`](input-script.md) §3); the
  owner records with `BOF3X_RECORD`. Route A/Bs: `analysis/validate_combat.sh`,
  `validate_shop.sh`, the world map's in [`world-map.md`](world-map.md).
- **Takeover recipe:** read to the last instruction; `symbols.toml` entry
  with evidence and `impl`; clone and fuzz under `BOF3X_SHADOW`, every call
  out re-aimed at a recording stand-in, boundaries seeded; plant a bug per
  behaviour and see it refused; live: oracle, dump, hash beside an
  original-vs-original pair; say in the doc what none of it reached. The
  details (clone ordering in `inject_all.cpp`, x87 `0x027F`, jump tables,
  MATRIX padding per DIV-0021) are in [`SCAFFOLDING.md`](SCAFFOLDING.md) and
  [`sprite-draw-order.md`](sprite-draw-order.md).
- **The parallel method** (rounds two to seven): split by file, each group a
  new `src/game/*.cpp` with its own shadow name and its call at the end of
  `inject_all.cpp`; **before spawning, commit and register the boundary
  callees** in `symbols.toml`; agents self-test headless only; merge one
  branch at a time, the build and `BOF3X_SHADOW='*'` after each, reading
  the build's output; resolve `symbols.toml` by entry, keyed on `pc`, then
  a `tomllib` check; `CMakeLists.txt` and `inject_all.cpp` want both sides.
  One live batch after the merge.
- **Other:** `save_convert.py` (docstring; `cygpath -m` paths in Git Bash);
  `crash_report.py` after `CRASH` lines; `loc_build.py all --disc
  "CDImage/Breath of Fire III (USA).cue" --game bof3` (a minute, 244
  overlays); `dat.py survey`; `verify_fixtures.py`; `ghidra_pc.py import`.

## In flight / uncommitted

Nothing uncommitted. Round eleven is merged (PR #30); its cleanup's cloud
half is pushed on `claude/round-10-cleanup-handoff-qtwcrk` (item 0) and
wants the `'*'` run and the other game-side checks before its PR. The wave
briefs are in `analysis/` (the template) and the session-`08306a9f`
scratchpad (the filled ones). **The other session works in the main checkout
on this branch**: check `git status` before a commit, and never build in
`build/` while its game runs (merges do not need to).

Local only, gitignored, worth keeping:

- `bof3/BOF3.CFG`, `build/bof3x.ini`; the owner's saves `bof3/BISLPS00`..`05`
  and `0F.DAT`, theirs to play in since 2026-09-26; **the recipes' saves are
  `tools/recipe_saves/*.DAT`** (`adult_ryu` - adult Ryu, Lv 38, a US
  conversion: menu square, confirm cross, cancel triangle - `town`, `combat`),
  swapped into slot 0 for a run by `input_run.py`
  ([`input-script.md`](input-script.md) §1a). Gitignored, so a fresh clone
  re-imports them from the owner's slots.
- `analysis/calltrace/wm1b_orig`, `wm1b_origb`, `wave2_ours`, `pace_ours`
  (the current hash reference and its matches); the older `ab*` references
  and their `entries_logic_09xx*.txt` lists are history. `all_b/` and
  `all_ab.callcounts.tsv`, the full-list runs the exclusion list came from.
- `analysis/attract/orig_a.tsv` (the oracle's reference);
  `analysis/memdump/clutref_a_*` / `clutref_b_*` (the dump's reference pair,
  its `clut` row stale - Traps); `slowref_*` (the `clut` region depends on
  run speed); `analysis/attract/ab12_shadow.log` (11 million calls at
  `0x027F`).
- `analysis/shots/` - every capture, A/Bs and sheets sent to the owner;
  `analysis/d1/` DIV-0010's; `analysis/memwatch/menu_state.tsv` the menu walk.
- `bof3/DAT/en.*.DAT` (rebuilt by `loc_build.py`); `analysis/font/`.
- `CDImage/` - the owner's PSX discs (USA, Japan, Germany, France) and two
  PSP images. Never commit; extract to scratch.

## Traps already paid for

_One line each, with a pointer. Add when something costs more than an hour._

- **A route A/B's off-list and scratch ini go stale**: the three
  `validate_*.sh` scripts' `DIVS` lists lacked the four 09-27 centring
  and layout divergences (DIV-0058..0061), and a scratch launcher ini
  copied from an older session had `cheat.steal=1` - the 2026-09-28 combat
  A/B "stole" and every banner differed by a few hundred pixels until both
  were fixed. After a DIV that moves pixels, add its `BOF3X_ORIGINAL` name
  to the scripts; check the ini's `cheat.*` lines before a combat run; and
  `validate_shop.sh` must pin `BOF3X_WIDE=0 BOF3X_SCALE=2` and a launcher
  copy like the other two or ours writes 1704 x 960 frames against 640 x
  480 grabs (fixed in the local copy).
- **A spell fuzz's call counts depend on the build directory**: the harness
  stores pointers into our DLL in game memory, so a worktree and the main
  checkout take different branches (Steal: 9,278 against 9,850, 0 mismatches
  in both). Judge a merge by 0 mismatches and controls refused (round9 doc
  section 6).
- **Parallel agents share `analysis/calltrace/entries_logic.txt`**; one
  agent's script emptied it (round9 doc section 10). Snapshots
  `entries_logic_0926_*_raw.txt` exist; after a wave, check every `impl`
  start has a line.
- **A token-pasting inject macro hides `BOF3_INJECT` lines from
  `ledger_check.py`** (S16's forty); write the injects out.
- **A usage cap cuts every agent at once**: tell them to commit early; a cut
  agent resumes with SendMessage, its worktree intact. Waves of about 350
  functions fit one window.
- **Past 12.4 days of Windows uptime (Restarted 2026-09-26, so about 2026-10-08; Fast Startup keeps
  it counting, only a Restart resets it) an all-original run stops pacing
  and, by the code, draws nothing** - `--original "*"` turns DIV-0022 /
  DIV-0047 off with the rest. Reference sides run `*,-Game_Clock` (the
  clock changes pace, never logic). Already past 6.2 days, all-original
  runs go at half speed (D5): hash verdicts stand (logic frames), wall
  times do not. Pace figures before 2026-09-21 are the 31.25 band.
- **An A/B original side that keeps a function ours can keep a dependency
  of it original.** `Text_DrawString`'s glyph guard was set only by
  `Font_SetGlyphData`, so with the English overlay the `*,KEEP` side
  trapped at the first battle banner (glyph `0xA6B` over `0xA00`), every
  run, tracer or not - found 2026-09-25 by the combat route, which no A/B
  had played since DIV-0052. Fixed in `LoadDatFile` (DIV-0016, amended).
  When a KEEP list keeps ours a function that reads state another of ours
  writes, keep the writer too, or set the state where both sides run.
- **An all-original side cannot write its own frames**: with `Display_Setup`
  original there is no Direct3D 11 device, `render::SaveFrame` returns false
  (`NOT saved:` in the log), and the runner falls back to the window grab at
  every frozen shot - the screen must be uncovered, and a covered one hung a
  run at its shot on 2026-09-25. A trace needs no captures: play the
  recorded route file itself (`combat.txt`), not its `_ab` twin.
- BSim produced one high-confidence wrong match; no BSim name exceeds
  `hypothesis` without a PC-side read ([`bsim-evaluation.md`](bsim-evaluation.md)).
- Indexing PSX overlays into the BSim database degrades published ranks unless
  done on a copy ([`overlay-transfer-feasibility.md`](overlay-transfer-feasibility.md)).
- Figures repeated across docs are not evidence - count them (the 29,036 case).
- A Python `'''` string or a bash heredoc eats backslashes (`\0` became two
  NUL bytes in `loc_build.py`), and a heredoc holding triple quotes or C++
  apostrophes dies with "unexpected EOF". Write source with the editor
  tools; keep backslashes out of `symbols.toml` evidence strings.
- Capcom's WndProc freezes the game when its window is not in front and
  replays the missed time unrendered ([`windowed-mode.md`](windowed-mode.md));
  ours does not (DIV-0033), so this bites under `BOF3X_BACKGROUND=0` or an
  all-original side - where **`attract_run.py --no-front` makes 0 logic
  frames** (`wm1`). Reference sides keep the foreground hold.
- A scripted run takes the language and filter from the owner's
  `build/bof3x.ini` unless the environment sets them: an English run against
  the Chinese reference looks exactly like a `LoadDatFile` regression.
  `attract_run.py` pins them; check the recording's second `#` line.
- A foreground-holding run sends **anything the owner types into the game**;
  an oracle or hash run on the owner's desktop needs keyboard and mouse left
  alone (lost runs 2026-09-19 and the wave-2 first start).
- A rebuild fails at link with "Permission denied" while a launched game
  holds `bof3x.dll`. Close the game.
- `pe_xref.py` indexes data memory operands only: "(no references)" for a
  function means nothing (use `callees` in `analysis/pc_funcs.json` or an
  E8/E9 scan), and `add eax, 0x803580` is invisible to it - walk the
  `imms` / `offs` / `globals_` lists ([`dialogue-localisation.md`](dialogue-localisation.md) §6).
- **The `clut` region is off by one palette row** in most dumps (row 506
  all-original, 482 ours): `clutref_a` predates DIV-0022. A one-row `clut`
  difference with arena and VRAM identical is that
  ([`sprite-draw-order.md`](sprite-draw-order.md) §17); re-recording is two
  all-original dumps.
- `mem_dump.py`'s `clut` region and `attract_diff.py` are unreliable under
  `BOF3X_CALLTRACE_MODE=all`. Dumps and the oracle at full speed, the hash
  under the tracer, never mixed ([`asset-loading-path.md`](asset-loading-path.md) §2).
- One disagreeing oracle frame at a state change is a torn sample until a
  re-run says otherwise; a two-frame skew from ~3376 on is the `wm1_ours`
  kind. The frame hash is the arbiter ([`attract-mode.md`](attract-mode.md) §6).
- `--original "*"` unquoted in `$(...)` or `echo` is glob-expanded, and the
  `cp` after it saves the PREVIOUS run's callframes. Check the `inject:`
  line says what the run was meant to be.
- A fuzz of random bytes barely tests a comparison with a constant: seed the
  boundaries and let the negative control say whether you did
  ([`sprite-draw-order.md`](sprite-draw-order.md) §6).
- **A frame-hash difference means nothing without an original-vs-original
  pair**: wall-clock draw code appears as tracing gets faster; `wallclock
  --static` is the fix ([`call-trace.md`](call-trace.md) §6).
- **An owned function missing from `entries_logic.txt` breaks the hash**
  with every count equal (`ab21`: 23 functions, 5,236 frames). The tracer
  reads a size only for an owned entry, so a wrong size matters once its
  function is taken over. Check the list before every batch.
- **The frame hash counts the CRT's `sscanf` by characters** (`ab26`, frame
  5524): `Mp3_MemoryIo` scans the music buffer's address in hex, so a
  shorter address is four fewer calls. Extra `0x5BCF64` calls
  (`0x5BD989` / `0x5BD9C0` pairs under `0x5B9AA6`) at one frame are this.
- **The compiler's tail call at a task's top frame shows in the hash**
  (`ab24`, `Boot_Task`): fix with `__attribute__((disable_tail_calls))`;
  deeper tail calls are harmless. Found in two minutes with
  `BOF3X_CALLTRACE_DETAIL=1-1` and a `diff`.
- The optimiser can hide a wrong build from the fuzz (strict aliasing);
  `-fno-strict-aliasing` is project-wide - do not remove it
  ([`psx-library-layer.md`](psx-library-layer.md) §2).
- **clang 22.1.8 miscompiles** `(a & 0xFFFFFF00) | ((unsigned char*)a)[8]`
  at -O2 to a plain byte load. Write it as inline asm, as `event_ops.cpp`'s
  `DirectionInPointer` does ([`event-ops.md`](event-ops.md) §8).
- A negative control NOT refused is information: the quirk may be
  unobservable (`MapView_SetElevation`, `Gte_Rtpt`), the fuzz may be blind
  (`Gte_DepthRamp` needed seeded values), or the change may change nothing
  (six on 2026-09-22). Ask whether any input could tell them apart; seed it.
  A control refused by a HANG proves less than one refused by a count.
- **A stand-in quieter than the real callee hides what the caller undoes**:
  give it the side effects the caller reads or reverts
  ([`movement-script.md`](movement-script.md) §1b).
- A fault in a start-up self-test **hangs at start-up** with nothing after
  the `cloned` lines; `Get-Process BOF3 | select CPU` near zero is a fault.
  A harness restoring a block too far can fault the original's copy.
- **A self-test runs before `BOF3.exe`'s C runtime**: anything reaching
  `_getptd` (`Rand` does) ends the process with only "could not load the
  dll". Give the fuzz a stand-in ([`sprite-draw-order.md`](sprite-draw-order.md) §16).
- A clone of a function with a jump table runs its cases in the ORIGINAL
  body; relocate the entries and the `jmp [reg*4 + table]` operand, as
  `text_draw.cpp` does.
- A fuzz can generate the original's own trap: `Text_DrawString` executes
  `in al, dx` for a glyph above `0xA00`.
- Pairing EMI sections to DAT chunks by order or address mis-pairs 47 files;
  use `dat_census.align` ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2). A raw
  byte scan of the `DAT`s drowns in audio - walk the chunks, keep kind 0;
  the movement scripts are in the exe's `.data`
  ([`movement-script.md`](movement-script.md) §3).
- A recipe's `shot NAME 10` can back out before the grab; keep the default 30
  on anything that changes ([`input-script.md`](input-script.md) §3).
- The field's buttons are save data (`0x903584` / `0x90358E` / `0x903590`,
  save 5 differs): press `@0x903584`, not a shape; `seek` the remembered
  top-bar cursor ([`input-script.md`](input-script.md) §4).
- Window grabs include Windows 11's rounded corners (mask 8 x 8 each), and a
  run not through `input_run.py` does not freeze for its shots. **A black
  screen cover makes grabbed captures black and every A/B "identical"**
  (`ab25`) - only the grab fallback now that the game writes its own frames;
  check captures are not black.
- **Agent worktrees start from `main`, not the current branch**: commit
  first, and have each agent `git merge-base --is-ancestor <commit> HEAD`
  and reset onto it.
- **`symbols.toml` does not merge by text**: git splices one group's block
  onto another's (twice in round two, again in round three). Rebuild by
  entry, three-way on `pc`, then check with `tomllib` that no address or
  name is bound twice. **Group names collide** across groups (W / Y, Z /
  V1): rename in the later group's files.
- **Our own scaffolding has ceilings, and they fail like hangs**: the
  `BOF3X_ORIGINAL` / `BOF3X_SHADOW` lists (2,048 characters), the
  tracer's owned-function table (256, then 2,048 - hit again at 3,165 ours
  on 2026-09-27, now 8,192 in `calltrace.cpp`) and the detour's owned
  table (4,096, hit at 4,097 on 2026-09-27, now 16,384 in `detour.cpp`)
  ended in a `Fatal` before the window. Read the log when a run is slow; a `Fatal` dialog blocks the
  runner until it is dismissed.
- **Do not build or self-test in the main checkout while a frame-hash
  reference side records**: the `orig` side recorded under the S32 merge's
  build made 9,675 logic frames against ~10,300 and four frames of
  25,000..33,000 calls where its twin had 2 (`r9_orig_0927_loaded`).
  Agents' worktree self-tests ran under every side without harm.
- **A failed build leaves the previous `bof3x.dll`** and the self-tests pass
  on it. Read the build's output, not only the exit code.
- **A divergence that stretches time must not stretch what the game sees**
  (DIV-0028): keep the logical state on the original's schedule, let only
  the audible or visible part linger; the traced hash with the DIV on is the
  check.
- Check a patch's expected bytes against the image, not the disassembly in
  your head (`0x461A61`); dump the bytes first.
- **`BOF3X_ORIGINAL=*` switched off the input recipe**: instruments pass
  `instrument = true`. `*,-NAME` excludes one name (`validate_shop.sh`).
- **An A/B's two sides need the same divergences**, or every capture
  "differs" by sub-pixel sampling (the first backdrop A/B).
- **The game's task stacks lie inside the main thread's stack**: DXGI's
  `Present` ran off a 16 KB task stack. Anything heavier than a few hundred
  bytes of stack runs on its own fiber (`render_d3d11.cpp`, `RunOnFiber`).
  Three hours.
- `DrawState` is a Win32 macro and `pass` an HLSL keyword.
- The crash reporter's stack scan misses the faulting frame: use the
  exception stream's context (the scratch `dumpstack.py`; worth folding into
  `tools/crash_report.py`).
- **The tracer's single step is visible to `pushfd`**: a stray step reached
  the CRT's `__except` (`0x5BA154`) and `_exit(code)` - no dialog, no CRASH
  line. `calltrace.cpp` handles it now. **A silent exit whose stack holds
  `0x5BA15F` is an unhandled exception**: `BOF3X_EXITTRACE=1` logs it
  ([`window-modes.md`](window-modes.md) §4a).
- **`renderer=0` in `bof3x.ini` selects Capcom's software renderer** on an
  all-original side; reference runs pin `renderer=1` in a scratch ini.
- **Our code calls ours directly**: `BOF3X_ORIGINAL=NAME` switches only what
  Capcom's code calls; where it matters from our side, call through
  `bof3::orig::NAME`.

## Waiting on someone else

- The owner: items 2 and 14; the choices in item 1.
- TheRealBiggs - not yet contacted ([`STATUS.md`](STATUS.md) obligations).

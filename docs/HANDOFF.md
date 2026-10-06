# Handoff — next session

**Status:** IN PROGRESS (2026-10-06: the platform round's step 3 on `phase-3/platform-round-2` - four groups merged, three divergences, 10,081 ours, `'*'` narrow and wide, the state hash live check identical on the attract sequence and all ten routes; the save-write comparison in the round doc; nothing pushed, the PR next)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

**10,065 functions are ours on `main`** (PR #42, `e047ee9b`, 2026-10-06) **and 10,081 on `phase-3/platform-round-2`**
([`platform-round-2.md`](platform-round-2.md), 2026-10-06, unpushed). Round fourteen (PR #41) was the remainder of the game's own code. Round fourteen was the remainder of the game's own code: 1,361 functions in stage A and four waves
([`takeover-queue-round14.md`](takeover-queue-round14.md)), all merged; what is not ours in `BOF3.exe` now is
the platform and library layer and the jump-table cases. Waves one to three were reviewed on 2026-10-05
([`round-14-review.md`](round-14-review.md)) and the high and medium items fixed; wave four ran the same day
with the review's lessons in its briefs (the round doc's section 13). The round's one validation, Chinese
against Chinese, is done twice: at `1bf5964`, and again at `main` on 2026-10-05 evening with every reference
pair re-recorded under the current skip list - the attract sequence and all ten routes identical to two runs of
Capcom's code on the state hash ([`state-hash.md`](state-hash.md) section 6). The tenth route's difference,
`shop`'s since before wave two, was the runner's early hand-back of slot 0, not the game's (section 5 there;
`input_run.py --slot0-hold`). The rest is [`STATUS.md`](STATUS.md)'s wave table; do not copy it here.

**The night of 2026-10-05, ten agents in two streams:** the round's end ([`round-14-cleanup.md`](round-14-cleanup.md): the rebinding, the harness fold and the host-extent lines, the review's lows and nits, every thin control, DIV-0076 at the owner's word) and the platform plan's step 2 ([`platform-round.md`](platform-round.md): the 40 live starts in four groups and **thirteen functions no catalogue ever held - game modes 3..6 and their steps**, hidden by `GameMode_Field`'s over-long `pc_funcs.json` extent; 10,009 -> 10,065). Both are on `phase-3/platform-round` (`round14-end` merged in), `'*'` narrow and wide twice over at `cdcadb9`. Two readings the owner asked for the same night: the camp-cell test's dead `0x91` compare has 91 real cells behind it (bridges, a harbour) and **the PlayStation has the same bug** (`0x801D21C4`); the save summary's mixed name and level is DIV-0076. The method that settled the first is worth keeping: for a defect left "for the owner", read the PSX twin first.

**The frame hash reference is `analysis/calltrace/r13_origb` (twin `r13_origc`,
identical on all 10,308 frames)**, recorded 2026-10-03 night at 8,648 ours
(the build `9d01ae9`; reference sides `--original "*,-Game_Clock"`,
`renderer=1`, windowed, foreground held, `BOF3X_LAYERING=0`; the batch is
`r13_live.sh` in the session-`e39af52c` scratchpad). `r13_ours` is identical
but frame 0, the set-up (as since `rb1`). `r13_orig` lost focus for a frame
and is not a reference; `r9_*` and older are history. **The hash sees less
each round**: the tracer arms only what is not ours (635 entries now) -
[`takeover-queue-round13.md`](takeover-queue-round13.md) section 18 item 6.

## Pick up here

000000000000. **The platform round's step 3 is done and live-checked (2026-10-06), on `phase-3/platform-round-2`** (cut
   from `main` `e047ee9b`; the record [`platform-round-2.md`](platform-round-2.md)). Four groups in a day - SCAN (the
   hidden-start scan: the game's own code was two functions short; `tools/pe_jumptables.py`), DEBTS (round fourteen's
   leftovers), CRT (`crt_rest`: fourteen runtime entries ours or the toolchain's), TWO (`game_last`: the two functions)
   - and three divergences the owner decided off captures the same morning: DIV-0077 (TILE_1 a quad, `BOF3X_TILE1`),
   DIV-0078 (`Cfg_Load`'s key table ours), DIV-0079 (the six LINE kinds as quads, `BOF3X_LINES`). `'*'` narrow and wide
   at every merge under `BOF3X_LANG=original` (the ini trap below); the state hash: the attract sequence and all ten
   routes identical at `inject: 10081 ours`. **Not pushed, no PR yet.** In the order they bite:
   1. **The PR**: `git log --format=%B e047ee9b..HEAD` for the sign-offs first (every commit has one; the merge
      commits carry none).
   2. **The owner's eye**: [`owner-review.md`](owner-review.md) is the one list - the three divergences in play (the
      dream scene's specks, the fishing gauge and any line, a hand-edited `BOF3.CFG`), the sound resume by ear,
      DIV-0076's load screen, the layering fix. The owner strikes what they have seen.
   3. **Left by the groups, small:** the five thin controls DEBTS listed; the 61 run-time raw calls (round thirteen's
      1.3, a decision); `pe_hidden.py` and `pe_funcs.py` still stop silently at an undecodable byte (SCAN's finding;
      fixing them regenerates the entry lists); two doc attributions SCAN corrected in its doc but not at their source
      (`mode-rest.md` section 0's "pc_funcs.json" is `pc_hidden.json`'s size; `Mp3_Create`'s evidence puts the decoder
      start at `0x5AB000`, it is `0x5ADF00`).
   4. **The platform plan's step 4**: the cutover's design ([`platform-layers-plan.md`](platform-layers-plan.md) section
      3) and the decoder question (section 2.4, I23); what is still Capcom's is the runtime's start-up, allocator and
      per-thread data, the decoder, and the software renderer's converters.
   **Mechanics that held:** briefs and scripts in the session-`7d0c9683` scratchpad (`plat2/common.md` + `brief_*.md`,
   `live_plat3.sh`, `chain_final.sh`, `launcher/` the 10,081 build). The agents' branches `phase-3/plat2-scan/-debts/
   -crt/-two` and their `.claude/worktrees/agent-*` are merged and can go.

00000000000. **Round fourteen's end is done and the platform round's step 2 with it (2026-10-05 night); both sit on
   `phase-3/platform-round`** (cut from `main` `2df90d9`; `phase-3/round14-end` merged into it at `cdcadb9`; the docs
   after). **Not pushed, no PR yet.** The records: [`round-14-cleanup.md`](round-14-cleanup.md) (sections 1 to 6: what
   each of the five agents did, the owner's decisions, the verification) and [`platform-round.md`](platform-round.md)
   (the five groups, the thirteen hidden functions, the read pass's questions answered, four proposed ledger entries,
   the merges). `'*'` narrow and wide at `cdcadb9` in two build directories: 10,065 ours, 0 mismatches; `ledger_check`
   76 entries, 0 errors. In the order they bite:
   1. ~~**The state hash live check**~~ - passed 2026-10-05 night ([`platform-round.md`](platform-round.md) section 6):
      the attract sequence identical on 10,305 ticks and the oracle at every frame, the ten routes identical on every
      tick, `Rand` counts the references'. **Two traps found on the way:** the attract side wants the window in front
      (the intro videos' frame alignment differs unfocused - a 10,009 control run diverged the same way), and any
      state-hash run wants nobody at the machine (a focus change reaches `Sound_PauseAll` / `Sound_ResumeAll`, ours now,
      and moves the sound page). The scripts: `live_plat.sh`, `live_plat_routes2.sh`.
   2. **The owner's calls**, gathered in `platform-round.md` section 4 and `round-14-cleanup.md` section 5: TILE_1 drawn
      as one point (visible: `whelpBoss`'s motes), the one-texel FT3 colour, `Cfg_Load`'s key-line overrun,
      `Sound_ResumeAll` after a fade, the `0x91` camp cells (Capcom's bug on both machines; a one-line DIV), debt 3's
      packet-pool read. DIV-0076 owes the owner's eye: save with someone other than record 0 leading, read the slot.
   3. **The PR**: one branch, `phase-3/platform-round`, after the live check; `git log --format=%B 2df90d9..HEAD` for the
      sign-offs first (every commit tonight has one; the merge commits carry none, as merges may).
   4. **The platform round's step 3**: the runtime's seventeen entry points, `rand` first
      ([`platform-read-pass.md`](platform-read-pass.md) section 3). And the hidden-start scan `mode-rest.md` section 0
      describes (every `jmp [reg*4 + imm]` in `.text`, the tables walked; each catalogue extent against where its code
      ends) - the class that hid thirteen functions from fourteen rounds.
   5. Left by the agents, small: 120 raw constants in 35 earlier-round files (`round-14-cleanup.md` 1.2); `kInflict`
      naming two targets; `Sprite_FlashClut`'s row mask; seven older hosts still covering an owned start in
      `entries_logic.txt` (section 2 there); the thin controls outside the night's lists (section 4); the 61 run-time raw
      calls (round thirteen's 1.3, still a decision).
   **Mechanics that held:** the briefs and scripts are in this session's scratchpad (`.../9b1166d3-19a7-43dc-87c1-42bff5c16eb4/scratchpad/`:
   `common.md` + `end14/brief_*.md` + `plat/brief_*.md`, `merge_one.sh` (debt 22 closed: a pass is the log's
   `self-test only: done` and `inject:` lines), `verify_tip2.sh`, `cell_scan.py` / `cell_render.py` (the area cell planes),
   `psx/` (the extracted EMIs and the twin reading), `live_plat.sh`, `launcher/` (the 10,065 build)). **Do not edit a
   script while bash is running it** (the merge script lost its place mid-run when `NOVERIFY` was added). **The classifier
   refused to stop a game process tonight, even the verify worktree's own**: a superseded run is left to finish, and the
   next verification goes to another finished agent worktree's `build/` (two were used: EA's and PW's).

000000000a. **2026-10-05: round fourteen's review, and what was done about it the same day** -
   [`round-14-review.md`](round-14-review.md) is the review, the round doc's section 12 the record of the fixes.
   Fixed: item 1 (a third of the group disturbance never ran: `sh::DisturbCase` in 22 fuzzes, every group's
   controls re-run by nine agents and 176 new ones planted on the formerly dead cases - every shadow 0 mismatches,
   no control lost, no defect of ours found), items 2 and 3 (the Rand counter on reference sides; `statehash.py`'s
   header check), item 4 (`battle_sprites.cpp`'s table by address again), item 5 (this file), and of the lows 6, 9,
   10, 11 and 18. **Open from it:** the other lows and the nits (the round's end); `field_c3`'s three new controls
   not refused at 30,000 rounds and the thin ones section 12.2 lists; `rest_1g`'s fuzz at 60,000 rounds a function
   (a minute and a half on every `'*'`).

000000000. **2026-10-05: round thirteen has had a code review, read-only, nothing changed:
   [`round-13-review.md`](round-13-review.md).** Seven reviewers read `c567ca3..0a2257d` (PR #40) without the game.
   Fix first: ~~**F1's double speed is held at x1 for up to two minutes whenever a
   streamed track starts**~~ (fixed 2026-10-05 on `phase-3/round14-end`: the hold gated on a flag only `BOF3X_SPEED`
   sets; DIV-0048's note), and ~~the Rand counter bypassed on reference sides~~ (fixed 2026-10-05, `8706e62`: the counter is an
   instrument and counts under `--original "*"`; wave one's `caughFish.txt` comparison was ours against the
   owner's recording and stands). Then E2E's 19
   `EffectKind48_State7..12_*` names, which E2D's three-state table makes unreachable (kind 0x49's cells by E2D's
   reading); `STATUS.md` and HANDOFF's "Where things stand" brought forward 2026-10-05. Eighteen low items and nits after that, each with
   where and a fix. The six commits signed off by Claude rather than the owner (rule 7) are the owner's call.

00000000. **Round fourteen, the remainder of the game's code, is under way** ([`takeover-queue-round14.md`](takeover-queue-round14.md);
   1,341 functions, stage A and four waves). On `phase-3/capture-round-fourteen`, cut from `main` at `5a94224` (PR #40,
   round thirteen) with the plan's four commits cherry-picked - `phase-3/round14-plan` sits on the history from before the
   sign-off rewrite and must not be merged. **Done 2026-10-04:** R0A, the seven shared helpers
   ([`rest_0a.md`](rest_0a.md)), merged at `ba2c3c3` and verified narrow and wide, 8,655 ours. **Wave one, R1A..R1G, merged at `4962b89`**: 341 functions, 8,989 ours, the
   attract sequence and `combat.txt` identical on the state hash, `caughFish.txt`'s `Rand` count the recording's (the round
   doc's section 9 has the record and the debts). **Wave two, R2A..R2H, merged at `f348fc1`**: 354 functions, 9,343 ours,
   four routes identical on the state hash and the shop route differing in a way older than the wave (section 10: the record,
   DIV-0073, kept by the owner 2026-10-04, the debts). **Wave three, R3A..R3G, merged at `ade9f98`**: 338 functions, 9,681 ours, verified narrow and wide (section 11:
   the record and the debts; round thirteen's mop-up is done with it). Paused there on 2026-10-04, reviewed and repaired on 2026-10-05 (items 0000000000 and 000000000a above). **Next:** wave four, R4A..R4F (326, the community band; merge order R4F R4E R4D R4A R4C R4B; `make_briefs14.py` then
   `post_brief2.py` with wave 4 and the tip; `collide.py` before each merge; the runner in two parts, each with its `END`,
   `tasklist` between). **Then one large validation, Chinese against Chinese** (the owner's decisions of 2026-10-04: no
   live batch per wave, no language overlay on either side, English a smoke test of ours only): `final_live.sh` in the
   session-`7bf3959f` scratchpad, about two hours with the machine quiet. Then the debts, and
   [`platform-layers-plan.md`](platform-layers-plan.md) (IDEAS I31) for what `BOF3.exe` still holds. The owner's decisions: three launch sessions (R0A + waves one and two; wave three; wave
   four); round thirteen's unplaced rows stay in wave three as R3E..R3G. The scripts and briefs are in the
   session-`309e3952` scratchpad (`.../309e3952-1e51-4cd8-8b59-6c0e2b38bc89/scratchpad/round14/`): `make_briefs14.py`
   then `post_brief.py <scratch> <wave> <tip>` (the `git commit -s` rule and stage A's addendum).
   **The live check from this round on is the state hash** ([`state-hash.md`](state-hash.md)): `BOF3X_STATEHASH`,
   `tools/statehash.py check REF REFB NEW`. The references are `analysis/statehash/attract_r14_orig.sh` / `_origb.sh` and
   `combat_orig.sh` / `_origb.sh` (ours identical on all 10,305 attract ticks at `dafd4a3`); the other routes want a
   pair each, and all of it wants the machine quiet. The call trace's hash (the paragraph above) still runs and sees
   what is left to it.

0000000. **The layering fix (DIV-0071) is merged into the round branch and on by default since 2026-10-03
   (`5f8b831`); what follows was written on `fix/tile-layering`, where it was off by default - the owner's
   eye in play is what it waits on.** `BOF3X_LAYERING=1`: a sprite is drawn up to three layers later while only
   walkable floor lies under its feet ([`sprite-draw-order.md`](sprite-draw-order.md) section 19 has the three shapes
   tried and why this one; the ledger entry has the rule). Captures, off against on: `analysis/shots/layering_1003/`
   (`layering_fix.png`, `fix_others.png`). The owner saw the first sheet and set the test - repair the corner on open
   ground, the forest still in front - and has not yet seen the build that passes it in play.
   - **To do with the owner:** play with `BOF3X_LAYERING=1` (a town, stairs, a bridge, followers close behind); then
     the default and a launcher key. The owner on the captures: "this looks perfect".
   - **Open:** why the owner's PlayStation emulator shots look less cut than the PC ("a layer higher") - the key
     function is the same on both; not measured.
   - **Mechanics:** the worktree is `<session e10cf965 scratchpad>/layering`, its own `build/`; live runs used
     `input_run.py --launcher <that build> --slot0-shared` while the wave-five agents' headless self-tests were up.
     The branch is off the round branch so the running sessions are not disturbed; merge it there when no merge
     runner is active.

000000. **2026-10-03, the fix wave for the owner's play reports: merged at `3f17bd1`, then validated live the same
   afternoon - the owner: "That looks right to me".** The validation (`analysis/shots/validate_1003/`, run from the
   merge worktree's build, not `build/`): GS's shout with a clean gap against Capcom's stray glyph (`bs2`, `bs2_orig`);
   CH's second fight paying 0 EXP under `BOF3X_EXP=0` where `--original Boss16_End` pays 110, the first fight 0 too
   (`bs1`); YN's shop and Manillo prompts (`shop`, `caughFish_b`); MB's backdrop wide; FL's banners, tabs and names in
   English (`camping_b`). **It found two FL defects, fixed in `42a7033`:** a space in the banner's one-byte draw was
   glyph `0xFFFA` (both fishing routes crashed in `Font_UnpackGlyph` at the first banner), and `tools/dat.py` did not
   know chunk kind 16 (`loc_build.py all` stopped after `en.FIRST.DAT`). Not seen: the master's prompt (no route), the
   stray frame line at window scale 1 (a capture is the render target, not the window - the owner's eye), the trigger-mode
   enemies (no route). As first written:
   *merged at `3f17bd1`, headless-verified, NOT yet seen in game.* Six Opus agents from `189ec55`, headless only; merged in a worktree (`fix/1003-merge`), `'*'` exit 0 narrow
   and wide (1,001 groups, 7,787 ours), `ledger_check` 0 errors, then this branch fast-forwarded. `build/` was not
   rebuilt (the owner's play DLL is still 30 September's). What merged, each with its own doc section for the live check:
   - **GS, DIV-0070** (renumbered: the capture wave took 0068): `MsgBox_EffectDraw` `0x4987E0` taken over, a space in a
     growing shout commits nothing - the 2026-10-02 crash (D197). `msgbox.md` section 9: `balioAndSunder_2.txt`, shots
     every 4 frames over 11476..11544. The owner's open question: the shout's fixed 12 px advance - judge from captures.
   - **CH, DIV-0045 amended:** the multipliers stop at 10 (a larger ini value is clamped and logged); `Boss16_End` and
     `BossWeretigr_EndMove` wrote the EXP total past `Battle_EnemyDefeated` and now go through the multiplier.
     `cheats.md` section 5. The owner recorded both Balio and Sunder routes with EXP 0, zenny 1.
   - **YN, DIV-0027 amended:** the hand and `Yes` at the load screen's spacing on the master's prompt, Manillo's two
     prompts and the shop's shared chooser; `yes-no-prompts.md` section 6 (`caughFish.txt` 3465, 3510, 3690, 3735;
     `shop.txt` 1050, 1680, 1980 against 270). Owed from the owner: a route for the master's prompt; whether
     `ItemTrade_Confirm`'s per-item prompt gets the same; the two masters-screen headers' PlayStation text.
   - **FL, DIV-0069:** the fishing banner and tabs from the disc (`loc_build.py all` first - it must print "fishing: 13
     lines, 3 tabs"), names to 12 characters; `fishing-text.md` section 6. Banner timing changed under English, so
     `campingFishing.txt` may drift: compare `randlog`. The stray frame line is Capcom's renderer (D198), not fixed.
   - **MB, DIV-0041 amended:** Manillo's backdrop tiled into the bands; `widescreen.md` section 5.
   - **WS, no behaviour change:** the shadow is covered by terrain drawn after the sprite, on world and field maps by
     one shared path, in the original too (D199). The owner wants it fixed beyond the original once the cause is
     measured: `BOF3X_DRAWORDER` (`sprite-draw-order.md` section 18) at `worldmap_sliver.txt` 1258-1260 and
     `field_view.txt` 1278-1280.
   - **Tooling:** `BOF3X_SPEED` / `input_run.py --speed N` (DIV-0048's note): x8 identical to x1 on two routes.
   - **Held:** the trigger-mode enemies (tar men, volts) - no route reaches them; a save and a recorded fight wanted.
   The owner's four routes (`balioAndSunder_1`, `_2`, `bossAndFlash`, `dragonGene`) have their `# save` lines and are
   still untracked; `balioAndSunder_2` saves to slot 6 on purpose. Today's captures: `analysis/shots/manillo_1003`,
   `shop_1003`, `speedtest`; reach traces `analysis/calltrace/reach_*_1003` (nothing uncatalogued).

00000. **2026-10-02, the owner's play notes - catalogued, nothing fixed, nothing ledgered yet.**
   - **Crash, diagnosed** (`build/bof3x.crash-30104-0.dmp`, `CRASH 0:` at the end of that run's `bof3x.log`): access
     violation in `Font_UnpackGlyph` (`tex_cells.cpp:240`) reading `0x17053EA0`, area `0x63`, message `0x24`, the
     message box in its grow effect (kind 2, flag 8 of `0x7DEE44`). Cause is Capcom's effect draw `0x4987E0` (not
     ours, called by address): `cmp cl, 0x20 / je 0x4988BE` at `0x498819` skips the glyph word `+0x16` and all eight
     u, v bytes for a space but still writes position and colour and commits the primitive, so each space of a
     growing shout draws whatever glyph index the packet buffer held. Here the stale word was `0xC254` (half of a
     float), 14 MB past `Font_GlyphData` (`0x162AA020`). The English message has `0x20` between the shout's letters;
     the dump's row of nine 23 px quads at y 176 has stale glyph words in exactly the four space slots. Proposed:
     take `0x4987E0` over, a space emits no primitive and only advances the pen - a DIVERGENCE entry and a
     known-defects entry with it. Not yet recorded in either.
   - **Enemies whose mode changes on a trigger do not change** (the owner, in play; not traced): tar men should
     take more physical damage once hit with a frost spell, volts should give extra EXP once hit with an electric
     spell. Neither happens. Unknown whether ours or Capcom's - compare against `BOF3X_ORIGINAL` first.
   - **Balio and Sunder's second fight ignores `cheat.exp=0`** (DIV-0045): its EXP presumably comes by another
     path than the one the cheat scales. Not traced. The owner's recipes `tools/recipes/balioAndSunder_1.txt` and
     `_2.txt` (untracked) reach it.
   - **The owner's four new recipes carry no `# save` line** (`balioAndSunder_1`, `balioAndSunder_2`,
     `bossAndFlash`, `sunderPeeing`, all untracked): the owner put each one's save in `tools/recipe_saves/` under
     the recipe's own name. Seen there 2026-10-02: `balioAndSunder_1.DAT`, `balioAndSunder_2.DAT`,
     `bossAndFlash.DAT`. `sunderPeeing` starts from the `balioAndSunder_2` save; the owner thinks it duplicates
     `balioAndSunder_2.txt` and is removing it - if it is still there, leave it to them. Add the `# save` lines to
     the other three before running them.
   - **The EXP / zenny multipliers should stop at 10, not 50** (the owner: 50 is humorously large for this game).
     Launcher dialog, `bof3x.ini` comment, `docs/cheats.md`, DIV-0045's text.

0000. **2026-09-30, three fixes from the owner's `tools/recipes/gameover.txt`** (a fight with Rei's Equip window, the party
   lost, GAME OVER, the title): DIV-0064's second load (the fatal the owner hit), DIV-0065 (the Equip window's stat
   labels a row up - Capcom's own offset), the loss screen's black widened (DIV-0041). Self-test `'*'` 0 mismatches,
   the recipe to `done` on ours; the recipe plays `# save combat` (the owner: the same save as `combat.txt`).
   Then DIV-0066: a pad press skips an FMV (the pump polls `PadRead_AnyInputDown` between messages; built and
   `pad_read` shadow 0 differ in a second build directory while the owner's game held `build/`'s DLL).
   Then the owner's `cutsceneAndNue.txt` (`# save nue`, the cutscene, the dialogue, the first Nue fight): its night
   tint and critical flash showed 320 wide, so every ours full-frame fill the `320.0f`/`240.0f` scan found now draws
   through `Widescreen_Fill()` (widescreen.h), armed in `InjectAll` after every self-test; nine sites the scan found
   are still Capcom's or unnamed (the DIV-0041 amendment lists them). The sunset sky of area 23's cutscene was
   Capcom's `0x4FD350`, found by a detail call trace (`BOF3X_CALLTRACE_DETAIL=380-383` under `REACH=1` and
   `BOF3X_ORIGINAL='*'`, the only `Gpu_SetPolyG4` builder): now `Gfx_DrawSkyGradient` in `area_backdrop.cpp`, widened
   and fuzzed; the glow over it (`0x4FD3E0`, the only other full-frame quad at frame 600) likewise, as
   `Gfx_DrawSunsetGlow`. The sunset now reads one colour across the frame (`analysis/shots/nue_sunset/`).
   The Nue question of `boss_sa.md` is settled by the same route's trace (fight 2 = area 23, kind 1). Then the
   owner saw trees pop at the periphery: four field x culls of ours moved out by the columns (`widescreen.md` §3b's
   table says which; the battle field's two read Capcom's `.rdata` and are left). Owed the owner's eye on the trees.
   The owner confirmed the pad skip on the intro videos, and later that day the sunset, the night shading and the
   Equip labels in game. **Owed the owner's eye:** the wide game over in a fight of their own, the trees at the
   edges, and a held pad input across a video's start (no skip until released).
   **The owner's catalogue, 2026-09-30 evening - to fix:** the master's (apprentice) "Is this OK?  Yes No" prompt
   over the party's stat panels has its hand a word's width left of `Yes` - the Chinese-fitted stop DIV-0027 moved
   for the four `Menu_YesNo` prompts, on a chooser DIV-0027 does not reach. Find its draw (`BOF3X_TEXTLOG` on the
   line, then the hand's x constant in that caller - a SHISU / SISYOU or scenario function, or a boot-resident
   chooser) and give it DIV-0027's stops, 218 on Yes and 274 on No, under a language overlay only; amend DIV-0027.
   Also: **the world map's party sprite has its shadow cut off** (three of the owner's crops in
   `analysis/shots/owner_catalogue/worldmap_shadow_*.png`, the pack-carrying walk: the shadow's ellipse ends at a
   straight edge under the feet). First tell wide from narrow (`tools/recipes/worldmap_sliver.txt` both ways), then
   whether it is the pinned sprite (`WorldMap_PinSprite`, ours in `area_backdrop.cpp`, pinned at (160, 80)) or the
   shadow's own draw; the sibling's PSX capture of the same walk says what the shadow should look like.
   Also: **the fishing minigame's control banner is still Chinese** (`analysis/shots/owner_catalogue/fishing_banner.png`:
   "鱼饵的装备" with the button glyphs, "钓鱼终了" with its button - bait equipment, end fishing). Not a dialogue
   overlay's string: find where the fishing overlay or the exe holds it (`BOF3X_TEXTLOG` on a cast), then either a
   label chunk (DIV-0064's kind 15, if the slot is a NUL-padded table) or the overlay's own text through
   `loc_build.py`; the US disc's fishing strings give the words.
   And the fishing equip menu (`analysis/shots/owner_catalogue/fishing_equip_menu.webp`), three things: (1) **the
   rod list truncates its names** - `Wooden R` for Wooden Rod - a count-limited `Text_DrawAt` like the ones DIV-0064
   and the list titles met (find the caller's count, and whether the Chinese slot is the limit or the draw's
   argument is); (2) **the three tab buttons 装备 / 资料 / 说明 are Chinese** (Equip, Data, Guide) - a verb set
   outside DIV-0018's nine `Menu_DrawButtonRow` sets, so either a tenth set in the same table or the fishing
   overlay's own strings; (3) **a stray frame line**: a vertical piece hangs right of the EQUIP and GUIDE boxes
   and a short one under EQUIP's bottom edge - a box drawn a column wider than its pieces, or pieces from the
   Chinese layout under DIV-0026-style widening; compare the same screen with `BOF3X_LANG=original` and narrow.
   **The owner's `tools/recipes/campingFishing.txt`** (`# save camping`; the camp's skill note, party choice and
   masters, then the fishing spot: equip menu, data page, a cast; `analysis/shots/camping/` every 240 frames) reaches
   all of the above and more still Chinese: the fishing banners at every step (frames 3120 "钓竿与鱼饵的装备",
   4080 / 4320 the cast's, 4560 / 4800 "鱼饵落空 鱼儿逃脱!" - the bait lost, the fish got away), the data page's
   `?????????` / `NO DATA` box, the masters screen's headers (frame 2400). **The fish is random beyond the
   recipe's reach** - the owner's run caught one, the replay did not: the fishing AI draws on something the frame
   count does not fix (wall clock? `Rand` seeded elsewhere?) - so the route is deterministic to the cast only. Worth
   a look when the fishing overlay is taken: what it seeds from, and whether a recipe run should pin it (a DIV).
   **The route does not replay on the all-original side** (`--original '*'`): ours shows the party choice at frame
   960 where the original shows black, and the original never leaves the camp room (the session's `camp_ab.png`) -
   a transition of Capcom's runs longer there than under ours, so a reach measurement of this route must be a
   trace on our side (plain `BOF3X_CALLTRACE`, which arms only what is not ours), not `REACH=1` on the original.
   Which transition, and why the frame count differs, is worth knowing: it is a divergence no ledger entry names.
   **The route's reach on our side** (plain `BOF3X_CALLTRACE`, 2026-09-30): 27 Capcom functions entered after the
   boot, all unnamed - 11 in the camp (the skill-note and masters windows: `0x596330`'s host of 14 + 11 hidden,
   `Window_Handler7KindTable`'s kinds at `0x59C110`.., `0x58BD50`, `0x591AC0`) and 16 in fishing (`0x52AF80`..
   `0x52CD47`: three hosts of 6, 8 and 18 recorded functions plus 11 hidden - the cast, the lure, the fish and the
   fight). Round 14 candidates: **the camp's window kinds and the fishing minigame**, about 80 functions, with the
   route to reach them. The owner on the fish, 2026-09-30: the placement looks fixed by the frame and only the
   activity random, so the random draw is in the bite, not the cast; a re-recording that catches a fish may replay.
   **It did not** (`tools/recipes/caughFish.txt`, `analysis/shots/fishing_catch/`): the fish sat elsewhere on the
   replay and the cast found nothing - the placement is random too. **Why, read 2026-09-30:** the game's `Rand`
   `0x5B93D2` is the MSVC6 CRT `rand()` (per-thread seed at ptd + 0x14), and **no `srand` is in the binary** (the
   linker dropped it: no call stores anything but rand's own product to that slot), so the sequence is fixed from
   boot - which is why battles replay. The fishing code (`0x52AF80..0x52CD47`) reads no clock; it calls `Rand`. What
   moves the sequence off the frame count is **draw code that calls `Rand` once per rendered frame**:
   `MapCell_DrawRising` `0x570660` (ours, `map_cells.cpp`: eight squares, one `Rand` each, every frame it is
   drawn), and a recipe's skipped frames replay as unrendered logic (win_main.cpp), so the number of draws - and of
   `Rand` calls - between two inputs depends on how fast the machine rendered. The water-side spot draws those
   cells. Anything else on the draw side calling `Rand` does the same (to list: the `Rand` callers among the
   draw-pass functions). **The fix is a DIV:** give draw-side callers a generator of their own (a private LCG
   stepped per drawn frame, seeded from `Frame_Counter`), so the logic's `Rand` stream depends on logic frames
   alone - fishing, encounters, item drops all replay, and nothing the player sees changes but the sparkle's
   exact pattern. **Done the same evening as DIV-0067, opt-in:** `BOF3X_DRAW_RAND=1`, or `draw_rand=1` in the
   launcher's ini (no dialog box yet); two switched replays of `caughFish.txt` identical frame for frame. A catch
   wants a recording made with it on; the owner's ini has it on for the fishing save. Recipes recorded with it
   off (every one before this) stay as they were: the key is off by default. **The catch replays** (the third
   `caughFish.txt`, 2026-09-30 night, the owner watching): the first two recordings under the switch missed on
   replay because their walk diverged at a ledge on the world map - a press shorter than a frame boundary -
   not because of the sequence; spacing the presses fixed it. Proof the sequence is fixed now: **recorded and
   scripted runs log `randlog     frame F rand K`** (the running `Rand` count, a counting replacement over a
   byte-copy of the CRT's; `input_script.cpp`), and the recording's and the replay's were identical on every
   one of 3,889 frames (`analysis/shots/fishing_catch2/randlog_*.txt`). That instrument stays: the first
   differing frame between two logs names any future consumer.
   **CORRECTION, later that night: the "per rendered frame" mechanism above is wrong, and DIV-0067 rests on
   nothing.** The frame loop runs all game code every logic frame; a late frame skips only `Gfx_DrawOTag`. The
   fish differed because this session's shot copies added a frame per shot (fixed by the other session in
   `9689c71`; use `tools/recipe_shots.py`, never an ad-hoc splitter) and because of the ledge walk. DIV-0067's
   entry carries the correction. **The switch is removed** (the owner's word, the same night: code, launcher
   key, ini line; DIV-0067 a withdrawn record), and `caughFish.txt` needs no new recording - replayed without
   the switch its `Rand` count matches the recording's on every frame and the fish is caught. Also
   suspect for the same reason: the note above that `campingFishing.txt` "does not replay on the all-original
   side" - that run used a shifted shot copy; recheck with a `recipe_shots.py` copy before believing it.
   **The catch route's reach** (plain trace, one end shot, the catch and Manillo reached): the same 16 fishing
   functions as the camping route (`0x52B1B0`..`0x52CCD0`) and nothing more armed - Manillo's screen is the shop
   code already ours or table-reached. **Manillo's screen, for the catalogue**
   (`analysis/shots/owner_catalogue/manillo_will_that_be_all.png`): "Will that be all?  Yes No" has the hand a
   word left of `Yes` (DIV-0027's stops again, with the master's prompt and, the owner says, two more
   pointer-to-choice mismatches on that screen), and **its tiled backdrop is 320 wide under the wide picture**.

000. **Round thirteen, the effect engine, is merged and its end is done but for one item: six waves, 35 groups and
   stage A, 6,891 -> 8,648 ours** ([`takeover-queue-round13.md`](takeover-queue-round13.md); section 18 is the
   round's end). Done 2026-10-03: the rebinding (58 constants, [`round-13-cleanup.md`](round-13-cleanup.md)), the
   defects D200..D238, the harness's end fold and the last two listed fills (all nine of DIV-0041's widened), and
   **the live checks** - the attract hash and six recipes (whelp, `cutsceneAndNue`, dragon, combat, shop, world
   map), every one identical to the original but frame 0. DIV-0068 and DIV-0072 have the owner's word (kept);
   DIV-0071 (layering) is on by default; nothing is pushed. **Left:** the rows in no group (about 90 functions, a
   mop-up takeover wave - each wave's section lists them; the fishing rows are round fourteen's); the 61 run-time
   raw calls of ours into ours ([`round-13-cleanup.md`](round-13-cleanup.md) 1.3, a decision); a launcher key for
   the layering; **for the owner in game** [`USER_CHECKS.md`](USER_CHECKS.md) item 8 and DIV-0071 in play. The
   scripts are in the session-`56ff1eb2` scratchpad (`.../56ff1eb2-8c2d-4d5f-82f0-a85df7f2d489/scratchpad/round13/`).
   **The verification worktree is this queue's.** The main checkout's `build/bof3x.ini` has `wide=1` and a running
   game locks its DLL. A full `verify_tip.sh` takes 22 to 31 minutes. **The tracer's tables are 32,768 since
   `9d01ae9`** (8,192 was hit at 8,648 ours: check the ceilings in HANDOFF's traps before a round's first traced run).
00. **Round twelve is complete** - [`takeover-queue-round12.md`](takeover-queue-round12.md) is the record: 654 functions
   in fourteen groups, 6,237 -> 6,891, the tip `0e51ec7` live-checked (its section 9: the attract hash and five routes
   identical but frame 0, the pictures at their baselines). On `phase-3/capture-round-twelve` from `main` `430f34b`,
   pushed 2026-09-29 and **merged as PR #33 (`d1b411c`)**. Next, in order: the round's debts (section 7 there: the
   ~~mask and stand-in folds into both harnesses~~ (2026-10-01, on round thirteen's tip: `boss_harness.md` 10.10,
   `scenario_harness.md` 8.6, **merged as PR #39** - **the next `'*'` moves FC1..FS's counts**, two field regions added;
   0 mismatches the bar), ~~the defects to number~~ (D175..D196, PR #37), ~~the rebinding between the groups~~ (82
   constants, PR #38), and two tools written 2026-10-01 in a cloud session: the pointer
   scan as `band_rows.py --pointer-scan` (**first run by the owner 2026-10-01**, [`band-rows.md`](band-rows.md) 7.1:
   FO's `0x578A40` as expected, FC2's and FC3's named since wave two so they cannot fire, 305 hits of which six are
   real - BE5's `Effect_Handlers` slots 4, 7, 11, 47, 91 and `ShopMode_States[9]` `0x583350` - the noise filtered in
   the code since; the second run (7.2 there) 33 rows, thirteen of them candidates for their band owners to read;
   the fourth run **29 rows, the expected set, the flag settled** (7.3); the regression without the flag identical on all seven outputs (`9479e06` against `7fe6406`), **debt 5 closed**; the branches of debt 7 are verified merged and the delete command is in the round doc's item 7) and
   the audit of the 33 owned starts without an `entries_logic.txt` line as `tools/entries_audit.py` (give it `--exe`,
   `--exclude analysis/calltrace/wallclock_reach.json` and each route's reach `bof3x.calltrace.tsv` as `--reach` -
   the reach runs are default-mode traces, `callcounts.tsv` is `MODE=all`'s; **run 2026-10-01**, the verdicts in the
   round doc's section 7 item 4: 9 covered, 9 excluded, 16 lines owed, which `--append` writes, and 49 duplicates,
   which `--dedupe` resolves - both on the machine with `analysis/`; **then the hash reference is re-recorded**, the
   armed set having grown by 16)); **round thirteen**, the effect engine
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
     section 10, re-checked after wave five: the same 34; 33 since
     `Sparkle_Launch` got its line): 9 are the wall-clock exclusions, the
     rest to audit - the hash matched with them absent, so each is covered
     by a host's registered range or off the attract path; `tools/entries_audit.py`
     says which (item 00 above).
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
  owner records with `BOF3X_RECORD`. `--speed 8` runs a recipe fast (`BOF3X_SPEED`,
  DIV-0048's tooling note: same frames, same shots; compare the `randlog` against an x1 run once per route). Route A/Bs: `analysis/validate_combat.sh`,
  `validate_shop.sh`, the world map's in [`world-map.md`](world-map.md).
- **What the live runs cost:** `attract_run.py` and `input_run.py` append a line a run to
  `analysis/run_times.tsv` (start, tool, what, wall seconds, the recipe frame reached, status,
  `BOF3X_ORIGINAL`, the tracer's variables, the launcher's directory); `python tools/run_times.py
  [--since "2026-10-03"] [--by tool|what|side]` sums it, with frames / 60 beside the wall time - the
  part a fast-forward could remove (the owner's question, 2026-10-03; logged from round thirteen's
  wave three on).
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
- **A suspected bug against the PlayStation:** read the twin ([`psx-twin-check.md`](psx-twin-check.md)) - the sibling's
  `tools/disc_ls.py --extract` and `tools/emi.py list`, capstone MIPS32 LE; find it by the caller's constants, not the
  function's (GCC folds small compares). Same on the PSX = Capcom's, a DIV; different = the port's, a restoration.
- **Other:** `save_convert.py` (docstring; `cygpath -m` paths in Git Bash);
  `crash_report.py` after `CRASH` lines; `loc_build.py all --disc
  "CDImage/Breath of Fire III (USA).cue" --game bof3` (a minute, 244
  overlays); `dat.py survey`; `verify_fixtures.py`; `ghidra_pc.py import`.

## In flight / uncommitted

`phase-3/platform-round-2` holds 2026-10-06's work, unpushed (item 0); `phase-3/platform-round` is merged (PR #42). The ten agent branches `phase-3/r14end-ea` .. `-ed2` and `phase-3/platform-ph/pl/pm/ps/pw` and their `.claude/worktrees/agent-*` are merged and can go, with the older ones: the six wave-four worktrees (`phase-3/round14-r4a` .. `r4f`), `feature/name-entry-scoping` and the nine `fix/r14-review-controls-*`.
Before it: nothing uncommitted. Round eleven is merged (PR #30); its cleanup's cloud
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

- **A stopped background chain kept running** (2026-10-06): a `bash chain.sh` launched in the background and then
  stopped through the harness left its child shell alive; a second launch of the same chain ran beside it. Both
  waited on `tasklist` for the previous launcher, both slipped through the one-second gap between its narrow and
  wide runs, the second's `cmake --build` failed (`ninja: error: opening deps log: Permission denied`, the launcher
  exe unwritable) and left the **previous tip's DLL** in `build/` - so a live check that read `inject: 10079 ours`
  had validated the tip before. Read the `inject:` count against what the tip should report before trusting a
  chain's result, and give one chain the machine: check `tasklist` for a stray `bash` or launcher before starting
  another.
- **`BOF3X_SHADOW='*'` in the main checkout's `build/` fails under the owner's settings** (2026-10-06): `build/bof3x.ini`
  is the owner's (`language=en`), the launcher exports it as `BOF3X_LANG`, and under English the `ConfigText` patch
  re-aims the call at `Config_DrawRowLabel + 0x9F` - which `field_c1`'s clone check reads (`FATAL: ... the site is
  re-aimed already, cannot clone`, exit 3, 978 of the 1,056 `MISMATCHES` lines reached). An environment variable wins
  over the ini: run every self-test with `BOF3X_LANG=original` (the agents' worktrees never saw it - their inis are
  defaults). Do not edit the owner's ini.
- **A `shot` line is a frame of the route** (2026-09-30). `shot NAME 1 [BUTTONS]` holds its buttons for one frame; a
  shot inserted without taking that frame out of the run it splits puts every later input a frame late. An ad-hoc
  splitter did that to copies of the owner's recordings - menus and dialogue tolerated it, a fishing cast did not, and
  three committed recipes had to be repaired (`9689c71`). Use `tools/recipe_shots.py RECIPE --every N --out COPY`,
  which keeps the total; a hand-placed shot replaces a frame (decrement its neighbour).

_One line each, with a pointer. Add when something costs more than an hour._

- **Every commit wants its `Signed-off-by`** (`.github/workflows/dco.yml`, CONTRIBUTING.md): seven commits made by
  cloud sessions on 2026-10-01 and 2026-10-04 had none and PR #40's check failed. At the owner's word the branch was
  rewritten on 2026-10-04 (`git filter-branch --msg-filter` from `69e9d3c`, messages only: every tree identical) and
  force-pushed: **185 commits from the old `2ab349b` on have new hashes**, the docs' citations were rewritten to
  them, and the old-to-new map is `analysis/round13_signoff_sha_map.tsv`. The cloud sessions' SSH signatures on
  the rewritten commits are gone (a rewritten commit cannot keep one). The local group branches
  (`phase-3/round13-*`, `fix/1003-*`) and `backup/round13-before-signoff` still point at the old commits. Commit
  with `git commit -s`; check `git log --format=%B <base>..HEAD` before opening a PR.

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

- The owner: [`owner-review.md`](owner-review.md) (started 2026-10-06: the sound resume by ear, DIV-0076's load screen, the layering fix in play, the TILE_1 quad's go-ahead; the camp cells and the FT3 colour parked; `Cfg_Load`'s overrun decided and unbuilt); items 2 and 14; the choices in item 1.
- TheRealBiggs - not yet contacted ([`STATUS.md`](STATUS.md) obligations).

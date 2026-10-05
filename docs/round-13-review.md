# Round thirteen: a code review of `phase-3/capture-round-thirteen`

**Status:** DRAFT (2026-10-05) - a read-only review of the round as merged
(PR #40). Nothing was changed: every item below is open, for the owner to
schedule. Line numbers are at the round's tip `0a2257d`. Round fourteen's
rebinding touched some of these files since then. The items checked again on
`phase-3/capture-round-fourteen` at `f3d5e99` still hold (marked **still
open at f3d5e99**).

## What was reviewed, and how

The range is `c567ca3..0a2257d`: 314 files, about 112,000 lines added, 259
non-merge commits. It holds the effect engine (stage A EGT, EKH, waves one to
six, 1,757 functions), the harness folds, the rebinding, the fix wave
(DIV-0027, -0041, -0045, -0069, -0070, -0071) and the tooling.

Seven reviewers read it in parallel by area. The areas were the six effect
waves (four reviewers), the harness and hook layer, the behaviour fixes
outside the effect engine, and the tools and docs.
The findings marked *verified* were re-read by the coordinating session
before they were written here.

**What a review without the game cannot see.** `BOF3.exe` was not on the
machine. So no reimplementation was checked against Capcom's bytes, and no
`CallSite` offset, clone extent or table content was checked against the
image. What was checked is the source: C++ correctness, the hard rules,
code against its own comments, docs and `symbols.toml`, and whether a fuzz can
fail. The checks run by script, all clean:

- every clone row's base and extent against `symbols.toml`;
- every `kTables` count against its `[[data]]` count;
- every `BOF3_INJECT` list against its clone table;
- every `// original 0x...` comment against its function's `pc`;
- the round's 2,063 new `symbols.toml` entries, for duplicate `pc`s, duplicate
  names, overlapping ranges, and an `impl` file that exists and is in
  `CMakeLists.txt`;
- every `switch (h % N)` disturbance, for dead or missing cases;
- E5A's 29 clone-base selectors, for off-by-ones, overlaps and gaps;
- the ledger's numbering, fields and links, and `known-defects.md` D200..D238;
- hard rule 1 over every added line (nothing found that is game data).

Two clang `-fsyntax-only` passes, over waves two, five and six with a
generated `symbols.gen.h`, were clean apart from 64-bit artefacts. **No
stub was found:** the only `return 0;` hits are Capcom's documented `al 0`
answers.

## Findings

### High

1. **F1's double speed is held at x1 while streamed music plays.** *Verified;
   still open at f3d5e99.*
   - **Where:** `src/game/win_main.cpp:610` computes the stream hold as
     `g_speed != 1 && RunSpeed_StreamPlaying()`. `g_speed` is also F1's
     toggle (`win_main.cpp:445`).
   - **Cause:** `RunSpeed_StreamStarted()` runs on every `Sound_LoadStream`,
     the music path included (`src/game/save_menu.cpp:306` and `:316`).
   - **Failure:** a player presses F1 while a streamed track plays. The
     overlay says "Speed x2", but the loop stays at x1 for up to 3,600 frames,
     about two minutes, until the 3,600-frame cut-off releases it. The next
     track change starts the hold again.
   - **Why it matters:** DIV-0048's amendment calls the hold tooling with no
     change to play. `src/hook/run_speed.h` says the loop asks only "while
     BOF3X_SPEED is above 1". F1 is the feature the owner signed off.
   - **Fix:** gate the hold on whether `BOF3X_SPEED` set the speed (its own
     flag), not on `g_speed`. Not measured in play: how often a track goes
     through `Sound_LoadStream`.

### Medium

2. **The Rand counter is switched off on Capcom's side by `--original "*"`.**
   *Verified; still open at f3d5e99.*
   - **Where:** `src/hook/input_script.cpp:642`, `RandCountStart` installs
     `CountingRand` with `bof3::Inject("Rand", ...)`.
   - **Cause:** `Inject` obeys `BOF3X_ORIGINAL` (`src/hook/detour.cpp:84-99`),
     and `*` matches `Rand`. So `WriteJmp` patches *our* counter to jump
     straight to Capcom's `Rand`, which stays unpatched.
   - **Failure:** every reference-side recipe run (`input_run.py
     --original "*"`) logs `randlog ... rand 0` on every frame. Line 643 still
     logs "Rand counted". A `randlog` comparison against ours then differs from
     frame 1, and the inject counts gain one row.
   - **Fix:** a raw `WriteJmp` outside `Inject`. The same file's latch retarget
     already does this, with the comment "Not an Inject: ... BOF3X_ORIGINAL has
     no say". The other fix is a `-Rand` exclusion in every reference side.
     Check first whether round fourteen's `caughFish.txt` Rand-count
     comparison ran its original side under `*`.

3. **E2E names 19 functions as kind 0x48's states 7..12, but kind 0x48 has three
   states.** *Verified.*
   - **E2E's claim:** `src/game/effect_2e.cpp:9-12` says E2D's
     `EffectKind48_Run` 0x4781B0 reaches "kind 0x48's states 7..12" through
     0x654578. Its section headers, the `EffectKind48_State7_*` ..
     `_State12_*` names, their `symbols.toml` evidence and
     `docs/effect_2e.md` say the same.
   - **E2D's code:** `EffectKind48_Run` (`src/game/effect_2d.cpp:667`) is
     bounded by `EffectKind48_States_count`, which is 3, and aborts past it.
     Kind 0x48's own states only set `+1` to 1 and 2.
   - **E2D's names for the same cells:** kind 0x49's (`0x4789B0` is
     `EffectKind49_V4Run`; `effect_2d_callees.h:33` says "0x4789D0, kind
     0x49's variant 4").
   - **What it means:** either 19 names and their evidence misattribute the
     functions, or a script hands kind 0x48 a `+1` past 2 and ours aborts where
     E2E says the original runs on. `docs/effect_2e.md:43-44` admits "not
     traced". D200 lists the round's other table disagreements, but not this
     one.
   - **Fix:** a rename (rule 3: the values stay), after a trace of who sets
     `+1`.

4. **STATUS.md was never brought forward for round thirteen.** *Verified.*
   **Fixed 2026-10-05** for STATUS: dated, its head at 8,648 on `main` and
   9,681 on round fourteen's branch, rows for round thirteen's stage A, six
   waves, fix wave and end, and round fourteen's three waves; the 09-30 and
   10-01 rows' 7,571 corrected to 7,570 (round thirteen's branch after wave
   two: 7,568 and the two sky draws). HANDOFF's "Where things stand" is
   still to do.
   - **Stale:** `docs/STATUS.md` is dated 2026-10-01 and still heads "6,891
     functions are ours". No row covers round thirteen's groups or
     DIV-0068..0072. CLAUDE.md calls STATUS authoritative.
   - **Inconsistent:** its own rows disagree. The 09-30 row adds 2 to 6,891 and
     reports 7,571.
   - **At the round's tip, HANDOFF also contradicted itself:** its status line
     was a garbled merge of two lines ("7,568 ours, merged, 8,648 ours"). Its
     summary paragraph called `main` round eleven at 6,891, which is round
     twelve's total.
   - Round fourteen's HANDOFF has a new status line. Its "Where things stand"
     paragraph still has the old numbers.

### Low

5. **E3D's full-screen quad is not widened, and DIV-0041 does not list it.**
   - **Where:** `src/game/effect_3d.cpp:152-171` (`FullScreen`) draws kinds
     0x76, 0x79, 0x7A, 0x7B and 0x7C as one full-frame quad over (0,0)..(320,320).
   - **Ledger:** DIV-0041's round's-end paragraph (`docs/DIVERGENCE.md:2213-2229`)
     says "all nine ... are now widened". It lists `EffectKind96_Pulse`, the
     same shape, as left narrow, but not this one. `widescreen.md` §5 omits it
     too.
   - **Failure:** under `BOF3X_WIDE=1`, kind 0x7A's darkening and 0x7B/0x7C's
     red pulse leave the side bands untouched.
   - **Fix:** one line in DIV-0041's "not widened, the owner's call" list. No
     code change.
   - *Verified.*
6. **Effect mode does not seed or compare most of `MessagePools`.**
   - **Where:** `src/game/scenario_harness.cpp:1552-1559` skips the field
     fold's `{0x803580, 0x400}` in effect mode, on the grounds that its own
     `kMessagePools` region holds it. That region is 0xE8 bytes
     (`scenario_harness.h:129-130`).
   - **Failure:** a wrong message id in an offset word past +0xE8 reads the
     same empty bytes on both passes. This is the blind spot FE1's fold closed
     for the field groups, open again for every effect group.
7. **`Gte_RotTransPers`'s effect override hashes 8 bytes.**
   - **Where:** `scenario_harness.cpp:1031`. The field row (`:747`) and
     `scenario_harness.md` §8.6 hash 6, because the fourth short is the stale
     pad (DIV-0023).
   - **Failure:** a false mismatch is possible. A missed one is not.
8. **`PreviewEffect` writes through argument pointers without checking them.**
   - **Where:** `src/game/boss_harness.cpp:598-607` writes through `a[2]` and
     `a[3]` with no `Writable` check. The scenario harness's twin,
     `FxPreviewSet`, guards every access.
   - **Failure:** a stand-in handed a random word writes wild memory instead of
     being refused.
9. **The shop's re-spaced line and its moved hand are separate flags under one
   patch name.**
   - **Where:** the patch name is `ShopYesNoLayout`, set in three modules
     (`menu_draw_helpers.cpp:167`, `shop_states2.cpp:116`, `field_s.cpp:1290`).
   - **Failure:** with `BOF3X_ORIGINAL=ShopWin_TitleRun`, Capcom's line comes
     back but ours still moves the hand. The hand then points at nothing.
10. **DIV-0027 contradicts itself on `ItemTrade_Confirm`.**
    - **Ledger:** one amendment says the prompt now has the layout. The fix
      wave's later amendment still lists it under "Not touched".
    - **Switch:** the code registers `PatchBytes("TradeConfirmLayout", ...)` at
      `field_e2.cpp:1954`. Neither the ledger nor `yes-no-prompts.md` names
      it, though every other YN switch is named in both.
11. **The "off by default" text for DIV-0071 is stale.**
    - **Where:** `known-defects.md:4800` (D199) and `world-map.md:349`.
    - **Fact:** DIV-0071 has been on by default since 2026-10-03
      (`layering.cpp:225`). *Still open at f3d5e99.*
12. **Only `attract_run.py` pins `BOF3X_LAYERING=0`.**
    - **Where:** `tools/input_run.py`, the recipe A/B runner, does not pin it.
      DIV-0071 and the round doc say the `validate_*.sh` scripts do, and no
      such script is tracked.
    - **Failure:** a recipe A/B against `--original "*"` compares Capcom's
      draw order with ours under DIV-0071.
    - *Still open at f3d5e99.*
13. **DIV-0066 describes `GetMessage` wrongly.**
    - **Where:** `src/game/fmv_play.cpp:114-130` and DIV-0066 say the
      original's `GetMessage` returning 0 "left [WM_QUIT] there".
    - **Fact:** `GetMessage` removes WM_QUIT when it returns 0. So the original
      lost a quit during a video, and ours re-posts it and quits. That is
      probably the better behaviour, but it is a divergence the ledger calls
      faithful.
14. **The pad starts again on every pump of an intro video.**
    - **Where:** `PadRead_AnyInputDown` (`pad_read.cpp:273-281`) calls
      `StartSdl()` on every pump of the video loop.
    - **Failure:** where `SDL_Init` fails, that is about 60 retries and log
      lines a second for the length of each video.
15. **`entries_audit.py` keeps only the last of duplicate entry lines.**
    - **Where:** `tools/entries_audit.py:203-211`. The tracer registers every
      line, which the script's own docstring says.
    - **Failure:** `00401000 100` followed by a bare `00401000` reports an
      owned start inside it as uncovered. `--append` then adds an unneeded
      line. Reproduced with a two-function file.
16. **Six commits carry Claude's sign-off, not the owner's.**
    - **Which:** `7350a93`, `5ada221`, `7fe6406`, `6f004bf`, `210a989`,
      `47ac577` carry only `Signed-off-by: Claude <noreply@anthropic.com>`.
      Rule 7 says the line is the owner's.
    - **Why CI passed:** `dco.yml` accepts any name, so CI did not catch them.
      They are in `main`. Rule 7 forbids a silent rewrite, so this is the
      owner's call.
17. **`effect_5f.cpp` never cites DIV-0072.**
    - **Where:** its header (`:30`) says "No divergence". DIV-0072 is at its
      `EffectKind18Sub4B_Run` (`:794`).
    - **Effect:** `ledger_check.py --verbose` reports DIV-0072 as not cited in
      `src/`.
18. **E2B and E2D still say "No divergence" after DIV-0041 widened one function
    in each.**
    - **Which:** `EffectKind38_DrawTint` (`effect_2b.cpp:644-647`) and
      `EffectKind46_DrawFlash` (`effect_2d.cpp:542-545`).
    - **Where the text is wrong:** `effect_2b.cpp:22`, `effect_2d.cpp:28`,
      `effect_2b.md:359-362` and `effect_2d.md:179-183`. The ledger itself is
      right.
19. **The round-13 cleanup doc misdescribes E5A's selectors.**
    - **Where:** `round-13-cleanup.md:96` says E5A's `In(lo, hi)` bound `hi` is
      "often an exclusive bound that happens to be the next function".
    - **Fact:** `In` is inclusive (`effect_5a_fuzz.cpp:241`), and every `hi`
      is its range's last member. All 12 ranges and 5 tests were checked: no
      off-by-one.
    - **Risk:** a tidy pass that trusts the doc would drop each range's last
      function. `_06_Draw` would then Fatal on about 40% of seeds.
20. **D206's address is wrong.**
    - **Where:** `effect_1b.cpp:133`, `effect_1b.md:209` and
      `known-defects.md:5485` give 0xFF's record at `0x7E91E0`.
    - **Fact:** it is `0x7E11E0 + 0xFF * 0x80 = 0x7E9160`, which
      `known-defects.md:3162` and `:5876` already say.
21. **E2F's "near points" seed does not make near points.**
    - **Where:** `effect_2f_fuzz.cpp:395-397` copies one coordinate, plus 1.0,
      into an independently chosen one. Half the time x gets y + 1.
    - **Effect:** it weakens the coverage of kind 0x52's angle step. It cannot
      fail falsely.
22. **E2C's size guard is checked once.**
    - **Where:** `EffectKind6B_Scatter` (`effect_2c.cpp:953-957`) checks
      w/h > 0xFF once, then reads `width` again inside the loop.
    - **Effect:** latent only, because the frame is constant `.data`.

### Nits

- **Stale comments after the rebinding:** `effect_4f_callees.h:9-11` and
  `effect_4a_callees.h:10-11` still say "raw until it merges".
- **Constant names that do not match their symbols:** E1A's `kItemListA` and
  `kItemListB` are crossed, which `round-13-cleanup.md` §1.1 already records.
  Rename before someone "fixes" the values.
- **Two divergence lines log even when the patch is off:** `effect_1e.cpp:1186`
  and `effect_1g.cpp:529` log their DIV-0027 line after `PatchBytes` whether or
  not the patch was refused under `BOF3X_ORIGINAL`.
- **UB at INT_MIN:** `effect_5d.cpp:79`'s `Abs` is `v < 0 ? -v : v`, and
  `EffectKind18Sub21_DrawSpiral` can hand it 0x80000080. The other groups use
  `(v ^ m) - m`.
- **`Ftol` bound:** the `long double` versions (`effect_5d.cpp:96`,
  `effect_6d.cpp:88`) miss [2^63 - 8, 2^63). The `double` version in E6C is
  right.
- **Overlapping tables:** E5A's `kHeights9` and `kFe9` overlap
  (`effect_5a_callees.h:72-73`). The fuzz passes, so this is probably Capcom's
  layout, but no doc says so.
- **A type pun:** `effect_gte.cpp:95` passes `reinterpret_cast<long*>(&out)`
  for a `float*`. It is harmless today.
- **E2G:** `kTextLo` / `kTextHi` are dead (`effect_2g_callees.h:52`), and
  `PickOf(..., 0x101, ...)` is stored into a byte (`effect_2g_fuzz.cpp:297`).
- **Harness doc counts:** `scenario_harness.md` lines 672, 817 and 829 give
  `kEffectOverrides` 10 rows and `kEffectStd` 109. The tables hold 16 and 110.
- **Exclusive extents:** 107 new `symbols.toml` entries write their evidence
  extent exclusive (E2B 0x4731A0.., E5A 0x4FD2E0.., `Gfx_DrawSkyGradient`,
  `Gfx_DrawSunsetGlow`). Every other group writes inclusive.
- **Diagnostics:** `draw_order.cpp:52-60`'s `Find` trusts stale tags.
  `layering.cpp:215-218` can print a stale stopper, and its `EnvInt` silently
  ignores a value of 16 characters or more.
- **Docs and tools:** `DIV-0045` "New behaviour" still says 0..50. The code
  and the launcher say 0..10.
  `area_backdrop.cpp:150` / `:178` widen through `Widescreen_Live()` where
  DIV-0041 says fills use the gated `Widescreen_Fill()`. That is safe today
  only because of the inject order. `recipe_shots.py` drops comments after the
  first frame line. `band_rows.py`'s "data" count disagrees with its list.
  `enemy_ai.py:115`'s `us =` regex also matches `status =`.
- **Cut-off comments:** an E1F comment in `inject_all.cpp:949` breaks off
  mid-sentence. `draw_order.h:56` says one frame early where the code starts
  two.

## Checked and found sound

These are worth knowing because they were the obvious places to look:

- **DIV-0041's last two sites**, E4B `0x489D47` and E4F `0x493308`: they match
  the ledger and inject before `Widescreen_ArmFills`, so their fuzzes still
  compare 320 x 240.
- **DIV-0068's rim depth** and its fuzz levelling: not vacuous.
- **DIV-0070's fix-on check** Fatals unless Capcom's copy committed every
  space.
- **DIV-0071's code** matches the ledger: the box, ahead, rise, slot 6,
  default on, armed after the self-tests.
- **DIV-0069 and DIV-0045** match their ledger entries, launcher included.
- **The rebinding's 58 constants** resolve to the values they replaced.
- **The harness internals:** the `ArgAt` encoding, the region count, the
  disturbance bit splits, and EKH's clone and group order.

## Suggested order

1 (a play bug), then 2 (the reference sides' Rand count), then 3 (names
before round fourteen's callers build on them), then 4 (STATUS before the
next session reads it). 5 to 22 fit a tidy pass. 16 is the owner's decision
alone.

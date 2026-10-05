# Round fourteen so far: a code review of `phase-3/capture-round-fourteen`

**Status:** DRAFT (2026-10-05) - a read-only review of the round as it stands
at `1c308c0` (R0A and waves one to three merged, paused before wave four).
Nothing was changed: every item below is open, for the owner to schedule.
Line numbers are at `1c308c0`.

## What was reviewed, and how

The range is `5a94224..1c308c0` (`main`, PR #40, to the branch's tip): 212
files, about 60,400 lines added, 119 non-merge commits. It holds the round's
plan, the state hash (`statehash.cpp`, `tools/statehash.py`, the skip list),
R0A, waves one to three (23 groups, 1,033 functions, 8,648 -> 9,681), the
rebinding, DIV-0073, the DIV-0041 amendment, the platform-layers plan and
IDEAS, and round thirteen's review.

Seven reviewers read it in parallel by area: R0A with wave one; wave two in
two halves; wave three in two halves; the harness, hook layer and tooling;
and a scripted pass over `symbols.toml`, the clone rows, the ledger and the
docs. The coordinating session then re-read the findings marked *verified*
before writing them here.

**What a review without the game cannot see.** `BOF3.exe` was not on the
machine. So no reimplementation was checked against Capcom's bytes, and no
clone extent, call-site offset or table content was checked against the
image. What was checked is the source: C++ correctness, the hard rules,
code against its own comments, docs and `symbols.toml`, and whether a fuzz
can fail. The checks run by script, all clean:

- the round's 1,025 new functions and 254 new `[[data]]` entries in
  `symbols.toml`: no duplicate `pc`s or names, no function and data entry on
  one `pc`, nothing removed or renamed; every `impl` file exists and is in
  `CMakeLists.txt`;
- each group's `impl` count against its `BOF3_INJECT` count (all 23; 1,033 in
  all), and every name, return type and parameter list against its
  definition;
- every `// original 0x...` comment against its function's `pc` (907
  comments over 1,033 definitions, 0 mismatches);
- every clone row: the set equals the group's `impl` set, each base is its
  `pc`, no extent overlaps another or holds another function's start, and
  the 160 rows that also give "(0xN bytes)" agree;
- every `kTables` / `DataTable` count against its `[[data]]` count (255; the
  one difference, `TacticsMenu_Steps` 6 against 7, is documented);
- the wave and round totals: 8,648 -> 8,655 -> 8,989 -> 9,343 -> 9,681, and
  every `rest_XX.md` count equal to its inject count;
- `inject_all.cpp`: 23 `RestXX_Inject` calls, each once, one
  `FishingText_Arm` after them, and the order constraints its comments state;
- the skip list: 165 ranges, about 426 KiB, no overlaps, all inside `.data`;
- `ledger_check.py`: 73 entries, 0 errors; DIV-0073's eleven fields;
- the 27 new docs each with one `docs/README.md` row; 947 relative links, the
  only two broken ones pointing into the sibling repository;
- hard rule 1 over all added lines: no CJK text, no hex-dump runs, no byte
  strings; the long literal lists are fuzz inputs or code immediates;
- hard rule 4: no `TODO`, `FIXME`, `XXX` or "for now" in added code, and every
  `return 0;` is a conditional or documented `al` answer. **No stub was
  found;**
- rule 7: all 119 non-merge commits carry the owner's `Signed-off-by`.

A `g++ -fsyntax-only -Wall -Wextra -Wshadow` pass over R0A and wave one, with
a generated `symbols.gen.h`, was clean.

## Findings

### High

1. **A third of the group disturbance never runs in eight of the round's
   groups.** *Verified, and by a simulation of 2M hashes.*
   - **Where:** `switch (h % 12)` in `rest_1b_fuzz.cpp:359`,
     `rest_1c_fuzz.cpp:390`, `rest_1e_fuzz.cpp:354`, `rest_1f_fuzz.cpp:429`,
     `rest_2a_fuzz.cpp:297`; `% 18` in `rest_1g_fuzz.cpp:484`; `% 15` in
     `rest_2f_fuzz.cpp:435`; `% 6` in `rest_3e_fuzz.cpp:471`.
   - **Cause:** a group's `Disturb(h)` is reached only from the harness's
     case 14 (`scenario_harness.cpp:233`, likewise `boss_harness.cpp:222`,
     `area_harness.cpp:138`, `magic_harness.cpp:137`). Each harness has
     already returned when `h % 3 == 0` (`scenario_harness.cpp:172`). So
     `h % N` with N a multiple of 3 never lands on a multiple of 3.
   - **Failure:** the cells those cases move are never disturbed, so a
     mutant that misses a re-read of them after a call passes. Among them:
     `Sprite_Current + 8` (R1B, R1C, R1E, R1F case 0), `Field_InputFlags`
     (R1B case 9), R2F's step, timer, list end and row, six of R1G's 18
     (`kLeaderStage`, `kEff5Frame` among them), R3E's `+9` and its two
     sounded flags. `rest_3e.md:201` says the fuzz moves `+9` and the sound
     flags. The controls reported as refused were refused by the other
     cases; no control of these cells could have been.
   - **Already known once:** R3G hit this (its control 37 not refused) and
     fixed its own fuzz in `c85840d` by drawing the case from `h >> 8`
     (`rest_3g.md` section 6). R3A..R3D do the same. No sibling was told.
   - **Not affected:** R2C (`% 18`) also drives the same moves from
     `sh::Noise()` in its stand-ins, so its cases are reached. R0A, R1A,
     R1D, R2B, R2D, R2E, R2G, R2H and R3F use moduli prime to 3.
   - **Outside this round's range, the same shape:** the group `Disturb` of
     `effect_1d`, `1e`, `1f`, `2e`, `2f`, `3d`, `4a`, `5a`, `5b`, `5d`, `5e`,
     `5f` and `field_c3` (`% 6`, `% 9` or `% 12`, each the disturb hook,
     checked by script). Round thirteen's review called the disturbance
     check clean: it looked for dead labels, not for unreachable ones.
   - **Fix:** `switch ((h >> 8) % N)` with the value from higher bits, as R3G
     did; then re-run each group's controls, and plant one control on a cell
     each formerly dead case moves. R2A's `% 12` with cases 0..10 (case 11
     a silent `default`) wants `% 11` or a twelfth case at the same time.

### Medium

2. **The Rand counter is still bypassed on Capcom's side, and the coming
   validation leans on it.** *Verified (round thirteen's item 2, still open
   at `1c308c0`).*
   - **Where:** `src/hook/input_script.cpp:644` installs `CountingRand` with
     `bof3::Inject("Rand", ...)`, which obeys `BOF3X_ORIGINAL`; under `*` the
     original side's `randlog` reads 0 on every frame.
   - **Not affected:** wave one's `caughFish.txt` count
     (`takeover-queue-round14.md` section 9) compares *ours* against the
     owner's recording, not an original side. It stands.
   - **Affected:** the planned large validation (`final_live.sh`, Chinese
     against Chinese) wants to know which routes replay on two originals
     per route, and d3117c7 says the fishing route's original side "runs to
     `done`" with the keep list. On those sides nothing shows whether the
     same catch replayed. The state hash cannot stand in: `Rand`'s seed
     lives in the CRT's per-thread data (`input_script.cpp:640-642`), not in
     `.data`, and `state-hash.md` section 3 does not list it among what the
     hash cannot see.
   - **Fix:** round thirteen's (a raw `WriteJmp` outside `Inject`, or `-Rand`
     in every reference side's list), before the validation; and the seed
     added to `state-hash.md`'s "does not see at all".

3. **`statehash.py check` and `diff` never compare the runs' headers.**
   *Verified.*
   - **Where:** `tools/statehash.py:35` reads base, pages, page size and the
     skip count; only `cmd_info` (`:137`) prints them. `cmd_diff` (`:148`)
     and `cmd_check` (`:162`) do not look.
   - **Failure:** one reference recorded without `BOF3X_STATEHASH_SKIP`, or
     with another list, makes every page holding a skipped range "noise" at
     every tick. `check` then never compares NEW on those pages (the task
     records, the window and sound pages, `0x7DE000`), and exits 0. The only
     sign is "(N of them at every tick)" in the summary. `state-hash.md`
     section 6 says a new skip list wants a new pair; nothing enforces it.
   - **Fix:** refuse runs whose headers differ (better: write a hash of the
     skip list's contents into the header); optionally fail when a page is
     noise at every tick.

4. **A rebinding changed a value it says it kept.** *Verified.*
   - **Where:** `src/game/battle_sprites.cpp:85`, commit `c85840d` (R3G):
     `Raw<void (__cdecl*)()>(0x494500)` became the bare
     `Battle_PlaceBossActors`. The table's other bare names are this file's
     own functions; this one is R3G's, so it resolves to our function in the
     DLL, not to `0x494500`. The commit and `rest_3g.md` say "the values
     unchanged"; lines 70-76 of the same table use
     `Raw<...>(bof3::addr::Name)` for other groups' functions.
   - **Failure:** `Battle_InitBossEncounter` calls ours directly, so
     `BOF3X_ORIGINAL=Battle_PlaceBossActors` no longer restores Capcom's
     code for this caller, and the call trace does not see the call. Rule 3.
     The fuzz is unaffected (its stand-ins replace the entry).
   - **Fix:** `Raw<void (__cdecl*)()>(bof3::addr::Battle_PlaceBossActors)`.

5. **HANDOFF's "Where things stand" is still stale.** *Verified (round
   thirteen's item 4, the half left).*
   - `docs/HANDOFF.md:17-18` says 8,648 on `phase-3/capture-round-thirteen`,
     6,891 on `main`, and that `main` is round eleven. `main` is round
     thirteen at 8,648 (PR #40); the branch is at 9,681. Its own item at
     line 55 and STATUS say this is owed.
   - Item 00000000 (line 66) still says "DIV-0073 owing the owner's word"; the owner
     kept it (`8ee1405`, the entry's last field, STATUS).
   - **Fix:** rewrite the paragraph from STATUS; one word in the item.

### Low

6. **R3A's dispatchers hand the caller's word on, and the fuzz cannot see
   it.** *Verified.* `rest_3a.cpp:69-74` passes `through` to the entry;
   `kOutsideTables` / `kEngineTables` (`rest_3a_fuzz.cpp:222-223`) leave
   `nargs` at 0, so the recorders log no word. A mutant that drops it in any
   of the five dispatchers passes. R3D hit the same blind spot (its control
   77) and fixed it with `{0x64ECCC, 7, 4, 1}`. **Fix:** `nargs` 1 on the
   five tables, and one control.
7. **Wave one's dispatchers treat an index past their table two ways.**
   *Verified.* R1A, R1B and R1C read the cell and abort only when it is not
   code (`rest_1a.cpp:58-70`, `rest_1b.cpp:53`, `rest_1c.cpp:66`); R1D, R1E,
   R1F and R1G abort at the count (`rest_1d.cpp:50`, `rest_1e.cpp:53`,
   `rest_1f.cpp:72`, R1G's `Run`). The tables are contiguous, so a state one
   past its table runs the next table's code in sets 0..9 (as Capcom does)
   and kills the game in sets 9..18 and the fishing spot. `rest_1b.md`
   section 6 and `rest_1d.md` L3 each describe their own rule. **Fix:** one
   rule for all seven, or the reason they differ in both docs.
8. **`AreaMap_Slope` reads more than the direction's low byte; wave one's
   fuzz says it does not.** `area_slope.cpp:119-123` reads `args[4 + n/2]`
   for the low byte n, which for n of 10 or more is the direction dword's
   own upper bytes. Ours zero-extends; Capcom passes the caller's register.
   The comments at `rest_1c_fuzz.cpp:247`, `rest_1d_fuzz.cpp:240`,
   `rest_1f_fuzz.cpp:261` say otherwise, and R1A's and R1B's `kU8` masks
   hide it from the fuzz. Unreachable while directions stay 0..7. R1E's doc
   (section 4) states it, as "8 or more" (10 is right). **Fix:** correct the
   comments; ledger the fork or say why it cannot be reached.
9. **`Fish_Spawn` hangs before its loud abort.** *Verified.*
   `rest_1g.cpp:685-691` loops `size -= m >> 1` while `size > m`; the Fatal
   for a top size of 0 is after it (`:704`). With `m` 0 or 1 and a non-zero
   roll the loop never ends. `rest_1g.md` section 7 says ours aborts. The
   shipped kinds are 20..240. **Fix:** the `m <= 1` check before the loop;
   the comment at `:659` ("halved by M") says what the code does not.
10. **DIV-0073 is not cited where it lives.** *Verified.* `rest_2b.cpp:19-23`
    says "Every one is a faithful replacement"; `Shisu_DrawModel` (`:859`)
    cites no entry; `rest_2b.md` sections 2 and 7 still say L1 "wants a
    ledger entry". The code matches the entry (a 16-short light matrix of
    which `Light_ObjectDirection` writes three; `Gte_NormalColor` overwrites
    its answer; the zeros persist in `.data` as the entry says). **Fix:**
    cite DIV-0073 at the function and in the doc.
11. **DIV-0041's amendment leaves out kind 0xAC, and R3G's notes on
    `0x492400` are stale.** *Verified.* Kind 0xAC's fades
    (`EffectKindAC_FadeIn` / `_FadeOut`) draw through
    `EffectKindAA_DrawFill`, so they widen too; the entry names only 0xAA.
    `rest_3g_callees.h:5-7,19`, `rest_3g.cpp:131` and `rest_3g_fuzz.cpp:228`
    still call `0x492400` not ours, semi-transparent and 320 x 240; it is
    ours, opaque and widened. The widened code itself matches the entry and
    is gated as the other fills are. **Fix:** one clause in DIV-0041; the
    comments.
12. **`TacticsFormation_Pick`'s text has the column move the wrong way
    round.** `rest_2f.cpp:465` and its `symbols.toml` evidence say
    left / right move the column "with a pick"; the code (`:575`) moves it
    only while `0x6BDFC6` is not 0, which includes "none picked" (0x7F). The
    fuzz seeds 0, 1, 2 and 0x7F and agreed, so the text is what is wrong.
    The reviewer's further claim, that only column 0 can be picked, was not
    confirmed here. Control 37's label reads the same way. **Fix:** the two
    texts.
13. **R2F's grid loops may not spin as the doc says.** `rest_2f.cpp:568-571`
    and `:585-589` read through plain pointers and write only locals, so
    under C++'s forward-progress rule the compiler may assume they end;
    `rest_2f.md:460-465` says ours spins as the original would. Unreachable
    in play. **Fix:** a volatile read, or an abort after three wraps (and the
    doc).
14. **R2E aborts on category 4's null count list earlier than the original
    faults.** `rest_2e.cpp:528`, `:731`, `:755` take the list at entry;
    Capcom faults only on the write. `FieldItemSort_Compact` and
    `FieldItems_ArrangeMove` already check at the write. Unreachable from
    R2D's states. **Fix:** move the check, or say so in `rest_2e.md:364`.
15. **The state hash file is lost on a crash.** `statehash.cpp:182` flushes
    every 64 ticks; there is no flush on `Fatal` (`TerminateProcess`) or in
    the crash handler. The frames dropped are the ones wanted for a crash
    such as the fishing route's `0x5A9E45`. **Fix:** flush from both, or
    every tick while `BOF3X_STATEHASH_DUMP` is set.
16. **Wave three's groups disagree with each other in four places.**
    *Verified.*
    - The party restat loop aborts past a count of 3 in R3C
      (`rest_3c.cpp:120,128`) and writes on into the window records in R3D
      (`rest_3d.cpp:461`, `:486-501`). One policy, or both docs say why.
    - R3C's slots 60.. are said to answer the callee's eax, but R3B's
      `EffectSlot04_SkillPower`, `_07_Heal` and `_11_HealFull` are `void`
      (R3D's matching tail is `void`). Harmless today; the signatures
      disagree.
    - R3E describes `0x480300` as `(point, size, dy, word)`
      (`rest_3e_callees.h:11-14`); R3F's `EffectKind5F_DrawLineDisc` is
      `(point, unused, wobble, dy)` with a constant size.
    - `takeover-queue-round14.md` debt 13 misses R3C's four raw calls into
      R3B (`rest_3c_callees.h:83-86`, 12 sites); `rest_3b_fuzz.cpp:285` and
      `rest_3c_fuzz.cpp:239,244` still say "raw until it merges". R3E's
      `kR3FRing` and R3G's `kFadeDraw` are raw too, though `rest_3f.md`
      section 10 asks for them to be bound.
17. **Wave two's groups disagree with each other in two places.** *Verified
    as disagreements.*
    - R2C reads `0x9398E0` as one record past `0x110` bytes
      (`rest_2c_callees.h`, `rest_2c.md` 1.1); R2B reads two `0x80` model
      records and the screen's cells at `0x9399E0`. R2C's "lift" is model
      B's scale, its "fade step" the fourth item count given, its "scale
      index" R2B's level.
    - `0x5B9450` is `strncpy` in R2C and `memcpy` in `field_e2`, `battle_e1`
      and `boss_harness`. Which is right wants the bytes; then a
      `symbols.toml` entry.
18. **`AreaMap_FrameAreaBD` survives outside DIV-0041.** `0x510780` is now
    `AreaMapBD_BuildView` (R3G), but DIV-0062 (`DIVERGENCE.md:3318`),
    `widescreen.md:68,88`, `psp-widescreen.md:246` and `effect_6c.md:230`
    keep the old name. **Fix:** the rename, and a line in DIV-0062.
19. **IDEAS is behind the owner's answers.** I31's "Gated on"
    (`IDEAS.md:1191`) and feasibility line still wait on whether our own
    executable is a goal; `platform-layers-plan.md` section 5 records that it
    is the first goal. I23's body (about line 961) still says "open; method
    written"; only its table row has the 2026-10-04 answer.
    `platform-layers-plan.md` section 2.4's heading says the decoder is
    replaced with a ledger entry, while section 5 says its shape waits on
    I23.

### Nits

- **`Stat_AddClampedTo`:** "ours puts 0 there" (`rest_2f.cpp:1053`,
  `rest_2f.md:487`) is false; the upper half is `cap`'s or 0xFFFF. Callers
  read the low word.
- **`ReadTitleCall`** (`rest_2f.cpp:1517`) refuses only a site that is not
  E8, not "anything else".
- **`battle_e5_callees.h:119`** uses `bof3::addr::Stat_AddClampedTo` without
  including `symbols.gen.h`; it compiles by include order.
- **Stale counts and names:** `rest_2e.md:11` "Thirteen" tables (12 since
  `8e652f8`); `symbols.toml:105279` "FieldItemsStates" (R2D's is
  `FieldMenuItems_States`); `rest_2c.md:186` "Not named, for R2D: 0x66456C"
  (`MasterQuit_Steps`); `shop_states_callees.h:67` "0x5800D0, not ours" (R2C's
  `FieldSave_Written`); R2D's `kMemberLabel` comment against R2C's
  `MasterPanel_DrawStats`.
- **Raw cross-group constants** whose targets are now named: R2B's
  `kModelADraw`; R2C's `kFigureDraw`, `kPickAsk`, `kGlyph`; R2D's
  `kMemberPanel`, `kMemberLabel`, `kPromptBox`, `kItemsWindows`,
  `kItemsReset`; R2E's `kAbilityUse`; R1C's 34 dispatcher table addresses
  (`rest_1c.cpp:434-489`). Presumably the round-end rebinding's.
- **Wave one's names:** set 16's form-action dispatchers are
  `PartyFormAction16_Form0` / `_Form1` in R1E and
  `PartyAction16_FormAction2` / `_FormAction` in R1F; sets 16..18's tables
  break the `PartyFormActionN_FormKStates` pattern; `PartyAction_TurnToSide`
  and `PartyFormAction_TurnToSide` suggest the opposite roles.
- **Wave one's comments:** `field_hidden.cpp:204-205` holds one line twice,
  each half rebound (a keep-both merge); `rest_1a_callees.h:57` and
  `rest_1a.cpp:510` call `0x9039A3` "Field_ScriptFlags + 3" (it is +1, the
  high byte); `rest_1d.md` L2's "not traced" was traced by R1B and R1D's own
  code; `rest_1f_fuzz.cpp:425` says the disturbance moves Field_Request (no
  case does).
- **Wave three's comments:** `rest_3b.cpp:323` "entry 7" (it is [6]);
  `rest_3d.cpp:123` gives the PSX twin R3D itself corrected;
  `rest_3b_callees.h:8-9` gives `0x44FB30`'s offsets one word off R3D's;
  `rest_3d.cpp:381-382` "callers pass 1 and 2" (3 too); `rest_3a.cpp:197`
  and `rest_3a.md:67` "records 1 and 2" (0 and 1); `rest_3e.cpp:132`'s loop
  "after 256" wraps instead.
- **The state hash tool:** `statehash.py bytes --gap` joins a gap one short
  of its help text (`:194` against `:227`); `LoadSkips`
  (`statehash.cpp:100-106`) skips an unparsable line as a comment and lets a
  huge length wrap; `cmd_info` raises `IndexError` on a file with no whole
  record; `state-hash.md:94`'s "3.5 MiB (3,472 KiB)" is 3.39 MiB.
- **The count:** `symbols.toml` has 9,682 functions with an `impl` at the
  tip and 8,649 at the base; every doc and the inject log say 9,681 and
  8,648. All 9,682 names are injected; the one's cause was not found.
  `takeover-queue-round14.md:13` uses the toml's figure, and its sentence
  (8,649 + 2,084) does not add to the 10,246 it sits beside.

## Checked and found sound

- **R3D's "equivalent mutant"** (control 53): `Battle_StatusTint` tests one
  bit of its argument, so the offset's upper half cannot matter; its near
  variant (control 75) was refused. R3C's C101, R3B's 89 and R2B's C72 / C75
  read correctly too.
- **R2E's latent defects** (the `FieldEquip_BestByOrder` armour tie, the
  `FieldEquip_RemoveSlot` stale slot) are what the code does, and refused
  controls back them as Capcom's.
- **DIV-0041's amendment:** `EffectKindAA_DrawFill` draws from
  `Widescreen_FillX()` to `320 + Widescreen_Fill()`, 0.0 / 320.0 exactly when
  narrow; `Rest3F_Inject` is before `Widescreen_ArmFills`, so its fuzz
  compares 320; `EffectKindA8_DrawBar` stays 0..320 as the owner decided.
- **`AreaMapBD_BuildView`** reads back the operands `widescreen.cpp` and
  `draw_pool.cpp` patch, injected after the first and before the second.
- **The state hash:** 868 pages of `0x5DA000..0x93E000`, the header the
  parser's, page splits of skip ranges right at the edges, the tick taken
  before `Input_Latch` on every path and never undone by `*`; `statehash.py`'s
  walk, its noise logic and its handling of a truncated last record.
- **The rebinding** in other files keeps every value but item 4.
- **The fishing keep list** (`d3117c7`): its eight names exist once each
  and are injected in the files that call `FishingText_On()`. The list
  itself is only in untracked scripts, so the doc's text is its record.

## Suggested order

1 first: it decides how much the round's controls proved, and the fix is a
line per fuzz and a re-run of the controls (the 13 older fuzzes with it).
Then 2 and 3, before the large validation is run. Then 4 (rule 3) and 5.
6 to 19 fit a tidy pass, best before wave four builds on wave three's names.

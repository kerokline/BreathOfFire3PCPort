# Handoff — next session

**Status:** IN PROGRESS (2026-10-13: `main` is `a7c7a9e5`, PRs #49..#56 merged; tonight's `cleanup/abc-crash-1012` is unpushed and ready for its PR, D260..D263 fixed - item 1 below)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

**10,088 functions are ours in the exe host, 10,087 in a hosted run** (the `inject:` line at `a7c7a9e5`); the game's
own code is all ours since the platform round's step 3 (PR #43). `main` is `a7c7a9e5`, with PRs #49..#56 merged:
#49 the testing catch-up and the four no-owner items, #50 the runtime's allocator, #51 state 2's prework
([`state2-proof.md`](state2-proof.md)), #52 the Jukebox ([`music-player.md`](music-player.md) - the owner uses it a
lot, "it's great", and named every song from the box set), #53 state 3, our own `bof3x.exe`
([`state3-build.md`](state3-build.md)), #54 the event battle's music restart (heard), #55 the disc-only round
([`disc-only-round.md`](disc-only-round.md)) and #56 the PSP-only round ([`psp-only-round.md`](psp-only-round.md)).
Neither install from a disc alone has been played. **Tonight's branch, `cleanup/abc-crash-1012`** (unpushed,
`git log --oneline main..HEAD`), holds two docs-audit passes and the root README's rewrite; the areas 16 / 45
re-run - no crash on any of seven installs, the PSP builds' trees (D258) and PSP-EU's cell hooks (D259) fixed by
`tools/psp_exe.py` rows, DIV-0101 amended ([`visual-findings-1012.md`](visual-findings-1012.md) 10); and the A/B/C
sweep ([`abc-sweep-1012.md`](abc-sweep-1012.md): ten routes on the `eu` triple, no regression between builds, the
PC path's state hash clean on combat, shop and the world-map route). The sweep found four defects, **D260..D263**, all
gaps of the disc-built image and all **fixed** by `twin` rows ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md)):
D260 was a crash a player could reach (PSP-EU, the faerie village); `fairyVillage` now runs to its end on all three
`eu` installs. Two more seen on that route, D264 (the village's stats zero) and D265, are open. The regression
check is the state hash ([`state-hash.md`](state-hash.md); the references `cn_<route>_orig2` / `_orig2b` in
`analysis/statehash`) and, across installs, the A/B/C harness ([`abc-harness.md`](abc-harness.md)).

## Pick up here

1. **Push `cleanup/abc-crash-1012` and open its PR, at the owner's word.** Everything is merged on it: the
   docs audit and README, the areas 16 / 45 re-run (D258, D259), the sweep, and the D260..D263 fixes
   ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md); DIV-0101 amended twice). Checked at the tip
   (2026-10-13): `ledger_check` 114 entries 0 errors, `importer.py check`, `exe_tables.py check`, `psp_exe.py
   check`, `suites.py check` clean; every one of the 11 non-merge commits signed off (CLAUDE.md rule 7); no `src/`
   change, so `main`'s build and self-tests stand. The exe host on the PSX-EU-only and PSP-EU-only installs exit 0
   with the new images. Still open from tonight: **D264** - the village's faerie stats zero on both disc images, so
   the village simulates differently (a new rule shape from `COMMU00`'s nine-byte records, not a `twin`) - and
   **D265**, PSP-EU's blank "Culture" label; `Menu_ButtonSets+0x2A` on the PSX builds, now classed `unread`, not
   looked at.

2. **The follow-up round: disc-only and PSP polish** (not staged; the owner's word of 2026-10-12 night). Closed
   tonight, not to redo: masterAndManillo's world-map entry, "You gain 3 EXP!", the PSP message boxes, the shop's
   walking-ahead and the PSP's Nue divergence ([`abc-sweep-1012.md`](abc-sweep-1012.md) 4,
   [`visual-findings-1012.md`](visual-findings-1012.md) 10). Still open:
   - the load screen's `"%X"` (D254, the engine's own string recommended) and the PC `en-150` empty-record word
     `0x669CCC` (the staged localisation PR, item 5, covers it);
   - the inn's black screen on a PSX-only install (D255): the jingles on a disc-only install
     ([`sound-import.md`](sound-import.md) 7), or the failed stream ending the wait (a DIV);
   - the PSP title's framing (D256), the owner's look at a PSP-EU title capture;
   - the twelve disc-unplaced runs one overlay alone carries (`visual-findings-1012.md` 9's list): drafted as
     `TWINS` rows, not committed - regenerate and check;
   - what is left on PSP-EU's image (`visual-findings-1012.md` 10.4's list) and the class 10.2 suggests: `psxmap`
     reads PSP files at PSX-JP's addresses and misses runs PSP-EU's `GAME.EMI` holds elsewhere;
   - the fishing ticker's text, PC against both discs (10.4; not looked into);
   - the `us` set, not swept (`abc-sweep-1012.md` 6);
   - a harness pass over WA's wide-mode scenes ([`psp-widescreen-a.md`](psp-widescreen-a.md) 6): AB with
     `BOF3X_WIDE=1`, the middle 320 columns against the narrow capture;
   - the harness refusing a recipe with no `# save` header, or taking `--save` ([`abc-harness.md`](abc-harness.md));
   - the disc-only round's own leftovers ([`disc-only-round.md`](disc-only-round.md) 5: the review's notes, the four
     stale Chinese pairs, a PSP-only build's 14 arenas);
   - the owner's open calls, in [`owner-review.md`](owner-review.md)'s round entry: D56's repair (proposed, not
     built: every battle reference re-recorded), the jingles on PSX-only installs, the field menu's time and money
     boxes in the wide picture, the PSP-EU Config screen (D246), the PAL banks (D257).

3. **The disc-data preference inventory** (the owner, 2026-10-12 night: "let's put this as a next up action - I
   agree a inventory pass is what's needed"). The owner prefers the PlayStation's data where both the PC install and
   a disc carry a chunk (prompted by the PC title's (R), which the PSX title lacks; DIV-0114). Today a PC + disc
   cache takes the PC's chunks wherever the PC has them; only the music follows the disc (the owner's 2026-10-08
   call). The pass is a reading task, no output change: every `base/` chunk whose PC form differs from the disc's,
   classed as a port repair (keep), port-specific art the PC needs (the dial page's keyboard legend, the re-laid
   icons - keep) or a port cosmetic change (the (R) - take the disc's), each with a proposed rule. The owner decides
   from the list; then a ledger entry and the importer's base policy, as the music decision was.

4. **The owner's eye, ear and play** - [`owner-review.md`](owner-review.md) is the one list: the PSP round's entry
   (the PSP stand-ins, logo and launcher page; the PSP's music, effects and jingles; the Beyd-Zig duel under
   `duel-loss`, Myria's item, a level-up of roster index 6), the random-encounter resume on the PlayStation, and the
   older listens and play checks. Nobody has played a disc-only or PSP-only install. The older owner checks still
   only in this file are under "Older open items" below; they belong in `owner-review.md`.

5. **The staged localisation PR**: the empty save slot blank as the US disc draws it (the `0x669CCC` word above)
   and IDEAS I34, the name entry back ([`name-entry-restoration.md`](name-entry-restoration.md); its section 8 is
   the owner's to answer first). Not launched.

6. **Work that needs no owner** (each doc's own list is the authority):
   [`exe-import-engine.md`](exe-import-engine.md) 5 (the 183 language addresses, the effect state tables' twins, a
   second map of the PSP ELF's own copies); the disc-only language build's gaps
   ([`loc-build-disc-only.md`](loc-build-disc-only.md): the village sheet, French `AREA004`, the Chinese chunks
   nothing replaces); [`cache-read.md`](cache-read.md) 10 (the MP3s in the cache, an ini key for
   `BOF3X_CACHE_DATA`); the music's long-fade row as a design ([`music-open-ends.md`](music-open-ends.md) 4) and
   `SsUtKeyOnV` read before the sound effects move; the exe's remaining Chinese - `tools/text_scan.py --all` finds
   424 strings, 241 at addresses `src/` names, to cross-check against DIV-0064's label tables for the definite list.

7. **Small, left over:** the controls still thin ([`round-14-cleanup.md`](round-14-cleanup.md) 4's last
   paragraph); the 61 run-time raw calls ([`round-13-cleanup.md`](round-13-cleanup.md) 1.3, a decision); E2E's 19
   `EffectKind48_State7..12_*` names, which E2D's three-state table makes unreachable
   ([`round-13-review.md`](round-13-review.md) 3); `pe_hidden.py` and `pe_funcs.py` stop silently at an
   undecodable byte (fixing them regenerates the entry lists); [`mode-rest.md`](mode-rest.md) 0's "pc_funcs.json" is
   `pc_hidden.json`'s size, and `Mp3_Create`'s evidence puts the decoder at `0x5AB000` where it is `0x5ADF00`; the
   PSP's eleven map-band changes unread (region-diff 6); Capcom's type-1 routine not located; a one-line log of
   each file `LoadDatFile` opens (would settle area names); DIV-0041's wider terrain margin as the creator of the
   bridge's edge cells, unmeasured; `Sprite_FlashClut`'s row mask; seven older hosts still covering an owned start
   in `entries_logic.txt`; `importer.py verify` on a US-only cache reports `opt/area4-walls` `AREA004.DAT` slot 6
   "composed with the layer, not the recipe's".

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
- **Widescreen's debts** ([`widescreen.md`](widescreen.md) 4, 5): the oracle and frame hash with `BOF3X_WIDE=1`,
  the culls of 3b's table, the sky `0x571C85`, the message-box table, the corner pop at margin 100.
- **[`new-code-audit.md`](new-code-audit.md)**: eight bugs confirmed by reading and not seen in game, ten unchecked
  reports, six questions; A1 and A2 the owner's. The lifter's first run on `BOF3.exe`
  ([`lifter-feasibility.md`](lifter-feasibility.md) 7).
- **`analysis/pairs_propagated.json`**: `0x445A30` / `0x44B9F0` swapped, `0x44F030` paired to the wrong twin
  ([`battle_damage.md`](battle_damage.md)); then [`attract-remaining.md`](attract-remaining.md) 5.1's divergence
  map (never the `call-disputed` tier) and the name import as `hypothesis`.
- **The first receipt** ([`STATUS.md`](STATUS.md) open decisions): the evidence exists (`attract_diff.py`,
  `mem_dump.py --compare`, the state hash, the route A/Bs).
- **IDEAS:** I13 save states (for what no route replays: bosses, event battles); I15's live look toggle key and
  I19 curvature; I18, F12's `Save_QuickWrite` disabled or made a true quicksave before shipping.
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

_Verified 2026-09-24; the disc-only, A/B/C and state-hash lines 2026-10-12..13._

- **Build:** `cmake --preset i686 && cmake --build build` - llvm-mingw's `i686-w64-mingw32-clang++` (the `retcomm`
  toolchain under `~/.local/share`), **not** MSYS2's. The first configure fetches SDL3 (network).
- **Run:** `build/bof3x-launcher.exe --game bof3 --no-config` from a script (`--no-config` skips the settings dialog,
  [`launcher-settings.md`](launcher-settings.md) §4); log in `build/bof3x.log`. `BOF3X_ORIGINAL=NAME` / `=*` /
  `*,-NAME` for original behaviour. Nothing mode-sets (DIV-0032); the game runs unfocused (DIV-0033), so
  `attract_run.py --no-front` leaves the desktop alone for all-ours runs; `--launcher DIR/bof3x-launcher.exe` runs a
  copy with its own `bof3x.ini` (or none: self-tests and references want an ini-less copy, Traps). End a run with
  `taskkill //F //IM BOF3.exe` only for a game you started (the runners do exactly that now,
  [`world-map.md`](world-map.md) §6); batches run detached.
- **Self-tests, headless:** `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' build/bof3x-launcher.exe --game <dir> --no-config`:
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
  `abc_compare.py --side A=<run> --side B=<run>` compares two builds' runs of one install. The roots in use:
  `C:\Users\kerok\ab` and `C:\Users\kerok\abx` (game-derived, never in the tree).
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

- **`cleanup/abc-crash-1012`** (the main checkout's branch, unpushed): item 1.
- **The D260..D263 fix agent** works in a worktree under `.claude/worktrees/` (the locked one, branch
  `worktree-agent-ae288769dd06909be`, at the time of writing). The two other worktrees there
  (`abc-sweep-1012`, `worktree-agent-ae1f9da045ac89510`) and the branch `worktree-agent-a8cf04c79e48fa916` are
  tonight's finished agents', every commit already on this branch; they can go once the fix merges.
- **Cleaned tonight:** 48 worktrees and 107 merged local branches removed (`psp/ledger2` and the round's group
  branches among them). Left: `music/event-battle-restart`'s scratch worktree (the session-`3186ef2f` scratchpad's
  `evb/`), which holds an untracked `cfg.out` - look before removing; and `claude/ecstatic-mccarthy-t8bg37`, an old
  README-art branch whose content `main` supersedes - deleting it was refused by the permission check, so it is
  the owner's to delete.
- **Scratch outside the tree:** the A/B/C roots `C:\Users\kerok\ab` (`c_pspeu` rebuilt tonight with `edc41de3`'s
  tools, no longer half-built) and `C:\Users\kerok\abx` (the areas 16 / 45 re-run's); the owner's scratch
  disc-only installs `C:\b3g` (US) and `C:\b3p` (PSP-EU), built before `wmrec` - rebuild with the current tools or
  remove (`visual-findings-1012.md` 10).
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

- **A branch from before 2026-10-04 looks unmerged by an ancestry check** (2026-10-13): the DCO rewrite gave 185
  commits new hashes, so `git branch --merged` and `merge-base --is-ancestor` call an old branch unmerged though its
  content is in `main`. Use `git cherry main <branch>` (a `-` line is in `main` by patch) before deleting or merging
  one.
- **`abc_run.py prepare` fails with WinError 32 while the root's `L\` is in use** (2026-10-13): it recopies the
  launcher copy, `bof3x.exe` included, so a game running from `L\` blocks it. Let the run finish (or stop it by its
  exact PID) first.
- **A recipe without a `# save` header loads each install's own slot 0** (2026-10-13, `fairyVillage`): the harness's
  three sides can start from different saves. Add the header in a scratch copy ([`abc-harness.md`](abc-harness.md);
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
  log); the two the attract reached are fixed (the button map, the story-flag seed).
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

- **The owner:** [`owner-review.md`](owner-review.md) - the eye, ear and play checks and the open calls (item 4,
  and item 2's calls). Settled since this section was last written: the battle resume was heard (DIV-0092's
  amendment, 30 frames), DIV-0076's load screen was seen, and `Cfg_Load`'s overrun was built as DIV-0078 (seen
  only with a hand-edited `BOF3.CFG`). Still the owner's: the layering in play, DIV-0077's TILE_1 and DIV-0079's
  lines in play; the camp cells and the POLY_FT3 colour stay parked.
- **The fix agent** on D260..D263 (item 1).
- **TheRealBiggs** - not yet contacted ([`STATUS.md`](STATUS.md) obligations).

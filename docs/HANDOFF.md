# Handoff — next session

**Status:** IN PROGRESS (2026-09-24)

[`STATUS.md`](STATUS.md) says where the project stands. This file is what to
pick up, how, and the traps already paid for. It **points at evidence rather
than restating it**.

**Maintenance rule: rewrite, do not append.** At the end of a session, replace
the sections below so they are true *now*. No dated banners stacked on top of
old paragraphs, no "the claim above is withdrawn" — the sibling's handoff
accreted that way and became hard to read. History belongs in `git log` and in
the investigation docs; anything durable moves to `STATUS.md`.

## Where things stand in one paragraph

**1,025 functions are ours** (`inject: 1025 ours, 0 left original`,
`build/bof3x.log` at `e428d77`); everything through PR 16 is merged
(`0c7260e`). Stage 1's attract queue closed at 0 in scope; the input-reached
queue runs on routes the owner records, and round seven took the battle
engine off the combat route, the wave-2 batch passing on 1,020 ours
([`takeover-queue-round7.md`](takeover-queue-round7.md) "Result"). Since
then: the cheats, D5's complete fix (DIV-0047), F1's 2x, the game writing its
own captures, and the controls (DIV-0050, DIV-0051; [`controls.md`](controls.md)
DONE, step 4 deferred to [`IDEAS.md`](IDEAS.md) I20). The rest is
[`STATUS.md`](STATUS.md)'s wave table; do not copy it here.

**The frame hash reference** is the all-original `analysis/calltrace/wm1b_orig`
(twin `wm1b_origb`; `analysis/validate_wm1b.sh`, reference sides at
`--original "*,-Game_Clock"`, `renderer=1`, windowed, foreground held). The
latest all-ours match is `wave2_ours` (1,020 ours: identical on all 10,313
logic frames but frame 0, the set-up, which has differed since `rb1` made
`Display_Setup` ours - compare from frame 1 or say so); DIV-0047's
`pace_ours` is identical to `wave2_ours` on all 10,319 frames. The functions
taken since wave 2 have their self-tests and their ledger entries' checks,
no hash run on record. `ab15`..`ab27`, `wm1` and `wm1_orig` are history.

## Pick up here

1. **Housekeeping.** `git worktree list` shows the twelve round-seven
   `.claude/worktrees/agent-*` worktrees (their `worktree-agent-*` branches
   all merged) and `vibrant-wilbur-f9676a` (detached at `5b8eeb2`, merged,
   the PSP session's); merged local branches `phase-3/more-ui-work`,
   `claude/vibrant-wilbur-f9676a` and `ui/control-changes` (PR 16; also on
   `origin`). Local `main` is at `61d20a0`, behind `origin/main`
   (`0c7260e`) - fast-forward it. Remove the worktrees and branches with
   the owner's nod; check each worktree for uncommitted files first.
   Also small: `Fmv_WndProc` `0x59E570` is detoured but has no `impl` line
   in `symbols.toml` (1,024 `impl` against 1,025 ours - STATUS).
2. **`entries_logic.txt`, then re-record the frame hash reference once.**
   About twenty wrong sizes and a few missing entries, listed per group in
   round seven's docs (BA, BB §7, BC, BE, BG §6, BH, BI) and
   [`world-map-hud.md`](world-map-hud.md) (`0x404160` at `0xC1`,
   `0x404620` at `0x58`); add every function owned since wave 2
   (`Fmv_WndProc`, the pad's `DInput_Init` / `Pad_Read` /
   `DInput_Shutdown`, and DIV-0051's `Config_DrawControllerRow`) - an owned function missing from the list breaks
   the hash (Traps). Fold in the other two things that move the hash's
   content, so it is re-recorded once, not three times: the CRT `sscanf`
   calls at frame 5524 (Traps), and **`pe_hidden.py` / `pe_funcs.py`
   seeding** - the functions after an inline jump table (ten or so at
   `0x593950`..`0x594240`, none in `entries.txt`,
   [`attract-remaining.md`](attract-remaining.md) §3), sizes that run on
   through pointer-reached neighbours (`0x56FF00` was `0xBA6`, really
   `0x118`), and seeding from the PSX pairs (§5 there). Then three 6-minute
   runs `validate_wm1b.sh`'s way, an original-vs-original pair beside ours.
   Do it before 2026-09-27 or after a Restart (Traps: 12.4 days).
3. **The owner's eye** on round seven and the world-map wave: the compass
   needle turning with the map (DIV-0044), the sky's wide bands (DIV-0041
   amended), and a fight under full ownership - the round-seven doc's
   header waits on that to go STABLE. The owner played the combat route
   with cheats on at 08:12 on 2026-09-24, after the merge, on 1,020 ours:
   **ask whether that counts** before asking for another fight.
4. **The next recorded route, round eight's queue.** The owner records with
   `BOF3X_RECORD` (F12 a shot); the route is A/B'd, traced once all
   original, less every earlier route's reach (`attract_catalog.py --minus`,
   the command in [`takeover-queue-round6.md`](takeover-queue-round6.md) and
   [`world-map.md`](world-map.md) §4). What the combat route does not reach,
   per round seven's docs: boss encounters, the event-battle paths, a full
   task table, key items in the battle list. The parallel method is under
   "How to run things".
5. **What the rounds left unowned**, each named in its group doc:
   - Round seven: `0x591F30`, `0x592570`, `0x5925A0` (the encounter
     placement's driver), `0x589110`, `0x589160`, `0x5891C0`, `0x4B9000`
     (D52), `0x430510` (the set-up chain's real caller), `0x43B130` (the
     battle effect host, which reads the turn counter's strings), the 27
     battle state handlers at `0x64DFE0`, the window-kind handlers
     `0x597A80`..`0x597C10`, the 130 effect handlers after
     `Effect_ApplyResult`, the boss handlers at `0x656954`.
   - Round six: nine pointer-reached window-task handlers
     `0x59B7B0`..`0x59BEA0` (Y); the leader's sub-state 2 `0x52E110` and
     where a step lands `0x52E580` (V1); the member's states 2..8 and
     `0x527640` (Z); the save block builder `0x5806F0` and `Save_QuickWrite`
     `0x5809C0` (X; see I18 below); `0x591810`, `0x591B60`
     (`Inventory_Remove`), `0x591CC0` (W, missing from every entry list);
     `MsgBox_SystemChoice` (D22).
   - Older: `0x5A7C70` (the `s16`-out `ApplyMatrix`, unreached); the
     software path's lock wrapper `0x5A3CC0` and `Display_Teardown`
     `0x5A6380` with the enumeration callbacks (retired by DIV-0031, still
     Capcom's bytes; [`display-setup.md`](display-setup.md)); WinMain's
     run-once callees `Game_Init` `0x4FD110`, `Gfx_LinkOTags` `0x4FD290`,
     `Disc_Probe` `0x5A72C0`, the sound pause pair (I8 / owner's scope).
   - **Fuzz only, no live check reaches them** - each round's doc lists
     its own (e.g. round six: the software-surface and direct-colour texture
     paths, step codes 2..7 and an encounter firing, the floor-damage kinds,
     `Stat_AddResist`, `Equip_PreviewSet`, `TitleFlow_NewGame`). Expect
     `pe_hidden.py`'s blind spot per group: round six found ~40 functions
     its queue missed.
6. **Localisation: the exe's remaining Chinese**
   ([`dialogue-localisation.md`](dialogue-localisation.md) §6 the open list,
   §8 the method and the chunk kinds; `BOF3X_TEXTLOG=1` gives a string's
   address). Located by round seven: the place plates want the US page
   section and `0x800D3800` per world map ([`world-map-hud.md`](world-map-hud.md));
   the target banner `攻 击` is `BattleWin_DrawCommandLabel` from
   `0x669D60`, patch point the `Text_DrawAt` call at `0x443AF5`
   ([`battle_windows.md`](battle_windows.md)); the enemy name is the enemy
   record's first 12 bytes at `0x8C55C8 + id * 0x8C`, copied to
   `0x93B9E0 + slot * 0x128` ([`battle_sprites.md`](battle_sprites.md); the
   sibling's `names/enemies.toml` has the JP side); the ability names are
   the 16-byte GBK field at `0x65C4C8 + id * 0x18`
   ([`battle_window_draw.md`](battle_window_draw.md)). Older and still open:
   the stat labels at `0x669CF0` (the US `Pwr` `Def` `Int` `Agl` stand
   before the verb table in `STATUS.EMI`; check `Pwr` fits the box); the
   skill list's header `龙技` `0x66A220` and the item list's `物品`; the turn
   counter's `残留` / `回合` at `0x669D10` / `0x669D18` (read only by
   `0x43B130`). Captures: `tools/recipes/menu_screens.txt`,
   `battle_commands.txt`, `combat.txt`. Also from §6 there: the seven
   character-at-a-time `Text_DrawAt` callers still at 12 px (`0x45B490`,
   `0x45B5F0`, `0x460730`, `0x460920`, `0x466260`, `0x4B1090`, `0x4B11F0`)
   plus `0x4987E0` and the 8 px UI font `0x516E70`; text in artwork; longer
   names (16-byte fields against the disc's 12, DIV-0008 - owner's call);
   a better upscale; German and French (10 glyph slots free). Saved names
   stay as they are (owner's decision).
7. **Widescreen's debts** ([`widescreen.md`](widescreen.md) §4, §5):
   the oracle and the frame hash once with `BOF3X_WIDE=1`; the attract A/B
   cropped to the middle 640 columns; the sprite and object culls (§3b's
   table), the sky `0x571C85`, the message-box table; whether any area
   change still shows a black centre with live bands; the corner pop with
   the margin at 100. A live `BOF3X_SHADOW=map_layers` under the wide view
   reports the cull's divergence by design.

## Then

Ordered; reasoning lives in [`STATUS.md`](STATUS.md), not here.

8. **`analysis/pairs_propagated.json` errors**: `0x445A30` / `0x44B9F0`
   swapped, `0x44F030` paired to the wrong twin
   ([`battle_damage.md`](battle_damage.md)). Then the divergence map of
   [`attract-remaining.md`](attract-remaining.md) §5.1 (`psx_pair.py areas
   && fill && propagate`, five minutes; never use the `call-disputed` tier),
   scenario overlays, the name import as `hypothesis`.
9. **The first receipt, and a CI job that compiles `src/`**
   ([`STATUS.md`](STATUS.md) open decisions; `.github/workflows/` holds only
   `dco.yml`). The evidence a receipt records exists: `attract_diff.py`,
   `mem_dump.py --compare`, `calltrace.py frames`, the route A/Bs.
10. **[`IDEAS.md`](IDEAS.md) I13, save states** - the random encounter is
    deterministic now (`combat.txt`), so save states are for what no route
    replays: bosses and event battles.
11. **I15, the live look toggle** - the input path is read and ours
    (DIV-0050), so it wants only a key; I19 (curvature for SatPixie) beside
    it.
12. **I18, F12 before shipping** - `Save_QuickWrite` writes a normal save to
    slot 0 from anywhere, battles included; the owner keeps it for now and
    wants it disabled or a true quicksave before shipping. The recorder's
    F12 shot lands on top of it.
13. **The display overhaul's owed checks** ([`display-overhaul.md`](display-overhaul.md)
    §5, [`window-modes.md`](window-modes.md) §6): the edge pixels of `rb1`
    - `BOF3X_PIXEL_OFFSET=0.498046875` against the 27 differing captures of
    the 55-shot attract A/B (`validate_rb1.sh`, about 25 minutes); the
    owner's tuning of the CRT look (`BOF3X_CRT`, best at k = 6 borderless)
    and of SatPixie (the Options dialog by hand, only tried by code); a
    rescale under the CRT look (DIV-0037); the title-bar drag under
    `BOF3X_BACKGROUND=0`; sprite edges at k = 3 / 6 looked at closely.
    Not built, loud if reached: a `Lock` of the primary or back buffer
    (`Gfx_DrawOTag` logs the first request), sub-rectangle locks, depth /
    fog / lighting, the back buffer's `GetDC`. The set-up's pixel formats
    and caps `0xCCD` are this machine's HAL's; the backend takes any RGB
    masks.
14. **Stage 2's regression check**: which of `MsgBox_Step`'s 23 control
    codes the attract sequence's eight messages use (tracer detail mode);
    nothing covers `Msg_OpenSystem` ([`attract-mode.md`](attract-mode.md) §6).
    And a check of the list the draw-order pass builds in the real game -
    I14 level 1 - with a hand-written header for the sprite object (five
    files address it by offset; [`sprite-draw-order.md`](sprite-draw-order.md)
    §5, §6).
15. **The owner, in game** ([`USER_CHECKS.md`](USER_CHECKS.md)): with
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
16. **Finish reading the asset path**: the 32 callers of `LoadDatFile`, the
    value-sequence search for the dropped PSX sections
    ([`asset-loading-path.md`](asset-loading-path.md) §4). For I1, the PC
    block builder `0x5806F0` and the options bytes at block `+0x78`;
    PC-to-PSX is still static only.
17. **Obligations** ([`STATUS.md`](STATUS.md)): contact TheRealBiggs; write
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

Nothing of the game's. PRs 8, 10, 12..16 are merged; `origin/main` is
`0c7260e`. This checkout is on `docs/refresh` with the docs refresh
uncommitted across `docs/`, `README.md` and `CLAUDE.md` (`git status`).
Worktrees and branches to remove: item 1.

Local only, gitignored, worth keeping:

- `bof3/BOF3.CFG`, `build/bof3x.ini`; the owner's saves `bof3/BISLPS00`..`05`
  and `0F.DAT` - **5 is adult Ryu, Lv 38, the one the menu recipes load**
  (a US conversion: menu square, confirm cross, cancel triangle); the combat
  route loads its own F12 save.
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

- **Past 12.4 days of Windows uptime (about 2026-09-27; Fast Startup keeps
  it counting, only a Restart resets it) an all-original run stops pacing
  and, by the code, draws nothing** - `--original "*"` turns DIV-0022 /
  DIV-0047 off with the rest. Reference sides run `*,-Game_Clock` (the
  clock changes pace, never logic). Already past 6.2 days, all-original
  runs go at half speed (D5): hash verdicts stand (logic frames), wall
  times do not. Pace figures before 2026-09-21 are the 31.25 band.
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
  `BOF3X_ORIGINAL` / `BOF3X_SHADOW` lists (2,048 characters) and the
  tracer's owned-function table (256) ended in a `Fatal` before the window -
  90 minutes. Both raised; read the log when a run is slow.
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

- The owner: items 3 and 15; the removal in item 1.
- TheRealBiggs - not yet contacted ([`STATUS.md`](STATUS.md) obligations).

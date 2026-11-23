# The beta.2 hardening proofs of 2026-11-23

**Status:** MEASURED (2026-11-23, `main` `0805b220`, the PR #277 merge; an
away day, the owner's word "Yes, hardening"). Every run below passed or is
explained. Nothing in `src/` changed. Not played by anyone: these are scripted
runs. The `v0.1.0-beta.2` tag stays held for a person's check of the Linux
build (the owner, 2026-11-22). **Since the runs** `main` took PR #278 (the
PSP preset, `1071ec91`): `src/launcher` and its tests and docs, no file of
the game's code; it was not built or run here.

What was owed before a second beta, by [`HANDOFF.md`](HANDOFF.md) as it stood:
the ten state-hash routes for the smooth-motion fixes (PRs #252, #254, #255)
and PR #246's tip, the three recipes that end with `quit`, a game on the
64-bit build since the pointer fixes of the Linux round, a battle on a
disc-built install on the 64-bit Windows build, a game under `BOF3X_ARENA=1`,
and a proof with the game for the romhack behaviours `LevelSkillCatchUp` and
`TownWarp`.

## 1. How it ran

One build of `main` `0805b220` at both widths (`cmake --preset i686`,
`--preset x86_64`) in the worktree `D:\bof3x-runs\hard1123\wt`, built before
the first run. One game at a time, the owner away; the chain
(`D:\bof3x-runs\hard1123\proofs.py`, the exe-retire round's `proofs1120b.py`
with new steps; its log `proofs.out`) starts a run only after 60 quiet
seconds and sets a run aside when a compiler was seen beside it. No kept run
saw one. Hashed runs under the `orig5` conditions (`BOF3X_OPT=none`,
`BOF3X_LAYERING=0`, the skip list, an ini-less launcher copy). The hash files
are `analysis/statehash/cn_<route>_h1123*` in the main checkout (not tracked).

The disc-built install is new: `importer.py setup` from the US PlayStation
disc alone into `D:\bof3x-runs\hard1123\home` (104 s), then the Definitive
Edition's 1.4 patch added by itself (`mods.py add` of the author's `.xdelta`,
`mods.py build`, 87 s: `opt/mod-definitive` 72 chunks, `mod-definitive-en-US`
384 chunks). The owner's slots and `BOF3.CFG` were copied into its `game\`.

## 2. Results

| Step | Result |
|---|---|
| The ten routes, 32-bit exe host, `<main checkout>\bof3` (`_h1123`) | All ten end `done`, rc 0. Against ours of 2026-11-20 (`_xr`) and of 2026-11-16 (`_m1116`): seven identical on every tick; `worldMapAndAreaTransition`, `caughFish` and `masterAndManillo` differ on `0x6C3000` alone (`D3d_BackMaterialHandle`'s page, run-to-run noise). A second run of each of the three (`_h1123b`) differs from the first on that page alone. Against the `orig5` pairs each route reports its recorded signature pages, as `_m1116` does ([`state-hash.md`](state-hash.md) 6) |
| `desertSummary`, `overheatAndDeepSea`, `shisuMinigame` (the recipes that end with `quit`) | Each `recipe done`, rc 0 (frames 2,771, 4,356 and 4,619) |
| `music_heard.py`, `whelpBoss` and `cutsceneAndNue` | Pass: 164 ring lines and 6 tracks, 116 and 5, the counts of 2026-11-20 |
| The hosted shadow suite (`BOF3X_SHADOW='*'`) | `done` in 899 s: 922 `MISMATCHES` lines, all 0, 0 FATAL |
| The exe host's self-test: 32-bit, the moved base, 64-bit | `done`, 10,394 log lines each, 0 FATAL |
| The ten routes on the 64-bit exe host (`_h1123x`) | All ten end `done`. Against the 32-bit runs: seven identical on every tick, the same three on `0x6C3000` alone |
| `combat` and `cutsceneAndNue` under `BOF3X_ARENA=1`, 32-bit (`_h1123a`) | Both `done`; the log names the arena (256 MB reserved); identical to the runs without it on every tick (2,561 and 7,425) |
| The exe host's self-test on the US-disc install, the mod on (the launcher's own hand-over, `language=en-US`, no `BOF3X_OPT` or `BOF3X_ON` in the environment), both widths | exit 0, 0 FATAL: optional layers 2 and 3 are the mod's, `9 tables, 1199 cells, 2286 bytes`, `27 ranges, 43 bytes`, `behaviours: 12 of 14 on` (the twelve of `data/mods/known.toml`) |
| The same, `mods_off=definitive` | exit 0: no mod layer, `0 of 14 on` |
| `BOF3X_ON=LevelSkillCatchUp`, the exe host's self-test on the PC folder | exit 0, `1 of 14 on` |
| `BOF3X_ON=TownWarp` and no mod, the PC folder | The self-test alone ends 0 (it does not load `FIRST.DAT`). A start of `field_menu` stops as designed: `FATAL: DIV-0225 TownWarp is on, and FIRST.DAT has no town-warp table (kind 21 tag 0 ...)` |
| The hosted fuzz, `LevelSkillCatchUp` on, each group alone | `rest_2d`, `field_modes`, `item_use`: exit 0, 0 mismatches. `rest_2e`: the harness stops, section 3 |
| Two routes with the mod on, the US-disc install, 32-bit (a smoke test: the routes were recorded without the mod) | `combat` done at frame 2,620, `menu_screens` at 1,770; both `12 of 14 on`, `DIV-0225 TownWarp: the mod's table and text taken, 11 towns`, no FATAL. In `menu_screens`' save the 1.4 rule ran: `the warp learned (record 4 holds ability 0x40); record 1 takes it` |
| `combat` and `whelpBoss` on the US-disc install, the mod off, 32-bit against 64-bit, the state hash and a frame each second | All four `done`. `combat`: identical on all 2,561 ticks, 87 frames of 87 the same bytes. `whelpBoss`: one page differs from tick 1,002 (section 3, not a fault), 439 frames of 439 the same bytes |

So with every behaviour off (the default), the hosted suite covers the fuzz
rows [`disc-patch-round.md`](disc-patch-round.md) 6b lists as owed "off" for
the two behaviours, and the ten routes are the "state-hash routes off".

## 3. Two things read

**`whelpBoss` at the two widths on a disc-built install: one word, a text
pointer.** `statehash.py diff` reports page `0x93B000` from tick 1,002 to the
end. A replay of both widths with raw dumps at ticks 1,001, 1,002, 1,003 and
5,000 (`p\dump`, `statehash.py bytes --fold 0x400000`): on that page one word
differs, `BattleFlow_TaskCurrent+0x38` (`0x93B8FC`), `0x11AFBED2` in the
32-bit run and `0x117FB8BE` in the 64-bit one, the same at every dumped tick
from 1,002. Each is `0x4E` past the word that the same run holds at
`Battle_CommandLabelPointers+0x80` (`0x11AFBE84`, `0x117FB870`): a place in
the disc-built image's label text, which lies in our own memory at another
address in each build. The same place in both; the hash does not fold it
because the word is not one of the slots it knows. On the PC install the text
is in the game's `.data` and the route is identical at both widths. The
frames agree byte for byte. **Not fixed:** the word could join the skip list
or the fold for disc-built installs; a 32-bit against 64-bit proof on such an
install reads this page as known until then.

**The `rest_2e` fuzz with `LevelSkillCatchUp` on stops in the harness.**
`FATAL: scenario_harness: ours calls 0x101C32B0, which no stand-in covers`.
The address is `AbilityList_ForType` (`llvm-symbolizer` on the run's
`bof3x.dll`: `src/game/menu/char.cpp:1085`), which `CatchUpSkill` calls. So
the fuzz reaches the behaviour's path (a row with Select pressed), and the
group's callee list does not hold the function the path calls. A gap of the
harness; with the behaviour on a mismatch there is the design in any case.
With it off the group passes in the suite.

## 4. Not run, still owed

- The owner's play of the two behaviours ([`disc-patch-round.md`](disc-patch-round.md)
  6b lists each check in play) and of 1.4's rules.
- The hosted fuzz with `TownWarp` on: the hosted build runs in a PC folder,
  which holds no mod layer, so `FIRST.DAT` has no table there.
- `DoubleHitSwords` and `SmokeSkill`'s encounter side still have no
  automated cover (6c's two findings stand).
- Smooth motion on (`interp=1`): the routes ran with the default, off. What
  they show is that the code merged since 2026-11-20 changes nothing the
  game computes with the box clear.
- A person's look at the Linux picture; the 64-bit build under a player's
  hand.
- A compiler ran on the machine twice during the chain (09:00 and 09:07,
  not this session's); the chain waited both times and no kept run saw it.

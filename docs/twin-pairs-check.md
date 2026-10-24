# The propagated PlayStation pairs checked against the US-to-JP map, and the twin markers

**Status:** STABLE (2026-10-23, headless, branch `audit/twins-remainder`; HANDOFF
item 0b's remainder). Addresses and counts only: the pairs file and the maps are
derived from the discs and stay under `analysis/` and the run root, never in the
tree (CLAUDE.md rule 1).

## 1. What was left

[`psx-twin-check.md`](psx-twin-check.md) reads a takeover beside its PlayStation
twin. Two maps find the twin: `tools/psx_pair.py propagate`'s pairs
(`analysis/pairs_propagated.json`, 3,330 JP functions paired with PC ones along
calls, positions and tables) and `tools/region_twin_map.py`, which places a US
decompilation's function boundaries on the JP disc by masked instruction words
(`us_jp.tsv`: 4,131 of 4,397 US functions `exact`) and joins the PC through our
`(PSX 0x...)` citations and the pairs (`pc_map.tsv`). The battle twin-diff found
6 of 190 propagated pairs wrong against 0 of 190 citations (D339). Two things
were left: the mapped PC functions with a matched twin but no citation on their
`// original 0x...` line, and a check of the pairs against the second map.

## 2. The markers (step 1)

**The reader.** `region_twin_map.py` read only `PSX 0x...` on the marker line.
It now also reads `PSX twin 0x...`, `PSX <name> 0x...`, `PSX FUN_80......` and a
citation wrapped between `PSX` and its address; it takes the area from
`in AREAnnn` or an `AreaNN_` name on the line as well as from `(area N` and the
file's `// ---- area N` header; and a citation whose JP address several
overlays share keeps the overlays its function's own pairs resolve to. That read
321 citations the map had missed (1,035 -> 1,356), and of the 15 citations that
landed in more than one overlay at the same address, 13 now land in one.

**The markers.** A mapped PC function got `PSX 0xJP` on its marker line when
(a) a twin read (`tools/twin_bundles.py`, the 377 + 955 reads of the twin-diff)
called its pair `same`, `differs`, `differs-ledgered` or `region?` against the
twin in the pair's **own** overlay, (b) the check below does not flag the pair,
and (c) the re-run map places the new citation in that overlay and no other.
**497 markers added.** Mapped functions with a matched twin and no citation:
**752** with the old reader and the old markers, **77** after step 1, **70**
after step 2's retirements (58 of them never taken over).

Two citations were the function ahead of the twin, a pairs-file hypothesis
copied into `symbols.toml`: `LeaderPanel_S1Choose` `0x528CD0` cited
`0x801DA068` and `LeaderPanel_S4Run` `0x5295F0` `0x801DB03C`; the read twins
are `0x801DA1E8` (the box at `(0x6C, 0x56)`, sounds `0x100` / `0x106` /
`0x103`) and `0x801DB1C0` in AREA030 section 4. Comments and `psx` fields
corrected.

**Withheld (19), each with its reason:**

| PC | name | why |
|---|---|---|
| `0x4680F0`, `0x468210` | `EffectKind0F_Child4Move`, `_DrawCursorPanel` | pairs crossed (D339; D340's twin is `0x801D6CF8`); retired below |
| `0x589110`, `0x589160` | `Sprite_PoseFromSet`, `Sprite_CopyFrames` | pairs crossed (D339); retired |
| `0x535490`, `0x535830`, `0x535D60` | `AreaMap_CellsAllWide`, `Field_WayBlockedWide`, `AreaMap_CellsNoneWide` | paired with a neighbour (D339); retired |
| `0x4DF820` | `Port_DroppedCall` | a bare `ret` where the PlayStation stops and replays (D345); the pair held, no marker on a stub |
| `0x5A99F4` | `Task_ClearPrivate` | read `unsure`: the two schedulers' records differ |
| `0x403050`, `0x409960`, `0x409970`, `0x4253C0` | `Area22_ArmTailOnYes`, `Area49_ShiftCameraUp` / `Down`, `Area152_Record8Spawn` | the read twin is another area's copy; `symbols.toml`'s `psx` names the function's own area, which the map does not cover |
| `0x43C9F0` | `BareRetZero` | a two-instruction stub with twins everywhere |
| `0x488EF0` | `EffectKind86_End` | the AREA032 body is a relative, not the twin (D339) |
| `0x4034C0`, `0x442080`, `0x442600`, `0x465840` | `Area27_ChoiceCounter5Cor5D`, `BattleObj_FallDispatch`, `BattleObj_ReviveByEquip`, `EffectKind05_Follow` | the cited JP address is a function in a second overlay too, and nothing on the line can tell the map which |

## 3. The check (step 2)

    python tools/psx_pair.py check --usjp <run>/us_jp.tsv --twins <run>/pc_map.tsv \
        --x <dir with jp/> [--reads <twin-read dir> ...] [--keep PC:JP ...] [--retire]

`us_jp.tsv` and `pc_map.tsv` are `region_twin_map.py`'s outputs; `--x` the JP
disc files it read; `--reads` the twin-read directories (bundles beside
`results_*.tsv`). A pair whose overlay the map has is flagged when

- **not-a-start**: its JP address lies strictly inside a function the map
  placed `exact` (start and length both certain);
- **cites-other**: our citation gives the PC function a different JP twin in
  the same overlay, and the two bodies' masked words differ;
- **read-mispair**: a twin read called it a mispair **against the pair's own
  overlay's function**. A read made against another overlay's function at the
  same address says nothing about the pair: that was the old map's artefact
  (section 4).

Call agreement was measured and dropped: resolved through every other pair it
is circular (a `call` pair's callees were paired by its own alignment), and
through the independent pairs alone it flagged 28 of 1,033 read-`same` pairs
against 4 of 18 known wrong.

**Result (2026-10-23):** 3,330 pairs; 1,606 in an overlay the map has, 1,724
not (MAGIC, BOSS, the other AREA and COMMU overlays); **38 flagged and
retired** (3,292 left; a re-run flags none). `--keep` held four that a read
or a hand check found right: `0x4DF820:8015DCF0` (the dropped call),
`0x587900:8015E10C` and `0x587900:8015E1B8` (`Sound_PlayById` stands for
three PlayStation functions, the read names all three), `0x437CC0:801F341C`
(an empty body, as its citation's is).

| PC | name | JP retired | overlay | how | flag |
|---|---|---|---|---|---|
| `0x403380` | `Area26_Flag2ESet` | `0x801F3348` | area026 | table | cites-other `0x801F3370` |
| `0x433650` | `BattleFx_WatchIcon` | `0x801E6EE8` | battle 3 | callers | cites-other `0x801E682C` |
| `0x433810` | `BattleFx_FollowOwner` | `0x800A5360` | battle 15 | call-disputed | read-mispair |
| `0x433810` | `BattleFx_FollowOwner` | `0x801E71E0` | battle 3 | callers | cites-other `0x801E722C` |
| `0x435CF0` | `BattleEnemy_PickTarget` | `0x801E28CC` | battle 3 | call-anchored | read-mispair |
| `0x4680F0` | `EffectKind0F_Child4Move` | `0x801D6B70` | area030 | gap | read-mispair |
| `0x468210` | `EffectKind0F_DrawCursorPanel` | `0x801D6CF8` | area030 | gap | read-mispair |
| `0x46C820` | `EffectKind34_V0DebrisStart` | `0x8019F964` | game | callers | cites-other `0x8019EFBC` |
| `0x4964E0` | `GameMode8_Leave` | `0x80198620` | game | callers | cites-other `0x801995C8` |
| `0x496B60` | `Boot_Task` | `0x8014EBA8` | exe | callers | read-mispair |
| `0x518180` | `AreaMap_BlockedWide` | `0x801A2EE0` | game | call | cites-other `0x801A2C64` |
| `0x5183C0` | `AreaMap_BlockedNarrow` | `0x801A2C64` | game | call | cites-other `0x801A2EE0` |
| `0x51AD60` | `Member_Follow` | `0x801BD3B0`, `0x801BD3F8` | game | callers | cites-other `0x801BCFF8` |
| `0x5253E0` | `FieldCore_ScriptMoveAlign` | `0x801B7BC0` | game | callers | read-mispair |
| `0x528CD0` | `LeaderPanel_S1Choose` | `0x801DA068`, `0x801DA588` | area030 | callers | cites-other `0x801DA1E8` |
| `0x5295F0` | `LeaderPanel_S4Run` | `0x801DB03C`, `0x801DC654`, `0x801DC710` | area030 | callers | cites-other `0x801DB1C0` |
| `0x532860` | `Encounter_PartyToPlaces` | `0x801B8980`, `0x801B8E6C` | game | callers | cites-other `0x801C0114` |
| `0x5353E0` | `AreaMap_CellsAll4` | `0x801C4B70` | game | call | cites-other `0x801C4A10` |
| `0x535490` | `AreaMap_CellsAllWide` | `0x801C4A10` | game | call | read-mispair |
| `0x535830` | `Field_WayBlockedWide` | `0x801C5038` | game | call | read-mispair |
| `0x535CA0` | `AreaMap_CellsNone4` | `0x801C58F4` | game | call | cites-other `0x801C5764` |
| `0x535D60` | `AreaMap_CellsNoneWide` | `0x801C5764` | game | call | read-mispair |
| `0x571A70` | `AreaMap_SetupTexture` | `0x80158EE4` | exe | gap | not-a-start (inside `0x80158EBC`) |
| `0x573090` | `Kind2_Dispatch` | `0x801C686C` | game | callers | cites-other `0x801C6830`; read-mispair |
| `0x57F760` | `InnPrompt_Greeting` | `0x801D51C8`, `0x801D58A4` | shop | callers | cites-other `0x801D3BA8` |
| `0x57FF80` | `FieldSave_Confirm` | `0x801D57A0`, `0x801D5AE0`, `0x801D5BC0` | shop | callers | cites-other `0x801D50D0` |
| `0x587B40` | `Music_FadeOutStop` | `0x8015DDF4`, `0x8015DF44` | exe | call | cites-other `0x801626C0` |
| `0x589110` | `Sprite_PoseFromSet` | `0x8014D440` | exe | gap | read-mispair |
| `0x589160` | `Sprite_CopyFrames` | `0x8014D4E4` | exe | gap | read-mispair |

Most `cites-other` rows are a PC function paired along `callers` with several
JP functions, one of which our citation names; the reads that saw the others
called them other functions (`0x46C820`, `0x4964E0`, `0x528CD0`).
`0x587B40`'s two are its callees (`0x8015DDF4` is `SsSepStop`).

**Citations corrected from the check** (comments and `symbols.toml`):
`BattleFx_FollowOwner` `0x433810` is JP `0x801E722C` in BATTLE section 3 (the
same copies, `+0x48` through `+0x3C`, in the same order), not the pairing's
`0x800A5360`; `AreaMap_SetupTexture` `0x571A70` is `0x80158EBC`, whose
scheduled header loads come before the `addiu $sp` the pairing took for a
start; `BattleEnemy_PickTarget` `0x435CF0`'s citation of `0x801E28CC` is
withdrawn - by shape its twin is `symbols.toml`'s hypothesis `0x801E2A0C`
(kinds 1 and 4, bits 0 and 4 of `+0x90`, the side codes), not cited as a
marker because the map would place that address in the shop overlay.

## 4. D339 against the check

**The first round's four:** `0x433810` / `0x800A5360` and `0x435CF0` /
`0x801E28CC` are flagged (read-mispair) and retired. **`0x45BFA0` /
`0x801D4284` and `0x52D140` / `0x801E105C` are right**, and are not flagged:
both reads were made against BATTLE section 3's function at the same address,
not the pair's own overlay (COMMU02 section 8 and AREA030 section 4). Read by
hand against their own overlays: `0x801D4284` in COMMU02 is the hit-and-blow
lost state (message `0x86`, request 2, step 8, the panel `(0x10, 0x18, 1)`, the
secret `(0x68, 0x40, 1)`, the row 8 -> 7) and `0x801E105C` in AREA030 is the
kind row (the draw mode `(0, 1)`, sprites 3..5 by `row * 20` / `row * 25`,
kinds `0x16` and `0xFF`, the name at `(0x70, 0x45 - row * 20)` in colour
`0xF`) - each `CommuHitBlow_Lost` and `FieldPanel_DrawKindRow` exactly.

**The second round's list** has the same artefact: `0x42EF50`, `0x42FAF0`,
`0x4363E0`, `0x441550`, `0x442080`, `0x445640` and `0x581ED0` were read
against the shop or start overlay's function at their JP address; against their
own overlay each was read `same` in the first round, and the check holds them.
Of D339's second-round pairs the check retires `0x573090`, `0x51AD60`,
`0x532860`, `0x5253E0`, `0x535490` / `0x535D60` / `0x535830`, `0x589110` /
`0x589160`, `0x4680F0` / `0x468210` and `0x496B60`; `0x56D8B0`, `0x56C830` and
`0x513AD0` / `0x513D20` / `0x513E80` are in overlays the map has not got, and
`0x433810`'s third pair (`0x800A5CA4`, BATTLE section 15) is not flagged: no
read was made against it and the citation is in section 3.

## 5. Re-running

    python -I tools/region_twin_map.py --decomp <decomp> --x <discs> --repo . --out <run>
    python tools/psx_pair.py check --usjp <run>/us_jp.tsv --twins <run>/pc_map.tsv --x <discs> \
        --reads <reads...> --keep 4DF820:8015DCF0 587900:8015E10C 587900:8015E1B8 437CC0:801F341C --retire

`psx_pair.py propagate` regenerates the pairs from its seeds and would bring the
retired 38 back; run `check --retire` after it.

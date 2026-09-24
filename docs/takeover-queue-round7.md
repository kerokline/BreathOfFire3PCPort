# The seventh round's queue: what the combat route reaches

**Status:** IN PROGRESS (2026-09-24) - ten groups merged, 1,020 ours, the batch passed (section "Result"); the owner's eye owed

The second input-reached queue after the shop's ([`takeover-queue-round6.md`](takeover-queue-round6.md))
and the world map's ([`world-map.md`](world-map.md) §4). The owner recorded
`tools/recipes/combat.txt` - an F12 save in slot 0, a few steps, a random
encounter, the whole fight to "You won the battle!" - and it plays back
identically run to run (43 of 43 captures, [`input-script.md`](input-script.md)),
so battle no longer waits for a save-state system. `combat_ab.txt` (a shot
every 60 frames) is the route; `analysis/validate_combat.sh` A/Bs it (5 of 43
identical, the other 38 at 4..15 tile-edge pixels, the `rb1` class - no logic
difference) and traces it all original (`analysis/calltrace/recipe_combat/`),
less the attract sequence's reach, the shop's and the world map's:

    python tools/attract_catalog.py analysis/calltrace/recipe_combat/bof3x.callcounts.tsv       --minus analysis/calltrace/hidden_b/bof3x.callcounts.tsv,analysis/calltrace/all_a/bof3x.callcounts.tsv,analysis/calltrace/all_b/bof3x.callcounts.tsv,analysis/calltrace/recipe_shop/bof3x.callcounts.tsv,analysis/calltrace/recipe_worldmap/bof3x.callcounts.tsv       --out analysis/combat_catalog.md

**209 reached and not ours; 204 in the groups below.** Left out:
`Task_RunAll` (the task system), `AreaMap_DrawBackdrop` `0x571BE0` (group 2 of
the world-map wave took it the same night), and `0x589110`, `0x589160`,
`0x5891C0` (three small functions under the inventory ops, missed by the cut -
the next round's). The battle engine is PC `0x42E400`..`0x4551A0`, the PSX's
`BATTLE.EMI` overlay compiled into the exe; [`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md)
paired six of its functions and names the RAM layout to read against
(`../BreathOfFire3Recomp/docs/BATTLE_RAM.md`). Sizes are `pe_funcs.py`'s and
run on through pointer-reached neighbours; the "callers" column names the
nearest known start. `0x42E2F0` (2,091 calls a route - the battle's frame,
every group's caller) and `0x596A90` (the window task) are ours already.

## The rule for calls across groups

Unchanged from round six: **each group writes `symbols.toml` entries for its
own functions only.** A call into another group's function goes through a raw
address in your own `_callees.h`, re-aimed at a recording stand-in in the
fuzz. Never bind (`impl`) or rename an address another group owns; say what
you learn about it in your report and doc. Every address already ours is fine
to call through its header.

## The groups

Each: a new `src/game/<file>.cpp` (+ `.h`, `_callees.h`, `_fuzz.cpp`), its own
shadow name (the file's stem), its Inject call at the end of `inject_all.cpp`,
a doc `docs/<file>.md`.

| Group | File | Functions | Bytes | Addresses |
|---|---|--:|--:|---|
| BA - battle set-up | `battle_setup.cpp` | 19 | 4,389 | `0x4301B0` `0x4303D0` `0x430640` `0x430790` `0x430890` `0x430970` `0x430A50` `0x430B30` `0x430C10` `0x430D40` `0x430F40` `0x431030` `0x4453C0` `0x445550` `0x4455C0` `0x444310` `0x446EA0` `0x44AA00` `0x44F4B0` |
| BB - the battle task and the turn flow | `battle_flow.cpp` | 21 | 2,578 | `0x42E400` `0x435110` `0x435180` `0x4351F0` `0x435260` `0x435830` `0x4358A0` `0x4358D0` `0x436090` `0x4360C0` `0x436B50` `0x437470` `0x437580` `0x437780` `0x4377D0` `0x437930` `0x4379D0` `0x432F10` `0x444480` `0x4445A0` `0x4456C0` |
| BC - the battle windows (window-task handlers under 0x596A90) | `battle_windows.cpp` | 22 | 5,739 | `0x441100` `0x441140` `0x441180` `0x4411B0` `0x4411E0` `0x4412B0` `0x442FA0` `0x4432F0` `0x4434C0` `0x443610` `0x443740` `0x443870` `0x4439A0` `0x443B10` `0x443D90` `0x443F60` `0x5979E0` `0x597A00` `0x597A30` `0x597ED0` `0x597F20` `0x597F40` |
| BD - the battle windows' draw helpers | `battle_window_draw.cpp` | 19 | 3,222 | `0x444230` `0x444290` `0x4442C0` `0x4442E0` `0x444340` `0x4447B0` `0x444900` `0x4449E0` `0x444A90` `0x444C40` `0x444CE0` `0x444D50` `0x444E00` `0x444EB0` `0x57DA70` `0x57DC90` `0x5918A0` `0x591DB0` `0x591E50` |
| BE - damage, effects and affinities | `battle_damage.cpp` | 18 | 7,416 | `0x444F40` `0x4450E0` `0x445640` `0x445680` `0x445730` `0x4458B0` `0x445980` `Battle_ApplyDamage` `0x445A30` `0x445CF0` `0x4462B0` `Battle_ScaleDamage` `0x446430` `0x446650` `0x44AE90` `0x44B2C0` `Effect_ApplyResult` `0x44B9F0` `0x44ED10` `Battle_ElementAffinity` `0x44EE80` `0x44F130` |
| BF - the small battle helpers | `battle_misc.cpp` | 27 | 2,186 | `0x4469F0` `0x446A10` `0x446A50` `0x446BB0` `0x446BD0` `0x446C30` `0x446E40` `0x446FB0` `0x446FD0` `0x446FF0` `0x447840` `0x447E60` `0x449E00` `0x449E10` `0x449FE0` `0x44A5C0` `0x44A650` `0x44A6E0` `0x44A810` `0x44A830` `0x44A880` `0x44A8C0` `0x44A8E0` `0x44A990` `0x44AAD0` `0x5171A0` `0x517440` |
| BG - battle sprites, tints and the enemy banner | `battle_sprites.cpp` | 27 | 7,609 | `0x452BF0` `0x452F70` `0x4530D0` `0x453190` `0x453210` `0x4532A0` `0x4537A0` `0x453A10` `0x453B10` `0x453C00` `0x453DA0` `0x453FA0` `0x454380` `0x454410` `Sprite_SetTint` `0x454CC0` `Tint_Release` `0x454D60` `0x454DF0` `0x4551A0` `0x494280` `0x494320` `0x4946C0` `0x494A80` `0x494EA0` `0x494ED0` `0x494F00` `Sprite_UpdateScreenSlot` `0x588F00` `Sound_PlayById` `0x587900` |
| BH - the battle item menu and the PSX setters it uses | `battle_items.cpp` | 20 | 4,368 | `0x4AD6F0` `0x4B7D40` `0x4B8FE0` `0x4B9300` `0x4B9490` `0x4B9680` `0x4B98B0` `0x4C5150` `0x4C54F0` `0x4FB830` `0x4FBD10` `0x4FBDB0` `0x4FC030` `0x4FC1F0` `0x5A75F0` `0x5A76B0` `0x5A76D0` `0x5A7750` `0x5A7140` `0x5A71C0` |
| BI - inventory operations and the event-script bits on the way | `inventory_ops.cpp` | 25 | 5,218 | `0x5920E0` `0x5922A0` `0x592400` `0x5924E0` `0x592600` `0x592760` `0x592800` `0x592890` `0x5928F0` `0x5929D0` `0x592A30` `0x592BD0` `0x592C30` `0x592CD0` `0x592E00` `0x592E10` `0x592E30` `0x52F570` `0x532550` `0x532660` `0x5326B0` `0x532860` `0x532A70` `0x532B60` `0x534880` |
| BJ - three Direct3D handlers, two big draw callees, one map helper | `battle_draw.cpp` | 6 | 3,619 | `0x5A0E80` `0x5A18B0` `0x5A1B50` `0x59CD00` `0x59D200` `0x573050` |

## The live check

After the merge: the combat A/B (`analysis/validate_combat.sh`), which reaches
every function here, the world-map and shop A/Bs, then the attract batch
(oracle, memory dump, frame hash beside an original-vs-original pair) - every
owned function in `analysis/calltrace/entries_logic.txt` first. The runners
kill only the game they started since this night ([`world-map.md`](world-map.md)
§6), so agents' headless self-tests and the coordinator's live runs no longer
collide.

## Result (2026-09-24 morning)

All ten groups merged one branch at a time, the build and the headless
self-tests (`BOF3X_SHADOW='*'`) at 0 mismatches after each: **1,020 ours**
(from 807 at the world-map wave's merge). The session's usage limit cut
every Opus agent off mid-work around 03:00; the worktrees kept everything,
and each agent resumed with its context when told to commit its WIP first
(nothing was restarted). Two groups were not what the catalogue said:
BH is the Healing Herb's sparkle effect and the shared magic-effect draws,
not an item menu; BI is the random encounter's placement on the field map,
not inventory. Their file names keep the queue's. The defects found are
D43..D57 in [`known-defects.md`](known-defects.md). Sizes wrong in
`entries_logic.txt`, per the group docs (BA, BB, BC, BE, BG, BH, BI): about
twenty lines; fixing them changes the frame hash's content, so re-record the
reference (`wm1b_orig`) with them in one go.

**The between-waves batch** (`analysis/validate_wave1.sh` then
`validate_wave1b.sh`, logs `analysis/attract/wave1_batch.log`,
`wave1b_batch.log`; on 807 and then 854 ours): self-tests clean in both
languages; the world-map A/B 7 of 35 identical, the map frames differing by
the needle alone (~650 px) and the rest by the tile-edge scatter; the wide
captures with the sky filling the bands; the combat A/B 5 of 43 at 4..15
px; the shop A/B 5 of 35 at up to 46 px; the attract captures 19 of 55 at
up to 139 px (the same class - the DirectDraw device against ours; not
chased); the oracle aligned (611 of 7,478 frames disagree by a two-frame
timing skew from frame 3376 on, the `wm1_ours` kind - the hash is the
arbiter); **the frame hash identical on every logic frame but frame 0**
against `wm1b_orig`; the memory dump did not dump ("area never read 4"
within 240 s; retried at 420 s in the wave-2 batch).

**The wave-2 batch passed** (`analysis/validate_wave2.sh`, log
`analysis/attract/wave2_batch.log`, 07:05-07:52, 1,020 ours; a first start
at 06:40 was stopped when the owner sat down and the captures grabbed the
browser - `wave2_batch_aborted.log`, not a regression): self-tests clean in
both languages; the world-map A/B 7 of 35 identical, the map frames' only
difference the needle (largest 1,015 px, the fade frame); the wide captures
none black; **the combat A/B 5 of 43 at 4..15 px** - the whole fight, with
the battle engine ours, within the tile-edge class; the shop A/B 5 of 35 at
up to 46 px; the attract captures 19 of 55 at up to 139 px; **the oracle
identical at every logged frame** (7,478 from the alignment point); **the
memory dump: arena and VRAM identical**, the palette row 506 the known
artefact (HANDOFF Traps); **the frame hash identical on all 10,313 logic
frames but frame 0** against `wm1b_orig`. Set the status header to STABLE
when the owner has seen a fight.

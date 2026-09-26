# Group S22: Blizzard, Jolt, Lightning and Myollnir (MAGIC096..099)

**Status:** IN PROGRESS (2026-09-26). All 56 functions are ours
(`src/game/magic_s22.cpp`, shadow name `magic_s22`). They are fuzzed headless
through the shared harness, with 0 mismatches over 112,000 rounds. There are
98 negative controls, and every one is refused. Nothing recorded casts these
spells, so this is fuzz only until the owner sees them cast.

This is round nine, first wave, group S22
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4).
It takes four `Magic_Rows` overlays:

| Row | File | Overlay | Ability ids | Read one id down | Extent |
|---|---|---|---|---|---|
| 102 | 0x271 | MAGIC096 | 0x60, 0xC6 | Blizzard | `0x4C8D40..0x4C9C53`, 13 functions |
| 19 | 0x272 | MAGIC097 | 0x61, 0xC7 | Jolt | `0x4C9C60..0x4CAA66`, 10 functions |
| 37 | 0x273 | MAGIC098 | 0x62, 0xC8 | Lightning | `0x4CAA70..0x4CB886`, 10 functions |
| 36 | 0x274 | MAGIC099 | 0x63, 0xC9 | Myollnir | `0x4CB890..0x4CC96E`, 23 functions |

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2), so each is a hypothesis. By reading,
the code fits: an ice crystal effect for MAGIC096, bolts of gouraud quads and
lines on every live target for 097 and 098, and a darkened caster with nine
children for 099. Which spell is which in play has not been measured. I also
have not verified whether the two ids each row carries mean a party cast and
an enemy cast.

The extents are `tools/magic_rows.py --unit MAGIC0NN --clones`. All 56
functions are in the units' extents; none was found inside or missing.

## 1. What each function does

The `symbols.toml` evidence gives each function to the instruction. In
outline:

- **Blizzard (MAGIC096).**
  - `Blizzard_Task` is the kind-2 task, a three-entry stack table.
  - `Blizzard_Start`:
    - moves the task to the targets' centre (`Blizzard_CenterOnTargets`);
    - makes sixteen shards, each a kind-1 task with parameter 0x4C, its index
      and a delay from `BlizzardShard_Delays`;
    - copies `Gfx_ClutStrip` row 26 back from its source;
    - plays sound 0x100.
  - `Blizzard_Wait` sets the target flag 0x200 once its count runs out.
  - The shard (`BlizzardShard_Task` / `_Run`, a five-entry table by +2) goes
    through these steps:
    - `_Launch` starts at an offset from `BlizzardShard_Offsets`, turned by
      direction through Capcom's unnamed `0x446770`, with a random tilt for
      directions 0..3;
    - `_Grow` grows up to `BlizzardShard_Sizes`;
    - two count-downs belong to other units (MAGIC086 `0x4C2D90`, MAGIC130
      `0x4E47F0`);
    - `_End` decrements the parent's count and frees the task.
  - Each frame the shard draws a crystal (`_DrawCrystal`: three facets of a
    textured triangle and quad), then a fan and a ring of gouraud primitives
    under the actor's matrix.
- **Jolt and Lightning (MAGIC097 / 098)** are one design twice:
  - `*_Start` makes a bolt (kind-1 task 0x14 or 0x0E) on every live enemy or
    party member (bit 6 of the target byte chooses the side). The task then
    takes the last bolt's position.
  - The bolt goes through four steps (`JoltBolt_Steps` / `LightningBolt_Steps`,
    shared bodies):
    - wait, then a random phase;
    - rise, then darken the target (tint -8, flag 0x10);
    - fade (flag 0x40, the tint released, a flash);
    - end.
  - The bolt draws three rows of a band: seventeen steps of four gouraud quads
    round a jittered square (`*_DrawBand(a1, a2, a3)`: inset, radius, jitter
    mask). It then draws zigzag lines (`*_DrawArcs`) and two flashes, each
    eight triangles round its screen point (`*_DrawFlash`).
  - Lightning's band turns by `& 0x1F << 7` and steps the first and third
    quads twice as far.
  - `LightningBolt_PushMatrix` is shared by all three spells' bolts, turned
    (0, 0, 0x200).
- **Myollnir (MAGIC099).**
  - `Myollnir_Start` does three things:
    - tints the source sprite (`0x904B4C`) black;
    - makes nine children of kind-1 task 0x0C: one bolt, four orbs, four rings
      (`MyollnirChild_Kinds`);
    - plays sound 0x100.
  - `Myollnir_Darken` darkens the tint record one step a frame, for eight
    frames. It is `BattleFx_Brighten` inverted, and `BattleFx_Brighten` is
    entry 3 of the task.
  - `Myollnir_WaitChildren` waits for the first child, and `Myollnir_End`
    waits for all nine.
  - The bolt draws one band row at the owner.
  - Orbs and rings circle the owner, at radius 0x28 and 0x18/0x10
    (`Myollnir_GrowHold`, `*_Circle`, `*_End`).
  - `Myollnir_DrawBand` skips the rest of a step when the first quad's third
    point projects at or above screen y 0.0.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with two
exceptions that follow the project's precedent
([`magic_fx_reached.md`](magic_fx_reached.md) §3):

- A phase past any of the ten dispatch tables aborts.
- `Blizzard_CenterOnTargets` aborts where the original would divide by zero,
  because every actor on the side is out (§6). The coordinator should confirm
  this counts as the same precedent.

Two calls push one argument more than the callee takes, as the originals do,
and ours pushes it too:

- `Gte_RotTrans` gets a flag pointer.
- The projections get depth and flag pointers.

## 3. Calls to other units (by raw address, not bound)

| Address | Owner | Reached as |
|---|---|---|
| `0x4C2D90` | MAGIC086 (S19) | entry 2 of `BlizzardShard_Run`'s stack table: +0xB down, then +2 on |
| `0x4E47F0` | MAGIC130 | entry 3 of the same table: +9 down, then +2 on |
| `0x4E5200` | MAGIC131 | entry 1 of `Jolt_Task` and `Lightning_Task`: the done flag and free once +0xB is 0 |
| `0x5A7590` | engine, unnamed | the PSX's SetPolyFT3 (code 0x24 at +7, 0.01 at +0x10/+0x20/+0x30) |
| `0x446770` | engine, unnamed | turns a task's dx/dz pair +0xC/+0x10 by its direction +8 |

The fuzz registers them as callees or handlers by address. Round seven's and
eight's functions (`BattleFx_Finish`, `BattleFx_Brighten`,
`MagicFx_PushActorMatrix`, the GTE / GPU layer, `MapView_LinkPrimAt`) are
called through their names.

## 4. Named data (`symbols.toml` `[[data]]`)

| Kind | Tables |
|---|---|
| MAGIC096's tables | `BlizzardShard_Offsets` `0x65B654` (16 dx/dz pairs), `_Sizes` `0x65B6D4`, `_Delays` `0x65B6E4` |
| Handler tables | `BlizzardShard_TaskTable` `0x65B6F4`, `JoltBolt_TaskTable` `0x65B6F8`, `JoltBolt_Steps` `0x65B6FC`, `LightningBolt_TaskTable` `0x65B70C`, `LightningBolt_Steps` `0x65B710`, `MyollnirChild_Kinds` `0x65B720`, `MyollnirBolt_Steps` `0x65B72C`, `MyollnirOrb_Steps` `0x65B73C`, `MyollnirRing_Steps` `0x65B74C` |

This document records only their addresses and sizes, not their values.

## 5. The fuzz

`BOF3X_SHADOW=magic_s22` runs `magic_harness::Run` over all 56 clones, 2,000
rounds each. It uses the consolidated harness (HX, `ea27991`) without edits.

**Callees and tables.**

- The group lists 44 callees:
  - the GTE / GPU layer, `Battle_ActorIsOut`, `Sprite_SetTint`;
  - the two unnamed engine functions;
  - this group's draws and pushes, called by address.
- It lists nine `.data` handler tables.

**Regions.** It adds these to the harness's own:

- DamageScratch `0x903850` and `Prim_VertexScratch` `0x9037A0`;
- `Gfx_PacketNext` and a 512-byte packet buffer of the fuzz's own;
- `MoveScript_TintRecords`;
- CLUT row 26 and its source;
- MAGIC096's `.data`.

**Harness features used.**

- `deref`:
  - The projections' vertices are logged by what they hold, 6 bytes each.
    Without that, the scratch they sit in is rewritten every step, so a wrong
    vertex went unseen. Control J17 was not refused until this was added.
  - The two matrix pushes log their vectors through `deref`.
- `effect`:
  - The pushes' GTE callees (`Gte_RotTrans`, `_RotMatrix`, `_MulMatrix0`) have
    effects that write a result where the real callee writes, and
    `Gte_SetTransMatrix` logs the translation.
  - `Battle_ActorIsOut` has an effect that keeps one actor of each side in,
    chosen by the seed, so the centre never divides by zero.

**Before the port.** These three functions ran in a fuzz of their own
(`SelfTestOwn`) until the port moved them into the shared one. The group's
earlier crash in the shared fuzz was the Disturb bug the fold fixed: the
seed's 0x40 side bit took the target byte past the enemy records.

**The seed and disturbance.**

- **Seed.**
  - Every dispatcher gets an index inside its table.
  - Each count-down is one or two steps from its end.
  - The loops bounded by +0xA are kept short for the log.
  - The side bit is on half the time.
  - `Gfx_PacketNext` points at one of four places in the buffer.
  - The float that `Myollnir_DrawBand` compares is set to 0.0, -0.0, ±1,
    ±1e-30 or a NaN.
- **Disturb.** After a call, the group's disturbance moves one of these:
  - `Gfx_PacketNext`;
  - a scratch word (the loop bounds kept small and the count dword non-zero);
  - a vertex word;
  - task fields the harness leaves alone;
  - the owner's position and height;
  - the side bit.

Result in this worktree (2026-09-26):

    shadow      magic_s22 self-test: 112000 rounds over 56 functions (2000 each), 4500803 calls to the stand-ins,
                0 MISMATCHES; 16180 bytes of state (16 regions) and the stand-ins' log compared

`BOF3X_SHADOW='*'` exits 0.

## 6. Controls

There are 98 plants, each put in `magic_s22.cpp` one at a time by a script
that was not committed. For each: build, run
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s22`, restore. All 98 are
refused. Every one exits 3, and all but B27 show a count, only in the
functions the plant touches. The figures are mismatched rounds out of 2,000.

| | Planted | Refused in |
|---|---|---|
| B1 | Blizzard_Task entries 0/1 swapped | Blizzard_Task 1,312 |
| B2 | Start: shard delay + 2 | Blizzard_Start 2,000 |
| B3 | Start: CLUT row one word short | Blizzard_Start 2,000 |
| B4 | Wait: flags 0x100 | Blizzard_Wait 642 |
| B5 | BlizzardShard_Task: wrong handler | BlizzardShard_Task 2,000 |
| B6 | Shard_Run: +2 not tested | BlizzardShard_Run 200 |
| B7 | Shard_Run: fan and ring swapped | BlizzardShard_Run 801 |
| B8 | Launch: +0x3C from the owner's +0x38 | BlizzardShard_Launch 656 |
| B9 | Launch: tilt base 0x81 | BlizzardShard_Launch 25 |
| B10 | Launch: re-roll one in eight | BlizzardShard_Launch 2 |
| B11 | Launch: parity inverted | BlizzardShard_Launch 41 |
| B12 | Launch: +0xB 0x1F - delay | BlizzardShard_Launch 656 |
| B13 | Grow: +9 up by 1 | BlizzardShard_Grow 2,000 |
| B14 | Shard_End: owner's +0xA | BlizzardShard_End 678 |
| B15 | PushMatrix: tilt from +0x1A | BlizzardShard_PushMatrix 2,000 |
| B16 | pushes: height / 4 | both PushMatrix 2,000 |
| B17 | LightningBolt_PushMatrix: rotation 0x100 | LightningBolt_PushMatrix 2,000 |
| B18 | Crystal: tip 33 x size | DrawCrystal 1,988 |
| B19 | Crystal: facet 0xC00 skipped | DrawCrystal 2,000 |
| B20 | Crystal: u + 0x21 | DrawCrystal 294 |
| B21 | Crystal: shade (7 - index) | DrawCrystal 1,985 |
| B22 | Crystal: quad sorted 0x44 | DrawCrystal 2,000 |
| B23 | Fan: shade a x 6 | DrawFan 1,920 |
| B24 | Fan: centre blue from the wrong word | DrawFan 1,993 |
| B25 | Ring: inner (a + 7) x 16 | DrawRing 2,000 |
| B26 | Center: + 0x4001 | CenterOnTargets 2,000 |
| B27 | Center: seven enemies | CenterOnTargets: mismatches, then ours' own abort (the kept enemy was 7) |
| B28 | Center: height from +0x3C | CenterOnTargets 1,999 |
| J1 | bolts: +9 x 10 + 2 | Jolt_Start 2,000, Lightning_Start 2,000 |
| J2 | bolts: slot 0 at the end | Jolt_Start 1,957, Lightning_Start 1,960 |
| J3 | Jolt_Start: parameter 0x15 | Jolt_Start 2,000 |
| J4 | JoltBolt_Run: second band jitter 0x1F | JoltBolt_Run 803 |
| J5 | JoltBolt_Run: flash 0x11 across | JoltBolt_Run 802 |
| J6 | JoltBolt_Rise: +0xA up by 3 | JoltBolt_Rise 2,000 |
| J7 | darken: tint -9 red (enemy) | JoltBolt_Rise 340, LightningBolt_Rise 352 |
| J8 | darken: party flags 0x11 | JoltBolt_Rise 338, LightningBolt_Rise 341 |
| J9 | Fade: party flash index + 1 | JoltBolt_Fade 326 |
| J10 | JoltBolt_End: +0xA down by 1 | JoltBolt_End 2,000 |
| J11 | Jolt band: angle & 7 | JoltBolt_DrawBand 2,000 |
| J12 | band radius + 1 | both DrawBand 2,000 |
| J13 | Jolt band: second quad step x -3 | JoltBolt_DrawBand 2,000 |
| J14 | band shade: one byte of row 2 | JoltBolt 486, LightningBolt 488 |
| J15 | band: first row's top one byte short | JoltBolt 77, LightningBolt 66 |
| J16 | band start: +6 = +0xA x 3 | JoltBolt 1,694, LightningBolt 1,692 |
| J17 | band start: height - Rand & 1 | JoltBolt 831, LightningBolt 802 (not refused before `deref`) |
| J18 | Jolt arcs: swing 0x41 + | JoltBolt_DrawArcs 1,845 |
| J19 | Jolt arcs: start radius 0x41 | JoltBolt_DrawArcs 1,997 |
| J20 | Jolt flash: blue x 11 | JoltBolt_DrawFlash 1,996 |
| J21 | flash: Rand & 3 | the three flashes 910 / 967 / 992 |
| J22 | flash: centre y from +0x2E | the three flashes 2,000 each |
| L1 | LightningBolt_Run: +0xB += 5 | LightningBolt_Run 762 |
| L2 | LightningBolt_Wait: Rand & 7 | LightningBolt_Wait 344 |
| L3 | LightningBolt_Rise: +0xA up by 5 | LightningBolt_Rise 2,000 |
| L4 | Lightning band: mask 0xF | LightningBolt_DrawBand 982 |
| L5 | Lightning band: third quad x 3 | LightningBolt_DrawBand 2,000 |
| L6 | Lightning arcs: swing 0xA1 + | LightningBolt_DrawArcs 1,880 |
| L7 | Lightning arcs: blue 0x21 | LightningBolt_DrawArcs 1,921 |
| L8 | Lightning_Start: parameter 0x0F | Lightning_Start 2,000 |
| M1 | Myollnir_Task entries 2/3 swapped | Myollnir_Task 803 |
| M2 | Start: tint blue 1 | Myollnir_Start 2,000 |
| M3 | Start: orb +0xB i << 1 | Myollnir_Start 2,000 |
| M4 | Start: rings made as orbs | Myollnir_Start 2,000 |
| M5 | Start: +9 7 | Myollnir_Start 2,000 |
| M6 | Darken: +5 not +3 | Myollnir_Darken 2,000 |
| M7 | Darken: flags 0x20 | Myollnir_Darken 640 |
| M8 | WaitChildren: +9 9 | Myollnir_WaitChildren 1,004 |
| M9 | End: at 8 | Myollnir_End 916 |
| M10 | End: flag bit 2 not 4 | Myollnir_End 323 |
| M11 | children: bolt and orb swapped | MyollnirChild_Task 1,323 |
| M12 | MyollnirBolt_Run: a1 0x61 | MyollnirBolt_Run 1,506 |
| M13 | MyollnirBolt_Start: +0x3C from +0x38 | MyollnirBolt_Start 2,000 |
| M14 | MyollnirBolt_Hold: +9 down by 2 | MyollnirBolt_Hold 2,000 |
| M15 | MyollnirBolt_End: owner's +0xA | MyollnirBolt_End 667 |
| M16 | MyollnirOrb_Run: +2 not +0 | MyollnirOrb_Run 955 |
| M17 | orbit: z from the owner's x | the six orbit functions 2,000 each |
| M18 | orbit: sine of the kept angle, not the re-read word | Orb_Start 8, Orb_Circle 2, Orb_End 1, Ring_Start 4, Ring_Circle 1, Ring_End 4 |
| M19 | orbit step: angle + 2 | the four steps 2,000 each |
| M20 | Orb_Start: angle i << 4 | MyollnirOrb_Start 1,744 |
| M21 | Orb_Start: radius 0x29 | MyollnirOrb_Start 2,000 |
| M22 | GrowHold: +9 0x3D | Myollnir_GrowHold 1,288 |
| M23 | Orb_Circle: radius 0x27 | MyollnirOrb_Circle 2,000 |
| M24 | Orb_End: radius 0x27 | MyollnirOrb_End 1,997 |
| M25 | Myollnir band: radius 0x11 | Myollnir_DrawBand 1,995 (not refused before `deref`) |
| M26 | band: shade x 14 | Myollnir_DrawBand 1,991 |
| M27 | band: > not >= 0.0 | Myollnir_DrawBand 782 |
| M28 | band: a2 not as a short | Myollnir_DrawBand 2,000 |
| M29 | band: third step x 3 | Myollnir_DrawBand 1,075 |
| M30 | band: first quad +0x16 1 | Myollnir_DrawBand 1,665 |
| M31 | Ring_Run: Jolt's arcs | MyollnirRing_Run 1,488 |
| M32 | Ring_Start: angle + 0x11 | MyollnirRing_Start 2,000 |
| M33 | Ring_Start: radius 0x19 | MyollnirRing_Start 1,999 |
| M34 | Ring_Grow: +9 0x3B | MyollnirRing_Grow 1,296 |
| M35 | Ring_Circle: radius 0x11 | MyollnirRing_Circle 1,998 |
| M36 | Ring_End: radius 0x17 | MyollnirRing_End 2,000 |
| M37 | ring arcs: swing 0x81 + | MyollnirRing_DrawArcs 1,856 |
| M38 | ring arcs: > not >= | MyollnirRing_DrawArcs 1,744 |
| M39 | ring arcs: start radius 0x41 | MyollnirRing_DrawArcs 1,994 |
| M40 | Myollnir flash: radius as a word | Myollnir_DrawFlash 1,998 |

The thinnest are these:

- **B10 (2 rounds):** Launch's one-in-sixteen re-roll, seen only when Rand's
  low nibble lands on 8.
- **M18 (1 to 8):** the orbit's sine taken from the kept angle rather than
  the word re-read after the cosine. It shows only when the disturbance moves
  that word between the two calls.

The J17 and M25 figures are after `deref`. Before it, the ported fuzz
refused neither: the vertex words those two plants change were overwritten
by the next step before the state compare. The fuzz was fixed rather than
the plant dropped.

## 7. What nothing reached

No recorded route casts any of these spells: the combat route casts none
(queue §5). The live check is the owner casting them, with a save that has
them or DIV-0045's cheat. Things to look for:

- Blizzard: sixteen shards falling round the targets' centre.
- Jolt and Lightning: a bolt on every target, which darkens then flashes.
- Myollnir: the caster darkened, with an orbit of orbs and rings.

## 8. Latent defects (Capcom's, kept)

These are described here, not numbered:

- **`Blizzard_CenterOnTargets` divides by zero** when every actor on the
  targeted side is out. The count is 0 and `idiv` faults. Ours aborts with a
  Fatal there instead. Whether a cast can reach it (a target side all out
  when Blizzard starts) is not measured.
- **`Jolt_Start` / `Lightning_Start` read slot 0** when no bolt was made
  (every target out). The slot index starts at 0 and is not checked, so the
  task takes task slot 0's position. It is harmless, and kept.
- **Every dispatcher's index is unchecked.** That covers the ten stack or
  `.data` tables, including the one-entry task tables: an index of 1 reads
  the next overlay's table. Ours aborts.
- **`BlizzardShard_Launch` / `_Grow` / `Blizzard_Start` index their `.data`
  tables** by the shard's +4, which is unbounded. `Myollnir_Darken` indexes
  `MoveScript_TintRecords` by +0xA (the tint slot `Sprite_SetTint` returned),
  also unbounded.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 46 lines under a `group S22` comment: 41 new,
plus five host extents re-listed smaller. Those five are `004C9AF0 5B5`,
`004CA8C0 561`, `004CB6E0 7A7`, `004CBE90 738` and `004CC7D0 71A`, now each
function's own size. Ten were listed right already.

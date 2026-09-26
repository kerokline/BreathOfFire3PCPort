# Spell group S20: MAGIC087, MAGIC088, MAGIC092

**Status:** IN PROGRESS (2026-09-26) - 51 functions ours
(`src/game/magic_s20.cpp`, shadow name `magic_s20`), fuzzed headless
through the shared harness with 0 mismatches; @CONTROLS_SUMMARY@. No
recorded route casts any of the three: fuzz only until the owner's eye.

Round nine, first spell wave, group S20
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4).
Three `Magic_Rows` overlays, taken whole through the harness
([`magic_harness.md`](magic_harness.md)). The functions are named by their
PSX file (`Magic087_`, `Magic088_`, `Magic092_`), not by a spell: the
ability names are the sibling's `magic.toml` **read one id down**, and
nothing read here confirms them. They are:

| Unit | Row | Ability ids | Read one id down (a hypothesis) |
|---|--:|---|---|
| MAGIC087 | 48 | `0x57`, `0xBD` | Silence |
| MAGIC088 | 29 | `0x1E` `0x1F` `0x58` `0x59` `0x5A` `0xBE` `0xBF` `0xC0` | Molasses, Tarbaby, Slow, Blunt, Weaken |
| MAGIC092 | 49 | `0x5C`, `0xC2` | Fireblast |

Whether any of these is a party spell or an enemy one is not measured
here. What the effects look like is described below from the code alone.

## 1. The extents

`tools/magic_rows.py --unit MAGIC087 / 088 / 092 --clones` (2026-09-25):
22 + 16 + 13 functions, none ours before, all taken. None was found
outside the extents and none is missing from them.

| Unit | Extent | Functions | Bytes |
|---|---|--:|--:|
| MAGIC087 | `0x4C3490..0x4C4486` | 22 | 3,897 |
| MAGIC088 | `0x4C4490..0x4C4FB1` | 16 | 2,769 |
| MAGIC092 | `0x4C5680..0x4C62F6` | 13 | 3,112 |

Shared bodies. 19 of MAGIC087's functions are reached from MAGIC083 and 086
as well (group S19's). `Magic088_WaveGrow` is reached from MAGIC039 and 040.
Seven of MAGIC092's are reached from MAGIC088 and 091, and two
(`Magic092_FlameGrow`, `Magic092_DrawFlame`) from nine files
(`analysis/magic_funcs.tsv`, `reached_by`). They lie in these extents, so
they are this group's; the harness keys on the address.

## 2. What they do

Every function is a battle task step: `void (void)`, with `Sprite_Current`
the slot and `0x93B940` its owner. The exceptions are `Magic087_DrawColumn(int)`
and the two allocators and `Magic088_Variant`, which answer al.
`symbols.toml` has each one's reading, instruction by instruction.

**Two private pools.** MAGIC087 keeps 80 "motes" of 0x84 bytes at
`0x6906D8` (`Magic087_Motes`). MAGIC092 keeps 48 at `0x693018`
(`Magic092_Motes`), directly after. Each overlay's kind-2 task first runs
its phase, then walks its pool. For every mote with `+0` bit 0, it sets
`Sprite_Current` to the mote and `0x93B940` to the mote's `+0x80`, runs the
mote's dispatcher, and puts both back. Each allocator answers the first
free mote's index, or `0xFF` when all are in use.

**MAGIC087 (row 48).** `Magic087_Task` has a two-entry stack table:
`_Start`, then `BattleFx_Finish` (`0x4F7350`, ours). `_Start` clears the
pool. For every actor on the target's side that is not out, it creates a
kind-1 child with parameter `0x27`. The side is the eight enemies when
`0x904B44` has bit `0x40`, else the three members. Each child sits at its
actor's position, raised by `0x800000`; an enemy's is raised again by a
byte at `0x8C564F + 0x8C * type`. `_Start` then sets CLUT strip row 26
semi-transparent, from `Gfx_ClutStripSource`. The child dispatches twice:
by type (`0x65B4E8`, one entry), then by phase (`0x65B4EC`, five phases):

- phase 0 spawns eight circling motes, four of each of the two orbit
  types;
- phase 1 waits for two of them;
- phase 2 spawns two rising motes every 8 frames, 16 times;
- phase 3 grows;
- phase 4 frees the child when the motes are gone (`+0xB` = `0x80`).

Phases 2 and 3 draw `Magic087_DrawColumn` twice (offsets 0 and 15). It
draws up to eight textured quads stacked up the actor's screen point,
swaying by a sine. The shade carries from each quad's top to the next
one's bottom.

A mote dispatches by type (`0x65B500`):

- **Circling motes** (types 0 and 1, `Magic087_OrbitRun`, four phases):
  after a delay they spin round the owner. The radius shrinks from `0x100`
  by 4 a frame, at an angle stepping 64. They wait for the owner's `0x82`,
  then fade.
- **Rising motes** (types 2 and 3, `_RiseRun`, two phases): they climb 8 a
  frame for 32 frames.

Each mote draws `Magic087_DrawTriangle`: a triangle outline in two
three-point gouraud lines, under the matrix `Magic087_PushMatrix` loads.
That is `MagicFx_PushActorMatrix`'s shape, with a turn by `+0xB` and a
lift of `0x100`.

**MAGIC088 (row 29).** `Magic088_Task` has a seven-entry stack table:

1. `_Start`: takes the variant from `Magic088_Variant`. It creates two
   kind-1 children (parameter `0x20`), copies CLUT strip rows 26 and 27
   from `0x4000` below, and plays sound `0x100`.
2. `_TintOn`: tints the source sprite (`0x904B4C`) black.
3. `_Darken`: steps the tint record down 8 frames.
4. `0x4EF7C0` (MAGIC167's): waits until the children are down to one.
5. `_Lighten`: steps the tint back up, then flashes the target.
6. `_Apply`: calls the effect library's stat change `0x4FB6F0` with the
   stat index `0x65C39C[variant]` and the target. It creates a kind-1
   child (parameter `0x48`) whose `+4` is `variant + 4` when the change
   answers non-zero, else 8.
7. `0x4E5200` (MAGIC131's end).

`Magic088_Variant` reads the ability id (word `0x904B80`). With the side
byte `0x904B35` at 4, a byte table in `.text` (`0x4C488C`, read in place)
maps ids `0x1E..0xC0` onto four answers:

- `0x1E`, `0x58` and `0xBE` answer 1;
- `0x1F`, `0x59` and `0xBF` answer 2;
- `0x5A` and `0xC0` answer 0;
- anything else answers 3.

On the other side, id `0x112` is rewritten to `0x5A` and answers 0.

The two children dispatch by type (`0x65B574`):

- **A fan** (`_FanRun`, three phases that are other overlays' bodies):
  sixteen gouraud triangles of radius `0xF0` under the actor's matrix.
- **A wave** (`_WaveRun`, four phases): grows, holds, fades.
  `_WaveStep` steps the six band phases at `0x65B588`. `_DrawWave` draws
  six bands of 64 flat quads round the actor. Each point is lifted by a
  sine of its band's phase plus its column. The colour comes from
  `0x65B52C` by variant and band.

**MAGIC092 (row 49).** `Magic092_Task` has a two-entry stack table:
`_Start`, then `0x4E5200`. After the phase it walks pool B. `_Start` does
what MAGIC087's does, with parameter `0x28` and delays `16 n + 1`, then
makes CLUT strip row 26 semi-transparent from `0x4000` below. The child
dispatches by type (`0x65B5C8`, one entry), then by phase (`0x65B5CC`,
four phases):

- phase 0 spawns three motes (a flame, then two sparks);
- phase 1 tints the actor at `+9` = 16;
- phase 2 is `0x4E93C0` (MAGIC144's), which waits for bit 7;
- phase 3 releases the tint, flashes the actor, sets its flag `0x40` and
  frees the child.

While the child lives it draws MAGIC144's `0x4E9420` under the actor's
matrix.

A mote dispatches by type (`0x65B5DC`):

- **The flame** (`_FlameRun`, whose phases are MAGIC144's bodies and
  `_FlameGrow`): `_DrawFlame` draws 28 rows of four textured gouraud quads
  up the screen point. The half-widths are sines of the row times `+9`,
  with `Rand() & 15` in the outer pair.
- **The spark** (`_SparkRun`): moves the screen point and tail-jumps to
  MAGIC144's `0x4E9850`.

## 3. Calls to other groups (raw addresses)

| Address | Owner | Called from |
|---|---|---|
| `0x4FB6F0` | LIBRARY (group L) | `Magic088_Apply` (`MH_AT`, two u8 arguments, al a flag) |
| `0x4E9420`, `0x4E9850` | MAGIC144 | `Magic092_ChildRun`, `Magic092_SparkRun` |
| `0x4EF7C0` | MAGIC167 | `Magic088_Task`'s stack table (a handler) |
| `0x4E5200` | MAGIC131 | the stack tables of `Magic088_Task` and `Magic092_Task` |
| `0x4C2D10`, `0x4EF840`, `0x4B1740` | MAGIC086, MAGIC167, MAGIC060 | `Magic088_FanPhases` (read in place) |
| `0x4E47F0` | MAGIC130 | `Magic088_WavePhases` |
| `0x4E93C0`, `0x4E9630`, `0x4E9690`, `0x4E96B0`, `0x4E9770`, `0x4E97D0`, `0x4E97F0` | MAGIC144 | MAGIC092's phase tables |
| `0x4E5950` | MAGIC131 | `Magic092_SparkPhases` |

`BattleFx_Finish` (`0x4F7350`) is ours already: `Magic087_Task`'s table
calls it by address through `Phase`. No other group's address is bound or
renamed here.

## 4. The fuzz

`BOF3X_SHADOW=magic_s20` (`magic_s20_fuzz.cpp`) runs 51 copies at 2,000
rounds each.

**Callees.** Beyond the harness's standard set, it lists:

- the PSX library layer the draws call (`Gpu_*`, `Gte_*`, `Math_Sin` /
  `Cos`, `MapView_LinkPrimAt`, `Gfx_CommitPrim`);
- `Battle_ActorIsOut` and `Sprite_SetTint` (five arguments logged);
- the group's own functions that call each other;
- the three raw addresses.

`deref` logs what the matrix calls read on the stack (`Gte_RotTrans`'s
vector, `Gte_RotMatrix`'s angles) and the vertices each projection reads
as it is called. Stack addresses themselves are masked off. `ret_mask 0xFF`
on the two allocators and `Magic088_Variant` compares their al.

**The thirteen `.data` tables** are swapped for recorders, each with the
entries its overlay reaches.

**Regions.** Beyond the harness's standard set:

- the two pools, and 256 slots' worth from pool B's start: an allocator's
  stand-in answers any byte, 0xFF included, and the unchecked callers write
  there;
- `Gfx_PacketNext` and the fuzz's own packet buffer, which it points at
  every round;
- 256 tint records;
- the scratch `0x903850..0x90385F` and `Prim_VertexScratch`;
- the CLUT source rows and strip rows 26 and 27;
- MAGIC088's band phases and the ability id.

That is 59,120 bytes in 19 regions.

**Seeds.** Each dispatcher's index stays inside its table. Each compare's
two sides are seeded:

- `+9` one step from its threshold;
- `+0xB` at 2, `0x80` or `0x82`;
- the radius reaching 0;
- the side bit `0x40`;
- the variant's ids and their neighbours, with and without side 4;
- `DrawColumn`'s `+4` round `0x10` and its phase 3;
- the pools full but for one mote, or full.

**Result (2026-09-26, this worktree, on the consolidated harness `ea27991`):**

    shadow      magic_s20 self-test: 102000 rounds over 51 functions (2000 each), 10663623 calls to the stand-ins,
                0 MISMATCHES; 59120 bytes of state (19 regions) and the stand-ins' log compared

Every stand-in and every handler was reached (the coverage lines). The
draws dominate: `Magic088_DrawWave` makes 4,262 calls a run.
`BOF3X_SHADOW='*'`: exit 0.

Before the harness was consolidated, this group added four things to it:

- a log of 8,192 entries, compared by prefix;
- `LogPointee`, now `Callee::deref`;
- `LogReturn`, now `Clone::ret_mask`;
- the Disturb 12 / 13 fix.

That last one was the crash that found it. A target of 2 made the
enemy-record disturbance write `0x93B940`, the owner pointer, and a side
bit indexed past the image. All four are in the consolidated harness
([`magic_harness.md`](magic_harness.md) §7).

## 5. The controls

@CONTROLS_TABLE@

## 6. What nothing reached

Everything here is fuzz only. No recorded route casts rows 29, 48 or 49
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §5).
The owner's live check is to cast the spells: with a save that has them,
or with DIV-0045's cheat.

What to look for, by the code:

- **MAGIC087:** a swaying column of quads over each actor on the target's
  side, and triangles circling in and rising.
- **MAGIC088:** the caster darkening and coming back, a fan and a wavy
  ring, and the stat change.
- **MAGIC092:** the actors tinted, flames rising from them, and sparks.

The fuzz cannot tell whether the picture is right. It can only tell that
ours computes what Capcom's does.

## 7. Defects (Capcom's, latent, kept)

These are described here and not numbered; the coordinator numbers them.

- **Unbounded dispatch.** None of the three stack tables and none of the
  thirteen `.data` tables is bounds-checked. Ours aborts past a stack table
  (the precedent) and reads the `.data` tables in place, as Capcom's does.
- **A full pool.** `Magic087_ChildSpawn` and `Magic087_ChildRing` do not
  test pool A's `0xFF`: with the pool full, their writes land 0xFF motes
  in, inside pool B. `Magic092_ChildSpawn` does test it.
- **The task slot index.** `BattleTask_Create`'s index is used untested in
  `Magic087_Start`, `Magic088_Start`, `Magic088_Apply` and
  `Magic092_Start`. `0xFF` writes past the 48 slots.
- **The tint index.** `Magic088_Darken` and `_Lighten` index the 32
  `MoveScript_TintRecords` by the answer `Sprite_SetTint` stored, unchecked.
  Its `0xFF` (no record free) writes 0xFF records in.
- **Actor indices.** `Magic092_ChildTint` and `_ChildEnd` take the actor's
  index `+3` unchecked. So does `Magic088_Apply` with the variant's
  `0x65C39C` index, which is always 0..3 by `Magic088_Variant`.
- **Side and id.** `Magic088_Variant` rewrites the ability id `0x112` to
  `0x5A` when the side byte is not 4. That is a mutation of battle state by
  a presentation effect. Whether it is meant is not known.

## 8. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy has a `group S20` comment. Four of the 51 were
listed right already. Seven were listed with a host's larger extent and
are re-listed smaller: `004C3CC0`, `004C4430`, `004C4830`, `004C4980`,
`004C4C00`, `004C5B10` and `004C62A0`. The other 40 are new. The
consolidation keeps the smaller of duplicates.

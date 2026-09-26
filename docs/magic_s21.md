# Group S21: MAGIC093, 094, 095 (Inferno, Frost, Iceblast read one id down)

**Status:** IN PROGRESS (2026-09-26). 48 functions are ours
(`src/game/magic_s21.cpp`, shadow name `magic_s21`) and fuzzed headless
through the shared harness (`src/game/magic_s21_fuzz.cpp`): 0 mismatches.
152 negative controls: 150 are refused by a count (exit 3), and the 2 not refused are plants no fuzz can see (section 4). No recorded route casts any of these spells, so the
fuzz is the only check until the owner looks.

This is group S21 of round nine's first spell wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)). It
takes three whole overlays of `Magic_Rows`, found with `tools/magic_rows.py`:

| Unit | Row | File | Ability ids | Label (one id down) | Extent | Functions |
|---|--:|---|---|---|---|--:|
| MAGIC093 | 103 | `0x26E` | `0x5D`, `0xC3` | Inferno | `0x4C6300..0x4C742A` | 24 |
| MAGIC094 | 20 | `0x26F` | `0x5E`, `0xC4` | Frost | `0x4C7430..0x4C7CCE` | 11 |
| MAGIC095 | 22 | `0x270` | `0x5F`, `0xC5` | Iceblast | `0x4C7CD0..0x4C8D34` | 13 |

The labels are hypotheses ([`cut-content.md`](cut-content.md) §2). The
reading fits them: MAGIC093 draws flames and MAGIC094 / 095 tint the
target (6, 4, 8) and draw shards and spires. That fit is a guess about what
the effect looks like, not a measurement. What each spell does in the game
is for the owner to say.

No function was found inside or missing from the extents. Every function
the clone tables list is taken, and nothing in them was `REFUSED`.

## 1. What each function does

The functions are all battle-task steps (`void (void)`): `Sprite_Current` is
the slot being run and `0x93B940` is its owner. `+n` is a byte of the slot.

### MAGIC093: the flame effect (row 103)

| Function | Entry | Bytes | Reached as | Does |
|---|---|--:|---|---|
| `Inferno_Task` | `0x4C6300` | 0x75 | row 103 | runs its phase `+1` through a two-entry stack table (Start, End), then every live slot of `FlamePool` as the current slot, with that slot's `+0x80` as the owner |
| `Inferno_Start` | `0x4C6380` | 0x1B3 | entry 0 | clears the pool; moves the task to the targets' centre; creates one scorch child (kind 1, `0x4D`) per target not out; takes 8 flame columns and 16 sparks from the pool; makes CLUT strip `0x812980` semi-transparent; sound `0x100` |
| `Inferno_End` | `0x4C6540` | 0x20 | entry 1 | waits for `+0xB` (flames) and `+9` (scorches) to reach 0, then sets the done flag and frees the task |
| `FxScorch_Dispatch` | `0x4C6560` | 0x12 | kind 1, `0x4D` | `jmp [FxScorch_Phases + 4 * +1]` |
| `FxScorch_Run` | `0x4C6580` | 0x12 | `FxScorch_Phases[0]` | `jmp [FxScorch_Steps + 4 * +2]` |
| `FxScorch_Delay` | `0x4C65A0` | 0xAF | step 0 | counts `+9` down; at 0, tints its actor (-8, -8, -8, 1) and sets the actor's flags `0x10` |
| `FxScorch_Hit` | `0x4C6650` | 0x98 | step 1 | once the owner has fewer than 8 flames: flag `0x40`, tint released, actor flashed |
| `FxScorch_Free` | `0x4C66F0` | 0xD | step 2 | decrements the owner's `+9`; frees the task |
| `FlamePool_Dispatch` | `0x4C6700` | 0x12 | `Inferno_Task` | `jmp [FlamePool_Types + 4 * +1]` |
| `FlameColumn_Task` | `0x4C6720` | 0x45 | type 0 | runs its step by `+2`; `+0xB` up; draws the disc and the band under the actor's matrix |
| `FlameColumn_Start` | `0x4C6770` | 0xCC | column 0 | random radius and flicker; placed on a circle around the owner by `+4` |
| `FlameColumn_Grow` | `0x4C6840` | 0xAA | column 1 | widens the radius by `+9 * 8` over 16 frames |
| `FlameColumn_Hold` | `0x4C68F0` | 0x14 | column 2 | holds until the owner's `+0xB` is 8 |
| `FlameColumn_Fade` | `0x4C6910` | 0x27 | column 3 | counts `+9` down; then decrements the owner's `+0xB` and frees the slot |
| `FlameColumn_DrawDisc` | `0x4C6940` | 0x1C6 | from the column | a fan of 16 `POLY_G3` |
| `FlameColumn_DrawBand` | `0x4C6B10` | 0x26C | from the column | a band of 16 `POLY_G4` |
| `FlameSpark_Task` | `0x4C6D80` | 0x2E | type 1 | runs its step by `+2`; updates its screen point; draws |
| `FlameSpark_Start` | `0x4C6DB0` | 0x11D | spark 0 | after a delay, moves to the owner's position 0x20 out in a random eighth; plays sound `0x101..0x103` when `+0xB` is odd |
| `FlameSpark_Grow` | `0x4C6ED0` | 0x1D | spark 1 | `+9` up by 0x20 until 0xC0 |
| `FlameSpark_Stretch` | `0x4C6EF0` | 0x2A | spark 2 | `+9` up by 2; `+0xA` down to 0x30 |
| `FlameSpark_Fade` | `0x4C6F20` | 0x41 | spark 3 | fades `+0xB` to 0; then decrements the owner's `+0xB` and frees the slot |
| `FlameSpark_Draw` | `0x4C6F70` | 0x346 | from the spark | two `POLY_GT4`, a flame and its tip, at the screen point |
| `FlamePool_Alloc` | `0x4C72C0` | 0x57 | `Inferno_Start` | returns the first free pool slot in al, or `0xFF` when none is free |
| `Inferno_TargetCentre` | `0x4C7320` | 0x10B | `Inferno_Start` | the mean position of the live targets: all enemies if the target byte has bit `0x40`, else the party |

### MAGIC094: the frost effect (row 20)

| Function | Entry | Bytes | Reached as | Does |
|---|---|--:|---|---|
| `Frost_Task` | `0x4C7430` | 0x26 | row 20 | runs its phase `+1` through a two-entry stack table |
| `Frost_Start` | `0x4C7460` | 0xAC | entry 0 | takes the source's position and screen point; creates a ring child (kind 1, `0x15`); sound `0x100` |
| `Frost_Wait` | `0x4C7510` | 0x98 | entry 1 | follows the source; once the ring sets `+0xB` to `0xFF`: releases the target's tint, flashes it, sets flag `0x40` and the done flag, and frees the task |
| `FrostRing_Task` | `0x4C75B0` | 0x52 | kind 1, `0x15` | runs its phase through `FrostRing_Phases`; draws its shards and disc unless the owner is at `0xFF`; then always sets a draw mode and commits it to slot 3 |
| `FrostRing_Mark` | `0x4C7610` | 0x2D | phase 0 | sets the target's flags `0x20` |
| `FrostRing_Grow` | `0x4C7640` | 0x85 | phase 1 | over 16 frames, then tints the target (6, 4, 8, 0) |
| `FrostRing_Spin` | `0x4C76D0` | 0x38 | phase 2 | `+0xA` up to 0x10; 16 frames |
| `FrostRing_Fade` | `0x4C7710` | 0x28 | phase 3 | after 32 frames, sets the owner's `+0xB` to `0xFF`; frees the task |
| `FrostRing_DrawShards` | `0x4C7740` | 0x337 | from the ring | six rings of eight `POLY_G3`, their radius and height set by the phase |
| `FrostRing_PushMatrix` | `0x4C7A80` | 0xA7 | from the ring | `MagicFx_PushActorMatrix`'s shape, rotated by `+0xA << 4` |
| `FrostRing_DrawDisc` | `0x4C7B30` | 0x19F | from the ring | 32 `POLY_G3` at the owner's screen point |

### MAGIC095: the ice effect (row 22)

| Function | Entry | Bytes | Reached as | Does |
|---|---|--:|---|---|
| `Iceblast_Task` | `0x4C7CD0` | 0x48 | row 22 | runs its phase `+1` through a five-entry stack table under the actor's matrix |
| `Iceblast_Start` | `0x4C7D20` | 0x10B | entry 0 | copies the source; fills 56 jitter bytes from `Rand`; creates 12 shard children (kind 1, `0x17`) with delays 0x30 - 4i; sound `0x100` |
| `Iceblast_Chill` | `0x4C7E30` | 0x8A | entry 1 | draws the disc; at `+0xA` 0xC, tints the target and sets its flags `0x200` |
| `Iceblast_Rise` | `0x4C7EC0` | 0x51 | entry 2 | draws the spires and the disc; at `+9` 0x40, sounds `0x101` and `0x102` |
| `Iceblast_Sink` | `0x4C7F20` | 0x2C | entry 3 | draws the same; counts `+9` down |
| `Iceblast_End` | `0x4C7F50` | 0x85 | entry 4 | draws the disc; at `+0xA` 0: releases the tint, flashes the target, sets flag `0x40` and the done flag, frees the task |
| `Iceblast_DrawSpires` | `0x4C7FE0` | 0x3F4 | Rise, Sink | six rows of eight spires (0x5A76F0's code `0x5C`) with jittered radii |
| `Iceblast_DrawSpiresInner` | `0x4C83E0` | 0x3F3 | Rise, Sink | the same rows as `POLY_G4`, shaded by row |
| `Iceblast_DrawDisc` | `0x4C87E0` | 0x190 | four phases | 16 `POLY_G3` of radius 0x1C0, shaded `+0xA * 8` |
| `IceShard_Task` | `0x4C8970` | 0x1C | kind 1, `0x17` | runs its phase through `IceShard_Phases` under the actor's matrix |
| `IceShard_Wait` | `0x4C8990` | 0x28 | phase 0 | waits for the owner's phase 3 (Sink), then its own delay |
| `IceShard_Run` | `0x4C89C0` | 0x23 | phase 1 | draws; frees after 32 frames |
| `IceShard_Draw` | `0x4C89F0` | 0x345 | from the shard | 32 flat triangles swirling up from a ring of the jitter bytes |

`symbols.toml` has each function's evidence. The data is named there too:
the seven `.data` handler tables `FxScorch_Phases` `0x65B604` (1 entry),
`FxScorch_Steps` `0x65B608` (3), `FlamePool_Types` `0x65B614` (2),
`FlameColumn_Phases` `0x65B61C` (4), `FlameSpark_Phases` `0x65B62C` (4),
`FrostRing_Phases` `0x65B63C` (4) and `IceShard_Phases` `0x65B64C` (2);
`FlamePool` `0x6948D8` (32 records of 0x84); and `Iceblast_Jitter`
`0x695958` (56 signed bytes, right after the pool). The kind-1 children and
the pool's phases are reached from MAGIC088, 091 and 092 too (they create
the same kinds), so taking them here takes them for those overlays as well.

**No divergence** and no ledger entry: each function is a faithful
replacement, with two exceptions. Ours aborts where a stack-table phase runs
past the table: `Inferno_Task` past 1, `Frost_Task` past 1,
`Iceblast_Task` past 4. This follows the precedent in
[`magic_fx_reached.md`](magic_fx_reached.md) §3. Ours also aborts where
`Inferno_TargetCentre` would divide by zero (section 5). The `.data` tables
are read in place and unchecked, exactly as the originals read them.
Nothing in `DIVERGENCE.md` or `cheats.cpp` patches bytes in the band
`0x4C6300..0x4C8D34`.

## 2. Calls across groups

Every call outside the group goes through the harness: named callees by
name, and these three by raw address (never bound):

| Address | Owner | What |
|---|---|---|
| `0x4F6290` | MAGIC219, group S37 (shared by 35 overlays) | a pool slot's free: clears `+0..+4` of `Sprite_Current` |
| `0x5A76F0` | outside the band, no group | a primitive setter: code `0x5C`, the four z floats at `+0x10..+0x40` |
| `0x5A7570` | outside the band, no group | `POLY_F3`'s setter: code `0x20`, the three z at `+0x10`, `+0x1C`, `+0x28` |

## 3. The fuzz

`BOF3X_SHADOW=magic_s21` runs through `magic_harness::Run`. It builds 48
copies with every call re-aimed at a recorder, re-aims the three stack
tables' immediates, and swaps the seven `.data` tables for recorders. The
effects on the callees (`magic_s21_fuzz.cpp`) do three things:

- The GTE projections log the vertices they read and where their outputs
  lie, and fill the screen points and the depth.
- The matrix calls log their vectors, their angles and the MATRIX layout (the
  translation at `+0x14` of the block).
- `MapView_LinkPrimAt` and `Gfx_CommitPrim` move the primitive cursor half
  the time. `Battle_ActorIsOut` answers "out" half the time, but never for
  the last actor of either side, so the fuzz never divides by zero.

The group's regions are the pool and the jitter bytes, the vertex scratch,
the scratchpad copy, the CLUT strip and its buffer, `Gfx_PacketNext`, and a
0x1000-byte primitive buffer of the fuzz's own. Its disturbance moves the
primitive cursor, a scratch or vertex word, a jitter byte, or a pool byte.
`phase_span` is 2.

The seed sets the following in every round:

- the cursor points into the buffer, and every pool slot's owner is a real
  record;
- each phase byte is inside its table (`IceShard_Task`'s two entries for all
  four task slots);
- the target byte has bit `0x40` half the time, where a function reads it;
- each counter sits at its threshold or one either side, and the draws' `+9`
  is small.

Result (2026-09-26, in this worktree, after the merge of the consolidated
harness `ea27991`):

    shadow      magic_s21 self-test: 96000 rounds over 48 functions (2000 each), 6210419 calls to the stand-ins,
                0 MISMATCHES; 20820 bytes of state (15 regions) and the stand-ins' log compared

The originals called every recorder and every handler of every table.
`BOF3X_SHADOW='*'` exits 0 with 0 mismatches everywhere. The call counts
depend on the build directory (see `magic_harness.md`).

## 4. The controls

152 negative controls were planted one at a time by a script, which is not committed. For each one it applies the plant, builds, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s21` and restores the source; the source was checked clean afterwards. They ran on the ported fuzz (the consolidated harness) on 2026-09-26. 150 are refused by a count (exit 3), each only in the function it touches, or in both functions of a shared body (the CP and SP rows). 2 are not refused.

A4 and KD4 were only refused after the seed gained two cases: every pool slot in use but the last, and a shard height of exactly 0 with `+9` non-zero. On the first run A4 was refused in 1 round and KD4 in none. Their counts here are from the rerun. Every other count is from the run before the seed change, which touched only the seeds of `FlamePool_Alloc` and `IceShard_Draw`.

| | Function | Planted | Refused in (rounds of 2,000) |
|---|---|---|---|
| T1 | `Inferno_Task` | table entries swapped | Inferno_Task 2,000 |
| T2 | `Inferno_Task` | owner not put back | Inferno_Task 1,621 |
| T3 | `Inferno_Task` | slot live by bit 1 | Inferno_Task 1,968 |
| T4 | `Inferno_Task` | task read before the phase | Inferno_Task 54 |
| S1 | `Inferno_Start` | pool +2 not cleared | Inferno_Start 2,000 |
| S2 | `Inferno_Start` | scorch parameter 0x4E | Inferno_Start 2,000 |
| S3 | `Inferno_Start` | scorch +9 0x11 | Inferno_Start 1,998 |
| S4 | `Inferno_Start` | enemy index without +3 | Inferno_Start 964 |
| S5 | `Inferno_Start` | column +4 i + 1 | Inferno_Start 2,000 |
| S6 | `Inferno_Start` | spark delay (i + 2) << 3 | Inferno_Start 2,000 |
| S7 | `Inferno_Start` | CLUT bit 14 | Inferno_Start 2,000 |
| S8 | `Inferno_Start` | strip not marked dirty | Inferno_Start 2,000 |
| S9 | `Inferno_Start` | sound 0x101 | Inferno_Start 2,000 |
| S10 | `Inferno_Start` | eight scorches over the party | Inferno_Start 1,036 |
| E1 | `Inferno_End` | done flag bit 1 | Inferno_End 647 |
| E2 | `Inferno_End` | +9 not waited for | Inferno_End 466 |
| A1 | `FlamePool_Alloc` | answer index + 1 | FlamePool_Alloc 1,530 |
| A2 | `FlamePool_Alloc` | full answers 0xFE | FlamePool_Alloc 470 |
| A3 | `FlamePool_Alloc` | marks bit 1 | FlamePool_Alloc 1,530 |
| A4 | `FlamePool_Alloc` | 31 slots | FlamePool_Alloc 230 |
| C1 | `Inferno_TargetCentre` | height from +0x3C | Inferno_TargetCentre 2,000 |
| C2 | `Inferno_TargetCentre` | x sar 8 | Inferno_TargetCentre 2,000 |
| C3 | `Inferno_TargetCentre` | z bias 0x3FFF | Inferno_TargetCentre 2,000 |
| C4 | `Inferno_TargetCentre` | party index + 1 | Inferno_TargetCentre 1,028 |
| D1 | `FxScorch_Dispatch` | by +2 | FxScorch_Dispatch 1,355 |
| D2 | `FxScorch_Run` | by +1 | FxScorch_Run 1,380 |
| SD1 | `FxScorch_Delay` | enemy tint 0xF9 | FxScorch_Delay 668 |
| SD2 | `FxScorch_Delay` | enemy flags 0x20 | FxScorch_Delay 668 |
| SD3 | `FxScorch_Delay` | party record + 1 | FxScorch_Delay 667 |
| SD4 | `FxScorch_Delay` | party tint alpha 0 | FxScorch_Delay 667 |
| SD5 | `FxScorch_Delay` | delay not stepped past 0 | FxScorch_Delay 1,664 |
| H1 | `FxScorch_Hit` | waits while above 8 | FxScorch_Hit 1,334 |
| H2 | `FxScorch_Hit` | enemy flash without + 3 | FxScorch_Hit 161 |
| H3 | `FxScorch_Hit` | index kept for the release | FxScorch_Hit 4 |
| F1 | `FxScorch_Free` | owner +0xB | FxScorch_Free 2,000 |
| P1 | `FlamePool_Dispatch` | by +2 | FlamePool_Dispatch 1,327 |
| CT1 | `FlameColumn_Task` | gate on +1 | FlameColumn_Task 637 |
| CT2 | `FlameColumn_Task` | +0xA stepped | FlameColumn_Task 2,000 |
| CT3 | `FlameColumn_Task` | band before disc | FlameColumn_Task 1,108 |
| CS1 | `FlameColumn_Start` | Rand & 7 | FlameColumn_Start 999 |
| CS2 | `FlameColumn_Start` | radius 0xC1 | FlameColumn_Start 1,998 |
| CS3 | `FlameColumn_Start` | height from the owner +0x38 | FlameColumn_Start 2,000 |
| CS4 | `FlameColumn_Start` | +9 left | FlameColumn_Start 1,990 |
| CP1 | `FlameColumn_Start/_Grow` | x sar 2 | FlameColumn_Start 2,000, FlameColumn_Grow 2,000 |
| CP2 | `FlameColumn_Start/_Grow` | angle & 3 | FlameColumn_Start 997, FlameColumn_Grow 974 |
| CP3 | `FlameColumn_Start/_Grow` | owner read before the sine | FlameColumn_Start 73, FlameColumn_Grow 88 |
| CG1 | `FlameColumn_Grow` | +9 * 4 | FlameColumn_Grow 1,999 |
| CG2 | `FlameColumn_Grow` | to 0x11 | FlameColumn_Grow 1,523 |
| CH1 | `FlameColumn_Hold` | at 7 | FlameColumn_Hold 1,660 |
| CF1 | `FlameColumn_Fade` | owner +9 | FlameColumn_Fade 1,304 |
| DD1 | `FlameColumn_DrawDisc` | green b * 8 | FlameColumn_DrawDisc 1,992 |
| DD2 | `FlameColumn_DrawDisc` | radius base 0x101 | FlameColumn_DrawDisc 1,993 |
| DD3 | `FlameColumn_DrawDisc` | step 0x80 | FlameColumn_DrawDisc 2,000 |
| DD4 | `FlameColumn_DrawDisc` | rim blue b * 4 | FlameColumn_DrawDisc 1,983 |
| DD5 | `FlameColumn_DrawDisc` | flicker & 0xF | FlameColumn_DrawDisc 987 |
| DB1 | `FlameColumn_DrawBand` | inner base 0x100 | FlameColumn_DrawBand 1,996 |
| DB2 | `FlameColumn_DrawBand` | B0 / B8 swapped | FlameColumn_DrawBand 2,000 |
| DB3 | `FlameColumn_DrawBand` | green b << 3 | FlameColumn_DrawBand 1,969 |
| DB4 | `FlameColumn_DrawBand` | flicker read before the sine | FlameColumn_DrawBand 79 |
| ST1 | `FlameSpark_Task` | no screen point | FlameSpark_Task 1,131 |
| SS1 | `FlameSpark_Start` | sounds swapped | FlameSpark_Start 425 |
| SS2 | `FlameSpark_Start` | +0xA 0x41 | FlameSpark_Start 1,297 |
| SS3 | `FlameSpark_Start` | z sar 4 | FlameSpark_Start 1,296 |
| SS4 | `FlameSpark_Start` | even tones sound | FlameSpark_Start 1,297 |
| SS5 | `FlameSpark_Start` | x pointer re-read after the sine | FlameSpark_Start 45 |
| SG1 | `FlameSpark_Grow` | by 0x1F | FlameSpark_Grow 2,000 |
| SR1 | `FlameSpark_Stretch` | to 0x2F | FlameSpark_Stretch 1,642 |
| SF1 | `FlameSpark_Fade` | +9 not widened | FlameSpark_Fade 1,997 |
| SW1 | `FlameSpark_Draw` | tip 15 up | FlameSpark_Draw 2,000 |
| SW2 | `FlameSpark_Draw` | shade << 2 | FlameSpark_Draw 1,974 |
| SW3 | `FlameSpark_Draw` | v 0xFE | FlameSpark_Draw 1,056 |
| SW4 | `FlameSpark_Draw` | mode 0xB4 | FlameSpark_Draw 2,000 |
| SW5 | `FlameSpark_Draw` | second quad on the first | FlameSpark_Draw 1,070 |
| SW6 | `FlameSpark_Draw` | point cached, not the scratch | FlameSpark_Draw 10 |
| FT1 | `Frost_Task` | table swapped | Frost_Task 2,000 |
| FS1 | `Frost_Start` | y 15 up | Frost_Start 1,984 |
| FS2 | `Frost_Start` | child actor byte +9 | Frost_Start 1,996 |
| FS3 | `Frost_Start` | parameter 0x16 | Frost_Start 2,000 |
| FS4 | `Frost_Start` | source re-read after the create | Frost_Start 35 |
| FW1 | `Frost_Wait` | at 0xFE | Frost_Wait 1,040 |
| FW2 | `Frost_Wait` | no done flag | Frost_Wait 491 |
| RT1 | `FrostRing_Task` | draws at 0xFF | FrostRing_Task 2,000 |
| RT2 | `FrostRing_Task` | commit slot 2 | FrostRing_Task 2,000 |
| RM1 | `FrostRing_Mark` | flags 0x40 | FrostRing_Mark 2,000 |
| RG1 | `FrostRing_Grow` | tint alpha 1 | FrostRing_Grow 1,362 |
| RS1 | `FrostRing_Spin` | up to 0x11 | FrostRing_Spin 1,331 |
| RS2 | `FrostRing_Spin` | not read again after +0xA | **not refused** (see below) |
| RF1 | `FrostRing_Fade` | at 0x1F | FrostRing_Fade 1,647 |
| SH1 | `FrostRing_DrawShards` | phase 2 height x 81 | FrostRing_DrawShards 1,205 |
| SH2 | `FrostRing_DrawShards` | phase 3 rim + +9 | FrostRing_DrawShards 376 |
| SH3 | `FrostRing_DrawShards` | else height x -4 | FrostRing_DrawShards 1,974 |
| SH4 | `FrostRing_DrawShards` | blue 0xC0 - .. | FrostRing_DrawShards 302 |
| SH5 | `FrostRing_DrawShards` | 1 at zero too | FrostRing_DrawShards 4 |
| SH6 | `FrostRing_DrawShards` | height word read before the sine | FrostRing_DrawShards 88 |
| SH7 | `FrostRing_DrawShards` | five rings | FrostRing_DrawShards 2,000 |
| SH8 | `FrostRing_DrawShards` | r16 / 8 | FrostRing_DrawShards 82 |
| PM1 | `FrostRing_PushMatrix` | turn << 3 | FrostRing_PushMatrix 1,986 |
| PM2 | `FrostRing_PushMatrix` | height not negated | FrostRing_PushMatrix 2,000 |
| PM3 | `FrostRing_PushMatrix` | translation before the rotation | FrostRing_PushMatrix 2,000 |
| PM4 | `FrostRing_PushMatrix` | z bias 0x3FFF | FrostRing_PushMatrix 2,000 |
| FD1 | `FrostRing_DrawDisc` | phase 2 0x41 | FrostRing_DrawDisc 417 |
| FD2 | `FrostRing_DrawDisc` | phase 3 x 3 | FrostRing_DrawDisc 384 |
| FD3 | `FrostRing_DrawDisc` | centre blue 0xC1 | FrostRing_DrawDisc 2,000 |
| FD4 | `FrostRing_DrawDisc` | owner x cached | FrostRing_DrawDisc 1,999 |
| FD5 | `FrostRing_DrawDisc` | else + 0x11 | FrostRing_DrawDisc 1,155 |
| IT1 | `Iceblast_Task` | Rise and Sink swapped | Iceblast_Task 784 |
| IT2 | `Iceblast_Task` | no pop | Iceblast_Task 2,000 |
| IS1 | `Iceblast_Start` | jitter & 7 | Iceblast_Start 2,000 |
| IS2 | `Iceblast_Start` | delay 0x31 - 4 i | Iceblast_Start 2,000 |
| IS3 | `Iceblast_Start` | parameter 0x18 | Iceblast_Start 2,000 |
| IS4 | `Iceblast_Start` | child position from the source | Iceblast_Start 1,781 |
| IS5 | `Iceblast_Start` | y 16 up (Frost_Start's) | Iceblast_Start 1,575 |
| IS6 | `Iceblast_Start` | odd Rand: positive | Iceblast_Start 2,000 |
| IC1 | `Iceblast_Chill` | at 0xD | Iceblast_Chill 783 |
| IC2 | `Iceblast_Chill` | flags 0x100 | Iceblast_Chill 395 |
| IC3 | `Iceblast_Chill` | read again always | **not refused** (see below) |
| IR1 | `Iceblast_Rise` | at 0x3F | Iceblast_Rise 1,456 |
| IR2 | `Iceblast_Rise` | +9 0x2F | Iceblast_Rise 1,172 |
| IR3 | `Iceblast_Rise` | second sound 0x101 | Iceblast_Rise 1,172 |
| IK1 | `Iceblast_Sink` | no disc | Iceblast_Sink 2,000 |
| IE1 | `Iceblast_End` | done flag bit 3 | Iceblast_End 928 |
| SP1 | `Iceblast_DrawSpires/_Inner` | row step 40 | Iceblast_DrawSpires 2,000, Iceblast_DrawSpiresInner 2,000 |
| SP2 | `Iceblast_DrawSpires` | shade x 5 | Iceblast_DrawSpires 780 |
| SP3 | `Iceblast_DrawSpires` | at most 0x31 | Iceblast_DrawSpires 2,000 |
| SP4 | `Iceblast_DrawSpires` | odd and even swapped | Iceblast_DrawSpires 2,000 |
| SP5 | `Iceblast_DrawSpires/_Inner` | row 4 closed | Iceblast_DrawSpires 2,000, Iceblast_DrawSpiresInner 2,000 |
| SP6 | `Iceblast_DrawSpires` | far edge unswapped | Iceblast_DrawSpires 2,000 |
| SP7 | `Iceblast_DrawSpires/_Inner` | jitter row 8 on read once | Iceblast_DrawSpires 2,000, Iceblast_DrawSpiresInner 2,000 |
| SP8 | `Iceblast_DrawSpires` | shade 7 i -> 6 i | Iceblast_DrawSpires 749 |
| SP9 | `Iceblast_DrawSpires/_Inner` | jitter j1 read before the cosine | Iceblast_DrawSpires 14, Iceblast_DrawSpiresInner 13 |
| SI1 | `Iceblast_DrawSpiresInner` | below 4 | Iceblast_DrawSpiresInner 99 |
| SI2 | `Iceblast_DrawSpiresInner` | 3 d - 11 | Iceblast_DrawSpiresInner 270 |
| SI3 | `Iceblast_DrawSpiresInner` | d = +9 - 7 i | Iceblast_DrawSpiresInner 791 |
| SI4 | `Iceblast_DrawSpiresInner` | unswapped edge B8 = old A0 | Iceblast_DrawSpiresInner 2,000 |
| ID1 | `Iceblast_DrawDisc` | radius 0x1C1 | Iceblast_DrawDisc 1,998 |
| ID2 | `Iceblast_DrawDisc` | grey << 2 | Iceblast_DrawDisc 2,000 |
| ID3 | `Iceblast_DrawDisc` | mode at the cursor read before the cosine | Iceblast_DrawDisc 269 |
| ID4 | `Iceblast_DrawDisc` | link slot 1 | Iceblast_DrawDisc 2,000 |
| KT1 | `IceShard_Task` | no pop | IceShard_Task 2,000 |
| KW1 | `IceShard_Wait` | owner phase 2 | IceShard_Wait 1,150 |
| KR1 | `IceShard_Run` | at 0x1F | IceShard_Run 1,606 |
| KD1 | `IceShard_Draw` | corner radius 0x20 - +9 | IceShard_Draw 2,000 |
| KD2 | `IceShard_Draw` | odd rows 0x23 higher | IceShard_Draw 1,667 |
| KD3 | `IceShard_Draw` | jitter h + k + 1 | IceShard_Draw 1,638 |
| KD4 | `IceShard_Draw` | clamp at 0 too | IceShard_Draw 39 |
| KD5 | `IceShard_Draw` | latch +9 + 1 | IceShard_Draw 475 |
| KD6 | `IceShard_Draw` | blue Rand & 0x3F | IceShard_Draw 1,998 |
| KD7 | `IceShard_Draw` | column (k / 4) & 7 | IceShard_Draw 1,341 |
| KD8 | `IceShard_Draw` | first corner + 0x11 | IceShard_Draw 2,000 |
| KD9 | `IceShard_Draw` | radius by +0xA x 2 | IceShard_Draw 2,000 |
| KD10 | `IceShard_Draw` | the bit tested on the task read at the top | IceShard_Draw 1,752 |
| KD11 | `IceShard_Draw` | h recomputed each column | IceShard_Draw 1,966 |

**Not refused, and why: no fuzz can see them.** Each plant moves a read of `Sprite_Current` to a stretch with no call in it, where nothing can move the pointer:

- RS2 (`FrostRing_Spin`): ours reads the task again after bumping `+0xA`, as the original does. The plant drops that second read.
- IC3 (`Iceblast_Chill`): the plant reads the task again on the path without the tint, where the original keeps its first read.

Ours is faithful in both places as written. The thinnest refusals are H3 (4 rounds), SH5 (4) and SW6 (10). The re-read plants are thin by nature: SP9 (13 / 14), FS4, SS5, CP3, DB4, SH6 and SH8. Each shows only when a recorder's disturbance moves the cell during the one call between the two reads.

## 5. Defects (Capcom's, latent, kept)

These are described here, not numbered. The coordinator numbers them.

- **`Inferno_TargetCentre` divides by the number of live targets**, with no
  check. With every target out, the original faults (#DE) and the game
  ends. Whether a cast can meet this (all enemies dead when the effect
  starts) was not measured. Ours aborts with a `Fatal`.
- **`Inferno_Start` uses `FlamePool_Alloc`'s answer unchecked.** With the 32
  slots in use, the answer is `0xFF` and the writes land at `0x6948D8 + 0xFF
  * 0x84`, past the pool. The start clears the pool first, so only a second
  flame effect running at the same time could fill it.
- **Three unchecked stack tables** (`Inferno_Task`, `Frost_Task`,
  `Iceblast_Task`) and seven `.data` tables read in place without a bound.
  The same class as D59 in [`known-defects.md`](known-defects.md). In every
  path read here, the phases stay inside their tables.
- **`IceShard_Draw` indexes the jitter bytes by `+0xB >> 1`** (and
  `+ k` up to 32) without a bound. The twelve shards `Iceblast_Start`
  creates stay inside the 56 bytes.

## 6. What the route reaches

Nothing. No recorded route casts Inferno, Frost or Iceblast
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §5).
The live check is the owner's: cast each spell with a save that has it or
with DIV-0045's cheat.

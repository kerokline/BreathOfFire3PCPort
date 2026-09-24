# The battle's item effects, the battle actor's helpers, and the setters under them

**Status:** IN PROGRESS (2026-09-23) - twenty-four functions ours
(`src/game/battle_items.cpp`, shadow name `battle_items`), each fuzzed
headless against a copy of Capcom's with every call re-aimed at a recorder;
72 negative controls, 71 refused by a count and one by a fault (with a counting twin). **Not yet through the live check** - the combat A/B runs
centrally after the merge (section 7).

Group BH of the seventh round ([`takeover-queue-round7.md`](takeover-queue-round7.md)).
The queue called it "the battle item menu"; read, none of it is a menu. It is
the **effect side of using an item in battle**: the Healing Herb's sparkles
(the PSX's `MAGIC070.EMI`, "Battle FX: Healing Herb, Rejuvenate" in the
sibling's `names/overlays.toml`), three effect draws that several `BMAGIC`
overlays share, the matrix they are drawn under, the battle actor's small
helpers from `BATTLE.EMI` section 15, the four libgpu setters those draws
use, and the one-shot `SND` stream's play and stop. The file keeps the
queue's name. No divergence: every function is a faithful replacement, and no
`DIVERGENCE.md` entry is owed.

**No function here draws text.** The item list's `物品` header is not drawn
by any of them (nor by anything they call); it belongs to the battle windows
(groups BC / BD) - [`HANDOFF.md`](HANDOFF.md) item 000 has the address.

## 1. The corrected extents

`pe_funcs.py` recorded `0x4B8FE0` as 0x315 bytes (`entries_logic.txt`:
`004B8FE0 315`). By disassembly (capstone, ret to ret, every jump checked
internal) the range holds six functions, five of them pointer-reached:

| Entry | Bytes | What |
|---|--:|---|
| `0x4B8FE0` | 0x12 | `Sparkle_Dispatch`, ours |
| `0x4B9000` | 0x8F | the sparkle's type-0 update - **left Capcom's**, section 3 |
| `0x4B9090` | 0x161 | `Sparkle_Launch`, ours (0x4B9000's stack table, entry 0) |
| `0x4B9200` | 0x6C | `Sparkle_Rise`, ours (entry 1) |
| `0x4B9270` | 0x85 | `Sparkle_Fade`, ours (entry 2) |

and `0x4B9900` (0x2F, `Sparkle_Free`, reached only by `Sparkle_Fade`'s tail
jmp) follows `Sparkle_Alloc`. These four were not in the queue; they are
taken here because they are the sparkles' own phases, reached by the combat
route (every one of `0x4B9000`'s 2,554 calls runs one of them), and in no
other group's row. For `analysis/calltrace/entries_logic.txt`: `004B8FE0 12`
in place of `004B8FE0 315`, and add `004B9090 161`, `004B9200 6C`,
`004B9270 85`, `004B9900 2F` (and `004B9000 8F`, Capcom's, so that its calls
are no longer charged to `0x4B8FE0`). Every other size in section 2 is the
queue's.

Also in this neighbourhood and not taken (not in the round, caller side):
the sparkle pool's walk `0x4B8D70` (a six-entry stack table: `0x4B8E00`
spawn, `0x4B1E70`, `0x4B1ED0`, `0x4EE8A0`, `0x4B8F50` end, `0x4F7350`), the
spawner `0x4B8E00` and the end step `0x4B8F50` - MAGIC070's entry points.

## 2. What is ours

| Function | Entry | Bytes | PSX twin | Does |
|---|---|--:|---|---|
| `Gpu_SetPolyG3` | `0x5A75F0` | 0x17 | `SetPolyG3` `0x8017B2F4` | code `0x30`, 0.01 at +0x10 / +0x20 / +0x30 |
| `Gpu_SetLineG2` | `0x5A76B0` | 0x14 | `SetLineG2` `0x8017B40C` | code `0x50`, 0.01 at +0x10 / +0x20 |
| `Gpu_SetLineG3` | `0x5A76D0` | 0x17 | `SetLineG3` `0x8017B440` | code `0x58`, 0.01 at +0x10 / +0x20 / +0x30; no `0x55555555` terminator (the PSX's has one) |
| `Gpu_SetTile1` | `0x5A7750` | 0x10 | `SetTile1` `0x8017B3A8` | code `0x68`, 0.01 at +0x10 |
| `Sparkle_Dispatch` | `0x4B8FE0` | 0x12 | (MAGIC070) | `jmp [0x65AE28 + 4 * type]` |
| `Sparkle_Launch` | `0x4B9090` | 0x161 | MAGIC070 `0x801EF47C` | section 3 |
| `Sparkle_Rise` | `0x4B9200` | 0x6C | MAGIC070 `0x801EF67C` | sway; count up to the rise length, then phase 2 |
| `Sparkle_Fade` | `0x4B9270` | 0x85 | MAGIC070 `0x801EF72C` | sway; fade out; free |
| `Sparkle_DrawRaysG2` | `0x4B9300` | 0x185 | MAGIC070 `0x801EF810` | four two-point rays |
| `Sparkle_DrawRaysG3` | `0x4B9490` | 0x1ED | MAGIC070 `0x801EFA44` | the same rays, three points each |
| `Sparkle_DrawDisc` | `0x4B9680` | 0x22F | MAGIC070 `0x801EFCDC` | the sparkle's body: eight triangles |
| `Sparkle_Alloc` | `0x4B98B0` | 0x4A | MAGIC070 `0x801F0000` | first free of 128 records |
| `Sparkle_Free` | `0x4B9900` | 0x2F | MAGIC070 `0x801F006C` | bytes +0..+4 cleared |
| `MagicFx_DrawFan` | `0x4AD6F0` | 0x1A3 | Sacrifice / Fireblast `0x801EFA20`.. (shape) | eight-triangle fan, radius 0x120 |
| `MagicFx_PushActorMatrix` | `0x4B7D40` | 0x9F | Sacrifice `0x801EF700` | Camera x actor translation, pushed |
| `MagicFx_DrawRing` | `0x4C5150` | 0x39C | Sacrifice `0x801EF240`.. | a textured band of 32 quads |
| `MagicFx_DrawDisc` | `0x4C54F0` | 0x186 | Sacrifice `0x801EF7B0` | eight triangles round the screen point |
| `BattleActor_SetAnimation` | `0x4FB830` | 0x4C | BATTLE `0x800ABEC0` | party: `Sprite_SetAnimation`; enemy: `0x435A20` |
| `BattleActor_UpdateScreenXY` | `0x4FBD10` | 0x95 | BATTLE `0x800AC68C` | the sprite's projected point to +0x2E / +0x30 |
| `BattleActor_Flash` | `0x4FBDB0` | 0x7F | BATTLE `0x800AC73C` | tint (-6, -10, 0, 0) when flagged |
| `BattleActor_PlaySound` | `0x4FC030` | 0xAC | BATTLE `0x800ACBC0` | the acting actor's sounds |
| `BattleActor_FxSize` | `0x4FC1F0` | 0x6B | unpaired | the effect size callers store to +9 |
| `SndStream_Play` | `0x5A7140` | 0x7C | none (the PSX streams from CD) | play a WAV image once |
| `SndStream_Stop` | `0x5A71C0` | 0x3E | none | stop and release it |

`symbols.toml` has each one's evidence. Twins: the setters by the sibling's
Psy-Q signature matches and the code byte; `MAGIC070`'s five by the call
order (`SetLineG2`, `SetLineG3`, `SetPolyG3` at `0x801EF948` / `0x801EFB80` /
`0x801EFE7C`, as the PC's three draws) and read side by side for the three
phases, the alloc and the free; the effect draws by scanning the overlay
captures for `SetPolyG3` callers and the constant 0x120 (Sacrifice, `MAGIC
6bdc2d45`, has all four, and Fireblast `04b565fa` the fan); the actor helpers
by `analysis/pairs_propagated.json` (call-anchored in BATTLE.EMI section 15,
`4065db04`). Differences from the PSX, all the port's and kept: the PSX
picks the draw mode's tpage by `GetGraphType` (0x55 / 0x125, 0x35 / 0xA5,
0x15 / 0x25), the port uses fixed ones - and `MagicFx_DrawRing` 0x35 where
the PSX has 0x55 / 0x125; the port's primitives are wider (floats, a depth
per vertex); the PSX's `MulMatrix2(camera, m)` is the port's
`Gte_MulMatrix0(camera, m, m)`.

Every callee is typed in `battle_items_callees.h`. Five belong to other
groups this round or to nobody and are called by raw address, never bound:
`0x572FA0` (a depth-sorted `Gfx_CommitPrim(x, z, slot, size)`, the PSX
`0x801564C4`, unnamed), `0x4456C0` (BB: "actor out" - bit 0 of +0 clear or
bit 6 of +0x91 set), `0x435A20` (an enemy's animation, via BB's
`0x4358D0`), `0x446A50` (BF: a sound from `Field_State` +0x2C's table,
index masked to a byte), `0x437450` (`Sound_PlayEffect` unless the id is
`0xFFFF`), `Sprite_SetTint` `0x454CC0` and `Sound_PlayById` `0x587900` (BG's).

## 3. The sparkles

A sparkle is a 0x2C-byte record of the pool at `0x684790` (128 of them);
`0x4B8D70` walks the pool every frame, pointing `0x685D90` at each one in use
and `0x93B940` - the battle actor the effect plays on - at its +0x28, and
calls `Sparkle_Dispatch`. Fields: +0 bit 0 in use, +1 type (0 always), +2
phase, +3 / +4 colour row and kind, +5 delay then brightness, +6 count, +7
the offset row then the rise length, +8 base x, +0xA drift, +0xC the sway's
phase, +0x14..+0x1C the actor's position, +0x20 / +0x22 the screen point.

`0x4B9000` (Capcom's) calls the phase handler through a three-entry table
**on its own stack** (`[esp + 4 * phase]`), then while the sparkle lives
draws its disc, and on frames where +0xC and 3 is non-zero its two rays.
Launch waits out the delay, takes the actor's position and screen point, is
thrown by the offset row of `0x65AD34` (x pushed 0..7 further from zero, y up
by dy + 0..7) and gets a random sway, drift and rise length; Rise sways
(`x = base + sin(((+0xC) and 0x3F) << 6) * 24 >> 12`) and climbs; Fade sways,
dims one step a frame, and frees itself, decrementing the actor's +0xB.

Kept as Capcom's has them, each written in the code where it is kept:

- `Sparkle_Launch` takes the x pointer before its `Rand` and reads the
  sparkle again after every call; the fuzz moves the current sparkle
  between calls to hold ours to it (control C9).
- The draws read their two arguments as low words: their caller pushes
  stale upper halves.

**Left Capcom's: `0x4B9000`.** Its table is unbounded - a phase of 3 calls
the dword above the table, which is the function's own return address (into
`0x4B8D70`), 4 and on the caller's stack. Phases stay 0..2 (Launch and Rise
step them, Free clears them), so it is latent; but ours could not reproduce
what a phase of 3 does, and a check would be a divergence - the reason
`MsgBox_SystemChoice` stayed Capcom's (D22). Its callees are ours through
their patched entries. Proposed for [`known-defects.md`](known-defects.md)
(section 8).

## 4. The shared effect draws and the matrix

`MagicFx_PushActorMatrix` pushes the GTE matrix and loads Camera x (the
actor's translation, no rotation): the SVECTOR (x >> 9 - 0x4000, z >> 9 -
0x4000, -(height / 2)) through `Gte_RotTrans` straight into the translation
of a `MATRIX` on its stack - +0x14 of the block `Gte_RotMatrix` and
`Gte_MulMatrix0` then fill. Ours lays the block out the same way
(`static_assert`), and the fuzz's stand-ins record where each block lies
relative to the one before, so a translation kept apart is refused (C37).
It pops nothing; its 74 callers pop.

The three draws (their callers `0x4AD024..0x4AD02E` and `0x4C5004..0x4C5013`
are two overlays' copies of one effect, folded by the linker into shared
bodies, as `ItemUse_HealAp5` was) are straight PSX transcriptions with the
port's floats; section 2 has what each draws. `MagicFx_DrawRing` sorts each
quad at the actor's position plus its inner x << 9 **on both axes** - the
PSX does the same.

## 5. The battle actor's helpers and the SND stream

`0x904B34` is the acting actor: 0..2 a party member (record `0x802D40` +
0x14C i, `ObjTrio`), 3 and up an enemy (`0x93B960` + 0x128 (i - 3)).

- `BattleActor_SetAnimation(offset, arg)` makes the effect's actor
  `Sprite_Current` for one call. As the original has it, the party's
  animation byte travels in the low byte of the actor's own pointer, pushed
  whole (ours builds it in inline assembly, the clang trap of
  [`event-ops.md`](event-ops.md) section 8; control C49 refuses the byte
  alone), and the enemy's index in the low byte of the caller's `ecx` -
  `0x435A20` reads only that byte.
- `BattleActor_UpdateScreenXY` projects into a `TILE_1` at `Gfx_PacketNext`
  that is never committed - the primitive is scratch for its vertex - and
  truncates the float x / y through the CRT's `_ftol` (a NaN or an
  out-of-range float gives 0, as `field_frame.cpp`'s `Ftol16`).
- `BattleActor_PlaySound(first, second)`: a party member becomes
  `Field_State` and plays `first + 1` and `second` through `0x446A50`; an
  enemy plays its own cue or `0x600 + 2 * type`.
- `BattleActor_FxSize` answers al only; all ten callers store al.
- `SndStream_Play` / `SndStream_Stop` work on the `IDirectSoundBuffer` at
  `0x7DE3C8`, re-read before every method call. `SndStream_Stop`'s status is
  an uninitialised local in the original that `GetStatus` fills; ours starts
  it at 0 - the same unless `GetStatus` fails without writing it, which the
  fuzz does not model.

## 6. The fuzz, and the controls

`BOF3X_SHADOW=battle_items`, at start-up: twenty-four byte-copies
(`battle_items_fuzz.cpp`, `kClones`), every call out re-aimed at a recorder
(`bof3::CloneCall` with the callee each site was read to call; `_ftol` kept),
none with a jump table (`Sparkle_Dispatch`'s table is in `.data`: its first
four entries are swapped for recorders and put back). 2,000 rounds per
function: random bytes over the sparkle pool, the scratch words
(`0x903850`, `0x9037A0`), the sparkles' tables `0x65AD34..0x65AE37`, the
party and eight enemy records, `Sprite_Current` / `Frame_Counter`,
`Gfx_PacketNext`, the actor index, `Field_State`, `Snd_Device`, the stream
cell, two sprite and two actor records and a primitive buffer of the fuzz's
own, and the effect sizes of enemy types 0..7 at `0x8C5652`; the pointers put back inside them; then each function's boundaries
seeded (the delay 0 / 1 / 2, the offset table's 0 / -1 / 0x7FFF / 0x8000 /
7 / -8, count one below the limit, `Frame_Counter` and 3 zero, the draws'
start 0 / 0xFFFF / 0xFFE0 and radius 0 / 0xFFFF / 0x10000 / -1, a full pool
or one free record at 0 / 1 / 0x7E / 0x7F, heights 0 / 1 / -1 / -2 / 0x8000
/ 0x7FFF, positions 0x80000000 / 0x7FFFFFFF, actor indices 0 / 1 / 2 / 3 /
4 / 10, sounds 0 / 1 / 0xFE / 0xFF with stale upper bytes, the flags bits
either way); theirs, then from the same state ours; the regions, the answer
(al for `Sparkle_Alloc` and `BattleActor_FxSize`) and the recorders' log
compared.

The recorders are louder than the real callees: any call may move the
current sparkle, the effect's actor, `Sprite_Current`, `Gfx_PacketNext`, the
stream's buffer (between two fakes, never to null - the original reads it
again and calls through it), a scratch or vertex word, a field of the
sparkle or the sprite, `Frame_Counter` or the actor's sparkle count; the
sine and cosine answer anything a quarter of the time, `Rand` negative
values the CRT never gives, the projections NaN, infinities, 2^63 and
-0.99999994 (each value of a call salted apart); the commits advance `Gfx_PacketNext` as the real ones do.

Result (2026-09-23):

    shadow      battle_items self-test: 48000 rounds over 24 functions (2000 each), 1583500 calls to the
                stand-ins, 0 MISMATCHES
    shadow      battle_items coverage: dispatched 536 / 481 / 500 / 483; alloc found 1590, full 410; launched 320;
                freed 311; party animations 1024, enemy 976; stream data rewritten 578; sorted commits 198000,
                sin 308000, cos 238000, tints 337, actor sounds 1598, enemy cues 531, sounds by id 503,
                lost-buffer plays 1704

`BOF3X_SHADOW='*'`: exit 0, every module's self-test passing (2026-09-24).

**Seventy-two negative controls**, planted one at a time by a script (not committed: apply, build, run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=battle_items`, restore), each build's output read. Seventy-one are refused by a count (exit 3), each only in the functions it touches; one by a fault:

| | Planted | Refused in (rounds of 2,000, by function) |
|---|---|---|
| C1 | PolyG3: code 0x31 | 2,000 |
| C2 | PolyG3: two floats only | 2,000 |
| C3 | LineG2: second float at +0x1C | 2,000 |
| C4 | LineG3: code 0x5C | 2,000 |
| C5 | Tile1: no float | 2,000 |
| C6 | Dispatch: the phase byte as the type | a fault (access violation, exit 0xC0000005) - see below |
| C7 | Launch: delay down by 2 | 2,000 |
| C8 | Launch: sign test <= 0 | 27 |
| C9 | Launch: x pointer read after the Rand | 7 |
| C10 | Launch: dy added | 307 |
| C11 | Launch: rise length Rand & 7 | 157 |
| C12 | Launch: row from the sparkle before the Rand | 14 |
| C13 | Sway: 25 not 24 | 2,458 over 2 functions (Sparkle_Rise 1,216, Sparkle_Fade 1,242) |
| C14 | Rise: phase on count != limit | 2,000 |
| C15 | Sway: angle & 0x7F | 2,024 over 2 functions (Sparkle_Rise 995, Sparkle_Fade 1,029) |
| C16 | Fade: every eighth frame | 614 |
| C17 | Fade: actor count kept | 296 |
| C18 | Fade: not freed | 311 |
| C19 | RaysG2: radius as a dword | 1,055 |
| C20 | RaysG2: angle & 0x3F | 1,485 |
| C21 | RayShade: * 5 | 3,769 over 2 functions (Sparkle_DrawRaysG2 1,907, Sparkle_DrawRaysG3 1,862) |
| C22 | RaysG2: three rays | 2,000 |
| C23 | RaysG3: a quarter radius | 1,419 |
| C24 | RaysG3: tip dark 0 | 2,000 |
| C25 | RaysG3: tip y by cos | 2,000 |
| C26 | Disc: Rand & 7 | 950 |
| C27 | Disc: colour row shade + 3 * kind | 1,950 |
| C28 | Disc: rim 1 | 1,992 |
| C29 | Alloc: full answers 0x7F | 410 |
| C30 | Alloc: 127 records | 207 |
| C31 | Free: four bytes | 1,992 |
| C32 | Fan: radius 0x121 | 1,978 |
| C33 | Fan: shade * 11 | 1,749 |
| C34 | Fan: closing tpage 0x16 | 2,000 |
| C35 | Fan: previous rim read before the setters | 187 |
| C36 | PushMatrix: height not negated | 1,498 |
| C37 | PushMatrix: translation outside the MATRIX | 2,000 |
| C38 | PushMatrix: x >> 8 | 1,021 |
| C39 | Ring: lift + 0x41 | 1,936 |
| C40 | Ring: sort lean from y | 2,000 |
| C41 | Ring: u1 = i * 4 | 1,999 |
| C42 | Ring: lift angle without i | 2,000 |
| C43 | Ring: last commit re-reads Sprite_Current | 1,999 |
| C44 | FxDisc: radius * 3 | 1,810 |
| C45 | FxDisc: red * 11 | 1,999 |
| C46 | FxDisc: Sprite_Current kept over the sin | 650 |
| C47 | SetAnimation: party below 4 | 314 |
| C48 | SetAnimation: Sprite_Current not put back | 1,941 |
| C49 | SetAnimation: the byte alone | 1,024 |
| C50 | Ftol16: NaN answers 0x8000 | 1,182 |
| C51 | UpdateScreenXY: height not negated | 1,467 |
| C52 | UpdateScreenXY: y from +8 | 1,485 |
| C53 | Flash: bit 0x40 | 173 |
| C54 | Flash: enemy flag at +0x90 | 172 |
| C55 | Flash: out ignored | 656 |
| C56 | PlaySound: first not + 1 | 799 |
| C57 | PlaySound: 0x601 | 503 |
| C58 | PlaySound: mode inverted | 1,034 |
| C59 | PlaySound: Field_State not set | 966 |
| C60 | FxSize: bit 0 | 428 |
| C61 | FxSize: enemy stride 0x8B | 859 |
| C62 | Play: no Snd_Device test | 521 |
| C63 | Play: lost is 0x88780097 | 762 |
| C64 | Play: second Play on the first buffer | 68 |
| C65 | Stop: status bit 1 | 786 |
| C66 | Stop: cell kept | 1,473 |
| C67 | Stop: Release on the first pointer | 35 |
| C68 | Launch: drift Rand & 3 | 114 |
| C69 | Launch: base x before the throw | 320 |
| C70 | Ring: v2 z not zeroed | 2,000 |
| C71 | Dispatch: table of words | 983 |
| C72 | Fade: count down with the phase | 1,204 |

Three need a word:

- **C6 is refused by a fault**: the phase byte read as the type sends both
  sides through `Sparkle_Types` past its four swapped entries, into data -
  exit `0xC0000005`, which proves less than a count (Traps). C71, the right
  byte but the table indexed by its bit 0, is its counting twin: 983 rounds.
- **The first run of the controls had two not refused** (C52 and C61), and
  both were the fuzz's blindness, not changes that change nothing: the
  projection stand-in wrote the same float to x and y (one `Hash()` per
  call), and the enemy effect-size table at `0x8C5652` was not a region (all
  zero at start-up). The stand-ins now salt each value within a call, and
  the table's first eight types are randomised with the enemy's type seeded
  0..7; both are refused (1,485 and 859 rounds), and the table above is the
  second run, every control against the final fuzz.
- The thinnest by count are C9 (7 rounds: the current sparkle moved by the
  `Rand` stand-in exactly while the x pointer is held), C12 (14), C8 (27: a
  row's dx at exactly 0), C67 (35) and C64 (68) - each a re-read that only a
  stand-in moving the cell can tell apart.

## 7. What the combat route reaches, and what is fuzz only

The combat route (`analysis/calltrace/recipe_combat`, all original) calls
every one of the twenty listed functions: the sparkles 2,554 dispatches,
1,514 discs, 1,130 of each ray, 60 allocations; the shared draws 66 / 192 /
66 and the matrix push 258 (from the effect at `0x4C4C00`); the setters
13,168 / 4,520 / 4,520 / 68; the actor helpers 2 / 68 / 2 / 1 / 1; the
stream 3 / 3. The four hidden phase functions are not entries in the trace;
they run under `0x4B9000`, once per dispatch, and `Sparkle_Free` once per
expired sparkle - reached by inference, not measured. Fuzz only: the
enemy branches of `BattleActor_SetAnimation` / `Flash` / `PlaySound` /
`FxSize` unless the route's actor was an enemy, `Sparkle_Alloc`'s full pool,
and `SndStream_Play`'s lost buffer.

For the batch check: all twenty-four on the `--original` list, and the
entries of section 1 in `entries_logic.txt`.

## 8. Defects found (Capcom's, latent, kept)

For the coordinator to number in [`known-defects.md`](known-defects.md):

- **`0x4B9000`'s phase table is unbounded** (section 3): a phase of 3 or
  more calls its own return address or the caller's stack. Latent - phases
  stay 0..2. Left Capcom's.
- **`Sparkle_Dispatch`'s table has one entry**: a type other than 0 jumps
  into the data after it. Latent - the one writer stores 0. Kept.
- `SndStream_Stop` tests an uninitialised local if `GetStatus` fails (section
  5). Latent.

Not defects, noted: `MagicFx_DrawRing`'s sort key adds the inner x to both
axes (the PSX's too); the port's fixed tpages where the PSX asks the video
mode.

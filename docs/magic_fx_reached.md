# The magic effects the fight casts, reached only through pointers

**Status:** IN PROGRESS (2026-09-25) - twenty-five functions ours
(`src/game/magic_fx_reached.cpp`, shadow name `magic_fx_reached`), each
fuzzed headless against a copy of Capcom's with every call re-aimed at a
recorder; 78 negative controls, every one refused by a count (exit 3) in
the one function it touches. Not yet through a live check: the coordinator's combat
A/B after the merge.

Group CJ of the eighth round ([`takeover-queue-round8.md`](takeover-queue-round8.md)).
The PSX `BMAGIC` overlays' code compiled into the exe, under the hosts
`0x4ACA50`, `0x4B5350`, `0x4B8BD0`, `0x4C4C00`, `0x4F5050` and `0x4FAF90`
that `pe_funcs.py` folded them into. Read, the 25 are four effects and one
shared end:

- **the disc-and-fan effect** (kind-2 row 21) and its six rings;
- **the steal** (row 69) and the thief's double;
- **the Healing Herb** (row 43; MAGIC070.EMI, [`battle_items.md`](battle_items.md))
  - its task and the sparkle's update, the two parts of it round seven left;
- **the backdrop dim** (kind 1, parameter 0x43 - the second task
  `Battle_StartAbilityMagic` starts);
- `MagicFx_EndWhenIdle`, the last phase the steal shares with another
  effect.

No divergence and no `DIVERGENCE.md` entry: every function is a faithful
replacement, except that an index past one of the five stack tables aborts
(section 3) - the precedent is `battle_flow.md` section 1 and `mode_flow`'s
`Transition_Task`.

Their neighbours `MagicFx_PushActorMatrix`, `MagicFx_DrawFan`,
`MagicFx_DrawDisc`, `MagicFx_DrawRing`, `Sparkle_Dispatch` and the sparkles'
phases are round seven's (`battle_items.cpp`), called here through their
names.

## 1. How they are reached

Every one runs as a battle task: a slot of the 48 at `0x93A000` (0x84
bytes, `BattleTask_Create`'s), with `Sprite_Current` the slot and `0x93B940`
its owner `+0x80`, both set by `BattleTask_RunAll`. So every function here
is `void (void)`, `__cdecl`, whether called or jumped to - its arguments and
return are the task runner's. The two entrances (both group CE's, not ours):

- **kind 2** - `0x4378B0`: `jmp [0x64C2BC + 8 * row]` (an eight-byte table,
  the second word of each row unread). Row 21 (`.data 0x64C364`) is
  `FxDiscFan_Task`, row 43 (`0x64C41C`) `Sparkle_Task`, row 69 (`0x64C4EC`)
  `Steal_Task`. The table is `0x4378B0`'s, so it is not named here.
- **kind 1** - `0x435350`: a 110-entry table built on its stack, called by
  the parameter `+5`. Entry 22 (`.text 0x43540A`) is `FxRing_Task`, 67
  (`0x4355DE`) `FxDim_Dispatch`, 69 (`0x4355F4`) `StealClone_Dispatch`.

Inside the effects the phase is a byte of the task (`+1`, or `+2` for the
double) and the dispatch is one of two kinds:

- **a stack table**: `mov [esp + 4k], imm32` then `call [esp + 4 * phase]`
  - `FxDiscFan_Task` (3), `Steal_Task` (4), `StealClone_Task` (4),
  `Sparkle_Task` (6), `Sparkle_Update` (3). Ours holds each table in
  `magic_fx_reached::g` (the callees header), so the fuzz can swap it.
- **a `.data` table read in place**: `FxRing_Task` calls through, and
  `StealClone_Dispatch` / `FxDim_Dispatch` are a 16-byte `jmp [table + 4 *
  +1]` stub. Ours reads the same cell and calls it; named in `symbols.toml`:

| Table | At | Entries | Read by |
|---|---|---|---|
| `FxRing_Phases` | `0x65B5B8` | 4: `0x4AD200` `0x4AD270` `0x4AD2B0` `0x4AD300` | `FxRing_Task` |
| `FxRing_PhasesTwin` | `0x65AA28` | the same four | `0x4AD1C0` (the twin effect's ring task; not ours) |
| `StealClone_Types` | `0x65AC28` | 2: `0x4B5830` `0x4B5B30` | `StealClone_Dispatch` |
| `FxDim_Phases` | `0x65C3A0` | 4: `0x4332E0` (CE's) `0x4FB010` `0x4FB050` `0x4FB070`, then a 0 | `FxDim_Dispatch` |
| `Steal_RateTable` | `0x65AC20` | 8 signed bytes: 0 1 3 6 12 16 32 32 | `Steal_Start` |
| `Sparkle_CountByKind` | `0x65AE10` | 8 bytes: 30 60 60 60 60 0 0 0 | `Sparkle_Spawn` |
| `Sparkle_DelayByKind` | `0x65AE18` | 8 bytes: 5 4 5 4 4 0 0 0 | `Sparkle_Spawn` |

The fourth `FxRing_Phases` entry and `StealClone_Types`' second are not
reached from these callers (the third ring phase frees the task; nothing
steps the double's `+1`).

## 2. What is ours

Sizes are ret to ret (capstone, every jump internal).

| Function | Entry | Bytes | Reached as | Does |
|---|---|--:|---|---|
| `FxDiscFan_Task` | `0x4C4FC0` | 0x5C | kind-2 row 21 | phase by `+1` (stack table), then the disc and the fan while it lives |
| `FxDiscFan_Start` | `0x4C5020` | 0xEC | its entry 0 | the source's position, six ring tasks, a CLUT made semi-transparent, flags 0x10, sound 0x100 |
| `FxDiscFan_Grow` | `0x4AD130` | 0x29 | entry 1 (shared with `0x4ACFE0`) | `+9` / `+0xA` up to 0x10 |
| `FxDiscFan_Fade` | `0x4AD160` | 0x5C | entry 2 (shared) | fade as the rings go; the end flags; free |
| `FxRing_Task` | `0x4C5110` | 0x33 | kind 1, parameter 0x16 | phase through `FxRing_Phases`, then the ring |
| `FxRing_Wait` | `0x4AD200` | 0x6A | `FxRing_Phases[0]` | the delay, then the owner's position |
| `FxRing_Rise` | `0x4AD270` | 0x38 | `[1]` | `+0xB` +1, `+0xA` +2, `+9` -2 to 0x10 |
| `FxRing_Fade` | `0x4AD2B0` | 0x43 | `[2]` | `+0xA` -3, `+9` -2 to 0; the owner's count; free |
| `Steal_Task` | `0x4B54B0` | 0x36 | kind-2 row 69 | phase by `+1` (stack table) |
| `Steal_Start` | `0x4B54F0` | 0x271 | its entry 0 | the double, and the roll (section 4) |
| `Steal_Wait` | `0x4B5770` | 0x47 | entry 1 | wait for the double; the thief's animation 4 |
| `Steal_Report` | `0x4B57C0` | 0x4C | entry 2 | the item's name and the message, once the window is down |
| `MagicFx_EndWhenIdle` | `0x4F52D0` | 0x16 | entry 3, and `0x4F50B0`'s entry 2 | the done flag and free, once the window is down |
| `StealClone_Dispatch` | `0x4B5810` | 0x12 | kind 1, parameter 0x45 | `jmp [StealClone_Types + 4 * +1]` |
| `StealClone_Task` | `0x4B5830` | 0x46 | `StealClone_Types[0]` | phase by `+2` (stack table), then its sprite's screen update |
| `StealClone_Run` | `0x4B5880` | 0x33 | its entry 1 | animate; at `+9` 0 the thief's sounds (0, 4) |
| `StealClone_Finish` | `0x4B58C0` | 0x2D | entry 2 | animate to the end; flag 0x40; the owner's count |
| `Sparkle_Task` | `0x4B8D70` | 0x81 | kind-2 row 43 | phase by `+1` (stack table), then every live sparkle's dispatch |
| `Sparkle_Spawn` | `0x4B8E00` | 0x14E | its entry 0 | the pool cleared, the source's position, the sparkles made |
| `Sparkle_End` | `0x4B8F50` | 0x85 | entry 4 | a tint record dimmed; the tints released, the target flashed |
| `Sparkle_Update` | `0x4B9000` | 0x8F | `Sparkle_Types[0]` | the sparkle's phase (stack table), its disc and its rays |
| `FxDim_Dispatch` | `0x4FAFF0` | 0x12 | kind 1, parameter 0x43 | `jmp [FxDim_Phases + 4 * +1]` |
| `FxDim_Down` | `0x4FB010` | 0x31 | `FxDim_Phases[1]` | `+9` down to -6, the map's CLUTs tinted |
| `FxDim_Hold` | `0x4FB050` | 0x12 | `[2]` | wait for `0x904AA8` bit 2 |
| `FxDim_Up` | `0x4FB070` | 0x30 | `[3]` | `+9` up to 0, free; tinted |

`symbols.toml` has each one's evidence. Every callee is typed in
`magic_fx_reached_callees.h`; the ones that are not ours and not named are
called by raw address, never bound: the steal double's phases `0x4ED5C0`
(its effect size) and `0x4AEE90` (a `jmp BattleTask_FreeCurrent`), the
Healing Herb's `0x4B1E70`, `0x4B1ED0`, `0x4EE8A0`, `0x4F7350` (all group
CK's), and `0x4B58F0` (an item's name into `Text_Records` by index and
category; in no group - reached only by a successful steal).

Globals read, none named before: `0x904AA8` bit 2 is the "effect is done"
flag every end sets and `FxDim_Hold` waits for; `0x904B34` the acting
actor, `0x904B44` the target, `0x904B4C` the sprite the effect starts from;
`0x939F60` is set while the battle message window is up (`0x597D90`, group
CM's, sets it when the message queue `0x93C2A0` / `0x93C2A1` is not empty;
`0x597E60` clears it).

## 3. The stack tables, and why `Sparkle_Update` is taken now

None of the five stack-table indices is checked by the original: past the
table it calls the dwords above it - its own return address, then the
caller's frame. In every effect here the index stays inside (each phase
steps it by one to the last entry, which frees the task or is never left:
`Sparkle_Task`'s last is `0x4F7350`'s free, `StealClone_Task`'s
`0x4AEE90`'s, `Steal_Task`'s `MagicFx_EndWhenIdle`'s, `FxDiscFan_Task`'s
`FxDiscFan_Fade`'s; `Sparkle_Update`'s phases stay 0..2 - Launch and Rise
step them, `Sparkle_Free` clears them). So it is latent. Ours aborts with a
`Fatal` naming the function and the phase (rule 4), as `battle_flow`'s
`Battle_PhaseDispatch` / `BattleTask_RunAll` and `mode_flow`'s
`Transition_Task` do.

Round seven left `0x4B9000` Capcom's for exactly this reason
([`battle_items.md`](battle_items.md) section 3: "ours could not reproduce
what a phase of 3 does, and a check would be a divergence"). The round-eight
queue puts it in this group, and the project has since taken the other
stack-table dispatchers with the abort (round seven's own `battle_flow`), so
it is taken the same way here. If the coordinator reads the abort as a
divergence, the same reading applies to `battle_flow`'s two and to the four
others here, and one ledger entry would cover them all.

The `.data` tables are read in place, index unchecked, exactly as the
original: a byte past the table reads the next `.data` dword and calls it,
which ours does too.

## 4. The effects

**The disc-and-fan effect** (row 21). `FxDiscFan_Start` takes the source
sprite's position, starts six ring tasks (kind 1, parameter 0x16) owned by
it with delays 1, 7, 13, 19, 25, 31, counted in its `+0xB`; makes the CLUT
of strip row 26 (VRAM row 506, `Gfx_ClutStrip + 0x3400`) semi-transparent -
cells 1..15 copied from the buffer 0x4000 below with bit 15 set, cell 0
cleared - and marks the strip dirty; sets the target's flags 0x10 and plays
sound 0x100. The task then grows `+9` / `+0xA` to 0x10, and fades them as
its rings finish (below four rings `+9`, below three `+0xA`), and with none
left sets the target's flag 0x40, the done flag, and frees itself. Every
frame it lives it draws `MagicFx_DrawDisc` at its screen point 16 up and
`MagicFx_DrawFan` under the actor's matrix. Each ring waits its delay,
takes the owner's position, rises (`+0xB` the ring's phase angle, `+0xA` its
lift, `+9` its radius, the three `MagicFx_DrawRing` reads) and fades,
decrementing the owner's count. `0x4ACFE0` is the same effect in a second
overlay (rings of parameter 0x34 through `0x4AD1C0` and `0x65AA28`, drawn by
`0x4AD350`, the sound before the flags); the linker folded their identical
phases into one body, so `FxDiscFan_Grow` / `_Fade` and the three ring
phases are shared - taking them here takes them for the twin too. Which
spell row 21 is was not identified: the draws are the ones round seven
matched to Sacrifice's overlay (MAGIC055), but other overlays share them.

**The steal** (row 69). Named for what `Steal_Start` does - the sibling's
`names/overlays.toml` has `MAGIC115` as "UtmostAttack, Steal", and the
queue's PSX twin for `0x4B54F0` is `801EEC90`; neither pairing was checked
further. The start plays the thief's animation 0xC, creates the thief's
double - a kind-1 task (parameter 0x45) whose first 0x80 bytes are a copy of
the acting party record, its `+1` / `+2` cleared and kind / parameter set
back after the copy - and rolls. (The animation is `BattleActor_SetAnimation`'s offset 0xC from the thief's own.)

- the multiplier m from the speed difference d = thief `+0xA8` - enemy
  `+0xB8` (words): 12 for d >= 49, then 11, 10, 9, 8, 7, 6, 5 below 49, 29,
  19, 9, -10, -20, -30, and 4 below -50;
- success when `Rand() & 0xFF` < `Steal_RateTable[enemy +0xAA]` (signed) x m;
- then the enemy's item `+0xA8` (category << 8 | index): none gives message
  0x3A; `Inventory_Add(category, index, 1)` answering 0 (no room: a stack at 99, or no free slot) gives
  0x39; else 0x38, the item kept in the task's `+0x2C` and the enemy's item
  and rate cleared;
- a failed roll gives 0x39, or 0x3A when the enemy's rate row is 0.

`Steal_Wait` waits for the double (`+0xB`), then at its first frame plays
animation offset 4 and clears the owner's bit 6, and counts `+9` from 0x1E down.
`Steal_Report`, once the message window is down, puts the item's name in
`Text_Records` for a theft and queues the system message `+0xA`
(`BattleQueue_Push(1, 0x1E, text)`). `MagicFx_EndWhenIdle` sets the done
flag and frees the task. The double: its size (`0x4ED5C0`), then
`StealClone_Run` animates it `+9` frames and plays the thief's sounds (0,
4), `StealClone_Finish` animates it to the end and sets the target's flag
0x40, `0x4AEE90` frees it; its sprite is updated each frame it lives. That
these message ids are "stole", "could not" and "nothing to steal" is by the
branches, not by reading the text.

**The Healing Herb** (row 43). `Sparkle_Task` runs the effect's phase and
then walks the sparkle pool ([`battle_items.md`](battle_items.md) section 3
has the sparkles). `Sparkle_Spawn` clears the pool, takes the source's
position and screen point, and makes `Sparkle_CountByKind[kind]` sparkles
(30 for kind 0, the one it sets) with launch delays in steps of
`Sparkle_DelayByKind` per four; a full pool skips one. `Sparkle_End` dims
the tint record `+0xA` (the one `0x4B1E70` took and `0x4B1ED0` brightened) by one a frame for `+9` frames, then releases the
source's tints and flashes the target. `Sparkle_Update` is the sparkle's type
0: its phase, its disc once launched, its rays on frames the sway allows.

**The backdrop dim** (kind 1, parameter 0x43). `0x4332E0` (CE's) zeroes
`+9`; `FxDim_Down` tints the map's CLUTs one step darker a frame to -6;
`FxDim_Hold` waits for the done flag; `FxDim_Up` brightens back to 0 and
frees the task - the tint of the last frame, 0, applied after the free.

## 5. The fuzz, and the controls

`BOF3X_SHADOW=magic_fx_reached`, at start-up: twenty-five byte-copies
(`magic_fx_reached_fuzz.cpp`, `kClones`), every call and tail jmp out
re-aimed at a recorder (`bof3::CloneCall` with the callee each site was
read to reach; none is kept), the five stack tables' immediates checked and
re-aimed in the copies, and the three `.data` tables' entries swapped for
recorders and put back. 2,000 rounds per function: random bytes over the
task slots, `0x93B8C0..0x93B960` (the current slot, the owner and the
fields an enemy index of 2 reads), `Sprite_Current` / `Gfx_ClutStripDirty`
/ `Frame_Counter`, the battle state `0x904AA8..0x904B50`, the message-window
byte, party records 0..4, enemy records 0..7, `Steal_RateTable`, the
sparkle tables, the sparkle pool and its current pointer, the tint records,
the CLUT row and its source, and two sprite records of the fuzz's own; the
pointers put back inside them (every sparkle's owner too); then each
function's boundaries seeded (each table index across its entries, the
`+9` / `+0xA` / `+0xB` values either side of every compare, the message
window up or down, the done flag either way, sparkle counts 0 / 1 / 2 / 5 /
30 / 60 / 255, the steal's target an enemy or a party slot, its rate row
0..7, the speed difference at each threshold and one below, the item 0 or
not, `Rand` near the rate times a multiplier); theirs, then from the same
state ours; the regions and the recorders' log compared.

The recorders are louder than the real callees: any call may move
`Sprite_Current`, the owner, the source, the current sparkle, the target
and actor bytes, the message-window byte, the flags, `Frame_Counter`, or a
field of the task, the owner, the sparkle or the target enemy; the ones
that answer a byte (`BattleTask_Create`, `Inventory_Add`,
`Sprite_ScriptTickOnce`, `Sparkle_Alloc`) answer garbage above it;
`Sparkle_Alloc` answers 0xFF a quarter of the time; `Rand` answers values
the CRT never gives.

Result (2026-09-25):

    shadow      magic_fx_reached self-test: 50000 rounds over 25 functions (2000 each), 248158 calls to the
                stand-ins, 0 MISMATCHES
    shadow      magic_fx_reached coverage: disc-fan phases 695 / 652 / 653, steal 506 / 472 / 504 / 518, double
                507 / 502 / 516 / 475, herb 307 / 341 / 351 / 336 / 342 / 323, sparkle 692 / 665 / 643; ring table
                479 / 510 / 484 / 527, double types 991 / 1009, dim table 499 / 466 / 516 / 519; stolen 432, bag full
                245, no add 1323; creates 14000, frees 1983, allocs 29642, dispatches 128174, rays 402 / 402, tints
                4000, messages 1339

The fuzz found no difference of ours. Its first runs failed twice on its
own blindness, both fixed before the result above: every sparkle's `+0x28`
became the owner `Sparkle_Task` hands on, and was a random dword the
stand-ins then wrote through (an access violation); and the fields a target
of 2 reads, `0x93B8E0..`, lay outside every region, so a stand-in's write
there in the original's pass survived into ours (34 rounds of `Steal_Start`).
The regions now cover `0x93B8C0..0x93B960` and the owners are put back. Two
seeds were added after the first run of the controls, which refused the
steal's two threshold controls in one round each: `Steal_RateTable` is given
the exe's values two rounds in three (random bytes made almost every rate
too large or negative to meet a roll), and the round's first `Rand` lands on
the compare or one below it half the time. The table below is the second
run, every control against the final fuzz.

`BOF3X_SHADOW='*'`: exit 0, every module's self-test passing (2026-09-25).

**Seventy-eight negative controls**, planted one at a time by a script (not
committed: apply, build, run `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=magic_fx_reached`, restore), each build's output read. All 78
are refused by a count (exit 3), each only in the function it touches; none
by a fault. At least one per behaviour: every constant, every compare's
bound, every table index, the order of the calls, and each re-read of a
cell the original reads again after a call.

| | Planted | Refused in (rounds of 2,000, by function) |
|---|---|---|
| C1 | DiscFanTask: screen y - 0xF | FxDiscFan_Task 980 |
| C2 | DiscFanTask: draws when +0 is 0 | FxDiscFan_Task 2,000 |
| C3 | DiscFanTask: fan before disc | FxDiscFan_Task 1,002 |
| C4 | DiscFanTask: table index phase ^ 1 | FxDiscFan_Task 1,347 |
| C5 | DiscFanStart: delay 6 i + 2 | FxDiscFan_Start 2,000 |
| C6 | DiscFanStart: parameter 0x17 | FxDiscFan_Start 2,000 |
| C7 | DiscFanStart: five rings | FxDiscFan_Start 2,000 |
| C8 | DiscFanStart: CLUT without bit 15 | FxDiscFan_Start 2,000 |
| C9 | DiscFanStart: CLUT cell 0 kept | FxDiscFan_Start 2,000 |
| C10 | DiscFanStart: Sprite_Current read before the create | FxDiscFan_Start 373 |
| C11 | DiscFanStart: dirty flag not set | FxDiscFan_Start 1,995 |
| C12 | DiscFanStart: target flags 0x20 | FxDiscFan_Start 2,000 |
| C13 | DiscFanStart: target read after the flags call order (sound first) | FxDiscFan_Start 2,000 |
| C14 | Grow: 0x11 | FxDiscFan_Grow 658 |
| C15 | DiscFanFade: first bound 5 | FxDiscFan_Fade 157 |
| C16 | DiscFanFade: second bound 4 | FxDiscFan_Fade 190 |
| C17 | DiscFanFade: done flag bit 3 | FxDiscFan_Fade 65 |
| C18 | RingTask: draws when +1 is 0 | FxRing_Task 251 |
| C19 | RingTask: table index ^ 1 | FxRing_Task 2,000 |
| C20 | RingWait: +9 = 0x41 | FxRing_Wait 335 |
| C21 | RingWait: +0x38 from the owner +0x3C | FxRing_Wait 335 |
| C22 | RingRise: +0xA += 3 | FxRing_Rise 2,000 |
| C23 | RingFade: owner count kept | FxRing_Fade 308 |
| C24 | StealTask: table index phase ^ 1 | Steal_Task 2,000 |
| C25 | StealStart: animation (0xC, 3) | Steal_Start 2,000 |
| C26 | StealStart: parameter 0x46 | Steal_Start 2,000 |
| C27 | StealStart: copy 0x7C bytes | Steal_Start 2,000 |
| C28 | StealStart: double kind 2 | Steal_Start 2,000 |
| C29 | StealStart: threshold 0x30 | Steal_Start 11 |
| C30 | StealStart: threshold -0x33 | Steal_Start 14 |
| C31 | StealStart: rate unsigned | Steal_Start 393 |
| C32 | StealStart: roll > not >= | Steal_Start 239 |
| C33 | StealStart: target not re-read after Rand | Steal_Start 27 |
| C34 | StealStart: Inventory_Add category from the low byte | Steal_Start 675 |
| C35 | StealStart: enemy item kept | Steal_Start 430 |
| C36 | StealStart: full bag 0x3A | Steal_Start 245 |
| C37 | StealStart: target not re-read after Inventory_Add | Steal_Start 16 |
| C38 | StealStart: owner bit 0x20 | Steal_Start 1,490 |
| C39 | StealStart: failed roll 0x39 whatever the rate | Steal_Start 65 |
| C40 | StealWait: animation 5 | Steal_Wait 126 |
| C41 | StealWait: owner &= 0x7F | Steal_Wait 93 |
| C42 | StealWait: first frame 0x1D | Steal_Wait 128 |
| C43 | StealReport: message window ignored | Steal_Report 661 |
| C44 | StealReport: name arguments swapped | Steal_Report 299 |
| C45 | StealReport: Sprite_Current not re-read after the name | Steal_Report 9 |
| C46 | StealReport: queue (1, 0x1F) | Steal_Report 1,339 |
| C47 | EndWhenIdle: flags |= 8 | MagicFx_EndWhenIdle 726 |
| C48 | CloneDispatch: index ^ 1 | StealClone_Dispatch 2,000 |
| C49 | CloneTask: update when +0 is 0 | StealClone_Task 2,000 |
| C50 | CloneTask: phase from +1 | StealClone_Task 1,504 |
| C51 | CloneRun: sounds (0, 5) | StealClone_Run 321 |
| C52 | CloneRun: phase +1 | StealClone_Run 321 |
| C53 | CloneFinish: owner +0xA | StealClone_Finish 1,033 |
| C54 | SparkleTask: owner not put back | Sparkle_Task 1,620 |
| C55 | SparkleTask: owner saved before the phase | Sparkle_Task 67 |
| C56 | SparkleTask: 127 sparkles | Sparkle_Task 995 |
| C57 | SparkleTask: table index phase ^ 1 | Sparkle_Task 2,000 |
| C58 | Spawn: +2 not cleared in the pool | Sparkle_Spawn 2,000 |
| C59 | Spawn: +9 = 9 | Sparkle_Spawn 1,774 |
| C60 | Spawn: delay n >> 1 | Sparkle_Spawn 1,332 |
| C61 | Spawn: colour Rand & 7 | Sparkle_Spawn 1,542 |
| C62 | Spawn: bound read once | Sparkle_Spawn 1,205 |
| C63 | Spawn: Sprite_Current not re-read after Rand | Sparkle_Spawn 605 |
| C64 | Spawn: sound 0x101 | Sparkle_Spawn 2,000 |
| C65 | Spawn: offset row n + 1 | Sparkle_Spawn 1,713 |
| C66 | End: tint bytes +1..+3 | Sparkle_End 2,000 |
| C67 | End: the actor flashed | Sparkle_End 317 |
| C68 | Update: G3 radius + 2 | Sparkle_Update 402 |
| C69 | Update: G3 start + 3 | Sparkle_Update 402 |
| C70 | Update: sway test & 7 | Sparkle_Update 278 |
| C71 | Update: disc without the launched test | Sparkle_Update 503 |
| C72 | Update: sparkle not re-read before G3 | Sparkle_Update 9 |
| C73 | Update: table index phase ^ 1 | Sparkle_Update 1,357 |
| C74 | DimDispatch: index ^ 1 | FxDim_Dispatch 2,000 |
| C75 | DimDown: -7 | FxDim_Down 407 |
| C76 | DimDown: tint not sign-extended | FxDim_Down 1,100 |
| C77 | DimHold: bit 3 | FxDim_Hold 996 |
| C78 | DimUp: +9 read before the free | FxDim_Up 20 |

The thinnest by count are C45 and C72 (9 rounds each: a stand-in moving
`Sprite_Current` or the current sparkle exactly across the one call between
the two reads), C29 (11) and C30 (14), the steal's thresholds moved by one
(only a roll landing on the compare with the difference at that threshold
shows them), C37 (16) and C78 (20) - each a re-read or a boundary that only a
stand-in moving the cell, or one seeded value, can tell apart.

## 6. For `analysis/calltrace/entries_logic.txt`

The 25 lines, sizes as in section 2, are appended to the main checkout's
copy (under a `group CJ` comment). The host lines there still run over
them; their bodies, measured to the first `ret` with no jump past it:

| Line now | Body |
|---|---|
| `004ACA50 8F3` | `57` (a stack-table dispatcher; the twin effect and its rings follow) |
| `004B5350 59D` | `158` |
| `004B8BD0 405` | `197` |
| `004B8FE0 315` | `12` (`Sparkle_Dispatch`; [`battle_items.md`](battle_items.md) section 1 asked for it, not yet applied) |
| `004C4C00 543` | `3B2` |
| `004F5050 590` | `57` |
| `004FAF90 442` | `57` |

For the batch check: all twenty-five on the `--original` list.

## 7. What the route reaches, and what is fuzz only

The first-call trace the queue was built from (2026-09-25) enters all 25
on the combat route. Not measured here: the call counts (the all-calls
trace never armed them), so which branches the route takes is unknown -
whether the steal succeeded, whether a ring's delay reached 0 on screen.
Fuzz only, certainly: an index past a stack table (the abort), the `.data`
tables' unreached entries, `BattleTask_Create` answering 0xFF (the stand-in
never does: the writes would leave the image), `Sparkle_Alloc`'s full pool
on the route, a steal with a party member as the target, a full bag.

## 8. Defects found (Capcom's, latent, kept)

For the coordinator to number in [`known-defects.md`](known-defects.md):

- **Five unbounded stack tables** (section 3): `FxDiscFan_Task`,
  `Steal_Task`, `StealClone_Task`, `Sparkle_Task`, `Sparkle_Update`. Latent;
  ours aborts. (The last was already proposed by round seven.)
- **Three unchecked `.data` tables**: `FxRing_Phases`, `StealClone_Types`,
  `FxDim_Phases` - a phase past the table calls the next `.data` dword
  (`FxDim_Phases`' fifth is 0: a call to null). Latent; kept.
- **`BattleTask_Create`'s 0xFF untested** by `FxDiscFan_Start` (six creates)
  and `Steal_Start` - with all 48 slots taken the owner goes to `0x9423FC`
  and, for the steal, a 0x80-byte copy to `0x94237C`: past the slots and
  past the image's end (`0x93F000`) - an access violation unless
  something else is mapped there. Round seven noted
  the same for the magic starters.
- **The steal does not check that the target is an enemy**: a target of 0..2
  reads (and on a theft writes) "enemy" fields below the enemy records - in
  the task slots, or at `0x93B8E0` for 2. Whether the game ever lets the
  player steal from an ally is not known here (a game fact for the owner).
- **`Steal_RateTable`'s row is the enemy's `+0xAA` unbounded** (8 rows);
  a row past 7 reads the dwords of `StealClone_Types` as rates.

Not defects, noted: `FxDim_Up` applies the tint after freeing its task
(`+9` survives `BattleTask_FreeCurrent`, so the value is right);
`Steal_Report` passes the message id in the low word of a dword whose high
word is `Sprite_Current`'s (`Msg_SystemPtr` reads the word).

## 9. The shape every spell effect shares - what a class harness needs

[`remaining-catalog.md`](remaining-catalog.md) section 3 item 5 wants one
fuzz set-up for the ~1,900 other battle magic functions. These 25 (and
round seven's) say what it must hold:

1. **One frame of state.** Every function is `void (void)` and reads
   `Sprite_Current` (the task slot), `0x93B940` (its owner), `0x904B34` /
   `0x904B44` / `0x904B4C` (actor, target, source), `0x904AA8`, `0x939F60`,
   the party and enemy records, and its own overlay's `.data` constants.
   A harness that randomises those regions and puts the three pointers back
   in the slots covers the class's inputs; this file's `g_regions` is the
   list.
2. **The phase dispatch, three ways.** A stack table (`mov [esp + 4k],
   imm32` / `call [esp + 4 * byte]`: find the immediates, check and re-aim
   them in the copy), a `.data` table read in place (swap its entries), or a
   `jmp [table + 4 * byte]` stub (the same). A scanner for the first two
   patterns finds every phase function of an effect from its entry - which
   is how the ~1,900 can be enumerated at all, since none is a `call rel32`
   target.
3. **One callee set.** Across these 25 the calls out are 29 functions, all
   ours except `Rand` and `0x4B58F0`: the task create / free, the target flags, the
   sounds, the actor animation and sounds, `Rand`, the script tick, the
   screen update, the matrix push / pop, the shared draws, the primitive
   setters (round seven), the CLUT tint, the message queue. A harness with
   one recording stand-in per callee - `StubFor` here and in
   `battle_items_fuzz.cpp`, merged - would clone any effect function
   without a per-function table, provided the calls are found by the
   disassembly (every `E8` / `E9` that leaves the extent - how this group's
   clone table was built, by a capstone pass over the 25).
4. **Linker-shared bodies.** Identical phases of two overlays are one body
   (`FxDiscFan_Grow`, the ring phases; round seven's shared draws): the
   harness must key on the address, and a takeover takes every overlay that
   shares it.
5. **What a harness cannot give**: the effect's meaning. Each still needs
   the owner's eye on the spell, which is the cast-per-spell live check the
   catalogue already names (DIV-0045's cheats).

The extent rule is the other half: `pe_hidden.py`'s extents over-ran every
host here, and the stack-table immediates are what bound them.

## 10. Other groups' addresses

- `0x4332E0` (group CE's list) is `FxDim_Phases[0]` - the dim's first phase
  (`+9` = 0, phase on), 0x12 bytes. It belongs with the dim, here, by what
  it is; left to CE as the queue files it.
- CK's `0x4ED5C0` and `0x4AEE90` are the steal double's phases 0 and 3, and
  CK's `0x4B1E70`, `0x4B1ED0`, `0x4EE8A0`, `0x4F7350` the Healing Herb
  task's phases 1, 2, 3 and 5 - by table, they are this group's effects'.
  Called here by raw address.
- `0x4B58F0` (the stolen item's name) is in no group.

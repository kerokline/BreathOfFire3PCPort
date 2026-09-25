# The magic effects the fight casts, reached only through pointers

**Status:** IN PROGRESS (2026-09-25) - twenty-five functions ours
(`src/game/magic_fx_reached.cpp`, shadow name `magic_fx_reached`), each
fuzzed headless against a copy of Capcom's with every call re-aimed at a
recorder; @CONTROLS@ Not yet through a live check: the coordinator's combat
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
back after the copy - and rolls:

- the multiplier m from the speed difference d = thief `+0xA8` - enemy
  `+0xB8` (words): 12 for d >= 49, then 11, 10, 9, 8, 7, 6, 5 below 49, 29,
  19, 9, -10, -20, -30, and 4 below -50;
- success when `Rand() & 0xFF` < `Steal_RateTable[enemy +0xAA]` (signed) x m;
- then the enemy's item `+0xA8` (category << 8 | index): none gives message
  0x3A; `Inventory_Add(category, index, 1)` answering 0 (a full stack) gives
  0x39; else 0x38, the item kept in the task's `+0x2C` and the enemy's item
  and rate cleared;
- a failed roll gives 0x39, or 0x3A when the enemy's rate row is 0.

`Steal_Wait` waits for the double (`+0xB`), then at its first frame plays
animation 4 and clears the owner's bit 6, and counts `+9` from 0x1E down.
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
the tint record `+0xA` by one a frame for `+9` frames, then releases the
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

@RESULT@

`BOF3X_SHADOW='*'`: exit 0, every module's self-test passing (2026-09-25).

@CONTROLTABLE@

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
  past the image's end (`0x93F000`), an access violation. Round seven noted
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
   already ours or raw: the task create / free, the target flags, the
   sounds, the actor animation and sounds, `Rand`, the script tick, the
   screen update, the matrix push / pop, the shared draws, the primitive
   setters (round seven), the CLUT tint, the message queue. A harness with
   one recording stand-in per callee - `StubFor` here and in
   `battle_items_fuzz.cpp`, merged - would clone any effect function
   without a per-function table, provided the calls are found by the
   disassembly (`calls.py`-style: every `E8` / `E9` leaving the extent).
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

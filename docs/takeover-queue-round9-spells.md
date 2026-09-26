# Takeover queue, round nine: the spell round

**Status:** DRAFT (2026-09-25) - the queue and a proposed first wave; group
SH (the harness and its proof, [`magic_harness.md`](magic_harness.md),
[`magic_steal.md`](magic_steal.md)) is done on its branch, no wave group is
cut. The owner chose the order: the harness group first, then a first wave
of about ten spell groups, later waves in later sessions.

The round takes every row of the effect table `Magic_Rows` (`0x64C2B8`, 151
rows of a u16 DAT file and a code pointer; [`battle_fx_tasks.md`](battle_fx_tasks.md)
§2): the PSX `BMAGIC` overlays compiled into the exe, and the five
engine-side rows. Names are the sibling's `names/magic.toml` **read one id
down** ([`cut-content.md`](cut-content.md) §2). The whole round is fuzz-only
(section 5).

## 1. The extents: where each overlay's code is

`tools/magic_rows.py` (new) prints every row and writes
`analysis/magic_rows.tsv` (a row a line: file, overlay, entry, abilities,
the overlay's extent and every function start in it with its size and
whether it is ours) and `analysis/magic_funcs.tsv` (a function a line).
Both are gitignored. `--unit MAGIC0NN` lists one overlay; `--clones` adds
the harness's clone table for it.

**The band.** The overlays sit between `0x498FE0` (MAGIC001's entry; the
function before it, `0x498DE0`, is the engine's level-up routine - it
works on the party records `0x903A7C..` and the level table `0x658F48`)
and `0x4FC6A0` (`Input_Latch`, the engine again). The coordinator's
"about `0x500330`" ran past the band's end into WinMain's neighbours.

**The starts** are pe_funcs.py's and pe_hidden.py's 2,030 in the band,
corrected by a recursive descent of each (capstone, every internal branch
and jump table followed):

- **3 dropped**: `0x49BFD0`, `0x4B15D0`, `0x4C9040` are cases of another
  function's jump table, not functions;
- **42 found**: code no start covered - 9 after a jump table (pe_hidden's
  rule wants a ret before a start), the rest reached only by a tail `jmp`
  or by nothing listed (e.g. `0x49DBC0`, `0x4F7220`, `0x4E9850`);
- **0 overlapping**: no function's descent runs into the next start.

**The rule.** Each overlay's code starts at its **lowest `Magic_Rows`
entry** and runs to the next overlay's. Measured, not assumed: every
function was reached from its file's entries (calls, tail jumps, code
immediates - the stack tables -, `.data` handler tables, and
`BattleTask_Create(1 or 3, parameter)` through the kind tables
`0x435350` / `0x4357D0`), and:

- in 126 of 127 overlays the lowest entry is the first function of the
  code its file reaches (the exception, MAGIC019, sits inside MAGIC018:
  the two are **one unit** here, `0x4A1980..0x4A218F`);
- **no function reached from one file only lies outside that file's
  extent** but one: `0x4DF820` in MAGIC124, reached by MAGIC113 - it is
  `Port_DroppedCall`, a bare `ret` every empty function of the program was
  folded into (symbols.toml), which the linker happened to keep there;
- files whose lowest entry is another file's function have **no code of
  their own**: MAGIC016 is folded whole into MAGIC015 (its only row, 58,
  is MAGIC015's row 7 entry), and MAGIC005, 029, 049, 133..136, 156, 157
  run MAGIC004's code (all ten rows point at `0x49BAF0`): the linker folded
  identical overlays into one copy;
- the code is linked in file-id order but twice: MAGIC008 comes after
  MAGIC018/019, and MAGIC080 (row 27, the general handler) comes last,
  after the effect library;
- the overlays' `.data` (their tables and constants, `0x65A4F0..0x65C43C`)
  runs in the same order, which is what the unreached functions' data
  references were checked against.

**Two things in the band that are not a spell's:**

- **the effect library**, `0x4FAFF0..0x4FC32F` (34 functions, 9 ours;
  taken by group L, [`magic_lib.md`](magic_lib.md)):
  after MAGIC226/227 and before MAGIC080, called by up to 94 overlays -
  `BattleActor_SetAnimation`, `_UpdateScreenXY`, `_Flash`, `_PlaySound`,
  `_FxSize`, the backdrop dim `FxDim_*`, a slot allocator for `0x6BAD60`.
  Its data follows MAGIC226's. `0x4FAF90` before it, first given to the
  library, is **MAGIC226/227's last** (settled by group L,
  [`magic_lib.md`](magic_lib.md) section 1: MAGIC225 ends with the same
  allocator for its own pool, and the two pools are consecutive `.bss`);
- **four `MapCell_Handlers` entries** and their helper, `0x4CEB40`,
  `0x4CED60`, `0x4CEFC0`, `0x4CF270`, `0x4CF4B0`, between MAGIC102 and
  MAGIC103: field map-cell draws (entries 40..43 of the 77 at `0x663008`,
  `Gfx_CommitPrim`, `Area_TestCondition`, `Prim_SetTexture`), reached from
  no spell. They are round eight DD's unqueued kind, not the spell round's.

**Shared bodies.** 781 of the round's functions are reached from more than
their own overlay: a kind-1 child effect several overlays create by the
same parameter (MAGIC008's code is reached from 017, 018 and 019 that way),
or a body the linker folded (`BattleFx_FreeTask` `0x4AEE90`, in MAGIC057,
is reached from 44). Taking one takes it for every overlay that reaches it;
the harness keys on the address, so nothing else changes.

**How sure.** The unit boundaries are as sure as the reach: every one is
confirmed by a function its own file reaches on each side; the library's
first boundary was settled by reading (above). The PSX catalogue's function counts
(`overlay_catalog.json`) are consistently about half the PC's: the PC
counts every stack-table phase, the catalogue's roots miss most.

## 2. The totals

| | Functions | Bytes |
|---|--:|--:|
| In the band, 125 overlay units and the library | 2,064 | 387,966 |
| Ours already (round seven's draws, CJ's 25, CE's, SH's 3, ...) | 52 | 8,145 |
| **The round: not yet ours** | **2,012** | **379,821** |
| The engine rows (section 3), outside the band | 22 | about 1,500 |
| Not the round's (the `MapCell_Handlers` five) | 5 | 2,688 |

Bytes are each function's own, to its last instruction (padding not
counted). The coordinator's measurement (~2,070 functions, ~640 KB) is the
same count before the corrections, and the bytes of a band running on to
`0x500330` with padding.

## 3. The engine rows

Five rows have file `0xFFFF` and a handler in the engine: rows 0 and 126
(`0x4378D0`: two phases, `0x47FDA0` and `0x437900`), 108 (`0x4525F0`, id
`0xD9` Restore Form: `0x452620`, `0x452660`), 123 and 128. None is ours.
Cut-content §3 asked for 123 and 128 to be read first. **Neither is
taken here**: both are small, but their callees and children are not read
to the end, and what they do is reading, not yet a measurement.

**Row 123, `0x43F3B0` (id `0x8B`, TCRF's Paralyzer: "crashes the game").**
A kind-2 task, a three-entry stack table by `+1`:

1. `0x43F3E0`: `Sprite_Current` becomes the current slot's (`0x93B8C4`)
   owner `+0x80` - the caster's sprite; `0x437450(word [[0x939AD8] + 0xF8]
   + 4)` - a sound cue (`Sound_PlayEffect` unless 0xFFFF) from the **current
   enemy's** cue table; `Sprite_SetAnimation(2)` on the caster; the slot
   back; on.
2. `0x43F430`: the caster's script ticked (`Sprite_ScriptTickOnce`) until
   it reports its end; on.
3. `0x43F460`: `Battle_SetTargetFlag40(target)`, the done flag, free.

`0x939AD8` is "the current enemy" (`battle_flow.md`): whichever live enemy
the per-frame loops set last, not the caster. Its `+0xF8` is a pointer
that only the **event-battle** set-ups write (`0x4379D0`, `0x437CC0`,
`0x4399E0`, `0x43B130`, `0x4400A0`: per-boss cue tables `0x64C7A0..`), and
every other reader of it tests the event-battle byte `0x904AAA` first
(`BattleActor_PlaySound`, `0x436740`, `0x436E40` - symbols.toml). Row 123
reads it **unconditionally**. So, by reading: cast in an ordinary battle,
its first step reads a word through whatever `+0xF8` holds there - if it is
0 (not measured), an access violation at address 0: **a crash**, on the PC as the
PSX's equivalent would be. An enemy-only skill used by an event-battle
boss never meets it. This is a defect candidate for the coordinator to
number, not a number. The check, one read: an ordinary battle's enemy
record `+0xF8` (`0x93BA58` for enemy 0) in a memory dump or `mem_watch`.

**Row 128, `0x43FC80` (id `0x80`, TCRF's Head Cracker: "freezes the
game").** A ten-entry stack table by `+1`:

1. `0x43FCE0`: animation 2 on the caster's sprite (as above), `+0xC` = 8.
2. `0x43FD20`: `+0xC` down; at 0 `Sound_PlayEffect(0x601)` and on; the
   caster's script ticked once.
3. `0x43FD80`: the caster's script until it reports its end; `+0xA` 0; on.
4. `0x43FDC0` (entries 3, 5, 7): a child task, kind 1 parameter `0x5D`
   (`0x43FE90`: something dropped on the target from above - sprite
   `0x1D3` at the target's position, falling under `+0x20` = -16 a frame
   until it meets the ground, `AreaMap_Elevation` + 0x180 - then
   `Battle_SetTargetFlag40(target)`, the parent's `+0xA` down, free);
   `+0xA` up; on.
5. `0x43FE00` (entries 4, 6, 8): wait while `+0xA` (children alive); then
   if `Battle_ActorIsOut(target)`, straight to entry 9; else **wait while
   the target's state byte `+1` is 6** (party `0x802D41 + 0x14C i`, enemy
   `0x93B961 + 0x128 (i - 3)`), then on.
6. `0x43FE80` (entry 9): the done flag, free.

Three drops, each waiting for the target to leave state 6. State 6 is the
reaction state: the turn step `EffectWait` sets `+1 = 6` for every actor
flagged `0x40` - which each drop's landing does - and the round's
effect-done bit is what it then waits for ([`battle_actions.md`](battle_actions.md)).
None of the waits has a limit. By reading, the freeze is one of two waits
that never end: the caster's animation 2 never reporting its end (step 3 -
the same wait row 123 has, if a party member's animation 2 is not what an
enemy's is), or **the target never leaving state 6** while Head Cracker
waits for it (step 5) - which would happen if the target's side holds
state 6 until the effect-done bit that Head Cracker sets only at its end.
For a party target (the enemy skill's design case) the member's state-6
handler is `0x441A10`'s table ([`battle_obj_states.md`](battle_obj_states.md),
not ours); for an enemy target (the skill hacked into the player's list,
TCRF's case) who moves an enemy's `+1` off 6 was not read. The check:
whoever writes an enemy's `+1` after `EffectWait`, or a live cast with the
cheat and a watch on `0x93B961`.

Both rows are **group E** in the queue: 22 functions with the child.
Taking them faithfully keeps the crash and the freeze; fixing either is a
divergence for the owner to choose (a living project may).

## 4. The queue

Whole overlays only, about 40..60 functions a group, in address order so
shared bodies stay close; the cut content first so it is visible; the
first wave marked **1**. Names are read one id down; "(no label)" is an id
whose lower neighbour has no English label. Counts are functions not yet
ours; bytes their own.

| Group | Overlays (MAGIC0NN) | Rows | Abilities (read one id down) | Fns | Bytes |
|---|---|---|---|--:|--:|
| C1 | 010, 080, 113, 145, 146, 213 | 9, 10, 27, 148, 149, 150 | Unmotivate, (no label), Watch Enemy, White Flag, Recall, MagicShuffle, Lark, The World, Again, Trump, Death Bomb, Roulette, Pentagram, Ink, Ink Ink, Miyakuri | 62 | 9,226 |
| C2 | 129, 057, 081, 116 | 59, 84, 85, 145 | Holocaust, Bone Dance, RottenBreath, UtmostAttack | 64 | 9,622 |
| C3 | 002, 111 | 2, 119 | - (no ability loads them) | 20 | 4,243 |
| E | engine: `0x4378D0`, `0x4525F0`, `0x43F3B0`, `0x43FC80` and their phases, `0x43FE90` | 0, 108, 123, 126, 128 | Restore Form, Paralyzer, Head Cracker, (no label) | 22 | ~1,500 |
| L **1** | the effect library | - | - | 25 | 3,899 |
| S01 | 001 | 1, 105 | Nue Stomp, Jump | 26 | 3,275 |
| S02 | 003, 004 (with 005, 029, 049, 133..136, 156, 157 folded) | 3, 88, 92, 93, 98..100, 129..132 | Super Combo, ThundrStrike, Holy Strike, Demonbane, Flame Strike, Pyrokinesis, Frost Strike, Wind Strike, Flame Claw, Frost Claw, Thunder Claw, Shining Claw | 48 | 6,774 |
| S03 | 006, 009, 012 | 51, 71, 109 | Mind Sword, Chlorine, Blitz | 44 | 9,184 |
| S04 | 013, 015 (016 folded) | 4, 7, 50, 55, 58 | Snap, Charge, Flying Kick, Air Raid | 56 | 9,776 |
| S05 | 017, 018/019 | 5, 6, 43, 53, 54, 68 | Astral Warp, Shadowwalk, Giant Growth, Aura, SpiritBlast, Double Blow, Multistrike, Triple Blow | 29 | 3,794 |
| S06 | 008, 020 | 8, 42 | Gambit, Mind Flay, Blind, Devour, (no label), Syphon, Feign Swing, Backhand, Risky Blow, Disembowel | 56 | 6,118 |
| S07 | 021, 038, 039, 040 | 33, 78, 79, 121 | Bonebreak, War Shout, Focus, Meditation, Enlighten | 59 | 10,091 |
| S08 | 041, 042, 043, 044 | 80, 81, 96, 110 | Berserk, Counter, Mind's Eye, WardOfLight, Resist, Evil Eye | 59 | 12,823 |
| S09 | 045, 046/047, 048, 050 | 31, 41, 61, 62, 63 | Bone Dart, Firebreath, Icebreath, Dream Breath, Pollen, Venom Breath | 47 | 8,956 |
| S10 | 052, 053, 054, 055, 056 | 56, 65, 107, 112, 134 | Ovum, Lavaburst, Howling, Ebonfire, Sacrifice | 60 | 9,739 |
| S11 | 058, 059 | 82, 83 | Sanctuary, Tornado | 35 | 7,142 |
| S12 | 060, 062 | 39, 95 | Identify, Celerity | 44 | 8,119 |
| S13 | 063 | 143 | Sudden Death | 25 | 3,796 |
| S14 | 064, 065, 066 | 17, 70, 77 | Weretiger, Pilfer, Tsunami | 57 | 8,654 |
| S15 | 067, 068, 069 | 18, 40, 75 | Chill, Foretell, Influence | 52 | 9,299 |
| S16 **1** | 071, 072, 073, 074 | 45, 47, 113, 114 | Healing Herb, Rejuvenate, Restore, Vitalize, Vigor | 60 | 12,517 |
| S17 **1** | 075, 077, 078 | 15, 30, 46 | Purify, Raise Dead, Resurrect, Leech Power | 48 | 7,685 |
| S18 **1** | 079, 082 | 32, 52 | Drain, Steroids, Magic Belt, Protect, Speed, Might | 42 | 7,758 |
| S19 **1** | 083, 086 | 28, 94 | Shield, (no label) | 43 | 7,848 |
| S20 **1** | 087, 088, 092 | 29, 48, 49 | Silence, Molasses, Tarbaby, Slow, Blunt, Weaken, Fireblast | 51 | 9,778 |
| S21 **1** | 093, 094, 095 | 20, 22, 103 | Inferno, Frost, Iceblast | 48 | 10,434 |
| S22 **1** | 096, 097, 098, 099 | 19, 36, 37, 102 | Blizzard, Jolt, Lightning, Myollnir | 56 | 14,969 |
| S23 **1** | 100, 101, 102, 103 | 13, 26, 57, 69 | Cyclone, Typhoon, Quake, Simoon | 51 | 12,001 |
| S24 **1** | 104, 105, 106 | 14, 64, 104 | Sirocco, Kyrie, Death | 47 | 10,068 |
| S25 **1** | 107, 108, 109, 110 | 12, 24, 25, 38 | Sleep, (no label), Confuse, Depress, Ragnarok | 56 | 12,823 |
| S26 | 114, 115, 117 | 11, 73, 106, 115 | Fire Whip, Remedy, Rest, Snooze, Douse | 48 | 8,928 |
| S27 | 118, 120, 121 | 16, 60, 122 | Burn, Whelp Breath, DragonBreath | 47 | 13,563 |
| S28 | 122, 123, 124 | 74, 76, 124 | Firebreath, Icebreath, ThundrBreath | 42 | 12,012 |
| S29 | 125, 126 | 125, 127 | DivineBreath, ShadowBreath | 49 | 10,165 |
| S30 | 130, 131 | 141, 144 | Venom, KaiserBreath | 60 | 9,099 |
| S31 | 132, 137, 138, 143 | 66, 101, 118, 139 | Doom Breath, Corona, Main Cannon, Thunder Clap | 51 | 9,832 |
| S32 | 144, 150 | 67, 72 | Wall of Fire, Eye Beam | 36 | 4,648 |
| S33 | 151, 154 | 35, 86 | Accession, Mighty Chop | 57 | 9,790 |
| S34 | 158, 159, 161, 162, 166 | 89, 90, 111, 116, 120 | Charm, (no label), Timed Blow, Transfer, Monopolize | 47 | 7,664 |
| S35 | 167, 168, 169 | 91, 97, 117 | Last Resort, Cure, Benediction | 46 | 9,873 |
| S36 | 172, 173, 218 | 23, 34, 142 | Magic Ball, Intimidate, Aura Breath | 45 | 12,729 |
| S37 | 219, 220/221, 222 | 133, 136, 138, 146 | Magma Breath, Geo Breath, Gaea's Breath, Combustion | 60 | 10,889 |
| S38 | 223, 225, 226/227 | 135, 137, 140, 147 | Tempest, Hurricane, (no label), MeteorStrike | 54 | 10,218 |

43 groups, 2,034 functions with group E. Rows already whole ours and in no
group: 21 (MAGIC091, Flare), 44 (MAGIC070, Heal), 87 (MAGIC216, Steal);
row 70 (MAGIC065, Pilfer) has one function left, in S14.

Notes on the names. The shift is TCRF's rule and holds where TCRF can be
checked; it breaks at least once: id `0xD5` (row 148) reads "Miyakuri",
where TCRF says PurifyAll. `0x7F` (row 126, TCRF's Assault) has no label
one below. The cut-content groups hold what cut-content §2 lists and TCRF
names; TCRF's other 22 enemy-only skills are in the S groups, unmarked,
because nothing here says which rows they are.

## 5. What a route reaches

Nothing new. The combat route's first-call traces (`analysis/hidden_reached_combat.json`,
`calltrace/recipe_combat`) enter, in the band, only functions already ours
(rows 21, 44, 70 and their shared phases, the library's ours) and one that
is not: `0x4DF820` (`Port_DroppedCall`, a bare `ret`, in S28's MAGIC124).
Every group is fuzz-only. Its live check is the owner casting the group's
spells, with a save that has them or DIV-0045's cheat; the shop and
world-map routes cast nothing.

## 6. The first wave (proposed)

**L, S16 .. S25**: eleven groups, 528 functions, about 110 KB - the effect
library the rest call, then the healing, restoring, buffing and elemental
attack spells (MAGIC071..110, contiguous from `0x4B9930` to `0x4D6110`).

Which spells players commonly cast is a **guess** here, not a measurement
or a sibling claim: these are the rows whose names read as the party's
spells, and most are loaded by two ids (`0x46` and `0xAE` for Heal, `0x5E`
and `0xC4` for Frost), which may be a party id and an enemy id - also a
guess. The owner's word replaces it. To trim to ten, drop S19 (Shield and
an unlabelled row).

**L first, within the wave**: its functions are callees of almost every
group, so taking it lets them join the harness's standard set (a group
lists what it calls of the library until then). The wave's groups do not
share functions with each other except through shared bodies the address
keys (section 1).

## 7. For the coordinator

- Each wave group follows [`magic_harness.md`](magic_harness.md) §3. Its
  module goes at the end of `inject_all.cpp` (after `MagicSteal_Inject`);
  order matters only where a cheat or divergence patches bytes inside a
  function a group takes - DIV-0046's two masks (Pilfer's, Steal's) are the
  only such patches in the band today (grep DIVERGENCE.md for any other
  before cutting a group).
- `tools/magic_rows.py` needs `--exe`, `--analysis` and `--sibling` from a
  worktree (the defaults are the repo's own `bof3/` and `analysis/`).
- Round eight CJ's doc numbers its rows one low in three places:
  `Sparkle_Task` is row **44** (not 43; its pointer at `0x64C41C` is
  right), `Steal_Task` row **70** (not 69; `0x64C4EC` right). Row 21 is
  right.

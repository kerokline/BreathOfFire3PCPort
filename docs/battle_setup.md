# Battle set-up: the per-turn status chain, its counters, the faster-side marks

**Status:** IN PROGRESS (2026-09-24) - nineteen functions ours
(`src/game/battle_setup.cpp`, shadow name `battle_setup`), each read whole
against its PSX twin and fuzzed headless against a copy of Capcom's with
every call re-aimed at a recorder: 38,000 rounds, 0 mismatches; 81 negative
controls, 80 refused by a count and one a change that changes nothing.
**Through the wave-2 batch** (2026-09-24, 1,020 ours - [`takeover-queue-round7.md`](takeover-queue-round7.md) "Result": the combat A/B 5 of 43 at 4..15 px, the tile-edge class); section 8 had it owed.

Group BA of the seventh parallel round
([`takeover-queue-round7.md`](takeover-queue-round7.md)). The battle engine is
the PSX's `BATTLE.EMI` compiled into the exe; the RAM layout it is read against
is the sibling's `docs/BATTLE_RAM.md` ([`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md)).
No divergence: every function is a faithful replacement and no
`DIVERGENCE.md` entry is owed. One defect of Capcom's found, kept as the
original has it (section 6, for the coordinator to number).

## 1. What is ours, and the extents

Extents by recursive descent (capstone, fall-through, jcc, jmp, stop at `ret`;
no jump table in any of them). PSX twins from `analysis/pairs_propagated.json`
unless noted, all in `BATTLE.EMI#3` (md5 `8a80230e`, load `0x801D0C00`) but
`0x800A0680`, which lives in the engine band loaded at `0x80093800`
(capture `4065db04`). Every twin was read side by side in capstone MIPS.

| Function | Entry | Bytes | PSX twin | Does |
|---|---|--:|---|---|
| `Battle_ClearActingFlags` | `0x4301B0` | 0xF6 | `0x801D4820` (call-anchored) | section 3 |
| `Battle_TickCounters` | `0x4303D0` | 0x123 | `0x801D4D14` (callers) | section 3 |
| `BattleStep_Expire4000` | `0x430640` | 0x150 | `0x801D524C` (no pair recorded; its place in the PSX chain driver `0x801D4FF4`) | section 2 |
| `BattleStep_Restore800` | `0x430790` | 0xF9 | `0x801D54C8` | section 2 |
| `BattleStep_PartyWake40` | `0x430890` | 0xD8 | `0x801D5628` | section 2 |
| `BattleStep_EnemyWake40` | `0x430970` | 0xD2 | `0x801D577C` | section 2 |
| `BattleStep_PartyWake20` | `0x430A50` | 0xD8 | `0x801D58DC` | section 2 |
| `BattleStep_EnemyWake20` | `0x430B30` | 0xD2 | `0x801D5A30` | section 2 |
| `BattleStep_Status80` | `0x430C10` | 0x127 | `0x801D5B90` | section 2 |
| `BattleStep_HpDrift` | `0x430D40` | 0x1F5 | `0x801D5D9C` | section 2 |
| `BattleStep_ApUpkeep` | `0x430F40` | 0xEB | `0x801D62A8` | section 2 |
| `Battle_ActorSkipped` | `0x431030` | 0x5F | `0x801D6494` | section 2 |
| `Battle_MarkFasterSide` | `0x4453C0` | 0x190 | `0x801DB020` | section 4 |
| `Battle_ActorStanding` | `0x445550` | 0x6A | `0x801DB2C0` | section 4 |
| `Battle_PartyOutpaces` | `0x4455C0` | 0x3F | `0x801DB368` | section 4 |
| `Battle_OpenMsgWindow` | `0x444310` | 0x2D | `0x801D9454` (call-anchored) | section 5 |
| `Battle_ReturnQueuedItem` | `0x446EA0` | 0x75 | `0x801DDFB4` (call-anchored) | section 5 |
| `Battle_OpeningMessage` | `0x44AA00` | 0x82 | `0x801DEA9C` | section 5 |
| `Battle_ClearStatus` | `0x44F4B0` | 0x194 | `0x800A0680` (call-anchored) | section 5 |

Every twin is the same function term for term; the differences are the
record strides (party 0x14C against 0x140, enemy 0x128 against 0x118), the
PSX scratchpad word `0x1F800044` that the PC keeps in `Sprite_Current`
`0x937F88`, and one re-read of the acting actor in `0x801D4820` that nothing
between can change. The names are hypotheses from the shape (the `status`
field says which); what each status bit and counter means in the game is not
read here (game facts are the owner's).

**For `analysis/calltrace/entries_logic.txt`**: three sizes there are
`pe_funcs.py`'s and run on - `004301B0 214` is `F6`, `004303D0 269` is `123`,
`00431030 97E` is `5F`. The other sixteen are right.

**The queue's note on the chain was off.** `0x4303D0` does not call the nine
steps: it calls only `Battle_ActorSkipped` and `0x4456C0`, and is itself
called from `0x430386`. The nine are called by **`0x430510`** (not in any
group), a state machine on the byte `0x904AA2` through the jump table
`0x4305FC` (14 states): each state advances `0x904AA2` and calls one step;
a step answering 1 (it opened the message window) returns, so the next frame
resumes at the next step; the last state zeroes `0x904AA2` and advances
`0x904AA1`. The PSX `0x801D4FF4` is the same (`0x801462DE` / `0x801462DD`).

## 2. The chain's steps

The actor objects: actors 0..2 the party, `ObjTrio` `0x802D40` stride 0x14C
(the persistent character record copied at +0x80, so its offsets + 0x80);
actors 3..10 the enemies, `0x93B960` stride 0x128 indexed by the actor less 3
(`EnemyWorkingRecords` `0x93B9E0` is its +0x80). Field names are in
`battle_setup_callees.h`.

- `Battle_ActorSkipped(actor)` - 1 while the timer byte `0x904B8E` runs and
  the actor lacks flag 0x10 (+0x134 / +0x114), else `0x4456C0(actor)`
  (group BB's: not present, or status 0x4000). Every step skips such actors.
- `BattleStep_Expire4000` - flag 0x4000 with its counter at 3: counter 0,
  flag cleared, HP change word (+0x128 / +0x108) = HP, the state bytes
  +1 +2 +4 +3 = 6 5 0 0, +0x12C / +0x10C = 0x11, `0x446FB0(actor)`; any:
  `Battle_OpenMsgWindow`, `Msg_SystemPtr(0x2D)`, `0x44A650(2, 0, 0, 0x2D,
  text)`. **The enemies' counter is read from the wrong array** - section 6.
- `BattleStep_Restore800` - members with status 0x800 and +0x143 at 5:
  counter 0, HP = max HP, AP = max AP, +0x9C = +0xAE,
  `Battle_ClearStatus(member, 0x8FF)`; one member: its name (`0x44A910`) and
  line 0x2B, more: 0x2C.
- The four wake steps are one template: `_PartyWake40` (status 0x40, lines
  0x25 / 0x26), `_EnemyWake40` (0x40, 0x25 / 0x27), `_PartyWake20` (0x20,
  0x28 / 0x29), `_EnemyWake20` (0x20, 0x28 / 0x2A). Each actor with the bit
  rolls `0x446CB0`, which reads the counter +0x12D / +0x10D: a hit zeroes the
  counter and clears the bit, a miss counts it up; one woken: its name
  (`0x44A910` / `0x44A960`) and the first line, more: the second.
- `BattleStep_Status80` - status 0x80: both change words zeroed, then
  `0x446540(actor)` sets the HP change (from HP: (HP + 5) / 10 by its
  arithmetic), effect 0x11, `0x446FB0`; any: line 0x18.
- `BattleStep_HpDrift` - per member the HP change word from 0: -1 while
  `0x904060` is 5; (max HP + 10) / -20 for character id 6; again for id 7 or 0
  with flag 2 while `0x904B89` is 0x12; weapon 0x52 adds (max HP + 10) / 20,
  then cut to HP - 1 when it reaches HP (signed); -1 for armour byte 3 = 0x1F
  and for each accessory 0x16 and each 0x17; - max HP / 2 with status 1. Not
  0: effect 0x11, `0x446FB0`. Enemies with status 1: -(max HP / 2). Answers
  whether `0x904B82` (the pending bits `0x446FB0` sets) is not 0.
- `BattleStep_ApUpkeep` - members with flag 2 and not 0x20 pay (the low byte
  of `0x904B78` + 1) / 2 AP: short, the state bytes 6 4 4 and `0x446FB0`;
  else AP lowered, +0x12A the cost, a cost not 0 marks effect 2 and calls
  `0x446FB0`. Without flag 2 or with 0x20: flag 0x20 cleared. Answers
  `0x904B82` not 0.

## 3. The bookkeeping

`Battle_TickCounters` `0x4303D0`: per member not skipped, +0x142 up while at
most 5 with flag 1; +0x143 up while at most 5 with status 0x800; +0x142 again
while at most 2 with flag 0x4000 (read after the first step); per enemy
+0x122 while at most 2 with flag 0x4000. Then `0x904B8E` counts down if
running, and on reaching 0 every actor `0x4456C0` does not rule out loses
flag 0x10.

`Battle_ClearActingFlags` `0x4301B0`: the acting actor (`0x904B34`'s low
byte) loses flag 0x40 and its byte +0x144 / +0x124 - unless the next byte is
4 and the word `0x904B80` 0x27 - then flag 0x80 and +0x145 / +0x125, unless
kind 4 with 0xA3. An actor above 2 is indexed (actor - 3) * 0x128 without a
bound, as in the original.

## 4. The faster-side marks

`Battle_MarkFasterSide` `0x4453C0` (called from `0x4302E2`, which switches on
its answer): the enemies `Battle_ActorStanding` passes give a summed and a
highest agility (+0xB8); the sum's low word is divided by the count only when
it is not 0. Each member `0x445980` (group BE's) lets act that
`Battle_PartyOutpaces` passes - agility (+0xA8) at least twice the average's
low word and at least the highest's - gets flag 0x8000, and the answer is 1
when any did. Then the members' agility the same way against each enemy
through `0x445600` (unnamed, `Battle_PartyOutpaces`' enemy twin, not taken),
flag 0x8000 on the enemy.

As the original has it: the sums and the highest are built from the whole
eax `Battle_ActorStanding` leaves - its upper half over the agility word -
so ours answers that eax exactly (the record offset with al over it); the
tests read low words only, so those upper halves are not observable today.
`Battle_ActorStanding` and `Battle_PartyOutpaces` both answer eax as the
original leaves it.

## 5. The rest

- `Battle_OpenMsgWindow` `0x444310`: `Window_Alloc(0, 3)`, then `0x803162` =
  6, `0x803163` = 0, words `0x803164` = 0x14 and `0x803166` = 0xFFEA. 18
  call sites, each followed by a line.
- `Battle_ReturnQueuedItem(actor)` `0x446EA0`: a member found among the
  queue bytes `0x904ACC` + (`0x904AE2` - 1) up to `0x904AE3`, whose queued
  kind +0x125 is 5 and item word +0x126 has a high byte of 0:
  `0x446D90(+0x12E, +0x126)` counts it back into the inventory list (capped
  at 99). Not kept: the loop index the original leaves in its own argument
  slot, and its eax - its 12 callers pop the slot, and put al over eax before
  passing it to `0x446650`, which reads the low byte.
- `Battle_OpeningMessage` `0x44AA00`: a line at random (`Rand & 1`) from a
  pair picked by the u32 `0x904B90` - 1: lines 0 / 1; 2..4: 2 / 3; 0 or
  above 4: 4 / 0 - through `0x44A6E0(0, 2, 0, 0, 0xFF, text)`. Two of its
  three paths push the id in ecx / edx over a 16-bit `movzx`, so the
  original's id carries `Rand`'s leftovers in its upper half; `Msg_SystemPtr`
  reads the id as a u16, so ours passes it clean (found by the fuzz, which
  first logged the whole dword).
- `Battle_ClearStatus(actor, mask)` `0x44F4B0` (the sibling's "engine
  0x800A0680 on status clear"): the status word (+0x90 / +0x92) loses each of
  bits 0x4000, 0x800, 0x80, 0x40, 0x10, 8, 4, 2, 1 the mask names; with 0x20
  in both, bit 0x20 too and 12 bytes put back at +0x34 from `0x93A034` +
  actor * 0x84; then `Sprite_ReleaseTint(object)`, the word stored,
  `0x446BB0(status)` with `Sprite_Current` the object for the call. Answers
  the word (the original's ax; its upper half is `0x446BB0`'s leftovers,
  read by none of the 23 callers - most are effect handlers whose answer
  `Effect_ApplyResult` drops after `call [0x64E73C + ...]`).

## 6. Found on the way: a defect of Capcom's (for the coordinator to number)

**`BattleStep_Expire4000` reads each enemy's flag-0x4000 counter from the
party array** (latent). The party loop tests +0x142 of the member's object;
the enemy loop tests `0x802D40 + actor * 0x14C + 0x142` - the party array
continued past its three members to actor 3..10 (`0x803266` ..) - while the
byte it then zeroes, and the one `Battle_TickCounters` counts up, is the
enemy's own +0x122 (`0x93B960 + (actor - 3) * 0x128 + 0x122`). The PSX
`0x801D524C` does the same (`lbu 0x5FC2(0x80140000 + actor * 0x140)`), so it
shipped on both. An enemy's flag 0x4000 thus ends when some unrelated byte
past the party objects happens to be 3, not when its counter reaches 3. What
lies at those addresses was not read. Kept as the original has it; control E1
(the enemy's own counter) is refused in 1,888 of 2,000 rounds.

Not written into [`known-defects.md`](known-defects.md): D41 was the last
number and nine sibling groups ran at the same time, so the coordinator
numbers it at the merge.

## 7. The fuzz, and the controls

`BOF3X_SHADOW=battle_setup`, at start-up: nineteen byte-copies
(`battle_setup_fuzz.cpp`, `kClones`), every call out re-aimed at a recorder
(`bof3::CloneCall` with the callee each site was read to call). 2,000 rounds
per function: random bytes over the party objects and the array on to actor
10 with the window fields (`0x802D40..0x803C00`), the saved positions, the
message queue and the enemy objects (`0x93A000..0x93C2C0`), the battle
globals and item queue (`0x904040..0x904C00`) and `Sprite_Current`; then the
branches' boundaries seeded - counters 0..6 and 0xFF, each flag and status
bit alone, the timer 0 / 1 / 2, kind 4 with abilities 0x27 / 0xA3 and their
neighbours, character ids 0 / 6 / 7, weapon 0x52, armour 0x1F, accessories
0x16 / 0x17, max HP around multiples of 20 and 0xFFFF, HP next to the weapon
term, AP next to the cost and costs 0 / 1 / 0xFF, agility words next to
twice the average and to the highest, each mask bit of the standing test
alone, queue ranges empty, inverted and one long with the actor at either
end, before and just past it, actor 3 as a would-be member, `0x904B90`
0..5 and wide values, status masks of one bit. Theirs, then from the same
state ours; the regions, the answer at the width the original defines and
the recorders' log compared.

The recorders are loud where the caller reads after the call: each one
scribbles on a field some caller reads again (flags, statuses, counters, HP
and AP, agility, the timer, the pending word, `Sprite_Current`); the pending
recorder sets its bit as `0x446FB0` does; `0x446540`'s writes the HP change
its callers zeroed first; the roll logs the counter it was handed;
`Sprite_ReleaseTint`'s writes the status word stored after it; `Window_Alloc`'s
the four window fields. Answers in al come with random upper bits.

Result (2026-09-24):

    shadow      battle_setup self-test: 38000 rounds over 19 functions (2000 each), 307157 calls to the stand-ins, 0 MISMATCHES; ...
    shadow      battle_setup coverage: calls skipped 144000, absent 6534, pending 24671, roll 15488, HP change 7776, clear status 1705, standing 22000, outpaces 4529 / 12049, can act 22000, window 10156, lines 12156 (one actor 3817, more 2417), names 2343 / 1474, tints 2000 / 2000, item back 120, rand 2000; answers 1: expire 1941, restore 1263, wakes 892 1596 902 1581, status80 1981, drift 1871, upkeep 1420, faster 1520, standing 712, outpaces 513; drift marks 9383

`BOF3X_SHADOW='*'`: exit 0, every module's self-test passing, 819 ours.

**Eighty-one negative controls**, planted one at a time by a script (not
committed: apply, build, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=battle_setup`,
restore), each build's output read. Eighty are refused by a count, each
only in the function it touches (the wake template's in all four). One, Q4,
is not refused and cannot be: the call happens only when the item word's high
byte is 0, so passing its low byte alone changes nothing; Q4b moves the slot
instead and is refused.

| | Planted | Refused in (rounds of 2,000) |
|---|---|--:|
| A1 | ClearActingFlags: exception ability 0x28 | 110 |
| A2 | ClearActingFlags: exception kind 5 | 51 |
| A3 | ClearActingFlags: actor 2 an enemy | 169 |
| A4 | ClearActingFlags: 0x80 zeroes the 0x40 counter | 806 |
| T1 | TickCounters: flag-1 step below 5 | 149 |
| T2 | TickCounters: party 0x4000 step at most 3 | 153 |
| T3 | TickCounters: status 0x800 read in the low byte | 805 |
| T4 | TickCounters: flag 0x10 cleared at 1 | 929 |
| T5 | TickCounters: enemies cleared without asking | 472 |
| T6 | TickCounters: enemy step at most 3 | 430 |
| T7 | TickCounters: third step reads the counter from before the first | 82 |
| S1 | ActorSkipped: flag 0x20 | 680 |
| S2 | ActorSkipped: timer ignored | 658 |
| S3 | ActorSkipped: actor 2 an enemy | 56 |
| W1 | OpenMsgWindow: 0xFFEB | 2,000 |
| W2 | OpenMsgWindow: Window_Alloc(0, 2) | 2,000 |
| N1 | ActorStanding: party mask without 0x40 | 56 |
| N2 | ActorStanding: enemy mask 0x4944 | 122 |
| N3 | ActorStanding: al alone | 1,626 |
| O1 | PartyOutpaces: twice the average refused when equal | 41 |
| O2 | PartyOutpaces: the highest refused when equal | 52 |
| O3 | PartyOutpaces: al alone | 1,486 |
| F1 | MarkFasterSide: party flag 0x4000 | 1,222 |
| F2 | MarkFasterSide: answer needs two | 864 |
| F3 | MarkFasterSide: average over count + 1 | 1,945 |
| F4 | MarkFasterSide: the highest the lowest | 1,982 |
| F5 | MarkFasterSide: enemy HP for agility | 1,972 |
| C1 | ClearStatus: 0x800 kept | 306 |
| C2 | ClearStatus: copy without the status bit | 335 |
| C3 | ClearStatus: Sprite_Current not put back | 2,000 |
| C4 | ClearStatus: stored before the release | 2,000 |
| C5 | ClearStatus: saved positions by actor + 1 | 793 |
| C6 | ClearStatus: Sprite_Current saved before the release | 2,000 |
| Q1 | ReturnQueuedItem: from 0x904AE2 itself | 44 |
| Q2 | ReturnQueuedItem: to inclusive | 31 |
| Q3 | ReturnQueuedItem: kind 4 | 141 |
| Q4 | ReturnQueuedItem: item byte alone | not refused - changes nothing |
| Q4b | ReturnQueuedItem: slot byte +0x12F | 119 |
| Q5 | ReturnQueuedItem: actor 3 a member | 33 |
| M1 | OpeningMessage: kind 2 for lines 0 / 1 | 249 |
| M2 | OpeningMessage: 5 among 2..4 | 127 |
| M3 | OpeningMessage: sixth line 5 | 726 |
| M4 | OpeningMessage: slot 1 | 2,000 |
| E1 | Expire4000: enemy counter its own (the defect fixed) | 1,888 |
| E2 | Expire4000: HP change from max HP | 1,224 |
| E3 | Expire4000: enemy effect 0x12 | 1,843 |
| E4 | Expire4000: line 0x2E | 1,941 |
| E5 | Expire4000: party counter 2 | 1,502 |
| R1 | Restore800: counter 4 | 1,495 |
| R2 | Restore800: mask 0x8FE | 1,263 |
| R3 | Restore800: AP from max HP | 1,253 |
| R4 | Restore800: one member line 0x2C | 868 |
| K1 | Wake: counter 1 on a hit | 867 / 1,566 / 883 / 1,550 (the four steps) |
| K2 | Wake: the name of the first actor | 499 / 669 / 495 / 627 |
| K3 | Wake: counter up before the roll | 1,464 / 1,951 / 1,489 / 1,937 |
| K4 | PartyWake40: many-line 0x27 | 146 |
| K5 | EnemyWake40: bits 0x60 | 1,749 |
| K6 | PartyWake20: one-actor line 0x29 | 729 |
| K7 | EnemyWake20: the party side | 2,000 |
| X1 | Status80: HP change zeroed after 0x446540 | 1,981 |
| X2 | Status80: bit 0x40 | 1,987 |
| X3 | Status80: line 0x19 | 1,981 |
| H1 | HpDrift: id 6 by -21 | 271 |
| H2 | HpDrift: mode 4 | 975 |
| H3 | HpDrift: cap above HP only | 256 |
| H4 | HpDrift: cap compared unsigned | 267 |
| H5 | HpDrift: id 0 dropped | 93 |
| H6 | HpDrift: second accessory 0x17 dropped | 508 |
| H7 | HpDrift: status 1 takes a quarter | 663 |
| H8 | HpDrift: enemies by HP | 1,907 |
| H9 | HpDrift: 0 marked too | 424 |
| H10 | HpDrift: 0x904B89 at 0x13 | 161 |
| H11 | HpDrift: weapon 0x53 | 1,295 |
| H12 | HpDrift: answer 1 | 129 |
| U1 | ApUpkeep: cost not rounded up | 613 |
| U2 | ApUpkeep: short when equal | 298 |
| U3 | ApUpkeep: 6 4 5 | 265 |
| U4 | ApUpkeep: 0x20 kept | 1,471 |
| U5 | ApUpkeep: effect 0x11 | 827 |
| U6 | ApUpkeep: AP change left 0 | 838 |
| U7 | ApUpkeep: flag 0x20 not tested | 1,035 |

The thinnest are Q2 (31), Q5 (33), O1
(41), Q1 (44), A2 (51), O2 (52). Three were thinner before their seeds
were fixed: the skip recorder first answered "skipped" three times in four
(K4 was 12), actor 3 was never seeded as a would-be member (Q5 was 8 and
first not refused at all), and odd agility words kept twice the average from
meeting it (O1 18).

Not observable, so not controlled: the upper halves `Battle_MarkFasterSide`
carries in its sum and highest (both tests read low words), and the order of
the stores `BattleStep_HpDrift` makes to its change word (nothing reads it
between).

## 8. What the combat route reaches

The combat route (`analysis/combat_catalog.md`, all original) reaches all
nineteen: `Battle_ClearActingFlags` 9 calls, `Battle_TickCounters` 1, each of
the nine steps 1, `Battle_ActorSkipped` 72, `Battle_MarkFasterSide` 2,
`Battle_ActorStanding` 22, `Battle_PartyOutpaces` 6, `Battle_OpenMsgWindow` 1,
`Battle_ReturnQueuedItem` 3, `Battle_OpeningMessage` 1, `Battle_ClearStatus`
3. Which branches ran is not known from counts: one turn's chain with (by the
single `Battle_OpenMsgWindow` call) one line shown, probably the opening
one. The status steps' effects, the defect of section 6, the AP upkeep's
payment and the item return with a found entry are fuzz only unless the
route's fight met those statuses - the batch's memory checks compare the
objects either way. A fight with a status that lasts turns, an AP-paying
state and an item cancelled mid-turn would reach the rest.

For the batch check: all nineteen on the `--original` list, and each on the
trace list with the sizes in section 1.

## 9. Learned about other groups' functions (said, not bound)

- `0x4456C0` (BB): 1 when the actor is not present (+0 bit 0) or has status
  0x4000 (party +0x91 bit 6, enemy +0x93); rewrites its own argument slot.
- `0x445980` (BE): present, none of 0x4944 / 0x4144, flag 0x10 while
  `0x904B8E` runs, then by `0x904AE4` (1 rules out enemies, 2 members, 3
  both).
- `0x446FB0` (BF): `0x904B82 |= 1 << actor`. `0x44A650` / `0x44A6E0` (BF):
  a 12-byte message record at `0x93B8E0` (first free of slots 2..7 / a given
  slot): +0 |= 1, bytes of three arguments, +8 a byte widened, +4 the text
  pointer. `0x446BB0` (BF): a tint (`0x454CC0(Sprite_Current, -6, -10, 0,
  0)`) when the status has bit 0x80.
- Unnamed, in no group: `0x44A910` / `0x44A960` copy a member's (8 bytes, via
  `0x66972C` and `CharacterRecords`) / an enemy's (12 bytes) name into
  `Text_Records[0]` through `0x5171A0`; `0x446CB0` is the status counter's
  roll (counter 3 or more: 75 in 100, else a byte table `0x64E3E8`; then a
  second roll against `0x64E3E0` and the affinity byte +0xB6 / +0xC6);
  `0x446540` sets the HP change to (HP + 5) / 10 under status 0x80;
  `0x445600` is `Battle_PartyOutpaces`' enemy twin; `0x446D90` puts one item
  back in the inventory; `0x430510` is the chain driver (section 1).

# The area's link list and the drop-in party placement (group O)

**Status:** IN PROGRESS (2026-09-22) - five functions ours in
[`src/game/area_entry.cpp`](../src/game/area_entry.cpp), each read to its last
instruction and against its PSX twin, fuzzed against Capcom's at start-up
(`BOF3X_SHADOW=area_entry`, 40,000 rounds, 0 mismatches) with 40 negative
controls: 37 refused by a count, 3 that change nothing. **Not yet through a
live batch, and nothing the attract sequence does reaches any of them**
(section 4). No divergence; no new defect.

Group O of the fourth parallel round
([`takeover-queue-round4.md`](takeover-queue-round4.md)): the two functions
group K left behind a stand-in ([`mode-flow.md`](mode-flow.md) section 1) and
the three the second calls that were still Capcom's. Everything here is a
*faithful* replacement: no `DIVERGENCE.md` entry is owed.

## 1. The functions

Sizes are each body to its last instruction (capstone, 2026-09-22; every jump
inside, the calls listed in `area_entry_fuzz.cpp`); they agree with
`analysis/calltrace/entries_logic.txt`, which already had all five. PSX twins
are `analysis/pairs_propagated.json`'s, each read in the sibling's
`analysis/ghidra/GAME_EMI0_80196800_decomp/`, and the one call whose argument
mattered (below) in the MIPS itself.

| PC | name | bytes | PSX twin | call sites (E8 scan) | what |
|---|---|---|---|--:|---|
| `0x5951D0` | `Area_LinkAt` | `0x177` | `FUN_801A1050` (gap15) | 12 | the area's link at a cell, made the pending area change |
| `0x531F90` | `Party_DropIn` | `0x227` | `FUN_801BF1A8` (call-anchored) | 393 | the party placed from an entry of the area's placement table |
| `0x5321C0` | `Party_SetUpMembers` | `0xE6` | `FUN_801BF56C` (callers) | 1 | the first `count` party records set up for the area |
| `0x5322D0` | `Party_SwapMembers` | `0x10C` | `FUN_801BF730` (call-anchored) | 1 | two party records trade places |
| `0x5323E0` | `ObjTrio_SwapFields` | `0x16A` | `FUN_801BF918` (call-anchored) | 3 | the fields that go with the member, exchanged |

`0x5323E0` was not in the queue: it is `Party_SwapMembers`' one unowned
callee, call-free and fully fuzzable, so it was taken rather than stood in
for; its two other callers (`0x52EE98`, `0x52EECF`) get ours. Every other
callee is already ours and is called by name: `Flags_Test`,
`Area_ClassifyPending`, `Area_PickMusic`, `Kind2_Place`, `Member_ClearState`,
`ScriptContext_Reset`, `Sprite_LoadPalette`, `Field_MemberSprite`.

The queue's "94 call sites" for `0x531F90` is the count of distinct callers
`pc_funcs.py` sees (68 in `analysis/pc_funcs.json` today); a byte scan for
`E8` rel32 finds 393 calls, most in functions that list does not have.

### `Area_LinkAt(x, z)` - `0x5951D0`

The descriptor's `+0x20` points at 12-byte links and `+0x30` holds the index
of the last (so there are `+0x30 + 1`). A link is a run of `+9` cells starting
at (`+0`, `+1`), along z when `+8` is non-zero and along x when it is zero.
The search goes from the last link down and along each run from its far end;
the first link holding (x & 0xFF, z & 0xFF) gives the destination:

- `+2` not `0xFFFF`: area `+2`, x `+4`, z `+6`, flags `+0xA`;
- `+2` `0xFFFF`: word `+4` indexes the descriptor's `+0x24` (pointers to lists
  of 10-byte alternatives) and `+0x28` (their counts); the first alternative
  whose `Flags_Test(Cond_Flags + [6] * 8, [7])` holds, or the one after the
  last when none does - area `+0`, x `+2`, z `+4`, flags `+8`.

Then exactly `Field_ChangeArea`'s four cells: the area word to `0x937F82`,
`(x & 0xFF80) << 8` and `(z & 0xFF80) << 8` to `0x903860` / `0x90384C`, flags
`& 0x8F` to `0x905B88` - plus bits 4-6 of the flags to `0x903851` (the PSX
scratchpad's `0x1F800001`; what reads it is not established) - then
`Area_ClassifyPending` and `Area_PickMusic(x >> 16, z >> 16, area)` of what is
now pending. **Not** `Field_Request = 5`: the caller decides whether to go.
Answers `al` 0 when a link was found, 1 when none. The PSX is the same
statement for statement.

Kept as the original has it: x and z compared as bytes, a run's start plus
its index as an int (a run that would pass `0xFF` never matches there); the
descriptor pointer read once; `Area_PickMusic`'s x and z the pending dwords
shifted arithmetically and read back *after* `Area_ClassifyPending`; its area
the pending word (pushed with a stale upper half that `Area_PickMusic` masks).
The result is `al` alone: of the twelve call sites six test `al` and the
other six (`GameMode_Enter`, ours, among them) ignore the result; none reads
more of `eax`. The original's upper 24 bits are an address in the link list or
whatever `Area_PickMusic` left.

The callers, from the scan: `GameMode_Enter` (at the leader's integer x, z,
after a transition); six in the field's leader movement, `0x52E2F3`..`0x52E4BB`,
on the scratch cell `0x903854` / `0x903856`, which test `al` and set bits of
`Field_ScriptFlags2` or `0x66C7D8` on a 0; and five (`0x41559A`, `0x41BFFA`,
`0x526AC9`, `0x52E6EE`, `0x52E734`) on `Sprite_Current`'s `+0x36` / `+0x3A`,
which ignore the result. The name is by what
the read shows; "link" rather than "exit" because a link need not be walked
through (`hypothesis` for the use, `evidence` for the shape).

**The links as they stand in the image** (walking `Area_Descriptors[0..255]`
through the exe's own data, a scratch script, 2026-09-22): 192 areas have a
list; 1,542 links, 772 along z and 770 along x, 14 of them indirect. **None
of the 1,528 direct links has bit 7 in its flags byte** (the 14 indirect ones'
alternatives were not walked), so `Area_Enter`'s drop-in path (flag `0x80`,
[`mode-flow.md`](mode-flow.md) section 4 step 12) is not entered through a
direct link: it takes a caller that sets `0x80` itself, or an alternative.

### `Party_DropIn(entry)` - `0x531F90`

Entry `entry & 0xFF` of the descriptor's `+0x14` table points at a header
byte: N in the low nibble, bit 5 "set bit 3 of `Field_ScriptFlags`", bits 4-5
both "place the kind-2 object after", and a kind in bits 6-7:

- **0** - exactly N members, else 1. N wanted members follow, each a member
  number or `0x80` for "any record not yet taken". The party list
  `0x904062` is copied to a scratch "free" list; for each wanted member the
  lowest record (`ObjTrio + 0x14C * j`, j below `Field_MemberCount`) holding it
  at `+0x89`, or for `0x80` the first record not struck out, is taken: its
  `+0x2C` byte, its slot, and the free list struck (`0xFF`). A wanted member
  nobody holds is 1; a wildcard with nothing free takes nothing. The party
  list is rewritten from the records in wanted order, and a selection sort by
  `Party_SwapMembers(s, slot[s], 0)` moves the records into it. The set-up
  list starts after the wanted list.
- **0x80** - at least N members, else 1; the set-up list follows the header
  (one byte per member is read from it; the kind-2 byte is still at N).
- **0x40, 0xC0** - no test; likewise.

Then `Party_SetUpMembers(Field_MemberCount, set-up list)`, `Kind2_Place` of the
byte after the set-up list when bits 4-5 are both set, `Field_ScriptFlags |=
0x100`, `Field_ScriptFlags2 |= 0x8000` (byte-wide, as the original), 0.
The PSX `FUN_801BF1A8` is the same; its call to `FUN_801BF56C` loads the
member count into `a0` at `0x801BF4BC` for every kind.

Kept as the original has it:

- The three scratch lists are the PSX scratchpad's `+0`, `+4`, `+8` (PC
  `0x903850`, `0x903854`, `0x903858`): four bytes apart, and N is not limited,
  so a kind-0 entry with N above 4 overruns one list with the next, as on the
  PSX. Kind 0 needs N equal to the member count, and `ObjTrio` holds three
  records, so getting there would take a count the records cannot hold.
- The first byte is written over the argument's low byte. **No caller reads
  that slot afterwards**: a scan of all 393 call sites tracking `esp` from the
  call to the slot's release found every one releases it (`add esp` or a
  `pop`) before any branch, `ret` or read of it, and no call in between takes
  more stack arguments than were pushed since. Ours keeps the byte in a local.
- The result is 1 on a failed kind-0 or kind-0x80 test with the placement
  table's address in the upper 24 bits (1 alone when a wanted member is
  missing), else 0. After `Kind2_Place`, the original's upper 24 bits are that
  callee's leftover `eax`; ours returns 0 there - which the original only
  defines through `Kind2_Place`'s internals, and `Kind2_Place` is ours. 207
  call sites return the value to their own caller; the rest ignore it or test
  `al` (the same scan).

**A misreading the fuzz caught.** The first build had the kinds other than 0
pass the entry number as `Party_SetUpMembers`' count - a reading of the jump
at `0x5321A3` / `0x5321AB` as landing on the `push` at `0x532153`. It lands
on `0x53214D`, the load of `Field_MemberCount`: 3,810 of 8,000 rounds
differed at once, all on the stand-in's recorded count. The candidate defect
it would have been was never written down.

### `Party_SetUpMembers(count, list)` - `0x5321C0`

For each of the first `count & 0xFF` records: made `Sprite_Current`; bit 5 of
`+0` cleared; `+0x5D`..`+0x5F` 0 and `+0x5C` 2; `Member_ClearState(i)`; `+1` 2,
`+7` and `+9` 0; its script pointer `+0x130` from the descriptor's `+0x18`
table at `list[i]`; its script context `+0x124` reset; its palette (`+5`)
loaded into `0x80D380 + +5 * 64` (`Sprite_LoadPalette(.., 0)`). Then the low
three bits of `Field_ScriptFlags2` cleared. The record pointer is kept across
the calls but `Sprite_Current` is re-read after `Member_ClearState` and after
`ScriptContext_Reset`, and the descriptor looked up afresh for each record,
as the original. The count is not limited to `ObjTrio`'s three records.

### `Party_SwapMembers(a, b, keep)` - `0x5322D0`

`ObjTrio_SwapFields(a, b, keep)`; then for record a and then b: made
`Field_State` and `Sprite_Current`; `Field_MemberSprite(+0x89, slot)`; its
`+0x80` (0xA4 bytes) copied from the character record
`0x903A70 + MoveScript_EffectState[+0x89] * 0xA4`; `+0x148` set to
`MoveScript_EffectState[+0x89]` read again **after** the copy, which has just
put the character record's `+9` there. On the PSX the same field is `+0x79`
and the copied byte the record's `+5`: the PC's character record has a 9-byte
name where the PSX has 5 ([`save-interchange.md`](save-interchange.md)
section 2), so both read the record's own member byte - consistent, not a slip.

`MoveScript_EffectState` (`0x66972C`) is, by this use, the member-to-character
index: its first 24 bytes are `0..7`, and eight records of `0xA4` from
`0x903A70` end exactly at `Cond_Flags` (`0x903F90`). The name was given by an
op that indexes it the same way; a rename is left to whoever owns that entry.

### `ObjTrio_SwapFields(a, b, keep)` - `0x5323E0`

Records `a & 0xFF` and `b & 0xFF` exchange the member `+0x89`; unless `keep`'s
low byte is set, also `+0x08`, `+0x29`, `+0x138` and the position dwords
`+0x34` / `+0x38` / `+0x3C`; always the dwords `+0x0C`..`+0x14` and the script
context's first two bytes `+0x124` / `+0x125`; then `+0x4B` of both `0xFF`.
The PSX's offsets are the same shape, the tail `0xC` lower (`0x140`-byte
records).

## 2. How the callers were checked

`scratchpad` scripts, 2026-09-22, over `.text` for `E8` / `E9` rel32 to each
entry: the twelve `Area_LinkAt` sites each followed to the first instruction
that reads or overwrites `eax` (six `test al, al`, six no read); the 393
`Party_DropIn` sites classified the same way and by the argument-slot scan in
section 1. No `E9` tail jumps to either.

## 3. The start-up fuzz

`BOF3X_SHADOW=area_entry` (`src/game/area_entry_fuzz.cpp`): byte copies of all
five, every call out re-aimed at a recording stand-in (for our own three
callees too, so each function is tested alone). `Area_Descriptors[0..3]` point
at four descriptors of the fuzz's own for the duration - one shared link list
of 17, sixteen placement entries, sixteen alternative lists, and a script
table per descriptor, so that a descriptor looked up once where the original
looks it up afresh shows. Compared: `ObjTrio` and three records past it,
`0x903840..0x90387F` (the pending x / z and the scratch lists),
`0x904060..0x90407F`, the eight character records, the pending area and
flags, both script-flag words, `Sprite_Current`, `Field_State`, the member
count, the area number, the descriptor slots, the fuzz's own data, the result
and the stand-ins' log.

Seeded: link runs of length 0..8 and 255-ish starts (the `0xFF` edge), cells on
a run's last cell, one past it, one before it, or anywhere; indirect links
with 0..7 alternatives; kinds 0 / 0x40 / 0x80 / 0xC0 with N 0..6 and 15 and a
member count of N, N - 1 or N + 1; wanted lists of wildcards, held and unheld
members; party lists with struck entries; swap slots 0..5 with a == b one
round in five and `keep` 0, a dword with a zero low byte, or anything; every
byte argument with stale upper bytes half the time.

The stand-ins are louder than the real callees where the caller reads after
them (Traps): `Area_ClassifyPending`'s moves the pending cells,
`Member_ClearState`'s and `ScriptContext_Reset`'s move `Sprite_Current`, its
palette byte and the area number, `Field_MemberSprite`'s moves `Field_State`
and the member byte, `Party_SwapMembers`' rewrites the slot list and the member
count, `ObjTrio_SwapFields`' the member bytes; `Flags_Test`'s returns `0x100`
for "false" to check that only `al` is tested.

**Result:** 40,000 rounds (8,000 per function), 101,441 calls to the
stand-ins, 0 mismatches; with `BOF3X_SHADOW='*'`, every module's self-test 0
mismatches and 471 functions ours.

### Negative controls

Planted one at a time in `area_entry.cpp`, built, self-tested, restored
(`build/controls.py`, not committed). 40 controls: **37 refused** by a count,
**3 not** - each a change that changes nothing (Traps).

| # | planted | refused |
|---|---|--:|
| O1 | links searched from the first up | 155 |
| O2 | a run searched from its start | **0** - one index at most can match a cell |
| O3 | a run one cell short | 1,093 |
| O4 | x masked to 9 bits | 787 |
| O5 | a run's z compared as a byte (wraps) | 154 |
| O6 | the direct x masked `0xFF00` | 1,062 |
| O7 | flags `& 0x0F` | 1,505 |
| O8 | the link's high bits `& 0xF` | 1,505 |
| O9 | `Area_PickMusic` before `Area_ClassifyPending` | 3,064 |
| O10 | no alternative holds: the last instead of the one after | 423 |
| O11 | "not found" answers 0 | 4,936 |
| O12 | x shifted logically for `Area_PickMusic` | 508 |
| O40 | z unmasked | 1,512 |
| O13 | kind 0x80 refuses a count equal to N | 727 |
| O14 | a wildcard takes a struck-out record | 1,037 |
| O15 | the sort's fix-up searched from s + 1 | **0** - slot s cannot hold s there unless the swap rewrites the scratch list, which the real one does not |
| O16 | `Kind2_Place` of the next byte | 1,430 |
| O17 | bit 3 on header bit 4 | 1,399 |
| O18 | a failed test without the table address | 558 |
| O19 | the member count taken before the swaps | 156 |
| O20 | the slot list not re-read after a swap | 35 |
| O21 | a wanted member from the highest slot | 443 |
| O22 | the party list not copied to the free list | 1,755 |
| O23 | `Field_ScriptFlags2`'s `0x80` in the low byte | 4,326 |
| O38 | the sort's swap with its arguments reversed | 691 |
| O39 | the sort swaps a slot already in place | 612 |
| O24 | the palette through the record, not `Sprite_Current` | 5,387 |
| O25 | the descriptor looked up once | 2,602 |
| O26 | `Field_ScriptFlags2 & 0xFFF0` | 4,071 |
| O27 | `+1` through the record after `Member_ClearState` | 3,927 |
| O28 | bit 4 of `+0` cleared instead of bit 5 | 6,437 |
| O29 | `+0x148` from the member before the copy | 6,458 |
| O30 | `Field_State` not re-read after `Field_MemberSprite` | 2,471 |
| O31 | the character copy one dword short | 8,000 |
| O37 | the member passed to `Field_MemberSprite` off by one | 8,000 |
| O32 | `keep` tested as a dword | 1,212 |
| O33 | `+0x125` not swapped | 4,786 |
| O34 | `+0x4B` set on a only | 4,792 |
| O35 | the position swapped even with `keep` | 1,210 |
| O36 | a swap writing b before a | **0** - with a == b both writes store the one value read |

Counts are rounds of 8,000 for the function concerned. Not planted: a count or
slot mask widened on `Party_SetUpMembers` / the swaps - with stale upper bytes
that writes far outside the regions the harness restores (Traps: a harness
that restores too little).

## 4. What nothing reaches yet

**None of the five is in any trace**: `hidden_b`, `all_a` and `all_b`
(`analysis/calltrace/*/bof3x.callcounts.tsv`) have no entry for any of them.
The attract sequence changes areas only through transitions that do not walk
a link at the leader's cell, and never places a party by entry. So the live
batch's attract, oracle and frame-hash runs will show only that nothing else
moved; they cannot confirm these five. What would reach them:

- **`Area_LinkAt`**: walking the leader through a door or an area edge in the
  field - the leader-movement callers at `0x52E2F3`..`0x52E4BB`. A recipe that
  walks save 5's leader out of an area (`tools/recipes/field_view.txt` walks,
  but not across an edge) and a traced run of it
  (`BOF3X_CALLTRACE_MODE=all`) would count it; then the usual A/B of captures
  across the change.
- **`Party_DropIn` and its three**: any scenario event that re-places the
  party (most of the 393 sites are in chapter scripts), or an area change with
  flag `0x80`. A new game's opening is the likeliest cheap reach; a traced
  run of the new-game capture (there is no recipe for it in `tools/recipes/`
  yet) would say whether it gets there.
- **The indirect links** (14 in the image) and **kinds 0x40 / 0xC0**: fuzz
  only, until a trace names an area that has them on its path.

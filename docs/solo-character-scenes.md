# The solo-character mode's scene audit (group SA)

**Status:** IN PROGRESS (2026-11-12, branch `solo/sa` off the round branch's
`fd017aa3`). Every site below was found by `grep` of `src/game/` and read;
none was reached in a run. The guards of section 3 are built, self-tested
headless and argued from the code; the owner has not played them. Section 4's
class is open: it needs the owner's call. The round doc is
[`solo-character-round.md`](solo-character-round.md) (sections 0a, 3, 6a,
6c); the fold this audits is [`solo-character-field.md`](solo-character-field.md)
(its section 8 was the starting list). The ledger entry is DIV-0228
`SoloCharacter`; this group's text for it is drafted in the hand-back, not
numbered here.

Code: `src/game/solo/solo_scenes.{h,cpp}` (the guards and their self-test),
and the call sites named in section 3.

## 1. What the fold changes for a scene

While `solo::Solo_Active()`, the field half's fold keeps the map team at
every member marked "must stay" (record 0, the hero, from New Game) and at
most one other (`solo-character-field.md` 1). It runs in `Field_LeaderStand`
after its first test, `Field_ScriptFlags & 0x100`, and `ScriptFlags_Set40`
(`event_cond.cpp:869`) sets exactly that bit: **a scene that has called
`Set40` is never folded under**. A scene that rebuilds its own party (the
call-table builders of [`scena_calls.md`](scena_calls.md): the count to 0,
then `Party_Join` of three) walks three until it gives control back. What a
scene can find is therefore the fold's team **at the moment it starts**: two
walkers (or one), the first party list `7 X FF`, the ObjTrio record past the
count turned off by `Party_Remove` but **still holding its old bytes** -
`+0x89` the id of whoever was last copied there, `+0x137` its last walk-in
byte.

Three kinds of read go wrong on that:

1. **A count tested for three**, or a branch chosen by the count.
2. **A slot read by index past the count** - Capcom's party helpers read
   `ObjTrio[0..2] +0x89` or the three list bytes whatever the count, and wait
   on `ObjTrio[1|2] +0x137`.
3. **A placement that needs three** - `Party_DropIn` kind 0 (exactly N, the
   wanted ids present) or kind 0x80 (at least N) answers 1 and places nobody
   (`field_6.cpp:1067`). Section 4.

And one engine fact turns kind 2 into memory damage: **`Party_Remove`
(`0x534030`) of an id that is not walking still takes the last walker off**
(`field_6.cpp:2220`): it searches both lists up to the count, uses the count
itself as the slot when the id is absent (writing `0xFF` one byte past the
walkers), turns ObjTrio record `count - 1` off and lowers the count. At count
0 it writes `ObjTrio[-1] +0` and `Member_ClearState(0xFF)` (`mode_11.cpp:236`,
`ObjTrio + 0xFF * 0x14C`), and the count wraps to 255. Capcom's helpers only
ever met three walkers, so they never passed it an absent id.

## 2. The audit

Classes: **SAFE** - the same outcome as with the party the scene was written
for; **COSMETIC** - a different, harmless branch (what the player sees is
said); **BREAKS** - a hang, a crash, memory written out of place, a wrong
story branch, or a member a scene waits on missing. **DROPIN** marks a site
whose outcome depends on section 4's open class. Line numbers are of
`fd017aa3` plus this branch.

### 2a. Walker-count tests in scene code (`Field_MemberCount`)

| # | Site | Tests | Folded team (count 2) | Class |
|---|---|---|---|---|
| A1 | `scena_01_2.cpp` `Scene17Leave` (~631) | count 2 or 3, else waits | its own two-member area change (0xA, `0x88`) | SAFE (the fold never makes 1) |
| A2 | `scena_01_2.cpp` `Scena01_Scene11` step 5 (~2044) | 3 → entry `0x83`, else `0x84` | `0x84`: area 5 entry 4, kind 0 for ids 3 and 0 | SAFE while the team is 0 and 3 (the fold keeps id 3 from chapter 1's list `0 3 4`); DROPIN after an L-swap bring-out |
| A3 | `scena_01_2.cpp` `Scena01_Scene11` step 0x17 (~2066) | **waits for count 3**, then entry `0x85` | waits for ever behind the inn's fade-out (nothing in the scene changes the count) | **BREAKS → fixed** (3.1) |
| A4 | `scena_02_2.cpp` step 0x14 (~772) | 3 → `0x85`, else `0x8B` | its own two-member entry | SAFE / DROPIN |
| A5 | `scena_02_2.cpp` `Scena02_Scene0B` step 0x17 (~1268) | 3 → (5, `0x85`), else (5, `0x500000`, `0x50000`, `0x86`) | its own two-member branch | SAFE / DROPIN |
| A6 | `scena_02_2.cpp` `Scena02_Scene0F` step 4 (~1478) | 3 → `0x8E`, else by leader `0x8C` / `0x8D` | its own two-member entries | SAFE / DROPIN |
| A7 | `scena_02_2.cpp` `Scena02_StepHook` area 0x1B (~2618) | a scene (run 0xA) only with **exactly two** walkers and flag 0xF clear | fires at a visit where the original walked three and did not fire | **open** (story order; 5.1) |
| A8 | `scena_06_3.cpp` `SortParty` (~338) with `boss_25.cpp` `Boss25_Exit` (~217) | sorts **all three** records by `+0x89`; the exit sets the count to **3** | the off record's stale id sorts it into a walker's place; the exit leaves a count of 3 over two walkers | **BREAKS → fixed** (3.2) |
| A9 | `scena_07_2.cpp` `Scena07_PartyHas89State2` (~1316), from `Scena07_StepHook` and `Scena10_StepHook` area 0x52 | a walker with id 2 | as the original without id 2 in the party: the bump back (counter 0 = 0x10) | COSMETIC (the player brings id 2 out with L, as the original asked them to put it in the party) |
| A10 | `scena_10.cpp` `Scena10_Party75` (~55) | leaves by count | resets the count and joins anew | SAFE |
| A11 | `scena_10_3.cpp` `MsgByMember` (~253) | the first of N ids walking | the line for the walker present | COSMETIC |
| A12 | `scena_10_3.cpp` `Run7Body` step 0 (~731) | a walker in state 3 | over the walkers | SAFE |
| A13 | `scena_10_3.cpp` `Run7Body` step 1 (~745) | saves the walkers' ids to `0x903A10` | two saved; the third byte left as an **earlier save** wrote it, which `Scena10_RestoreSaved` then loads and joins | **BREAKS → fixed** (3.3) |
| A14 | `scena_10_3.cpp` `Scena10_StepHook` area 0x78 (~2006) | a walker with id 2 | no scene until id 2 walks | COSMETIC |
| A15 | `scena_14.cpp` `Scena14_RecountLeaveAll` (~199) | recounts by `+0` bit 0, leaves each id not 7 | stale ids are absent or repeat a walker's | SAFE with 3.4's guard (BREAKS without it) |
| A16 | `scena_14_2.cpp` `RestoreRecords` (~437) | the walkers' records refreshed | over the walkers | SAFE |
| A17 | `scena_14_2.cpp` step 0xB (~1247) | id 4 walking → entry 6, else 7 | the entry for the team present | COSMETIC / DROPIN |
| A18 | `scena_14_2.cpp` `Scena14_TalkByMember` (~1415) | the first of four ids walking | that walker's line, or none | COSMETIC |
| A19 | `scena_calls.cpp` `scena_calls_SaveAndLeave` (~96; chapter 9 B[6], 10 B[3]) | saves the walkers, then leaves the first **three local bytes** not 7 | the local's third byte is the caller's `ecx` (0), the saved third stale; a leave of an absent id takes the hero off (count 0) | **BREAKS → fixed** (3.3, 3.4) |
| A20 | `scena_calls.cpp` `scena_calls_LeaveAllParty7` (~123; chapter 15 A[0]) | leaves the three local bytes, 7 too | the third leave at count 0: `ObjTrio[-1]` and `Member_ClearState(0xFF)` written | **BREAKS → fixed** (3.3, 3.4) |
| A21 | `area_103.cpp:69`, `area_112.cpp:81`, `area_143.cpp:84`, `area_145.cpp:100`, `area_148.cpp:137`, `area_173.cpp:81`, `area_196.cpp:41`, `area_197.cpp:74` | a message by the first of N ids walking | that walker's line, or none (no caller waits on it) | COSMETIC |
| A22 | `area_108.cpp:85`, `area_112.cpp:206`, `area_135.cpp:869`, `area_140.cpp:272`, `area_52.cpp:348`, `area_85.cpp:240`, `area_45.cpp:377` | a walker in a rectangle / a step ahead / on a moving block | over the walkers (the leader included) | SAFE |
| A23 | `area_117.cpp:99`, `area_118.cpp:93`, `area_169.cpp:90`, `area_171.cpp:123`, `area_49.cpp:154` | the member frames: each walker's zones | over the walkers | SAFE |
| A24 | `area_172.cpp:130` | id 4 walking → the script 3 bytes on | the other branch until id 4 walks | COSMETIC |
| A25 | `area_192.cpp:314` | the walkers' records refreshed | over the walkers | SAFE |
| A26 | `area_85.cpp:115` | an object follows the walker with id 2 | it does not move until id 2 walks | COSMETIC |
| A27 | `scena_08_2.cpp` case 0xE / 0x1B (~1015, ~1088) | saves the second list's first two, `Scenario_CallB(0)` (id 2 leaves), later `CallA(2)` (id 2 joins), the pair back, `ObjTrio[1]` to the saved place | id 2 not walking: the leave is a no-op (3.4), the join adds it, the kept walker stands where id 2 stood | COSMETIC (SAFE when the kept walker is id 2) |
| A28 | effects, magic, menus over `Field_MemberCount` (`effect_kind_*`, `kaiser.cpp`, `master_talk*.cpp`, `master_quit.cpp`, `field_menu.cpp`, `item_trade.cpp` `Area_Enter`'s walk, `member.cpp:412`, `field_3.cpp:595`) | loops over the walkers | over the walkers; the masters' screens list walkers only, so a reserve member is brought out to meet a master | SAFE (the masters: COSMETIC, MS's screens) |

### 2b. `Party_Count`

| # | Site | Folded team | Class |
|---|---|---|---|
| B1 | `area_189.cpp:179` (the desert's encounter pace) | a reserve member's accessory (equip 3 kinds 0x15 / 0x14) no longer doubles or halves the pace | COSMETIC (pace; ledger note) |
| B2 | `field_4.cpp:634` (walk or run by a walker's `+0x10` bit 7), `field_6.cpp:2689` (poison's field HP drain), `field_6.cpp:1663` / `:1706`, `field_4.cpp:1761` | over the walkers: a reserve member is not slowed by, nor drained on the field by, its status | COSMETIC (field rules for the reserve; ledger note) |
| B3 | `core/sound.cpp:1169`, `shop/field_save.cpp:805` (a load sets the count from the list) | the saved list | SAFE |
| B4 | `field_ability*.cpp`, `field_menu*.cpp`, `item_use.cpp`, `shop_*.cpp`, `party_form.cpp`, `config_menu.cpp` | menus over the walkers | not scenes: MS's (out of scope) |
| B5 | `core/transition.cpp:997`, `encounter.cpp` `party_count(1)` | the battle's count | CB's (out of scope) |

### 2c. Party list reads (`0x904062..0x904067`) and the leader's id

| # | Site | Folded team | Class |
|---|---|---|---|
| C1 | effect spawns by a slot's list byte or record: `area_11.cpp:46` (list byte 2), `area_117/118/119` `SpawnAtMember`, `area_130/133/134` `SpawnAtMember`, `area_136.cpp:129..165` (slots 1, 2), `area_39.cpp:102/105`, `area_49.cpp:65`, `area_68.cpp:75`, `area_77.cpp:86`, `area_w3c.cpp:99`, `area_92.cpp:143..163`, `area_94.cpp:135..178`, `area_135.cpp:485` (list byte 1) | for a slot past the count: the effect at the off record's last screen place, its sub-argument the table byte at `0xFF` (each read is unchecked into `.data` by the symbols' evidence; none aborts) | COSMETIC (a stray effect where a walker stood) |
| C2 | leader-id tables: `scena_01_2.cpp:2267`, `area_26.cpp:140`, `area_57.cpp:61`, `scena_05_2.cpp:1747` | the bring-out leads only joined, unmarked records, whose ids these per-character tables cover (the original's members screen let the same characters lead) | SAFE |
| C3 | leader-id branches: `scena_02_2.cpp` `LeadMember` (551, 1056, 1374, 1400, 1405, 1419, 1481, 1956, 2066, 2270, 2293), `scena_05_2.cpp` `MemberMessage` / `PartyMessage` / `PartyGate` (440, 470, 479), `area_91.cpp:143` (a master's test by leader 6 or 3), `area_39.cpp:195`, `scena_13_2.cpp:1374`, script condition 11 and variable 11 (`event_cond.cpp:658`, `:698`) | a Capcom-written branch for whoever leads | COSMETIC (1374 / 1400 pick an entry: DROPIN) |
| C4 | `area_49.cpp:271` / `:277` (counter 1 / 2 = 1 when list byte 1 / 2 is id 4) | byte 2 is `0xFF`: counter 2 not set | COSMETIC |
| C5 | `scena_05_2.cpp` `PairCode` (~565; run 16 steps 5, 6) | the second list's first two must be one of six pairs (two tables chosen by the answer of a choice); no match leaves the step for ever | **open** (5.2) |
| C6 | `scena_13.cpp` `Scena13_SaveParty7` / `Scena13_RestoreSaved` | the folded lists saved (`7 X FF`); the leave of `0xFF` and the join of `0xFF` | **BREAKS → fixed** by 3.4 (no edit at the site) |
| C7 | party builders that write the lists (`scena_04.cpp:33`, `scena_13.cpp:87`, `scena_14.cpp:115`, `scena_call.cpp:37`, `scena_calls.cpp:69`, `scena_16.cpp:33`) | rebuild | SAFE |
| C8 | script condition 15, `EventCond_RecordBit0` (`event_cond.cpp:676`) | a reserve member reads as with the party | SAFE: bit 0 is "joined", and the original's reserve (the members screen's) read the same |

### 2d. Walker slots read by index, and the leave / save / restore helpers

| # | Site | Folded team | Class |
|---|---|---|---|
| D1 | waits on `ObjTrio[1|2] +0x137`: `scena_02_2.cpp:1568`, `scena_09_2.cpp:1151`, `:1306`, `:1508`, `:1609`, `:1637`, `scena_12_2.cpp:1119`, `scena_13_2.cpp:1244-1245` | the off record keeps its last walk-in byte (`Area_EntryWalk` writes only records below the count, `item_trade.cpp:1081`); a stale 2 / 3 / 7 or non-zero waits for ever | **BREAKS (when stale) → fixed** (3.5) |
| D2 | `scena_07.cpp` `Scena07_LeaveThird` (chapter 7 B[0]) | the "third member" is the off record's stale `+0x89` - after a compaction that is a walker's id, possibly the hero's | **BREAKS → fixed** (3.3) |
| D3 | `scena_10_2.cpp` `Scena10_LeaveAll`, `scena_13.cpp` `Scena13_LeaveAllParty7` | the stale third id: absent (the last walker taken off, or memory written at count 0) or a walker's repeated | **BREAKS → fixed** by 3.4 |
| D4 | `scena_14.cpp` `Scena14_SaveLeaveAllPartyA`, `Scena14_SaveLeaveAllParty7` | the stale third id saved, then restored by `Scena14_Party74First` / `_Party74Other` | **BREAKS → fixed** (3.3) |
| D5 | `scena_09.cpp` `ScenaCall_Party72Saved`, `scena_14.cpp` `Scena14_Party74Other`, `scena_10_2.cpp` `Scena10_RestoreSaved`, `Scena14_Party74First` | a saved third that is stale (a wrong member joins, its joined bit set; or a party-set triple no row holds, `PartySet_ErrorLoop`, which never returns) | **BREAKS → fixed** (3.3, 3.4) |
| D6 | writes to an off record: `scena_03.cpp:184`, `scena_08_2.cpp:269`, `:1092`, `scena_13_2.cpp:1011`, `scena_14_2.cpp:340`, `:821`, `boss_nina.cpp:543` | a write to a record nothing draws; `Party_Join` rebuilds it | SAFE |
| D7 | `scena_03.cpp:491` (the kind-2 object to `ObjTrio[1]`'s place) | chapter 3's party is the game's own | SAFE |

### 2e. `Party_DropIn` (211 references outside the fuzz files)

Every call whose area entry is kind 0 with N or kind 0x80 with N, N above
the team's count or naming an id the team lacks: **BREAKS (a hang where the
scene waits on a counter the placed members' scripts set)**; section 4 has
the evidence and the options. Kind 0x40 / 0xC0 entries (no test) and entries
the team satisfies: SAFE.

### 2f. Counts

| Class | Rows | Sites (a row may hold many sites) |
|---|--:|---|
| SAFE | 19 | A1, A2, A4, A5, A6 (each its own two-member branch; DROPIN inside), A10, A12, A15 (with 3.4), A16, A22, A23, A25, A28; B3; C2, C7, C8; D6, D7 |
| COSMETIC | 14 | A9, A11, A14, A17, A18, A21, A24, A26, A27; B1, B2; C1, C3, C4 |
| BREAKS, fixed | 11 | A3, A8, A13, A19, A20; C6; D1, D2, D3, D4, D5 |
| BREAKS, open (design call) | 1 class | 2e / section 4 (`Party_DropIn`): A2, A4..A6, A17 and C3's entry picks fall in it when the entry misses |
| Open (needs the game's text or the owner's play) | 2 | A7, C5 |
| Out of scope | 2 | B4 (MS), B5 (CB) |

## 3. The guards as built

Every guard asks `solo::Scene_Active()` (SoloCharacter on and
`Solo_Active()`); off, each returns the raw value or "no" and the site runs
the original's bytes of behaviour. With three walkers every guard gives the
original's answer: they differ only for a slot past the count, or an id that
walks nowhere.

### 3.1 Chapter 1's inn, step 0x17 (`scena_01_2.cpp`, A3)

**Why the original tests 3.** Run 0x11 is the inn: object handler 0xF starts
it at step 0, handler 0x10 at step 5 (`Scena01_Object0F` / `10`). Step 0's
path (0x14 heal and fade, 0x15 / 0x16 the jingle, 0x17) has only a
three-member area entry (`0x85`); nothing on it changes the count, so the wait
is a guard, not "wait for a restore" (`PartyRestore` here is
`Party_HealJoined`, `scena_sc1_callees.h:63`: a heal). Step 5's path goes to
the same place, (`0x510000`, `0x80000`), with `0x83` for three and `0x84` for
fewer.

**The fix.** In the mode, a count of 2 goes on with step 5's own two-member
entry for the same place (`0x84`); 3 keeps `0x85`; any other count waits as
the original. Area 5's entry 4 is kind 0 for ids 3 and 0 (the exe's table,
section 4's scan), which the fold's chapter-1 team is (list `0 3 4` keeps 3);
after an L-swap bring-out (`0 4`) it is section 4's class.

### 3.2 Chapter 6's `SortParty` and `Boss25_Exit` (A8)

`SortParty` (run 8 step 0) exchange-sorts the three ObjTrio records by
`+0x89`, saves the second list's `count` bytes and the count (`0x939A02`),
and cuts the party to one; `Boss25_Exit` copies `n` bytes back and sets the
count to **3**. With three walkers both agree. With two, the off record's
stale id can sort it into slot 0 or 1 and a walker out of the count, and the
exit leaves a count of 3 over two walkers (the fold then refuses the party:
a first-list `0xFF` below the count). Chapter 6's own "must stay" mark is
`Scena06_Run12`'s (M7), not run 8's, so the team at run 8 can be two.

**The fix.** In the mode with fewer than three walkers, `SortParty` sorts
the walking records only, and `Boss25_Exit` puts back the count `SortParty`
saved. **This touches `boss_25.cpp`, beside CB's battle restore** (M4: CB's
restore must keep `Boss25_Exit`'s writes); the coordinator should check the
two together.

### 3.3 The party helpers' slots and saves (A13, A19, A20, D2, D4, D5)

`Scene_MemberId(slot, raw)` is `0xFF` for a slot past the count;
`Scene_PadSaved(saved, n)` writes `0xFF` past `n`; `Scene_SavedNone(id)` is
"a saved `0xFF`". Applied where a helper reads the three slots or saves them:

- `scena_calls_SaveAndLeave`: the saved bytes past the count padded; the
  local's bytes past it (the caller's `ecx`) `0xFF`.
- `scena_calls_LeaveAllParty7`: the local's bytes past the count `0xFF`.
- `Run7Body` step 1 (`scena_10_3.cpp`): the tally padded.
- `Scena07_LeaveThird`: `Scene_MemberId(2, ...)`.
- `Scena14_SaveLeaveAllParty7`: the saved slots through `Scene_MemberId`.
- `Scena14_SaveLeaveAllPartyA`: the slots through `Scene_MemberId`; with no
  walker neither 7 nor 4 the saved byte is `0xFF`, not an earlier save's.
- `ScenaCall_Party72Saved` and `Scena14_Party74Other`: a saved `0xFF` is
  passed over as 7 / 2 and 7 / 4 are, so the restored third is the walker the
  save held (or Capcom's default 8).

`PartySet_Load` takes `0xFF` as "none" already (`PartySet_Find`,
`mode_11.cpp`); every pair with id 7 that a row holds is found by its search
(a replay of `PartySet_Find` on the exe's 19 rows, all pairs of ids 0..10:
29 pairs held, one missed by the narrowing, `(4, 9)`, which holds no 7).

### 3.4 `Party_Remove` and `Party_Join` (`field_6.cpp`)

- `Party_Remove`: an id in neither list below the count removes nothing
  (after `Party_JoinReset`, the original's first step). Covers every leave of
  a slot the fold emptied, a padded `0xFF`, an `ecx` byte, and a scene's
  leave of a member who is in the reserve (the original's path would take
  the last walker off instead). A line is logged.
- `Party_Join`: a byte past `Character_IndexById`'s 24 joins nothing and
  answers 0. Only a padded `0xFF` reaches it. A line is logged.

The fold's own `Party_Remove` calls name walking ids and are unchanged. The
second-list case (`SortParty`'s state: the id in the second list only) runs
the original's.

### 3.5 Waits on a walker's walk-in byte (D1)

`Scene_SlotPose(slot, raw)` is 0 for a slot past the count - a walker that
is not there has nothing left to walk in. All eight waits treat 0 as done
(four wait while 2 / 3 / 7, `Scena13_Run8` until both are 0).

### 3.6 The self-test

`solo scenes self-test: 70 checks passed` (`SoloScenes_SelfTest`, run from
`Field6_Inject` under `BOF3X_SELFTEST_ONLY`): every slot and count with the
mode off and on, the pad, `Party_Remove`'s and `Party_Join`'s tests (mode
off, walking ids, a reserve id, an `ecx` 0, a padded `0xFF`, the second-list
case, a count past three), and a folded team carried through a save, a
leave-all and a restore as chapter 9 / 10 run them, on state of its own.

## 4. The open class: `Party_DropIn` with a team smaller than its entry

**What the code does.** `Party_DropIn(entry)` reads entry `entry` of the
area descriptor's `+0x14` table. Kind 0 needs `Field_MemberCount == N` and
each wanted id walking; kind 0x80 needs at least N. On a miss it returns 1
and does nothing: no member is placed, **no member gets the entry's script**
(`Party_SetUpMembers`: each record's `+0x130` from the descriptor's `+0x18`
table), and `Field_ScriptFlags` bit 8 / `Field_ScriptFlags2` bit 15 are not
set. Of the scene sites read, one tests the answer (`scena_15_2.cpp:586`);
area handlers hand it to their callers ([`area-entry.md`](area-entry.md) 1).

**Why a miss hangs.** The scenes wait on the script counters (`0x903848`),
and the placed members' scripts are what set them (move-script ops
`A0`/`A4`/`A8`/`AC`, counter `(op >> 2) & 3` = the next byte; symbols.toml's
evidence for `MoveScript_CounterOps`). One instance, by reading and a byte
scan of the exe's data (not run): chapter 12, area 0x86 (134). Entering it
with flag 0x2F clear starts run 9 at step 0 with `Set40` and nothing else
(`scena_12_2.cpp:252-258`: no party rebuilt). `Scena12_Run9` step 0 waits on
slot 2's walk-in byte, calls `Party_DropIn(0)` and step 1 waits for counter
0 to be 1. Area 134's entry 0 is **kind 0x80, N 3**; its first member's
script sets counter 0 to 1 two bytes in. With the fold's two the drop-in
misses, counter 0 stays 0, and step 1 waits for ever with bit 8 held: the
player cannot move.

**How many entries.** A scan of every area's table in `BOF3.exe` (a
scratch script beside this round's runs; table ends found by shape, so a
heuristic): 955 entries read as valid, of which kind 0 N 3 with wanted ids
222, kind 0 N 3 with wildcards 134, kind 0x80 N 3 97 - **453 entries a
team of two misses** - and kind 0 N 2 for named ids 75 (missed unless the
kept walker is the one named; chapter 1 and 2's are mostly ids 0 and 3).
Kinds 0x40 / 0xC0 (no test): 172. Which of them a scene reaches without
rebuilding its party first is not counted: a scene that rebuilds (the count
cleared, three joined) is protected by its own `Set40`.

**The options (the owner's call; nothing built).**

1. **Un-fold at the drop-in.** In the mode, when a kind-0 / 0x80 entry
   wants more walkers (or ids) than walk, bring joined reserve members onto
   the map first - the wanted ids, then for wildcards the members the fold
   dropped (it would have to remember them) or record order - loading a party
   set that holds them (`FindRow` first: a miss would be `PartySet_ErrorLoop`),
   then run the original test. The fold folds back at control's return.
   Cost: the entry's members appear in their scene places (where a drop-in
   puts them anyway); a set load mid-scene (FS measured 0 frames, 0.4 to
   4.4 ms); FS's "the fold never adds a member" is given up inside scenes;
   wildcards pick who speaks in scenes written for the player's choice; a
   wanted id the story has away (bit 0 clear) misses as in the original.
   Covers A2, A4..A6, A17, C3's entry picks and every 2e site.
2. **Fold for the eye only.** Keep three walkers in the lists and the count,
   and hide the walkers past the rule (not drawn, not colliding) instead of
   `Party_Remove`. Every scene read is then the original's; a drop-in shows
   the members it places (`Party_SetUpMembers` clears `+0` bit 5). Cost: FS's
   fold and bring-out rebuilt on another mechanism; the L swap's bring-out
   would trade a hidden walker for a visible one; the reserve is then "the
   joined members not in the three", as the original's.
3. **No map fold.** The map team stays the game's; the champion fights
   alone (CB) and nothing else changes. Cost: 6a item 1's "hero and at most
   one other" walking is dropped. No scene risk at all.

The guards of section 3 stay right under 1 and harmless under 2 and 3 (they
change nothing with three walkers).

## 5. The other open sites

### 5.1 A7: chapter 2's two-walker trigger in area 0x1B

`Scena02_StepHook` starts run 0xA only when exactly two walk and flag 0xF is
clear; run 0xA sets 0xF at its end. Area 0x1B's entries are mostly kind 0 for
ids 0 and 3 (one is for 0, 4, 3), so the original walks two there except at
one visit with three. With the fold, the scene can play at that visit - before
the story order the original keeps. A mode-aware test needs the count the
original would have had: with chapters 1 and 2's at most three joined, every
joined member walks in the original, so `min(3, joined)` is it there - but
not after a game-made small party (a scene that leaves members joined while
walking one), so it is not a general rule. Left unchanged; the owner's play
through chapter 2 with the mode on, past area 0x1B twice, settles whether it
matters.

### 5.2 C5: chapter 5's pair code

`Scena05_Run16` step 4 branches on a choice's answer (1 → step 5, 2 → step
6); each step looks the second list's first two up in its own six pairs (ids
0, 1, 5 or 0, 1, 6) and stays at the step for ever on no match. With the
fold, the pair is the hero and the kept walker; whether the choice offered
can disagree with who walks depends on the choice's text (game data, not
read here). Left unchanged; the owner's play of that choice with the mode on
settles it.

## 6. What was run, what was read

- **Read:** every site in section 2.
- **Measured from `BOF3.exe`** (scratch scripts, not in the tree; nothing of
  the data copied here beyond kinds, counts and the ids named above): the
  drop-in table kinds; area 134 entry 0's first counter op; area 5's entries
  3..6; the party-set search over all pairs.
- **Run:** the i686 build; `BOF3X_SELFTEST_ONLY=1` (exe host): `solo scenes
  self-test: 70 checks passed` beside `solo self-test: 132 checks passed` and
  `solo field self-test: 60 checks passed`, exit 0; the hosted fuzz
  (`BOF3X_SHADOW='*'`), section 7.
- **Not run:** any fixed site in the game. No save under `tools/recipes/`
  stands next to one of them (chapter 1's inn, chapter 6's run 8, chapter
  7's B[0], chapter 9 / 10's save-and-leave, chapter 12's area 134, chapter
  13 / 14's helpers).

## 7. The hosted fuzz

See the hand-back for the run's result on this branch's tip.

## 8. What the owner would record or play

1. Chapter 1, the inn whose keeper starts the rest at step 0 (handler 0xF),
   with the mode on: the rest ends and the party wakes in area 5 (3.1).
2. Chapter 12, entering area 0x86 the first time with the mode on: whether
   the scene hangs (section 4's instance); a save just before it is the
   cheapest proof of the class.
3. Chapter 6's boss 25 (run 8) with the mode on and two walkers: the party
   after the fight is the two who walked in (3.2).
4. Chapters 9 and 10 where Ryu goes on alone and the party comes back
   (`ScenaCall_SaveAndLeave` then `Party72Saved` / `RestoreSaved`): the
   walker who left comes back, nobody else (3.3, 3.4).
5. Chapter 2, area 0x1B, and chapter 5's pair choice (section 5).

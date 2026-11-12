# The solo-character mode's field half (group FS)

**Status:** SUPERSEDED (2026-11-12, the owner, round doc 6e: the map party
stays the game's own three, so this half was built, merged and taken out of
`solo/round` the same day. The code is on branch `solo/fs`, tip `01ca8f40`.
The measurements here stand: the party-set load cost, `PartySet_Find`'s
missed row, the scenes that test the party. Round doc:
[`solo-character-round.md`](solo-character-round.md).)

Code: `src/game/solo/solo_field.{h,cpp}`; hooks in
`src/game/field/field_4.cpp` (`Field_LeaderStand`, `Field_LeaderSwapTest`,
`Field_SwapExchange`, `Field4_Inject`) and `src/game/field/field_6.cpp`
(`Party_Join`). Nothing in `solo.{h,cpp}` changed.

## 1. The rule as built

While `solo::Solo_Active()` (the behaviour on, the chapter not 0 or 16, a
champion in the save's mark):

- **The map team is every member the game marks "must stay" and at most one
  member who is not marked.** Record 0 carries the mark from New Game and
  nothing clears it (solo-character-reads.md M7), so with record 0 on the map
  this is "the hero and one other" (6a item 1); a scene's mark (chapters 6
  and 8) keeps a third. With record 0 away (a scene took him), it is one
  walker plus any marked member.
- **The "one other" kept is the first unmarked member in the first list's
  order**, which is the leader whenever the leader is not marked. Since the
  leader is either marked or that first unmarked member, the leader is never
  taken off the map, and ObjTrio record 0 is never rebuilt by the fold.
- **The fold never adds a member.** A team the game made smaller than the
  rule (Ryu alone, a scene's two) stays as the game made it.
- Everyone else who has joined (`+0xB` bit 0) is in reserve. The reserve is
  not stored by the game anywhere: both members screens build it from bit 0
  each time they open (M6), so a member the fold took off is in reserve with
  no write of ours beyond `Party_Remove`.

## 2. The fold

**Where.** `Field_LeaderStand` (`0x52DA70`) after its first three returns
(`Field_ScriptFlags` bit 8, `Field_ScriptFlags2` bit 6, `Field_Request` 5),
before the button tests - M8's frame, so a fold comes before the L swap's
test in the same frame.

**When.** A signature - both lists `0x904062..0x904067`, `Field_MemberCount`,
the eight records' `+0xB & 3` - is compared with the one the last
evaluation settled; it runs when it differs, or on the first frame the mode is
active after it was not (entering the mode: a save loaded, the chapter
leaving 0, the mark stamped). Two more guards, either of which only defers to
the next frame: `Game_Mode` is 2 (the field) and `Field_Request` is 0 (no
menu, encounter or members screen pending).

**What it will not touch** (logged once per signature, `no fold, <why>`):

- a list holding an id past the member count. **Measured:** the
  `fairyVillage` save (chapter 9) walks one member with both lists `07 08 05`
  - the game's own one-walker state, as `PartyForm_Reload`'s "lone" path
  sets the count to 1 without changing the lists (`party_form.cpp:343-360`).
  A fold or a set load there would leave the lists' other ids without
  sprites when the game walks them again;
- the second list not holding the first list's members (chapter 6's
  `SortParty` sets the second list alone until `Boss25_Exit`);
- an id past `Character_IndexById`'s 24, a record on the map twice, a count
  above 3.

**How.** Each member over the rule leaves through the game's own
`Party_Remove` (`0x534030`): both lists compacted, the ObjTrio records after
it moved down and renumbered, the last one turned off, the formation byte
`0x904060` reset as for a party that shrinks, the count one less, the joined
bit left set (M6: `Party_Remove` does not clear it). Then
`Party_ReloadPalettes` (`0x533E00`), each walker's palette from its slot.
This is the exact sequence Capcom's scenes use to take a member out:
`Scena02_Leave43` (`src/game/scenario/scena_02.cpp:39-49`) is `Leave(4)`,
`Leave(3)`, `Palettes()`. **Not `Field_PartyLoad(0)`**: it turns every
walker off and rebuilds it (`Party_ClearAll`, the leader's state to 0), the
right call after a set change (`PartyForm_Reload`) but more than a removal
needs; and not `PartyForm_Reload`, whose transition, area placements and
"lone" path belong to the members screen. The walkers that stay keep their
sprite banks: the loaded set already holds them.

`Sprite_Current` and `Field_State` are put back to the leader after the
calls, so `Field_LeaderStand` goes on with its frame as before.

**The champion in the party set.** After the fold, when the loaded set does
not hold the champion and a row holds the walkers and the champion, that
row is loaded (section 4), so the battle's set is already there (round doc
M1, 6a "the party set with three on the map"). Where no row holds them all,
nothing is loaded here and CB's own load at the battle's start applies.

## 3. The L swap

`Field_LeaderSwapTest` is unchanged in what it tests (two walkers or more,
the button, `Party_CanSwap`); the mode never lets a single walker swap, since
the fold never adds a member.

**The rotation is the original's.** What the mode adds is the **bring-out**:
when the member who leads at the press is not marked, is the map's only
member not marked, and some joined, unmarked character is not on the map,
that member is replaced, as the exchange moves them off the lead, by the
**next joined character in record order** after them (records 0..7, round to
the start; a record's id is its `+9`, and a record whose id does not map back
to it through `Character_IndexById` is skipped). With the hero and one other
the presses go:

    (Ryu leads, A) -> (A leads, Ryu) -> (Ryu leads, B) -> (B leads, Ryu) -> (Ryu leads, C) ...

so every joined character is reached as the walker beside the hero and as the
leader, in a fixed order, and the hero leads every other press. With a
scene's third (a marked member) the rotation is the original's three-way one
and the unmarked member is the one replaced when it leaves the lead. With no
one in reserve the swap is the original's exactly.

**Built on `Field_SwapExchange`'s reload steps, at the press.** In
`Field_LeaderSwapTest`, once the swap is allowed and a bring-out is due:

1. the party set: when the loaded set does not hold the team after the swap
   (the hero, the newcomer, any marked member), a row that holds it - and the
   champion too where a row allows - is loaded by `PartySet_Load(a, b, c, 0)`,
   the call `PartyForm_Reload` makes;
2. the leader who is leaving the lead is made the newcomer in place, with the
   steps the exchange takes for each member it moves (`0x52EE70`): `+0x89`,
   `Field_MemberSprite(id, slot)` (sprite bank and palette from the loaded
   set), the record's `0xA4` bytes to `+0x80`, `+0x148`, the first list's
   byte; and the second list's byte that held the old id;
3. after a load, every walker's sprite is set again (`Field_MemberSprite`,
   `Sprite_SetAnimation`), as `PartyForm_Reload` does after its load, because
   a new row moves the ids' columns;
4. the swap then runs as the original's: the newcomer spins off the lead and
   the hero takes it.

**Why at the press and not inside the exchange** (the first build did it
inside the exchange, after `ObjTrio_SwapFields`): the leaving member still
walks through `Field_SwapGather` and `Field_SwapSpinOut` with its sprite, so
the set would have to hold the hero, the leaving member and the newcomer at
once, and a walker whose id the set lacks gets column 3 from
`Field_MemberSprite`, the bank table's fourth dword, as its bank. **Measured
2026-11-12** on the `dolphinBoss` route probe: the first bring-out loaded a
set without the leaving member, set every walker's sprite again, and
`Sprite_LoadPalette` read `0x01C22FA2` and crashed (`0xC0000005` at
`bof3x.dll+0x10D623`, the crash dump in the run root). Replacing the member
first needs a set for the team after the swap only - two ids and the
champion - which the rows hold far more often than three walkers' worth.
The cost is visible: the newcomer appears on the lead as the button is
pressed, then the original swap animation plays.

**A row is never assumed.** `PartySet_Find` (`0x5368F0`) narrows on the
lowest id's column-0 rows first and can miss a row that holds the ids; on a
miss it calls `PartySet_ErrorLoop` (`0x536A60`), which never returns.
`solo_field.cpp`'s `FindRow` is that search without its side effects, and a
load is made only for ids it finds. (By hand on the 19 rows of round doc M1:
`(4, 9)` is in row `(3,4,9)` but the search misses it; the self-test holds
the same shape on a table of its own.) No row: the swap is the original's
and a line is logged.

**A bring-out does not need a load when the set already holds the
newcomer**; then step 1 and 3 are skipped and step 2 alone runs.

## 4. The cost of a set load mid-walk (measured)

`LoadSet` reads `Frame_Counter` (`0x937F94`, WinMain's frame count) and the
performance counter around `PartySet_Load`:

| Run | Load | Frames | Time |
|---|---|--:|--:|
| `geist` save, fold's champion load | set `87 -> 88`, ids `07 02 05` | 0 | 4,417 us; 483 us on a second run |
| `dolphinBoss` route, fold's champion load | `81 -> 82`, ids `00 01 06` | 0 | 515 us; 402, 408 us on reruns |
| `dolphinBoss` probe, swap bring-out 1 | `82 -> 84`, ids `02 00 06` | 0 | 626 us |
| bring-out 2 | `84 -> 86`, ids `05 00 06` | 0 | 2,156 us |
| bring-out 3 | none (set `86` held id 6) | - | - |
| bring-out 4 | `86 -> 82`, ids `01 00 06` | 0 | 552 us |

`PartySet_Load` asks `File_LoadDone` before its first `Task_Sleep`, and on
this port the two `LoadDatFile` calls are done by then, so no frame is spent
waiting: the load costs part of one frame's time, not frames. The swap's
states therefore never wait a load out: the load is over before
`Field_LeaderSwapTest` returns. The sleep path is still the game's own (a
slow disk would sleep the field task a frame at a time, as a scene's
`PartySet_Load` does); it is not reached in the runs above.

## 5. A join with three on the map

`Party_Join` (`0x533EF0`) sets the joined bit first and, with three walkers,
asks for `Field_Request` 6 and answers 1. In the mode, with three walkers, it
answers 1 without the request: the member has joined and is in reserve, and
the choose-who-leaves screen does not open.

With fewer than three walkers the join is the original's: the member walks
at once (so a scene that joins members and then moves them by slot finds
them), and the fold puts the member over the rule in reserve at the frame
control comes back. Since the kept unmarked member is the first in list
order and a join appends, **the newcomer is the one who goes to reserve** -
the effect of the round doc's "the new member goes to the reserve", made at
the fold rather than inside `Party_Join` (a party builder that clears the
lists and joins three, e.g. `Scena16_PartyReset` then three joins, would
otherwise build an empty or one-member party in the middle of its scene).

**What the original's screen did that the mode skips** (the screen is
`PartyForm`, `ShopMode_States[6]`, `src/game/field/party_form.cpp`): the
player chose which of the three walkers leaves for the newcomer (a marked
member cannot be chosen, `PartyForm_Swap`'s bit-1 test); then
`PartyForm_Reload` loaded the party set for the new list, rebuilt the walkers
(`Field_PartyLoad(0)`), stepped Garr half a cell when he became the leader,
placed the party for areas `0x85` and `0x5C`, and ran the transition back.
In the mode none of that happens at a join: the walkers stay as they were,
the newcomer is in reserve, and reaching them is the L swap's bring-out (or
the members screen, MS's).

## 6. Entering and leaving the mode

Entering (first frame of `Solo_Active()` after it was false): the same fold.
Leaving (the champion's mark gone, chapter 0 or 16, the behaviour off): the
field half does nothing, and the party is whatever the game holds. A member
the fold took off is not put back; they are in reserve with the joined bit
set, which the game's own members screens offer, as a member a scene dropped
with `Party_Remove` is.

**CB's window.** CB saves the second list in mode 5 step 1 and restores it at
`GameMode5_Place`'s entry. `Field_LeaderStand` runs only as the field leader's
state handler, never in mode 5, and the fold also requires `Game_Mode == 2`.
So the fold cannot run between CB's save and restore. What the fold can see
after a battle is CB's restored lists, which equal the lists it settled
before the battle; the signature is then unchanged and nothing runs.

## 7. State and the quicksave

The field half keeps two globals outside the image span: the last settled
signature and whether the mode was active. Both are a cache: the fold is
idempotent (a team already the rule's has nothing to drop), so a stale cache
after a quick-load or a load from the title costs one evaluation that changes
nothing. **No snapshot participant.** The bring-out's plan is not stored: it
is read again in `Field_SwapExchange` from the lists and records, which
nothing changes between the swap test and the exchange. The one thing a
quick-load can leave out of step is the champion in the party set (the set
byte is not in the signature); CB's load at the battle covers it.

## 8. The risk: scenes that test the party they find

The fold changes what a later scene finds. Sites found by reading (`grep`
of `src/game`, 2026-11-12), not run:

- **`Party_DropIn`** (`0x531F90`, `field_6.cpp:1067`): kind 0 (exactly N
  members, each wanted id present) and kind 0x80 (at least N) answer 1 and
  set nothing up when the team is smaller; 211 references to it outside the
  fuzz files. Which entries are kind 0 / 0x80 is in each area's descriptor
  table (data), not counted here.
- **Script condition 15**, `EventCond_RecordBit0` (`event_cond.cpp:676`):
  tests bit 0, so a reserve member reads as "with us" while not walking.
- **Scenes that test the walker count** (scenario and area code):
  `scena_01_2.cpp:2044` (an area entry chosen by `Field_MemberCount == 3`),
  **`scena_01_2.cpp:2066` (step 0x17 waits for `Field_MemberCount == 3`: a
  hang if it is reached with fewer)**, `scena_02_2.cpp:772`, `:1478` (entry
  flags by the count), `:2618` (a trigger only with exactly two).
- **Scenes that read the leader's id** (`Cond_Flags[0xD2]`):
  `scena_01_2.cpp:2267`, `scena_02_2.cpp:47` (`LeadMember`),
  `scena_05_2.cpp:440`, `:470`, `:479`, `:1747`, `area_26.cpp:140`. The
  fold never changes the leader, but the bring-out lets any joined character
  lead.
- **`Party_Count(0)`** callers outside the field and menus: `area_189.cpp:179`
  (an encounter pace doubled by a member's state).
- The **`geist` save** (section 9) loads into a conversation that starts after
  the fold took a member off; whether that scene wanted the member is not
  known from the run.

## 9. Runs (2026-11-12, root `D:\bof3x-runs\solo1112\FS\root`)

Headless, `--no-front`, an ini-less launcher copy, a game copy with
`--slot0-shared`, `BOF3X_ON=SoloCharacter` and `BOF3X_SOLO=<name>`. The
probe recipes are FS's own (`root\recipes`), not recorded routes:
`fs_swap*.txt` load a save the way `field_view.txt` does and press L1;
`fs_dolphin_swap.txt` is `tools/recipes/dolphinBoss.txt`'s first 3,061
frames, eight L1 presses 90 frames apart, then the route's next 1,800
frames. No shot lines were added inside recorded input.

- **Self-test** (`BOF3X_SELFTEST_ONLY=1`, exe host): `solo field self-test:
  60 checks passed`, beside `solo self-test: 48 checks passed`.
- **The fold with three walkers**, `dolphinBoss` save (chapter 7), champion
  Peco: `fold (the mode enters): first 00 01 06 second 00 01 06 count 3 set 82
  -> first 00 01 FF second 00 01 FF count 2 set 82; to the reserve: 06`. Also
  on `combat` (chapter 1, `00 04 03 -> 00 04`, the world map) and `geist`
  (chapter 10, `07 02 04 -> 07 02`, then the champion's set `87 -> 88`).
- **The game's lone walk left alone**: the camp after the `dolphinBoss`
  load, `no fold, a list holds an id past the member count ...: first 00 01
  FF second 00 01 FF count 1`; the `fairyVillage` save (`07 08 05`, one
  walker) in the first build, which loaded a set there before the check
  existed - the reason for it.
- **A scripted scene that builds a party and returns control**: on the
  `dolphinBoss` route a house scene joins Momo (`first 00 01 05 count 3`);
  at control's return `fold (the party changed): ... -> first 00 01 FF ...;
  to the reserve: 05`, then `party set 81 -> 82 for ids 00 01 06 (the
  walkers and the champion)`. The route went on through the world map to
  the next area.
- **The L swap through every joined character**: eight presses, every
  other one a bring-out: `brings id 02 out in place of id 01`, `05 ... 02`,
  `06 ... 05`, `01 ... 06` - Nina, Garr, Momo, Peco and back, the four
  joined characters beside Ryu (records 0, 1, 2, 5, 6 joined in that save).
  The captures show each one's own sprite.
- **A door after it**: the route's next 1,800 frames, unchanged input,
  walked into the next area and its scene and finished (`recipe done at
  recipe frame 6080`).
- **The first build's crash** (section 3) on the same probe, before the
  bring-out moved to the press.
- **Mode off**, the same probe without `BOF3X_ON` (`BOF3X_SOLO=peco` set):
  no line of the field half in the log, `solo: BOF3X_SOLO=peco is set and
  SoloCharacter is off: nothing reads it`, the recipe done. Not a state-hash
  proof (the coordinator's).

**Desync.** The `dolphinBoss` route with the mode on ran on past the house
scene and timed out at eight minutes with `BOF3X_SHOT_EVERY=90` slowing it;
whether its later input still lands is not checked frame by frame. Routes
recorded with a party of three cannot be expected to hold once the team is
two; the owner's recordings are the way to a solo route (hand-back).

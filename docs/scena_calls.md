# The chapter call tables' entries: `Scena<NN>_*` and `ScenaCall_*` at `0x519890..0x51AC50` (group CALLS)

**Status:** IN PROGRESS (2026-09-27) - 98 functions ours through the
scenario harness, 0 mismatches in 294,000 rounds; 106 of 106 controls
refused. Fuzz only. Round ten wave two group CALLS
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 6).

`src/game/scena_calls.cpp` (ours), `scena_calls.h`, `scena_calls_callees.h`
(the raw addresses), `scena_calls_fuzz.cpp` (the group). Shadow name
`scena_calls`.

## 1. The band and what is in it

`Scenario_CallA` `0x5341A0` (ours, [`field-modes.md`](field-modes.md)) and
`Scenario_CallB` `0x5341C0` (Capcom's, named by SCH) jump to entry `n & 0xFF`
of the current chapter's call table A or B (`Scena<NN>_CallA` / `_CallB`,
`Scenario_CallATables` `0x660B84` / `Scenario_CallBTables` `0x660BD4`,
[`scenario-roots.md`](scenario-roots.md) section 2), with the caller's
arguments in place and **ecx = the index `n & 0xFF`** (both thunks: `mov ecx,
[esp + 4]; and ecx, 0xFF; jmp [edx + ecx * 4]`). Wave one found that the
tables' entries lie in no chapter's band, but in one engine-side block,
`0x519890..0x51AC50` (`scenario_rows.py --unit CALLS`).

The block (0x13C0 bytes) holds 100 entries, 0x10C3 bytes of code, the rest
`nop` padding (capstone, 2026-09-27: every byte of the block is in one of the
100 or is 0x90):

- `0x519890` `Scena16_PartyReset` - ours already (`move_cmds.cpp`), chapter
  16's and chapter 0's A[0];
- `0x519F70` `Scena08_PartyJoin784` - SE's (`scena_se.cpp`), chapter 8's A[5]
  and the tail of A[6]; the tool counts it in SE's unit;
- **98 taken here** - the tool's 99 less `0x519890`. Its "98 not walked" are
  these: the walk stopped at the tables as its frontier. Each was read to its
  last instruction; **every one is a function**, none a jump-table case, none
  missing: each is referenced only by the call tables (152 dwords in
  `0x65F664..0x660B84`, a byte scan of the image), each ends in `ret` or a tail
  `jmp`, and the only `E8` / `E9` into the block is `0x519FA0`'s tail `jmp` to
  SE's `0x519F70`. No start dropped, none added.

**What they are.** Every one is a party change, built from five callees:
`PartySet_Load(a, b, c, 0)` (the party set's files for three members, a frame
at a time until loaded - ours), `Field_PartyLoad(0)` (the field members rebuilt
from the first party list - ours), `Party_Join(id)` (ours), `0x534030(id)` (the
member leaves: out of both party lists, the field objects after it moved down
one, `Field_MemberCount` - 1 - nobody's) and `0x533E00` (the members' palettes
reloaded - nobody's), with `Field_MemberCount` cleared before a party is joined
anew. Some also clear or set a character record's `+0xB` bit 0 (the bit
`Party_Join` sets: "in the party") or its `+9`, through
`MoveScript_EffectState[id]`; empty or rewrite the party lists
`0x904062..0x904067`; or save a party to `0x903A10..0x903A15` (three ids, then
a copy of the second list) and restore it later - a cell only this block reads
and writes. What each party change means in the story is not read here (the
member ids are the character records' ids; which character each is comes from
the owner or the records, not from memory).

**Names.** `Scena<NN>_` where one chapter's tables hold the entry, `ScenaCall_`
where several do (25 are shared: one entry sits in up to six tables). Then
`Party<ids>` - the count cleared and those members joined (after a set of the
same three), `Set<abc>` a party set loaded, `Reload`, `Join<ids>`,
`Leave<ids>`; `X` is 0xFF; the 26 entries with anything more are named by it
(`SaveAndLeave`, `RestoreSaved`, `LeaveAll`, `Out10` for a record bit cleared).

**No argument, no answer.** None reads a word the caller pushed. None of the
153 `Scenario_CallA` call sites and none of the 119 `Scenario_CallB` sites reads
`eax` after the call (capstone, to the first branch or write of `eax`; the
CallA count is `field_modes.cpp`'s), so ours are `void (void)`. **Two read the
caller's ecx** (section 6): `0x51A300` and `0x51AB50` gather ids in a pushed
`ecx` and read its bytes back when there are fewer than three members. Ours
for those two is a naked entry that pushes `ecx` for a C++ body - what
`Scenario_CallA` / `B` leave there, the index, reaches ours as it reaches the
original.

## 2. The functions

Chapters' tables: `9 B[6]` is chapter 9's table B, entry 6
(`analysis/scenario_roots.json` `roots`). **PSX by position**: the entry at the
same chapter, table and index of the PSX's own tables
(`names/scenario_records.toml` `subA` / `subB`, one copy inside each chapter's
SCENA overlay; the PSX list of `subA` runs on into `subB`, so its `subA` counts
are the PC's A plus B) - a hypothesis by table position, the code is not
compared; the names are not taken from it.

| PC | Name | Bytes | Chapters' tables | PSX by position | What it does |
|---|---|--:|---|---|---|
| `0x5198F0` | `Scena01_Set934Party9` | 0x49 | 1 A[0] | `0x801FE10C` | the record of MoveScript_EffectState[10] (Char_WhelpSlot) +0xB bit 0 cleared; PartySet_Load(9, 3, 4, 0); Field_MemberCount 0; the record of MoveScript_EffectState[9] (read after the load) +9 = 9; Party_Join(9) |
| `0x519940` | `Scena01_Party034` | 0x42 | 1 A[1] | `0x801FE1B0` | the record of MoveScript_EffectState[0] +9 = 0; PartySet_Load(0, 3, 4, 0); Field_MemberCount 0; Party_Join(0), (3), (4) |
| `0x519990` | `ScenaCall_Join4` | 0x9 | 1 A[2], 2 A[0] | `0x801FE224`, `0x801FE178` | Party_Join(4) |
| `0x5199A0` | `ScenaCall_Leave4` | 0xF | 1 B[0], 2 B[0], 9 B[0], 11 B[0], 12 B[0] | `0x801FE244`, `0x801FE198`, `0x801FE12C`, `0x801FADF8`, `0x801FD0B0` | 0x534030(4); 0x533E00 |
| `0x5199B0` | `Scena02_Leave43` | 0x49 | 2 B[1] | `0x801FE1C0` | the records of MoveScript_EffectState[4] and [3] +0xB bit 0 cleared; 0x534030(4), 0x534030(3); tail 0x533E00 |
| `0x519A00` | `Scena03_Set012Party0` | 0x3F | 3 A[0] | `0x801FCE6C` | PartySet_Load(0, 1, 2, 0); both party lists 0x904062..0x904067 = 0xFF; Field_MemberCount 0; Party_Join(0) |
| `0x519A40` | `ScenaCall_Join1` | 0x9 | 3 A[1], 5 A[1] | `0x801FCEDC`, `0x801FD30C` | Party_Join(1) |
| `0x519A50` | `ScenaCall_Leave1` | 0xF | 3 B[0], 5 B[1] | `0x801FCEFC`, `0x801FD508` | 0x534030(1); 0x533E00 |
| `0x519A60` | `Scena04_Set015Party01` | 0x46 | 4 A[0] | `0x801FA0B4` | PartySet_Load(0, 1, 5, 0); both party lists 0x904062..0x904067 = 0xFF; Field_MemberCount 0; Party_Join(0), (1) |
| `0x519AB0` | `ScenaCall_Party015` | 0x2D | 4 A[1], 7 A[0] | `0x801FA12C`, `0x801FDB18` | PartySet_Load(0, 1, 5, 0); Field_MemberCount 0; Party_Join(0); Party_Join(1); Party_Join(5) |
| `0x519AE0` | `Scena05_ReloadJoin516` | 0x35 | 5 A[0] | `0x801FD298` | the record of MoveScript_EffectState[6] +0xB bit 0 set; Field_PartyLoad(0); Party_Join(5), (1), (6) |
| `0x519B20` | `Scena05_Join0` | 0x9 | 5 A[4] | `0x801FD36C` | Party_Join(0) |
| `0x519B30` | `Scena05_Set015ReloadJoin5` | 0x1F | 5 A[5] | `0x801FD38C` | PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(5) |
| `0x519B50` | `Scena05_Set015ReloadJoin1` | 0x1F | 5 A[6] | `0x801FD3C8` | PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(1) |
| `0x519B70` | `Scena05_Set016ReloadJoin6` | 0x1F | 5 A[7] | `0x801FD404` | PartySet_Load(0, 1, 6, 0); Field_PartyLoad(0); Party_Join(6) |
| `0x519B90` | `ScenaCall_Join2` | 0x9 | 5 A[8], 8 A[2] | `0x801FD440`, `0x801FE740` | Party_Join(2) |
| `0x519BA0` | `Scena05_Leave51` | 0x16 | 5 B[0] | `0x801FD4D8` | 0x534030(5); 0x534030(1); 0x533E00 |
| `0x519BC0` | `ScenaCall_Leave5` | 0xF | 5 B[2], 6 B[6], 9 B[7], 10 B[1], 11 B[3], 12 B[3] | `0x801FD530`, `0x801FE364`, `0x801FE30C`, `0x801FE614`, `0x801FAE70`, `0x801FD128` | 0x534030(5); 0x533E00 |
| `0x519BD0` | `ScenaCall_Leave6` | 0xF | 5 B[3], 6 B[4], 9 B[8], 11 B[2], 12 B[2] | `0x801FD558`, `0x801FE30C`, `0x801FE334`, `0x801FAE48`, `0x801FD100` | 0x534030(6); 0x533E00 |
| `0x519BE0` | `Scena05_Leave0` | 0xF | 5 B[4] | `0x801FD580` | 0x534030(0); 0x533E00 |
| `0x519BF0` | `Scena06_Set015ReloadJoin05` | 0x2A | 6 A[0] | `0x801FDFFC` | PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(0); Party_Join(5); 0x533E00 |
| `0x519C20` | `Scena06_Set015ReloadJoin5` | 0x23 | 6 A[1] | `0x801FE048` | PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00 |
| `0x519C50` | `Scena06_Set015ReloadJoin1` | 0x23 | 6 A[2] | `0x801FE08C` | PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(1); 0x533E00 |
| `0x519C80` | `Scena06_Set015ReloadJoin15` | 0x2A | 6 A[3] | `0x801FE0D0` | PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(1); Party_Join(5); 0x533E00 |
| `0x519CB0` | `ScenaCall_Join6` | 0x9 | 5 A[3], 6 A[5] | `0x801FD34C`, `0x801FE13C` | Party_Join(6) |
| `0x519CC0` | `ScenaCall_Set012ReloadJoin1` | 0x1F | 5 A[9], 5 A[10], 6 A[6], 6 A[8] | `0x801FD460`, `0x801FD49C`, `0x801FE15C`, `0x801FE1D4` | PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(1) |
| `0x519CE0` | `Scena06_Set012ReloadJoin2` | 0x1F | 6 A[7] | `0x801FE198` | PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(2) |
| `0x519D00` | `Scena06_Set012ReloadJoin12` | 0x26 | 6 A[9] | `0x801FE210` | PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(1); Party_Join(2) |
| `0x519D30` | `Scena06_Leave02` | 0x16 | 6 B[0] | `0x801FE254` | 0x534030(0); 0x534030(2); 0x533E00 |
| `0x519D50` | `Scena06_Leave05` | 0x16 | 6 B[1] | `0x801FE284` | 0x534030(0); 0x534030(5); 0x533E00 |
| `0x519D70` | `Scena06_Leave06` | 0x16 | 6 B[2] | `0x801FE2B4` | 0x534030(0); 0x534030(6); 0x533E00 |
| `0x519D90` | `ScenaCall_Leave2` | 0xF | 6 B[3], 9 B[2], 11 B[1], 12 B[1] | `0x801FE2E4`, `0x801FE184`, `0x801FAE20`, `0x801FD0D8` | 0x534030(2); 0x533E00 |
| `0x519DA0` | `Scena06_Leave62` | 0x16 | 6 B[5] | `0x801FE334` | 0x534030(6); 0x534030(2); 0x533E00 |
| `0x519DC0` | `Scena06_Leave56` | 0x16 | 6 B[7] | `0x801FE38C` | 0x534030(5); 0x534030(6); 0x533E00 |
| `0x519DE0` | `Scena07_Party012` | 0x2D | 7 A[1] | `0x801FDB64` | PartySet_Load(0, 1, 2, 0); Field_MemberCount 0; Party_Join(0); Party_Join(1); Party_Join(2) |
| `0x519E10` | `Scena07_LeaveThird` | 0xD | 7 B[0] | `0x801FDBB0` | 0x534030(the third field member's +0x89, ObjTrio[2] at 0x803061) |
| `0x519E20` | `Scena07_Leave2Out1562` | 0x78 | 7 B[1] | `0x801FDBD8` | 0x534030(2); then the records of MoveScript_EffectState[1], [5], [6], [2] +0xB bit 0 cleared, each index read after the call; no palettes |
| `0x519EA0` | `Scena08_Set72APartyA` | 0x23 | 8 A[0] | `0x801FE698` | PartySet_Load(7, 2, 0xA, 0); Field_MemberCount 0; Party_Join(0xA); 0x533E00 |
| `0x519ED0` | `Scena08_Party7` | 0x25 | 8 A[1] | `0x801FE6DC` | Field_MemberCount 0; the record of MoveScript_EffectState[10] (Char_WhelpSlot) +0xB bit 0 cleared; Party_Join(7) |
| `0x519F00` | `Scena08_Set724Party72` | 0x2A | 8 A[3] | `0x801FE760` | PartySet_Load(7, 2, 4, 0); Field_MemberCount 0; Party_Join(7); Party_Join(2); 0x533E00 |
| `0x519F30` | `ScenaCall_Party728` | 0x31 | 8 A[4], 14 A[7] | `0x801FE7AC`, `0x801FC074` | PartySet_Load(7, 2, 8, 0); Field_MemberCount 0; Party_Join(7); Party_Join(2); Party_Join(8); 0x533E00 |
| `0x519FA0` | `Scena08_SetParty784` | 0x15 | 8 A[6] | `0x801FE840` | PartySet_Load(7, 8, 4, 0); tail Scena08_PartyJoin784 (SE) |
| `0x519FC0` | `Scena08_Leave2` | 0x9 | 8 B[0] | `0x801FE874` | 0x534030(2) |
| `0x519FD0` | `Scena09_Set782ReloadJoin8` | 0x23 | 9 A[0] | `0x801FDCF4` | PartySet_Load(7, 8, 2, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00 |
| `0x51A000` | `Scena09_Set782ReloadJoin2` | 0x23 | 9 A[1] | `0x801FDD38` | PartySet_Load(7, 8, 2, 0); Field_PartyLoad(0); Party_Join(2); 0x533E00 |
| `0x51A030` | `ScenaCall_Set785ReloadJoin85` | 0x2A | 9 A[2], 11 A[0], 11 A[3], 12 A[0], 12 A[3] | `0x801FDD7C`, `0x801FAB74`, `0x801FAC48`, `0x801FCDE0`, `0x801FCEB4` | PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(8); Party_Join(5); 0x533E00 |
| `0x51A060` | `ScenaCall_Set785ReloadJoin5` | 0x23 | 9 A[3], 11 A[2], 11 A[5], 12 A[2], 12 A[5] | `0x801FDDC8`, `0x801FAC04`, `0x801FACD8`, `0x801FCE70`, `0x801FCF44` | PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00 |
| `0x51A090` | `ScenaCall_Set785ReloadJoin8` | 0x23 | 9 A[4], 11 A[1], 12 A[1] | `0x801FDE0C`, `0x801FABC0`, `0x801FCE2C` | PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00 |
| `0x51A0C0` | `Scena09_ReloadJoin6` | 0x16 | 9 A[5] | `0x801FDE50` | Field_PartyLoad(0); Party_Join(6); 0x533E00 |
| `0x51A0E0` | `ScenaCall_Set765ReloadJoin56` | 0x2A | 9 A[6], 11 A[6], 12 A[6] | `0x801FDE80`, `0x801FAD1C`, `0x801FCF88` | PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(5); Party_Join(6); 0x533E00 |
| `0x51A110` | `ScenaCall_Set765ReloadJoin6` | 0x23 | 9 A[7], 11 A[7], 12 A[7] | `0x801FDECC`, `0x801FAD68`, `0x801FCFD4` | PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(6); 0x533E00 |
| `0x51A140` | `Scena09_Set765ReloadJoin5` | 0x23 | 9 A[8] | `0x801FDF10` | PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00 |
| `0x51A170` | `ScenaCall_Party72Saved` | 0x5C | 9 A[9], 10 A[5] | `0x801FDF54`, `0x801FE4F0` | k = 8; for each of the three saved bytes 0x903A10..0x903A12: k = the byte unless it is 7 or 2, the byte = 0; PartySet_Load(7, 2, k, 0); Field_MemberCount 0; Party_Join(7), (2), (k); 0x533E00 |
| `0x51A1D0` | `Scena09_Set748ReloadJoin8` | 0x23 | 9 A[10] | `0x801FE014` | PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00 |
| `0x51A200` | `Scena09_Set748ReloadJoin48` | 0x2A | 9 A[11] | `0x801FE058` | PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(4); Party_Join(8); 0x533E00 |
| `0x51A230` | `Scena09_Set748ReloadJoin4` | 0x23 | 9 A[12] | `0x801FE0A4` | PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(4); 0x533E00 |
| `0x51A260` | `Scena09_Set748ReloadJoin7` | 0x23 | 9 A[13] | `0x801FE0E8` | PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(7); 0x533E00 |
| `0x51A290` | `Scena09_Leave42` | 0x16 | 9 B[1] | `0x801FE154` | 0x534030(4); 0x534030(2); 0x533E00 |
| `0x51A2B0` | `Scena09_Leave28` | 0x16 | 9 B[3] | `0x801FE1AC` | 0x534030(2); 0x534030(8); 0x533E00 |
| `0x51A2D0` | `Scena09_Leave48` | 0x16 | 9 B[4] | `0x801FE1DC` | 0x534030(4); 0x534030(8); 0x533E00 |
| `0x51A2F0` | `ScenaCall_Leave8` | 0xF | 9 B[5], 11 B[4], 12 B[4] | `0x801FE20C`, `0x801FAE98`, `0x801FD150` | 0x534030(8); 0x533E00 |
| `0x51A300` | `ScenaCall_SaveAndLeave` | 0x53 | 9 B[6], 10 B[3] | `0x801FE234`, `0x801FE6F4` | n = Field_MemberCount; for i below n: the member's +0x89 (ObjTrio + 0x14C i) into 0x903A10 + i and a 4-byte local (the caller's ecx, pushed); then each of the local's first three bytes not 7: 0x534030(it); 0x533E00. A count below 3 leaves the rest of the local as ecx had it (the entry index); a count above 4 writes over the return address (latent) |
| `0x51A360` | `Scena09_Leave7` | 0xF | 9 B[9] | `0x801FE35C` | 0x534030(7); 0x533E00 |
| `0x51A370` | `Scena10_Party728` | 0x2D | 10 A[0] | `0x801FE314` | PartySet_Load(7, 2, 8, 0); Field_MemberCount 0; Party_Join(7); Party_Join(2); Party_Join(8) |
| `0x51A3A0` | `Scena10_Party75` | 0x6A | 10 A[1] | `0x801FE360` | for i while i < Field_MemberCount (read again after every call): 0x534030(ObjTrio[i] +0x89); PartySet_Load(7, 5, 8, 0); Field_MemberCount 0; Party_Join(7), (5). 0x534030 lowers the count and moves the later members down, so the loop passes over every second member (latent) |
| `0x51A410` | `Scena10_Join8` | 0x9 | 10 A[2] | `0x801FE3F8` | Party_Join(8) |
| `0x51A420` | `ScenaCall_Join5` | 0x9 | 5 A[2], 6 A[4], 10 A[3] | `0x801FD32C`, `0x801FE11C`, `0x801FE418` | Party_Join(5) |
| `0x51A430` | `Scena10_RestoreSaved` | 0x55 | 10 A[4] | `0x801FE438` | the saved bytes s0..s2 (0x903A10..0x903A12); PartySet_Load(s0, s1, s2, 0); each of s0, s1, s2 not 7: Party_Join(it); no count reset, no palettes |
| `0x51A490` | `Scena10_Leave5` | 0x24 | 10 B[0] | `0x801FE5B0` | the record of MoveScript_EffectState[5] +0xB bit 0 cleared; 0x534030(5); tail 0x533E00 |
| `0x51A4C0` | `Scena10_LeaveAll` | 0x43 | 10 B[2] | `0x801FE63C` | the three members' +0x89 copied; each not 7: 0x534030(it); 0x533E00 |
| `0x51A510` | `ScenaCall_Set725ReloadJoin5` | 0x23 | 11 A[4], 12 A[4] | `0x801FAC94`, `0x801FCF00` | PartySet_Load(7, 2, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00 |
| `0x51A540` | `ScenaCall_Set742ReloadJoin42` | 0x2A | 11 A[8], 12 A[8] | `0x801FADAC`, `0x801FD018` | PartySet_Load(7, 4, 2, 0); Field_PartyLoad(0); Party_Join(4); Party_Join(2); 0x533E00 |
| `0x51A570` | `Scena12_Set752ReloadJoin25` | 0x2A | 12 A[9] | `0x801FD064` | PartySet_Load(7, 5, 2, 0); Field_PartyLoad(0); Party_Join(2); Party_Join(5); 0x533E00 |
| `0x51A5A0` | `Scena13_Party765` | 0x2D | 13 A[0] | `0x801FBB78` | PartySet_Load(7, 5, 6, 0); Field_MemberCount 0; Party_Join(7); Party_Join(6); Party_Join(5) |
| `0x51A5D0` | `Scena13_SaveParty7` | 0x60 | 13 A[1] | `0x801FBBC4` | 0x903A10..12 = the first list 0x904062..64 and 0x903A13..15 = the second 0x904065..67; 0x534030(0x903A10[i]) for i 0..2, each read after the last call; PartySet_Load(7, 4, 8, 0); Field_MemberCount 0; Party_Join(7); 0x533E00 |
| `0x51A630` | `Scena13_Set758Party7` | 0x1F | 13 A[2] | `0x801FBC88` | PartySet_Load(7, 5, 8, 0); Field_MemberCount 0; Party_Join(7) |
| `0x51A650` | `Scena13_RestoreSaved` | 0x6A | 13 A[3] | `0x801FBCC4` | the saved s0..s2 (0x903A10..12); PartySet_Load(s0, s1, s2, 0); Field_MemberCount 0; Party_Join(s0), (s1), (s2); then the second list 0x904065..67 = 0x903A13..15, read after the calls |
| `0x51A6C0` | `Scena13_Party758` | 0x31 | 13 A[4] | `0x801FBD84` | PartySet_Load(7, 5, 8, 0); Field_MemberCount 0; Party_Join(7); Party_Join(5); Party_Join(8); 0x533E00 |
| `0x51A700` | `Scena13_LeaveAllParty7` | 0x5D | 13 A[5] | `0x801FBDD8` | the three members' +0x89 copied; 0x534030 on each (no test); PartySet_Load(7, 4, 2, 0); Field_MemberCount 0; Party_Join(7); 0x533E00 |
| `0x51A760` | `Scena13_Join4` | 0xF | 13 A[6] | `0x801FBE98` | Party_Join(4); 0x533E00 |
| `0x51A770` | `Scena13_Join2` | 0xF | 13 A[7] | `0x801FBEC0` | Party_Join(2); 0x533E00 |
| `0x51A780` | `Scena14_Party748` | 0x2D | 14 A[0] | `0x801FBD2C` | PartySet_Load(7, 8, 4, 0); Field_MemberCount 0; Party_Join(7); Party_Join(4); Party_Join(8) |
| `0x51A7B0` | `Scena14_Party746` | 0x2D | 14 A[1] | `0x801FBD78` | PartySet_Load(7, 4, 6, 0); Field_MemberCount 0; Party_Join(7); Party_Join(4); Party_Join(6) |
| `0x51A7E0` | `Scena14_Party756` | 0x2D | 14 A[2] | `0x801FBDC4` | PartySet_Load(7, 5, 6, 0); Field_MemberCount 0; Party_Join(7); Party_Join(5); Party_Join(6) |
| `0x51A810` | `Scena14_SaveLeaveAllPartyA` | 0x71 | 14 A[3] | `0x801FBE10` | the three members' +0x89 copied, the last that is neither 7 nor 4 also into 0x903A10; 0x534030 on each copy (no test); PartySet_Load(0xA, 0xFF, 0xFF, 0); Field_MemberCount 0; Party_Join(0xA); 0x533E00 |
| `0x51A890` | `Scena14_Out10Party748` | 0x46 | 14 A[4] | `0x801FBF18` | the record of MoveScript_EffectState[10] (Char_WhelpSlot) +0xB bit 0 cleared; PartySet_Load(7, 4, 8, 0); Field_MemberCount 0; Party_Join(7), (4), (8); tail 0x533E00 |
| `0x51A8E0` | `Scena14_Party015Lists785` | 0x47 | 14 A[5] | `0x801FBFA8` | PartySet_Load(0, 1, 5, 0); Field_MemberCount 0; Party_Join(0), (1), (5); 0x533E00; then the second list 0x904065..67 = 7, 8, 5 |
| `0x51A930` | `Scena14_Party726` | 0x31 | 14 A[6] | `0x801FC020` | PartySet_Load(7, 2, 6, 0); Field_MemberCount 0; Party_Join(7); Party_Join(2); Party_Join(6); 0x533E00 |
| `0x51A970` | `Scena14_Party74First` | 0x3A | 14 A[8] | `0x801FC0C8` | PartySet_Load(7, 4, 0x903A10, 0); Field_MemberCount 0; Party_Join(7), (4), (0x903A10 read again after the calls); tail 0x533E00 |
| `0x51A9B0` | `Scena14_SaveLeaveAllParty7` | 0x5D | 14 A[9] | `0x801FC130` | 0x903A10..12 = the three members' +0x89; 0x534030(0x903A10[i]) for i 0..2, each read after the last call (no test); PartySet_Load(7, 8, 4, 0); Field_MemberCount 0; Party_Join(7); 0x533E00 |
| `0x51AA10` | `Scena14_Set726Party7` | 0x23 | 14 A[10] | `0x801FC1E8` | PartySet_Load(7, 2, 6, 0); Field_MemberCount 0; Party_Join(7); 0x533E00 |
| `0x51AA40` | `ScenaCall_Party784` | 0x31 | 14 A[11], 15 A[3] | `0x801FC22C`, `0x801FE520` | PartySet_Load(7, 8, 4, 0); Field_MemberCount 0; Party_Join(7); Party_Join(8); Party_Join(4); 0x533E00 |
| `0x51AA80` | `Scena14_Party74Other` | 0x68 | 14 A[12] | `0x801FC280` | k = the first of 0x903A10..12 that is neither 7 nor 4 (0x903A12 if all are); PartySet_Load(7, 4, k, 0); Field_MemberCount 0; Party_Join(7), (4), (k); 0x533E00 |
| `0x51AAF0` | `Scena14_RecountLeaveAll` | 0x53 | 14 B[0] | `0x801FC328` | Field_MemberCount = the members whose +0 has bit 0, and their +0x89 copied, over all three records; each copy not 7: 0x534030(it); no palettes |
| `0x51AB50` | `Scena15_LeaveAllParty7` | 0x68 | 15 A[0] | `0x801FE3C8` | n = Field_MemberCount; a 4-byte local (the caller's ecx, pushed) gets the first n members' +0x89; 0x534030 on each of its first three bytes (no test); PartySet_Load(7, 4, 2, 0); Field_MemberCount 0; Party_Join(7); 0x533E00. A count below 3 leaves the rest of the local as ecx had it (the entry index); a count above 4 writes over the return address (latent) |
| `0x51ABC0` | `Scena15_Join42` | 0x16 | 15 A[1] | `0x801FE49C` | Party_Join(4); Party_Join(2); 0x533E00 |
| `0x51ABE0` | `Scena15_Party782` | 0x31 | 15 A[2] | `0x801FE4CC` | PartySet_Load(7, 8, 2, 0); Field_MemberCount 0; Party_Join(7); Party_Join(8); Party_Join(2); 0x533E00 |
| `0x51AC20` | `ScenaCall_Party034` | 0x2D | 17 A[0], 18 A[0], 19 A[0] | `0x801F971C`, `0x801F6D04`, `0x801F6D68` | PartySet_Load(0, 3, 4, 0); Field_MemberCount 0; Party_Join(0); Party_Join(3); Party_Join(4) |

## 3. Who reaches them (for the rebinding pass)

Only `Scenario_CallA` / `Scenario_CallB` through the tables above; the
chapters call those with the index (area handlers and event ops,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) section 2). Nothing
calls an entry by address, so the rebinding pass has nothing to re-aim here;
what it has is the other way: ours calls **`0x534030`** and **`0x533E00`**
(nobody's) by raw address, `Scena08_PartyJoin784` (SE's) by name, and
`PartySet_Load`, `Party_Join`, `Field_PartyLoad` (ours) by name.

No entry is on the attract path: the demo runs chapter 16, whose table A holds
only `Scena16_PartyReset` (not this group's). The frame hash reference is
untouched.

## 4. The fuzz (`scena_calls_fuzz.cpp`)

One `Run`, chapter 9, 3,000 rounds a function, every entry `Shape::kEntry`
(ten random words: none is read). **Why one chapter suffices**: no entry reads
`Cond_ByteFA`, the flag row or any chapter byte, and every callee is a
recorder, so the chapter a table belongs to changes nothing an entry does -
except `ecx`, the entry's index, which the seed sets directly (below). Chapter
9's tables hold both entries that read it.

- **The callees.** Listed by the group (registered before the standard set,
  so they stand): `PartySet_Load` with byte masks (it and `PartySet_Find` read
  a, b, c and the mode as bytes; four entries pass dwords whose upper bytes are
  stale stack or the caller's `ecx`, which the standard set's whole-word
  listing would compare), `Party_Join` (`kU8`, `kFlag`), `Field_PartyLoad`
  (`kU8`) and SE's `Scena08_PartyJoin784` (neither in the standard set),
  `0x534030` (`kU8`) and `0x533E00` by address. Each recorder's `effect` moves
  one cell an entry reads again after a call - the member count (0..4), a
  member's id or its `+0` bit 0, a saved byte, a party-list byte, an
  `MoveScript_EffectState` byte (0..7), a record's `+9` / `+0xB` - from the
  recorders' stream (the harness's own disturbance reaches a group cell about
  one call in 24, too seldom to tell a read before a call from one after it,
  wave one's lesson). The group's `disturb` moves the same cells from the hash
  it is given.
- **The regions** beyond the standard 22 (which hold the party lists,
  `Field_MemberCount`, `ObjTrio` and `0x903A04..0x903A13`): `0x903A14..17`,
  `MoveScript_EffectState` (24), and the eight character records it names
  (`CharacterRecords`, 8 x 0xA4) - 11,336 bytes in 25 regions.
- **The seed.** `Field_MemberCount` 0..4 (a count of 5 or more makes
  `0x51A300` and `0x51AB50` overwrite their return address: those two cannot
  be run past 4); the three members' ids, the six saved bytes and, half the
  time, the six list bytes each 7, 2, 4 (the values the entries compare with)
  or 0..11; every `MoveScript_EffectState` byte 0..7, so each index names a
  record in the region.
- **`ecx`.** The harness calls a clone with whatever `ecx` its own code left.
  For `0x51A300` and `0x51AB50` this file hands the harness, as the original,
  a copy of Capcom's bytes behind one instruction of its own - `mov ecx,
  [g_calls_ecx]` (the call sites' rel32 re-pointed from the copy at the real
  callees, which the harness then re-aims as for any clone) - and, as ours, a
  naked stub loading the same cell and jumping to our entry. The seed sets the
  cell to the entry's index in the tables that hold it (6 or 3 for `0x51A300`,
  0 for `0x51AB50`), another byte, or any word. No harness edit.

`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_calls` (2026-09-27, this worktree):
**294,000 rounds over 98 functions (3,000 each), 998,780 calls to the
stand-ins, 0 mismatches**, 11,336 bytes of state (25 regions); exit 0.
Coverage (calls the originals made): `PartySet_Load` 180,000, `Party_Join`
381,523, `Field_PartyLoad` 81,000, `Scena08_PartyJoin784` 3,000, `0x534030`
167,257, `0x533E00` 186,000. `BOF3X_SHADOW='*'`: exit 0.

## 5. Controls

A plant per function, planted in two batches (`controls.py` in the
group's scratch directory: plant, rebuild, run, restore, rebuild; each plant
anchored on a string that occurs once in its function's body). Functions are
fuzzed independently - none of ours calls another of ours, each calls only
recorders - so one batch plants one control in each of many functions and the
harness's per-function "mismatched in N rounds" lines say which it refused.
Batch A: one control in each of the 98 (for the 72 straight-line ones, one of:
the last join's or leave's id one up, the palettes dropped, the set's last two
swapped, the count cleared after the first join instead of before, the reload
or the tail dropped - in rotation; for the 26 others, their own logic). Batch
B: eight more on the entries whose shape is their own. **106 planted, 106
refused**, every one by a count of mismatching rounds (of 3,000):

| # | Function | Plant | Rounds refused |
|---|---|---|--:|
| A1 | `Scena01_Set934Party9` | the +9 record indexed before the load, not after | 15 |
| A2 | `Scena01_Party034` | record index 1 for 0 | 2641 |
| A3 | `ScenaCall_Join4` | the last join Join(4) one id up | 3000 |
| A4 | `ScenaCall_Leave4` | the last leave Leave(4) one id up | 3000 |
| A5 | `Scena02_Leave43` | the second record loses bit 1, not bit 0 | 2141 |
| A6 | `Scena03_Set012Party0` | the lists emptied after the join | 400 |
| A7 | `ScenaCall_Join1` | the last join Join(1) one id up | 3000 |
| A8 | `ScenaCall_Leave1` | the last leave Leave(1) one id up | 3000 |
| A9 | `Scena04_Set015Party01` | member 1 not joined | 3000 |
| A10 | `ScenaCall_Party015` | the set's last two swapped | 3000 |
| A11 | `Scena05_ReloadJoin516` | bit 1 set, not bit 0 | 2239 |
| A12 | `Scena05_Join0` | the last join Join(0) one id up | 3000 |
| A13 | `Scena05_Set015ReloadJoin5` | the last join Join(5) one id up | 3000 |
| A14 | `Scena05_Set015ReloadJoin1` | the set's last two swapped | 3000 |
| A15 | `Scena05_Set016ReloadJoin6` | no reload | 3000 |
| A16 | `ScenaCall_Join2` | the last join Join(2) one id up | 3000 |
| A17 | `Scena05_Leave51` | no palettes | 3000 |
| A18 | `ScenaCall_Leave5` | the last leave Leave(5) one id up | 3000 |
| A19 | `ScenaCall_Leave6` | no palettes | 3000 |
| A20 | `Scena05_Leave0` | the last leave Leave(0) one id up | 3000 |
| A21 | `Scena06_Set015ReloadJoin05` | the set's last two swapped | 3000 |
| A22 | `Scena06_Set015ReloadJoin5` | no reload | 3000 |
| A23 | `Scena06_Set015ReloadJoin1` | the last join Join(1) one id up | 3000 |
| A24 | `Scena06_Set015ReloadJoin15` | no palettes | 3000 |
| A25 | `ScenaCall_Join6` | the last join Join(6) one id up | 3000 |
| A26 | `ScenaCall_Set012ReloadJoin1` | the set's last two swapped | 3000 |
| A27 | `Scena06_Set012ReloadJoin2` | no reload | 3000 |
| A28 | `Scena06_Set012ReloadJoin12` | the last join Join(2) one id up | 3000 |
| A29 | `Scena06_Leave02` | no palettes | 3000 |
| A30 | `Scena06_Leave05` | the last leave Leave(5) one id up | 3000 |
| A31 | `Scena06_Leave06` | no palettes | 3000 |
| A32 | `ScenaCall_Leave2` | the last leave Leave(2) one id up | 3000 |
| A33 | `Scena06_Leave62` | no palettes | 3000 |
| A34 | `Scena06_Leave56` | the last leave Leave(6) one id up | 3000 |
| A35 | `Scena07_Party012` | the set's last two swapped | 3000 |
| A36 | `Scena07_LeaveThird` | the second member leaves | 2312 |
| A37 | `Scena07_Leave2Out1562` | record index 3 for 2 | 1482 |
| A38 | `Scena08_Set72APartyA` | no palettes | 3000 |
| A39 | `Scena08_Party7` | record index 9 for 10 | 1956 |
| A40 | `Scena08_Set724Party72` | the set's last two swapped | 3000 |
| A41 | `ScenaCall_Party728` | the count cleared after the first join | 198 |
| A42 | `Scena08_SetParty784` | the set's last two swapped | 3000 |
| A43 | `Scena08_Leave2` | the last leave Leave(2) one id up | 3000 |
| A44 | `Scena09_Set782ReloadJoin8` | the set's last two swapped | 3000 |
| A45 | `Scena09_Set782ReloadJoin2` | no reload | 3000 |
| A46 | `ScenaCall_Set785ReloadJoin85` | the last join Join(5) one id up | 3000 |
| A47 | `ScenaCall_Set785ReloadJoin5` | no palettes | 3000 |
| A48 | `ScenaCall_Set785ReloadJoin8` | the set's last two swapped | 3000 |
| A49 | `Scena09_ReloadJoin6` | the last join Join(6) one id up | 3000 |
| A50 | `ScenaCall_Set765ReloadJoin56` | the last join Join(6) one id up | 3000 |
| A51 | `ScenaCall_Set765ReloadJoin6` | no palettes | 3000 |
| A52 | `Scena09_Set765ReloadJoin5` | the set's last two swapped | 3000 |
| A53 | `ScenaCall_Party72Saved` | the first saved id kept, not the last | 828 |
| A54 | `Scena09_Set748ReloadJoin8` | no reload | 3000 |
| A55 | `Scena09_Set748ReloadJoin48` | the last join Join(8) one id up | 3000 |
| A56 | `Scena09_Set748ReloadJoin4` | no palettes | 3000 |
| A57 | `Scena09_Set748ReloadJoin7` | the set's last two swapped | 3000 |
| A58 | `Scena09_Leave42` | the last leave Leave(2) one id up | 3000 |
| A59 | `Scena09_Leave28` | no palettes | 3000 |
| A60 | `Scena09_Leave48` | the last leave Leave(8) one id up | 3000 |
| A61 | `ScenaCall_Leave8` | no palettes | 3000 |
| A62 | `ScenaCall_SaveAndLeave` | the local zeroed, not the caller's ecx | 926 |
| A63 | `Scena09_Leave7` | the last leave Leave(7) one id up | 3000 |
| A64 | `Scena10_Party728` | the set's last two swapped | 3000 |
| A65 | `Scena10_Party75` | the count read once | 428 |
| A66 | `Scena10_Join8` | the last join Join(8) one id up | 3000 |
| A67 | `ScenaCall_Join5` | the last join Join(5) one id up | 3000 |
| A68 | `Scena10_RestoreSaved` | 2 skipped instead of 7 | 2723 |
| A69 | `Scena10_Leave5` | record index 6 for 5 | 1943 |
| A70 | `Scena10_LeaveAll` | 7 sent away too | 1853 |
| A71 | `ScenaCall_Set725ReloadJoin5` | no reload | 3000 |
| A72 | `ScenaCall_Set742ReloadJoin42` | the last join Join(2) one id up | 3000 |
| A73 | `Scena12_Set752ReloadJoin25` | no palettes | 3000 |
| A74 | `Scena13_Party765` | the set's last two swapped | 3000 |
| A75 | `Scena13_SaveParty7` | the saved ids read once before the calls | 155 |
| A76 | `Scena13_Set758Party7` | the count cleared after the first join | 299 |
| A77 | `Scena13_RestoreSaved` | the saved list put back into the first list | 2999 |
| A78 | `Scena13_Party758` | the last join Join(8) one id up | 3000 |
| A79 | `Scena13_LeaveAllParty7` | the set (7, 2, 4) | 3000 |
| A80 | `Scena13_Join4` | no palettes | 3000 |
| A81 | `Scena13_Join2` | the last join Join(2) one id up | 3000 |
| A82 | `Scena14_Party748` | the last join Join(8) one id up | 3000 |
| A83 | `Scena14_Party746` | the set's last two swapped | 3000 |
| A84 | `Scena14_Party756` | the count cleared after the first join | 239 |
| A85 | `Scena14_SaveLeaveAllPartyA` | 2 excluded from the save instead of 4 | 1696 |
| A86 | `Scena14_Out10Party748` | record index 11 for 10 | 2028 |
| A87 | `Scena14_Party015Lists785` | the list 7, 5, 5 | 3000 |
| A88 | `Scena14_Party726` | the set's last two swapped | 3000 |
| A89 | `Scena14_Party74First` | the saved id read once | 129 |
| A90 | `Scena14_SaveLeaveAllParty7` | the set (7, 4, 8) | 3000 |
| A91 | `Scena14_Set726Party7` | the count cleared after the first join | 266 |
| A92 | `ScenaCall_Party784` | the last join Join(4) one id up | 3000 |
| A93 | `Scena14_Party74Other` | two saved ids searched, not three | 667 |
| A94 | `Scena14_RecountLeaveAll` | bit 1 counted, not bit 0 | 1483 |
| A95 | `Scena15_LeaveAllParty7` | 7 not sent away (0x51A300's test) | 1222 |
| A96 | `Scena15_Join42` | no palettes | 3000 |
| A97 | `Scena15_Party782` | the set's last two swapped | 3000 |
| A98 | `ScenaCall_Party034` | the count cleared after the first join | 242 |
| B1 | `Scena15_LeaveAllParty7` | the local zeroed, not the caller's ecx | 603 |
| B2 | `ScenaCall_SaveAndLeave` | ids saved for three members whatever the count | 2323 |
| B3 | `Scena10_Party75` | the loop from index 1 | 2232 |
| B4 | `Scena14_RecountLeaveAll` | the count not stored | 1770 |
| B5 | `ScenaCall_Party72Saved` | the saved ids not cleared | 2996 |
| B6 | `Scena13_SaveParty7` | the second list not saved | 2953 |
| B7 | `Scena10_RestoreSaved` | the set (s0, s2, s1) | 2314 |
| B8 | `Scena14_Party74Other` | the id read after the search fails is 0x903A10 | 275 |

The weakest, A1 (15 rounds), is a record index read before
`PartySet_Load` instead of after: refused only when the load's recorder moves
that `MoveScript_EffectState` byte (one stir in eight, one byte in 24).

## 6. Latent defects (Capcom's, kept; for the coordinator to number)

- **The caller's `ecx` as data** (`0x51A300` `ScenaCall_SaveAndLeave`,
  `0x51AB50` `Scena15_LeaveAllParty7`). Each gathers the field members' ids
  in a 4-byte local that is the caller's `ecx`, pushed (`push ecx`), one byte a
  member below `Field_MemberCount`, then reads the first three bytes whatever
  the count. With fewer than three members the rest are `ecx`'s bytes: through
  the call tables, the entry's index (`0x51A300` is chapter 9's B[6] and
  chapter 10's B[3], so `0x534030(6)` / `(3)` or `(0)`; `0x51AB50` is chapter
  15's A[0], so `0x534030(0)`). Ours reproduces it through a naked entry.
- **A member count above 4 overwrites the return address** (the same two).
  The copy loop runs `Field_MemberCount` times into the 4-byte local: a 5th
  byte lands on the return address, more on the caller's frame; the original
  then returns to what it wrote. Ours does everything the original does before
  that return and aborts with a message (the owner's rule for an unchecked
  index, round9 doc section 6: no DIVERGENCE entry). `Party_Join` compares the
  count with 3 before it adds a member (`0x533F2A`); what else writes the
  count was not surveyed.
- **Every second member skipped** (`0x51A3A0` `Scena10_Party75`). The loop
  sends member `i` away while `i < Field_MemberCount`, reading the count again
  after each call - but `0x534030` lowers the count and moves the later field
  members down one, so with three members the first and the third leave and
  the second stays (in the lists and on the field) when the new party joins.
  Kept.
- **Unchecked indexes**: every record reached through
  `MoveScript_EffectState[k]` (a byte, `CharacterRecords + 0xA4` x it) and
  every member through `ObjTrio + 0x14C i` for `i` up to the count, as
  wave one's groups found elsewhere.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D136 (reads and writes by an unchecked byte or count),
D147 (uninitialised bytes), D149 (every second member), D150 (the smashed
return) in [`known-defects.md`](known-defects.md).

## 7. What nothing reached

- A member count above 4 in `0x51A300` / `0x51AB50` (the abort): the seed
  stops at 4, since the clone would jump through the smashed return address.
- The real callees: every call is a recorder, so what a party change looks
  like - the set's files, the field members rebuilt, the palettes - is the
  recorded route's, not the fuzz's. No chapter here (1..15, 17..19) has a
  route yet; nothing was run live.

## 8. Cross-group calls

| Address | What | Owner |
|---|---|---|
| `0x534030` | the member leaves | nobody (wave three's engine group) |
| `0x533E00` | the members' palettes reloaded | nobody (listed in round10 doc section 3) |
| `0x519F70` `Scena08_PartyJoin784` | by name | SE (merged) |

No call into a wave-two chapter band (SC5, SC6, SC7, SC9a): the entries call
only engine code.

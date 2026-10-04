# Group R3D: Effect_Handlers' last eighteen slots, their helpers, and the Dragon command's part dispatcher

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave three, on
the boss harness's engine frame ([`boss_harness.md`](boss_harness.md)
section 10) without edits to it. **36 functions ours**
(`src/game/rest_3d.cpp`, shadow name `rest_3d`), each read to its last
instruction with capstone: the cut's 36 rows for R3D, every one a function
(none a jump-table case, a shared tail or data; none added). One `Run`,
216,000 rounds (6,000 a function), 0 mismatches; CONTROLS_SUMMARY (section
6). Two live routes enter five of them by the call traces under
`analysis/calltrace` (section 11); the rest are fuzz-only.

Names are this group's, from what the code does and who calls it. Only two
of the 36 have a PSX twin in `analysis/pairs_propagated.json` (`0x44F030`,
`0x44FF00`); the sibling's `names/*.toml` and `symbols.toml` name neither.
`Battle_PsiStatusDeathAffinity` is the name `symbols.toml` already had for
`0x44F030` (a hypothesis, kept); its twin is corrected (section 2). Nothing
here says what an ability or a status is in play: "the target" is the byte
`0x904B54`, "a status bit" a bit of a record's status word, as the code
handles them.

## 0. What the cut listed, and what the code has

`tools/band_rows.py --group R3D` (through the round's `band14.py`) against
`analysis/round14_cut.tsv`:

- **36 functions, 0 not listed, 0 flagged.** The 4,756 bytes read against
  the cut's 4,925: 17 extents differ only by trailing padding, none by code.
  Every extent in `symbols.toml` and `entries_logic.txt` is the code's.
- **The eighteen slots `0x44E4B0..0x44EC40` are hidden starts** in the
  catalogue extent of `Effect_ApplyResult` `0x44B9F0` (ours,
  `battle_damage.cpp`). Ours of `Effect_ApplyResult` calls every slot through
  `Effect_Handlers` (`0x64E73C`), so none is a branch of it: each is a
  function reached by its own table cell (`0x64E8FC..0x64E940`, slots
  112..129; a scan of the image finds each address once, in that cell).
- **`0x44FF00` is a hidden start** in `BattleForm_ApplyStats`' (BE5's)
  catalogue extent; ours of `BattleForm_ApplyStats` ends at `0x44FEF1`, and
  `Battle_MenuSteps[7]` (`0x64AE70`) reaches `0x44FF00` by address - a
  function of its own (BE5's doc called it "not ours, in no group").
- **The `hypothesis` row** (`0x44FC10`) is code: `Effect_RollStatStep`.
- **No code in the band is left out**: the gaps between the 36 are padding
  (`0x44F12D..0x44F130` and `0x44F451..0x44F460` among them) or functions
  already ours (`Effect_SkillDamage`, `Battle_ElementAffinity`,
  `Effect_HealAmount`, `Battle_ClearStatus`, `BattleForm_ApplyStats`).
- **BE5's doc calls `0x44EB50` slot 124**: its cell is `0x64E934`, slot
  126. Slot 124 is `0x44EA90`.

## 1. What each function does

The result record is `*0x904B60` (+4 the HP delta, +6 the AP delta, +8 a
mark, +0x14.. eight s8 stat steps); the actor `0x904B34` and the target
`0x904B54` are bytes, 0..2 a party member (ObjTrio `0x802D40`, stride
0x14C), above an enemy object at `0x93B960 + (actor - 3) * 0x128`, both
unbounded. A member's flag words are `+0x130` / `+0x134`, an enemy's
`+0x110` / `+0x114`; HP `+0x98` / `+0xA4`; the status word `+0x90` /
`+0x92`.

### 1.1 The slots (`Effect_Handlers` 112..129, `void (void)`)

| Slot | Address | Name | What (by the code) |
|--:|---|---|---|
| 112 | `0x44E4B0` | `Effect112_HpToOne` | Rand bit 0 set and `Battle_StatusResisted` answering 0: the HP delta the target's HP - 1 |
| 113 | `0x44E530` | `Effect113_ActorNullDamage` | `Effect_NoHitReaction`; the actor's flag 0x10000 (the flag `Battle_CalcDamage` and `Effect_SkillDamage` answer 0 for) |
| 114 | `0x44E580` | `Effect114_SkillApDamage` | the mark 2; the AP delta `Effect_SkillDamage(actor, target, the ability's +3 byte, 1)` (its psi branch) |
| 115 | `0x44E5D0` | `Effect115_HpToZero` | `Battle_StatusResisted20` answering 0: the HP delta the target's whole HP |
| 116 | `0x44E640` | `Effect116_HpLessDefence` | `Effect_HpBasedDamage(1)` less the target's DEF (`+0xA6` / `+0xB6`), at least 0; the side by the target read before the call, the record by the one after |
| 117 | `0x44E6C0` | `Effect117_PartyFlag400Others` | `Effect_NoHitReaction`; a member target: every member below the party size `0x904AB0` gets `+0x134` bit 10, then the target loses it |
| 118 | `0x44E720` | `Effect118_RaiseCharByte1E` | a member target: its character record's (`CharacterRecords + 0xA4 * +0x148`) byte `+0x1E` below 9 raised with the member's `+0x9E`; `Char_RecalcStats`; every member's equipment bytes `+0x92..+0x97` and block `+0xC0..+0xDF` from its character record; `Formation_ApplyStatMods`; `+0xC0..` back to `+0xA0..`; `BattleForm_ApplyStats`; `Battle_RecalcStats` per member; the HP delta minus the target's max HP `+0xA0` |
| 119 | `0x44E8B0` | `Effect119_ActorFlag1000` | `Effect_NoHitReaction`; the actor's `+0x134` / `+0x114` bit 12; `Battle_RecalcStats(target)` |
| 120 | `0x44E920` | `Effect120_TargetFlag800` | `Battle_StatusResisted`: the mark 1 and `Battle_SetDamagePopup(0, target)`; else the target's `+0x134` / `+0x114` bit 11 and `Battle_RecalcStats`; a tail jump to `Effect_NoHitReaction` either way |
| 121 | `0x44E9C0` | `Effect121_ActorFlag4000` | `Effect_NoHitReaction`; the actor's `+0x134` / `+0x114` bit 14 and its byte `+0x142` / `+0x122` zeroed; `Battle_RecalcStats(target)` |
| 122 | `0x44EA40` | `Effect122_InflictThree` | `Effect_NoHitReaction`; `Effect_RollInflict` with 0x20, 0x80 and 8 (three rolls) |
| 123 | `0x44EA70` | `Effect123_Ability6A` | the acting kind `0x904B35` 4 and the ability `0x904B80` 0x6A, a tail jump to R3C's `0x44D8B0` |
| 124 | `0x44EA90` | `Effect124_HalfHpLessDefence` | as slot 116 with `Effect_HpBasedDamage(2)` |
| 125 | `0x44EB10` | `Effect125_Inflict8Roll80` | `Effect_NoHitReaction`; `Battle_StatusResistedMask(actor, target, 0x80)` answering 0: `Battle_InflictStatus(target, 8)` |
| 126 | `0x44EB50` | `Effect126_Ability4EDrainAp` | the acting kind 4 and the ability 0x4E, a tail jump to `Effect_DrainAp` (BE5's) |
| 127 | `0x44EB70` | `Effect127_AttackDoubledKind4` | the HP delta `Battle_CalcDamage(actor, target, 0xFFFF)`; an enemy target (the read before the call) whose object's `+0x8D` (the target read again) is 4 doubled |
| 128 | `0x44EBE0` | `Effect128_HalfAttackInflict4` | the HP delta `Battle_CalcDamage(actor, target, 0xFFFF) / 2` (signed, toward 0); not 0 and `Battle_StatusResisted` answering 0: `Effect_RollInflict(4)` (a second roll) |
| 129 | `0x44EC40` | `Effect129_TargetSoleFlag40000` | `Effect_NoHitReaction`; for each actor not out, bit 18 cleared - **on the target's record, not the actor's** (section 7); then the target gets the bit |

### 1.2 The helpers

| Address | Name | What (by the code) |
|---|---|---|
| `0x44FB30` | `Effect_NoHitReaction()` | `0x904AA9` bit 5 (the round flags' 0x2000: `EnemyOp_ReceiveAction` and its party twin skip the hit sound and pop-up), the target's `+0x130` / `+0x110` bit 9 (they skip the hit pose) and `+0x12C` / `+0x10C` bit 0 cleared (the damage pop-up's flag they test). 23 sites in R3B and R3C (nine by `jmp`), ten here, `Effect_QuarterAttack`'s tail |
| `0x44F6A0` | `Battle_StatusResisted(actor, target)` | the status roll, al 1 resisted: a = min(INT `0x939FEA` / 5 + 50, 100), d = max(125 - `0x939F8A` / 5, 50), rate = s16 `Battle_StatusResistRate(target, the ability's mask)`; -1 is al 0; else Rand % 100 >= rate * d * a / 10000. The actor word is not read |
| `0x44F880` | `Battle_StatusResistedMask(actor, target, mask)` | the roll with the caller's mask & 0x1FF |
| `0x44F940` | `Battle_StatusResisted20(actor, target)` | the roll on `Battle_StatusResistRate20` |
| `0x44FA70` | `Battle_StatusResisted80(attacker, target)` | the roll with the mask 0x80: `Battle_ApplyDamage`'s weapon-status test (three sites) |
| `0x44F770` | `Battle_StatusResistRate(actor, mask)` | eax: the s16 of `Battle_StatusResistRates` (`0x64E96C`) by the class byte `+0xB5` / `+0xB6` / `+0xB7` (enemy `+0xC5..`) of the last of mask bits 0x40 / 0x80 / 0x100 set; 0xFFFF when none of 0x1C0 is |
| `0x44FA10` | `Battle_StatusResistRate20(actor, mask)` | eax: with the mask byte's 0x20, the s16 of `Battle_StatusResistRates20` (`0x64E98C`) by the class byte `+0xB4` / `+0xC4`; else 0 |
| `0x44F030` | `Battle_PsiStatusDeathAffinity(target, mask)` | eax: as `Battle_StatusResistRate` on `Battle_PsiAffinityRates` (`0x64E97C`), 0 when no bit is set. `Effect_SkillDamage`'s affinity when its psi byte is set |
| `0x44F1D0` | `Battle_InflictStatus(target, status)` | the status word read once (the original keeps it in its argument slot); for each bit of the status's byte, and 0x800, new to that word (and none of its blocking bits, never with 0x800): the bit into `0x904B98` (`0x904B99` bit 3 for 0x800), with `Battle_ClearStatus`, `Battle_ReturnQueuedItem`, `Battle_RemoveFromTurnOrder` for 0x40, 4 and 0x800; bit 0x40 needs `Battle_LacksAccessory(target, 6)`, 0x20 `(target, 8)` (and zeroes the actor's `0x93A009 + 0x84 n`, `+0x125`, `+0x130` bit 1, `+0x134` bit 2), 8 `(target, 9)` and `Battle_LacksArmour(target, 0x2B)`. Then `Sprite_ReleaseTint`, the word ORed with `0x904B98` (both read after the call), `Battle_StatusTint` |
| `0x44F460` | `Battle_LacksAccessory(actor, item)` | al 0 when the actor is a member and `item` is **Field_State's** `+0x96` or `+0x97` (the accessory bytes slot 118 copies from the character record's `+0x16` / `+0x17`); else 1 |
| `0x44F490` | `Battle_LacksArmour(actor, item)` | the same on Field_State's `+0x94` (the character record's `+0x14`) |
| `0x44F650` | `Effect_StepStatByte(step, stat)` | the result's s8 `+0x14 + stat` moved by the signed word: above 50 made 50, below -25 made -25 |
| `0x44FBB0` | `Effect_RollStatStepQuiet(stat)` | `Effect_NoHitReaction`; resisted al 1; else `Effect_StepStatByte(the ability's s8 +3, stat)`, `Battle_RecalcStats(target)`, al 0 |
| `0x44FC10` | `Effect_RollStatStep(stat)` | the same without the mark (`MagicFx_ApplyBuff`'s roll) |
| `0x44FC60` | `Effect_RollInflictQuiet(status)` | `Effect_NoHitReaction`; resisted al 1; else `Battle_InflictStatus(target, status)`, al 0 |
| `0x44FCA0` | `Effect_RollInflict(status)` | the same without the mark |
| `0x44FCE0` | `Effect_HpBasedDamage(divisor)` | eax: the actor's HP / divisor x `Effect_HpDamageVariance[Rand & 7]` x the affinity (100, or `Battle_ElementAffinity` when the ability's mask has an element bit) / 10000; ax zeroed for a target with flag 0x10000. Callers pass 1, 2, 3 |

### 1.3 The Dragon command's part dispatcher

`0x44FF00` `DragonCmd_PartDispatch` is `jmp [DragonCmd_Parts + 4 * byte
0x904AA3]` with no compare: `Battle_MenuSteps[7]`, reached through
`BattleMenu_ConfirmDispatch`. `DragonCmd_Parts` (`0x64ECCC`, 7, named by
BE5) holds BE5's six dispatchers and `DragonCmd_Open`. Ours aborts past the
seven where the original jumps through `DragonCmd_LoadSteps`' cells; it hands
the caller's word on and answers the part's eax, as the `jmp` does.

## 2. The tables named, and a twin corrected

`symbols.toml` `[[data]]`, each read by a function of this group (no values
copied): `Battle_StatusResistRates` `0x64E96C` (8), `Battle_PsiAffinityRates`
`0x64E97C` (8), `Battle_StatusResistRates20` `0x64E98C` (8; `Skill_HealByHoly`
follows at `0x64E99C`), `Effect_HpDamageVariance` `0x64E9AC` (8, by `Rand &
7`). The first three are indexed by a record's class byte, unbounded (their
counts are the spacing to the next table). `Effect_Handlers` and
`DragonCmd_Parts` were named already; their readers' entries are the slots
and `0x44FF00`.

**The twin of `0x44F030` was wrong.** `symbols.toml` paired it with PSX
`0x8009FD08`, and its note left `0x44F770` "the other candidate".
`Effect_SkillDamage`'s own entry (term for term against the PSX
`FUN_8009f820`) has the PSX call `0x8009FE88` where the PC calls `0x44F030`,
and the sibling's `BATTLE_RAM.md` gives `0x8009FE88` the table `0x800B189C`
and `0x8009FD08` the table `0x800B188C` - laid out as the PC's `0x64E97C`
and `0x64E96C`, the tables `0x44F030` and `0x44F770` read. So `0x44F030`'s
`psx` is now `0x8009FE88` and `0x44F770` carries `0x8009FD08` (by the
tables, not read on the PSX side).

## 3. Divergence

None: every function is a faithful replacement, and no `DIVERGENCE.md` entry,
`cheats.cpp` patch or `widescreen.cpp` operand names any of the 36 addresses
(grepped). No full-frame fill in the band. Ours aborts where the original
would jump past `DragonCmd_Parts` or divide by 0 (section 7); those are the
owner's rule for an unchecked index, not divergences.

## 4. Arguments and answers

- `al` compared (`ret_mask 0xFF`) on the eleven that answer a flag:
  `Battle_LacksAccessory`, `Battle_LacksArmour`, the four rolls, the four
  rolled helpers. Whole `eax` (`0xFFFFFFFF`) on the three rate lookups and
  `Effect_HpBasedDamage`, whose every path writes all 32 bits (the -1 path
  of a roll leaves the rate's upper bytes, so the rolls compare `al` only).
- Words handed on whole where the original pushes a whole register:
  `Battle_InflictStatus` hands its `target` word to `Battle_ClearStatus`,
  `Battle_ReturnQueuedItem` and `Battle_RemoveFromTurnOrder` (each reads the
  byte); its `Battle_StatusTint` word carries the record offset's upper half
  (the callee tests bit 7 only); `Effect_RollStatStep(Quiet)`'s
  `Battle_RecalcStats` word carries the stat word's upper bytes; slot 120
  hands the target's dword. Where the upper bytes are the original's
  leftovers (slot 118's frame slot for `Battle_RecalcStats` and slot 129's
  for `Battle_ActorIsOut`, the entry's `ecx`; slot 114's power word, the
  entry's `edx`), the callee reads less than the word and ours passes the
  byte; the masks in section 5 compare what the callee reads.

## 5. The fuzz

`src/game/rest_3d_fuzz.cpp`: one `boss_harness::Run` with `Group::engine`,
36 clones - the eighteen slots `kStep`; the seventeen helpers `kHelper`;
`DragonCmd_PartDispatch` `kDispatch` with `state_cell 0x904AA3` and `states`
7, `DragonCmd_Parts` a `DataTable` of one word (its recorders log the word
the `jmp` leaves) - 6,000 rounds a function. `BOF3X_R3D_ONLY=<substring>`
runs the clones whose name holds it.

**The listing** beyond the engine set, each mask what the callee reads
(capstone, cited in the file): the group's own called directly
(`Effect_NoHitReaction`; the rolls `{0, byte}` and `{0, byte, 0x1FF}`,
`kFlag`; the rates `{byte, 0x1C0}` / `{byte, 0x20}`; `Battle_InflictStatus`
`{byte, word}`; the two equipment tests `{byte, byte}`, `kFlag`;
`Effect_StepStatByte` `{word, byte}`; `Effect_RollInflict`;
`Effect_HpBasedDamage`), ours outside the engine set (`Effect_SkillDamage`
`{0, byte, word, byte}`, `Battle_ElementAffinity` `{byte, 0x1F}`,
`Battle_RecalcStats` `{byte}` as BE6 lists it, `BattleForm_ApplyStats`,
`Effect_DrainAp`), and R3C's `0x44D8B0` by address.

**Louder stand-ins**: the rate lookups answer 0xFFFF a third of the time
(`RateEffect`: the rolls stop at -1) and small rates of either sign;
`Battle_CalcDamage` and `Effect_HpBasedDamage` move the target half the
time, the old byte noted (slots 116, 124 and 127 pick the side by the read
before the call and the record by the read after: two controls went
unrefused, or nearly, until they did); `Effect_HpBasedDamage` answers within
2 of the new target's DEF half the time (the clamp at 0 is otherwise reached
once in 65,536).

**Regions** beyond the engine frame: `CharacterRecords` past the engine
frame's head to `0x903F90` (slot 118's eight records), and `0x803478` for
0x6F8 bytes (past `WindowRecords` to party index 10's `+0x134` dword: slot
129's party-indexed writes for an enemy target 5..10; 3 and 4, and slots 117
/ 118 at a party size of 4, land in `WindowRecords`).

**Seeds** (every function, after the harness's fill): the actor and the
target 0..10 (past 10 the enemy objects run off the image's end
`0x93F000`); the ability below 0x200 (its mask and step read from
`NameTable_Abilities`), 0x4E and 0x6A among them; the roll's two words at
their clamps (`0x939FEA` 249 / 250 / 254 / 255 / 499 / 500, `0x939F8A` 374 /
375 / 379 / 380); `0x904B98`; the party size 1..3, 0..4 a third of the time;
each side's class bytes 0..7, any byte a third of the time; the flag words
with 0x10000 half the time; the status words at each bit the inflict tests;
an enemy's `+0x8D` 4 half the time; the members' characters 0..7 and their
`+0x1E` at 7..10; Field_State's `+0x94` 0x2B and `+0x96` / `+0x97` 6, 8, 9.
**Arguments**: the actor / target bytes 0..10 with garbage above half the
time, the masks as the rates' bits alone and together, the status words bit
by bit, the step's word at the clamps' edges, the stat 0..7, the divisor 1,
2, 3, -1, -2, 100, 0x10000 or any word but 0.

**Disturbance** (the group's case, from the hash only): the target, the
actor, the party size, `0x904B98`, a member's or an enemy's status word, an
enemy's `+0x8D`, a member's max HP - the cells the functions read again
after a call.

Results (this worktree, 2026-10-04; counts depend on the build directory):

    SELFTEST_LINE

COVERAGE_LINE

`BOF3X_SHADOW='*'` in this worktree: STAR_RESULT

## 6. Controls

`BOF3X_R3D_ONLY=<clone>` with one change planted in ours at a time (the
scratch `controls.py`: plant on a unique anchor, rebuild, run, restore; one
rebuild at the end), this worktree, 2026-10-04. CONTROLS_SUMMARY.

CONTROLS_TABLE

**Not refused, and why.** CONTROLS_NOT_REFUSED

## 7. Latent defects and ranges (Capcom's, kept)

- **`Effect129_TargetSoleFlag40000` clears the target's record, not each
  actor's.** Its two loops test each actor 0..2 and 3..10 with
  `Battle_ActorIsOut`, but the `and` that follows indexes by the target
  `0x904B54` - as a party member in the first loop and as an enemy in the
  second, whatever the target is - so no other actor's bit 18 is ever
  cleared by this slot, and the wrong-side index writes elsewhere: for an
  enemy target (3..10) the party-indexed dword `0x802E74 + 0x14C t` lands in
  `WindowRecords` (t 3: window 6's bytes +0x20..+0x23, t 4: window 16's
  +4..+7) and past it (t 5..10, up to `0x803B6C`); for a member target the
  enemy-indexed dword `0x93BA74 + 0x128 (t - 3)` lands at `0x93B6FC`,
  `0x93B824` (task slots 44 and 46) or `0x93B94C` (past the 48 slots). Each
  write only clears bit 18 of a dword, once per actor not out. Ours copies
  it (control 30 plants the per-actor form and is refused). The fix and its
  ledger entry are the owner's; what in play reaches slot 129 was not
  measured.
- **The inflict's equipment tests read Field_State**, not the target's
  record (`Battle_LacksAccessory`, `Battle_LacksArmour`): an inflict on a
  member other than Field_State's member tests that member's accessories.
  Which member Field_State holds when `Effect_ApplyResult` runs was not
  measured.
- **`Battle_InflictStatus` tests every bit against the status word as it
  was on entry**: a `Battle_ClearStatus` made for 0x40 is not seen by the
  tests of 0x20, 8, 0x10, 4 and 0x800 after it. Kept as read.
- **Unbounded indexes, all kept**: the class bytes into the three rate
  tables (eight entries each; a byte past 7 reads the next table); the
  actor / target bytes into the party and enemy records (past 10 off the
  image); the party size `0x904AB0` in slots 117 and 118 (the members past
  3 are `WindowRecords` and beyond); the character byte `+0x148` into
  `CharacterRecords`; `Effect_StepStatByte`'s stat byte past the result's
  eight steps (its callers pass 0..3 and R3C's 0x44DCA0 its own).
- **Ours' two aborts**: `DragonCmd_PartDispatch` past `DragonCmd_Parts`' seven
  (every Dragon step keeps `0x904AA3` below 7, BE5's section 7) and
  `Effect_HpBasedDamage` on a divisor of 0 (an `idiv` fault in the original;
  every caller pushes 1, 2 or 3). Neither is reached by the fuzz, which
  draws the part below 7 and never a divisor of 0.

No function here reads memory it never wrote into what is drawn or decided:
**no ledger entry is owed** by this group.

## 8. Calls across groups, inbound

Out of this group, raw in `rest_3d_callees.h` until its owner merges:

| Callee | Owner | Called by |
|---|---|---|
| `0x44D8B0` | R3C (wave three) | `Effect123_Ability6A` (tail jump) |

Every other callee is ours already and called by name.

Into this group (inbound), for the rebinding pass:

| Caller | Owner | Callee |
|---|---|---|
| R3B's `0x44BED0`, `0x44C080`, `0x44C0A0`, `0x44C220`, `0x44C240`, `0x44C260`, `0x44C280`, `0x44CF40`; R3C's `0x44D350`, `0x44D420`, `0x44D450`, `0x44D6D0`, `0x44D850`, `0x44D9D0`, `0x44DB40`, `0x44DBC0`, `0x44DC10`, `0x44DCA0`, `0x44E260`, `0x44E2B0`, `0x44E330`, `0x44E3B0`, `0x44E400` | this wave | `Effect_NoHitReaction` `0x44FB30` (23 sites, nine by `jmp`) |
| R3B's `0x44BF70`, `0x44BFF0`, `0x44C330`, `0x44C940`; R3C's `0x44D290`, `0x44D350`, `0x44D850`, `0x44DF80` | this wave | `Effect_RollInflict` `0x44FCA0` |
| R3B's `0x44C380`, `0x44C7C0`, `0x44C7D0`, `0x44CC90`, `0x44CFC0`; R3C's `0x44D060`, `0x44D080` | this wave | `Effect_RollStatStepQuiet` `0x44FBB0` |
| R3B's `0x44C7E0`, `0x44C7F0`, `0x44CEF0`; R3C's `0x44D0E0`, `0x44D280`, `0x44D330`, `0x44D340` | this wave | `Effect_RollInflictQuiet` `0x44FC60` |
| R3B's `0x44C9F0`; R3C's `0x44D7E0`, `0x44D8B0` (the call at `0x44D8BD`), `0x44D9D0`, `0x44DBC0`, `0x44DD60`, `0x44DFD0` | this wave | `Battle_StatusResisted` `0x44F6A0` |
| R3B's `0x44C0A0`; R3C's `0x44D220`, `0x44D240`, `0x44D260` | this wave | `Effect_HpBasedDamage` `0x44FCE0` |
| R3C's `0x44D580` | this wave | `Battle_InflictStatus` `0x44F1D0` |
| R3C's `0x44DCA0` | this wave | `Effect_StepStatByte` `0x44F650` |
| `Battle_ApplyDamage` | ours (battle_damage) | `Battle_StatusResisted80`, `Battle_InflictStatus` (rebound, section 9) |
| `Effect_SkillDamage` | ours (battle_damage) | `Battle_PsiStatusDeathAffinity` (rebound) |
| `EnemyAI_ApplyAction`, `Effect_DrainHp`, `Effect_DrainAp`, `Effect_QuarterAttack` | ours (battle_e5) | `Battle_InflictStatus`, `Battle_StatusResisted`, `Effect_NoHitReaction` (rebound) |
| `MagicFx_ApplyBuff` | ours (magic_lib) | `Effect_RollStatStep` (rebound) |
| `Effect_ApplyResult` | ours (battle_damage) | the eighteen slots, through `Effect_Handlers` (read in place) |
| `BattleMenu_ConfirmDispatch` | ours | `DragonCmd_PartDispatch`, through `Battle_MenuSteps[7]` (read in place) |

## 9. The rebinding

Every raw reference to an R3D function in our files that is not a fuzz key,
rebound in the round-ten form (the value unchanged, so the fuzz keys stand):

- `battle_damage.cpp`'s `kOriginals`: `0x44FA70`, `0x44F1D0`, `0x44F030` ->
  `bof3::addr::Battle_StatusResisted80`, `Battle_InflictStatus`,
  `Battle_PsiStatusDeathAffinity` (a line beside the include says so);
- `battle_e5_callees.h`: `kInflict`, `kResisted`, `kMissTail` ->
  `bof3::addr::Battle_InflictStatus`, `Battle_StatusResisted`,
  `Effect_NoHitReaction`;
- `magic_lib.cpp`: `kBuffRoll` -> `bof3::addr::Effect_RollStatStep`.

**Left raw on purpose**: the fuzz files' keys (`battle_damage_fuzz.cpp`'s
`case 0x44F030 / 0x44F1D0 / 0x44FA70` stand-ins and `CallSite` rows,
`battle_e5_fuzz.cpp`'s and `magic_lib_fuzz.cpp`'s listings and `CallSite`
rows, `battle_phases_fuzz.cpp`'s `kMenuTargets` with `0x44FF00`), comments
that describe the original's calls by address, and **`boss_harness.cpp`'s
three `kEngineStandard` rows `"0x44F1D0"`, `"0x44F6A0"`, `"0x44FB30"`**
(not this group's file to edit). They still work: a raw row's key is its
address, so every caller that calls those addresses raw resolves through
it; this group lists the three by name in its own callees, which register
first. Each row against the code: `0x44F1D0` `{kAll, kAll}` compares more
than the callee reads (its byte and the status's low word) - right, only
wider; `0x44F6A0` `{0, kU8}`, `kFlag` - matches; `0x44FB30` no words,
garbage - matches. No row of `scenario_harness*` or another harness names
an R3D address.

**For the coordinator**: R3B's and R3C's files (this wave) will call the
helpers above by address; once this group merges first (section 4 of the
round doc: R3D, R3B, R3C) they can call them by name.

## 10. For `analysis/calltrace/entries_logic.txt`

The 36 extents of section 1 (the code's). Seventeen were there already with
the same extents (`0x44F030 FD` .. `0x44FCE0 F2`, the catalogue's); the
nineteen others - the eighteen slots and `0x44FF00 E` - were appended to the
main checkout's file on 2026-10-04 under a comment line. No host line covers
any of them (`Effect_ApplyResult`'s `0x44B9F0 750` ends at `0x44C140`,
`BattleForm_ApplyStats`' `0x44FDE0 112` at `0x44FEF2`).

## 11. The live route

The catalogue's `reach` column marks the eighteen slots (their host,
`Effect_ApplyResult`, was entered: an upper bound) and none of the others.
The call traces under `analysis/calltrace` (every `bof3x.callcounts.tsv`;
before today the slots and `0x44FF00` were not listed entries, so no trace
could count them) show:

- **`dragonTransform.txt`** (`recipe_dragon`, `hash*_dragon_*`):
  `Effect_HpBasedDamage` once, called from `0x44EAC8` - inside
  `Effect124_HalfHpLessDefence`, its enemy-side call - and
  `Effect_NoHitReaction` once (from a tail `jmp`, caller unknown). So the
  route enters slot 124. It also passes through parts 1 and 4 of
  `DragonCmd_Parts` (BE5's section 10), which only `0x44FF00` reads: by
  inference it enters `DragonCmd_PartDispatch`.
- **`cutsceneAndNue.txt`** (`hash_r13_nue_*`): `Effect_RollInflict` once from
  `0x44C372` (R3B's `0x44C330`), and under it `Battle_StatusResisted`,
  `Battle_StatusResistRate` and `Battle_InflictStatus` once each.
- `combat.txt` and the boss routes: none of the 36.

With this group's lines in `entries_logic.txt`, the coordinator's state-hash
check of `dragonTransform.txt` and `cutsceneAndNue.txt` is the live check of
these five or six; the rest are fuzz-only.

## 12. What nothing reached

- Ours' two aborts (section 7): the fuzz draws the part below 7 and the
  divisor never 0, as the originals would fault or run foreign code there.
- Actor and target bytes past 10 (both sides would run off the image).
- Every one of the 36 was called 6,000 times and every recorder of the
  listing was reached (section 5's coverage line); slot 118's body runs only
  for a member target (about 1,600 rounds of its 6,000).

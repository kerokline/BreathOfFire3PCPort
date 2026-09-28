# Group BSJ: fights 52, 54, 55, kinds 59 and 61, the effect tasks of slots 4 and 5

**Status:** MEASURED (2026-09-28) - round eleven ([`takeover-queue-bosses.md`](takeover-queue-bosses.md)),
wave two. 44 functions of `0x43F7A0..0x441087` ours (`src/game/boss_sj.cpp`,
shadow `boss_sj`), each read to its last instruction with capstone and
fuzzed through the boss harness ([`boss_harness.md`](boss_harness.md)),
seven `Run`s, 0 mismatches in 264,000 rounds; 128 controls planted, 127 refused by a count, one equivalent with its near variant refused (section 4). The
group's fifth unit, FB93 (`HeadCrackerRock_*`, five functions), was ours
already (the spell round's row 128) and nothing here calls it. **The first
users of the harness's `kTask` shape.** Fuzz only: no recorded route
reaches a boss fight. No divergence.

Enemy and fight names are `tools/boss_rows.py --disc`'s (the US disc's area
records), not memory of the game. Which fight a set-up is comes from the
tool's rows; nothing here says what happens in a fight.

## 1. The units and their functions

`analysis/boss_funcs.tsv` (regenerated after wave one, so its group column
now reads `BSE` for set-ups 52..55 and F4, F5 and `BSD` for K59 - the brief's
units are the cut): 49 functions, 44 to take, all taken.

| Unit | Fight / enemy (the tool) | Functions | Sibling's image (static roots) |
|---|---|--:|---|
| K59 | D>Lord, area 172 | 6 | BOSS052 (12) |
| B52 | id 52: D>Lord, area 172 row 7 | 3 | BOSS052 |
| F4 | `BattleBossFx_Dispatch` slot 4: D>Lord's effect (kind 59's hook creates it) | 4 | BOSS052 |
| K61 | Shroom, area 119 | 3 | BOSS054 (16; HugeSlug, kind 60, is BSI's) |
| B54 | id 54: Shroom, area 119 row 6 | 2 | BOSS054 |
| B55 | id 55: Myria, area 198 row 7 (kind 62 is BSF's) | 2 | BOSS055 (55, with kind 62's 28) |
| F5 | slot 5: Myria's effect (kind 62's `0x440660` creates it: `BattleTask_Create(3, 5)`) | 24 | BOSS055 |
| FB93 | `BattleMagicFx_Dispatch` slot 93, the Head Cracker rock | 5, ours | - |

BOSS052's 12 against 13 here; BOSS055's 55 against 54 (kind 62's 28, B55's
2, F5's 24): the unit sets are the images' size.

**One start dropped, one extent corrected.** The tool cuts F5's last
function at `0x44103A` and lists `0x44103A` as frontier engine code; it is
`0x440EF0`'s own branch (a `je` at `+0xFD`, the `esi` its prologue pushed
popped there, no other reference in the image - an E8 / E9 / imm32 scan of
the exe, 2026-09-28). So `BossMyriaFx_Follow` is `0x440EF0..0x441087`,
0x197 bytes (the main checkout's `entries_logic.txt` already said so), and
its clone row holds four calls to `0x441090`, not two; the tool's row
refused the `je` out. No start added. The tool's table extents are long as
always: kind 59's `+1` table ("22 code entries") is 12, its `+2` table 6,
its hook table 3; kind 61's 12 and 3; F4's state table 1 (its step
table is on the stack); F5's 7.

### Kind 59 (D>Lord) and set-up 52

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43F7A0` | `BossDLord_Dispatch` | 0x12 | `BossKind_Table[59]`: `jmp [BossDLord_States + 4 * +1]`, 12 |
| `0x43F7C0` | `BossDLord_Enter` | 0x51 | state 0: `0x939AD8` `+0xFC` `BossDLord_Anims`, `+0xF4` `BossDLord_Hook`, `+0xF8` `BossDLord_Sounds`, `+0x114 \|= 8`; `+1` = 2; tail `Sprite_ScriptTick` (`al`) |
| `0x43F820` | `BossDLord_ActDispatch` | 0x12 | state 6: `BossDLord_ActSubs` by `+2` (the generic act table, its death at 4) |
| `0x43F840` | `BossDLord_Death` | 0x52 | `Sprite_EnsureAnimation(3)`, `Sprite_ScriptTickOnce`, `Battle_EnemyDefeated`; `0x939AD8 +0x110 \|= 0x1000`, `+0 &= 0xBF`, states 3, 0, 0 |
| `0x43F8A0` | `BossDLord_Hook` | 0x10 | `+0xF4`: `BossDLord_Hooks` by the word's low byte (`BareRet`, `BareRet`, `BossDLord_HookFx`) |
| `0x43F8B0` | `BossDLord_HookFx` | 0x9E | word 2 (`BattleEnemy_RunAll`'s): at `+1` 7, `+2` 1, `+9` 0 - `BattleTask_Create(3, 4)` (slot 4 of `BattleBossFx_Dispatch`: F4), the acting enemy's object (`0x93B960 + (0x904B34 - 3) * 0x128`, the actor read after the call) copied into the slot, 0x20 dwords forward, then the slot's `+1` 0, `+2` 0, `+6` 3, `+5` 4, `+9` 0, `+0x29` 3 |
| `0x43F950` | `Boss52_Setup` | 0x1F | end `Boss52_End`, exit `Boss52_Exit`, event `BareRetZero` |
| `0x43F970` | `Boss52_End` | 0x117 | not won: tail `0x446E00`; won: each member with `+0` bit 0 - `Battle_RemoveFromTurnOrder(+5)`, `Sprite_Current` = it, `Sprite_PoseFromSet(+8 + (0x1C with +0x90 bit 14, else 4), 0x8C5D80, 0x1800)` (`+0x90`, `+8` read after the call) - then chapter step `0x8034E5` = 0x1A, tail `0x446DE0` |
| `0x43FA90` | `Boss52_Exit` | 0x23 | `BossActor_ClearBit40(0)`; `BossActor_Find(0)` into `Field_ActiveMember` and `Sprite_Current`; `Sprite_SetAnimation(5)` |

### D>Lord's effect task (slot 4, `kTask`)

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43FAC0` | `BossDLordFx_Dispatch` | 0x12 | `jmp [BossDLordFx_States + 4 * +1]`, one entry |
| `0x43FAE0` | `BossDLordFx_StepDispatch` | 0x2E | a stack table by `+2`: `BossDLordFx_Start`, `BossDLordFx_Count`, `BattleFx_FreeTask` |
| `0x43FB10` | `BossDLordFx_Start` | 0x4B | `+0xB` = 0; `+9` = enemy 0's data record's `+0x8A` (the byte at `0x8C5652 + 0x8C *` enemy 0's `+0xF0`, `BattleActor_FxSize`'s cell); `Sprite_SetAnimation(4)`, `Sprite_ScriptTick`, `Sprite_QueueOverlay`; `+2` up |
| `0x43FB60` | `BossDLordFx_Count` | 0x4A | the count `+9`: 0xFF stays; 0 plays `Sound_PlayEffect(0x603)` and becomes 0xFF; else down one. Tick, overlay; with round-flag bit 2 `+2` up (to `BattleFx_FreeTask`) |

### Kind 61 (Shroom), set-ups 54 and 55

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43FBB0` | `BossShroom_Dispatch` | 0x12 | `BossKind_Table[61]`: `BossShroom_States` by `+1`, 12 (the generic act dispatcher at 6) |
| `0x43FBD0` | `BossShroom_Enter` | 0x51 | as D>Lord's entry with `BossShroom_Anims`, `_Hook`, `_Sounds` |
| `0x43FC30` | `BossShroom_Hook` | 0x10 | `BossShroom_Hooks` (3, `BareRet` each) |
| `0x43FC40` | `Boss54_Setup` | 0x1F | end `Boss54_End`, exit `BareRet`, event `BareRetZero` |
| `0x43FC60` | `Boss54_End` | 0x1A | won: movement-script variable 3 (`0x903848`) = 0x32, tail `0x446DE0`; else `0x446E00` |
| `0x4406E0` | `Boss55_Setup` | 0x1F | end `Boss55_End`, exit `BossHook_ExitActor0Bit40`, event `BareRetZero` |
| `0x440700` | `Boss55_End` | 0x11F | not won: tail `0x446E00`; won: the party posed as `Boss52_End`'s, chapter step = 0xF, **`0x446E20` called** (step 3, not a tail), then `Music_Track` = 0xFF |

### Myria's effect task (slot 5, `kTask`)

`BossMyriaFx_Dispatch` (`0x440830`, 0x1C) sets `0x939AD8` = enemy 0's object
and jumps through `BossMyriaFx_States` by `+1` (7). Each state is a stack
table by `+2` (`_StateN`, 0x26; state 6's 0x2E has three); the owner is
`0x93B940` (Myria's object: kind 62 stores `0x93B960` in the slot's `+0x80`).
Every step but the waits' early exits ends in `BossMyriaFx_Follow`.

| State | Dispatch | Step 0 (`Enter`) | Step 1 | Step 2 |
|---|---|---|---|---|
| 0 | `0x440850` | `0x440880` (0x38): bank 0x30D; `+0x48`, `+0x24`, dword `+0x3C` = 0; `_State0Loop` called; `+2` up | `0x4408C0` `_State0Loop` (0x62): with the owner's `+0xB`, `Sprite_SetAnimation` of a ten-byte stack table by the word `0x904B7E`; `BattleEnemy_ScriptTick`, `Sprite_QueueOverlay`, Follow | |
| 1 | `0x440930` | `0x440960`: bank 0x30C, as above | `0x4409A0` (0x64): as above, its own poses | |
| 2 | `0x440A10` | `0x440A40`: bank 0x30C | `0x440A80` (0x64) | |
| 3 | `0x440AF0` | `0x440B20` (0x86): bank 0x30D, the fields cleared, its pose always, `BattleEnemy_ScriptTickOnce`, overlay, Follow, `+2` up | `0x440BB0` `_State3Wait` (0x1F): `BattleEnemy_ScriptTick`; the owner's `+1` at 2 - tail `BattleTask_FreeCurrent`; else overlay, tail Follow | |
| 4 | `0x440BD0` | `0x440C00` (0x84): bank 0x318, pose, tick once | `0x440C90` `_State4Wait` (0x2E): when `BattleEnemy_ScriptTickOnce` answers `al` not 0, the owner's `+1` up one and `+2` = 0, tail `BattleTask_FreeCurrent`; else overlay, Follow | |
| 5 | `0x440CC0` | `0x440CF0` (0x86): bank 0x318, pose, tick each frame | `0x440D80` `_State5Wait` (0x1F): the owner's `+1` at 2 - free; else tick, overlay, Follow | |
| 6 | `0x440DA0` | `0x440DD0` (0x86): bank 0x318, pose, tick | `0x440E60` `_State6Loop` (0x6E): with the owner's `+0xB`, pose and `+2` up; tick, overlay, Follow | `0x440ED0` `_State6Wait` (0x20): tick once answered - the owner's `+2` up, free; else overlay, Follow |

`0x440EF0` `BossMyriaFx_Follow` (0x197): the owner's `+0`, `+0x48`, dwords
`+0x40` / `+0x44`, `+0x27`, `+0x28`, `+0x2A`, words `+0x2E` / `+0x30` /
`+0x32`, `+0x5C..+0x5F` copied to `Sprite_Current`; then the words `+0x2E`
and `+0x30` drift by the state's pair of `BossMyriaFx_Drift`: with `+0x48`,
`+= 0x441090(s16 * dword +0x40, that dword)` (a 16.16 product's high word,
rounded up - [`area_w4f.md`](area_w4f.md)), without, `+= 0x441090(s16 << 16,
0x10000)` (the s16 itself). The second axis reads `Sprite_Current` again
after the first call; each add lands in the word of the sprite read before
its own call. `Area198_EffectA6Follow` (`0x42D580`, area 198 is Myria's
area) is the same shape.

### Named data (`symbols.toml`, 12 `[[data]]`)

`BossDLord_Anims` (12 bytes, `+0xFC`), `BossDLord_Sounds` (4 words, `+0xF8`),
`BossDLord_States` (12), `BossDLord_ActSubs` (6), `BossDLord_Hooks` (3),
`BossDLordFx_States` (1), `BossShroom_Anims` (12), `BossShroom_Sounds` (6
words, to the next table), `BossShroom_States` (12), `BossShroom_Hooks` (3),
`BossMyriaFx_Drift` (7 s16 pairs), `BossMyriaFx_States` (7).

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed (DIVERGENCE.md and `cheats.cpp` grepped for the 44 addresses and the
tables: nothing). Two things ours expresses differently, neither a change of
behaviour: the kinds' dispatchers take their caller's stack word and hand it
on, answering the entry's eax, as the original's `jmp` does
([`boss_sc.md`](boss_sc.md) section 1.2); and where the original indexes
past a table ours aborts with a `Fatal` naming the function (round9 doc
section 6) - the dispatchers and hook tables, the stack tables by `+2`, the
ten-byte pose tables by `0x904B7E`, `BossMyriaFx_Drift` by `+1`, and a task
slot of 0xFF (section 6). Nothing reached one.

## 3. The fuzz

`BOF3X_SHADOW=boss_sj`, `src/game/boss_sj_fuzz.cpp`, seven `Run`s of 6,000
rounds a function (`BOF3X_BSJ_RUN=k59|b52|f4|k61|b54|b55|f5` runs one):

| Run | Fight, kind | Clones (shape) | `.data` tables swapped | Result (this worktree) |
|---|---|---|---|---|
| `k59` | 52, 59 | dispatcher, act dispatcher (`kDispatch`, `+1` < 12, `+2` < 6), entry, death (`kState`), hook, `HookFx` (`kEnemyHook`) | hook table (one word) first, `+1`, `+2` | 36,000 rounds, 42,215 calls, 0 mismatches |
| `b52` | 52 | set-up, end, exit | | 18,000, 35,666, 0 |
| `f4` | 52 | the four (`kTask`) | `BossDLordFx_States` | 24,000, 43,185, 0 |
| `k61` | 54, 61 | dispatcher, entry, hook | hook table, `+1` | 18,000, 18,000, 0 |
| `b54` | 54 | set-up, end | | 12,000, 6,000, 0 |
| `b55` | 55 | set-up, end | | 12,000, 17,602, 0 |
| `f5` | 55 | the 24 (`kTask`) | `BossMyriaFx_States` | 144,000, 352,421, 0 |

264,000 rounds in all. Coverage (the originals' calls, this worktree, the
final build): every entry of every swapped table (`phase 0x4363B0` .. `phase 0x437420` about 500 each in `k59` and `k61`, `BossDLord_HookFx` 1,945 through the hook table, each of F5's seven states 830..900 through `BossMyriaFx_States`), and every stack-table handler (F5's steps 1,950..9,040; F4's 1,920 / 1,999 / `BattleFx_FreeTask` 2,081); `k59`: `BattleTask_Create` 215 (`HookFx` past its three tests), `Port_DroppedCall` 976; `b52` / `b55`: `Sprite_PoseFromSet` 5,833 / 5,801; `f4`: `Sound_PlayEffect` 1,185; `f5`: `BossMyriaFx_Follow` 56,362, `0x441090` 12,000, `BattleTask_FreeCurrent` 15,638, `Sprite_SetAnimation` 35,915. Counts move with the build directory; judge by 0
mismatches.

**`kTask`, first use.** The shape works as documented: `Sprite_Current` and
`0x93B8C4` a task slot, the owner `0x93B940` one of the four slots or the
harness's two records. What it lacks, done in the group's seed (for the
coordinator to fold back if wanted):

- **No state draw.** `Clone::states` is read only for `kDispatch`, and
  `Fix` draws the four slots' `+1` / `+2` below 3; F5's dispatcher and
  `Follow` index by `+1` up to 6, so the seed draws `+1` below 7 in all four
  slots and the stack tables' `+2` below their 2 or 3.
- **The owner may be `Sprite_Current` itself** (`OwnerFor` picks among the
  same four slots): a seed that writes the owner's bytes must write the
  task's own state bytes after them. Met once (the first `f4` run faulted:
  the owner's `+1` overwrote the dispatcher's 0, and the original jumped
  through the dword after its one-entry table). A disturbance that writes
  the owner's `+1` must keep it below 7 for the same reason.

**Callees added to the standard set** (group listings): `Sprite_PoseFromSet`
and `Battle_RemoveFromTurnOrder` as `boss_se_fuzz.cpp` lists them (a byte
read; the latter louder than the real one, moving a member's `+0x91`,
`+8` or `+0` that the end hooks read after it); `0x441090` (two whole
words); `BossMyriaFx_Follow` itself as a `kPhase` recorder at `0x440EF0` (the
states call it directly). `Port_DroppedCall` keeps the standard one-byte
listing: the dispatchers hand the word on, so both passes give it the
harness's word (control D2).

**Seeds.** The kinds: every dispatcher's other state bytes inside their
tables; `HookFx`'s three tests at and beside their values (`+1` 7 / 6 / 8,
`+2` 1 / 0 / 2, `+9` 0 / 1 / 0xFF) and the acting enemy 3..10 two times in
three (the harness draws a member, whose "object" `0x93B960 - (3 - actor) *
0x128` lies inside the task slots, a compared region); `DisturbKind` moves
the actor, which `HookFx` reads after `BattleTask_Create`. The end hooks:
`0x904AE8` with bit 1 and without, bit 0 alone, garbage; each member's `+0`
bit 0 two times in three, `+8` where the byte add wraps (0xE3, 0xE4, 0xFC),
`+0x90` bit 14 either way. The tasks: `+1` / `+2` as above, the pose word
`0x904B7E` below 10 (its ends 0 and 9 often), the owner's `+0xB` zero half
the time, its `+1` at 2 two times in three, `+0x48` set or not, the scales
`+0x40` / `+0x44` at 0x10000, 0x18000, 0, 1, negative, `0x7FFF0000`, 0x8000
or any; D>Lord's count at 0, 1, 2, 0xFE, 0xFF; enemy 0's record index
below 8 two times in three. `DisturbTask` moves the owner's `+1` (below 7)
and `+0xB`, the pose word, round-flag bit 2. `phase_span` 7 for the tasks.

`BOF3X_SHADOW='*'` (every group of every harness, this worktree): @STAR@

## 4. Controls

`python controls.py` (the group's scratch script): each control one textual
change to ours anchored on a string that occurs once, rebuild, the one
`Run` that holds the function, restore, rebuild at the end. A plant in a
helper several functions share (`Dispatch`, `Steps`, `FxEnter`,
`FxEnterPosed`, `PoseParty`, `Drift`) is keyed on the function where it can
be, else named for the function whose run shows it (the others' counts
follow "also"). Run once, on the final seeds. **128 planted, 127 refused by a count, none by a Fatal; one equivalent** (D9: `BossDLord_ActDispatch` dropping its caller's word - no entry of its table reads one; D2, the same plant in `BossDLord_Dispatch`, refused in 976 rounds on `Port_DroppedCall`'s states 1 and 10). The thinnest refusals are the re-read plants, refused only when the disturbance moves the cell in that call: D22 13 rounds (the actor read before `BattleTask_Create`), M28 14 (the pose word read before the bank call), F11 38, M41 97, M24 150, and the `Sprite_Current` re-reads of `FxEnter` / `FxEnterPosed` near 180..200.

| # | Function | Plant | Refused |
|---|---|---|---|
| D1 | `BossDLord_Dispatch` | the next entry | 6000 rounds |
| D2 | `BossDLord_Dispatch` | the word not handed on | 976 rounds |
| D3 | `BossDLord_Enter` | +0x114 bit 4 | 4529 rounds |
| D4 | `BossDLord_Enter` | Shroom's hook | 6000 rounds |
| D5 | `BossDLord_Enter` | +0xFC and +0xF8 swapped | 6000 rounds |
| D6 | `BossDLord_Enter` | al \| 1 | 4007 rounds |
| D7 | `BossDLord_Enter` | state 3 | 5972 rounds |
| D8 | `BossDLord_ActDispatch` | the next entry | 6000 rounds |
| D9 | `BossDLord_ActDispatch` | the word not handed on | NOT REFUSED (equivalent: every entry of `BossDLord_ActSubs` is a generic act step that reads no argument, so the word it would see is never read; D2, the same plant in `BossDLord_Dispatch`, whose table holds `Port_DroppedCall`, refused) |
| D10 | `BossDLord_Death` | animation 4 | 6000 rounds |
| D11 | `BossDLord_Death` | +0x110 bit 13 | 4471 rounds |
| D12 | `BossDLord_Death` | +0 bit 7 cleared too | 2995 rounds |
| D13 | `BossDLord_Death` | +1 = 4 | 6000 rounds |
| D14 | `BossDLord_Death` | 0x939AD8 read before the calls | 427 rounds |
| D15 | `BossDLord_Death` | Sprite_Current read before the calls | 643 rounds |
| D16 | `BossDLord_Hook` | the word's second byte flipped | 6000 rounds |
| D17 | `BossDLord_Hook` | the entry given the masked word | 3058 rounds |
| D18 | `BossDLord_HookFx` | +1 at 6 | 289 rounds |
| D19 | `BossDLord_HookFx` | +2 at 2 | 299 rounds |
| D20 | `BossDLord_HookFx` | +9 not tested | 435 rounds |
| D21 | `BossDLord_HookFx` | parameter 5 | 215 rounds |
| D22 | `BossDLord_HookFx` | the actor read before the call | 13 rounds |
| D23 | `BossDLord_HookFx` | 0x7C bytes | 215 rounds |
| D24 | `BossDLord_HookFx` | +0x29 = 2 | 215 rounds |
| D25 | `BossDLord_HookFx` | +5 = 3 | 215 rounds |
| D26 | `BossDLord_HookFx` | the enemy one on | 215 rounds |
| D27 | `BossDLord_HookFx` | +6 = 2 | 215 rounds |
| D28 | `BossDLord_HookFx` | +8 for +9 | 215 rounds |
| B1 | `Boss52_Setup` | the exit hook the end hook | 6000 rounds |
| B2 | `Boss52_Setup` | event BareRet | 6000 rounds |
| B3 | `Boss52_End` | step 0x1B | 2967 rounds |
| B4 | `Boss52_End` | the win by bit 0 | 3500 rounds |
| B5 | `Boss52_End` | pose +0x1D (PoseParty) | 2085 rounds |
| B6 | `Boss52_End` | bit 13 (PoseParty) | 2089 rounds |
| B7 | `Boss52_End` | +0x90 read before the call (PoseParty) | 631 rounds |
| B8 | `Boss52_End` | size 0x1000 (PoseParty) | 2926 rounds |
| B9 | `Boss52_End` | member bit 1 (PoseParty) | 2656 rounds |
| B10 | `Boss52_End` | the actor from +4 (PoseParty) | 2924 rounds |
| B11 | `Boss52_Exit` | animation 6 | 6000 rounds |
| B12 | `Boss52_Exit` | Field_ActiveMember not set | 6000 rounds |
| B13 | `Boss52_Exit` | bit 0x40 of actor 1 | 6000 rounds |
| B14 | `Boss52_Exit` | actor 1 found | 6000 rounds |
| F1 | `BossDLordFx_Dispatch` | step 0 entered directly | 6000 rounds |
| F2 | `BossDLordFx_StepDispatch` | the next entry | 6000 rounds |
| F3 | `BossDLordFx_StepDispatch` | steps 1 and 2 swapped | 3992 rounds |
| F4 | `BossDLordFx_Start` | +0xB = 1 | 5880 rounds |
| F5 | `BossDLordFx_Start` | the record's +0x89 | 3984 rounds |
| F6 | `BossDLordFx_Start` | enemy 1's index | 4018 rounds |
| F7 | `BossDLordFx_Start` | animation 5 | 6000 rounds |
| F8 | `BossDLordFx_Start` | +2 on the Sprite_Current of before the calls | 517 rounds |
| F9 | `BossDLordFx_Count` | the sound at 1 | 1763 rounds |
| F10 | `BossDLordFx_Count` | sound 0x604 | 1148 rounds |
| F11 | `BossDLordFx_Count` | +9 on the Sprite_Current of before the call | 38 rounds |
| F12 | `BossDLordFx_Count` | round-flag bit 3 | 3015 rounds |
| F13 | `BossDLordFx_Count` | 0xFE as the stop | 1724 rounds |
| S1 | `BossShroom_Dispatch` | the next entry | 6000 rounds |
| S2 | `BossShroom_Dispatch` | the word not handed on | 975 rounds |
| S3 | `BossShroom_Enter` | D>Lord's hook | 6000 rounds |
| S4 | `BossShroom_Enter` | +0x114 bit 2 too | 2981 rounds |
| S5 | `BossShroom_Enter` | D>Lord's animations | 6000 rounds |
| S6 | `BossShroom_Hook` | the word's second byte flipped | 6000 rounds |
| E1 | `Boss54_Setup` | exit BareRetZero | 6000 rounds |
| E2 | `Boss54_End` | variable 3 = 0x33 | 3007 rounds |
| E3 | `Boss54_End` | the win by bit 0 | 3437 rounds |
| E4 | `Boss54_End` | step 3 for the win | 3007 rounds |
| G1 | `Boss55_Setup` | exit BareRet | 6000 rounds |
| G2 | `Boss55_End` | step 0xE | 2935 rounds |
| G3 | `Boss55_End` | track 0xFE | 3007 rounds |
| G4 | `Boss55_End` | step 1 for step 3 | 3007 rounds |
| G5 | `Boss55_End` | the win by bit 0 or 1 | 1473 rounds |
| G6 | `Boss55_End` | pose +5 (PoseParty) | 2080 rounds |
| G7 | `Boss55_End` | the party not posed | 2906 rounds |
| M1 | `BossMyriaFx_Dispatch` | 0x939AD8 enemy 1 | 6000 rounds |
| M2 | `BossMyriaFx_Dispatch` | the next entry | 6000 rounds |
| M3 | `BossMyriaFx_State0` | the next entry | 6000 rounds |
| M4 | `BossMyriaFx_State0Enter` | bank 0x30E | 6000 rounds |
| M5 | `BossMyriaFx_State0Enter` | state 1's loop | 6000 rounds |
| M6 | `BossMyriaFx_State0Enter` | +0x48 on the Sprite_Current of before the call (FxEnter) | 198 rounds (also BossMyriaFx_State1Enter 184, BossMyriaFx_State2Enter 202) |
| M7 | `BossMyriaFx_State1` | the next entry | 6000 rounds |
| M8 | `BossMyriaFx_State1Enter` | bank 0x30D | 6000 rounds |
| M9 | `BossMyriaFx_State1Enter` | +0x24 = 1 (states 1 and 2) | 6000 rounds (also BossMyriaFx_State2Enter 6000) |
| M10 | `BossMyriaFx_State2` | the next entry | 6000 rounds |
| M11 | `BossMyriaFx_State2Enter` | bank 0x30B | 6000 rounds |
| M12 | `BossMyriaFx_State2Enter` | +2 on the Sprite_Current of before the loop (FxEnter) | 191 rounds (also BossMyriaFx_State0Enter 193, BossMyriaFx_State1Enter 185) |
| M13 | `BossMyriaFx_State0Loop` | pose 4 = 1 | 365 rounds |
| M14 | `BossMyriaFx_State0Loop` | the owner's +0xA (FxLoop) | 3011 rounds (also BossMyriaFx_State1Loop 3010, BossMyriaFx_State2Loop 3063) |
| M15 | `BossMyriaFx_State1Loop` | pose 9 = 2 | 364 rounds |
| M16 | `BossMyriaFx_State1Loop` | the tick once (FxLoop) | 6000 rounds (also BossMyriaFx_State0Loop 6000, BossMyriaFx_State2Loop 6000) |
| M17 | `BossMyriaFx_State2Loop` | pose 0 = 1 | 321 rounds |
| M18 | `BossMyriaFx_State2Loop` | Follow not called (FxLoop) | 6000 rounds (also BossMyriaFx_State0Loop 6000, BossMyriaFx_State1Loop 6000) |
| M19 | `BossMyriaFx_State0Loop` | the index's high byte (Pose) | 2259 rounds (also BossMyriaFx_State1Loop 2619, BossMyriaFx_State2Loop 2621, BossMyriaFx_State3Enter 1630, BossMyriaFx_State4Enter 737, BossMyriaFx_State5Enter 2075, BossMyriaFx_State6Enter 2041, BossMyriaFx_State6Loop 1070) |
| M20 | `BossMyriaFx_State3` | the next entry | 6000 rounds |
| M21 | `BossMyriaFx_State3Enter` | pose 9 = 1 | 677 rounds |
| M22 | `BossMyriaFx_State3Enter` | the tick each frame | 6000 rounds |
| M23 | `BossMyriaFx_State3Enter` | bank 0x30C | 6000 rounds |
| M24 | `BossMyriaFx_State3Wait` | the owner read before the tick | 150 rounds |
| M25 | `BossMyriaFx_State3Wait` | the owner's +1 at 3 | 4124 rounds |
| M26 | `BossMyriaFx_State4` | the next entry | 6000 rounds |
| M27 | `BossMyriaFx_State4Enter` | pose 2 = 4 | 737 rounds |
| M28 | `BossMyriaFx_State4Enter` | the pose index read before the bank call (FxEnterPosed) | 14 rounds (also BossMyriaFx_State3Enter 34, BossMyriaFx_State5Enter 36, BossMyriaFx_State6Enter 36) |
| M29 | `BossMyriaFx_State4Wait` | the owner's +2 = 1 | 4033 rounds |
| M30 | `BossMyriaFx_State4Wait` | the owner's +1 up by two | 3985 rounds |
| M31 | `BossMyriaFx_State4Wait` | the task not freed | 4033 rounds |
| M32 | `BossMyriaFx_State5` | the next entry | 6000 rounds |
| M33 | `BossMyriaFx_State5Enter` | pose 9 = 2 | 668 rounds |
| M34 | `BossMyriaFx_State5Enter` | +2 on the Sprite_Current of before Follow (FxEnterPosed) | 179 rounds (also BossMyriaFx_State3Enter 182, BossMyriaFx_State4Enter 182, BossMyriaFx_State6Enter 187) |
| M35 | `BossMyriaFx_State5Wait` | the tick before the test | 3905 rounds |
| M36 | `BossMyriaFx_State5Wait` | the owner's +1 at 1 | 4231 rounds |
| M37 | `BossMyriaFx_State6` | the next entry | 6000 rounds |
| M38 | `BossMyriaFx_State6Enter` | pose 9 = 2 | 721 rounds |
| M39 | `BossMyriaFx_State6Enter` | Follow not called (FxEnterPosed) | 6000 rounds (also BossMyriaFx_State3Enter 6000, BossMyriaFx_State4Enter 6000, BossMyriaFx_State5Enter 6000) |
| M40 | `BossMyriaFx_State6Loop` | pose 8 = 2 | 362 rounds |
| M41 | `BossMyriaFx_State6Loop` | +2 on the Sprite_Current of before the call | 97 rounds |
| M42 | `BossMyriaFx_State6Loop` | +2 up every frame | 2984 rounds |
| M43 | `BossMyriaFx_State6Wait` | the owner's +1 up | 4010 rounds |
| M44 | `BossMyriaFx_State6Wait` | BattleEnemy_ScriptTick | 6000 rounds |
| M45 | `BossMyriaFx_Follow` | +0x5E not copied | 5274 rounds |
| M46 | `BossMyriaFx_Follow` | +0x32 from +0x30 | 6000 rounds |
| M47 | `BossMyriaFx_Follow` | +0x44 from +0x40 | 5649 rounds |
| M48 | `BossMyriaFx_Follow` | +0 bit 0 set | 2955 rounds |
| M49 | `BossMyriaFx_Follow` | the y from the x (Drift) | 4929 rounds |
| M50 | `BossMyriaFx_Follow` | the word of the Sprite_Current of after the call (Drift) | 380 rounds |
| M51 | `BossMyriaFx_Follow` | sign 0x8000 unscaled (Drift) | 3079 rounds |
| M52 | `BossMyriaFx_Follow` | the product as the sign (Drift) | 2903 rounds |
| M53 | `BossMyriaFx_Follow` | the drift zero-extended (Drift) | 2123 rounds |
| M54 | `BossMyriaFx_Follow` | the second axis on the Sprite_Current of before the first call | 380 rounds |
| M55 | `BossMyriaFx_Follow` | the neighbour state's pair (Drift) | 5153 rounds |
| M56 | `BossMyriaFx_Follow` | +0x48 not copied | 5228 rounds |

## 5. What nothing reached

The fuzz reached every branch the controls planted in, except the aborts
(section 6), which are the originals' out-of-table reads. What only a fight
would show: which pose each of Myria's states gives, what `0x904B7E` holds
while her effect runs (kind 62 writes it: `0x440630`'s byte argument, 8 or
0), the drift's look, and the end hooks' hand-back to the chapter
(`0x8034E5` = 0x1A, 0xF; variable 3 = 0x32).

## 6. Latent defects (Capcom's, kept)

- **Myria's effect indexes a ten-byte stack table by a 16-bit word.** Every
  state's pose is `table[0x904B7E]` with the table ten bytes of a twelve-byte
  frame: 10 and 11 read the frame's two unset bytes, 12 and up the saved
  registers and the caller's stack. Kind 62 (BSF's) writes the word from a
  byte argument (`0x440630`) or as 8 or 0; whether it stays below 10 in the
  fight is kind 62's callers', not read here. Ours aborts at 10.
- **`BossMyriaFx_Follow` indexes `BossMyriaFx_Drift` by `+1` unchecked**; only
  states 0..6 reach it through the dispatcher. Ours aborts past 7.
- **`BossDLord_HookFx` uses `BattleTask_Create`'s answer untested**: with all
  48 slots taken the answer is 0xFF and the enemy's 0x80 bytes and six bytes
  more are written at `0x93A000 + 0xFF * 0x84`, far past the slots. It also
  takes the acting actor minus 3 as an enemy index untested (a member acting
  would copy from inside the task slots). Ours aborts on the slot, keeps
  the copy's address arithmetic.
- **`BossDLordFx_Start` indexes the enemy data records by enemy 0's `+0xF0`
  unchecked** (as `BattleActor_FxSize` does; `Battle_CopyEnemyData` sets it
  to the record's slot, so it is 0..7 in play). Kept unchecked, as there.
- **The dispatchers, hook tables and stack tables index unchecked**; ours
  aborts.
- **`Boss52_Exit` stores `BossActor_Find(0)` untested** into
  `Field_ActiveMember` and `Sprite_Current` and calls `Sprite_SetAnimation`
  on it; with no actor tagged 0 that is null. Ours does the same (it
  dereferences nothing itself).

## 7. Calls across groups

**Out of BSJ, to code nobody owns** (raw, `boss_sj_callees.h`): `0x446DE0`,
`0x446E00`, `0x446E20` (the end phase's steps; the harness's standard set)
and `0x441090` (the 16.16 round-up, engine). Everything else is ours by
name (`BossActor_*`, `BareRet`, `BareRetZero`, `BossHook_ExitActor0Bit40`,
the engine's). Nothing of another wave-two group's is called or stored.

**Into BSJ from outside the group** (an E8 / E9 / imm32 scan of the image,
2026-09-28, and a `grep` of `src/`: none of our code names a BSJ address):
`Boss_SetupTable` entries 52, 54, 55 and `BossKind_Table` entries 59, 61 (the
roots); `BattleBossFx_Dispatch`'s stack table (`0x435802` stores
`0x43FAC0`, `0x43580A` stores `0x440830`: slots 4 and 5). Kind 62's
`0x440660` (BSF's) creates the slot-5 task but calls nothing of ours.

## 8. For `analysis/calltrace/entries_logic.txt`

The 44 extents appended to the main checkout's file (2026-09-28): 40 new
lines; `00440EF0 197` was already exact; three host lines (`004408C0 D8`,
`004409A0 D8`, `00440A80 470`, which covered F5's states 0..2 and more) are
fixed by the functions' own smaller extents at the same starts (0x62, 0x64,
0x64).

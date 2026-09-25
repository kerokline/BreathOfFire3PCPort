# Banner ticks, two effect handlers and six effect-object states (group CK)

**Status:** IN PROGRESS (2026-09-25) - ten functions ours in
[`src/game/battle_odds.cpp`](../src/game/battle_odds.cpp), every one
pointer-reached. Each was read to its last instruction, and every site
holding its address was read for what the caller does with the call. Each is
fuzzed against Capcom's at start-up (`BOF3X_SHADOW=battle_odds`, 48,000
rounds, 0 mismatches). There are 49 negative controls: 48 refused by a comparison, 1 a change that changes nothing (section 3.1). With `BOF3X_SHADOW='*'` the self-test exits 0 with 1,034 injects. No divergence. The live check
(the combat route) is the coordinator's, after the merge.

This is group CK of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md), "CK"). The queue
calls it "banner, effect result, sparkle and the odd ones": two handlers of
the banner pool ([`battle_misc.md`](battle_misc.md) 1.1), two of the 130
effect handlers under `Effect_ApplyResult`
([`battle_damage.md`](battle_damage.md)), and six small states of the
battle's effect objects, reached through call tables their hosts build on the
stack.

Every claim about the binary is from capstone over `bof3/BOF3.exe` on
2026-09-25: `tools/pe_disasm.py` for the bodies, and two scratchpad scripts
(`ck/around.py find`, `ck/sites.py`) that find every dword in the image equal
to each entry and disassemble from the byte after it to the table's call and
the three instructions after that call.

## 1. The functions

| PC | name | code | reached from | what |
|---|---|---|---|---|
| `0x44A740` | `BattleBanner_TickKind1` | `0x6D` | `BattleBanner_Dispatch`'s stack table, entry 1 (`0x44A5D4`) | the name banner's timer (1.1) |
| `0x44A7B0` | `BattleBanner_TickKind2` | `0x58` | entry 2 (`0x44A5DC`); PSX `0x801DE71C` | the message banner's timer (1.1) |
| `0x44C140` | `Effect_Heal20` | `0xC` | `Effect_Handlers` slot 8 (`.data 0x64E75C`) | HP delta -20 (1.2) |
| `0x44C990` | `Effect_HalfAttack` | `0x28` | `Effect_Handlers` slot 31 (`.data 0x64E7B8`) | HP delta = weapon attack / 2 (1.2) |
| `0x4AEE90` | `BattleFx_FreeTask` | `5` | 31 stack tables, `.data 0x64D474`; PSX `0x800C2120` | `jmp BattleTask_FreeCurrent` (1.3) |
| `0x4B1E70` | `BattleFx_TintActor` | `0x54` | 7 stack tables | countdown, then a fresh tint on the acting actor (1.3) |
| `0x4B1ED0` | `BattleFx_Brighten` | `0x65` | 11 stack tables | the tint's r, g, b + 1, countdown (1.3) |
| `0x4ED5C0` | `BattleFx_SetSize` | `0x17` | 7 stack tables | +9 = `BattleActor_FxSize()`, +2 stepped (1.3) |
| `0x4EE8A0` | `BattleFx_WaitStep4` | `0x18` | 5 stack tables | wait for +0xB <= 4 (1.3) |
| `0x4F7350` | `BattleFx_Finish` | `0x28` | 45 stack tables | at +0xB 0: flag the target, round flag 4, free the task (1.3) |

"code" is to the byte after the last instruction; the queue's extents
(`pe_hidden.py`, to the next start) are each that plus `nop` padding. The
names of the six effect states are hypotheses (they say what the code does,
not which spells use it); the four others are `evidence`.

**Entered by a call, not a jump.** The queue lists eight of the ten as
"jumped to". None is: every one of the 108 `.text` sites that holds one of
these addresses is a `mov [esp + k], imm32` followed by `call [esp + reg*4
(+ k)]` in the same host, and the two `.data` slots are called by
`Effect_ApplyResult` (`call [0x64E73C + 4 * i]`). No argument is pushed
for any of them. The "jumped to" reading is the first-call trace's: the
return address is in the host, after an indirect call, which the catalogue's
rule does not recognise as a call. None is a switch case inside a function:
each is a whole function with its own `ret` (or tail `jmp`), and each is
stored in several hosts' tables.

**eax is returned whole by all ten.** Most hosts drop it, but 37 of the 108
sites end `call [esp + reg*4]; (a store;) add esp, n; ret`, handing the
handler's eax to their own callers: 10 of `BattleFx_FreeTask`'s (`0x434910`,
`0x43EBC6`, `0x4AA316`, ...), 2 of `BattleFx_TintActor`'s and 5 of
`BattleFx_Brighten`'s (`0x4B1CEE`, `0x4C022E`, `0x4C282E`, `0x4CB8C6`,
`0x4EF65E`), and 20 of `BattleFx_Finish`'s (`0x49C3EE`, `0x4A4C36`,
`0x4A647E`, ...). So ours returns what the original leaves in eax on every path.

### 1.1 The banner handlers

`BattleBanner_Dispatch` (ours, `battle_misc.cpp`) sets `0x93B8C0` to the entry
and `DamageScratch` to its index, then calls the kind's handler. The pool
layout is `battle_misc.md` 1.1.

**Kind 1** (`0x44A740`), the name banner (`BattleBanner_ShowName` sets entry 0
with kind 1):
1. A timer (word +8) of `0xFF` runs forever: straight to step 4.
2. Otherwise `0x904AE9` (the kinds seen this frame) `|=` the kind, the timer
   is decremented in memory, and the pointer is read again.
3. Timer 0: the active byte 0; the kind taken back out of `0x904AE9`
   (through the pointer read again); `BattleBanner_NoneOfKind(1)`; when that
   answers al set, window record 4's `+3` (`0x8031F3`) = 0. eax is
   NoneOfKind's.
4. Timer running: window record 4's `+0xA` (`0x8031FA`) = `DamageScratch`.
   eax is the entry.

**Kind 2** (`0x44A7B0`), the same shape with three differences: the kind is
or'ed into `0x904AE9` first, even for a timer of `0xFF`; it is never taken
back out; the test is `NoneOfKind(4)`, after which window record 0's `+3`
(`0x803163`) = 2. While the timer runs record 0's `+0xA` (`0x80316A`) gets
`DamageScratch`. The decrement goes through `cx` (only the word stored).

Window records are `WindowRecords` (`0x803160`, `0x24` each).

### 1.2 The two effect handlers

Both fill the result record `*0x904B60` that `Effect_ApplyResult` then clamps
and applies (+4 the HP delta, positive is damage).
- **`Effect_Heal20`**: word +4 = `0xFFEC`, -20: a fixed 20-point heal. eax the
  record pointer.
- **`Effect_HalfAttack`**: `Battle_CalcDamage(actor, target, 0xFFFF)` -
  actor the low byte of `0x904B34`, target the byte `0x904B54`, `0xFFFF`
  the weapon's element - then `sar ax, 1` and the word stored at
  `*0x904B60 + 4`, the pointer read after the call. The two bytes are
  loaded with `mov al` / `mov cl`, so the pushed dwords carry the entry's
  upper bits; `Battle_CalcDamage` masks both to a byte (ours and, by its
  fuzz, the original), so ours passes the bytes. eax is the callee's with
  its low word halved.

Which items or skills map to slots 8 and 31 (through `0x64E540` and
`0x64E72C`) was not read.

### 1.3 The effect-object states

Each runs on `Sprite_Current`, an effect object whose byte +1 is the state
its host's table is indexed by. The other fields these touch: +2 a second
step counter, +9 a countdown, +0xA a tint record's index, +0xB a byte the
waits test.

- **`BattleFx_FreeTask`** (`0x4AEE90`) is five bytes, `jmp 0x4351F0`
  (`BattleTask_FreeCurrent`, ours in `battle_flow.cpp`): the state that ends
  an effect. The most shared state here: 31 host tables, and the `.data`
  table ending at `0x64D474` (a run of `0x43Cxxx` handlers, whose owner was
  not read).
- **`BattleFx_TintActor`** (`0x4B1E70`): the countdown +9 stepped; at 0,
  `Sprite_ReleaseTint(obj)` then `Sprite_SetTint(obj, 0, 0, 0, 1)` on the
  acting actor's object `obj = *0x904B4C` (read at entry; the action phase
  stores it, `0x42F400`), the tint's index (al) to +0xA, +9 = 8, and the next
  state. `Sprite_Current` is read again for each store.
- **`BattleFx_Brighten`** (`0x4B1ED0`): bytes +2, +3, +4 (r, g, b) of
  `MoveScript_TintRecords[+0xA]` (12 bytes each) each + 1, the index read
  again for each and not bounded (255 * 12 + 4 still lands inside the 0xC00
  bytes); the countdown stepped and at 0 the next state. Together with the
  state before it, a fade-in from black on the actor, by inference.
- **`BattleFx_SetSize`** (`0x4ED5C0`): +9 = `BattleActor_FxSize()` (ours,
  `battle_items.cpp`), `Sprite_Current` read after the call; +2 stepped.
- **`BattleFx_WaitStep4`** (`0x4EE8A0`): once +0xB is 4 or below (unsigned),
  +9 = 8 and the next state.
- **`BattleFx_Finish`** (`0x4F7350`): once +0xB is 0,
  `Battle_SetTargetFlag40(byte 0x904B44)` (pushed in `ecx` with the entry's
  upper bits; the callee masks), byte `0x904AA8 |= 4`, and a tail jump to
  `BattleTask_FreeCurrent`, whose eax is returned.

## 2. Tables

`Effect_Handlers` (`0x64E73C`, 130 dwords) is named in `symbols.toml` by this
group: `battle_damage` reads it through a constant (`kHandlers`), and this
group's two handlers are its only rows in the round's queue. The six effect
states sit in call tables made of immediates on the stack, which are code,
not data, and have no `[[data]]` entry.

## 3. The fuzz

`battle_odds_fuzz.cpp`, the shape of `battle_misc_fuzz.cpp`. Each original is
cloned (`CloneOriginal`) before `BattleOdds_Inject` patches it, with every
`E8` and `E9` leaving it re-aimed at a recording stand-in: `0x44A830` (twice),
`0x445CF0`, `0x4351F0` (the whole of `0x4AEE90`, and `0x4F7350`'s tail jump),
`0x454DC0`, `0x454CC0`, `0x4FC1F0` (at `0x4ED5C0`'s first byte) and
`0x4530D0`. Ours runs on the same stand-ins through `battle_odds::g`. Per
round the state is random with the edges seeded, Capcom's copy and ours run
from it with the same random eax and ecx on entry, and the calls out (with
their arguments), the whole eax and every region either could write are
compared.

What is seeded:
- the banner timer at 0, 1, 2, `0xFE`, `0xFF`, `0x100`, `0x1FF`, `0x8000`,
  `0xFFFF`, the kinds byte and the kind byte at their small values;
- the calc answer's low word at the shift's edges (`0x7FFF`, `0x8000`,
  `0xFFFF`, odd values);
- the object bytes +9 and +0xB at 0, 1, 4, 5, `0x7F`, `0x80`, `0xFF`;
- for `BattleFx_Brighten`, a third of the rounds put `Sprite_Current` inside
  the tint records so that a byte the loop steps is the object's own +0xA or
  +9, which shows the index being read again.

The stand-ins change what the caller reads after them: the banner pointer,
the kinds byte and the window bytes the handlers store after `NoneOfKind`;
the result pointer (after `Battle_CalcDamage`); `Sprite_Current` and
`0x904B4C` (around the tint calls and `BattleActor_FxSize`); the round flags
(after `Battle_SetTargetFlag40`). `BattleTask_FreeCurrent`'s stand-in
records `Sprite_Current` and the round flags, so what `BattleFx_Finish` does
before its tail jump is compared.

| function | rounds | calls out | covered |
|---|--:|--:|---|
| `BattleBanner_TickKind1` | 10,000 | 2,036 | 2,036 reached NoneOfKind |
| `BattleBanner_TickKind2` | 10,000 | 2,067 | 2,067 reached NoneOfKind |
| `Effect_Heal20` | 1,000 | 0 | |
| `Effect_HalfAttack` | 5,000 | 5,000 | |
| `BattleFx_FreeTask` | 1,000 | 1,000 | |
| `BattleFx_TintActor` | 5,000 | 1,328 | 664 reached the tint |
| `BattleFx_Brighten` | 5,000 | 0 | 1,652 with the object inside the records |
| `BattleFx_SetSize` | 3,000 | 3,000 | |
| `BattleFx_WaitStep4` | 3,000 | 0 | 918 at +0xB <= 4 |
| `BattleFx_Finish` | 5,000 | 5,472 | 2,736 reached the finish |

### 3.1 Negative controls

Each control was planted alone in ours by the scratchpad's `ck/controls.py`:
a text substitution in `battle_odds.cpp`, build, headless self-test
(`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=battle_odds`), restore. The count is
mismatching rounds of the function; every refused control ended the process
with exit 3 (the self-test's Fatal), by comparison - none by a fault or a
hang.

**49 controls, 48 refused, 1 that changes nothing.** Per function: kind 1
9, kind 2 7, heal 3, half 6, free 1, tint 7, brighten 5, size 3, wait 3,
finish 5.

| # | control | refused |
|---|---|---|
| K1 | kind1: the 0xFF timer counted down too | `BattleBanner_TickKind1` 706 |
| K2 | kind1: kind not taken out of the kinds seen | `BattleBanner_TickKind1` 1,759 |
| K3 | kind1: NoneOfKind(2) | `BattleBanner_TickKind1` 2,036 |
| K4 | kind1: record 4 +3 cleared whatever al | `BattleBanner_TickKind1` 771 |
| K5 | kind1: record 4 +3 cleared before the call | `BattleBanner_TickKind1` 902 |
| K6 | kind1: the index to record 0 | `BattleBanner_TickKind1` 7,964 |
| K7 | kind1: the entry left active | `BattleBanner_TickKind1` 2,028 |
| K8 | kind1: returns 0 while the timer runs | `BattleBanner_TickKind1` 7,964 |
| K9 | kind1: returns al only | `BattleBanner_TickKind1` 2,036 |
| L1 | kind2: kind or-ed only for a running timer | `BattleBanner_TickKind2` 381 |
| L2 | kind2: kind taken out at 0 | `BattleBanner_TickKind2` 1,785 |
| L3 | kind2: NoneOfKind(2) | `BattleBanner_TickKind2` 2,067 |
| L4 | kind2: record 0 +3 = 0 | `BattleBanner_TickKind2` 1,251 |
| L5 | kind2: 0xFF counted down too | `BattleBanner_TickKind2` 643 |
| L6 | kind2: the index to record 4 | `BattleBanner_TickKind2` 7,933 |
| L7 | kind2: the entry left active | `BattleBanner_TickKind2` 2,055 |
| H1 | heal: -19 | `Effect_Heal20` 1,000 |
| H2 | heal: the AP delta | `Effect_Heal20` 1,000 |
| H3 | heal: returns 0 | `Effect_Heal20` 1,000 |
| A1 | half: logical shift | `Effect_HalfAttack` 2,336 |
| A2 | half: actor and target swapped | `Effect_HalfAttack` 4,879 |
| A3 | half: element 0 | `Effect_HalfAttack` 5,000 |
| A4 | half: result pointer read before the call | `Effect_HalfAttack` 1,234 |
| A5 | half: eax the half only | `Effect_HalfAttack` 5,000 |
| A6 | half: the whole dword halved | `Effect_HalfAttack` 5,000 |
| F1 | free: eax 0 | `BattleFx_FreeTask` 1,000 |
| T1 | tint: the actor object read after the release | `BattleFx_TintActor` 159 |
| T2 | tint: alpha 0 | `BattleFx_TintActor` 664 |
| T3 | tint: countdown 7 | `BattleFx_TintActor` 664 |
| T4 | tint: Sprite_Current not read again after the calls | `BattleFx_TintActor` 273 |
| T5 | tint: no release | `BattleFx_TintActor` 664 |
| T6 | tint: tested before the decrement | not refused: a change that changes nothing (below) |
| T7 | tint: 0 tested before the decrement | `BattleFx_TintActor` 1,377 |
| B1 | brighten: index read once | `BattleFx_Brighten` 352 |
| B2 | brighten: bytes +1..+3 | `BattleFx_Brighten` 5,000 |
| B3 | brighten: next state always | `BattleFx_Brighten` 4,562 |
| B4 | brighten: stride 0x10 | `BattleFx_Brighten` 4,988 |
| B5 | brighten: no countdown | `BattleFx_Brighten` 5,000 |
| S1 | size: Sprite_Current read before the call | `BattleFx_SetSize` 725 |
| S2 | size: +1 stepped | `BattleFx_SetSize` 3,000 |
| S3 | size: returns the size | `BattleFx_SetSize` 3,000 |
| W1 | wait: 4 waits too | `BattleFx_WaitStep4` 238 |
| W2 | wait: signed compare | `BattleFx_WaitStep4` 969 |
| W3 | wait: countdown 9 | `BattleFx_WaitStep4` 918 |
| N1 | finish: flags or-ed before the call | `BattleFx_Finish` 1,583 |
| N2 | finish: bit 3 | `BattleFx_Finish` 2,059 |
| N3 | finish: the target byte 0x904B45 | `BattleFx_Finish` 2,525 |
| N4 | finish: no free | `BattleFx_Finish` 2,736 |
| N5 | finish: test inverted | `BattleFx_Finish` 5,000 |

**T6 changes nothing.** It tests the countdown for 1 before stepping it
instead of for 0 after: the same condition on a byte, with no call between
the step and the test. T7, the wrong test (0 before the step), was added in
its place and is refused.

## 4. What the fuzz does not reach

- **The real callees.** All seven are other modules' and already ours; each
  is checked against its own copy there.
- **The hosts.** The 108 stack tables and `Effect_ApplyResult`'s call are
  not exercised: ours is entered only through the detour, which a live call
  reaches exactly as the fuzz's direct call does. `BattleBanner_Dispatch`'s
  own fuzz still calls stand-ins for both kind handlers.
- **Which effects run which state.** The six states are in hosts owned by
  groups `CJ` (`0x4B8D70`, `0x4B5830`) and by functions no round has queued
  yet; the names are what each state does, not what it is for.

## 5. Found on the way (other groups' addresses - said, not acted on)

- **The "jumped to" column is wrong for all eight of this group's rows**
  (section 1). If the catalogue rule is the same for other groups' rows under
  stack-table hosts, those are calls too.
- **`entries_logic.txt` extents that cover these entries**: `0044A6E0 128`
  (`BattleBanner_Set`, battle_misc, is `0x5B`: the 0x128 runs over both kind
  handlers and `BattleBanner_ClearAll`), `0044B9F0 3317`
  (`Effect_ApplyResult` is `0x4D6`, [`battle_damage.md`](battle_damage.md)
  says so too), and `004B1520 B40` (host `0x4B1520`, which runs over
  `0x4B1CB0`, `0x4B1E70` and `0x4B1ED0`). This group's ten are added with
  their code extents: `0044A740 6D`, `0044A7B0 58`, `0044C140 C`,
  `0044C990 28`, `004AEE90 5`, `004B1E70 54`, `004B1ED0 65`, `004ED5C0 17`,
  `004EE8A0 18`, `004F7350 28`.
- **`0x4B1CB0`** (inside `0x4B1520`'s extent) is itself a state dispatcher of
  six (`0x4B1D00`, `0x4B1E70`, `0x4B1ED0`, `0x4C03B0`, `0x4B1F40`,
  `0x4B2040`) that returns the handler's eax.
- **`battle_misc_callees.h`** still calls `Sprite_SetTint` "group BG" through
  a raw address; it has been ours (`battle_sprites.cpp`) since round seven.
  Harmless.

No defect of the original was found in these ten.

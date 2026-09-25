# The window-kind handlers: the enemy HP gauge, the battle banner, the battle message

**Status:** IN PROGRESS (2026-09-25) - twelve functions ours
(`src/game/window_kinds.cpp`, shadow name `window_kinds`), each fuzzed
headless against a copy of Capcom's with every call re-aimed at a recorder:
24,000 rounds, 0 mismatches; 44 negative controls planted, 44 refused by a count of mismatches. `BOF3X_SHADOW='*'` passes. Not yet
through a live batch: the coordinator's check after the merge is the combat
route.

Group CM of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). The queue listed
eleven; the twelfth, `0x597D10`, is the third slot of one of the eleven's own
stack tables, was in no group (the combat route never reached it), and was
taken with its table. Everything here is a *faithful* replacement: no
`DIVERGENCE.md` entry is owed.

## 1. The functions

Extents measured by disassembly to the last instruction (`pe_disasm.py`,
2026-09-25), not the catalogue's; the catalogue's were `pe_hidden.py`'s
distance to the next start and include the `nop` padding.

| PC | Name | Bytes | catalogue | Reached from | What it does |
|---|---|--:|--:|---|---|
| `0x597A80` | `Window_HpGaugeTrack` | 0x14C | 336 | `Window_DispatchKind` slot 0 (imm32 at `0x597A63`) | the HP gauge settled: notices a new max, starts a drain or a fill |
| `0x597BD0` | `Window_HpGaugeDrain` | 0x40 | 64 | slot 1 (`0x597A6B`) | the drained part shrinks by the step |
| `0x597C10` | `Window_HpGaugeFill` | 0x5F | 96 | slot 2 (`0x597A73`) | the gauge grows by the step |
| `0x597C70` | `BattleWin_BannerRun` | 0x7D | 128 | `0x596FA0` slot 6 (`0x596FE2`) | the banner window: its state, the box, a banner pool text |
| `0x597CF0` | `BattleWin_BannerSlideIn` | 0x1B | 32 | `BattleWin_BannerRun` slot 0 (`0x597C7F`) | y += 8 until 0x12 |
| `0x597D10` | `BattleWin_BannerSlideOut` | 0x33 | - | slot 2 (`0x597C92`) | y -= 8 until -0x16, then the mask bit and the record freed |
| `0x597D50` | `BattleWin_MessageRun` | 0x36 | 64 | `0x596FA0` slot 7 (`0x596FEA`) | the message window's four states |
| `0x597D90` | `BattleWin_MessageWait` | 0x24 | 48 | slot 0 (`0x597D5F`) | waits for the ring to hold an entry |
| `0x597DC0` | `BattleWin_MessageSlideIn` | 0x27 | 48 | slot 1 (`0x597D67`) | y += 8 until 0x12, drawing the message |
| `0x597DF0` | `BattleWin_MessageShow` | 0x6F | 112 | slot 2 (`0x597D72`) | the message until a button or its timer |
| `0x597E60` | `BattleWin_MessageSlideOut` | 0x70 | 112 | slot 3 (`0x597D7A`) | y -= 8 until -0x16, drawing the entry just shown |
| `0x597F60` | `Window_Handler4Kinds` | 0x3E | 64 | `Field_RunTaskRecords` handler 4 (`0x59E264`) | one of six by record byte +2 |

**None of the twelve is "jumped to".** The queue's "Reached as" column says
so for all eleven, but every one is entered by an indirect *call* -
`call dword ptr [esp + eax * 4]` (`0x597A77` with `+ 0x20`, `0x596FEE`,
`0x597C96`, `0x597D7E`, `0x597F96`, `0x59E2A2` in the window task's walk) -
through a table its caller built on its own stack. The first return address
follows an indirect call, which the catalogue's "follows a `call rel32`" test
does not see. The prototypes are therefore the dispatchers' own: eight
pointers for the gauge (section 2), none for the rest. Two of the twelve end
in a tail jump *out*: `0x597D36` to `Window_FreeCurrent`, `0x597DE2` to
`BattleWin_DrawMessage`.

**The stack tables have no address.** Each lives only in the dispatcher's
frame, built from `mov [esp + k], imm32`, so there is nothing for a
`[[data]]` entry to name; `symbols.toml` records each table in its
dispatcher's `evidence`, with the `.text` address of every immediate. Four
tables are here or reach here:

| Dispatcher | Index | Slots |
|---|---|---|
| `Window_DispatchKind` `0x597A30` (ours, `battle_windows.cpp`) | `*arg7` | `0x597A80` `0x597BD0` `0x597C10` |
| `BattleWin_BannerRun` `0x597C70` | record +3 | `0x597CF0`, `0x437CC0` (a bare `ret`), `0x597D10` |
| `BattleWin_MessageRun` `0x597D50` | record +3 | `0x597D90` `0x597DC0` `0x597DF0` `0x597E60` |
| `Window_Handler4Kinds` `0x597F60` | record +2 | `0x597FA0`, `0x5984B0`, **0**, **0**, `0x598570`, `0x5986C0` |

None of the four is bounded in the original (known-defects.md D59). Ours abort loudly on an index
past the table (rule 4), as `Field_RunTaskRecords` and `Window_Run` already
do: the slot past the end is the dispatcher's own return address.

## 2. The enemy HP gauge (`Window_DispatchKind`'s three)

`Window_DispatchKind` has one caller, the enemy target window `0x597320`
(group CL's) at `0x59738E`. Its eight pushes, read off that frame:

| arg | points at | what |
|---|---|---|
| 0 | w + 0xB | u8, the gauge's length in pixels, 0..0x37 |
| 1 | enemy + 0x30 | s16, the enemy's max HP |
| 2 | w + 0x1C | s16, the max HP last seen |
| 3 | w + 0x14 | s16, the HP the gauge last showed |
| 4 | enemy + 0x24 | s16, the enemy's HP |
| 5 | w + 0xD | u8, the drained part still drawn |
| 6 | w + 0x18 | s16, the step a frame |
| 7 | w + 8 | u8, the gauge state - the index `Window_DispatchKind` dispatches on |

w is the window record, enemy `EnemyWorkingRecords` `0x93B9E0 + 0x128 *
(w[+0xA] - 3)`. That +0x24 is the HP and +0x30 the max is the caller's own
test at `0x5973C1` (HP below max >> 2 sets bit 0x20 of the enemy's +0x13) and
the stride's evidence in `symbols.toml`.

- **State 0, `Window_HpGaugeTrack`.** When the max differs from the one last
  seen: the HP is clamped to the max (`jg`), the gauge becomes 55 * HP / max
  (`lea` x 3 and `idiv`), at least 1 while the HP is non-zero, when the shown
  HP is the HP - otherwise 55 * shown / max, capped at 0x37 with the shown
  HP then catching up; the max is remembered. Then, if the shown HP is above
  the HP, **state 1**: the gauge drops at once to 55 * HP / max (floor 1),
  the difference becomes the drained part - all of the old gauge when the HP
  is 0 - and a sixteenth of it (`shr cl, 4`) the step. If below, **state 2**:
  the step is a sixteenth, toward zero (`cdq; and edx, 0xF; add; sar 4`), of
  the pixel difference, both divides sharing the one max read. Either way the
  step is at least 1 and the shown HP becomes the HP.
- **State 1, `Window_HpGaugeDrain`.** The drained byte, zero-extended, against
  the step as a signed word: above it, it loses the step's low byte; else it
  and the state go to 0.
- **State 2, `Window_HpGaugeFill`.** While 55 * HP / max minus the step is
  more than the gauge, the gauge gains the step's low byte; else it takes
  that length and the state goes to 0.

All three set the gauge to 1 when it is 0 and the HP is not. The old gauge
length passes through `DamageScratch`'s first byte `0x903850` (the relocated
PSX scratchpad) and is left there - ours writes it too; a control that keeps
it in a register is refused.

What `eax` they leave is dead: `Window_DispatchKind` returns it and its caller
loads `al` over it at `0x597399` before pushing it to `Battle_ActorIsOut`,
which masks the argument to a byte. So ours return nothing.

## 3. The banner and message windows (kinds 6 and 7 of `0x596FA0`)

Both are window records with their y at +6 and their state at +3.

**The banner** (`BattleWin_BannerRun`): state 0 slides the record down 8 a
frame until y is 0x12; state 1 is the bare `ret` - something else (not read)
moves it on; state 2 slides it up 8 a frame until y is -0x16 (`0xFFEA`), then
clears the kind bits of the banner entry `0x93B8C0` points at from the mask
`0x904AE9` and frees the record. Every frame after the state, the message box
(`BattleWin_DrawMessageBox`) at +4 / +6 and the text of banner pool entry
n = record +0xA (`0x93B8E0 + 0xC n`: text +4, drawn in byte +0xA) through
`Text_DrawAt` at (+4 + 4, +6 + 3), to its end.

The colour argument is `eax` after `mov al, [0x93B8EA + eax]` with `eax` =
0xC n, so it carries 0xC n's bits above the low byte - non-zero from n = 22.
Ours passes the same dword (a control dropping the upper bits is refused in
685 rounds). Nothing in the pool has 22 entries, so it is only reachable with
a wild +0xA.

**The message** (`BattleWin_MessageRun`), over the ring of 16 8-byte entries
at `0x93C2C0` (+0 flags, +1 timer, +4 text; read index `0x93C2A0`, write
index `0x93C2A1`, docs/battle_windows.md section 2):

| state | does |
|---|---|
| 0 `BattleWin_MessageWait` | nothing drawn; when the ring holds an entry, state 1 and `0x939F60` = 1 |
| 1 `BattleWin_MessageSlideIn` | `0x939F60` = 1; y += 8 until 0x12, then state 2; the message drawn |
| 2 `BattleWin_MessageShow` | the message drawn; flag bit 0 and a button down (`Input_Pressed`) advance the ring; flag bit 1 counts the timer down (0xFF is forever) and at 0 advances; an advance that empties the ring gives state 3 |
| 3 `BattleWin_MessageSlideOut` | y -= 8 until -0x16, there `0x939F60` = 0 and state 0 (the record is kept); the box and the entry *before* the read index - the one just shown - drawn |

`BattleWin_MessageShow` re-reads the read index between its two tests, so a
button that advances the ring makes the timer test look at the *next* entry in
the same frame; ours does the same.

`0x939F60` is "the message window is out": written only by this family, read
at seven sites in the battle code (`pe_xref.py`, 2026-09-25: `0x42F130`,
`0x42FE20`, `0x42FF70`, `0x4AF1C0`, `0x4B57C0`, `0x4B78D0`, `0x4F52D0`). Not
given a `[[data]]` name here, because those readers belong to groups CA, CB
and CJ, which may name it from their side; the name the reads suggest is
`BattleMsg_Open`.

## 4. `Window_Handler4Kinds`

Handler 4 of `Field_RunTaskRecords`' nine: one of six by record byte +2.
Slots 2 and 3 are stored from `eax` (0), not immediates - **a record of kind 2
or 3 calls address 0** in the original. Ours aborts loudly with a message
there. `0x598570` and `0x5986C0` are group CD's (catalogued under
`BATL_END.EMI`); `0x597FA0` is itself a one-slot stack dispatch
(`0x597FC0`, by record +3) and `0x5984B0` a draw, both calling `0x5982D0`,
both in no group and unread. All four stay Capcom's.

## 5. The fuzz and its negative controls

`BOF3X_SHADOW=window_kinds`, at start-up (`src/game/window_kinds_fuzz.cpp`):
twelve byte-copies, every call re-aimed at a recording stand-in (the two tail
jumps included), and the three stack tables' eleven immediates re-aimed
inside the copies (`BattleWin_BannerRun` +0xF / +0x17 / +0x22,
`BattleWin_MessageRun` +0xF / +0x17 / +0x22 / +0x2A, `Window_Handler4Kinds`
+0x1A / +0x22 / +0x2A / +0x32) - an unpatched copy would run the original
states. Each round: random bytes over the 22 window records, the current
record pointer, the ring and its indices, `0x939F60`, the banner pool, its
pointer and mask, `Input_Pressed` and `DamageScratch`'s byte, and over the
gauge's two records (buffers of ours at the window's and the enemy's
offsets); then the indices put back inside their tables, the pool pointer
inside the pool; then each branch's boundaries seeded - y at 0x12, 0xFFEA and
eight either side, the max HP at 1, 2, 55, 56, 0x7FFF, 0x8000, 0xFFFF and
never 0, the HP at the max, one either side, 0 and below max / 55, the step
at the edge of each test, the timer at 0xFF, 0, 1, 2, a banner index past the
pool. Theirs runs, then ours from the same state; all of the above and the
stand-ins' log are compared.

The stand-ins are not quiet: `BattleMsg_Advance`'s moves the read index and
answers as the real one does, `Window_FreeCurrent`'s clears the record's bytes
0, +2 and +3, and between calls a `Disturb()` writes one of ten watched bytes
or, once in 23, repoints `0x905B84` at another record.

    shadow      window_kinds self-test: 24000 rounds over 12 functions (2000 each), 18833 calls to the stand-ins, 0 MISMATCHES; the 22 window records, the current record, the message ring and its indices, 0x939F60, the banner pool, its pointer and mask, Input_Pressed, DamageScratch's byte, the gauge's two records and the stand-ins' log compared
    shadow      window_kinds coverage: gauge max changed 651, HP clamped 130, capped at 0x37 279; to drain 342, to fill 415, settled 1243, emptied 228; drain on 1437 done 563; fill on 1091 done 909; slid in 260, banner freed 148, colour with upper bits 671; message opened 448, advanced by a button 506, by the timer 151, closed 152

### Negative controls

44 planted bugs, one at a time, rebuilt and re-run headless (`BOF3X_SELFTEST_ONLY=1`, a scratch driver that edits `window_kinds.cpp`, builds, runs and restores). **44 of 44 were refused by a count of mismatches**, each in the function it was planted in and nowhere else.

| the bug | rounds that refused it (of 2,000) |
|---|--:|
| Track: the HP not clamped to a changed max | 130 |
| Track: no floor of 1 on the recomputed gauge (shown = HP) | 30 |
| Track: the recomputed gauge capped at 0x38, not 0x37 | 24 |
| Track: the cap leaves the shown HP behind | 168 |
| Track: the max not remembered | 651 |
| Track: the old gauge kept in a register, DamageScratch not written | 380 |
| Track: the drain step an eighth, not a sixteenth | 396 |
| Track: the fill step shifted (floor), not divided (toward zero) | 11 |
| Track: a fall starts the fill state | 510 |
| Track: no floor of 1 on the step | 379 |
| Track: HP 0 leaves the gauge standing | 113 |
| Track: an unchanged HP treated as a fall | 888 |
| Drain: continues while the drained part EQUALS the step | 194 |
| Drain: the step compared unsigned | 59 |
| Drain: the gauge state not reset when drained | 563 |
| Drain: no floor of 1 on the gauge | 95 |
| Fill: grows while the gap EQUALS the step | 249 |
| Fill: grows by the step plus one | 629 |
| Fill: the gauge state not reset when full | 909 |
| Fill: no floor of 1 on the gauge | 239 |
| BannerRun: the colour argument its low byte alone | 671 |
| BannerRun: the text at x + 5 | 2000 |
| BannerRun: the record not re-read after the box | 62 |
| BannerRun: states 0 and 2 swapped | 1355 |
| BannerSlideIn: arrives at 0x1A | 278 |
| BannerSlideIn: 4 a frame | 1874 |
| BannerSlideOut: the mask and-ed with the kind, not its complement | 140 |
| BannerSlideOut: freed at 0xFFE2 | 282 |
| BannerSlideOut: the record not freed | 148 |
| MessageRun: states 0/1 and 2/3 swapped | 2000 |
| MessageWait: 0x939F60 not set | 448 |
| MessageWait: opens when the ring is EMPTY | 2000 |
| MessageSlideIn: 0x939F60 not set | 1861 |
| MessageSlideIn: the message not drawn | 2000 |
| MessageShow: a button needs no press | 462 |
| MessageShow: the read index not re-read before the timer | 352 |
| MessageShow: a timer of 0xFF counts down too | 129 |
| MessageShow: the button closes to state 2 | 130 |
| MessageShow: the timer on flag bit 2, not bit 1 | 859 |
| MessageSlideOut: the entry AT the read index drawn | 2000 |
| MessageSlideOut: back to state 4, not 0 | 152 |
| MessageSlideOut: 0x939F60 not cleared | 136 |
| MessageSlideOut: the record not re-read after the box | 53 |
| Handler4Kinds: kind 4 runs kind 5 | 484 |

The first run (2026-09-25) did not refuse *"no floor of 1 on the recomputed gauge"*: its path needs a changed max, the shown HP equal to the HP, and 55 * HP / max = 0, which the random HP almost never gave. The seeds were widened (an HP below max / 55, a shown HP at the 0x37 cap) and every control re-run; the table is that second run. The fill step's "shifted, not divided" control is the thinnest (it needs a negative max, where the pixel difference goes negative) and is refused all the same.

## 6. What the fuzz did not reach, and what no check has seen

- **Any of this live.** The queue says the combat route reaches all eleven;
  the live check is the coordinator's, after the merge. What to look at: the
  enemy target window's gauge on a hit (drain) and on a heal (fill), the
  message window's slide, button and timer, and a banner.
- **`BattleWin_BannerSlideOut`** was not reached by the combat route (it is
  not in the queue). Fuzz only.
- **An index past a table, and kinds 2 and 3 of handler 4.** Ours aborts
  loudly; the fuzz never seeds them (the original would crash the self-test).
- **A max HP of 0.** Both divide by zero (`idiv` raises #DE); never seeded.
- **What moves the banner out of state 1**, and what sets the ring's flags and
  timers (`0x447840`, `0x447F40`, `0x450510` write the timer byte) - other
  groups' code, unread here.

## 7. Defects of the original (numbered in known-defects.md)

Numbered after the merge (2026-09-25): D66, D71, D72, D68. None is changed
here.

1. `Window_Handler4Kinds` `0x597F60`: kinds 2 and 3 of its stack table are 0,
   so a record-handler-4 window of kind 2 or 3 calls address 0. Latent: no
   writer of such a kind is known (D66).
2. `BattleWin_BannerSlideOut` `0x597D10` clears the mask bits of the banner
   entry `0x93B8C0` points at - whichever entry `BattleBanner_Dispatch`
   visited last - not of the entry this window shows (record +0xA). With more
   than one banner live the wrong kind's bit may be cleared. Read, not seen;
   whether it matters depends on who reads `0x904AE9` after the window task
   runs in a frame (unread) (D71).
3. The four slides compare y with 0x12 or 0xFFEA for *equality*, stepping 8:
   a window placed at a y not congruent to 0x12 modulo 8 never arrives and
   wraps round the 16-bit word forever. Latent: the creators are not read
   (D72).
4. The gauge divides by the max HP without a test: a max of 0 is a
   divide-by-zero fault (D68).

## 8. Learned about other groups' addresses (said, not acted on)

- `0x596FA0` (CL): its eight-slot stack table by record +2 is `0x597000`,
  `0x597090`, `0x5971B0`, `0x597200`, `0x597320`, `0x5975D0`, `0x597C70`,
  `0x597D50`; slots 6 and 7 are this group's.
- `0x597320` (CL) is the enemy target window, the sole caller of
  `Window_DispatchKind`; the eight pointers it passes are section 2's.
- `0x598570`, `0x5986C0` (CD): the queue files them as "folded into
  `0x5982D0`", but they are kinds 4 and 5 of `Window_Handler4Kinds`
  (`0x597F8A`, `0x597F92`); `0x5982D0` is a draw they (and `0x597FC0`,
  `0x5984B0`) call. Their prototype is `void (void)` like their siblings'.
- `0x597FA0`, `0x5984B0`, `0x597FC0`: pointer-reached, in no group, unread
  beyond their first block.
- **`analysis/calltrace/entries_logic.txt` still lists `00597A30 4A0` and
  `00597F40 38A`** (group BC's `Window_DispatchKind` and `Text_GlyphCount`,
  really 0x4F and 0x1D; `battle_windows.md` section 1 says so). The first runs
  over all twelve of these. Left for the coordinator; the lines below are
  added.

## 9. For the batch check

Added to `analysis/calltrace/entries_logic.txt` (hex address, hex size):

    # --- 2026-09-25 (round 8), group CM: the window-kind handlers (src/game/window_kinds.cpp)
    00597A80 14C
    00597BD0 40
    00597C10 5F
    00597C70 7D
    00597CF0 1B
    00597D10 33
    00597D50 36
    00597D90 24
    00597DC0 27
    00597DF0 6F
    00597E60 70
    00597F60 3E

## 10. Chinese text

Two draws here put game strings on screen, both through `Text_DrawAt`
(`msgbox.cpp`, which the localisation work already hooks):

- `0x597CE4` in `BattleWin_BannerRun`: the banner pool text,
  `0x93B8E4 + 0xC n` - filled by `0x44A650` / `0x44A6E0` and directly by
  `0x431030` (`0x431999`) and `0x4319B0` (`0x431C9B`, `0x431E5F`).
- `0x597EC7` in `BattleWin_MessageSlideOut`: the battle message ring's entry
  `0x93C2C4 + 8 i`, the same strings `BattleWin_DrawMessage` draws at
  `0x597F11` - written by `0x447F40`, `0x44FDE0`, `0x450510` and `0x450D60`
  (`pe_xref.py`).

Nothing here translates or measures them; x is fixed at the window's +4 + 4,
so a longer string runs off the box's right edge rather than being centred.

# The event script: the area's placement script and its interpreter

**Status:** IN PROGRESS (2026-09-22) - twenty-eight functions ours: the
interpreter (the op dispatch, the if, the switch and their skips), the object
placements the attract cycle reaches, and the bit-flag helpers three other
groups call - each read to its last instruction against its PSX twin and
fuzzed against a copy of Capcom's with every call re-aimed at a recorder,
**46 negative controls, 45 refused by a count**. Every one of the 200 areas'
placement scripts decodes (`tools/event_scan.py`) and is byte-identical to the
PlayStation disc's. **Not yet through the live batch check** - that runs
centrally after the merge (section 8).

[`attract-remaining.md`](attract-remaining.md) section 4.5 catalogued the
`0x579740`..`0x57BA60` run as "entered only a few dozen times, from
`0x594E60`; what it sets up is unread". This document reads it. The *other*
script of the field objects - the movement script that runs a step a frame -
is [`movement-script.md`](movement-script.md); the two share only the object
records and the counter table.

## 1. What it is

When the game enters an area (`0x594E60`, and `0x580535` on a load), it runs
the **placement script**: the byte stream at the area descriptor `+0x00`
(`Area_Descriptors` `0x667590`, [`attract-remaining.md`](attract-remaining.md)
section 5). The script places the area's objects - who stands where, facing
which way, with which animation bank, speed and movement script - under
`if` / `switch` on the game's flags, so that the same area holds different
people at different points of the story.

| PC | PSX (`GAME.EMI` section 0) | what |
|---|---|---|
| `0x579740` `Area_RunPlacement` | `FUN_801A4E58` | count = 0, the four extra objects' byte 0 cleared, run, then every object from the count to 29 cleared |
| `0x5797C0` `EventScript_Run` | `FUN_801A4F44` | the loop: an op below `F0`, a control op above it, until `FF` |
| `0x579F30` `EventScript_Op` | `FUN_801A5C00` | the handler by the high nibble, then the length from `EventScript_OpLengths` |
| `0x579800` `EventScript_Control` | `FUN_801A5408` | `F0` `F1` `F4` `F9` `FA` |
| `0x579890` `EventScript_If` | `FUN_801A4FC8` | `F0 c x` ... [`FD` ...] `FE` |
| `0x579950` `EventScript_IfNot` | `FUN_801A5160` | `F1 c x`, the condition inverted |
| `0x579A10` `EventScript_IfRunStep` | `FUN_801A52F8` | one step of an arm that runs |
| `0x579A50` `EventScript_IfSkipStep` | `FUN_801A537C` | one step of an arm that is skipped |
| `0x579AA0` `EventScript_SkipControl` | `FUN_801A54AC` | a control op stepped over |
| `0x579B00` `EventScript_Switch` | `FUN_801A5520` | `F4 c` { `F6 x` ... `F8` \| `F7` ... `F8` } `F5` |
| `0x579BA0` `EventScript_CaseRun` | `FUN_801A5648` | a case body that runs |
| `0x579C20` `EventScript_CaseSkip` | `FUN_801A56F0` | a case body skipped |
| `0x579CA0` `EventScript_SkipIf` | `FUN_801A57A0` | Capcom's: a nested if skipped |
| `0x579CF0` `EventScript_SkipSwitch` | `FUN_801A584C` | Capcom's: a nested switch skipped |

**The grammar**, read from the five functions above:

```
  op < F0        one of fifteen handlers by the high nibble; its length is
                 EventScript_OpLengths[op >> 4] - the byte read AGAIN after
                 the handler has run
  F0 c x  ...  [FD  ...]  FE       if condition c (operand x), else
  F1 c x  ...  [FD  ...]  FE       the same, negated
  F4 c { F6 x ... F8 | F7 ... F8 } F5    switch on condition c
  F9 b           the flag bank becomes b
  FA             the flag bank goes back to Cond_ByteFA
  FF             the end
```

`F6 x` labels may follow one another: `EventScript_CaseRun` steps over an `F6`
by two bytes and an `F7` by one, so several labels share one body, as C's do.
`EventScript_CaseSkip` stops **at** the next `F6`, which the switch then tests.

**The conditions** are `EventScript_Conditions` `0x663B30`, seventeen entries
(PSX `0x80182AF4`, 25 entries of the same shape with a null at 10); each takes
the *address* of the script position - so it may move it, and none of the
seventeen does - and answers in `al`. The three the scripts reach:

| index | PC | PSX | what |
|--:|---|---|---|
| 0 | `0x57C180` `EventCond_ByteFA` | `0x8015C00C` | the operand equals `Cond_ByteFA` |
| 2 | `0x57C1C0` `EventCond_Flag` | `0x8015C05C` | flag `operand` of row `EventScript_FlagBank` of `Cond_Flags` |
| 8 | `0x57C210` `EventCond_ByteFD` | `0x8015C0C0` | the operand equals `Cond_ByteFD` |

`EventScript_FlagBank` `0x7DEE20` (PSX `0x80148208`) is the row: it starts each
run as `Cond_ByteFA` sign-extended, and ops `F9` / `FA` change it. The **if**
masks the condition index to five bits; the **switch** reads it *signed and
unmasked* - a difference the PSX has too.

The op handlers, by the high nibble, are fifteen functions (`EventScript_Op`'s
jump table, in the PSX's order). Four are ours - the ones the attract cycle
reaches:

| nibble | PC | PSX | bytes | what |
|---|---|---|--:|---|
| 1 | `0x57A1E0` `EventOp_1x` | `FUN_801A60C8` | 16 | place `Sprite_Objects[count]` |
| 2 | `0x57A5E0` `EventOp_2x` | `FUN_801A67F8` | 19 | the same, plus `+0x18` / `+0x1C` and object `+0x83` |
| B | `0x57B130` `EventOp_Bx` | `FUN_801A7CA0` | 14 | place `Sprite_ObjectsExtra[op[0xB]]` - a party member |
| C | `0x57B4E0` `EventOp_Cx` | `FUN_801A82B8` | 2 | the kind-2 object, through `Kind2_Place` |
| 0, F | `0x57A010` `EventOp_0x` | `FUN_801A5D5C` | 17 | Capcom's: `1x` with object `+0x83` |
| 3, 4, 5, 6, 7, 8, 9, A, D, E | `0x57A7C0`, `0x57A990`, `0x57B310`, `0x57AD10`, `0x57AB50`, `0x57A3A0`, `0x57B530`, `0x57AEC0`, `0x57B500`, `0x5898D0` | | 18, 17, 18, 16, 17, 12, 13, 14, 4, 7 | Capcom's: more placements, `Dx` a context byte, `Ex` an effect |

and their four helpers, all ours:

| PC | PSX | what |
|---|---|---|
| `0x579E30` `EventObj_Reset` | `FUN_801A5AB0` | the sprite's fields and its script context to their starting values |
| `0x579DB0` `EventObj_SetFlags` | `FUN_801A5998` | the flags byte: `+7`, bit 0x40 of `+0`, the context's `+0x80`, `+0x48`, and `+0xA` = 16 / `Field_MoveSpeeds`[speed] |
| `0x57B100` `PartyRecord_Clear` | `FUN_801A5B88` | four bytes of `MoveScript_PartyRecords[index]` |
| `0x57BA60` `Sprite_InitFromEntry` | `0x8015B67C` | a sprite from an area descriptor `+8` entry; `+0x6C` from `Sprite_DirectionAngles` |
| `0x579D70` `EventObj_Face` | `FUN_801A591C` | Capcom's: the pose or the facing, at the end of a placement |

**The count** is the word at `DamageScratch` `0x903850` - the PSX's scratchpad
word `0x1F800000`, which the port moved to an ordinary global - and the word
after it (`0x903852`, PSX `0x1F800002`) carries the animation bank from the op
to `Sprite_SetAnimationBank`. Both are general scratch, used all over the exe
(2,114 and 712 references to the two addresses); inside this interpreter they
mean only this.

## 2. Where the scripts live

**In `BOF3.exe`'s `.data`, laid out as the PlayStation's area section was, and
byte for byte the PlayStation's.** Measured 2026-09-22: for each of the 200
areas, `Area_Descriptors[area]` and then its `+0x00` pointer gives the script;
all 200 land in `.data` (`0x5DA190`..`0x649E2C`), none in `.rdata` or the
`DAT`s. Against the Japanese disc (`BIN/WORLD00/AREA000.EMI`.., the section
that loads at `0x801F2C00`, whose descriptor the boot EXE's table `0x801802EC`
names), walking each script to its terminating `FF`: **200 of 200 identical,
90,414 bytes**. This is the same finding as the movement scripts'
([`movement-script.md`](movement-script.md) section 3): the PC build compiled
each area's section into the exe and the `DAT`s dropped it
([`DAT_CONTAINER.md`](DAT_CONTAINER.md) section 2). The scripts are game data:
none of them is quoted here or in the tool.

A second kind of event script exists and is *not* the placement script: the
area descriptor `+0x04` table, indexed by an object's word `+8` when its bit
15 is set, which `MoveScript_SetTurnTarget` `0x517E90` and `0x52F670` run
through `EventScript_Run` - the talk scripts. Same grammar, same interpreter;
not decoded here (the table's length is not established).

## 3. The op-length table

`EventScript_OpLengths` `0x663B0C`, sixteen bytes by high nibble:

```
  0x 11   1x 10   2x 13   3x 12   4x 11   5x 12   6x 10   7x 11
  8x 0C   9x 0D   Ax 0E   Bx 0E   Cx 02   Dx 04   Ex 07   Fx 00
```

Entries 0..E are byte for byte the PSX `GAME.EMI` table at `0x801C943C`. Entry
15 differs - `0x00` on the PC, `0x2E` on the PSX - and is unreachable: `F0` and
above never reach `EventScript_Op`. (The PSX byte is the last before the
SCENARIO table at `0x801C944C`.) Every length agrees with the operands the
four handlers we read use, and `tools/event_scan.py` decodes all 200 scripts
with them.

## 4. The decoder

`python tools/event_scan.py` walks every area's placement script from the
descriptor, structurally - both arms of every if, every case - with the length
table read out of the exe, and reports what it meets. 2026-09-22, all 200
areas, 90,414 bytes:

| | |
|---|---|
| ops | `0x` 1,687, `1x` 2,251, `2x` 39, `3x` 84, `4x` 5, `5x` 168, `6x` 49, `7x` 94, `8x` 39, `9x` 252, `Ax` 25, `Bx` 181, `Cx` 7, `Ex` 406 |
| control | 563 `if`, 635 `if-not`, 520 `else`, 195 `switch`, 860 cases, 3 defaults, 13 `F9`, 10 `FA` |
| conditions used | 0 x154, 2 x740, 3 x1, 4 x8, 5 x1, 6 x2, 8 x343, 9 x22, 12 x88, 13 x5, 14 x16, 16 x13 |
| speed indices placed | 0 x1,918, 1 x52, 2 x2,045, 3 x140, 4 x3 |
| `Bx` extra objects | 0 x106, 1 x45, 2 x25, 3 x5; directions 0..7 only |
| anomalies | **1** |

The one anomaly is area 5, a second `FD` inside one if: the interpreter only
sets the else byte again, so the bytes after it run in the same arm as the
first else - harmless, and the decoder says so.

What the decoder proves, against the defects of section 5: **no shipped
placement script has a condition index outside the seventeen, a speed index
that divides by zero, a `Bx` object past the four, a direction past the eight,
or a byte where the interpreter would loop for ever.** Each of those is a real
hang or fault in the original, waiting on a script that would ask for it.

## 5. Quirks kept, and candidate defects

Kept, each said in a comment where it is implemented:

- **A control op that does not advance never ends.** `EventScript_Control`
  returns the position unchanged for `F2` `F3` `F5`..`F8` `FB`..`FE` (and for
  any byte below `F0`), so `EventScript_Run` reads the same byte for ever. The
  same shape in `EventScript_CaseRun` / `CaseSkip` (`F2` `F3` `F5`, `FB`..`FF`)
  and in `EventScript_Switch` (any byte between cases). The PSX is identical.
- **The if masks the condition index to five bits; the switch does not mask it
  at all and reads it signed.** Indices 17..31 reach `MoveScript_CounterOps`
  `0x663B74`, the `E9` handlers `0x663B84` and then data words, with the
  address of the caller's script position where those expect other arguments;
  a negative switch index reads whatever lies before the table. Both platforms.
- **`EventObj_SetFlags` divides 16 by `Field_MoveSpeeds`[the context's speed
  index]**, a whole byte into a table of six. Indices 6, 7, 9, 10, 11, 13 and
  most past the table are zero there: a divide fault, which the PSX raises as
  its own trap (`trap(0x1c00)` in the Ghidra listing). Index 0 skips the
  division. Ours keeps the fault (an `idiv`, not a C++ `/`).
- **Nothing bounds an object number.** `EventOp_Bx` places
  `Sprite_ObjectsExtra[op[0xB]]` for any byte; `Sprite_InitFromEntry` indexes
  `Sprite_DirectionAngles`' eight words with a whole byte; the placements'
  count is signed and only tested (against 30) on entry.
- **The op's first byte is read again after the handler** for the length, and
  the placements re-read `Sprite_Current`, `Field_ActiveMember` and the count
  after every call they make. A callee that moved one moves what follows.
- **`Flags_Test` returns all of `eax` as 0 or 1** (`setne al` over a value
  below 0x20). 844 call sites: 833 read `al`, seven `and eax, 0xFF`, four
  return it as their own. Ours returns `unsigned char`, which the compiler
  zero-extends - the fuzz compares the whole of `eax` to be sure.

Candidate defects, written down and left alone: the four above (each needs a
script that asks for it, and none does); and the port's own `EventOp_Bx`,
which stores the entry byte at sprite `+0x70` as a byte where the PSX stores a
word (`FUN_801A7CA0`) - the field is read as a byte everywhere we have looked,
so nothing observed depends on it. One more, the table's own: `EventScript_Conditions`
`0x663B30` holds a null at index 10 on the PC as on the PSX (`0x663B30 + 0x28`
is 0), so an `if` or `switch` naming condition 10 calls address 0; the decoder
(section 4) finds no shipped script that does.

No `DIVERGENCE.md` entry: nothing here changes behaviour.

## 6. The fuzz

`BOF3X_SHADOW=event_script` (`src/game/event_script_fuzz.cpp`). Every one of
the 28 originals is byte-copied with **every** call re-aimed at a recording
stand-in - the interpreter's calls to itself included, so each function is
tested alone - and ours runs with the same stand-ins through
`event_script::g`. Five jump tables are moved into their copies; the three
calls through `EventScript_Conditions` are re-aimed at a table of 512 numbered
stand-ins, entry 128 at the base, so that an index the switch reads signed
(-128) or a control that drops the sign (up to 383) still lands on one.

A round: random state with each branch's boundary values seeded, theirs, the
same state again, ours; every byte of the state, the stand-ins' log (a count,
a hash of every entry, the first 48 kept) and the result compared. The scripts
are generated so that each loop ends: a body of the bytes that function does
not hang on, then a tail of its terminator. The stand-ins give back what the
real callee leaves for the caller to read - a moved position, the arm's depth
and else bytes, `Sprite_Current +0x54` - and now and then disturb what the
caller reads again: the count, `Sprite_Current`, the op's own byte.

**28 x 20,000 rounds, 1,154,330 stand-in calls, 0 mismatches** (four seconds).
Coverage, from the log: every high nibble of the first byte at least 524
times in each test; 88,907 arm steps run and 89,697 skipped in the if;
15,123 case bodies run and 44,008 skipped, 10,145 switches with a negative
condition index; 15,400 placements with room and 4,600 with the count at 30 or
more; 12,964 placement runs where the script left the count below 30, 1,392 of
them negative (clearing objects *before* the array, as the original does);
18,280 divisions in `EventObj_SetFlags`.

## 7. The negative controls

One planted bug each, built and run through the fuzz
(`analysis`-free; the runner is a scratch script). **45 of 46 refused**, by a
count and never by a hang:

| # | control | refused |
|--:|---|---|
| 0 | `Run`: the op / control split at `0xF1` | 3,141 |
| 1 | `Run`: the flag bank takes `Cond_ByteFA` unsigned | 10,072 |
| 2 | `Op`: the length from the byte as it was before the handler | 3,618 |
| 3 | `Op`: the length by the low nibble | 17,785 |
| 4 | `Control`: `F9` steps one byte | 611 |
| 5 | `Control`: `FA` takes `Cond_ByteFA` unsigned | 303 |
| 6 | `Control`: an unknown byte steps one | 16,954 |
| 7 | `If`: the condition index masked to six bits | 9,944 + 10,021 |
| 8 | `If`: the byte after the operand not stepped over | 20,000 + 20,000 |
| 9 | `If`: the else byte read once, before the arms | 20,000 + 20,000 |
| 10 | `If`: `FE` ends the arms at any depth | 20,000 + 20,000 |
| 11 | `IfNot`: the condition's sense not inverted | 20,000 |
| 12 | `IfRunStep`: control ops up to `FE` | 581 |
| 13 | `IfRunStep`: `FE` steps one byte | 636 |
| 14 | `IfSkipStep`: the op length by the low nibble | 9,010 |
| 15 | `SkipControl`: `F9` steps one byte | 616 |
| 16 | `SkipControl`: the nested if and switch swapped | 1,830 |
| 17 | `Switch`: the condition index read unsigned | 6,790 |
| 18 | `Switch`: later cases still tested after one has run | 8,133 |
| 19 | `Switch`: a true case's body starts where the condition left the position | 10,041 |
| 20 | `CaseRun`: `F6` steps one byte | 4,193 |
| 21 | `CaseSkip`: `F6` comes back one byte past itself | 6,011 |
| 22 | `Reset`: the context's `+0x86` cleared instead of `0xFF` | 20,000 |
| 23 | `Reset`: `+0x40` / `+0x44` take `0x1000` | 20,000 |
| 24 | `SetFlags`: the flags byte read once, not again after its own store | 610 |
| 25 | `SetFlags`: bit 6 makes `+0x48` one | 20,000 |
| 26 | `SetFlags`: the quotient to `+0xB` | 18,280 |
| 27 | `Place`: the count's test is "above 30" | 1,312 + 1,347 |
| 28 | `Place`: the object's bytes at the count as it was on entry | 5,543 + 5,488 |
| 29 | `Place`: the count incremented from the one on entry | 7,992 + 8,010 |
| 30 | `Place`: a coordinate's half from bit 0 of the byte | 8,563 + 8,531 + 12,160 |
| 31 | `Place`: the kind `+6` from the low nibble | 14,763 + 14,762 |
| 32 | `Place`: `2x`'s `+0x18` stored as a byte | 15,404 |
| 33 | `Bx`: the kind is the high nibble, not one less | 20,000 |
| 34 | `Bx`: `+0x70` from `+0x54` as it was before the ground call | 4,963 |
| 35 | `Bx`: the extra object's number masked to the four | 9,801 |
| 36 | `InitFromEntry`: the angle's index masked to the eight | 16,140 |
| 37 | `InitFromEntry`: the entry's `+4` read before the stores | 2,594 |
| 38 | `PartyRecord_Clear`: the index masked to seven bits | 5,087 |
| 39 | `PartyRecord_Clear`: the four bytes in the order `+0 +1 +2 +3` | **not refused** |
| 40 | `Flags_Set`: the bit number masked to four bits | 5,078 |
| 41 | `Flags_Test`: the byte at index >> 4 | 8,599 |
| 42 | `EventCond_Flag`: the bank's whole word as the row | 13,203 |
| 43 | `EventCond_ByteFA`: the comparison signed | 4,955 |
| 44 | `ScriptFlags_Set40`: bit 0 of `Field_ScriptFlags`, not bit 8 | 15,099 |
| 45 | `ObjTrio_SetBit40`: the third object at `+0x290` | 14,977 |

Control 39 is a change that changes nothing: four stores to four distinct
bytes with no read between them, so no input can tell the orders apart (the
trap from [`HANDOFF.md`](HANDOFF.md), "a control that is not refused may be a
change that changes nothing"). It is kept in the list as the record of that.

Four controls were **first** refused by a fault or not at all, and each one
found a blind spot in the fuzz, now closed:

- the unsigned switch index (17) ran off a 256-entry stand-in table; the table
  is 512 long now, and the control is refused by a count;
- `Bx`'s `+0x70` (34) dereferenced a random `+0x54`; every object the fuzz
  seeds now carries a readable one;
- `PartyRecord_Clear`'s mask (38) was invisible while the index was drawn
  0..7; it is drawn over the whole byte now, with the region to match;
- `SetFlags`' re-read (24) was invisible while the flags pointer only ever
  aliased `+7`, where the store writes back the same value; it now also
  aliases `+0`, `+0x48` and the context's `+0x80`, which the function changes
  between the reads.

## 8. What the attract cycle reaches, and what would reach the rest

The cycle enters an area twelve times, so the interpreter runs twelve scripts
(counts from `analysis/attract_catalog.md`): `Area_RunPlacement` 12,
`EventScript_Run` 12, `EventScript_Op` 93, `EventScript_Control` 24,
`If` 3, `IfNot` 3, the arm steps 42 and 18, `Switch` 18, `CaseRun` 18,
`CaseSkip` 27, `SkipControl` 27, `EventObj_Reset` and `SetFlags` 84 each,
`EventOp_1x` 9, `EventOp_2x` 36, `EventOp_Bx` 21, `EventOp_Cx` 9,
`PartyRecord_Clear` 21, `Sprite_InitFromEntry` 21, `Flags_Set` 9,
`Flags_Clear` 12, `Flags_Test` 20, `EventCond_ByteFA` 24, `EventCond_Flag` 6,
`EventCond_ByteFD` 9, `ScriptFlags_Set40` 6, `ObjTrio_SetBit40` 15.

**Not reached, and only the fuzz has run it:**

- `EventScript_SkipIf` / `SkipSwitch` - an if or a switch *nested inside a
  skipped arm*; the catalogue lists neither. The decoder's indentation shows
  which areas nest one, and entering such an area with the outer condition
  the other way would run them.
- the ten op handlers that stay Capcom's. The catalogue lists none of them
  either - but `EventScript_Op` ran 93 times against the 75 calls into the
  four handlers that are ours, so about eighteen ops went to a handler of
  theirs, most likely `EventOp_0x`, the jump table's default. Which ones is
  not measured: they are not in the trace list. Taking them over is what
  would settle it.
- `EventScript_Run`'s two other callers, the talk scripts of the descriptor
  `+0x04` table (`MoveScript_SetTurnTarget`): talking to somebody would.
- a placement with the count already at 30 (an area with more than 30 objects
  on one path), a negative count (no script leaves one), and every condition
  but 0, 2 and 8.

## 9. Names

`symbols.toml`'s 2026-09-22 group C block: 23 new functions with `impl`, seven
callees typed and left Capcom's, and four data names -
`EventScript_FlagBank` `0x7DEE20`, `EventScript_OpLengths` `0x663B0C`,
`EventScript_Conditions` `0x663B30` (17 entries; `MoveScript_CounterOps`
follows it at `0x663B74`) and `Sprite_DirectionAngles` `0x65F644`. Five
entries that already existed - `EventScript_Run`, `Flags_Set`, `Flags_Test`,
`ScriptFlags_Set40`, `ObjTrio_SetBit40` - gained `impl` and the evidence a
whole reading warrants; their names and signatures are unchanged, since
groups A and B call them.

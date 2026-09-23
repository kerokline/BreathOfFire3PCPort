# The dialogue box

**Status:** IN PROGRESS (verified 2026-09-22)

**The live batch, 2026-09-22 (`ab24`, `analysis/validate_ab24.sh`):** the whole third round - groups H, J, K, L and M, 137 functions, 466 ours - checked at once, original against ours: the field, new-game, field-menu and menu-screens capture pairs identical (4, 9, 7, 5 of each), the 9-minute attract 55 of 55 (and 55 of 55 against `ab22`'s ours), the same attract in English 55 of 55, the oracle identical at all 7,478 compared frames, the memory dump identical, and the frame hash identical on all 10,063 frames (`ab24_orig` / `ab24_oursb`, beside an original-vs-original pair identical on 10,062).

Forty-three functions of the message box and the last two text pens, taken
over in `src/game/msgbox.cpp` (+ `msgbox_callees.h`, `msgbox_fuzz.cpp`) as
group H of the third parallel round. Every one was read to its last
instruction with capstone against `bof3/BOF3.exe`, and fuzzed against a
byte-copy of Capcom's at start-up (`BOF3X_SHADOW=msgbox`): **43,000 rounds,
0 mismatches**, with 51 planted bugs of which 48 are refused by a count and
one by a fault (§5).

What this family is: `Msg_OpenScript` points the box at a message, the frame
task runs a state machine over it once a frame, state 0 eats the control
codes ahead of the printer and decides how long the next character waits, and
`MsgBox_Step` re-walks the message from the top every frame and draws as many
characters as the count says. `Text_DrawAt` and `Text_EmitGlyph` are the two
pens underneath - the last two of the engine's text path that were still
Capcom's.

## 1. The functions

PC addresses are `BOF3.exe`'s; PSX addresses are the sibling's
`SLPS_009.90`, from `analysis/pairs_propagated.json` and confirmed by reading
both sides. **Sizes are the body's measured extent**, to the byte after the
last instruction (the two inline jump tables included in their owner's), not
`pe_funcs.py`'s - §6 lists what was wrong.

| PC | Name | Size | PSX | Calls (hidden_b) | What it does |
|---|---|--:|---|--:|---|
| `0x4976D0` | `Msg_OpenScript` | 0x32 | `0x8015034C` | 24 | message `id` of the script pool, then `MsgBox_Reset` |
| `0x497770` | `MsgBox_Reset` | 0x7F | `0x8015042C` | 24 | clears the state, eats a leading `0x0C`, claims window 0 |
| `0x4977F0` | `MsgBox_FrameTask` | 0x4C | `0x80150508` | 2,781 | the box origin, the state machine, the effect, one step |
| `0x497840` | `MsgBox_Step` | 0x22D + 0x5C table | `0x80150598` | 2,613 | the printer: the 23 control codes, one frame's characters |
| `0x497AD0` | `MsgBox_StateDispatch` | 0x55 | `0x801508EC` | ~2,781 | the eight states, through a table built on the stack |
| `0x497B30` | `MsgBox_StatePrint` | 0x301 + 0x5C table | `0x8015096C` | 351 | state 0: eats codes, sets the per-character delay |
| `0x497E90` | `MsgBox_StateDelay` | 0x16 | | | state 1: counts that delay down |
| `0x497EB0` | `MsgBox_State2` | 0x22 | | | state 2, two entries |
| `0x497EE0` | `MsgBox_State2Press` | 0x33 | | | a button (or a kept flag) closes window 0 |
| `0x497F20` | `MsgBox_State2Closed` | 0x11 | | 144 | window 0 at 4 -> flag 2, "the box is done" |
| `0x497F40` | `MsgBox_State3` | 0x22 | | | state 3, two entries |
| `0x497F70` | `MsgBox_State3Arrow` | 0x57 | | | the blinking "more" arrow; a button moves on |
| `0x497FD0` | `MsgBox_State3Step` | 0x76 | | | seven rows of scroll, then re-open or restart |
| `0x498050` | `MsgBox_State4` | 0x52 | | | state 4, a choice list, eight entries |
| `0x4980B0` | `MsgBox_ChoiceOpen` | 0x35 | | | claims window 1 kind 1 |
| `0x4980F0` | `MsgBox_ChoiceWaitOpen` | 0x19 | | | waits for both windows |
| `0x498110` | `MsgBox_ChoiceInput` | 0x82 | | | cancel / confirm / the cursor |
| `0x4981A0` | `MsgBox_ChoiceWaitShut` | 0x19 | | | toggles the hold flag back |
| `0x4981C0` | (state 4 entry 5) | 0x6D | | | **left Capcom's**, §4 |
| `0x498230` | `MsgBox_ChoiceReopen` | 0x1E | | | re-opens the message at the answer |
| `0x498250` | `MsgBox_ChoiceDone` | 0x27 | | | window 1 at 4 -> flag 0x80, state 2 |
| `0x498280` | `MsgBox_Reopen` | 0x40 | | | the message again from the top, id unsigned |
| `0x4982C0` | `MsgBox_State5` | 0x42 | | | state 5, the same list as a menu, six entries |
| `0x498310` | `MsgBox_MenuOpen` | 0x27 | | | claims window 1 kind 2 |
| `0x498340` | `MsgBox_MenuWaitOpen` | 0x10 | | | waits for window 1 |
| `0x498350` | `MsgBox_MenuInput` | 0x61 | | | confirm and the cursor, pad masked to 0x5000 |
| `0x4983C0` | (state 5 entry 3) | 0x4C | | | **left Capcom's**, §4 |
| `0x498410` | `MsgBox_MenuDone` | 0x31 | | | id 0xFFFF ends the box, else on to the scroll |
| `0x498450` | `MsgBox_State6` | 0x11 | | | state 6: back to state 0 (unreachable, §2) |
| `0x498470` | `MsgBox_State7` | 0x22 | | 2,040 | state 7, code 0x16's wait, two entries |
| `0x4984A0` | `MsgBox_State7Delay` | 0x34 | | 1,896 | counts that wait down |
| `0x4984E0` | `MsgBox_EffectTask` | 0x3F | `0x80151A1C` | 2,781 | the box effect, six entries |
| `0x498520` | `MsgBox_EffectShake` | 0x40 | | | effect 1: the pen x offset rocks +/- 2 |
| `0x498560` | `MsgBox_ShakeOut` | 0x1A | | | out to -2 |
| `0x498580` | `MsgBox_ShakeBack` | 0x1A | | | back to +2 |
| `0x4985A0` | `MsgBox_EffectGrow` | 0x34 | | | effects 2 and 3: the effect draw's offset |
| `0x4985E0` | `MsgBox_GrowStart` | 0x27 | | | turns flag 8 on |
| `0x498610` | `MsgBox_GrowStep` | 0x55 | | | walks the offset to its limit, then stops |
| `0x498670` | `MsgBox_EffectWander` | 0xCA | | | effect 4: both offsets wander |
| `0x498740` | `MsgBox_EffectRise` | 0x22 | | | effect 5, two entries |
| `0x498770` | `MsgBox_RiseStart` | 0x21 | | | an 8.8 fixed-point speed |
| `0x4987A0` | `MsgBox_RiseStep` | 0x38 | | | integrates it into the y offset |
| `0x59E2D0` | `Window_Alloc` | 0x40 | `0x80159874` | 24 | claims a window record |
| `0x516B30` | `Text_DrawAt` | 0x35 | `0x8014F6BC` | 38,560 | plants the pen, calls `Text_DrawString` |
| `0x516D50` | `Text_EmitGlyph` | 0x11B | | 85,739 | one glyph's `POLY_FT4` |

The PSX pairs are not a guess. `MsgBox_StateDispatch` / `0x801508EC` copies
the same eight handler pointers out of `0x80149A5C` onto its own stack and
calls entry `[byte 0x8014909C]`; the PC compiler materialised the eight
immediates instead. `MsgBox_Step` / `0x80150598` loads the walk pointer from
`0x801490A8`, the two pen words from `0x801490BC` / `BE` and the character
count from `0x801490B5` - the same `MsgBoxState` block at the same offsets -
and its newline adds `0xE` (`0x80150690`) and its `0x2A` hang is at
`0x801507A8`, exactly as the port's.

**A correction to `symbols.toml`:** the entry for `MsgBox_Step` carried
`psx = 0x8015096C`. That address is `MsgBox_StatePrint`'s twin, not the
stepper's - it begins with the substitution countdown on `0x801490A3` and the
pointer pair `0x801490AC` / `B0`, which is state 0's shape. `MsgBox_Step` is
`0x80150598`. Both fields are fixed.

## 2. The state machine, and what can reach what

`0x7DEE40` picks one of eight; each state picks one of its own on `0x7DEE41`;
the effect picks one of six on `0x7DEE42` and its phase on `0x7DEE43`.
**None of the six dispatchers bounds its index** - each is `mov dword
[esp+k], imm32` runs followed by `call dword ptr [esp + eax*4]`, so a byte
past the table calls whatever lies beyond the dispatcher's own stack frame.
Ours refuses loudly (`bof3::Fatal`) instead, which is sound because every
writer of every index byte is enumerable and none can leave the range:

| Byte | Written by | Range it can hold |
|---|---|---|
| `0x7DEE40` state | `MsgBox_Reset` (0), state 0's codes `0x00 0x02 0x0B 0x14 0x16` (2, 3, 1, 4 or 5, 7) and its glyph path (0, 1), `MsgBox_StateDelay` (0), `MsgBox_State3Step` (1), `MsgBox_ChoiceDone` (2), `MsgBox_Reopen` (1), `MsgBox_MenuDone` (2), `MsgBox_State6` (0) | 0..7 |
| `0x7DEE41` sub-state | `MsgBox_Reset` (0), state 0's code `0x0F` (0), and each state's own handlers, which only count up inside their own table or reset to 0 | 0..7 |
| `0x7DEE42` effect | `MsgBox_Reset` (0), state 0's code `0x0F` (the kind byte of a 4-byte record at `0x658E98`), and the effects themselves (0) | **unbounded in principle** |
| `0x7DEE43` phase | `MsgBox_Reset` (0), code `0x0F` (0), the effect handlers (0, 1) | 0..1 |

The one genuinely unbounded index is the effect kind: code `0x0F nn` reads
`0x658E98 + 4 * nn` with `nn` a message byte and no check, so a malformed
message can name any kind. The records that ship hold kinds 1, 2 and 3 (the
first twenty, read 2026-09-22: `01 00 3C 00`, `01 00 78 00`, `01 00 FF FF`,
then nine of kind 2 at offsets 6, 0x0B and 0x18, then kind 3 at -3, -6, -9).
**Nothing writes 6 to `0x7DEE40`**, so `MsgBox_State6` is dead as far as this
family goes.

Two dead branches inside state 0, at `0x497B80` and `0x497B86`: the code byte
is zero-extended at `0x497B78`, so the two comparisons against `0x100` can
never be greater-than or equal. The flag-word bit 8 that `0x497DA1` would set
is therefore never set here, although bit 8 is still *read* at `0x497DE5`,
where it forces delay 3 and state 1. Something outside this family sets it,
or nothing does.

`MsgBox_FrameTask`'s answer is a byte - bit 1 of the flag word, inverted -
and the original leaves the rest of `eax` holding the box origin x it
computed. `0x595A80` tests `al`; the two tail jumps at `0x595855` and
`0x595A6E` (group J's) hand it to their own callers. Ours returns
`unsigned char`, so the upper bytes are zero where the original's were the
box origin and the caller's own leftovers. Nothing can sanely depend on that,
and the live frame hash is the check.

## 3. The stepper, character by character

`MsgBox_Step` resets the pen to the box origin and walks the whole message
**from the top every frame**, stopping when the number drawn *equals*
`0x7DEE59`. So the box filling in is the count going up, one a frame, in
`MsgBox_StatePrint`; a colour or an offset a control code set is re-applied
on every frame's walk.

    0x00      end: outside a substitution it is SKIPPED, so the walk runs on
    0x01      newline: pen y += 14, pen x = the box's line start
    0x03      the current member's name, 8 characters
    0x04 nn   member nn's name, 8 characters
    0x05 nn   take colour nn;  0x06  colour 0
    0x07 nn   Text_Records + nn * 32, 32 characters
    0x08 nn   message nn of the sub-pool 0x803580 + [0x803584], 16 characters,
              starting ONE byte past where its header points
    0x0A 0x0C 0x0F 0x14    two bytes, nothing drawn
    0x0D / 0x0E            the offset pen on / off
    0x02 0x09 0x0B 0x10 0x11 0x16   one byte, nothing drawn
    0x12 0x13 0x15 and everything above 0x16   a glyph

A substitution is one deep; its countdown is decremented *before* the
character is read, so a count of 9 draws eight characters, and the return
position is the saved pointer **plus one**.

Two quirks worth naming. A first byte `0x2A` or `0x3C` exactly at the line
start is pulled 12 px into the margin - the PSX does the same for its own
quote at 8 px (`SLUS-00422 0x80150680`), which is what DIV-0006's hang
handling exists for. And under the offset pen (`0x0D`) the x comes from
`MsgBox_PenX` *in memory* while the y comes from the register copy, both
plus the signed byte offsets `0x7DEE5A` / `0x7DEE5B`; without it both come
from the registers and take no offset.

**DIV-0006 lives at the call now.** The original's one `Text_DrawAt` call at
`0x497A22` was re-aimed by `bof3::RetargetCall` in
`src/game/text_advance.cpp`; ours *calls* `MsgBox_DrawChar` there instead.
The rule is unchanged and still lives in that file: draw, then move
`MsgBox_PenX` by `advance - 12` so the stepper's own `+ 12` lands the pen by
the glyph's advance. With no advance table loaded - every shipped file -
`MsgBox_DrawChar` is `Text_DrawAt` and this is the original. **The
`RetargetCall` stays where it is**: it is what keeps DIV-0006 alive when the
owner runs `BOF3X_ORIGINAL=MsgBox_Step` and Capcom's body executes.
[`DIVERGENCE.md`](DIVERGENCE.md) DIV-0006 has the note.

`MsgBox_Step`'s other draw, `0x4987E0`, taken while flag 8 of `0x7DEE44` is
set, is still Capcom's and still 12 px (§4). One detail the takeover
settles: the original pushes the colour local there as a **dword whose upper
three bytes are stack it never wrote**, and `0x4987E0` reads
`mov al, [esp+4]` then `and eax, 0xF` - so only the low nibble can reach it
and passing the byte zero-extended is exactly equivalent. Likewise
`0x498D20` takes both of `MsgBox_State3Arrow`'s arguments with
`movsx ecx, word ptr` (`0x498D7C`), so the stale upper halves the original's
registers carry there are unobservable.

`Text_EmitGlyph` is faithful. Its texture window is a flat **12 units wide
whatever the quad** (`0` and `0x0C` in every u) with `u` as the row - the
fact DIV-0010 turns on. DIV-0010 itself is the config screen's own stand-in
for `Text_DrawAt` (`src/game/config_text.cpp`) and is not a change here.

## 4. Left Capcom's, and why

- **`0x4981C0`** (state 4 entry 5, the choice list's commit) and **`0x4983C0`**
  (state 5 entry 3, the menu's). Both do
  `call dword ptr [[Area_Descriptors + Game_AreaNumber * 4] + 0x34 + kind * 4]`
  - an indirect call through a per-area handler table picked by the choice
  id `0x7DEE64` - or, for an id `>= 0x80`, call `0x498A30`, which is itself a
  16-entry stack dispatcher on the same byte and unread. Neither can be
  cloned (the copy's indirect call reaches the real table) and neither can be
  read clean without the area descriptors. Rule 4: not touched, not stubbed.
  Ours calls both by address.
- **`0x4987E0`**, the stepper's effect draw. Unread, 12 px, named in
  `src/game/text_advance.cpp` as the last unconverted pen of the dialogue
  path. Its argument handling is now settled (above) but its body is not
  read. Ours calls it by address.
- **`0x498D20`**, the "more" arrow, and **`0x461EB0`**, the pad auto-repeat:
  both small and both read enough to call correctly, but outside this
  group's list. Called by address, with recording stand-ins in the fuzz.

## 5. The fuzz, and the controls

`BOF3X_SHADOW=msgbox`, at start-up, half a second. Forty-three byte-copies
are made before any inject; **every call out of a copy is re-aimed at a
recording stand-in**, the six dispatchers' stack-table *immediates* are
rewritten in the copy the same way (a copy carries absolute targets, exactly
as a jump table does), and the two inline jump tables - `MsgBox_Step`'s 23
codes at `0x497A70` and `MsgBox_StatePrint`'s at `0x497E34` - are relocated
into the copy with `move_script::Relocate`. Ours reaches the same stand-ins
through a pointer table, so each function is tested alone and inject order
cannot matter.

One round is one function: random bytes over the whole `MsgBoxState` block,
both window records, the three text pens and `Input_Pressed`; a freshly
generated message; that function's own branch boundaries seeded; then
theirs, then ours from the same state. Compared: the five regions, the
primitive buffer, the packet cursor, the return value (masked to the bits the
function actually defines) and the stand-ins' log.

    43,000 rounds (1,000 per function, 43 functions), 32,177 calls to the
    stand-ins, 0 MISMATCHES
    coverage: characters stepped 994, in a substitution 954, a glyph drawn
    761, the print state moved 817, a row scrolled 173, an effect ended 743,
    the cursor moved 1,317, a window taken 368

**Fifty-one negative controls.** Forty-eight are refused by a count, one by a
fault, three are unobservable by construction. Rounds are out of the 1,000
that function gets.

| Planted | Refused in |
|---|--:|
| `FrameTask`: box origin x + 11, not + 10 | 946 |
| `FrameTask`: the window y shifted by 3, not 4 | 944 |
| `FrameTask`: the answer is flag bit 0, not bit 1 | 513 |
| `FrameTask`: the flag read for the answer moved before the three calls | 45 |
| `Reset`: the flag word keeps 0x60, not 0x40 | 491 |
| `Reset`: the leading placement code is 0x0D, not 0x0C | 565 |
| `Reset`: the placement byte left alone when there is no 0x0C | 541 |
| `Msg_OpenScript`: the id zero-extended, not sign-extended | 158 |
| `Msg_OpenScript`: the message index stored before `MsgBox_Reset` | 23 |
| `Step`: the newline moves the pen 12 px, not 14 | 111 |
| `Step`: only 0x2A hangs into the margin, not 0x3C as well | 16 |
| `Step`: the pen advances 11 px a glyph, not 12 | 761 |
| `Step`: the 0x04 substitution runs 9 characters, not 8 | 41 |
| `Step`: the pen NOT reloaded from memory after the draw | 206 |
| `StatePrint`: code 0x0B sets the delay to 8 instead of adding 8 | 11 |
| `StatePrint`: code 0x14 picks state 4 for a zero column and 5 otherwise | 7 |
| `StatePrint`: the speed table's two columns swapped | 284 |
| `StatePrint`: a zero delay leaves state 1, not state 0 | 432 |
| `StatePrint`: the substitution returns to the saved pointer, not one past | 51 |
| `StatePrint`: code 0x08 clears the keep-going flag | 11 |
| `StatePrint`: code 0x0A does not reload the two pointers after the sound | 20 |
| `StatePrint`: the sound id without the 0x200 | 14 |
| `State3Step`: the delay tested after the decrement, not before | 363 |
| `State3Step`: the scroll ends after 6 rows, not 7 | 57 |
| `State7Delay`: the delay tested after the decrement, not before | 349 |
| `StateDelay`: the byte tested before the decrement, not after | 302 |
| `State2Press`: the kept flags tested as 0x80 alone, not 0xC0 | 122 |
| `State2Closed`: window 0 is done at 3, not 4 | 154 |
| `State3Arrow`: the arrow x takes the row byte whole, not halved | 998 |
| `ChoiceInput`: the bottom of the list compared unsigned, not signed | 164 |
| `ChoiceInput`: confirm advances the sub-state by one, not two | 91 |
| `MenuInput`: the pad handed to the auto-repeat unmasked | 509 |
| `ChoiceWaitShut`: the hold flag cleared rather than toggled | 88 |
| `Reopen`: the message id sign-extended, as `Msg_OpenScript` takes it | 179 |
| `EffectShake`: the timer counted down even at 0xFFFF | 326 |
| `GrowStep`: kind 2 stops at zero as well as below it | 15 |
| `GrowStep`: every kind walks the offset up, kind 2 included | 124 |
| `EffectWander`: the x offset stored after the wait counter, not before | 186 |
| `EffectWander`: the turn-around y is 0xF7, not 0xF6 | 22 |
| `RiseStep`: the accumulator high byte taken after the add, not before | 607 |
| `RiseStart`: the speed scaled by 8, not 16 | 1,000 |
| `Window_Alloc`: the failure answer is 0xFF, not the argument with its low byte set | 341 |
| `Window_Alloc`: the slot index not masked to a byte | a fault |
| `Text_DrawAt`: the pen y is the argument, not the argument plus one | 1,000 |
| `Text_DrawAt`: the line start not planted | 1,000 |
| `Text_EmitGlyph`: the far texture row is u - v, without the 12 | 1,000 |
| `Text_EmitGlyph`: the texture window takes the quad width, not a flat 12 | 943 |
| `Text_EmitGlyph`: the corners take v where the original takes u | 971 |

`Window_Alloc`'s unmasked slot writes far outside the record block and kills
the process instead of producing a count - a refusal that proves less than a
count does (`HANDOFF.md`, "a control refused by a HANG proves less"), so it
is listed as what it is.

**Three the harness was blind to, and the fix each needed.** Every one is
the trap the handoff names - a control that is not refused is information:

1. `Msg_OpenScript`'s sign-extended index and `MsgBox_Reopen`'s unsigned one
   both read an untouched **zero** word, because the pool below index 0 was
   never seeded. Dropping either extension was refused by **none** of 1,000
   rounds. The 0x20 bytes below the pool are seeded now, and ids `0xFFF8`
   and `0xFFFF` are in both seed lists: 158 and 179 rounds.
2. The character records (`0x903A70 + n * 164`) and the text records
   (`0x904CE0 + n * 32`) are **zeros** at start-up, so every substitution met
   code `0x00` on its first character and returned - its count (8, 16, 32)
   could not be observed at all. Changing 9 to 10 was refused by none. Both
   blocks now hold plain glyphs and the step count is seeded up to 20, so a
   substitution outlasts it: 41 rounds.
3. The sound stand-in was **quieter than `Sound_PlayEffect`**: it did not
   move the two pointers `MsgBox_StatePrint` reloads after that call and
   after nothing else, so dropping the reload was invisible. It moves both
   every call now: 20 rounds. (This is the movement-script trap again.)

**Three that are unobservable by construction**, asked before being called
blind:

- *`MsgBox_Step` re-reads `[esi]` after the draw to decide whether to skip a
  second text byte.* `esi` is not moved between the two reads, so the only
  input that could tell the re-read from the register apart is a callee that
  **writes into the message** - which neither `Text_DrawAt` nor `0x4987E0`
  does. Kept from the disassembly, not from a control.
- *The offset pen takes x from `MsgBox_PenX` in memory, not from the
  register.* Every path that changes one writes the other in the same breath
  (entry, newline, the hang, after each draw), and no call sits between, so
  the two are provably equal at that point. Also kept from the disassembly.
- *The walk ends when the count **equals** `0x7DEE59`, never when it passes
  it.* The difference shows only if a callee lowers the count below the
  number already drawn - which makes **Capcom's** walk run off the end of the
  message, i.e. a start-up hang rather than a mismatch. The harness therefore
  only ever raises that byte, and the `==` is kept from the disassembly. It
  is also a latent defect in the original: see §6.

## 6. What the catalogue had wrong

- `MsgBox_Step` `0x497840` is **557 bytes** of body plus a 92-byte jump
  table, 0x28C in all. `pe_funcs.py` says 752 and `entries_logic.txt` says
  **0xA37** - 2,615 bytes, which swallows `MsgBox_StateDispatch`,
  `MsgBox_StatePrint` and every state handler up to `0x498277`.
- `0x498280` is **0x40** bytes; `entries_logic.txt` says 0x254.
- `0x4984E0` is **0x3F** bytes; `entries_logic.txt` says 0x2F8.
- `0x4984E0`'s stack table has **six** entries, not the four the round's task
  list named: entries 2 and 3 are both `0x4985A0`, loaded into `eax` once at
  `0x4984E3` and stored twice. `0x4985A0` and its two handlers `0x4985E0` /
  `0x498610` were missing from the list entirely.
- `MsgBox_StatePrint` `0x497B30` is **769 bytes plus a second 92-byte jump
  table**; `entries_hidden.txt` says 0x100. A second table inside what
  `pe_funcs.py` calls one 256-byte function.
- `MsgBox_Step`'s `psx` field named state 0's twin (§1).
- Every other `entries_hidden.txt` size in this family is the *padded* extent
  (rounded up to the next 16), which is harmless for a range but is not the
  body.

Two latent defects of Capcom's, written down and not fixed (neither is
reachable by a shipped script as far as the reads go):

- **Six unbounded stack dispatchers.** Any of `0x7DEE40..43` past its table's
  length calls off the dispatcher's own stack frame. The one that a message
  could actually drive is the effect kind, through code `0x0F`'s unbounded
  index into `0x658E98`.
- **An unterminated instant-print span hangs the game.** With flag `0x10` set
  `MsgBox_StatePrint`'s tail always puts the keep-going flag back, so the
  loop runs until a `0x10` or `0x11` toggles the flag off - a message that
  opens instant print and never closes it never returns. The same shape
  appears in `MsgBox_Step`: a step count that a callee lowers below the
  number already drawn walks the message pointer forward without end.

## 7. What no check reached

The attract sequence reaches **states 0 and 7 only** (`hidden_a`, a first-hit
trace of every hidden start, never hit 1..6) and **effect kind 0**, the bare
`ret` at `0x437CC0`. So everything below is fuzz-only - correct against a
byte-copy of Capcom's on 43,000 random states, and never yet run in the game
by ours. What would reach each, in game:

| Not reached | What would reach it |
|---|---|
| `MsgBox_State2`, `State2Press` | the end of any message - press any button at the "waiting" box. Reached constantly in play, not by the attract, which presses nothing |
| `MsgBox_State3`, `State3Arrow`, `State3Step` | a message longer than the box: the page-break code `0x02`, then a button. The arrow is the blinking marker under the box |
| `MsgBox_State4` and its seven | a **choice list** - code `0x14` with a non-zero high nibble. Any yes/no or multiple-answer question |
| `MsgBox_State5` and its five | the same code with a **zero** high nibble - the menu-shaped list (shops, the masters) |
| `MsgBox_State6` | nothing: no writer of `0x7DEE40` in this family sets 6 (§2) |
| `MsgBox_Reopen` | the frame after a choice or a menu is answered, and every seventh row of a scroll |
| effects 1..5 (`0x498520`, `0x4985A0`, `0x498670`, `0x498740` and their eight) | control code `0x0F nn` in a message, `nn` naming a record at `0x658E98`. The shipped records are the shake (kind 1), the grow (2 and 3)... the wander and the rise have no record in the first twenty read |
| `Window_Alloc(1, ...)` | only the two list openers, so the same as state 4 and state 5 |

Reached and live-checked by the batch: `Msg_OpenScript`, `MsgBox_Reset`,
`MsgBox_FrameTask`, `MsgBox_Step`, `MsgBox_StateDispatch`,
`MsgBox_StatePrint`, `MsgBox_State7`, `MsgBox_State7Delay`,
`MsgBox_State2Closed` (state 7's entry 1, 144 calls), `MsgBox_EffectTask`,
`Text_DrawAt` and `Text_EmitGlyph` - the last two at 38,560 and 85,739 calls
an attract cycle, so the frame hash covers them thoroughly.

**Still open, and not answered here:** which of the 23 control codes the
attract sequence's eight messages actually use
([`HANDOFF.md`](HANDOFF.md) item 6). The jump table says what each code
*does*, which is §3, but which appear in those eight messages is a property
of the script data and needs a live count - the cheapest form is a 23-slot
counter in `MsgBox_StatePrint`'s switch, logged once at the end of a run,
which nothing in the tree has yet. It is not in this change.

## 8. Files

- `src/game/msgbox.cpp` - the forty-three, and `MsgBox_Inject` (last in
  `inject_all.cpp`; every call of its clones is re-aimed, so order does not
  matter).
- `src/game/msgbox_callees.h` - the addresses with no name in `symbols.toml`,
  the callee pointer table, the byte/word accessors.
- `src/game/msgbox_fuzz.cpp` - the clones, the stand-ins, the seeding.
- `symbols.toml` - 35 new `[[func]]` entries in group H's own block at the
  end, plus a signature and `impl` on the eight that already existed.
- `analysis/calltrace/entries_logic_round3_h.txt` (local, gitignored) - the
  43 ranges for the trace list, with the three sizes that must be *replaced*
  rather than appended (§6).

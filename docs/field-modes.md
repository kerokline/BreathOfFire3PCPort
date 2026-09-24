# The field's mode handlers: the scenario dispatcher and scenario 16

**Status:** IN PROGRESS (2026-09-22) - seventeen functions ours: the
scenario dispatcher `Field_ModeDispatch` with the two tail dispatchers after
it, the chapter call table's thunk `Scenario_CallA`, scenario 16's eleven
handlers (the attract sequence's demo) and the CLUT strip fade and restore
under them. Each fuzzed against a copy of Capcom's with every call re-aimed
at a recorder and every table it reads swapped for recording entries; 37
negative controls, all refused. **Through the live batch check `ab22` + `ab22b`** (2026-09-22, the second parallel round - [`HANDOFF.md`](HANDOFF.md)); section 8 had it owed.

[`attract-remaining.md`](attract-remaining.md) section 4.6 listed this as
"the dispatcher `0x56D690` and the handlers it reaches - `0x6619E8` is a
table of them ... `0x662CC0` points at it and at three sibling tables". What
the tables are: **`0x662C80` is the PSX's SCENARIO vtable table**, one entry
per scenario chapter, and the byte that indexes it - `Cond_ByteFA`
`0x8034E0` - is the chapter (the sibling's `docs/LOADER_RECORDS.md`: PSX
`0x801C944C`, chapter `s8[0x8014686C]`, 20 chapters, each a `SCENA<nn>.EMI`
overlay at `0x801F6C00`). `0x662CC0` is entry 16, and **chapter 16 is the
attract demo** (`SCENA16.EMI`); `0x662CC4` is 17, the credits (the
sibling's `DATA_ISLANDS.md`: SCENA17 holds the staff roll). The PC compiled
every chapter into the exe; the vtables stayed tables.

## 1. The chain, frame by frame

```
Field_ModeDispatch 0x56D690          (GameMode_Field, 0x517240, 0x5172F0, 0x595056, 0x56D5E0's tail)
  [[0x662C80 + chapter * 4]]()       vtable slot 0 of the chapter
    = Scena16_Frame 0x56B2A0         for chapter 16: jmp [0x6619FC + state * 4]
        0 Scena16_Start              the demo's first frame, state -> 1
        1 Scena16_EnterArea          once per area entered, state -> 2
        2 Scena16_Run                jmp [0x661A08 + run * 4], run = MoveScript_Var7
            0 0x437CC0 (ret)   1 Scena16_End   2 Scene2   3 Scene3   4 Scene4   5 0x43C9F0
  jmp Field_ModeTail 0x56D8B0        jmp [0x662CD0 + phase * 4], phase = s8 0x9039F2
        0 0x56D8C0 (Scenario_SubInit, group E: sets phase 1)
        1 Field_ModeTailRun 0x56D920 jmp [0x662CE8 + kind * 4], kind = s8 0x9039F3 (0: a bare ret)
```

Every index is signed and unchecked, as on the PSX. The PSX has the same
three levels: the thunk `0x801A8834` calls slot 0 of `0x801C944C[chapter]`
and then `0x801A8BF8`, which calls through `0x801D949C` on `0x801448E6`;
`0x801A8CB4` is the third on `0x801448E7`. The PC turns each second call
into a tail jump.

Chapter 16's vtable `0x6619E8` is `0x56B2A0`, `0x56C080`, `0x43C9F0`,
`0x43C9F0` - PSX `0x801F8524`: `0x801F6C90`, `0x801F8344`, `0x801F838C`,
`0x801F8394` (two `return 0`s). Slot 1 (`0x56C080`, called by
`Field_ObjectTrigger` with an object) is not reached by the attract run and
stays Capcom's.

**The state table and the run table overlap.** `Scena16_Frame` reads
`0x6619FC` (`Scena16_Start`, `Scena16_EnterArea`, `Scena16_Run`) and
`Scena16_Run` reads `0x661A08` - the same words from +0xC on. The PSX has
them adjacent too (`0x801F8538`, `0x801F8544`).

## 2. The scenario bytes

| PC | PSX | Use |
|---|---|---|
| `0x8034E0` `Cond_ByteFA` | `0x8014686C` | the chapter, s8 (not renamed: other groups read it) |
| `0x8034E2` | `0x8014686E` | scenario 16's state, s8 |
| `0x8034E4` `MoveScript_Var7` | `0x80146870` | the run, s8 - which scene. The movement script's op F6 sets it, so the field scripts move the demo on |
| `0x8034E5` | `0x80146871` | the scene's step, u8 |
| `0x8034E6` | `0x80146872` | the scene's timer, u16 |
| `0x903848..0x90384B` | `0x80146860..63` | the four script counters (`MoveScript_CounterOps`); the scenes wait on them |
| `0x903850` | `0x1F800000` | the effect slot just taken (a byte), and scene 4's step 4 word |
| `0x929ED0` | `0x80146868` | the flag bits `Flags_Test` / `Flags_Set` are given |
| `0x66C7F8` `Title_BadgeOn` | `0x80143C30` | the demo's corner badge |

## 3. Scenario 16, handler by handler

Each is the PSX `SCENA16.EMI` function at the same table slot, read side by
side (capstone MIPS of the sibling's overlay capture `d9e4564d`). Sizes are
whole bodies, jump tables included.

| PC | Name | Bytes | PSX | Does |
|---|---|--:|---|---|
| `0x56B2A0` | `Scena16_Frame` | 0xE | `0x801F6C90` | the state's handler (tail jump) |
| `0x56B2B0` | `Scena16_Start` | 0x86 | `0x801F6CCC` | `Scenario_CallA(0)`; pass flags `0x1F`; the area change `0x594E00(4, 0x1A0000, 0x88000, 5)` (PSX `Field_ChangeArea` `0x801A0A30`) and the same four into `0x903A04..`; script flags `|= 0x240`; `0x587A20(6)`; `Task_Sleep(1)` until `File_LoadDone`; counters 0; state 1 |
| `0x56B340` | `Scena16_EnterArea` | 0x57 | `0x801F6D90` | on `Game_AreaNumber` 0x1F / 4 / 2 its set-up, badge 0 / 1 / 1; state 2 for any area |
| `0x56B3A0` | `Scena16_Area1F` | 0x5A | `0x801F6E30` | camera angles (0x100, 0, 0), distance 0x100, `ObjTrio_SetBit40`; once (flag 1): pass flags 0, run 3 |
| `0x56B400` | `Scena16_Area04` | 0x4C | `0x801F6EB0` | once (flag 0): pass flags 0, `ScriptFlags_Set40`, run 2; then with `Cond_ByteFD` 2: pass flags 0x1F, `ObjTrio_SetBit40` |
| `0x56B450` | `Scena16_Area02` | 0x108 | `0x801F6F30` | once (flag 2): counters 0, elevation 0x300, `Kind2_Place(0)`, angle 0 + 0xAA, an effect (mode 0x40), `ObjTrio_SetBit40`, `0x802D74` / `78`; else once (flag 10): `0x531F90(0)` and `Field_Kind2X` / `Z` |
| `0x56B560` | `Scena16_Run` | 0xE | `0x801F7144` | the run's handler (tail jump) |
| `0x56B570` | `Scena16_End` | 0x51 | `0x801F7188` | run 1, once the wait word is 0: badge and pass flags off, the strip restored, effects 0x213 / 0x214, the music stopped, the task ended (`jmp Task_Exit`) |
| `0x56B5D0` | `Scena16_Scene2` | 0x37C | `0x801F7230` | run 2: steps 0-11 (below) |
| `0x56B950` | `Scena16_Scene3` | 0x364 | `0x801F7790` | run 3: steps 0-9, then the view test |
| `0x56BCC0` | `Scena16_Scene4` | 0x3B8 | `0x801F7CC4` | run 4: steps 0-12 |
| `0x56C0A0` | `ClutStrip_FadeTo` | 0x68 | `0x801F839C` | 16 colours of `Gfx_ClutStrip` from the source, each channel clamped to a level (section 7) |
| `0x56C110` | `ClutStrip_Restore` | 0x20 | `0x801F8448` | the 16 colours copied back |

**Two catalogue entries are not functions.** `0x56B730` (608 bytes by
`pe_hidden.py`) is case 4 of `Scena16_Scene2`'s switch, and `0x56B990`
(1,807 bytes, "the largest") is case 1 of `Scena16_Scene3`'s: both are
entries of the scenes' inline jump tables (`0x56B91C`, `0x56BC8C`), which
`pe_hidden.py` took for pointer tables. And the two real functions after
them, **`0x56B950` and `0x56BCC0`, are missing from `pc_hidden.json`**
although `0x661A14` / `0x661A18` point at them - so `0x56B990`'s 1,807
bytes ran on through both. That is the inline-jump-table blind spot of
HANDOFF's "found on the way" again.

### 3.1 The three scenes

A step byte and a timer; each step waits on something, does one thing and
moves on. Captions are script-pool lines (`Text_DrawAt(x, y, 0, 0xFF,
0x803580 + u16[0x803580 + 2 * id])`), drawn every frame they show.

- **Scene 2** (run 2): transition in, music 6, script flags `|= 0x80`;
  caption 0x10 at (0x82, 0x64) held while the wait word is set, then 0x7F
  frames; out. Pass flags 0x1F, `ScriptFlags_Set40`, `ObjTrio_SetBit40`, in,
  counter 3 = 1; on counter 3 = 0x54 out; in; caption 0x11 at (0x44, 0x50)
  held, 0x7F frames, out; the change to area 4 at (0x440000, 0x80000); on
  `Cond_ByteFD` 2 pass flags 0x1F; on counter 2 = 0x23 an effect (mode 0x60,
  x = angle 0 + 0x180); on counter 3 = 0x60 the change to area 0x1F at
  (0x70000, 0x140000), `0x937F98` = 0xFF, the music stopped over 0x40,
  counter 2 = 0, run 3.
- **Scene 3** (run 3): in; caption 0x02 at (0x4C, 0x50) held, 0x7F frames,
  out; in, counter 2 = 0x30, pass flags 0x1F, an effect (mode 0xFF, angle 0
  + 0x1C0); caption 0x03 with the strip faded up over 0x20 frames
  (`ClutStrip_FadeTo(timer)`, restored at 0x20), held 0x7F, faded down over
  0x20 and restored, 0x1E frames' pause; music 2, the change to area 2 at
  (0x340000, 0x430000) facing 3, run 4. **After every step - and after a
  step above 9 - the view test:** while `MapView_FocusZ` is 0x4400 it moves
  `Field_Kind2Z`, `0x802038` and `0x7E0978` back 0x1E0000, the word
  `0x7E068A` back 0x1E, the focus to 0x6200, and tail-jumps to `0x56FCA0`
  (PSX `0x80155154`).
- **Scene 4** (run 4): script message 1 (`Msg_OpenScript`, request 2)
  waited out; on counter 0 = 3 message 2; on 4, 0x130 frames of caption 0x03
  at (0x64, 0x50): with `into = 0x130 - timer` (a u16, also stored as the
  word `0x903850` and read back after the caption) below 0x20 a fade to
  `into`, at 0x20 the restore, above 0x9F a fade to `0xBF - into` (as a
  byte), at 0xBF the restore without the caption, above nothing; and every
  frame the camera distance `(0x67FFF90 - (timer - 1) * 0x57943) >> 16`
  (unsigned) while the timer counts down. On counters 5 and 8 two effects
  (x -0x34A mode 0x60; x -0x2AC mode 0x20), then 0x200 frames of distance
  `((13 * timer) << 14) >> 16` (unsigned, wrapping); on 0xB out; in with the
  music stopped over 0x20; caption 0x04 at (0x5E, 0x64) held, 0x7F frames,
  out; once the wait word is 0, run 1 - `Scena16_End`.

### 3.2 `Scenario_CallA` `0x5341A0`

27 bytes: `jmp [[0x660B84 + chapter * 4] + (n & 0xFF) * 4]` - PSX
`Scenario_CallA` `0x801C2DE8` through `0x801CDC4C`. **Kept a tail jump,
written as a naked function**: the entries take the caller's arguments
where they lie, and 153 call sites (a rel32 scan of `.text`) push anything
from one argument to several with cleanup deferred and merged (`add esp, 8`
after two calls, and so on), so no C++ signature forwards them all. None of
the 153 reads eax after the call (capstone, up to the first branch or write
of eax). The fuzz checks that the entry sees the caller's two argument
words unmoved (C5). Chapter 16's table (`0x65F8C0`): `0x519890`, `0x437CC0`.

## 4. What differs from the PSX

All kept - faithful takeover of the port:

- **The music.** `Scena16_End` stops the music over 10 frames where the PSX
  restarts the area's track from its table (`0x8015DDF4`) and sets the
  volume (`0x8016CA38(0x7F, 0x7F)`); scene 2's step 0 and step 2 drop the
  PSX's volume calls (`0x8016CA38(0, 0)`, then `(timer, timer)` - a fade-in
  in step with the caption's timer); `Music_Play` takes two arguments where
  the PSX's `0x80162610` took three. The same rewrite as
  `GameMode_Field`'s area change ([`mode-tasks.md`](mode-tasks.md) section 3).
- **`0x937F98`.** Scene 2's last step stores 0xFF there; the PSX stores it
  to `0x80143F1D`, which by the arena's delta would be `0x904F19`. Unread.
- **Shared tails.** Where the PSX repeats a call in two steps, the PC's
  compiler merged them (scene 2's steps 4 and 7 end in one
  `Transition_Start(0)`; the held captions share one call) - call sites
  differ in number, behaviour does not.

## 5. The fuzz

`BOF3X_SHADOW=field_modes`, at start-up (`src/game/field_modes_fuzz.cpp`):
seventeen byte-copies, every call out re-aimed at a recorder (the capstone
lists in `kClones`), the three scenes' jump tables relocated into their
copies, and five tables swapped for recording entries for the duration -
the 20 chapter vtables, the 20 call tables (each 256 entries, scattered
over 16 recorders so that an index off by a power of two lands elsewhere),
`0x662CD0`'s two, `0x662CE8`'s first eight, and the nine words at
`0x6619FC` that hold both the state and the run table. The indices are
seeded inside the swapped windows: outside them the original jumps through
whatever words follow, and so would ours. 34,000 rounds, 2,000 per function;
random bytes in every region any of them touches (the scenario bytes, pass
and script flags, the tail bytes, the pending change, the camera and
distance, counters and slot, request, badge, wait word, track, the first
eight effect records, `0x802D74`, `Field_Kind2X` / `Z`, the strip, its
source and dirty count, the view words, the area number, the five pool
offsets), each branch's boundaries seeded (areas 2 / 4 / 0x1F, every step,
timers at each step's turning points, counters 3 / 4 / 5 / 8 / 0xB, 0x23,
0x54, 0x60, focus 0x4400, fade levels 0x1F / 0x20 / 0xFF / 0x11F), both
sides from the same state; the regions and the recorders' log compared.
The recorders change what their caller reads again after them (the step,
timer, wait word, request, counters, slot word, pass flags, run, camera,
track, `Cond_ByteFD`, flag pointer, area); `File_LoadDone`'s answers "not
yet" 0-3 times; the effect slot is one of the first eight or 0xFF.

Result (2026-09-22): **0 mismatches**, 58,795 calls to the stand-ins:

    shadow      field_modes self-test: 34000 rounds (2000 per function), 58795 calls to the stand-ins, 0 MISMATCHES; ...
    shadow      field_modes coverage: scene 2 steps 0..12+: 140 135 146 127 138 151 123 149 138 122 126 114 391;
                scene 3: 160 144 165 144 176 164 181 146 159 159 402; scene 4: 134 129 132 122 129 131 123 131 143 119 126 128 104 349
    shadow      field_modes coverage: areas 0x1F 117, 4 103, 2 111, other 1669; effects placed 1029, no slot 190; view shifted 1334; fades 322

With `BOF3X_SHADOW='*'` every self-test passes (exit 0).

**Negative controls**, each planted in `field_modes.cpp`, built, run
(`BOF3X_SELFTEST_ONLY=1`) and reverted by a script; every one refused with
exit 3 - mismatching rounds in brackets:

| | Planted | Refused |
|---|---|--:|
| C1 | dispatch: the tail not run | 2,000 |
| C37 | dispatch: the next chapter's vtable | 2,000 |
| C2 | tail: indexed by the kind byte | 1,035 |
| C3 | tail run: index + 1 | 2,000 |
| C4 | call A: index `& 0x7F` | 1,006 |
| C5 | call A: the arguments moved (call instead of jump) | 2,000 |
| C6 | frame: state + 1 | 2,000 |
| C35 | run: index + 1 | 2,000 |
| C7 | start: pass flags stored before `Scenario_CallA` | 102 |
| C8 | start: sleeps before asking `File_LoadDone` | 2,000 |
| C9 | start: pending facing 4 | 2,000 |
| C10 | enter: area 4's badge off | 103 |
| C11 | enter: another area keeps the state | 1,661 |
| C12 | area 1F: flag pointer read once | 72 |
| C13 | area 1F: run 2 | 999 |
| C14 | area 04: tail on `Cond_ByteFD` 3 | 1,170 |
| C15 | effects: x read before the slot is taken | 86 |
| C16 | area 02: second flag 0xB | 958 |
| C17 | end: the task ended only with music playing | 717 |
| C18 | end: effect 0x215 | 1,379 |
| C19 | scene 2: the timer counted after the caption | 25 |
| C20 | scene 2: last step to run 4 | 33 |
| C21 | scene 2: step 4 on 0x53 | 35 |
| C36 | scene 2: flag 0x80 into the high byte | 64 |
| C22 | scene 3: no view test after a step above 9 | 264 |
| C23 | scene 3: step 5 restores at 0x1F | 31 |
| C24 | scene 3: step 7 draws before counting | 146 |
| C25 | scene 3: view word back 0x1F | 1,334 |
| C26 | scene 4: level not read back after the caption | 5 |
| C27 | scene 4: fade out from 0x9F | 7 |
| C28 | scene 4: distance slope 0x57942 | 11 |
| C29 | scene 4: step 7 shift 13 | 117 |
| C30 | scene 4: step 1 waits on any request | 64 |
| C31 | fade: channels kept in place (no red / blue swap) | 1,754 |
| C32 | fade: bit 15 always set | 172 |
| C33 | fade: level taken whole | 41 |
| C34 | restore: dirty not counted | 2,000 |

The first run of the controls had three not refused, each explained: C4
matched because the call tables repeated every 16 entries (then scattered);
C37 was a change that changed nothing (`vtable - 4` slot 1 is slot 0); and
two controls faulted (exit `0xC0000005`) because they indexed outside the
swapped tables - rewritten to stay inside.

**What the fuzz cannot see:** anything the callees really do, and the
coroutine behaviour: `Scena16_Start`'s `Task_Sleep` switches tasks in game,
a recorder here; `Task_Exit` does not return in game, a recorder returns.
The dispatchers' tail jumps are calls in ours: a table entry that read its
stack arguments would see our frame where the original's saw its caller's -
no entry of these tables takes arguments (they are called argumentless),
and the fuzz does not compare those words. Indices outside the tables and a
negative chapter are not exercised (they would jump through data on both
sides). Unobservable and without control: `ClutStrip_FadeTo`'s garbage upper
bits of `ecx` / `edx` (only the low 16 bits are stored), the upper bytes of
the fade level the scenes push (the callee reads the byte), and the order
of stores with no call between them.

## 6. The callees

| Callee | What | Owner |
|---|---|---|
| `0x594E00` | the area change: (area, x, z, facing) - PSX `Field_ChangeArea` `0x801A0A30` | none, by address |
| `0x587A20` | (6) at the demo's start - PSX `0x801625AC` | none, by address |
| `0x589810` | takes an effect slot, 0xFF none - PSX `0x8019701C` | none, by address |
| `0x531F90` | (0) in area 2 - PSX `0x801BF1A8` | none, by address |
| `0x56FCA0` | the view shift after scene 3's focus test - PSX `0x80155154` | none, by address |
| `0x4976D0` | `Msg_OpenScript` (no signature in symbols.toml) | by address |
| `0x5A99AD` | `Task_Exit` (group E's name) | by address here |
| `Flags_Test`, `Flags_Set`, `ScriptFlags_Set40`, `ObjTrio_SetBit40` | | group C |
| `Kind2_Place` | | nobody this round |
| `MapView_SetElevation`, `Text_DrawAt`, `Transition_Start`, `Music_*`, `Sound_PlayEffect`, `File_LoadDone`, `Task_Sleep` | | as named |

## 7. Found on the way

- **`ClutStrip_FadeTo` swaps red and blue** (both platforms, a latent
  defect candidate - not fixed). It takes the source's channels from bit 0
  up and shifts each in from the bottom, so bits 0-4 end in 10-14 and 10-14
  in 0-4; `ClutStrip_Restore` copies straight. Unless the 16 colours are
  grey (R = B), whatever draws with them is red / blue swapped for the
  length of every fade and snaps back at the restore. Which 16 colours
  they are in the demo, what uses them, and whether the owner sees it, is
  unchecked - captures of scene 3's and scene 4's fades would say. C31 shows the fuzz would catch a change.
- `pe_hidden.py`'s inline-jump-table blind spot (section 3): two case labels
  listed as functions, two functions missing.
- `entries_logic.txt`'s sizes for this block are the catalogue's run-ons:
  `0056B450 C4F` covers everything from `Scena16_Area02` to `0x56C09F`
  (the run table's handlers and the three scenes), and `0056C110 A8F` runs
  from `ClutStrip_Restore` through scenario 17's handlers to `0x56CB9F`.
  The true sizes are in section 3.

## 8. In game: what the attract cycle reaches

From `hidden_b` (a whole attract cycle): `Field_ModeDispatch`,
`Field_ModeTail`, `Field_ModeTailRun`, `Scena16_Frame` about 11,900 each,
`Scena16_Run` 11,888, `Scena16_Scene2` 6,177 (its case 4, as the hidden
start `0x56B730`, 1,923), `Scena16_Start` 3, `Scena16_EnterArea` 12, the
three area set-ups 3 / 6 / 3, `Scena16_End` 2, `ClutStrip_FadeTo` 375,
`ClutStrip_Restore` 14, `Scenario_CallA` 3. Scenes 3 and 4 were never armed
(not in `pc_hidden.json`), but calls from inside them are counted: every
scene's captions, fades, effects and area changes. By call site, **not
reached**:

- `Scena16_Start`'s `Task_Sleep` - `File_LoadDone` is always 1 on the PC,
  so the wait never sleeps;
- `Scena16_Area02`'s second branch (flag 10: no call to `0x531F90`) - area
  2 is entered with flag 2 clear each cycle;
- steps whose calls the trace does not count (`Transition_Start` is not in
  its list) or that call nothing - scene 2's 4, 5, 9; scene 3's 0, 4, 8;
  scene 4's 1, 3, 7, 8 - are reached by the order of the steps around
  them, not by a count; the view test's shift (`0x56FCA0` is a tail jump)
  is not counted either way.

The live batch check should put all seventeen on its `--original` list.
Every one is logic, so the frame hash re-records; the sizes in section 3
replace `entries_logic.txt`'s two run-ons, and `0x56B2A0`, `0x56B2B0`,
`0x56B340`, `0x56B560`, `0x56B570`, `0x56B5D0`, `0x56B950`, `0x56BCC0`,
`0x56D8B0`, `0x56D920` join it.

# The top-level mode tasks

**Status:** IN PROGRESS (2026-09-22) - six functions ours: task 1's title
sequence (its body, the start check, the backdrop, the logo and the sprite
helper under them) and the field task's mode-2 handler, each fuzzed against
a copy of Capcom's with every call re-aimed at a recorder, 31 negative
controls all refused. **Not yet through the live batch check** - that runs
centrally after the merge (section 6).

[`attract-remaining.md`](attract-remaining.md) section 4.3 catalogued this
layer as "the task at `0x4621C0` and its state table ... what each state is
... is unread". This document reads it. The machinery under it - four
coroutine tasks, `Task_Sleep`, the `0x4000`-byte stacks - is
[`attract-mode.md`](attract-mode.md) section 2.

## 1. Who runs what

| Task slot | Entry | What it is |
|--:|---|---|
| 0 | `0x496B60` (WinMain), later `0x495800` | the field task: `call [0x656A44 + 4 * Game_Mode]` a frame. Mode 2 is `GameMode_Field` (section 3) |
| 1 | `0x496C90` -> `Title_Task` `0x4621C0` | the title sequence (section 2) |
| 2 | `0x495070` | a screen transition, started by `Transition_Start` `0x495040` |

`0x496B60` (task 0, from WinMain) ends by `Task_Create(1, 0x496C90)`;
`0x496C90` loads DAT `0x226`, waits on `File_LoadDone`, and **tail-jumps** to
`0x4621C0` (`E9` at `0x496CB9`). No pointer to `0x4621C0` exists anywhere in
the image (a dword scan of the whole file finds none) - which is why
`pe_funcs.py` never saw it. A detour at its entry catches the jump like a call.

Task 1's private words (`Task_Records` + `0x28`..`0x3F`) are the title's
state; symbols.toml names them `Title_*`:

| Address | Name | PSX | Use |
|---|---|---|---|
| `0x66C7F8` | - | `0x80143C30` | the demo's corner badge on (`0x4624F0`; also written at `0x56B368`..`0x56B57C`) |
| `0x66C7F9` | `Title_Fade` | `0x80143C31` | backdrop fade: 1 in, 2 held, 3 out, 0 off |
| `0x66C7FA` | `Title_LogoState` | `0x80143C32` | 1 fading in, 2 shown |
| `0x66C7FB` | `Title_StartArmed` | `0x80143C33` | Start is looked at |
| `0x66C7FC` | `Title_Timer` | `0x80143C20` | frames left in states 2 and 3 |
| `0x66C7FE` | `Title_Scroll` | `0x80143C22` | the backdrop's scroll, from `0xC8`, +2 a frame |
| `0x66C800` | `Title_Bright` | `0x80143C24` | the backdrop's colour, 0..`0x80` |
| `0x66C802` / `0x66C804` | `Title_LogoShadeA` / `B` | `0x80143C26` / `28` | the logo's two shades |
| `0x66C806` | - | - | `Title_DrawSetA`'s blink counter |
| `0x66C808` | `Title_State` | `0x80143C10` | index into `Title_States` |
| `0x66C80A` | `Title_Step` | `0x80143C12` | zeroed on entry, read nowhere |

## 2. Task 1: the title sequence

`Title_Task` `0x4621C0` (0x3F bytes; PSX GAME.EMI section 1 at
`0x801D0C00`, `FUN_801D0C04`, call for call):

```
Title_State = 0; Title_Step = 0; Task_ClearPrivate()
for (;;) { Task_Sleep(1); Title_CheckStart(); Title_States[Title_State](); Title_DrawBackdrop(); Title_DrawLogo(); }
```

The eight states (`Title_States` `0x653830`, PSX `0x801D1BD4`), read
2026-09-22 and left original - they are pointer-reached, and each is small:

| # | Handler | Does |
|--:|---|---|
| 0 | `0x462200` | `LoadDatFile(0x136)` (PSX loads `0x25F`), wait on `File_LoadDone`, `Music_Play(0x8D, 8)`; Fade 1, Scroll `0xC8`, the rest 0, StartArmed 1; next |
| 1 | `0x462290` | when Fade is 2: Timer `0x168` (360 frames); next |
| 2 | `0x4622B0` | Timer down; at 0: Fade 3 (backdrop out), LogoState 1 (logo in), Timer `0x384` (900); next |
| 3 | `0x4622E0` | Timer down; at 0: `Transition_Start(0)`, `Title_FadeMusic`; next |
| 4 | `0x462300` | when `MoveScript_WaitWordDA` is 0: clear the title's bytes, stop the music, zero the play clock `0x9040C8..CB`, `0x56D5E0(0x10)`, `Task_Create(0, 0x495800)` - **the demo**; next |
| 5 | `0x462370` | task 0 gone: state 0 (the title again). Start, with `File_LoadDone` and the wait word 0: `MoveScript_Var7 = 1`, `Transition_Start(4)`, `Music_FadeOutStop(8)`, next. Tail: `0x4624F0` (the badge) |
| 6 | `0x4623D0` | task 0 gone: `0x90412C = 0xFF`, state 0; else `0x4624F0` |
| 7 | `0x4623F0` | when the wait word is 0: stop the music, `Task_Create(0, 0x588E70)` (the title menu), end this task (`jmp 0x5A99AD`) |

So the attract cycle is states 0-5 round and round: backdrop, logo, demo,
backdrop. The title menu is state 7, reached only through Start.

Since taken over, with the badge, the logo sets and the scenario start: [`title-states.md`](title-states.md).

### 2.1 `Title_CheckStart` `0x462420`

0x8E bytes; PSX `FUN_801D104C` branch for branch. Returns unless
StartArmed, Start (`Input_Pressed & 0x800`) newly pressed, `File_LoadDone`,
the wait word 0 and task 0 free - in that order. Then: at state 0..2, cut the
hold short (Timer 1, state 2: the next frame's state-2 handler ends it); at
state 3 or later with the backdrop's fade done and the logo shown,
`Transition_Start(0)`, `Title_FadeMusic`, `Sound_PlayEffect(0x105)`,
StartArmed 0, state 7. During the demo task 0 is running, so this is a no-op
and state 5 reads Start itself. The original leaves `Transition_Start`'s
pushed 0 on the stack under `Sound_PlayEffect`'s argument (one `add esp, 8`
later); `Sound_PlayEffect` reads `[esp + 4]` only, so that is unobservable.

### 2.2 `Title_Sprite` `0x462560`

0xA0 bytes; PSX `FUN_801D1760`. `(x, y, index, slot, semi)`: a SPRT at
`Gfx_PacketNext` - `Gpu_SetSprt`, `Gpu_SetSemiTrans(semi & 0xFF)`, rgb `0x80`,
x and y as s16 converted to floats (the PC primitive's), entry `index & 0xFF`
of `Title_Sprites` `0x653850` (10 bytes: u, -, v, -, w, h, clut; the clut
shifted left 6), `Gfx_CommitPrim(slot, 0x1C)`; returns the primitive as it
was on entry. The PSX writes s16 positions into a `0x14`-byte SPRT; the
table is the same shape (`0x801D1BF4`). The original stores the converted x
and y back over its own argument slots; all five callers only pop them
(read 2026-09-22), so that is dead.

### 2.3 `Title_DrawBackdrop` `0x462600`

0x13B bytes; PSX `FUN_801D1880`. While `Title_Fade` is set: scroll += 2;
four 255-wide columns from texture pages 5, 7, 9, 11 (with bit `0x80`) at
x = `0x140 - scroll + 255 i`, y `0x18`, sprites `0xB..0xE`, a column skipped
while s16 scroll / 640 > i. The PSX asks a video-mode function (`0x8017BC2C`)
whether to use page bit `0x200` or `0x80`; the port always takes `0x80`.

Quirks kept, each a negative control (section 5):

- the fade steps **once per column drawn**, not once a frame - four steps a
  frame while all four columns show (so the fade-in takes 32 frames, not
  128). Same on the PSX: its step is inside the column loop too;
- fade 1 stops at `0x80` by an s16 compare, stores `0x80` and fade 2 but
  still draws that column;
- fade 3 ends at 0 or below (s16), stores 0 and draws nothing - and every
  later column that frame takes the "held" branch, **storing `0x80` into the
  brightness just zeroed** before it sees fade 0 and skips. Harmless: state 0
  zeroes the brightness before the next fade-in. The PSX does the same;
- brightness and fade are re-read from memory after each column drawn, and
  the column's colour is the stored word's low byte.

### 2.4 `Title_DrawLogo` `0x462740`

0xDA bytes; PSX `FUN_801D1A88`. LogoState 0: nothing. 1: shade A += 4 and
B += 2, each clamped at `0x80` (s16) with its flag cleared; both clamped,
state 2. 2: both `0x80`, flags clear. Anything else: flags set, nothing
moves - still drawn. Then `Title_DrawSetA(flag B, shade B)`,
`Title_DrawSetB(0x1A, 0x18, flag B, shade B)`, `Title_DrawSetC(-6, 0x1C,
flag A, shade A)`, each shade read as a byte after the previous set. The
flags are the sets' semi-transparency while fading. The three sets are left
original (`0x462820`, `0x462A00`, `0x462930`, 2,748 calls each in `all_b`);
`Title_DrawSetA` also blinks sprite 7 on the counter `0x66C806` once the
backdrop is gone and the logo is up - the "press Start" prompt, by where it
appears (hypothesis). The original passes the flags as dwords whose upper
bytes are uninitialised stack; each set reads the byte (masked or stored as
a byte), so that is unobservable too.

## 3. The field task's mode 2: `GameMode_Field` `0x4959F0`

The catalogue's 2,139 bytes were `pe_funcs.py` running on through the next
handlers: the function is `0x4959F0..0x495B85` and a nine-entry jump table
`0x495B88..0x495BAB`, 0x1BC bytes in all. PSX GAME.EMI section 0
`FUN_80198378`, case for case.

`0x56D690` (the event script's dispatcher), `0x517200` (the field frame), then
on the byte `Field_Request`:

| Request | Next mode | Also | PSX |
|--:|--:|---|---|
| 1 | 3 (menu) | `Transition_Start(2)`, `Field_WaitTransition(0)`, `Game_Step` 0 | same |
| 2 | 4 | | same |
| 3 | 5 (battle) | zero `0x904AA0..A2` and one byte in each of three records (`0x802D4B`, `0x802E97`, `0x802FE3`, `0x14C` apart), step 0, the dword `0x904134` + 1 | same (`0x801462DC..DE`, `0x80145E97` / `FD7` / `6117`, `0x80145028`) |
| 4 | 6 | | same |
| 5 | 1 (area change) | `0x905BA4 |= 0x40`; unless the track byte `0x904131` is `0xFF` or equals the area's `0x904CD0`, `Music_FadeOut(0x10)`; unless `0x904EE0` is `0xFF`, `Transition_Start(it)`, `Field_WaitTransition(0)`; then the same test again and `Music_FadeOutStop(10)`. The track byte is re-read after each call | the PSX starts the next area's music from a table (`0x8015DEE8`, `0x8015DDF4`) where the port fades |
| 6 | 7 | menu block `0x929F00` = 6, 0, 0, `+0xC` = `0xFF`, step 0 | same (`0x8014864C..`) |
| 7 / 8 | 9 / 10 | step 0, menu block `+0..+2` = 0 | same |
| 9 | 11 | `0x905DA0` = 1, then four zeros | same (`0x80146268..6C`) |
| 0, 10.. | - | nothing | |

The request is read only after the two calls. What modes 4, 6, 7 and 9-11
are is not read here; mode 3 is the menu ([`menu-screens.md`](menu-screens.md)),
5 battle, 1 the area load `0x495900`.

## 4. PSX twins

| PC | Bytes | PSX | Pairing |
|---|--:|---|---|
| `Title_Task` `0x4621C0` | 0x3F | GAME.EMI §1 `0x801D0C04` | read side by side 2026-09-22 (capstone MIPS of the sibling's overlay capture `093aeda4`) |
| `Title_CheckStart` `0x462420` | 0x8E | `0x801D104C` | same |
| `Title_Sprite` `0x462560` | 0xA0 | `0x801D1760` | same |
| `Title_DrawBackdrop` `0x462600` | 0x13B | `0x801D1880` | same |
| `Title_DrawLogo` `0x462740` | 0xDA | `0x801D1A88` | same |
| `GameMode_Field` `0x4959F0` | 0x1BC | GAME.EMI §0 `0x80198378` | `pairs_propagated.json` (callers tier) and the sibling's Ghidra output, read |

`psx_pair.py` paired only `0x462200` of task 1's code (`0x801D0C90`, callers
tier); the rest were found by reading section 1's 1,068 words.

## 5. The fuzz

`BOF3X_SHADOW=mode_tasks`, at start-up: six byte-copies (the field
handler's jump table relocated into its copy), every call out re-aimed at a
recorder (capstone lists, `kClones` in `src/game/mode_tasks.cpp`), and for
the task body the eight entries of `Title_States` swapped for recording
handlers for the duration. The task body never returns; the `Task_Sleep`
recorder ends it after one to six frames with a long jump (a small setjmp of
our own - clang's `__builtin_setjmp` brought the harness's loop registers
back wrong, which showed as rounds naming the wrong function). 24,000 rounds,
4,000 per function; random bytes in every region any of them touches (task
records 0..2 with `Field_Request`, `Game_Mode`, `Game_Step` and the title's
words, `Input_Pressed`, the music, battle, menu and mode-11 bytes), the
boundaries seeded, both sides run from the same state; the regions, two
primitive buffers, the packet cursor, the return and the recorders' log
compared. The recorders disturb what their caller reads again after them
(the state word, fade, brightness, scroll and shades; the request, track and
area bytes), `Gfx_CommitPrim`'s moves the packet cursor as the real one does,
and `Gpu_SetSprt`'s scribbles over the bytes the caller writes after it.

Result (2026-09-22): **0 mismatches**, with a coverage line:

    shadow      mode_tasks self-test: 24000 rounds (4000 per function), 108771 calls to the stand-ins, 0 MISMATCHES; ...
    shadow      mode_tasks coverage: start cut 202, left 229; backdrop faded in 166, out 308 (relit 256); logo done 147,
                other 1632; field modes 1:725 3:277 4:252 5:284 6:281 7:278 9:280 10:272 11:272; task frames 1..6: 612 700 691 659 707 631

**Negative controls**, each planted, built, run (`BOF3X_SELFTEST_ONLY=1`) and
reverted; every one refused with exit 3 - mismatching rounds in brackets:

| | Planted | Refused |
|---|---|--:|
| C1 | start check: state `< 2` for `<= 2` | 66 |
| C2 | start check: task-0 test dropped | 196 |
| C3 | start check: `File_LoadDone` asked after the wait word | 745 |
| C29 | start check: StartArmed not cleared leaving | 229 |
| C4 | sprite: semi unmasked | 2,019 |
| C5 | sprite: returns the cursor after the commit | 4,000 |
| C6 | sprite: clut shifted by 5 | 3,893 |
| C7 | sprite: x not taken as s16 | 4,000 |
| C8 | sprite: rgb stored before `Gpu_SetSprt` | 3,982 |
| C9 | backdrop: unsigned division | 1,408 |
| C10 | backdrop: scroll not re-read per column | 585 |
| C11 | backdrop: no relight after the fade-out ends | 405 |
| C12 | backdrop: fade-in clamp unsigned | 129 |
| C30 | backdrop: fade-out ends at exactly 0 | 293 |
| C13 | backdrop: fade not re-read after a draw | 624 |
| C14 | backdrop: brightness not re-read after a draw | 289 |
| C15 | backdrop: colour from the register | 1,624 |
| C16 | logo: states above 2 draw nothing | 1,632 |
| C17 | logo: clamp unsigned | 223 |
| C18 | logo: set A gets flag A | 366 |
| C19 | logo: sets B and C swapped | 3,189 |
| C20 | field: track not re-read after the first fade | 59 |
| C31 | field: track not re-read after the transition | 69 |
| C21 | field: battle count not counted | 284 |
| C22 | field: request read before the two calls | 1,865 |
| C23 | field: request 9 ignored | 274 |
| C24 | field: menu block `+0xC` not set | 277 |
| C25 | field: the menu's wait runs the field | 277 |
| C26 | task: `Title_Step` not zeroed | 4,000 |
| C27 | task: logo before backdrop | 3,388 |
| C28 | task: state read before the start check | 1,366 |

C20 was first refused in only 3 rounds: the fade's recorder changed the track
at random. It now ends the track or leaves the area's one, and request 5 is
seeded a quarter of the time - 59. (Counts above are from the final build.)

**What the fuzz cannot see:** anything the callees really do - the fuzz
tests each function alone against its copy. The task body's coroutine
behaviour (running on task 1's stack across `Task_Sleep`) is not exercised
at start-up at all; only the game runs it. Two quirks are unobservable and
have no control: `Title_Sprite`'s stores over its own argument slots, and the
stray argument under `Sound_PlayEffect`; likewise the garbage upper bytes of
arguments the callees read as bytes (the logo's flags, the backdrop's x and
sprite index, the area change's transition kind).

## 6. In game: what the attract cycle reaches

From `hidden_b` (a whole attract cycle): `Title_Task` entered once and its
loop every frame of task 1 (16,127), `Title_CheckStart` / `Title_DrawBackdrop`
/ `Title_DrawLogo` 16,127 each, `Title_Sprite` 36,109, `GameMode_Field` 8,891.
But not every branch:

- the attract run presses nothing, so `Title_CheckStart` never passes
  `Input_Pressed` - **the cut and the title-menu exit are not reached**. A
  recipe that presses Start on the title (`tools/recipes/`, any that opens
  the title menu) reaches the exit; one that presses it during the backdrop
  reaches the cut;
- `GameMode_Field`'s area change (request 5) is reached - `Field_WaitTransition`
  is called 9 times from `0x495AD2` - but not the menu (request 1: no call
  from `0x495A25`), battle (3) or the rest. `field_menu.txt` reaches the
  menu; a new game's opening battle (`battle_commands.txt`) should reach 3.

The live batch check (after the merge) should put `Title_Task`,
`Title_CheckStart`, `Title_Sprite`, `Title_DrawBackdrop`, `Title_DrawLogo`,
`GameMode_Field` on its `--original` list. `Title_Task` is a *logic* function
the tracer never armed (hidden); taking it over changes no hash entry of its
own, but the frame hash re-records anyway with the other five.

## 7. Defects and open questions

No defect of the 2001 code was found: every quirk above is Capcom's on both
platforms (the column-counted fade, the relight after fade-out) or
unobservable. The port's own changes against the PSX, all kept: the video-mode
test dropped from the backdrop, a different title DAT (`0x136` against
`0x25F`), and the area change's music faded rather than restarted.

Open: what modes 4, 6, 7, 9, 10 and 11 are; what the corner badge
(`0x4624F0`, sprite 10 at `0xC0, 4`, gated by `0x66C7F8` and
`Draw_PassFlags`) shows in the demo; what `0x56D5E0(0x10)` sets up for it;
the three logo sets and the eight state handlers are still Capcom's.

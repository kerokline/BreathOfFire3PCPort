# What the attract sequence still runs of Capcom's code

**Status:** IN PROGRESS (verified 2026-09-21)

Stage 1 of the order of work ([`STATUS.md`](STATUS.md)) is "replace every
function the attract sequence reaches". This is the list of what is left:
every function the sequence was measured to enter that `symbols.toml` does not
give an `impl`, with its call count, size, how it is reached and who calls it,
grouped by what it is part of. It replaces reading
[`call-trace.md`](call-trace.md) §9's queue as the picture of what remains;
the queue still orders the work.

It also found that **the tracer's entry list is blind to every function
reached only through a pointer** (§3), and measured what the sequence runs of
those.

## 1. How it was counted

- **Reach** is the union of four all-original runs: `all_a` and `all_b`
  ([`call-trace.md`](call-trace.md) §9; 3,072 and 12,813 logic frames), and
  two taken for this document, both `BOF3X_ORIGINAL=*`, windowed, with the
  owner away:
  - `hidden_a` - first-call mode over the 7,294 hidden entries of §3 plus
    `Task_RunAll` (the tracer refuses a list without it); 13,520 logic frames
    by `attract_watch`, a whole attract cycle. **89 reached.**
  - `hidden_b` - all-calls mode over `entries.txt` plus those 89, with each
    known function's size cut at the next hidden start (§3); 16,128 frames by
    the tracer's count of `Task_RunAll` (`attract_watch`, which starts after a
    30-second wait, saw 15,568). **Every count in the tables is from this run.**
- **Not ours** means no `impl` in `symbols.toml` (112 have one, counted with
  `tomllib`). The three `D3d_DrawSprt*` handlers of DIV-0010 are Capcom's code
  with two operands re-aimed; the tracer treats them as owned and leaves them
  unarmed, so they have no count here, and they are listed as not ours.
- **Groups** are address ranges plus the call edges - the source's file
  decomposition survived into the binary ([`SHARED_SOURCE.md`](SHARED_SOURCE.md)),
  so a range is usually one source file or a few neighbours. Group names say
  what the evidence cited in each section supports and no more.
- **Main callers** are the containing function of the three commonest return
  addresses, resolved against known *and* hidden starts; `ours` is the tracer's
  `0xFFFFFFFF`, a call made from inside an owned function
  ([`call-trace.md`](call-trace.md) §7).
- **Bytes** is the distance to the next known or hidden start, so an upper
  bound; for a function that had a hidden one inside it, much less than
  `pc_funcs.json` says (`Window_Task` 2,041 there, 96 here).
- Speed matters for the renderer: 14 draw functions, `0x5A0C40` at 2.2 M calls
  among them, were entered by `hidden_b` and by neither earlier run - the same
  effect as [`call-trace.md`](call-trace.md) §6, where a fast run enters
  branches of the draw a slow one does not. The render group is a floor.

To regenerate, from a fresh `pc_funcs.json` (commands in [`call-trace.md`](call-trace.md) §8
for the runs):

```
python tools/pe_hidden.py scan                     # -> entries_hidden.txt
BOF3X_CALLTRACE=<abs>/entries_hidden.txt attract_run.py --original "*" --minutes 7.5
python tools/pe_hidden.py classify <that run>/bof3x.calltrace.tsv
python tools/pe_hidden.py plus                     # -> entries_plus_hidden.txt
BOF3X_CALLTRACE=<abs>/entries_plus_hidden.txt BOF3X_CALLTRACE_MODE=all \n    attract_run.py --original "*" --minutes 9
python tools/attract_catalog.py <that run>/bof3x.callcounts.tsv --also <all_a>,<all_b>
```

The last writes every table below to `analysis/attract_catalog.md`; the ones
here are its output, unedited.

## 2. The numbers

**541 functions reached and not ours.** Of the 112 ours, 88 were reached by a
run made before they were taken over; the tracer never arms an owned function,
so whether the other 24 are reached is not measured by any of these runs.

| Group | Functions | Of them hidden | Calls in `hidden_b` | § |
|---|--:|--:|--:|---|
| Windows shell | 12 | 1 | 48,423 | 4.1 |
| Task system | 6 | 1 | 87,585 | 4.2 |
| Top-level modes | 31 | 17 | 155,147 | 4.3 |
| Text and windows | 27 | 11 | 162,305 | 4.4 |
| Field objects | 102 | 18 | 908,972 | 4.5 |
| Event script | 52 | 14 | 136,550 | 4.6 |
| Map and draw layers | 24 | 4 | 890,599 | 4.7 |
| Sprite draw | 6 | 0 | 121,826 | 4.8 |
| Miscellaneous game code | 15 | 2 | 4,651,007 | 4.9 |
| Renderer | 45 | 5 | 9,249,636 | 4.10 |
| PSX library layer | 21 | 1 | 665,058 | 4.11 |
| Sound | 23 | 0 | 13,425,759 | 4.12 |
| MP3 decoder | 77 | 15 | 11,342,888 | 4.13 |
| MSVC CRT | 100 | 0 | 5,284,093 | 4.13 |
| **Total** | **541** | **89** | | |

Two groups are not takeover targets in the per-function sense: the **MP3
decoder** is a statically linked third-party library (unidentified) and the
**MSVC CRT** is the compiler's runtime. Both go when their callers are ours -
a decoder we choose, our own C runtime - not one function at a time. That
leaves **364 functions of game code, renderer, library layer and sound glue**
as stage 1's real remainder.

## 3. The blind spot: functions reached only through a pointer

`tools/pe_funcs.py` recovers function entries **from direct call targets
only**. A function the game reaches solely through a pointer - an event-script
opcode handler, a state-machine table, a task entry, a window procedure - has
no `call rel32` aimed at it, so it is folded into whatever function precedes
it. `pc_funcs.json` says `Gfx_UploadLzss` `0x462070` is 939 bytes; the LZSS
decoder ends with `ret` at `0x4621B7`, and after eight bytes of `nop` comes
`0x4621C0`, a task body that loops for ever:

```
0x4621C0  zero the words 0x66C808 / 0x66C80A; call 0x5A99F4
0x4621D3  Task_Sleep(1); call 0x462420
          call [0x653830 + 4 * u16 [0x66C808]]      ; a state table
          call 0x462600; call 0x462740; jmp 0x4621D3
```

and after it `0x462200`, the table's first handler (loads `DAT` `0x136`, waits
for it, starts sound `0x8D`, advances the state word). None of them has an
entry of its own, so the tracer never armed them, and since the owned range of
`Gfx_UploadLzss` is taken from the same wrong size, **their calls into
Capcom's code are logged as calls made by our code**. That is equally true in
both configurations, so the frame hash's original-vs-ours check is sound; it
just could not see them.

**Measured extent** (`python tools/pe_hidden.py scan`): an address inside a
recorded extent, 16-aligned, preceded by `ret` or `jmp` and `nop` / `int3`
padding, the target of no branch inside the extent, and whose first dword is
not itself a `.text` address (44 jump tables dropped that way). **7,294 such
starts in 769 extents.** 7,185 of them have a pointer to them somewhere in the
image, and their first instructions are ordinary prologues
(`mov eax`, `mov ecx`, `sub esp`, `push ecx`, calls into the script helpers).
So **the binary has roughly 10,200 functions, not 2,952** - a figure `STATUS.md`
and `call-trace.md` use as the denominator. The biggest hosts: `0x43E290` holds
148, `0x44B9F0` 123, `0x5197F0` 99, `0x437CC0` 85.

**What the attract sequence runs of them: 89** (`hidden_a`), classified by the
instruction before each first return address:

| Reached as | Count | What it means |
|---|--:|---|
| indirect call | 62 | the first return address follows a `call` through a register or memory - pointer tables in `.data`; eleven tables hold a reached one, by start: `0x653830`, `0x653EDC`, `0x6552BC`, `0x656A44`, `0x65F5DC`, `0x65F654`, `0x660918`, `0x6619E8`, `0x663004`, `0x663290`, `0x663B30` |
| jumped to | 16 | the return address does not follow a `call`: a switch case whose table is in `.text` (inside `MsgBox_Step` and `0x498280`), a state continuation inside `Window_Task`, or a tail jump through a pointer. **Not all of these are functions** |
| task entry | 5 | first return address 0 - started by `Task_Create`: `0x4621C0`, `0x495070`, `0x495800`, `0x496B60`, `0x496C90` |
| OS callback | 6 | return address outside the image: the window procedure `0x4FC6F0`, `Fmv_WndProc` `0x59E570`, four under the renderer's set-up `0x5A5160` / `0x5A60E0` (DirectDraw / Direct3D enumeration, by where they sit) |

A function reached by a tail `jmp` inherits its caller's return address and is
classified as its caller was: `0x4624F0` shows as an indirect call because two
state handlers jump to it.

Three consequences:

- **Four recorded "functions" are never entered at all** - `0x494F00`,
  `0x496AD0`, `0x469AD0`, `0x56ADF0` were armed in every run and never hit;
  what runs inside their extents is the hidden functions after them. Every
  earlier caller attribution to them meant "some code in that range".
- **The WndProc was misidentified.** WinMain stores `lpfnWndProc = 0x4FC6F0`
  (`0x4FCB98`); `0x4FC6A0`, cited as the WndProc in
  [`windowed-mode.md`](windowed-mode.md), [`save-files.md`](save-files.md) and
  `Cfg_Fullscreen`'s evidence, is the 78-byte function before it. The save
  writer's call is at `0x4FC9A1`, inside `0x4FC6F0`. All three are corrected.
  `0x4FC6A0` itself is **the per-frame input latch**: it calls `0x5A9700` and
  keeps held, previous and newly pressed words for two pads at
  `0x7E1BE8`..`0x7E1BF4` (read 2026-09-21, not yet in `symbols.toml`).
- **The fix is upstream.** `pe_funcs.py` should seed entries from pointers
  into `.text` found in data, and from `push imm32` / `mov r, imm32` of an
  address that is preceded by a function end - the rule above, made
  permanent. That regenerates `pc_funcs.json`, `entries.txt` and the owned
  ranges, and so changes the frame hash's content (calls from the hidden
  functions get real callers): **the reference must be re-recorded with it**,
  about five minutes. Not done here.
- **A case `pe_hidden.py` misses (2026-09-21).** A function with an inline
  jump table: `0x593860` is 213 bytes, then its table at `0x593938`, then
  a dispatcher at `0x593950` and the ten or so handlers of the table at
  `0x66A470` - all folded into `0x593860`'s recorded 2,644 bytes. The scan's
  rule wants a `ret` or `jmp` before the padding, and here the table's data
  comes first; past it, the linear sweep is out of step. Seeding from
  pointers in data, as in the item above, would find them - `0x66A470`
  names them. [`sprite-draw-order.md`](sprite-draw-order.md) section 14.

## 4. The catalogue

### 4.1 Windows shell

WinMain `0x4FCB00` and what it calls once (`Cfg_Load`, the drive-root probe
`0x5A72C0`, the set-up `0x4FD110`), the WndProc, the input latch `0x4FC6A0`
and the pad read under it `0x5A9700` (341 bytes, five indirect calls - the
HANDOFF's "library leaf"; a DirectInput poll by its caller and its indirect
calls, hypothesis), and `0x4FD290`,
called once a frame (16,128 - the tracer's frame count, as are the input
latch's and `0x5A9700`'s). None of it is logic the oracle can compare;
it is also the part [`IDEAS.md`](IDEAS.md) I8 and I12 want replaced anyway.

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x4FC6A0` |  | 16,129 | 80 |  | `0x4FCB00` |
| `0x4FC6F0` |  | 27 | 964 | hidden, OS callback | outside exe |
| `0x4FCAC0` |  | 2 | 61 |  | `0x4FCB00` |
| `0x4FCB00` |  | 1 | 1322 |  | `0x5BA057` |
| `0x4FD030` | `Cfg_Load` | 1 | 212 |  | `0x4FCB00` |
| `0x4FD110` |  | 1 | 226 |  | `0x4FCB00` |
| `0x4FD200` |  | 2 | 36 |  | `0x4FD110` |
| `0x4FD290` |  | 16,128 | 80 |  | `0x4FCB00` |
| `0x5A72C0` |  | 1 | 176 |  | `0x4FCB00` |
| `0x5A9700` |  | 16,129 | 341 |  | `0x4FC6A0` |
| `0x5A9880` |  | 1 | 22 |  | `0x4FD030` |
| `0x5A9907` |  | 1 | 13 |  | `0x4FCB00` |

### 4.2 Task system

The four-coroutine scheduler of [`attract-mode.md`](attract-mode.md) §2:
hand-written `esp` swaps, so a takeover here is assembly or it is nothing.
`0x5A98F0` is new - 23 bytes inside `Task_RunAll`'s extent, reached by an
indirect call 58,329 times; by position and count, the point a task yields
back into the scheduler (hypothesis).

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x5A98A0` | `Task_RunAll` | 16,128 (frames) | 80 |  |  |
| `0x5A98F0` |  | 58,329 | 23 | hidden, indirect call | `0x495620`, `0x56D690`, `0x496B60` |
| `0x5A9914` | `Task_Create` | 59 | 53 |  | `0x494F00`, `0x462300`, `0x496B60` |
| `0x5A9949` | `Task_Sleep` | 29,136 | 45 |  | `0x4621C0`, `0x495800`, `0x495620` |
| `0x5A99AD` |  | 57 | 71 |  | `0x495620`, `0x56D690`, `0x496B60` |
| `0x5A99F4` |  | 4 | 46 |  | `0x495800`, `0x4621C0` |

### 4.3 Top-level modes

The layer above the field: the task at `0x4621C0` and its state table
`0x653830` (exactly eight handlers, `0x462200`..`0x4623F0`; the first loads
`DAT` `0x136` and starts a sound, the last starts a task at `0x588E70` that
the attract run never enters; `0x4624F0` is a tail-jump target of two of them), with `0x462420` / `0x462600` /
`0x462740` run every frame around the handler; and a second family of tasks at
`0x495070` / `0x495800`, the latter dispatching through `0x656A44` to
`0x4959F0` - 8,891 calls, the loop that runs the field frame `0x517200`
(4.5). What each state *is* - boot, title, attract, new game - is unread and is
the natural next read: it is where the attract sequence itself is sequenced.

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x4621C0` |  | 2 | 64 | hidden, task entry |  |
| `0x462200` |  | 3 | 144 | hidden, indirect call | `0x4621C0` |
| `0x462290` |  | 96 | 32 | hidden, indirect call | `0x4621C0` |
| `0x4622B0` |  | 1,080 | 48 | hidden, indirect call | `0x4621C0` |
| `0x4622E0` |  | 2,700 | 32 | hidden, indirect call | `0x4621C0` |
| `0x462300` |  | 48 | 112 | hidden, indirect call | `0x4621C0` |
| `0x462370` |  | 12,200 | 96 | hidden, indirect call | `0x4621C0` |
| `0x462420` |  | 16,127 | 142 |  | `0x4621C0` |
| `0x4624B0` |  | 3 | 18 |  | `0x4622E0` |
| `0x4624D0` |  | 3 | 32 |  | `0x462300` |
| `0x4624F0` |  | 12,198 | 107 | hidden, indirect call | `0x4621C0` |
| `0x462560` |  | 36,109 | 160 |  | `0x462820`, `0x4624F0`, `0x462930` |
| `0x462600` |  | 16,127 | 315 |  | `0x4621C0` |
| `0x462740` |  | 16,127 | 218 |  | `0x4621C0` |
| `0x462820` |  | 2,748 | 261 |  | `0x462740` |
| `0x462930` |  | 2,748 | 194 |  | `0x462740` |
| `0x462A00` |  | 2,748 | 109 |  | `0x462740` |
| `0x494030` |  | 12,165 | 47 |  | `0x517200`, `0x517240` |
| `0x495070` |  | 108 | 192 | hidden, task entry |  |
| `0x495130` |  | 28 | 32 | hidden, jumped to | `0x495070` |
| `0x495150` |  | 26 | 32 | hidden, jumped to | `0x495070` |
| `0x495620` |  | 54 | 126 |  | `0x495130`, `0x495150` |
| `0x495750` |  | 864 | 176 |  | `0x495620` |
| `0x495800` |  | 6 | 64 | hidden, task entry |  |
| `0x495840` |  | 3 | 192 | hidden, indirect call | `0x495800` |
| `0x495900` |  | 12 | 240 | hidden, indirect call | `0x495800` |
| `0x4959F0` |  | 8,891 | 2139 | hidden, indirect call | `0x495800` |
| `0x4967F0` |  | 18 | 58 |  | `0x495900`, `0x4959F0` |
| `0x496870` |  | 11,901 | 399 |  | `0x495800` |
| `0x496B60` |  | 2 | 304 | hidden, task entry |  |
| `0x496C90` |  | 2 | 48 | hidden, task entry |  |

### 4.4 Text and windows

The dialogue engine the attract sequence's story boxes run
([`kinship-probe-text-engine.md`](kinship-probe-text-engine.md)): the
`MsgBox_*` family, the two string pens that are not ours (`Text_DrawAt`,
`Text_EmitGlyph`), and `Window_Task` with the states it jumps between. Stage 2
made this the most-changed part of the game; taking it over is what would let
DIV-0005..0017 stop being patches around Capcom's pens.

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x4976D0` | `Msg_OpenScript` | 24 | 50 |  | `0x517E90`, `0x56B990` |
| `0x497770` | `MsgBox_Reset` | 24 | 127 |  | `0x4976D0` |
| `0x4977F0` | `MsgBox_FrameTask` | 2,781 | 76 |  | `0x5954E0`, `0x595A80` |
| `0x497840` | `MsgBox_Step` | 2,613 | 752 |  | `0x4977F0` |
| `0x497B30` |  | 351 | 256 | hidden, jumped to | `0x497840` |
| `0x497F20` |  | 144 | 32 | hidden, jumped to | `0x498470` |
| `0x498470` |  | 2,040 | 48 | hidden, jumped to | `0x497840` |
| `0x4984A0` |  | 1,896 | 52 | hidden, jumped to | `0x498470` |
| `0x4984E0` |  | 2,781 | 64 |  | `0x4977F0` |
| `0x516B30` | `Text_DrawAt` | 38,560 | 53 |  | `0x497840`, `0x56B990`, `0x56B5D0` |
| `0x516D50` | `Text_EmitGlyph` | 85,739 | 283 |  | ours |
| `0x594E00` |  | 15 | 82 |  | `0x56B730`, `0x495840`, `0x56B990` |
| `0x594E60` |  | 12 | 765 |  | `0x495900` |
| `0x595350` |  | 15 | 51 |  | `0x594E00` |
| `0x595390` |  | 12 | 29 |  | `0x594E60` |
| `0x5953B0` |  | 15 | 152 |  | `0x594E00` |
| `0x595450` | `Window_Task` | 14 | 96 |  | `0x595390`, `0x5953B0` |
| `0x5954B0` |  | 2,973 | 48 | hidden, jumped to | `0x59E230` |
| `0x5954E0` |  | 2,973 | 64 | hidden, jumped to | `0x5954B0` |
| `0x595520` |  | 24 | 336 | hidden, jumped to | `0x5954E0` |
| `0x595670` |  | 144 | 432 | hidden, jumped to | `0x5954E0` |
| `0x595820` |  | 2,637 | 64 | hidden, jumped to | `0x5954E0` |
| `0x595860` |  | 144 | 544 | hidden, jumped to | `0x5954E0` |
| `0x595A80` |  | 24 | 32 | hidden, jumped to | `0x5954E0` |
| `0x595C50` |  | 2,637 | 770 |  | `0x595820` |
| `0x595F60` |  | 264 | 190 |  | `0x595670`, `0x595860` |
| `0x596150` |  | 13,449 | 104 |  | `0x595C50`, `0x595F60` |

### 4.5 Field objects

The biggest group: everything fanned out from the field frame `0x517200`
(12,165 calls - one per field frame), through the object update `0x517490`
and its handler table `0x65F604` / `0x65F614`, the second object kind of
`0x494030` ([`HANDOFF.md`](HANDOFF.md), 20 objects of `0x80` bytes; its table
entry at `0x65539C` reaches `0x469E30` in 4.9), the dispatch through `0x6632B0`
at `0x573080`, and the `0x576B50` / `0x576E00` / `0x577760` family with three
entries reached by a jump through a pointer. The `0x579740`..`0x57BA60` run is
entered only a few dozen times, from `0x594E60` (12 calls, from the mode task
`0x495900`; what it sets up is unread), and
`0x5894D0` is the unbounded queue append of [`known-defects.md`](known-defects.md).
The hottest are `0x589770` (74 k), `0x518980` / `0x5197F0` (72 k each),
`0x517BF0` (69 k), `0x588F20` (67 k), `0x518D10` (58 k) and `0x576B50` /
`0x576E00` (58 k).

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x517200` |  | 9,168 | 64 |  | `0x4959F0`, `0x4967F0` |
| `0x517240` |  | 2,997 | 70 | hidden, indirect call | `0x495800` |
| `0x517350` |  | 12,165 | 139 |  | `0x517200`, `0x517240` |
| `0x5173E0` |  | 12,165 | 96 |  | `0x517200`, `0x517240` |
| `0x517490` |  | 12,165 | 432 |  | `0x517200`, `0x517240` |
| `0x517A70` |  | 2,381 | 371 | hidden, indirect call | `0x517490` |
| `0x517BF0` |  | 69,494 | 668 |  | `0x517490`, `0x5192A0`, `0x57B7B0` |
| `0x517E90` |  | 18 | 149 |  | `0x577420` |
| `0x517F30` |  | 2,181 | 176 |  | `0x517490` |
| `0x517FE0` |  | 12,468 | 23 | hidden, indirect call | `0x517490`, `0x57B7B0` |
| `0x518000` |  | 491 | 113 |  | `0x517A70` |
| `0x518080` |  | 827 | 122 |  | `0x518CA0`, `0x5196E0` |
| `0x518100` |  | 827 | 125 |  | `0x518080` |
| `0x518180` |  | 827 | 569 |  | `0x518100` |
| `0x518620` |  | 1,752 | 310 |  | `0x518180` |
| `0x518760` |  | 2,481 | 63 |  | `0x518180` |
| `0x5187C0` |  | 2,872 | 64 |  | `0x517A70` |
| `0x518980` |  | 71,816 | 320 |  | `0x517BF0`, `0x517A70` |
| `0x518AC0` |  | 742 | 49 | hidden, indirect call | `0x517490`, `0x57B7B0` |
| `0x518B00` |  | 83 | 22 |  | `0x517BF0` |
| `0x518CA0` |  | 491 | 106 |  | `0x517A70` |
| `0x518D10` |  | 58,332 | 190 |  | `0x517BF0`, `0x517A70` |
| `0x518DD0` |  | 94 | 68 |  | `0x517A70` |
| `0x5190A0` |  | 663 | 512 |  | `0x518D10` |
| `0x5192A0` |  | 21,189 | 272 | hidden, indirect call | `0x517490` |
| `0x519670` |  | 1,084 | 104 |  | `0x517BF0` |
| `0x5196E0` |  | 905 | 259 |  | `0x517490`, `0x517A70` |
| `0x5197F0` |  | 71,816 | 160 |  | `0x518980` |
| `0x519890` |  | 3 | 96 | hidden, indirect call | `0x56B2B0` |
| `0x573080` |  | 12,165 | 16 |  | `0x517200`, `0x517240` |
| `0x573090` |  | 8,201 | 544 | hidden, indirect call | `0x517200`, `0x517240` |
| `0x5732B0` |  | 6,269 | 112 | hidden, indirect call | `0x517200`, `0x517240` |
| `0x5733B0` |  | 195 | 70 | hidden, indirect call | `0x517240`, `0x517200` |
| `0x573400` |  | 41 | 237 |  | `0x576B50`, `0x577800`, `0x577760` |
| `0x5734F0` |  | 12 | 102 |  | `0x57B4E0`, `0x56B450` |
| `0x576B50` |  | 57,758 | 384 |  | `0x517BF0`, `0x573090` |
| `0x576CD0` |  | 29,807 | 301 | hidden, jumped to | outside exe |
| `0x576E00` |  | 57,758 | 1145 |  | `0x576B50` |
| `0x577280` |  | 27 | 413 |  | `0x576E00` |
| `0x577420` |  | 105 | 480 |  | `0x576B50` |
| `0x577760` |  | 29,781 | 160 |  | `0x576CD0` |
| `0x577800` |  | 6 | 848 | hidden, jumped to | outside exe |
| `0x577B80` |  | 20,649 | 1165 | hidden, jumped to | outside exe |
| `0x578D10` |  | 6 | 164 |  | `0x577800` |
| `0x578DC0` |  | 6 | 234 |  | `0x5793B0` |
| `0x578EB0` |  | 21,105 | 235 |  | `0x5192A0`, `0x578DC0` |
| `0x5792A0` |  | 12 | 232 |  | `0x577800` |
| `0x579390` |  | 6 | 29 |  | `0x5792A0` |
| `0x5793B0` |  | 6 | 146 |  | `0x5792A0` |
| `0x579450` |  | 2,448 | 120 |  | `0x576E00` |
| `0x579740` |  | 12 | 120 |  | `0x594E60` |
| `0x5797C0` |  | 12 | 54 |  | `0x579740` |
| `0x579800` |  | 24 | 136 |  | `0x579BA0`, `0x5797C0` |
| `0x579890` |  | 3 | 187 |  | `0x579800` |
| `0x579950` |  | 3 | 187 |  | `0x579800` |
| `0x579A10` |  | 42 | 62 |  | `0x579950`, `0x579890` |
| `0x579A50` |  | 18 | 72 |  | `0x579950`, `0x579890` |
| `0x579AA0` |  | 27 | 91 |  | `0x579C20`, `0x579A50` |
| `0x579B00` |  | 18 | 156 |  | `0x579800` |
| `0x579BA0` |  | 18 | 121 |  | `0x579B00` |
| `0x579C20` |  | 27 | 200 |  | `0x579B00` |
| `0x579DB0` |  | 84 | 122 |  | `0x57A5E0`, `0x57B130`, `0x579F30` |
| `0x579E30` |  | 84 | 197 |  | `0x57A5E0`, `0x57B130`, `0x579F30` |
| `0x579F30` |  | 93 | 674 |  | `0x579BA0`, `0x579A10` |
| `0x57A1E0` |  | 9 | 442 |  | `0x579F30` |
| `0x57A5E0` |  | 36 | 478 |  | `0x579F30` |
| `0x57B100` |  | 21 | 39 |  | `0x57B130` |
| `0x57B130` |  | 21 | 476 |  | `0x579F30` |
| `0x57B4E0` |  | 9 | 26 |  | `0x579F30` |
| `0x57B780` |  | 12,165 | 45 |  | `0x517200`, `0x517240` |
| `0x57B7B0` |  | 12,165 | 123 |  | `0x517200`, `0x517240` |
| `0x57B830` |  | 20,793 | 554 |  | `0x57B780` |
| `0x57BA60` |  | 21 | 120 |  | `0x57B130` |
| `0x57C0A0` |  | 42,936 | 74 |  | `0x589770`, `0x5192A0` |
| `0x57C0F0` |  | 9 | 31 |  | `0x56B3A0`, `0x56B450`, `0x56B400` |
| `0x57C110` |  | 12 | 37 |  | `0x594E60` |
| `0x57C140` |  | 20 | 32 |  | `0x57C1C0`, `0x56B400`, `0x56B3A0` |
| `0x57C180` |  | 24 | 32 | hidden, indirect call | `0x579B00` |
| `0x57C1C0` |  | 6 | 48 | hidden, indirect call | `0x579950`, `0x579890` |
| `0x57C210` |  | 9 | 32 | hidden, indirect call | `0x579B00` |
| `0x57C310` |  | 2,448 | 240 |  | `0x576E00` |
| `0x57C460` |  | 33 | 32 | hidden, indirect call | `0x576E00` |
| `0x57C480` |  | 27,757 | 32 | hidden, indirect call | `0x576E00` |
| `0x57C4A0` |  | 39 | 31 | hidden, indirect call | `0x576E00` |
| `0x57C4C0` |  | 358 | 132 |  | `0x517490`, `0x579CF0` |
| `0x57C7C0` |  | 6 | 20 |  | `0x56B5D0`, `0x56B400` |
| `0x57C810` |  | 15 | 45 |  | `0x56B5D0`, `0x56B3A0`, `0x56B340` |
| `0x57C840` |  | 172 | 82 |  | `0x576B50`, `0x577760`, `0x577800` |
| `0x57CDC0` |  | 2,808 | 78 |  | `0x577800` |
| `0x588F20` |  | 67,101 | 448 |  | `0x5173E0` |
| `0x5891F0` |  | 705 | 16 |  | `0x576E00`, `0x57C4C0`, `0x589330` |
| `0x589200` |  | 793 | 303 |  | `0x5891F0`, `0x57C4C0`, `0x576E00` |
| `0x589330` |  | 12 | 30 |  | `0x5305B0` |
| `0x5894D0` | `Sprite_SetFrameQueueUpload` | 12 | 189 |  | `0x589200` |
| `0x589590` |  | 63 | 206 |  | `0x57A5E0`, `0x579F30`, `0x57A1E0` |
| `0x589770` |  | 74,397 | 152 |  | `0x588F20`, `0x57B830` |
| `0x589810` |  | 15 | 44 |  | `0x56B990`, `0x56B450`, `0x56B730` |
| `0x589840` |  | 12 | 47 |  | `0x469E30` |
| `0x589870` |  | 300 | 45 |  | `0x5898A0` |
| `0x5898A0` |  | 15 | 35 |  | `0x594E60`, `0x462300` |
| `0x592F00` |  | 12,165 | 28 |  | `0x4959F0`, `0x495800`, `0x4967F0` |
| `0x592F20` |  | 12,165 | 254 |  | `0x592F00` |

### 4.6 Event script

The dispatcher `0x56D690` (`call [[0x662C80 + s8 [0x8034E0] * 4]]`, then a tail
jump) and the handlers it reaches - `0x6619E8` is a table of them (`0x56B2A0`,
`0x56B2B0`, `0x56B340`, `0x56B560`, `0x56B570`, `0x56B5D0` ... with
`0x437CC0`, the bare `ret`, and a null in its empty slots), and `0x662CC0`
points at it and at three sibling tables - eight of the handlers hidden,
`0x56B990` at 1,807 bytes the largest - plus the `0x52D8F0`..`0x536AC0` block
the area set-up and the field frame call into. **This is where the PSX side
should be able to help most**: an opcode handler table has a PSX twin with the
same order (§5).

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x52D8F0` |  | 12,177 | 48 |  | `0x517350`, `0x5334A0` |
| `0x52D920` |  | 12 | 304 | hidden, indirect call | `0x5334A0` |
| `0x52DA50` |  | 12,165 | 32 | hidden, indirect call | `0x517350` |
| `0x52DA70` |  | 12,165 | 280 | hidden, indirect call | `0x52DA50` |
| `0x52FEB0` |  | 12 | 288 |  | `0x495900` |
| `0x52FFD0` |  | 12 | 82 |  | `0x52FEB0` |
| `0x5305B0` |  | 12 | 67 |  | `0x52D920` |
| `0x531B60` |  | 12,165 | 75 |  | `0x517200`, `0x517240` |
| `0x531BB0` |  | 12 | 32 |  | `0x533110` |
| `0x531CF0` |  | 1,840 | 184 |  | `0x519670`, `0x518080` |
| `0x531DB0` |  | 3,664 | 59 |  | `0x531CF0`, `0x531F10` |
| `0x531F10` |  | 1,824 | 113 |  | `0x519670`, `0x518080` |
| `0x5322B0` |  | 12 | 32 |  | `0x5734F0` |
| `0x533110` |  | 12 | 904 |  | `0x594E60` |
| `0x5334A0` |  | 12 | 211 |  | `0x594E60` |
| `0x533580` |  | 12 | 258 |  | `0x533110` |
| `0x533760` |  | 12,165 | 32 |  | `0x517350` |
| `0x533BA0` |  | 18 | 308 |  | `0x533CE0`, `0x533EF0` |
| `0x533CE0` |  | 15 | 288 |  | `0x533110`, `0x495840` |
| `0x533EF0` |  | 3 | 282 |  | `0x519890` |
| `0x534010` |  | 3 | 18 |  | `0x533EF0` |
| `0x5341A0` |  | 3 | 27 |  | `0x56B2B0` |
| `0x534EC0` |  | 15 | 75 |  | `0x533CE0` |
| `0x536650` |  | 12 | 30 |  | `0x52D920` |
| `0x5366A0` |  | 18 | 85 |  | `0x533BA0` |
| `0x536730` |  | 78 | 48 |  | `0x536760`, `0x5367A0`, `0x533EF0` |
| `0x536760` |  | 15 | 51 |  | `0x533CE0` |
| `0x5367A0` |  | 12 | 53 |  | `0x5334A0` |
| `0x5367E0` |  | 3 | 108 |  | `0x519890` |
| `0x536850` |  | 3 | 56 |  | `0x5367E0` |
| `0x536890` |  | 3 | 84 |  | `0x5367E0` |
| `0x5368F0` |  | 6 | 363 |  | `0x536890`, `0x536850` |
| `0x536AC0` |  | 2 | 149 |  | `0x536890`, `0x536850` |
| `0x56B2A0` |  | 11,903 | 16 | hidden, indirect call | `0x56D690` |
| `0x56B2B0` |  | 3 | 144 | hidden, indirect call | `0x56D690` |
| `0x56B340` |  | 12 | 87 | hidden, indirect call | `0x56D690` |
| `0x56B3A0` |  | 3 | 90 |  | `0x56B340` |
| `0x56B400` |  | 6 | 76 |  | `0x56B340` |
| `0x56B450` |  | 3 | 272 |  | `0x56B340` |
| `0x56B560` |  | 11,888 | 16 | hidden, indirect call | `0x56D690` |
| `0x56B570` |  | 2 | 96 | hidden, indirect call | `0x56D690` |
| `0x56B5D0` |  | 6,177 | 352 | hidden, indirect call | `0x56D690` |
| `0x56B730` |  | 1,923 | 608 | hidden, indirect call | `0x56D690` |
| `0x56B990` |  | 48 | 1807 | hidden, indirect call | `0x56D690` |
| `0x56C0A0` |  | 375 | 104 |  | `0x56B990` |
| `0x56C110` |  | 14 | 32 |  | `0x56B990`, `0x56B570` |
| `0x56D5E0` |  | 3 | 130 |  | `0x462300` |
| `0x56D670` |  | 3 | 20 |  | `0x56D5E0` |
| `0x56D690` |  | 11,903 | 21 |  | `0x4959F0`, `0x517240`, `0x594E60` |
| `0x56D8B0` |  | 11,901 | 16 | hidden, indirect call | `0x4959F0`, `0x517240`, `0x594E60` |
| `0x56D8C0` |  | 1 | 96 | hidden, indirect call | `0x462300` |
| `0x56D920` |  | 11,900 | 16 | hidden, indirect call | `0x4959F0`, `0x517240`, `0x594E60` |

### 4.7 Map and draw layers

`DrawLayer_Open` and the map-record handlers it reaches through `0x663008` -
a table of 78 code pointers that starts one slot earlier, at `0x663004`, 69 of
them hidden starts (three reached here: `0x570020`, `0x570660`, `0x571500`), the view set-up
`0x56E6C0` / `0x56EC00`, and `0x572A00` - 123,820 calls, 1,225 bytes, 43 x87
instructions, the heaviest logic function left
([`psx-library-layer.md`](psx-library-layer.md) §3 is the recipe).

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x56E6C0` |  | 12,165 | 730 |  | `0x517200`, `0x517240` |
| `0x56E9A0` |  | 196 | 131 |  | `0x56E6C0` |
| `0x56EA30` |  | 66 | 113 |  | `0x56E6C0` |
| `0x56EAB0` |  | 164 | 157 |  | `0x56E6C0` |
| `0x56EB50` |  | 117 | 161 |  | `0x56E6C0` |
| `0x56EC00` |  | 6,839 | 2465 |  | `0x56E6C0` |
| `0x56F670` |  | 12 | 657 |  | `0x594E60` |
| `0x56F9B0` |  | 28,357 | 283 |  | `0x56EC00` |
| `0x56FAD0` |  | 12 | 248 |  | `0x56F670` |
| `0x56FD20` | `DrawLayer_Open` | 567,490 | 345 |  | ours |
| `0x56FF00` |  | 41,884 | 288 |  | `0x571B40`, `0x570020` |
| `0x570020` |  | 12,183 | 496 | hidden, indirect call | `0x56FD20` |
| `0x570660` |  | 5,098 | 528 | hidden, indirect call | `0x56FD20` |
| `0x571500` |  | 33,550 | 543 | hidden, indirect call | `0x56FD20` |
| `0x571720` |  | 12 | 131 |  | `0x56F670` |
| `0x5717B0` |  | 12 | 208 |  | `0x571720` |
| `0x571AF0` |  | 12,165 | 80 |  | `0x592F00` |
| `0x571B40` |  | 29,701 | 160 | hidden, indirect call | `0x571AF0` |
| `0x571FF0` |  | 543 | 206 |  | `0x56E6C0`, `0x56F670` |
| `0x5720C0` |  | 11,553 | 523 |  | `0x570660`, `0x531DB0`, `0x518980` |
| `0x5722D0` |  | 4,612 | 672 |  | `0x518760`, `0x578B00` |
| `0x572570` |  | 24 | 31 |  | `0x52D920`, `0x533580` |
| `0x572590` |  | 24 | 46 |  | `0x572570` |
| `0x572A00` |  | 123,820 | 1225 |  | `0x570660`, `0x571500`, `0x56F9B0` |

### 4.8 Sprite draw

`Sprite_AddDrawRecords` and `Sprite_Draw` with what hangs under them - the
three callees of `Sprite_DrawPass` that are not ours
([`sprite-draw-order.md`](sprite-draw-order.md) §11). `0x57C070` is ours
since 2026-09-21, with the matrix product (4.11).

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x57BAE0` | `Sprite_AddDrawRecords`, **ours** 2026-09-21 | 10,812 | 993 |  | ours |
| `0x57BED0` | `Light_ObjectDirection`, **ours** | 10,812 | 285 |  | `0x57BAE0` |
| `0x57BFF0` | `Sprite_ObjectMatrix`, **ours** | 10,812 | 128 |  | `0x57BAE0` |
| `0x57C070` | `Camera_LoadMatrix`, **ours** 2026-09-21 | 10,812 | 34 |  | `0x57BAE0` |
| `0x5935B0` | `Sprite_Draw` | 45,858 | 683 |  | ours |
| `0x593860` |  | 32,720 | 256 |  | `0x5935B0` |

### 4.9 Miscellaneous game code

`0x437CC0` is a bare `ret` used as a null handler - 4.6 million calls, mostly
from the renderer's primitive dispatch; `0x4DF820` is another (the empty
`Menu_DrawFrame` of DIV-0011) and `0x454810` is `return 1`. The rest are small
helpers of the modes and the field, and `0x469E30`, the object kind of
`0x494030`.

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x437CC0` |  | 4,611,067 | 16 |  | `0x5A0C40`, `0x5A2900`, ours |
| `0x454810` |  | 12,224 | 6 |  | `0x462370`, `0x594E60`, `0x5367E0` |
| `0x4549B0` |  | 13 | 52 |  | `0x594E60`, `0x462200`, `0x496B60` |
| `0x454A20` |  | 13 | 37 |  | `0x594E60`, `0x496B60` |
| `0x454A50` |  | 8 | 33 |  | `0x454AB0` |
| `0x454AB0` |  | 1 | 20 |  | `0x496B60` |
| `0x454AD0` |  | 12,165 | 491 |  | `0x517200`, `0x517240` |
| `0x454DC0` |  | 18 | 43 |  | `0x533BA0` |
| `0x455250` |  | 12,165 | 50 |  | `0x517200`, `0x517240` |
| `0x461E10` |  | 1 | 63 |  | `0x496B60` |
| `0x469E30` |  | 1,662 | 26 | hidden, indirect call | `0x494030` |
| `0x469E50` |  | 15 | 153 |  | `0x469E30` |
| `0x469EF0` |  | 1,635 | 128 |  | `0x469E30` |
| `0x469F70` |  | 12 | 64 | hidden, indirect call | `0x469E30` |
| `0x4DF820` |  | 8 | 1 |  | `0x462300`, `0x462200`, `0x4FD110` |

### 4.10 Renderer

The Direct3D end ([`asset-loading-path.md`](asset-loading-path.md) §2,
[`IDEAS.md`](IDEAS.md) I8): the per-frame draw `0x59EE50`, the primitive
dispatch under it, the texture-cache builders, the 3.7 KB set-up `0x5A5160` and
its enumeration callbacks. Its product is a surface, not memory, so how a
takeover here is checked is still the open question
([`IDEAS.md`](IDEAS.md) I14). A floor - see §1.

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x59E230` |  | 2,997 | 148 |  | `0x517240` |
| `0x59E2D0` | `Window_Alloc` | 24 | 64 |  | `0x497770` |
| `0x59E310` |  | 24 | 29 |  | `0x5954E0` |
| `0x59E330` |  | 4 | 38 |  | `0x495800`, `0x496B60` |
| `0x59E360` | `Fmv_Play` | 2 | 387 |  | `0x4FCB00` |
| `0x59E570` | `Fmv_WndProc` | 7 | 212 | hidden, OS callback | outside exe |
| `0x59ECE0` |  | 9,465 | 263 |  | `0x59EDF0` |
| `0x59EDF0` |  | 9,466 | 96 |  | `0x5A7860` |
| `0x59EE50` |  | 10,860 | 208 |  | `0x4FCB00` |
| `0x59F840` |  | 10,155 | 31 |  | `0x5A2CA0`, `0x5A0510`, `0x5A37D0` |
| `0x59F860` |  | 1 | 160 |  | `0x5A5160` |
| `0x59F900` |  | 224 | 171 |  | `0x5A0080`, `0x5A32B0`, `0x5A2CA0` |
| `0x59F9B0` |  | 94 | 154 |  | `0x5A32B0` |
| `0x59FBA0` |  | 2,310,145 | 249 |  | `0x5A0C40`, `0x5A2900`, ours |
| `0x59FCA0` |  | 2,305,069 | 260 |  | `0x5A0C40`, `0x5A2900`, ours |
| `0x59FFE0` |  | 2,213,770 | 151 |  | `0x5A0C40`, ours, `0x5A14C0` |
| `0x5A0080` |  | 107 | 1076 |  | `0x59FFE0` |
| `0x5A0510` |  | 3,482 | 785 |  | `0x59FFE0` |
| `0x5A0C40` |  | 2,183,660 | 573 |  | `0x59EE50` |
| `0x5A14C0` |  | 1,692 | 724 |  | `0x59EE50` |
| `0x5A17A0` |  | 8,662 | 271 |  | `0x59EE50` |
| `0x5A1D10` |  | 1,894 | 389 |  | `0x59EE50` |
| `0x5A20D0` |  | 2,320 | 330 |  | `0x59EE50` |
| `0x5A2300` | `D3d_DrawSprt` (DIV-0010 copy) | unarmed | 529 |  |  |
| `0x5A2900` |  | 60,707 | 692 |  | `0x59EE50` |
| `0x5A2BC0` |  | 60,707 | 215 |  | `0x5A2900` |
| `0x5A2CA0` |  | 6,005 | 457 |  | `0x5A2BC0` |
| `0x5A2E70` |  | 336 | 60 |  | `0x5A37D0`, `0x5A32B0`, `0x5A5160` |
| `0x5A2EB0` |  | 17,716 | 682 |  | `0x59EE50` |
| `0x5A3160` |  | 17,716 | 324 |  | `0x5A2EB0` |
| `0x5A32B0` |  | 94 | 1243 |  | `0x5A3160` |
| `0x5A3790` |  | 94 | 62 |  | `0x5A32B0` |
| `0x5A37D0` |  | 241 | 650 |  | `0x5A3160` |
| `0x5A5130` |  | 1 | 46 |  | `0x4FC6F0` |
| `0x5A5160` |  | 1 | 2656 |  | `0x4FCB00` |
| `0x5A5BC0` |  | 1 | 640 | hidden, OS callback | outside exe |
| `0x5A5E40` |  | 541 | 96 | hidden, indirect call | `0x5B9B80`, `0x5B9CD4` |
| `0x5A5EA0` |  | 75 | 240 | hidden, OS callback | outside exe |
| `0x5A5F90` |  | 2 | 90 | hidden, OS callback | outside exe |
| `0x5A5FF0` |  | 1 | 82 |  | `0x5A5160` |
| `0x5A9A59` |  | 2,242 | 417 |  | `0x5A0510`, `0x5A0080` |
| `0x5A9BFA` |  | 1,347 | 309 |  | `0x5A0510`, `0x5A0080` |
| `0x5A9E1E` |  | 6,005 | 544 |  | `0x5A2CA0` |
| `0x5AA1E0` |  | 1,570 | 278 |  | `0x5A37D0`, `0x5A32B0` |
| `0x5AA4AE` |  | 110 | 296 |  | `0x5A37D0`, `0x5A32B0` |

### 4.11 PSX library layer

What is left of [`psx-library-layer.md`](psx-library-layer.md): the matrix
product `0x5A7D70` and the four rotations on it - **taken over 2026-09-21**,
with zeros in the padding bytes (the owner's call, DIV-0021), and marked in
the table rather than removed so the counts above still add up - the
draw-record append `0x5A6790` and its
reset `0x5A6780`, three primitive setters `0x5A7630` / `0x5A7690` / `0x5A7740`
used by the window code, and one-off set-up.

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x5A6050` |  | 51 | 136 |  | `0x5A5EA0`, `0x5A5BC0` |
| `0x5A60E0` |  | 1 | 336 |  | `0x5A5160` |
| `0x5A6230` |  | 23 | 129 | hidden, OS callback | outside exe |
| `0x5A62C0` |  | 5 | 186 |  | `0x5A6230`, `0x5A60E0` |
| `0x5A6780` |  | 16,129 | 11 |  | `0x4FCB00` |
| `0x5A6790` |  | 146,892 | 97 |  | `0x5935B0` |
| `0x5A6830` |  | 1 | 325 |  | `0x4FD110` |
| `0x5A7630` |  | 2,637 | 26 |  | `0x595C50` |
| `0x5A7690` |  | 2,901 | 26 |  | `0x595C50`, `0x595F60` |
| `0x5A7740` |  | 3,501 | 16 |  | `0x595C50`, `0x495750` |
| `0x5A7860` |  | 10,860 | 38 |  | `0x4FCB00` |
| `0x5A7890` |  | 10,860 | 77 |  | `0x4FCB00` |
| `0x5A78E0` |  | 2 | 40 |  | `0x4FD110` |
| `0x5A7910` |  | 2 | 74 |  | `0x4FD110` |
| `0x5A7A70` |  | 438 | 27 |  | `0x5190A0` |
| `0x5A7D70` | `Gte_MulMatrix0`, **ours** 2026-09-21 | 207,930 | 403 |  | `0x5A7F80`, `0x5A7F10`, `0x5A7FF0` |
| `0x5A7F10` | `Gte_RotMatrixX`, **ours** | 65,706 | 103 |  | `0x5A8060` |
| `0x5A7F80` | `Gte_RotMatrixY`, **ours** | 65,706 | 101 |  | `0x5A8060` |
| `0x5A7FF0` | `Gte_RotMatrixZ`, **ours** | 65,706 | 103 |  | `0x5A8060` |
| `0x5A8060` | `Gte_RotMatrix`, **ours** | 65,706 | 66 |  | `0x57BED0`, `0x578EB0`, `0x56E6C0` |
| `0x5A94C0` |  | 1 | 352 |  | `0x4FD110` |

### 4.12 Sound

The glue between the game's sound calls and DirectSound: `Snd_LoadBank`'s
callees, the music start `0x5A6CC0`, the decode-and-fill `0x5A6F30`, and the
pump `0x587C70` / `0x5A7230` - 6.7 million calls each, because WinMain spins
on it while waiting out the frame, so its count measures idle time, not work
([`call-trace.md`](call-trace.md) §4, §6).

| Entry | Name | Calls | Bytes | Found as | Main callers |
|---|---|--:|--:|---|---|
| `0x587740` |  | 148 | 281 |  | `0x577800`, `0x577280`, `0x56B570` |
| `0x587A20` |  | 9 | 184 |  | `0x587AE0`, `0x56B2B0` |
| `0x587AE0` |  | 18 | 96 |  | `0x594E60`, `0x56B990`, `0x56B5D0` |
| `0x587B40` |  | 8 | 56 |  | `0x4624D0`, `0x56B730`, `0x56B990` |
| `0x587BA0` |  | 12 | 54 |  | `0x587AE0` |
| `0x587BE0` |  | 3 | 50 |  | `0x4624B0` |
| `0x587C70` |  | 6,710,895 | 81 |  | `0x4FCB00` |
| `0x587CD0` | `Snd_LoadBank` | 14 | 224 |  | ours |
| `0x5A69C0` |  | 176 | 26 |  | `0x587CD0` |
| `0x5A69E0` |  | 176 | 188 |  | `0x5A69C0` |
| `0x5A6AA0` |  | 176 | 68 |  | `0x5A69E0` |
| `0x5A6AF0` |  | 1,328 | 187 |  | `0x5A7230`, `0x5A69E0`, `0x5A6E60` |
| `0x5A6BB0` |  | 12 | 117 |  | `0x587740` |
| `0x5A6C90` |  | 153 | 41 |  | `0x587CD0` |
| `0x5A6CC0` |  | 12 | 159 |  | `0x587AE0` |
| `0x5A6D60` |  | 12 | 129 |  | `0x5A6CC0` |
| `0x5A6DF0` |  | 12 | 112 |  | `0x5A6CC0` |
| `0x5A6E60` |  | 12 | 204 |  | `0x5A6CC0` |
| `0x5A6F30` |  | 1,152 | 121 |  | `0x5A7230`, `0x5A6E60` |
| `0x5A6FB0` |  | 514 | 64 |  | `0x587C70`, `0x587AE0` |
| `0x5A7050` |  | 11 | 48 |  | `0x587C70`, `0x5A7230` |
| `0x5A70A0` |  | 11 | 153 |  | `0x5A6CC0` |
| `0x5A7230` |  | 6,710,895 | 132 |  | `0x587C70` |

### 4.13 MP3 decoder and C runtime

Listed for completeness, with call counts; see §2 for why these are not
per-function work. The decoder is `0x5AB000`..`0x5B9380` (fifteen of its
entries hidden, reached through its own function tables); the CRT starts at
`Crt_sprintf` `0x5B9380`.

<details><summary>MP3 decoder, 77 functions</summary>

`0x5ACBB0` (2), `0x5ACBB6` (1), `0x5ACBBC` (1), `0x5ADF00` (12), `0x5AE150` (12), `0x5AE160` (12), `0x5AE360` (12), `0x5AE6A0` (12), `0x5AE790` (23), `0x5AE820` (12), `0x5AE900` (12), `0x5AE920` (12), `0x5AE9C0` (35), `0x5AEA10` (57,043), `0x5AEAC0` (11), `0x5AEB40` (56,997), `0x5AEBA0` (23), `0x5AEC80` (18,438), `0x5AFC40` (18,429), `0x5AFD80` (18,415), `0x5AFF40` (18,415), `0x5B0630` (11), `0x5B08A0` (12), `0x5B08C0` (11), `0x5B08E0` (56,997), `0x5B0910` (23), `0x5B0940` (12), `0x5B0970` (12), `0x5B0D50` (12), `0x5B0DB0` (12), `0x5B0F40` (12), `0x5B1010` (11), `0x5B1050` (12), `0x5B1080` (11), `0x5B1090` (56,997), `0x5B1120` (23), `0x5B1150` (12), `0x5B1160` (12), `0x5B1180` (94), `0x5B11D0` (70), `0x5B1210` (24), `0x5B1290` (22), `0x5B12C0` (1,030,724), `0x5B21C0` (1,326,396), `0x5B2370` (1,325,880), `0x5B23C0` (1,325,880), `0x5B3600` (12), `0x5B3670` (46), `0x5B36A0` (35), `0x5B36C0` (11), `0x5B3730` (7,402), `0x5B3760` (4,053,572), `0x5B37E0` (18,429), `0x5B3820` (18,429), `0x5B3980` (18,415), `0x5B3AF0` (18,415), `0x5B3B30` (37), `0x5B3BE0` (18,415), `0x5B3D60` (35), `0x5B3D70` (12), `0x5B3F50` (18,429), `0x5B4180` (18,436), `0x5B42C0` (18,427), `0x5B4720` (1,325,880), `0x5B4B40` (35), `0x5B4B70` (18,415), `0x5B4BD0` (18,415), `0x5B4C00` (18,415), `0x5B4C70` (18,415), `0x5B4CC0` (18,415), `0x5B4CF0` (18,415), `0x5B5210` (18,415), `0x5B5520` (73,660), `0x5B5560` (73,660), `0x5B6100` (73,660), `0x5B7250` (73,660), `0x5B74B0` (73,660)

</details>

<details><summary>MSVC CRT, 100 functions</summary>

`0x5B9380` Crt_sprintf (546), `0x5B93D2` Rand (40,972), `0x5B940C` (1), `0x5B9550` (5,173,613), `0x5B9577` Crt_free (268), `0x5B9660` Crt_malloc (355), `0x5B9672` (355), `0x5B969E` (355), `0x5B9993` Crt_fclose (25), `0x5B99C4` (25), `0x5B9A10` (2), `0x5B9A9B` (2), `0x5B9AA6` (24), `0x5B9ADA` (3), `0x5B9B3C` (31), `0x5B9B6D` Crt_fopen (31), `0x5B9B80` (2), `0x5B9CD4` (8), `0x5B9D22` (81), `0x5B9D4E` Crt_fread (23), `0x5B9D7D` (23), `0x5BA057` (1), `0x5BA221` (74), `0x5BA360` (18,429), `0x5BA4F3` (546), `0x5BAC69` (561), `0x5BAC9A` (1,122), `0x5BACD2` (561), `0x5BACFD` (1), `0x5BAD51` (1), `0x5BAD64` (40,984), `0x5BADCB` (1), `0x5BAE1B` (1), `0x5BB297` (1), `0x5BB3DF` (1), `0x5BC650` (1), `0x5BC679` (150), `0x5BC6DA` (150), `0x5BC8E0` (109), `0x5BC9D0` (6), `0x5BCA43` (6), `0x5BCA4C` (6), `0x5BCBD3` (51), `0x5BCC02` (31), `0x5BCC25` (82), `0x5BCC77` (25), `0x5BCCD4` (25), `0x5BCD57` (25), `0x5BCDB0` (25), `0x5BCF64` (24), `0x5BD989` (288), `0x5BD9C0` (360), `0x5BD9DA` (48), `0x5BD9F1` (48), `0x5BDA20` (230), `0x5BDA9B` (49), `0x5BDB77` (31), `0x5BDCE7` (31), `0x5BDDAF` (48), `0x5BDE14` (48), `0x5BDFF0` (23), `0x5BE57A` (69), `0x5BE74E` (1), `0x5BE854` (2), `0x5BE9E6` (1), `0x5BEA3E` (1), `0x5BEAF7` (1), `0x5BEB90` (2), `0x5BED44` (1), `0x5BEE76` (1), `0x5BF230` (1), `0x5BF288` (24), `0x5BF3C0` (1,153), `0x5BF430` (1,153), `0x5BF4A5` (1), `0x5BF4DA` (1), `0x5BF4F0` (1), `0x5BF582` (1), `0x5C03B0` (6), `0x5C0A22` (31), `0x5C0B45` (25), `0x5C0BC1` (25), `0x5C0C40` (119), `0x5C0C82` (96), `0x5C0CE1` (127), `0x5C0D96` (1), `0x5C1005` (48), `0x5C104F` (24), `0x5C10BD` (31), `0x5C138C` (65), `0x5C139D` (65), `0x5C13CE` (1), `0x5C157B` (1), `0x5C1621` (1), `0x5C17A6` (2), `0x5C184B` (2), `0x5C1A6F` (2), `0x5C359A` (12), `0x5C35D6` Crt_filelength (23), `0x5C3660` Crt_fileno (23)

</details>

## 5. Where the PSX side lines up

The owner's question, 2026-09-21: can the sibling's overlay and function-entry
work be reused against these entries? **Yes, and the tables of §3 are the
way in.** Both builds kept the same pointer tables, in the same order, so a
PSX table found in a PC one pairs every slot with a PC function - and most of
those PC functions are exactly the hidden starts §3 found. `tools/psx_pair.py`
does both measurements below; it reads the sibling checkout
(`names/area_records.toml`, `analysis/overlay_captures_all.json`,
`disc/SLPS_009.90`) and writes only to `analysis/`.

**The area descriptor table (`psx_pair.py areas`).** The sibling found a
200-entry table at PSX `0x801802EC`, one pointer per area, to a 0x44-byte
descriptor whose `+0x3C` is an array of handlers (called by field-script
opcodes `0x03` and `0xDE`) and `+0x40` an init function
(its `docs/loader_records/AREA.md`). **The PC has it at `0x667590`**: 200
pointers to descriptors with the same two fields. Measured:

- **198 of 200 areas agree** on the handler array's length and on whether an
  init exists. Areas 75 and 86 disagree (PC 13 handlers against 8, and 2
  against none); most likely the PC array walk running into a neighbour, as it
  stops only at a non-code word. Unread.
- That pairs **728 PSX entries with 579 distinct PC functions** - 505 of them
  hidden starts, one a `pc_funcs.json` entry, and 73 that neither list has
  (starts §3's rule misses: unpadded or not 16-aligned).
- **No PSX entry has two PC twins.** 60 PC functions serve several PSX
  entries, all but one at *different* PSX addresses: code the PSX duplicated
  into several area overlays and the PC compiled once.
- **Order survives.** Of the 77 overlays with two or more unshared pairs, 75
  have them in the same order on the PC as on the PSX; the typical span of one
  area's handlers on the PC is about 2 KB.
- **So do the gaps.** Between two neighbouring anchors, the PSX overlay's
  known starts (the sibling's static discovery, header and engine entries) and
  the PC's starts (known, hidden and paired) are **equal in number in 352 of
  400 gaps**. Where they differ the PC nearly always has fewer: starts §3
  misses, or functions the PC build folded.

That is the pattern the owner asked about: **an area overlay sits in the PC exe
as one ordered block, and within a gap of equal count every function pairs by
position.** It carries the sibling's overlay work - its 121 named overlay
functions, and structure for the rest - onto PC addresses wholesale, and does
the same for the other direction.

**Tables by shape (`psx_pair.py tables`).** 489 runs of code pointers in the
PC data, 1,245 in the PSX boot EXE and 406 overlays, compared by shape: each
distinct target numbered by first appearance, nulls and bare returns marked,
whole or as a window of a longer run. **16 PC tables have exactly one PSX
twin with at least two null, return or repeated slots** - `0x64DFE0` (27
slots) with `BATTLE.EMI#3` `0x801EB094`, `0x653EDC` (25) with `GAME.EMI#0`
`0x801C909C`, `0x660D68` (50) with `SCENA01.EMI#0` `0x801FE288`, and thirteen
more (the tool lists them). Shapes that are all distinct match everything of
their length and prove nothing; that is most tables, including the attract
run's `0x653830`.

Not matched yet: the attract run's big tables, `0x663004` (78),
`0x65F654` (163), `0x6552BC` (224) and the script's `0x6619E8` (14), have no
exact PSX twin. The likely reason is the PC build's folding: two PSX functions
that are one on the PC turn distinct slots into a repeat and change the shape.
A tolerant score is the next step - or, simpler, the area block alignment
above, which does not need shapes at all.

### 5.1 Growing the pairs (`psx_pair.py fill`, `propagate`)

Done 2026-09-21, the first of the next steps below. Filling the area overlays'
equal gaps alone adds only 57 pairs - an area's handlers mostly sit next to
each other - so `propagate` grows the pairs to a fixed point with four
methods, each tried only when the ones before it add nothing:

1. **Calls.** A paired function's PSX `jal` list and its twin's PC `call`
   list, when equal in length, pair position by position; refused when any
   position contradicts a pair already made.
2. **Gaps**, in every address space, the boot EXE included - refused when any
   pair in the gap is more than 2x off the typical PC/PSX size ratio (x0.59,
   measured on the area-table pairs). Without that filter the positional
   pairs were about 85% right; a misplaced start shifts everything after it,
   and the size of the pair shows it (below).
3. **Tables.** A PSX pointer table (or window) that agrees with a PC one on
   two slots already paired, contradicts none, and is the only such window,
   pairs every other slot. No shape match needed, so it works where the PC
   build folded functions.
4. **Callers.** An unpaired PSX function whose paired callees - two or more -
   are all called by exactly one PC function pairs with it.

**Result: 3,330 pairs, 433 of them boot EXE functions**, after about a
hundred rounds (five minutes). 63 pairs are left tagged `call-disputed`: an alignment usable
at the end contradicts them; they are kept for the record and excluded from
every use.

**How right they are**, measured without trusting any method on itself:

| Method | Pairs | PSX calls the PC twin also makes | Baseline (twin's PC neighbour) | Calls *into* the pair found |
|---|--:|--:|--:|--:|
| area table (independent, the reference) | 728 | 96.1% | 29.6% | - |
| gap, area overlays | 57 | 89.1% | 29.1% | 94.4% |
| calls | 200 | 95.0% | 19.2% | 97.2% |
| calls, anchored | 451 | 95.1% | 17.4% | 95.0% |
| gaps, later rounds (size-filtered) | 540 | 94.7% | 30.1% | 93.6% |
| tables, anchored | 738 | 90.5% | 28.4% | 80.0% (5 edges) |
| callers | 541 | 97.6% - biased, chosen on these edges | 14.4% | 93.1% |

A true pair does not reach 100%: the PC build inlines and folds. Reading the
columns against the reference, the call and gap pairs are at the reference's
level, the anchored tables and the callers a few points under it. Leave-one-out
against the independent pairs, the call alignments are **262 right and 0
wrong**; the nine `psx` fields `symbols.toml` already had all agree.

**Coverage.** 70 of the sibling's 556 named boot functions and 46 of its 121
named overlay functions now have a PC twin; of the attract catalogue's 544
entries, 180 have a PSX twin and 24 a sibling name. The named boot functions
are thin because the sibling named what it studied - the text engine, battle,
files - and the text path is exactly where the port was rewritten.

**What does not pair is information too** (owner, 2026-09-21: the port
dropped the naming and options screens at start-up and redid text
rendering). Every method needs positive evidence, so a rewritten function
fails to pair rather than pairing wrongly - except through the callers method,
the one most able to pick a PC rewrite that calls the same helpers. Once the
pairs stop growing, **unpaired runs inside paired neighbourhoods are a map of
where the port diverged**; not drawn yet.

**Next, in order:** (1) draw that map - unpaired PSX runs between paired
neighbours, and PC functions with no PSX twin in paired blocks; (2) the same for SCENARIO (the
sibling's 20-entry `0x801C944C`, fully proven there) and the other loader
record kinds; (3) import `names/functions.toml`'s overlay names through the
pairs, as `hypothesis` in `symbols.toml` until a PC-side read confirms each
(the BSim lesson, [`bsim-evaluation.md`](bsim-evaluation.md)); (4) feed the
paired starts back into `pe_funcs.py` (§3), which the 73 missed starts argue
for anyway.

## 6. What this does not cover

- Anything the attract sequence does not reach: battle, menus, save and load,
  most of the event script's opcodes.
- Hidden functions that do not fit the rule of §3 - one without padding before
  it, or not 16-aligned - are still invisible; so is one only ever reached
  through a jump table in `.text`.
- The 16 "jumped to" entries are counted as functions but some are case
  labels; each needs reading before it is named.
- No row here is read beyond what its section cites. The grouping is a map
  for reading, not a claim about what any function does.

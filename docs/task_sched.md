# The task scheduler: `Task_RunAll` and its hand-written unit

**Status:** IN PROGRESS (2026-09-25) - eight entries ours
(`src/game/task_sched.cpp`, shadow name `task_sched`), fuzzed headless
against one copy of Capcom's unit on stacks of the fuzz's own: 20,000
rounds, 0 mismatches. Thirty-three negative controls: 30 refused by a count (exit 3), 3 by a fault of the self-test. Not yet through a live check: the
coordinator's batch after the merge (section 7 says what it must look at).

Group EA of the ninth round. The three recorded routes (shop, world map,
combat), traced all-original under a first-call trace of every
pointer-reached start on 2026-09-25, enter one hand-written unit of
Capcom's that is game-side code and still Capcom's: the scheduler every
task body of the game - and every function of ours that runs in one -
stands on. It is taken whole, `0x5A98A0..0x5A9A21`.

The queue also named `0x576CD0` and `0x577B80` as probable task entry
functions. They are not: section 2.

No divergence and no `DIVERGENCE.md` entry: every entry is a faithful
replacement, except that `Task_Create` aborts on a slot past 3 where the
original writes past the records (section 3; the precedent is
[`battle_flow.md`](battle_flow.md) section 1).

## 1. The unit, read to its last instruction

Capstone over `0x5A98A0..0x5A9A30` (2026-09-25): one block of Capcom's
own assembly - fixed push and pop sets around a raw `esp` swap, no
compiler prologues - that ends at `Task_ClearPrivate`'s `ret` at
`0x5A9A21`, then `int3` padding to `0x5A9A30` (an unrelated function, the
software renderer's MMX probe). Nothing in it calls anything: every
transfer that leaves it is the switch itself, a `ret` into a task or a
task's `jmp` back to the landing. An E8 / E9 scan of `.text` and a dword
scan of the image for each entry:

| Entry | Name | Bytes | Reached from |
|---|---|--:|---|
| `0x5A98A0` | `Task_RunAll` | 0x50 (0x67 with the landing) | `call` at `0x4FCF82`, WinMain's loop, once per logic frame unless F9 pauses |
| `0x5A98F0` | `Task_BackToScheduler` | 0x17 (to `0x5A9906`, shared with the walk's end) | `jmp` from `0x5A9971`, `0x5A99A8`, `0x5A99EF` - `Task_Sleep`, `Task_Restart`, `Task_Exit`; nothing else |
| `0x5A9907` | `Task_SetStackBase` | 0xD | WinMain's restart `0x4FCFF6`, `Game_Init`'s tail `jmp` `0x4FD1ED` |
| `0x5A9914` | `Task_Create` | 0x35 | 5 calls: `0x4FCDA8` (WinMain: 0, `Boot_Task`), `0x496C77` (`Boot_Task`: 1, `0x496C90`), `0x49505D` (`Transition_Start`: 2), `0x462359`, `0x46240D` (the title states: 0) |
| `0x5A9949` | `Task_Sleep` | 0x2D | 76 calls |
| `0x5A9976` | `Task_Restart` | 0x37 | 6 calls: `0x4327DF`, `0x569B64`, `0x56CB53`, `0x56CB71`, `0x56D500`, `0x58886D` |
| `0x5A99AD` | `Task_Exit` | 0x47 | 9 calls and a tail `jmp` (`0x462415`) |
| `0x5A99F4` | `Task_ClearPrivate` | 0x2E | 4 calls |

`Task_Restart` was unnamed (`symbols.toml`'s note on `Task_Sleep` called it
"restarts the current task"); the name is the one
[`shop_states2.md`](shop_states2.md) already used for it. The landing is
not a function but a label inside `Task_RunAll`; it has its own entry here
because three jumps reach it, and so that `BOF3X_ORIGINAL` can switch it
with the rest. The first-call traces list it with "caller" `0x496C84` -
that is only the dword at `esp` when the first `Task_Exit` (`Boot_Task`'s)
jumps there.

### The records and the three words

`Task_Records` `0x66C7D0` (now a `[[data]]` entry, no `ctype`, so that
`bof3::addr::Task_Records` keeps working): four records of 0x20 bytes -
`+0` u16 state (0 free, 1 sleeping, **any other value runnable**), `+2` u16
frames left to sleep, `+4` the saved `esp`, `+8..+0x1F` the task's own
words (task 0's `Game_Mode` / `Game_Step` are `+0x18` / `+0x1A`). Named
here, with `ctype = "unsigned long"`: `Task_CurrentOffset` `0x66C850` (the
running record's offset, 0 / 0x20 / 0x40 / 0x60, written only by the walk),
`Task_SchedulerEsp` `0x66C854`, `Task_StackTop` `0x66C858`. Task k's
stack is the 0x4000 bytes below `Task_StackTop - k * 0x4000`.

### The contract a task sees

This is what the replacement had to keep, and it is a register contract,
not a C signature:

- **`Task_RunAll`** pushes `ebx ecx esi edi ebp`, stores `esp` in
  `Task_SchedulerEsp`, and walks `edx = 0, 0x20, 0x40, 0x60`: the offset to
  `Task_CurrentOffset`; state 0 skipped; state 1 has its sleep word
  decremented (16 bits) and is skipped unless that reached 0, when it
  becomes 2; any other state runs. Running is `esp = +4`, then `pop ebp edi
  esi edx ecx ebx` and `ret` into the task. After the fourth record it pops
  `ebp edi esi ecx ebx` and returns. **`eax` is never written**: a task
  entered sees the `eax` the previous yield left (`Task_Sleep`: the
  yielding task's `edx`; `Task_Restart`: its entry; `Task_Exit`: the
  exiting task's `eax`), and so does WinMain; `edx` comes back 0x80.
- **The landing** reloads `esp` from `Task_SchedulerEsp` and goes on at
  `edx + 0x20`, so whoever jumps there must leave `edx` = its own offset.
- **`Task_Sleep(frames)`** keeps the caller's `edx` through `eax`, pushes
  `ebx ecx edx esi edi ebp` in the scheduler's pop order under its own
  return address, sets state 1 and the sleep word to the low 16 bits of
  `frames`, and stores `esp`. Resumed, the six are the caller's and the
  `ret` returns with the argument still on the stack (cdecl).
- **`Task_Restart(entry)`** and **`Task_Create(slot, entry)`** compute the
  fresh saved `esp` as `Task_StackTop - slot * 0x4000 - 0x1C` (32-bit) and
  write the entry at `+0x18`: the six dwords below it are whatever the
  stack holds, and are the registers the task first sees. Neither touches
  the sleep word. `Task_Restart` takes the slot from the current offset
  `>> 5`, leaves `ebx` the new `esp` and `eax` the entry, and leaves by the
  landing - the task starts again on the next frame.
- **`Task_Exit`** zeroes the six dwords `+0..+0x17` (state, sleep, `esp`,
  the private words to `+0x17`) and leaves by the landing; `+0x18..+0x1F`
  survive.
- **`Task_ClearPrivate`** zeroes `+8..+0x17` of the current record.
- **`Task_SetStackBase`** is `Task_StackTop = esp - 0x4000` with `esp` as
  it arrives, and leaves `eax` that value.

## 2. `0x576CD0` and `0x577B80` are switch cases, not task bodies

Read 2026-09-25. Neither address is passed to `Task_Create` or
`Task_Restart` (every push of an immediate to either is listed in section
1's table, and the only dword in the image equal to each is inside a jump
table): each is a case block of a function that is already ours.

- `0x576CD0` is a case of `MoveScript_Step` `0x576B50` (ours,
  `move_script.cpp`): the opcode's high nibble indexes the byte table
  `0x576D1C`, which picks one of ten entries of `0x576CF0`; entry 9 (the
  dword at `0x576D14`), nibble `0xF`, is `0x576CD0` - `MoveScript_GroupF(object,
  script, value)`, the position word `+0xA` + 1, then the host's epilogue.
- `0x577B80` is the `0xFF` case of `MoveScript_GroupF` `0x577760` (ours):
  byte - 0xF0 indexes the sixteen entries of `0x577B90`; entry 15 (the
  dword at `0x577BCC`) is `0x577B80` - the position word - 1, the value
  byte back, the epilogue.

Their "callers" `0x7DEFA4`, `0x7DF048`, `0x7DF0EC`, `0x5DDD64`, `0x5EF4B8`
are not return addresses: a case block reached by `jmp` finds whatever the
host left at `esp`, which here is a task stack or a data pointer.
[`attract-remaining.md`](attract-remaining.md) said the same of both on
2026-09-22. The routes reach them only on an all-original run; with the
hosts ours those bytes never execute. Nothing to take: they went with
their hosts.

## 3. What is ours

`src/game/task_sched.cpp`. The five that switch stacks - `Task_RunAll`,
the landing, `Task_SetStackBase`, `Task_Sleep`, `Task_Restart`,
`Task_Exit` - are naked functions whose asm is the original's instruction
for instruction, leaving the same registers in the same states, `eax` and
`ebx` included: nothing is known to read them, but any compiled task body
may, and "nothing is known" is not a contract. The literal addresses in
the asm are the `symbols.toml` data above. `Task_Sleep`, `Task_Restart`
and `Task_Exit` jump to `_Task_BackToScheduler`, the landing's symbol, so
that `BOF3X_ORIGINAL=Task_BackToScheduler` sends them to Capcom's. The
landing jumps into a label of our `Task_RunAll`'s walk.

`Task_Create` and `Task_ClearPrivate` switch nothing and are cdecl C++.
They may clobber `ecx` where the original keeps it; every caller is
compiled code (MSVC's or ours) for which `ecx` is volatile across a call.
`Task_Create` aborts on a slot past 3: the original writes the fifth
record over `Task_CurrentOffset` and `Task_SchedulerEsp`, so the next
landing would load a garbage `esp`. Every caller passes a constant 0, 1 or
2.

Mixed A/B sides are safe in any combination: both sides keep the same
stack layout, the same three words and the same register contract, so
Capcom's `Task_Sleep` landing in our walk or ours in Capcom's is the same
machine. `BOF3X_ORIGINAL='*'` with the self-test passes (section 4).

Other files touched, both for the tracer (section 5):
`src/hook/calltrace.cpp` and `src/game/win_main.cpp`.

## 4. The fuzz, and the controls

`BOF3X_SHADOW=task_sched`, at start-up (`task_sched_fuzz.cpp`). One copy of
the whole unit (`CloneOriginal`, 0x182 bytes: every jump in it stays in it,
and it calls nothing), its eight entries found by offset
(`task_sched_callees.h`, which holds the layout the two files share -
there are no callees). A round:

- **A world the scheduler can run.** A scratch arena of the fuzz's own
  (four stacks and 0x1000 above), filled with random dwords;
  `Task_StackTop` over it at a random 4-byte offset; the four records with
  seeded state and sleep words (0, 1, 2, 3, 0xFF, 0x100, 0x101, 0x8000,
  0xFFFF, or random), random private words, and for every record not free
  a saved frame of six random registers under the entry of the fuzz's task
  body, somewhere in the top 0x400 bytes of its stack; random
  `Task_CurrentOffset` and `Task_SchedulerEsp`.
- **One to six frames** of `Task_RunAll` with seven random registers in,
  then `Task_SetStackBase` at a random depth; first through the copy's
  entries, then from the same world through ours.
- **The task body is the recorder.** Every entry logs the eight registers
  and `esp` it arrives with, and its own state and sleep words (a fresh
  task's registers are the arena's words the
  scheduler popped, so `Task_Create`'s and `Task_Restart`'s arithmetic
  shows there), then follows the round's plan: up to two of
  `Task_Create` of another slot, `Task_ClearPrivate`, or a poke at another
  live record's state or sleep word; then one yield - `Task_Sleep` with
  six random registers loaded (logged again, with `eax` and `esp`, when it
  resumes; frames 1, 2, 3, 0, 0x10001, -1, 0x7FFF or random), `Task_Exit`,
  `Task_Restart` at its own entry, or its state 0 and a bare `jmp` to the
  landing with `edx` set as the yields set it.
- **Compared:** the event logs, the registers each `Task_RunAll` returns,
  `Task_SetStackBase`'s `eax` and result, the four records, the three words
  and the whole arena (every saved frame and every entry slot).

Every call into the side under test goes through an asm thunk that reads
the target from a global table, so neither side's addresses land on a task
stack; after the two C++ entries of ours the thunk zeroes 0x100 bytes below
its `esp`, the dead bytes a compiled callee leaves that differ from the
copy's.

Result (2026-09-25):

    shadow      task_sched self-test: 20000 rounds, 341961 events, 0 MISMATCHES; the records, the
                scheduler's three words, the four stacks and every task entry's registers compared
    shadow      task_sched coverage: frames 70229, entries 74978, resumes 17920; sleeps 58229, exits
                11555, restarts 11340, landings 11774; creates 23062, clears 23292, pokes 19582; stack
                bases 20000; records at a walk's start: free 118520, asleep 76522, waking 16560,
                state 2 21602, other states 47712

(The coverage counts the ours passes only.)

The fuzz found no difference of ours. Its first run faulted on its own
bug: an enumerator `kSleep` (the log's event kind, 6) shadowed the record
layout's `kSleep` (2), so the pokes wrote `+6` of a record - the high word
of a free record's saved `esp` - and the walk loaded it.

`BOF3X_SHADOW='*'`: exit 0, every module's self-test passing; and
`BOF3X_ORIGINAL='*' BOF3X_SHADOW=task_sched` (every entry of ours patched
back to Capcom's): exit 0.

**Thirty-three negative controls**, planted one at a time by a script (not
committed: apply, build, run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=task_sched`,
restore; each build read). 30 are refused by a count (exit 3), 3 by a fault
of the self-test - each of those a stack arithmetic that lands a task on
another's stack, or a record left live over a frame the task has since
stood on; the in-bounds variants of the same two arithmetics (C18, C21)
are refused by a count. At least one per behaviour: the sleep countdown,
state 1 to 2, the skipped free record, the walk's end, the pop and push
orders, every register the contract leaves (`eax` included), create's and
restart's stack arithmetic, exit's and clear's extents.

C2 was **not** refused on the first run: a woken task that stays state 1
is overwritten by its own yield before anything else reads the record. It
is observable only by a task reading its own record while it runs, so the
recorder now logs the running task's state and sleep words at every entry
and resume; the table is the second run, every control against the final
fuzz.

| | Planted | Rounds refused (of 20,000) |
|---|---|--:|
| C1 | `Task_RunAll`: the sleep word counted down by 2 | 16,199 |
| C2 | `Task_RunAll`: a waking task not made state 2 | 9,469 |
| C3 | `Task_RunAll`: the free test on a byte | 8,324 |
| C4 | `Task_RunAll`: the sleeping test on a byte | 4,654 |
| C5 | `Task_RunAll`: the walk ends after three records | 20,000 |
| C6 | `Task_RunAll`: the current offset stored only for a task entered | 13,150 |
| C7 | `Task_RunAll`: resume pops `ecx` / `edx` swapped | 19,677 |
| C8 | `Task_RunAll`: resume pops `ebp` / `edi` swapped | 19,677 |
| C9 | `Task_RunAll`: the end restores `edx` for `ecx` | 20,000 |
| C10 | the landing goes back to the same record | 18,694 |
| C11 | `Task_SetStackBase`: 0x3FFC below `esp` | 20,000 |
| C12 | `Task_SetStackBase`: `eax` not left the base | 20,000 |
| C13 | `Task_Sleep`: the sleep word written as a byte | 16,929 |
| C14 | `Task_Sleep`: state 2, not 1 | 18,217 |
| C15 | `Task_Sleep`: `ebx` / `ecx` saved swapped | 18,418 |
| C16 | `Task_Sleep`: `eax` not left the caller's `edx` | 18,422 |
| C31 | `Task_Sleep`: the frames read from `[esp+8]` | 18,216 |
| C17 | `Task_Create`: the first frame 0x18 below the top | 11,439 |
| C18 | `Task_Create`: stacks 0x4010 apart | 9,904 |
| C19 | `Task_Create`: state 1, not 2 | 11,181 |
| C20 | `Task_Create`: the sleep word cleared too | 10,490 |
| C30 | `Task_Create`: stacks 0x2000 apart | fault (stacks overlap) |
| C21 | `Task_Restart`: stacks 0x4010 apart | 6,279 |
| C22 | `Task_Restart`: the state not set | 4,268 |
| C23 | `Task_Restart`: the first frame 0x18 below the top | 7,954 |
| C24 | `Task_Restart`: `eax` not left the entry | 7,954 |
| C29 | `Task_Restart`: the slot from `>> 6` | fault (stacks overlap) |
| C25 | `Task_Exit`: `+0x14` left | 6,453 |
| C26 | `Task_Exit`: `+0x18` zeroed too | 8,897 |
| C32 | `Task_Exit`: the state word left (the sleep word zeroed instead) | fault (a stale frame resumed) |
| C27 | `Task_ClearPrivate`: three dwords | 11,038 |
| C28 | `Task_ClearPrivate`: `+0x18` zeroed too | 12,732 |
| C33 | `Task_ClearPrivate`: from `+0xC` | 11,038 |

## 5. The call tracer's frame count

The tracer (`BOF3X_CALLTRACE`, [`call-trace.md`](call-trace.md)) counts a
logic frame as an arrival at `Task_RunAll` `0x5A98A0` - an `int3` re-armed
after every hit - and flushes its hits from that breakpoint. Owning
`Task_RunAll` would have broken it three ways, and each is fixed without
changing what a frame is:

1. `calltrace.cpp` took the address from the name `Task_RunAll`, which with
   an `impl` becomes our function in `bof3x.dll`: the list check would have
   `Fatal`ed. It now takes `bof3::addr::Task_RunAll`.
2. A function registered with `Inject` is never armed (section 7 of
   call-trace.md). `Task_RunAll` is now the one exception: its owned range
   is recorded as for any owned function (it makes no call, so the range
   changes no caller) and its entry is armed as before. On the ours side
   the `int3` sits on the detour's `E9`; the handler restores it, steps the
   `jmp`, and re-arms from our function's first instruction. Under
   `BOF3X_ORIGINAL` it sits on Capcom's `push ebx` as it always did.
3. WinMain is ours and called `Task_RunAll` by name, which would now reach
   ours directly and never pass `0x5A98A0`. It calls
   `bof3::orig::Task_RunAll()` - the literal address, the same code the
   macro compiled to before - so every frame still arrives at `0x5A98A0`
   and goes through the detour (or, under `BOF3X_ORIGINAL`, is Capcom's).

So a frame is exactly what it was: WinMain's arrival at `0x5A98A0`.

**What does change in the hash**: `Task_Sleep`, `Task_Exit`,
`Task_Create`, `Task_ClearPrivate`, `Task_Restart` and `Task_SetStackBase`
are owned now, so the tracer leaves them unarmed on both sides of an A/B
(as for every takeover). Every frame's hash loses the `Task_Sleep` calls it
had. **A reference recorded before this merge cannot be compared with a
run after it** - re-record the original-vs-original pair with the merged
build, as every round does.

`analysis/calltrace/entries_logic.txt` (main checkout): the seven entries
were already listed at the extents read here (`005A98A0 67`, `005A9907 D`,
`005A9914 35`, `005A9949 2D`, `005A9976 37`, `005A99AD 47`, `005A99F4 2E`);
`005A98F0 17`, the landing, is added.

This could not be checked headless: the tracer starts after the self-tests
end the process. It is the first thing the live check must look at.

## 6. Defects of the original (latent, kept)

For the coordinator to number:

- **`Task_Create` does not test the slot.** A slot of 4 writes the fifth
  "record" over `Task_CurrentOffset` / `Task_SchedulerEsp` (and its entry
  somewhere below the arena); the next landing loads a garbage `esp`.
  Every caller passes a constant 0..2. Latent; ours aborts.
- **The task stacks have no guard.** A task deeper than 0x4000 bytes
  writes into the next task's stack (task 3's into whatever lies below the
  arena in WinMain's frame's neighbourhood). Known since
  [`SCAFFOLDING.md`](SCAFFOLDING.md) section 3; kept - it is the layout.
- Not defects, noted: any state word but 0 and 1 runs (0x100 is not free,
  0x101 is not asleep); a sleep of 0 lasts 65,536 frames (decrement, then
  test); `Task_Exit` keeps `+0x18..+0x1F`, so task 0's `Game_Mode` /
  `Game_Step` survive its exit; a task created in a later slot runs the
  same frame, one in an earlier slot the next.

## 7. What the route reaches, and what the live check must look at

Every logic frame of every run goes through `Task_RunAll`, the landing and
`Task_Sleep`; boot through `Task_SetStackBase` and `Task_Create`
(`hidden_b`: frame 0), the title through `Task_ClearPrivate` and
`Task_Exit` (frame 1). `Task_Restart` is entered, by the code, whenever
the title enters a game (`TitleFlow_EnterGame` `0x58886D`) and by the
other five sites; the first-call traces did not list it, so its reach is
not measured. `Task_SetStackBase`'s restart path (`Game_RestartFlag`,
F9's second press in game) is not on any recorded route.

Fuzz only, certainly: a slot past 3 (the abort), states other than 0..2
and sleep words past a few frames in a real record, `Task_Restart` from a
task other than the ones the game restarts.

**The live check, in order:**

1. **The tracer**: a traced run must log `calltrace: N entries armed ...`
   with `Task_RunAll` among them (no `Fatal` about the list), and
   `bof3x.calltrace.tsv` / `callframes.tsv` must advance frame by frame at
   the usual rate. A frame count stuck at 0 means WinMain is not reaching
   `0x5A98A0`.
2. **The frame hash** original-vs-original and original-vs-ours on the
   same (merged) build, each side with its own new reference (section 5).
3. The game boots to the title, the demo starts and ends, a game is
   entered from the title (`Task_Restart`), a transition fades, and F9
   pauses and resumes.
4. The oracle and the memory dump as usual; `Task_Records` lies in the
   dump's `.data`.

## 8. Other groups' addresses

None taken. `0x576CD0` and `0x577B80` are the hosts' (section 2), both
already ours in `move_script.cpp`.

# A crude x86 → C lifter: feasibility (IDEAS I3)

**Status:** DRAFT (measured 2026-09-25, on a synthetic corpus; nothing here has
yet run against `BOF3.exe`)

[`PLAN.md`](PLAN.md) §2B rejects static recompilation as the project's base and
keeps it as a phase-4 accelerator: "a *crude* x86→C lifter is far easier than a
good one, because in model C it never has to be pretty - only correct"
([`IDEAS.md`](IDEAS.md) I3). This study asks how far "crude but correct" is, with
two kinds of evidence: a working prototype measured on code we wrote, and what
the docs have already measured about `BOF3.exe`. Only the owner's machine can
supply the third kind, the prototype run on the real binary. §7 gives that step.

## Result

**Lifting the instructions is the easy part.** The prototype is 643 lines of
Python over capstone (`tools/lift/lift_x86.py`). It lifts all 33 functions of a
synthetic corpus (780 instructions, written to imitate the shapes the docs record
in `BOF3.exe`) to C that compiles unchanged for x86-64. That C matches the
original machine code run in an emulator on every comparable round:
**0 mismatches in 55,774 compared rounds** (§3). Six negative controls are all
refused. The model is PLAN's crude one: registers as variables, the whole 32-bit
address space as one array, so the output is 64-bit clean.

**The hard parts are ones the docs have already measured** (§4). In order of
how much each blocks a cutover:

1. **The task system switches coroutines by a raw `esp` swap.** Lifted C cannot
   express that: a lifted function returns through the host's C stack. The four
   tasks have to become host coroutines, written by us, before any lifted code
   can call `Task_Sleep`. None of the task system is ours today.
2. **The x87 model is exact only under one control word.** Plain doubles are
   exactly right under `0x027F`, the word the game thread was measured running.
   The renderer's word has never been measured, and Direct3D may set 24-bit. The
   prototype aborts on any other word rather than compute something else.
3. **Function discovery.** About 10,200 starts against the 2,952 that call
   sites give; recursive descent from the entry reaches 309. A lifter cannot
   find its own inputs. It needs the entry lists and `symbols.toml`, and it
   turns every miss into a loud `rt_fail` rather than a wrong answer.
4. **The platform.** 101 imported symbols and the DirectX vtables. These gate
   phase 4 exactly as PLAN already says (phase 3 first).

**Recommendation:** raise I3's feasibility from LOW to **MEDIUM** and keep it a
phase-4 option. Add two gates that PLAN does not name: the task system ours on
host coroutines, and the renderer thread's control word measured. §6 has a use
for the lifter before phase 4: it keeps a differential oracle alive after the
original bytes can no longer run in our process.

## 1. What was built

All under [`tools/lift/`](../tools/lift). It reads no game data, and CI runs
its self-test (`.github/workflows/checks.yml`).

| File | What it is |
|---|---|
| `corpus/corpus.c` | 33 functions written for this study: record arrays at fixed addresses, a dense `switch` (jump table), a sparse one, handlers reached only through a pointer table, x87 doubles and a float store, float→int conversion under a changed rounding mode, 64-bit fixed-point products (`imul` + `shrd`), `shld`, stdcall and fastcall, recursion, byte tables, `memcpy`/`memset`, signed compares at the overflow edge. No game code. |
| `corpus/build.sh` | Builds it as a `/FIXED` i386 PE at `0x400000` with no CRT: `BOF3.exe`'s layout. `-march=pentium -mno-sse` gives x87 and no `cmov`, as MSVC6 targets. |
| `lift_x86.py` | The lifter. Recursive descent per function from a given entry; jump tables recognised as `jmp [table + reg*4]` and bounded by the preceding `cmp`; one C function per entry. |
| `lift_rt.h`, `lift_rt.c` | The runtime: the register file, the x87 stack, the guest space, the control word, the indirect-call table. |
| `lift_fuzz.py` | The oracle: the original bytes run in Unicorn (QEMU's x86 core) against the lifted C compiled for the host, on identical random state. |
| `selftest.sh` | Build, lift, fuzz, then the negative controls. |

**The model.** Each lifted function loads the eight registers into locals and
computes every flag eagerly after every instruction. The C compiler deletes the
flags nobody reads. At `-O2` the lifted `Fixed_Mul`, 20 lines of C, is one
`imul`, one `shrd`, and the loads and stores of the register file: 16 host
instructions. Memory is `M[addr]` with `M` a 4 GiB
`MAP_NORESERVE` reservation, so a guest address is an offset and no host
pointer enters guest memory. A direct call pushes the real return address onto
the guest stack, so a callee that reads its caller's frame reads the right
bytes. An indirect call or jump goes through a sorted table of lifted
functions. x87 is a stack of eight doubles. `fldcw` mirrors the rounding
control into the host's with `fesetround`, which is why the output must be
built with `-frounding-math`. It aborts on any precision control but 53-bit.

**What it refuses rather than guesses**, per `CLAUDE.md` rule 4: an instruction
with no semantics, a segment override (SEH), an import call, 80-bit x87 loads
and stores, an indirect target that was not lifted, division faults. Each
becomes `rt_fail()` at that address. It aborts only if that path runs, and the
lifter's report counts it.

## 2. Why an emulator is the oracle

The project's differential fuzz runs a byte-copy of the original beside ours in
the game's own process (`CloneOriginal`, [`SCAFFOLDING.md`](SCAFFOLDING.md) §2).
That process is 32-bit, and lifted C is meant for the 64-bit build, where the
original bytes cannot run in-process. OpenRCT2's "data crosses the 32→64
boundary; code does not" ([`prior-art/openrct2.md`](prior-art/openrct2.md) §2.2)
applies to the oracle too.

An emulator removes that dependency. Unicorn executes the PE's own bytes from
the entry to a sentinel return address. `lift_fuzz.py` gives both sides:

- the same random registers;
- random bytes in every writable section and in a 32 KiB stack window;
- arguments drawn from boundary values, small integers, negative values, noise,
  and pointers into writable data;
- control word `0x027F`.

It then compares the eight registers, the x87 TOP, st(0) if the call pushed
one, and every writable byte. A round where the *original* faults, such as a
random pointer, is "skipped", never counted as a pass. The fuzz is seeded, so a
run repeats exactly.

## 3. Measurements

The corpus: 33 functions, 780 instructions, 2,629 lines of C (3.4 lines per
instruction). It compiles with no warnings under
`-Wall -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable`.

**The faithful lift.** `lift_fuzz.py --rounds 2000 --seed 7`: 33 functions x 2,000 rounds = 66,000; **55,774 compared, 0 mismatches**; 10,226 skipped.
Each skip is a round where the original faulted, and all of them fall in six
functions, the ones that take pointer arguments:

| Function | Skipped (of 2,000) |
|---|---|
| `H_Die`, `H_Idle`, `H_Move`, `H_Wait` (the pointer-table handlers) | 1,596..1,612 each |
| `memset` | 1,846 |
| `memcpy` | 1,967 |

A skipped round is not a comparison, so those six rest on fewer rounds than the
rest: `memcpy` on only 33, because a random length usually runs off the end of
the data. The fuzz aims 20% of arguments at valid data, and that is what reaches
them at all.

**Negative controls** (`selftest.sh`, 500 rounds a function over the 32
functions before `Signed_Order` was added, 2026-09-25; CI runs them at 100).
Each is one deliberate fault, and each must be refused:

| Control | Mismatching rounds |
|---|---|
| `adc` without its carry | 555 |
| `sar` done as a logical shift | 712 |
| `cmp`'s overflow flag taken from the wrong operand | 4 (43 at 100 rounds, once `Signed_Order` was added) |
| `fdivr`'s operands swapped | 500 |
| a float store kept at double width | 500 |
| the oracle at 64-bit precision, `0x037F` | 1,212 |

Two of these are findings, not just checks:

- **Overflow needs aimed inputs.** The first corpus had no function whose
  result turns on signed overflow in a compare, and the wrong-`OF` control was
  refused on only 4 of 16,000 rounds. One function comparing unmasked signed
  values (`Signed_Order`) raised that to 43 in 100 rounds. For `BOF3.exe` this
  means boundary seeding per function, the same lesson the hand
  reimplementations' fuzzes learned.
- **The control word decides whether doubles are right.** The same lifted C,
  against an oracle running at 64-bit precision, mismatches on 1,212 rounds.
  The whole x87 model rests on the measurement in
  [`psx-library-layer.md`](psx-library-layer.md) §2: `0x027F` on all
  11,272,192 calls on the game thread.

**What the output looks like** (from the corpus, not the game).
`Fixed_Mul(a, b)`, `(s32)(((s64)a * b) >> 12)`, three instructions:

```c
L_004010F4: /* imul dword ptr [esp + 4] */
    { s64 p_ = (s64)(s32)eax * (s32)LD32((u32)(esp + 0x4u)); eax = (u32)p_; edx = (u32)((u64)p_ >> 32);
      cf = of = p_ != (s64)(s32)p_; }
L_004010F8: /* shrd eax, edx, 0xc */
    { u32 n_ = (0xcu) & 31u; if (n_) { u32 d_ = eax, s_ = edx, r_; r_ = (d_ >> n_) | (s_ << (32 - n_)); ...
```

PLAN §2B's objection stands: this is unreadable, and changing game logic in it
is no easier than hooking. In model C it does not need to be readable. It is a
placeholder that compiles for 64 bits until a hand-written function replaces it.

## 4. What `BOF3.exe` adds that the corpus does not

Each item gives what the docs measured and what it means for a lifter.

### 4.1 The task system: a raw `esp` swap (the structural blocker)

Game logic runs on four cooperative tasks with 16 KiB stacks. `Task_Sleep`
saves callee state on the task's stack, records `esp`, and jumps back into the
scheduler. It is hand-written assembly, not compiler output
([`attract-mode.md`](attract-mode.md) §2, [`SCAFFOLDING.md`](SCAFFOLDING.md)
"Hazard"). `Task_Sleep(1)` is how every wait loop in the game is written.

A lifted function is a host C function. It returns through the host stack, so
a guest `esp` swap into another task's frames cannot be lifted. The only
workable shape is ours: `Task_Create` / `Task_Sleep` / `Task_RunAll`
reimplemented on host coroutines (fibers on Windows, `ucontext` or a small
assembly switch elsewhere). Each task then gets its own host stack as well as
its guest one, and lifted code calls `Task_Sleep` as an ordinary function that
does not return until the task is resumed. **None of the task system is ours
today** (no `impl` on any `Task_*` in `symbols.toml`). It is small, and it is
a precondition.

### 4.2 x87 under more than one control word

- About 10,800 x87 instructions, and no MMX or SSE ([`PLAN.md`](PLAN.md) §1).
- On the game thread the control word is `0x027F` on every call measured
  ([`psx-library-layer.md`](psx-library-layer.md) §2). Doubles are exact there,
  as the negative control above shows the other way round.
- The renderer thread's word "has never been measured", and Direct3D may set
  24-bit. The Direct3D handlers keep their original x87 sequences in inline
  assembly because SSE differed under `0x007F`
  ([`d3d-draw.md`](d3d-draw.md), the precision section). Those handlers are
  ours already, so this matters only if something on that thread is left to the
  lifter.
- `fpatan` computes at 64 bits whatever the control word says
  ([`move-cmds.md`](move-cmds.md) §2). `Math_Ratan2` is ours already. Any other
  transcendental needs the same care, and the prototype has no semantics for
  any of them yet, so it refuses them.
- 80-bit memory operands (`fld tbyte`) are refused. How many there are is
  unmeasured (§7).

The prototype's `rt_fldcw` aborts on any precision but 53-bit. For `BOF3.exe`
that is right until a 24-bit model is needed. That model would be `float`
arithmetic mirrored the same way, but it is not written.

### 4.3 Function discovery

- 2,952 bodies from direct-call targets; 7,294 more starts found by
  `pe_hidden.py`'s padding rule; "roughly 10,200 functions"
  ([`attract-remaining.md`](attract-remaining.md) §3). The catalogue counts a
  universe of 10,246 starts, "a floor"
  ([`remaining-catalog.md`](remaining-catalog.md)).
- Recursive descent from the entry point reaches 309 functions
  ([`PLAN.md`](PLAN.md) §1). The game is a function-pointer machine.
- Known misses: functions after an inline jump table (`0x593860` is 213 bytes,
  then its table at `0x593938`, then about ten handlers the sweep falls out of
  step on - [`attract-remaining.md`](attract-remaining.md) §3), and sizes that
  run into a neighbour (`0x56FF00` recorded as `0xBA6`, really `0x118`).

For a lifter this is a seeding problem, as PLAN says. The prototype helps in
two ways. Its recursive descent per function is independent of the recorded
sizes, so it lifts a function's real extent whatever the entry list says about
its length. And an indirect call to a start nobody listed lands in
`rt_dispatch`, which fails loudly with the address. The call tracer
([`call-trace.md`](call-trace.md)) is already a runtime harvester for exactly
those misses.

### 4.4 Indirect control flow

- 1,392 indirect calls: 738 through a register-based memory operand, 280
  through `[imm]`, 259 through `[reg*4+imm]`, 109 through a register.
  1,542 indirect jumps, "mostly tail-call and jump-table shapes"
  ([`PLAN.md`](PLAN.md) §1).
- Jump tables live in `.text` right after their functions, sometimes behind a
  byte index table ([`d3d-draw.md`](d3d-draw.md), `Gfx_DrawOTag`). Some have no
  bound and land in another function's cases (DIV-0024).
- 255 dispatchers build call tables on the stack
  ([`attract-remaining.md`](attract-remaining.md) §3).

The prototype lifts a table jump as a `switch` on the target it loads at run
time, one case per decoded entry and `rt_fail` as the default. Including too
many entries costs only unused labels. Missing one fails loudly. An unbounded
table is safe for the same reason, which is what DIV-0024's shape needs.
MSVC's byte-index-table form is **untested**: clang places the corpus's tables
in `.rdata` and never emits the two-level form.

### 4.5 The platform: imports, COM, SEH

- 101 imported symbols across 7 DLLs ([`PLAN.md`](PLAN.md) §1). DirectX is
  reached through COM vtables.
- 37 `fs:` references, all in the CRT.

The prototype refuses import calls and segment overrides. A lifted build
therefore needs the platform layer ours first, which is PLAN's phase 3 gate,
unchanged. The CRT (242 functions, [`remaining-catalog.md`](remaining-catalog.md))
is replaced, not lifted.

### 4.6 Register contracts between functions: free in a lifter

The hand reimplementations kept meeting callers that read what a callee left
behind. For example, `Battle_StatusTint` returns *the caller's own* `eax` on
its no-tint path ([`battle_misc.md`](battle_misc.md), the return-value table),
and other callers read only `al`. Each of those had to be found and matched.
A lifter keeps the whole register file across every call and return, so these
survive by construction. This is one place where lifting is *more* faithful
for less effort than reading.

### 4.7 The address space

`BOF3.exe` is `/FIXED` at `0x400000` with no `.reloc` ([`PLAN.md`](PLAN.md) §1),
so guest addresses never need relocating. The prototype offsets every access
from `M`. The open design question is our 1,025 C++ functions: they reach game
memory as native 32-bit pointers (`symbols.gen.h`). In a 64-bit process they
would need the same `M` offset, or the guest space would need mapping at its
own addresses (the image range and a heap below 4 GiB, reserved at start-up),
so that `M` is 0 and a guest pointer is a host pointer. The second keeps our
code unchanged. It is **untested here**.

## 5. Limits of this study

- **The corpus is clang output, not MSVC6.** It imitates the game's shapes but
  not MSVC's idioms: jump tables inside `.text`, the byte-index form, calls to
  the CRT's `_ftol`, shared epilogues and tails
  ([`tex-page.md`](tex-page.md)), folded identical functions. Each needs the
  real binary to test.
- **The oracle is an emulator.** Unicorn's x87 is QEMU's softfloat, so its
  agreement with a real CPU under precision control 53 is assumed, not
  measured here. The in-process clone fuzz checks the real CPU.
- **Denormals and overflow are untested.** Under 53-bit precision the x87
  still has a 15-bit exponent, so a result in double's denormal or overflow
  range is rounded twice (once to 53 bits, once when stored) where SSE rounds
  once. The corpus's random inputs never land there.
- **Pointer-taking functions rest on fewer rounds** (the skip table in §3).
- **No performance measurement.** The original cannot run natively in a 64-bit
  process to compare against.

## 6. Uses before phase 4

- **An oracle that outlives the 32-bit process.** Once the build is 64-bit,
  `CloneOriginal` has nothing to run. Unicorn on the PE's bytes (this fuzz), or
  the lifted C itself, keeps "compare against Capcom" available. That matters
  because every prior-art project lost its oracle
  ([`prior-art/README.md`](prior-art/README.md) §1). The verifier becomes the
  generator: the lifted C of a function is both a runnable placeholder and a
  reference.
- **Fuzzing hand reimplementations off the game process.** `lift_fuzz.py`'s
  harness can run `BOF3.exe`'s bytes for a function in isolation on any
  machine that has the file, with no game running. The in-process self-tests
  remain the authority. This is a faster loop, not a replacement.

## 7. First concrete step (on the owner's machine)

Nothing below should be committed: its output is derived from `BOF3.exe` and
belongs in `analysis/` (`CLAUDE.md` rule 1).

1. An entry list for the whole binary: `hex-address name` per line, from
   `symbols.toml` plus the hidden entries (`pe_hidden.py`'s list).
2. `python tools/lift/lift_x86.py --pe bof3/BOF3.exe --entries analysis/lift/entries.txt --out analysis/lift/lifted.c`.
   Its report counts, per reason, what it refused: the mnemonics with no
   semantics, 80-bit operands, import calls, unbounded tables, functions it
   could not decode. That report is the real measure of how far "crude" is from
   `BOF3.exe`, and it is the number I3 needs next.
3. Fuzz the leaf functions (no calls out) with `lift_fuzz.py`, with the lifted
   C built as in `selftest.sh`. Then compare its verdicts on functions that are
   ours against their in-process self-tests. Agreement there is what would let
   the emulator oracle be trusted where the clone cannot go.

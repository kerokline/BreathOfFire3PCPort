# Scaffolding — launcher, detour layer, generated symbol header

**Status:** STABLE (verified 2026-09-19 — phase 0 exit test passed, §5)

How our code gets into the game's process and takes over one original function
at a time. This is [`PLAN.md`](PLAN.md) phase 0. It is **scaffolding built to be
dismantled**: every function we land makes it matter less, and the endpoint of
the project is a build where none of it exists. Keep it thin.

The design is taken from two projects that finished this journey
([`prior-art/tr1x.md`](prior-art/tr1x.md) §2.1–2.4,
[`prior-art/openrct2.md`](prior-art/openrct2.md) §2.1). Both are GPL-3.0, so it
was implemented **from those notes' descriptions, not from their source**
(`CLAUDE.md` rule 5).

## 1. The pieces

| Piece | Where | What it does |
|---|---|---|
| `bof3x-launcher.exe` | `src/launcher/` | Hashes the player's `BOF3.exe` against the catalogued build, starts it **suspended**, runs `LoadLibraryW(bof3x.dll)` in it on a remote thread, waits for that to succeed, then resumes the game. |
| `bof3x.dll` | `src/hook/` + `src/game/` | In `DllMain`: verify the image, then install every detour. Holds our reimplementations. |
| `bof3/symbols.gen.h` | generated into `build/gen/` | Every address, and the one binding of every callable name. Generated from [`symbols.toml`](../symbols.toml) by `tools/gen_symbols.py`; never committed, so it cannot drift. |

Because the process is suspended until the DLL has loaded, **every detour is in
place before the game's entry point `0x5BA057` runs.** There is no window in
which a game thread can be inside a function while we overwrite it.

The original file is never modified and never copied. The launcher works on
whatever install the player points it at (`--game <dir>`, else `BOF3_GAME_DIR`,
else the current directory, else `./bof3`).

### Two identity checks, on purpose

Every address in this project is only meaningful inside one exact image
(`CLAUDE.md` rule 3), so nothing is patched on faith:

- the **launcher** refuses to start a `BOF3.exe` whose SHA-256 is not
  `fixtures.toml`'s `pc-zh` build;
- the **DLL** independently refuses to patch unless the main module sits at
  `0x400000` with the PE timestamp and `SizeOfImage` recorded in
  `symbols.toml` `[meta]` — so it is also safe when loaded by something other
  than our launcher.

Both fail loudly (message box, log line, process ended). A half-patched game is
worse than no game.

## 2. The detour

Five bytes: `E9` + rel32, written over the first instruction(s) of the original
function. No trampoline, no disassembler, no MinHook. That is sound only
because of two commitments:

- `BOF3.exe` is `/FIXED` at `0x400000` with no `.reloc` ([`PLAN.md`](PLAN.md)
  §1) — a literal address is the function, every run.
- **A replaced function is replaced whole.** We never resume into the original
  body, so the prologue bytes the `jmp` destroys never execute again. This is
  `CLAUDE.md` rule 4 seen from the other side: no stubs, and no *wrappers*
  either. If you want to run code before or after an original function, you
  have not finished reading it yet.

### The A/B switch

`BOF3X_ORIGINAL` is an environment variable holding a comma-separated list of
function names, or `*`. For a listed function the patch is written in the
**other direction** — over the start of *our* function, jumping to Capcom's — so
every caller, including our own code, gets original behaviour in the same
process under the same loader.

```
BOF3X_ORIGINAL=File_Read   build/bof3x-launcher.exe     # that one is Capcom's
BOF3X_ORIGINAL=*           build/bof3x-launcher.exe     # all of it is Capcom's
```

This is what makes "did we change this, or break this?" a one-line experiment,
and it is the run configuration any replay-style regression oracle must use
([`prior-art/tr1x.md`](prior-art/tr1x.md) §3). `-falign-functions=16` exists for
this direction: none of our functions may be shorter than five bytes before the
next begins.

### The shadow check

The A/B switch compares two *runs*. `BOF3X_SHADOW=Name` compares the two
*functions*, in one process, on the same input — for a function whose effect no
external check can see. `bof3::CloneOriginal` (`hook/detour.h`) byte-copies the
original into executable memory before `Inject` destroys its entry; the owning
file then plans its own result on a copy of the state, lets the clone do the
real work, and compares — and, at start-up, can fuzz the pair on synthetic
input. First and so far only user: `Gfx_InvalidateTextures`
(`src/game/gfx_texcache.cpp`, [`asset-loading-path.md`](asset-loading-path.md)
§2).

It is **not a trampoline** and does not soften the rule that a replaced
function is replaced whole: nothing resumes into the original body, and a build
without the variable never makes the copy. It is sound only for a function
whose every relative jump and call stays inside the copied range — calls
through absolute slots or registers are fine — and that has to be established
from the disassembly, per function, and said where `CloneOriginal` is called.
A relative *call* that does leave can be named and re-aimed (`CloneCall`), at
the original callee or at another clone, so that cloned callers reach cloned
callees and never ours — `src/game/gfx_clut.cpp` clones three that way.
`BOF3X_SHADOW` takes the names each file asks for (`Gfx_InvalidateTextures`,
`gfx_clut`) or `*`.
The copy exists in process memory only; nothing of Capcom's is written to disk.

## 3. One name, bound once

For every function in `symbols.toml` that has a signature, the bare name is
bound **exactly once**, in the generated header, to one of two things:

| `symbols.toml` entry | The name `File_Open` is… |
|---|---|
| has `ret`/`params`, no `impl` | a macro: the literal address cast to a typed function pointer — calls Capcom's code |
| has `impl = "src/…"` | an ordinary prototype for **our** function |

Call sites are identical in both cases. So taking over a function touches none
of its callers:

1. Read it. Record what you learned in its `evidence` field — a signature in
   `symbols.toml` is a claim about the binary and the evidence rule applies.
2. Write it, whole, in the module it belongs to under `src/game/`, as
   `extern "C"` with the recorded calling convention.
3. Add `impl = "src/game/<file>.cpp"` to its entry.
4. Add `BOF3_INJECT(Name);` to that module's `<Module>_Inject()` — **next to
   the code, not in a central table**, so the address and the implementation
   are reviewed in one diff. `src/hook/inject_all.cpp` only lists modules.
5. If behaviour differs from the original *in any way a player or another
   function could observe*, it needs a [`DIVERGENCE.md`](DIVERGENCE.md) entry
   before it merges (rule 2). A faithful replacement needs none.

Data works the same way: `ctype` (and `count` for arrays) on a `[[data]]` entry
yields a typed lvalue or pointer macro at the literal address. When a block of
globals becomes ours the macro becomes a real variable, again with no call-site
churn — and note the class of cutover bug waiting there
([`prior-art/openrct2.md`](prior-art/openrct2.md) §3, aliased globals).

`tools/gen_symbols.py` rejects a name or an address bound twice, an address
outside the image, an `impl` without a signature, and an `impl` path that does
not exist.

### Hazard: our code runs on 16 KB coroutine stacks

Game logic does not run on the process's main stack. It runs inside one of four
cooperative tasks, each on a **0x4000-byte stack** carved out of a static arena
(`Task_Create` `0x5A9914`, [`attract-mode.md`](attract-mode.md) §2), switched by
a hand-written `esp` swap. Any function of ours that replaces game logic
inherits that stack, and there is no guard page below it — an overflow silently
corrupts the next task's stack or the data under the arena.

So: no large locals, no deep recursion, no `alloca`, and be suspicious of
library calls with big frames (`printf`-family formatting is the usual
offender; `hook/log.cpp` keeps its buffer at 1 KB for this reason). A function
that calls `Task_Sleep` is also *resumed* later with its stack intact — that
works for compiled C++ exactly as it does for Capcom's code, provided nothing
of ours holds a lock or an RAII guard across the yield.

### Not built yet, deliberately

- **Non-`__cdecl` conventions beyond what the compiler offers.** `cc` accepts
  `__stdcall`/`__fastcall`/`__thiscall`. MSVC6 functions that take arguments in
  arbitrary registers (whole-program-optimised leaves) will need a
  register-struct thunk as OpenRCT2 had; build it when the first one is met,
  against that real case.
- **The `*` flag** ("not ours, but our code already calls it") and a progress
  report. Both fall out of the generator once there is more than one module.
- **Pushing names back into Ghidra.** Phase 1.

## 4. Building

One toolchain, on purpose ([`prior-art/tr1x.md`](prior-art/tr1x.md) §2.8): the
way never to grow an MSVC dependency is never to have an MSVC path. **llvm-mingw,
i686 target**, C++20, no modules. 32-bit is not a preference — the DLL lives in
an i386 process until the last original instruction is gone
([`prior-art/openrct2.md`](prior-art/openrct2.md) §2.2).

```
cmake --preset i686
cmake --build build
build/bof3x-launcher.exe --game bof3              # settings dialog, then the game
build/bof3x-launcher.exe --game bof3 --no-config  # straight to the game
```

The launcher shows a settings dialog before starting the game
([`launcher-settings.md`](launcher-settings.md)); `--no-config` skips it, which
is what scripted and agent runs want.

Needs `i686-w64-mingw32-clang++`, `cmake` ≥ 3.25, `ninja` and `python` ≥ 3.11 on
`PATH` (or `LLVM_MINGW_ROOT` set). Verified with llvm-mingw 20260616 / clang
22.1.8, cmake 3.31.12, ninja 1.13.2. The build reads no game data; outputs land
in `build/`, which is gitignored. The DLL is linked `-static` so that it depends
only on system DLLs — it is loaded by path into a process whose search order we
do not control.

[`PLAN.md`](PLAN.md) phase 0 originally said "clang-cl and MSVC both green".
That was written before the TR1X note; this document supersedes it, and
`PLAN.md` has been updated.

Log: `build/bof3x.log`, next to the DLL, rewritten each run.

## 5. The exit test

> One function of Capcom's binary replaced by one function of ours, under a
> non-MSVC compiler, with the game still running. — `PLAN.md` phase 0

**Passed 2026-09-19.** Target: `File_Read` `0x5A7470`
([`asset-loading-path.md`](asset-loading-path.md) §1) — eleven instructions, five
callers, every one of them an asset loader, and its effect is checkable in
bytes.

| Run | Log | Result |
|---|---|---|
| default | `inject ON File_Read original 0x005A7470 -> ours`, then `first call File_Read(handle=0, size=1176679)` | **1,176,679 is the exact size of `DAT/FIRST.DAT`** — the game's first whole-file read was served by our function. Process alive with 5 threads 28 s later, memory growing (52.0 → 53.5 MB). |
| `BOF3X_ORIGINAL=File_Read` | `inject OFF File_Read ours -> original 0x005A7470`, no `first call` line | Same process shape, Capcom's code serving the reads. |

Compiled `File_Read` was checked by disassembly (`objdump -d build/bof3x.dll`):
it pushes `File_Slots[handle]`, `size`, `1`, `dst` and calls `0x5B9D4E`,
preserving `ebx`/`esi`/`edi`.

What the test did **not** establish: that the screen looked right. It was run
headlessly by an agent watching the log and the process list; nobody watched
the picture. Byte-for-byte that cannot matter for this function, but the first
replacement with a visible effect needs eyes.

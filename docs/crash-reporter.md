# The crash reporter

**Status:** DONE (2026-09-19) — self-tested, and caught its first real crash in play the same day (D4's reproduction)

[`IDEAS.md`](IDEAS.md) I11, asked for by the owner after the first crash seen
in play ([`known-defects.md`](known-defects.md) D4). Code:
`src/hook/crash.cpp`, `tools/crash_report.py`.

## 1. What it does

Always on; nothing to enable. When the game raises a fault — access violation,
in-page error, illegal or privileged instruction, stack overflow, integer
divide by zero, and a few rarer codes — `bof3x.log` gets, prefixed `CRASH n:`:

- the exception code and address, as `module+offset`; for an access violation,
  whether it was reading, writing or executing, and the target address;
- all registers and the sixteen bytes at `eip`;
- where the game was: area word, task 0's state, the top-level mode and step,
  the message index;
- every word in the first 8 KB of stack that points just after a `call` inside
  `BOF3.exe` or `bof3x.dll`. **A scan, not a walk** — the game's code has no
  frame pointers and its task stacks are hand-switched — so stale return
  addresses from earlier calls can appear among the live ones;

and a minidump, `build/bof3x.crash-<pid>-<n>.dmp`, with every module's data
segments — the game's globals, which is what D4 was diagnosed from. The log
already says, at the top, which functions were ours in that run, and (while
`File_Open` is ours) every file opened.

It **observes and never handles**: the handler always returns
`EXCEPTION_CONTINUE_SEARCH`, so the game's own handlers, the crash itself and
Windows Error Reporting all proceed as they would without it. At most four
reports per process.

## 2. Why it is built the way it is

- **A vectored handler, not `SetUnhandledExceptionFilter`.** Game logic runs on
  16 KB task stacks carved from a static buffer
  ([`attract-mode.md`](attract-mode.md) §2), outside the thread's TEB stack
  limits, and Windows does not dispatch frame-based handlers — the unhandled
  filter among them — from such a stack. Vectored handlers run before that
  check. This is reasoning from how Windows dispatches exceptions, **not yet
  observed**: neither test so far faulted on a task stack (D4 was on
  WinMain's).
- **The work happens on another thread.** The handler runs on the stack that
  faulted, which may be small or may be what broke. It copies the exception
  record and context into static storage, wakes a reporter thread created at
  start-up, and waits at most 20 s for it. `dbghelp.dll` is loaded by that
  thread at start-up, not under the loader lock and not during a crash.
- **Everything it reads, it reads through `ReadProcessMemory`** on its own
  process, so a report about corrupted memory cannot itself fault.
- A first-chance handler also sees faults the game goes on to handle. None has
  been seen; if one appears it costs a log entry and a dump, nothing else.

## 3. Reading a dump

```
python tools/crash_report.py                         # newest build/bof3x.crash-*.dmp
python tools/crash_report.py FILE.dmp
python tools/crash_report.py FILE.dmp --u32 0x929EDC 0x937F84
python tools/crash_report.py FILE.dmp --read 0x903880 0x120
```

Names come from `symbols.toml`, else the nearest entry in
`analysis/pc_funcs.json` plus an offset. It reads WER's dumps too
(`%LOCALAPPDATA%/CrashDumps`); those carry almost no `.text`, so code bytes
are taken from `bof3/BOF3.exe`, which is sound only because the image is
`/FIXED`. On the D4 dump it gives `0x0059EE50+0x3FF`, reading `0x00080000`,
called from `0x004FCB00+0x374` — the same answer reached by hand.

**Dumps are game-derived** — code and data of `BOF3.exe`. `build/` is
gitignored; do not attach one to an issue (CLAUDE.md rule 1).

## 4. Testing it

`BOF3X_CRASH_TEST=<seconds>` makes a thread of ours read address `0x10` that
long after start-up. Run 2026-09-19 with 8: eight `CRASH 0:` lines and an
8.7 MB dump, which `crash_report.py` reads back as an access violation reading
`0x00000010` at `bof3x.dll+0x2FDC`.

## 5. Open

- ~~Attract oracle~~ — **passed 2026-09-19**: all ten functions ours, reporter
  armed, tracer off, against the all-original `orig_a.tsv`: identical `Rand`
  count, message index and area word at all 7,478 frames; first attract frame
  1,311 in both; no `CRASH` line in the log.
- First real catch, 2026-09-19 17:06: D4 reproduced; report and dump complete,
  and `crash_report.py` on the dump matches the log. That fault was on
  WinMain's stack.
- A fault on a task stack, to turn §2's first point from reasoning into
  observation.
- The logic frame number is only known while the call tracer is armed.

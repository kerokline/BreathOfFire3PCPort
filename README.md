# Breath of Fire III — PC Port

A **living-game** renovation of the 2001 Chinese PC port of *Breath of Fire III*:
fix its bugs, modernise its platform, and get to source you can actually change.

This is a sibling to [**BreathOfFire3Recomp**](https://github.com/kerokline/BreathOfFire3Recomp),
which is the *archival* project — a static recompilation of the PlayStation
release whose correctness criterion is "matches original hardware". The two
projects have deliberately opposite invariants, and that is what makes them good
collaborators rather than competitors. See [`docs/PLAN.md`](docs/PLAN.md) §6.

> **Status: scoping.** There is no code yet. [`docs/PLAN.md`](docs/PLAN.md) is
> the whole project right now — target analysis, architecture options, and a
> phased path. The load-bearing experiment (a PSX↔PC function matcher) has not
> been run, and the plan is contingent on it.

## What the target is

The shipped `BOF3.exe` (2,584,576 bytes, 2001-04-18), measured:

- 32-bit x86, MSVC 6.0, no packing, fixed image base `0x400000` — **native code,
  there is no CPU to emulate**
- ~590,900 instructions in `.text`, ~3,276 distinct direct-call targets
  (order 3–5k functions)
- **101 imported symbols across 7 DLLs** — DirectDraw, DirectSound, DirectInput,
  MCI, user32/gdi32, and the MSVC6 CRT. The entire host platform to reimplement
  fits on one page.
- Heavy function-pointer dispatch: recursive descent from the entry point
  reaches only 309 functions, so symbol recovery needs seeding, not just a
  disassembler.

## The approach

Not static recompilation, despite the sibling project's name. Static recomp
preserves behaviour *by construction* — it is an archival technique, and this
project's deliverable is deliberate divergence.

Instead, **incremental decompilation into a hybrid binary**, the OpenRCT2 /
OpenLoco model: load the original executable in-process, replace one function at
a time with readable C++, delete original code paths as subsystems complete. The
game stays playable at every commit; when the last function is replaced, the
original binary is no longer needed.

## Prior art this builds on

- [**bof3ext**](https://github.com/TheRealBiggs/bof3ext) by TheRealBiggs — a
  replacement `ddraw.dll` that already translates most of the game to English,
  fixes bugs, and replaces the renderer with OpenGL. It is the injection
  substrate this project starts from, and its `src/bof3/*.ixx` files are a
  partial symbol table for the PC binary.
  That repo currently ships no `LICENSE` file, though its author licenses his
  other work MIT — see [`docs/PLAN.md`](docs/PLAN.md) §7. Nothing from it is
  vendored here until the file exists.
- [**bof3ext_resources**](https://github.com/TheRealBiggs/bof3ext_resources) —
  360 files of translated text, fonts, and HD textures.
- [**BreathOfFire3Recomp**](https://github.com/kerokline/BreathOfFire3Recomp) —
  the archival sibling, and the source of ~30k mapped functions plus the
  reverse-engineering corpus that this project hopes to transfer names from.

## Legal

You need a legally owned copy of the Chinese PC port. **No game data is
distributed here and none may be committed** — see [`.gitignore`](.gitignore).

This repository is licensed under [PolyForm Noncommercial 1.0.0](LICENSE).
It is an independent interoperability and preservation effort, not affiliated
with or endorsed by Capcom.

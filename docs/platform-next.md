# What is next for the platform round: the choices, staged

**Status:** PROPOSAL (2026-10-06, after step 3 - [`platform-round-2.md`](platform-round-2.md);
the owner asked for the choices to be staged, naming the music / MP3
investigation, the single-source game data rework and the PSP logic
investigation). Nothing here is scheduled until the owner picks; what is
picked moves to [`STATUS.md`](STATUS.md)'s order of work and
[`HANDOFF.md`](HANDOFF.md).

Where the platform stands: every function of the game's own code and of the
platform layer is ours, and the runtime's entries the game calls are ours or
the toolchain's. What still runs of Capcom's in `BOF3.exe` is the runtime's
start-up, allocator and per-thread data, the MP3 decoder, and the software
renderer's path nothing reaches ([`crt-rest.md`](crt-rest.md) section 3,
[`platform-layers-plan.md`](platform-layers-plan.md) section 2). The plan's
remaining steps are 4 (the music, which decides the decoder) and 5 (state 2
proved, then our own executable).

## The four candidates

### A. The music investigation, then the decoder's fate (I23; the plan's step 4)

- **What it is.** Hear the PC's MP3s against the disc's sequenced music, then
  decide what section 2.4 becomes: a permissively licensed MP3 decoder behind
  the seam that is already ours (`Music_OpenDecoder`, `Mp3_Decode`,
  `Mp3_Seek`), or sequenced music from the disc (a SEQ/VAB player on an
  SPU-accurate synth - the largest item on the disc-only path), or both as a
  choice.
- **What exists.** The method, written and unrun
  ([`bgm-comparison.md`](bgm-comparison.md) section 3): inventory both sides
  (`ffprobe` the 166 MP3s; the disc's 81 `BGM*.EMI`; the exe's track table
  beside `BGM\%03d` at `0x666FB8`), pick three tracks, the loop test, the
  timbre test against an emulator render, then the owner listens. The seam
  on the calling side is ours; `Mp3_Create` / `Mp3_Destroy` and the decoder's
  `calloc` / `free` are what the CRT group left for the cutover.
- **First step.** The listening set: three tracks rendered both ways, aligned,
  for the owner's ear (an afternoon of tooling, headless; the listening is the
  owner's). It also answers I32's P10 (instrumentation differs on the PSP?)
  if the PSP's sequences are rendered beside.
- **Cost after the decision.** The decoder swap is one group: a decoder
  chosen ([`LICENSING.md`](LICENSING.md) section 4: no copyleft; its notice in
  [`THIRD_PARTY.md`](THIRD_PARTY.md)), a ledger entry with the PCM error bound
  per track and the loop seam measured, the state hash unaffected (the staging
  buffer is in its skip list). The sequencer is phase-5 work (section 8 item
  8 of [`ASSET_SOURCES.md`](ASSET_SOURCES.md)), months not days.
- **Needs from the owner.** Their ear, at the machine, for the listening set;
  then the decision. **Unblocks:** state 2 (no Capcom code runs), and so the
  cutover.

### B. Single-source game data: the region measurement, then the importer (ASSET_SOURCES)

- **What it is.** The game built from whatever copies the player owns - the PC
  install, one or more PSX discs, or a mix - through one importer into one
  canonical cache the engine reads, with a language-neutral base and
  per-language layers ([`ASSET_SOURCES.md`](ASSET_SOURCES.md) sections 1, 3,
  6). `tools/loc_build.py` is the prototype for one language.
- **What exists.** The identity layer (`fixtures.toml`, `verify_fixtures.py`);
  the census pairing PC against JP (`dat_census.py`); the language overlays
  (DIV-0005 and after); the plan's staging (section 8). Not done of the cheap
  items: the `DAT/` tree hashed into `fixtures.toml`, per-file hash tables
  for each held disc, logical ids on the assets whose loaders are ours (which
  is now all of them).
- **First step.** The phase 4 measurement the plan already schedules: diff
  each held Western disc against the JP disc section by section the way the
  census pairs PC against JP, and sort every difference into text, layout or
  logic. It decides whether the base is one tree or has per-language
  exceptions. **Headless, no owner time, no code of ours at risk**; a round of
  two or three agents (the tool, the PSX pairs, the write-up), with the two
  PSP pairs through the same tool for C.
- **Cost after.** The importer, one build at a time (JP first, then US), the
  recipe format decided (TOML or generated JSON, section 9); the exe-resident
  tables from each SKU's boot EXE; disc-only boot last. Phase 5 by the plan.
- **Needs from the owner.** Nothing for the measurement; later the recipe
  format and which SKUs to support first. **Unblocks:** I2's selectable
  localisations in general, I32's data toggles (a PSP data change is a cache
  layer), and the disc-only build the licensing path wants.

### C. The PSP release's logic changes (I32)

- **What it is.** Find what the PSP release changed in behaviour and content,
  catalogue it, offer what the owner wants as a toggle. Sixteen leads listed,
  one measured (the widened view), none of the behaviour rows confirmed
  ([`IDEAS.md`](IDEAS.md) I32's tables).
- **What exists.** Both PSP discs held and catalogued; the ELF readable (a
  native MIPS port, read once for the widescreen); one section known
  byte-identical to the JP disc's; the sibling's PSX name corpus to pair PSP
  functions with their PSX originals; `psx-twin-check.md`'s method for reading
  a twin.
- **First step.** The data half rides on B's measurement (the two PSP pairs
  through the same diff). The code half is its own read: the PSP ELF against
  the PSX code function by function, widescreen and platform set aside -
  large, and the only way to confirm P1..P5. Start with the leads that have a
  head start: P6 / P7 (the fused boss's palette and attack name, which the
  `balioAndSunder_2` route may reach), P2 / P3 / P4 (field, menu and
  encounter code, all ours on the PC side, each with a named PSX counterpart
  to read the PSP's against).
- **Cost.** The data half is nearly free once B's tool exists; the code half
  is a reading round of several agents, with no fuzz to lean on (the PSP's
  code runs nowhere here) - evidence is the read, cross-checked against the
  PSX. A toggle built on a code change is a branch in our code under one
  option; a default is a ledger entry.
- **Needs from the owner.** Which confirmed changes become options, and which
  (if any) the default. **Risk:** the premise is a recollection and a fan
  wiki; the count of confirmed rows may be small.

### D. The cutover: state 2 proved, then our own executable (the plan's step 5)

- **What it is.** State 2: the tracer armed on every start that is not ours,
  over the attract sequence and every route, records nothing after the entry
  point's hand-off - "we own every call" as a measurement. State 3: a 32-bit
  process of ours that maps the player's `BOF3.exe` data sections at their
  linked addresses and never maps its code; the game's tables stay the
  player's file (the engine / data split the licensing path rests on).
- **What exists.** The design in [`platform-layers-plan.md`](platform-layers-plan.md)
  section 3; the CRT group's left table (what the executable still provides:
  start-up, the allocator, per-thread data, `atoi`, `sscanf`, `_ftol` - each
  small once the decoder is gone); the tracer and the state hash.
- **First step.** Two pieces that need no decision: the state hash made
  address-independent where the game's tables point into our image (section
  3's note - small, and wanted before state 3), and a design doc for the
  state-3 executable (the image's sections mapped, the `/FIXED` base, the
  imports still named, the launcher's role). Both headless.
- **Cost.** State 2's proof is a day once A has removed the decoder; state 3
  is a round of its own with the biggest live check yet (every route in the
  new process against the old).
- **Needs from the owner.** Nothing until state 3's shape is decided (the
  owner has named it the first goal of the phase 4 / 5 work). **Blocked on:**
  A's decision, because state 2 is defined with the decoder gone (the owner,
  2026-10-05).

## How they depend on each other

```
A (listen) ──decides──▶ A (decoder swap or sequencer) ──▶ D state 2 ──▶ D state 3
B (region diff) ──feeds──▶ C data half ──▶ C toggles (data)
                 └──────▶ B importer (phase 5)
C code half: independent, large
D prep (hash address-independence, the state-3 design): independent, small
```

## Three ways to stage it

1. **Owner's ear first, tooling beside it (recommended).** This week: A's
   listening set built headless and handed to the owner; B's region
   measurement with the PSP pairs as one agent round (headless, ~a day); D's
   two preparatory pieces in the same round. The owner listens when at the
   machine and decides the decoder; the next round is the decoder swap and
   state 2's proof, with C's data half read from B's output. C's code half
   after, as its own round, sized by what the data half found.
2. **The cutover as fast as possible.** A's listening set and the decoder
   swap first (choose the permissive decoder now, the sequencer stays a
   phase-5 option), then state 2, then state 3. B and C wait. Gets to our own
   executable soonest; defers everything the data side unblocks.
3. **Data first.** B's measurement and importer before any of the rest, C's
   data half with it. The platform stays hosted longer; the disc-only build
   and the language work arrive sooner.

The recommendation is 1 because its first round needs nothing from the owner
but an afternoon of listening, runs headless beside them, and no choice in it
is one the project has not already made.

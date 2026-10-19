# The quicksave round: save states behind F5, F6 and F7

**Status:** RUNNING (staged 2026-10-17; stage A launched 2026-10-19 off
`19180e63` on the integration branch `quicksave/round`, beside the typed-data
round's naming work, the owner's call. The owner's design of 2026-10-17, on
IDEAS I13 and I18.)

The owner (2026-10-17): F5 opens a quicksave menu that looks like the save
menu, so its draw functions are reused, and a quicksave is picked there to
load; F6 quicksaves to quicksave slot 0; F7 quick-loads from it. The files
take the saves' own naming from hex 10 up, `BISLPS10.DAT`..`BISLPS1F.DAT`.
**A quicksave is a full memory snapshot**, restored to the exact moment,
battles and events included: F12's save-anywhere "leads to strange
conditions" and is not the model. **The save file is cheap, so a quicksave
carries one too**, at least the part the menu draws (level, play time, the
party).

## 0. The shape in one paragraph

Between two logic frames, in `WinMain`'s loop, every task of the game is
suspended on a stack that lives inside the image, and the main thread holds
nothing but the loop's locals. At that point the game's state is the
writable image (`.rdata`, `.data`, `.bss`, the draw pool: `0x5C4000..0x987000`
in the exe host, [`state3-design.md`](state3-design.md) 1.2) plus a short
list of things outside it that the state hash's skip list already names
([`tools/statehash_skip.txt`](../tools/statehash_skip.txt)): the display's
texture caches, DirectSound's buffers, the music stream, the window and the
clock. A quicksave copies the image span and asks each **participant** (a
module owning state outside the span) for a record; a quick-load puts the
span back and asks each participant to restore from its record, rebuilding
what cannot be copied (textures from the VRAM shadow, sound buffers from
the banks, the music from its track and position). The file starts with an
ordinary save block built by `Save_BuildBlock`, so `Save_ReadSummaries` and
`SaveMenu_DrawSlots` draw a quicksave as they draw a save.

The proof is the state hash: run to a tick, quicksave, run 300 frames,
quick-load, run 300 frames; the two hash sequences are equal tick for tick.
Then the same across two processes. A difference names what the snapshot
missed. That is I13's first experiment, and it is the acceptance test of
every group below.

## 1. What is already known, and the one trap

> **Corrected by SC's census (2026-10-19, [`quicksave-census.md`](quicksave-census.md)).**
> Two premises below are false as staged. (1) The task stacks are not in the
> image: `Task_SetStackBase` hangs four `0x4000`-byte slices below the main
> thread's esp (`[Task_StackTop-0x10000, Task_StackTop)`), so SN copies them
> as a second span and runs the take and restore on a stack of their own;
> the skip row `0x66C854` holds only the scheduler's words. (2) Our code is not
> at a fixed address in the exe host: `bof3x.dll` is `DYNAMIC_BASE` and was
> measured loading at different bases from two directories; the file records
> the DLL base and refuses a mismatch. The owner chose a fixed link base
> (2026-10-19): SN links `bof3x.dll` without `DYNAMIC_BASE` at a set base,
> keeping its relocations so a taken base still loads (and the check refuses).
> Also: the music stream has no thread of its own (`Music_Pump` runs from
> `Sound_Tick` on the main thread), so nothing is paused around a restore.

- **The image is one flat span at a fixed base** and holds the task stacks
  (`0x66C854`, 37,292 bytes, the skip list's "task stacks") and the
  scheduler's saved stack pointers: a copy between frames captures every
  suspended task as plain data. The saved pointers into the main thread's
  stack (`Task_SchedulerEsp`) are the one thing in the span that belongs to
  the process: the restore writes the current process's values over them.
- **Return addresses on the task stacks point into our code.** In the exe
  host our code sits at a fixed address (state 3's image has no
  relocations), so they hold across launches **of the same build only**. A
  quicksave records the build (the launcher's version block and the file's
  hash, [`releases/README.md`](releases/README.md)) and a mismatched build
  refuses the snapshot half, loudly, while the save half stays loadable
  (section 3). In the hosted build (`bof3x.dll` in `BOF3.exe`) the DLL's base
  is the loader's choice, so a quicksave there is in-process only.
- **The heap.** `malloc` / `free` are the toolchain's since the C runtime's
  replacement ([`crt-rest.md`](crt-rest.md)), so heap addresses differ between
  launches and the heap is not copied. What game code holds on the heap is
  measured first (SC, section 2); the design assumes each such object can be
  rebuilt from what the image says (a sound bank from its DAT, a stream
  from its track) rather than copied. If SC finds a heap block that holds
  logic state with no source to rebuild from, I13's fixed-address arena
  becomes a step of SN; nothing here is designed on that until it is seen.
- **The display is rebuilt on demand.** Textures come from the VRAM shadow
  inside the span (`Gfx_InvalidateTextures` over the whole shadow,
  `Gfx_ConvertRow` for the palette rows), and the caches that hold surface
  pointers are listed to the byte in the skip list (`Gfx_TexCache`
  `0x6C3F40`, the 82 `Font_TexCache` records, `0x6BE1D0`, `0x6C3A40`).
- **The trap: the ordinary save list would eat the quicksaves.**
  `Save_ListFiles` globs `BISLPS??.DAT` into a **16-entry table with no
  bound** (a seventeenth file writes past it towards `Save_Staging`,
  [`save-files.md`](save-files.md) 2), and `Save_ReadSummaries` takes a
  file's slot from `name[7]` as **one hex digit**, so `BISLPS10.DAT` would
  read as slot 0. Ours therefore lists `BISLPS0?.DAT` for the saves and
  `BISLPS1?.DAT` for the quicksaves, each into its own table, with the
  functions taking the pattern and the tables as arguments and the
  original's callers passing the original's. A ledger entry (draft B) and a
  defect fixed on the way (the bound).

## 2. Stage A (three groups, in parallel)

Each group in its own worktree off `main`, `git commit -s`, headless
self-tests, a hand-back report; harness roots per group
(`C:\Users\kerok\q_<group>`).

### SN: the snapshot core (`src/hook/snapshot.{h,cpp}`, `src/game/win_main.cpp`; merges first)

- **The point.** In the loop, after `Task_RunAll` and `Gfx_LinkOTags` and
  the frame count, before the next frame's draw: `Snapshot_Pending()` runs a
  take or a restore that `WndProc` or the menu asked for. Never from
  `WndProc` itself (a key arrives mid-frame).
- **The span.** `0x5C4000..0x987000` in the exe host (the hosted build: the
  same `.rdata` / `.data` / `.bss` span of `BOF3.exe` plus the DLL's draw
  pool, by its own extent), copied whole. On restore: the span written back,
  then the process-owned words fixed up (`Task_SchedulerEsp`, the window's
  `Game_Hwnd` / `Game_HInstance`, `Frame_Deadline` left as the loop's),
  then every participant's `Restore`.
- **Participants.** A registry: `Snapshot_Register(tag, Save(writer),
  Restore(reader))`, each module owning state outside the span registering
  once at start-up. SN ships the registry and the three fix-ups above; SC
  says which modules must register; SP-A and SP-G build the two large ones.
  A restore with a record whose tag has no participant, or a participant
  with no record, is refused (rule 4: no silent partial restore).
- **The file** (`BISLPS1X.DAT`): bytes `0..0x12AF` an ordinary save file
  (`Save_BuildBlock`'s `0x12B0` bytes, the summary at `0xCA0` where the
  menu's reader looks); then a header (magic, format version, the build id,
  `Frame_Counter`, the span's base and length, the participant count); the
  span; the participants' records (tag, length, bytes). Raw, about 4 MB a
  file; a zero-page skip if it matters (measured, not assumed). The save
  half is what the quick menu's summary draw reads; it also keeps a later
  "load as a plain save" possible without a format change, though the title's
  load menu never lists these files (section 5, answer 2).
- **The keys.** F6: **the quicksaves rotate, then a take into slot `0x10`**
  (the owner, 2026-10-17): `BISLPS1F.DAT` is deleted, `1E` renamed to `1F`,
  down to `10` renamed to `11`, and the new quicksave written as `10`, so
  the sixteen slots hold the sixteen most recent and F7 always loads the
  newest. Renames, not rewrites; the rotation runs top down, so a rename
  that fails stops the take with a refusal line and leaves every file
  present under some name, nothing half-rotated. The overlay's
  "Quicksave OK" line through the same `Overlay_Text` path as F12's; F7:
  restore from `0x10`, "Quickload" or a refusal line naming why (no file,
  another build). F5 is QM's (stage B). F7 is free since DIV-0040 made it
  nothing.
- **Self-test:** a span copy, a scribble over game pages, a restore, the
  pages equal; a participant missing on either side refused; a build-id
  mismatch refused with the save half still parsed; the rotation on a
  directory of sixteen named files (each moves up one, `1F` gone, `10` new)
  and on one with gaps in the sequence.
- **Ledger draft A:** the quicksave keys and files. (F12 is unchanged, section 5.)
- Output: `docs/quicksave.md` (the format, the point, the registry).

### SC: the census of state outside the span (reads; no `src/`)

Every mutable namespace-scope or `static` object in `src/game`, `src/render`,
`src/audio`, `src/input` and `src/hook` that can hold a value across frames,
classed:

- **per-frame**: rebuilt every frame from the span (needs nothing);
- **configuration**: the launcher's and the ini's (survives a restore
  unchanged by design: window size, filter, wide mode, layering on);
- **game state**: must be in a participant's record, or is a defect of the
  design (examples to check: DIV-0071's layering caches, `field_frame.cpp`,
  `map_layers.cpp`'s inset, the language overlay's tables, DIV-0028's fade,
  the random split of DIV-0041, `BOF3X_SPEED`'s scripted speed, the music
  sequencer in `src/audio/seq.cpp`, minimp3's decoder state in `mp3.cpp`,
  the WAV dump's and the band census's instruments).

And the heap: every `malloc` the game's own code reaches at run time
(`Sound_Banks`' buffers, the music's file image `0x6BDE44`, `Music_Decoder`,
whatever `LoadDatFile`'s callers keep), with the pointer in the span that
holds each block and what the block can be rebuilt from. The method: the
skip list's "noise" rows as the first list, a read of each allocator call
site (`crt-rest.md` 4's table), and a run with `BOF3X_STATEHASH` from a
restore against a live run to catch what the reading missed.

Output: `docs/quicksave-census.md`, a table one row per object: module,
class, participant needed, rebuild source. SP-A and SP-G take their lists
from it; a "game state" row with no participant at the end of stage B is a
finding, not a shrug.

### RC: the routes and the proof batch (`tools/recipes/`, `tools/`)

- `quick_<route>.txt` variants of six existing routes, each pressing `f6`
  at a chosen tick and `f7` 300 frames later (the input script knows
  `f5`..`f12`, `src/hook/input_script.cpp`): a field walk
  (`worldMapAndAreaTransition`), a battle mid-turn (`combat`), an event
  (`cutsceneAndNue`), the world map, a shop (`shop`), a music fade
  (`musicStopCheck`). Built with `tools/recipe_shots.py` where shots are
  wanted.
- `tools/quicksave_check.py`: runs a route with `BOF3X_STATEHASH`, splits
  the hash log at the `f7` tick, and reports the first tick after the
  restore whose hash differs from the tick 300 frames earlier (the in-process
  proof); and the cross-process form: a run that starts, quick-loads the
  file at its first frame, and continues, against the live run from the
  same tick.
- The references: the six routes live, identity, from an ini-less launcher
  copy with `BOF3X_OPT=none BOF3X_LAYERING=0` (the state hash's conditions)
  and once with the player's defaults.
- Output: the recipes, the script, `docs/quicksave-routes.md`.

## 2a. What changed on `main` between staging and launch (2026-10-19)

Checked against `581e35a8..19180e63`:

- **The button round (PR #85) added input state** outside the span:
  `src/input/sigma.cpp` (the unified button map) and `src/game/shoulder_glyphs.cpp`.
  SC classes both; σ's map is configuration if it is rebuilt from the install
  and the ini alone, game state if anything in play changes it.
- **DIV-0143** (issue #88): an area change keeping a track that is fading to
  a stop keeps none. A fade-to-stop in progress is one of SP-A's saved states,
  and RC's `musicStopCheck` route covers it.
- **Routes differ by release** since PR #85 (`harness/recipe-buttons-by-release`,
  [`input-script.md`](input-script.md) 2a): on a PSX-West or PSP-EU install
  confirm is cross. RC's quick routes take the per-release form.
- **`src/hook/gapwatch.cpp`** (the install-gap round's read watch) is
  tooling, off by default; SC lists it as such.
- The jukebox's recorded streams (PR #92) play in the launcher only; nothing
  in the game to save.
- **Beside this round:** the typed-data round's naming and census work runs
  in parallel ([`typed-data-round.md`](typed-data-round.md)). It renames reads
  and adds no behaviour; a merge between the two is a rename conflict, taken
  with both sides' meaning.

## 3. Stage B (after SN merges; three groups in parallel)

### SP-A: the audio participant (`src/game/sound.cpp`, `src/audio/`)

- **Sound effects.** `Sound_Banks` (`0x6BC928`, 5,400 bytes of records with
  DirectSound buffer pointers) and `Sound_Channels`: on restore the buffers
  are re-created from the banks' wave data the way `0x5A69E0` makes them,
  the channels' playing voices restarted or silenced (a one-shot effect cut
  by a restore is not a defect; a looping one, the record says, is restarted).
- **Music.** The track, loop state, the decoder's position in the stream
  (minimp3 is ours, its state in `src/audio/mp3.cpp`), the sequencer's
  position and voices for disc music (`seq.cpp`), the volume and DIV-0028's
  fade, DIV-0132's duck: saved as numbers, restored by reopening the track
  and seeking, then resuming. `Music_Staging` (`0x7CC378`, 73,728 bytes, the
  stream's own thread) is excluded from the span copy and refilled by the
  resume; the stream thread is paused around the restore.
- Self-test: a fade in progress saved and restored keeps its step and count;
  a track at a known sample position resumes within a buffer's length
  (measured with `BOF3X_WAV_DIR`, `tools/wav_diff.py`).
- Output: a section of `docs/quicksave.md`.

### SP-G: the graphics participant (`src/render/`, `src/game/layering.cpp`, `field_frame.cpp`, `map_layers.cpp`)

- The texture caches named in the skip list cleared on restore and the
  shadow invalidated whole; the Direct3D 11 backend's own caches
  (`render_d3d11.cpp`) likewise; `SatPixie`'s and the frame grab's
  (DIV-0125) state per SC's class.
- DIV-0071's layering, the field frame and the map layers: whatever SC
  classes as game state, saved; the rest rebuilt on the first frame after
  a restore.
- Self-test: a restore followed by one frame draws the same primitives as
  the live frame (the frame hash, [`call-trace.md`](call-trace.md) 6).
- Output: a section of `docs/quicksave.md`.

### QM: the F5 menu and the two lists (`src/game/save_menu.cpp`, `rest_2c.cpp`, a new `quick_menu.cpp`)

- **The lists.** `Save_ListFiles(pattern, table)` and
  `Save_ReadSummaries(table, summaries)`: the original's callers pass
  `BISLPS0?.DAT`, `Save_Directory` and the sixteen summaries at `0x905BC0`;
  the quick menu passes `BISLPS1?.DAT` and two tables of its own, in our
  memory, outside the span (they are a listing, not game state). The bound
  on the table is added (sixteen, the rest ignored and logged). `name[7]`
  stays the slot digit, `0`..`F` in both lists.
- **The menu.** Runs while `Game_Paused` is set, in the loop's paused branch
  beside the pause lines: `Menu_DrawBlackScreen`, `SaveMenu_DrawSlots` over
  the quicksave summaries (three shown, the cursor and first-shown words
  the menu's own, not `0x9036D4` / `0x8034D0` which are the game's),
  `SaveMenu_DrawChoices` for the row of choices; input through our
  `Pad_Read` seam each paused frame, since no task runs. Choices per slot:
  **Load** (a restore asked of SN, the pause ended) and **Back**; an empty
  slot offers Back only (section 5: load only, F6 the one way to save). F5
  opens it from the game; a second F5 or cancel closes it.
- **F12.** Unchanged, the owner's answer (section 5).
- Self-test: the two lists on a directory with saves, quicksaves and a
  seventeenth file; the menu's cursor over 0..15 with gaps.
- **Ledger drafts:** B (the lists split and bounded), D (the F5 menu).
- Output: a section of `docs/quicksave.md`; the recipes `quick_menu.txt`
  (F5, a slot, Load) for RC's batch.

## 4. Stage C: the proof batch, then the owner's play

The coordinator, unattended: RC's six routes in-process and cross-process
on the exe host from a US disc-only install and the PC install, under the
state hash's conditions and under the player's defaults; `quicksave_check.py`
reporting the first differing tick per route. A route whose restore differs
is a finding for SC's table (what was missed), fixed in its participant,
re-run. Then one capture per screen (the F5 menu with three quicksaves
listed, the overlay lines for F6 and F7, the title's load menu with a
quicksave's save half) for the owner by chat, and the owner's play of a
quicksave in a battle and in a cutscene at their return.

Merge order: SN; then SP-A, SP-G, QM in any order (SP-G before QM if the
menu's draw wants the texture participant); RC's recipes any time. After
each merge the build, `BOF3X_SHADOW='*'` headless, RC's in-process check on
one route.

Numbering: drafts A, B and D lettered (C dropped with F12 unchanged); the coordinator numbers from the first
free entry (DIV-0145 at launch, 2026-10-19; check `main`). The bound on
`Save_ListFiles`' table is a port defect fixed, a D-number from the first
free (D322 at launch). Agents number nothing. Every commit `git commit -s`; no save file's bytes in any
doc (rule 1: sizes, offsets and field names only).

## 5. The owner's answers (2026-10-17, by chat), and what is still open

Answered:

1. **F12 stays as it is** - the save-anywhere, unchanged. Draft C is dropped;
   I18 closes on this answer.
2. **The quicksave menu is reached from F5 only.** The title's load menu does
   not list `BISLPS1?.DAT`; a quicksave's save half is read by nothing but
   the quick menu's own summary draw (it still makes the file self-describing,
   and keeps a later "load as a plain save" possible without a format change).
4. **F6 overwrites slot `0x10` silently**, for now. No prompt.
6. **4 MB a file is fine for now**; shrinking it is logged as IDEAS I45, not
   this round's.

3. **Load only in the F5 menu**; F6 is the one way to save (the owner,
   2026-10-17).
5. **Sixteen quicksave slots** `0x10`..`0x1F`, kept as a history: **F6 pushes
   every quicksave down a slot** (`10` -> `11`, ..., `1E` -> `1F`, the old
   `1F` deleted) before it writes the new one to `10` (the owner,
   2026-10-17; F is the last hex digit, so sixteen). Three shown at a time
   as the save menu shows them, newest first.

Nothing of section 5 is open.

## 6. Not in this round

- Restoring into an all-original process for comparison (I13's second
  half): since state 3 Capcom's code is a separate process, and the
  snapshot's return addresses are ours. A later tooling entry, if wanted.
- The hosted build across processes (the DLL's base moves).
- A save-state format that survives a build change: the build id refuses,
  the save half remains. Versioning the snapshot beyond that is I33's
  "saves" row.
- Rewinding, slots on a timer, or a thumbnail in the menu: ideas for the
  IDEAS doc once the base works.

## 7. The brief, per group

Each brief names: the goal; the files it owns and must not touch; the docs
to read first ([`save-files.md`](save-files.md), [`save-menu.md`](save-menu.md)
1 and 2, [`rest_2c.md`](rest_2c.md) 1.2 for the save point's states,
[`state-hash.md`](state-hash.md) 3 and the skip list, [`state3-design.md`](state3-design.md)
1.2 and 5, IDEAS I13 and I18, `win_main.cpp`'s loop); its harness root;
the checks that mean done; the hand-back report's shape (what is saved and
restored, what is rebuilt, what SC's table still lists without a
participant, the drafts lettered, the captures' paths outside the tree).
Rules 1, 4 and 7 restated. A usage cut resumes with a message naming the
next step; only the hand-back report means merge.

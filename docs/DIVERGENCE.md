# Divergence ledger

**Status:** IN PROGRESS (opened 2026-09-18; 22 entries, DIV-0001..0022)

Every intentional behavioural difference between this project and the original
Chinese PC port gets an entry here.

## Why this file exists on day one

This is a **living** project: divergence is the deliverable, not a defect
([`PLAN.md`](PLAN.md) §6). That freedom has a well-known failure mode. Renovation
projects diverge gradually, nobody records which changes were deliberate, and
some years in nobody can say whether a given oddity is a fix, a regression, or
original behaviour that always looked wrong. At that point the original is the
only oracle left, and re-deriving intent from a decade of commits is miserable.

We are unusually well placed to avoid it. A provably faithful implementation of
this game sits in the next directory — the archival sibling,
[`BreathOfFire3Recomp`](https://github.com/kerokline/BreathOfFire3Recomp) —
which means **every behavioural difference here can be detected and attributed.**
That only pays off if the ledger is kept from the first change, which is why the
file exists before the code does. Retrofitting it means reconstructing intent
from memory, and the memory will be gone.

The governing rule, from [`CLAUDE.md`](../CLAUDE.md) rule 2:

> **Unledgered divergence is a bug.**

Not a style violation — a bug, closed by either writing the entry or reverting
the change.

## What counts

**Ledger it** when the game *does something different*: damage or formula
changes, altered encounter or drop rates, changed menu behaviour, different
text, new or removed content, fixed design bugs, changed timing that is visible
in play.

**Don't ledger** changes that preserve behaviour: refactors, a function
reimplemented to be byte-equivalent, performance work, build system, renderer
changes that produce the same image at the same time, or anything in tooling and
docs.

### What belongs somewhere else

There are **three** kinds of difference in this project and only one of them is
ledgered here:

| Category | What it is | Where it goes |
|---|---|---|
| **Port divergence** | The porting house changed it between the 1997 PlayStation release and the 2001 PC port. Not ours — history, not a decision. | [`SHARED_SOURCE.md`](SHARED_SOURCE.md) §3 |
| **Our divergence** | We changed it, deliberately. | **Here** |
| **Regression** | It changed and nobody meant it to. | A bug |

The first two must not be conflated. In five years, "why does this differ from
the PlayStation version?" needs to distinguish *Capcom's porting house did that
in 2001* from *we did that on purpose* — the first is an observation to record,
only the second is a decision to justify. This ledger is for the second.

The boundary case is worth naming, because it will come up constantly:
**a port bug fix is still a divergence.** The fullscreen fallback to 640×480 is
a defect, and fixing it is obviously correct — and it still gets an entry,
because five years from now "why does this not match the original?" deserves an
answer better than "we assumed it was a bug". Cheap to write, and the reason the
ledger stays trustworthy.

## Entry format

```markdown
### <short imperative title>

- **ID:** DIV-0001
- **Date:** YYYY-MM-DD
- **Subsystem:** text / battle / field / menu / platform / audio / render
- **Original behaviour:** what the shipped PC port does, and how that was
  established (the evidence rule applies — cite the measurement).
- **New behaviour:** what this project does instead.
- **Rationale:** why. If it is a fix, say what makes it a bug rather than a
  design choice.
- **Also in the PSX version?** yes / no / unknown — whether the archival sibling
  shows the same behaviour. This is what distinguishes a *port* bug from an
  *original* one, and it changes how confident the fix should be.
- **Reversible?** whether it sits behind a config toggle, and the key if so.
```

Assign IDs sequentially and never reuse them. A superseded entry stays, marked
`SUPERSEDED by DIV-NNNN` — the record of what was once believed is part of the
value.

## Differential testing

Where an algorithm exists in both binaries — damage formulas, encounter tables,
script control-code handling — it can be run head-to-head against the archival
build.

**One exception, established 2026-09-18: RNG sequences cannot.** The PSX calls
the BIOS `A0:2F` `rand()`; the PC port calls the MSVC6 CRT `rand()`. Same role,
different generator, different sequence
([`SHARED_SOURCE.md`](SHARED_SOURCE.md) §5). Everything downstream of a random
draw is still comparable, but only with the draw **injected rather than
generated** — otherwise the comparison is noise and the first differential test
fails for the wrong reason. These are the same algorithms compiled for two
architectures ([`PLAN.md`](PLAN.md) §3), so a mismatch is real signal rather
than noise.

Two distinct uses, and they should not be confused:

- **Before a change:** establish what the original actually does, so the
  "Original behaviour" field is measured rather than assumed.
- **After a change:** confirm the *only* differences are the ledgered ones.
  An unexpected mismatch is a regression, found immediately instead of in a bug
  report two years later.

Harness to be built in phase 2/3; this section records the intent so it gets
designed in rather than bolted on.

---

## Entries

### Re-encode `capcom.avi` from Indeo 5 to Cinepak

- **ID:** DIV-0001
- **Date:** 2026-09-19
- **Subsystem:** platform
- **Original behaviour:** the port plays `capcom.avi` through MCI — `open
  avivideo!%s alias vfw`, `play vfw window from 0 notify`, `stop vfw wait`
  (strings in `.rdata`; `WINMM.mciSendStringA` is the only `winmm` import).
  The file is Indeo Video 5 (`IV50`, 640x480, 15 fps, 113 frames) with a PCM
  22.05 kHz stereo track. **On Windows Vista and later there is no Indeo
  decoder**: `ir50_32.dll` is absent and `HKLM\SOFTWARE\WOW6432Node\Microsoft\
  Windows NT\CurrentVersion\Drivers32` has no `vidc.iv50` entry, Indeo having
  been dropped after XP and blocked by Microsoft security advisory 954157.
  The result is *not* an error — driving the game's exact MCI sequence from a
  32-bit host, every command returns success and the position advances to 113
  on schedule, while `mciavi32` draws its own placeholder: a hatched white field
  captioned "Video not available, cannot find 'vids:iv50' decompressor."
  Measured 2026-09-19; full method and screenshots in
  [`media-stack-survey.md`](media-stack-survey.md) §2.
- **New behaviour:** `capcom.avi` is the same video re-encoded to Cinepak
  (`cvid`), whose decoder `iccvid.dll` is registered as `vidc.cvid` and still
  ships with Windows. The audio stream is copied, not re-encoded, so it is
  bit-identical. Playback path, command sequence and timing are unchanged —
  **no code changes.** Verified playing through the same harness.
- **Rationale:** the alternative is a black-box error caption where the Capcom
  logo belongs, on every launch, on every supported version of Windows. This is
  the cheapest possible repair: a data substitution inside the port's own
  playback path, with no new dependency and nothing to maintain. Cinepak was
  chosen over Microsoft Video 1 on measurement — SSIM 0.9898 vs 0.9576 against
  the original ([`media-stack-survey.md`](media-stack-survey.md) §6).
  It is ledgered, rather than treated as a silent repair, because the player
  genuinely sees a different (lossily re-compressed) image, and because
  "why is the logo video not the shipped file?" deserves an answer.
- **Also in the PSX version?** No — not applicable. The PlayStation release has
  no AVI; its FMV is PSX `STR` streams decoded by the console's MDEC. This is a
  defect of the 2001 PC port's platform choices, not of the game.
- **Reversible?** Yes, by file substitution — the shipped original is kept
  alongside as `capcom.avi.indeo5`. Not behind a config toggle; there is no
  config system yet, and restoring the original restores a broken video.

  Identities, so the substitution is provable (`sha256`, 2026-09-19):

  | File | Size | SHA-256 |
  |---|---|---|
  | `capcom.avi` as shipped (now `capcom.avi.indeo5`) | 2,262,824 | `65ca9b150945e9973cd86492388f574d884603ce894300d6813f828e32df118a` |
  | `capcom.avi` as installed (Cinepak) | 4,609,908 | `460d25ce731c40d653286087b43655aca88aa661f2e3423b1c78590dc318f4f6` |

  Reproduce with ffmpeg 9.0.1:

  ```bash
  ffmpeg -i capcom.avi.indeo5 -c:v cinepak -c:a copy \
         -fflags +bitexact -flags:v +bitexact capcom.avi
  ```

  The output hash is tied to that encoder version; a different ffmpeg may
  produce a different — and equally valid — file.

### Refresh the save directory after writing a save

- **ID:** DIV-0002
- **Date:** 2026-09-19
- **Subsystem:** menu
- **Original behaviour:** a save written to a slot that had no file when the
  save directory was last listed **does not appear in the save menu** until the
  game is restarted; the file itself is written correctly. Reported by the
  owner 2026-09-19 (slots 0 and F, a New Game session with no prior saves) and
  traced the same day ([`save-files.md`](save-files.md) §3): the menu's state 0
  (`0x57FDE0`) calls `Save_ReadSummaries` `0x588DC0`, which rebuilds all sixteen
  slot summaries from the table `Save_Directory` `0x929F40` — but only
  `Save_ListFiles` `0x4548B0` fills that table, and its two call sites
  (`0x587E6B`, `0x588156`) are both outside the save flow. Measured on the
  running game by read-only `ReadProcessMemory`: both files on disk at 4,784
  bytes, `Save_Directory` entirely zero. After a restart both saves listed and
  one loaded (owner, original exe, no DLL).
  *Not yet established:* a reproduction of the vanishing save under
  `BOF3X_ORIGINAL=*`. The first observation was made with `File_Read` ours; the
  mechanism above is entirely in Capcom's code and upstream of any read, but
  the clean A/B has not been run.
- **New behaviour:** our `Save_WriteFile` (`src/game/save_io.cpp`, replacing
  `0x454870`) does exactly what the original does and then calls
  `Save_ListFiles` before returning, so the table the menu reads is current.
  Nothing else changes: same file, same bytes, same return values.
  **Verified 2026-09-19:** owner saved to a new slot (1) in a session launched
  through `bof3x-launcher`, reopened the menu, and the slot was listed at once.
  `bof3x.log` for that run: `inject ON Save_WriteFile original 0x00454870`,
  then `first call Save_WriteFile(BISLPS01.DAT, 4784)`; the file is on disk at
  4,784 bytes.
- **Rationale:** a bug, not a design choice — the player is shown an empty slot
  where they have just saved, and the natural response (save again, or assume
  saving is broken and stop playing) is harmful either way. The repair is the
  smallest available: one call to the game's own lister at the one place the
  directory changes. Costs: each call leaks one search handle, because
  `Save_ListFiles` never calls `_findclose` (original defect, unfixed) — one
  per save, negligible. Deliberately *not* fixed here: `File_OpenWrite`
  `0x5A7420` reporting a failed `fopen` as success (since fixed, DIV-0003), and
  `Save_ListFiles` not bounding its table at 16 — a separate entry when it is
  touched.
- **Also in the PSX version?** Unknown. The listing and the per-slot files are
  porting-house code — the PlayStation reads the memory card's directory —
  so a port bug is the likelier reading, but nobody has checked whether the
  PSX menu refreshes after a write. Sibling-side question.
- **Reversible?** Yes: `BOF3X_ORIGINAL=Save_WriteFile` runs Capcom's function
  instead ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2). No config toggle; there is
  no config system yet.

### Report a failed save-file open instead of crashing

- **ID:** DIV-0003
- **Date:** 2026-09-19
- **Subsystem:** platform
- **Original behaviour:** `File_OpenWrite` `0x5A7420` stores the `fopen(path,
  "wb")` result into `File_Slots[slot]` and returns the slot index **without
  testing the result** (disasm 2026-09-19: `call 0x5B9B6D` at `0x5A7457`, then
  `mov [esi*4 + 0x7DE3E4], eax` / `mov eax, esi` / `ret`, no `test`). It
  returns -1 only when all sixteen slots are taken. So when the save file
  cannot be created — a read-only game directory, e.g. an install under
  `Program Files` — its sole caller `Save_WriteFile` `0x454870` sees success
  and calls `File_Write`, which passes the null stream to `Crt_fwrite`
  `0x5B9E65`. That begins with the stream lock `0x5BCBD3`, which for any
  pointer outside the CRT's static stream table calls `EnterCriticalSection`
  (import slot `0x5C40FC`) on `stream + 0x20` — address `0x20` for a null
  stream. *Established by reading, not by reproduction:* nobody has yet run the
  original against a read-only directory and watched it fault.
- **New behaviour:** our `File_OpenWrite` (`src/game/file_io.cpp`) returns -1
  when the open fails and claims no slot, which is what `File_Open` `0x5A7380`
  already does on the read side. `Save_WriteFile` already returns -1 for a -1
  handle, and both of its call sites (`0x580074`, `0x5809FF`) already compare
  the result with -1 and skip their slot-summary update — at `0x580074` the
  branch target `0x5800C0` is a bare `ret`. A successful open is unchanged:
  same slot scan, same mode, same return value. *Not yet verified in game:*
  the failing case needs a save attempted with the game directory read-only;
  what the menu then shows the player has not been observed.
- **Rationale:** a bug, not a design choice — the function's read-side twin
  checks, its caller checks for the only failure it can currently report, and
  the callers' callers check too; the chain of -1 handling exists and this one
  link drops it. The repair adds no new behaviour of our own: it routes the
  failure into the path Capcom's code already has. Deliberately *not* done
  here: telling the player the save failed. If the existing -1 path is silent,
  that is its own entry.
- **Also in the PSX version?** No counterpart. The file layer is porting-house
  code ([`asset-loading-path.md`](asset-loading-path.md) §1); the PlayStation
  saves to a memory card through the BIOS.
- **Reversible?** Yes: `BOF3X_ORIGINAL=File_OpenWrite`
  ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2). No config toggle.

### Drain queued image uploads on frames that are not rendered

- **ID:** DIV-0004
- **Date:** 2026-09-19
- **Subsystem:** platform
- **Original behaviour:** game logic queues image uploads (sprite animation
  frames, through `0x5894D0`: `queue[count] = ...; count++`, no bound), and the
  queue is drained by `Gfx_FlushUploadQueue` `0x461F00` **only in the branch of
  WinMain's loop that renders**, taken when `GetTickCount` is still below the
  frame deadline ([`call-trace.md`](call-trace.md) §6). After any stretch of
  logic frames without rendering the queue holds them all. The arrays have 20
  slots; the flush unpacks every entry into a bump-allocated scratch buffer
  with about 127 KB below the next globals. Past either limit it overwrites
  the draw structures, its own loop bound among them, and the draw that
  follows faults. **Seen three times 2026-09-19**, the last with
  `BOF3X_ORIGINAL=*` (crash at `0x59F24F` reading `0x00080000`, caller
  `0x4FCE74`, identical to the first): leave the game unfocused for about two
  and a half minutes at a spot with idle animations and click back — the game
  freezes while unfocused and replays the missed frames unrendered
  ([`windowed-mode.md`](windowed-mode.md)). Holding the window's title bar does
  the same with the game focused (queue 6 after 57 s; not taken to the crash).
  Full record: [`known-defects.md`](known-defects.md) D4.
- **New behaviour:** our `Gfx_BeginFrame` (`src/game/gfx_frame.cpp`, original
  `0x4FD230`, WinMain's once-per-logic-frame set-up, otherwise reimplemented
  faithfully) first calls `Gfx_FlushUploadQueue` if the queue is not empty. On
  a rendered pass the queue is already empty there and nothing changes. On an
  unrendered pass the uploads are applied then instead of accumulating: the
  same uploads, in the same order, by Capcom's own flush, and — as in the
  original — before the next frame's logic. What can differ is *when* an
  upload reaches the VRAM shadow relative to wall-clock time, which was
  already not a function of the frame count.
- **Rationale:** a bug, not a design choice: nothing in the design wants
  uploads to wait for a rendered frame, it is simply where the call was put,
  on a platform where every frame was expected to render. Draining was chosen
  over bounding the queue (owner, 2026-09-19): a bound has to drop or refuse
  uploads, draining loses nothing.
- **Not fixed by this:** the replay of missed time itself (the fast-forward on
  refocus), the pause on focus loss, the stalled audio while a title bar is
  held, and the float deadline ([`known-defects.md`](known-defects.md) D5).
  Each is its own entry if it is changed.
- **Verification:** attract oracle identical to the all-original reference
  (7,478 frames); and the reproduction survives — 665 s unfocused, then
  refocus, queue never above 1, `DIV-0004` logged, no crash (owner,
  2026-09-19). Runs are listed in [`known-defects.md`](known-defects.md) D4.
- **Also in the PSX version?** Not applicable as such — the PlayStation cannot
  lose focus and its `LoadImage` is queued to the GPU. Whether the PSX code
  has the same unbounded queue is a sibling-side question.
- **Reversible?** Yes: `BOF3X_ORIGINAL=Gfx_BeginFrame`
  ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2). No config toggle.

### Walk a language overlay after each `DAT` file

- **ID:** DIV-0005
- **Date:** 2026-09-20
- **Subsystem:** assets / localisation
- **Original behaviour:** `LoadDatFile` `0x454590` reads `DAT\<name>` and walks
  its chunks; that is all. There is one language, compiled in with the data
  ([`dialogue-localisation.md`](dialogue-localisation.md) §2).
- **New behaviour:** with the environment variable `BOF3X_LANG=xx` set, our
  `LoadDatFile` (`src/game/dat_load.cpp`) walks `DAT\xx.<name>` after
  `DAT\<name>` when that file exists, with the same chunk walker. An overlay
  holds only the chunks that differ, and they land on top: a kind-0 chunk over
  the same arena bytes, a kind-3 chunk through `Font_SetGlyphData`, which frees
  the shipped table - the branch no shipped data had ever run. Without the
  variable, or without the file, nothing changes (one `GetFileAttributesA` per
  load when the variable is set). The overlays are built on the player's
  machine from the player's disc by `tools/loc_build.py`; none is shipped or
  committed (CLAUDE.md rule 1).
- **Rationale:** stage 2 of the owner's order of work - playable text for
  the owner, and the groundwork for selectable languages
  ([`STATUS.md`](STATUS.md)). A design choice, not a bug fix.
- **Known and accepted:** anything that stored glyph codes under one language
  and draws them under another is scrambled - character names in a save, for
  one. Sixteen areas' English text does not fit below the system pool and gets
  no overlay yet ([`dialogue-localisation.md`](dialogue-localisation.md) §6).
- **Verification:** attract run with `BOF3X_LANG=en`, 2026-09-20: the log
  shows `DAT\en.FIRST.DAT` and `DAT\en.AREA004.DAT` opened after their
  originals, and the area caption and dialogue draw in English in the donor's
  glyphs (screenshots, `analysis/font/shots/`). *Not run:* the oracle with the
  variable unset after this change - the path is unchanged by reading, but
  that is reading.
- **Also in the PSX version?** No. Each PlayStation language is its own build.
- **Reversible?** Yes: unset `BOF3X_LANG`, or `BOF3X_ORIGINAL=LoadDatFile`.

### Advance the dialogue pen by the glyph's width

- **ID:** DIV-0006
- **Date:** 2026-09-20
- **Subsystem:** text
- **Original behaviour:** `MsgBox_Step` `0x497840` draws one character through
  `Text_DrawAt` `0x516B30` (call at `0x497A22`) and then adds a flat 12 to its
  pen, `MsgBox_PenX` `0x7DEE5C` (`add bp, 0xC` at `0x497A44`) - the PSX JP
  engine's advance, right for 12 px cells. The Western PlayStation builds use
  a font of 8 x 12 cells and their stepper adds 8 (`SLUS_004.22`,
  `addiu v0, v0, 8` at `0x80150770`, read 2026-09-20): monospaced, not
  proportional.
- **New behaviour:** a `DAT` chunk of **kind 4**, which is ours - the original
  walker skips any kind above 3, and no shipped file has one (census of 742) -
  carries one byte a glyph, the advance in PSX pixels; its tag is the advance
  of the space `0x20`. `src/game/text_advance.cpp` re-aims the one call at
  `0x497A22` (`bof3::RetargetCall`; `Text_DrawAt` and its 340 other callers
  are untouched), makes the same call, then moves `MsgBox_PenX` by
  `advance - 12`, so the stepper's own `+ 12` lands the pen by the glyph's
  advance. With no kind-4 chunk loaded the adjustment is zero. The table is
  data, so a proportional font needs no further engine change.
- **Extended the same day to the string draw itself.** `Text_DrawString`
  `0x516B70` is ours (`src/game/text_draw.cpp`), faithful in everything but
  its two `+ 12`s - after a glyph and after a `0x20` - which take the same
  table. That is the pen of every caller of `Text_DrawAt` that is not the
  dialogue box: the narration, the boxes that read the script block themselves
  ([`dialogue-localisation.md`](dialogue-localisation.md) §6), the menus. With
  no table loaded both are 12. Callers that centre or right-align text by
  counting characters at 12 px are NOT adjusted; none has been seen wrong yet.
- **And to the window text draw.** `Text_DrawImmediate` `0x5961C0` is ours
  (`src/game/text_immediate.cpp`): the choice lists - yes / no, the camp
  menu - which the owner saw still spaced at 12 px on 2026-09-20. It keeps
  its pen in a register and adds 12 itself, so nothing short of the function
  reaches it. Faithful but for that `+ 12`, including a quirk the fuzz found
  and the read had missed: control `0x08` starts the inserted message at its
  second byte.
- **The hang.** `MsgBox_Step` pulls a first byte `0x2A` or `0x3C` 12 px into
  the margin at the start of a line; the US stepper does it for its double
  quote, by 8. `loc_build.py` gives the quote byte `0x2A`, and
  `MsgBox_DrawChar` draws a hung character at line start minus *its advance*
  and leaves the pen for the stepper to bring back to the line start.
- **The data is not the US release's in one respect:** the apostrophe and the
  comma advance 5, not 8. That is its own entry, DIV-0009.
- **Not covered:** the stepper's other draw, `0x4987E0` (flag 8 of
  `0x7DEE44`; unread).
- **Rationale:** English at a 12 px advance overflows the box on the first
  line (seen 2026-09-20); the donor script's line breaks are authored for 8.
- **Verification:** attract run, 2026-09-20: `retarget ON MsgBox_DrawChar` and
  `DIV-0006: advance table, 2551 glyphs, space 8 px` in the log; dialogue
  fits its box (`analysis/font/shots/_boxes2.png`). `Text_DrawString`:
  start-up fuzz against a clone with its jump table relocated
  (`BOF3X_SHADOW=text_draw`), 6,000 strings, 78,029 glyphs through a
  recording stand-in for `Text_EmitGlyph`, 0 mismatches; builds with the
  remembered colour, the count-out return and the once-read clip pair each
  changed are refused (2,119 / 2,437 / 2,949 rounds), and ones with the glyph
  bias or the limit changed hang in the trap instruction. Attract oracle with
  `BOF3X_LANG` unset and all 111 ours: identical to the all-original reference
  at every one of 1,734 compared frames. In English the narration draws at
  8 px (`analysis/font/shots/_narr.png`). `Text_DrawImmediate`: the same kind
  of fuzz (`BOF3X_SHADOW=text_immediate`), 6,000 strings, 279,360 characters,
  0 mismatches - after 3,896 before the `0x08` quirk was matched; builds with
  the line height, the count-out resume, the `0x08` start and the name count
  changed are all refused. Oracle with 112 ours and no language: identical,
  1,734 frames. Hang and tight punctuation seen in the attract dialogue
  (`analysis/font/shots/_boxes5.png`). Owner, in game: the dialogue, the
  narration and the menus seen and "looks great" (2026-09-20); the choice
  lists at 8 px are owed a second look.
- **Also in the PSX version?** The Western builds, yes, as a constant.
- **Reversible?** Yes: `BOF3X_ORIGINAL=MsgBox_DrawChar`, or load no kind-4 chunk.

### Move the system message pool out of the area script's way

- **ID:** DIV-0007
- **Date:** 2026-09-20
- **Subsystem:** text / assets
- **Original behaviour:** the area script loads at arena offset 0 and the
  system pool at `0x4000` (PSX `0x80010000` / `0x80014000`, the Japanese
  layout), so an area's text has 16 KiB. The largest shipped block is `0x39CB`.
  `Msg_SystemPtr` `0x497740` reads the pool at the constant `0x807580`.
- **What Capcom did about it:** the US release moved the pool, not the script -
  44 sections go from `0x80014000` to `0x8001A000`, and `0x80010000` "does not
  move in any of the five releases" (sibling `regional-builds.md`). The
  Chinese port was made from the Japanese layout and never needed to.
- **New behaviour:** with `BOF3X_LANG` set, `MsgPool_Relocate`
  (`src/game/msg_pool.cpp`) gives the pool a 16 KiB buffer of its own; our
  `LoadDatFile` copies a kind-0 chunk of tag `0x4000` there instead of into the
  arena; and our `Msg_SystemPtr` reads a variable base. The area script may
  then run to `0x8000`, where the CLUT strip begins; the largest English block
  is `0x5559`, and all 200 areas now get an overlay. Without the variable the
  base is the original's constant and nothing differs.
- **Why this side:** measured 2026-09-20 over every operand of every function
  in `analysis/pc_funcs.json` - the script window has 177 references from 41
  functions; the pool window has **two, both in `Msg_SystemPtr`**. And over
  every shipped `DAT`: exactly 44 chunks land in `0x4000`..`0x8000`, all at tag
  `0x4000`, all pools (first dword 8), none straddling either edge.
- **Not ruled out:** a reader that reaches the pool through a computed
  address. None is known; one would read English text bytes instead of pool
  bytes, in the sixteen long areas only.
- **Verification:** start-up fuzz against a clone of the original
  (`BOF3X_SHADOW=msg_pool`): 20,000 ids, in place and relocated, 0 mismatches;
  a build with the index mask narrowed to `0x1FFF` is refused - after the
  first version of the fuzz, blind to it, was not. Attract run with
  `BOF3X_LANG=en`: `Rand` count identical to the all-original reference at
  every compared frame; 5 of 1,715 sampled frames differ in *message index*
  only, each at a message change - English messages are longer and the box
  moves on when it has finished printing. That difference is the language's,
  not this entry's. Owner, in game, in a long area and in a menu: owed.
- **Also in the PSX version?** The Western builds, as a different constant.
- **Reversible?** Yes: unset `BOF3X_LANG`; `BOF3X_ORIGINAL=Msg_SystemPtr`
  alone is NOT safe with it set (the original would read an arena the pool
  was never copied to).

### Item and ability names, and the system pool, from a language overlay

- **ID:** DIV-0008
- **Date:** 2026-09-20
- **Subsystem:** text / assets
- **Original behaviour:** the names the menus draw are compiled into
  `BOF3.exe`: six fixed-stride record tables in `.data` (`symbols.toml`
  `NameTable_*`), the PSX `GAME.EMI` tables with the name field widened.
  Measured 2026-09-20 against the sibling's `names/*.toml`: **name[16] in all
  six**, where the JP disc has name[8] and the US disc name[12] (strides
  22 / 20 / 28 / 26 / 24 / 24 against JP 14 / 12 / 20 / 18 / 16 / 16), and the
  numeric fields equal to the JP disc's in 534 of 534 records. Descriptions
  and menu strings are in the system pool, `FIRST.DAT` and 43 other files at
  arena tag `0x4000`: two blocks of the area script's shape, 309 and 455
  slots - the same two counts as the US disc's pool at `0x8001A000`.
- **New behaviour:** a `DAT` chunk of **kind 5**, ours, carries one table's
  names, 16 bytes each; its tag is the address of record 0's name field.
  `NameTables_Apply` (`src/game/name_tables.cpp`) accepts the six known
  addresses with exactly `count * 16` bytes and aborts on anything else, and
  writes the name fields only. The pool needs no new mechanism: a kind-0
  chunk at tag `0x4000` in an overlay goes where the shipped one goes
  (DIV-0005, DIV-0007). The relocated pool's buffer is `0x8000` bytes: the US
  `FIRST` pool is `0x41E8`, which is the reason Capcom moved it.
- **The data:** `tools/loc_build.py all` finds each donor table in `GAME.EMI`
  section 0 *by the PC table's numeric bytes* at the donor's stride - no donor
  address is assumed, and a disc whose numbers differ is refused - and
  re-encodes the names as it does dialogue: 538 of 538 names, 44 of 44 pools,
  no slot kept as shipped. Capcom's US strings, verbatim: `BallockKnife`,
  `Lgt.Clothing`. The 16-byte field would hold longer spellings; using it is
  a separate decision, and on screen the budget is unmeasured (eight Chinese
  glyphs are 96 px, twelve letters at 8 px).
- **Not covered:** enemy names (12-byte fields in battle data,
  [`DAT_CONTAINER.md`](DAT_CONTAINER.md)), character names, place names,
  anything drawn as artwork, and whatever strings are in `.text`/`.data`
  outside these tables.
- **Verification:** attract run with `BOF3X_LANG=en`, 2026-09-20: six
  `DIV-0008` lines, no crash, oracle identical to the English run before it.
  **Nothing here is visible in the attract sequence.** Owner, in a menu: owed.
  Unread: who reads the tables, and whether a name is drawn to a NUL or to a
  count - the tool keeps every name under 16 bytes so that either works.
- **Also in the PSX version?** Each language is its own build with its own
  tables.
- **Reversible?** Yes: unset `BOF3X_LANG`. The write is to process memory.

### Tighten the apostrophe and the comma in the English font

- **ID:** DIV-0009
- **Date:** 2026-09-20
- **Subsystem:** text / localisation data
- **What this diverges from:** not the Chinese PC port, which has no English,
  but **the US PlayStation release**, which is where the English comes from
  and what "as it was" means for it (CLAUDE.md rule 6).
- **Original behaviour (US release):** the dialogue font is monospaced. Every
  cell is 8 x 12 and the stepper adds 8 after every character, with no
  exception for any glyph (`SLUS_004.22`, `addiu v0, v0, 8` at `0x80150770`;
  its one special case is the hung double quote, `0x80150680`). The
  apostrophe's and the comma's ink is columns 1..3 of their cells, so each is
  followed by four empty pixels: `you' ll`, and a comma and a space together
  make a gap of twelve. **Confirmed on the original by the owner, 2026-09-20,
  on an emulated PSX build**: "you'll be needing some supplies, eh?" shows
  both.
- **New behaviour:** `tools/loc_build.py` writes the advance table
  (DIV-0006) with 5 for those two glyphs and 8 for the rest. The rule, not a
  list: a glyph whose ink ends by column 3 advances to two past it. Measured
  over the donor's 100 cells it takes exactly the apostrophe and the comma;
  the stop, the colon and the exclamation mark are centred in their cells and
  keep 8. No engine code is involved - the engine advances by whatever the
  table says.
- **Rationale:** the owner's, having seen both: it reads better. A line can
  only get shorter, so text authored for 8 px still fits its box.
- **What it costs:** text the US script centres or aligns with spaces, on the
  assumption that every character is 8 px wide, shifts left by 3 px for each
  apostrophe or comma before the point in question. None seen wrong.
- **Reversible?** Yes: build the overlays with `--mono`.

### Give a sprite's last texel row and column their share of the screen

- **ID:** DIV-0010
- **Date:** 2026-09-20
- **Subsystem:** render
- **Original behaviour:** the three Direct3D sprite handlers (`D3d_DrawSprt`
  `0x5A2300`, `D3d_DrawSprt8` `0x5A2520`, `D3d_DrawSprt16` `0x5A2710`) take the
  quad's texture edges from the float table `0x7CA9E0`, read live as
  `(i + 0.512) / 256`: near edge `tc[u]`, far edge `tc[u + w - 1]`, with the
  quad the full `w` pixels. The values are right for bilinear filtering of a
  cell in a shared page - the centres of the first and last texel - but a
  vertex value is reached one pixel PAST the last pixel drawn, so at 2x the
  last pixel of an 8 pixel sprite samples `u + 7.07`, not `u + 7.5`: the last
  texel row gets about 0.7 of a screen row where the first gets 1.6. Seen by
  the owner as menu numerals cut off at the bottom
  ([`known-defects.md`](known-defects.md) D1); the 8 px digits' bottom stroke
  is the last row of their cell (VRAM shadow read live).
- **New behaviour:** the far vertex value is `u + w - 1/30 + 0.012` texels, so
  that the last PIXEL samples the centre of the last texel (exact for w = 8 at
  scale 2, within 0.03 of a texel otherwise, and never past the centre for
  w >= 8). The near edge and the table `0x7CA9E0` are untouched. Implemented
  as the original handlers' own bytes, copied, with the two far-edge operands
  re-aimed at our table (`src/game/gfx_sprite_uv.cpp`) - not a
  reimplementation; the handlers end in COM calls and a drawn surface cannot
  be checked yet.
- **Rationale:** a bug, not a choice: the first and last texel are treated
  differently for no reason a design would have, and it cuts the base off every
  `2`. The obvious fix (far edge `u + w`) was built first and is wrong - it
  blends in the neighbouring cell and drew seams through the title logo
  (`analysis/d1/fix1`, 2026-09-20).
- **Also in the PSX version?** no - the PlayStation GPU does not filter and
  copies a sprite texel for texel.
- **Reversible?** `BOF3X_ORIGINAL=D3d_DrawSprt,D3d_DrawSprt8,D3d_DrawSprt16`.
- **Checked:** attract run, no crash, oracle unaffected (render only);
  screenshots original against ours in `analysis/d1/`: no seams, sprites
  drawn at a true 2x where the original stretched w - 1 texels over w pixels
  (the title's (R) mark is a pixel shorter). **The menu numerals, A/B, by
  input recipe 2026-09-21** ([`input-script.md`](input-script.md) §5): with
  Capcom's handlers the bottom row of every HP / AP numeral is cut off and
  the portrait frame has a seam under it; with ours both are whole. **The
  owner judged the capture: "numerals look good".**

### Draw the Config panel's frame, which the PC build compiled to nothing

- **ID:** DIV-0011
- **Date:** 2026-09-20
- **Subsystem:** menu
- **Original behaviour:** the Config screen's panel draw `0x461710` and its
  controller sub-panel `0x461A50` each begin with a call `(x, y, w, h)` -
  `(.., 0x21, 0x0D)` and `(.., 0x0C, 0x0F)` - to `0x4DF820`, a bare `ret` with
  25 call sites of 0, 1 and 4 arguments (several empty functions folded into
  one). The rows are drawn with no panel behind them; seen by the owner
  against the PlayStation game, 2026-09-20.
  **Extended the same day to the reserve list of "change party members"**,
  which the owner showed unframed: the PlayStation's `0x801EA99C` frames it
  `0x12` by `0x15` cells with the same function, and the PC has that call
  twice, `0x581313` (in `0x581300(x, y)`) and `0x59AA98` (in `0x59AA80(obj)`),
  both to the empty function. Config's frame was confirmed right by the owner
  in game.
- **New behaviour:** those call sites - four now (`bof3::RetargetCall`; the
  other 21 are untouched) reach `Menu_DrawFrame` in `src/game/menu_frame.cpp`, which
  draws what the PlayStation's function draws, piece for piece and in its
  order: four edge strips and a fill of 8 x 8 tiles, then four 16 x 16
  corners, all through the PC's own `Menu_DrawPiece` `0x57D860`.
- **Rationale:** a port defect. The PlayStation function is `0x801DF56C` in
  `STATUS.EMI`, called from the same function with the same arguments (read
  from the owner's Japanese disc, 2026-09-20; the decode is the comment on
  ours). It repeats each tile across one sprite with the GPU's texture window,
  which the port's renderer does not have - the likely reason the body was
  left empty (not established). The PC executable still carries all nine
  pieces in `Menu_DrawPiece`'s rectangle table `0x663C8C`, the tiles at
  exactly the PlayStation's window origins, so nothing is invented: ours
  places one piece a tile where the PlayStation placed one sprite a strip.
- **Also in the PSX version?** no - the PlayStation draws the frame.
- **Reversible?** `BOF3X_ORIGINAL=Menu_DrawFrame`.
- **Checked:** start-up validates both call sites (`retarget ON` lines), and
  an attract run is unaffected. **Not yet seen in game** - the attract
  sequence opens no menu; owner to look at Config, and at the controller
  sub-panel. 421 and 172 sprites a frame: watch for anything else on the
  screen going missing, which is what a full packet pool would look like.

### Offer point sampling as a choice of look

- **ID:** DIV-0012
- **Date:** 2026-09-20
- **Subsystem:** render
- **Original behaviour:** the renderer's set-up `0x5A5160` sets stage 0's
  filters once - `SetTextureStageState(0, D3DTSS_MINFILTER 0x11, 2)` at
  `0x5A5B28` and `(0, D3DTSS_MAGFILTER 0x10, 2)` at `0x5A5B3B`, 2 being LINEAR
  - and alpha testing as GREATER than 8 of 255. Everything is drawn
  bilinearly at 2x, and a glyph's edge, blended towards the transparent
  colour key, is drawn as a grey fringe: part of the "glow" the owner saw
  round PC text (the other part is DIV-0013).
- **New behaviour:** with `BOF3X_FILTER=point` in the environment, the two
  pushed 2s become 1 (POINT) - `bof3::PatchBytes`, new in the hook layer,
  which refuses unless the bytes are the expected ones. Unset, or `linear`,
  nothing is touched: **the default is the original's.**
- **Rationale:** the owner's wish, 2026-09-20: a "clean / sharp" look beside
  the port's soft one, as a toggle. This is the clean half, as a launch-time
  switch; a live toggle waits on the game's input being read
  ([`IDEAS.md`](IDEAS.md) I15). With DIV-0010 a point-sampled sprite is an
  exact 2x.
- **Also in the PSX version?** the PlayStation does not filter: point
  sampling is its look.
- **Reversible?** opt-in; and `BOF3X_ORIGINAL=Gfx_FilterPoint`.
- **Checked:** the owner played a session launched this way, 2026-09-20
  (`analysis/d1/point/`): menu, numerals, portraits and the reserve list's
  frame all crisp, nothing missing.

### White text in the disc's shades when the text is the disc's

- **ID:** DIV-0013
- **Date:** 2026-09-20
- **Subsystem:** text
- **Original behaviour:** `FIRST.DAT`'s CLUT strip (kind 0, tag `0x8000`)
  differs from the PlayStation's (`FIRST.EMI`, section for `0x80033800`; US
  and JP discs alike) in **row 0 alone**, white text: indices 1-7 are 28, 27,
  25, 24, 21, 17, 12 and index 8 (0, 0, 6), where the discs have 25, 23, 20,
  17, 13, 8, 4 and (0, 0, 1). Brightened, presumably for the port's
  anti-aliased Chinese glyphs. Measured 2026-09-20 from the files, and the
  port's row read back from live VRAM at (0, 480).
- **New behaviour:** `tools/loc_build.py all` puts the strip into
  `en.FIRST.DAT` with row 0 taken from the player's disc; the other rows are
  the port's, which already match. `--pc-white` leaves it out. Only under
  `BOF3X_LANG`: with no language set nothing changes.
- **Rationale:** the donor font's cells carry their drop shadow at index 7
  (1,812 of 9,600 pixels over the 100 cells). On the port's row that is
  mid-grey, and the owner saw it: a grey shadow where the PlayStation's is
  black. The cells were drawn for the disc's row, so they get it.
- **Side effect, accepted:** Chinese glyphs still on screen under
  `BOF3X_LANG=en` (names not yet converted) are drawn with the darker ramp.
- **Also in the PSX version?** this restores the PSX values.
- **Reversible?** rebuild with `--pc-white`, or play without `BOF3X_LANG`.
- **Checked:** live VRAM row 0 reads the disc's values in an attract run;
  dialogue before and after in `analysis/d1/cmp_white.png` - dark shadow.


### The title menu in the overlay's language, its third row cut from the disc's letters

- **ID:** DIV-0014
- **Date:** 2026-09-20
- **Subsystem:** menu / text
- **Original behaviour:** the title menu is artwork - image chunk
  `0x1C000200` of `START.DAT`, three rows of 32 px Chinese characters (new
  game, load game, options) - drawn by `0x5888D0` one `SPRT` a row, centred,
  with the row widths 96, 128 and 64 as immediates at `0x5888E4`,
  `0x5888E9`, `0x5888EE`. Measured 2026-09-20: `dat.py compare` against the
  JP `START.EMI`, the de-tiled page, the disassembly
  ([`title-menu.md`](title-menu.md)).
- **New behaviour:** under `BOF3X_LANG`, `en.START.DAT` (built by
  `tools/loc_build.py all` from the player's disc) replaces the page: NEW GAME
  and LOAD GAME exactly as the disc's `START.EMI` has them, and **CONFIG**, a
  word no disc has, assembled from their letters - C from G, F from E and L, I
  from L. A chunk of kind 6, ours, carries the three widths (130, 140, 96) and
  `src/game/title_menu.cpp` writes them into the immediates. The draw itself
  stays the original's. With no language set nothing changes.
- **Rationale:** the owner's request, 2026-09-20. The word for the third row
  was the owner's choice among CONFIG, OPTIONS and OPTION: CONFIG needs no
  letter drawn by us, so everything on the screen derives from the player's
  disc, and it is the US release's own word for that screen in the field menu.
- **Also in the PSX version?** the first two rows restore the PlayStation's
  lettering (US and JP pages are byte-identical). The third row does not exist
  there: the PlayStation title has two.
- **Reversible?** play without `BOF3X_LANG`, or delete `en.START.DAT`;
  `BOF3X_ORIGINAL=TitleMenu_Widths` keeps the original widths (the English
  rows are then cut off - for A/B only).
- **Checked:** an offline composition through CLUT 0 at the draw's positions,
  seen by the owner ("perfect"). **In game, 2026-09-21**: captured by input
  recipe ([`input-script.md`](input-script.md) §5; the cursor starts on LOAD
  GAME when saves exist) and judged by the owner: "the title menu looks
  perfect". The two-row layout and each row's destination are still open
  ([`USER_CHECKS.md`](USER_CHECKS.md) 6).


### The in-game Config screen in the overlay's language

- **ID:** DIV-0015
- **Date:** 2026-09-20
- **Subsystem:** menu / text
- **Original behaviour:** the Config screen's text is not in any `DAT`. It is
  in `BOF3.exe`, in three shapes, read 2026-09-20
  ([`config-screen.md`](config-screen.md)): six row labels as address operands
  in the row draw `0x461800` (`0x669F0C`, `0x669F18`, `0x669F24`, `0x669F2C`,
  `0x669F34`, `0x669F3C`); seventeen option strings in 16-byte records at
  `0x6536F8`, each a count, a signed x and the string, reached through the
  row-to-record and row-to-count tables at `0x653808` / `0x653810`; and six
  controller-panel names at `0x66A338` behind the pointer table `0x66A368`.
  The screen reads 讯息速度 / 视窗颜色 / 背景 / 音效 / 冲刺 / 控制.
- **New behaviour:** under `BOF3X_LANG`, a chunk of kind 7 - ours - in
  `en.FIRST.DAT` carries the donor disc's own strings for all twenty-nine,
  extracted by `tools/loc_build.py` from `BIN/ETC/START.EMI` and re-encoded
  for the PC's glyph table. `src/game/config_text.cpp` writes the option
  records in place, with **the donor's own count and x bytes**, and re-points
  the six label operands and the six entries of the controller pointer table
  `0x66A368` at buffers of its own - a UI string is two bytes a character, so
  "Background" needs 21 where the Chinese string has 4, and "Change" 13 where
  the slot holds 8.

  Every string names the donor's **8 x 8 UI cells** (glyph `0xA00` up,
  DIV-0016) two bytes at a time, even where a single-byte slot for the
  character exists. Two reasons. The 8-unit quad this screen draws with shows
  the *whole* 24 x 24 cell scaled to 16 x 16 on screen - the emitter
  `0x516D50` fixes the texture extent at `0xC` units whatever the quad, which
  is why a full-width Chinese label renders legibly there - so the 8 x 8 cell,
  stored **tripled** to fill its slot, arrives as the PlayStation's doubled
  8 x 8. And because every
  character is two bytes, a string's byte length is exactly twice its
  character count - which is what the screen's own `4 * len` right-alignment
  and `4 * count` centring already assume, so **the width arithmetic is left
  alone**.

  Latin words are longer than the two to four Chinese characters the screen
  was laid out for, and two one-byte operands move it to suit them, found by
  the owner looking at the result: the label column's right edge from 168 to
  205 (`0x3A` -> `0x5F` at `0x46189D` and `0x4618ED`, both branches of the row
  draw). The rows' y is the original's: an earlier build dropped every string
  two pixels, which suited cells that sat in the top of their slot and left
  the tripled 8 x 8 sitting on the row's floor (owner, in game, 2026-09-20),
  so it was removed. With no language set none of this is applied.
- **Rationale:** the owner's request, 2026-09-20, with a screenshot of the
  PlayStation screen. Everything drawn comes from the player's own disc; no
  English string is in this repository.
- **Also in the PSX version?** this *is* the PlayStation version's text, down
  to its quirks: "Off" carries the count 6 it inherited from "Stereo", so it
  hangs where "Stereo" hangs, and the two unused records 15 and 16
  ("Manual" / "Auto") are left as the disc leaves them. Two rows say something
  different from the Chinese port rather than translating it: row 1 and row 2
  are `1 2 3 4` where the port names the colours and the patterns, and row 4
  is **Autorun Off / On** where the port has 冲刺 (dash) 手动 / 自动
  (manual / auto). Both are the US release's own wording, kept deliberately;
  the port's naming is arguably the better of the two and is a candidate for a
  later, separate divergence.
- **Reversible?** play without `BOF3X_LANG`, or delete `en.FIRST.DAT`;
  `BOF3X_ORIGINAL=ConfigText` leaves all thirteen operands alone - the six
  label pointers and the seven layout numbers - so the labels stay Chinese and
  the layout is the original's, while the options and controller names, which
  are data writes rather than patches, are English. For A/B only.
- **Checked:** the extractor reproduces the owner's screenshot of the
  PlayStation screen string for string (2026-09-20), and a read-only
  `ReadProcessMemory` sample of the running game decodes all twenty-nine
  strings back to that text.
  **The layout is confirmed in game by the owner, 2026-09-20**: the label
  column's right edge, over three rounds. **The tripled 8 x 8 lettering was
  seen by the owner the same day** - "much closer" - with the text two pixels
  low, which was the earlier drop and is removed; seen again without it and
  confirmed, 2026-09-21.
  Two cell choices were seen and rejected by the
  owner: a doubled 8 x 8 (two thirds of the disc's size) and a doubled 8 x 12
  (the right height but two thirds of the width - thin letters, wide gaps).
  The PlayStation screenshot, measured with the panel as the ruler, has label
  ink 8 rows tall on an 8 advance under a 12-row banner: the 8 x 8 set, as the
  owner held throughout. 
- **An earlier build of this entry got it wrong**, and it is worth recording
  why. It encoded the strings as ordinary single-byte codes - which reach the
  12 px cells - and then patched three half-width computations to compensate.
  The owner's screenshot of the Chinese screen settled it: the rows are
  16 pixels a character against the banner's 24, and the labels are
  right-aligned on a common edge, both of which the original arithmetic
  already produces. Wrong glyphs, then code changed to hide it. The three
  patches are gone.


### The donor's 8 x 8 UI font, and a glyph guard that follows the table

- **ID:** DIV-0016
- **Date:** 2026-09-20
- **Subsystem:** text / font
- **Original behaviour:** the string draw `Text_DrawString` `0x516B70`
  compares a glyph index against a flat `0xA00` (`cmp cx, 0xA00 / jbe` at
  `0x516C94`) and runs `mov dx, 0x1000 / in al, dx` above it - a privileged
  instruction, so a debug trap. The shipped table holds `0x993` glyphs, so the
  bound was already 109 past the end of the data it guards.
- **New behaviour:** two things, both only reachable with a language overlay.
  (1) `tools/loc_build.py` now imports the donor's **second** Latin set - the
  same 100 characters at 8 x 8, atlas rows 120..151, same 31 to a row and the
  same code order (found by the owner, 2026-09-20) - **tripled to 24 x 24**
  and appended at glyph `0xA00`, beside the 8 x 12 dialogue set at `0x993`.
  This is the set the 8 px UI draw `0x516E70` wants. Tripled, not doubled:
  that draw's quad is 8 units but it samples the whole 24 x 24 glyph into it
  (`0x516D50` writes u = 0..`0xC` regardless of the quad's size), so a cell
  that fills the glyph lands at 16 x 16 - the PlayStation's doubled 8 x 8.
  (2) the guard is no longer a constant. `Font_SetGlyphData` passes the
  table's glyph count - a size the original takes and never reads - and the
  bound becomes `max(0xA00, glyphs - 1)`. With every shipped file that is
  `0xA00`, the original's own number.
- **Rationale:** the owner asked for the 8 x 8 set in the English font
  (2026-09-20) after spotting it in the atlas. It cannot go anywhere below
  `0xA00` - the 8 x 12 set holds `0x993`..`0x9F6` and only ten slots remain
  under the old bound - and the bound is in a function that is already ours,
  so moving it is a change to our own code rather than a patch.
- **Also in the PSX version?** the 8 x 8 cells are the PlayStation's own, at
  the size it draws them. The guard has no PSX counterpart in evidence.
- **Reversible?** play without `BOF3X_LANG`: the table is the shipped one, the
  count is `0x993` and the bound is the original's `0xA00`.
- **Checked:** the table builds at 2,661 glyphs (`0xA65`, the last a blank
  for the space), the advance chunk follows it, and the game loaded the
  doubled build of both with no trap and no crash
  (2026-09-20, `bof3x.log`). The imported block was rendered and read back:
  digits, punctuation, `A`-`Z`, `a`-`z` and the symbol tail, in the same code
  order as the dialogue set. **The Config screen draws with it** (DIV-0015).
  It was first stored doubled, which the 8-unit quad shrank to two thirds of
  the disc's size; the screen was then moved to the 8 x 12 cells, which came
  out the right height and two thirds of the width. Both were the same
  mistake about scale, not about which font - corrected 2026-09-20 by
  tripling. The tripled table is rendered and read back, and **confirmed in
  game by the owner, 2026-09-21.** Anything else that draws at 8 units goes through the same
  two-thirds scaling and wants these cells as they now are.

### The Config screen's selected row, in the dialogue font at its own advance

- **ID:** DIV-0017
- **Date:** 2026-09-21
- **Subsystem:** menu / Config screen (only with a language overlay)
- **Original behaviour:** the row under the cursor is drawn large. `0x461800`
  (label) and `0x461970` (options) each branch on the row being selected and
  draw through `Text_DrawAt` `0x516B30` - the 12-unit quad - instead of the
  8-unit draw `0x516E70`, reckoning the string's width at 12 units a
  character (`len * 6` at `0x461894`, `count * 12` at `0x4619E1`) where the
  small branch reckons 8. Same string, same glyphs: in Chinese the large form
  is the same character at full size.
- **New behaviour:** at those two call sites only, `ConfigText_DrawSelected`
  stands in for `Text_DrawAt` and redraws the string with every 8 x 8 UI glyph
  (`0xA00`..`0xA63`) swapped for the 8 x 12 dialogue glyph of the same
  character (`0x993`..`0x9F6`; the two sets share a code order, so it is a
  constant offset). And the two width computations become `len * 4` and
  `count * 8` - the numbers the small branches already use - because the
  dialogue glyphs advance 8 (DIV-0006), not 12.
- **Rationale:** on the PlayStation the large form is a different *font*, not
  a bigger copy: the 8 x 12 dialogue cells on the same 8 advance, so a row does
  not move or widen when the cursor lands on it (owner's screenshots of both
  releases, 2026-09-21). With DIV-0015's strings the original code drew a
  tripled 8 x 8 at full size on an 8 advance - crowded - and placed it as if it
  were 12 a character, four units a character too far left: "Background"
  began off the panel's edge, under the cursor.
- **Also in the PSX version?** Yes - this is the PlayStation's behaviour.
- **Reversible?** play without `BOF3X_LANG`, or `BOF3X_ORIGINAL=ConfigText`.
- **Checked:** builds and links; the two call sites and both byte patches are
  validated against the original bytes at start-up. **Seen in game by the
  owner, 2026-09-21: "looks perfect"** - the lowercase `g` is what shows the
  large form is a different font and not a magnified one.

### The menu's short verbs - the buttons above a panel - in the overlay's language

- **ID:** DIV-0018
- **Date:** 2026-09-21
- **Subsystem:** menu (only with a language overlay)
- **Original behaviour:** the buttons above a menu panel - Config's two, and
  the rows of Items, Ability, Equipment and Tactics - are drawn by `0x574890`
  from a set number: 5-byte records at `0x66383C` (a count and up to four
  verb indices) select from 22 NUL-padded 8-byte strings at `0x66A228`,
  through the pointer table `0x6637E4`. Each label goes through `Text_DrawAt`
  at `0x57499B`, centred in its button as `x0 + 0x16 + 48 * i - 6 * n`, `n`
  being its character count from `0x57D800` - half of 12 units a character.
  The strings are Chinese (Config's are `终了` / `预设值`) and live in the
  exe, not in any `DAT`. Read 2026-09-21 ([`config-screen.md`](config-screen.md) §8).
- **New behaviour:** a kind-8 chunk in the English `FIRST.DAT` carries the US
  disc's verbs - `Use`, `Sort`, `Drop`, `Eqip`, `Opti`, `Abil`, `Buy`,
  `Sell`, `Chng`, `Read`, `Vtal`, `Quit`, `Form`, `Bttn`, `Init`, `Note`,
  `Fast`, `Pool`, `Ally`, `Gene`, `Knd`, `Look` - read from the player's
  `START.EMI` and written into the 22 slots in place, one byte a character in
  the dialogue font's single-byte slots. And the one label draw at
  `0x57499B` is re-aimed at `MenuVerbs_DrawLabel`, which moves the label by
  `6 * n - width / 2`, the width being what the pen will really cover
  (DIV-0006's advances) - zero for 12-unit glyphs, so Chinese text is placed
  exactly as before.
- **Rationale:** the stage-2 text swap (DIV-0005). The US disc has the same
  table, the same 5-byte set records byte for byte for sets 0 to 7, and a
  23rd verb, `End`, that its last set uses where the PC's uses `Quit` again;
  so the verbs pair by index and the PC's own sets are the anchor
  `loc_build.py` finds the donor's by. The re-centring is DIV-0017's problem
  again: English advances 8 where the draw reckons 12, which would put every
  label two units a character left of centre.
- **Also in the PSX version?** Yes - these are the PlayStation's strings; the
  US verbs are abbreviated to fit its buttons.
- **Reversible?** play without `BOF3X_LANG`; `BOF3X_ORIGINAL=MenuVerbs` keeps
  the original centring (for A/B only - the English labels then sit left).
- **Checked:** the chunk's 22 slots are validated against the pointer table
  before any write; captured by input recipe 2026-09-21 on the Config screen
  (`Quit`, `Init`) and on Items, Ability, Equipment and Tactics
  (`analysis/shots/menu_screens/`), each label centred in its button - sets
  6, 0, 2, 1 and 7. Not seen: sets 3 (`Buy` / `Sell`), 4 (`Look` / `Chng`),
  5 (`Read` / `Sort` / `Drop`) and 8 (`Knd` / `Quit`), whose screens are
  unidentified, and anything in battle.

### The battle's command labels in the overlay's language

- **ID:** DIV-0019
- **Date:** 2026-09-21
- **Subsystem:** battle (only with a language overlay)
- **Original behaviour:** holding a direction or shoulder button on the
  battle's command cross shows the command's name in a box beside it:
  seven 8-byte slots at `0x669D28` behind the pointer table `0x669D60` -
  攻击, 特能, 道具, 观看, 防御, 突击, 逃走 (Attack, Skill, Item, Watch,
  Defend, Charge, Escape; glyphs rendered from the port's own font) - drawn
  by `0x4439A0` as `Text_DrawAt(box_x + 8, y, 0, 8, label)` (`0x443AF5`),
  left-aligned in a frame whose right edge is `box_x + 0x25`, box positions
  per command at `0x64E2C8`. Found 2026-09-21 by `BOF3X_TEXTLOG` during the
  new game's scripted battle.
- **New behaviour:** a kind-9 chunk in the English `FIRST.DAT` carries the US
  disc's labels, `Atk` `Abl` `Use` `Exa` `Def` `Chg` `Esc`, written into the
  seven slots after each is checked against the pointer table. Nothing about
  the layout changes: three 8-unit characters are exactly as wide as two
  12-unit ones.
- **Rationale:** stage 2 (DIV-0005). The owner's screenshots of the US
  PlayStation show `Atk` and `Esc` in these boxes; the US `BATTLE.EMI` has
  the seven slots immediately before a box table byte-identical to the PC's
  `0x64E2C8`, which is how `loc_build.py` finds them. The owner chose the
  disc's wording over authored English.
- **Also in the PSX version?** Yes - the PlayStation's strings.
- **Reversible?** play without `BOF3X_LANG`.
- **Checked:** captured by `tools/recipes/battle_commands.txt`, 2026-09-21:
  all seven, each in its box, Chg on L1 and Esc on R1. Still Chinese in the
  same fight: the target-select banner (`攻 击`, seen when L2 confirmed
  Attack), the combatants' names, the skill list's header `龙技`
  (`0x66A220`).

### The characters' default names in the overlay's language

- **ID:** DIV-0020
- **Date:** 2026-09-21
- **Subsystem:** text / New Game (only with a language overlay)
- **Original behaviour:** the port has no name entry. New Game (`0x437820`)
  copies seven 0xA4-byte default character records from `0x64B390` into the
  live table `0x903A70` and the whelp's, the eighth, from `0x64B80C` into
  slot 7 (`0x669736`); each begins with a 9-byte name - 龙 妮娜 加兰多 带波
  雷伊 小桃 培克洛 巴比 (glyphs rendered from the port's font). One more
  copy of the whelp's name, the 8-byte slot `0x669CE0`, is copied five bytes
  into character 7's name at `0x42E09D`, a reset at some event.
- **New behaviour:** a kind-10 chunk in the English `FIRST.DAT` carries the US
  disc's eight default names - Ryu, Nina, Garr, Teepo, Rei, Momo, Peco,
  Whelp - read from `START.EMI`'s own default records, and the DLL writes
  them into the eight name fields and the whelp's name into `0x669CE0`, after
  checking the three instructions that read those addresses. New Game hands
  them on as it always did.
- **Rationale:** stage 2 (DIV-0005); the owner asked for the names New Game
  gives. The US records equal the PC's in every byte past the name, four
  places earlier (155 of 155 in all eight), which is how `loc_build.py`
  finds and checks them.
- **Also in the PSX version?** Yes - the PlayStation's default names; the
  PlayStation also asks for Ryu's, which the port does not
  ([`save-interchange.md`](save-interchange.md) §2).
- **Reversible?** play without `BOF3X_LANG`.
- **Checked:** captured 2026-09-21 in the new game's battle: "Whelp" in the
  turn banner and the status bar. **Not changed: saves.** A loaded save
  carries its own names, so a game begun without the overlay keeps its
  Chinese ones, and a game begun with it writes English names into its save -
  which then draw as whatever glyphs the single-byte codes name when played
  *without* the overlay. **Decided by the owner, 2026-09-21:** that is
  acceptable - saves stay as they are and show gibberish across a language
  switch until a language-independent name system exists.
- **Also: Manillo, the fish merchant** (identified by the owner). The PC
  keeps his name once, 马尼洛 in the 8-byte slot `0x669CD8`, which the
  battle (`0x52D1EC`, combatant `0x16`) and seven field functions copy 16
  bytes from; a kind-11 chunk writes "Manillo", exactly 8 bytes with its NUL,
  after checking the battle's read. On the US disc the name lives in the
  fishing module every fishing area carries (AREA030, 089, 129, ...), in a
  12-byte slot right after twelve bytes the PC still has at `0x6608CC` -
  the anchor `loc_build.py` finds it by. Applied at start-up, logged;
  not yet seen on screen.


### Zeros in the matrix product's padding bytes

- **ID:** DIV-0021
- **Date:** 2026-09-21
- **Subsystem:** platform (PSX library layer)
- **Original behaviour:** the matrix product `Gte_MulMatrix0` `0x5A7D70`
  builds its nine `s16` results on the stack and copies **five dwords** to
  the out, 20 bytes for an 18-byte result. So bytes 18 and 19 of every out, a
  PSX `MATRIX`'s alignment hole, get the stale stack at `esp + 0x3E`. That
  word is never written by the function, so it can't be reproduced, only
  replaced. Measured 2026-09-20 over an attract run of 98,305 calls
  ([`psx-library-layer.md`](psx-library-layer.md) §4): twelve distinct values,
  `0000` 76,199 times, `000E` 8,535, `6322` 7,964, and others. The three
  rotation builders multiply in place, so the word lands in the caller's
  matrix. From there `Gte_SetRotMatrix` carries it into `Gte_Matrix`, where
  no instruction in the image names those two bytes (`pe_xref`).
- **New behaviour:** ours writes `0000` there, which is what the original
  leaves three calls in four. The nine results and the translation are
  unchanged. The same zeros reach `Gte_Matrix` through `Camera_LoadMatrix`
  `0x57C070`, and each caller's matrix through the rotations.
- **Rationale:** the only alternatives are to copy stale stack in from
  somewhere else, or to leave the out's two bytes alone. Zeros make the
  product deterministic. Forcing the word to `FFFF` on every call changed
  nothing any check could see: the oracle was identical over 7,478 frames,
  arena, VRAM and CLUT dumps were identical, and the frame hash differed only
  at same-configuration noise. So the choice is not visible in play. It is
  still a difference, so it is ledgered. Owner's call, 2026-09-21: zeros.
- **Not covered:** an indexed read of `+0x12`, or a dword read at `+0x10`
  whose top half is used, in code the attract run does not reach. None was
  looked for beyond the GTE's own globals.
- **Also in the PSX version?** Unknown; not looked up. libgte's `MulMatrix0`
  by its documented shape stores nine halfwords and leaves the padding
  alone, which would make both the 2001 port's stale word and our zero
  divergences from it.
- **Verification:** start-up fuzz against the original, 29,856 rounds, 0
  mismatches outside the padding and ours zero in it every time
  ([`psx-library-layer.md`](psx-library-layer.md) §4.1); a 3-minute oracle
  identical; then the batch check of 2026-09-21 - full-cycle oracle over
  7,478 frames, arena, VRAM and CLUT dumps, and the frame hash on all 10,062
  frames, all identical to the original (`ab17_*`).
- **Reversible?** Yes: `BOF3X_ORIGINAL=Gte_MulMatrix0`. That puts back the
  original product, and every caller of ours reaches it. No config toggle.


### The game's clock starts with the game

- **ID:** DIV-0022
- **Date:** 2026-09-21
- **Subsystem:** platform (frame pacing)
- **Original behaviour:** WinMain paces logic frames against a deadline kept
  in a **32-bit float** at `0x6BC628`: a `GetTickCount` value in
  milliseconds, advanced by 33.334 a frame. `GetTickCount` counts from the
  last full boot of Windows, and past 2^24 ms a float cannot hold every
  millisecond, so the pace depends on how long Windows has been up
  ([`known-defects.md`](known-defects.md) D5). Measured on this machine,
  steady state after 30 s: 31.25 logic frames a second between 2^28 and 2^29
  ms (every run of 2026-09-20), 15.62 past 2^29 (every run of 2026-09-21),
  and past 2^30, reproduced with the switch below, 91.7 a second - the
  spin never waits, and DIV-0004's drain fired, which it does only after an
  unrendered frame: consistent with D5's "nothing drawn", not looked at on
  screen. **Fast Startup**, on by default, makes Shut down a hibernate that
  `GetTickCount` counts through, so a player who shuts down every night
  reaches half speed about a week after their last Restart.
- **New behaviour:** the exe's import slot for `GetTickCount`
  (`Imp_GetTickCount` `0x5C407C`, read once, by WinMain) points at a clock of
  ours: milliseconds since the DLL was injected, from `GetTickCount64`.
  WinMain is `GetTickCount`'s only caller in the exe (three calls, all its
  pacing and a once-a-second counter that only takes differences), so
  nothing else sees the change. The float code is untouched; it now sees the
  small numbers it was written for, as on a machine booted that day. Measured:
  **30.00** logic frames a second after 30 s.
- **Rationale:** a bug of the platform, not a design choice - the code
  assumes a tick count small enough for a float, which a 2001 machine booted
  that morning had and a 2026 one with Fast Startup does not. This is the
  owner's short-term fix (2026-09-21): four bytes, no game code changed. The
  complete one, the deadline in a double, is [`IDEAS.md`](IDEAS.md) I16.
- **Not covered:** one unbroken session still drifts through the float's
  bands - 30.0 under 35 minutes, 29.85-30.3 up to 4.7 hours, 29.4 to 9.3
  hours, 31.25 from there to 6.2 days, then half speed (I16 has the table).
  The fast-forward after focus loss is untouched (I12). Other processes and
  DLLs reading `GetTickCount` are unaffected: only the exe's own slot is
  changed.
- **Also in the PSX version?** Not applicable: the PlayStation paces by
  vertical blank, not by a clock.
- **Tooling that comes with it:** `BOF3X_TICK_BASE=N` (decimal or `0x`)
  starts our clock at N ms instead of 0, which puts the original's pacing
  code in any band of D5 on demand - how the three bands above were measured
  (`analysis/attract/clk_*`). `GameClock_Pause` / `GameClock_Resume` stop it
  while a recipe's frozen `shot` holds the game thread
  ([`input-script.md`](input-script.md) section 3), so the deadline has no
  debt to replay; only with `BOF3X_SHOT_WAIT` set, which only `input_run.py`
  sets.
- **Verification:** steady-state pace from the recordings, 30 s on: 30.00
  with the fix, 31.25 at base 2^28, 91.7 at base 2^30 (a run at base 2^29 never reached the attract
  sequence in its 90 s; that band is the real clock's, 15.62, `ab17_*`); the
  attract oracle
  identical over 3,607 frames with the fix (`clk_fix.tsv`). And a 6-minute traced run with
  the fix against the all-original reference: calls and hash identical on all
  10,062 frames of `ab17_orig` (`analysis/calltrace/clk_hash`) - the logic
  does not see the clock. **Confirmed in game by the owner, 2026-09-22:** the
  game's speed is recovered.
- **Reversible?** Yes: `BOF3X_ORIGINAL=Game_Clock` leaves the slot on
  Windows' clock. No config toggle.


### Zeros in a map cell's vertex padding

- **ID:** DIV-0023
- **Date:** 2026-09-21
- **Subsystem:** platform (PSX library layer, as DIV-0021)
- **Original behaviour:** the map-cell handler `MapCell_DrawQuads` `0x570020`
  builds each quad's vertices as 8-byte PSX `SVECTOR`s in its own stack
  frame and writes x, y and z but never the fourth word. `Gte_LoadVertex` and
  `Gte_LoadVertices3` copy whole dwords, so the stale stack there lands in
  the top halves of `Gte_Vertices[1]`, `[3]` and `[5]`. It is never written by
  the function, so it cannot be reproduced, only replaced. Every reference to
  `0x7DE468..0x7DE47F` in the image is a whole-dword store by those two
  loaders or an address `Gte_Rtps` / `Gte_Rtpt` hand to the projection, which
  reads x, y and z (`pe_xref --range`, 2026-09-21).
- **New behaviour:** ours writes `0000` there. Every other byte is unchanged.
- **Rationale:** the same choice as DIV-0021, for the same class of word.
  No instruction reads the two bytes, so nothing a player sees changes. It is
  still a difference, so it is ledgered. Owner's call, 2026-09-21: zeros, to
  match the matrix product.
- **Not covered:** a read of those halves by code outside the image (none
  exists in ours).
- **Also in the PSX version?** Unknown; not looked up.
- **Verification:** start-up fuzz against the original's whole call tree,
  `BOF3X_SHADOW=map_cells`: 24,000 rounds, 0 mismatches outside the padding,
  which differed 16,740 times with ours zero every time
  ([`sprite-draw-order.md`](sprite-draw-order.md) section 16); a control with
  ours writing `0001` is refused (11,290 mismatches). Live, a 7-minute attract
  run comparing every call against a clone: 8,192 calls, 0 mismatches, the
  pad word differing 16,095 times; then the batch check of 2026-09-21 -
  oracle, memory dump and frame hash identical (`ab18_*`).
- **Reversible?** Yes: `BOF3X_ORIGINAL=MapCell_DrawQuads`. No config toggle.
- **Also, 2026-09-22: `Sprite_ProjectA` `0x57B860`** (the party objects'
  screen update, [`field-frame.md`](field-frame.md) section 4) builds one
  vertex the same way and never writes its fourth word; `Gte_LoadVertex`
  carries the stale stack into the top half of `Gte_Vertices[3]`. Ours writes
  `0000`, under the same ruling - the same word, the same reader. Its fuzz
  compares x, y and z and leaves the pad out, so the zero there is by
  construction, not measured. `BOF3X_ORIGINAL=Sprite_ProjectA` restores it.


### A field fade past its jump table stops instead of jumping

- **ID:** DIV-0024
- **Date:** 2026-09-22
- **Subsystem:** field objects (the kind handlers, [`object-kinds.md`](object-kinds.md))
- **Original behaviour:** `Field_ObjectFadeOut` `0x5193B0` and
  `Field_ObjectFadeIn` `0x5194E0` dispatch on the sprite's sub-state byte
  `+4` through jump tables with no bound (`0x65F654`, `0x65F65C`, adjacent).
  Fade-out's 2 and 3 land in fade-in's cases; fade-out from 4 and fade-in
  from 2 land in the cases of another function (`0x65F664`'s, at `0x5198xx`),
  run in the wrong frame.
- **New behaviour:** ours runs fade-out's 2 and 3 as the original does, and
  past them calls `Fatal`, naming the function and the sub-state.
- **Rationale:** what the original does there is a jump into the middle of
  another function's body with this one's stack frame; it cannot be
  reproduced, only imitated wrongly, and CLAUDE.md rule 4 says a
  reimplementation that cannot do the original's thing aborts loudly.
  Nothing is known to reach it: these two functions only ever set `+4` to
  0 and 1.
- **Not covered:** a sub-state set to 4 or more by code outside the two
  functions - unmeasured in game.
- **Also in the PSX version?** The PSX twins `0x801A459C` / `0x801A47DC`
  have the same unbounded dispatch (object-kinds.md).
- **Verification:** the start-up fuzz, `BOF3X_SHADOW=object_kinds`, seeds
  sub-states 0..3 only; controls in object-kinds.md.
- **Reversible?** Yes: `BOF3X_ORIGINAL=Field_ObjectFadeOut,Field_ObjectFadeIn`.

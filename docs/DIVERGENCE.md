# Divergence ledger

**Status:** IN PROGRESS (opened 2026-09-18; 9 entries, DIV-0001..0009)

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
  (the title's (R) mark is a pixel shorter). **Not yet seen: the menu numerals
  themselves** - the attract sequence draws none; owner to look.

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


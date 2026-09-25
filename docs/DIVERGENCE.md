# Divergence ledger

**Status:** IN PROGRESS (opened 2026-09-18; 57 entries, DIV-0001..0057)

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
- **2026-09-22: the stepper itself is ours.** `MsgBox_Step` `0x497840` is
  reimplemented in `src/game/msgbox.cpp` ([`msgbox.md`](msgbox.md)), so the
  rule no longer reaches it through a patched byte: ours *calls*
  `MsgBox_DrawChar` where the original called `Text_DrawAt`, and the rule
  still lives in `src/game/text_advance.cpp`, unchanged. The `RetargetCall`
  at `0x497A22` stays exactly where it was - it is what keeps this entry
  alive when the owner runs `BOF3X_ORIGINAL=MsgBox_Step` and Capcom's body
  executes. No behavioural change: with no advance table loaded
  `MsgBox_DrawChar` is `Text_DrawAt`, and the fuzz compares ours against a
  clone with both sides calling the same stand-in. `Text_DrawAt` `0x516B30`
  itself is now ours too, faithful.
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
- **Rationale (why this side):** measured 2026-09-20 over every operand of every function
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
- **Rationale:** the names the menus draw are compiled into `BOF3.exe`,
  not carried in any `DAT`, so the overlays of DIV-0005 cannot reach them.
  Without this, an English game shows Chinese item and ability names in
  every menu. The port's 16-byte field holds every US name, so only the name
  bytes are replaced and the numbers stay Capcom's.
- **Not covered:** enemy names (12-byte fields in battle data,
  [`DAT_CONTAINER.md`](DAT_CONTAINER.md); since DIV-0053), character names, place names,
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
- **Also in the PSX version?** no - the US release, this entry's original,
  advances every character by 8 (above).
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
  w >= 8). The near edge and the table `0x7CA9E0` are untouched. From
  2026-09-20 implemented as the original handlers' own bytes, copied, with
  the two far-edge operands re-aimed at our table (`gfx_sprite_uv.cpp`, gone);
  **since 2026-09-23 inside our reimplementation of the three**
  (`src/game/sprt_draw.cpp`, [`sprt-draw.md`](sprt-draw.md)): the handlers
  read the far edge at a base of ours, `4 * (u + w)` on - Capcom's
  `0x7CA9DC` or our table - with the same texture coordinates as the copies
  (checked against re-aimed copies of Capcom's bytes, and the table's values
  pinned by hash). One difference from the copies, where neither was
  Capcom's: our table had 1,024 entries and SPRT's unchecked index read past
  it into our dll's memory for `u + w` of 1,024 and up; it now has an entry
  for every index a `u8 + u16` can make, the same line continued. At a
  render scale k other than 2 (DIV-0036) the table is refilled for k at
  set-up by the same derivation, `j + 0.012 - (8k - 15) / (2 (8k - 1))`,
  which is the value above at k = 2 (and k = 2 keeps the pinned table).
- **Rationale:** a bug, not a choice: the first and last texel are treated
  differently for no reason a design would have, and it cuts the base off every
  `2`. The obvious fix (far edge `u + w`) was built first and is wrong - it
  blends in the neighbouring cell and drew seams through the title logo
  (`analysis/d1/fix1`, 2026-09-20).
- **Also in the PSX version?** no - the PlayStation GPU does not filter and
  copies a sprite texel for texel.
- **Reversible?** Yes, two ways, since 2026-09-23 (the owner's names of
  2026-09-20 still work):
  `BOF3X_ORIGINAL=SpriteFarEdge` runs our three handlers with Capcom's far
  edge, `tc[u + w - 1]` - the divergence alone off, everything else ours;
  `BOF3X_ORIGINAL=D3d_DrawSprt,D3d_DrawSprt8,D3d_DrawSprt16` runs Capcom's
  handlers themselves, so DIV-0010 is off with them (one name per handler -
  `D3d_DrawSprt8` alone switches off the 8 x 8 font's fix and nothing
  else). With both, the handlers are Capcom's. Either gives exactly
  Capcom's texture coordinates: `BOF3X_SHADOW=sprt_draw` checks ours with
  `SpriteFarEdge` off byte for byte against copies of Capcom's bytes.
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
  **Since then (noted 2026-09-24):** the owner has seen Config's panel frame
  in game - "looks right" ([`HANDOFF.md`](HANDOFF.md) item 0a). The reserve
  list's frame on "change party members" was seen in the owner's session
  (`analysis/d1/point/s009.png`) but the owner has not commented: still open.

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
  **Amended 2026-09-25:** `LoadDatFile`'s kind-3 case sets the same bound
  before it calls `Font_SetGlyphData`. Found by the combat route on the A/B
  original side (`--original "*,-LoadDatFile,..,-Text_DrawString,.."`):
  there `Font_SetGlyphData` is Capcom's, the bound stayed `0xA00`, and
  DIV-0052's EX suffix at glyph `0xA6B` tripped the guard at the first
  command phase - the same fault, every run, with and without the tracer;
  the all-ours side played the route through. The bound now follows the
  table loaded on either side.
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
- **Also in the PSX version?** Unknown: the pairing names `0x801A459C` /
  `0x801A47DC` as the twins, but the sibling's Ghidra output has no function
  there and neither body was read (object-kinds.md section 1).
- **Verification:** the start-up fuzz, `BOF3X_SHADOW=object_kinds`, seeds
  sub-states 0..3 only; controls in object-kinds.md.
- **Reversible?** Yes: `BOF3X_ORIGINAL=Field_ObjectFadeOut,Field_ObjectFadeIn`.

### Glyphs sample texel centres

- **ID:** DIV-0025
- **Date:** 2026-09-22
- **Subsystem:** renderer (the glyph handler, [`glyph-draw.md`](glyph-draw.md))
- **Original behaviour:** `D3d_DrawGlyph` `0x5A2900`, the Direct3D draw of
  every glyph (primitive code `0x6C`), sets each corner's texture coordinate
  to `2u / 32`, `2v / 32` - the glyph's 24 texels on a 24-pixel quad, 1:1,
  with no half-texel offset. Every pixel's sample point then falls exactly
  on the edge between two texels: under point filtering the winner is the
  interpolator's rounding, different in each of the quad's two triangles;
  under bilinear every pixel is a 50 / 50 blend
  ([`known-defects.md`](known-defects.md) D17). Seen by the owner in English,
  point filter: strokes one, two or three pixels wide, an `l` narrowing where
  the quad's diagonal crosses it; soft text under the default bilinear.
- **New behaviour:** `(2u + 0.5) / 32` and `(2v + 0.5) / 32` on every corner:
  both edges move half a texel, the scale stays 1:1, and pixel k of the quad
  samples the centre of texel k. Positions, colours, the draw and every call
  are the original's. All Chinese and Latin text alike.
- **Rationale:** the owner asked for it, 2026-09-22, after comparing the
  same line against the sibling's recompiled PSX build ("wobbly" against
  even). The sprite handlers already have a deliberate texel inset
  (`0x7CA9E0`, `(i + 0.512) / 256`); the glyph handler never got one.
- **Also in the PSX version?** No: the PlayStation's GPU samples texels by
  integer coordinates and has no such edge.
- **Verification:** `BOF3X_SHADOW=glyph_draw`: 20,000 rounds with the fix
  checked to change exactly the eight `tu` / `tv` floats by exactly `1/64`
  against Capcom's copy, and nothing else; 20,000 with the fix off checked
  byte for byte. 39 negative controls, all refused
  ([`glyph-draw.md`](glyph-draw.md) §5). **Confirmed in game by the owner,
  2026-09-23**, English, point filter: "the text looks straight now".
- **Reversible?** Yes: `BOF3X_ORIGINAL=GlyphTexelCentres` (our function,
  Capcom's arithmetic) or `BOF3X_ORIGINAL=D3d_DrawGlyph` (Capcom's function).

### The Config screen's controller panel: names at their own width, in the dialogue font, inside a wider frame

- **ID:** DIV-0026
- **Date:** 2026-09-22
- **Subsystem:** menu (Config, [`config-screen.md`](config-screen.md) §8)
- **Original behaviour:** the controller panel's row draw `0x461AF0` places
  each name at `row x + 0x20 - len * 6` and draws it through the large
  `Text_DrawAt` - right-aligned for Chinese, two bytes and 12 units a
  character. With DIV-0015/0016's names (two bytes a character, advance 8)
  every name started 4 units a character too far left, ragged, and in the
  large quad the tripled 8 x 8 cells crowded (the owner's screenshot,
  2026-09-22: 4 letters at 315 px, 5 at 292, 6 at 268).
- **New behaviour:** width `len * 4` (`0x461B36`: `lea eax, [ecx+ecx*2]` ->
  `[ecx+ecx]`, the `shl eax, 1` after it kept) and the draw re-aimed at
  `ConfigText_DrawSelected` (`0x461B43`), which swaps the UI cells for the
  dialogue font's - the pair DIV-0017 applied to the selected row. Then,
  after the owner's look in game (2026-09-23: "Change" and "Action" still
  began left of the frame, the rows ran past its right side): the right
  edge from `row x + 0x20` to `row x + 0x36` (`0x461B3F`: `83 C1 20` ->
  `83 C1 36`, 22 units in, every name still left of the separator at
  `x + 0x3F`), and the panel's frame (DIV-0011's `Menu_DrawFrame`, the call
  at `0x461A84`) from 0xC cells to 0xF (`0x461A61`: `push 0xC` ->
  `push 0xF`; 0xE, the first try, still left the rows' boxes past it).
- **Rationale:** the owner's report; the same fix as DIV-0017, which the
  owner judged right in game; the edge and the frame at the owner's request.
- **Also in the PSX version?** Not applicable: the text is the overlay's.
- **Verification:** `tools/recipes/config_controller.txt` (six downs reach
  Controller), English, point filter: `analysis/shots/ctrl_fix2`.
  **Confirmed in game by the owner, 2026-09-23**: the words "look right
  now", the frame "Perfect!".
- **Reversible?** Yes: `BOF3X_ORIGINAL=ConfigController`. Only under a
  language overlay, not with `BOF3X_LANG=original`.

### The Yes / No chooser laid out for Latin text

- **ID:** DIV-0027
- **Date:** 2026-09-22
- **Subsystem:** menu (the chooser `Menu_YesNo` `0x5747D0`,
  [`glyph-draw.md`](glyph-draw.md) §7)
- **Original behaviour:** the chooser under "OK to overwrite?" (and "Do you
  want to save?", "Load game?", "Is this what you want?") draws system
  message `0xF` at x `0x1C` and the pointing hand at `0xFE - 36 * selection`.
  The words' places are the line's own spaces; the hand's stops, 218 and 254,
  were fitted to the Chinese line's words at 220 and 256. The English line
  (27 spaces, `Yes`, 1 space, `No`, 8 units a character) puts them at 244
  and 276, so the hand stops 22 to 25 units short of each and on No covers
  the `Y` (the owner's screenshots at an inn, 2026-09-22).
- **New behaviour:** the owner's layout
  ([`dialogue-localisation.md`](dialogue-localisation.md) §6 item 8): the line
  with three spaces moved from its lead into its gap, so `Yes` starts at 220
  (3 units right of the left hand's tip) and `No` stays at 276; the hand at
  `0x112 - 56 * selection` - 218 on Yes as before, 274 on No, its tip 3 units
  before `No` (`0x5747F0` `mov ecx, 0x112`, `0x5747F7` `imul eax, eax, 56`,
  `0x5747FC` three `nop`s; `Msg_SystemPtr`'s call at `0x5747D2` re-aimed at
  the re-spacing).
- **Rationale:** the owner's mockup, 2026-09-22: not the PlayStation's
  layout (whose hand on No covers "es"), but the Chinese build's look - the
  hand beside each word, touching neither.
- **Also in the PSX version?** The US disc moved the stops too (the recomp's
  hand tips at about 246 and 277 units); its code is unread.
- **Verification:** the re-spacing at start-up (`BOF3X_SHADOW=yes_no_layout`,
  both line shapes). **Confirmed in game by the owner, 2026-09-23**, at an
  inn's save: "looked right". All four prompts change together.
- **Reversible?** Yes: `BOF3X_ORIGINAL=YesNoLayout`. Only under a language
  overlay, not with `BOF3X_LANG=original`.

### Music fades step once per logic frame

- **ID:** DIV-0028
- **Date:** 2026-09-22
- **Subsystem:** sound (`Sound_Tick` `0x587C70`, [`sound.md`](sound.md))
- **Original behaviour:** the fades (`Music_FadeIn` / `FadeOut` /
  `FadeOutStop`, `Music_Play`'s fade in, the event ops `B4` `B5` `B9`..`BB`)
  take a count in frames - op `B4 tt 08` is an 8-frame fade in - and
  `Sound_Tick` takes one step per call. Its only caller is WinMain's wait for
  the next frame (`0x4FCEBC`), which calls it on every spin: some 416 a frame
  in a traced run, more at full speed. So every fade is over within a
  fraction of one frame: music starts at full volume and fade-outs are cuts
  ([`known-defects.md`](known-defects.md) D26).
- **New behaviour:** a step is taken only on the first `Sound_Tick` after
  the frame deadline `0x6BC628` has moved - once per logic frame, replayed
  frames included - so an 8-frame fade lasts 8 logic frames, about 0.27 s
  at the port's 30 a second. The first step of a new fade is at once. The
  steps, the volume arithmetic, the stop at the end and the pump are the
  original's. **What the game sees does not change** (corrected 2026-09-23,
  after the `ab25` frame hash): a stopping fade still marks the music
  stopped (`Music_Track` 0xFF) at the first `Sound_Tick` after it starts, as
  the original's instant fade did, and a music command after that tick
  (`Music_Play`, another fade) completes the stop first. The first build
  let `Music_Play` of the same track inside the fade do nothing and then
  stopped the music - silence where the original restarts a track, frame
  3439 of the attract sequence.
- **Rationale:** the owner remembers the PlayStation fading music in and
  out, and asked for the fix on 2026-09-22 ("8 frames is like a quarter
  second? That sounds pretty close to how I remember it"), to judge in play.
- **Also in the PSX version?** No - the PSX counts the same fades in frames
  and they are audible (owner's recollection); the per-spin step is the
  PC port's.
- **Verification:** `BOF3X_SHADOW=sound`: the takeover's fuzz runs the
  original per-call path (0 mismatches, 46,000 rounds); then an 8-frame
  stopping fade through `Sound_Tick` with counting stand-ins takes exactly
  one step per frame over 8 frames of 5 spins and stops once. Negative
  control: with the deadline test removed the self-test is refused (8 steps
  in the first frame, a Fatal). A second case: `Music_Play` of the fading
  track is ignored in the asking frame and restarts the track after the
  first tick; both controls (no completion, no marking) are refused. Frame
  hash, all ours with the fix on against all original: see
  [`sound.md`](sound.md). **Confirmed in game by the owner,
  2026-09-23**: "the fade sounds great".
- **Reversible?** Yes: `BOF3X_ORIGINAL=MusicFadePerFrame` (our function,
  Capcom's per-spin steps) or `BOF3X_ORIGINAL=Sound_Tick` (Capcom's
  function).

### The save / load slot's name two units further in

- **ID:** DIV-0029
- **Date:** 2026-09-22
- **Subsystem:** menus (the save / load slot panel `0x576960`)
- **Original behaviour:** the panel draws the slot's name - five bytes of
  the save header, copied to `0x904BA0` - through `Text_DrawAt` at the
  panel's x + `0x13` (`lea eax, [ebp + 0x13]` at `0x576A46`, the one call
  site, found live with a probe on `Text_DrawAt`). On screen the glyph's
  first two pixel columns are lost - at x 135 of 640 for slot 1. The
  Chinese glyphs have blank columns there; the English cells (the US
  8 x 12 letters doubled, DIV-0005) start at column 0, so an `R` loses its
  stem (owner's screenshot, and `analysis/shots/load_names`). What does the
  cutting is not established: the draw mode the panel sends first takes its
  texture window from whatever the caller left in `ebx`.
- **New behaviour:** x + `0x15`: the name two PSX units (four pixels)
  further in, the whole glyph past the cut with a pixel to spare. Five
  Latin letters (80 pixels) still end well inside the name box.
- **Rationale:** the owner's request, 2026-09-22 - the only such hard cut
  they have seen, so the fix is local to this screen rather than a margin
  for every Latin cell.
- **Also in the PSX version?** No cut there to fix; the PSX draws its own
  font.
- **Verification:** `tools/recipes/load_list.txt`, English, point filter:
  slot 1's `Ryu` whole (`analysis/shots/load_names3`), against the cut `R`
  before it (`load_names`). The save screen shares the panel and was not
  captured.
- **Reversible?** Yes: `BOF3X_ORIGINAL=SaveNameInset`. Only under a
  language overlay, not with `BOF3X_LANG=original` (it rides in
  `YesNoLayout_Inject`, `src/game/yes_no_layout.cpp`).

### The menu backdrop past Config's four draws nothing

- **ID:** DIV-0030
- **Date:** 2026-09-23
- **Subsystem:** menus (`Menu_DrawBackdrop` `0x575690`, [`menu-windows.md`](menu-windows.md) §3)
- **Original behaviour:** the backdrop's kind is Config's "Background" byte
  `0x903A5B`, which the Config screen keeps in 0..3 (`0x461239` /
  `0x46126D`) - four patterns. The function picks its CLUT from four words on
  its own stack by that byte with no bound (`mov cx, [esp + eax*2 + 0x24]` at
  `0x575729`), and its tile pattern's start from `0x66396C[kind]`, a table of
  four. A kind of 4 or more reads the return address, the argument, then the
  caller's frame for the CLUT, and `.data` past the table for the pattern.
  Poked into a running game with Capcom's function in place
  (`tools/recipes/backdrop_kinds.txt`, save 3's field menu,
  `analysis/shots/backdrop_kinds`): kinds 4, 5, 6, 7, 8, 16, 64 and 255 all
  showed **no backdrop** - the menu's windows on black - with no crash and
  no hang. A second run (`ab26b`, linear filter, the pixel DIVs off) agreed
  for every kind but **5, which drew speckled white tiles** over the whole
  backdrop: kind 5 reads the return address's high word, a fixed CLUT word
  `0x0057` - VRAM row 1, x `0x170`, inside the display framebuffer on the
  PlayStation's layout - so its "palette" is whatever pixels were there, and
  differs run to run. Kinds 4 and 6..255 were black both times (a CLUT that
  lands on zeros, which the PlayStation's convention draws transparent, would
  do it; not read).
- **New behaviour:** for a kind of 4 or more ours makes the same draw-mode
  and CLUT calls and then draws no tiles. Kinds 0..3 are Capcom's, faithful
  (the fuzz; the shop route's A/B is owed with the next batch - run in
  `ab26`, 35 of 35 identical: [`HANDOFF.md`](HANDOFF.md), round six's batch,
  and [`takeover-queue-round6.md`](takeover-queue-round6.md)).
- **Rationale:** the stack read cannot be reproduced in C++; the takeover
  first aborted there (CLAUDE.md rule 4), which turned what a player of the
  original sees - a black backdrop, or at kind 5 speckle that varies from
  run to run - into a crash. Drawing nothing copies what was seen at every
  kind but 5, not the mechanism; at 5 it replaces run-dependent noise with
  the same black. The owner, 2026-09-23: only four entries
  are valid, and past them the original turned black.
- **Not covered:** the packet pool: the original commits its garbage tiles
  (and a pattern of zero-height rectangles would commit more than a real
  kind); ours commits none, so the frame's packet use differs for such a
  save. Every kind past 8 but 16, 64 and 255 is unseen.
- **Also in the PSX version?** The twin `0x801DBCBC` was not read.
- **Verification:** `analysis/validate_ab26b.sh` step 2, Capcom's function
  against ours with the pixel DIVs off on both sides: kinds 0..4 and 6..255
  identical, 5 not (the speckle above).
- **Reversible?** Yes: `BOF3X_ORIGINAL=Menu_DrawBackdrop` (Capcom's function,
  stack read and all).

### Draw through Direct3D 11 behind DirectX 6's objects, with no exclusive display mode

- **ID:** DIV-0031
- **Date:** 2026-09-23
- **Subsystem:** display (`Display_Setup` `0x5A5160`, [`display-setup.md`](display-setup.md); the backend, [`render-backend.md`](render-backend.md))
- **Original behaviour:** the set-up enumerates DirectDraw drivers and their
  modes, keeps a synthetic "Software Render" device as record 0
  (`Cfg_RenderMode` `0x65DA48` = 0 selects it: the port's own rasterisers into
  system-memory surfaces), makes an `IDirectDraw4`, an `IDirect3D3` and a HAL
  `IDirect3DDevice3` on the device's smallest mode of at least 640 x 480 x 16
  - fullscreen an exclusive `SetDisplayMode` and a flip chain, windowed a
  fixed 640 x 480 window with a clipper - and, when the HAL device refuses
  the windowed back buffer, flips `Cfg_Fullscreen` to 1 for the rest of the
  process and repeats the set-up fullscreen. Every draw then goes through
  the `IDirect3DDevice3`, every texture through DirectDraw surfaces, and the
  frame is shown by `Blt` (windowed) or `Flip`. The FMVs take the screen
  through a second DirectDraw in exclusive mode.
- **New behaviour:** the seven object slots hold this project's own
  DirectDraw- and Direct3D-shaped objects (`src/render/render_shim.cpp`);
  they record the frame and a Direct3D 11 device draws it into a target of
  the logical picture's size times an integer scale (640 x 480 at the
  default `BOF3X_SCALE=2`) and presents it centred on the window at the
  largest integer scale the client area holds, black around it, through a
  point or bilinear sampler (`BOF3X_FILTER`). No display mode is ever set,
  `Cfg_Fullscreen` is never forced, and the software renderer is gone:
  `Cfg_RenderMode` 0 and 1 both take the hardware path (F7 still cycles
  the two names). The pixel-format records, the device caps and the scale
  are what the HAL device reported on the owner's machine on 2026-09-23, so
  the texture builders make the same texels. The GPU work runs on a fiber
  with a 1 MB stack, because the game calls the present from a 16 KB task
  stack.
- **Rationale:** the owner's UI overhaul ([`display-overhaul.md`](display-overhaul.md)):
  borderless and windowed modes without an exclusive mode-set, integer
  scaling, and a shader present pass all need the picture in a render
  target of our own. Route 2 there: one function taken over, no draw handler
  changed, since every handler already reaches DirectX only through these
  objects' vtables.
- **Not covered:** the FMVs still take their own DirectDraw and exclusive
  mode (`Fmv_Play` `0x59E360`, skipped when windowed); the software
  rasterisers, `D3d_AfterDraw`'s read-back of the back buffer (never seen
  requested; it ends the process loudly if it is), a `Lock` of the primary
  or back buffer, sub-rectangle locks, depth, fog and lighting states (none
  set by the game), texture formats other than RGB with masks.
- **Also in the PSX version?** No: the PlayStation's GPU. This is the port's
  layer.
- **Verification:** `analysis/validate_rb1.sh` (2026-09-23): the four-shot
  title and attract captures against Capcom's set-up and DirectDraw, point
  filter - the title and narration screens pixel-identical, the two field
  scenes 13 and 3 pixels apart, single texels on tile edges where the two
  rasterisers' edge rules differ; the 55-shot attract A/B, the frame hash,
  the oracle and the memory dump - results in [`render-backend.md`](render-backend.md) §5.
- **Reversible?** Yes: `BOF3X_ORIGINAL=Display_Setup` runs Capcom's set-up,
  and with it Capcom's DirectDraw, Direct3D 3 and every path above,
  untouched (`gfx_filter.cpp`'s patch of its filter bytes serves that path).

### Window modes: a resizable window, or a borderless window the size of the monitor

- **ID:** DIV-0032
- **Date:** 2026-09-23
- **Subsystem:** platform (`Game_WinMain` `0x4FCB00`, `Game_WndProc`
  `0x4FC6F0`, [`window-modes.md`](window-modes.md))
- **Original behaviour:** WinMain creates the window with style `0xCA0000`
  (`WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX`, not resizable) in both modes:
  windowed a 640 x 480 client centred on the desktop, fullscreen the desktop's
  size at 0,0 with the caption still on, and then `Display_Setup` takes an
  exclusive 640 x 480 x 16 mode over it. A 640 x 480 desktop forces
  fullscreen. F8 tears the display down (`Display_Teardown`), flips
  `Cfg_Fullscreen`, resizes the window and runs `Display_Setup` again; F7
  does the same with the next device record. The F7 device name, the F11
  frame rate and the F12 "Save OK" are drawn by `TextOut` through the back
  buffer's GDI device context (`Display_TextOut` `0x5A66B0`). Read whole
  2026-09-23 (`python tools/pe_disasm.py 0x4fcb00:450 0x4fc6f0:400`;
  `symbols.toml` has each branch).
- **New behaviour:** windowed is a `WS_OVERLAPPEDWINDOW` - resizable, with
  a 640 x 480 client to start, centred as before (since DIV-0036 the client
  of the launcher's window size, 320k x 240k) - and the backend presents
  at the largest integer scale the client holds, so dragging the window
  bigger is the scale control. Fullscreen (`Cfg_Fullscreen` from `BOF3.CFG`
  line 1, F8) is a borderless `WS_POPUP` window covering the monitor the
  window is on, with no caption: on a 1440-row monitor the 640 x 480 target
  shows at exactly three times, where the captioned window of the original
  left room for two. F8 changes the window's style and placement and
  nothing else - no teardown, no set-up; the windowed placement the player
  last had is restored when F8 comes back from borderless. F7 cycles the
  device name it shows and re-makes nothing (with DIV-0031 every device is
  the same backend). The three overlays go to `build/bof3x.log` (`overlay`
  lines) when the back buffer is the backend's, since its surface has no
  device context; with Capcom's set-up (`BOF3X_ORIGINAL=Display_Setup`)
  they are drawn as before. The 640 x 480 desktop rule is kept.
- **Rationale:** the display overhaul's step 3 ([`display-overhaul.md`](display-overhaul.md)
  §4a): window modes without a mode-set, so that the picture is an integer
  multiple of 320 x 240 whatever the monitor. The owner's report of
  2026-09-23 was the captioned "fullscreen" window: two times instead of
  three on a 3440 x 1440 desktop, with the title bar showing.
- **Also in the PSX version?** No: the PlayStation has no window. This is
  the port's layer.
- **Verification:** [`window-modes.md`](window-modes.md) §4.
- **Reversible?** `BOF3X_ORIGINAL=Game_WinMain,Game_WndProc,Cursor_Sync,Display_WindowMoved,Display_DeviceName`
  runs Capcom's window and loop (the FMV player is DIV-0035's switch). Not
  with the backend in fullscreen: Capcom's WinMain would make its
  captioned desktop-sized window again, which is the report above.

### The game keeps running while its window is not in front

- **ID:** DIV-0033
- **Date:** 2026-09-23
- **Subsystem:** platform (`Game_WndProc` `0x4FC6F0`, `Game_WinMain`
  `0x4FCB00`; [`IDEAS.md`](IDEAS.md) I12)
- **Original behaviour:** `WM_ACTIVATEAPP` with a zero word clears
  `App_Active` `0x6BC63B` and pauses the sound (`Sound_PauseAll`
  `0x587C30`); WinMain's loop pumps messages and does nothing else while the
  byte is 0, and the frame deadline keeps no relation to the clock, so on
  reactivation the time away is replayed as unrendered logic frames
  ([`windowed-mode.md`](windowed-mode.md) "Focus loss", measured
  2026-09-19). The DirectInput keyboard is opened `DISCL_NONEXCLUSIVE |
  DISCL_BACKGROUND` (`DInput_Init` `0x5A94C0`), so once the loop ran
  unfocused it would read every key typed into other windows.
- **New behaviour:** deactivation leaves `App_Active` set and the sound
  playing; the loop runs, renders and paces as when in front. While the
  window is not the foreground application the six pad words
  (`Input_Held` / `Input_Previous` / `Input_Pressed` and pad 2's) are zeroed
  after `Input_Latch` (`src/hook/input_script.cpp`, `DeviceLatch`), so the
  game sees no input until it is in front again; a recipe's or the
  recorder's words are unaffected. `BOF3X_BACKGROUND=0` (the launcher's
  "Keep running when the window is not in front" box unticked,
  `background=0` in `bof3x.ini`) restores the original's freeze - and its
  replay, now bounded by DIV-0034.
- **Rationale:** the owner, 2026-09-19: every attract-oracle and
  memory-dump run took the PC away for minutes because the game had to be
  in front. `tools/attract_run.py` and `input_run.py` still foreground the
  window for their runs (a keypress elsewhere no longer reaches the game,
  but their captures want the window unobscured); an ordinary play session
  no longer has to stay in front.
- **Also in the PSX version?** No: a console has no other window.
- **Verification:** [`window-modes.md`](window-modes.md) §4.
- **Reversible?** Yes, `BOF3X_BACKGROUND=0` / the launcher box, without
  switching the loop back to Capcom's; `BOF3X_ORIGINAL=Game_WndProc` also
  restores it, along with the rest of WndProc.

### Frame debt is dropped, not replayed

- **ID:** DIV-0034
- **Date:** 2026-09-23
- **Subsystem:** platform (`Game_WinMain` `0x4FCB00`'s loop)
- **Original behaviour:** the deadline in `Frame_Deadline` `0x6BC628`
  advances 33.334 ms a logic frame and nothing clamps it: after any stall -
  the window inactive (DIV-0033 removes that one), a title-bar drag or
  resize (a modal loop inside `DispatchMessage`), a suspend - the loop runs
  30 logic frames per second of stall back to back, skipping every present,
  until the deadline catches up ([`windowed-mode.md`](windowed-mode.md)).
- **New behaviour:** when the loop finds the deadline more than 500 ms
  behind the clock, it restarts the deadline at now + 33.34 as at the
  loop's start and logs one `DIV-0034` line with the debt. The time away is
  dropped. A debt under 500 ms - up to fifteen frames - is still replayed
  as before, so ordinary hitches keep the original's catch-up.
- **Rationale:** the "obviously wanted fix" the focus-loss finding named
  and the owner asked for beside I12. What the game computes does not
  change - logic frames are deterministic from launch - only how many run
  after a stall.
- **Also in the PSX version?** No: the PlayStation's frame loop is
  VSync-driven.
- **Verification:** [`window-modes.md`](window-modes.md) §4.
- **Reversible?** With the loop: `BOF3X_ORIGINAL=Game_WinMain`. No switch
  of its own.

### The FMVs play into the window, at an integer scale, with no mode-set

- **ID:** DIV-0035
- **Date:** 2026-09-23
- **Subsystem:** platform (`Fmv_Play` `0x59E360`, [`replacing-mci.md`](replacing-mci.md))
- **Original behaviour:** with `Cfg_Fullscreen` set, `Fmv_Play` calls
  `Fmv_EnterFullscreen` `0x59E4F0`: `WS_POPUP` on the window, a second
  DirectDraw object, `SetCooperativeLevel(EXCLUSIVE | FULLSCREEN)` and
  `SetDisplayMode(640, 480, 16)` - twice before the title screen - then
  tells MCI `put vfw destination at 0 0 640 480`, plays, and afterwards
  releases the DirectDraw, restores the style and sizes the window to the
  desktop. Windowed, the same fixed 640 x 480 destination in the 640 x 480
  client. Read 2026-09-19 and again whole 2026-09-23 (`symbols.toml`).
- **New behaviour:** `Fmv_EnterFullscreen` is never called: no second
  DirectDraw, no display mode, whatever `Cfg_Fullscreen` says. The
  destination rectangle is computed from the window's client area: the
  largest integer multiple of 640 x 480 that fits, centred, the class's
  black brush around it; a client smaller than 640 x 480 gets the largest
  4:3 fit. In a 640 x 480 client that is the original's rectangle. The
  open with its disc-root retry, the subclass by `Fmv_WndProc`, the play,
  the modal pump, the skip on a key or a click, the stop and close and the
  restore of the window procedure are unchanged. The `DIV-0035` log line
  says where the video landed.
- **Rationale:** DIV-0032 has no exclusive mode for the FMV to take, and
  MCI draws into whatever window it is given. This is the first of the two
  ways [`display-overhaul.md`](display-overhaul.md) §4a offered, chosen by
  the owner 2026-09-23 over I7's bundled decoder, which stays on the list.
  The video is still Cinepak through `mciavi32` (DIV-0001).
- **Also in the PSX version?** No: the PlayStation streams its movies
  through the CD subsystem.
- **Verification:** [`window-modes.md`](window-modes.md) §4.
- **Reversible?** `BOF3X_ORIGINAL=Fmv_Play` runs Capcom's player, mode-set
  included when `Cfg_Fullscreen` is set - on a borderless DIV-0032 window
  that is untested. (Our WinMain reaches the player, the set-up and the
  window procedure through Capcom's addresses since the review of
  2026-09-23, so each name's switch holds on its own; before, it called
  ours directly and the switch did nothing under our WinMain.)

### The picture is drawn at an integer multiple of 320 x 240 chosen for the window

- **ID:** DIV-0036
- **Date:** 2026-09-23
- **Subsystem:** platform (`Display_Setup` `0x5A5160` ours since DIV-0031; `Game_WinMain` ours since DIV-0032; [`display-overhaul.md`](display-overhaul.md) §4b)
- **Original behaviour:** the picture is 640 x 480 - a 640 x 480 x 16
  display mode or a 640 x 480 client - at `D3d_ScaleX/Y` 2.0, and under
  DIV-0031 until now a 640 x 480 render target presented at the largest
  integer scale the client holds (`BOF3X_SCALE` could already set another
  multiple, from the environment only).
- **New behaviour:** the render target is 320k x 240k, `D3d_ScaleX/Y` = k,
  with k chosen once, at set-up: **a borderless window** (`display=fullscreen`)
  takes the largest k from 1 to 8 whose target fits its client - the
  monitor: k = 6, 1920 x 1440, on the owner's 3440 x 1440 - and **a window**
  takes the launcher's new "Window size" (`scale=2..8` in `bof3x.ini`, which
  sets `BOF3X_SCALE`; 2 when unset) and opens with a client of the target's
  size, k lowered while the frame would not fit the work area. A later resize
  or F8 changes only the present's scale of that target; a client smaller
  than the target now gets the largest fit of its shape instead of a crop
  from the top-left (reached by F8 back to a window from a borderless start).
  DIV-0010's far texture edge follows k (`SprtDraw_SetScale`, the same
  derivation). One `DIV-0036` log line gives the k and why.
- **Rationale:** the owner's rule, 2026-09-23: "windowed options should be
  the available fixed k values, fullscreen uses largest k that will fit", k
  picked at start-up only. Polygon edges and 3D geometry are then drawn at
  the monitor's resolution instead of being magnified from 640 x 480.
  Widescreen, when it comes, re-opens the choice (the owner).
- **Also in the PSX version?** No: the PlayStation draws 320 x 240.
- **Verification:** k = 3 run 2026-09-23: target 960 x 720, the far edge's
  inset -0.183652 for scale 3 (the exact last-pixel-centre value for an 8
  pixel sprite, computed independently). **Owed:** the owner's eye on the
  borderless window at k = 6 and a window at k = 3 (sprite edges, glyphs -
  still 640-res textures, point-scaled - and the full-screen tiles).
- **Reversible?** A window with the size at 640 x 480 (the default) is the
  original's picture; `BOF3X_SCALE=2` in the environment fixes a window at 2.
  A borderless window always fits its monitor. `BOF3X_ORIGINAL=Display_Setup`
  is Capcom's set-up, at 640 x 480.

### An optional CRT look: scanlines and halation

- **ID:** DIV-0037
- **Date:** 2026-09-23
- **Subsystem:** platform (the Direct3D 11 present, `src/render/crt.cpp`; [`crt-look.md`](crt-look.md))
- **Original behaviour:** the picture is shown as drawn - by DirectDraw's
  flip or Blt, or since DIV-0031 by the present scaling the render target
  onto the window, nearest or bilinear.
- **New behaviour:** with `BOF3X_PRESENT=crt` (the launcher's Look box,
  "CRT - scanlines and glow", `screen=crt`) the present draws the target
  through four passes of our own: a 320 x 240 linear-light shrink, a
  two-pass Gaussian glow, and a picture pass with a brightness-dependent
  Gaussian beam per game line and the glow added (halation). No curvature
  and no phosphor mask (a grille was built first and removed at the
  owner's request the same evening). The numbers are `BOF3X_CRT` knobs, printed on
  the `DIV-0037` log line. Off by default; the harnesses pin it off.
- **Rationale:** the owner, 2026-09-23: the display overhaul's presets
  (§4c) as "a pre-packaged deal" rather than a slang loader, modelled on
  libretro's `crt-easymode-halation` without the curvature. That shader is
  GPL; ours is written from the technique, not from its code (`CLAUDE.md`
  rule 5, [`LICENSING.md`](LICENSING.md) §4).
- **Also in the PSX version?** No - the console's picture went to a real
  CRT, which is what this imitates.
- **Verification:** [`crt-look.md`](crt-look.md) §4: a k = 3 capture shows
  the lines and the glow. The owner's eye owed.
- **Reversible?** Yes: the default, `BOF3X_PRESENT=clean`, or another Look.

### F9's pause lines in the overlay's language

- **ID:** DIV-0038
- **Date:** 2026-09-23 (per language 2026-09-25)
- **Subsystem:** text (`Pause_LinesGame` `0x66A418`, `Pause_LinesTitle` `0x66A448`; `src/game/pause_text.cpp`, `tools/loc_build.py` `PAUSE_LINES`, [`window-modes.md`](window-modes.md) §6)
- **Original behaviour:** F9 pauses and WinMain draws two Chinese lines at
  (100, 100) and (0x70, 0x80) through `Text_DrawAt` - in game "press F9
  again to return to the title / any other key to continue", on the title
  and in the attract sequence "press F9 again to quit the game / any other
  key returns to the title". The PC port's own strings in the exe; no disc
  has them, so the English overlays (DIV-0005) left them Chinese.
- **New behaviour:** once an English overlay's glyphs are installed, the
  four pointers point at English of ours - "Press F9 again for the title
  screen" / "Press any other key to continue", and "Press F9 again to quit
  the game" / "Any other key returns to the title" - and our WinMain
  centres each on its width. Without an English overlay nothing changes.

  **Amended 2026-09-25: one set per language.** The English lines were
  built into the DLL and applied on *any* overlay's advance table (kind 4),
  so the Japanese, French and German overlays showed the English too - under
  Japanese, drawn through the Japanese glyph table
  ([`new-code-audit.md`](new-code-audit.md) A2). Now `loc_build.py` writes
  each language's four lines, in that overlay's own encoding, as chunk kind
  14 in `FIRST.DAT`'s overlay, after its glyphs; the DLL re-aims the
  pointers at those. An overlay without one keeps Capcom's Chinese. The
  wording is ours, agreed with the owner 2026-09-25:
  - **fr** "Appuyez sur F9 pour l'écran titre" / "Une autre touche pour
    continuer"; "Appuyez sur F9 pour quitter" / "Une autre touche : écran
    titre".
  - **de** "F9 erneut: zum Titelbildschirm" / "Andere Taste:
    weiterspielen"; "F9 erneut: Spiel beenden" / "Andere Taste: zum
    Titelbild".
  - **ja** もういちど F9 で タイトルへ / ほかの キーで つづける; もういちど F9 で
    ゲームを おわる / ほかの キーで タイトルへ - kana only, since the table
    holds just the JP disc's kanji; F and 9 are the disc's own cells.
  - **en** as above.

  `loc_build.py` refuses a line with a character the overlay has no glyph
  for, or wider than the 320-unit screen: widest 280 units (en), 261 (fr),
  240 (de), 228 (ja), built off the owner's four discs 2026-09-25.
- **Rationale:** the owner, 2026-09-23, on seeing the pause: "lets
  translate that page as well". The wording is ours (the PlayStation has no
  such screen), kept to one line each so the original two-line layout
  stays.
- **Also in the PSX version?** No: the console has no F9.
- **Verification:** [`window-modes.md`](window-modes.md) §6: both pairs
  captured in game at k = 3 (English, 2026-09-23). The per-language build:
  the four overlays built with the chunk, the rest of `FIRST.DAT`'s overlay
  unchanged against the installed ones; the in-game check for fr, de and ja
  is owed ([`new-code-audit.md`](new-code-audit.md) A2).
- **Reversible?** `BOF3X_ORIGINAL=PauseText`, or no `BOF3X_LANG`.

### The window's title and the missing-disc box in English

- **ID:** DIV-0039
- **Date:** 2026-09-23
- **Subsystem:** platform (`Game_WinMain` `0x4FCB00`, `src/game/win_main.cpp`)
- **Original behaviour:** the window is created with the title `0x65DA78`,
  GBK 龙战士Ⅲ ("Breath of Fire III", the game's Chinese name), and a failed
  disc probe shows `0x65DA98` 请插入龙战士Ⅲ光盘！ ("please insert the Breath
  of Fire III disc!") captioned `0x65DAB0` 错误 ("error"). All three are GBK
  bytes handed to the ANSI calls, so outside a Chinese locale Windows shows
  them as mojibake - the title as "ÁúÕ½Ê¿¢ó".
- **New behaviour:** the title is "Breath of Fire III"; the box reads
  "Please insert the Breath of Fire III disc." captioned "Breath of Fire
  III". Always, whatever `BOF3X_LANG` says: this is Windows' chrome, not the
  game's text, and the GBK is unreadable on the locales it would show on.
- **Rationale:** the owner, 2026-09-23: "fix the chinese name of the exe in
  the top left of the window".
- **Also in the PSX version?** No window, no probe.
- **Verification:** built; the title is seen at the next run.
- **Reversible?** `BOF3X_ORIGINAL=Game_WinMain` (Capcom's window, with the
  rest of DIV-0032..0036).

### F7 and F11 do nothing

- **ID:** DIV-0040
- **Date:** 2026-09-23
- **Subsystem:** platform (`Game_WndProc` `0x4FC6F0`, `src/game/win_main.cpp`)
- **Original behaviour:** F7 tore the display down, took the next renderer
  device record, set it up again and showed its name for 0x78 frames; F11
  toggled a "Frame Rate = N" readout. Since DIV-0031 / DIV-0032, F7 only
  cycled a name and both readouts went to the log.
- **New behaviour:** neither key does anything, beyond ending a pause as
  every non-F9 key does. F8, F9 and F12 are unchanged.
- **Rationale:** the owner, 2026-09-23: "those aren't really necessary as
  function keys". With the backend there is one device.
- **Also in the PSX version?** No.
- **Verification:** built; to be pressed at the next run.
  **Since 2026-09-24:** F11 is taken again, by tooling - it saves the frame
  beside the DLL (`SaveFrameBesideDll`, `src/game/win_main.cpp`; the capture
  work of DIV-0049). Nothing of the game's behaviour changes with it, so it
  is not an entry of its own; F7 still does nothing.
- **Reversible?** `BOF3X_ORIGINAL=Game_WndProc` (Capcom's window procedure,
  which also brings back the unfocused freeze).

### A 426 x 240 picture: the view widened by 53 columns a side (survey build)

- **ID:** DIV-0041
- **Date:** 2026-09-23
- **Subsystem:** display (`src/game/widescreen.{h,cpp}`, `src/render/render_d3d11.cpp`,
  `src/game/display_setup.cpp`, `MapView_Build` `0x56EC00` ours in
  `src/game/map_layers.cpp`, `AreaMap_FrameAreaBD` `0x510780` Capcom's;
  [`widescreen.md`](widescreen.md))
- **Original behaviour:** the picture is the game's 320 x 240 view (at
  `D3d_ScaleX/Y`, DIV-0036), 4:3. Two culls keep primitives whose projected
  x lies in a fixed screen interval: `MapView_Build`'s terrain cell cull
  `[-50, 370]` (`fcomp` against the `.rdata` floats `0x5C4234` / `0x5C4230`)
  and `AreaMap_FrameAreaBD`'s frame pass, a wide range `[-200, 520]`
  (`0x5C4240` / `0x5C423C`, operands at `0x51097E` / `0x510991`) or a narrow
  `[-50, 370]` (the same two floats as the terrain cull, operands at
  `0x5109BB` / `0x5109D2`) chosen by a y-derived test.
- **New behaviour:** under `BOF3X_WIDE=1` the render target is 426k x 240k
  and every primitive's x is moved by 53k on the way to it, in the scene
  vertex shader: the game keeps drawing its 320-wide view with the
  projection centre at (160, 120), the view sits centred, and whatever the
  game already draws past its old edges - terrain, 3D geometry, scrolling
  layers - fills the 53 columns each side. `D3d_ScaleX/Y` stay k, the
  window opens 426k wide, a borderless window takes the largest k whose
  426k x 240k fits its monitor (6 on 3440 x 1440, full height), the CRT
  look's source is 426 x 240. The terrain cull becomes `[-117, 437]` and
  the frame pass's ranges `[-252, 572]` and `[-102, 422]`: for the latter
  the four operand addresses are re-aimed at floats in the dll (`PatchBytes`
  `Widescreen`); the `.rdata` floats are untouched, since our
  `MapView_Build` no longer reads them and nothing else does (image scan
  2026-09-23: six references, all in these two functions). Off (the
  default, and every oracle and hash run) nothing changes: the target is
  320k wide, the shift 0, the culls the original's.
- **Rationale:** the owner, 2026-09-23: 16:9 with **no crop** - the PSP's
  384 x 216 (12 rows cut) was offered and turned down; 426 = 240 x 16/9
  rounded down to even. The approach is Capcom's own from the PSP release
  ([`psp-widescreen.md`](psp-widescreen.md) §2.3: +32 in all 21 primitive
  converters, the same two culls widened, +46 and +31 for 32 columns; here
  +67 and +52 for 53, the PSP's margin beyond its columns kept). Shifting
  primitives rather than moving the projection centre means centred UI
  needs nothing and a missed element stays centred instead of drifting.
  **This is the survey build** ([`widescreen.md`](widescreen.md) §3e): the
  sprite and object culls, the full-frame fills and fades, and the
  edge-anchored UI are not yet re-authored, so their pops, bright bands and
  edge gaps are expected and are what the survey lists (§5 there).
- **Also in the PSX version?** No. The PSP release does the same shift, by
  32, into a cropped 384 x 216 window (§2.4 there).
- **Verification:** self-tests at 0 mismatches with `BOF3X_WIDE` 0 and 1
  (the patches log `patch ON Widescreen` x4 and one `DIV-0041` line;
  `Widescreen_Inject` runs last in `inject_all.cpp`, after every fuzz, so
  the fuzzes compare the original culls). Live 2026-09-23: the field recipe
  at k = 2, 852 x 480 captures (`analysis/shots/wide_field/`), the terrain
  continuous across all 852 columns, the sprite centred. The survey
  recipes' findings in `widescreen.md` §5; the same evening the menu
  backdrop (two more column pairs), the fade tile and the save menu's
  black tile (both (-53, 0) 426 x 240) were widened under
  `Widescreen_Live()`, and the terrain cull opened to `[-150, 470]` after
  the owner saw the map's corners pop while the attract sequence rotates
  it; self-tests 0 mismatches both ways (a first cut wrote -0.0 into the
  tile's x with the view off and the mode_flow fuzz caught it).
  The launcher's "Widescreen" box (`wide=1`) sets `BOF3X_WIDE=1`. Later
  the same night, fourteen slide-out bounds of the menu and shop boxes
  (window-task states, `kSlides`) moved outward by 53, after the owner's
  screenshots showed them hanging in the bands; recaptured clean.
  **The sky gradient, 2026-09-23 night** (the owner's screenshot of the hill
  area before the world map: the gradient over the middle 320 columns,
  the bands black): the area's backdrop is `AreaMap_EntryKind1` `0x571BE0`
  - now `AreaMap_DrawBackdrop`, ours in `src/game/area_backdrop.cpp`
  ([`area-backdrop.md`](area-backdrop.md)) - one POLY_G4 whose left x was a
  zeroed register, so no byte patch. Under `Widescreen_Live()` its corners
  are `(-53, 0)`, `(373, 0)`, `(-53, 240)`, `(373, 240)` in the game's
  320-wide units (`0.0f - wide` and `320.0f + wide`, `wide` = 53), the
  same rule as the fade tile and the save menu's black tile: the backend's
  shift of 53k and scale k put those on target columns 0 and 426k exactly,
  no band and no seam. Off, the quad is the original's (0, 0)..(320, 240)
  bit for bit, and that is what the start-up fuzz compares (60,000 rounds,
  0 mismatches; 16 planted changes refused, `area-backdrop.md` §4).
  Self-tests at 0 mismatches with `BOF3X_WIDE` 0 and 1 (313 lines, 800
  injects). **Owed for the sky:** the world-map route captured wide and
  narrow after the merge - the gradient across all 852 columns at k = 2
  in the hill and coast frames, the middle 640 identical to the narrow
  capture. Read on the way and not widened: `0x4112A0`, "the sky" of
  `widescreen.md` §3d, is `WorldMap_PinSprite` - a sprite pinned to (160,
  80), centred, so it needs nothing under the primitive shift. **Owed:**
  the oracle and the frame hash once with `BOF3X_WIDE=1` (a difference is
  a cull gating logic), the 55-shot attract A/B cropped to the middle 640
  columns, the owner's eye.
  **Since then (noted 2026-09-24):** the world-map route was captured wide
  after the merge, in the between-waves batch ("the wide captures with the
  sky filling the bands") and the wave-2 batch ("the wide captures none
  black") - [`takeover-queue-round7.md`](takeover-queue-round7.md) "Result".
  The middle-640 comparison against the narrow capture is not reported
  there. Still owed: the oracle and the frame hash with `BOF3X_WIDE=1`, the
  cropped attract A/B, the owner's eye.
- **Reversible?** Unset `BOF3X_WIDE` (the default). `BOF3X_ORIGINAL=Widescreen`
  keeps the frame pass's original ranges under a wide picture;
  `BOF3X_ORIGINAL=MapView_Build` the terrain cull's;
  `BOF3X_ORIGINAL=AreaMap_DrawBackdrop` the 320-wide sky.

### The window resizes freely; the picture snaps to whole multiples or fills the height

- **ID:** DIV-0042
- **Date:** 2026-09-23
- **Subsystem:** platform (`Game_WndProc` and `Game_WinMain` `src/game/win_main.cpp`,
  `Display_Setup` `src/game/display_setup.cpp`, `Fmv_Play` `src/game/fmv_play.cpp`,
  the backend `src/render/render_d3d11.cpp`; the launcher)
- **Original behaviour:** a fixed 640 x 480 client (not resizable). Under
  DIV-0036 until now: a window of a size picked in the launcher (2x..8x),
  the render target made once at that k, and a later resize only
  re-scaling the present.
- **New behaviour:** the launcher's size list is gone; a "Snap the picture
  to whole multiples of its size" box (`snap=` in `bof3x.ini`,
  `BOF3X_SNAP=0` off) chooses between two rules, and the window is
  resized instead. **Snap on (default):** a drag lands the client on a
  whole multiple of the picture (the nearest to the dragged height, 1..8;
  a side edge: to the dragged width), the render target is remade at that
  k between frames, and the present shows it 1:1 - or centred at the
  largest whole multiple when the client is not a multiple (a borderless
  window, as DIV-0036 had it). **Snap off:** a drag keeps the picture's
  aspect at the dragged height, the target is made at the smallest k whose
  picture is not smaller than the client's height (8 at most) and the
  present fits it to the client - the picture fills the height, never
  cropped, never enlarged. The target change is asked for on `WM_SIZE`
  and applied by the backend after the next present, then the set-up
  rewrites `D3d_ScaleX/Y`, DIV-0010's far-edge table, `Gfx_ScreenRect`
  and the primary and back surfaces' size. The FMVs (DIV-0035) follow the
  rule too and never resize the window: snapped, the largest whole
  multiple of 640 x 480 that fits, centred (a 3x window shows them at 2x
  with a border); fitted, the largest 4:3 fit. The last windowed placement
  is kept in `bof3x.window` beside the dll and used at the next start
  when it still lands on a monitor; the ini's `scale=` is only the first
  window's size before that file exists.
- **Rationale:** the owner, 2026-09-23: "give people the option to either
  stretch to window size, or choose a multiplier size window/fullscreen.
  If multiplier sized, they should snap to the appropriate size, and if
  stretchy, they should stretch on the height axis only (so the window
  always stays 4:3 / 16:9)"; "no need to show the literal x2/3/4 choices,
  since the window can be resized"; FMVs "shouldn't cause the window to
  resize if they are at a half-size for the game".
- **Also in the PSX version?** No.
- **Verification:** 2026-09-23, the window driven to four client sizes in
  each mode by `SetWindowPos` (which sends `WM_SIZE` but not `WM_SIZING`),
  the title logo's width measured:
  snap 871 / 1162 / 290 px for clients 1100 x 800 / 1280 x 960 / 700 x
  300 (3x / 4x / 1x, the picture centred), fit 968 / 1162 / 361 (3.33x /
  4x / 1.25x, the height filled); the log's `DIV-0042 target` lines at
  each change; the FMV at 1x in the 3x window with snap; `bof3x.window`
  written at `WM_CLOSE`. Self-tests 0 mismatches. **The owner, the same
  evening, dragging the window by hand: "snap works perfect", "stretch
  works perfect".** A rescale under the SatPixie look ran (three target
  changes, `analysis/shots/resize_satpixie/`); under the CRT look not
  measured (its textures are remade by `CrtResize` the same way).
- **Reversible?** `BOF3X_SNAP` unset and the window left at 2x is
  DIV-0036's picture; `BOF3X_ORIGINAL=Game_WndProc` is Capcom's window
  procedure (no `WM_SIZING`, no rescale).

### An optional second CRT look: SatPixie, with its parameters in the launcher

- **ID:** DIV-0043
- **Date:** 2026-09-23
- **Subsystem:** display (the present, `src/render/satpixie.{h,cpp}`,
  `src/render/render_d3d11.cpp`; the launcher's Look box and its Options
  dialog; [`crt-look.md`](crt-look.md) §5, [`THIRD_PARTY.md`](THIRD_PARTY.md))
- **Original behaviour:** the picture is presented as drawn (DIV-0037 added
  our own CRT look as an option).
- **New behaviour:** `BOF3X_PRESENT=satpixie` presents through a port of
  Conkwer's CRT-SatPixie (Mattias Gustavsson's newpixie CRT, forked):
  accumulate, blur across, blur down, then chromatic aberration, ghosting,
  rolling scanlines, a vignette, an optional shadow mask, filmic tone
  mapping, noise and flicker. Every parameter of the preset is a knob in
  `BOF3X_SATPIXIE` and a slider or switch in the launcher's "Options..."
  dialog (saved in `bof3x.ini` as `satpixie.<name>=`). Two defaults differ
  from the preset's: overscan crop off, and the vignette over the whole
  picture rather than a 4:3 shape. Off by default; every oracle and capture
  harness pins the clean present.
- **Rationale:** the owner, 2026-09-23: "I found a MIT/Open Source shader I
  like, so I was wondering if we could incorporate it in a sub-menu with
  sliders for the options". MIT with its notice kept is within
  `LICENSING.md` section 4; the notice is in `THIRD_PARTY.md`. The 4:3
  vignette on the wide view (DIV-0041) darkened the middle and left the
  bands bright - the owner's screenshot of the title's mural - hence the
  default.
- **Also in the PSX version?** No.
- **Verification:** 2026-09-23: compiles under the dll's HLSL 4.0 path; a
  live run on the title at 3x, wide, through three rescales
  (`analysis/shots/resize_satpixie/`): scanlines, fringing and the vignette
  as the preset draws them, no Fatal. The owner ran it on the title's
  mural. Owed: the owner's tuning; the dialog's sliders by hand (only
  tried by code).
- **Reversible?** The Look box's other entries; `BOF3X_PRESENT` unset.

### A primitive's corner at depth 0 is drawn at the nearest depth: the world map's compass needle

- **ID:** DIV-0044
- **Date:** 2026-09-23
- **Subsystem:** display (the Direct3D 11 backend's `DrawPrimitive`,
  `src/render/render_shim.cpp`; [`world-map.md`](world-map.md) §3,
  [`known-defects.md`](known-defects.md) D41)
- **Original behaviour:** the port's draw handlers give every corner the
  primitive's depth as `z` and `rhw = 0.1 / z` ([`d3d-draw.md`](d3d-draw.md)).
  A corner at depth 0 gets an infinite `rhw`, and Capcom's Direct3D 6 device
  draws nothing for the primitive. The one known is the world map's compass
  needle, `0x408530`: a Gouraud quad whose four corners `Gte_PrimDepths4_10B`
  puts at depths 1/4096, 0, 0, 0, so the PC port never shows it. The
  PlayStation draws it - a red-to-blue diamond that turns with the map (the
  owner's screenshots of the JP and US releases, 2026-09-23).
- **New behaviour:** a corner whose `rhw` is not at or below 409.6 - the
  value at depth 1/4096, the smallest the game otherwise hands the handlers;
  infinity and NaN included - is drawn at that nearest depth: `rhw` 409.6,
  `z` 1/4096. The first four are logged (`render: DrawPrimitive corner ...`).
  The backend runs no depth test, so only the perspective-correct
  interpolation sees the change.
- **Rationale:** before this, our backend divided by the infinity itself and
  collapsed those corners to the screen centre - the "purple triangle/plane"
  the owner asked about on the world map (a sliver from the dial to the
  party's position, the world-map A/B's 2,365 differing pixels per map
  frame). The intent is the needle the PlayStation draws; the owner, on
  seeing it: "Its what I remember - a red-blue compass marker".
- **Also in the PSX version?** No: its GPU has no per-corner depth to divide
  by, and the needle draws.
- **Verification:** 2026-09-23. Bisect: the sliver stayed with every draw
  handler Capcom's (`BOF3X_ORIGINAL` of the 18 `D3d_*`), and went with
  `Display_Setup` Capcom's - so the backend, not the primitive.
  `BOF3X_DRAWLOG_RGB=800080` logged the quad: corners at (72, -48) .. (92,
  20), `z` 0.000244 / 0 / 0 / 0, `rhw` 409.6 / inf / inf / inf, diffuse red,
  purple, purple, blue. With the clamp, `analysis/shots/sliver_fix/wm.png`
  shows the diamond in the dial; the world-map A/B re-run
  (`analysis/shots/worldmap_ours2`, `analysis/attract/worldmap_ours2_compare.log`)
  differs from Capcom's on the map frames by the needle alone (~650 pixels)
  plus the tile-edge scatter every field frame shows since `rb1`. Owed: the
  owner's eye in game, turning the map.
- **Reversible?** No switch of its own; `BOF3X_ORIGINAL=Display_Setup`
  returns to Capcom's DirectDraw device, where the needle vanishes again.

### The EXP and zenny won in battle, times a launcher slider

- **ID:** DIV-0045
- **Date:** 2026-09-24
- **Subsystem:** battle (`Battle_EnemyDefeated` `0x437470`,
  `src/game/battle_flow.cpp`; `src/game/cheats.cpp`; the launcher's
  "Cheats..." dialog; [`cheats.md`](cheats.md))
- **Original behaviour:** a fallen enemy's EXP (`+0x96`) and zenny (`+0x94`)
  are added to the battle totals `0x904AEC` / `0x904AF0` as they are.
- **New behaviour:** each yield is multiplied by `BOF3X_EXP` / `BOF3X_ZENNY`
  (0..50) on the way into its total; the enemy's record is left alone. The
  results screen, the per-member split and level-ups read the totals and
  follow. 1, and unset, is the original's; 0 grants nothing. Set from the
  launcher's Cheats dialog (`cheat.exp=` / `cheat.zenny=` in `bof3x.ini`).
- **Rationale:** the owner, 2026-09-24: "there's a few cheats created [in the
  sibling] - 2 for multiplying exp / zenny, and one for 100% steal chance.
  Can we replicate that function here". The sibling scales the enemy's
  record from a hook because the addition is overlay code there; here the
  addition is ours, so the multiplier goes where the number is used.
- **Also in the PSX version?** No; the sibling's `bof3.exp-boost` mod is the
  same feature as a plugin.
- **Verification:** 2026-09-24, headless with `BOF3X_EXP=10 BOF3X_ZENNY=3`:
  the log's `DIV-0045 EXP x10, zenny x3`; `BOF3X_EXP=99` is a Fatal. The
  `battle_flow` fuzz passes with the variables unset (21,000 rounds, 0
  mismatches). The same day, the recorded combat route under `BOF3X_EXP=10
  BOF3X_ZENNY=3` (`analysis/shots/combat_cheats` against `combat_clean2`),
  watched by the owner: "30xp scaling correctly from 3". Owed: the zenny
  line read off the results screen.
- **Reversible?** Yes: the sliders at 1, or the variables unset.

### Pilfer and Steal take the item whenever the enemy has one

- **ID:** DIV-0046
- **Date:** 2026-09-24
- **Subsystem:** battle (the Pilfer and Steal state steps `0x4B54F0` and
  `0x4F5140`, Capcom's; two `PatchBytes` in `src/game/cheats.cpp`; the
  launcher's "Cheats..." dialog; [`cheats.md`](cheats.md) §3)
- **Original behaviour:** each steal rolls `Rand() & 0xFF` against the
  enemy's steal level (`+0x1A`) through the table `[0, 1, 3, 6, 12, 16, 32,
  32]` times an agility tier 12 .. 4, and takes the drop-1 item (`+0x18`)
  on a pass - a 9..14 % chance on the sibling's anchor enemy.
- **New behaviour:** with `BOF3X_STEAL=1` the mask's immediate at `0x4B5691`
  and `0x4F51EF` is 0, so the random byte is 0 and the roll passes whenever
  the enemy's chance was above zero. An enemy at level 0 stays unstealable,
  an enemy with nothing still says so, and the routine clears the item on
  success, so the first attempt takes it and later ones find nothing. Off
  by default; `cheat.steal=1` in `bof3x.ini`.
- **Rationale:** the owner's request above. The sibling's `bof3.steal-always`
  mod is the same patch on the disc's overlays; the port's copies of the
  routine are the PSX's term for term, so its patch and its limits carry
  over. A byte patch rather than a takeover because the roll sits inside
  two state steps that are not yet ours and the divergence is one operand
  (DIV-0012's case).
- **Also in the PSX version?** No.
- **Verification:** 2026-09-24, headless with `BOF3X_STEAL=1`: `patch ON
  Cheat_StealAlways 6 bytes at 0x004B5691` and `0x004F51EF`, the expected
  bytes (the immediate with the compare after it) found at both. The same
  day, the recorded combat route (`tools/recipes/combat_ab.txt`) under
  `BOF3X_STEAL=1` against a clean run at the same window size: capture 960
  reads "You couldn't steal anything!" clean and "You grabbed Marbles!"
  with the cheat, and the item list at 1740..1860 has Marbles as a sixth
  entry (6/128); every other capture identical. The owner watched it:
  "Looked right to me, with the marbles steal".
- **Amended 2026-09-25 (round eight, group CJ):** Pilfer's step `0x4B54F0`
  is ours now (`Steal_Start`, `src/game/magic_fx_reached.cpp`) and never
  runs the patched body, so the takeover had silently dropped Pilfer's half
  of this cheat - the merged tree's shadow fuzz caught it (402 rounds under
  `BOF3X_STEAL=1`: the original's copy carries the patch, ours did not). Ours
  now masks the roll with `Cheats_PilferRollMask()`, the byte the patch
  leaves at `0x4B5691` read back after `Cheats_Inject`, so it rolls as the
  patched original does; the patch is still made, so
  `BOF3X_ORIGINAL=Steal_Start` keeps the cheat too. The fuzz passes with the
  variable set and unset. Steal's step `0x4F5140` is still Capcom's and
  still patched.
- **Reversible?** Yes: the switch off, `BOF3X_STEAL` unset, or
  `BOF3X_ORIGINAL=Cheat_StealAlways`.

### The frame period is the PlayStation's 29.97, and the deadline is a double

- **ID:** DIV-0047
- **Date:** 2026-09-24
- **Subsystem:** platform (`Game_WinMain` `0x4FCB00`'s loop,
  `src/game/win_main.cpp`; [`known-defects.md`](known-defects.md) D5,
  [`IDEAS.md`](IDEAS.md) I16)
- **Original behaviour:** the loop paces logic frames against a deadline in
  the 32-bit float `Frame_Deadline` `0x6BC628`, advanced by the double
  33.334 at `0x5C4218` (29.999 frames a second) and held under the 2^32
  tick wrap. A float's spacing grows with its size, so the pace drifted
  through bands as the clock grew - 31.25 a second past 2^28 ms, half speed
  past 2^29 (D5). DIV-0022 kept the clock small, which left one unbroken
  session drifting through the same bands from 35 minutes on (I16).
- **New behaviour:** the deadline is a base plus a count of frames times
  the period, in a double, and the period is the PlayStation's NTSC frame,
  1001 / 30 = 33.3667 ms (29.970 a second). The clock it is held against is
  `GameClock_NowMs`, milliseconds since the game started as a double from
  `QueryPerformanceCounter` (paused with DIV-0022's tick clock for a
  recipe's frozen shot), so the tick wrap needs no handling - and the tick
  slot's 15.6 ms steps no longer set the pace: it is still read for the
  once-a-second frame-rate text only. The float at `0x6BC628` is no longer
  written. DIV-0034's clamp restarts the base and
  the count. `BOF3X_FRAME_MS=n` (1..1000; tooling) sets another period -
  33.334 is the port's own.
- **Rationale:** the owner, 2026-09-24: "I would like the game to be back to
  the official 29.97 frame pace". The PlayStation paces by vertical blank at
  59.94 / 2; the port's 33.334 was its approximation. A count of frames
  times the period cannot drift, whatever the session's length.
- **Also in the PSX version?** The rate, yes - it is the PlayStation's; the
  mechanism is not applicable there.
- **Verification:** 2026-09-24. A 2.5-minute attract recording
  (`analysis/attract/pace_ntsc.tsv`): 29.971 logic frames a second from
  60 s to the end, 29.964 over 60..120 s, against 29.995 / 29.997 under
  DIV-0022 alone (`clk_fix.tsv`). The exact check, a 6-minute traced run
  (`analysis/calltrace/pace_ours`): **the per-frame call hash identical to
  the branch's latest reference (`wave2_ours`) on all 10,319 frames**, and
  against the all-original `wm1b_orig` differing on frame 0 alone, as
  `wave2_ours` does - logic counts frames and reads no clock, so every
  check keyed by `Frame_Counter` (the oracle, the frame hash, the memory
  dumps, the recipes' captures) is unaffected; only wall-clock-timed draw
  and pump calls move, which the hash's wall-clock exclusion already drops
  ([`call-trace.md`](call-trace.md) §6). The polled oracle
  (`attract_diff.py`, `clk_fix` against `pace_ntsc`) disagrees on 8
  transition frames, the sampler's one-frame jitter that an
  original-vs-original pair (`ab11_orig` / `ab11_origb`) shows too. Owed:
  the owner's eye over a long session (the bands began at 35 minutes).
- **Reversible?** With the loop: `BOF3X_ORIGINAL=Game_WinMain` (the float and
  33.334 again); `BOF3X_FRAME_MS=33.334` keeps the double at the port's rate.

### F1 toggles double speed

- **ID:** DIV-0048
- **Date:** 2026-09-24
- **Subsystem:** platform (`Game_WinMain` `0x4FCB00`'s loop and window
  procedure, `src/game/win_main.cpp`; [`IDEAS.md`](IDEAS.md) I17)
- **Original behaviour:** F1 does nothing; the loop's period is fixed.
- **New behaviour:** F1 toggles between the period (DIV-0047's 1001 / 30
  ms) and half of it: two logic frames per frame of wall time. The deadline
  is rebased at the change so it starts from the current deadline. "Speed
  x2" / "Speed x1" shows on screen for 120 frames, as F12's "Save OK" does;
  a held key's repeats do not toggle. Like every other key, F1 ends an F9
  pause. Off at start; not saved.
- **Rationale:** the owner, 2026-09-24: "Can we bind F1 to a '2x speed'
  function by halving the logic frame per second time?" - I17's mechanism.
  Logic counts frames and reads no clock, so what the game computes is the
  same at either speed: saves, the RNG, scripted events. When drawing cannot
  keep up the loop skips presents, as it does catching up after a stall,
  and DIV-0004 drains the upload queue after unrendered frames. The music
  is fed from the spin at wall-clock time and is not sped up.
- **Also in the PSX version?** No.
- **Tooling that comes with it:** `BOF3X_FPS_LOG=1` writes one `fps` line a
  second to the log - frames drawn, logic frames, the speed in force.
- **Verification:** 2026-09-24. An attract run with F1 posted to the window
  at 75 s (`analysis/attract/f1_speed.log`): 31.0 drawn / 31.0 logic a
  second before, then `DIV-0048 speed x2 (F1) at Frame_Counter 1857` and
  60.5 drawn / 60.5 logic a second over the next 75 s - every logic frame
  still drawn, no present skipped. The owner watched it: "it looks like
  its holding up pretty well to my eye". Where the skip begins, by period
  (`BOF3X_FRAME_MS`, `period_*.log`): 4x, 121.8 logic and 62.8 drawn a
  second; 1 ms, 1,015.6 logic and 64.4 drawn. **That 64 was the tick
  slot's granularity, not the display:** `BOF3X_FPS_LOG` timing put the
  draw branch at 0.1-1.2 ms and the logic at 0.04 ms, so time was never
  short, but the deadline was held against a clock that steps 15.6 ms at a
  time (1000 / 15.6 = 64), and every deadline but the first inside a step
  counted as late. With the deadline on `QueryPerformanceCounter`
  (DIV-0047, amended the same day): 4x, 121.8 drawn of 121.8 logic a
  second (`qpc_8.3417.log`); 1x, 29.971 a second from the recording
  (`qpc_33.3667.tsv`); the Config recipe's eight frozen shots with no
  DIV-0034 restart. What the 60 Hz display shows of 120 presents a second
  is its own business; nothing blocks on it (`BOF3X_VSYNC` unset).
  Those two runs first ended in `render: a draw uses a released surface`,
  our backend's guard: a frame skip across an area change left draws
  recorded against a surface the game then released, and the snapshot
  taken for exactly that case was unreachable - the release zeroed the
  surface's dimensions, the present swept its GPU object before the draws
  ran, and the guard tested the live pixels rather than the snapshot. Fixed
  the same day in `render_shim.cpp` / `render_d3d11.cpp`
  ([`render-backend.md`](render-backend.md)); the same path opens at 1x
  after any stall inside DIV-0034's 500 ms that spans an area change. Not a
  divergence - a defect of ours.
- **Reversible?** F1 again; with the loop, `BOF3X_ORIGINAL=Game_WinMain`.

### The logo videos keep playing when the window is not in front

- **ID:** DIV-0049
- **Date:** 2026-09-24
- **Subsystem:** platform (`Fmv_WndProc` `0x59E570`, ours in
  `src/game/fmv_play.cpp`; the window procedure `Fmv_Play` installs for the
  length of a video)
- **Original behaviour:** on `WM_ACTIVATEAPP` the procedure sends MCI
  `pause vfw` when the window is deactivated and `SetFocus` plus
  `resume vfw` when it is activated again (read 2026-09-19 and again
  2026-09-24: 0xD4 bytes, every case listed in the source; its default case
  is `DefWindowProcA`, not the saved procedure as `symbols.toml` had said).
  The videos stop whenever another window takes the focus - the owner's
  report, 2026-09-24.
- **New behaviour:** under DIV-0033 (the game keeps running when the window
  is not in front, the default) neither MCI command is sent, and the video
  plays on; the focus is still taken back on activation. `BOF3X_BACKGROUND=0`
  keeps the original pair. Everything else of the procedure is as the
  original: a key or a click ends the video, `MM_MCINOTIFY` at its end,
  `WM_DESTROY` sets the quit flag.
- **Rationale:** DIV-0033 made the loop keep running unfocused; the videos
  before it were the one part of the start-up still freezing. The owner,
  2026-09-24: "the startup videos freeze when focus is taken away, we should
  fix that".
- **Also in the PSX version?** Not applicable.
- **Verification:** 2026-09-24. An attract run with `WM_ACTIVATEAPP` (clear)
  posted to the window 4 s after launch and (set) at 12 s, the window left
  in the background: 1,326 logic frames in 60 s with ours - the loop began
  at about 15.8 s, as with no deactivation - against 1,088 with
  `BOF3X_ORIGINAL=Fmv_WndProc`, the loop at about 23.7 s: the 8 s of
  deactivation spent paused (`analysis/attract/fmv_ours.tsv`, `fmv_capcom.tsv`).
- **Reversible?** `BOF3X_BACKGROUND=0`, or `BOF3X_ORIGINAL=Fmv_WndProc`.

### The pad through SDL3; the keyboard as the original; the DirectInput joystick dropped

- **ID:** DIV-0050
- **Date:** 2026-09-24
- **Subsystem:** platform / input (`DInput_Init` `0x5A94C0`, `Pad_Read`
  `0x5A9700`, `DInput_Shutdown` `0x5A9690`, ours in `src/game/pad_read.cpp`;
  [`controls.md`](controls.md))
- **Original behaviour:** `DInput_Init` enumerates the first attached
  DirectInput joystick and `Pad_Read` maps it digitally: axes X / Y past half
  travel are the directions, buttons 0..3 cross / square / triangle / circle,
  buttons 5..9 R2 / L1 / R1 / start / select; the POV hat is never read, no
  button reaches L2, button 4 is unused, hot-plug does not exist. On an
  Xbox-class pad that is A = cross, B = square, X = triangle, Y = circle,
  RB = R2, Back = L1, Start = R1, the stick clicks start / select, and a dead
  d-pad. The keyboard is the DirectInput system keyboard, non-exclusive and
  background, through the 32-entry key table (`BOF3.CFG` lines 3+ or the
  default at `0x66C648`).
- **New behaviour:** the keyboard path is reproduced as it was (the shadow
  check `BOF3X_SHADOW=pad_read` runs a clone of the original with both device
  pointers null against ours over random key states and tables). The joystick
  enumeration is not run; SDL3's gamepad subsystem is started in its place
  and `Pad_Read` ORs in one pad's word - the first connected, re-opened on
  hot-plug: d-pad and left stick (half travel, the original's threshold) the
  directions, LB / RB L1 / R1, LT / RT past half travel L2 / R2, Start /
  Back start / select, the face buttons by position (south cross, east
  circle, west square, north triangle) or, with `BOF3X_PAD_LAYOUT=nintendo`,
  swapped in pairs; `auto` follows the pad's own button labels. The pad, like
  the keyboard, is read whether or not the window is in front (SDL's
  background-events hint), and DIV-0033 decides what becomes of the words.
  `DInput_Shutdown` releases what was opened and stops SDL. **Later the
  same day:** the launcher's Controls dialog and `bof3x.ini` hold the
  physical bindings (`src/input/bindings.h`); `BOF3X_KEYS` rewrites the
  game's own key table and `BOF3X_PAD` the pad map when they differ from
  the default, so a player who never opens the dialog gets exactly the
  above ([`controls.md`](controls.md) §6 step 2).
- **Rationale:** the owner, 2026-09-24: modern pads, one pad, a Nintendo
  toggle, SDL3 rather than XInput for DualSense and the community mapping
  database. Nothing of the PlayStation-level button swap (the Config panel's
  Controller row, save data) is touched: the physical map feeds the pad word,
  the save's words decide what the word does.
- **Also in the PSX version?** Not applicable.
- **Verification:** 2026-09-24, this machine. The shadow check: 20,000
  rounds, 0 differ, 17,200 non-zero words, 19,402 short tables. Live:
  `tools/recipes/config_controller.txt` played to its end with SDL up in the
  process (`analysis/shots/ctrl_sdl`, the panel as before), and
  `tools/key_probe.py` - Z, up, Enter and Q sent to the window as scancodes
  each set triangle, up, start and L2 in `Input_Held` and cleared on release
  (ALL OK). The first live run found `DInput_Init`'s arguments reversed in
  `symbols.toml` (it is (hinstance, hwnd)), corrected. **The pad side is
  unexercised**: no pad was attached; the owner's first plug-in is the test.
  **Later the same day** the owner plugged in an Xbox Series X pad (the log:
  `pad: opened Xbox Series X Controller`) and closing the window from the
  Config screen ended in `ResizeBuffers failed, HRESULT 0x80070057`. Two
  faults, neither the pad's, both ours and both fixed: the present read a
  destroyed window's client rectangle into an uninitialised `RECT` and
  handed the garbage to `ResizeBuffers` - a `main` build without this
  change fails the same way, 1 of 1 (`tools/close_probe.py`); and with
  that gone the game ran on without a window, 3 of 3, because SDL's HIDAPI
  device discovery pumps its message window from our thread and takes the
  `WM_QUIT` (`SDL_hidapi.c`, `PeekMessage` / `GetMessage` on `m_hwndMsg`;
  without SDL started the loop left normally, 1 of 1). Now `Show` returns
  when the window is gone, and `WM_DESTROY` sets `Game_QuitFlag`, which the
  loop checks first - what the original's `Fmv_WndProc` does on the same
  message. After the fix: closed at the title 2 of 2, every teardown step
  logged, the process gone. **Confirmed in game by the owner, 2026-09-24,
  with the Xbox Series X pad: "game feels good with the controller"."
- **Reversible?** `BOF3X_ORIGINAL=DInput_Init,Pad_Read,DInput_Shutdown`
  restores the DirectInput joystick; the SDL build stays linked.

### The Config screen's controller panel: the PlayStation's one icon column

- **ID:** DIV-0051
- **Date:** 2026-09-24
- **Subsystem:** menu (Config, [`controls.md`](controls.md) §2, §6 step 3;
  `src/game/config_text.cpp`, `tools/loc_build.py`)
- **Original behaviour:** the 2001 port draws two cells right of each action
  name: the keyboard key of the *default* table, hard-coded (D58), and a
  coloured glyph naming the PlayStation button - which in the port's glyph
  table is a circled numeral (circle ①, cross ②, triangle ③, R1 ⑥, L1 ⑤)
  or an X (square), the port's stand-ins; the PlayStation's shapes were
  never drawn (the Chinese capture `analysis/shots/ctrl_cn`, 2026-09-24).
  Under the English overlay those glyph slots hold the US letters (b, X, c,
  d, g, f) - DIV-0006's doubled font - so the column read as letters. The
  PlayStation screen has one column, the button icons.
- **New behaviour:** under an overlay language the second cell is not set up
  (`0x461BAC` jumps to the epilogue, whose `add esp` loses the block's three
  pushes), the row's dark box is 0x58 wide instead of 0x68, the frame is
  0xD cells (DIV-0026's 0xF is not applied; `push 0xC` at `0x461A61` made
  `push 0xD`, `src/game/config_text.cpp`), and the cell's
  draw `0x461C00` is ours: the PlayStation's icon for the row's button, six
  new glyphs `loc_build.py` builds from the disc's atlas - the four shapes
  from its 12 x 12 set at atlas y 48 (the owner's choice, 2026-09-24, over
  the 8 x 8 and 8 x 12 sets the atlas also holds), L1 and R1 composed of
  the dialogue capital and the 8 x 8 set's serifed 1, as the PlayStation's
  panel reads them (the owner spotted the serif) - each doubled to fill the
  24 x 24 glyph, body nibbles remapped to the letters' so the button's
  colour index applies. The row box is 0x58 wide (0x68), the frame 0xD
  cells (DIV-0026's 0xF; a cell past the box's end, as the frame stands
  off the rows at the left), the icon's quad at the call's x - 0x0C - the
  three settled by the owner's eye off the fourth and fifth captures
  ("half a glyph to the right, the box half a glyph shorter", then the
  frame "half a glyph" back out past the box), the sixth capture
  `analysis/shots/ctrl_icons6`. **Confirmed by the owner off that capture,
  2026-09-24: "That looks perfect to me".** The keyboard's live bindings
  are the launcher's Controls dialog (DIV-0050 step 2). Under
  `BOF3X_LANG=original` nothing changes.
- **Rationale:** the owner, 2026-09-24: "go back to the psx design"; the
  PlayStation's screen as the reference.
- **Also in the PSX version?** Not applicable - this is the PlayStation's
  layout brought back.
- **Verification:** 2026-09-24, `tools/recipes/config_controller.txt` in
  English after `loc_build.py all` (the table 2,667 glyphs, the icons at
  0xA65..0xA6A): `analysis/shots/ctrl_icons4/controller_panel.png`, the
  fourth build - the first used the 8 x 12 dialogue shapes, which the owner
  read as squashed; the second the 8 x 8 set, whose pre-coloured nibbles
  came out in the wrong colours; the third fixed the colours; the fourth is
  the 12 x 12 set with the composed labels. Six rows, one cell each, circle
  red, square pink, cross blue, triangle green, R1 and L1 white, the box
  ending four past the icon and the frame three past the box; the patches
  and the inject logged ON.
- **Reversible?** `BOF3X_LANG=original`, or `BOF3X_ORIGINAL=ConfigController`
  (the patches) and `Config_DrawControllerCell`.

### The battle banner's words and its EX suffix

- **ID:** DIV-0052
- **Date:** 2026-09-24
- **Subsystem:** battle (only with a language overlay;
  `src/game/battle_text.cpp`, `BattleBanner_SetMessage` in
  `src/game/battle_misc.cpp`, `tools/loc_build.py`)
- **Original behaviour:** the banner at the top of a fight shows an actor's
  name or one of twelve messages. `BattleBanner_SetMessage` `0x44A8E0` copies
  message k through the pointer table `0x669DE0` with `Str_CopyN(.., .., 8)`;
  `BattleBanner_ShowName` `0x44A990` appends message 1, while `0x904B7A` is
  set, to the name. Message 1 is a space and two picture glyphs, `0x8050`
  `0x8051`, the "EX" of an extra turn; message 3 is a second pair,
  `0x8052` `0x8053`. The other ten are Chinese. Under the English overlay,
  glyphs `0x50..0x53` are the single-byte slots of `v` `w` `x` `y`, which
  DIV-0006 paints with the US letters, so the extra-turn banner read
  "Rei vw" (owner's screenshot, 2026-09-24).
- **New behaviour:** a kind-12 chunk in the English `FIRST.DAT` carries the
  US `BATTLE.EMI`'s twelve messages, from its 13-byte slots at `0x801EB000`:
  Attack, EX, Examine, the second suffix, Defend, Charge, Reprisal, Critical,
  Lucky Strike, Instant Kill, Escape, Counter. `loc_build.py` puts the two
  suffixes' glyphs at `0xA6B..0xA6E`, after DIV-0051's icons, and re-encodes
  the US codes (`0x151B 0x151C`, `0x151F 0x1520`) to them. EX is the disc's
  own art, not the port's: one picture across two cells of the US atlas's
  12 x 12 row (y 60, x 156 and 168). The letters overlap, and palette
  entries 9..F are banded top to bottom, from pale through yellow and orange
  to pink. Each cell is doubled with its nibbles as they are. The overlay's
  text palette is the disc's (DIV-0013), whose row 0 holds that ramp. The
  owner found the port's redrawn EX (the shipped `0x50` `0x51`, first
  build) "pretty close" but not the PlayStation's fade or overlap. The
  second suffix's cells were not identified in the atlas, so it keeps the
  port's shipped glyphs `0x52` `0x53`. The PC's strings are packed 8 bytes apart (one
  12), too small for "Lucky Strike", so the engine keeps the strings itself
  and repoints the table, checking each shipped pointer first. The message
  copy is 12 bytes, the US count (`addiu a2, zero, 0xC` at `0x801DE988`).
  The banner text `0x904EC0` has 32 bytes before the next referenced global
  (`0x904EE0`). Under `BOF3X_LANG=original` nothing changes.
- **Rationale:** stage 2 (DIV-0005). The owner's US PlayStation screenshot
  shows "Momo EX" in the pink picture glyphs, and the owner confirmed that the
  English and Japanese releases draw it that way.
- **Also in the PSX version?** Yes. These are the PlayStation's strings and
  its copy length.
- **Verification:** 2026-09-24. `loc_build.py all` reports
  `battle messages: 12`, and only `en.FIRST.DAT` differs from the previous
  build. `BOF3X_SHADOW=battle_misc` gives 147,500 rounds, 0 mismatches (the
  shipped copy length, no overlay). The owner saw "Rei EX" in game with the
  port's glyphs, 2026-09-24. **Confirmed by the owner in game the same day
  with the disc's EX: "the EX looks good now".** **Owed:** a message banner
  (Attack, Critical, ...) seen in game.
- **Reversible?** play without `BOF3X_LANG`.

### Enemy names in the overlay's language

- **ID:** DIV-0053
- **Date:** 2026-09-24
- **Subsystem:** battle (only with a language overlay; `tools/loc_build.py`,
  data only)
- **Original behaviour:** each `AREAnnn.DAT` loads the area's eight enemy
  kinds as a kind-0 chunk at arena `0xC2000`, size `0x4A8`: a 0x48-byte
  header, then eight 0x8C-byte records whose first 12 bytes are the name,
  landing at `0x8C55C8` (`MessagePools` `0x803580` + `0xC2048`).
  `Battle_CopyEnemyData` `0x4946C0` copies the name into
  `0x93B9E0 + slot * 0x128`. The enemy status banners, the target banner and
  the actor banner draw it in Chinese.
- **New behaviour:** `loc_build.py all` takes each US `AREAnnn.EMI`'s section
  for `0x800E4000`. That section has the same header and the same records at
  stride 0x88 with an 8-byte name, every later byte the same. For each live
  record, the tool writes one 12-byte kind-0 chunk into `en.AREAnnn.DAT` at
  arena `0xC2048 + k * 0x8C`: the US name, one byte a letter
  (`encode_char`), NUL-padded. An area whose header or record numbers differ
  keeps its names, and so does a name that does not encode in 8 bytes. That
  is the most the banner (`Str_CopyN` 8) and the name window (count 8) draw.
  The engine is unchanged: DIV-0005's overlay walk lays the chunks over the
  shipped ones.
- **Rationale:** stage 2 (DIV-0005). The owner asked for the enemy names in
  English, 2026-09-24. The names are Capcom's US spellings, verbatim
  (`Berserkr`, `BlueGbln`).
- **Also in the PSX version?** Yes. These are the PlayStation's names.
- **Verification:** 2026-09-24, by a scratch comparison of all 200 areas and
  by `loc_build.py`'s own checks. The headers are identical, and **448 of 448
  live records match past the name byte for byte**. There are 168 distinct
  names, 3..8 bytes long, **448 written and 0 kept**. **Confirmed by the
  owner in game, 2026-09-24**: "Mage Goo" and "Eye Goo" in the enemy status
  banners ("the names look good").
- **Reversible?** play without `BOF3X_LANG`.

### French and German overlays, with the discs' accented glyphs

- **ID:** DIV-0054
- **Date:** 2026-09-24
- **Subsystem:** text / font (only with a language overlay;
  `tools/loc_build.py`, and one table in `src/game/config_text.cpp`)
- **Original behaviour:** the PC port shipped Chinese only. Each
  PlayStation language was its own build on its own SKU, with no language
  switching in any of them (`STATUS.md`, "A stated goal worth recording
  now").
- **New behaviour:** `loc_build.py all --lang fr|de` builds French and German
  overlays from the player's own French or German disc, the same way as the
  English ones (DIV-0005..0009, 0014..0020, 0052, 0053). With
  `BOF3X_LANG=fr` or `de` the game loads them. Three changes make that
  work:
  1. **Accented glyphs.** Both discs extend the English atlas's grid past
     code `0x93`: French to `0xAA`, German to `0xA7`, in both the 8 x 12 and
     the 8 x 8 sets. The US disc's cells there are empty. The tool finds the
     run from the atlas (`latin_extension`) and appends two blocks, dialogue
     cells at glyph `0xA80` and UI cells at `0xAA0`. These come after
     everything DIV-0016/0051/0052 placed, so an English table is unchanged.
     The Config screen's selected-row redraw swaps the second pair by a
     constant offset, as it does the first (`kUiExtGlyphs`).
  2. **Slot count from the PC file.** A text block's slot count was read off
     the donor's first pointer. The European tables do not always agree with
     it (15 FR and 11 DE areas), so the count is now the PC file's. Any slot
     whose donor pointer falls outside the block keeps the PC's own message.
  3. The Config trim accepts the accented codes as text.
- **Rationale:** the owner's request, 2026-09-24: the other official
  languages as options. Everything drawn comes from the player's discs.
- **Also in the PSX version?** The text and the glyphs are the French and
  German PlayStation releases'. Choosing among languages is ours.
- **Verification:** 2026-09-24, off the owner's discs.
  - **English is unchanged.** An English build with the change is byte for
    byte the build without it, in all 245 overlay files, with the same log.
  - **Both languages build end to end.** Font, Config, verbs, character
    names, merchant, battle labels and messages, the six name tables with 0
    kept, and 200 of 200 areas. French keeps 962 slots as shipped, German
    769, against English's 496.
  - **No Japanese leftovers leak through.** 6,600 French and 5,706 German
    accented messages were checked against the Japanese disc; none is an
    untranslated leftover.
  - **Not yet seen in game.**
- **Known gaps:**
  - **Out-of-range slots.** The extra kept slots (~466 FR, ~273 DE) are all
    donor pointers outside their block where the US disc has English text.
    The European builds shortened some tables, aimed some slots elsewhere
    (FR `AREA000` slot 57 is `0x4600` in a `0x24D3` block) and wrote text
    over some table tails (FR `AREA004`, `AREA094`). Whether the PC's
    scripts reach those slots is unread.
  - **Enemy names.** 37 FR and 54 DE enemy names stay Chinese: each has an
    accented letter, which is two bytes, so the name no longer fits the
    banner's 8.
  - **Title art and upscales.** The title menu stays as shipped: its letters
    were measured on the US sheet. `--glyphs` / `--upscaler` refuse a disc
    with accented cells, because the sheet covers 100 cells.
  - **Launcher.** The launcher's settings file knows only `en` and
    `original`. French and German need `BOF3X_LANG` set in the environment.
- **Reversible?** play without `BOF3X_LANG`, or with `en`.

### The world map's place plates from the donor disc

- **ID:** DIV-0055
- **Date:** 2026-09-24
- **Subsystem:** world map (only with a language overlay;
  `tools/loc_build.py`, data only)
- **Original behaviour:** the place plates on the ten world maps (`AREA016`,
  `033`, `045`, `065`, `087`, `088`, `115`, `121`, `151`, `152`) are paint
  on each map's page, kind-1 chunk `0x0E001000`. The port repainted them in
  Chinese over the Japanese disc's page: 4..7 tiles in one band.
- **New behaviour:** `loc_build.py all` puts four of the donor disc's
  sections for each world map into `<lang>.AREAnnn.DAT`, whole:
  - the page;
  - `0x800D3800` -> tag `0xB0000`, the map's sprite frames, the plates sized
    per name;
  - `0x800E3800` -> tag `0xC0000`;
  - the palette section (`0x8002D800` JP / `0x80035800` Western) -> tag
    `0xA000`, whose first CLUT is the plates'.

  The same code serves every language: each disc carries its own plates.
  The world map's code (in `BOF3.exe`, compiled from the JP overlay) and
  its map section `0x80104000` are untouched.
- **Rationale:** the owner's request, 2026-09-24 ("town banners"). These are
  Capcom's own localised plates, off the player's disc.
- **Also in the PSX version?** Yes, per language. These are those releases'
  pages.
- **Verification:** 2026-09-24.
  - **The PC's side is Japanese.** The PC's `0xA000`, `0xB0000` and
    `0xC0000` chunks are the JP disc's sections byte for byte in all ten
    areas.
  - **Nothing of the port's own is lost.** Every tile the PC changed lies
    inside the tiles each Western page replaces.
  - **The palette is needed.** Rendered, each disc's plates are right only
    under its own first CLUT of the palette section.
  - **English text is untouched.** An English build differs from the one
    before only in those ten files, whose earlier chunks are intact.
  - **Seen in game:** the owner's route `worldMapAndAreaTransition_ab`
    (`f01260`, `f01740`) captured in all three languages shows "Cedar
    Woods" / "Bois de Cèdres" / "Zedernwälder" and "McNeil" / "Dubois" /
    "McNeil" on frames sized to them. The owner saw "Cedar Woods" live.
- **Not yet seen:** `AREA065` (every Western disc) and `AREA121` (German).
  There the discs rearranged the page itself: 25..32 tiles move, and
  `0xC0000` and the block order of `0xB0000` change with them. The swap
  carries all of it, and is right unless the exe's world-map code
  addresses those blocks directly.
- **Reversible?** play without `BOF3X_LANG`.

### A Japanese overlay, and the layout switch for languages

- **ID:** DIV-0056
- **Date:** 2026-09-24
- **Subsystem:** text / font, and the layout switch for languages (only with
  a language overlay; `tools/loc_build.py`, `src/game/lang_layout.cpp`)
- **Original behaviour:** as DIV-0054, the port shipped Chinese only. The
  Japanese PlayStation release draws its text from `ENDKANJI.EMI`:
  - section 0 is 21 x 21 cells of 12 px, in which byte `b` below `0x5B` is
    cell `b`, a kana `b` from `0x5B` up is cell `b + 0x23`, and `0x15 nn` is
    cell `nn + 0x5B`;
  - section 1 is the kanji sheet, where `0x12nn` / `0x13nn` is cell
    `code - 0x1200`.

  The sibling repo read this off the JP EXE's mapper `0x80151F4C`
  (`docs/TEXT_ENGINE.md`), and it was re-measured here.
- **New behaviour:**
  1. **The Japanese overlay.** `loc_build.py all` on a disc that boots
     `SLPS_` (`is_japanese`) builds a Japanese overlay:
     - both sheets whole, 882 cells doubled to the PC's 24 x 24, from glyph
       `0x993`, on the PC's own 12-unit advance;
     - every code two bytes, except the two hanging brackets `「` `『`. The PC
       hangs the same two at `0x2A` / `0x3C`, so they keep those bytes with
       the disc's glyphs painted over the port's;
     - `0xFF` as a space;
     - the kanji lead bytes converted in dialogue, pools, item and ability
       names and enemy names;
     - the JP white text palette at `0x8002B800`, the JP system pool at
       `0x80014000`, and the JP name records' 8-byte field;
     - the world-map plates as DIV-0055.

     The exe's own strings (kinds 7..12: Config, verbs, default names,
     merchant, battle labels and messages) are not built for a Japanese
     disc, so they stay Chinese. The exception, since 2026-09-25, is F9's
     pause lines (kind 14, DIV-0038), which a Japanese overlay carries in
     kana.
  2. **The layout switch.** The Latin layout patches (Config screen
     DIV-0015/0017/0026/0051, menu verbs DIV-0018, Yes / No DIV-0027) fit
     8-unit text and were keyed on "a language is set". `Lang_FullWidth()`
     now names the full-width languages, `ja` and `zh` (owner, 2026-09-24:
     the five official languages are a closed set, en/fr/de at 8 and zh/ja
     at 12). For those the three injectors leave the original layout, which
     was made for full-width text. Every other value, `original` and unset
     included, takes the same path as before.
- **Rationale:** the owner's request, 2026-09-24: Japanese as an option,
  and the layout split by language type.
- **Also in the PSX version?** The text and glyphs are the Japanese
  release's. The layout is the port's own, which suits them.
- **Verification:** 2026-09-24.
  - **English is unchanged:** a build is byte for byte the one before, in
    all 245 files.
  - **Japanese builds in full.** 200 of 200 areas with **0 slots kept**;
    44 pools with 42 slots kept; the six name tables with 72 of 538 kept
    (8-character names do not fit with a terminator, and the PC's own
    names never pass 12 bytes); enemy names 87 written and 361 kept (the
    banner draws 8 bytes, 4 full-width glyphs).
  - **Seen by capture, `BOF3X_LANG=ja`:**
    - the attract sequence's narration and dialogue (ギリー, モーグ, the
      ゴースト caption);
    - the world-map plates シーダの森 and マクニール村 with the region
      banner;
    - the item and ability lists (げんきだま, 純げんきだま, リリフ).

    「特能を使います」 was checked against the disc: the JP pool's own bytes
    `12 3a 12 88`, decoded the same way by the sibling's `jptext.py`.
  - **Not yet seen by the owner.**
- **Known gaps:**
  - The exe-side strings: menu buttons, list headers, default names, battle
    labels.
  - The 361 enemy names and 72 item names that do not fit.
  - The launcher's settings file, which knows only `en` and `original`.
  - The grow / shrink draw `0x4987E0` (unread), which Japanese shouts use.
- **Reversible?** play without `BOF3X_LANG`.

### Two kana in one glyph code: pair codes for Japanese names

- **ID:** DIV-0057
- **Date:** 2026-09-24
- **Subsystem:** text (only with a Japanese overlay; `tools/loc_build.py`,
  `src/game/text_pairs.cpp`, `Text_DrawString`, `Text_CharCount`,
  `Text_GlyphCount`, the two enemy name windows)
- **Original behaviour:** a glyph code draws one glyph. A Japanese name (at
  most 8 glyphs, `name[8]` on the JP disc) is two bytes a glyph on the PC.
  So the longest do not fit the items' 16-byte field with a terminator
  (72 of 538), nor the 8 bytes the battle banner copies (124 of 168 distinct
  enemy names).
- **New behaviour:**
  - **Pair codes.** A kind-13 chunk in `ja.FIRST.DAT` lists *pair codes*:
    glyph numbers from `0xD10`, each standing for two ordinary glyphs.
    `loc_build.py` pairs a Japanese item, ability or enemy name from its end
    until it fits (items 15 bytes, enemies 8), and nothing else. That takes
    226 pairs, and every name fits.
  - **The draw.** `Text_DrawString` draws a pair as its two glyphs, the
    second one advance on; the pair's own advance (kind 4) is the sum.
  - **Counting.** A pair counts as two characters there and in
    `Text_CharCount` / `Text_GlyphCount`, so every `count * 6` centring stays
    exact.
  - **The enemy name windows.** `BattleWin_DrawTargetEnemy` /
    `BattleWin_DrawEnemyStatus` draw through Capcom's 8-unit `0x516E70`, so
    they pass it the name with each pair replaced by its two codes
    (`TextPairs_Expand`).
  - **The placeholder.** The glyph at a pair code is a placeholder: both
    kana at native 12 px, which draw at half size. A path that does not
    expand pairs therefore shows small text, not a wrong word.
  - **No table loaded.** With no table loaded every function is what it
    was: the lookups answer "not a pair" and `TextPairs_Expand` returns its
    argument.
- **Rationale:** the owner's idea, 2026-09-24 ("two letters occupying a
  single 2 byte location"). It was preferred over one-byte kana (which would
  break byte-based widths and leave 3 enemy names over) and over widening
  records (which moves every later field).
- **Also in the PSX version?** No. The names are the JP release's and they
  look the same; storing two glyphs in one code is ours.
- **Verification:** 2026-09-24.
  - **Census** (docs/dialogue-localisation.md section 9): every item and
    ability name reaches `Text_DrawAt`, the text records or a banner copy,
    all ours.
  - **The missed path.** The census missed the enemy windows' `0x516E70`.
    The placeholder showed it on the first battle capture (ヌイグルミ as
    ヌイグ and a half-size ルミ), and the fix above was then checked on the
    same capture: ヌイグルミ even. The 18 callers of `0x516E70` were then
    read, and only those two carry names.
  - **Seen in game:** 8-glyph names ドラゴンシールド and フォースアーマー
    whole in the equipment panel. `BOF3X_TEXTLOG` showed pair codes drawn
    from the armour table.
  - **Self-tests:** `text_draw` 6,000 strings, `menu_windows` 74,000
    rounds, `battle_windows` 66,000 rounds, all 0 mismatches.
  - **English is unchanged:** 245 of 245 files.
- **Reversible?** play without `BOF3X_LANG`, or in any other language.

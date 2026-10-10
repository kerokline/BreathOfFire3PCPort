# Divergence ledger

**Status:** IN PROGRESS (opened 2026-09-18; 87 entries, DIV-0001..0087, DIV-0067 withdrawn)

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
- **Tier:** Forced | Intent | Sensible | Extension - see "Tiers" below; an
  optional " - <clause>" says why when the call is not obvious.
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

### Tiers

Every entry says which *kind* of change it is. The fields above record what
changed; the tier records how far from the original it is allowed to take the
game, which is what a player-facing preset ("as shipped", "fixed", "extended")
would be built from. Adapted from Severed Chains' retail-accuracy tiers
([`prior-art/severed-chains.md`](prior-art/severed-chains.md) §2.1), with one
added because we ledger engine-level changes and they do not.

| Tier | The original... | We... | Example |
|---|---|---|---|
| **Forced** | does something a reimplementation cannot reproduce: reads stale stack, jumps into another function's body | pick the nearest deterministic behaviour. Not a choice, so no preset can turn it off | DIV-0021, DIV-0024 |
| **Intent** | crashes, hangs, loses data, or does what it plainly did not mean to - a port bug, an original bug, or a platform the code no longer fits | make it do what it meant. Where the PlayStation shows what was meant, that is the target | DIV-0002, DIV-0004, DIV-0011 |
| **Sensible** | works as designed | change presentation, platform, language or comfort - the window, the look, the pace of play, the text - without changing what happens *in play* | DIV-0031, DIV-0048, the language overlays |
| **Extension** | works as designed | change what happens in play: rules, balance, content, cheats | DIV-0045, DIV-0046 |

There is no "accurate" tier because matching the original is not a divergence
and has no entry. Debug and development features (tracers, the A/B switch) are
tooling, not divergence, and are not ledgered at all.

When a change could be read as Intent or Sensible, write the clause. The test
is the rationale: Intent argues the original was *wrong*; Sensible argues it
was fine and something else is better. `tools/ledger_check.py` refuses an
entry with no tier or a tier not in this table.

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
- **Tier:** Intent - the logo video is broken on every current Windows
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
- **Tier:** Intent
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
- **Tier:** Intent
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
- **Tier:** Intent
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
- **Tier:** Sensible
- **Original behaviour:** `LoadDatFile` `0x454590` reads `DAT\<name>` and walks
  its chunks; that is all. There is one language, compiled in with the data
  ([`dialogue-localisation.md`](dialogue-localisation.md) §2).
- **New behaviour:** with the environment variable `BOF3X_LANG=<tag>` set, our
  `LoadDatFile` (`src/game/dat_load.cpp`) walks `DAT\<tag>.<name>` after
  `DAT\<name>` when that file exists, with the same chunk walker. The tag
  is the BCP 47 tag of the release the text came from (`fixtures.toml`'s
  `tag` per build: `en-US`, `en-150`, `fr-FR`, `de-DE`, `ja-JP`; since
  2026-10-08 - a bare code `en` before, and the overlays named `en.<name>`). An overlay
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
- **Tier:** Sensible
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
  `0x7DEE44`). Read 2026-10-03 and ours as `MsgBox_EffectDraw` (DIV-0070):
  it advances `12 + P` a character, space included, and does not take this
  table - an open question for the owner ([`msgbox.md`](msgbox.md) §3).
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Amended 2026-10-08 (DIV-0086):** `NameTables_Apply` accepts a run of
  records too (a kind-5 chunk whose tag is any record's name field and whose
  size is a whole number of records), for the `opt/` name layers; whole-table
  chunks behave as before.

### Tighten the apostrophe and the comma in the English font

- **ID:** DIV-0009
- **Date:** 2026-09-20
- **Subsystem:** text / localisation data
- **What this diverges from:** not the Chinese PC port, which has no English,
  but **the US PlayStation release**, which is where the English comes from
  and what "as it was" means for it (CLAUDE.md rule 6).
- **Tier:** Sensible
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
- **Tier:** Intent
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
- **Tier:** Intent
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **French and German, 2026-09-29 (the owner's choice):** their discs carry
  their own lettering in the same style - `NOUVEAU JEU` / `CHARGER JEU`,
  `NEUES SPIEL` / `SPIEL LADEN` - and no letters to spell CONFIG. So
  `fr.START.DAT` and `de.START.DAT` take the disc's two rows as they are,
  each row's width the ink's right edge plus 2 (the rule the US widths
  obey: 172 / 172, 156 / 162), and the third row from `en.START.DAT` beside
  them - the English overlay is built first; without it the page is left as
  shipped and the build says so. Seen by capture (`analysis/shots/title_fr`,
  `title_de`). Japanese needs nothing: its sheet is the US one.
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
- **Tier:** Sensible
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
- **Fix, 2026-09-27:** the owner pressed F9 twice in play and the game
  stopped with "config: controller pointer 0 holds 0x5E1DD364, expected
  0x0066A338" - returning to the title walks `FIRST.DAT`'s overlay again,
  and `ConfigText_Apply` checked the controller pointers and the label
  sites against the original's values a second time, when they already
  named ours. Now applied once, as the pause lines (DIV-0038) are; the
  text is the same on every walk. Checked the same evening on the
  `tools/recipes/field_f9.txt` route (the `adult_ryu` field, then idle) with
  the owner pressing F9 twice at the window: the log opens `FIRST.DAT` and
  `en.FIRST.DAT` twice (the load, then the title), one `DIV-0015` line, no
  Fatal; the owner: "it successfully returned to the title", and F9 on the
  title "gracefully closed the game with no error message".
- **Round twelve, 2026-09-29 (group FC1):** the row draw `0x461800` is ours
  now (`Config_DrawRowLabel`, [`field_c1.md`](field_c1.md) section 2). It reads
  the six label operands and the two anchor bytes from the original's code at
  every call, so this entry's patches hold for ours unchanged; no behaviour
  moved. Not yet seen in game through ours.
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
  large form is a different font and not a magnified one. **Round twelve,
  2026-09-29 (group FC1):** `0x461800` is ours (`Config_DrawRowLabel`); it
  reads the width code at `0x461894` (either form, anything else a Fatal) and
  the large call's target at `0x46189F` in place at every call, so this
  entry holds for ours unchanged ([`field_c1.md`](field_c1.md) section 2).

### The menu's short verbs - the buttons above a panel - in the overlay's language

- **ID:** DIV-0018
- **Date:** 2026-09-21
- **Subsystem:** menu (only with a language overlay)
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Forced
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
- **Tier:** Intent - a platform bug: the code assumes a tick count that fits a float
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
- **Tier:** Forced
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
- **Also, 2026-09-29 (round twelve group FE2, [`field_e2.md`](field_e2.md)
  section 6):** four more functions build SVECTORs on their stacks and never
  write the fourth word - the map-cell handlers `MapCell_DrawFrames`
  `0x570870`, `MapCell_DrawShaded` `0x570BC0` and `MapCell_DrawSpinning`
  `0x570DE0` (each quad's four vertices; the spinning one also its angles and
  its centre, handed to `Gte_RotMatrix` and `Gte_RotTrans`), and
  `Mode11_ObjectDraw` `0x536F10` (the marker's point for `Gte_RotTransPers`,
  and the disc's angles and translation). Ours writes `0000` in each, under
  the same ruling. The fuzz hashes each SVECTOR's first six bytes only, so
  the zero is by construction, not measured. `BOF3X_ORIGINAL=<name>`
  restores each.


### A field fade past its jump table stops instead of jumping

- **ID:** DIV-0024
- **Date:** 2026-09-22
- **Subsystem:** field objects (the kind handlers, [`object-kinds.md`](object-kinds.md))
- **Tier:** Forced
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
- **Tier:** Intent - the sprite handlers carry a texel inset the glyph handler never got
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

  **Amended 2026-09-27: the inset follows the scale.** 0.5 is half a
  texel, which is half a pixel only at scale 2, where the 24-texel glyph
  fills a 24-pixel quad. The owner plays at scale 4 (a 1704 x 960 window)
  and reported stroke bottoms "still crooked" - a `t` and an `l` a pixel
  off in their last rows, one instance of a letter and not the next. At
  scale 4 the quad is 48 pixels for 24 texels, so with a half-texel inset
  every odd pixel samples a texel *edge* again, and the quad's two
  triangles round it differently - the D17 mechanism, one level up.
  Measured on the game's own pre-look frame of the shop's dialogue at
  scale 4 (`tools/input_run.py tools/recipes/shop_ab.txt --env
  BOF3X_SCALE=4`): 174 of 1,813 bright runs began one pixel left of the
  4-pixel texel grid, all in glyphs' bottom rows. Now the inset is one
  screen pixel in texels, `1 / scale`, per axis from `D3d_ScaleX` /
  `D3d_ScaleY` at draw time (0.5 at scale 2, unchanged there), plus
  `1 / 256` of a texel so that an odd scale, whose pixel centres also land
  on edges (scale 3: pixel 1 at texel 1.0), picks the same side in both
  triangles. `glyph_draw.cpp` `TexelInset`; the fuzz's `SameWithInset`
  recomputes it from each round's scales. Verification below.
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
  2026-09-23**, English, point filter: "the text looks straight now" (at
  scale 2 or 3 then; the scale-4 recurrence is the amendment above).
  Amendment, 2026-09-27: the self-test's 20,000 inset rounds pass with
  the rule recomputing the inset from each round's random scales; and the
  shop dialogue's two boxes (`f01440`, `f01530` of the recipe) on the
  game's own pre-look frames, every bright run's left edge and width and
  every column run's top and height taken modulo the scale - before the
  fix at scale 4, 236 of 3,229 runs off the grid (174 horizontal, 62
  vertical); after it, 0 of 3,244 at scale 4, 0 of 2,433 at scale 3, 0 of
  1,622 at scale 2, and 0 of 4,055 at scale 5 (a scale-6 window did not
  fit the monitor and the game chose 5 - DIV-0036 - which made it the
  second odd-scale check). **Confirmed in game by the owner the same
  evening, at scale 4: "the lettering looks perfect so far".**
- **Reversible?** Yes: `BOF3X_ORIGINAL=GlyphTexelCentres` (our function,
  Capcom's arithmetic) or `BOF3X_ORIGINAL=D3d_DrawGlyph` (Capcom's function).

### The Config screen's controller panel: names at their own width, in the dialogue font, inside a wider frame

- **ID:** DIV-0026
- **Date:** 2026-09-22
- **Subsystem:** menu (Config, [`config-screen.md`](config-screen.md) §8)
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Amended again 2026-10-03, from the owner's `masterAndManillo.txt`:** the
  master's "Is this OK?" seen fixed in game (frames 3990, 4020, 4950, 4980,
  `analysis/shots/validate_1003/sheet_master.png`). Manillo's per-item
  "Is <item> OK?" (`ItemTrade_Confirm` `0x593E60`, `field_e2.cpp`) had been
  left as Capcom's - the hand on No over Yes (frames 2700, 3030) - and now has
  the same layout: its two answers drawn from `0xDE` less three spaces, three
  spaces more between them, the hand two units left of each
  (`g_trade_confirm_layout`, armed after the self-test under a Latin overlay;
  `sheet_manillo_item2.png`). A long item name was not measured against the
  hand's new first stop; the owner (2026-10-03): a name is twelve letters at
  most, they think, which would come close but should fit.
- **Reversible?** Yes: `BOF3X_ORIGINAL=YesNoLayout`. Only under a language
  overlay, not with `BOF3X_LANG=original`.
- **Amended 2026-10-03 (fix wave, group YN; [`yes-no-prompts.md`](yes-no-prompts.md)):**
  the owner's captures of 2026-09-30 and 2026-10-03 found the same fault on
  choosers `Menu_YesNo` does not reach: the hand a word short of `Yes`, and
  on `No` over `Yes`. The owner: "move the yes hand and yes word to the left
  to match the spacing on the load save screen". Each gets the load / save
  screen's offsets - three spaces moved from before `Yes` into the gap, the
  hand two units left of each word (its tip three before it), `No` where it
  was - with the stops measured from the line as the pen draws it
  (`YesNoLayout_Tail`, `YesNoLayout_ShopHandX`; DIV-0006's advances), so
  French and German answers of other lengths are met too:
  - **the master's "Is this OK?"** (areas 3, 37, 41, 50, 55, 59, 61, 68,
    74, 91, 98, 113, 116, 143): Capcom's `0x586D20` - its line call
    `0x586E78` (`Text_DrawAt`) and hand call `0x586E97` (`Menu_DrawHand`,
    `0xCF + 36 * answer`, 0 Yes) re-aimed (`BOF3X_ORIGINAL=MasterAskLayout`);
    English: `Yes` 243 -> 219, the hand 207 / 243 -> 217 / 273;
  - **Manillo's "Will that be all?"** (`ItemTrade_LeaveAsk` /
    `ItemTrade_LeaveWait`, ours, `effect_1g.cpp`'s `LeavePrompt`; the
    original's hand `0xE0 + 36 * answer`): `Yes` 240 -> 216, the hand
    224 / 260 -> 214 / 270 (`BOF3X_ORIGINAL=TradeLeaveLayout`);
  - **the shop's yes / no** - "Buy ...?" (help `0x49`), "Equip it?"
    (`0x4A`), "Sell ...?" (`0x52`) and "use the item?" (`0x36`,
    `SharedList_UseItem`): `ShopWin_TitleRun` draws system message `0xF`,
    Menu_YesNo's own line, over the help line, and `YesNoFrame`
    (`shop_states2.cpp`) / `SharedList_UseItem` (`field_s.cpp`) put the
    hand at window x `+ 0xE8 - 36 * answer` (1 Yes) - the original stops
    over the unmoved line. Now the line is `Respace`d as Menu_YesNo's and
    the hand stops over it (`BOF3X_ORIGINAL=ShopYesNoLayout`, all three
    sites);
  - **Manillo's "Want to buy anything?"** (message `0x44` of area 30's
    pool, `LeaderPanel_S4Again`, ours, `effect_1e.cpp`): the hand's row,
    not its x. The original puts it at `0xAA + 12 * answer`, the second row
    of the message - right for a one-row question; the English question
    wraps, so the hand pointed at its second row. The owner: "it just needs
    to be one row lower". Ours counts the message's rows (newlines less one
    is the first answer's row) and moves the hand by the difference - 12
    units for the English line, nothing for a one-row question
    (`BOF3X_ORIGINAL=ShopAskRow`). Its x (tip 3 units before `Yes`) was
    already aligned.
  Each only under a Latin overlay (DIV-0056), switched on after its module's
  self-test, which compares the original's draw. **Verification:**
  `BOF3X_SHADOW=yes_no_layout` re-spaces four prompt shapes (en, de, fr
  with a two-byte code) and checks the stops against the pen summed
  separately; `'*'` headless 0 mismatches; the captures are owed
  ([`yes-no-prompts.md`](yes-no-prompts.md) section 6 has the recipes and
  frames). Not touched, same fault likely: Manillo's per-item "Is ... OK?
  Yes No" (`ItemTrade_Confirm`, the hand `0xDC + 36 * answer` over "Yes No"
  at `0xDE`), and the help-line chooser that draws message `0xF`
  (`MenuList_WideTitleBox` `0x59A2E0`, ours since round fourteen's R2G, for
  help `0x1A` / `0x31`; the note once also named `0x59AE00`, which is
  `MenuList_GeneWinDraw` and draws no message `0xF` - corrected 2026-10-05
  from a read of both) - not marked by the owner.

### Music fades step once per logic frame

- **ID:** DIV-0028
- **Date:** 2026-09-22
- **Subsystem:** sound (`Sound_Tick` `0x587C70`, [`sound.md`](sound.md))
- **Tier:** Intent - restores the PlayStation fade the port lost
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
- **Tier:** Sensible
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
- **Since round twelve (2026-09-29):** the panel is ours
  (`Menu_DrawSaveSlot`, `src/game/field_o.cpp`, [`field_o.md`](field_o.md)
  section 2). It reads the disp8 at `0x576A48` back - `0x13` or this entry's
  `0x15`, anything else a Fatal - so the patch, and switching it off, work
  unchanged. Read with it: the texture window of the draw mode the panel
  sends is the `push 0` at `0x5769C8`; the `ebx` pushed at `0x5769C7` is a
  register save, not an argument.

### The menu backdrop past Config's four draws nothing

- **ID:** DIV-0030
- **Date:** 2026-09-23
- **Subsystem:** menus (`Menu_DrawBackdrop` `0x575690`, [`menu-windows.md`](menu-windows.md) §3)
- **Tier:** Forced
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Intent - replaying a stall is not what a frame loop is for
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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

  **Withdrawn 2026-09-27.** The owner, having played with both CRT looks:
  "we can remove the homegrown scanline option (newpixie works much
  better)". `src/render/crt.{h,cpp}` are deleted, `BOF3X_PRESENT=crt` is a
  Fatal in the dll ("clean or satpixie"), and the Look box holds three
  entries. An older `bof3x.ini`'s `screen=crt` is read as `satpixie`, so a
  player who had the look on keeps *a* CRT rather than none. The entry
  stays as the record of what was built (`git log -- src/render/crt.cpp`,
  [`crt-look.md`](crt-look.md) §1-4); DIV-0043 is the CRT look now.

### F9's pause lines in the overlay's language

- **ID:** DIV-0038
- **Date:** 2026-09-23 (per language 2026-09-25)
- **Subsystem:** text (`Pause_LinesGame` `0x66A418`, `Pause_LinesTitle` `0x66A448`; `src/game/pause_text.cpp`, `tools/loc_build.py` `PAUSE_LINES`, [`window-modes.md`](window-modes.md) §6)
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
  `src/game/map_layers.cpp`, `AreaMapBD_BuildView` `0x510780` - Capcom's when
  this was written and called `AreaMap_FrameAreaBD` below, which is the name of
  `0x510630`; ours since round fourteen's R3G, `src/game/rest_3g.cpp`, which
  reads the four cull operands back from the code;
  [`widescreen.md`](widescreen.md))
- **Tier:** Sensible
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
  `Widescreen_Inject` ran last in `inject_all.cpp` when written, after every
  fuzz, so the fuzzes compared the original culls - modules added since sit
  below it and fuzz against the widened bounds under `BOF3X_WIDE=1`, so a
  takeover of a `kSlides` site must read its bound from the operand, as
  `menu_lists`, `menu_draw_helpers` and `battle_e7` do: 2026-09-29, the
  three gene-list slide-outs had held the original bounds since `d1b411c`,
  found by `battle_e7`'s self-test failing in a build directory with
  `wide=1`; [`battle_e7.md`](battle_e7.md) §3). Live 2026-09-23: the field recipe
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
  **The loss screen's black, 2026-09-30** (the owner's shot: GAME OVER
  over a black middle, the battle field in both bands, the caption's bar
  sliding on into the right one): `BattleLoss_DrawBlack` `0x432930`
  (ours, `src/game/battle_e1.cpp`) draws its TILE at `(-53, 0)` 426 x
  240 by the fade tile's rule. The columns are read into a byte flag by
  `BattleE1_Inject` after its self-test, since that inject runs after
  `Widescreen_Inject` and the fuzz compares the original's 320; the bar
  and caption are as they were, now over black in the band. Off, bit for
  bit the original's. **Later that day, the rest of section 3c** (the
  owner's `cutsceneAndNue.txt`: the night's sepia and dark tint, the
  critical hit's flash, both 320 wide): every full-frame fill found by a
  scan of `.text` for `320.0f` beside `240.0f` (22 sites, 2026-09-30) that
  is ours draws through `Widescreen_Fill()` - the columns once
  `Widescreen_ArmFills` has run, which `InjectAll` calls after every
  module's self-test, so a fuzz on either side of `Widescreen_Inject`
  compares the original's (0, 0) 320 x 240 and the per-module flag above
  is gone. Widened: `EffectKind38_DrawTint` `0x473F10` (the tint),
  `EffectKind46_DrawFlash` `0x477D20` (the flash), `EffectKind11_DrawShade`
  `0x46A450` (the POLY_G4 shade), `FieldPanel_DrawShade` `0x52D5C0`,
  `Area145_DrawGradient` `0x421F10`, `Magic020_DrawFade` `0x4A3A70`,
  `IdentifyDim_Draw` `0x4B1770`, `Scena17_DrawFade` `0x56CBA0` (its tile;
  the frame copy's two sprites stay 320, being a copy of the 320 view),
  `BattleLoss_DrawBlack`, and three area sky gradients a grep of ours for
  `320.0f` found (`area_w3b.cpp`, `area_w3c.cpp`, `Area172_DrawShade` in
  `area_w4b.cpp`, their corners as `Area145_DrawGradient`'s). Seen in the
  recipe's captures (`analysis/shots/nue_before` / `nue_after`, frames 600
  and 1500): the night's shade now over the whole frame. **The sunset sky
  of area 23's cutscene** (frame 600, orange over black bands) was none of
  these: a detail call trace of the scene (`BOF3X_CALLTRACE_DETAIL`, the
  only `Gpu_SetPolyG4` builder in its frames) found Capcom's `0x4FD350`,
  a screen-wide gradient of a colour over black `(0, 0)..(319, 239)`
  whose left x is a zeroed register - now `Gfx_DrawSkyGradient`, ours in
  `src/game/area_backdrop.cpp` beside the area backdrop, its corners at
  `(-53, 0)..(372, 239)` by the same rule and fuzzed against its clone
  (20,000 rounds, 0 mismatches); the sunset now fills the frame
  (`analysis/shots/nue_after/f0600.png` before, the session's
  `shots_sky` after). The thin brightening left over the middle 320
  columns (188 against 203 in the bands' red) was the sunset's glow,
  Capcom's `0x4FD3E0` next door - a semi-transparent POLY_F4 of
  `(0, 0)..(320, 240)`, red the step word over eight - found by the same
  detail trace at frames 598..600 (the only POLY_F4 builder beside the map
  cells): now `Gfx_DrawSunsetGlow`, ours beside the gradient, widened the
  same way, 20,000 rounds at 0 mismatches; the sky reads the same colour
  at every column (`analysis/shots/nue_sunset/`). The owner, in game the
  same day: the sunset and the night shading correct.
  **The object culls, the same day** (the owner: trees popping in and out
  at the periphery under the wide view): four x culls of ours moved
  outward by the columns through `Widescreen_Fill()`, so each keeps beyond
  the 426 view the margin it had beyond the 320 - `MapCell_DrawUprights`
  `[-80, 400]` (the trees and other uprights; 27 px of margin were left),
  `MapCell_DrawAnimated` `[-60, 380]`, `MapCell_DrawQuads` `[-100, 420]`
  and `Sprite_Draw` `[-64, 384]` ([`widescreen.md`](widescreen.md) §3b's
  table). The y bounds are as they were. Their modules' shadows at 0
  mismatches (23 self-test lines); the fuzz compares the original bounds,
  `Widescreen_Fill` being 0 until every self-test has run. Not moved: the
  battle field's `MapCell_DrawTexQuads` and `MapCell_DrawGroundSprite`
  (`battle_e6.cpp`), which read Capcom's `.rdata` floats that his
  remaining code shares; `Encounter_OnScreen` `[-40, 360]`, which gates
  logic; and the unnamed `0x5054E3` `[-20, 340]`. Owed the owner's eye on
  the trees. Not yet: five sites in `.text` no symbol names
  (`0x489D47`, `0x48CB07`, `0x48CD10`, `0x48DC19`, `0x493308` in the magic
  engine's range; `0x507BDC`, `0x507CE3`, `0x50B4B5`, `0x50F7B5` after
  `Gfx_BeginFrame`), Capcom's still, and any fill built from integers or
  registers the scan cannot see - the owner's eye finds those.
  **Round thirteen's wave four, 2026-10-03**: the first five are ours
  now. E4D's three draw through `Widescreen_FillX` / `Widescreen_FillWidth`
  (`Effect_DrawScreenTint` `0x48CA90`, its blend variant `0x48CC90`,
  `EffectKind98_DrawFlash` `0x48DBA0`; `src/game/effect_4d.cpp`, fuzzed
  against the original's 320 x 240 with the fill unarmed); E4B's
  `0x489D47` and E4F's `0x493308` (`EffectKindAF_DrawScreen`) are ours and
  still 320 wide - to widen the same way (done at the round's end,
  below). `EffectKind96_Pulse`'s quad
  `(0, 0)..(320, 320)`, its 320s from a register, is not widened (no
  spawner found).
  **Wave five, the same day**: the next three are ours and widened the
  same way - E5E's `EffectKind18Sub3F_WhiteOut` (`0x507BDC`) and
  `EffectKind18Sub3F_DrawSky` (`0x507CE3`), E5G's
  `EffectKind18Sub36_Pulse` (`0x50B4B5`; left corners at
  `Widescreen_FillX()`, right at `320 + Widescreen_Fill()`), each fuzzed
  against the original's 320 x 240 with the fill unarmed. `0x50F7B5`
  (E6B's) is the last of the nine. Found by the wave and not widened:
  sub-kind 0x15's 320-wide strips and gradients (E5C), the spiral
  `0x505E60` centred on the constants 160 / 120 (E5D), the glow's cull
  `0x4FF6A3` at Capcom's `[-60, 380]` (E5B), `0x5054E3` as above.
  **Wave six, the same day**: the last of the nine, E6B's
  `EffectKind18Sub54_Pulse` (`0x50F7B5`), is ours and widened, and E6C
  widened one the scan had not listed - sub-kind 0x44's red additive
  full-frame tile (`0x510E6C`). So seven of the nine are widened and two
  (`0x489D47`, `0x493308`) are ours at 320 wide. Found and not widened:
  sub-kind 0x44's full-width draws `0x510F12` and `0x510FE5` (E6C), E6D's
  overlay quads at `0..320 x 0..256` and its 512-wide mist layers.
  **The round's end, the same day**: the last two are widened as
  `Effect_DrawScreenTint` is - E4B's `EffectKind89_DrawTint` (`0x489CD0`,
  the site `0x489D47`: the semi-transparent tint tile of kinds 0x89, 0x9F
  and 0xA4) and E4F's `EffectKindAF_DrawScreen` (`0x4932E0`, the site
  `0x493308`: kind 0xAF's red tile), each at `(Widescreen_FillX(), 0)`
  `Widescreen_FillWidth()` x 240 (`src/game/effect_4b.cpp`,
  `src/game/effect_4f.cpp`). Both groups inject before
  `Widescreen_ArmFills`, so their fuzzes still compare the original's
  `(0, 0)` 320 x 240 and narrow play is Capcom's to the bit. **All nine of
  the scan's sites and E6C's `0x510E6C` are now widened.** **Round fourteen,
  2026-10-04**: R3F's `EffectKindAA_DrawFill` (`0x492400`, the float at
  `0x492450`: kind 0xAA's two shaded quads over the whole frame) is drawn
  from `Widescreen_FillX()` over `Widescreen_FillWidth()`, as
  `EffectKind18Sub36_Pulse` is (`src/game/rest_3f.cpp`; its inject is before
  `Widescreen_ArmFills`, so its fuzz compares the original's 320). **Kind
  0xAC's two fades widen with it**: R3G's `EffectKindAC_FadeIn` and
  `_FadeOut` (`0x4925E0`, `0x492620`, `src/game/rest_3g.cpp`) draw through
  the same `EffectKindAA_DrawFill` (round fourteen's review, item 11). Not
  widened: `EffectKindA8_DrawBar` (`0x4920F0`), sixteen red bars the frame's
  width but not its height - **left as it is by the owner, 2026-10-04, to be
  looked at in game under the wide picture.** Where it shows is not
  established: no code of ours or Capcom's stores or pushes kind `0xA8` (a
  scan of the image for `mov byte [reg + 5], 0xA8` and for `push 0xA8`
  before a spawn found nothing), so the kind is asked for from data - a
  script or a table. The neighbouring kind `0xA7` is spawned by area 198
  (`Area198_SpawnEffectA7`; the sibling's `names/places.toml` gives that
  area as `AREA198` of `WORLD04`), and kind `0xB0` (E4F) draws the same
  bars: those two are where to look first. Still not
  widened, the owner's call: `EffectKind96_Pulse`, the spiral `0x505E60`,
  the culls (`0x4FF6A3`, `0x5054E3`, `Encounter_OnScreen`, the battle
  field's two), the strips and full-width draws listed above.
  **Manillo's backdrop, 2026-10-03** (the owner's catalogue,
  `manillo_will_that_be_all.png`: the trade screen's tiled fish pattern
  320 wide, black bands): `ItemTrade_DrawBackground` `0x5942C0` (ours
  since round thirteen's E1G, `src/game/effect_1g.cpp`), two POLY_FT4s
  `(0, 0)..(0xA0, 240)` and `(0xA0, 0)..(0x140, 240)`, u `0..0xA0`,
  textured through a 32 x 32 texture window that the port builds as the
  tile repeated over a 256 x 256 page (`Tex_Convert4` / `_8`). A
  pattern wants more tiles, not a stretch: under the columns the left
  quad runs from `0 - columns` with u from `(-columns) mod 32` (11 for
  53) and the right one to `320 + columns`, each u range grown by its
  columns (u up to 224), so every column shows the tile the original's
  phase puts there and the 426 columns are covered exactly
  (`ItemTrade_BackdropSpan`; a span past u 255 aborts). The columns are
  `Widescreen_Fill()`'s, 0 until every self-test has run, so the clone
  fuzz compares the original's quads; a property check in the same
  self-test proves the spans for 0..63 columns (edges, texels = columns,
  phase). Off, bit for bit the original's. `BOF3X_ORIGINAL=ItemTrade_DrawBackground`
  brings back the 320-wide pattern. [`widescreen.md`](widescreen.md) §5
  has the check owed and the nine sites above as the owner's routes
  reach them (none seen).
  **The view's cell inset, 2026-10-07** (the owner's bridge, 2026-10-06:
  stair-stepped black notches at the wide picture's left and right edges
  where terrain cells were missing, `analysis/shots/owner_reports/bridge_left_edge_unrendered_1006.webp`,
  `bridge_garr_wide_1006_a.png`; reproduced on the owner's `bridgeWalk`
  route). Not a cull: `MapView_Build` and `DrawLayer_Open` walk each row
  of the 28-column view ring from `MapView_Column + inset` for
  `(14 - inset) * 2` columns, the inset being columns trimmed off each
  side by the camera's angle and distance (`map_layers.cpp` `Inset()`,
  `MapView_Inset` `0x905D80`; two references in the image, both ours).
  The inset was sized for 320 columns of picture, and the 53 extra columns
  a side fell outside it - cells the terrain cull would have kept were
  never visited. Under `Widescreen_Live()` the inset is lowered by
  `Widescreen_InsetColumns()`, 3 columns a side (a ring column is two map
  cells across, about 20 screen px at the field camera's usual distance;
  `BOF3X_WIDE_INSET=0..14` moves it), not below 0; the fuzz and every
  narrow run see 0. Verified on the `bridgeWalk` route wide, a capture
  every 60 frames: the sea reaches both edges on every frame where it
  stepped before (`inset3_660.png` in the session-`a8d0ee80` scratchpad,
  `vis/`); no `no draw item` line in the log (the pool, DIV-0062, has
  room).
  **The sea bridge's sky, 2026-10-07** (the owner's screenshots of area 41:
  the sky black for the band's 53 columns on one side as the camera pans,
  and a sliver of stretched cloud growing in the other band). The sky there
  is `EffectKind18Sub15_Draw` `0x502E60` (ours, `src/game/effect_5c.cpp`;
  `effect_5c.md` section 1 had marked it "not a DIV-0041 site"): a haze
  band 0..320 x 56..88, three 256-wide cloud strips scrolled by
  `Frame_Counter / 8` and clipped to 0..320, and four gradient quads over
  the columns 0, 60, 160, 260, 320, all linked into layer 15's third list.
  A strip starting past 320 was drawn from its start back to 320 - off the
  original's picture, inside our band - which was the sliver. Under
  `Widescreen_Fill()`: the haze and the outer gradients run from -53 to 373
  (two more gradient quads, flat at the table's outer rows), the strips are
  clipped to those bounds with the original's u rule (u from the clipped-off
  width on the left, to the visible width less one on the right), a fourth
  strip at phase - 512 covers the left band at every phase, and a strip
  starting past the right bound is skipped. Off, every packet is the
  original's bit for bit (the fuzz compares them). The owner on the
  capture: "Sky looks perfect". **The bands' pool guard, 2026-10-10** (a code
  review): the two band quads are written at the four gradient
  quads' cursor, 0x110 past it, which runs on whether or not
  `EffectKind18Sub15_LinkLayer` refused a link; 0x198 is more than the
  0x54 of slack under the pool limit, so near a full pool they would have
  been written past this buffer's 64 KB. Both are now skipped - nothing
  written, nothing linked, a `draworder` line under `BOF3X_DRAWORDER` -
  unless they end under the limit `LinkLayer` tests. Narrow, nothing
  changes (the bands are never drawn).
- **Reversible?** Unset `BOF3X_WIDE` (the default). `BOF3X_ORIGINAL=Widescreen`
  keeps the frame pass's original ranges under a wide picture;
  `BOF3X_ORIGINAL=MapView_Build` the terrain cull's;
  `BOF3X_ORIGINAL=AreaMap_DrawBackdrop` the 320-wide sky;
  `BOF3X_WIDE_INSET=0` the original's cell inset under a wide picture.

### The window resizes freely; the picture snaps to whole multiples or fills the height

- **ID:** DIV-0042
- **Date:** 2026-09-23
- **Subsystem:** platform (`Game_WndProc` and `Game_WinMain` `src/game/win_main.cpp`,
  `Display_Setup` `src/game/display_setup.cpp`, `Fmv_Play` `src/game/fmv_play.cpp`,
  the backend `src/render/render_d3d11.cpp`; the launcher)
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Intent - the needle the PlayStation draws
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
- **Tier:** Extension
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
- **Amended 2026-10-03 (fix wave, group CH): the cap is 10, and two boss
  hooks are scaled too.** (1) The owner, 2026-10-02: 50 is humorously large
  for this game - the multipliers stop at 10 (launcher sliders 0..10, the
  ini comment, `Multiplier` in `src/game/cheats.cpp`). A number above 10, as
  an ini saved under the old cap holds (the owner's had `cheat.zenny=50`),
  is **clamped to 10 and logged** (`DIV-0045    BOF3X_ZENNY=50 is above the
  cap, 10 used`), not a `Fatal`: the owner is not locked out by their own
  file; the Cheats dialog shows and saves 10. Not a number, or negative, is
  still a `Fatal` (`must be 0..10`). (2) The owner, 2026-10-02: Balio and
  Sunder's second fight ignores `cheat.exp=0`. Its EXP is not
  `Battle_EnemyDefeated`'s: the fight's end hook `Boss16_End` `0x43A190`
  (ours since round eleven, `src/game/boss_sc.cpp`) **stores** enemy 0's
  plus enemy 1's `+0x96` into `0x904AEC` on a win (`mov [0x904AEC], eax` at
  `0x43A1B3`), replacing whatever the kills had added. An operand scan of
  `.text` for `0x904AEC` / `0x904AF0` finds one more writer outside the
  battle set-up and the result screen: `BossWeretigr_EndMove` `0x43D5A0`
  (`src/game/boss_sa.cpp`, `add` at `0x43D616`), Weretigr's end walk. Both
  now multiply the EXP they write by `Cheats_ExpMultiplier()`; nothing else
  writes the zenny total, so zenny needed nothing. Same feature, same kind
  of change, so an amendment and not a new entry. Verified headless: with
  the variables unset the `boss_sc`, `boss_sa` and `battle_flow` fuzz pass
  (0 mismatches); with `BOF3X_EXP=0` `boss_sc` mismatches in `Boss16_End`
  alone (3,033 of 24,000 rounds) and `boss_sa` in `BossWeretigr_EndMove`
  alone (2,007), the first differing byte `0x904AEC` - the multiplier
  reaches those two stores and nothing else. Owed: the live run in
  [`cheats.md`](cheats.md) §5 (the second fight's EXP at 0, and the first
  fight's, measured).

### Pilfer and Steal take the item whenever the enemy has one

- **ID:** DIV-0046
- **Date:** 2026-09-24
- **Subsystem:** battle (the Pilfer and Steal state steps `0x4B54F0` and
  `0x4F5140`, Capcom's; two `PatchBytes` in `src/game/cheats.cpp`; the
  launcher's "Cheats..." dialog; [`cheats.md`](cheats.md) §3)
- **Tier:** Extension
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
  variable set and unset. Steal's step `0x4F5140` was still Capcom's and
  still patched at the time; **since round nine (2026-09-25, group SH) it is
  ours** as `SkillSteal_Roll` (`magic_steal.cpp`), and ours reads the mask
  back from the patched original's byte (`Cheats_StealRollMask`), so the
  cheat behaves the same with either side in place.
- **Reversible?** Yes: the switch off, `BOF3X_STEAL` unset, or
  `BOF3X_ORIGINAL=Cheat_StealAlways`.

### The frame period is the PlayStation's 29.97, and the deadline is a double

- **ID:** DIV-0047
- **Date:** 2026-09-24
- **Subsystem:** platform (`Game_WinMain` `0x4FCB00`'s loop,
  `src/game/win_main.cpp`; [`known-defects.md`](known-defects.md) D5,
  [`IDEAS.md`](IDEAS.md) I16)
- **Tier:** Intent - the PlayStation's pace; the port's 33.334 ms was an approximation of it
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
- **Tier:** Sensible - logic is unchanged at either speed
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
- **Tooling, 2026-10-03 (no behaviour of play changes):** `BOF3X_SPEED=n`
  (1..64) starts a run at that speed, for scripted runs nobody watches
  (`input_run.py --speed N`). Two things keep a recipe recorded at speed 1 in
  step: a frame a recipe `shot` is about to save is drawn even when the loop
  is late, and while a stream started by `Sound_LoadStream` plays the loop
  runs at the ordinary period - audio plays in wall time and scenes wait on
  it (the inn counts 150 frames, then waits for its jingle;
  `src/hook/run_speed.h`). Measured the same day: `shop.txt` with a shot every
  30 frames, 105 shots pixel-identical and the `randlog` identical at x8
  against x1 (180 s to 97 s); `balioAndSunder_2.txt`, 21,835 frames, `randlog`
  identical (746 s to 114 s). A route with another wall-clock wait would show
  as a `randlog` or shot difference against its x1 run: check each new route
  once. `attract_run.py` does not take it (`attract_watch.py` counts frames
  by polling and would undercount). **The stream hold is `BOF3X_SPEED`'s
  alone** (2026-10-05, round thirteen's review item 1): it was gated on the
  speed itself, which F1 also sets, so a player's x2 was held at x1 for up
  to two minutes whenever a track started. Now a flag only `BOF3X_SPEED`
  sets and F1 clears (`g_speed_scripted`, `win_main.cpp`).
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
- **Tier:** Sensible
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
     at 12) - by the tag's primary subtag since 2026-10-08 (`ja-JP`, `zh-CN`). For those the three injectors leave the original layout, which
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
  - The grow / shrink draw `0x4987E0` (ours since 2026-10-03,
    `MsgBox_EffectDraw`, DIV-0070; no pair handling), which Japanese shouts use.
- **Reversible?** play without `BOF3X_LANG`.

### Two kana in one glyph code: pair codes for Japanese names

- **ID:** DIV-0057
- **Date:** 2026-09-24
- **Subsystem:** text (only with a Japanese overlay; `tools/loc_build.py`,
  `src/game/text_pairs.cpp`, `Text_DrawString`, `Text_CharCount`,
  `Text_GlyphCount`, the two enemy name windows)
- **Tier:** Sensible
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

### The field menu's screen title centred on its real width

- **ID:** DIV-0058
- **Date:** 2026-09-27
- **Subsystem:** menu (only with a Latin language overlay; `MenuList_TitleBox`
  `0x599FA0`, `src/game/menu_lists.cpp`; [`menu_lists.md`](menu_lists.md) §3)
- **Tier:** Sensible
- **Original behaviour:** the top bar's screen title (Items, Ability, ...,
  the system text of `FieldMenu_TitleIds` `0x6672E4`) is drawn in a box
  `0x48` wide with its text starting at x + `0x25` - 6 n, n the string's
  `Text_CharCount`: centred on the box's middle for the 12-unit glyphs it
  was written for. Under an English overlay the glyphs advance 8 (DIV-0006)
  and the title sits left of centre - the owner's note of 2026-09-21
  ([`menu-screens.md`](menu-screens.md) §3 item 3), read as
  [`known-defects.md`](known-defects.md) D86, and shown again by the owner
  2026-09-27 ("Ability", "Tactics" offset to the left in their box).
- **New behaviour:** with `BOF3X_LANG` set to a Latin language the text
  starts at x + `0x25` - width / 2, the width being what the pen will cover
  (the sum of DIV-0006's advances) - the same number as before for 12-unit
  glyphs, so Chinese and the full-width languages (DIV-0056) are placed
  exactly as the original places them. Every read and call of the original
  is kept; only the x differs. `g_title_centre`, patched to 1 at inject under
  the name `MenuTitleCentre`.
- **Rationale:** DIV-0018's for the button verbs, one row up: a caller
  that centres by counting characters at 12 px is wrong for any narrower
  font, and the fix is to centre on the real width.
- **Also in the PSX version?** The US disc's own title draw centres its own
  font; not checked how.
- **Verification:** the menu_lists shadow self-test (the fuzz runs before the
  patch and compares the original's arithmetic byte for byte); the
  `menu_screens` recipe under `--lang en` with and without
  `BOF3X_ORIGINAL=MenuTitleCentre` - both run 2026-09-27: the self-test's
  20,000 rounds over 20 functions, 0 mismatches; and a new recipe,
  `tools/recipes/menu_titles.txt` (the top bar itself, a shot on each of
  its six slots - the older recipe's shots are inside the screens, where
  the description line stands in for the title), at scale 4, English. The
  title moved right by 6 n - width / 2 game units on every slot - 10 for
  "Items" (n 5, 40 wide), 14 for "Ability" (n 7, 56 wide) - and sits on the
  box's middle in the side-by-side. **Confirmed by the owner the same
  evening: "that looks good".**
- **Reversible?** play without `BOF3X_LANG`; `BOF3X_ORIGINAL=MenuTitleCentre`
  keeps the overlay and the original's x.

### The item and skill lists' title centred on its real width

- **ID:** DIV-0059
- **Date:** 2026-09-27
- **Subsystem:** menu and battle (only with a Latin language overlay;
  `BattleMenu_DrawItemList` `0x59CD00`, `BattleMenu_DrawSkillList`
  `0x59D200`, `src/game/battle_draw.cpp`)
- **Tier:** Sensible
- **Original behaviour:** both list windows draw a title in the `0x99`-wide
  box at their top - the item list its category (`0x66B58C`, 物品 / 武器 /
  ...), the skill list its kind (`0x66B5A0`, 攻击 / ...) or, while word
  `+0x14` is set, the pointer `0x66B5B0`: in battle the name of the skill
  being targeted. The text starts at x + 6 (13 - n), n the character count
  (the item list's from `Text_CharCount`, the skill list's `strlen`): centred
  on x + 78 for 12-unit glyphs. Under an English overlay the skill name is
  8-unit text and sits left of centre - the owner's capture 2026-09-27,
  "Examine" while it targets. The Chinese headers themselves are still
  Chinese under every overlay (HANDOFF, staged).
- **New behaviour:** with `BOF3X_LANG` a Latin language the text starts at
  x + 78 - width / 2, the width being what the pen will cover (DIV-0006's
  advances); the same number as before for 12-unit glyphs, so the Chinese
  headers and the full-width languages are placed as the original places
  them - **except the skill list's own Chinese header**, which the original
  counts in *bytes* (`strlen`, 4 for 攻击) and so places 12 units left of
  where the item list's (`Text_CharCount`, 2) lands: 13 units left of its
  box's middle, visible in the owner's Ability-screen capture the same day.
  Under a Latin overlay ours puts it on the middle; without one the
  original's placement stands. `g_list_title_centre`, patched to 1 at inject
  under the name `BattleListTitleCentre`. DIV-0058's fix, one box over.
  **Extended 2026-09-29** (DIV-0064's headers made it visible) to every
  list that draws such a title: the field menu's item list
  (`Menu_DrawItemList` `0x5759C0`, `src/game/menu_windows.cpp`), its
  ability and item panels (`Menu_DrawAbilityPanel` `0x575F50`,
  `Menu_DrawItemPanel` `0x5763F0`, `src/game/field_o.cpp`, whose own
  switch is set after their fuzz, which runs later than the patch) and the
  two title draws still Capcom's (the `Text_DrawAt` calls at `0x596D13`
  and `0x59DEFA`, re-aimed under the same name at `ListTitle_DrawAt`) -
  all through `ListTitle_X` (`src/game/list_title.h`). **Extended
  2026-10-07** to the camp's master list (`MasterWin_DrawList` `0x59C2C0`,
  `src/game/rest_2h.cpp`), whose title DIV-0064 made `MSTR` the same day:
  its box is `0x71` wide and the original centres on `x + 0x3A` (`6` a
  character back), so it asks `ListTitle_Centring` and centres on its own
  middle; armed after the group's fuzz, as `field_o`'s lists are.
- **Rationale:** as DIV-0018 and DIV-0058.
- **Also in the PSX version?** The US disc's own draw centres its own font;
  not checked how.
- **Verification:** the battle_draw shadow self-test (the fuzz runs before
  the patch): 102,000 rounds over 6 functions, 0 mismatches. Live: the
  `combat_ab` recipe with and without `BOF3X_ORIGINAL=BattleListTitleCentre`
  - three of 43 frames differ, all within the skill list's title box
  (x 190..225, y 71..82), the header 12 units right; the owner's "Examine"
  case owed their eye.
- **Reversible?** play without `BOF3X_LANG`; `BOF3X_ORIGINAL=BattleListTitleCentre`
  keeps the overlay and the original's x.

### The battle result's EXP line in the US layout

- **ID:** DIV-0060
- **Date:** 2026-09-27
- **Subsystem:** battle (only with a Latin language overlay;
  `BattleResultWin_DrawExp` `0x5985A0`, `src/game/battle_result.cpp`;
  [`battle_result.md`](battle_result.md))
- **Tier:** Sensible
- **Original behaviour:** each party line of the EXP window is the name at
  x `0x19`, the next level (`"%2d"`, the 12-unit font) at `0x85`, the EXP
  still needed (`"%6d"`) at `0xE3` and then system message `0x15` at
  `0x55` - drawn last, over the numbers' column. The Chinese message is
  short and stops before `0x85`; the English overlay's "EXP to next level:"
  is 144 units and runs over the level number, which shows through the
  sentence as one stray glyph (the owner's capture, 2026-09-27: a digit
  hidden in "next").
- **New behaviour:** with `BOF3X_LANG` a Latin language, the US release's
  own order and positions: the EXP right-aligned at `0x36`, the sentence at
  `0x82`, the next level at `0x114` (and the level-99 message at `0x82`).
  Same calls, same order, other x; nothing changes without an overlay or
  under a full-width one. `g_exp_layout_us`, patched to 1 at inject under
  the name `BattleResultExpLayout`.
- **Rationale:** the owner photographed the US screen the same evening:
  "[EXP] EXP to next level: [level]" - `10 EXP to next level: 2`. The three
  x positions are read off that photograph against the box's 280-unit
  width, to within a unit; the US EXE was not read.
- **Also in the PSX version?** This *is* the US version's layout, by eye.
- **Verification:** the battle_result shadow self-test (the fuzz runs before
  the patch); `tools/recipes/combat_exp.txt` (the combat route with shots
  after the win) with and without `BOF3X_ORIGINAL=BattleResultExpLayout`:
  17,000 rounds over 17 functions, 0 mismatches; the route's own shots fire
  before the window opens (its presses do not reach it - owed), so the
  owner pressed through both runs at the window and captured them,
  2026-09-27: the "off" run the Chinese layout with the stray digit, the
  "on" run `Ryu  7 EXP to next level: 2` / `Rei  45 EXP to next level: 6`,
  the US photograph's arrangement. The owner's capture is the record.
- **Reversible?** play without `BOF3X_LANG`; `BOF3X_ORIGINAL=BattleResultExpLayout`
  keeps the overlay and the original's x.

### The battle's action banners centred on their real width

- **ID:** DIV-0061
- **Date:** 2026-09-27
- **Subsystem:** battle (only with a Latin language overlay;
  `BattleWin_BannerFrame` `0x597230`, `src/game/battle_win_states.cpp`)
- **Tier:** Sensible
- **Original behaviour:** the banner at the top of the battle - the actor's
  name while it chooses, the action's name while it acts ("Flare", "Heal",
  "Green Apple", "Attack") - is a record of the banner pool `0x93B8E0`
  drawn by window kind 3: a wide banner (+2 set) is the medium box at
  (x - 0x10, y) with its text at x + 0x26 - 6 n, a narrow one the small box
  at (x, y) with its text at x + 6 (6 - n), n the `Text_GlyphCount`:
  centred for 12-unit glyphs. Under an English overlay the 8-unit names sit
  left of centre - the owner's "Green Apple" in the combat route,
  2026-09-27.
- **New behaviour:** with `BOF3X_LANG` a Latin language the text starts at
  the same middle (x + 0x26 wide, x + 0x24 narrow) less half the width the
  pen will cover (DIV-0006's advances); the same number as before for
  12-unit glyphs, so Chinese and the full-width languages are placed as the
  original places them. Every read and call kept; `g_banner_centre`,
  patched to 1 at inject under the name `BattleBannerCentre`. DIV-0058 and
  DIV-0059's fix, in the battle.
- **Rationale:** as DIV-0018.
- **Also in the PSX version?** The US disc's own draw centres its own font;
  not checked how.
- **Verification:** the battle_win_states shadow self-test (the fuzz runs
  before the patch): 57,000 rounds over 19 functions, 0 mismatches. The
  `combat_ab` recipe with and without `BOF3X_ORIGINAL=BattleBannerCentre`:
  22 of 43 frames differ, every one within the banner band (y 46..57);
  "Green Apple", "Pilfer" and "Teepo EX" on the box's middle in the
  side-by-side. The owner's eye owed.
- **Reversible?** play without `BOF3X_LANG`; `BOF3X_ORIGINAL=BattleBannerCentre`
  keeps the overlay and the original's x.

### The draw-item pool doubled: cells the wide view keeps ran it dry

- **ID:** DIV-0062
- **Date:** 2026-09-27
- **Subsystem:** display / field (the map view's draw-item pool,
  `src/game/draw_pool.{h,cpp}` and its readers; [`widescreen.md`](widescreen.md)
  §3b, [`map-layers.md`](map-layers.md) §1)
- **Tier:** Sensible
- **Original behaviour:** every cell of the map view the terrain cull keeps
  takes a draw item (one, or three with its side triangles) from a pool of
  1,024 items at `0x905E80` (index 0 never handed out) through a 1,024-word
  free queue at `0x7E09E0`, `DrawItemPool_Top` its head. When the queue is
  empty `DrawItemPool_Alloc` returns 0 and `MapView_Build` leaves the cell
  undrawn that frame; nothing says so. Sized for the port's own cull,
  `[-50, 370]`.
- **New behaviour:** 2,048 items and a 2,048-word queue. The index is 12
  bits in the cell word and 16 in the item, so nothing changes shape; the
  thirteen places in Capcom's remaining code that name the item array as an
  immediate (`0x486EBD`, `0x5098F4`, `0x5098FB`, `0x509909`, `0x509910`,
  `0x509930`, `0x5109FC`, `0x51242A`, `0x512607`, `0x5139C2`, `0x513B9B`,
  `0x513BAC`, `0x513C41` - a raw scan of `.text` for the array's address and
  its two interior offsets, then for every immediate inside item 0, each
  confirmed by disassembly) are re-aimed at inject, the one bound
  `AreaMapBD_BuildView` (`0x510780`; called `AreaMap_FrameAreaBD` here until
  2026-10-05, which is the name of `0x510630` - ours since round fourteen's
  R3G) compares its bump index with (`0x400` at `0x510878`)
  is raised to `0x800`, and ours read the pool through `draw_pool::Items()`
  / `Free()` / `Count()` - including the two resets that prime every item's
  halves (`Field_ViewReset`, `Weretiger_ResetMapView`), which the original
  walks 1,024 items deep by address; a half never primed drew as garbage.
  **Where the array lives:** the items are linked into the ordering table
  by 24-bit addresses (the PlayStation's tag; `d3d_list.cpp` masks a link to
  24 bits), so it must sit below 16 MB, as all of Capcom's data does - the
  first build put it in the dll and crashed in `Gfx_DrawOTag`. By the time
  the dll runs, the child's low memory is cut up by heaps and mapped files
  (the largest gap measured, even at `DllMain`, was 128 KB), so the
  **launcher** takes the 288 KB in the suspended child at the first free of
  `0xF00000`, `0xE00000`, ... `0xA00000` and stamps it (`ReserveDrawPoolIn`);
  the dll finds the stamp at the same candidates (`DrawPool_Reserve`, first
  in `InjectAll`), or without the launcher scans for a free region itself,
  and with neither keeps the original's pool and says so. The switch
  (`DrawPool_Grow`) runs last, after every module's start-up fuzz, so each
  compared the original's arrays. Always on, not only under the wide view:
  the narrow view already ran the same scene at 855 of 1,023.
- **Rationale:** the owner's coast capture, 2026-09-27: blue parallelograms
  along the river bank after the Yraall cutscene, "renders slowly while the
  game pans, and then some spots stay glitched", widescreen only. The
  owner's recorded route (`tools/recipes/textureglitch.txt`, the save before
  the scene) reproduced it in every wide run and no narrow one. Measured:
  the page-texture cache never filled (0 full pages, 27 builds in the
  route); the pool did - `DrawItemPool_Top` wrapped to 0 at frame 523 and
  sat at the ceiling through the pan, 200 cells a second refused an item
  and left undrawn, their 128-high walls (`MapCell_DrawWalls`) showing
  through as the blue faces; a cell refused every frame stays missing. The
  wide cull `[-150, 470]` keeps about half as many cells again as the
  original's. Shrinking the margin to the original's 50 would still put the
  scene near 1,070; the pool had to grow.
- **Also in the PSX version?** No - the PlayStation drew its cells straight
  from the ordering table; the pool is the port's.
- **Verification:** the self-tests of every reader (draw_pool, map_layers,
  map_scroll, field_misc, world_map, draw_pass, area_w1a, magic_s14) pass
  as before, and the whole sweep after. The owner's route wide: 36 of 36
  shots, 0 cells refused an item (was 200 a second from frame 523), no
  crash, and every shot's shared 320 columns identical to the narrow run's
  (before: 7 shots differed, the pan and the stuck patch). The relocated
  array capped at 1,024 reproduced the old 7 exactly, which is what
  separated the priming fault from the relocation. The owner's eye owed.
- **Reversible?** `BOF3X_ORIGINAL=DrawPool` leaves the arrays and the
  fourteen patches alone. So does `BOF3X_ORIGINAL` naming any of the twelve
  owned functions whose original names the arrays (the pool's three, the two
  resets, `MapView_Build`, `MapView_CellTextures`, `MapView_ItemHalfAt`,
  `AreaMap_ApplyPatch`, `MapCell_FlatOverlay`, `Sprite_DrawPass`,
  `Area40_DrawGrid`; `DrawPool_Grow`'s list): the patch sites cover only code
  we do not own, and one original body on the old arrays beside ours on the
  new would hand one item to two cells. Logged, 2026-09-29 (the capture
  review of rounds 10..12).

### The party's dragon form with a partner missing: no form, where the original read its stack

- **ID:** DIV-0063
- **Date:** 2026-09-29
- **Subsystem:** battle (the transformation, `DragonForm_PartyRecipe`
  `0x4523C0`, `src/game/battle_e6.cpp`; [`battle_e6.md`](battle_e6.md)
  sections 2 and 7, L1)
- **Tier:** Forced
- **Original behaviour:** with gene `0x10` among the chosen genes and the
  party's size byte `0x904AB1` at 3, the recipe search tries the party's
  form before recipe 6: `DragonForm_PartyRecipe` collects the `+0x89` byte
  of each member who is not the actor and not out by `Battle_ActorIsOut`
  into a list on its stack, and tries the first two as a pair against five
  pairings. One pairing of the five answers 0 for two pairs of partners
  (`+0x89` 2 with 4, and 2 with 5, either way round; `battle_e6.cpp`,
  `case 2`), which the search reads as "no party form": recipe 6 is skipped and the search goes on. When fewer
  than two such members are found the list is one byte or none, and the
  function dispatches on and compares stack bytes it never wrote: what it
  answers depends on what its callers left there.
- **New behaviour:** fewer than two found answers 0, the answer of the
  two pairs that fail. Every case with two or more found is unchanged.
- **Rationale:** a reimplementation cannot reproduce the read, and an abort
  there (what the takeover first did) would end a battle an ordinary party
  can reach: three members, one of the other two down, the gene chosen.
  **Which answer** is the owner's account of the game, 2026-09-29: the
  gene used with one partner standing, or with either of two pairs of
  partners, fails and gives the default dragon - one outcome for both. The
  code has that outcome for the two pairs as the answer 0, so the missing
  partner takes it too. `0xFF` (no pairing matched) was the other
  candidate and is a different path: it ends the search at once, where 0
  still tries recipes 7 to 10. That gene `0x10` is the one the owner means
  is the owner's reading of the shape (the form depends on who the other
  two members are); nothing in the binary names it.
- **Also in the PSX version?** Not measured. The owner's memory of the
  outcome is of the game as played; the sibling has no name in this
  function and its twin was not read.
- **Verification:** `battle_e6`'s self-test has the case as seven rows run
  on ours alone (one partner out in each position, both out, a failing
  pairing, no pairing), since there is nothing of Capcom's to compare with;
  the fuzz of the function against the original's clone is as it was and
  never seeds the case. **Owed: the owner's check in game**, once a save
  has the gene and a full party - a partner down, and each failing pair.
- **Reversible?** `BOF3X_ORIGINAL=DragonForm_PartyRecipe` leaves Capcom's
  function, its read with it.

### The short labels from a language overlay: status words, stats, item and skill types

- **ID:** DIV-0064
- **Date:** 2026-09-29
- **Subsystem:** menu and battle text (`src/game/labels.cpp`, chunk kind 15
  of `tools/loc_build.py`; the 8 px draw `Text_DrawSmall` `0x516E70` in
  `src/game/mode_states.cpp`; [`dialogue-localisation.md`](dialogue-localisation.md) §8)
- **Tier:** Sensible
- **Original behaviour:** five groups of NUL-padded slots in `BOF3.exe`'s
  `.data` hold Chinese labels that every overlay left as shipped: the
  status words 中毒 / 昏乱 (2 x 8 at `0x66A0E8`, drawn after a member's
  level by the 8 px draw), the menu's stats 攻击 防御 智力 速度 (4 x 8 at
  `0x66A0F8`, the Equip column and the shops' member panel), the item
  types 物品 武器 防具 选项 重要道具 (`0x66A120`, four of 8 and one of 12,
  behind the pointer table `0x663970` and its copies - the item lists'
  titles), the skill types 治疗 辅助 攻击 技能 龙技 (5 x 8 at `0x66A200`
  behind `0x663984`, the fifth behind `0x66B5B0` alone - the skill lists'
  titles) and the battle's stats (4 x 8 at `0x669CF0` behind `0x64AE08`).
  The US disc has each group beside bytes the PC still has (`START.EMI`:
  `Pois Conf` and `Pwr Def Int Agl` between the bytes at `0x663648` and
  `0x663660`, `ITEM WEAPON ARMOR OPTION VITAL` after the sixteen at
  `0x663960`; `BATTLE.EMI`: `HEAL ASSIST ATTACK SKILL DRAGON` and the
  four stats before the sixteen at `0x66B5B4`); the French and German
  discs the same tables, found by the same bytes.
- **New behaviour:** `loc_build.py` writes the disc's strings as kind-15
  chunks (tag = the group), one byte a letter as the verbs are (DIV-0018).
  `Labels_Apply` writes the status words and both stat groups into their
  slots after checking the push operand or pointer that names each; the
  item and skill types, which nothing reaches but their pointer tables (a
  scan of the image for each slot's address), go into 16-byte buffers of
  the DLL's and the eight tables are re-aimed at them after each entry is
  checked - so the French `ARMEMENT` and `CAPACITE` and the German
  `RÜSTUNG`, eight letters against slots of 8 bytes, fit (the owner's
  question, 2026-09-29: the box has the room, the slot had not; a pair
  code would not have helped, since two one-byte letters paired are still
  two bytes). A string over its room is sent empty and the slot stays as
  shipped. The status words alone go through the 8 px draw,
  which samples a whole glyph into an 8-unit quad: `Text_DrawSmall` asks
  `Labels_SmallGlyph` for a one-byte character inside those two slots and
  draws the overlay's 8 x 8 cell of the letter (glyph `0xA00 + code -
  0x30`, DIV-0015's set) once a chunk has written them - two bytes a
  letter, as the Config screen's text has, would be 9 bytes in a slot of
  8. Everything else draws through `Text_DrawAt` unchanged; the titles'
  centring is DIV-0059's, extended today to every list that draws one.
- **Rationale:** as DIV-0018: the exe's own strings are the last Chinese
  under an English overlay; the owner's captures of 2026-09-27 (HANDOFF
  item 6).
- **Also in the PSX version?** These are the US disc's own strings in the
  US disc's own slots; the PC's slots are wider.
- **Verification:** `BOF3X_SHADOW='*'` headless with `BOF3X_LANG=original`
  (the fuzz runs before any overlay loads): 0 mismatches. Live, English:
  `tools/recipes/menu_screens.txt` -> `analysis/shots/labels_en` - `ITEM`
  and `HEAL` on their boxes' middles, `Pwr Def Int Agl` in the Equip
  column; `battle_commands.txt` -> `analysis/shots/labels_cmd` - the
  whelp's list titled `DRAGON`; `combat_ab.txt` -> `labels_combat`, 43
  frames, nothing amiss. French, `menu_screens.txt` -> `labels_fr`:
  `OBJET`, `GUERIR`, and the log's `5 of 5` for both repointed groups.
  **2026-09-30:** the overlay's second load - the title's `FIRST.DAT`
  after a game over (the owner's `tools/recipes/gameover.txt`, its run
  ending in `FATAL: item types: 0x00663970 holds ...`) - aborted in
  `Labels_Apply`: a repointed group's witness is its first pointer table's
  entry, which the first load had re-aimed at our buffer. The slot check
  now accepts that address as the table check already did; the strings
  are written into the same buffers again and the tables re-aimed at
  what they already hold.
  **The owner, 2026-09-30:** `Pois` after Teepo's level in the Items
  screen's member panel, English - the status word through the 8 px draw
  confirmed (their capture). **Owed the owner's eye:** `Conf`, the
  battle's stats, the `WEAPON`..`VITAL` and `ASSIST`..`SKILL` titles, the
  German build and the French weapon and skill pages.
  **Extended 2026-10-07 - a sixth group, the camp's master list** (the
  owner's report of 2026-10-06 night: the list's title still 师匠 under the
  English overlay, and a cross beside every completed master where the US
  screen draws a star, their web capture
  `analysis/shots/owner_reports/master_list_web_reference_1006.png`). Two
  slots, drawn by `MasterWin_DrawList` `0x59C2C0` (ours, `rest_2h.cpp`):
  the title, 8 bytes at `0x66A1F0` (`push` at `0x59C5B5`), and the mark
  beside a completed master, 4 bytes at `0x66A2D8` (`push` at `0x59C464`).
  **The cross was the overlay's own doing:** the mark is the one byte `t`,
  and the shipped font's single-byte slot for `t` (glyph `0x4E`) holds a
  star - rendered from `FIRST.DAT` with `font_pc.py`'s `glyph_pixels`,
  2026-10-07 - so the 2001 port drew a star as the PlayStation does, and
  DIV-0006's repaint of the 75 single-byte slots with the US letters turned
  it into a lowercase t (the same fault DIV-0051 fixed for the controller
  panel's button icons). Both strings are on the US disc: `SHOP.EMI` has
  `MSTR` and then the one code `0x84` - the dialogue set's filled star,
  the cell rendered from each disc's atlas - as two NUL-ended strings
  padded to four bytes between the 24 bytes the PC has at `0x66B3B8` and
  the masters' requirement lists it has byte for byte at `0x66B3D0`; the
  German disc the same, the French `ME` and the same star. `loc_build.py`
  finds them by those two anchors (`LABEL_MASTER_HEAD` / `_LISTS`) and
  writes the group as tag 6; `Labels_Apply` writes both slots in place
  after checking the two push operands. The star goes in as the two-byte
  code of the appended cell (`0x9E7`), so the mark no longer depends on
  which glyph a single-byte slot holds. The title is centred on its real
  width under a Latin overlay (DIV-0059, extended to this list the same
  day: its centre is `x + 0x3A`, not the `0x99` box's `x + 78`). A
  Japanese overlay leaves both as shipped: it repaints no letter slots, so
  the port's star stands there. **The portrait box's label 弟子 at
  `0x66A1F8` (`MasterWin_DrawPupils`) is still as shipped:** `SHOP.EMI` has
  no string beside `MSTR` for it, and the owner's capture does not show
  that box; what the US screen draws there is still the question to the
  owner ([`yes-no-prompts.md`](yes-no-prompts.md) section 5).
  *Verification:* `loc_build.py all` on the US and French discs reports
  `master list 2`; the `rest_2h`, `battle_draw` and `field_o` self-tests
  0 mismatches (the centring is armed after the fuzz, as `field_o`'s is);
  the owner's `campingFishing.txt` with a shot every 30 frames around 2400
  (`analysis/shots/master_labels/masters.png`): `MSTR` on the box's middle,
  Mygas with the unfinished dot as before. **Owed the owner's eye:** the
  star itself, on a save with a completed master (no committed route has
  one), and the French `ME`.
  **Extended 2026-10-07 again - five more groups, from the owner's
  `tools/recipes/sortScreens.txt`** (their route through the item and
  ability sort menus, the formation screen and the camp's Skill Notes;
  `BOF3X_TEXTLOG=1` named every string drawn from `.data`, and the one
  the log could not see was found by its bytes):
  7. **the sort menus** - eleven slots from `0x66A170` of 8 or 12 bytes
     (整理 / 自己整理 / 通常道具 / 战斗道具, the equipment sort's three,
     AP大 / AP小, 通常技能 / 战斗技能) behind the pointer table `0x66B12C`.
     `START.EMI` has `SORT`, `ManualSort`, `NormalItem`, `CombatItem`,
     `Power`, `Defence`, `Kind`, `High AP`, `Low AP`, `NormalAbil`,
     `CombatAbil` right after the 28 bytes the PC has at `0x66B110`, the
     US disc packed in the same rooms, the French and German padded to
     four and placed by their eleven pointers (`loc_build.py`'s
     `label_run`). Nothing but the two tables (`0x66B12C`, and `0x66B374`
     for the AP pair) reaches the slots, so the group is repointed into
     16-byte buffers of the DLL's as the item and skill types are: the
     French `PC élevé` and `Défense` (an accent is two bytes) and the
     German `AP niedr` are over their 8-byte slots.
  8. **the camp's Skill Notes sort** - its title 选单 `0x66A1DC` and first
     choice 察看技能 `0x66A1E4` behind `0x66B36C` (the other two are
     group 7's AP pair again): `SHOP.EMI`'s `SORT` and `LOOK` after the
     same 28 bytes.
  9. **the Skill Ink count's label** 墨水 `0x66A118` (`push` at `0x585964`,
     `SharedList_DrawItemCount`), which section 8 of
     [`dialogue-localisation.md`](dialogue-localisation.md) had down as
     "not found on the US disc as text": `SHOP.EMI`'s `Ink` (`Encre`,
     `Tinte`) after the `SKILL` slot the PC has at `0x664290` and its
     three pointers.
  10. **the formation names** - ten records of 28 at `0x6636B0` (a name of
     16, then the three s16 pairs of the icon wheel), read by
     `Menu_DrawIconWheel` `0x573F70` through the base at `0x573FC6` and
     drawn by the 8 px draw, so `Labels_SmallGlyph` now serves their
     slots as it serves the status words'. `START.EMI` has the ten as
     records of 20 (US: a name of 7 and its length) or 22 (German: 8, the
     length, a pad) with the same pairs, found by the pairs at either
     stride: `Normal` x3, `Attack` x2, `Defense` x2, `Chain`, `Magic`,
     `Refuge` - all ten PC records match the disc's pair for pair. Their x
     is DIV-0084's.
  11. **the zenny unit** - `0x66A31C`, 4 bytes, the one byte `s`: the
     shipped font's `s` slot is the port's coin glyph, and the overlay's
     repaint made it a letter (`11957s` on the owner's Items screen) - the
     master list's star again. Four pushes (`Menu_DrawMoneyBox`'s
     `0x57465C` is the witness; `Commu_DrawZennyBox`, the enemy target
     panel's three and the battle result's zenny window share the slot).
     `START.EMI` has the US code `0x60`, the dialogue set's stylised Z, in
     the slot right after the icon wheel's triangle (24 bytes the PC has
     at `0x6637C8`), before the full stop and the verbs' pointers; the US
     money box itself (`0x801dc9xx`, found by its `%7d`) draws no unit,
     the shop's zenny box does.
  *Verification:* `loc_build.py all` on the US, French and German discs
  reports `sort menus 11, formations 10, zenny unit 1, note sort 2, ink
  label 1`; the `field_o`, `field_s` and `menu_windows` self-tests 0
  mismatches; the owner's route replayed with a shot every 30 frames
  (`analysis/shots/sortScreens2`): frame 420 `SORT` / `ManualSort` /
  `NormalItem` / `CombatItem`, 720 `SORT` / `ManualSort` / `High AP` /
  `Low AP`, 1440 `Normal` / `Attack` / `Defense` centred in their boxes,
  1980 `Ink  2`, 330 `11957` with the Z. **The owner, the same morning:**
  the armour and weapon screens' sorts (`Power` / `Defence` / `Kind`) look
  correct too. **Owed the owner's eye:** the French and German words in
  play.
- **Reversible?** play without `BOF3X_LANG`; the chunk is the overlay's.
  Not by a `BOF3X_ORIGINAL` name: the slots are data.

### The battle equip window's stat labels beside their own values

- **ID:** DIV-0065
- **Date:** 2026-09-30
- **Subsystem:** battle menu, the Equip window (`BattleEquipWin_Draw`
  `0x59D640`, ours in `src/game/battle_e7.cpp`, kind 3 of
  `Window_Handler8Kinds`)
- **Tier:** Sensible
- **Original behaviour:** the window lists a member's four stats with a
  label each. The values go at `y + 0x1C + 13 k` (the 8 px font, read at
  `0x59D722`), the labels at `y + 0x27 + 13 k` (`0x59D6B6`, `lea ecx,
  [esi + 0x27]`, `Text_DrawAt` with `0x66A0F8..`): each label sits two
  pixels above the *next* stat's value, the first value has no label, and
  the fourth label is drawn under the frame's bottom edge, off the box.
  Capcom's own constant, not a slip of ours - the run of
  `tools/recipes/gameover.txt` under `BOF3X_ORIGINAL='*'` (Chinese, since
  `'*'` leaves the overlay off; frames 540 and 720) shows 29 alone, 攻击
  beside 16, 防御 beside 22, 智力 beside 18 and 速度 clipped, the owner's
  English capture the same with `Pwr Def Int`. The field's member panel
  (`0x5738A0`: the label at `y + 8`, the value at `y + 0xA`) puts a label
  two pixels above its own value; `0x27` is that relation one 13-pixel
  row down (`0x1A + 0xD`).
- **New behaviour:** the labels at `y + 0x1A + 13 k`, beside their own
  values, once a byte flag is on. `BattleE7_Inject` sets it (`PatchBytes
  BattleEquipLabelsRow`) after the module's self-test, whose fuzz compares
  the original's rows; every other pixel of the window is as it was.
- **Rationale:** the owner, 2026-09-30, from the equip window in a fight:
  "str shows up next to defense, def next to intelligence". A label
  belongs to its own value; the field's panel shows where Capcom put it.
- **Also in the PSX version?** Not read: `0x59D640` has no PSX twin in
  the pairs, and the sibling has no name for the battle equip window.
- **Verification:** `BOF3X_SHADOW='*'` headless: 0 mismatches (the flag is
  set after the fuzz). Live: the recipe above on ours, English, frames 540
  and 720 - `Pwr Def Int Agl` each beside its value, the fourth inside the
  box. The owner, in game the same day: correct.
- **Reversible?** `BOF3X_ORIGINAL=BattleEquipWin_Draw` leaves Capcom's
  function and its rows.

### A pad press skips an FMV as a key does

- **ID:** DIV-0066
- **Date:** 2026-09-30
- **Subsystem:** platform (`Fmv_Play` `0x59E360`, ours since DIV-0035,
  `src/game/fmv_play.cpp`; the pad through `src/game/pad_read.cpp`,
  DIV-0050)
- **Tier:** Sensible
- **Original behaviour:** while a video plays, `Fmv_Play` pumps messages
  with a blocking `GetMessage` until `Fmv_WndProc` `0x59E570` clears
  `Fmv_Playing`: on `WM_KEYDOWN`, `WM_LBUTTONDOWN`, `WM_RBUTTONDOWN`, the
  MCI notify at the video's end, or `WM_DESTROY`. A pad reaches the game
  only through `Pad_Read` from WinMain's latch, which the pump never
  calls, so a pad press does nothing to a video - the original's own
  joystick included.
- **New behaviour:** the pump drains the queue with `PeekMessage` and
  dispatches as before, then polls the pad (`PadRead_AnyInputDown`: any
  input down, bound or not, after `PadSdl_Poll`; it starts SDL's pad
  itself, since the two intro videos play before `DInput_Init` would -
  the owner's first try skipped nothing for that reason) and waits up to 16 ms
  for the next message (`MsgWaitForMultipleObjects`). An input going down
  during the video clears `Fmv_Playing` as a key does; one held from
  before the video is ignored until it is released. A `WM_QUIT` taken off
  the queue is posted again for WinMain's loop, which the original's
  `GetMessage` returning 0 left there. The key, click and end-of-video
  paths are `Fmv_WndProc`'s, unchanged; the log says `DIV-0066 NAME
  skipped by the pad`.
- **Rationale:** the owner, 2026-09-30: "can we have controller button
  presses skip the intro fmvs like keyboard strokes do?"
- **Also in the PSX version?** The PlayStation's movies are skipped by the
  pad (its only input); the PC port's keyboard-only skip is the port's.
- **Verification:** builds; `pad_read` shadow 0 differ (the player is not
  fuzzed - it runs MCI). The owner, 2026-09-30, on the intro videos: "it
  works now" (after the pad's start moved ahead of them). Not yet tried:
  a pad input held from before the video (no skip until released).
- **Reversible?** `BOF3X_ORIGINAL=Fmv_Play` runs Capcom's player.

### Withdrawn: the rising squares' random numbers from a generator of their own

- **ID:** DIV-0067 (withdrawn and removed 2026-09-30, the day it was made;
  the number is not reused)
- **Date:** 2026-09-30
- **Subsystem:** display (`MapCell_DrawRising` `0x570660`; the switch and
  its code are gone)
- **Tier:** Sensible - withdrawn: the premise was wrong, see below
- **Original behaviour:** the rising squares draw their random numbers
  from the shared `Rand` `0x5B93D2`, as every caller does.
- **New behaviour:** none. The entry is kept so the number and the story
  are not lost; nothing in the code differs from the original on this
  account.
- **Rationale:** the fields below ("what it was", "why it is gone") are the
  record; the required fields above are here so the ledger's checker reads
  the entry as one (2026-10-01).
- **Also in the PSX version?** Moot: no divergence stands.
- **Reversible?** Nothing to reverse; the switch, its launcher key and its
  code were removed with the withdrawal.
- **What it was:** an opt-in switch (`BOF3X_DRAW_RAND=1`, the launcher's
  `draw_rand`) giving `MapCell_DrawRising` `0x570660` a private generator in
  place of Capcom's `Rand`, on the belief that draw code consumed the
  shared sequence once per *rendered* frame and so made a fishing recipe
  replay differently.
- **Why it is gone:** the belief was wrong. `Game_WinMain`'s loop runs every
  game function on every logic frame; a late frame skips only
  `Gfx_DrawOTag`. The fish differed because this session's ad-hoc shot
  copies added a frame per shot (`tools/recipe_shots.py` is the tool; a
  shot line is a frame of the route) and, in the owner's first two
  recordings, a walk diverged at a world-map ledge. Measured after the
  removal: `tools/recipes/caughFish.txt`, recorded with the switch on,
  replays without it - the `Rand` count identical on all 3,889 frames
  (`randlog`), the fish caught - so the squares never ran on that route.
- **What stays, none of it a divergence:** the finding that `Rand`
  `0x5B93D2` is the MSVC6 CRT `rand()` and the binary holds no `srand`, so
  the sequence is fixed from boot; and the per-frame `randlog` line in
  recorded and scripted runs (`src/hook/input_script.cpp`), a counting
  replacement over a byte-copy of `rand` that changes no value.

### Kind 0x64's glow: the rim vertices' depth the centre's

- **ID:** DIV-0068
- **Date:** 2026-10-03
- **Subsystem:** effects (`EffectKind64_DrawGlow` `0x481740`, ours in
  `src/game/effect_3a.cpp`; effect kind 0x64, chapter 10's run 2)
- **Tier:** Forced
- **Original behaviour:** the glow is a fan of 32 semi-transparent
  `POLY_G3`, the centre the projected point, the two rim vertices points on
  a circle round it. Each vertex is three dwords (x, y, depth) and the
  renderer reads the depth (`0x5A0E80` divides by it, `+0x10` / `+0x20` /
  `+0x30`). The centre's depth is the projection's; the rim's x and y are
  computed into two stack locals at `esp + 0x20` / `+ 0x24` of the frame,
  but their depth is read from `esp + 0x28` (`0x481836`, `0x48189D`), a dword
  the function never writes: whatever the stack held there.
- **New behaviour:** ours writes the centre's depth to the rim vertices
  (`+0x20`, `+0x30`). Every other byte of every primitive is the original's.
- **Rationale:** a stack word the function never writes cannot be
  reproduced, only replaced (DIV-0023's class). The centre's depth is what
  the two sibling discs of the same kinds write to all three vertices
  (`EffectKind64_DrawSpark` `0x4820C0`, `EffectKind68_DrawMote` `0x481CC0`,
  read 2026-10-03): a flat disc at the point's depth, which a zero (a divide
  by zero in the renderer) would not be.
- **Also in the PSX version?** Not read: `0x481740` has no PSX twin in the
  pairs.
- **Verification:** `BOF3X_SHADOW=effect_3a` headless: the fuzz compares
  every byte of the packet but those two dwords of the glow's triangles
  (its `Gfx_CommitPrim` stand-in copies `+0x10` over them on both sides
  while `0x481740` runs), 0 mismatches; a control planting a wrong centre
  depth is refused ([`effect_3a.md`](effect_3a.md) section 5). Not seen
  live: no recorded route reaches kind 0x64.
- **Reversible?** `BOF3X_ORIGINAL=EffectKind64_DrawGlow` runs Capcom's
  function, its stale depth included.
- **The owner's word, 2026-10-03:** kept as written; the owner will say if
  the glow looks wrong in game.

### The fishing minigame's text in the overlay's language, laid out for Latin letters

- **ID:** DIV-0069
- **Date:** 2026-10-03
- **Subsystem:** field text, the fishing spot (only with a language overlay;
  `src/game/fishing_text.cpp`, chunk kind 16 of `tools/loc_build.py`; the
  draws effect kind 0xF's lines in `src/game/effect_1a.cpp`, its tabs and
  name lists in `src/game/effect_1b.cpp`;
  [`fishing-text.md`](fishing-text.md))
- **Tier:** Sensible
- **Original behaviour:** the fishing spot's banner - the lines effect
  kind 0xF types right to left across the top window ("set rod and lure",
  "quit fishing", the cast's, the lost catch's) - is thirteen strings of
  the port's glyph codes at `0x669FC0..0x66A06D`, reached only through the
  8-byte records at `0x653B98` (a pointer, a label byte, a pause byte); the
  three tabs over the equip menu are three 8-byte slots at `0x66A070`
  behind the pointer table `0x66A088`. Every overlay left both Chinese
  (the owner's `fishing_banner.png`, `fishing_equip_menu.webp`, the
  camping route's frames 3120..4800). The layout is the 12-unit glyph's:
  a character is typed every 6 frames as the line moves 2 units a frame
  (`0x466310`, `+9` = 6; the first by `0x4662B0`), the flip cursor is 12
  wide at `2 * left + 0x119`, a line's button label goes 12 units a
  character after the line's start (`0x466460`, `0x4665E0`), the leaving
  character moves the line on by 12 (`0x27` = `0x1B + 12`); the tabs are
  drawn at `x + 0xA / 0x35 / 0x6A` with counts 2, 3, 2 (`0x468AC0`); an
  accessory's name is drawn to 8 characters (`0x468C50`, `0x468F00`,
  `0x469210`, `0x465230`). The label itself is a single-byte glyph,
  `0x66A2FC[line]` - the port's circled numerals 1, 2, 3, which are its
  stand-ins for circle, cross, triangle (the same table and colours 2, 1,
  6 the Config screen's `0x461C00` uses, DIV-0051) - and an overlay
  paints those single-byte slots with letters: the owner saw `b`, `c`,
  `d` after the lines, and `aa` where the Chinese text has its two-dot
  ellipsis (glyph `0x3B`).
- **New behaviour:** `loc_build.py` finds the fishing module every fishing
  area carries on the disc (the US `AREA030.EMI` section at `0x801D0C00`)
  by the row table the PC still has byte for byte (`0x653C04`, 36 bytes):
  the disc's thirteen line records end twelve bytes before it - 12 bytes
  each, a count added, the label and pause bytes the PC's - and its tab
  labels follow the edge-quad records the PC has at `0x653E6C`, at the
  fixed width the module's own code hands the draw (`addiu $a3, $zero, n`
  before the `lui` / `addiu` of their address: US and German 4, French 7).
  Chunk kind 16, tag 1 the lines, tag 2 the tabs; `FishingText_Apply`
  copies each into a buffer of ours and re-aims the record's pointer (or
  the table entry) after checking it names the shipped string or our
  buffer (a second load of `FIRST.DAT`). US: the tabs `Gear`, `Data`,
  `Rule`; the lines are the module's own (not copied here), its `>>>`
  the dialogue font's ellipsis where the port has its two dots. And,
  armed in `InjectAll` after every self-test
  under a Latin overlay only (`Lang_Latin`): each character typed after
  half its own advance in frames, so it lands where the flip cursor ends
  (the US module's own constant is 4 for its 8 units, `0x801D3A28`,
  `0x801D3AEC`), the flip cursor as wide as that advance with its right
  edge at `0x125` as before, the leaving character moving the line on by
  its advance (kept even, so the line still meets `0x1D`); the label
  after the line's real pen width, drawn as DIV-0051's PlayStation icon
  for the button the port's numeral names (circle, cross, triangle, in
  the line's own colour); the tabs whole and centred in their 0x28-wide
  boxes by their real width (the US module's `x + 6 + 0x30 i` for four
  letters); an accessory's name to 12 characters, the US field - `Wooden
  Rod`, `Heavy Ca...` no longer cut at 8. For 12-unit glyphs every one of
  these computes the original's number.
- **Rationale:** the stage-2 text swap (DIV-0005) for the last Chinese on
  the fishing screens, and the layout the swap needs, as DIV-0018 and
  DIV-0059 re-centred theirs: English typed at the 12-unit cadence lands
  four units further left of the flip cursor with each character, and its
  label floats half the line's width past its end. The label's icon follows the
  port's buttons, not the US disc's: the US module's labels are `x`,
  triangle, square (codes `0x81..0x83`, colours 1, 6, 5), the US release's
  button layout; the PC's numerals name circle, cross, triangle, which is
  what the port's input answers to, and the Config screen shows them the
  same way under an overlay (DIV-0051).
- **Also in the PSX version?** The strings are the PlayStation's own, the
  layout constants its 8-unit ones; the icons are the port's buttons drawn
  in the PlayStation's shapes.
- **Verification:** `BOF3X_SHADOW=effect_1a` and `effect_1b` headless (the
  fuzz runs before the layout is armed): 210,000 and 288,000 rounds, 0
  mismatches; armed during the fuzz instead (a control), 10,281 and 23,399
  mismatches - `LineNext`, `LineScroll`, `LineFade`, `ShowName`,
  `DrawToggles`, `DrawItemsB`, `DrawItemsA`, `DrawEquipped` - which is to
  say the fuzz sees the change and the 12-unit cadence is unchanged
  (`LineStart`, `LineType` equal under 12-unit advances, by construction).
  `BOF3X_SHADOW='*'` headless, narrow: exit 0, 7,687 ours, 0 mismatches.
  `loc_build.py`'s converter run on the US, French and German discs: 13
  lines and 3 tabs each. **Not seen in game** (a headless wave): the
  coordinator's live check is in [`fishing-text.md`](fishing-text.md)
  section 6. Owed the owner's eye: the banner's cadence and labels, the
  tabs, the full names; the French tabs (`Equip`, `Données`, seven letters
  over a 40-unit box - the French disc widened its boxes) and the German
  build.
- **Found by the live check, 2026-10-03:** the banner's one-byte draw
  (`EffectKind0F_DrawGlyph`) makes glyph `0x20 - 0x26` of a space, far past the
  font - the disc's English lines have spaces, Capcom's have none - and both
  fishing routes crashed in `Font_UnpackGlyph` at the first banner. Under the
  Latin layout a space now draws nothing (the callers move the pen). And
  `tools/dat.py` did not know chunk kind 16, so `loc_build.py all` stopped
  after its first file. After both: `caughFish.txt` and `campingFishing.txt`
  to `done`, no crash, the banners, tabs and names in English
  (`analysis/shots/validate_1003/camping_b`, `caughFish_b`).
- **The owner's word, 2026-10-03:** the tab words are the English disc's
  (Gear / Data / Rule). The banner's button icons stay the port's (circle, cross,
  triangle) although the US disc shows cross, triangle, square - the owner reads
  that as the US release's different default bindings - until the banner can draw
  the icons of the bindings in force ([`IDEAS.md`](IDEAS.md) I29). The French
  tab that overflows its box waits on a picture of the French game.
- **Reversible?** play without `BOF3X_LANG` (or `BOF3X_LANG=original`): no
  chunk, and the layout is never armed. `BOF3X_ORIGINAL` on any of the
  draws named above leaves Capcom's, which reads the re-aimed pointers
  with the original's layout.

### A space in a growing shout draws nothing

- **ID:** DIV-0070
- **Date:** 2026-10-03
- **Subsystem:** text (`MsgBox_EffectDraw` `0x4987E0`, ours in
  `src/game/msgbox.cpp`; `MsgBox_Step`'s draw under flag 8 of `0x7DEE44`,
  the grow / shrink effects 2 and 3)
- **Tier:** Intent - the original crashes, and its own branch shows a space
  was meant to have nothing to texture
- **Original behaviour:** for a character inside a grow span the draw
  writes the CLUT word, then `cmp cl, 0x20 / je 0x4988BE` at `0x498819`:
  a space skips the glyph word `+0x16` and all eight texture bytes, yet
  `0x4988BE` onward writes the shade and the corners and commits the
  primitive (`Gpu_SetCode6C`, `Gpu_SetSemiTrans`, `Gfx_CommitPrim`). The
  slot's glyph word and texture are whatever the last primitive there left
  (disassembly 2026-10-03, [`msgbox.md`](msgbox.md) §9). Capcom's Chinese
  script never puts a space in a grow span (63 grow presets over the
  shipped `AREA*.DAT`, none after a span with a `0x20`); the English
  overlay does - 15 of its 65, in areas 11, 40, 41 and 99 - and on
  2026-10-02 the owner's game crashed at one (area 99, message `0x24`:
  nine characters, four of them `0x20`):
  `build/bof3x.crash-30104-0.dmp`, `Font_UnpackGlyph` reading `0x17053EA0`
  for the stale word `0xC254`, the dump's four space slots holding stale
  words.
- **New behaviour:** a space writes the CLUT word and moves the pen by `P`
  (`0x7DEE68`) exactly as the original does, and builds and commits no
  primitive: the gap is the same width with nothing in it. Every other
  character is the original's to the byte. A byte flag
  (`g_effect_space_skips`, `PatchBytes MsgBoxEffectSpaceSkips`) turns it on
  after the `msgbox` self-test, whose fuzz compares Capcom's spaces.
- **Rationale:** the owner's crash. A primitive with an unwritten glyph
  word draws an arbitrary glyph at best and faults the renderer at worst;
  the original's own skip says a space has no texture.
- **Also in the PSX version?** Yes, the same shape: the twin `0x80151F4C`
  takes its word separator `0xFF` from `0x80152010` to `0x80152BF8`,
  skipping the tpage word and the `u`, `v` bytes and still writing the
  corners and committing (sibling `disasm_exe.py`, SLPS). The port
  carried the slip over with `0x20` for `0xFF`. Whether a JP shout puts an
  `0xFF` in a grow span was not measured.
- **Verification:** `BOF3X_SHADOW=msgbox` 44,000 rounds, 0 mismatches,
  1,000 of them this function against a byte-copy with the packet slot
  random and compared (354 spaces, each committed by the copy); with the
  flag on, the same inputs: glyphs identical, spaces equal to the copy's
  state with the primitive taken out, 0 mismatches. 35 controls planted,
  35 refused, including the fix stuck on (refused in all 354 space
  rounds). Not run live in this wave: [`msgbox.md`](msgbox.md) §9 has the
  route and frames (`balioAndSunder_2.txt`, frames 11,476..11,544).
- **Not changed:** the advance. A grow span advances `12 + P` a
  character, space included; DIV-0006's table is not consulted (the
  owner's call, [`msgbox.md`](msgbox.md) §3).
- **Reversible?** `BOF3X_ORIGINAL=MsgBox_EffectDraw` runs Capcom's draw,
  stale quad and all.

### Walkable floor does not cover a sprite's feet

- **ID:** DIV-0071
- **Date:** 2026-10-03
- **Subsystem:** field and world-map draw order (`Sprite_DrawPass`
  `0x593060`, ours in `src/game/draw_pass.cpp`; the rule in
  `src/game/layering.cpp`)
- **Tier:** Intent - the owner's decision, 2026-10-03, beyond the original
- **Status:** built, **on by default since 2026-10-03** (the owner's word,
  with the branch merged into round thirteen's; `BOF3X_LAYERING=0` is the
  original's order); the owner has seen captures, not yet played with it.
  Reference and A/B runs pin `BOF3X_LAYERING=0` (`attract_run.py`, the
  `validate_*.sh` scripts): the rule moves sprites' places in the draw
  list, so an ours side with it on differs from Capcom's by design
- **Original behaviour:** a painter's order by diagonal row. Every cell of
  a nearer row is emitted after a sprite of the row behind, flat floor
  included, and there is no depth test, so the floor of the next one to
  three rows is drawn over whatever of the sprite reaches past its own row
  on screen: the shadow's lower corners, sometimes a foot
  (`known-defects.md` D199; measured with `BOF3X_DRAWORDER`,
  [`sprite-draw-order.md`](sprite-draw-order.md) §18.9). The same order
  is what puts a forest, a roof or a wall of the next row in front of the
  party, which is wanted.
- **New behaviour:** after the pass's first sort, each sprite of the draw
  list is given the layer it is drawn in: up to three layers later than its
  own, one layer at a time, for as long as everything it would newly be
  drawn over that reaches the box round its feet (28 x 14 px round the
  screen point `+0x74` / `+0x78`) is *floor* - a cell's own quad, its
  lowest corner no more than 2 units above the feet (the comparison the
  original's draw table already makes inside one layer), and the cell not
  blocked to walking (`AreaMap_CellBlocked`, the high nibble of the area's
  cell byte). Anything else there stops it at the layer before: a blocked
  cell (forest, water, a building), a raised cell, a side triangle, the
  second list, a frame node, a table item that is not floor, a cell record
  within two cells, or a later sprite's body. The key's layer byte is
  changed for the pass, the list sorted again, and the key put back after
  the pass. Nothing in the layers' lists is touched.
- **Rationale:** ground the party can walk onto is never something standing
  in front of it. The owner's test (2026-10-03, on the first captures): the
  corner must be repaired on open ground *without* pushing the sprite over
  the forest graphic - which a plain "one layer later" does.
- **Also in the PSX version?** The order is the same code
  (`FUN_8014D184`'s key is the PC's term for term) and the sibling's
  renders show cuts too. The owner's emulator shots look less cut than the
  PC, "a layer higher"; **why is not established** - the owner suspects
  the GPU's rasterisation against Direct3D's, and nothing here measures it.
- **Verification:** live, narrow, x8, English, ours with the switch off
  against on (`analysis/shots/layering_1003/`, sheet `layering_fix.png`):
  `field_view.txt` - the floor over Ryu's foot gone on all four shots;
  `worldMapAndAreaTransition_ab.txt` - `f01620`, `f01680` whole where they
  were cut, `f01200`, `f01260..f01380`, `f01740` (forest, roof) unchanged,
  and no pixel outside a sprite's feet differs on any of the 32 frames;
  `worldmap_sliver.txt` (the Cedar Woods node, forest in front) unchanged.
  Three other characters' feet on the route are repaired the same way
  (`fix_others.png`). The same route wide (`BOF3X_WIDE=1`, `route_wide_m0` / `_m1`): the
  same six frames differ, the same boxes 106 px right. Headless with the
  switch set: `BOF3X_SHADOW='*'` exit 0, no mismatch line, 8,103 ours (the
  rule arms after the self-tests, so this shows only that they still pass
  around the pass's split sort). **Not seen:** a crowded town, a bridge or
  stairs, a battle (slot 4: the rule is off there by construction).
- **The owner, 2026-10-03, on the captures:** "this looks perfect - trees
  cover the character, shadows are unobstructed". In play: not yet.
- **Rejected on the way** (the same sheet's first version,
  `first_attempts_three_modes.png`): every sprite's key one layer later
  (`BOF3X_LAYERING=2`, kept as a comparison build) - whole shadows, and the
  party over the trees; and lifting the floor cells out of their lists to
  draw them before the sprite - the lists outlive a frame while the view is
  still, and a cell moved before the rows behind it is painted over by
  them on a hillside.
- **Refined 2026-10-07, the owner's first sighting in play** (Nina walking
  behind a crate and drawn through its top; the owner's `ninaWalkBehindBlock`
  route, frames 360 and 540, identical with the switch off and wrong with
  it on): the rule tested only the feet's box, and a raised cell of a
  later layer that covered the body but not the feet (the crate's top,
  corners 28 against feet 16, in layer 27's second list) was crossed. Now
  anything that is not floor stops the sprite where it reaches the sprite
  at all (the feet's box widened upward to the body's 44 px, `whole_box`
  in `layering.cpp` `Stops()`); floor is still crossed, and still only
  where it reaches the feet. The same box is used against later sprites'
  bodies and the table items. Verified: the route's eleven captures
  identical to the switch off; `field_view.txt` and
  `worldMapAndAreaTransition_ab.txt` re-run on the refined rule (below).
- **Reversible?** `BOF3X_LAYERING=0`. `BOF3X_LAYERING_AHEAD`
  (1..8) and `BOF3X_LAYERING_RISE` (0..64) move the two numbers;
  `BOF3X_LAYERING_LOG=1` with a `BOF3X_DRAWORDER` window logs each
  sprite's verdict and what stopped it.

### A panel's far corners from the quad's own x, where the original reads a stack word it never wrote

- **ID:** DIV-0072
- **Date:** 2026-10-03
- **Subsystem:** effects (`EffectKind18Sub4B_Run` `0x50A510`, ours in
  `src/game/effect_5f.cpp`; kind 0x18's sub-kinds 0x4B and 0x4C)
- **Tier:** Forced
- **Original behaviour:** on a frame of the draw pass the function builds
  three shaded quads, quad k's corners at `x0 + 0xA00 k -/+ b`. It keeps
  x0 in `ebp`, adds the quad's offset for the near corners, then reloads
  `ebp` from `[esp + 0x20]` - a local no instruction of the function
  writes - and builds the far corners' x (v1, v3) from it; from the second
  quad on `ebp` is that word, so all four corners' x come from it. The far
  corners of the first quad and the whole of the other two sit wherever
  the caller's stale stack puts them.
- **New behaviour:** ours uses x0 for that word: v0 / v2 at
  `x0 + 0xA00 k - b`, v1 / v3 at `x0 + 0xA00 k + b`, each quad symmetric
  about its column. Every other byte of every primitive is the original's.
- **Rationale:** a stack word the function never writes cannot be
  reproduced, only replaced (DIV-0023's and DIV-0068's class). x0 is the
  base the near corners already use and the only x the function computes.
- **Also in the PSX version?** Not read; the pairs give `0x801F38F0`, an
  AREA overlay copy.
- **Verification:** `BOF3X_SHADOW=effect_5f` headless: the fuzz levels
  those x words on both sides and compares every other byte, 0 mismatches
  ([`effect_5f.md`](effect_5f.md) sections 4 and 7). Not seen live: no
  recorded route reaches sub-kinds 0x4B / 0x4C.
- **Reversible?** `BOF3X_ORIGINAL=EffectKind18Sub4B_Run` runs Capcom's
  function, its stale word included.
- **The owner's word, 2026-10-03:** kept as written (entered by the
  coordinator from E5F's report).

### The masters' model's light matrix zeroed past its first row, where the original copies stale stack

- **ID:** DIV-0073
- **Date:** 2026-10-04
- **Subsystem:** the masters' screen (`Shisu_DrawModel` `0x57EEF0`, ours in
  `src/game/rest_2b.cpp`; game mode 8 step 8)
- **Tier:** Forced
- **Original behaviour:** the function's light matrix is a local at
  `[esp + 0x7C]`, of which `Light_ObjectDirection` writes the first three
  shorts - the light's row. `Gte_SetMatrix2` `0x5A8DA0` then copies all 32
  bytes into `Gte_Matrix2` `0x7DE4E0`: the other two rows and the
  translation, 26 bytes, are whatever the caller's stack held.
- **New behaviour:** ours hands `Gte_SetMatrix2` the same first row and
  zeros in the other 26 bytes. Every primitive the function draws is the
  original's.
- **Rationale:** stack the function never writes cannot be reproduced, only
  replaced (DIV-0021's and DIV-0023's class). `Gte_Matrix2` is read only by
  `Gte_NormalColor`, whose result the port throws away (the colour in is
  copied over it, `src/game/psx_gte_transform.cpp`), so nothing drawn or
  decided depends on the bytes; zero is the value that says so.
- **Also in the PSX version?** Not read; the cut pairs the screen's rows
  with the SHISU overlay.
- **Verification:** `BOF3X_SHADOW=rest_2b` headless: the fuzz compares the
  matrix's first row and levels the rest, 0 mismatches
  ([`rest_2b.md`](rest_2b.md) section 7, L1). Not seen live: no recorded
  route opens the masters' model. On a route that does, the state hash
  ([`state-hash.md`](state-hash.md)) reports `0x7DE4E6..0x7DE4FF` from the
  frame the model first draws - this entry, and not in the skip list, since
  the field's own draws load the whole matrix there and are compared.
- **Reversible?** `BOF3X_ORIGINAL=Shisu_DrawModel` runs Capcom's function,
  its stale bytes included.
- **The owner's word, 2026-10-04:** kept as written, zeros in the unused
  bytes as the other stale-byte entries have it - after a look at the
  sibling for anything it knew of the function: `../BreathOfFire3Recomp`
  has the SHISU overlay unnamed (`names/overlays.toml`, id `0x015`, no role,
  no evidence), no name or note at the twin `0x801D2308`, and nothing on its
  light matrix; the code there is the recompiler's output only. So the
  PlayStation side has not been read either, and "Also in the PSX
  version?" stays unanswered.

### A random enemy from the bytes the function wrote, where the original's four-byte list overflows into stale stack

- **ID:** DIV-0074
- **Date:** 2026-10-05
- **Subsystem:** battle (`Battle_RandomLiveEnemy` `0x452F10`, ours in
  `src/game/rest_4a.cpp`; its caller is `BattleAction_PickRandomAbility`
  `0x42F9D0`)
- **Tier:** Forced
- **Original behaviour:** the function lists the enemies 3..10 that are not
  out in four bytes of a 12-byte stack frame and answers the one at
  `Rand() % count`. Eight enemies can stand. From the fifth on, the writes
  run over the count byte (the count becomes that enemy's number) and into
  the two dwords after the list, so the pick is any of frame bytes 0..9 -
  among them six bytes no instruction of the function writes (the upper
  bytes of the count's dword and of the loop's): whatever the caller's
  stack held.
- **New behaviour:** ours keeps the same twelve bytes and makes the same
  writes, the overflow included, so every pick the original makes from a
  byte it wrote is ours too. The six never-written bytes are 0. With four
  or fewer enemies standing nothing differs.
- **Rationale:** stack the function never writes cannot be reproduced, only
  replaced (DIV-0023's, DIV-0068's and DIV-0072's class); 0 is what the
  function itself puts in those dwords' neighbours. The overflow itself is
  Capcom's defect and is kept: a bound on the list would change which enemy
  is picked in fights the original plays deterministically, and that is the
  owner's to ask for, not this entry's.
- **Also in the PSX version?** Not read.
- **Verification:** `BOF3X_SHADOW=rest_4a` headless: the fuzz's
  `Battle_ActorIsOut` stand-in zeroes those bytes on the original's side
  and every answer is compared, 0 mismatches; control C6 (one of the bytes
  not 0) is refused in 600 of 6,000 rounds
  ([`rest_4a.md`](rest_4a.md) sections 6 and 7, L1). Not seen live: whether
  a recorded battle calls it with five or more enemies standing is not
  known.
- **Reversible?** `BOF3X_ORIGINAL=Battle_RandomLiveEnemy` runs Capcom's
  function, its stale bytes included.
- **The owner's word, 2026-10-05:** fine as written.

### The community's name entry ends unanswered, where the original never leaves its step

- **ID:** DIV-0075
- **Date:** 2026-10-05
- **Subsystem:** the community's name screen (`CommuName_SlotEntry`
  `0x45D730` and `CommuName_MemberEntry` `0x45E2C0`, ours in
  `src/game/rest_4d.cpp`)
- **Tier:** Intent - the port removed the PlayStation's name entry (its
  grid's draws are calls of a bare `ret`, its input step a function that
  answers 0) but left the step that waited on it.
- **Original behaviour:** on the PlayStation the input step itself moves
  the entry's step on, when a name is finished or the entry abandoned
  ([`name-entry-restoration.md`](name-entry-restoration.md) section 4).
  The port's replacement only answers 0 and nothing else writes the step
  byte `0x939A3F`, so once play chooses to enter a name the screen stays
  on this step for good: the panel, an underline, no grid and no input
  that leaves it.
- **New behaviour:** after the answer (still 0) ours moves the step on,
  which is what the PlayStation's input does when the player abandons the
  entry. The next step, unchanged, slides the panel out and takes its
  unanswered branch: message 0xF7 and back to the screen's state 4. No
  name is changed. Drawing a name at random, the screen's other choice,
  is untouched.
- **Rationale:** the owner's decision, 2026-10-05: fix the hang now;
  bringing the entry itself back waits for the localisation rework, which
  may change the font files the grid would draw from. Abandoning is the
  one exit the PlayStation's routine has that needs no grid, no glyphs and
  no input, and it leaves the save as it was.
- **Also in the PSX version?** No: the PlayStation has the entry
  (`COMMU02`, input step `0x801DA1D8`).
- **Verification:** `BOF3X_SHADOW=rest_4d` headless compares Capcom's two
  steps with the switch off (the switch is set after the self-test, as
  DIV-0070's), 0 mismatches; then, with the switch on, a row of ours alone
  (DIV-0063's form, `rest_4d::EntryAbandonTest`, 2026-10-05): the two steps
  fuzzed against Capcom's with the step byte taken back by one, 8,000 rounds,
  0 mismatches, three plants refused. Not seen live: no recorded route enters
  the community, and whether the port's play can reach the entry at all is not
  established.
- **Reversible?** `BOF3X_ORIGINAL=CommuName_SlotEntry,CommuName_MemberEntry`
  runs Capcom's steps, which do not return from the entry.

### The save slot's summary names the character whose level it shows

- **ID:** DIV-0076
- **Date:** 2026-10-05
- **Subsystem:** the save block (`Save_BuildBlock` `0x5806F0`, ours in
  `src/game/rest_2c.cpp`; reached by `FieldSave_Write` and `Save_QuickWrite`)
- **Tier:** Intent - the owner's decision, 2026-10-05: the summary a save slot shows named one character and levelled another.
- **Original behaviour:** the slot's summary takes its name from the
  party leader's record (the record of party id 0, `strncpy` 5 and 4 bytes)
  but its level (`0x903A7A`) and the dword at `+0xC` (`0x903A7C`) from
  record 0, whatever the party. With another member leading, the save and
  load screens show the leader's name beside record 0's level. Confirmed in
  game by the owner, 2026-10-05.
- **New behaviour:** with the switch `g_summary_record0` on (set by
  `Rest2C_Inject` after the self-test), the name is record 0's too: the
  summary names and levels one character. The rest of the summary and the
  block are unchanged.
- **Rationale:** the owner's decision, 2026-10-05: "always pull the name and
  level of Ryu's character" - record 0 is the character the game's level
  field already describes, so the name follows it rather than the other way.
- **Also in the PSX version?** Not read: the PlayStation's save summary is a
  different structure ([`save-interchange.md`](save-interchange.md)); whether
  its block builder mixes the two is not established.
- **Verification:** `BOF3X_SHADOW=rest_2c` headless compares Capcom's
  `Save_BuildBlock` with the switch off (the switch is set after the
  self-test, as DIV-0075's), 0 mismatches. **Seen live by the owner,
  2026-10-06:** a save with another member leading loads showing Ryu's level
  and name together.
- **Reversible?** `BOF3X_ORIGINAL=Save_BuildBlock` runs Capcom's.

### A TILE_1 covers the PlayStation pixel's footprint, not one screen pixel

- **ID:** DIV-0077
- **Date:** 2026-10-06
- **Subsystem:** the renderer (`D3d_DrawTile1` `0x5A2220`, ours in
  `src/game/d3d_rest.cpp`; `Gfx_DrawOTag`'s handler for code `0x68`,
  `Gpu_SetTile1`'s primitive)
- **Tier:** Intent - the owner's decision, 2026-10-06, off a capture: the
  dream scene's drifting specks were a quarter of their size.
- **Original behaviour:** the port draws a TILE_1 (a one-pixel tile of the
  PlayStation's 320 x 240) as a `D3DPT_POINTLIST` of one vertex at the
  scaled corner. At `D3d_ScaleX` / `D3d_ScaleY` = 2 the frame has four pixels
  where the PlayStation had one and the point lights one of them, the
  top-left; at larger scales the gap grows ([`d3d-rest.md`](d3d-rest.md)
  D-a). Seen in the `whelpBoss` route's dream scene (frame 11880: the specks
  in Deis's light pillar) and built for the Kaiser and shadow-mote battle
  effects no route casts.
- **New behaviour:** with the switch `g_tile1_quad` on (set by
  `D3dRest_Inject` after the self-test), the tile is a quad from the scaled
  corner to the scaled (x + 1, y + 1) - a triangle strip of four, as
  `D3d_DrawTile` draws a TILE of w = h = 1 - with the point's colour, blend
  and shade. The primitive, its builders and the rest of the walk are
  unchanged.
- **Rationale:** the owner, 2026-10-06, shown a four-times zoom of frame
  11880 beside a paint mock of the 2x2: "go ahead and make it larger".
  The PlayStation's pixel is the unit the effect was authored in.
- **Also in the PSX version?** No: the PlayStation's GPU drew the TILE_1 as
  its one pixel. This is the port's renderer, which has no PSX twin.
- **Verification:** `BOF3X_SHADOW=d3d_rest` headless compares Capcom's
  handler with the switch off (set after the self-test, as DIV-0075's and
  DIV-0076's), 0 mismatches. Live: the `whelpBoss` route's frames around
  11880, ours against `BOF3X_TILE1=0`.
- **Reversible?** `BOF3X_TILE1=0` leaves the switch off (the point);
  `BOF3X_ORIGINAL=D3d_DrawTile1` runs Capcom's.

### BOF3.CFG's key lines cannot run off the end of Cfg_Load's frame

- **ID:** DIV-0078
- **Date:** 2026-10-06
- **Subsystem:** the shell (`Cfg_Load` `0x4FD030`, ours in `src/game/shell.cpp`
  as `Shell_CfgLoadFrame`; `Cfg_SetKeyTable` `0x5A9860` copies the result)
- **Tier:** Sensible - a stack overrun from a configuration file; the owner's
  decision, 2026-10-06: "cfg_load probably needs overrun protection".
- **Original behaviour:** each key line of `BOF3.CFG` (line 3 on) is scanned
  with `sscanf("%d %d")` into two **byte** pointers of the function's own
  0x3C-byte frame, two lines an entry, and `Cfg_SetKeyTable` then copies 0x80
  bytes from the frame's `+0x14` into `Key_Table`. The frame holds 0x28 bytes
  of that table, so entries 10..31 are the return address into WinMain and
  0x54 bytes of WinMain's frame; from the 21st key line the scan overwrites
  the return address itself ([`shell.md`](shell.md) sections 2 and 5). Only a
  hand-edited file reaches it: the launcher writes two lines and passes the
  rest through unchanged.
- **New behaviour:** with the switch `g_cfg_own_table` on (set by `Shell_Inject`
  after the self-test), the key lines scan into a zero-filled 0x80-byte table
  of our own (plus the 8 bytes the last pair's ints spill into), through the
  same byte pointers and the same packing - two lines an entry, the spill as
  the original leaves it - and lines past the 32nd entry (the 66th line on) are
  read and ignored. `Cfg_SetKeyTable` copies our table. The first two lines,
  the no-file and the two-lines-or-fewer cases are unchanged. What a player
  with key lines sees: entries 10..31 are zero instead of stack bytes, so
  `Pad_Read`'s walk stops where the lines end, and 21 or more lines no longer
  return into garbage.
- **Rationale:** a stack overrun from a configuration file is a defect of the
  port with no gameplay content; the owner asked for protection. The fix is the
  one group PW proposed ([`platform-round.md`](platform-round.md) section 4).
- **Also in the PSX version?** No: the PlayStation has no `BOF3.CFG`; the file
  and its reader are the port's.
- **Verification:** `BOF3X_SHADOW=shell` headless compares Capcom's `Cfg_Load`
  with the switch off (the frame and the bytes above it byte for byte, as
  before), 0 mismatches. The switch-on path is not compared against the
  original (it exists to differ); not yet seen live.
- **Reversible?** `BOF3X_ORIGINAL=Cfg_Load` runs Capcom's.

### LINE primitives are the PlayStation pixel's width, not one screen pixel

- **ID:** DIV-0079
- **Date:** 2026-10-06
- **Subsystem:** the renderer's six LINE handlers - `D3d_DrawLineF2` `0x5A17A0`,
  `D3d_DrawLineF4` `0x5A1D10` (`src/game/d3d_draw.cpp`), `D3d_DrawLineF3`
  `0x5A1A00` (`field_misc.cpp`), `D3d_DrawLineG2` `0x5A18B0`, `D3d_DrawLineG3`
  `0x5A1B50` (`battle_draw.cpp`), `D3d_DrawLineG4` `0x5A1EA0` (`d3d_rest.cpp`);
  the shared drawer `src/game/d3d_lines.cpp`
- **Tier:** Intent - the owner's decision, 2026-10-06, off a capture of the
  fishing gauge: "all elements should scale".
- **Original behaviour:** each handler scales its corners to screen
  coordinates and hands Direct3D a `LINESTRIP`, which rasterises one screen
  pixel wide at any window scale. A PlayStation line was one pixel of 320 x
  240, so at the owner's window (a scale of about 3.3) every line in the game
  is a third of its width: the fishing gauge's bar and centre mark, the
  fishing grey lines (LINE_F2, 8,662 calls in the recorded routes; LINE_F4
  1,894), and whatever builds LINE_F3 and the G kinds (no route). Measured on
  the `caughFish` route's frame 1680 ([`owner-review.md`](owner-review.md)).
- **New behaviour:** with `d3d_lines::g_wide` on (armed after every module's
  self-test, as DIV-0041's fills are), each segment is a quad: the ends moved
  to their pixel's centre, extended half a pixel along the line (a square cap)
  and half a pixel to each side, each axis at its own scale, drawn as a
  `TRIANGLESTRIP` of four with the ends' own colour, depth and blend. An
  axis-aligned line covers exactly the pixels the PlayStation's covered; a
  diagonal is a smooth band of that width (the PlayStation's stepped in
  pixel stairs); a polyline is one quad per pair of corners, with a notch at
  the joints a one-pixel line never showed. A zero-length line is one pixel.
  The primitive, its builders and the handlers' state calls are unchanged.
- **Rationale:** the owner, 2026-10-06: "all elements should scale, right?",
  then the recommendation accepted: one segment per pair, square ends, smooth
  diagonals, on by default. The TILE_1 class (DIV-0077) for lines.
- **Also in the PSX version?** No: the PlayStation's GPU drew lines in its
  own pixels. This is the port's renderer, which has no PSX twin.
- **Verification:** the four modules' shadows (`d3d_draw`, `d3d_rest`,
  `battle_draw`, `field_misc`) compare Capcom's handlers with the switch
  off, 0 mismatches. Live (2026-10-06): the `caughFish` route's frames
  1500..1700 ours against `BOF3X_LINES=0` (`analysis/shots/lines_1006_quad`,
  `_strip`): frame 1680 differs in 11,949 pixels, all in the gauge (the red
  bar and the centre mark, now the scale's width) and the instruction
  banner's outline - the only lines on the frame; nothing else moved.
- **Reversible?** `BOF3X_LINES=0` leaves the switch off (the strip);
  `BOF3X_ORIGINAL=D3d_DrawLineF2,...` runs Capcom's handler.

### Dauna Mine's minecart map is walled as every later release walled it

- **ID:** DIV-0080
- **Date:** 2026-10-06
- **Subsystem:** the area data as loaded (`src/game/area4_walls.cpp`, run at
  the end of `LoadDatFile` in `src/game/dat_load.cpp` for `AREA004.DAT`; the
  cell plane at `AreaMap_Header` `0x8CB580`'s block, the battle placement
  nibble map at `0x8C3D80`)
- **Tier:** Intent - the owner's decision, 2026-10-06: "if it looks like a bug
  fix, it's probably worth keeping the change as the default option", and
  "make the code change so that it works the same regardless of source".
- **Original behaviour:** the PC port carries the Japanese disc's map of area
  4 (Dauna Mine's minecart area), in which 72 cells along the raised strip's
  east edge (x 28, z 9..30 and 35..65, the doorway at z 32..33 open), its
  west side's north end (x 25, z 9..11) and the corridor's bottom edge (z 71,
  x 7..22) are open floor between wall stubs, and 8 of those cells are open
  to battle placement. Every later release - the US, French and German PSX
  discs and both PSP discs - walls those cells (`0x10`, the value the
  neighbouring stubs already carry) and the PSX discs close the 8 placement
  cells; the PSP took the walls but not the placement half
  ([`region-diff.md`](region-diff.md) sections 8 and 10).
- **New behaviour:** with `BOF3X_AREA4_WALLS` on (the default; armed after
  every module's self-test), each load of `AREA004.DAT` sets the 72 cells to
  `0x10` and the 8 placement nibbles to 0, from a coordinate table in our
  code, guarded: only when the block is 90 x 88 and every cell still holds
  JP's value; a map that already has the walls is left alone with a log
  line. The later discs' 30-cell re-texture of the same strip is **not**
  taken: it is Capcom's texture records, not expressible by coordinate.
- **Rationale:** a collision fix every later build made; applied by
  coordinate so the PC install, the JP disc and any later disc give the same
  area - the engine / data split keeps the bytes the player's
  ([`ASSET_SOURCES.md`](ASSET_SOURCES.md) section 5).
- **Also in the PSX version?** The JP disc has the open cells; every later
  disc has the walls. This follows the later discs.
- **Verification:** `tools/region_read.py fix` parses the table out of the
  C++ source, applies it to the JP disc's sections 8 and 10 and compares with
  the US and German discs: the cell bytes and the placement map identical,
  the 920 remaining differences all in the re-texture not taken.
  `'*'` narrow at the agent's tip, 0 mismatches (the switch is armed after
  the self-tests). **Not seen live:** the owner's walk of the strip's east
  and bottom edges, blocked with the fix and open with `BOF3X_AREA4_WALLS=0`.
  Area 4 plays twice in the attract cycle, so state-hash reference runs want
  the switch off. **Attract on against off (2026-10-06,
  `analysis/statehash/attract_walls_on` / `_off`):** the off run identical to
  the references on all 10,305 ticks; the on run differs from them in
  exactly four pages, the cell plane's two (`0x8D3000`, `0x8D4000`) and the
  placement map's two (`0x8C3000`, `0x8C4000`), from the first load at tick
  1,312 - the fix's own footprint and nothing else, so the demo's scripted
  moves never meet the walls. The oracle's one differing row is a sampler
  poll on an area-load boundary (area `0x0002` against the `0xffff` marker),
  not game state.
- **Reversible?** `BOF3X_AREA4_WALLS=0` leaves the map as loaded. **From a
  Western disc alone** (the importer's cache, unified-data step 3,
  [`importer-transforms.md`](importer-transforms.md) section 5): the cache
  holds that disc's own `AREA004` rows, walls and the 30-cell re-texture
  with them; the guard's "already walled" branch leaves them, and the switch
  cannot restore the open map from such a cache. A by-source difference
  until the owner says otherwise (the owner's call 4 there).

### The music loops inside its file, at the points the disc's sequence loops

- **ID:** DIV-0081
- **Date:** 2026-10-06
- **Subsystem:** the music pump (`Music_Decode` `0x5A6F30`'s end-of-stream
  path, ours in `src/game/sound.cpp`; the new `src/game/music_loops.cpp` with
  its generated table `music_loops_table.inc`)
- **Tier:** Intent - the owner, 2026-10-06, after the listening set: "the
  quality between the mp3 / disc isn't that bad, but the seams are *very*
  noticeable - I noticed the combat one in game, but the town music is also
  really noticeable side by side."
- **Original behaviour:** a looping track's decoder is rewound to the file's
  start when the stream ends: the intro replays on every pass (7.4 s on the
  battle theme), a file cut mid-pass jumps from mid-phrase to the intro, and
  the join is a cut plus the file's lead-in (6..23 ms of near-silence). The
  PlayStation's sequences loop to a point inside the song
  ([`bgm-comparison.md`](bgm-comparison.md) sections 7 and 12.1).
- **New behaviour:** with `BOF3X_MUSIC_LOOPS` on (the default; armed after
  every module's self-test), a track with a measured row loops from the row's
  end back to its start inside the file, sample-accurate - the decoder is
  rewound and the frames before the loop start discarded, so the samples
  after the jump are the first pass's bit for bit - with a 2.9 ms crossfade
  at the join (5.8 ms on a row shifted to fit a file cut short of one body,
  the battle theme today). A track without a row rewinds as before. Thirteen
  tracks have rows today (`003 014 036 051 060 063 064 079 082 085 090 144
  153`); the full measurement of the 156 looping songs is paused for a
  machine left on (section 11's resume command). The town theme `000` is
  **excluded**: its file is 0.44 s shorter than one loop period, so no
  correct loop exists inside it - the owner's decision (section 11.3).
- **Rationale:** the owner's words above; the measurement that the disc
  loops inside the song. The table is our own measurement, regenerable from
  `analysis/bgm/loops.json` by `tools/bgm/gen_loop_table.py`; it holds
  sample positions, not game data.
- **Also in the PSX version?** The PlayStation plays the sequence, which
  loops at these points by its own markers; the PC's rewind is the port's.
  This restores the disc's loop structure on the PC's recordings.
- **Verification:** `tools/bgm/prove_loops.py` splices each row the engine's
  way and scores the first second after the join against the disc's render
  or the file's own continuation: the battle theme from -0.10 with 809
  near-silent samples to 0.63 with no gap; the eleven in-file rows from
  -0.14..0.22 to 0.977..0.989. `BOF3X_SHADOW=sound` loops a stand-in decoder
  sample-exactly in four call patterns and checks every row; `'*'` narrow
  passed on the agent's tip and the `sound` module at the merged tip.
  **Not yet heard in the game**: `analysis/bgm/listen/153_loop_fixed.wav` is
  the offline splice for the owner's ear; whether replaying the intro's
  frames at each loop causes a hitch in play is unmeasured.
- **Reversible?** `BOF3X_MUSIC_LOOPS=0` rewinds every track as the original.
- **The full measurement, 2026-10-08** ([`bgm-comparison.md`](bgm-comparison.md)
  section 11): all 153 songs rendered and measured, the method rewritten
  the same day (the SPU's noise voices and free-running modulations defeat
  a waveform-only measure). Of 156 looping tracks 48 have a loop in their
  file, 80 hold less than a period (20 by a frame or two, `near_full` - the
  owner's call whether a slip of that size a pass beats the rewind), 28
  none that can be measured. The table in `music_loops_table.inc` is
  regenerated from `loops.json` by `gen_loop_table.py` (not yet re-run at
  this writing). **The owner's stance the numbers led to:** the disc's
  music by default wherever a disc is a source
  ([`unified-data-plan.md`](unified-data-plan.md) section 6); this entry
  is what a PC-only install gets.

### The enemy AI rows' done bits cleared at spawn (the Volt's EXP bonus)

- **ID:** DIV-0082
- **Date:** 2026-10-07
- **Subsystem:** battle setup (`Battle_CopyEnemyData` `0x4946C0`, ours in
  `src/game/battle_sprites.cpp`; read by `EnemyAI_TurnCheck` `0x44AAD0`,
  `EnemyAI_RowDone` `0x44AC80`, `EnemyAI_SetRowDone` `0x44B2E0`)
- **Tier:** Intent - a port bug; the PlayStation shows what was meant.
- **Original behaviour:** an enemy's four AI rows each carry a "fired" bit in
  the enemy object's byte `+0xF1` (`EnemyAI_SetRowDone` sets it after a hit
  row fires; `EnemyAI_RowDone` refuses a row whose bit is set). Nothing in the
  PC port clears the byte: `Battle_CopyEnemyData` clears `+0x10D`, `+0x110..`,
  `+0x114..`, `+0x118..`, `+0x11C..` and `+0x122..+0x125` and sets `+0xF0`,
  `Battle_PlaceBossActor` clears the first 0x80 bytes and the same list,
  `BattleEnemy_ClearStates` zeroes bytes `+0..+4` at a fight's end. The
  objects (`0x93B960`, eight of 0x128) are `.bss`, zeroed once at process
  start. So a once-only row fires in the first fight that meets its condition
  in that slot and never again for the session: the Volt's row 1 ("hit by
  element mask 4: EXP x3, message 0x82", `tools/enemy_ai.py --name Volt`),
  the Tar Man's frost row, every other once-only hit row.
  The owner's fight of 2026-10-06 (three Volts, one Thunder, the Thunder's
  own whole-side lightning as the trigger): one Volt tripled, 84 + 28 + 28 +
  16 = 156, halved over the two living members = the 78 on screen
  ([`trigger-mode-enemies.md`](trigger-mode-enemies.md)).
- **New behaviour:** `Battle_CopyEnemyData` also writes `+0xF1 = 0`
  (working record `+0x71`), so every spawned enemy starts with its rows
  unfired, as on the PlayStation.
- **Rationale:** the PSX twin (`0x800A9148`) clears the same list and not
  `+0xE1` either - but there the enemy objects (`0x801EB5A0`, stride 0x118)
  lie inside the BATTLE.EMI#3 game-mode image (`0x801D0C00..0x801ED93F`,
  118,080 bytes, md5 `8a80230e...` as the sibling's catalogue has it), which
  is loaded from disc over the field overlay at every battle entry; the
  image's bytes at every object are zero (read off the sibling's disc image
  2026-10-07). The PC keeps the objects in static memory and so keeps what
  the PlayStation threw away. The logic (`EnemyAI_TurnCheck`,
  `EnemyAI_CondElement`, `EnemyAI_ApplyAction` kind 6, `EnemyOp_ReceiveAction`
  running the check after the damage and before the kill) matches the PSX
  twins (`0x80098BB0`, `0x80099874`, `0x80099954`, `0x801E3B8C`) line for
  line; the state did not.
- **Also in the PSX version?** No: the reload resets it. This restores the
  PlayStation's effective behaviour.
- **Verification:** `BOF3X_SHADOW=battle_sprites`: the clone comparison seeds
  `+0xF1` 0 for `Battle_CopyEnemyData`'s rounds (3,000, 0 mismatches), and a
  dedicated check hands both the original's clone and ours a record with
  `+0xF1 = 0xFF` on each of the eight slots - the original keeps 0xFF, ours
  leaves 0. Owed: the owner's live fight (a Volt group with a thunder hit,
  twice in one session; every Volt should yield 84 both times).
- **Reversible?** No switch: a static byte the PlayStation never carried over.

### The master list's pupils box without the port's label

- **ID:** DIV-0083
- **Date:** 2026-10-07
- **Subsystem:** menu, the camp's master list (`MasterWin_DrawPupils`
  `0x59C8F0`, ours in `src/game/rest_2h.cpp`, kind 17 of
  `Window_Handler7KindTable`'s camp set)
- **Tier:** Sensible
- **Original behaviour:** beside the pupils' portrait box (x + 3, y + 3,
  0x72 x 0x58) the 2001 port draws a second, smaller box to its left (x -
  0x25, y + 3, 0x22 x 0x10, its border at x - 0x28) with the label 弟子
  (`0x66A1F8`), which no overlay translated: a Chinese box on an English
  screen (the camp route's frame 2400,
  `analysis/shots/master_labels/masters.png`). The PlayStation's screen has
  the portrait box alone - the owner's capture of the US release,
  2026-10-07, after the question of [`yes-no-prompts.md`](yes-no-prompts.md)
  section 5 - and the US disc carries no string for such a label beside
  its `MSTR` (DIV-0064's sixth group found `MSTR` and the star mark alone
  in `SHOP.EMI`).
- **New behaviour:** under a Latin language overlay the label box, its
  border and the label are not drawn; the portrait box and its border are
  as before. `g_pupil_label_off`, patched to 1 by `Rest2H_Inject` after the
  group's fuzz under the name `MasterPupilLabel`.
- **Rationale:** the PlayStation's screen as the reference, as DIV-0051; a
  box whose only content no overlay has a word for.
- **Also in the PSX version?** Not applicable - this is the PlayStation's
  layout brought back.
- **Verification:** the `rest_2h` self-test (the fuzz runs before the
  patch): 144,000 rounds over 36 functions, 0 mismatches. Live: the camp
  route with a shot at frame 2400 (`analysis/shots/master_labels2/masters.png`):
  `MSTR`, Mygas, the stat box and the portrait box, nothing left of the
  portraits - the owner's US capture beside it, the same.
- **Reversible?** play without `BOF3X_LANG`, or
  `BOF3X_ORIGINAL=MasterPupilLabel` keeps the overlay and the box.

### The formation screen's names centred in their boxes

- **ID:** DIV-0084
- **Date:** 2026-10-07
- **Subsystem:** menu, the formation screen's icon wheels
  (`Menu_DrawIconWheel` `0x573F70`, ours in `src/game/field_o.cpp`)
- **Tier:** Sensible
- **Original behaviour:** each wheel's box is 0x45 wide and its name is
  drawn by the 8 px draw from x + 0x16: the port's four-glyph names
  (传统阵形, 32 px) sit on the box's middle. The US release draws the name
  from its `START.EMI` twin at x + 0x27 (`0x801dcf30`, `addiu $a0, $fp,
  0x27` before the small-text call `0x8014fc90`), the box's middle, so its
  routine centres - `Defense` at 8 px a letter is 56 px, which from x +
  0x16 would run six past the box.
- **New behaviour:** once DIV-0064's group 10 has written the names one
  byte a letter (`Labels_SmallWritten(10)`), the name starts at x + 0x27 -
  4 x its letters, under a Latin overlay: `g_wheel_name_centre`, patched
  to 1 by `FieldO_Inject` after the fuzz under the name
  `FormationNameCentre`. Otherwise the port's x + 0x16.
- **Rationale:** as DIV-0059: the US words in the US place.
- **Also in the PSX version?** Not applicable - the PlayStation's own
  placement brought back.
- **Verification:** the `field_o` self-test (before the patch): 82,000
  rounds over 41 functions, 0 mismatches. Live: the owner's
  `sortScreens.txt`, frame 1440 (`analysis/shots/sortScreens2/f01440.png`):
  `Normal`, `Attack`, `Defense` each on its box's middle.
- **Reversible?** play without `BOF3X_LANG`, or
  `BOF3X_ORIGINAL=FormationNameCentre` keeps the overlay's words at the
  port's x.

### A side face gained at run time drawn with the cell's one side word (the sea bridge's "waterfall")

- **ID:** DIV-0085
- **Date:** 2026-10-08
- **Subsystem:** the field view's cell textures (`MapView_CellTextures`
  `0x56F9B0`, ours in `src/game/map_layers.cpp`; the snapshot
  `map_layers::SnapshotSides`, run at the end of `LoadDatFile` in
  `src/game/dat_load.cpp` when a file carried the area block, tag `0xC8000`)
- **Tier:** Intent - the owner's request, 2026-10-08: the bridge's camera is
  fixed, so a duplicate texture on the south face "won't be noticeable".
- **Original behaviour:** a cell's side faces are chosen once, when its draw
  item is created, by comparing corner heights at that frame; its texture
  words are read own, south (if it has one), east (if it has one) from its
  tile's run. The map authors a word for each side the *file's* heights
  give a cell and no more (`known-defects.md` D239: the survey of all 200
  area blocks, PC and JP disc). Where code moves heights during play, a cell
  can be created with a side its file heights do not give it, and then reads
  a dword past its run - the next tile's own word. On `AREA060`'s sea bridge
  the sky effect (`EffectKind18Sub15_Draw`) leaves one-unit steps along the
  deck behind a party walking north: the deck's east edge (column 48, its
  run own + the east cliff's 64-texel rectangle) gets a south step, the step
  takes the cliff word and the 32-unit cliff takes the sea's 16 x 16 tile,
  stretched - the streaks. The PlayStation is the same code
  (`FUN_80153B8C`, `FUN_80154D50`) and the same data.
- **New behaviour:** with `BOF3X_SIDE_DUP` on (the default; armed after
  every module's self-test), when a cell is textured with both side faces
  and the file's heights give it exactly one, both faces take that one side
  word - when its tile's run carries no third word of its own (the run
  ends at the next tile index any cell uses; a one-sided cell whose tile has
  a third word, 10,309 across the game, has one authored for the other side
  and keeps the original's read, added 2026-10-08 after the owner's check).
  Every other cell is read as the original reads it: a cell the file gives
  both sides, or none, is untouched, so shipped geometry draws as before.
  The first 20 cells it changes are logged (`DIV-0085    map cell x,z`).
- **Rationale:** the extra face exists only because heights moved; the
  word the original gives it is another tile's, and on the bridge a tall
  cliff stretched from a sea tile. The cell's own side word is the closest
  texture the data has, and under the bridge's fixed camera the south step
  it lands on is a few texels high.
- **Also in the PSX version?** Yes, the defect: identical code and data.
  Whether it shows there depends on when the cull creates the deck's edge
  cells; in this port, D239's two logs tie it to DIV-0041's
  terrain margin; that the margin puts the creation line on the steps is
  inferred, not measured.
- **Verification:** compiled (mingw GCC, `map_layers.cpp`, `dat_load.cpp`,
  `inject_all.cpp`; a cloud session). Offline on `AREA060`'s block: the sky
  effect's height writes applied for a walk north from row 70 to 50 leave
  column 48's rows 63..83 with both faces, each a file east-only cell whose
  original east word would be `0x0180001E` (the sea tile) and is now its own
  `0x12800100`. **Seen by the owner, 2026-10-08**, on their machine's
  build, wide, the bridge walked several times: the streaks do not come
  back and nothing is off at the deck's edges ("I can't notice anything
  off about the deck edges").
  `AreaMap_ApplyPatch`'s height patches come after the snapshot, so a
  patched cell keeps the file's side set; checked offline, it never matters:
  the four areas with height patches (`AREA094`, `103`, `128`, `140`; every
  combination of their entries' two values) change the sides of up to 42
  cells and never give a one-sided cell both. Where else the rule can act:
  `known-defects.md` D239, "Where DIV-0085 can act". The run guard was
  compiled and mirrored offline (the bridge's 97 edge cells still qualify);
  the owner's look was before it.
- **Reversible?** `BOF3X_SIDE_DUP=0`.

### Optional layers from the player's PSP disc, walked after the language overlay

- **ID:** DIV-0086
- **Date:** 2026-10-08
- **Subsystem:** assets (`LoadDatFile` `0x454590`, ours in
  `src/game/dat_load.cpp`; the kind-5 name chunks of
  `src/game/name_tables.cpp`; the launcher's `opt=` key,
  `src/launcher/config.cpp`)
- **Tier:** Sensible - a later release's palettes, textures, texture
  coordinates and names, off by default; nothing that happens in play
  changes (no cell byte, height, number or line of code is in a layer).
- **Original behaviour:** `LoadDatFile` walks `DAT\<name>` and nothing else
  (DIV-0005 added the language overlay `DAT\<tag>.<name>`). The PC port
  carries the JP disc's art, maps and names; the PSP releases (2005, from JP's
  data) recoloured Stallion (P6), renamed an ability and some items (P7 and
  more), redrew or blanked 954 area texture tiles, changed 515 palette rows in
  52 areas and edited 11 areas' map bands ([`region-diff.md`](region-diff.md)
  5, [`psp-stallion.md`](psp-stallion.md) 2-4,
  [`exe-tables-by-build.md`](exe-tables-by-build.md) 4.4).
- **New behaviour:** with `BOF3X_OPT=<layer>[,<layer>...]` set, after the
  file and its language overlay, `DAT\<layer>.<name>` is walked for each
  layer in that order when it exists, so each lands on top of the last.
  The layers ([`opt-layers.md`](opt-layers.md)) are built by
  `tools/importer.py build --opt` from the player's own PSP disc and copied
  in by `importer.py install`; none is shipped or committed: `psp-art`
  (Stallion's two palette rows in area 67 and eight in area 166),
  `psp-tiles` (840 page tiles and 515 palette rows of the areas; the port's
  dial-page tiles kept), `psp-maps` (11 map bands' texture coordinates, tile
  words and cell-run records), `psp-names-en-150` (8 names, ability 116
  among them) and `psp-names-ja-JP` (5). A layer chunk is a sub-range of the
  chunk it changes: kind 0 at the base tag plus the offset, kind 1 at the
  replaced tiles' rectangle, and kind 5 - **widened here** - on any run of
  records of a name table (`NameTables_Apply` took only whole tables; a
  whole-table chunk is handled as before). Refused at start-up, not skipped:
  a layer name over 23 characters or with a character other than a letter,
  digit or `-`, one named twice, more than 8, a layer with no
  `DAT\<layer>.*.DAT`, a text layer (a name ending in `-<tag>`) under another
  language than `BOF3X_LANG`'s; and in the walk, a path that would not fit
  its 0x40-byte buffer. The launcher's `opt=` key sets the variable with
  the layers installed and of the language played (no dialog box yet).
- **Rationale:** the plan's divergence policy for data
  ([`unified-data-plan.md`](unified-data-plan.md) 7): a later build's
  content change is an optional layer from the player's own disc, off by
  default, recorded here as existing. The owner asked for the PSP's
  Stallion colours as a toggle ([`psp-stallion.md`](psp-stallion.md)).
- **Known and accepted:** what the PSP's blanked tiles and map-band edits
  are for is read only as far as their structures (texture coordinates
  inset by a texel or two, textures given to 152 empty cells at the edge of
  one map, [`opt-layers.md`](opt-layers.md) section 4); a render-filter
  and wide-view accommodation is the reading, not seen. A text layer's names
  are the PSP's spelling under the PSX layer's font; in a save they are
  stored by id, not by text.
- **Verification:** offline (2026-10-08, [`opt-layers.md`](opt-layers.md)
  section 6): `LoadDatFile`'s walk simulated over the PC's `DAT/` and the
  layers - every chunk a layer lands on equals the PSP disc's section (130
  of 133; the other 3 differ in exactly the port's 14 dial-page tiles, kept),
  every other chunk the PC's own (920 of 920); the layers byte-identical
  built from either PSP disc; `importer.py verify` composes each landing
  chunk against `recipes/opt.toml` (133 of 133). Compiled
  (`i686-w64-mingw32-g++ -fsyntax-only -Wall -Wextra`: `dat_load.cpp`,
  `name_tables.cpp`, `launcher/config.cpp`). **Not run:** the owner's
  llvm-mingw build, the `'*'` self-tests, and any live look (Stallion is
  fight 24 in area 67; no recorded route reaches it).
- **Also in the PSX version?** No: the PSX discs have JP's palettes, tiles,
  maps and names (the Western discs their own names); these are the PSP
  releases' changes, laid over the PC's data.
- **Reversible?** Unset `BOF3X_OPT` (or `BOF3X_OPT=original`), or empty the
  ini's `opt=`; `BOF3X_ORIGINAL=LoadDatFile` runs Capcom's loader, which
  walks neither overlay.

### The cache's songs synthesised from the disc's sequence, the MP3 the fallback

- **ID:** DIV-0087
- **Date:** 2026-10-08
- **Subsystem:** music (`Music_LoadFile` `0x587A20`, `Music_Start`
  `0x5A6CC0`, `Music_Decode` `0x5A6F30`, `Music_SetVolume` `0x5A6FB0`,
  `Music_Stop` `0x5A7050`, `Music_Release` `0x5A70A0`, `Music_Pump`
  `0x5A7230`, ours in `src/game/sound.cpp`; the seam's state in
  `src/game/music_seq.cpp`; the player in `src/audio/`; the launcher's
  `cache=` / `music=`, `src/launcher/config.cpp`)
- **Tier:** Sensible - the PlayStation's own music where the player's cache
  holds it, the MP3 everywhere else; nothing changes without a cache.
- **Original behaviour:** the port plays a 128 kbit/s MP3 of a 2001 render of
  each song (`BGM\NNN.DAT` looping, `BGM\NNNN.DAT` once), looped file end to
  file start (the measured loops moved that inside the file for 48 tracks,
  `music_loops.h`), streamed through a two-half DirectSound ring; the fades
  set the buffer's volume, `volume / 127 * 10000 - 10000` hundredths of a
  decibel, linear in decibels. The MP3s roll off above about 11 kHz
  ([`bgm-comparison.md`](bgm-comparison.md) 8.2) and 80 of the 156 looping
  ones hold less than one loop body.
- **New behaviour:** with `BOF3X_CACHE=<dir>` (or the ini's `cache=`)
  naming the importer's cache, `Music_LoadFile(N)` reads
  `<dir>\base\bgm\NNN.DAT` when it is there ([`seq-format.md`](seq-format.md))
  in place of the MP3. `Music_FileLoops` is then the song's loop flag, and the
  bank `base\bgm\bank\NAME.DAT` is read once and kept while the songs that
  follow share it, as the PSX keeps the SEP's VAB resident. `Music_Start`
  opens no MP3 decoder for it and starts `psx::MusicSynth`, a model of the
  SPU and of Sony's libsnd 3.7 sequencer as the PSX boot EXE runs them
  ([`libsnd-reading.md`](libsnd-reading.md)). `Music_Decode` renders the
  synth into the ring, with the loop the sequence's own markers and the
  reverb the game's. When a once-only song has ended (its end of track has
  keyed off and every voice has released) the rest of the call is zeros and
  `Music_Finished` is set, as at an MP3's end. The ring, its notifications,
  the pump, the fades' state machine and DIV-0028 are the MP3 path's,
  unchanged. A track the cache lacks plays its MP3 whatever the switch, so a
  PC-only install is unchanged.
  **The volume:** the synth plays at sequence volume 127 (`Play(song, 1, 1)`,
  whose one-frame ramp to 1 does nothing, then `SsSepSetVol(127, 127)`).
  The game's 0..127 sets the DirectSound buffer's volume instead, by libsnd's
  sequence-volume law rather than the PC's. The volume is truncated toward
  zero, below 1 taken as 1 as `_SsVmSetSeqVol` does, and gives amplitude
  `(v / 127)^2`, the square law of [`libsnd-reading.md`](libsnd-reading.md)
  3.6: `round(4000 * log10(v / 127))` hundredths of a decibel (1 -> -84.15 dB,
  63 -> -12.18 dB, 127 -> 0). So a fade is linear in libsnd volume units and
  quadratic in amplitude, the PSX crescendo's and decrescendo's shape
  (section 5.1), where the MP3's is linear in decibels. The synth's own
  volume is not what the fades move: the ring renders 0.4 to 0.8 s ahead of
  the play cursor, so a volume given to the synth would be heard that much
  late and in 0.42 s steps. A song would start with about 0.84 s at
  volume 1, and a stopping fade's `Music_Stop` would cut the ring before the
  quieter halves it had rendered were played. The one thing the law loses is
  libsnd's integer rounding of each voice's volume register at each step.
  **Mono:** the options screen's Sound row (`0x903A59`, Stereo 0 / Mono 1;
  `ConfigScreen_Rows` `0x461070` flips it), which the PC's driver never
  reads, is passed to the synth's `SsSetMono` before each render, as the
  PSX's options screen calls `SsSetMono` / `SsSetStereo`.
  The switch: `BOF3X_MUSIC` (or the ini's `music=`) unset or `seq`, the
  cache's song where it has one; `mp3`, the PC's file always; anything else
  is fatal. `BOF3X_CACHE` naming something that is not a directory, or a root
  longer than 42 characters, is fatal at start-up. The 42 comes from
  `File_Open`'s retry: it builds `File_CdRoot` plus the path in a 0x50-byte
  buffer with no length check, kept from the original, and every cache file
  is read through it. A directory without `base\bgm` gives one log line and
  no cache. Armed after every module's self-test; one log line when armed,
  one per cache song started.
- **Rationale:** the owner's stance of 2026-10-08: the disc's music is the
  default wherever a disc is a source, the PC's MP3s only when nothing else
  is there ([`sequenced-music-plan.md`](sequenced-music-plan.md), head and
  section 5). The fade curve is the plan's recommended "authentic" (section
  6), carried by the buffer for the timing reason above.
- **Known and accepted:** **the level.** The PSX starts a song with
  `SsSepSetCrescendo` to the caller's volume, and the title passes 100 over
  8 frames, which libsnd's step arithmetic ends at **97**
  ([`libsnd-reading.md`](libsnd-reading.md) 5.1). The PC's `Music_Play`
  fades to its own 127 whatever the song. So a cache song plays at sequence
  volume 127 at the end of a fade-in, about 2.3 dB above the PSX title's 97
  by the square law. Which level is the owner's call. A once-only song's
  reverb tail is cut where its voices end, and the ring's last half is lost
  to the pump's stop as for an MP3. `Music_Loops` does not rewind a sequence
  (the loop is the data's). The synth's tick phase is arbitrary (0).
- **Verification:** the self-test under `BOF3X_SHADOW=sound`
  (`music_seq::SelfTest`, after DIV-0028's and the measured loops'), on a
  bank and two songs built in memory by the format, no game data. A looping
  song goes through `Music_LoadFile`, `Music_Start` (no decoder opened) and
  `Music_CreateBuffer`'s first half plus five 0x12000-byte `Music_Decode`
  calls, and is checked sample for sample against one `MusicSynth::Render`
  of the same length. `Music_SetVolume` with a stand-in buffer is checked at
  0, 63.9, 127 and 300 (-8415, -1218, 0, 0). A once-only song on the same
  bank (not read again) must give the same samples up to the piece its end
  falls in, then zeros and `Music_Finished`. A missing track and
  `BOF3X_MUSIC=mp3` must reach the MP3's two names. Host-checked: the two
  synthetic songs load and play (the looping one never ends, the once-only
  one ends at sample 44,032). Compiled
  (`i686-w64-mingw32-g++ -std=c++20 -fsyntax-only -Wall -Wextra`:
  `sound.cpp`, `music_seq.cpp`, `sound_fuzz.cpp`, `inject_all.cpp`,
  `src/audio/*.cpp`, `launcher/config.cpp`). **Not run:** the owner's
  llvm-mingw build, the `sound` and `'*'` self-tests, and a listen;
  against the renders, the synth's fidelity is
  [`sequenced-music-plan.md`](sequenced-music-plan.md) section 2's table.
- **Also in the PSX version?** Yes in substance: this *is* the PSX's player
  on the PSX's data, at the PC's fade level and timing.
- **Reversible?** `BOF3X_MUSIC=mp3` (or `music=mp3`); unset `BOF3X_CACHE`
  (or empty `cache=`); no cache, no change.

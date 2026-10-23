# Divergence ledger

**Status:** IN PROGRESS (opened 2026-09-18; 163 entries, DIV-0001..0163, DIV-0067 withdrawn)

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
- **Also in the PSX version?** No - the port's (read 2026-10-23). The
  PlayStation lists the card's directory with `firstfile` / `nextfile` at its
  card check (US `START.EMI#9` `0x801EFCA8`, called from `SHOP.EMI#0`
  `0x801D6F00` whenever the card status is 0) and, when it builds a save,
  writes the new slot's summary into its slot table and grows the count (US
  `0x801D69E0` -> `0x801D6CE4`), so a new save shows at once. The stale
  `Save_Directory` and the per-slot files are porting-house code. Not traced:
  which state of the save screen runs the card check on each opening.
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
  2026-10-08 - a bare code `en` before, and the overlays named `en.<name>`).
  **Amended 2026-10-10:** a `bof3x.ini` still saying a bare code reads as
  its tag - `en` as `en-US` (the default English, the owner's word of
  2026-10-08), `fr` as `fr-FR`, `de` as `de-DE`, `ja` as `ja-JP` - with a
  note on the launcher's stderr, and the next save writes the tag
  (`ConfigLegacyLanguage`, `src/launcher/config.cpp`); until then such an
  ini fell silently to the original. Only the ini is mapped: `BOF3X_LANG`
  in the environment is taken as given. **Amended again 2026-10-10 (the
  owner's word: sunset, not map):** the bare codes are retired and refused
  wherever a language is an input. `language=en` in `bof3x.ini` or
  `BOF3X_LANG=en` in the environment stops the launcher before its dialog
  (message box and stderr, exit 1: `bof3x.ini's language=en is retired;
  use en-US (en-150 for the European English) (DIV-0005)`), and the DLL
  stops at injection on a `BOF3X_LANG` that reaches it (Fatal, `DIV-0005:
  BOF3X_LANG=en is retired; use en-US (en-150 for the European English)`);
  `fr`, `de`, `ja` name `fr-FR`, `de-DE`, `ja-JP`. One table,
  `RetiredLanguageReplacement` in `src/game/language_tags.h`, serves both;
  `tools/language_tags.py` mirrors it, and `input_run.py`, `attract_run.py`,
  `importer.py` and `loc_build.py` refuse the codes as `--lang`, and the
  runners a launcher ini or environment saying one, before they launch.
  Overlays named `en.*.DAT` (and `fr.`, `de.`, `ja.`) are read by nothing;
  the importer's `install` and `verify` and `loc_build.py`'s title menu
  only note them, to delete. `ConfigLegacyLanguage` is gone. An overlay
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
- **Amended 2026-10-17 (the button unification round, DIV-0140):** in the
  nine button areas a layer's text carries a button code before each button
  mention, and the dialogue draws the button the Config has put that role on.
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
- **Also in the PSX version?** No - the port's (read 2026-10-23). The
  PlayStation's product is libgte `MulMatrix2` (JP `0x80179630`, US
  `0x80178CB8`, in place), and its last store is `swc2 $11, 0x10($a1)`: a word
  of GTE register IR3, whose upper half is element [2][2]'s sign extension. So
  bytes 18..19 are `0000`, or `FFFF` when m[2][2] < 0 - deterministic, never
  stale stack. Ours agrees whenever m[2][2] >= 0. (The libgte guess above,
  nine halfwords and the padding left alone, is not what this binary does.)
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
- **Also in the PSX version?** Yes, in the source (read 2026-10-23). The
  twins (US `0x80156000` for `MapCell_DrawQuads`, `0x8015707C`, `0x80157358`;
  JP `0x8015B394` for `Sprite_ProjectA`) write vx, vy and vz and never the
  pad: JP `0x80156ABC` stores `0x10`, `0x12`, `0x14($sp)` and loads the vector
  with `lwc2 $0` / `lwc2 $1` with `0x16($sp)` unwritten. On the hardware the
  GTE's VZ register is 16 bits wide and drops the pad, so only the PC's
  dword-wide register image receives it. `MapCell_DrawFrames` and
  `Mode11_ObjectDraw` not read.
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
- **Also in the PSX version?** Yes - Capcom's (read 2026-10-23). JP
  `0x801A459C` / `0x801A47DC` (US `0x801A3594` / `0x801A37D4`, GAME.EMI#0)
  index two adjacent two-entry tables (JP `0x801C942C`, `0x801C9434`) by `+4`
  with no bound, and their cases set `+4` only to 0 and 1, as the PC's do.
  Past the tables the PlayStation reaches a data word (`0x12131011`), not
  another function's cases.
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
  hand tips at about 246 and 277 units); its code is unread. Read 2026-10-23:
  the chooser's hand sits one row lower on the US disc under the dragon
  panel's question - `0xB6 + 12 * row` at US `0x801DC224` against `0xAA` on
  the JP (`0x801DC2A4`) and the PC (`0x529FA0`), the US question being one
  row longer; the row-counting flag gives `0xB6` for a two-row question.
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
- **Amended 2026-10-10 (the frame clock):** the step keyed on the float
  frame deadline at `0x6BC628` moving, and DIV-0047's WinMain (2026-09-24)
  deliberately never writes that float - so from that day every fade took
  its first step and then none: a fade in stalled at one step and **a
  stopping fade never stopped the music** (the owner's reports, D241: the
  title music through the load screen, the battle music after a fight; the
  owner's route `tools/recipes/musicStopCheck.txt` under `BOF3X_MUSIC_LOG=1`
  showed `Music_FadeOutStop` at frame 483 and no `Music_Stop` until the next
  `Music_Play` at frame 913, the same with the MP3s and with the cache's
  music, while Capcom's code stopped the title music on time to the owner's
  ear - `analysis/shots/musicstop_1010/`). The step now keys on
  `Frame_Counter` `0x937F94`, which both WinMains increment once at the end
  of every pass through the frame path (the original at `0x4FCFE5`), so the
  behaviour above is what runs again; the self-test drives `Frame_Counter`
  instead of the float. `BOF3X_MUSIC_LOG=1` stays as a diagnostic (one line
  per music command with its caller and frame; no behaviour).
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
- **Also in the PSX version?** Yes, the same shape (read 2026-10-23 against
  the US twin `0x801DBD24`, SHOP.EMI#0, every masked word of the JP
  `0x801DBCBC` identical): the PlayStation picks its CLUT from a four-entry
  array on its own stack by the unbounded kind byte, and its tile pattern's
  start from a four-entry table (`0x801E5B1C[kind]`), so a kind of 4 or more
  reads its stack frame there too; what that drew on a console was not
  measured.
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
  **Amended 2026-10-11 (state 3, DIV-0094):** `bof3x.exe` declares itself
  DPI-aware (per-monitor v2) - `BOF3.exe` never did, so on a scaled desktop
  Windows stretched its whole window after the picture was drawn and blurred
  it. Aware, the window gets real pixels, and the default k when `scale=` is
  unset follows the display: twice the scale rounded down, never below 2 -
  100 % and 125 % open at k = 2, 150 % and 175 % at k = 3, 200 % at k = 4 -
  so the window opens the same apparent size as before and the player
  resizes from there (the owner's rule, 2026-10-11). The hosted build is
  told 96 dpi and keeps k = 2. `DisplaySetup_WindowedScale`.
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

### A 426 x 240 or 560 x 240 picture: the view widened by 53 or 120 columns a side

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
  spawner found). **Widened 2026-10-15**: the owner's `radiationRoom.txt`
  reaches it - area 118's purple pulse, which stopped at the 4:3 edges
  (found by `BOF3X_DRAWORDER=500-501:20,20,22,22`: a code-`0x3A` POLY_G4
  `(0, 0)-(320, 320)` committed from `EffectKind96_Pulse`). Its left
  corners now at `Widescreen_FillX()`, its right at
  `Widescreen_FillRight(320.0f)`, as `EffectKind18Sub36_Pulse`; the y
  corners Capcom's. `effect_4d`'s self-test 0 mismatches narrow and at 21:9
  (the fill unarmed during it); seen whole at 16:9 and 21:9 in the recipe's
  frame 540. **The same day, a scan of ours for the rest** (every `320.0f` /
  `0x43A00000` and integer `0x140` in `src/game` not routed through a
  `Widescreen_*` helper): one more full-frame quad, `effect_3d.cpp`'s
  `FullScreen` - the red POLY_G4 `(0, 0)..(320, 320)` of effect kinds 0x76,
  0x79, 0x7A, 0x7B and 0x7C (`EffectKind76_Shade`, `EffectKind79_Shade`,
  `EffectKind7A_Shade`, the pulses of 0x7B / 0x7C) - widened by the same
  rule; `effect_3d`'s self-test 0 mismatches narrow and at 21:9. No recorded
  route reaches those kinds (not seen). The scan's other hits are widened
  already, narrow-only paths (sub-kind 0x15's `!wide` strips), window and list
  positions, or culls.
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
  `BOF3X_WIDE_TERRAIN=0..400` (an experiment's switch, 2026-10-07) sets the
  terrain cull's margin past the original's `[-50, 370]` under a wide
  picture: unset it is 100, the `[-150, 470]` above; 0 is the original's
  cull. Narrow it is not read.

  **Amended 2026-10-12 (the PSP-only round, group WA,
  [`psp-widescreen-a.md`](psp-widescreen-a.md); the owner's answer to
  [`psp-widescreen-placements.md`](psp-widescreen-placements.md) 6.1: option
  (a), the PSP release's per-element choices taken at our 53 columns, every
  value moved by our 53 as the PSP's moved by its 32; narrow, every packet is
  the original's bit for bit, each function's fuzz comparing it):**
  - **The full-frame fills the PSP widened.** Fifteen more reach the wide
    frame's edges: the five spell flashes drawn to x 319 that the `320.0`
    scan could not see (`Magic008_DrawFlash`, `Magic111Wash_Draw`,
    `Magic111Flash_Draw`, `Magic225Veil_DrawShade`,
    `SpellRagnarok_DrawScreenTint`) and five more found by pairing the PSP's
    unpaired fills (`ChillFlash_Draw`, `KaiserFlash_Draw`, `CoronaFlash_Draw`,
    `TimedBlow_DrawFlash`, `TempestFlash_Draw`), each `0..319` becoming
    `-53..372` through `Widescreen_FillRight(319.0f)` (the PSP's `-32..351`);
    `EffectKind44_RingDraw` (its side runs and two tiles); `BattleWin_DimScreen`
    (x from `Widescreen_FillX()`); `CommuDraw_Reveal` (its gradient band and
    its black tile - the PSP widened the tile only; the owner kept both
    widened: "I assume some psp draws will leave 16 row bands since we are
    not cropping the same"); `EffectKindA8_DrawBar` (the bars `-53..373`, the
    PSP's `-32..352`) - **this ends the hold of 2026-10-04 above**, at the
    owner's word ("If psp widened the a8 bar, let's widen it too"); and area
    189's sky (`EffectKind18Sub44_Draw`: the textured band continued by a quad
    each side at the middle's texel rate rather than stretched as the PSP did,
    the white tile `(-53, 50)` 426 wide, a fourth sky quad so the left band is
    covered at every camera angle, a flat haze quad each side). Every `319.0f`
    / `320.0f` operand in `.text` now lies in a function of ours. **The rule
    for later widening (the owner, 2026-10-12):** the PSP crops 12 rows top and
    bottom and we do not, so where a PSP widening relied on that crop to hide
    an edge, ours widens over the full height.
  - **The battle window's full-frame tile.** Entry 11 of the size table
    `0x64E268` (321 x 241), the tile the bosses' death flashes and rest_3a's
    flashes draw at x 0, which the PSP widened to 385 with its callers at -32:
    applied where the table is read (`BattleWin_DrawTile`, `_DrawTileRgb`,
    `_DrawTileTint`, via `Widescreen_BattleTile`), size 11 only, the table
    untouched.
  - **The mote cull.** `Fx105_MoteDraw`'s x bounds `[-64, 384]` become
    `[-117, 437]` in the wide picture (the PSP's `[-96, 416]` for 32); y as it
    was.
  - **Edge anchors** (`Widescreen_Anchor()`): elements the PSP moved 32 toward
    an edge move 53 - the charge gauge of areas 104 / 121
    (`Area104_DrawGauge`'s bars, `Area121_GaugeSprite`'s frame), area 104's
    countdown panel, its triangles and the leader's ring (the countdown's box
    and digits stay centred, as on the PSP), area 172's panel (x only), and
    **the message boxes' side placements** - codes 3 and 5 left, 4 and 6
    right, in kind 0's tables `kBoxXY` `0x66ADD8` / `kBoxAltXY` `0x66ADE8` and
    kind 2's `MsgBox_PlacementTable` `0x66AE10`, moved where
    `window_task.cpp` reads them, never in the tables. This **reverses the
    owner's "the dialogue boxes seem ok to me" of 2026-09-23**, at the
    owner's word of 2026-10-12 (message boxes and the HUD anchored). The
    field menu's time and money boxes are unchanged (the owner's open call).
  - **Decided, not built: the ending's letterbox stays.** `Scena17_DrawLetterbox`
    keeps its 320-wide bars; the PSP's crop of the letterboxed picture is not
    taken (the owner, 2026-10-12).
  - **Verification:** each function's start-up fuzz against the original
    (narrow bit for bit); the hosted `'*'` narrow and with `BOF3X_WIDE=1`
    (1,073 self-test lines, 0 mismatches each); the exe host on a PSX
    disc-only install narrow and wide, exit 0; **not seen wide** - the scenes for the screenshot harness are
    `psp-widescreen-a.md` 6. **Reversible:** unset `BOF3X_WIDE`;
    `BOF3X_ORIGINAL=<function>` for any function above brings back Capcom's
    narrow-sized draw.

  **Amended 2026-10-13 (the ultrawide round, group UW,
  [`ultrawide-frame.md`](ultrawide-frame.md)): the wide picture comes in two
  widths, 426 and 560.** `BOF3X_WIDE=1` is the 16:9 picture as before, 426 x
  240, 53 columns a side; **`BOF3X_WIDE=2` is the 21:9 picture, 560 x 240,
  120 columns a side** (240 x 21 / 9 exactly; the owner's choice of
  2026-10-13: a fixed preset rather than the monitor's width or 32:9). This
  replaces [`widescreen.md`](widescreen.md) 1's "a 21:9 monitor stays
  pillarboxed". Everything this entry widens follows the columns: the shift,
  the target, the fills, anchors and culls of ours, the battle tile. The
  values that had 53 folded in are rules in the columns (`widescreen_rule`):
  the terrain cull's margin `c + 47` (100 at 53 as tuned by eye, **167 at
  120, owed the owner's eye**), the cell inset lowered by `ceil(c / 20)` (3,
  and 6 - the original's largest inset, so at 21:9 the whole 28-column ring
  is walked; [`ultrawide-culls.md`](ultrawide-culls.md) 3 measured it to
  cover 560 at the field camera but not past `Camera_Distance` ~2,200), the
  menu backdrop's `ceil(c / 64)` extra pairs (1, 2), the area-map frame
  ranges moved by `c - 1`, the fourteen slide bounds by `c` (drawn only since
  the faster slide below). Manillo's backdrop draws the original's two halves
  and a band quad each side from 65 columns on, since the grown halves would
  pass u 255. The launcher's "Widescreen" box is a three-way choice
  (`wide=0|1|2`), and the run tools take `--wide 0|1|2`. On the owner's 3440
  x 1440 the 21:9 picture lands on k = 6, 3360 x 1440 with 40 px of black
  each side. **Verification:** a start-up property check proves every rule
  gives the 16:9 build's constant at 53 (`Widescreen_CheckRules`, and
  `static_assert`s), the backdrop's spans are proved for 0..224 columns,
  `FitScale` at the three widths; the hosted `'*'` self-tests unset, `=1` and
  `=2`. Live in the round's census ([`ultrawide-census-1.md`](ultrawide-census-1.md)
  6): Manillo's backdrop seamless across 560 columns with its middle
  pixel-identical to 16:9's, the left message box whole at the picture's left
  edge (game x -104), no ring notch at the edges on the routes run; areas 31,
  90 and 53, narrower than 560, show black bands. Not played by the owner.
  **Reversible:** `BOF3X_WIDE=1` (16:9) or unset (narrow); the
  `BOF3X_ORIGINAL` switches as above.

  **Amended 2026-10-13 (the ultrawide round, group UC2,
  [`ultrawide-culls-fix.md`](ultrawide-culls-fix.md) section 1): the
  off-screen flag split from the sprite cull.** `Sprite_Draw` `0x5935B0`
  sets bit 7 of an object's byte 0 when it culls it, and nine functions of
  game logic read that bit: `Member_FollowStep` and `Member_CatchUp` (a
  follower off screen may jump or catch up), `Scena08_Scene4` (waits for an
  actor to leave), `BossNina_WalkStep` (walks until off screen),
  `EffectKind32_Arc` / `_Settle`, `EffectKind34_V4Move` and
  `Area65_Record8Move` (effects that hop, land or free themselves off
  screen), and `Fish_Swim` (a sound); `Sprite_FindFree` and
  `Effect_FindFree` test the whole byte for 0 and so read it too
  ([`ultrawide-culls.md`](ultrawide-culls.md) section 2). **The 2026-09-30
  object culls above changed logic, not only the picture.** Moving this cull
  out by the columns meant those decisions were taken at the wide edge under
  `BOF3X_WIDE=1` (`[-117, 437]`, 53 px past Capcom's) from then until this
  amendment, and "a cull's margin changes what is drawn, not logic" did not
  hold for this one. It was never ledgered; the census measured it
  (`combat`'s `Sprite_Objects[2]` byte 0 `0x81` narrow, `0x01` at 21:9;
  [`ultrawide-census-1.md`](ultrawide-census-1.md) 5, known-defects D290).
  *Original behaviour:* one bound `[-64, 384]` for both drawing and the bit.
  *New behaviour:* the object is drawn within `[-64 - c, 384 + c]` (c the
  columns: 0, 53 or 120), and bit 7 is set by `[-64, 384]` at every width. In
  the bands the object is drawn with the bit set, a state the original never
  produces. Every reader decides as Capcom's code does, narrow, 16:9 and 21:9
  alike; at 16:9 that is a return to the game before 2026-09-30. *Why:* the
  owner, 2026-10-13 ([`ultrawide-round.md`](ultrawide-round.md) 4a): widen
  the picture, not the logic. The PSP also kept this cull at `[-64, 384]`
  for its 32 columns ([`psp-widescreen.md`](psp-widescreen.md) 3b).
  *Verification:* `sprite_draw`'s self-test, a second loop at the build's
  columns (16,000 rounds, x biased to the narrow and wide edges, the object
  compared with Capcom's clone): 0 mismatches at 53 and at 120; its control,
  the flag moved with the picture, refused at both widths. Narrow, the loop
  does not run. Live: `combat`'s byte is `0x81` at 21:9 as narrow, and
  `caughFish`, `masterAndManillo` and `shop`'s object pages match narrow
  ([`ultrawide-census-2.md`](ultrawide-census-2.md) 0).

  **Amended 2026-10-13 (the ultrawide round, group UC2,
  [`ultrawide-culls-fix.md`](ultrawide-culls-fix.md) sections 2 and 3):
  Capcom's remaining culls.** UC found six screen-x bounds of ours still
  holding Capcom's values, four of them inside or within 7 px of the 16:9
  view ([`ultrawide-culls.md`](ultrawide-culls.md) 1b). The owner,
  2026-10-13: widen them at both widths, as this entry has done so far, an
  exception to the round's "16:9 bit for bit" taken on purpose. Each was
  read for whether it gates logic first. *Moved by the columns* (bound minus
  / plus `Widescreen_Fill()`, compared in our code; `.rdata`'s floats
  untouched): `MapCell_DrawTexQuads` `0x4CEB40` `(-100, 420)`,
  `MapCell_DrawGroundSprite` `0x4CF270` and `EffectKind18Sub0D_DrawGlow`
  `0x4FF610` `[-60, 380]` (x only; at 53 `(-153, 473)` / `[-113, 433]`, at
  120 `(-220, 540)` / `[-180, 500]`). *`Sprite_ProjectA` `0x57B860`, the
  draw half only:* the off-screen flag and the place on `Sprite_DrawList`
  keep `Sprite_ScreenMargins`' margins (`[-80, 400]` / `[-100, 420]`). A
  sprite culled for x alone but inside the margins moved out by the columns
  is noted on a side list, and `Field_DrawFrame` `0x592F00` appends it just
  before the draw pass, into slots the frame left, then puts back the scale
  dwords `Sprite_Draw` wrote: the original's 40-entry list, every original
  `Sprite_Draw` call and every flag are unchanged. Model sprites (`+0x24`
  bit 6) are not noted. *Left at Capcom's because they gate logic:*
  `EffectKind18Sub1C_OnScreen` `0x505480` `[-20, 340]`, which gates
  `Sound_PlayEffect(0x200)` (it writes `Sound_Channels` and stops the
  channel's sound), and `BreathMote_Fly` `0x4DECA0` `(0, 320)`, which gates
  `BreathMote_DrawRing`, a draw that calls `Rand` (known-defects D293, D294).
  *Original behaviour:* those bounds at every width. *New behaviour:* the
  four above drawn into both bands; the two left pop at Capcom's edges,
  inside both wide views. *Verification:* the three modules' self-tests at
  0 mismatches at every width (the fills are unarmed during them, so they
  compare Capcom's bounds); `field_frame`'s band test at 53 and 120
  (`Sprite_ProjectA` with the band armed against Capcom's clone, the whole
  state compared, 4,000 rounds, 0 mismatches; the append and restore checked
  on a fixed case). Not seen in play.

  **Amended 2026-10-13 (the ultrawide round, group SL,
  [`ultrawide-slides.md`](ultrawide-slides.md)): the faster slide - the
  slide-off bounds split from the picture.** The fourteen slide-off states of
  the menu and shop windows (`0x596000..0x59D000`, `widescreen.cpp`
  `g_slides`) step a window record's x by 0x20 a frame and hold it at a bound
  just past the 4:3 edge (nine) or free the window past it (five). **The
  2026-09-23 slide bounds above changed logic, not only the picture.** Each
  bound moved out by the columns, so in the wide modes a box took longer to
  hold or be freed - 2 frames at 16:9 and 4 at 21:9 for a short slide - and
  the game waits on the window: `dragonTransform` and `cutsceneAndNue` left
  the narrow route by it ([`ultrawide-census-2.md`](ultrawide-census-2.md)
  3.2, known-defects D291). A parked box's x was also read by another
  window's state (`MenuList_SlideLeftBesidePanel` `0x59A070` stops beside
  record 4's x). *Original behaviour:* one bound per state for the hold or
  free and the picture. *New behaviour:* every state decides by Capcom's
  bound at every width (the immediates are no longer patched), so the
  record's x, its state and the free are Capcom's on every frame. In the
  wide modes the box is drawn on a straight line from where its slide
  started to the bound moved out by the columns (`widescreen_rule::SlideBound`,
  unchanged), reaching it on the frame Capcom's x reaches Capcom's bound - a
  faster slide (at 120 columns 1.55 times Capcom's speed for
  `MenuSlide_LeftOff` from 17, 1.95 for `MenuSlide_RightOff` from 194) - and
  a parked box is drawn there. The drawn x is in the record only while the
  record's own handler draws it: `Field_RunTaskRecords` puts Capcom's x back
  after each handler, so every other reader sees Capcom's. Off until
  `Widescreen_ArmFills`, narrow, under `BOF3X_ORIGINAL=Widescreen`, and
  while `Field_RunTaskRecords` is Capcom's. *Why:* the owner, 2026-10-13:
  widen the picture, not the logic; the faster slide over freeing at
  Capcom's bound with the box vanishing in the band. *Verification:* a
  start-up property check (`Widescreen_CheckSlides`): all fourteen states,
  1,278 start x each, at 0, 53 and 120 columns - the hold or free frame and
  every x outside the handler equal narrow's, the drawn x on or past the
  wide bound on that frame (3,380,148 frames). The six modules' fuzzes at 0
  mismatches at `BOF3X_WIDE` unset, 1 and 2; their control
  (`BOF3X_SL_CONTROL=1`, the late free) refused by all six at `=1` and by
  `battle_e7` at `=2`. Live: `dragonTransform` and `cutsceneAndNue` stay on
  the narrow route at 16:9 and 21:9 ([`ultrawide-census-3.md`](ultrawide-census-3.md)
  0). **Owed:** the owner's eye on the faster slide; a box sliding back in
  starts inside the 21:9 band (known-defects D296). The 2026-09-23 note that
  "a takeover of a `kSlides` site must read its bound from the operand" no
  longer holds: the operand is Capcom's at every width.

  **Amended 2026-10-13 (the ultrawide round, group RN,
  [`ultrawide-rand.md`](ultrawide-rand.md)): the map walk's random split.**
  **The 2026-10-07 cell inset above changed logic, not only the picture.**
  Lowering `MapView_Build`'s inset by `ceil(columns / 20)` (3 at 16:9, 6 at
  21:9) makes `DrawLayer_Open` `0x56FD20` hand `MapCell_Handlers` the cells
  of columns the original never walks. `MapCell_DrawRising` `0x570660` draws
  eight squares a cell with one `Rand` each, with no cull. From 2026-10-07
  until this amendment each such cell moved the CRT stream that encounters,
  battle damage and town walkers share by 8 calls a frame, and routes left
  the narrow one: `whelpBoss` at both widths from frame 285, `shop` at 21:9,
  the world map ([`ultrawide-census-2.md`](ultrawide-census-2.md) 3.1;
  known-defects D283, area 2's steam vent). *Original behaviour:* every cell
  the walk reaches calls `Rand`. *New behaviour:* `Inset()` notes the inset
  Capcom's arithmetic gives before the lowering. `DrawLayer_Open` marks each
  cell outside the original's walk of its row as a band cell, and
  `MapCell_DrawRising` takes a band cell's eight numbers from a draw-only
  generator of ours, `Rand`'s recurrence on a seed of its own that nothing
  else reads. The original's cells keep Capcom's `Rand` in the original's
  order, so the band's rising squares still draw and the game's random
  stream is the narrow one. The inset lowering itself is unchanged. *Why:*
  the owner, 2026-10-13: split the random; widen the picture, not the
  logic. *Scope:* `DrawRising` is the only random consumer under the
  handler table (an image scan of all 17 handlers and their callees). The
  area patch handlers (kinds `0x24` / `0x25`) also change game state from a
  band cell and are left as they were, the owner's call ("leave for now";
  known-defects D295, measured in [`ultrawide-patch-cells.md`](ultrawide-patch-cells.md)).
  *Verification:* `wide_rand`'s self-test runs ours alone, with stand-in
  handlers and a recording `Rand`: the boundary at 0 / 53 / 120 columns, over
  every original inset 0..14, input flag, row parity and view column, 0
  mismatches; whole-frame fuzzes at four lowerings show Capcom's `Rand` calls
  equal to the narrow frame's, call for call and cell for cell, 0
  mismatches; the control (band cells on Capcom's `Rand`) refused at 53 and
  120. Live: with the flag split and the faster slide, the eleven hash routes
  keep narrow's `Rand` count on every frame at 16:9 and 21:9 and no game page
  differs ([`ultrawide-census-3.md`](ultrawide-census-3.md) 0,
  [`ultrawide-census-4.md`](ultrawide-census-4.md) 0). **Reversible:**
  `BOF3X_WIDE_INSET=0` (no band: the insets equal, nothing split).

  **The round's re-staging and hide** are entries of their own: DIV-0122
  (the scenes' edge actors), DIV-0123 (case zero) and DIV-0124 (the 21:9
  hide).

  **Amended 2026-10-21 (the wide-ring round, group RG,
  [`wide-ring.md`](wide-ring.md), [`wide-ring-read.md`](wide-ring-read.md);
  IDEAS I37): the field's view ring grown to the PSP's 33 x 58 for the
  draw.** *Original behaviour:* the field's view is a ring of 56 rows by 28
  cells (`MapView_CellItems` `0x937FA0`, `MapView_Cells` `0x904F20`, the
  heads `MapView_Row` / `_Column`, `MapView_Origin`); `MapView_Build` and the
  draw pass walk 55 of its rows and at most 28 columns, so at the wide
  picture a far camera (`Camera_Distance` past about 2,200, kind 0x8D's
  pull) or an off-axis yaw over low ground runs out of cells at the
  picture's edges (known-defects D292). *New behaviour:* when the cell inset
  is lowered (the view wide), Capcom's ring is kept exactly and a rim of
  ours holds the 346 cells the PSP's 58 x 33 ring has beyond it (its origin
  Capcom's plus (-3, +2), the PSP's `Field_ViewReset` `0xE560`), kept by the
  same four shifts, the resets, `MapView_FillCells` and `MapView_PlaceRuns`,
  and read by the draw alone: `MapView_Build` walks grown rows -1..55 and
  grown columns `g .. 32 - g`, `g = max(0, i + 2 - L)` (today's walk plus
  one column); `DrawLayer_Open` walks the PSP's columns of each row round
  Capcom's own walk, which runs unchanged inside it; `Sprite_DrawPass` draws
  layer -1 (a record of ours) and layer 0x37 (Capcom's row 55) - terrain
  and frame nodes, never a sprite; `MapView_ItemAt` and
  `MapCell_DrawFlatFaces` read the rim's draw items. The logic readers of
  the window - `MoveCmd_TestFB` / `FC`, `AreaMap_ClearCell`,
  `Area40_TileLit`, the Quake walk - keep Capcom's 56 x 28 (the PSP grew
  those too; its `TestFB` twin `0xEDC0` tests `s < 0x3A`, `d < 0x42`), and
  `Sprite_UpdateScreen`'s `0..0x36` test keeps setting the off-screen flag
  and the draw list as on the original: a sprite in the grown rows stays
  hidden. DIV-0071's `layering.cpp` sees Capcom's window only. Every cell
  only the grown walk hands to `MapCell_Handlers` - the rim, Capcom's row
  55, the column it adds inside the window - is a band cell of the random
  split, its `MapCell_DrawRising` numbers from the draw-only generator;
  D295's rule for the patch handlers 0x24 / 0x25 is kept. The rim is a
  record of the graphics participant; a quickload across widths rebuilds it
  (a wide take loaded narrow gives the rim's draw items back, a narrow take
  loaded wide fills it afresh) and asks `MapView_Redraw` 2 (both display
  buffers' layer lists), never refuses. *Why:* the owner, 2026-10-15 (build
  I37 now) and 2026-10-21 (the PSP's 33 x 58 at 16:9 and 21:9, Capcom's
  28 x 56 at 4:3 byte for byte; DIV-0041's lowered inset kept; sprites in
  the grown rows hidden; D295's rule kept; the walk today's plus one; rebuild
  across widths, with `MapView_Redraw` 2) - widen the picture, not the
  logic. *Scope:* kind 0x8D's first frames need more than 33 columns, so
  D292 narrows to them (the owner accepts that). *Verification:*
  `BOF3X_SHADOW=map_rim`, a model ring of the PSP's shape against Capcom's
  code plus the rim over 10,040 random steps (shifts, builds, resets, origin
  jumps with `MapView_FillCells`): every grown position's map cell and run
  word equal, the pool's count equal to the items held, 0 mismatches, the
  control (Capcom's `MapView_CellToMap` at the grown row) refused;
  `wide_rand`'s grown boundary at 53 and 120 columns (every original inset,
  the input flag, three heads, layers -1..0x37: band exactly where the
  original's walk leaves a cell out, Capcom's `Rand` calls in the original's
  order) and the grown fuzz (3,000 frames -1..0x37 equal to narrow's call
  for call), 0 mismatches; every touched function's fuzz at narrow 0
  mismatches. Live: at 4:3 the ten hash routes and `dolphinBoss` equal the
  round base tick for tick but for the exe host's tick-3 and sound noise; at
  16:9 and 21:9 every route keeps narrow's `Rand` on every frame and no game
  page differs (`dolphinBoss`'s object pages part as with the ring off -
  DIV-0122 / 0123's staging); the draw-item pool's most is 1,469 of 2,048 at
  21:9; a quicksave loads across 4:3, 16:9 and 21:9 both ways with only
  picture pages differing from the live run. The real far-camera scenes
  crafted saves reach (area 167's init, chapter 1's scene 3, area 149's
  chapter-15 camera; [`wide-ring.md`](wide-ring.md) 8) were already covered
  by Capcom's ring but for one corner pixel at 21:9 in scene 3, which the
  grown ring fills. **Owed:** the owner's play. **Reversible:**
  `BOF3X_WIDE_RING=0` (Capcom's ring alone at the wide picture); narrow
  nothing runs. Code: `src/game/map_rim.{h,cpp}` and the sites
  [`wide-ring.md`](wide-ring.md) 1.2 lists.

  **Amended 2026-10-21 (the wide-ring round, group SW,
  [`wide-sweeps.md`](wide-sweeps.md); IDEAS I38): the two edge sweeps drawn
  mapped.** Two effects sweep across the picture and end a state at a
  screen-x bound ([`ultrawide-culls.md`](ultrawide-culls.md) rows 14 and
  15): effect kind 0xD's slide (`EffectKind0D_*` `0x465F30..0x466050`, in
  from x 390 by 30 a frame, held at 160 for 30 frames, out to -70 where the
  record idles) and area 172's shade (`Area172_EffectA5Grow` `0x428240`, the
  bad ending's wipe that reveals the *Fin* panel; its edge grows by 10 a
  frame and above 400 the scenario's step goes on). In the wide modes they
  started or vanished inside the bands; moving their bounds would change the
  idle frame and the script's timing. *Original behaviour:* each drawn at
  its logic x. *New behaviour:* the logic x, the idle frame and the step are
  Capcom's; only the drawn x is mapped, by `c` columns a side - the slide
  `[160, 390]` onto `[160, 390 + c]` and `[-70, 160]` onto `[-70 - c, 160]`,
  the shade `[0, 220]` onto `[-c, 220 + c]` and `x + c` from 220 on
  (anchored right with its panel, so the reveal is on the original's
  frames). The slide's mapped float sits in the record's `+0x74` only for
  its draw in `Sprite_DrawOverlays` (Capcom's put back after); its `+0x2E`
  and the off-screen bit are never touched. Off until
  `Widescreen_ArmFills`, narrow, under `BOF3X_ORIGINAL=Widescreen`, and for
  the slide while `Sprite_DrawOverlays` or `Field_DrawFrame` is Capcom's.
  *Why:* the owner, 2026-10-15: keep the logic's x and draw it mapped, so
  each sweep starts and ends past the wide edge on the original's frames;
  2026-10-21, off the two sheets: the shade's right-anchored map (the *Fin*
  reveal frame-exact) and the banner's faster slide in the bands (1.23x at
  16:9, 1.52x at 21:9, as the SL amendment's menu boxes) both accepted.
  *Verification:* a start-up property check (`Widescreen_CheckSweeps`,
  17,277 checks at 0, 53 and 120 columns); `--selftest` at 0 mismatches
  narrow, 16:9 and 21:9, with the fuzzes of `effect_1a`, `area_w4b` and
  `field_frame` among them; the state hash on the eleven routes at 16:9 and
  21:9 with narrow's `Rand` on every frame and no page outside the picture
  set differing where the slide runs (`caughFish`, `masterAndManillo`: kind
  0xD is the fishing minigame's *READY!* banner); both sweeps captured at
  all three widths (area 172 from a chapter-jump save). **Owed:** the
  owner's play. **Reversible:** `BOF3X_ORIGINAL=Widescreen`.

  **Amended 2026-10-21 (the wide-ring round, side groups PB and PX,
  [`narrow-pillarbox.md`](narrow-pillarbox.md) 8 and 11): three narrow
  areas framed.** *Original behaviour:* the 4:3 picture; in the wide modes
  an area narrower than the picture showed the void past its map's edge in
  the bands (eleven areas by their maps,
  [`ultrawide-culls.md`](ultrawide-culls.md) 5). *New behaviour:* in the
  wide modes **areas 31, 90 and 198** are framed as a narrower picture on
  purpose: black bars drawn by us over the bands. The three are the owner's
  choice, not a value of Capcom's: the areas with real void past the frame.
  The width each is framed at is measured: the widest of the port's
  pictures (320, 426, 560) that the area's widest screen row reaches, never
  below 320, the row in ring columns of 32 px from the map block the game
  loaded (`AreaMap_Header`), once per area entry - on the shipped data 31
  and 198 at 320 (16:9 and 21:9) and 90 at 426 (21:9 only). The other eight
  areas the measurement names (149, 186, 98 and the camps 53, 153, 154, 192,
  194) are not framed: at their own cameras they draw sky, wall, floor and
  the clearing's rim past the frame, which the crafted-save captures showed
  ([`narrow-pillarbox.md`](narrow-pillarbox.md) 10). The bars are appended
  to the frame at the present, after the frame hash (`render_shim.cpp`
  `Present`, `pillarbox.cpp` `Cover`); they change with the area number,
  which `Area_Enter` sets while the picture is black. Off in a battle
  (`Game_Mode` 5), narrow, before `Widescreen_ArmFills`, until the area word
  has changed since task slot 0 was last free (the title menu runs with the
  demo's last area), and under `BOF3X_PILLARBOX=0`. A menu (the shop, BATE's
  screens) opened in a framed area is framed too. Edge-anchored elements
  follow the frame in: `Widescreen_Anchor` gives the visible columns when
  fewer than the fill's, so DIV-0041's side message boxes stand at the
  framed edge. Draw-only otherwise: the view, the inset, the culls, the ring
  and `Rand` are the wide mode's. *Why:* the owner, 2026-10-15
  ([`owner-review.md`](owner-review.md), "a per-scene pillarbox");
  2026-10-21 widened to every area narrower than the picture, then, on the
  crafted-save sheets the same day, cut to the three with void past the
  frame ("every other area renders fine without it"); menus in a framed
  area stay framed. *Verification:* the rule's self-test
  (`BOF3X_SHADOW=pillarbox`, 82 rounds: the widest row on test blocks, the
  frame at 0, 53 and 120 columns, the list over every area word, the rule by
  area; two controls refused - every area by its measurement, and "the
  narrowest picture that holds the area"); `Rand` equal to narrow on every
  frame of the ten hash routes at 16:9 and 21:9; no page changed by the bars
  (`BOF3X_PILLARBOX=0` against on, the two routes that enter areas 31 and
  90); captures of 31, 90 and 198 framed and 149, 154 and 186 unframed at
  16:9 and 21:9 (PX). **Owed:** the owner's play of the three framed areas
  (the three-area build not yet seen). **Reversible:** `BOF3X_PILLARBOX=0`.

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
- **Amended 2026-10-17 (the button unification round, DIV-0133):** the icon
  column is the first reader of `σ`, the one table every icon and every
  hard-coded test now asks; words 7 / 8 follow the Speak and Action lines.
- **Amended 2026-10-21 (the owner): L1 and R1 are the disc's own small
  labels.**
  - *Was:* L1 and R1 composed of the dialogue capital and the 8 x 8 set's
    serifed 1 in a 12 x 12 cell, doubled.
  - *Now:* the font atlas's top strip packs 8 x 8 cells, "L1 L2 R1 R2 START
    SELECT" on its third row (y 16..23); `loc_build.py` takes L1 (x 0) and R1
    (x 16) whole and triples each to the 24 x 24 glyph (`SHOULDER_CELLS`,
    `shoulder_glyphs`), body nibbles remapped as the shapes' are. The same
    64 nibbles of each on all seven discs (PSX US, EU, FR, DE, JP; PSP EU,
    JP; hashed 2026-10-21). The glyph numbers (`0xA69` / `0xA6A`) are
    unchanged, so the Config panel, the item lists' corner (DIV-0138) and
    the dialogue's button mentions (DIV-0140) all draw them. The `ja-JP`
    layer gets the same two glyphs at `JA_SHOULDERS_AT` (`0xD05` / `0xD06`,
    after its kanji sheet), so every layer but the PC's Chinese draws one
    L1 / R1.
  - *Why:* the owner, 2026-10-21, off renders of every disc's atlas: unify
    the non-Chinese builds on the disc's own small L1 / R1. An 8 x 8 cell
    scales to 24 x 24 by a whole factor, so no pixel is invented.
  - *Verification:* the two glyphs rendered from a disc-only Latin table
    (US disc) and a `ja-JP` table (JP disc) built by the amended
    `loc_build.py`; not yet seen in game.
  - *And the Japanese panel (the owner, 2026-10-21):* under `ja-JP` the
    panel keeps the original's two cells, but its glyph cell drew the
    stand-in behind `0x66A2FC`, which no Japanese layer writes - an empty
    string, so no button showed. It now draws the Japan disc's own icon in
    the same colour at the same place: circle, cross, triangle, square the
    sheet's `0x15 08..0B` (`0x9F6..0x9F9`), L1 / R1 the small labels at
    `0xD05` / `0xD06` (`ConfigJapaneseIcons`, armed after every self-test;
    the draw stops loudly if the font is too short). `BOF3X_SHADOW=rest_4f`
    (Capcom's cell, before the switch), 0 mismatches; not yet seen on a
    Japanese install.
- **Reversible?** `BOF3X_LANG=original`, or `BOF3X_ORIGINAL=ConfigController`
  (the patches) and `Config_DrawControllerCell`; the Japanese panel's icons
  `BOF3X_ORIGINAL=ConfigJapaneseIcons`.

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
  look the same; storing two glyphs in one code is ours. The PlayStation's
  own character counter (JP `0x801B020C`, US `0x801AF20C`; read 2026-10-23)
  treats codes 0x12 and 0x13 as two-byte and stops at 8 characters on the JP
  disc, 12 on the US; the PC's (`0x57D800`) treats any byte with bit 7 as
  two-byte and stops at 16 - each release's encoding, not a port change.
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
  **Extended 2026-10-10 - three more groups, from the owner's
  `fairyVillage.txt`, `dragonMenu.txt` and `fairyVillageNews.txt`**
  ([`village-text-scan.md`](village-text-scan.md), which also lists every
  Chinese string the exe still draws and what each needs):
  12. **the gene splicing window's tabs** 资料 / 选择 / 最佳化 - 3 x 8 at
     `0x66A14C` behind the pointer table `0x66A164` (`GeneWin_DrawChoices`
     `0x598E90`, its one reader): `BATTLE.EMI`'s `Data` / `Pick` / `Best`,
     6-byte slots between the `AP` / `1` / `0` bytes the PC has at
     `0x66AF38` and the pieces and `DATA` / `BEST` it has at `0x66AF4E`.
     Written in place; the draw's count is `0x10`.
  13. **the faerie village's board lists** - the ten facility names
     `0x669E18..` behind `0x669E68` (`CommuBoard_DrawListBox`) and the ten
     choices `0x669E90..` behind `0x669EE0` (`CommuBoard_DrawListBoxB`);
     nothing but the two tables reaches them (`tools/text_scan.py`), so
     repointed into 16-byte buffers as groups 3, 4 and 7 are: `COMMU01.EMI`
     has the twenty as 8-byte slots right before the list rows and record
     offsets the PC has at `0x652C6C`, and `Merchant`, `Explorer`,
     `Antiques` and `Handyman` fill theirs with no NUL. The two draws pass
     `Text_DrawAt` the count 3 - the Chinese lines' three glyphs - which
     would cut `Merchant` to `Mer`: once the group is written they pass
     `0xFF` (`Labels_Written`), the strings being NUL-ended.
  14. **the village's words**, five strings the DLL's own draws read through
     `Labels_Slot` from buffers of ours (the shipped slots untouched): the
     board panel's label 文化 `0x669E10` (`Culture`, the list's own word;
     count 2 -> `0xFF`, and the number after it from `0x98`, under the seven
     letters, to `0xBE`, where the owner's web reference of the US screen
     has it); the one glyph 名 `0x669F08` that the ranked lists' headings
     draw after a count - the US disc's pair `faeries` / `faery`
     (`COMMU05.EMI`, two copies of the pair of 8-byte slots right before the
     kind table the PC has at `0x653180`), the heading drawing the plural,
     or the singular for a count of one (inferred from the pair; the US
     routine was not read), a letter's gap after the number; and the hi-lo
     game's money and stake titles 所持金 / 赌金 `0x669F60` / `0x669F68`
     (`Commu_DrawZennyBox` / `_DrawStakeBox`, count 3 -> `0xFF`):
     `COMMU02.EMI`'s `Cash` and `Pot`, each ending in the disc's zenny code
     `0x60`, right after the 72 bytes of sprite rectangles the PC has at
     `0x652D04`. **The headings' place:** `Population change` from the
     shipped `x + 0x58` ran under the page counter the 8 px draw puts at
     `0xEE`; the shipped heading is four glyphs, so under a written group
     the heading is centred on its middle, `x + 0x70` (`TextAdvance_Width`),
     within a few pixels of the US screen.
     **The count's column** (the French and German check, 2026-10-10): the
     count sits at `x + 0x6C`, sized for the five-glyph Chinese row label,
     and the French `Taux de natalité` ran under it, the German
     `Geburtenrate` touched it; under a written group the column moves right
     of the label's pen width by a letter when it would collide (the
     owner's word), the count's word following. Whether the French
     PlayStation overlaps the same way was not read.
  *Verification:* `loc_build.py all` on the US disc reports `gene tabs 3,
  village lists 20, village words 5`; `rest_4b`, `rest_4c` and `rest_4e`
  self-tests 0 mismatches (the fuzz runs before any overlay loads, so every
  changed draw is the original there); the three routes replayed
  (`analysis/shots/dragonmenu_loc2`, `fairy_loc3`, `news_loc3`): the tabs,
  the lists, `Culture 0`, `Population change` with `1/2` clear of it and
  `Birth 3 faeries`. **Owed the owner's eye:** the hi-lo game's boxes (no
  route), the French and German words.
  **Extended 2026-10-10 again, at the owner's word** (the list boxes, the
  Identify panel, the faeries' names):
  - **The list boxes' width** (group 13): the shipped box is `0x30` wide,
    three Chinese glyphs and a margin, its frame four 8-px columns, and
    `Merchant` / `Handyman` ran past its edge. The discs sized it per
    language - the US and German `COMMU01.EMI` `0x50`, the French `0x60`
    (the two `addiu $a2` constants that vary across the four discs; the JP
    `0x30`): the widest word plus 16. Once the group is written
    `CommuBoard_DrawListBox` / `_DrawListBoxB` use that (`Labels_MaxWidth`,
    rounded up to 8, never under `0x30`) and `CommuBoard_DrawListFrame` a
    column per 8 of it.
  15. **the Identify panel's headings** 弱点 / 持有物 (`0x66A3E0` /
     `0x66A3E8`, `Identify_DrawMember` / `_DrawEnemy`, `magic_s12.cpp`; the
     owner's `identify.txt`): **no disc carries a word for them** - the US
     `MAGIC059.EMI` (the ability's own overlay) and `BATTLE.EMI` hold no
     such string, nor the panel's question marks - **because the US panel
     draws no headings**: the owner's wiki capture of it (2026-10-10) shows
     the name, the EXP and zenny lines, a small `ITEM` label above the two
     items, and nothing where the port's 弱点 / 持有物 stand. So the overlay
     builder sends a space for each (`IDENTIFY_WORDS`), which the two draws
     read through `Labels_Slot` and draw as nothing, and above the items
     they draw the `ITEM` the port still holds beside `EXP` at `0x65AAB4`
     (read by nothing in the port) in the 8 px font, 23 px left of the
     items' column on the heading's row, as the US does (**amended
     2026-10-13 by DIV-0117**, the Identify panel's small words: the discs
     draw a small WEAK POINT heading after all, the item word is each disc's
     own, and the places are the disc routine's); the target's name,
     drawn with the count 5 of five Chinese glyphs, is drawn with 8 (`Fly M`
     -> `Fly Man`). (`Weakness` / `Items`, the builder's own words, stood
     for an hour before the capture.) `Magic080_DrawText` reads the shipped
     slot still (one glyph of each string, an effect).
  16. **the sixty faeries' names.** The port's faeries are the PlayStation's:
     a table of sixty 20-byte trait records at `0x653210` - four stat bytes,
     then a name of 16 - and a birth (`CommuSim_AddRecord`) copies the
     record's four stats and **five bytes of its name** into the save's name
     table, where the US names (five letters at most) fit whole and a
     Chinese one loses its third glyph (the owner's question, 2026-10-10:
     the stats are fixed to the faerie on the PC too; only the renamer's
     suggestion, `CommuName_MakeRandom`, rolls two random halves, and the
     port cut that screen - DIV-0075). `COMMU00.EMI` has the sixty as 9-byte
     records, a 5-byte name padded with the space code and the same four
     stats, found by the stats (all sixty equal to the PC's; the French and
     German discs carry the US list unchanged). Written into the sixteen-byte
     name fields in place; the witness `CommuEntry_DrawPanel`'s read of the
     bars at `0x45E945`. The sixtieth record's PC name field holds no glyph
     string (`01 01 02 03 ..`) and is kept as shipped (`59 of 60`).
     **Saves made before the overlay** keep their Chinese bytes:
     `tools/faerie_names.py` rewrites each in-use entry's five bytes from
     the overlay's list - entry r is trait record r, so the name is exact -
     and recomputes the checksum; run on the owner's `fairyVillage.DAT`,
     `BISLPS00` and `BISLPS0C` (six faeries each: Candy, Green, Lily, Lemon,
     Chirp, Wolke), the other slots holding none.
  *Verification:* `loc_build.py all` on the US disc reports `identify words
  2, faerie names 60`; the log `59 of 60 faerie names`; `magic_s12`,
  `rest_4a`, `rest_4b` and `rest_4e` self-tests 0 mismatches (the first
  caught a changed call order - the heading's character count is taken
  before the enemy record is read, as the original has it); the owner's
  routes replayed (`analysis/shots/fairy_loc4`: `Weapons` / `Items` /
  `Handyman` in a box sized to them; `news_loc4`: `Coo`, `Pan`, `Candy` on
  the board from the renamed save; `identify_loc4`: no headings, `ITEM`
  small above the items, `Fly Man`).
  **Amended 2026-10-18 - group 16's slots, four more groups, group 17 on
  PSP-EU** (the install-gap round, group LA,
  [`install-gap-labels.md`](install-gap-labels.md) 2..5; D289). **The
  owner's confirmation is pending** (labels doc 8, and
  [`owner-review.md`](owner-review.md) "2026-10-18, the install-gap round"):
  this changes the PC install's output under every Latin layer, so it is
  written here as built and stays the owner's to keep or undo.
  *Original:* group 16 above wrote faerie r's name into record r + 1's field
  (`0x653214 + 20 r`): the PC's trait record is the name first, at `0x653200
  + 20 r`, and the stats follow at `+0x10`, so the "sixtieth record with no
  glyph string" was record 59's stats read as a name, and the sixtieth write
  would have landed on the area-limit table `0x6536B0`. Faerie 0 kept the
  shipped name and every other showed its neighbour's, under every Latin
  layer, the PC included (D289). Five more slots held the PC's own words
  under every layer and were zero on a disc-built image where the disc
  re-words them: kind 0x5E's menu title `0x66A098` (Chinese), the equip and
  experience panels' three labels `0x663668` / `70` / `74`, the shared
  list's title `0x664288` and the gene list's title `0x66B1F0` (English).
  PSP-EU had no group 17 (DIV-0118's guest name), its battle code being in
  `BOOT.BIN`. *New:* group 16's sixty slots are `0x653200 + 20 r`, and
  `loc_build.py` sends all sixty disc names (`FAERIE_UNNAMED` is empty,
  checked against the PC by `loc_build.py anchors`; the disc's record r is
  the PC's record r by the stats anchor, all sixty in order). Four new
  groups write in place, each witnessed by the one operand that names its
  slot. Group 20 is kind 0x5E's title, found in `EffectKind5E`'s PSX twin in
  `SCENARIO/SCENA10.EMI` (`parts_title`: the call whose second argument is a
  `"%s"`, cut at the next call's count); `rest_3e.cpp`'s
  `EffectKind5E_DrawMenu` draws it to its NUL once written, at the PC's
  x + 4, where the discs draw it to their own count at x + 8. Group 21 is
  the panels' labels (`START.EMI`), group 22 the shared list's title
  (`SHOP.EMI`) and group 23 the gene list's (`STATUS.EMI`, after the 16
  bytes the PC has at `0x66B1E0`). Group 17 on PSP-EU reads the guest's name
  from `BOOT.BIN`'s own code (the copy of 5, the sound `0x102` two calls
  before it). The Japanese builds build none of these (D280's). *Why:* the
  stage-2 text swap (DIV-0005) for the slots no group reached, and D289's
  fix as [`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4 described
  it. Under a French or German layer the PC now shows that disc's words
  where it showed the PC's English, and every faerie its own name.
  *Verification:* `loc_build.py selftest` (the two new readers on synthetic
  code, a refusal each); `loc_build.py all` over the seven discs (US, EU, FR,
  DE: `panel labels 3`, `list title 1`, `parts title 1`, `gene title 1`,
  `guest name 1`, `faerie names 60`); PSP-EU through `convert_labels_psp`
  (`guest name 1`, `parts title 1`, `faerie names 60`); `importer.py check`
  0 errors; `BOF3X_SHADOW='*,-field_c1'` under `fr-FR` and `de-DE` and `'*'`
  under `original`, no mismatch; the exe host on FR and DE disc-only
  installs logs every new chunk applied. Captured: the shared list's title
  on `campingFishing`'s Skill Notes, FR and DE. Not seen: the panels'
  labels, the gene list's title, kind 0x5E's title (area 128), the guest's
  name and the faerie names (the village route starts a new game on a
  German install). An overlay built before this sends record 59 empty: its
  fifty-nine names then land in their own records and record 59 keeps the
  shipped name.
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
- **Also in the PSX version?** No - the port's (read 2026-10-23). The
  PlayStation's equip window (JP `0x800B025C`, US `0x800B3324`, BATTLE.EMI#15)
  draws the four labels in a loop at `y + 0x1A + 13 k` (`addiu $s0, $s2,
  0x1a`), beside their own values at `y + 0x1C + 13 k`; the PC's `0x27` is the
  port's slip, and ours now does what the PlayStation does.
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
- **Also in the PSX version?** No - the port's (read 2026-10-23). The
  PlayStation's glow is JP `0x801F46EC` in AREA098.EMI#13 (the function before
  the mote's twin `0x801F4BC0`; found by its shape - draw mode `0x2C0` /
  `0x100`, 32 `POLY_G3`s with a black rim). Its packets carry screen x and y
  only, ordered by the OT slot, and every word is written, the rim's from
  locals set before the loop. The stale rim depth exists only in the port's
  depth-carrying vertex format.
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
- **Amended 2026-10-17 (the button unification round, DIV-0136):** the
  banner's label names the button the line's test is on now, through `σ`;
  the owner's word above (IDEAS I29) is met.
- **Amended 2026-10-18 (D286's French and German remainder; the install-gap
  round, group LA, [`install-gap-labels.md`](install-gap-labels.md) 1 and 5):
  the equip panels' four headings from the disc.** *Original:*
  `EffectKind0F_DrawItemsB` / `_DrawItemsA` / `_DrawEquipped` draw the four
  fixed fields at `0x653EA0` / `A4` / `A8` / `AD` with the counts 3, 4, 5, 5
  at `x + 0x35`, `0x2F`, `0x2A`, `0x2A`. That is the PC's English under every
  layer, and nothing on a French or German disc-built image, where the
  fields are zero (D286; DIV-0101's amendment of 2026-10-16 placed them on
  the other builds). *New:* `loc_build.py` `fishing_headings` finds the
  module's four heading references past the tabs, each with the count and
  the row its own draw passes (`addiu $a3` and `$a1` beside the `lui` /
  `addiu` of the address; exactly four, rows 7, 7, 7, `0x47`, in the PC's
  order), and writes each disc's word to its count as chunk kind 16 tag 3:
  US, EU, FR and DE. Not PSP-EU (its code is the PSP's own and its image has
  the PC's words by the `twin` row) nor JP (no fishing chunk for a Japanese
  disc). `fishing_text.cpp` copies the four into buffers of ours; nothing in
  `.data` is re-aimed. Under a Latin overlay `effect_1b.cpp`'s `DrawHeading`
  draws the disc's word whole, centred on the original's centre `x + 6 *
  count` by its real width (`TextAdvance_Width`); for a 12-unit word of
  `count` glyphs that is the original's x. Without the chunk, or not armed,
  the original draw. *Why:* the French and German words exist only at their
  discs' widths, so no byte-equal run can place them; the US module's own x
  puts its words within a unit of the same centre. The words and counts are
  each disc's, the x the PC's centre, not the disc's. *Verification:*
  `loc_build.py selftest` (the reader on synthetic code, with a refusal);
  `loc_build.py all` over the seven discs (`4 headings` on US, EU, FR, DE);
  `BOF3X_SHADOW='*,-field_c1'` hosted under `fr-FR` and `de-DE`, `inject:
  10088 ours`, no mismatch; the exe host on FR and DE disc-only installs logs
  `4 of 4 fishing headings (D286)`; `campingFishing`'s equip screen captured
  on both disc-only installs and on the PC under both layers (harness root
  `C:\Users\kerok\g_la`, labels doc 7). **Owed the owner's eye:** those
  captures; the French tabs still overrun their boxes (unchanged here).
  *Reversible:* play without `BOF3X_LANG`, or with an overlay built before.
- **Amended 2026-10-18 (D279, branch `install-gap/d279`): the Japanese layer
  carries kind 16, and the banner's labels.** *Original:* `loc_build.py`
  built kind 16 for Latin discs only, so a `ja-JP` layer left the thirteen
  lines, the three tabs and the label strings behind `0x66A2FC` as shipped:
  the port's Chinese on the PC, zero on a Japanese disc-built image (D279's
  two paths, D280). *New:* `convert_fishing_ja` reads a Japanese disc's
  fishing module by its own readers, never by the PC's bytes. The line
  records and the label table come from `EffectKind0F_LineNext`'s twin (an
  `lbu` of a text's label byte at `+8`, three times it into the table's
  `lui` / `addiu`, the pen at `+2`; PSX-JP `0x801D3CF0..0x801D3D18`, five
  such draws, one pair of addresses; the records end at the table). The
  tabs come from `EffectKind0F_DrawToggles`' twin (three counted draws of
  one base and two displacements; PSX-JP `0x801D7CF8..`, counts 2, 3, 2 at
  `+0`, `+4`, `+8`, the PC's counts). A Japanese line's record count is its
  characters, a lead byte's code one, not its bytes. The strings are encoded
  for the `ja-JP` font as the dialogue is (`encode_text`: the JP sheet's
  cells, `0xFF` the space `0x20`). Chunk kind 16 tags 1 and 2 as before, and
  a new tag 4: the module's three labels, one character each, by the label
  byte's entry (circle, cross, triangle: the JP icon codes `0x15 0x08..0x0A`,
  their pens 2, 1, 6 the PC's). `fishing_text.cpp` keeps tag 4 in buffers of
  ours, and once `FishingText_Arm` has run, `Label()` (not under a Latin
  layer) draws them in place of `0x66A2FC`'s string. The table itself is not
  re-aimed: the Config cell `0x461C00` reads it too, and that is D280's.
  Entries 3..5 (square, L1, R1, drawn only when sigma moves a button) have
  no module copy and keep `0x66A2FC`'s string (zero on a Japanese disc-built
  image, which DIV-0144 then declines). PSX-JP from `WORLD00/AREA030.EMI`
  section 4 (`AREA089`, `AREA129` the same); PSP-JP from its own UMD's
  `AREA030.EMI`, the Japanese build's overlay, section 4 byte-identical to
  PSX-JP's. Every word read there is checked to be in PSP-JP's `BOOT.BIN`
  data too (it is), so the words are the PSP's own; its `BOOT.BIN` code is
  the PSP's and is not read. The 12-unit layout stays: `Lang_Latin` is false
  under `ja-JP`, and the JP glyphs are 12 units. *Why:* D279's second fix
  (the owner, 2026-10-13: "both"), and the part of D280 the banner needs.
  Under `ja-JP` on the PC the banner, the tabs and the labels change from
  the port's Chinese to the Japanese disc's words. *Verification:*
  `loc_build.py selftest` (the two readers and the character count on
  synthetic code; two label tables and a gap refused); `loc_build.py all`
  over the five PSX discs into a PC copy (`ja-JP`: `13 lines, 3 labels, 3
  tabs, from BIN/WORLD00/AREA030.EMI, records 0x801E1EA0`; the Latin
  discs' `13 lines, 3 tabs, 4 headings` as before); the live runs in
  [`known-defects.md`](known-defects.md) D279. *Reversible:* an overlay
  built before, or `BOF3X_LANG=original`.
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
- **Amended 2026-10-13 (the ultrawide round, group FY,
  [`faerie-over-table.md`](faerie-over-table.md) sections 5-7; corrected
  2026-10-14 by [`campfire-and-crystal.md`](campfire-and-crystal.md)): a model
  keeps its layer.** A model (`+0x24` bit `0x40`, drawn as depth-sorted quad
  records) is never moved, and a sprite whose body overlaps a model of its
  own layer is not moved either: moved, either would leave the depth sort it
  shares with the model's quads. The case was the model at the lighthouse's
  pedestal in `eldritchGene.txt` (object `0x802148`), **the lighthouse
  bulb's filament**: the rule had moved it out of layer 38 and drawn it over
  the globe's semi-transparent facets instead of inside them. (FY's doc
  called it the dragon gene; the gene is the crystal of known-defects D242,
  which this amendment does not touch.) The owner likes the filament's look
  as fixed. Both log under `BOF3X_LAYERING_LOG=1` ("is a model: layer kept",
  "beside model ...: kept"). Verified live, narrow, against the rule before
  the amendment: `field_view.txt` 4 of 4, `worldmap_sliver.txt` 1 of 1,
  `worldMapAndAreaTransition_ab.txt` 35 of 35, `ninaWalkBehindBlock.txt` 11
  of 11 and `dolphinBoss.txt` 60 of 60 shots identical; the four pedestal
  shots differ in the model's box only, which is now the same as with the
  switch off, and at frame 3,300 the whole frame equals Capcom's.
- **Amended 2026-10-14 (the ultrawide round, group CF, the owner's campfire
  report; [`campfire-and-crystal.md`](campfire-and-crystal.md) sections 1-5,
  known-defects D301): a cell record that drew over the body stops the
  move.** A cell record stops the rule when what its run drew in the last
  pass reaches the sprite, as well as when it is anchored within two cells.
  The camp's fire (a `MapCell_DrawFrames` record, the same in the
  cutscene's `AREA053` and the field's `AREA090`) is anchored three cells
  from Garr; the rule drew him three layers later, over the flames, where
  Capcom's order puts the flames over him. Each run's `POLY_FT4` /
  `POLY_G4` quads are kept from each pass, up to eight a run (past that,
  their union box; `DrawLayer_Open` brackets the run, `Gfx_CommitPrim` hands
  over its primitives, mode 1 only; two generations in a 2,048-entry table,
  about 1.2 MB), and tested with the lists' convex test (`QuadReaches`).
  Quads with a point past +/-4,096 and other primitives are left to the
  proximity test; a run not drawn in the last pass counts as in the way.
  The amendment only stops moves, so every pixel it changes is the
  original's order at that spot. The quads are one frame old. Verified live,
  narrow, against the rule before it: `vaccinationAndFireScene.txt` 29 shots,
  each differing in the fire's box only; `campingFishing.txt` 50 / 50,
  `field_view.txt` 4 / 4, `worldmap_sliver.txt` 1 / 1,
  `worldMapAndAreaTransition_ab.txt` 35 / 35, `ninaWalkBehindBlock.txt`
  11 / 11 and `eldritchGene.txt` 18 / 18 identical; `dolphinBoss.txt` 58 /
  60, the other two with plants in front of a character drawn in front again,
  equal in their boxes to the switch off. Headless `'*'` at `BOF3X_WIDE`
  unset, 1 and 2: exit 0, every mismatch count 0 (the amendment arms after
  the self-tests, as DIV-0071 does). The owner has seen it in a replay and
  approved (2026-10-14). `BOF3X_LAYERING_RECORDS=0` drops this amendment
  alone (the control).
- **Amended 2026-10-17 (GitHub issue #79, a player; known-defects D306): a
  run's quads are read when the run ends, not when each is committed.**
  `MapCell_DrawUprights` (Capcom's `0x570210`) commits each pair of leaf
  quads and then writes their outer vertices 0 and 1, so the 10-14 amendment
  read those two points from whatever the packet pool held there before.
  On the Yraall road's bushes the stale points put all six quads on the
  trunk's axis one pass in three (`BOF3X_LAYERING_LOG` and a temporary dump
  of the kept quads, frames 3433..3439): the run "drew nothing over" Rei,
  she was moved in front of the bush, and the next pass put her back - the
  flicker the player and the owner saw. The run now notes its committed
  primitives (up to 64; past that the oldest are read at once, as before) and
  reads them in `RunEnd`, after its last handler. No other change to the
  rule. Verification: `yraallRoad.txt` frames 3300..3720 shot every frame,
  the box round Rei and the bush: 100 of 421 frames flipping before, 0 after,
  and all 421 equal to `BOF3X_LAYERING=0` in that box (the bush in front of
  her, as Capcom draws it).
- **Amended 2026-10-21 (the wide-ring round, group GF, the owner's "Garr
  over fire" report; [`garr-fire.md`](garr-fire.md), known-defects D301):
  the record test takes what the sprite drew.** The 2026-10-14 amendment
  tested a cell record's last-pass quads against the sprite's body box, 28
  px wide. In two camps Garr sits beside the fire, not at it: chapter 6's
  `AREA053` camp (`cs_53.txt`) and `AREA192`'s (`cs_192.txt`, the scene the
  owner saw). That is the same `MapCell_DrawFrames` record as D301, three
  cells from him. His arm and weapon reach over the flames 4 to 26 px
  beyond the body box, so the rule still moved him three layers on, over
  the fire. `Sprite_DrawPass` now notes each sprite's committed cell sprite
  (code `0x84`), and its box (the cells' union, as `D3d_BuildCellTexture`
  lays them out) is kept for the next pass. The record test uses the body
  box grown to it. Nothing else in the rule changes, and it only stops
  moves. The quicksave's graphics record carries the boxes (version 3).
  `BOF3X_LAYERING_SPRITEBOX=0` drops this amendment alone (the control).
  The owner, 2026-10-21: the flames over Garr's arm and weapon, as Capcom's
  order draws them, kept; the wider reach (the sprite's whole drawn cell
  box, transparent corners included, stopping a move) accepted after the
  owner's check on the PlayStation in an emulator - at a camp only Ryu is
  controlled, he can walk behind the fire (the flames then drawn over him)
  but not onto the pit, and the other members spawn at several spots round
  the camp, Garr close enough to overlap the flames; a stopped move only
  ever leaves Capcom's order, which is what the PlayStation draws at the
  fire. Verified live, narrow:
  - `cs_53` 41 shots and `cs_192` 4, pixel-identical to `BOF3X_LAYERING=0`
    and to Capcom's draw pass (`BOF3X_ORIGINAL` naming its five functions);
    `main`'s build drew Garr over the fire in 21 and 3 of them;
  - identical to the rule before it on `vaccinationAndFireScene` 29 / 29,
    `campingFishing` 22 / 22, `dolphinBoss` 19 / 19 (the D300 faerie) and
    60 / 60, `ninaWalkBehindBlock` 11 / 11, `yraallRoad` 56 / 56,
    `worldMapAndAreaTransition_ab` 35 / 35 and `field_view` 4 / 4;
  - `eldritchGene` 58 / 59, the other shot being dialogue typing that
    differs between runs of one build;
  - headless `'*'` at `BOF3X_WIDE` unset, 1 and 2: exit 0, every
    mismatch count 0;
  - the state hash on the ten routes, narrow, against `main`: `Rand` equal
    every frame, no game page; only the draw list's order differs, on
    `worldMapAndAreaTransition` ticks 2,064..2,112, where the picture is
    identical.

  **Owed:** the owner's play of the two camps.
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
- **Also in the PSX version?** No - the port's (read 2026-10-23). The twin is
  JP `0x801F39BC` in AREA167.EMI#13 (the pairs' `0x801F38F0` is the function
  before it; found by the one window on the disc holding `0x4EC0`, `0x3FC0`,
  `0xB00`, `0x95` and `0xA00`). It keeps x0 in a written local (`sw $v0,
  0x30($sp)`) and puts v0 / v2 at `x0 + 0xA00 k - b`, v1 / v3 at `x0 + 0xA00 k
  + b`, as ours now does.
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
- **Also in the PSX version?** Yes - Capcom's (read 2026-10-23). The
  PlayStation twin (JP `0x801D2308`, SHISU.EMI#0) also hands `SetLightMatrix`
  (JP `0x80179980`, five words into the GTE) a stack matrix at `0x78($sp)` of
  which the direction routine (JP `0x8015BC9C`, US `0x8015B2AC`) writes three
  shorts, so rows 2..3 are stale there too; it loads 20 bytes, not 32, and the
  hardware's `NormalColorCol` (JP `0x80179B54`) then reads all three rows.
  Whether the colour matrix cancels the stale rows was not read.
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
- **Also in the PSX version?** No (read 2026-10-23): the PlayStation keeps the
  list in eight bytes, so eight standing enemies never overflow it - US
  `BATTLE.EMI#15` `0x800A955C`, JP `0x800A64BC`, every masked instruction
  word identical. The four-byte list is the port's; the overflow is the
  port's defect, not Capcom's PlayStation one. (The same read found the
  member picker's and the enemy picker's twins cited swapped in
  `rest_4a.cpp`; corrected the same day.)
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
- **Also in the PSX version?** No - the port's (read 2026-10-23). The
  PlayStation's builder (JP `0x801D69F0`, US `0x801D69E0`, SHOP.EMI#0) copies
  the name from record 0 (`strncpy` of 5 from `0x801448D4 + 0x90`, the
  character records' base) and the level from the same record (`lbu 6($s3)`):
  one character, whoever leads, with no leader lookup and no second copy. The
  leader-indexed name is the port's; ours now does what the PlayStation does.
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

### Dauna Mine's minecart map is walled as every later release walled it, from the player's Western disc

- **ID:** DIV-0080
- **Date:** 2026-10-06
- **Rewritten:** 2026-10-10 (the walls moved from a coordinate table in our
  code to an optional layer built from the player's own disc); **changed**
  the same day: on by default wherever the layer is installed (the owner's
  word)
- **Subsystem:** the area data as loaded: the `area4-walls` layer
  (`tools/importer.py`, `recipes/opt.toml`, [`opt-layers.md`](opt-layers.md)
  section 1), walked by DIV-0086's `BOF3X_OPT` prefix in `LoadDatFile`
  (`src/game/dat_load.cpp`), named by default by the launcher
  (`src/launcher/config.h` `kOptDefault`); the cell plane of the area block at
  `AreaMap_Header` `0x8CB580` (PC tag `0xC8000`) and the battle placement
  nibble map at `0x8C3D80` (PC tag `0xC0800`)
- **Tier:** Intent - the owner, 2026-10-06: "if it looks like a bug fix, it's
  probably worth keeping the change as the default option"; and 2026-10-10:
  do it without carrying Capcom's table in our code, if we can
  ([`LICENSING.md`](LICENSING.md) section 3).
- **Original behaviour:** the PC port carries the Japanese disc's map of area
  4 (Dauna Mine's minecart area), in which 72 cells along three lines - the
  raised strip's east edge (its doorway open), the north end of its west
  side, and the corridor's bottom edge - are open floor between wall stubs
  Capcom had already placed on the same lines, and 8 of them are open to
  battle placement ([`region-diff.md`](region-diff.md) 8.1, 8.2 for the
  cells). Every later release - the US, European, French and German PSX discs
  and both PSP discs - walls those cells and the PSX discs close the 8
  placement cells; the PSP took the walls but not the placement half.
- **New behaviour:** with the `area4-walls` layer installed and named in
  `BOF3X_OPT` - which the launcher does by default, below - each load of `AREA004.DAT`
  walks `DAT\area4-walls.AREA004.DAT` after the file: two kind-0 chunks, the
  Western disc's **whole** cell-byte plane (7,920 bytes) and its **whole**
  placement map (5,000 bytes), each landing exactly on JP's. On JP's map that
  changes the 72 cells to the wall value and the 8 nibbles to 0 and nothing
  else (verification). **On by default (2026-10-10):** `tools/importer.py
  build` builds the layer whenever the player's US, European, French or
  German PSX disc is among its sources (any preset or source order), and
  `install` installs it whenever the cache holds it; the launcher names it in
  `BOF3X_OPT` whenever `DAT\area4-walls.*.DAT` is there and the ini's `opt=`
  is empty (the default; [`launcher-settings.md`](launcher-settings.md)). It
  is never shipped or committed. `recipes/opt.toml` records the two chunks'
  tags, sizes and hashes and the hashes of the composed chunks, and no cell's
  place or value. **Turning it off:** `opt=none` in `bof3x.ini` (or an `opt=`
  list that does not name it, or `BOF3X_OPT=none`) plays the shipped map
  with the layer still installed; `importer.py install --no-opt area4-walls`
  removes it from `DAT\`; `build --no-opt area4-walls` does not build it. An
  `opt=` value the launcher cannot honour stops it with a message rather
  than falling back to the default. **No disc, no walls:** a player with
  only the PC install, the JP disc or a PSP disc builds no layer and plays
  the shipped open map, by default and without a choice. The code that wrote
  the walls from a table (`src/game/area4_walls.cpp`, `BOF3X_AREA4_WALLS`) is
  gone; that variable is now refused at start-up (`=0`, the old "shipped
  map", is accepted with a log line when the layer is not named, and refused
  when it is, since it would get walls it asked to be without). The later
  discs' 30-cell re-texture of the strip is still **not** taken: the layer
  carries the cell plane, not the tile words or texture records, so the
  textures stay JP's.
- **Rationale:** a collision fix every later build made. The owner wanted it
  without our code carrying Capcom's coordinates. A rule derived at load time
  from the JP map's own data was looked for first and does not exist
  ([`region-diff.md`](region-diff.md) 10.1): the 72 cells are not
  distinguishable from other open cells by the map's heights or cell bytes.
  So the walls come from the player's own disc, as the unified-data plan
  takes every later build's content change ([`unified-data-plan.md`](unified-data-plan.md)
  7): the engine / data split keeps the bytes the player's.
- **Also in the PSX version?** The JP disc has the open cells; every later
  disc has the walls. This follows the later discs where the player has one.
- **Verification (2026-10-10):** `tools/importer.py opt-recipes` with the JP,
  US, European, French, German and both PSP discs: the layer's two chunks are
  byte-identical on all four Western PSX discs, the PC's chunks are JP's
  sections exactly, and the planes differ from JP's in 78 bytes (the 72 cells,
  and 6 bytes holding the 8 nibbles). `build --opt area4-walls` from the US
  disc and, separately, the German disc: the same layer file (md5 equal);
  `verify`: 2 of 2 chunks and 2 of 2 composed base chunks as recorded.
  `tools/region_read.py fix --layer` lays the built layer over the JP disc's
  sections as `LoadDatFile` lays a kind-0 chunk and compares with each later
  disc: against US, German, French and European, the cell plane and the
  placement map identical; exactly 72 cells changed, every one `0x00` to
  `0x10`, the cells the old table had; 8 nibbles, every one to 0 and
  on a walled cell; the section outside the cell plane untouched (the 920
  bytes still different are the re-texture not taken). `importer.py check`:
  6 layers, 0 errors. Built (llvm-mingw); `BOF3X_SELFTEST_ONLY=1
  BOF3X_SHADOW=field_blocked` from an ini-less copy: exit 0, `inject: 10081
  ours`, the `BOF3X_AREA4_WALLS=0` line logged; with `BOF3X_AREA4_WALLS=1`,
  the refusal (exit 3). **Not run:** a game with the layer installed (the
  loader walk is DIV-0086's, already verified; this layer adds no chunk kind),
  and the owner's walk of the strip's east and bottom edges. The 2026-10-06
  attract A/B (`analysis/statehash/attract_walls_on` / `_off`) still
  describes the layer's effect - the same bytes in memory: four pages
  differ (the cell plane's two, `0x8D3000`, `0x8D4000`; the placement map's
  two, `0x8C3000`, `0x8C4000`) from the first load at tick 1,312, and the
  demo's moves never meet the walls. The state-hash references are recorded
  in a game directory without the layer installed, so they are unchanged;
  in one with it installed, the default run is the old "on" run.
  **The default (2026-10-10):** a scratch harness over the launcher's
  `opt=` handling (fake `DAT\` with and without the layer file): empty `opt=`
  gives `BOF3X_OPT=area4-walls` with it and nothing without; `opt=none`
  nothing; an explicit list exactly itself; a set `BOF3X_OPT` untouched; bad
  values (stray characters, `none` in a list, empty names, duplicates, more
  than 8, too long) refused; an ini saved and reloaded keeps an empty
  `opt=` empty. The real launcher with `BOF3X_SELFTEST_ONLY=1` on a
  hard-linked scratch game directory: layer file present and no ini, the
  DLL logs `optional layer 1, DAT\area4-walls.*.DAT`, exit 0; `opt=none`,
  no layer; `BOF3X_AREA4_WALLS=0` with the layer named, exit 3. The
  importer's default (`default_opt`, `install`, `--no-opt`) over a fake cache
  and game directory. **Not run:** a real `build` with the default (its
  layer is the one verified above).
- **Reversible?** `opt=none` (or `BOF3X_OPT=none`); the layer is a separate
  file (`DAT\area4-walls.AREA004.DAT`), and the shipped `AREA004.DAT` is
  never changed. **From a Western disc alone** (the
  importer's cache, unified-data step 3,
  [`importer-transforms.md`](importer-transforms.md) section 5): `base/`
  holds that disc's own `AREA004` rows, walls and the 30-cell re-texture with
  them, and the open map cannot be had from such a cache. A by-source
  difference until the owner says otherwise (the owner's call 4 there).

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
  after the jump are the first pass's bit for bit - with a crossfade at the
  join. A track without a row rewinds as before. **The table as it ships
  (2026-10-10, second cut: 39 rows; 40 with the owner-approved row the same night)** of the 156 looping tracks
  ([`bgm-comparison.md`](bgm-comparison.md) section 11.2;
  [`music-open-ends.md`](music-open-ends.md) 3 for the second cut):
  - **31 full** (`001 003 013 014 036 037 039 047 051 060 061 062 063 064
    065 068 076 078 079 082 084 085 088 090 092 099 104 109 130 144 164`): the
    file holds a whole body after the loop start; the row is the disc's loop,
    start and period, in the file's samples, with a 2.9 ms crossfade.
  - **8 shifted** (`008 056 113 120 123 133 142 157`): the file
    ends before one whole body after the loop start but holds a whole period
    from its start; the row keeps the disc's period, ends as late as the file
    allows, and so starts that period earlier - its first moments are intro
    material standing in for the body's missing tail (0.07..8.1 s), with a
    5.8 ms crossfade. In time and in phase
    with the disc's loop; not the disc's notes for that stretch.
  - **1 owner-approved below the gate** (`165`, the battle fanfare; added
    2026-10-10 night, `gen_loop_table.py`'s `OWNER_APPROVED`): the measured
    shifted row - a loop back to 2.0 s, after the trumpet intro, period 22.65 s,
    stand-in 0.978 over 0.04 s - refused only on loop correlation 0.787 under
    0.80, because the held voices carry across the loop as they do on the disc.
    The owner, A/B against the disc's music: the attract, areas 091 / 093 and
    the combat loops all right, "the fanfare was the only difference I noticed -
    it loops back to the start, instead of several seconds into the song (after
    the trumpet bit)". The gate stands for every other row; this one is the
    owner's ear over the number. The owner also noted the disc's seams "bleed"
    (release tails across the loop) where the MP3's cut is harsher - the long
    crossfade of [`music-open-ends.md`](music-open-ends.md) 4 is the measured
    answer, not built (an idea, not a defect).
  - **The gates** (`tools/bgm/measure_loops.py` `gate()`): every row's period
    correlation >= 0.8 and an alignment of 10+ windows within 50 samples (512
    on the onset envelope); a shifted row's stand-in must also correlate
    with the body's tail it replaces at >= 0.5, however short; and **since
    the second cut, the period within one VSync and four samples of one of
    the sequence's own pass lengths** (`tools/bgm/seq_periods.py`: our
    sequencer, whose periods equal the renders' to the VSync on all 156,
    `libsnd-reading.md` 9.1) - the measurement's candidates pinned to those
    lengths, the vote within +-8 samples. The first cut shipped four rows the
    envelope had measured at a wrong period: `064` and `076` at 30.09 s of a
    32.10 s body (15 of its 16 bars: the last bar never played after the
    first pass), `131` at 64.19 s of 67.10 s, `070` 1.22 VSyncs (20 ms) short.
    Measured again with the pin, `064` and `076` are full rows at 32.099 s
    (their files repeat there at a median 0.99, against 0.52 at the old
    period), `131` is *shortened* (its file holds no whole body at the true
    period) and `070` falls under the correlation gate (0.638); `037`, `084`,
    `109` and `130` - refused before on correlation at wrong periods (`037`
    at 1.5 times its body) - pass every gate; `164` moved by one VSync and
    4.8 s of start. 31 rows are byte for byte the first cut's. The number
    sits in a gap of the measured distribution (29 shifted rows: 15 at
    -0.10..0.34, none to 0.62, 14 at 0.62..1.00). The first cut of the gate
    kept 151 and 153 (the battle theme and its twin) on their 0.35..0.37 s
    stand-ins, a crossfaded run-in; **the owner's call of 2026-10-10 refuses
    them with the rest**, so the battle theme rewinds as the original does
    until the disc's music (DIV-0087) plays it.
  - **Refused, rewinding as the original: 117** (second cut;
    `loops.json`'s `why` names each one's cause): shifted rows on the
    stand-in gate (`007 089 093 119 125 128 136 138 143 145 147 151 153
    159` among them: about 4 s of other material a pass, 17.3 s on 145,
    0.35 s on 151 and 153; twelve of them in the table until 2026-10-10 -
    the gate had been documented and not applied); 81 *shortened* - the
    file shorter than one loop period from its start, so no correct loop
    exists inside it (the town theme `000`: 0.44 s short; 23 of them by a
    frame or two, `near_full`, the owner's call whether a slip that size a
    pass beats the rewind - [`music-open-ends.md`](music-open-ends.md) 5
    measures it: the rewind is within -41..+7 ms of the period on all 23);
    the rest under the correlation or alignment gates.
- **Rationale:** the owner's words above; the measurement that the disc
  loops inside the song. The table is our own measurement, regenerable from
  `analysis/bgm/loops.json` by `tools/bgm/gen_loop_table.py` (which refuses
  a row whose decode is not exactly frames x 1,152 samples - the positions
  are the engine's only while ffmpeg trims nothing, true of every file
  today); it holds sample positions, not game data.
- **Also in the PSX version?** The PlayStation plays the sequence, which
  loops at these points by its own markers; the PC's rewind is the port's.
  A full row restores the disc's loop exactly; a shifted row restores its
  period and phase, not the stand-in's notes; a refused track keeps the
  port's rewind.
- **Verification of the second cut (2026-10-10, [`music-open-ends.md`](music-open-ends.md) 3):**
  `seq_periods.py` - 128 of the 156 rows' periods agreed with the sequence,
  4 of the 37 shipped did not; after the pinned re-measurement every one of
  the 39 does, by construction and by the gate. The seven rows new or moved,
  `prove_loops.py`'s seam numbers (the render's continuation, the MP3's own
  continuation past the end as the yardstick): `037` 0.58 (0.81), `064` 0.78
  (0.81), `076` 0.73 (0.75), `084` 0.69 (0.72), `109` 0.81 (0.85), `130` 0.76
  (0.77), `164` 0.65 (0.70); steps at or under the file's own; gaps 0..31.
  The 2 s continuity cannot see a wrong period - the first cut's `064` scored
  0.78 too, since bar 16 and bar 1 sound alike - which is why the sequence
  check is a gate and not a score. Built; `BOF3X_LANG=original
  BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=sound`: exit 0, "39 table rows in bounds".
- **Verification of the first cut (2026-10-10, the 39-row cut; the 37 that shipped are among them):** `prove_loops.py`'s splice and
  scores, re-run offline over the regated table (the renders on disk; the
  first second after the join against the render's continuation, before =
  the original's rewind): full rows from -0.18..0.25 (8 unmeasured: the
  render ends before the file does) to a median 0.79 (0.60..0.90), with
  steps 0.01..3.15 matching the render's own loop's (013: 3.15 against
  3.01) - except `061`, 0.01, an envelope-aligned song whose waveform
  never correlates with the render, so the score cannot speak; its file's
  own second pass matches its first at 0.949. Shifted rows from
  -0.10..0.18 to 0.32..0.86 (median 0.62; 153 0.63 before it was refused),
  steps 0.09..0.55. The
  rewind's 336..2,205 near-silent samples after the join fall to 0..97,
  except where the music rests at that point in the render too (001, 078,
  092; 085 640 against the render's 58). Built clean;
  `BOF3X_LANG=original BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=sound`: exit 0,
  the stand-in decoder looped exactly in four call patterns, "39 table
  rows in bounds"; the 37-row table built and `'*'` run with the review's
  tip. **Heard in part** (2026-10-10/11: the attract, combat, `165` on the
  MP3 path - the amendments above); the second cut's other rows not yet;
  whether replaying the intro's frames at each loop causes a hitch in play
  is unmeasured.
- **Reversible?** `BOF3X_MUSIC_LOOPS=0` rewinds every track as the original.
- **The owner's stance (2026-10-08):** the disc's music by default wherever
  a disc is a source ([`unified-data-plan.md`](unified-data-plan.md)
  section 6); this entry is what a PC-only install gets.

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
- **Reversible?** No switch of its own: a static byte the PlayStation never
  carried over. `BOF3X_ORIGINAL=Battle_CopyEnemyData` runs Capcom's function,
  which leaves the byte as it was.

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
- **Reversible?** `BOF3X_SIDE_DUP=0`. A second switch beside it,
  `BOF3X_SIDE_ZERO=1` (an experiment from the investigation, 2026-10-07;
  off unless set, `0` or `1`), leaves undrawn a side face whose texture
  word is 0; off, side faces are the original's (and this entry's).

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
  the layers installed and of the language played (no dialog box yet); an
  empty `opt=` gives the default layers installed (since 2026-10-10 DIV-0080's
  `area4-walls`), `opt=none` none. `BOF3X_OPT=none` is none, as `original` is.
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
- **Reversible?** `BOF3X_OPT=none` (or `original`), or the ini's `opt=none`
  (an empty `opt=` now plays the default layers installed, DIV-0080);
  `BOF3X_ORIGINAL=LoadDatFile` runs Capcom's loader, which
  walks neither overlay.

  **Amended 2026-10-12 (the PSP-only round, DIV-0107 and DIV-0109..0113,
  [`launcher-suites.md`](launcher-suites.md)):** the launcher's "PSP extras"
  boxes are replaced by the three suites (DIV-0107); `opt=` is unchanged, and
  an `opt=` written by the boxes reads as a partial suite, kept as written. A
  layer may now hold `bgm\`, `snd\` or `exe\` trees in the cache as well as
  `dat\` (DIV-0109..0111), read from the cache only (`install --opt` refuses
  them), and a layer's banks reach `Snd_LoadBankFile` too (DIV-0113). The
  limit of "more than 8" layers is 16 (`kOptMax`; `ConfigOptValid` follows).

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
  too long for its longest path (a bank's, `\base\bgm\bank\` plus a
  15-character name plus `.DAT`) to stay within `MAX_PATH` - 226 characters -
  is fatal at start-up. (Until 2026-10-10 the limit was 42: `File_Open`'s
  retry builds `File_CdRoot` plus the path in a 0x50-byte buffer with no
  length check, and every cache file is read through `File_Open`. That
  retry is now skipped for a path whose prefixed form would not fit the
  buffer, and the open fails as when the retry fails; with the shipped,
  relative file names nothing changes, and `File_CdRoot` in front of an
  absolute cache path could name no file anyway.) A directory without
  `base\bgm` gives one log line and no cache. **The cache is checked at
  start-up:** every `base\bgm\NNN.DAT` (a name `Music_LoadFile` can ask for)
  is parsed, its number checked against its name, and each bank the songs
  name is parsed and checked once (format, the music slot's size, master
  volume, its name). A damaged cache - a song or bank that is truncated, has
  a bad magic or version, or names a bank that is not there - is fatal then,
  with the file's path in the message, rather than at the scene change that
  would first play it; a track with no song file still plays its MP3. The
  same checks run again as each song loads, for a cache changed while the
  game runs. Armed after every module's self-test; one log line for the
  check, one when armed, one per cache song started. **A change of bank**
  keys off the synth's voices and clears their records' references into the
  old bank ([`libsnd-reading.md`](libsnd-reading.md) 3.11); libsnd's
  `SsVabClose` leaves them, so on the PSX a later volume or pitch-bend call
  can reach the old song's release tails with the new bank's tone table,
  past its end when the new bank is smaller (the player aborted there until
  2026-10-10). An abort inside the SPU model is fatal through the same log
  line as the player's (`DIV-0087: SPU model: ...`).
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
  (the loop is the data's). The synth's tick phase is arbitrary (0). After a
  change of bank, no volume or bend reaches the previous song's release
  tails, where on the PSX one can (with tone attributes read from the new
  bank, which the player cannot reproduce past its table). Whether any game
  path changes bank while a tail is still audible is not measured.
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
  **Amended 2026-10-10 (review fixes):** the self-test adds a song on the
  second program of a two-program bank, then one on the one-program bank,
  on the same synth, each equal to the oracle's; before the fix the second
  song's `Play` aborted (`tone 16 of 16`; reproduced with the 2026-10-08
  `seq.cpp` in a scratch harness). Built with llvm-mingw, no warnings;
  `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=sound` exit 0; the start-up check run
  on a synthetic cache under a 200-character root: intact, 2 songs and 1
  bank checked, armed; a truncated bank, a missing bank and a song of the
  wrong version each fatal at start-up naming the file.
  **Amended 2026-10-10 (`catchup/music-tooling`, [`music-open-ends.md`](music-open-ends.md)):**
  the SPU model's ADPCM prediction loses its `+32` (`spu-model.md` R1), which
  the renders preferred on all nine songs measured - a closer copy of the
  hardware, not a change of what the entry does; the player against the
  renders, oracle-timed with the fit run to each voice's next key on: 155 of
  the 156 renders on the owner's machine at >= 0.99 of the windows (94 at
  0.986, an allocation tie the interrupt latency decides). The host suite
  runs on Windows now; `BOF3X_SHADOW=sound` and `'*'` (19.5 min) exit 0 with it.
- **Also in the PSX version?** Yes in substance: this *is* the PSX's player
  on the PSX's data, at the PC's fade level and timing.
- **Reversible?** `BOF3X_MUSIC=mp3` (or `music=mp3`); unset `BOF3X_CACHE`
  (or empty `cache=`); no cache, no change.

  **Amended 2026-10-12 (the PSP-only round, DIV-0109):** a layer named in
  `BOF3X_OPT` with a `bgm` part is looked in before `base\bgm` (the last
  named first), and a song's bank is held by its file path, not its name.
  A PSP-only cache's `base\bgm` is the PSP's own music (DIV-0105).

### The faerie village's board buttons from the disc's own paint

- **ID:** DIV-0088
- **Date:** 2026-10-10
- **Subsystem:** the community band's sprite sheet (`COMMU01.DAT` and
  `COMMU05.DAT`, the kind-1 chunk tagged `0x1C080200` - VRAM 896, 256, 256 x
  256 at 4 bits; `tools/loc_build.py`'s `build_village_sheet`;
  [`village-text-scan.md`](village-text-scan.md) section 4)
- **Tier:** Sensible
- **Original behaviour:** the board's three side buttons - hunt, clear,
  build - are paint on that sheet (`CommuBoard_DrawSprite` kinds 3..5, 40 x
  16 at u `0x20` / `0x58` / `0x80`, v `0xE0` / `0xF0` / `0xF0`), 狩り / 開拓 /
  建築 on the port's, and every overlay left them.
- **New behaviour:** `loc_build.py` writes `<tag>.COMMU01.DAT` and
  `<tag>.COMMU05.DAT`, each one kind-1 chunk of the same tag: the port's
  sheet with, in rows 224..255, every byte that differs from the disc's
  section of the same dest taken from the disc - `Hunt` / `Clear` / `Build`
  on the US (the owner's web reference of the US screen shows the same),
  the French and German discs their own words. The sheets differ elsewhere
  too (rows 96..175, the board's frame pieces, which the port's draws place
  and which the disc's art must not replace; the French and German in a few
  more rows), and those rows stay the port's. The chunk uploads after the
  shipped one (DIV-0005's walk) and lands on the same VRAM.
- **Rationale:** the owner's ask, 2026-10-10 ("swap the side buttons for
  their localized counterparts"); the discs painted them, so there is
  nothing to draw by hand. The sprite table is the same on the US disc as on
  the PC (its fourteen 6-byte records at `COMMU01.EMI` `0x270E0`, byte for
  byte), so the rectangles are the same.
- **Also in the PSX version?** Each disc's own paint, where the PC's has the
  Chinese.
- **Verification:** `loc_build.py all` on the US disc: `village board
  sheet: COMMU01.DAT, COMMU05.DAT`; the owner's `fairyVillage.txt` replayed
  (`analysis/shots/fairy_loc4/f01620.png` and on): `Hunt`, `Clear`, `Build`
  beside their icons, the frame pieces as before. **Owed the owner's eye:**
  the French and German sheets.
- **Reversible?** play without `BOF3X_LANG`; the chunk is the overlay's.

### The importer's cache read before the install's DAT\ and SND\

- **ID:** DIV-0089
- **Date:** 2026-10-10
- **Subsystem:** assets (`LoadDatFile` `0x454590`, ours in
  `src/game/dat_load.cpp`; `Snd_LoadBankFile` `0x454770` and
  `Sound_LoadStream` `0x587910`, ours in `src/game/save_menu.cpp`; the state
  in `src/game/dat_cache.cpp`; the launcher's availability test,
  `src/launcher/config.cpp`)
- **Tier:** Sensible - the same bytes from another place; nothing changes
  without a cache, and with a cache the importer built from the PC's own
  files nothing that is loaded changes either.
- **Original behaviour:** the three functions of the exe that build a `DAT\`
  or `SND\` path (the only references to `"DAT\%s"` `0x652894` and
  `"SND\%s.DAT"` `0x666F9C`, byte search 2026-10-10) read the game
  directory's `DAT\NAME` and `SND\NAME.DAT`; DIV-0005 and DIV-0086 walk
  `DAT\<tag>.NAME` and `DAT\<layer>.NAME` after the first, which
  `tools/importer.py install` copies in from a cache. Only the music read
  the cache (DIV-0087).
- **New behaviour:** with `BOF3X_CACHE=<dir>` (or the ini's `cache=`), and
  `BOF3X_CACHE_DATA` unset or `1`:
  - `DAT\NAME` is, when the cache holds it whole, `<dir>\base\dat\NAME` and
    `<dir>\loc\zh-CN\dat\NAME` walked chunk by chunk in the PC's slot order
    as the cache's `manifest.toml` records it, then the zh file's enemy
    names; for `LoadDatFile` every chunk, for `Snd_LoadBankFile` the banks.
    A container the cache lacks, or holds only in part (a layer unwritten,
    a row with no source, a disc-made enemy table with no PC to name it), is
    the install's, whole.
  - The language overlay and each optional layer are, when the cache has
    that layer (`<dir>\loc\<tag>\dat\` or `<dir>\opt\<layer>\dat\` holding
    a `.DAT`), the cache's file for `NAME` or none; otherwise the install's
    `DAT\<layer>.NAME` as before - what `importer.py install` leaves, since
    it replaces a layer's files in `DAT\` whole. `BOF3X_OPT` may name a
    layer only the cache has.
  - `Sound_LoadStream` opens `<dir>\base\snd\NAME.DAT` when it exists,
    `SND\NAME.DAT` otherwise (its path buffer `0x28` -> `MAX_PATH`).
  - **Checked at injection:** the manifest parsed (a malformed row, rows of
    one container apart, a names row not after its table, a layer other
    than `base` / `loc/zh-CN`, a `target` other than `pc-zh`: fatal), and
    every held container's files walked by their headers against it (a
    chunk count off, a chunk past the end, a zh tail that is not kind 0:
    fatal, naming the file). `BOF3X_CACHE` not a directory or over 214
    characters, `BOF3X_CACHE_DATA` other than `0` / `1`: fatal. No manifest:
    no container from the cache, its layers and `base\snd` still read.
    Armed after every module's self-test.
  - The launcher offers a language or a layer the cache has as it does one
    in `DAT\`.
- **Rationale:** the unified-data plan's cache as the thing the engine reads
  ([`unified-data-plan.md`](unified-data-plan.md) 2 and 7,
  [`sound-import.md`](sound-import.md) 7): a disc-built cache can be played
  without copying it into the install, and the PC's own files stop being the
  only source. The slot order is required, not a nicety: walking base then zh
  changes `FIRST.DAT`'s VRAM (a zh image under a base one's tiles;
  [`cache-read.md`](cache-read.md) 3).
- **Known and accepted:** the enemy names land after a container's later
  slots rather than inside their table's chunk - the same bytes, because no
  later chunk overlaps a table; the walk allocates two buffers where the
  original allocated one, so heap addresses handed out later may differ; a
  root of 215..226 characters that DIV-0087 accepts is refused here unless
  `BOF3X_CACHE_DATA=0`; a manifest edited by hand must keep
  `write_manifest`'s exact format. **The stand-ins** (the entry
  [`importer-transforms.md`](importer-transforms.md) 5 asked for "when the
  engine reads the cache"): a cache built without the PC's `DAT/` plays a
  disc's own sections where only the PC carries the chunk - of the 35
  containers with one, the 14 with no Chinese text (`DEMO`'s language page,
  11 `MAGIC*` glyph atlases, `SCENA17`'s copy of the page) are read so, the
  disc's words where the PC kept Japan's; the 21 with text (the dial page
  among them) stay the install's. A cache built with the PC's `DAT/` has
  none. The cache holds no MP3s, by the plan:
  `BGM\` stays DIV-0087's cache song or the install's MP3. `BOF3X_CACHE` as
  the one root is the owner's open call; `BOF3X_CACHE_DATA=0` is the other
  answer ([`cache-read.md`](cache-read.md) 6).
- **Verification:** `BOF3X_SHADOW=dat_cache` (in `'*'`): the parse and the
  check on synthetic manifests (nine bad caches refused), `LoadDatFile` end
  to end on in-memory files (the cache, the fallback, unarmed = the
  install's walk), the two sound readers; 14 controls refused; seven
  planted bugs each refused, then removed. The full `'*'` self-test exit 0.
  Headless start-up against `analysis/cache/pc-plus-us`: 742 of 742
  containers held and checked; four synthetic caches, the three damaged
  fatal. Offline, `tools/cache_walk.py compare`: 742 of 742 containers the
  same end state as the install after `importer.py install`, with no
  language, with `en-US` + `area4-walls` + `psp-art` from the cache, and
  with the install's `fr-FR`; `cache_walk.py check` (in `importer.py check`)
  runs `cmd_install` itself on a synthetic cache. The 880 `base\snd` files
  byte-identical to `SND\`. **Not run:** any play with `BOF3X_CACHE` set
  (the owner's; [`cache-read.md`](cache-read.md) 9).
- **Also in the PSX version?** No: the PSX reads its disc; this is where the
  PC finds its files.
- **Reversible?** `BOF3X_CACHE_DATA=0` (the cache for the music only), or
  unset `BOF3X_CACHE`; `BOF3X_ORIGINAL=LoadDatFile` (and the other two) runs
  Capcom's readers, which read only the install.

  **Amended 2026-10-12 (the PSP-only round, DIV-0110 and DIV-0113):**
  `Sound_LoadStream` takes a named layer's `snd\` file before `base\snd`'s,
  and `Snd_LoadBankFile` a named layer's banks after the shipped file's,
  when a named layer carries one.

### A language layer built from the disc alone keeps nothing of the port's

- **ID:** DIV-0090
- **Date:** 2026-10-10
- **Subsystem:** text (`tools/loc_build.py all --cache`, run by `importer.py
  build --lang` when no PC source is given; the `loc/<tag>/` layer of an
  importer cache; [`loc-build-disc-only.md`](loc-build-disc-only.md))
- **Tier:** Sensible - the layer's bytes come from the player's disc; where
  the PC-built layer keeps the port's own, there is nothing of the port's to
  keep.
- **Original behaviour:** every language overlay is built over the port's
  own files and lets them show through: the font appends to the port's 2,451
  Chinese glyphs (and keeps its half-width single-byte glyphs and the second
  battle suffix's two), an area message the disc has no glyph for keeps the
  port's Chinese one, the village board sheet is the port's with the disc's
  labels (DIV-0088). Measured against the PC-built layers, 2026-10-10
  (`loc-build-disc-only.md` section 5).
- **New behaviour:** a layer built from a disc alone has the same chunks with
  the port's bytes left out: those 2,376 glyphs blank (a Japanese table: 42
  single-byte slots painted from the disc's sheet, `JA_SHARED`, the rest
  blank); the second suffix's two glyphs the disc's own cells by its mapper
  (`SECOND_CELLS`); such a message empty (en-US / en-150: slots 0..15 of 31
  areas, 496; fr-FR 497, de-DE 496; ja-JP 42 pool slots); a French or German
  block on that disc's own slot table, with 34 / 11 messages in the disc's
  language that the PC build keeps Chinese; no village sheet, no text for
  French `AREA004`, no title page from a French or German disc without an
  English one. Over the PC install nothing changes (byte-identical, all five
  tags).
- **Rationale:** the unified-data plan's disc-only install: a player with a
  disc and no PC port has none of the port's glyphs, messages or art, and the
  layer must not need them.
- **Also in the PSX version?** The messages: the Western discs carry the 496
  as Japanese bytes their fonts cannot draw, and the 42 Japanese pool slots
  point outside their blocks; the French and German slot tables are those
  releases' own. The glyphs and the sheet are the port's, so no.
- **Verification:** built and diffed container by container for en-US,
  en-150, fr-FR, de-DE and ja-JP (`loc-build-disc-only.md` sections 5..6).
  Not played (the engine reads a cache since DIV-0089 and plays disc-only
  since PR #55). **Owed the owner's eye**: the second suffix (banner message 3) and the blank single-byte
  slots.
- **Reversible?** build the layer with the PC install among the sources
  (`importer.py build --source bof3/DAT --source bof3/BOF3.exe ...`, or
  `loc_build.py all --game`).

  **Amended 2026-10-12 (the disc-only round, [`loc-build-disc-only.md`](loc-build-disc-only.md) 4.3, 4.5, 4.7):**
  "no village sheet, no text for French `AREA004`, no title page from a
  French or German disc without an English one" is no longer so: the village
  sheet and the other seventeen images are the disc's own (DIV-0097), the
  French `AREA004` block is built (DIV-0098), and a French or German disc's
  title page cuts CONFIG from its own sheet's letters, byte-identical to the
  PC-built layers' (no player difference, so no entry of its own).

  **Amended 2026-10-12 (the PSP-only round, group LG, [`loc-build-psp.md`](loc-build-psp.md)):**
  a PSP disc alone builds the layer too (`importer.py build --source <PSP iso>
  --lang ja-JP` or `en-150`), its names and the exe's strings from
  `BOOT.BIN`: 254 containers from either PSP disc. Against the PSX layer of
  the same tag it differs only in the PSP's own text, names and font tile
  (`loc-build-psp.md` 5.2, 5.3). Two `en-150` chunks are not built from
  PSP-EU: the Config screen's text (the PSP's screen has other rows) and
  label group 14; their addresses draw blank (D246, D247). **The font run
  takes dialogue-only cells:** `latin_extension` counted an accented cell
  past 0x93 only with ink in both the 12 px and the 8 x 8 sets; ink in the
  12 px set is now enough, and PSP-EU's three cells 0x94..0x96 are appended
  with their UI cells blank - PSP-EU's ending text uses them, and without
  them two messages of `AREA199` were empty. No PSX sheet has such a cell:
  every PSX and PC-path layer is byte-identical (section 5.1). Not played.
  Over the PC install nothing changes.

  **Amended 2026-10-13 (the faerie village, [`abc-sweep-1013-village.md`](abc-sweep-1013-village.md) 3):**
  label group 14 is built from PSP-EU too: `BOOT.BIN`'s stake title is the
  first word after the pointer table that ends before CommuDraw's category
  table (`0x652DEC`, an anchor held as its hash), and the group's payload is
  byte-identical to the PSX-EU layer's (`Culture`, `faeries` / `faery`, `Cash`
  and the zenny code, `Pot`). The board's culture label, the ranked lists'
  heading word and the hi-lo game's titles no longer draw blank on a PSP-EU
  install (D247, D265); the culture label seen in `fairyVillage`, the other
  three on no route yet. The Config screen's text (D246)
  is still not built.

### The MP3 decoder is minimp3, not the library Capcom linked

- **ID:** DIV-0091
- **Date:** 2026-10-10
- **Subsystem:** music (the MP3 path: `Music_OpenDecoder` `0x5A6D60`,
  `Music_Decode` `0x5A6F30`, `Music_Release` `0x5A70A0`, `music_loops`'
  `LoopFrame`, all ours in `src/game/sound.cpp`; the decoder behind them,
  `src/audio/mp3.cpp` over `third_party/minimp3/minimp3.h`;
  [`mp3-decoder.md`](mp3-decoder.md))
- **Tier:** Sensible - the same stream decoded by a decoder we may publish,
  more accurately.
- **Original behaviour:** the eight decoder entries `Mp3_Create` `0x5ADF00`
  .. `Mp3_Destroy` `0x5B0630` of a statically linked MPEG-1 layer III
  library of unknown lineage (200 functions, `0x5AB7E0..0x5B8DA0`, 55.5 KB)
  decode `BGM\*.DAT` and the eleven MP3s of `SND\` a frame at a time into
  the streaming ring. Measured on all 177 files ([`mp3-decoder.md`](mp3-decoder.md)
  section 2): every frame 1,152 stereo samples, as many frames as the file
  holds, `Seek(0)` a bit-exact restart, nothing written at the end of the
  stream and the last frame's byte count left there (D240); its samples
  within one unit of ffmpeg's on most frames and off by up to 370 of 32,767
  in some (one channel of a frame at a time - its own decoding fault).
- **New behaviour:** the same eight entries answered by minimp3
  (`ea99364f`, CC0; [`THIRD_PARTY.md`](THIRD_PARTY.md) section 3) through
  the callee table, with the seam's shape kept - the frame's count at
  `[[decoder + 0x14] + 8]`, the `%X@%X` name, the 4,608 left at the end as
  the original leaves it - and memory from the toolchain's heap. Same frame
  count, same frame size, same restart on every file; the samples within
  one unit of ffmpeg's on every sample of the three files measured against
  it, so the 3.38 % of samples that differ from Capcom's are Capcom's
  errors corrected. Capcom's decoder is no longer called by anything of
  ours; its 200 functions stay in the image until the cutover.
- **Rationale:** [`platform-layers-plan.md`](platform-layers-plan.md)
  section 2.4 and state 2 - no code of Capcom's may run, and a player with
  the PC install alone still needs the MP3s; the choice of decoder in
  [`mp3-decoder-choice.md`](mp3-decoder-choice.md) (the owner, 2026-10-10).
- **Also in the PSX version?** No MP3 on the PlayStation: its music is the
  SPU's sequences (DIV-0087 renders those from the disc). The PC's MP3s are
  the port's own re-encode of the disc's music.
- **Verification:** `BOF3X_MP3_COMPARE` over the 177 files, 177 the same
  shape (the table in [`mp3-decoder.md`](mp3-decoder.md) 2);
  `BOF3X_SHADOW=sound`'s `DIV-0091 self-test` line; `'*'` narrow and wide
  headless. The state hash is unaffected (`Music_Staging` is skipped).
  **Heard 2026-10-11** - the owner, on six songs as WAV pairs from `BOF3X_MP3_DUMP` (the town theme 000, 045, 086, the title 141, the battle theme 153, the fanfare 165; `analysis/bgm/listen2/`): "all sound pretty similar to me ... maybe slight variations in volume, but it could just be placebo". It is: the pairs' RMS levels agree to 0.0001 dB and their peaks to 0.006 dB on every song (the decoders' difference is -68 dBFS at the median file).
- **Reversible?** `BOF3X_MP3=original` - **deliberately not a launcher box** (the
  owner, 2026-10-11, after the listen: no reason to keep a player-facing option
  for Capcom's decoder; and the switch dies at state 3's cutover, which maps
  none of Capcom's code, so a box would outlive its use by a session). The
  decoder's code is untouched in the image until then.

### The field's music after a battle, and after a load of a save without a track, is the area's own

- **ID:** DIV-0092
- **Date:** 2026-10-10
- **Subsystem:** the field's music (`GameMode5_Leave` `0x496150`,
  `src/game/mode_rest.cpp`; `GameMode_Start` `0x495840`,
  `src/game/mode_flow.cpp`; the area's track byte `0x904CD0`, written by
  `Area_TrackFor` `0x5953B0` from the area's zone on every
  `Field_ChangeArea`)
- **Tier:** Sensible - the PlayStation's behaviour where the port left
  silence.
- **Original behaviour:** two cases, one rule each.
  1. **After a battle.** `BattleEnd_ExitHook` `0x4317B0` stops the fanfare
     with `Music_FadeOutStop(10)`, whose end marks the track byte `0x904131`
     `0xFF`. `GameMode5_Leave`, frames later once the field has reloaded,
     restarts "the field's track" as `Music_Play(Music_Track, 0xA)` - unless
     the track is `0xFF`. It always is by then, so the call never ran and
     the field came back silent until the next area change (**confirmed by
     the owner's ear on Capcom's code, 2026-10-10 night**: "capcom also
     failed to restart the area music after combat"). The PSX ends a
     fight by resuming the song it paused at the fight's start: the twin
     `0x80198F48` (JP; US `0x80197F48`, read 2026-10-23) copies the 1,376
     saved words back and `SsSepReplay`s the sequence
     ([`known-defects.md`](known-defects.md) D241 cites `0x80162610`; an
     earlier reading here said the track was restarted from its table
     through `0x8015DDF4`, which is `SsSepStop` - see
     [`field-modes.md`](field-modes.md) 4 and D347).
  2. **After a load.** `GameMode_Start`, for a loaded game (the play clock
     not zero), overwrites the area's track `0x904CD0` - which
     `Field_ChangeArea` has just chosen from the area's zone - with the
     track byte the save block holds, and `Area_Enter` plays that. A save
     written while no track was playing holds `0xFF`, and the area loads in
     silence. Saves at save points always hold a track; the port's F12 save
     (`Save_QuickWrite`, anywhere, any time) can hold `0xFF` - the owner's
     `eldritchGene` save does (byte `0x751` of the block, read 2026-10-10),
     most likely written while the stuck fade (DIV-0028's amendment) or case
     1 had the track marked stopped. Capcom's code loads it silent too (the
     owner's ear, 2026-10-10 night).
- **New behaviour:** in both places, when the track Capcom's rule would
  play is `0xFF`, the area's own track `0x904CD0` is played instead (still
  nothing when that is `0xFF` too). A save that holds a track still plays
  that track, as before. One switch, `g_area_track_fallback`, armed in
  `ModeRest_Inject` after both modules' fuzzes, which run Capcom's rule.
  **Amended 2026-10-10 night: the song resumes where it left off.** In case
  1, when the area's track is the song the battle took over, it picks up
  where it was heard instead of starting from the top
  (`src/game/music_resume.h`):
  - **Taken.** `GameMode5_Load`, before the battle's `Music_FadeOutStop` and
    `Music_Play`, calls `music_resume::Remember`. It records the track
    playing (`Music_Track`, when the stream is the BGM file `Music_Play`
    started) and the sample *heard*. The ring is two halves of 0x12000 bytes
    (18,432 samples, 0.418 s). The stream sample each half starts with is
    kept as `Music_CreateBuffer` and `Music_Pump` write it, and DirectSound's
    `GetCurrentPosition` gives the play cursor. The heard sample is that
    half's first sample plus the cursor's offset into it. This takes out
    exactly the 0.42 to 0.84 s the decoder runs ahead of the ear, and the
    lead is logged. The point is folded into the song's first pass and loop:
    the measured loop row, the rewind period, or the synth's loop-end jumps.
  - **Used.** In `GameMode5_Leave`'s fallback, `music_resume::Arm(track)`
    makes the position pending when it is for that track. Then
    `Music_Play(track, music_resume::kResumeFadeFrames)` (30 frames, half
    a second: the owner heard the 10-frame first cut on 2026-10-11 and asked
    for the longer fade, so the song does not seem to start so close to the
    battle music's end). Its `Music_Start` advances the new stream to the point
    before the first half is decoded. The sequenced song is fast-forwarded
    without a sample mixed (`MusicSynth::FastForward` over `Spu::Skip`): the
    sequencer's events, the keys and the envelopes are as a render from the
    top would leave them, and the reverb starts empty
    ([`music-seq-engine.md`](music-seq-engine.md) 8). The MP3 is decoded
    from the top and the samples dropped, which is the measured loop's own
    seek ([`mp3-decoder.md`](mp3-decoder.md) 6). A position is used once
    and then cleared (`music_resume::Forget` after the leave's music). A
    different track, a once-only song asked past its end, or nothing
    remembered starts from the top, as before, over Capcom's 0xA frames.
    The load of a save (case 2) stays a fresh start.
  - **Where it lives.** The position is the DLL's own state (statics in
    `sound.cpp`), never game memory, so the state hash and the save files
    do not see it. With the switch off nothing is remembered, and
    `Remember` returns before reading anything.
  - Under `BOF3X_MUSIC_LOG=1`, one line each:
    `music_log   fN remember track T at sample S (heard H; W written, the ring L samples ahead)`
    and `music_log   fN resume track T at sample S`.
- **Rationale:** the owner, 2026-10-10: "The battle music will continue
  after battle ends, and the area music won't resume" (compared to the
  PlayStation), and on the quicksave: "this seems to have the effect of not
  loading the area's music. Can we fix that?" The PSX twin rule
  ([`HANDOFF.md`](HANDOFF.md) traps): the PSX resumes, so case 1 is a port
  repair; case 2 cannot arise on the PSX, so it is this project's choice.
  The resume: the owner, 2026-10-10 night, from their PlayStation copy:
  "The original psx had a true resume - the song would pick up where it
  left off, not restart." DIV-0092 as first built restarted the song from
  the top (frame 2838 of the owner's route log), which the owner heard as
  wrong.
- **Also in the PSX version?** Case 1 no - the PSX resumes the area's song
  at a battle's end (read 2026-10-23 in GAME.EMI#0: the twin `0x80198F48`,
  US `0x80197F48`, restores the saved sequence words and `SsSepReplay`s the
  paused sequence - a resume, where Capcom's PC replays the track from its
  top). Case 2 cannot happen there. **Heard on the
  PlayStation, 2026-10-11 night (the owner, mednafen, the `cutsceneAndNue`
  route's save converted with `save_convert.py pc2psx` into a copy of their
  US card):** the area's music starts on that load there too - the load
  half matches for a save taken inside a scripted silence; and **after the
  Nue event battle the PlayStation starts the area's song from its top**,
  where ours resumed it mid-song, because there the pre-boss song *is* the
  current-track byte's song (the music log of the replay: `Music_Play(29)`
  by the script at frame 3458, `remember track 29` at 4240, `resume track
  29` at 6726; the zone's table says 27).
- **Second amendment (2026-10-11 night):** after an **event battle**
  (`Field_StartEventBattle`'s byte `0x904AAA`, which `GameMode5_Leave`
  clears after use) the **area's own song from its table** starts from its
  top with Capcom's fade (`0xA`) - the zone record of the leader's cell,
  read as `Area_PickMusic` reads it on an area change (`Area_ZoneTrack`),
  **not** the current-track byte `0x904CD0`, which the script's own
  `Music_Play` had left at 29 where the zone's table says 27 (the owner:
  27 is what the load starts and what mednafen plays after the fight; a
  first cut played 29 from its top and the owner heard the difference).
  The resume is for random encounters only. One `DIV-0092    event battle N
  over` log line names both tracks. **Heard, 2026-10-11 night:** the
  `cutsceneAndNue` replay on this build, the owner listening - "music starts
  fresh with the area track"; the log `event battle 2 over: the zone's
  track 27 from its top (the current byte held 29)`, `Music_Play(27, 10)` at
  frame 6726. Headless `BOF3X_SHADOW=mode_rest,sound,mode_flow,window_task`
  exit 0, 0 mismatches. A script's own music call after the battle still
  takes precedence, as on the PlayStation: the leave plays before the field
  resumes, and a later `Music_Play` replaces it. The random-encounter resume itself rests
  on the owner's earlier listening and is on the owner's list to confirm
  from the same save.
- **Verification:** `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=mode_flow,mode_rest,sound`
  exit 0 (the fuzzes run Capcom's rule, the switch armed after them). The
  owner's route `tools/recipes/musicStopCheck.txt` under `BOF3X_MUSIC_LOG=1`
  before the fix: after the fanfare's `Music_FadeOutStop` at frame 2804 no
  `Music_Play` until the second fight's at 3189; after the load, the first
  `Music_Play` is the first room change's (`Area_Enter`, frame 913). After
  the fix: the owner's ear, owed (the loaded area's music and the field's
  after the fight). What `0x904CD0` holds for the quicksave's area is the
  zone's answer, which may differ from the track an event had set when the
  save was written - accepted.
  The resume: `BOF3X_SHADOW=sound` checks both paths against the stream
  from the top. On the MP3 path a stand-in decoder is resumed at folded
  points (a rewinding track, a once-only track past its end, a measured
  loop and its crossfade) and must match sample for sample. On the
  sequenced path a synthetic song resumed through `Music_Start` must equal
  an oracle's render from the top bit for bit, on a bank without reverb.
  The test also checks the heard sample from a stand-in play cursor,
  nothing remembered with the switch off, and use once and only for the
  remembered track. The full `'*'` self-test, narrow and wide, exits 0 with
  0 mismatches. On the cache's songs 91, 0 and 1 (host `synth_render
  --skip`), a fast-forward of 60 s differs from the render from the top only
  in the reverb's return: about -20 dB for half a second, then bit
  identical from 2 s. It costs about 240 ms per minute skipped. **Not
  heard: the owner's ear is the measure.** The route is
  `tools/recipes/musicStopCheck.txt`, replayed with
  `BOF3X_CACHE=<repo>/analysis/cache/pc-plus-us` and then with
  `BOF3X_MUSIC=mp3`, under `BOF3X_MUSIC_LOG=1`.
- **Reversible?** `BOF3X_ORIGINAL=AreaTrackFallback` (Capcom's rule in
  ours, and no position remembered), or
  `BOF3X_ORIGINAL=GameMode5_Leave,GameMode_Start`. There is no runtime
  switch for the resume alone (the restart from the top under the fallback
  would be a code change: no `Arm` in `GameMode5_Leave`).

### The MP3 decoder's end-of-stream count is zero, not the last frame's

- **ID:** DIV-0093
- **Date:** 2026-10-11
- **Subsystem:** music (the MP3 path's decoder, `src/audio/mp3.cpp`
  `DecodeFrame`, behind `Music_Decode` `0x5A6F30`'s seam - DIV-0091)
- **Tier:** Sensible - a stale-byte repair, as the port's other stale-byte
  decisions (D192's zeros, DIV-0073's zeroed `Gte_Matrix2`).
- **Original behaviour:** at the end of an MP3 stream Capcom's `Mp3_Decode`
  `0x5AFC40` returns `0xFFFFFDFE`, writes nothing to the output, and leaves
  the last frame's byte count, 4,608, at `[[decoder + 0x14] + 8]`.
  `Music_Decode` reads that count after every call, the end included, and
  adds it to `filled`: on a looping track the rewind continues 1,152 samples
  past the last sample written, so the 4,608 bytes before it are whatever
  `Music_Staging` held from its previous fill, the music 0.418 s earlier,
  played for 26 ms at the seam; on a once-only track 26 ms of that stale
  music precede the silence ([`known-defects.md`](known-defects.md) D240,
  measured on all 177 files by DIV-0091's comparison). DIV-0081's measured
  loops never reach the end of a stream; every other looping track and
  every once-only one has it. DIV-0091 mirrored the count on purpose, to
  keep the stream's logic unchanged by the decoder swap.
- **New behaviour:** ours stores 0 at the end of the stream: nothing was
  written, nothing is counted. A rewind continues from the last sample
  decoded and a once-only track's zeros begin where its music ends.
  `Music_Decode` is untouched; the one-line change is `kEosBytes = 0` in
  `src/audio/mp3.cpp`, with `kCapcomEosBytes` (4,608) kept for what the
  comparison expects of Capcom's side.
- **Rationale:** the owner, 2026-10-11: "My instinct says to have it become
  zeros, to match our implementation in other stale-byte decisions." The
  26 ms were never music the composer placed there; they were the staging
  buffer's leftovers.
- **Also in the PSX version?** Not applicable: the PlayStation release has no
  MP3 decoder - its music is sequenced (DIV-0087); the decoder and its
  end-of-stream count are the PC port's alone.
- **Verification:** the DIV-0091 self-test (`BOF3X_SHADOW=sound`, headless
  on the build of 2026-10-11): `BGM\001.DAT` 782 frames of 4,608 bytes, as
  many as the header walk counts, **0 stored at the end of the stream**, the
  replay after `Seek(0)` identical over all 782 frames; game process exit 0,
  `inject: 10087 ours`. `BOF3X_MP3_COMPARE`'s shape test now expects the one
  planned difference (Capcom's 4,608 against ours 0) and reports every
  other difference as before. **Heard 2026-10-11** by the owner in play on
  the battle theme and the fanfare after it: "neither loop seemed affected
  by the 0s" - as expected, since both have rows in DIV-0081's measured loop
  table and jump before the stream's end, so this entry never acts on them;
  it is the regression check that they are unchanged. Not yet heard: a
  looping track without a table row reaching its end, and a once-only song's
  end (the 26 ms were not reported by ear before either).
- **Reversible?** `BOF3X_MP3=original` (Capcom's decoder, its count with it)
  until the cutover; after it the constant.

### The game runs as our own executable

- **ID:** DIV-0094
- **Date:** 2026-10-11
- **Subsystem:** the platform under the game: the process, its start-up and
  its loader (`bof3x.exe`, written by `tools/make_exe.py` from
  `src/hook/exe_layout.h`; `Engine_Main`, `src/hook/exe_host.cpp`; the
  launcher's default, `src/launcher/launcher.cpp`;
  [`state3-design.md`](state3-design.md), [`state3-build.md`](state3-build.md))
- **Tier:** Sensible - the same game, on a platform we can change, with
  nothing of Capcom's code in the process.
- **Original behaviour:** the player starts `BOF3.exe`, Capcom's image at
  `0x400000`. The loader maps its `.text`, `.rdata`, `.data` and `.rsrc`,
  binds its 109 import slots and runs the Visual C++ 6 runtime's start-up
  ([`crt-startup.md`](crt-startup.md) 9): the Windows version words, the
  heap, the per-thread data, the locks, the floating-point conversion hooks,
  `_adjust_fdiv`, the low-level I/O table, `stdio`, the command-line,
  environment and multibyte tables, the C initialisers, the signal filter,
  the x87 word `0x027F`, an SEH frame whose `__except` ends the process
  silently, the runtime's R6xxx message boxes - then `WinMain`. Under the
  port so far the launcher created that process suspended, reserved the
  draw-item pool's room in it (DIV-0062) and injected `bof3x.dll`, which
  wrote a jmp over each of 10,087 of Capcom's function starts.
- **New behaviour:** the launcher starts `bof3x.exe`, an image of ours based
  at `0x400000` whose sections span Capcom's range: `.stub`
  (`0x401000..0x5C4000`, `int3` but for the 10,087 jmps to our functions and
  `DirectSoundCreate`'s thunk), `.game` (`.rdata` and `.data` copied from the
  player's own `BOF3.exe`, SHA-256-verified, never written to disk; `.bss`
  zero), `.pool` (DIV-0062's 2,048 draw items). It imports `bof3x.dll`,
  whose `Engine_Main` binds the fourteen import slots our code still calls
  through, sets and asserts the x87 word `0x027F`, makes `.stub`
  read-execute and calls `Game_WinMain` on the main thread, its answer the
  exit code. What the platform **no longer does**: map or run any code of
  Capcom's (his start-up, and the twelve `[[func]]` starts still without an
  `impl` - the MP3 decoder's eight among them - land on `int3`); the runtime's heap, per-thread data, locks,
  I/O and `stdio` tables, environment and multibyte tables, initialisers,
  signal filter and R6xxx boxes (none read by the game); an exception
  nobody handles ends with our crash report and `Fatal`'s box and exit
  code 3, not a silent `_exit`; the launcher no longer suspends, reserves
  or injects; Capcom's icon is not loaded (ours, a placeholder until the
  owner's artwork - since DIV-0095 generated from the game's own data, since
  its amendment of 2026-10-15 our "III" for the file and the whelp for the
  window); `.rsrc` and the import descriptors are not mapped. What
  is **new to the process**: the code page is UTF-8 (the manifest's
  `activeCodePage`, so a cache folder of any name works - section 9 of the
  brief), `longPathAware` and themed message boxes (common controls 6) are
  declared; the stack header is reserve `0x100000` / commit `0x10000`
  (`BOF3.exe`'s asked `0x10000` / `0x10000` and Windows gave its 256 KB
  floor). The process is named `bof3x.exe`. **Kept as `BOF3.exe` had it:**
  base `0x400000`, no relocations, subsystem 4.0, no DEP or ASLR opt-in, no
  `dpiAware` (the window scales on a high-DPI desktop as before), no
  `setlocale`, the command line past the program name as `_wincmdln` gave
  it, the show flag from `STARTUPINFO`, the window title (DIV-0039).
- **Rationale:** [`platform-layers-plan.md`](platform-layers-plan.md)
  section 3, state 3: the game no longer needs Capcom's executable to run,
  only his data, and the platform becomes ours to change (64 bits, another
  system, the engine folded into one file - later states). The owner's five
  calls, 2026-10-11 ([`state3-design.md`](state3-design.md) section 10):
  `bof3x.exe`; the icon the owner's to supply; UTF-8 both halves now; the
  player's file first, the cache's `data.bin` under `BOF3X_EXEIMAGE`; this
  entry, tier Sensible.
- **Also in the PSX version?** Not applicable: the PlayStation has no
  executable start-up of this kind; the PC port's own runtime and loader
  are what is replaced, and no game behaviour is meant to change.
- **Verification:** both hosts' headless self-tests ([`state3-build.md`](state3-build.md)
  section 3: the exe host exit 0 - layout, data copy, slots, x87, 10,087
  injects, `.stub` holding nothing but them; the hosted fuzz under
  `BOF3X_SHADOW='*'` with no mismatch); the attract sequence on the state
  hash, 2026-10-11 (section 3.1): the hosted run of the same build identical
  to `attract_r14_orig` / `_origb` on all 10,305 ticks, the exe-host run
  identical to both but on the two `.bss` pages `0x93C000` / `0x93D000`,
  where every differing byte is a variable Capcom's runtime start-up wrote
  and the game never reads (the multibyte tables, the I/O tables, `_crtheap`,
  `_acmdln`) - the platform's work this entry drops, measured. Not yet: the
  ten routes, the stack watch on the F9 route, the owner's eye in play.
- **Reversible?** `bof3x-launcher --host bof3` starts the player's `BOF3.exe`
  with the DLL injected, as before - and is chosen without `--host` when
  `BOF3X_ORIGINAL`, `BOF3X_SHADOW`, `BOF3X_CALLTRACE` or
  `BOF3X_MP3=original` is set, which `bof3x.exe` refuses. The hosted build
  stays the shadow fuzz's and the reference pairs' home until a later state
  retires it.

### The executable's icon, drawn from the game's own data on the player's machine

- **ID:** DIV-0095
- **Date:** 2026-10-11
- **Subsystem:** platform (`src/hook/exe_icon.cpp`, called from
  `Game_WinMain`'s loop, `src/game/win_main.cpp`; the launcher's move of
  `bof3x.exe.new`, `src/launcher/launcher.cpp`; [`exe-icon.md`](exe-icon.md))
- **Tier:** Sensible - Windows' chrome, not the game: nothing in play changes.
- **Original behaviour:** `BOF3.exe`'s window class loads Capcom's icon
  `BOF3_ICON` from his `.rsrc` (`Game_WinMain` `0x4FCB00`, `LoadIconA`);
  Explorer, the taskbar and Alt-Tab show it. Under DIV-0094 `bof3x.exe`
  carried a plain placeholder under the same name instead (rule 1: Capcom's
  artwork is not shipped).
- **New behaviour:** in the exe host, once a drawn frame shows the VRAM
  shadow holding the save slot's party sheet and its CLUT (the title's load
  screen, the field menu - the same bytes on two drawn frames running), the
  engine renders icon 10 of `Menu_DrawBigIcon`'s table (`0x66367C`: the
  whelp, its centre 40 x 40 of 40 x 48) exactly as `tools/make_icon.py`
  does, sets it as the window's big and small icons (`WM_SETICON`) and the
  window class's (Alt-Tab), and - once an install, a worker thread, any
  failure one log line - writes `bof3x.exe.new` (the group `BOF3_ICON`
  with 16, 24, 32, 48 and 256 px images) and the `bof3x.icon` stamp beside
  `bof3x.exe`. The launcher moves the new file into place at its next start
  when the stamp says it was made from the `bof3x.exe` present. Before that
  frame, and on a first run until the launcher's next start, the
  placeholder shows. Nothing of the picture is shipped: it exists only on
  the player's machine, from the player's data.
- **Rationale:** the owner, 2026-10-11: the icon is ours, not Capcom's
  file (rule 1), and generated on the player's machine from the game's own
  data rather than supplied; the whelp, the centre crop. The menu's CLUTs
  arrive by the compressed upload path, not as image chunks (measured,
  `make_icon.py`'s docstring), so the picture can only be cut once the game
  has uploaded them.
- **Also in the PSX version?** Not applicable: the PlayStation has no window
  or file icon; the sprite is the save slot's party icon there as here.
- **Verification:** the self-test from a dump of the VRAM shadow at the load
  screen (`BOF3X_ICON_VRAM`, `BOF3X_ICON_REF`): all 69,696 pixels of the
  five images identical to `make_icon.py`'s `whelp_centre.ico`, the
  rewritten executable's sections and headers unchanged but `.rsrc` and its
  self-test exit 0; a live load-screen run (`input_run.py`, headless) wrote
  a `bof3x.exe.new` byte-identical to the self-test's, and the next run's
  launcher moved it into place and the game ran from it
  ([`exe-icon.md`](exe-icon.md) section 4). Not yet: the owner's eye on the
  window, the taskbar and Explorer.
- **Reversible?** No switch. The hosted build (`--host bof3`) keeps
  Capcom's icon from his `.rsrc`; deleting `bof3x.icon` and putting the
  built `bof3x.exe` back restores the placeholder until the next load screen.
- **Amended 2026-10-15 (the owner): the file icon is ours, the whelp the
  window's only.** Windows Defender quarantined the launcher
  (`Behavior:Win32/DefenseEvasion.A!ml`) when started from Explorer, and a
  game that writes a modified copy of its own executable for the launcher to
  swap in and run is the pattern such models flag; a rewritten executable can
  never carry a signature either. Now `bof3x.exe` and both launchers carry our
  own icon from the build - bold white "III" in Palatino on black,
  `data/icon/bof3x.ico` (`tools/make_brand_icon.py`; `tools/make_exe.py`
  puts it in `bof3x.exe` as the group `BOF3_ICON`, `launcher.rc` in the
  launchers) - and nothing writes an executable: the engine still renders the
  whelp at the load screen and sets it as the window's and the class's icons,
  but writes no `bof3x.exe.new` and no stamp, and the launcher deletes those
  an older build left (`RemoveOldIconFiles`). Before that frame the window
  shows "III". The icon self-test keeps its pixel comparison and the window
  icons; its rewritten-executable step is gone. Checked: the three
  executables' icons as Explorer extracts them, "III"; a headless self-test
  in a disc-built install, exit 0, an old `bof3x.exe.new` and `bof3x.icon`
  deleted, nothing written ([`exe-icon.md`](exe-icon.md) section 6).

### A disc-only cache's party animation scripts are the disc's own

- **ID:** DIV-0096
- **Date:** 2026-10-12
- **Subsystem:** data import (`tools/importer.py` `own_why = "arena"` /
  `"start-arena"`, `recipes/pc-zh.toml`; drawn by `Sprite_SetFrameQueueUpload`
  and the animation script functions; [`arena-edits.md`](arena-edits.md))
- **Tier:** Sensible - the disc's own data where the port's hand edit cannot
  be carried.
- **Original behaviour:** the PC port ships its own edit of 10 of member 2's
  69 animation scripts in the eight `PL` arenas that hold member 2 (parts
  moved by one or two in their byte 1, parts added, three unstepped frames
  dropped - net 352 bytes shorter) and of 1 of member 1's 17 battle scripts
  in `BPLD012` / `015` / `016` (two parts added); `START` carries `PL012`'s
  arena with that edit. In every edited bank the images, palette and frame
  entries are unchanged (`arena-edits.md` section 2).
- **New behaviour:** a cache built without the PC's `DAT/` takes each of
  those 11 arenas from the player's PSX disc, decoded as the PlayStation has
  it, without the port's edit; `START` takes that disc's `PL012` arena. With
  the PC's `DAT/` among the sources nothing changes (byte-identical). The
  `27A` pair (the port held the second of two identical 4-step loops 8 ticks
  a step instead of 4) is a rule (`hold`) and owes no entry: its output is
  the PC's chunk byte for byte.
- **Rationale:** the edit is the port's hand-made content, not derivable
  from the disc; carrying it would mean carrying the port's bytes (hard rule
  1, [`LICENSING.md`](LICENSING.md) section 3). The consumers are the
  PlayStation's code and run the unedited data. The disc-only round
  ([`disc-only-round.md`](disc-only-round.md)), 2026-10-12.
- **Also in the PSX version?** The disc's scripts are the PlayStation's own;
  the port's edit is what a disc-only player does not get.
- **Verification:** a US disc alone writes 741 of 741 `base/` containers
  (from 727); JP 741 of 741; the PC + US + PSP build byte-identical on all
  2,125 files; hosted `BOF3X_SHADOW=sprite_pose,sprite_anim,field_event,battle_sprites`
  0 mismatches. **Not seen:** member 2's field sprites and member 1's battle
  sprite may differ from the PC's by a part in those animations - the
  owner's eye ([`owner-review.md`](owner-review.md)).
- **Reversible?** Build with the PC's `DAT/` as a source.

### A disc-only layer stands in the disc's own images where the port's art showed

- **ID:** DIV-0097
- **Date:** 2026-10-12
- **Subsystem:** text and art (`tools/loc_build.py all --cache`:
  `build_standins`, `RELAID` / `relay_sheet`; [`loc-build-disc-only.md`](loc-build-disc-only.md)
  sections 4 and 5)
- **Tier:** Sensible - the disc's own paint laid where the port's draws read.
- **Original behaviour:** over the PC install every language layer leaves
  eighteen images of `loc/zh-CN` to the port's files - the glyph atlas of
  eleven modules (`AREA030`, `AREA089`, `AREA129`, `BATE`, `COMMU02B`,
  `FIRST`, `SHISU`, `SHOP`, `SISYOU`, `START`, `STATUS`), the kanji sheet of
  `ENDKANJI` / `FIRST`, the area pages of `AREA104`, `127`, `134`, `164` -
  and a disc-only cache (DIV-0090) had none of them, nor the village sheet.
  The port's atlas and kanji sheet are the PlayStation's sheet (stored as
  tiles) with rectangles cleared and a strip of the sheet's own 16x16 icons
  re-laid at a pitch of 20; the village sheet's rows 96..175 are empty
  PlayStation cells the port cleared for its strip.
- **New behaviour:** a disc-only layer writes each as the disc's section of
  the same dest, the atlas and the kanji sheet re-laid by the port's own
  rule (coordinates only), which gives the port's chunk byte for byte from
  the JP disc; the village sheet the same way. The player sees the port's
  layout with the disc's paint where the disc repainted (US: the village
  labels; French and German: their words, also on the village sheet where the
  PC-built layer keeps the port's), and on the four area pages the
  PlayStation's paint where the PC shows the port's. `FIRST.DAT`'s atlas is
  left out (a later base slot covers it on the PC too). Over the PC install
  nothing changes (byte-identical, five tags).
- **Rationale:** the disc-only install has no port art; the disc's own is
  what that release shows, laid where the port's draws read. The disc-only
  round, 2026-10-12.
- **Also in the PSX version?** The paint is each disc's own; the layout is
  the port's.
- **Verification:** `check_relaid` on five discs (the rule reproduces the
  port's chunk); the US village sheet identical to DIV-0088's; coverage of
  the 475 `loc/zh-CN` chunks 363 / 361 / 362 -> 381 for US / FR / DE. Not
  played: the four area pages and the French and German sheets are owed the
  owner's eye.
- **Reversible?** Build with the PC's `DAT/` as a source.

  **Amended 2026-10-12 (the PSP-only round, [`loc-build-psp.md`](loc-build-psp.md) 4):**
  from a PSP disc the same 16 stand-ins and 2 village sheets are the PSP's
  sections re-laid by the same rule: the rule's rectangles and strip fall on
  the PSP's sheets as on the PSX's. The PSP repainted one icon of the strip's
  source row, which the laid strip then carries (242 pixels against the PSX
  layer), and, on PSP-JP, one kanji-sheet glyph outside the rule. Not seen.

### French AREA004's first seven message slots, disc-only

- **ID:** DIV-0098
- **Date:** 2026-10-12
- **Subsystem:** text (`tools/loc_build.py` `table_after_bad_head`;
  [`loc-build-disc-only.md`](loc-build-disc-only.md) 4.5)
- **Tier:** Sensible - the choice the other discs' tables make.
- **Original behaviour:** the French disc's `AREA004` slot table starts with
  seven words past its block's end; the disc-only build skipped the block (no
  text for area 4), and the PC-built `fr-FR` layer keeps two of the port's
  messages (464 and 440 bytes) in slots 0..6.
- **New behaviour:** a disc-only block whose head slots point past its end
  takes its count from the first in-block offset (256) and points those
  slots at its first message, as the German table does. Slots 7..255 equal
  the PC-built layer's.
- **Rationale:** the US and German tables point those slots at the same
  first message; an empty message would be the only other choice.
- **Also in the PSX version?** On the French PlayStation those slots read
  past the block.
- **Verification:** the FR-only layer's `AREA004` built, slots 7..255
  identical to the PC-built layer; not played - which of slots 0..6 the event
  script names is not read.
- **Reversible?** Build with the PC's `DAT/` as a source.

### A disc-only install shows the disc's Capcom logo movie, converted at import

- **ID:** DIV-0099
- **Date:** 2026-10-12
- **Subsystem:** FMV (`tools/importer.py` "the disc's movie": `fmv_build` /
  `fmv_verify` / `fmv_install`; played by `Fmv_Play` through MCI as before;
  [`fmv-import.md`](fmv-import.md))
- **Tier:** Sensible - the movie Capcom made, from the player's disc.
- **Original behaviour:** the PC port plays `CAPCOM.AVI` from its install
  (shipped as Indeo 5, 640 x 480, 15 fps, 113 frames, PCM 22.05 kHz stereo,
  7.53 s; DIV-0001 re-encoded it to Cinepak, 112 frames). The PlayStation
  release plays `LOGO/CAPCOM30.STR` through its MDEC (320 x 240, 30 fps, 231
  frames, 7.70 s; XA 37.8 kHz stereo). Every PSX disc carries the file,
  byte-identical from the subheader on.
- **New behaviour:** a cache built from a PSX disc holds `base/fmv/CAPCOM.AVI`,
  converted by the player's ffmpeg from that stream (Cinepak, 640 x 480 by
  pixel doubling, 30 fps, 231 frames, PCM 44.1 kHz stereo; `-r 30` because
  ffmpeg's `psxstr` demuxer stamps 15), and `install` puts it where
  `Fmv_Play` opens it when the game directory has no `CAPCOM.AVI` of its own.
  A PC install keeps the PC's file; a PC install whose `CAPCOM.AVI` stayed on
  the CD gets the disc's converted movie in its game directory when the cache
  had a PSX source (and `Disc_Probe`'s local test is then satisfied by it).
  `install` copies only the movie the manifest recorded, whole (its hash).
  The player of a disc-only install sees
  the disc's logo: twice the frames per second, a 320 x 240 picture doubled
  rather than the PC's 640 x 480 master, about 0.2 s longer, audio at
  44.1 kHz from the disc's 37.8 kHz source, and Cinepak's loss (SSIM 0.970
  against the direct decode; the PC's re-encode 0.990). Without ffmpeg there
  is no movie (DIV-0100).
- **Rationale:** rule 1 - a disc-only build carries none of the PC's files,
  and the disc's own stream is the movie Capcom made. Converting at import
  keeps the engine's MCI path unchanged and puts no decoder in the engine
  (the owner, 2026-10-12, after `fmv-sibling.md`'s reading: the sibling's
  decoder is Beetle's, GPL, and the disc has one movie); the player's ffmpeg
  does the work as for the PSP's effects, so nothing is vendored.
- **Also in the PSX version?** The movie is the PSX version's; the
  difference is from the PC port's file.
- **Verification:** 32-bit Video for Windows decodes 231 of 231 frames; MCI
  with the game's own `open avivideo!` string reports 7,700 ms, frame rate
  30,000, and plays to frame 231, the PC's own file the control. **Not
  played in the game, not heard** - the owner's eye and ear. Found: MCI
  error 304 on a full path of about 160 characters (the 8.3 form opens; the
  PC's own movie has the same limit).
- **Reversible?** A `capcom.avi` in the game directory is never replaced;
  delete the converted one to go without.

### A disc-only install starts without the movie the disc probe looks for

- **ID:** DIV-0100
- **Date:** 2026-10-12
- **Subsystem:** start-up (`src/game/win_main.cpp`, `Disc_Probe` `0x5A72C0`;
  `src/game/fmv_play.cpp`; [`disc-only-install.md`](disc-only-install.md)
  section 6)
- **Tier:** Sensible - the probe asked whether the game was installed; a
  mapped disc image answers it.
- **Original behaviour:** `Game_WinMain` calls `Disc_Probe("CAPCOM.AVI",
  "BOF3.EXE")`: the movie in the game directory, else `BOF3.EXE` at the root
  of a CD drive C..L; neither, the insert-the-disc box (DIV-0039's text) and
  exit code 1. A movie `Fmv_Play` cannot open is skipped (MCI answers 304
  for a missing file too), silently.
- **New behaviour:** the same probe; when it fails under `bof3x.exe` with a
  disc-built image of `.data` mapped (`BOF3X_EXEIMAGE`, no `BOF3.exe`), the
  game starts as a local install (the CD root empty, as the probe's local
  branch leaves it), logged once. A missing movie is skipped as before, now
  logged once per file. Every other case unchanged: the hosted build, and
  the exe host over the PC's `.data`, still refuse. A disc-only install
  without ffmpeg has no movie; `LOGOS.AVI` (the porting house's) has no disc
  counterpart either way.
- **Rationale:** a disc-only install has `CAPCOM.AVI` only when the player's
  ffmpeg converted the disc's movie (DIV-0099); the movie is not what makes
  it an install. The disc-only round, 2026-10-12.
- **Also in the PSX version?** No probe: the PlayStation boots its disc.
- **Verification:** built; the self-tests do not reach `Game_WinMain`; the
  attract run from the scratch install (no AVI) starts the game and plays
  the same scenes as the PC-plus-US run (`disc-only-install.md` section 5).
- **Reversible?** Put a `BOF3.exe` or a `CAPCOM.AVI` in the game directory
  and the original probe decides.

### A disc-built image runs with the game values no disc carries as zero (provisional)

- **ID:** DIV-0101
- **Date:** 2026-10-12
- **Subsystem:** the exe image (`src/hook/exe_image.cpp`, `recipes/exe-gaps.tsv`'s
  `gap` class; [`disc-only-install.md`](disc-only-install.md) section 4,
  [`exe-engine-own.md`](exe-engine-own.md))
- **Tier:** Sensible - provisional: the owner's call is open ([`owner-review.md`](owner-review.md)
  "Decisions", the first item); this entry records what the build does
  meanwhile so that the behaviour is not an unledgered fork (rule 2).
- **Original behaviour:** `BOF3.exe`'s `.data` holds, in 456 ranges (US;
  3,358 non-zero bytes), tables that exist in no shape on any PlayStation
  disc: Capcom's game values that the port's build laid out (the largest
  `Effect_HpDamageVariance+0xB8`, 468 bytes; `Magic_Rows+0x4`, 300;
  `Equip_SlotIcons+0x50`, 131; `EventBattle_Records+0xE0`, 82), the disc's
  bytes the maps do not place (818), eight language addresses (238), and
  DIV-0055's re-laid plates (106).
- **New behaviour:** a disc-built image (`BOF3X_EXEIMAGE` from a cache with no
  PC source) maps those ranges as zero, logs each once at start, and runs;
  `BOF3X_EXEIMAGE_STRICT=1` refuses the image instead. With the PC's exe
  among the cache's sources, or the PC's file beside the game, nothing
  changes. Read from the code, not played: each spell's or item's effect file
  number reads 0 (`Magic_Rows`: the wrong effect file - the most visible),
  the dragon-form set files load wrong, the ending roll's steps are zero, the
  equipment and experience panels draw zeros, one mode's per-record words are
  zero. The field button map was one of these ranges until the BD group held
  it as a rule (a New Game would have started with every action unbound).
- **Rationale:** rule 1 forbids copying the values into the repository; the
  disc-only build runs so that the owner can see what the gaps break and
  decide between accepting them, deriving each by a rule, and requiring the
  PC's exe for those tables. Until then the permissive default is the
  measuring instrument; strict is one switch away.
- **Also in the PSX version?** Not applicable: the PlayStation's own
  executable carries its tables; the PC port's layout of them is what no disc
  has.
- **Verification:** the exe host's self-test from the scratch install with
  no `BOF3.exe` (exit 0, 10,088 ours); the gate's attract pair (the
  disc-only install runs the same scenes as the PC-plus-US run); the game
  values' effect in play not seen.
- **Reversible?** `BOF3X_EXEIMAGE_STRICT=1`; a cache built with the PC's exe
  among its sources carries every range.

  **Amended 2026-10-12 (the owner: "let's go ahead and fill them",
  `psp-only-round.md` 7.1 item 1; the PSP-only round, groups IM and IM2,
  [`disc-gap-fills.md`](disc-gap-fills.md), [`psp-image.md`](psp-image.md)):**
  four of the gaps are no longer zero on any disc-built image, PSX or PSP.
  `Magic_Rows`' effect files, `Effect_HpDamageVariance`'s dragon set-file
  tables, `EventBattle_Records`' battle files and the formation wheel's
  spots are read from the disc's own tables - a PSX disc's `BATTLE.EMI#15`,
  `GAME.EMI#0` and `BATE.EMI#0`, a PSP disc's `BOOT.BIN` and
  `BATTLE.EMI#15` - the disc's file ids turned into the PC's by the file-id
  rule (the disc's file table to path to `Dat_FileNames`' slot, all in the
  tree as addresses and names), the wheel widened from the disc's 8- or
  10-byte names. Every filled byte equals `BOF3.exe`'s, on all seven builds
  (`recipes/exe-psp.tsv`, kept only so): 860 more bytes of the PC's per
  build; per PSX build 102 fewer gap ranges, game-value bytes US 2,196 to
  1,336 and JP 1,653 to 793 (measured on the round's base, before main's
  `GAME.EMI` place of the story-flag seed, which takes 13 more from US).
  Still zero: the event battles' track half (the port's own MP3 numbering),
  the formation names (language), and the rest of `disc-gap-fills.md` 3's
  list - so the entry stays provisional for those. Verification:
  `psp_exe.py recipe` (0 bytes off the PC's on every row); the exe host's
  self-test over the real install (no `place` range differs from
  `BOF3.exe`) and on a scratch disc-only install (exit 0); not played.

  **Amended again 2026-10-12, evening and night (the owner's visual findings,
  the PSP-only round's group VF, [`visual-findings-1012.md`](visual-findings-1012.md)):**
  more runs are no longer zero on any disc-built image, each read verbatim
  from the overlay whose code reads it (`twin`, `psp_exe.TWINS`): the battle
  equipment window's slot icons and categories and the BATE screen's copy
  (`BATTLE.EMI#15`); the battle windows' `/` and `:` texts and the party's
  ability-bonus row (`BATTLE.EMI#3`); the menus' figure, count, EXP and
  play-time formats and the play-time colon (`START.EMI#8`); the result
  window's and the trade screen's icon-by-kind tables, the item row's count
  format and the encounter mode's approach pairs (`GAME.EMI#0`); the enemy
  name column's places and the wake chances (`BATTLE.EMI`); the result
  tally's steps (`BATE.EMI#0`); the world maps' buttons, drift and record-8
  animation tables (each area's own `AREAnnn.EMI#13`). The second batch:
  `WorldMap_Records`' area words (`wmrec`, `GAME.EMI#0`: the cause of the
  area 16 / 45 crash, D253), the first bytes of 30 areas' exit lists (each
  area's own overlay), the walk poses and the world-map area list
  (`GAME.EMI#0`), and the formats `"%d"`, `"AP%2d"`, `"%3dZ"`, `"*%2d"`. On
  PSP builds also the zone music sets (`msets`, from `BOOT.BIN` by its
  relocations), the dialogue box tables (`boxxy`, DIV-0103's amendment), and
  on PSP-EU the battle item command's categories, the facing step pairs and
  `Escape_Chance`'s row. Every filled byte equals `BOF3.exe`'s. These were
  the owner's "combatants shifted left", the equip icons all one glyph and
  the save menu's numbers (D251, D252). Still zero: the save slot number's
  `"%X"` (D254: the engine's own string is the recommendation, the owner's
  call) and the empty slot's text (no disc carries either), and the long
  tail of `visual-findings-1012.md` 9. Gaps after both batches (from
  `psp/round` `6c2ae0c9`): PSX-US 351 to 285 ranges (game-value 1,323 to
  1,141), PSX-EU 349 to 286, PSP-EU 351 to 286 (791 to 626). Verification:
  every rule row "0 not BOF3.exe's"; the PC path's image identical to the
  reference; no byte that was `BOF3.exe`'s lost against the previous images;
  the exe host on a US disc-only, a PSP-EU-only and the real install exit 0;
  AB's harness (`abc-harness.md`): caughFish and masterAndManillo no longer
  crash on PSX-EU, combat 81 of 87 frames equal to the PC's.
- Amended 2026-10-13, night (the areas 16 / 45 re-run,
  [`visual-findings-1012.md`](visual-findings-1012.md) 10): on PSP builds the
  four upright-quad tables `MapCell_DrawUprights` `0x570210` reads
  (`MapCell_UprightCounts` `0x66313C` and its `_V01`, `_U02`, `_Clut`
  siblings) come from `BOOT.BIN`'s packed copy (`twin`: the PSP packs the nine
  upright tables without the PC's padding, so the map's segment placed only
  the five that fill their slot), and on PSP-EU `Area_CellHooks`' 28 area
  words `0x662F28` from `GAME.EMI#0` (`cellhk`, sharing `wmrec`'s finder
  `records_place`; PSP-JP had them from `psxmap` at PSX-JP's address, which
  PSP-EU's `GAME.EMI` holds 0x1000 earlier). Every filled byte equals
  `BOF3.exe`'s. They close D258 (no trees in the field or on the world map on
  a PSP build) and D259 (no area cell hooks on PSP-EU). Verification: every
  rule row "0 not BOF3.exe's"; `importer.py check`, `exe_tables.py check`,
  `psp_exe.py check` clean; the hosted `'*'` self-test exit 0 and the exe host
  on the PSP-EU-only install exit 0 (10,088 ours); the PSP-EU image against
  PSX-EU's 27 ranges / 3,010 bytes to 22 / 2,757; the harness's
  masterAndManillo PC against PSP-EU 173 to 113 differing frames, area 61's
  trees seen back.
- Amended 2026-10-13 (the A/B/C sweep's fixes,
  [`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3): four more runs no
  longer zero on a disc-built image, each read verbatim from the overlay whose
  code reads it (`twin`; `TWINS` gains an optional sixth field picking the
  n-th copy of a run in its file): the faerie board's help words for kinds 4
  and 5 / pick rows 0 and 1 (`COMMU01.EMI#7`, every build - D260, the PSP-EU
  crash), the village's area limits (`COMMU00.EMI#0`, PSX-JP and PSX-EU -
  D261), the field equipment compare panel's slot icons (`STATUS.EMI#0`, the
  second of its two copies, every build - D262) and the gene window's `AP`
  label (`BATTLE.EMI#15`, every build - D263). Every filled byte equals
  `BOF3.exe`'s. Verification: all 23 new rows "0 not BOF3.exe's";
  `importer.py check`, `exe_tables.py check`, `psp_exe.py check` clean; the
  exe host on the PSP-EU-only and PSX-EU-only installs exit 0 (10,088 ours);
  `fairyVillage` to its last frame on all three `eu` installs.
- Amended 2026-10-13, morning (the faerie village's stats,
  [`abc-sweep-1013-village.md`](abc-sweep-1013-village.md) 2): another gap
  class closes, a `language-zero` one by a new rule shape, `faerie`: the four
  stat bytes of each of the sixty faerie trait records (`0x653200 + 20 r +
  0x10`, read by the community's ticks and the board's bars) were zero on every
  disc-built image, so the bars drew black, the food counter read 34 where the
  PC's reads 84 and the village simulated with zero traits (D264).
  `ETC/COMMU00.EMI#0` carries the same stats in sixty 9-byte records (a 5-byte
  name, the stats at +5) on every disc; the rule copies them record for record,
  found at recipe time by all sixty stat words in order (`records_place` with
  the disc's own stride). The names stay the language layer's. Every filled
  byte equals `BOF3.exe`'s. Verification: the row "236 bytes, 0 not
  BOF3.exe's" on all seven discs; `recipes/exe-gaps.tsv` loses 59 rows a build;
  `importer.py check` / `verify`, `exe_tables.py check`, `psp_exe.py check`
  clean; the exe host on the PSP-EU-only and PSX-EU-only installs exit 0
  (10,088 ours); `fairyVillage` on the three `eu` installs: of the 134 village
  frames, PC against PSX-EU differs only at the ranked list's page counter
  (`0x6531F4`, engine-own, known) and the stat bars, food counter and pop-ups
  are equal on all three.
- Amended 2026-10-13, morning (the one-file runs and PSP-EU's own layout,
  [`exe-image-twins-1013.md`](exe-image-twins-1013.md)): 39 more runs no longer
  zero on the disc-built images that lacked them, each the PC's whole run held
  by one file of the disc and read there by that overlay's code (`twin`, the
  builds whose gap it closes): 13 `disc-unplaced` (effect-kind state and frame
  tables in their area and scenario overlays, `BossGolem_Hooks+0xC`,
  `Area94_EffectArgsD` from the third of AREA094's copies, and on the PSP
  builds `ItemUse_HandlerIndex+0xD0` from `BOOT.BIN`) and 26 `game-value` (more
  effect-kind tables, five spell effects' tables). A new way of finding a
  place on a PSP disc, `pspmap`: where the PSP disc's file is laid out
  otherwise than PSX-JP's (PSP-EU's `GAME.EMI` from 0x1000 earlier, parts of
  `BATTLE.EMI#15` 0x3000 later), a run `psxmap` misses at PSX-JP's address is
  read at the one place where the PSX-JP disc's own bytes either side of it
  stand beside BOF3.exe's run - four runs on PSP-EU (`EffectKind34_V0Areas`,
  `Scenario_Hooks+0x58`, `Field_ModeTailKinds+0x100`,
  `MenuList_GeneWinSteps+0xE`), none on PSP-JP. Every filled byte equals
  `BOF3.exe`'s; no byte that was the PC's is lost. Gap ranges (`exe-gaps.tsv`): PSX-EU 282
  to 247, PSP-EU 251 to 220, PSX-US 282 to 249. No symptom of any of them was
  observed, so no defect is numbered. Verification: 221 recipe rows (217
  `twin`, each "0 not BOF3.exe's"; 4 `pspmap`, kept only when equal);
  `psp_exe.py`, `exe_tables.py`, `importer.py check` clean;
  the exe host on a PSX-EU-only and a PSP-EU-only install exit 0 (10,088 ours,
  `BOF3X_EXEIMAGE gap` 247 and 220 ranges); not played.
- Amended 2026-10-13, afternoon (set 8's verb, the owner's call on
  [`abc-sweep-1013-village.md`](abc-sweep-1013-village.md) 4.1, option 1:
  "keep `Quit`"; [`disc-data-inventory.md`](disc-data-inventory.md) 9.3): one
  byte that no Western PSX disc carries in the PC's shape is filled by a rule,
  `verb`. `Menu_ButtonSets` set 8 (`0x663864`) is `(2, 0x14, 0x0B)` on the PC
  and the JP-shaped discs; the US, European, French and German discs give its
  second button their own 23rd verb (`0x16`, `End`), which the port's 22-entry
  verb table does not have, so the map refused the byte and the image held 0
  (the button would read `Use`). The rule finds the disc's record by sets 0..7
  verbatim in `ETC/STATUS.EMI`, requires the disc's byte to be `0x16`, and
  writes the port's `Quit` (`0x0B`) at `0x663866` - a mapping of the disc's
  verb to the PC's, not a copied value; the row is kept only because the
  result equals `BOF3.exe`'s. The Western releases' own `End` (a 23rd verb
  in the engine) was the alternative; the owner chose the port's word.
  Verification: `psp_exe.py recipe` over the seven discs
  (scratch `policy/regen_r1.log`): one `verb` row on each of US, EU-English,
  FR and DE, "1 bytes, 0 not BOF3.exe's"; `recipes/exe-gaps.tsv` loses the
  four `unread` rows `0x663866..0x66386C` (the five bytes after the verb were
  already the PC's zeros); `exe.toml`'s four Western images change by that
  one byte each (`placed_equal` + 1). Planted faults (scratch `negverb.py`):
  a `verb` row on PSX-JP, a two-byte row, another file - each refused by
  `psp_exe.py check`; a disc byte other than `0x16` refused by the rule. A
  PSX-EU-only install (`importer.py build --source <EU cue> --lang en-150`,
  `install --base`): the image holds `02 14 0B` at set 8; the exe host's
  self-test exit 0, `inject: 10088 ours`. `psp_exe.py`, `exe_tables.py`,
  `importer.py check` clean. Not played: no route reaches set 8.
- Amended 2026-10-13 (the us set's sweep,
  [`abc-sweep-1013-us.md`](abc-sweep-1013-us.md) 4): the faerie ranked lists'
  page counter format `0x6531F4` (`"%d/%d"`, `CommuRank_Show`'s format, a
  `language-zero` gap on all seven builds) is no longer zero on a disc-built
  image, read verbatim from `ETC/COMMU05.EMI#7` (every build, `twin`), where
  that overlay's own reader loads it; every filled byte equals `BOF3.exe`'s.
  On a disc-only install the village's ranked lists drew a stale digit where
  the PC draws `1/2`. Verification: that doc 4.2 (the seven rows "5 bytes, 0
  not BOF3.exe's"; the checks clean; the exe host on the US disc-only install
  exit 0; `fairyVillage` reads `1/2` on both sides at 810).
- Amended 2026-10-16 (D303, D286; [`fishing-rule-pages.md`](fishing-rule-pages.md)):
  the fishing spot's Rule pages and panel headings are no longer zero on a
  disc-built image. The pages' title and row ids (`0x653DF4`, 6 + 54 u16) are
  two catalogued tables (`tables.toml` `fishing_rule_titles` /
  `fishing_rule_lines`), read in every build from the fishing module whose
  `EffectKind0F_DrawMessageList` twin reads them (`WORLD00/AREA030.EMI#4`; on
  the PSPs `BOOT.BIN`'s one copy); the panel headings `ROD` .. `GUIDE`
  (`0x653EA0`, US, EU, JP and the PSPs) and the counter format `"%d/6"` with
  the data page's total (`0x653EB4`, all seven) are `twin` rows of the same
  module, equal to `BOF3.exe`'s. Zero, the rows each drew the pool's message
  0 (the fish list's first entry) nine times down the panel, the title that
  message's first line, the counter a stale `0`, the headings nothing. **The
  one deliberate difference from `BOF3.exe`**: the Western discs (US, EU, FR,
  DE, and PSP-EU's `BOOT.BIN`) lay the rows out for their own text - page
  1's third line one row lower (a blank row before it), page 3 a line `0x93`
  after its second (the blank after it one row later), DE also `0x6D` in page
  3's last, blank row; titles and pages 2, 4, 5, 6 as the PC's - and the image takes the disc's
  layout, since the message pool it indexes is that disc's (the owner
  prefers the disc's data where both exist, `visual-findings-1012.md`, "For the other files";
  DIV-0104 is the precedent of a
  disc-built image holding a disc's own table). JP and PSP-JP equal the
  PC's. On PSP-EU this replaces the JP layout its map took from the EMI copy.
  Still zero: FR's and DE's headings (their discs re-lay the words at other
  widths; D286 stays open for those two). Verification: `psp_exe.py recipe`
  over the seven discs (12 new rows, each "0 not BOF3.exe's", no other row
  moved); `exe_tables.py recipe` and `gaps` (13 gap rows out, FR's and DE's
  headings in under their new name, nothing else); `tables.py`,
  `exe_tables.py`, `psp_exe.py`, `importer.py`, `exe_own.py check` clean;
  `campingFishing` on a fresh PSX-US disc-only install (`fishing-rule-pages.md`
  6): `Fundamentals`, the three lines with the US blank row, `1/6`, the
  equip headings drawn, the route to its last frame. The other builds not
  played.
- Amended 2026-10-16 (D254, the owner's report of odd characters on the
  empty save slots and behind the named ones on a PSX-only install): the
  save slot number's format `0x6639D4` (`"%X"`, `Menu_DrawSaveSlot`'s) is
  no longer zero on a disc-built image. No disc carries it - the port's
  sixteen save files are its own - so it is the engine's own string
  (`exe_own.cpp`'s `SlotNumber`, [`exe-engine-own.md`](exe-engine-own.md)
  2), not a `twin`; it equals `BOF3.exe`'s bytes, checked at every start
  where the file is there. Before it every slot of the save and load lists
  drew the format buffer's stale text in the number's place (`) 57` on the
  empty slots, a stray glyph after the name on a saved one); now each slot
  draws its number as the PC does and an empty slot is otherwise blank, as
  the US release leaves it (`0x669CCC` is still zero, untouched). With the
  PC's `.data` the fill changes no byte. Verification: `exe_own.py check`
  (19 regions, 20,890 bytes); the hosted self-test on the PC install with
  `BOF3X_SHADOW=exe_own` (exit 0, `19 regions ... each equal to BOF3.exe's
  .data`, the zero-image fill `16906 non-zero bytes ... nothing outside`);
  PSX-US `load_list` and `cutsceneAndNue` before and after ([`known-defects.md`](known-defects.md) D254).
- Amended 2026-10-16 (the owner, a PSX-only install; [`IDEAS.md`](IDEAS.md)
  I43, [`exe-import-engine.md`](exe-import-engine.md) 7.7): the importer
  places a region by the placed pointers that point into it, read from the
  disc's own words. On a disc-only install the child's field sprite in the
  forest after the mine was drawn in pieces: the record lists of party members
  7 and 9 (`0x669868..0x669883`, a `game-value` gap on all seven builds) were
  zero, every pose bank 0. They are now the disc's own (US `BOOT@0x80181C58`
  and `@0x80181C68`), with 18 to 54 more places per build. Every placed byte
  equals `BOF3.exe`'s (`placed_differ` unchanged on every build); `gap` bytes
  PSX-US 2,502 to 2,277, PSX-EU 2,453 to 2,308, PSX-JP 1,817 to 1,672, PSP-EU
  1,406 to 1,261. Verification: the recipe checks clean; `whelpBoss` frame
  4710 on a rebuilt PSX-EU disc-only install, the sprite's region against the
  PC install's: 5,190 differing pixels before, 44 after (the rest is the
  frame's 3x scale sampled down); not played by the owner yet.
- Amended 2026-10-17 (D282, issue #77 - a player on a PSX-only install): the
  Config screen's row tables (`0x653808` row to first record, `0x653810` row
  to option count, 13 bytes) are no longer zero on a disc-built image. Zero,
  every row drew no option value (the labels only), on every disc build. A
  `twin` row from `ETC/START.EMI` (`psp_exe.TWINS`): the 13 bytes are there on
  all seven discs, after each disc's own records (US `#8@0x801ECDAC`), and
  `STATUS.EMI#0` holds the same copy; the padding after them differs by disc,
  so the run stops at the count table's last row. Equal to `BOF3.exe`'s
  (`exe-psp.tsv` "13"; `placed_equal` +13). `exe-gaps.tsv`: the `language`
  row now ends at `0x653808` and the 24 / 27 bytes after the tables are a
  `game-value` gap, as they were in substance. Verification: the recipe checks
  clean; a PSX-US disc-only install rebuilt from the recipes draws `Fast Med.
  Slow`, `1 2 3 4` twice, `Stereo Mono`, `Off On` on the title's Config
  (the 10-12 cache's image had the 13 bytes zero); not played by the owner yet.
- Amended 2026-10-17 (the install-gap census, [`install-gap-census.md`](install-gap-census.md)
  4.1 and 8; D310, D311, D278): seven more bytes runs a disc-built image left
  zero are filled. **Six draw formats and marks of Capcom's code** - area 75's
  counter format `0x60ABF0`, area 104's countdown `0x61BC74`, the faerie
  village board's dash and formats `0x653084..0x653098`, the level-up's
  ability-name `%s` `0x654900` (the Western discs' gap), the Identify panel's
  unknown marks `0x65AAC4..0x65AAD0` and the field menu's play-time
  separator `0x6637E0` - are the engine's own, as D254's is (`exe_own.cpp`,
  a new `game-format` class in `exe_own.py`): each disc keeps its copy inside
  its reading overlay where no search can place it (too short to be unique;
  the Western `MAGIC060` lays the marks out otherwise), and each equals
  `BOF3.exe`'s bytes. **The hi-lo game's payouts** `0x652CF4..0x652D04` are a
  `twin` from `ETC/COMMU02.EMI` section 8, the disc's own 16 bytes, on all
  seven builds. Before: area 75's counters, area 104's countdown digits, the
  board's rows, a level-up's learned-ability names (Western PSX), the
  Identify marks and the play-time colon drew nothing, and the hi-lo game
  paid 0 (not measured in play). With the PC's `.data` nothing changes.
  Verification: `psp_exe.py recipe`, `exe_tables.py recipe`, `exe_own.py
  measure`, `exe_tables.py gaps` over the seven discs (`exe-psp.tsv` +7 twin
  rows; `exe-gaps.tsv` loses exactly those runs on each build, the
  `TornadoMote_Phases` row ending at `0x65AAC4`; every image hash moves by the
  twin); the six fills equal `BOF3.exe`'s bytes (an offline comparison);
  `importer.py`, `exe_tables.py`, `exe_own.py`, `psp_exe.py`, `tables.py`
  checks clean. The hosted `BOF3X_SHADOW=exe_own` self-test and play: not run
  yet.
  The same day, D267 and D268: a `twin` row now takes back a byte that a map
  segment of another file placed with a different value (`psp_exe.fill`), and
  a new `twin` from `MAGIC226.EMI#3` holds the Meteor Strike chip's lifts. On
  every build the lifts and area 112's link list equal `BOF3.exe`'s; nothing
  else moved (`placed_differ` -4 on each PSX build, -1 on each PSP; no place
  changed).
- Amended 2026-10-17 (D316, issue #83 - players on disc-only installs: the
  wrong music in every boss fight, and in the miners' fight at the start):
  **the event battles' track half** (`EventBattle_Records+0xE2`, 41 words)
  is no longer zero on a disc-built image. Zero, `GameMode5_Load` played song
  0 for every event battle, over the scene's own music where the PC's word is
  0xFF. No disc holds the number - the PSX's `GameMode5_Load` (`0x80198C94`)
  loads the battle file and its own music plays - so the `pairs` rule now
  derives it: the battle file's music sections are a whole copy of one
  `BIN/BGM` file, and the word is that file's sub-song 0 in the disc's own
  song table; a file with no SEP gives 0xFF (`psp_exe.event_track`). Equal to
  `BOF3.exe`'s on all 41 words of both PSP builds; on the five PSX builds 40
  of 41, the other one boss the PC gives 153 where the rule gives 151:
  `BGMBAT02.EMI` is a byte copy of `BGMBAT00.EMI` on those discs, so the two
  songs are one music from a PSX disc's data (on the PSP they are two files
  whose SEPs differ by 20 bytes; a PSX base with the PSP's music layer plays
  that boss `BGMBAT00`'s PSP render). The recipe's equality gate accepts
  exactly that: a track naming a song of the same byte-identical set. With the
  PC's `.data` nothing changes. *2026-10-21 (the owner: "have the fight use
  BGMBAT02"):* a tie now takes the highest song of the set, so that boss
  (`BOSS033`, event battle 25) is 153 on every build, `BOF3.exe`'s word: 164
  of 164 bytes equal on all seven (`psp_exe.py recipe`), and a PSX base with
  the PSP's music layer plays `BGMBAT02`'s own recording. Verification: `psp_exe.py recipe`,
  `exe_tables.py recipe`, `exe_own.py measure`, `exe_tables.py gaps` over the
  seven discs (`exe-psp.tsv`'s `pairs` rows 82 to 164 bytes; `exe-gaps.tsv`
  loses the 41 track rows per build); `psp_exe.py check` clean; the `whelpBoss` route on a scratch PSP-EU-only install under
  `BOF3X_MUSIC_LOG=1` (en-150; the route, recorded in en-US, drifts after the
  fight): before, frame 718 loads `BOSS007` and plays `Music_Play(0, 10)`,
  `bgm\000`; after `importer.py setup` rebuilt the image, the same frame plays
  `Music_Play(151, 10)`, `bgm\151` (bank `BGMBAT00`), and the start-up log's
  41 `EventBattle_Records` gap lines are gone. Not heard.
- Amended 2026-10-18 (the install-gap round, [`install-gap-round.md`](install-gap-round.md);
  its stages A and B merged on `install-gap/round`, the recipes regenerated at
  `0baf26f7`): the census's NEEDS FIX rows
  ([`install-gap-census.md`](install-gap-census.md) 8) are filled by five new
  kinds of placement: 61 of the 145 symbols with a `gap` row at `06a209bd`
  have none left (90 remain, six of them the remainders of pieced rows).
  Zero, those bytes were read as zero: of the ten gap rows the read watch
  saw read in play ([`install-gap-reads.md`](install-gap-reads.md) 0), eight
  were live defects. Six of them, and half of a seventh, are filled here:
  the Nue fight's cloud offsets, the PSPs' default light pair, screen
  margins, member-flag byte and trait list 1 (every save load), PSX-JP's
  fish kinds (every area change), and the German equipment panel's slot
  icons (D319). The same German row's labels and the French and German
  fishing headings (the eighth) are a language layer's (DIV-0069's and
  DIV-0064's amendments of the same day). The other two rows are the owner's
  (D266's title mark, the empty save slot). Every byte the round fills
  equals `BOF3.exe`'s, and the kinds are:
  - **Pieced segments** (TW, [`install-gap-twins.md`](install-gap-twins.md)
    2): a `TWINS` entry may carry a seventh field, segments inside its run.
    The place is the one disc address where every segment equals
    `BOF3.exe`'s bytes at one displacement; `find_rows` writes one `twin`
    row per segment, and D268's rule applies to each. The bytes between are
    the disc's own values and stay unfilled, whole fields only. Used for
    `EffectKind66_States+0x18` and `EffectKind5B_States+0xC` (Western PSX)
    and `CommuMusic_CloseSteps+0x28` (FR, DE). The three u16 fields each of
    the first two rows, which the Western discs hold otherwise, are an owner's
    call. TW's plain twins with them: 146 `twin` rows over the seven builds,
    among them area 169's dial grids (P1 item 7: one verbatim run in
    `WORLD04/AREA170.EMI#13` after all), world map 115's legend frame and
    area 148's and 170's attach vectors (group RD's readers,
    [`install-gap-readers.md`](install-gap-readers.md) 4).
  - **`BOOT.BIN` twins by its relocations** (PB, twins doc 5.1): `boot_place`
    settles the ELF's hi16 / lo16 pairs by register and takes, among a
    run's copies in `.rodata` / `.data`, the one a relocated pair reads into
    whose place agrees with the PC's pointer words before it; exactly one, or
    no row. PSP builds only, refused loudly on a PSX build. Seven rows per
    PSP build: the default light pair, two leader move scripts packed without
    the PC's padding, four `EffectKind18Sub*` state tails. The rule
    `margins` is DIV-0103's amendment of the same day. **Through a relocated
    pointer table** (PT, twins doc 6.1): where no pair reads a copy
    directly, `boot_place` takes the one `BOOT.BIN` table whose entry names
    the copy, whose relocated words sit at exactly the PC table's pointer
    offsets and which a relocated pair reads: `Char_TraitLists`' list 1 on
    both PSPs (`Char_ApplyTraits`, every save load). `psp_exe.py check`'s
    `table_selftest` refuses five near misses on a synthetic ELF. PT also
    appended group CW's two rows ([`install-gap-live-rows.md`](install-gap-live-rows.md)):
    `ChlorineCloud_Offsets` from `BMAGIC/MAGIC009.EMI#3` on all seven, and
    the PSPs' whole member-flag table `0x669738..0x669743` from `BOOT.BIN`
    (the three gap bytes alone match 503 places; the 11-byte table one), and
    the German slot icons `0x663660..0x663668` from `BATE.EMI`'s second copy,
    the panel's twin (D319).
  - **`immref`** (OT, [`exe-immediate-places.md`](exe-immediate-places.md)):
    a row of `recipes/exe-imm.tsv` names the disc file and section, the PSX
    twin of a PC function that reads the gap, and the `lui` and `%lo`
    instructions in it whose address holds the bytes in the twin's own
    overlay. `exe_tables.py recipe` writes one only where every number byte
    from the reader's operand equals the PC's (four or more, two non-zero)
    at one place; `build` applies it last, to bytes nothing else filled;
    `check` refuses a row its disc does not decode or that does not rebuild
    `BOF3.exe`'s bytes. The value shape (`imm`, a twin's stored constants)
    is built too and matched nothing. Engine-own regions are excluded:
    D310's ability-name format stays the engine's string though `immref`
    would place a copy from `SCENARIO/SCENA10.EMI` on PSX-US (the owner's
    call). The twins come from OT's code-pointer pairs and group FP's
    pairing of the effect, magic and boss state functions
    ([`install-gap-pairs.md`](install-gap-pairs.md)), written to
    `symbols.toml`'s `psx` fields at the `hypothesis` tier.
  - **The zero-tail end** (FP's proposal, built by the coordinator): an
    `immref` row ends where the PC's remaining bytes are all zero and the
    disc's next data begins, so the PC's alignment zeros stay unfilled (the
    PC's own value there); any other difference still refuses the row. It
    placed `EffectKind2A_States+0x10` (the rows that move the party),
    `Area118_SpawnKinds`, `BreathMote_Angles` and two `EffectKind18Sub*`
    tails.
  - **`IMM_SKIP`** (`tools/exe_tables.py`): regions the immediate places
    leave alone because they are the owner's - today `Area104_PlateAnims`,
    DIV-0055's re-laid plates, which the zero-tail end would otherwise have
    placed.

  Two `find_places` fixes change only provenance: a paired code word agrees
  on a PSX build only where the disc's word is a code address (five
  `AREA170` place rows dropped, the image unchanged), and a section's last
  bytes compare on their own (five place rows grow, placing
  `Area91_RingTurns` and `Area151_DriftUV+0x8`). Reclassed without a byte
  changing: `MagicBallOrb_Shades+0x10`, `MasterWin_Requirements+0x44` (a
  string no operand names: no Western list, so P1 item 8 is not one) and
  `Music_LoadedTrack+0x1E30` are `unread`; `LANGUAGE_ZERO` is per build, so
  three rows a Latin layer writes are `language` on the Latin builds and
  stay gaps under `ja-JP`. `exe-gaps.tsv`'s `gap` rows / bytes / the PC's
  non-zero bytes, `06a209bd` to `0baf26f7`: PSX-JP 104 / 1,486 / 797 to 58 /
  673 / 311; PSX-US 122 / 2,089 / 1,320 to 76 / 1,172 / 761; PSX-EU 118 /
  2,120 / 1,348 to 75 / 1,262 / 827; PSX-FR 122 / 2,269 / 1,460 to 76 /
  1,295 / 857; PSX-DE 125 / 2,297 / 1,474 to 78 / 1,314 / 865; PSP-JP 92 /
  1,069 / 628 to 54 / 548 / 296; PSP-EU 93 / 1,079 / 634 to 52 / 517 / 274
  (the census's section 8 has the table). The recipes: `exe-psp.tsv` + 172
  `twin` rows, 2 `margins`, one more `psxmap` per PSP build; `exe-imm.tsv`
  141 `immref` rows (PSX-JP, US, DE 24, EU 23, FR 22, each PSP 12).
  Verification: every new `twin` / `margins` row "0 not BOF3.exe's";
  `exe_tables.py check --game` with the seven discs decodes every `immref`
  row against `BOF3.exe` (0 errors; three hand-corrupted rows refused);
  `psp_exe.py`, `importer.py`, `exe_tables.py check` clean after each merge;
  `tools/exe_scan.py` over the seven discs at `0baf26f7` (harness root
  `C:\Users\kerok\g_end\scan`): `missing/gap` 311, 761, 827, 857, 865, 296,
  274, the table's last column byte for byte; the exe host's self-test from
  PSX-US, PSP-EU, PSP-JP, FR and DE disc-only caches in the groups' roots
  (exit 0). Not played: the round's live end batch (the A/B/C caches, the
  attract gate, the hash routes, the captures) is not run, every local
  launcher build being quarantined by Windows Defender. The entry stays
  provisional, and `BOF3X_EXEIMAGE_STRICT=1` is not the default (I43 step 4,
  not ready: the read watch is to be re-run on the merged branch, and the
  two rows read in play are the owner's).
- Amended 2026-10-20 (D266, the owner's call of 2026-10-13, "the away day"
  in [`owner-review.md`](owner-review.md), confirmed 2026-10-20: "read the
  12"): one byte the Western PSX discs carry in their own value is filled
  by a rule, `titlemark`. `Title_Sprites` (`0x653850`, 17 entries of u16 u,
  v, w, h, clut) entry 1 is the title's mark; its width is 10 on the PC and
  the Japanese discs (the (R)) and 12 on the US, European, French and German
  discs (their glyph page's TM), so no segment matched and the image held 0:
  a Western disc-only title drew no mark (D266). The rule finds the table in
  `ETC/GAME.EMI` by entries 0..6 equal to `BOF3.exe`'s but for that byte
  (entries 7..9 are regional too), requires the disc's byte to be 8..16, and
  copies it - the disc's own value, the second rule after `fishpen` whose row
  is kept although it differs from `BOF3.exe`'s. Verification: `psp_exe.py
  recipe` over the seven discs: one `titlemark` row on each of US, EU-English,
  FR and DE (`GAME.EMI#1`, `0x801D1C7A` on the US disc, `0x801D1C3A` on the
  three PAL ones), "1 bytes, 1 not BOF3.exe's", each byte 12; no other row of
  `exe-psp.tsv` moved. The regeneration (`exe_tables.py recipe`,
  `exe_own.py measure`, `exe_tables.py gaps`) also carried in the second
  pairing pass's rows ([`install-gap-pairs.md`](install-gap-pairs.md) 8,
  merged with PR #94 but not regenerated then): `exe-imm.tsv` + 16 `immref`
  rows (PSX-JP 4, every other build 2); `exe-gaps.tsv`'s `gap` rows PSX-JP 58
  to 53, US 76 to 72, EU 75 to 71, FR 76 to 72, DE 78 to 74, PSP-JP 54 to 51,
  PSP-EU 52 to 49, none new; `exe_tables.py check` with the seven discs 0
  errors. Planted faults: a `titlemark` row on PSX-JP, a two-byte row and
  another file each refused by `psp_exe.py check`; a disc byte of 40 refused
  by the rule. Live: a US disc-only install built by this branch's importer
  draws the TM whole at (262, 130), against the PC install's (R)
  (`analysis/shots/d266-title-mark-1020/`, local).

### A disc-only install refuses the port's Chinese text instead of playing without text

- **ID:** DIV-0102
- **Date:** 2026-10-12
- **Subsystem:** language (`src/launcher/config.cpp` `ConfigResolveLanguage` /
  `ConfigOriginalAvailable`, the launcher's dialog; `src/game/dat_load.cpp`
  `RefuseOriginalWithoutChinese`; `tools/language_tags.py` `original_available`,
  reported by `importer.py install`; [`disc-only-install.md`](disc-only-install.md)
  section 2 and 6)
- **Tier:** Sensible - rule 4: a start that cannot have its text says so.
- **Original behaviour:** the port has one language, its Chinese, in `DAT\`;
  every install had it, and `original` (the port's script) is the default
  language of ours (DIV-0005's tags) for a PC install.
- **New behaviour:** `original` (`BOF3X_LANG` unset or `original`,
  `language=original`) needs `BOF3.exe` in the game directory or the cache's
  `loc\zh-CN` layer - the two facts that mean the port's text is there.
  Without either, a disc-only install: the launcher refuses `original` and any
  language not held, before the dialog, with a message naming the languages
  held; the dialog does not offer "Chinese (PC script)" and says why; the
  default is the first language held in the engine's list (`en-US` from a US
  disc); and the engine's `DatLoad_Inject` stops with a Fatal naming the same
  languages when such a request reaches it past the launcher. A PC install is
  unchanged.
- **Rationale:** the round's end batch started a disc-only install under
  `original` by mistake (the attract tool's own `--lang` default) and the game
  read every Chinese chunk as missing, played with no text, and the attract
  sequence stalled in area 4's message box (message 8 never closed; read from
  the run's message columns, not seen on screen). The text is the PC's alone,
  so it cannot be made; the start can say so. The disc-only round, 2026-10-12.
- **Also in the PSX version?** No: each disc carries its own text.
- **Verification:** `disc-only-install.md` section 5's rows of 2026-10-12: the
  launcher's refusal under `original` and `fr-FR` against the US-only install
  (naming `en-US`), its start under `en-US` and with no language (exit 0,
  10,088 ours), `bof3x.exe` run directly under `original` (exit 3, the Fatal),
  the real install unchanged under all three (exit 0), hosted `shell` 0
  mismatches. Harnesses: `tools/attract_run.py` defaults `--lang` to
  `original`, so a run against a disc-only install must pass `--lang`.
- **Reversible?** A PC install, or a cache built with the PC's `DAT\` among
  its sources, holds the Chinese layer and `original` plays as before.

### A PSP-only install's base is the PSP's own sections

- **ID:** DIV-0103
- **Date:** 2026-10-12
- **Subsystem:** data import (`tools/psp_standins.py`, `recipes/psp-standins.toml`,
  the one call in `tools/importer.py` `get_chunk`; the manifest's `[build]
  base_policy`; [`psp-stand-ins.md`](psp-stand-ins.md); the PSP-only round,
  [`psp-only-round.md`](psp-only-round.md), group SI; the dropped tone,
  [`psp-music-banks.md`](psp-music-banks.md) 5, group MB)
- **Tier:** Sensible - the release the player owns is the base when it is the
  only one; nothing of the PlayStation's is there to fall back on.
- **Original behaviour:** the PC port ships the PlayStation's data in `DAT/`
  with its own edits. A cache built from a PSP disc alone had no source for
  about 270 `base/` chunks the PSP does not carry in the PC's form and wrote
  519 of 741 containers (`psp-only-build.md` 8). The plan's data policy
  ([`unified-data-plan.md`](unified-data-plan.md) 7) keeps a later release's
  content off by default.
- **New behaviour:** when every source is a PSP disc (`base_policy = "psp"`),
  each `base/` chunk no source carries is the PSP's own section at the same
  place: its area pages, palettes, map bands and UI page (206 containers),
  Ryu's form data (P8: `RYUD00..03` with the port's `0x7ACE` byte, 14 arenas,
  and `BPLD27A` with the port's hold), PSP-EU's own language plates and pages
  (22), the PSP's dial page where the port drew its legend (16), and the 11
  port-edited arenas and `START` as the PSP ships them (the PSX disc's
  decode, so DIV-0096 describes them unchanged). `BPLU27A` and `FIRST`'s
  icons are the PC's bytes, by the port's rules. **One exception, the
  dropped tone:** the effects bank shared by 16 battle containers (`BPLD` /
  `BPLU` 015, 025, 056, 257, 457, 567, 578, `BRTD` / `BRTU` 457) declares on
  the PSP one silent tone for a chord the PlayStation and the PC sound with
  a second tone past the count (sample 5); the PSP-only base restores that
  tone by a rule read from the PSP's own sections (a chord one slot past a
  program whose declared tones are all silent, and exactly one sample no
  tone references), so the 16 chunks are the PC's byte for byte and the base
  plays the PlayStation's chord. The PSP's one tone is an option (DIV-0113).
  With the PC's `DAT/` or a PSX disc among the sources nothing changes
  (`base_policy = "psx"`).
- **Rationale:** the owner's call of 2026-10-10 (`psp-only-build.md` 7, call
  1): for a PSP-only player there is no PlayStation original to fall back
  on, so the PSP's content changes are the base, and the PlayStation's look
  is the option a PSX disc enables. This inverts the plan's data policy for
  PSP-only installs only. The chord: the PSP's converter kept a program's
  declared tones only and lost a sound that was meant to play; the owner
  (`psp-only-round.md` 7.1 item 8): keep the rule, the PSP's one tone a
  toggle.
- **Also in the PSX version?** No: the PSX's own data is the PC's base; these
  are the PSP's sections. The chord: the PSX plays both tones; whether the
  PSP plays the second is unchecked.
- **Verification:** `importer.py build` and `verify` from each PSP disc alone:
  725 of 741 containers with SI's stand-ins (253 stand-ins on PSP-JP, 275 on
  PSP-EU), 0 problems; with MB's rule `se_check.py` gives 901 of 901 banks
  equal to PSX-JP's chunks on each PSP (885 before), the rule firing on the
  16 groups only; MS stage B, on the merged tools, 741 `base` containers. The
  PC and PSX paths byte-identical (JP + PC 2,125 files, US 1,872, Europe
  1,864) but for the manifest's new `[build]` table. **Not seen:** the PSP's
  area art, UI page, dial and title pages and Ryu's forms as our engine draws
  them, and whether a tile the PSP blanked shows as a hole (the end batch's
  captures, the owner's eye).
- **Reversible?** Add a PSX disc to the sources (`base_policy = "psx"`), or
  the PC's `DAT/`.

  **Amended 2026-10-12, evening (the PSP-only round's group VF,
  [`visual-findings-1012.md`](visual-findings-1012.md) 4 and 8; WS's survey,
  [`psp-widescreen-placements.md`](psp-widescreen-placements.md) 5 (c)):
  the PSP's base without its 16:9 re-framing.** Of the widescreen values the
  PSP keeps in data, those that reach a PSP-built image of `.data` are put
  back to the 4:3 frame at import by the PSP's rule undone, so the image
  equals a PSX-built one there and any widescreen anchoring is DIV-0041's,
  in code: `MsgBox_PlacementTable` entries 3..6, the side placements, which
  the PSP moved 32 toward its wider frame's edges (`unwide`: x + 32 left of
  the centre, x - 32 right of it), and the dialogue box tables `kBoxXY` /
  `kBoxAltXY` (`boxxy`), which `BOOT.BIN` holds as s16 pairs framed for its
  384-wide view, read back with entries 3..6 undone (3 and 5 x + 32, 4 and 6
  x - 32) into the PC's u8 layout. The values come from the player's
  `BOOT.BIN`; a recipe row is written only when the result equals
  `BOF3.exe`'s. The battle tile size entry 11 is already the PC's in the PSP
  image; the ending's letterbox is code. Kept as the PSP's: `Title_Sprites`
  entry 10, a source rectangle into the PSP's own title page, which the
  install also takes. Verification: `psp_exe.py recipe` ("0 not BOF3.exe's"
  on both PSP builds); the PSP-EU image equals the PSX-EU one at
  `0x66AE1C..0x66AE2A`; the exe host self-test with the PSP-EU image. Not
  rendered: the PSP-EU message boxes are not yet re-run (they sat at the top
  left before `boxxy`).

  **Amended 2026-10-18 (the install-gap round, group PB,
  [`install-gap-twins.md`](install-gap-twins.md) 5.3 and 5.6):
  `Sprite_ScreenMargins` on a PSP-built image.** *Original:* a PSP-only image
  held the eight bytes `0x663B28..0x663B30` as a `gap` - no file of either PSP
  disc holds them verbatim - so `Sprite_ProjectA`'s cull tested every sprite
  against zero margins, flagged any sprite outside the bare 320 x 240 frame
  off-screen and kept it off `Sprite_DrawList`. The gap watch saw the row read
  about 95,000 times over PSP-EU's five routes ([`install-gap-reads.md`](install-gap-reads.md)
  5 item 3). The PSP's own copy is in `BOOT.BIN` (PSP-EU `0x2E5398`, PSP-JP
  `0x2E5B80`, read through one relocated pair by the twin of
  `Sprite_ProjectA`). It is the PC's table with both x margins of each entry
  32 larger: the PSP's 32 columns either side of its 384-wide view. *New:*
  the rule `margins` (`tools/psp_exe.py`, PSP builds only) finds that copy as
  `boot_place` finds a twin's and writes x - 32 and y as read. It refuses
  loudly if a margin would go under zero, and writes a row only when the
  result equals `BOF3.exe`'s, which it does on both PSP discs (one row each,
  8 bytes, "0 not BOF3.exe's"). *Why:* the rule of `unwide` and `boxxy`
  above, the PSP's base without its re-framing; DIV-0041's wide modes read
  the PC's margins, so the PC's table is what our engine expects.
  (`psp-widescreen.md` 3b says the PSP left its sprite culls alone: it left
  their code, and moved this one table.) Verification: PB's regeneration over
  the seven discs and the round's again at `0baf26f7`; `exe-gaps.tsv` loses
  the row on both PSP builds; `tools/exe_scan.py` lists no byte of the symbol
  off the PC's; the exe host's self-test from a PSP-EU and a PSP-JP cache
  exits 0 (10,088 and 10,087 ours). Not rendered. *Reversible:* drop the two
  `margins` rows from `recipes/exe-psp.tsv`.

### A PSP-built image of `.data` carries the PSP's level table and consumable 87

- **ID:** DIV-0104
- **Date:** 2026-10-12
- **Subsystem:** the exe image (`tools/psp_exe.py`, `tools/exe_tables.py`
  `build_from`; the manifest's `[build] exe_tables_from`;
  [`psp-image.md`](psp-image.md); the PSP-only round, group IM)
- **Tier:** Extension - a balance change (roster index 6's level table) and
  an item flag, taken as the base of a PSP-only install.
- **Original behaviour:** `BOF3.exe`'s `.data` holds the PlayStation's
  `Char_ExpTable` (every PSX build equal) and consumable 87 as the
  PlayStation has it.
- **New behaviour:** an image built from a PSP disc (`exe_tables_from` a PSP
  build: a PSP-only install) holds `BOOT.BIN`'s: roster index 6's 99 rows of
  `exp_table` (249 bytes: the EXP and stat-gain bytes, not the learnt
  abilities) and consumable 87's one differing byte. With a PSX disc or the
  PC's exe first, nothing changes. Beside a PlayStation base the same bytes
  are an option (DIV-0111).
- **Rationale:** the owner's call of 2026-10-10, the PSP's data as the base
  of a PSP-only install ([`psp-only-build.md`](psp-only-build.md) 7).
- **Also in the PSX version?** No: these are the PSP's changes; every PSX disc
  holds the PC's values.
- **Verification:** `exe_tables.py measure` names exactly these two among the
  tables (`exe_table differs 249 bytes in records 629..`, `consumables
  differs 1 bytes in record 87`); CD's diff found nothing that stops our code
  running on either ([`psp-code-diff.md`](psp-code-diff.md) 6). Not played:
  a level-up of roster index 6 is the check.
- **Reversible?** A PSX disc among the sources, or the PC's exe.

### A PSP-only install plays the PSP's music: its sequences and its banks

- **ID:** DIV-0105
- **Date:** 2026-10-12
- **Subsystem:** music (`tools/ppms.py`, `tools/vag.py` `psp_music_vab`, the
  importer's `base/bgm/` from a PSP source; played by DIV-0087's sequencer
  unchanged; the Jukebox's PSP source; [`psp-music.md`](psp-music.md),
  [`psp-music-banks.md`](psp-music-banks.md); the PSP-only round, groups MS
  and MB)
- **Tier:** Sensible - the release the player owns; its re-authored music is
  what it plays.
- **Original behaviour:** the PC port plays its MP3s. With a cache (DIV-0087)
  the songs are the PSX disc's sequences on the PSX's banks, through
  `psx::MusicSynth`. The PSP re-authored all 166 songs as Standard MIDI
  Files (`pPMS`): 73 carry the PSX's events exactly; the others differ in
  notes (16), controllers (11), pitch-bend resets (56), the setup of song 31,
  event order within a tick (5), the rhythm field (4) and two tempos (22,
  157). Its music banks merge each tone's volume with its program's and the
  VAB master's (`(tv * pm * bm * 32) // 127**2`), and its pan is
  `(tp + mp + 64) // 3`, a third of the PSX's spread.
- **New behaviour:** when the player gives a PSP disc and no PSX disc, the
  importer writes `base/bgm/` from the PSP: the songs inverted to the cache's
  song form (format only - every note, bend and controller is the PSP's),
  and the banks with the merge inverted: master and program volumes 127,
  each tone's volume the merged one rounded, program pans 64, each tone's pan
  `3p - 128`; samples, pitch, range, ADSR and mode the PSP's, which are the
  PSX's. The sequencer plays them unchanged. The Jukebox lists them as the
  PSP disc's.
- **Rationale:** the owner's call (`psp-only-build.md` 7, call 2): a PSP-only
  install uses the PSP's audio as it is; there is no PSX original to fall
  back on. Of the merge, the inverse undoes what the conversion lost, where
  playing the squeezed pan as it is would narrow the image; the owner
  (`psp-only-round.md` 7.1 item 6): the restored stereo width is fine, no
  narrow-pan form wanted.
- **Also in the PSX version?** No: the PSX plays its own sequences and the
  unmerged bank fields. The PSX driver neither resets nor remembers a bend at
  key-on (`_SsVmKeyOn` -> `note2pitch`, disassembled), so the PSP's 728
  centre resets after a real bend retune voices still sounding: the bend-only
  songs are real differences, not encoding.
- **Verification:** `ppms.py check`, `vag.py check`; `importer.py verify`
  against `fixtures/bgm-psp.tsv` (247 of 247, both PSPs); the song A/B on the
  PSX banks: 80 of the 93 songs that are not byte-identical differ within
  180 s, 50 of them bend-only (`psp-music.md` 4.2); the banks against the
  PSX-built: 166 songs, SNR median 41.9 dB, one song at 16.8 dB (22, whose
  bank has its own program pans), level within half a decibel
  (`psp-music-banks.md` 3); all 166 render on the PSP banks with no abort.
  **Not heard** (the owner's ear: the 16 note songs, 22 and 157, 31).
- **Reversible?** A PSX disc among the sources builds the PSX's songs and
  banks; `BOF3X_MUSIC=mp3` has no MP3s to play on a PSP-only install. Beside
  a PlayStation base the PSP's music is an option (DIV-0109).

### A PSP-only install has the PSP's effects, voices, jingles and logo movie

- **ID:** DIV-0106
- **Date:** 2026-10-12
- **Subsystem:** sound streams and FMV (`tools/at3.py`, `tools/psp_fmv.py`,
  `importer.py build` and `install --base`; read by DIV-0089's
  `Sound_LoadStream` seam and DIV-0099's `Fmv_Play` path unchanged;
  [`psp-install.md`](psp-install.md); the PSP-only round, group PN)
- **Tier:** Sensible - the release the player owns, decoded by the player's
  own ffmpeg.
- **Original behaviour:** the PC port plays its own `SND\` (875 effects and 5
  voices as 22.05 kHz mono WAVs, the 11 jingles as bare 128 kbit/s MP3s) and
  its own `CAPCOM.AVI` (DIV-0001's Cinepak, 15 fps, 112 frames). A PSX
  disc-only install has the disc's exact effects, no jingles and the disc's
  movie (DIV-0099).
- **New behaviour:** an install built from a PSP disc alone (`importer.py
  build` with no PSX disc and no PC source, then `install --base`) has:
  the 880 effects and voices decoded from the PSP's ATRAC3plus by the
  player's ffmpeg, each cut at its measured onset (shifted up to 250 ms to
  where the PC's clip starts) and padded to the PC file's length; the 11
  jingles, which a PSX-only install lacks, re-encoded from the PSP's
  ATRAC3plus to bare 128 kbit/s stereo MP3s in the PC's form (2 or 3 frames
  longer than the PC's); and the PSP's logo movie (`PSMF/CAPLOGO.PMF`, H.264
  480 x 272 with ATRAC3plus) re-encoded at import as `CAPCOM.AVI`: Cinepak
  640 x 480 (the PSP's frame scaled back to the PlayStation's 4:3 by
  lanczos), 30000/1001 fps, 231 frames, 7.708 s, PCM 44.1 kHz stereo aligned
  by the stream's time stamps (ffmpeg has no PSMF demuxer: the video by its
  MPEG-PS read, the audio by our demux). Without ffmpeg none of them is
  written, with the reason in the build's output and the manifest; the
  install then has no `SND\` effects and no movie (DIV-0100 starts it).
- **Rationale:** the owner's calls of 2026-10-10 (`psp-only-build.md` 7: a
  PSP-only install uses the PSP's audio as it is; the effects decoded by the
  player's own ffmpeg, nothing vendored, since no permissive ATRAC3plus
  decoder exists, [`LICENSING.md`](LICENSING.md) 4) and of 2026-10-12 (the
  logo by the player's ffmpeg at import, as DIV-0099). Rule 1: a PSP-only
  install carries no PC file.
- **Also in the PSX version?** No: the PSX plays its XA and MDEC; these are
  the PSP release's lossy re-encodes, and ours of them.
- **Verification:** built from both PSP discs (byte-identical outputs);
  against the PSX's XA the effects correlate at median 0.998 (min 0.990),
  SNR median 24.9 dB (16.9 .. 30.0), the voices 19-25 dB, the jingles 0.995-0.9996
  and 20-31 dB; the movie's Cinepak loss SSIM 0.868 / PSNR 34.6 dB against
  the direct decode; `verify` 0 problems; the exe host's headless self-test
  from the PSP-EU install exit 0 (`psp-install.md` 1-5). **Not heard, not
  seen**; MCI not run on this movie (30000/1001 fps, where DIV-0099's was 30).
- **Reversible?** A PSX disc among the sources gives the PSX's exact files
  instead; a PC install's own files are never replaced. Beside a PlayStation
  base the PSP's effects and jingles are an option (DIV-0110).

### The launcher offers three suites, the PSP's or the PlayStation's, in place of the PSP extras

- **ID:** DIV-0107
- **Date:** 2026-10-12
- **Subsystem:** launcher (`src/launcher/suites.cpp`, `config_dialog.cpp`,
  `launcher.rc`; `recipes/suites.toml`, `tools/suites.py`;
  [`launcher-suites.md`](launcher-suites.md); the PSP-only round, groups LT
  and TG)
- **Tier:** Sensible - a choice of which release's presentation and data to
  play, made before the game starts; what the suites move is the layers
  (DIV-0086, DIV-0109..0113) and the PSP's battle rules (DIV-0108), off
  beside a PlayStation base unless chosen.
- **Original behaviour:** the 2001 port has no settings program (`START.EXE`
  is an autorun shell, [`launcher-settings.md`](launcher-settings.md) 1) and
  carries no PSP content. Until this entry our launcher offered DIV-0086's
  layers as a "PSP extras" box per installed layer.
- **New behaviour:** the settings dialog lists three suites from
  `recipes/suites.toml`: graphics (`psp-art`, `psp-tiles`, `psp-maps`), audio
  (the PSP's music; its effects, voices and jingles) and data (the level
  table and item 87; the English and Japanese renames; Ryu's forms, P8; the
  PSP's two battle rules). Each is the PlayStation's version or the PSP's,
  chosen whole, whatever the install. A side the cache's `manifest.toml`
  cannot provide is greyed, naming the discs that would enable it. Every
  member is a layer beside a PlayStation base (`recipes/opt.toml`'s, or
  `recipes/psp-layers.toml`'s: the music, the effects, the exe tables, P8 -
  DIV-0109..0112) or a code member (DIV-0108's `psp=`); a layer not in the
  cache is greyed with `build with --opt NAME`. Under the manifest's
  `base_policy = "psp"` it inverts: the PSP's is the base, and the
  PlayStation's is greyed, naming a PlayStation disc. The choice is written
  as `opt=`'s layers and `psp=`'s rules (no other new ini key); an untouched
  page writes both lines back byte for byte, and `music=` keeps its meaning.
  `opt=` takes up to 16 layers (8 before); an `opt=` written by the old
  boxes reads as a partial suite, kept as written.
- **Rationale:** the owner's calls of 2026-10-10
  ([`psp-only-build.md`](psp-only-build.md) 7): three toggles, every option
  listed, greyed by source; a PSP-only install the other way round. The
  membership as LT proposed it (the owner, `psp-only-round.md` 7.1 item 5):
  the name layers and P8 under data, the button-label page left out.
- **Also in the PSX version?** No: the PlayStation release has no launcher
  and no PSP content.
- **Verification:** the launcher host suite (`config_tests.exe`, 137 checks
  with PB's code members, 0 failures); `dialog_test.py`'s three fake caches
  (35 checks at LT's merge); `suites.py check` in `importer.py check`. **Not
  seen:** the page on the owner's display (the end batch's captures).
- **Reversible?** Each suite's PlayStation side, or `opt=none` / an `opt=`
  list and `psp=none` in `bof3x.ini`; a PSP base has no PlayStation side to
  choose until a PSX disc is added.

### The PSP's two battle rules, behind a switch

- **ID:** DIV-0108
- **Date:** 2026-10-12
- **Subsystem:** battle (`src/game/psp_code.*`; `BattleAction_End`
  `0x430010`, `BattleRoundEnd_NextRound` `0x431090`, `Battle_PlaceBossActor`
  `0x494570`; `BOF3X_PSP` / `bof3x.ini` `psp=`; the data suite's two code
  members, `recipes/suites.toml`; [`psp-battle-changes.md`](psp-battle-changes.md);
  the PSP-only round, group PB, from CD1's reading
  [`psp-code-diff-battle.md`](psp-code-diff-battle.md))
- **Tier:** Extension - the PSP's own changes to play, taken as options. C1
  may turn out to be Intent (a repair of a PlayStation fault) once the owner
  has played the fight.
- **Original behaviour:** the PC port and the PlayStation end a battle
  whenever the battle-end byte `0x904AE8` is non-zero, in the turn step
  (`BattleAction_End`, PSX `0x801D43BC`) and the round start
  (`BattleRoundEnd_NextRound`, PSX `0x801D653C`); and a boss actor placed as
  an enemy (`Battle_PlaceBossActor`, PSX `0x800A8F4C`) keeps the enemy data's
  first item word `+0xA8` (the item a steal takes and the first drop rolls).
  The PSP's `BOOT.BIN` adds, in both releases: **(C1, `duel-loss`)** in event
  battle 0x1B - the duel between Beyd and Zig, chapter 6 run 9
  (`Scena06_Run09` starts it), area 92 - an end byte with bit 0 set and bit 2
  clear does not end the battle (PSP-JP `0x11DFA4` and `0x11F36C`; PSP-EU
  `0x11DC14` and `0x11EFDC`); **(C2, `myria-item`)** in area 198 under event
  battle 0x37 (set-up 55, which our records label Myria) the placed enemy's
  item word becomes 0x317 after its data is copied (PSP-JP
  `0x141570..0x141598`, PSP-EU `0x14133C..0x141364`; the PSP's `+0x98` is
  ours `+0xA8`, its enemy data copy `0x141BA0` read field by field). 0x317 is
  accessory 0x17, "Ivory Charm" by the sibling's `names/items.toml`. Read
  with capstone ([`psp-battle-changes.md`](psp-battle-changes.md) 2; CD1's
  first reading took `+0x98` for agility, corrected there).
- **New behaviour:** both rules reproduced in our three functions under one
  switch item per rule, `BOF3X_PSP` / `psp=` (`duel-loss`, `myria-item`,
  `none`). Unset, both are on when the cache's manifest says `[build]
  base_policy = "psp"` (a PSP-only install) and off otherwise. The
  launcher's data suite carries them as two code members, and its choice
  writes `psp=`.
- **Rationale:** the owner, 2026-10-12 (`psp-only-round.md` 7.1 items 2 and
  3): both "worth keeping as a toggle in the launcher psx vs psp area -
  leaving them as default for psp-only installs makes sense". A PSP-only
  install plays as the PSP did; any other plays as the PlayStation and the PC
  did, unless the player chooses the PSP's.
- **Also in the PSX version?** No: the PSX twins have neither test (CD1, the
  constants 0x1B-with-the-end-byte and 791 searched in every function of
  both sides). C1's PSX twin of `Boss27_Event` is uncaptured, so whether the
  PSP's C1 repairs a PSX fault is open (the owner's, in play).
- **Known and accepted:** with `duel-loss` on, a party loss in the duel lasts
  until `Boss27_Event`'s other path sets bit 2 (`boss_sf.md`'s reading): a
  scripted end or a stall, to be judged in play. Only
  `Battle_PlaceBossActor` gets C2; `Battle_SetupEnemy` has no store on
  either side.
- **Verification:** the shadows off and on (`battle_actions`,
  `battle_turn_steps`, `rest_3g` with the real callees: 0 mismatches; 3 of 3
  planted faults refused); the switch's six cases headless; the hosted `'*'`
  narrow exit 0; the exe host on a PSX disc-only and a PSP-only install exit
  0 (`psp-battle-changes.md` 4). **Not seen in play:** the duel lost with
  `duel-loss` on, and what Myria carries with `myria-item` on.
- **Reversible?** `BOF3X_PSP=none` or `psp=none` turns both off; with both
  off the three functions are the original's byte for byte (the battle
  modules' shadows, off and on).

  **Amended 2026-10-12, night:** what the PlayStation holds in Myria's item
  word, read from `AREA198.EMI` section `0x800E4000` (record 0, enemy data
  `+0x2C`) on the US and JP discs: 0x0017 with class byte 1, consumable 0x17,
  "Napalm" by the sibling's names - so the PSP keeps the index and changes
  only the category to an accessory, "Ivory Charm" ([`psp-battle-changes.md`](psp-battle-changes.md)
  1; IDEAS I32 P17). The owner: keep it a toggle (`myria-item`), as built.

### The PSP's music as a layer beside a PlayStation disc's

- **ID:** DIV-0109
- **Date:** 2026-10-12
- **Subsystem:** music (`src/game/music_seq.cpp`, DIV-0087's
  `Music_LoadFile` seam; `tools/psp_layers.py`, `recipes/psp-layers.toml`;
  the Jukebox's PSP column, `src/launcher/music_player.cpp`;
  [`launcher-suites.md`](launcher-suites.md) 9; the PSP-only round, group TG)
- **Tier:** Sensible - a later release's content, off unless chosen
  (DIV-0086's rule for layers).
- **Original behaviour:** the port plays its MP3s; with DIV-0087, the cache's
  `base\bgm` (one disc's songs).
- **New behaviour:** with `opt=` naming `psp-music` (the audio suite's PSP
  side), a track is the layer's `opt\psp-music\bgm\NNN.DAT` on its own
  `bank\` when it has one, else `base\bgm`'s, else the MP3. A song's bank is
  held by its file path, not its name (the PSP's banks are named as the
  PSX's). Arming checks every song and bank of the layer at start-up as it
  checks `base\bgm`, and no longer needs `base\bgm` when a named layer has
  songs; a root too long for the layer's bank paths is a Fatal naming the
  layer. `BOF3X_MUSIC=mp3` still plays every MP3. The Jukebox lists the
  layer's songs as the PSP source beside a PlayStation disc's.
- **Rationale:** the owner's calls of 2026-10-10 (three suites, the PSP's
  audio a choice beside a PSX base); the songs are DIV-0105's.
- **Also in the PSX version?** No: the PSP's own sequences
  ([`psp-music.md`](psp-music.md): 93 of 166 differ).
- **Verification:** the DIV-0109 self-test of `BOF3X_SHADOW=sound` (a named
  layer's song on its own bank before `base\bgm`'s; a base song on a bank
  named as the layer's: the control); the exe host on a PSX disc-only
  install with the layer: its 166 songs and 81 banks checked, exit 0. **Not
  heard.**
- **Reversible?** The audio suite's PlayStation side, or `opt=` without the
  layer.
- **Amended 2026-10-21 (DIV-0150):** the layer is no longer the audio suite's:
  the launcher's Music box chooses the music source, and under a chosen source
  (`BOF3X_MUSIC` psx, psp or pc) the engine reads `opt\psp-music\bgm` by that
  choice whether or not `opt=` names it. With `BOF3X_MUSIC` unset or `seq`,
  this entry's rule holds as written.

### The PSP's effects, voices and jingles as a layer

- **ID:** DIV-0110
- **Date:** 2026-10-12
- **Subsystem:** sound streams (`src/game/dat_cache.cpp` `SoundPath`,
  DIV-0089's `Sound_LoadStream` seam; [`launcher-suites.md`](launcher-suites.md)
  9; the PSP-only round, group TG)
- **Tier:** Sensible - a later release's content, off unless chosen.
- **Original behaviour:** `SND\NAME.DAT` (with DIV-0089, `base\snd` first).
- **New behaviour:** with `opt=` naming `psp-snd`, the named layers with a
  `snd` part, last first, then `base\snd`, then `SND\`: the layer's
  `opt\psp-snd\snd\NAME.DAT` holds the PSP's ATRAC3plus clips as decoded by
  the player's ffmpeg (DIV-0106), and the 11 jingles - which a PSX-only
  install otherwise lacks. A layer without a file leaves it to the one under
  it. Over a PC install the cache's files win where both exist (DIV-0089),
  so the PC's own MP3 jingles are replaced by the PSP's while the layer is
  on.
- **Rationale:** as DIV-0109; DIV-0106 is the same audio as a PSP-only base.
- **Also in the PSX version?** No.
- **Verification:** the DIV-0110 self-test of `BOF3X_SHADOW=dat_cache` (two
  named layers, the top layer's stream, the one under it, an unnamed
  layer's refused); the exe host with the layer, exit 0. **Not heard.**
- **Reversible?** The audio suite's PlayStation side, or `opt=` without the
  layer.

### The PSP's level table and consumable 87 laid over .data

- **ID:** DIV-0111
- **Date:** 2026-10-12
- **Subsystem:** the exe host's data mapping (`src/hook/exe_image.cpp`
  `ExeImage_LayOptTables`, `exe_host.cpp` `CopyData`;
  [`launcher-suites.md`](launcher-suites.md) 9; the PSP-only round, group TG)
- **Tier:** Extension - a balance change (roster index 6's level table,
  [`opt-layers.md`](opt-layers.md) 3), chosen by the player; DIV-0104 makes
  it a PSP-only install's base.
- **Original behaviour:** `.data` as `BOF3.exe` (or the disc-built image)
  holds it: the PlayStation's tables.
- **New behaviour:** with `opt=` naming `psp-tables`, the exe host lays
  `opt\psp-tables\exe\place.bin` over `.data` at map time, after the image
  is in place and filled and before it is compared with the file: every
  record of `exp_table` and `consumables`, names left out, from the
  PSP-built image. Over the PC's, the US disc's and the European disc's
  images it changes exactly 250 bytes: 249 of roster index 6's rows and 1 of
  consumable 87. Refused with a Fatal naming the file: a `place.toml` not in
  `psp_layers.py`'s shape, ranges out of order or outside `.data`, a byte
  count that differs, a `place.bin` whose sha256 is not the toml's, and,
  over a disc-built image, a range on a class that must equal the PC's. The
  hosted build refuses the layer (its `.data` is Capcom's, the A/B and
  shadow reference).
- **Rationale:** the owner's data suite; the bytes are DIV-0104's.
- **Also in the PSX version?** No: the PSP's changes.
- **Verification:** `exe_cmp.py` (the layer laid as the exe host lays it,
  against the PSP-built image: 250 bytes change, 0 outside the ranges); the
  DIV-0111 self-test on synthetic files (5 controls refused); the exe host
  over the PC's `.data` and a disc-built one: `250 of them differ`; a
  flipped byte of `place.bin`: exit 3. **Not played:** a level-up of roster
  index 6.
- **Reversible?** The data suite's PlayStation side, or `opt=` without the
  layer.

### Ryu's forms (P8) as a layer

- **ID:** DIV-0112
- **Date:** 2026-10-12
- **Subsystem:** data (`LoadDatFile`, DIV-0086's walk; no new code;
  `recipes/psp-layers.toml`'s `psp-ryu-forms`; the PSP-only round, group TG)
- **Tier:** Sensible - presentation and form data the PSP changed; CD settled
  that our code runs it ([`psp-code-diff.md`](psp-code-diff.md)).
- **Original behaviour:** the PlayStation's arenas and `RYUD` sections (the
  PC's chunks).
- **New behaviour:** with `opt=` naming `psp-ryu-forms`, the 19 chunks a
  PSP-only base stands in (DIV-0103): `RYUD00..03` with the port's byte, 14
  type-1 arenas, `BPLD27A` with the port's hold, each at the PC's kind, tag
  and size. The arenas' tag resets `Gfx_UploadQueueCount` again, which
  changes nothing: in each of the 19 containers only banks follow the arena.
- **Rationale:** the owner's data suite (P8 under data, `psp-only-round.md`
  7.1 item 5).
- **Also in the PSX version?** No.
- **Verification:** `verify_layer` (each chunk against the PC's kind, tag and
  size and SI's hash); PSP-JP and PSP-EU give identical layers; the hosted
  self-test with the layer named, exit 0. **Not seen:** Ryu's forms in
  battle.
- **Reversible?** The data suite's PlayStation side, or `opt=` without the
  layer.

### The battle chord as the PSP plays it, and a layer's banks in Snd_LoadBankFile

- **ID:** DIV-0113
- **Date:** 2026-10-12
- **Subsystem:** sound banks (`src/game/save_menu.cpp` `Snd_LoadBankFile`,
  `dat_load.cpp` `DatLoad_OptLayerChunks`); the launcher's Jukebox
  (`music_player.cpp`, `suites.cpp` `ChordEvaluate`, `launcher.rc`);
  [`launcher-suites.md`](launcher-suites.md) 9.6; the PSP-only round, group
  TG)
- **Tier:** Sensible - a choice, off by default.
- **Original behaviour:** `Snd_LoadBankFile` loads a container's banks from
  the shipped file only, so a layer's bank applied when `LoadDatFile` loaded
  the container and not when a scene loader asked for its banks. The
  PlayStation's (and the PC's) battle effects bank in 16 containers keys a
  chord of two tones; the PSP's drops the second, and every base plays the
  restored chord (DIV-0103).
- **New behaviour:** `Snd_LoadBankFile` also loads the kind-2 chunks of each
  named layer's file for the container, after the shipped file
  (`LoadDatFile`'s precedence; `Snd_LoadBank` frees and replaces a slot's
  bank as when the original loads one over another), only when a named layer
  carries a bank at all (read at injection, logged; with none, the function
  is the original's). The layer `psp-chord-one-tone`, built whenever a PSP
  disc is a source on any base, holds those 16 banks as the PSP plays them;
  the Jukebox's box "Battle chord as the PSP plays it (one tone)" puts it in
  `opt=`. Off by default; greyed with the reason when no cache is named, the
  cache has no PSP source, or it lacks the layer.
- **Rationale:** the owner, 2026-10-12 (`psp-only-round.md` 7.1 item 8): the
  dropped tone toggle-able, on the Jukebox, the restored chord the default.
- **Also in the PSX version?** No: the PSX plays two tones.
- **Verification:** the launcher host suite's `ChordEvaluate` (six states);
  `verify_layer` (each chunk against a bank tag of its container, the
  one-tone form different from the PC's chunk); the hosted `'*'` and the
  exe host with the layer named, exit 0 (a first build without the bank gate
  crashed the hosted run in `Snd_LoadBankFile`'s shadow fuzz, fixed before
  the merge). **Not heard;** the Jukebox does not play it (an effect, not a
  song).
- **Reversible?** The box, or `opt=` without the layer.

### A disc-only install draws the disc's title page, not the PC port's

- **ID:** DIV-0114
- **Date:** 2026-10-12
- **Subsystem:** art (the title page `0x1A080400` of `DEMO.DAT` /
  `FIRST.DAT`, `Title_Sprites`; the importer's base from a disc;
  [`visual-findings-1012.md`](visual-findings-1012.md) 8, the harness's
  `pc-title-mark` rule, [`abc-harness.md`](abc-harness.md))
- **Tier:** Sensible - the release the player owns; recorded so that the
  port's own change has a ledger answer.
- **Original behaviour:** the PC port (2001) draws the **Japanese**
  PlayStation's title unchanged: the copyright page `0x1A080400` of
  `DEMO.DAT` is byte-identical to the JP and European-English discs' (the US
  disc's own adds a Capcom U.S.A. line, 5,062 bytes in 9 tiles; `FIRST.DAT`'s
  page at that tag is every disc's); the registered mark is `Title_Sprites`
  entry 1 (10 wide) on the glyph page `FIRST.DAT` `0x1E000200`, both JP's.
  The US and European discs draw a ™ there (entry 1 12 wide, their own page).
  Not the porting house's change
  ([`disc-data-inventory.md`](disc-data-inventory.md) 4.1, corrected
  2026-10-13).

  *Corrected 2026-10-13.* This paragraph first said the page and the (R)
  were the port's own and that "every disc has 12" for entry 1's width; the
  inventory pass rendered each source's page and read each disc's
  `Title_Sprites`: the JP disc and PSP-JP have the PC's 10 and draw the (R).
  The owner's call the same day (inventory Q4): **a PC install keeps Japan's
  (R) title** - "the JP psx disc makes most sense to use as default for
  graphics if there's ever a simple divergence, just for consistency sake"
  (the base policy, DIV-0115) - so no Western-title entry is written.
- **New behaviour:** a disc-only install - a cache with no PC source - draws
  the disc's title page from the disc. A Western disc's install draws **no
  mark at all** (neither the PC's (R) nor its own ™), because `Title_Sprites`
  entry 1's width (`0x65385E`) is a gap of the disc-built image (zero); once
  that gap is filled it draws the disc's ™. *Filled 2026-10-20* by DIV-0101's
  `titlemark` rule (D266): a Western disc-only install draws its disc's ™. A JP disc-only install places the
  width and draws the (R), as the JP release does. A PSP-only install
  draws the PSP's own title page and `Title_Sprites` entry 10 (DIV-0103).
  A PC install, or a cache with the PC's `DAT/` among its sources, keeps the
  PC's title (Japan's).
- **Rationale:** rule 1 - a disc-only install carries none of the PC's
  files, and the disc's own title is what that release shows. Whether a PC
  install should draw the PlayStation's title was the owner's call, part of
  the disc-data preference inventory; answered 2026-10-13: it already does -
  Japan's, which stays the default (inventory Q4, see the correction above).
- **Also in the PSX version?** Yes: the PC's title is the JP disc's; a
  Western disc's title differs by region (™, and the US notice's third line),
  not by the port.
- **Verification:** AB's harness: the PC against the PSX-EU disc-only
  install differ on the title by the `pc-title-mark` region, classed by that
  rule; not ledgered before. The PSP title's framing in our 4:3 frame is
  open (D256).
- **Reversible?** Build with the PC's `DAT/` as a source.

### Prefer the Japanese PlayStation disc's data over a port change the owner has decided; the 27A arenas' held loop first

- **ID:** DIV-0115
- **Date:** 2026-10-13
- **Subsystem:** assets (`tools/importer.py` `preferred` / `get_chunk`, the
  new `recipes/prefer.toml`; first members `BPLD27A.DAT` and `BPLU27A.DAT`
  kind 0 `0x10000`; the engine reads the cache's `base/` since DIV-0089;
  [`disc-data-inventory.md`](disc-data-inventory.md) 2 row P6, 9)
- **Tier:** Sensible - the PlayStation's presentation where the port
  changed it for no reason the PC's platform gives; nothing changes in play.
- **Original behaviour:** the PC port's `base/` is the Japanese PlayStation
  release's except 35 chunks the port changed (inventory 0 and 2). One is the
  27A battle arenas: in member A's bank of `BPLD27A` and `BPLU27A`, the
  PlayStation has two identical four-step loops, both at 4 ticks a step; the
  port holds the second at 8 ticks a step, so it plays at half speed - 4
  bytes in each file (`0x41C88..0x41C8E`, `0x42E60..0x42E66`;
  [`arena-edits.md`](arena-edits.md) 2). Measured again 2026-10-13 (session
  scratch `policy/hash27a.py`): the type-1 decode of `BPLCHAR/BPLD27A.EMI#7`
  and `BPLU27A.EMI#7` is byte-identical on all five PSX discs (JP, US,
  EU-English, FR, DE) and differs from the PC's chunk in exactly those 4
  bytes. Before this entry every cache took the port's bytes, a disc-only one
  included: the importer's `hold` rule rebuilt them from any PSX disc. Which
  animation the loop is was not read or rendered.
- **New behaviour:** `recipes/prefer.toml` lists the chunks whose disc form
  the base policy takes. For each row, when a disc the row names (`from`) is
  among the build's sources - in any position of the player's order - the
  importer takes that disc's section through the row's transform, hashes it
  against the row, and writes it in `base/` ahead of the PC's chunk; the
  manifest records it as `<build>:prefer:<EMI#section>`, `verify` checks it
  against the row, and `check` checks the table's shape against the recipe.
  The first two rows: the 27A arenas from `psx-jp` (`type1`), so both loops
  play at the disc's 4 ticks. Where no named disc is a source nothing
  changes: a PC + US cache takes the PC's chunk (the US disc carries the same
  decode, but the row names Japan's), and a US disc-only install the `hold`
  rule's port bytes. A PC install with no cache is the port's.
- **Rationale:** the owner, 2026-10-13: "the JP psx disc makes most sense to
  use as default for graphics if there's ever a simple divergence, just for
  consistency sake". And on the 27A loop (inventory Q2), take the disc's: the
  PlayStation tied music playback to frame timing and the PC does not, so
  the port's half-speed copy cannot be a correct fix here. The policy is the
  mechanism; each further member is the owner's call on that chunk. Not
  members (2026-10-13): the 11 hand-edited arenas and Ryu's byte (Q1 / Q3,
  to be rendered first), the dock legend (Q5), the wrap clicks (Q6); the
  title needs none (Japan's is the PC's, DIV-0114 as corrected).
- **Also in the PSX version?** Yes: this is every PSX disc's loop.
- **Verification:** `importer.py build` from the PC's `DAT/` and `BOF3.exe`
  with the JP disc (`c_pcjp`), with the US disc (`c_pcus`), and with US then
  JP (`c_pcusjp`), each `verify` 0 problems
  ([`disc-data-inventory.md`](disc-data-inventory.md) 9.2): with the JP disc
  the two chunks are `psx-jp:prefer:BPLCHAR/BPL?27A.EMI#7` at the disc's
  hashes and 740 of 742 containers stay the PC's; with US alone both are
  `pc-zh:chunk` and 742 of 742 are the PC's. `get_chunk` over six source
  orders (scratch `quick27a.py`); planted faults (scratch `neg27a.py`): a
  wrong hash refused by the build, the PC's hash / another section / a PSP
  build / no such chunk / an unknown transform refused by `importer.py
  check`. `importer.py check`, `exe_tables.py check` 0 errors. Not played,
  not rendered: the loop has not been seen at either speed.
- **Reversible?** Build the cache without the JP disc among its sources, or
  with `importer.py build --prefer <a file with no rows>`.

### An encounter refuses a combatant left of or above the screen, as the PlayStation does

- **ID:** DIV-0116
- **Date:** 2026-10-13
- **Subsystem:** field (the random encounter's placement: `Encounter_OnScreen`
  `0x5928F0`, ours since round seven's group BI, `src/game/inventory_ops.cpp`;
  its one caller `Encounter_Place` `0x591F30`, `src/game/field_hidden.cpp`;
  [`d56-encounter-bounds.md`](d56-encounter-bounds.md), known defect D56)
- **Tier:** Intent - a port defect; the PlayStation shows what was meant.
- **Original behaviour:** after the party and the enemies are placed and the
  camera aimed, `Encounter_Place` asks `Encounter_OnScreen` of each member
  (margin 0x20) and each active enemy slot (its kind's `+0x86` / `+0x87`): the
  footprint's four corners projected, refused if a screen x is 320.0 or more
  or a screen y 240.0 or more (x87 float compares). A corner at x < 0 or
  y < 0 passes. A member refused cancels the encounter for that step; an enemy
  refused drops its slot (no slot left cancels it). So on the PC a fight can
  start with an enemy partly or wholly off the left or top edge and never off
  the right or bottom.
- **PlayStation:** the twin, GAME.EMI section 0 `0x801C805C` (JP disc,
  `disc_ls.py --extract BIN/ETC/GAME.EMI`, capstone), loads each projected
  corner's x with `lhu $v0, 0x10($sp)` and tests `sltiu $v0, $v0, 0x140`
  (`0x801C80D4`, bytes `4001422c`), and y with `lhu $v0, 0x12($sp)` /
  `sltiu $v0, $v0, 0xf0` (`0x801C80E8`, `f000422c`): unsigned, so a negative
  short is refused too - x and y must lie in 0..319 and 0..239. Its callers
  `0x801C72A4` (a member, `a3 = 0x20`) and `0x801C7310` (an enemy slot; a
  refusal stores 0 to the slot's active byte and decrements `0x801CE050`)
  are the PC's `Encounter_Place` branch for branch. The PC's float compares
  kept only the upper bounds: the port's.
- **New behaviour:** a corner whose screen x or y is below 0.0 is refused
  too, so the PC's test is the PlayStation's (a NaN passes, as the upper
  compares let it). The test sits behind a byte flag that
  `InventoryOps_Inject` sets (`PatchBytes EncounterOnScreenLowerBounds`)
  after the module's self-test, whose fuzz compares the original's bounds
  (DIV-0065's pattern). Once the self-test is past, every corner below 0 is
  logged, `D56 Encounter_OnScreen(x, z, size, margin) corner k at (sx, sy):
  refused (DIV-0116)` - or "passed, the port's bounds" with the flag off - so a
  run shows where the two part; the log changes nothing the game reads.
- **Rationale:** the owner, 2026-10-13: build D56's repair, a restoration of
  the PC's lost guard. A combatant the camera cannot see is a fight the
  player cannot read (visual-findings-1012 1.4 shows the look: a name box
  for an enemy that is never drawn).
- **Also in the PSX version?** No - the PlayStation refuses such a corner;
  this entry restores it.
- **Cost:** any encounter whose placement put a corner left of or above the
  screen now loses that enemy (or the encounter for that step, and the walk
  goes on to the next due step), so the random draws and everything after can
  differ from that encounter on. Measured on nine battle routes: no route
  reaches such a placement, so none differs
  ([`d56-encounter-bounds.md`](d56-encounter-bounds.md) 3).
- **Verification:** `BOF3X_SHADOW='*'` headless self-test exit 0
  (`inventory_ops` 50,000 rounds, 0 mismatches, the flag set after it;
  `inject: 10087 ours`). The A/B and the state hash in the doc's section 3.
  A crafted capture (the combat route with `Camera_ShiftX` poked to -512
  before its encounter): with the flag off the Gooey is placed with all four
  corners at x -40 .. -62 and the fight starts with it off screen; with it
  on, that encounter is refused and the next one starts with one enemy on
  screen (`analysis/shots/d56_before`, `d56_after`, frame 500). Owed: the
  owner's eye in play.
- **Reversible?** `BOF3X_ORIGINAL=EncounterOnScreenLowerBounds` keeps ours with
  the port's bounds; `BOF3X_ORIGINAL=Encounter_OnScreen` runs Capcom's
  function.

### The Identify panel's small words: WEAK POINT on the weakness row, the disc's own item word and EXP label

- **ID:** DIV-0117 (an amendment of DIV-0064 group 15, written as its own
  entry because it adds a heading the group had decided against and changes
  a Japanese layer)
- **Date:** 2026-10-13
- **Subsystem:** battle, the Identify panel (`Identify_DrawMember` /
  `_DrawEnemy`, ours in `src/game/magic_s12.cpp`; DIV-0064 group 15 in
  `src/game/labels.cpp`, chunk kind 15 tag 15 of `tools/loc_build.py`;
  [`labels-1013.md`](labels-1013.md) 1)
- **Tier:** Sensible - the release the player owns draws these words; DIV-0064's
  group 15 rested on a reading that missed them.
- **Original behaviour:** the PC port draws the headings 弱点 and 持有物
  (`0x66A3E0` / `0x66A3E8`) centred on the weakness row (y `0x44`) and over
  the items (y `0x8A`), and the EXP label `0x65AAB0` with the count 3. Under
  a Latin overlay since DIV-0064 group 15 (2026-10-10): no headings, and the
  PC's own `ITEM` (`0x65AAB4`, read by nothing in the port) in the 8 px font
  at (`0x59`, `0x8E`), placed by eye off the owner's wiki capture, in every
  Latin language. Under a Japanese overlay the Chinese headings, and a
  target name cut at 5 glyphs.
- **New behaviour:** under every PlayStation overlay (en-US, en-150, fr-FR,
  de-DE, and now ja-JP) the overlay's group 15 carries six strings: the two
  headings' spaces (drawn as nothing, as before), then the four words the
  disc's `MAGIC060.EMI` draws, found in its routine by the places it draws
  them: `WEAK` at (`0x5A`, `0x44`) and `POINT` at (`0x80`, `0x44`) in the
  8 px font where the port draws its weakness heading, the item word at
  (`0x5A`, `0x8A`) - the French disc's `OBJET`, the German `OBJEKT`, the
  others `ITEM` - and the EXP label drawn whole (the German `EP`). Both
  panels draw `WEAK POINT`. A word the builder does not find, and every word
  of an overlay built before this entry (its chunk has the two headings
  alone; the DLL takes it and logs it), is the PC's own copy of the
  Japanese disc's English, which `BOF3.exe` carries beside EXP unread
  (`WEAK` `0x65AAA8`, `POINT` `0x65AABC`). With no overlay, nothing changes.
- **Rationale:** the owner's call on [`exe-chinese-remaining.md`](exe-chinese-remaining.md)
  4.1 (2026-10-13): restore the heading the PlayStation's way in every overlay
  language, and fix the French and German `ITEM` (a defect, described in
  [`labels-1013.md`](labels-1013.md) 1.3).
- **Also in the PSX version?** The words and places are each disc's own
  (US `0x801EF1E0..0x801EF2A0`, the small draw `0x801EF76C`). Two differences
  stay: **the Western discs draw `WEAK POINT` on the party member's panel
  only** - their enemy panel has no weakness row at all, neither heading nor
  the element glyphs (US `0x801EF2DC`; only the Japanese disc draws it on
  both, with the elements at (`0x66`, `0x50`), `0x801EF36C`) - and the PC's
  enemy panel keeps the row and its six Chinese element glyphs (`0x66A380`,
  drawn as quads by glyph code), which no layer repaints. Whether a Western
  layer should drop the enemy's weakness row as the Western discs do is the
  owner's to decide ([`labels-1013.md`](labels-1013.md) 1.2).
- **Verification:** `loc_build.py all` on the US, European, French, German
  and Japanese discs reports `identify words 6` (the readers' words per disc
  in [`labels-1013.md`](labels-1013.md) 1.1); `loc_build.py selftest`; the
  log `DIV-0064: 6 of 6 identify words` under all five tags; the owner's
  `identify.txt` with a shot every 30 frames from 600 (`--save identify`),
  frame 900, before and after under en-US, and after under fr-FR, de-DE,
  ja-JP and `original` (`labels-1013.md` 1.4): `WEAK POINT` at rows
  `0x44..0x4A` from x `0x5A`, the item word from row `0x8A` (`ITEM`,
  `OBJET`, `OBJEKT`), `EP` under de-DE, the Chinese panel unchanged under
  `original`; on a US-disc-only and a French-disc-only install the same
  words and log (`labels-1013.md` 6.2); an overlay built before this entry
  taken and logged (6.1); the self-tests in `labels-1013.md` 6.3.
- **Reversible?** play without `BOF3X_LANG`; the chunk is the overlay's.

### Beyd's name from the disc when the battle extra opens its equipment screen

- **ID:** DIV-0118
- **Date:** 2026-10-13
- **Subsystem:** battle extra (`BattleExtra_EquipOpen` `0x42DC20`, ours in
  `src/game/battle_e1.cpp`; DIV-0064's group 17 in `src/game/labels.cpp`,
  chunk kind 15 tag 17 of `tools/loc_build.py`)
- **Tier:** Sensible - as DIV-0018: the exe's own strings are the Chinese an
  overlay leaves.
- **Original behaviour:** as the equipment screen of fight 26's battle extra
  opens, `BattleExtra_EquipOpen` copies the five bytes at `0x669CE8` - a
  Chinese name - over character record 7's (the whelp's slot);
  `BattleExtra_EquipLeave` copies the whelp's name back from `0x669CE0`
  (DIV-0020 writes that one). So under a Latin overlay the guest showed the
  port's Chinese name.
- **New behaviour:** under a PlayStation overlay the five bytes come from the
  overlay's string - `Beyd` on the US, European, French and German discs -
  kept in a buffer of ours (`Labels_Slot(17, 0)`); `0x669CE8` stays as
  shipped. The builder finds the string by what the disc's twin does: in
  `BATE.EMI` the copy of 5 bytes (`addiu $a2, $zero, 5`) two calls after
  the sound `0x102` (US `0x801D0F18..0x801D0F58`, the string `0x801D86F8`
  right after `Whelp`), the shape of the PC's own function.
- **Rationale:** the owner's call on [`exe-chinese-remaining.md`](exe-chinese-remaining.md)
  section 7 (2026-10-13: build this group this round).
- **Also in the PSX version?** The same copy of the same five bytes; the
  name is the disc's.
- **Verification:** `loc_build.py all` reports `guest name 1` on the four
  Western discs; `loc_build.py selftest` (a synthetic copy found, one with no
  sound before it refused); the log `DIV-0064: 1 of 1 guest name`; the
  `battle_e1` group in the `'*'` self-test (the fuzz runs before an overlay
  loads). **Not seen in game:** no recipe reaches the duel's equipment
  screen ([`labels-1013.md`](labels-1013.md) 4).
- **Reversible?** play without `BOF3X_LANG`, or
  `BOF3X_ORIGINAL=BattleExtra_EquipOpen` (Capcom's copy reads `0x669CE8`).

### Fight 26's turn counter in the disc's words and layout

- **ID:** DIV-0119
- **Date:** 2026-10-13
- **Subsystem:** battle, fight 26's count task (`Boss26Fx_DrawCount`
  `0x43C780`, ours in `src/game/boss_sf.cpp`; DIV-0064's group 18 in
  `src/game/labels.cpp`, chunk kind 15 tag 18 of `tools/loc_build.py`)
- **Tier:** Sensible - as DIV-0018.
- **Original behaviour:** in its box at (`0x6B`, `0x10`) the PC draws a
  Chinese word through `0x669D20` at (`0x7A`, `0x12`), the count `0x15 - turn`
  (`%2d`) in the 12 px numerals at `0x92`, and a second Chinese word through
  `0x669D24` at `0xAA` - under every overlay.
- **New behaviour:** under a PlayStation overlay, the disc's own: the count
  at the disc's x and one word at its x, drawn whole - `turns left` after the
  count at (`0x70`, `0x80`) on the US, European and German discs, the
  European and German singular `turn  left` beside it, the French
  `Rest   tours` / `Rest   tour` at `0x70` with the count at `0x94` between
  its words. The singular follows the disc's test on the turn `0x904B90`:
  the European disc's when the turn is `0x14` (one left), the French and
  German when it is `0x14` or more, the US never (it has no singular). The
  builder reads all of it off `BOSS025.EMI`'s routine (the call drawing the
  box, the two draws on row `0x12`, the strings loaded between them, the
  `sltiu` or `beq` on the turn); `BOSS027.EMI` holds the same routine and
  words on every disc.
- **Rationale:** the owner's call on [`exe-chinese-remaining.md`](exe-chinese-remaining.md)
  section 7 (2026-10-13).
- **Also in the PSX version?** The words, places and test are each disc's
  (US `0x800C3088..0x800C3100`; the European, French and German routines in
  [`labels-1013.md`](labels-1013.md) 2). The German disc's words are the
  English ones.
- **Verification:** `loc_build.py all` reports `turn counter 3` on the four
  Western discs (the layouts in `labels-1013.md` 2); `loc_build.py selftest`;
  the log `DIV-0064: 3 of 3 turn counter`; the `boss_sf` group in the `'*'`
  self-test. **Not seen in game:** no recipe reaches fight 26
  (`labels-1013.md` 4).
- **Reversible?** play without `BOF3X_LANG`, or
  `BOF3X_ORIGINAL=Boss26Fx_DrawCount`.

### The hi-lo game's three choices from the disc, centred in their boxes

- **ID:** DIV-0120
- **Date:** 2026-10-13
- **Subsystem:** the faerie village's first game (`CommuHiLo_DrawChoice`
  `0x45B1E0`, ours in `src/game/rest_4c.cpp`; DIV-0064's group 19 in
  `src/game/labels.cpp`, chunk kind 15 tag 19 of `tools/loc_build.py`)
- **Tier:** Sensible - as DIV-0018.
- **Original behaviour:** each choice box (`0x2A` x `0x14`) draws its label
  through `0x669F54` - one or two Chinese glyphs - at (x + `0xB`, y + 3) with
  the count 2, under every overlay.
- **New behaviour:** under a PlayStation overlay the label is the disc's word
  - `HIGH` / `LOW` / `SET` (US, European), `Haut` / `Bas` / `Mise`, `Hoch` /
  `Tief` / `Setz` - drawn whole and centred in the box as the discs place it
  (a four-letter word at x + 6, a three-letter one at x + `0xA`: here
  x + (`0x2C` - its pen width) / 2). The builder takes the three 4-byte slots
  right before the 72 bytes of sprite rectangles the PC has at `0x652D04`
  (the anchor group 14 already uses), in `COMMU02.EMI` or PSP-EU's
  `BOOT.BIN`.
- **Rationale:** the owner's call on [`exe-chinese-remaining.md`](exe-chinese-remaining.md)
  section 7 (2026-10-13).
- **Also in the PSX version?** The words and places are each disc's
  (`COMMU02.EMI`, US `0x801D2BD0`). One difference: the US and European
  routines pass the draw the count 3 when the box's mode is 1 or 2, and the
  French when it is 1, which by the PC's reading of the count would cut a
  four-letter word to three letters; ours draws every word whole
  ([`labels-1013.md`](labels-1013.md) 3).
- **Verification:** `loc_build.py all` reports `hi-lo choices 3` on the four
  Western discs; the log `DIV-0064: 3 of 3 hi-lo choices`; the `rest_4c`
  group in the `'*'` self-test. **Not seen in game:** no recipe reaches the
  hi-lo game (`labels-1013.md` 4).
- **Reversible?** play without `BOF3X_LANG`, or
  `BOF3X_ORIGINAL=CommuHiLo_DrawChoice`.

### A map edge that leads to no area holds, instead of crashing

- **ID:** DIV-0121
- **Date:** 2026-10-13
- **Subsystem:** where a step lands (`Field_LeaderStepLands` `0x52E580`, ours
  in `src/game/event_leader.cpp`); the crash it avoids is in
  `Area_ZoneAtIn` `0x595450` (`src/game/window_task.cpp`)
- **Tier:** Sensible - a crash in the PC port's own path.
- **Original behaviour:** in an area whose `Field_InputFlags` has bit 1 (the
  encounter fields, area 29 among them), a step onto the map's edge calls
  `Field_ChangeArea(word 0x802290, 0x7E091C, 0x7E0920, 4)` - the area, and
  the position, the party came from, which `Area_Enter` records on every
  entry. `GameMode_Start` makes `Game_AreaNumber` `0xFFFF` before it enters
  a loaded game's area, so in a game loaded in such an area the word is
  `0xFFFF` and the position 0. `Area_PickMusic` then looks area `0xFFFF` up
  in the 200-entry zone-list table `0x668D80`: entry `0xFFFF` is
  `0x6A8D7C`, zero in `.bss`, and `Area_ZoneAtIn` reads address 0. Capcom's
  six functions (`BOF3X_ORIGINAL=Field_LeaderStepLands,Field_ChangeArea,
  Area_PickMusic,Area_ZoneAtIn,Area_Enter,GameMode_Start`) crash at the same
  recipe frame as ours did, at `BOF3.exe+0x195475` (known-defects D270).
- **New behaviour:** when that word is `0xFFFF` the edge is not an exit: the
  leader's walk ends there (sprite state 1, as after the edge's own area
  change) and the party stays in the area. Every other edge, and every edge
  in a game that entered the area from somewhere, is Capcom's.
- **Rationale:** a crash. A game can only be loaded in an encounter field
  through the PC port's F12 save (`Save_QuickWrite`, anywhere, any time - I18),
  which the input recorder's shots also press; the owner's `combat` save is
  one, in area 29. There is no area to send the party to - the save block
  holds neither the world map nor the place on it - so the edge holds. **The
  party may then have no way out of the field** (the owner's to say whether
  the field has another); the fuller repair is I18's - F12 refused in such an
  area, or saving the world-map place it came from.
- **Also in the PSX version?** The same code: `Field_ChangeArea` `0x801A0A30`,
  `Area_PickMusic` `0x801A1388` and the zone lookup `0x801A1498` (JP
  `GAME.EMI`) have no bound on the area either, and entry `0xFFFF` of the
  PSX table `0x80181ADC` is an instruction word of `GAME.EMI`, `0x90620000`,
  an unmapped address. Whether a PlayStation save can be made in an
  encounter field, so that the PSX could reach it, is not established here
  (no anywhere-save on the disc; the owner's knowledge).
- **Verification:** `stallion.txt` from the `combat` save at `--speed 8`
  (the ultrawide census's case, D270): ours before the change and Capcom's
  six functions both crash at recipe frame 10,726, with `Rand` equal at every
  frame up to it; ours after it plays the recipe to its end (frame 22,647)
  with no area file opened after frame 10,726 - the party held in area 29.
  `stallion.txt` from its own save (the `# save stallion` header it lacked)
  plays to its end through boss encounter 23 in area 67. The `event_leader`
  fuzz runs Capcom's rule (the switch `g_edge_no_area` is armed after it).
  **Not seen by the owner.**
- **Reversible?** `BOF3X_ORIGINAL=EdgeNoArea`.

### The wide modes re-stage the scenes' edge actors

- **ID:** DIV-0122
- **Date:** 2026-10-13
- **Subsystem:** field scenes (`src/game/wide_staging.cpp` `kSites`; hooks in
  `MoveScript_Step`, the flow pass's `0D`, `EventScript_Op`;
  [`wide-staging.md`](wide-staging.md))
- **Tier:** Sensible - the wider picture's own consequence, fixed the way
  Capcom fixed it on the PSP.
- **Original behaviour:** in the 320-column view, actors a scene starts,
  parks or ends just past the view's edge are not seen. Under DIV-0041's
  wider picture, 36 such rows show in the 16:9 band by the static scan
  ([`ultrawide-staging.md`](ultrawide-staging.md)), among them actors
  standing at the edge through a conversation and actors popping in or out
  there.
- **New behaviour:** in the wide modes only (`Widescreen_Live()` not 0), 18
  site rows (22 of the scan's rows and one of the PSP diff's,
  [`ultrawide-staging-psp.md`](ultrawide-staging-psp.md)) move the actor's
  start out past the live picture's edge, or extend its last walk before a
  hide or an end past it, by `ceil((columns + 12 - beyond) * 10 / pps10)`
  steps (capped by the steps to the picture's top), and lengthen the walk by
  the same steps, so it ends where the original's did. Three rows walk the
  added steps at the fastest speed, because the actor's next wait is for a
  clock value close to the walk's. Narrow is the original's. A row whose
  script bytes differ (a disc-built or PSP-data install) is left original and
  logged. 14 of the scan's 16:9 rows cannot take this shape (a point the
  scene ends on, a stationary actor, a walk whose end is itself in the band)
  and are left for the owner ([`wide-staging.md`](wide-staging.md) 3).
- **Rationale:** the owner's call of 2026-10-13
  ([`ultrawide-round.md`](ultrawide-round.md) 4): fix properly at 16:9, park
  farther. It is the shape Capcom used on the PSP for its 32 columns (9 of the
  scan's 56 band rows in 7 scenes, starts and exits moved out with the walk
  lengthened, no hide added), written here as a rule in the columns, never as
  the PSP's script bytes.
- **Also in the PSX version?** The PSX has the 320 view, where the original
  staging is never seen; the PSP has its own re-staging of some of these
  sites (its data, not ours).
- **Verification:** `tools/wide_staging_check.py` (each row's address, hash,
  ops, and the rule's point outside the picture at 53 and 120 by the scan's
  camera; 0 problems); `BOF3X_SHADOW=wide_staging` (the rule, the table,
  narrow applying nothing, the engine on six real rows; 0 mismatches) and ten
  planted controls refused. Live: the census after the fix
  ([`ultrawide-census-3.md`](ultrawide-census-3.md) 4.2) finds no band row
  for the re-staged sites the routes reach (site 3 in `bossAndFlash`, sites
  15..17 in `dolphinBoss`), and `Rand` stays narrow's on both routes
  ([`ultrawide-census-4.md`](ultrawide-census-4.md) 1). 20 of the 24 armed
  rows are not reached by any route. **Not played by the owner.**
- **Reversible?** `BOF3X_WIDE_STAGING=0`, or narrow.

### Case zero takes the PSP's staging in the wide modes

- **ID:** DIV-0123
- **Date:** 2026-10-13
- **Subsystem:** area 175, chapter 7 (`Scena07_Scene2`), the party's scene
  scripts `+0x18[4]` and `[5]` (`wide_staging.cpp` `kCaseZero`;
  [`wide-staging.md`](wide-staging.md) 5, 5a)
- **Tier:** Sensible - Capcom's own later staging.
- **Original behaviour:** the pair walk 24 steps west at clock `0x1B` and
  wait at screen x -40, in the 16:9 band (the owner's capture, 2026-10-13:
  two party members standing at the top left of a forest path while the
  hero is spoken to); later they walk the same way again and 18 / 20 steps
  east.
- **New behaviour:** in the wide modes, as the PSP stages it: each west walk
  is 3 steps shorter and is followed by a walk north; the east walk is
  preceded by the same walk south, and is 3 shorter. The pair leave the band
  for the 4:3 view's top, and the route ends where it did. Two departures
  from the PSP's 6 north / 6 south (revised 2026-10-14,
  [`wide-staging.md`](wide-staging.md) 5a): each rewritten walk takes exactly
  the original's frames - the 3 lost steps' frames are spent on the added
  walk, partly at the actor's speed and the rest at the fastest whole speed,
  out of view; and the north walk goes far enough to take the drawn
  rectangle's bottom above the top row (read at run time; 12 steps by the
  dumps). The PSP crops 12 rows, which hid the pair; we keep 240. Narrow is
  the original's.
- **Rationale:** the owner's call ([`ultrawide-round.md`](ultrawide-round.md)
  4a): copy the PSP's staging, written as a change of the original's steps in
  our code, cited to the PSP diff
  ([`ultrawide-staging-psp.md`](ultrawide-staging-psp.md) section 2, rows 53
  and 54). Game timing stays Capcom's: the PSP's own walk, 3 steps' frames
  longer, moved `dolphinBoss`'s recorded route
  ([`ultrawide-census-3.md`](ultrawide-census-3.md) 3.2), which is why the
  revision keeps the original's frame count.
- **Also in the PSX version?** The PSX is the original's; the PSP (JP and EU
  alike) is the model.
- **Verification:** the scripts' address and hash at the arm; the engine's
  self-test (each walk's frames equal the original's at speed indices 1..5
  and feet rows -10..120; north clears row 0 where the frames allow; south
  equals north; the speed is put back; only in area 175); two controls
  refused. Live ([`ultrawide-census-4.md`](ultrawide-census-4.md) 1):
  `dolphinBoss` at 16:9 and 21:9 keeps narrow's `Rand` on all 14,923 frames
  through the fight; the walks start on narrow's frames and the dialogue
  matches frame for frame; through the waits the 4:3 middle is
  pixel-identical to narrow's (no sliver on the top row). **The owner has
  seen the walk and approved it (2026-10-14).**
- **Reversible?** `BOF3X_WIDE_STAGING=0`, or narrow.

### At 21:9, a field object past the 16:9 view is not drawn in a scene

- **ID:** DIV-0124
- **Date:** 2026-10-13
- **Subsystem:** `Sprite_Draw` `0x5935B0` (ours), one call after its cull and
  bit 7 (`wide_staging.cpp` `HideSite`; [`wide-staging.md`](wide-staging.md) 6)
- **Tier:** Sensible - draw-only.
- **Original behaviour:** no picture wider than 320 columns; under DIV-0041
  at 21:9, actors parked or passing in the 120 columns past the 16:9 view
  are drawn during scenes.
- **New behaviour:** at `BOF3X_WIDE=2`, a field object, extra object or
  non-leader party member whose drawn rectangle lies wholly past -53 or 373,
  while a scene holds the leader (the census's scene mask, bits 1..8) in a
  field scene (`Game_Mode` 2, or 4 - the field with a message open,
  `GameMode4_Run` `0x495E60`, added 2026-10-14 - no world-map record for the
  area by `WorldMap_RecordIndex`, not the attract demo's chapter 16), and that
  no exception names, is not drawn. Bit 7 and every field the original writes
  are unchanged; only the primitive is skipped. 16:9 and narrow are
  unchanged.
- **Rationale:** the owner's call ([`ultrawide-round.md`](ultrawide-round.md)
  4, layer 2): a simple hide for what is still visible in the 21:9-only part
  of the bands.
- **Also in the PSX version?** No wider picture there.
- **Verification:** the self-test's fuzz of the decision at the band edges,
  the mask on and off, the leader and an exception excluded (and a sweep of x
  -200..520); controls refused; the census's `hidden` column records the same
  decision. Live: 215 rows hidden in field scenes in 13 areas and none on the
  world map or in the attract demo ([`ultrawide-census-3.md`](ultrawide-census-3.md)
  4.3); after the mode-4 keying, every field-object row in a scene in
  `balioAndSunder_2`'s areas 39 and 41 hidden (283 rows) and the leader never
  ([`ultrawide-census-4.md`](ultrawide-census-4.md) 3). The hash differs from
  the run without it in draw pages only. **Not played by the owner.**
- **Reversible?** `BOF3X_WIDE_HIDE=0`, or 16:9.

### A `DR_MOVE` out of the display buffer textures what follows: the frame-buffer grab

- **ID:** DIV-0125
- **Date:** 2026-10-13
- **Subsystem:** render (`src/render/fb_grab.{h,cpp}`,
  `render_shim.{h,cpp}`, `render_d3d11.cpp`; `src/game/d3d_list.cpp`
  `MoveImage`, `d3d_draw.cpp` `D3d_BindTexture`, `gfx_texcache.cpp`
  `Gfx_InvalidateTextures`; [`depress-wobble.md`](depress-wobble.md) 6)
- **Tier:** Intent - the port could not do what the code plainly asks for,
  and the PlayStation shows what was meant.
- **Original behaviour:** Capcom's port carries out a `DR_MOVE` as a copy
  inside its VRAM shadow (`Gfx_MoveImage` `0x59E9A0`), which never holds the
  rendered frame. Primitives that texture from a move out of the display
  buffer therefore sample nothing: Depress (`MAGIC109`) draws a solid black
  sixteen-sided disc over the party's corner for about 144 frames; the
  fishing panel's pedestal is blank (light grey-white); kind 0x51's column
  and the ending's frame copy lose their picture.
- **New behaviour:** the shadow move is unchanged, and the backend also
  copies that part of the picture drawn so far, at the window's resolution
  (the owner's call), into a texture. Direct-colour page textures over the
  destination sample it until the shadow there is written again. Depress
  shows the PlayStation's whirlpool; the fishing pedestal shows the field's
  ground. Not reached: sprites of a grabbed page (they take the cell cache),
  4- and 8-bit pages over a grab, and the four effect states that read the
  grabbed rectangle back on the CPU (`Gfx_StoreImage`; known-defects D297).
- **Rationale:** the PlayStation's GPU moves from the frame buffer. The PC's
  code issues the same moves call for call (the twin of `MAGIC109`,
  [`depress-wobble.md`](depress-wobble.md) 2), and the owner's PlayStation
  frame shows the swirl (2a there). A port defect, restored.
- **Also in the PSX version?** Yes: this is what the PlayStation draws.
- **Verification:** `BOF3X_SHADOW=fb_grab`: property checks, 0 failures,
  three controls refused; `'*'` at `BOF3X_WIDE` unset, 1 and 2: exit 0;
  `depress.txt` narrow and 16:9 and `caughFish.txt` with the grab on and off
  (6.3, 6.4 there); off is pixel-identical to the black-disc run (20 of 20
  shots). **The owner, 2026-10-14, on the captures:** "That restoration looks
  perfect", the fishing pedestal included. Kind 0x51's column and the
  ending's frame copy are unseen (no recipe reaches them).
- **Reversible?** `BOF3X_FBGRAB=0`. Also `BOF3X_ORIGINAL=Gfx_DrawOTag` (no grab
  recorded) or `D3d_BindTexture` (no draw tagged).

### The debris matrix is the GTE's identity: the lighthouse's rays come back

- **ID:** DIV-0126
- **Date:** 2026-10-14
- **Subsystem:** the effect engine's GTE helper `EffectGte_SetDiagonalOne`
  `0x494180` (ours, `src/game/effect_gte.cpp`; [`lighthouse-rays.md`](lighthouse-rays.md))
- **Tier:** Intent - a restoration: the PlayStation's behaviour, lost in the
  port.
- **Original behaviour:** Capcom's PC writes 1 on the matrix's diagonal
  (`mov edx, 1`). Its seven callers premultiply it by `Gte_RotMatrixX / Y / Z`
  (each product `>> 12`), so the rotation they build collapses to 0, 1 and -1
  and every edge it turns becomes 0 or -1 (known-defects D215). Seen in play
  as D243: kind 0x4B's 32 additive `POLY_G3` rays in the lighthouse (area 77,
  `eldritchGene.txt`, `Frame_Counter` 872..1057) are drawn as points on the
  globe; `BOF3X_ORIGINAL` of the six functions involved draws none, its
  records byte-identical to ours before the change.
- **New behaviour:** the diagonal is 0x1000, the identity in 4.12; the rays
  fan out of the globe over the scene, and the six other callers' shards,
  arms and debris (kinds 0x1E, 0x1F, 0x21, 0x64, 0x72, 0x97, 0xA0) turn by a
  true rotation. The switch is a word armed through
  `bof3::PatchBytes("DiagonalIdentity", ...)` after the `effect_gte` fuzz, so
  the fuzz keeps comparing ours against Capcom's 1s.
- **Rationale:** the PlayStation twin `0x801B0F5C` (JP `GAME.EMI`) stores
  0x1000; kind 0x4B's twin (`AREA077.EMI` `0x801F3E04..0x801F4B7C`) is the
  PC's call for call otherwise. The owner's PlayStation picture shows the
  rays.
- **Also in the PSX version?** No - the PSX is what this restores.
- **Verification:** live ([`lighthouse-rays.md`](lighthouse-rays.md) 4):
  before, the 32 rays' boxes a tenth of a pixel (`BOF3X_DRAWORDER`) and their
  edges 0 or -1; after, every edge 4,000+ long and the rays in every capture
  from recipe frame 882 to 1014. `effect_gte` fuzz 80,000 rounds on Capcom's
  rule, the armed identity check 20,000 rounds, 5 of 5 controls refused. The
  21:9 hash of `eldritchGene` keeps narrow's `Rand` (13 pages differ, all
  picture; [`ultrawide-census-4.md`](ultrawide-census-4.md) 0). **The owner
  approved the rays (2026-10-14).** The other six callers are not seen in
  play (no recorded route reaches them).
- **Reversible?** `BOF3X_ORIGINAL=DiagonalIdentity` (ours writes Capcom's 1s);
  `BOF3X_ORIGINAL=EffectGte_SetDiagonalOne` (Capcom's function).

### The ending's backdrop: the seam gone, the pictures and the letterbox into the wide bands

- **ID:** DIV-0127
- **Date:** 2026-10-14
- **Subsystem:** the ending's backdrop, `EffectKind18Sub61_DrawOverlay`
  `0x5151A0` (ours, `src/game/effect_6d.cpp`), and its letterbox,
  `Scena17_DrawLetterbox` `0x56CFF0` (ours, `src/game/scena_sc15.cpp`);
  [`ending-backdrop.md`](ending-backdrop.md)
- **Tier:** Intent - the seam is a port artefact (known-defects D302) and the
  bands are DIV-0041's class, the wide picture the owner chose
- **Original behaviour:** the ending's desert scene (chapter 17, area 0xC7)
  is the area's terrain and the party under two paletted pictures drawn in
  screen space by effect kind 0x18's sub-kind 0x61: a tile over 0..320 x
  0..256, then eight pieces of the table `0x65F470` - pieces 0 and 1 the
  cloud sky (256 + 64 columns, 48 rows, page 0x99), pieces 2..7 the far dunes
  (page 0x97, a 256-wide picture whose first 64 columns are repeated down the
  last 64 of the screen). The chapter's letterbox is two black tiles 320 x
  30. Two things follow from the table under the PC's draw: (1) pieces 0 and
  1 are 48 rows and 64 columns with a `dv` of 48 and (piece 1) a `du` of 64,
  so the port's texel-centre table (`D3d_TexCoords`, `(i + 0.512) / 256`)
  samples the row under the strip and the column past it on their last
  pixel row and column - index 0 of the page, a different blue: a one-pixel
  line across the picture where the sky meets the dunes and down the sky's
  last column (D302). (2) Under DIV-0041's wide picture everything stops at
  the 320 columns: black above the terrain in the band corners, the dunes'
  picture cut at the old edges, and terrain showing above and below the
  letterbox at the sides.
- **New behaviour:** once the fills are armed (`Widescreen_FillsArmed`, after
  the self-tests, wide or narrow) pieces 0 and 1 are drawn with `du` = w - 1
  and `dv` = h - 1, the rule piece 0's `du`, pieces 4 and 5 and the port's own
  fills already follow: the sky's last row and column are its own, the lines
  gone. Under the wide picture (`Widescreen_Fill()` columns a side) the
  letterbox tiles are full-frame fills at `(Widescreen_FillX(), y)`
  `Widescreen_FillWidth()` x 30; the tile runs to the bands; and every piece
  on the picture's left edge (x 0) or right edge (x + w = 320) gets band
  pieces beside it: its own rows, mirrored about the edge column, and
  mirrored again past the piece's width (the 64-wide right pieces under the
  21:9 preset's 120 columns) - the texel at the edge continues outward, so
  the sky and the dunes meet the band without a seam. Narrow play draws the
  original's packets apart from the two pieces' `du` / `dv`; every fuzz and
  `BOF3X_ORIGINAL` see Capcom's packets bit for bit.
- **Rationale:** the owner's launcher background is this scene, in the wide
  picture (2026-10-14: "extend the black bars, extend the background draw to
  426 instead of 320, and fix the seam line"). Mirroring is the choice that
  needs no new art and shows no seam; the pictures are 256 wide and Capcom
  already repeated them for the last 64 columns, so a wider painting through
  the substituted-art path is the art route, not a code one.
- **Also in the PSX version?** The seam, no, by the reading: the PlayStation
  GPU steps its texel coordinate in whole texels from `v0`, so 48 rows from
  16 show rows 16..63 whatever `v1` says; it is the port's own texel-centre
  table (DIV-0025's inset, one level up) that makes the overshoot visible.
  Not measured on the disc. The bands: the PSX picture is 320 wide.
- **Verification:** captures of the scene from the crafted chapter-17 save
  (`tools/recipes/ending17.txt`; `ending-backdrop.md` 1), point filter,
  before and after, at recipe frames 2,469..2,729 (14 shots each): narrow,
  the only rows that differ are the sky's (game rows 30..47) and the line at
  row 48 and column 319 is gone; every row outside the sky identical. Wide
  (`BOF3X_WIDE=1`) and 21:9 (`BOF3X_WIDE=2`): `tools/wide_seams.py` 0 frames
  at or above 0.2 on either side (14 each; 10 to 13 of 14 before). Headless
  self-tests `'*'` and `en-US`: [`ending-backdrop.md`](ending-backdrop.md) 5.
- **Reversible?** `BOF3X_ORIGINAL=EffectKind18Sub61_DrawOverlay` (Capcom's
  draw: the seam and the 320-wide pictures), `BOF3X_ORIGINAL=Scena17_DrawLetterbox`
  (the 320-wide bands). Narrow play without the wide picture loses only the
  seam.

### The Identify panel's element row as the game's own element icons

- **ID:** DIV-0128
- **Date:** 2026-10-15
- **Subsystem:** the Identify panel's element row, `Identify_DrawElements`
  `0x4B1520` (ours, `src/game/magic_s12.cpp`); `Lang_Japanese`
  (`src/game/lang_layout.cpp`)
- **Tier:** Intent - the owner's choice (2026-10-15)
- **Original behaviour:** for an identified enemy the panel draws six 12 x 12
  glyph quads (code `0x6C`) 0x14 apart, the glyphs the pointers at `0x66A398`
  name - the port's Chinese characters 炎 冰 雷 风 地 圣 (`0x66A380..`), in
  every language: no label layer repaints them (`exe-chinese-remaining.md` 3).
  An element the enemy is weak to (its resistance byte 1 or less; holy 3 or
  less) is drawn at shade 0x80, the rest at 0x30.
- **New behaviour:** under every language overlay but the Chinese original
  (`Lang_Latin()` or `ja-JP`), once the self-tests are over
  (`Widescreen_FillsArmed`), each glyph is the game's own 8 x 8 element icon
  instead - texture page 0xF, row 248, icon i at u `(i + 7) * 8`, the sheet
  `Menu_DrawIcon8` draws item kinds from (icons 0..5: fire, ice, thunder,
  wind, earth, holy, the glyphs' order; CLUT `0x780E` for 1 and 3, `0x780C`
  for the rest, as `Menu_DrawIcon8` picks) - a POLY_FT4 stretched over the
  same 12 x 12 cell, the same shade (bright a weakness, dim the rest), slot
  2. Positions, spacing and the weakness test are unchanged.
- **Rationale:** the owner, 2026-10-15: the Chinese characters on an English
  panel are unreadable to its players; the icons are the game's own art and
  already mean these elements in the item and skill lists. Kept under the
  Chinese original, whose text the characters were made for. The weakness
  row itself stays on every language (the owner's earlier call; the Western
  discs drop it, `labels-1013.md` 1.2).
- **Also in the PSX version?** No: the Japanese disc draws its own element
  glyphs there (`0x801EFC84`); the Western discs draw no weakness row.
- **Verification:** the owner's `radiationRoom.txt` (an Identify on a
  GntRoach), en-US, 16:9, frame 1,920: the six icons in the row, fire bright
  (the weakness), the rest dim. `magic_s12`'s self-test 88,000 rounds, 0
  mismatches under `en-US` and `original`.
- **Reversible?** `BOF3X_LANG=original`, or `BOF3X_ORIGINAL=Identify_DrawElements`.

### A missing sound stream ends its wait; a PSX-only install has the 11 jingles

- **ID:** DIV-0129
- **Date:** 2026-10-16
- **Subsystem:** audio - `Sound_LoadStream` `0x587910` and `Sound_StreamDone`
  `0x587A00` (ours, `src/game/save_menu.cpp`); `tools/xa.py` `jingles` / `mp3`,
  `importer.py build` (`snd_from_psx`, the manifest's `[snd_jingles]`)
- **Tier:** Intent - the original hangs (the engine half); the jingles are the
  release the player owns, by the player's own ffmpeg (the data half, as
  DIV-0106).
- **Original behaviour:** `Sound_LoadStream` stores the stream kind, then
  returns at a failed `SND\NAME.DAT` open without stopping anything; a wait
  that follows (the inn's and the camp's rest; twenty more source files call
  `Sound_StreamDone`) asks `Music_IsPlaying() ^ 1` for kind 0, which is the
  area's song, still playing - so it never ends. The PC install always has
  the file; a PSX disc-only install had none of the 11 kind-0 jingles (the
  PC's are MP3s of the disc's `S_XA00.STR` clips, `sound-import.md` 4), and
  the inn and the camp hung on a black screen for good (D255: 106 of 106
  frames black over 4,050 frames on PSX-EU, `eye-batch-1013.md` Q7; PSX-US
  the same, `abc-sweep-1013-us.md` 6.2).
- **New behaviour:** (1) a failed open is remembered (our flag, no game
  state; logged once) until the next open succeeds, and `Sound_StreamDone`
  answers 1 meanwhile, so a wait on a stream that is not there ends at once
  on any install, for any of the three kinds. (2) `importer.py build` with a
  PSX disc and no PC install among its sources writes the 11 jingles to
  `base/snd/`: each recipe row's `S_XA00.STR` clip (`recipes/pc-zh.snd.toml`,
  kind 0) decoded from a fresh history by `xa.decode`, then resampled to
  44.1 kHz and encoded by the player's ffmpeg in the PC's form (128 kbit/s,
  plain stereo, no ID3v2 tag or Xing frame - the encoder call now shared with
  `at3.py`, whose output is unchanged). `install --base` lays them in `SND\`.
  Without ffmpeg they are not written (the build says why) and (1) keeps the
  inn and the camp leavable without the jingle. With a PC install among the
  sources nothing changes: its own MP3s play.
- **Rationale:** the owner, 2026-10-15 (`owner-review.md`): "both" - the
  disc's XA stream as the jingle on a PSX-only install, and a failed jingle
  open ending the wait at once, so no missing file can hang it again. A wait
  for a sound that never started is plainly not what the code meant.
- **Also in the PSX version?** No: the PlayStation streams its XA from the
  disc (op 89, `0x80164090`), which always has the clip; the hang is the
  port's file-based stream meeting an install it never shipped for.
- **Verification:** `shop` on PSX-US (an install rebuilt from the US disc by
  this branch, root `C:\Users\kerok\abd255`): `OYASUMI` from the cache, the
  inn black 2670..2850 and back at 2880 (the PC's frame); the same install
  with the 11 removed: the open fails, logged once, back at 2820 (before: black
  to the route's end). `campingFishing` on both reaches the fishing spot (it
  hung at the camp before; D303..D305 are what that showed). The jingles
  against the PC's MP3s: `sound-import.md` 6 item 3. The headless self-test
  `BOF3X_SHADOW='*'` hosted on the PC install: exit 0, every self-test 0
  MISMATCHES, `save_menu`'s 66,000 rounds among them (the fuzz clears the flag
  before each `Sound_StreamDone` round - the original has none - and the
  engine clears it once the self-tests are done), and DIV-0129's own: a
  missing kind-0 stream answers 1 where the original answers 0, a present
  one 0.
- **Reversible?** (1) `BOF3X_ORIGINAL=Sound_StreamDone` (the original's
  answer; the flag is then read by nothing). (2) a build with a PC install
  among the sources, or the 11 deleted from `base/snd/` and `SND\`.
- **Amended 2026-10-21 (DIV-0150):** the 11 are made whenever a PSX disc and
  ffmpeg are both there. With a PC install among the sources they go in
  `base/psx-jingles/` (the manifest's `[snd_jingles] dir`), not `base/snd/`, so
  nothing that chooses no music source plays them over the PC's; a PSX-only
  build keeps them in `base/snd/` as above.

### A disc's world-map legend shows only the live button icon

- **ID:** DIV-0130
- **Date:** 2026-10-17
- **Subsystem:** the importer's dial-page stand-in - `importer.py`
  `dial_icons_blank` / `dial_icons_of`, applied in `get_chunk` to a disc's own
  section standing in for the port's dial page (kind 1, tag `0x0A081000`,
  class `art`, the 16 world-map areas; PSX `port-art` stand-ins and PSP
  ones alike). The draw is unchanged: `WorldMap_DrawFrame` `0x404390` and its
  area copies.
- **Tier:** Intent - the owner's decision, 2026-10-17
  ([`button-roles-by-release.md`](button-roles-by-release.md) 8: every button
  icon follows the Config).
- **Original behaviour:** each disc's dial page bakes its release's default
  buttons into the legend twice - dim in the frame (sprite 0) and lit in
  labels 1 and 2 - and the code draws the glyph of the save's word over the
  same place (rows 1 and 2: words 0 and 6). On the PlayStation the two agree
  until a Config swap. On the PC the code's words are Japan's, so a Western
  disc-only install showed two icons at once: a circle over the baked cross
  (Enter), a cross over the baked triangle (Guide) (GitHub issue #78, a
  player; [`key-icons-by-mapping.md`](key-icons-by-mapping.md) 9;
  known-defects D307).
- **New behaviour:** the importer repaints the baked icons of a disc's dial
  page with the panel's background (texture u 56..63, v 8..24 in the frame;
  u 16..23, v 88..104 in the labels; each from the plain column beside it, u
  64 and u 24), so the live glyph - the button the save's Config gives Speak
  and Action - is the one icon drawn. The PC install's own page (the port's
  keyboard legend) is not touched. On a JP disc nothing visible changes until
  a Config swap, after which the legend follows it, as on the PlayStation.
- **Rationale:** the owner's answers on the key icons: unify every button
  flow; an icon shows what the Config holds now; PlayStation icons only. The
  baked icon is the release default, the live one is the truth.
- **Also in the PSX version?** Partly: the same code over the same page, so a
  Config swap on the PlayStation showed the baked default under the live
  glyph too. The Western overlap at defaults is the PC's (Japan's words over a
  Western page).
- **Verification:** `worldMapAndAreaTransition` on a PSX-US disc-only install
  built with and without the change (root `C:\Users\kerok\ab79`, frames every
  30): 17 frames differ, each only in the legend's icon column (`143,47` to
  `158,80` of 640 x 480), showing a lone circle (Enter) and cross (Guide)
  where the old install drew the circle over a cross and the cross over a
  triangle. The cells were read on the US, EU, FR, DE, JP, PSP-JP and PSP-EU
  pages (the same places, the background column plain on each).
  `importer.py check`, `psp_standins.py check`: 0 errors.
- **Amended 2026-10-17 (the button unification round, DIV-0135):** the
  PC install's own page is no longer left alone: its key names are painted
  over and the live buttons drawn as glyphs. The disc repaint here is
  unchanged.
- **Reversible?** Not a runtime switch: only by building the cache with an
  importer from before this entry (the stand-in's recipe hash is the disc's,
  checked before the repaint; the manifest records the repainted body's).

### A PSP-only install's title logo is cut from the PSP's pages where the PSP's art lies

- **ID:** DIV-0131
- **Date:** 2026-10-17
- **Subsystem:** the title - `Title_DrawSetA` `0x462820`, `Title_DrawSetC`
  `0x462930`, `Title_DrawSetB` `0x462A00` (ours, `src/game/title_states.cpp`);
  `psp_code::PspBase` (`src/game/psp_code.cpp`)
- **Tier:** Sensible - a repair of how our engine draws a PSP base's own art,
  which no original ever drew.
- **Original behaviour:** the three sets cut the logo from the title pages
  through `Title_Sprites`' rectangles and place it at Capcom's PlayStation
  offsets. On a PSP base (DIV-0103) the pages are the PSP's art, laid out for
  the PSP's own code: the text's lower piece starts 11 rows earlier and is 10
  wider, the emblem's page carries a "FISHING ver." caption under it, and the
  art holds its own TM. Drawn through the PlayStation rectangles the logo tore
  at the "F", its right wing was cut flat, a caption showed and a second, cut
  TM (the mark sprite 1) sat beside the art's (D256; captured 2026-10-17 at
  4:3, 16:9 and 21:9, the same at every width).
- **New behaviour:** on a PSP base (`base_policy = "psp"`, read whatever
  `BOF3X_PSP` says) the sets draw the same sprites and pages with three
  rectangles measured from the PSP's pages - the upper text (0, 0, 224 x 116),
  the lower text (0, 117, 106 x 102), the emblem (0, 0, 202 x 122) - the upper
  piece 3 left of set C's origin, the lower at its x + 224 and y + 5 (where the
  F's strokes join: the best of a column match, `docs/psp-title-framing.md`),
  the emblem 30 right of set B's; sprite 3 (empty on the PSP's page) and the
  mark sprite 1 are not drawn. The logo is centred on the 4:3 frame and drawn
  the same in the wide modes, so 4:3 cuts its wingtips by a pixel or two (the
  owner's call: one layout at every width, the narrow frame cropping it).
  Every other base draws as before.
- **Rationale:** the owner, 2026-10-17, on the mock-up: "much better". The
  emblem's place against the text is the PlayStation's relation (centred over
  the text, the same row as Capcom's); the PSP's own is in its code, not read.
- **Also in the PSX version?** No: the PlayStation and the PC draw their own
  pages through their own rectangles.
- **Verification:** the PSP-EU-only install (`C:\Users\kerok\rvp`), frames
  0..720 every 10 at `BOF3X_WIDE` 0, 1 and 2: the logo whole and joined, one
  TM, no caption, the fade clean; the PSX-US-based install's title at 4:3,
  all 73 frames pixel-identical to the build before. The start-up fuzz of
  `title_states` runs before the switch is read, so it compares Capcom's
  layout.
- **Reversible?** A PSX-based install; the switch is the cache's policy.

### The fishing panel ducks the music as the PlayStation does

- **ID:** DIV-0132
- **Date:** 2026-10-17
- **Subsystem:** the fishing spot's panel - `LeaderPanel_S9Begin` `0x52A440`
  and `LeaderPanel_S9Menu` `0x52A4A0` (ours, `src/game/effect_1e.cpp`); the
  duck itself `sound::DuckMusic` / `UnduckMusic` (`src/game/sound.cpp`), on
  the fade state `Sound_Tick` `0x587C70` steps.
- **Tier:** Intent - a port bug (the owner, 2026-10-17: "I would class this
  as a pc bug"); the PlayStation shows what was meant.
- **Original behaviour:** opening the Gear / Data / Rule panel at the fishing
  spot (area 30, mode 8) calls `Music_FadeOut(0x10)`, a fade to volume 0
  that keeps the track playing, and cancelling it `Music_FadeIn(0x10)`, a
  fade to 127: the fishing song is silent the whole time the panel is open
  (known-defects D308). The PlayStation's twins call
  `SsSepSetDecrescendo(0, 0, 0x14, 0x10)` (`0x8015DEE8`, from `0x801DC908`)
  and `SsSepSetCrescendo(0, 0, 0x14, 0x10)` (`0x8015DE8C`, from
  `0x801DC9BC`), read in the sibling's `generated/overlays_static_0247.c`: a
  duck. Under libsnd's ramp (`MusicSynth::SetVolData` / `SndDecrescendo`: a
  ramp of 20 over 16 VSyncs is one unit a VSync) the song, at sequence
  volume 97 after `Music_Play`'s crescendo
  ([`libsnd-reading.md`](libsnd-reading.md) 5.1), goes to 81 and back:
  amplitude (81 / 97)^2, about -3.1 dB. The port's rewrite of the music
  calls kept the frames and dropped the amount.
- **New behaviour:** the panel's open fades the music over the same 16
  logic frames (DIV-0028's clock) to 313 hundredths of a decibel below where
  it was, in the law the music plays under: under the sequencer (DIV-0087,
  `music_seq::Level`, libsnd's square law) the volume times 81 / 97, 127 to
  106.05; under an MP3 (`Music_SetVolume`, linear in decibels, 10000 / 127
  hundredths a unit) the volume less 3.98, 127 to 123.02. Cancel fades back
  over 16 frames to the volume the duck found (127 when no duck is
  outstanding, as `Music_FadeIn`). No stop at either end; a later fade or
  `Music_Play` takes over as from Capcom's.
- **Rationale:** the owner heard the song stop behind the panel on a
  PSX-only install, while it plays on behind Manillo's screen, and asked for
  the PlayStation's behaviour back.
- **Also in the PSX version?** No - the PlayStation ducks. The silence is the
  PC port's, on every install: measured the same on the PC install with its
  own MP3s and on a PSX-US disc-only install.
- **Verification:** `caughFish.txt` with the WAV dump (`input_run.py --wav`,
  harness root `abfish`, `BOF3X_MUSIC_LOG=1`). Before: frames 866..966 at
  digital silence between the menu's sound effects, on the PC install
  (`BOF3X_MUSIC=mp3`) and on the PSX-US install alike. After:
  `DuckMusic(16)` at frame 856 and `UnduckMusic(16)` at 952 (again at 1838 /
  1957, the second panel), 127 to 123.02 and back on the PC, 127 to 106.05
  and back on PSX-US; no frame of 840..1000 below RMS 400 on either; both
  routes reach `recipe done` at 3,889.
  `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_1e,sound,sound_rest` on the PC
  install: 0 mismatches (effect_1e 144,000 rounds with the two original fades
  among the stand-ins' calls - the flag is 0 until after the self-test;
  sound 46,000; sound_rest 27,000). **Not yet heard by the owner.**
- **Reversible?** `BOF3X_ORIGINAL=FishPanelDuck` (the patch's name): the
  fade to nothing and back.

### The Config's six lines as one permutation; menus confirm on Speak's button and cancel on Action's

- **ID:** DIV-0133
- **Date:** 2026-10-17
- **Subsystem:** input, the Config words - `src/input/sigma.{h,cpp}`; the
  three writers `Boot_Task` `0x496B60`, `LoadMenu_Apply` `0x5885D0` and
  `ConfigScreen_Controller` `0x4613B0` (all ours). **Amends DIV-0051:** the
  controller panel's icon column was the first site keyed on the mapping;
  from here every icon and every hard-coded test asks one table, `σ`, and
  DIV-0051's column is that table's first reader. Words 7 and 8 are named
  here as Speak's and Action's. The button unification round, group SG
  ([`button-sigma.md`](button-sigma.md) 6, ledger draft A).
- **Tier:** Intent - the owner, 2026-10-17: unify every button flow (answer
  1, [`button-roles-by-release.md`](button-roles-by-release.md) 8).
- **Original behaviour:** the Config screen swaps buttons between its six
  lines' words only. `Field_ConfirmButtons` (word 7) and
  `Field_CancelButtons` (word 8), which every menu tests, are written only by
  the boot defaults and the load, so a player who moves Speak off circle
  still confirms every menu with circle. The same holds on the PlayStation.
- **New behaviour:** after each of the three writers, word 7 is Speak's word
  (with its L2 / R2) and word 8 is Action's: menus confirm on whatever Speak
  is on and cancel on whatever Action is on.
  - The six words are read as a permutation `σ` of the six face and shoulder
    bits, in Japan's terms; every other site of the round asks it through
    `Sigma` / `SigmaFrom`. L2 / R2 stay outside it (the rebind masks them
    out; they stay ORed into Speak and confirm, as the original's).
  - Six words that are not one button each (a hand-edited save) are refused
    with a Fatal that names the remedy, not guessed at (rule 4).
  - At every release's defaults nothing changes: words 7 / 8 equal the
    originals and `σ` is the release's own map.
  - A Speak line put on a shoulder makes every menu confirm on that shoulder
    (answer 1 taken literally; the owner judges it on the swapped captures).
- **Rationale:** the owner's answer 1. One flow, so that an icon that
  follows the Config tells the truth in the menus too.
- **Also in the PSX version?** Yes - the PlayStation has the same third flow
  on every release, so this changes Capcom's design, not a port defect.
- **Verification:** `BOF3X_SHADOW=sigma` (2,211 tables, 11 non-permutations
  refused, 13 source lists; 0 mismatches) and the shadows of `mode_flow`,
  `save_menu` and `rest_4f`, which compare Capcom's stores before `Arm`
  ([`button-sigma.md`](button-sigma.md) 5, 8). Live, the round's validation
  ([`button-routes.md`](button-routes.md) 9, 9.1): after `sigma_swap.txt`
  slot 7 holds confirm `0x43` and cancel `0x20` on all four install kinds,
  and the swapped routes stay in step with their identity runs.
- **Reversible?** `BOF3X_ORIGINAL=ButtonSigma` (`σ` the identity; it also
  turns off DIV-0139). A save written under it keeps its words 7 / 8 as saved;
  holding Start while it loads writes the defaults.

### An install starts with the Config of the release it was built from

- **ID:** DIV-0134
- **Date:** 2026-10-17
- **Subsystem:** the boot defaults (`Boot_Task` `0x496B60`, after its copy
  of `0x656AEC`) and `LoadMenu_Apply`'s Start-held reset
  (`0x588629..0x588670`); the release from the cache manifest's sources
  (`src/input/sigma.cpp`). The button unification round, group SG
  ([`button-sigma.md`](button-sigma.md) 6, ledger draft B).
- **Tier:** Intent - the owner's answer 2, 2026-10-17: an install starts
  with the defaults of the release it was built from.
- **Original behaviour:** the PC starts every New Game with Japan's buttons
  (Speak and confirm on circle, Action and cancel on cross, Move square,
  Menu triangle). Since the disc-only round ([`controls.md`](controls.md)
  2.1) a disc-only install starts the same way, so a US disc-only install
  started with buttons its own text and art never name.
- **New behaviour:** the nine words are the release's, sources taken in the
  order JP, the PlayStation West, the PSP.
  - **The PC's own data counts as a JP source** (the owner, 2026-10-17;
    `kPcIsJapanSource`): a PC install with a Western layer or a Western disc
    starts with Japan's buttons, so every PC install is unchanged.
  - A Western disc-only install starts with the West's: Speak and confirm
    on cross, Move circle, Menu square, Action and cancel on triangle.
  - A PSP-EU-only install starts with PSP-EU's: Speak and confirm on cross,
    Move triangle, Menu square, Action and cancel on circle.
  - The Start-held reset writes the same buttons without L2 / R2 in words 0
    and 7, as the PC's reset does for Japan's.
- **Rationale:** the owner's answer 2: a disc-only install's text and art
  are its release's, so its buttons should be too.
- **Also in the PSX version?** No - each PlayStation release starts with its
  own defaults; this brings that back for disc-only installs, which the PC
  never had.
- **Verification:** the self-test's release-by-source cases
  ([`button-sigma.md`](button-sigma.md) 5). Live
  ([`button-routes.md`](button-routes.md) 9.1): on `psx-us` and `psp-eu`
  the state hash's `0x903000` (the nine pad words' page) differs from the
  reference from tick 1 until the save's load and nowhere after; the PC
  installs' equal. A New Game's Config panel on a disc-only install is not
  yet seen.
- **Reversible?** `BOF3X_ORIGINAL=StartConfigBySource`. For the other
  reading of answer 2 (a layer's release counts as a source),
  `kPcIsJapanSource` in `sigma.cpp` is a one-line change.

### The world map's legend shows the buttons the Config holds, as glyphs, no key names

- **ID:** DIV-0135
- **Date:** 2026-10-17
- **Subsystem:** the world map's legend - `WorldMap_DrawFrame` `0x404390`
  and its ten area copies (`world_map_legend`, `src/game/world_map.cpp`),
  and the importer's dial page (`importer.py` `dial_with_disc_shapes`,
  `dial_disc_shapes`). **Amends DIV-0130:** its sentence "The PC install's
  own page (the port's keyboard legend) is not touched" no longer holds;
  its repaint of a disc's baked icons is unchanged. The button unification
  round, group IC-W ([`world-map-hud.md`](world-map-hud.md) 9, ledger draft
  C).
- **Tier:** Intent - the owner, 2026-10-17: every icon follows the Config,
  PlayStation icons only; answer 2, the port's `SPACE` / `X` / `ENTER` art
  replaced by the glyphs alone, no label text
  ([`button-unification-round.md`](button-unification-round.md) 5.1).
- **Original behaviour:** the port's dial page names keyboard keys
  (`SPACE`, `X`, `ENTER`), lit in the labels and dim in the frame, beside
  its pictograms. It blanks the PlayStation's key sheet, so the code's live
  key (word 0, word 6) draws an empty cell. The names are the default key
  table's and lie once a key or a Config line moves (known-defects D275,
  D58's class). The PlayStation draws the glyph of the button the save's
  word holds.
- **New behaviour:**
  - With a PSX disc among the sources, the importer paints the key names
    over with the panel and lays in the disc's key sheet and START: the
    legend shows the PlayStation's circle / cross / START shapes for the
    buttons the Config holds, beside the port's pictograms, lit and dim as
    on the PlayStation.
  - With no PSX disc (a PC install alone, or PC + PSP), the engine paints
    the names over in VRAM by the same rule and draws, for the lit rows, the
    port's own stand-ins (the Config screen's circled numeral or `X`, in the
    button's colour); row 3 gets START in the port's small font. A withheld
    row's stand-in (rows 1 and 2, and START when row 3 is withheld) is
    dimmed by a disc's rule - its colours lighter by 56 a channel, drawn
    through five dim CLUTs the engine keeps in rows v 104..108 of the port's
    blank key sheet ([`world-map-hud.md`](world-map-hud.md) 9.8).
  - The pictograms, the PC-only `START` and the spacing stay (the owner,
    9.7). A disc-only install is unchanged (DIV-0130).
- **Rationale:** the owner's answers. The legend is the one site whose code
  already followed the Config; the port's art was what lied.
- **Also in the PSX version?** No - the PlayStation draws the shapes; this
  undoes a port change (the keyboard legend), with the port's stand-ins
  where the install has no PlayStation shapes.
- **Verification:** [`world-map-hud.md`](world-map-hud.md) 9.4, 9.5 and 9.8
  (the self-tests; `sigma_worldmap` identity and swapped on `pc`, `pc-en150`
  and `psx-eu`; the dim rows +49..+58 a channel on `pc`, the lit rows and
  the disc installs unchanged). The round's validation
  ([`button-routes.md`](button-routes.md) 9, 9.1): on `pc` and `pc-enus`
  every differing identity frame differs only in the legend's key column
  (game pixels `66,24-90,49`), the disc installs 51 / 51 equal; swapped,
  rows 1 and 2 trade on all four.
- **Reversible?** The engine's half: `BOF3X_ORIGINAL=WorldMapLegend`. The
  importer's half: a cache built without a PSX disc, or by an importer from
  before this change (the engine then repaints the page itself, unless
  switched off).

### The fishing banner's labels name the button the line's test is on now

- **ID:** DIV-0136
- **Date:** 2026-10-17
- **Subsystem:** field text, the fishing spot's banner labels -
  `src/game/fishing_text.cpp` `FishingText_LabelOf`; the label draws of
  effect kind 0xF's states 10..12 in `src/game/effect_1a.cpp`. **Amends
  DIV-0069:** its label "for the button the port's numeral names" becomes
  the label for that button through `σ`. The button unification round,
  group IC-F and its two follow-ups ([`fishing-text.md`](fishing-text.md)
  8, ledger draft D).
- **Tier:** Intent - the owner, 2026-10-17: unify every button flow (answer
  1). DIV-0069's owner's word waited on exactly this ("until the banner can
  draw the icons of the bindings in force", IDEAS I29).
- **Original behaviour:** each labelled line of the banner (seven of the
  thirteen records at `0x653B98`) draws a fixed label by its line byte:
  `0x66A2FC[line]`, the port's circled 1, 2 or 3, in pen `0x653C00[line]`
  (2, 1, 6). It names circle, cross or triangle whatever the Config says;
  DIV-0069 draws DIV-0051's icon for the same fixed button under a Latin
  layer.
- **New behaviour:** the label names the button the line's test is on now.
  - The cast's line (text 0) takes word 7's button; every other labelled
    line `Sigma` of the line byte's button.
  - **Read in the terms of the release the running `.data` was built
    from** (D313; `ExeImage_MappedBuild`, `BOF3.exe`'s own and a `pc-zh`
    image being Japan's): the line byte names the line's label entry on
    every release, and the entry is that release's button, `M_R` of Japan's
    (`P` on the PlayStation West: cross, triangle, square; `S` on PSP-EU:
    cross, circle, square). The label is `SigmaFrom(R, that button)`.
  - **A button `σ` leaves where it was draws exactly as the original reads
    it** (the owner's rule, 2026-10-17: recolour only when the button
    moves): the line byte's label in the image's pen, which on a disc image
    is since D313 the disc module's own (West 1, 6, 5; PSP-EU 1, 2, 5). A
    moved button is drawn as its own label in its own pen (circle 2, cross
    1, triangle 6, square 5, L1 / R1 0, as the Config cell `0x461C00`).
  - Under a Latin layer the label is DIV-0051's shape; on the PC's own text
    the port's stand-in, `0x66A2FC[entry]`. At the identity Config nothing
    changes; a Western install at its own defaults draws the US banner's
    labels, and at Japan's words Japan's buttons in their own pens.
- **Rationale:** the round's answer 1. With DIV-0139 moving the tests, an
  icon that follows the Config is true in the fishing too.
- **Also in the PSX version?** Yes - the PlayStation's labels are fixed too
  (the US module's are its own release's buttons); following the Config is
  ours.
- **Verification:** [`fishing-text.md`](fishing-text.md) 8.3, 8.7 and 8.8
  (`BOF3X_SHADOW=fishing_text,sigma,effect_1a`; `sigma_fishing` on
  `psx-us` 49 / 49 equal to RC's pre-round reference at the identity and
  to RC's swapped run under the swap; after D313 the reel label a red
  circle at Japan's words, a blue cross under the swap and under the West's
  own defaults). The round's validation ([`button-routes.md`](button-routes.md)
  9, 9.1): swapped, the reel label trades on all four installs.
- **Reversible?** `BOF3X_ORIGINAL=FishingLabelSigma`.

### The dock's parts-menu legend shows the buttons the Config has its tests on

- **ID:** DIV-0137
- **Date:** 2026-10-17
- **Subsystem:** the dock's parts menu (area 128) -
  `EffectKind5E_DrawPanel` `0x47FBE0` (ours, `src/game/rest_3e.cpp`); its
  tests are `EffectKind5E_Pick` `0x47F610` (DIV-0139). The button
  unification round, group IC-D ([`rest_3e.md`](rest_3e.md) 11, ledger
  draft E).
- **Tier:** Intent - the owner's decision, 2026-10-17
  ([`button-roles-by-release.md`](button-roles-by-release.md) 8: every
  button icon follows the Config; PlayStation icons only).
- **Original behaviour:** the legend is baked art of the area page: each
  release's default buttons for the three tests (Japan's page, the PC's:
  circle place, cross quit, triangle memo; the West's cross / triangle /
  square; PSP-EU's cross / circle / square). It never changes with the
  Config, and on a Western disc-only install it named the US buttons over
  Japan's code (known-defects D274).
- **New behaviour:** each row shows `Sigma` of its test's bit. Where that is
  the page's own icon in that row, the original's one quad is drawn
  unchanged (every row at every release's defaults). Otherwise the row's
  icon cell is left out of the panel and the button drawn there: the page's
  own icon of it from another row; else DIV-0051's icon under a Latin
  overlay; else the port's stand-in glyph (`0x66A2FC`'s, as the Config
  panel's `0x461C00`), in the button's colour. The page's icons are read
  from the VRAM shadow by a shape rule; a page whose cells are not three of
  the four shapes is drawn as it is.
- **Rationale:** the owner's answers on the key icons: an icon shows what
  the Config holds now. Japan's terms (`Sigma`), so the legend and
  DIV-0139's tests agree on every install.
- **Also in the PSX version?** Yes - the PlayStation's legend is the same
  baked art and never follows a Config swap. The Western overlap at
  defaults was the PC's (Japan's words over a Western page), gone with
  DIV-0134's starting Config by source.
- **Verification:** `BOF3X_SHADOW=rest_3e`: the fuzz 0 mismatches before
  the switch; the dock self-test 84 cells, 2,167 plans, 0 mismatches
  (identity on the three page orders, the stand-in swap, 720 assignments
  each); the rule read the seven builds' pages as [`rest_3e.md`](rest_3e.md)
  11's table. **Not seen:** no recorded route reaches the dock yet (RC's
  placed save `dock128` is the owner's to record).
- **Reversible?** `BOF3X_ORIGINAL=DockLegend` (the page as it is), or
  `BOF3X_ORIGINAL=EffectKind5E_DrawPanel`.

### The item lists' L1 / R1 corner shows the buttons the Config pages on

- **ID:** DIV-0138
- **Date:** 2026-10-17
- **Subsystem:** the menu pieces - `Menu_DrawPiece` `0x57D860` (ours,
  `src/game/menu_windows.cpp`); the plan in `src/game/shoulder_glyphs.cpp`.
  The button unification round, group IC-L
  ([`button-shoulder-corner.md`](button-shoulder-corner.md) 6, ledger draft
  F).
- **Tier:** Intent - the owner, 2026-10-17: every icon tied to the mapping
  (answer 1); answer 3 for the PSP page, one code path.
- **Original behaviour:** the ten item and skill lists draw `L1` above their
  scroll bar and `R1` below it, baked into five pieces of the UI page, so
  they name L1 / R1 whatever the Config says. On the PSP, Capcom redrew them
  as `L` / `R`.
- **New behaviour:** each labelled piece is drawn with its label box
  refilled from its own plain background and the glyph of `σ(L1)` /
  `σ(R1)` drawn at the slot.
  - The glyph is the page's own L1 or R1 art where the page has it (the
    PC's and the PSX discs'); otherwise the Config panel's glyph for that
    button: DIV-0051's icon under a Latin layer, else the port's stand-in.
  - On a PSP base, whose UI page and dial sheet carry only `L` / `R`, the
    PSP's tiles are blanked and the Config panel's L1 / R1 drawn, as on
    every other install (the owner's answer 3).
  - With `σ(slot)` the slot's own button on a page with the art, the piece
    is drawn by the original's code: nothing changes on a PC or PSX install
    at the identity.
- **Rationale:** an icon that follows the Config tells the truth once
  DIV-0139 routes the lists' L1 / R1 tests through `σ`.
- **Amended 2026-10-18 (D321): L1 / R1 on a PSP base under `ja-JP` are the
  font's capital `L` and `R`.**
  - *Original:* the PSP's own page draws `L` / `R`; this entry as first
    built blanked those tiles and drew the stand-in strings behind
    `0x66A2FC[4]` / `[5]`. No Japanese layer writes them (D280), so on a
    PSP-JP-only install they were empty and `GlyphCode` stopped the game at
    the first item list (rule 4).
  - *New:* on a base without the L1 / R1 art and under `ja-JP` (no Latin
    layer), L1 and R1 draw the `ja-JP` font's capital `L` (glyph `0x9DF`)
    and `R` (`0x9E5`) in their colour 0, and never read the two stand-ins.
    The codes are a rule, not a table: the `ja-JP` layer puts the Japan
    disc's single-byte sheet whole from `JA_AT` = `0x993`, cell `b` at
    `JA_AT + b` (`loc_build.py` `ja_cell_of`), and the script's capitals are
    ASCII's codes `0x41..0x5A` (`JA_SHARED`; the sibling's
    `docs/TEXT_ENGINE.md`). The draw stops loudly if the font loaded is too
    short to hold them. Circle, cross, triangle and square keep their paths;
    psx-jp (the art) and every Latin install (DIV-0051's icons) are
    unchanged; any other empty stand-in still stops the game.
  - *Why:* the owner, 2026-10-18: on a Japanese-only install, draw just a
    capital L and R in the corner; the Japanese font has capital Latin
    letters that fit. The Japan disc has no L1 / R1 glyph to take
    (D280's notes), and the letters are what the PSP itself printed there.
  - *Measured* ([`known-defects.md`](known-defects.md) D321): in the
    psp-jp and psx-jp caches' `ja-JP` font, glyphs `0x9DF` / `0x9E5` are a
    capital L and R, ink 14 and 16 of 24 columns (about 5 of the 8 pixels
    the corner draws a glyph in); DIV-0051's Latin icons fill all 24.
- **Also in the PSX version?** Yes - the PlayStation's labels are baked the
  same way on every release; this changes Capcom's design, not a port
  defect.
- **Verification:** `BOF3X_SHADOW=shoulder_glyphs`
  ([`button-shoulder-corner.md`](button-shoulder-corner.md) 4). The round's
  validation ([`button-routes.md`](button-routes.md) 9, 9.1):
  `sigma_items` 32 / 32 equal at the identity on `psx-us`, its corner
  unchanged on `pc` and `pc-enus`; on `psp-eu` the tiles `L` / `R` become
  `L1` / `R1` in every list frame, as intended; swapped, the tiles trade on
  all four (`161,72-174,220`). D321's amendment: the self-test plants
  stand-ins of its own in `0x66A2FC`'s six entries, then L1's and R1's
  empty, and checks the `ja-JP` branch (24 glyph codes, 0 mismatches);
  live, psp-jp's five item-list routes run to `recipe done`, and psx-jp's
  and psp-eu's `menu_screens` are pixel-equal to the base build's in all 60
  frames.
- **Amended 2026-10-21 (the owner): under `ja-JP` every corner glyph is the
  Japan disc's own.**
  - *Was:* the face buttons under `ja-JP` read the stand-ins behind
    `0x66A2FC`, which no Japanese layer writes, so a face button in a
    shoulder slot (Change on circle, say) stopped the game at the first
    item list on psx-jp and psp-jp disc-only installs, D321 again by
    another entry (found by the 2026-10-21 review of PR #91). L1 / R1 on a
    PSP base were the font's capital `L` / `R`.
  - *Now:* under `ja-JP` circle, cross, triangle and square are the
    single-byte sheet's `0x15 08..0B` (`0x9F6..0x9F9`, `button_text.h`'s
    `kJapaneseIcons`, the glyphs D279's fishing labels and DIV-0140 already
    draw; the owner confirmed the square at `0x15 0B` off a render of the
    sheet, 2026-10-21), and L1 / R1 the disc's small labels at `0xD05` /
    `0xD06` (DIV-0051's 2026-10-21 amendment). No stand-in is read under
    `ja-JP`; the draw still stops loudly if the font is too short.
  - *Verification:* `BOF3X_SHADOW=shoulder_glyphs,button_text`: 720 plans,
    24 glyph codes (the six under `ja-JP` planted over empty stand-ins), 0
    mismatches; ButtonText 180 mentions, 0 mismatches. Not yet run on a
    Japanese install.
- **Reversible?** `BOF3X_ORIGINAL=ShoulderGlyphs`.

### The hard-coded button tests ask the Config

- **ID:** DIV-0139
- **Date:** 2026-10-17
- **Subsystem:** input - the fishing, the rope pull, the crane, the well,
  the chop, the antenna's dial, the ending's skip, the dock's parts menu,
  the item lists' L1 / R1, and (group HT-2) the text's held speed-up, the
  faerie village's music room and the battle command cross's L1 / R1.
  Every function and line is in [`button-tests.md`](button-tests.md) 2 (83
  lines in 20 files of `src/game/`, and HT-2's 5 lines in 3 files, 2.4);
  the self-test in `src/input/button_tests.cpp`. The button unification
  round, groups HT and HT-2 (that doc 6, ledger draft G). Builds on
  DIV-0133's `σ`.
- **Tier:** Intent - the owner, 2026-10-17: unify every button flow (answer
  1). The same day the owner decided [`button-tests.md`](button-tests.md)
  3's remainder: the speed-up, the music room and the cross through `σ`;
  `SuperCombo_ReadButton` and the rebind left on their own buttons;
  `Scena19_Run0`'s R1 left as it is (the owner does not place that
  section).
- **Original behaviour:** these tests compare the pad with Japan's buttons
  as constants (circle to cast, cross to quit, L1 / R1 to page, and so on)
  and ignore the Config. A player who swaps Speak with Action still casts
  the rod on circle, pulls the rope and works the crane on circle, and pages
  the item lists on L1 / R1 after moving View and Change; holding circle
  speeds the text, circle plays a track in the music room and cross closes
  it, and in battle L1 points the cross at the auto battle and R1 at the
  escape (`Battle_CommandPadMasks` `0x64AE3C`). The same holds on the
  PlayStation, where the Western releases instead hard-code `P` of each face
  constant.
- **New behaviour:** each test asks `Sigma(b)` for Japan's bit `b`: the
  button the line whose Japan default is `b` is on now.
  - Exact compares stay exact; the fishing's stage 1 takes `0x74`'s four
    bits one by one.
  - The speed-up is `Sigma(0x20)`: the Speak line's button, never L2 / R2,
    which word 0 also holds.
  - The battle command cross asks `Sigma` of each of its table's masks; the
    table stays Capcom's bytes and the d-pad stays physical. The escape
    follows the View line's button and the auto battle the Change line's;
    the command boxes stay where they are.
  - At Japan's defaults (every PC install) nothing changes. A Western
    disc-only install starts at the West's Config (DIV-0134), so `σ` is `P`
    there and the tests are the PlayStation US's own. On a PSP-EU-only
    install `σ` is `S`: PSP-EU's own except at the well, whose disc tests
    `P` (known-defects D315); ours follows the Config there too.
- **Rationale:** with the icons (DIV-0135..0138, DIV-0140) and the menus
  (DIV-0133), one table decides every button: the text names the button the
  code tests, on every install and after any rebind.
- **Also in the PSX version?** Yes - every PlayStation release hard-codes
  these tests in its own terms (Japan's, `P` for the West, `S` for PSP-EU
  with the well left in `P`); this changes Capcom's design, not a port
  defect. HT-2's three were not read on the PlayStation discs; `P` and `S`
  do not move L1 or R1.
- **Verification:** `BOF3X_SHADOW=button_tests`: 555 masks under stand-in
  words (three releases' defaults and the swap) and 261 live runs of five
  functions, 0 mismatches; the 20 edited modules' fuzzes, run before `Arm`
  with `σ` the identity, 0 mismatches ([`button-tests.md`](button-tests.md)
  4, 8). The round's validation ([`button-routes.md`](button-routes.md) 9,
  9.1): after the swap the fishing's cross at 850 opens the gear menu as the
  identity's circle does, and L1 pages the item list as the identity's R1
  does, on all four installs. **Owed:** the minigames (the rope pull, the
  crane, the well, the chop, the antenna, the ram, the dock), from RC's
  placed saves; HT-2's battle, held text and music room.
- **Amended 2026-10-21: five more sites through `σ`.** The review of PR #85
  found tests the census missed (each at a jump target just past a `ret`):
  the field's ability and equipment slot lists (`SharedList_PickSlot`,
  `Equip_ChooseSlot`) and the battle's equipment slot list
  (`BattleItemCmd_EquipSlotPick`) tested a raw triangle `0x10` before or
  after cancel, so on a West install, where cancel is triangle, the cancel
  button unequipped instead of backing out; `DragonCmd_StoreSource` kept the
  first of its two triangle tests raw; and `ExtraSlots_PickItem` paged on a
  raw L1 / R1. All now `Sigma(...)` ([`button-tests.md`](button-tests.md)
  2.3). The identity is unchanged, so every self-test still compares
  Capcom's code.
- **Reversible?** `BOF3X_ORIGINAL=ButtonSigma`, DIV-0133's switch: with it
  `Sigma` is the identity and every test is the original's. There is no
  separate switch: a swapped Config with DIV-0133 off but this on would
  split the flow, so the two are kept together.

### The dialogue names buttons by code, drawn as the button the Config has put the role on

- **ID:** DIV-0140
- **Date:** 2026-10-17
- **Subsystem:** text - `tools/loc_build.py` (the build half: `convert_run`,
  `set_button_area`, `loc_build.py buttons`); `MsgBox_Step` `0x497840`
  (`src/game/msgbox.cpp`) and `Text_DrawString` `0x516B70`
  (`src/game/text_draw.cpp`, `src/game/button_text.h`), the draw half.
  **Amends the DIV-0005 family** (the language layer's text). The button
  unification round, groups DT-1 and DT-2
  ([`button-dialogue.md`](button-dialogue.md) 3.4 and 4, ledger draft H).
- **Tier:** Intent - the owner's "unify every button flow" (2026-10-17);
  fixes known-defects D272.
- **Original behaviour:** a button in running text is an icon glyph after a
  colour control, baked per release: the PC's text draws Japan's buttons
  (the port's circled numerals and `X`, read by their colour), the
  PlayStation West's and PSP-EU's draw their own (`P`, `S` of Japan's), and
  a Western layer copied those over the PC's code. So under every Western
  layer the rope pull, the fishing help and the crane name a button the code
  does not test (D272), and no text follows the Config.
- **New behaviour:**
  - **The build:** `loc_build.py` writes a button code, `05 (0xB0 + n) 06`,
    before each mention in the nine button areas (8, 30, 89, 129, 68, 75,
    111, 121, 135), naming Japan's bit `1 << n`: the release's icon read by
    `P^-1` or `S^-1` (by `P^-1` in PSP-EU's areas 68 and 121, the owner's
    answer 3), the identity for JP. The release's own icon stays after the
    code, so any reader that does not resolve it draws as before. Area 1's
    PC-only mark is not a button ([`button-dialogue.md`](button-dialogue.md)
    4.3); areas 78 and 175 are left.
  - **The draw:** `MsgBox_Step` and `Text_DrawString` resolve each mention
    at draw time to `σ(bit)`. When that is the button the text's icon
    shows, the text's bytes are drawn as they are. Otherwise the icon is
    drawn as that button from the same glyph family as the icon (a Latin
    layer's four icons and DIV-0051's L1 / R1; ja-JP's four; the port's
    stand-ins), in that button's colour (circle 2, cross 1, triangle 6,
    square 5, L1 / R1 0, the Config cell's `0x461C00`), the line re-flowing
    by the drawn glyph's width.
  - **The PC's own text** (the owner's "the colour swap", 2026-10-17): with
    no language layer, in the nine button areas, the colour before one of
    the port's stand-ins names the button (2 circle, 1 cross, 6 triangle, 5
    square); a mention whose button `σ` moves is drawn as the Config cell's
    stand-in for `σ(bit)` (`0x66A2FC`'s strings) in its colour. No new art.
- **Rationale:** the owner's answers; the PC-only player sees the same
  stand-in for a role in the dialogue as in the Config panel.
- **Known and accepted:** at the text's own release's defaults every mention
  draws as the text has it, except PSP-EU's areas 68 and 121 (three
  mentions), whose text names the West's buttons: there the draw names the
  button the unified code tests, `S` of Japan's (D314). Under a PC install
  with a Western layer, at Japan's defaults, all 32 Western mentions now
  name Japan's button - D272's fix. On a ja-JP disc-only install an L1 / R1
  draw would be a blank cell (the port's stand-ins are not in that table;
  not measured). `Text_DrawString`'s count budget loses two passes per code.
- **Also in the PSX version?** Partly - each PlayStation release's text names
  its own buttons and none follows a Config swap; the Western text over
  Japan's code (D272) is the PC's layers'. The code is ours.
- **Verification:** the build: every held disc built and diffed against the
  build before it, nothing drawn differently, the codes counted against the
  census (31 PSX mentions, 32 under each Western layer, the PSP-EU set;
  [`button-dialogue.md`](button-dialogue.md) 2.3, 3.1, 3.2); `loc_build.py
  selftest` (`selftest_buttons`). The draw: `BOF3X_SHADOW=button_text`, 180
  mentions through both readers, 0 mismatches, and the resolver over every
  held text (4.6). **No capture:** no recorded route opens these messages;
  the minigame starts are the owner's to record.
- **Reversible?** `BOF3X_ORIGINAL=ButtonText`: both readers draw every
  mention as its text has it (`=MsgBox_Step` and `=Text_DrawString` too).
  The codes alone change nothing drawn.

### A PSP-EU layer's Config screen: the PSP's words, and four of ours

- **ID:** DIV-0141
- **Date:** 2026-10-17
- **Subsystem:** text / menu - `tools/loc_build.py` `convert_config_psp`;
  the kind-7 chunk DIV-0015 applies. Rides on DIV-0090's PSP language layer
  ([`loc-build-psp.md`](loc-build-psp.md) 3.3 and its D246 addition, branch
  `fix/psp-config-text`).
- **Tier:** Sensible - the screen's words; nothing in play moves.
- **Original behaviour:** a layer built from PSP-EU alone had no Config
  chunk (the PSP's own screen has other rows), so on a PSP-only English
  install the Config screen's six labels and six controller names drew blank
  and its seventeen records kept the image's zeros (known-defects D246).
- **New behaviour:** the chunk is built from `BOOT.BIN`'s screen: 25 of the
  29 slots take the PSP's word; the four its screen has no word for take
  ours, a table of four one-word names in `loc_build.py`
  (`CONFIG_BUILTIN_LABELS`: Sound, Controller; `CONFIG_BUILTIN_OPTIONS`:
  Stereo, Mono), the Sound row's two records at the Autorun row's count and
  x. The chunk is byte-identical to the PSX-EU layer's.
- **Rationale:** the owner's decision, 2026-10-17: fill the slots with the
  words, none more than a one-word name of a function; quoted labels naming
  a game function are fine in code (the owner's stance, 2026-10-10). The
  PSP's word first, the PSX-EU layer's where a PSX-EU disc is held (never on
  this path: that disc builds the layer itself), then ours.
- **Also in the PSX version?** No - the words are the PlayStation's (the
  PSX-EU and PSX-US screens read the same); the PSP itself has no Sound row
  and names the Controller row Button Config.
- **Verification:** `loc_build.py selftest` (`selftest_config_psp`,
  synthetic); every other layer byte-identical before and after; the Config
  screen and its Controller panel on a PSP-only install, headless on
  `sigma_config.txt` (harness root `C:\Users\kerok\b_d246`): blank before,
  filled after and pixel-equal to the PSX-US install's; PSX-US and PC +
  `en-US` 32 of 32 frames equal before and after (D246). Not seen by the
  owner.
- **Reversible?** No toggle; a layer built from a PSX disc, or from the PC
  install, is unchanged.

### A sprite with `+0x24` bit 3 is drawn into VRAM, as on the PlayStation: Amalgam's melt comes back

- **ID:** DIV-0142
- **Date:** 2026-10-17
- **Subsystem:** the sprite draw (`Sprite_Draw` `0x5935B0`, ours,
  `src/game/sprite_draw.cpp`; [`known-defects.md`](known-defects.md) D317,
  issue #84)
- **Tier:** Intent - a restoration. The code plainly asks for the draw (it
  clears the page, sets the bit, and draws from the page), and the
  PlayStation does it.
- **Original behaviour:** Capcom's `Sprite_Draw` never tests bit 3 of
  `+0x24`. A sprite meant to be drawn into VRAM (0x340, 0x100) is drawn on
  the screen instead, every frame, at the point its code moved it to, and
  the page stays as `Gfx_ClearRect` left it. Amalgam's death leaves the boss
  frozen at (50, 65) in its last pose for 43 frames: the melt draws from an
  empty page, so nothing shows, and then the boss is gone. Mikba's death,
  magic_s14's four images and effect kinds 0x6B, 0x61, 0xA1 / 0xA3 and 0x9D
  meet the same lack (D317).
- **New behaviour:** the PlayStation's branch (twin `0x8014C3C8`, test at
  `0x8014C538`). After the cull, with bit 3 set:
  - the point moves as the PSX moves it (`+0x2E` += 0x340, `+0x30` += 0x10 +
    240 x (`Gfx_BufferIndex` ^ 1)), so the object is culled on the next frame
    and the capture lasts one frame;
  - no primitive and no cell record are taken;
  - the frame is composed as `D3d_BuildCellTexture` composes it (`DrawCells`'
    decode, texel colour 0 clear, later cells over earlier) and laid into
    `Gfx_VramShadow` at (0x340 + x, 0x100 + y);
  - it is scaled and mirrored as the Direct3D end draws the primitive
    (nearest texel), shaded as the GPU modulates (texel x shade / 0x80, each
    channel to 31), and, with bit 5 of byte 0, blended by the tpage's mode
    over the page for texels with their top bit set;
  - it is clipped to the PSX's draw area (0x340, 0x100, 0xC0, 0x100), and
    then `Gfx_InvalidateTextures` runs over that area, which also releases a
    DIV-0125 grab there.

  The melt's SPRT and FT4 then draw the captured boss, and the CPU readers
  of D297 read it back. Not modelled: the `DR_AREA`'s clip of the
  primitives that follow in the ordering table (the PC draws none into
  VRAM), the GPU's dither, and the blend between overlapping cells (the PC
  composes them first, as it does for every sprite). The picture is at 1x,
  the shadow's resolution.
- **Rationale:** a player's report (issue #84), and the PSX twin rule: the
  PlayStation draws the capture, so its absence is a port defect.
  Amalgam's own code is the same on both machines (D317).
- **Also in the PSX version?** Yes: this is what the PlayStation does.
- **Verification:**
  - `BOF3X_SHADOW=sprite_draw`: the start-up fuzz against Capcom's clone
    runs before the switch is armed: 24,000 rounds, 0 mismatches, unchanged.
  - The capture's own check: one three-piece object (one cell flipped) over
    a 4-bit page and a palette of the test's own, captured plain, mirrored,
    scaled x2 and shaded half. Each picture equals one worked out texel by
    texel (1,225 texels, 0 mismatches). Nothing else is written, the point
    takes the PSX's shift, and no primitive or cell is taken. The control
    (the piece flip ignored) is refused in all 4 cases.
  - The live check skips calls with the bit set.
  - `'*'` headless, narrow and at `BOF3X_WIDE` 1 and 2.
  - Not seen in play: no recipe reaches Amalgam (area 28) or Mikba, and the
    owner's eye is the measure.
  - **Amended 2026-10-23:** first reached in play by the owner's route
    `ropeAndAmalgam.txt` (issue #111): the capture runs (one line, object at
    (50, 65) into VRAM (0x372, 0x141)), but the melt still drew nothing. A
    second defect, Capcom's, came after it: a direct-colour page's texels all
    have alpha 0 and are discarded (D333). DIV-0159 fixes that; with both,
    the melt plays.
- **Reversible?** `BOF3X_ORIGINAL=SpriteCapture` (Capcom's draw), or
  `BOF3X_ORIGINAL=Sprite_Draw`.

### An area change that keeps the track while it fades to a stop keeps none: Ryu's dream is silent, as on the PlayStation

- **ID:** DIV-0143
- **Date:** 2026-10-18
- **Subsystem:** the area's music (`Area_PickMusic` `0x5953B0`, ours,
  `src/game/window_task.cpp`; [`known-defects.md`](known-defects.md) D318,
  issue #88)
- **Tier:** Sensible - a port repair. The PlayStation is silent here, and
  the scene stops the song on purpose just before the change.
- **Original behaviour:** with `Field_ScriptFlags` bit 0x80 set, the area
  change keeps the current track (`0x904131` into `0x904CD0`) even while a
  `Music_FadeOutStop` is still running. The fade then marks the track
  stopped (0xFF), and `Area_Enter` plays the kept track from the top. In
  `Scena01_Scene02` step 2 the bedroom's song (track 14) therefore comes back
  eight frames after its stop and plays through Ryu's dream.
- **New behaviour:** in that branch, with a stopping fade still running
  (`Music_FadeStops` set, `Music_FadeCount` not 0), the kept track is 0xFF.
  `Area_Enter` plays nothing and marks the track 0xFF, and the fade finishes
  its stop. The wake's area change (step 0xA, the bit cleared) plays the
  zone's track 7 as before. A kept track with no stopping fade is copied as
  before.
- **Rationale:** a player's report (issue #88) and the PSX twin rule.
  - The PlayStation's stop there (`0x8015DDF4`, `SsSepStop`) is immediate
    and leaves its current-track byte alone.
  - The kept track then equals the current one, and the area's play
    (`0x80162514`) skips it.
  - Silence is what the scene gets there, measured live in the sibling
    recomp (D318).
  - The PC's fade-out-stop is the one difference.
- **Also in the PSX version?** Yes: this is what the PlayStation does.
- **One knock-on:** the bedroom's song now fades over 27 frames into the
  black rather than 10. The area's track is 0xFF and differs from the song
  still fading, so `GameMode_Field` (`0x4959F0`) does what it does at any
  change to a different track: `Music_FadeOut(16)`, then
  `Music_FadeOutStop(10)`. The song is fully stopped at f10848 instead of
  f10831. The PlayStation stops it at once. Left as the game's own
  transition fade.
- **Verification:**
  - The `whelpBoss` route under `BOF3X_MUSIC_LOG=1` on the PC install
    (`pc-en150`) and on PSP-EU alone, root `C:\Users\kerok\ab88`. On both,
    no `Music_Play` runs from the bedroom's stop (f10821) to the wake's
    track 7 (f12708, the same frame as before), and the dream is silent.
  - Against the PC's run before the change, the whole music log differs
    only there: the replay of 14 at f10839 is gone, and so are its two
    later no-ops; the fade pair above is new; and the wake's fade of 14
    (f12690 / f12707) is gone with nothing left to fade.
  - Both runs reach area 10 at frame 13,170, as before.
  - `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=window_task,sound,mode_flow` from
    the ini-less launcher copy exits 0: `window_task` 33,600 rounds,
    `mode_flow` 41,000 and `sound` 46,000, each with 0 mismatches. They run
    before the switch is armed, and the window_task coverage line shows the
    keep branch ("music kept 352").
  - Not heard yet.
- **Reversible?** `BOF3X_ORIGINAL=AreaKeepStoppingNone` (Capcom's keep),
  or `BOF3X_ORIGINAL=Area_PickMusic`.

### The fishing banner draws no glyph for an empty line, nor for a one-byte code below 0x26

- **ID:** DIV-0144
- **Date:** 2026-10-18
- **Subsystem:** the fishing spot's banner (`EffectKind0F_LineType` `0x466310`,
  ours in `src/game/effect_1a.cpp`; `EffectKind0F_DrawGlyph` `0x469AD0`, ours
  in `src/game/effect_1b.cpp`; [`known-defects.md`](known-defects.md) D279)
- **Tier:** Sensible - a guard. The original turns an empty string into a
  glyph past the font. The data the PC ships never asks it to.
- **Original behaviour:** while a character is being typed, `LineType` draws
  the character under its cursor (`+0x50`) through `DrawGlyph` before it
  counts the string, so an empty line draws its NUL. `DrawGlyph`'s one-byte
  branch makes the glyph `c - 0x26`: a NUL becomes `0xFFDA`, a space
  `0xFFFA`, a cell about 18.9 MB past the glyph table. That is garbage on the
  screen, or an access violation in `Font_UnpackGlyph` when the read lands on
  unmapped memory. The banner's button label (the one-character string behind
  `0x66A2FC`, drawn by `LineNext`, `LineScroll` and `LineFade`) reaches the
  same branch. On a Japanese disc-built image no layer wrote either, and
  PSX-JP crashed at the fishing spot on every run of the live end batch,
  PSP-JP on one run in five.
- **New behaviour:** two switches, armed after every module's self-test.
  - `FishingEmptyLine`: `LineType` with its cursor on a NUL draws no cursor
    glyph and returns (nothing is typed yet: `+0x49` is 0). In that state the
    cursor rests on a NUL only when the line has no character; at a line's
    last NUL the other branch has already set the pause and moved on. A NUL
    with characters typed is a Fatal (it cannot happen).
  - `FishingLowGlyph`: `DrawGlyph` draws nothing for a one-byte code below
    `0x26`, under any layer. That is a NUL, and the space a Japanese layer's
    lines hold (the disc's word separator `0xFF` is the PC's `0x20`).
    DIV-0069's rule, a space draws nothing under a Latin layer, is unchanged
    beside it.

  The PC's own strings are never empty and hold no one-byte code below
  `0x26`, so nothing they draw changes.
- **Rationale:** the owner's decision of 2026-10-13, confirmed 2026-10-18:
  fix D279 both ways. One is a Japanese layer that writes the words
  (DIV-0069's amendment of the same day). The other is this guard, which
  keeps any empty string from reaching the font. It covers a layer built
  before the fix, a disc whose module is not found, and the label entries no
  module holds (square, L1 and R1, drawn only for a moved button).
- **Also in the PSX version?** The PlayStation's strings are its own disc's,
  and none is empty there, so the question does not arise. The twin's typing
  state was not read for this. The guard is the port's.
- **Verification:**
  - `BOF3X_SHADOW=fishing_empty` (new; it runs in `FishingEmpty_Arm`, before
    `FishingText_Arm`, so the label is the original's read). An empty line is
    planted in text record 1 and a NUL in all six `0x66A2FC` entries, with a
    record of ours as `Sprite_Current`. `LineType` over the empty line (a
    typing frame and the last frame), `LineNext`'s label, and `DrawGlyph` of
    a NUL and of a space leave `Gfx_PacketNext` where it was. The calls that
    reach `DrawGlyph` are counted: none from `LineType`, one from
    `LineNext`'s label (declined there). The control, with
    `FishingEmptyLine` off, reaches the draw once. 13 checks, 0 mismatches.
  - effect_1a's and effect_1b's fuzz run before the switches, so they still
    compare Capcom's draws: `BOF3X_SHADOW='*'` hosted under `ja-JP` and
    under `original`, each `inject: 10087 ours`, 1,086 `MISMATCHES` lines
    all 0, no `FATAL`.
  - Live: `caughFish` to its end on psx-jp (3 runs) and psp-jp (4 runs), no
    crash (before: every psx-jp run, one psp-jp run in five); on the PC
    install with `en-US` the run with both switches off is pixel-equal to
    the run with them on, 129 of 129 frames
    ([`known-defects.md`](known-defects.md) D279).
- **Reversible?** `BOF3X_ORIGINAL=FishingEmptyLine` and / or
  `BOF3X_ORIGINAL=FishingLowGlyph` (the original's draws; on a Japanese
  disc-only install with a layer built before the fix the crash comes back).

### F6 saves the whole game state to a quicksave and F7 restores it to the frame

- **ID:** DIV-0145
- **Date:** 2026-10-19
- **Subsystem:** the window procedure's F6 and F7 and the loop's point
  between logic frames (`Game_WndProc` `0x4FC6F0`, `WinMain` `0x4FCB00`, ours
  in `src/game/win_main.cpp`; the snapshot core `src/hook/snapshot.{h,cpp}`
  and its participants `ST`, `SOUND` (`src/game/sound.cpp`) and `GFX`
  (`src/render/gfx_snapshot.cpp`); [`quicksave.md`](quicksave.md))
- **Tier:** Extension - save states, which the original never had.
- **Original behaviour:** F6 did nothing. F7 selected the next renderer
  device and showed its name (nothing since DIV-0040). There were no save
  states: F12's `Save_QuickWrite` writes an ordinary save to slot 0
  (`BISLPS00.DAT`), the field's state only.
- **New behaviour:** F6 saves the game's whole state between two logic
  frames (the `.data` / `.bss` span, the task stacks below the main thread's
  stack, and the participants' records) to `BISLPS10.DAT`, after moving the
  existing quicksaves up a slot by renames (`1E` -> `1F` .. `10` -> `11`, the
  old `1F` deleted), so the sixteen slots hold the sixteen newest. F7
  restores the newest to the frame; the F5 menu (DIV-0147) restores any.
  Each file starts with an ordinary save file whose summary the menu draws.
  A file from another build, or from a process whose DLL or task stacks sit
  elsewhere, is refused with its reason and nothing is written; so is one
  taken under the original Chinese text and loaded under a translation, or
  the reverse (from 2026-10-20; before, that load ended the game in a Fatal
  part-way through), and one whose music file this launch reads differently
  - another `BOF3X_MUSIC`, cache layers or `BGM` (from 2026-10-21, the same
  Fatal before). One translation's quicksave loaded under another is
  let through: the take's text shows until the game reloads its own
  ([`quicksave.md`](quicksave.md) 4b, IDEAS I47).

  Sound after a quick-load: the music resumes at the sample heard at the take
  (for an MP3 on a track that rewinds, the start of its 26 ms frame), at the
  take's volume and fade; looping sound effects and a playing voice stream
  resume at their positions; a one-shot sound effect playing at the take is
  not resumed. The first frame shown after F7 is the picture from before the
  load (one frame).

  F6's and F7's confirmation lines ("Quicksave OK", "Quickload", a refusal)
  reach the log only while our back buffer is in use, as F12's "Save OK"
  does (DIV-0032); the F5 menu shows its refusal on screen.

  Files `BISLPS10.DAT`..`BISLPS1F.DAT` (and a transient `BISLPS10.TMP`) in
  the game directory, about 3.9 to 4.7 MB each (I45). `bof3x.dll` is linked
  at a fixed base (`0x10000000`, relocations kept) so a quicksave loads in a
  later launch of the same build; a build setting, not game behaviour.
- **Rationale:** the owner's design of 2026-10-17 (IDEAS I13, I18;
  [`quicksave-round.md`](quicksave-round.md) section 5): save states restored
  to the exact moment, battles and events included, which F12's
  save-anywhere is not; the fixed base the owner's call of 2026-10-19. A
  quicksave is native memory, so loading someone else's is trusting it like
  a program (IDEAS I46).
- **Also in the PSX version?** No. Emulators' save states are the nearest
  thing, and they hold an emulated machine's memory, not the process's own.
- **Verification:** the self-tests `snapshot`, `gfx_snapshot`, `SOUND`
  ([`quicksave.md`](quicksave.md) 7, the SP-G section, 10); the proof batch
  ([`quicksave-routes.md`](quicksave-routes.md) 7): six routes in-process on
  the PC install under the state hash's conditions and the player's
  defaults, on a US disc-only install and in wide mode, and across two
  processes on the PC and PSP-EU installs. No page of the game's own state
  differed after a restore beyond SN's overlay count and the display parity
  at the first tick, nothing crashed in 46 runs, and all 22 capture pairs
  (the frame k ticks after F6 against the frame k ticks after F7) are
  pixel-equal. Re-checked after `main`'s typed-data round (PR #94) merged.
  The language refusal (2026-10-20): the `snapshot` self-test in the exe
  host under the original and under en-US, both passing.
- **Reversible?** The keys are ours; without them nothing of the game
  changes. Not a switch.

### The save list is BISLPS0?.DAT and bounded at sixteen; the quicksaves list apart

- **ID:** DIV-0146
- **Date:** 2026-10-19
- **Subsystem:** the save list (`Save_ListFiles` `0x4548B0`,
  `Save_ReadSummaries` `0x588DC0`, ours in `src/game/save_menu.cpp`;
  [`save-files.md`](save-files.md) 2; [`quicksave.md`](quicksave.md) QM)
- **Tier:** Sensible - the quicksaves need their own list, and the
  original's table has no bound ([`known-defects.md`](known-defects.md) D322).
- **Original behaviour:** `Save_ListFiles` lists `BISLPS??.DAT` into the
  sixteen `0x18`-byte entries of `Save_Directory`, with no bound, and
  `Save_ReadSummaries` gives each entry the slot of its `name[7]`, one hex
  digit.
- **New behaviour:** the save list is `BISLPS0?.DAT`, at most sixteen
  entries (a seventeenth match, or a name too long for an entry, is logged
  and not stored). The two functions take the pattern and the tables, so
  the quick menu lists `BISLPS1?.DAT` into tables of its own. The title's
  load menu and the inn's save menu never list a quicksave.
- **Rationale:** the quicksaves (`BISLPS10..1F.DAT`, DIV-0145) match the
  original's pattern and their `name[7]` is a hex digit: `BISLPS10.DAT`
  would show as save slot 0, and sixteen saves plus any quicksave overrun
  the table. The owner, 2026-10-17: the quicksaves are reached from F5 only.
- **Also in the PSX version?** No: a memory card has no file listing of
  this kind; the PC port's own.
- **Verification:** the quick menu self-test: a directory of seventeen saves
  lists sixteen with the seventeenth logged and the table's guard unwritten;
  the quick list on slots 0, 3 and F; the original pattern measured showing
  `BISLPS10.DAT` as slot 0. Hosted, the `save_menu` fuzz, 66,000 rounds, 0
  mismatches after the split ([`quicksave.md`](quicksave.md) QM.3).
- **Reversible?** `BOF3X_ORIGINAL=SaveLists` (the original's pattern and its
  unbounded table, quicksaves included in the list).

### F5 opens a quick menu drawn as the title's load menu, to load any of the sixteen quicksaves

- **ID:** DIV-0147
- **Date:** 2026-10-19
- **Subsystem:** the window procedure's F5 and the loop's paused branch
  (`Game_WndProc` `0x4FC6F0`, `WinMain` `0x4FCB00`, ours in `win_main.cpp`;
  `src/game/quick_menu.cpp`; [`quicksave.md`](quicksave.md) QM)
- **Tier:** Extension - a menu the original did not have.
- **Original behaviour:** F5 did nothing but end a pause, as any key does.
- **New behaviour:** F5 pauses and opens the quick menu, drawn as the
  title's load menu: its backdrop, title box and "Load which file?", the
  save slots (the sixteen quicksaves newest first, three shown), and on a
  slot with a quicksave its "Load game?" Yes / No with the pointing hand (an
  empty slot takes no confirm). Yes restores that quicksave to the frame
  (DIV-0145); a refusal keeps the menu open with the load menu's "File is
  damaged" and the reason in the 8 x 8 font; No returns to the slots; a
  second F5 or cancel closes it. Confirm and cancel are the release's
  (DIV-0133). The menu reads the pad itself while paused, plays no sound,
  and writes none of the game's menu state.
- **Rationale:** the owner's design of 2026-10-17 (round doc 5, answers 2,
  3 and 5): F5 the only way to the quicksaves, load only, sixteen slots
  newest first, three shown as the save menu shows them; and their call of
  2026-10-19 on the first captures: the save screen's background and its
  load yes / no with the hand, in the game's own font and text.
- **Also in the PSX version?** No quicksaves there; ours.
- **Verification:** the quick menu self-test (the cursor over 0..15 with
  gaps, no question on an empty slot, Yes first, No and cancel, a load only
  at the release); live, three quicksaves listed and the second loaded back
  to its frame, a damaged file refused with the menu kept open; the recipe
  `quick_menu.txt` in the proof batch.
- **Reversible?** Not a switch: F5 is ours (nothing of Capcom's is
  replaced; without the key nothing changes).

### The staff roll follows the language: a US or PAL layer rolls its release's credits

- **ID:** DIV-0148
- **Date:** 2026-10-20
- **Subsystem:** the ending's roll (`Scena17_ScrollRoll` `0x56D110`, ours in
  `src/game/scena_sc15.cpp`, `ScenaSc15_RollApply`; `Scena17_RollLines`
  `0x661A20`); the language layer's kind 17 (`FIRST.DAT`, `tools/loc_build.py`
  `build_all`, `tools/exe_roll.py`, `tools/dat.py` `KINDS`;
  `src/game/dat_load.cpp` `TakeChunk`)
- **Tier:** Sensible - the release the player's language comes from
  (DIV-0115's stance, the PlayStation's data where both exist).
- **Original behaviour:** the PC port rolls the Japanese release's credits
  under every language: `Scena17_RollLines` (351 lines, 173 blank) and the
  strings after `Scena17_EndSteps` equal the Japan disc's
  `SCENARIO/SCENA17.EMI#0`; the en-US layer and the original text roll the
  same frames (82 of 84 shots pixel-equal, the two others the fade into
  "Fin"). The US and PAL discs carry rolls of their own: other job titles
  ("Engine Design" for "System Planner"), no naming contest, testing and
  localization blocks (the US only), 345 lines on the US disc and 293 on the
  European, French and German discs, which are byte-identical to each other.
  A disc-only install of a Western disc drew neither: the image's pointer
  words are the PC's, so its lines named the PC's offsets while the map had
  placed stretches of the disc's text there (D323).
- **New behaviour:** the roll follows the language played. A layer built
  from a US or PAL PlayStation disc (en-US; en-150, fr-FR, de-DE) carries
  that release's roll as kind 17 of its `FIRST.DAT`: the disc's line table,
  the 14 entries after its end (the "Presented by" card is the fifth) and its
  text. When the layer is walked the engine copies it, and
  `Scena17_ScrollRoll` takes its lines from it - on a PC install with that
  layer and on a disc-only install alike. The original text, a ja-JP layer
  and a layer from a PSP disc carry none and roll `.data`'s table, the
  Japanese release's (the Japan disc's roll is the PC's; the PSPs' place the
  PC's bytes). `.data` is untouched (its pointer words stay the PC's), so the
  state hash and the image's checks are as before. The title's mark stays
  the base's (DIV-0114): a PC install under en-US keeps the (R) and rolls the
  US credits.
- **Rationale:** the owner, 2026-10-20: "let western installs use their own
  roll", then "let's have the roll track with localization", after the
  renders of the PC's roll and of the patchwork.
- **Also in the PSX version?** Yes: this is each release's own roll, read
  from the player's disc.
- **Verification:** `tools/exe_roll.py`'s synthetic self-test (in
  `loc_build.py selftest` too); the Japan disc's roll extracted the same way
  equals the PC's table word for word (365 of 365: the 351 lines and the 14
  after the end, every string equal). Layers built by the importer: kind 17,
  tag 1, 4,283 bytes on the en-US layer of a PC + US cache and of a US
  disc-only cache, 3,907 bytes on an EU disc-only cache's en-150. The ending
  (`ending17`, shots every 300 frames, scratch
  `C:\Users\kerok\bof3x-runs\away1020`): the PC install under en-US and the US
  disc-only install both log `DIV-0148: the language's staff roll, 345 lines`
  and roll the US credits to "Presented by CAPCOM" and "Fin"; the EU
  disc-only install logs `293 lines` and rolls the PAL credits to the same end; the PC install's original text rolls the
  Japanese credits as before. `data_census.py --check` 27,406 sites.
- **Reversible?** Rebuild the language without the chunk, or play the
  original text: the roll then reads `.data`'s table (on a Western disc-only
  install, D323's patchwork). Not a runtime switch.

### An event battle with no music of its own leaves the scene's song playing

- **ID:** DIV-0149
- **Date:** 2026-10-21
- **Subsystem:** music at a battle's start (`GameMode5_Load` `0x495FD0`, ours
  in `src/game/mode_rest.cpp`)
- **Tier:** Intent - the PlayStation's behaviour, heard by the owner.
- **Original behaviour:** five event battles' files carry no music (their
  track word in `EventBattle_Records+0xE2` is 0xFF; the miners' fight at the
  start is one, issue #83). `GameMode5_Load` fades the scene's song out over
  10 frames and calls `Music_Play(0xFF, 0xA)`; song 255 has no file, so
  `Music_LoadFile` fails and `Music_Play` starts the file still loaded - the
  scene's song - again from its top, fading in (D24's path, which the PC's
  own data reaches here). The PlayStation's `GameMode5_Load` (`0x80198C94`)
  names no song and the battle file brings none, so the scene's song plays
  on.
- **New behaviour:** when the event battle's track word is 0xFF, neither the
  fade nor the `Music_Play` happens: the song playing goes on into the fight
  without a break, and the log says `DIV-0149    event battle N has no music
  of its own: track T plays on`. Every other battle is as before. The end
  of the fight is unchanged (DIV-0092: the fanfare's stop, then the zone's
  song).
- **Rationale:** the owner, 2026-10-21, playing the PlayStation release on
  an emulator: the music starts slightly before the fight and continues into
  it without restarting. A PC-only difference is a port repair (the PSX twin
  rule); found by the 2026-10-21 review of PR #86.
- **Also in the PSX version?** This is the PSX's behaviour brought back.
- **Verification:** `BOF3X_SHADOW=mode_rest` (Capcom's rule, before the
  switch is armed): 52,000 rounds, 0 mismatches; the switch logs ON. Not yet
  heard in the PC build.
- **Reversible?** `BOF3X_ORIGINAL=EventBattleKeepsMusic`.

### The music comes from a chosen source first: the PlayStation's, the PSP's or the PC's

- **ID:** DIV-0150
- **Date:** 2026-10-21
- **Subsystem:** music - `Music_LoadFile` `0x587A20` (ours, `src/game/sound.cpp`),
  the sequencer's seam (`src/game/music_seq.cpp` `Arm` / `Find` / `StreamPath`),
  `Sound_LoadStream` `0x587910`'s kind 0 (ours, `src/game/save_menu.cpp`);
  `importer.py build` (`snd_from_psx`, `[snd_jingles] dir`); the launcher's
  Music box (`src/launcher/config_dialog.cpp`, `config.cpp` `ConfigMusicSource`),
  the Jukebox's PlayStation streams, and `recipes/suites.toml` (the audio
  suite loses its music member to the Music box)
- **Tier:** Intent - the owner's decision, 2026-10-21.
- **Original behaviour:** the PC port plays its MP3s (`BGM\NNN.DAT`) and its
  11 recorded streams (`SND\NAME.DAT`, the kind-0 pieces: the inn's
  `OYASUMI`, the staff roll's `PURE`, ...). Before this entry the port's own
  order was: a song from a PSP layer `opt=` names (DIV-0109), then the cache's
  `base\bgm` (DIV-0087), then the MP3; `music=mp3` the MP3 alone. The
  streams: a named layer's `snd\`, then `base\snd\`, then `SND\`
  (DIV-0089, DIV-0110) - and a cache with a PC install among its sources had
  no disc streams at all (DIV-0129), so the Jukebox listed the PlayStation's
  greyed.
- **New behaviour:** `BOF3X_MUSIC` (the ini's `music=`) names a source, and
  the songs and the 11 streams come from the first place in that source's
  order that has the piece:
  - `psx`: the PlayStation's (`base\bgm`; the streams `base\psx-jingles`,
    then `base\snd`), then the PSP's (`opt\psp-music\bgm`, `opt\psp-snd\snd`;
    a PSP-only cache's `base\`), then the PC's (`BGM\`, `SND\`);
  - `psp`: the PSP's, then the PlayStation's, then the PC's;
  - `pc`: the PC's, then the PlayStation's, then the PSP's (`Music_LoadFile`
    and the quicksave's size probe try the MP3 names first).
  The PSP's layers are read by this choice, whether or not `opt=` names them.
  `seq` and an unset `BOF3X_MUSIC` keep the order above as it was, and `mp3`
  the MP3s alone. The launcher always hands the game a source: the ini's, or
  for an empty `music=` (or `seq`) the PSP's when the layers it hands the game
  name `psp-music` (the audio suite's PSP side before this entry), else the
  PlayStation's; `mp3` is `pc`. Its Music box lists the three as "PlayStation
  Encoded", "PSP Encoded" and "PC MP3s", one this install lacks saying so; an
  untouched box writes `music=` back as read. The importer now makes the 11
  PlayStation streams whenever a PSX disc and ffmpeg are both among the
  sources; beside a PC install they go in `base/psx-jingles/` (so the unset
  order above never plays them over the PC's), and the manifest's
  `[snd_jingles] dir` says where. The game logs the order at start-up
  (`DIV-0150    music from ... first`) and each stream as it starts.
- **Rationale:** the owner, 2026-10-21: the Music tab's dropdown as three
  sources, "the game can then use the chosen source for all music (when
  available)"; with either disc chosen the other disc second and the PC
  third, with the PC chosen the PlayStation second and the PSP third. The
  Jukebox already played each source on its own; the PlayStation's streams
  were missing from it on a mixed install.
- **Also in the PSX version?** No: the PlayStation plays its own sequences
  and XA. Which recording plays is the player's choice here; the pieces are
  the same music, and a stream's length (`Sound_StreamDone`'s waits: the
  inn, the staff roll) differs between sources by the encoders' padding.
- **Verification:** `BOF3X_SHADOW=sound`: the DIV-0150 self-test (the PC's MP3
  names opened before the cache's song; the PSP's song before the
  PlayStation's; a stream from the cache, from the install, in neither, the
  install first, no source chosen) and DIV-0087 / DIV-0109's, exit 0. The
  launcher host tests: 260 checks, 0 failures (`ConfigMusicSource`,
  `[snd_jingles] dir`, the audio suite without its music member).
  `snd_from_psx` with the PC install and the US disc: 11 streams in
  `base/psx-jingles/`, `base/snd/` the 880 and `VOICE00`..`04` as before.
  `BOF3X_SHADOW=save_menu` 66,000 rounds, 0 mismatches (its fuzz drives
  `Sound_LoadStream` before `Arm`, so no source is chosen there).
  Live, `ending17` to the staff roll on a PC install at x8 (2026-10-21): with
  a copy of a PC + US cache given `base/psx-jingles/`, `psx` logs `stream PURE
  from ...\base\psx-jingles\PURE.DAT` and `pc` `SND\PURE.DAT`; on the owner's
  cache (read only) `psp` logs `...\opt\psp-snd\snd\PURE.DAT` and its songs
  from `opt\psp-music\bgm` first. On a PSX-US disc-only install the roll
  played `PURE` from `base\snd` and was heard. **Not yet played with the
  launcher's box.**
- **Reversible?** `music=seq` in `bof3x.ini` (or `BOF3X_MUSIC=seq`) for the
  order before this entry; the streams beside a PC install are then the
  PC's again.

### The table faerie in the faerie village is drawn over its table

- **ID:** DIV-0151
- **Date:** 2026-10-21
- **Subsystem:** field draw order (`Sprite_DrawPass` `0x593060`, ours in
  `src/game/draw_pass.cpp`; the rule in `src/game/layering.cpp`, beside
  DIV-0071's; [`faerie-fix.md`](faerie-fix.md))
- **Tier:** Intent - the owner's decision, 2026-10-15: a faerie the scene
  stands on the table is drawn on the table, not through it
- **Original behaviour:** in area 175 (the faerie village's wooden room
  after the Faerie Tiara; `dolphinBoss.txt` recipe frames about
  960..2,850) the gold-winged faerie at the front left of the two-plank
  table is placed at floor height inside the left plank's raised cell. Its
  layer is the right plank's row, and a raised cell's top is in its layer's
  second list, drawn after the layer's sprites, so the right plank's top
  cuts the faerie's skirt on a straight diagonal for the whole scene; the
  plank's side triangles hide the faerie's shadow at its floor point
  ([`faerie-over-table.md`](faerie-over-table.md) 1-3, known-defects D300).
  Capcom's: the same with `BOF3X_LAYERING=0` and with Capcom's code; the
  PlayStation's twins of the three deciding functions order it the same
  way; the placing data are the disc's; the owner's PlayStation look
  (2026-10-15) shows the same cut.
- **New behaviour:** that faerie alone - a field object of area 175 running
  the area's movement script 1 (object `+0x83`), whose feet stand below its
  own map cell's lowest corner - is drawn one layer later when DIV-0071 left
  it in its own layer, judged by DIV-0071's test with one exemption: the
  tops and sides of cells as high as its own in the second list it stands
  in. The side triangles of those cells within reach of it are drawn again
  right after it (copies of their primitives, after a draw mode as
  `DrawLayer_Close` commits), so they still hide its shadow; if any other
  sprite would come under those copies, the faerie stays where it was.
  `BOF3X_LAYERING_LOG=1` logs each pass it is judged (`D300 fix`).
- **Rationale:** the scene stands the faerie on the table; the cut comes
  from where its object is placed, not from anything meant to stand in
  front of it. Fixed for this scene only, on the owner's word. **To be
  broadened** to [`faerie-over-table.md`](faerie-over-table.md) section 4's
  general rule - any sprite standing inside a raised cell drawn after its
  layer's second list, where everything there that reaches it is a top or
  side of cells of its own height - if more examples of the class turn up;
  known-defects D242's gene crystal would be the second, the same class from
  the other side.
- **Also in the PSX version?** Yes, the same cut (above): a change to the
  original's picture, not a port repair.
- **Verification:** [`faerie-fix.md`](faerie-fix.md) 3-5: 19 captures at
  4:3 and 19 at 16:9 against `main`, differing only in the faerie's box; 19
  identical with `BOF3X_LAYERING=0`; the state hash on the ten routes,
  narrow and at both wide widths, equal to `main`'s build up to run-to-run
  noise, and narrow with the switch off as `cn_<route>_a1020` against the
  `orig3` pairs; the self-tests exit 0 with no mismatch; no `D300 fix` line
  on any of the ten routes. The owner accepted the before/after sheets
  (2026-10-21, "the faerie looks great"), the sides' second draw with them.
  **Not seen in play.**
- **Reversible?** `BOF3X_LAYERING=0` (with DIV-0071); Capcom's order under
  `BOF3X_ORIGINAL` (the pass is Capcom's).

### Start + Select held a second returns to the title, as on the Western discs

- **ID:** DIV-0152
- **Date:** 2026-10-21
- **Subsystem:** the frame loop - `Game_WinMain` (ours, `src/game/win_main.cpp`,
  `PadReset_Frame` after `Task_RunAll`), through the PC's own restart
  (`Game_RestartFlag` `0x6BC63C`, F9's)
- **Tier:** Intent - the owner's request, 2026-10-21 ("restore it if the pc
  version dropped it").
- **Original behaviour:** `BOF3.exe` has no pad reset. A linear disassembly of
  its `.text` finds no test of the held word `Input_Held` against `0x900`
  (Start `0x800` + Select `0x100`); the one `cmp dx, 0x900` is an area's
  counter (`area_w4e.cpp:688`). Its one reset is the keyboard's: F9, then F9
  again (`Game_WndProc`). The PC was built from the Japanese source, and the
  Japanese boot EXE (`SLPS_009.90`) has no such test either: `Main_FrameLoop`
  `0x8014AAC8` goes from the flip to `0x8014B630` with nothing between.
- **New behaviour:** after each frame's tasks, unless paused, Start and Select
  both held counts a frame; any frame without both starts the count again.
  On the frame after 60 counted, the count is cleared and `Game_RestartFlag`
  is set, which the loop reads as F9's second press: the task stack is reset
  and task 0 created again - the title. Logged `DIV-0152    Start + Select
  held 60 frames: back to the title at Frame_Counter N`.
- **Rationale:** every Western PlayStation release has it. The US boot EXE
  (`SLUS_004.22`, read 2026-10-21 from the held disc) loads the held word
  `0x80145AA4` in `Main_FrameLoop` (`0x8014ABB8`), and when `& 0x900` is
  `0x900` counts a byte `0x80143F44`. At 60, with the loader idle
  (`0x80162D00`), it clears the byte, calls `0x8015CEBC` and `0x8014B33C`,
  and creates task 0 again (`0x8014B854(0, 0x8014EA80)`); a release clears the
  byte. The European, French and German boot EXEs (`SLES_013.04`,
  `SLES_013.19`, `SLES_013.20`) carry the same test at `0x8014ABCC`. It is a
  post-Japanese change the port never had, like the others in
  `region-diff.md`. The owner remembers using it. A pad player on the PC had
  no reset at all.
- **Also in the PSX version?** The Western discs, yes; the Japanese disc, no.
  The PC's loads are synchronous (`File_LoadDone` is `mov eax, 1`), so the US
  wait for an idle loader has no counterpart. The restart is the PC's own,
  F9's, and not a port of the US calls. One difference: F9 on the title
  quits the program, but the pad reset on the title restarts it.
- **Verification:** builds clean. Live, `tools/recipes/field_padreset.txt`
  (the `adult_ryu` save to its field, Start + Select held 70 frames, then the
  title expected) written but **not yet run**: the first try pointed at a
  folder with no game, and the rerun is held while another session's test
  chain uses the game.
- **Reversible?** `BOF3X_ORIGINAL=PadReset`.

### The Holy Mantle's doubled encounter threshold stops at 255 instead of wrapping

- **ID:** DIV-0153
- **Date:** 2026-10-21
- **Subsystem:** random encounters - `Field_EncounterThresholdRoll` `0x52FEB0` (ours,
  `src/game/field_event.cpp`); known-defects D325
- **Tier:** Intent - the owner's decision, 2026-10-21 ("let's go ahead and make
  that change to the cap").
- **Original behaviour:** the threshold is the zone's base plus `Rand & 0x1F`
  (at most the zone's limit), all in a byte. When a member wears accessory
  `0x15`, the Holy Mantle, it is doubled with `shl bl, 1` (`0x52FF74`) and
  wraps. Of the seven zones (`0x660A24`, base and limit pairs) only zone 1,
  base 120 and limit 60, can pass 127. Its thresholds 120..151 become:
  - 240..254, for a roll of 0..7 (a quarter of rolls);
  - 0, for a roll of 8. That means no encounters until the next roll, after a
    battle or on entering an area;
  - 2..46, for a roll of 9..31 (seven rolls in ten), where the zone otherwise
    needs at least 120.

  Only area 83 has zone 1 in its own list (いせき2 in the developers' names,
  rectangle x 17..78, z 45..98; `Area_StateRecords` `0x668D80`). There the
  Mantle mostly makes fights come sooner. The JP twin `0x801B4CC4` stores the
  same byte.
- **New behaviour:** a doubled value that would pass 255 is 255. Zone 1 with
  the Mantle gives 240..255; every other zone is unchanged, since doubling
  never passes 254 there. The Bell Collar's halving (`0x14`) still follows.
- **Rationale:** the Mantle is meant to make encounters rarer. In the one zone
  meant to have the fewest, the wrap makes them more frequent, and players
  have long suspected the item "does not work right" (the owner). A cap keeps
  the threshold in its byte and the rest of the roll as it was.
- **Also in the PSX version?** Yes, the wrap is Capcom's on every release
  (PSX twin above; the zone table equal on PSX and PSP, `psp-stallion.md`
  5.2). This entry changes it on the PC only.
- **Verification:** builds clean. The cap is armed after the `field_event`
  self-test, which still compares Capcom's roll. Not yet run live.
- **Reversible?** `BOF3X_ORIGINAL=MantleCap`.

### The round's end clears the faster-side mark from every member and enemy

- **ID:** DIV-0154
- **Date:** 2026-10-21
- **Subsystem:** battle round end - `BattleRoundEnd_CheckFaster` `0x4302D0` (ours,
  `src/game/battle_turn_steps.cpp`); known-defects D63
- **Tier:** Intent - the owner's decision, 2026-10-21 ("let's clear the third
  character's extra-round bit just for consistency").
- **Original behaviour:** `Battle_MarkFasterSide` `0x4453C0` sets flag 0x8000
  (+0x134 / +0x114) on members 0..2 and enemies 0..7 that outpace the other
  side. In an extra round the two order builders admit only marked actors.
  The round's end clears the mark with `ecx = 2` over the members and
  `ecx = 7` over the enemies, so the third member and the eighth enemy keep
  it, and join every later extra round of the battle whether or not they
  still outpace anyone. The JP twin `0x801D4B08` has the same bounds.
- **New behaviour:** the clear covers all three members and all eight enemies,
  the setter's own bounds. A fighter joins an extra round only when it
  qualified in the round that started it.
- **Rationale:** the asymmetry is an off-by-one. Members 0 and 1 already lose
  the mark each round; the third member now does too.
- **Also in the PSX version?** Yes, Capcom's on the JP disc; this changes it on
  the PC only.
- **Verification:** builds clean. Armed after the `battle_turn_steps`
  self-test, whose F7 / F8 controls still plant the full loops against
  Capcom's and are refused. Not yet played; D63's live outline applies.
- **Reversible?** `BOF3X_ORIGINAL=ExtraRoundClear`.

### A reading row over the dialogue box's text: furigana from the player's Japanese layer

- **ID:** DIV-0155
- **Date:** 2026-10-22
- **Subsystem:** the dialogue box - `MsgBox_Step` `0x497840`,
  `MsgBox_StatePrint` `0x497B30`, `Msg_OpenScript` `0x4976D0`,
  `MsgBox_Reopen` `0x498280` (ours, `src/game/msgbox.cpp`, namespace
  `msgbox::ruby`) and `Msg_OpenSystem` `0x497710` (ours,
  `src/game/menu_windows.cpp`); `LoadDatFile` `0x454590`'s kind 18 (ours,
  `src/game/dat_load.cpp`); `Game_WndProc`'s study key
  (`src/game/win_main.cpp`); the optional text layer `ruby-furigana-ja-JP`,
  built on the player's machine by `tools/ruby_build.py furigana`
  ([`reading-row.md`](reading-row.md) 1 the contract, 2 the geometry, 3 the
  renderer; [`reading-row-builder.md`](reading-row-builder.md);
  [`reading-row-round.md`](reading-row-round.md))
- **Tier:** Sensible - presentation of the text played (a row of readings
  above it), off by default; what happens in play is unchanged except that a
  page the builder splits asks one more confirm press.
- **Original behaviour:** the box draws one script: up to three rows of
  12-unit glyphs a page on a 14-unit newline (`MsgBox_Step`,
  [`msgbox.md`](msgbox.md) 3). The PC port has no readings; the Japanese
  text (DIV-0056) prints its kanji bare. F4 does nothing (`Game_WndProc`
  leaves F2, F3 and F4 unbound).
- **New behaviour:** with `ruby-furigana-ja-JP` in `BOF3X_OPT` under
  `BOF3X_LANG=ja-JP`, each area file and message pool the layer annotates
  carries one **kind-18 side chunk** of composed messages keyed by slot (tag
  0 for the area's script block, `0x4000` for a pool with its half in header
  word 3); the arena's text block is never rewritten. `Msg_OpenScript`,
  `MsgBox_Reopen` and `Msg_OpenSystem` open the composed copy of a slot that
  has one, and the box draws it as **row pairs**: a half-height reading row
  of the same sheet's glyphs (a 6 x 6-unit quad, the 12 px cell texel for
  texel, two kana over one kanji cell) over each text row, at **pen y 2, 9,
  21, 28 from the box origin**, bottom-aligned (the last text row lands where
  the plain page's third row does, so the next-page arrow and the space under
  it are the original's), with an **even gap**: 1.5 units of clear space over
  and under a reading's ink at a pair pitch of 19, the nearest even spacing
  on the unit grid to the owner's "2 and 2" ([`reading-row.md`](reading-row.md)
  3.3, measured off the capture). **Two pairs a page**; a longer page is
  split by the builder on a `0x02` (**4,559 area pages split**, 6 in the
  pools, each one more confirm press). A reading appears with the first
  glyph of its word; the typewriter counts and paces the text's glyphs
  alone. In a reading row **`0x12`** stands for the drawn width of the k-th
  insert of the text row below (a member name, an item record, a sub-pool
  message), resolved at draw time. Built from the player's own `ja-JP` layer
  with SudachiPy and `data/furigana/readings.toml` (the sibling's sidecar,
  the owner's choices): 10,324 area messages (5,454 distinct) composed with
  33,385 readings, none dropped; 456 left as they were (narration rows past
  the box 265, timed three-row pages 131, offset-pen and effect pages 60).
  Choice options, the shout draw (`MsgBox_EffectDraw`), menus, battle text
  and every other text path are unchanged. **F4 hides and shows the
  readings** (`BOF3X_RUBY_KEY`: `f1`..`f12` or `none`; `ruby_key=` in
  `bof3x.ini`): the reading glyphs only; the layout, pages and arrow stay
  exactly as shown.
- **Rationale:** GitHub issue #97 ("Second Script", 2026-10-20) and IDEAS
  I21 (2026-09-24): readings over the Japanese text, for players learning to
  read it. The owner's calls: two pairs a page and the side chunk
  (2026-10-20, on the census); the even gap, the Latin size, the reveal with
  the word, plain options, the study key and its optional binding
  (2026-10-22).
- **Also in the PSX version?** No. No release has readings; the archival
  sibling's `jp_furigana` is its own extension, done there by re-authoring
  the script.
- **Verification:** `ruby_build.py furigana --selftest` 41,664 messages
  joined back byte for byte, 0 mismatches; `--lint` 51,031 reading rows, 0
  misplaced. `BOF3X_SHADOW=msgbox` 44,000 rounds and `menu_windows` 74,000,
  0 mismatches; `BOF3X_SHADOW=*` exit 0. The ten routes' state hash with no
  layer against the base tip's: no page differs that does not also differ
  between two runs of one build ([`state-hash.md`](state-hash.md), the dated
  line of 2026-10-22). Contact sheets of the nue route's dialogue, a split
  page, a pickup line with an inserted item, F4, and the dolphin fight, at
  4:3 and 16:9 (`C:\Users\kerok\bof3x-runs\rr_rr\shots\sheets\`, outside the
  tree). **Not yet played by the owner.** Its known costs and open items:
  D326..D329.
- **Amended 2026-10-22 (group OS): timed boxes stop, effect pages are
  composed.** The owner, the same day, on finding the new game's opening
  scene showed no readings (it was left plain whole as an effect message
  of eight timed pages): "let's add an explicit stop to the text; it will
  make the new game not comparable to before, but it makes it better for
  study."
  **What changes under a layer:** a timed close (`0x16 nn`, which shuts the
  box `nn` frames after it is reached) becomes the box's stop - the
  composed copy is the slot's message cut at its first `0x16 nn` with
  `0x00`, the box's own end, in its place, so the box waits for a confirm
  press where the timer was (state 2, no arrow, as every message end; a
  three-row page before it splits on `0x02` and shows the arrow there); and
  messages with the offset pen (`0x0D` / `0x0E`) or an effect preset
  (`0x0F nn`) are composed, the codes kept in the text row, the walk drawing
  them as `MsgBox_Step` does and a reading over a glyph under the pen moving
  with it (the shake, wander and rise offsets); a row the grow / shrink
  draw (`MsgBox_EffectDraw`, left unannotated) is drawing in shows no
  reading until the effect ends. Measured: `0x16` ends the box (state 7,
  window 0 to its shrink, then freed) and only the two openers,
  `MsgBox_Reopen` and the page restart write the walk's base, so the bytes
  after a timed close are the next slots', each composed in its own copy;
  the new game's opening is `AREA024` slots 0..7, opened one by one
  ([`reading-row.md`](reading-row.md) 3.6). With no layer nothing changes -
  the original bytes and the identity gates stand; with one, the opening is
  no longer frame-comparable to the original, which the owner accepts.
  Under the furigana layer a timed box with no reading is composed too
  (empty reading rows) so every timed box stops; a box of a space alone
  that closes on its timer (`0C 81 20 16 00`, a blank beat of three
  scenes) and narration past the box keep their timers. Numbers (area
  blocks, the held layer): 10,518 messages composed (was 10,324), 281
  timed closes made stops (83 with no reading), 59 effect messages
  composed, 4,663 pages split (was 4,559); left plain only past the box
  (62) or past three rows (202). **Verification:** `--selftest` 41,664
  messages, 0 mismatches; `check` on every composed message; `--lint`
  51,021 rows, 0 misplaced; `BOF3X_SHADOW=msgbox` 44,000 rounds 0
  mismatches and `BOF3X_SHADOW=*` exit 0 with no layer; NEW GAME's opening
  played to its last timed box under the layer at 4:3 and 16:9, every box
  holding 72 frames past its 32-frame timer until the press and the scene
  going on slot by slot (sheets `opening_furigana.png` and
  `opening_no_layer.png` in `C:\Users\kerok\bof3x-runs\rr_os\sheets\`, outside the tree). Not
  seen live: a reading over a word under the offset pen while an effect
  moves it (the opening's shout has no kanji). Not yet played by the owner.
- **Reversible?** Off by default: leave the layer out of `BOF3X_OPT` (the
  launcher's *Row above the text: None*); F4 hides the readings in play;
  `BOF3X_ORIGINAL=MsgBox_Step` (or `MsgBox_StatePrint`) disarms the repoint
  and plays every block's own message.

### A second language in the reading row

- **ID:** DIV-0156
- **Date:** 2026-10-22
- **Subsystem:** the dialogue box, as DIV-0155 (its `0x13` marker in
  `src/game/msgbox.cpp`); the optional text layers `ruby-<second>-<main>`,
  built on the player's machine by `tools/ruby_build.py second`, every held
  pair by `setup` (`tools/player_sources.py` `build_second_layers`); the
  composed font chunk in the layer's `FIRST.DAT`
  ([`reading-row-second.md`](reading-row-second.md);
  [`second-language-census.md`](second-language-census.md))
- **Tier:** Sensible - the text played shown with a second language above
  it, off by default; pages the layout splits ask one more confirm press.
- **Original behaviour:** the box shows one language. The engine has one
  glyph table, every overlay's `FIRST.DAT` carries a whole one, and the
  Latin and Japanese tables both start their cells at `0x993` (76 of the 90
  single-byte slots and both hanging slots differ;
  [`second-language-census.md`](second-language-census.md) 7), so two
  languages' glyphs cannot be resident together.
- **New behaviour:** with `ruby-<second>-<main>` in `BOF3X_OPT` (for example
  `ruby-en-US-ja-JP`, English over the Japanese played), the same message
  from the second layer - same block, same slot - stands in the reading rows
  over the main's text, by DIV-0155's side chunk, row table, reveal and study
  key. **The pairing rule** ([`reading-row-second.md`](reading-row-second.md)
  2): pages pair in order when their counts agree, else the second's text is
  shared over the main's pages in reading order by glyphs; the main keeps its
  authored rows where two fit, and a three-row page is re-broken into two (a
  speaker-name row kept as its own) or split on `0x02` - the main's breaks
  decide, never the second's; a page whose second fits one reading row is
  balanced over its two text rows. Left plain, whole, and counted: a message
  with a row past the box, a page past three rows, the offset pen, a timed
  page that would need a split, or an insert of the second the marker cannot
  reference. **`0x13`** in a reading row draws the k-th insert of the text
  row below at reading size, from the main's name table, so the second shows
  the name. Latin reading rows are drawn at **1/2** (a 4 x 6-unit letter on a
  4-unit advance), kana at 6 x 6. The layer's `FIRST.DAT` carries a
  **composed font chunk** (kind 3 with its kind-4 advances) that replaces the
  main's table when played: the main's kept whole and the second's cells
  relocated above it - Latin over Japanese at `0xE00..0xE63` (space `0xE64`;
  the French and German accented cells from `0xE70`), the 882 Japanese cells
  at `0xAC0..0xE31` over a Latin main (space `0xE40`), and Latin over another
  Latin main in the main's letters, with the accented block added where the
  main is blank. Tables of 2,673..3,719 glyphs, under DIV-0016's `0x1000`.
  **Every ordered pair of held languages** is built (20 with `ja-JP`,
  `en-US`, `en-150`, `fr-FR`, `de-DE`): 17,155..17,825 area slots paired a
  pair; pages added about 640..760 under a Japanese main and 5,000..5,800
  under a Latin main (its own third rows); the second needs more than two
  reading rows on 1.0..1.7% of pages over Japanese and almost never
  otherwise (the per-pair table, [`reading-row-second.md`](reading-row-second.md)
  7).
- **Rationale:** IDEAS I50 and issue #97: one scene read in two languages at
  once, for language study. The owner's calls: two pairs a page, overflow as
  presses, the side chunk (2026-10-20); the Latin row at 1/2, "plenty
  legible" (2026-10-22); every held pair offered in the launcher.
- **Also in the PSX version?** No.
- **Verification:** `ruby_build.py second --selftest` 0 mismatches in all 20
  pairs (74,918..79,183 messages each); `check` on every composed message
  (the main's text, and every main glyph in the original's colour); `--lint`
  0 faults in all 20; an independent read-back of the 20 layers; `--cache`
  and `--game` builds byte-identical. English over Japanese played on the nue
  route with `0x13` drawing the speaker's name (`second_en-US_over_ja-JP_*.png`
  in `C:\Users\kerok\bof3x-runs\rr_rr\shots\sheets\`). A page whose text rows
  are all empty would draw no readings (the count is text glyphs only;
  [`reading-row.md`](reading-row.md) 3) - none seen. Japanese over English is
  not captured. **Not yet played by the owner.**
- **Amended 2026-10-22 (group OS): timed boxes stop, effect pages are
  composed**, by the owner's call quoted in DIV-0155's amendment and the
  same rule:
  **What changes under a layer:** a timed close (`0x16 nn`, which shuts the
  box `nn` frames after it is reached) becomes the box's stop - the
  composed copy is the slot's message cut at its first `0x16 nn` with
  `0x00`, the box's own end, in its place, so the box waits for a confirm
  press where the timer was (state 2, no arrow, as every message end; a
  three-row page before it splits on `0x02` and shows the arrow there); and
  messages with the offset pen (`0x0D` / `0x0E`) or an effect preset
  (`0x0F nn`) are composed, the codes kept in the text row, the walk drawing
  them as `MsgBox_Step` does and a reading over a glyph under the pen moving
  with it (the shake, wander and rise offsets); a row the grow / shrink
  draw (`MsgBox_EffectDraw`, left unannotated) is drawing in shows no
  reading until the effect ends. Measured: `0x16` ends the box (state 7,
  window 0 to its shrink, then freed) and only the two openers,
  `MsgBox_Reopen` and the page restart write the walk's base, so the bytes
  after a timed close are the next slots', each composed in its own copy;
  the new game's opening is `AREA024` slots 0..7, opened one by one
  ([`reading-row.md`](reading-row.md) 3.6). With no layer nothing changes -
  the original bytes and the identity gates stand; with one, the opening is
  no longer frame-comparable to the original, which the owner accepts.
  The second's message is cut at its own first `0x16` too, so a scene of
  timed boxes pairs box for box. The plain classes "offset pen" and "timed
  page needing a split" are gone; a blank timed beat is counted "the main
  has no text" (136 area slots). English over Japanese: 17,389 area slots
  paired (was 17,332), 199 timed closes made stops, 87 effect messages
  composed, 652 pages added (was 640); every pair's numbers in
  [`reading-row-second.md`](reading-row-second.md) 7. **Verification:**
  `--selftest` and `--lint` 0 in all 20 pairs; English over Japanese
  played through the opening's seven timed boxes at 4:3 and 16:9, each
  held past its timer until the press (`opening_en-US_over_ja-JP.png`,
  same folder). D332 is what the captures showed of the re-break there.
- **Amended 2026-10-22 (group NR): pages are re-flowed, rows never.** The
  owner, off the capture of the opening where the re-break ran the name row
  into the speech and the row reached the frame (D332): "in the original
  script, the name gets its own row - just go ahead and do a page break
  instead of fitting it onto one page. That leads to more pages of text,
  but less re-flow of sentences." **What changes:** `SecondComposer.rebreak_page` /
  `rebreak` and class B are removed, not left switchable - one rule. Every
  authored row of the main is kept as it is; a page of three text rows is
  two pages on `0x02` (rows 1-2, then row 3), the second shared over them
  by the existing rule (page by page where counts match, else by glyphs)
  and re-flowed into its own reading rows as before (their budget
  unchanged). One routine call: when the share of a split puts an insert of
  the second over the part whose text rows do not carry it, the boundary
  moves to the nearest unit where each part's inserts are its own
  (`share_split`), so fewer messages are left plain for an insert.
  **What it costs:** pages added, area blocks, before -> after: 652 -> 4,395
  for English over Japanese (+3,743; French and German over Japanese
  +3,669 / +3,637), 5,018 -> 8,218 for Japanese over English, +2,315..3,522
  under the Latin mains; area slots paired 17,389 -> 17,460 for English over
  Japanese. Every pair's numbers in
  [`reading-row-second.md`](reading-row-second.md) 7 (rule 4 there amended).
  **Verification:** `check` now stops the build when a composed text row
  is not the authored row it stands for or is wider than it - 0 re-flowed
  rows in all 20 pairs (56,172..63,841 text rows each); `--lint` 0 faults
  and every pair exit 0; `second --selftest` 0 mismatches (ja-JP/en-US,
  en-US/ja-JP, en-US/fr-FR, fr-FR/de-DE); the furigana layer byte-identical
  before and after (239 files) and its `--selftest` 0. Played under
  `ruby-en-US-ja-JP` at 4:3, point filter, from NEW GAME: the opening's
  seven boxes are ten stops, the three name-plus-two-row boxes each two
  pages with the name on its own row and the arrow on the first
  (`opening_en-US_over_ja-JP_rows_kept.png` in
  `C:\Users\kerok\bof3x-runs\rr_nr\sheets\`).
- **Reversible?** Off by default (*Row above the text: None*); F4 and
  `BOF3X_ORIGINAL=MsgBox_Step` as DIV-0155.

### Mods: a player's zip built into an optional layer, chosen as the row above the text

- **ID:** DIV-0157
- **Date:** 2026-10-22
- **Subsystem:** launcher and setup - `tools/mods.py`, `tools/make_mod.py`,
  `tools/player_sources.py` (setup's mod step), `src/launcher/config.cpp`
  (`ruby=`, `ConfigRubyPlayable`, the layer added to `BOF3X_OPT`),
  `src/launcher/sources_dialog.cpp` (the Mods page), `config_dialog.cpp`
  (*Row above the text*), `.github/workflows/release.yml`
  ([`mods.md`](mods.md), [`launcher-settings.md`](launcher-settings.md) 2a)
- **Tier:** Sensible - which built layer is handed to the game, chosen
  before it starts; the mechanism changes nothing in play by itself (what a
  reading-row layer draws is DIV-0155 and DIV-0156).
- **Original behaviour:** the port has no mods, and its launcher no way to
  add one.
- **New behaviour:** a **mod** is a `.zip` of `mod.toml` at its root and the
  data files it names - never code, never game data (`.DAT` and the language
  layers' names refused), bounded (16 members, 4 MiB a member, 8 MiB in
  all), with a `kind` the tools know (today `reading-row`) and `requires` a
  list of language tags. The launcher's **Mods** tab adds one (`mods.py
  add`: checked, unpacked to `<home>\mods\<id>\`, listed in
  `<home>\mods.toml`; the same id again replaces it), removes one, and lists
  each, greyed with its reason when the install lacks what it requires.
  `setup` (and an add on the tab) runs each mod's build step: for
  `reading-row` with `base = "ja-JP"`, `tools/ruby_build.py furigana` with
  the mod's `readings.toml`, writing the optional layer `ruby-<id>-<base>`
  into the home's cache and its manifest; a missing builder, readings or
  output stops setup loudly. The Settings page's dropdown **Row above the
  text** lists None, each built reading-row mod whose base is the language
  played, and each other held language (DIV-0156's layers), every entry
  shown and greyed by source where it cannot be played; the choice is `ruby=`
  in `bof3x.ini` (the mod id or language tag, so it survives a language
  change), and the launcher appends `ruby-<ruby>-<language played>` to
  `BOF3X_OPT` when that layer is built, else plays without one and says so on
  stderr. The **furigana zip** (`make_mod.py furigana`, from
  `data/furigana/`) is attached to releases by `release.yml`.
- **Rationale:** the owner's call 2 of the round, 2026-10-22: furigana ships
  as a mod zip, "just so we can work on the first mod implementation in this
  round"; the readings can be re-issued without a release. IDEAS I21, I50
  and I33.
- **Also in the PSX version?** No.
- **Verification:** `mods.py selftest`, 28 zips each refused with its
  sentence or accepted, and the add / build / remove cycle in a temporary
  home; `make_mod.py --selftest`; `config_tests` (`TestRuby`);
  `dialog_test.py` 58 checks with the Mods tab and the box, captured
  (`C:\Users\kerok\bof3x-runs\r_ruby\ml\shots\`). The setup step's build was
  checked with a stand-in builder; the real builder run from an installed zip
  on a player's machine needs SudachiPy, which the release does not bundle
  (`make_release.py` `NOT_BUNDLED`; [`HANDOFF.md`](HANDOFF.md)). **Not yet
  played by the owner.**
- **Reversible?** Nothing changes without a mod and a choice: *Row above the
  text: None* (`ruby=` empty); Remove on the Mods tab takes the folder, the
  row and the layer.

### The faster text: Msg Speed's Med. at Fast's pace, Fast a page at once

- **ID:** DIV-0158
- **Date:** 2026-10-22
- **Subsystem:** the field and story message box - `MsgBox_StatePrint`
  (`0x497B30`, `src/game/msgbox.cpp`, its glyph tail); the launcher's
  `text_speed=` (`src/launcher/config.cpp`, `BOF3X_TEXT_SPEED`) and its box
  on Mods, Extras (`config_dialog.cpp`, `launcher.rc`). Issue #108.
- **Tier:** Sensible - the pace the text appears at; nothing in play changes.
- **Original behaviour:** each glyph waits a delay from the byte table at
  `0x658E94`, indexed by the Msg Speed option (Fast 0, Med. 1, Slow 2) plus
  one when the Speak button is not held; the box types its text out a glyph
  at a time at every speed. Only a script's own `0x10` / `0x11` span prints
  at once.
- **New behaviour:** with `text_speed=1` (off by default), Med. reads Fast's
  entries of the table (held and not), and on Fast a glyph sets no wait: the
  printer runs on in the same frame to the next code that stops it - the
  end (`0x00`), a page break (`0x02`), a choice (`0x14`), a timed wait
  (`0x16`) or a pause (`0x0B`) - so a page appears whole. Unlike flag `0x10`
  it never forces the loop past those, so every page, prompt and choice
  stops where the script put it. The delay byte is still set to Fast's, and
  `0x0B` adds its 8 frames to that, so a pause the script writes into a line
  lasts as long as it does on Fast (the owner's call, 2026-10-22: the pauses
  stay). Slow is unchanged. At most 1,024 glyphs print in a frame; past that
  the page goes on in the next frame, so a message with no stop code cannot
  hold the loop (`docs/msgbox.md` section 6's unclosed-span hang).
  `BOF3X_TEXT_SPEED` is 0 or 1; anything else stops the game at start, and 1
  with `MsgBox_StatePrint` not ours stops it too. Battle messages are not
  touched: they never type out (the battle ring draws each whole with
  `Text_DrawAt`, `BattleWin_DrawMessage` `0x597ED0`) and never read Msg
  Speed.
- **Rationale:** a request from the discussion page (issue #108): replaying
  players want the text as fast as they read. The original's Fast still
  types; holding Speak helps only a little.
- **Also in the PSX version?** The same three speeds and the same printer;
  no instant mode.
- **Verification:** builds; the exe host's self-test passes with the option
  off and on, the start log names it on, `BOF3X_TEXT_SPEED=2` stops with its
  message, and `text_speed=1` in the ini reaches the game. The hosted fuzz
  with it off passes: msgbox's 44,000 rounds against Capcom's copy, 0
  mismatches, and the whole `BOF3X_SHADOW='*'` run exits 0. Played by the owner
  2026-10-22: "This is brilliant".
- **Reversible?** Off by default; the Extras box or `text_speed=0`.

### A direct-colour page's texels are opaque but for 0: Amalgam's melt draws

- **ID:** DIV-0159
- **Date:** 2026-10-23
- **Subsystem:** the page textures - `Tex_Convert16` (`0x5A9D2F`, ours,
  `src/game/tex_page.cpp`; [`tex-page.md`](tex-page.md);
  [`known-defects.md`](known-defects.md) D333, issue #111)
- **Tier:** Intent - a restoration. A 15-bit texture page on the
  PlayStation draws every non-zero cell; the code that draws from one (the
  melt after DIV-0142's capture) plainly expects that.
- **Original behaviour:** `Tex_Convert16` packs each cell's red, green and
  blue and never adds `Gfx_PixelFormat`'s opaque bits (dword 7), as
  `Gfx_ConvertRow` does for palettes. The page format is A1R5G5B5, opaque
  bits `0x8000` (Capcom's set-up prefers it, and the port's backend makes
  the same choice), so every texel of a direct-colour page has alpha 0 and
  the colour-keyed draw discards it. Amalgam's melt draws nothing, and the
  boss vanishes on the frame of the capture; so do Mikba's death,
  Weretiger's form and effect kind 0x9D's pulse and glow (D333). Capcom's
  build, given a filled page, shows the same (D333).
- **New behaviour:** `Gfx_ConvertRow`'s rule in `Tex_Convert16`: a cell
  that packs non-zero takes the opaque bits, a non-zero cell that packs to
  0 becomes 1 plus the opaque bits, and a cell of 0 stays 0, transparent as
  on the PlayStation. Bit 15 of a cell is still not read (D333's drawers do
  not need it). Under a format with no opaque bits nothing changes. A
  texel inside a DIV-0125 grab is the grab's, as before.
- **Rationale:** a player's report (issue #111) after DIV-0142, and the PSX
  twin rule: the PlayStation draws these pages.
- **Also in the PSX version?** The PlayStation draws them; this restores
  that.
- **Verification** (2026-10-23, `ropeAndAmalgam.txt`, recipe frames
  7600..7780, shots every 3):
  - On: the melt plays - the streaks rise over the boss's skulls and fall
    away by frame 7714, as on the owner's PSX capture.
    `BOF3X_ORIGINAL=DirectOpaque`: the boss is gone from the capture's
    frame, as before.
  - `BOF3X_SHADOW=tex_page` (the fuzz against Capcom's copy, run before the
    switch is armed): `Tex_Convert16` 1,000 rounds, the builders 4,000
    each, no mismatch.
  - `ending17.txt` (the ending's frozen frame, a direct-colour drawer behind
    a DIV-0125 grab): all 162 shots identical on and off. With the grab
    off (`BOF3X_FBGRAB=0`) the frame now shows the font sheets that page
    holds, read as 15-bit, at its right edge where before it drew nothing;
    a debugging setting only.
  - Not seen: Mikba's death, Weretiger and kind 0x9D (no route reaches
    them). Kind 0x9D's page is not cleared first (D333), so it will draw
    what that page holds; the owner's look when a route gets there.
- **Reversible?** `BOF3X_ORIGINAL=DirectOpaque`, or
  `BOF3X_ORIGINAL=Tex_Convert16`.

### Semi-transparent primitives blend only the texels with bit 15 (the bit-15 round)

- **ID:** DIV-0160
- **Date:** 2026-10-23 (steps 2 and 3 of the round)
- **Subsystem:** the texel converters `Gfx_ConvertRow` (`0x59EBB0`, `src/game/gfx_clut.cpp`) and
  `Tex_Convert16` (`0x5A9D2F`, `src/game/tex_page.cpp`); the backend's texture upload
  (`src/render/render_d3d11.cpp` `Convert`, `PutStp`) and `ReadPixel` (`src/render/render_shim.cpp`);
  [`stp-blend.md`](stp-blend.md), [`stp-census.md`](stp-census.md), [`known-defects.md`](known-defects.md)
  D334
- **Tier:** Intent - a restoration. The PlayStation's GPU picks the blended texels of a semi-transparent
  primitive by bit 15 of their colour; the data (the rope's palette, the text palettes) was made for that.
- **Original behaviour:** every non-zero texel converts with the page format's opaque bits, whatever its bit
  15, and a semi-transparent primitive blends every texel it draws by its tpage's mode. The rooftop rope
  (area 28) draws at a quarter of its brightness; text on page 15 is added to what is under it (D334).
- **New behaviour:** step 2 - under our backend the textures carry bit 15 in the one
  A1R5G5B5 combination Capcom's converters never write - a colour with bit 15 set converts to that colour with
  alpha clear (forced to 1 if it would round to 0, as before); a colour with it clear keeps the alpha bit; 0
  stays 0, transparent. The backend reads alpha clear on a colour as a drawn texel, as before, and uploads a
  one-byte plane beside each such texture, 255 where bit 15 is set; the colour texture is byte for byte what it
  was. **Step 3, the draw:** a blended draw of such a texture runs a second pixel shader under dual-source
  blending (`PSStp`, `SRC1_ALPHA` / `SRC1_COLOR`): a bit-15 texel, or a transparent one, takes the draw's own
  D3DBLEND factors, as before; any other drawn texel is drawn opaque (its coverage and one less it, so a
  bilinear edge stays soft). Not a DIV-0125 grab draw, and only the factor pairs the shader can express (all
  four Capcom's `D3d_SetBlend` uses). Seen: the rope at its PlayStation brightness, and the text on page 15
  in its palette colour, no longer added to the window under it.
- **Rationale:** HANDOFF item 00, agreed with the owner 2026-10-23; the census (`stp-census.md`) sized it.
- **Also in the PSX version?** The PlayStation draws by bit 15; this restores that.
- **Verification** (2026-10-23, [`stp-blend.md`](stp-blend.md) 4): the `gfx_clut` self-test checks the rule
  against Capcom's copy (74,752 bit-15 cells, 0 mismatches) and `BOF3X_SHADOW='*'` passes; 73 shots over
  four routes are pixel-identical on and off; `BOF3X_STPVIEW` leaves the rope unmarked and marks the melt,
  the shadows and the windows' backings. The owner's playthrough the same day confirmed the rope and the
  text.
- **Reversible?** `BOF3X_ORIGINAL=StpBlend` draws as Capcom's port (the textures still carry the bit; the
  frames are then identical to step 2's); `BOF3X_ORIGINAL=StpTexels` turns both off. Armed after the start-up fuzzes, only with DIV-0159
  (`DirectOpaque`: without it a direct-colour texel has alpha clear on a colour, which this reads as drawn),
  and only under our backend (`InitShim` has run): under `BOF3X_ORIGINAL=Display_Setup`, Capcom's DirectX 6
  objects would read alpha clear as transparent, so the converters keep Capcom's rule there.
  `BOF3X_STPVIEW=1` (diagnostic) draws the texels the plane marks in magenta.

### Two battle draws take their coordinates signed: a box or cell partly off the left or top edge clips

- **ID:** DIV-0161
- **Date:** 2026-10-23
- **Subsystem:** the battle draws - `BattleWin_DrawCell16` (`0x444C40`,
  `src/game/battle_window_draw.cpp`) and `GeneWin_DrawCursorBox` (`0x599A00`,
  `src/game/battle_e7.cpp`); [`known-defects.md`](known-defects.md) D336
- **Tier:** Sensible - a draw repair; nothing in play changes.
- **Original behaviour:** both mask their coordinates with `0xFFFF` before
  `fild` (`0x599A6E..0x599A81`, the same shape in `0x444C40`), so a negative
  x or y becomes a float near 65,536 and the primitive lands off-screen. The
  cursor box masks its width and height the same way and adds them in 32
  bits. Ours kept that (both modules' fuzzes compare it with Capcom's copy).
- **New behaviour:** each vertex is the PlayStation's signed halfword: the
  cell's x and y sign-extended from 16 bits; the box's x, y, x + w - 1 and
  y + h - 1 each cut to 16 bits and sign-extended. For coordinates from 0 to
  32,767 nothing changes. Below 0 the cell or the box draws clipped at the
  screen's edge, where it vanished.
- **Rationale:** the PlayStation stores these as signed vertex halfwords
  (the cursor box: the JP disc's `sh` of x, y and y + h - 1 at
  `0x800AEEF4..0x800AEF08`, US `0x800B1F04`; the cell: `sh` into +8 / +0xA,
  US `0x801DA078`); the PC's mask is the port's conversion, not a choice.
  Reach: the gene list's draw `GeneWin_DrawList` (`0x5990E0`) puts the box
  at the window's x + 5 and runs on every frame of the list's slide-in and
  slide-out (`0x599020` / `0x599050`, x down to -0xA5, and further in the
  wide view under DIV-0041's slide), so if the box is drawn then (step 6,
  not sub-step 5) the PC drops it for the frames where x + 5 is below 0 and
  the PlayStation slides it out with the list. The cell's callers
  (`0x442FA0`, `0x443F60`) are not known to pass a negative coordinate. Not
  seen in play either way.
- **Also in the PSX version?** No: the PlayStation draws them clipped; this
  restores that.
- **Verification:** builds; `BOF3X_SHADOW='*'` self-tests exit 0 (the
  `battle_window_draw` and `battle_e7` fuzzes run before the switch is armed,
  so they still compare Capcom's unsigned reading; neither sees the signed
  path, which differs only for a coordinate of 0x8000 or more). Not seen in
  play.
- **Reversible?** `BOF3X_ORIGINAL=BattleDrawSigned` (both functions; one
  switch byte in each module).

### The dragon store prompt's hand reads its answer as the word every writer stores

- **ID:** DIV-0162
- **Date:** 2026-10-23
- **Subsystem:** battle - `DragonCmd_StoreAsk` (`0x451160`,
  `src/game/battle_e5.cpp`); [`known-defects.md`](known-defects.md) D338
- **Tier:** Sensible - a latent read-width repair; nothing a player sees
  changes today.
- **Original behaviour:** the hand's x is `0xD5 + 0x22 * answer` with the
  answer the dword at `0x904B74` (`0x4511F2`), where every writer stores a
  word (`0x451042`, `0x4511EA`, `0x431C70`). The level-up notice's
  `sprintf` reads the dword too (`0x431C77`) but masks it with `0xFFFF`
  (`0x431C7C`), so it already reads the word and is not touched.
- **New behaviour:** the hand reads the word. The two readings differ only
  when the halfword `0x904B76` is not 0; no instruction in the exe names
  `0x904B74..0x904B77` but the six above (a scan of `.text` for the
  addresses, 2026-10-23), so it is 0 in every run we know of, and nothing a
  player sees changes.
- **Rationale:** the PlayStation (US `0x800A632C`, JP `0x800A332C`) reads the
  answer as a `u16`; the PC's dword read is the port's. Cheap insurance
  against a stray write making the hand jump off its two choices.
- **Also in the PSX version?** No: the PlayStation reads the halfword.
- **Verification:** builds; `BOF3X_SHADOW='*'` self-tests exit 0 (the
  `battle_e5` fuzz runs before the switch is armed, so it still compares
  Capcom's dword read).
- **Reversible?** `BOF3X_ORIGINAL=StoreAskWord`.

### A battle's start clears the target-pick flag, as the US disc

- **ID:** DIV-0163
- **Date:** 2026-10-23
- **Subsystem:** battle - `Battle_Init` (`0x42E4A0`,
  `src/game/battle_phases.cpp`); the flag `BattleE4_Picking` (`0x904AAF`);
  [`known-defects.md`](known-defects.md) D349
- **Tier:** Sensible - a Capcom fix the PC port never received; nothing in
  play changes when the last battle ended normally.
- **Original behaviour:** `Battle_Init` resets the battle's globals
  (`0x904AA5..0x904B97`, one by one) but not `0x904AAF`, the flag the
  command steps set while a target is being chosen (`0x446FF0`, `0x447840`,
  `0x447F40`, `0x450510`, the item and skill picks) and clear on confirm or
  cancel. The JP disc's `Battle_Init` (`0x801D1228`) lacks the clear too.
- **New behaviour:** the flag is cleared with its neighbours, as the US
  disc's `Battle_Init` (`BATTLE.EMI#3 0x801D1228`) does (US `0x801462EF`, the
  JP's `0x801462EB`).
- **Rationale:** a battle that starts with the flag set shows a target
  highlight nobody is picking: the member or enemy whose actor byte the
  stale command target holds (or the whole side, bit 0x40 / 0x80) pulses
  with the cursor's tint (`BattleObj_IdleTintOn` `0x4417A0`, the enemies'
  idle highlight), and the enemy gauge windows step on as if chosen
  (`BattleWin_EnemyGaugeFrame` `0x5974C0`), until the first pick clears it.
  Capcom fixed it between the JP and the US builds. How a battle can end
  with the flag still 1 on the PC was not found by reading: every pick
  clears it on confirm and cancel, a battle cannot end during command entry,
  and a quickload (DIV-0145..0147) restores the byte with the rest of the
  snapshot. The US disc's re-entry is likely its soft reset, which the PC
  does not have. So the repair is expected to change nothing in normal play.
- **Also in the PSX version?** The JP disc: the same as the PC. The US disc:
  cleared, as here.
- **Verification:** builds; `BOF3X_SHADOW='*'` self-tests exit 0 (the
  `battle_phases` fuzz runs before the switch is armed, so it still compares
  Capcom's resets).
- **Reversible?** `BOF3X_ORIGINAL=BattleInitPickClear`.

# Divergence ledger

**Status:** IN PROGRESS (opened 2026-09-18; 2 entries, DIV-0001..0002)

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
  `0x5A7420` reporting a failed `fopen` as success, and `Save_ListFiles` not
  bounding its table at 16. Those are separate entries when they are touched.
- **Also in the PSX version?** Unknown. The listing and the per-slot files are
  porting-house code — the PlayStation reads the memory card's directory —
  so a port bug is the likelier reading, but nobody has checked whether the
  PSX menu refreshes after a write. Sibling-side question.
- **Reversible?** Yes: `BOF3X_ORIGINAL=Save_WriteFile` runs Capcom's function
  instead ([`SCAFFOLDING.md`](SCAFFOLDING.md) §2). No config toggle; there is
  no config system yet.

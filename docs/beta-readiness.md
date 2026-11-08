# Beta readiness - what stands between `main` and `v0.1.0-beta.1`

**Status:** DRAFT (2026-11-07 evening; written at the owner's word after the first two full clears, to be picked up on 2026-11-08)

The owner finished the game and validated the clear save on 2026-11-07; ScrubOasis did the same on a separate
build. The owner's judgment: the builds are stable enough to call the next release a beta. This document is the
checklist for that cut: what the two clears establish, what is still owed, what the beta's notes may claim, and
the work to do before the tag. It **points at evidence rather than restating it**; the release routine itself is
[`releases/README.md`](releases/README.md), unchanged.

The releases so far: `v0.1.0-alpha.5` is the last *published* release (2026-10-22). `v0.1.0-alpha.6` was tagged
at `b7bd1d29` on 2026-10-29 and is still a draft. The beta is therefore the first public release since alpha.5
and carries everything alpha.6's notes list plus what landed after it (the mountain passes, the exe-gap round,
issues 177..191, the 64-bit round, the FMV decoders, the ending scenes; [`STATUS.md`](STATUS.md) lists each with
its PR). `main` at the time of writing is PR #209's merge, `67c3259b`.

## 1. What the clears establish

Three installs have now been played through or deep into, by hand:

| Who | Install | Language | How far |
|---|---|---|---|
| the owner | PC port, the game folder in `game\` | the furigana dual-line layer (a row above the text, DIV-0155 / DIV-0156) | the ending and the clear save, 2026-11-07; earlier the 64-bit build for a stretch of play, its music, voices and the Capcom movie heard and seen right (owner-review, 2026-11-07) |
| ScrubOasis | PlayStation US disc + PSP Western (European) disc | English | the ending and the clear save, on their own build |
| a third player | PlayStation French disc alone | French | in progress |

What that retires from alpha.6's "watched, not yet played" list ([`releases/v0.1.0-alpha.6.md`](releases/v0.1.0-alpha.6.md)):

- **The sound changes** (the sound-layer round, DIV-0169..0175, the stop-cuts): heard through two full plays.
- **The crash and drawing fixes** of the alpha.6 list: played past every place they were found.
- **The row above the text**: the owner's whole play was under it.
- **The quicksaves, the button changes, the widescreen changes**: played through (the owner at 21:9, the width
  the ending reports came from).
- **The disc-only and PSP-only installs**: a PSX-US + PSP-EU mixed install played to the end; a PSX-FR disc-only
  install played in part. A **PSP-only** install and a **PSX-JP** install are still scripted runs only.
- **English and French**: played. **German and Japanese**: scripted, and the Japanese text seen by the owner
  only as the furigana row's source, not as the main language.

What the clears do **not** establish, and the notes must keep saying:

- **Answered by the owner, 2026-11-07 night:** the **enemy-information switch** (DIV-0187) and the **no-fail
  minigame cheats** (DIV-0188) were both played with and work well - "played" lines in the notes. The **Preset
  page** (PR #164) was opened and toggled, but the play was under **Recommended** throughout: the page itself is
  seen, the other three presets are still "checked headless", and the notes say so.
- **The 32-bit build's movies through our decoder** (DIV-0205): the owner saw the Capcom movie on the 64-bit
  build copied into `game\` (owner-review, 2026-11-07 evening); the 32-bit build goes through the same code and
  has not been started since. One start of the released 32-bit zip covers it (section 4, item 2).
- The **upgrade from alpha.5** over an existing folder (section 4, item 3).

## 2. Open defects and issues at the cut

- **Issue #190, the dragon gene window crash** (a write to a code address from the Area16 slide-out after
  Data / Best then backing out). **Left open on purpose** (the owner, 2026-11-07): the owner tried the
  reporter's steps at several places, in long and short plays, in bosses and in ordinary fights, and could not
  reproduce it; the earlier harness attempt is in `D:\bof3x-runs\i190` (window 21 as an enemy gauge). It stays
  open to gather data from other players. The beta notes name it under known issues, with the ask: the
  `CRASH n:` lines from `bof3x.log` and the install and language, not the `.dmp` (game data).
- **Issue #109, the randomizer**: a feature request, not a release item.
- **Open D-numbers a player can meet** ([`known-defects.md`](known-defects.md) status line): D328 the furigana
  readings unproofread; D350 and D352 the Scena16 caption wait and the window reset's five fields (held for
  timing); D343 the inn night's pause-and-resume against the PlayStation (open); D296 slide-ins seen in the
  21:9 band and D295 the band's patch cells (the owner: leave for now); D297 the four CPU readers of the grab.
  None is a crash; each is a known-issues line or nothing.
- **Harness-only**: D396 (the fuzz under a preset), D276, D329 / D330 (tooling). Not release items.
- **D407 fixed, D406 fixed, the owner's listen on D406 owed** (a quick-load during a battle voice): a
  known-issues line if not heard before the cut.

## 3. Branches not on `main`

- None that is a release item. DIV-0203 (Horis's desert directions, issue #184) looked unmerged on 2026-11-07
  because STATUS and owner-review still named its branch; it has been on `main` since PR #193 (2026-11-03, with
  issue #189's fade), and the branch is gone. The rule changes the direction word only (Westen -> Osten, and
  the hyphenated keep-going word made whole), so the German reads as the disc wrote it with the direction put
  right; the owner's eye on a rebuilt `de-DE` layer is still owed (owner-review, 2026-11-03) and is a
  "watched, not yet played" line, not a blocker.
- The source-layout round's stage B, the typed-data stage 3 and the platform-layers plan are not release items.

## 4. The work before the tag

In order. Items 2 and 3 are the owner's; 4..8 an agent can do and the owner reads.

1. ~~The owner's answer~~ - given 2026-11-07 night (section 1): the two cheats played, the Preset page seen,
   the other presets not played.
2. **One start of the 32-bit zip** from the `latest` build or the draft: the two movies with sound (DIV-0205 on
   the player's width), then the title. Five minutes.
3. **The upgrade path**: unzip the draft over a copy of a real alpha.5 folder (the owner's own, copied aside, or
   ScrubOasis's shape, a disc-built one is the stronger test). Expect: the launcher names the stale pieces and
   offers the rebuild; a disc-built image built by an older release is refused until rebuilt (DIV-0101's
   strict mode, `BOF3X_EXEIMAGE_STRICT=0` the override); the saves and quicksaves are kept and load; the
   Jukebox's names still show. A clean-folder install is already step 4 of the release routine.
4. **The release notes**, `docs/releases/v0.1.0-beta.1.md`, in alpha.6's shape, rewritten rather than appended:
   - the first paragraph says *beta*, and in one sentence what the word means here: two full clears by hand,
     every scripted route hashing against its reference, the crashes found in play since alpha.5 fixed, the
     one open crash report named;
   - "New since v0.1.0-alpha.5" carries alpha.6's list **and** what landed after it (the mountain passes
     DIV-0191, the item sort DIV-0189, the dock legend DIV-0190, the no-fail cheats DIV-0188, the exe-gap round's
     loss tune / blank slot / PAL banks / `ja-JP` kinds (DIV-0192..0196), issues 177..179 and 184..191, the
     movies decoded in-process DIV-0205, the sound through SDL3 DIV-0201 with `off=SoundSdl` for DirectSound,
     the ending scenes DIV-0127 / 0142 / 0206 / 0207, the 64-bit zip as a developer build). Each line from the
     ledger entry, not from memory;
   - "Watched, not yet played" is section 1's remainder, no more and no less;
   - "Known issues" is section 2 plus the carried lines (furigana does not build from the zip and is
     unproofread; the launcher smoothed at 125 % and 150 % display scaling; `.chd` / `.cso` and 2048-byte
     PlayStation `.iso` images unread; the first setup's minutes and ~1 GB);
   - a line on saves: the game's own save files and the quicksaves carry forward through every beta, and a
     quicksave refuses the other width's take (the x64 round's stage C measurement).
   The workflow refuses to run without the file (`release.yml`, "Release notes exist").
5. **The README's status line** (line 68 of [`../README.md`](../README.md), "There is an alpha build"): say
   beta, and point at the releases page; the two sentences after it ("the goal is to sunset it eventually")
   are the owner's to keep or drop.
6. **A bug-report issue template** (`.github/ISSUE_TEMPLATE/bug-report.md`; the repo has a PR template only):
   the version as the launcher shows it, the sources on the Sources page and the language, the preset, what was
   done and what was seen, the `CRASH n:` block from `bof3x.log` if any, and in bold: **do not attach the
   `bof3x.crash-*.dmp` files** (they hold the game's data, [`crash-reporter.md`](crash-reporter.md) 3). In
   ASD-STE100, as the launcher's text is (CLAUDE.md). A second template for a feature request is optional.
7. **The owner-review sweep** ([`owner-review.md`](owner-review.md), 96 KB): strike what the two clears cover
   (a full play under the furigana row at 21:9 with the sound on reaches most of the "to play" and "to listen
   to" lists from 2026-10-06 onward), each strike naming the clear as the evidence; leave what a clear does not
   reach (a German disc, a PSP-only install, the Preset page unless answered yes in item 1, D406's quick-load
   listen, DIV-0203's German on a rebuilt `de-DE` layer). The result is what makes section 1's remainder honest. An agent proposes the
   strikes as a sheet; the owner confirms before the file changes.
8. **`STATUS.md` and `HANDOFF.md`** rewritten for the cut (the rule: rewrite, not append): STATUS's head says the
   beta is cut and what it holds, HANDOFF's item 0 is "the beta's first reports".

Then the routine: commit the notes on `main` (signed off), tag `v0.1.0-beta.1`, read the draft, publish.
`v0.1.0-alpha.6`'s draft can be deleted or published first; a published alpha.6 keeps the history linear, a
deleted one keeps the releases page short. The owner's call; nothing in the workflow depends on it.

## 5. What a beta should promise and not promise

Written so the notes and the README say the same thing:

- **Promised:** a game that plays to the end from any catalogued source, saves that survive every upgrade, a
  report path for what breaks (the template, the `CRASH` lines), fixes recorded in the ledger with a switch to
  turn each off (`off=` in `bof3x.ini`, the Preset page).
- **Not promised:** German and Japanese as main languages at the level of English and French; a PSP-only install
  at the level of the mixed and PSX-only ones; the furigana readings' accuracy; the 64-bit build as anything
  but a developer build; the launcher's picture at a scaled desktop.
- **What would make it 1.0** (for [`IDEAS.md`](IDEAS.md) or PLAN, not for the notes): each catalogued source and
  language played through once by hand; the owner-review list empty or every line parked by a decision; no
  open crash report older than a release; the furigana readings proofread or the feature marked experimental
  in the launcher.

## 6. Traps already paid for

- A release note never claims more than STATUS and the README's status line do
  ([`releases/README.md`](releases/README.md) step 1).
- The `latest` pre-release is never GitHub's "latest release"; a published `v*` keeps that mark.
- Defender has quarantined unsigned builds three times; the release zips are signed, a locally built launcher
  is not ([`releases/README.md`](releases/README.md) "Signing"). Test the upgrade with the draft's zip, not a
  local build.
- A dump is game-derived and never goes in an issue (CLAUDE.md rule 1); the template says so.
- Every commit of the cut is signed off (rule 7); CI checks it, and a history rewrite is the owner's call.

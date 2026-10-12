# The PSP-only round: a working install from the player's PSP disc alone

**Status:** STAGED (2026-10-12). Nothing launched, nothing measured here. This
plan regroups [`psp-only-build.md`](psp-only-build.md)'s sections 3, 5 and 8
into groups that can run in parallel, with every live check held for one batch
at the end.

**It runs after the PSX disc-only round** (the owner, 2026-10-12), and is
written as if that round has landed. The base is `main` after that round's
merge, and the round branch is `psp/round`, rebased onto it at launch.

The owner asked, 2026-10-12, for the work a PSP-only install requires,
staged as a round in the way the capture rounds were. That means parallel
groups, merges one at a time, and the validations batched at the end. The
owner's calls of 2026-10-10 govern
([`psp-only-build.md`](psp-only-build.md) 7):
- the PSP's art, data and audio are the base;
- there are three suite toggles (audio, graphics, data) in the launcher;
- every option is listed, and greyed when its disc is missing;
- the effects are decoded by the player's own ffmpeg.

## 0. The answer in one paragraph

The disc-only round makes a PSX disc alone a playable install: our
executable, `.data` from the cache's `base/exe/`, no `BOF3.exe`, no `DAT\`,
`SND\`, `BGM\` or AVIs, and a language layer without the PC's bytes. This
round is then the PSP's difference on that path.

Eight groups can start at once, and none needs another's output to start.
Four are importer work on separate modules:
- **SI**, the stand-ins;
- **EX**, a PSP `base/exe/` that meets the disc-only round's image contract;
- **MB**, the music banks and the dropped tone;
- **MS**, the sequences.

Then there is **LG**, the PSP's language layers on the disc-only round's
language path, and **CD**, the PSP code diff, in two stages. **PN** covers
the PSP's start on the disc-only path: its jingles, logos and effects.
**LT** is the launcher's three suites.

Three things wait for wave one's merges:
- MS's stage B, which writes `base/bgm/`;
- CD's stage B, which runs two readers;
- **TG**, the suites' engine half.

The end batch is a true PSP-only start: no `BOF3.exe`, no PC file. Its
reference is the PSX disc-only install of the same language (section 5).

## 1. What the disc-only round is assumed to have delivered

The coordinator checks each line against that round's record before launch.
A line that does not hold becomes a group here.

| Assumed landed | Used here by |
|---|---|
| `bof3x.exe` starts with `.data` mapped from the cache's `base/exe/`, with the unplaced pointer words and code pointers held as layout, the ~22 KB no disc carries as the engine's own, and the operand store's bytes as values | EX, PN, the end batch |
| The gate: the state hash of a disc-built image against the PC-built one, with `data.toml`'s ranges as the skip list | EX, the end batch |
| A start with no `BOF3.exe`, `DAT\`, `SND\`, `BGM\` or AVIs: every file the engine opens comes from the cache, or is a named Fatal | PN |
| The PSX's logo handled (the `STR`, or skipped) and the jingles from a PSX disc | PN (the PSP's form of each) |
| The disc-only language layer complete (`loc-build-disc-only.md` 7: the `zh-CN` chunks no layer replaced, the slot counts, the `NameError`s) | LG |
| **The 14 port-edited arenas (`pc-edit`) settled**, because a PSX-only install cannot play without them | SI (the PSP's arenas take the same rule) |
| The default key table as a rule | nothing PSP-specific |

## 2. What is left for the PSP

| # | Needed | From | Group |
|---|---|---|---|
| 1 | `base/` complete from a PSP disc: the ~270 stand-ins (2.2), the `icons` and dial rules reading the PSP's section, and the `pc-edit` rule applied to the PSP's arenas | 5 step 2, call 1 | **SI** |
| 2 | Ryu's form data (`RYUD00..03`, the 14 `type1` arenas, P8) as the PSP's own | 1, 2.2 | **SI** (data); **CD** (the code that reads it) |
| 3 | The 16 containers whose bank lost its second tone | 2.1 | **MB** |
| 4 | The PSP's music: `pPMS` to the sequence form, and the 81 music banks to the form the sequencer plays | 2.1, 5 step 5 | **MS**, **MB** |
| 5 | A PSP `base/exe/` that passes the disc-only round's image contract. Its data pointers rebuild through a map of the ELF's own copies (59 % today; the rest held as layout, by that round's mechanism). `sin_table` is built by rule, and the PSP's `exp_table` and consumable 87 are the base | 2.3, `exe-import-engine.md` 5.7 | **EX** |
| 6 | The language layers from a PSP disc: `ja-JP` from PSP-JP, `en-150` from PSP-EU. The names come from `BOOT.BIN`, not `GAME.EMI` | 2.4 | **LG** |
| 7 | The PSP's start on the disc-only path. The 11 jingles are at3's MP3s. The logo is the PSP's `PMF`, or skipped. The effects are the ATRAC3plus decodes, now heard in play (their ledger entry is owed). `WHISTLE` is not imported | 2.1.1, 2.5, 8 | **PN** |
| 8 | The launcher's three suites, greyed by source, inverted for a PSP-only install | 6, call 3 | **LT**, then **TG** |
| 9 | Whether the PSP's data runs on our code, and the code toggles if any | 4, I32 | **CD** |
| 10 | ~~The PSP's banks (885 / 901), effects, voice and jingles~~ | 8 | done 2026-10-10 |

Not required, and left out: a clean-room ATRAC3plus decoder (I36, low
priority) and the "deeper choice" screen.

## 3. The groups

| Group | Wave | What | Writes (new files are the group's own) | Touches shared | DIV drafted |
|---|---|---|---|---|---|
| **SI** | 1 | The PSP-only base policy; the `own` stand-ins from a PSP section for the ~270 chunks; the `icons` and dial rules on the PSP's sections; P8's data as stand-ins; the disc-only round's `pc-edit` rule run on the PSP's arenas | `tools/psp_standins.py`, `docs/psp-stand-ins.md` | `tools/importer.py` (one call site), `recipes/pc-zh.toml` (regenerated; **SI alone**) | A: the PSP-only base |
| **EX** | 1 | The ELF's own-copies map and the pointer rebuild on PSP; what still does not rebuild, held by the disc-only round's mechanism; `sin_table` by rule, proved byte-identical on the PC's and every PSX build; the PSP tables as base. Then a PSP `base/exe/` that maps and starts headless | `tools/psp_exe.py`, `docs/psp-exe.md` | `tools/importer.py` (one call site), `recipes/exe*` (**EX alone**); the engine's image loader only if the PSP needs a case it lacks | B: the PSP's level table and consumable 87 |
| **MB** | 1 | `PPHD` music banks to VAB (the volume/pan merge inverted as far as it goes: a proposed rule); the dropped tone of the 16 containers by rule, or refused with the reason | `tools/vag.py` (**MB alone**), `docs/psp-music-banks.md` | none | (in C) |
| **MS** | 1 + 2 | Stage A: `pPMS` to the sequence form, measured (73 exact, the classes). Stage B, after MB merges: the importer writes `base/bgm/` from a PSP source when no PSX disc is given; `verify` on it | `tools/ppms.py`, `docs/psp-music.md` | `tools/importer.py` (stage B, one call site) | C: the PSP's music as base |
| **LG** | 1 | The disc-only round's language build run against PSP-JP and PSP-EU: the names from `BOOT.BIN`, every container, diffed against the `psx-jp` and `psx-eu-en` layers | `docs/loc-build-psp.md` | `tools/loc_build.py` (**LG alone**) | amends DIV-0090 |
| **CD** | 1 + 2 | Stage A: pair the sibling's named PSX functions with their PSP twins (`psp-stallion.md` 3.1's method), and diff the pairs with widescreen and platform set aside. Stage B: two readers (CD1 battle and effects; CD2 field, menus, items, system). Priority questions: the consumers of `RYUD`, the button-label page, `exp_table` and consumable 87 | `tools/psp_pair.py`, `docs/psp-code-diff.md` | none | none (a list) |
| **PN** | 1 | The PSP's start on the disc-only path: what a PSP cache holds against what that start reads, file by file; the jingles from at3's MP3s; the logo (`PMF` or skipped); the effects' ledger entry; a headless self-test from a PSP-only cache with no PC file | `src/` (the seams the disc-only round left), `docs/psp-start.md` | `src/game/fmv_play.cpp`, the cache seams | D: the PSP's effects and logo in play (amends DIV-0089) |
| **LT** | 1 | The launcher's options page: a catalogue (`recipes/opt.toml` plus a suites table), availability from the cache's manifest, the three suites greyed with the discs that would enable them, the PSP-only inverse. Replaces the "PSP extras" boxes. A suite whose engine half is not built yet is greyed with that reason, never selectable | `src/launcher/`, `recipes/suites.toml`, `docs/launcher-suites.md` | `config.cpp`, `config_dialog.cpp` (**LT alone**) | E: three suites replace the extras (amends DIV-0086) |
| **TG** | 2 | The suites' engine half: `opt/` layers for `base/bgm/` and `base/snd/`, and a chunk kind for the exe tables, so that audio and data work with a PSX disc present; LT's greyed reasons lifted | `src/`, `docs/launcher-suites.md` section | the cache seams, the music seam | amends E |

Wave one has eight agents. Wave two is MS stage B (the same agent, by
SendMessage), CD stage B (CD continues as CD1, and one new agent is CD2), and
TG (new).

**Why this split.** The shared files are the ones that collided in earlier
rounds: `tools/importer.py`, the recipes, `DIVERGENCE.md`, `README.md` and the
launcher's two dialog files. Each one has a single owner or a single call
site. `vag.py` is MB's alone, so MS waits for MB through a stated contract
(section 4), not a shared edit. EX and PN may both want the engine's image or
cache code. EX touches it only for a PSP-specific case; PN touches only the
file seams. If both need the same function, the coordinator decides at the
first report. Agents draft their ledger entries in their own doc. The
coordinator inserts them and numbers them (section 5).

## 4. Contracts between groups

- **MB to MS:** `vag.psp_music_vab(pphd: bytes, pbvc: bytes) -> (vh: bytes,
  vb: bytes)`, in the form `seq.py` / `base/bgm` already pairs with a song.
  MS stage A does not need it. MS stage B starts from the round tip after MB
  merges.
- **SI to LT, and EX to LT:** the manifest records the base policy (`psp` or
  `psx`) and, per chunk class, the build that supplied it. The `[build]` keys
  are `base_policy` (SI) and `exe_tables_from` (EX).
- **CD to SI, and CD to EX:** if CD finds that our code cannot run on the
  PSP's `RYUD`, arena or table layout, the matching stand-in becomes a
  refusal. CD reports this as soon as it is known. The coordinator relays it.
- **EX and SI to PN:** PN's PSP-only self-test needs a PSP cache with
  `base/exe/`. Until EX merges, PN tests with the PSP's `base/` over the
  PSX-built `base/exe/`, and says so.

## 5. Merge order and the coordinator's jobs

Order: **CD-A** (a tool and a doc), then **EX, MB, SI, LG, MS-A** (importer),
then **PN** (engine), then **LT** (launcher). Wave two follows: **MS-B, CD-B,
TG**.

After each merge, in the verify worktree (`analysis/psp_round/merge_psp.sh`):
- `python tools/importer.py check`.
- The PC and PSX paths unchanged: JP + PC, and the US disc alone, identical to
  the reference caches built once from the base. Hash the file lists.
- `python tools/ledger_check.py`.
- If `src/` changed: the i686 build, and the headless self-test `'*'` narrow
  (wide at the end).
- If `src/launcher/` changed: the launcher host suite and `dialog_test`.

These are all offline or headless.

The coordinator's own jobs:
- DIV numbers. A to E are drafted as letters. At the first merge that needs
  one, take the next free id after the disc-only round's.
- One `docs/README.md` row per new doc.
- `psp-only-build.md`'s section 5 struck through as steps land.
- Known-defect numbers: agents describe defects, the coordinator numbers them.

## 6. The validations batched at the end

Run once, after the last merge, from the round tip's build in a clean folder.
The folder holds `bof3x.exe`, the DLL and the launcher only: no `BOF3.exe`, no
`bof3x.ini`, and no PC file of any kind. `--launcher` takes an absolute path.
Pass `BOF3X_LAYERING=0` to `input_run.py`.

**V1, offline and integrated** (any time):
1. `importer.py build --source <PSP-JP iso> --lang ja-JP`, and the same for
   PSP-EU with `en-150`. The target is all 741 containers, less the 16 banks
   if MB's rule did not land. Also `verify`, the manifest's policy keys,
   `base/bgm/`'s 166, `base/snd/`'s 891, and `base/exe/`.
2. Both PSP caches byte-identical in every shared file.
3. The PC and PSX paths unchanged (section 5's hash, once more on the tip).
4. The music, offline. The 166 PSP-built songs rendered by the sequencer's
   offline render (the music measurement's tooling, `music-open-ends.md`)
   against the PSX-built. The 73 exact songs must render identically. The
   others are reported by class (2.1).
5. `ledger_check`, and `'*'` narrow and wide.

**V2, the image gate** (headless): the state hash of the PSP-built image
against the PC-built one. The skip list is the disc-only round's, plus the
PSP's ledgered tables (`exp_table`, consumable 87). Every other difference is
named.

**V3, live with the owner away.** The reference is the **PSX disc-only
install of the same language**: PSP-EU `en-150` against the European PSX
disc's disc-only cache. Each side gets `BOF3X_OPT=none`.
1. **The attract sequence**, twice per side. The state hash differs only on
   the pages the stand-ins and the PSP's tables drive. List and name each.
2. **The ten routes** under `en-150`, on both sides. A PSP run must finish
   without a Fatal. Where it parts from the PSX run, name the tick and the
   cause. A level-up that reads roster index 6's table is the expected one.
   The PSX disc-only side is checked against itself first (two runs), since
   `en-150` routes are untried.
3. **Captures for the owner's eye**, through `tools/recipe_shots.py` (never
   hand-added shots): the stand-in areas a route reaches, the battle menu (the
   button-label page), and the title, PSP against PSX side by side.
4. **The launcher**, by script: the options page on a PSP-only cache, a
   PSX-only cache and a PC + PSX + PSP cache. Each suite's state and greyed
   reason. The jukebox's PSP column now lit.

**V4, the owner's**, gathered into [`owner-review.md`](owner-review.md) as one
entry:
- Eye: V3.3's sheet, and the options page on their display.
- Ear: the 16 note-changed songs, the 2 tempo songs and song 31; a fight
  with the dropped-tone bank; effects and a jingle from ATRAC3plus; the
  logo.
- A play from the PSP-only install.

## 7. For the owner (none blocks the launch)

1. **The PSP's logo** (PN): play the `PMF` (a decoder question, like
   ATRAC3plus), skip it, or do whatever the disc-only round chose for the
   PSX's `STR`. Recommended: match the disc-only round.
2. **The music banks' volume/pan merge** (MB): the PSP folded each tone's
   volume and pan into its program's and squeezed pan about 3:1. MB proposes
   an inverse; the owner hears it in V4.
3. **The suites' membership** (LT proposes):
   - Which suite carries the PSP's name layers: data, or the language choice?
   - Is the PSP's button-label page in graphics, or left out?
   - Does P8, Ryu's forms, go under data or graphics?
4. **The 16 banks** (MB): a rule if one exists, otherwise a PSP-only install
   ships without that chord's second tone, as the PSP itself does.

## 8. The scratch, for whoever launches

The briefs are in `analysis/psp_round/` in the main checkout, which is
gitignored and survives sessions:
- `brief_common.md`, shared by every agent;
- `groups.md`, one section per group;
- `merge_psp.sh`, section 5's merge and checks.

Before launch:
- Check section 1 against the disc-only round's record.
- Rebase `psp/round` onto the `main` that holds it.
- Set `<TIP>` and `<DISC_ONLY_DOC>` (that round's doc) in the briefs.
- Build the reference caches `ref_jppc`, `ref_us` and `ref_eu` with the
  base's tools.

Branches are `psp/<group lowercase>`, worktrees under the session's
scratchpad, and each agent resets onto `<TIP>` first.

## 9. Not verified

- Section 1 is an assumption about a round not yet run.
- The sizes are `psp-only-build.md`'s estimates. MS + MB is 4 to 6 days by
  that estimate, which is more than one usage window. Rate-limit cuts are
  resumable by SendMessage.
- Whether CD's pairing method scales from the boss module to 2.9 MB of
  `.text`.
- Whether the ten routes replay under `en-150`. V3.2 checks the PSX side
  against itself first.
- Whether the disc-only round's held-as-layout mechanism fits the PSP's
  pointer remainder, which is larger than any PSX build's. This is EX's first
  measurement.

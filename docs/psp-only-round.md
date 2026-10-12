# The PSP-only round: a working install from the player's PSP disc alone

**Status:** STAGED (2026-10-12). Nothing launched, nothing measured here. This
plan regroups [`psp-only-build.md`](psp-only-build.md)'s sections 3, 5 and 8
into groups that can run in parallel, with every live check held for one batch
at the end. Base `main` `54911be6` (PRs #52 and #53 merged); round branch
`psp/round`.

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

Nine groups can start now, and none of them needs another group's output to
start. Four are importer work on separate modules:
- **SI**, the stand-ins;
- **EX**, the exe from `BOOT.BIN`;
- **MB**, the music banks and the dropped tone;
- **MS**, the sequences.

Two are text and reading work:
- **LG**, the PSP's language layers;
- **AR**, the port's 14 arena edits.

One is a reading pass in two stages, **CD**, the PSP code diff. One is engine
work, **NI**, the engine with no install. One is the launcher, **LT**, the
three suites.

Three things wait for wave one's merges:
- MS's stage B, which writes `base/bgm/` once MB's banks exist;
- CD's stage B, which runs two readers over the pairing that stage A builds;
- **TG**, the suites' engine half.

Every check that needs the game runs once at the end, with the owner away
(section 5). **One thing is outside this round and gates the last check.** A
start with no `BOF3.exe` at all needs the disc-only round (HANDOFF item 0): the
data pointers held as layout, the 22 KB no disc carries, and the operand
store as values. Until that lands, the batch runs the PSP's data over the
player's `BOF3.exe` as the `.data` source. That checks everything but the
exe image itself.

## 1. What "required" means here

A PSP-only player has a PSP disc and nothing else. The install must build,
start and play from that disc, and the launcher must show the right suites.
The list is from [`psp-only-build.md`](psp-only-build.md), with what has
landed since 2026-10-10 struck out:

| # | Needed | From | Group |
|---|---|---|---|
| 1 | `base/` complete from a PSP disc: the ~270 stand-ins (2.2), and the `icons` and dial rules reading the PSP's section | 5 step 2, call 1 | **SI** |
| 2 | Ryu's form data (`RYUD00..03`, the 14 `type1` arenas, P8) as the PSP's own | 1, 2.2 | **SI** (data); **CD** (the code that reads it) |
| 3 | The 16 containers whose bank lost its second tone | 2.1 | **MB** |
| 4 | The PSP's music: `pPMS` to the sequence form, and the 81 music banks to the form the sequencer plays | 2.1, 5 step 5 | **MS**, **MB** |
| 5 | `base/exe/` from `BOOT.BIN`: the data pointers rebuilt through a map of the ELF's own copies (59 % today), `sin_table` by rule, and the PSP's `exp_table` and consumable 87 as base | 2.3, `exe-import-engine.md` 5.7 | **EX** |
| 6 | The language layers from a PSP disc: `ja-JP` from PSP-JP, `en-150` from PSP-EU, with names from `BOOT.BIN` | 2.4 | **LG** |
| 7 | The 14 port-edited arenas (`pc-edit`). Every disc-only build needs them, so they are not PSP-specific; they are here because no round holds them | 3.2 | **AR** |
| 8 | The engine with no install: no `DAT\`, `SND\`, `BGM\` or AVIs. The jingles' MP3s and the PSP's effects from `base/snd/`, the logos, and every other file it opens | 2.5, 8, `cache-read.md` 5 | **NI** |
| 9 | The launcher's three suites, greyed by source, inverted for a PSP-only install | 6, call 3 | **LT**, then **TG** |
| 10 | Whether the PSP's data runs on our code, and the code toggles if any | 4, I32 | **CD** |
| 11 | ~~The PSP's banks (885 / 901), effects, voice and jingles~~ | 8 | done 2026-10-10 |
| 12 | Our own executable, and `.data` from the cache | 3.1 | ~~state 3~~ done; **the disc-only round** for the rest |

Not required, and left out: a clean-room ATRAC3plus decoder (I36, low
priority); `OTHER/WHISTLE`; the PSP's logo `PMF` (NI asks the owner, section
6); the "deeper choice" screen.

## 2. The groups

| Group | Wave | What | Writes (new files are the group's own) | Touches shared | DIV drafted |
|---|---|---|---|---|---|
| **SI** | 1 | The PSP-only base policy; the `own` stand-ins from a PSP section for the ~270 chunks; the `icons` and dial rules on the PSP's sections; P8's data as stand-ins | `tools/psp_standins.py`, `docs/psp-stand-ins.md` | `tools/importer.py` (one call site), `recipes/pc-zh.toml` (regenerated; **SI alone**) | A: the PSP-only base |
| **EX** | 1 | The ELF's own-copies map and the pointer rebuild on PSP; `sin_table` by rule, proved byte-identical on the PC's and every PSX build; the PSP tables recorded as base in the manifest | `tools/psp_exe.py`, `docs/psp-exe.md` | `tools/importer.py` (one call site), `recipes/exe*.tsv` / `exe.toml` (**EX alone**) | B: the PSP's level table and consumable 87 |
| **MB** | 1 | `PPHD` music banks to VAB (the volume/pan merge inverted as far as it goes: a proposed rule); the dropped tone of the 16 containers by rule, or refused with the reason | `tools/vag.py` (**MB alone**), `docs/psp-music-banks.md` | none | (in C) |
| **MS** | 1 + 2 | Stage A: `pPMS` to the sequence form, measured (73 exact, the classes). Stage B, after MB merges: the importer writes `base/bgm/` from a PSP source when no PSX disc is given; `verify` on it | `tools/ppms.py`, `docs/psp-music.md` | `tools/importer.py` (stage B, one call site) | C: the PSP's music as base |
| **LG** | 1 | `loc_build all` against PSP-JP and PSP-EU: the names from `BOOT.BIN`, the font from the disc's atlas (DIV-0090's route), every container; diffed against the `psx-jp` and `psx-eu-en` layers | `docs/loc-build-psp.md` | `tools/loc_build.py` (**LG alone**) | amends DIV-0090 |
| **AR** | 1 | Read the 14 arena edits: what each fixes, and whether our code depends on the edited layout. Propose a rule, a recipe, or "the disc's decode, ledgered". No output change | `docs/pc-edit-arenas.md`, a reading script | none | none (a proposal) |
| **CD** | 1 + 2 | Stage A: pair the sibling's named PSX functions with their PSP twins (`psp-stallion.md` 3.1's method), and diff the pairs with widescreen and platform set aside. Stage B: two readers over the halves (CD1 battle and effects; CD2 field, menus, items, system). Priority questions: the consumers of `RYUD`, the button-label page, `exp_table` and consumable 87 | `tools/psp_pair.py`, `docs/psp-code-diff.md` | none | none (a list) |
| **NI** | 1 | Every file the engine opens, enumerated; the engine started from the cache alone (a temp game folder with no `DAT\`, `SND\`, `BGM\` or AVIs); the 11 jingles' MP3s from `base/snd/`; the logos when no AVI exists; a missing file a Fatal that names the importer, not a crash; a self-test of the no-install path | `src/` (the file seams), `docs/no-install-start.md` | `src/game/fmv_play.cpp`, `dat_cache`, `Sound_LoadStream` | D: the logos with no AVI; amends DIV-0089 (the PSP's effects heard) |
| **LT** | 1 | The launcher's options page: a catalogue (`recipes/opt.toml` plus a suites table), availability from the cache's manifest, the three suites greyed with the discs that would enable them, the PSP-only inverse. Replaces the "PSP extras" boxes. A suite whose engine half is not built yet is greyed with that reason, never selectable | `src/launcher/`, `recipes/suites.toml`, `docs/launcher-suites.md` | `config.cpp`, `config_dialog.cpp` (**LT alone**) | E: three suites replace the extras (amends DIV-0086) |
| **TG** | 2 | The suites' engine half: `opt/` layers for `base/bgm/` and `base/snd/`, and a chunk kind for the exe tables, so that audio and data work with a PSX disc present; LT's greyed reasons lifted | `src/`, `docs/launcher-suites.md` section | `dat_cache`, the music seam | amends E |

Wave one has nine agents. Wave two is MS stage B (the same agent, by
SendMessage), CD stage B (CD continues as CD1, and one new agent is CD2), and
TG (new). That is two new agents.

**Why this split.** The shared files are the ones that collided in earlier
rounds: `tools/importer.py`, the recipes, `DIVERGENCE.md`, `README.md` and the
launcher's two dialog files. Each one has a single owner or a single call
site. `vag.py` is MB's alone, so MS waits for MB through a stated contract
(section 3), not a shared edit. Agents draft their ledger entries in their own
doc's "For DIVERGENCE.md" section. The coordinator inserts them and numbers
them (section 4).

## 3. Contracts between groups

- **MB to MS:** `vag.psp_music_vab(pphd: bytes, pbvc: bytes) -> (vh: bytes,
  vb: bytes)`, in the form `seq.py` / `base/bgm` already pairs with a song.
  MS stage A does not need it: it measures sequences alone. MS stage B starts
  from the round tip after MB merges.
- **SI to LT, and EX to LT:** the manifest records the base policy (`psp` or
  `psx`) and, per chunk class, the build that supplied it. LT reads the
  policy, not the chunk rows. SI and EX each add one key to the manifest's
  `[build]` table; the key names are fixed here as `base_policy` (SI) and
  `exe_tables_from` (EX).
- **CD to SI:** if CD finds that our code cannot run on the PSP's `RYUD` or
  arena layout, SI's P8 stand-ins become a refusal. CD reports this as soon as
  it is known, before the doc is finished. The coordinator relays it.
- **AR to everyone:** AR changes no output. Its proposal goes to the owner.

## 4. Merge order and the coordinator's jobs

Order: **AR, CD-A** (docs and a tool), then **EX, MB, SI, LG, MS-A**
(importer), then **NI** (engine), then **LT** (launcher). Wave two follows:
**MS-B, CD-B, TG**.

After each merge, in the verify worktree:
- `python tools/importer.py check`.
- The PC path unchanged: JP + PC 742 of 742 byte-identical, and the US-only
  cache identical to the base's except the build time. Hash the manifests.
- `python tools/ledger_check.py`.
- If `src/` changed: the i686 build, and the headless self-test `'*'` narrow
  (wide at the end).
- If `src/launcher/` changed: the launcher host suite and `dialog_test`.

These are all offline or headless. None needs the owner's machine.

The coordinator's own jobs:
- DIV numbers. A to E are drafted as letters. At the first merge that needs
  one, take the next free id. On 2026-10-12, DIV-0095 is the last, and PR #54
  only amends DIV-0092.
- One `docs/README.md` row per new doc.
- `psp-only-build.md`'s section 5 struck through as steps land.
- Known-defect numbers: agents describe defects, the coordinator numbers them.

## 5. The validations batched at the end

Run once, after the last merge, from the round tip's build in its own folder.
`--launcher` takes an absolute path. Pass `BOF3X_LAYERING=0` to `input_run.py`.
Use a clean launcher folder with no `bof3x.ini`.

**V1, offline and integrated** (any time):
1. `importer.py build --source <PSP-JP iso> --lang ja-JP`, and the same for
   PSP-EU with `en-150`. Containers written, against the target: 741 less the
   14 `pc-edit`, unless AR's rule landed, and less the 16 banks, unless MB's
   rule landed. Also `verify`, the manifest's policy keys, and `base/bgm/`'s
   166.
2. Both PSP caches byte-identical in every shared file (a cache-wide hash
   list).
3. The PC and PSX paths unchanged (section 4's hash, once more on the tip).
4. The music, offline. The 166 PSP-built songs rendered by the sequencer's
   offline render (the music measurement's tooling, `music-open-ends.md`)
   against the PSX-built. The 73 exact songs must render identically. The
   others are reported by class (2.1).
5. `ledger_check`, and `'*'` narrow and wide.

**V2, headless with the owner away.** Use `BOF3X_CACHE=<PSP-EU cache>`,
`BOF3X_OPT=none`, and `BOF3.exe` as the `.data` source:
1. **The attract sequence**, run twice. Against the PC-built cache's run, the
   state hash differs only on the pages the stand-ins and the PSP's tables
   drive. List the pages and name each.
2. **The ten routes** under `en-150`. The bar is not identity: a route must
   finish without a Fatal. Where it parts from the `en_<route>_sh3a` pair,
   name the tick and the cause. A level-up that reads roster index 6's table
   is the expected one.
3. **No install:** the attract from a folder holding only the launcher, the
   DLL, `bof3x.exe` and `BOF3.exe`. No `DAT\`, `SND\`, `BGM\` or AVIs.
4. **Captures for the owner's eye**, through `tools/recipe_shots.py` (never
   hand-added shots): the stand-in areas a route reaches, the battle menu (the
   button-label page), and the title.
5. **The launcher**, by script: the options page on a PSP-only cache, a
   PSX-only cache and a PC + PSX + PSP cache. Each suite's state and greyed
   reason. The jukebox's PSP column now lit.

**V3, the owner's**, gathered into [`owner-review.md`](owner-review.md) as one
entry:
- Eye: V2.4's sheet, and the options page on their display.
- Ear: the 16 note-changed songs, the 2 tempo songs and song 31 under the PSP
  build; a fight with the dropped-tone bank; a few effects and a jingle from
  ATRAC3plus.
- A play under the PSP-EU cache.

**V4, after the disc-only round lands** (not this round's gate): a start with
no `BOF3.exe`. Then the state hash of a PSP-built image against the PC-built
one, with `data.toml`'s ranges plus the ledgered PSP tables as the skip list.

## 6. For the owner (none blocks the launch)

1. **The disc-only round beside this one?** It is HANDOFF item 0 and V4's
   gate. Its groups (the pointers as layout, the 22 KB, the operand store, the
   key table) touch `src/` and `tools/exe_*`. EX touches `tools/exe_*` too.
   Running both together means about 14 agents and two importer owners.
   **Recommended: this round's wave one first, the disc-only round as its own
   round after EX merges.**
2. **The logos with no AVI** (NI): skip them (recommended), or play the PSP's
   `PMF` (a decoder question, like ATRAC3plus).
3. **The music banks' volume/pan merge** (MB): the PSP folded each tone's
   volume and pan into its program's and squeezed pan about 3:1. MB proposes
   an inverse; the owner hears it in V3.
4. **The suites' membership** (LT proposes):
   - Which suite carries the PSP's name layers: data, or the language choice?
   - Is the PSP's button-label page in graphics, or left out? (It shows the
     PSP's buttons.)
   - Does P8, Ryu's forms, go under data or graphics?
5. **The 14 arenas** (AR): which outcome.
6. **The 16 banks** (MB): a rule if one exists, otherwise a PSP-only install
   ships without that chord's second tone, as the PSP itself does.

## 7. The scratch, for whoever launches

The briefs are in `analysis/psp_round/` in the main checkout, which is
gitignored and survives sessions:
- `brief_common.md`, shared by every agent;
- `groups.md`, one section per group;
- `merge_psp.sh`, section 4's merge and checks for one group branch.

Each brief wants `<TIP>` set to the round tip at launch. Branches are
`psp/<group lowercase>`, worktrees under the session's scratchpad, and each
agent resets onto `<TIP>` first. The round's verify worktree is the
coordinator's.

## 8. Not verified

- The sizes are `psp-only-build.md`'s estimates, not measured. MS + MB is 4
  to 6 days by that estimate, which is more than one usage window.
  Rate-limit cuts are resumable by SendMessage.
- Whether CD's pairing method scales from the boss module to 2.9 MB of
  `.text`.
- Whether the ten routes survive a language other than the one they were
  recorded under. The `en-US` pairs say yes for `en-US`; `en-150` is untried.
- Whether NI's no-install folder needs anything else of the PC's: a save
  folder, `bof3x.ini`, `DIR1`. That is NI's first measurement.

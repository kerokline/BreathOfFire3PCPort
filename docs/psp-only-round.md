# The PSP-only round: a working install from the player's PSP disc alone

**Status:** IN PROGRESS (2026-10-12): wave one launched from `platform/disc-only-round` `98c17e8b`, after its code review's six gating items (the owner: "go ahead and start against a latest pull from the branch"). This plan regroups
[`psp-only-build.md`](psp-only-build.md)'s sections 3, 5 and 8 into groups
that can run in parallel, with every live check held for one batch at the
end.

**It runs after the PSX disc-only round** (the owner, 2026-10-12) and builds on
that round's decisions. They are read from `platform/disc-only-round` at
`98c17e8b`: all eight merges in, and its review's gating items fixed
([`disc-only-round.md`](disc-only-round.md),
[`disc-only-install.md`](disc-only-install.md)). The round branch `psp/round`
sits on that tip and is rebased onto `main` once the disc-only round merges.

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

The disc-only round made a US disc alone a playable install: `importer.py
build`, then `install --base`, then `bof3x.exe` with `.data` from the cache.
It also did several PSP things on the way. CP's layout table gives a
PSP-built image **0 unfilled pointer bytes**. NB's engine regions, the sine
table among them, fill every disc image. `exe-gaps.tsv` already classes both
PSP builds.

What is left is the PSP's difference on that path. Eight groups can start
at once:
- **SI:** the stand-ins, and the arenas from a PSP source;
- **IM:** the PSP image's gaps and tables, and whether `BOOT.BIN` carries the
  game values that no PSX disc does;
- **MB:** the music banks and the dropped tone;
- **MS:** the sequences;
- **LG:** the language layers;
- **CD:** the code diff, in two stages;
- **PN:** the PSP's install and start, meaning the logo movie, the jingles and
  `install --base`;
- **LT:** the launcher's three suites.

Wave two is MS stage B, CD stage B, and **TG**, the suites' engine half. The
end batch is a PSP-only start with no PC file. Its reference is the PSX
disc-only install of the same language, built by that round's steps.

## 1. What the disc-only round decided, and what it means here

| The disc-only round (merge, doc) | For the PSP |
|---|---|
| **Pointers by layout** (CP, `recipes/exe-layout.tsv`, `exe-import-engine.md` 7): every pointer word from a table of addresses, applied after the rebuild | Done for the PSP too: PSP-JP 59,936 to **0** unfilled pointer bytes, 609,917 of 638,976 bytes identical to the PC's (7.5). The ELF own-copies map (`exe-import-engine.md` 5.7) is no longer needed for pointers. It may still place game data (IM) |
| **The engine's own bytes** (NB, `src/hook/exe_own.*`, [`exe-engine-own.md`](exe-engine-own.md)): 17 regions, `Math_SinTable` computed by rule | `sin_table`'s absence from the PSP is moot: the engine computes it. The PSP's `own` count is 16,895 against 14,899 on PSX; IM reads the difference |
| **Gaps** (IN, `recipes/exe-gaps.tsv`): what no disc carries, classed and zero on a disc-built image; `Magic_Rows`' effect file numbers the most visible. Accepting them, deriving them by rule, or requiring the PC's exe for them is **the owner's open call** | PSP-JP 499 gap ranges against 435 on PSX-JP. A PSP-only install inherits the owner's answer. The PSP's native `BOOT.BIN` holds its own copies of tables, so it may carry values a PSX disc does not: IM's main question |
| **The arenas** (AR, [`arena-edits.md`](arena-edits.md), DIV-0096): `27A` and `START` by rule; the 8 `PL` and 3 `BPLD` the disc's arena as is. "`OWN_FROM` is the PSX discs'": **a PSP-only build still lacks the 14**. The PSP ships them decompressed. (Corrected by SI: `BPLD012/015/016` do **not** carry the PSP's change - the 15 P8 arenas are other stems, so the 14 stand in byte for byte) | SI extends `OWN_FROM` and the two rules to a PSP source |
| **Language** (LG, [`loc-build-disc-only.md`](loc-build-disc-only.md), DIV-0097 / 0098): the re-laid sheets as a rule (`RELAID`), the stand-ins, CONFIG from the disc's own letters, the 8 language bytes that stay zero | LG runs the same build against `BOOT.BIN`'s names. The PSP's sheets may not match `RELAID`'s coordinates; LG measures that |
| **The logo** (FM, [`fmv-import.md`](fmv-import.md), DIV-0099): `LOGO/CAPCOM30.STR` converted by the player's ffmpeg to `base/fmv/CAPCOM.AVI`, Cinepak 640x480 30 fps. No ffmpeg means a logged skip | PN converts the PSP's `PSMF/CAPLOGO.PMF` by the same route, if the player's ffmpeg reads PSMF (unmeasured) |
| **The install** (IN, [`disc-only-install.md`](disc-only-install.md), DIV-0100): `install --base` lays out `DAT\`, `SND\` and an empty `BGM\`; a missing movie is skipped; the disc probe passes with a disc image mapped. **The 11 jingles are not there** on a PSX-only install (an owner's call) | A PSP-only install **has** the jingles: at3 writes them as MP3s. PN takes them into `install --base`. `BGM\` stays empty, since the sequences come from `base/bgm` (MS) |
| The key table as a rule; `.rdata` from the engine; `BOF3.exe` optional (RD) | Nothing PSP-specific |

Ledger numbers: the disc-only round took DIV-0096..0101 (0101, the gaps, provisional). This round's
drafts, A to E, take the next free ids at merge.

## 2. What is left for the PSP

| # | Needed | Group |
|---|---|---|
| 1 | `base/` complete from a PSP disc: the ~270 stand-ins (`psp-only-build.md` 2.2), the `icons` and dial rules on the PSP's section, the 14 arenas from a PSP source, and P8's Ryu form data as the PSP's own | **SI**; **CD** reads the code that consumes P8 |
| 2 | The PSP image measured: its extra gaps and `own` bytes over PSX-JP's; each `placed_differ` byte named (expected: `exp_table`, consumable 87); the PSP's tables as base; **the PSX gaps that `BOOT.BIN` fills** | **IM** |
| 3 | The 16 containers whose bank lost its second tone | **MB** |
| 4 | The PSP's music: `pPMS` to the sequence form, and the 81 music banks to the form the sequencer plays | **MS**, **MB** |
| 5 | The language layers from a PSP disc: `ja-JP` from PSP-JP, `en-150` from PSP-EU, names from `BOOT.BIN` | **LG** |
| 6 | The PSP's install and start: the `PMF` logo, the 11 jingles into `install --base`, the PSP's `base/snd` (891), and the effects' ledger entry now that they are heard | **PN** |
| 7 | The launcher's three suites, greyed by source and inverted for a PSP-only install | **LT**, then **TG** |
| 8 | Whether the PSP's data runs on our code, and the code toggles if any | **CD** |
| ~~9~~ | ~~The PSP's banks (885 / 901), effects, voice and jingles~~ | done 2026-10-10 |
| ~~10~~ | ~~Pointers, `sin_table`, the exe host with no `BOF3.exe`~~ | the disc-only round |

## 3. The groups

| Group | Wave | What | Writes (new files are the group's own) | Touches shared | DIV drafted |
|---|---|---|---|---|---|
| **SI** | 1 | The PSP-only base policy; the `own` stand-ins from a PSP section for the ~270 chunks; the `icons` and dial rules on the PSP's sections; the 14 arenas (`OWN_FROM` and AR's `hold` / `start-arena` rules on a PSP source, with the PSP's decompressed form); P8's data | `tools/psp_standins.py`, `docs/psp-stand-ins.md` | `tools/importer.py` (one call site), `tools/type1.py` (the PSP form only), `recipes/pc-zh.toml` (regenerated; **SI alone**) | A: the PSP-only base |
| **IM** | 1 | The PSP-built image against PSX-JP's, by `exe_tables` / `exe_own.py` class: the extra 64 gap ranges, the extra `own` bytes, every `placed_differ` named. Then a search of `BOOT.BIN` for the game-value gaps (`Magic_Rows`' effect files, the dragon set files, `Equip_SlotIcons`, `EventBattle_Records`, the ending's steps) by structure, not by copied values. Where found, a place or transform from the ELF, so that a PSP disc fills them, alone or beside a PSX disc. The manifest's `exe_tables_from` | `tools/psp_exe.py`, `docs/psp-image.md` | `tools/importer.py` (one call site), `recipes/exe*` (**IM alone**) | B: the PSP's level table and consumable 87 as base |
| **MB** | 1 | `PPHD` music banks to VAB (the volume/pan merge inverted as far as it goes: a proposed rule); the dropped tone of the 16 containers by rule, or refused with the reason | `tools/vag.py` (**MB alone**), `docs/psp-music-banks.md` | none | (in C) |
| **MS** | 1 + 2 | Stage A: `pPMS` to the sequence form, measured (73 exact, the classes). Stage B, after MB merges: the importer writes `base/bgm/` from a PSP source when no PSX disc is given; `verify` on it | `tools/ppms.py`, `docs/psp-music.md` | `tools/importer.py` (stage B, one call site) | C: the PSP's music as base |
| **LG** | 1 | The disc-only language build run against PSP-JP and PSP-EU: names from `BOOT.BIN`; the `RELAID` rule and the stand-ins on the PSP's sheets; every container; diffed against the `psx-jp` and `psx-eu-en` disc-only layers | `docs/loc-build-psp.md` | `tools/loc_build.py` (**LG alone**) | amends DIV-0090 / 0097 |
| **CD** | 1 + 2 | Stage A: pair the sibling's named PSX functions with their PSP twins (`psp-stallion.md` 3.1), and diff the pairs with widescreen and platform set aside. Stage B: two readers (CD1 battle and effects; CD2 field, menus, items, system). Priority: the consumers of `RYUD`, the button-label page, `exp_table`, consumable 87, and the tables IM finds in the ELF | `tools/psp_pair.py`, `docs/psp-code-diff.md` | none | none (a list) |
| **PN** | 1 | `CAPLOGO.PMF` to `base/fmv/CAPCOM.AVI` by FM's route, if ffmpeg reads it (else the logged skip). The PSP's `base/snd` and its 11 MP3 jingles into `install --base`. The effects' ledger entry. A headless self-test from a PSP-only install (`install --base`, no PC file) | `tools/` (FM's module, the PSP case), `docs/psp-install.md` | `tools/importer.py` (`install`, one call site), `tools/fmv*` (the PSP case only) | D: the PSP's effects, jingles and logo in a PSP-only install |
| **LT** | 1 | The launcher's options page: a catalogue (`recipes/opt.toml` plus a suites table), availability from the cache's manifest, the three suites greyed with the discs that would enable them, the PSP-only inverse. Replaces the "PSP extras" boxes. A suite whose engine half is not built yet is greyed with that reason, never selectable | `src/launcher/`, `recipes/suites.toml`, `docs/launcher-suites.md` | `config.cpp`, `config_dialog.cpp` (**LT alone**; RD's launcher changes are its base) | E: three suites replace the extras (amends DIV-0086) |
| **TG** | 2 | The suites' engine half: `opt/` layers for `base/bgm/` and `base/snd/`, and a chunk kind for the exe tables, so that audio and data work with a PSX disc present; LT's greyed reasons lifted | `src/`, `docs/launcher-suites.md` section | the cache seams, `exe_image.cpp` | amends E |

Wave one has eight agents. Wave two is MS stage B (the same agent, by
SendMessage), CD stage B (CD continues as CD1, and one new agent is CD2), and
TG (new).

**Why this split.** Each shared file has one owner, or takes one call site:
`tools/importer.py`, the recipes, `vag.py`, `loc_build.py`, and the launcher's
dialog files. `DIVERGENCE.md` and `README.md` are the coordinator's. `vag.py`
is MB's alone, so MS waits for MB through a stated contract (section 4). IM
and PN both reach the importer's exe and install paths, by different call
sites. Neither changes `src/hook/exe_image.cpp` in wave one; TG may.

## 4. Contracts between groups

- **MB to MS:** `vag.psp_music_vab(pphd: bytes, pbvc: bytes) -> (vh: bytes,
  vb: bytes)`, in the form `seq.py` / `base/bgm` already pairs with a song.
  MS stage B starts from the round tip after MB merges.
- **SI to LT, and IM to LT:** the manifest's `[build]` keys `base_policy`
  (`psp` or `psx`; SI) and `exe_tables_from` (IM). LT reads these keys, not the
  chunk rows.
- **CD to SI, and CD to IM:** if our code cannot run on the PSP's `RYUD`,
  arena or table layout, the matching stand-in or place becomes a refusal. CD
  reports it as soon as it knows. The coordinator relays it.
- **IM to the owner:** a game-value gap that the PSP fills changes the open
  disc-only call on gaps (section 7 item 1). IM reports the ranges and counts
  and changes no PSX-only output.
- **SI, MS and IM to PN:** PN's self-test wants a complete PSP cache. Until
  those merge, PN uses what exists (`base/` 519 containers plus the disc-only
  round's paths) and says so.

## 5. Merge order and the coordinator's jobs

Order: **CD-A**, then **IM, MB, SI, LG, MS-A** (importer), then **PN**, then
**LT**. Wave two follows: **MS-B, CD-B, TG**.

After each merge, in the verify worktree (`analysis/psp_round/merge_psp.sh`):
- `python tools/importer.py check`.
- The PC and PSX paths unchanged: JP + PC, the US disc alone, and the European
  disc alone, identical to the reference caches built once from the base.
  Hash the file lists, and compare `recipes/exe-gaps.tsv`'s PSX rows.
- `python tools/ledger_check.py`.
- If `src/` changed: the i686 build; the hosted `'*'` narrow; the exe host's
  self-test on the scratch PSX disc-only install (`disc-only-install.md` 5).
- If `src/launcher/` changed: the launcher host suite and `dialog_test`.

The coordinator's own jobs:
- DIV numbers after the disc-only round's last.
- One `docs/README.md` row per new doc.
- `psp-only-build.md`'s section 5 struck through as steps land.
- Known-defect numbers: agents describe defects, the coordinator numbers them.

## 6. The validations batched at the end

These run once, after the last merge, from the round tip's build. They follow
the disc-only round's method (`disc-only-install.md` 1 and 5): `build`, then
`install --base`, then the launcher with `cache=` and `language=`, no
`BOF3.exe` and no PC file. Keep paths short, since MCI fails on long paths.
`--launcher` takes an absolute path. Pass `BOF3X_LAYERING=0` to
`input_run.py`.

**V1, offline and integrated** (any time):
1. `importer.py build --source <PSP-JP iso> --lang ja-JP`, and the same for
   PSP-EU with `en-150`. Check:
   - `base/` holds 741 containers, less the 16 banks if MB's rule did not
     land;
   - `base/bgm` holds 166 songs, `base/snd` 891 files, and `base/fmv` the logo
     or a recorded skip;
   - `base/exe` passes `verify`, and its gaps are listed against PSX-JP's;
   - the manifest's policy keys.
2. PSP-JP and PSP-EU caches compared, every difference named (SI found they are not identical: PSP-EU's own language pages in 15 containers, and IM 124 bytes of `base/exe`).
3. The PC and PSX paths unchanged (section 5's hash, once more on the tip).
4. The music, offline. The 166 PSP-built songs rendered by the sequencer's
   offline render (the music measurement's tooling, `music-open-ends.md`)
   against the PSX-built. The 73 exact songs must render identically. The
   others are reported by class.
5. `ledger_check`, and `'*'` narrow and wide.

**V2, headless:** the exe host's self-test from the PSP-EU install, and from
the PSP-JP one: exit 0, the gaps logged, `exe_own` filled.

**V3, live with the owner away.** The reference is **the PSX disc-only install
of the same language**: PSP-EU `en-150` against the European PSX disc's
install. Each side uses `BOF3X_OPT=none`, the same `BOF3.CFG` copied in, and
`exe_tables.py gate-skip`'s list.
1. **The attract sequence**, twice per side, then `statehash.py diff`. The
   pages that differ must be the ones the stand-ins and the PSP's tables
   drive, and each is named. This is IN's attract-pair method; that round's
   pair is not yet read to the byte, so read it first.
2. **The ten routes** under `en-150`, on both sides. A PSP run must finish
   without a Fatal. Where it parts from the PSX run, name the tick and the
   cause. A level-up that reads roster index 6's table is the expected one.
   The PSX side is checked against itself first, since `en-150` routes are
   untried.
3. **Captures for the owner's eye**, through `tools/recipe_shots.py` (never
   hand-added shots): the stand-in areas a route reaches, the battle menu (the
   button-label page), the title and the logo, PSP against PSX side by side.
4. **The launcher**, by script: the options page on a PSP-only cache, a
   PSX-only cache and a PC + PSX + PSP cache. Each suite's state and greyed
   reason. The jukebox's PSP column now lit.

**V4, the owner's**, gathered into [`owner-review.md`](owner-review.md) as one
entry:
- Eye: V3.3's sheet, and the options page on their display.
- Ear: the 16 note-changed songs, the 2 tempo songs and song 31; a fight
  with the dropped-tone bank; effects and the jingles from ATRAC3plus.
- A play from the PSP-only install, including a battle with magic. That
  checks the `Magic_Rows` gap under whatever the owner decided.

## 7. For the owner (none blocks the launch)

1. **The game-value gaps** (the disc-only round's open call). A PSP-only
   install has the same kind of gaps. If IM finds them in `BOOT.BIN`, a PSP
   disc could fill them for a PSP-only install, and possibly for a PSX-only
   install that also has a PSP disc. Wait for IM's numbers before answering?
2. **The music banks' volume/pan merge** (MB): the PSP folded each tone's
   volume and pan into its program's and squeezed pan about 3:1. MB proposes
   an inverse; the owner hears it in V4.
3. **The suites' membership** (LT proposes):
   - Which suite carries the PSP's name layers: data, or the language choice?
   - Is the PSP's button-label page in graphics, or left out?
   - Does P8, Ryu's forms, go under data or graphics?
4. **The 16 banks** (MB): a rule if one exists, otherwise a PSP-only install
   ships without that chord's second tone, as the PSP itself does.
5. **The jingles**: a PSP-only install has them (as at3's MP3s) and a
   PSX-only install does not. That asymmetry stands unless the disc-only
   call on the PSX's jingles changes it.

## 8. The scratch, for whoever launches

The briefs are in `analysis/psp_round/` in the main checkout, which is
gitignored and survives sessions:
- `brief_common.md`, shared by every agent;
- `groups.md`, one section per group;
- `merge_psp.sh`, section 5's merge and checks.

Before launch:
- Rebase `psp/round` onto `main` once the disc-only round has merged, or
  launch from `platform/disc-only-round`'s tip if the owner says not to wait
  for its validation.
- Set `<TIP>` in the briefs.
- Build the reference caches `ref_jppc`, `ref_us` and `ref_eu` with the
  base's tools.

Branches are `psp/<group lowercase>`, worktrees under the session's
scratchpad, and each agent resets onto `<TIP>` first.

## 9. Not verified

- Section 1 reads `platform/disc-only-round` at `98c17e8b`. A later finding there can move this plan.
- The sizes are `psp-only-build.md`'s estimates. MS + MB is 4 to 6 days by
  that estimate, which is more than one usage window. Rate-limit cuts are
  resumable by SendMessage.
- Whether the player's ffmpeg reads `PMF` (PSMF: H.264 with ATRAC3plus).
- Whether `BOOT.BIN` carries any of the game-value gaps. Its native code may
  hold the same tables in a different shape, or none.
- Whether CD's pairing method scales from the boss module to 2.9 MB of
  `.text`.
- Whether the ten routes replay under `en-150`.

# Romhacks as mod packages: what one real hack changes, and what it would take

**Status:** IN PROGRESS 2026-11-10 (issue 216). Measured: one hack applied to
a scratch copy of the owner's US disc and read edit by edit. Built: the patch
reader (`tools/vcdiff.py`), the census (`tools/disc_patch_census.py`) and the
author on the Mods tab (section 5). Measured 2026-11-11: the disc-patch
round's five questions (section 8). Not built: any way for the hack to reach
the game. The design it lands on is IDEAS I58 (EMI patch mods); this note says
what I58 gets right, what it did not know, and the order of work.

Scratch root: `D:\bof3x-runs\romhack216` (the patch, the patched track, the
two caches, the TSVs). Nothing from it is in the repository (rule 1).

## 1. The hack

*Breath of Fire 3 Definitive Edition 1.0*, by **TheGreatBen**
(romhacking.net hack 10312). The author gave permission to repackage it for
this build on the condition that he is credited (the owner, issue 216,
2026-11-09, by Discord; the owner holds the message). Its readme lists the
changes: faster levels, BoF1 spell damage, breath tiers, stat-downs on bosses,
encounter-free maps, Ally anywhere, masters from the camp menu, three swords
that hit twice, faster faerie village, Giant Growth for Garr, a new skill
(Smoke) for Ryu, drop rates, a shop's stock, difficulty after the volcano,
Examine always learns.

| | |
|---|---|
| Archive | `Breath of Fire 3 Definitive Edition 1.0.xdelta.7z`, 989,134 bytes: the patch and a readme |
| Patch | 1,352,669 bytes, sha256 `d321b7af...ef9b346`; VCDIFF (RFC 3284) from xdelta3, secondary compressor 1 (DJW), 53 windows of 8 MiB, an Adler-32 per window |
| Source it names | `Breath of Fire III (USA) (Track 1).bin`, 442,938,048 bytes: the first 442,938,048 bytes of the held single-file `psx-us` image (fixtures.toml) |
| Result | 442,938,048 bytes, sha256 `830f07f8...d1a8ca3d`; all 53 window checksums pass |

**The xdelta format is read in pure Python**, by `tools/vcdiff.py` (`info`,
`apply`, `selftest`): the VCDIFF window and default code table from the RFC,
and xdelta3's DJW Huffman stage written from a reading of `xdelta3-djw.h`
(Apache-2.0; `THIRD_PARTY.md` 4). It applies this patch in about 6 seconds to
the sha256 above, and refuses another disc at the first window ("the source is
not the file the patch was made against"). I58 left the patch format "to
choose"; a hack as its author released it is an xdelta of the disc track, so
that is the format to take. xdelta3 can also use LZMA or FGK as the secondary
stage; those, a custom code table and VCD_TARGET windows are refused by name.
The self-test makes its own patches (an encoder for both stages is in the
tool), checks every refusal, and changes random bytes of a good patch 200
times: applied or refused, never a crash. CI runs it.

## 2. What it changes (patched disc against `psx-us`)

The disc was rebuilt, not patched in place: 885 of 887 files sit at another
LBA, and 43 EMIs are one sector shorter. So the boot EXE's and `LOGO.EXE`'s
changes are almost all one 957-byte file-position table each; the boot EXE's
own edits are about 25 bytes in three places. `SYSTEM.CNF` changes too.

215 files differ. Their changed sections, de-duplicated (many EMIs carry the
same section) and aligned so that an insertion is not counted as a change to
everything after it:

| Load address | What it is | Distinct sections (copies) | Edit runs | Bytes replaced | In the PC's `DAT/`? |
|---|---|---|---|---|---|
| `0x80010000` | per-area script and text block | 66 (79) | 291 | about 25,900; 66 of the 79 copies resized | yes (kept) - through the language layer |
| `0x8001A000` | shared text pool of BATTLE / BOSS / AFLDKWA / FIRST | 2 (44) | 157 | about 3,600, resized | yes (kept) - language layer |
| `0x800E4000` | per-area enemy and formation table | 49 (51) | 212 | about 780 | yes (kept), `widen` transform |
| `0x0E001000` kind 3, `0x00000002` kind 7 | two images (areas 63, 115), one kind-7 section (area 154) | 3 | - | 1,413 / 295 / 14,041 raw | the images yes; the kind-7 section too, as the area's sound bank (section 8, M4: the census says `dropped`, which is wrong for a bank) |
| `0x80096800` | BATTLE section 15, the battle overlay's data and code (42 copies, all the same) | 1 (42) | 335 | about 1,450, **8 bytes longer**, a 12-byte block moved | no (dropped) |
| `0x80195800` | `GAME.EMI` section 0: the content tables | 1 | 129 | about 5,900, 872 deleted and 468 inserted inside it, same size | no (no PC container) |
| `0x801D0C00` | menu and battle overlays (BATTLE 3, SHOP, SHISU, STATUS, SISYOU, COMMU, START, areas 30 / 89 / 129) | 13 (57) | 279 | about 2,500, some resized | no (dropped) |
| `0x801EEC00` | BATL_END, MAGIC modules, the faerie areas' section 14 | 11 (24) | 53 | about 470 | no |
| `0x801F2C00` | area overlays (33 areas, 1 to 3 bytes each) | 33 | 38 | 89 | no |
| `0x801F6C00` | SCENA modules (7) | 7 | 9 | 17 | no |
| `0x800F5000`, `0x800C1800` | areas 30 / 89 / 129 section 5; two BOSS sections | 3 (5) | 37 | 244 | no |

About 40 KiB of real edits in about 1,500 runs over about 190 distinct
sections. Roughly three quarters of the bytes are in sections the PC kept
(text, script, enemy tables); the rest is in the overlays the port compiled
into `BOF3.exe`.

### 2.1 The census: every edit, named

`python tools/disc_patch_census.py <US disc> <patched track> --out census.tsv
--twins <the US function map>` writes one row an edit run: 1,551 rows. For the
sections the PC does not hold as DAT chunks it says whether a run is code or
data and names it - from the function table of a decompilation of the US disc
where one covers the section (`region_twin_map.py`'s `us_jp.tsv` and
`pc_map.tsv`), else from `exe_maps/psx-us.tsv`, else a guess marked `?`.

| Runs | What |
|---|---|
| 662 | in kept sections: 291 script and text, 157 shared text pool, 212 enemy tables, 2 images |
| 456 | **moved**: an address that changed only because the bytes it points at moved. The author added 8 bytes to one battle function (`0x800B1A74`) and moved a 12-byte block, and his tool relinked every jump and pointer past them; areas 30 / 89 / 129's menu overlay likewise. Not edits. A build step must do the same relinking in reverse (section 4 item 2), never reimplement them |
| 240 | **data** in the overlays (205 placed or bounded, 35 guessed): the EXP and level tables and the shop records in `GAME.EMI`; in BATTLE 15 the damage-variance and effect-handler tables and the ability and item effect indexes; `Char_DefaultRecords` in START; a few bytes of per-area tables in 30 areas |
| 189 | **code** (120 in named functions, 69 guessed) in **84 functions**: 56 change 16 bytes or fewer (a constant, a branch), 15 change 17 to 64, 13 change more |
| 4 | the boot EXE and `LOGO.EXE` file-position tables, `SYSTEM.CNF` |

The 13 large code edits are where the hack's new features are: `START.EMI`
`0x801E33E8` (735 bytes, also in STATUS: the camp menu), five functions of
`GAME.EMI` around `0x801AE2B0`..`0x801AEA80` (the menu box and icon drawing,
moved and rewritten) and `0x8019C244`, `0x801B57F8`, `0x801BB81C`; the battle
window `0x801D8090` (`BattleWin_DrawSmallBox` on the PC); `BATL_END`
`0x801EFE5C` (the battle result, about 570 bytes replaced by 300); two
functions of `COMMU05` (the faerie village); the item-trade list of areas 30 /
89 / 129. Of the 55 named functions 25 have a PC twin recorded in
`pc_map.tsv`; the small edits include `Field_EncounterDue`,
`BattleEnemy_PickAction`, `Game_ClockTick` and four `DragonForm_*` functions.

The data names are the nearest `symbols.toml` symbol at or before the address,
so a name with a large offset is a table after the one named, not that table.

## 3. What the importer does with the patched disc today

`importer.py build --source <patched> --lang en-US`, with the patched path
forced to identify as `psx-us` (the per-file manifest does not match, so it is
"not a build fixtures.toml catalogues" otherwise):

1. **Refused at the first changed chunk**: `AREA003.DAT chunk 8 from psx-us
   (widen): hash differs from the recipe`. Every chunk is gated on the recipe's
   hash. Correct for a disc; a mod needs a declared way through.
2. **With that gate waived for chunks that differ from the original disc's**
   (each counted): the base layer builds. 52 chunks waived: 51 enemy tables
   and area 63's image. Sound and music build unchanged.
3. **`base/exe` stops**: `psp_exe: psx-us: BATTLE/BATTLE.EMI 0x800B65F8 row 0
   is no code-pointer row`. BATTLE 15 grew by 8 bytes, so everything after the
   insertion is 8 bytes from where `exe_maps` and the pointer rebuild expect
   it. The language layer was not reached.

So the existing route carries the enemy tables and the images as they are. It
does not yet carry: a resized dropped section (the maps are by address), the
text and scripts (not reached; they should work, resized blocks are what
`loc_build.py` handles between languages already - to measure), and any code.

## 4. What I58 has to change

I58's design holds: canonical EMIs as the base, the mod carries a patch,
kept sections become a DAT layer, mapped data becomes an image layer, code is
refused by name. Four things this hack adds:

1. **The patch is of the disc track, not of an EMI.** The mod carries the
   author's `.xdelta` untouched (its sha256 in `mod.toml`; 1.35 MiB, inside
   `mods.py`'s 4 MiB member bound). The build step applies it to the player's
   own `psx-us` track in a temporary file, reads the patched ISO tree, and
   diffs it file by file against the original. Everything after that is I58's
   sorting step. Keeping the author's file byte for byte is also the plainest
   form of credit and lets a player check it against his release.
2. **A table that moves needs the map translated, not refused.** The aligned
   diff of a patched section against the canonical one gives an address
   translation (old address to new); the map rows and the pointer rebuild are
   run through it. I58 said "a table moved or resized is a refusal"; on the
   first real hack that would refuse BATTLE 15 and with it most of the balance
   changes. A table whose record count or stride changed is still a refusal.
3. **The hash gate needs a mod's provenance.** A chunk built from a patched
   section is listed in the manifest as `mod:<id>@<version>` with its own
   hash, the way mods.md 4 lists a reading-row layer's chunks, and the recipe's
   hash still gates every chunk the patch did not touch.
4. **Code edits are most of the hack's named features.** Ally anywhere,
   masters from the camp menu, Smoke, Examine always learns, and probably the
   double-hit swords are MIPS changes in overlays the PC does not run. Each is
   ours to reimplement, as I58 says, but a hack is one thing to its player: a
   package that silently lacks half its readme is worse than a refusal. So a
   romhack mod names **the behaviours it needs from the engine** (`needs =
   ["ally-anywhere", ...]`), each a switch of ours under a DIV entry, off by
   default, turned on by the mod; the Mods tab greys a mod whose behaviours
   this release does not have, with the reason. The mod stays data; the
   behaviours are reviewed code of ours.

The hack is made against the US disc, so the mod needs `psx-us` held and
plays over `en-US` text. Balance data (enemy tables, `GAME.EMI`'s numbers) is
the same in every language and could be offered over other text later; the
script and text edits cannot.

## 5. The author on the launcher

Built 2026-11-10 (mods.md 2 and 5):

- `[mod] author = "TheGreatBen"`, and `url` and `credit` (one line the author
  chooses): optional for every kind, since furigana 0.1.0 is released without
  them; a romhack kind will require `author` when it exists. The furigana mod
  names the owner from 0.1.1 (`tools/make_mod.py`; the owner's word, 2026-11-10).
- The Mods list shows name, `by <author>`, version, state; the status line
  for the selected mod reads `<name> <version>, by <author>. <credit> <url>
  Kind: <kind>.` `mods.py list --json` and `<home>\mods.toml` carry the keys.
  The url is text: the launcher does not open it.

Not built:

- Where a mod turns behaviours on, the Settings page says which mod asked.
- A repackaged mod attached to a release is named in the release notes with
  its author, and the permission is recorded in `docs/THIRD_PARTY.md` (who,
  when, the condition).

Launcher text follows ASD-STE100.

## 6. Order of work

1. **Done 2026-11-10: `tools/vcdiff.py`** (section 1).
2. **Done 2026-11-10: the census** (section 2.1). What it leaves: the 69
   guessed code runs and the 30 named functions with no PC twin want their
   twins found; then each of the 84 functions is read once and sorted into
   "a constant the PC holds as data after all", "a behaviour to offer", or
   "the hack's own plumbing".
3. **Done 2026-11-10: the author on the launcher** (section 5; `author`,
   `url` and `credit` are optional in `mod.toml`, since furigana 0.1.0 is
   released without them).
4. **Built 2026-11-11, the launcher's half too (5d)
   ([`disc-patch-round.md`](disc-patch-round.md) 5a, 5b). Kind `disc-patch`, kept sections only**: enemy tables, images, and the
   text and script blocks through `loc_build.py`, as an `opt/` layer over
   `en-US`. This is I58's first step with the patch format settled. The enemy
   balance and every text change play at this point.
5. **Built 2026-11-11 ([`disc-patch-round.md`](disc-patch-round.md) 3b, 5c): 2,408 bytes
   of seven table files and 64 loose bytes. The image layer with the translated map**: `GAME.EMI` and BATTLE 15's
   tables (the 240 data runs). I58's second step plus item 2 of section 4.
6. **The behaviours**, one DIV each, from the census. The open-ended part;
   each is also a toggle a player without the hack can use.

Steps 1 to 4 make "a romhack as a mod package" true for data hacks. A hack
like this one is whole only after 6.

## 7. Open

- Area 154's sound bank: whether its change is meant (section 8, M4).
- `disc_patch_census.py` calls a bank's sections `dropped`; the PC keeps them
  as a kind-2 chunk. To correct when the builder is written.
- The 84 functions read against the readme: which feature each edit serves.
  "Probably" in section 4 item 4 is a reading of the readme against the
  sections touched, not a disassembly.

## 8. The disc-patch round's group M (2026-11-11)

The five questions of [`disc-patch-round.md`](disc-patch-round.md) section 1,
run on `main` `8d2d987d`. Scratch root `D:\bof3x-runs\dp1111`: a psx-us
disc-only cache with en-US built that day (`cache_orig`), the scripts and
their logs. The patched track is the one of section 1. No byte of the game is
in this section.

**M1. `loc_build.py` and the patched disc.** `loc_build.py all --disc <patched
track> --cache <a copy of cache_orig> --lang en-US`:

- It stops at `names: the character records differ from the PC's past their
  names`. `convert_char_names` finds the default names in `START.EMI` by the
  numbers that follow each name and checks them against the image; the hack
  changed those numbers (the census's `Char_DefaultRecords` runs).
- Before that it lists 51 `SKIP ... enemy records: ... numbers differ` (41) or
  `the headers differ` (10): an area whose enemy table is not the base's keeps
  no names from the disc. Not a stop.
- With every such check answered yes by a wrapper, it stops at the next table
  it finds the same way: `labels: formations 0 holds a code this language does
  not have`, after `formation pairs` did not match. A waived landmark reads
  the wrong bytes, so waiving is not a route.
- **On a hybrid disc it builds, exit 0, no SKIP.** The hybrid is the original
  disc's file tree, with a section of the patched disc put in where the two
  differ and the section's load address is `0x80010000`, `0x8001A000` or
  `0x0E001000` (`loc_hybrid.py`, a wrapper round `psx_disc.Disc.read` that
  rebuilds the EMI). It took 79 + 44 + 2 sections, the census's copies exactly.

The hybrid's `loc\en-US\dat\` against the original's, chunk by chunk (254
files each):

| Chunk | Differ | Of them resized |
|---|---|---|
| kind 0, tag 0 (an area's script and text block) | 78, in 78 files | 66 |
| kind 0, tag `0x4000` (the shared text pool) | 44, in 44 files | 44 |
| kind 19, tag 0 (DIV-0203's corrected copy, areas 191 and 192) | 2 | 2 |
| kind 1, tag `0x0E001000` (area 115's page) | 1 | 0 |

125 chunks in 123 files, and nothing else. 79 patched script sections give 78
changed blocks: one area's edit does not reach the PC's block on a disc-only
install (it does on a PC-based one, M5: area 7).

**M2. A mod's chunk over a language's.** Read, not run
(`src/game/battle/dat_load.cpp`, `dragon_cmd.cpp` `LoadDatFile`): the shipped
file or the cache's, then the language layer, then each `BOF3X_OPT` layer in
the list's order; every chunk goes to the handler of its kind, and kind 0
copies the payload to its tag. So the last layer walked wins a tag, and a mod
layer holds whole chunks. A kind-19 chunk is taken in any layer and must
follow the kind-0 block of its tag in the same file. A language's enemy names
are separate kind-0 chunks of 12 bytes at `0xC2000 + 0x48 + 0x8C * record`,
over the base's table at `0xC2000`: a mod layer that carries a whole table
must carry the names after it. The live proof is the round's acceptance (a
text change read back from memory).

**M3. The patch's literal bytes** (`m3_add.py`: the instruction stream of all
53 windows walked, every ADD and RUN placed in the patched track and, for user
data, in the patched disc's file tree and against the census's edit runs):

| | Bytes |
|---|---|
| ADD, in a sector's address and mode bytes | 302,896 |
| ADD, in a sector's error-correction and detection bytes, and 64 of subheader | 228,214 |
| ADD, user data, inside one of the author's edit runs | 27,607 |
| ADD, user data, outside every file (directories, path tables) | 6,687 |
| ADD, user data, **the game's bytes the encoder found no copy for** | **1,981** (1,648 in changed files outside every edit run, 333 in files that only moved) |
| RUN (one byte repeated), 3,111 runs | 4,129,805, all but 462 in user data: fill |

567,385 ADD bytes in 193,685 runs. The answer for rule 1 is 1,981, not zero;
an edit run also holds the game's own bytes where the author changed some of a
record and the diff is greedy.

**M4. The three sections that are not text or tables.**

- **Area 154, section 2 (kind 7, load address 2)** is the sample body of the
  area's sound bank: sections 0, 1 and 2 are the bank's header (kind 6), cue
  table (8) and samples (7), which `vag.py` turns into the PC's kind-2 chunk.
  The header and the cue table are the same on both discs. The body differs
  in one run, `0x17000` to `0x1A7FF`: seven whole sectors, none of them a
  sector of the original disc. They lie in samples 10 to 12 of 16. The bank
  built from the patched sections is 469,616 bytes; from the original,
  631,064. `importer.py`'s bank path returns nothing for a bank that is not
  the recipe's, which is why the experiment of section 3 said the sound built
  unchanged: it took the recipe's bank from elsewhere. Whether the author
  meant a sound change is not known; the readme names none.
- **Area 63's image** (kind 3 on the disc) is a base chunk on the PC, copied
  (`psx-us:copy`): the patched section is the chunk. The change is in three
  rows of the page.
- **Area 115's image** is not a base chunk: the PC's is a page with words
  painted on, a language's, and `loc_build.py` builds en-US's from the disc.
  Through the hybrid it comes out as one changed kind-1 chunk (M1's table).

**M5. A PC-based install.** `loc_build.py all --disc ... --game <a copy of the
PC's BOF3.exe and DAT>`, the way `importer.py build_languages` runs it, with
the original disc and with the hybrid (text sections only):

| Chunk | Differ |
|---|---|
| kind 0, tag 0 | 79, in 79 files (the disc-only 78 and area 7) |
| kind 0, tag `0x4000` | 44 |
| kind 19, tag 0 | 2 |

The hybrid's run keeps 497 text slots as the PC shipped them where the
original's keeps 496: one slot the hack changed falls back to the PC's own
text on this install kind. Which one is not looked up. The enemy tables were
not built for this install kind here; they are base chunks from the disc
either way.

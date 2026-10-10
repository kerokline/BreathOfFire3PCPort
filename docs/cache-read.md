# The engine reading the cache: `DAT\`, `SND\` and the layers from the importer's cache

**Status:** IN PROGRESS (2026-10-10, one session on `catchup/cache-read`.
Built with llvm-mingw, no warnings; `BOF3X_SHADOW=dat_cache` and the full
`'*'` self-test exit 0; the start-up check run headless against a real cache
and four synthetic ones; the walk proved offline against `importer.py install`
on all 742 containers. **Not run:** any live play with `BOF3X_CACHE` set -
the owner's, section 9)

[`unified-data-plan.md`](unified-data-plan.md) section 7 and step 6's open
end ([`sound-import.md`](sound-import.md) section 7): until now the engine read
only the cache's `base\bgm` (DIV-0087), and took `DAT\`, `SND\` and `BGM\`
from the install, the language and optional layers copied into `DAT\` by
`importer.py install` under the names the loader walks (DIV-0005, DIV-0086).
This is the rest: with `BOF3X_CACHE` set, the three readers that build a
`DAT\` or `SND\` path open the cache's file first and the install's second.
It is one ledger entry, **DIV-0089** (the next free number on 2026-10-10;
another branch may take it first, in which case it renumbers at the merge).

No game content is reproduced here: counts, names of containers, tags and
hashes only (CLAUDE.md rule 1). The real cache measured is
`analysis/cache/pc-plus-us` (built 2026-10-10 from the PC's `DAT/` and
`BOF3.exe`, the US disc and both PSP discs, with `loc/en-US`,
`opt/area4-walls` and `opt/psp-art`), which is game data and not committed.

## 1. What was built

| Where | What |
|---|---|
| `src/game/dat_cache.{h,cpp}` (new) | The cache's state: the root, `manifest.toml`'s container index, the start-up check, the merged walk, the paths, the overlay precedence, the self-test's seam and in-memory files. |
| `src/game/dat_load.cpp` | `LoadDatFile` `0x454590`: the shipped container from the cache when it holds it whole; each overlay by the precedence below; the chunk switch moved into `TakeChunk`, unchanged, so both walks share it; `ReadOptLayers` accepts a layer the cache has. The end-to-end self-test. |
| `src/game/save_menu.cpp` | `Snd_LoadBankFile` `0x454770` (the banks of a container) from the same merged walk; `Sound_LoadStream` `0x587910` opening `base\snd\NAME.DAT`. Their self-test. |
| `src/hook/inject_all.cpp` | `dat_cache::Arm()` after every module's self-test, beside `music_seq::Arm()`. |
| `src/launcher/config.{h,cpp}`, `config_dialog.cpp` | A language or layer counts as available when the cache has it as well as when `DAT\` has it (section 7). |
| `tools/cache_walk.py` (new) | The engine's rule restated in Python and held to `importer.py install`'s result; `check` (no game data) is part of `importer.py check`. |

## 2. The readers

Three functions of the exe build a `DAT\` or `SND\` path, and only three: a
byte search of `BOF3.exe` for the format strings' addresses (2026-10-10,
section table mapped) finds `"DAT\%s"` `0x652894` referenced at `0x4545B0`
(inside `LoadDatFile` `0x454590`) and `0x45478C` (inside `Snd_LoadBankFile`
`0x454770`), and `"SND\%s.DAT"` `0x666F9C` at `0x587939` (inside
`Sound_LoadStream` `0x587910`). All three are ours. The music's two
formats, `0x666FB8` and `0x666FA8`, are referenced once each, at `0x587A2F`
and `0x587A3F`, inside `Music_LoadFile` `0x587A20` - DIV-0087's seam.

## 3. The shipped container: the PC's slot order

The importer splits each PC container into `base/dat/NAME.DAT` and
`loc/zh-CN/dat/NAME.DAT`, each holding its layer's chunks in file order, the
enemy tables' names as extra kind-0 chunks after the zh file's own
([`importer.md`](importer.md) 2 and 3; `importer.py build`). `verify`
interleaves them back in the recipe's slot order. **The engine must do the
same**, not walk one file and then the other:

- Measured on `recipes/pc-zh.toml` (scratch `order.py`, 2026-10-10): 253
  containers have both layers; walking base then zh reorders 446 pairs of
  chunks. Three pairs touch the same thing: in `FIRST.DAT` a zh kind-1 chunk
  (tag `0x1C080200`) comes before a base one (`0x1A080400`) whose tiles
  overlap it; in `START.DAT` two zh kind-1 chunks move past the base kind-0
  chunk tagged `0x10000`, which resets `Gfx_UploadQueueCount`. The second is
  harmless - `Gfx_LoadImage` writes VRAM at once (`gfx_image.cpp`) and the
  queue is the sprite poses' (`sprite_pose.cpp`) - but the first is not.
- Simulated end states on the real cache (scratch `sim.py`, then
  `tools/cache_walk.py`): base-then-zh gives the install's end state in 741
  of 742 containers, `FIRST.DAT` differing in VRAM; the slot order gives 742
  of 742.

So the engine reads the order from the cache's own `manifest.toml`, whose
`[cache] assets` rows are `[NAME, SLOT, LAYER, WHERE, HASH]`, one per chunk
in slot order (`write_manifest`). `dat_cache::Configure` parses them into a
per-container string of `b` and `z`, and walks the two files by it, then the
zh file's remaining chunks (the names). The names land after the
container's later slots where the PC's chunk carries them in place; that is
the same bytes because no later chunk overlaps an enemy table (the same
`order.py` pass checked every base chunk after a table; the 742-container
end-state comparison covers the rest).

**Held whole, or the install's.** A container is read from the cache only
when every file it needs is there: the base file if it has a base slot, the
zh file if it has a zh slot or names (a `names` row, or a base row made by
`widen`, whose names only the PC carries), and no row with an empty source
(a chunk no source gave). Otherwise `DAT\NAME` is read from the install, the
whole container: the importer writes a layer's container whole or not at
all, and a container with its text missing would play blank. On the real
cache all 742 are held; a disc-only cache would hold the containers with no
Chinese text and leave the rest to the install - which it does not have,
the documented gap ([`importer-transforms.md`](importer-transforms.md) 8).

**The stand-ins.** A cache built without the PC's `DAT/` can hold chunks
where a disc's own section stands in for one only the PC carries (the
manifest's `own` rows, [`importer-transforms.md`](importer-transforms.md) 5).
With the engine reading the cache these now play: that document's "one entry
for the stand-ins as a whole, when the engine reads the cache" is folded into
DIV-0089. By the recipe (2026-10-10), 35 containers have a stand-in; the 21
that also carry Chinese text or enemy names - the world map's dial page
among them - are not held by such a cache (their text layer is missing) and
stay the install's; the other 14 are read with the disc's sections:
`DEMO.DAT`'s language page, 11 `MAGIC*` glyph atlases and `SCENA17.DAT`'s
copy of the page (the disc's own words where the PC kept Japan's). A cache
built with the PC's `DAT/` has no stand-in (the PC carries every chunk), as
`analysis/cache/pc-plus-us` shows. The start-up line counts them
(`N of them with a disc's own section standing in`).

**Checked at start-up.** Every held container's files are walked by their
headers (seeking, not reading - the banks make `base/dat` some 300 MB): the
base file must have exactly a chunk per base row, the zh file a chunk per zh
row and then only kind-0 chunks. A mismatch, a chunk past the end, a
malformed row, rows of one container apart, a names row not after its
table, a layer other than `base` and `loc/zh-CN`, or a manifest whose
`target` is not `pc-zh` is fatal, naming the file or the line. A cache
without `manifest.toml` holds no container (its layers and `base\snd` are
still read).

## 4. The overlays: the precedence `install` gives

`importer.py install` (read 2026-10-10, `cmd_install`) copies
`loc/<tag>/dat/X.DAT` to `DAT/<tag>.X.DAT` and `opt/<layer>/dat/X.DAT` to
`DAT/<layer>.X.DAT`, **after removing every `DAT/<name>.*` file the layer
does not have**, and never touches `DAT/NAME.DAT` itself. So after an install
the language or layer played is the cache's, whole, and an older overlay of
the install's that the cache's layer lacks is gone. The engine reproduces
exactly that:

| Armed | The cache has the layer | ... and a file for `NAME` | Walked |
|---|---|---|---|
| no | - | - | the install's `DAT\<layer>.NAME`, if it exists (DIV-0005 / DIV-0086 as before) |
| yes | no | - | the install's `DAT\<layer>.NAME`, if it exists |
| yes | yes | yes | the cache's `<root>\<loc or opt>\<layer>\dat\NAME` |
| yes | yes | no | **nothing** (install would have removed a stale `DAT\<layer>.NAME`) |

"Has the layer" is `<root>\<kind>\<layer>\dat\` holding a `.DAT`, asked once
at injection for `BOF3X_LANG`'s tag and each `BOF3X_OPT` layer. Layers keep
their order: the language overlay, then the optional layers as listed.

**Evidence that the two agree.**
- `tools/cache_walk.py check` (no game data, in `importer.py check`): a
  synthetic install and cache in a temporary directory, `importer.cmd_install`
  itself run on a copy, every container's end state the same by the engine's
  rule over the uninstalled game and by the plain walk over the installed
  one; the synthetic `A.DAT` has `FIRST.DAT`'s shape. Two controls must
  disagree, and do: a walk without the slots, and a precedence that falls
  back to the install's stale overlay.
- `tools/cache_walk.py compare` on the real cache and the owner's install,
  read only (a virtual install: the cache's layers under `DAT\`'s names, as
  `cmd_install` would leave them): **742 of 742** containers the same end
  state with no language; with `--lang en-US --opt area4-walls --opt
  psp-art` (all three the cache's); and with `--lang fr-FR --opt psp-art`
  (the language the install's, the cache having no `loc/fr-FR`). The control
  without slots differs in `FIRST.DAT` each time.
- `importer.py verify --cache analysis/cache/pc-plus-us`: 742 of 742
  containers byte-identical to `fixtures/pc-zh.DAT.files.tsv` when composed;
  both `opt/` layers as recorded. Its composition is the engine's merge.

## 5. `SND\` and `BGM\`

**`Sound_LoadStream`** opens `<root>\base\snd\NAME.DAT` when the cache has
it and `SND\NAME.DAT` otherwise. The cache's 880 files are byte-identical to
the install's (compared 2026-10-10, 880 of 880); the install's `SND\` has 13
more - the 11 MP3 jingles, `019_02.DAT` and `DIR1` - which the cache never
holds ([`sound-import.md`](sound-import.md) 4), so those are always the
install's. The path buffer grew from `0x28` to `MAX_PATH` for the absolute
path.

**`Snd_LoadBankFile`** reads a container for its kind-2 chunks only; when
the cache holds it, the same merged walk feeds them to `Snd_LoadBank` in the
PC's order (every bank is a base chunk, so their order is the base file's).

**`BGM\%03d.DAT` needed no new code.** DIV-0087's seam already is the
cache-or-install fallback: `Music_LoadFile` takes `<root>\base\bgm\NNN.DAT`
when the cache has the song and the install's `BGM\NNN.DAT` / `BGM\NNNN.DAT`
MP3 otherwise, and its self-test checks both. The cache holds no MP3s by the
plan ([`unified-data-plan.md`](unified-data-plan.md) 7: the MP3s stay the
install's, the fallback for a PC-only install and for `166`), so there is
no cache file for an MP3 to come from. If the owner wants the PC's MP3s in
the cache too (a self-contained cache from a PC source), that is an importer
change - a `base/bgm-mp3/` written by `build` - and a third name in the
seam; not made.

## 6. The switches and the root

- `BOF3X_CACHE` (or the ini's `cache=`): DIV-0087's root, read again here,
  the same way (a trailing separator dropped). Unset: nothing in this
  change runs, and the three readers are the original's. Not a directory:
  fatal. Longer than **214** characters: fatal - the longest path built is
  `\opt\` + a 23-character layer + `\dat\` + a 12-character name, within
  `MAX_PATH` (DIV-0087 allows 226 for its own longest; a root of 215..226
  that played music before now needs `BOF3X_CACHE_DATA=0`). Each path is
  also checked as it is built.
- `BOF3X_CACHE_DATA`: unset or `1`, the cache's `DAT\` and `SND\` files as
  above; `0`, the cache for the music only - the behaviour before this
  change; anything else fatal. No ini key yet.
- Log lines: one at injection for the check (`DIV-0089    the cache ...
  checked: N of the manifest's M containers held whole, P in part`), one per
  layer (`the cache's loc layer` / `the install's`), one when armed. The
  file layer's log line per open (`file_io.cpp`) now names the cache's paths
  - [`attract-mode.md`](attract-mode.md)'s file lists read from such a log
  would show them.

**The owner's call, "`BOF3X_CACHE` as the one root"**
([`sequenced-music-plan.md`](sequenced-music-plan.md) call 4, open on
`HANDOFF.md`'s list) is not decided here. The design works either way:
- **One root (as built).** One variable names `base/`, `loc/` and `opt/`;
  `install` stays a bridge that gives the same result (section 4), so a
  player may use either or both.
- **Not one root** - the cache for the music, `install` the only way into
  `DAT\`: set `BOF3X_CACHE_DATA=0`, or make `0` its default (one line in
  `dat_cache::Configure`); the launcher's availability test follows it.
- **Separate roots** (say a `BOF3X_DATA_CACHE`): `dat_cache::Configure` is the
  one place this change reads the root; the launcher's `ConfigCacheDataRoot`
  the other. Nothing else names the variable.

## 7. The launcher

`ConfigApplyEnvironment` offers a language when `DAT\<tag>.*` exists **or**
the cache (`BOF3X_CACHE`, else the ini's `cache=`; none under
`BOF3X_CACHE_DATA=0`) has `loc\<tag>\dat\*.DAT`, and likewise a layer from
`opt=` or the default `area4-walls`; the settings dialog's language list the
same. Without that, a language that only the cache holds would be dropped
before the DLL saw it. The DLL's own refusal of a layer neither place has
names both. Compiled; not exercised by a self-test (the self-tests run with
`--no-config`).

## 8. The self-tests

`BOF3X_SHADOW=dat_cache` (so in `'*'`), three parts, no game data (two
container names and one stream name read from the exe's tables, as
strings):
1. `dat_cache::SelfTest`: a synthetic manifest in `write_manifest`'s format
   parsed to five containers (an interleaved one with a disc-made enemy
   table and the PC's names, one per layer, one whose table no PC named,
   one with a sourceless row), three held and two in part; the merged walk
   against the PC's order, a lower-case name, the fallbacks, nothing before
   the arming; the sound and layer paths; the precedence table. Controls,
   each refused: the index as a by-layer order, a parse that ignores
   `widen`, one that ignores a sourceless row, a mixing precedence, an
   unarmed one, and the walk without slots. Nine bad caches refused, each
   with its message in the log.
2. `LoadDatFile` itself (`dat_load.cpp`) on in-memory files with the chunk
   handler recorded: armed, the cache's container in slot order, the cache's
   language file, no stale `lay-a`, the install's `lay-b`, and none of the
   install's files the cache stands for opened; a name the manifest lacks
   from the install with the cache's layer file; unarmed, the install's four
   files exactly as DIV-0005 and DIV-0086 walk them. Four controls refused.
3. `Sound_LoadStream` and `Snd_LoadBankFile` (`save_menu.cpp`) with
   `File_Open` and `Snd_LoadBank` recorded: the cache's wave, the install's
   when the cache lacks it or is unarmed; the cache's banks with no install
   file opened. Four controls refused.

**Bugs planted, one at a time, each refused, then removed** (2026-10-10,
rebuilt each time): the merge sorted by layer (walk WRONG); `PickOverlay`
falling back to the install where the cache's layer lacks the file
(precedence WRONG); the same fall-through in `LoadDatFile`'s own switch
(LoadDatFile from the cache WRONG); `Sound_LoadStream` formatting `SND\`
always (from the cache WRONG); `Snd_LoadBankFile` ignoring the cache (from the
cache WRONG); `widen` ignored in the parse (index NONAME.DAT zh); the walk
taken before the arming (unarmed WRONG). Each exited 3 with that line.

**Start-up, headless** (`BOF3X_SELFTEST_ONLY=1`, no shadow):
`BOF3X_CACHE=analysis/cache/pc-plus-us BOF3X_LANG=en-US BOF3X_OPT=psp-art`
exits 0 with `742 of the manifest's 742 containers held whole, 0 in part`,
both layers the cache's, armed, and DIV-0087's `166 songs and 81 banks`.
Four synthetic caches (scratch `badcache.py`): a base file one chunk short
of its rows, a truncated chunk, a row cut short - each exit 3 naming the
file or line; an intact one exit 0. `BOF3X_CACHE_DATA=0` over the damaged
one exits 0 (the check not run); `BOF3X_CACHE_DATA=2` exits 3.

**The full self-test** `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`: exit 0,
`inject: 10081 ours, 0 left original by BOF3X_ORIGINAL`.

## 9. What only a live run shows (the owner's)

- A play with `BOF3X_CACHE=<cache>` against one without, on the same save:
  the title, a town, a fight, the world map's dial page, the menus drawing
  from `FIRST.DAT`'s images (the one container whose order matters; which
  screen shows the overlapped tiles is not read), an inn (`SND\` waves), a
  language overlay and `psp-art` (Stallion, area 67) from the cache with
  nothing installed in `DAT\`.
- The state hash A/B ([`state-hash.md`](state-hash.md)): the bytes loaded
  are the same, but the walk allocates two buffers where the original
  allocated one, so later heap addresses the game stores may differ - a
  difference in pointer words, not content, to tell apart from a real one.
- Start-up time with the check (some 1,000 files' headers): not measured.

## 10. Open

- The owner's call above; and whether `BOF3X_CACHE_DATA` wants an ini key.
- The MP3s in the cache (section 5), if a self-contained cache is wanted.
- The renderer and the state hash read nothing from the cache; `base/exe/`
  is step 8's engine half, untouched here.

## For the other files

- **STATUS.md** (the coordinator's): "DIV-0089 (2026-10-10): with
  `BOF3X_CACHE` set, `LoadDatFile`, `Snd_LoadBankFile` and `Sound_LoadStream`
  read the cache's `base/dat` + `loc/zh-CN/dat` (in the manifest's slot
  order), `loc/<tag>`, `opt/<layer>` and `base/snd` before the install's;
  `BOF3X_CACHE_DATA=0` for the music only. Self-tested and proved offline
  (742 of 742 against `install`); not played. [`cache-read.md`](cache-read.md)."
- **HANDOFF.md**: item 3's "the engine reading the cache" done but its live
  check (section 9); the owner's one-root call now has its consequences
  written (section 6).

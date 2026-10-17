# An install against the release: which pieces are stale

**Status:** DONE (built and measured 2026-10-17 on `install/recipe-version`; the
launcher's prompt built, not yet seen by the owner)

HANDOFF item 11. A player's install is built once by the importer from their own
discs (`importer.py setup`, [`install-sources.md`](install-sources.md) 5). A new
release brings a new launcher, DLL and importer, but the install's cache - the
exe image `base/exe/data.bin`, the language layers, the sound effects, the movie -
stays what the old importer made. Until now the only guard was
`exe_image.cpp`'s `FilledSince()`, a hand-kept list of exe ranges an older image
left as gaps: a fix nobody listed there stayed silent.

The owner's calls (2026-10-17): **prompt, then rebuild** (no starts nothing);
staleness covers **the exe image and the cache recipe**, and more - **each piece
judged on its own**, so a rebuild makes only what changed.

## 1. Measured records, not declared keys

Every piece of the cache is built inside a `build_reuse.Step`
(`tools/build_reuse.py`), which records in `<cache>/build-keys/<step>.json`
what the step was *measured* to read:

| Part | How measured |
|---|---|
| **code** | each function of `tools/` that ran, by file and qualified name, with a digest of its compiled body (bytecode, names, constants, flags; no line numbers). `sys.monitoring` `PY_START`, `DISABLE` after the first call: about free. A child Python the step starts (`loc_build.py`) records into the same step through `BOF3X_STEP_RECORD`. |
| **globals** | every data-like module-level value of a loaded `tools/` module whose name a recorded function's code names (`co_names`): a constant table, `FMV_ARGS`. Digested in a **fresh interpreter**, so a memo filled during the build never counts. |
| **data** | every file of the release's data (`SHIPPED_DATA`: `recipes/`, `fixtures/`, `exe_maps/`, ...) the step opened, with its sha256 - an audit hook on `open`. |
| **inputs** | what the caller names: source discs, the player's ffmpeg, digests of earlier steps' output (a language layer keys on `base/dat` and `base/exe`). |

A step's record **holds** when the Python, the inputs, and every function,
global and data file it lists digest the same. A function the step did not run
cannot have changed what it made, unless a function it did run changed to call
it - and that function's own digest catches that.

**Memos.** A value one step caches and a later step reads would be read
unmeasured, so every step starts with the memos of `tools/` emptied: every
`functools.lru_cache`, and the module-level memos in `build_reuse.MEMOS`.
`build_reuse.py lint` (run by `importer.py check`) fails on a module-level
global a setup tool writes at run time that `MEMOS` does not list.

The steps: `base` (identify, recipe, base policy, every chunk), `snd` / `snd-psp`,
`bgm`, `exe`, `loc-<tag>`, `opt`, `manifest`, `fmv`, `opt-<psp layer>`, and
setup's `game-pc` and `game-install`. Reused when their record holds: `snd`,
each `loc-<tag>`, each PSP layer, and the movie by its own rule
(`fmv_reusable`, bit-exact arguments). The rest are rebuilt each setup (seconds).

## 2. `importer.py stale` and the launcher

`importer.py stale --home H` judges each record against the release (code,
globals, data; not the inputs, which are the player's): `stale <step>\t<reason>`
lines then `stale N` and exit 1, or `fresh` (0), or `none` (2, no cache). A
cache with no measured records - any cache built before this change, which has
`<step>.txt` keys - is `stale cache`, once.

The launcher (`launcher.cpp`, `sources_dialog.cpp` `SourcesCheckStale`) runs it
before the settings, only for the install the Sources page made (the home's
`game\` and `cache\`, no other cache named, no self-test or `--no-config`). A
fresh answer is remembered in `build-keys\checked.txt` by the launcher's and the
importer's sizes and write times, so an unchanged release asks nothing at the
next start. Stale: a message box names the pieces; yes opens the Sources page,
which builds at once (`SourcesDialogRun`'s `rebuild`) and starts the game when
done; no starts nothing. A check that cannot run is logged and the game starts.

`FilledSince()` stays as the DLL's backstop for an image whose gap crashes (a
cache named by `BOF3X_CACHE` or `cache=`, which the launcher does not judge).

## 3. Measured (2026-10-17, this machine, `psx-us` alone, home `C:\Users\kerok\rv`)

| Run | Time |
|---|---|
| first setup | 270 s (movie 141, `loc/en-US` 62, `snd` 46, chunks 10) |
| `stale` after it | `fresh`, 0.6 s |
| setup again, nothing changed | **62 s**: `snd`, `loc/en-US`, the movie taken from the cache; `stale` fresh after |

What one change reaches (`stale` on that install, each change put back after):

| Change | Stale |
|---|---|
| a comment, or a new function, in `importer.py` | nothing |
| `cmd_verify`'s body (setup never runs it) | nothing |
| `psp_exe.TWINS` (read only when the recipes are generated) | nothing |
| a row of `recipes/exe-psp.tsv` (what the #77 fix changes) | `exe` |
| `loc_build.run_upscaler` (not run for a layer) | nothing |
| `xa.importer_snd`'s body | `snd` |
| `FMV_ARGS` | `fmv` |
| `recipes/pc-zh.toml` | `base`, `exe`, `loc-en-US`, `manifest` |

A setup after the `exe-psp.tsv` change rebuilds `exe`; a language layer is
rebuilt too only if the image's bytes changed (its input is `base/exe`'s digest).

## 4. Not done

- The launcher's prompt has not been seen in a real start (the owner's eye).
- `psp_layers.py`'s `[psp_layers]` manifest table is written outside any step;
  a change to its writer alone is not seen.
- A Python a step starts other than `loc_build.py` must import `build_reuse` to
  be recorded; there is none today.

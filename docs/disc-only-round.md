# The disc-only round: a playable build from a PlayStation disc alone

**Status:** IN PROGRESS (staged 2026-10-12 morning from `main` `39bd3740`, PR #54;
the owner's "what is next" of 2026-10-11, [`HANDOFF.md`](HANDOFF.md) item 0).
Six agent groups in parallel on stage A, one on stage B after they merge, the
validations batched at the end (section 4). Nothing pushed.

## 0. What the round is for

After state 3 ([`state3-build.md`](state3-build.md), DIV-0094) the game runs
in our own `bof3x.exe`, but the engine still opens the player's `BOF3.exe` at
start: for `.rdata`, for `.data` (unless `BOF3X_EXEIMAGE` names a cache built
from the PC's exe), for ~450 bytes of Capcom's operands our screens read
back (`CodeBytes`, `state3-build.md` 2.3), and the launcher for the default
key table. A cache built from a PlayStation disc alone
([`exe-import-engine.md`](exe-import-engine.md) 4.4) is compared and logged,
never mapped, because its image is incomplete: the 9,142 code-pointer
words, the data pointers that do not rebuild, the `.bss` / `.rdata` pointer
words, and 22,331 non-zero bytes no disc carries (JP). The data side lacks
14 port-edited arenas ([`type1-compression.md`](type1-compression.md) 3) and
the language layer's gaps ([`loc-build-disc-only.md`](loc-build-disc-only.md) 7).

This round closes every one of those so that **a US disc alone, with no
`BOF3.exe` and no PC `DAT/`, gives a cache the engine maps and plays**, and
the gate is measured: the state hash of a run on the disc-built image against
the same run on the PC-built one, with `data.toml`'s engine-filled ranges as
the skip list.

The owner's words for the scope (2026-10-11): the code pointers and the
unplaced pointer words held as layout; the ~22 KB no disc carries as the
engine's own; the operand store's bytes as values of ours; the default key
table as a rule; the language gaps of `loc-build-disc-only.md`.

## 1. The groups (stage A, parallel)

| Code | Branch | Owns | Deliverable |
|---|---|---|---|
| CP | `disc-only/pointers` | `tools/exe_tables.py` (`recipe`, `disc_image`, `decide_pointers`, `verify`), `recipes/exe-*.tsv`, `data.toml`'s classes | every pointer-shaped word of a disc-built image equal to the PC's: the code pointers and the unrebuilt data pointers as a layout table of addresses (no values), the 117 pooled zero objects and the 147 `.bss` / 63 `.rdata` words likewise; `verify` says `pointer N of N` |
| NB | `disc-only/engine-own` | new `src/hook/exe_own.*` (the fill), a measurement under `tools/`, `docs/exe-engine-own.md` | the 22,331 number bytes no disc carries classified by region and held by meaning as the engine's own where they are the port's (file layer, platform regions, re-laid plates); language bytes listed for LG; a Capcom game-value table reported, not copied |
| OP | `disc-only/operands` | `src/hook/code_bytes.*`, the ~43 reader sites in ~30 `src/game` files, `docs/operand-store.md` | each read of a Capcom operand becomes a value of ours owned by the divergence or function that reads it; the exe host reads no `.text` byte of the file; a hosted check keeps the values equal to the patched code |
| RD | `disc-only/rdata-and-keys` | `src/hook/exe_host.cpp`, `src/game/rdata_consts.*`, `src/launcher/launcher.cpp` (the key table, the file optional), `src/input/bindings.*`, `src/game/shell*` (the key table) | the exe host maps nothing from the file's `.rdata` (`c_dfDIKeyboard` from the SDK); the default key table a rule of the engine and the launcher; `BOF3.exe` optional when a cache is the `.data` source |
| AR | `disc-only/arenas` | `tools/importer.py` (the arena rows), `tools/type1.py`, `recipes/pc-zh.toml`, `docs/arena-edits.md` | the 14 port-edited arenas and `START` read against the PC's consumers; the honest form built (a rule, or a recipe row) so a US disc alone writes the 15 containers, verified against the fixtures |
| LG | `disc-only/language` | `tools/loc_build.py`, `docs/loc-build-disc-only.md` | the village sheet, the 11 glyph atlases, the two kanji sheets and the four area pages as disc stand-ins; FR `AREA004`; the CONFIG row for a French- or German-only cache; the four `tag` NameErrors |

Stage B: **IN** `disc-only/integration` - the disc-only install end to end on
the merged tip: `importer.py build --source <US cue>` complete, the exe host
mapping the disc-built image (`BOF3X_EXEIMAGE` accepts an image whose
`data.toml` names every engine-filled range), `BOF3X_CACHE` the data, the
launcher starting an install with no `BOF3.exe`, the self-tests under it, and
the gate's two runs.

Briefs: `<session 24422d10 scratchpad>/r/common.md` + `brief_*.md`.

## 2. Out of scope, listed (the owner's calls)

- The PC path's 34 French and 11 German misread slots (`loc-build-disc-only.md` 7.3).
- A disc-only player's dial page; the 11 jingles; the PAL banks (`owner-review.md`).
- FMV from a disc (MDEC / STR, phase 5, I7): a disc-only install has no AVIs;
  IN reports what the engine does without them and the ledger form it wants.
- The four stale Chinese route pairs and the `en-US` `cutsceneAndNue`
  difference (`state3-build.md` 5.4) - not this round's.

## 3. Merges

(filled as groups merge: branch, tip, what the merge needed)

## 4. The validations, batched

(filled at the end: the hosted `'*'` narrow and wide, the exe host's
self-test, the gate's state-hash pair, the attract and the routes against the
existing pairs)

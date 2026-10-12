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

One at a time onto `platform/disc-only-round`, in a scratch worktree; the build
and the exe host's self-test after each; the full validations in section 4.

| # | Group | Branch, tip | Merge | What it brought |
|---|---|---|---|---|
| 1 | RD | `disc-only/rdata-and-keys` `62b2b06a` | `eaae3db8`, clean | `.rdata` from the engine (`rdata::Map`: the 44 constants, the SDK's `c_dfDIKeyboard` at `0x5C4948`, the 14 bound slots; the 63 `.data` words into `.rdata` are all the C runtime's, read by nothing of ours); the default key table as a rule (`input::DefaultKeys()`, 24 `DIK_` names, written to `0x66C648` at map time, compared with the file's when present); `BOF3.exe` optional (`bof3::ExeFile_Present()`; the launcher accepts an install with no file when a cache is named and sets `BOF3X_EXEIMAGE` from it). **Found:** the keyboard has been dead in `bof3x.exe` since state 3 - Capcom's keyboard object table lies in `.text` at `0x5ACBD0` (the int3 fill in the exe host; DirectInput 7 rejects it: `SetDataFormat` / `Acquire` `E_INVALIDARG`), the SDK's format is accepted; a live key press in `bof3x.exe` is owed. `symbols.toml`: `DInput_KeyboardFormat`'s object table corrected to `0x5ACBD0`. |
| 2 | AR | `disc-only/arenas` `865756d6` | `789c80ed`, clean | the port's edits read ([`arena-edits.md`](arena-edits.md)): in every edited bank only the animation scripts changed (member 2's bank in the eight `PL` arenas: 10 of 69 scripts, parts moved or added, three unplayed frames dropped, net -352; `BPLD01x`: member 1's script 1 gains two parts, the "1,949 words" are offset tables shifting behind it; `27A`: the second of two identical 4-step loops held 8 ticks a step instead of 4; `START` is `PL012`'s arena, loaded once at boot). Forms: `27A` a rule (`hold`, `pc-hold`, no ledger entry); `START` a rule (`start-arena`); the 8 `PL` + 3 `BPLD` the disc's arena as is (`own_why = "arena"`, **DIV-TBD-AR-1**, the owner's eye on member 2's field sprites and member 1's battle sprite). **A US disc alone: 741 of 741 `base/` containers** (from 727; 476 byte-identical, 12 a disc's arena); JP 741 of 741; the PC + US + PSP build byte-identical on all 2,125 files. Count correction: 13 arenas + `START` = 14 stems; `AFLDKWA` is `loc/zh-CN` only, so `base/` is 741 of the 742. A PSP-only build still lacks the 14 (`OWN_FROM` is the PSX discs'). |
| 3 | LG | `disc-only/language` `a19f8db4` | `22cdff33`, clean | the port's glyph atlases and kanji sheets are the PlayStation's sheets (stored as tiles, not rows) with rectangles cleared and a strip of their own 16x16 icons re-laid at a pitch of 20: a rule of coordinates (`RELAID`, `relay_sheet`), byte-identical to the port's chunk from the JP disc, re-checked on every PC-path build; disc-only, `build_standins` writes the 16 remaining `loc/zh-CN` images as the disc's own sections (re-laid where the rule applies) and the village sheet (its rows 96..175 were empty PlayStation cells the port cleared, not frame pieces - the doc corrected); `FIRST.DAT`'s atlas left out (a later base slot covers it on the PC too). French and German title pages: CONFIG cut from the disc's own letters found by hash, byte-identical to the PC-built pages; FR `AREA004`: the slot count from the first in-block offset, the 7 bad slots at the first message as the German table's are. The four `tag` NameErrors gone; `loc_build.py selftest` in CI. Exe language bytes on a US-only image: 182 uncarried, 83 written by the handlers, 91 read through our buffers, **8 stay zero and none crashes** (doc 5.4: four want a new label group in `src/`, `0x6531F4` is the `%d/%d` format - NB's, the faerie stats and the area-limit bytes are game values with no chunk kind). Measured: the PC path byte-identical to the parent on every tag; single-disc layers US 245->254, FR 244->254, DE 244->254 containers; coverage of the 475 `loc/zh-CN` chunks 363/361/362 -> 381 each. **DIV-TBD-LG-1** (the disc-only images), **DIV-TBD-LG-2** (FR `AREA004`), an amendment to DIV-0090. **Found, not fixed (the owner's call):** label group 16 (DIV-0064) writes the faerie names one record late - record 0 never named, the "unnamed" record 59 is the area-limit table at `0x6536B0`; the fix is `labels.cpp` + `loc_build.py` and changes the PC path's output. |

## 4. The validations, batched

(filled at the end: the hosted `'*'` narrow and wide, the exe host's
self-test, the gate's state-hash pair, the attract and the routes against the
existing pairs)

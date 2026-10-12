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

Raised by the groups (2026-10-12), for the owner:

- **Capcom game values no disc carries** (NB, [`exe-engine-own.md`](exe-engine-own.md)):
  JP 1,622 / US 2,135 bytes - `Effect_HpDamageVariance+0xB8` (468), `Magic_Rows+0x4`
  (300), `Equip_SlotIcons+0x50` (131), `EventBattle_Records+0xE0` (82) the largest.
  Rule 1 says they are not copied; a disc-built image maps them as zero, logged
  (IN). What a player sees is IN's reading; whether a disc-only build accepts it,
  derives them by a rule, or requires the PC's exe for those tables is the owner's.
- **The faerie village's label group writes names one record late** (LG,
  `loc-build-disc-only.md` 5.4; DIV-0064's group 16): record 0 never named, the
  "unnamed" record 59 is the area-limit table. A `labels.cpp` + `loc_build.py` fix
  changes the PC path's output: a ledger entry when the owner says.
- **The porting house's logo movie** (`LOGOS.AVI`, the second of `Fmv_Play`'s two
  calls, no disc counterpart; `fmv-sibling.md` in the session scratchpad): keep
  it in the PC-install build, or drop it with a ledger entry? The owner asked
  2026-10-12 whether there is a reason to keep it; none technical was found.
- **The FMV from the disc**: decided 2026-10-12 - converted at install time by the
  player's ffmpeg into the AVI the engine already plays (FM, stage B), no decoder
  in the engine.
- The six doubtful `.bss` words CP holds as pointers (24 bytes); DIV-0055's 94
  plate bytes with no transform (the PSX source unmeasured).

## 3. Merges

One at a time onto `platform/disc-only-round`, in a scratch worktree; the build
and the exe host's self-test after each; the full validations in section 4.

| # | Group | Branch, tip | Merge | What it brought |
|---|---|---|---|---|
| 1 | RD | `disc-only/rdata-and-keys` `62b2b06a` | `eaae3db8`, clean | `.rdata` from the engine (`rdata::Map`: the 44 constants, the SDK's `c_dfDIKeyboard` at `0x5C4948`, the 14 bound slots; the 63 `.data` words into `.rdata` are all the C runtime's, read by nothing of ours); the default key table as a rule (`input::DefaultKeys()`, 24 `DIK_` names, written to `0x66C648` at map time, compared with the file's when present); `BOF3.exe` optional (`bof3::ExeFile_Present()`; the launcher accepts an install with no file when a cache is named and sets `BOF3X_EXEIMAGE` from it). **Found:** the keyboard has been dead in `bof3x.exe` since state 3 - Capcom's keyboard object table lies in `.text` at `0x5ACBD0` (the int3 fill in the exe host; DirectInput 7 rejects it: `SetDataFormat` / `Acquire` `E_INVALIDARG`), the SDK's format is accepted; a live key press in `bof3x.exe` is owed. `symbols.toml`: `DInput_KeyboardFormat`'s object table corrected to `0x5ACBD0`. |
| 2 | AR | `disc-only/arenas` `865756d6` | `789c80ed`, clean | the port's edits read ([`arena-edits.md`](arena-edits.md)): in every edited bank only the animation scripts changed (member 2's bank in the eight `PL` arenas: 10 of 69 scripts, parts moved or added, three unplayed frames dropped, net -352; `BPLD01x`: member 1's script 1 gains two parts, the "1,949 words" are offset tables shifting behind it; `27A`: the second of two identical 4-step loops held 8 ticks a step instead of 4; `START` is `PL012`'s arena, loaded once at boot). Forms: `27A` a rule (`hold`, `pc-hold`, no ledger entry); `START` a rule (`start-arena`); the 8 `PL` + 3 `BPLD` the disc's arena as is (`own_why = "arena"`, **DIV-TBD-AR-1**, the owner's eye on member 2's field sprites and member 1's battle sprite). **A US disc alone: 741 of 741 `base/` containers** (from 727; 476 byte-identical, 12 a disc's arena); JP 741 of 741; the PC + US + PSP build byte-identical on all 2,125 files. Count correction: 13 arenas + `START` = 14 stems; `AFLDKWA` is `loc/zh-CN` only, so `base/` is 741 of the 742. A PSP-only build still lacks the 14 (`OWN_FROM` is the PSX discs'). |
| 3 | LG | `disc-only/language` `a19f8db4` | `22cdff33`, clean | the port's glyph atlases and kanji sheets are the PlayStation's sheets (stored as tiles, not rows) with rectangles cleared and a strip of their own 16x16 icons re-laid at a pitch of 20: a rule of coordinates (`RELAID`, `relay_sheet`), byte-identical to the port's chunk from the JP disc, re-checked on every PC-path build; disc-only, `build_standins` writes the 16 remaining `loc/zh-CN` images as the disc's own sections (re-laid where the rule applies) and the village sheet (its rows 96..175 were empty PlayStation cells the port cleared, not frame pieces - the doc corrected); `FIRST.DAT`'s atlas left out (a later base slot covers it on the PC too). French and German title pages: CONFIG cut from the disc's own letters found by hash, byte-identical to the PC-built pages; FR `AREA004`: the slot count from the first in-block offset, the 7 bad slots at the first message as the German table's are. The four `tag` NameErrors gone; `loc_build.py selftest` in CI. Exe language bytes on a US-only image: 182 uncarried, 83 written by the handlers, 91 read through our buffers, **8 stay zero and none crashes** (doc 5.4: four want a new label group in `src/`, `0x6531F4` is the `%d/%d` format - NB's, the faerie stats and the area-limit bytes are game values with no chunk kind). Measured: the PC path byte-identical to the parent on every tag; single-disc layers US 245->254, FR 244->254, DE 244->254 containers; coverage of the 475 `loc/zh-CN` chunks 363/361/362 -> 381 each. **DIV-TBD-LG-1** (the disc-only images), **DIV-TBD-LG-2** (FR `AREA004`), an amendment to DIV-0090. **Found, not fixed (the owner's call):** label group 16 (DIV-0064) writes the faerie names one record late - record 0 never named, the "unnamed" record 59 is the area-limit table at `0x6536B0`; the fix is `labels.cpp` + `loc_build.py` and changes the PC path's output. |
| 4 | NB | `disc-only/engine-own` `b59a7b34` | `0668b698`, clean | `tools/exe_own.py` (`measure`, `gen`, `check`, `text_words`) classes every unfilled non-zero number byte of the JP and US images; `src/hook/exe_own.*` holds **17 regions, 20,866 bytes by meaning** (the DAT and SND name pools generated from `recipes/pc-zh*.toml` by the measured layout rule into `exe_own_names.inc`; the platform strings, the initial values, the GTE identity; `Math_SinTable` computed from a sine rule), `ExeOwn_Fill` after the copy in `ExeImage_Source`, verified at every start as `InjectAll` step 2 and under `BOF3X_SHADOW=exe_own`, `exe_own.py check` in CI ([`exe-engine-own.md`](exe-engine-own.md)). Of the 22,331 / 23,013 bytes (JP / US): held 13,942; unread in the exe host 3,856 (the old decoder's data, the C runtime's, the MMX tables); language 1,745 / 1,738 (LG's list); **game values, not copied: 1,622 / 2,135** (`Effect_HpDamageVariance+0xB8` 468, `Magic_Rows+0x4` 300, `Equip_SlotIcons+0x50` 131, `EventBattle_Records+0xE0` 82 the largest - **the owner's call**, section 2); on the disc but not placed 1,026 / 1,202 (CP's places); DIV-0055's plates 94 (no transform; the PSX source unmeasured); the key table 46 (RD's). 319 words of `exe-pointers.tsv` are text inside held regions (sent to CP). No ledger entry: with the PC's `.data` the fill changes no byte. |
| 5 | OP | `disc-only/operands` `3d75d764` | `2389aecd`, one conflict (`exe_host.cpp`: RD's `OpenExe` / `CopyData` kept, the `CodeBytes_Open` call dropped) | every `CodeBytes` read gone from `src/`; each value owned: patched operands by their divergence (`Widescreen_SlideBound` / `_SlideOperand` / `_RangeOperand`, `ConfigText_Operands()`, yes_no_layout's stops and lines, `draw_pool`'s counts, cheats' masks, `TitleMenu_Width`), re-aimed calls by the divergence that re-aims them (`MenuFrame_Draw()`, `ListTitle_SiteDraw()`, ...), the compiler tables as switches in our functions (`Tail32Case`, d3d_list `Entry`, `KindCase`, `VariantCase`, `NibbleBlocks`); the verifications registered and compared hosted only. The hosted check: `Operand` / `OperandExpect` / `OperandCall` / `OperandRule` register 77 values (1,310 bytes; the store held 43 reads, 450 bytes), `Operands_Check` from `InjectReport` and after the late FIRST.DAT / START.DAT patches, `Operands_BeforeJmp` for a site under a jmp of ours (`Menu_YesNo`'s call `0x5747D2`, found by the first run); in the exe host an inventory line, nothing read ([`operand-store.md`](operand-store.md)). `exe-table-audit.md` 7's field_blocked mask read superseded. No ledger entry. |
| 6 | CP | `disc-only/pointers` `23f993df` | `d74a870d`, clean | `recipes/exe-layout.tsv`, one row per pointer word (`pc kind target`; code 8,784 - 8,673 by `[[func]]` name, 111 by address - data 9,959, rdata 63, bss 35, number 471), written by `recipe`, checked by `check`, applied by `disc_built` as one `layout` range after the rebuild (a rebuilt word disagreeing with the table stops the build; none does); the 462 + 7 mid-function words, the `.bss` words and NB's text words classed by rules (`code_numbers`: a pointer only at a `[[func]]` start or an instruction boundary after `ret` / `int3`; `bss_numbers`; `own_numbers`), `exe-pointers.tsv` 19,311 -> 18,840 words; `data.toml` classes `layout` and `own` (NB's regions, over `pointer` and `none`); byte-window keys for the places (JP +9 places, +662 bytes). **Unfilled pointer bytes JP / US: 48,692 / 49,408 -> 0 / 0**; `none` with PC bytes non-zero 22,331 / 23,013 -> 8,071 / 8,732; bytes identical to the PC's 577,657 / 576,417 -> 612,863 / 612,176 of 638,976. Six doubtful `.bss` words held as pointers (24 bytes, doc 7.2); 111 code targets with no `[[func]]` and no stub, read by nothing of ours. `exe_image.cpp`'s `Count by[]` still lacks `layout` / `own` (IN's). No ledger entry. |
| 7 | FM | `disc-only/fmv` `c0968241` (stage B) | clean | the disc's one movie, `LOGO/CAPCOM30.STR` (every PSX disc, 1,155 sectors, byte-identical from the subheader on; 320x240, 231 frames at 30 fps from the sector timing, 7.70 s; XA 37.8 kHz stereo), converted by the player's ffmpeg at `importer.py build` into `base/fmv/CAPCOM.AVI` - Cinepak as DIV-0001's, 640x480 by pixel doubling, 30 fps (`-r 30`: ffmpeg's `psxstr` demuxer stamps 15), PCM 44.1 kHz; the PC's own `capcom.avi` is Cinepak 640x480 15 fps 112 frames, PCM 22.05 kHz; SSIM 0.970 against a direct decode (the PC's re-encode 0.990); about 2 minutes; `install` copies it unless a `capcom.avi` is there; no ffmpeg: a logged skip, recorded for `verify` ([`fmv-import.md`](fmv-import.md), **DIV-TBD-FM-1**). Verified outside the game: 32-bit Video for Windows decodes 231 of 231 frames, MCI with the game's own `open avivideo!` string plays to frame 231. **Not played in the game, not heard.** Found: MCI error 304 on a full path of about 160 characters (the 8.3 form opens; the PC's own movie has the same limit); `Disc_Probe` `0x5A72C0` tests for `CAPCOM.AVI` as "local install" (IN's). |
| 8 | IN | `disc-only/integration` `e75694b1` (stage B) | clean | `recipes/exe-gaps.tsv` (`exe_tables.py gaps`, from `exe_own.py measure` on all seven discs: addresses, classes, symbols) classes every unfilled byte where the PC has a value as `language`, `unread`, `keys` or `gap` (game-value, disc-unplaced, port-relaid, LG's eight as `language-zero`); the importer writes the classes into `data.toml`, so `none` now means zero on the PC too; **`exe_image.cpp` maps a disc-built image** (a `pointer` range or an old cache refused; the engine's regions filled; each gap logged once; `BOF3X_EXEIMAGE_STRICT=1` refuses any gap), `ExeImage_CompareWithFile` when the file is present (a Fatal if `none`, `own`, `keys`, `layout`, `rebuilt` or `exe` differ); `importer.py install --base` lays out `DAT/` (742), `SND/` (880), an empty `BGM/`; a missing movie skipped as the port does, logged once; `Disc_Probe`'s "local install" test (`CAPCOM.AVI`) satisfied by a mapped disc image (`win_main.cpp`, **DIV-TBD-IN-1**); `exe_tables.py gate-skip` writes the gate's list (the committed list + 1,222 ranges, 48,955 bytes; `layout` / `own` / `keys` stay hashed since the map step proves them equal - a departure from the brief, right) ([`disc-only-install.md`](disc-only-install.md)). Measured: the real install with the US cache 627,123 of 638,976 bytes equal to the file, every class that must equal equal; **the scratch install with no `BOF3.exe` under `en-US`: self-test exit 0, 10,088 ours**. US gaps: 456 ranges, 3,358 PC bytes (game-value 2,196, disc-unplaced 818, language-zero 238, port-relaid 106); read, not played: `Magic_Rows` (each effect's file number 0, the wrong effect file - the most visible), the dragon-form set files, the ending roll's steps, the equipment / experience panels' numbers, one mode's per-record words. **The gate's attract pair: identical at tick 0 (with the real install's `BOF3.CFG`), the VRAM shadow and `0x903000` from tick 1, sprites / CLUT strip / area map from tick 1310 - not yet read to the byte** (section 4). |

## 4. The validations, batched

(filled at the end: the hosted `'*'` narrow and wide, the exe host's
self-test, the gate's state-hash pair, the attract and the routes against the
existing pairs)

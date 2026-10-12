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
| 2 | AR | `disc-only/arenas` `865756d6` | `789c80ed`, clean | the port's edits read ([`arena-edits.md`](arena-edits.md)): in every edited bank only the animation scripts changed (member 2's bank in the eight `PL` arenas: 10 of 69 scripts, parts moved or added, three unplayed frames dropped, net -352; `BPLD01x`: member 1's script 1 gains two parts, the "1,949 words" are offset tables shifting behind it; `27A`: the second of two identical 4-step loops held 8 ticks a step instead of 4; `START` is `PL012`'s arena, loaded once at boot). Forms: `27A` a rule (`hold`, `pc-hold`, no ledger entry); `START` a rule (`start-arena`); the 8 `PL` + 3 `BPLD` the disc's arena as is (`own_why = "arena"`, **DIV-0096**, the owner's eye on member 2's field sprites and member 1's battle sprite). **A US disc alone: 741 of 741 `base/` containers** (from 727; 476 byte-identical, 12 a disc's arena); JP 741 of 741; the PC + US + PSP build byte-identical on all 2,125 files. Count correction: 13 arenas + `START` = 14 stems; `AFLDKWA` is `loc/zh-CN` only, so `base/` is 741 of the 742. A PSP-only build still lacks the 14 (`OWN_FROM` is the PSX discs'). |
| 3 | LG | `disc-only/language` `a19f8db4` | `22cdff33`, clean | the port's glyph atlases and kanji sheets are the PlayStation's sheets (stored as tiles, not rows) with rectangles cleared and a strip of their own 16x16 icons re-laid at a pitch of 20: a rule of coordinates (`RELAID`, `relay_sheet`), byte-identical to the port's chunk from the JP disc, re-checked on every PC-path build; disc-only, `build_standins` writes the 16 remaining `loc/zh-CN` images as the disc's own sections (re-laid where the rule applies) and the village sheet (its rows 96..175 were empty PlayStation cells the port cleared, not frame pieces - the doc corrected); `FIRST.DAT`'s atlas left out (a later base slot covers it on the PC too). French and German title pages: CONFIG cut from the disc's own letters found by hash, byte-identical to the PC-built pages; FR `AREA004`: the slot count from the first in-block offset, the 7 bad slots at the first message as the German table's are. The four `tag` NameErrors gone; `loc_build.py selftest` in CI. Exe language bytes on a US-only image: 182 uncarried, 83 written by the handlers, 91 read through our buffers, **8 stay zero and none crashes** (doc 5.4: four want a new label group in `src/`, `0x6531F4` is the `%d/%d` format - NB's, the faerie stats and the area-limit bytes are game values with no chunk kind). Measured: the PC path byte-identical to the parent on every tag; single-disc layers US 245->254, FR 244->254, DE 244->254 containers; coverage of the 475 `loc/zh-CN` chunks 363/361/362 -> 381 each. **DIV-0097** (the disc-only images), **DIV-0098** (FR `AREA004`), an amendment to DIV-0090. **Found, not fixed (the owner's call):** label group 16 (DIV-0064) writes the faerie names one record late - record 0 never named, the "unnamed" record 59 is the area-limit table at `0x6536B0`; the fix is `labels.cpp` + `loc_build.py` and changes the PC path's output. |
| 4 | NB | `disc-only/engine-own` `b59a7b34` | `0668b698`, clean | `tools/exe_own.py` (`measure`, `gen`, `check`, `text_words`) classes every unfilled non-zero number byte of the JP and US images; `src/hook/exe_own.*` holds **17 regions, 20,866 bytes by meaning** (the DAT and SND name pools generated from `recipes/pc-zh*.toml` by the measured layout rule into `exe_own_names.inc`; the platform strings, the initial values, the GTE identity; `Math_SinTable` computed from a sine rule), `ExeOwn_Fill` after the copy in `ExeImage_Source`, verified at every start as `InjectAll` step 2 and under `BOF3X_SHADOW=exe_own`, `exe_own.py check` in CI ([`exe-engine-own.md`](exe-engine-own.md)). Of the 22,331 / 23,013 bytes (JP / US): held 13,942; unread in the exe host 3,856 (the old decoder's data, the C runtime's, the MMX tables); language 1,745 / 1,738 (LG's list); **game values, not copied: 1,622 / 2,135** (`Effect_HpDamageVariance+0xB8` 468, `Magic_Rows+0x4` 300, `Equip_SlotIcons+0x50` 131, `EventBattle_Records+0xE0` 82 the largest - **the owner's call**, section 2); on the disc but not placed 1,026 / 1,202 (CP's places); DIV-0055's plates 94 (no transform; the PSX source unmeasured); the key table 46 (RD's). 319 words of `exe-pointers.tsv` are text inside held regions (sent to CP). No ledger entry: with the PC's `.data` the fill changes no byte. |
| 5 | OP | `disc-only/operands` `3d75d764` | `2389aecd`, one conflict (`exe_host.cpp`: RD's `OpenExe` / `CopyData` kept, the `CodeBytes_Open` call dropped) | every `CodeBytes` read gone from `src/`; each value owned: patched operands by their divergence (`Widescreen_SlideBound` / `_SlideOperand` / `_RangeOperand`, `ConfigText_Operands()`, yes_no_layout's stops and lines, `draw_pool`'s counts, cheats' masks, `TitleMenu_Width`), re-aimed calls by the divergence that re-aims them (`MenuFrame_Draw()`, `ListTitle_SiteDraw()`, ...), the compiler tables as switches in our functions (`Tail32Case`, d3d_list `Entry`, `KindCase`, `VariantCase`, `NibbleBlocks`); the verifications registered and compared hosted only. The hosted check: `Operand` / `OperandExpect` / `OperandCall` / `OperandRule` register 77 values (1,310 bytes; the store held 43 reads, 450 bytes), `Operands_Check` from `InjectReport` and after the late FIRST.DAT / START.DAT patches, `Operands_BeforeJmp` for a site under a jmp of ours (`Menu_YesNo`'s call `0x5747D2`, found by the first run); in the exe host an inventory line, nothing read ([`operand-store.md`](operand-store.md)). `exe-table-audit.md` 7's field_blocked mask read superseded. No ledger entry. |
| 6 | CP | `disc-only/pointers` `23f993df` | `d74a870d`, clean | `recipes/exe-layout.tsv`, one row per pointer word (`pc kind target`; code 8,784 - 8,673 by `[[func]]` name, 111 by address - data 9,959, rdata 63, bss 35, number 471), written by `recipe`, checked by `check`, applied by `disc_built` as one `layout` range after the rebuild (a rebuilt word disagreeing with the table stops the build; none does); the 462 + 7 mid-function words, the `.bss` words and NB's text words classed by rules (`code_numbers`: a pointer only at a `[[func]]` start or an instruction boundary after `ret` / `int3`; `bss_numbers`; `own_numbers`), `exe-pointers.tsv` 19,311 -> 18,840 words; `data.toml` classes `layout` and `own` (NB's regions, over `pointer` and `none`); byte-window keys for the places (JP +9 places, +662 bytes). **Unfilled pointer bytes JP / US: 48,692 / 49,408 -> 0 / 0**; `none` with PC bytes non-zero 22,331 / 23,013 -> 8,071 / 8,732; bytes identical to the PC's 577,657 / 576,417 -> 612,863 / 612,176 of 638,976. Six doubtful `.bss` words held as pointers (24 bytes, doc 7.2); 111 code targets with no `[[func]]` and no stub, read by nothing of ours. `exe_image.cpp`'s `Count by[]` still lacks `layout` / `own` (IN's). No ledger entry. |
| 7 | FM | `disc-only/fmv` `c0968241` (stage B) | clean | the disc's one movie, `LOGO/CAPCOM30.STR` (every PSX disc, 1,155 sectors, byte-identical from the subheader on; 320x240, 231 frames at 30 fps from the sector timing, 7.70 s; XA 37.8 kHz stereo), converted by the player's ffmpeg at `importer.py build` into `base/fmv/CAPCOM.AVI` - Cinepak as DIV-0001's, 640x480 by pixel doubling, 30 fps (`-r 30`: ffmpeg's `psxstr` demuxer stamps 15), PCM 44.1 kHz; the PC's own `capcom.avi` is Cinepak 640x480 15 fps 112 frames, PCM 22.05 kHz; SSIM 0.970 against a direct decode (the PC's re-encode 0.990); about 2 minutes; `install` copies it unless a `capcom.avi` is there; no ffmpeg: a logged skip, recorded for `verify` ([`fmv-import.md`](fmv-import.md), **DIV-0099**). Verified outside the game: 32-bit Video for Windows decodes 231 of 231 frames, MCI with the game's own `open avivideo!` string plays to frame 231. **Not played in the game, not heard.** Found: MCI error 304 on a full path of about 160 characters (the 8.3 form opens; the PC's own movie has the same limit); `Disc_Probe` `0x5A72C0` tests for `CAPCOM.AVI` as "local install" (IN's). |
| 8 | IN | `disc-only/integration` `e75694b1` (stage B) | clean | `recipes/exe-gaps.tsv` (`exe_tables.py gaps`, from `exe_own.py measure` on all seven discs: addresses, classes, symbols) classes every unfilled byte where the PC has a value as `language`, `unread`, `keys` or `gap` (game-value, disc-unplaced, port-relaid, LG's eight as `language-zero`); the importer writes the classes into `data.toml`, so `none` now means zero on the PC too; **`exe_image.cpp` maps a disc-built image** (a `pointer` range or an old cache refused; the engine's regions filled; each gap logged once; `BOF3X_EXEIMAGE_STRICT=1` refuses any gap), `ExeImage_CompareWithFile` when the file is present (a Fatal if `none`, `own`, `keys`, `layout`, `rebuilt` or `exe` differ); `importer.py install --base` lays out `DAT/` (742), `SND/` (880), an empty `BGM/`; a missing movie skipped as the port does, logged once; `Disc_Probe`'s "local install" test (`CAPCOM.AVI`) satisfied by a mapped disc image (`win_main.cpp`, **DIV-0100**); `exe_tables.py gate-skip` writes the gate's list (the committed list + 1,222 ranges, 48,955 bytes; `layout` / `own` / `keys` stay hashed since the map step proves them equal - a departure from the brief, right) ([`disc-only-install.md`](disc-only-install.md)). Measured: the real install with the US cache 627,123 of 638,976 bytes equal to the file, every class that must equal equal; **the scratch install with no `BOF3.exe` under `en-US`: self-test exit 0, 10,088 ours**. US gaps: 456 ranges, 3,358 PC bytes (game-value 2,196, disc-unplaced 818, language-zero 238, port-relaid 106); read, not played: `Magic_Rows` (each effect's file number 0, the wrong effect file - the most visible), the dragon-form set files, the ending roll's steps, the equipment / experience panels' numbers, one mode's per-record words. **The gate's attract pair: identical at tick 0 (with the real install's `BOF3.CFG`), the VRAM shadow and `0x903000` from tick 1, sprites / CLUT strip / area map from tick 1310 - not yet read to the byte** (section 4). |
| 9 | BD | `disc-only/button-defaults` `3e83f7cc` (after the gate reading, 4.1) | clean | the field button map's 18 default bytes at `0x656AEC` (nine pad words: speak = circle + L2 + R2, move = square, menu = triangle, view = R1, change = L1, select, action = cross, confirm = circle + L2 + R2, cancel = cross; two zero bytes) held as an engine rule (`FieldButtonDefaults` in `exe_own.cpp`, each word by `input::Action`; `exe_own.py`'s 18th region; 7 gap rows gone, `exe.toml` updated, the image hashes unchanged). Read: Config Init `0x460FDE..0x46100B` and `LoadMenu_Apply`'s Start-held reset write the same values; the Japanese PlayStation's `Boot_Init` copies nine identical halfwords from `0x8017FEE4` - Capcom's default, not the port's; **the US PlayStation's (`SLUS_004.22`, `0x8014EB24`, from `0x8017F56C`) is a different assignment** (cross speaks and confirms, triangle acts and cancels, circle moves, square opens the menu - the `symbols.toml` save-5 measurement): the PC port took the Japanese one, so a US disc-only install starts with the Japanese buttons - **the owner's call** whether a Western install offers the US assignment (a DIV). Self-tests: exe host with the file and the PC cache (`18 regions, 20886 bytes ... each equal`), the real install with a fresh US `base/exe/` (627,132 of 638,976 equal), the no-exe scratch install (10,088 ours), hosted `exe_own,shell,mode_flow` 0 mismatches. No ledger entry. |
| 10 | LX | `disc-only/language-refusal` `1940675c` (after the batch's invalid gate, 4.2) | clean | a language the install cannot play is refused: `original` needs `BOF3.exe` in the game directory or the cache's `loc\zh-CN` layer (one rule in the launcher's `ConfigOriginalAvailable`, the engine's `RefuseOriginalWithoutChinese` in `DatLoad_Inject`, the tools' `language_tags.original_available`); refused before the dialog with the languages held named, the dialog without "Chinese (PC script)", the default the first language held (`en-US` from a US disc), the engine's Fatal as the last line; a PC install unchanged. **DIV-0102.** Measured against IN's install: `original` and `fr-FR` refused, `en-US` and no language exit 0 (10,088 ours), `bof3x.exe` run directly under `original` exit 3; the real install exit 0 under all three. `attract_run.py` defaults `--lang` to `original`: pass it. |
| 11 | SF | `disc-only/story-flag-seed` `c3e01f40` (after the corrected gate, 4.3) | clean | the 16 bytes `Scenario_SubInit` copies into `Cond_Flags` are on every disc, byte-identical to the PC's, in `ETC/GAME.EMI#0` right after the dispatch table's two entries (JP `0x801C94A4`, US `0x801C84AC`, EU / DE `0x801C8730`, FR `0x801C8734`, the PSPs too): disc data the places missed, not a game value. `find_places` gains the `code_windows` key (16 number bytes between two code-pointer words, tried where no pointer key places a region): `0x662CA4..0x662F24` placed from `GAME.EMI#0` on every PSX build, `0x662DE8..DF0` (`FieldTail_DropInMove`'s table) with it; two `game-value` rows gone from each PSX build (`identical` +4), the PSPs keep both (their `GAME.EMI` uses PSX-range code words; widening the key moved 20 unrelated places - reverted). No ledger entry: the PC's behaviour restored. |

## 4. The validations, batched

### 4.1 The gate's first pair, read (2026-10-12)

IN's attract pair (the scratch disc-only install against the real install with
`analysis/cache/pc-plus-us`, both `en-US`, `BOF3X_OPT=none`, the gate skip
list) differs on 94 pages from tick 1. A read-only agent placed every
differing cache chunk where `LoadDatFile` puts it (images as 32x32 tiles into
the VRAM shadow, kind 0 at `0x803580`+tag) without running the game; each
placement gave exactly the observed page set (`gate-reading.md` in the
session scratchpad). Two disc-only runs are identical but at tick 0 (the
missing `BOF3.CFG`), so every difference is systematic.

| Pages | Ticks | Cause |
|---|---|---|
| VRAM rows 16-22 and 60-71 (9 pages) | 1 to end | `FIRST`'s kanji sheet is the disc's stand-in in the en-US layer - DIV-0097 |
| 28 VRAM pages | 2-1311, 5719-7028 | `DEMO` slot 2, the language page from the US disc - DIV-0089's stand-ins |
| `0x813000`, `0x83A000..0x85F000` | from 1310 | `PL27A`, the disc's own arena - DIV-0096 |
| `0x8C3000` / `0x8C4000`, `0x8D3000..0x8D9000` | from 1310 | `AREA004`'s walls and the 920-byte re-texture - DIV-0080's "Western disc alone" case (the PC reference ran without `area4-walls`; with it only `0x8D5000..0x8D9000` would remain) |
| `0x802000`, `0x92B000`; windows on `0x801000`, `0x905000`, `0x938000` | from 1310; 1313-1613, 7030-7330 | arena pointers and, probably, a draw-count change from the edited scripts - DIV-0096 (by mechanism, not to the byte) |
| `0x904000` | from 1310 | **unexplained**: starts on the `PL27A` load tick; wants dumps around ticks 1305 and 1320 on both sides and `statehash.py bytes` |
| `0x7DE000`, `0x937000` | tick 3 | noise (two reference runs of the same code differ there) |
| **`0x903000`** | **1 to end** | **a defect:** `Boot_Task` (`0x496B60`) copies `0x656AEC..0x656B00` (18 bytes) into the field button map (`Field_MenuButton`, `Field_ConfirmButtons`, `Field_CancelButtons`); the range is a `game-value` gap (`GameMode11_Steps+0xC`), zero in a disc-built image, so a New Game on a disc-only install likely starts with every action unbound (only `Boot_Task`, `LoadMenu_Apply` and the Config screen write the words). **Held as a rule of the engine like the key table - the BD group, `disc-only/button-defaults`, launched 2026-10-12 afternoon.** |

Not exercised by this gate: `START` is not loaded in the attract, so DIV-0096's
`START` arena and DIV-0097's `START` atlas stand-in wait for a route that
boots through it.

### 4.2 The batch (2026-10-12, 10:06-12:09, the build at `5ac7f151`)

One chain (`r/end/batch.sh` in the session scratchpad), from an ini-less copy
of the round's build, `--no-front`, `BOF3X_OPT=none`, `BOF3X_LAYERING=0`.
**The machine was not quiet:** the review's fixes were built and the BD group
built and ran its self-tests beside it, which the chain guards only at each
run's start. The binaries are the pre-review-fix build; the fixes change the
probe's condition and a log line, which no run here reaches.

| Run | Result |
|---|---|
| hosted `BOF3X_SHADOW='*'` narrow | exit 0, 15 min, `inject: 10087 ours`, 0 non-zero `MISMATCHES` lines |
| hosted `'*'` wide (`BOF3X_WIDE=1`) | exit 0, 16 min, 0 mismatches |
| hosted `'*,-field_c1'` under `en-US` | exit 0, 15 min, 10,088 ours, 0 mismatches |
| the exe host: the file; the PC-plus-US cache; the US-only cache | exit 0 each, 10,087 ours |
| **the gate**: the attract on the disc-only install (`gate_us`) against the PC-plus-US cache on the real install (`gate_pcus`), the gate skip list, `en-US` | **349 pages differ from tick 1** (the VRAM shadow from `0x6CA000`, `AreaMap_Corners`' pages from tick 3855, ...): the by-source differences 4.1 read on IN's pair, on a longer run; **two disc-only runs identical on all 11,649 ticks** (`gate_us` / `gate_usb`): the disc-only build is deterministic. A page-by-page reading of this pair against 4.1's categories is owed. |
| the attract against `attract_r14_orig` / `_origb` | 2 pages: `0x7DE000` (the sound page) on 4 ticks, `0x7C9000` (`D3d_ScaleY` / `Font_TexCache`) on 2 ticks at 1561-1562 - the loaded machine's noise of `state-hash.md` 4 |
| `caughFish`, `masterAndManillo`, `shop` against `cn_<route>_orig2` / `_orig2b` | **identical on every tick** (3,841 / 5,505 / 3,137) |
| `combat` | `0x7DE000` on 2 ticks (227, 1233): the sound page's one-tick noise |
| `worldMapAndAreaTransition`, `whelpBoss`, `menu_screens`, `field_menu` | five VRAM-shadow pages, `0x7DE000` and `0x937000` at tick 3 only (an upload landing a frame late: the loaded machine); `menu_screens` / `field_menu` also `0x7DE000` for most of the run - the DIV-0092 staleness `state3-build.md` 5.4 names for these two |
| `dragonTransform`, `cutsceneAndNue` | `0x904000` (`Music_Track`), `0x665000`, `0x6BD000` (`Music_*`), `0x7DE000` from a battle's end - **the four Chinese pairs stale since DIV-0092**, as `state3-build.md` 5.4 found; re-record them (`--original "*,-Game_Clock"`, `BOF3X_OPT=none`) - not this round's |

**The batch's gate pair was not a gate** (read page by page, `gate-reading-2.md`):
`batch.sh` set `BOF3X_LANG=en-US` in the environment, which `attract_run.py`
overrides with its own `--lang` (default `original`), so both sides played
under the port's Chinese layer - which a disc-only cache does not have by
design - and the disc side stalled in area 4's message box (message 8 never
closes); the 349 pages are 81 of absent Chinese chunks, 185 knock-on from
the stall, and the by-source classes of 4.1. **Found by it:** a disc-only
install started under `original` plays with no text and nothing refuses it -
the LX group (`disc-only/language-refusal`): refused loudly in the launcher
and the engine, the install's default its disc's own language.

**The re-run** (the BD build; the LX group built beside it, so not quiet
either): the attract **identical to its pair on all 10,305 ticks**;
`menu_screens` and `field_menu` the DIV-0092 staleness (`Music_*` pages from
tick 752); `combat` and `worldMapAndAreaTransition` the five VRAM pages at
tick 3 only; `whelpBoss` a seven-tick window at 7177-7754 inside the build.
A truly quiet re-run of `combat` and `worldMapAndAreaTransition` is owed
before the tick-3 pages are called noise (4.4).

### 4.3 The gate, corrected (2026-10-12, the BD build, `--lang en-US` as the tool's argument)

`g2_pcus` (the PC-plus-US cache on the real install) against `g2_us` (the
disc-only install), the gate skip list, dumps at ticks 1309 / 1310 / 1312 /
1330 on both sides (`build/end2/` of the round worktree): **97 pages over
11,009 ticks**, read page by page and, where the dumps reach, to the byte
(`gate-reading-3.md`):

| Cause | Pages |
|---|---|
| DIV-0097 (the disc's own images) | 9 |
| DIV-0089 (`DEMO`'s stand-in) | 28 |
| DIV-0096 (`PL27A`'s arena 39; `0x802000` / `0x92B000` to the byte: pointers moved by +0x160, the arena's size) | 41 |
| DIV-0080 (the walls and the re-texture; `0x938000` to the byte: a cell-texture nibble) | 10 |
| probably DIV-0080 (`0x801000`, `0x905000`: a draw count 8 against 6 in the same window; not settled to the source) | 2 |
| **a second gap of the button map's kind:** `0x904000` from tick 1310 is two bytes of `Cond_Flags` (`+0xA0` 0x08, `+0xA3` 0x0A on the PC, zero on the disc side: flags 3, 0x19, 0x1B), written by `Scenario_SubInit` `0x56D8C0` from `0x662CD8..0x662CE8` - a `game-value` gap in every build; the PSX twin's table is cited in `GAME.EMI` (`0x801C94A4`), so the disc may carry it unplaced - **the SF group, `disc-only/story-flag-seed`: placed from the disc (merge 11)** | 1 |
| `0x903000`: the difference from tick 1 is **gone** (the BD fix holds); a 62-tick window at 1545-1607 and 7262-7324 the old difference hid - a dump at about tick 1550 on both sides would settle it | 1 |
| noise (`0x7C1000..0x7C3000`, `0x7DE000`, `0x937000`: two-tick windows at 1311 and 4051, a CLUT-area upload landing a tick apart - the pattern the references show at tick 3) | 5 |

### 4.4 The final chain (the tip after the SF merge)

The US cache rebuilt from the tip (SF's places, FM's movie), its gate skip
list, then with nothing else on the machine: `combat` and
`worldMapAndAreaTransition` against their pairs, and the gate pair once more
with dumps at ticks 1310, 1550 and 1560.

**The cache** (`r/end/cache-us-final`): built in 3 m 34 s with the movie
(`CAPCOM.AVI` from `psx-us`, ffmpeg 9.0.1); `verify` 0 problems but the
pre-existing `opt/area4-walls` `AREA004.DAT` slot 6 line (HANDOFF item 4);
the gate skip list the committed one plus 1,219 ranges, 48,911 bytes (gap
5,411, language 2,904, map 104, unread 37,427, widen 3,065).

**The gate, final** (`g3_pcus` against `g3_us`, `--lang en-US`): **96 pages
over 10,945 ticks** - 4.3's 97 less `0x904000`, which is gone (the SF place
holds); `0x903000`'s 63-tick windows (1545-1607, 7262-7324) are one word,
`Scratch_Swap` `0x90385C`, the area-map draw-table sort's exchange scratch
(`DrawTable_Sort`, `AreaMap_Frame`'s cell rebuild - `symbols.toml`), in the
area-4 scenes where DIV-0080's walls change the cells drawn: the same
region as the two draw-count pages, explained by mechanism. Every page of
the final pair is a ledger entry of this round (DIV-0096, DIV-0097,
DIV-0089, DIV-0080) or the references' own noise pattern; **nothing of the
round's code differs between a disc-built image and the PC-built one.**

**The two routes with the machine quiet:** `combat` and
`worldMapAndAreaTransition` still differ from their Chinese pairs at tick 3
(five VRAM-shadow pages, `0x7DE000`, `0x937000`; `worldMap` again at
331-334) - so this is not the batch's load. The discriminator (combat on
the round's hosted build and its exe host, dumps at ticks 2..4, against the
pair and against each other) - 4.5.

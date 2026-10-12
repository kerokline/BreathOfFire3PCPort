# The `.data` bytes no disc carries: classified, and the port's held as the engine's own

**Status:** MEASURED (2026-10-12, the disc-only round's group NB, branch
`disc-only/engine-own` off `6e6b4f3b`; the owner's machine: `BOF3.exe`, the JP
and US discs, headless self-tests only)

The disc-only round ([`disc-only-round.md`](disc-only-round.md)) needs a
disc-built image of `BOF3.exe`'s `.data` the engine can map. After the
data-pointer rebuild and the places ([`exe-import-engine.md`](exe-import-engine.md)
3.1) a JP-built image still leaves **22,331 non-zero number bytes** no disc
carries (`none_pc_nonzero`), the US-built **23,013**. The owner's call
(2026-10-11): the ~22 KB no disc carries as the engine's own. This document is
the measurement of what those bytes are, and what of them the engine now holds.
No table value appears here (CLAUDE.md rule 1): addresses, sizes, names and
counts; the per-region lists are `tools/exe_own.py measure`'s output, under
`analysis/`.

## 0. What came out

1. **A measurement**, `tools/exe_own.py measure` (section 1): every unfilled
   region holding non-zero number bytes, split at `symbols.toml`'s `[[data]]`
   symbols, with its size, its readers (`src/`'s names and literals,
   Capcom's instructions by `analysis/pc_xref.json`, the pointer words into it),
   whether its bytes are on the disc at all, and a class, byte by byte.
2. **The port's platform bytes held by meaning**, `src/hook/exe_own.cpp`
   (section 2): 17 regions, 20,866 bytes of `.data` - **13,942 of the JP
   build's 22,331 non-zero bytes no disc carries** (and the same 13,942 of the
   US build's 23,013), plus 319 words `recipes/exe-pointers.tsv` calls pointers
   that are four characters of a held string. The file layer's 742 DAT and 891
   SND names are generated from `recipes/pc-zh.toml` and `pc-zh.snd.toml`
   (`src/hook/exe_own_names.inc`, `tools/exe_own.py gen`); the rest are the
   strings, initial values and two computed tables they are.
3. **Verified at every start**, in both hosts: each region filled into a
   scratch copy and compared with the running `.data` (the PC's), a Fatal on
   a difference; with `BOF3X_EXEIMAGE` a pc-zh image is checked before the
   fill is laid over it. `BOF3X_SHADOW=exe_own` adds the self-test: a fill into
   a zeroed image writes exactly the regions' bytes and nothing else.
4. **What is left, and whose it is** (section 4): JP 1,745 bytes of language
   (LG's), 1,622 of Capcom game values no disc carries in any shape found
   (reported, not copied), 1,026 that **are** on the disc and the places miss
   (CP's), 94 re-laid by the port (DIV-0055's plates, a transform), 46 of the
   default key table (RD's), and 3,856 that nothing the exe host runs reads.

## 1. The method (`tools/exe_own.py measure`)

    python tools/exe_own.py measure --game <dir with BOF3.exe> --disc <JP cue> --disc <US cue> --out <dir under analysis/ or outside the repo>

For each disc: the image as `base/exe/` holds it (`exe_tables.disc_built`:
the maps' segments, the places, the tables, the pointer mask, the rebuilt
data pointers - this round's base, `6e6b4f3b`). A byte is a **number byte no
disc carries** when nothing wrote it, no word of `exe-pointers.tsv` covers it,
and the PC's byte is non-zero - `exe-import-engine.md` 3.1's `none_pc_nonzero`,
the same 22,331 on JP.

- **Regions**: every maximal unfilled span, cut at each `[[data]]` symbol
  inside it; those with a non-zero number byte are listed (225 on JP, 254 on
  US), each named by the symbol at or below it.
- **Readers**: the `[[data]]` names and six-digit literals `src/` uses whose
  extent meets the region (`exe_tables.src_addresses`); every instruction of
  Capcom's `pc_xref.json` gives an operand from the region's symbol to its end,
  by the `[[func]]` at or below it; the `.data` pointer words of the PC whose
  value lands in the region.
- **On the disc**: every 16-byte window of the region's pointer-free stretches
  with six or more non-zero bytes, searched verbatim in every source the twin
  survey searches (`exe_twins.sources`: the boot EXE and every type-0 EMI
  section). Any window found: the bytes are the disc's, unplaced.
- **The class**, byte by byte: held by `exe_own.cpp`; else a row of the tool's
  `BYTES` (a few bytes inside a platform region that are something else); else
  the `REGIONS` row holding the byte (each with its reason, from the readers
  above and the doc section it cites); else by the region's symbol's name
  (`NAME_RULES`); else `unread` when nothing reads it, `disc-unplaced` when a
  window is on the disc, `game-value` otherwise.

The classes:

| Class | What | Whose |
|---|---|---|
| `held` | the port's platform bytes and two computed tables, in `exe_own.cpp` | this group's (section 2) |
| `language` | text, labels, formats and glyph tables a language layer carries | LG's |
| `game-value` | a Capcom table of game numbers no disc carries in any shape found | reported, not copied (rule 1) |
| `disc-unplaced` | game data whose bytes are on the disc verbatim, which the places miss | CP's (`find_places`) |
| `port-relaid` | the plates DIV-0055 re-laid: on the disc in another shape | a transform (the coordinator's) |
| `rule (RD)` | `Key_TableDefault` | RD's (a rule of the engine and the launcher) |
| `unread` | nothing the exe host runs reads it | nobody |

## 2. What the engine holds (`src/hook/exe_own.cpp`)

| Region | What | bytes | non-zero | text words | JP's unfilled non-zero |
|---|---|---:|---:|---:|---:|
| `0x64FFE4..0x652894` | `Dat_FileNames`' 742 names (`recipes/pc-zh.toml`) | 10,416 | 7,520 | 312 | 7,520 |
| `0x652894..0x6528A9` | the DAT directory format and the save files' pattern | 21 | 18 | 0 | 18 |
| `0x656AEC..0x656B00` | the field button map's defaults `Boot_Task` copies (a rule: [`controls.md`](controls.md) 2.1; added 2026-10-12) | 20 | 9 | 0 | 2 |
| `0x65DA44..0x65DA50` | `Cfg_Fullscreen`, `Cfg_RenderMode`, `Cursor_Hidden`: 1 | 12 | 3 | 0 | 3 |
| `0x65DA54..0x65DA76` | WinMain's messages and the logo AVI's name | 34 | 25 | 2 | 25 |
| `0x65DA84..0x65DA96` | the window class and the icon's name | 18 | 13 | 0 | 13 |
| `0x65DAB8..0x65DAE7` | the Capcom AVI, the exe's name, `BOF3.CFG`, its format and mode | 47 | 33 | 0 | 33 |
| `0x664068..0x664077` | the save file's name format | 15 | 14 | 0 | 14 |
| `0x6653BC..0x6653C0` | `Music_LoadedTrack`: none (-1) | 4 | 4 | 0 | 4 |
| `0x6653C0..0x666F9C` | `Sound_LoadStream`'s 891 SND names (`recipes/pc-zh.snd.toml`) | 7,132 | 5,341 | 3 | 5,341 |
| `0x666F9C..0x666FC5` | the SND and BGM file formats | 41 | 35 | 0 | 35 |
| `0x66B5D8..0x66B6A7` | the AVI player's MCI commands | 207 | 183 | 0 | 183 |
| `0x66B708..0x66B71C` | `Gfx_ScreenRect` and the display mode's none (-1) | 20 | 8 | 0 | 8 |
| `0x66B720..0x66B724` | `D3d_AlphaOpCache`: 1 | 4 | 1 | 0 | 1 |
| `0x66B928..0x66BC3F` | the DirectX error messages, the caption, the file layer's formats | 791 | 708 | 2 | 708 |
| `0x66BC40..0x66BC58` | `Gpu_OtTerminator` and `Gte_IdentityRotation` | 24 | 7 | 0 | 7 |
| `0x66BC58..0x66C45A` | `Math_SinTable` (a rule) | 2,050 | 1,996 | 0 | 0 |
| `0x66C7B0..0x66C7CE` | the pad `DInput_EnumJoystick` looks for by product name | 30 | 29 | 0 | 29 |
| total | 18 regions | 20,886 | 15,947 | 319 | 13,944 |

"Text words": words inside the region that `exe-pointers.tsv` calls pointers -
four characters of a string that happen to read as an address in the image
and that no disc places, so the pointer decision kept them (`exe_own.py
text_words` lists all 319). They are numbers; the fill writes them, and the
verification proves them equal to the PC's.

The button map's defaults (added after this group, 2026-10-12) are a game
rule rather than the port's platform bytes: the JP disc places 16 of their
20 bytes, the Western discs none (their boot EXEs hold a different
assignment), so a Western disc-only image had zeros there. Held by button
name, not copied ([`controls.md`](controls.md) 2.1).

`Math_SinTable` is placed from the JP disc's boot EXE already (0 unfilled), but
held anyway: it is a rule (`round(4096 sin(2 pi i / 4096))`, i = 0..1024,
exact on all 1,025 entries), so the image needs no disc for it.

### 2.1 How each is defined

- **The two name pools** are generated: `tools/exe_own.py gen` writes
  `src/hook/exe_own_names.inc` from the recipes' file lists - the 742 DAT
  containers in `pc-zh.toml`'s (sorted) order, the SND clips by
  `Sound_LoadStream` kind (0: 11, 1: 880, 2: 5) and id in `pc-zh.snd.toml`
  (its two rows of kind -1, `019_02` and `dir1`, are files no stream names);
  `exe_own.py check` (CI) fails when the include is not what the recipes give.
- **The layout rule**, measured on every region here (a session model over the
  PC's bytes, then the start-up verification): the port's compiler put each
  string literal at a 4-aligned address, its NUL and zero padding up to the
  next; a table's literals are laid **downward** from the first byte after the
  pool, in the table's order, a name the table repeats laid once (the SND
  tables repeat five: `063_02`..`063_06`). The DAT pool ends at `0x64FFE4`, the
  first byte after the 799-word pointer table; the SND pool at `0x6653C0`,
  after `Music_LoadedTrack`.
- **Strings** are the literals (`Strings({...})`), **initial values** the words
  (`Words({...})`), each with a comment naming its reader; the GTE identity
  is `ONE` on the diagonal; the sine table is computed.

The pointer words of those tables (`Dat_FileNames`' 799, the SND tables' 896,
`Display_ErrorBox`'s 23, the three stream-table words at `0x6653B0`) are CP's;
they are layout (addresses), and the pool rule above gives every one of them
(name *i*'s address) should CP want a check.

### 2.2 Where it runs

- `ExeOwn_Fill(data_base)`: writes every region over an image whose first byte
  is `0x5DA000`; nothing outside the regions changes.
- `ExeOwn_Verify(pc_data, against)`: fills a scratch copy of `pc_data` and
  compares each region with it; a Fatal names the region and the first
  differing address.
- **`InjectAll`'s second step** (`ExeOwn_CheckAtStart`, after `rdata::Verify`),
  every start in both hosts: `.data` is still the PC's there - the file's in
  the exe host, the loader's hosted, or a pc-zh cache already proven equal -
  so the regions are verified against it. With `BOF3X_SHADOW=exe_own`, the
  self-test too.
- **`ExeImage_Source`** (the exe host with `BOF3X_EXEIMAGE`): after the copy,
  the pc-zh image's bytes verify the regions, then `ExeOwn_Fill` lays them
  (changing nothing over a pc-zh image). That is the call a disc-built image
  needs, already in its place.

**Keyed on**, until RD's predicate exists: the running `.data` in `InjectAll`
(always the PC's in this tree, since a disc-built image is refused) and the
pc-zh `data.bin` in `ExeImage_Source`. When IN lets a disc-built image be
mapped, the running `.data` is the disc's plus the fill, so the start-up check
must compare against the file's `.data` when the file is present (RD's
`ExeFile_Present()` or its like) - against the running image it would compare
the fill with itself.

## 3. The table

Bytes are non-zero number bytes no disc carries, by the class of each byte
(regions: how many of the 225 / 254 hold some):

| Class | JP bytes | JP regions | US bytes | US regions |
|---|---:|---:|---:|---:|
| held (`exe_own.cpp`) | **13,942** | 13 | **13,942** | 13 |
| unread | 3,856 | 7 | 3,856 | 7 |
| language | 1,745 | 22 | 1,738 | 29 |
| game-value | 1,622 | 123 | 2,135 | 139 |
| disc-unplaced | 1,026 | 50 | 1,202 | 56 |
| port-relaid | 94 | 16 | 94 | 16 |
| rule (RD) | 46 | 1 | 46 | 1 |
| total | 22,331 | 225 | 23,013 | 254 |

Left after this group: JP **8,389** (of which 3,856 unread need nothing), US
**9,071**. The bytes a running disc-only image still lacks, once LG, CP and RD
deliver theirs, are the game values (JP 1,622, US 2,135) and the re-laid
plates (94).

## 4. What is left, and for whom

### 4.1 `unread` (3,856, both builds): nothing

- `0x66C858..0x675A00` past `Task_StackTop` (3,557): two 480-word tables
  (`0x66C8B8..0x66D7B8`) read only by the unnamed MMX routines after
  `Gfx_MoveCells` (`0x5AA929`.., 32 operands found by a dword scan of the file;
  `pc_xref.json` does not index that code), the original MP3 decoder's data
  (Xaudio's strings and tables, read only by `Mp3_*` - DIV-0091 replaced the
  decoder) and the C runtime's (ctype, locale, stdio, read by `Crt_GetPtd` and
  the CRT routines our toolchain's replace). No `[[func]]` with an `impl`
  reads any of them; the exe host never runs that code. The `crt_rest` fuzz
  (hosted only) reads `0x672854` and the `_pctype` table in the PC's own
  `.data`, never in a disc-built image.
- `0x66C45C..0x66C648` and `0x66C6C8..0x66C7B0` (136): a table of
  (DirectInput scan code, key name) and its names; no instruction and no
  word of the exe's file (a scan of every dword of `BOF3.exe`) names the table.
- `0x66B724..0x66B928` (160): sixteen records of three words after
  `D3d_AlphaOpCache`; no instruction and no word of the file names them.
- Three single dwords: `0x65DA50`, `0x66B71C` (initialised 1, unnamed),
  `Area128_Handlers+0xBC` (1 byte).

### 4.2 `language` (LG's): JP 1,745 bytes, US 1,738

Not filled. By region (JP; the US adds seven pieces of
`Area175_ScriptMessages`):

| Region | Bytes | What |
|---|---:|---|
| `0x6531F4..0x6536C0` (`CommuRank_Modes`+0x80) | 516 | the page-format and glyph tables `rest_4e.cpp` names |
| `0x669FA4..0x66A228` (`CommuName_RecordNames`) | 379 | the community's member names |
| `0x669D60..0x669FA4` (`Battle_CommandLabelPointers`) | 243 | the battle command labels' text |
| `0x66A228..0x66A3C8` (`Menu_Verbs`) | 155 | the menu verbs |
| `0x6536F8..0x65382D` (`ConfigScreen_OpenStates`+0x24) | 116 | the config screen's labels in the port's code page, and its key letters |
| `0x6317B8..0x63186F` (`Area144_Choices`+0x54) | 83 | an area's choice text |
| `0x669CCC..0x669D28` (`Encounter_SlotChance`+0x1C) | 46 | the unnamed text after it |
| `0x66A3C8..0x66A418` (`Magic080_TextPointers`) | 44 | |
| `0x66A418..0x66A448` (`Pause_LinesGame`) | 34 | the pause lines |
| `0x65DA78`, `0x65DA98..0x65DAB8` | 32 | the window title and `Game_WinMain`'s two message-box texts, in GBK |
| `0x669D28..0x669D60` (`Battle_CommandLabels`) | 28 | |
| 11 more (area messages and choices, `Area08_MessageFormat`, `Area75_CounterFormat`, `NameTable_Abilities`+0x1538) | 69 | 1..15 bytes each |

Two of these are format strings rather than text (`Area08_MessageFormat` `%d`,
`Area75_CounterFormat`): the engine's own by the brief's definition, left to LG
with their regions because their symbols are an area's message records. The
three GBK strings leave the window title and two message boxes empty in a
disc-only image until a language layer gives them.

### 4.3 `game-value`: JP 1,622 bytes, US 2,135 - reported, not copied

Capcom's game numbers that no disc carries in any 16-byte window the search
found - mostly small numbers between the code-pointer words of the PC's state
and step tables, which on the PlayStation are likely immediates in the
overlay's code rather than data (not measured: the owning-function method,
`exe-import-engine.md` 3.3, is the way to find them). Copying them would be a
copy of Capcom's values (CLAUDE.md rule 1), so they stop here. The largest
(JP; address, non-zero bytes):

| Region | Bytes |
|---|---:|
| `0x64EA64..0x64EC99` `Effect_HpDamageVariance`+0xB8 (halfword tables after eight pointers; readers by the symbol: `Kaiser_LoadSetFile`, `Accession_LoadForm*`) | 468 |
| `0x64C2BC..0x64C770` `Magic_Rows`+0x4 (ids between the rows' code pointers) | 300 |
| `0x6636B0..0x6637C7` `Equip_SlotIcons`+0x50 | 131 |
| `0x64DECC..0x64DF6F` `EventBattle_Records`+0xE0 (past its 56 records) | 82 |
| `0x663970..0x6639D8` `Menu_ButtonSets`+0x134 | 28 |
| the rest: 34 effect state tables' number bytes, 38 area tables, seven scenario and three boss tables, menus and windows | 613 |

By family (JP): other 793, `Magic*` 312, `EffectKind*` 214, `Area*` 180,
menus and windows 52, `Boss*` 20, `Scena*` 16, `Commu*` 16, `WorldMap*` 11,
`Battle*` 8. The US adds `Scena*` 485 (16 regions) and a few more.

### 4.4 `disc-unplaced` (CP's): JP 1,026 bytes, US 1,202

The bytes are on the disc verbatim; the places (`exe_tables.find_places`) did
not reach them, mostly for want of a pointer key. Largest (JP; windows found
of tried): `Area169_Rects`+0xA 96 (20 / 21), `Area110_Weights`+0x50 91
(16 / 17), `Area117_SpawnKinds2`+0xC 86 (13 / 15), `Area115_Sprites` 84
(10 / 10), `Area106_Cells`+0xB8 60, `Area143_EffectStates` 57,
`Area130_EffectArgsC`+0xC 44, `EffectKind0F_Child5Steps`+0xFC 24,
`TornadoMote_Phases` 22; by family `Area*` 678, `EffectKind*` 180, `Battle*`
40, `Boss*` 37, other 91. A byte search per region (the twin method on one
region at a time) places most of these.

### 4.5 `port-relaid` (94 bytes, both builds): a transform

The ten world maps' `AreaNN_DriftUV` (u base, v base, cell size: the plates'
place on the page the port repainted), `AreaNN_Record8Anims` and
`Area104_PlateAnims` - 16 regions. None is on either disc verbatim (searched in
every EMI section and the boot EXE). The PSX source is the world-map overlay's
plate-drawing code (the twin of `Area16_DrawDrift`), where the disc's own
plates' coordinates are presumably immediates - not measured. No transform
exists; with DIV-0055 the page and its plates are the disc's, so a disc-only
install wants the disc's coordinates, which only that code gives. **For the
coordinator.**

### 4.6 `rule (RD)`: `Key_TableDefault`

64 halfwords, `0x66C648..0x66C6C8` (46 non-zero number bytes outside the
pointer-shaped words). Not filled here. The key names after it are unread
(4.1); the pad's product name after them is held (section 2).

## 5. For the other groups

- **CP** (`exe-pointers.tsv`, `data.toml`'s classes): the 319 text words
  (`python tools/exe_own.py text_words`) are numbers - leave them out of the
  pointer layout; the engine writes them. The regions are `ExeOwn_Regions()`
  in C++ and `exe_own.own_regions()` in Python (the `// regions:` block of
  `exe_own.cpp`, `{lo, hi, "what"}`): `data.toml` can class `[lo, hi)` as
  `own`, taking precedence over `none` and `pointer` (over `map` it writes the
  same bytes). Section 4.4's regions are the places' to find.
- **IN**: `ExeImage_Source` already calls `ExeOwn_Fill` after the copy; when a
  disc-built image is accepted, `ExeOwn_CheckAtStart` must verify against the
  file (section 2.2).
- **RD**: `Key_TableDefault` is 128 bytes; nothing of this group's touches it.
- **LG**: section 4.2's list.

## 6. Commands

```
python tools/exe_own.py measure --game <dir> --disc <JP cue> --disc <US cue> --out <dir>    # ~100 s; <bid>.own.tsv, summary.json
python tools/exe_own.py gen          # src/hook/exe_own_names.inc from the recipes
python tools/exe_own.py check        # CI: the include, the regions' order and classes, the key table untouched
python tools/exe_own.py text_words   # the 319 words exe-pointers.tsv calls pointers inside the held strings

# a copy of build/bof3x-launcher.exe, bof3x.dll, bof3x.exe in a directory with no bof3x.ini:
BOF3X_SELFTEST_ONLY=1 ./bof3x-launcher.exe --game <dir> --no-config                                    # exe host
BOF3X_SELFTEST_ONLY=1 BOF3X_EXEIMAGE='<pc-built cache>' ./bof3x-launcher.exe --game <dir> --no-config  # exe host, pc-zh image
BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=exe_own,field_blocked ./bof3x-launcher.exe --game <dir> --no-config   # hosted (d3d_draw: d3d_list's)
```

Results (2026-10-12), each exit 0 with `self-test only: done` and
`inject: 10087 ours, 0 left original by BOF3X_ORIGINAL`:

| Run | `exe_own` lines |
|---|---|
| exe host | `exe_own: 17 regions, 20866 bytes of .data held by the engine (...), each equal to BOF3.exe's .data` |
| exe host, `BOF3X_EXEIMAGE` = `analysis/cache/pc-plus-us` (pc-zh) | the same against `base/exe/data.bin (pc-zh)`, then against `.data` |
| hosted, `BOF3X_SHADOW=d3d_list,field_blocked,labels,exe_own`, then `d3d_draw` | the same, and `exe_own self-test: a fill into a zeroed image writes 16895 non-zero bytes in 17 regions, each the PC's, nothing outside them`; `d3d_draw` (which runs `d3d_list`'s) and `field_blocked` 0 mismatches |

The brief's `d3d_list` and `labels` are not shadow names: `d3d_list`'s
self-test runs under `d3d_draw`, and `labels` has none (its check runs at
every start).

## 7. Ledger

None. The fill writes the PC's bytes, verified at every start; with
`BOF3.exe`'s `.data` or a pc-zh image it changes no byte, so no behaviour
diverges. What a disc-only image does without the language and game-value
bytes of section 4 is stage B's (IN) to measure and ledger.

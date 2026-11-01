# The placed `.data` words into the middle of a function

**Status:** STABLE (verified 2026-10-10, on the owner's machine: `BOF3.exe`
and the seven held discs, read only; nothing run)

[`exe-import-engine.md`](exe-import-engine.md) section 5 item 3, the read that
section 4.2 (b) needs before the entry-stub page is written: the stub page
plants `jmp ours` at every `symbols.toml` `[[func]]` start, so a `.data` code
pointer whose target is *inside* a function gets no stub. Each such word is
either a function start the catalogue misses (a `[[func]]` to add, and a
takeover gap if no code of ours implements it), a label our implementation of
the enclosing function makes unnecessary, or not a pointer at all.

**Result: all seven are numbers, not pointers.** None is a missing start,
none a label; the stub page needs nothing for them, and no takeover gap was
found. What they do show is that `recipes/exe-pointers.tsv` holds at least
seven numbers it calls pointers (section 3).

## 1. The count, re-derived

`python tools/exe_midfunc_pointers.py` (new; the session scripts behind
section 4.1, `codeptr.py` / `codeptr2.py`, are gone). It takes every word of
`recipes/exe-pointers.tsv`'s `text` runs, reads its value from `BOF3.exe`,
and compares it with the `[[func]]` starts; "a disc places it" is a byte of the
word inside any build's `exe_maps/<build>.tsv` segment, `recipes/exe-places.tsv`
place, or a catalogued table's span with a place in that build (the ranges of
`exe_tables.disc_image`, built from the recipe files alone).

| | words | targets |
|---|---:|---:|
| `.data` words into `.text` | 9,142 | 6,346 |
| at a `[[func]]` start (every one with `impl`) | 8,673 | 6,211 |
| inside a function | 469 | 135 |
| of those, a disc places the word | **7** | 7 |
| of those, no disc places it | 462 | |
| `.rdata` words into `.text` | 127 | 81, **none a `[[func]]` start** |

Section 4.1's numbers, every one, by an independent script. All seven words
are placed by an `exe_maps/` segment on every build; none by an
`exe-places.tsv` place.

## 2. The seven

The target is the word's value. "Boundary" is whether a linear capstone sweep
from the enclosing function's start lands on the target. "Builds that differ"
is what made `exe_tables.decide_pointers` call the word a pointer (a placed
word some build holds differently from the PC); `--disc` prints it.

| Word | Its `[[data]]` (offset) | Target | Enclosing `[[func]]` (impl) | Boundary | Builds that differ | What the word is | Class |
|---|---|---|---|---|---|---|---|
| `0x5EB748` | `Area25_Handlers` +0xA74 | `0x424140` | `Area151_DrawFrame` `0x424010` +0x130 (`area_w3g.cpp`) | no (inside `mov edx, [0x90358C]` at `0x42413C`) | PSP-JP, PSP-EU (a `BOOT.BIN` segment from `0x5EB72C`) | the second half of an 8-byte record that the pointer `0x5EC144` (in a run `0x5EC0E8..` of pointers into this area's records) addresses: three ascending byte indices and a zero; the next three records at `0x5EB74C`.. have the same shape with the next indices, and those are numbers to the recipe (every disc equal). Area 26's data pointing mid-instruction into area 151's draw routine has no reading as code | **not a pointer** |
| `0x6538B8` | `Title_Sprites` +0x68 | `0x500080` | `EffectKind18Sub0E_Open` `0x500040` +0x40 (`effect_5b.cpp`) | yes, by chance (no branch targets it) | PSP-JP, PSP-EU (a `BOOT.BIN` segment from `0x6538A5`) | entry 10 (`10 x 10` + 4) of the title's sprite table: its `w` and `h` halfwords, read by `Title_Sprite` `0x462560` (ours, `mode_tasks.cpp`: `Word(entry + 4)`, `Word(entry + 6)`) | **not a pointer** |
| `0x654878` | `EffectKind5B_States` +0x2C (the symbol's extent runs past its three entries into kind 0x5D/0x5E's layout words) | `0x500060` | `EffectKind18Sub0E_Open` `0x500040` +0x20 (`effect_5b.cpp`) | yes, by chance (the fall-through of `jg` at `0x50005E`) | the four Western PSX builds (a `SCENA10.EMI#0` segment from `0x654876`) | the list box's `x`, `y` halfwords (`kListBox` in `src/game/rest_3e_callees.h`), read by `EffectKind5E_DrawList` `0x47FAF0` (ours, `rest_3e.cpp`) at `0x47FB14` / `0x47FB23` | **not a pointer** |
| `0x65C380` | `MeteorStrikeChip_Lifts` +0xC | `0x500000` | `EffectKind18Sub0E_Wait` `0x4FFF80` +0x80 (`effect_5b.cpp`) | no (inside `push 0x200` at `0x4FFFFF`) | every build (a `BATTLE.EMI#15` segment from `0x65C37F`) | the fourth of the four lift dwords read `[+0xB & 3]` by `MeteorStrikeChip_Launch` `0x4FA6C0` and `_Rise` `0x4FA790` (ours, `magic_s38.cpp` `kChipLifts`). The PSX twin of the table, `MAGIC226.EMI` section 3 (US `0x801F0C68`, after `_Shades` / `_Fades` which the map places from there), holds the PC's four values; the disc word that differs is the `BATTLE.EMI` segment's, a misplacement | **not a pointer** |
| `0x6628A8` | `Scena17_EndSteps` +0x7DC (past its three entries: the roll's strings) | `0x4F544F` | `AuraBreathDome_Run` `0x4F5420` +0x2F (`magic_s36.cpp`) | no (inside the tail `jmp` at `0x4F544D`) | PSP-JP, PSP-EU (a `BOOT.BIN` segment from `0x6628A7`) | text: bytes `+0x10..` of the credits string `Scena17_RollLines` points at from `0x661C84` (`0x662898`), drawn by `Scena17_ScrollRoll` `0x56D110` (ours, `scena_sc15.cpp`) | **not a pointer** |
| `0x66AE1C` | `MsgBox_PlacementTable` +0xC | `0x560091` | `Scena12_Run6` `0x55FD70` +0x321 (`scena_sc12.cpp`) | no (inside `mov byte [0x903849], 5` at `0x56008C`) | PSP-JP, PSP-EU (a `BOOT.BIN` segment from `0x66ADF7`) | entry 3's `x`, `y` halfwords of the 0x0C control code's box positions, read `[index * 4]` by `Window_Grow` `0x595670`, `Window_Shrink` `0x595860`, `Window_Kind2Open` `0x595B80` (ours, `window_task.cpp` `kPlaceXY`) | **not a pointer** |
| `0x66AE20` | `MsgBox_PlacementTable` +0x10 | `0x5600E2` | `Scena12_Run6` `0x55FD70` +0x372 | no (inside `push 0xBC` at `0x5600DF`) | as `0x66AE1C` | entry 4, the same | **not a pointer** |

Every enclosing function has `impl`, and so does every reader named. No
`call` / `jmp` / `jcc` (rel8 or rel32, a byte-pattern search over all of
`.text`, a superset of the decoded ones) targets any of the seven; the value
appears as an `imm32` only for `0x500000` (8 sites) and `0x500080` (2), all
coordinates or heights (`ChangeArea` positions and the like), never a pushed
or stored handler. Five of the seven are not instruction boundaries at all.

## 3. What follows

1. **The stub page needs nothing for these seven.** Section 4.2 (b)'s list
   loses the item: the 17 functions without `impl`, then the page.
2. **`recipes/exe-pointers.tsv` calls seven numbers pointers.** Each was
   decided a pointer because one build's placed word differs from the PC's -
   and five of the six placing segments that differ start off a 4-byte
   boundary (`0x65C37F`, `0x6538A5`, `0x6628A7`, `0x66ADF7`, and the
   Western `0x654876`; the sixth, PSP `0x5EB72C`, is aligned but 0x21 bytes
   long), the shape of a short search hit, not a twin; for `0x65C380` the
   real twin was read and holds the PC's value. Effect
   today: a disc-built `base/exe/` leaves these 28 bytes unfilled where the
   disc's twin (or the PC's value) belongs; a PC-built one copies them, so the
   running game is unaffected. A cheap tightening of `decide_pointers`: a word
   into `.text` that is not a `[[func]]` start is a number (here: 7 placed
   words, and an upper bound on the 462 unplaced). Not done here: it changes
   the recipe, whose regeneration needs the discs (`exe_tables.py recipe`) and
   moves `recipes/exe.toml`'s counts.
3. **A pointer word hides a map difference from `measure`.** `exe-import.md`
   section 3's `differ_map` list does not show `MeteorStrikeChip_Lifts` or the
   other six, because a pointer word is masked before bytes are compared. The
   `BATTLE.EMI#15` segment from `0x65C37F` (every build; US to `0x65C4C5`) is
   misplaced at least over its first bytes; the bytes it places beyond this
   word were not checked.

## 4. Commands

```
python tools/exe_midfunc_pointers.py                       # sections 1 and 2's first columns (seconds; no disc)
python tools/exe_midfunc_pointers.py --all                 # also every unplaced word
D="--disc <JP cue> --disc <US cue> --disc <EU cue> --disc <FR cue> --disc <DE cue> --disc <PSP-JP iso> --disc <PSP-EU iso>"
python tools/exe_midfunc_pointers.py $D                    # "builds that differ" and the placing segment (~3 min)
python tools/pe_disasm.py 0x424010 0x4FFF80 0x500040 0x4F5420 0x55FD70   # the enclosing functions
```

The boundary column, the readers (`analysis/pc_xref.json` for
`0x654878`, `0x65C374`, `0x66AE10`; a `.data` scan for the pointers to
`0x5EB744` and `0x662898`), the branch and `imm32` search and the
`MAGIC226.EMI` twin read were session
scripts over the same inputs (`exe_twins.sources` for the disc file).

## 5. Not verified

- The reader of area 26's record run (`0x5EC0E8..`, reached from `0x5EC5B4`):
  no code names those addresses directly; it is read through the area's data,
  and the record's byte layout is inferred from its neighbours, not from the
  reading code.
- ~~Whether the Western PSX list box value (`0x654878`) is a regional layout
  change or a misplaced segment: the US segment there starts at `0x654876`
  but maps the word to the same disc address as JP's.~~ **Answered
  2026-11-01** (DIV-0198, D400): a regional layout change - the US disc's
  box is (96, 96, 128, 128) against Japan's (96, 80, 128, 144), paired with
  a changed margin in the US twin of `EffectKind5E_DrawList` (`+4` for
  `+0x14`); the map's `agree 138` for the run is wrong by those two words.
- The 462 words no disc places (312 into `Scena03_Scene7`, 98 into
  `Gfx_PackRgb`, 10 into `Crt_GetPtd`, then 35 functions with 1..3 each): not
  read, as section 4.1 says. If any were a real code pointer it would need a
  stub; the `decide_pointers` tightening of section 3 item 2 would settle them
  as numbers only by rule, not by reading.
- The 127 `.rdata` words into `.text`: only counted (no target a `[[func]]`
  start), not read.

## 6. The 462 words no disc places, read (2026-10-12)

Section 5's third item, settled by the disc-only round
([`exe-import-engine.md`](exe-import-engine.md) section 7.2):
`exe_tables.code_numbers` sweeps `.text` from the `[[func]]` start below each
target and calls a word into the middle of a function a pointer only when its
target is an instruction boundary right after a `ret` or `int3` - a function
start the catalogue lacks. Of the 469 (the 7 above and the 462):

| | words | targets | what |
|---|---:|---:|---|
| pointers: a start after `ret` | 111 | 90 | `0x5DA00C`, `0x5DA010`, `0x5DA01C` (the C runtime's, at the head of `.data`); 98 into `0x5AA7D3..0x5AC9F5` (a jump table of the blitters after `Gfx_PackRgb`, which is 0x37 bytes); 10 into the C runtime after `Crt_GetPtd` / `Rand` (`0x5B93F4`, `0x5BE78C`, `0x5BFFF5` x6, ...) |
| numbers: not an instruction boundary | 349 | | the 312 `DAT\0` of `Dat_FileNames`; key names (`END`, `TAB`, `IFT`); `PST` / `PDT`; `C:\`; text and halfword pairs in area tables; five of the seven above |
| numbers: a boundary inside a function | 9 | | two small halfwords (`0x500080`, `0x500060`, `0x500058`, `0x580057`, `0x4FFFFF`, `0x50FF80`) and the text `DEL`, `INS`, `LID`; two of the seven above |

The 111 are held by address in `recipes/exe-layout.tsv` (its `code` rows
whose target is no `[[func]]` name; `exe.toml`'s `layout_code_unnamed`); the
358 are its `number` rows and left `exe-pointers.tsv`. The boundary sweep
and the `ret` test were cross-read against the disassembly at seven of the
targets (`0x5BFFF5`, `0x5BE78C`, `0x5B93F4`, `0x5B940B`, `0x5BCB17`,
`0x5C17A6`: each follows a `ret`; `0x5BA2E8`: inside a code-page table of
the C runtime, a number).

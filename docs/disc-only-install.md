# Playing from a PlayStation disc alone: the disc-only install

**Status:** MEASURED (2026-10-12, the disc-only round's group IN, branch
`disc-only/integration`; headless self-tests and the attract sequence from a
US disc alone; **not played by hand** - the owner's)

The steps for a player who has a US PlayStation disc of *Breath of Fire III*
and none of the 2001 PC port's files, and what that install does and does not
have. The engine side is [`exe-import-engine.md`](exe-import-engine.md) 4.4;
the round is [`disc-only-round.md`](disc-only-round.md).

## 1. The steps

You need: the disc as a `.cue` + `.bin` image, Python 3.11 or later, and this
project's build (`bof3x-launcher.exe`, `bof3x.dll`, `bof3x.exe` in one
directory). `ffmpeg` on `PATH` is optional (section 2, the movie).

```
# 1. the cache: everything the engine reads, from the disc (about two minutes)
python tools/importer.py build --source "Breath of Fire III (USA).cue" --lang en-US --out C:\bof3-cache

# 2. a game directory laid from the cache: DAT\, SND\, BGM\ and the language layer
python tools/importer.py install --base --cache C:\bof3-cache --game C:\bof3-game --lang en-US

# 3. the launcher, pointed at both (or set them once in the settings dialog: Cache folder, Language)
bof3x-launcher.exe --game C:\bof3-game
```

with `bof3x.ini` beside the launcher holding `cache=C:\bof3-cache` and
`language=en-US` (the settings dialog writes both). With no `BOF3.exe` in the
game directory the launcher takes `.data` from the cache
(`BOF3X_EXEIMAGE` = the `cache=` folder) and starts `bof3x.exe`; the hosted
build (`--host bof3`) needs the PC's `BOF3.exe` and is refused.

`install --base` refuses a directory holding `BOF3.exe`: a PC install keeps
its own files. Keep the paths short: MCI fails to open a movie whose full
path is around 160 characters (error 304; the FM group's finding,
[`fmv-import.md`](fmv-import.md)), so a deep folder can lose the movie for
the path alone.

## 2. What is there, and what is not

| | A disc-only install |
|---|---|
| The game's data (`DAT\`) | the disc's: 741 containers from `base/`, the 14 port-edited arenas the disc's own or by rule ([`arena-edits.md`](arena-edits.md)); 252 containers lack the port's Chinese text slots and take the text from the language layer; `AFLDKWA.DAT`, wholly Chinese on the PC, is written empty for the layer to fill |
| The language | the disc's, as a layer built from the disc alone (DIV-0090, [`loc-build-disc-only.md`](loc-build-disc-only.md)); `language=` must name it (`en-US` from a US disc). Without it the shipped text slots are absent |
| The music | the disc's sequences through the engine's sequencer (DIV-0087, `base/bgm`); no MP3s - `BGM\` is empty. The 11 jingles the PC plays as MP3s are not there (an owner's call, `owner-review.md`) |
| The sound effects | the disc's, as `SND\` (880 files, byte-identical to the PC's) |
| The exe's tables (`.data`) | from the disc (`base/exe/data.bin`), the pointer words by layout, the port's own bytes filled by the engine; what no disc carries is zero (section 3) |
| The opening movies | `CAPCOM.AVI` only when the importer could convert the disc's movie with the player's ffmpeg (FM, `base/fmv/`); `LOGOS.AVI`, the porting house's logos, has no disc counterpart. A movie that is not there is skipped and the game goes on (section 4) |
| The keyboard defaults | the engine's rule (`input::DefaultKeys`), the same 24 keys |

## 3. The bytes no disc carries (US)

`recipes/exe-gaps.tsv` classes them per build; `bof3x.log` lists each `gap`
range once at start (`BOF3X_EXEIMAGE  gap ...`). For the US image: 455 gap
ranges, 5,435 bytes, of which the PC has 3,349 non-zero - Capcom game values
no disc carries in any shape found (2,187 non-zero bytes, 321 ranges),
bytes on the disc the places miss (818, 53), DIV-0055's re-laid plates (106,
17) and the eight language addresses no layer writes (238, 64). Read from
the code (`analysis/pc_xref.json`, `symbols.toml`), **not played**; what a
zero there does:

| Range (US) | Read by | A zero |
|---|---|---|
| `Magic_Rows` +0x8.. (151 rows, the 4 bytes beside each row's code pointer: the u16 DAT file of a spell's effect) | `Magic_LoadForItem`, `Magic_LoadForAbility`, `BattleAction_EffectPreload` (`cmp word [ecx*8 + 0x64C2B8], 0xFFFF`) | every row names file 0 instead of its effect file or `0xFFFF` (none): a spell or item effect loads the wrong file. Likely the most visible gap - any battle magic |
| `Effect_HpDamageVariance`+0xE0, 525 bytes (halfword tables behind the eight records at `0x64E9BC`) | `Kaiser_LoadSetFile`, `Accession_LoadFormA` / `B`, `BattleFxRestore_LoadDat` (through the records' pointers) | the dragon forms' set-file numbers zero: a transformation loads the wrong set |
| `Scena17_EndSteps`+0x364 318 bytes and eleven smaller pieces | `Scena17_EndTask`'s step table and the ending roll that follows it | the ending's roll data zero |
| `Equip_SlotIcons`+0x50, 279 bytes | the equipment and experience panels (`Menu_DrawEquipPanel`, `Menu_DrawExpPanel`, `Menu_DrawBigIcon`) by symbol | numbers those panels draw with, zero |
| `EventBattle_Records`+0xE0, 163 bytes | `GameMode5_Load` (`mov ax, word [edx*4 + 0x64DECC]`) | that mode's per-record words zero |

The field button map's defaults (`0x656AEC..0x656B00`, which `Boot_Task`
copies to `0x903580`: zero there meant a New Game with every action
unbound) left this list on 2026-10-12: they are a rule of the engine now,
the default button of each action by name ([`controls.md`](controls.md)
2.1), held in `exe_own.cpp` and verified against `BOF3.exe` when it is there.

None of it is copied (CLAUDE.md rule 1); whether a disc-only build accepts
it, derives the values by a rule, or asks for the PC's exe for those tables
is the owner's call ([`disc-only-round.md`](disc-only-round.md) section 2).
`BOF3X_EXEIMAGE_STRICT=1` refuses to start with any gap.

## 4. The movies

`Fmv_Play` (ours, `src/game/fmv_play.cpp`) opens the AVI through MCI, then
again under the CD root; when both fail it returns and `Game_WinMain` goes
on - the port's own behaviour with a missing file, unchanged, now logged
once per file name (`fmv: CAPCOM.AVI not opened ...`). No ledger entry for
that.

What did change: `Game_WinMain`'s disc probe (`Disc_Probe(CAPCOM.AVI,
BOF3.EXE)`, original `0x5A72C0`) takes `CAPCOM.AVI` in the game directory as
its proof of a local install, and without it looks for `BOF3.EXE` on a CD
drive; neither found, the port shows its insert-the-disc box and exits. A
disc-only install without ffmpeg has no `CAPCOM.AVI` and no PC disc, so
under `bof3x.exe` with a disc-built image mapped the probe's failure starts
it as a local install instead (DIV-0100, section 6). With the movie
there, the probe passes as it always did.

## 5. Measured

| Run | Result |
|---|---|
| `importer.py build` (US cue, `--lang en-US`) | 741 `base/` containers, `loc/en-US` 254, `opt/area4-walls` 1, `base/snd` 880, `base/bgm` 167 files; `base/exe` `psx-us` with `gaps`; `verify base/exe/`: 0 problems |
| `importer.py install --base` | `DAT\` 742 (252 without the Chinese slots, `AFLDKWA.DAT` empty), `SND\` 880, `BGM\` empty, `DAT\en-US.*` 254, `DAT\area4-walls.*` 1; 384 MB |
| exe host self-test, the scratch install, `BOF3X_CACHE` + `BOF3X_EXEIMAGE` = the US cache, `BOF3X_LANG=en-US` | exit 0, `inject: 10088 ours`, `self-test only: done`; `exe_own: ... filled over a disc-built image; no BOF3.exe to verify them against`; `DIV-0089 ... 488 of the manifest's 742 containers held whole, 253 in part (those from the install's DAT\)` |
| exe host self-test, the real install, `BOF3X_EXEIMAGE` = the US cache | exit 0; against the file after the fills: `627123 of 638976 bytes equal`; differing only in `map` (13 ranges, 104 bytes), `widen` (6, 3,065: the blank names), `language` (99, 1,394), `unread` (107, 3,932) and `gap`; every must-equal class equal |
| the attract sequence, 6 minutes, `--no-front`, `BOF3X_OPT=none`, `BOF3X_LANG=en-US`, under `<cache-us>/base/exe/gate-skip.txt`: the scratch install (US cache, no AVI, the real install's `BOF3.CFG` copied in) against the real install with `analysis/cache/pc-plus-us` | both start (no AVI: `DIV-0100` and two `fmv: ... not opened (MCI 304 ...)` lines - MCI answers 304, "the filename is invalid", for a missing file too) and run the same scenes in the same order, two cycles (10,789 / 11,201 ticks). `statehash.py diff`: tick 0 identical; **first difference tick 1**, 94 pages: the VRAM shadow (10 pages from tick 1, 28 from tick 2 - the disc's art and stand-ins where the PC's install has the port's) and the page `0x903000`; `Snd_*` at tick 3 only; from tick 1310 (the first field scene) the sprite, CLUT-strip and area-map pages. Not read to the byte (a `BOF3X_STATEHASH_DUMP` pair is the next step). Without `BOF3.CFG` the run differs at tick 0 too (`Cfg_*`, the display scale): a disc-only install starts with the config's defaults |

## 6. Ledger (for the coordinator)

**DIV-0100 - A disc-only install starts without the movie the disc
probe looks for.**
- Original: `Game_WinMain` calls `Disc_Probe("CAPCOM.AVI", "BOF3.EXE")`
  (`0x5A72C0`): the movie in the game directory, else `BOF3.EXE` at the root
  of a CD drive C..L; neither, the insert-the-disc box (DIV-0039's text) and
  exit code 1.
- New: the same probe; when it fails under `bof3x.exe` with a disc-built
  image of `.data` mapped (`BOF3X_EXEIMAGE`, no `BOF3.exe`), the game starts
  as a local install (the CD root empty, as the probe's local branch leaves
  it), logged once. Every other case unchanged: the hosted build, and the
  exe host over the PC's `.data`, still refuse.
- Why: a disc-only install has `CAPCOM.AVI` only when the player's ffmpeg
  converted the disc's movie; the movie is not what makes it an install.
- Also in the PSX version? No probe: the PlayStation boots its disc.
- Verification: built; the self-tests do not reach `Game_WinMain`; the attract
  run from the scratch install (no AVI) starts the game (section 5).

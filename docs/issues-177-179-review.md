# Issues #177, #178 and #179: the board crash, the dock's legend and list, the engine gauge's line

**Status:** MEASURED (2026-11-01, a cloud session with the owner's US disc, PSP-EU disc
and PC ISO; two fixes built and headless-checked, every live look owed the owner).
Branches: `fix/issue-177-passage-list` (DIV-0197) and, stacked on it,
`fix/issue-178-dock-glyph` (DIV-0137's amendment, DIV-0198) - merge in that order,
the ledger's numbers are sequential. #179 is a reading, no code.

All three reports are ScrubOasis's, 2026-11-01, on `main` `7bd917d` (PR #176),
a PSX-US + PSP-EU English disc-built install. Nothing here was played: there is
no Windows in the session, so the route replays, `recipe_shots.py` and the
`BOF3X_SHADOW` self-tests are the owner's; what follows is read off the code,
the report, the discs and an image the importer built from them.

## 1. #177 - crash when reading the faerie village board (DIV-0197, D399)

**The report, resolved.** A build of `7bd917d` with the CI toolchain (llvm-mingw
20260616 `ucrt-ubuntu-22.04-x86_64`, cross-compiled on Linux, `cmake --preset i686`)
is layout-identical to the reporter's DLL: the sixteen bytes the report gives at
eip are the build's own at `0x101B7486`, and every stack word resolves to a
function's return. `llvm-addr2line`:

| report | function |
|---|---|
| eip `bof3x.dll+0x1B7486` | `Area_PassageAhead+0x56` (`event_ops.cpp`), `movzx edi, byte [eax+3]` |
| `+0x30 0x101A8BF8` | `Field_LeaderTalkTest` |
| `+0x34 0x101B6731` | `Field_LeaderWalk` |
| `+0x40 0x101B8836` | `Field_LeaderControl` |
| `+0x54 0x100EDB5C` | `Field_MembersFrame` |
| `+0x58 0x100B69FB` | `Field_Frame` |

The talk button, no object faced, no 0x51 cell hook taken, a 0x50 / 0x52 / 0x53 /
0x55 cell around (`Field_LeaderTalkTest`, [`event-ops.md`](event-ops.md) 3).

**The registers.** `ecx 0x644628` is `Area_Descriptors[0xB9]`, area 185's
descriptor (`recipes/exe-layout.tsv` `0x667874`); `esi 0` its `+0x31`; `eax 0` its
`+0x2C` passage-list pointer plus `0 * 8`: the first iteration, the list null,
`e[3]` read at address 3. (A first reading of the report took `ecx` for
`0x48`, which no path gives; the value is `0x644628`.)

**Whose.** The layout table has no pointer word at `+0x2C` for that descriptor,
nor for any of the eleven faerie village descriptors (areas 175..185), nor for
105 of the 200 (a walk of `exe-layout.tsv` from `Area_Descriptors`). The PC's
`BOF3.exe` read directly (section 4): area 185's `+0x2C` is 0 and `+0x31` 0, as the
image's. So Capcom's `0x530600` makes the same read from a PC install and faults;
on the PlayStation `FUN_801B6C4C` reads low RAM at address 3 and goes on. The
same class as D397 / DIV-0185 and D102 / DIV-0183. `event-ops.md` 5 had this
family down as "none reachable by what the field sets".

**The fix.** DIV-0197: a null `+0x2C` answers null, which `Field_LeaderTalkTest`
already tests for (the porting house's own null check, `event-ops.md` 5); one log
line per area. A descriptor with a list takes the original walk unchanged; the
`event_ops` fuzz (`SeedPassages`) gives area 0 a list every round, so it compares
that path alone. The checks `ledger_check`, `ledger_switches`, `ledger_registry
--check`, `reloc_bare --check`, `reloc_census --check` pass; the DLL, exe host
and launcher build.

**Owed.** The reporter's spot live before and after; the same press on the PC
install under `--original Area_PassageAhead` (the reading says Capcom's faults
too). Not established: why the board did not answer first - the `fairyVillage`
route reads it on all three `eu` installs (D260's fix), so the press differs from
the route's: a cell the board's 0x51 hook (`Area179_CellHook`, cell `(0x3F, 0xE)`,
pose 1) does not take, beside a door cell, is the guess. `tools/crash_report.py
<dmp> --read 0x644628 0x44` on the reporter's dump would show the descriptor.

## 2. #178 - the dock's parts menu: two unrelated misses

The owner's reading of the capture ("the parts loaded one row too low, the circle
rendering halfway into the square... unless it's two unrelated misses"): two.

### 2.1 The glyph (DIV-0137 amended)

`EffectKind5E_DrawPanel`'s glyph fallback drew at the cell's `y + 3`; with
`Text_DrawAt`'s `y + 1` as the glyph's top, a 12-unit icon sat at `y + 4 ..
y + 16` over a cell of `y .. y + 10` - 5 px low, its lower half over the next
row's icon. The capture agrees to the pixel (the red/pink shape spans about 76..87
game px with the OK row's cell at about 71..81). Now `y - 2`: the glyph at `y - 1
.. y + 11`, centred. [`rest_3e.md`](rest_3e.md) 11.5 had this as "measured off the
code, not seen; the owner tunes by eye off the first capture" - this was that
capture. The owner's eye on the next one is owed, with the glyph's x.

### 2.2 The list under its hand (DIV-0198, D400)

`EffectKind5E_DrawMenu` draws the hand at the hand word's y plus 15 per pick;
`EffectKind5E_DrawList` its eight lines from the list box's `y + 0x14`, 15 apart.
Read off the owner's sources (section 4):

| | list box `0x654878` | hand `0x654894` | the twin's margin | first part |
|---|---|---|---|---|
| PC (`BOF3.exe`), psx-jp, PSP-EU (`SCENA10.EMI#0` `0x1F7B50`) | 96, 80, 128, 144 | 100, 100 | 0x14 | y 100 |
| psx-us (`SCENA10.EMI#0`; the twin `0x801F7AD4`, `0x801F7B50 addiu $s3, $v1, 4`) | 96, 96, 128, 128 | 100, 100 | 4 | y 100 |

The US release moved the list box down 16 and shortened it, and changed the margin
constant in its twin, so its first part stays at y 100 under the hand; the PC's
20 px strip above the first part is Japan's own layout, with the hand right.
Since DIV-0195 a Western PSX disc-built image carries the West's box (the run
`0x654876..0x654898` is `map SCENARIO/SCENA10.EMI#0@0x801FE7EA`, the US disc's
own bytes; `exe_maps/psx-us.tsv` has it as `agree 138` of 138, wrong by those two
words - not chased), and the PC's code added Japan's 0x14 over it: the first part
at y 116, the hand 16 px above it on the empty strip. DIV-0195's reading had the
three fields `0x65486C` / `0x654870` / `0x654874` and not these two words;
[`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md) 5's open question
on `0x654878` is answered.

The fix: `ListFirstLine()` in `rest_3e.cpp` answers the twin's constant for the
release of `ExeImage_MappedBuild()`'s build (`sigma::ReleaseOfSources`: `kWest` 4,
else 0x14; no image, the PC's `.data`, 0x14), once, logged once - the D313 idiom
(`fishing_text.cpp`). The hosted fuzz has no mapped image, so it compares the 0x14
path alone. Not read: the EU-en, FR and DE discs' words and twins (the US disc
alone was here), inferred from DIV-0195's all-four finding.

**Owed.** The owner's `shipPartsMenu` recording replayed on a Western-disc-built
install (the hand on `Part A`, the eight rows in the box, the glyph centred) and on
the PC (unchanged). The `shipPartsMenu.DAT` save the owner placed is the one.

## 3. #179 (and the crane game's button) - D198, the `POLY_FT4` far edge; no change

The red/blue line under the ENGINE gauge, and the green/yellow border on the right
and bottom of the crane game's button, are one defect: **D198** - a textured
quad's far edge samples the texel past its art - which DIV-0010 fixed for the
`SPRT` handlers and not for `D3d_DrawPolyFT4`. The owner's reading ("it moves with
the energy bar, an over-read of the pixel, not its own quad") is the mechanism.

**The code.** `Area104_DrawGauge` (`area_w2e.cpp`, also area 121's) draws each bar
as one FT4: y 29..39 (10 px), v `0xD8..0xE0` (8 texels), u `0x98` (the back bar,
CLUT `0x780B`) or `0x90` (the front, `0x780E`) to `u + 4` over the bar's width
(4 texels, 16 px each at full width). `D3d_DrawPolyFT4` takes every corner
through `D3d_TexCoords`, `tc[i] = (i + 0.512) / 256` (`d3d_draw.cpp` `PutTexel`,
tv as tu), so the far corner is `0xE0 + 0.512` and the quad's bottom screen row
samples texel row `0xE0` at every scale (8 texels over 10 px: the last row's
centre at `0xE0 + 0.512 - 0.4/k`). The PlayStation never samples `v1`.

**The data** (the PC ISO's `FIRST.DAT`, the UI page, kind-1 tag `0x1E000200` at
VRAM (960, 0), 2 tiles by 8, read with `tools/dat.py` and the tiling
`loc_build.py`'s `page_texel` describes): rows `0xD8..0xDF` hold the two bars;
row `0xE0`, the top of the next glyphs, holds under the **front** bar's u span
index 2 at `u 0x93` and 7 at `u 0x94`, and zero under the back bar's
(`0x98..0x9C`). So the bottom row paints texel 3 of 4 across the last quarter of
the front bar's width, plus the edge texel: a short two-colour line at the bar's
right end, at whatever width the bar has - "moves with the bar". Far edges only:
the near corner at `tc[u0]` stays inside the first texel, which fits the crane
button's right/bottom border. Capcom's port drew this too; ours is faithful.

**The cure, the owner's call** (D198's entry: renderer-wide, an entry of its own):
DIV-0010's `SPRT` rule carried to the `POLY_FT4` path - for the corner(s) whose u
(and, separately, v) is the quad's maximum, the far value `u - 1/30 + 0.012`
texels instead of `u + 0.512`, so the last screen pixel samples the last texel's
centre. That reproduces the PlayStation's exclusive edge for both builder kinds
(`u + w` and `u + w - 1`). It touches every textured quad in the game and is
render-only (the state hashes cannot see it), so the proof is captures: the
ENGINE gauge on `overheadAndDeepSea`, the fishing menu's frame lines (D198), the
crane button. A one-quad fix is impossible (u and v are bytes in the primitive).

## 4. The sources, and what was built from them

- **The US disc** (`.bin/.cue`), **the PSP-EU disc** (`.iso`): `importer.py
  identify` psx-us and psp-eu. `importer.py build --source <US> --source <PSP>`
  (Python 3.13 for `sys.monitoring`; capstone 5.0.7) built the psx-us-first image
  whose `base/exe/data.toml` and `data.bin` section 2.2 reads. The twins were read
  from each disc's `SCENARIO/SCENA10.EMI#0` through `region_diff.Build` and
  capstone (MIPS32 LE).
- **The PC ISO** (`Breath of Fire III[CHS].iso`, the disc image with `BOF3.exe`,
  `DAT/`, `BGM/`, `SND/` at its root): `BOF3.exe` matches `fixtures.toml`'s
  sha256. Five of the 742 `DAT/` files hash differently from
  `fixtures/pc-zh.DAT.files.tsv` (the owner's install, 2026-10-06) - `AREA093`,
  `AREA140`, `BOSS020`, `BOSS051`, `DEMO` - and in each the one differing chunk is
  a kind-2 audio bank, nothing else; `identify` refuses the tree as a whole for
  them, so the exe and `FIRST.DAT` were read directly. Which copy is the odd one is
  the owner's to know.
- Nothing game-derived is in the repository: the caches and extracted trees lived
  under the session's `/tmp`.

## 5. What the owner validates locally

1. Merge `fix/issue-177-passage-list`, then `fix/issue-178-dock-glyph`.
2. #177: the board on a disc-built install (and `--original Area_PassageAhead` on
   the PC install, which should fault); the reporter's dump's descriptor.
3. #178: `shipPartsMenu` on a Western-disc-built install - the hand on `Part A`,
   the glyph centred in row 0; the same on the PC, unchanged. The `DockLegend
   page ... shown ...` and `DIV-0198` log lines say what each draw decided.
4. #179: the call on D198's cure; if taken, captures of the gauge, the fishing
   frame lines and the crane button at scales 1, 2 and 4.

# The fishing spot's Rule pages on a disc-only install (D303, D286)

**Status:** MEASURED (2026-10-16): the cause read from the disc-built image's
`data.toml`, the PC's `.data` and all seven held discs' fishing module; the
fix built into the recipes and checked; the live check in section 5.

## 1. What was seen

`campingFishing` on the PSX-US disc-only install (frames 3690..3750, the
`Rule` tab, `How to Fish`): the page's title `Type of animal` where the PC
draws `Fundamentals`, the page's rows drawn one on top of the next down the
panel and past its bottom edge, and the page counter `0` where the PC draws
`1/6`. PSP-EU drew the page right but its counter `0` too. The same screens'
equip panel draws no `ROD` / `LURE` / `EQUIP` / `GUIDE` headings on the
disc-only installs (D286).

## 2. The cause: three gaps of the disc-built image

The page is `EffectKind0F_DrawMessageList` `0x468560` (ours,
[`effect_1b.md`](effect_1b.md)): nine rows of the fishing area's message
pool by the u16 ids at `0x653E00` (54, a blank row for `0xFFFF`), the title
by `0x653DF4[page]` (6), the counter `0x653EB4` (`"%d/6"`) printed with the
page + 1. The panel headings are four fixed-width fields at `0x653EA0`, the
data page's total `0x653EBC`.

On every disc-built image before this fix those bytes were zero
(`recipes/exe-gaps.tsv`, PSX-US):

| PC range | What | Class | Effect on a zero |
|---|---|---|---|
| `0x653DF4..0x653E3C` | the 6 title ids and the first 30 row ids | `disc-unplaced` (US, EU, FR, DE) | every id 0: each row draws message 0 of the pool, the fish list's four-line `Type of animal` entry, nine of them 13 dots apart - the "stacked" text; the title the same message's first line |
| `0x653EA0..0x653EB4` | `ROD`, `LURE`, `EQUIP`, `GUIDE` | `disc-unplaced` (US, EU, JP, both PSPs), `game-value` (FR, DE) | blank headings (D286) |
| `0x653EB4..0x653EC0` | `"%d/6"` and the data page's `23` | the same rows | an empty format: the counter's buffer keeps a stale digit, the `0` (the faerie board's counter showed the same, 59d44eb4) |

Why no disc filled them: each disc's fishing module (`WORLD00/AREA030.EMI`
section 4; `AREA089`, `AREA129` carry the same) holds every one of these, but
not where the twin survey could place them.

- **The id table** is at the place the map's segments either side imply
  (US `0x801E2114`, `0x48` past the segment that places `0x653DAC`; the
  segment after it resumes at `0x653E3C`), but the Western discs re-laid
  the rows for their own text. Nine rows a page, `--` a blank row:

  | Page | PC and JP | US, EU, FR, PSP-EU's `BOOT.BIN` | DE |
  |---|---|---|---|
  | 1 | `72 73 74 -- -- -- -- -- --` | `72 73 -- 74 -- -- -- -- --` | as US |
  | 3 | `7b 7c -- 7d 7e 7f 80 -- --` | `7b 7c 93 -- 7d 7e 7f 80 --` | `7b 7c 93 -- 7d 7e 7f 80 6d` |

  Pages 2, 4, 5 and 6 and the six titles are the same in every build. So
  the West moves page 1's third line down a row, adds line `0x93` on page 3
  (pushing the blank row after it down one), and DE fills page 3's last
  row with `0x6D`. Nothing moves to another page. The bytes differ from the PC's, so
  no byte-equal segment covers them. JP's equals the PC's and its map placed
  it.
- **The headings** follow the tabs (`Gear` `Data` `Rule` on the US disc, 12
  bytes the PC does not have there), so the map's segment stops before them.
  FR and DE re-lay them at other widths (`Canne`, `App\x9et`; `RUTE`,
  `K\x95DER`, ...), so the PC's four fields have no copy on those two discs.
- **The format** is at the head of the module's read-only data
  (`0x801D0C08` on every disc, PSP copies included), far from the map's
  segments, and 12 bytes is below the places' 16-byte window.

**Evidence that each is the disc's own copy**: the module's code reads it
there (a `lui` / low-half pair, `capstone`-checked, scratch `refs.py`):

| Build | Ids (titles, rows) | Readers | Format | Headings |
|---|---|---|---|---|
| psx-jp | `0x801E2140`, `+0xC` | `0x801D7710` (lhu), `0x801D7608` (addiu) | `0x801D0C08` by `0x801D7768` | `0x801E21F8` |
| psx-us | `0x801E2114` | `0x801D76C8`, `0x801D75C0` | by `0x801D7720` | `0x801E21CC` (read at `0x801D81E4`, `+4` `0x801D8878`, `+8` `0x801D8948`, `+0xD` `0x801D8CEC`) |
| psx-eu-en | `0x801E2134` | `0x801D76C8`, `0x801D75C0` | by `0x801D7720` | `0x801E21EC` |
| psx-fr | `0x801E23C8` | `0x801D7858`, `0x801D7750` | by `0x801D78B0` | - |
| psx-de | `0x801E2110` | `0x801D7694`, `0x801D758C` | by `0x801D76EC` | - |
| psp-jp, psp-eu | `BOOT.BIN` `0x2E02CA` / `0x2DFAEE` (the one copy there); the EMI copy `0x801E2140` | the EMI copy's `0x801D7710`, `0x801D7608` | EMI `0x801D0C08` | EMI `0x801E21F8` |

PSP-EU's `BOOT.BIN` holds the Western layout while its EMI copy (which its
map placed) holds PSX-JP's: the PSP-EU image drew the JP layout with its own
English text, which is why its page looked right but dropped line `0x93`.

## 3. The fix (DIV-0101, amended)

- `tables.toml`: two catalogued tables, `fishing_rule_titles`
  (`EffectKind0F_RuleTitleIds`) and `fishing_rule_lines`
  (`EffectKind0F_RuleLineIds`), with a place in all seven builds; a
  disc-built image takes the build's own bytes there (`table` ranges of
  `data.toml`), the PSPs' from `BOOT.BIN`.
- `tools/psp_exe.py` `TWINS`: the headings (US, EU, JP, both PSPs) and the
  format with the total (all seven) from `AREA030.EMI`. Both equal the PC's
  byte for byte (`exe-psp.tsv`: "0 not BOF3.exe's").
- `symbols.toml`: the five data names above.
- Regenerated: `exe-psp.tsv` (+12 rows), `exe.toml`, `exe-layout.tsv`
  (three words of the row table now classed numbers, `0x653EA0` no longer
  one), `exe-gaps.tsv` (13 rows out, two in: FR and DE keep the headings,
  now named `EffectKind0F_PanelTitles`). Nothing else in any recipe moved.

**What it changes**: on a disc-only install the Rule pages draw the disc's
own rows, the counter `1/6`, the headings. On the Western builds the rows
are the Western layout - page 1's third line a row lower, page 3's extra line - where
the PC (and the PC with an English layer) draws the JP layout. That is the
one place the image now differs from `BOF3.exe` on purpose; the ledger
amendment records it. The PC install is unchanged.

## 4. Left

- FR and DE: the panel headings stay blank (D286's remainder). The PC's four
  fixed fields cannot hold their words; drawing them needs the module's own
  widths in ours, as DIV-0069 did for the tabs.
- The PC with an English layer (`pc-en150`, `pc-enus`) draws the JP row
  layout with the Western text: line `0x93` (and DE's `0x6D`) is never
  shown there. Whether the language layer should carry the row table is the
  owner's call (not changed here).

## 5. Checks

- `tables.py check`, `exe_tables.py check`, `psp_exe.py check`,
  `importer.py check`, `exe_own.py check`: 0 errors.
- The live check, section 6.

## 6. The run

`tools/abc_run.py run --set us --only psx-us --route campingFishing` on a
fresh PSX-US disc-only install built from these recipes (root
`C:/Users/kerok/abd303`, outside the repo; launcher the `fix/d255-inn-jingle`
build, which the route needs to get past the camp, D255), 2026-10-16:

- frame 3720 (the Rule tab): title `Fundamentals`, the page's rows
  `Xbutton: Sets casting power` / `Wind reel`, a blank row, `Directional
  button: Move rod` (the US disc's layout), counter `1/6`. Before: section 1.
- frame 3360 (`fishing_equip`): `EQUIP`, `GUIDE` and `LURE` drawn (D286).
- The route to its last frame (5273), the same 187 timed shots as the
  D255 run; no crash, no frame-deadline line in `bof3x.log`.

The image's `data.toml` for that cache: `0x653DF4..0x653E00` and
`..0x653E6C` `table` from `AREA030.EMI#4` `0x801E2114` / `0x801E2120`,
`0x653EA0` and `0x653EB4` `place` (`twin`) from `0x801E21CC` / `0x801D0C08`.
Not run: PSX-EU, FR, DE and the PSPs live (the recipe rows and the image
bytes only), and the Rule pages past page 1 (the route shows page 1).

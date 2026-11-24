# The state hash on a clean install: the desktop's runs of 2026-11-24

**Status:** MEASURED (2026-11-24, the Windows desktop, the `latest` release
zips of `main` `65b7810f`; the hosted side a local build of the same commit).
The desktop's half of the cross-machine comparison the Linux laptop's session
runs on `harness/only-slot0`: `bridgeWalk` at both widths, three checks before
the ten routes, and the ten routes (section 6). Nothing in `src/` changed. The
comparison with the laptop's hash files is the laptop's to make; this note
holds only what the desktop measured. All files are in `D:\bof3x-runs\orig6`
(not in the repository: the hash files are derived, the dumps are game data).

## 1. The set-up

- **Builds:** `bof3x-latest-win.zip` and `bof3x-latest-win-x64.zip` of the
  `latest` release at `65b7810f`, each unzipped into its own folder with no
  `bof3x.ini` (`L32`, `L64`). These are the stripped, signed files a player
  gets. Log line: `build: latest-65b7810f`.
- **Install:** `importer.py sources add` of the PC folder, the US PlayStation
  disc and the PSP Europe disc, then `importer.py setup --home
  D:\bof3x-runs\orig6\home` (120 s). The manifest reads `target = "pc-zh"`,
  `base_policy = "psx"`, `install_kind = "pc"`. The game folder holds `BGM`,
  `DAT`, `SND`, `capcom.avi`, `CAPCOM30.STR`: no `BOF3.exe`, no save slot, and
  **no `BOF3.CFG`** - setup does not write one.
- **`BOF3.CFG`:** put in by hand from section 3 on, six bytes `0\r\n1\r\n`
  (CRLF, as the PC folder's own file).
- **The save:** `tools/recipe_saves/bridgeWalk.DAT`, sha256
  `e8963f1857559b18a2015af40b0db092dbe10c0ffac882f5ec57dfd253bfc287`, the
  laptop's.
- **Run settings:** `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, `BOF3X_HOME` the
  install's home, `tools/input_run.py` of `main` with `--no-front`. The
  Windows harness swaps slot 0 in the install itself; the folder holds no
  other slot, so `--only-slot0` was not needed and was not exercised here.

## 2. The manifest does not hash as the laptop's

The sha256 of `home\cache\manifest.toml` from its `[cache]` line to the end
(LF line ends, no CR in the file):

    5e19dc832791178a6e37d6f33d584bd264f0b7669f87b52c69da5fdb3d1e358c

The laptop's was given as `350a36d7...17ec`. A second desktop install made
from the same three sources on 2026-11-23 (`D:\bof3x-runs\psptext1123\home`)
hashes as `5e19dc83...` too, so the desktop repeats. Which block differs is
not known here. The desktop's blocks, each from its header line to the next
header, sha256's first 16 digits:

| Block | Lines | Hash |
|---|---|---|
| `[cache]` (the one line) | 1 | `7fe3730e1d803659` |
| `assets = [` | 3786 | `2a1666324d92be69` |
| `overlays = [` | 500 | `90f59a742ecfac6a` |
| `opt = [` (with `opt_recipe_sha256` before it in `overlays`' block) | 1227 | `85b4c9fbff5eabec` |
| `bgm = [` | 255 | `f17f2b50c2b2ead4` |
| `[snd_jingles]` | 4 | `9cc972bbe0860b88` |
| its `files = [` | 16 | `38d55d2e48570a2d` |
| `[exe]` | 6 | `640c674b112479c0` |
| `[fmv]` | 9 | `99cee7a8983eb2a2` |
| `[psp_layers]` | 2 | `0c81be7909aca9f5` |
| its `rows = [` | 1179 | `80a01b451ea31113` |

The split is `re.split(r'(?m)^(?=\[|\w+ = \[)', text)` over the text from
`[cache]`.

## 3. `bridgeWalk`, our code, both widths

Every run: rc 0, `recipe done at recipe frame 2099`, **2049 records** (ticks
0..2048). The laptop's run has 2074 with the same end frame, so the two sides
are 25 ticks apart before the first record and a comparison wants `statehash.py
diff --align recipe`. What makes the 25 ticks was not looked for.

| Run | Language | `BOF3.CFG` | Scale | Skip list | sha256 of the `.sh` |
|---|---|---|---|---|---|
| `bridgeWalk_w32.sh`, `_w64.sh` | en-US | none | unset | 173 ranges, `2C491AB2` | `18934459...0e9258` (both) |
| `bridgeWalk_w32win.sh` | en-US | `0`, `1` | 2 | 173, `2C491AB2` | `59d9c9dd...f60d08` |
| `bridgeWalk_w32s2.sh`, `_w64s2.sh` | en-US | `0`, `1` | 2 | 176, `929F2913` | `a6c7cde4...7977cf` (both) |
| `bridgeWalk_w32zh.sh` | none | `0`, `1` | 2 | 176, `929F2913` | (section 5's our side) |

- **32-bit against 64-bit: byte-identical files**, in both pairs.
- **No `BOF3.CFG` against the two-line file at scale 2** (`_w32` against
  `_w32win`): four pages, every tick from tick 0 - `0x65D000`
  (`Cfg_Fullscreen`, `Cfg_RenderMode`, `Cursor_Hidden`), `0x66B000` (the
  `MenuList_*States` tables), `0x7C9000` (`Display_DesktopWidth`,
  `D3d_ScaleX`, `D3d_ScaleY`), `0x7DE000` (`Snd_BufferDesc`,
  `Snd_WaveFormat`, `Snd_Device`). The bytes were not named.
- **173 ranges against 176** (`_w32win` against `_w32s2`): identical on all
  2049 ticks on every page outside the three the added ranges touch.
- Raw dumps (`BOF3X_STATEHASH_DUMP`) at ticks 975 and 1000 were written for
  `_w32d` (no `BOF3.CFG`) and `_w32win`; tick 975 is the laptop's tick 1000 if
  the 25 ticks are all before the first record - an inference from the record
  counts.

## 4. Check 2: the install keeps its `BOF3.CFG`

After every run of sections 3, 5 and 6 the file is still `0\r\n1\r\n`. The log
of a windowed run reads `render: Direct3D 11 feature level 0xB000, window 640 x
480, target 640 x 480 (view 320 x 240 at 2, 0 columns a side)` and
`Display_Setup: 640 x 480, scale 2, linear filter, device 1`.

## 5. Check 3: Capcom's code on the clean install

The hosted side is `--original "*,-Game_Clock"` with `BOF3.exe` copied into
the install's game folder for the run and removed after. **The release zips
cannot run it**: `--host bof3` and the hosted-only switches are the developer
launcher's (`bof3x-launcher-dev.exe`), which a release does not carry, so the
hosted side ran from a local build of `65b7810f` (`build\`, unstripped; log
line `build: v0.1.0`).

### 5.1 Capcom's side does not take the language

With `--lang en-US` on both sides 195 pages differ, from tick 1: the name
tables (`0x657000`, `0x658000`), `MessagePools`, the glyph pages of
`Gfx_VramShadow`, `Menu_Verbs`. The hosted run's log has the language lines
(`language en-US: the cache's loc layer`), yet its hash differs from the
hosted run with no language on 8 pages only. So a pair with Capcom's side is
made with no language on both. (`bridgeWalk` was recorded under en-US and
still ends at frame 2099 with no language, on both sides.)

### 5.2 No language: 36 pages, and the control pair on the PC folder

The same pair was run on the PC folder (`<main>\bof3`, ten save slots, our
side reading `.data` from `BOF3.exe`) as a control.

| Difference | Install pair | PC-folder pair |
|---|---|---|
| `0x5FD000`, every tick (DIV-0176, the ledgered one) | yes | yes |
| about 30 pages of `Gfx_VramShadow`, `Gfx_ClutStrip` and `0x6BE000` (`FieldHidden_*`), the route's last ticks | from 2039 | from 2042 |
| `0x6BD000` from tick 2 | yes | no |
| `0x7CA000`, every tick | yes | no |
| `0x7E0000`, `0x803000`, `0x903000`, `0x929000`, ticks 235..305 only | yes | no |
| `0x92A000` from tick 192 (`Save_Staging`, `MapView_CornerPtr`) | no | yes |

**No page of field, party, save or scenario state differs persistently on the
install.** Nothing here says Capcom's loader reads the opt layers' files out
of `DAT\`.

### 5.3 Ticks 235..305: the empty save slot, DIV-0193

Raw dumps at ticks 236, 260 and 300: the four pages differ in
`Gfx_PacketNext`, `Gfx_OtHeads`, `Gfx_OtPointers`, `DrawItemPool_Top` and
`Text_PenY` / `Text_PenX` - the draw list and the text pen. Captures of the
load menu at recipe frames 240..372 (`out\pair_264.png`): Capcom's side draws
the port's text in slots 1 and 2, ours draws them blank. That is DIV-0193 (an
empty save slot draws no text). The earlier references never showed it: the PC
folder has all ten slots full. **Confirmed** by our side again with
`BOF3X_OFF=SaveSlotBlank`: those four pages no longer differ and the pair is
32 pages (`diff_capcomzh_ours_slot.txt`).

### 5.4 `0x7CA000`: `Display_ModeBpp`, a Windows setting on the PC folder's path

`Display_ModeBpp` `0x7CADE0` reads `0x20` on Capcom's side on the install and
`0x10` on the PC folder; ours writes `0x10` always. Capcom's set-up takes the
first entry of the device's sorted mode list ([`display-setup.md`](display-setup.md)
2.6). Windows has `DWM8And16BitMitigation` recorded for
`<main>\bof3\BOF3.exe` (`AppCompatFlags\Layers`, in both HKCU and HKLM), which
offers a 16-bit mode there; the install's path has no entry, so the first mode
is 32-bit. A property of this machine and of that path, not of the data. The
page's other raw differences (`Font_TexCache`, `D3d_Vertices`) are in both
pairs' dumps and the PC-folder pair's hash does not report the page, so they
are ranges the hash already leaves out or folds.

### 5.5 What is left, and not explained

- `0x6BD000` from tick 2: `Music_File`, `Music_FileSize`. Read as our side
  playing the cache's song and Capcom's the file in `BGM\`; **an inference
  from the symbols, not confirmed.**
- The route's last ticks, in both pairs: Capcom's code against ours, not the
  install. Why it begins three ticks apart in the two folders was not looked
  for.

### 5.6 What a Capcom-side reference on a clean install needs

1. No language on both sides (5.1).
2. Our side with `BOF3X_OFF=SaveSlotBlank`, or DIV-0193 taken as part of the
   signature on every route that shows the load menu (5.3).
3. `0x7CADE0` (4 bytes) in the skip list, or the reference recorded from a
   path that has the Windows setting (5.4). Neither was done.
4. The developer build for the hosted side.

### 5.7 One hosted run stopped before its first frame

A hosted run on the PC folder sat for nine minutes with a `.sh` of 0 bytes and
no error line in the log (the last line `retarget ON InputScript`). It was
ended by its process id, the harness put slot 0 back, and the same command ran
to the end at once. Cause not known.

## 6. The ten routes, our code, both widths

Section 3's settings with no language (the ten are run so), the two-line
`BOF3.CFG`, `BOF3X_SCALE=2` and the 176-range list (`929F2913`), on the same
install; each route on the 32-bit build, then the 64-bit, one game at a time
(`suite.py`, 07:33..08:29, 55 minutes). **All twenty ended rc 0 on the first
attempt, and every route is identical between the two widths on every tick
both files hold.** `BOF3.CFG` was `0\r\n1\r\n` at the end.

| Route | Save | End frame | Records, 32 / 64 | 32 against 64 | sha256 of the 32-bit `.sh` (first 16) |
|---|---|---|---|---|---|
| `field_menu` | `adult_ryu` | 1454 | 1409 / 1409 | files identical | `ad745d9af13c07e4` |
| `menu_screens` | `adult_ryu` | 1770 | 1729 / 1729 | files identical | `15a598dd18f21508` |
| `combat` | `combat` | 2620 | 2625 / 2561 | identical on 2561 ticks | `ee2fcc7a399b49fe` |
| `shop` | `town` | 3157 | 3137 / 3137 | files identical | `b4cf6366fbce90af` |
| `dragonTransform` | `adult_ryu` | 4329 | 4289 / 4289 | files identical | `9511ea5bbd00d492` |
| `cutsceneAndNue` | `nue` | 7421 | 7425 / 7425 | files identical | `6d30516acf2cbd2a` |
| `whelpBoss` | `whelp_boss` | 13181 | 13185 / 13121 | identical on 13121 ticks | `fa9d084a488b831d` |
| `worldMapAndAreaTransition` | `town` | 2143 | 2113 / 2113 | files identical | `25a6e212d53df4c7` |
| `caughFish` | `camping` | 3889 | 3841 / 3841 | files identical | `5c8c3292e2ee66d0` |
| `masterAndManillo` | `masterAndManillo` | 5519 | 5505 / 5505 | files identical | `5998f16ab6c6e16c` |

`combat` and `whelpBoss` differ in length by 64 records: the file is flushed
every 64 ticks and the runner ends the game with `taskkill`
([`state-hash.md`](state-hash.md) 1), so one width's file got one flush more.
The 64-bit sha256s of those two: `926e81e2046194fa`, `3239b26394088694`.

The files are `D:\bof3x-runs\orig6\suite\cn_<route>_o6w32.sh` and
`_o6w64.sh`, each with its `.runlog` and the game's `.log`. `shop` ran with
`--slot0-hold`, as in the earlier proofs. Not compared with the laptop's
files here, and not with any Capcom-side run: section 5.6 says what that
would take.

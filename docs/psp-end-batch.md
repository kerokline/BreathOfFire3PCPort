# The PSP-only round's end batch

**Status:** MEASURED (2026-10-12, evening, group EB, branch `psp/endbatch` off
the round tip `88817a07`; offline, headless and live runs on the owner's
machine while the owner was away). The brief is `psp-only-round.md` 6. No
game data here: counts, hashes, offsets and file names only (CLAUDE.md rule 1).
Scripts and outputs are in the session scratch `psp_eb/`.

## V1, offline

| Check | Result |
|---|---|
| `importer.py build` PSP-JP `--lang ja-JP` / PSP-EU `--lang en-150` (4 min 34 s / 4 min 41 s) | `base/dat` **741** each; `base/bgm` 166 songs + 81 banks; `base/snd` **891**; `base/fmv/CAPCOM.AVI` (231 frames, cvid, 640 x 480); `base/exe` `psp-jp` / `psp-eu`; `loc/ja-JP` / `loc/en-150` **254**; `opt/psp-chord-one-tone` 16. `[build]`: `base_policy = "psp"`, `standins_sha256 = aec008f4...`, `exe_tables_from` the disc |
| `verify` on both | exit 0, 0 problems; 447 of 742 containers byte-identical to the PC's fixture, 41 with the disc's own sections, 254 incomplete (the Chinese text) |
| PSP-JP against PSP-EU, file by file (`treecmp.py`) | 1,881 identical; differ: 15 `base/dat` (`AREA004/016/024/033/045/065/087/088/115/121/128/151/152`, `DEMO`, `FIRST`: SI's language pages), `base/exe` (data.bin **124 bytes in 67 runs**, IM's total; the split 89 / 23 / 12 was not re-derived), the manifest; `loc/` by tag only. `base/bgm` identical |
| European PSX `--lang en-150` | **733** containers: 8 bank containers (`AREA100/112/135/138/147/168/170/188`) have no source on that disc alone (the coordinator's `ref_eu` the same); `verify` exit 1 on the known `area4-walls` slot-6 line only |
| PC + US + PSP-EU, `--opt psp-music psp-snd psp-tables psp-ryu-forms` | `verify` exit 0; 742 of 742 byte-identical; the five TG layers 0 problems (247, 891, 2, 19, 16 files) |
| The music (host `synth_render`, 60 s, PSX-EU base against PSP-EU base) | 166 of 166 rendered, no abort. The 73 sequence-identical songs: **8 sample-identical, SNR 31.7 / 42.7 dB min / median** on their own banks (MB's inverse); on the PSX banks **73 of 73 identical**. The others by MS's class, SNR min / median: rhythm 35.2 / 43.0, order 32.6 / 42.6, bend 16.4 / 31.8, controllers 16.0 / 20.8, setup (31) 16.8, notes -3.7 / 23.7 (157, the tempo song, lowest) |

## V2, headless

Exe host self-test, ini-less launcher copy, no PC file: PSP-EU exit 0
(`place 527 / 5296`, `gap 352 ranges 3132 bytes`, `exe_own 17 regions`,
`inject: 10088 ours`), PSP-JP exit 0 (`place 543 / 5299`, `gap 319 / 2925`,
`10087 ours`: `Config_DrawControllerCell` is the Latin languages' only). Both
log `DIV F ... duel-loss on, myria-item on (the default: the cache's
base_policy is psp)`. The European PSX install: exit 0, `gap 352 / 4113`,
`DIV F ... off`.

## V3, live

Every route needed one change to run at all (below, the first defect); the
route and capture numbers are from the round tip plus main's two commits
`36ef8605` and `a70916a6`'s `exe_own.cpp`, built uncommitted and reverted.
The attract pair is the tip as is.

**V3.1, the attract sequence** (6 min, twice a side, the union of both
caches' `gate-skip` lists, 1,478 ranges): each side identical to itself on
all 11,393 ticks. PSP against PSX: 154 pages from tick 1, the same scenes in
the same order. Named from a `BOF3X_STATEHASH_DUMP` pair (ticks 1 .. 2359):
the English names (`NameTable_*`, LG's renames), `Char_ExpTable` (the PSP's
level table), `SimoonDust_Offsets` (IM), `Scena17_EndSteps` (the disc's own
credits line), the six Config controller names (`0x66A368`: no Config text
in PSP-EU `en-150`, LG's known defect) and the missing village words, the
VRAM shadow and CLUT strips (the stand-ins), `Snd_*` from tick 3 (the
PSP's effects), `Music_File*` (the PSP's songs). **Not named to a write:**
`MsgBox_PlacementTable+0x10` (2 bytes, the PSP image's `map` bytes differ
from the PC's and the PSX's); at the first field scene `Light_Angles`,
`Music_Track` and `0x904CD0` are zero on the PSP side where the PSX's are
set, so song 000 is opened for a moment before area 4's 006 (first cycle
only). The inference, not traced: `DEMO.DAT`'s PSP-own chunks 1, 2, 4
carry a demo state whose play clock is zero.

**V3.2, the routes** (`en-150`): on the tip **no route loads its save** -
see the defects. With the trial build: combat, shop, field_menu,
menu_screens, worldMapAndAreaTransition, dragonTransform, cutsceneAndNue
and whelpBoss finish on both sides; caughFish and masterAndManillo **Fatal
on both** at area entry. The PSX side agrees with itself but for the sound
page and tick 3. PSP against PSX, beyond the text, art and sound pages:
`0x903000` from the first field tick on every route; the shop from tick
2850 (`Game_Mode`, then `Party_Zenny` at 3061); cutsceneAndNue's battle
from tick 5074 (enemy records): the Nue fight goes differently (captured).
None is named to the byte: the PSX side of the route dump pair lost a race
with a stale log and was not re-run. No route reaches area 92 or 198.

**V3.3** the contact sheet `psp_eb/contact_sheet.png`. **V3.4**
`dialog_test.py` all ok; the options page and Jukebox captured on the three
real caches (`psp_eb/lcap/`). **V3.5** `BOF3X_MP3_COMPARE` on the PSP's 11
jingles: 11 the same shape, max |delta| 1, worst RMS 0.036.

## Defects (described, not numbered)

**Numbered 2026-10-12, night** (the coordinator's second pass): 1 fixed by `main`'s button-map rule, merged (no entry); 2 is **D253**; 3 **D251**; 4 and 5 **D252**; 6 **D256**; 7 **D257**. (The `DIV F` log lines above are DIV-0108's, as logged before the numbering.)

1. **The round tip lacks main's button-map rule**: `0x656AEC` (the field
   button map Boot_Task copies) is a gap on every Western disc image, so at
   the title only Start confirms and a recorded route never loads its save.
   Main fixed it (`36ef8605`); the tip predates it.
2. **Areas 16 and 45 crash on entry** on both disc-only installs:
   `MoveCmd_AttachOffset` (`field_objects.cpp:265`) reads a null `+0x0C`
   vector table of the area's descriptor.
3. **Battle camera shifted** on both disc-only installs against the PC
   install: the party lower right, the enemy off the left edge, its name box
   at the top left (combat, frame 480). The owner saw it live.
4. **Equipment slot icons all flames** on both (cutsceneAndNue's equip
   window), consistent with the `Equip_SlotIcons` gap.
5. **Item counts drawn as stray glyphs** (the `x 2` and `5/128` of the PC)
   on both, and the dragon list's AP costs all 0.
6. **The PSP title is the fishing version's logo**, its tiles misaligned.
7. The European PSX disc alone lacks 8 bank containers (above).

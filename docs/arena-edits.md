# The port's edits to the type-1 arenas, read, and what a disc-only build does

**Status:** MEASURED (2026-10-12, the disc-only round's group AR,
[`disc-only-round.md`](disc-only-round.md); every held PSX disc and the PC's
`DAT/` checked; not rendered, not played)

[`type1-compression.md`](type1-compression.md) section 3 found 14 type-1
arenas the port changed after decompressing them - eight `PL` arenas, `START`,
`BPLD012` / `015` / `016`, `BPLD27A` / `BPLU27A` - and left unread what was
changed. This reads them against the code that consumes them and builds each
from a disc: a US (or Japanese) disc alone now writes **all 741 `base/`
containers** (727 before), and the PC-plus-US build is unchanged byte for byte.

Hard rule 1: nothing below is a copy of a byte of the arenas. Offsets, counts,
script and frame numbers, and the size of each change are structure.

## 1. The arena, as the code reads it

| Level | Layout | Read by (ours, the PSX's code term for term) |
|---|---|---|
| arena (kind 0, tag `0x10000`, so `0x813580`; PSX `0x8003B800`) | three `u32` offsets from the arena, one **bank** per party member in the order the file name's three digits list them | `Field_MemberSprite` `0x533BA0` (PSX `0x801C224C`, `src/game/field_event.cpp`): sprite `+0x4C` = `0x813580 + dword[column]` |
| bank | four `u32` offsets from the bank: `[0]` the image table, `[4]` the palette, `[8]` the scripts, `[0xC]` the 3-byte frame entries | `Sprite_SetFrameQueueUpload` `0x5894D0` (PSX `0x8014DAFC`, `src/game/sprite_pose.cpp`) reads `[0]`, `[8]`, `[0xC]` |
| scripts | a `u16` offset table, one script per entry; the first entry is the table's own size | the same: `+0x50` = scripts + `u16[entry byte 1]` |
| script | `frames`, `steps`; `steps` x (hold ticks, frame) or a jump (`>= 0x80`); `frames` x `u16` offsets; the frames | `Sprite_ScriptStart` / `Tick` / `Step` ([`sprite-draw-order.md`](sprite-draw-order.md), "the animation script") |
| frame | a count byte, then that many 5-byte parts | measured: in the four edited banks, PC and disc, 1,236 of 1,236 frames end exactly at `1 + 5n` (the last of a script within 3 bytes of padding); the part's fields are not read in code here |

Every level is found through an offset the data carries. No address or offset
inside an arena is a constant of the exe: a scan of `BOF3.exe` for the
PC-only bank and block offsets (`0x30594`, `0x42D60`, `0x3042C`, `0x51A00`,
`0x51428`, `0x40488`, `0x42580`, `0x306F4`, `0xF428`) finds none, and the
only arena address in its code is `0x813580` itself (3 references).

## 2. What the port changed

Each edit lives in **one bank, the same bank byte for byte in every arena that
holds it**; the other two banks of every edited arena are byte-identical to
the disc's decode. In every edited bank the images, the palette and the frame
entries are unchanged: **only animation scripts changed.**

| Arenas | Bank | What changed (scripts of the bank, disc against PC) |
|---|---|---|
| `PL012` `025` `026` `247` `257` `267` `278` `27A` | member 2's (198,376 bytes on the disc, 198,024 on the PC; one hash each in all eight) | 10 of 69 scripts. Scripts 0, 4, 18-21, 34-37: of the 6,216 parts in frames that keep their part count, 757 change - 659 by +1 in the part's byte 1, 24 by +2, the rest part-order changes; scripts 18-21 lose their frames 5-7 (104 bytes each) - each has one step, showing one of frames 1-4 (3, 4, 2, 1), so on every build those frames are never stepped to; script 34's frame 4 loses one part (6 bytes with its padding); script 42 gains one 5-byte part in each of its frames 2-15 (+70). Net **-352** = 4 x 104 + 6 - 70, which is the 352 bytes `type1-compression.md` measured, and the scripts after them, the frame entries and (where member 2 is not the last bank) the arena header move down by it |
| `BPLD012` `015` `016` | member 1's (a fixed 0x1B000 bytes; one hash each in all three) | Script 1 of 17 gains one part in its frame 6 and one in its frame 15 (+10); script 16, the last, drops 2 bytes of trailing padding; the frame entries move up 8, out of the bank's zero tail. The "1,949 words, deltas -16 / -8 / +8 / -0x1000 / +10" of `type1-compression.md` are the moved offset tables and the bytes behind the insertions - no coordinate was adjusted |
| `BPLD27A`, `BPLU27A` | member A's | The bank holds **two identical scripts** of four frames, each step held 4 ticks (`BPLD27A` scripts 0 and 4, `BPLU27A` scripts 0 and 1); the port held the **second** one 8 ticks a step. The 4 bytes are those holds |
| `START` | all three | not an edit of `START`'s own arena: the PC ships `PL012`'s PC arena there (section 5) |

What each edit is for is not established: none was rendered. The PL and BPLD
edits move single parts of single frames by a pixel-sized step, add parts and
drop unstepped frames - hand edits of member 2's field animations and member
1's battle animation, not a mechanical transform: no rule reproduces them
(the +1s are confined to 10 scripts and are not every part even there). The
27A edit makes the second copy of a loop play at half speed.

## 3. Do our consumers run the unedited data?

Yes, by every measure taken. The functions that read the arena are the
PlayStation's own, read term for term against their twins
([`sprite-pose.md`](sprite-pose.md), [`field-event.md`](field-event.md)), and
the PlayStation runs these very arenas unedited; every one of the 51 arenas
the port left alone goes through the same code; every offset is read from the
data (section 1); no script step of either version names a frame past its
script's frame count (0 of all steps in the four banks). Size: the largest
unedited arena (`PL025`, 408,916 bytes) is 352 bytes longer than the largest
PC arena (408,564), so it ends at `0x877354` rather than `0x8771F4`; the
PlayStation loads the same bytes to the same place of its map
(`0x8003B800`), and `symbols.toml` names nothing in `0x870000..0x880000`. The
coordinator's state-hash gate (round section 4) is the run that would show a
consumer that thinks otherwise.

So [`psp-only-build.md`](psp-only-build.md)'s warning - a disc's unedited
arena as a crash - does not apply: these edits change what is drawn, not the
layout any code depends on.

## 4. The form each takes

| Edit | Form | Why |
|---|---|---|
| `BPLD27A`, `BPLU27A` | **a rule**: transform `hold` - decode, then in the third bank the second of two identical four-step loops held 8 ticks a step (`importer.py` `hold_loop`) | derivable from the disc's own data by structure (find the two identical loops, take the second, the hold `4 -> 8`); byte-identical to the PC's chunk from all five PSX discs, so the recipe lists them as sources (`on = [psx-jp:hold, ...]`, class `pc-hold`) like `ryud` - and no ledger entry is owed, a disc-only build has the PC's bytes |
| the 8 `PL`, `BPLD012` / `015` / `016` | **the disc's arena as is**: an `own` stand-in (`own_why = "arena"`), each PSX disc's same-named type-1 section, decoded | not derivable (section 2), and the consumers run the unedited data (section 3). A recipe of the edit would have to carry the parts the port added and the per-part steps - copies of the port's values, which rule 1 keeps out. The difference is ledgered (section 7) |
| `START` | **a rule**: `START`'s arena is `PL012`'s (`own_why = "start-arena"`, `ARENA_FROM`) - from a disc, the disc's `PL012` section decoded | section 5 |

The PC install, when it is a source, still gives all 14 its own bytes: an
`own` row is used only when no listed source carries the chunk.

## 5. `START`

`START.DAT` is file `0x31D` of `Dat_FileNames` (`0x64F368`, entry 797 the
string `START.DAT`), loaded once, by `TitleMode_Load` `0x588EB0`, task 0's
boot mode ([`shop_states2.md`](shop_states2.md),
[`attract-mode.md`](attract-mode.md) section 2). The disc's own `START`
arena (245,444 bytes) holds the same three members as `PL012` but smaller
sets: images 21 / 31 / 18 against 29 / 36 / 37, scripts 87 / 51 / 42 against
106 / 67 / 69, different palettes - 82 / 39 / 30 of its scripts equal `PL012`'s
at the same index. The PC ships `PL012`'s whole PC arena in its place. Which screen,
if any, draws from the arena while `START` is the last arena loaded was not
traced, so the port's reason is not known; the form keeps the port's choice
(the same arena as `PL012`) rather than the disc's smaller set, so that no
code of the PC's that asks the arena for one of `PL012`'s animations finds it
missing. From a disc alone `START` is therefore byte-identical to that cache's
`PL012` (and so carries section 2's member-2 difference too).

## 6. Measured

`tools/importer.py recipes` regenerated from the PC `DAT/` and all seven held
discs (the generator unchanged otherwise: before this change it reproduced the
committed recipe exactly but for the date); the recipe's diff is the 14 rows
and the header. `importer.py check`: 742 files, 3,582 chunks, 0 errors.

| Build | `base/` containers written | `verify` against `fixtures/pc-zh.DAT.files.tsv` |
|---|---:|---|
| US disc alone, before (`6e6b4f3b`) | 727 of 741 | 474 byte-identical; the 14 not written (`pc-edit`) |
| US disc alone, now | **741 of 741** | 476 byte-identical (+ the two 27A), 12 holding a disc's own arena, 0 problems |
| JP disc alone, now | **741 of 741** | 477 byte-identical, 11 holding a disc's own arena, 0 problems |
| PC `DAT/` + `BOF3.exe` + US + PSP-EU + PSP-JP | 741 | every one of the cache's 2,125 files byte-identical to the same build at `6e6b4f3b` |

742 containers in all; `AFLDKWA.DAT` has only a `loc/zh-CN` chunk, so 741 is
every `base/` container. Own chunks used from the US disc alone: 41 before, 53
now (the 11 arenas and `START`). The PSP discs are not stand-ins (`OWN_FROM` is
the PSX discs): the PSP ships these arenas decompressed, and 3 of the 13
(`BPLD012` / `015` / `016`) also carry the PSP's own change
([`type1-compression.md`](type1-compression.md) 2); a PSP-only build still
lacks the 14 ([`psp-only-build.md`](psp-only-build.md)).

Self-tests of the consumers read (no code changed; the record that nothing
moved), from an ini-less copy of this branch's build:
`BOF3X_SHADOW=sprite_pose,sprite_anim,field_event,battle_sprites`, exit 0,
`inject: 10087 ours, 0 left original`, `self-test only: done`, 37 of 37
result lines `0 MISMATCHES`.

The disc-only verify's one other problem line, `opt/area4-walls AREA004.DAT
slot 6`, is the same at `6e6b4f3b`: not this change's.

## 7. Ledger entry (draft)

**DIV-0096** - a disc-only cache's party animation scripts.

- **Subsystem:** the arenas as imported (`tools/importer.py` `own_why =
  "arena"` / `"start-arena"`, `recipes/pc-zh.toml`); drawn by
  `Sprite_SetFrameQueueUpload` and the script functions.
- **Original behaviour:** the PC port ships its own edit of 10 of member 2's 69
  animation scripts in the 8 `PL` arenas that hold member 2 (parts moved by
  one or two in their byte 1, parts added, unstepped frames dropped) and of 1
  of member 1's 17 battle scripts in `BPLD012` / `015` / `016` (two parts
  added); `START` carries `PL012`'s arena with that edit.
- **New behaviour:** a cache built without the PC's `DAT/` takes each of
  those 11 arenas from the player's PSX disc, decoded as the PlayStation has
  it, without the port's edit; `START` takes that disc's `PL012` arena. With
  the PC's `DAT/` among the sources nothing changes.
- **Why:** the edit is the port's hand-made content, not derivable from the
  disc; carrying it would mean carrying the port's bytes (hard rule 1,
  [`LICENSING.md`](LICENSING.md) section 3). The consumers are the
  PlayStation's code and run the unedited data (section 3).
- **Visible?** Not seen: member 2's field sprites and member 1's battle
  sprite may differ from the PC's by a part here and there in those
  animations. Rendering the two side by side is the open item.
- **Reversible?** build with the PC's `DAT/` as a source.

The 27A rule owes no entry: its output is the PC's chunk, byte for byte.

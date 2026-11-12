# TownWarp: a warp to a visited town from the world map

**Status:** IN PROGRESS - design verified against the patched US disc and our
source 2026-11-12; played by the owner under the hack's version 1.0 the same
day ([`owner-review.md`](owner-review.md)). **Sections 1 to 6 describe version
1.0 of the hack; version 1.4 changes four rules, and section 7 has them. The
code is 1.4's since `romhack/definitive-1.4`; 1.4's rules are not played.**

The Definitive Edition romhack (TheGreatBen, used with permission; issue 216,
[`romhack-mods.md`](romhack-mods.md)) adds a field ability that, used on the
world map, opens a list of the towns the party has visited and moves the
party to the one chosen. Its readme does not name it. This is behaviour
`TownWarp` ([`mods.md`](mods.md) 11), ledger entry DIV-0225. The reading it
starts from is second-hand (`D:\bof3x-runs\de1112\reading\game_summary.md`,
`TownWarp`, Q1, Q2); every point below was measured again for this note
(scripts `D:\bof3x-runs\de1112\b6\`, which print code and stay there). No text
of the hack and no byte of the game is in this file: area ids, offsets and
counts only.

## 1. The chain, as verified

| Step | PlayStation (patched US disc) | Ours (PC) |
|---|---|---|
| Use | START.EMI 8: slot 0 of the field-ability handler table (`0x801ED224`; the release's handler `0x801E792C` answers 3, "not here") points at GAME `0x8019C304`. The dispatcher (`0x801E7D20`, the twin of `FieldAbility_Use`; the same index rules, `0x6F` falls to slot 0, the same "1 or 5 takes the AP cost") calls it with the ability in `s0`. Ability `0x6F` with `Field_InputFlags & 0x21` equal to 1 (the world map, bit 5 clear): "asked" (`0x801AE3E4`) = 1, answer 1. Anything else: answer 3, the release's | `FieldAbility_Use` (`field_ability.cpp`): with the behaviour on and ability `0x6F` (index 0), the same test and answers in place of handler 0's; every other ability unchanged |
| Who learns it | Data, not code: the mod's carried `exp_table` gives ability `0x6F` at one level step (record 112, `ability_a`); the release's table gives it nowhere. Its `abilities` record has one carried cell | Carried by the mod's table cells (`opt\mod-<id>\exe\tables.toml`), unchanged by this work |
| Open | GAME `Field_ModeTailKinds` slot 0 chain (the field frame with no tail armed): `0x801AE340` -> `0x801BB81C` -> `0x801B5860` -> `0x8019C244`. On the world map (`Field_InputFlags & 1`): with "menu open" (`0x801AE3E5`) set, the menu's frame (below); else with "asked" set: flags `& 0x21` not 1 - asked cleared; `Field_Request` not 0 - kept for a later frame; else cleared and `0x801B57F8`, which loads the hidden file at `0x80016000` (MTEST's file entry pointed at LBA `0x2DF24` for one load) and calls its `+0` | `FieldIdle_Tick` (`field_7.cpp`), cases in the hack's order; the data comes from the mod's layers (section 4), not from a file loaded at the moment |
| The menu | Hidden `+0`: page, "rebuild", "chosen" and the saved pointer 0; `ScriptFlags_Set40`; the page built; "menu open" = 1. The page: the question message copied to a buffer, then for towns 0..10 whose visited bit is set, after skipping `page` of them, up to 4 names; a fifth visited town adds "more" (`0xFE`) and stops; "cancel" (`0xFF`) always. The question ends in a choice control (`0x14`, its id, a byte, `0x10`); the last byte becomes `count | 0x10` (the list form, `count` the entries with "more" and "cancel"). Each entry's town index (or `0xFE`/`0xFF`) kept. The standing area's descriptor `+0x34` saved and pointed at a two-entry table of the hack's answer handler (skipped when the descriptor is null). The message pointers `0x801490AC`/`B0` = the buffer, the message word `0x801490A8` = 0, `MsgBox_Reset` (US `0x80150304`), `Field_Request` = 2 | The same page into a buffer of ours; `Msgbox_Base` and `MenuWindows_ListText` (PC `0x7DEE4C`/`50`, the twins at the same offsets of the box's state) the buffer, the message word `0x7DEE48` 0, `MsgBox_Reset`, `Field_Request` 2. No descriptor is written: `CommitChoice` (`msg_box.cpp`), the one reader of `+0x34`, calls our answer while the menu holds the box |
| The answer | Hidden `0x800162C8`: the answer byte (`0x801490C7`, s8; PC `AreaW0c_ChoiceAnswer` `0x7DEE67`, the same offset) not negative: its entry - `0xFF` nothing; `0xFE` page += 4 and "rebuild" = 1; a town - "chosen" = town + 1. Always the message word `0xFFFF` (no message follows) | The same, in `CommitChoice`'s place |
| The menu's frame | Hidden `+8` (from the world-map case while "menu open"): `Field_Request` 2 - return. Else the descriptor put back; "rebuild": cleared, the page built again (a new message), return. Else "menu open" 0, `ScriptFlags_Clear40`; with "chosen": cleared, the town's row of an 11-row table (area u16, flags u16, x and z 16.16), scratchpad `0x1F800001` = 0, `Field_ChangeArea`(area, x, z, flags) (US `0x8019FA28`), then `0x80143F1E` = 0 and `0x80143F1D` = `0x0C` | The same; `SharedScratch.b_01` (`0x903851`), `Field_ChangeArea` `0x594E00`, `ModeFlow_AreaTransition` (`0x904EE0`) 0 and `Area_EnterTransition` (`0x937F98`) `0xC` |

**The arrival bytes are the world map's own town entry.** The release's
world-map walk, on a town cell (`0xAF`), calls `Field_ChangeArea` with the
pending link and then stores `0x80143F1E` = 0 and `0x80143F1D` = `0x0C`
(GAME `0x801B15F0..0x801B1608`, original disc); area 104's and area 121's
copies do the same (AREA104 `0x801F4EB8..0x801F4ED0`). Our twins of those
sites (`field_4.cpp` `Field_LeaderStepLands`, `Area104_LeaderStep`,
`Area121_LeaderStep`) store `ModeFlow_AreaTransition = 0` and
`Area_EnterTransition = 0xC` at the same points. `0x80143F1D` is also the
byte `Area_ClassifyPending` sets to `0xB` or 1 (GAME `0x801A02E8`, PC
`0x595350`). Scratchpad `0x1F800001` is where `Area_LinkAt` puts bits 4..6
of a link's flags (GAME `0x801A0264`; PC `0x5951D0` writes `0x903851`). So
the warp arrives exactly as a walk into a town from the world map does, with
the hack's entrance in place of the cell's link.

**The visited mask.** A 32-bit word at save-block `+0x10AC` (PSX
`0x80145984`, PC `0x904A8C`), written and read only by the hack's code:

- `0x8019C244`, every idle field frame **off** the world map: the first
  town `i` (0..10) whose area id (GAME `0x801CA6EC`, 11 halfwords) equals
  `Game_AreaNumber` sets bit `i`.
- `0x801BB81C`, every idle field frame anywhere: when `AreaW0b_PartySet`
  (block `+0x74C`) is not `0xFF`, its low seven bits are at least 7 and not
  12, and bit 31 is clear, the word becomes exactly bit 31 - every town
  forgotten, once. This is the hack's story lock: the towns visited before
  that point leave the list.
- The menu reads bits 0..10 (the hidden file's page build). Nothing else
  addresses the word: every EMI section and the boot program of both discs
  scanned for a `lui`/offset pair to it give none on the original and, on
  the patched disc, the two routines above alone (`b6\scan.py`).

**Order in the idle frame** (the hack's jumps): CampMasters' request
(DIV-0223), TownWarp's forgetting, AllyAnywhere's request (DIV-0217, which
ends the frame's chain when it raises one), TownWarp's recording or
world-map step.

## 2. The destinations

By town index 0..10 the town list's areas and the warp table's areas are the
same: 1, 0, 37, 50, 21, 57, 74, 58, 155, 143, 147. Each table row adds an
entrance (x, z in 16.16) and a flags byte (values 1, 3, 5 and 7 here: no
bit 7, so `Field_ScriptFlags2`'s bit 15 is cleared on arrival, and bits 4..6
clear). The names, the question and the two labels are the hack's English
text, carried by the mod (section 4).

## 3. What the reading said, and where it was wrong

Every point of the reading holds, re-read from the patched listings (GAME
`0x8019C244..0x8019C334`, `0x801B57F8`, `0x801BB81C`, START `0x801E7D20`,
the hidden file whole), with one correction: the message pointers are
`0x801490AC`/`0x801490B0` and the message word `0x801490A8` (the code's
`lui 0x8015` with negative offsets), not `0x8014B0AC`/`B0B0`/`B0A8`. The
reading's open items are settled: `0x80143F1D` is the PC's
`Area_EnterTransition`, `0x80143F1E` its `ModeFlow_AreaTransition`, and the
scratchpad byte the PC's `SharedScratch.b_01` (section 1); ability `0x6F` is
learnt through the mod's carried `exp_table`.

## 4. The mod's data: kind 21 in `FIRST.DAT`

The question, the labels, the names and the table are the hack author's, so
they come from the player's built mod, never from this repository.
`tools/disc_patch_build.py` reads them on the player's machine from the
patched disc: the file outside the directory whose one section loads at
`0x80016000` (`outside_files`, `outside_sections`) and the town list in the
patched GAME section 0. The reader is bound to this hack's code: it takes
each address from the instruction that loads it (the opcode and registers
checked, at fixed offsets of the file) and the town count from both loops'
bound, and refuses anything else. Built into a chunk kind of ours, **21**,
in `FIRST.DAT` (the file the title loads, walked through every layer):

| Layer | Chunk | Payload |
|---|---|---|
| `opt\mod-<id>\dat\FIRST.DAT` | kind 21, tag 0: the table | version 1, the count `n` (11), then `n` rows of 16 bytes: the town's area id (u16, the visited test), the destination area (u16), the flags (u16), 2 zero bytes, x and z (s32, 16.16) |
| `opt\mod-<id>-<tag>\dat\FIRST.DAT` | kind 21, tag 1: the text, in the PC's encoding of the disc's language (`loc_build.convert_run`, in the text builder's child while its language is set up) | version 1, `n`, then `n + 3` strings, each a u16 length and its bytes: the question (ending in its 4-byte choice control, no NUL), "more", "cancel", the `n` names (each with its NUL) |

The engine passes over kind 21 unless `TownWarp` is on; on, it checks and
keeps each payload (`TownWarp_Take`), a `Fatal` on a payload that is not
this shape. After `FIRST.DAT`'s walk, `TownWarp` on and either chunk missing
is a `Fatal` that names the missing chunk and says to rebuild the mod: the
game stops at the title, before any menu could be drawn empty. Text and
table are checked against each other (the same `n`, at most 31 towns; the
longest page fits the buffer).

`report.toml` `[carried]` gains `town_warp_towns` (the count; 0 when not
carried), and the warp's file leaves `[not_carried]`'s `outside_files` /
`outside_bytes`. The Mods tab says `a town warp to N towns`. Only a mod
whose `needs` names `TownWarp` carries it; such a mod whose patched disc has
no such file this reader recognises does not build.

**As built on the real patch, offline (2026-11-12, a copy of the proof's
scratch install, `D:\bof3x-runs\de1112\b6\home`):** `mods.py build` exit 0
in 107 s; `mod-definitive\dat\FIRST.DAT` holds kind 21 tag 0 (178 bytes),
`mod-definitive-en-US\dat\FIRST.DAT` gains kind 21 tag 1 (178 bytes);
`report.toml` `town_warp_towns = 11`, `outside_files` 15 -> 14,
`outside_bytes` 137,216 -> 133,120, `text_chunks` 345 -> 346; the Mods tab's
line ends `a town warp to 11 towns`.

## 5. What is saved

- **The mask** rides in the save block at the hack's offset (the owner's
  decision): `+0x10AC..+0x10AF`, the block's last four bytes. Nothing of
  ours or Capcom's addresses them: `docs/save-block.md` lists `+0x1041..`
  as UNTOUCHED, `analysis/pc_xref.json` has no reference into
  `0x904A1D..0x904A8F` (DIV-0191's evidence) and none at
  `0x904A8C..0x904A8F` past it, and our source has none. The save editor
  (`src/launcher/save/block.cpp`) lists it as an advanced field ("Town warp:
  towns visited", hex), the untouched range shortened by four. A memory-card
  conversion keeps it both ways: on the PlayStation it is the hack's own
  word with the same meaning, and the release never reads it. A save made
  with the behaviour on, loaded without it: the word is inert (nothing
  reads it); with it on again, the list is as it was. A save made without
  it, loaded with it: the word is 0 (new games zero the block), so no town
  is listed until one is entered.
- **The menu's state** (asked, open, page, rebuild, chosen, the entry ids,
  the "box is ours" flag and the built message) is statics of ours, as the
  hack's are RAM: not in the save, cleared at every game's start, in the
  quicksave participant `BEHAVIOURS` (version 4, the message bytes with
  them, since the box points into them); a record taken with the menu up is
  refused when the behaviour is off.

## 6. How the PC differs

- The PlayStation loads the warp's code, text and tables from a hidden file
  at the moment of use. The PC has the code as ours and the data from the
  mod's layers, taken at the title.
- The hack points the standing descriptor's `+0x34` at its handler; ours
  answers in `CommitChoice` while the menu holds the box, so no game data is
  written, and a null descriptor (where the hack skips the swap and a choice
  would read through it) is no case.
- The text is the mod's text layer's, converted to the PC's encoding.

## 7. The hack's version 1.4 (2026-11-12)

The author's 1.4 was applied to the same US track and compared with the 1.0
track file by file and section by section (scratch `D:\bof3x-runs\de14`:
`tools\cmp_10_14.py`, `tools\dis3.py`, which print code and stay there).
2,734 bytes of the track differ. For the warp:

| | Version 1.0 (sections 1 to 6) | Version 1.4 | Ours |
|---|---|---|---|
| Who learns it | the mod's `exp_table` record 112 names ability `0x6F` | that cell is the release's again; GAME `0x801BB81C` is a jump to new code in the boot program, `0x80156228`: while the word's bit 31 is clear, the forty bytes at `0x80144C54` are searched for `0x40`; found, the word becomes `0x80000000` and the ten bytes at `0x80144A72` take `0x6F` in the first empty one, unless one holds `0x6F`; then `0x801BB85C`, the chain as before | `FieldIdle_Tick`: record 4's `abilities` (all four lists) and record 1's `abilities[1]` |
| The two addresses | - | the hack's record base is `0x80144968` (its level catch-up loads it, START `0x801E3450`), lists at `+0x5C + 10 * type`: `0x80144C54` is record 4's four lists, `0x80144A72` record 1's type-1 list | `CharRecord::abilities` at `+0x60`, the lists `AbilityList_ForType` `0x591EC0` addresses |
| Recording | every idle field frame off the world map | the same loop, and the bit is set only when the word's bit 31 is set (`0x8019C294`) | the same test |
| Forgetting | the party set at least 7 and not 12, bit 31 clear: the word becomes bit 31 alone | that code (`0x801BB824..`) is not reached; the word is cleared once, when the warp is learned | the party-set test removed |
| Arrival | `0x80143F1E` = 0 and `0x80143F1D` = `0x0C` after `Field_ChangeArea` | `0x80143F1E` = 0 alone (the hidden file is 8 bytes shorter) | `Area_EnterTransition` is not stored |
| The town list | GAME `0x801CA6EC` | `0x801CA6F0`, the same 11 areas | the builder reads the address from the instruction; no change |
| The hidden file | LBA `0x2DF24`, `0x485` bytes | the same LBA, `0x47D` bytes; the table and the text are the same bytes | the built kind-21 chunks are the same bytes as 1.0's |

Bit 31 of the word is "the warp is learned" from 1.4 on. The readme's new
line says Nina learns Warp at level 14 and that the list resets over the
time skip; the code teaches it by the test above, not by a level. Which
character and ability records 4 and `0x40` are was not looked up (the
owner's or the author's to say).

**A save played under 1.0's rules:** the towns it recorded stay listed; no
new town is recorded while bit 31 is clear; the moment above clears the list
once. A save whose bit 31 the 1.0 forgetting set records towns at once and
is not taught `0x6F` by the new code; a member who learned it at 1.0's level
step keeps it.

# CampMasters: a master's talk from the camp menu's master list

**Status:** IN PROGRESS - design verified against the patched US disc and our
source 2026-11-12; code on `romhack/b4-masters`; not played (no game was
started: a proof chain was running on the machine).

The Definitive Edition romhack (TheGreatBen, used with permission; issue 216,
[`romhack-mods.md`](romhack-mods.md)) lets the player talk to a master from
the master list instead of only reading the master's entry there. Its readme
says masters "can also now be accessed from the master tab in the camp menu".
This is behaviour `CampMasters` ([`mods.md`](mods.md) 11), ledger entry
DIV-0223. The reading it starts from is second-hand
(`D:\bof3x-runs\de1112\reading\game_summary.md`, `CampMasters` and the
answers Q1 and Q2); every point below was measured again for this note.
Scratch scripts: `D:\bof3x-runs\de1112\b4\` (they print game data and stay
there).

## 1. The chain, as verified

Where the list itself is reached from in play (which screen shows the master
list) is the owner's to say; the code facts are these.

| Step | PlayStation (patched US disc) | Ours (PC) |
|---|---|---|
| Entry | SHOP.EMI `0x801E2150` (`ShopBrowse_ChooseSteps[1]`): 16 bytes at `0x801E2394` replace the confirm's two sounds: `0x801AE318` = the entry under the cursor + 1 (the byte the `0xFF` test reads), then `j 0x801E2410` - the cancel path's two sounds (`0x106`, `0x102`) and its close. No detail page. An entry of `0xFF` still plays `0x107`. | `ShopBrowse_Choose` `0x5833E0` (`src/game/shop/shop_browse.cpp`), its confirm branch: the same, the pending byte a static of ours. The list closes as on a cancel (`ShopBrowse_End`, ShopMode 1, the field). |
| Request | `0x801AE340`, slot 0 of `Field_ModeTailKinds` (GAME.EMI `0x801C84BC`; the release's word is a bare return): pending not 0 - pending 0, "active" (`0x801AE319`) = it, `ScriptFlags_Set40`, tail kind 4, tail state 0, tail argument = entry. Then the chain goes on (`j 0x801BB81C`): it does not end the frame's chain. | `FieldIdle_Tick` (`src/game/field/field_7.cpp`), first case, the hack's order. `Field_ModeTailRun` now reads the kind once, as the original's `jmp [table + 4 * kind]` does, so the armed tail runs from the next frame, as on the PSX (there the hook is slot 0's function). |
| Talk | Tail kind 4 (`0x801A7CF0`, PC `FieldTail_LoadBank` `0x56D930`): state 0 loads SISYOU, state 1 starts the talk (`0x801D0C18`, PC `MasterTalk_Reset` `0x585A00`), state 2 runs it, state 3 reloads the party set's bank, state 4 clears `ScriptFlags_Clear40` and the tail. | The same function, unchanged but for the two swaps. |
| Swap in | State 1: `jal 0x801D0C18` -> `jal 0x801AE984`. With active set: the RAM block at `0x80010000` (0x1540 words) copied to `0x801D8000`; a hidden EMI (section 2) loaded over `0x80010000` by pointing MTEST's file entry at its LBA for one load; then `0x801CA650` (code stored in the free shop records): the current area's descriptor `+0x34` saved and pointed at the copied table, unless the descriptor is null; then the original call. | State 1, after `Gfx_ClutStripCopyRow(2)` and before `MasterTalk_Reset`: the home area's text block landed over the arena (section 4) after the bytes it covers are saved, and from then on the choice table and the dialogue-button test read the home area's (section 4). |
| Swap out | State 3: `jal 0x8016728C` -> `jal 0x801AE39C`. With active set: active 0, the block copied back; then `0x801CA6C8`: the descriptor's `+0x34` restored; then the original call. | State 3, after the party set's store and before `Snd_LoadBankFile`: the saved bytes put back, the reading row's side chunk of the current area put back, the flags cleared. |
| End | State 4: the field resumes. | The same: **the talk returns to the field**, not to the menu. |

The PlayStation's GAME words were read in `bundles\game\GAME_s00_801AE2B0_region.md`
(the region `0x801AE2B0..0x801AEBA0`), SHOP's in
`bundles\small\SHOP_s00_801E2150.md`; the reading agrees with both on every
point above.

**Master id = list entry.** The tail argument the hack writes is the list
entry, and the release's own talks arm the same tail with the same number in
the same areas: our home-area triggers set `AreaW0c_TailSub` 0 (area 3), 1 and
2 (areas 61, 55, through their callers), 3 (68), 4 (50), 5 (59), 6 (113), 7
(74), 8 (116), 9 (98), 10 (41), 11..14 (37, `Area37_Trigger45..48`), 15
(143), 16 (91). That is the reading's home table, found independently.

## 2. The 14 hidden files, measured

`tools/disc_patch_build.py` `outside_files` / `outside_sections` on the
patched track (`b4\verify_hidden.py`): 15 EMIs outside the directory, 14 with
one section at `0x80010000` and one at `0x80016000` (the town warp's). GAME's
LBA-offset table (`0x801AE31C`, 18 halfwords) gives each list entry its file;
entries 11..14 share one (area 37's). For all 17 entries:

- **The text block, byte for byte: holds.** Each hidden section's first n
  bytes equal the home area's `0x80010000` section of the patched disc
  (n its size, 0x79F..0x3333 bytes), and equal no other area's.
- **The rest of each file** (0x141..0x9EF bytes) is code and a table: the
  choice table at the offset GAME's second table gives (`0x801CA628`), whose
  entries point into the copied code. Compared with the home area's
  descriptor `+0x34` table in its own overlay (section 13 at `0x801F2C00`),
  instruction by instruction with branch, jump and `$at`-relative
  immediates masked (data the copies moved with them): **entry i of the copy
  is entry i of the home table**, for the whole length of the home table
  (5, 5, 5, 7, 5, 5, 5, 5, 5, 6, 15, 22, 13, 12 entries; area 91's copy has one
  more, a second pointer to its entry 5, past the table's end). The copies are
  not trimmed: the hack copied each table whole, with the data its handlers
  read.

So no new data is needed, and on the PC, where every area's code and
`.data` are in the image at once (`Area_Descriptors` `0x667590`, 200
pointers, each descriptor's `+0x34` table there), the home area's own
handlers stand in for the copies.

## 3. Per master: what the talk asks

The talk (`MasterTalk_*`, `MasterQuit_*`, `MasterJoin_*`, `MasterScreen_*`,
`src/game/menu/master_talk*.cpp`, `master_quit.cpp`) opens the master's
messages `base + 0 .. base + 0x11` (`base` the u16 at `0x6644E8 + 2 * master`)
and master 9's `0x2E`, all from the arena's text block, and reads party and
story state only - with one exception, the leave pose (below). Its messages
raise five choice ids each (control code `0x14`; `b4\master_choices.py`,
identical in the zh-CN, en-US, en-150, fr-FR, de-DE and ja-JP layers): ids
`a, a+1, a+2, a+3, a+4` where `a` is 0 for most, 2 for master 3, 4 for master
10 and `5k` for area 37's four (master 9 raises the last four only). The
handlers (our C++, `Area_Descriptors[home] +0x34`), and what they touch:

| Entry | Home | Handlers | State touched | Verdict |
|---|---|---|---|---|
| 0 | 3 | `Area03_ChoiceMessage42`, `Area143_ChoiceYesMark3`, `Area143_ChoiceYesMark5`, `Area03_ChoiceMessage44` | the message word, the talk's two step bytes | works |
| 1 | 61 | `Area61_ChoiceClearZenny`, the two `YesMark`, `Area61_ChoiceMark4` | message, step bytes, `Party_Zenny` | works |
| 2 | 55 | `Area55_ChoiceTakeItem8`, the two, `Area55_ChoiceMark24` | message, step bytes, the inventory | works |
| 3 | 68 | `Area68_ChoiceAskCount` (DIV-0219's), the two, `Area68_ChoiceAnswer84` | message, step bytes, the inventory's count | needs care: the leave pose; area 68 is a dialogue-button area |
| 4 | 50 | `Area50_ChoiceAsk82`, the two, `Area68_ChoiceAnswer84` | message, step bytes, a story byte | works |
| 5 | 59 | `Area59_ChoiceMessage2or4`, the two, `Area61_ChoiceMark4` | message, step bytes | needs care: the leave pose |
| 6 | 113 | `Area113_ChoiceAsk`, the two, `Area61_ChoiceMark4` | message, step bytes, `FieldE1_Rank` | works |
| 7 | 74 | `Area74_ChoiceAsk92`, the two, `Area74_ChoiceAnswer94` | message, step bytes, a learnt-ability bit | works |
| 8 | 116 | `Area116_ChoicePay10000`, the two, `Area61_ChoiceMark4` | message, step bytes, `Party_Zenny` | works |
| 9 | 98 | the two, `Area98_ChoiceAsk2F` | message, step bytes | needs care: the leave pose |
| 10 | 41 | `Area41_ChoiceTrade`, the two, `Area41_ChoiceConfirm64` | message, step bytes, the inventory | works |
| 11..14 | 37 | `Area37_ChoiceAsk5E` / `72` / `86` / `9A`, the two, `Area37_ChoiceConfirm60` / `74` / `88` / `9C` | message, step bytes | works |
| 15 | 143 | `Area143_ChoiceAsk52`, the two, `Area143_ChoiceAsk54` | message, step bytes, `Gene_Bits` | works |
| 16 | 91 | `Area91_ChoiceTakeItem59`, the two, `Area91_ChoiceMessage7` | message, step bytes, the inventory | works |

No handler a talk raises reads an area's statics, objects or map: each
writes the message word and the talk's bytes and reads or writes global
state (zenny, inventory, story bytes). The messages they choose are
`base`-relative ids of the same block. The tables' other entries (area 3's
map cells, area 41's drop-ins and cells, area 91's object states) are never
raised by a master's messages.

**The leave pose.** `MasterScreen_State6Leave` (`0x5876F0`, the talk's last
state) gives `Sprite_Objects[byte 0x90384B]` the state byte
`0x66459C[master]` unless that is `0xFF`: masters 3, 5 and 9 have one (3, 3,
6). In the home area the byte is the master's own sprite, set by the area's
script; from the camp it is whatever the standing area's scripts left in a
general script variable - another object, or (ours aborts past 30 records)
past the array. The hack does not touch this (SISYOU is unchanged on its
disc), so on the PlayStation it writes an unrelated object's state. **Ours
skips the pose during a camp talk**: the master's sprite is not in the area.
A deliberate difference from the hack, in the ledger entry, for the owner to
confirm.

**Dialogue buttons.** DIV-0140's cue test asks whether the standing area is
one of nine whose dialogue names a button (`text_draw.cpp` `InButtonArea`);
area 68 is one. During a camp talk it asks about the area whose text is in
the arena, so the test follows the text, as it does in the home area.

Verdict counts: **14 work, 3 need care (handled), 0 cannot yet** - all 17
are built.

## 4. The PC's swap

- **The text block** is the kind-0 chunk of tag 0 of the home area's DAT
  (file `home + 3`, `Area_Enter`'s rule), which lands at `MessagePools`
  (`0x803580`, the PSX's `0x80010000`) through `LoadDatFile`'s whole
  precedence: the shipped file or the cache's, the language layer, every
  optional layer (DIV-0005, DIV-0086, DIV-0089) - so the mod's own text and a
  reading-row layer's side chunk (kind 18, tag 0) come too, and a language
  layer's corrected block (kind 19, tag 0, DIV-0203) by its switch. Only
  those three kinds at tag 0 are taken; every other chunk (the map, the
  images, the banks, the language layers' enemy names at `0xC2xxx`) is
  skipped. `dat_load.cpp` walks the file twice: once to measure the largest
  such chunk, then to land them, after the arena's bytes they will cover are
  saved (at most `0x8000`, the text room `loc_build.py` builds to; more is
  refused).
- **The reading row's side chunk** of the current area (tag 0) is set aside
  before the walk and put back at the swap out (the home's own, if any,
  retired); a stale one is caught by its own hash test and dropped, never
  drawn wrong.
- **The choice table**: `CommitChoice` (`msg_box.cpp`) reads
  `Area_Descriptors[home]` instead of the standing area's while the home's
  text is in place. The hack writes the standing descriptor's `+0x34`
  instead; with one reader of `+0x34` in the program the effect is the same,
  and no game `.data` is written.
- **A null descriptor or no text chunk** for the home area is refused
  (`Fatal`): every one of the 14 has both.

## 5. What is saved and what is not

The four statics (pending, active, the swap's flag and home area) and the
saved bytes are ours: not in the save (no save can be made between the
choice and the talk's end: the list closes to the field, the next frame arms
the tail and `ScriptFlags_Set40` holds the field until state 4), cleared at
every game's start (`GameMode_Start`), and in the quicksave participant
`BEHAVIOURS` (version 3: the bytes with them), so a quick-load inside a talk
puts back the moment it took. A record taken during a camp talk is refused
when `CampMasters` is off. The master's met, intro and pupil flags are the
game's own and change exactly as in a home-area talk.

## 6. How the PC differs from the PlayStation here

- The PlayStation keeps one area's code and text at a time (an overlay at
  `0x801F2C00`, a text block at `0x80010000`), so the hack ships a copy of
  each home area's text and handlers and loads it over the current one. The
  PC links every area's code and `.data` into the image: only the text block
  is per-area at run time, so that is all that moves; the handlers are the
  home area's own, at their own addresses.
- The PlayStation copies a fixed 0x5500 bytes; ours saves exactly the bytes
  the home's chunks cover.
- The text comes through the player's language and layers, not the hack's
  English copy - the same bytes when the mod's layer is the hack's text.
- The leave pose is skipped from the camp (section 3).

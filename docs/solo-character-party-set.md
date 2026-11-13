# The solo-character round: group PS, a party set holding ids 4 and 1

**Status:** IN PROGRESS (2026-11-13, branch `solo/ps` off `solo/round`
`22c878cb`). Measured, built and run on fight 16's route (section 5). Not
played by the owner. The ledger text is drafted in the hand-back, not
numbered here.

The owner's question (round doc 6i item 2): "Is the reason there's no option
with Rei in the balio fight because his child graphics are never in a scene
with Nina? Could we create such a graphic set to let the fight work as
intended?" The short answer: yes to both. No row of the game's 19 holds id 4
beside id 1, and fight 16's code dresses a sprite as id 1 from the loaded
row; in a battle of one, two of the three character sections the battle file
carries are unused, and id 1's battle section can be lifted whole from the
player's own battle file of a row that holds it into one of them. That is
built, behind the mode and only where a fight would otherwise read off the
row.

Every measurement below was taken from the files of the install the harness
runs (`D:\bof3x-runs\solo1112\CB\root\game\DAT`, junctioned to the owner's
`bof3\DAT`) with throwaway scripts that print offsets, sizes and hashes - no
file bytes are reproduced here.

## 1. What a party set is on disk

**The row table and the files.** `PartySet_Select` `0x536AC0`
(`src/game/field/mode_11.cpp`) records a row of `0x669750` (19 rows of three
ids) and loads `Dat_FileNames[row + base]`, base `0x2C2` (mode 0), `0xFC`
(mode 1), `0x10F` (mode 2), `0x2D5` (mode 3). Read from `Dat_FileNames`
`0x64F368` for all 19 rows: mode 0 is `PL<ids>.DAT` (the field set), mode 1
`BPLD<ids>.DAT`, mode 2 `BPLU<ids>.DAT` (the battle sets; `GameMode5_Turn`
picks 2 when `0x904AAC` bit 1 is set), and mode 3 has **no file** (every one
of the 19 entries is null, so mode 3 loads nothing). The file name is the row
in hex digits: row 5 `(0,3,4)` is `PL034`, `BPLD034`, `BPLU034`; row 11
`(2,7,10)` is `PL27A`. (`BRTD/BRTU247, 457, 467, 478` are not reached by
`PartySet_Select`; they are the same shape.)

**The files' chunks** (`docs/DAT_CONTAINER.md`'s format):

| File | Chunks |
|---|---|
| `PL<ids>` | one kind-2 sound bank (tag 1), one kind-0 data chunk at tag `0x10000` |
| `BPLD<ids>`, `BPLU<ids>` | kind-2 banks tags 3, 4, 5 (one per column: id 2's bank is `0x1A770` bytes wherever id 2 sits), one kind-0 data chunk at tag `0x10000` |

Every one of the 57 files has exactly one kind-0 chunk, always at tag
`0x10000`, which `TakeData` (`src/game/battle/dat_load.cpp`) copies to
`MessagePools 0x803580 + 0x10000 = 0x813580` - `field_event`'s
`kBankTable`. No kind-1 (VRAM image) chunk: the set's pictures are uploaded
from that data frame by frame.

**The data chunk's layout.** Three dword offsets from its own start, one per
column of the row, then the three character sections:

- the first offset is always `0xC` (195 of 195 sections);
- in a **field** file the sections are packed end to end, each its own size
  (id 1's `0x1511C`, id 2's `0x30588`, ...);
- in a **battle** file each section sits in a fixed slot: `0x1B000` bytes,
  except id 2's, `0x24000`; the unused tail of a slot is zeros (largest data
  `0x5A00C` = two `0x1B000` slots and one `0x24000`).

**A section is self-contained.** Its own header is four dword offsets from
the section's start: the image table `+0`, the palette table `+4`
(`Sprite_LoadPalette`'s `bank + Long(bank + 4)`), the script words `+8` and
the 3-byte frame entries `+0xC` (`Sprite_SetFrameQueueUpload`). Inside them
every reference is again relative (an image is `images + dword[images +
entry * 4]`, a palette `offsets + dword[offsets + index * 4]`, a script
`words + u16[...]`). All four header offsets fall inside the section's used
bytes in 195 of 195 sections (`m5.py`, the scratch script). So a section can
be moved whole to any place and still read correctly.

**A section is per character, not per row.** Hashing each section (its used
bytes) across every file: one hash per id, in every file that holds it,
except the child/adult pairs - battle id 0 and id 4, and field id 4, have one
section in rows 5 `(0,3,4)` and 12 `(3,4,9)` and another in the others
(row 12's id 9 battle section is row 5's id 0's). So the art a row draws is a
property of the file, and id 1's battle section is the same in `BPLD012`,
`015`, `016` (`BPLU` alike).

**How the loader places it and the sprites index it.** `Field_MemberSprite`
`0x533BA0` (`src/game/field/field_6.cpp`) takes the member's column in row
`0x90412C & 0x7F` (3 when absent), sets `+0x2C` to it and `+0x4C` (the
sprite's animation data) to `0x813580 + dword[0x813580 + column * 4]`, the
section; its texture place by the **slot** argument, not the column (`+0x25`
5, `+0x26` slot * `0x50`, `+0x27` slot + `0x78` the palette row), and
palette 0 of the section into `0x80D380 + slot * 64`. Every later frame
(`Sprite_SetAnimationAt` with no bit 7, `Sprite_SetFrameQueueUpload`) reads
only `+0x4C`. Only two reads use the row afterwards, both in
`Sprite_SetAnimationAt` (`src/game/field/sprite.cpp`): an animation with bit
7 (`+0x64` = the row's id at `+0x2C`, the member animation table
`0x669884`) and the member-redress path (`+0x24` bit 1). With column 3 the
`+0x4C` read is the first section's own header word taken as an offset - the
`Sprite_LoadPalette` crash RT found.

## 2. What fight 16 reads (measured)

`Boss16_Event` code 2 and `BossNina_HookHit` (`src/game/boss/boss_nina.cpp`)
call `Field_MemberSprite(1, 1)` on enemy 2 (`0x93BBB0`). A diagnostic build
(not kept) logged every animation call and bank change on enemy 2 through
the plain run of the route: the first is at frame 19356, right after the
dressing (`+0x24` 1, slot 5, row `0x50`, column 1, data `0x82E58C` =
`0x813580 + 0x1B00C`, column 1's section of `BPLD012`); its animations are
`BossNina_Anims2`'s (5, `0xD`, `0x11`, 1, 0 - no bit 7, the twelve bytes at
`0x64CCAC`) and the member-redress bit never set; the last, at 19551, is
`Sprite_SetAnimationBank(0xF9)` (its own art, bank record column 4) before it
walks off. One more read of the row by id 1, found by the first
champion-Rei run with the dressing built (it crashed in `Sprite_LoadPalette`
at the battle's exit; the stack, mapped with `llvm-nm` on the build's dll:
`Boss16_Exit` +0x293 -> `ScenaCall_Join1` -> `Party_Join` ->
`Field_MemberSprite`): `Boss16_Exit`'s `Scenario_CallA(1)` joins id 1 to the
party (`Party_Join(1)`, `src/game/scenario/scena_call.cpp`), which sets up
the joined record's sprite from the row still loaded - the battle's row,
lacking id 1. The exit hides that record at once (`ObjTrio[1].flags |=
0x40`) and the mode rebuilds it at step 7 from the field's own row
(`solo-character-combat.md` 9), so only the set-up's reads matter there. So,
apart from the join, the dressing reads the row once, inside `Field_MemberSprite`,
and then only the section. Sounds: a second diagnostic logged every
`Sound_PlayEffect` of banks 3 to 5 in fight 16: the plain run played bank 3
only (Ryu, column 0), champion Momo's run bank 5 only (Momo, column 2) - the
dressed sprite plays nothing from a party bank, so the bank of the column it
borrows does not matter.

## 3. The two shapes

**(a) A synthetic row** `(0,1,4)` with field and battle files assembled from
two rows' sections. *Buildable*: sections lift whole (section 1), battle
slots are fixed, and the cache folder the port uses (`BOF3X_CACHE`,
`dat_cache.h`) could hold the result. *But not contained*: the row table at
`0x669750` is read at a fixed address by `PartySet_Find`,
`Field_MemberSprite`, `Sprite_SetAnimationAt` and the accession code, and
holds exactly 19 rows (`0x669789` on is other data), so a row 19 needs every
reader moved or a row of the 19 overwritten for the battle's length - a
global change that must be undone on every way out (a game over restarts the
boot task with `.data` as it is, and a field load of row 5 then finds id 3
missing: column 3, the same crash in the field). Its `Dat_FileNames` entry
would need a new name or `LoadDatFile` intercepted, and a field file only if
the row were to outlive the battle. And it would buy nothing more for fight
16 than (b): the dressing is the only read.

**(b) No new set: the one section beside the champion's row** - built. The
battle keeps the row the mode would choose for the champion alone (Rei from
row 0: row 5 `(0,3,4)`, whose id 4 section is the one rows 5 and 12 carry - the
"child graphics" of the owner's question, by the owner's word), whose battle file the
step loads as always. At the dressing, id 1's battle section is read from the
player's own battle file of the first row holding id 1 (row 0, column 1),
through `LoadDatFile`'s own precedence (the cache, the language layer, the
optional layers - `LoadDatFile` itself with its chunk handler swapped, as
`DatLoad_TextBlockBytes` does), and copied into column 1 of the loaded data
(Teepo's slot, unused in a battle of one); `Field_MemberSprite(1, 1)` then
runs with that column's row byte reading 1 for the one call, and the row is
the game's again before it returns. Why it is sound:

- the section is the game's own id 1 battle section, bytes and all, so the
  sprite is drawn exactly as in the plain run (section 5's pixel compare);
- the copy lands inside the battle file's own slot (`0xF4D1` bytes used of a
  `0x1B000` slot; the slot's tail zeroed), in the arena, so a quicksave
  carries it and the sprite's pointers stay inside the image span; nothing of
  the game's points outside it and nothing in the arena outlives the battle (the battle's end loads the
  field file over it, `BattleResult_SplitExp` / `BattleEnd_AwaitRestore`);
- texture place and palette follow the slot argument (1), as in the original
  fight, where Ryu is alone in slot 0 - the champion is alone in slot 0 too;
- the row changes for one synchronous call only, so no way out of the battle
  has anything to undo;
- the plan is a function of live state (the set byte, `0x904AAC`, the
  champion) and the player's files: Arm makes it and holds it with the
  section's bytes for its battle (`Game_BattleCount`), each dressing uses
  the held one (no file read per hit), and a dressing that finds none for
  its battle - a quick-load from another process - makes it again from the
  files once, so a quick-load needs no record of ours (amended 2026-11-13,
  the round's review, note 5: it read both files at every dressing);
- the dressing and the join act only in a battle the champion fights alone
  (`SoloBattle_ChampionFightsThis`, read from the game's bytes) and only when
  the loaded row lacks id 1 (review note 1: they asked only
  `Solo_Active()`);
- before copying, the loaded data must be the row's battle file (its three
  offsets and the other columns' section headers compared with the file
  read): otherwise a Fatal, since the original call would read off the row.

What it risks: an install whose battle files differ in shape (no kind-0
chunk at `0x10000`, a section larger than the free slot) - then Arm stands
down for the fight as before, with a log line naming the reason; and an
event battle with `0x904AE5` bit 4 (the battle draws from the field file):
not supplied, stands down (no route reaches one with this rule).

**Does it lift other limits?** The rule table's only `kNeedsIdInRow` is fight
16. The mechanism is not specific to it: in a battle of one two columns are
always free, and every battle section but id 2's fits a `0x1B000` slot (id
2's used length is `0x1DD31` / `0x1C511`, so it fits only a `0x24000` slot,
which a row has only where id 2 sits). A future fight that dresses a sprite
as another id can call `PartySetExtra_Dress` in place of its
`Field_MemberSprite` and get the same; a fight that needs the id as a
*party member* (`kNeedsRecord`, fight 34) is a different question and is not
touched.

## 4. What is built

- `src/game/solo/solo_party_set.{h,cpp}`: `PartySetExtra_Arm` (Arm's
  question: reads the two battle files, picks the column, logs the plan or
  the reason it cannot), `PartySetExtra_Dress` (the copy and the dressing),
  and `Solo_PartySetSelfTest` (the column choice, section bounds, used
  length, self-containment on synthetic bytes; called from `Solo_SelfTest`).
- `src/game/solo/solo_battle.cpp`: in `SoloBattle_Arm`, when `ChooseRow`
  finds no row for the champion and the rule's id, `PartySetExtra_Arm` is
  asked before standing down (one call; the rule line is unchanged).
- `src/game/boss/boss_nina.cpp`: `Boss16_Event` code 2 and
  `BossNina_HookHit` call `PartySetExtra_Dress(1, 1)` and fall back to the
  game's `Field_MemberSprite(1, 1)` when it answers false (mode off, or the
  row holds id 1); `Boss16_Exit`'s `Scenario_CallA(1)` runs inside
  `PartySetExtra_Around(1, ...)`, which, only when the loaded row lacks id 1
  in a battle the champion fights alone, lets the free column read 1 for that one synchronous call, so
  the join's set-up reads a valid section (that column's, of whatever set
  file is loaded) for a record that is hidden and rebuilt.

Off (SoloCharacter off): `Arm` is never asked (`SoloBattle_Arm` returns at
`Solo_ChampionFights()`), `Dress` and `Around` find no battle of the mode's
(`SoloBattle_ChampionFightsThis` asks `Solo_ChampionFights()` first): the
game's call, untouched.

## 5. The runs

Group RT's runner, the full `balioAndSunder_2` route to its cut at speed 4,
`--mark-at-cut`, root `D:\bof3x-runs\solo1112\PS\root\boss` (frames, logs and
recipes under `boss\`). The recipe's post-kill seek gives up after 250 circle
presses; champion Rei (record 4, level 5 on this save) reaches the end of
the fight's script later than the others, and the first Rei run with the
dressing built gave up there with the fight still running (the frames show
it on the game's own script: Nina acts, walks out, Balio picks his escape at
frame 21014). The proof runs therefore ran through a scratch wrapper that
raises that one budget to 600 (`sweep600.py`, outside the tree; the tools
are not edited), for every side alike.

| Side | Verdict | Arm / dressing | After |
|---|---|---|---|
| plain | started, ended (win, enemies defeated), back at 20271 | - | modes 2 4, chapter 3, recipe done |
| rei | started, ended (win, enemies defeated), back at 21359 | the plan at step 1 (row 5, `BPLD034`, id 1 from `BPLD012` column 1 into column 1, `0xF4D1` of `0x1B000` bytes); dressed at frame 20089; the join set up from column 1 | party put back, mask `0x35`, count 2, set `80` (put back, its field file loaded); **same as plain** on ending, party after and after the return |
| momo | win, back at 20246 | row 1 holds id 1: nothing of this module runs | same as plain |
| peco | win, back at 21499 | row 2: nothing of this module | same as plain |
| nina | win, back at 20547 | row 0 kept: nothing of this module | same as plain |

No run logged a "frame of no pieces" line; the first Rei run with the
dressing but without the join's column crashed at the exit (section 2), the
second ran to the end.

**The dressed sprite against the plain run** (frames written by the run):
`f16_plain\t019404.png` against `f16_mark_rei\t020139.png` (Nina seated,
just dressed) and `f16_plain\t019482.png` / `t019527.png` against
`f16_mark_rei\t020562.png` / `t020607.png` (her action and its end). Aligned
on the dressing (plain 19356, Rei 20089) the 63 x 45 box around her differs
in 0 pixels at three of the first ten sampled frames and 18 at three more;
aligned on her action (plain 19446, Rei 20526) the 200 x 180 box over
Balio, Sunder and her differs in 18 to 32 pixels through the action (the
enemies' idle frames) and 155 to 169 at its last three samples. Looked at
side by side: the same Nina, the same poses.

**The field after:** `f16_mark_rei\t021660.png` against
`f16_plain\t020574.png`, the recipe's last frames ("Did you beat them?"):
Ryu and Nina on the field with their own sprites, 61 pixels of 307,200
differ.

**Self-tests:** exe host `BOF3X_SELFTEST_ONLY=1`: `solo self-test: 144
checks passed`, `solo battle self-test: 33 checks passed`, `participant
'SOLOCB' registered`, exit 0. Hosted `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`
from the ini-less launcher copy: the same three lines, 916 `MISMATCH` lines
all `0 MISMATCHES` (the `boss_sc` groups, `boss_nina.cpp`'s, among them),
exit 0. Logs: `D:\bof3x-runs\solo1112\PS\root\selftest_exe.log`,
`selftest_hosted.log`.
`tools/ledger_check.py`: 229 entries, 0 errors (6 notes);
`tools/ledger_switches.py --check`: 0 errors.

## 6. Not done

- The owner's play of the fight with Rei.
- A quick-load mid-fight 16 with Rei (the design needs no record - the plan
  is remade from live state - but it was not run).
- An install other than the owner's PC install (a PSP-only or disc-only
  install's `BPL*` files): not run. Arm stands down with a reason if the
  files do not have the shape measured here.
- The 64-bit build: not built or run here.
- Fight 34 (`kNeedsRecord`): not this module's.

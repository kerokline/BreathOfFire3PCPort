# The record types (`src/game/records.h`): typed-data round groups SD and C1

**Status:** DRAFT (verified 2026-10-19 against `origin/main` 19180e63, then
corrected by an independent verification the same day, section 3; stage B
converted sites onto it; stage C1, 2026-10-19 on `typed/c1`, added the shared
`SpriteHead`, the audits' renames, the per-meaning views and the families
the header lacked, sections 6 to 8. The headers compile standalone, and the
tree's machine code is unchanged: `codegen_same.py` 640 objects, 0 differ)

Group SD of [`typed-data-round.md`](typed-data-round.md) section 2: one struct
per record family. Each family has `static_assert(sizeof == stride)` and one
`static_assert(offsetof == offset)` per named field. Stage B converted the
sites; stage C1 ([`typed-data-stage-c.md`](typed-data-stage-c.md) section 3)
reshaped the header so that stage C2 can convert the rest. Three headers
since C1: `src/game/records.h` (the families and the views),
`src/game/records_gpu.h` (the GPU packets and `SVector`) and
`src/game/records_effect.h` (the effect sub-pools); `records.h` includes the
other two.

## 1. The method

- **A field is named only where a cited reader names it.** The readers are a
  module doc, a `*_callees.h` constant with its width comment, a `symbols.toml`
  evidence line, or the reimplemented function itself. Each field's source is
  in the tables below and beside the field in the header. Everything else is
  `unsigned char pad_<offset>[n]`, with the reason for the gap beside it.
- **Widths come from the reader's own access**: `Word`, `Long`, `Byte`, a
  `u16` in a doc table, or a `movsx`.
  - Where readers take a dword and also its high word (the 16.16 positions
    `+0x34` / `+0x38` / `+0x3C`, whose whole parts are read at `+0x36` /
    `+0x3A` / `+0x3E`), the dword is the member and the comment names the
    high word.
  - The same applies where the field and battle code read one dword as two
    bytes (`PartyMember +0x134`). A union was not used, so that stage B
    decides that shape with the sites in front of it.
- **Addresses are `std::uint32_t`**, because the image is 32-bit. Nothing is
  packed: every named field is at its natural alignment. The header was
  checked both with and without `#pragma pack(1)`; all asserts hold either
  way, so the plain layout was kept.
- **Families split where the data split them.**
  - The battle enemy (`0x128`) is its sprite (`+0..+0x7F`) and then
    `EnemyWorkingRecords`. `EnemyData` (`0x8C`) is the area's record that
    `Battle_CopyEnemyData` copies in.
  - The battle task (`0x84`) is the 48 slots at `0x93A000`, not the
    scheduler's `0x20`-byte `Task_Records` ([`task_sched.md`](task_sched.md)).
  - The stride `0xA4` covers two layouts. One is the sprite object (this
    header). The other is the persistent character record `0x903A70 + id *
    0xA4` (`kCharStride` / `kRecordStride`), which is not covered here except
    as `PartyMember +0x80..+0x123`, the copy of it.
- The structs are flat on purpose. The sprite, the party member, the enemy,
  the effect object and the battle task all go through `Sprite_Current`, so
  their first `0x80` bytes are read by the same sprite functions. Their own
  readers still give some of those bytes different meanings (the battle
  task's `+0x60` is the popup's value, and the sprite's `+0x60` is the
  depth). A shared `SpriteHead` is a stage B or C decision, made against the
  sites. **Stage C1 made it: section 6.** The five families now derive from
  `SpriteHead`; the bytes whose meaning differs by family are anonymous
  unions of each family's name inside it.

## 2. Checked against the sources (2026-10-19)

Each layout was re-read against the doc or reader it cites before it was
written:

- **Window record**: every field is from [`window-task.md`](window-task.md)
  section 3's table, except `+0xA`, which each handler reads differently and
  is named `param_a` (section 3 item 8).
- **Save summary**: from `Menu_DrawSaveSlot` (`field_o.cpp`) and
  [`save-files.md`](save-files.md) section 2. [`save-interchange.md`](save-interchange.md)
  calls `+5` the leader's `char_id`, and `Menu_DrawSaveSlot` reads `+5..+7`
  as the three party icons. These agree: `party[0]` is the leader.
- **Directory entry**: from `Save_ListFiles` (`save_menu.cpp`). The name's
  0x14-byte extent is the gap up to the size field. The writer is unbounded
  (save-files.md's first defect).
- **Battle task**: from `battle_flow.cpp` (`BattleTask_Create` / `RunAll` /
  `FreeCurrent`) and `battle_fx_tasks.cpp` (the popup's five states). `+0x60`
  is read as a dword at the start and as a word by the draw. The dword is the
  member.
- **Party member**: from `battle_setup_callees.h`, `battle_damage_callees.h`
  (whose offsets are from `PartyWorkingRecords` `0x802D50`, the record
  `+0x10`; converted here), `area_entry_callees.h`, `area_backdrop.cpp`,
  `effect_3a_callees.h` and [`member-sprites.md`](member-sprites.md) section
  2.
  - A first census reported `+0x70` as a separate byte "raised". The member
    readers do test only its low byte (`member_sprites.cpp`), but
    [`sprite-pose.md`](sprite-pose.md) section 3 writes the sprite's `+0x70`
    as a dword. The member is a `u32` named `raised` (the sprite's `reach`
    since C1, section 6.3).
  - The same census put `+0x136` / `+0x137` as bytes inside the battle's
    `u32` at `+0x134`. That is the overlap described in section 1.
- **Enemy and enemy data**: from `Battle_SetupEnemy` / `Battle_CopyEnemyData`
  (`battle_sprites.cpp`), `battle_flow.cpp`, `battle_setup_callees.h`,
  `battle_damage_callees.h` (offsets from `+0x80`) and `tools/enemy_ai.py`'s
  header.
  - The data's level `+0x18` is a `u16`, because `enemy_ai.py` and the copy
    both move a word. battle_damage's `kELevel` says "u8, also read as a u16".
  - Zenny and EXP at the data's `+0x14` / `+0x16` are named through the copy
    chain to the enemy's `+0x94` / `+0x96`, which the kill reads by those
    names (`battle_flow.cpp`).
- **Sprite object**: from [`sprite-draw-order.md`](sprite-draw-order.md)
  sections 2, 5, 6, 12 and 14, [`sprite-pose.md`](sprite-pose.md) section 3,
  [`movement-script.md`](movement-script.md) sections 1b and 1c,
  `sprite_screen.cpp` and `field_objects.cpp`.
- **Effect object**: from `symbols.toml`'s evidence lines for
  `Effect_Objects`, `Effect_FindFree`, `Effect_RunObjects`, `Effect_Spawn` and
  `0x5898D0`; the state, step, timer, shade, depth and projected point from
  the kind dispatchers and [`effect_1a.md`](effect_1a.md) section 2 and
  [`effect_4d.md`](effect_4d.md).

## 3. Unsettled, and what the verification settled

An independent verification of the first commit (2026-10-19, reported in
`C:\Users\kerok\v_typed\sd_report.md`, outside the repository) agreed on
every offset and width, found four fields with the wrong signedness, and
found bytes left as "unread" padding although readers use them. The
corrections are in the header; each was re-read at the cited line before it
was applied. Items 1, 2, 4 and 5 below are settled by it.

1. **`PartyMember +0x148`: settled. It is the character record's index, in
   battle too.**
   - `area_entry_callees.h` (`kRecCharacterIndex`), `effect_3a_callees.h`
     and `member_sprites.cpp` read it so in the field.
   - The battle readers of the same byte use it the same way:
     `CharRecord(f[0x148])` (`battle_e3.cpp:684`, `:723`),
     `CharRecord(p[0x148])` (`battle_e4.cpp:126`) and
     `kCharRecords + member[0x148] * kCharStride` (`battle_e6.cpp:318`).
   - `battle_damage_callees.h`'s `kPCharIndex` (named `kPWeaponKind`, a
     misnomer, until stage B1 renamed it) is this byte. Its value is
     `0x138`, and the party pointer it is used on is `Party(a)` =
     `0x802D50 + a * 0x14C` (`battle_damage.cpp:95`), which is `0x10` past
     the records at `0x802D40`; so `Battle_CalcDamage`'s character-index
     test, now `pr->char_index == 5` on the record's own start
     (`battle_damage.cpp:634`), reads record `+0x148`. The weapon-element
     rule applies when the attacker's character index is 5.
   - The field stays `char_index`.
2. **The two flag dwords: settled; there is no conflict.**
   `battle_setup_callees.h`'s `kPFlags = 0x134` and `kEFlags = 0x114` are
   `battle_damage`'s `kPFlags2` / `kEFlags2` (working `0x124 + 0x10` and
   `0x94 + 0x80`): `battle_setup` names only the second dword. The bits mean
   the same on both sides: the second dword's 0x10 is the gate, its 0x40 and
   0x80 the charges, and the first dword's 0x2 is cleared the same way for
   both (`battle_damage.cpp:151-181`, `:296-343`, `:468-472`, `:678-690`). The
   names `flags_130` / `flags_134` and `flags_110` / `flags_114` stay.
3. **The party tail's field and battle reuse.**
   - `+0x124` is the script context, or the action / result record's head.
   - `+0x128` is the walking speed (a byte), or the HP change (a signed word).
   - `+0x130` is the script pointer, or a flag dword.
   - `+0x134` is the standing counter and action bytes, or a flag dword.

   In the field the tail is laid out like the sprite object's `+0x80` script
   context: the party's `+0x124` context (`area_entry_callees.h`
   `kRecContext`) is the sprite's `+0x80`, its `+0x128` speed index the
   sprite's `+0x84`. In battle the same bytes are the result record that
   `0x904B60` points at (`+0x124` head, `+0x125` kind, `+0x126` id,
   `+0x128` / `+0x12A` the signed deltas; the enemy's is at `+0x104`). The
   battle names and widths are used, and the field use is in each comment. A
   union per overlap, or a shared script-context type with the sprite's
   `+0x80`, is stage B's call. **C1: the two views `ActionRecord` and
   `MemberFieldTail` (section 7); `PartyMember` keeps its battle names.**
4. **`+0x144` / `+0x145` (party; `+0x124` / `+0x125` on the enemy):
   settled. Both readings are one thing.** They are the attack and ability
   charge counts, consumed with bit 0x40 / 0x80 of the second flag dword:
   `battle_damage.cpp:466-472` adds `count * ATK / 2` and clears the count
   and bit 0x40; `:676-690` multiplies the power by `count + 1` and clears
   the count and bit 0x80 (party and enemy alike). `battle_setup`'s "zeroed
   with flags 0x40 / 0x80" is the same pairing. The fields keep their names
   (`count_40` / `count_80`) until stage B; `charge_attack` /
   `charge_ability` would fit.
5. **The save summary's time: settled.** `+0xC` is hours, `+0xD` minutes,
   `+0xE` seconds and `+0xF` frames. `Save_BuildBlock` copies the play-clock
   dword whole (`rest_2c.cpp:396`, `PutL(0x90468C, L(0x9040C8))`, the summary
   being `0x904680`); the clock's bytes are hours, minutes, seconds and
   frames (`map_field_objects_callees.h`, and the `Game_Clock` evidence in
   `symbols.toml`); `Menu_DrawSaveSlot` prints `summary[0xC]` then
   `summary[0xD]` (`field_o.cpp:623`). The same builder writes `+9` and
   `+0xA` from the window style bytes `0x903A5A` / `0x903A5B`
   (`rest_2c.cpp:397-398`): `style` and `style2`.
6. **The sprite's `+0x64..+0x6F`.** These are rotation angles in their low
   words (sprite-draw-order 12), and also the member and slot row during a
   member set-up (sprite-pose 3); `MoveCmd_OpF9` runs a party object's tilt
   counts into them (`field_o.cpp`). They are named `angle[3]`, `u32`, with
   the uses in the comment.
7. **Bytes still left as padding although some reader touches them.** The
   verification's list of bytes a reader names was applied where the reader
   was confirmed; these remain, each with its pad comment:
   - The sprite's `+2..+5` (per-handler state bytes: `+2` a step to
     movement-script.md's F8 and the spawn op), `+0x9F` (one reader,
     `area_w0c.md` `Area32_TintStart`) and `+0xA1` (written by
     `field_objects.cpp`).
   - The party's sprite bytes `+0x2A` / `+0x2B` and `+0x5C..+0x5F` (written
     on entry); `+0x9E` the max-HP scale (`battle_e5.cpp:403`); `+0xB5..+0xB7`
     the rest of the resistance bytes, `+0xBA` a percentage byte
     (`battle_e3.cpp:1040`) and the base stat block at `+0xC0`
     (`battle_e5.cpp:405-425`); `+0x136` / `+0x137`, inside `flags_134` by
     design (section 1).
   - The effect object's `+3`, `+4`, `+0xB` (a spawn argument, per kind a
     hold), `+0x32` (draw key) and `+0x64..+0x6C` (angles): no effect reader
     confirmed for this family.
   - The enemy's `+0x2B..+0x2D` (the sprite's key class and column; no enemy
     reader), `+0x58` / `+0x5A` (pose words, `boss_sc_callees.h`
     `kEnemyPose`), `+0xAE` and the once-markers `+0xAB` / `+0xAF` (`rest_3c.md`
     `Effect_EnemyRaiseAAAE`),
     `+0xBC..+0xBE` and `+0xC5..+0xEF` (the stat block's copies; base HP, AP
     and stats at `+0xD0..`, `battle_e5.md`), and `+0x11C` (written at
     set-up, `battle_sprites.cpp`).
   - The battle task's `+0xB` (compared with 1) and `+0x24..+0x2C` (written
     by `PoseStart`, `battle_fx_tasks.cpp:330-345`).
   - The window record's `+8`, `+9`, `+0xC`, `+0xE`, `+0x1C..+0x23`: each
     handler reads them differently (item 8).
8. **`WindowRecord +0xA`: a per-handler byte.** It is the half-width in
   pixels for the field windows ([`window-task.md`](window-task.md) section
   3), the actor slot for battle handler 3
   ([`battle_win_states.md`](battle_win_states.md) section 2;
   [`battle_sprites.md`](battle_sprites.md) section 3), and the item category
   for the list windows (the greyed / price percent in the buy list,
   [`menu-windows.md`](menu-windows.md) section 2). It is named `param_a`
   with the three meanings in its comment. The other bytes past `+3` are
   redefined per handler too (battle: gauge kinds `+8` / `+9`, gauge lengths
   `+0xB` / `+0xC`, home place `+0x10` / `+0x12`, HP / AP shown `+0x14` /
   `+0x16`; lists: mode `+8`, greyed `+9`, top `+0xB`, cursor `+0xD`, scroll
   `+0x12`, the shop list's dword `+0x20`). Stage B: keep `+0..+7` and `+0xF`
   top-level and put the rest in a union of a field, a battle and a list view.
   **C1: `WindowRecord` stays the field windows' view; `BattleWindow` and
   `ItemListWindow` are the other two (section 7). `+4` / `+6` are `x` /
   `y`, the top-left corner, not the centre (section 6.3).**

## 4. What next

- The census (group CS) gives each record-by-offset site a family.
- Stage B converted the sites it could; stage C2 converts the rows of
  `analysis/typed_left.tsv` that C1's types unlock (section 8). Every offset
  a site reads that is padding here becomes a named field then, with the
  site as its reader.
- `symbols.toml`'s tables take these as their `ctype` with a `count`:
  `Sprite_Objects` 30, `ObjTrio` 3, `Effect_Objects` 20, the enemies 8, the
  battle tasks 48, `WindowRecords` 22, the summaries and `Save_Directory` 16.

## 5. The field tables

Moved to section 9 at stage C1, after the head, the views and the new
families (sections 6 to 8), and written from the headers since.

## 6. The shared head (stage C1, 2026-10-19)

### 6.1 The claim, and the form

The field sprite object, the party member, the effect object, the battle
enemy and the battle task all go through `Sprite_Current`, and the battle
code copies one into another: `Battle_InitActorContexts` `0x446FF0` copies
each actor's first `0x80` bytes (a member's or an enemy's) into a task slot
with `rep movsd`, as `BossWeretigr` `0x43D450` and the MAGIC017/018 images
do (the B1 magic audit, `C:\Users\kerok\v_typed\b1_audit_magic.md`;
`symbols.toml`). So their first `0x80` bytes are one layout as far as the
sprite engine reads them. `SpriteHead` is that layout, and each family
derives from it:

    struct SpriteObject : SpriteHead { /* +0x80..+0xA3 */ };
    struct PartyMember  : SpriteHead { /* +0x80..+0x14B */ };
    struct EffectObject : SpriteHead {};
    struct BattleEnemy  : SpriteHead { /* +0x80..+0x127 */ };
    struct BattleTask   : SpriteHead { std::uint32_t owner; };

A base rather than a first member, so that `p->field` is spelled as before
for every field the families already named. A derived class with members of
its own is not standard-layout, so `offsetof` on it is
conditionally-supported and clang warns (`-Winvalid-offsetof`); the asserts
on the derived families sit inside a `#pragma GCC diagnostic` push that
silences that one warning, and they hold (a single non-virtual base is laid
at offset 0, the derived members after it). Every head offset is asserted on
`SpriteHead` itself, which is standard-layout. `EffectObject` adds nothing,
so it is standard-layout too.

`BattleEnemy::kind` (`+0x8C`) hid the head's `kind` (`+5`, the effect's
and the task's); C3's naming pass renamed it `enemy_kind` (`EnemyData::kind`'s
copy), so on an enemy `->kind` is now the head's `+5` union member.

### 6.2 What the readers say, byte by byte

Each head byte was read against every family's readers (the struct comments
of stage A, the B1 left-lists' evidence, the module docs). Where they agree
the head has one name; where they do not, the head holds an anonymous union
of each family's own name, so that **that family keeps its own field**, as
the brief asks. Such a name compiles on any family; it is right only on its
own, and its comment says which.

| Offset | Readers agree? | Head |
|---|---|---|
| `+0x00` | yes: bit 0 in use / present / taken; the sprite draw's bits 5..7 on all | `flags` (the effect's `in_use` renamed) |
| `+0x01` | yes: the handler's state on all five | `state` |
| `+0x02` | yes: the state's step (the effect kinds' sub-state tables, the actors' second state byte, a field object's step byte) | `step` |
| `+0x03`, `+0x04` | the actors' third and fourth state bytes; per effect kind a counter | `state3`, `state4` |
| `+0x01..+0x04` | the party and enemy readers index them as an array (`battle_setup_callees.h` kState1..4) | the same bytes as `states[4]`, in a union with the four |
| `+0x05` | **no.** Member and enemy: the actor index (`battle_actions.cpp:353` reads both as one actor). Effect: `Effect_KindHandlers[+5]`. Task: the entry of the runner's table (`battle_fx_tasks.md` 1). The effect's and the task's are one meaning (the handler's entry), the actors' another | union `actor` / `kind` |
| `+0x06` | **no.** Field object: the type (0x0A a party object). Effect: `Effect_Spawn`'s first argument. Task: which of `BattleTask_RunAll`'s four runners. Party: whom it follows (unnamed) | union `type` / `param` / `runner` |
| `+0x07` | **no.** Field object: `flags2`. Task: the popup's draw mode | union `flags2` / `mode` |
| `+0x08` | **not shown to agree.** Field object and member: the facing; the battle banner reads a member's and an enemy's `+8` as "a side byte" (`battle_win_states.md` 2); the enemy's writers call it the layout ^ 2 | union `direction` / `layout` |
| `+0x09` | **no** in name: frames left in a step (the sprite step's multiplier) against the effect's and the task's own frame counter | union `step_frames` / `timer` |
| `+0x0A` | one reader: the popup's digit count | `digits` |
| `+0x0B` | **no**: four readings, none shared | `pad_0B` |
| `+0x0C`, `+0x10` | the sprite engine's step (`sprite-draw-order.md` 6; `Sprite_ClearSteps`) on all; the effect kinds and the magic tasks reuse them (section 7, `EffectWork`) | `step_x`, `step_z` |
| `+0x14` | the third step (`Sprite_ClearSteps` clears it with the two; the field's rise); a magic task's scratch | `step_y` |
| `+0x18..+0x23` | **no**: each handler's own dwords (an acceleration, a handle, a radius, an angle offset, a speed, an address) | `work_18`, `work_1c`, `work_20` |
| `+0x24..+0x31` | yes (the draw's bytes; the screen point) | the sprite's names; `+0x2A` `flip` (section 6.3) |
| `+0x32` | **no**: the draw key against the damage popup's digit phase (started from the owner's draw key, `battle_sprites.cpp:564`) | union `draw_key` / `digit_phase` |
| `+0x34..+0x3F` | yes: x, z, height, 16.16; the whole parts read alone | each a union of the dword and its two `u16` halves (section 6.4) |
| `+0x40..+0x5F` | yes (the sprite's scale, script, frames, mode, shade) | the sprite's names |
| `+0x60` | **no**: the projected depth against the popup's number | union `depth` / `value` |
| `+0x64..+0x7F` | yes (angles, reach, the float point and depth) | the sprite's names; `+0x70` `reach` (section 6.3) |

No family's head turned out to be another layout altogether: the
disagreements are all handler bytes at `+5..+0xB`, the handler dwords at
`+0x18..+0x23`, and the popup task's reuse of `+0x32` and `+0x60`.

### 6.3 The renames

Same offsets, same types, better names; every existing use renamed. The
uses were found by compiling the tree with each old name hidden (the clang
diagnostics give file, line and column) and edited at exactly those places,
so a homonym on another struct was never touched; the gate then proved the
machine code unchanged.

| Was | Now | Sites | Evidence |
|---|---|--:|---|
| `BattleTask::y` (`+0x38`) | `z` | 334 | world z in all 331 magic uses and the popup's (B1 magic audit N1); every other family names `+0x38` `z` |
| `BattleTask::vel_x` / `vel_y` (`+0xC` / `+0x10`) | `step_x` / `step_z` | 230 / 193 | the head's names (the brief); a velocity only some of the time, and then dx / dz (N2) |
| `BattleTask::start_x` / `start_y` (`+0x18` / `+0x1C`) | `work_18` / `work_1c` | 39 / 45 | never a start point in the magic code: a radius, a step, an angle offset, an acceleration (N3) |
| `SpriteObject::accel_x` / `accel_z` | `work_18` / `work_1c` | 15 / 12 | one byte, one name with the task's; `MoveCmd_OpF9`'s acceleration, but F8 and the spawn store a handle there (B1 audit rbs) |
| `BattleTask::param` (`+5`) | `kind` | 7 | the task's `+5` is the entry its runner dispatches on, as the effect's `+5` `kind` is (`battle_fx_tasks.md` 1) |
| `BattleTask::kind` (`+6`) | `runner` | 5 | `BattleTask_RunAll`'s four-entry table by `+6`; the effect's `kind` is at `+5`, so the two could not share the name at different offsets |
| `WindowRecord::centre_x` / `centre_y` | `x` / `y` | 293 / 264 | `Window_Grow` moves `+4` back by half the width step as the width grows - that keeps a box centred only if `+4` is its left edge (`window_task.cpp` `HalfStep`); the menus draw the box at `+4 + 3` and the text at `+4 + 7` and clamp `+4` at pixel 0x50 (B1 audit rbs section 4; `menu-windows.md` 2 "x, y") |
| `PartyMember::member` (`+5`) | `actor` | 84 | `battle_actions.cpp:353` takes a member's `+5` and an enemy's `+5` (`actor`) as one actor index |
| `PartyMember::raised` (`+0x70`) | `reach` | 1 | one byte, the sprite's name: the reach `Sprite_PointInReach`, `Party_MemberAt` and the blocked tests take from the low byte; member-sprites 2's "raised" is the same byte non-zero |
| `SpriteObject::pose` (`+0x2A`) | `flip` | 8 | the draw ORs 0x400, the flip, into the mode word when it is set (`sprite-draw-order.md` 14); `BattleEnemy::flip` already named it so (`battle_flow.md` "the flip byte") |
| `EffectObject::in_use` (`+0`) | `flags` | 17 | the head's `+0` on all five; the effect's "non-zero in use" is the same test |
| `PartyMember::state[0..3]`, `BattleEnemy::state[0..3]` | `state`, `step`, `state3`, `state4` | 220 / 170 | the head's names (an array named `state` cannot share the head with the other families' scalar `state`); 7 of the 390 are spelled `states[k]`, below |
| `WindowRecord::kind`'s comment | (no rename) | - | "0..2" is the field windows'; the battle windows' are 0..7 and the menus write 5 and 0xB (the writers are listed in the comment) |

1,937 sites in 124 files.

**A trap the gate caught: two branches that change different state bytes.**
The first build after the renames differed in three objects:
`EnemyOp_CastStart` (`battle_e3.cpp`), `BattleObj_CastStart`
(`battle_obj_states.cpp`) and `Field_LeaderStand` (`field_event.cpp`). In
each, two branches end in a store to a different state byte (`++state[0]` in
one, `++state[1]` in the other; or `state[1] = 1` beside a raw byte store).
With the array, the two stores differ only in an index, so the compiler
sinks them into one store through a register; with two named fields they
differ in the field and are not merged. Same behaviour, different
instructions. Those seven stores are spelled `states[0]` / `states[1]`,
which is why the head keeps the array view; after that, `0 differ`. C2 will
meet the same where it converts a raw state-byte store: if the gate refuses
a named state byte, try `states[k]`.

### 6.4 The 16.16 halves

`+0x34`, `+0x38` and `+0x3C` are each an anonymous union of the dword and an
anonymous struct of two `u16`: `x_frac` / `x_whole` (`+0x34` / `+0x36`),
`z_frac` / `z_whole`, `height_frac` / `height_whole`. The whole parts are
read alone about 1,000 times in the left-list (width rows), mostly as a
`u16` (`Word`), sometimes signed (`SW`).

Checked against the gate with throwaway sites, then reverted: in
`field_c1.cpp` sixteen `Word(rv::Bytes(o) + 0x36 / 0x3A / 0x3E)` reads became
`o->x_whole` / `z_whole` / `height_whole` and two setters
`rv::Store<unsigned>(o->height_whole, ...)`; in `field_c2.cpp` three `SW(...)`
reads became `static_cast<std::int16_t>(s->z_whole)` and `x_whole`. Full
build, `codegen_same.py`: 640 objects, 0 differ - the member is the same
16-bit load and store, and a signed read is the cast of it. The two files
were then restored.

An anonymous struct inside a union is a GNU extension (clang's `-Wpedantic`
names it `-Wgnu-anonymous-struct` / `-Wnested-anon-types`); the build does
not use `-Wpedantic`, and `-Wall -Wextra` is silent. `SharedScratch` uses the
same form (section 7).

## 7. The views and the new families (stage C1)

### 7.1 Views: one record's bytes as one handler reads them

A view is a struct to cast to with `rv::As<T>(p)` where the meaning is
known; the record keeps its family struct. None is a union inside the
family, so a view's names do not leak onto the family's other readers.

- **`ScriptContext`** (0xC): the movement-script context, at a field
  object's `+0x80` and a party member's `+0x124` in the field
  (`area_entry.cpp` hands `record + kRecContext` to `ScriptContext_Reset`).
  `MoveScript_Step` and its group handlers take it as `object`. Named from
  `ScriptContext_Reset` `0x5322B0` (writes `+0, +1, +2, +3, +5, +7, +0xA`,
  speed 3 at `+4`), `move_script_bytes.h` `Pos` (`+0xA`) and the step's ops.
  Past `+0xB` the sprite and the member differ, so the view stops there.
- **`MemberFieldTail`** (0x1C, cast at a member's `+0x124`): the context,
  then the script pointer `+0xC` (`kRecScript`), the standing counter and
  action `+0x12` / `+0x13`, `field_flags` / `field_object` `+0x14` / `+0x15`
  (member-sprites 2; `area_backdrop.cpp`; `field_e1.cpp`).
- **`ActionRecord`** (0x1C, at a member's `+0x124` and an enemy's `+0x104`;
  the action blocks' `+0xC`, and what `0x904B60` is pointed at): the pairs of
  constants in `battle_setup_callees.h` and `battle_damage_callees.h`
  (`kPHpChange` / `kEHpChange` and the rest) give the same offsets from both
  bases. Its names are new and neutral (`command`, `command_kind`,
  `action_id`); the families' own spellings were brought to them in C3's
  naming pass (`PartyMember::queued_kind` and `BattleEnemy::command`, the
  `+1` byte, are `command_kind`; `BattleEnemy::result_head` is `command`).
- **`BattleWindow`** and **`ItemListWindow`** (0x24 each): the battle
  windows' layout (`battle_win_states.md` 2, all of it) and the item list's
  (`menu-windows.md` 2's first column). The buy and sell lists read `+0xA..+0xD`
  otherwise; those bytes stay `WindowRecord`'s. `WindowRecord` remains the
  field windows' view and the generic one.
- **`EffectWork`** (0x80, standalone): the effect kinds' own words at
  `+0xC`, `+0x10` (dwords, `work_0c` / `work_10`) and `+0x2E`, `+0x30` (u16,
  `work_2e` / `work_30`). The B1 union rows at these four offsets are 760 in
  59 kinds; the widths are the rows' majority (UL / SetUL at the dwords,
  `Word` at the words). `+2`, `+6` and `+9` - the other per-kind bytes the
  brief lists - are the head's `step`, `param` and `timer`, which already
  read as the effect family's own.
  The C2 audit (2026-10-19, `typed-data-round.md` 3) added `+0x32`
  (u16, `work_32`): 52 sites in thirteen kinds and subs had taken it as
  the head's `draw_key` where the kind keeps a count, a radius, an angle
  or a lid index there (the table in section 9).

### 7.2 New families

Each only as far as its readers settle it; where they disagree, `pad_` with
the disagreement in the comment.

- **`CharRecord`** (0xA4; `CharacterRecords` 0x903A70 + id * 0xA4, eight)
  with **`CharStatBlock`** (0x20, twice: the effective block `+0x20` and its
  base `+0x40`). From `char-stats.md` 2 (`Char_RecalcStats` and the
  equipment passes), `item-use.md` 2, `save-interchange.md` 2 (the nine-byte
  name; the PSX record + 4) and the experience readers (`Char_LevelUp`,
  `BattleResult_AddExp`, `Menu_DrawMemberStatus`). Not the sprite object's
  layout. `PartyMember +0x80..+0x123` is a copy; its own `base_*` names at
  `+0xC0..+0xCB` are the copy's base block, and `BattleEnemy` got the same
  at `+0xD0..+0xDB` (`EnemyAI_ScaleStat` caps HP at `+0xD0`, AP at `+0xD2`,
  `battle_e5.md`).
- **`EncounterSlot`** (12; 0x939F00, eight): `active`, `kind` (the enemy data
  id), `x`, `z` - `Battle_InitEnemies` hands `+1`, `+4`, `+8` to
  `Battle_SetupEnemy`; three callee headers agree.
- **`CommunityRecord`** (8; 0x9046D0, sixty): `in_use`, `slot`, `stamp`;
  `+2` / `+3` stay padding (states, marks, or an item and its tab, by
  reader). **`CommunityBuilding`** (8; 0x9048B0, eight): `kind` only; `+1`
  is a level, a lit flag or a key by reader, `+4` a stamp or a tally.
- **`SharedScratch`** (0x1C; 0x903850, the PSX scratchpad relocated): the
  magic code uses it as the PSX compiler's spill area, so no offset has one
  meaning - the same offset is a word in one spell and a byte or a dword in
  the next. The struct is a union of three views named by offset (`w_0a`,
  `d_08`, `b_0a`), each width typed, with the extent its readers reach
  (`+0x18`'s dword). It gives the 1,546 magic sites a typed spelling, not a
  meaning.
- **The GPU packets** (`records_gpu.h`): `GpuPrimHead` and nine packets -
  `GpuPolyFT4` (0x48), `GpuPolyGT4` (0x54), `GpuLineF2`, `GpuLineF4`,
  `GpuTile`, `GpuSprt`, `GpuGlyph` (code 0x6C), `GpuCellSprite` (code 0x84),
  `GpuDrawMode` (0xE8) - each from the Direct3D handler that draws it
  (`d3d-draw.md` 3, `glyph-draw.md` 2, `sprt_draw.cpp`) and the setter that
  starts it (`psx_gpu.cpp`, `prim.cpp`). The packets repeat the head's five
  members rather than derive, so each is standard-layout. Not covered: the
  F3 / FT3 / F4 / G3 / GT3 / G4 / G2-line / TILE_1 handlers are Capcom's
  (the D3D group did not read them), and the `+2` / `+4` / `+6` word stores
  of 15 rows match no packet here.
- **`SVector`** (8): libgte's `SVECTOR`, the shape of `Prim_VertexScratch`
  0x9037A0 (four) and of the effect files' `Vertices()` / `Vtx()` /
  `Vertex(i)` views of it.
- **The effect sub-pools** (`records_effect.h`): `EffectSpeck` (0x14),
  `EffectSpark` (0x28), `EffectDrop` (0x18) - the three families E2A lays
  over `EffectKind30_Shards` (`effect_2a.md` 1.3..1.5) -, `EffectGlowSpark`
  (0x18, kinds 0x64 / 0xA0), `EffectShard` (0x2C, kinds 0x64 / 0x1E / 0x1F /
  0x72 / 0xA0, every field written by `EffectKind64_InitShard`),
  `EffectTrailPoint` (0x34, kind 0x4A), `EffectRay` (0x38, kind 0x62) and
  `EffectMote` (0x1C, kind 0x68). `EffectKind30_Shards` is one scratch every
  kind lays out its own way (D201), so which pool a pointer into it holds is
  the running kind's - an audit point for C2. Not made: kind 0x61's
  particles (0x14 at 0x92DF80), E3B's and the other kinds' pools that the
  docs give no layout for.

## 8. What C1 unlocks, for C2

The rows of `analysis/typed_left.tsv` whose reason is C1's
(`field-missing`, `no-struct`, `union`, `width`: 7,982 rows on `typed/c`),
sorted by the type that now names them. Counted by a script over the
left-list (the C1 scratch's `classify.py`), from each row's family and
offset in its evidence, or its base when there is no family; the counts are
rough by design - a row is C2's to confirm against its function before it is
converted.

| Type | Rows | Reasons | Notes for C2 |
|---|--:|---|---|
| `SpriteHead` names (all five families) | 1,153 | field-missing 1,105, union 48 | magic's `via=Sc` task rows are most of it (`+0x14`, `+0x20`, `+0x2E`, `+0x30`, `+0x3C`); party and effect head bytes; task `+0x32` / `+0x38` |
| `SpriteHead` 16.16 halves | 1,011 | width 902, field-missing 109 | `x_whole` / `z_whole` / `height_whole` (and `_frac`); a signed read is `static_cast<std::int16_t>(p->x_whole)` (section 6.4) |
| `SpriteHead`, no field at the row's width | 88 | width 28, field-missing 54, union 6 | a dword read as a word (`+0xC`, `+0x10`, `+0x20`), a byte of a word: stay `rv::Bytes` |
| `EffectWork` | 760 | union | the kind's own words at `+0xC`, `+0x10`, `+0x2E`, `+0x30` |
| `ActionRecord` / `MemberFieldTail` | 100 | union | the party tail `+0x124..+0x13F`; battle code: `PartyMember`'s own names are the battle meaning already |
| `BattleWindow` / `ItemListWindow` | 65 | union | 4 more are the shop lists' bytes, which stay |
| `SharedScratch` | 1,546 | field-missing | the magic files' `SB` / `SW` / `SD` helpers; the pointer still comes from `Mem(kS)` (the seam) |
| GPU packets | 754 | field-missing 224, no-struct 530 | the packet's kind is the setter the function calls on it (`Gpu_SetPolyFT4`, `Gpu_SetSprt`, ...); a pointer the function hands to two setters is two packets |
| `SVector` | 307 | no-struct | `Vertices()` and friends |
| `CharRecord` | 107 | no-struct | `table=0x903a70`, `CharRecord(...)`, `Record` in the scenario and item files |
| effect sub-pools | 192 | field-missing 68, no-struct 117, width 7 | `Spark`, `Ray`, `Pool`, the `EffectKind30_Shards` bases; check the stride against the struct |
| community records | about 50 | no-struct | `rest_4a` / `rest_4b` / `rest_4d` / `rest_4e` / `field_e2`: only the pointers into 0x9046D0 / 0x9048B0 |
| `EncounterSlot` | about 18 | no-struct, width, field-missing | `field_hidden`, `inventory_ops`, `battle_sprites` |
| new `base_*` stat fields (party, enemy) | 36 | field-missing | `PartyMember +0xC0..+0xCB`, `BattleEnemy +0xD0..+0xDB` |

About 6,200 of the 7,982 rows, by family: magic (`SharedScratch`, the task
head), effect (`EffectWork`, the sub-pools, the packets, `SVector`), battle
(`ActionRecord`, `BattleWindow`, the enemy's base block), field and area
(the halves, the head), scenario and menus (`CharRecord`, `ItemListWindow`,
the packets), rest (community, encounter slots). What stays is mostly not
records: 746 `no-struct` rows on locals and `.data` tables (rest_3e's rays,
effect_2b's, effect_1c's per-kind tables, the 0x903EEC / 0x652E3C tables),
710 `width` rows on globals (`tex_page.cpp`'s `kTexKey` and the like), 172
`field-missing` and 115 `union` rows whose family the evidence does not
name.

**Traps.**

- **The head's union names compile on every family.** `task->actor`,
  `sprite->digit_phase`, `enemy->runner` all compile; each is right only on
  the family its comment names. The family audit after C2 should check every
  union member's use against its family (the gate cannot).
- **`BattleEnemy::enemy_kind` is `+0x8C`** (`kind` until C3's naming pass,
  which hid the head's `kind` at `+5`); an enemy's `+5` is `actor`.
- **State bytes merged through an index** (section 6.3): if the gate refuses
  `p->state` / `p->step`, try `p->states[k]`.
- **`SVector` is signed.** A `Word(v + 2)` read is unsigned; `v->y` is
  `movsx`. Cast to `std::uint16_t` where the site reads it unsigned, or the
  gate refuses it.
- **`EffectWork`, `ActionRecord`, the window views and the GPU packets are
  standalone structs.** A function that also reads head bytes needs two
  views of one pointer, `rv::As<records::EffectObject>(p)` and
  `rv::As<records::EffectWork>(p)`.
- **`EffectKind30_Shards` holds whichever pool the running kind lays there**
  (D201): type a pointer into it by the kind that runs, never by the
  address.

**For SG (C3), the entries these structs could type** (not edited by C1:
`symbols.toml` is SG's):

- `[[block]] SharedScratch` 0x903850: `records::SharedScratch`, extent 0x1C
  by its readers (0x30 at most: `Gfx_BufferBlocks` 0x903880 follows);
  `Scratch_Swap` 0x90385C is its `+0xC` dword.
- `[[block]] CharacterRecords` 0x903A70: `records::CharRecord`, count 8.
- `[[block]] WindowRecords` 0x803160: `records::WindowRecord`, count 22.
- `[[block]] EnemyWorkingRecords` 0x93B9E0: the `BattleEnemy` records at
  0x93B960 + 0x80 (the working record is `BattleEnemy +0x80..`); the table
  is `records::BattleEnemy[8]` at 0x93B960.
- `[[data]]` with byte ctypes that could take a struct: `Sprite_Objects`
  (`SpriteObject[30]`), `Sprite_ObjectsExtra` (`SpriteObject[4]`), `ObjTrio`
  (`PartyMember[3]`), `Effect_Objects` (`EffectObject[20]`),
  `Prim_VertexScratch` (`SVector[4]`), `EffectKindA0_Sparks`
  (`EffectGlowSpark[16]`), `EffectKindA0_Shards` (`EffectShard[32]`),
  `Save_Directory` (`SaveDirEntry[16]`). `EffectKind30_Shards` stays bytes.
- No entry yet: the encounter slots 0x939F00 (`EncounterSlot[8]`), the
  community records 0x9046D0 (`CommunityRecord[60]`) and buildings 0x9048B0
  (`CommunityBuilding[8]`), the battle tasks 0x93A000 (`BattleTask[48]`).

## 9. The fields and where each came from

The header's comments hold the same text; since C1 these tables are written
from the three headers by a script (the C1 scratch's `gen_tables.py`), so
they cannot drift from them. The section names are abbreviated:
"sprite-draw-order 14" is [`sprite-draw-order.md`](sprite-draw-order.md)
section 14, "battle_setup kPHp" is the constant in `battle_setup_callees.h`,
and so on. A struct "from `SpriteHead`" has `SpriteHead`'s table for its
first 0x80 bytes and lists only its own fields; the head's unions show as
two or three rows at one offset, each naming its family.

### `SpriteHead` (`0x80` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `flags` | bit 0 in use / present / taken, 1 no image upload, 5 semi-transparent, 6 not drawn, 7 hidden / culled (sprite-draw-order 5, 14; sprite-pose 3); the party's bit 0 present in battle (battle_setup_callees.h kPresent), the enemy's 0x40 defeated (battle_flow.md); the effect pool's non-zero in use (Effect_FindFree / Effect_RunObjects, symbols.toml); the task slot's non-zero runs (battle_flow.cpp BattleTask_Create / RunAll) |
| `+0x01` | u8 | `state` | the handler's state: a field object's Field_ObjectHandlers index (field-frame.md 2; movement-script.md F8), every effect kind dispatcher's jump index (effect_1a.md 2.1), the battle actor's first state byte (battle_setup_callees.h kState1), the task's state (battle_fx_tasks.md 1) |
| `+0x02` | u8 | `step` | the state's step: the effect kinds' sub-state tables by +2 (effect_*.md, e.g. EffectKind52_TrailSubStates), the actor's second state byte (kState2, battle_e2.cpp BattleEnemy_*); a field object's step byte to movement-script.md's F8 and event_ops.cpp's spawn (op[0xB]); zeroed with +3, +4 by BattleTask_FreeCurrent and by 0x589870 (symbols.toml) |
| `+0x03` | u8 | `state3` | the actor's third state byte (battle_setup_callees.h kState3); per effect kind a counter (effect_4e.md EffectKindA0 +3 a shade) |
| `+0x04` | u8 | `state4` | the actor's fourth state byte (kState4) |
| `+0x01` | u8[4] | `states` | the four state bytes +1..+4 as an array (battle_setup_callees.h kState1..4, "the actor's state bytes"). The same bytes as state, step, state3, state4; spelled as the array where two branches change different state bytes and the original's code merges them through an index (typed-records.md 6: EnemyOp_CastStart, BattleObj_CastStart, Field_LeaderStand) |
| `+0x05` | u8 | `actor` | party member, battle enemy: the actor index, a member's 0..2 (its palette slot in the field: area_entry_callees.h kRecPalette) and an enemy's slot + 3 (Battle_SetupEnemy, battle_sprites.cpp); battle_actions.cpp:353 reads the member's and the enemy's +5 as one actor |
| `+0x05` | u8 | `kind` | effect object, battle task: the handler's entry - Effect_KindHandlers[+5] (symbols.toml Effect_RunObjects; Effect_Spawn sets 6), and the entry +5 of the task runner's table (battle_fx_tasks.md 1: BattleFx_Dispatch's 19, BattleMagicFx_Dispatch's 110, Magic_Rows' row; BattleTask_Create's parameter) |
| `+0x06` | u8 | `type` | field sprite object: 0x0A a party object; 8 and up never turn (sprite-draw-order 5; sprite-pose 3). On a party record +6 is whom it follows (member-sprites 2; event_leader.md), not named |
| `+0x06` | u8 | `param` | effect object: Effect_Spawn's first argument (symbols.toml Effect_Spawn); per kind a gap or a row (effect_1a.md 2.6, effect_3a.md 1.2) |
| `+0x06` | u8 | `runner` | battle task: which of BattleTask_RunAll's four runners, 0 BattleFx_Dispatch, 1 BattleMagicFx_Dispatch, 2 BattleMagicRow_Run (battle_fx_tasks.md 1 calls it the kind) |
| `+0x07` | u8 | `flags2` | field sprite object: bit 3 not turned when talked to, bit 1 keeps its own facing rather than turning to the leader (field_frame.cpp:292-294; field-frame.md 2; shop_states.md ShopMode_End); bit 7 skips the blocked test (field_objects.cpp) |
| `+0x07` | u8 | `mode` | battle task: the damage popup's draw mode (battle_fx_tasks.md 1; battle_sprites.cpp:399-411, 558-559) |
| `+0x08` | u8 | `direction` | field sprite object, party member: facing, & 7 indexes Field_DirectionSteps (field_e2.cpp, area_w3e.cpp; member-sprites 2); the battle banner's side byte of a member (battle_win_states.md 2) |
| `+0x08` | u8 | `layout` | battle enemy: the layout ^ 2 (Battle_SetupEnemy); 2 / 3 pick the next animation (battle_flow.md); the banner reads it as the side byte (battle_win_states.md 2), enemy_ai_ops.cpp as a mode |
| `+0x09` | u8 | `step_frames` | field sprite object, party member: frames left in a step / a timed move; multiplies the step in Sprite_PointInReach (sprite-draw-order 6; movement-script 1b; member-sprites 2) |
| `+0x09` | u8 | `timer` | effect object, battle task: the kind's frame counter (effect_1a.md 2.3: EffectKind07_Start sets 0xF0, _FadeIn 0xFF); the popup's (battle_fx_tasks.cpp) |
| `+0x0A` | u8 | `digits` | battle task: the popup's digit count (battle_fx_tasks.cpp; battle_flow.cpp:483); per effect kind a wait (effect_1a.md 2.6 kind 5's +0xA); a field object's +0xA is unread |
| `+0x0C` | s32 | `step_x` | the step, 16.16 a frame (sprite-draw-order 6; member-sprites 2); dword, Effect_Spawn's movsx byte (symbols.toml). Per effect kind and per magic task a word of its own (B1 magic audit N2; effect_1a.md 2.5): EffectWork's work_0c for those |
| `+0x10` | s32 | `step_z` | the second step (sprite-draw-order 6); per kind and per task a word of its own, as +0xC |
| `+0x14` | s32 | `step_y` | the third step, 16.16: Sprite_ClearSteps clears +0xC, +0x10, +0x14 (field_event.cpp); the field objects' rise added to the elevation (field_o.cpp MoveCmd_OpF9; area_w1b.cpp adds +0x14 * +9 to +0x3C; move_cmds.cpp; kind2_object.cpp); a magic task's scratch dword (B1 magic audit) |
| `+0x18` | s32 | `work_18` | a handler's own dword: MoveCmd_OpF9's acceleration, added to +0xC each frame (field_o.cpp), and a handle from movement-script.md F8 and event_ops.cpp's spawn; a radius, a step or an angle offset in the magic tasks (B1 magic audit N3); kind 3's speed (effect_1a.md 2.5); kind 1's extra-record address (effect_1a.md 2.2). Was SpriteObject::accel_x, BattleTask::start_x |
| `+0x1C` | s32 | `work_1c` | likewise: MoveCmd_OpF9's second acceleration (field_o.cpp), F8's and the spawn's handle word, the magic tasks' scratch. Was SpriteObject::accel_z, BattleTask::start_y |
| `+0x20` | s32 | `work_20` | likewise: cleared by Battle_SetupEnemy; field_c3.cpp's gravity added to +0x14; kind 5's speed (effect_1a.md 2.6); the magic tasks' scratch (92 rows) |
| `+0x24` | u8 | `draw_flags` | bit 0 frames uploaded, 1 member set-up pending, 2 CLUT row 0x1F0, 6 a 3D object, 7 overlay list (sprite-pose 3; sprite-draw-order 5, 14; movement-script 1c) |
| `+0x25` | u8 | `tex_slot` | texture slot (sprite-pose 3) |
| `+0x26` | u8 | `tex_row` | the slot's y row (sprite-pose 3) |
| `+0x27` | u8 | `clut` | palette (sprite-draw-order 14 Sprite_ClutWord; battle_fx_tasks.cpp DamagePopup) |
| `+0x28` | u8 | `clut_mode` | palettes to a row; non-zero is "wide" (sprite-draw-order 14) |
| `+0x29` | u8 | `ot_slot` | the draw slot Gfx_CommitPrim takes (sprite-draw-order 14; area_backdrop.cpp kTrioSlot; member-sprites 2); 4 for an enemy (Battle_SetupEnemy) |
| `+0x2A` | u8 | `flip` | the draw's flip: non-zero ORs 0x400 into the mode word (sprite-draw-order 14), set from bit 7 by BattleEnemy_SetAnimation (battle_flow.md "the flip byte"); written as a pose byte's 0 / 1 by sprite-pose 3's readers. Was SpriteObject::pose |
| `+0x2B` | u8 | `key_class` | row of Sprite_KeyAdjust (sprite_screen.cpp 0x588F20) |
| `+0x2C` | u16 | `column` | frame-offset table column (sprite-pose 3; area_entry_callees.h kRecColumn); dispatched as "the form word" by rest_1b.cpp ByForm; on a task the copied owner's (battle_fx_tasks.md: indexed by the owner's +0x2C) |
| `+0x2E` | s16 | `screen_x` | the truncated screen point (movement-script 1c; sprite-draw-order 14; battle_actions.cpp SetMemberBlock / SetEnemyBlock; area_011.cpp); Effect_Spawn's word arguments (symbols.toml). Per effect kind a word of its own (EffectWork's work_2e) |
| `+0x30` | s16 | `screen_y` | as +0x2E; EffectWork's work_30 per kind |
| `+0x32` | u16 | `draw_key` | layer << 8 / sort byte (sprite-draw-order 2; movement-script 1c; battle_actions.cpp SetMemberBlock / SetEnemyBlock) |
| `+0x32` | u16 | `digit_phase` | battle task, the damage popup: the digit phase (battle_fx_tasks.cpp PopupStart; battle_flow.cpp:477-485), started from the owner's draw key (battle_sprites.cpp:564) |
| `+0x34` | s32 | `x` | position, 16.16 (sprite_screen.cpp; area_backdrop.cpp kTrioPosition; Battle_SetupEnemy; battle_setup kPos; battle_fx_tasks.cpp); a dword read by AreaMap_Elevation (symbols.toml) |
| `+0x34` | u16 | `x_frac` | x's fraction, read alone as a word (B1 width rows, 30 sites with +0x38) |
| `+0x36` | u16 | `x_whole` | x's whole part, the cell: read alone as a u16 (sprite_screen.cpp; member-sprites 2 "+0x36 the cell"; the spawn 0x5898D0 writes the halves apart, symbols.toml) |
| `+0x38` | s32 | `z` | position, 16.16 (the same readers); a battle task's world z (B1 magic audit N1: all 331 uses; was BattleTask::y) |
| `+0x38` | u16 | `z_frac` | z's fraction |
| `+0x3A` | u16 | `z_whole` | z's whole part, the cell (member-sprites 2) |
| `+0x3C` | s32 | `height` | the pass's third sort key (sprite-draw-order 2); the third dword of the 12-byte point at +0x34 (battle_setup_callees.h kPos; effect_3a_callees.h kLeaderPoint) |
| `+0x3C` | u16 | `height_frac` | the height's fraction |
| `+0x3E` | u16 | `height_whole` | the height's whole part: the elevation Sprite_ObjectMatrix reads as s16 (sprite-draw-order 12), the ground (member-sprites 2; Battle_SetupEnemy), the u16 the spawn 0x5898D0 writes (symbols.toml) |
| `+0x40` | s32 | `scale_x` | 16.16 (sprite-draw-order 12, 14) |
| `+0x44` | s32 | `scale_y` | 16.16 (sprite-draw-order 14) |
| `+0x48` | u8 | `scale_mode` | 1: scale by depth; 0: 1.0 (sprite-draw-order 12, 14) |
| `+0x49` | u8 | `script_steps` | step count (sprite-draw-order 6 Sprite_ScriptStart) |
| `+0x4A` | u8 | `script_hold` | ticks left on the step (sprite-draw-order 5, 6) |
| `+0x4B` | u8 | `animation` | the current animation (sprite-pose 3); 0xFF on a member swap (area_entry.cpp:282) and at an enemy's set-up (Battle_SetupEnemy); saved to the member's +0x12F on a hit (battle_e3.cpp:380) |
| `+0x4C` | u32 | `anim_data` | address: the animation data (sprite-pose 3) |
| `+0x50` | u32 | `script` | address: the byte script / a 3D object's quads (sprite-draw-order 5, 12) |
| `+0x54` | u32 | `frames` | address: the frame-word table (sprite-draw-order 5, 6) |
| `+0x58` | u16 | `script_pos` | (sprite-draw-order 5); saved to the member's +0x140 on a hit (battle_e3.cpp:381); an enemy's pose word (boss_sc_callees.h kEnemyPose) |
| `+0x5A` | u16 | `frame_word` | (sprite-draw-order 5, 14) |
| `+0x5C` | u8 | `draw_mode` | into the mode word (sprite-draw-order 14); written on a member's entry (area_entry.cpp; area-entry.md) |
| `+0x5D` | u8[3] | `shade` | (sprite-draw-order 12, 14; effect_1a.md 2.3: EffectKind07 zeroes them and fades them up) |
| `+0x60` | s32 | `depth` | the projection's depth (movement-script 1c; sprite-draw-order 14; effect_1a.md 2.2) |
| `+0x60` | s32 | `value` | battle task, the damage popup: the number shown (battle_fx_tasks.cpp; read as a word by the draw) |
| `+0x64` | u32[3] | `angle` | the rotation angles in the low words (sprite-draw-order 12); +0x64 / +0x68 also the member / its row during a member set-up (sprite-pose 3); MoveCmd_OpF9 runs a party object's tilt counts into them (field_o.cpp); effect kind 0x13 (the camera turn): the three target Camera_Angles, dwords (CameraTurn_Start, frame-callees.md; area_w3f.cpp Kind13) |
| `+0x70` | u32 | `reach` | dword (sprite-pose 3: its low byte ORed from the bank's first byte); the low byte the object's reach: Sprite_PointInReach's margin (sprite-draw-order 6), Party_MemberAt's (area_w1c.md Area51_MemberAtObject), blocked_at's and AreaMap_CellsNone's size (member_sprites.cpp; field_c3.cpp), the jump's scale (field_c3.md); member-sprites 2 calls a member's non-zero byte "raised" (Field_CellAhead splits on it). Was PartyMember::raised |
| `+0x74` | u32 | `screen_xf` | the float screen point's bits (movement-script 1c; sprite_screen.cpp; effect_4d.md EffectKind98_DrawBurst) |
| `+0x78` | u32 | `screen_yf` |  |
| `+0x7C` | u32 | `depth_f` | the float projected depth's bits (rest_4f.cpp:1215 F(s + 0x7C); effect_4d.md EffectKind98_DrawBurst) |

Padding:

- a field object's Effect_Objects index for ops 9x (symbols.toml Effect_Spawn); member-sprites 2's effect-object index; an effect's spawn argument (0x5898D0) or per kind a hold; compared with 1 by the popup's PopupStart (y 9 higher). One byte, four readings, none shared

### `SpriteObject` (`0xA4` bytes, from `SpriteHead`)

| off | type | field | source |
|---|---|---|---|
| `+0x80` | u8 | `context` | the script context's first byte: flags (movement-script 1b; field_objects.cpp) |
| `+0x81` | u8 | `pose_wait` | (field_objects.cpp) |
| `+0x82` | u8 | `loop` | the context's loop counter (movement-script 1b) |
| `+0x83` | u8 | `area_script` | which of the area's scripts (movement-script 1b) |
| `+0x84` | u8 | `speed` | index into Field_MoveSpeeds (movement-script 1b) |
| `+0x85` | u8 | `saved_facing` | the facing kept while talked to, / 1 for a state-0x0A object (field_frame.cpp:293,300); put back to +8 (mode_states.cpp:250; shop_states.md ShopMode_End; field-frame.md 2) |
| `+0x86` | u8 | `trigger` | the object-trigger id: Field_ObjectTriggers by object[+0x86] (area-rows.md) |
| `+0x87` | u8 | `move_steps` | steps left in a timed move (movement-script 1b) |
| `+0x88` | u16 | `trigger_word` | written op[0xD] << 8 / op[0xE] by the spawn (event_ops.cpp:201); 0xFFFF none (event_ops.cpp:986) |
| `+0x8A` | u16 | `script_position` | the context's script position (movement-script 1b) |
| `+0x8C` | s32 | `target_x` | MoveCmd_OpF9's target on Field_ActiveMember, 16.16 (field_o.cpp); the placement op's home (wide_staging.cpp:572); zeroed by event_script.cpp:494 |
| `+0x90` | s32 | `target_z` | (the same readers) |
| `+0x94` | u32 | `best_score` | Field_ObjectBestDirection's running score, started 0 or 0xFFFFFF (field_c3.md); zeroed by field_o.cpp:1229 |
| `+0x98` | u16 | `half_w` | the object's half-widths, /d/ >> 16 against them (field_c3.md Field_ObjectApproachDirection); op[3] (event_ops.cpp:199; event-ops.md) |
| `+0x9A` | u16 | `half_d` | op[4] (event_ops.cpp:200) |
| `+0x9C` | u16 | `counter` | counted up for every object in reach of the point ahead (event-ops.md); a u16 zeroed by field_objects.cpp |
| `+0x9E` | u8 | `tag` | the boss set-ups' tag (boss_spawn.h; field_o.cpp:1157 op[0x10]; psp-battle-changes.md) |
| `+0xA0` | u8 | `handler` | & 0x7F the area object handler, 0x7F none (rest_2a.md Area_ObjectHandler); 0x7F at a spawn (event_ops.cpp:204,259; field_o.cpp:1158) |

Padding:

- area_w0c.md Area32_TintStart puts a slot in the active member's +0x9F; one reader, meaning not settled
- +0xA1 written by field_objects.cpp, not named

### `PartyMember` (`0x14C` bytes, from `SpriteHead`)

| off | type | field | source |
|---|---|---|---|
| `+0x89` | u8 | `char_id` | record +0x09 (battle_setup_callees.h kPCharId; area_entry_callees.h kRecMember) |
| `+0x8A` | u8 | `level` | record +0x0A (battle_damage_callees.h kPLevel, working +0x7A) |
| `+0x90` | u16 | `status` | record +0x10 (battle_setup kPStatus; battle_damage kPStatus) |
| `+0x92` | u8 | `weapon` | record +0x12 (battle_setup kPWeapon) |
| `+0x96` | u8[2] | `accessory` | record +0x16 / +0x17 (battle_setup kPAccessory) |
| `+0x98` | u16 | `hp` | record +0x18 (battle_setup kPHp; battle_damage kPHp) |
| `+0x9A` | u16 | `ap` | record +0x1A |
| `+0x9C` | u8 | `survive` | record +0x1C, set from +0xAE (battle_setup kPByte9C); the overkill a kill leaves the flag below (battle_damage kPSurvive) |
| `+0xA0` | u16 | `max_hp` | record +0x20 (battle_setup kPMaxHp) |
| `+0xA2` | u16 | `max_ap` | record +0x22 |
| `+0xA4` | u16 | `atk` | record +0x24 (char-stats.md 2; rest_3b.cpp ActorWord(actor, 0xA4, 0xB4); battle_e5.cpp:408; the stat copy's +4, battle_damage kAttackerAtk) |
| `+0xA6` | u16 | `def` | record +0x26 (battle_damage kPDef) |
| `+0xA8` | u16 | `agi` | record +0x28 (battle_setup kPAgi; battle_damage kPAgi) |
| `+0xAA` | u16 | `intel` | record +0x2A (char-stats.md 2; the stat copy's +0xA, battle_damage kAttackerInt / kTargetInt) |
| `+0xAE` | u8 | `byte_ae` | record +0x2E, copied to +0x9C (battle_setup kPByteAE) |
| `+0xAF` | u8[5] | `elements` | affinity classes, fire .. wind (battle_damage kPElements) |
| `+0xB4` | u8 | `holy` | holy affinity class (battle_damage kPHoly) |
| `+0xC0` | u16 | `base_max_hp` | record +0x40, the base copy of +0xA0 (char-stats.md 2; item-use.md 2; battle_e5.cpp:405-425) |
| `+0xC2` | u16 | `base_max_ap` | record +0x42 |
| `+0xC4` | u16 | `base_atk` | record +0x44 |
| `+0xC6` | u16 | `base_def` | record +0x46 |
| `+0xC8` | u16 | `base_agi` | record +0x48 |
| `+0xCA` | u16 | `base_intel` | record +0x4A |
| `+0x124` | u8 | `command` | battle: the command auto-battle gives (docs/battle_e4.md); field: the script context's first byte (area_entry_callees.h kRecContext). ActionRecord::command / MemberFieldTail::context |
| `+0x125` | u8 | `command_kind` | ActionRecord::command_kind (was queued_kind; C3 naming): 5 an item, 4 an ability (battle_setup kPQueuedKind; battle_damage kPCommand) |
| `+0x126` | u16 | `queued_id` | category << 8 / id, or the ability id (battle_setup kPQueuedItem; battle_damage kPCommandId) |
| `+0x128` | s16 | `hp_change` | battle: the result record's +4, read signed (battle_setup kPHpChange; battle_e3.cpp:486,499,898 SWord); field: its low byte the walking speed (member-sprites 2) |
| `+0x12A` | s16 | `ap_change` | battle: the result record's +6, read signed (battle_setup kPApChange; battle_e3.cpp:499) |
| `+0x12C` | u8 | `shown` | status-shown bits, 0x11 / 2 with an effect (battle_setup kPEffect; battle_damage kPShown) |
| `+0x12D` | u8 | `counter` | the 0x40 / 0x20 statuses' counter (battle_setup kPCounter); battle_damage kPShown2 "cleared with a cured status" |
| `+0x12E` | u8 | `item_slot` | (battle_setup kPItemSlot) |
| `+0x12F` | u8 | `saved_animation` | +0x4B saved by BattleObj_HitEnter (battle_e3.cpp:380) |
| `+0x130` | u32 | `flags_130` | battle: flag dword (battle_damage kPFlags; battle_sprites.cpp "the party's flags dword"); field: the script pointer (area_entry_callees.h kRecScript) |
| `+0x134` | u32 | `flags_134` | battle: flag dword (battle_setup kPFlags; battle_damage kPFlags2); field: byte +0x136 the standing counter, +0x137 the action (member-sprites 2) - one dword in battle, two bytes in the field, so the bytes are not separate members (MemberFieldTail has them) |
| `+0x138` | u8 | `field_flags` | field: bit 0, bit 3 CLUT pending (area_backdrop.cpp kTrioFlags; member-sprites 2) |
| `+0x139` | u8 | `field_object` | field: a Sprite_Objects index (field_e1.cpp:478); battle zeroes +0x138..+0x13F (battle_e3.cpp:691; battle_e6.cpp:217-218; battle_phases.cpp:222-223) |
| `+0x140` | u16 | `saved_script_pos` | the sprite's +0x58 saved by BattleObj_HitEnter (battle_e3.cpp:381) |
| `+0x142` | u8 | `count_4000` | (battle_setup kPCount4000) |
| `+0x143` | u8 | `count_800` | (battle_setup kPCount800) |
| `+0x144` | u8 | `count_40` | zeroed with flag 0x40 (battle_setup kPCount40); battle_damage kPCharge, the attack charge |
| `+0x145` | u8 | `count_80` | zeroed with flag 0x80 (battle_setup kPCount80); battle_damage kPCharge2, the ability charge |
| `+0x148` | u8 | `char_index` | the character record's index, in the field and in battle (area_entry_callees.h kRecCharacterIndex; effect_3a_callees.h kMemberFlag; battle_e3.cpp:684,723; battle_e4.cpp:126; battle_e6.cpp:318). battle_damage_callees.h kPCharIndex (0x138 from 0x802D50; kPWeaponKind until stage B1) is this byte: Battle_CalcDamage's rule is "character index 5" (docs/typed-records.md 3) |
| `+0x149` | u8 | `tint` | field: the running script's tint record, MoveScript_TintRecords by it (area_w1a.cpp:134) |

Padding:

- the character record's name (+0..+8, CharRecord::name)
- character record +0x0B..+0x0F; +0x8B bit 0 "joined" per char-stats.md, no member reader addresses it; +0x8C the experience (CharRecord::exp)
- armour (battle_setup kPArmour3 names only +0x95)
- record +0x1D..+0x1F; +0x9E the max-HP scale (battle_e5.cpp:403; char-stats.md 2), not named here
- record +0x2C, +0x2D; not named
- the rest of the character record's effective block: +0xB5..+0xB7 the rest of the nine resistance bytes, +0xB8..+0xBC the five percentage bytes (+0xBA one, battle_e3.cpp:1040; char-stats.md 2); not named here
- the rest of the base block (record +0x4C..+0x5F) and record +0x60..+0xA3 (CharRecord); not named here
- zeroed with +0x138 in battle; not named
- unread
- unread

### `EffectObject` (`0x80` bytes, from `SpriteHead`)

No members of its own: every byte is `SpriteHead`'s.

### `BattleEnemy` (`0x128` bytes, from `SpriteHead`)

| off | type | field | source |
|---|---|---|---|
| `+0x80` | char[12] | `name` | from EnemyData +0 (battle_sprites.md 3) |
| `+0x8C` | u8 | `enemy_kind` | EnemyData::kind (+0xC); was kind, which hid the head's kind at +5 (C3 naming); grouped by Battle_OpenEnemyNames (battle_sprites.cpp) |
| `+0x8D` | u8 | `family` | EnemyData +0xD (battle_damage kEFamily) |
| `+0x8E` | u8 | `odds_index` | EnemyData +0xE (Battle_CopyEnemyData) |
| `+0x8F` | u8 | `kind_flag` | Battle_EnemyKindFlag (Battle_SetupEnemy) |
| `+0x90` | u16 | `flags_90` | EnemyData +0x12 (Battle_CopyEnemyData); bit 1 read by BattleEnemy_Chance70 (battle_flow.cpp) |
| `+0x92` | u16 | `status` | (battle_setup kEStatus; battle_damage kEStatus); 0x4000 dead (battle_flow.cpp) |
| `+0x94` | u16 | `zenny` | (battle_flow.cpp, the kill) |
| `+0x96` | u16 | `exp` | (battle_flow.cpp, the kill) |
| `+0x98` | u16 | `level` | battle_damage kELevel: u8, also read as a u16 |
| `+0x9A` | u16 | `bank` | the animation bank, EnemyData +0x1A (Battle_SetupEnemy) |
| `+0x9C` | u8[8] | `abilities` | EnemyData +0x1C (Battle_CopyEnemyData) |
| `+0xA4` | u16 | `hp` | (battle_setup kEHp; battle_damage kEHp) |
| `+0xA6` | u16 | `ap` | (battle_damage kEAp) |
| `+0xA8` | u16 | `item_a8` | EnemyData +0x30's low word (Battle_CopyEnemyData): an item word, Identify_DrawEnemy hands it to Identify_DrawItem (magic_s12.cpp:448) (C3 naming) |
| `+0xAA` | u8 | `steal_row` | the steal-rate row, SkillSteal_RateTable[+0xAA], unbounded (magic_steal.cpp SkillSteal_Roll; magic_steal.md) |
| `+0xAC` | u16 | `item_ac` | EnemyData +0x34's low word: the second item word Identify_DrawEnemy hands to Identify_DrawItem (magic_s12.cpp:450) (C3 naming) |
| `+0xB0` | u16 | `max_hp` | (battle_setup kEMaxHp) |
| `+0xB2` | u16 | `max_ap` | (battle_damage kEMaxAp) |
| `+0xB4` | u16 | `atk` | the stat copy's +4 (battle_damage kAttackerAtk; rest_3b.cpp ActorWord(actor, 0xA4, 0xB4)) |
| `+0xB6` | u16 | `def` | the stat copy's +6 (battle_damage kTargetDef; rest_3b.cpp:464; rest_3b.md) |
| `+0xB8` | u16 | `agi` | (battle_setup kEAgi) |
| `+0xBA` | u16 | `intel` | the stat copy's +0xA (battle_damage kAttackerInt / kTargetInt); BattleEnemy_PickAction tests it <= 1 (battle_e2.md). magic_steal.md R22 calls +0xBA the speed, but SkillSteal_Roll reads +0xB8 (magic_steal.cpp): a doc conflict |
| `+0xBF` | u8[5] | `elements` | (battle_damage kEElements) |
| `+0xC4` | u8 | `holy` | (battle_damage kEHoly) |
| `+0xD0` | u16 | `base_max_hp` | the stat block's base copy, +0x20 after the working one as the character record's +0x40 after +0x20 (char-stats.md 2): EnemyAI_ScaleStat caps HP +0xA4 at it (battle_e5.md) |
| `+0xD2` | u16 | `base_max_ap` | EnemyAI_ScaleStat caps AP +0xA6 at it (battle_e5.md) |
| `+0xD4` | u16 | `base_atk` | scaled and capped at 999 by EnemyAI_ScaleStat (battle_e5.md); the base copy of +0xB4 |
| `+0xD6` | u16 | `base_def` | likewise, of +0xB6 |
| `+0xD8` | u16 | `base_agi` | likewise, of +0xB8 |
| `+0xDA` | u16 | `base_intel` | likewise, of +0xBA |
| `+0xF0` | u8 | `data_id` | the id's low byte: indexes EnemyData (battle_e3_callees.h kEnemyData8B; battle_e2.md) and the AI scripts (battle_damage kEScript; Battle_CopyEnemyData) |
| `+0xF1` | u8 | `ai_rows_done` | a bit per AI row fired; DIV-0082 (battle_damage kERowsDone) |
| `+0xF2` | s8 | `screen_dx` | added to the screen point by the popup and the action block (battle_fx_tasks.cpp BattleFx_PopupStart; battle_actions.cpp SetEnemyBlock); from EnemyData +0x78..+0x7B (Battle_SetEnemyOffset) |
| `+0xF3` | s8 | `screen_dy` |  |
| `+0xF4` | u32 | `event_hook` | address, called with 2 in an event battle (battle_flow.md) |
| `+0xF8` | u32 | `cue_table` | address of the sound cues (battle_items.cpp:724 Long(r + 0xF8); enemy_ai_ops.md; battle_e2.md; boss_sb.md) |
| `+0xFC` | u32 | `anim_table` | address of the animation bytes (battle_flow.cpp BattleEnemy_SetAnimation) |
| `+0x100` | u8 | `run_state` | indexes the state table 0x64B084 (battle_flow.md); EnemyData +0x88 |
| `+0x101` | u8 | `hit_class` | EnemyData +0x89; the hit-sound class (battle_sprites.cpp:534 Battle_PlayHitSound) |
| `+0x104` | u8 | `command` | ActionRecord::command, the action record's first byte (was result_head; C3 naming): 0x904B60 points here (battle_actions.cpp SetEnemyBlock; battle_actions.md; enemy_ai_ops.md); the party's is +0x124 |
| `+0x105` | u8 | `command_kind` | ActionRecord::command_kind: 5 an item, 4 an ability (battle_damage kECommand); was command (C3 naming) |
| `+0x106` | u16 | `ability` | the ability id, one of +0x9C by Rand() & 7 (battle_e2.cpp:828,866; battle_e2.md BattleEnemy_PickAction) |
| `+0x108` | s16 | `hp_change` | the result record's +4, signed (battle_setup kEHpChange; battle_e2.cpp:1133; boss_sd.cpp:404; boss_sa.cpp:394; battle_e2.md "the s16 +0x108") |
| `+0x10A` | s16 | `ap_change` | the result record's +6, signed like +0x108 (battle_setup kEApChange) |
| `+0x10C` | u8 | `shown` | (battle_setup kEEffect; battle_damage kEShown) |
| `+0x10D` | u8 | `counter` | (battle_setup kECounter; battle_damage kEShown2) |
| `+0x110` | u32 | `flags_110` | bit 15, bits 0x70, the popup's 0x80 (battle_damage kEFlags; battle_flow.cpp; battle_fx_tasks.cpp) |
| `+0x114` | u32 | `flags_114` | bit 0x10 (battle_setup kEFlags; battle_damage kEFlags2) |
| `+0x122` | u8 | `count_4000` | (battle_setup kECount4000) |
| `+0x124` | u8 | `count_40` | (battle_setup kECount40); battle_damage kECharge |
| `+0x125` | u8 | `count_80` | (battle_setup kECount80); battle_damage kECharge2 |

Padding:

- +0xAB: the rest of the EnemyData +0x30 dword; its marker bit raises +0xAA once (rest_3c.md Effect_EnemyRaiseAAAE); not named
- +0xAE, +0xAF: the rest of the EnemyData +0x34 dword; +0xAE raised with +0xAA, each once by the marker bits of +0xAB / +0xAF (rest_3c.md Effect_EnemyRaiseAAAE); not named
- the stat copy's +0xC..+0xE; not named
- the rest of the stat block copy (+0xC5..+0xC7 the last resistance bytes, Battle_CopyEnemyData; +0xC8.. the percentage bytes as the character record's +0x38..); not named
- the rest of the base copy (+0xDF.. the resistance bytes, Battle_CopyEnemyData); not named
- unread
- unread
- +0x118..+0x11F zeroed at set-up, unnamed
- zeroed at set-up, unnamed
- unread

### `EnemyData` (`0x8C` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | char[12] | `name` | (enemy_ai.py; battle_sprites.md 3) |
| `+0x0C` | u8 | `kind` | to BattleEnemy +0x8C; Battle_EnemyKindFlag's argument |
| `+0x0D` | u8 | `family` | to BattleEnemy +0x8D |
| `+0x0E` | u8 | `odds_index` | the base action-odds index (enemy_ai.py) |
| `+0x12` | u16 | `flags` | (enemy_ai.py); to BattleEnemy +0x90 |
| `+0x14` | u16 | `zenny` | to BattleEnemy +0x94 |
| `+0x16` | u16 | `exp` | to BattleEnemy +0x96 |
| `+0x18` | u16 | `level` | (enemy_ai.py reads a u16) |
| `+0x1A` | u16 | `bank` | the animation bank (Battle_SetupEnemy) |
| `+0x1C` | u8[8] | `abilities` | one picked by Rand & 7 (enemy_ai.py) |
| `+0x24` | u16 | `hp` | (enemy_ai.py) |
| `+0x26` | u16 | `ap` |  |
| `+0x28` | u16[4] | `stats` | names hypotheses (enemy_ai.py) |
| `+0x38` | u8[64] | `ai_rows` | four rows of 16 bytes (enemy_ai_data.md 1; enemy_ai.py) |
| `+0x78` | s8[4] | `place` | x / z offsets by side, copied or negated to BattleEnemy +0xF2 / +0xF3 (Battle_SetEnemyOffset, battle_sprites.cpp:997-1007) |
| `+0x7C` | u8[9] | `resist` | to BattleEnemy +0xDF.. and +0xBF.. (Battle_CopyEnemyData) |
| `+0x85` | u8 | `status` | to BattleEnemy +0x92 (Battle_CopyEnemyData) |
| `+0x86` | u8 | `size` | the on-screen test's size (field_hidden.cpp:525; field_hidden_callees.h kKinds) |
| `+0x87` | u8 | `margin` | (field_hidden.cpp:525; field_hidden_callees.h kKinds); battle_e2.md BattleFx_PlaceOverOwner uses it as the y |
| `+0x88` | u8 | `run_state` | to BattleEnemy +0x100 (Battle_CopyEnemyData) |
| `+0x89` | u8 | `hit_class` | to BattleEnemy +0x101, the hit-sound class (battle_sprites.cpp:534) |
| `+0x8A` | u8 | `fx_size` | (battle_items_callees.h kEnemyFxSize = 0x8C55C8 + 0x8A; boss_sj_callees.h; boss_si_callees.h names the same byte kEnemyDataCount) |
| `+0x8B` | u8 | `cast_fx` | (battle_e3_callees.h kEnemyData8B, by the enemy's +0xF0; battle_e3.md EnemyOp_CastStart) |

Padding:

- unread
- two dwords copied to BattleEnemy +0xA8 / +0xAC, unnamed

### `BattleTask` (`0x84` bytes, from `SpriteHead`)

| off | type | field | source |
|---|---|---|---|
| `+0x80` | u32 | `owner` | address of the owning actor (battle_flow_callees.h kTaskOwner); BattleTask_RunAll copies it to 0x93B940 (B1 magic audit) |

### `WindowRecord` (`0x24` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` | Window_Alloc sets 1, Window_FreeCurrent clears it (window-task.md 3) |
| `+0x01` | u8 | `handler` | index into Field_RunTaskRecords' nine |
| `+0x02` | u8 | `kind` | the window kind, per handler: 0..2 for the field windows (window-task.md 3, 4), 0..7 for the battle windows (battle_win_states.md 2; written 7 battle_phases.cpp:286, 6 battle_setup.cpp:121, 4 battle_sprites.cpp:970 and battle_result.cpp:148, 5 battle_menu_states.cpp:348), and by the menus 5 (rest_2d.cpp:810) and 0xB (rest_2f.cpp:843); the fuzz seeds 0..7 (battle_win_states_fuzz.cpp) |
| `+0x03` | u8 | `state` | 0..4 |
| `+0x04` | u16 | `x` | the box's left edge: Window_Grow moves it back half the width step while the width grows, which keeps the box centred only on a left edge (window_task.cpp Window_Grow, HalfStep); 12.4 in the field windows (kind 1 writes << 4, rest_2f.cpp), whole pixels in the menus (Menu_DrawBox at +4 + 3, text at +4 + 7, menu_windows.cpp; MenuSlide_RightTo80 clamps it at 0x50, rest_2f.cpp) and the battle windows (battle_win_states.md 2; menu-windows.md 2 "x, y (s16)"). Was centre_x: window-task.md 3 said "centre" until the C3 naming pass (B1 audit rbs section 4) |
| `+0x06` | u16 | `y` | the top edge, as +4. Was centre_y |
| `+0x0A` | u8 | `param_a` | per handler: the half-width in pixels for the field windows (window-task.md 3); the actor slot for battle handler 3 (battle_win_states.md 2; battle_sprites.md 3); the item category for the list windows, the greyed / price percent in the buy list (menu-windows.md 2). The per-handler views are BattleWindow and ItemListWindow |
| `+0x0B` | u8 | `half_height` |  |
| `+0x0D` | u8 | `flags` | bit 0 no frame, bit 1 the narrow box |
| `+0x0F` | u8 | `pass` | 0..2 |
| `+0x10` | u16 | `width` | 12.4 |
| `+0x12` | u16 | `height` | 12.4 |
| `+0x14` | u16 | `width_step` |  |
| `+0x16` | u16 | `height_step` |  |
| `+0x18` | u16 | `settled_x` | kind 1's settled place (Window_Grow snaps +4 to it) |
| `+0x1A` | u16 | `settled_y` |  |

Padding:

- not in window-task.md 3
- not in window-task.md 3
- not in window-task.md 3

### `SaveSummary` (`0x1C` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | char[5] | `name` | five bytes (save-interchange.md; Menu_DrawSaveSlot) |
| `+0x05` | u8[3] | `party` | icons, 0xFF none; +5 the leader's char_id (save-interchange.md) |
| `+0x08` | u8 | `level` | (Menu_DrawSaveSlot) |
| `+0x09` | u8 | `style` | the window style byte 0x903A5A, written by Save_BuildBlock (rest_2c.cpp:397) |
| `+0x0A` | u8 | `style2` | 0x903A5B (rest_2c.cpp:398) |
| `+0x0C` | u8 | `hours` | the play clock 0x9040C8..CB copied whole (rest_2c.cpp:396); drawn first (field_o.cpp Menu_DrawSaveSlot) |
| `+0x0D` | u8 | `minutes` | drawn second (Menu_DrawSaveSlot) |
| `+0x0E` | u8 | `seconds` | (map_field_objects_callees.h: hours, minutes, seconds, frames) |
| `+0x0F` | u8 | `frames` | 30 a second |
| `+0x10` | u32 | `exp` | Menu_DrawExpBar's argument |
| `+0x14` | u8 | `marked` | non-zero: frame flags 2 and piece 0x3D (Menu_DrawSaveSlot) |
| `+0x15` | u8 | `dir_index` | Save_Directory index, 0xFF empty (save-files.md 2) |
| `+0x16` | char[4] | `name_tail` | copied after the name (Menu_DrawSaveSlot) |

Padding:

- unwritten by Save_BuildBlock, unread
- unread

### `SaveDirEntry` (`0x18` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | char[20] | `name` | the file name, NUL-ended; the writer copies strlen + 1 unbounded, the extent here is the gap to +0x14 |
| `+0x14` | u32 | `size` | the file size |

### `ScriptContext` (`0xC` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `flags` | bits 0x40 / 0x80 set and cleared by the step's ops (move_script.cpp 188-299); SpriteObject::context |
| `+0x01` | u8 | `pose_wait` | op 0x5x stores op & 0xF (MoveScript_Step); SpriteObject::pose_wait |
| `+0x02` | u8 | `loop` | the loop counter of op 02 (move_script.cpp:123-130; movement-script.md 1b) |
| `+0x03` | u8 | `area_script` | which of the area's scripts (movement-script.md 1b) |
| `+0x04` | u8 | `speed` | index into Field_MoveSpeeds; 3 after a reset (ScriptContext_Reset; movement-script.md 1b); the member's walking speed (member-sprites 2: PartyMember +0x128's low byte) |
| `+0x05` | u8 | `saved_facing` | cleared by ScriptContext_Reset; the facing kept while talked to (SpriteObject +0x85, field_frame.cpp:293,300) |
| `+0x07` | u8 | `move_steps` | steps left in a timed move: op 1x-4x stores op & 7 (MoveScript_Step; movement-script.md 1b) |
| `+0x0A` | u16 | `position` | the script position: move_script_bytes.h Pos / SetPos, every store 16 bits |

### `MemberFieldTail` (`0x1C` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | ScriptContext | `context` | (+0x124) area_entry_callees.h kRecContext; ScriptContext_Reset |
| `+0x0C` | u32 | `script` | (+0x130) the script pointer: area_entry_callees.h kRecScript, set from the descriptor's +0x18 list (area_entry.cpp:210); the interpreter's third caller's "pointer at struct +0x130" (movement-script.md 3) |
| `+0x12` | u8 | `standing` | (+0x136) the standing counter (member-sprites 2) |
| `+0x13` | u8 | `action` | (+0x137) the action (member-sprites 2) |
| `+0x14` | u8 | `field_flags` | (+0x138) bit 0, bit 3 CLUT pending (area_backdrop.cpp kTrioFlags; member-sprites 2); PartyMember::field_flags |
| `+0x15` | u8 | `field_object` | (+0x139) a Sprite_Objects index (field_e1.cpp:478); PartyMember::field_object |

Padding:

- no field reader named
- no field reader named

### `ActionRecord` (`0x1C` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `command` | the command: auto-battle gives 3 (battle_e4.md AutoBattle_FillCommands); PartyMember::command, BattleEnemy::command |
| `+0x01` | u8 | `command_kind` | 5 an item, 4 an ability (battle_setup kPQueuedKind; battle_damage kPCommand / kECommand); PartyMember::command_kind, BattleEnemy::command_kind |
| `+0x02` | u16 | `action_id` | the action id (battle_actions.md): category << 8 / id or the ability id (battle_setup kPQueuedItem; battle_damage kPCommandId); the enemy's ability (battle_e2.cpp:828,866). PartyMember::queued_id, BattleEnemy::ability |
| `+0x04` | s16 | `hp_change` | read signed (battle_setup kPHpChange / kEHpChange; battle_e3.cpp:486; battle_e2.cpp:1133) |
| `+0x06` | s16 | `ap_change` | (kPApChange / kEApChange) |
| `+0x08` | u8 | `shown` | status-shown bits (battle_setup kPEffect / kEEffect; battle_damage kPShown / kEShown) |
| `+0x09` | u8 | `counter` | (battle_setup kPCounter / kECounter; battle_damage kPShown2 / kEShown2) |
| `+0x0A` | u8 | `item_slot` | the member's (battle_setup kPItemSlot); unread on an enemy |
| `+0x0B` | u8 | `saved_animation` | the member's +0x4B saved by BattleObj_HitEnter (battle_e3.cpp:380); unread on an enemy |
| `+0x0C` | u32 | `flags` | (battle_damage kPFlags / kEFlags; battle_flow.cpp; battle_fx_tasks.cpp); PartyMember::flags_130, BattleEnemy::flags_110 |
| `+0x10` | u32 | `flags2` | bit 0x10 the gate, 0x40 / 0x80 the charges (battle_setup kPFlags / kEFlags; battle_damage kPFlags2 / kEFlags2; typed-records.md 3 item 2); PartyMember::flags_134, BattleEnemy::flags_114 |

Padding:

- zeroed as two dwords in battle (the member's +0x138 / +0x13C, battle_phases.cpp:222-223, battle_e6.cpp:217-218; the enemy's +0x118..+0x11F at set-up, battle_sprites.cpp); no reader names them

### `BattleWindow` (`0x24` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` | as WindowRecord |
| `+0x01` | u8 | `handler` | 3 |
| `+0x02` | u8 | `kind` | 0..7 |
| `+0x03` | u8 | `state` |  |
| `+0x04` | u16 | `x` | handed to the draws as they are, not 12.4 |
| `+0x06` | u16 | `y` |  |
| `+0x08` | u8 | `hp_gauge` | the HP gauge's kind (Window_DispatchKind) |
| `+0x09` | u8 | `ap_gauge` | the AP gauge's kind |
| `+0x0A` | u8 | `slot` | party member 0..2, enemy 3..8, or (kind 3) a banner-pool record |
| `+0x0B` | u8 | `hp_length` | the HP gauge's length, 0..55 |
| `+0x0C` | u8 | `ap_length` | the AP gauge's length |
| `+0x0D` | u8 | `out_wait` | cleared on set-up; the out test waits on it being 0 |
| `+0x0F` | u8 | `pass` | Window_FlagAndAdvance sets 1, the target states clear it |
| `+0x10` | u16 | `home_x` | the home place Window_RestoreAndBack puts back |
| `+0x12` | u16 | `home_y` |  |
| `+0x14` | u16 | `hp_shown` | HP as shown |
| `+0x16` | u16 | `ap_shown` | AP as shown |
| `+0x1C` | u16 | `hp_top_shown` | HP top as shown |
| `+0x1E` | u16 | `ap_top_shown` | AP top as shown |

Padding:

- cleared on set-up, unread
- handed to the gauge step (group CM's handlers); not read by the battle window states
- unread

### `ItemListWindow` (`0x24` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` | as WindowRecord |
| `+0x01` | u8 | `handler` |  |
| `+0x02` | u8 | `kind` |  |
| `+0x03` | u8 | `state` |  |
| `+0x04` | u16 | `x` | whole pixels (menu-windows.md 2 "x, y (s16)") |
| `+0x06` | u16 | `y` |  |
| `+0x08` | u8 | `use_mode` | the Item_CanUse mode |
| `+0x09` | u8 | `greyed` |  |
| `+0x0A` | u8 | `category` | 0..4 |
| `+0x0B` | u8 | `top` | the top index |
| `+0x0C` | u8 | `marked` | a marked index |
| `+0x0D` | u8 | `cursor` |  |
| `+0x0F` | u8 | `pass` | as WindowRecord |
| `+0x10` | u16 | `arrows` | arrow bits 1, 0; a countdown in the high nibble |
| `+0x12` | u16 | `scroll` | the scroll state (Menu_ListScroll) |
| `+0x20` | u32 | `shop_list` | the buy list's: the shop list (a count, then (category, id) pairs) |

Padding:

- unread by the list
- unread by the list

### `EffectWork` (`0x80` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x0C` | u32 | `work_0c` | a dword of the kind's own (UL / SetUL in 268 of the 358 union rows at +0xC / +0x10, the rest Long / SetLong; kind 3's level, effect_1a.md 2.5; kind 0x10's extra record, 2.2) |
| `+0x10` | u32 | `work_10` | likewise (kind 3's acceleration, kind 5's target height, effect_1a.md 2.5, 2.6) |
| `+0x2E` | u16 | `work_2e` | a word of the kind's own, read as u16 (Word / W) in 367 of the 402 union rows at +0x2E / +0x30, as s16 (SW) in 35 (a count: effect_2d.cpp kind 0x45; effect_3a.md 1.5 kind 0x68's count) |
| `+0x30` | u16 | `work_30` | likewise (kind 0x68's frame, effect_3a.md 1.5; kind 0x45's, effect_2d.cpp) |
| `+0x32` | u16 | `work_32` | likewise, where the kind does not use +0x32 as a draw key (the C2 audit, ea F1 and doubt 1, frs F4: kind 0x2B's count, 0x43's countdown, 0x45's amplitude, 0x51's and 0x98's radius, 0xAE's ellipse word and angle, 0x78's saved turn of object 2; kind 18's subs 0x0F and 0x2B a height, 0x17 a counter and a height factor, 0x24 an angle, 0x25, 0x26 and 0x39 a lid index). Kind 18's sub 0x44 draws by it as a key (Sub44_Follow stores 0x3200) and keeps draw_key |

Padding:

- EffectObject's
- EffectObject's
- EffectObject's

### `CharStatBlock` (`0x20` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u16 | `max_hp` | (record +0x20 / +0x40; item-use.md 2) |
| `+0x02` | u16 | `max_ap` |  |
| `+0x04` | u16 | `atk` | (char-stats.md 2: Char_ApplyWeapon adds the weapon's +0x16) |
| `+0x06` | u16 | `def` |  |
| `+0x08` | u16 | `agi` |  |
| `+0x0A` | u16 | `intel` |  |
| `+0x0E` | u8 | `byte_0e` | Stat_AddCap99's byte (char-stats.md 2; ItemUse_Stat2EUp, item-use.md 2); PartyMember::byte_ae is the working copy's |
| `+0x0F` | u8[9] | `resist` | nine resistance bytes, Stat_AddResist's (char-stats.md 2); PartyMember::elements / holy are the first six of the working copy's |
| `+0x18` | u8[5] | `percent` | five percentage bytes, Stat_AddCap100's (char-stats.md 2) |

Padding:

- "a word" (char-stats.md 2; armour 0x28 adds 1); meaning not named
- copied with the block (Char_RecalcStats copies 32 bytes), unread

### `CharRecord` (`0xA4` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | char[9] | `name` | the name, nine bytes on the PC (save-interchange.md 2; 0x45F5A0 writes 8 of them) |
| `+0x09` | u8 | `char_id` | (save-interchange.md 2; PartyMember +0x89, battle_setup_callees.h kPCharId) |
| `+0x0A` | u8 | `level` | (save-interchange.md 2; PartyMember +0x8A) |
| `+0x0B` | u8 | `flags` | bit 0 joined: counted by Inventory_Count (char-stats.md 2) and by the name entry 0x45F020 (save-interchange.md 2) |
| `+0x0C` | s32 | `exp` | the experience: Char_LevelUp, Char_LevelUpGain, BattleResult_AddExp (cap 9999999), Menu_DrawMemberStatus read it as a dword |
| `+0x10` | u16 | `status` | the status word (item-use.md 2; PartyMember +0x90) |
| `+0x12` | u8 | `weapon` | (char-stats.md 2; PartyMember +0x92) |
| `+0x13` | u8[3] | `armour` | (char-stats.md 2: Char_ApplyArmour copies the three) |
| `+0x16` | u8[2] | `accessory` | (char-stats.md 2; PartyMember +0x96) |
| `+0x18` | u16 | `hp` | (item-use.md 2; char-stats.md 2) |
| `+0x1A` | u16 | `ap` | (item-use.md 2) |
| `+0x1C` | u8 | `survive` | the working copy's +0x9C (battle_setup kPByte9C; battle_damage kPSurvive) |
| `+0x1D` | u8 | `halving` | the halving bits, a Stat_AddResist pass per bit (char-stats.md 2 Char_RecalcStats step 4) |
| `+0x1E` | u8 | `hp_scale` | the max-HP scale (char-stats.md 2; item-use.md 2 ItemUse_HpScaleUp, to 9) |
| `+0x1F` | u8 | `trait` | the trait byte, 0xFF none (char-stats.md 2 Char_ApplyTraits) |
| `+0x20` | CharStatBlock | `stats` | the effective block (char-stats.md 2) |
| `+0x40` | CharStatBlock | `base` | its base copy, copied over +0x20 first by Char_RecalcStats (char-stats.md 2) |

Padding:

- not read by the records' readers here; save-interchange.md 2's late three-byte field is at +0x74 (carried verbatim, not understood)

### `EncounterSlot` (`0xC` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `active` | used / placed (Encounter_FillSlots; Battle_InitEnemies; field_hidden.cpp's on-screen test clears it) |
| `+0x01` | u8 | `kind` | the enemy data id (Battle_SetupEnemy's second argument; battle_sprites.md 3), 0xFF none (Encounter_FillSlots) |
| `+0x04` | s32 | `x` | the place, Battle_SetupEnemy's x (Long, battle_sprites.cpp:759; field_hidden.cpp:530) |
| `+0x08` | s32 | `z` | its z |

### `CommunityRecord` (`0x8` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` | non-zero in use (every reader; CommuName_CountSlots, rest_4d.md) |
| `+0x01` | u8 | `slot` | the building it is placed in, 1..8 (rest_4a.cpp Rec(i)[1] == building + 1; field_e2_callees.h "the record it names (1-based)"; rest_4b.md "the slot"), or a loose kind 0, 9, 0xA, 0xB (rest_4a.md) |
| `+0x04` | u32 | `stamp` | a time from the clock 0x904134 (rest_4a.md; rest_4a.cpp:1037 age = Rest4a_Clock - +4; rest_4b.md); "a dword" (rest_4e.md) |

### `CommunityBuilding` (`0x8` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `kind` | the kind, 0 empty, else 4 + a pick (rest_4a.md; rest_4b.md; field_e2.cpp:135,149 compares it with 4 and with a kind) |

### `SharedScratch` (`0x1C` bytes)

One anonymous union of three views over the same 0x1C bytes, named by
offset: `w_00` .. `w_1a` (u16, the `SW` / `SetSW` words), `d_00` .. `d_18`
(s32, the `SD` / `SetSD` dwords) and `b_00` .. `b_1b` (u8, the `SB` bytes).
No offset has one meaning: section 7.

### `GpuPrimHead` (`0x8` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | the ordering table's link (psx-library-layer.md 1) |
| `+0x04` | u8 | `r` | (Prim_SetShade; D3d_PrimColor) |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` | the GPU code, the walk's dispatch: (code & 0xFC) - 0x20 (d3d-draw.md 2) |

### `GpuPolyFT4` (`0x48` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | GpuPrimHead |
| `+0x04` | u8 | `r` |  |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` |  |
| `+0x08` | f32 | `x0` |  |
| `+0x0C` | f32 | `y0` |  |
| `+0x10` | f32 | `z0` |  |
| `+0x14` | u8 | `u0` |  |
| `+0x15` | u8 | `v0` |  |
| `+0x16` | u16 | `clut` |  |
| `+0x18` | f32 | `x1` |  |
| `+0x1C` | f32 | `y1` |  |
| `+0x20` | f32 | `z1` |  |
| `+0x24` | u8 | `u1` |  |
| `+0x25` | u8 | `v1` |  |
| `+0x26` | u16 | `tpage` |  |
| `+0x28` | f32 | `x2` |  |
| `+0x2C` | f32 | `y2` |  |
| `+0x30` | f32 | `z2` |  |
| `+0x34` | u8 | `u2` |  |
| `+0x35` | u8 | `v2` |  |
| `+0x38` | f32 | `x3` |  |
| `+0x3C` | f32 | `y3` |  |
| `+0x40` | f32 | `z3` |  |
| `+0x44` | u8 | `u3` |  |
| `+0x45` | u8 | `v3` |  |

Padding:

- unread by D3d_DrawPolyFT4
- unread

### `GpuPolyGT4` (`0x54` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | GpuPrimHead |
| `+0x04` | u8 | `r` |  |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` |  |
| `+0x08` | f32 | `x0` |  |
| `+0x0C` | f32 | `y0` |  |
| `+0x10` | f32 | `z0` |  |
| `+0x14` | u8 | `u0` |  |
| `+0x15` | u8 | `v0` |  |
| `+0x16` | u16 | `clut` |  |
| `+0x18` | u8 | `r1` |  |
| `+0x19` | u8 | `g1` |  |
| `+0x1A` | u8 | `b1` |  |
| `+0x1C` | f32 | `x1` |  |
| `+0x20` | f32 | `y1` |  |
| `+0x24` | f32 | `z1` |  |
| `+0x28` | u8 | `u1` |  |
| `+0x29` | u8 | `v1` |  |
| `+0x2A` | u16 | `tpage` |  |
| `+0x2C` | u8 | `r2` |  |
| `+0x2D` | u8 | `g2` |  |
| `+0x2E` | u8 | `b2` |  |
| `+0x30` | f32 | `x2` |  |
| `+0x34` | f32 | `y2` |  |
| `+0x38` | f32 | `z2` |  |
| `+0x3C` | u8 | `u2` |  |
| `+0x3D` | u8 | `v2` |  |
| `+0x40` | u8 | `r3` |  |
| `+0x41` | u8 | `g3` |  |
| `+0x42` | u8 | `b3` |  |
| `+0x44` | f32 | `x3` |  |
| `+0x48` | f32 | `y3` |  |
| `+0x4C` | f32 | `z3` |  |
| `+0x50` | u8 | `u3` |  |
| `+0x51` | u8 | `v3` |  |

Padding:

- +0x1B
- +0x2F
- +0x3E, +0x3F
- +0x43
- +0x52, +0x53

### `GpuLineF2` (`0x20` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | GpuPrimHead |
| `+0x04` | u8 | `r` |  |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` |  |
| `+0x08` | f32 | `x0` |  |
| `+0x0C` | f32 | `y0` |  |
| `+0x10` | f32 | `z0` |  |
| `+0x14` | f32 | `x1` |  |
| `+0x18` | f32 | `y1` |  |
| `+0x1C` | f32 | `z1` |  |

### `GpuLineF4` (`0x38` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | GpuPrimHead |
| `+0x04` | u8 | `r` |  |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` |  |
| `+0x08` | f32 | `x0` |  |
| `+0x0C` | f32 | `y0` |  |
| `+0x10` | f32 | `z0` |  |
| `+0x14` | f32 | `x1` |  |
| `+0x18` | f32 | `y1` |  |
| `+0x1C` | f32 | `z1` |  |
| `+0x20` | f32 | `x2` |  |
| `+0x24` | f32 | `y2` |  |
| `+0x28` | f32 | `z2` |  |
| `+0x2C` | f32 | `x3` |  |
| `+0x30` | f32 | `y3` |  |
| `+0x34` | f32 | `z3` |  |

### `GpuTile` (`0x1C` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | GpuPrimHead |
| `+0x04` | u8 | `r` |  |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` |  |
| `+0x08` | f32 | `x` |  |
| `+0x0C` | f32 | `y` |  |
| `+0x10` | f32 | `z` |  |
| `+0x14` | f32 | `w` |  |
| `+0x18` | f32 | `h` |  |

### `GpuSprt` (`0x1C` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | GpuPrimHead |
| `+0x04` | u8 | `r` |  |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` |  |
| `+0x08` | f32 | `x` |  |
| `+0x0C` | f32 | `y` |  |
| `+0x10` | f32 | `z` |  |
| `+0x14` | u8 | `u` |  |
| `+0x15` | u8 | `v` |  |
| `+0x16` | u16 | `clut` |  |
| `+0x18` | u16 | `w` | SPRT only |
| `+0x1A` | u16 | `h` | SPRT only |

### `GpuGlyph` (`0x28` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | GpuPrimHead |
| `+0x04` | u8 | `r` |  |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` |  |
| `+0x08` | s16 | `x0` |  |
| `+0x0A` | s16 | `y0` |  |
| `+0x0C` | u8 | `u0` |  |
| `+0x0D` | u8 | `v0` |  |
| `+0x0E` | u16 | `clut` |  |
| `+0x10` | s16 | `x1` |  |
| `+0x12` | s16 | `y1` |  |
| `+0x14` | u8 | `u1` |  |
| `+0x15` | u8 | `v1` |  |
| `+0x16` | u16 | `glyph` |  |
| `+0x18` | s16 | `x2` |  |
| `+0x1A` | s16 | `y2` |  |
| `+0x1C` | u8 | `u2` |  |
| `+0x1D` | u8 | `v2` |  |
| `+0x20` | s16 | `x3` |  |
| `+0x22` | s16 | `y3` |  |
| `+0x24` | u8 | `u3` |  |
| `+0x25` | u8 | `v3` |  |

Padding:

- unread by D3d_DrawGlyph
- +0x26, +0x27

### `GpuCellSprite` (`0x20` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` | GpuPrimHead |
| `+0x04` | u8 | `r` |  |
| `+0x05` | u8 | `g` |  |
| `+0x06` | u8 | `b` |  |
| `+0x07` | u8 | `code` |  |
| `+0x08` | f32 | `x` |  |
| `+0x0C` | f32 | `y` |  |
| `+0x10` | f32 | `scale_x` |  |
| `+0x14` | f32 | `scale_y` |  |
| `+0x18` | u16 | `first` |  |
| `+0x1A` | u16 | `count` |  |
| `+0x1C` | u16 | `clut` |  |
| `+0x1E` | u16 | `flags` | the colour's and the blend's tpage; bit 0x400 flips x |

### `GpuDrawMode` (`0xC` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u32 | `link` |  |
| `+0x04` | u16 | `tpage` | Gfx_DrawTpage takes the word (d3d-draw.md 2) |
| `+0x06` | u8 | `mode_flags` | bit 0 dfe (Gfx_DrawEnable), bit 1 dtd |
| `+0x07` | u8 | `code` | 0xE8 |
| `+0x08` | u32 | `window` | the texture window; Gfx_TexCacheKey from it when non-null (d3d-draw.md 2) |

### `SVector` (`0x8` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | s16 | `x` |  |
| `+0x02` | s16 | `y` |  |
| `+0x04` | s16 | `z` |  |
| `+0x06` | s16 | `pad` | never written by the vertex builders (no store at +6 + 8k in the B1 rows) |

### `EffectSpeck` (`0x14` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` | 1 when spawned; 0 free (EffectSpecks_Spawn takes the first 0) |
| `+0x01` | u8 | `count` | 0x40 at the spawn (effect_2a.md 1.3) |
| `+0x02` | u16 | `fall` | the fall speed word, (Rand & 0xFFF) + 0x1000; the height goes down by it << 8 (EffectSpecks_Move) |
| `+0x04` | s32 | `x` | the point (effect_2a.md 1.3: cos * d sar 4 + the record's +0x34) |
| `+0x08` | s32 | `z` | (+ the record's +0x38) |
| `+0x0C` | s32 | `height` | (the record's +0x30 << 16 + +0x3C); freed once below Sprite_Current's +0x3C, signed |

Padding:

- unread

### `EffectSpark` (`0x28` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | s32 | `x` | the point (EffectSparks_Draw links at x, z: MapView_LinkPrimAt(UL(spark), UL(spark + 4))) |
| `+0x04` | s32 | `z` |  |
| `+0x08` | s32 | `height` | the ground holds it (EffectSparks_Move) |
| `+0x10` | s32 | `vx` | the velocity, (x, z, height), normalised in place by Gte_VectorNormal (effect_2a.md 1.4) |
| `+0x14` | s32 | `vz` |  |
| `+0x18` | s32 | `vheight` | zeroed when the ground stops it |
| `+0x20` | u32 | `size` | up 4 a frame; the quad's half size from it >> 6 (UL, effect_2a.cpp EffectSparks_Draw) |
| `+0x24` | u8 | `shade` | down one a frame; the quad's r, g, b |
| `+0x25` | u8 | `frames` | frames left, 0 free (EffectSparks_Clear / _FindFree) |

Padding:

- a fourth dword copied with the point (effect_2a.md 1.4); not named
- unread
- unread

### `EffectDrop` (`0x18` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | f32 | `x` | the screen point, floats (effect_2a.md 1.5) |
| `+0x04` | f32 | `y` | down by the speed each frame |
| `+0x08` | f32 | `z` | the drop's depth, copied to the quad's four z (effect_2a.cpp EffectDrops_Draw) |
| `+0x0C` | s16 | `half_length` | up one a frame; read signed (SW) |
| `+0x0E` | s16 | `speed` |  |
| `+0x10` | s16 | `top` | the top bound (SW) |
| `+0x12` | s16 | `bottom` | the bottom bound (SW) |
| `+0x14` | u16 | `frames` | frames left, 0 free; a word (EffectDrops_Clear) |

Padding:

- unread

### `EffectGlowSpark` (`0x18` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` | (EffectKind64_ClearSparks clears it; StepSparks skips a 0) |
| `+0x02` | u8 | `life` | down one a step, out of use at 0 (StepSparks); 0x10 at a spawn (EffectKindA0_SpawnSpark) |
| `+0x04` | s32 | `x` | the point, projected (EffectKind64_DrawSpark: Point(spark + 4)) |
| `+0x08` | s32 | `z` |  |
| `+0x0C` | s32 | `height` |  |
| `+0x14` | u16 | `size` | down 0x10 a step (StepSparks); the disc's radius projected (DrawSpark) |
| `+0x16` | u8 | `centre` | the disc's centre shade (DrawSpark) |
| `+0x17` | u8 | `rim` | the rim's |

Padding:

- unread

### `EffectShard` (`0x2C` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | s32 | `x` | the point: the record's x (InitShard) |
| `+0x04` | s32 | `z` | the record's z |
| `+0x08` | s32 | `height` | the record's height |
| `+0x10` | SVector | `edge0` | the first edge (cos, sin of 0x10, 0), turned in place by a random matrix (InitShard) |
| `+0x18` | SVector | `edge1` | the second (cos, sin of -0x10, 0) |
| `+0x20` | u16[3] | `angle` | zeroed by InitShard; +0x24 the spin, up 0x10 a frame (EffectKind64_DrawShards; effect_3a.md 1.4) |
| `+0x28` | u16 | `speed` | (Rand & 2) + 3 (InitShard); the edges' scale in the draw (effect_4e.md); E1C's debris a scale 10 + Rand % 4 |
| `+0x2A` | u16 | `shade` | 0x20 (InitShard; 0x40 for kind 0xA0's); E1C's debris shade up 0x20 / down 2 (effect_1c.md 1.3) |

Padding:

- unwritten by InitShard
- unwritten

### `EffectTrailPoint` (`0x34` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | s32 | `x` | the point |
| `+0x04` | s32 | `z` |  |
| `+0x08` | s32 | `height` |  |
| `+0x10` | s32 | `edge0_x` | the first edge point (x; z + cos << 3; height + sin << 11: TrailStep), projected by TrailDraw |
| `+0x14` | s32 | `edge0_z` |  |
| `+0x18` | s32 | `edge0_height` |  |
| `+0x20` | s32 | `edge1_x` | the second (x; z - cos << 3; height - sin << 11) |
| `+0x24` | s32 | `edge1_z` |  |
| `+0x28` | s32 | `edge1_height` |  |
| `+0x30` | u16 | `angle` | the angle word: Effect2e_TrailTurn for the head (TrailStep), 0 at the start (TrailInit) |

Padding:

- unread
- unread
- unread
- unread

### `EffectRay` (`0x38` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` |  |
| `+0x01` | u8 | `phase` | 0 the first end moves, 1 the second |
| `+0x02` | u8 | `count` | 8 a phase |
| `+0x03` | u8 | `shade0` | the first end's shade, down 10 a step in phase 0 |
| `+0x04` | u8 | `shade1` | the second's, in phase 1 |
| `+0x08` | s32 | `end0_x` | the first end, the record's point + (cos, sin) * length sar 12 |
| `+0x0C` | s32 | `end0_z` |  |
| `+0x10` | s32 | `end0_height` |  |
| `+0x18` | s32 | `end1_x` | the second end |
| `+0x1C` | s32 | `end1_z` |  |
| `+0x20` | s32 | `end1_height` |  |
| `+0x28` | s32 | `speed_x` | the speed, -(cos, sin) << 1, added to the moving end |
| `+0x2C` | s32 | `speed_z` |  |
| `+0x30` | s32 | `speed_height` |  |

Padding:

- unread
- unread
- unread

### `EffectMote` (`0x1C` bytes)

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` |  |
| `+0x01` | u8 | `state` | EffectKind68_MoteStates by it, unbounded |
| `+0x02` | u8 | `count` | 8 at the start; reloaded from +4 (MoteGlow) or 0x10 (MoteHold) |
| `+0x03` | u8 | `shade` | up 0x10 (MoteGlow), down 8 (MoteRise); the disc's centre |
| `+0x04` | u8 | `reload` | 0x20 (InitMote) |
| `+0x08` | u32 | `speed` | up 0x400 a step, added to +0x10 (MoteRise) |
| `+0x0C` | s32 | `x` | the point: the record's x moved ((Rand & 0xFF) - 0x80) << 10 (InitMote); the disc round it (DrawMote) |
| `+0x10` | s32 | `z` | the record's z; up by the speed (MoteRise) |
| `+0x14` | s32 | `height` | the record's height, up (Rand % 0x300) << 16 |

Padding:

- unread

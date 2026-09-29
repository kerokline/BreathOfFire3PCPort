# Group E1A: the effect kinds of `0x462B00..0x4672F0` - dispatchers, kinds 1, 3, 5, 7, 0xA, 0xC, 0xD, 0xF, 0x1A and the panel draws

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9 to 11),
wave one, from the round branch's `1bb41df`. **70 functions ours**
(`src/game/effect_1a.cpp`, shadow name `effect_1a`): the cut table's 64 rows
for E1A (`analysis/round13_cut.tsv`, the band `0x462B00..0x467270`) and six
kind dispatchers in the band that no list of the cut holds (section 1). Each
read to its last instruction with capstone and fuzzed through the scenario
harness in effect mode ([`scenario_harness.md`](scenario_harness.md) section
8) without edits to it: **210,000 rounds, 0 mismatches**. **133 controls
planted one at a time, 130 refused by a count, 3 equivalent** (each with a
near variant refused, section 7). Thirteen state tables named. **Fuzz only**:
no recorded route enters any of the 70 (section 9).

| Part | Functions | Reached through |
|---|--:|---|
| Dispatchers: kinds 1, 2, 3, 5, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xF, 0x1A by +1; kinds 0xE and 0x16 by the world map's record; kind 0x5C by the area | 16 | `Effect_KindHandlers` `0x655350` (`Effect_RunObjects` calls entry `+5`) |
| Kind 1 (a quad under an extra sprite) and kind 0x10 (a quad between two) | 3 | `EffectKind01_States`, `Effect_KindHandlers[0x10]` |
| Kind 7's first two states (a colour ramp) | 2 | `EffectKind07_States` |
| The panel draws `EffectHud_*` (cdecl helpers) | 9 | calls from kinds 2, 3, 0xC and from E1B's kinds |
| Kind 3 (a fading bar panel and a name with a count) | 3 | `EffectKind03_States` |
| Kind 5 (a sprite that rises, lands, bounces, follows an object) | 5 | `EffectKind05_States` |
| Kind 0xA (record 0's sprite copied: a shadow) | 2 | `EffectKind0A_States` |
| Kind 0xC (an aim bar) | 2 | `EffectKind0C_States` |
| Kind 0xD (a sprite that slides in and out) | 4 | `EffectKind0D_States` |
| Kind 0xF (a message window typing lines) | 12 | `EffectKind0F_States` |
| Kind 0x1A (the kind-count panel and its window) | 12 | `EffectKind1A_States` (its entries 13..16 are E1B's) |

Every name of a state is a hypothesis from what the code does (`symbols.toml`
status `hypothesis`; the dispatchers `evidence`). "Shadow", "window", "aim",
"slide" name the code's shape - which record it copies, what it draws, how
a word moves - not a play-tested fact: which scene spawns which kind was not
traced and is the owner's word, not this doc's (section 9).

## 1. The band, and what the cut did not list

`tools/band_rows.py --group E1A --byte-tables` prints the 64 rows, **0 "code
no list has"**, and every extent equal to the cut's or shorter by padding
only (45 rows; 11,216 bytes read against the cut's 11,645). No row is a
jump-table case or a shared tail: every row is reached by a `.data` cell or a
`call`; the rows' own tail jumps (`Effect_Release`, `Sprite_UpdateScreen`,
`Sprite_QueueOverlay`, `0x52CD50`, the two area handlers) all leave the band.

**Six functions in the band that no list of the cut holds**, taken as the
brief's "functions in your band are yours too", because they are the kinds'
own dispatchers - the entries of `Effect_KindHandlers` whose tables hold this
group's states (the round takes a kind whole): `0x463EE0` (kind 0xB),
`0x464F20` (3), `0x465310` (5), `0x465A60` (0xC), `0x466080` (0xF), `0x4667A0`
(0x1A). They are catalog part-2 rows (`Table Effect_KindHandlers`); the
labelling pass proposed "area overlays" or "scenario banks" for them from their
neighbours, so the join (section 10 of the round doc) left them out. The tool
does not flag them because a list (the catalog) has them. `0x463EE0`'s states
are scenario-bank rows, as are those of kinds 2, 8 and 9 whose dispatchers the
cut did give E1A; it is taken with them.

**The cut's `unit` column over-joins.** Its unit "Fn_466080 kind 0xF (24)"
is two kinds: `0x653C28` is kind 0xF's table (13 entries, `0x466080` indexes
it) and `0x653C5C` is kind 0x1A's (17 entries, `0x4667A0`), found by a raw scan
of `.text` for every cell address (section 3). Likewise "Fn_464F20 kind 0x3"
is kinds 3 (`0x653AFC`, 4) and 5 (`0x653B0C`, 6), and "Fn_4658C0 kind 0xA"
is kinds 0xA (`0x653B64`, 2), 0xC (`0x653B6C`, 3) and 0xD (`0x653B78`, 5).

**The `hypothesis` rows** (the addendum): `0x462B00`, `0x462B20`, `0x462B60`,
`0x462EB0`, `0x463040`, `0x464350`, `0x464660`, `0x465F10` - every one is
effect code (an `Effect_KindHandlers` entry, or kind 7's state 1). All taken.

**Not effect code, not taken:** none of the 64.

## 2. What each function does

`S` is `Sprite_Current` (an `Effect_Objects` record of 0x80), `+n` its byte or
dword at `n`. Full per-function readings are the `symbols.toml` evidence
strings; this is the shape.

### 2.1 The dispatchers

`EffectKind01_Run` `0x462BA0`, `_07_` `0x462E50`, `_08_` `0x463040`, `_09_`
`0x463440`, `_0B_` `0x463EE0`, `_02_` `0x464660`, `_03_` `0x464F20`, `_05_`
`0x465310`, `_0A_` `0x4658C0`, `_0C_` `0x465A60`, `_0D_` `0x465F10`, `_1A_`
`0x4667A0`: `mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp
[eax*4 + table]`, no bound. `EffectKind0F_Run` `0x466080` first releases the
record (`Effect_Release`, a tail jump) unless it is record 3 (`0x7E1360`) or
record 3's `+1` is above 3. `EffectKind0E_WorldMap` `0x462B00` and
`EffectKind16_WorldMap` `0x462B20` jump through the world map record's `+4` /
`+8` handler by `WorldMap_RecordIndex` (as ours `EffectKind00_WorldMap` does
through `+0`, [`worldmap_area.md`](worldmap_area.md) section 5).
`EffectKind5C_Run` `0x462B60`: area 0x68 to `Area104_Kind5CRun`, any other
to `Area121_Kind5CRun`.

### 2.2 Kind 1 and kind 0x10: a textured quad at an extra sprite

`EffectKind01_Start` `0x462BC0` copies `+0x54` from the `Sprite_ObjectsExtra`
record the dword `+0x18` names. `EffectKind01_Draw` `0x462BF0` makes that
extra record `Sprite_Current` while it builds: its point is copied into the
effect record, a RECT (0, 0xF0, 16 x 16) primitive is made by `0x5A7840` and
linked at the point, then a `POLY_FT4` whose top pair is the point plus
`MoveCmd_AttachOffset(+0x1C)` projected (`Gte_RotTransPers`, the answer to the
effect's `+0x60`) at x - 8.0 / x + 8.0 and y 0, the bottom pair the same x at
the projected y; `Sprite_Current` back; CLUT (0xB0, 0x1E3), tpage (0x2C0,
0x100), v on top `(Frame_Counter >> 2) & 0x15` (a flicker), below that plus
the bottom y through `_ftol`; shade 0x80 and `Gpu_SetShadeTex(1)`; linked
(0x48); a RECT (0, 0, 256 x 256) primitive linked after it (the draw area
put back). `EffectKind10_Run` `0x464350` (its entry a `jmp` over eleven
`nop`s) builds the same quad between two extra records, `+0xC` with
`AttachOffset(+0x10)` on top and `+0x18` with `AttachOffset(+0x1C)` below,
each projection's depth by `Gte_StoreDepthF`, v below = v above + |bottom y
- top y|.

### 2.3 Kind 7's start and fade

`EffectKind07_Start` `0x462E70`: `+0x5D..+0x5F` = 0, `+9` = 0xF0, `+1` = 1.
`EffectKind07_FadeIn` `0x462EB0`: `0x462F10(1)` (a sprite primitive,
scenario-bank code), the three bytes up by 2; at `+0x5D` 0x80 the counter byte
`0x903848` = 0x14, `+1` = 2, `+9` = 0xFF. Its states 2..4 (`0x462FC0` twice,
`0x462FF0`) are scenario-bank rows, not this round's.

### 2.4 The panel draws (`EffectHud_*`, cdecl)

| Function | Arguments | Draws |
|---|---|---|
| `EffectHud_Draw` `0x464BA0` | x, y | panel sprites 0xD, 0xE, 0xF (`0x52CFE0`); the count 0x3E - record 0's `+0x3A` (0 when negative or record 0 is idle) printed (`Boss26Fx_CountFormat`) and drawn in 12-dot text, colour 2 when 0x32 or more with the leader's `+2` at 4 on a blinking frame; `EffectHud_DrawGauge` |
| `EffectHud_DrawGauge` `0x464C70` | x, y | `EffectHud_Bar`; a marker lifted by record 0's elevation / 32 (never below y as an s16); a `POLY_F4` from the marker to y + 0x28; at record 0 `+1` 4 an 8-dot sprite by its height word |
| `EffectHud_Bar` `0x464DA0` | x, y, slot | a `POLY_G4` 16 x 32, (0x64, 0x64, 0x80) to (0, 0, 0x50) |
| `EffectHud_Marker` `0x464E40` | x, y, slot | a `SPRT_16`, u from the four bytes `0x653AF8` by `(Frame_Counter >> 3) & 3` |
| `EffectHud_Sprite8` `0x464EC0` | x, y, slot | a `SPRT_8` at u 0xB8, v 0x48 |
| `EffectHud_TwoBars` `0x465120` | x, y, w, slot | two `POLY_G4`, 4 high each, black to green and green to black |
| `EffectHud_DrawCount` `0x4652D0` | x, y, slot, n | panel sprites 0x17 and 0x17 + n |
| `EffectHud_DrawArrow` `0x465D90` | x, y, flip, slot | a `POLY_FT4` 16 x 8, u swapped by `flip` |
| `EffectHud_DrawMark` `0x465E50` | x, y, width, shade, slot, blink | panel sprite 0x1C widened to `width`, u + `shade`; blinking: a red half-transparent `TILE` `width` x 8 |

Coordinates are read as s16 (`movsx`); every primitive is committed with
`Gfx_CommitPrim(slot, size)`.

### 2.5 Kind 3: a fading bar and a name

`EffectKind03_Start` `0x464F40`: `+0x18` = byte 0 of the parameter block the
pointer `0x939A20` names. `EffectKind03_Fade` `0x464F80` steps the level
`+0xC` by `+0x18` in the block's mode byte 1 (0 bouncing, 1 ramp and reset, 2
accelerating by `+0x10`), draws panel sprite 0x10 and `EffectHud_TwoBars` at
the level * 0x60 / 255, and moves on when the leader's `+2` is 3.
`EffectKind03_ShowName` `0x465230`: an accessory's name (`NameTable_Accessories`
by one of two bytes, alternating every 64 frames) and a count 1..4 (record 0's
`+7` less the byte `+0xF` of the block `0x939A24` names).

### 2.6 Kind 5 and kind 0xA

`EffectKind05_Start` `0x465330` places the sprite at the leader, its target
height `+0x10` from the parameter byte 2 times record 2's `+0xC`, its row `+6`
(0..3) of two small tables (`0x653B24`, `0x653B44`), clamps `Field_Kind2Z`,
sets the animation bank from the second block, and spawns a kind-0xA record.
`_Rise` `0x465540` moves z by -0x4000 a frame toward `+0x10`, the height by
its speed (`+0x14`, `+0x20`), with a sound at arrival; `_Land` `0x4656A0`
waits for the animation; `_Bounce` `0x4656C0` bounces on the height with a
wait `+0xA` and a count `+7`, spawning a kind-0xA record at each bounce;
`_Follow` `0x465840` follows the `Sprite_Objects` record the byte `0x939A1C`
names. `_Bounce` and `_Follow` end with E1F's `0x52CD50`.

`EffectKind0A_Start` `0x4658E0` / `_Follow` `0x4659A0`: a copy of record 0's
sprite fields while record 0 is live and above the ground (its `+1` set, its
height word above 0), else `Effect_Release`. Record 0 is the kind-5 sprite
when kind 5 took the first free record; the code does not check which kind
record 0 is.

### 2.7 Kind 0xC and kind 0xD

`EffectKind0C_Start` `0x465A80`; `EffectKind0C_Aim` `0x465AB0`: a mark
(`EffectHud_DrawMark`) at the word `+0x30` chases the byte `+0xA` of the object
`0x939A1C` names (after 15 frames out of reach), clamped by the parameter byte
3; two `LINE_F2`s, `EffectHud_DrawArrow` at the object, a sound every 16th
frame while the arrow is outside the mark (the leader's `+2` at 4).

`EffectKind0D_Start` `0x465F30` (animation bank 0x2B, a sound from the six
words `0x653B8C` by `+6`), `_SlideIn` `0x465FE0` (the screen x `+0x2E` down
0x1E a frame to 0xA0), `_Hold` `0x466020` (30 frames), `_SlideOut` `0x466050`
(on to -0x46, then back to state 0), each queued as an overlay.

### 2.8 Kind 0xF: the message window

`EffectKind0F_Start` / `_Open` / `_Frame` / `_Close` move the window's y
`+0x30` (-0x19 to 0x12 by 8) and draw it through E1B's `0x469750`.
`_Choose` `0x466120` takes a row `+6` of `0x653C04` (four bytes: three text
indexes and a spawn flag): with the flag it spawns a second kind-0xF record at
state 8 (the line typer) and waits; else it names the title text. `_Title`,
`_TitleClose` draw the title (`0x653B98`'s thirteen records: a string, a line
byte, a pause byte). The typer: `_LineStart` `0x4662B0` picks the text by the
row's column `+0x4A`, `_LineType` `0x466310` types a character every six
frames (`0x469AD0` the typing cursor, two bytes for a code with bit 7),
`_LineNext` `0x466460` shows the text's label (`0x653C00` pens, `0x66A2FC`
strings) and spawns the next line's typer, `_LineScroll` `0x4665E0` scrolls
the text out to the left a character at a time, `_LineFade` `0x466750` fades
the label and releases.

### 2.9 Kind 0x1A: the kind-count panel

`EffectKind1A_Start` .. `_Reset` (`0x4667C0..0x467270`): a window (E1B's
`0x469750`), three option boxes (`0x468AC0`, the bit `1 << +6` when the
leader's `+3` is 3 or more), member rows (`0x469210`) and two item lists
(`0x468C50` / `0x468F00` by `+7` bit 0) sliding in and out by `+9` (0..4);
`_FindKind` `0x466B30` walks the 32 count bytes at `0x9040EC` to the first
set one; `_PanelIn` / `_Panel` / `_PanelNext` / `_PanelBack` / `_PanelOut`
draw FE1's `FieldPanel_*` (header, kind icon, row, total, message) sliding by
`+9`. States 13..16 of its table are E1B's.

## 3. The state tables

A raw scan of `.text` for every dword address `0x653A40..0x653D00`
(scratch `refscan.py`) finds each table's dispatcher and the next table start;
a table's count is its own length to that start (the stage-A addendum: the
tables overlap the runs of code pointers).

| Table | Count | Dispatcher | Next start |
|---|--:|---|---|
| `EffectKind01_States` `0x653A44` | 2 | `0x462BA0` | kind 7's |
| `EffectKind07_States` `0x653A4C` | 5 | `0x462E50` | kind 8's (`0x653A5C` is `WorldMap_RecordIndex`'s loop end, not a table) |
| `EffectKind08_States` `0x653A60` | 4 | `0x463040` | kind 9's |
| `EffectKind09_States` `0x653A70` | 10 | `0x463440` | kind 0xB's |
| `EffectKind0B_States` `0x653A98` | 5 | `0x463EE0` | data at `0x653AAC` |
| `EffectKind02_States` `0x653AC0` | 5 | `0x464660` | `0x653AD4` (`call [eax*4 + ..]` at `0x46478B`) |
| `EffectKind03_States` `0x653AFC` | 4 | `0x464F20` | kind 5's |
| `EffectKind05_States` `0x653B0C` | 6 | `0x465310` | data at `0x653B24` |
| `EffectKind0A_States` `0x653B64` | 2 | `0x4658C0` | kind 0xC's |
| `EffectKind0C_States` `0x653B6C` | 3 | `0x465A60` | kind 0xD's |
| `EffectKind0D_States` `0x653B78` | 5 | `0x465F10` | data at `0x653B8C` |
| `EffectKind0F_States` `0x653C28` | 13 | `0x466080` | kind 0x1A's |
| `EffectKind1A_States` `0x653C5C` | 17 | `0x4667A0` | `0x653CA0` (`0x4674F0`'s by `+2`, E1B's) |

E1B's states sit in `EffectKind1A_States` (entries 13..16): E1B may name the
table too - **the coordinator keeps one entry** (section 10).

## 4. The fuzz (`effect_1a_fuzz.cpp`)

70 clones from `band_rows.py --clones --harness scenario`, the six
dispatchers' by hand (`0x466080`'s tail `jmp` to `Effect_Release` its one site);
`0x464350` cloned from its body `0x464360` (the harness refuses an entry that
opens with a `jmp`, [`magic_harness.md`](magic_harness.md) section 5), its call
offsets 0x10 less. Shapes: the 61 handlers `kEffect` with their kind and
`state_span` the table's count; the nine `EffectHud_*` `kCall`. 3,000 rounds
each; `BOF3X_E1A_ONLY=<name>` runs a subset. **No function answers**: every
`ret_mask` is 0 - the handlers are `void`, and no caller in this group reads
`eax` after an `EffectHud_*` call (the callers outside it, E1B's and the
scenario-bank states, are theirs to read).

**Data tables swapped for recorders:** the thirteen state tables and the
world map records' `+4` / `+8` cells of the eleven records (index 11's are
kind 1's and kind 7's tables, already swapped; record 6's are 0 in the image
and a recorder here - ours aborts on a 0 there, section 6).

**Regions beyond effect mode's:** the parameter blocks the pointers `0x939A20`
/ `0x939A24` name and the characters kind 0xF's cursors walk, buffers of the
fuzz's own (random every round, then seeded).

**Seeds** (every one of the 20 records, because the disturbance moves
`Sprite_Current` among them and the code re-reads it after calls): the extra
indexes `+0x18` / `+0xC` below 4; `+6` below the table the function indexes
(4, 6, or 7 for kind 0xF - rows 7 and 8 name texts past the table, section 6);
`+0x4B` below 13 and, for the functions that index the label tables
unchecked, a text with a line; `+0x4A` a column naming a text for
`_LineStart`; the cursors `+0x50` / `+0x54` into the characters; the kind word
`+0x3E` near the 32 counts. Then per function the compares' boundaries: the
fade's level at 0 / 0xFF and its steps' signs, kind 5's z at the target and at
the table offset, the bounce's height at -1 / 0 / 1 after its adds, the aim's
distance at the margin and one either side, the slides' words one step from
their stops, kind 0xF's y one step from its ends, `+0x49` against the count,
record 0's and record 3's state, the leader's `+2` / `+3`, the count bytes
none / some / all / just one (the last half the time at 0x1F), the area 0x68.
The current record's `+1` is kept below its span after the seed (record 0 or
3 may be it). Args for the panel draws: small coordinates over random high
bytes, y near the s16 sign for `EffectHud_DrawGauge`.

**Stand-ins added or re-listed** (the group's listing stands over the
standard set's):

| Callee | How | Why |
|---|---|---|
| the group's own called directly | `kPhase` (`EffectKind0A_Follow`, `EffectKind0F_Close`), typed (`EffectHud_*`) | ours calls them by name |
| `EffectHud_DrawCount` x, n; `_DrawArrow` x, flip; `_DrawMark` x, width, shade, blink | masks 0xFFFF / 0xFF | the original pushes a register whose high bytes are a caller's leftover (a stack dword, an entry register): the callee reads the word / byte (`movsx`, `mov al`) |
| `Text_DrawAt` | masks x, y 0xFFFF, colour, count 0xFF; the text hashed to its NUL | `msgbox.cpp` stores x, y as shorts; `text_draw.cpp` reads the low bytes; the original's count is `al` over a callee's leftover `eax` |
| `FieldPanel_DrawHeader`, `_DrawKindIcon`, `_DrawKindRow`, `_DrawTotal`, `_DrawMessage` | x, y 0xFFFF; kind, count, row 0xFF; the message id 0xFFFF | `field_e1.cpp`: the coordinates go to `0x52CFE0` as s16, the rest are cast to bytes / a word; the original's x is `movzx ax` over `Sprite_Current`'s high half |
| `Gte_RotTransPers` | masks 0 / all / 0, the vertex's six bytes hashed, the screen point two small whole floats, the depth cue's dword noise | the vertex and the cue are the caller's stack; the vertex's fourth word is stale stack in the original and 0 in ours (DIV-0023's ruling, as `field_e2`) |
| `MapView_LinkPrimAt` | size and dy as bytes; the cursor moves by the size two times in three | `world_map.cpp` moves `Gfx_PacketNext` when the row is on the map; the quad kinds read the cursor again after it |
| `0x5171E0` | the string hashed; answers a count 0..19 (whole `eax`) | the standard row answers garbage, so `_LineType`'s `+0x49 < count` was never at its boundary |
| E1F's `0x52CD50`; E1B's `0x469AD0` (pen & 0xF, text 2 bytes, width byte, x word), `0x469210`, `0x468C50`, `0x468F00`, `0x468A40` | by address | not in any standard set |

**Where ours passes the original's leftover bytes on purpose:** E1B's
`0x469750`, `0x468AC0`, `0x469210`, `0x468C50`, `0x468F00` are not re-listed
(their readings are E1B's to make): kind 0xF and 0x1A pass the exact 32-bit
values - `Sprite_Current`'s high half or the previous callee's `eax` under a
byte or a word (the SH_AT pointers are typed to answer `eax` for that). In the
game, once E1B's are ours, the previous callee's `eax` is whatever its body
leaves; every use of those arguments that was read (0x469790 / 0x469960 `and
0xFFFF`, 0x468BB0 through `Menu_DrawBox` and `0x52CFE0`) takes the low 16
bits.

**Result** (this worktree's build): **210,000 rounds over 70 functions,
563,293 calls to the stand-ins, 0 mismatches**, 24,948 bytes of state in 47
regions. Every state-table entry and the world map cells reached (coverage in
the log: `phase 0x...` for each).

## 5. Calls across groups

**Out, raw until the owners merge** (`effect_1a_callees.h`):

| Callee | Owner | Callers |
|---|---|---|
| `0x52CF60`, `0x52CFE0` | E1F | `EffectHud_Draw`, `_Marker`, `_DrawCount`, `_DrawMark`, `EffectKind03_Fade`, `EffectKind0C_Aim` |
| `0x52CD50` | E1F | `EffectKind05_Bounce`, `_Follow` (a tail jump) |
| `0x469750`, `0x468AC0`, `0x469210`, `0x468C50`, `0x468F00`, `0x468A40` | E1B | kind 0xF's and 0x1A's states |
| `0x469AD0` | E1B | `EffectKind0F_LineType`, `_LineNext`, `_LineScroll`, `_LineFade` |
| `0x462F10` | nobody (scenario-bank row) | `EffectKind07_FadeIn` |
| `0x5171E0`, `0x5A7840` | nobody (unlabelled; library layer) | kind 0xF; kinds 1 and 0x10 |

**In, from outside the group:** `Effect_KindHandlers` (the engine's
`Effect_RunObjects`) for the 16 dispatchers; the world map records hold
nothing of ours (they hold the area groups'). E1B's `0x467B10` calls
`EffectHud_Bar`, `_Marker`, `_Sprite8`; `0x4678C0` `EffectHud_TwoBars`;
`0x467E10` `EffectHud_DrawCount`; `0x4680F0` and `0x468210` `EffectHud_DrawArrow`
and `_DrawMark`. Kind 2's scenario-bank states `0x464730`, `0x464780`,
`0x4647B0` call `EffectHud_Draw`. `scenario_harness_ekh.cpp` copies
`0x462BC0` (`EffectKind01_Start`) - hence the inject after
`ScenarioHarnessEkh_Inject`.

## 6. What ours aborts on, and the latent defects

Ours aborts with a message (the round-nine rule) where the original reads
past a table or a list: a dispatcher's `+1` past its table (all 13);
`WorldMap_RecordIndex` past 11 and a record's handler of 0; an extra index
(`+0x18`, `+0xC`) of 4 or more; `0x939A1C` of 30 or more; kind 5's `+6` past
the four rows of `0x653B44`; kind 0xD's `+6` past the six sound words; kind
0xF's text past thirteen, a row / column past `0x653C04`, a line byte of 3 or
more where the code does not test 0xFF; an `Effect_FindFree` answer past the
twenty. None of these is reached by a value the game writes as far as the
code shows, except as the defects below say. No divergence: nothing here
changes what the game does (DIVERGENCE.md unchanged; `cheats.cpp`,
`widescreen.cpp` patch nothing in the band - grepped 2026-09-29).

**Latent defects** (Capcom's, described, not fixed; not numbered - the
coordinator numbers them):

1. **Kinds 0xE and 0x16 in area 104, or off a world map.** Record 6 (area
   104) of `WorldMap_Records` holds 0 at `+4` and `+8`: an effect of kind 0xE
   or 0x16 there jumps to address 0. Off a world map, index 11 reads
   `EffectKind01_States[1]` (`EffectKind01_Draw`) and `EffectKind07_States[0]`
   (`EffectKind07_Start`) - D73's reading for kinds 0 / 0x58 / 0x18, extended.
   Which scenes spawn kinds 0xE / 0x16 is not read.
2. **Kind 0xF's rows 7 and 8 name texts 13 and 14**, past `0x653B98`'s
   thirteen records: a record of kind 0xF with `+6` 7 or 8 reads its "text"
   from the pen bytes and the row table as a pointer (`_Choose` / `_Title`,
   `_LineStart`). Which `+6` the spawner writes is not read; the rows exist, so
   either the spawner never writes 7 / 8 or the game reads garbage there.
3. **`_LineNext` and `_LineFade` index the label tables by a line byte they do
   not test for 0xFF**; they are safe only because `_LineType` sets the wait
   `+0xA` for a line other than 0xFF and `_LineScroll` releases the record for
   0xFF. A disturbed `+0x4B` between the two would read `0x653C00 + 0xFF`.
4. **Pushed registers with stale high halves** (section 4): coordinates for
   `0x469750`, `0x468AC0`, `0x469210`, `0x468C50`, `0x468F00`,
   `FieldPanel_*` and `Text_DrawAt` carry `Sprite_Current`'s high half or a
   previous callee's `eax` above the byte or word the original loaded; kind
   0xC's `flip` a stale stack word. Harmless where the callees were read (low
   16 bits); a reading of E1B's bodies should confirm for theirs.
5. **`EffectKind05_Bounce` at a height of exactly 0** clears `+0xB` (the jump
   at `0x4657A0` lands on the `jle` at `0x4657DA`, taken at 0), where a
   height above 0 sets it: the bounce is re-armed only by an exact landing.
   Possibly intended; recorded because it reads like a compiler fold.
6. **Kind 0xA copies record 0 whatever record 0 is**: it shadows record 0,
   which is kind 5's sprite only when kind 5 took the first free record.
7. **E1F's `0x52CD50`** divides by `+0x60 - 2 * s16 +0x3E` unchecked (E1F's to
   describe; kind 5's last two states call it).

## 7. Controls

133 planted one at a time by `scratch/e1a/controls.py` (plant, rebuild, run
the function alone, restore, rebuild; each anchored on a unique string of
`effect_1a.cpp`); the results in scratch `controls.tsv`. **130 refused by a
count, 3 not refused, each an equivalent mutant with a near variant refused.**
Every one of the 70 functions has at least one refused control.

### 7.1 The three equivalent mutants

- **K10A** (`EffectKind10_Run`, the top y copied to `+0x1C` through the FPU
  instead of as a dword): differs only for a signalling NaN, which the
  projection never writes. Near variant **K10A2** (`+0x1C` from the depth
  `+0x10`): refused in 2,987 rounds.
- **G5** (`EffectHud_DrawGauge`, `height >= 0` for `> 0`): at height 0 both
  sides call `EffectHud_Sprite8(.., 0xB4 - 0 / 32, ..)` = 0xB4. Near variant
  **G5b** (`> 0x20`): refused in 103 rounds.
- **DC2** (`EffectHud_DrawCount`, the answer's bits 8..15 dropped under the
  sprite id): `0x52CFE0` reads the id's byte. Near variant **DC1** (id + 0x18):
  refused in all 3,000.

### 7.2 The table

CONTROLS_TABLE

## 8. The rebinding

No code of ours named any of the 70 by a raw address (the tool's `--refs`:
22 references in 6 files, all comments or harness files). Rebound (comments,
the line only): `area_w2e.cpp` 21 and 839, `area_w3b.cpp` 20 and 1064 name
`EffectKind5C_Run`; `worldmap_area.cpp` 596 names `EffectKind01_States` and
`EffectKind01_Run`. **Left for the coordinator:** `field_e1.cpp` 127 (a
comment naming "callers `0x466BE0..0x4670C0` ... Capcom's" - now
`EffectKind1A_PanelIn..PanelOut`, ours; E1E may edit the same line);
`scenario_harness_ekh.cpp` 61..65, 158..162 (`0x462BC0`, the harness's
self-test: its clone is Capcom's on both sides by design) and
`scenario_harness.h` 399 / `scenario_harness.cpp` 1183 (the effect band's
start, a bound, not a function). The raw callees of section 5 turn into names
when E1F and E1B merge.

## 9. The live route

The catalog's reach columns (`analysis/remaining_catalog.tsv`: attract, shop,
world map, combat) are empty for all 106 starts in `0x462B00..0x4672F0`, and
neither first-call trace (`analysis/calltrace/reach_dragon`, `reach_whelp`)
names any of the 70. **Fuzz only.** No live run was made (the brief). Which
scene shows which kind is the owner's to say; a recorded walk through one
would let the coordinator's frame-hash A/B cover it.

## 10. For the coordinator

- **`EffectKind1A_States`** `0x653C5C` (17) holds E1B's states 13..16; if E1B
  names it too, keep one entry.
- **Six dispatchers taken beyond the cut** (section 1): if another round's plan
  holds them, they are ours now.
- **Harness gaps, met in the fuzz file** (for the fold after the round): the
  packet buffer's size (`g_packets`, 0x800) is not exported, so the fuzz's
  `Advance` names it; `Gte_RotTransPers`' override compares the vertex pointer
  (a stack address) and hashes eight bytes (the fourth word stale) - FE2 and
  this group re-list it with masks 0 / all / 0 and six bytes; the standard
  `0x5171E0` answers garbage where callers compare a small count;
  `MapView_LinkPrimAt` does not move the cursor; `FieldPanel_*` and
  `Text_DrawAt` compare whole words where the callees read 16 or 8 bits.
- **`analysis/calltrace/entries_logic.txt`** (main checkout): 65 lines
  appended (the five others already there at the same extent), under a
  comment; the hosts `0x462F10`, `0x463350`, `0x463D80`, `0x4641D0`, `0x464EC0` (0x252),
  `0x465120` (0x1A2), `0x4652D0` (0x6CD), `0x4659A0` (0x3EB), `0x465E50`
  (0x406) and `0x466260` (0x1DD2) are cut by them.
- **Counts depend on the build directory**: the figures here are this
  worktree's.

**Not verified here:** which scenes spawn these kinds and with which `+6` /
`+0x18` (the spawners are scenario-bank and script code, not read); E1B's
bodies' use of the leftover high bytes (section 4); the PSX twins are cited
from `analysis/pairs_propagated.json` and were not read on the PSX side (the
sibling names none of them).

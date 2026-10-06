# Group R4F: the Config screen's machine, and the states of effect kinds 2, 7, 8, 9 and 0xB

**Status:** MEASURED (2026-10-05) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave four, from the
round branch's tip `35ec19ef`. **52 functions ours** (`src/game/rest_4f.cpp`,
`rest_4f.h`, `rest_4f_callees.h`, shadow name `rest_4f`): the cut's 50 rows for
R4F (`analysis/round14_cut.tsv`, the band `0x460CB0..0x464B60`) and two starts
in their spans no list had - `0x4613B0` (the Config screen's controller step,
band_rows' "code no list has" inside the catalog's extent of `0x461070`) and
`0x464970` (a cell of kind 2's mode table and the shared tail of two of its
states, inside the catalog's extent of `0x4648F0`). None dropped: no start is a
jump-table case. Each read to its last instruction with capstone and fuzzed
through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) sections 7 and 8), used
unchanged: **208,000 rounds, 0 mismatches**; `BOF3X_SHADOW='*'` exit 0, narrow
and with `BOF3X_WIDE=1` (section 5). Controls: **133 planted, 133 refused** (section 8).
**Five `.data` tables named** (section 3). **Fuzz only**: no recorded route
enters any of the 52 (section 10).

**The band is not the community village.** The cut's columns proposed scenario
banks (SCENA), text and windows and effect-kind tables; read, the band holds two
things:

| Part | Functions | Reached through |
|---|--:|---|
| The Config screen (field menu state 7): its machine, opening, top bar, rows, controller panel, closing, and the panel draws | 14 | `ConfigMenu_Body` `0x590340` (R2F's) jumps to `ConfigScreen_Run`; `ConfigScreen_States` `0x6536C0`, `ConfigScreen_OpenStates` `0x6536D4` |
| `WorldMap_ExitRecords`, the area's exit list | 1 | E8 from `Field_ExitFromCell` `0x531AF0` (event_leader) |
| Kind 7's states 2..4 and its sprite | 3 | `EffectKind07_States` `0x653A4C` [2..4] (E1A's `EffectKind07_Run`); E8 from E1A's `EffectKind07_FadeIn` |
| Kind 8's states 0..2 and its line | 4 | `EffectKind08_States` `0x653A60` [0..2] |
| Kind 9's ten states, its blades and fans | 12 | `EffectKind09_States` `0x653A70` [0..9] |
| Kind 0xB's five states and its quad | 6 | `EffectKind0B_States` `0x653A98` [0..4] |
| Kind 2's states 0, 2..4 and the eight steps of its body | 12 | `EffectKind02_States` `0x653AC0` [0, 2..4]; `EffectKind02_Modes`, `_Mode1Steps`, `_Mode3Steps` |

The cut's PSX pairs (`analysis/pairs_propagated.json`, "by callers", a SCENA
overlay section): `EffectKind07_DrawSprite` `0x462F10` with `0x801F7134`,
`EffectKind0B_Follow` `0x464140` with `0x801F8AB8`, `EffectKind0B_DrawQuad`
`0x4641D0` with `0x801F8BCC`. The sibling (`names/*.toml`, `symbols.toml`)
names none of the three, so no name is transferred; the names here come from
what the code does. **What any of these effects or this screen looks like in
play is not stated here**: the descriptions are the code's shapes (which
record is copied, what is drawn where), not play-tested facts; the Config
screen's rows and their text are [`config-screen.md`](config-screen.md)'s.

## 1. The band, and what the cut did not list

`tools/band_rows.py --group R4F` (2026-10-05; `--byte-tables` through the
round's scratch wrapper `band14.py` - the tool stops at its fixpoint limit on
this cut, `--clones`, `--refs` and `--edges` ran as they are) prints the 50 rows
and one "code no list has": `0x4613B0`, in the span the catalog gave
`0x461070` (128 bytes there; the function is 0x340 with its jump table, and
`0x4613B0` follows it, entered by `ConfigScreen_States[3]`). Taken.

**`0x464970` is a function of its own** (the brief's "a draw or a tail that
several states jmp to is a function of its own when it has its own frame and
ret"): it is entered by address (`EffectKind02_Modes[2]`, the mode kind 2's
body calls through), it returns by `ret` on one path and by a tail `jmp
Sprite_QueueOverlay` on the others, and `0x4648F0` and `0x464AF0` reach it by a
tail `jmp`. The tool counted it inside `0x4648F0` (0x123 bytes); `0x4648F0`
ends at its `jmp` (`0x4648F0..0x464965`, then ten `nop`s). Taken as
`EffectKind02_CopyLeader`; the two states call it by name as their tail.

**The hosts.** 39 of the rows are hidden starts: nine in `0x460C40` (R4E's
`Fn_460C40` - its line in `entries_logic.txt` carries 0xAAB bytes over them),
the kind states in `0x462F10`, `0x463350`, `0x463D80`, `0x4641D0` - which are
this group's own draws, each ending where the states begin. Every hidden start
is entered by a `.data` cell (a state table) or a call; none is a case.

**Extents.** Every extent is the tool's read but two: `0x4648F0` (0x76, above)
and `0x464970` (0xA3). The cut's sizes are the catalog's and run on over the
padding (27 rows) or over the next start (`0x461070`, `0x4648F0`).

**A cell that is not a reference.** The tool lists a second `.data` cell for
`ConfigScreen_TopBar`, `0x63F298`: it lies inside a byte stream of data whose
bytes happen to read `0x00460E10`; no code names it.

## 2. What each function does

`S` is `Sprite_Current` (an `Effect_Objects` record of 0x80), `+n` its byte or
dword at `n`; the menu block is `0x929F00..` ([`menu-screens.md`](menu-screens.md)
section 1). The full per-function readings are the `symbols.toml` evidence
strings; this is the shape.

### 2.1 The Config screen

`ConfigMenu_Body` (R2F's, `ConfigMenu_Steps[1]`) tail-jumps to
**`ConfigScreen_Run`** `0x460CB0`: `jmp [ConfigScreen_States + 0x929F02 * 4]`,
unbounded. The screen's own state `0x929F02` is 0 the opening (by `0x929F03`
through `ConfigScreen_OpenStates`: `_OpenFade` - `Transition_Start(1)`;
`_OpenWait` - the screen at rest until `MoveScript_WaitWordDA` is 0; `_SlideIn` -
the screen slid in over the counter `0x929F04` from 5; `ConfigMenu_Open` sets
`0x929F03` to 2, so the menu's path slides in and the other two are the
title's), 1 the top bar, 2 the six rows, 3 the controller panel, 4 the closing
(`_SlideOut`: slid out, then `0x929F01` on, or at `Game_Step` 4 a fade back to
the title).

The cursor `0x929F05` is 0 / 1 on the top bar's two buttons, 2..7 on the rows,
8..13 on the controller panel's rows. **`ConfigScreen_TopBar`**: 0xA000 flips
between the buttons, 0x1000 / 0x4000 (`Input_AutoRepeat` of `Input_Pressed &
0xF000`) enter the rows at the last / first; a confirm button on button 0
leaves (the state + 3), on button 1 restores the defaults (the five settings,
six button words). **`ConfigScreen_Rows`**: by the cursor - 2, through a jump
table of six inside the function (bounded): rows 0..2 step their setting on
0x2000 / 0x8000 (row 2 tests 0x8000 first), rows 3 and 4 flip theirs on 0xA000,
row 5 opens the controller panel on a confirm button or 0xA000; up / down move
the cursor and back to the top bar past either end; a cancel button or 0x800
back to the top bar. **`ConfigScreen_Controller`**: a face button pressed
(`Input_Pressed`'s low byte & 0xFC, its lowest bit) is given to the cursor's row
and the row that held it takes the cursor row's old word (the rows' words by
the six bytes `0x6536F0`); bits 0 and 1 of `0x903580` and
`Field_ConfirmButtons` are kept set. Up / down wrap in 8..13; 0xA000 back to the
rows at 7, 0x800 to the top bar.

The draws: **`ConfigScreen_DrawButtons`** (sel, x, y) is
`Menu_DrawButtonRow(x, y, 6, sel, ..)`. **`Config_DrawPanel`** (x, y): DIV-0011's
frame, then six rows of `Config_DrawRowLabel` (FC1's) and
**`Config_DrawRowOptions`** (the row's options from the records `0x6536F8`, the
setting's large on the row under the cursor). **`Config_DrawControllerPanel`**
(x, y): its frame, then six rows of **`Config_DrawControllerRow`** (the row box,
the name right-aligned, two grey cells) and `Config_DrawControllerCell`
(DIV-0051's, config_text.cpp).

**`WorldMap_ExitRecords`** `0x462AC0` answers `WorldMap_Records`' `+0x14` of
the record `WorldMap_RecordIndex` names: the list `Field_ExitFromCell` walks.

### 2.2 Kind 7's states 2..4

**`EffectKind07_DrawSprite`** (semi): a draw mode (page 0xDD) and a SPRT at
(32, 32), 0x100 by 0x6E8, coloured `+0x5D..+0x5F`, semi-transparency the
argument. It is not a full-frame fill (not `(0, 0)` 320 x 240 from the float
constants; DIV-0041 untouched). **`_Hold`** (states 2 and 3) draws it and counts
`+9` down, then on with `+9` 0xFF; **`_FadeOut`** (state 4) fades the colour by 2
and releases the record at 0. (E1A's `_Start` and `_FadeIn` are states 0 and 1.)

### 2.3 Kind 8

**`EffectKind08_Start`**: the point (0xB0000, 0xF8000) and its ground height.
**`_TraceLine`** (state 1): a grey line along the ground in steps of 0x8000 of z
from the record's point to the z of the `Sprite_Objects` record `+6` names, each
step at the ground's height (`AreaMap_Elevation`), the last at the object's own;
on to state 2 when the chapter's byte `0x90384A` is 0x32. **`_TraceTwo`** (state
2): the same line, then a dark one from (0xB0000, 0x138000) down z to the
object's; on when `0x90384A` is 0x34. **`_DrawLine`** (two points and a colour, by
value): one `LINE_F2` projected through the GTE. State 3 is
`Effect_StateRelease`.

### 2.4 Kind 9

**`EffectKind09_Start`** places the record (0x638000, 0xD0000, height 0x300) and
clears its lengths. **`_Open1`..`_Open8`** (states 1..8) draw the first n fans
(**`_DrawFan`**: one `POLY_G3` blade of a length under the record's object
matrix with three angles of its own - the record's `+0x64..+0x6C` set and put
back around it), each state growing the n-th length to 0x60 with a sound at the
start and the end; `_Open8` adds one to the chapter's count `0x903848`.
**`_Spin`** (state 9) draws **`_DrawBlades`** (twenty-four blades turning by
0xAA from `(+9 << 12) / 360`, two alternating shapes) while `+9` counts down,
growing `+0xC` to 0x80 and `+0x10` to 0xB6, then releases the record.

### 2.5 Kind 0xB

**`EffectKind0B_Start`**: the screen point from the first extra sprite's words
`+0x2E` / `+0x30` plus two offsets (`0x653AAC`), the depth 0.01, the side 0x20,
the colour 0x80, on to `+7`. **`_Rise`**: left 4.0 a frame, up by a table by
`+9 >> 2` (`0x653AB0`), fading by 2 and growing every fourth frame;
**`_Drop`**: down 2.0, fading by 4; **`_Attach`**: the first extra sprite's point
and angles plus an attachment offset; **`_Follow`**: that point lifted 0x20 a
frame and projected, fading by 4. Each releases the record at colour 0 and
draws **`_DrawQuad`**: a textured semi-transparent square of side `+6` round the
point, committed only while `Draw_PassFlags` is set.

### 2.6 Kind 2

**`EffectKind02_Start`** copies four of the leader's (ObjTrio record 0,
`0x802D40`) bytes and puts a panel's y at 0xF0; **`_SlideIn`** moves it to 0xAE,
**`_SlideOut`** back to 0xF0 (then state 1, `BareRet`, waits); each draws
`EffectHud_Draw(0xC, y)` (E1A's). **`_Body`** (state 3) calls through
`EffectKind02_Modes` by `+2`: 0 `BareRet`, 1 **`_RunMode1`** and 3
**`_RunMode3`** (each by `+3` through a table of its own), 2
**`_CopyLeader`** - the leader's sprite fields (`+0x2A`, `+0x49..+0x4B`, `+0x50`,
`+0x54`, `+0x58`, `+0x5A`) copied and the record queued as an overlay, unless
the leader's `+2` is 1. The modes recolour the record: **`_Mode1Tint`** /
**`_Mode3Tint`** build palette 0x7B in the CLUT shadow from the leader's palette
with bit 15 set and put the record on it; **`_Mode1Brighten`** /
**`_Mode3Darken`** step the colour and return it to its own palette at the end;
**`_Mode3Wait`** waits. Which mode runs follows the word `0x905E62` against 0x38
and the leader's `+2`.

## 3. The state tables

A raw scan of `.text` for every dword address in `0x6536C0..0x653700` and
`0x653AD4..0x653AF8` (scratch `r4fdis.py`) finds each table's reader; a table's
count is its own run of code pointers to the next table a dispatcher indexes
(the readers bound none):

| Table | Count | Reader | Next |
|---|--:|---|---|
| `ConfigScreen_States` `0x6536C0` | 5 | `ConfigScreen_Run` by `0x929F02` | `ConfigScreen_OpenStates` at its sixth cell |
| `ConfigScreen_OpenStates` `0x6536D4` | 3 | `ConfigScreen_OpenRun` by `0x929F03` | data at `0x6536E0` |
| `EffectKind02_Modes` `0x653AD4` | 4 | `EffectKind02_Body` by `+2` (a `call`) | `EffectKind02_Mode1Steps` at its fifth cell |
| `EffectKind02_Mode1Steps` `0x653AE4` | 2 | `EffectKind02_RunMode1` by `+3` | `EffectKind02_Mode3Steps` at its third cell |
| `EffectKind02_Mode3Steps` `0x653AEC` | 3 | `EffectKind02_RunMode3` by `+3` | data at `0x653AF8` (`EffectHud_Marker`'s bytes) |

`EffectKind07..0B_States` and `EffectKind02_States` are E1A's (their readers are
its dispatchers). The counts match what the entries write: kind 2's steps set
`+3` to 0..2 and `+2` to 0..3, the Config steps keep `0x929F02` in 0..4.

## 4. The fuzz (`rest_4f_fuzz.cpp`)

52 clones from `band_rows.py --clones --harness scenario`, the call sites
checked against the capstone read; `0x4648F0` cloned to its tail `jmp`
(0x76) with that `jmp` listed as a call site to `0x464970`, `0x464970`
(0xA3) its own clone. **Four Config draws are copied by the fuzz file itself
and handed to the harness as a jump** (`rest_2h_fuzz.cpp`'s way):
`Config_DrawPanel` and `Config_DrawControllerPanel`, whose frame call DIV-0011
re-aims before this module's inject (always), and `Config_DrawRowOptions` and
`Config_DrawControllerRow`, whose text call DIV-0017 / DIV-0026 re-aim under a
Latin overlay - the harness's `CloneOriginal` refuses a re-aimed site; each site
is aimed at the recorder for where it reaches now, and the two divergences'
targets get rows of their own keyed by that address. Shapes: the Config
states `kMenu` (`0x929F02` below 5), its draws `kCall`; the kind states
`kEffect` with their kinds and state spans (kind 2's body and modes `+2` below
4); the effect draws `kCall`; `WorldMap_ExitRecords` answers a pointer
(`ret_mask` all). 4,000 rounds each; `BOF3X_R4F_ONLY=<name>` runs a subset,
`BOF3X_R4F_ROUNDS` the rounds.

**Data tables swapped for recorders:** the five of section 3.

**Regions beyond effect mode's:** the button words 0 and 1 (`0x903580`, 4
bytes; the rest are effect mode's `kMenuButtons`), palette 0x7B in the CLUT
shadow (`0x811440`, 0x40).

**Seeds:** the opening byte below 3; the counter 0..5; the cursor in the range
each state keeps it (the controller's 8..13 only: past them the original reads
the row byte from other `.data` and writes a button word there, as ours does in
place - not a value this state writes); `Input_Pressed` half the time one of the
bits the states test; the confirm and cancel words, `Game_Step` 4 / 5,
`MoveScript_WaitWordDA` 0, the five settings at their edges (0..3, 0x7F, 0x80,
0xFF). For the kinds: every record's `+6` below 30 (kind 8's object), `+9` and
`+0x5D` at their edges; kind 8's object z a few steps of 0x8000 from the
record's and from 0x138000 (the walks stay short) and `0x90384A` at 0x32 / 0x34;
kind 9's lengths one step from their sounds and ends, `+0xC` / `+0x10` about
0x80 / 0xB6; kind 0xB's floats small and finite most rounds, `+0x5D` 0 / 2 / 4,
`+9` at the fourth and eighth, `Draw_PassFlags`; kind 2's y about 0xAE / 0xF0,
`+3` below its table, the word `0x905E62` about 0x38, the leader's `+2` 1, the
colour one step from its compares, the leader's palette 0x7B or not. Args for
the draws: small coordinates over random high bytes, a controller row below 6
(ours aborts past, section 7), the options' row below 8, the state 3 half the
time.

**The group's disturbance** (`Disturb(h)`, its case `sh::DisturbCase(h, 7)`,
every value from `h`): the current record's `+9`; one of its bytes `+0x5D..+0x5F`,
`+6..+8`, `+0xA`, `+0xB`; one of its dwords `+0xC`, `+0x10`, `+0x18`, `+0x1C`,
`+0x20`, `+0x64`, `+0x68`, `+0x6C`; the Config counter, or the cursor in the
function's range; one of the five settings; the chapter's byte `0x90384A`;
`Input_Pressed`. Each case is shown live by a control that misses a re-read of
its cell after a call (section 8).

**Stand-ins added or re-listed** (the group's listing stands over the
standard set's):

| Callee | How | Why |
|---|---|---|
| the group's own called directly | typed, masks what each reads; `EffectKind09_DrawBlades`, `EffectKind0B_DrawQuad`, `EffectKind02_CopyLeader` `kPhase` | ours calls them by name |
| `Config_DrawPanel`, `_DrawControllerPanel`, `_DrawControllerRow`, `ConfigScreen_DrawButtons` | coordinates 0xFFFF; rows, settings, sel 0xFF | the original pushes registers whose high halves are a caller's leftover (`movzx cx, byte` / `shl` over a callee's `ecx`); every reader takes the low word (`Menu_DrawBox`, `Text_DrawAt`, the lines' `& 0xFFFF`) |
| `EffectKind08_DrawLine` | the heights 0xFFFF0000, the colour 0xFFFFFF | it reads each height's high word only (`movsx word [hi]`); the colour's fourth byte is a stale register's |
| `Menu_DrawButtonRow`, `Config_DrawRowLabel`, `Config_DrawControllerCell`, `EffectHud_Draw` | by what each reads (menu_windows.cpp, field_c1.cpp, config_text.cpp, effect_1a.cpp) | not in a standard set |
| `Gfx_CommitPrim` | the cursor advanced with 0x90 to spare | the quad writes 0x48 bytes past the cursor (rest_3f_fuzz.cpp's) |
| `Gte_RotTransPers` | masks 0 / all / 0, the vertex's six bytes hashed | DIV-0023: the vector's fourth word is the original's stale stack, ours 0 (effect_1a_fuzz.cpp's row) |
| `Text_DrawAt` | the text hashed to its NUL | the options' and names' strings |
| `Menu_DrawFrame` (DIV-0011), `ConfigText_DrawSelected` (DIV-0017 / DIV-0026) | keyed by where the sites reach | the re-aimed sites |

**Where ours passes a value cleaner than the original:** the stale high halves
above (masked), the colour's fourth byte in kind 8, `EffectKind08_TraceTwo`'s
first height word (section 7). The original also writes its own argument slots
as scratch (`Config_DrawControllerRow`'s floats, `Config_DrawControllerPanel`'s
row byte): the callers never read them.

**Result** (this worktree's build): **208,000 rounds over 52 functions, about
1,650,000 calls to the stand-ins, 0 mismatches**, 24,824 bytes of state in 47
regions. Every table entry reached (the `phase` lines of the log), every
listed callee called.

## 5. Self-tests

Headless, this worktree, 2026-10-05: `BOF3X_SHADOW=rest_4f` exit 0 (as above).
`BOF3X_SHADOW='*'` exit 0: 742 self-test lines, every one 0 mismatches,
`inject: 9733 ours, 0 left original` (9,681 at the base plus these 52).
With `BOF3X_WIDE=1`: `'*'` exit 0, the same 742 lines, 0 mismatches (no
widescreen operand lands in this band). Neither run died silently.
`tools/ledger_check.py`: 73 entries, 0 errors. Controls: section 8.

## 6. The divergences inside these functions

No byte patch of `cheats.cpp` or `widescreen.cpp` lands in the band (grepped
2026-10-05). **Four divergences rewrite operands and call sites inside the
Config draws**, and ours reads each one in place at every call, as FC1's
`Config_DrawRowLabel` reads its own:

| Site | What | Divergence |
|---|---|---|
| `0x461778` in `Config_DrawPanel`, `0x461A84` in `Config_DrawControllerPanel` | the call to the empty `0x4DF820`, re-aimed at `Menu_DrawFrame` (always) | DIV-0011 (menu_frame.cpp) |
| `0x461A62` | the controller frame's width immediate, 0xC -> 0xD | DIV-0026 / DIV-0051 |
| `0x4619E1`, `0x4619F9` in `Config_DrawRowOptions` | the large option's width (x 12 -> x 8) and its call (`ConfigText_DrawSelected`) | DIV-0017 |
| `0x461B07`, `0x461B36`, `0x461B41`, `0x461B43`, `0x461BAC` in `Config_DrawControllerRow` | the box width, the name's width and right edge, its call, and the jump over the second cell | DIV-0026, DIV-0051 |

Only under a Latin overlay (`Lang_Latin`, config_text.cpp) are DIV-0017 /
DIV-0026 / DIV-0051 applied; the self-test runs without one, so **the fuzz
compares the Chinese forms of those operands**, and the Latin forms are read
by the same code (each a two-way choice, anything else a Fatal) but not fuzzed
- the same gap R2H names for its Latin paths. DIV-0011's frame is always on and
is fuzzed. No `DIVERGENCE.md` entry is added or wanted: nothing here changes
what the game does.

## 7. What ours aborts on, and the latent defects

Ours aborts with a message (the round-nine rule) where the original reads
past a table: `ConfigScreen_Run` past 5, `ConfigScreen_OpenRun` past 3,
`EffectKind02_Body` past 4, `_RunMode1` past 2, `_RunMode3` past 3 (each jumps
through the dword after); `Config_DrawControllerRow`'s row past the six names
(the original reads the dword after `0x66A368` as a string); kind 8's `+6` past
the thirty `Sprite_Objects` (`EffectKind08_TraceLine`, `_TraceTwo`);
`WorldMap_ExitRecords` past index 11. None is reached by a value the game
writes as far as the code shows. One policy throughout: every unbounded index
of this group aborts before the read.

**Latent defects** (Capcom's, described, not fixed; not numbered):

1. **`EffectKind08_TraceTwo`'s first height word is stale stack.** The frame's
   height cell is not set before the first step; only its low word survives
   into the heights passed to `EffectKind08_DrawLine`, which reads the high
   word only - so nothing drawn depends on it. Ours passes 0 there.
2. **Kind 8's walks have no step limit.** Each step adds 0x8000 to z until it
   passes the object's; a far object is many lines (a signed overflow would
   walk up to 2^17 steps). The game places the record at a fixed point and the
   object is the scene's; not reached as far as read.
3. **`ConfigScreen_Controller` indexes the row bytes by the cursor**
   (`0x6536E8 + s8 cursor`) and writes a button word by the byte it finds: a
   cursor outside 8..13 would write a word elsewhere in `0x903580..0x90377E`.
   The state keeps the cursor in 8..13 itself.
4. **`WorldMap_ExitRecords` off a world map** (index 11) answers
   `EffectKind07_States[3]` - a code pointer read as exit records; area 104's
   record answers 0. `Field_ExitFromCell` walks the list with no end test
   (event_leader.cpp says so). Whether a 0xA0 cell exists there is the
   area's.
5. **Stale high halves** in pushed coordinates (section 4): harmless where
   every reader was read (the low word).

## 8. Controls

133 planted one at a time by scratch `r4f/controls.py` with `control_list.py` (plant, rebuild, run the function alone with `BOF3X_R4F_ONLY`, restore; a rebuild at the end; each anchored on a unique string of `rest_4f.cpp`); results in scratch `controls.tsv`. **133 refused by a count, none not refused, no equivalent mutant.** Every one of the 52 functions has at least one refused control.

**The disturbance's cases, each shown live** (a mutant that keeps a value over a call, while the record is the same where the cell is a record's, so that only the group's own case can refuse it): case 0 (`+9`) C58, C90; case 1 (the record's bytes) C56, C115; case 2 (its dwords) C91, C96; case 3 (the counter, the cursor) C06, C22, C38; case 4 (the settings) C21; case 5 (`0x90384A`) C75; case 6 (`Input_Pressed`) C13. **Thin** (the group's case runs after about one stand-in call in a few hundred): C21 3 of 60,000 rounds (0 at 4,000 - the setting is re-read after one sound only, and the case picks one of five cells), C22 and C91 1 of 4,000, C38, C56 and C58 2, C81 5, C13 10. A louder stand-in on `Sound_PlayEffect` would make C21 cheap; left as is (the review's debt 17 form).

**C21 made cheap (2026-10-05, round fourteen's end, debt 23).** `Sound_PlayEffect` is re-listed in the fuzz with a stand-in (`FxSound`) that half the time sets the five settings case 4 moves to 0..3 from the noise, so a setting kept over the sound is seen at the call. C21 in this worktree: **99 of 4,000 rounds** (14 when one setting a quarter of the time); the group's rounds stay 4,000, its shadow 0 mismatches.

| Id | Function | Plant | Verdict |
|---|---|---|---|
| C01 | `ConfigScreen_Run` | state + 1 mod 5 | refused, 4000 of 4000 rounds |
| C02 | `ConfigScreen_OpenRun` | opening + 1 mod 3 | refused, 4000 of 4000 rounds |
| C03 | `ConfigScreen_OpenFade` | 0x929F03 + 2 | refused, 4000 of 4000 rounds |
| C04 | `ConfigScreen_OpenWait` | wait word != 1 | refused, 2008 of 4000 rounds |
| C05 | `ConfigScreen_SlideIn` | sound at counter 4 | refused, 1159 of 4000 rounds |
| C06 | `ConfigScreen_SlideIn` | the counter kept from the entry (case 3's counter) | refused, 63 of 4000 rounds |
| C07 | `ConfigScreen_TopBar` | flip by == 1 | refused, 224 of 4000 rounds |
| C08 | `ConfigScreen_TopBar` | rows entered at 6 | refused, 2037 of 4000 rounds |
| C09 | `ConfigScreen_TopBar` | Game_Step != 5 | refused, 161 of 4000 rounds |
| C10 | `ConfigScreen_TopBar` | default word 3 is 9 | refused, 293 of 4000 rounds |
| C11 | `ConfigScreen_TopBar` | default row 0 is 2 | refused, 293 of 4000 rounds |
| C12 | `ConfigScreen_TopBar` | bit 0x400 at Game_Step 4 | refused, 26 of 4000 rounds |
| C13 | `ConfigScreen_TopBar` | Input_Pressed read before the calls (case 6) | refused, 10 of 4000 rounds |
| C14 | `ConfigScreen_Rows` | row 0 tops at 3 | refused, 69 of 4000 rounds |
| C15 | `ConfigScreen_Rows` | row 2 tests up first | refused, 48 of 4000 rounds |
| C16 | `ConfigScreen_Rows` | row 3 xor 3 | refused, 174 of 4000 rounds |
| C17 | `ConfigScreen_Rows` | row 5 with 0x2000 | refused, 34 of 4000 rounds |
| C18 | `ConfigScreen_Rows` | below 3 to the top bar | refused, 164 of 4000 rounds |
| C19 | `ConfigScreen_Rows` | counter up to 4 | refused, 100 of 4000 rounds |
| C20 | `ConfigScreen_Rows` | cancel with 0x400 | refused, 81 of 4000 rounds |
| C21 | `ConfigScreen_Rows` | the setting read before the sound (case 4) | refused, 3 of 60000 rounds; 99 of 4000 with `FxSound` (2026-10-05) |
| C22 | `ConfigScreen_Rows` | the cursor read before the buttons (case 3's cursor) | refused, 1 of 4000 rounds |
| C23 | `ConfigScreen_Controller` | cursor 8 names 0xC5 | refused, 1260 of 4000 rounds |
| C24 | `ConfigScreen_Controller` | panel y 0x63 | refused, 4000 of 4000 rounds |
| C25 | `ConfigScreen_Controller` | bits & 0xFE | refused, 1631 of 4000 rounds |
| C26 | `ConfigScreen_Controller` | no swap | refused, 2873 of 4000 rounds |
| C27 | `ConfigScreen_Controller` | bits | 1 | refused, 2777 of 4000 rounds |
| C28 | `ConfigScreen_Controller` | wrap below 9 | refused, 346 of 4000 rounds |
| C29 | `ConfigScreen_Controller` | state + 0xFF | refused, 116 of 4000 rounds |
| C30 | `ConfigScreen_Controller` | every bit kept | refused, 2961 of 4000 rounds |
| C31 | `ConfigScreen_SlideOut` | step on at 4 | refused, 849 of 4000 rounds |
| C32 | `ConfigScreen_SlideOut` | Game_Step 6 | refused, 965 of 4000 rounds |
| C33 | `ConfigScreen_SlideOut` | buttons y 0x27 | refused, 3035 of 4000 rounds |
| C34 | `ConfigScreen_DrawButtons` | set 5 | refused, 4000 of 4000 rounds |
| C35 | `Config_DrawPanel` | frame w 0x22 | refused, 4000 of 4000 rounds |
| C36 | `Config_DrawPanel` | rows 3 / 4 swapped | refused, 3713 of 4000 rounds |
| C37 | `Config_DrawPanel` | rows from y + 8 | refused, 4000 of 4000 rounds |
| C38 | `Config_DrawPanel` | the state kept over the label (case 3) | refused, 2 of 4000 rounds |
| C39 | `Config_DrawRowOptions` | large at state 2 | refused, 651 of 4000 rounds |
| C40 | `Config_DrawRowOptions` | colour 1 | refused, 1530 of 4000 rounds |
| C41 | `Config_DrawRowOptions` | large width x 11 | refused, 442 of 4000 rounds |
| C42 | `Config_DrawRowOptions` | small x + 0x75 | refused, 3031 of 4000 rounds |
| C43 | `Config_DrawRowOptions` | large y - 1 | refused, 442 of 4000 rounds |
| C44 | `Config_DrawRowOptions` | the count read once | refused, 3031 of 4000 rounds |
| C45 | `Config_DrawControllerPanel` | frame w + 1 | refused, 4000 of 4000 rounds |
| C46 | `Config_DrawControllerPanel` | words 2 / 6 swapped | refused, 3988 of 4000 rounds |
| C47 | `Config_DrawControllerPanel` | rows 17 apart | refused, 4000 of 4000 rounds |
| C48 | `Config_DrawControllerPanel` | cell x + 0x57 | refused, 4000 of 4000 rounds |
| C49 | `Config_DrawControllerRow` | box h 0x11 | refused, 4000 of 4000 rounds |
| C50 | `Config_DrawControllerRow` | width x 5 | refused, 4000 of 4000 rounds |
| C51 | `Config_DrawControllerRow` | line to y + 0xF | refused, 4000 of 4000 rounds |
| C52 | `Config_DrawControllerRow` | second line x + 0x52 | refused, 4000 of 4000 rounds |
| C53 | `WorldMap_ExitRecords` | the record's +0x18 | refused, 4000 of 4000 rounds |
| C54 | `EffectKind07_DrawSprite` | h 0x6E9 | refused, 4000 of 4000 rounds |
| C55 | `EffectKind07_DrawSprite` | semi & 0x7F | refused, 380 of 4000 rounds |
| C56 | `EffectKind07_DrawSprite` | +0x5D kept over SetSprt while the record is the same (case 1) | refused, 2 of 4000 rounds |
| C57 | `EffectKind07_Hold` | +9 0xFE | refused, 974 of 4000 rounds |
| C58 | `EffectKind07_Hold` | +9 kept over the draw while the record is the same (case 0) | refused, 2 of 4000 rounds |
| C59 | `EffectKind07_FadeOut` | +0x5D down 3 | refused, 4000 of 4000 rounds |
| C60 | `EffectKind07_FadeOut` | +9 1 | refused, 238 of 4000 rounds |
| C61 | `EffectKind08_Start` | z 0xF8001 | refused, 4000 of 4000 rounds |
| C62 | `EffectKind08_Start` | slot 6 | refused, 4000 of 4000 rounds |
| C63 | `EffectKind08_DrawLine` | x - 0x4001 | refused, 4000 of 4000 rounds |
| C64 | `EffectKind08_DrawLine` | A projected first | refused, 4000 of 4000 rounds |
| C65 | `EffectKind08_DrawLine` | colour byte 2 from byte 1 | refused, 1344 of 4000 rounds |
| C66 | `EffectKind08_DrawLine` | height / 2 without the sign | refused, 3029 of 4000 rounds |
| C67 | `EffectKind08_TraceLine` | colour 0xC0C0C1 | refused, 4000 of 4000 rounds |
| C68 | `EffectKind08_TraceLine` | 0x33 | refused, 1014 of 4000 rounds |
| C69 | `EffectKind08_TraceLine` | ground height << 15 | refused, 1443 of 4000 rounds |
| C70 | `EffectKind08_TraceLine` | steps of 0x7FFF | refused, 1785 of 4000 rounds |
| C71 | `EffectKind08_TraceTwo` | the end at >= | refused, 878 of 4000 rounds |
| C72 | `EffectKind08_TraceTwo` | start 0x138001 | refused, 4000 of 4000 rounds |
| C73 | `EffectKind08_TraceTwo` | dark line drawn forward | refused, 4000 of 4000 rounds |
| C74 | `EffectKind08_TraceTwo` | start height not the ground's | refused, 4000 of 4000 rounds |
| C75 | `EffectKind08_TraceTwo` | 0x90384A read before the lines (case 5) | refused, 127 of 4000 rounds |
| C76 | `EffectKind09_Start` | +9 0x77 | refused, 3967 of 4000 rounds |
| C77 | `EffectKind09_Start` | +0x6C left | refused, 3999 of 4000 rounds |
| C78 | `EffectKind09_Open1` | at 0x58 | refused, 1956 of 4000 rounds |
| C79 | `EffectKind09_Open2` | sound at 0x30 | refused, 1461 of 4000 rounds |
| C80 | `EffectKind09_Open3` | +1 = 5 | refused, 1240 of 4000 rounds |
| C81 | `EffectKind09_Open4` | a sound at 0x10 | refused, 5 of 4000 rounds |
| C82 | `EffectKind09_Open5` | sound at 0x18 | refused, 1594 of 4000 rounds |
| C83 | `EffectKind09_Open6` | step 7 | refused, 4000 of 4000 rounds |
| C84 | `EffectKind09_Open7` | +1 = 9 | refused, 1039 of 4000 rounds |
| C85 | `EffectKind09_Open8` | sound 0x20E at the end | refused, 690 of 4000 rounds |
| C86 | `EffectKind09_Open8` | fan 4's angle 0xE37 | refused, 4000 of 4000 rounds |
| C87 | `EffectKind09_Open7` | fan 7 by +6 | refused, 3992 of 4000 rounds |
| C88 | `EffectKind09_Spin` | +0xC to 0x80 inclusive | refused, 479 of 4000 rounds |
| C89 | `EffectKind09_Spin` | hold at 0xB5 | refused, 1291 of 4000 rounds |
| C90 | `EffectKind09_Spin` | +9 kept over the blades while the record is the same (case 0) | refused, 12 of 4000 rounds |
| C91 | `EffectKind09_Spin` | +0xC kept over the blades while the record is the same (case 2) | refused, 1 of 4000 rounds |
| C92 | `EffectKind09_DrawBlades` | +0x6C up 0xAB | refused, 4000 of 4000 rounds |
| C93 | `EffectKind09_DrawBlades` | angle >> 9 | refused, 2905 of 4000 rounds |
| C94 | `EffectKind09_DrawBlades` | tips -(+9) | refused, 4000 of 4000 rounds |
| C95 | `EffectKind09_DrawBlades` | odd and even swapped | refused, 3317 of 4000 rounds |
| C96 | `EffectKind09_DrawBlades` | +0x6C kept while the record is the same (case 2) | refused, 1188 of 4000 rounds |
| C97 | `EffectKind09_DrawFan` | middle colour 0x6F | refused, 4000 of 4000 rounds |
| C98 | `EffectKind09_DrawFan` | +0x70 not put back | refused, 1394 of 4000 rounds |
| C99 | `EffectKind09_DrawFan` | cos(0x50) | refused, 4000 of 4000 rounds |
| C100 | `EffectKind09_DrawFan` | put back into the first record | refused, 1394 of 4000 rounds |
| C101 | `EffectKind0B_Start` | +0x7C off by one | refused, 4000 of 4000 rounds |
| C102 | `EffectKind0B_Start` | +1 = +6 | refused, 3984 of 4000 rounds |
| C103 | `EffectKind0B_Start` | y by the x offset | refused, 4000 of 4000 rounds |
| C104 | `EffectKind0B_Rise` | x by 2.0 | refused, 3506 of 4000 rounds |
| C105 | `EffectKind0B_Rise` | +6 each other | refused, 230 of 4000 rounds |
| C106 | `EffectKind0B_Rise` | rise by +9 >> 3 | refused, 1812 of 4000 rounds |
| C107 | `EffectKind0B_Drop` | +6 each fourth | refused, 584 of 4000 rounds |
| C108 | `EffectKind0B_Attach` | +0x6C from +0x68 | refused, 3999 of 4000 rounds |
| C109 | `EffectKind0B_Attach` | height by out[1] | refused, 4000 of 4000 rounds |
| C110 | `EffectKind0B_Follow` | height up 0x21 | refused, 4000 of 4000 rounds |
| C111 | `EffectKind0B_Follow` | fade by 2 | refused, 3213 of 4000 rounds |
| C112 | `EffectKind0B_DrawQuad` | right edge - side | refused, 3332 of 4000 rounds |
| C113 | `EffectKind0B_DrawQuad` | commit 0x47 | refused, 2627 of 4000 rounds |
| C114 | `EffectKind0B_DrawQuad` | CLUT y 0x1E2 | refused, 4000 of 4000 rounds |
| C115 | `EffectKind0B_DrawQuad` | half read after the call (case 1's +6) | refused, 124 of 4000 rounds |
| C116 | `EffectKind02_Start` | +0x30 0xC9 | refused, 4000 of 4000 rounds |
| C117 | `EffectKind02_Start` | slot 3 | refused, 4000 of 4000 rounds |
| C118 | `EffectKind02_SlideIn` | held at <= 0xAE | refused, 468 of 4000 rounds |
| C119 | `EffectKind02_Body` | +2 xor 1 | refused, 4000 of 4000 rounds |
| C120 | `EffectKind02_SlideOut` | +1 = 2 | refused, 1232 of 4000 rounds |
| C121 | `EffectKind02_SlideOut` | held at >= 0xF0 | refused, 497 of 4000 rounds |
| C122 | `EffectKind02_RunMode1` | +3 xor 1 | refused, 4000 of 4000 rounds |
| C123 | `EffectKind02_RunMode3` | +3 + 1 mod 3 | refused, 4000 of 4000 rounds |
| C124 | `EffectKind02_Mode1Tint` | +3 = 1 | refused, 1699 of 4000 rounds |
| C125 | `EffectKind02_Mode1Tint` | bit 14 | refused, 4000 of 4000 rounds |
| C126 | `EffectKind02_Mode1Tint` | strip dirty 2 | refused, 4000 of 4000 rounds |
| C127 | `EffectKind02_Mode1Tint` | above 0x38 inclusive | refused, 661 of 4000 rounds |
| C128 | `EffectKind02_Mode1Brighten` | unsigned compare | refused, 1222 of 4000 rounds |
| C129 | `EffectKind02_CopyLeader` | +0x5A from +0x58 | refused, 3049 of 4000 rounds |
| C130 | `EffectKind02_CopyLeader` | +2 = 2 | refused, 1247 of 4000 rounds |
| C131 | `EffectKind02_Mode3Tint` | colour 0xC1 | refused, 4000 of 4000 rounds |
| C132 | `EffectKind02_Mode3Darken` | at 0x90 | refused, 2060 of 4000 rounds |
| C133 | `EffectKind02_Mode3Wait` | > 0x38 | refused, 484 of 4000 rounds |

## 9. The rebinding, and calls across groups

**Outbound:** every callee is ours by name - no raw-address call to another
group (the `--edges` list has none out of R4F).

**Inbound, from outside the group:** `ConfigMenu_Body` (R2F, round 14 wave two)
tail-jumps to `ConfigScreen_Run` by the raw `rest_2f_callees.h`
`kConfigMachine = 0x460CB0`; `Field_ExitFromCell` (event_leader) calls
`WorldMap_ExitRecords`; E1A's `EffectKind07_FadeIn` calls
`EffectKind07_DrawSprite`; E1A's dispatchers jump through the kind tables.

**Rebound** (the round-ten form, values unchanged, each the line it changes):
`effect_1a_callees.h` `kKind07Sprite` = `bof3::addr::EffectKind07_DrawSprite`
(and its comment); `effect_1a_fuzz.cpp`'s call site `{0x2, ..}` in
`kCalls462EB0`; `event_leader_callees.h` `kAreaExits` =
`bof3::addr::WorldMap_ExitRecords`; `event_leader_fuzz.cpp`'s call site of
`Field_ExitFromCell`. Comments naming the functions by address: `effect_1a.cpp`
(four), `event_leader.cpp`, `worldmap_area.cpp`, `save_menu.cpp`,
`field_c1.cpp`, `menu_frame.cpp` (three), `config_text.cpp` (four).

**Left raw, for the coordinator:** `rest_2f_callees.h` `kConfigMachine`,
`rest_2f.cpp` 1018 and `rest_2f_fuzz.cpp` 58 / 248 (R2F's, a group of this
round); `scenario_harness.cpp` 1131's `FX_RAW(0x462F10)` row and
`scenario_harness_ekh.cpp` 70's comment (a harness; the row still serves E1A's
raw-keyed call and registers unchanged - nothing stops).

## 10. The live route

None. The catalog's reach columns are empty for all 52, and neither first-call
trace (`analysis/calltrace/reach_dragon`, `reach_whelp`) names one. **Fuzz
only.** The Config screen is reached from the field menu (and the title); a
recorded route that opens it, changes a setting and remaps a button would let
the coordinator's state hash cover the screen; which scenes spawn kinds 2, 7,
8, 9 and 0xB is the owner's to say.

## 11. For the coordinator

- **`entries_logic.txt`** (main checkout): 46 lines appended under a comment
  (six were there at the same extent). Host lines with longer extents over
  these, left for the round end: `00460C40 AAB` (nine hidden starts; R4E's
  `00460C40 6E` beside it), `00462F10 130` (two), and `00462AC0 20`,
  `00463350 F0`, `00463D80 160`, `004641D0 180` (padding only).
- **Harness gaps met in the fuzz file**: the field runs table names none of
  the Config band (the log lists eleven clones "outside the field runs"; no
  effect); the four copies above; the Latin forms of DIV-0017 / DIV-0026 /
  DIV-0051 unfuzzed (section 6).
- **No harness row stops**: `FX_RAW(0x462F10)` registers as before and keys
  E1A's raw call; this group's own row for the address registers first.
- **Counts depend on the build directory**: the figures here are this
  worktree's.

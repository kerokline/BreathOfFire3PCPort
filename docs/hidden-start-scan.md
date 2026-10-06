# The hidden-start scan: every table dispatch and every extent, against the code

**Status:** MEASURED (2026-10-06) - the platform round's step 3 batch, group
SCAN, from `phase-3/platform-round-2` at `f18b9d9c`. The scan
[`mode-rest.md`](mode-rest.md) section 0 proposed and did not run, built as
`tools/pe_jumptables.py` and run over all of `.text` (`0x401000..0x5C3668`);
output `analysis/pc_jumptables.json` (and `pc_jumptables_control.json`, the
control). **In the game's own code it finds two functions no catalogue held -
`0x591810` (`Item_UseFlags`) and `0x593950` (`ItemTrade_Dispatch`), both
Capcom's, called by the battle item menu's states and by the trade screen's
step - and one second chunk already on record.** Both are entered in
`symbols.toml` as `hypothesis` `[[func]]`s, not taken. **The control passes: with group PM's
thirteen entries removed, each half of the scan finds all thirteen.** The
rest of what it lists is platform and library code (section 4). No game
behaviour changed; nothing run live.

## 1. Why `pe_hidden.py` missed the thirteen: it stopped reading

`pe_hidden.py`'s rule (an address inside a recorded extent, 16-aligned,
after a `ret` / `jmp` and `nop` / `int3` padding) reads each extent with
capstone's `disasm` from the extent's start. **`disasm` stops at the first
byte it cannot decode, and the script does not notice.** In `0x495750`'s
extent (0xAFB bytes; `pc_hidden.json` cut its tail into `GameMode_Field`
`0x4959F0` and the 0x85B bytes [`mode-rest.md`](mode-rest.md) section 0 and
[`platform-round.md`](platform-round.md) section 2 attribute to
`pc_funcs.json` - the size is `pc_hidden.json`'s, the extent it was cut
from `pc_funcs.json`'s):

- `GameMode_Field`'s code ends with `ret` at `0x495B85`, then `mov edi, edi`
  (two bytes of padding) and its 9-entry jump table `0x495B88..0x495BAB`;
- the linear sweep reads the table as instructions and falls out of step: at
  `0x495BAB` it decodes `add [eax + 0x33909090], dl`, six bytes that run over
  the four `nop`s and the first byte of `0x495BB0` (`GameMode3_Run`, which is
  16-aligned);
- the next decode, `shl byte ptr [esi - 0x5F], 0xEA` at `0x495BB1`, ends at
  `0x495BB5`, where `EA C7 66 00 ...` does not decode - and **the sweep ends
  there**, 0x696 bytes before the extent does.

So `0x495BB0` has no `ret` / `jmp` before its padding in the sweep (the
table's data does), and the other twelve, `0x495BC0..0x496230`, were never
examined at all. Replaying the rule over `0x495750`'s extent: none of the
thirteen is an instruction boundary of the sweep. **The same silent stop
cuts 29 of `pc_funcs.json`'s 2,952 extents short, 79,925 bytes never
examined, 76,512 of them in game code** (23 extents; the largest,
`0x55CC50`, loses 0x5099 bytes, and most of the rest are the scenario band
`0x538300..0x569D9D`). Every one of those bytes is now covered by a
`[[func]]` start or examined by this scan (sections 2 and 4).

`pe_funcs.py` loses starts the same way from the other side: a direct call
target that its sweep decodes *across* (not at an instruction boundary) is
dropped (`entries = sorted(e for e in entries if e in insns)`). That is how
`0x591810`, 18 call sites, never had a row (section 4.1).

## 2. The scan

**Half 1, dispatches.** A byte scan of `.text` for `FF 24 xx` / `FF 14 xx`
with a scale-4 SIB and no base (`jmp` / `call [reg*4 + imm32]`) and for
`8B xx xx` loading `reg` from `[reg*4 + imm32]` with a `jmp reg` / `call reg`
within four instructions; a hit is kept when capstone decodes the
instruction there and it does not sit inside an instruction some flow
decoded. **1,861 dispatches, 13,352 cells.** Each table is walked while its
cells point into `.text` - bounded by the next named `[[data]]` or `[[func]]`,
by a named table's `count`, and by the first cell that lands inside a decoded
instruction. A target that is neither a `[[func]]` start nor inside the
dispatching function's own reachable code is a candidate. Then **every named
`[[data]]` whose first cell points into `.text` (1,626)**, walked the same
way; a cell inside a known start's code is not a candidate (one such cell
from a `.data` table: `Area08_ChoiceMessages` holds the value `0x4FFFFF`, a
message number, not a pointer).

**Half 2, extents.** For every `[[func]]` (10,093 at `f18b9d9c`), its bytes
to the next `[[func]]` start are compared with its reachable code: the flow
from the start through branches inside that range and the cases of its own
switch tables (a table in `.text`, MSVC's); a jump to a known start or out of
the range is a tail call, and `ret`, other indirect jumps and `int3` end a
path. **A table in `.data` is a pointer table: its cells are functions of
their own, not cases** - the rule that makes `GameMode3_Run`'s
`jmp [GameMode3_Steps + eax*4]` give its steps as candidates rather than
swallowing them. Unreached bytes that are not padding (`nop`, `int3`, and
MSVC's `mov edi, edi`, `lea ecx, [ecx]`, `lea esp, [esp]` forms), a switch
table, or an inline table the code reads as data (114: a two-level switch's
byte index, read up to the next padding) are a candidate's start; the
candidate's own flow is walked in turn, so one gap gives as many as it holds.
Then each candidate is classed by what reaches it - a direct `call` from any
flow, a tail `jmp`, a `.data` pointer, a dispatch, a `.text` immediate - and
whether a catalogue (`pc_funcs.json`, `pc_hidden.json`) lists it.

**Cross-checks that came out clean.** All 1,861 dispatch sites but three
(inside the C runtime's `memcpy` family) lie on the flow of a start or of a
candidate, so no dispatcher hides in unread code. **All 210 `NOTFN` rows of the round-fourteen cut lie
inside a known start's own flow** - reached through their host's `.text`
switch table - which is what the cut said they are: cases, not functions.

## 3. The control

The scan run twice more with `--drop`: A leaves out group PM's thirteen
`[[func]]`s, B those and the two step tables `GameMode3_Steps` /
`GameMode5_Steps` (the state before PM, when the tables had no name). In
both, **all thirteen come back as `function` candidates - and nothing else
changes: B's candidates are the as-they-are run's plus exactly these
thirteen** - each found up to three ways:

| Found by | The four modes | The nine steps |
|---|---|---|
| Half 1, dispatch | `Field_Task`'s `call [0x656A44 + eax*4]` at `0x495822` | `jmp [0x656A74 + eax*4]` at `0x495BB8` (mode 3), `jmp [0x656A84 + eax*4]` at `0x495E98` (mode 5) |
| Half 1, named table | `GameMode_Handlers` `0x656A44` (the coordinator's night scan) | `GameMode3_Steps` / `GameMode5_Steps` in A; not in B, where only the dispatch finds them |
| Half 2, extent | gaps of `GameMode_Field`'s range from `0x495BAC` to `0x496250` | the same gaps |

Half 1 alone finds all thirteen (the steps' dispatch sites are found by
bytes even when no flow reaches them), and so does half 2 alone. The run
over the symbols as they are lists none of the thirteen. B's output is
`analysis/pc_jumptables_control.json`:

```
python tools/pe_jumptables.py --drop GameMode3_Run,GameMode3_Enter,GameMode3_Leave,GameMode4_Run,GameMode5_Run,GameMode5_TurnSense,GameMode5_Turn,GameMode5_ToPlaces,GameMode5_Load,GameMode5_Script,GameMode5_Place,GameMode5_Leave,GameMode6_Run,GameMode3_Steps,GameMode5_Steps --out analysis/pc_jumptables_control.json
```

(from a worktree, with `--exe`, `--funcs`, `--hidden` and `--cut` pointing
into the main checkout).

## 4. Every candidate (over `symbols.toml` at `f18b9d9c`)

595 candidates. **439 are catalogued** - a `pc_funcs.json` or `pc_hidden.json`
start with no `[[func]]`: the MP3 decoder's 173, the C runtime's 219, the
software renderer's 27 (`0x5A3A60..0x5A62C0`, the read pass's "software
surfaces" and set-up, original-only since DIV-0031) and the hand-written
converters' 20. Known, not hidden; not listed again here. The other 156
follow. (`analysis/pc_jumptables.json` is the run over this branch's
symbols: the same 595 less `0x591810` and `0x593950`, now starts.)

### 4.1 The game's code (`0x401000..0x59E000`): three

| PC | Reached by | First instructions | Class | Reading |
|---|---|---|---|---|
| `0x591810` | **18 direct calls** (`0x436857`, `0x441BC5`, `0x448651`, `0x448701`, `0x4487E4`, `0x448924`, `0x448BC1`, `0x448C5C`, `0x449402`, `0x4494C2`, `0x4495A8`, `0x4496E8`, `0x449932`, `0x4499D0`, `0x4532B0`, `0x45413B`, `0x57DA12`, `0x57DA27`); in `Item_EquipMask`'s range | `mov eax, [esp+4]; and eax, 0xFF; dec eax; cmp eax, 3; ja; jmp [eax*4 + 0x591888]` - a prologue-less leaf | **a function no catalogue holds** | `Item_UseFlags`, below |
| `0x593950` | **a tail `jmp` from `GameMode8_TradeStep`** `0x52CF30` (ours); in `Sprite_ClutWord`'s range | `xor eax, eax; mov al, [0x93985C]; jmp [eax*4 + 0x66A470]` | **a function no catalogue holds** | `ItemTrade_Dispatch`, below |
| `0x442C60` | a tail `jmp` at `0x442BB2`, inside `BattleObj_StateCastDone` `0x442BA0` | `test byte [0x904AA8], 4; je ret; jmp 0x442DD0` | a second chunk, nothing hidden | already recorded: `BattleObj_StateCastDone`'s evidence and [`battle_obj_states.md`](battle_obj_states.md) section 1 ("two chunks") |

**`0x591810` `Item_UseFlags(category, item)`**, cdecl, no calls, 0x88 bytes
with its 4-entry table `0x591888..0x591897`: `Item_EquipMask` `0x5917A0`'s
shape one byte further on. By the category's low byte: 1, the byte at
`0x657461 + 28 * id`; 2, `0x657D79 + 26 * id`; 3, `0x658461 + 24 * id`; 4,
zero; anything else (0 among them), `0x656B38 + 22 * id` - the consumable's
flag byte. `id` is the item's low byte. Only `al` is the answer: for 2 and
the default, `eax`'s upper bytes are left from the address arithmetic. Its
callers read it as the item's flag byte - the battle item menu's target side
and state ([`battle_menu_states.md`](battle_menu_states.md) section 2),
`BattleObj_HitReceive`, the field menu's use gate `0x57D9A0`. PSX twin
`0x80166918` by [`battle_sprites.md`](battle_sprites.md). It has been
"nobody's" since round twelve (`battle_e3.md`, `battle_e4.md`), keyed by
address in the scenario and boss harnesses' standard set
([`round-14-cleanup.md`](round-14-cleanup.md) section 2). **It sits beside
`char_stats.cpp`'s `Item_EquipMask`** (the same author, the same tables);
the battle groups' harness rows already model it.

**`0x593950` `ItemTrade_Dispatch()`**, 0xE bytes, unchecked: the trade
screen's state dispatcher by the byte `0x93985C` through `ItemTrade_States`
`0x66A470`. [`field_e2.md`](field_e2.md) named the table and called the
dispatcher "nobody's"; [`sprite-draw-order.md`](sprite-draw-order.md) found
it in 2026-09 and described this very miss. Its one way in is round
thirteen's `GameMode8_TradeStep` (ours, E1F), a five-byte tail jump, so
Capcom's dispatcher runs between our step and our trade states. **It belongs
beside `ItemTrade_*`** (round twelve's FE2 states, `effect_1f.cpp`'s
`GameMode8_TradeStep`); `kEffectStd` keys it by address.

Neither is a takeover here (the brief). Both are cheap: `Item_UseFlags` is a
five-way table read, `ItemTrade_Dispatch` three instructions.

### 4.2 The hand-written pixel converters (`0x5AA5D0..0x5ADF00`): 84

Between `Gfx_PackRgb` and the MP3 decoder `Mp3_Create` `0x5ADF00` is a family
of register-convention assembly routines (`shld` packers, MMX
`punpcklbw` blends) that **`0x5AA671`** - `pc_funcs.json`'s, called only from
`0x5A6187` inside the software set-up `0x5A60E0` - selects by the display's
bit depth and green mask (`0x7DED62`, `0x7DED74`: 0x3E0, 0x7E0, else 4-4-4)
and an MMX probe (`0x5A9A30`, `cpuid`), storing pointers at `0x66C8AC`,
`0x66C8B0`, `0x6722B8`, `0x6722C4` from the tables `0x6722BC..0x672450`.

- **76 reached by a pointer in those tables** (`function`): `0x5AA7D3`,
  `0x5AA7F1`, `0x5AB159`, `0x5AB1A6`, `0x5AB20D`, `0x5AB29C`, `0x5AB328`,
  `0x5AB3BA`, `0x5AB44C`, `0x5AB4BB`, `0x5AB52A`, `0x5AB59C`, `0x5AB5F2`,
  `0x5AB653`, `0x5AB6D6`, `0x5AB75A`, `0x5AB866`, `0x5AB8C9`, `0x5AB92C`,
  `0x5AB992`, `0x5AB9DC`, `0x5ABA34`, `0x5ABAAE`, `0x5ABB29`, `0x5ABBA6`,
  `0x5ABC21`, `0x5ABC79`, `0x5ABCD1`, `0x5ABD2C`, `0x5ABD3C`, `0x5ABDAC`,
  `0x5ABDF5`, `0x5ABE44`, `0x5ABE9C`, `0x5ABECD`, `0x5ABEEF`, `0x5ABF17`,
  `0x5ABF61`, `0x5ABFAF`, `0x5ABFFC`, `0x5AC025`, `0x5AC04E`, `0x5AC07C`,
  `0x5AC08B`, `0x5AC0A6`, `0x5AC0E3`, `0x5AC17E`, `0x5AC19C`, `0x5AC1BF`,
  `0x5AC1F7`, `0x5AC245`, `0x5AC2BB`, `0x5AC32C`, `0x5AC3A5`, `0x5AC3FD`,
  `0x5AC455`, `0x5AC4F7`, `0x5AC549`, `0x5AC5BD`, `0x5AC62F`, `0x5AC6A6`,
  `0x5AC6FB`, `0x5AC7A8`, `0x5AC7E6`, `0x5AC82B`, `0x5AC892`, `0x5AC8F7`,
  `0x5AC961`, `0x5AC9AB`, `0x5AC9F5`, `0x5ACA42`, `0x5ACA6A`, `0x5ACA98`,
  `0x5ACABD`, `0x5ACB15`, `0x5ACB67`. Several are second entries into one
  body (`0x5AB159` / `0x5AB1A6` differ by a `mov edx, [0x66C860]`), so
  "function" here means "entry point".
- **6 named only by `0x5AA671`'s immediates** (`function?`): the 16-bit
  packers `0x5AA718`, `0x5AA731`, `0x5AA74A` and unpackers `0x5AA763`,
  `0x5AA776`, `0x5AA789` (5-5-5, 5-6-5, 4-4-4) - functions, `ret`-ended.
- **`0x5AA624`**, `push ebp` framed, fills 0x10000 words or dwords of its
  argument with `0x7DED7C` where they were zero, else zero: **no reference
  anywhere** (no call, jump or pointer; the image holds no dword equal to
  it). Dead code. `0x5ADBD9`: data.

**All of it is original-only**: the selector's one caller is in the
software set-up, and the software render flag's only setter is inside
Capcom's `Display_Setup` (ours since DIV-0031,
[`platform-round.md`](platform-round.md) section 3). It goes at the cutover
with the read pass's 32 ([`platform-read-pass.md`](platform-read-pass.md)
section 5), which is why it gets no `[[func]]` rows here; `Mp3_Create`'s
evidence placing the decoder at `0x5AB000` is wrong by this family - the
decoder starts at `0x5ADF00`.

### 4.3 The MP3 decoder (`0x5ADF00..0x5B9380`): 5

`0x5B4120`, `0x5B4230` (three callers), `0x5B7820`, `0x5B8FA0`, `0x5B9260`:
**direct call targets `pc_funcs.json` dropped** (section 1's second way),
each called from decoder functions `pc_funcs.json` does list. Library code
like the 173 catalogued beside them, none of which has a `[[func]]`; they
belong in whatever replaces the decoder (IDEAS I31).

### 4.4 The C runtime (`0x5B9380..0x5C3668`): 64

- 23 reached by a `.data` / `.rdata` pointer: the `__except` filters
  (`push 1; pop eax; ret`) and handler bodies (`mov esp, [ebp - 0x18]`) the
  SEH scope tables at `0x5D8D34..0x5D9374` name - pieces of their host
  functions, not functions - and three cells of the pointer run
  `0x672930..0x672960` (`0x5B93F4`, `0x5B940B` a bare `ret`, `0x5BFFF5`;
  by its shape the runtime's initialiser and terminator lists, not read);
- 2 dropped direct call targets, `0x5BAC34` and `0x5C210C`; one chunk
  `0x5BADDD`; 11 named by `.text` immediates; 16 `memcpy` / `memmove`
  switch cases (`0x5BE148..0x5BFFDC`); 10 unreferenced and one undecodable.

Runtime code; the cutover links our own.

## 5. The catalogue's extents against the code

**`pc_funcs.json` / `pc_hidden.json` extents, 10,093 starts:**

- **897 run past the code.** 867 of them **run over 7,610 later
  `[[func]]` starts** - the class that hid the thirteen; every one is a
  start now. The biggest hosts are `pc_hidden.json`'s first cut:
  `BossMikba_DrawQuad` `0x43E290` over 149 (its code 0x2A5 of 0x23A0),
  `Effect_ApplyResult` `0x44B9F0` over 129, `Party_ApplyRecord` `0x5197F0`
  over 100, `BareRet` `0x437CC0` over 85 (a one-byte `ret`). The other 30
  hold no start: inline byte tables after the code (`Area44_GateTailA`
  +0x16, `AreaMap_CellBlocked` +0xF6, `Field_CellKind` +0xF1, ...) - **a
  mis-sized extent with nothing hidden** - except `Item_EquipMask` (hides
  `0x591810`), `Gfx_MoveCells` and `Gfx_PackRgb` (section 4.2),
  `Mp3_SetIo`, `Mp3_Destroy`, `Mp3_MemoryIo` (catalogued decoder starts) and
  `Rand` (the runtime's pointer-run cells, section 4.4).
- **56 stop short of the code**: `pc_hidden.json` cut a host at one of its
  own switch cases, a start the padding rule took (`Area27_TailDropIn1`
  `0x403570` cut at its case `0x4035B0` to 0x40; the code and table run to
  `0x40365C`). These are the `NOTFN` hosts; the hand sizes in their evidence
  are right and the catalogue's are not.

**The groups' hand sizes** (the first `0x.. bytes` in a `[[func]]`'s
evidence) against the catalogue's: 6,275 compared, **5,414 differ by padding
only** (the catalogue runs to the next start), **861 differ by more**:

| What settles it | Count |
|---|--:|
| The catalogue runs over later starts (the hand size is the function) | 783 |
| The flow agrees with the hand size | 32 |
| The flow agrees with the catalogue | 13 |
| Neither (an inline table after the code counted by hand, or the first `0x.. bytes` in the evidence is another count) | 33 |

None of the 33 hides a start: section 4 lists every byte the flows leave
unexplained. Examples of "neither": `Mp3_MemoryIo`'s first byte count is the
0x2C-byte I/O table it writes, not its size; `Gfx_UploadLzss`'s 0x410 is its
locals. The full list is the JSON's `evidence_size_disagree`, each row
with the hand size, the catalogue's, the flow's and which source.

## 6. What this does not cover

- Starts reached only by a computed address (a base plus an offset, a
  pointer built at run time) and in no flow's gap: none is known in game
  code; the scan cannot rule them out.
- A gap that begins with undecodable bytes is skipped whole (two, both in
  section 4: `0x5ADBD9`, `0x5BFEB1`); an inline byte table is read to the
  next padding. A function packed against data with no padding between could
  hide behind either; of the 114 inline tables only `0x556B20` (151 bytes)
  holds a byte above 0x3F, its 0x42 default - a byte index.
- Calls that reach a candidate are counted from the flows of known starts
  and candidates; a call from code no flow reaches would be missed.
  `0x5AA624` has none by a byte scan of every `E8` / `E9` too.

## 7. For the coordinator

- **Two `[[func]]` rows added, `status = "hypothesis"`**: `0x591810`
  `Item_UseFlags` (beside `Item_EquipMask`, `char_stats`), `0x593950`
  `ItemTrade_Dispatch` (beside the trade states). Neither has an `impl`; the
  count of "ours" does not move, and Capcom's starts in game code go from 0
  catalogued to 2. Both are typed (`ret` / `params`), so the generated
  header binds them to Capcom's address; nothing calls the names yet (the
  harness rows key them by address).
- `pe_hidden.py` and `pe_funcs.py` keep their bugs (section 1): a rerun of
  either would still miss these. A fix (`disasm` resumed past an
  undecodable byte; a dropped call target kept) would change
  `pc_funcs.json` / `pc_hidden.json` and so the tracer's entry lists - not
  done, since nothing downstream reads them for the count any more.
- `Mp3_Create`'s evidence ("the MP3 decoder (0x5AB000..0x5B9380)") has the
  decoder's start wrong by section 4.2; left as written (hard rule 3), noted
  here.

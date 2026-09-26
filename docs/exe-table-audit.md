# Audit: does taken-over code transcribe the exe's game tables?

**Status:** IN PROGRESS (2026-09-26, `src/` at `f2444e9`; the audit is done and §7 records what was fixed; the rule's wording is open)

The rule under test, proposed in [`ASSET_SOURCES.md`](ASSET_SOURCES.md) §5:

> A taken-over function reads game tables from loaded data — the image by
> address today, the asset cache later. It does not transcribe their values
> into C++.

Two reasons: a transcribed table is game data in a `.cpp` file (rule 1), and
it binds that function to one build, which rules out sourcing the table from a
disc of another region.

**Result: `src/` already follows the rule, almost everywhere.** Reading content
tables by address is a consistent habit. Two transcriptions were found, both
small. There are three borderline cases where a `switch`'s case list was
copied out of `.text`, and a few comments that quote game text.

No values or game text are reproduced in this note.

## 1. Method, and its limit

- All 1,655 array initialisers in `src/` were classified by hand: 327 in
  ordinary files and 1,328 in `*_fuzz.cpp`. The grep was
  `grep -rnE "(constexpr|const)\s+[^=;(]+\[[^]]*\]\s*=\s*\{" src`, extended
  with a scan for any line holding eight or more numeric literals (to catch
  vector and brace initialisers the regex misses), non-ASCII text, and
  comments such as "as the exe has it" or "table at 0x".
- Every `[[data]]` entry in `symbols.toml` (458) and every `k… = 0x5C4000–0x675FFF`
  constant in `src/` was cross-referenced to the files that read it.
- **Limit:** `BOF3.exe` was not available where the audit ran. So whether a
  value was an instruction immediate or a `.rdata`/`.data` table is judged
  from source comments, `docs/` and `symbols.toml`, not from disassembly. §4
  names the function to disassemble for each case the comments do not settle.
- The violations in §3 were re-read in the source to confirm them.

## 2. Counts (non-fuzz files)

| Category | Count | Meaning |
|---|---:|---|
| **V** violation | **2** | table values copied out of the exe's data (one of the two is in a fuzz file) |
| **?** unclear | **3** | a `switch`'s case list, lowered by the compiler into `.text`, held as C++ data |
| I code | 26 | the original's immediates, tables it built on its stack, compiler float constants |
| S layout | 4 | strides, counts and addresses: where data is, not the data |
| P scaffolding | 179 | patch bytes, call-site and clone tables, expected-value checks |
| F fuzz input | 69 | edge values inside self-tests |
| O ours | 17 | launcher, look and input settings we authored |
| — | 31 | local arrays of runtime values, not tables |

The fuzz files are almost entirely F and P. `worldmap_area_fuzz.cpp:494-496`
shows the right pattern: it snapshots the exe's tables at run time before
scribbling over them.

## 3. Findings

### Violations

- **V1 — `src/input/bindings.cpp:189-199`, `Bindings::Defaults().keys`.** The
  24 pairs of `Key_TableDefault` `0x66C648`, copied in order. Only the
  launcher uses it. In game, `pad_read.cpp` leaves the exe's table alone
  unless `BOF3X_KEYS` is set. This is PC-only platform data, on no disc.
  *Fix:* the launcher already opens `BOF3.exe` to hash it, so it can read the
  pairs from the file. Or "defaults" can mean exporting no `BOF3X_KEYS`.
- **V2 — `src/game/magic_fx_reached_fuzz.cpp:534-536`, `kRates`.** `Steal_RateTable`
  `0x65AC20`, copied as a fuzz seed. Under another build it would silently
  seed wrong values. *Fix:* snapshot the table before the fuzz runs, as
  `worldmap_area_fuzz.cpp` does. `cheats.cpp:26` quotes the same values in a
  comment.

### Game text quoted in comments

Rule 1 covers "a paste into a doc", and a comment is the same thing.

- **`src/game/pause_text.cpp:10-13`** carries the four PC pause lines in full.
  *Fix:* keep the addresses and the English glosses, and drop the original
  text.
- Shorter samples used to identify a string: `win_main.cpp:86-87` (the window
  title and error messages), `battle_windows.cpp:455`, `battle_text.cpp:15-17`,
  `menu_verbs.cpp:85-86`. These are defensible as identification. Trimming
  `win_main.cpp` the same way would be tidy.

### Unclear: a `switch` lowered into `.text`

In each case Capcom's source had a `switch`. The compiler lowered it to an
index table and a jump table inside the function's body. By origin that makes
them code, but our port holds the case list as data, and the case list is
game content.

- **?1 — `field_blocked.cpp:40`, `kBlockingNibbles`:** which cell codes block
  a step. `CheckCellTable()` already computes this mask from the image at
  start-up and aborts on a mismatch. *Fix:* use the computed value.
- **?2 — `event_ops.cpp:43-50`, with `:1325` and `:1361`:** the areas that have
  step and arrive hooks, and each hook's handler ([`event-ops.md`](event-ops.md)
  §6). Per-area content. On the PSX these hooks live with the area overlays.
- **?3 — `menu_windows.cpp:1157`, `kCategory`:** equip slot to item category.
  Small, and the same in every region.

These need a policy decision (§6), not just a fix.

## 4. Borderline code cases

Settle each by disassembling the original (`tools/pe_disasm.py`). It is code
if the values are `mov [esp+n], imm` stores or case-body immediates, with no
operand in `.rdata`/`.data` feeding them.

- **Built on the stack on the PC, but copied from a data section on the PSX:**
  `char_stats.cpp:86 kIconClut` (`0x5903F0`), `battle_windows.cpp:364 kCommandClut`
  (`0x4434C0`), `battle_damage.cpp:419 kFlagRoll` (`Battle_CalcDamage 0x445CF0`),
  `battle_flow.cpp:358 kChance` (`Battle_RollDrops`), `battle_setup.cpp:317 kLines`
  (`0x44AA00`), and `title_menu.cpp:40`'s widths. Code for this build. A build
  sourced from a disc must take them from that disc's executable. `kFlagRoll`
  and `kChance` are balance numbers, the most data-like of the set.
- **Origin unknown:** `event_objs.cpp:265 kKind` (disassemble `0x534A00`) and
  `char_stats.cpp:226 kHalved` (disassemble `0x590660`).
- **Compiler float constants** (`win_main.cpp:100`, the cull bounds in
  `map_field_objects.cpp`): x87 has no float immediates, so these sat in
  `.rdata` only because of the compiler. They count as code.
- **Content written as code in the original too:** item-effect switches in
  `char_stats.cpp`, weapon-family damage doubling in `battle_damage.cpp`,
  `EnemyAI_ChooseActions`, and the per-area handlers in `worldmap_area.cpp` and
  `world_map.cpp`. They are code on the PSX too, so the rule permits them.

**Layout notes:**

- `name_tables.cpp`'s strides are the PC's 16-byte name layout. The JP and US
  records use 8- and 12-byte names, so a disc-sourced build needs a schema
  per region there.
- `menu_windows.cpp:1072-1075` repeats the item tables' addresses and strides
  as raw numbers instead of the named constants.

## 5. What a disc-only build must source

This is the inventory [`ASSET_SOURCES.md`](ASSET_SOURCES.md) §5 asks for.

| `symbols.toml` `[[data]]` class | Entries | Needs a disc source? |
|---|---:|---|
| Runtime state in BSS | 225 | no |
| Initialised runtime state | 22 | no (initial values only) |
| Code-pointer and dispatch tables | 133 | no: these become our code |
| Platform | 3 | no |
| **Content tables** | **75** | **yes** |

The 75 fall into these groups (the audit's working notes list them by address):

- **Items and battle:** the six name/stat tables (`NameTable_*`), damage
  variance, holy affinity, steal rates, sparkle tables, encounter slot chance,
  `Magic_Rows` (mixed content and code pointers), the new-game character
  records.
- **Field and scripts:** `Area_Descriptors` (the compiled movement and event
  scripts sit behind it), op-length tables, move speeds and direction tables,
  the encounter, event, blocking and jump cell lists, the world-map records and
  area 29/33 tables, map-cell geometry.
- **UI and text:** menu button sets, verbs and labels, command boxes, field-menu
  ids, shop cursor widths, message-box placement, CLUT tables.
- **No PSX source:** `Math_SinTable` (PSX library data), and the PC-only
  `Dat_FileNames`, `Key_TableDefault` and `Pause_Lines*`.

**A second list is invisible to tooling.** About 150 more content tables are
read correctly by address, but only through `at::k…` constants in `*_callees.h`
files, with no `[[data]]` entry. They cover battle balance, placement offsets,
party and field tables, the experience and growth tables, shop inventories,
menu layout and art, and battle text. A must-source list generated from
`symbols.toml` would miss all of them.

**Since 2026-09-26** the item-adjacent ones among them have `[[data]]` entries
(`Shop_Records`, `Char_ExpTable`, `Char_TraitLists`, the effect and magic
index tables, the icon tables and others), and the six item and ability
record tables have field layouts in [`tables.toml`](../tables.toml). The same
research found that `NameTable_Abilities` is framed 16 bytes late; the code
reads `Ability_Records` `0x65C4C8`. It also found that `Item_Price` read the
help-line message, not the price, so it is now `Item_HelpMessage`.

For about half the content tables, the PSX address is already recorded
(`psx =` or a comment). Where the port re-laid UI textures, the PC's UI
geometry tables may match no PSX table at all.

## 6. Proposed actions

For the owner to confirm. None has been done.

1. **Fix V1 and V2.** Both are small.
2. **`field_blocked.cpp`:** use the mask `CheckCellTable()` computes (?1).
3. **Trim `pause_text.cpp:10-13`** to addresses and glosses. `win_main.cpp:86-87`
   is optional.
4. **Decide the switch policy (?2, ?3).** Either read index and jump tables in
   place, or state that a `switch`'s case list counts as code. The first
   suits a disc-only build better: `event_ops.cpp`'s area list is per-area
   content, which a Western disc might order differently.
5. **Give the ~150 `at::`-only content tables `[[data]]` entries** (name,
   ctype, count, psx). This makes §5 generatable. Mechanical, and the
   largest item here.
6. **Write the rule with its exceptions named:** stack-built tables, switch
   immediates and compiler float constants are code; expected-value checks
   (P) verify, they do not supply. Then decide whether it becomes a
   `CLAUDE.md` hard rule.

## 7. Resolved (2026-09-26)

- **V1 fixed.** The launcher reads `Key_TableDefault` out of the player's
  `BOF3.exe` by address, right after the hash check
  (`src/launcher/exe_image.cpp`, the one launcher file that includes
  `symbols.gen.h`). `input::KeysFromTable` parses it and `SetDefaultKeys`
  hands it to `Bindings::Defaults()`. An unreadable table is a `Die`. In the
  game, `Defaults().keys` is now empty; nothing there reads it (`pad_read.cpp`
  uses only `.pad`).
- **V2 fixed.** The fuzz snapshots `Steal_RateTable` from the image at the
  start of `SelfTest`, the `worldmap_area_fuzz.cpp` pattern. The values in
  `cheats.cpp`'s comment are replaced by a reference. The table is Capcom's
  original steal chance. DIV-0046's always-steal cheat never touches it: it
  zeroes the random byte's mask instead.
- **?1 fixed.** `field_blocked.cpp` reads the blocking nibbles from the
  switch tables at `0x518660` / `0x518654` on first use. The structural checks
  (an index above 2, a jump target other than the two case bodies) are still
  Fatals.
- **?2 and ?3 kept as code.** Reading them in place would mean decoding
  instructions, not reading a table. `kCategory`'s values are case-body
  immediates. The area lists would need the switch's bounds decoded from
  `.text`, and handler addresses taken from each case body's `call`. Both are
  code on the PSX too: the area hooks live in the area overlays, which a
  disc-only build replaces with our code anyway. So a `switch`'s case list
  counts as code.
- **The pause lines stay.** The owner's call: they exist only in the PC port,
  so no disc build has to supply them.

Not verified at run time: this container has no `BOF3.exe` and cannot run
Windows programs. The changed files compile under `i686-w64-mingw32-g++`
`-std=c++20 -Wall -Wextra` with no new warnings, and `KeysFromTable` passed a
native test on made-up tables. What the owner should check: the launcher
starts, and the Controls dialog's Defaults button shows the 24 original keys.
Both fuzzes (`field_blocked`, `magic_fx_reached`) should still report 0
mismatches under `BOF3X_SHADOW`.

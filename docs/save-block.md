# The save block, byte by byte

**Status: IN PROGRESS (2026-11-10).** One reading pass over the source for each of
five ranges, then the passes' claims that a save can show were measured on 80
distinct PC saves (the owner's play folder and `tools/recipe_saves`). The range
files are the record; this page is the index. Where it differs from
[`save-editor-spec.md`](save-editor-spec.md) section 7 or 8, this page is the later
reading.

The block is `0x10B0` bytes, live at `0x9039E0`. Offsets are block offsets; a
PlayStation card save holds the block after a `0x200` header.

Tags: **READ** code that writes or uses the bytes was read; **MEASURED** checked on
the saves; **UNTOUCHED** no code in `src/` reaches the bytes.

## 1. Result

| | Bytes |
|---|---:|
| The block | 4,272 |
| With a writer or a reader read in code | 3,810 |
| UNTOUCHED | 462 |

**Every UNTOUCHED byte is 0 in all 80 saves (MEASURED).** So no byte of a PC save
is live without a known use. The meaning of a few read bytes is still thin
(section 6).

## 2. The map

| Offset | Size | What | File |
|---|---:|---|---|
| `+0x000` | 16 | Scenario state, a copy made at save and copied back at load: chapter, status bits, state, a chapter's own byte, run, step, a u16 timer, a u16 step count, 6 bytes only ever copied (0 in every save) | [A](save-block/range-a-head.md) |
| `+0x010` | 10 | The field's "mode tail" state machine, saved as it stands: `+0x12` phase (the event-flag seed marker), `+0x13` kind, `+0x14` state, `+0x15` argument, `+0x16` u16 timer. A save made at a save point holds `0A 07 FF` at `+0x13` (MEASURED 10 of 80; the rest `00 00 00`) | A |
| `+0x01A` | 6 | UNTOUCHED | A |
| `+0x020` | 4 | The four movement-script counters, copied at save and load | A |
| `+0x024` | 12 | Where to resume: area u16, `+0x26` UNTOUCHED, `+0x27` facing in the low bits with bit 7 a drop-in, x and z | A |
| `+0x030` | 8 | Scratch shared by chapters: a saved party, chapter 10's eight counts, member ids | A |
| `+0x038` | 56 | UNTOUCHED | A |
| `+0x070` | 2 | Checksum; `+0x72..+0x73` its dword's upper half | A |
| `+0x074` | 4 | UNTOUCHED | A |
| `+0x078` | 7 | Options: message speed, mono sound, window colour, background, two bytes no screen sets, dash. **Not the button mapping** | A |
| `+0x07F` | 17 | UNTOUCHED | A |
| `+0x090` | 1,312 | Eight character records, section 3 | [B](save-block/range-b-characters.md) |
| `+0x5B0` | 160 | Chapter flag rows | [`story-flags.md`](story-flags.md) |
| `+0x650` | 32 | Event flags 0..255 (32 bytes, not 40) | `story-flags.md` |
| `+0x670` | 8 | UNTOUCHED | [C](save-block/range-c-party.md) |
| `+0x678` | 4 | Zenny | C |
| `+0x67C` | 4 | Area 189's walk state: a u16 frame word, a walk count, a reserve | C |
| `+0x680` | 2 | Active formation; unlocked formations (bit n opens cell n) | C |
| `+0x682` | 6 | Field order, battle order: the same ids, `0xFF` at the end | C |
| `+0x688` | 32 | Enemy kinds beaten, 256 bits | C |
| `+0x6A8` | 32 | Abilities learnt, 256 bits | C |
| `+0x6C8` | 32 | Enemy kinds identified, 256 bits | C |
| `+0x6E8` | 4 | Play clock | C |
| `+0x6EC` | 32 | Chest and field-object flags | `story-flags.md` |
| `+0x70C` | 32 | Largest catch for each kind of fish | C |
| `+0x72C` | 32 | Passage find flags | C |
| `+0x74C` | 1 | The party-combination row, bit 7 set by a load: the load path loads the party's files by it | C |
| `+0x74D` | 1 | `Field_ScriptFlags`' low byte, copied at save and back at load | C |
| `+0x74E` | 3 | Fishing: lure id, lure count, rod id, held outside the inventory | C |
| `+0x751` | 1 | Music track to restore | C |
| `+0x752` | 2 | UNTOUCHED | C |
| `+0x754` | 4 | Battle count | C |
| `+0x758` | 16 | Four u32 tallies: zenny gained (not every gain), passages taken, chest contents taken, fishing sessions | C |
| `+0x768` | 12 | The return trip: x, z, area, an "away" byte, the facing on arrival | C |
| `+0x774` | 1,024 | Item ids then counts: 4 categories of 128, counts to 99 | [D](save-block/range-d-skills.md) |
| `+0xB74` | 32 | Key items, ids only | D |
| `+0xB94` | 128 | The shared ability list: ability ids | D |
| `+0xC14` | 1 | A rank 0..12 that area 113 reads for its reward | D |
| `+0xC15` | 5 | UNTOUCHED | D |
| `+0xC1A` | 1 | World-map display mode 0..2 | D |
| `+0xC1B` | 1 | Battles since master 4 was met, to 30 | D |
| `+0xC1C` | 12 | Remembered pages, scroll rows and cursors of menus | D |
| `+0xC28` | 24 | Six recent gene combinations, newest first, 4 bytes each: up to three gene numbers then `0xFF`, and a form byte; a form byte of `0xFF` is an empty row | D |
| `+0xC40` | 48 | Twelve stored gene combinations, the same rows | D |
| `+0xC70` | 4 | Gene bits, a dword: 18 bits used, the top byte must stay 0 | D |
| `+0xC74` | 6 | Masters met; masters open | D |
| `+0xC7A` | 2 | UNTOUCHED | D |
| `+0xC7C` | 8 | Two countdowns shaped like the play clock; while one runs an ability is refused | D |
| `+0xC84` | 28 | UNTOUCHED | D |
| `+0xCA0` | 28 | Slot summary: name bytes 0..4, portraits, level, copies of the window colour and background, clock, EXP, cleared byte, `+0xCB5` (0 in the block; the slot's present flag after a save), name bytes 5..8 at `+0xCB6` | [E](save-block/range-e-tail.md) |
| `+0xCBC` | 18 | Nine button words, a copy of the live mapping at save, copied back at load | E |
| `+0xCCE` | 2 | UNTOUCHED | E |
| `+0xCD0` | 32 | Faerie village values: a flag tally, five battle-count stamps, an area-seen word, mood, two levels, two biases rebuilt from the records, a transient byte | E |
| `+0xCF0` | 480 | Faerie records, 60 of 8 bytes | E |
| `+0xED0` | 64 | Faerie buildings, 8 of 8 bytes | E |
| `+0xF10` | 300 | Faerie names, 60 of 5 bytes | E |
| `+0x103C` | 4 | UNTOUCHED | E |
| `+0x1040` | 1 | **Ours, not Capcom's:** the mountain-passes bits of DIV-0191 (1 in 9 saves) | E |
| `+0x1041` | 111 | UNTOUCHED | E |

## 3. A character record

PC offsets within the record (the PlayStation's are 4 lower from `+0x09` on).
[B](save-block/range-b-characters.md) has every byte.

- **Rebuilt on every load:** `+0x20..+0x3F`, from `+0x40..+0x5F` plus equipment and
  master bonuses, for all eight records. Editing them does nothing. HP is cut to
  the new maximum; AP is not.
- **The spec had fatigue one byte off.** PC `+0x1D` is the halving bits (0 in
  all 640 records measured). PC `+0x1E` is the max-HP scale: up by one when a
  member ends a fight down, cleared at the inn (values 0, 1, 2, 5, 9 measured).
- **Named now:** PC `+0x0B` is a flags byte (bit 0 joined, bit 1 fixed in the party
  screen; values 0, 1, 3 measured); PC `+0x1C` is the current willpower; PC
  `+0x89..+0x8E` are signed per-level bonuses a master gives, in the order HP, AP,
  power, defence, agility, intelligence.
- **By position, not by id:** the EXP curve is the record's index. Record 0 feeds
  the summary. Record 7 is reused for guests.
- **Record 7's `+0x7E` is not an ability:** one minigame keeps its rank there
  (the value 5 in 17 of the 80 saves).
- UNTOUCHED: PC `+0x3D..+0x3F`, `+0x5D..+0x5F`, `+0x8F..+0xA3`, 27 bytes a record.

## 4. Measured checks of the passes' claims

| Claim | Result on 80 saves |
|---|---|
| Every UNTOUCHED byte | 0 in all |
| `+0x13..+0x15` is `0A 07 FF` in a save-point save | 10 saves; the other 70 hold zeros |
| `+0x0A..+0x0F` | 0 in all |
| `+0xCA9`, `+0xCAA` equal `+0x7A`, `+0x7B` | 80 of 80 |
| The 9-byte summary name is record 0's | 77 of 80 (the field leader's: 67) |
| An empty gene row is four `0xFF` | no exception; 282 rows in use |
| No faerie stamp is above the battle count | no exception |
| The active formation is unlocked | 80 of 80 |
| `+0x74C` | bit 7 set in 79, one save holds `0x02` |
| `+0x1040` | 0 or 1 |

## 5. What must agree, for an editor

- The two party lists hold the same ids; `+0x74C` matches them; the active
  formation is unlocked and fits the party size.
- The play clock is not all zero: zero reads as a new game.
- Faerie stamps stay at or below the battle count `+0x754`. A change to the count
  moves every stamp by the same amount.
- The button words stay a valid mapping: with DIV-0133 on, our build stops at the
  load on a bad one.
- `+0x7A`, `+0x7B` and their summary copies; the summary and record 0.
- Gene rows are `0xFF` when empty, never 0. The gene dword's top byte is 0.
- The mode tail `+0x13..+0x17` and script flag `0x40` in `+0x74D` are cleared
  together or not at all.
- A member's level at joining a master is not above their level. AP is not above
  its maximum. An item worn is not also in the inventory count.

## 6. Open

- No reader was found for the accuracy byte, and no setter for the halving bits.
- Whether the PlayStation reads its twin of `+0x26` and of `+0xC84..+0xC9F`.
- Settled 2026-11-10: the two countdowns do reach zero and stop (`CountDown`,
  `mode_flow.cpp`); the note that said never is corrected there and in `symbols.toml`.
- Fatigue has two caps, 5 and 9, and both are the game's design (the owner,
  2026-11-10): a member left down after a fight loses max HP up to the cap of 5
  until an inn; area 189's walk without water goes on to 9.
- Possible defects of the original that the passes list in their files' last
  sections. None is in `docs/known-defects.md` yet.
- The passes are one reading each. Only the claims in section 4 are measured.
- The PlayStation code was not read. Its saves agree where a save can show it: on 20
  distinct PlayStation blocks (US and JP cards) every UNTOUCHED byte is 0, the
  halving byte is 0 in all 160 records and fatigue holds 1, 3 and 5, two blocks
  hold `0A 07 FF` at `+0x13`, the option copies agree in all 20, no gene row and
  no faerie stamp breaks its rule, and `+0xCB6..+0xCB9` (the PC's name bytes 5..8)
  are 0.

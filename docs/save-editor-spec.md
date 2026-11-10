# Save editor for the launcher: specification

**Status:** DRAFT (2026-11-09). Nothing is built. The format facts tagged
MEASURED were checked that day against the saves in section 15. Everything else
carries its tag. The owner's calls so far are in section 1, the open ones in
section 14.

A native page in `bof3x-launcher` that opens, edits, converts and creates Breath
of Fire III saves for the PC port and for PlayStation memory cards. It reads
names and defaults from the player's own install, so it ships no game data.

## 1. What the owner has decided (2026-11-09)

- **Reimplement natively for the launcher**, rather than embed Red Herring's web
  editor. Red Herring gets back either a fixed-up fork or a doc of suggested
  fixes. Which one is not decided.
- **Target systems on export:** the PC port's files, or card files for Mednafen,
  Beetle, psxrecomp and the like, with conversion between them.
- **Bytes the existing editor does not model are worth adding**, optional for
  the player.
- **Creation means from nothing, settable to any story point.**
- **The story-point table and the flag documentation are reference material**, like
  the song names: neither code nor assets, so they are checked in. Investigation
  results also serve as material for the wiki.
- **Chapter starts come first.** They give a baseline of the default flags at each
  chapter, and finer story points are added on top of it.

## 2. Provenance

`glitch-in-the-herring/bof3-save-editor` has no licence (GitHub API, 2026-11-09),
so its code cannot enter this repo, and CC BY-SA or copyleft terms would not
change that ([`LICENSING.md`](LICENSING.md) section 4). This spec is the wall:

- The code is written from this document and from our own docs.
- Red Herring's editor is read and credited as a cross-check on offsets. No code
  and no tables come from it, nor text from his CC BY-SA notes.
- frost's work is usable: the decomp is CC0, the wiki and monorepo are MIT. Facts
  taken from it are tagged CITED until we measure them.
- A shipped feature needs each field it edits at MEASURED or READ.

Evidence tags used below:

| Tag | Meaning |
|---|---|
| MEASURED | Checked against real saves, section 15 |
| READ | From our own disassembly or takeover source |
| CITED | From frost, the sibling or Red Herring, not yet checked by us |
| UNKNOWN | Live in real saves, meaning not established |

## 3. Terms

- **Block:** the 4,272-byte (`0x10B0`) game state. All offsets below are block
  offsets unless a section says otherwise.
- **Container:** what wraps a block on disk: a PC file or a memory-card image.
- **Target:** a container plus the release it is meant for. A target fixes the
  record layout, the text encoding and the file naming.
- **Story point:** a named position in the game that creation can start from
  (section 11).

## 4. Targets

| Target | Container | Record layout | Character names | File or entry name |
|---|---|---|---|---|
| PC port, any language | PC file | PC | The install's glyph codes (section 9) | `BISLPS0X.DAT` |
| PSX JP (`SLPS-00990`) | Raw card | PSX | One-byte kana | `BISLPS-00990BOF3NN` |
| PSX US (`SLUS-00422`) | Raw card | PSX | ASCII | `BASLUS-00422BOF3NN` |
| PSX EU, FR, DE | Raw card | PSX | Open, section 14 | Open, section 14 |

The raw-card format covers every emulator named so far. Mednafen, Beetle
per-card files and psxrecomp all hold the same 128 KiB image (MEASURED, section
15), so they differ by file name only. RetroArch's `.srm` cards are the same
format too (MEASURED, section 15). The target picker offers "PC" or "memory
card" plus the release, and the file extension is the player's choice.

**Out of scope for now:**

- DexDrive `.gme` and single-save files. These only add headers, so they can
  follow, but we hold no samples.
- PS3 `.psv` and Vita or PSP `.vmp` files, which are signed, and PSP savedata,
  which uses the PSP's own save utility.
- Mednafen's `.mc0`..`.mc9`. These are gzipped save states, not cards (MEASURED:
  magic `1F 8B`, over 1 MB each). Opening one gets a plain refusal.
- The port's quicksaves, below.

## 5. Containers

### 5.1 PC file

- One file per slot in the install's game folder, `BISLPS%02X.DAT`, slots `00`..`0F`. READ:
  [`save-files.md`](save-files.md) section 2.
- Exactly `0x12B0` bytes. The block sits at file offset 0, followed by `0x200`
  zero bytes. MEASURED: 67 of 67 files.
- **`BISLPS10`..`1F` are quicksaves**, a different format
  ([`quicksave.md`](quicksave.md) section 4). They match the same `BISLPS??.DAT`
  pattern. The editor lists and writes `00`..`0F` only, and never touches `1X`.

### 5.2 Raw memory card

`0x20000` bytes, starting `MC`. Directory frames at `0x80 * n` for `n` in 1..15,
save bodies at `0x2000 * n`.

Directory entry, MEASURED on all 28 BoF3 entries:

| Offset | Value |
|---|---|
| `+0x00` | `0x51`, first block in use. Only this value is a live save, so deleted entries (`0xA1`) are ignored |
| `+0x04` | u32 size, `0x2000` |
| `+0x08` | u16 next block, `0xFFFF` |
| `+0x0A` | ASCII file name |
| `+0x7F` | XOR of bytes `0..0x7E` |

Save body, MEASURED on all 28:

| Offset | Size | Content |
|---|---:|---|
| `0x000` | 2 | `SC` |
| `0x002` | 1 | Icon flag, `0x13` on every sample |
| `0x003` | 1 | Block count, 1 |
| `0x004` | 64 | Title, Shift-JIS, for the BIOS menu only |
| `0x060` | 32 | Icon palette |
| `0x080` | 384 | Three 16x16 4bpp icon frames |
| `0x200` | `0x10B0` | The block |
| `0x12B0` | `0x50` | Zero |
| `0x1300` | `0xD00` | Not written by the game: non-zero on 24 of 28 |

- **Replacing a block:** write `0x200..0x12AF` only. Everything else stays. The
  title is refreshed if the play time or level changed.
- **New entry:** zero the body past `0x12B0`, write the header, write the
  directory frame with its XOR.
- **Icons:** copied from the source when converting a card save. Otherwise an icon
  we draw ourselves. Capcom's pixels are never shipped.
- **`NN` must be `00`..`02`.** The game lists only names whose last digit is 0 to
  2 (CITED: frost, `formats/save`). MEASURED: every sample uses `00`, `01` or
  `02`. A card therefore shows at most three BoF3 saves in game, and the editor
  says so if a fourth is asked for.

## 6. The checksum

A u16 at `+0x70`: the byte sum of `0..0x10AF` with that word counted as zero,
low 16 bits. MEASURED: all 12 distinct card blocks and all 67 PC files. The port's load
menu rejects a wrong one as damaged, and the PSX does too (READ:
[`save-menu.md`](save-menu.md); CITED: frost `SAVE_RunLoadVerify`).

On open, a bad checksum gets a warning and the block opens read-only until the
player chooses "Open anyway". On every write the checksum is recomputed last.

## 7. The block

Class says how the editor treats a field:

- **E:** editable on a normal page.
- **A:** editable on the Advanced page only.
- **D:** derived. Regenerated on write, editable only on Advanced.
- **P:** preserved byte for byte, shown in the hex view.

The table was rewritten on 2026-11-10 from [`save-block.md`](save-block.md) and
its range files [A](save-block/range-a-head.md), [B](save-block/range-b-characters.md),
[C](save-block/range-c-party.md), [D](save-block/range-d-skills.md) and
[E](save-block/range-e-tail.md), which hold the evidence for each byte.
UNTOUCHED there means no code in our source reaches the bytes.

| Offset | Size | Field | Class | Evidence |
|---|---:|---|---|---|
| `+0x000` | 1 | Chapter, s8, a copy made at save and copied back at load. Selects the flag row `+0x5B0 + 8 * chapter`; 0..19, not 16 | A | READ [A](save-block/range-a-head.md), [`ending-backdrop.md`](ending-backdrop.md); CITED frost `SceneState` |
| `+0x001` | 1 | Status bits, copied the same way. Bit 7 starts the next chapter at the next area entry: clear it with a chapter set by hand | A | READ [A](save-block/range-a-head.md) |
| `+0x002` | 1 | Chapter state. A load's area entry sets it to 1 | A | READ [A](save-block/range-a-head.md) |
| `+0x003` | 1 | A chapter's own byte (chapter 17: the part) | A | READ [A](save-block/range-a-head.md) |
| `+0x004` | 1 | The chapter's run | A | READ [A](save-block/range-a-head.md) |
| `+0x005` | 1 | The step within the run | A | READ [A](save-block/range-a-head.md) |
| `+0x006` | 2 | The chapter's timer, u16 | A | READ [A](save-block/range-a-head.md) |
| `+0x008` | 2 | Step count as of the last field pass, u16 | A | READ [A](save-block/range-a-head.md); MEASURED: differs on every save (section 12.1) |
| `+0x00A` | 6 | Copied at save and load, used by nothing else | A | READ [A](save-block/range-a-head.md); MEASURED 0 in all 80 PC saves |
| `+0x010` | 2 | Written 0 with the tail block, never read | P | READ [A](save-block/range-a-head.md) |
| `+0x012` | 1 | "Event flags seeded", the tail's phase: 0 makes the game seed event flags 0..127 from its table on the first field frame, then it is 1 | D | READ and MEASURED: 1 in every PC save ([`story-flags.md`](story-flags.md) section 9) |
| `+0x013` | 1 | Field tail kind, run every field frame. Clear the tail (`+0x13..+0x17`) and script flag `0x40` in `+0x74D` together or not at all | A | READ [A](save-block/range-a-head.md); MEASURED: `+0x13..+0x15` is `0A 07 FF` in the 10 of 80 saves made at a save point, zeros in the other 70 |
| `+0x014` | 1 | Field tail state | A | READ [A](save-block/range-a-head.md) |
| `+0x015` | 1 | Field tail argument: a message id, a master, a table index, by kind | A | READ [A](save-block/range-a-head.md) |
| `+0x016` | 2 | Field tail timer, u16; also the three-dial puzzle's answer | A | READ [A](save-block/range-a-head.md); MEASURED zero on the section 15 samples |
| `+0x018` | 2 | Word written 0 with the tail block, never read | P | READ [A](save-block/range-a-head.md) |
| `+0x01A` | 6 | Untouched | P | UNTOUCHED [A](save-block/range-a-head.md) |
| `+0x020` | 4 | The movement script's four counter bytes, copied at save and load: per-area and per-chapter scratch | A | READ [A](save-block/range-a-head.md) |
| `+0x024` | 2 | Area to resume in, u16. Also a new game's first area | E | READ [A](save-block/range-a-head.md); MEASURED: high byte zero on every sample |
| `+0x026` | 1 | No PC code reads or writes it. frost calls it an entry kind | P | UNTOUCHED [A](save-block/range-a-head.md); CITED frost |
| `+0x027` | 1 | Entry flags: facing in the low bits, bit 7 a drop-in | E | READ [A](save-block/range-a-head.md); values seen are 1..5, 7 and `0x80` |
| `+0x028` | 4 | Position x, s32 16.16 fixed | E | READ [A](save-block/range-a-head.md); crafted saves rely on it |
| `+0x02C` | 4 | Position z, s32 16.16 fixed | E | READ [A](save-block/range-a-head.md) |
| `+0x030` | 8 | Scratch shared by chapters: a saved party, chapter 10's eight offering counts, member ids | A | READ [A](save-block/range-a-head.md) |
| `+0x038` | 56 | Untouched | P | UNTOUCHED [A](save-block/range-a-head.md) |
| `+0x070` | 2 | Checksum | D | MEASURED |
| `+0x072` | 2 | Upper half of the dword the load reads as the checksum; never written, ignored | P | READ [A](save-block/range-a-head.md); MEASURED zero |
| `+0x074` | 4 | Untouched | P | UNTOUCHED [A](save-block/range-a-head.md) |
| `+0x078` | 1 | Option: message speed 0..2. Not the button mapping | A | READ [A](save-block/range-a-head.md) |
| `+0x079` | 1 | Option: sound, 0 stereo, 1 mono | A | READ [A](save-block/range-a-head.md) |
| `+0x07A` | 1 | Option: window colour 0..3. Equals its summary copy `+0xCA9` | A | READ [A](save-block/range-a-head.md); MEASURED equal to `+0xCA9` in 80 of 80 |
| `+0x07B` | 1 | Option: background 0..3. Equals its summary copy `+0xCAA` | A | READ [A](save-block/range-a-head.md); MEASURED equal to `+0xCAA` in 80 of 80 |
| `+0x07C` | 1 | Set 0 at boot; no option sets it and the PC original never reads it | A | READ [A](save-block/range-a-head.md) |
| `+0x07D` | 1 | Non-zero hides the battle command's label banner; no option sets it | A | READ [A](save-block/range-a-head.md) |
| `+0x07E` | 1 | Option: dash, 0 manual, 1 auto. Not the button mapping | A | READ [A](save-block/range-a-head.md) |
| `+0x07F` | 17 | Untouched | P | UNTOUCHED [A](save-block/range-a-head.md) |
| `+0x090` | 8 x `0xA4` | Character records, section 8 | | READ [B](save-block/range-b-characters.md) |
| `+0x5B0` | 160 | Chapter flag rows, 8 bytes for each of 20 chapters | A | READ: `Cond_Flags` `0x903F90` ([`story-flags.md`](story-flags.md)); 82 bytes live |
| `+0x650` | 32 | Event flags 0..255. Flag 146 is the summary's cleared byte `+0xCB4`. Red Herring's editor calls `+0x66E..+0x66F` the faerie village stage; engine loops write flags 245..255 there | A | READ: [`story-flags.md`](story-flags.md) section 9; 21 bytes live; CITED Red Herring for the stage |
| `+0x670` | 8 | Untouched | P | UNTOUCHED [C](save-block/range-c-party.md) |
| `+0x678` | 4 | Zenny, u32, at most 9,999,999 | E | READ [C](save-block/range-c-party.md); MEASURED |
| `+0x67C` | 2 | Area 189's walk: frame word, u16 | A | READ [C](save-block/range-c-party.md) |
| `+0x67E` | 1 | Area 189's walk: walk count | A | READ [C](save-block/range-c-party.md) |
| `+0x67F` | 1 | Area 189's walk: reserve | A | READ [C](save-block/range-c-party.md) |
| `+0x680` | 1 | Active formation, a cell 0..5. Must be unlocked in `+0x681` and fit the party size | E | READ [C](save-block/range-c-party.md); MEASURED unlocked in 80 of 80 |
| `+0x681` | 1 | Unlocked formations: bit n opens cell n of the formation grid (`TacticsFormation_Build`). Bits 0..2 are set at a new game; bits 3, 4, 5 are set once each by masters 0xB, 0xD, 0xE at 3 levels gained (`MasterGrant_Step1Member`) | E | READ, MEASURED (`0x07`, `0x1F`, `0x3F` on the samples) |
| `+0x682` | 3 | Field order, character ids, `0xFF` at the end. The same ids as `+0x685`, each with its record's joined bit; `+0x74C` and the summary portraits follow it | E | READ [C](save-block/range-c-party.md); MEASURED |
| `+0x685` | 3 | Battle order, the same ids | E | READ [C](save-block/range-c-party.md); MEASURED: a permutation of field order, 75 of 75 |
| `+0x688` | 32 | Enemy kinds beaten, 256 bits | A | READ [C](save-block/range-c-party.md) |
| `+0x6A8` | 32 | Abilities learnt, 256 bits. Setting a bit adds no ability to a list | A | READ [C](save-block/range-c-party.md) |
| `+0x6C8` | 32 | Enemy kinds identified, 256 bits | A | READ [C](save-block/range-c-party.md) |
| `+0x6E8` | 4 | Play clock: h, m, s, frames. Never all zero (that reads as a new game); equals the summary's `+0xCAC` | E | READ [C](save-block/range-c-party.md); MEASURED |
| `+0x6EC` | 32 | Chest and field-object content flags | A | READ and MEASURED: [`story-flags.md`](story-flags.md) section 1 |
| `+0x70C` | 32 | Largest catch for each kind of fish, 0 never caught; 23 used | E | READ [C](save-block/range-c-party.md) |
| `+0x72C` | 32 | Passage find flags | A | READ [C](save-block/range-c-party.md) |
| `+0x74C` | 1 | Party combination row for the field order's ids, bit 7 set by a load, `0xFF` none. The load loads the party's files by it, so it must match `+0x682` | A | READ [C](save-block/range-c-party.md); MEASURED: bit 7 set in 79 of 80, one save holds `0x02` |
| `+0x74D` | 1 | `Field_ScriptFlags`' low byte, copied at save and back at load. Bit `0x40` goes with the field tail `+0x13..+0x17` | A | READ [C](save-block/range-c-party.md) |
| `+0x74E` | 1 | Fishing lure, an item id held outside the inventory | E | READ [C](save-block/range-c-party.md) |
| `+0x74F` | 1 | Fishing lure count: 1 with a lure, else 0 | E | READ [C](save-block/range-c-party.md) |
| `+0x750` | 1 | Fishing rod, an item id held outside the inventory | E | READ [C](save-block/range-c-party.md) |
| `+0x751` | 1 | Music track to restore after a load, `0xFF` none | A | READ [C](save-block/range-c-party.md) |
| `+0x752` | 2 | Untouched | P | UNTOUCHED [C](save-block/range-c-party.md) |
| `+0x754` | 4 | Battle count. Every faerie stamp stays at or below it | A | READ [C](save-block/range-c-party.md); MEASURED, section 12.1 |
| `+0x758` | 4 | Zenny tally, u32: not every gain is counted | A | READ [C](save-block/range-c-party.md) |
| `+0x75C` | 4 | Passage finds taken, u32 | A | READ [C](save-block/range-c-party.md) |
| `+0x760` | 4 | Chest contents taken, u32 | A | READ [C](save-block/range-c-party.md) |
| `+0x764` | 4 | Fishing sessions entered, u32 | A | READ [C](save-block/range-c-party.md) |
| `+0x768` | 4 | Return trip: point x, 16.16 | A | READ [C](save-block/range-c-party.md) |
| `+0x76C` | 4 | Return trip: point z, 16.16 | A | READ [C](save-block/range-c-party.md) |
| `+0x770` | 2 | Return trip: area, u16 | A | READ [C](save-block/range-c-party.md) |
| `+0x772` | 1 | Return trip: non-zero while away. Write 0 for a save not on a trip | A | READ [C](save-block/range-c-party.md) |
| `+0x773` | 1 | Return trip: facing on arrival in area 189 | A | READ [C](save-block/range-c-party.md) |
| `+0x774` | 512 | Item ids, 4 categories x 128 at `+0x774`, `+0x7F4`, `+0x874`, `+0x8F4`; 0 empty. One stack per id | E | READ [D](save-block/range-d-skills.md); MEASURED by the sibling's `save_tool.py verify` against disc tables |
| `+0x974` | 512 | Item counts 1..99, parallel to the ids | E | as above |
| `+0xB74` | 32 | Key items, ids only. Do not fill all 32: a 33rd lands in `+0xB94` | E | as above |
| `+0xB94` | 128 | The shared ability list: 128 ability ids, gaps allowed. Keep an id here or in a member's list, not both | E | READ [D](save-block/range-d-skills.md) |
| `+0xC14` | 1 | A rank 0..12 that area 113 reads for its reward | A | READ [D](save-block/range-d-skills.md) |
| `+0xC15` | 5 | Untouched | P | UNTOUCHED [D](save-block/range-d-skills.md) |
| `+0xC1A` | 1 | World-map display mode 0..2 | A | READ [D](save-block/range-d-skills.md) |
| `+0xC1B` | 1 | Battles since master 4 was met, to 30 | A | READ [D](save-block/range-d-skills.md) |
| `+0xC1C` | 9 | Battle item-window memory, page, scroll and cursor for each battle-party slot; follows the battle order | A | READ [D](save-block/range-d-skills.md) |
| `+0xC25` | 3 | The party item list's page 0..3, top row and cursor | A | READ [D](save-block/range-d-skills.md) |
| `+0xC28` | 24 | Six recent gene combinations, newest first, 4 bytes each: up to three gene numbers then `0xFF`, and a form byte. An empty row is four `0xFF`, never 0 | E | READ [D](save-block/range-d-skills.md); MEASURED with `+0xC40`: no row breaks the rule, 282 rows in use |
| `+0xC40` | 48 | Twelve stored gene combinations, the same rows | E | READ [D](save-block/range-d-skills.md) |
| `+0xC70` | 3 | Dragon genes, 18 bits | E | READ [D](save-block/range-d-skills.md) |
| `+0xC73` | 1 | High byte of the gene dword: never written, must stay 0 | P | READ [D](save-block/range-d-skills.md) |
| `+0xC74` | 3 | Masters met, 17 bits: set when a master's introduction opens (`MasterTalk_IntroSay`) | E | READ, MEASURED |
| `+0xC77` | 3 | Masters open, 17 bits: set when the introduction closes (`MasterTalk_IntroWait`); the masters window lists these. **Not a copy of `+0xC74`**: bit 9 is set later by a scenario step, and three samples have it met but not open. Bit 4 met and not open counts battles at `+0xC1B` up to 30 | E | READ, MEASURED |
| `+0xC7A` | 2 | Untouched | P | UNTOUCHED [D](save-block/range-d-skills.md) |
| `+0xC7C` | 4 | Countdown shaped like the play clock; while it runs ability `0x3E` is refused. Set after the effect the source calls `Celerity_End` | E | READ [D](save-block/range-d-skills.md) |
| `+0xC80` | 4 | The same kind of countdown for ability `0x15`, set by `Effect_DoubleDamage` | E | READ [D](save-block/range-d-skills.md); CITED the name Bonebreak |
| `+0xC84` | 28 | Untouched | P | UNTOUCHED [D](save-block/range-d-skills.md) |
| `+0xCA0` | 28 | Slot summary, below | D | READ [E](save-block/range-e-tail.md); MEASURED |
| `+0xCBC` | 18 | Nine button words, u16: a copy of the live mapping at save, copied back at load. Must stay a valid mapping: with DIV-0133 on, our build stops at the load on a bad one | A | READ [E](save-block/range-e-tail.md) |
| `+0xCCE` | 2 | Untouched | P | UNTOUCHED [E](save-block/range-e-tail.md) |
| `+0xCD0` | 4 | Faerie village: battles counted toward its next story flag | A | READ [E](save-block/range-e-tail.md) |
| `+0xCD4` | 4 | Faerie village: battle-count stamp of the last growth. At or below `+0x754` | A | READ [E](save-block/range-e-tail.md); MEASURED: no stamp above the count |
| `+0xCD8` | 4 | Faerie village: stamp of the last shrink. At or below `+0x754` | A | as above |
| `+0xCDC` | 4 | Faerie village: stamp of the last mood update. At or below `+0x754` | A | as above |
| `+0xCE0` | 4 | Faerie village: stamp of the lit level. At or below `+0x754` | A | as above |
| `+0xCE4` | 4 | Faerie village: stamp of the dark level. At or below `+0x754` | A | as above |
| `+0xCE8` | 2 | Faerie village: the highest area number its queue has seen, u16 | A | READ [E](save-block/range-e-tail.md) |
| `+0xCEA` | 1 | Faerie village: mood 0..99 | E | READ [E](save-block/range-e-tail.md); CITED Red Herring's name "food" |
| `+0xCEB` | 1 | Faerie village: dark level, 1..10 once started | E | READ [E](save-block/range-e-tail.md); CITED "max jobs" |
| `+0xCEC` | 1 | Faerie village: lit level 0..7 | E | READ [E](save-block/range-e-tail.md); CITED "culture" |
| `+0xCED` | 1 | Faerie village: a bias rebuilt from the records at each entry | D | READ [E](save-block/range-e-tail.md); CITED "clear power" |
| `+0xCEE` | 1 | Faerie village: the second bias, rebuilt the same way | D | READ [E](save-block/range-e-tail.md); CITED "build power" |
| `+0xCEF` | 1 | Area 185's exit-square arm byte, transient. Write 0 | A | READ [E](save-block/range-e-tail.md) |
| `+0xCF0` | 480 | Faerie records, 60 x 8: in use, place, state, a status byte, a battle-count stamp at or below `+0x754` | E | READ [E](save-block/range-e-tail.md) and [`save-interchange.md`](save-interchange.md) section 2a |
| `+0xED0` | 64 | Faerie buildings, 8 x 8: kind, two rows, level, a battle-count stamp at or below `+0x754` | E | READ [E](save-block/range-e-tail.md) |
| `+0xF10` | 300 | Faerie names, 60 x 5, by record | E | READ [E](save-block/range-e-tail.md) |
| `+0x103C` | 4 | Untouched | P | UNTOUCHED [E](save-block/range-e-tail.md) |
| `+0x1040` | 1 | **This project's own, not Capcom's:** the mountain-passes bits of DIV-0191, bit 0 only. The original ignores it | A | READ [E](save-block/range-e-tail.md); MEASURED 0 or 1, 1 in 9 saves |
| `+0x1041` | 111 | Untouched | P | UNTOUCHED [E](save-block/range-e-tail.md) |

**Coverage:** of the 4,272 bytes, 3,810 have a writer or a reader in our source
and 462 have none (READ, [`save-block.md`](save-block.md) section 1). Those 462
are 0 in all 80 PC saves measured (MEASURED, 2026-11-10). The existing editor
writes 2,931 of the 4,272 bytes (MEASURED, 75 distinct blocks); of the other
1,341, 527 are non-zero in at least one save and 390 vary between saves. Every
byte the editor does not know must survive a write.

### Slot summary at `+0xCA0`

Derived. Regenerate it on write unless the player overrides it on Advanced.

| Offset | Size | Field | Rule | Evidence |
|---|---:|---|---|---|
| `+0xCA0` | 5 | Name, bytes 0..4 | Record 0's name (DIV-0076, our default); the field leader's in Capcom's code | READ [E](save-block/range-e-tail.md); MEASURED: the 9-byte name is record 0's in 77 of 80, the field leader's in 67 |
| `+0xCA5` | 3 | Portraits | Field order `+0x682`, id 4 written as `0x0B` unless `+0x74C & 0x7F` is 5 or `0xC` | READ [E](save-block/range-e-tail.md); MEASURED 74 of 75 |
| `+0xCA8` | 1 | Level | Record 0's level | READ [E](save-block/range-e-tail.md); MEASURED 75 of 75 |
| `+0xCA9` | 1 | Window colour | Equals `+0x7A` | READ [E](save-block/range-e-tail.md); MEASURED 80 of 80 |
| `+0xCAA` | 1 | Background | Equals `+0x7B` | READ [E](save-block/range-e-tail.md); MEASURED 80 of 80 |
| `+0xCAB` | 1 | Padding | Preserve | READ [E](save-block/range-e-tail.md) |
| `+0xCAC` | 4 | Play clock | Equals `+0x6E8` | READ [E](save-block/range-e-tail.md); MEASURED 76 of 76 h:m:s |
| `+0xCB0` | 4 | EXP | Record 0's EXP | READ [E](save-block/range-e-tail.md); MEASURED 75 of 75 |
| `+0xCB4` | 1 | Cleared game | Event flag 146 | MEASURED: 1 in the cleared save, 0 in the 18 others of the play folder; READ in `Save_BuildBlock` |
| `+0xCB5` | 1 | The slot's present flag once a save copies the summary; nothing writes it in the block | Write 0, never `0xFF`: that marks the slot empty | READ [E](save-block/range-e-tail.md) |
| `+0xCB6` | 4 | Name, bytes 5..8 | From the same record as bytes 0..4 | READ [E](save-block/range-e-tail.md) |
| `+0xCBA` | 2 | Padding | Preserve | READ [E](save-block/range-e-tail.md) |

The name follows record 0 only with DIV-0076 on; Capcom's code takes the field
leader's name, but record 0's level and EXP ([E](save-block/range-e-tail.md)
section 4).

## 8. Character records

Eight records, stride `0xA4`. **Both layouts below.** The PC name is 9 bytes, and
every later field is 4 bytes further in: PC = PSX + 4 from PSX `+0x05` on. The
stride is unchanged, so the PC gives up the PSX's last four bytes, which are zero
in every sample of both kinds (MEASURED).

Record positions mostly hold fixed characters. MEASURED `char_id` by position: 0
is {0, 7, 9}, 1 is {1, 8}, positions 2 to 6 are their own number, 7 is 10. The
EXP curve follows the position, not the `char_id`, and record 7 is also reused
for guests ([B](save-block/range-b-characters.md) section 4).

The game rebuilds PC `+0x20..+0x3F` from `+0x40..+0x5F`, the equipment and the
master for all eight records on every load, and cuts HP to the new maximum; AP is
not cut ([B](save-block/range-b-characters.md) section 1.1).

| PSX | PC | Size | Field | Class | Evidence |
|---|---|---:|---|---|---|
| `+0x00` | `+0x00` | 5 (PC: 9) | Name. On the PC byte 8 is 0. Record 0's feeds the slot summary | E | MEASURED; READ [B](save-block/range-b-characters.md) |
| `+0x05` | `+0x09` | 1 | `char_id`, the character the record holds. Change it only to a value the game puts at that position | A | MEASURED; READ [B](save-block/range-b-characters.md) |
| `+0x06` | `+0x0A` | 1 | Level 1..99. Goes with the EXP | E | MEASURED; READ [B](save-block/range-b-characters.md) |
| `+0x07` | `+0x0B` | 1 | Flags: bit 0 joined (needed by every id in the party lists), bit 1 fixed in the party screen, bit 2 cleared after a fight left KO | A | READ [B](save-block/range-b-characters.md); MEASURED values 0, 1, 3 |
| `+0x08` | `+0x0C` | 4 | EXP, s32, at most 9,999,999 | E | MEASURED; READ [B](save-block/range-b-characters.md) |
| `+0x0C` | `+0x10` | 2 | Status bits, u16. `0x4000` (KO) goes with HP 0; `0x2000` is rewritten on the next status draw | A | READ [B](save-block/range-b-characters.md) |
| `+0x0E` | `+0x12` | 1 | Weapon id, 0 none | E | MEASURED by the sibling; READ [B](save-block/range-b-characters.md) |
| `+0x0F` | `+0x13` | 3 | Three armour ids. The sibling calls them shield, helm, armor; the code does not name them | E | MEASURED by the sibling; READ [B](save-block/range-b-characters.md) |
| `+0x12` | `+0x16` | 2 | Two accessory ids. An item worn is not also in the inventory count | E | MEASURED by the sibling; READ [B](save-block/range-b-characters.md) |
| `+0x14` | `+0x18` | 2 | Current HP, u16. Cut to max HP on load | E | MEASURED: HP <= max on all; READ [B](save-block/range-b-characters.md) |
| `+0x16` | `+0x1A` | 2 | Current AP, u16. Not cut on load: keep it at or below max AP | E | READ [B](save-block/range-b-characters.md) |
| `+0x18` | `+0x1C` | 1 | Current willpower. Reset at the inn; keep it at or below the effective willpower | E | READ [B](save-block/range-b-characters.md) |
| `+0x19` | `+0x1D` | 1 | Halving bits: each set bit raises one resistance class by 2. Not fatigue | A | READ [B](save-block/range-b-characters.md); MEASURED 0 in 640 records |
| `+0x1A` | `+0x1E` | 1 | Fatigue: the max-HP scale 0..9. Up by one when a member ends a fight KO, cleared at the inn | E | READ [B](save-block/range-b-characters.md); MEASURED values 0, 1, 2, 5, 9 |
| `+0x1B` | `+0x1F` | 1 | Master, `0xFF` none. Changes together with the apprenticing level and the growth bias | E | READ [B](save-block/range-b-characters.md) |
| `+0x1C` | `+0x20` | 2 | Max HP, effective | D | READ [B](save-block/range-b-characters.md): rebuilt on load |
| `+0x1E` | `+0x22` | 2 | Max AP, effective | D | as above |
| `+0x20` | `+0x24` | 8 | Power, defence, agility, intelligence, effective, u16 | D | as above |
| `+0x28` | `+0x2C` | 2 | A word, the effective copy of PC `+0x4C`; no reader found | D | as above |
| `+0x2A` | `+0x2E` | 1 | Willpower, effective | D | as above |
| `+0x2B` | `+0x2F` | 5 | Element classes, effective: fire, ice, lightning, earth, wind by the code's comment | D | as above |
| `+0x30` | `+0x34` | 1 | Holy class, effective | D | as above |
| `+0x31` | `+0x35` | 3 | Three status classes, effective | D | as above |
| `+0x34` | `+0x38` | 5 | Surprise, reprisal, critical, dodge, accuracy, effective, in % | D | as above; CITED the name accuracy |
| `+0x39` | `+0x3D` | 3 | Untouched but for the rebuild's copy from PC `+0x5D` | P | UNTOUCHED [B](save-block/range-b-characters.md) |
| `+0x3C` | `+0x40` | 2 | Base max HP, u16, at most 999 | E | READ [B](save-block/range-b-characters.md) |
| `+0x3E` | `+0x42` | 2 | Base max AP, u16 | E | READ [B](save-block/range-b-characters.md) |
| `+0x40` | `+0x44` | 8 | Base power, defence, agility, intelligence, u16, at most 999 | E | MEASURED: top bit clear in all 608 records; READ [B](save-block/range-b-characters.md) |
| `+0x48` | `+0x4C` | 2 | Base of the PC `+0x2C` word, set only by the default records; meaning not known | P | READ [B](save-block/range-b-characters.md), shape only |
| `+0x4A` | `+0x4E` | 1 | Base willpower 0..99, made from the level at a new game | E | READ [B](save-block/range-b-characters.md) |
| `+0x4B` | `+0x4F` | 9 | Base resistances: five elements, holy, three status classes; 0..6 or 7 | E | READ [B](save-block/range-b-characters.md) |
| `+0x54` | `+0x58` | 5 | Base surprise, reprisal, critical, dodge, accuracy, 0..100 | E | READ [B](save-block/range-b-characters.md); CITED the name accuracy |
| `+0x59` | `+0x5D` | 3 | Untouched but for the rebuild's copy | P | UNTOUCHED [B](save-block/range-b-characters.md); MEASURED zero |
| `+0x5C` | `+0x60` | 10 | Ability list, type 0 (the sibling's heal): ids packed from the front | E | MEASURED by the sibling; READ [B](save-block/range-b-characters.md) |
| `+0x66` | `+0x6A` | 10 | Ability list, type 1 (assist) | E | as above |
| `+0x70` | `+0x74` | 10 | Ability list, type 2 (attack) | E | as above |
| `+0x7A` | `+0x7E` | 10 | Ability list, type 3 (skill), the one the masters check. Record 7's first byte is a minigame's rank, not an ability | E | as above; MEASURED for record 7's first byte: 5 in 17 saves ([`save-block.md`](save-block.md) section 3) |
| `+0x84` | `+0x88` | 1 | Level at apprenticing, 0 with no master; not above the level | E | READ [B](save-block/range-b-characters.md) |
| `+0x85` | `+0x89` | 6 | Growth bias, **signed** bytes, for base max HP, max AP, power, defence, agility, intelligence: the master's six bytes, 0 with no master | E | MEASURED: 680 bytes have the top bit set; READ [B](save-block/range-b-characters.md) |
| `+0x8B` | `+0x8F` | 21 | Untouched | P | UNTOUCHED [B](save-block/range-b-characters.md); highest live offset is PSX `+0x8A` ([`save-interchange.md`](save-interchange.md)) |
| `+0xA0` | - | 4 | PSX only, no PC home: dropped in conversion to PC | P | MEASURED zero |

**Signedness is per field and lives in the field table.** Read and write must use
the same entry.

## 9. Text

- A PC name is **glyph codes for the install's font**, so its meaning depends on
  language. Switching language scrambles stored names (known and accepted,
  [`dialogue-localisation.md`](dialogue-localisation.md)). MEASURED: PC samples
  hold both high-byte and low-byte codes, and byte 8 is always zero.
- Each release has its own encoding, and the editor needs one codec per target.
  A name that cannot be encoded is refused, not truncated silently.
- **The default names are not ours to ship.** They come from the default records
  of the install (section 10).
- Faerie names are 5 bytes on every target, space-padded, in the source release's
  encoding.
- The port's item names are ids in the save, so they convert freely
  ([`DIVERGENCE.md`](DIVERGENCE.md)'s layer note).

## 10. What is read from the player's install

Nothing below is copied into the repo or the launcher binary.

| Need | Source | State |
|---|---|---|
| Default character records | `Char_DefaultRecords` `0x64B390`, 7 x `0xA4`; whelp record `0x64B80C`; `Char_WhelpSlot` | READ. The launcher already reads exe bytes by address ([`exe_image.h`](../src/launcher/exe_image.h)). A disc-built install has them in `base/exe/data.bin` ([`exe-import.md`](exe-import.md)) |
| The derived new-game byte per character | `NewGame_InitCharacters` `0x437820`, level and a divisor table | READ |
| Item, ability and area names | `Item_NamePtr` and the install's text | Open: section 14 |
| Icons for pages | Install's graphics | Not located. Text-only is acceptable for a first version |
| PSX card icons | `ETC/START.EMI` entry 9 on a disc install | CITED frost. Optional, section 5.2 |

If a table is not available in an install, the feature that needs it is greyed out
with the reason, the way the launcher's source-dependent options are.

## 11. Conversion

Conversion is a pure function: block and source target in, block and destination
target out. The rules are those proved by
[`tools/save_convert.py`](../tools/save_convert.py), and that tool stays as the
reference to test against.

1. Reject a source that fails its checksum, unless the player chose "Open anyway".
2. PSX to PC: name field 5 to 9, body 4 bytes later, last four bytes dropped.
   Abort if those four are non-zero. PC to PSX: the reverse, zero-filling the tail.
3. Names: the destination's default for that record, or text the player types.
4. Summary: regenerated (section 7).
5. Faerie names: copied as they are, with a warning if any are in use, since they
   stay in the source encoding.
6. Control scheme: the button words at `+0xCBC` and the option bytes travel with
   the save. The player chooses
   "keep" or "use this target's default".
7. Seal: checksum last.

Before writing, the page lists what will change: names replaced, faerie names at
risk, and the control scheme.

**State of proof:** PSX to PC has loaded and played on the port. PC to PSX has only
been checked statically ([`save-interchange.md`](save-interchange.md) section 5).
The Mednafen load of a PC-sourced card is an owed test before this path ships.

## 12. Creation from nothing

A generator, not a template. No binary save is shipped.

### 12.1 The new-game block

1. Start from a zero block.
2. Copy the default records from the install (section 10), applying the derived
   byte, then the whelp record.
3. Write the target's layout, names and options.
4. Scene state at the new-game chapter. That chapter's Start sets the area.
5. Fill `+0xC28..+0xC6F` with `0xFF`, as the first chapter's start does (below).
6. Seed event flags 0..127 from `Scenario_SubInitTable` (`0x662CD8`, 16 bytes),
   read from the install like the default records, and write block `+0x12` as 1
   ([`story-flags.md`](story-flags.md) section 9).
7. Write the few remaining start-up fields (below), then seal.

The test is a byte comparison against a save the game wrote at the first
opportunity on a new game, per target. The references are held (below).

On PC the new-game chapter is 0, read (`Scenario_Start(0)` in `TitleFlow_NewGame`,
`save_menu.cpp`) and now measured. frost cites `Scene_Start(16)` on the PSX, so
chapter numbers are not assumed to be the same there, or to be story order.

#### The PC reference saves (owner, 2026-11-09)

Two saves in `tools/recipe_saves/`, git-ignored like the rest:

| Save | Made | State |
|---|---|---|
| `start.DAT` | F12 one frame into a new game. The game would never save here | Chapter 0, area `0x18`, the whelp alone, clock at frame 1. 558 non-zero bytes |
| `start_normal.DAT` | The first save point, 9:49 in, after the whelp section | Chapter 1, area `0x0A`, record 0 leading. 572 non-zero bytes |

MEASURED on `start.DAT`:

- **Records are the install's defaults.** Records 0..6 equal `Char_DefaultRecords`,
  and record 7 the whelp record, apart from: the name (the install's language
  layer, DIV-0020), the derived byte at PC `+0x1C`, `+0x2E` and `+0x4E`, and three
  recalculated live stats at PC `+0x24`, `+0x26`, `+0x28`. The whelp record also
  differs at PC `+0x0B` and `+0x38`.
- **The patterns near `+0xC28` are simple.** `+0xC28..+0xC6F` is the empty rows
  of the two gene-combination lists, 6 rows at `+0xC28` and 12 at `+0xC40`
  ([D](save-block/range-d-skills.md)), not eight rows and 40 copies. Both are
  already there at frame 1 and unchanged in the second save. The block is in the image's
  uninitialised part (it sits at `0x9039E0`; the section's file data ends at
  `0x676000`), so code writes them at start-up. READ: `Scena00_Start` writes -1 to
  the 18 dwords `0x904608..0x90464F`, which is exactly this span, and the bytes
  are all `0xFF` ([`story-flags/chapter-00.md`](story-flags/chapter-00.md)).
- **Three flags are set at frame 1:** bit 3 of chapter 0's flag word, and event
  flags 3, 25 and 27.
- **Other start-up fields, to trace one by one:** the nine button masks, the
  option byte `+0x78`, formation byte `+0x681`, and `+0x74C`, `+0x74D`, `+0x751`.

MEASURED between the two saves (36 bytes differ, checksum aside):

- **The flag words are indexed by chapter.** Chapter 0's word gains bits 0, 1, 2,
  4..7 and 10. Chapter 1's word, at `+0x5B8`, gains bits 0 and 1. No event flag
  changes.
- **The item picked up** takes slot 0 of inventory category 1: one id byte at
  `+0x7F4` and one count byte at `+0x9F4`.
- **The battle count** at `+0x754` goes from 0 to 3. The owner fought three
  battles in that section, the boss included, so the field counts battles fought.
- **The lead changes** from id 10 to id 9 in both orders, and record 0's
  `char_id` becomes 9.
- Also changed, meaning not established: `+0x20`, `+0x68B`, `+0x6EC`, `+0x760`,
  the three bytes near `+0x74C`, and scene bytes 0, 1, 4, 5 and 8.

So the opening section's whole effect on a save is 36 bytes. That is the size of
thing a chapter-start baseline row has to hold.

#### The same point on the US and JP discs

RetroArch's earliest saves are at the same save point: US at 0:12 and JP at 0:25,
both chapter 1, area `0x0A`. Against `start_normal.DAT`, with the PC records put
in the PSX layout (MEASURED):

- **US: 20 bytes differ.** The clock and its summary copy, the checksum, two option
  bytes (`+0x78`, `+0x7E`), six button-mask bytes, the battle count (5 against 3),
  one HP byte, scene byte 8, `+0x68B`, and two more bits of chapter 0's flag word
  (8 and 9).
- **The character names are the same bytes.** An English PC install stores the
  names the US disc does, so on that install names convert without a table.
- **JP: 65 bytes differ.** The same kinds of difference, plus the names in its own
  encoding and a few more items picked up.
- **Scene bytes 0..5 and the event flags are identical on all three.** So the
  chapter byte and the flags mean the same thing on PC, US and JP at this point.
  The first evidence that chapter baselines measured on one target carry to the
  others.

Bits 8 and 9 of chapter 0's word are set on both disc saves and not on the PC one,
and those saves count two more battles. READ in our source, `Scena00_Run6`
(`src/game/scena_sc0.cpp`, original `0x538F90`): the run sets flag 7, 8 or 9 as it
starts event battle 4, 5 or 6, and `Scena00_Area18` starts the run for the second
and third only while flag 8 or 9 is clear. [`boss-fights.md`](boss-fights.md) lists
chapter 0's event battles as 1, 4, 5, 6 and 7, which is the owner's five. So the PC
save fought battles 1, 4 and 7, and the disc saves all five. Each flag is set when
its encounter starts, not when the battle is won.

**This is the method for the flag documentation**, now in
[`story-flags.md`](story-flags.md), with a map of every chapter. Its section 2
gives how a chapter starts and ends, which is what a story-point row must
reproduce. A chapter's code names its flags
outright, as `Flags_Set(Row(), n)` and `Flags_Test(Row(), n)`, so each flag's
meaning can be read from the function around it, and the saves confirm the reading.

**A third save at the same point** (owner, 2026-11-09): the English psxrecomp card,
`../BreathOfFire3EnglishRecomp/saves/card1.mcd`, 0:11 in, three battles. It is the
same raw card format, and its header frame and unused frames are byte-identical to
RetroArch's. Against `start_normal.DAT` 19 bytes differ, every one accounted for:

| Bytes | Reading |
|---|---|
| `+0x6E9..+0x6EB`, `+0xCAD..+0xCAF`, `+0x70` | Clock, its summary copy, checksum |
| `+0x78`, `+0x7E`, six button-mask bytes from `+0xCBC` | Message speed and dash (`+0x78`, `+0x7E`), and the button mapping (`+0xCBC`). Equal on both US saves, different on PC |
| `+0x7B`, `+0xCAA` | Probably the window background and its summary copy: the owner chose a different background on this save, and both bytes differ from PC and from RetroArch's US save |
| Record 7 `+0x14` | One HP byte |
| `+0x08` | Scene byte 8. Differs on every save, so a counter or timer |

Its flags equal the PC save's exactly. Against RetroArch's US save, which fought
all five battles, it differs in the battle count, chapter 0's flags 8 and 9, and
`+0x68B`, so `+0x68B` also follows the optional battles.

These serve as the US and JP references for the first save point. A frame-1
save like `start.DAT` has no disc equivalent, since the PSX cannot save there.

### 12.2 Story points

A story point is a row in `recipes/save-story-points.toml`:

| Column | Content |
|---|---|
| `id`, `order`, `label` | Stable name, story order, the words the player sees |
| `chapter`, `state`, `scenario_bytes` | Block `+0x00..+0x0F` |
| `area`, `x`, `z`, `facing` | Where the party stands, omitted when the chapter's Start places it |
| `flags_set` | Scene word and event flag numbers set at this point |
| `party` | Field and battle order, formations |
| `typical` | Optional. Levels, equipment, key items, faerie state, zenny |
| `tested` | `pc`, `psx-us`, `psx-jp`, each with a date |

Three tiers, shipped in this order:

- **Chapter starts, first.** Each chapter gets a baseline: the flags set on entering
  it in a normal playthrough, the "default flags at chapter x". The chapter's Start
  places the party, as reached on PC for chapter 17
  ([`ending-backdrop.md`](ending-backdrop.md), recipe `ending17`). Chapter values
  0..15 and 17 appear in the samples.
- **Story events inside a chapter.** These need `flags_set`, stored as differences
  from the chapter's baseline. Captured from real
  playthroughs: the harness runs a recorded route and logs flag bytes at each
  point, and the existing recipe saves give a head start.
- **Free choice.** Any row, plus the Advanced page for anything else.

Baselines come from saves a real game wrote at or near a chapter's entry. A crafted
chapter-jump save does not qualify, because it changes only the scene bytes and
keeps its parent's flags. Where several real saves fall in one chapter, the flags
set in all of them show what is fixed by that point.

Rules:

- A row ships only when `tested` has an entry. Untested rows are hidden unless the
  player turns on an "untested" switch.
- **On the PSX, `scenario_bytes` must be real.** Zeroing `+0x08` turns off the
  walk's height test until the next link transition (measured on Mednafen,
  2026-10-26). A row therefore stores the captured bytes, not zeros.
- "Typical progress" is optional. Without it the party gets the defaults.
- Picking a point warns about, but does not refuse, a party that does not fit it.

The table is reference material (section 1): flag numbers, positions and labels in
text. It never embeds a save or a dump of one. Flag meanings are written up as they
are found, with their evidence, in a form that can also go to the wiki.

## 13. Behaviour

**Never overwrite without a backup.** Every write goes to a temporary file, is read
back and verified, then replaces the target, with the previous file kept beside
it. New saves default to a free slot. For a card the same applies to the whole
image.

**Not while the game runs.** The game reads and writes these files, so the page is
available only before launch. The launcher already returns while the game runs, so
this needs a real check for `BOF3.exe`.

**Errors are said out loud.** A file that cannot be read gets a message naming the
reason: wrong size, no `MC` header, gzip save state, no BoF3 saves.

**Warnings, not walls.** Odd values are allowed. Values that would not fit the
field's width are refused at entry.

**Player-facing text is ASD-STE100** (`CLAUDE.md`), and the page's own labels are
ours. Item names and other game text keep their voice.

**Layout.** One new page in the launcher's window, following
[`launcher-settings.md`](launcher-settings.md) section 2a. Sections: Summary,
Characters, Inventory, Party, Position and Story, Faerie Village, Fishing and
Counters, Advanced. Advanced is off by default and holds flags, the A-class fields
and a hex view of the P-class bytes.

**Ledger.** The page changes no game code, but it is a player-facing launcher
feature, so it gets a [`DIVERGENCE.md`](DIVERGENCE.md) entry when it ships, as
the suites did (DIV-0107).

### Checks on write

From the byte map ([`save-block.md`](save-block.md) section 5 and the range files'
third sections) and the review of 2026-11-10. "Warnings, not walls" still holds:
only a value the game reads with no bound is refused.

**Set by the editor, not asked (D-class and copies):**

- The checksum, and the slot summary from record 0 (name in 5 + 4 bytes, level,
  EXP), the clock copy, the two option copies `+0xCA9`, `+0xCAA`, the cleared byte.
  `+0xCB5` is written 0.
- The party-combination byte `+0x74C`, from the field order. The table that maps
  three ids to a row is read from the player's install.
- The battle order, kept a permutation of the field order.
- Record `+0x20..+0x3F` (PC) is left alone: the game rebuilds it at load.
- A change to the battle count `+0x754` moves every faerie stamp by the same
  amount: `+0xCD4..+0xCE7`, each faerie record's `+4`, each building's `+4`.
- An item that is equipped is taken out of the inventory count, and put back when
  it is taken off.
- On a write to a PlayStation card: `+0x1040` (ours, DIV-0191) is 0, and the
  options `+0x7C`, `+0x7D`, which no PC screen sets, are 0 unless the source was a
  PlayStation save.

**Refused, with a message that names the field:**

- A chapter outside 0..19, or 16.
- Message speed `+0x78` outside 0..2; window colour `+0x7A` and background `+0x7B`
  outside 0..3. The game uses them as indexes with no bound.
- Button words that are not one button for each line. Our build loads such a
  save with the reset buttons (DIV-0133, amended 2026-11-10); the original and the
  PlayStation load them as they are.
- A play clock of all zero in a save that is not a new game: the game reads it as
  one.
- An active formation that is not unlocked, or not allowed for the party size.
- A gene combination row with a gap, or with a first byte of `0xFF` and a form
  byte that is not `0xFF`. A non-zero top byte of the gene dword.
- Bit 7 of the last byte of either find bank (`+0x70B`, `+0x74B`): it makes every
  object or passage that has no flag read as taken.
- A faerie stamp above the battle count.

**Warned, and written if the player says so:**

- EXP past the stored level: the next level-up grants every level between.
- A level at joining a master above the level; a master byte, a level at joining
  and six growth bytes that do not go together.
- AP above its maximum. Fatigue above 9. Current willpower above the effective one.
- A member in the party lists without the joined bit, or the reverse.
- The mode tail `+0x13..+0x17` cleared without script flag `0x40` in `+0x74D`, or
  the reverse.
- A character id that the game does not put at that record position.
- Record 7's `+0x7E` is shown as a minigame's prize rank, not as an ability.

### Code

```
src/launcher/save/
  block.h/.cpp        the block, checksum, field table
  layout.h/.cpp       PSX and PC record layouts
  container.h/.cpp    PC file, raw card, directory entries
  convert.h/.cpp      section 11
  create.h/.cpp       section 12
  text.h/.cpp         per-target name codecs
src/launcher/save_dialog.cpp   the page
```

The first group has no Win32 in it, and can run headless.

The field table is the single source of offset, width, signedness, class and
evidence. Both read and write go through it, which is what prevents a saver and a
loader from disagreeing. The Advanced page is generated from it.

### Tests

Fixtures are local saves and are never committed.

| Test | Passes when |
|---|---|
| Round trip | Open then write changes zero bytes on every fixture, checksum included |
| One edit | Setting one field changes only that field, the checksum and any derived field |
| Conversion | Matches `save_convert.py` byte for byte, and PSX to PC to PSX is identical |
| Sibling's verifier | `save_tool.py verify` accepts an exported card |
| New game | Matches the reference save outside recorded differences |
| In game | Loads, walks, fights and re-saves on the PC harness, and on Mednafen for PSX targets |

## 14. Open questions

For the owner:

1. **Saves in the chapters no real PSX save covers:** 0, 3, 7, 8, 9, 11, 12 and 13
   (section 15). The new-game references are held for PC, US and JP (section 12.1).

For later investigation:

2. EU, FR and DE card file names. Red Herring's code accepts `SLES-01304`. We hold
   no such saves, and the FR and DE serials are unconfirmed.
3. Name codecs for the PSX EU, FR and DE releases.
4. Settled: the 18 dwords at `+0xC28` are the two gene-combination lists
   (section 7, [D](save-block/range-d-skills.md)).
5. How the launcher turns the install's glyph codes into text it can draw. Until
   then, item and ability lists can show ids.
6. Which of the UNKNOWN bytes matter. They stay preserved meanwhile.
7. The CITED fields to confirm before they become editable.

## 15. How the measurements were made

Run 2026-11-09 with read-only scratch scripts. Output was counts and verdicts only.

| Set | Files |
|---|---|
| PSX | 12 raw card images under `../BreathOfFire3Recomp/saves` and `mednafen/sav`: 28 BoF3 entries (20 JP, 8 US), 12 distinct blocks |
| PC | 67 files in `tools/recipe_saves/`, 64 distinct |
| Combined | 75 distinct after normalising PC records to the PSX layout. Counts of 76 count one block seen on both sides twice |

Limits: the PC set is mostly crafted recipe saves, so some scene bytes are not what
the game would write. No EU, FR or DE save is in the set, and the latest PSX save
is about 42 hours in.

**RetroArch's cards, read 2026-11-09** (`E:\SteamLibrary\steamapps\common\RetroArch\saves`,
read in place): two `.mcr` and two `.srm` hold BoF3 saves, and all four are the
same raw card format, with valid directory checksums, `SC` headers and block
checksums. A third `.mcr` is an empty card. The `.srm.bsv` is an input recording
(magic `BSV1`), not a card. Nine saves, of which six are new to the set: JP at 0:25
and 1:38, US at 0:12, 4:59, 8:17 and 43:25.

**The sibling's `saves/card1.mcd.bak`** holds the authentic psxrecomp JP saves
(owner, 2026-11-09: the other cards there are copies made from it): 0:28, 5:19 and
12:48. The first pass skipped `.bak` files. Two were already in the set through
those copies, and the 5:19 save, chapter 2, is new. `card2.mcd.bak` is empty.

**The owner's play folder** (`game\game` and `game\backups`, read 2026-11-09 after
the owner pointed at it): 44 PC save files, 19 distinct. Two were known (the
first-save-point reference and the edited gene test). The other 17 are one real
PC playthrough: chapters 0, 1, 1, 2, 3, 3, 4, 5, 6, 6, 8, 9, 9, 12, 14, 14 and a
save made after the ending. They are the best evidence held for chapter
baselines, and the cleared save gives the summary's "cleared" byte at `+0xCB4`
([`story-flags.md`](story-flags.md) section 4).

**Real PSX saves by chapter byte, all sources:** chapter 1 has seven, chapter 6
five, chapter 2 two, and chapters 4, 5, 10, 14 and 15 one each. Chapters 0, 3, 7, 8, 9, 11, 12
and 13 have none. These are the saves chapter baselines can use.

The coverage count transcribes each saver in Red Herring's editor to a byte map at
commit `7c93fd3`, then compares it with the samples.

## 16. Build order

1. Format core and field table, with the round-trip test, headless.
2. Containers and conversion, with the tests against `save_convert.py`.
3. The page: open, edit and write for the E-class fields.
4. Advanced page, generated from the field table.
5. New-game creation, with the reference comparison.
6. Story points: chapter-start baselines, then captured events.

Each stage is its own branch and can stop there and still be useful.

## 17. Mistakes in the existing editor not to repeat

Found by reading its code. None has been reproduced in a run.

- Defence, agility and intelligence are read unsigned and written signed. Values
  above 32,767 would be clamped on write. No sample has one.
- The signed reader tests byte 0 for the sign, which is wrong past one byte.
- A short faerie name is padded at the rooms offset instead of the names offset,
  which can overwrite other names and leaves old letters behind.
- Deleted card entries are treated as live saves.
- A file that cannot be read fails with no message on the page.
- The slot summary is edited separately from the record it mirrors.
- The masters are read from 3 bytes and written to those 3 and the next 3. The
  second 3 are their own field (`+0xC77`, masters open), so a save through the
  editor opens every master that was only met.

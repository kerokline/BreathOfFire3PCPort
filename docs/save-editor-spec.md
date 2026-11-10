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

| Offset | Size | Field | Class | Evidence |
|---|---:|---|---|---|
| `+0x000` | 16 | Scene state. Byte 0 is the chapter, byte 2 the state | A | READ, [`ending-backdrop.md`](ending-backdrop.md); CITED frost `SceneState` |
| `+0x010` | 3 | Unnamed | P | UNKNOWN |
| `+0x013` | 3 | Scene event mode, step, selection | A | CITED |
| `+0x016` | 10 | Unnamed, zero on every sample | P | MEASURED zero |
| `+0x020` | 4 | Script variables | A | CITED |
| `+0x024` | 2 | Area, u16 | E | MEASURED: high byte zero on every sample |
| `+0x026` | 1 | Entry kind | A | CITED |
| `+0x027` | 1 | Facing | E | CITED; values seen are 1..5, 7 and `0x80` |
| `+0x028` | 8 | Position x, z, 16.16 fixed | E | CITED sibling and frost; crafted saves rely on it |
| `+0x030` | 64 | Unnamed, 6 bytes live | P | UNKNOWN |
| `+0x070` | 2 | Checksum | D | MEASURED |
| `+0x072` | 6 | Unnamed | P | MEASURED zero |
| `+0x078` | 8 | Options. `+0x78` and `+0x7E` follow the button mapping, `+0x7B` probably the window background (section 12.1). frost cites sound mono at `+0x79` and window style at `+0x7A` | A | READ that the port honours them; bytes MEASURED as differences, meanings probable |
| `+0x080` | 16 | Unnamed, 2 bytes live across `+0x07B..+0x08F` | P | UNKNOWN |
| `+0x090` | 8 x `0xA4` | Character records, section 8 | | |
| `+0x5B0` | 160 | Scene flag words, 8 bytes for each of 20 chapters | A | READ: `Cond_Flags` `0x903F90`; 82 bytes live |
| `+0x650` | 40 | Event flags | A | READ: `Flag_Test` family; 21 bytes live |
| `+0x66E` | 2 | Faerie village stage, inside the flags above | E | CITED Red Herring |
| `+0x678` | 4 | Zenny | E | MEASURED |
| `+0x67C` | 4 | Unnamed, 3 bytes live | P | UNKNOWN |
| `+0x680` | 1 | Active formation | E | CITED |
| `+0x681` | 1 | Unlocked formations | E | CITED |
| `+0x682` | 3 | Field order, character ids, `0xFF` empty | E | MEASURED |
| `+0x685` | 3 | Battle order, same ids | E | MEASURED: a permutation of field order, 75 of 75 |
| `+0x688` | 96 | Unnamed, 42 live | P | UNKNOWN |
| `+0x6E8` | 4 | Play clock: h, m, s, frames | E | MEASURED |
| `+0x6EC` | 32 | Unnamed, 22 live | P | UNKNOWN |
| `+0x70C` | 32 | Fishing best catch, 23 used | E | CITED |
| `+0x72C` | 32 | Unnamed, 14 live | P | UNKNOWN |
| `+0x74C` | 1 | Loaded character index | A | CITED |
| `+0x74D` | 4 | Unnamed, all live | P | UNKNOWN |
| `+0x751` | 1 | BGM id | A | CITED |
| `+0x752` | 2 | Unnamed | P | MEASURED zero |
| `+0x754` | 4 | Battle count | A | MEASURED, section 12.1 |
| `+0x758` | 28 | Lifetime zenny and unnamed | A / P | CITED |
| `+0x774` | 512 | Item ids, 4 categories x 128 | E | MEASURED by the sibling's `save_tool.py verify` against disc tables |
| `+0x974` | 512 | Item counts | E | as above |
| `+0xB74` | 32 | Vital items | E | as above |
| `+0xB94` | 128 | Skill notes | E | CITED |
| `+0xC14` | 92 | Unnamed. `+0xC28`: eight 4-byte rows, identical at a new game, one byte of each constant on every sample. `+0xC48`: 40 copies of one byte on every sample (section 12.1) | P | MEASURED shape, meaning UNKNOWN |
| `+0xC70` | 3 | Dragon genes | E | CITED |
| `+0xC73` | 1 | Unnamed | P | UNKNOWN |
| `+0xC74` | 6 | Masters, written twice over | E | CITED |
| `+0xC7A` | 2 | Unnamed | P | UNKNOWN |
| `+0xC7C` | 8 | Celerity and Bonebreak timers | E | CITED |
| `+0xC84` | 28 | Unnamed | P | UNKNOWN |
| `+0xCA0` | 28 | Slot summary, below | D | MEASURED |
| `+0xCBC` | 18 | Nine button masks, u16 | A | CITED sibling and frost |
| `+0xCCE` | 2 | Unnamed | P | UNKNOWN |
| `+0xCD0` | 26 | Faerie battle counters at `+0xCD0`, `+0xCDC`, `+0xCE0`, u32, and unnamed bytes | E / P | CITED |
| `+0xCEA` | 5 | Faerie food, max jobs, culture, clear and build power | E | CITED |
| `+0xCEF` | 1 | Unnamed | P | UNKNOWN |
| `+0xCF0` | 480 | Faerie jobs, 60 x 8 | E | READ: [`save-interchange.md`](save-interchange.md) section 2a |
| `+0xED0` | 64 | Faerie rooms, 8 x 8 | E | CITED |
| `+0xF10` | 300 | Faerie names, 60 x 5 | E | READ |
| `+0x103C` | 116 | Tail, 1 byte live | P | MEASURED |

**Coverage (MEASURED, 75 distinct blocks):** the existing editor writes 2,931 of
the 4,272 bytes. Of the other 1,341, 527 are non-zero in at least one save and
390 vary between saves. Every byte the editor does not know must survive a write.

### Slot summary at `+0xCA0`

Derived. Regenerate it on write unless the player overrides it on Advanced.

| Offset | Size | Field | Rule | Evidence |
|---|---:|---|---|---|
| `+0xCA0` | 5 | Name | Record 0's name | MEASURED 73 of 75 |
| `+0xCA5` | 3 | Portraits | Field order, with one substitution (id `0x0B`) | MEASURED 74 of 75; CITED rule |
| `+0xCA8` | 1 | Level | Record 0's level | MEASURED 75 of 75 |
| `+0xCAC` | 4 | Play clock | Equals `+0x6E8` | MEASURED 76 of 76 h:m:s |
| `+0xCB0` | 4 | EXP | Record 0's EXP | MEASURED 75 of 75 |

This follows record 0, not the walking leader. A first reading against the field
order's leader matched 64 of 76.

## 8. Character records

Eight records, stride `0xA4`. **PSX layout below.** The PC name is 9 bytes, and
every later field is 4 bytes further in. The stride is unchanged, so the PC gives
up the last four bytes, which are zero in every sample of both kinds (MEASURED).

Record positions hold fixed characters. MEASURED `char_id` by position: 0 is
{0, 7, 9}, 1 is {1, 8}, positions 2 to 6 are their own number, 7 is 10.

| Offset | Size | Field | Class | Evidence |
|---|---:|---|---|---|
| `+0x00` | 5 | Name (PC: 9) | E | MEASURED |
| `+0x05` | 1 | `char_id` | A | MEASURED |
| `+0x06` | 1 | Level | E | MEASURED |
| `+0x07` | 1 | Unnamed | P | UNKNOWN |
| `+0x08` | 4 | EXP | E | MEASURED |
| `+0x0C` | 2 | Status bits | A | CITED |
| `+0x0E` | 6 | Weapon, shield, helm, armor, two accessories | E | MEASURED by the sibling |
| `+0x14` | 4 | Current HP, AP | E | MEASURED: HP <= max on all |
| `+0x18` | 1 | Unnamed | P | UNKNOWN |
| `+0x19` | 1 | Fatigue | E | CITED |
| `+0x1A` | 1 | Unnamed | P | UNKNOWN |
| `+0x1B` | 1 | Master | E | CITED |
| `+0x1C` | 4 | Max HP, AP, current | D | READ: rebuilt on load |
| `+0x20` | 28 | Live stats and live copies of `+0x4A..` | D | READ: `Char_RecalcStats` on every load, `TitleFlow_EnterGame` in `src/game/save_menu.cpp` |
| `+0x3C` | 4 | Base max HP, AP | E | CITED |
| `+0x40` | 8 | Base power, defence, agility, intelligence, u16 | E | MEASURED: top bit clear in all 608 records |
| `+0x48` | 2 | Unnamed | P | UNKNOWN |
| `+0x4A` | 1 | Willpower | E | CITED |
| `+0x4B` | 9 | Resistances | E | CITED |
| `+0x54` | 5 | Surprise, reprisal, critical, dodge, accuracy | E | CITED |
| `+0x59` | 3 | Unnamed, zero on every sample | P | MEASURED zero |
| `+0x5C` | 40 | Four ability lists of 10: heal, assist, attack, skill | E | MEASURED by the sibling |
| `+0x84` | 1 | Apprenticing level | E | CITED |
| `+0x85` | 6 | Growth, **signed** bytes | E | MEASURED: 680 bytes have the top bit set |
| `+0x8B` | 25 | Unused | P | MEASURED zero; highest live offset is `+0x8A` ([`save-interchange.md`](save-interchange.md)) |

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
6. Control scheme: the option bytes travel with the save. The player chooses
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
6. Write the few remaining start-up fields (below), then seal.

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
- **The patterns near `+0xC28` are simple.** `+0xC28..+0xC47` is eight identical
  4-byte rows. `+0xC48..+0xC6F` is one byte 40 times. Both are already there at
  frame 1 and unchanged in the second save. The block is in the image's
  uninitialised part (it sits at `0x9039E0`; the section's file data ends at
  `0x676000`), so code writes them at start-up. READ: `Scena00_Start` writes -1 to
  the 18 dwords `0x904608..0x90464F`, which is exactly this span, and the bytes
  are all `0xFF` ([`story-flags.md`](story-flags.md) section 3).
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
[`story-flags.md`](story-flags.md), with chapter 0 complete. A chapter's code names its flags
outright, as `Flags_Set(Row(), n)` and `Flags_Test(Row(), n)`, so each flag's
meaning can be read from the function around it, and the saves confirm the reading.

**A third save at the same point** (owner, 2026-11-09): the English psxrecomp card,
`../BreathOfFire3EnglishRecomp/saves/card1.mcd`, 0:11 in, three battles. It is the
same raw card format, and its header frame and unused frames are byte-identical to
RetroArch's. Against `start_normal.DAT` 19 bytes differ, every one accounted for:

| Bytes | Reading |
|---|---|
| `+0x6E9..+0x6EB`, `+0xCAD..+0xCAF`, `+0x70` | Clock, its summary copy, checksum |
| `+0x78`, `+0x7E`, six button-mask bytes from `+0xCBC` | The button mapping. Equal on both US saves, different on PC |
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
4. What the 18 dwords at `+0xC28` mean. Their writer and start value are known
   (section 12.1).
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

**Real PSX saves by chapter byte, all sources:** chapter 1 has seven, chapter 6
five, and chapters 2, 4, 5, 10, 14 and 15 one each. Chapters 0, 3, 7, 8, 9, 11, 12
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

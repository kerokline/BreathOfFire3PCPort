# The cache's music containers: `base/bgm/NNN.DAT` and `base/bgm/bank/NAME.DAT`

**Status:** SPECIFIED (2026-10-08) - the format the importer writes
(`tools/seq.py`, group IMP of [`sequenced-music-plan.md`](sequenced-music-plan.md)
section 9) and the engine's player reads (`src/audio/`, group SEQ). Written
first so that the two groups can work side by side; a change here is made
by whichever group needs it, with the version bumped, and the other told.

Both files are little-endian, every field naturally aligned, no padding
beyond what is written here. Every string is NUL-padded to its field.
Nothing in either file is a byte copy of a disc section: the importer
re-lays every record ([`ASSET_SOURCES.md`](ASSET_SOURCES.md) section 3's
rule), and the sample bodies, which are the SPU's own ADPCM blocks, are the
one thing carried as they are, because the SPU is what reads them.

## 1. `base/bgm/NNN.DAT` - one song

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | `BF3S` |
| 4 | `u32` | version, 1 |
| 8 | `u32` | flags: bit 0 the song loops (it carries libsnd's loop markers); the rest 0 |
| 12 | `char[16]` | the bank's name: `base/bgm/bank/<name>.DAT`, e.g. `BGM019` |
| 28 | `u16` | the song's number N (the PC's: song N, `165` the battle bundles' sub 1) |
| 30 | `u16` | the sub-song index in its SEP (0..3), for the record |
| 32 | `u16` | resolution (ticks per quarter note, the SEP sub-song header's) |
| 34 | `u16` | rhythm, the two bytes of the header as they are (for the record) |
| 36 | `u32` | the initial tempo, microseconds per quarter note (the header's 24-bit value) |
| 40 | `u32` | event count E |
| 44 | `u32` | the tick of the (first) loop start marker (controller 99 = 20), or `0xFFFFFFFF` |
| 48 | `u32` | the tick of the loop end marker (controller 99 = 30), or `0xFFFFFFFF` |
| 52 | `u32` | the tick of the end-of-track meta event |
| 56 | `Event[E]` | the events, 12 bytes each, in the order the SEP has them |

`Event`:

| Offset | Type | Field |
|---|---|---|
| 0 | `u32` | tick, absolute from the song's start |
| 4 | `u8` | status, with the channel in the low nibble (`0x80`..`0xEF`), or `0xFF` for a meta event; running status resolved |
| 5 | `u8` | the first data byte (note, controller number, program, bend LSB, meta type) |
| 6 | `u8` | the second data byte (velocity, controller value, bend MSB; 0 where the message has one) |
| 7 | `u8` | 0 |
| 8 | `u32` | the meta event's value (`0x51`: the tempo in microseconds; `0x2F`: 0); 0 for a channel event |

Rules the importer keeps:

- every event in the sub-song is written, the loop markers and the
  end-of-track included; nothing is dropped or re-ordered, since libsnd
  processes events at one tick in the file's order;
- a note-on with velocity 0 is written as it is (status `0x9n`, velocity 0),
  not turned into a note-off: the player decides as libsnd does;
- the SEP's only meta events are `0x51` (tempo) and `0x2F` (end); anything
  else is an error in the import, not a silent skip;
- two songs carry a **second loop start** before their one loop end: 37
  (`BGM028` sub 1, starts at ticks 18 and 402, end 1170) and 80 (`BGM082`
  sub 0, 24 and 1560, end 6936). The header's loop start is the first; both
  markers are in the events, and the player treats the second as libsnd
  does (the reading of `_SsContNrpn2` decides which start the end jumps to).
  More than one end, or an end without a start before it, is refused by the
  import (none on the disc).

## 2. `base/bgm/bank/NAME.DAT` - one VAB

**Version 2** (2026-10-08, group IMP): version 1 indexed the tone table by
program number, which is wrong for 27 of the 81 banks. The VAB header holds
one 16-slot tone block per program *that has tones*, in program order, not
one per program number: `BGM053`'s one program is program 10 and its tones
are block 0; `BGM032`'s six are programs 2..7 in blocks 0..5. Measured on all
81 `BGM*.EMI` VABs: block k's tones all carry `prog` = the k-th program with
`tones > 0`, and `ps` is the count of such programs. Version 2 puts that
block index in the `Program` record's byte 5 (below); nothing else changed.

| Offset | Type | Field |
|---|---|---|
| 0 | `char[4]` | `BF3B` |
| 4 | `u32` | version, 2 |
| 8 | `char[16]` | the EMI's name, e.g. `BGM019` |
| 24 | `u16` | the VAB header's `ps`, the program count the header declares |
| 26 | `u16` | the VAB header's `ts`, the tone count |
| 28 | `u16` | the VAB header's `vs`, the sample count S |
| 30 | `u8` | the VAB header's `mvol` |
| 31 | `u8` | the VAB header's `pan` |
| 32 | `u8` | the VAB header's `attr1` |
| 33 | `u8` | the VAB header's `attr2` |
| 34 | `u16` | 0 |
| 36 | `Program[128]` | the program table, 8 bytes each |
| 1060 | `Tone[ps * 16]` | the tone table, 24 bytes each, as the VAB header lays it out: `ps` blocks of 16 slots, block k the k-th program with tones (`Program.block`), so program p's tone t is `Program[p].block * 16 + t`; slots past the program's `tones` are carried as they are |
| 1060 + 384 ps | `Sample[S]` | the sample table, 8 bytes each: `Sample[i - 1]` is the VAB's sample i (a tone's 1-based `vag`) |
| 1060 + 384 ps + 8 S | bytes | the ADPCM bodies, back to back in sample order, sample i at its table offset, each a whole number of 16-byte blocks |

`Program` (the VAB `ProgAtr`'s fields, re-laid; the two reserved bytes dropped):

| Offset | Type | Field |
|---|---|---|
| 0 | `u8` | `tones` - the number of tones the program uses |
| 1 | `u8` | `mvol` |
| 2 | `u8` | `prior` |
| 3 | `u8` | `mode` |
| 4 | `u8` | `mpan` |
| 5 | `u8` | `block`: the program's tone block, its rank among the programs with `tones > 0` (0 for the first), `0xFF` for a program with no tones (version 2) |
| 6 | `u16` | `attr` |

`Tone` (the VAB `VagAtr`'s fields, re-laid; the four reserved halfwords dropped):

| Offset | Type | Field |
|---|---|---|
| 0 | `u8` | `prior` |
| 1 | `u8` | `mode` (bit 2: reverb on, as libsnd reads it) |
| 2 | `u8` | `vol` |
| 3 | `u8` | `pan` |
| 4 | `u8` | `center` (the note at which the sample plays at its recorded pitch) |
| 5 | `u8` | `shift` (the fine tuning, 1/128 of a semitone) |
| 6 | `u8` | `min` (the lowest note the tone answers) |
| 7 | `u8` | `max` |
| 8 | `u8` | `vibW` |
| 9 | `u8` | `vibT` |
| 10 | `u8` | `porW` |
| 11 | `u8` | `porT` |
| 12 | `u8` | `pbmin` |
| 13 | `u8` | `pbmax` |
| 14 | `u16` | 0 |
| 16 | `u16` | `adsr1`, the SPU ADSR1 register value as the header holds it |
| 18 | `u16` | `adsr2` |
| 20 | `u16` | `prog`, the tone's program (the header's own field, for the record) |
| 22 | `u16` | `vag`, the sample index, **1-based as the VAB has it** (0 = none) |

`Sample`:

| Offset | Type | Field |
|---|---|---|
| 0 | `u32` | the body's offset from the start of the bodies |
| 4 | `u32` | the body's size in bytes (from the header's size table, which is in 8-byte units) |

The bodies are the VAB body's bytes for that sample, block 0 (the zero block
Sony's tools write) **included** - libsnd's `SsVabTransBody` puts the body
into SPU RAM as it is and the SPU starts from the block the tone's address
names, so the player sees exactly what the hardware saw.

## 3. The manifest

Each file hashed into `manifest.toml` with its source:
`psx-jp:seq:BGM019.EMI#<section>` for a song, `psx-jp:vab:BGM019.EMI#<vh>,<vb>`
for a bank.

## 4. Checks

- `tools/seq.py dump <file>` prints a song's events (or a bank's programs,
  tones and sample sizes) in a plain text form; `importer.py check` round-trips
  a synthetic SEP and VAB through the writer and the dump with no game data.
- The player's loader refuses a wrong magic or version loudly.

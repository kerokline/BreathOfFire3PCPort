# Save interchange: the PC save is the PSX game block, with one field widened

**Status:** IN PROGRESS (2026-09-19) — format established and converter written;
statically verified both ways; **not yet loaded in either game.**

[`IDEAS.md`](IDEAS.md) I1. Tool: [`tools/save_convert.py`](../tools/save_convert.py).
Save files are game-derived data: none is committed, and nothing here quotes
their contents beyond offsets, sizes and the decoded play-state figures the
tool prints.

## 1. What was compared

| Side | Files | Source |
|---|---|---|
| PC | `bof3/BISLPS00.DAT`, `01`, `0F` — 19 minutes in, new game | owner, 2026-09-19 |
| PSX JP | `../BreathOfFire3Recomp/saves/card1.mcd`, three saves (0:28, 5:19, 12:48) | the sibling's psxrecomp runtime |
| PSX US | `../BreathOfFire3Recomp/saves/Breath of Fire III (USA).1.mcr`, one save (42:37) | RetroArch (owner's recollection: Beetle core) |

The PSX layout — an 8 KiB card block with title and icon frames in its first
`0x200` bytes, then a `0x10B0`-byte game block copied from RAM `0x801448D4` and
byte-summed into a u16 at block `+0x70` — is the sibling's finding
(`docs/SAVE_IMPORT.md`, `tools/save_tool.py`), as is the fact that the US and JP
releases write the same block. Both were re-measured here rather than trusted:
see §2.

## 2. Findings

**The PC file is the game block alone, from offset 0.** No card header, no
title frame, no icons. `0x12B0` = `0x10B0` + `0x200`: the file keeps the PSX
*file* length minus nothing useful — bytes `0x10B0..0x12AF` are zero in all
three PC saves.

**Same checksum, same place.** In all three PC saves the u16 at `+0x70` equals
the byte-sum of bytes `0..0x10AF` with that u16 excluded — the PSX rule at the
PSX offset. (It also verifies over `0x12B0`, the tail being zero; so the length
the PC game sums is not determined by this. The PC-side checksum code is
unread, and whether the PC load path *checks* it is unknown.)

**Same fields at the same offsets.** Decoded with the sibling's PSX offsets, the
PC saves give play time 00:19:04 / 00:19:26 (the two saves were made 22 s
apart — consistent with their file times), zenny 0, area 10, party `[9, -, -]`
— the same opening state as the JP 0:28 save (area 10, party `[9, -, -]`).
The slot summary sits at `+0xCA0`, `0x1C` bytes, on both; on the PC side that
is independently what `Save_ReadSummaries` `0x588DC0` reads
([`save-files.md`](save-files.md) §2 — whose first reading had the offset and
size swapped; corrected today).

**One layout difference: the character name is 5 bytes on PSX and 9 on PC.**
The eight character records are at `+0x90`, stride `0xA4`, on both. Inside a
record the PSX has name `+0..+4` and `char_id` at `+5`; the PC has name
`+0..+8` and `char_id` at `+9`, and **every later field is 4 bytes further in**
— level, exp, HP, the stat blocks, and the late three-byte field the JP save
has at `+0x70` and the PC at `+0x74`. Found by diffing PC slot 0 against the JP
0:28 save: the differing runs start exactly on the eight record boundaries.
The stride did not grow, so the PC record has 4 fewer tail bytes; across every
PSX save on this machine (the three cards above plus the eight Mednafen cards
under `../BreathOfFire3Recomp/mednafen/`) the highest nonzero record offset is
`0x8A`, so nothing observed is lost.

This is the second measured instance of the port widening a name field for
two-byte Chinese text — the enemy name went 8→12
([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2). It also corrects
[`PLAN.md`](PLAN.md) §3's "the persistent character record is 164 bytes in both
binaries": true of the stride, **not** of the layout. Any name transfer of
record *field offsets* from the PSX corpus needs +4 past the name.

**The summary's 5-byte name did not widen.** The `+0xCA0` summary starts with a
5-byte name then the leader's `char_id` at `+5`, on both sides. A PC name longer
than two characters cannot fit; what the PC game does then is unobserved.

**Names do not convert.** JP stores its one-byte kana encoding, US stores ASCII,
PC stores two-byte codes. The converter therefore takes names from a **donor
save of the destination format**, record for record — every save carries all
eight records with default names from a new game on. A renamed Ryu becomes the
donor's Ryu.
**The PC port has no name entry at New Game (owner, 2026-09-19).** On the PSX,
New Game prompts for Ryu's name, then for UI/control options, then starts the
Dauna mines scene. On the PC, New Game goes straight to the Dauna mines; the
options screen was moved to a new entry on the start screen, and the naming
prompt is gone. This is a *port divergence* in the ledger's sense (theirs, not
ours). Consequence for conversion: PSX→PC loses only a custom Ryu name, which
the PC game could not have produced anyway, and any PC save is a complete name
donor. PC→PSX gives the donor's names. *Not established:* whether the
name-entry code survives unreachable in the exe, and whether any later point
in the game can rename a character.

**Carried verbatim, not understood:** the eight bytes at `+0x78..+0x7F` differ
between the JP, US and PC saves compared and are the likely home of the option
config the sibling saw ride along on a US→JP import (`SAVE_IMPORT.md`, "Open").
A US save may bring US confirm/cancel with it. Whether the PC port reads those
bytes at all is unknown.

## 3. Static verification

Run 2026-09-19 on scratch copies; the originals were not touched.

- **Round trip is byte-identical:** PC slot 0 → `pc2psx` into a JP card →
  `psx2pc` back, PC donor = the original: `cmp` clean.
- **The sibling's verifier accepts a PC save.** After `pc2psx` into slot 2 of a
  copy of `card1.mcd`, `python tools/save_tool.py verify` (sibling) ran its
  checks joining the block against the JP disc's item, ability and equipment
  tables: 49 pass, 1 fails. The failure is the card-manager title text still saying
  05:19 — expected, `pc2psx` leaves that frame alone.
- `psx2pc` of the JP 12:48 save and of the US 42:37 save both produce files
  whose checksum verifies and whose decoded state matches the source line for
  line.

## 4. What is not established

- **Nothing has been loaded in a game.** The converted JP and US saves are in
  `bof3/BISLPS02.DAT` and `BISLPS03.DAT` for the owner to try
  ([`USER_CHECKS.md`](USER_CHECKS.md)). Until then this is a format finding
  and a tool that is self-consistent, not a working feature.
- Whether ids — items, abilities, flags, area numbers — mean the same thing on
  PC as on the JP disc. The sibling proved US = JP; PC = JP is the expectation
  (the port derives from the JP release, and `AREANNN.DAT` numbering matches
  `AREA<n>.EMI`), but the first real evidence will be the US save loading into
  area 141 with its inventory intact.
- The PC-side code that builds and checks the block (`0x5806F0` by position).
- The other direction in a real PSX runtime: `pc2psx` output has only been
  checked statically.

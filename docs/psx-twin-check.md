# Checking a suspected bug against the PlayStation: the twin read

**Status:** METHOD (2026-10-05) - first used the same night on
`FieldMenu_CampAllowedCell` ([`rest_2d.md`](rest_2d.md) L1); the owner's
words after it: "we can read the original psx calls to see if they match
functionality or are pc-original bugs - a useful validation path for the
open questions".

Every round leaves latent defects "described, not fixed" and some of them
"for the owner": a compare that cannot match, a record read from the wrong
character, a bound that reads on. Each one hides a question the ledger
needs answered before anything is changed - **is it Capcom's design, Capcom's
bug, or the porting house's?** The PC's code cannot say. The PlayStation's
can, because the two binaries are compilations of one source tree
([`SHARED_SOURCE.md`](SHARED_SOURCE.md)): read the PSX twin of the function,
and

- **the same on the PSX** - Capcom's. Fixing it is a divergence from the
  original in the ledger's sense (an Intent entry, the owner's word), and
  the sibling repo may want the finding (an unnamed PSX function with a
  reproducible bug: STATUS's "write findings back" obligation);
- **different on the PSX** - the port's. Fixing it is a restoration; the
  entry says what the PlayStation does and that ours now does the same.

Either way the entry's "Also in the PSX version?" line stops being a guess.

## 1. The method

1. **Find the twin.** `analysis/pairs_propagated.json` first; the group doc's
   `psx` field; the sibling's `names/*.toml` and `symbols.toml`. For the
   functions this question tends to land on there is often no pair, because
   they are reached through tables and the pairing grew along call edges.
   Then find it by the **caller's** shape, not the function's constants:
   the PC caller's sound ids, input masks and global addresses survive
   compilation where small compares do not (below). `FieldMenu_TopBarInput`'s
   twin was found by `li 0x101` / `li 0x104` / `li 0x107` and `andi 0xA000`
   inside one window, and the cell test was the `jal` beside the camp flag.
2. **Get the bytes.** The sibling repo (`../BreathOfFire3Recomp`) has the JP
   disc: `python tools/disc_ls.py "disc/Breath of Fire III (Japan).cue"`
   lists the files with their start sectors, `--extract BIN/ETC/X.EMI --out
   <scratch>` pulls one, and `python tools/emi.py list <file>` gives its
   sections with their RAM destinations (the code section is the large
   `misc/untyped` one at `0x80......`). `SLPS_009.90` is the resident
   executable (`disc/`; its text starts at file offset `0x800`, the
   destination dword at `0x18`). Extract to scratch, never into either repo.
3. **Read it.** capstone, `CS_ARCH_MIPS`, `CS_MODE_MIPS32 |
   CS_MODE_LITTLE_ENDIAN`, the section's bytes at its destination. Read the
   twin to its `jr $ra`, as a PC takeover reads to its `ret`.
4. **Record it** in the group doc's latent-defect entry: the twin's address,
   the module, the instructions that decide the question, and the verdict;
   the ledger entry's "Also in the PSX version?" line when one is written.

## 2. What the compiler hides

GCC for the R3000 folds what MSVC keeps. The camp test's
`x == 0xA0 || x == 0xA1` became `addiu v1, v1, 0x60; andi v1, v1, 0xFF;
sltiu v1, v1, 2` - a range test, no `0xA0` or `0xA1` anywhere - and the
`== 0x91` became `xori v0, a0, 0x91; sltu v0, zero, v0`. So a scan for the
PC's immediates finds nothing and proves nothing: three disc-wide scans for
the four camp constants found no window holding even three of them. Search
for the caller's larger constants, then read.

The order of operations is what the question usually turns on (the mask
before the compares, the leader's record against record 0), and that order
is what survives: a `andi 0xF0` before the range test is the same bug
however the compares are spelt.

## 3. Where things are

| | |
|---|---|
| The field menu | `BIN/ETC/START.EMI` and `BIN/ETC/STATUS.EMI`, whose code sections (`0x801D0C00`, 117,858 bytes) are byte-identical |
| The field, the world map | `BIN/ETC/GAME.EMI` section 0 (`GameMode_Field`'s twin `0x80198378`), `FIRST.EMI` |
| Battles | the `BIN/BATTLE/*.EMI` and `BIN/BOSS/BOSS*.EMI` - the boss overlays repeat the battle engine's code (47 copies of one block showed up in a disc-wide scan), so a scan over the whole image is noise there |
| Areas | `BIN/WORLDnn/AREAnnn.EMI`; the PC's `DAT/AREAnnn.DAT` sections are mostly byte-identical to them ([`DAT_CONTAINER.md`](DAT_CONTAINER.md)), so a *data* question (a cell plane, a table) can be read on the PC side |
| Names | `../BreathOfFire3Recomp/names/places.toml` (the areas, with the developers' map names), `names/*.toml`, `symbols.toml`; `docs/PC_PORT_CROSS_REFERENCE.md` there |

## 4. Done so far

| Question | Twin | Verdict |
|---|---|---|
| `FieldMenu_CampAllowedCell` `0x589FB0` masks the cell before comparing with `0xA1`, `0xAF`, `0x91`, so `0x91` cells (bridges, a harbour: 91 cells on nine world maps) allow the camp | `0x801D21C4`, START / STATUS.EMI, called from `0x801D1B94` beside the camp flag `0x80145046` | **Capcom's**: `andi 0xF0` first, then the three tests on the masked byte ([`rest_2d.md`](rest_2d.md) L1) |

Open and suited to it: debt 3's `PartyAction_WaitEffectDone` reading inside
`Gfx_PacketPools` ([`rest_1d.md`](rest_1d.md) L1); DIV-0076's "Also in the
PSX version?" (the save summary's name against its level); PH's question
whether FT3 / GT3 share the sprite handlers' far-edge slip
([`d3d-rest.md`](d3d-rest.md)); every "for the owner" entry in the wave
docs of rounds twelve to fourteen.

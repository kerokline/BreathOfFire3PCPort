# The EMI type-1 compression, and the PC's own edits to those arenas

**Status:** STABLE (2026-10-08, a cloud session; all held discs and the PC's `DAT/` checked)

[`unified-data-plan.md`](unified-data-plan.md) step 1. [`DAT_CONTAINER.md`](DAT_CONTAINER.md)
section 2 found that type-1 sections (65 on the JP disc, all bound for
`0x80033800`: the `PL*`, `BPL*`, `BRT*` arenas and `START`) are compressed, and
that the PC and the PSP ship them decompressed. Until now nothing could
decompress them. **`tools/type1.py` does, and every one of the 65 is proved
byte for byte against copies held independently.**

## 1. The format

Worked out from the data: the JP stream set against the PSP's decompressed
copy of the same section, the literal runs located by alignment, and the
remaining bytes read as tokens until one rule set reproduced the first section
whole. It was then run against all 65 without changes.

```
u32  size      the decompressed size (= the PSP section's size, 65 of 65)
u16  0xFFFF    65 of 65
tokens until `size` bytes are out, then a single 0xFF byte (65 of 65)

b0 = 0x11..0x1F   0x20 - b0 literal bytes follow                  (1..15)
b0 = 0x10, n      16 + n literal bytes follow                      (16..271)
otherwise b0, b1  a back-reference into the output:
                    distance = 0x800 - ((b0 >> 5) << 8 | b1)       (1..2048)
                    length   = (b0 & 0x0F) + 3; if that is 18, add
                               the next byte                       (3..17, 18..273)
```

An LZ77 variant with an 11-bit distance: no flag bits, no window ring. A literal
run and a reference are told apart by the high three bits and bit 4 of the first
byte. Counted over the 65 streams (`type1.py`'s decode loop, instrumented in
scratch): 861,142 short and 60,914 extended references, 487,253 short and
146,420 extended literal runs. The largest distance is 2,047. **No reference
reaches before the start of the output** (so no pre-filled window is needed),
and **bit 4 of a reference's first byte is never set**. A copy may overlap its
own output, and the decoder copies byte by byte to allow for it.

Not established: Capcom's routine itself. A scan of `SLPS_009.90` for an LZ
loop, the EMI loader's type dispatch, or a reference to the boot EXE's one
`MATH_TBL` string found none of them, so the routine is presumably in an overlay.
So the meaning of the two marks (the leading `0xFFFF`, the trailing `0xFF`), and
of a reference byte with bit 4 set, comes only from the 65 streams. The decoder
**refuses**, with a reason, anything those streams never do: a bit-4 reference,
a distance before the start, an overrun of `size`, a missing terminator, a
truncated stream (CLAUDE.md rule 4). A stream from a build we have not seen will
say so rather than be decoded on a guess. Reading the routine would settle the
marks; nothing in the importer waits on that.

## 2. The proof

```
python tools/type1.py check --disc JPDISC --psp PSPJPISO --dat DAT --disc2 USDISC --disc2 FRDISC --disc2 DEDISC
python tools/type1.py selftest
```

Measured 2026-10-08 on the held builds, each verified against `fixtures.toml`
file by file first:

| Against | Byte-identical | The rest |
|---|---:|---|
| the PSP-JP disc's decompressed sections | **50 of 65** | the 15 `BPLD*` / `BRTD*` arenas carrying the PSP's own 13,770-byte change to Ryu's form data ([`region-diff.md`](region-diff.md) 5.2's P8 candidate) |
| the PC's `DAT/` kind-0 chunks | **51 of 65** | 14 the port edited after decompressing (section 3) |
| either | **64 of 65** whole | `BPLD27A`, which both edit: every one of its 269,856 bytes agrees with one of them (13,770 differ from the PSP, 4 from the PC, **0 from both**) |
| the US, FR and DE discs | **65 of 65** decode to the JP output | one set of arenas on every PSX disc, so one decoder |

The US disc against the PSP-EU gives the same 50 and 15. `selftest` needs no
game data: twelve hand-built streams, one for each token form and limit (the
16 + n run, the overlapping copy, the extended length, distance 2,048) and the
five refusals. CI runs it.

This supersedes the plan's "37 PSP oracles": 37 was the count of PSP sections
equal to the PC's. Against our own decode, the PSP confirms 50, and the PC
confirms 51.

## 3. What the port did to 14 of the arenas

The importer must reproduce these to rebuild the PC's `DAT/` from a disc (the
plan's section 3 step 5 verify). They are **the port's edits, not decoding
differences**: the PSP and the PSX agree on each. Every arena opens with a table
of three `u32` block offsets (the first is `0xC`), one block per party member,
in the order the file name lists them. The digits are the members.

| Arenas | What the PC changed | Measured |
|---|---|---|
| `PL012`, `PL025`, `PL026`, `PL247`, `PL257`, `PL267`, `PL278`, `PL27A` | **352 bytes (`0x160`) shorter, the change inside member 2's block** wherever in the file that block sits: the block offsets after it in the arena header move down by 352 (`PL012`, whose member 2 is the last block, has an offset word inside it moved instead); the content shifts by 352 from a point in the block (`+0x54660` in `PL012`, `+0x3F540` in `PL025` / `026`, `+0x2CD80` in the other five), and in every one of the eight, **exactly 480 32-byte windows** match neither the unshifted nor the shifted decode - one edit of member 2's data, made the same way each time | header words; the content by 32-byte windows against the decode at offsets 0 and +352. The 8 are every `PL` arena naming member 2; the other 11 `PL` arenas are byte-identical |
| `START` | the PC's chunk is **`PL012`'s PC arena**, byte-identical (DAT_CONTAINER's outlier) - not a decode of `START`'s own section (245,444 bytes against 360,060) | compared whole |
| `BPLD012`, `BPLD015`, `BPLD016` | same size; **the same 1,949 words, at the same offsets and with the same values, differ in all three**, all between `+0x1B018` and `+0x2A4DC`, inside member 1's block (`0x1B00C..0x3600C`). Read as `u16`s the changes are small deltas (most often -16, -8, +8, -0x1000, +10) - offsets or coordinates adjusted, not content moved | word diff, the three files' diff lists compared |
| `BPLD27A`, `BPLU27A` | two adjacent words in the last block (4 bytes), the same edit in both | word diff |

Not read: what member 2's 352 bytes are, and what the `BPLD01x` and `27A`
edits fix. That is a reading for the PC executable (which consumes the arenas)
before the importer decides between applying them as a rule (the DIV-0080 form),
carrying them as a recipe, or treating them as the port's defects. For step 2 the
cheapest honest form is a recipe row per arena: `decompress-type1`, then a named
PC edit, verified against `fixtures/pc-zh.DAT.files.tsv`.

## 4. What it changes

- [`DAT_CONTAINER.md`](DAT_CONTAINER.md) section 2's type-1 row and the section 4
  open item: closed.
- [`ASSET_SOURCES.md`](ASSET_SOURCES.md)'s compressed-arenas row: written.
- [`unified-data-plan.md`](unified-data-plan.md) step 1: done; the 14 PC edits
  carried into step 2's recipe set.

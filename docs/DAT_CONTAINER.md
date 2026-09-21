# The `DAT/*.DAT` container

**Status:** STABLE for the container and census, DRAFT for payload semantics (verified 2026-09-19)

The PC port's `DAT/` directory holds 742 files. They are **not encrypted and not
compressed** at the container level. Each is the PSX `.EMI` of the same name
with its TOC inlined into per-section headers, its audio decoded to WAV, and its
MIPS code sections dropped. **2,120 of 2,680 paired sections are byte-identical
to the Japanese PSX disc** (§2).

Tool: [`tools/dat.py`](../tools/dat.py) (`list`, `survey`, `extract`, `compare`).
Measured against the `pc-zh` build in [`fixtures.toml`](../fixtures.toml); the
`DAT/` tree itself is not yet hashed there (open item below).

> **Kinds 4 and 5 are ours (2026-09-20).** No shipped file has them and the
> original loader skips any kind above 3. They occur only in the language
> overlays `tools/loc_build.py` writes: kind 4 is a pen advance for every
> glyph, tag = the space's advance (DIV-0006); kind 5 is one name table's
> 16-byte names, tag = the table's address in `BOF3.exe` (DIV-0008). See
> [`dialogue-localisation.md`](dialogue-localisation.md).

## 1. Format

A `.DAT` is a flat stream of chunks. No file header, no trailer, no alignment.

```
chunk header, 16 bytes, little-endian
  0x00 u32 kind   0 data | 1 image | 2 audio bank | 3 (one instance)
  0x04 u32 tag    meaning depends on kind, see §2
  0x08 u32 size   payload bytes, header excluded
  0x0C u32 zero
payload; next header at +16+size
```

**Evidence:** `python tools/dat.py survey <DAT dir>` — 742 of 742 files walk to
exactly EOF under these rules with `kind ∈ {0,1,2,3}` and word 3 always zero:
3,582 chunks (data 2,087, image 593, audio bank 901, kind-3 1).

### Kind 2 — audio bank

Offsets relative to the payload start:

```
0x000  descriptor area, 0x188 bytes            (not decoded yet)
0x188  61 x (u32 offset, u32 size); size 0 = empty slot
0x380  subfiles, contiguous, each a RIFF WAVE
```

**Evidence:** same survey — on all 901 bank chunks the live slots are contiguous
from `0x380` and the last ends exactly at the payload end; all 4,779 subfiles
begin `RIFF`. 123 banks have no live slots.

This corrects [`PLAN.md`](PLAN.md) §4a, which put the table at file offset
`0x198` and the first WAV at `0x380`: those were a payload-relative offset and a
file-relative one mixed. In a file that opens with a bank, the table is at file
`0x198` and the first `RIFF` at file `0x390`.

## 2. Correspondence with the PSX `.EMI` — full census

All 742 `.DAT` files have a same-named `.EMI` on the Japanese disc
(SLPS-00990; 880 EMIs, the other 138 are §5). Census:

```
python tools/dat_census.py <DAT dir> <EMI dir> --out analysis/dat_census.json
```

Pairing is an order-preserving alignment (`dat_census.align`): every chunk must
pair, images only where `tag == dest`, identical content preferred over equal
size, zero-fill loses ties. An earlier greedy version keyed on address and
mis-paired 47 files; two independent re-derivations agree with the totals below.

| EMI section | Becomes | Tag |
|---|---|---|
| 6 / 8 / 7 (VH, ?, VB) group | one kind-2 bank — exact in 742/742 | the group's EMI `dest` word |
| 6 / 10 / 7 group at `dest 0` (SEQ) | **nothing** — 38 files (35 BOSS, BATTLE×2, DEMO) | — |
| 3 image | kind 1, same size | EMI `dest` verbatim, 593/593 |
| 0 data | kind 0 | PC-side offset; `dest→tag` is 1:1 and constant across the 200 AREA files |
| **1 compressed data** (65, all `0x80033800`) | kind 0, **decompressed** | as type 0 |
| 0, overlay code / BSS | **dropped** — 542 sections | — |

| | identical | differ |
|---|---|---|
| Images | **546** | 47 |
| Data | **1,574** | 513 |

### The 513 data differences have six causes

Counted by PSX `dest` from `analysis/dat_census.json`; each model below was
re-checked directly against the files, not taken from the agent reports alone.

| `dest` | n | What | Tier |
|---|---|---|---|
| `0x800E4000` | 200 (every AREA) | Enemy/formation table. 8 records; the port widens the name field at `+0x48` from 8 to 12 bytes, stride `0x88`→`0x8C`, size 1160→1192. Everything outside the name fields is byte-equal in **200/200**. | evidence |
| `0x80010000` | 199 (AREA083 identical, unexplained) | Per-area script/text block, replaced slot-for-slot: `u16[0]` equal in 199/199. PC text decodes (PLAN §4a inverse) to 2,264 distinct codes, all ≤ `0x0FFF` — a dense glyph index, not GBK. | evidence (structure) / hypothesis (encoding) |
| `0x80033800` | 65 (PL, BPL*, BRT*, START) | EMI **type 1 is compressed**; the PC ships the decompressed arena. `u32[0]` of the PSX payload equals the PC chunk size in 56/65; 8 PL files are short by exactly `0x160`; START is the outlier (its PC arena is byte-identical to `PL012`'s). Control: `BPLU349` ships uncompressed on PSX and is byte-identical on PC. No decompressor written. | evidence |
| `0x80014000` | 44 (BOSS×40, BATTLE×2, FIRST, AFLDKWA) | System message pool, retranslated. 2 distinct blobs per side. `AFLDKWA.DAT` ≡ `FIRST.DAT` chunk 9, byte for byte. | evidence |
| `0x8007280C` &c. | 4 (RYUD00–03) | **One byte**, offset `0x7ACE`, `0xF7`→`0xFF`, same in all four. Fix, flag, or corruption — unknown. | evidence (the byte) |
| `0x8002B800` | 1 (FIRST) | 15 bytes in a u16 table. | unexamined |

Pointer relocation was tested and **disconfirmed**: `0x80xxxxxx` words inside
identical sections are byte-equal on PC, so PSX addresses ship in PC data and
whatever consumes them must translate at runtime.

The enemy-table widening is the first measured place the two builds disagree on
a **struct layout**. It belongs in [`SHARED_SOURCE.md`](SHARED_SOURCE.md)'s
catalogue of porting-house changes, and it is a warning for save interchange:
"same 164-byte record" needs checking field by field, not assuming.

### The 47 image differences

All wholesale tile replacements (payloads are 32×32-halfword tiles; minimum diff
2 whole tiles, 1,798 bytes — no palette tweaks). Tag decodes per the sibling's
rule `(x/32)<<24 | (y/32)<<16 | (bytes/16K)<<8 | flag`, size field correct on
47/47. Only **33 distinct** replacement images: one glyph atlas pasted into 13
sections, one 14-tile block pasted into 16 area pages.

Against the sibling's 37 language-bearing sections
(`regional-builds.md`): 35 in both, **12 only ours** (incl. `FIRST#6`,
`START#5`, which JP and US share — art only the Chinese port touched), 2 only
theirs (`AREA128#7`, `DEMO#5`: PC ≡ JP). bpp/CLUT not established; nothing
rendered.

### The 542 dropped sections — not all code

By `dest`: `801F2C00` 200, `801EEC00` 146, `801D0C00` 57, `80033800` 44,
`80093800` 42, `800C1800` 40, `80117000` 9, `800F5000` 3, `801F6C00` 1.

Word-statistics classification (sibling's `emi_survey.classify`; no
disassembly): 404 code, 67 mixed, 71 data-like. The 44 at `80033800` are
all-zero BSS — nothing lost. **Open and worth chasing:** 8 of 9 at `80117000`
(1.4–6 KB, no `jr ra`, no prologues — looks like s16 tables) and 19 of 200 at
`801F2C00`. If those are data, the port either embedded them in `BOF3.exe` or
lost them; a value-sequence search in the exe (the battle probe's second anchor)
would settle it.

### Kind-0 tags are not a constant rebase

| PSX dest | PC tag | difference |
|---|---|---|
| `0x8002B800` | `0x8000` | `0x80023800` |
| `0x8002EC00` | `0xB400` | `0x80023800` |
| `0x80014000` | `0x4000` | `0x80010000` |
| `0x80010000` | `0x0000` | `0x80010000` |
| `0x800D3800` | `0xB0000` | `0x80023800` |
| `0x800E3800` | `0xC0000` | `0x80023800` |
| `0x80104000` | `0xC8000` | `0x8003C000` |

Three distinct deltas in eleven samples — the same "constant within a block,
different between blocks" shape the kinship probes found for globals. What the
tag indexes on the PC side (one arena? several?) needs the loader read:
`LoadDatFile` `0x454590`.

`AFLDKWA.DAT` is a single kind-0 chunk, tag `0x4000`, size `0x3743` — the same
tag and size as `FIRST.DAT` chunk 9.

## 3. What this gives us

- A **byte-level asset path exists** for everything except audio, contrary to
  PLAN §4a's heading. The sibling's section-level knowledge (image formats, data
  tables, `dest` addresses) applies to the identical chunks directly.
- A **diff oracle for localisation**: sections that differ between JP disc and
  ZH port are, by construction, where language lives.
- The kind-1 tag being the PSX VRAM word confirms the port kept the PSX GPU
  model down to the file format.

## 4. Open

- **The non-code dropped sections** (§2) — search `BOF3.exe` for their values.
- **A type-1 decompressor**, to turn 65 "differ" into identical-or-not.
- **The dropped SEQ groups** in 38 files, and the RYUD byte.
- ~~**Kind-0 tag semantics**~~ — **answered 2026-09-19:** one arena, payload
  copied to `0x803580 + tag` ([`asset-loading-path.md`](asset-loading-path.md)
  §2). Follow-on: generate the PSX-region → tag table from the census.
- **Bank descriptor area** (`0x000–0x188`). First bytes look like
  `{u8, u8, u16 sample rate}` records (`0xAC44`, `0x5622` appear in FIRST);
  unverified.
- ~~**The kind-3 chunk** in `FIRST.DAT`~~ — **answered 2026-09-19:** the port's
  Chinese font, 2,451 glyphs of 24 x 24 at 288 bytes
  ([`asset-loading-path.md`](asset-loading-path.md) §2).
- **Hash the `DAT/` tree into `fixtures.toml`** so these counts are tied to a
  build.

## 5. Outside `DAT/` — `SND/` and `BGM/`

Both directories use the `.DAT` extension but are **not** chunk containers:
`SND/` files are bare RIFF WAVs (mono, 22,050 Hz, 16-bit) and `BGM/` files are
bare MP3s. The exe builds the names itself: `SND\%s.DAT`, `BGM\%03d.DAT`,
`BGM\%03dN.DAT` (strings at file offsets `0x266F9C`–`0x266FB8`).

**`SND/` is the PSX disc's XA audio, not EMI audio** (measured 2026-09-19):

- An XA sector of 37.8 kHz mono ADPCM holds 4,032 samples = 0.10667 s = exactly
  2,352 samples = **4,704 bytes** at 22,050 Hz/16-bit. All **876 of 876**
  numbered files (`NNN_KK.DAT`) are `46 + 4704·n` bytes — whole XA sectors.
- Their total is 14,790 sectors; `BIN/BMAG_XA/MAGIC00.STR` on the JP disc holds
  14,856 audio sectors (16 channels, coding byte 0 = mono 37.8 kHz). So the
  numbered files are that stream cut into clips (99.6% accounted for).
- `VOICE00`–`VOICE04.DAT` are 71, 63, 61, 60, 42 sectors — **exactly** channels
  0–4 of `BIN/SCE_XA/VOICE.STR`.
- `NNN` is a 3-digit group (109 groups), `KK` runs 0–8. 87 of 109 group numbers
  coincide with a `MAGICnnn` file number. Hypothesis: `NNN` = spell/skill id,
  `KK` = which character shouts it. Not confirmed; needs the caller of the
  `SND\%s.DAT` format string.
- The other 11 named files (`Win`, `Item01`, `Oyasumi`, `Sippai01`, …) are *not*
  whole-sector lengths, so they are not straight XA conversions — presumably
  recorded jingles. `KARA`/`PURE` are ~916 sectors long; unexplained.
- `BIN/SCE_XA/S_XA00.STR` (8 channels, 11,302 audio sectors, coding 1) has no
  identified PC counterpart yet.

**`BGM/`**: 166 MP3s, numbered densely `000`–`166` with some `N` variants. The
disc's music is sequenced (81 `BGM*.EMI`, sparse numbers up to 197, VH/VB + SEQ),
so these are renders, renumbered; the dense-vs-sparse numbering means a name
match is not evidence. Mapping unresolved — the table is presumably in the exe
next to the `BGM\%03d` strings.

The 138 disc EMIs with no `.DAT`: BGM* 81 (incl. BGMBAT, BGMA variants), SCENA
19, PLP 18, MAGIC 10, SCEEF* 5, singletons.

Sibling erratum found in passing: `regional-builds.md`'s per-destination table
sums to 41, not 37; the split that reproduces 37 is 13/2/10/11/1.

Working notes behind this section: `analysis/census/*.md` (gitignored).

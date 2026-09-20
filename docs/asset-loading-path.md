# The asset-loading path: file layer and `LoadDatFile`

**Status:** IN PROGRESS (2026-09-19)

[`HANDOFF.md`](HANDOFF.md) step 1, first pass. Read outward from the anchor the
`DAT/` work left — `LoadDatFile` `0x454590` — far enough to choose and land the
phase 0 exit-test target ([`SCAFFOLDING.md`](SCAFFOLDING.md) §5), and to answer
[`DAT_CONTAINER.md`](DAT_CONTAINER.md)'s kind-0 question. Not finished; §4 lists
what is still unread.

All disassembly via `python tools/pe_disasm.py <va>`; caller counts from
`analysis/pc_funcs.json` (`tools/pe_funcs.py`); data references from
`python tools/pe_xref.py <va>`. Names are in [`symbols.toml`](../symbols.toml)
with their tiers.

## 1. The file layer — `0x5A7370`..`0x5A75F0`

One small translation unit of porting-house code (the PSX side reads sectors
through the CD subsystem; nothing here has a PSX counterpart). It wraps the
statically linked MSVC6 CRT's stdio behind a **16-slot table of `FILE*` at
`0x7DE3E4`**; the slot index is the "handle" every loader passes around.

| VA | Name | Reads as | Tier |
|---|---|---|---|
| `0x5A7370` | `File_CdRoot` | `return (char*)0x66BC2C` — the drive-root prefix, a 4-byte buffer | evidence — **ours** |
| `0x5A7380` | `File_Open` | first free slot (−1 if 16 taken); `fopen(path, "rb")`; on failure retry with the drive root prepended via `sprintf("%s%s")` into a 0x50-byte stack buffer; returns slot or −1 | evidence — **ours since 2026-09-19**, with a log line per open; regression-checked against the attract oracle ([`attract-mode.md`](attract-mode.md) §7) |
| `0x5A7420` | `File_OpenWrite` | the write-side opener: same slot scan, `fopen(path, "wb")`, no drive-root retry, **no null check on the result**; sole caller `Save_WriteFile` `0x454870` | evidence — **ours, with the null check added: [`DIVERGENCE.md`](DIVERGENCE.md) DIV-0003** |
| `0x5A7470` | `File_Read` | `fread(dst, 1, size, slot[h])` | evidence — **ours since 2026-09-19** |
| `0x5A74A0` | `File_Write` | `fwrite(src, 1, size, slot[h])` | evidence — **ours** |
| `0x5A74D0` | `File_Size` | `_filelength(_fileno(slot[h]))` | evidence — **ours** |
| `0x5A74F0` | `File_Seek` | `fseek(slot[h], offset, SEEK_SET)` — callee `0x5B9F9E` read 2026-09-19 and is MSVC6 `fseek.c` step for step | evidence — **ours** |
| `0x5A7510` | `File_Close` | `fclose(slot[h]); slot[h] = 0` | evidence — **ours** |

**The whole layer is ours since 2026-09-19** (`src/game/file_io.cpp`; CRT
evidence per callee in `symbols.toml`). All 13 references to `File_Slots`
(`pe_xref 0x7DE3E4`) are inside these seven functions, so the table's contents
are now private to our code: a slot no longer has to hold the exe's `FILE*`.
Nothing uses that yet. Two other users of the exe's CRT streams bypass the
layer entirely and are unaffected: `0x5A72C0` below and `Cfg_Load` `0x4FD030`.

**Who sets the drive root: `0x5A72C0`, read 2026-09-19.** `(path)`: tries
`fopen(path, "rb")` bare, and on success closes it, **empties the root**
(stores 0 to byte 0 of `0x66BC2C`) and returns 1; failing that, for each of ten letters from `C` it
overwrites byte 0 of `0x66BC2C`, and where `GetDriveTypeA` (import slot
`0x5C4088`) returns 5 (`DRIVE_CDROM`) retries the open with the root
prepended, returning 1 on the first success and 0 if none. The find-the-disc
probe. One caller: `0x4FCB50`, in the function at `0x4FCB00` (E8 rel32 scan of
`.text`) — startup-side code beside the window procedure, unread. *(An
earlier revision said "no caller": that came from `pe_xref.py`, which indexes
data addresses only and never sees a call. Corrected 2026-09-19.)*

Observations that matter later:

- **No function here checks its handle.** No bounds check, no null-slot check.
  Every caller tests `File_Open`'s result against −1 first, so it holds — but a
  replacement that adds a check is a (harmless, ledgerable) divergence, and one
  that *relies* on a check is wrong.
- **`File_Open` ignores two of the three arguments its callers push.**
  `LoadDatFile` pushes `(path, 0, 0)`; the body reads only `[esp+0x5C]` after
  its own frame — the first. Probably a vestige of the PSX signature.
- **`File_Open`'s fallback buffer is 0x50 bytes** and the path is
  `sprintf`-ed into it unchecked. Not reachable with the shipped filename
  table; worth remembering when the root becomes configurable.
- **The streams are the exe's CRT's, not ours.** A `FILE*` from our CRT must
  never be stored in `File_Slots`, nor one of those passed to ours. Until
  `File_Open` is ours, our code reaches them only through `Crt_*` bindings.
  This is why the natural unit of replacement here is the *whole file layer at
  once* (then the table can hold Win32 handles or anything else) rather than
  one more function at a time.
- Callers: `File_Open`/`File_Read` 5 each (`0x454590`, `0x454770`, `0x454820`,
  `0x587910`, `0x587A20`), `File_Size` 4, `File_Close` 6. The two `0x5879xx`
  callers are in the sound module — the `SND\`/`BGM\` side, unread.

CRT identifications rest on body reads, not position: `0x5B9D4E` is `fread`
(lock / inner / unlock wrapper; inner computes `size*count`, tests `_flag` at
`FILE+0x0C` against `0x10C`, takes `_bufsiz` from `FILE+0x18` or `0x1000`);
`0x5C3660` returns `FILE+0x10` (`_fileno`); `0x5C35D6` validates the fd then
`lseek` CUR / END / restore (`_filelength`).

## 1a. The BGM loader, and what the `N` suffix means

Read 2026-09-19, prompted by `open … FAILED  BGM\141.DAT` appearing twice in
every attract run.

`0x587A20` `(track)` formats **both** names up front — `BGM\%03d.DAT`
(`0x666FB8`) and `BGM\%03dN.DAT` (`0x666FA8`) — sets a flag at `0x6BDE4C` to 1
and opens the plain name; if that fails it opens the `N` name and sets the flag
to 0; if both fail it gives up. Then: free the previous track's buffer
(`0x6BDE50`), `File_Size`, allocate, `File_Read` the whole file, `File_Close`.
So a failed open of the plain name is **the designed test for which kind of
track this is**, not an error. The install has 166 files in `BGM\`, 9 of them
`N`-suffixed (004, 009, 028, 042, 058, 096, 105, 141, 150).

The flag is the third argument of the music start `0x5A6CC0(buffer, size,
flag)`, called from `0x587B2D`, which stores it at `0x7DE3DC`. Its one reader
is the decode-and-fill routine `0x5A6F30`: when the MP3 decoder `0x5AFC40`
returns `0xFFFFFDFE` (end of stream) it tests the flag — **non-zero: call
`0x5AEBA0(decoder, 0)` and keep decoding, i.e. rewind and loop; zero: zero-fill
the rest of the buffer and set the finished flag `0x7DE3E0`.** Plain `NNN.DAT`
loops; `NNNN.DAT` plays once. Track 141 is the attract/logo music, which fits.
The DirectSound buffer itself is always started looping (`Play(0, 0, 1)`
through vtable `+0x30` at `0x5A6D47`) — it is a streaming ring, and looping the
*music* is done by the decoder rewind.

Whether the PSX marks one-shot tracks the same way, or the porting house
invented the filename convention, is a sibling-side question.

## 2. `LoadDatFile` `0x454590`

`void LoadDatFile(int file_index)` — 477 bytes, 32 callers. **Ours since
2026-09-19** (`src/game/dat_load.cpp`), faithful, including the original's
missing checks; its three callees for kinds 1-3 are bound as originals with
signatures taken from these call sites only (hypothesis tier).

**Verified in bytes, 2026-09-19** (`tools/mem_dump.py`): three fresh hands-off
launches, each suspended 300 logic frames after the area word first reads 4
(the attract sequence's first field scene — after `FIRST`, `DEMO`, `PL27A` and
`AREA004` have loaded) and dumped. Two runs with `BOF3X_ORIGINAL=LoadDatFile`
and one with ours gave the **same SHA-256 for the arena** (`0x803580`,
`0xE0C4C` bytes; `011447acff87b4a1…`) **and for the VRAM shadow** (`0x6C9F44`,
1 MiB; `c481fa48b33f9847…`) in all three. The original-vs-original pair is the
noise floor, and it is zero — the port's determinism reaches memory contents,
not just the `Rand` count. What this does not cover: kinds 2 and 3 land outside
both regions (checked only by the game sounding and reading right), and only
the files that scene loads were exercised.

1. `name = u32[0x64F368 + 4*index]`; return if null. (`0x64F368`, file offset `0x24F368`,
   is a table of `char*`: **799 entries, 57 of them null**, every other one
   pointing into `0x64FFE4`..`0x652888`, and the table ends exactly where the
   first string begins — checked entry by entry 2026-09-19. The path buffer
   is 0x28 bytes on the stack, unchecked.)
2. `sprintf(path, <DAT-directory format at 0x652894>, name)`.
3. `File_Open`; return on −1. `File_Size`; allocate that many bytes
   (`0x5B9660`, MSVC6 `malloc` — read 2026-09-19, `symbols.toml`; the result
   is not null-checked); `File_Read` **the whole file**;
   `File_Close`.
4. Walk chunks until the offset reaches the file size. Header fields used:
   `s8 kind` at +0 (sign-extended, compared unsigned against 3), `u32 tag` at
   +4, `u32 size` at +8, payload at +0x10. Next chunk at `+0x10 + size`.
   Dispatch through a 4-entry jump table at `0x454758` →
   `0x454623`, `0x45465A`, `0x4546FD`, `0x45470E`.
5. Free the file buffer (`0x5B9577`).

This is the container [`DAT_CONTAINER.md`](DAT_CONTAINER.md) derived from the
files alone, confirmed from the code side — including "word 3 always zero": the
loader never reads +0xC.

### The four kinds

| Kind | What the loader does |
|---|---|
| **0 data** | `memcpy(0x803580 + tag, payload, size)`. Before copying: if byte `0x9035A0` is set **and** `tag == 0x10000`, clear it. |
| **1 image** | Splits the payload into **0x800-byte tiles** (32×32 px at 16 bpp) and uploads `size >> 11` of them through `0x59EA70(rect*, pixels*)`. `rect = {x, y, 32, 32}` with `x = (tag >> 24) * 32`, `y = ((tag >> 16) & 0xFF) * 32`; after each tile `x += 32`, and when `x` reaches `x0 + ((tag >> 8) & 0xFF) * 32` it wraps to `x0` and `y += 32`. (The code computes these as `(tag >> 19) & 0x1FE0` etc. — same thing.) |
| **2 audio bank** | `Snd_LoadBank` `0x587CD0(bank, payload, size)`: the tag is a **bank number, 1..6** in the shipped data. Slot = `0x6BC928 + (bank-1)*0x384`: the payload's first `0x380` bytes are a header kept in the slot (64 voice entries of 8 bytes at `+0x180`), the rest is sample data copied to a fresh `malloc`; each voice entry's offset becomes a pointer and gets a sound buffer from `0x5A69C0`. Reloading a bank frees the old data and releases its 64 buffers first. Read 2026-09-19. |
| **3 font** | `buf = malloc(size); memcpy(buf, payload, size); Font_SetGlyphData(buf, size)` `0x5A6800`. The buffer is not freed here — the callee owns it, and frees the previous one. Read 2026-09-19, below. |

### Below the loader: where the three non-data kinds go

Read 2026-09-19; per-function evidence is in `symbols.toml`.

**Kind 1 lands in a shadow of PSX VRAM.** `Gfx_LoadImage` `0x59EA70` is the PC
counterpart of the PSX `LoadImage(RECT*, u_long*)`: it copies the pixels into a
**16-bit 1024 x 512 buffer at `0x6C9F44`** (1 MiB, row stride `0x800`) and then
invalidates. A rect that crosses the right or bottom edge is dropped whole, not
clipped. Invalidation is `0x59E700(rect, 0)`: it walks the 32 PSX-style texture
pages (64 x 256), intersects each with the rect (`IntersectRect`), and for every
touched page releases and zeroes that page's 32 entries in a **1,024-entry
texture cache at `0x6C3F40`** (`0x18` bytes each, two COM pointers at `+0x10` /
`+0x14`). So the renderer keeps PSX VRAM as the source of truth and builds
Direct3D textures from it lazily, per page and per one-of-32 variants —
presumably per CLUT. **This is the seam [`IDEAS.md`](IDEAS.md) I8 wants:** a
replacement presentation layer can keep the VRAM shadow and the `LoadImage`
interface and replace only what sits between the cache and the screen.

**`Gfx_LoadImage` is ours, faithful, and checked in bytes (2026-09-19).**
`src/game/gfx_image.cpp` keeps what the original does at its edges: only the
upper bounds are checked, a failing rect is dropped with no invalidation, `h`
is re-read from the rect every row, and `h <= 0` still invalidates. The
invalidation itself, now named `Gfx_InvalidateTextures`, is still Capcom's and
does more than the paragraph above says — a second pass over the previous
page, not yet read (`symbols.toml`). Verification, `tools/mem_dump.py` at the
default point with the upload queues empty in every run: the all-original pair
`drain_a` / `drain_b` identical in both regions (noise floor zero); `img_ours`,
thirteen functions ours, identical to `drain_a` in both; `img_neg`, the same
build with the destination moved one cell, arena identical and **vram
different in 357,615 bytes**, exit 1 — so the check does see this function.
The attract oracle passed on the same `img_ours` run, 7,478 frames against
`orig_a.tsv`. What none of this covers: anything the attract sequence does not
upload, and the invalidation's effect on the texture cache.

`Font_SetGlyphData` went over in the same change. It runs once per launch —
the one kind-3 chunk — so its store is exercised and its free-the-previous
branch never is, in this run or by any shipped data.

**Kind 3 is the Chinese font, and there is exactly one.** `Font_SetGlyphData`
`0x5A6800` stores the copy in one global, `0x7CC35C`, freeing any previous one.
Its only reader, `0x5A2CA0`, indexes it as `base + glyph*0x120` beside `0x18`
constants: 24 x 24 glyphs, 288 bytes each. The census finds a single kind-3
chunk in all 742 files — `FIRST.DAT`, 705,888 bytes = **2,451 glyphs**. That
answers [`DAT_CONTAINER.md`](DAT_CONTAINER.md)'s "what is kind 3", and it is the
port's one wholly new asset class: the PSX releases keep their glyphs in VRAM
textures. **The cell is exactly double the PSX one** (owner, 2026-09-19): the
JP build draws 12 px glyph cells at a flat 12 px advance
(`../BreathOfFire3Recomp/docs/TEXT_ENGINE.md`), and the port renders 640 x 480
against the PSX's 320 x 240, so 24 x 24 keeps text the same size on screen at
twice the resolution. **The pixel format is 4 bits per pixel, read from code 2026-09-19:** the glyph
draw `0x5A2CA0` locks a DirectDraw surface (vtable `+0x64`), calls the unpacker
`0x5A9E1E(dst, glyph, palette, pitch)`, unlocks (`+0x80`) and blits (`+0x14`).
The unpacker takes the glyph a dword at a time — eight pixels, **low nibble
first** — and writes each nibble through a **16-entry table of 16-bit colours**;
24 pixels a row (three dwords), 24 rows: 288 bytes. Sixteen levels means the
glyphs can carry anti-aliasing, where the PSX's are palette-indexed texture
cells. A second code path, taken when byte `0x7DED63` is 4, is unread
(by position: a 32-bit display). Whether the glyph art was drawn at 24 px or
scaled up from something is not known.
For [`STATUS.md`](STATUS.md)'s localisation goal it means the PC text
path draws from this table, not from the 32 KB atlas the PSX builds use.

### Kind 0 answers the open question: one arena

[`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2 found three different `dest − tag`
differences and asked whether the tag indexes one arena or several. **One.**
Every kind-0 chunk lands at `0x803580 + tag`, no table, no selector.

And that base is already in `symbols.toml`: `0x803580` is the `MessagePools`
block, PSX `0x80010000`. Tag `0x0000` is the script message pool and tag
`0x4000` is the system pool at `0x807580` — exactly the two bases
`Msg_OpenScript` and `Msg_SystemPtr` read. So the arena is **the port's image of
PSX main RAM from `0x80010000` up**, with the tag as the offset into it — but
*repacked*: the porting house closed the gaps between PSX load regions (the
three deltas in `DAT_CONTAINER.md`'s table are three regions sliding down by
different amounts), which is why the tag had to be stored rather than derived
from the PSX `dest`.

Consequences:

- Any PSX address inside a loaded data section translates to a PC address by
  *which region it is in*, not by one subtraction. The region table is
  recoverable from the census (`dest → tag` is 1:1) — worth generating, since
  it converts the sibling's data-table addresses wholesale.
- The arena's extent is a lower bound on a very large BSS object: the highest
  tag in the table above is `0xC8000`, so at least `0x803580..0x8CB580+`.
- `0x9035A0` is a one-shot flag consumed when the chunk at tag `0x10000` is
  loaded. Other touchers: `0x461F00`, `0x4A29C0`, `0x5894D0`. Unread.

### Kind 1: the tag is a tile rectangle, not an address

Bytes of the tag, high to low: **x tile, y tile, width in tiles, (unused)** —
all in 32-pixel units, as this loader reads it; the low byte is not used here.
(The census found this word copied verbatim from the EMI `dest`; whether the
PSX loader decodes it the same way is a sibling-side question, not checked.) `0x59EA70` takes a PSX-style `RECT {s16 x, y, w, h}` plus pixels: it is
the port's `LoadImage`, the entry to the emulated-VRAM side of the renderer, and
the natural next anchor for the graphics layer.

## 3. Why `File_Read` was the exit-test target

Small enough to be certain of (11 instructions), called by every asset loader so
it is exercised within a second of launch, and its result is checkable in
bytes: the first call observed was `File_Read(handle=0, size=1176679)`, and
1,176,679 is the size of `DAT/FIRST.DAT` on disk. See
[`SCAFFOLDING.md`](SCAFFOLDING.md) §5.

## 4. Still unread

- `0x454770` — `LoadDatFile`'s sibling, also formats a `DAT` path. (`0x454820`
  and `0x454870` turned out to be `Save_ReadFile` / `Save_WriteFile`,
  [`save-files.md`](save-files.md).)
- `0x587910` and the `SND\%s.DAT` string — settles the `SND/NNN_KK` numbering
  question ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §5). The BGM half is §1a.
- Below the sinks: `0x5A69C0` / `0x5A6C90` (sound buffer create / release),
  `0x5A2CA0` (glyph draw), and the texture-cache builder that reads the VRAM
  shadow at `0x6C9F44`.
- `0x4FCB00`, the caller of the drive-root probe `0x5A72C0` (§1) — what path
  it probes with, and when.
- The value-sequence search for the dropped non-code PSX sections
  ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2).
- The 32 callers of `LoadDatFile` — which index each passes is the map from
  game state to file.

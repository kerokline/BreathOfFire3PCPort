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
probe. `pe_xref` finds no reference to it, so its caller is unidentified —
an indirect call, or dead code.

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

## 2. `LoadDatFile` `0x454590`

`void LoadDatFile(int file_index)` — 477 bytes, 32 callers.

1. `name = u32[0x64F368 + 4*index]`; return if null. (`0x64F368`, file offset `0x24F368`,
   is a table of `char*`; the name strings [`HANDOFF.md`](HANDOFF.md) located at
   file offset `0x24FFE4` sit just after it and are presumably what it points
   into — not yet checked entry by entry.)
2. `sprintf(path, <DAT-directory format at 0x652894>, name)`.
3. `File_Open`; return on −1. `File_Size`; allocate that many bytes
   (`0x5B9660`, CRT `malloc` by role — unread); `File_Read` **the whole file**;
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
| **2 audio bank** | `0x587CD0(tag, payload, size)` — into the sound module. Unread. |
| **3** | `buf = malloc(size); memcpy(buf, payload, size); 0x5A6800(buf, size)`. The buffer is *not* freed here — ownership passes to `0x5A6800`. Unread. |

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

- `0x454770`, `0x454820`, `0x454870` — `LoadDatFile`'s siblings (`0x454770` also
  formats a `DAT` path; `0x454820` opens and reads without calling `File_Size`;
  `0x454870` is the sole caller of the second opener and of `File_Write` — a
  save path would fit, but that is a guess from the call graph)..
- `0x587910`, `0x587A20` and the `SND\%s.DAT` / `BGM\%03d.DAT` / `BGM\%03dN.DAT`
  strings — settles the `SND/NNN_KK` and `BGM/` numbering questions
  ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §5).
- `0x587CD0` (kind 2), `0x5A6800` (kind 3), `0x59EA70` (kind-1 upload).
- Who writes the drive root at `0x66BC2C` — it ships as `C:\` and `Fmv_Play`
  also uses it, so something at startup must set it to the CD drive.
- The value-sequence search for the dropped non-code PSX sections
  ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2).
- The 32 callers of `LoadDatFile` — which index each passes is the map from
  game state to file.

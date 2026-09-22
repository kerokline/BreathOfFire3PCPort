# The field objects' movement script

**Status:** DRAFT (2026-09-21) - the interpreter's shape, where its scripts
live, and one command read; most opcode lengths are unread.

Found following [`known-defects.md`](known-defects.md) D6 back to its source:
the byte a sprite object is attached by comes from a command of this script.
It is not the frame-animation script of [`sprite-draw-order.md`](sprite-draw-order.md)
§6 (`Sprite_Script*`, two bytes a step), and not the area event script.

## 1. The interpreter

| | PC | PSX (`GAME.EMI` §0) | pairing |
|---|---|---|---|
| one step | `0x576B50` | `FUN_801a9d38` | `call-anchored` |
| the `0xF0` group | | `FUN_801aade8` | |
| the `0x80` group | `0x578790` | `FUN_801ac458` | `callers` |
| attach command, `F8` | `0x5792A0` | `FUN_801ad768` | `call` |

PSX side read from the sibling's Ghidra output
(`analysis/ghidra/GAME_EMI0_80196800_decomp/`); pairs from
`analysis/pairs_propagated.json` (`attract-remaining.md` §5, tiers there).

One call runs one byte of the script at `base + ctx[+10]` (`u16`, the
script position). Each call first runs `FUN_801a9f94` - the low-nibble group
that [`AREA.md`](../../BreathOfFire3Recomp/docs/loader_records/AREA.md) in
the sibling describes calling the area's `+0x3C` handlers - then dispatches on
the byte's **high nibble**:

| high nibble | what (PSX) |
|---|---|
| `0x10`..`0x40` | if `b & 7`: `FUN_801acba8(ctx, (b - 0x10) >> 3)`, with `b & 7` to `ctx[+7]` |
| `0x50` | `ctx[+1] = b & 0xF` |
| `0x60` | `FUN_801aca24` |
| `0x80` | `FUN_801ac458` (opcode `0x86` sets `[0x8014624C] + 0x124` from the descriptor's `+0x18` array) |
| `0x90` | `FUN_801ac8b0` |
| `0xA0` | position `-1`: the byte repeats next time |
| `0xC0` | `FUN_801abb00` |
| `0xD0` | `FUN_801ab470` |
| `0xE0` | `FUN_801aa944` |
| `0xF0` | `FUN_801aade8`, below |

then the position advances by one. The six group handlers' operand lengths
are **unread**; that is what a script walker needs next.

The `0xF0` group, lengths as `FUN_801aade8` advances the position (the
dispatcher's own `+1` included):

| op | bytes | what |
|---|---|---|
| `F0`, `F1`, `F5`, `FB`, `FC` | 1 | |
| `F2`, `FE` | 3, 3 | |
| `F3`, `FA` | 5 | |
| `F4` | 4 | |
| `F6` | 2 | a byte to `0x80146870` |
| `F7` | 7 | |
| `F8` | 2, or 5 when the mode is `7` | **attach**, section 2 |
| `F9` | 3; or 0 - repeats - while `FUN_801ad33c(b1, b2)` returns non-zero | |
| `FD` | 1, or repeats | waits on `0x8014D9E0` |
| `FF` | 0 - repeats | the end: the script stays on it |

## 2. `F8`: attach

`F8 m h s`: store `m` in the object's byte `+1` - or, on the
`0x8015CB38() == 1` path, `m == 7` in byte `+2`; if `m` is `7`, also store `h`
as a dword at `+0x18` - the **handle** - and `s` at `+0x1C`, and advance past
them. The PC's
`0x5792A0` is the same (`and ecx, 0xFF; mov [obj + 0x18], ecx` at `0x5792EE`
and `0x579356`). Nothing else writes object `+0x18` as a byte: a scan of the
exe for stores to `[reg + 0x18]` finds primitive colour triples, and by
displacement from `Sprite_Objects` only two dword stores (`0x41E99E`,
`0x5380E8`, the second into object 0 only).

What reads the handle: `Sprite_ObjectByHandle` `0x57C0A0` turns it into an
object number, for `0x5192A0` (PSX `ov_entry_801A4418`: the object is put at
the other's position, plus an offset from `s` through the descriptor's
`+0x0C` vectors, `FUN_801ad1dc`) and `Sprite_InheritDrawKey` `0x589770` (it
takes the other's draw key). Bit 7 of the handle is D6.

## 3. Where the scripts are

**The area descriptor table** (`0x667590` on the PC, `0x801802EC` on the
PSX; `attract-remaining.md` §5), indexed by `Game_AreaNumber` `0x904EFC`. The
interpreter's three callers, read on both sides:

| PC call site | PSX caller | script base |
|---|---|---|
| `0x517C7A`, in `0x517BF0` (the object update) | `FUN_801a238c` | descriptor `+0x10` `[object +0x77]` (PC `+0x83`) |
| `0x5730E6`, in `0x573090` | `FUN_801c686c` | descriptor `+0x1C` `[byte 0x7E09C3]` |
| `0x5254CF`, in `0x5253E0` | `FUN_801b7bc0` | a pointer at struct `+0x130` (PSX `+0x124`) |

On the PSX the scripts are in each area's section at `0x801F2C00`, which the
PC's `DAT`s drop ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2): **on the PC they
are in `BOF3.exe`'s `.data`**, laid out as the area section was. Area 100, in
address order: its `+0x10` scripts, then the `+0x10` array at `0x619F30`,
`+0x14`, `+0x18`, the `+0x1C` scripts and array at `0x61A3D4`, `+0x04`'s
pointer array at `0x61A420`, the `+0x08` block at `0x61A42C` (one entry's
data - what `FUN_801a78c8` gives an object it spawns), `+0x08` itself at
`0x61AAC0`, `+0x3C`, `+0x34`, and the descriptor at `0x61AAF0`.

`python tools/movement_scan.py` walks the `+0x10` and `+0x1C` arrays (1,688
script starts in 168 areas) and scans `.data` for `F8 07 xx`. The arrays have
no stored length: each walk stops at the first word that is not a pointer
below the array, so the counts are the tool's bound.

## 4. Open

- The six group handlers' lengths, then a walker that decodes every script
  from its start - which would turn D6's raw-byte scan into an exact list of
  attachments.
- `FUN_801a9f94`'s low-nibble group runs before every byte; how it and the
  high-nibble dispatch share a byte is unread.
- The PC's store to struct `+0x130` (the twin of opcode `0x86`), and what the
  struct is.
- For the sibling, not acted on here: its `docs/loader_records/AREA.md`
  lists `FUN_801ad1dc` as a user of descriptor `+0x04`; the function reads
  `+0x0C` (its 6-byte vectors, `index * 6`).
- The PC functions of section 1 are in the attract run's queue - `0x576B50`
  57,758 calls and `0x517BF0` 69,494 ([`attract-remaining.md`](attract-remaining.md)
  §4.5) - and are the natural next takeovers of this group.

# Save files: `BISLPS0?.DAT`, and a save that vanishes from the menu

**Status:** IN PROGRESS (2026-09-19)

First read of the save path, prompted by the owner reaching a save point on
2026-09-19. Feeds [`IDEAS.md`](IDEAS.md) I1 (save interchange). Save files are
game-derived data: none is committed, and nothing here quotes their contents
beyond sizes.

Disassembly via `python tools/pe_disasm.py`; live values by read-only
`ReadProcessMemory` on the running game.

## 1. What is on disk

One file per slot in the game directory, **`BISLPS0<X>.DAT`**, `X` = one hex
digit `0`-`F`: sixteen slots, presented in game as **one card with slots 0-F**
(owner, 2026-09-19) rather than the PSX's two cards. The name is the PSX
memory-card convention (`BI` + product code `SLPS`), kept as a filename.

Each file is **`0x12B0` = 4,784 bytes**, written in binary mode (`"wb"`,
`0x66BC3C`). Observed: two saves, slots 0 and F, both exactly that size. A PSX
memory-card block is 8,192 bytes, so this is **not** a raw card block — whether
it is the PSX save payload with the card header stripped is the first question
for I1, and needs a JP-disc save to compare against.

## 2. The code

| VA | Role | Tier |
|---|---|---|
| `0x454870` | `Save_WriteFile(path, size)`: second opener `0x5A7420` (`"wb"`), `File_Write` `size` bytes from the **fixed staging buffer `0x92A0E0`**, `File_Close`; 0 or -1 | evidence |
| `0x454820` | `Save_ReadFile(path, offset, size)`: `File_Open`, `File_Seek(offset)`, `File_Read` into `0x92A0E0`, close; 0 or -1 | evidence |
| `0x4548B0` | `Save_ListFiles()`: zeroes a 16 x `0x18` table at `0x929F40`, then `_findfirst` / `_findnext` over the pattern at `0x65289C` (`BISLPS??.DAT`), copying each name to entry +0 and its size to +0x14; returns the count | evidence |
| `0x588DC0` | `Save_ReadSummaries()`: marks all 16 summaries empty (`0xFF` at +0x15 of each `0x1C`-byte record from `0x905BC0`); for each slot, finds the directory entry whose **name[7]** parses as that hex digit, reads `0xCA0` bytes at file offset `0x1C`, copies the first `0x1C` bytes of the buffer into the summary, and stores the directory index at +0x15 | evidence |
| `0x5806F0` | fills the staging buffer before a write (by position: called immediately before both `Save_WriteFile` calls) | hypothesis |
| `0x5809C0` | formats the slot filename (`0x664068`, slot from `0x9036D4`), calls `0x5806F0`, writes `0x12B0` bytes. **Called from the window procedure `0x4FC6A0`** — a save triggered by a window message or key, not by the menu. Unread | hypothesis |

Two original-code defects, noted, not fixed: `Save_ListFiles` **never calls
`_findclose`** (a search handle leaks per call), and it **does not bound the
table** — a seventeenth matching file writes past `0x929F40 + 0x180`, into the
`0x92A0E0` staging buffer's neighbourhood.

Call sites: `Save_ListFiles` from `0x587E6B` and `0x588156` only; both are
states of the menu state machine keyed on byte `0x6BDF86` (jump table
`0x66720C`). `Save_WriteFile` from `0x580074` (menu) and `0x5809FF`.

## 3. The defect: a fresh save is missing when the menu is reopened

**Reported (owner, 2026-09-19):** saving appears to succeed, but on reopening
the save menu the slot shows empty.

**Measured, same session:** both files exist on disk with the right size. In
the running game, the directory table `0x929F40` is **entirely zero**; the
slot-0 summary holds data but is marked empty (`+0x15 = 0xFF`); the slot-F
summary, written last, is marked present.

Reading: the write path updates the saved slot's summary directly, but
`Save_ReadSummaries` rebuilds all sixteen from the directory table — and the
table is whatever the last `Save_ListFiles` found. If the menu re-runs
`Save_ReadSummaries` without re-running `Save_ListFiles`, every save made since
the last listing is wiped from the display. That fits all three observations.

**Supported by a code read, same day.** `Save_ReadSummaries` has exactly two
callers (full `.text` scan for `E8` rel32 to `0x588DC0`):

- `0x58815F` — immediately after `Save_ListFiles` at `0x588156`, and only if it
  found something. The load flow: lists, then summarises. Correct.
- `0x57FDE0` — **state 0 of the in-game save menu** (state byte `0x929F02`,
  jump table `0x663F94`). The whole state is: `call Save_ReadSummaries`, set
  three globals, advance. **No listing.**

The other `Save_ListFiles` call, `0x587E6B`, increments a sequence word
(`0x66C7EA`) and records "any save exists" in `0x6BDF8E` / `0x6BDF85`. It is
**not** run at boot, as first guessed: a memory peek taken right after a
restart with two saves on disk found `Save_Directory` still zero and
`0x6BDF8E` = 0, yet the load screen then listed both. So it belongs to the
title/load flow. Either way the conclusion stands: in a session that reaches
gameplay without listing a directory that contains the slot, every opening of
the save menu rebuilds its slots from a table that does not know the file. The owner then confirmed slot F also shows empty on reopening, which
the first hypothesis predicted and the earlier memory peek (slot F still marked
present, *before* the menu was reopened) is consistent with.

Corollary worth testing: in a session that *began* with saves on disk, saving
over an **existing** slot should display correctly (its directory entry
exists), while a save to a **new** slot should vanish until restart.

**Confirmed 2026-09-19.** After a restart (original exe, no DLL) both saves
were listed and one loaded — so the listing works and the data was never at
risk; only the in-session display was stale. Fixed as
[`DIVERGENCE.md`](DIVERGENCE.md) **DIV-0002**: our `Save_WriteFile` re-lists
after writing, verified by the owner with a save to a new slot.

**Not caused by our code.** `File_Read` is ours in this
session, but the empty directory table is upstream of any read — nothing is
read because nothing is listed. The mechanism is entirely in Capcom's
code; the in-harness A/B is listed as owed below.

Still owed: a reproduction of the vanishing save under `BOF3X_ORIGINAL=*`,
and the corollary above (overwrite of an existing slot displays correctly in
the original).

Whether the PSX original has the same flaw is a sibling-side question
(`CLAUDE.md` rule 6); a fix here is a divergence and needs a ledger entry.

# The seven "Windows shell" starts (group PW)

**Status:** IN PROGRESS (2026-10-05 - group PW of the platform round's step 2
([`platform-layers-plan.md`](platform-layers-plan.md) section 4): seven
functions ours in [`src/game/shell.cpp`](../src/game/shell.cpp), each read to
its last instruction, fuzzed against Capcom's at start-up
(`BOF3X_SHADOW=shell`, 21,000 rounds, 0 mismatches), 26 of 27 controls
refused (25 by a count, 1 by a fault with its near variants refused by a
count, 1 that changes nothing).
Headless only: the live check is the state hash after the merge (section 6).
No divergence; two latent defects of Capcom's described (section 3).)

The platform read pass ([`platform-read-pass.md`](platform-read-pass.md)
section 2) filed seven starts as "Windows shell" by their address and found
every one entered by every traced run. They are game code - the pad latch,
the configuration file, the boot set-up, the frame's ordering-table links -
and none of them is a Windows wrapper but `Disc_Probe`, whose one Windows
call is `GetDriveTypeA`. All seven are called from ours: `WinMain`
([`window-modes.md`](window-modes.md)) and, for the latch, `InputScript_Latch`
([`input-script.md`](input-script.md)).

## 1. The functions

| Start | Name | Bytes | Called from | What |
|---|---|--:|---|---|
| `0x4FC6A0` | `Input_Latch` | `0x4E` | WinMain `0x4FCDDE` (ours: `DeviceLatch`, or a recipe's latch) | `Pad_Read`, then the three words of pad 1 and of pad 2 |
| `0x4FD030` | `Cfg_Load` | `0xD4` | WinMain `0x4FCBFE` | `BOF3.CFG`: display mode, renderer, the key table |
| `0x5A9880` | `Cfg_SetDefaultKeys` (named here) | `0x16` | `Cfg_Load` `0x4FD0FA` | `Key_TableDefault` to `Key_Table`, 32 dwords |
| `0x5A72C0` | `Disc_Probe` | `0xB0` | WinMain `0x4FCB50` | the local file, else a CD-ROM drive C..L holding the disc file |
| `0x4FD110` | `Game_Init` | `0xE2` | WinMain `0x4FCD8B` | the boot set-up, ending in a jmp to `Task_SetStackBase` |
| `0x4FD200` | `Gfx_InitBufferBlock` (named here) | `0x24` | `Game_Init` `0x4FD1BE`, `0x4FD1C8` | a double-buffer block's ordering table cleared, its DRAWENV's background on and black |
| `0x4FD290` | `Gfx_LinkOTags` | `0x4A` | WinMain `0x4FCFE0` | the frame's eight list heads spliced onto the current block's ordering table |

Every relative call site above was found by an E8 / E9 scan of `.text`
(2026-10-05); each function has exactly the callers listed.

### The data

| Address | Name | What |
|---|---|---|
| `0x903880` | `Gfx_BufferBlocks` (`[[data]]`, new) | two 0x90-byte blocks, `0x903880` and `0x903910`: a DISPENV at `+0`, a DRAWENV at `+0x14`, the eight-slot ordering table at `+0x70`. `Gfx_CurrentEnv` points at one; WinMain puts `+0` and `+0x14` every rendered frame ([`display-env.md`](display-env.md)) |
| `0x66C648` | `Key_TableDefault` | had its entry ([`controls.md`](controls.md) section 1) |
| `0x7DE7A8` | `Key_Table` | had its entry |
| `0x65DAD8`, `0x65DAE4`, `0x65DAD0` | - | the `BOF3.CFG` name, its mode, the two-integer format (strings, used by address) |
| `0x66BC38`, `0x66BC30` | - | `Disc_Probe`'s mode and its two-string format |
| `0x5C4088` | - | the import slot of KERNEL32 `GetDriveTypeA` (from the import directory) |

Three C runtime entries got names (no `impl`): `Crt_fgets` `0x5B9ADA`,
`Crt_atoi` `0x5B9A9B` (a wrapper over `atol` `0x5B9A10`), `Crt_sscanf`
`0x5B9AA6` (a string FILE on the stack, then the input engine `0x5BCF64`).

## 2. How each works, quirks kept

**`Input_Latch`.** `Pad_Read` (ours, DIV-0050's path) first; then pad 1:
`Input_Previous` = `Input_Held` as read *after* the call, `Input_Held` = the
low word, `Input_Pressed` = `(old ^ new) & new`. The same for pad 2 at
`0x7E1BF0` from the high word, which `Pad_Read` never sets - so pad 2's
three words are zero after the first frame. The original re-reads
`Input2_Held` as a dword for the final mask; the low word is the one just
stored.

**`Cfg_Load`.** `fopen("BOF3.CFG", "rt")`; none: `Cfg_Fullscreen` and
`Cfg_RenderMode` 1, the default keys. Otherwise `fgets` 0x14 bytes at a time
into the frame's first 0x14 bytes, so a line longer than 19 characters counts
as several; line 0 through `atoi` to `Cfg_Fullscreen`, line 1 to
`Cfg_RenderMode`, and each line n >= 2 through `sscanf(line, "%d %d", frame
+ 0x10 + 2n, frame + 0x11 + 2n)`. Those are **byte pointers**: each `int`
lands one byte above the last, so a line leaves the low byte of its first
integer and the low byte of its second, and the second's upper three bytes
lie under the next line's pair until it overwrites them. Then `fclose`, and
with more than two lines `Cfg_SetKeyTable(frame + 0x14)` (ours,
[`rest_2h.md`](rest_2h.md)), else the default keys.

What that makes of the key lines **differs from [`controls.md`](controls.md)
section 1's "two integers a line"**, read as one `(scancode, bits)` entry a
line: a Key_Table entry is four bytes, and a line gives two. Lines 2 and 3
are entry 0 - line 2's two integers the scancode's low and high byte, line 3's
the bits' low and high byte - lines 4 and 5 entry 1, and so on. After an even
number of key lines each below 256, the last line's spilled bytes are zeros,
so the next entry's scancode is 0 and `Pad_Read`'s walk stops there. The
frame holds 0x28 bytes of the array, ten entries; `Cfg_SetKeyTable` copies
0x80, so entries 10 to 31 are the return address and 0x54 bytes of WinMain's
frame (section 3).

Ours keeps the frame: `Cfg_Load` is naked asm that lays out the original's 0x3C
bytes with `edi` and `esi` saved under them, and calls a C++ body with the
frame's address. The body uses the same offsets, so `Key_Table` gets what it
got, garbage included, provided the caller's stack is the same - which it is
when the caller is the same WinMain.

**`Cfg_SetDefaultKeys`** (`0x5A9880`): `push esi edi`, `rep movsd` of 0x20
dwords from `Key_TableDefault` to `Key_Table`, `pop`, `ret`. Named for what
it does; its sibling `0x5A9860` is `Cfg_SetKeyTable`.

**`Disc_Probe`** - *what it probes and how* (the read pass's question). Two
steps: `fopen(local_file, "rb")` - WinMain passes `CAPCOM.AVI`, a file of the
install - and, if that opens, `fclose` it, store 0 in byte 0 of
`File_CdRootBuf` `0x66BC2C` (`"C:\"` in the image, so the root becomes the
empty string and `File_Open`'s paths relative to the current directory), and
answer 1. Otherwise the drive scan: `GetDriveTypeA` is read from its import
slot once, then for the ten letters `C`..`L` byte 0 of the root becomes the
letter and `GetDriveTypeA(root)` is called; on `DRIVE_CDROM` (5) only,
`sprintf(buf, "%s%s", root, disc_file)` - WinMain passes `BOF3.EXE` - into a
0x50-byte stack buffer, and `fopen(buf, "rb")`; opened: `fclose`, answer 1,
the root left naming that drive. No drive: 0, the root left at `L:\`, and
WinMain shows its insert-the-disc box (DIV-0039). No `GetLogicalDrives`, no
volume label: any CD-ROM with a `BOF3.EXE` at its root passes. Every stream
opened is closed again.

**`Game_Init`.** In order: `Gte_InitGeom`, `Gte_SetGeomOffset(0xA0, 0x78)`,
`Gte_SetGeomScreen(1000)` - the GTE's screen centre and distance (the symbol
entry called these "sound set-up, unread"; corrected) -
`DInput_Init(Game_HInstance, Game_Hwnd)`, `0x5A6830(Game_Hwnd)` (the sound
set-up, not ours; `Game_Hwnd` read again after `DInput_Init`),
`Port_DroppedCall(0)`, then the blocks: `Gpu_SetDefDrawEnv(block0 + 0x14, 0,
0, 320, 240)`, `Gpu_SetDefDispEnv(block0, 0, 240, 320, 240)`,
`Gpu_SetDefDrawEnv(block1 + 0x14, 0, 240, 320, 240)`,
`Gpu_SetDefDispEnv(block1, 0, 0, 320, 240)` - each block draws in one half of
VRAM and shows the other - `Gfx_InitBufferBlock` on each, `Gfx_BufferIndex` 0,
`Gfx_CurrentEnv` block 0, `Gfx_BeginFrame`, `Game_QuitFlag` 0, and a **jmp**
to `Task_SetStackBase`. That function stores `esp - 0x4000` as
`Task_StackTop`, and with a jmp the esp it sees is `Game_Init`'s at entry -
the task stacks hang from WinMain's frame. Ours: a naked entry that calls the
C++ body (which returns the jmp's target from `shell::g`) and then jmps, so
the esp is the original's.

**`Gfx_InitBufferBlock`** (`0x4FD200`): `Gpu_ClearOTagR(block + 0x70, 8)`,
then the bytes `+0x2C` = 1 and `+0x2D..+0x2F` = 0 - the DRAWENV's `isbg` and
its clear colour. So both blocks clear to black every frame
([`display-env.md`](display-env.md): `Gfx_Present` clears when `isbg` is
set).

**`Gfx_LinkOTags`.** For slot i of 8: `Gpu_AddPrim(Gfx_CurrentEnv + 0x70 +
4i, &Gfx_OtHeads[Gfx_BufferIndex * 8 + i], Gfx_OtPointers[i])` - the frame's
list for slot i (headed in `Gfx_OtHeads`, its tail pointer in
`Gfx_OtPointers` as the frame's draws left it) spliced onto the block's
ordering table. The symbol entry had the second and third arguments wrong
(`&Gfx_OtPointers[...]` and "a count dword at `0x929E30 + 4i`": the operand
is `0x929E30 + esi` with esi from 0x70, so it is `Gfx_OtPointers[i]`);
corrected. All three globals are read again in each pass, after the call
before.

## 3. Latent defects of Capcom's (described, kept)

Neither is reached by any recorded route; neither is fixed.

- **`BOF3.CFG` with key lines writes stack bytes into `Key_Table`**, and from
  the 21st line **corrupts `Cfg_Load`'s return address**. The array is 0x28
  bytes; the copy is 0x80. With key lines present, entries 10..31 are the
  return address into WinMain and 0x54 bytes of WinMain's frame, and the
  entries the lines did not reach are whatever the stack held - so the table
  can hold arbitrary "scancodes" (`Pad_Read` indexes the 256-byte `Key_State`
  by them, unchecked) unless the lines end it with a zero, which an even
  count of small values does by the spill. Line 21 (n = 20) stores its
  second integer's top byte over the return address's low byte; line 22
  more. The launcher passes lines 3+ through untouched and adds none
  ([`launcher-settings.md`](launcher-settings.md)), so only a hand-edited file
  reaches this. **Wants a ledger entry if fixed**: proposed - read the pairs
  into an array of 32 entries of our own, zero-filled, two lines an entry as
  the original packs them, and ignore lines past the 32nd entry; the
  difference is visible only to a player with key lines.
- **The disc probe takes any CD-ROM with a `BOF3.EXE` at its root** and stops
  at `L:`. Only when `CAPCOM.AVI` is missing from the current directory,
  which a full install never is. Not a defect worth a fix; noted.

## 4. The fuzz

`BOF3X_SHADOW=shell` ([`shell_fuzz.cpp`](../src/game/shell_fuzz.cpp)): each
function alone, 3,000 rounds a function, a byte-copy of Capcom's against ours
on the same seeded input. Every relative call out of the copy is re-aimed at
a recording stand-in, ours put on the same stand-ins through `shell::g`;
`Disc_Probe`'s `mov edi, [GetDriveTypeA]` has its disp32 (copy `+0x25`)
re-aimed at a slot of ours holding a recorder, the way `sound_fuzz.cpp`
re-aims its imports. The C runtime's stand-ins behave as the runtime would
where the caller reads the result: `fgets` writes the round's next line into
the buffer (at most n - 1 characters), `sscanf` stores 0, 1 or 2 ints (or
answers EOF) through the byte pointers it gets, `sprintf` joins its two
strings, `fopen` answers a stream or null. `atoi`, `GetDriveTypeA` and the
pad answer seeded values. Stand-ins may disturb what their caller reads after
them: the pad words, `Game_Hwnd`, `Game_HInstance`, the buffer index, the
current block, the slot pointers, the root's bytes, the block's DRAWENV bytes.

**One stack, one call site.** Every call is made by `Shell_CallOnStack` on a
64 KB stack of the fuzz's own, from the same instruction, with 0x900 bytes
around the top seeded per round. That is what lets `Cfg_Load` be compared
whole: its 0x80-byte copy reaches the return address and 0x54 bytes above,
which are then the same for both; for `Cfg_Load` the frame and everything
above it are compared byte for byte, and stack addresses in the log are
relative to the top - a frame laid out differently is a mismatch. `Game_Init`'s
`Task_SetStackBase` stand-in is naked: it records the esp it arrives with.

**Seeds.** Every compared region random per round, a third of its bytes zero.
`Cfg_Load`: 0..3 lines half the rounds (the count's boundaries: no line, the
two settings, the first key line), else 0..20 (20 the most before the return
address); a line of up to 19 characters, digits mostly; no file one round in
six. `Disc_Probe`: the local open failing one round in one to three, a CD-ROM
one drive call in one to six, the root `"C:\"` three rounds in four, the
local name the image's `CAPCOM.AVI` or ours. `Gfx_LinkOTags`: the buffer index
0 or 1 half the rounds, else any byte. `Input_Latch`: the old words all zero a
round in four.

**Compared per round:** `Cfg_Fullscreen`, `Cfg_RenderMode`, `File_CdRootBuf`,
`0x6BC620..0x6BC63B` (the handles to `Game_QuitFlag`), both key tables, the
six pad words, `Gfx_BufferIndex`, `Gfx_OtPointers`, `Gfx_CurrentEnv`, the
blocks handed in, `Cfg_Load`'s frame and above, the log of calls (each call's
arguments, the key table's 0x80 bytes by hash), and `Disc_Probe`'s result.

Result (2026-10-05): 21,000 rounds, **0 mismatches**; 1,661 rounds with key
lines, 65 with 20 lines; in all, 3,000 pad reads, 12,300 opens,
16,654 `fgets`, 10,334 `sscanf`, 1,357 key-table copies, 1,643 default
copies, 13,388 drive calls, 6,300 `sprintf`, 3,000 `Task_SetStackBase` arrivals.

## 5. Negative controls

Twenty-seven bugs planted one at a time in `shell.cpp`, each anchored on a
unique string, built and self-tested headless under `BOF3X_SHADOW=shell` (a
scratch driver, not committed), then removed and the build restored. 25
refused by a count (exit 3), 1 by a fault, 1 a change that changes nothing.

| Planted | Refused in (of 3,000) |
|---|---|
| `Input_Latch`: `Input_Pressed` without `& new` | 1,920 |
| `Input_Latch`: pad 2's `Previous` the new word | 2,231 |
| `Input_Latch`: `Input_Held` read before `Pad_Read` | 7 |
| `Cfg_Load`: the pairs at frame `+0x14 + 2n` | **a fault** (exit `0xC0000029`): line 20's second int reaches the return address |
| `Cfg_Load`: the pair pointers swapped | 1,357 |
| `Cfg_Load`: the pairs a byte higher (`+0x11`, `+0x12`) | 1,357 |
| `Cfg_Load`: the key table from two lines (`>= 2`) | 390 |
| `Cfg_Load`: the frame 0x40 bytes (asm) | 2,472 |
| `Cfg_Load`: no file leaves `Cfg_RenderMode` | 527 |
| `Cfg_Load`: `fgets` of 0x13 | 2,472 |
| `Game_Init`: `call`, not `jmp`, to `Task_SetStackBase` | 3,000 |
| `Game_Init`: `Game_Hwnd` read once | 87 |
| `Game_Init`: block 1 shows y 240 | 3,000 |
| `Game_Init`: `Gfx_BufferIndex` set after `Gfx_BeginFrame` | 44 |
| `Gfx_InitBufferBlock`: `isbg` 0 | 3,000 |
| `Gfx_InitBufferBlock`: seven slots cleared | 3,000 |
| `Gfx_InitBufferBlock`: the bytes before the clear | 1,491 |
| `Gfx_LinkOTags`: the buffer index read once | 473 |
| `Gfx_LinkOTags`: heads indexed `i * 8 + buffer` | 3,000 |
| `Gfx_LinkOTags`: the neighbouring slot's pointer | 3,000 |
| `Disc_Probe`: the root kept when the local file opens | 1,065 |
| `Disc_Probe`: letters `D`..`M` | 1,828 |
| `Disc_Probe`: the disc's stream left open | 725 |
| `Disc_Probe`: a RAM disk (6) taken for a CD-ROM | 789 |
| `Cfg_SetDefaultKeys`: 31 dwords | 3,000 |
| `Cfg_SetDefaultKeys`: the source two bytes on | 3,000 |

The fault is the control that moved the pairs up four bytes: in both copies
the fuzz's 20-line rounds then write over the return address. Its near
variants (swapped, one byte higher) are refused by a count.

Thin but refused: `Input_Held` read before `Pad_Read` (7 rounds - the pad
stand-in must disturb that one word), `Game_Hwnd` read once (87),
`Gfx_BufferIndex` after `Gfx_BeginFrame` (44): each needs a disturbance at one
call.

**Not refused, and it changes nothing:** `Disc_Probe`'s path buffer 0x40
bytes instead of 0x50. The strings never reach 0x40; the size is kept as the
original's for WinMain's names, and nothing compares the stack for
`Disc_Probe`. Also not planted, being unobservable: `GetDriveTypeA` read from
its slot at each call rather than once (nothing writes the slot).

## 6. Live coverage, and what nothing reaches

From the read pass's traces (`analysis/calltrace/platform_1005/`, eleven runs:
the attract sequence and the ten recorded routes) and
`analysis/platform_reach_1005.tsv`: **all seven are entered in every run** -
`Disc_Probe`, `Cfg_Load`, `Cfg_SetDefaultKeys` (caller `0x4FD0FF`, so
`BOF3.CFG` is missing or two lines here), `Game_Init`, `Gfx_InitBufferBlock`
once each at boot, `Input_Latch` and `Gfx_LinkOTags` once a frame. So the
state hash ([`state-hash.md`](state-hash.md)) checks them on every route,
frame for frame; it is the coordinator's after the merge.

Not reached by any route, fuzz only:

- `Cfg_Load` with key lines (`Cfg_SetKeyTable`), with a missing file - which
  of the two the owner's install takes is not measured, only that the
  default keys are copied - and with long lines.
- `Disc_Probe`'s drive scan: `CAPCOM.AVI` is in the install.
- Pad 2's words with a non-zero high word (`Pad_Read` never sets one).
- What the stack below `Cfg_Load`'s frame holds after it returns: ours runs
  a C++ body under the frame, the original its calls' arguments; nothing
  reads that area before writing it as far as the hash has ever shown.

**Rebinding.** WinMain's four calls (`Disc_Probe`, `Cfg_Load`, `Game_Init`,
`Gfx_LinkOTags`) and `input_script.cpp`'s two (`DeviceLatch`, `HashedLatch`)
go through `bof3::orig::`, so `BOF3X_ORIGINAL=Name` still restores Capcom's for
each under our WinMain, as its `Fmv_Play` and `Display_Setup` calls already
did; `input_script.cpp`'s three `kInputLatch = 0x4FC6A0` (the expected callee
of `RetargetCall` at `0x4FCDDE`) are `bof3::addr::Input_Latch`. No raw address
of the seven was left elsewhere in `src/` (grep, 2026-10-05). `0x5A6830` is
called raw through `shell_callees.h` (`kSoundSetup`) and is the sound group's
to name.

## 7. For `analysis/calltrace/entries_logic.txt`

All seven are listed already, every size right (checked 2026-10-05 against
the reads above): nothing to add.

```
004FC6A0 4E
004FD030 D4
004FD110 E2
004FD200 24
004FD290 4A
005A72C0 B0
005A9880 16
```

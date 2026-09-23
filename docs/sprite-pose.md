# Sprite animation, facing and the effect pool

**Status:** IN PROGRESS (2026-09-22 — ten functions ours, fuzzed headless;
through the live batch `ab24`)

**The live batch, 2026-09-22 (`ab24`, `analysis/validate_ab24.sh`):** the whole third round - groups H, J, K, L and M, 137 functions, 466 ours - checked at once, original against ours: the field, new-game, field-menu and menu-screens capture pairs identical (4, 9, 7, 5 of each), the 9-minute attract 55 of 55 (and 55 of 55 against `ab22`'s ours), the same attract in English 55 of 55, the oracle identical at all 7,478 compared frames, the memory dump identical, and the frame hash identical on all 10,063 frames (`ab24_orig` / `ab24_oursb`, beside an original-vs-original pair identical on 10,062).

What a sprite object does when something asks it to play an animation or
face a way: `Sprite_SetAnimationAt` `0x589200` and its three front doors
(`Sprite_SetAnimation`, `Sprite_EnsureAnimation`, `Sprite_FaceDirection`),
the two things it calls to get a texture and a frame
(`Sprite_SetAnimationBank`, `Sprite_SetFrameQueueUpload` — the feed side of
[`known-defects.md`](known-defects.md) D4), and, next to them in the image,
the four functions that keep the 20-object effect pool (`Effect_FindFree`,
`Effect_Release`, `Effect_ReleaseAt`, `Effect_ClearAll`).

Group M of the third parallel round (`src/game/sprite_pose.cpp`, the fuzz in
`src/game/sprite_pose_fuzz.cpp`, shadow name `sprite_pose`). Everything here
is a *faithful* replacement: no `DIVERGENCE.md` entry is owed, and the
unbounded upload append is kept exactly as it was.

## 1. The functions

Extents measured by capstone, 2026-09-22 (`python tools/pe_disasm.py`, and a
range dump for the neighbours); PSX twins from `analysis/pairs_propagated.json`
/ `symbols.toml` and each read in the sibling's `SLPS_009.90` (main
executable) or `GAME_EMI0` overlay with capstone MIPS. Calls: attract trace
`analysis/calltrace/hidden_b/bof3x.callcounts.tsv`.

| PC | bytes | PSX | name | attract calls | what it does |
|---|--:|---|---|--:|---|
| `0x5891F0` | 0x10 | `0x8014D5AC` | `Sprite_SetAnimation` | 705 | `Sprite_SetAnimationAt(a, 0)` |
| `0x589200` | 0x12F | `0x8014D5D0` | `Sprite_SetAnimationAt` | 793 | the animation, the member's version of it, the texture, the frame, the script |
| `0x589330` | 0x1E | `0x8014D7C8` | `Sprite_EnsureAnimation` (new name) | 12 | 1 if already playing it; else set it and 0 |
| `0x5894D0` | 0xBD | `0x8014DAFC` | `Sprite_SetFrameQueueUpload` | 12 | queue the frame's image (no bound), +0x2A, the script pointer |
| `0x589590` | 0xCE | `0x8014DC30` | `Sprite_SetAnimationBank` | 63 | find a bank record, copy its texture fields; al 0 / 1 |
| `0x57C4C0` | 0x84 | `0x8015C4B4` | `Sprite_FaceDirection` | 358 | a direction to an animation and a pose byte |
| `0x589810` | 0x2C | `0x8019701C` | `Effect_FindFree` (new name) | 15 | first free of the 20 effect objects, or 0xFF |
| `0x589840` | 0x2F | `0x80197070` | `Effect_Release` | 12 | bytes 0..4 of the current object to 0 |
| `0x589870` | 0x2D | `0x801970C0` | `Effect_ReleaseAt` (new name) | 300 | the same by index, unbounded |
| `0x5898A0` | 0x23 | `0x8019711C` | `Effect_ClearAll` | 15 | `Effect_ReleaseAt(0..19)` |

The catalogue's sizes (`pe_funcs.py`) were all right for these ten, and
`analysis/calltrace/entries_logic.txt` already lists all ten with these
sizes (§7). None has a jump table, an indirect call, or a jump that leaves
it. The PSX twins match branch for branch; the port's differences are listed
in §4.

## 2. Signatures, and what the callers read

Two entries in `symbols.toml` were wrong in a way that would have changed
behaviour, and are corrected with the takeover:

- **`Sprite_SetAnimationAt`'s start is 16 bits, not a byte.** The original
  pushes its second argument's whole dword on to `Sprite_ScriptStart`, which
  reads a word of it (the PSX masks with `0xFFFF`). `MoveScript_Flow` and
  `Sprite_FaceDirection` happen to pass byte values (a `movzx ax, byte` and
  a `movzx cx, cl`), but 20 other call sites are not ours and were not all
  read.
- **`Sprite_SetAnimationBank` returns al**: 0 when the bank was found, 1 when
  it was not (and nothing was stored). Two callers test it (`0x46C857`,
  `0x46CE02`, both `test al, al` straight after the call).

Changing those two types touched the typed callee members of the four
already-ours files that call them — `move_script.cpp` (`set_animation_at`),
`move_groups.cpp`, `event_script_callees.h` and `field_event_callees.h`
(`set_bank`) — and their stand-ins' signatures (`move_script.cpp`,
`move_groups.cpp`, `event_script_fuzz.cpp`, `field_event_fuzz.cpp`); nothing
else in them. Every one of those files' self-tests still passes.

**What of eax each caller reads** was checked by scanning every E8 call site
of each function in `.text` and following the code after it until eax (or
al, or ah) is written or read (a scratch capstone script, 2026-09-22):

| function | sites | read al | read more of eax | return it on (not chased) |
|---|--:|--:|--:|--:|
| `Sprite_SetAnimation` | 311 | 0 | 0 | 102 |
| `Sprite_SetAnimationAt` | 22 | 0 | 0 | 6 |
| `Sprite_EnsureAnimation` | 241 | many | 1 (`0x52E7D5`: overwrites ax first, the callee reads a word) | 59 |
| `Sprite_SetAnimationBank` | 114 | 2 | 0 | 8 |
| `Sprite_FaceDirection` | 25 | 0 | 0 | 4 |
| `Effect_FindFree` | 404 | 398 | 6, all `and eax, 0xFF` | 0 |
| `Effect_Release` | 209 (most `jmp`) | 0 | 0 | 32 |
| `Sprite_SetFrameQueueUpload` | 2 | 0 | 0 | 0 |

So the upper bytes of eax — which the originals leave as whatever the path
left there (the caller's own eax for `Effect_FindFree`, `Sprite_Current`'s
upper bytes for a found bank) — are not reproduced, and no call site seen
reads them. The sites that return the value on to *their* caller were not
followed further; that is the one unchecked edge of the signatures.

## 3. What the sprite object's fields mean here

The object is still addressed by offset (as in `sprite_anim.cpp` and
[`sprite-draw-order.md`](sprite-draw-order.md) §5-6). What these ten
establish, each from the code that uses it:

| offset | what |
|---|---|
| `+0x00` bit 1 | no image upload for this object (`Sprite_SetFrameQueueUpload`) |
| `+0x06` | type: 8 and up never turn; 6 takes a "direction" as an animation number |
| `+0x24` bit 0 | frames are uploaded to VRAM (`Sprite_SetFrameQueueUpload`), else the script is found in place through `0x9039D8` |
| `+0x24` bit 1 | a party member's set-up is pending: `+0x64` / `+0x68` hold the member and the slot row, and the next plain animation calls `Field_MemberSprite` first |
| `+0x25` `+0x26` | texture slot and its y row (a slot above 0x10 is the upper half: x from slot − 0x10, y + 0x100) |
| `+0x27` `+0x28` `+0x2B` | set from a bank record and the bank's two bytes at `0x6690A0` |
| `+0x2A` | a pose byte: the second of a direction pair, a member record's byte 3, a frame entry's byte 2 |
| `+0x2C` | u16: the frame-offset table's column, the party-set column, the bank record's byte 7 |
| `+0x4B` | the current animation |
| `+0x4C` / `+0x50` | the animation data (three offsets: images, script words, 3-byte frame entries) / the script pointer |
| `+0x64` / `+0x68` | dword: the party member / its y row (slot × 80) |
| `+0x70` | dword; its low byte is ORed from the bank's first byte after being cleared |

The data they read, none of it named in `symbols.toml` yet (constants in
`sprite_pose.h`):

| address | what |
|---|---|
| `0x90412C` | the loaded party set; `& 0x7F` picks a row (bit 7 from `PartySet_Select` mode 0) |
| `0x669750` | the 19 party-set rows, 3 bytes each: the member in each column |
| `0x669884` | 11 pointers, one per member: arrays of 4-byte records {u16 bank, animation, +0x2A} indexed by `animation & 0x7F` |
| `0x9039D8` | pointer: a dword offset per `+0x2C`, then u16 offsets per animation |
| `0x8C3580` | dword; its low byte counts the bank records (PSX `0x800E3800`, a byte) |
| `0x7E0880` | pointer to the 8-byte bank records |
| `0x6690A0` | two bytes per bank number |
| `0x65F5BC` | two bytes per direction: the animation, the pose byte (the first 16 pairs hold animations 0..6; which directions the game passes is not established) |
| `Field_ActiveMember` `+0xA1` | the step animations 5 and 6 start at when facing; 0xFF for none (PSX `+0x95`) |

## 4. The PC against the PSX

Read side by side, 2026-09-22. The ten are the PSX's, with these
differences, all kept on the PC side (this project reproduces the *PC*
release):

- `Sprite_SetAnimationBank`: the PSX re-reads the record count every turn of
  the search; the PC reads it once. And the PSX's `+0x70` store is a whole
  word of the bank byte (`and` with `-0x100`, then `sw` of the byte over it -
  the AND is dead), where the PC clears the low byte and ORs the bank byte
  in, **keeping `+0x70`'s upper three bytes**. What those bytes hold is
  unread; `Field_MemberSprite` also keeps them.
- The sprite fields and the active-member record are laid out further on in
  the PC (`+0xA1` for the PSX's `+0x95`), and the effect records are 0x80
  bytes against the PSX's 0x74.
- `Sprite_SetAnimationAt` re-reads `Sprite_Current` (the PSX's scratchpad
  word `0x1F800044`) at the same points on both.

**Quirks kept, each run as a control (§6):** the unbounded upload append
(D4) and its byte count wrapping from 0xFF to 0; the `xor 3` on `+0x24`
that flips bit 0 as well as setting bit 1; the member's slot row divided
*signed* by 80; `+0x2A` written through the pointer read on entry and
`+0x50` through a fresh one; type 6's direction passed straight through as
an animation; `Effect_ReleaseAt`'s index not checked against the 20; the
first matching bank record winning.

## 5. The fuzz

`BOF3X_SHADOW=sprite_pose`, at start-up (`src/game/sprite_pose_fuzz.cpp`).
**Two phases, twenty byte-copies:**

1. **Alone** — each of the ten copied on its own, every call out re-aimed at
   a recording stand-in (`bof3::CloneCall` with `expected`), ours routed to
   the same stand-ins through `sprite_pose::g`. 1,500 rounds each.
2. **Chained** — the copies call each other: `Sprite_FaceDirection`'s copy
   reaches the copies of `Sprite_EnsureAnimation`, `Sprite_SetAnimation`,
   `Sprite_SetAnimationAt`, `Sprite_SetAnimationBank` and
   `Sprite_SetFrameQueueUpload`; `Effect_ClearAll`'s the copy of
   `Effect_ReleaseAt`. Ours call ours. Only `Field_MemberSprite` and
   `Sprite_ScriptStart` — other files' — stay stand-ins. 1,500 rounds from
   each of five entries.

Each round seeds four sprite objects of 0xA4 bytes at random, then each
branch's boundaries: type 0..9, 6 and 8 among them; slot 0x0F / 0x10 / 0x11 /
0x20 / 0xFF; the slot row at 0, 79 / 80 / 81, 159 / 160, −1 / −79 / −80 / −81,
`0x7FFFFFFF`, `0x80000000`; the upload count at 0, 1, 0x13 / 0x14 (the
arrays' 20), 0x27 / 0x28, 0x7F, 0xFE / 0xFF (the byte's wrap); the bank count
at 0, 1, 0x7F / 0x80, 0xFF with random upper bytes, bank numbers from a small
set so records repeat, and the searched-for bank at the last record searched
and the first one not; the active member's byte at 0xFF half the time; the
effect pool all taken a third of the time; an animation equal to the
current one a third of the time; every byte argument with random upper
bytes, as a caller's register would have. Theirs runs, then ours from the
same state; the four sprites, the effect pool (0x40 records), the upload
queue's arrays (all 256 slots' worth), its count, `Sprite_Current`, the
result (as much of eax as the function defines) and the stand-ins' log are
compared.

The tables the ten index — the member pointers, the party-set rows, the
frame offsets, the bank records and count, the active member pointer — are
swapped for buffers of the fuzz's own and put back afterwards. The
direction pairs and the bank bytes are read where they are. **The inputs are
bounded, not the functions**: the party row inside the 0xC0 bytes swapped,
members inside the 11 pointers, `+0x2C` below 4 (it indexes the offsets),
effect indices below 0x40. The upload count takes every value, so the D4
append writes where it would in the game, inside the regions the fuzz
snapshots and restores.

    shadow  sprite_pose self-test: alone 15000 rounds (1500 per function),
            36924 calls to the stand-ins, 0 MISMATCHES; chained 7500 rounds
            over 5 entries, 6316 calls to the two stand-ins, 0 MISMATCHES
    shadow  sprite_pose coverage (alone): SetAnimationAt bank path 747
            (member worked out 137), pending member 353; queue skipped 768,
            wrapped from 0xFF 34, high slot 533; bank found 951 / not 549;
            face: no turn 242, type 6 197, no start 572, already 174, turned
            315 (5 or 6 with a start 59); all effects taken 382
    shadow  sprite_pose coverage (chained): 1224 rounds queued an upload;
            SetAnimationAt bank path 776 (member worked out 65), pending
            member 318; face: no turn 235, type 6 195, no start 527, already
            469, turned 74

### The stand-ins are not quiet

Every stand-in records what its callee reads of its arguments (a byte, or 16
bits) and `Sprite_Current` at the call, and then a `Disturb()` may **repoint
`Sprite_Current` at another of the four sprites**, or change the current
one's `+0x24`, `+0x2C`, `+0x4B` or `+0x64` — each a field the caller reads
again after the call. That is what makes the re-read controls fail.
`Sprite_EnsureAnimation`'s and `Sprite_SetAnimationBank`'s stand-ins return
0 or 1 by the round's hash.

## 6. Negative controls

Forty-four planted bugs, one at a time, each rebuilt and run under
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=sprite_pose` by a scratch driver (the
source restored after each). **Forty-three were refused by a count of
mismatches**; one faulted instead (below). The counts are rounds, not
calls: "alone" is out of 1,500 rounds of the function the bug is in,
"chained" out of 7,500 over the five entries (a function reached through
several entries can exceed 1,500).

| the bug | alone (of 1,500 a function) | chained |
|---|--:|--:|
| `Sprite_SetAnimation`: start 1, not 0 | 1500 | 3330 |
| `SetAnimationAt`: the member path on bit 6, not bit 7 | 762 | 2118 |
| `SetAnimationAt`: party-set rows 2 bytes apart, not 3 | 333 | 984 |
| `SetAnimationAt`: +0x24 xor 2, not xor 3 | 296 | 1098 |
| `SetAnimationAt`: +0x68 from +0x25, not +0x26 | 366 | 1094 |
| `SetAnimationAt`: +0x4B from the record's byte 3, not 2 | 742 | 2099 |
| `SetAnimationAt`: the record index keeps bit 7 | 747 | 2109 |
| `SetAnimationAt`: Sprite_Current not re-read after Sprite_SetAnimationBank | 193 | 0 |
| `SetAnimationAt`: the slot row divided by 81, not 80 | 264 | 1065 |
| `SetAnimationAt`: the slot row divided unsigned | 175 | 532 |
| `SetAnimationAt`: old +0x24 bits kept & 0xFE, not & 0xFC | 66 | 264 |
| `SetAnimationAt`: Sprite_Current not re-read after Field_MemberSprite | 56 | 253 |
| `SetAnimationAt`: the upload on +0x24 bit 2, not bit 0 | 755 | 2470 |
| `SetAnimationAt`: the frame-offset table indexed by +0x2C * 2, not * 4 | access violation (0xC0000005) | - |
| `SetAnimationAt`: the script found from the offsets' base, not the column's table | 765 | 2456 |
| `SetAnimationAt`: the animation's word one further on | 765 | 2456 |
| `SetAnimationAt`: start passed as a byte on the upload path | 733 | 765 |
| `Sprite_EnsureAnimation`: 1 after setting, not 0 | 998 | 988 |
| `SetFrameQueueUpload`: the upper-half wrap at slot >= 0x10, not > 0x10 | 39 | 56 |
| `SetFrameQueueUpload`: the upper half's y + 0x80, not + 0x100 | 533 | 933 |
| `SetFrameQueueUpload`: the count up by 2 | 732 | 1224 |
| `SetFrameQueueUpload`: the append BOUNDED at 20 (the D4 fix, as a control) | 564 | 956 |
| `SetFrameQueueUpload`: +0x2A from the entry's byte 1, not 2 | 1495 | 2457 |
| `SetFrameQueueUpload`: the frame masked to 7 bits | 772 | 529 |
| `SetFrameQueueUpload`: no upload on +0 bit 0, not bit 1 | 758 | 1245 |
| `SetFrameQueueUpload`: the image offset table at 2 bytes an entry | 729 | 1221 |
| `SetAnimationBank`: the count masked to 7 bits | 139 | 151 |
| `SetAnimationBank`: one record past the count searched | 210 | 25 |
| `SetAnimationBank`: +0x70's upper bytes cleared (the PSX's store) | 951 | 1089 |
| `SetAnimationBank`: 1 when found | 951 | 0 |
| `SetAnimationBank`: +0x27 from the record's +3, not +2 | 947 | 1085 |
| `SetAnimationBank`: +0x2B from the bank's first byte, not second | 161 | 181 |
| `FaceDirection`: types 9 and up do not turn, not 8 | 20 | 13 |
| `FaceDirection`: type 7 takes the direction as an animation, not 6 | 360 | 359 |
| `FaceDirection`: no start on 0xFE, not 0xFF | 576 | 67 |
| `FaceDirection`: animations 5 and 7 start at a step, not 5 and 6 | 32 | 63 |
| `FaceDirection`: +0x2A from the pair's first byte | 893 | 611 |
| `FaceDirection`: no 'already playing it' test | 174 | 173 |
| `FaceDirection`: the start from +0xA0, not +0xA1 | 633 | 158 |
| `Effect_FindFree`: 19 objects searched, not 20 | 51 | 0 |
| `Effect_FindFree`: 0 when none is free, not 0xFF | 382 | 0 |
| `Effect_Release`: 4 bytes cleared, not 5 | 1013 | 0 |
| `Effect_ReleaseAt`: the index kept to 5 bits | 739 | 0 |
| `Effect_ClearAll`: 19 objects, not 20 | 1500 | 1255 |

**Read with the table:**

- *The frame-offset table indexed by `+0x2C * 2`* was refused by an access
  violation, not a count: a misaligned read of the fuzz's offset table gives
  a wild offset, and the script lookup then faults - the original's own
  behaviour on such data. A control refused by a fault proves less than one
  refused by a count (`HANDOFF.md`), so the next row replaces it: the script
  found from the offsets' base instead of the column's table, refused in 765
  rounds.
- Zero in the chained column is expected where the chain cannot see the
  bug: the copies do not move `Sprite_Current` (the re-read after
  `Sprite_SetAnimationBank`), no chained caller reads the bank's or
  `Effect_FindFree`'s result, and the effect functions other than
  `Effect_ClearAll` are not chain entries.
- *`Effect_FindFree`: 19 objects searched* was **not refused at first**
  (0 of 1,500): with each object free one time in four, object 19 is the
  first free almost never. The fuzz now also takes all 20 and frees exactly
  one - 0, 1, 18, 19 or any - and refuses it in 51 rounds.
- *The upload append bounded at 20* - the D4 fix, planted as a bug - is
  refused in 564 rounds: the fuzz would notice if the defect were quietly
  fixed here instead of in the ledger.
- Not planted, because no input could tell: `Sprite_Current` re-read
  between stores with no call between them, and `+0x2C` read as a word
  rather than a byte (the fuzz keeps `+0x2C` below 4, which it must - it
  indexes the offsets unchecked).

## 7. For the batch check

`analysis/calltrace/entries_logic.txt` **already lists all ten, with these
sizes** (checked 2026-09-22) — nothing to add:

    0057C4C0 84
    005891F0 10
    00589200 12F
    00589330 1E
    005894D0 BD
    00589590 CE
    00589810 2C
    00589840 2F
    00589870 2D
    005898A0 23

`BOF3X_ORIGINAL` takes the ten names: `Sprite_SetAnimation`,
`Sprite_SetAnimationAt`, `Sprite_EnsureAnimation`,
`Sprite_SetFrameQueueUpload`, `Sprite_SetAnimationBank`,
`Sprite_FaceDirection`, `Effect_FindFree`, `Effect_Release`,
`Effect_ReleaseAt`, `Effect_ClearAll`.

**What the live batch should look at.** The attract sequence calls
`Sprite_SetAnimationAt` 793 times, so animation changes are all over it:

- the **memory dump** is the direct check: every sprite object's `+0x24`
  .. `+0x2C`, `+0x4B`, `+0x50` and the effect pool are in the arena it
  compares, and the upload queue is empty at a dump (drained by the flush).
- the **frame hash** and the **oracle** as usual; the nine-minute attract
  capture would show a wrong texture slot or pose as a visibly wrong frame.
- `Sprite_SetFrameQueueUpload` runs only 12 times in the cycle (from the
  upload path of `0x5892EB`); the queue's contents reach the screen only
  through `Gfx_FlushUploadQueue`, so a wrong x / y / record would show as a
  wrong image in VRAM — the `vram` region of the dump.

## 8. What no check reached

From the attract trace's callers (`hidden_b`):

- **The member-animation path** of `Sprite_SetAnimationAt` (animations with
  bit 7): `Sprite_SetAnimationBank` is never called from `0x58926E`, so the
  attract plays no member-specific animation; nor is `Field_MemberSprite`
  called from `0x5892B9`, so the pending member set-up never runs either.
  Walking a party member (`tools/recipes/field_view.txt`) probably does both;
  unconfirmed.
- `Sprite_FaceDirection`'s type-6 and "no start" (`+0xA1` = 0xFF) paths:
  `Sprite_EnsureAnimation` is called 12 times, all from
  `Field_LeaderAnimation` (`0x5305C3`), none from `0x57C538` / `0x57C542`.
- `Sprite_SetAnimationBank`'s "not found" answer, and the two callers that
  test it (`0x46C857`, `0x46CE02` — neither in the trace).
- `Effect_ReleaseAt` from its other caller `0x52CE45` (not in the trace; it
  is a loop in `0x52CE20` that releases objects 7..19 after clearing bytes
  1..4 of objects 0..6 inline - their in-use byte left set - and then sets
  object 1's state byte to 2; so neither caller in the image passes an index
  past 19, and the missing bound is harmless as the code stands).
- An upload count past 20 — that is the D4 pile-up, which DIV-0004's drain
  keeps from happening in the ordinary run.

All of that is covered by the fuzz only.

## 9. Open

- `+0x70`'s upper bytes (kept by the PC, cleared by the PSX, §4): what reads
  them.
- The 20 of `Sprite_SetAnimationAt`'s 22 call sites not read, and whether
  any passes a start of 0x100 or more.
- The data in §3 wants names in `symbols.toml` once a second reader
  confirms each.

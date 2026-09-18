# Kinship probe — the text engine

**Status:** STABLE (measured 2026-09-18)

The load-bearing experiment from [`PLAN.md`](PLAN.md) §8 step 2: *if two binaries
are compilations of one source, can PSX-side names be transferred onto the PC
binary?* Run on the text engine, because the sibling repo's
[`PC_PORT_CROSS_REFERENCE.md`](../../BreathOfFire3Recomp/docs/PC_PORT_CROSS_REFERENCE.md)
had already established three landmarks there.

## Result

**Pass, and by a wider margin than the plan assumed.**

The plan's hypothesis was call-graph isomorphism up to inlining — a statistical
signal, needing a matcher and producing hypotheses. What the binary actually
shows is stronger and cheaper to exploit:

1. **Function bodies are the same algorithm, operation for operation.** Not
   merely similar shape — the same constants, the same field order, the same
   branch structure. Recognisable by eye.
2. **Globals keep their relative layout.** A group of related globals — a struct,
   or the statics of one translation unit — occupies one contiguous block in both
   binaries with **identical internal offsets**, at a per-block constant delta.
   The blocks were reordered by the linker; their contents were not.

(2) is the important one, because it means name transfer does not have to start
from the call graph at all. One verified function pair yields the delta for
whichever global block it touches, and that delta names *every* field in the
block at once — which in turn names the functions that touch those fields.

**Phase 1 is real.** See *What this changes* below for how it changes shape.

---

## Evidence

Tooling written for this probe, all in [`tools/`](../tools):
`pe_funcs.py` (function inventory), `pe_disasm.py` (disassemble by virtual
address), `pe_xref.py` (global cross-reference index). Outputs land in
`analysis/` and are never committed (`CLAUDE.md` rule 1).

Baseline sweep of `BOF3.exe` (2,584,576 bytes, 2001-04-18,
sha256 `15984158…31dfb8cb`), `python tools/pe_funcs.py`:
**591,984 instructions, 119 stray data bytes, 3,004 distinct direct-call targets
in `.text`, 2,952 recovered function bodies** (median 76 instructions, p90 457).
This independently reproduces PLAN §1's figures; the call-target count is lower
than §1's 3,276 because this one counts only targets landing inside `.text`.

### 1. `Msg_SystemPtr` — the same function, twice

PSX `Msg_SystemPtr` `0x801503F8` (`symbols.toml`, status `confirmed`):
`block = 0x80014000 + u32[0x80014000 + ((id >> 12) & 0xC)]`, returns
`block + u16[block + 2*(id & 0x3FFF)]`.

PC `0x497740`, disassembled:

```
mov  eax, [esp+4]        ; id
and  eax, 0xffff
mov  ecx, eax
and  eax, 0x3fff         ; id & 0x3FFF
shr  ecx, 0xe            ; id >> 14
mov  ecx, [ecx*4 + 0x807580]
add  ecx, 0x807580       ; block
mov  dx, [ecx + eax*2]
add  eax, ecx            ; block + u16[block + 2*(id & 0x3FFF)]
ret
```

`(id >> 12) & 0xC` and `(id >> 14) * 4` are the same address computation. Same
selector bits, same mask, same halfword indirection, same base-relative return.
The only difference is the pool's address: PSX `0x80014000`, PC `0x807580`.

### 2. The message-box interpreter state block

PC `0x497770` (reached by a direct call from `0x4976D0`) zeroes a run of globals
and keeps exactly one flag bit. Laid against `MsgBox_Reset` `0x8015042C`'s
`confirmed` note in `symbols.toml`, every field lines up at
**PC = PSX + 0x80695DA4**:

| PSX | PC | What, and how it was matched |
|---|---|---|
| `0x8014909C`–`9F` | `0x7DEE40`–`43` | four state bytes, zeroed as four byte stores in both |
| `0x801490A0` | `0x7DEE44` | flag word — `and word [0x7DEE44], 0x40` keeps only bit `0x40`, exactly the PSX note |
| `0x801490A2` | `0x7DEE46` | per-glyph delay counter, zeroed |
| `0x801490A4` | `0x7DEE48` | message index — written by `0x4976D0` from its argument |
| `0x801490A6` | `0x7DEE4A` | zeroed at open |
| `0x801490A8` | `0x7DEE4C` | box string base |
| `0x801490AC` | `0x7DEE50` | stepper pointer (base + 4, in both) |
| `0x801490B4`–`B7` | `0x7DEE58`–`5B` | zeroed block of four |
| `0x801490BC` / `BE` | `0x7DEE60` / `62` | box origin x / y |
| `0x801490C4` | `0x7DEE68` | zeroed at open |
| `0x801490CA` | `0x7DEE6E` | the leading `0x0C` argument — see below |

Twelve fields, one delta, no exceptions. The box origin pair is corroborated
independently: PC `0x4977F0` computes it as `(window x >> 4) + 10`,
`(window y >> 4) + 6`, which is verbatim `MsgBox_FrameTask` `0x80150508`'s note.

### 3. The control-code dispatch

`MsgBox_Step` `0x8015096C` switches on control codes `0x00`–`0x16`. PC `0x497840`:

```
mov  cl, [esi]
and  eax, 0xff
cmp  eax, 0x16
ja   default
jmp  [eax*4 + 0x497a70]     ; 23-entry table
```

Same upper bound, same vocabulary size. Inside it, at `0x497D97`:

```
xor  word ptr [0x7dee44], 0x10
```

— which is the PC's copy of `DAT_801490a0 ^= 0x10`, the single toggle shared by
codes `0x10` and `0x11` that the cross-reference document's §2.1 used to prove
they are instant-print start/end. That proof rested on the PSX decompile plus a
6,696-message disc census; this is a **third, independent confirmation** from the
PC binary itself, and unlike `bof3ext`'s table it is machine code rather than a
translation vocabulary.

### 4. Window records — the `0x24` stride survives

PC `0x59E2D0`:

```
and  ecx, 0xff
lea  ecx, [ecx + ecx*8]
shl  ecx, 2                      ; slot * 0x24
cmp  byte [ecx + 0x803160], 0    ; free?
mov  byte [ecx + 0x803160], 1    ; claim
mov  byte [ecx + 0x803161], kind
mov  byte [ecx + 0x803162], 0
mov  byte [ecx + 0x803163], 0
...                              ; else return 0xFF
```

`Window_Alloc` `0x80159874`'s note: *"claims window record `0x8014832C + slot*0x24`
if its byte 0 is free (sets 1, +1 = kind, +2/+3 = 0), returns slot or `0xFF`;
`MsgBox_Reset` calls it as (0, 0)."* Byte for byte, including the stride and the
sentinel. And PC `0x497770` does call it as `(0, 0)`.

### 5. Global block deltas are per-block, not global

```
msgbox state        PSX 0x8014909C -> PC 0x7DEE40     delta 0x80695DA4
window records      PSX 0x8014832C -> PC 0x803160     delta 0x806BAE34
script msg pool     PSX 0x80010000 -> PC 0x803580     delta 0x807F3580
system msg pool     PSX 0x80014000 -> PC 0x807580     delta 0x807F3580
character records   PSX 0x80144964 -> PC 0x64B390     delta 0x80506A2C  (cross-ref §1)
```

Four distinct deltas over five blocks — the linker reordered the blocks, as it
must. But the two message pools share a delta *exactly*, because they are one
region: the pools sit `0x4000` apart in both binaries.

So the unit that transfers is the **block**, and a block is roughly what one C
translation unit's statics occupy. That is the granularity the matcher should
work at.

---

## Pairs established

Eight functions, transferred from `BreathOfFire3Recomp/symbols.toml` onto the PC
binary and recorded in [`symbols.toml`](../symbols.toml).

| PSX | Name | PC | Tier |
|---|---|---|---|
| `0x8015034C` | `Msg_OpenScript` | `0x4976D0` | evidence |
| `0x801503AC` | `Msg_OpenSystem` | `0x497710` | evidence |
| `0x801503F8` | `Msg_SystemPtr` | `0x497740` | evidence |
| `0x8015042C` | `MsgBox_Reset` | `0x497770` | evidence |
| `0x80150508` | `MsgBox_FrameTask` | `0x4977F0` | evidence |
| `0x8015096C` | `MsgBox_Step` | `0x497840` | evidence |
| `0x80159874` | `Window_Alloc` | `0x59E2D0` | evidence |
| `0x80159F00` | `Window_Task` | `0x595450` | hypothesis |

`bof3ext` names three of these (`GetText` `0x497740`, `LoadDialogue` `0x4976D0`,
and `0x497770`), which is a useful cross-check but was not the source: each pair
above was derived from the PSX side and verified against the PC disassembly
(`CLAUDE.md` rule 5, and the evidence rule).

`Window_Task` is `hypothesis`, not `evidence`: PC `0x595450` calls
`MsgBox_FrameTask` and drives the same window record, which is what `Window_Task`
`0x80159F00` does, but at 613 instructions it is far larger than its PSX
counterpart and is more likely a driver with `Window_Task` inlined into it.

---

## Answering the sibling's open `0x0C` question

`PC_PORT_CROSS_REFERENCE.md` §2.2 leaves one question open: *nothing in the PSX
boot EXE reads `0x801490CA`, so an overlay does, and that read is the thing that
would name this code.* The candidate names were "speaker/portrait id" (ours) and
"textbox type" (`bof3ext`'s), with the disc census favouring the latter.

The delta above predicts the PC address of that byte: `0x7DEE6E`. Cross-reference
(`python tools/pe_xref.py 0x7dee6e`) finds exactly two writers — both in
`MsgBox_Reset` `0x497770`, storing the leading `0x0C` argument or `0` — and
exactly two readers, both inside `0x595450`:

```
mov  al, byte ptr [0x7dee6e]
mov  cx, word ptr [eax*4 + 0x66ae10]    ; table[n].x
add  cx, 0x23
shl  ecx, 4                             ; 12.4 fixed point
mov  word ptr [edx + 4], cx             ; window record +4  = x

mov  al, byte ptr [0x7dee6e]
mov  cx, word ptr [eax*4 + 0x66ae12]    ; table[n].y
add  cx, 0x10
shl  ecx, 4
mov  word ptr [edx + 6], cx             ; window record +6  = y
```

`+4` / `+6` of the window record are the box position in 12.4 fixed point
(`Window_Alloc` `0x80159874`, `Window_Task` `0x80159F00`). So:

> **`0x0C nn` selects the dialogue box's on-screen position, from a table of
> `(x, y)` pairs indexed by `nn`.**

That confirms `bof3ext`'s "textbox type" over our "speaker/portrait id", makes it
precise, and explains the census distribution §2.2 found: two values carrying 83%
of head uses is exactly what a *placement* selector looks like — most lines are
in the standard bottom box, a few are repositioned.

The PSX-side prediction that follows: the overlay reader of `0x801490CA` will
index a 4-byte-stride `(x, y)` table and write window record `+4`/`+6`. **This is
corroboration, not proof, and the rule is unchanged in that direction — the disc
decides what the PSX version does** (`CLAUDE.md` rule 6). Worth sending to the
sibling repo as a lead with a specific shape to look for, not as an answer to
paste in.

---

## What this changes

For [`PLAN.md`](PLAN.md) §3 and phase 1:

- **The matcher's primary signal should be global block deltas, not call-graph
  shape.** Seed with a handful of hand-verified function pairs; each yields a
  block delta; each delta names a block of fields; the PC functions touching
  those fields are then candidates for the PSX functions touching their
  counterparts. Iterate. Call-graph shape becomes a *check* on the propagation
  rather than the engine of it.
- **A matched pair is close to free to verify.** Bodies are recognisable by eye
  because the algorithms are identical, so `evidence`-tier naming does not
  require a decompile of both sides — a disassembly read is enough. That makes
  the honest-count exit test of phase 1 much cheaper than budgeted.
- **The 29,036 overlay functions are still the prize and are not yet in play.**
  Everything above is boot-EXE against `.text`. The overlays are not extracted on
  this machine and the PC port has no overlay structure at all — it is one flat
  image — so overlay-side transfer needs its own probe before phase 1 is scoped.

## Open

- Whether the delta propagation actually *scales*, or whether eight functions in
  one well-documented subsystem is a best case. The next probe should be a
  subsystem with no existing cross-reference landmarks — the battle engine is the
  obvious candidate, and it is where the overlay corpus lives.
- PC `0x80316F`, written `2` by `MsgBox_Reset`, is window record 0 field `+0xF`;
  the PSX counterpart would be `0x8014833B`. Unexamined. (Not to be confused with
  `0x8014832F` = field `+3`, which `MsgBox_ReplayIfShown` `0x80151554` checks for
  `2`; different field, same value, and that coincidence is a trap.)
- Function-entry recovery misses code before the first direct-call target: the
  area-script writers of `0x7DEE48` at `0x40100A` and up are attributed to no
  function. Harmless for this probe, a gap to close in phase 1.

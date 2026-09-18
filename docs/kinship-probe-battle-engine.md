# Kinship probe — the battle engine

**Status:** STABLE (measured 2026-09-18)

The successor question to
[`kinship-probe-text-engine.md`](kinship-probe-text-engine.md), which passed on a
subsystem that was already well cross-referenced and lived entirely in the PSX
**boot EXE**. Two things were still open:

1. **Does it scale to a subsystem with no pre-existing landmarks?**
2. **Do *overlay* functions transfer?** This is the one that decides the size of
   the prize: 29,036 of the 30,062 mapped PSX functions are overlay-resident, and
   the PC port has no overlay structure at all — it is one flat `.text`.

## Result

**Both yes.** Every function matched below is a PSX **overlay** function, from
two different overlays (`BATTLE.EMI#3` and the battle engine `BATTLE.EMI#15`),
and they were found in a flat PC image with no overlay boundaries to guide the
search.

The probe also turned up a **second anchoring technique that needs no seed at
all** — see *Data tables transfer by value* — which found `Battle_ScaleDamage`
from a table of eight numbers.

Two working-record blocks were recovered, and the field agreement is measurable
rather than impressionistic:

| Block | PC | PSX | Field offsets | Agreement |
|---|---|---|---|---|
| Enemy working records | `0x93B9E0` | `0x801EB620` | PC = PSX **+ 0x10** | **21 / 26** documented fields land on a referenced PC address (baseline 17.2%, binomial P ≈ 2.5e-12) |
| Party working records | `0x802D50` | `0x80145E8C` | PC = PSX **+ 0** | **19 / 21** (baseline 26.8%, P ≈ 1.6e-09) |

"Baseline" is the fraction of bytes inside one record that are referenced as an
absolute operand anywhere in `.text`, so it is the hit rate a *wrong* delta would
score. Method and misses are below.

---

## Evidence

Source for every PSX-side claim:
[`BATTLE_RAM.md`](../../BreathOfFire3Recomp/docs/BATTLE_RAM.md) and
`names/functions.toml` in the archival sibling. PC-side work used the probe
tooling from the text-engine run plus one addition, `tools/pe_data.py` (read
initialised `.data` by virtual address).

### 1. Seeding from the one landmark that existed

`PC_PORT_CROSS_REFERENCE.md` §3 records `bof3ext`'s declaration
`EnemyBattleData @ 0x93B9E0`, with no stride. Disassembling its three referencing
functions gives the stride directly — PC `0x44A960`:

```
sub  eax, 3                  ; actor id -> enemy index (party is actors 0..2)
lea  ecx, [eax + eax*8]      ; x9
lea  edx, [eax + ecx*4]      ; x37
lea  eax, [edx*8 + 0x93b9e0] ; x296 = 0x128
```

**PC stride `0x128`; PSX stride `0x118`.** A difference of `0x10` — the port
prepended a 16-byte name field (12 bytes of which `0x44A960` copies out). That
predicts **PC offset = PSX offset + 0x10**, which is what the table above
measures.

### 2. Measuring the delta instead of asserting it

Building the global cross-reference index (`tools/pe_xref.py --build`, 7,601
distinct referenced globals) and asking which addresses inside record 0 are
touched at all, then laying `BATTLE_RAM.md`'s documented PSX field list over it
at the candidate shift:

```
enemy working record (PC 0x93B9E0 = PSX 0x801EB620, offsets +0x10):
  21/26 documented PSX fields land on a referenced PC address
  baseline 17.2%, binomial P(X>=21) = 2.46e-12
  misses: +0x04 zenny, +0x1C/+0x1E drop 2, +0x22 max AP, +0x94 charge multiplier

party working record (PC 0x802D50 = PSX 0x80145E8C, offsets +0x00):
  19/21 documented PSX fields land on a referenced PC address
  baseline 26.8%, binomial P(X>=19) = 1.60e-09
  misses: +0x74 persistent-record copy, +0x119 command byte
```

The misses are not counter-evidence, and it is worth saying why: an address
appears in this index only when some instruction names it as an **absolute**
operand, which happens only for record 0 accessed at a constant offset. A field
touched exclusively through a computed pointer — `+0x74`, a `0xA8`-byte block
that is `memcpy`'d rather than read field-wise, is the clearest case — is
invisible here regardless of whether the delta is right.

Two independent corroborations of the same shift, both structural rather than
statistical:

- **The resistance grid.** `BATTLE_RAM.md` puts nine consecutive affinity-class
  bytes at enemy `+0x2F..+0x37` (fire, ice, lightning, earth, wind, holy,
  psionic, status, death). At the shift those are PC `0x93BA1F..0x93BA27` — and
  all nine are referenced, the first six by one function (`0x44EE80`) and the
  last four by another (`0x44F030`), which is exactly the PSX split between
  `Battle_ElementAffinity` and `0x8009FD08`.
- **Shared consumers.** PC party HP (`0x802DD8`) and PC enemy HP (`0x93BA04`)
  are referenced by an overlapping set of six functions — `0x437CC0`,
  `0x43B130`, `0x44B9F0`, `0x44FCE0`, `0x49D680`, `0x596A90`. Code that handles
  both sides of the damage path is precisely what should touch both, and a wrong
  delta on either block would not produce that overlap.

### 3. The party record grew at the end, the enemy record at the front

| | PSX stride | PC stride | Where the growth is |
|---|---|---|---|
| Enemy working record | `0x118` | `0x128` | `+0x10` **before** every field — a prepended name |
| Party working record | `0x140` | `0x14C` | `+0xC` **after** `+0x13C` — every documented field keeps its offset |

Worth noting for its own sake: the persistent character record did **not** grow
(164 bytes in both, `PC_PORT_CROSS_REFERENCE.md` §1), and the party working
record carries a copy of it at `+0x74`. So the port left the shared record
layout alone and only widened what it had to.

### 4. `Battle_ScaleDamage` — an overlay function, matched whole

PC `0x446430`, against `Battle_ScaleDamage` `0x801DCD18` (`BATTLE.EMI#3`,
`names/functions.toml`, status `evidence`):

```
shl  esi, 8                      ; base << 8  (8.8 fixed point)
...  magic divide, shl 0x11, sub from 0x100, sar 8
cmp  edi, 0xcd
jge  +5
mov  edi, 0xcd                   ; clamp to 205 = 0xCD
call 0x5b93d2                    ; rand
and  eax, 7                      ; Rand & 7
mov  esi, [eax*4 + 0x64e38c]     ; variance table
imul esi, edi / sar esi, 8       ; /256
mov  ax, [0x903850]
test al, 0x1f                    ; weapon byte & 0x1F
je   +
call 0x44ee80                    ; Battle_ElementAffinity(target, element)
imul ecx, esi / magic divide     ; /100
mov  ax, [0x903850]
...  test bit 0x20 -> [eax*2 + 0x64e3ac] / 100
```

Against the PSX note, term for term: `base * 205/256`, the `Rand & 7` variance
draw over an eight-entry table `/256`, the `& 0x1F` elemental branch through
`Battle_ElementAffinity` `/100`, the `& 0x20` holy branch through a `s16` table
`/100`. The `0xCD` is the same constant in the same role.

Three further identifications fall out of that one function:

- **PC `0x44EE80` = `Battle_ElementAffinity`** ↔ PSX `0x8009FA78` — which is in
  `BATTLE.EMI#15`, a *different overlay* from `Battle_ScaleDamage`. It reads the
  five element bytes at enemy `+0x3F..+0x43`.
- **PC `0x903850` ↔ PSX scratchpad `0x1F800000`**, the damage scratch word the
  weapon byte is read from. The PSX has 1 KB of fast scratchpad RAM and x86 has
  no equivalent, so the port relocated it to an ordinary global. **A structural
  divergence forced by the hardware, and one to expect more of** — anything the
  PSX put at `0x1F80xxxx` is somewhere arbitrary on the PC side, so scratchpad
  addresses will never transfer by delta.
- **PC `0x5B93D2` = the CRT `rand`**, standing where the PSX calls the BIOS
  `A0:2F` thunk (`Rand` `0x8017ED4C`).

### 5. Data tables transfer by value — and that is a second anchor

The PSX damage-variance table is `{0xDA, 0xE6, 0xF3, 0x100, 0x10D, 0x11A, 0x126,
0x133}` as `u16` at `0x801EAF50`; the holy multiplier table is
`{300, 200, 200, 150, 125, 100, 50, 0}` as `s16` at `0x801EAF70`. Searching
`BOF3.exe` for those value sequences:

```
0x64E38C  .data  u32 x8: 218, 230, 243, 256, 269, 282, 294, 307   <- variance
0x64E3AC  .data  s16 x8: 300, 200, 200, 150, 125, 100, 50, 0      <- holy
```

Same values, same order, adjacent in both binaries — and `0x446430` is the
**only** function referencing either, exactly as on the PSX side where the holy
lookup is inlined into `Battle_ScaleDamage`.

Note the variance table is `u32` on the PC and `u16` on the PSX. The values did
not change; the element width did. **A value-sequence search must therefore be
width-agnostic** — the first attempt, `u16` only, reported the table absent, and
a width-agnostic retry found it immediately. That near-miss is the reason this
section exists: a negative result from a value search is worth nothing until
every plausible width has been tried.

What this buys is an anchoring technique that **requires no seed pair**. Any
documented PSX constant table can be searched for directly in the PC image, and
each hit names the functions that reference it. That is a way into subsystems
where nothing is known yet, and it is independent of the delta propagation, so
the two check each other.

---

## Pairs established

Added to [`symbols.toml`](../symbols.toml). Every function here is PSX
**overlay**-resident.

| PSX | Overlay | Name | PC | Tier |
|---|---|---|---|---|
| `0x801DCD18` | `BATTLE.EMI#3` | `Battle_ScaleDamage` | `0x446430` | evidence |
| `0x8009FA78` | `BATTLE.EMI#15` | `Battle_ElementAffinity` | `0x44EE80` | evidence |
| `0x8009FD08` | `BATTLE.EMI#15` | psionic / status / death affinity | `0x44F030` | hypothesis |
| `0x8017ED4C` | boot | `Rand` (CRT `rand` here) | `0x5B93D2` | evidence |

`0x44F030` is `evidence` for its *role* — it reads enemy `+0x45`/`+0x46`/`+0x47`
and the party equivalents under mask bits `0x40`/`0x80`/`0x100` and indexes a
`s16` percentage table, which is unambiguously the psionic/status/death affinity
lookup — and `hypothesis` for the *identification*, because `BATTLE_RAM.md` lists
a sibling `0x8009FE88` on the same three fields and nothing yet separates them.

Two neighbours are left deliberately unnamed: `0x44F770` is a near-duplicate of
`0x44F030`, and `0x44F130` reads the holy byte but indexes a third table
(`0x64E99C`). Both are real functions doing something specific; guessing which
would be exactly the silent-hypothesis-promotion the tiering exists to prevent.

---

## What this changes

- **The overlay corpus is in play.** This was the open question from the text
  engine probe and it is now answered: 29,036 overlay functions are transferable
  in principle, not just the 1,026 boot-EXE ones. That is the difference between
  a partially annotated PC binary and a substantially annotated one.
- **There are two independent anchoring techniques, not one.** Delta propagation
  needs a seed and then spreads cheaply; value-sequence search needs no seed but
  only works where a constant table exists. Used together they cross-check, and
  the second is how to enter a subsystem cold.
- **Phase 1 should build the matcher around both.** PLAN §3's call-graph shape
  remains a *verifier* rather than the primary signal.
- **Not everything transfers by delta, and the exceptions are predictable.** PSX
  scratchpad (`0x1F80xxxx`) has no PC counterpart; BIOS thunks become CRT calls;
  record strides change where a field was widened. None of these break the
  method, but a matcher that assumes a uniform mapping will produce confident
  wrong answers at exactly those points.

## Open

- **The overlay corpus is not extracted on this machine**, so this probe worked
  from `BATTLE_RAM.md`'s prose rather than from `names/functions.toml`
  programmatically. Automating transfer at scale needs the overlay images and
  their symbol tables in a machine-readable form on this side.
- **Enemy `+0x00..+0x0F` (the prepended name) is unexamined.** 16 bytes, 12 of
  which are copied out by `0x44A960`. Whether the remaining 4 are padding or
  fields is unknown.
- **Party record growth `+0xC` after `+0x13C` is unexplained.** All documented
  fields keep their offsets, so whatever was added sits past the end of what
  `BATTLE_RAM.md` covers.
- **`0x44F130`'s third table `0x64E99C`**, and the run of eight-entry `s16`
  tables around `0x64E95C`, which look like the PC counterpart of the PSX family
  at `0x800B187C`/`0x800B188C`/`0x800B189C`. Not verified — that PSX data is
  overlay-resident and was not read on this side.

# Area 11: two handlers and an init, the area harness's proof

**Status:** IN PROGRESS (2026-09-27) - three functions ours
(`src/game/area_011.cpp`, shadow name `area_011`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)): 0 mismatches in
12,000 rounds; 20 controls planted, 18 refused by a count, 1 refused then hung (replaced), 1 equivalent with its near variant refused (section 4). Fuzz only: no recorded route enters area 11
(section 6). No divergence; no new defect.

Group ARH of round ten's first wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1), as the proof
of the area harness ([`takeover-queue-areas.md`](takeover-queue-areas.md)
§7 item 3).

## 1. The area, and why this one

Area 11 is `Area_Descriptors` entry 11, descriptor `0x5E2738`, the PSX's
`BIN/WORLD00/AREA011.EMI` (descriptor `0x801F30B0` there, by the sibling's
`names/area_records.toml`, which pairs all three roots below). What the
area *is* in the story is not read here.

**Why this area.** The brief preferred a world 0 area the attract cycle or a
recorded route reaches. None qualifies: no function of `0x401000..0x405000`
has an entry in any `analysis/calltrace/*/bof3x.callcounts.tsv`; the hidden
reach lists (`analysis/hidden_reached_*.json`, `pc_hidden_reached.json`)
name only areas 17 (world-map code, 18 exclusive functions), 29 and 33
(their exclusive functions ours already) and 34 (world-map code shared with
the eleven world-map copies). So, per the fallback, the smallest world 0
area with a handler array **and** an init: area 11, three exclusive
functions, none shared with another area, none ours, and no unreached start
in its block (the scratch walk `area_roots3.json` of 2026-09-26; area 18 has
two plus a shared body, area 20 one).

| PC | Name | Bytes | Root | PSX twin | What |
|---|---|--:|---|---|---|
| `0x401750` | `Area11_StepCameraDistance` | `0x1C` | handler 0 (`Area11_Handlers` `0x5E2730`) | `0x801F2C04` | `Camera_Distance`, as a signed word above `-0x1400`, is `0xA0` less; `MapView_Redraw = 2` either way |
| `0x401770` | `Area11_SpawnEffect` | `0x46` | handler 1 | `0x801F2C44` | `Sprite_Current` made the leader; `Effect_Spawn(1, 0, Area11_EffectKinds[member], leader +0x2E, leader +0x30)`, member the byte `0x904064`; an answer other than `0xFF` to `Sprite_Current[0xB]`, `Sprite_Current` read again |
| `0x4017C0` | `Area11_DimBackdrop` | `0x79` | init (`+0x40`) | `0x801F2CC4` | with `Cond_Flags` row 14's bit `0x13` set and `0x14` clear: the first header entry of kind exactly `0x81` from `AreaMap_EntryBase` gets `0x21080000` in both colour dwords `+8` and `+12` |

Extents by capstone recursive descent (2026-09-27, the scratch `adis.py`
over `tools/magic_rows.py`'s `descend` and `clone_sites`): every jump
internal, no jump table, nothing refused. The block is `0x401750..0x401840`
(area 12's first function); the gaps are padding.

**Reading notes.**

- Handler 1's member byte is loaded as a dword and masked; `0x904064` is the
  third byte of the first party list (`0x904062`,
  [`field-event.md`](field-event.md)). `Area11_EffectKinds` `0x5E2400` is
  eight bytes before the area's link list (`+0x20`, `0x5E2408`); the index
  is unchecked, as the original's. The call is exactly the one the
  movement-script ops `90..95` make (`Effect_Spawn`'s evidence), with the
  kind 1 and the effect byte from the table.
- The init's walk is `AreaMap_HeaderPass`'s ([`map-layers.md`](map-layers.md)
  §1): a zero dword ends it, byte `+2` is the step in dwords. Kind `0x81` is
  `AreaMap_EntryHandlers` entry 1 with bit 7, `AreaMap_DrawBackdrop`
  ([`area-backdrop.md`](area-backdrop.md)), whose `+8` / `+12` are the
  gradient's colour and its alternate: `0x21080000` is black above (low
  word 0) and 5-bit (8, 8, 8) below. So on that flag state the sky is
  dimmed; what the flags mean is not read.
- `Flags_Test(0x904000, n)`: `0x904000` is `Cond_Flags + 0x70`.

## 2. Ours

`src/game/area_011.cpp`, calling out only through the harness
(`AH_CALL(Effect_Spawn)`, `AH_CALL(Flags_Test)`). Kept as the original:
`Sprite_Current` re-read after `Effect_Spawn`; the leader's words read
before the store to `Sprite_Current`; the init writes `+12` before `+8`
(no reader in between); a step of 0 on an entry of another kind never ends
(section 4).

## 3. The fuzz

`BOF3X_SHADOW=area_011` (`src/game/area_011_fuzz.cpp`): 4,000 rounds per
function, `Group::area` 11 (the real descriptor and tables in place). The
clone table is hand-built; the shapes are handler, handler, init.
`Effect_Spawn` is listed by the group (the standard `kFlag` never answers
`0xFF`): `kByte` `0xFE..0x02`, so `0xFF` a fifth of the time and `0xFE`
beside it. Seeds: `Camera_Distance` at `-0x1400`, `-0x1401`, `-0x13FF`,
`-0x13A0`, `0x7FFF`, `-0x8000`, 0 two rounds in three; the member byte
0..7 two rounds in three (else any byte: the table's neighbours are read
from the image as the original reads them); the init's two flag bits in all
four ways (`Flags_Test` is a recorder, `kBool`: its answers steer the path)
and a chain of 0..10 entries of kinds `0x81` (twice as likely), `0x80`,
`0x82`, `0x01`, `0x00`, `0xC1`, `0x41` with steps 1..3 and a zero end, from
an `AreaMap_EntryBase` of `0x10..0x30F`.

**Result (in this worktree):** 12,000 rounds, 10,656 calls to the
stand-ins, 0 mismatches, 16,208 bytes of state (20 regions); coverage
`Effect_Spawn 4000, Flags_Test 6656`. `BOF3X_SHADOW='*'`: exit 0, every module's self-test 0 mismatches (335 self-test lines), the spell groups through `magic_harness` unchanged.

## 4. Controls

Planted one at a time in `area_011.cpp` by a script (the scratch `controls.py`, not committed) that plants, rebuilds, checks the file recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_011`, restores; after the last it rebuilt and ran the clean self-test (0 mismatches). **20 planted: 18 refused by a count (exit 3), one refused and then hung (A16, replaced by A16b), one equivalent (A18, with its near variant A18b refused).**

| # | planted | refused in rounds (of 4,000 for the function) |
|---|---|---|
| A1 | handler 0: the compare `>=` for `>` | 391 |
| A2 | handler 0: the step `0x9F` for `0xA0` | 2,268 |
| A3 | handler 0: `MapView_Redraw = 3` | 4,000 |
| A4 | handler 0: the redraw only when the distance steps | 1,726 |
| A5 | handler 1: the member byte from `0x904063` | 3,579 |
| A6 | handler 1: x and z passed swapped | 4,000 |
| A7 | handler 1: `Effect_Spawn` kind 2 | 4,000 |
| A8 | handler 1: the store skipped on 0, not on `0xFF` | 1,579 |
| A9 | handler 1: the answer stored through the leader, `Sprite_Current` not read again | 113 |
| A10 | handler 1: `Sprite_Current` not made the leader | 3,161 |
| A11 | init: flag `0x12` for `0x13` | 4,000 |
| A12 | init: the second flag test not negated | 1,710 |
| A13 | init: the kind compared without bit 7 (`0x01` matches too) | 231 |
| A14 | init: the alternate colour `+12` not written | 550 |
| A15 | init: the colour `0x21080001` | 550 |
| A16 | init: no `return` after the first match | refused (mismatches from round 389), then **ours hung**: the next step lands in the rewritten colour dword (`0x21080000`: kind `0x21`, step 8) and walks the random bytes to a step of 0. Killed; replaced by A16b |
| A16b | init: the kind compared `>=` `0x81` (`0xC1` matches) | 369 |
| A17 | init: the second flag test on `0x904001` | 2,656 |
| A18 | init: the walk ends on `(e & 0xFFFFFF) == 0` | **0 - equivalent**: an entry whose low 24 bits are 0 has a step of 0, which never ends the original (section 6), so no input that ends the original tells them apart |
| A18b | init: the walk ends on `(e & 0xFF00FFFF) == 0` (near A18) | 82 |


## 5. What `tools/area_rows.py` should print for area 11

For group ART's output to be checked against: descriptor `0x5E2738`
(`0x667590` entry 11); `+0x34` (choice table) null; `+0x38` null; `+0x3C`
`0x5E2730`, two handlers `0x401750`, `0x401770`; `+0x40` init `0x4017C0`;
no entry in `Area_StepHook` / `Area_ArriveHook`, `0x662CE8` or `0x662F28`;
no further code pointer in the area's data block (the only dwords in the
image holding the three addresses are the two handler cells and the
descriptor's `+0x40`). Block `0x401750..0x401840`: three exclusive
functions, no shared body, no unreached start. Clone rows: `0x401750`
`0x1C` bytes, no calls; `0x401770` `0x46`, `{0x30, 0x57CE10}`; `0x4017C0`
`0x79`, `{0x8, 0x57C140}, {0x1B, 0x57C140}`; no immediates, no jump table,
nothing refused. Frontier: `Effect_Spawn` (Capcom's), `Flags_Test` (ours).

## 6. What reaches it, defects, calls across groups

- **Reach:** no trace names any of the three. Entering area 11 runs the
  init; the handlers run when the area's scripts use op `03` / `DE`. A
  recorded walk of world 0 is the live check ([`takeover-queue-areas.md`](takeover-queue-areas.md) §5).
- **Latent defect (described, not fixed):** the init's header walk, as
  `AreaMap_HeaderPass`'s, never ends on an entry of another kind whose
  step byte is 0. Not planted; the fuzz keeps steps above 0.
- **Cross-group calls:** none. `Effect_Spawn` is Capcom's and unowned this
  wave; `Flags_Test` is ours by name.
- `analysis/calltrace/entries_logic.txt`: `00401750 1C`, `00401770 46`,
  `004017C0 79` appended.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D136 (reads and writes by an unchecked byte or count),
D144 (the unbounded walks) in [`known-defects.md`](known-defects.md).
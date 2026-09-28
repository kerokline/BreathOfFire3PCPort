# The boss tool: every function of the boss band, found from the engine's three root sets and cut into units

**Status:** MEASURED (2026-09-28) - `tools/boss_rows.py` exists and the boss
plan ([`takeover-queue-bosses.md`](takeover-queue-bosses.md)) is written
from its report. No function taken; nothing named yet (the plan's section 7
step 2 lists the names). The cut into groups (section 3) is a proposal for
the coordinator; a boss harness, when written, consumes `--clones`.

## 1. What it reads, and how to run it

```
python tools/boss_rows.py --exe .../bof3/BOF3.exe --analysis .../analysis \
    --sibling .../BreathOfFire3Recomp                -> the report
    ... --unit K01                                   -> one unit, function by function (B12, K07, F2, FB8, H)
    ... --unit B01 --clones                          -> clone tables (C++, magic_harness's API)
    ... --groups                                     -> the group table (markdown)
    ... --disc "CDImage/Breath of Fire III (USA).cue"  -> every kind's enemy name and every set-up's fight
```

The flags are `area_rows.py`'s (`--exe`, `--analysis`, `--sibling`,
`--symbols`, `--unit`, `--clones`, `--groups`, `--group-size` default 50,
`--quiet`). It writes `analysis/boss_rows.tsv` (a unit a line: root, how
reached, span, counts, its `.data` tables, its frontier, the sibling's file
for a set-up, its group, every function) and `analysis/boss_funcs.tsv` (a
function a line: start, size, the units that reach it, `exclusive` /
`two-units` / `shared` / `gap`, group, ours, name, the catalogue's label and
combat column, the tables its code names). Both gitignored (CLAUDE.md rule
1). A run takes about 40 seconds.

Inputs: the exe; `analysis/pc_funcs.json` and `pc_hidden.json` (the
starts), `remaining_catalog.tsv` (labels, report only); `symbols.toml`; the
sibling's `names/boss_records.toml` (boss id to `BOSSnnn.EMI`). The
descent is `area_rows.Descent` / `discover` re-banded to
`0x437A00..0x441000`; the clone sites are `magic_rows.clone_sites`.

## 2. The method

Three root sets (the plan's section 1.2, each verified in the disassembly):

1. `Boss_SetupTable` `0x656954`: 56 dwords, `jmp [eax*4 + 0x656954]` at
   `0x4942AC` with `eax` the event-battle byte `0x904AAA`. Unit `B<id>`.
2. `BossKind_Table` `0x64B088`: 63 dwords read until the first that is
   not a `.text` address (`[63]` is `0x40C0709`); `[0]` is null. Unit
   `K<kind>` for the 61 in the band.
3. The stack tables of `BattleFx_Dispatch` `0x4352A0`, `BattleMagicFx_Dispatch`
   `0x435350` and the kind-3 dispatcher `0x4357D0` (`mov [esp + 4k], imm32`
   with a `.text` value): every slot in the band except `0x437CC0`. Unit
   `F<slot>` (the kind-3 dispatcher) or `FB<slot>` (the other two).

A unit's closure follows the descent's direct calls, tail jumps, code
immediates (the hooks a set-up stores, the `+0xF4` hooks a kind stores, the
tasks it spawns) and the pointer runs of every `.data` address its code
names. **A table is read from the address the code names to the next
address any band code or `symbols.toml` names**, skipping up to 0x40 bytes
of flag header; without that bound the runs read on into the next kind's
tables (the tables are contiguous in `.data`) and every closure leaked into
its neighbours - the first scratch walk's mistake. A function three or
more units reach is a **shared helper**, closures stop at it, and the set
of helpers is iterated to a fixpoint (one round suffices today: 20).

Then: units in address order; groups of whole units, `--group-size` each
(a unit is never split; a function two units share goes with its first
group); a function no root reaches is a **gap** and goes with the group
whose span holds it. Today the gaps are 10 and all ours (the Head Cracker
rock's states, reached through the spell round's engine rows), so the
closure is the band.

## 3. The numbers (2026-09-28, `4a15d61`, 4,554 ours)

| | |
|---|--:|
| Band `0x437A00..0x441000`: functions / listed / dropped / found | 541 / 538 / 5 / 8 |
| Ours / to take / bytes to take | 16 / 525 / 33,300 |
| Units: set-ups / kinds / effect tasks | 55 / 61 / 7 |
| Reached by one unit / two / shared by 3+ / gaps | 496 / 15 / 20 / 10 (all ours) |
| Consecutive unit spans overlapping | 17, every one a shared body or a table inside a neighbour's span |
| Frontier / ours / named not ours / unnamed | 117 / 67 / 3 / 47 |
| Groups (`--groups`, size 50) / functions to take | 11 / 525 |

The set-ups' entries in address order are the sibling's file order
(`boss_records.toml`) wherever ids share a file, which is the block rule's
check: the PC linked the images in disc order.

## 3a. The names (`--disc`)

The working record's `+0x100`, the byte `EnemyRunAll` dispatches
`BossKind_Table` by, is copied by `Battle_CopyEnemyData` from the enemy
data record's `+0x88`, and the enemy data is the area's (US `AREAnnn.EMI`
section `0x800E4000`, stride `0x88`, name 8 bytes, kind `+0x84`). With the
US disc's `.cue` the tool reads all 200 area files and prints every kind
with its enemy name and areas, then every set-up with its file
(`boss_records.toml`), its record row (`EventBattle_Records[id] +2`), the
kinds of its file (a file's kinds precede its set-ups in address order)
and the areas whose row carries them. 62 of 62 kinds resolve; 50 of 55
set-ups resolve to one fight (the plan's section 7 lists the five that do
not and why).

## 4. What it does not do

- It reads no route. The combat route enters one band function
  (`0x4AEE90`'s tail); no fight has been recorded. The report's last line
  lists the 53 `push id; call Field_StartEventBattle` sites by the
  caller's 4 KiB page, which with the scenario plan's band table gives
  each id's chapter for the recipe saves.
- `--clones` prints `boss_harness::` types that do not exist yet; the
  harness is the plan's section 7 step 3, and the C++ is `magic_harness`'s
  API one for one, as `area_rows.py`'s was.
- It does not name anything; the names are the round's.

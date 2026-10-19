# The typed-data round, stage C: staging

**Status:** DRAFT (staged 2026-10-19 on `typed/b1` `b78726f8`, after stages A
and B; nothing of stage C built). The round is
[`typed-data-round.md`](typed-data-round.md) (IDEAS I44, stages 1 and 2);
this doc replaces its old section 4, which becomes C4 below.

Stages A and B left 27,394 raw data sites, down from 50,943, with the machine
code identical to `origin/main` 19180e63. What is left is no longer
mechanical: it needs a record type that does not exist yet, a decision about
a field that means different things in different places, or a better name.
Stage C makes those types and decisions, then runs B's method again on the
sites they unlock. Still no behaviour change, and no ledger entry.

## 0. What stage B left, measured

`analysis/data_census.tsv` on `typed/b1`: **27,394 sites**, of which 10,347
are global, 15,219 record and 1,828 raw address. The record sites B1 left
were in seven `analysis/typed_b1_<group>_left.tsv` files, each with its
reason; since C0 they are rows of the one `analysis/typed_left.tsv`
(`tools/typed_left.py`, keyed by place rather than line, `source` column
`b1-<group>`). Totals across the groups (15,228 rows, with reasons normalised;
counts are rows, not census sites):

| Reason | Rows | What unlocks it |
|---|---:|---|
| field-missing | 4,037 | C1: the field named in `records.h` (most are the sprite head's bytes, padding in the effect, task and party structs); 1,546 of magic's are the `kS` scratch block and 224 are GPU packets, which want new structs |
| family | 3,985 | C2: a family established by a doc read, or a typed parameter, or left; includes `Sprite_Current` in functions whose record is not documented |
| no-struct | 2,049 | C1: a struct for a family `records.h` lacks (GPU packets, character records `0x903A70`, community records, the 12-byte enemy slots, the effect sub-pools) |
| union | 1,130 | C1: a view per meaning (the party tail, the window's per-handler words, the effect kinds' per-kind words) |
| width | 1,099 | C1: the whole-part halves of 16.16 positions (+0x36 / +0x3A / +0x3E) and similar narrow reads |
| seam | 1,050 | stays: harness `Mem()` sites and fuzz seeds that write raw records on purpose |
| computed | 911 | stays, or C2 by a typed array where the offset is an index |
| platform | 548 | stays: our own buffers, not the game image |
| gate | 270 | C0's `Store()` and C2's typed locals; mostly the assignment-order case below |
| order, accessor, audit-family, other | 149 | case by case |

Global sites B0 left (`analysis/typed_b0_left.tsv`, now `typed_left.tsv`'s
`b0` rows): 9,135 on an address with
no `[[data]]` entry, 685 inside an entry, 576 at another width than the
entry, 351 on an untyped entry, 219 bound or size names.

## 1. The rules, as stage B learned them

- **The proof is `tools/codegen_same.py`:** every object's disassembly equal
  to the `origin/main` baseline build, `0 differ`, after every batch. It is
  stronger than the state-hash runs section 1 of the round doc planned (the
  same instructions cannot behave differently) and needs no game run.
- **The gate cannot see a wrong family.** A pointer typed as the wrong struct
  compiles the same when the field offsets agree. Stage B1's audit found 13
  such pointers in about 1,500 (reports in `C:\Users\kerok\v_typed\`,
  `b1_audit_*.md`). **Every typing pass in stage C is followed by a read-only
  family audit**, by agents who did not do the typing, before its merge.
- **Build before calling anything done.** Stage A's checks were header-only and
  Python, and missed that the merged tree no longer compiled (a typed
  `[[data]]` entry's macro rewrote `bof3::addr::Party_Zenny`). A typed entry's
  address is spelled `<Name>_addr` (`gen_symbols.py` emits it).
- **Assignment order.** `p->f = expr` evaluates `expr` before `p` (C++17);
  `SetLong(p + off, expr)` evaluated the pointer first. Where the pointer is
  re-read from memory (`Sprite_Current`, `Cur()`), the two differ and the gate
  refuses the plain assignment. C0's shared `Store()` keeps the setter's order.
- **The harness seam stays:** in modules that reach memory through a
  harness's out-of-line `Mem()`, a typed pointer still comes from `Mem()`.
- **Ownership:** one owner per file; `records.h`, `symbols.toml`, `tools/` and
  shared headers change only in the stages that own them below.
- Rules 1, 4 and 7 of `CLAUDE.md` as everywhere. Each agent keeps its scratch
  in a folder of its own: in stage B two groups' `conv.py` overwrote each other
  in a shared scratchpad.

## 2. C0: consolidation (one agent, alone, first)

Every B1 group invented its own spelling of the same few things, so the
record code now has seven dialects. C0 makes one, over the whole tree:

- **`src/game/record_views.h`:** the typed views every group wrote per file -
  `ScTask()`, `MemberRec(p)`, `EffectRec(p)`, `CurFx()`, `AsFx(p)`, `AsSpr(p)`,
  `SC()`, `Spr(p)`, `Win(p)`, `Self()`, `TEffectAt(i)` and the like - as a
  small set of templates (`As<T>(p)`, `AsConst<T>(p)`, `Current<T>()` for
  `Sprite_Current`), and one `Store(field, value)` with its order documented.
  The per-file helpers go; the sites move to the shared names.
- **`tools/typed_convert.py`:** two zero-width insertions at one offset are
  both dropped by `Converter.render` (stage B1's effect group: 34 false gate
  failures). Merge insertions at one point before rendering.
- **The left-raw lists:** one `analysis/typed_left.tsv`, regenerated from the
  tree by a tool, reasons carried by key (file, offset, base) rather than by
  line, so they cannot go stale. Today's area list is off by about 7 lines in
  `area_w1c.cpp` and was hand-renumbered once already.
- **Docs:** [`typed-records.md`](typed-records.md) section 3 item 1 still says
  `kPWeaponKind`; stage B1 renamed it `kPCharIndex`.

Check: `0 differ`, the census unchanged (C0 converts nothing), no file keeps a
private view helper. One agent, because it touches files in every group.

## 3. C1: the types (one agent; `records.h`, its doc, new headers)

Header and doc only; no site changes. Each item cites the readers, as stage
A's SD did, and the B1 left-lists say which sites wait on it.

1. **`SpriteHead`, bytes `+0x00..+0x7F`.** The sprite object, the party
   member, the effect object, the battle enemy and the battle task all begin
   with the same 0x80-byte layout (stage A's verification, the B1 reports,
   and the magic audit's reading of `Battle_InitActorContexts` 0x446FF0,
   which copies an actor's first 0x80 bytes into a task slot). Make it one
   struct that each family begins with, with every offset asserted, so the
   head's fields are named once. This is the largest single unlock:
   most field-missing rows are head bytes padded out of the effect, task and
   party structs.
2. **Renames the audits asked for** (same offsets, better names):
   - `WindowRecord::centre_x` / `centre_y` are the top-left corner (in pixels
     for the menus, 12.4 for kind 1), not the centre: `Window_Grow`'s
     half-step arithmetic and `menu-windows.md` 2 ("x, y (s16)").
   - `BattleTask::y` at +0x38 is world z in all 331 uses; `vel_*` and
     `start_*` at +0xC..+0x1F hold angles, radii, counters and offsets as often
     as velocities. With the head shared they take the head's names, and the
     scratch dwords neutral ones.
   - `+0x70` is `raised` in `PartyMember` and `reach` in `SpriteObject`: one
     byte, one name.
   - `WindowRecord::kind` is documented 0..2 and written 4..7 and 0xB.
3. **Views for fields that change meaning** (a union, or a view struct per
   meaning, cast where the meaning is known):
   - the party tail +0x124..+0x13F: a script context in the field (laid out as
     the sprite's +0x80) and an action record in battle;
   - the window record's +0x8..+0x1F, per handler (field, battle, list);
   - the effect kinds' per-kind words (+2, +6, +9, +0xC, +0x10, +0x2E, +0x30;
     [`effect_1a.md`](effect_1a.md) 2.2..2.6 and the B1 effect report).
4. **The 16.16 halves:** the whole-part words at +0x36 / +0x3A / +0x3E are
   read as `u16` in about 1,100 sites. A named view (`x_whole` and so on) in
   the head.
5. **New structs for families `records.h` lacks**, each only as far as its
   readers settle it: the damage scratch block `DamageScratch` (`kS`,
   `0x903850`, a `[[block]]` today; 1,546 magic sites), the GPU primitive
   packets at `Gfx_PacketNext`, the character records `0x903A70`, the
   community records `0x9046D0` / `0x9048B0`, the 12-byte enemy slots
   `0x939F00`, and the effect sub-pools the B1 lists name.

Check: the header compiles standalone (`-std=c++20 -Wall -Wextra`, and under
`#pragma pack(1)`), every offset asserted, and the whole tree `0 differ`
(renamed fields mean renamed sites: C1 renames the existing uses with the
gate, and nothing else).

## 4. C2: the sites C1 unlocks (seven file groups, then the audit)

Stage B1 again, with its brief (`C:\Users\kerok\b1\BRIEF.md`) and its seven
file lists (`C:\Users\kerok\b1\*.txt`), updated for C0's shared views and
C1's types. Each group converts the rows of its left-list whose reason C1
removed: field-missing, no-struct, union, width; and the gate rows C0's
`Store()` now allows. Then, before any merge, **three read-only family audits**
as after B1, and a revert of whatever they find.

## 5. C3: the globals' follow-ups (SG again; then one tree-wide rename)

- **295 data addresses reached only through module constants** were outside
  stage A's SG scan (it read the `at::` constants only;
  [`typed-data-census.md`](typed-data-census.md) 6). Settle their widths with
  `tools/data_widths.py` and add the entries.
- **151 pointer-holding cells typed `unsigned long`** (`BattleFlow_TaskCurrent`,
  `BattleE4_MenuActor`, ...): retype as pointers, which lets B0's 151
  `pointer-in-integer` sites convert.
- **The 74 scalars SG placed inside `[[block]]` records** become those records'
  fields (they overlap C1's structs).
- **A naming pass.** SG named each new entry after its most-used module
  constant, so some read oddly where used: the save menu now reads
  `BattleDraw_Colour` and `FieldS_Backdrop`. One agent, tree-wide, renames by
  meaning with evidence; the gate proves the renames changed nothing. It runs
  alone, after C2, since it touches every group's files.

## 6. C4: the layout as one struct, and the strict rule

The round doc's old section 4: `tools/gen_symbols.py` grows `GameData`, one
struct spanning `.rdata` and `.data` from the typed entries in address order,
named padding between, `static_assert`ed on every member's offset and the
total size; the engine takes a `GameData*` at the image base and the macros
become members. Then the census's stricter rule
([`typed-data-census.md`](typed-data-census.md) 3): a raw accessor in
`src/game` fails CI, except the sites on a committed list with a reason
(seams, platform buffers, computed offsets).

## 7. Order, and what runs in parallel

C0 alone -> C1 (one agent) beside C3's entry work (SG, `symbols.toml` only) ->
C2 (seven groups) -> the C2 audit and its revert -> C3's naming pass alone ->
C4. Each merge one at a time with the gate; the census regenerated by the
coordinator after each stage.

## 8. For the owner

Nothing here changes what the player sees, so no ledger entries. Two calls:

1. **Merge stages A and B first** (the PR from `typed/b1`), so stage C
   branches from `main`. Recommended: stage C's diffs are easier to review
   against a merged B.
2. **A live smoke run at the round's end:** the headless `BOF3X_SHADOW='*'`
   self-test and one hash route, from a plain copy of `build/` as the
   install-gap round's end batch ran them on 2026-10-18
   ([`install-gap-round.md`](install-gap-round.md) 6; Defender stopped none of
   those copies). Identical machine code makes it a formality, but it is the
   only check that runs the game.

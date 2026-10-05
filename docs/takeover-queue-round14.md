# Round fourteen: the remainder of the game's code

**Status:** MEASURED (2026-10-03, at `aed35f8` on
`phase-3/capture-round-thirteen`, while round thirteen's end was still in
progress) - a plan, a cut and filled briefs. Nothing taken, nothing named,
no C++ or tool changed. **The scope and the order are decided** (section 6: the whole
remainder, in three launch sessions). The cut must be regenerated at the tip the round
starts from if anything on the round branch takes a function first
(section 5).

## 0. The answer in one paragraph

At `aed35f8` 8,649 of 10,246 catalogued starts are ours and 2,084 are not.
Of those, 533 are the platform and the library layer (the C runtime, the
MP3 decoder, the shell, the renderer and sound shims), 210 are jump-table
cases and not functions, and **1,341 are functions of the game's own code -
all that is left of it.** They fall into four address bands of about 340
each: the party sets' field actions with the fishing minigame, the menus /
windows / shop / master screens, the battle engine's last handlers with
what round thirteen left in the effect bands, and the community band. The
round is **four waves, 28 groups, behind a stage-A group of
seven shared helpers**, on the two harnesses that exist (no harness group).
After it nothing of the game's own code is Capcom's.

## 1. How it was measured

All output is in the session scratchpad
(`.../309e3952-1e51-4cd8-8b59-6c0e2b38bc89/scratchpad/round14/`); the cut is
also the main checkout's `analysis/round14_cut.tsv` (gitignored,
game-derived).

1. `tools/remaining_catalog.py` at the tip's `symbols.toml`
   (`catalog_aed35f8.md`, `.tsv`): 2,084 not ours.
2. `tools/label_runs.py` over the catalog (`labels/labels.tsv`): a class
   proposed for each of the 1,198 boot-resident and unlabelled starts, 786
   `evidence` and 412 `hypothesis`; 188 flagged as cases.
3. `make_cut14.py`: every start of parts 2 to 7, less the cases, into four
   waves by address, each wave's rows in address order cut at about 48 where
   the unit changes, never above 60 rows or about 14 KB by the catalog's
   extents, a run under 12 joined to its neighbour.
4. `tools/band_rows.py --byte-tables --groups` and `--edges` over the cut
   (`band_groups.txt`, `band_groups_rows.tsv`, `band_edges.txt`). **The tool
   stopped with `settle: no fixpoint in 8 rounds` on this cut**; it was run
   through the scratch wrapper `band14.py`, which executes the same source
   with the limit at 64. The tool itself is unchanged; raising its limit is
   a one-line fix for whoever next edits it. It flagged 22 more starts as
   inside their hosts' jump tables (`inside_host.txt`); the cut was made
   again without them.

| Part | Not ours | In this round | Left out |
|---|--:|--:|---|
| 0 Platform | 464 | 0 | the C runtime 242, the MP3 decoder 200, set-up 10, the shell 7, 5 not functions |
| 1 Library layer | 69 | 0 | renderer 44, PSX library 17, sound 8 |
| 2 Boot-resident | 699 | 1,341 together | 210 cases together |
| 3 Field modes | 7 | | |
| 5 Area overlays | 2 | | |
| 6 Scenario and character sets | 344 | | |
| 7 Unlabelled | 499 | | |

By the class the labels or the catalog propose (takeable rows only): field
core 422, battle engine 202, COMMU-paired 199, text / windows / menus 196,
effect objects 85, SHISU / SISYOU-paired 58, SCENA-paired 45, minigames and
master 31, shop / inn / save point 22, PLP-paired 17, SCE1xEF-paired 12,
top-level 13, scenario banks 12, battle effects 9, and 18 others. 224 rows
carry the `hypothesis` tier.

## 2. The waves and the groups

| Group | Band | Fns | Hidden | `hypothesis` | Reach | Bytes (catalog) | Not listed | Module | Harness | Mostly |
|---|---|--:|--:|--:|--:|--:|--:|---|---|---|
| R0A (stage A) | `0x51C390..0x524DA0` | 7 | 0 | 0 | 0 | 1,152 | 0 | rest_0a | scenario | the field actions' shared helpers |
| R1A | `0x51BA80..0x51D6D0` | 49 | 45 | 1 | 9 | 6,423 | 0 | rest_1a | scenario | field actions (5 PLP-paired) |
| R1B | `0x51D710..0x51F1F0` | 47 | 43 | 4 | 5 | 5,645 | 0 | rest_1b | scenario | field actions (11 PLP-paired) |
| R1C | `0x51F210..0x520C80` | 50 | 45 | 3 | 0 | 7,136 | 1 | rest_1c | scenario | field actions |
| R1D | `0x520E10..0x5226B0` | 46 | 42 | 0 | 0 | 5,880 | 0 | rest_1d | scenario | field actions |
| R1E | `0x5226D0..0x523EB0` | 47 | 42 | 2 | 0 | 5,981 | 0 | rest_1e | scenario | field actions |
| R1F | `0x523ED0..0x528970` | 49 | 40 | 10 | 1 | 8,391 | 0 | rest_1f | scenario | field actions, `Member_States` |
| R1G | `0x5289A0..0x52CCD0` | 43 | 28 | 1 | 0 | 8,807 | 2 | rest_1g | scenario | fishing (38 rows from `0x52ADA0`) |
| R2A | `0x5372E0..0x537ED0` | 19 | 12 | 18 | 0 | 2,920 | 3 | rest_2a | scenario | the scenario bands' leftovers |
| R2B | `0x551E40..0x57F330` | 39 | 26 | 5 | 0 | 13,913 | 1 | rest_2b | scenario | master / apprentice (23 SHISU-paired), 3 SCENA rows |
| R2C | `0x57F340..0x586980` | 60 | 50 | 23 | 10 | 7,162 | 0 | rest_2c | scenario | shop, inn, save point; master |
| R2D | `0x5869A0..0x58B1C0` | 51 | 41 | 14 | 11 | 7,891 | 0 | rest_2d | scenario | master; the field menu's screens |
| R2E | `0x58B1D0..0x58ED10` | 48 | 37 | 6 | 0 | 13,694 | 1 | rest_2e | scenario | the field menu's screens |
| R2F | `0x58ED40..0x596A90` | 48 | 32 | 13 | 0 | 9,302 | 1 | rest_2f | scenario | the field menu's screens, windows |
| R2G | `0x597FA0..0x59AA50` | 48 | 46 | 13 | 32 | 3,394 | 0 | rest_2g | scenario | window kinds, `MenuList_Kinds` |
| R2H | `0x59AA80..0x5A9860` | 34 | 23 | 13 | 6 | 7,075 | 1 | rest_2h | scenario | window kinds |
| R3A | `0x404180..0x437820` | 44 | 37 | 26 | 32 | 3,819 | 0 | rest_3a | boss | BATE's root and steps, small battle runs |
| R3B | `0x4468B0..0x44CFE0` | 60 | 54 | 0 | 49 | 4,600 | 4 | rest_3b | boss | `Effect_Handlers`' slots |
| R3C | `0x44D000..0x44E400` | 60 | 60 | 0 | 60 | 5,296 | 1 | rest_3c | boss | `Effect_Handlers`' slots |
| R3D | `0x44E4B0..0x44FF00` | 36 | 19 | 1 | 18 | 4,925 | 0 | rest_3d | boss | `Effect_Handlers`' slots and their helpers |
| R3E | `0x46A320..0x480190` | 50 | 20 | 6 | 0 | 10,399 | 0 | rest_3e | scenario | the effect bands' leftovers (26 SCENA-paired) |
| R3F | `0x480210..0x492580` | 48 | 36 | 0 | 0 | 9,735 | 2 | rest_3f | scenario | the effect bands' leftovers (11 SCE1xEF-paired) |
| R3G | `0x4925C0..0x5171E0` | 32 | 20 | 13 | 12 | 5,820 | 0 | rest_3g | scenario | the effect bands' leftovers, top-level |
| R4A | `0x452DD0..0x456D30` | 48 | 12 | 26 | 0 | 7,187 | 0 | rest_4a | scenario | field core below the community rows |
| R4B | `0x456D50..0x459E50` | 60 | 39 | 6 | 0 | 12,485 | 0 | rest_4b | scenario | community (43 COMMU-paired) |
| R4C | `0x459EE0..0x45C3A0` | 60 | 47 | 0 | 0 | 9,386 | 0 | rest_4c | scenario | community |
| R4D | `0x45C400..0x45E820` | 60 | 49 | 0 | 0 | 9,243 | 0 | rest_4d | scenario | community |
| R4E | `0x45E870..0x460C40` | 48 | 26 | 5 | 0 | 9,137 | 0 | rest_4e | scenario | community, minigames |
| R4F | `0x460CB0..0x464B60` | 50 | 39 | 15 | 0 | 8,600 | 1 | rest_4f | scenario | effect kinds and SCENA-paired rows above the community band |

Wave one is R1A..R1G (331, after R0A's 7), two R2A..R2H (347), three
R3A..R3G (330), four R4A..R4F (326): 1,341. "Reach" is the catalog's (a
traced run entered the row, or for a hidden start its host: an upper
bound). "Not listed" is the band tool's "code no list has" in the group's
spans, 18 in all; they are the group's. "Mostly" is the cut's proposed
class, a hint: no row was read by hand for this plan.

**The modules are `rest_<wave><letter>`** because the groups are mixed by
subsystem; a group that finds its band is one thing says so at the top of
its doc. No `src/game/rest_*` file exists.

**Wave three absorbs round thirteen's mop-up**: the rows that round's
groups returned or that no group held (its section 18 item 3) are R3E..R3G.

## 3. Stage A: seven helpers, no harness group

**R0A.** Before the helpers were taken out, wave one's six field-action
groups called each other in a ring (every pair, 1 to 10 sites each). All of
it goes through seven short functions: `0x522FB0` (45 sites from the six
groups), `0x51C390` (34), `0x522560` (18), `0x524DA0` (14), `0x521510`,
`0x51DD70` and `0x51C6A0` (8 each). Each one's callees are already ours
(`Sprite_ObjectAt`, `AreaMap_ByteAt`, `Sprite_PointInReach`,
`Field_EffectAhead`, `Effect_FindFree`, `AreaMap_Elevation`,
`MapView_SlopeAt`, `MapView_GroundAt`; the band tool's read of the calls
only). With them in a stage-A group, as EGT was in round thirteen, **wave
one has no edge between its groups at all**.

**The harnesses.** `scenario_harness` has the shapes this round's classes
want - `kSprite` and the field mode (round twelve), `kMenu` and the window
records (FS), `kEffect` (EKH) - and `boss_harness` with EH's widening
served round twelve's battle groups. So no harness group is staged. **Not
verified**: that `kMenu` fits the window-kind handlers of
`Window_Handler7KindTable` (a handler handed arguments wants a typed
stand-in per entry, in the group's fuzz file), and that the community
band's screens fit any shape. Wave two's and wave four's first reports say;
a fold follows a wave, as in round thirteen.

## 4. Merge order and the edges

396 call sites between groups (`band_edges.txt`), 133 of them into R0A.

- **The order of the waves is callee first**: the community groups call
  wave two's `0x57D520` (R2B, 11 sites) and `0x586160` (R2C, 9), and R3G
  calls into R1G once. Three sites run forward (R2B to R3G, R2G to R3B, R2F
  to R4F, one each): raw until the round's rebinding.
- **Wave one**: any order after R0A.
- **Wave two**: R2H, R2G, then R2B, R2C, R2F, R2E, R2D (each neighbouring
  pair calls both ways, 1 to 6 sites: the earlier one of a pair calls raw).
- **Wave three**: R3F, R3E, R3G; R3D, R3B, R3C (R3D's `0x44FB30` has 23
  sites in R3B and R3C; one site runs back from R3D to R3C).
- **Wave four**: R4F, R4E, R4D, R4A, R4C, R4B (R4D has 69 sites into R4E,
  R4C 33 into R4D; R4E 4 and R4D 11 run back).

## 5. Suspects, and what moves the cut

- **`NOTFN`, 210 starts**: the labelling tool's 188 cases and the band
  tool's 22 (`0x415C60`, `0x4201F0`, `0x422530`; seven in
  `0x538A60..0x546910`; five in `0x54AAD0..0x55D140`; seven in
  `0x56D240..0x578A40`). Most are cases of scenario and field hosts that
  are already ours, declined by those groups' docs.
- **R2B's three SCENA rows** (`0x551E40`, `0x553E50`, `0x559AD0`) each end
  before bytes the tool could not classify ("uncovered: data or
  unreached"): a read each.
- **224 `hypothesis` rows**, most in R3A and R4A (26 each), R2C (23) and
  R2A (18 of 19). The brief's rule (section 6, question 2) makes them the
  group's whatever they turn out to be.
- **Patches inside wave two's band**: the language overlays and the yes /
  no stops retarget call sites and operands in the menu and shop code
  (DIV-0018, 0026, 0027, 0059, 0064, 0065, 0069), and DIV-0041 patches
  bounds when wide. Each brief says to read the patch back; DIV-0046 was
  lost once this way.
- **The fishing rows call `Rand`**, and `caughFish.txt` replays a catch
  only while the count of `Rand` calls per frame is unchanged: R1G's live
  check is the `randlog` of that route.
- **Regenerate if the base moves.** Round thirteen's end listed a mop-up
  wave for its unplaced rows; if that is launched on the round branch, or
  anything else takes a function, rerun steps 1 to 4 at the new tip
  (`make_cut14.py` reads the catalog's file name: edit it). The briefs are
  generated (`make_briefs14.py <scratch> <round 13 scratch> <wave> <tip>`,
  `make_brief_r0a.py`) and carry `<TIP>` until then.

## 6. Decided by the owner (2026-10-03)

1. **The scope is the whole remainder, launched as three sessions** to keep
   a single session's size down: **session one** stage A (R0A), then wave
   one and wave two (685); **session two** wave three (330); **session
   three** wave four (326). The waves and groups are as cut; a wave is
   still launched and merged on its own inside a session.
2. **The order is 1, 2, 3, 4** (callee first), as proposed.
3. **The `hypothesis` rule stands as the briefs have it**: every start
   that is a function is the group's, whatever class the cut gave it (round
   thirteen left such rows out, and they are this round's wave three).
4. **What follows the round**: parts 0 and 1 (533 starts) are not
   per-function takeovers - the MP3 decoder's replacement and the C runtime
   are [`IDEAS.md`](IDEAS.md) I8 / I12.

## 7. The scratch, for whoever launches

In `.../309e3952-1e51-4cd8-8b59-6c0e2b38bc89/scratchpad/round14/`:
`brief_r0a.md` and `brief_r1a.md`..`brief_r4f.md` (each wants `<TIP>`: rerun
the generators with the SHA); `merge_group14.sh <group lowercase> <scratch>`
(`MOD=rest_0a` and so on; branches `phase-3/round14-<group lowercase>`),
`runner14.sh` with `pending14.txt`, `verify_tip.sh`, `keepboth.py`,
`one_grow.py`, `rebind_resolve.py`, `table_resolve.py` - round thirteen's,
renamed and not yet run; `edges_summ.py` (section 4's figures); `band14.py`.
The plan's worktree is `r14plan/` there (branch `phase-3/round14-plan`).

## 8. Not verified

- No function was read. Classes, units and "what a group mostly is" are the
  tools' proposals; the labelling tool's `evidence` tier measured 93.6%
  right on our own code ([`labelling-pass.md`](labelling-pass.md) section 2).
- Sizes in section 2 are the catalog's extents; the band tool's read sizes
  differ (R2B: 13,913 against 6,765 read).
- The merge scripts are copies with names replaced, not run.
- Whether four waves of this size fit one usage window each: round
  thirteen's waves of 302 to 379 did.
- The base's proof (`'*'` narrow and wide at `aed35f8`) is the round
  thirteen session's, not repeated here.

## 9. Stage A and wave one, as they ran (2026-10-04)

On `phase-3/capture-round-fourteen`, cut from `main` at `5a94224` (round
thirteen, PR #40) with this plan's commits cherry-picked; the code at
`aed35f8` and at that tip is the same, so the cut stood. Before stage A the
state hash was built ([`state-hash.md`](state-hash.md)): the live check of
this round, since the call trace has less to arm each wave.

| Group | Functions | Rounds | Controls planted / refused | Merge | Ours after |
|---|--:|--:|---|---|--:|
| R0A ([`rest_0a.md`](rest_0a.md)) | 7 | 140,000 | 53 / 52 | `ba2c3c3` | 8,655 |
| R1B ([`rest_1b.md`](rest_1b.md)) | 47 | 188,000 | 88 / 86 | `1a81381` | 8,702 |
| R1E ([`rest_1e.md`](rest_1e.md)) | 47 | 188,000 | 76 / 76 | `454ee82` | 8,749 |
| R1A ([`rest_1a.md`](rest_1a.md)) | 49 | 196,000 | 85 / 82 | `47ef5cb` | 8,798 |
| R1C ([`rest_1c.md`](rest_1c.md)) | 51 | 204,000 | 101 / 100 | `71f85ed`, `1ae6e1a` | 8,849 |
| R1G ([`rest_1g.md`](rest_1g.md)) | 45 | 270,000 | 103 / 103 | `db0c327` | 8,894 |
| R1F ([`rest_1f.md`](rest_1f.md)) | 49 | 392,000 | 108 / 105 | `4df1600` | 8,943 |
| R1D ([`rest_1d.md`](rest_1d.md)) | 46 | 276,000 | 103 / 101 | `4962b89` | 8,989 |

341 functions (the cut's 338 and three starts no list had: R1C's `0x51FA30`,
R1G's `0x528BE0` and `0x52BF90`), every group 0 mismatches, every control not
refused an equivalent mutant with a refused near variant (each group's doc
lists them). Each merge built and ran its own shadow and `'*'` in the
verification worktree, exit 0, `ledger_check` 0 errors; the tip `4962b89`
also with `BOF3X_WIDE=1`. No group needed a ledger entry.

**What the wave found out.**

- **R1G's band is the fishing spot**, not field actions: the rest of the
  leader's state 9 and game mode 8's fish. The cut's 3,056 bytes for
  `0x52BBD0` ran on over `0x52BF90`.
- **Two groups gave one name to two functions**: R1A's `0x51D6D0` and R1C's
  `0x520350` were both `PartyAction_WaitEffect`, and `gen_symbols.py` stopped
  R1C's merge. R1C's is `PartyAction_WaitEffectEnd` since `1ae6e1a`. The
  party sets repeat a handful of bodies instruction for instruction (the
  Begin, Resolve, cell-pickup and strike shapes; R1F lists `0x524BB0` equal to
  `0x51D4E0`, `0x51F880`, `0x522E20`), so the next waves' briefs say: a name
  not carrying the group's own set, form or screen is checked against
  `symbols.toml` and the sibling groups' likely names first.
- **R1E reports the toolchain dropping bits** (its doc's section 4):
  `(pointer & 0xFFFFFF00) | byte` passed as an `unsigned long` argument was
  emitted as the byte alone; an empty `asm volatile` on the upper part is its
  workaround. Read, not reproduced by the coordinator: a minimal case is owed
  before it is called a compiler defect.

**Live checks at `4962b89`** (the machine quiet; the verification worktree's
build copied to a launcher of its own):

- The attract sequence, state hash: identical to the pair `attract_r14_*` on
  all 10,305 ticks; the oracle identical at every logged frame.
- `combat.txt`, state hash: identical on all 2,561 ticks to a pair recorded
  with the same build. Against the morning's pair it differed on seven pages
  from tick 1 - the English overlay's pointers into our DLL, which moved with
  the build. **Under a language overlay the reference pair is recorded with
  the build under test** ([`state-hash.md`](state-hash.md) section 6).
- `caughFish.txt`, the route that enters R1G: ours runs to `done`, and its
  `Rand` count is the owner's recording's on all 3,889 frames
  (`analysis/shots/fishing_catch2/randlog_recording.txt`). **The original side
  of this route crashes** (`*` with the route A/Bs' keep list: an access
  violation at `0x5A9E45`, Capcom's code, near recipe frame 852, both runs),
  so the route has no state-hash pair. Only five functions are ours on that
  side, so it is not wave one's; not diagnosed.

**Debts** (for the round's end unless a wave trips on one):

1. `entries_logic.txt` lines that carry a host's extent where the function is
   now ours with its own: R1C's four (`0051F4B0`, `0051F880`, `005206C0`,
   `00520C80` - the last runs into R1D's range), R1D's four (`00521200`,
   `005218C0`, `00521F80`, `00522320`), R1F's four (`0x5242B0`, `0x524BB0`,
   `0x525150`, `0x5287B0`), R0A's two that cover R1A's hidden starts.
2. The harness fold: `Sprite_LoadPalette`'s standard row hashes a destination
   the callee only writes (it hid a wrong stride from R1A's C74); R1C's six
   field stand-ins the standard set lacks; `scenario_harness.cpp`'s `FX_RAW`
   row for `0x52B330` answers garbage where the function answers al 0 / 1
   (R1G); seven `FX_RAW` rows now name functions of ours.
3. The owner's: `PartyAction_WaitEffectDone` `0x521A20` (R1D's L1, and the
   same read in R1C's `0x520350`) reads a byte inside `Gfx_PacketPools` when
   all 20 effect slots are full, which R1D says play reaches - ours copies the
   read; ending the action instead would be a ledger entry. And R1B's: 5 of 16
   strikes on a `0xF6` / `0xF7` cell set `Field_Request` to 2 without opening
   a message.
4. Unbounded indexes: every dispatcher of the wave aborts past its table and
   several states abort on an effect index past 19 or an object index past
   `0x21`, where the original reads on. No group could show play reaching
   one; each doc lists its own.
5. The caught-fish route's original side (above), and R1E's minimal case.
6. De-duplication of the repeated bodies is a refactor for after the round.

## 10. Wave two, as it ran (2026-10-04)

From `fa583bc`, eight Opus agents; the briefs carried wave one's addendum
(one name once, R1E's caution, the stand-in and crash lessons).

| Group | Functions | Rounds | Controls planted / refused | Merge | Ours after |
|---|--:|--:|---|---|--:|
| R2A ([`rest_2a.md`](rest_2a.md)) | 22 | 132,000 | 71 / 70 | `d9f424f` | 9,011 |
| R2G ([`rest_2g.md`](rest_2g.md)) | 48 | 192,000 | 91 / 91 | `cee7a20` | 9,059 |
| R2D ([`rest_2d.md`](rest_2d.md)) | 51 | 306,000 | 118 / 117 | `df20d18` | 9,110 |
| R2E ([`rest_2e.md`](rest_2e.md)) | 49 | 294,000 | 121 / 119 | `8a110b1` | 9,159 |
| R2H ([`rest_2h.md`](rest_2h.md)) | 36 | 144,000 | 84 / 83 | `76f1fc9` | 9,195 |
| R2F ([`rest_2f.md`](rest_2f.md)) | 49 | 294,000 | 144 / 139 | `b5c84a9` | 9,244 |
| R2C ([`rest_2c.md`](rest_2c.md)) | 61 | 244,000 | 188 / 186 | `10d770d` | 9,305 |
| R2B ([`rest_2b.md`](rest_2b.md)) | 38 | 152,000 | 106 / 103 | `2698d13` | 9,343 |

354 functions (the cut's 347 less three starts that are cases of hosts
already ours - R2B's `0x551E40`, `0x553E50`, `0x559AD0` - plus ten no list
had: R2A's `0x537760`, `0x537B10`, `0x537CE0`; R2B's `0x57E720`, `0x57EDF0`;
R2C's `0x583350`; R2E's `0x58CFC0`; R2F's `0x596530`; R2H's `0x59C810` and
`0x59E160`), every group 0 mismatches, every control not refused an equivalent
mutant with a refused near variant. Each merge built and ran its shadow and
`'*'`; the tip `f348fc1` (the last merge plus DIV-0073's text) was verified
alone, narrow and wide: exit 0, 9,343 ours, `ledger_check` 73 entries 0
errors.

**What the wave found out.**

- **The bands are not what the cut's labels said**: R2A is HP / AP helpers,
  a mode-11 sprite pass and four linked-object handlers, not scenario code;
  R2C holds `Save_BuildBlock` and `Save_QuickWrite` (F12's path, IDEAS I18);
  R2H's `Menu_DrawVerbPair` `0x59E160` was filed as renderer by its address
  range. **The "platform and library" rows from `0x59E4F0` on were not read
  by anyone**: one of them was game code, so that bucket wants a pass before
  the round is called complete.
- **Two tables were named by two groups each** (an address entered twice,
  which the merge's check refuses): `0x667354` (R2D's
  `FieldMenuItems_State5Steps` kept, R2E's entry removed on its branch) and
  `0x663E28` (R2C's `MasterFigure_States` kept, R2B's removed). The reader
  is one group's and the entries another's; `collide.py` in the scratch
  checks names and addresses across the reported branches before a merge.
- **A merge left a second `FishingText_Arm()`** between two groups' injects
  (wave one's keep-both resolution); removed in `48b7d97`, and `one_grow.py`
  now keeps one block of each of the four arming calls before `InjectReport`.
- **R2B edited two harness rows** (`Effect_Spawn` in `area_harness.cpp`,
  `Effect_SpawnAt` in `scenario_harness.cpp`, theirs to ours, the masks
  unchanged) and re-keyed eleven area fuzz files by name: once the two
  names are ours `Register` stops at a row that lists them as Capcom's.
  Read by the coordinator; they are right.
- **DIV-0073** (R2B): the masters' model's light matrix, zeros where the
  original copies 26 bytes of stale stack into `Gte_Matrix2`. The owner's
  word is owed.
- **The merge runner outlived its kill.** The harness stops a background
  command at two hours; the runner's shell went on, merged R2B beside the
  runner started to replace it, and three processes built and self-tested in
  the verification worktree at once (a `Permission denied` from ninja, an
  exit 126, a launcher gone, two games left running). The repository was
  not harmed - one merge commit - and the tip was verified again alone.
  **Before a second runner starts, `tasklist` for the first's shell**; a
  wave of eight merges wants more than two hours, so queue it in two parts.

**Live checks at `f348fc1`** (the machine quiet, the state hash, each route's
pair recorded with this build):

| Route | Ticks | Ours against the pair |
|---|--:|---|
| the attract sequence (the pair `attract_r14_*` of `dafd4a3`) | 10,305 | identical but tick 3 on seven pages (the start-up upload and a sound byte - the pages two originals differed on at tick 3 in the first trials); the oracle identical at every logged frame |
| `combat.txt` | 2,561 | identical |
| `menu_screens.txt` | 1,729 | identical |
| `field_menu.txt` | 1,409 | identical |
| `shop.txt` | 3,137 | **differs**, below |
| `masterAndManillo.txt` | - | no pair: the original side crashes at `0x5A9E45` near frame 965, as `caughFish.txt`'s does; ours runs to `done` |

**The shop route's two differences, neither wave two's.**

1. One window record's x (`WindowRecords + 0x2F8`, `0x803458`) is `0xD9` in
   ours and `0xD8` in the original for 141 ticks from tick 1000. It goes
   away with the 2026-10-03 fix wave's layout divergences switched off
   (`ShopYesNoLayout`, `ShopAskRow`, `MasterAskLayout`, `TradeLeaveLayout`,
   `TradeConfirmLayout`, `BattleEquipLabelsRow`, `MsgBoxEffectSpaceSkips`):
   DIV-0027's amendment, as meant. **The route A/Bs' `DIVS` list in
   `analysis/validate_combat.sh` predates those seven names**; an ours side
   compared against Capcom's wants them in it.
2. From tick 2975 to the end, twelve bytes at `0x905BC6..0x905BD1` differ
   and ours has built one more primitive a frame (`Gfx_PacketNext` 0x18
   further). It stays with every one of wave two's 354 functions switched
   back to Capcom's (8,990 ours) and with all the divergences above off, so
   **it is older than this wave** - a difference the call hash of rounds
   nine to thirteen could not see. `0x905BC0` is a table of 4-byte records
   that `0x57F340`, `Save_BuildBlock` `0x5806F0`, `0x5809C0`, `0x587CD0` and
   `0x588DC0` reference. Not diagnosed: which function of ours writes it
   differently is the next read (a dump at tick 2975 on both sides, then
   `BOF3X_ORIGINAL` by module over the callers of those five).

**Debts added to section 9's list:**

7. The shop route's second difference (above) - the state hash's first
   finding that is not already in the ledger.
8. The original side's crash at `0x5A9E45` on `caughFish.txt` and
   `masterAndManillo.txt`. **Cause found 2026-10-04 evening** (captures every
   15 frames to the crash, `analysis/shots/fish_crash_orig/`: it comes as the
   fishing banner's first text starts to draw): the route A/Bs' keep list
   leaves `LoadDatFile` ours, so the English fishing lines are loaded
   (DIV-0069), while `*` hands the banner's draw back to Capcom's code, which
   reads the one-byte Latin text as two-byte glyphs and unpacks a glyph far
   outside the font. With the eight functions that consult
   `FishingText_On()` kept ours as well (`EffectKind03_ShowName`,
   `EffectKind0F_LineStart`, `_LineType`, `_LineNext`, `_LineScroll`,
   `_LineFade`, `_DrawGlyph`, `_DrawToggles`) the original side of
   `caughFish.txt` runs to `done` (13 ours). So it is the A/B's set-up, not a
   defect of either code: **a fishing route's original side wants those eight
   in its keep list**, and the pair for the two routes is owed at wave
   three's live check (the machine quiet). The same shape as HANDOFF's trap
   "an A/B original side that keeps a function ours can keep a dependency".
9. `window_task_callees.h`'s `kRecordHandlers` line is still raw (R2F left
   it, shared with R2G and R2H); DIV-0027's note names `MenuList_TitleBox`
   where the function is `MenuList_WideTitleBox` `0x59A2E0` (R2G).
10. Host-extent lines in `entries_logic.txt` to split, per group doc: R2A
    one, R2B four, R2C four, R2D five, R2E three, R2F seven, R2G two, R2H
    seven.
11. The fuzz of three functions leaves a path out under a Latin overlay
    (`Win2_DrawItemList` and `MasterScreen_AskYesNo`'s re-aimed calls,
    R2H's DIV-0059 title site): the English paths there are unfuzzed.
12. For the owner: `FieldMenu_CampAllowedCell` masks the cell with `0xF0`
    and compares with `0xA1`, `0xAF`, `0x91`, which cannot match (R2D);
    `Save_BuildBlock` shows the level and experience of record 0 beside the
    leader's name (R2C); four more states read effect record 255 inside
    `Gfx_PacketPools` when no effect slot is free (R2A - the read of
    section 9's item 3).

## 11. Wave three, as it ran (2026-10-04), and the pause

From `7f116a2`, seven Opus agents, launched the same day at the owner's word
(section 6 had it for a second session). The briefs carried both earlier
waves' addenda.

| Group | Functions | Rounds | Controls planted / refused | Merge | Ours after |
|---|--:|--:|---|---|--:|
| R3A ([`rest_3a.md`](rest_3a.md)) | 45 | 270,000 | 75 / 75 | `33dd722` | 9,388 |
| R3C ([`rest_3c.md`](rest_3c.md)) | 61 | 366,000 | 128 / 127 | `0b4918a` | 9,449 |
| R3E ([`rest_3e.md`](rest_3e.md)) | 50 | 200,000 | 94 / 93 | `3c135bc7` | 9,499 |
| R3G ([`rest_3g.md`](rest_3g.md)) | 32 | 128,000 | 139 / 138 | `adef385` | 9,531 |
| R3D ([`rest_3d.md`](rest_3d.md)) | 36 | 216,000 | 77 / 76 | `edff087` | 9,567 |
| R3F ([`rest_3f.md`](rest_3f.md)) | 50 | 200,000 | 112 / 112 | `98bcdaa` | 9,617 |
| R3B ([`rest_3b.md`](rest_3b.md)) | 64 | 384,000 | 116 / 115 | `80bc7b0` | 9,681 |

338 functions (the cut's 330 plus eight no list had: R3A's `0x4041B0`; R3B's
four `Effect_Handlers` slots `0x44C040`, `0x44C120`, `0x44C170`, `0x44CF60`;
R3C's `0x44D8B0`; R3F's `0x48F1E0`, `0x48F300`), every group 0 mismatches,
every control not refused an equivalent mutant with a refused near variant.
R3E, R3F and R3G are round thirteen's unplaced rows: **that round's mop-up
is done.** The merge runner ran in two parts, each ended by its own `END`
line, with no process of the first alive before the second started. The tip
`ade9f98` (the last merge plus DIV-0041's amendment) verified alone: `'*'`
exit 0 narrow and with `BOF3X_WIDE=1`, 9,681 ours, `ledger_check` 73 entries
0 errors.

**What the wave found out.**

- **DIV-0041 amended**: R3F's `EffectKindAA_DrawFill` `0x492400` is a
  full-frame fill and is drawn through the widescreen helpers;
  `EffectKindA8_DrawBar` `0x4920F0` (the frame's width, not its height) is
  left narrow, the owner's call. `0x510780`, which the ledger called
  `AreaMap_FrameAreaBD (Capcom's)`, is `AreaMapBD_BuildView` and ours (R3G);
  it reads DIV-0041's four cull operands and DIV-0062's array and bound back
  from the code.
- **`0x656AB8` is four tables** (game modes 8 to 11), so round twelve's
  `Mode8_Step5` and `Mode8_Step8` are step 1 of modes 9 and 10 (R3G). A
  rename for the round's end.
- **Three effect slots write a record of the wrong side** when the actor and
  the target are on different sides - slot 47 (R3B's L1), slot 97 (R3C),
  slot 129 (R3D): a bit lands in the window records or the task slots. Each
  is deterministic and reproduced; whether play reaches one with such a pair
  is not established. The owner's, if it does.
- **Corrections on the way**: `NewGame_InitCharacters`' offsets and three
  comments' battle-frame address (R3A); `Battle_PsiStatusDeathAffinity`'s PSX
  twin, and BE5's doc calling `0x44EB50` slot 124 where it is 126 (R3D).
- **Live reach** (from existing traces): `cutsceneAndNue.txt` enters R3A's
  `0x435A20`, R3E's `EffectKind41_DrawNumber` and four of R3D's status
  helpers; `dragonTransform.txt` enters three of R3D's. The rest of the wave
  is fuzz only.

**The live checks are held, by the owner's decision of 2026-10-04 evening:**
one large validation after wave four, not one a wave - each merge is
self-tested, and a late finding is placed by switching modules back with
`BOF3X_ORIGINAL` (section 10's bisect took 16 minutes for eight modules).
**And they are Chinese against Chinese** (the owner, the same evening): no
language overlay on either side, so the original side is Capcom's code
throughout and the pair outlives builds; English is a smoke test of ours
only (the route runs to `done`, its `randlog` the recording's). The batch is
written and not run: `final_live.sh` in the session-`7bf3959f` scratchpad
(the attract sequence against `attract_r14_*`, then ten routes - `combat`,
`shop`, `menu_screens`, `field_menu`, `cutsceneAndNue`, `dragonTransform`,
`whelpBoss`, `worldMapAndAreaTransition`, `caughFish`, `masterAndManillo` -
each as two originals and ours under `--lang original`, with the English run
of ours beside it). The routes were recorded under English and text timing
differs, so which of them replay under Chinese is the batch's first answer;
those that drift want an English pair (the fishing ones with section 10's
eight names kept) or a new recording, the owner's choice.

**Paused here at the owner's word.** Wave four (R4A..R4F, 326 functions, the
community band; merge order R4F R4E R4D R4A R4C R4B) is not launched: its
briefs are generated by `make_briefs14.py <scratch> <round 13 scratch> 4
<tip>` and `post_brief2.py <scratch> 4 <tip>`. After it: the large
validation, the debts of sections 9 to 11, and then
[`platform-layers-plan.md`](platform-layers-plan.md).

**Debts added:**

13. The 59 call sites of R3B and R3C that reach R3D's helpers by raw
    address, and R3D's tail jump to R3C's `0x44D8B0`, R3E's five to R3F's
    `0x480300`, R3G's two to R3F's `0x492400`: rebinding between the wave's
    groups, as every round's end.
14. Harness rows keyed by address that now name functions of ours and could
    be the `_OURS` form: R3A's four, R3B's three `kThrough`, R3D's three,
    R3E's eighteen, R3F's five, R3G's seven; `0x491E30`'s row compares a
    whole word where the function reads a word's half (R3F), `0x44F1D0`'s
    and `0x5100B0`'s masks are wider than the functions read.
15. Host-extent lines in `entries_logic.txt`: R3A two, R3B two, R3E two,
    R3G one.
16. DIV-0027's note on the two help-line choosers names `MenuList_TitleBox`
    and `0x59AE00`; R2G says the function is `MenuList_WideTitleBox`
    `0x59A2E0`. The two do not agree and neither was read by the
    coordinator.

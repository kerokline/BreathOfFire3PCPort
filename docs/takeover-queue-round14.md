# Round fourteen: the remainder of the game's code

**Status:** MEASURED (2026-10-03, at `aed35f8` on
`phase-3/capture-round-thirteen`, while round thirteen's end was still in
progress) - a plan, a cut and filled briefs. Nothing taken, nothing named,
no C++ or tool changed. **The scope is a proposal and the owner's to
decide** (section 6). The cut must be regenerated at the tip the round
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
proposal is one round of **four waves, 28 groups, behind a stage-A group of
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

## 6. For the owner

1. **The scope.** One round for the whole remainder (1,341, four waves), or
   a smaller round first - [`HANDOFF.md`](HANDOFF.md) named the camp's
   windows and fishing (about 80, with routes) as round fourteen's
   candidates before this count was made. As cut, those are inside R1G and
   wave two; wave one and wave two can run as a round of their own (678)
   with the battle and community waves after.
2. **The `hypothesis` rule.** Proposed: every start that is a function is
   the group's, whatever class the cut gave it (round thirteen left such
   rows out, and they are this round's wave three).
3. **The order.** Proposed 1, 2, 3, 4 (callee first). The community wave
   has no route; the fishing and camp rows have two.
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

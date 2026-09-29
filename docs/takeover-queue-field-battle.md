# Round twelve: the field modes and the battle engine, cut into fourteen groups

**Status:** PROPOSED (2026-09-28 night) - a plan and a cut, not scheduled
past the owner's word of 2026-09-28 ("field modes and battle as round
twelve, the area overlays as round thirteen"). Listed as
[`IDEAS.md`](IDEAS.md) I27 (this round) and I28 (round thirteen, section
9). The method is round eleven's ([`takeover-queue-round11.md`](takeover-queue-round11.md):
one harness group in stage A, the rest staged behind it, one merge at a
time, fuzz-only); the numbers are `tools/remaining_catalog.py` run at
`main` `c4b0d32` (6,238 ours, 4,446 not), the cut is section 3 and the
per-function table `analysis/round12_cut.tsv` (game-derived, never
committed; section 8 regenerates it).

## 0. The question, and the answer in one paragraph

Rounds nine to eleven took the code the game loads as overlays - spells,
chapters, areas, bosses - because each has a table that enumerates it.
What is left of the *game* (parts 3 and 4 of
[`remaining-catalog.md`](remaining-catalog.md)) has no such table: it is
the field modes' and the battle engine's resident code that no recorded
route entered, so no reach-built queue ever listed it. **623 functions:
323 field, 300 battle**, in 55 address runs, about 119 KiB. They cut by
address into fourteen groups of 31 to 57 functions, seven a side, on the
harnesses that exist. The battle side has a live route since tonight -
`tools/recipes/dragonTransform.txt` enters 58 of them - and the field side
has none yet, which is the owner's part (section 5). With the eight engine
neighbours round eleven's debts name (section 2) the cut is 631.

## 1. The measurement

`tools/remaining_catalog.py --symbols <main's symbols.toml>` at `c4b0d32`.
Every row below is a function start (`pc_funcs` or `pc_hidden`) with no
`impl` line; "hidden" is a pointer-reached start inside another start's
extent; "reached" is any traced run, and it is **0 for every row** - by
construction, since every reached function was taken by round nine.

| Group (catalog label) | Functions | Hidden | KiB | Names | PSX twins |
|---|--:|--:|--:|--:|--:|
| Field core (`GAME.EMI`) | 127 | 100 | 18 | 0 | 76 |
| Event script | 77 | 30 | 15 | 2 | 0 |
| Field objects | 55 | 32 | 14 | 15 | 0 |
| Shop / inn / save point (`SHOP.EMI`) | 51 | 37 | 13 | 0 | 43 |
| Sprite draw, map and draw layers, field menu | 13 | 11 | 5 | 0 | 2 |
| **Field, part 3** | **323** | **209** | **65** | 17 | |
| Battle engine (`BATTLE.EMI`) | 264 | 187 | 45 | 0 | 212 |
| Battle extra (`BATE.EMI`) | 16 | 13 | 2 | 0 | 6 |
| Battle result (`BATL_END.EMI`), `BATL_OVR.EMI` | 16 | 7 | 5 | 0 | 16 |
| Battle magic effects (`BMAGIC`) | 4 | 4 | 2 | 0 | 4 |
| **Battle, part 4** | **300** | **209** | **54** | 0 | |

"Names" are ours (17 field functions named from readings, none taken);
"PSX twins" are `pairs_propagated.json` matches - a name to transfer and
verify, not a name held. Two thirds of each side is hidden: state handlers
and table slots inside larger extents, the shape the spell and boss rounds
were made of. 130 of the hidden starts (66 field, 64 battle) sit inside
the extent of a function that is already ours; the consolidated entry list
cuts a host at the next listed start, so these are separate functions the
host's extent overlapped, not code we have reimplemented.

**The address runs** (a gap over 0x400 bytes starts a new one), which are
the material of the cut:

| Side | Run | Fns | What the catalog sees |
|---|---|--:|---|
| battle | `0x42D7A0..0x42E0D3` | 16 | BATE: windows, `Input_Pressed`, `MoveScript_WaitWordDA` |
| battle | `0x42ED90..0x42F070`, `0x42F5E0..0x42FF70` | 10 | the command menu's tail: `Battle_MenuSteps`, `Cmd_AutoBattle_Begin`, the ability/item action steps |
| battle | `0x4315C0..0x432B70` | 13 | BATL_END / BATL_OVR: the result screen's remainder, `Card_CopyIconFrame2` |
| battle | `0x433650..0x435104` | 24 | `Sprite_Current` x120, `WindowRecords` x41: the action tasks |
| battle | `0x435AB0..0x4366E0` | 16 | `Rand` x4, `Battle_ActorIsOut` x4: the action's begin (the transform's, section 5) |
| battle | `0x436BC0..0x437462` | 19 | `EnemyOp_*Subs` tables: the enemy ops round eleven left |
| battle | `0x441A10..0x442F97` | 33 | `Field_State` x64, `Formation_ApplyStatMods`, `Char_RecalcStats`: battle objects and poses |
| battle | `0x444660..0x447FD0` | 19 | damage (`0x446110`, `0x4461B0`, `0x4463E0`), faster side, the target picker `0x447F40` |
| battle | `0x448BA0..0x44AAC9` | 34 | `WindowRecords` x190: the battle menus' windows |
| battle | `0x44B240..0x44CD00` | 7 | enemy AI row helpers |
| battle | `0x44FDE0..0x452BE7` | 68 | `WindowRecords` x127, `LoadDatFile` x3: the Dragon command (`0x4525B0`), the transformation (`0x4514A0`..), the trail task `0x452B60` |
| battle | `0x453300..0x453F96` | 6 | `PartyWorkingRecords` x16: buffs and status picks |
| battle | `0x4CEB40..0x4CF4A4` | 4 | BMAGIC: `Draw_OtSlot`, `MapView_ScreenXY` |
| battle | `0x597FC0..0x599B50`, `0x59D640` | 31 | `Menu_DrawHand` x4: the battle windows, the gene screen (`0x598A30`..`0x598E90`) among them |
| field | `0x461800`, `0x469D10..0x46D5ED` | 81 | `Sprite_Current` x454, `AreaMap_*`: field core, one long run, 58 hidden |
| field | `0x5172C0..0x5195F9` | 11 | `Field_Object*Direction`: object steering |
| field | `0x525390..0x526DB0` | 46 | `Field_State`, `MapView_GroundAt`, `Field_JumpStart`: the leader's movement |
| field | `0x52D080..0x533BA0` (7 runs) | 45 | event script: message pools, `Text_DrawAt`, `Encounter_RollInitiative`, `Zenny_Add` |
| field | `0x5341C0..0x5372D8` (4 runs), `0x56D240` | 32 | event script: `Flags_Set`, `Sprite_EnsureAnimation` x11, `Char_LoseHp` |
| field | `0x56E020`, `0x570870..0x571090`, `0x5728D0`, `0x593960..0x594060` | 12 | draw layers (`Draw_OtSlot`), a map reader, the sprite-draw prompt states |
| field | `0x5738A0..0x57CD89` (8 runs) | 44 | field objects: `NameTable_Armour`, `Menu_DrawBox`, `EventObj_*`, `MoveScript_*` |
| field | `0x57FF80..0x5859F9` (3 runs), `0x58C7A0` | 52 | SHOP: `WindowRecords` x265, `Field_ConfirmButtons`: the shop screens' remainder and one field-menu window |

## 2. What these functions are, to a harness

Nothing new in shape. The battle side is the engine the boss harness
already fakes around: round eleven's debts
([`takeover-queue-round11.md`](takeover-queue-round11.md) section 7, item
1) list "louder stand-ins" for `Battle_OpenMsgWindow`,
`Battle_RemoveFromTurnOrder`, `BattleTask_Create` and `0x446DE0`, and
item 2 lists outbound raw calls "to code nobody owns" - `0x446700`,
`0x437450`, `0x4376A0`, `0x4376F0`, `0x452B60`, `0x441090`, `0x446DE0`,
`0x446E00`, `0x446E20`, `0x454A80`, `0x455290`. Three of the eleven are part 4 rows (`0x446700` BE4, `0x437450` BE3,
`0x452B60` BE6); the other eight are part 2 or 7 rows of 22 to 102 bytes
lying in the battle bands, and the cut adds them to the band's group
(`0x4376A0`, `0x4376F0`, `0x441090` BE3; `0x446DE0`, `0x446E00`,
`0x446E20` BE4; `0x454A80`, `0x455290` BE6), so that after this round
no boss or spell code calls raw into engine code. So the
battle groups replace stand-ins with the real thing, and the fuzz that
proves them is the boss harness's recorder over a wider band.

The field side is what the scenario and area harnesses call *into*:
`Sprite_Current`, `Field_State`, `AreaMap_*`, `MapView_*`, the event
script's message pools. `scenario_harness` already seeds the field record
and the sprite pool for chapter code; the field groups fuzz the callees of
that code with the same seeds.

**Two harness groups, then, and neither is new code**: EH widens
`boss_harness` (its band constant `0x437A00..0x441000` becomes the union
of the battle runs above, its standard-callee list gains the engine
entries the groups take, and round eleven's six fold-backs land in it -
they are its debt anyway); FH does the same for `scenario_harness` over
the field runs. Round nine's rule holds: **groups do not edit a
harness**; each side's harness group merges first and the seven groups
behind it run stage B against its API.

## 3. The groups

Address bands, about 50 functions a group, whole runs where a run fits and
a run split at a start where it does not (the 80-start field-core run at
`0x46BBF0`, the 68-start Dragon run at `0x451480`). **A group owns the
functions `analysis/round12_cut.tsv` lists for it**, not a band; the bands
are how the table was made.

| Group | Band | Fns | Hidden | Bytes | What |
|---|---|--:|--:|--:|---|
| EH | - | 0 (+ fold-backs) | | | the boss harness widened to the battle runs; round eleven's six fold-backs |
| BE1 | `0x42D7A0..0x432B70` | 39 | 28 | 7,063 | BATE's windows, the command menu's tail, the result and overlay remainder |
| BE2 | `0x433650..0x437030` | 48 | 36 | 8,130 | the action tasks and the action's begin (`0x435AB0`, the transform's path) |
| BE3 | `0x437030..0x442F97` | 47 | 41 | 5,828 | the enemy ops left after round eleven, battle objects and poses (`0x442420`); `0x4376A0`, `0x4376F0`, `0x441090` from the debts |
| BE4 | `0x444660..0x44AAC9` | 56 | 25 | 9,034 | damage, faster side, the target picker, the battle menus' windows; `0x446DE0`, `0x446E00`, `0x446E20` from the debts |
| BE5 | `0x44B240..0x451480` | 47 | 40 | 8,769 | enemy AI helpers and the Dragon run's first half |
| BE6 | `0x451480..0x455290`, `0x4CEB40..0x4CF4A4` | 40 | 19 | 10,085 | the transformation (`0x4514A0`..`0x452080`), the Dragon command task `0x4525B0`, the trail task, buffs, BMAGIC's four; `0x454A80`, `0x455290` from the debts |
| BE7 | `0x597FC0..0x59DB61` | 31 | 20 | 7,261 | the battle windows: the gene screen, the result windows |
| FH | - | 0 | | | the scenario harness widened to the field runs |
| FC1 | `0x461800..0x46BBF0` | 41 | 31 | 6,786 | field core, first half (`0x469D10` run) |
| FC2 | `0x46BBF0..0x46D5ED` | 40 | 27 | 6,134 | field core, second half |
| FC3 | `0x5172C0..0x526DB0` | 57 | 48 | 7,539 | object steering, the leader's movement |
| FE1 | `0x52D080..0x533BA0` | 45 | 24 | 7,239 | event script, first seven runs |
| FE2 | `0x5341C0..0x5372D8`, `0x56D240..0x5729F8`, `0x593960..0x594060` | 44 | 16 | 11,455 | event script's rest, draw layers, the sprite-draw prompt states |
| FO | `0x5738A0..0x57CD89` | 44 | 26 | 13,090 | field objects |
| FS | `0x57FF80..0x58CD40` | 52 | 37 | 14,676 | SHOP's remainder, the field-menu window `0x58C7A0` |

**631 functions, 14 groups, 2 harness groups.** The battle side is 308
in seven (the 300 of part 4 and the eight neighbours of section 2), the
field side 323 in seven. FC3 at 57 and FS at 52 are the
largest; FE2 and FO carry the most bytes (long window and object
functions), so they are the slow ones, as round ten's SC12 was.

## 4. The waves

**Wave one is the battle side**: EH alone in stage A (as BH was - one
agent on the harness files), BE1..BE7 in stage B behind it. It goes first
because its live route exists (section 5) and because EH's fold-backs are
owed anyway. **Wave two is the field side**: FH in stage A, FC1..FS in
stage B. FH can run in wave one's stage A alongside EH - different files -
so that wave two starts the moment wave one's tip is verified.

Each wave as round eleven's: a brief per group from
`analysis/round11_wave1_brief.md`, one Opus agent per group in a worktree
reset onto the wave's tip, headless self-tests only, `merge_group11.sh`'s
routine with `MOD=` the group's module, the group's shadow, `'*'` and
`ledger_check.py` in the detached `verify/` worktree, one merge at a time.
Every group's doc on `magic_s16.md`'s shape; the round doc
`takeover-queue-round12.md` when it runs.

## 5. The live check

The catalog's reach columns are zero for all 623: no recorded route enters
any of them, which is why they are left. Tonight changed the battle side.

**`tools/recipes/dragonTransform.txt`** (the owner, 2026-09-28: the
`adult_ryu` save, a random encounter, the Dragon command, a gene chosen,
the transformation, Dragon Breath, the win, the results) played under the
tracer's new reach switch (section 7) entered **58 of this round's
functions**, by group:

| Group | Entered live | At recipe frame |
|---|---|---|
| BE7 | `0x598A30`, `0x598BE0`, `0x598E90` (the gene screen; the shot `analysis/shots/recipe_dragon/gene.png`), `0x5982D0`, `0x598810`, `0x598750` (the result windows) | 977, 4,100..4,188 |
| BE6 | `0x4525B0` (the Dragon command), `0x4514A0`, `0x451670`, `0x4516F0`, `0x451750`, `0x4517A0`, `0x451C50`, `0x451C80`, `0x451DA0`, `0x451FE0`, `0x452080`, `0x453300`, `0x453560`, `0x453910`, `0x453EB0` | 976, 1,798, 2,366, 2,526, 3,163 |
| BE2 | `0x435AB0`, `0x435CF0`, `0x435EF0`, `0x435E10` (the action's begin) | 1,702, 1,983 |
| BE4 | `0x446110`, `0x4461B0`, `0x4463E0`, `0x446600`, `0x446810`, `0x4468B0`, `0x446990`, `0x4469D0`, `0x446A80`, `0x446F20`, `0x446F50`, `0x446F80`, `0x447F40`, `0x445600`, `0x44A910` | 1,678..4,192 |
| BE3 | `0x442420`, `0x442310`, `0x4376A0`, `0x4376F0` | 1,747, 1,769, 2,731 |
| BE5 | `0x44FDE0`, `0x44FB30`, `0x44FCE0` | 2,731, 2,788, 3,958 |
| BE1 | `0x4319B0`, `0x432170`, `0x431FE0` (the result screen) | 4,098..4,188 |
| FE2 | `0x532C10`, `0x532D10`, `0x532D50`, `0x5341E0`, `0x5343C0`, `0x534420` (the return to the field) | 4,195..4,227 |

The same run made 77 fuzz-only functions live for the first time (the
Accession effect, Dragon Breath, RestoreForm, WhiteFlag, Magic008,
Magic019), which is a live check on round nine's spell groups nobody had.
The list with callers is `analysis/calltrace/reach_dragon/reach_dragon_new.txt`.

**What the battle side still needs recorded**: the other commands the
cross offers (Defend, Escape, Examine, the auto-battle - `0x42EF50` is
`Cmd_AutoBattle_Begin`), a battle lost, an item's battle use with its
target prompt, a formation change; each a recipe on the `combat` save, ten
minutes of the owner's play. `battle_commands.txt` (NEW GAME's scripted
fight) and `combat.txt` cover what they cover, and every function they
reach is ours already.

**The field side has no live route at all.** The runs' data touches say
what to record: an event that gives zenny (`0x5307C0`: `Zenny_Add`,
`Msg_OpenSystem`), one that costs HP (`0x534C20`: `Char_LoseHp`), object
steering in a crowded town (`0x518B20..`), a jump (`Field_JumpStart` in
`0x525390..`), the shop's equip and sell screens' every branch
(`0x5836E0..`), a save point's prompt states (`0x593960..`). Each recipe
is played under the reach switch before its group is staged, so the
group's brief can say which of its functions the live check will cover.

## 6. What is different from rounds nine to eleven

- **No enumerating table**: the groups are address bands, not units. A
  function a band splits from its callers is taken by the band that holds
  its address (round ten's rule, "a group owns the functions whose
  address lies in its band"); the cut table is the authority.
- **The harnesses are widened, not built.** Stage A is a day's work per
  side, not BH's 56 minutes plus a doc.
- **The callers are ours.** Every one of these functions is called from
  code we own (or from a table ours indexes), so the rebinding pass that
  ended rounds ten and eleven is this round's *first* step per group: the
  brief lists the raw `0x...` call sites in our files for each function
  (`grep -rn` over `src/game`), and the group rebinds them as it lands.
- **Live coverage is measurable per recipe**, with the tracer's reach
  switch (section 7). Rounds nine to eleven could only say "fuzz-only".

## 7. Before the first cut

1. **Merge the tracer's reach switch** (`BOF3X_CALLTRACE_REACH=1`,
   `src/hook/calltrace.cpp` and `src/hook/detour.cpp`'s `IsEnabled`,
   on `phase-3/round12-plan` with this doc): without it a trace on an
   all-original side arms only unregistered entries - 1,282 of 7,706 at
   round eleven's tip - because `IsOwned` counts a function left original
   by `BOF3X_ORIGINAL` as owned. The switch arms those too. A reach run:

   ```
   python tools/input_run.py tools/recipes/<route>.txt --out analysis/shots/reach_<route> --no-front --lang en \
     --original "*,-LoadDatFile,-MsgBox_DrawChar,-Msg_SystemPtr,-ConfigText,-MenuVerbs,-TitleMenu_Widths,-Text_DrawString,-Text_DrawImmediate,-Game_Clock,-Gfx_BeginFrame" \
     --env BOF3X_CALLTRACE=<abs path of analysis/calltrace/entries_logic.txt> --env BOF3X_CALLTRACE_REACH=1 --minutes 6
   ```

   then `python tools/calltrace.py report build/bof3x.calltrace.tsv`. The
   counts of such a run are not a frame hash and are not comparable with
   a run made without the switch.
2. **Round eleven's debts** item 1 (the harness fold-backs) go to EH;
   items 2..6 can land before or alongside wave one, as round ten's
   cleanup did.
3. **Regenerate the cut at the round's base** (section 8) and reread the
   two split points; a function taken in between moves the counts, not
   the bands.
4. **The recipe saves**: `dragonTransform.txt` carries `# save adult_ryu`
   now; the routes of section 5 need theirs.
5. **The brief**: `analysis/round11_wave1_brief.md` with the harness API
   paragraph swapped for EH's / FH's, and a new paragraph for the
   rebinding step (section 6).

## 8. Regenerating the cut

```
python tools/remaining_catalog.py --tsv analysis/remaining_catalog.tsv
python - <<'EOF'
import csv, collections
rows = list(csv.DictReader(open('analysis/remaining_catalog.tsv'), delimiter='\t'))
fs = sorted((int(r['entry'], 16), r) for r in rows if r['part'] in ('3 Field modes', '4 Battle'))
G = [('BE1', 0x42D7A0, 0x433000), ('BE2', 0x433000, 0x437000), ('BE3', 0x437000, 0x444000),
     ('BE4', 0x444000, 0x44B000), ('BE5', 0x44B000, 0x451400), ('BE6', 0x451400, 0x460000),
     ('BE7', 0x597000, 0x5A0000), ('FC1', 0x461000, 0x46BBF0), ('FC2', 0x46BBF0, 0x470000),
     ('FC3', 0x517000, 0x527000), ('FE1', 0x52D000, 0x5341C0), ('FE2', 0x5341C0, 0x573000),
     ('FO', 0x573000, 0x57D000), ('FS', 0x57F000, 0x58D000)]
EXTRA = {0x4376A0: 'BE3', 0x4376F0: 'BE3', 0x441090: 'BE3', 0x446DE0: 'BE4', 0x446E00: 'BE4',
         0x446E20: 'BE4', 0x454A80: 'BE6', 0x455290: 'BE6'}   # round eleven's debts, section 2
fs += sorted((int(r['entry'], 16), r) for r in rows if int(r['entry'], 16) in EXTRA)
def grp(a):
    if a in EXTRA: return EXTRA[a]
    if 0x593960 <= a < 0x594100: return 'FE2'      # the sprite-draw prompt states
    if 0x4CEB40 <= a < 0x4CF500: return 'BE6'      # BMAGIC's four
    return next(g for g, lo, hi in G if lo <= a < hi)
out = collections.defaultdict(list)
for a, r in fs: out[grp(a)].append((a, r))
w = csv.writer(open('analysis/round12_cut.tsv', 'w', newline=''), delimiter='\t')
w.writerow(['group', 'entry', 'size', 'label', 'hidden', 'host', 'name'])
for g, _, _ in G:
    for a, r in sorted(out[g]):
        w.writerow([g, f'0x{a:06X}', r['size'], r['label'], r['hidden'], r['host'], r['name']])
    print(g, len(out[g]))
EOF
```

The catalog's `--symbols` defaults to the checkout's `symbols.toml`; run
it from the round's base commit.

## 9. Round thirteen: the area overlays' remainder

Round ten took "every area overlay of worlds 0..4" as its tool enumerated
them - the closures of the descriptors' call tables. The catalog at
`c4b0d32` still labels **629 functions** as area overlays: world 0 171,
world 1 112, world 2 199, world 3 47, world 4 100; 443 hidden; 109 KiB.
Their label source says what they are: 267 "neighbour" (between two area
functions, in no table), 114 "host" (inside an area function's extent),
the rest paired to PSX area code by address delta. They are the area code
the walk did not reach - state handlers behind bytes the tool did not
follow, and helpers only a neighbour calls. The plan for them is
[`takeover-queue-areas.md`](takeover-queue-areas.md)'s tool run the other
way: `tools/area_rows.py` over the catalog's rows, grouped by world and
area as round ten's waves were, about twelve groups of fifty, on
`area_harness` unchanged. The cut waits for round twelve's base, since
FC/FE take field core the area code calls; the count is the catalog's.

After that, the remainder is parts 2 and 7: 891 boot-resident functions
(mostly the `Effect_*` and `Field_*` tables' slots, enumerable from their
tables) and **1,349 unlabelled** starts that no table, host, pairing or
name reaches - those need a labelling pass (a reading of each address run's
touches, as this doc's section 1 does by hand) before they can be cut at
all.

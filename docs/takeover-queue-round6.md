# The sixth round's queue: what the shop route reaches

**Status:** IN PROGRESS (2026-09-23)

The first input-reached queue (HANDOFF "Pick up here" 0000 item 2). The owner
recorded a route from save 3 - the item shop (buy, sell), a found item, the
weapon shop with equipping, walking and running, the inn's rest and save menu
- as `tools/recipes/shop.txt` ([`input-script.md`](input-script.md) §5a); its
A/B, `analysis/validate_shop.sh`, was 35 of 35 identical with 583 ours. The
route played once all original under the all-calls tracer
(`analysis/calltrace/recipe_shop/`), less what the attract sequence reaches:

    python tools/attract_catalog.py analysis/calltrace/recipe_shop/bof3x.callcounts.tsv \
      --minus analysis/calltrace/hidden_b/bof3x.callcounts.tsv,analysis/calltrace/all_a/bof3x.callcounts.tsv,analysis/calltrace/all_b/bof3x.callcounts.tsv \
      --out analysis/shop_catalog.md

**181 reached and not ours, 164 in scope.** Left out: the CRT (15), the task
system (2), and `MsgBox_SystemChoice` `0x498A30` (D22, no faithful C++ for
ids `0x90` and up). Counts and sizes are the catalogue's; `pe_funcs.py` sizes
run on through pointer-reached neighbours (`0x539AC0` is listed at 16 bytes
and calls eleven things - read the extent), and the catalogue's "callers"
column attributes a call to the nearest *known* start below it, so a caller
there can be a neighbour of the real one. Expect a case label or two posing
as a function, and a pointer-reached function or two missing, per group
(HANDOFF 0000 item 4).

## The rule for calls across groups

Many calls cross groups - the stats helpers (W) are called from Y, X and V.
**Each group writes `symbols.toml` entries for its own functions only.** A
call into another group's function goes through a raw address in your own
`_callees.h`, as `item_use_callees.h` does, and in the fuzz is re-aimed at a
recording stand-in like any other callee. Never bind (`impl`) or rename an
address another group owns; if you learn something about one, say so in your
report and doc. Addresses already ours are fine to call through the header.

## The groups

Each: a new `src/game/<file>.cpp` (+ `.h`, `_callees.h`, `_fuzz.cpp`), its own
shadow name, its Inject call at the end of `inject_all.cpp`, a doc
`docs/<file>.md`.

| Group | File | Functions |
|---|---|---|
| V1 - the event script's shop and scene ops | `event_ops.cpp` | `EventOp_5x` `0x57B310`, `EventOp_Dx` `0x57B500`, `EventOp_9x` `0x57B530`; `0x52DB90` `0x52E060` `0x52E140` `0x52E160` `0x530030` `0x5301F0` `0x5302C0` `0x530380` `0x5303E0` `0x530430` `0x530480` `0x530600` `0x530800` `0x530860` `0x5308D0` `0x530920` `0x530A50` `0x530BF0` `0x530C90` `0x530CC0` `0x531660` `0x531950` `0x531DF0`; `0x539AC0`, `0x56D700`, `0x56D750`, `0x56E050`, `0x56E440`, `0x56E4E0` |
| V2 - the event script's object ops | `event_objs.cpp` | `0x534590` `0x5345E0` `0x534610` `0x534710` `0x534790` `0x534800` `0x534920` `0x534990` `0x534A00` `0x534F10` `0x5350C0` `0x535120` `0x535150` `0x535240` `0x535270` `0x535310` `0x535390` `0x5353E0` `0x535C50` `0x535CA0` `0x535F50` `0x536670` |
| W - stats and inventory | `char_stats.cpp` | `0x5903F0`, `Menu_DrawHand` `0x5905D0`, `Char_RecalcStats` `0x590660`, `0x590800`, `0x590960`, `Inventory_Add` `0x590BB0`, `0x590F30`, `0x590FC0`, `0x591190`, `0x591490`, `0x591680`, `0x591720`, `0x5917A0`, `0x5918E0`, `0x591940`, `0x5919B0`, `0x591A80`, `0x591C20` |
| X - save, load, the inn, the stream | `save_menu.cpp` | `0x454770`, `Save_ReadFile` `0x454820`, `Save_ListFiles` `0x4548B0`, `0x4549F0`; `0x580630` `0x5808E0` `0x580970` `0x583020` `0x5830D0` `0x583100` `0x583140` `0x583210`; `0x588880` `0x5888D0` `0x588AC0` `0x588BA0` `0x588C00` `0x588C90` `0x588D20`, `Save_ReadSummaries` `0x588DC0`; `Sound_LoadStream` `0x587910`, `Sound_StreamDone` `0x587A00`, `0x5A7020` |
| Y - the menu and shop windows | `menu_windows.cpp` | `Msg_OpenSystem` `0x497710`, `0x498D20`, `0x516F60`, `0x517090`, `0x596020`; `0x573A80` `0x573CE0` `0x573E50` `0x573F30` `0x574530` `0x574610`, `Menu_YesNo` `0x5747D0`, `Menu_DrawButtonRow` `0x574890`, `0x5749F0` `0x574A60` `0x574AB0` `0x575430` `0x575690` `0x575830` `0x5759C0` `0x5762D0`; `0x57CF60` `0x57D360` `0x57D420` `0x57D760`, `Text_CharCount` `0x57D800`, `0x57D830`, `Menu_DrawPiece` `0x57D860`, `0x57D910` `0x57D9A0` `0x57DBF0` `0x57DD10` `0x57DF00`; `0x59B580` `0x59B820` `0x59BBC0`; `0x5A7720` |
| Z - the party members' sprites | `member_sprites.cpp` | `Field_MemberFrame` `0x51AC50`, `0x51AD60` `0x51B050` `0x51B430` `0x51B5D0` `0x51B9D0` `0x51BDA0`; `0x526DB0` `0x527470` `0x528070` `0x5280F0` `0x528120` `0x5282C0` `0x528370` `0x528730` `0x528770` |
| M - map patches, move tests, three draw handlers, four setters | `field_misc.cpp` | `AreaMap_BlockedNarrow` `0x5183C0`, `0x570AB0`, `AreaMap_ApplyPatch` `0x571110`, `0x5718F0`, `0x5725C0`, `MoveCmd_TestFB` `0x572650`, `MoveCmd_TestFC` `0x572790`, `0x572ED0`; the Direct3D handlers `0x5A0AB0` `0x5A1290` `0x5A1A00` (on `d3d_fuzz.*`, as group R's); the PSX setters `0x5A75B0` `0x5A7610` `0x5A7670` `0x5A7810` |

## The live check

After the merge, with round five's group U: the shop A/B
(`analysis/validate_shop.sh`), which reaches every function here, then the
attract batch (oracle, memory dump, frame hash with an original-vs-original
pair) - every owned function in `analysis/calltrace/entries_logic.txt` first.
Anything the shop route does not reach is fuzz only; each doc says what.

# The eighth round's queue: what the recorded routes reach that the traces could not see

**Status:** IN PROGRESS (2026-09-25) - all 22 groups merged, 1,465 ours, the batched live check passed ("Result"); the owner's eye owed

The third input-reached queue, after the shop's ([`takeover-queue-round6.md`](takeover-queue-round6.md)),
the world map's ([`world-map.md`](world-map.md) §4) and the combat route's
([`takeover-queue-round7.md`](takeover-queue-round7.md)). It comes from no new
route. Rounds six and seven closed their queues at zero with every function
the all-calls tracer could arm; what they could not arm was every function
reached only through a pointer, which `pe_funcs.py` folds into the function
before it ([`attract-remaining.md`](attract-remaining.md) §3). On 2026-09-25
the three routes were played once more under a first-call trace of those
7,294 starts ([`remaining-catalog.md`](remaining-catalog.md) §4), and **442
functions the routes enter are still Capcom's** - 407 of them
pointer-reached, 35 recorded starts the earlier rounds left. Every one has a
live check the day it is taken: the route that reaches it.

Two waves, by route: **A** is the combat route's 236 (groups `CA`..`CM`),
**B** is the shop's and the world map's 206 (`DA`..`DI`). Group sizes are
15..31 functions, 0.6..6 KiB each, the sizes round seven's agents got through
in a night. The per-function rows are `analysis/remaining_catalog.tsv`
(never committed); the tables here are its output for these 442, unedited.

## What is different about this round

1. **Most of the queue is pointer-reached.** A function here was never a
   `call rel32` target. It is found from a table in `.data` (the "Pointer
   in" column: the battle phase table `0x64AE28`.., the enemy AI op tables
   `0x64B084`.., the battle object states `0x64DFE0`, the shop's two tables
   `0x663E44`.. and `0x664118`.., the menu lists `0x66AF94`..), from a
   pointer in `.text` (a `mov [esp+k], imm32` call table built on the
   stack, [`attract-remaining.md`](attract-remaining.md) §3), or as a task
   start. **Name the table** in `symbols.toml` (a `[[data]]` entry with
   `ctype = "unsigned long"` and its count) as part of the group: that is
   what turns "the handler at slot 7" into a name, and it is what the
   catalogue's `table` rule keys on next time.
2. **"Reached as: jumped to" is a third of the queue** (183 of 442; groups
   `CA`, `CC`, `CD`, `CE`, `CH`, `CI`, `CJ`, `CL`, `CM` almost entirely). The first
   return address does not follow a `call`: the function was entered by a
   tail `jmp`, usually from a 16-byte stub that dispatches on a state byte -
   `0x4302B0` is `xor eax, eax; mov al, [0x904AA1]; jmp [eax*4 + 0x64AF2C]`.
   The five-byte detour works on a jumped-to entry exactly as on a called
   one (execution arrives at the first byte either way), but the arguments
   and the return are **the stub's caller's**, not the stub's: read the
   caller's frame, not the stub's, for the prototype. Take the stub and its
   targets as one unit; the stub itself is a one-line reimplementation.
   Where a "jumped to" entry turns out to be a switch case inside a
   function rather than a function (round three found these in
   `MsgBox_Step`), say so in the doc and leave it: it goes when its host
   does.
3. **The extents are `pe_hidden.py`'s** - to the next start, an upper bound.
   A function here may run into the next one's padding or be shorter than
   listed; the disassembly decides. Add every function you own to
   `analysis/calltrace/entries_logic.txt` with the extent you read, or the
   frame hash breaks (HANDOFF, Traps).
4. **The A/B original side needed a fix before this round could be checked
   at all**: the combat route trapped on the English EX glyph on every
   `*,KEEP` run since DIV-0052 (DIV-0016, amended 2026-09-25). The fix is
   in `LoadDatFile`; the trace this queue is built from is with it. Rebuild
   before A/B'ing.

## The rule for calls across groups

Unchanged from rounds six and seven: **each group writes `symbols.toml`
entries for its own functions and its own tables only.** A call into another
group's function goes through a raw address in your own `_callees.h`, re-aimed
at a recording stand-in in the fuzz. Never bind (`impl`) or rename an address
another group owns; say what you learn about it in your report and doc. Every
address already ours is fine to call through its header. A table whose slots
span two groups (the window-kind handlers `0x597A80`.. are `CM`'s, the
window task's battle states `0x596FA0`.. are `CL`'s, both under
`Window_Task`'s host) belongs to the group named here; the other group calls
through it.

## The groups

Each: a new `src/game/<file>.cpp` (+ `.h`, `_callees.h`, `_fuzz.cpp`), its own
shadow name (the file's stem), its Inject call at the end of `inject_all.cpp`,
a doc `docs/<file>.md`. The model to copy is round seven's
[`battle_flow.md`](battle_flow.md) and its sources. The "Bytes" column counts
extents; a "(n calls)" beside a recorded start is the all-calls count from
its route's trace, for the hot ones.

| Group | File | Functions | Bytes | Jumped to | Routes | Addresses |
|---|---|--:|--:|--:|---|---|
| CA - the battle phase table, first half | `battle_phases.cpp` | 18 | 3,028 | 17 | combat 18 | `0x42E370` `0x42E470` `0x42E4A0` `0x42E730` `0x42E770` `0x42E8C0` `0x42E8D0` `0x42E930` `0x42E990` `0x42E9A0` `0x42E9E0` `0x42EAD0` `0x42EED0` `0x42EEF0` `0x42F070` `0x42F080` `0x42F130` `0x42F1D0` |
| CB - the action phases | `battle_actions.cpp` | 23 | 3,262 | 1 | combat 23 | `0x42F220` `0x42F250` `0x42F260` `0x42F4C0` `0x42F500` `0x42F510` `0x42F5B0` `0x42F670` `0x42F680` `0x42F880` `0x42FAB0` `0x42FAF0` `0x42FB00` `0x42FBD0` `0x42FC10` `0x42FC50` `0x42FC60` `0x42FD20` `0x42FD90` `0x42FDD0` `0x42FDE0` `0x42FF70` `0x430010` |
| CC - the per-turn steps | `battle_turn_steps.cpp` | 20 | 2,237 | 20 | combat 20 | `0x4302B0` `0x4302C0` `0x4302D0` `0x430500` `0x430510` `0x431090` `0x4311E0` `0x4311F0` `0x431200` `0x431220` `0x431320` `0x4314B0` `0x4314C0` `0x431520` `0x431760` `0x431770` `0x4317B0` `0x4317F0` `0x431910` `0x431920` |
| CD - the battle result | `battle_result.cpp` | 17 | 2,007 | 17 | combat 17 | `0x431940` `0x431A20` `0x431A90` `0x431AB0` `0x431B30` `0x431B60` `0x431B80` `0x431D50` `0x431D70` `0x432050` `0x432070` `0x4320F0` `0x598570` `0x5985A0` `0x5986C0` `0x5986F0` `0x598700` |
| CE - the battle effect tasks | `battle_fx_tasks.cpp` | 18 | 3,319 | 16 | combat 18 | `0x432B70` `0x432DB0` `0x432DE0` `0x432E50` `0x432EA0` `0x433190` `0x4331D0` `0x433290` `0x4332E0` `0x433460` `0x4334C0` `0x4337F0` `0x433810` `0x4352A0` `0x435350` `0x437720` `0x437750` `0x4378B0` |
| CF - the enemy AI script ops | `enemy_ai_ops.cpp` | 22 | 2,468 | 0 | combat 22 | `0x4360F0` `0x436110` `0x436170` `0x436190` `0x4361B0` `0x436210` `0x4363B0` `0x4363E0` `0x436510` `0x436560` `0x4366E0` `0x436700` `0x436720` `0x436740` `0x436A00` `0x436A20` `0x436D90` `0x436DB0` `0x436E00` `0x436E40` `0x436EC0` `0x437420` |
| CG - the battle object states | `battle_obj_states.cpp` | 22 | 2,356 | 0 | combat 22 | `0x441200` `0x4414E0` `0x441550` `0x441570` `0x4417A0` `0x4417F0` `0x441860` `0x441890` `0x441930` `0x441950` `0x4419D0` `0x4429E0` `0x442A00` `0x442B20` `0x442B80` `0x442BA0` `0x442BC0` `0x442BD0` `0x442C50` `0x442D60` `0x442D80` `0x442DB0` |
| CH - actor copies and the battle item menu | `battle_actor_copies.cpp` | 16 | 2,647 | 16 | combat 16 | `0x447110` `0x447120` `0x447140` `0x447160` `0x447190` `0x447390` `0x447430` `0x447440` `0x447460` `0x4474B0` `0x4478B0` `0x447940` `0x447960` `0x4479E0` `0x447B10` `0x447CC0` |
| CI - the battle menu states | `battle_menu_states.cpp` | 15 | 2,816 | 15 | combat 15 | `0x447FD0` `0x447FE0` `0x448020` `0x4480E0` `0x448180` `0x448190` `0x4481B0` `0x4481E0` `0x448210` `0x448630` `0x4486C0` `0x4486E0` `0x448780` `0x4488C0` `0x448A70` |
| CJ - magic effects the fight casts | `magic_fx_reached.cpp` | 25 | 2,837 | 21 | combat 25 | `0x4AD130` `0x4AD160` `0x4AD200` `0x4AD270` `0x4AD2B0` `0x4B54B0` `0x4B54F0` `0x4B5770` `0x4B57C0` `0x4B5810` `0x4B5830` `0x4B5880` `0x4B58C0` `0x4B8D70` `0x4B8E00` `0x4B8F50` `0x4B9000` `0x4C4FC0` `0x4C5020` `0x4C5110` `0x4F52D0` `0x4FAFF0` `0x4FB010` `0x4FB050` `0x4FB070` |
| CK - banner, effect result, sparkle and the odd ones | `battle_odds.cpp` | 10 | 592 | 8 | combat 10 | `0x44A740` `0x44A7B0` `0x44C140` `0x44C990` `0x4AEE90` `0x4B1E70` `0x4B1ED0` `0x4ED5C0` `0x4EE8A0` `0x4F7350` |
| CL - the battle windows under the window task | `battle_win_states.cpp` | 19 | 2,612 | 19 | combat 19 | `0x596FA0` `0x597000` `0x597030` `0x597070` `0x597090` `0x5970C0` `0x597160` `0x5971B0` `0x5971E0` `0x597200` `0x597230` `0x597320` `0x597400` `0x5974C0` `0x597510` `0x5975D0` `0x5976D0` `0x597850` `0x5978B0` |
| CM - the window-kind handlers | `window_kinds.cpp` | 11 | 1,104 | 11 | combat 11 | `0x597A80` `0x597BD0` `0x597C10` `0x597C70` `0x597CF0` `0x597D50` `0x597D90` `0x597DC0` `0x597DF0` `0x597E60` `0x597F60` |
| DA - the world map area handlers and the effect kinds it runs | `worldmap_area.cpp` | 31 | 3,703 | 0 | combat 6, worldmap 25 | `0x4037B0` `0x403CE0` `0x403D40` `0x403D70` `0x403E00` `0x403EE0` `0x404030` `0x404080` `0x4040E0` `0x404130` `0x404150` `0x404180` `0x404230` `0x404250` `0x4042C0` `0x404330` `0x4048E0` `0x411310` `0x414BB0` `0x419110` `0x462A90` `0x462AE0` `0x462B40` `0x462B80` `0x469BB0` `0x469BD0` `0x469CB0` `0x469CD0` `0x469DB0` `0x46D830` `0x46D850` |
| DB - top-level modes, the system choice and the field core | `mode_states.cpp` | 21 | 3,630 | 1 | worldmap 14, shop 14, combat 9 | `0x496250` `0x496290` `0x4962A0` `0x496390` `0x496830` `0x496A00` `0x496AD0` `0x497C30` `0x498A30` `0x516E70` `0x517290` `0x5172F0` `0x517300` `0x525370` `0x5258B0` `0x5258D0` `0x525920` `0x539AD0` `0x539B20` `0x53D830` `0x5960D0` |
| DC - the event script: the leader, the steps, the placements | `event_leader.cpp` | 27 | 5,517 | 0 | shop 7, worldmap 20, combat 6 | `0x52E110` `0x52E580` `0x52E9D0` `0x52E9F0` `0x52EBA0` `0x52ED80` `0x52EE70` `0x52F1B0` `0x52F350` `0x52F390` `0x52F3B0` `0x52F3D0` `0x52F400` `0x52F420` `0x52F490` `0x52F5E0` `0x52F600` `0x52F760` `0x52F8A0` `0x52FB60` `0x5317F0` `0x531920` `0x531AF0` `0x533780` `0x535610` `0x535640` `0x535730` |
| DD - map layers and field objects | `map_field_objects.cpp` | 16 | 4,236 | 0 | shop 7, worldmap 16, combat 3 | `0x570210` `0x570530` `0x570A00` `0x571090` `0x5710D0` `0x5712E0` `0x571880` `0x571A30` `0x571A70` `0x573560` `0x5744B0` `0x5746C0` `0x579CF0` `0x579F00` `0x57A3A0` `0x57C7A0` |
| DE - field-side hidden functions: direction, party sets, inventory ops | `field_hidden.cpp` | 15 | 2,366 | 0 | worldmap 12, combat 6 | `0x51BA60` `0x51BBD0` `0x51DA30` `0x51E910` `0x51E930` `0x51EAF0` `0x51EBD0` `0x51F1B0` `0x589110` `0x589160` `0x5891C0` `0x5898D0` `0x591F30` `0x592570` `0x5925A0` |
| DF - the shop overlay, first table | `shop_states.cpp` | 25 | 2,800 | 0 | shop 25 | `0x57F500` `0x57F520` `0x57F5D0` `0x57F650` `0x57F660` `0x57F6E0` `0x57F6F0` `0x57F760` `0x57F810` `0x57F8A0` `0x57FAB0` `0x57FB60` `0x57FB70` `0x57FBE0` `0x57FC40` `0x57FC70` `0x57FCB0` `0x57FD20` `0x57FD80` `0x57FDD0` `0x57FDE0` `0x57FE00` `0x57FE70` `0x580150` `0x5801A0` |
| DG - the shop overlay, second table, and the save summaries | `shop_states2.cpp` | 26 | 6,012 | 0 | shop 26, worldmap 2, combat 2 | `0x5818B0` `0x5818C0` `0x5818D0` `0x581970` `0x5819B0` `0x5819C0` `0x581AE0` `0x581AF0` `0x581BE0` `0x581ED0` `0x582090` `0x5821E0` `0x5822C0` `0x5824D0` `0x5826A0` `0x582770` `0x582780` `0x5827F0` `0x582B60` `0x582D00` `0x582E80` `0x584F70` `0x588E70` `0x588EB0` `0x5916B0` `0x5917D0` |
| DH - the field menu and the list draws | `menu_lists.cpp` | 20 | 2,829 | 6 | worldmap 14, shop 5, combat 2 | `0x589970` `0x589990` `0x589B60` `0x589B70` `0x589E50` `0x589E60` `0x589FE0` `0x599B50` `0x599B70` `0x599B90` `0x599D50` `0x599DC0` `0x599E50` `0x599FA0` `0x59A3A0` `0x59A3D0` `0x59A580` `0x59A5B0` `0x59A5E0` `0x59A680` |
| DI - the shop and menu draw helpers | `menu_draw_helpers.cpp` | 25 | 1,290 | 15 | shop 20, combat 6 | `0x59B220` `0x59B240` `0x59B310` `0x59B350` `0x59B390` `0x59B3C0` `0x59B3F0` `0x59B440` `0x59B470` `0x59B4A0` `0x59B4F0` `0x59B530` `0x59B560` `0x59B7B0` `0x59B7E0` `0x59B810` `0x59BB60` `0x59BB80` `0x59BBB0` `0x59CB00` `0x59CB20` `0x59CB40` `0x59CB60` `0x59CBC0` `0x59CBE0` |
| | **wave A (C\*)** | **236** | **31,285** | | |
| | **wave B (D\*)** | **206** | **32,383** | | |

### CA - the battle phase table, first half

`src/game/battle_phases.cpp`, 18 functions, 3,028 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x42E370` | 132 | `0x42E2F0` | `.data 0x656A98` | indirect call | Field_Task | combat | Unlabelled |  |
| `0x42E470` | 48 | Battle_PhaseDispatch (ours) | `.text 0x42E40C` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x42E4A0` | 656 | Battle_PhaseDispatch (ours) | `.text 0x42E479` | jumped to | `0x42E470` | combat | Battle engine (BATTLE.EMI) | Battle_Init |
| `0x42E730` | 64 | Battle_PhaseDispatch (ours) | `.text 0x42E486` | jumped to | `0x42E470` | combat | Boot: battle |  |
| `0x42E770` | 336 | Battle_PhaseDispatch (ours) | `.text 0x42E739` | jumped to | `0x42E730` | combat | Battle engine (BATTLE.EMI) | psx 801D1820 |
| `0x42E8C0` | 16 | Battle_PhaseDispatch (ours) | `.text 0x42E746` | jumped to | `0x42E730` | combat | Battle engine (BATTLE.EMI) | psx 801D1AAC |
| `0x42E8D0` | 96 | Battle_PhaseDispatch (ours) | `.text 0x42E74E` | jumped to | `0x42E730` | combat | Battle engine (BATTLE.EMI) | psx 801D1AE0 |
| `0x42E930` | 96 | Battle_PhaseDispatch (ours) | `.text 0x42E756` | jumped to | `0x42E730` | combat | Battle engine (BATTLE.EMI) | psx 801D1B8C |
| `0x42E990` | 16 | Battle_PhaseDispatch (ours) | `.text 0x42E41B` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x42E9A0` | 64 | Battle_PhaseDispatch (ours) | `.data 0x64AE28` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | Battle_RoundStart |
| `0x42E9E0` | 240 | Battle_PhaseDispatch (ours) | `.data 0x64AE2C` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D1D08 |
| `0x42EAD0` | 704 | Battle_PhaseDispatch (ours) | `.data 0x64AE30` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D1E84 |
| `0x42EED0` | 32 | Battle_PhaseDispatch (ours) | `.data 0x64AE38` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | BattleMenu_ConfirmDispatch |
| `0x42EEF0` | 96 | Battle_PhaseDispatch (ours) | `.data 0x64AE64` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D2514 |
| `0x42F070` | 16 | Battle_PhaseDispatch (ours) | `.text 0x42E423` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x42F080` | 176 | Battle_PhaseDispatch (ours) | `.data 0x64AE74` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | Battle_CommitRound |
| `0x42F130` | 160 | Battle_PhaseDispatch (ours) | `.data 0x64AE78` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D28E0 |
| `0x42F1D0` | 80 | Battle_PhaseDispatch (ours) | `.data 0x64AE7C` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D2A1C |

### CB - the action phases

`src/game/battle_actions.cpp`, 23 functions, 3,262 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x42F220` | 48 | Battle_PhaseDispatch (ours) | `.text 0x42E42B` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D2AA0 |
| `0x42F250` | 16 | Battle_PhaseDispatch (ours) | `.data 0x64AE80` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D2B14 |
| `0x42F260` | 608 | Battle_PhaseDispatch (ours) | `.data 0x64AE94` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | Battle_BeginAction |
| `0x42F4C0` | 64 | Battle_PhaseDispatch (ours) | `.data 0x64AE98` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D2FB8 |
| `0x42F500` | 16 | Battle_PhaseDispatch (ours) | `.data 0x64AE84` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D300C |
| `0x42F510` | 160 | Battle_PhaseDispatch (ours) | `.data 0x64AE9C`, `.data 0x64AEA4` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3048 |
| `0x42F5B0` | 48 | Battle_PhaseDispatch (ours) | `.data 0x64AEA0` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3160 |
| `0x42F670` | 16 | Battle_PhaseDispatch (ours) | `.data 0x64AEAC` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D32C0 |
| `0x42F680` | 512 | Battle_PhaseDispatch (ours) | `.data 0x64AEBC` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D32FC |
| `0x42F880` | 329 | Battle_PhaseDispatch (ours) | `.data 0x64AEC0` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D360C |
| `0x42FAB0` | 64 | `0x42F9D0` | `.data 0x64AEC4` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D399C |
| `0x42FAF0` | 16 | `0x42F9D0` | `.data 0x64AEB0` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3A08 |
| `0x42FB00` | 208 | `0x42F9D0` | `.data 0x64AF08` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3A44 |
| `0x42FBD0` | 64 | `0x42F9D0` | `.data 0x64AF0C` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3B94 |
| `0x42FC10` | 64 | `0x42F9D0` | `.data 0x64AF10` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3BFC |
| `0x42FC50` | 16 | `0x42F9D0` | `.data 0x64AE88` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3C68 |
| `0x42FC60` | 192 | `0x42F9D0` | `.data 0x64AF14` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3CA4 |
| `0x42FD20` | 112 | `0x42F9D0` | `.data 0x64AF18` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3E60 |
| `0x42FD90` | 64 | `0x42F9D0` | `.data 0x64AF1C` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3F4C |
| `0x42FDD0` | 16 | `0x42F9D0` | `.data 0x64AE8C` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D3FCC |
| `0x42FDE0` | 64 | `0x42F9D0` | `.data 0x64AF20` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D4008 |
| `0x42FF70` | 160 | `0x42F9D0` | `.data 0x64AF28` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D4284 |
| `0x430010` | 405 | `0x42F9D0` | `.data 0x64AE90` | indirect call | `0x42F220` | combat | Battle engine (BATTLE.EMI) | psx 801D43BC |

### CC - the per-turn steps

`src/game/battle_turn_steps.cpp`, 20 functions, 2,237 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x4302B0` | 16 | Battle_ClearActingFlags (ours) | `.text 0x42E433` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x4302C0` | 16 | Battle_ClearActingFlags (ours) | `.data 0x64AF2C` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x4302D0` | 244 | Battle_ClearActingFlags (ours) | `.data 0x64AF38` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D4B08 |
| `0x430500` | 16 | Battle_TickCounters (ours) | `.data 0x64AF3C` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x430510` | 297 | Battle_TickCounters (ours) | `.data 0x64AF30` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x431090` | 336 | Battle_ActorSkipped (ours) | `.data 0x64AF34`, `.data 0x64AF40` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D653C |
| `0x4311E0` | 16 | Battle_ActorSkipped (ours) | `.text 0x42E43B` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x4311F0` | 16 | Battle_ActorSkipped (ours) | `.data 0x64AF44` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x431200` | 32 | Battle_ActorSkipped (ours) | `.data 0x64AF58`, `.data 0x64AF7C` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x431220` | 256 | Battle_ActorSkipped (ours) | `.data 0x64AF5C`, `.data 0x64AF80` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x431320` | 400 | Battle_ActorSkipped (ours) | `.data 0x64AF60` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D69E4 |
| `0x4314B0` | 16 | Battle_ActorSkipped (ours) | `.data 0x64AF48` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x4314C0` | 96 | Battle_ActorSkipped (ours) | `.data 0x64AF64` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x431520` | 32 | Battle_ActorSkipped (ours) | `.data 0x64AF68` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x431760` | 16 | Battle_ActorSkipped (ours) | `.data 0x64AF54` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x431770` | 64 | Battle_ActorSkipped (ours) | `.data 0x64AF90` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | Battle_WriteBackParty |
| `0x4317B0` | 64 | Battle_ActorSkipped (ours) | `.data 0x64AF94` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x4317F0` | 256 | Battle_ActorSkipped (ours) | `.data 0x64AF98` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) | psx 801D7320 |
| `0x431910` | 16 | Battle_ActorSkipped (ours) | `.data 0x64AF6C` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x431920` | 32 | Battle_ActorSkipped (ours) | `.data 0x64AFA0` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |

### CD - the battle result

`src/game/battle_result.cpp`, 17 functions, 2,007 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x431940` | 110 | Battle_ActorSkipped (ours) | `.data 0x64AFAC` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EECD4 |
| `0x431A20` | 112 | `0x4319B0` | `.data 0x64AFB0` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EEE50 |
| `0x431A90` | 32 | `0x4319B0` | `.data 0x64AFB4` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EEEE4 |
| `0x431AB0` | 128 | `0x4319B0` | `.data 0x64AFB8` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EEF18 |
| `0x431B30` | 48 | `0x4319B0` | `.data 0x64AFBC` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EEFD0 |
| `0x431B60` | 32 | `0x4319B0` | `.data 0x64AFA4` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EF01C |
| `0x431B80` | 144 | `0x4319B0` | `.data 0x64AFC0` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EF058 |
| `0x431D50` | 32 | `0x4319B0` | `.data 0x64AFA8` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EF310 |
| `0x431D70` | 619 | `0x4319B0` | `.data 0x64AFC8` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EF34C |
| `0x432050` | 32 | `0x431FE0` | `.data 0x64AFCC` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EF780 |
| `0x432070` | 128 | `0x431FE0` | `.data 0x64AFD0` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | psx 801EF7B8 |
| `0x4320F0` | 116 | `0x431FE0` | `.data 0x64AFD4` | jumped to | Battle_PhaseDispatch | combat | Battle result (BATL_END.EMI) | Card_CopyIconFrame1 |
| `0x598570` | 48 | `0x5982D0` | `.text 0x597F8A` | jumped to | `0x597F60` | combat | Battle result (BATL_END.EMI) | psx 801F065C |
| `0x5985A0` | 288 | `0x5982D0` | `.text 0x598587` | jumped to | `0x598570` | combat | Battle result (BATL_END.EMI) | psx 801F06D4 |
| `0x5986C0` | 48 | `0x5982D0` | `.text 0x597F92` | jumped to | `0x597F60` | combat | Battle result (BATL_END.EMI) |  |
| `0x5986F0` | 16 | `0x5982D0` | `.text 0x59857F`, `.text 0x5986CF` | jumped to | `0x598570` | combat | Battle result (BATL_END.EMI) |  |
| `0x598700` | 74 | `0x5982D0` | `.text 0x5986D7` | jumped to | `0x5986C0` | combat | Battle result (BATL_END.EMI) |  |

### CE - the battle effect tasks

`src/game/battle_fx_tasks.cpp`, 18 functions, 3,319 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x432B70` | 576 | `0x432A30` | `.text 0x4352B7` | jumped to | `0x4352A0` | combat | Battle engine (BATTLE.EMI) | psx 801E5BE4 |
| `0x432DB0` | 48 | `0x432A30` | `.text 0x432B87` | jumped to | `0x432B70` | combat | Battle overlay BATL_OVR.EMI |  |
| `0x432DE0` | 112 | `0x432A30` | `.text 0x432B92` | jumped to | `0x432B70` | combat | Battle overlay BATL_OVR.EMI |  |
| `0x432E50` | 80 | `0x432A30` | `.text 0x432B9A` | jumped to | `0x432B70` | combat | Battle overlay BATL_OVR.EMI |  |
| `0x432EA0` | 108 | `0x432A30` | `.text 0x432BA2` | jumped to | `0x432B70` | combat | Battle overlay BATL_OVR.EMI |  |
| `0x433190` | 64 | BattleFx_RollingDigits (ours) | `.text 0x4352CA` | jumped to | `0x4352A0` | combat | Boot: battle |  |
| `0x4331D0` | 192 | BattleFx_RollingDigits (ours) | `.text 0x4331A9` | jumped to | `0x433190` | combat | Boot: battle |  |
| `0x433290` | 32 | BattleFx_RollingDigits (ours) | `.text 0x4331B4` | jumped to | `0x433190` | combat | Boot: battle |  |
| `0x4332E0` | 32 | BattleFx_RollingDigits (ours) | `.text 0x4332BF`, `.data 0x65C3A0` | jumped to | `0x435350` | combat | Boot: battle |  |
| `0x433460` | 96 | BattleFx_RollingDigits (ours) | `.text 0x4352E2` | jumped to | `0x4352A0` | combat | Boot: battle |  |
| `0x4334C0` | 144 | BattleFx_RollingDigits (ours) | `.text 0x43346C` | jumped to | `0x433460` | combat | Boot: battle |  |
| `0x4337F0` | 32 | BattleFx_RollingDigits (ours) | `.text 0x4352EA` | jumped to | `0x4352A0` | combat | Boot: battle |  |
| `0x433810` | 352 | BattleFx_RollingDigits (ours) | `.text 0x4337FD` | jumped to | `0x4337F0` | combat | Battle engine (BATTLE.EMI) | psx 800A5360 |
| `0x4352A0` | 176 | BattleTask_ClearAll (ours) | `.text 0x435118` | jumped to | BattleTask_RunAll | combat | Boot: battle |  |
| `0x435350` | 1152 | BattleTask_ClearAll (ours) | `.text 0x435120` | jumped to | BattleTask_RunAll | combat | Boot: battle |  |
| `0x437720` | 48 | `0x4376F0` | `.text 0x436127` | indirect call | `0x431320` | combat | Unlabelled |  |
| `0x437750` | 43 | `0x4376F0` | `.text 0x436131` | indirect call | `0x4317B0` | combat | Unlabelled |  |
| `0x4378B0` | 32 | NewGame_InitCharacters | `.text 0x435128` | jumped to | BattleTask_RunAll | combat | Boot: top-level modes and tasks |  |

### CF - the enemy AI script ops

`src/game/enemy_ai_ops.cpp`, 22 functions, 2,468 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x4360F0` | 32 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B084` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3140 |
| `0x436110` | 96 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1A0` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3184 |
| `0x436170` | 32 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1A4` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3224 |
| `0x436190` | 32 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1D4` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3268 |
| `0x4361B0` | 96 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1DC` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E32AC |
| `0x436210` | 96 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1E0` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3310 |
| `0x4363B0` | 48 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1A8`, `.data 0x64C7B8`, `.data 0x64C818` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3550 |
| `0x4363E0` | 304 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1AC`, `.data 0x64C7BC`, `.data 0x64C81C` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E35B0 |
| `0x436510` | 80 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1EC` | indirect call | `0x4363E0` | combat | Battle engine (BATTLE.EMI) | psx 801E37B4 |
| `0x436560` | 112 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1F0` | indirect call | `0x4363E0` | combat | Battle engine (BATTLE.EMI) | psx 801E3838 |
| `0x4366E0` | 32 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1B8`, `.data 0x64C8A8`, `.data 0x64C940` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3AE0 |
| `0x436700` | 32 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B1FC`, `.data 0x64B204`, `.data 0x64C7E0` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3B24 |
| `0x436720` | 32 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B200`, `.data 0x64C7E4`, `.data 0x64C844` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E3B48 |
| `0x436740` | 704 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B214` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | Battle_ResolveAction_Enemy |
| `0x436A00` | 32 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B218`, `.data 0x64B248` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E406C |
| `0x436A20` | 300 | BattleEnemy_ScriptTickOnce (ours) | `.data 0x64B21C`, `.data 0x64B230` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E40B4 |
| `0x436D90` | 32 | BattleEnemy_Chance70 (ours) | `.data 0x64B20C` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E46B8 |
| `0x436DB0` | 80 | BattleEnemy_Chance70 (ours) | `.data 0x64B234` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E46FC |
| `0x436E00` | 64 | BattleEnemy_Chance70 (ours) | `.data 0x64B238` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E4738 |
| `0x436E40` | 128 | BattleEnemy_Chance70 (ours) | `.data 0x64B23C` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E4794 |
| `0x436EC0` | 64 | BattleEnemy_Chance70 (ours) | `.data 0x64B240` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E482C |
| `0x437420` | 40 | BattleEnemy_Chance70 (ours) | `.data 0x64B1CC`, `.data 0x64C8BC`, `.data 0x64C900` | indirect call | BattleEnemy_RunAll | combat | Battle engine (BATTLE.EMI) | psx 801E51C8 |

### CG - the battle object states

`src/game/battle_obj_states.cpp`, 22 functions, 2,356 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x441200` | 164 | BattleObj_RunState (ours) | `.data 0x64DFE0` | indirect call | BattleParty_RunStates | combat | Battle engine (BATTLE.EMI) | Battle_InitMemberActor |
| `0x4414E0` | 112 | BattleObj_PickPose (ours) | `.text 0x44150C` | indirect call | `0x441550` | combat | Boot: battle |  |
| `0x441550` | 32 | BattleObj_PickPose (ours) | `.data 0x64DFE8` | indirect call | BattleParty_RunStates | combat | Battle engine (BATTLE.EMI) | psx 801DF36C |
| `0x441570` | 560 | BattleObj_PickPose (ours) | `.data 0x64DFEC` | indirect call | BattleParty_RunStates | combat | Battle engine (BATTLE.EMI) | psx 801DF3AC |
| `0x4417A0` | 80 | BattleObj_PickPose (ours) | `.data 0x64E014` | indirect call | `0x441570` | combat | Battle engine (BATTLE.EMI) | psx 801DF71C |
| `0x4417F0` | 112 | BattleObj_PickPose (ours) | `.data 0x64E018` | indirect call | `0x441570` | combat | Battle engine (BATTLE.EMI) | psx 801DF7A0 |
| `0x441860` | 48 | BattleObj_PickPose (ours) | `.data 0x64DFF0` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | Attack_DispatchByCharacter |
| `0x441890` | 160 | BattleObj_PickPose (ours) | `.data 0x64E01C`, `.data 0x64E020`, `.data 0x64E024` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801DF8C8 |
| `0x441930` | 32 | BattleObj_PickPose (ours) | `.data 0x64DFF4` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801DF9D0 |
| `0x441950` | 128 | BattleObj_PickPose (ours) | `.data 0x64E074` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | Battle_SwingCue_Step |
| `0x4419D0` | 64 | BattleObj_PickPose (ours) | `.data 0x64E078` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801DFB18 |
| `0x4429E0` | 32 | `0x442420` | `.data 0x64DFFC` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801E1604 |
| `0x442A00` | 288 | `0x442420` | `.data 0x64E0E4` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801E1648 |
| `0x442B20` | 96 | `0x442420` | `.data 0x64E0E8` | indirect call | BattleParty_RunStates | combat | Battle engine (BATTLE.EMI) | psx 801E1814 |
| `0x442B80` | 32 | `0x442420` | `.data 0x64E0EC` | indirect call | BattleParty_RunStates | combat | Battle engine (BATTLE.EMI) | psx 801E18C4 |
| `0x442BA0` | 32 | `0x442420` | `.data 0x64E000` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801E191C |
| `0x442BC0` | 16 | `0x442420` | `.data 0x64E118` | indirect call | `0x442BA0` | combat | Battle engine (BATTLE.EMI) | psx 801E1968 |
| `0x442BD0` | 112 | `0x442420` | `.data 0x64E11C` | indirect call | `0x442BA0` | combat | Battle engine (BATTLE.EMI) | Actor_SkillItemDone |
| `0x442C50` | 32 | `0x442420` | `.data 0x64E128` | indirect call | `0x442BA0` | combat | Battle engine (BATTLE.EMI) | psx 801E1A88 |
| `0x442D60` | 32 | `0x442420` | `.data 0x64E010` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801E1C5C |
| `0x442D80` | 48 | `0x442420` | `.rdata 0x5C72FD`, `.rdata 0x5C733D`, `.data 0x64E134` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801E1CA0 |
| `0x442DB0` | 144 | `0x442420` | `.data 0x64E138` | indirect call | `0x42E370` | combat | Battle engine (BATTLE.EMI) | psx 801E1D0C |

### CH - actor copies and the battle item menu

`src/game/battle_actor_copies.cpp`, 16 functions, 2,647 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x447110` | 16 | Battle_SpawnActorCopies (ours) | `.data 0x64AE54` | jumped to | Battle_PhaseDispatch | combat | Boot: unnamed | psx 80093A74 |
| `0x447120` | 32 | Battle_SpawnActorCopies (ours) | `.data 0x64E3EC` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x447140` | 32 | Battle_SpawnActorCopies (ours) | `.data 0x64E3F0` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x447160` | 48 | Battle_SpawnActorCopies (ours) | `.data 0x64E3F4` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x447190` | 256 | Battle_SpawnActorCopies (ours) | `.data 0x64E3F8` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x447390` | 80 | Battle_SpawnActorCopies (ours) | `.data 0x64E400` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x447430` | 16 | Battle_SpawnActorCopies (ours) | `.data 0x64AE58` | jumped to | Battle_PhaseDispatch | combat | Boot: unnamed | psx 80093F00 |
| `0x447440` | 32 | Battle_SpawnActorCopies (ours) | `.data 0x64E408` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x447460` | 80 | Battle_SpawnActorCopies (ours) | `.data 0x64E424` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x4474B0` | 903 | Battle_SpawnActorCopies (ours) | `.data 0x64E40C` | jumped to | Battle_PhaseDispatch | combat | Boot: battle |  |
| `0x4478B0` | 144 | ItemMenu_CanUseSelected (ours) | `.data 0x64E414` | jumped to | Battle_PhaseDispatch | combat | Boot: party state, items and saves |  |
| `0x447940` | 32 | ItemMenu_CanUseSelected (ours) | `.data 0x64E418` | jumped to | Battle_PhaseDispatch | combat | Boot: party state, items and saves |  |
| `0x447960` | 128 | ItemMenu_CanUseSelected (ours) | `.data 0x64E428` | jumped to | Battle_PhaseDispatch | combat | Boot: party state, items and saves |  |
| `0x4479E0` | 304 | ItemMenu_CanUseSelected (ours) | `.data 0x64E42C` | jumped to | Battle_PhaseDispatch | combat | Boot: party state, items and saves |  |
| `0x447B10` | 432 | ItemMenu_CanUseSelected (ours) | `.data 0x64E430` | jumped to | Battle_PhaseDispatch | combat | Boot: party state, items and saves |  |
| `0x447CC0` | 112 | ItemMenu_CanUseSelected (ours) | `.data 0x64E434`, `.data 0x64E444` | jumped to | Battle_PhaseDispatch | combat | Boot: party state, items and saves |  |

### CI - the battle menu states

`src/game/battle_menu_states.cpp`, 15 functions, 2,816 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x447FD0` | 16 | `0x447F40` | `.data 0x64AE60` | jumped to | Battle_PhaseDispatch | combat | Boot: unnamed | psx 80095070 |
| `0x447FE0` | 64 | `0x447F40` | `.data 0x64E44C` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x448020` | 192 | `0x447F40` | `.data 0x64E450` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x4480E0` | 96 | `0x447F40` | `.data 0x64E454` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x448180` | 16 | `0x447F40` | `.data 0x64AE5C` | jumped to | Battle_PhaseDispatch | combat | Boot: unnamed | psx 80095314 |
| `0x448190` | 32 | `0x447F40` | `.data 0x64E45C` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x4481B0` | 48 | `0x447F40` | `.rdata 0x5C7001`, `.rdata 0x5C7041`, `.data 0x64E420` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x4481E0` | 48 | `0x447F40` | `.data 0x64E488` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x448210` | 1008 | `0x447F40` | `.data 0x64E460` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x448630` | 144 | `0x447F40` | `.data 0x64E468` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x4486C0` | 32 | `0x447F40` | `.data 0x64E46C` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x4486E0` | 160 | `0x447F40` | `.data 0x64E48C` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x448780` | 320 | `0x447F40` | `.data 0x64E490` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x4488C0` | 432 | `0x447F40` | `.data 0x64E494` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |
| `0x448A70` | 208 | `0x447F40` | `.data 0x64E498`, `.data 0x64E4A8` | jumped to | Battle_PhaseDispatch | combat | Battle engine (BATTLE.EMI) |  |

### CJ - magic effects the fight casts

`src/game/magic_fx_reached.cpp`, 25 functions, 2,837 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x4AD130` | 48 | `0x4ACA50` | `.text 0x4ACFF7`, `.text 0x4C4FD7` | jumped to | `0x4C4FC0` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4AD160` | 96 | `0x4ACA50` | `.text 0x4AD002`, `.text 0x4C4FE2` | jumped to | `0x4C4FC0` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4AD200` | 112 | `0x4ACA50` | `.data 0x65AA28`, `.data 0x65B5B8` | indirect call | `0x4C5110` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4AD270` | 64 | `0x4ACA50` | `.data 0x65AA2C`, `.data 0x65B5BC` | indirect call | `0x4C5110` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4AD2B0` | 80 | `0x4ACA50` | `.data 0x65AA30`, `.data 0x65B5C0` | indirect call | `0x4C5110` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4B54B0` | 64 | `0x4B5350` | `.data 0x64C4EC` | jumped to | BattleTask_RunAll | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4B54F0` | 640 | `0x4B5350` | `.text 0x4B54BF` | jumped to | `0x4B54B0` | combat | Battle magic effects (BMAGIC overlays) | psx 801EEC90 |
| `0x4B5770` | 80 | `0x4B5350` | `.text 0x4B54C7` | jumped to | `0x4B54B0` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4B57C0` | 80 | `0x4B5350` | `.text 0x4B54D2` | jumped to | `0x4B54B0` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4B5810` | 32 | `0x4B5350` | `.text 0x4355F4` | jumped to | `0x435350` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4B5830` | 80 | `0x4B5350` | `.data 0x65AC28` | jumped to | `0x435350` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4B5880` | 64 | `0x4B5350` | `.text 0x4B5847` | jumped to | `0x4B5830` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4B58C0` | 45 | `0x4B5350` | `.text 0x4B5852` | jumped to | `0x4B5830` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4B8D70` | 144 | `0x4B8BD0` | `.data 0x64C41C` | jumped to | BattleTask_RunAll | combat | Unlabelled |  |
| `0x4B8E00` | 336 | `0x4B8BD0` | `.text 0x4B8D85` | jumped to | `0x4B8D70` | combat | Unlabelled |  |
| `0x4B8F50` | 133 | `0x4B8BD0` | `.text 0x4B8DA5`, `.text 0x4B9965`, `.text 0x4BA495` | jumped to | `0x4B8D70` | combat | Unlabelled |  |
| `0x4B9000` | 144 | Sparkle_Dispatch (ours) | `.data 0x65AE28` | indirect call | `0x4B8D70` | combat | Boot: battle |  |
| `0x4C4FC0` | 96 | `0x4C4C00` | `.data 0x64C364` | jumped to | BattleTask_RunAll | combat | Unlabelled |  |
| `0x4C5020` | 240 | `0x4C4C00` | `.text 0x4C4FCF` | jumped to | `0x4C4FC0` | combat | Unlabelled |  |
| `0x4C5110` | 51 | `0x4C4C00` | `.text 0x43540A` | jumped to | `0x435350` | combat | Unlabelled |  |
| `0x4F52D0` | 32 | `0x4F5050` | `.text 0x4B54DA`, `.text 0x4F50D2` | jumped to | `0x4B54B0` | combat | Battle magic effects (BMAGIC overlays) |  |
| `0x4FAFF0` | 32 | `0x4FAF90` | `.text 0x4355DE` | jumped to | `0x435350` | combat | Unlabelled |  |
| `0x4FB010` | 64 | `0x4FAF90` | `.data 0x65C3A4` | jumped to | `0x435350` | combat | Unlabelled |  |
| `0x4FB050` | 32 | `0x4FAF90` | `.data 0x65C3A8` | jumped to | `0x435350` | combat | Unlabelled |  |
| `0x4FB070` | 48 | `0x4FAF90` | `.data 0x65AC0C`, `.data 0x65C3AC` | jumped to | `0x435350` | combat | Unlabelled |  |

### CK - banner, effect result, sparkle and the odd ones

`src/game/battle_odds.cpp`, 10 functions, 592 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x44A740` | 112 | BattleBanner_Set (ours) | `.text 0x44A5D4` | jumped to | BattleBanner_Dispatch | combat | Boot: battle |  |
| `0x44A7B0` | 88 | BattleBanner_Set (ours) | `.text 0x44A5DC` | jumped to | BattleBanner_Dispatch | combat | Boot: battle |  |
| `0x44C140` | 16 | Effect_ApplyResult (ours) | `.data 0x64E75C` | indirect call | Effect_ApplyResult | combat | Boot: battle |  |
| `0x44C990` | 48 | Effect_ApplyResult (ours) | `.data 0x64E7B8` | indirect call | Effect_ApplyResult | combat | Boot: battle |  |
| `0x4AEE90` | 16 | `0x4AE130` | `.text 0x43490C`, `.text 0x434BBC`, `.text 0x434D9C` | jumped to | `0x4B5830` | combat | Boss battle scripts (BOSS overlays) | psx 800C2120 |
| `0x4B1E70` | 96 | `0x4B1520` | `.text 0x4B1CC7`, `.text 0x4B8D8D`, `.text 0x4B994D` | jumped to | `0x4B8D70` | combat | Unlabelled |  |
| `0x4B1ED0` | 112 | `0x4B1520` | `.text 0x4B1CD2`, `.text 0x4B8D95`, `.text 0x4B9955` | jumped to | `0x4B8D70` | combat | Unlabelled |  |
| `0x4ED5C0` | 32 | `0x4ECE80` | `.text 0x49C19F`, `.text 0x4A2DEC`, `.text 0x4A2F79` | jumped to | `0x4B5830` | combat | Unlabelled |  |
| `0x4EE8A0` | 24 | `0x4EE580` | `.text 0x4B8D9D`, `.text 0x4B995D`, `.text 0x4BA48D` | jumped to | `0x4B8D70` | combat | Unlabelled |  |
| `0x4F7350` | 48 | `0x4F7190` | `.text 0x49C3E7`, `.text 0x4A4C32`, `.text 0x4A647A` | jumped to | `0x4B8D70` | combat | Unlabelled |  |

### CL - the battle windows under the window task

`src/game/battle_win_states.cpp`, 19 functions, 2,612 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x596FA0` | 96 | `0x596A90` | `.text 0x59E25C` | jumped to | Field_RunTaskRecords | combat | Unlabelled |  |
| `0x597000` | 48 | `0x596A90` | `.text 0x596FAF` | jumped to | `0x596FA0` | combat | Unlabelled |  |
| `0x597030` | 64 | `0x596A90` | `.text 0x59700F` | jumped to | `0x597000` | combat | Unlabelled |  |
| `0x597070` | 32 | `0x596A90` | `.text 0x597017` | jumped to | `0x597000` | combat | Unlabelled |  |
| `0x597090` | 48 | `0x596A90` | `.text 0x596FB7` | jumped to | `0x596FA0` | combat | Unlabelled |  |
| `0x5970C0` | 160 | `0x596A90` | `.text 0x59709F` | jumped to | `0x597090` | combat | Unlabelled |  |
| `0x597160` | 80 | `0x596A90` | `.text 0x5970A7` | jumped to | `0x597090` | combat | Unlabelled |  |
| `0x5971B0` | 48 | `0x596A90` | `.text 0x596FC2` | jumped to | `0x596FA0` | combat | Unlabelled |  |
| `0x5971E0` | 32 | `0x596A90` | `.text 0x5971CC` | jumped to | `0x5971B0` | combat | Unlabelled |  |
| `0x597200` | 48 | `0x596A90` | `.text 0x596FCA` | jumped to | `0x596FA0` | combat | Unlabelled |  |
| `0x597230` | 240 | `0x596A90` | `.text 0x597217` | jumped to | `0x597200` | combat | Battle engine (BATTLE.EMI) | psx 801E9470 |
| `0x597320` | 224 | `0x596A90` | `.text 0x596FD2` | jumped to | `0x596FA0` | combat | Unlabelled |  |
| `0x597400` | 192 | `0x596A90` | `.text 0x59732F` | jumped to | `0x597320` | combat | Unlabelled |  |
| `0x5974C0` | 80 | `0x596A90` | `.text 0x597337` | jumped to | `0x597320` | combat | Unlabelled |  |
| `0x597510` | 192 | `0x596A90` | `.text 0x597342` | jumped to | `0x597320` | combat | Unlabelled |  |
| `0x5975D0` | 256 | `0x596A90` | `.text 0x596FDA` | jumped to | `0x596FA0` | combat | Unlabelled |  |
| `0x5976D0` | 384 | `0x596A90` | `.text 0x5975DF` | jumped to | `0x5975D0` | combat | Unlabelled |  |
| `0x597850` | 96 | `0x596A90` | `.text 0x5975E7` | jumped to | `0x5975D0` | combat | Unlabelled |  |
| `0x5978B0` | 292 | `0x596A90` | `.text 0x5975F2` | jumped to | `0x5975D0` | combat | Unlabelled |  |

### CM - the window-kind handlers

`src/game/window_kinds.cpp`, 11 functions, 1,104 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x597A80` | 336 | Window_DispatchKind (ours) | `.text 0x597A63` | jumped to | Window_DispatchKind | combat | Boot: text, windows and menus |  |
| `0x597BD0` | 64 | Window_DispatchKind (ours) | `.text 0x597A6B` | jumped to | Window_DispatchKind | combat | Boot: text, windows and menus |  |
| `0x597C10` | 96 | Window_DispatchKind (ours) | `.text 0x597A73` | jumped to | Window_DispatchKind | combat | Boot: text, windows and menus |  |
| `0x597C70` | 128 | Window_DispatchKind (ours) | `.text 0x596FE2` | jumped to | `0x596FA0` | combat | Boot: text, windows and menus |  |
| `0x597CF0` | 32 | Window_DispatchKind (ours) | `.text 0x597C7F` | jumped to | `0x597C70` | combat | Boot: text, windows and menus |  |
| `0x597D50` | 64 | Window_DispatchKind (ours) | `.text 0x596FEA` | jumped to | `0x596FA0` | combat | Boot: text, windows and menus |  |
| `0x597D90` | 48 | Window_DispatchKind (ours) | `.text 0x597D5F` | jumped to | `0x597D50` | combat | Boot: text, windows and menus |  |
| `0x597DC0` | 48 | Window_DispatchKind (ours) | `.text 0x597D67` | jumped to | `0x597D50` | combat | Boot: text, windows and menus |  |
| `0x597DF0` | 112 | Window_DispatchKind (ours) | `.text 0x597D72` | jumped to | `0x597D50` | combat | Boot: text, windows and menus |  |
| `0x597E60` | 112 | Window_DispatchKind (ours) | `.text 0x597D7A` | jumped to | `0x597D50` | combat | Boot: text, windows and menus |  |
| `0x597F60` | 64 | Text_GlyphCount (ours) | `.text 0x59E264` | jumped to | Field_RunTaskRecords | combat | Boot: text, windows and menus |  |

### DA - the world map area handlers and the effect kinds it runs

`src/game/worldmap_area.cpp`, 31 functions, 3,703 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x4037B0` | 208 | `0x402570` | `.data 0x5EE2B0` | indirect call | Area_Enter | combat | Area overlays, world 0 | psx 801F2D2C |
| `0x403CE0` | 96 | `0x402570` | `.data 0x5EF4C8` | indirect call | `0x577B80` | worldmap | Area overlays, world 0 | psx 801F2C38 |
| `0x403D40` | 48 | `0x402570` | `.data 0x5EF4CC` | indirect call | `0x577B80` | worldmap | Area overlays, world 0 | psx 801F2CD0 |
| `0x403D70` | 144 | `0x402570` | `.data 0x662DF4` | indirect call | GameMode_Field | worldmap | Area overlays, world 0 |  |
| `0x403E00` | 224 | `0x402570` | `.data 0x65392C` | indirect call | Effect_RunObjects | worldmap | Area overlays, world 0 |  |
| `0x403EE0` | 336 | `0x402570` | `.data 0x5EF5E0` | indirect call | `0x403E00` | worldmap | Area overlays, world 0 |  |
| `0x404030` | 80 | `0x402570` | `.data 0x5EF5E4` | indirect call | `0x403E00` | worldmap | Area overlays, world 0 |  |
| `0x404080` | 96 | `0x402570` | `.data 0x5EF5E8` | indirect call | `0x403E00` | worldmap | Area overlays, world 0 |  |
| `0x4040E0` | 80 | `0x402570` | `.data 0x5EF5EC` | indirect call | `0x403E00` | worldmap | Area overlays, world 0 |  |
| `0x404130` | 32 | `0x402570` | `.data 0x653938` | indirect call | Effect_RunObjects | worldmap | Area overlays, world 0 |  |
| `0x404150` | 10 | `0x402570` | `.data 0x5EF5F4` | indirect call | Effect_RunObjects | worldmap | Area overlays, world 0 |  |
| `0x404180` | 96 | WorldMap_FrameStep (ours) | `.data 0x5EF5FC` | indirect call | `0x404150` | worldmap | Boot: top-level modes and tasks |  |
| `0x404230` | 32 | WorldMap_FrameStep (ours) |  | indirect call | Effect_RunObjects | worldmap | Boot: top-level modes and tasks |  |
| `0x404250` | 112 | WorldMap_FrameStep (ours) | `.data 0x5EF60C` | indirect call | Effect_RunObjects | worldmap | Boot: top-level modes and tasks |  |
| `0x4042C0` | 112 | WorldMap_FrameStep (ours) | `.data 0x5EF610` | indirect call | Effect_RunObjects | worldmap | Boot: top-level modes and tasks |  |
| `0x404330` | 87 | WorldMap_FrameStep (ours) | `.data 0x5EF614` | indirect call | Effect_RunObjects | worldmap | Boot: top-level modes and tasks |  |
| `0x4048E0` | 1136 | WorldMap_DrawHud (ours) | `.data 0x65393C` | indirect call | Effect_RunObjects | worldmap | Boot: top-level modes and tasks |  |
| `0x411310` | 48 | `0x4112F0` | `.data 0x5E5F78`, `.data 0x5EF5F8`, `.data 0x5F7670` | indirect call | `0x404150` | worldmap | Unlabelled |  |
| `0x414BB0` | 64 | `0x414AC0` | `.data 0x5E5F88`, `.data 0x5EF608`, `.data 0x5F7680` | indirect call | Effect_RunObjects | worldmap | Unlabelled |  |
| `0x419110` | 32 | `0x418A40` | `.data 0x5E5F70`, `.data 0x5EF5F0`, `.data 0x5F7668` | indirect call | Effect_RunObjects | worldmap | Area overlays, world 3 |  |
| `0x462A90` | 38 (4,560 calls) |  |  | call (recorded start) |  | worldmap | Top-level modes |  |
| `0x462AE0` | 32 | `0x462AC0` | Effect_KindHandlers | indirect call | Effect_RunObjects | worldmap | Top-level modes |  |
| `0x462B40` | 32 | `0x462AC0` | `.data 0x6554B0` | indirect call | Effect_RunObjects | worldmap | Table Effect_KindHandlers | Effect_KindHandlers |
| `0x462B80` | 32 | `0x462AC0` | `.data 0x654074`, `.data 0x654078` | indirect call | Effect_RunObjects | worldmap | Top-level modes |  |
| `0x469BB0` | 32 | `0x469AD0` | `.data 0x655368` | indirect call | Effect_RunObjects | combat | Table Effect_KindHandlers | Effect_KindHandlers |
| `0x469BD0` | 224 | `0x469AD0` | `.data 0x653EC8`, `.data 0x653EF4` | indirect call | Effect_RunObjects | combat | Field core (GAME.EMI) | psx 8019B404 |
| `0x469CB0` | 32 | `0x469AD0` | `.data 0x653ECC` | indirect call | Effect_RunObjects | combat | Field core (GAME.EMI) | psx 8019B544 |
| `0x469CD0` | 64 | `0x469AD0` | `.data 0x653EDC`, `.data 0x653EE0`, `.data 0x653EE4` | indirect call | Effect_RunObjects | combat | Field core (GAME.EMI) | psx 8019B588 |
| `0x469DB0` | 48 | `0x469AD0` | `.data 0x653ED0`, `.data 0x653EFC` | indirect call | Effect_RunObjects | combat | Field core (GAME.EMI) | psx 8019B7DC |
| `0x46D830` | 32 | `0x46D770` | `.data 0x6553B0` | indirect call | Effect_RunObjects | worldmap | Table Effect_KindHandlers | Effect_KindHandlers |
| `0x46D850` | 64 | `0x46D770` | `.data 0x65406C` | indirect call | Effect_RunObjects | worldmap | Boot: unnamed |  |

### DB - top-level modes, the system choice and the field core

`src/game/mode_states.cpp`, 21 functions, 3,630 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x496250` | 64 (18 calls) |  |  | call (recorded start) |  | worldmap | Top-level modes |  |
| `0x496290` | 16 | `0x496250` | `.data 0x656A60` | indirect call | Field_Task | shop | Top-level modes |  |
| `0x4962A0` | 240 | `0x496250` | `.data 0x656AAC` | indirect call | Field_Task | shop | Top-level modes |  |
| `0x496390` | 160 | `0x496250` | `.data 0x656AB4` | indirect call | Field_Task | shop | Top-level modes |  |
| `0x496830` | 58 (2 calls) |  |  | call (recorded start) |  | worldmap | Top-level modes |  |
| `0x496A00` | 193 (176 calls) |  |  | call (recorded start) |  | worldmap | Top-level modes |  |
| `0x496AD0` | 144 (18 calls) |  |  | call (recorded start) |  | worldmap | Top-level modes |  |
| `0x497C30` | 288 | MsgBox_Step (ours) | `.text 0x497E84` | OS callback | outside the exe | shop | Text and windows |  |
| `0x498A30` | 160 (1 calls) |  |  | call (recorded start) |  | shop | Boot: text, windows and menus | MsgBox_SystemChoice |
| `0x516E70` | 238 (8,255 calls) |  |  | call (recorded start) |  | worldmap, combat | Boot: unnamed | psx 8014FD78 |
| `0x517290` | 35 (260 calls) |  |  | call (recorded start) |  | worldmap, combat | Field objects | Field_LoadingFrame |
| `0x5172F0` | 16 (38 calls) |  |  | call (recorded start) |  | worldmap | Field objects |  |
| `0x517300` | 48 | `0x5172F0` | `.data 0x656AB0` | indirect call | Field_Task | shop | Field objects |  |
| `0x525370` | 18 | `0x525150` | `.data 0x65F968`, `.data 0x660920` | indirect call | Field_MembersFrame | shop, worldmap, combat | Field core (GAME.EMI) | psx 801B79B8 |
| `0x5258B0` | 32 | `0x525390` | `.data 0x660124` | indirect call | Field_MembersFrame | shop, worldmap, combat | Field core (GAME.EMI) | psx 801B8264 |
| `0x5258D0` | 80 | `0x525390` | `.data 0x660158` | indirect call | Field_MembersFrame | shop, worldmap, combat | Field core (GAME.EMI) | psx 801B82A8 |
| `0x525920` | 64 | `0x525390` | `.data 0x66015C` | indirect call | Field_MembersFrame | shop, worldmap, combat | Field core (GAME.EMI) | psx 801B833C |
| `0x539AD0` | 16 | Scenario_NoHook (ours) | `.data 0x660D68` | indirect call | Field_ModeDispatch | shop, worldmap, combat | Boot: top-level modes and tasks |  |
| `0x539B20` | 416 | Scenario_NoHook (ours) | `.data 0x660D80` | indirect call | Field_ModeDispatch | shop, worldmap, combat | Boot: top-level modes and tasks |  |
| `0x53D830` | 1264 | `0x53D3B0` | `.data 0x660D70` | indirect call | Scenario_StepHook | shop, worldmap, combat | Unlabelled |  |
| `0x5960D0` | 80 | `0x596090` |  | jumped to | Window_Kind2States | shop | Text and windows |  |

### DC - the event script: the leader, the steps, the placements

`src/game/event_leader.cpp`, 27 functions, 5,517 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x52E110` | 40 | Field_LeaderSetPace (ours) | `.data 0x66095C` | indirect call | Field_LeaderControl | shop, worldmap, combat | Event script |  |
| `0x52E580` | 1104 | Field_LeaderStepTarget (ours) |  | indirect call | Field_LeaderControl | shop, worldmap, combat | Event script |  |
| `0x52E9D0` | 32 | Field_LeaderStepTarget (ours) | `.data 0x660924` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52E9F0` | 420 | Field_LeaderStepTarget (ours) | `.data 0x660968` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52EBA0` | 126 (20 calls) |  |  | call (recorded start) |  | worldmap | Event script |  |
| `0x52ED80` | 240 | `0x52EC20` | `.data 0x66096C` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52EE70` | 832 | `0x52EC20` | `.data 0x660970` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52F1B0` | 416 | `0x52EC20` | `.data 0x660974` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52F350` | 64 | `0x52EC20` | `.data 0x660978` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52F390` | 32 | `0x52EC20` | `.data 0x660928` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52F3B0` | 32 | `0x52EC20` | `.data 0x66097C` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52F3D0` | 48 | `0x52EC20` | `.data 0x660980` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x52F400` | 32 | `0x52EC20` | `.data 0x66092C` | indirect call | Field_MembersFrame | combat | Event script |  |
| `0x52F420` | 112 | `0x52EC20` | `.data 0x660984` | indirect call | Field_MembersFrame | combat | Event script |  |
| `0x52F490` | 96 | `0x52EC20` | `.data 0x660988` | indirect call | Field_MembersFrame | combat | Event script |  |
| `0x52F5E0` | 32 | Sprite_TurnSense (ours) | `.data 0x660934` | indirect call | Field_MembersFrame | shop | Event script |  |
| `0x52F600` | 352 | Sprite_TurnSense (ours) | `.data 0x66098C` | indirect call | `0x52F5E0` | shop | Event script |  |
| `0x52F760` | 320 | Sprite_TurnSense (ours) | `.data 0x660990` | indirect call | `0x52F5E0` | shop | Event script |  |
| `0x52F8A0` | 80 | Sprite_TurnSense (ours) | `.data 0x660994` | indirect call | `0x52F5E0` | shop | Event script |  |
| `0x52FB60` | 80 | Sprite_TurnSense (ours) | `.data 0x660940` | indirect call | Field_MembersFrame | worldmap | Event script |  |
| `0x5317F0` | 46 (1 calls) |  |  | call (recorded start) |  | worldmap | Event script |  |
| `0x531920` | 43 (562 calls) |  |  | call (recorded start) |  | worldmap | Event script |  |
| `0x531AF0` | 111 (2 calls) |  |  | call (recorded start) |  | worldmap | Event script |  |
| `0x533780` | 304 | Field_PendingJump (ours) | `.data 0x660B60` | indirect call | Field_MembersFrame | shop, worldmap, combat | Event script |  |
| `0x535610` | 41 (20 calls) |  |  | call (recorded start) |  | worldmap | Event script |  |
| `0x535640` | 232 (20 calls) |  |  | call (recorded start) |  | worldmap | Event script |  |
| `0x535730` | 250 (20 calls) |  |  | call (recorded start) |  | worldmap | Event script |  |

### DD - map layers and field objects

`src/game/map_field_objects.cpp`, 16 functions, 4,236 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x570210` | 800 | Area_TestCondition (ours) | `.data 0x66300C`, `.data 0x663010`, `.data 0x663014` | indirect call | DrawLayer_Open | shop, worldmap, combat | Map and draw layers |  |
| `0x570530` | 304 | Area_TestCondition (ours) | `.data 0x663018`, `.data 0x6630D8` | indirect call | DrawLayer_Open | worldmap | Map and draw layers |  |
| `0x570A00` | 166 | Area_TestCondition (ours) | `.data 0x66304C`, `.data 0x663050`, `.data 0x663054` | indirect call | DrawLayer_Open | shop, worldmap | Map and draw layers |  |
| `0x571090` | 64 | MapCell_FlatOverlay (ours) | `.data 0x663098` | indirect call | DrawLayer_Open | shop, worldmap | Map and draw layers |  |
| `0x5710D0` | 52 | MapCell_FlatOverlay (ours) | `.data 0x66309C` | indirect call | DrawLayer_Open | shop, worldmap | Map and draw layers |  |
| `0x5712E0` | 544 | AreaMap_ApplyPatch (ours) | `.data 0x6630A4` | indirect call | DrawLayer_Open | shop, worldmap | Map and draw layers |  |
| `0x571880` | 111 | AreaMap_ClutCycleStart (ours) | AreaMap_SetupHandlers | indirect call | AreaMap_SetupEntries | shop, worldmap, combat | Map and draw layers |  |
| `0x571A30` | 64 | Gfx_ClutAdjust (ours) | `.data 0x663294` | indirect call | AreaMap_SetupEntries | shop, worldmap, combat | Map and draw layers |  |
| `0x571A70` | 113 | Gfx_ClutAdjust (ours) | `.data 0x663298` | indirect call | AreaMap_SetupEntries | worldmap | Map and draw layers |  |
| `0x573560` | 818 (114 calls) |  |  | call (recorded start) |  | worldmap | Field objects |  |
| `0x5744B0` | 118 (114 calls) |  |  | call (recorded start) |  | worldmap | Field objects |  |
| `0x5746C0` | 264 (38 calls) |  |  | call (recorded start) |  | worldmap | Field objects |  |
| `0x579CF0` | 188 (1 calls) |  |  | call (recorded start) |  | worldmap | Field objects | EventScript_SkipSwitch |
| `0x579F00` | 39 (30 calls) |  |  | call (recorded start) |  | worldmap | Field objects |  |
| `0x57A3A0` | 569 (7 calls) |  |  | call (recorded start) |  | worldmap | Field objects | EventOp_8x |
| `0x57C7A0` | 22 (1 calls) |  |  | call (recorded start) |  | worldmap | Field objects | ScriptFlags_Clear40 |

### DE - field-side hidden functions: direction, party sets, inventory ops

`src/game/field_hidden.cpp`, 15 functions, 2,366 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x51BA60` | 32 | Field_DirectionTo (ours) | `.data 0x65F96C` | indirect call | Field_MembersFrame | worldmap | Boot: field, map and sprites |  |
| `0x51BBD0` | 288 | Field_DirectionTo (ours) | `.data 0x65F97C` | indirect call | Field_MembersFrame | worldmap | Boot: field, map and sprites |  |
| `0x51DA30` | 102 | `0x51D4E0` | `.data 0x65F9C8`, `.data 0x65FA20`, `.data 0x65FA7C` | indirect call | `0x52FB60` | worldmap | Party character sets (PLP overlays) |  |
| `0x51E910` | 32 | `0x51E6C0` | `.data 0x65FC18` | indirect call | `0x52FB60` | worldmap | Unlabelled |  |
| `0x51E930` | 448 | `0x51E6C0` | `.data 0x65FBC0` | indirect call | `0x52FB60` | worldmap | Unlabelled |  |
| `0x51EAF0` | 219 | `0x51E6C0` | `.data 0x65FBC4` | indirect call | `0x52FB60` | worldmap | Unlabelled |  |
| `0x51EBD0` | 288 (3 calls) |  |  | call (recorded start) |  | worldmap | Unlabelled |  |
| `0x51F1B0` | 32 | `0x51EF30` | `.data 0x6609E4` | indirect call | `0x52FB60` | worldmap | Unlabelled |  |
| `0x589110` | 69 (3 calls) |  |  | call (recorded start) |  | combat | Field objects |  |
| `0x589160` | 89 (12 calls) |  |  | call (recorded start) |  | combat | Field objects |  |
| `0x5891C0` | 45 (9 calls) |  |  | call (recorded start) |  | combat | Field objects |  |
| `0x5898D0` | 159 (16 calls) |  |  | call (recorded start) |  | worldmap | Field objects | EventOp_Ex |
| `0x591F30` | 427 (83 calls) |  |  | call (recorded start) |  | worldmap, combat | Field core (GAME.EMI) | psx 801C70D8 |
| `0x592570` | 43 (83 calls) |  |  | call (recorded start) |  | worldmap, combat | Field core (GAME.EMI) | psx 801C7A74 |
| `0x5925A0` | 93 (83 calls) |  |  | call (recorded start) |  | worldmap, combat | Field core (GAME.EMI) | psx 801C7AE4 |

### DF - the shop overlay, first table

`src/game/shop_states.cpp`, 25 functions, 2,800 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x57F500` | 32 | `0x57F340` |  | indirect call | Field_Task | shop | Unlabelled |  |
| `0x57F520` | 176 | `0x57F340` | `.data 0x663E40` | indirect call | Field_Task | shop | Unlabelled |  |
| `0x57F5D0` | 128 | `0x57F340` | `.data 0x663E44`, `.data 0x663E50` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D0FF8 |
| `0x57F650` | 16 | `0x57F340` | `.data 0x663E4C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57F660` | 128 | `0x57F340` | `.data 0x663F4C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57F6E0` | 16 | `0x57F340` | `.data 0x663F50` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57F6F0` | 112 | `0x57F340` | `.data 0x663F60` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57F760` | 176 | `0x57F340` | `.data 0x663F64` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D3BA8 |
| `0x57F810` | 144 | `0x57F340` | `.data 0x663F68` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57F8A0` | 416 | `0x57F340` | `.data 0x663F6C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D3D94 |
| `0x57FAB0` | 176 | `0x57F340` | `.data 0x663F74` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FB60` | 16 | `0x57F340` | `.data 0x663F54` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FB70` | 112 | `0x57F340` | `.data 0x663F78` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FBE0` | 96 | `0x57F340` | `.data 0x663F7C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FC40` | 48 | `0x57F340` | `.data 0x663F80` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FC70` | 64 | `0x57F340` | `.data 0x663F84` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FCB0` | 112 | `0x57F340` | `.data 0x663F88` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D447C |
| `0x57FD20` | 96 | `0x57F340` | `.data 0x663F8C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FD80` | 80 | `0x57F340` | `.data 0x663F90` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FDD0` | 16 | `0x57F340` | `.data 0x663F58` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FDE0` | 32 | `0x57F340` | `.data 0x663F94` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FE00` | 112 | `0x57F340` | `.data 0x663F98` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x57FE70` | 272 | `0x57F340` | `.data 0x663F9C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x580150` | 80 | `0x57F340` | `.data 0x663FAC` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x5801A0` | 144 | `0x57F340` | `.data 0x663FB0` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |

### DG - the shop overlay, second table, and the save summaries

`src/game/shop_states2.cpp`, 26 functions, 6,012 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x5818B0` | 16 | `0x581720` | `.data 0x663E48` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x5818C0` | 16 | `0x581720` | `.data 0x664118` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x5818D0` | 160 | `0x581720` | `.data 0x66412C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D1154 |
| `0x581970` | 64 | `0x581720` | `.data 0x664130` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x5819B0` | 16 | `0x581720` | `.data 0x66411C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x5819C0` | 288 | `0x581720` | `.data 0x664134` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x581AE0` | 16 | `0x581720` | `.data 0x664120` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x581AF0` | 240 | `0x581720` | `.data 0x664138` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x581BE0` | 752 | `0x581720` | `.data 0x66413C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D1634 |
| `0x581ED0` | 448 | `0x581720` | `.data 0x664140` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D1A88 |
| `0x582090` | 336 | `0x581720` | `.data 0x664144` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D1D58 |
| `0x5821E0` | 224 | `0x581720` | `.data 0x664148` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D1F74 |
| `0x5822C0` | 528 | `0x581720` | `.data 0x66414C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D20D8 |
| `0x5824D0` | 464 | `0x581720` | `.data 0x664150` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D23EC |
| `0x5826A0` | 208 | `0x581720` | `.data 0x664154` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801D26B0 |
| `0x582770` | 16 | `0x581720` | `.data 0x664124` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x582780` | 112 | `0x581720` | `.data 0x664158`, `.data 0x66417C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x5827F0` | 880 | `0x581720` | `.data 0x66415C`, `.data 0x664180` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x582B60` | 416 | `0x581720` | `.data 0x664160`, `.data 0x664184` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x582D00` | 384 | `0x581720` | `.data 0x664164`, `.data 0x664188` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x582E80` | 48 | `0x581720` | `.data 0x664168`, `.data 0x66418C` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) |  |
| `0x584F70` | 27 | `0x584120` | `.data 0x664128`, `.data 0x664264` | indirect call | Field_Task | shop | Shop / inn / save point (SHOP.EMI) | psx 801E0E08 |
| `0x588E70` | 64 | Save_ReadSummaries (ours) | `.text 0x462407` | task entry | a task start | shop, worldmap, combat | Boot: party state, items and saves |  |
| `0x588EB0` | 68 | Save_ReadSummaries (ours) | `.data 0x667294` | indirect call | `0x588E70` | shop, worldmap, combat | Boot: party state, items and saves |  |
| `0x5916B0` | 109 | Item_NamePtr (ours) | `.text 0x59170C` | indirect call | Shop_DrawBuyList | shop | Boot: party state, items and saves |  |
| `0x5917D0` | 112 | Item_EquipMask (ours) | `.text 0x591804` | indirect call | Shop_DrawMemberStats | shop | Boot: party state, items and saves |  |

### DH - the field menu and the list draws

`src/game/menu_lists.cpp`, 20 functions, 2,829 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x589970` | 32 (38 calls) |  |  | call (recorded start) |  | worldmap | Unlabelled |  |
| `0x589990` | 464 | `0x589970` | `.data 0x6672B4` | indirect call | `0x5172F0` | worldmap | Field menu (START.EMI) | psx 801D17B0 |
| `0x589B60` | 16 | `0x589970` | `.data 0x6672B8` | indirect call | `0x5172F0` | worldmap | Field menu (START.EMI) |  |
| `0x589B70` | 656 | `0x589970` | `.data 0x6672D8` | indirect call | `0x5172F0` | worldmap | Field menu (START.EMI) |  |
| `0x589E50` | 13 | `0x589970` | `.data 0x6672E0`, `.data 0x667350` | indirect call | `0x5172F0` | worldmap | Field menu (START.EMI) |  |
| `0x589E60` | 336 (1 calls) |  |  | call (recorded start) |  | worldmap | Field menu (START.EMI) | psx 801D2010 |
| `0x589FE0` | 256 (1 calls) |  |  | call (recorded start) |  | worldmap | Field menu (START.EMI) |  |
| `0x599B50` | 32 | `0x599A00` | `.text 0x59E274` | jumped to | Field_RunTaskRecords | worldmap | Battle engine (BATTLE.EMI) |  |
| `0x599B70` | 32 | `0x599A00` | `.data 0x66AF94` | jumped to | Field_RunTaskRecords | worldmap | Battle engine (BATTLE.EMI) |  |
| `0x599B90` | 160 (114 calls) |  |  | call (recorded start) |  | worldmap | Unlabelled |  |
| `0x599D50` | 64 | `0x599B90` | `.data 0x66AF98` | jumped to | Field_RunTaskRecords | worldmap | Unlabelled |  |
| `0x599DC0` | 48 | `0x599B90` | `.data 0x66AF9C` | jumped to | Field_RunTaskRecords | worldmap | Unlabelled |  |
| `0x599E50` | 288 | `0x599B90` | `.data 0x66AFA0` | jumped to | Field_RunTaskRecords | worldmap | Unlabelled |  |
| `0x599FA0` | 144 | `0x599B90` | `.data 0x66AFA4` | jumped to | Field_RunTaskRecords | worldmap | Unlabelled |  |
| `0x59A3A0` | 48 | `0x599B90` | `.data 0x66AE54`, `.data 0x66AE60`, `.data 0x66AF04` | indirect call | `0x59B310` | shop | Unlabelled |  |
| `0x59A3D0` | 48 | `0x599B90` | `.data 0x66AE58`, `.data 0x66AF08`, `.data 0x66B040` | indirect call | `0x59B240` | shop | Unlabelled |  |
| `0x59A580` | 48 | `0x599B90` | `.data 0x66AE6C`, `.data 0x66B0B0`, `.data 0x66B1B0` | indirect call | `0x59CB40` | combat | Unlabelled |  |
| `0x59A5B0` | 48 | `0x599B90` | `.data 0x66AE70`, `.data 0x66AEFC`, `.data 0x66B0B4` | indirect call | `0x59B3F0` | shop | Unlabelled |  |
| `0x59A5E0` | 48 | `0x599B90` | `.data 0x66AEEC`, `.data 0x66B00C`, `.data 0x66B018` | indirect call | `0x59B560`, `0x59CBC0` | shop, combat | Unlabelled |  |
| `0x59A680` | 48 | `0x599B90` | `.data 0x66AE64`, `.data 0x66B0C8`, `.data 0x66B258` | indirect call | `0x59B350` | shop | Unlabelled |  |

### DI - the shop and menu draw helpers

`src/game/menu_draw_helpers.cpp`, 25 functions, 1,290 bytes.

| Entry | Bytes | Folded into | Pointer in | Reached as | First callers | Routes | Catalogue label | PSX twin / note |
|---|--:|---|---|---|---|---|---|---|
| `0x59B220` | 32 | `0x59AE00` | `.text 0x59E27C` | jumped to | Field_RunTaskRecords | shop | Unlabelled |  |
| `0x59B240` | 208 | `0x59AE00` | `.data 0x66B1F8` | jumped to | Field_RunTaskRecords | shop | Unlabelled |  |
| `0x59B310` | 64 | `0x59AE00` | `.data 0x66B200` | jumped to | Field_RunTaskRecords | shop | Unlabelled |  |
| `0x59B350` | 64 | `0x59AE00` | `.data 0x66B1FC` | jumped to | Field_RunTaskRecords | shop | Unlabelled |  |
| `0x59B390` | 48 | `0x59AE00` | `.data 0x66B260` | indirect call | `0x59B350` | shop | Unlabelled |  |
| `0x59B3C0` | 48 | `0x59AE00` | `.data 0x66B208` | jumped to | Field_RunTaskRecords | shop | Unlabelled |  |
| `0x59B3F0` | 80 | `0x59AE00` | `.data 0x66B20C` | jumped to | Field_RunTaskRecords | shop | Unlabelled |  |
| `0x59B440` | 48 | `0x59AE00` | `.data 0x66B270`, `.data 0x66B284`, `.data 0x66B304` | indirect call | `0x59B3F0` | shop | Unlabelled |  |
| `0x59B470` | 48 | `0x59AE00` | `.data 0x66B278` | indirect call | `0x59B3F0` | shop | Unlabelled |  |
| `0x59B4A0` | 80 | `0x59AE00` | `.data 0x66B27C` | indirect call | `0x59B3F0` | shop | Unlabelled |  |
| `0x59B4F0` | 64 | `0x59AE00` | `.data 0x66B210` | jumped to | Field_RunTaskRecords | shop | Unlabelled |  |
| `0x59B530` | 48 | `0x59AE00` | `.data 0x66B288` | indirect call | `0x59B4F0` | shop | Unlabelled |  |
| `0x59B560` | 32 | `0x59AE00` | `.data 0x66B214` | jumped to | Field_RunTaskRecords | shop | Unlabelled |  |
| `0x59B7B0` | 48 | Shop_DrawBuyList (ours) | `.data 0x66B294` | indirect call | `0x59B560` | shop | Boot: party state, items and saves |  |
| `0x59B7E0` | 48 | Shop_DrawBuyList (ours) | `.data 0x66B298` | indirect call | `0x59B560` | shop | Boot: party state, items and saves |  |
| `0x59B810` | 13 | Shop_DrawBuyList (ours) | `.data 0x66B218` | jumped to | Field_RunTaskRecords | shop | Boot: party state, items and saves |  |
| `0x59BB60` | 32 | Shop_DrawBuyDetail (ours) | `.data 0x66B21C` | jumped to | Field_RunTaskRecords | shop | Boot: party state, items and saves |  |
| `0x59BB80` | 48 | Shop_DrawBuyDetail (ours) | `.data 0x66B2C4` | indirect call | `0x59BB60` | shop | Boot: party state, items and saves |  |
| `0x59BBB0` | 13 | Shop_DrawBuyDetail (ours) | `.data 0x66B220` | jumped to | Field_RunTaskRecords | shop | Boot: party state, items and saves |  |
| `0x59CB00` | 32 | `0x59CA00` | `.text 0x59E284` | jumped to | Field_RunTaskRecords | combat | Unlabelled |  |
| `0x59CB20` | 32 | `0x59CA00` | `.data 0x66AE40`, `.data 0x66AECC`, `.data 0x66AFCC` | jumped to | Field_RunTaskRecords | shop, combat | Unlabelled |  |
| `0x59CB40` | 32 | `0x59CA00` | `.data 0x66B538` | jumped to | Field_RunTaskRecords | combat | Unlabelled |  |
| `0x59CB60` | 48 | `0x59CA00` | `.data 0x66B554` | indirect call | `0x59CB40` | combat | Unlabelled |  |
| `0x59CBC0` | 32 | `0x59CA00` | `.data 0x66B53C` | jumped to | Field_RunTaskRecords | combat | Unlabelled |  |
| `0x59CBE0` | 48 | `0x59CA00` | `.data 0x66B568` | indirect call | `0x59CBC0` | combat | Unlabelled |  |

## The live check

After each wave's merge, in this order, on a fresh build (item 4 above):

1. The route's A/B - `analysis/validate_combat.sh` for wave A,
   `validate_shop.sh` and `validate_worldmap.sh` for wave B - Capcom's code
   against ours, captures compared. The runners kill only the game they
   started ([`world-map.md`](world-map.md) §6), so agents' headless self-tests
   and the coordinator's live runs do not collide.
2. The first-call trace of the route again (`analysis/trace_hidden_recipes.sh`,
   one route at a time is fine): every function of the wave should now be
   **absent** from `bof3x.calltrace.tsv` (the tracer never arms an owned
   function) and `inject:` should count them. What is still there is what
   the wave missed - a hidden function inside a group's extent, or a stub.
3. The attract batch (oracle, memory dump, frame hash beside an
   original-vs-original pair), with every owned function in
   `entries_logic.txt` first. Do it before 2026-09-27 or after a Restart
   (HANDOFF, Traps: 12.4 days).

## Result (2026-09-25)

All 22 groups were taken by Opus agents in their own worktrees, wave A (the
`C` groups) first and wave B (the `D` groups) after it, and merged into
`phase-3/takeover-queue-round-eight-nine` one branch at a time: after each,
the build, the group's own shadow and `BOF3X_SHADOW='*'` headless, all exit
0. **440 functions taken, 1,025 -> 1,465 ours** (`inject: 1465 ours, 0
left original`). The queue said 442; the difference is entries that turned
out not to be functions (switch cases inside hosts already ours: `0x404180`,
`0x497C30`, `0x4414E0`, `0x5916B0`, `0x5917D0`), less the handful of
functions the groups found inside their extents and took with them (CE's
`0x432C40`, CG's `0x442DD0`, CM's `0x597D10`). Negative controls: 1959
planted, 1938 refused by a mismatch count, the rest shown to change
nothing observable (or, four of DG's, refused only by a fault).

| Group | Doc | Taken | Controls | Refused | Not counted |
|---|---|--:|--:|--:|---|
| CA | [`battle_phases.md`](battle_phases.md) | 18 | 140 | 139 | 1 unobservable |
| CB | [`battle_actions.md`](battle_actions.md) | 23 | 118 | 117 | 1 unobservable |
| CC | [`battle_turn_steps.md`](battle_turn_steps.md) | 20 | 109 | 109 |  |
| CD | [`battle_result.md`](battle_result.md) | 17 | 72 | 69 | 3 unobservable |
| CE | [`battle_fx_tasks.md`](battle_fx_tasks.md) | 19 | 81 | 81 |  |
| CF | [`enemy_ai_ops.md`](enemy_ai_ops.md) | 22 | 112 | 112 |  |
| CG | [`battle_obj_states.md`](battle_obj_states.md) | 22 | 115 | 114 | 1 unobservable |
| CH | [`battle_actor_copies.md`](battle_actor_copies.md) | 16 | 92 | 90 | 2 unobservable |
| CI | [`battle_menu_states.md`](battle_menu_states.md) | 15 | 74 | 74 |  |
| CJ | [`magic_fx_reached.md`](magic_fx_reached.md) | 25 | 78 | 78 |  |
| CK | [`battle_odds.md`](battle_odds.md) | 10 | 49 | 48 | 1 unobservable |
| CL | [`battle_win_states.md`](battle_win_states.md) | 19 | 41 | 40 | 1 unobservable |
| CM | [`window_kinds.md`](window_kinds.md) | 12 | 44 | 44 |  |
| DA | [`worldmap_area.md`](worldmap_area.md) | 30 | 97 | 97 |  |
| DB | [`mode_states.md`](mode_states.md) | 20 | 107 | 104 | 3 unobservable |
| DC | [`event_leader.md`](event_leader.md) | 27 | 95 | 94 | 1 unobservable |
| DD | [`map_field_objects.md`](map_field_objects.md) | 16 | 132 | 132 |  |
| DE | [`field_hidden.md`](field_hidden.md) | 15 | 83 | 81 | 2 unobservable |
| DF | [`shop_states.md`](shop_states.md) | 25 | 104 | 104 |  |
| DG | [`shop_states2.md`](shop_states2.md) | 24 | 121 | 116 | 4 refused by a fault, 1 unobservable |
| DH | [`menu_lists.md`](menu_lists.md) | 20 | 56 | 56 |  |
| DI | [`menu_draw_helpers.md`](menu_draw_helpers.md) | 25 | 39 | 39 |  |

**The live check** (`analysis/validate_round8.sh`, then
`validate_round8_hash.sh` and `validate_round8_nosteal.sh`, one batch with
the owner away):

| Check | Result |
|---|---|
| Combat A/B, cheats as the owner plays (steal on) | 5 of 43 identical; ours steals (Pilfer, DIV-0046) where Capcom's side, with `*`, has the cheat off - the item list and the victory line follow from it |
| Combat A/B, steal off | 5 of 43 identical, the rest tile-edge pixels only (15 at most) |
| Shop A/B | 5 of 35 identical, the rest tile-edge pixels only (2..46) |
| World map A/B | 7 of 35 identical; the needle (DIV-0044) and tile-edge pixels |
| First-call traces of the hidden list | no owned function among those entered, on all three routes; 28 / 25 / 23 hidden entries still Capcom's (shop / world map / combat) - the next queue |
| Frame hash, all-original pair (`r8_orig` vs `r8_origb`) | identical on all 10,318 frames |
| Frame hash, ours (`r8_ours`) | identical but frame 0, the set-up (as since `rb1`) |
| Oracle | identical at every logged frame |

The tile-edge pixels are the class every A/B has shown since the Direct3D 11
backend (DIV-0031); round six's shop A/B was 35 of 35 before it.

What the round found on the way, each written down where it belongs:

- **The A/B original side trapped** on the English EX glyph since DIV-0052
  (the glyph guard set only by a function the `*,KEEP` side leaves
  Capcom's): DIV-0016 amended, `LoadDatFile` sets the bound too.
- **CJ's Pilfer lost DIV-0046**: the cheat patches a byte of the original
  `0x4B54F0`, which ours replaced; ours now reads the patch back (DIV-0046
  amended). DI and DH kept DIV-0041's slide bounds the same way.
- **`entries_logic.txt`**: the agents' lines, and 78 host extents that ran
  over the functions taken, consolidated by `analysis/consolidate_entries.py`;
  a comment line over the tracer's 511-character read stopped the first
  frame-hash attempt, and the script now wraps comments.
- **Latent defects of the original** the group docs describe, numbered in
  [`known-defects.md`](known-defects.md) from D59.
- **Naming questions** the groups raised and left alone (their docs say
  where): `Item_Price` `0x591C20` returns a help-line message id, not a
  price; `MoveScript_EffectState` `0x66972C` is the character-to-record
  byte table; `0x596A90` is a menu panel draw, not the window task
  (`Field_RunTaskRecords` `0x59E230` is); `0x42E2F0` is not the battle's
  frame (`0x42E370` is); `ItemMenu_CanUseSelected` reads the other way round.

## Owed after the round

- The owner's eye on a fight and on the shop under full ownership.
- `remaining-catalog.md` regenerated: the 440 are ours, and the 76 hidden
  entries the routes still enter (28 / 25 / 23 above) are the next queue,
  beside a route the owner records (`menu_screens.txt` first, then a boss).
  The groups' docs name further unowned handlers in the tables they named.
- The tables named in the round (the battle phase table, the enemy AI ops,
  the shop's two, the menu lists) added to the catalogue's `table` rule by
  their `symbols.toml` entries - nothing to do beyond naming them.

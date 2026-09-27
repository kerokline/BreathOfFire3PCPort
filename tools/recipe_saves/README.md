# tools/recipe_saves/

The saves the recipes load, one `NAME.DAT` each - copies of a game slot, made
with `python tools/recipe_saves.py import SLOT NAME`. **Game data: nothing in
this directory but this file is tracked** (`.gitignore`; CLAUDE.md rule 1).

A recipe names its save with a `# save NAME` header line. `tools/input_run.py`
puts that file into the game's slot 0, with the owner's slot 0 backed up under
`bof3/.recipe_slot0/`, and puts the owner's back as soon as the game logs the
load (or when it has exited) - so every recipe opens slot 0 and the game's
slots stay free to play in. Runs queue on that lock for the minute a load
takes; a game that is not a scripted run refuses the swap. `python
tools/recipe_saves.py list` shows the saves, the recipes that use them and
any lock; `restore` puts slot 0 back after a run that died holding it.
[`docs/input-script.md`](../../docs/input-script.md) §1a.

The saves on this machine (2026-09-26), for the record - not their contents:

| Save | Was | Recipes |
|---|---|---|
| `adult_ryu.DAT` | slot 5: adult Ryu, Lv 38, a US conversion (menu square, confirm cross, cancel triangle) | `field_menu`, `menu_screens`, `field_view`, `camera_rotate` |
| `town.DAT` | slot 3: the town of the owner's shop and world-map routes | `shop`, `shop_ab`, `backdrop_kinds`, `worldMapAndAreaTransition`, `_ab`, `worldmap_sliver` |
| `combat.DAT` | slot 0 as of 2026-09-26 07:41: the F12 field save the combat route was recorded from (2026-09-23) - if slot 0 had been re-saved since, this is that later save and the route wants re-importing | `combat`, `combat_ab` |

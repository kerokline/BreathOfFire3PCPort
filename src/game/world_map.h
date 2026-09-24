// Group 1 of the world-map wave: the map task's frame state machine, the map's
// frame (the dial, the keyboard legend, the compass needle), the sprite draw
// they share, the HUD (the region box and its label), the needle, and two
// map-view helpers (a draw item's half for this buffer; a primitive linked
// into a map-view row). docs/world-map.md section 7.
#pragma once

void WorldMap_Inject();

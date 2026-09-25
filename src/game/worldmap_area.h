// The world map's area code and the effect kinds its route runs, reached only
// through pointers: area 29's init (0x4037B0), area 33's handlers and its
// place message (0x403CE0..0x403D70), the Yraall map's place plate, HUD task,
// region box and drift layer (0x403E00..0x404330, 0x4048E0), the three state
// blocks the eleven world-map copies share (0x411310, 0x414BB0, 0x419110),
// the world-map record lookup and the three effect kinds that go through it
// (0x462A90..0x462B80), and effect kinds 6 and 0x18 (0x469BB0..0x469DB0,
// 0x46D830..0x46D850). docs/worldmap_area.md.
#pragma once

void WorldmapArea_Inject();

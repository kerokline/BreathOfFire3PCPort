// Searches of the sprite object arrays, and what uses them: originals 0x589660,
// 0x531C70, 0x57C0A0 and 0x589770.
#pragma once

void SpriteFind_Inject();

namespace sprite_find {
extern unsigned char(__cdecl* g_handle_live)(unsigned);   // defined in sprite_find.cpp (source-layout B0: promoted for a moved reader)
extern void(__cdecl* g_inherit_live)();   // defined in sprite_find.cpp (source-layout B0: promoted for a moved reader)
}  // namespace sprite_find

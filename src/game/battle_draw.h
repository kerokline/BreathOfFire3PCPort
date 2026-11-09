// Group BJ of the seventh takeover round: three Direct3D draw handlers
// (POLY_G3, LINE_G2, LINE_G3), the battle menu's item and skill lists, and the
// area palette's tint. docs/battle_draw.md.
#pragma once

void BattleDraw_Inject();

extern unsigned char g_list_title_centre;   // defined in battle_draw.cpp (named here for a moved reader: source-layout B6)

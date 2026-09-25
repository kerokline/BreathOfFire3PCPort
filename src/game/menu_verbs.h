#pragma once

#include <cstdint>

// DIVERGENCE DIV-0018: the menu's short verbs - the buttons above a menu panel
// ("Quit", "Init", "Use", "Sort", ...) - from a language overlay. See
// src/game/menu_verbs.cpp and docs/config-screen.md section 8.

// Applies a kind-8 chunk: a count of 22, then 22 NUL-terminated strings, each
// written into its 8-byte slot in BOF3.exe's .data. Aborts loudly on anything
// else.
void MenuVerbs_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// DIVERGENCE DIV-0019: applies a kind-9 chunk - a count of 7, then the battle
// command cross's seven labels, each into its 8-byte slot at 0x669D28.
void BattleCommands_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// Re-centres the button row's labels by their real width. Only with
// BOF3X_LANG set, and not for a full-width language (DIV-0056).
void MenuVerbs_Inject();

// The label draw Menu_DrawButtonRow's call at 0x57499B reaches after
// MenuVerbs_Inject: ours when DIV-0018 is on, null when it is Text_DrawAt.
// For our Menu_DrawButtonRow (src/game/menu_windows.cpp).
using MenuVerbs_LabelFn = const unsigned char* (__cdecl*)(int x, int y, int color, int count, const unsigned char* text);
MenuVerbs_LabelFn MenuVerbs_ActiveLabel();

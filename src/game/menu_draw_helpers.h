// Group DI of the eighth takeover round: the shop and menu draw helpers -
// record handlers 7 and 8 of Field_RunTaskRecords (0x59B220, 0x59CB00), the
// window kinds they dispatch to that draw a shop or battle-menu panel, and the
// slides those kinds step through. docs/menu_draw_helpers.md.
#pragma once

void MenuDrawHelpers_Inject();
extern unsigned char g_shop_yes_no_line;   // defined in menu_draw_helpers.cpp, armed by MenuDrawHelpers_Inject (named here for a moved reader: source-layout B0)

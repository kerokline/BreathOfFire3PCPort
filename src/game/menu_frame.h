// The menu's panel frame, which the PC build compiled to nothing, DIV-0011
// (docs/menu-screens.md section 3).
#pragma once

#include <cstdint>

void MenuFrame_Inject();

// What the four frame calls DIV-0011 re-aims (Config_DrawPanel 0x461778,
// Config_DrawControllerPanel 0x461A84, 0x581313 and 0x59AA98, the reserve
// lists) call in this build: Port_DroppedCall 0x4DF820 (a bare ret), or our
// Menu_DrawFrame once MenuFrame_Inject re-aimed them. A value of ours
// (docs/operand-store.md): our reimplementations of those four draws call it,
// never reading the call site.
std::uint32_t MenuFrame_Draw();

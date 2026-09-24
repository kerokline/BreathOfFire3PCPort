// The area's backdrop and the two other handlers of AreaMap_EntryHandlers
// (docs/area-backdrop.md): AreaMap_DrawBackdrop 0x571BE0 (entry 1, the sky
// gradient - widened under BOF3X_WIDE, DIV-0041), AreaMap_TextureCycle
// 0x571D30 (entry 2) and AreaMap_SlotZones 0x571E20 (entry 3); and
// WorldMap_PinSprite 0x4112A0, the world map's sprite pinned to (160, 80).
#pragma once

void AreaBackdrop_Inject();

// Save, load, the inn and the sound stream: the title and load flow
// 0x587DB0..0x588E6F, the save files 0x454770..0x454A1D, the inn and the save
// menu's pieces 0x580630..0x5809B2, the shop's price helpers
// 0x583020..0x583347, and the stream 0x587910 / 0x587A00 / 0x5A7020.
// docs/save-menu.md.
#pragma once

void SaveMenu_Inject();

namespace save_menu {
// DIV-0129's flag (Sound_StreamDone answers 1 while a stream's file is
// missing), read and put back by the quicksave's audio participant (sound.cpp):
// logic-visible, so a quick-load restores it as it was at the take.
bool StreamMissing();
void SetStreamMissing(bool missing);
}  // namespace save_menu

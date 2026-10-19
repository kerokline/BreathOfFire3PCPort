// Save, load, the inn and the sound stream: the title and load flow
// 0x587DB0..0x588E6F, the save files 0x454770..0x454A1D, the inn and the save
// menu's pieces 0x580630..0x5809B2, the shop's price helpers
// 0x583020..0x583347, and the stream 0x587910 / 0x587A00 / 0x5A7020.
// docs/save-menu.md.
#pragma once

#include <cstdint>

void SaveMenu_Inject();

// The quicksave round's draft B (docs/quicksave.md, section QM): the save list as
// BISLPS0?.DAT and bounded at sixteen, switched on by PatchBytes "SaveLists"
// (BOF3X_ORIGINAL=SaveLists keeps BISLPS??.DAT, unbounded). InjectAll, after
// every module's self-test.
void SaveLists_Arm();

namespace save_menu {

// Save_ListFiles' and Save_ReadSummaries' bodies on any pattern and tables
// (the original's entries pass Save_Directory 0x929F40 and the summaries
// 0x905BC0; the quick menu passes BISLPS1?.DAT and tables of its own).
// A table is kListEntries entries of kEntryBytes (a name at +0, NUL included,
// the size at +0x14); the summaries 16 of kSummaryBytes, +0x15 the entry's
// index or 0xFF. `bounded`: at most sixteen entries and a name under 0x14
// bytes, the rest not stored and logged (the original stores every match).
// `close`: _findclose the search (the original never does).
constexpr unsigned kListEntries = 16;
constexpr unsigned kEntryBytes = 0x18;
constexpr unsigned kSummaryBytes = 0x1C;
int ListFiles(const char* pattern, unsigned char* table, bool bounded, bool close);
// Reads one file's 0x1C summary bytes into `into`; false leaves the slot empty.
using SummaryRead = bool (*)(const char* name, unsigned char* into, void* context);
void ReadSummaries(const unsigned char* table, unsigned char* summaries, SummaryRead read, void* context);

// The title's load menu's windows (LoadMenu_Open..LoadMenu_Loaded draw them
// first): Menu_DrawBackdrop of the Config's background byte 0x903A5B,
// Menu_DrawTitleBox(0x14, 0x12, 0x118, 0x13, the window colour 0x903A5A) and
// system message `message` through Text_DrawAt at (0x1C, 0x15).
void DrawLoadWindows(unsigned message);

// SaveMenu_DrawSlots' body over any summaries, first-shown word and cursor
// word (the original's entry passes 0x905BC0, 0x8034D0 and 0x9036D4).
void DrawSlotsOver(int x, int y, unsigned highlight, const unsigned char* summaries,
                   const volatile std::uint32_t* top_word, const volatile std::uint32_t* cursor_word);

// DIV-0129's flag (Sound_StreamDone answers 1 while a stream's file is
// missing), read and put back by the quicksave's audio participant (sound.cpp):
// logic-visible, so a quick-load restores it as it was at the take.
bool StreamMissing();
void SetStreamMissing(bool missing);
}  // namespace save_menu

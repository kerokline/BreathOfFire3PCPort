// Save files: one BISLPS0<slot>.DAT per slot in the game directory
// (docs/save-files.md).
#include "game/save_io.h"

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// original 0x454870.
//
// DIVERGENCE DIV-0002: the original ends at `return 0`. Ours refreshes
// Save_Directory first. The in-game save menu rebuilds its sixteen slot
// summaries from that table without ever re-listing the directory, so in the
// original a save to a slot that had no file when the table was last filled
// disappears from the menu until the game is restarted.
//
// Faithfully kept: File_OpenWrite reports a failed fopen as success (it only
// returns -1 when all 16 slots are taken), and the File_Write result is
// discarded. Both are original defects, recorded in symbols.toml, not fixed
// here.
extern "C" int __cdecl Save_WriteFile(const char* path, unsigned size) {
    BOF3_LOG_FIRST_CALL("Save_WriteFile(%s, %u)", path, size);
    int handle = File_OpenWrite(path);
    if (handle == -1) return -1;
    File_Write(handle, Save_Staging, size);
    File_Close(handle);
    Save_ListFiles();  // DIV-0002
    return 0;
}

void SaveIo_Inject() {
    BOF3_INJECT(Save_WriteFile);
}

// Moved here by tools/move_funcs.py from save_io.cpp, save_menu.cpp (docs/source-layout-round.md).
#include "game/field/save.h"

#include <cstdint>
#include <cstring>
#include <io.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/music_seq.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/save_io.h"
#include "game/save_menu.h"
#include "game/save_menu_callees.h"
#include "game/sound_in.h"
#include "game/title_menu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/run_speed.h"
#include "input/sigma.h"

using namespace save_menu;

namespace {

unsigned char* AtSaveMenu(save_menu::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

namespace {

constexpr const char* kSavePattern = "BISLPS0?.DAT";

}  // namespace

#pragma clang attribute pop

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// 0x454820 (docs/save-files.md). File_Open(path, 0, 0), -1 on failure;
// File_Seek(offset); File_Read(size) into Save_Staging; File_Close; 0. The
// seek's and the read's results are not looked at.
extern "C" int __cdecl Save_ReadFile(const char* path, unsigned size, int offset) {
    const int handle = g.file_open(path, 0, 0);
    if (handle == -1) return -1;
    g.file_seek(handle, offset);
    g.file_read(handle, AtSaveMenu(::bof3::Img(Save_Staging_addr)), size);
    g.file_close(handle);
    return 0;
}

#pragma clang attribute pop

// original 0x454870.
//
// DIVERGENCE DIV-0002: the original ends at `return 0`. Ours refreshes
// Save_Directory first. The in-game save menu rebuilds its sixteen slot
// summaries from that table without ever re-listing the directory, so in the
// original a save to a slot that had no file when the table was last filled
// disappears from the menu until the game is restarted.
//
// Faithfully kept: the File_Write result is discarded, an original defect.
// The -1 test below was nearly dead code in the original, where File_OpenWrite
// reported a failed fopen as success; since DIV-0003 (file_io.cpp) it is the
// path a read-only game directory takes.
extern "C" int __cdecl Save_WriteFile(const char* path, unsigned size) {
    BOF3_LOG_FIRST_CALL("Save_WriteFile(%s, %u)", path, size);
    int handle = File_OpenWrite(path);
    if (handle == -1) return -1;
    File_Write(handle, Save_Staging, size);
    File_Close(handle);
    Save_ListFiles();  // DIV-0002
    return 0;
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// 0x4548B0 (docs/save-files.md): the body above on Save_Directory - with the
// original's BISLPS??.DAT (0x65289C) and no bound, or, armed, draft B's
// BISLPS0?.DAT and the bound. Kept either way, the original's defect: no
// _findclose (a search handle leaks per call).
extern "C" int __cdecl Save_ListFiles(void) {
    if (save_menu::g_lists_split != 0) return save_menu::ListFiles(kSavePattern, AtSaveMenu(::bof3::Img(Save_Directory_addr)), true, false);
    return save_menu::ListFiles(reinterpret_cast<const char*>(AtSaveMenu(::bof3::Img(at::kListPattern.v))), AtSaveMenu(::bof3::Img(Save_Directory_addr)), false, false);
}

#pragma clang attribute pop

void Save_Inject() {
    BOF3_INJECT(Save_ReadFile);
    BOF3_INJECT(Save_WriteFile);
    BOF3_INJECT(Save_ListFiles);
}

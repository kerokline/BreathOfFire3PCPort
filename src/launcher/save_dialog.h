// The save editor's window (docs/save-editor.md, docs/save-editor-spec.md): a
// DIALOGEX like the launcher's other dialogs (launcher.rc IDD_SAVEEDIT),
// opened from the Mods tab. The format is in launcher/save/.
#pragma once

#include <windows.h>

#include <string>

namespace bof3x {

// `game_dir` holds the save files (BISLPS00..0F.DAT); `cache` and `language`
// say where the names for the lists are (a language layer's FIRST.DAT).
void SaveEditorRun(HWND owner, const std::wstring& game_dir, const std::wstring& cache, const std::string& language);

}  // namespace bof3x

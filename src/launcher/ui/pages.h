// The drawn launcher's pages (docs/launcher-portable.md, stage P2): the state
// the pages share and each page's three calls. It is the Win32 window's
// DialogState, Populate and ReadBack (config_dialog.cpp) less the windows, so
// that a page writes the settings file the Win32 page writes:
//
//   Open   fills a page's boxes from the settings (Populate).
//   Page   draws it, each frame, and changes the boxes only.
//   Apply  writes the boxes back to the settings (ReadBack). An untouched
//          page writes its settings back as they were read.
//
// The settings change only in PagesApply: on Play and on Back, as in the
// Win32 window (docs/launcher-settings.md 2a "What is saved").
//
// One file for each page (page_*.cpp), which holds the boxes no other page
// reads. The boxes more than one page reads are here.
#pragma once

#include <string>
#include <vector>

#include "launcher/config.h"
#include "launcher/suites.h"

namespace bof3x::ui {

struct Pages {
    Config* cfg = nullptr;
    std::wstring game_dir;
    // The overlay languages found (ConfigLanguagesAvailable), and whether
    // "Chinese (PC script)" is offered (ConfigOriginalAvailable, DIV-0102):
    // the Language box holds it, when offered, and then the languages.
    std::vector<std::string> languages;
    bool original = true;

    // ---- Boxes more than one page reads ----
    int language = 0;      // Settings: the Language box's entry
    std::string cache;     // Data: the cache folder box's text, UTF-8, as typed
    int preset = 0;        // Settings, Preset: the Preset box's entry (kPresets' order)
    bool ruby_ui = false;  // Mods, Extras: "Show the Row above the text box on Settings"
    // Game Options: the three suites as the page last showed them, and the
    // player's choice for each: -1 untouched, 0 the PlayStation's, 1 the PSP's.
    std::vector<SuiteState> suites;
    std::vector<int> choice;
    // The Jukebox's battle-chord box: -1 untouched, 0 off, 1 on (stage P5).
    int chord = -1;
    // Raised when the language, the cache folder or the preset changes: a
    // page that shows what follows them (the row above the text, the preset's
    // list, the suites, the music sources) makes it again when this differs
    // from the number it made it at.
    unsigned stamp = 1;

    // Back applied the pages to the settings: Exit then saves them.
    bool applied = false;
    // Settings' Options...: SatPixie's options are shown in place of the tabs' page.
    bool satpixie_open = false;
    // A Controls cell was used: the bind capture is shown in place of the tabs' page.
    bool capture_open = false;
    // The Sources page (stage P4) is shown in place of the title page or the
    // tabs' page, from Data's Sources... or before the window (main.cpp: a
    // first run, a game folder to set up in place, a rebuild). When it closes,
    // `sources_closed` is raised for the window, with `sources_built` true
    // when a build finished (SourcesDialogRun's answer).
    bool sources_open = false, sources_closed = false, sources_built = false;
    // Raised when the Mods page added, built or removed a mod
    // (kMsgModsChanged): Settings' Row above the text box is made again.
    unsigned mods_stamp = 0;

    // ---- The Jukebox (stage P5) ----
    // Shown in place of the title page, from its Jukebox button, until its Close.
    bool jukebox_open = false;
    // ---- The save editor (stage P5) ----
    // Mods' Save Editor...: the editor is shown in place of the tabs' page
    // until its Close (the Win32 launcher shows a window of its own).
    bool saves_open = false;
};

// The Win32 pages are laid out in dialog units of Segoe UI 9: 1.75 of the
// picture's pixels across and 1.875 down. A page keeps launcher.rc's numbers.
constexpr float Dx(float dialog_units) { return dialog_units * 1.75f; }
constexpr float Dy(float dialog_units) { return dialog_units * 1.875f; }

// A wide text of the settings model as the widgets' UTF-8.
std::string PagesText(const std::wstring& wide);

// Every page's Open, in Populate's order.
void PagesOpen(Pages& p, const std::wstring& game_dir, Config& cfg);
// Every page's Apply, in ApplyPages' order.
void PagesApply(Pages& p);

// The language the Language box names; empty for "Chinese (PC script)".
std::string PagesLanguageChosen(const Pages& p);
// The cache the game will be given: BOF3X_CACHE when the environment has it,
// else the cache box as it stands, trimmed, else the home's own; and where
// the game finds layers in it (none under BOF3X_CACHE_DATA=0).
std::wstring PagesCacheRoot(const Pages& p);
std::wstring PagesDataRoot(const Pages& p);
// Whether the preset in the Preset box sets this ini key when the game
// starts ("display", "background", "music"): its control is then grey.
bool PagesPresetSets(Pages& p, const char* key);
// That preset's plan for the Language box's language on this install, as the
// start will make it (ConfigPresetPlan); made again when Pages::stamp moves.
const PresetPlan& PagesPresetPlan(Pages& p);
// The sentence a control greyed by the preset shows.
extern const char kPresetGreyReason[];

// ---- The pages -----------------------------------------------------------------

// Settings and its Preset sub-page (page_settings.cpp).
void SettingsOpen(Pages& p);
void SettingsApply(Pages& p);
void SettingsPage(Pages& p);
void PresetPage(Pages& p);
// Mods, Extras, Page 1: the launcher's extras (page_extras.cpp).
void ExtrasOpen(Pages& p);
void ExtrasApply(Pages& p);
void ExtrasPage(Pages& p);
// Mods, Extras, Page 2: New Game's two screens (page_newgame.cpp).
void NewGameOpen(Pages& p);
void NewGameApply(Pages& p);
void NewGamePage(Pages& p);
// Music (page_music.cpp).
void MusicOpen(Pages& p);
void MusicApply(Pages& p);
void MusicPage(Pages& p);
// Game Options: Graphics and audio, and Data - the three suites (page_versions.cpp).
void VersionsOpen(Pages& p);
void VersionsApply(Pages& p);
void VersionsGraphicsPage(Pages& p);
void VersionsDataPage(Pages& p);
// Game Options, Cheats, two pages (page_cheats.cpp).
void CheatsOpen(Pages& p);
void CheatsApply(Pages& p);
void CheatsPage1(Pages& p);
void CheatsPage2(Pages& p);
// Game Options, Solo character (page_solo.cpp).
void SoloOpen(Pages& p);
void SoloApply(Pages& p);
void SoloPage(Pages& p);
// Data: the cache folder (page_data.cpp).
void DataOpen(Pages& p);
void DataApply(Pages& p);
void DataPage(Pages& p);
// SatPixie's options, shown from Settings' Options... (page_satpixie.cpp).
void SatpixieOpen(Pages& p);
void SatpixiePage(Pages& p);
// Controls: Directions and buttons, Shoulders, Start and Select, Function
// keys, and the bind capture over a cell of any of them (page_controls.cpp).
void ControlsOpen(Pages& p);
void ControlsApply(Pages& p);
void ControlsPage1(Pages& p);
void ControlsPage2(Pages& p);
void FnKeysPage(Pages& p);
void CapturePage(Pages& p);
// The key the Function keys page gives the window action, as the page shows
// it now (a key number, -1 for none): the Settings page's Display box names it
// (config_dialog.cpp DisplayItems).
int PagesWindowKey(const Pages& p);

// The Sources page (page_sources.cpp; sources_dialog.cpp's IDD_SOURCES): the
// player's copies of the game, checked and built by tools/importer.py.
// SourcesBegin opens it (SourcesDialogRun's arguments): `first_run`, no
// install was found; `rebuild`, the player agreed to rebuild a stale install,
// so the build starts at once and the page closes when it is done; `folder`
// (with `first_run`), a game folder with no image to set up in place
// (docs/exe-retire-round.md X3). Pages::cfg must be set: Locate ffmpeg.exe...
// writes cfg->ffmpeg at once, and the caller saves it.
void SourcesBegin(Pages& p, bool first_run, bool rebuild, const std::wstring& folder);
void SourcesPage(Pages& p);
// The page's tool, each frame, shown or not: its lines and its end.
void SourcesPoll(Pages& p);
// The page as text, for a check's `report sources`: the intro, the folders,
// the list, the status, the progress and the log.
std::string SourcesText();

// The home's install against this release (sources_dialog.h SourcesCheckStale,
// docs/install-staleness.md): `importer.py stale --home H`, run once for each
// launcher and importer the install meets.
struct StaleCheck {
    bool stale = false;                  // a piece would be made differently: offer the rebuild
    std::vector<std::string> pieces;     // each, as the player reads it (the step and why)
    std::string error;                   // the check could not run: said, and the game starts
};
StaleCheck SourcesCheckStale();
// The question SourcesAskRebuild asked, its title and its text.
extern const char kRebuildTitle[];
std::string SourcesRebuildText(const StaleCheck& check);

// Mods, Data: the player's mods as tools/mods.py lists them (page_mods.cpp;
// sources_dialog.cpp's IDD_PAGE_MODS). Open reads mods_off=; the list is
// asked for the first time the page is shown. A box changed writes mods_off=
// into the settings at once, as the Win32 window does (kMsgModsOffChanged).
void ModsOpen(Pages& p);
void ModsPage(Pages& p);
void ModsPoll(Pages& p);
// The page as text, for a check's `report mods`.
std::string ModsText();

// The Jukebox (page_jukebox.cpp; music_player.cpp's IDD_MUSIC): the songs and
// recorded streams of each source, played through jukebox_play.h. Begin lists
// them, opens the sound device and shows the page (MusicPlayerRun); its Close
// stops the sound. Its battle-chord box sets Pages::chord, which the Game
// Options page's Apply writes into opt= with the suites (DIV-0113).
void JukeboxBegin(Pages& p);
void JukeboxPage(Pages& p);
// The page as text, for a check's `report jukebox`: the sources, the note,
// the song box's entries (grey ones with why), the status and the chord box.
std::string JukeboxText();
// A check (main.cpp --check): no sound out whatever the environment, and with
// `samples` the samples handed to the sound written to that file (--samples).
void JukeboxCheck(const std::string& samples);
// ---- The save editor (stage P5; page_saves.cpp, save_dialog.cpp's IDD_SAVEEDIT) ----
// SavesBegin reads the install's names and tables for the language the game
// plays now (SaveEditorRun) from Pages::game_dir and Pages::cfg as saved, opens
// the first good slot and sets Pages::saves_open; the page's Close clears it.
void SavesBegin(Pages& p);
void SavesPage(Pages& p);
// The editor as text, for a check's `report saves`: the two file boxes, the
// sections, the records, the fields, the value box, the notes, and each
// message and question with its answer.
std::string SavesText();
// A check's stand-ins for the game's marks (main.cpp game-program,
// game-window): the program's file name and the window class the editor asks
// the seam about before it writes a PC save. Null keeps one; "" asks for none.
void SavesGameMarks(const std::string* program, const std::string* window_class);

// A page that is not in this launcher yet: its sentence.
void PageNotYet();

}  // namespace bof3x::ui

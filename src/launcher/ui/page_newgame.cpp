// Mods, Extras, Page 2: New Game's two screens (launcher.rc IDD_PAGE_EXTRA2;
// docs/new-game-round.md 5 and 4c-LT). config_dialog.cpp's NewGameRefresh,
// NewGamePopulate and ApplyPages' ConfigNewGameOffEdit, less the windows. The
// texts are the Win32 page's, word for word.
//
// Two boxes over code names in off=: the name entry (NewGameNameEntry and
// CommuNameGrid) and the options step (NewGameOptions), both on by default.
// Each box shows what off= says and is written back only once clicked
// (DialogState::names_box, options_box: -1 untouched). Grey, with the reason,
// as ConfigNewGameState decides for the language and the preset chosen now
// and the cache in the box: made again when Pages::stamp moves.
#include "launcher/ui/pages.h"

#include "launcher/ui/ui.h"

namespace bof3x::ui {

namespace {

struct Boxes {
    bool names = false, options = false;   // the boxes as shown
    int names_box = -1, options_box = -1;  // -1 untouched, else the box as clicked
    NewGameState state;                    // NewGameRefresh's, made at this stamp
    std::string names_why, options_why, why;
    unsigned stamp = 0;
};
Boxes g_boxes;

// NewGameRefresh: whether each box can be changed, and why not.
void NewGameRefresh(Pages& p) {
    Boxes& b = g_boxes;
    if (b.stamp == p.stamp) return;
    b.stamp = p.stamp;
    Config chosen = *p.cfg;
    chosen.language = PagesLanguageChosen(p);
    if (chosen.language.empty()) chosen.language = kLanguageOriginal;
    const std::string language = ConfigLanguagePlayed(p.game_dir, chosen);
    const PresetPlan& plan = PagesPresetPlan(p);   // the same preset and language played
    b.state = ConfigNewGameState(*p.cfg, language, ConfigLayerHasNameGrid(p.game_dir, language, PagesDataRoot(p)),
                                 plan.off);
    std::wstring why = b.state.name_entry_why;
    if (!b.state.options_why.empty() && b.state.options_why != why)
        why += (why.empty() ? L"" : L"\n") + b.state.options_why;
    b.names_why = PagesText(b.state.name_entry_why);
    b.options_why = PagesText(b.state.options_why);
    b.why = PagesText(why);
}

}  // namespace

// NewGamePopulate: the boxes as off= says.
void NewGameOpen(Pages& p) {
    Boxes& b = g_boxes;
    b = Boxes{};
    const NewGameState s = ConfigNewGameState(*p.cfg, std::string(), false, std::string());
    b.names = s.name_entry;
    b.options = s.options;
}

// off= as read unless a box was clicked (ConfigNewGameOffEdit).
void NewGameApply(Pages& p) {
    p.cfg->off = ConfigNewGameOffEdit(p.cfg->off, g_boxes.names_box, g_boxes.options_box);
}

void NewGamePage(Pages& p) {
    Boxes& b = g_boxes;
    NewGameRefresh(p);
    const NewGameState& s = b.state;

    GroupBegin("New Game", 0, 0, Dx(336), Dy(162));
    Place(Dx(7), Dy(14));
    if (!s.name_entry_enabled) Grey(b.names_why.c_str());
    if (Check("Show the name entry screen at New Game and in the faerie village##names", &b.names))
        b.names_box = b.names ? 1 : 0;
    Place(Dx(19), Dy(27));
    Size(Dx(312));
    if (!s.name_entry_enabled) Grey(b.names_why.c_str());
    Label("You select the letters of a name on a grid, as in the PlayStation version. At New Game, you give a name to "
          "the hero. In the faerie village, you can change a name. Off: New Game does not ask for a name, and a name "
          "in the faerie village does not change.");

    Place(Dx(7), Dy(63));
    if (!s.options_enabled) Grey(b.options_why.c_str());
    if (Check("Show the options screen at New Game, before the game starts##options", &b.options))
        b.options_box = b.options ? 1 : 0;
    Place(Dx(19), Dy(76));
    Size(Dx(312));
    if (!s.options_enabled) Grey(b.options_why.c_str());
    Label("As in the PlayStation version. You can also change these settings from the title menu. Off: New Game "
          "starts the game immediately.");

    Place(Dx(7), Dy(100));
    Size(Dx(324));
    Label(b.why.c_str());
    GroupEnd();
}

}  // namespace bof3x::ui

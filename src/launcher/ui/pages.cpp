// launcher/ui/pages.h: the state the pages share, and the order they are
// filled and read back in.
#include "launcher/ui/pages.h"

#include "common/home.h"
#include "launcher/ui/ui.h"
#include "os/env.h"
#include "os/text.h"

#include <iterator>

namespace bof3x::ui {

const char kPresetGreyReason[] = "The preset (Preset page) sets this setting when the game starts.";

namespace {

// The preset's plan for the Preset box and the Language box as they stand,
// made again when Pages::stamp moves.
struct PlanAt {
    unsigned stamp = 0;
    PresetPlan plan;
};
PlanAt g_plan;

std::wstring Trimmed(const std::wstring& text) {
    const std::size_t b = text.find_first_not_of(L" \t"), e = text.find_last_not_of(L" \t");
    return b == std::wstring::npos ? std::wstring() : text.substr(b, e - b + 1);
}

}  // namespace

std::string PagesText(const std::wstring& wide) { return bof3::os::ToUtf8(wide.c_str()); }

std::string PagesLanguageChosen(const Pages& p) {
    const int lang = p.language + (p.original ? 0 : 1);
    return lang >= 1 && static_cast<std::size_t>(lang) <= p.languages.size()
               ? p.languages[static_cast<std::size_t>(lang) - 1]
               : std::string();
}

std::wstring PagesCacheRoot(const Pages& p) {
    wchar_t root[1024];
    const std::uint32_t n = bof3::os::GetEnvW(L"BOF3X_CACHE", root, 1024);
    std::wstring w;
    if (n > 0 && n < 1024) {
        w.assign(root, n);
    } else {
        w = Trimmed(bof3::os::ToWide(p.cache.c_str()));   // as ConfigLoad trims a line
        if (w.empty()) w = HomeCacheDefault();             // the home's own, as ConfigCacheRoot
    }
    while (w.size() > 1 && (w.back() == L'\\' || w.back() == L'/')) w.pop_back();
    return w;
}

std::wstring PagesDataRoot(const Pages& p) {
    wchar_t text[4];
    const std::uint32_t m = bof3::os::GetEnvW(L"BOF3X_CACHE_DATA", text, 4);
    if (m == 1 && text[0] == L'0') return std::wstring();
    return PagesCacheRoot(p);
}

bool PagesPresetSets(Pages& p, const char* key) { return PresetOverrides(PagesPresetPlan(p), key); }

const PresetPlan& PagesPresetPlan(Pages& p) {
    if (g_plan.stamp != p.stamp) {
        const int sel = p.preset;
        const Preset preset = kPresets[sel >= 0 && static_cast<std::size_t>(sel) < std::size(kPresets) ? sel : 0].id;
        Config chosen = *p.cfg;
        chosen.language = PagesLanguageChosen(p);
        if (chosen.language.empty()) chosen.language = kLanguageOriginal;
        g_plan.plan = ConfigPresetPlan(p.game_dir, *p.cfg, preset, ConfigLanguagePlayed(p.game_dir, chosen));
        g_plan.stamp = p.stamp;
    }
    return g_plan.plan;
}

void PagesOpen(Pages& p, const std::wstring& game_dir, Config& cfg) {
    p = Pages{};
    p.cfg = &cfg;
    p.game_dir = game_dir;
    p.languages = ConfigLanguagesAvailable(game_dir, ConfigCacheDataRoot(cfg));
    p.original = ConfigOriginalAvailable(game_dir, ConfigCacheDataRoot(cfg));   // DIV-0102
    g_plan = PlanAt{};
    // Populate's order (config_dialog.cpp): the cache box first, since what
    // an install holds is read from the cache it names.
    DataOpen(p);
    SettingsOpen(p);
    ExtrasOpen(p);
    MusicOpen(p);
    VersionsOpen(p);
    CheatsOpen(p);
    SoloOpen(p);
    NewGameOpen(p);
    // The Controls pages' cells, then the Function keys page's table (with
    // the red cells of all three), as the Win32 window's WM_INITDIALOG.
    ControlsOpen(p);
    SatpixieOpen(p);
    // The Mods page's mods_off=, sent once the window is filled (kMsgModsOff).
    ModsOpen(p);
}

void PagesApply(Pages& p) {
    // ApplyPages' order (config_dialog.cpp): the cache, then ReadBack - the
    // settings, the extras, the music source read against opt= as it was
    // read, then the suites' edit of opt= - then the cheats, the solo
    // character, New Game's screens, the bindings and the function keys.
    DataApply(p);
    SettingsApply(p);
    ExtrasApply(p);
    MusicApply(p);
    VersionsApply(p);
    CheatsApply(p);
    SoloApply(p);
    NewGameApply(p);
    ControlsApply(p);
    p.applied = true;
    ++p.stamp;   // the settings moved: what a page made from them is made again
}

void PageNotYet() {
    Place(0, 4);
    Size(614);
    Label("This page is not in this launcher yet. To change these settings, use bof3x-launcher.");
}

}  // namespace bof3x::ui

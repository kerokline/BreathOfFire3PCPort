// Settings and its Preset sub-page (launcher.rc IDD_PAGE_SETTINGS,
// IDD_PAGE_PRESET; docs/launcher-settings.md 2a and 3.3). config_dialog.cpp's
// Populate, ReadBack, RubyRefresh, RubyPicked and PresetRefresh, less the
// windows. The texts are the Win32 pages', word for word.
#include "launcher/ui/pages.h"

#include "common/home.h"
#include "input/bindings.h"
#include "launcher/ledger_registry.h"
#include "launcher/ui/ui.h"

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace bof3x::ui {

namespace {

struct Boxes {
    int look = 0, display = 0, wide = 0;
    bool snap = false, interp = false, background = false, show = true;
    // The row above the text (docs/mods.md 5): the box's entries as last
    // made, the entry shown, and the player's pick - written as ruby= only
    // once the box was used, so an untouched page writes ruby= as it was read.
    std::vector<RubyChoice> ruby;
    std::vector<std::string> ruby_items;
    std::vector<bool> ruby_usable;
    int ruby_index = 0;
    std::string ruby_pick;
    bool ruby_touched = false;
    unsigned ruby_stamp = 0;
    unsigned ruby_mods = 0;   // Pages::mods_stamp it was made at: the Mods page changed the mods (kMsgModsChanged)
    // The Preset page's list, made at this stamp.
    std::string preset_list;
    unsigned preset_stamp = 0;
};
Boxes g_boxes;

const wchar_t* LanguageLabel(const std::string& code) {
    for (const LanguageInfo& lang : kLanguages)
        if (code == lang.code) return lang.label;
    return L"?";
}

// None; each installed reading-row mod; each other held language. Every
// entry is listed, and one this install cannot play is grey with its reason
// (the owner's rule, 2026-10-10). Made again when the language or the cache
// folder changes, and when the Mods page added, built or removed a mod.
void RubyRefresh(Pages& p) {
    Boxes& b = g_boxes;
    if (b.ruby_stamp == p.stamp && b.ruby_mods == p.mods_stamp) return;
    b.ruby_stamp = p.stamp;
    b.ruby_mods = p.mods_stamp;
    b.ruby = ConfigRubyChoices(p.game_dir, PagesLanguageChosen(p), PagesDataRoot(p), PagesCacheRoot(p), HomeDir());
    const std::string want = b.ruby_touched ? b.ruby_pick : p.cfg->ruby;
    int sel = -1;
    for (std::size_t i = 0; i < b.ruby.size(); ++i)
        if (b.ruby[i].id == want) sel = static_cast<int>(i);
    if (sel < 0 && !want.empty()) {   // bof3x.ini's value, kept and shown as what it is
        b.ruby.push_back({want, ConfigFromUtf8(want) + L" (bof3x.ini's ruby=)", L"not in this install for this language"});
        sel = static_cast<int>(b.ruby.size()) - 1;
    }
    b.ruby_items.clear();
    b.ruby_usable.clear();
    for (const RubyChoice& c : b.ruby) {
        b.ruby_items.push_back(PagesText(c.why.empty() ? c.label : c.label + L" - " + c.why));
        b.ruby_usable.push_back(c.why.empty());
    }
    b.ruby_index = sel < 0 ? 0 : sel;
}

std::string PresetTitle(const ledger::Entry& e) {
    std::string t(e.title);
    t.erase(std::remove(t.begin(), t.end(), '`'), t.end());   // the ledger's code marks
    return t;
}

std::string PresetLine(const ledger::Entry& e) {
    char id[16];
    std::snprintf(id, sizeof id, "DIV-%04u  ", static_cast<unsigned>(e.id));
    return std::string("  ") + id + PresetTitle(e);
}

// The controls of the ini keys a preset can set today, as the list names them.
struct PresetControl {
    const char* key;
    const char* label;
};
constexpr PresetControl kPresetControls[] = {
    {"display", "Display"},
    {"background", "Keep the game running when its window is not active"},
    {"music", "Music (Music page)"},
};

// The Preset page's read-only list of what the preset changes when the game
// starts (config_dialog.cpp PresetRefresh, the same sections and sentences).
void PresetRefresh(Pages& p) {
    Boxes& b = g_boxes;
    if (b.preset_stamp == p.stamp) return;
    b.preset_stamp = p.stamp;
    const PresetPlan& plan = PagesPresetPlan(p);
    const std::string& language = plan.language;
    std::string out = "For the language that you selected: ";
    out += language == kLanguageOriginal ? "Chinese (PC script)" : PagesText(LanguageLabel(language));
    out += ".\n";
    std::vector<const char*> greyed;
    for (const PresetControl& c : kPresetControls)
        if (PresetOverrides(plan, c.key)) greyed.push_back(c.label);
    if (plan.items.empty()) {
        out += "\nThis preset turns off no change.\n";
        b.preset_list = out;
        return;
    }
    if (!greyed.empty()) {
        out += "\nThe preset sets these settings in place of yours (they are grey):";
        for (std::size_t i = 0; i < greyed.size(); ++i) out += (i ? ", " : " ") + std::string(greyed[i]);
        out += ".\n";
    }

    std::string off, here, kept, cannot, also;
    int n_off = 0, n_here = 0, n_kept = 0, n_cannot = 0, n_also = 0, n_lang = 0, n_skip = 0, n_never = 0;
    for (const PresetPlan::Item& item : plan.items) {
        const ledger::Entry& e = *item.entry;
        switch (item.action) {
        case PresetAction::kOff: {
            const int count = static_cast<int>(ledger::SwitchesEnd(e) - ledger::SwitchesBegin(e));
            const int left = count - static_cast<int>(item.not_here.size() + item.kept.size());
            std::string why;
            for (const auto& [sw, reason] : item.not_here) why += (why.empty() ? " - " : "; ") + sw.substr(4) + ": " + reason;
            for (const auto& [sw, reason] : item.kept) why += (why.empty() ? " - " : "; ") + sw.substr(4) + ": " + reason;
            if (left == 0 && !item.not_here.empty()) {
                here += PresetLine(e) + why + "\n";
                ++n_here;
            } else if (left == 0) {
                kept += PresetLine(e) + why + "\n";
                ++n_kept;
            } else {
                std::string sw;
                for (const ledger::Switch* s = ledger::SwitchesBegin(e); s != ledger::SwitchesEnd(e); ++s)
                    if (s->form == ledger::SwitchForm::Ini || s->form == ledger::SwitchForm::Env)
                        sw += (sw.empty() ? " (" : ", ") + std::string(s->text.substr(4));
                if (!sw.empty()) sw += ")";
                off += PresetLine(e) + sw + why + "\n";
                ++n_off;
            }
            break;
        }
        case PresetAction::kCannot: cannot += PresetLine(e) + "\n"; ++n_cannot; break;
        case PresetAction::kAlsoAvailable: also += PresetLine(e) + "\n"; ++n_also; break;
        case PresetAction::kLangGoverned: ++n_lang; break;
        case PresetAction::kSkippedLanguage: ++n_skip; break;
        case PresetAction::kNever: ++n_never; break;
        default: break;
        }
    }
    auto section = [&out](const char* head, int n, const std::string& lines) {
        if (n == 0) return;
        out += "\n";
        out += head;
        out += " (" + std::to_string(n) + "):\n";
        out += lines;
    };
    section("Turned off when the game starts", n_off, off);
    section("Not on this install: the preset cannot turn these off here", n_here, here);
    section("Kept: a setting of yours or of the environment controls these", n_kept, kept);
    section("This build cannot turn these off", n_cannot, cannot);
    section("Also available: the preset keeps these on, but they are off. Turn them on yourself", n_also, also);
    out += "\nNot changed: " + std::to_string(n_lang) +
           " follow the language that you select (no preset changes the language), " + std::to_string(n_skip) +
           " are not used with this language, " + std::to_string(n_never) +
           " must stay on (Forced, or they cannot go back).\n";
    b.preset_list = out;
}

}  // namespace

void SettingsOpen(Pages& p) {
    const Config& cfg = *p.cfg;
    Boxes& b = g_boxes;
    b = Boxes{};
    p.language = 0;
    for (std::size_t i = 0; i < p.languages.size(); ++i)
        if (p.languages[i] == cfg.language) p.language = static_cast<int>(i) + (p.original ? 1 : 0);
    b.look = cfg.satpixie ? 2 : cfg.filter == Filter::kPoint ? 1 : 0;
    b.display = cfg.display == Display::kWindowed ? 1 : 0;
    b.background = cfg.background;
    b.wide = cfg.wide == 1 || cfg.wide == 2 ? cfg.wide : 0;   // DIV-0041: the box's entry is the ini's wide=
    b.snap = cfg.snap;
    b.interp = cfg.interp;   // DIV-0230
    b.show = cfg.show_launcher;
    p.preset = 0;
    for (std::size_t i = 0; i < std::size(kPresets); ++i)
        if (kPresets[i].id == cfg.preset) p.preset = static_cast<int>(i);
}

// The inverse of SettingsOpen. Every look but the first takes the point
// filter (the CRT look is "over the point filter", config.h).
void SettingsApply(Pages& p) {
    Config& cfg = *p.cfg;
    const Boxes& b = g_boxes;
    const std::string language = PagesLanguageChosen(p);
    cfg.language = language.empty() ? kLanguageOriginal : language;
    cfg.language_named = true;   // the box's choice (DIV-0102)
    cfg.filter = b.look >= 1 ? Filter::kPoint : Filter::kLinear;
    cfg.satpixie = b.look == 2;
    cfg.display = b.display == 1 ? Display::kWindowed : Display::kFullscreen;
    cfg.snap = b.snap;
    cfg.interp = b.interp;
    cfg.background = b.background;
    cfg.wide = b.wide == 1 || b.wide == 2 ? b.wide : 0;
    cfg.show_launcher = b.show;
    if (b.ruby_touched) cfg.ruby = b.ruby_pick;   // untouched: ruby= as read
    if (p.preset >= 0 && static_cast<std::size_t>(p.preset) < std::size(kPresets)) cfg.preset = kPresets[p.preset].id;
}

void SettingsPage(Pages& p) {
    Boxes& b = g_boxes;
    RubyRefresh(p);
    // The row above the text is shown only with Mods, Extras' toggle on
    // (ruby_ui=, the owner's call 2026-10-22). Hidden, every control below it
    // moves up by its row, 16 dialog units, and the box shortens by as much.
    const float up = p.ruby_ui ? 0.0f : 16.0f;
    GroupBegin("Settings", 0, 0, Dx(296), Dy(164 - up));

    Place(Dx(9), Dy(17));
    Label("Language:");
    std::vector<std::string> languages;
    if (p.original) languages.push_back("Chinese (PC script)");
    for (const std::string& code : p.languages) languages.push_back(PagesText(LanguageLabel(code)));
    Place(Dx(77), Dy(15));
    Size(Dx(186));
    if (Combo("language", &p.language, languages)) ++p.stamp;   // the row, the preset's list and the suites follow it
    // DIV-0102: a disc-only install has no Chinese text to offer. Offering an
    // option that cannot work is worse than explaining why it is missing.
    const char* note = !p.original ? (p.languages.empty() ? "Chinese is only in the PC version. No other language is built "
                                                            "(importer.py --lang)."
                                                          : "Chinese (PC script) is only in the PC version.")
                       : p.languages.empty() ? "No language files in DAT\\. Build them with tools/loc_build.py."
                                             : "";
    Place(Dx(77), Dy(30));
    Label(note);

    if (p.ruby_ui) {
        Place(Dx(9), Dy(42));
        Label("Row above the text:");
        Place(Dx(77), Dy(40));
        Size(Dx(186));
        if (ComboGreyed("ruby", &b.ruby_index, b.ruby_items, b.ruby_usable)) {
            b.ruby_touched = true;
            b.ruby_pick = b.ruby[static_cast<std::size_t>(b.ruby_index)].id;
        }
    }

    Place(Dx(9), Dy(58 - up));
    Label("Look:");
    Place(Dx(77), Dy(56 - up));
    Size(Dx(132));
    Combo("look", &b.look,
          {"Smooth - bilinear (original)", "Sharp - point (no blur)", "CRT effect (SatPixie)"});
    Place(Dx(213), Dy(55 - up));
    Size(Dx(50), 23);
    if (b.look != 2) Grey("Only the CRT effect has options.");
    if (Button("Options...")) p.satpixie_open = true;

    // The Display box names the key that changes the window (the Function
    // keys page's window action, as that page shows it now), or no key when
    // it has none (config_dialog.cpp DisplayItems).
    Place(Dx(9), Dy(74 - up));
    Label("Display:");
    const int key = PagesWindowKey(p);
    std::vector<std::string> displays = {"Full screen", "Window"};
    if (key >= 0) {
        const std::string name = input::KeyName(key);
        displays = {"Full screen (" + name + " changes to a window)", "Window (" + name + " changes to full screen)"};
    }
    Place(Dx(77), Dy(72 - up));
    Size(Dx(186));
    if (PagesPresetSets(p, "display")) Grey(kPresetGreyReason);
    Combo("display", &b.display, displays);

    // DIV-0041: off, 16:9 or 21:9 (wide= 0, 1, 2; docs/ultrawide-frame.md).
    Place(Dx(9), Dy(90 - up));
    Label("Widescreen:");
    Place(Dx(77), Dy(88 - up));
    Size(Dx(186));
    Combo("wide", &b.wide, {"Off - 4:3, 320 x 240 (original)", "16:9 - 426 x 240", "21:9 - 560 x 240"});

    Place(Dx(7), Dy(104 - up));
    Check("Integer scaling (2x, 3x, 4x ...)", &b.snap);
    Place(Dx(7), Dy(115 - up));
    Check("Smooth motion: 60 frames each second (experimental)", &b.interp);
    Place(Dx(7), Dy(126 - up));
    if (PagesPresetSets(p, "background")) Grey(kPresetGreyReason);
    Check("Keep the game running when its window is not active", &b.background);
    Place(Dx(7), Dy(137 - up));
    Check("Show this window at start", &b.show);
    Place(Dx(19), Dy(147 - up));
    Size(Dx(272));
    Label("Off: the game starts immediately.\nTo show this window again, start bof3x-launcher --config.");
    GroupEnd();

    // Why a setting of this page is grey.
    if (PagesPresetSets(p, "display") || PagesPresetSets(p, "background")) {
        Place(0, Dy(166 - up));
        Label("A grey setting: the preset (Preset page) sets it when the game starts.");
    }
}

void PresetPage(Pages& p) {
    PresetRefresh(p);
    GroupBegin("Preset", 0, 0, Dx(346), Dy(166));
    Place(Dx(9), Dy(17));
    Label("Preset:");
    std::vector<std::string> presets;
    for (const PresetInfo& info : kPresets) presets.push_back(PagesText(info.label));
    Place(Dx(59), Dy(15));
    Size(Dx(236));
    if (Combo("preset", &p.preset, presets)) ++p.stamp;
    const int sel = p.preset >= 0 && static_cast<std::size_t>(p.preset) < std::size(kPresets) ? p.preset : 0;
    Place(Dx(9), Dy(32));
    Size(Dx(330));
    Label(PagesText(kPresets[sel].note).c_str());
    Place(Dx(9), Dy(52));
    Size(Dx(330));
    Label("What the preset changes when the game starts. Your settings in bof3x.ini do not change.");
    Place(Dx(7), Dy(63));
    Size(Dx(332), Dy(98));
    ReadText("list", g_boxes.preset_list);
    GroupEnd();
}

}  // namespace bof3x::ui

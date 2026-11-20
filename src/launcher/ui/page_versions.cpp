// Game Options: Graphics and audio, and Data - the three suites (launcher.rc
// IDD_PAGE_VERSIONS, IDD_PAGE_VERSIONS2; docs/launcher-suites.md).
// config_dialog.cpp's SuitesRefresh, SuiteRadioClicked and the suites' part of
// ReadBack, less the windows. The texts are the Win32 pages', word for word.
//
// Graphics, audio and data, each the PlayStation's version or the PSP's
// (recipes/suites.toml). Every suite is shown whatever the install; a side the
// install cannot play is grey with the discs that would enable it, and on a
// PSP base it is the PlayStation's side that is grey. A member is grey when
// choosing its suite cannot play it, and the reason line says why. The pages
// read the cache named in the Data tab's box as it stands, so another cache
// makes them again (Pages::stamp); a choice is written as opt= only in
// PagesApply (SuitesOptEdit), and an untouched page writes opt= back as read.
#include "launcher/ui/pages.h"

#include "launcher/ui/ui.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace bof3x::ui {

namespace {

// The suites' texts as last made, at this stamp.
struct Boxes {
    std::string note;                 // the line under the box's title (IDC_SUITENOTE)
    std::vector<std::string> labels;  // "Graphics:"
    std::vector<std::string> why;     // each suite's reason line (kSuiteWhy)
    unsigned stamp = 0;
};
Boxes g_boxes;

std::wstring WideAscii(const std::string& s) {
    std::wstring w;
    for (unsigned char c : s) w += static_cast<wchar_t>(c);
    return w;
}

// The suites for the install as the pages stand: the cache box, the Language
// box and opt= as read. A choice whose side is now grey is taken back.
void SuitesRefresh(Pages& p) {
    Boxes& b = g_boxes;
    if (b.stamp == p.stamp) return;
    b.stamp = p.stamp;
    const std::wstring cache = PagesCacheRoot(p), data = PagesDataRoot(p), game = p.game_dir;
    SuiteInputs in;
    in.manifest = ManifestRead(cache);
    // a layer's songs from the cache the music reads, under BOF3X_CACHE_DATA=0 too
    in.installed = [game, data, cache](const char* layer) { return ConfigOptInstalled(game, layer, data, cache); };
    in.language = PagesLanguageChosen(p);
    in.opt = p.cfg->opt;
    in.psp = p.cfg->psp;
    in.defaults = ConfigOptDefaultInstalled(game, data);
    p.suites = SuitesEvaluate(in);
    p.choice.resize(p.suites.size(), -1);

    std::wstring note;
    if (!in.manifest.error.empty()) {
        note = WideAscii(in.manifest.error) + L": these options are not available.";
    } else if (!in.manifest.present) {
        note = cache.empty() ? L"No cache folder is set. The PSP versions need one (Data tab)."
                             : L"The cache folder has no manifest.toml. Build the install again (Data, Sources).";
    } else {
        // the sources themselves are listed on Data, Sources
        note = std::wstring(in.manifest.base_policy == "psp" ? L"This install uses PSP data."
                                                             : L"This install uses PlayStation data.") +
               L" Select a version for each group. The items under a group are what the PSP version changes.";
    }
    b.note = PagesText(note);

    b.labels.assign(p.suites.size(), std::string());
    b.why.assign(p.suites.size(), std::string());
    for (std::size_t i = 0; i < p.suites.size(); ++i) {
        const SuiteState& s = p.suites[i];
        b.labels[i] = PagesText(std::wstring(s.info->label) + L":");
        int& choice = p.choice[i];
        if ((choice == 0 && !s.psx_enabled) || (choice == 1 && !s.psp_enabled)) choice = -1;
        const int shown = choice >= 0 ? choice : s.side == SuiteSide::kPsx ? 0 : s.side == SuiteSide::kPsp ? 1 : -1;

        // The reasons, the same reason said once for the members that share it.
        std::vector<std::pair<std::wstring, std::wstring>> notes;   // note, the members it is about
        for (const SuiteMemberState& m : s.members) {
            if (m.note.empty()) continue;
            auto it = std::find_if(notes.begin(), notes.end(), [&](const auto& n) { return n.first == m.note; });
            if (it == notes.end()) notes.emplace_back(m.note, m.info->label);
            else it->second += std::wstring(L", ") + m.info->label;
        }
        std::wstring why;
        if (!s.psx_enabled && !s.psx_why.empty()) why += s.psx_why + L" ";
        if (!s.psp_enabled && !s.psp_why.empty() && s.psp_why != s.psx_why) why += s.psp_why + L" ";
        if (shown < 0) why += L"bof3x.ini (opt=, psp=) sets part of this group. Select a version to change it. ";
        for (const auto& [n, who] : notes) why += who + L": " + n + L". ";
        if (!why.empty()) why.pop_back();
        b.why[i] = PagesText(why);
    }
}

// The member lines' places in dialog units, from the suite's label line:
// two columns, two rows on Graphics and audio, three on Data (launcher.rc).
struct MemberPlace {
    float x, dy;
};
constexpr MemberPlace kMemberPlaces[] = {{16, 11}, {138, 11}, {16, 20}, {138, 20}, {16, 29}, {138, 29}};

// Suite `i` with its label line at `y` dialog units in the group, and its
// reason line `why_y` below that line, `why_h` high.
void SuiteRows(Pages& p, std::size_t i, float y, float why_y, float why_h) {
    if (i >= p.suites.size()) return;
    const SuiteState& s = p.suites[i];
    const Boxes& b = g_boxes;
    Place(Dx(8), Dy(y));
    Label(b.labels[i].c_str());

    int& choice = p.choice[i];
    int shown = choice >= 0 ? choice : s.side == SuiteSide::kPsx ? 0 : s.side == SuiteSide::kPsp ? 1 : -1;
    const std::string id = s.info->id;
    const std::string psx_why = PagesText(s.psx_why), psp_why = PagesText(s.psp_why);
    Place(Dx(50), Dy(y - 1));
    if (!s.psx_enabled) Grey(psx_why.c_str());
    if (Radio(((s.psp_base ? "PlayStation" : "PlayStation (base)") + std::string("##") + id + "-psx").c_str(), &shown, 0))
        choice = 0;
    Place(Dx(156), Dy(y - 1));
    if (!s.psp_enabled) Grey(psp_why.c_str());
    if (Radio(((s.psp_base ? "PSP (base)" : "PSP") + std::string("##") + id + "-psp").c_str(), &shown, 1))
        choice = 1;

    // The members, grey when the PSP's version cannot play them here.
    for (std::size_t k = 0; k < s.members.size() && k < std::size(kMemberPlaces); ++k) {
        const SuiteMemberState& m = s.members[k];
        const std::string note = PagesText(m.note);
        Place(Dx(kMemberPlaces[k].x), Dy(y + kMemberPlaces[k].dy));
        if (!m.available) Grey(note.c_str());
        Label(PagesText(m.info->label).c_str());
    }
    Place(Dx(16), Dy(y + why_y));
    Size(Dx(244), Dy(why_h));
    Label(b.why[i].c_str());
}

}  // namespace

void VersionsOpen(Pages& p) {
    g_boxes = Boxes{};
    p.suites.clear();
    p.choice.assign(std::size(suites_gen::kSuites), -1);
    // Each suite's side as opt= plays it now (an empty opt= the default
    // layers, none of which is a suite's).
    SuitesRefresh(p);
}

// The suites: a choice moves its layers in opt=; none chosen, opt= as read.
// The Jukebox's chord box with them, in one edit of the line as read: a
// choice that returns to where it started writes opt= back byte for byte.
void VersionsApply(Pages& p) {
    SuitesRefresh(p);   // the suites for the cache and the language as they stand, as the Win32 page kept them
    Config& cfg = *p.cfg;
    std::vector<std::pair<std::string, bool>> extra;
    if (p.chord >= 0) extra.emplace_back(kChordLayer, p.chord == 1);
    cfg.opt = SuitesOptEdit(cfg.opt, ConfigOptDefaultInstalled(p.game_dir, PagesDataRoot(p)), p.suites, p.choice, extra);
    cfg.psp = SuitesPspEdit(cfg.psp, p.suites, p.choice);   // DIV-0108: the code members
}

void VersionsGraphicsPage(Pages& p) {
    SuitesRefresh(p);
    GroupBegin("PlayStation or PSP version", 0, 0, Dx(263), Dy(176));
    Place(Dx(8), Dy(12));
    Size(Dx(248), Dy(18));
    Label(g_boxes.note.c_str());
    SuiteRows(p, 0, 34, 30, 36);
    SuiteRows(p, 1, 104, 30, 36);
    GroupEnd();
}

void VersionsDataPage(Pages& p) {
    SuitesRefresh(p);
    GroupBegin("PlayStation or PSP version##data", 0, 0, Dx(263), Dy(104));
    SuiteRows(p, 2, 14, 39, 45);
    GroupEnd();
}

}  // namespace bof3x::ui

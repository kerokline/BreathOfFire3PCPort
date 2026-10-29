// The launcher's window: a Win32 DIALOGEX from launcher.rc, shown with
// DialogBoxParam - a title page, and Configure, whose tabs are child dialogs
// (docs/launcher-settings.md section 2a). No third-party toolkit, which is
// deliberate - the launcher stays a single dependency-free 32-bit exe, and
// nothing is vendored (CLAUDE.md rule 5, docs/LICENSING.md section 4).
#include "launcher/config_dialog.h"

#include <windows.h>

#include <commctrl.h>
#include <objbase.h>
#include <shlobj.h>
#include <wincodec.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "common/home.h"
#include "input/pad_sdl.h"
#include "launcher/ledger_registry.h"
#include "launcher/music_player.h"
#include "launcher/resource.h"
#include "launcher/sources_dialog.h"
#include "launcher/suites.h"

namespace bof3x {
// The presets' table, generated from the ledger by tools/ledger_registry.py
// (docs/ledger-fields-round.md section 5): it compiles, and it is whole.
static_assert(ledger::kEntryCount > 0 && ledger::TableIsWhole());

namespace {

// The Controls page's cells (DIV-0050, below with the page's code).
struct Cells {
    int key[input::kActionCount][2];   // DIK scancode, or -1
    int pad[input::kActionCount][2];   // PadInput index, or -1
    input::Layout layout;
    std::vector<input::KeyBinding> kept;   // multi-action keys and unnamed scancodes, as loaded
};

// Configure's tabs, in the order of their buttons (IDC_TAB0 + page).
enum Page { kPageTitle = -1, kPageSettings, kPageControls, kPageMusic, kPageGame, kPageMods, kPageData, kPageCount };

// The sub-pages, each a child dialog. A tab shows one at a time; a tab with
// more than one shows a button for each over it (IDC_SUB0 + its place among
// the tab's), so every page fits the 640 x 480 window at any display scale.
struct SubPage {
    int page, templ;
    const wchar_t* label;   // the sub-tab button's, when the tab has more than one
};
constexpr SubPage kSubPages[] = {
    // Settings and the preset (launcher/config.h; docs/launcher-settings.md
    // "preset="), on a sub-page of its own: its list does not fit beside them.
    {kPageSettings, IDD_PAGE_SETTINGS, L"Settings"},
    {kPageSettings, IDD_PAGE_PRESET, L"Preset"},
    {kPageControls, IDD_PAGE_CONTROLS, L"Directions and buttons"},
    {kPageControls, IDD_PAGE_CONTROLS2, L"Shoulders, Start and Select"},
    {kPageMusic, IDD_PAGE_MUSIC, nullptr},
    {kPageGame, IDD_PAGE_VERSIONS, L"Graphics and audio"},
    {kPageGame, IDD_PAGE_VERSIONS2, L"Data"},
    {kPageGame, IDD_PAGE_GAME, L"Cheats"},
    // The Mods tab's sub-row (the owner's call, 2026-10-22): Data, the player's
    // mods (docs/mods.md; its own procedure, ModsPageProc), and Extras, the
    // launcher's extras - today the Row above the text toggle (ruby_ui=).
    {kPageMods, IDD_PAGE_MODS, L"Data"},
    {kPageMods, IDD_PAGE_EXTRA, L"Extras"},
    {kPageData, IDD_PAGE_DATA, nullptr},
};
constexpr int kSubPageCount = static_cast<int>(std::size(kSubPages));
constexpr int kSubTabsMax = 3;   // IDC_SUB0 .. IDC_SUB0 + 2

struct DialogState {
    Config* cfg = nullptr;
    // The overlay languages found in DAT\ (ConfigLanguagesAvailable): the
    // Language box holds "Original" and then these, in this order.
    std::vector<std::string> languages;
    std::wstring game_dir;
    // The three suites as the page last showed them (SuitesRefresh), and the
    // player's choice for each: -1 untouched, 0 the PlayStation's, 1 the PSP's.
    std::vector<SuiteState> suites;
    std::vector<int> choice = std::vector<int>(std::size(suites_gen::kSuites), -1);
    // The Jukebox's battle-chord box (MusicPlayerRun): -1 untouched, 0 off, 1 on.
    int chord = -1;
    // DIV-0102: whether "Chinese (PC script)" is offered (ConfigOriginalAvailable);
    // without it the box holds the languages alone, and its note says why.
    bool original = true;
    // The row above the text (docs/mods.md 5): the box's entries as last shown
    // (RubyRefresh), and the player's pick - written as ruby= only once the box
    // was touched, so an untouched window writes ruby= back as it was read.
    std::vector<RubyChoice> ruby;
    std::string ruby_pick;
    bool ruby_touched = false;

    // The window (section 2a): the page shown, the sub-pages, and the Controls
    // and Game Options pages' edits - written to cfg only when touched, so an
    // untouched window writes bindings and cheats back as they were read.
    int page = kPageTitle;
    int sub[kPageCount] = {};             // each tab's sub-page shown, its place among the tab's
    HWND pages[kSubPageCount] = {};       // kSubPages' order
    Cells cells{};
    bool cells_touched = false, cheats_touched = false;
    // Back applied the pages to cfg: Exit then saves them (kDialogSave).
    bool applied = false;
    // The picture, 32-bit top-down pixels, and the scale it is drawn at.
    std::vector<unsigned char> picture;
    int picture_w = 0, picture_h = 0;
    double scale = 1.0;   // the display's: 1 at 100 %, 2 at 200 %
    HFONT button_font = nullptr;
};

// A control on the window or on any of its sub-pages: the ids are unique
// across launcher.rc's pages, so the code that fills and reads them back does
// not care which tab holds a control.
HWND Item(HWND dlg, int id) {
    if (HWND h = GetDlgItem(dlg, id)) return h;
    for (HWND page = FindWindowExW(dlg, nullptr, L"#32770", nullptr); page;
         page = FindWindowExW(dlg, page, L"#32770", nullptr))
        if (HWND h = GetDlgItem(page, id)) return h;
    return nullptr;
}

// The sub-page made from a template.
HWND SubPageOf(const DialogState& state, int templ) {
    for (int i = 0; i < kSubPageCount; ++i)
        if (kSubPages[i].templ == templ) return state.pages[i];
    return nullptr;
}

void SetText(HWND dlg, int id, const wchar_t* text) { SetWindowTextW(Item(dlg, id), text); }
void Check(HWND dlg, int id, bool on) { SendMessageW(Item(dlg, id), BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0); }
bool Checked(HWND dlg, int id) { return SendMessageW(Item(dlg, id), BM_GETCHECK, 0, 0) == BST_CHECKED; }
void Enable(HWND dlg, int id, bool on) { EnableWindow(Item(dlg, id), on); }

// ---- DIV-0087: the music source and the cache folder --------------------------
//
// The ini's music= and cache= lines as a hand edit has them. The Music box's
// first entry is the default, which an empty music= and music=seq both mean:
// it keeps whichever the ini said, and writes nothing new unless the box moved.
// The cache path is UTF-8, in the ini and in Config (ConfigCacheFromIni); the
// launcher hands it to the game wide, and bof3x.exe - its code page UTF-8 by
// its manifest - reads the same bytes (GetEnvironmentVariableA, music_seq.cpp
// Arm). Any folder name can be given (docs/state3-design.md section 9).

std::wstring DialogText(HWND dlg, int id) {
    const int n = GetWindowTextLengthW(Item(dlg, id));
    std::wstring w(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(Item(dlg, id), w.data(), n + 1);
    w.resize(static_cast<size_t>(n));
    return w;
}

// What the game will make of the folder, said under the box: the DLL's own
// tests (music_seq.cpp Arm), so the note never promises what the game refuses.
// The 226 is its kRootMax: MAX_PATH - 1 less the longest path it builds
// (\base\bgm\bank\ + a 15-character bank name + .DAT), counted as the DLL
// counts it - in the UTF-8 bytes it receives, up to three a character.
void CacheNote(HWND dlg) {
    std::wstring w = DialogText(dlg, IDC_CACHE);
    const size_t b = w.find_first_not_of(L" \t"), e = w.find_last_not_of(L" \t");
    w = b == std::wstring::npos ? std::wstring() : w.substr(b, e - b + 1);   // ConfigLoad trims
    const wchar_t* note = L"";
    if (w.empty()) {
        note = HomeCacheDefault().empty() ? L"No cache folder: the game plays the PC MP3 music."
                                          : L"Empty: the game uses the cache\\ folder of this install.";
    } else {
        while (w.size() > 1 && (w.back() == L'\\' || w.back() == L'/')) w.pop_back();
        const std::string a = ConfigToUtf8(w);
        const DWORD at = GetFileAttributesW(w.c_str());
        if (at == INVALID_FILE_ATTRIBUTES || !(at & FILE_ATTRIBUTE_DIRECTORY))
            note = L"This is not a folder. The game cannot start.";
        else if (a.size() > MAX_PATH - 1 - (sizeof "\\base\\bgm\\bank\\" - 1 + 15 + sizeof ".DAT" - 1))
            note = L"The path is too long (more than 226 bytes as UTF-8). The game cannot start.";
        else {
            const DWORD bg = GetFileAttributesW((w + L"\\base\\bgm").c_str());
            note = bg != INVALID_FILE_ATTRIBUTES && (bg & FILE_ATTRIBUTE_DIRECTORY)
                       ? L"Has base\\bgm: the game plays the disc music."
                       : L"No base\\bgm: the game plays the PC MP3 music.";
        }
    }
    SetText(dlg, IDC_CACHENOTE, note);
}

void BrowseCache(HWND dlg) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    BROWSEINFOW bi{};
    bi.hwndOwner = dlg;
    bi.lpszTitle = L"Select the cache folder (it contains base and manifest.toml)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    if (PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi)) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            SetText(dlg, IDC_CACHE, path);
            CacheNote(dlg);
        }
        CoTaskMemFree(pidl);
    }
    if (SUCCEEDED(co)) CoUninitialize();
}

// ---- The three suites (docs/launcher-suites.md) ----------------------------------
//
// Graphics, audio and data, each the PlayStation's version or the PSP's
// (recipes/suites.toml). Every suite is shown whatever the install; a side the
// install cannot play is greyed with the discs that would enable it, and on a
// PSP base (the manifest's base_policy = "psp") it is the PlayStation's side
// that is greyed. A member is greyed when choosing its suite cannot play it,
// and the reason line says why. The page reads the cache named in the box as
// it stands, so typing or browsing to another cache redraws it; a choice is
// written as opt= only on Play (SuitesOptEdit), and an untouched page writes
// opt= back as it was read. (Until 2026-10-12 this place held the "PSP extras"
// boxes, one per installed layer; the suites carry the same layers.)

int Selected(HWND dlg, int id);  // below, with the combo box helpers

std::string LanguageChosen(HWND dlg, const DialogState& state) {
    const int lang = Selected(dlg, IDC_LANGUAGE) + (state.original ? 0 : 1);
    return lang >= 1 && static_cast<size_t>(lang) <= state.languages.size()
               ? state.languages[static_cast<size_t>(lang) - 1]
               : std::string();
}

// The cache the game will be given: BOF3X_CACHE when the environment has it
// (it wins at start, ConfigCacheDataRoot), else the box as it stands, trimmed as
// ConfigLoad trims a line, without trailing separators.
std::wstring DialogCacheRoot(HWND dlg) {
    wchar_t root[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_CACHE", root, MAX_PATH);
    std::wstring w;
    if (n > 0 && n < MAX_PATH) {
        w.assign(root, n);
    } else {
        w = DialogText(dlg, IDC_CACHE);
        const size_t b = w.find_first_not_of(L" \t"), e = w.find_last_not_of(L" \t");
        w = b == std::wstring::npos ? std::wstring() : w.substr(b, e - b + 1);
        if (w.empty()) w = HomeCacheDefault();   // the home's own, as ConfigCacheRoot
    }
    while (w.size() > 1 && (w.back() == L'\\' || w.back() == L'/')) w.pop_back();
    return w;
}

// Where the DLL finds layers in the cache: none under BOF3X_CACHE_DATA=0
// (the cache for the music only), as ConfigCacheDataRoot.
std::wstring DialogDataRoot(HWND dlg) {
    wchar_t text[4];
    const DWORD m = GetEnvironmentVariableW(L"BOF3X_CACHE_DATA", text, 4);
    if (m == 1 && text[0] == L'0') return std::wstring();
    return DialogCacheRoot(dlg);
}

std::wstring WideAscii(const std::string& s) {
    std::wstring w;
    for (unsigned char c : s) w += static_cast<wchar_t>(c);
    return w;
}

void SuitesRefresh(HWND dlg, DialogState& state) {
    const std::wstring cache = DialogCacheRoot(dlg), data = DialogDataRoot(dlg), game = state.game_dir;
    SuiteInputs in;
    in.manifest = ManifestRead(cache);
    // a layer's songs from the cache the music reads, under BOF3X_CACHE_DATA=0 too
    in.installed = [game, data, cache](const char* layer) { return ConfigOptInstalled(game, layer, data, cache); };
    in.language = LanguageChosen(dlg, state);
    in.opt = state.cfg->opt;
    in.psp = state.cfg->psp;
    in.defaults = ConfigOptDefaultInstalled(game, data);
    state.suites = SuitesEvaluate(in);

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
    SetText(dlg, IDC_SUITENOTE, note.c_str());

    for (size_t i = 0; i < state.suites.size(); ++i) {
        const SuiteState& s = state.suites[i];
        const int base = IDC_SUITE0 + kSuiteStride * static_cast<int>(i);
        SetText(dlg, base + kSuiteLabel, (std::wstring(s.info->label) + L":").c_str());
        SetText(dlg, base + kSuitePsx, s.psp_base ? L"PlayStation" : L"PlayStation (base)");
        SetText(dlg, base + kSuitePsp, s.psp_base ? L"PSP (base)" : L"PSP");
        Enable(dlg, base + kSuitePsx, s.psx_enabled);
        Enable(dlg, base + kSuitePsp, s.psp_enabled);
        int& choice = state.choice[i];
        if ((choice == 0 && !s.psx_enabled) || (choice == 1 && !s.psp_enabled)) choice = -1;
        const int shown = choice >= 0 ? choice : s.side == SuiteSide::kPsx ? 0 : s.side == SuiteSide::kPsp ? 1 : -1;
        Check(dlg, base + kSuitePsx, shown == 0);
        Check(dlg, base + kSuitePsp, shown == 1);

        // The members, greyed when the PSP's version cannot play them here; and
        // the reasons, the same reason said once for the members that share it.
        std::vector<std::pair<std::wstring, std::wstring>> notes;   // note, the members it is about
        for (int k = 0; k < kSuiteMembers; ++k) {
            // Lines 0..3 every suite has; 4..5 the data suite alone (resource.h).
            HWND line = Item(dlg, base + (k < 4 ? kSuiteMember0 + k : kSuiteMember4 + (k - 4)));
            if (!line) continue;
            if (static_cast<size_t>(k) >= s.members.size()) {
                SetWindowTextW(line, L"");
                ShowWindow(line, SW_HIDE);
                continue;
            }
            const SuiteMemberState& m = s.members[static_cast<size_t>(k)];
            SetWindowTextW(line, m.info->label);
            EnableWindow(line, m.available);
            ShowWindow(line, SW_SHOW);
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
        SetText(dlg, base + kSuiteWhy, why.c_str());
    }
}

// A suite's radio: the player's choice, written on Play.
bool SuiteRadioClicked(DialogState& state, int id) {
    const int at = id - IDC_SUITE0, i = at / kSuiteStride, k = at % kSuiteStride;
    if (at < 0 || static_cast<size_t>(i) >= state.choice.size() || (k != kSuitePsx && k != kSuitePsp)) return false;
    state.choice[static_cast<size_t>(i)] = k == kSuitePsp ? 1 : 0;
    return true;
}

const wchar_t* LanguageLabel(const std::string& code) {
    for (const LanguageInfo& lang : kLanguages)
        if (code == lang.code) return lang.label;
    return L"?";
}

void AddItem(HWND dlg, int id, const wchar_t* text) {
    SendMessageW(Item(dlg, id), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
}

void Select(HWND dlg, int id, int index) {
    SendMessageW(Item(dlg, id), CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

int Selected(HWND dlg, int id) {
    return static_cast<int>(SendMessageW(Item(dlg, id), CB_GETCURSEL, 0, 0));
}

// ---- The row above the text (docs/mods.md 5) -------------------------------------
//
// None; each installed reading-row mod; each other held language. Every entry
// is listed and one this install cannot play is greyed with its reason (the
// owner's rule, 2026-10-10): picking it changes nothing. Redrawn when the
// language, the cache folder or the mods change; the Mods page is told the
// language too, so it greys a mod for another one.

void RubyRefresh(HWND dlg, DialogState& state) {
    const std::string language = LanguageChosen(dlg, state);
    state.ruby = ConfigRubyChoices(state.game_dir, language, DialogDataRoot(dlg), DialogCacheRoot(dlg), HomeDir());
    const std::string want = state.ruby_touched ? state.ruby_pick : state.cfg->ruby;
    int sel = -1;
    for (size_t i = 0; i < state.ruby.size(); ++i)
        if (state.ruby[i].id == want) sel = static_cast<int>(i);
    if (sel < 0 && !want.empty()) {   // bof3x.ini's value, kept and shown as what it is
        state.ruby.push_back({want, ConfigFromUtf8(want) + L" (bof3x.ini's ruby=)", L"not in this install for this language"});
        sel = static_cast<int>(state.ruby.size()) - 1;
    }
    HWND box = Item(dlg, IDC_RUBY);
    SendMessageW(box, CB_RESETCONTENT, 0, 0);
    for (const RubyChoice& c : state.ruby) {
        const std::wstring text = c.why.empty() ? c.label : c.label + L" - " + c.why;
        const LRESULT i = SendMessageW(box, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(box, CB_SETITEMDATA, static_cast<WPARAM>(i), c.why.empty() ? 1 : 0);
    }
    Select(dlg, IDC_RUBY, sel < 0 ? 0 : sel);
    if (HWND mods = SubPageOf(state, IDD_PAGE_MODS))
        SendMessageW(mods, kMsgModsLanguage, 0, reinterpret_cast<LPARAM>(&language));
}

// A pick in the box: a greyed entry is not taken (the box goes back to what it
// showed); another is the player's choice.
void RubyPicked(HWND dlg, DialogState& state) {
    const int i = Selected(dlg, IDC_RUBY);
    if (i < 0 || static_cast<size_t>(i) >= state.ruby.size()) return;
    if (!state.ruby[static_cast<size_t>(i)].why.empty()) {
        const std::string want = state.ruby_touched ? state.ruby_pick : state.cfg->ruby;
        int back = 0;
        for (size_t k = 0; k < state.ruby.size(); ++k)
            if (state.ruby[k].id == want) back = static_cast<int>(k);
        Select(dlg, IDC_RUBY, back);
        MessageBeep(MB_ICONWARNING);
        return;
    }
    state.ruby_touched = true;
    state.ruby_pick = state.ruby[static_cast<size_t>(i)].id;
}

// Mods, Extras' toggle (ruby_ui=, the owner's call 2026-10-22): the box and its
// label shown or hidden. Display only - hidden, the box keeps its entries and
// pick, ruby= is written back as before and the game is given it as before.
// Hidden, every Settings control below the box moves up by its row and the
// page's group box shortens by as much, closing the gap; shown, they move back
// (the owner, 2026-10-22: "gives slightly more space"). kRubyRowDlu is the
// row's pitch in launcher.rc: the box at y 40, the Look row under it at 56.
constexpr int kRubyRowDlu = 16;
constexpr wchar_t kRubyUpProp[] = L"bof3x.ruby_row_closed";

void RubyUiShow(HWND dlg, bool on) {
    HWND box = Item(dlg, IDC_RUBY), label = Item(dlg, IDC_RUBYLABEL);
    for (HWND h : {label, box}) ShowWindow(h, on ? SW_SHOW : SW_HIDE);
    HWND page = GetParent(box);
    const bool closed = GetPropW(page, kRubyUpProp) != nullptr;
    if (closed != on) return;   // already laid out for this state
    RECT row{0, 0, 0, kRubyRowDlu};
    MapDialogRect(page, &row);
    const int dy = on ? row.bottom : -row.bottom;
    RECT b;
    GetWindowRect(box, &b);
    MapWindowPoints(nullptr, page, reinterpret_cast<POINT*>(&b), 2);
    for (HWND c = GetWindow(page, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        if (c == box || c == label) continue;
        RECT r;
        GetWindowRect(c, &r);
        MapWindowPoints(nullptr, page, reinterpret_cast<POINT*>(&r), 2);
        // below the box's row by where the control sat with the row open: its top
        // past the row's middle (the Options... button sits 1 DLU above Look, and
        // the language's note 10 DLU above the box, so neither edge will do)
        const int open_top = r.top + (closed ? row.bottom : 0);
        if (open_top >= b.top + row.bottom / 2)
            SetWindowPos(c, nullptr, r.left, r.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        else if (r.bottom > b.bottom)   // the group box around the page
            SetWindowPos(c, nullptr, 0, 0, r.right - r.left, r.bottom - r.top + dy,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (on) RemovePropW(page, kRubyUpProp);
    else SetPropW(page, kRubyUpProp, reinterpret_cast<HANDLE>(1));
    InvalidateRect(page, nullptr, TRUE);
}

// WM_DRAWITEM for the box: as the system draws a list, a greyed entry in the
// grey text colour (the Jukebox's song list does the same, music_player.cpp).
void RubyDraw(const DRAWITEMSTRUCT& d) {
    const bool selected = (d.itemState & ODS_SELECTED) != 0;
    FillRect(d.hDC, &d.rcItem, GetSysColorBrush(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    if (d.itemID == static_cast<UINT>(-1)) return;
    const LRESULT n = SendMessageW(d.hwndItem, CB_GETLBTEXTLEN, d.itemID, 0);
    if (n < 0) return;
    std::wstring text(static_cast<size_t>(n) + 1, L'\0');
    SendMessageW(d.hwndItem, CB_GETLBTEXT, d.itemID, reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(n));
    SetBkMode(d.hDC, TRANSPARENT);
    SetTextColor(d.hDC, GetSysColor(!d.itemData ? COLOR_GRAYTEXT : selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    RECT r = d.rcItem;
    r.left += 2;
    DrawTextW(d.hDC, text.c_str(), static_cast<int>(text.size()), &r,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    if ((d.itemState & ODS_FOCUS) && !(d.itemState & ODS_NOFOCUSRECT)) DrawFocusRect(d.hDC, &d.rcItem);
}

// ---- DIV-0150: the Music box -------------------------------------------------
//
// The source the game plays first (music=, ConfigMusicSource), its index the
// box's: the PlayStation's, the PSP's, the PC's MP3s. Every source is listed
// (the Config screen's rule); one this install lacks says so, as the next in
// its order then plays (music_seq.cpp Arm).
const char* const kMusicSources[] = {"psx", "psp", "pc"};

int MusicIndex(const std::string& source) {
    for (int i = 0; i < 3; ++i)
        if (source == kMusicSources[i]) return i;
    return 0;
}

void MusicRefresh(HWND dlg, const DialogState& state) {
    const std::wstring cache = DialogCacheRoot(dlg), data = DialogDataRoot(dlg);
    const auto is_dir = [](const std::wstring& d) {
        const DWORD a = GetFileAttributesW(d.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
    };
    const CacheManifest m = ManifestRead(cache);
    const bool psp_base = ManifestPspBase(m);
    const bool songs = !cache.empty() && is_dir(cache + L"\\base\\bgm");
    WIN32_FIND_DATAW found;
    const HANDLE find = FindFirstFileW((state.game_dir + L"\\BGM\\*.DAT").c_str(), &found);
    const bool mp3s = find != INVALID_HANDLE_VALUE;
    if (mp3s) FindClose(find);
    const bool have[3] = {songs && !psp_base,
                          psp_base ? songs : ConfigOptInstalled(state.game_dir, "psp-music", data, cache), mp3s};
    const wchar_t* const names[3] = {L"PlayStation", L"PSP", L"PC (MP3)"};
    const wchar_t* const lacks[3] = {L"PlayStation - not in the cache (plays PSP, then PC)",
                                     L"PSP - not in the cache (plays PlayStation, then PC)",
                                     L"PC (MP3) - no BGM\\ folder (plays PlayStation, then PSP)"};
    const int sel = Selected(dlg, IDC_MUSIC);
    SendMessageW(Item(dlg, IDC_MUSIC), CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < 3; ++i) AddItem(dlg, IDC_MUSIC, have[i] ? names[i] : lacks[i]);
    Select(dlg, IDC_MUSIC, sel < 0 ? MusicIndex(ConfigMusicSource(state.cfg->music, state.cfg->opt)) : sel);
}

// ---- The preset (launcher/config.h; docs/launcher-settings.md "preset=") -----
//
// The Settings tab's Preset sub-page: the box (kPresets' order), the preset's
// sentence, and a read-only list of what it changes when the game starts -
// ConfigPresetPlan for the language in the Language box, on this install, as
// the launch will make it. A control whose ini key the preset sets is grey,
// with a line under Settings: the player's value stays in bof3x.ini, and the
// game gets the preset's.

// The controls of the ini keys a preset can set today (the ledger's ini:
// switches of Default on entries): display=, background=, music=. scale= and
// opt= have no control of their own.
struct PresetControl {
    const char* key;
    int id;
    const wchar_t* label;
};
constexpr PresetControl kPresetControls[] = {
    {"display", IDC_DISPLAY, L"Display"},
    {"background", IDC_BACKGROUND, L"Keep the game running when its window is not active"},
    {"music", IDC_MUSIC, L"Music (Music page)"},
};

std::wstring PresetTitle(const ledger::Entry& e) {
    std::wstring t = ConfigFromUtf8(std::string(e.title));
    t.erase(std::remove(t.begin(), t.end(), L'`'), t.end());   // the ledger's code marks
    return t;
}

std::wstring PresetLine(const ledger::Entry& e) {
    wchar_t id[16];
    std::swprintf(id, 16, L"DIV-%04u  ", static_cast<unsigned>(e.id));
    return std::wstring(L"  ") + id + PresetTitle(e);
}

void PresetRefresh(HWND dlg, DialogState& state) {
    if (!state.cfg) return;
    const int sel = Selected(dlg, IDC_PRESET);
    const Preset preset = kPresets[sel >= 0 && static_cast<size_t>(sel) < std::size(kPresets) ? sel : 0].id;
    Config chosen = *state.cfg;
    chosen.language = LanguageChosen(dlg, state);
    if (chosen.language.empty()) chosen.language = kLanguageOriginal;
    const std::string language = ConfigLanguagePlayed(state.game_dir, chosen);
    const PresetPlan plan = ConfigPresetPlan(state.game_dir, *state.cfg, preset, language);
    SetText(dlg, IDC_PRESETNOTE, PresetOf(preset).note);

    std::wstring out = L"For the language that you selected: ";
    out += language == kLanguageOriginal ? L"Chinese (PC script)" : LanguageLabel(language);
    out += L".\r\n";
    std::vector<const wchar_t*> greyed;
    for (const PresetControl& c : kPresetControls) {
        const bool set = PresetOverrides(plan, c.key);
        Enable(dlg, c.id, !set);
        if (set) greyed.push_back(c.label);
    }
    SetText(dlg, IDC_PRESETGREY, PresetOverrides(plan, "display") || PresetOverrides(plan, "background")
                                     ? L"A grey setting: the preset (Preset page) sets it when the game starts."
                                     : L"");
    if (plan.items.empty()) {
        out += L"\r\nThis preset turns off no change.\r\n";
        SetText(dlg, IDC_PRESETLIST, out.c_str());
        return;
    }
    if (!greyed.empty()) {
        out += L"\r\nThe preset sets these settings in place of yours (they are grey):";
        for (size_t i = 0; i < greyed.size(); ++i) out += (i ? L", " : L" ") + std::wstring(greyed[i]);
        out += L".\r\n";
    }

    std::wstring off, here, kept, cannot, also;
    int n_off = 0, n_here = 0, n_kept = 0, n_cannot = 0, n_also = 0, n_lang = 0, n_skip = 0, n_never = 0;
    for (const PresetPlan::Item& item : plan.items) {
        const ledger::Entry& e = *item.entry;
        switch (item.action) {
        case PresetAction::kOff: {
            const int count = static_cast<int>(ledger::SwitchesEnd(e) - ledger::SwitchesBegin(e));
            const int left = count - static_cast<int>(item.not_here.size() + item.kept.size());
            std::wstring why;
            for (const auto& [sw, reason] : item.not_here)
                why += (why.empty() ? L" - " : L"; ") + ConfigFromUtf8(sw.substr(4)) + L": " + ConfigFromUtf8(reason);
            for (const auto& [sw, reason] : item.kept)
                why += (why.empty() ? L" - " : L"; ") + ConfigFromUtf8(sw.substr(4)) + L": " + ConfigFromUtf8(reason);
            if (left == 0 && !item.not_here.empty()) {
                here += PresetLine(e) + why + L"\r\n";
                ++n_here;
            } else if (left == 0) {
                kept += PresetLine(e) + why + L"\r\n";
                ++n_kept;
            } else {
                std::wstring sw;
                for (const ledger::Switch* s = ledger::SwitchesBegin(e); s != ledger::SwitchesEnd(e); ++s)
                    if (s->form == ledger::SwitchForm::Ini || s->form == ledger::SwitchForm::Env)
                        sw += (sw.empty() ? L" (" : L", ") + ConfigFromUtf8(std::string(s->text.substr(4)));
                if (!sw.empty()) sw += L")";
                off += PresetLine(e) + sw + why + L"\r\n";
                ++n_off;
            }
            break;
        }
        case PresetAction::kCannot: cannot += PresetLine(e) + L"\r\n"; ++n_cannot; break;
        case PresetAction::kAlsoAvailable: also += PresetLine(e) + L"\r\n"; ++n_also; break;
        case PresetAction::kLangGoverned: ++n_lang; break;
        case PresetAction::kSkippedLanguage: ++n_skip; break;
        case PresetAction::kNever: ++n_never; break;
        default: break;
        }
    }
    auto section = [&out](const wchar_t* head, int n, const std::wstring& lines) {
        if (n == 0) return;
        wchar_t count[16];
        std::swprintf(count, 16, L" (%d)", n);
        out += L"\r\n";
        out += head;
        out += count;
        out += L":\r\n";
        out += lines;
    };
    section(L"Turned off when the game starts", n_off, off);
    section(L"Not on this install: the preset cannot turn these off here", n_here, here);
    section(L"Kept: a setting of yours or of the environment controls these", n_kept, kept);
    section(L"This build cannot turn these off", n_cannot, cannot);
    section(L"Also available: the preset keeps these on, but they are off. Turn them on yourself", n_also, also);
    wchar_t tail[320];
    std::swprintf(tail, std::size(tail),
                  L"\r\nNot changed: %d follow the language that you select (no preset changes the language), "
                  L"%d are not used with this language, %d must stay on (Forced, or they cannot go back).\r\n",
                  n_lang, n_skip, n_never);
    out += tail;
    SetText(dlg, IDC_PRESETLIST, out.c_str());
}

// ---- Pad navigation of the dialogs (the owner, 2026-09-24: from the couch) --
//
// Every dialog polls the pad on a timer and, when it is the active window,
// turns new presses into what the keyboard would do: up / down move the
// focus, left / right change a combo box or a slider (else move the focus),
// cross clicks the focused button or toggles its box or cycles a combo,
// circle is Cancel and start is OK. The map is the launcher's own bindings,
// so a player who rebinds cross navigates with the new cross. Edges only -
// hold a direction and nothing repeats. The capture dialog does not attach:
// there a press is the thing being captured.
namespace padnav {

// Ours, not an address: any id no attached dialog uses for another timer (the
// capture's is 1, and it does not attach). Polled every 16 ms, about 60 Hz.
constexpr UINT_PTR kTimer = 0x5D1;
std::vector<input::PadBinding> g_map = input::Bindings::Defaults().pad;
unsigned g_prev = 0;
HWND g_last = nullptr;

void SetMap(const std::vector<input::PadBinding>& map) { g_map = map; }

void Attach(HWND dlg) { SetTimer(dlg, kTimer, 16, nullptr); }

void Tick(HWND dlg) {
    if (!input::PadSdl_Started() || GetActiveWindow() != dlg) return;
    input::PadSdl_Poll();
    const unsigned word = input::PadSdl_Word(g_map);
    if (dlg != g_last) {   // a dialog just became active: what is held is not a press
        g_last = dlg;
        g_prev = word;
        return;
    }
    const unsigned pressed = word & ~g_prev;
    g_prev = word;
    if (!pressed) return;

    HWND focus = GetFocus();
    wchar_t cls[32] = L"";
    if (focus) GetClassNameW(focus, cls, 32);
    const bool combo = wcscmp(cls, L"ComboBox") == 0, slider = wcscmp(cls, L"msctls_trackbar32") == 0,
               button = wcscmp(cls, L"Button") == 0;
    const int id = focus ? GetDlgCtrlID(focus) : 0;

    auto move = [&](bool back) { SendMessageW(dlg, WM_NEXTDLGCTL, back ? 1 : 0, FALSE); };
    auto combo_step = [&](int step, bool wrap) {
        const int n = static_cast<int>(SendMessageW(focus, CB_GETCOUNT, 0, 0));
        int i = static_cast<int>(SendMessageW(focus, CB_GETCURSEL, 0, 0)) + step;
        if (wrap) i = (i + n) % n; else i = i < 0 ? 0 : i >= n ? n - 1 : i;
        SendMessageW(focus, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
        SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(focus));
    };
    if (pressed & input::kUp) move(true);
    if (pressed & input::kDown) move(false);
    if (pressed & (input::kLeft | input::kRight)) {
        const int step = pressed & input::kLeft ? -1 : 1;
        if (combo) {
            combo_step(step, false);
        } else if (slider) {
            SendMessageW(focus, TBM_SETPOS, TRUE, SendMessageW(focus, TBM_GETPOS, 0, 0) + step);
            SendMessageW(dlg, WM_HSCROLL, TB_THUMBTRACK, reinterpret_cast<LPARAM>(focus));
        } else {
            move(step < 0);
        }
    }
    if (pressed & input::kCross) {
        if (button) SendMessageW(focus, BM_CLICK, 0, 0);
        else if (combo) combo_step(1, true);
    }
    if (pressed & input::kCircle) SendMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    if (pressed & input::kStart) SendMessageW(dlg, WM_COMMAND, IDOK, 0);
}

}  // namespace padnav

void Populate(HWND dlg, DialogState& state) {
    const Config& cfg = *state.cfg;

    if (state.original) AddItem(dlg, IDC_LANGUAGE, L"Chinese (PC script)");
    int selected = 0;
    for (size_t i = 0; i < state.languages.size(); ++i) {
        AddItem(dlg, IDC_LANGUAGE, LanguageLabel(state.languages[i]));
        if (state.languages[i] == cfg.language) selected = static_cast<int>(i) + (state.original ? 1 : 0);
    }
    if (!state.original) {
        // DIV-0102: a disc-only install has no Chinese text to offer.
        SetText(dlg, IDC_LANGNOTE, state.languages.empty()
                                       ? L"Chinese is only in the PC version. No other language is built (importer.py --lang)."
                                       : L"Chinese (PC script) is only in the PC version.");
    } else if (state.languages.empty()) {
        // Offering an option that cannot work is worse than explaining why it
        // is missing. The overlays are built, not shipped.
        SetText(dlg, IDC_LANGNOTE, L"No language files in DAT\\. Build them with tools/loc_build.py.");
    } else {
        SetText(dlg, IDC_LANGNOTE, L"");
    }
    Select(dlg, IDC_LANGUAGE, selected);
    RubyRefresh(dlg, state);   // after the language: the box's entries follow it
    Check(dlg, IDC_EXTRA_RUBY, cfg.ruby_ui);
    RubyUiShow(dlg, cfg.ruby_ui);
    Check(dlg, IDC_EXTRA_TEXTSPEED, cfg.text_faster);
    Check(dlg, IDC_EXTRA_LEARNBLUE, cfg.unlearned_blue);

    AddItem(dlg, IDC_FILTER, L"Smooth - bilinear (original)");
    AddItem(dlg, IDC_FILTER, L"Sharp - point (no blur)");
    AddItem(dlg, IDC_FILTER, L"CRT effect (SatPixie)");
    Select(dlg, IDC_FILTER, cfg.satpixie ? 2 : cfg.filter == Filter::kPoint ? 1 : 0);
    Enable(dlg, IDC_LOOKOPTIONS, cfg.satpixie);

    AddItem(dlg, IDC_DISPLAY, L"Full screen (F8 changes to a window)");
    AddItem(dlg, IDC_DISPLAY, L"Window (F8 changes to full screen)");
    Select(dlg, IDC_DISPLAY, cfg.display == Display::kWindowed ? 1 : 0);
    Check(dlg, IDC_BACKGROUND, cfg.background);
    // DIV-0041: the picture's width, the box's index the ini's wide= (0, 1, 2).
    AddItem(dlg, IDC_WIDE, L"Off - 4:3, 320 x 240 (original)");
    AddItem(dlg, IDC_WIDE, L"16:9 - 426 x 240");
    AddItem(dlg, IDC_WIDE, L"21:9 - 560 x 240");
    Select(dlg, IDC_WIDE, cfg.wide == 1 || cfg.wide == 2 ? cfg.wide : 0);

    Check(dlg, IDC_SNAP, cfg.snap);


    SetText(dlg, IDC_CACHE, ConfigFromUtf8(cfg.cache).c_str());
    CacheNote(dlg);
    SendMessageW(Item(dlg, IDC_MUSIC), CB_RESETCONTENT, 0, 0);
    MusicRefresh(dlg, state);  // DIV-0150

    // Each suite's side as opt= plays it now (an empty opt= the default
    // layers, none of which is a suite's).
    SuitesRefresh(dlg, state);

    Check(dlg, IDC_SHOW, cfg.show_launcher);

    // The preset, after the boxes it may grey (PresetRefresh).
    SendMessageW(Item(dlg, IDC_PRESET), CB_RESETCONTENT, 0, 0);
    int preset = 0;
    for (size_t i = 0; i < std::size(kPresets); ++i) {
        AddItem(dlg, IDC_PRESET, kPresets[i].label);
        if (kPresets[i].id == cfg.preset) preset = static_cast<int>(i);
    }
    Select(dlg, IDC_PRESET, preset);
    PresetRefresh(dlg, state);
}

// The inverse of Populate. Every look but the first takes the point filter
// (the CRT look is "over the point filter", config.h); renderer 1 is
// Direct3D, 0 Software (config.h, Cfg_RenderMode).
void ReadBack(HWND dlg, const DialogState& state) {
    Config& cfg = *state.cfg;
    const int lang = Selected(dlg, IDC_LANGUAGE) + (state.original ? 0 : 1);
    cfg.language = lang >= 1 && static_cast<size_t>(lang) <= state.languages.size()
                       ? state.languages[static_cast<size_t>(lang) - 1]
                       : kLanguageOriginal;
    cfg.language_named = true;   // the box's choice (DIV-0102)
    const int look = Selected(dlg, IDC_FILTER);
    cfg.filter = look >= 1 ? Filter::kPoint : Filter::kLinear;
    cfg.satpixie = look == 2;
    cfg.display = Selected(dlg, IDC_DISPLAY) == 1 ? Display::kWindowed : Display::kFullscreen;
    cfg.snap = Checked(dlg, IDC_SNAP);
    cfg.background = Checked(dlg, IDC_BACKGROUND);
    const int wide = Selected(dlg, IDC_WIDE);
    cfg.wide = wide == 1 || wide == 2 ? wide : 0;
    cfg.show_launcher = Checked(dlg, IDC_SHOW);
    if (state.ruby_touched) cfg.ruby = state.ruby_pick;   // untouched: ruby= as read
    cfg.ruby_ui = Checked(dlg, IDC_EXTRA_RUBY);
    cfg.ruby_ui_named = true;
    cfg.text_faster = Checked(dlg, IDC_EXTRA_TEXTSPEED);
    cfg.unlearned_blue = Checked(dlg, IDC_EXTRA_LEARNBLUE);
    const int preset = Selected(dlg, IDC_PRESET);   // a grey box above keeps the player's value
    if (preset >= 0 && static_cast<size_t>(preset) < std::size(kPresets)) cfg.preset = kPresets[preset].id;

    // The Music box (DIV-0150): the source music= already means keeps the line
    // as written (an empty music=, seq or mp3 too); another writes its name.
    const int music = Selected(dlg, IDC_MUSIC);
    if (music >= 0 && music != MusicIndex(ConfigMusicSource(cfg.music, cfg.opt))) cfg.music = kMusicSources[music];

    // The suites: a choice moves its layers in opt=; none chosen, opt= as read.
    // The Jukebox's chord box with them, in one edit of the line as read: a
    // choice that returns to where it started writes opt= back byte for byte.
    std::vector<std::pair<std::string, bool>> extra;
    if (state.chord >= 0) extra.emplace_back(kChordLayer, state.chord == 1);
    cfg.opt = SuitesOptEdit(cfg.opt, ConfigOptDefaultInstalled(state.game_dir, DialogDataRoot(dlg)), state.suites,
                            state.choice, extra);
    cfg.psp = SuitesPspEdit(cfg.psp, state.suites, state.choice);   // DIV-0108: the code members
}

// The cache box as the ini's UTF-8 bytes, trimmed as ConfigLoad trims a line.
// Every path can be held since state 3: the refusal for characters the ANSI
// code page lacked is gone.
void ReadCache(HWND dlg, Config& cfg) {
    std::wstring w = DialogText(dlg, IDC_CACHE);
    const size_t b = w.find_first_not_of(L" \t"), e = w.find_last_not_of(L" \t");
    w = b == std::wstring::npos ? std::wstring() : w.substr(b, e - b + 1);
    cfg.cache = ConfigToUtf8(w);
}

// ---- DIV-0043: the SatPixie options dialog ----------------------------------

struct Slider {
    int id, label;
    float Config::Satpixie::*value;
    float lo, hi, step;
    const char* format;
};
const Slider kSliders[] = {
    {IDC_SP_MODULATE, IDC_SP_MODULATE_V, &Config::Satpixie::modulate, 0.0f, 1.0f, 0.01f, "%.2f"},
    {IDC_SP_GAMMA, IDC_SP_GAMMA_V, &Config::Satpixie::gamma, 1.8f, 2.6f, 0.1f, "%.1f"},
    {IDC_SP_CHROMA, IDC_SP_CHROMA_V, &Config::Satpixie::chroma, 0.0f, 5.0f, 0.1f, "%.1f"},
    {IDC_SP_BLURX, IDC_SP_BLURX_V, &Config::Satpixie::blur_x, 0.0f, 5.0f, 0.25f, "%.2f"},
    {IDC_SP_BLURY, IDC_SP_BLURY_V, &Config::Satpixie::blur_y, 0.0f, 5.0f, 0.25f, "%.2f"},
};
struct Toggle {
    int id;
    bool Config::Satpixie::*value;
};
const Toggle kToggles[] = {
    {IDC_SP_NATURAL, &Config::Satpixie::natural},   {IDC_SP_GHOSTING, &Config::Satpixie::ghosting},
    {IDC_SP_CHROMAON, &Config::Satpixie::chroma_on}, {IDC_SP_VIGNETTE, &Config::Satpixie::vignette},
    {IDC_SP_VIG43, &Config::Satpixie::vignette_43},  {IDC_SP_WIGGLE, &Config::Satpixie::wiggle},
    {IDC_SP_SCANROLL, &Config::Satpixie::scanroll},  {IDC_SP_OVERSCAN, &Config::Satpixie::overscan},
};

int SliderSteps(const Slider& s) { return static_cast<int>((s.hi - s.lo) / s.step + 0.5f); }

void ShowSliderValue(HWND dlg, const Slider& s, float v) {
    char text[32];
    snprintf(text, sizeof text, s.format, static_cast<double>(v));
    SetDlgItemTextA(dlg, s.label, text);
}

void SatpixiePopulate(HWND dlg, const Config::Satpixie& sp) {
    for (const Slider& s : kSliders) {
        HWND h = GetDlgItem(dlg, s.id);
        SendMessageW(h, TBM_SETRANGE, TRUE, MAKELPARAM(0, SliderSteps(s)));
        SendMessageW(h, TBM_SETTICFREQ, static_cast<WPARAM>(SliderSteps(s) / 10 > 0 ? SliderSteps(s) / 10 : 1), 0);
        const float v = sp.*s.value;
        SendMessageW(h, TBM_SETPOS, TRUE, static_cast<LPARAM>((v - s.lo) / s.step + 0.5f));
        ShowSliderValue(dlg, s, v);
    }
    for (const Toggle& t : kToggles) CheckDlgButton(dlg, t.id, sp.*t.value ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(dlg, IDC_SP_MASK, CB_RESETCONTENT, 0, 0);
    AddItem(dlg, IDC_SP_MASK, L"Off");
    AddItem(dlg, IDC_SP_MASK, L"Brightness lines");
    AddItem(dlg, IDC_SP_MASK, L"Colour stripes");
    Select(dlg, IDC_SP_MASK, sp.mask >= 0 && sp.mask <= 2 ? sp.mask : 0);
}

void SatpixieReadBack(HWND dlg, Config::Satpixie& sp) {
    for (const Slider& s : kSliders) {
        const int pos = static_cast<int>(SendDlgItemMessageW(dlg, s.id, TBM_GETPOS, 0, 0));
        sp.*s.value = s.lo + static_cast<float>(pos) * s.step;
    }
    for (const Toggle& t : kToggles) sp.*t.value = IsDlgButtonChecked(dlg, t.id) == BST_CHECKED;
    const int mask = Selected(dlg, IDC_SP_MASK);
    sp.mask = mask >= 0 && mask <= 2 ? mask : 0;
}

INT_PTR CALLBACK SatpixieProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        padnav::Attach(dlg);
        auto* sp = reinterpret_cast<Config::Satpixie*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(sp));
        SatpixiePopulate(dlg, *sp);
        return TRUE;
    }
    case WM_HSCROLL: {
        // A slider moved: show its value.
        HWND h = reinterpret_cast<HWND>(lp);
        for (const Slider& s : kSliders) {
            if (GetDlgItem(dlg, s.id) != h) continue;
            const int pos = static_cast<int>(SendMessageW(h, TBM_GETPOS, 0, 0));
            ShowSliderValue(dlg, s, s.lo + static_cast<float>(pos) * s.step);
        }
        return TRUE;
    }
    case WM_TIMER:
        if (wp == padnav::kTimer) {
            padnav::Tick(dlg);
            return TRUE;
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK: {
            auto* sp = reinterpret_cast<Config::Satpixie*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            SatpixieReadBack(dlg, *sp);
            EndDialog(dlg, 1);
            return TRUE;
        }
        case IDC_SP_DEFAULTS: {
            Config::Satpixie defaults;
            SatpixiePopulate(dlg, defaults);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, 0);
            return TRUE;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

// The options dialog over the settings dialog; the parameters change only on OK.
void SatpixieOptions(HWND owner, Config::Satpixie& sp) {
    Config::Satpixie edit = sp;
    const INT_PTR r = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SATPIXIE), owner, SatpixieProc,
                                      reinterpret_cast<LPARAM>(&edit));
    if (r == 1) sp = edit;
}

// ---- DIV-0045 / DIV-0046: the Game Options page's cheats --------------------
//
// Read back into cfg only when the page was touched (DialogState), so an ini
// holding more than the cap is shown as the cap but written back as it was.

struct CheatSlider {
    int id, label;
    int Config::Cheats::*value;
};
const CheatSlider kCheatSliders[] = {
    {IDC_CH_EXP, IDC_CH_EXP_V, &Config::Cheats::exp},
    {IDC_CH_ZENNY, IDC_CH_ZENNY_V, &Config::Cheats::zenny},
};
constexpr int kCheatMultiplierMax = 10;   // the DLL clamps more to this (src/game/cheats.cpp)

void CheatsPopulate(HWND dlg, const Config::Cheats& ch) {
    for (const CheatSlider& s : kCheatSliders) {
        HWND h = GetDlgItem(dlg, s.id);
        SendMessageW(h, TBM_SETRANGE, TRUE, MAKELPARAM(0, kCheatMultiplierMax));
        SendMessageW(h, TBM_SETTICFREQ, 1, 0);
        // An ini saved while the cap was 50 may hold more: shown as the cap, and
        // saved as the cap once the page is touched.
        const int v = ch.*s.value > kCheatMultiplierMax ? kCheatMultiplierMax : ch.*s.value;
        SendMessageW(h, TBM_SETPOS, TRUE, static_cast<LPARAM>(v));
        SetDlgItemInt(dlg, s.label, static_cast<UINT>(v), FALSE);
    }
    CheckDlgButton(dlg, IDC_CH_STEAL, ch.steal ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_CH_INFO, ch.enemy_info ? BST_CHECKED : BST_UNCHECKED);
}

void CheatsReadBack(HWND dlg, Config::Cheats& ch) {
    for (const CheatSlider& s : kCheatSliders) {
        const int pos = static_cast<int>(SendDlgItemMessageW(dlg, s.id, TBM_GETPOS, 0, 0));
        ch.*s.value = pos < 0 ? 0 : pos > kCheatMultiplierMax ? kCheatMultiplierMax : pos;
    }
    ch.steal = IsDlgButtonChecked(dlg, IDC_CH_STEAL) == BST_CHECKED;
    ch.enemy_info = IsDlgButtonChecked(dlg, IDC_CH_INFO) == BST_CHECKED;
}

// A slider on the page moved: show its value. False when it is not one of these.
bool CheatsSliderMoved(HWND page, HWND h) {
    for (const CheatSlider& s : kCheatSliders) {
        if (GetDlgItem(page, s.id) != h) continue;
        SetDlgItemInt(page, s.label, static_cast<UINT>(SendMessageW(h, TBM_GETPOS, 0, 0)), FALSE);
        return true;
    }
    return false;
}

// ---- DIV-0050: the Controls page (docs/controls.md section 4.2) -------------
//
// Fourteen rows, one a PlayStation input, each with two key cells and two pad
// cells; a cell is a button showing its binding. Clicking one opens the
// capture, which takes the next key (a keyboard hook, so the focus does not
// matter) or the next pad input that was not already held, and the cell
// takes it - from whichever cell held it before, so a key means one thing.
// Play or Back rebuilds the bindings list from the cells, once one has
// changed (DialogState.cells_touched). A key entry that names two
// actions at once (the keypad's diagonals in the default table) has no cell,
// and is kept as it is unless a cell takes its key.

std::wstring Wide(const std::string& s) {
    std::wstring w;
    for (unsigned char c : s) w += static_cast<wchar_t>(c);
    return w;
}

Cells CellsFrom(const input::Bindings& b) {
    Cells c{};
    for (int row = 0; row < input::kActionCount; ++row)
        c.key[row][0] = c.key[row][1] = c.pad[row][0] = c.pad[row][1] = -1;
    const input::ActionInfo* actions = input::Actions();
    for (const input::KeyBinding& k : b.keys) {
        const bool single = (k.bits & (k.bits - 1)) == 0;
        // KeyName spells an unnamed scancode as "0x..". The 0 key is named "0",
        // so the test is the prefix, not the first character.
        const std::string name = input::KeyName(k.dik);
        const bool named = name.compare(0, 2, "0x") != 0;
        int row = -1;
        for (int r = 0; r < input::kActionCount; ++r)
            if (actions[r].bit == k.bits) row = r;
        if (!single || !named || row < 0) {
            c.kept.push_back(k);
            continue;
        }
        if (c.key[row][0] < 0) c.key[row][0] = k.dik;
        else if (c.key[row][1] < 0) c.key[row][1] = k.dik;
        else c.kept.push_back(k);
    }
    for (const input::PadBinding& p : b.pad) {
        int row = -1;
        for (int r = 0; r < input::kActionCount; ++r)
            if (actions[r].bit == p.bits) row = r;
        if (row < 0) continue;
        if (c.pad[row][0] < 0) c.pad[row][0] = static_cast<int>(p.input);
        else if (c.pad[row][1] < 0) c.pad[row][1] = static_cast<int>(p.input);
    }
    c.layout = b.layout;
    return c;
}

void BindingsFrom(const Cells& c, input::Bindings& b) {
    const input::ActionInfo* actions = input::Actions();
    bool taken[256] = {};
    b.keys.clear();
    b.pad.clear();
    for (int row = 0; row < input::kActionCount; ++row) {
        for (int slot = 0; slot < 2; ++slot) {
            const int dik = c.key[row][slot];
            if (dik < 0 || taken[dik]) continue;
            taken[dik] = true;
            b.keys.push_back({static_cast<unsigned char>(dik), actions[row].bit});
        }
        for (int slot = 0; slot < 2; ++slot) {
            const int in = c.pad[row][slot];
            if (in < 0) continue;
            bool dup = false;
            for (const input::PadBinding& p : b.pad) dup = dup || static_cast<int>(p.input) == in;
            if (!dup) b.pad.push_back({static_cast<input::PadInput>(in), actions[row].bit});
        }
    }
    for (const input::KeyBinding& k : c.kept) {
        if (taken[k.dik]) continue;
        taken[k.dik] = true;
        b.keys.push_back(k);
    }
    if (b.keys.size() > static_cast<size_t>(input::kKeyTableMax)) b.keys.resize(input::kKeyTableMax);
    b.layout = c.layout;
}

// `dlg` is the window: the cells are on two sub-pages, found by id (Item).
void ControlsRefresh(HWND dlg, const Cells& c) {
    for (int row = 0; row < input::kActionCount; ++row) {
        for (int slot = 0; slot < 2; ++slot) {
            const int dik = c.key[row][slot];
            SetText(dlg, (slot ? IDC_CT_KEY2 : IDC_CT_KEY1) + row,
                    dik < 0 ? L"(none)" : Wide(input::KeyName(dik)).c_str());
            const int in = c.pad[row][slot];
            SetText(dlg, (slot ? IDC_CT_PAD2 : IDC_CT_PAD1) + row,
                    in < 0 ? L"(none)" : Wide(input::PadInputs()[in].label).c_str());
        }
    }
}

void ControlsPopulate(HWND dlg, const Cells& c) {
    ControlsRefresh(dlg, c);
    SendMessageW(Item(dlg, IDC_CT_LAYOUT), CB_RESETCONTENT, 0, 0);
    AddItem(dlg, IDC_CT_LAYOUT, L"Position - the bottom button accepts (Xbox, PlayStation)");
    AddItem(dlg, IDC_CT_LAYOUT, L"Nintendo - the right button accepts");
    AddItem(dlg, IDC_CT_LAYOUT, L"Automatic - use the labels on the pad");
    Select(dlg, IDC_CT_LAYOUT, static_cast<int>(c.layout));
}

// ---- The capture -------------------------------------------------------------------

enum class Captured { kCancel, kKey, kPad, kClear };
struct Capture {
    const wchar_t* what;   // "Up", "Cross" ...
    bool key_cell;         // which kind of cell asked, for the prompt only: either device may answer
    Captured kind = Captured::kCancel;
    int value = -1;
    bool held_at_open[static_cast<int>(input::PadInput::kCount)] = {};
    HWND dlg = nullptr;
    HHOOK hook = nullptr;
};
Capture* g_capture = nullptr;

// WH_KEYBOARD on this thread: the scancode is bits 16..23 of lParam, the
// extended flag bit 24, and DirectInput's code is the scancode with the
// extended flag in bit 7 (Up 0xC8 = 0x48 | 0x80, RCtrl 0x9D = 0x1D | 0x80).
LRESULT CALLBACK CaptureKeyHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && g_capture && !(lp & (1u << 31))) {   // a key going down
        const int scan = (lp >> 16) & 0xFF;
        const bool extended = (lp & (1 << 24)) != 0;
        if (scan == 0x01) {   // Escape cancels
            g_capture->kind = Captured::kCancel;
            EndDialog(g_capture->dlg, 0);
            return 1;
        }
        if (scan != 0) {
            g_capture->kind = Captured::kKey;
            g_capture->value = scan | (extended ? 0x80 : 0);
            EndDialog(g_capture->dlg, 1);
            return 1;
        }
    }
    (void)wp;
    return CallNextHookEx(nullptr, code, wp, lp);
}

INT_PTR CALLBACK CaptureProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* c = reinterpret_cast<Capture*>(lp);
        c->dlg = dlg;
        g_capture = c;
        std::wstring prompt = std::wstring(L"Press a key or a pad button for ") + c->what + L".";
        SetDlgItemTextW(dlg, IDC_CP_PROMPT, prompt.c_str());
        // What is held now is not the answer: a button that opened this dialog
        // through the pad navigation is still down.
        input::PadSdl_Poll();
        for (int i = 0; i < static_cast<int>(input::PadInput::kCount); ++i)
            c->held_at_open[i] = input::PadSdl_InputDown(static_cast<input::PadInput>(i));
        c->hook = SetWindowsHookExW(WH_KEYBOARD, CaptureKeyHook, nullptr, GetCurrentThreadId());
        // The pad poll. WM_DESTROY kills it and removes the hook.
        SetTimer(dlg, 1, 16, nullptr);
        return TRUE;
    }
    case WM_TIMER: {
        auto* c = g_capture;
        if (!c || !input::PadSdl_Started()) return TRUE;
        input::PadSdl_Poll();
        for (int i = 0; i < static_cast<int>(input::PadInput::kCount); ++i) {
            const bool down = input::PadSdl_InputDown(static_cast<input::PadInput>(i));
            if (!down) c->held_at_open[i] = false;
            else if (!c->held_at_open[i]) {
                c->kind = Captured::kPad;
                c->value = i;
                EndDialog(dlg, 1);
                return TRUE;
            }
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_CP_CLEAR) {
            g_capture->kind = Captured::kClear;
            EndDialog(dlg, 1);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            g_capture->kind = Captured::kCancel;
            EndDialog(dlg, 0);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        KillTimer(dlg, 1);
        if (g_capture && g_capture->hook) UnhookWindowsHookEx(g_capture->hook);
        if (g_capture) g_capture->hook = nullptr;
        g_capture = nullptr;
        break;
    default:
        break;
    }
    return FALSE;
}

// The capture over a cell; then the cell takes what was pressed and any other
// cell that held it lets go.
void CaptureInto(HWND owner, Cells& c, int row, int slot, bool key_cell) {
    const std::wstring what = Wide(input::Actions()[row].label);
    Capture cap{what.c_str(), key_cell};
    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CAPTURE), owner, CaptureProc,
                    reinterpret_cast<LPARAM>(&cap));
    if (cap.kind == Captured::kCancel) return;
    if (cap.kind == Captured::kClear) {
        (key_cell ? c.key : c.pad)[row][slot] = -1;
        return;
    }
    // A key answers a pad cell, or a pad a key cell: the device decides the
    // kind of cell, the row stays; it goes in that row's first free cell of
    // its kind, or its first cell.
    int (*cells)[2] = cap.kind == Captured::kKey ? c.key : c.pad;
    if ((cap.kind == Captured::kKey) != key_cell) {
        slot = cells[row][0] < 0 ? 0 : cells[row][1] < 0 ? 1 : 0;
    }
    for (int r = 0; r < input::kActionCount; ++r)
        for (int s = 0; s < 2; ++s)
            if (cells[r][s] == cap.value) cells[r][s] = -1;
    cells[row][slot] = cap.value;
    if (cap.kind == Captured::kKey)
        c.kept.erase(std::remove_if(c.kept.begin(), c.kept.end(),
                                    [&](const input::KeyBinding& k) { return k.dik == cap.value; }),
                     c.kept.end());
}

// ---- The window (docs/launcher-settings.md section 2a) -----------------------
//
// The picture is 640 x 480 with black bands above and below the scene, as the
// game drew it; the banner sits in the top band and the title page's buttons in
// the bottom one. Everything is placed in the picture's pixels times the
// display's scale (the owner, 2026-10-15): 640 x 480 at 100 %, 1280 x 960 at
// 200 %. The picture is drawn without smoothing at a whole scale and smoothed
// at a fractional one, where unsmoothed pixels would come out uneven. The
// sub-pages are dialog units, which Windows scales by the same factor, so
// they are laid out to fit the window at any scale; one that did not would
// leave the window larger, the picture centred.

constexpr int kPictureW = 640, kPictureH = 480;
// Configure's rows, in the picture's pixels: the banner's band, the tabs, the
// sub-tabs when the tab has them, the page.
constexpr int kBandH = 60, kTabsY = 70, kTabH = 23, kPageY = kTabsY + kTabH + 8, kSubTabH = 22,
              kSubPageY = kPageY + kSubTabH + 6, kMargin = 13, kBottom = 8;
// The title page's buttons, in the picture's pixels (the owner's mock-up, 2026-10-15).
struct TitleButton {
    int id, x0, x1;
};
constexpr TitleButton kTitleButtons[] = {
    {IDOK, 236, 324}, {IDC_CONFIGURE, 338, 426}, {IDC_MUSICPLAYER, 439, 527}, {IDCANCEL, 541, 629}};
constexpr int kTitleButtonY = 440, kTitleButtonH = 23;

// The page the window opens on: the title page, or the Data page again after
// the Sources page rebuilt the install (kDialogSourcesChanged).
int g_reopen = kPageTitle;

// The picture's pixels at the display's scale.
int Px(const DialogState& state, double v) { return static_cast<int>(v * state.scale + 0.5); }

template <typename T>
void Release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

// The picture out of the exe's resources (IDR_BACKGROUND), decoded by WIC -
// Windows' own codecs, nothing vendored - to 32-bit top-down pixels.
bool LoadPicture(DialogState& state) {
    HMODULE self = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(self, MAKEINTRESOURCEW(IDR_BACKGROUND), MAKEINTRESOURCEW(10));   // RT_RCDATA
    HGLOBAL mem = res ? LoadResource(self, res) : nullptr;
    auto* bytes = static_cast<BYTE*>(mem ? LockResource(mem) : nullptr);
    if (!bytes) return false;
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    UINT w = 0, h = 0;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_IWICImagingFactory, reinterpret_cast<void**>(&factory))) &&
              SUCCEEDED(factory->CreateStream(&stream)) &&
              SUCCEEDED(stream->InitializeFromMemory(bytes, SizeofResource(self, res))) &&
              SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
              SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
              SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, nullptr,
                                              0.0, WICBitmapPaletteTypeCustom)) &&
              SUCCEEDED(converter->GetSize(&w, &h)) && w > 0 && h > 0;
    if (ok) {
        state.picture.resize(static_cast<size_t>(w) * h * 4);
        ok = SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(state.picture.size()), state.picture.data()));
        state.picture_w = static_cast<int>(w);
        state.picture_h = static_cast<int>(h);
    }
    if (!ok) state.picture.clear();
    Release(converter);
    Release(frame);
    Release(decoder);
    Release(stream);
    Release(factory);
    return ok;
}

// The banner, after the North American logo's arrangement - BREATH and FIRE in
// capitals, a small OF between them, a taller III - in Palatino Linotype, which
// every Windows install has (the owner's choice, 2026-10-15). Our own lettering,
// not the logo's art. The sizes are the picture's pixels, set by eye.
void Banner(HDC dc, double s, int x) {
    struct Run {
        const wchar_t* text;
        int em, gap;
    };
    const Run runs[] = {{L"BREATH", 48, 12}, {L"OF", 26, 10}, {L"FIRE", 48, 12}, {L"III", 58, 0}};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    const UINT align = SetTextAlign(dc, TA_BASELINE | TA_LEFT);
    const auto px = [s](int v) { return static_cast<int>(v * s + 0.5); };
    x += px(8);
    for (const Run& r : runs) {
        HFONT font = CreateFontW(-px(r.em), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, L"Palatino Linotype");
        HGDIOBJ old = SelectObject(dc, font);
        const int n = static_cast<int>(wcslen(r.text));
        TextOutW(dc, x, px(50), r.text, n);
        SIZE size{};
        GetTextExtentPoint32W(dc, r.text, n, &size);
        x += size.cx + px(r.gap);
        SelectObject(dc, old);
        DeleteObject(font);
    }
    SetTextAlign(dc, align);
}

// The window's background: the picture on the title page, the banner's band
// over the dialog colour on Configure. The controls draw themselves over it.
void Paint(HWND dlg, const DialogState& state, HDC dc) {
    RECT client;
    GetClientRect(dlg, &client);
    const int pw = Px(state, kPictureW), ph = Px(state, kPictureH), ox = (client.right - pw) / 2;
    HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (state.page == kPageTitle) {
        FillRect(dc, &client, black);
        if (!state.picture.empty()) {
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof bi.bmiHeader;
            bi.bmiHeader.biWidth = state.picture_w;
            bi.bmiHeader.biHeight = -state.picture_h;   // top-down
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            // unsmoothed at a whole scale; smoothed at a fractional one
            if (state.scale == static_cast<int>(state.scale)) {
                SetStretchBltMode(dc, COLORONCOLOR);
            } else {
                SetStretchBltMode(dc, HALFTONE);
                SetBrushOrgEx(dc, 0, 0, nullptr);
            }
            StretchDIBits(dc, ox, 0, pw, ph, 0, 0, state.picture_w, state.picture_h,
                          state.picture.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
        }
        Banner(dc, state.scale, ox);
    } else {
        RECT band{0, 0, client.right, Px(state, kBandH)}, body{0, Px(state, kBandH), client.right, client.bottom};
        FillRect(dc, &band, black);
        FillRect(dc, &body, GetSysColorBrush(COLOR_BTNFACE));
        Banner(dc, state.scale, 0);
    }
}

// The window's size and every control's place, once, when it opens.
void Layout(HWND dlg, DialogState& state) {
    // The display's scale. The launcher is DPI-aware for the system's DPI
    // (launcher.manifest), so this is the factor its dialog units take too.
    HDC screen = GetDC(nullptr);
    state.scale = GetDeviceCaps(screen, LOGPIXELSX) / 96.0;
    ReleaseDC(nullptr, screen);
    const auto px = [&state](double v) { return Px(state, v); };

    // The sub-pages' room, in pixels: a page under sub-tabs starts lower.
    int need_w = 0, need_h = 0;
    for (int i = 0; i < kSubPageCount; ++i) {
        RECT r;
        GetWindowRect(state.pages[i], &r);
        const int top = px(kSubPages[i].label ? kSubPageY : kPageY);
        need_w = std::max(need_w, static_cast<int>(r.right - r.left) + 2 * px(kMargin));
        need_h = std::max(need_h, top + static_cast<int>(r.bottom - r.top) + px(kBottom));
    }
    const int cw = std::max(px(kPictureW), need_w), ch = std::max(px(kPictureH), need_h);
    if (cw > px(kPictureW) || ch > px(kPictureH))
        std::fprintf(stderr, "bof3x-launcher: a page needs %d x %d at this scale; the window grows to fit\n", need_w,
                     need_h);
    const DWORD style = static_cast<DWORD>(GetWindowLongW(dlg, GWL_STYLE));
    const DWORD ex = static_cast<DWORD>(GetWindowLongW(dlg, GWL_EXSTYLE));
    RECT r{0, 0, cw, ch};
    AdjustWindowRectEx(&r, style, FALSE, ex);
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    GetMonitorInfoW(MonitorFromWindow(dlg, MONITOR_DEFAULTTOPRIMARY), &mi);
    const int work_w = mi.rcWork.right - mi.rcWork.left, work_h = mi.rcWork.bottom - mi.rcWork.top;
    const int ww = r.right - r.left, wh = r.bottom - r.top;
    SetWindowPos(dlg, nullptr, mi.rcWork.left + std::max(0, (work_w - ww) / 2),
                 mi.rcWork.top + std::max(0, (work_h - wh) / 2), ww, wh, SWP_NOZORDER | SWP_NOACTIVATE);

    // The title page's buttons and the tabs in a type that grows with the
    // picture; the sub-tabs keep the dialog font, as the pages do.
    state.button_font = CreateFontW(-px(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH,
                                    L"Segoe UI");
    auto place = [&](int id, int x, int y, int w, int h, HFONT font) {
        HWND b = GetDlgItem(dlg, id);
        if (font) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        SetWindowPos(b, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    };
    const int ox = (cw - px(kPictureW)) / 2;
    for (const TitleButton& b : kTitleButtons)
        place(b.id, ox + px(b.x0), px(kTitleButtonY), px(b.x1 - b.x0), px(kTitleButtonH), state.button_font);

    // The tabs from the left, each as wide as its label; Back at the right.
    HDC dc = GetDC(dlg);
    HGDIOBJ old = SelectObject(dc, state.button_font);
    auto width = [&](const std::wstring& text, int pad, int least) {
        SIZE size{};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
        return std::max(static_cast<int>(size.cx) + px(pad), px(least));
    };
    int x = px(kMargin);
    for (int i = 0; i < kPageCount; ++i) {
        const int w = width(DialogText(dlg, IDC_TAB0 + i), 24, 64);
        place(IDC_TAB0 + i, x, px(kTabsY), w, px(kTabH), state.button_font);
        x += w + px(8);
    }
    const int back = width(DialogText(dlg, IDC_BACK), 24, 88);
    place(IDC_BACK, cw - px(kMargin) - back, px(kTabsY), back, px(kTabH), state.button_font);
    // The sub-tabs, as wide as the longest label any tab gives them (ShowPage sets them).
    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(dlg, WM_GETFONT, 0, 0)));
    int sub_w = 0;
    for (const SubPage& p : kSubPages)
        if (p.label) sub_w = std::max(sub_w, width(p.label, 20, 80));
    for (int k = 0; k < kSubTabsMax; ++k)
        place(IDC_SUB0 + k, px(kMargin) + k * (sub_w + px(6)), px(kPageY), sub_w, px(kSubTabH), nullptr);
    SelectObject(dc, old);
    ReleaseDC(dlg, dc);

    for (int i = 0; i < kSubPageCount; ++i)
        SetWindowPos(state.pages[i], nullptr, px(kMargin), px(kSubPages[i].label ? kSubPageY : kPageY), 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// The title page (kPageTitle) or one of Configure's tabs, on the sub-page the
// tab was left on.
void ShowPage(HWND dlg, DialogState& state, int page) {
    state.page = page;
    const bool title = page == kPageTitle;
    for (const TitleButton& b : kTitleButtons) ShowWindow(GetDlgItem(dlg, b.id), title ? SW_SHOW : SW_HIDE);
    for (int i = 0; i < kPageCount; ++i) {
        ShowWindow(GetDlgItem(dlg, IDC_TAB0 + i), title ? SW_HIDE : SW_SHOW);
        CheckDlgButton(dlg, IDC_TAB0 + i, i == page ? BST_CHECKED : BST_UNCHECKED);
    }
    ShowWindow(GetDlgItem(dlg, IDC_BACK), title ? SW_HIDE : SW_SHOW);
    int k = 0;   // a sub-page's place among the tab's
    for (int i = 0; i < kSubPageCount; ++i) {
        const bool mine = !title && kSubPages[i].page == page;
        ShowWindow(state.pages[i], mine && k == state.sub[page] ? SW_SHOW : SW_HIDE);
        if (!mine) continue;
        if (kSubPages[i].label && k < kSubTabsMax) {
            SetDlgItemTextW(dlg, IDC_SUB0 + k, kSubPages[i].label);
            CheckDlgButton(dlg, IDC_SUB0 + k, k == state.sub[page] ? BST_CHECKED : BST_UNCHECKED);
        }
        ++k;
    }
    for (int j = 0; j < kSubTabsMax; ++j)
        ShowWindow(GetDlgItem(dlg, IDC_SUB0 + j), k > 1 && j < k ? SW_SHOW : SW_HIDE);
    SendMessageW(dlg, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(GetDlgItem(dlg, title ? IDOK : IDC_TAB0 + page)), TRUE);
    // the window and every page and control on it, so nothing shows the face before
    RedrawWindow(dlg, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

// A sub-page's own procedure: what its controls say goes to the window, which
// knows every control by its id (Item).
INT_PTR CALLBACK PageProc(HWND page, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        return FALSE;   // the window decides the focus
    case WM_DESTROY:
        RemovePropW(page, kRubyUpProp);   // RubyUiShow's mark on the Settings page
        break;
    case WM_COMMAND:
    case WM_HSCROLL:
        SendMessageW(GetParent(page), msg, wp, lp);
        return TRUE;
    case WM_MEASUREITEM: {
        // the owner-drawn Row above the text box; sent before WM_INITDIALOG
        auto* m = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        if (m->CtlID != IDC_RUBY) break;
        const HDC dc = GetDC(page);
        const HFONT font = reinterpret_cast<HFONT>(SendMessageW(page, WM_GETFONT, 0, 0));
        const HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
        TEXTMETRICW tm;
        GetTextMetricsW(dc, &tm);
        if (old) SelectObject(dc, old);
        ReleaseDC(page, dc);
        m->itemHeight = static_cast<UINT>(tm.tmHeight + (m->itemID == static_cast<UINT>(-1) ? 4 : 2));
        return TRUE;
    }
    case WM_DRAWITEM: {
        auto* d = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (d->CtlID != IDC_RUBY) break;
        RubyDraw(*d);
        return TRUE;
    }
    default:
        break;
    }
    return FALSE;
}

// Every page into cfg: Play, Back, and before the Sources page rebuilds.
void ApplyPages(HWND dlg, DialogState& state) {
    ReadCache(dlg, *state.cfg);
    ReadBack(dlg, state);
    if (state.cheats_touched) CheatsReadBack(SubPageOf(state, IDD_PAGE_GAME), state.cfg->cheats);
    if (state.cells_touched) {
        const int layout = Selected(dlg, IDC_CT_LAYOUT);
        state.cells.layout = static_cast<input::Layout>(layout < 0 ? 0 : layout);
        BindingsFrom(state.cells, state.cfg->bindings);
        padnav::SetMap(state.cfg->bindings.pad);
    }
}

// Back to the title page. The pages are applied to cfg, so an Exit from there
// saves them (kDialogSave); Play would have anyway.
void Back(HWND dlg, DialogState& state) {
    ApplyPages(dlg, state);
    state.applied = true;
    SuitesRefresh(dlg, state);   // opt= as it now reads
    ShowPage(dlg, state, kPageTitle);
}

INT_PTR CALLBACK Proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
    switch (msg) {
    case WM_INITDIALOG: {
        padnav::Attach(dlg);
        state = reinterpret_cast<DialogState*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        for (int i = 0; i < kSubPageCount; ++i) {
            state->pages[i] = CreateDialogParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(kSubPages[i].templ), dlg,
                                                 kSubPages[i].templ == IDD_PAGE_MODS ? ModsPageProc : PageProc, 0);
            if (!state->pages[i]) {
                std::fprintf(stderr, "bof3x-launcher: page %d could not be created (error %lu)\n", i, GetLastError());
                EndDialog(dlg, -1);
                return TRUE;
            }
        }
        if (!LoadPicture(*state)) std::fprintf(stderr, "bof3x-launcher: the title page's picture could not be decoded\n");
        Populate(dlg, *state);
        CheatsPopulate(SubPageOf(*state, IDD_PAGE_GAME), state->cfg->cheats);
        state->cells = CellsFrom(state->cfg->bindings);
        ControlsPopulate(dlg, state->cells);
        Layout(dlg, *state);
        // The title bar's and the taskbar's icon: the launcher's own (launcher.rc IDI_BOF3X).
        for (const auto& [which, metric] : {std::pair{ICON_BIG, SM_CXICON}, std::pair{ICON_SMALL, SM_CXSMICON}}) {
            const int side = GetSystemMetrics(metric);
            if (HANDLE icon = LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_BOF3X), IMAGE_ICON, side, side, 0))
                SendMessageW(dlg, WM_SETICON, which, reinterpret_cast<LPARAM>(icon));
        }
        ShowPage(dlg, *state, g_reopen);
        g_reopen = kPageTitle;
        // Started from a terminal or a script, the launcher's window can open
        // behind the window that started it.
        SetForegroundWindow(dlg);
        return FALSE;   // ShowPage set the focus
    }
    case kMsgModsChanged:   // the Mods page added, built or removed one
        if (state) RubyRefresh(dlg, *state);
        return TRUE;
    case kMsgModsReadingRowAdded:
        // A reading-row mod added: Extras' toggle on and the box shown at once
        // (the owner's call, 2026-10-22). The add is on disk already, so the
        // toggle is kept too: into cfg, and the window saves cfg on any way out
        // (kDialogSave from the caption's close, as after Back). A remove
        // leaves the toggle as it is.
        if (state) {
            Check(dlg, IDC_EXTRA_RUBY, true);
            RubyUiShow(dlg, true);
            state->cfg->ruby_ui = true;
            state->cfg->ruby_ui_named = true;
            state->applied = true;
        }
        return TRUE;
    case WM_ERASEBKGND:
        if (!state) break;
        Paint(dlg, *state, reinterpret_cast<HDC>(wp));
        SetWindowLongPtrW(dlg, DWLP_MSGRESULT, TRUE);
        return TRUE;
    case WM_TIMER:
        if (wp == padnav::kTimer) {
            padnav::Tick(dlg);
            return TRUE;
        }
        break;
    case WM_HSCROLL:
        if (state && CheatsSliderMoved(SubPageOf(*state, IDD_PAGE_GAME), reinterpret_cast<HWND>(lp)))
            state->cheats_touched = true;
        return TRUE;
    case WM_CLOSE:
        // The caption's close button leaves from either face; Escape and the
        // pad's circle (IDCANCEL) step back from Configure first.
        EndDialog(dlg, state && state->applied ? kDialogSave : kDialogClosed);
        return TRUE;
    case WM_DESTROY:
        if (state && state->button_font) {
            DeleteObject(state->button_font);
            state->button_font = nullptr;
        }
        break;
    case WM_COMMAND: {
        if (!state) break;
        const int id = LOWORD(wp);
        switch (id) {
        case IDOK:
            ApplyPages(dlg, *state);
            EndDialog(dlg, kDialogPlay);
            return TRUE;
        case IDCANCEL:
            if (state->page != kPageTitle) Back(dlg, *state);
            else EndDialog(dlg, state->applied ? kDialogSave : kDialogClosed);
            return TRUE;
        case IDC_CONFIGURE:
            ShowPage(dlg, *state, kPageSettings);
            return TRUE;
        case IDC_BACK:
            Back(dlg, *state);
            return TRUE;
        case IDC_LANGUAGE:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                SuitesRefresh(dlg, *state);   // a names layer's note follows the language
                RubyRefresh(dlg, *state);     // the rows above it too
                PresetRefresh(dlg, *state);   // and what the preset turns off
            }
            return TRUE;
        case IDC_PRESET:
            if (HIWORD(wp) == CBN_SELCHANGE) PresetRefresh(dlg, *state);
            return TRUE;
        case IDC_RUBY:
            if (HIWORD(wp) == CBN_SELCHANGE) RubyPicked(dlg, *state);
            return TRUE;
        case IDC_EXTRA_RUBY:
            if (HIWORD(wp) == BN_CLICKED) RubyUiShow(dlg, Checked(dlg, IDC_EXTRA_RUBY));
            return TRUE;
        case IDC_CACHE:
            if (HIWORD(wp) == EN_CHANGE) {
                CacheNote(dlg);
                if (state->cfg) {
                    SuitesRefresh(dlg, *state);   // another cache, another manifest
                    MusicRefresh(dlg, *state);    // DIV-0150: which sources it holds
                    RubyRefresh(dlg, *state);     // which reading-row layers it holds
                }
            }
            return TRUE;
        case IDC_CACHEBROWSE:
            BrowseCache(dlg);
            return TRUE;
        case IDC_FILTER:
            if (HIWORD(wp) == CBN_SELCHANGE) Enable(dlg, IDC_LOOKOPTIONS, Selected(dlg, IDC_FILTER) == 2);
            return TRUE;
        case IDC_LOOKOPTIONS:
            SatpixieOptions(dlg, state->cfg->sp);
            return TRUE;
        case IDC_CH_STEAL:
        case IDC_CH_INFO:
            if (HIWORD(wp) == BN_CLICKED) state->cheats_touched = true;
            return TRUE;
        case IDC_CH_DEFAULTS:
            CheatsPopulate(SubPageOf(*state, IDD_PAGE_GAME), Config::Cheats{});
            state->cheats_touched = true;
            return TRUE;
        case IDC_CT_LAYOUT:
            if (HIWORD(wp) == CBN_SELCHANGE) state->cells_touched = true;
            return TRUE;
        case IDC_CT_DEFAULTS:
            state->cells = CellsFrom(input::Bindings::Defaults());
            ControlsPopulate(dlg, state->cells);
            state->cells_touched = true;
            return TRUE;
        case IDC_MUSICPLAYER: {
            // the cache folder as the box has it now, saved or not
            std::wstring cache = DialogText(dlg, IDC_CACHE);
            const size_t b = cache.find_first_not_of(L" \t"), e = cache.find_last_not_of(L" \t");
            cache = b == std::wstring::npos ? std::wstring() : cache.substr(b, e - b + 1);
            if (cache.empty()) cache = HomeCacheDefault();   // the home's own, as ConfigCacheRoot
            MusicPlayerRun(dlg, state->game_dir, cache, DialogDataRoot(dlg), &state->cfg->opt, &state->chord);
            return TRUE;
        }
        case IDC_SOURCES:
            // A rebuilt install may be another game folder and cache: the
            // launcher reads them again and reopens the window on this page.
            if (SourcesDialogRun(dlg, *state->cfg, false)) {
                ApplyPages(dlg, *state);
                g_reopen = kPageData;
                EndDialog(dlg, kDialogSourcesChanged);
            }
            return TRUE;
        default:
            break;
        }
        if (HIWORD(wp) != BN_CLICKED) break;
        if (id >= IDC_TAB0 && id < IDC_TAB0 + kPageCount) {
            ShowPage(dlg, *state, id - IDC_TAB0);
            return TRUE;
        }
        if (id >= IDC_SUB0 && id < IDC_SUB0 + kSubTabsMax && state->page != kPageTitle) {
            state->sub[state->page] = id - IDC_SUB0;
            ShowPage(dlg, *state, state->page);
            SendMessageW(dlg, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(GetDlgItem(dlg, id)), TRUE);
            return TRUE;
        }
        // A Controls cell: four columns 20 ids apart from IDC_CT_KEY1 (KEY1,
        // KEY2, PAD1, PAD2 - resource.h), id = column base + row.
        if (id >= IDC_CT_KEY1 && id < IDC_CT_KEY1 + 4 * 20) {
            const int kind = (id - IDC_CT_KEY1) / 20, row = (id - IDC_CT_KEY1) % 20;
            if (row < input::kActionCount) {
                CaptureInto(dlg, state->cells, row, kind & 1, kind < 2);
                ControlsRefresh(dlg, state->cells);
                state->cells_touched = true;
                SetFocus(Item(dlg, id));
                return TRUE;
            }
        }
        if (SuiteRadioClicked(*state, id)) return TRUE;
        break;
    }
    default:
        break;
    }
    return FALSE;
}

}  // namespace

ConfigDialogResult ConfigDialogRun(const std::wstring& game_dir, Config& cfg) {
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    // WIC, for the title page's picture.
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    DialogState state;
    state.cfg = &cfg;
    state.languages = ConfigLanguagesAvailable(game_dir, ConfigCacheDataRoot(cfg));
    state.game_dir = game_dir;
    state.original = ConfigOriginalAvailable(game_dir, ConfigCacheDataRoot(cfg));  // DIV-0102
    padnav::SetMap(cfg.bindings.pad);
    const INT_PTR result = DialogBoxParamW(GetModuleHandleW(nullptr),
                                           MAKEINTRESOURCEW(IDD_CONFIG), nullptr, Proc,
                                           reinterpret_cast<LPARAM>(&state));
    if (SUCCEEDED(co)) CoUninitialize();
    if (result == -1) {
        // Starting the game with settings the player never saw would be worse
        // than not starting: say so and let them try again.
        MessageBoxW(nullptr, L"The launcher's window could not be created.", L"bof3x-launcher",
                    MB_OK | MB_ICONERROR);
        return kDialogClosed;
    }
    switch (result) {
    case kDialogPlay:
    case kDialogSourcesChanged:
    case kDialogSave:
        return static_cast<ConfigDialogResult>(result);
    default:
        return kDialogClosed;
    }
}

}  // namespace bof3x

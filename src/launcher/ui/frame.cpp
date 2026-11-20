// launcher/ui/frame.h: the title page and Configure's tabs.
#include "launcher/ui/frame.h"

#include "launcher/ui/assets.h"
#include "launcher/ui/pages.h"
#include "launcher/ui/ui.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>

namespace bof3x::ui {

namespace {

// The picture is 640 x 480 with black bands above and below the scene, as the
// game drew it; the banner sits in the top band and the title page's buttons
// in the bottom one (config_dialog.cpp, the same numbers).
constexpr float kPictureW = 640, kPictureH = 480;
constexpr float kBandH = 60, kTabsY = 70, kTabH = 23, kPageY = kTabsY + kTabH + 8, kSubTabH = 22,
                kSubPageY = kPageY + kSubTabH + 6, kSheetPageY = kSubPageY + kSubTabH + 6, kMargin = 13, kBottom = 8;
constexpr float kTitleButtonY = 440, kTitleButtonH = 23;
constexpr float kTextEm = 12;

constexpr Color kBlack{0, 0, 0}, kWhite{255, 255, 255};

const char* const kTabs[kFrameTabs] = {"Settings", "Controls", "Music", "Game Options", "Mods", "Data"};
const char* const kTabNames[kFrameTabs] = {"settings", "controls", "music", "game", "mods", "data"};

// A tab's pages, as config_dialog.cpp's kSubPages: a tab with more than one
// shows a sub-tab button for each `label`; a sub-tab with more than one page
// shows a third row of buttons, one for each `sheet`. `body` draws the page
// (launcher/ui/pages.h); null: the page is not in this launcher yet.
struct SubPage {
    int tab;
    const char* label;
    const char* sheet;
    void (*body)(Pages&);
};
constexpr SubPage kSubPages[] = {
    {0, "Settings", nullptr, SettingsPage},
    {0, "Preset", nullptr, PresetPage},
    {1, "Directions and buttons", nullptr, ControlsPage1},
    {1, "Shoulders, Start and Select", nullptr, ControlsPage2},
    {1, "Function keys", nullptr, FnKeysPage},
    {2, nullptr, nullptr, MusicPage},
    {3, "Graphics and audio", nullptr, VersionsGraphicsPage},
    {3, "Data", nullptr, VersionsDataPage},
    {3, "Cheats", "Page 1", CheatsPage1},
    {3, "Cheats", "Page 2", CheatsPage2},
    {3, "Solo character", nullptr, SoloPage},
    {4, "Data", nullptr, ModsPage},
    {4, "Extras", "Page 1", ExtrasPage},
    {4, "Extras", "Page 2", NewGamePage},
    {5, nullptr, nullptr, DataPage},
};

bool SameSubTab(const SubPage& a, const SubPage& b) {
    return a.tab == b.tab && a.label && b.label && !std::strcmp(a.label, b.label);
}

Picture* g_background = nullptr;
bool g_background_tried = false;

// The banner, after the North American logo's arrangement - BREATH and FIRE in
// capitals, a small OF between them, a taller III. Our own lettering, not the
// logo's art; the sizes are config_dialog.cpp's, set by the owner's eye.
void Banner(float x) {
    struct Run {
        const char* text;
        float em, gap;
    };
    constexpr Run runs[] = {{"BREATH", 48, 12}, {"OF", 26, 10}, {"FIRE", 48, 12}, {"III", 58, 0}};
    x += 8;
    for (const Run& r : runs) {
        Text(Face::kBanner, r.em, x, 50, kWhite, r.text);
        x += TextWidth(Face::kBanner, r.em, r.text) + r.gap;
    }
}

float ButtonWidth(const char* label, float pad, float least) {
    return std::max(TextWidth(Face::kText, kTextEm, label) + pad, least);
}

FrameAsk Title(FrameState& state, Pages& pages) {
    if (!g_background_tried) {
        g_background_tried = true;
        g_background = PictureFromPng(kUiBackgroundPng, kUiBackgroundPngSize);
    }
    Fill(0, 0, kPictureW, kPictureH, kBlack);
    PictureDraw(g_background, 0, 0, kPictureW, kPictureH);
    Banner(0);

    FrameAsk ask = FrameAsk::kNone;
    // The buttons' places are the owner's mock-up's (2026-10-15).
    Place(236, kTitleButtonY);
    Size(88, kTitleButtonH);
    if (Button("Play")) ask = FrameAsk::kPlay;
    if (state.opened) DefaultFocus();
    Place(338, kTitleButtonY);
    Size(88, kTitleButtonH);
    if (Button("Configure")) {
        state.page = 0;
        state.opened = true;
        return FrameAsk::kNone;
    }
    Place(439, kTitleButtonY);
    Size(88, kTitleButtonH);
    if (Button("Jukebox")) {   // stage P5: in place of the title page until its Close
        JukeboxBegin(pages);
        return FrameAsk::kNone;
    }
    Place(541, kTitleButtonY);
    Size(88, kTitleButtonH);
    if (Button("Exit")) ask = FrameAsk::kExit;
    state.opened = false;
    if (Pressed(Press::kStart)) ask = FrameAsk::kPlay;
    if (Pressed(Press::kBack)) ask = FrameAsk::kExit;
    return ask;
}

FrameAsk Configure(FrameState& state, Pages& pages) {
    Fill(0, 0, kPictureW, kBandH, kBlack);
    Fill(0, kBandH, kPictureW, kPictureH - kBandH, Paper());
    Banner(0);

    // SatPixie's options, from Settings' Options...: in place of the tabs and
    // their page until its own buttons close it (the Win32 launcher shows a
    // window of its own). Escape and the pad's circle close it as its Cancel.
    if (pages.satpixie_open) {
        AreaBegin("##satpixie", kMargin, kTabsY, kPictureW - 2 * kMargin, kPictureH - kTabsY - kBottom);
        SatpixiePage(pages);
        AreaEnd();
        if (Pressed(Press::kBack)) pages.satpixie_open = false;
        return FrameAsk::kNone;
    }
    // The bind capture, from a Controls cell: in place of the tabs and their
    // page until it has its answer (the Win32 launcher shows a window of its
    // own). Escape, the pad's circle and Start are its inputs, not Back or Play.
    if (pages.capture_open) {
        AreaBegin("##capture", kMargin, kTabsY, kPictureW - 2 * kMargin, kPictureH - kTabsY - kBottom);
        CapturePage(pages);
        AreaEnd();
        return FrameAsk::kNone;
    }
    // The save editor (stage P5), from Mods' Save Editor...: in place of the
    // tabs and their page until its Close (the Win32 launcher shows a window of
    // its own). Escape and the pad's circle are its Close; Start is not Play.
    if (pages.saves_open) {
        AreaBegin("##saves", kMargin, kTabsY, kPictureW - 2 * kMargin, kPictureH - kTabsY - kBottom);
        SavesPage(pages);
        AreaEnd();
        return FrameAsk::kNone;
    }

    // The tabs from the left, each as wide as its label; Back at the right.
    const int tab = state.page;
    float x = kMargin;
    for (int i = 0; i < kFrameTabs; ++i) {
        const float w = ButtonWidth(kTabs[i], 24, 64);
        Place(x, kTabsY);
        Size(w, kTabH);
        // A tab and a sub-tab can have one label (Settings, Data): each row's names end with the row.
        if (Tab((std::string(kTabs[i]) + "##tab").c_str(), i == tab)) {
            state.page = i;
            state.opened = true;
        }
        if (i == tab && state.opened) DefaultFocus();
        x += w + 8;
    }
    const float back_w = ButtonWidth("Back", 24, 88);
    Place(kPictureW - kMargin - back_w, kTabsY);
    Size(back_w, kTabH);
    const bool back = Button("Back");
    state.opened = false;
    if (back || Pressed(Press::kBack)) {
        PagesApply(pages);   // an Exit from the title page then saves them; Play would have anyway
        state.page = -1;
        state.opened = true;
        return FrameAsk::kNone;
    }
    if (state.page != tab) return FrameAsk::kNone;   // another tab: its rows from the next frame

    // The tab's sub-tabs from the left, each as wide as the tab's longest
    // label, and the third row likewise under them.
    float sub_w = 0, sheet_w = 0;
    for (const SubPage& p : kSubPages) {
        if (p.tab != tab) continue;
        if (p.label) sub_w = std::max(sub_w, ButtonWidth(p.label, 20, 80));
        if (p.sheet) sheet_w = std::max(sheet_w, ButtonWidth(p.sheet, 20, 80));
    }
    // A sub-tab or a page the tab has not (a check's --sub): the first.
    int subs = 0, sheets = 0;
    for (int i = 0; i < static_cast<int>(std::size(kSubPages)); ++i) {
        const SubPage& p = kSubPages[i];
        if (p.tab != tab) continue;
        const bool first_of_sub = i == 0 || !SameSubTab(kSubPages[i - 1], p);
        if (first_of_sub) ++subs;
        if (subs - 1 == state.sub[tab]) ++sheets;
    }
    if (state.sub[tab] < 0 || state.sub[tab] >= subs) state.sub[tab] = 0, state.sheet[tab] = 0;
    else if (state.sheet[tab] < 0 || state.sheet[tab] >= sheets) state.sheet[tab] = 0;

    const SubPage* shown = nullptr;
    int place = -1;   // the sub-tab's place among the tab's
    x = kMargin;
    for (int i = 0; i < static_cast<int>(std::size(kSubPages)); ++i) {
        const SubPage& p = kSubPages[i];
        if (p.tab != tab) continue;
        const bool first_of_sub = i == 0 || !SameSubTab(kSubPages[i - 1], p);
        if (first_of_sub) ++place;
        const bool sub_on = place == state.sub[tab];
        if (p.label && first_of_sub) {
            Place(x, kPageY);
            Size(sub_w, kSubTabH);
            if (Tab((std::string(p.label) + "##sub").c_str(), sub_on)) {
                state.sub[tab] = place;
                state.sheet[tab] = 0;
            }
            x += sub_w + 6;
        }
        if (!sub_on) continue;
        int sheet = 0;   // this page's place among the sub-tab's pages
        for (int k = i; k > 0 && SameSubTab(kSubPages[k - 1], p); --k) ++sheet;
        if (p.sheet) {
            Place(kMargin + sheet * (sheet_w + 6), kSubPageY);
            Size(sheet_w, kSubTabH);
            if (Tab((std::string(p.sheet) + "##sheet").c_str(), sheet == state.sheet[tab])) state.sheet[tab] = sheet;
        }
        if (sheet == state.sheet[tab]) shown = &p;
    }
    // The save editor's button, after the Mods tab's sub-tabs (the owner's place for it, 2026-11-10).
    if (tab == 4) {
        Place(x + 6, kPageY);
        Size(ButtonWidth("Save Editor...", 20, 80), kSubTabH);
        if (Button("Save Editor...")) {
            SavesBegin(pages);   // shown from the next frame, in place of the tabs
            return FrameAsk::kNone;
        }
    }

    if (shown) {
        const float top = shown->sheet ? kSheetPageY : shown->label ? kSubPageY : kPageY;
        AreaBegin("##page", kMargin, top, kPictureW - 2 * kMargin, kPictureH - top - kBottom);
        if (shown->body) shown->body(pages);
        else PageNotYet();
        AreaEnd();
    }
    return Pressed(Press::kStart) ? FrameAsk::kPlay : FrameAsk::kNone;
}

}  // namespace

bool FrameOpen(FrameState& state, const char* name) {
    if (!std::strcmp(name, "title")) {
        state.page = -1;
        state.opened = true;
        return true;
    }
    for (int i = 0; i < kFrameTabs; ++i) {
        if (std::strcmp(name, kTabNames[i])) continue;
        state.page = i;
        state.opened = true;
        return true;
    }
    return false;
}

// The Sources page (stage P4), in place of the title page or the tabs, with
// the banner above it, until its Close (the Win32 launcher shows a window of
// its own): from Data's Sources..., or before the window (main.cpp).
static FrameAsk SourcesFace(Pages& pages) {
    Fill(0, 0, kPictureW, kBandH, kBlack);
    Fill(0, kBandH, kPictureW, kPictureH - kBandH, Paper());
    Banner(0);
    AreaBegin("##sources", kMargin, kTabsY, kPictureW - 2 * kMargin, kPictureH - kTabsY - kBottom);
    SourcesPage(pages);
    AreaEnd();
    return FrameAsk::kNone;
}

// The Jukebox (stage P5), in place of the title page with the banner above
// it, until its Close (the Win32 launcher shows a window of its own); the
// title page is then shown again with the focus on Play, as when it opens.
static FrameAsk JukeboxFace(Pages& pages, FrameState& state) {
    Fill(0, 0, kPictureW, kBandH, kBlack);
    Fill(0, kBandH, kPictureW, kPictureH - kBandH, Paper());
    Banner(0);
    AreaBegin("##jukebox", kMargin, kTabsY, kPictureW - 2 * kMargin, kPictureH - kTabsY - kBottom);
    JukeboxPage(pages);
    AreaEnd();
    if (!pages.jukebox_open) state.opened = true;   // back on the title page: its focus as when it opens
    return FrameAsk::kNone;
}

FrameAsk FrameDraw(FrameState& state, Pages& pages) {
    // The tools' lines and ends, whichever page is shown, as the Win32 pages
    // take their messages while hidden.
    SourcesPoll(pages);
    ModsPoll(pages);
    if (pages.sources_open) return SourcesFace(pages);
    if (pages.jukebox_open) return JukeboxFace(pages, state);
    return state.page < 0 ? Title(state, pages) : Configure(state, pages);
}

}  // namespace bof3x::ui

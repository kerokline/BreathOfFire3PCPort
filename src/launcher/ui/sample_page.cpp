// A sample of every widget of launcher/ui/ui.h, in the arrangement of the
// Win32 launcher's Settings page (launcher.rc IDD_PAGE_SETTINGS), for the
// look of stage P1 (docs/launcher-portable.md): the owner tunes the look off
// a capture of the title page and of this page. It reads and changes no
// setting. Stage P2's Settings page replaces it, and this file goes then.
#include "launcher/ui/frame.h"
#include "launcher/ui/ui.h"

#include <string>
#include <vector>

namespace bof3x::ui {

namespace {

// The Win32 page is laid out in dialog units of Segoe UI 9: 1.75 of the
// picture's pixels across and 1.875 down.
constexpr float X(float du) { return du * 1.75f; }
constexpr float Y(float du) { return du * 1.875f; }

struct Sample {
    int language = 0, look = 1, display = 1, wide = 0, volume = 7, song = 1, order = 0;
    bool snap = true, smooth = false, background = false, show = true;
    std::string folder = "D:\\Games\\bof3x\\cache";
};
Sample g_sample;

}  // namespace

void SamplePage() {
    Sample& s = g_sample;
    const std::vector<std::string> languages = {"English (United States)", "English (Europe)", "Japanese"};
    const std::vector<std::string> looks = {"Smooth - bilinear (original)", "Sharp - point (no blur)",
                                            "CRT effect (SatPixie)"};
    const std::vector<std::string> displays = {"Full screen (F8 changes to a window)",
                                               "Window (F8 changes to full screen)"};
    const std::vector<std::string> wides = {"Off - 4:3, 320 x 240 (original)", "16:9 - 426 x 240", "21:9 - 560 x 240"};
    const std::vector<std::string> songs = {"First line of a list", "Second line of a list", "Third line of a list",
                                            "Fourth line of a list"};

    GroupBegin("Settings", 0, 0, X(196), Y(164));
    Place(X(9), Y(17));
    Label("Language:");
    Place(X(50), Y(15));
    Size(X(140));
    Combo("language", &s.language, languages);

    Place(X(9), Y(33));
    Label("Look:");
    Place(X(50), Y(31));
    Size(X(100));
    Combo("look", &s.look, looks);
    Place(X(153), Y(31));
    Size(X(37), 23);
    Grey("Only the CRT effect has options.");
    Button("Options...");

    Place(X(9), Y(49));
    Label("Display:");
    Place(X(50), Y(47));
    Size(X(140));
    Combo("display", &s.display, displays);

    Place(X(9), Y(65));
    Label("Widescreen:");
    Place(X(50), Y(63));
    Size(X(140));
    Combo("wide", &s.wide, wides);

    Place(X(7), Y(80));
    Check("Integer scaling (2x, 3x, 4x ...)", &s.snap);
    Place(X(7), Y(93));
    Check("Smooth motion: 60 frames each second (experimental)", &s.smooth);
    Place(X(7), Y(106));
    Grey("A sample of a setting that this install cannot use.");
    Check("Keep the game running when its window is not active", &s.background);
    Place(X(7), Y(119));
    Check("Show this window at start", &s.show);
    Place(X(19), Y(131));
    Size(X(172));
    Label("Off: the game starts immediately. To show this window again, start bof3x-launcher --config.");
    GroupEnd();

    GroupBegin("Other widgets", X(202), 0, X(148), Y(164));
    Place(X(9), Y(17));
    Label("Order:");
    Place(X(42), Y(16));
    Radio("By name", &s.order, 0);
    Place(X(88), Y(16));
    Radio("By number", &s.order, 1);

    Place(X(9), Y(33));
    Label("Volume:");
    Place(X(50), Y(31));
    Size(X(90));
    Slider("volume", &s.volume, 0, 10);

    Place(X(9), Y(49));
    Label("Folder:");
    Place(X(50), Y(47));
    Size(X(90));
    Edit("folder", &s.folder);

    Place(X(9), Y(64));
    Size(X(131), Y(46));
    List("songs", &s.song, songs);

    Place(X(9), Y(116));
    Size(X(131), 10);
    Progress(0.6f);

    Place(X(9), Y(128));
    Size(X(63), 23);
    Button("Browse...");
    Place(X(77), Y(128));
    Size(X(63), 23);
    Grey("A sample of a button that cannot be used.");
    Button("Remove");
    GroupEnd();

    Place(0, Y(167));
    Label("A sample of the widgets. It changes no setting.");
}

}  // namespace bof3x::ui

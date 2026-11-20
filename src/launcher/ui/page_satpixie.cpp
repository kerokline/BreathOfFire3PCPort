// SatPixie's options, the CRT look's parameters (launcher.rc IDD_SATPIXIE;
// DIV-0043). config_dialog.cpp's kSliders, kToggles, SliderSteps,
// ShowSliderValue, SatpixiePopulate, SatpixieReadBack and SatpixieProc, less
// the windows. The texts are the Win32 window's, word for word but two
// check boxes' labels (kToggles).
//
// The Win32 launcher shows a window of its own over the settings; this one
// shows the page in place of the tabs while Pages::satpixie_open is true
// (frame.cpp). As there: the boxes are filled from the settings each time it
// opens, OK writes them into the settings (Config::sp) and closes, Cancel -
// and Escape or the pad's circle, frame.cpp - closes and writes nothing, and
// Defaults fills the boxes with the defaults and writes nothing.
//
// The Win32 window is 300 x 252 dialog units, taller than the page area
// (614 x 402 pixels, 214 dialog units), so it is laid out tighter here: the
// window's caption is a line at the top; the Amounts rows are 14 dialog units
// apart (16 there) and each value text is shown in its slider (beside it
// there); the Effects rows are 11 apart (14 there), and the sentence under
// the mask is under the mask box; two check boxes' labels lose their words in
// brackets, so the eight are four rows of two (five rows there), and
// Defaults, OK and Cancel are a row under the boxes, as there (the owner,
// 2026-11-20).
#include "launcher/ui/pages.h"

#include "launcher/ui/ui.h"

#include <cstdio>

namespace bof3x::ui {

namespace {

// A slider: its step and range as the Win32 window's (kSliders), its name
// for a check and its text.
struct SliderRow {
    const char* name;
    const char* text;
    float Config::Satpixie::*value;
    float lo, hi, step;
    const char* format;
};
constexpr SliderRow kSliders[] = {
    {"modulate", "Accumulate modulation", &Config::Satpixie::modulate, 0.0f, 1.0f, 0.01f, "%.2f"},
    {"gamma", "Gamma (natural vision)", &Config::Satpixie::gamma, 1.8f, 2.6f, 0.1f, "%.1f"},
    {"chroma", "Chroma strength", &Config::Satpixie::chroma, 0.0f, 5.0f, 0.1f, "%.1f"},
    {"blurx", "Horizontal blur", &Config::Satpixie::blur_x, 0.0f, 5.0f, 0.25f, "%.2f"},
    {"blury", "Vertical blur", &Config::Satpixie::blur_y, 0.0f, 5.0f, 0.25f, "%.2f"},
};
constexpr int kSliderCount = static_cast<int>(sizeof kSliders / sizeof kSliders[0]);

// A check box (kToggles) and its place in the Effects box, in dialog units of the box.
struct ToggleRow {
    const char* text;
    bool Config::Satpixie::*value;
    float x, y;
};
// Two labels are the Win32 window's less their words in brackets (the owner,
// 2026-11-20): "Natural vision (neutral colours, gamma above)" and "Overscan
// crop (trims the picture's edges)". That makes four rows of two, and room
// for the buttons under the boxes.
constexpr ToggleRow kToggles[] = {
    {"Natural vision", &Config::Satpixie::natural, 7, 14},
    {"Overscan crop", &Config::Satpixie::overscan, 175, 14},
    {"Ghosting", &Config::Satpixie::ghosting, 7, 25},
    {"Chromatic aberration", &Config::Satpixie::chroma_on, 175, 25},
    {"Vignette", &Config::Satpixie::vignette, 7, 36},
    {"Vignette shaped for 4:3 only", &Config::Satpixie::vignette_43, 175, 36},
    {"Interference wiggle", &Config::Satpixie::wiggle, 7, 47},
    {"Rolling scanlines", &Config::Satpixie::scanroll, 175, 47},
};
constexpr int kToggleCount = static_cast<int>(sizeof kToggles / sizeof kToggles[0]);

struct Boxes {
    int pos[kSliderCount] = {};      // each slider's step, 0 .. SliderSteps
    float shown[kSliderCount] = {};  // the value its text shows (ShowSliderValue)
    bool on[kToggleCount] = {};
    int mask = 0;
    bool live = false;   // the page is open and its boxes are filled
    bool focus = false;  // the page opened this frame
};
Boxes g_boxes;

int SliderSteps(const SliderRow& s) { return static_cast<int>((s.hi - s.lo) / s.step + 0.5f); }

// A slider's step for a value, as TBM_SETPOS takes it: the nearest step,
// held to the slider's range.
int StepOf(const SliderRow& s, float v) {
    const float x = (v - s.lo) / s.step + 0.5f;
    if (!(x >= 0)) return 0;
    return x >= static_cast<float>(SliderSteps(s)) ? SliderSteps(s) : static_cast<int>(x);
}

float ValueOf(const SliderRow& s, int pos) { return s.lo + static_cast<float>(pos) * s.step; }

// SatpixiePopulate: the text of a slider shows the value as given, until the
// slider moves.
void FillBoxes(const Config::Satpixie& sp) {
    Boxes& b = g_boxes;
    for (int i = 0; i < kSliderCount; ++i) {
        b.pos[i] = StepOf(kSliders[i], sp.*kSliders[i].value);
        b.shown[i] = sp.*kSliders[i].value;
    }
    for (int i = 0; i < kToggleCount; ++i) b.on[i] = sp.*kToggles[i].value;
    b.mask = sp.mask >= 0 && sp.mask <= 2 ? sp.mask : 0;
}

// SatpixieReadBack.
void StoreBoxes(Config::Satpixie& sp) {
    const Boxes& b = g_boxes;
    for (int i = 0; i < kSliderCount; ++i) sp.*kSliders[i].value = ValueOf(kSliders[i], b.pos[i]);
    for (int i = 0; i < kToggleCount; ++i) sp.*kToggles[i].value = b.on[i];
    sp.mask = b.mask >= 0 && b.mask <= 2 ? b.mask : 0;
}

void CloseOptions(Pages& p) {
    g_boxes.live = false;
    p.satpixie_open = false;
}

}  // namespace

void SatpixieOpen(Pages& p) {
    g_boxes = Boxes{};
    FillBoxes(p.cfg->sp);
}

void SatpixiePage(Pages& p) {
    Boxes& b = g_boxes;
    if (!b.live) {   // opened from Settings' Options...: the boxes from the settings as they are now
        FillBoxes(p.cfg->sp);
        b.live = true;
        b.focus = true;
    }

    Place(0, 0);
    Label("CRT options (SatPixie)");

    GroupBegin("Amounts", 0, Dy(12), Dx(350), Dy(87));
    for (int i = 0; i < kSliderCount; ++i) {
        const SliderRow& s = kSliders[i];
        const float y = 11 + 14 * static_cast<float>(i);
        Place(Dx(7), Dy(y + 3));
        Label(s.text);
        char text[32];
        std::snprintf(text, sizeof text, s.format, static_cast<double>(b.shown[i]));
        Place(Dx(99), Dy(y));
        Size(Dx(244));
        if (SliderShowing((std::string("##") + s.name).c_str(), &b.pos[i], 0, SliderSteps(s), text))
            b.shown[i] = ValueOf(s, b.pos[i]);
    }
    GroupEnd();

    GroupBegin("Effects", 0, Dy(102), Dx(350), Dy(94));
    for (int i = 0; i < kToggleCount; ++i) {
        Place(Dx(kToggles[i].x), Dy(kToggles[i].y));
        Check(kToggles[i].text, &b.on[i]);
    }
    Place(Dx(7), Dy(61));
    Label("Shadow mask:");
    Place(Dx(69), Dy(59));
    Size(Dx(120));
    Combo("##mask", &b.mask, {"Off", "Brightness lines", "Colour stripes"});
    Place(Dx(19), Dy(74));
    Size(Dx(324));
    Label("On: the side bands of a wide picture stay bright. Off: the vignette covers the full picture.");
    GroupEnd();

    // The buttons in a row under the boxes (the owner, 2026-11-20): Defaults
    // at the left, OK and Cancel at the right. OK is the window's default
    // button: it takes the focus when the page opens.
    Place(0, Dy(200));
    Size(Dx(52), 23);
    if (Button("Defaults")) FillBoxes(Config::Satpixie{});
    Place(Dx(350 - 52 - 4 - 52), Dy(200));
    Size(Dx(52), 23);
    const bool ok = Button("OK");
    if (b.focus) DefaultFocus();
    b.focus = false;
    Place(Dx(350 - 52), Dy(200));
    Size(Dx(52), 23);
    const bool cancel = Button("Cancel");

    if (ok) {
        StoreBoxes(p.cfg->sp);
        CloseOptions(p);
    } else if (cancel || Pressed(Press::kBack)) {
        CloseOptions(p);   // Escape and the pad's circle: frame.cpp closes the page as this Cancel
    }
}

}  // namespace bof3x::ui

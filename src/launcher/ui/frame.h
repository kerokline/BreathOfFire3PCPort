// The drawn launcher's window (docs/launcher-portable.md, stage P1): the title
// page and Configure with its tabs, as the Win32 window of
// docs/launcher-settings.md section 2a, drawn through launcher/ui/ui.h. The
// places are that window's, in the picture's pixels (config_dialog.cpp).
#pragma once

namespace bof3x::ui {

// What the player asked of the window in this frame.
enum class FrameAsk {
    kNone,
    kPlay,   // Play, or the pad's Start
    kExit,   // Exit, or Escape or the pad's circle on the title page
};

constexpr int kFrameTabs = 6;

struct FrameState {
    int page = -1;                 // -1: the title page; else Configure's tab
    int sub[kFrameTabs] = {};      // each tab's sub-tab shown, its place among the tab's
    int sheet[kFrameTabs] = {};    // each tab's page shown within that sub-tab (the third row)
    bool opened = true;            // the face changed: its first control takes the focus
};

// The face a name asks for: "title", or a tab's name in small letters
// ("settings", "controls", "music", "game", "mods", "data"). False: no such face.
bool FrameOpen(FrameState& state, const char* name);

// One frame of the window, between ui::FrameBegin and ui::FrameEnd.
FrameAsk FrameDraw(FrameState& state);

// A sample of every widget, in the Settings page's arrangement, for the look
// (sample_page.cpp). It changes no setting; stage P2's Settings page replaces it.
void SamplePage();

}  // namespace bof3x::ui

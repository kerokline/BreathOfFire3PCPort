// The drawn launcher's widgets (docs/launcher-portable.md, stage P1): one set
// of windows for Windows and Linux. The pages are written against this header
// and name no library: ui_imgui.cpp draws it with Dear ImGui on SDL3's
// renderer today, and a widget set of our own (the document's way F) could
// take its place behind the same names.
//
// Every place and size is in the picture's pixels, as the Win32 window's are
// (docs/launcher-settings.md 2a): the window is 640 x 480 of them, and each is
// `Scale()` pixels of the display - 1 at 100 %, 2 at 200 %. Every text is UTF-8.
//
// A frame is immediate: between FrameBegin and FrameEnd the page calls a
// widget for each control it shows, each frame, and a widget returns true in
// the frame its value changed (a button: in the frame it was pressed). The
// keyboard and the pad move between the widgets and use them with no code in
// the page: the arrows or the d-pad move, Enter, Space or the pad's cross
// use, Escape or the pad's circle leave an open list.
#pragma once

#include <string>
#include <vector>

namespace bof3x::ui {

struct Color {
    unsigned char r = 0, g = 0, b = 0, a = 255;
};

// ---- The window ---------------------------------------------------------------

struct OpenOptions {
    const char* title = "Breath of Fire III";
    int width = 640, height = 480;   // the picture's pixels
    double scale = 0;                // 0: the display's scale
    bool hidden = false;             // not shown on the screen: a frame is drawn to be saved (SaveFrame)
};
// The window, the renderer, the fonts and the look. False with `why` when the system gives no window.
bool Open(const OpenOptions& options, std::string* why);
void Close();
double Scale();
// The look's two colours a page draws with by itself: the pages' background and their text.
Color Paper();
Color Ink();
// The name of SDL's renderer in use ("software", "vulkan", ...), for a log line.
const char* RendererName();

// One frame. FrameBegin reads the events and waits for one when nothing moves,
// so an idle window costs no processor time; false: the player closed the window.
bool FrameBegin();
void FrameEnd();
// The next FrameEnd also writes the frame it draws as a PNG, the window's
// pixels as drawn, so a check of a page needs no screen. FrameSaved: true
// once it is written, false with `why` when it could not be.
void SaveFrame(const std::string& png_path);
bool FrameSaved(std::string* why);

// ---- Keys with a meaning for the whole page -----------------------------------

enum class Press {
    kStart,   // the pad's Start
    kBack,    // Escape or the pad's circle, when no open list or edit field took it
};
bool Pressed(Press press);

// ---- For checks ---------------------------------------------------------------

// A key or a pad button held or let go, as if the player did it, before a
// FrameBegin: a check moves through a page with no screen and no hands
// (main.cpp --keys). The names: up, down, left, right, enter, space, escape,
// tab, pad-up, pad-down, pad-left, pad-right, pad-cross, pad-circle,
// pad-start. False: no such name.
bool TestKey(const char* name, bool down);

// A widget used by its name, as if the player did it, in the next frame that
// draws it: a button or a tab pressed (any value), a check box set ("0" or
// "1"), a radio button chosen (any value), a combo box, a list or a slider
// set to a number, an edit field set to a text. The name is the widget's
// label as the page wrote it, or its part before or after ##. TestPending:
// the widget was not drawn yet, or it is grey (TestGrey) and took nothing.
void TestSet(const char* widget, const char* value);
bool TestPending();
bool TestGrey();

// ---- Drawing under the widgets ------------------------------------------------

struct Picture;
// A PNG's pixels as a picture; null when they are not a PNG. Lives until Close.
Picture* PictureFromPng(const unsigned char* bytes, unsigned size);
// Without smoothing at a whole scale and smoothed at a fractional one, where
// unsmoothed pixels would come out uneven (docs/launcher-settings.md 2a).
void PictureDraw(Picture* picture, float x, float y, float w, float h);
void Fill(float x, float y, float w, float h, Color color);

enum class Face {
    kText,     // the pages' text
    kBanner,   // the banner's lettering
};
// `em` is the type's size as a font's em, as CreateFontW's negative height is.
float TextWidth(Face face, float em, const char* text);
void Text(Face face, float em, float x, float baseline, Color color, const char* text);

// ---- Widgets ------------------------------------------------------------------
//
// A widget's label is its name: two widgets of one page with the same label
// take "label##other" (the part from ## is not shown). A widget with no label
// to show takes "##name".

// The next widget's top left corner, in the pixels of the area it is in (the
// window, or the group or area opened last).
void Place(float x, float y);
// The next widget's width and height; 0 keeps the widget's own.
void Size(float w, float h = 0);
// The next widget is shown grey and cannot be used; `reason` is shown when
// the pointer rests on it. Every option stays listed (the owner's standing
// call): a page greys a widget, it does not leave it out.
void Grey(const char* reason);
// The widget before this call takes the focus when its page opens.
void DefaultFocus();

bool Button(const char* label);
// A push-like button that stays down while `on`: a tab, a sub-tab.
bool Tab(const char* label, bool on);
bool Check(const char* label, bool* on);
bool Radio(const char* label, int* value, int mine);
bool Combo(const char* label, int* index, const std::vector<std::string>& items);
// A combo box with entries the player cannot choose: `usable[i]` false shows
// entry i grey. Every entry stays listed (the owner's standing call).
bool ComboGreyed(const char* label, int* index, const std::vector<std::string>& items,
                 const std::vector<bool>& usable);
bool Slider(const char* label, int* value, int low, int high);
bool Edit(const char* label, std::string* text);
// A list with one line chosen; Size gives its box.
bool List(const char* label, int* index, const std::vector<std::string>& items);
// A text. With a width from Size it wraps at that width.
void Label(const char* text);
// A text of many lines in a box that scrolls, read and not changed; the
// keyboard and the pad reach it and scroll it. Size gives its box.
void ReadText(const char* label, const std::string& text);
void Progress(float fraction);

// A box with a title; the widgets up to GroupEnd are placed in its pixels.
void GroupBegin(const char* title, float x, float y, float w, float h);
void GroupEnd();
// A part of the window with no box; it scrolls when its widgets do not fit.
void AreaBegin(const char* name, float x, float y, float w, float h);
void AreaEnd();

// ---- The system's own windows -------------------------------------------------

// A message with an OK button; returns when the player has read it, before
// Open too. With no window system the text goes to stderr.
void Message(const char* title, const char* text, bool error);

// The system's file and folder pickers. They do not stop the frames: Picked
// answers true once, in a later frame, with the path the player chose - empty
// when the player chose none. `ask` is the page's own number for the question.
// `pattern` is a list of name endings with no dot, "ppf;xdelta"; null: any file.
void PickFile(int ask, const char* kind, const char* pattern);
void PickFolder(int ask);
bool Picked(int ask, std::string* path);

}  // namespace bof3x::ui

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
    const char* banner_font = nullptr;   // a font file for the banner's lettering, to try one; null: the built-in font
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

// The bind capture's two inputs, as if the player pressed them, while a
// capture is on (CaptureBegin): a key by its SDL scancode, and a pad input by
// its number (input/bindings.h's PadInput). CaptureKey and CapturePad answer
// them once, before any key or pad input of the system. A check binds a key
// with no hands (main.cpp bind-key, bind-pad).
void TestCaptureKey(int scancode);
void TestCapturePad(int input);

// Whether the widget of that name (as TestSet names it) was drawn in the last
// frame, and whether its text was red (Red). False, false: not drawn.
bool TestDrawn(const char* widget, bool* red);

// In a check (OpenOptions::hidden) no question, picker or link is shown on
// the screen (stage P4). A question (Ask) waits for TestAnswer; a picker
// (PickFile, PickFolder) waits for TestPick, which answers the one asked
// last, as if the player chose that path ("" for none); a link (OpenLink) is
// said on stdout. TestAsked: the question waiting now, "" for none.
// TestPicking: a picker waits.
void TestAnswer(bool yes);
std::string TestAsked();
void TestPick(const std::string& path);
bool TestPicking();

// ---- The tools (stage P4) -----------------------------------------------------

// Wakes the window from any thread (a tool's line has come): the next
// FrameBegin does not wait for an event.
void WakeFrame();
// Waits up to `ms` for an event or a WakeFrame, and draws nothing: a check's
// rest between two frames while a tool runs (main.cpp wait).
void Rest(int ms);

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
// The next widget's text is red: a Controls cell whose key or pad input does
// two or more things (config_dialog.cpp's red cells).
void Red();
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
// A slider over the steps low..high that shows `shown` in place of the step's
// number: the value the page makes of the step ("0.65" for step 65 of 0.01).
// A check sets it to a step, as Slider.
bool SliderShowing(const char* label, int* value, int low, int high, const char* shown);
bool Edit(const char* label, std::string* text);
// A list with one line chosen; Size gives its box.
bool List(const char* label, int* index, const std::vector<std::string>& items);
// A text. With a width from Size it wraps at that width.
void Label(const char* text);
// A text on one line, cut at Size's width with "..." where it does not fit:
// at its end, or in its middle (`middle`, for a path: its start and its last
// part stay). The whole text shows when the pointer rests on a cut one. The
// Win32 pages' SS_ENDELLIPSIS and SS_PATHELLIPSIS labels.
void LabelCut(const char* text, bool middle);
// A text of many lines in a box that scrolls, read and not changed; the
// keyboard and the pad reach it and scroll it. Size gives its box.
void ReadText(const char* label, const std::string& text);
// A log: a box of text read and not changed, its lines wrapped at the box's
// width, scrolled to its last line each time the text grows (the Win32
// pages' log boxes, ES_AUTOVSCROLL with no horizontal scroll). The mouse
// wheel scrolls it; unlike ReadText, the keyboard and the pad do not reach it
// and its text cannot be selected.
void LogText(const char* label, const std::string& text);
void Progress(float fraction);

// A row of CheckList: its columns with a tab between them, a check box at
// its left or not, and grey text. A grey row can still be chosen.
struct ListRow {
    std::string text;
    bool box = false, ticked = false, grey = false;
};
// A list with one line chosen, a column for the check boxes at the left of
// every row, and the row's columns at `stops` (each a part of the width right
// of the box column, from 0 to 1). Size gives its box. Returns true in the
// frame the chosen line changed. `*toggled` is the row whose box the player
// used in this frame - a click on the box (which chooses the row too), or
// Space on the chosen row - else -1; the page decides what the box does. A
// check sets it as a List ("2"), and uses a row's box with "box 2". The Win32
// Mods list (sources_dialog.cpp ModsDraw, ModListProc).
bool CheckList(const char* label, int* index, const std::vector<ListRow>& rows, const std::vector<float>& stops,
               int* toggled);

// ---- The Jukebox (stage P5) ---------------------------------------------------

// A combo box whose entries `grey[i]` true are drawn in grey text and can
// still be chosen, the closed box too when one is chosen: the Win32
// Jukebox's owner-drawn song box, where a stream a source lacks is grey and
// Play then says why. A check sets it to any entry, as Combo.
bool ComboGreyText(const char* label, int* index, const std::vector<std::string>& items,
                   const std::vector<bool>& grey);

// A box with a title; the widgets up to GroupEnd are placed in its pixels.
void GroupBegin(const char* title, float x, float y, float w, float h);
void GroupEnd();
// A part of the window with no box; it scrolls when its widgets do not fit.
void AreaBegin(const char* name, float x, float y, float w, float h);
void AreaEnd();

// ---- The bind capture ---------------------------------------------------------
//
// The Controls pages' capture (config_dialog.cpp CaptureProc): from
// CaptureBegin to CaptureEnd the next key the player presses and the next
// pad input go to the capture, and not to the widgets. A key going down is
// taken before the widgets' navigation sees it, so Escape, Enter, Tab and the
// arrows are keys like any other (the Win32 capture's keyboard hook); a key
// that repeats while held is not a new key. The pad's navigation is off, so
// the pad's cross and circle are pad inputs like any other. The mouse still
// uses the widgets (the capture's Clear and Cancel).

// `layout` is input/bindings.h's Layout, as a number: the face buttons'
// meaning the pad is read with. As the Win32 launcher's pad (launcher.cpp,
// PadSdl_Start before the window), the first capture's layout stays for
// later ones.
void CaptureBegin(int layout);
void CaptureEnd();
bool Capturing();
// The key the player pressed since the last call, as SDL's scancode (SDL's
// numbers are the USB HID usages on every system). False: none.
bool CaptureKey(int* scancode);
// The first pad input (input/bindings.h's PadInput, the layout applied) that
// went down since CaptureBegin and was not held then: a button that opened
// the capture through the navigation is still down (CaptureProc's
// held_at_open). False: none, or no pad.
bool CapturePad(int* input);

// ---- The system's own windows -------------------------------------------------

// A message with an OK button; returns when the player has read it, before
// Open too. With no window system the text goes to stderr. In a check
// (OpenOptions::hidden) it is not shown: it is said on stdout (stage P5).
void Message(const char* title, const char* text, bool error);

// A question with Yes and No: the system's message box, and the answer, 1
// for Yes and 0 for No; the frames stop until the player answers. In a check
// the question is not shown and Ask returns -1 until TestAnswer gives the
// answer: the caller asks again in each frame until it has one.
int Ask(const char* title, const char* text);

// The system's file and folder pickers. They do not stop the frames: Picked
// answers true once, in a later frame, with the path the player chose - empty
// when the player chose none. `ask` is the page's own number for the question.
// `pattern` is a list of name endings with no dot, "ppf;xdelta"; null: any file.
// `title`: the picker's title, null for the system's own (stage P4: the Win32
// pickers' titles are the player's texts).
void PickFile(int ask, const char* kind, const char* pattern, const char* title = nullptr);
void PickFolder(int ask, const char* title = nullptr);
bool Picked(int ask, std::string* path);

// A web page in the system's browser (the Sources page's link to ffmpeg).
void OpenLink(const char* url);

// ---- The save editor (stage P5) -----------------------------------------------

// A combo box whose open list is `list_width` wide, wider than the box when
// its lines are long (the Win32 CB_SETDROPPEDWIDTH). A check sets it as Combo
// ("3"), or chooses the first entry that holds a text with "has <text>".
bool ComboWide(const char* label, int* index, const std::vector<std::string>& items, float list_width);

// An edit field with its list beside it (the Win32 CBS_DROPDOWN): the player
// types a text, or opens the list and chooses an entry. Returns true in the
// frame Enter was pressed in the field; `*picked` is the entry chosen from the
// list in this frame, its text then in `*text`, else -1. `focus`: the field
// takes the keys in this frame. A check sets the text ("12 x 3"), or chooses
// entry n from the list with "entry n".
bool EditCombo(const char* label, std::string* text, const std::vector<std::string>& items, int* picked, bool focus);

// A list of rows in columns under their titles, one row chosen (the Win32
// report list view: LVS_REPORT, LVS_SINGLESEL, a whole row selected).
// `weights`: each column's part of the width. Returns true in the frame the
// chosen row changed. `*used` is the row double-clicked in this frame, or the
// chosen row used again with Enter, Space or the pad's cross (a keyboard and a
// pad have no double-click), else -1. A check chooses a row as a List ("2"),
// and uses one with "use 2".
bool ColumnList(const char* label, int* index, const std::vector<std::string>& titles, const std::vector<float>& weights,
                const std::vector<std::vector<std::string>>& rows, int* used);

// A question as Ask, whose default answer, for Enter and Escape, is No (the
// Win32 MB_DEFBUTTON2): a question that discards or replaces.
int AskNo(const char* title, const char* text);

// The file picker with the Win32 picker's arguments: its filters (`pattern`
// as PickFile's, "*" for any file), the folder it opens in, and `may_be_new`,
// which lets the player name a file that is not there yet (the system's
// picker for a new file: it asks before it names a file that is there). It
// answers through Picked, as PickFile; a check answers it with TestPick.
struct PickFilter {
    std::string kind, pattern;
};
void PickFileIn(int ask, const std::vector<PickFilter>& filters, const char* folder, bool may_be_new, const char* title);

}  // namespace bof3x::ui

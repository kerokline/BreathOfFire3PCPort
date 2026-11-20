// Controls: Directions and buttons, Shoulders, Start and Select, and Function
// keys (launcher.rc IDD_PAGE_CONTROLS, IDD_PAGE_CONTROLS2, IDD_PAGE_CONTROLS3;
// DIV-0050, DIV-0240; docs/controls.md sections 4.2 and 7), and the bind
// capture over a cell of any of them (IDD_CAPTURE). config_dialog.cpp's
// Cells, CellsFrom, BindingsFrom, ControlsRefresh, ControlsPopulate,
// CaptureProc, CaptureInto, FnCaptureInto, FnRedRefresh, FnRefresh,
// FnPopulate, each page's Defaults and the Controls part of ApplyPages, less
// the windows. The texts are the Win32 pages', word for word.
//
// The two binding pages: fourteen rows, one a PlayStation input, each with
// two key cells and two pad cells. A cell is a button that shows its binding;
// using it opens the capture, which takes the next key or pad input, and the
// cell takes it - from whichever cell of its kind held it before, so a key
// means one thing. A key entry that names two actions at once (the keypad's
// diagonals in the default table) has no cell and is kept as it is, unless a
// cell takes its key.
//
// The Function keys page: one row an action, a key cell and a pad cell, the
// same capture. Nothing is taken from another cell: a key or pad input that
// does two or more things is allowed, and every cell that holds it has red
// text, on all three pages (input::FnRedCells).
//
// The bindings are written back only once a cell, the face-button box or the
// Defaults of the binding pages was used (DialogState::cells_touched), and
// the function keys only once one of that page's cells or its Defaults was
// (fk_touched): an untouched window writes them back as they were read. As in
// the Win32 window, a cell whose capture was cancelled counts as used.
//
// The capture is shown in place of the tabs' page (frame.cpp), as the Win32
// launcher shows a window of its own over them. Its keys come from ui.h's
// capture (CaptureKey: SDL's scancode, taken before the widgets' navigation
// sees it) and are made a cell's number by capture_key.h, which keeps the
// Win32 capture's numbers (D427); its pad input comes from CapturePad
// (input/pad_sdl.cpp, as the Win32 capture reads it).
#include "launcher/ui/pages.h"

#include "input/bindings.h"
#include "input/function_keys.h"
#include "launcher/ui/capture_key.h"
#include "launcher/ui/ui.h"

#include <algorithm>
#include <string>
#include <vector>

namespace bof3x::ui {

namespace {

// config_dialog.cpp's Cells.
struct Cells {
    int key[input::kActionCount][2];   // a key number, or -1
    int pad[input::kActionCount][2];   // a PadInput, or -1
    input::Layout layout = input::Layout::kPositional;
    std::vector<input::KeyBinding> kept;   // multi-action keys and unnamed numbers, as loaded
};

// Which cell the capture is over.
enum class Asker { kNone, kGame, kFn };

struct Boxes {
    Cells cells{};
    int layout_box = 0;   // the Face buttons box's entry
    bool cells_touched = false;
    input::FnBindings fk = input::FnBindings::Defaults();
    bool fk_touched = false;
    int pad_layout = 0;   // the layout read at the open: the pad is read with it (launcher.cpp's PadSdl_Start)
    // The capture: the cell it is over, its prompt, and the cell that takes
    // the focus when the page is shown again (the Win32 window's SetFocus).
    Asker asker = Asker::kNone;
    int row = 0, slot = 0;   // a binding page's row and slot, or the Function keys page's action (row)
    bool key_cell = true;
    std::string what, prompt, focus;
    // The red cells (FnRedRefresh), made each frame.
    bool red_key[input::kActionCount][2] = {}, red_pad[input::kActionCount][2] = {};
    bool red_fk[input::kFnActionCount] = {}, red_fp[input::kFnActionCount] = {};
};
Boxes g_boxes;

const char kNone[] = "(none)";

Cells CellsFrom(const input::Bindings& b) {
    Cells c{};
    for (int row = 0; row < input::kActionCount; ++row)
        c.key[row][0] = c.key[row][1] = c.pad[row][0] = c.pad[row][1] = -1;
    const input::ActionInfo* actions = input::Actions();
    for (const input::KeyBinding& k : b.keys) {
        const bool single = (k.bits & (k.bits - 1)) == 0;
        // KeyName spells an unnamed number as "0x..". The 0 key is named "0",
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
    for (const input::PadBinding& pb : b.pad) {
        int row = -1;
        for (int r = 0; r < input::kActionCount; ++r)
            if (actions[r].bit == pb.bits) row = r;
        if (row < 0) continue;
        if (c.pad[row][0] < 0) c.pad[row][0] = static_cast<int>(pb.input);
        else if (c.pad[row][1] < 0) c.pad[row][1] = static_cast<int>(pb.input);
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
            for (const input::PadBinding& pb : b.pad) dup = dup || static_cast<int>(pb.input) == in;
            if (!dup) b.pad.push_back({static_cast<input::PadInput>(in), actions[row].bit});
        }
    }
    for (const input::KeyBinding& k : c.kept) {
        if (taken[k.dik]) continue;
        taken[k.dik] = true;
        b.keys.push_back(k);
    }
    if (b.keys.size() > static_cast<std::size_t>(input::kKeyTableMax)) b.keys.resize(input::kKeyTableMax);
    b.layout = c.layout;
}

// The Win32 page's FnRedRefresh: every key cell and pad cell of the three
// pages, and the kept keys (no cell, but a function key on one is a double
// binding).
void RedRefresh() {
    Boxes& b = g_boxes;
    std::vector<int> keys, pads;
    for (int row = 0; row < input::kActionCount; ++row)
        for (int slot = 0; slot < 2; ++slot) {
            keys.push_back(b.cells.key[row][slot]);
            pads.push_back(b.cells.pad[row][slot]);
        }
    for (const input::KeyBinding& k : b.cells.kept) keys.push_back(k.dik);
    const input::FnRed red = input::FnRedCells(keys, pads, b.fk);
    std::size_t i = 0;
    for (int row = 0; row < input::kActionCount; ++row)
        for (int slot = 0; slot < 2; ++slot, ++i) {
            b.red_key[row][slot] = red.game_keys[i];
            b.red_pad[row][slot] = red.game_pads[i];
        }
    for (int a = 0; a < input::kFnActionCount; ++a) {
        b.red_fk[a] = red.fn_key[a];
        b.red_fp[a] = red.fn_pad[a];
    }
}

std::string KeyText(int key) { return key < 0 ? kNone : input::KeyName(key); }

std::string PadText(int pad) {
    return pad < 0 || pad >= static_cast<int>(input::PadInput::kCount) ? kNone : input::PadInputs()[pad].label;
}

// A cell: its text, and the name a check gives it after ##. True: used.
bool Cell(const std::string& text, const std::string& name, bool red, float x, float y, float w, float h) {
    Boxes& b = g_boxes;
    Place(x, y);
    Size(w, h);
    if (red) Red();
    const bool used = Button((text + "##" + name).c_str());
    if (b.focus == name) {   // the cell the capture was over, when the page is shown again
        DefaultFocus();
        b.focus.clear();
    }
    return used;
}

void Ask(Pages& p, Asker asker, int row, int slot, bool key_cell, const std::string& name) {
    Boxes& b = g_boxes;
    b.asker = asker;
    b.row = row;
    b.slot = slot;
    b.key_cell = key_cell;
    b.focus = name;
    b.what = asker == Asker::kFn ? input::FnActions()[row].label : input::Actions()[row].label;
    b.prompt = asker == Asker::kFn ? "Press a key or a pad button for this action: " + b.what + "."
                                   : "Press a key or a pad button for " + b.what + ".";
    p.capture_open = true;
    CaptureBegin(b.pad_layout);
}

// One binding page's rows, `first` to `last`, and its column titles, in the
// Bindings box (`rows` of them, 17 dialog units apart from y 27).
void BindingRows(Pages& p, int first, int last, float box_h) {
    Boxes& b = g_boxes;
    RedRefresh();
    GroupBegin("Bindings", 0, 0, Dx(346), Dy(box_h));
    Place(Dx(49), Dy(13));
    Label("Keyboard");
    Place(Dx(108), Dy(13));
    Label("Keyboard 2");
    Place(Dx(170), Dy(13));
    Label("Pad");
    Place(Dx(257), Dy(13));
    Label("Pad 2");
    const input::ActionInfo* actions = input::Actions();
    for (int row = first; row <= last; ++row) {
        const float y = 27 + 17 * static_cast<float>(row - first);
        Place(Dx(7), Dy(y + 2));
        Label(actions[row].label);
        const std::string name = actions[row].name;
        struct Column {
            float x, w;
            bool key;
            int slot;
            const char* prefix;
        };
        constexpr Column columns[] = {
            {49, 56, true, 0, "key1-"}, {108, 56, true, 1, "key2-"}, {170, 84, false, 0, "pad1-"}, {257, 84, false, 1, "pad2-"}};
        for (const Column& c : columns) {
            const std::string text = c.key ? KeyText(b.cells.key[row][c.slot]) : PadText(b.cells.pad[row][c.slot]);
            const bool red = c.key ? b.red_key[row][c.slot] : b.red_pad[row][c.slot];
            const std::string cell = c.prefix + name;
            if (Cell(text, cell, red, Dx(c.x), Dy(y), Dx(c.w), Dy(14)))
                Ask(p, Asker::kGame, row, c.slot, c.key, cell);
        }
    }
    GroupEnd();
}

// The capture's answer into a binding page's cells (CaptureInto): the cell
// takes what was pressed, and any other cell of its kind that held it lets go.
void GameInto(bool key, int value) {
    Boxes& b = g_boxes;
    Cells& c = b.cells;
    int slot = b.slot;
    // A key answers a pad cell, or a pad a key cell: the device decides the
    // kind of cell, the row stays; it goes in that row's first free cell of
    // its kind, or its first cell.
    int (*cells)[2] = key ? c.key : c.pad;
    if (key != b.key_cell) slot = cells[b.row][0] < 0 ? 0 : cells[b.row][1] < 0 ? 1 : 0;
    for (int r = 0; r < input::kActionCount; ++r)
        for (int s = 0; s < 2; ++s)
            if (cells[r][s] == value) cells[r][s] = -1;
    cells[b.row][slot] = value;
    if (key)
        c.kept.erase(std::remove_if(c.kept.begin(), c.kept.end(),
                                    [&](const input::KeyBinding& k) { return k.dik == value; }),
                     c.kept.end());
}

// The capture is over: the page is shown again, and the cell's page counts as
// used whatever the answer (the Win32 window sets cells_touched or fk_touched
// after any capture, a cancelled one too).
void Done(Pages& p) {
    Boxes& b = g_boxes;
    (b.asker == Asker::kFn ? b.fk_touched : b.cells_touched) = true;
    b.asker = Asker::kNone;
    p.capture_open = false;
    CaptureEnd();
}

}  // namespace

void ControlsOpen(Pages& p) {
    Boxes& b = g_boxes;
    b = Boxes{};
    b.cells = CellsFrom(p.cfg->bindings);
    b.layout_box = static_cast<int>(b.cells.layout);
    b.pad_layout = static_cast<int>(p.cfg->bindings.layout);
    b.fk = ConfigFnKeys(*p.cfg);
    RedRefresh();
}

// ApplyPages' Controls part.
void ControlsApply(Pages& p) {
    Boxes& b = g_boxes;
    if (b.cells_touched) {
        b.cells.layout = static_cast<input::Layout>(b.layout_box < 0 ? 0 : b.layout_box);
        BindingsFrom(b.cells, p.cfg->bindings);
    }
    if (b.fk_touched) ConfigFnKeysSet(*p.cfg, b.fk);   // untouched: the fkey lines and ruby_key= as read
}

int PagesWindowKey(const Pages&) { return g_boxes.fk.key[static_cast<int>(input::FnAction::kWindow)]; }

// Directions and buttons: Up to Triangle.
void ControlsPage1(Pages& p) { BindingRows(p, 0, 7, 166); }

// Shoulders, Start and Select: L1 to Select, the face buttons' box and Defaults.
void ControlsPage2(Pages& p) {
    Boxes& b = g_boxes;
    BindingRows(p, 8, 13, 132);
    Place(Dx(7), Dy(140));
    Label("Face buttons:");
    Place(Dx(69), Dy(138));
    Size(Dx(200));
    if (Combo("layout", &b.layout_box,
              {"Position - the bottom button accepts (Xbox, PlayStation)", "Nintendo - the right button accepts",
               "Automatic - use the labels on the pad"}))
        b.cells_touched = true;
    Place(Dx(289), Dy(137));
    Size(Dx(52), 23);
    if (Button("Defaults##controls")) {
        b.cells = CellsFrom(input::Bindings::Defaults());
        b.layout_box = static_cast<int>(b.cells.layout);
        b.cells_touched = true;
    }
    Place(Dx(7), Dy(155));
    Size(Dx(340));
    Label("To change a binding, click a cell, then press a key or a pad button. Escape cancels. Keypad keys 7, 9, 1 "
          "and 3 move diagonally if no cell uses them.");
}

// Function keys: one row an action (FnPopulate, FnRefresh).
void FnKeysPage(Pages& p) {
    Boxes& b = g_boxes;
    RedRefresh();
    GroupBegin("Function keys", 0, 0, Dx(346), Dy(150));
    Place(Dx(150), Dy(12));
    Label("Keyboard");
    Place(Dx(207), Dy(12));
    Label("Pad");
    for (int a = 0; a < input::kFnActionCount; ++a) {
        const input::FnActionInfo& info = input::FnActions()[a];
        const float y = 22 + 14 * static_cast<float>(a);
        Place(Dx(7), Dy(y + 2));
        Label(info.label);
        const std::string key = std::string("fkey-") + info.name, pad = std::string("fpad-") + info.name;
        if (Cell(KeyText(b.fk.key[a]), key, b.red_fk[a], Dx(150), Dy(y), Dx(54), Dy(13)))
            Ask(p, Asker::kFn, a, 0, true, key);
        if (Cell(PadText(b.fk.pad[a]), pad, b.red_fp[a], Dx(207), Dy(y), Dx(80), Dy(13)))
            Ask(p, Asker::kFn, a, 0, false, pad);
        Place(Dx(290), Dy(y + 2));
        Label(info.note);
    }
    GroupEnd();
    Place(Dx(7), Dy(153));
    Label("Red text: one key or pad button does two or more things.");
    Place(Dx(7), Dy(164));
    Label("Click a cell, then press a key or a pad button. Escape cancels.");
    Place(Dx(289), Dy(161));
    Size(Dx(52), 23);
    if (Button("Defaults##fkeys")) {   // this page's table only, as each page's Defaults
        b.fk = input::FnBindings::Defaults();
        b.fk_touched = true;
    }
}

// The capture (CaptureProc), in a box the size of the Win32 window.
void CapturePage(Pages& p) {
    Boxes& b = g_boxes;
    if (b.asker == Asker::kNone) {   // nothing asked: nothing to capture
        p.capture_open = false;
        CaptureEnd();
        return;
    }
    const bool fn = b.asker == Asker::kFn;
    GroupBegin("Bind", 0, 0, Dx(250), Dy(74));
    Place(Dx(14), Dy(12));
    Size(Dx(222));
    Label(b.prompt.c_str());
    Place(Dx(14), Dy(38));
    Label("Escape cancels. Clear removes the binding.");
    Place(Dx(138), Dy(53));
    Size(Dx(48), 23);
    const bool clear = Button("Clear##capture");
    Place(Dx(194), Dy(53));
    Size(Dx(48), 23);
    const bool cancel = Button("Cancel##capture");
    GroupEnd();

    if (cancel) return Done(p);
    if (clear) {
        if (fn) (b.key_cell ? b.fk.key : b.fk.pad)[b.row] = -1;
        else (b.key_cell ? b.cells.key : b.cells.pad)[b.row][b.slot] = -1;
        return Done(p);
    }
    int scancode = 0;
    while (CaptureKey(&scancode)) {
        int number = -1;
        switch (CaptureTakeKey(scancode, fn, &number)) {
        case CaptureTake::kNothing:
            continue;
        case CaptureTake::kCancel:
            return Done(p);
        case CaptureTake::kRefused:
            b.prompt = "The game does not get F10 or Alt. Press a different key or a pad button.";
            continue;
        case CaptureTake::kKey:
            if (fn) b.fk.key[b.row] = number;
            else GameInto(true, number);
            return Done(p);
        }
    }
    int input = -1;
    if (CapturePad(&input)) {
        if (fn) b.fk.pad[b.row] = input;
        else GameInto(false, input);
        return Done(p);
    }
}

}  // namespace bof3x::ui

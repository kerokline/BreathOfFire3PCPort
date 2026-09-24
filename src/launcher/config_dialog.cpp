// The settings dialog: a Win32 DIALOGEX from launcher.rc, shown with
// DialogBoxParam. No third-party toolkit, which is deliberate - the launcher
// stays a single dependency-free 32-bit exe, and nothing is vendored
// (CLAUDE.md rule 5, docs/LICENSING.md section 4).
#include "launcher/config_dialog.h"

#include <windows.h>

#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "input/pad_sdl.h"
#include "launcher/resource.h"

namespace bof3x {
namespace {

struct DialogState {
    Config* cfg;
    bool english_available;
};

void AddItem(HWND dlg, int id, const wchar_t* text) {
    SendDlgItemMessageW(dlg, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
}

void Select(HWND dlg, int id, int index) {
    SendDlgItemMessageW(dlg, id, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

int Selected(HWND dlg, int id) {
    return static_cast<int>(SendDlgItemMessageW(dlg, id, CB_GETCURSEL, 0, 0));
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

void Populate(HWND dlg, const DialogState& state) {
    const Config& cfg = *state.cfg;

    AddItem(dlg, IDC_LANGUAGE, L"Original (Chinese)");
    if (state.english_available) {
        AddItem(dlg, IDC_LANGUAGE, L"English (PlayStation script)");
        SetDlgItemTextW(dlg, IDC_LANGNOTE, L"");
    } else {
        // Offering an option that cannot work is worse than explaining why it
        // is missing. The overlays are built, not shipped.
        SetDlgItemTextW(dlg, IDC_LANGNOTE,
                        L"No DAT\\en.* overlays; build them with tools/loc_build.py.");
    }
    const bool english = cfg.language == Language::kEnglish && state.english_available;
    Select(dlg, IDC_LANGUAGE, english ? 1 : 0);

    AddItem(dlg, IDC_FILTER, L"Smooth - bilinear (original)");
    AddItem(dlg, IDC_FILTER, L"Sharp - point");
    AddItem(dlg, IDC_FILTER, L"CRT - scanlines and glow");
    AddItem(dlg, IDC_FILTER, L"CRT - SatPixie (newpixie fork, MIT)");
    Select(dlg, IDC_FILTER, cfg.satpixie ? 3 : cfg.crt ? 2 : cfg.filter == Filter::kPoint ? 1 : 0);
    EnableWindow(GetDlgItem(dlg, IDC_LOOKOPTIONS), cfg.satpixie);

    AddItem(dlg, IDC_DISPLAY, L"Fullscreen - borderless window");
    AddItem(dlg, IDC_DISPLAY, L"Windowed - resizable, F8 toggles");
    Select(dlg, IDC_DISPLAY, cfg.display == Display::kWindowed ? 1 : 0);
    CheckDlgButton(dlg, IDC_BACKGROUND, cfg.background ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_WIDE, cfg.wide ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(dlg, IDC_SNAP, cfg.snap ? BST_CHECKED : BST_UNCHECKED);

    AddItem(dlg, IDC_RENDERER, L"Direct3D (default)");
    AddItem(dlg, IDC_RENDERER, L"Software");
    Select(dlg, IDC_RENDERER, cfg.renderer ? 0 : 1);

    CheckDlgButton(dlg, IDC_SHOW, cfg.show_launcher ? BST_CHECKED : BST_UNCHECKED);
}

void ReadBack(HWND dlg, Config& cfg) {
    cfg.language = Selected(dlg, IDC_LANGUAGE) == 1 ? Language::kEnglish : Language::kOriginal;
    const int look = Selected(dlg, IDC_FILTER);
    cfg.filter = look >= 1 ? Filter::kPoint : Filter::kLinear;
    cfg.crt = look == 2;
    cfg.satpixie = look == 3;
    cfg.display = Selected(dlg, IDC_DISPLAY) == 1 ? Display::kWindowed : Display::kFullscreen;
    cfg.renderer = Selected(dlg, IDC_RENDERER) == 1 ? 0 : 1;
    cfg.snap = IsDlgButtonChecked(dlg, IDC_SNAP) == BST_CHECKED;
    cfg.background = IsDlgButtonChecked(dlg, IDC_BACKGROUND) == BST_CHECKED;
    cfg.wide = IsDlgButtonChecked(dlg, IDC_WIDE) == BST_CHECKED;
    cfg.show_launcher = IsDlgButtonChecked(dlg, IDC_SHOW) == BST_CHECKED;
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

// ---- DIV-0045 / DIV-0046: the cheats dialog ---------------------------------

struct CheatSlider {
    int id, label;
    int Config::Cheats::*value;
};
const CheatSlider kCheatSliders[] = {
    {IDC_CH_EXP, IDC_CH_EXP_V, &Config::Cheats::exp},
    {IDC_CH_ZENNY, IDC_CH_ZENNY_V, &Config::Cheats::zenny},
};
constexpr int kCheatMultiplierMax = 50;   // the DLL refuses more (src/game/cheats.cpp)

void CheatsPopulate(HWND dlg, const Config::Cheats& ch) {
    for (const CheatSlider& s : kCheatSliders) {
        HWND h = GetDlgItem(dlg, s.id);
        SendMessageW(h, TBM_SETRANGE, TRUE, MAKELPARAM(0, kCheatMultiplierMax));
        SendMessageW(h, TBM_SETTICFREQ, 5, 0);
        SendMessageW(h, TBM_SETPOS, TRUE, static_cast<LPARAM>(ch.*s.value));
        SetDlgItemInt(dlg, s.label, static_cast<UINT>(ch.*s.value), FALSE);
    }
    CheckDlgButton(dlg, IDC_CH_STEAL, ch.steal ? BST_CHECKED : BST_UNCHECKED);
}

void CheatsReadBack(HWND dlg, Config::Cheats& ch) {
    for (const CheatSlider& s : kCheatSliders) {
        const int pos = static_cast<int>(SendDlgItemMessageW(dlg, s.id, TBM_GETPOS, 0, 0));
        ch.*s.value = pos < 0 ? 0 : pos > kCheatMultiplierMax ? kCheatMultiplierMax : pos;
    }
    ch.steal = IsDlgButtonChecked(dlg, IDC_CH_STEAL) == BST_CHECKED;
}

INT_PTR CALLBACK CheatsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        padnav::Attach(dlg);
        auto* ch = reinterpret_cast<Config::Cheats*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ch));
        CheatsPopulate(dlg, *ch);
        return TRUE;
    }
    case WM_HSCROLL: {
        HWND h = reinterpret_cast<HWND>(lp);
        for (const CheatSlider& s : kCheatSliders) {
            if (GetDlgItem(dlg, s.id) != h) continue;
            SetDlgItemInt(dlg, s.label, static_cast<UINT>(SendMessageW(h, TBM_GETPOS, 0, 0)), FALSE);
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
            auto* ch = reinterpret_cast<Config::Cheats*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            CheatsReadBack(dlg, *ch);
            EndDialog(dlg, 1);
            return TRUE;
        }
        case IDC_CH_DEFAULTS: {
            Config::Cheats defaults;
            CheatsPopulate(dlg, defaults);
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

// The cheats dialog over the settings dialog; the settings change only on OK.
void CheatsOptions(HWND owner, Config::Cheats& ch) {
    Config::Cheats edit = ch;
    const INT_PTR r = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CHEATS), owner, CheatsProc,
                                      reinterpret_cast<LPARAM>(&edit));
    if (r == 1) ch = edit;
}

// ---- DIV-0050: the Controls dialog (docs/controls.md section 4.2) -----------
//
// Fourteen rows, one a PlayStation input, each with two key cells and two pad
// cells; a cell is a button showing its binding. Clicking one opens the
// capture, which takes the next key (a keyboard hook, so the focus does not
// matter) or the next pad input that was not already held, and the cell
// takes it - from whichever cell held it before, so a key means one thing.
// OK rebuilds the bindings list from the cells. A key entry that names two
// actions at once (the keypad's diagonals in the default table) has no cell,
// and is kept as it is unless a cell takes its key.

std::wstring Wide(const std::string& s) {
    std::wstring w;
    for (unsigned char c : s) w += static_cast<wchar_t>(c);
    return w;
}

struct Cells {
    int key[input::kActionCount][2];   // DIK scancode, or -1
    int pad[input::kActionCount][2];   // PadInput index, or -1
    input::Layout layout;
    std::vector<input::KeyBinding> kept;   // multi-action keys and unnamed scancodes, as loaded
};

Cells CellsFrom(const input::Bindings& b) {
    Cells c{};
    for (int row = 0; row < input::kActionCount; ++row)
        c.key[row][0] = c.key[row][1] = c.pad[row][0] = c.pad[row][1] = -1;
    const input::ActionInfo* actions = input::Actions();
    for (const input::KeyBinding& k : b.keys) {
        const bool single = (k.bits & (k.bits - 1)) == 0;
        const bool named = input::KeyName(k.dik)[0] != '0';
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

void ControlsRefresh(HWND dlg, const Cells& c) {
    for (int row = 0; row < input::kActionCount; ++row) {
        for (int slot = 0; slot < 2; ++slot) {
            const int dik = c.key[row][slot];
            SetDlgItemTextW(dlg, (slot ? IDC_CT_KEY2 : IDC_CT_KEY1) + row,
                            dik < 0 ? L"(none)" : Wide(input::KeyName(dik)).c_str());
            const int in = c.pad[row][slot];
            SetDlgItemTextW(dlg, (slot ? IDC_CT_PAD2 : IDC_CT_PAD1) + row,
                            in < 0 ? L"(none)" : Wide(input::PadInputs()[in].label).c_str());
        }
    }
}

void ControlsPopulate(HWND dlg, const Cells& c) {
    ControlsRefresh(dlg, c);
    SendDlgItemMessageW(dlg, IDC_CT_LAYOUT, CB_RESETCONTENT, 0, 0);
    AddItem(dlg, IDC_CT_LAYOUT, L"By position - the lower button confirms (Xbox, PlayStation)");
    AddItem(dlg, IDC_CT_LAYOUT, L"Nintendo - the pairs swapped, the right button confirms");
    AddItem(dlg, IDC_CT_LAYOUT, L"Automatic - from the pad's own labels");
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
        std::wstring prompt = std::wstring(L"Press a key or a pad input for ") + c->what + L".";
        SetDlgItemTextW(dlg, IDC_CP_PROMPT, prompt.c_str());
        // What is held now is not the answer: a button that opened this dialog
        // through the pad navigation is still down.
        input::PadSdl_Poll();
        for (int i = 0; i < static_cast<int>(input::PadInput::kCount); ++i)
            c->held_at_open[i] = input::PadSdl_InputDown(static_cast<input::PadInput>(i));
        c->hook = SetWindowsHookExW(WH_KEYBOARD, CaptureKeyHook, nullptr, GetCurrentThreadId());
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

INT_PTR CALLBACK ControlsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* c = reinterpret_cast<Cells*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(c));
        ControlsPopulate(dlg, *c);
        padnav::Attach(dlg);
        return TRUE;
    }
    case WM_TIMER:
        if (wp == padnav::kTimer) {
            padnav::Tick(dlg);
            return TRUE;
        }
        break;
    case WM_COMMAND: {
        auto* c = reinterpret_cast<Cells*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
        const int id = LOWORD(wp);
        switch (id) {
        case IDOK:
            c->layout = static_cast<input::Layout>(Selected(dlg, IDC_CT_LAYOUT) < 0 ? 0 : Selected(dlg, IDC_CT_LAYOUT));
            EndDialog(dlg, 1);
            return TRUE;
        case IDC_CT_DEFAULTS:
            *c = CellsFrom(input::Bindings::Defaults());
            ControlsPopulate(dlg, *c);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, 0);
            return TRUE;
        default:
            break;
        }
        if (id >= IDC_CT_KEY1 && id < IDC_CT_KEY1 + 4 * 20 && HIWORD(wp) == BN_CLICKED) {
            const int kind = (id - IDC_CT_KEY1) / 20, row = (id - IDC_CT_KEY1) % 20;
            if (row < input::kActionCount) {
                CaptureInto(dlg, *c, row, kind & 1, kind < 2);
                ControlsRefresh(dlg, *c);
                SetFocus(GetDlgItem(dlg, id));
                return TRUE;
            }
        }
        break;
    }
    default:
        break;
    }
    return FALSE;
}

// The Controls dialog over the settings dialog; the bindings change only on OK.
void ControlsOptions(HWND owner, input::Bindings& b) {
    Cells edit = CellsFrom(b);
    const INT_PTR r = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CONTROLS), owner, ControlsProc,
                                      reinterpret_cast<LPARAM>(&edit));
    if (r == -1) std::fprintf(stderr, "bof3x-launcher: the Controls dialog could not be created (error %lu)\n", GetLastError());
    if (r == 1) {
        BindingsFrom(edit, b);
        padnav::SetMap(b.pad);
    }
}

INT_PTR CALLBACK Proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        padnav::Attach(dlg);
        auto* state = reinterpret_cast<DialogState*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        Populate(dlg, *state);
        // The launcher is a console program, so its dialog can open behind the
        // window that started it.
        SetForegroundWindow(dlg);
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
            auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            ReadBack(dlg, *state->cfg);
            EndDialog(dlg, 1);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, 0);
            return TRUE;
        case IDC_FILTER:
            if (HIWORD(wp) == CBN_SELCHANGE) EnableWindow(GetDlgItem(dlg, IDC_LOOKOPTIONS), Selected(dlg, IDC_FILTER) == 3);
            return TRUE;
        case IDC_LOOKOPTIONS: {
            auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            SatpixieOptions(dlg, state->cfg->sp);
            return TRUE;
        }
        case IDC_CHEATS: {
            auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            CheatsOptions(dlg, state->cfg->cheats);
            return TRUE;
        }
        case IDC_CONTROLS: {
            auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            ControlsOptions(dlg, state->cfg->bindings);
            return TRUE;
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

}  // namespace

bool ConfigDialogRun(const std::wstring& game_dir, Config& cfg) {
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);

    DialogState state{&cfg, ConfigEnglishAvailable(game_dir)};
    padnav::SetMap(cfg.bindings.pad);
    const INT_PTR result = DialogBoxParamW(GetModuleHandleW(nullptr),
                                           MAKEINTRESOURCEW(IDD_CONFIG), nullptr, Proc,
                                           reinterpret_cast<LPARAM>(&state));
    if (result == -1) {
        // Starting the game with settings the player never saw would be worse
        // than not starting: say so and let them try again.
        MessageBoxW(nullptr, L"The settings dialog could not be created.", L"bof3x-launcher",
                    MB_OK | MB_ICONERROR);
        return false;
    }
    return result == 1;
}

}  // namespace bof3x

// The settings dialog: a Win32 DIALOGEX from launcher.rc, shown with
// DialogBoxParam. No third-party toolkit, which is deliberate - the launcher
// stays a single dependency-free 32-bit exe, and nothing is vendored
// (CLAUDE.md rule 5, docs/LICENSING.md section 4).
#include "launcher/config_dialog.h"

#include <windows.h>

#include <commctrl.h>

#include <cstdio>
#include <string>
#include <vector>

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
// Fourteen rows, one a PlayStation input, each with two key boxes and two pad
// boxes. The boxes show what the bindings list holds for that input; OK
// rebuilds the list from the boxes. A key entry that names two actions at
// once (the keypad's diagonals in the default table) has no box, and is kept
// as it is unless a box takes its key.

std::wstring Wide(const std::string& s) {
    std::wstring w;
    for (unsigned char c : s) w += static_cast<wchar_t>(c);
    return w;
}

void ControlsPopulate(HWND dlg, const input::Bindings& b) {
    const input::ActionInfo* actions = input::Actions();
    for (int row = 0; row < input::kActionCount; ++row) {
        const unsigned short bit = actions[row].bit;
        for (int slot = 0; slot < 2; ++slot) {
            const int id = (slot ? IDC_CT_KEY2 : IDC_CT_KEY1) + row;
            SendDlgItemMessageW(dlg, id, CB_RESETCONTENT, 0, 0);
            AddItem(dlg, id, L"(none)");
            for (int k = 0; k < input::KeyCount(); ++k) AddItem(dlg, id, Wide(input::Keys()[k].name).c_str());
            Select(dlg, id, 0);
        }
        for (int slot = 0; slot < 2; ++slot) {
            const int id = (slot ? IDC_CT_PAD2 : IDC_CT_PAD1) + row;
            SendDlgItemMessageW(dlg, id, CB_RESETCONTENT, 0, 0);
            AddItem(dlg, id, L"(none)");
            for (int i = 0; i < static_cast<int>(input::PadInput::kCount); ++i)
                AddItem(dlg, id, Wide(input::PadInputs()[i].label).c_str());
            Select(dlg, id, 0);
        }
        // The first two single-action keys and the first two pad inputs bound to this input.
        int found = 0;
        for (const input::KeyBinding& k : b.keys) {
            if (k.bits != bit || found == 2) continue;
            int index = 0;
            for (int i = 0; i < input::KeyCount(); ++i)
                if (input::Keys()[i].dik == k.dik) index = i + 1;
            if (index == 0) continue;   // a scancode without a name: kept, not shown
            Select(dlg, (found ? IDC_CT_KEY2 : IDC_CT_KEY1) + row, index);
            ++found;
        }
        found = 0;
        for (const input::PadBinding& p : b.pad) {
            if (p.bits != bit || found == 2) continue;
            Select(dlg, (found ? IDC_CT_PAD2 : IDC_CT_PAD1) + row, static_cast<int>(p.input) + 1);
            ++found;
        }
    }
    SendDlgItemMessageW(dlg, IDC_CT_LAYOUT, CB_RESETCONTENT, 0, 0);
    AddItem(dlg, IDC_CT_LAYOUT, L"By position - the lower button confirms (Xbox, PlayStation)");
    AddItem(dlg, IDC_CT_LAYOUT, L"Nintendo - the pairs swapped, the right button confirms");
    AddItem(dlg, IDC_CT_LAYOUT, L"Automatic - from the pad's own labels");
    Select(dlg, IDC_CT_LAYOUT, static_cast<int>(b.layout));
}

void ControlsReadBack(HWND dlg, input::Bindings& b) {
    const input::ActionInfo* actions = input::Actions();
    std::vector<input::KeyBinding> keys;
    std::vector<input::PadBinding> pad;
    bool taken[256] = {};
    for (int row = 0; row < input::kActionCount; ++row) {
        for (int slot = 0; slot < 2; ++slot) {
            const int index = Selected(dlg, (slot ? IDC_CT_KEY2 : IDC_CT_KEY1) + row);
            if (index <= 0 || index > input::KeyCount()) continue;
            const unsigned char dik = input::Keys()[index - 1].dik;
            if (taken[dik]) continue;   // a key in two rows: the first row has it
            taken[dik] = true;
            keys.push_back({dik, actions[row].bit});
        }
        for (int slot = 0; slot < 2; ++slot) {
            const int index = Selected(dlg, (slot ? IDC_CT_PAD2 : IDC_CT_PAD1) + row);
            if (index <= 0 || index > static_cast<int>(input::PadInput::kCount)) continue;
            const auto in = static_cast<input::PadInput>(index - 1);
            bool dup = false;
            for (const input::PadBinding& p : pad) dup = dup || p.input == in;
            if (!dup) pad.push_back({in, actions[row].bit});
        }
    }
    // Multi-action keys and unnamed scancodes from the old list, unless taken.
    for (const input::KeyBinding& k : b.keys) {
        const bool single = (k.bits & (k.bits - 1)) == 0;
        const bool named = input::KeyName(k.dik)[0] != '0';
        if ((single && named) || taken[k.dik]) continue;
        taken[k.dik] = true;
        keys.push_back(k);
    }
    if (keys.size() > static_cast<size_t>(input::kKeyTableMax)) keys.resize(input::kKeyTableMax);
    b.keys = keys;
    b.pad = pad;
    const int layout = Selected(dlg, IDC_CT_LAYOUT);
    b.layout = layout >= 0 && layout <= 2 ? static_cast<input::Layout>(layout) : input::Layout::kPositional;
}

INT_PTR CALLBACK ControlsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* b = reinterpret_cast<input::Bindings*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(b));
        ControlsPopulate(dlg, *b);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK: {
            auto* b = reinterpret_cast<input::Bindings*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            ControlsReadBack(dlg, *b);
            EndDialog(dlg, 1);
            return TRUE;
        }
        case IDC_CT_DEFAULTS: {
            auto* b = reinterpret_cast<input::Bindings*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            *b = input::Bindings::Defaults();
            ControlsPopulate(dlg, *b);
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

// The Controls dialog over the settings dialog; the bindings change only on OK.
void ControlsOptions(HWND owner, input::Bindings& b) {
    input::Bindings edit = b;
    const INT_PTR r = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_CONTROLS), owner, ControlsProc,
                                      reinterpret_cast<LPARAM>(&edit));
    if (r == -1) std::fprintf(stderr, "bof3x-launcher: the Controls dialog could not be created (error %lu)\n", GetLastError());
    if (r == 1) b = edit;
}

INT_PTR CALLBACK Proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* state = reinterpret_cast<DialogState*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        Populate(dlg, *state);
        // The launcher is a console program, so its dialog can open behind the
        // window that started it.
        SetForegroundWindow(dlg);
        return TRUE;
    }
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

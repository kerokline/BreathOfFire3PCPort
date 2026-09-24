// The settings dialog: a Win32 DIALOGEX from launcher.rc, shown with
// DialogBoxParam. No third-party toolkit, which is deliberate - the launcher
// stays a single dependency-free 32-bit exe, and nothing is vendored
// (CLAUDE.md rule 5, docs/LICENSING.md section 4).
#include "launcher/config_dialog.h"

#include <windows.h>

#include <commctrl.h>

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
    Select(dlg, IDC_FILTER, cfg.crt ? 2 : cfg.filter == Filter::kPoint ? 1 : 0);

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
    cfg.display = Selected(dlg, IDC_DISPLAY) == 1 ? Display::kWindowed : Display::kFullscreen;
    cfg.renderer = Selected(dlg, IDC_RENDERER) == 1 ? 0 : 1;
    cfg.snap = IsDlgButtonChecked(dlg, IDC_SNAP) == BST_CHECKED;
    cfg.background = IsDlgButtonChecked(dlg, IDC_BACKGROUND) == BST_CHECKED;
    cfg.wide = IsDlgButtonChecked(dlg, IDC_WIDE) == BST_CHECKED;
    cfg.show_launcher = IsDlgButtonChecked(dlg, IDC_SHOW) == BST_CHECKED;
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

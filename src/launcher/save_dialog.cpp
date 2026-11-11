// The save editor's window: a list of sections, the section's fields in a
// list, and one value box. Every row is made from the field table
// (launcher/save/block.h), so the window holds no offset of its own.
// docs/save-editor.md says what it does; docs/save-editor-spec.md why.
#include "launcher/save_dialog.h"

#include <commctrl.h>
#include <commdlg.h>

#include <cstdio>
#include <string>
#include <vector>

#include "launcher/config.h"
#include "launcher/exe_image.h"
#include "launcher/resource.h"
#include "launcher/save/block.h"
#include "launcher/save/names.h"

namespace bof3x {

namespace {

constexpr int kOtherFile = save::kSlots;   // the file box's last entry: a file picked by the player

struct State {
    std::wstring game_dir;
    save::Names names;
    save::PartyTable table;

    bool loaded = false;
    int opened = -1;            // the slot opened, or kOtherFile
    std::wstring other;         // the file picked, for kOtherFile
    save::Block original{}, block{};

    bool advanced = false;
    std::vector<int> sections;          // the list's rows: indexes into save::Sections()
    std::vector<save::Field> fields;    // the field list's rows
    bool filling = false;               // the code is changing a box: its messages are not the player's
};

std::wstring W(const std::string& s) { return ConfigFromUtf8(s); }
std::string U(const std::wstring& w) { return ConfigToUtf8(w); }

std::wstring SlotPath(const State& s, int slot) {
    wchar_t name[32];
    std::swprintf(name, 32, L"\\BISLPS%02X.DAT", slot);
    return s.game_dir + name;
}

std::wstring Text(HWND dlg, int id) {
    HWND h = GetDlgItem(dlg, id);
    const int n = GetWindowTextLengthW(h);
    std::wstring w(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(h, w.data(), n + 1);
    w.resize(static_cast<size_t>(n));
    return w;
}
void Status(HWND dlg, const std::wstring& text) { SetDlgItemTextW(dlg, IDC_SE_STATUS, text.c_str()); }
int Selected(HWND dlg, int id) { return static_cast<int>(SendDlgItemMessageW(dlg, id, CB_GETCURSEL, 0, 0)); }

// The game's window, by the class it registers (magic_080.cpp: "Bof3"). The
// game reads and writes the same files, so the editor does not write while it runs.
bool GameRuns() { return FindWindowW(L"Bof3", nullptr) != nullptr; }

bool Dirty(const State& s) { return s.loaded && s.block != s.original; }

// ---- the file boxes --------------------------------------------------------------

std::wstring SlotLabel(const State& s, int slot) {
    wchar_t head[32];
    std::swprintf(head, 32, L"Slot %X - ", slot);
    save::Block b;
    std::string why;
    if (GetFileAttributesW(SlotPath(s, slot).c_str()) == INVALID_FILE_ATTRIBUTES) return std::wstring(head) + L"empty";
    if (!save::ReadPcFile(SlotPath(s, slot), b, why)) return std::wstring(head) + L"not a save file";
    return head + W(save::Describe(b, s.names)) +
           (save::Checksum(b) == save::StoredChecksum(b) ? L"" : L" (damaged)");
}

void FillSlots(HWND dlg, const State& s) {
    for (const int id : {IDC_SE_SLOT, IDC_SE_TARGET}) {
        const int keep = Selected(dlg, id);
        SendDlgItemMessageW(dlg, id, CB_RESETCONTENT, 0, 0);
        for (int i = 0; i < save::kSlots; ++i)
            SendDlgItemMessageW(dlg, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(SlotLabel(s, i).c_str()));
        const std::wstring other = id == IDC_SE_SLOT ? (s.other.empty() ? L"A different file..." : L"File: " + s.other)
                                                     : L"The file that is open";
        if (id == IDC_SE_SLOT || s.opened == kOtherFile)
            SendDlgItemMessageW(dlg, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(other.c_str()));
        SendDlgItemMessageW(dlg, id, CB_SETCURSEL, static_cast<WPARAM>(keep), 0);
    }
}

// ---- the sections and the fields ---------------------------------------------------

void FillSections(HWND dlg, State& s) {
    const int keep = static_cast<int>(SendDlgItemMessageW(dlg, IDC_SE_SECTIONS, LB_GETCURSEL, 0, 0));
    const int kept = keep >= 0 && keep < static_cast<int>(s.sections.size()) ? s.sections[static_cast<size_t>(keep)] : 0;
    s.sections.clear();
    SendDlgItemMessageW(dlg, IDC_SE_SECTIONS, LB_RESETCONTENT, 0, 0);
    int select = 0;
    const std::vector<save::Section>& all = save::Sections();
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        if (all[static_cast<size_t>(i)].advanced && !s.advanced) continue;
        if (i == kept) select = static_cast<int>(s.sections.size());
        s.sections.push_back(i);
        SendDlgItemMessageW(dlg, IDC_SE_SECTIONS, LB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(W(all[static_cast<size_t>(i)].name).c_str()));
    }
    SendDlgItemMessageW(dlg, IDC_SE_SECTIONS, LB_SETCURSEL, static_cast<WPARAM>(select), 0);
}

int Section(HWND dlg, const State& s) {
    const int row = static_cast<int>(SendDlgItemMessageW(dlg, IDC_SE_SECTIONS, LB_GETCURSEL, 0, 0));
    return row >= 0 && row < static_cast<int>(s.sections.size()) ? s.sections[static_cast<size_t>(row)] : -1;
}

void FillRecords(HWND dlg, State& s) {
    s.filling = true;
    const int section = Section(dlg, s);
    const int records = section < 0 ? 1 : save::Sections()[static_cast<size_t>(section)].records;
    const int keep = Selected(dlg, IDC_SE_RECORD);
    SendDlgItemMessageW(dlg, IDC_SE_RECORD, CB_RESETCONTENT, 0, 0);
    if (records > 1 && s.loaded)
        for (int i = 0; i < records; ++i)
            SendDlgItemMessageW(dlg, IDC_SE_RECORD, CB_ADDSTRING, 0,
                                reinterpret_cast<LPARAM>(W(save::RecordLabel(s.block, section, i, s.names)).c_str()));
    SendDlgItemMessageW(dlg, IDC_SE_RECORD, CB_SETCURSEL, static_cast<WPARAM>(keep >= 0 && keep < records ? keep : 0), 0);
    EnableWindow(GetDlgItem(dlg, IDC_SE_RECORD), records > 1 && s.loaded);
    InvalidateRect(GetDlgItem(dlg, IDC_SE_RECORD), nullptr, TRUE);
    s.filling = false;
}

void SetCell(HWND list, int row, int column, const std::wstring& text) {
    LVITEMW item{};
    item.iSubItem = column;
    item.pszText = const_cast<wchar_t*>(text.c_str());
    SendMessageW(list, LVM_SETITEMTEXTW, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&item));
}

// One row's value, and the value the file has when the player changed it.
void ShowRow(HWND dlg, const State& s, int row) {
    HWND list = GetDlgItem(dlg, IDC_SE_FIELDS);
    const save::Field& f = s.fields[static_cast<size_t>(row)];
    const std::string now = save::FieldText(s.block, f, s.names), was = save::FieldText(s.original, f, s.names);
    SetCell(list, row, 1, W(now));
    SetCell(list, row, 2, now == was ? std::wstring() : W(was));
}

int FieldRow(HWND dlg) {
    return static_cast<int>(SendDlgItemMessageW(dlg, IDC_SE_FIELDS, LVM_GETNEXTITEM, static_cast<WPARAM>(-1), LVNI_SELECTED));
}

// The value box for the row selected: its text, its list, its note.
void ShowValue(HWND dlg, State& s) {
    s.filling = true;
    const int row = FieldRow(dlg);
    SendDlgItemMessageW(dlg, IDC_SE_VALUE, CB_RESETCONTENT, 0, 0);
    std::wstring note;
    bool can = false;
    if (s.loaded && row >= 0 && row < static_cast<int>(s.fields.size())) {
        const save::Field& f = s.fields[static_cast<size_t>(row)];
        for (const std::string& c : save::FieldChoices(s.block, f, s.names))
            SendDlgItemMessageW(dlg, IDC_SE_VALUE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(W(c).c_str()));
        SetDlgItemTextW(dlg, IDC_SE_VALUE, W(save::FieldText(s.block, f, s.names)).c_str());
        can = !save::FieldReadOnly(f);
        note = W(f.note);
        if (!can) note = (note.empty() ? L"" : note + L" ") + L"You cannot change this field.";
    } else {
        SetDlgItemTextW(dlg, IDC_SE_VALUE, L"");
    }
    SetDlgItemTextW(dlg, IDC_SE_NOTE, note.c_str());
    EnableWindow(GetDlgItem(dlg, IDC_SE_VALUE), can);
    EnableWindow(GetDlgItem(dlg, IDOK), can);
    s.filling = false;
}

void FillFields(HWND dlg, State& s) {
    HWND list = GetDlgItem(dlg, IDC_SE_FIELDS);
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list, LVM_DELETEALLITEMS, 0, 0);
    s.fields.clear();
    const int section = Section(dlg, s);
    if (s.loaded && section >= 0) {
        const int record = Selected(dlg, IDC_SE_RECORD);
        for (save::Field& f : save::SectionFields(section, record < 0 ? 0 : record))
            if (s.advanced || f.cls == 'E') s.fields.push_back(std::move(f));
        for (int i = 0; i < static_cast<int>(s.fields.size()); ++i) {
            const std::wstring label = W(s.fields[static_cast<size_t>(i)].label);
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = i;
            item.pszText = const_cast<wchar_t*>(label.c_str());
            SendMessageW(list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
            ShowRow(dlg, s, i);
        }
    }
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, TRUE);
    ShowValue(dlg, s);
}

void ShowFileNote(HWND dlg, const State& s) {
    std::wstring note;
    if (!s.loaded) note = L"Select a save file.";
    else if (Dirty(s)) note = L"You changed this save. The file does not change until you click Save.";
    else note = L"The file and the editor are the same.";
    SetDlgItemTextW(dlg, IDC_SE_FILENOTE, note.c_str());
    EnableWindow(GetDlgItem(dlg, IDC_SE_SAVE), s.loaded);
    EnableWindow(GetDlgItem(dlg, IDC_SE_RELOAD), s.loaded);
}

void Refresh(HWND dlg, State& s) {
    FillRecords(dlg, s);
    FillFields(dlg, s);
    ShowFileNote(dlg, s);
}

// ---- open and save ----------------------------------------------------------------

bool DiscardOk(HWND dlg, const State& s) {
    return !Dirty(s) || MessageBoxW(dlg, L"You changed this save and did not save it.\nDiscard the changes?",
                                    L"Save Editor", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
}

// Opens the entry the file box shows. False, with the box put back, when the
// file cannot be opened or the player keeps what is open.
bool Open(HWND dlg, State& s, int entry, const std::wstring& other) {
    const std::wstring path = entry == kOtherFile ? other : SlotPath(s, entry);
    save::Block b;
    std::string why;
    bool ok = save::ReadPcFile(path, b, why);
    if (!ok && entry != kOtherFile && GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        why = "This slot is empty.";
    if (ok && save::Checksum(b) != save::StoredChecksum(b))
        ok = MessageBoxW(dlg, L"The checksum of this save is incorrect. The game shows this save as damaged.\n\n"
                              L"Open it? When you save, the editor writes a correct checksum.",
                         L"Save Editor", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
    else if (!ok)
        MessageBoxW(dlg, W(why).c_str(), L"Save Editor", MB_OK | MB_ICONWARNING);
    if (!ok) {
        SendDlgItemMessageW(dlg, IDC_SE_SLOT, CB_SETCURSEL, static_cast<WPARAM>(s.loaded ? s.opened : -1), 0);
        return false;
    }
    s.loaded = true;
    s.opened = entry;
    if (entry == kOtherFile) s.other = other;
    s.original = s.block = b;
    FillSlots(dlg, s);
    SendDlgItemMessageW(dlg, IDC_SE_SLOT, CB_SETCURSEL, static_cast<WPARAM>(entry), 0);
    SendDlgItemMessageW(dlg, IDC_SE_TARGET, CB_SETCURSEL, static_cast<WPARAM>(entry), 0);
    Status(dlg, L"");
    Refresh(dlg, s);
    return true;
}

void PickSlot(HWND dlg, State& s) {
    const int entry = Selected(dlg, IDC_SE_SLOT);
    if (entry < 0 || (s.loaded && entry == s.opened && entry != kOtherFile)) return;
    if (!DiscardOk(dlg, s)) {
        SendDlgItemMessageW(dlg, IDC_SE_SLOT, CB_SETCURSEL, static_cast<WPARAM>(s.opened), 0);
        return;
    }
    std::wstring other;
    if (entry == kOtherFile) {
        wchar_t file[MAX_PATH] = L"";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof ofn;
        ofn.hwndOwner = dlg;
        ofn.lpstrFilter = L"Save files (*.DAT)\0*.DAT\0All files\0*.*\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = s.game_dir.c_str();
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&ofn)) {
            SendDlgItemMessageW(dlg, IDC_SE_SLOT, CB_SETCURSEL, static_cast<WPARAM>(s.loaded ? s.opened : -1), 0);
            return;
        }
        other = file;
    }
    Open(dlg, s, entry, other);
}

void Save(HWND dlg, State& s) {
    if (!s.loaded) return;
    if (GameRuns()) {
        MessageBoxW(dlg, L"The game is running. Close the game, then save.", L"Save Editor", MB_OK | MB_ICONWARNING);
        return;
    }
    const int target = Selected(dlg, IDC_SE_TARGET);
    if (target < 0) return;
    const std::wstring path = target == kOtherFile ? s.other : SlotPath(s, target);

    save::Block out = s.block;
    save::Finish(s.original, out, s.table);
    std::wstring errors, warnings;
    for (const save::Issue& i : save::Check(s.original, out, s.table))
        (i.error ? errors : warnings) += L"- " + W(i.text) + L"\n";
    if (!errors.empty()) {
        MessageBoxW(dlg, (L"The editor cannot save these values:\n\n" + errors).c_str(), L"Save Editor",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    if (!warnings.empty() &&
        MessageBoxW(dlg, (L"These values are unusual:\n\n" + warnings + L"\nSave them?").c_str(), L"Save Editor",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;
    if (target != s.opened && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES &&
        MessageBoxW(dlg, L"This slot has a save. Replace it?\nThe editor keeps a copy of the save in the save-backups folder.",
                    L"Save Editor", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
        return;

    std::wstring backup;
    std::string why;
    if (!save::WritePcFile(path, out, s.game_dir + L"\\save-backups", backup, why)) {
        MessageBoxW(dlg, W(why).c_str(), L"Save Editor", MB_OK | MB_ICONERROR);
        return;
    }
    s.original = s.block = out;
    s.opened = target;
    FillSlots(dlg, s);
    SendDlgItemMessageW(dlg, IDC_SE_SLOT, CB_SETCURSEL, static_cast<WPARAM>(target), 0);
    SendDlgItemMessageW(dlg, IDC_SE_TARGET, CB_SETCURSEL, static_cast<WPARAM>(target), 0);
    Refresh(dlg, s);
    Status(dlg, backup.empty() ? L"Saved." : L"Saved. A copy of the file from before is in " + backup);
}

// The value box's text into the row selected.
void Apply(HWND dlg, State& s, const std::wstring& text) {
    const int row = FieldRow(dlg);
    if (!s.loaded || row < 0 || row >= static_cast<int>(s.fields.size())) return;
    std::string why;
    if (!save::FieldSet(s.block, s.fields[static_cast<size_t>(row)], U(text), s.names, why)) {
        Status(dlg, W(why));
        MessageBeep(MB_ICONWARNING);
        return;
    }
    Status(dlg, L"");
    ShowRow(dlg, s, row);
    ShowValue(dlg, s);
    ShowFileNote(dlg, s);
}

void InitList(HWND dlg) {
    HWND list = GetDlgItem(dlg, IDC_SE_FIELDS);
    SendMessageW(list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    RECT r;
    GetClientRect(list, &r);
    const int w = r.right - GetSystemMetrics(SM_CXVSCROLL);
    const struct {
        const wchar_t* title;
        int width;
    } columns[] = {{L"Field", w * 42 / 100}, {L"Value", w * 33 / 100}, {L"In the file", w * 25 / 100}};
    for (int i = 0; i < 3; ++i) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = const_cast<wchar_t*>(columns[i].title);
        c.cx = columns[i].width;
        c.iSubItem = i;
        SendMessageW(list, LVM_INSERTCOLUMNW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&c));
    }
}

INT_PTR CALLBACK Proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
    switch (msg) {
    case WM_INITDIALOG: {
        s = reinterpret_cast<State*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        InitList(dlg);
        FillSlots(dlg, *s);
        FillSections(dlg, *s);
        Refresh(dlg, *s);
        std::wstring intro = s->names.item[0].empty()
                                 ? L"This install has no names that the editor can show. The lists show numbers."
                                 : L"";
        if (s->table.empty())
            intro += (intro.empty() ? L"" : L" ") +
                     std::wstring(L"This install has no BOF3.exe: you can change the order of the party, not its members.");
        Status(dlg, intro);
        return TRUE;
    }
    case WM_NOTIFY: {
        const auto* n = reinterpret_cast<const NMHDR*>(lp);
        if (!s || n->idFrom != IDC_SE_FIELDS) break;
        if (n->code == LVN_ITEMCHANGED) {
            const auto* c = reinterpret_cast<const NMLISTVIEW*>(lp);
            if ((c->uChanged & LVIF_STATE) && (c->uNewState & LVIS_SELECTED) && !(c->uOldState & LVIS_SELECTED))
                ShowValue(dlg, *s);
            return TRUE;
        }
        if (n->code == NM_DBLCLK) {
            // A switch changes on a double-click; any other field takes the keys.
            const int row = FieldRow(dlg);
            if (row >= 0 && row < static_cast<int>(s->fields.size())) {
                const save::Field& f = s->fields[static_cast<size_t>(row)];
                if (f.type == save::Type::kBit && !save::FieldReadOnly(f))
                    Apply(dlg, *s, save::FieldText(s->block, f, s->names) == "On" ? L"Off" : L"On");
                else
                    SetFocus(GetDlgItem(dlg, IDC_SE_VALUE));
            }
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        if (s && DiscardOk(dlg, *s)) EndDialog(dlg, 0);
        return TRUE;
    case WM_COMMAND: {
        if (!s) break;
        switch (LOWORD(wp)) {
        case IDOK:   // Set, and Enter in the value box
            Apply(dlg, *s, Text(dlg, IDC_SE_VALUE));
            return TRUE;
        case IDCANCEL:
            if (DiscardOk(dlg, *s)) EndDialog(dlg, 0);
            return TRUE;
        case IDC_SE_SLOT:
            if (HIWORD(wp) == CBN_SELCHANGE) PickSlot(dlg, *s);
            return TRUE;
        case IDC_SE_RELOAD:
            if (s->loaded && DiscardOk(dlg, *s)) {
                s->block = s->original;   // DiscardOk asked; Open reads the file again
                Open(dlg, *s, s->opened, s->other);
            }
            return TRUE;
        case IDC_SE_SECTIONS:
            if (HIWORD(wp) == LBN_SELCHANGE) {
                SendDlgItemMessageW(dlg, IDC_SE_RECORD, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                FillRecords(dlg, *s);
                FillFields(dlg, *s);
            }
            return TRUE;
        case IDC_SE_RECORD:
            if (HIWORD(wp) == CBN_SELCHANGE && !s->filling) FillFields(dlg, *s);
            return TRUE;
        case IDC_SE_VALUE:
            // An entry picked from the list is set at once.
            if (HIWORD(wp) == CBN_SELENDOK && !s->filling) {
                const int pick = Selected(dlg, IDC_SE_VALUE);
                if (pick >= 0) {
                    const LRESULT n = SendDlgItemMessageW(dlg, IDC_SE_VALUE, CB_GETLBTEXTLEN, static_cast<WPARAM>(pick), 0);
                    std::wstring text(static_cast<size_t>(n) + 1, L'\0');
                    SendDlgItemMessageW(dlg, IDC_SE_VALUE, CB_GETLBTEXT, static_cast<WPARAM>(pick),
                                        reinterpret_cast<LPARAM>(text.data()));
                    text.resize(static_cast<size_t>(n));
                    Apply(dlg, *s, text);
                }
            }
            return TRUE;
        case IDC_SE_ADVANCED:
            if (HIWORD(wp) == BN_CLICKED) {
                s->advanced = IsDlgButtonChecked(dlg, IDC_SE_ADVANCED) == BST_CHECKED;
                FillSections(dlg, *s);
                Refresh(dlg, *s);
            }
            return TRUE;
        case IDC_SE_SAVE:
            Save(dlg, *s);
            return TRUE;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    return FALSE;
}

}  // namespace

void SaveEditorRun(HWND owner, const std::wstring& game_dir, const std::wstring& cache, const std::string& language) {
    State s;
    s.game_dir = game_dir;
    while (!s.game_dir.empty() && (s.game_dir.back() == L'\\' || s.game_dir.back() == L'/')) s.game_dir.pop_back();

    // The names: the language layer's FIRST.DAT, in the game's DAT\ or in the cache.
    const NameTableTags t = ExeNameTableTags();
    const save::NameTags tags{t.items, t.key_items, t.weapons, t.armour, t.accessories, t.abilities};
    const bool latin = save::LanguageIsLatin(language);
    const std::wstring tag = ConfigFromUtf8(language);
    s.names = save::LoadNames(s.game_dir + L"\\DAT\\" + tag + L".FIRST.DAT", tags, latin);
    if (s.names.item[0].empty() && !cache.empty())
        s.names = save::LoadNames(cache + L"\\loc\\" + tag + L"\\dat\\FIRST.DAT", tags, latin);

    std::string why;
    if (!ExePartyTable(s.game_dir + L"\\BOF3.exe", s.table, why)) s.table.clear();

    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SAVEEDIT), owner, Proc, reinterpret_cast<LPARAM>(&s));
}

}  // namespace bof3x

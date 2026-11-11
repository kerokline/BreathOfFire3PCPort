// The save editor's window: a list of sections, the section's fields in a
// list, and one value box. The two file boxes hold the PC game's sixteen
// slots and the saves of one memory card file (launcher/save/card.h); a save
// written to the other kind is converted. Every row is made from the field table
// (launcher/save/block.h), so the window holds no offset of its own.
// docs/save-editor.md says what it does; docs/save-editor-spec.md why.
#include "launcher/save_dialog.h"

#include <commctrl.h>
#include <commdlg.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "launcher/config.h"
#include "launcher/exe_image.h"
#include "launcher/resource.h"
#include "launcher/save/block.h"
#include "launcher/save/card.h"
#include "launcher/save/namemap.h"
#include "launcher/save/names.h"

namespace bof3x {

namespace {

// What a row of the two file boxes is.
enum class Kind {
    kPcSlot,     // BISLPS0X.DAT of the game folder; `slot` 0..15
    kPcFile,     // a PC save file anywhere else: `path`
    kCardSave,   // a save on the memory card: `slot` is its directory frame
    kCardNew,    // a place for a new save on the memory card: `prefix` + `number`
    kPickFile,   // "A different file..."
    kPickCard,   // "A memory card file..."
};
struct Entry {
    Kind kind = Kind::kPcSlot;
    int slot = 0;
    std::string prefix;
    int number = -1;
};
bool SameEntry(const Entry& a, const Entry& b) {
    return a.kind == b.kind && a.slot == b.slot && a.prefix == b.prefix && a.number == b.number;
}
// The rows compare without the name a card save carries.
Entry RowOf(const Entry& e) { return e.kind == Kind::kCardSave ? Entry{e.kind, e.slot, {}, -1} : e; }

struct State {
    std::wstring game_dir;
    save::Names names;
    save::PartyTable table;
    std::vector<save::NameSet> name_sets;   // each release's default names (save/namemap.h): the discs of the
                                            // languages installed, this install's layers, the exe's

    bool loaded = false;
    Entry opened;                 // where the open save came from
    std::wstring opened_path;     // its file, for kPcFile and kCardSave
    std::vector<std::uint8_t> opened_header;   // a card save's 0x200 bytes before the block: its icon goes with it
    save::Block original{}, block{};

    // The memory card that the two file boxes show. One at a time.
    std::wstring card_path;
    save::Card card;
    bool card_new = false;        // not a file yet: the first save makes it

    std::vector<Entry> sources, targets;   // the rows of the two file boxes

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

bool IsCard(Kind k) { return k == Kind::kCardSave || k == Kind::kCardNew; }

std::wstring FileName(const std::wstring& path) {
    const size_t cut = path.find_last_of(L"\\/");
    return cut == std::wstring::npos ? path : path.substr(cut + 1);
}

// The release a card save's file name belongs to, in words: by the language
// installed from that disc, else by the two names measured, else the name.
std::wstring ReleaseWords(const State& s, const std::string& prefix) {
    std::string tag;
    for (const save::NameSet& set : s.name_sets)
        if (!set.prefix.empty() && set.prefix == prefix) tag = set.id;
    if (tag == "en-US" || (tag.empty() && prefix == save::kCardPrefixUs)) return L"US release";
    if (tag == "ja-JP" || (tag.empty() && prefix == save::kCardPrefixJp)) return L"Japan release";
    if (tag == "en-150") return L"European release";
    if (tag == "fr-FR") return L"French release";
    if (tag == "de-DE") return L"German release";
    return W(prefix);
}

// The file name prefix of the card save in a directory frame.
std::string CardPrefixAt(const State& s, int slot) {
    for (const save::CardEntry& e : save::CardEntries(s.card))
        if (e.slot == slot) return e.prefix;
    return {};
}

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

std::wstring CardSaveLabel(const State& s, const save::CardEntry& e) {
    save::Block b;
    std::string why;
    std::wstring text = L"Card save " + std::to_wstring(e.number + 1) + L", " + ReleaseWords(s, e.prefix) + L" - ";
    if (!save::CardGet(s.card, e.slot, b, why)) return text + L"not a save that the editor can open";
    const bool good = save::Checksum(b) == save::StoredChecksum(b);
    // For the line only: the names the game gave, as this install writes them, so that the line can be read.
    for (const save::NameSet& set : s.name_sets)
        if (set.id == "pc" && set.Known()) save::TranslateNames(b, s.name_sets, set);
    return text + W(save::Describe(b, s.names)) + (good ? L"" : L" (damaged)");
}

int FindEntry(const std::vector<Entry>& rows, const Entry& e) {
    for (size_t i = 0; i < rows.size(); ++i)
        if (SameEntry(rows[i], e)) return static_cast<int>(i);
    return -1;
}

// The row of the open save in a box, or -1: a card save is a row only while its card is the card shown.
int OpenedRow(const State& s, const std::vector<Entry>& rows) {
    if (!s.loaded) return -1;
    if (s.opened.kind == Kind::kCardSave && s.opened_path != s.card_path) return -1;
    return FindEntry(rows, RowOf(s.opened));
}

void Select(HWND dlg, int id, int row) { SendDlgItemMessageW(dlg, id, CB_SETCURSEL, static_cast<WPARAM>(row), 0); }

// Fills the two boxes. The first selects the open save; the second keeps `target` when it is still a row.
void FillSlots(HWND dlg, State& s, const Entry* target = nullptr) {
    Entry keep;
    const int was = Selected(dlg, IDC_SE_TARGET);
    const bool had = was >= 0 && was < static_cast<int>(s.targets.size());
    if (target) keep = *target;
    else if (had) keep = s.targets[static_cast<size_t>(was)];

    s.sources.clear();
    s.targets.clear();
    for (const int id : {IDC_SE_SLOT, IDC_SE_TARGET}) {
        const bool source = id == IDC_SE_SLOT;
        std::vector<Entry>& rows = source ? s.sources : s.targets;
        SendDlgItemMessageW(dlg, id, CB_RESETCONTENT, 0, 0);
        const auto add = [&](const Entry& e, const std::wstring& text) {
            rows.push_back(e);
            SendDlgItemMessageW(dlg, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        };
        for (int i = 0; i < save::kSlots; ++i) add({Kind::kPcSlot, i, {}, -1}, SlotLabel(s, i));
        if (!s.card_path.empty()) {
            for (const save::CardEntry& e : save::CardEntries(s.card))
                if (e.bof3) add({Kind::kCardSave, e.slot, {}, -1}, CardSaveLabel(s, e));
            if (!source) {
                // A new save for each release the player has installed as a language (its disc gives the
                // file name and the names), and for the release of the open card save: it can keep its name.
                std::vector<std::string> prefixes;
                const auto offer = [&prefixes](const std::string& p) {
                    for (const std::string& have : prefixes)
                        if (have == p) return;
                    if (!p.empty()) prefixes.push_back(p);
                };
                for (const save::NameSet& set : s.name_sets) offer(set.prefix);
                if (s.loaded && s.opened.kind == Kind::kCardSave) offer(s.opened.prefix);
                for (const std::string& p : prefixes) {
                    const int n = save::CardFreeNumber(s.card, p);
                    if (n >= 0 && save::CardFreeSlot(s.card))
                        add({Kind::kCardNew, 0, p, n},
                            L"Card: a new save, " + ReleaseWords(s, p) + L" (save " + std::to_wstring(n + 1) + L")");
                }
            }
        }
        if (s.loaded && s.opened.kind == Kind::kPcFile)
            add({Kind::kPcFile, 0, {}, -1}, source ? L"File: " + s.opened_path : L"The file that is open");
        if (source) add({Kind::kPickFile, 0, {}, -1}, L"A different file or memory card...");
        else add({Kind::kPickCard, 0, {}, -1}, s.card_path.empty() ? L"A memory card file..." : L"A different memory card file...");
    }
    Select(dlg, IDC_SE_SLOT, OpenedRow(s, s.sources));
    int row = target || had ? FindEntry(s.targets, keep) : -1;
    if (row < 0) row = OpenedRow(s, s.targets);
    Select(dlg, IDC_SE_TARGET, row);
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
        for (save::Field& f : save::SectionFields(section, record < 0 ? 0 : record, s.names))
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
    if (!s.card_path.empty())
        note += L" Memory card: " + FileName(s.card_path) + (s.card_new ? L" (a new file)" : L"");
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

void Warn(HWND dlg, const std::string& why) { MessageBoxW(dlg, W(why).c_str(), L"Save Editor", MB_OK | MB_ICONWARNING); }

// A file name from the player. `must_exist` false lets the player write the name of a new file.
bool PickFile(HWND dlg, const State& s, const wchar_t* filter, bool must_exist, std::wstring& path) {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = dlg;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = s.game_dir.c_str();
    ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | (must_exist ? OFN_FILEMUSTEXIST : 0);
    if (!GetOpenFileNameW(&ofn)) return false;
    path = file;
    return true;
}

constexpr wchar_t kCardFilter[] =
    L"Memory card files (*.mcr;*.mcd;*.srm;*.mc;*.mcs;*.mem;*.ps;*.bin)\0*.mcr;*.mcd;*.srm;*.mc;*.mcs;*.mem;*.ps;*.bin\0"
    L"All files\0*.*\0";
constexpr wchar_t kAnyFilter[] =
    L"Save files and memory card files\0*.DAT;*.mcr;*.mcd;*.srm;*.mc;*.mcs;*.mem;*.ps;*.bin\0"
    L"All files\0*.*\0";

bool FileIsCard(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    return GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a) && a.nFileSizeHigh == 0 &&
           a.nFileSizeLow == save::kCardSize;
}

// Makes `path` the memory card that the boxes show. A file that is not there
// yet is a new, empty card when `may_be_new`. False, with a message, when the
// file is not a card.
bool AttachCard(HWND dlg, State& s, const std::wstring& path, bool may_be_new) {
    save::Card card;
    bool fresh = false;
    if (may_be_new && GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        card = save::EmptyCard();
        fresh = true;
    } else {
        std::string why;
        if (!save::ReadCardFile(path, card, why)) {
            Warn(dlg, why);
            return false;
        }
    }
    s.card = std::move(card);
    s.card_path = path;
    s.card_new = fresh;
    return true;
}

// Opens a row of the file box. False, with the box put back, when the save
// cannot be opened or the player keeps what is open.
bool Open(HWND dlg, State& s, const Entry& entry, const std::wstring& path) {
    save::Block b;
    std::string why;
    bool ok = false;
    std::vector<std::uint8_t> header;
    Entry opened = entry;
    if (entry.kind == Kind::kCardSave) {
        ok = save::CardGet(s.card, entry.slot, b, why);
        for (const save::CardEntry& e : save::CardEntries(s.card))
            if (e.slot == entry.slot) {
                opened.prefix = e.prefix;
                opened.number = e.number;
            }
        if (ok) header.assign(save::CardHeader(s.card, entry.slot), save::CardHeader(s.card, entry.slot) + 0x200);
    } else {
        ok = save::ReadPcFile(path, b, why);
        if (!ok && entry.kind == Kind::kPcSlot && GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            why = "This slot is empty.";
    }
    if (ok && save::Checksum(b) != save::StoredChecksum(b))
        ok = MessageBoxW(dlg, L"The checksum of this save is incorrect. The game shows this save as damaged.\n\n"
                              L"Open it? When you save, the editor writes a correct checksum.",
                         L"Save Editor", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
    else if (!ok)
        Warn(dlg, why);
    if (!ok) {
        Select(dlg, IDC_SE_SLOT, OpenedRow(s, s.sources));
        return false;
    }
    s.loaded = true;
    s.opened = opened;
    s.opened_path = entry.kind == Kind::kCardSave ? s.card_path : path;
    s.opened_header = std::move(header);
    s.original = s.block = b;
    const Entry same{opened.kind, opened.slot, {}, -1};
    FillSlots(dlg, s, &same);
    Status(dlg, L"");
    Refresh(dlg, s);
    return true;
}

void PickSlot(HWND dlg, State& s) {
    const int row = Selected(dlg, IDC_SE_SLOT);
    if (row < 0 || row >= static_cast<int>(s.sources.size())) return;
    const Entry entry = s.sources[static_cast<size_t>(row)];
    if (row == OpenedRow(s, s.sources)) return;
    if (!DiscardOk(dlg, s)) {
        Select(dlg, IDC_SE_SLOT, OpenedRow(s, s.sources));
        return;
    }
    if (entry.kind == Kind::kPickFile) {
        std::wstring path;
        if (!PickFile(dlg, s, kAnyFilter, true, path)) {
            Select(dlg, IDC_SE_SLOT, OpenedRow(s, s.sources));
            return;
        }
        if (!FileIsCard(path)) {
            Open(dlg, s, {Kind::kPcFile, 0, {}, -1}, path);
            return;
        }
        if (!AttachCard(dlg, s, path, false)) {
            Select(dlg, IDC_SE_SLOT, OpenedRow(s, s.sources));
            return;
        }
        // The card's first save of this game, when it has one.
        for (const save::CardEntry& e : save::CardEntries(s.card))
            if (e.bof3) {
                if (!Open(dlg, s, {Kind::kCardSave, e.slot, {}, -1}, path)) FillSlots(dlg, s);
                ShowFileNote(dlg, s);
                return;
            }
        FillSlots(dlg, s);
        ShowFileNote(dlg, s);
        Status(dlg, L"This memory card has no save of this game. To write a save to the card, select the card in \"Write to\".");
        return;
    }
    if (entry.kind == Kind::kCardSave && !s.card_new) {
        // The card file again, as it is now.
        const int slot = entry.slot;
        if (!AttachCard(dlg, s, s.card_path, false)) return;
        Open(dlg, s, {Kind::kCardSave, slot, {}, -1}, s.card_path);
        return;
    }
    Open(dlg, s, entry, entry.kind == Kind::kPcSlot ? SlotPath(s, entry.slot) : s.opened_path);
}

// The player picked "A memory card file..." in the box to write to.
void PickTargetCard(HWND dlg, State& s) {
    std::wstring path;
    if (PickFile(dlg, s, kCardFilter, false, path)) {
        if (path.find(L'.', path.find_last_of(L"\\/") == std::wstring::npos ? 0 : path.find_last_of(L"\\/")) ==
            std::wstring::npos)
            path += L".mcr";
        if (AttachCard(dlg, s, path, true)) {
            FillSlots(dlg, s);
            // A new save for the release of the open save, else the first row of the card.
            int row = -1;
            for (size_t i = 0; i < s.targets.size() && row < 0; ++i)
                if (s.targets[i].kind == Kind::kCardNew &&
                    (s.opened.kind != Kind::kCardSave || s.targets[i].prefix == s.opened.prefix))
                    row = static_cast<int>(i);
            for (size_t i = 0; i < s.targets.size() && row < 0; ++i)
                if (IsCard(s.targets[i].kind)) row = static_cast<int>(i);
            if (row < 0) {
                bool any = false;
                for (const save::NameSet& set : s.name_sets) any = any || !set.prefix.empty();
                Status(dlg, any ? L"This memory card has no room for a save of this game."
                                : L"The editor makes a new card save for a PlayStation release that you installed as a "
                                  L"language. Install one, or build the language again, then open the editor.");
                row = OpenedRow(s, s.targets);
            } else {
                Status(dlg, s.card_new ? L"The editor makes this memory card file when you click Save." : L"");
            }
            Select(dlg, IDC_SE_TARGET, row);
            ShowFileNote(dlg, s);
            return;
        }
    }
    Select(dlg, IDC_SE_TARGET, OpenedRow(s, s.targets));
}

void Save(HWND dlg, State& s) {
    if (!s.loaded) return;
    const int row = Selected(dlg, IDC_SE_TARGET);
    if (row < 0 || row >= static_cast<int>(s.targets.size())) {
        Status(dlg, L"Select where to write the save in \"Write to\".");
        return;
    }
    const Entry target = s.targets[static_cast<size_t>(row)];
    if (target.kind == Kind::kPickCard) {
        PickTargetCard(dlg, s);
        return;
    }
    const bool to_card = IsCard(target.kind), from_card = s.opened.kind == Kind::kCardSave;
    if (!to_card && GameRuns()) {
        MessageBoxW(dlg, L"The game is running. Close the game, then save.", L"Save Editor", MB_OK | MB_ICONWARNING);
        return;
    }
    const std::wstring path = target.kind == Kind::kPcSlot ? SlotPath(s, target.slot)
                              : target.kind == Kind::kPcFile ? s.opened_path : s.card_path;

    save::Block out = s.block;
    // A save that changes game: the names the game gave become the names the other game gives (namemap.h).
    const save::NameSet* names_to = nullptr;
    save::NameReport renamed;
    if (to_card != from_card) {
        const std::string want = !to_card ? "pc"
                                 : target.kind == Kind::kCardNew ? target.prefix : CardPrefixAt(s, target.slot);
        // The card's release must be one the player installed: its disc tells which names the game gave.
        const std::string card_release = to_card ? want : s.opened.prefix;
        bool installed = false;
        for (const save::NameSet& set : s.name_sets) installed = installed || (!set.prefix.empty() && set.prefix == card_release);
        for (const save::NameSet& set : s.name_sets)
            if (installed && (to_card ? set.prefix == want : set.id == want) && set.Known()) names_to = &set;
        if (names_to) renamed = save::TranslateNames(out, s.name_sets, *names_to);
    }
    save::Finish(s.original, out, s.table);
    std::wstring errors, warnings;
    for (const save::Issue& i : save::Check(s.original, out, s.table))
        (i.error ? errors : warnings) += L"- " + W(i.text) + L"\n";
    std::string why;
    if (to_card) {
        std::vector<std::uint8_t> psx(save::kBlockSize);
        if (!save::PcToPsx(out, psx.data(), why)) errors += L"- " + W(why) + L"\n";
    }
    if (!errors.empty()) {
        MessageBoxW(dlg, (L"The editor cannot save these values:\n\n" + errors).c_str(), L"Save Editor",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    if (!warnings.empty() &&
        MessageBoxW(dlg, (L"These values are unusual:\n\n" + warnings + L"\nSave them?").c_str(), L"Save Editor",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;

    // A save that goes from one game to the other: say what changes, then ask.
    if (to_card != from_card) {
        std::wstring text = to_card ? L"This save goes from the PC game to a PlayStation memory card.\n\n"
                                    : L"This save goes from a PlayStation memory card to the PC game.\n\n";
        if (names_to) {
            text += L"- The game gave " + std::to_wstring(renamed.chars) + L" character names and " +
                    std::to_wstring(renamed.faeries) + L" faerie names of this save. The editor writes them as the names of " +
                    std::wstring(to_card ? L"that release" : L"this install") + L".\n";
            if (!renamed.kept.empty()) {
                text += L"- These names are not names that the game gave. The editor writes them as they are, and they "
                        L"can show incorrectly in the other game:";
                for (size_t i = 0; i < renamed.kept.size() && i < 12; ++i) text += (i ? L", " : L" ") + W(renamed.kept[i]);
                if (renamed.kept.size() > 12) text += L", and " + std::to_wstring(renamed.kept.size() - 12) + L" more";
                text += L". You can change them before you save.\n";
            }
        } else {
            text += L"- The editor writes the names of the characters and of the faeries as they are, because the release "
                    L"of this card save is not installed as a language, or its language was built by an older version. "
                    L"Each release of the game has its own letters, thus the names can show incorrectly in the other "
                    L"game. To change the names automatically, install that release as a language or build it again. "
                    L"You can also change the names in Characters before you save.\n";
        }
        text += L"- The button setup goes with the save.\n";
        if (to_card && (out[0x1040] || out[0x7C] || out[0x7D]))
            text += L"- The editor clears the values that only the PC game has (Mountain passes open, two options).\n";
        if (to_card)
            text += L"\nNobody has loaded a save from the PC game in a PlayStation game or an emulator yet. Keep a "
                    L"copy of your memory card.\n";
        text += L"\nContinue?";
        if (MessageBoxW(dlg, text.c_str(), L"Save Editor", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        if (to_card) save::ClearPcOnly(out);
    }

    const bool same_place = s.loaded && SameEntry(RowOf(s.opened), target) &&
                            (target.kind == Kind::kPcSlot || s.opened_path == path);
    const bool replaces = target.kind == Kind::kCardSave ||
                          (!to_card && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES);
    if (!same_place && replaces &&
        MessageBoxW(dlg, to_card ? L"This place on the memory card has a save. Replace it?\nThe editor keeps a copy of "
                                   L"the memory card file in the save-backups folder."
                                 : L"This slot has a save. Replace it?\nThe editor keeps a copy of the save in the "
                                   L"save-backups folder.",
                    L"Save Editor", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
        return;

    std::wstring backup;
    Entry now = target;
    std::vector<std::uint8_t> header = s.opened_header;
    if (to_card) {
        save::Card card = s.card;
        int slot = target.slot;
        if (target.kind == Kind::kCardSave) {
            if (!save::CardPut(card, slot, out, why)) slot = 0;
        } else {
            // The icon: the open save's when it came from a card, else that of a save of this game on the card.
            const std::uint8_t* icon = from_card && s.opened_header.size() == 0x200 ? s.opened_header.data() : nullptr;
            for (const save::CardEntry& e : save::CardEntries(card))
                if (!icon && e.bof3) icon = save::CardHeader(s.card, e.slot);
            slot = save::CardAdd(card, target.prefix, target.number, out, icon, why);
        }
        if (!slot || !save::WriteCardFile(path, card, s.game_dir + L"\\save-backups", backup, why)) {
            MessageBoxW(dlg, W(why).c_str(), L"Save Editor", MB_OK | MB_ICONERROR);
            return;
        }
        s.card = std::move(card);
        s.card_new = false;
        now = {Kind::kCardSave, slot, {}, -1};
        for (const save::CardEntry& e : save::CardEntries(s.card))
            if (e.slot == slot) {
                now.prefix = e.prefix;
                now.number = e.number;
            }
        header.assign(save::CardHeader(s.card, slot), save::CardHeader(s.card, slot) + 0x200);
    } else {
        if (!save::WritePcFile(path, out, s.game_dir + L"\\save-backups", backup, why)) {
            MessageBoxW(dlg, W(why).c_str(), L"Save Editor", MB_OK | MB_ICONERROR);
            return;
        }
        header.clear();
    }
    s.original = s.block = out;
    s.opened = now;
    s.opened_path = path;
    s.opened_header = std::move(header);
    const Entry same = RowOf(now);
    FillSlots(dlg, s, &same);
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
    } columns[] = {{L"Field", w * 32 / 100}, {L"Value", w * 43 / 100}, {L"In the file", w * 25 / 100}};
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
        // The lists of the two file boxes are as wide as the window: a card save's line is long.
        RECT wide{0, 0, 416, 0};
        MapDialogRect(dlg, &wide);
        for (const int id : {IDC_SE_SLOT, IDC_SE_TARGET})
            SendDlgItemMessageW(dlg, id, CB_SETDROPPEDWIDTH, static_cast<WPARAM>(wide.right), 0);
        FillSlots(dlg, *s);
        FillSections(dlg, *s);
        Refresh(dlg, *s);
        // The window opens on the first slot that holds a good save, so it never
        // starts empty on an install that has saves.
        for (int i = 0; i < save::kSlots; ++i) {
            save::Block b;
            std::string why;
            if (save::ReadPcFile(SlotPath(*s, i), b, why) && save::Checksum(b) == save::StoredChecksum(b)) {
                Open(dlg, *s, {Kind::kPcSlot, i, {}, -1}, SlotPath(*s, i));
                break;
            }
        }
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
                const Entry row = RowOf(s->opened);
                if (row.kind == Kind::kCardSave) {
                    // The card file again; a card that is not a file yet has nothing newer.
                    if (!s->card_new || s->opened_path != s->card_path)
                        if (!AttachCard(dlg, *s, s->opened_path, false)) return TRUE;
                    Open(dlg, *s, row, s->opened_path);
                } else {
                    Open(dlg, *s, row, row.kind == Kind::kPcSlot ? SlotPath(*s, row.slot) : s->opened_path);
                }
            }
            return TRUE;
        case IDC_SE_TARGET:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                const int row = Selected(dlg, IDC_SE_TARGET);
                if (row >= 0 && row < static_cast<int>(s->targets.size()) &&
                    s->targets[static_cast<size_t>(row)].kind == Kind::kPickCard)
                    PickTargetCard(dlg, *s);
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

    // The reference names, beside the launcher (data/save-names.tsv; as the Jukebox's music-names.tsv).
    wchar_t self[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring beside(self, n < MAX_PATH ? n : 0);
    beside.erase(beside.find_last_of(L"\\/") == std::wstring::npos ? 0 : beside.find_last_of(L"\\/"));
    save::LoadReferenceNames(beside + L"\\save-names.tsv", s.names);
    // A kind of fish is an item of the install (item 0x38 + kind, which the
    // fishing panel's row reads: FieldPanel_DrawKindRow), so where the install
    // gives item names the fish take them, in the install's language.
    for (unsigned k = 0; k < 22; ++k)
        if (0x38 + k < s.names.item[0].size() && !s.names.item[0][0x38 + k].empty())
            s.names.fish[k] = s.names.item[0][0x38 + k];

    std::string why;
    if (!ExePartyTable(s.game_dir + L"\\BOF3.exe", s.table, why)) s.table.clear();

    // Each release's default names (save/namemap.h), all from what the player installed:
    //  - the exe's own, from its tables: the seven default records New Game copies (Char_DefaultRecords
    //    0x64B390, 0xA4 apart, the name first) and the whelp's (0x64B80C), and the sixty faerie trait records
    //    (0x653200, 20 bytes each, the name first) - docs/save-block/range-b-characters.md, range-e-tail.md;
    //  - for each language installed (the Language box's list): the disc's own names and card file name, and
    //    the names as the PC plays that language, both from the layer's FIRST.DAT.
    {
        std::vector<unsigned char> records, whelp, traits;
        const std::wstring exe = s.game_dir + L"\\BOF3.exe";
        save::NameSet original;
        if (ExeImageRead(exe, 0x64B390, 7 * 0xA4, records, why) && ExeImageRead(exe, 0x64B80C, 9, whelp, why) &&
            ExeImageRead(exe, 0x653200, 60 * 20, traits, why)) {
            std::vector<std::uint8_t> chars(8 * 9), faeries(60 * 5);
            for (int k = 0; k < 7; ++k) std::memcpy(chars.data() + k * 9, records.data() + k * 0xA4, 9);
            std::memcpy(chars.data() + 7 * 9, whelp.data(), 9);
            for (int r = 0; r < 60; ++r) std::memcpy(faeries.data() + r * 5, traits.data() + r * 20, 5);
            original = save::NameSetFromTables("pc-original", chars.data(), faeries.data());
        }
        bool played = false;
        for (const std::string& code : ConfigLanguagesAvailable(s.game_dir, cache)) {
            const std::wstring w = ConfigFromUtf8(code);
            const std::wstring files[2] = {s.game_dir + L"\\DAT\\" + w + L".FIRST.DAT",
                                           cache.empty() ? std::wstring() : cache + L"\\loc\\" + w + L"\\dat\\FIRST.DAT"};
            save::NameSet disc, layer;
            for (const std::wstring& f : files)
                if (!f.empty() && save::NameSetFromDisc(f, code, disc)) {
                    s.name_sets.push_back(disc);
                    break;
                }
            for (const std::wstring& f : files)
                if (!f.empty() && save::NameSetFromLayer(f, code == language ? "pc" : "pc-" + code, original, layer)) {
                    s.name_sets.push_back(layer);
                    played = played || code == language;
                    break;
                }
        }
        if (original.Known()) {
            s.name_sets.push_back(original);
            if (!played) {   // the port's own text is what plays
                original.id = "pc";
                s.name_sets.push_back(original);
            }
        }
    }

    // The ways into each area, from the install's link lists: one entrance for
    // each different position, named by the areas it comes from.
    std::vector<AreaLink> links;
    if (ExeAreaLinks(s.game_dir + L"\\BOF3.exe", links, why)) {
        const auto place = [&s](unsigned area) {
            const auto it = s.names.area.find(area);
            return it == s.names.area.end() ? "area " + std::to_string(area) : it->second + " (" + std::to_string(area) + ")";
        };
        struct Group {
            const AreaLink* first;
            std::vector<unsigned> from;
        };
        std::vector<Group> groups;
        for (const AreaLink& l : links) {
            Group* g = nullptr;
            for (Group& have : groups)
                if (have.first->to == l.to && have.first->x == l.x && have.first->z == l.z && have.first->flags == l.flags) g = &have;
            if (!g) {
                groups.push_back({&l, {}});
                g = &groups.back();
            }
            bool listed = false;
            for (const unsigned f : g->from) listed = listed || f == l.from;
            if (!listed) g->from.push_back(l.from);
        }
        for (const Group& g : groups) {
            std::string name = "from ";
            for (std::size_t i = 0; i < g.from.size(); ++i) name += (i ? ", " : "") + place(g.from[i]);
            s.names.entrances.push_back({g.first->to, name, g.first->x / 65536.0, g.first->z / 65536.0, g.first->flags});
        }
    }

    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SAVEEDIT), owner, Proc, reinterpret_cast<LPARAM>(&s));
}

}  // namespace bof3x

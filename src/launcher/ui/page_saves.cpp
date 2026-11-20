// The save editor (launcher.rc IDD_SAVEEDIT; docs/save-editor.md, DIV-0211),
// from the Mods tab's Save Editor...: save_dialog.cpp's State, its file
// boxes, sections, fields, value box, Open, Save, Apply, Reload and Close, and
// SaveEditorRun's read of the install, less the windows. The texts are the
// Win32 window's, word for word. The format is launcher/save/'s, unchanged:
// every row is made from the field table, so the page holds no offset of its
// own.
//
// What a Win32 message box asked and waited for is a question here (ui::Ask,
// ui::AskNo for a box whose default button is No) asked in the frames after
// the action, with the rest of the action run on its answer; a picker answers
// in a later frame (ui::Picked). While either waits the page is grey, as the
// Win32 window is under its modal box.
//
// "A game is running" is asked of the operating-system seam (os/running.h): a
// process of the game's program, and on Windows a window of the class the
// game registers, "Bof3", which is what the Win32 editor asked FindWindowW
// (docs/known-defects.md D428).
//
// The Win32 window is 430 x 292 dialog units; the page area is 614 x 402 of
// the picture's pixels, about 350 x 214 units. It is laid out again in that
// area, every control and text kept: the two file boxes are 246 and 194 units
// wide (from 250), Reload, Set, Save and Close are 54 wide (from 56) at the
// right, the sections list is 86 wide (from 96), the record box and the field
// list 256 (from 314), the field list 105 high (from 162), the value box 170
// (from 222), and the rows below it are closer.
#include "launcher/ui/pages.h"

#include "common/cache_manifest.h"
#include "launcher/config.h"
#include "launcher/exe_image.h"
#include "launcher/save/block.h"
#include "launcher/save/card.h"
#include "launcher/save/finds.h"
#include "launcher/save/namemap.h"
#include "launcher/save/names.h"
#include "launcher/start.h"
#include "launcher/ui/ui.h"

#include "os/env.h"
#include "os/running.h"
#include "os/text.h"

#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bof3x::ui {

namespace {

constexpr bool kWindowsNames = std::filesystem::path::preferred_separator == L'\\';
const char kTitle[] = "Save Editor";
constexpr int kAskOpen = 31, kAskCard = 32;   // this page's questions to the pickers

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

// A Win32 message box that waits for its answer: asked in the next frames.
struct Question {
    std::string text;
    bool no_default = false;   // MB_DEFBUTTON2: Enter and Escape answer No
    std::function<void(bool)> then;
};

struct State {
    Pages* p = nullptr;
    std::wstring game_dir;
    save::Names names;
    save::PartyTable table;
    std::vector<save::NameSet> name_sets;   // each release's default names (save/namemap.h): the discs of the
                                            // languages installed, this install's layers, the image's
    std::string image_note;   // what the install's image does not hold, for the first status line

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
    std::vector<std::string> source_rows, target_rows;
    int source_sel = -1, target_sel = -1;

    bool advanced = false;
    std::vector<int> sections;          // the list's rows: indexes into save::Sections()
    std::vector<std::string> section_rows;
    int section_sel = -1;
    std::vector<std::string> record_rows;
    int record_sel = -1;
    bool record_on = false;
    std::vector<save::Field> fields;    // the field list's rows
    std::vector<std::vector<std::string>> field_rows;
    int field_sel = -1;
    std::string value, note;            // the value box's text and the note under it
    std::vector<std::string> choices;   // the value box's list
    bool value_on = false;              // the value box and Set can be used
    bool focus_value = false;           // a double-click on a field that is not a switch: the value box takes the keys
    std::string file_note, status;

    std::optional<Question> question;
    int picking = 0;                                 // the picker that is open, or 0
    std::function<void(const std::string&)> picked;   // what its answer does
    std::vector<std::string> said;                    // the messages and the questions, for a check's report
};
State g_s;

// A check's stand-ins for the game's marks (SavesGameMarks).
std::string g_program = kWindowsNames ? "bof3x.exe" : "bof3x";
std::string g_window_class = "Bof3";

std::wstring WideOf(const std::string& s) { return ConfigFromUtf8(s); }
std::string Utf8Of(const std::wstring& w) { return ConfigToUtf8(w); }

std::wstring SlotPath(const State& s, int slot) {
    wchar_t name[32];
    std::swprintf(name, 32, L"\\BISLPS%02X.DAT", slot);
    return s.game_dir + StartSep(name);
}

bool Exists(const std::wstring& path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

void Status(State& s, const std::string& text) { s.status = text; }

// The game's process, or on Windows its window: the game reads and writes the
// same files, so the editor does not write while it runs.
bool GameRuns() {
    if (!g_program.empty() && bof3::os::ProgramsRunning(g_program.c_str()) > 0) return true;
    return !g_window_class.empty() && bof3::os::WindowClassOpen(g_window_class.c_str());
}

bool Dirty(const State& s) { return s.loaded && s.block != s.original; }

bool IsCard(Kind k) { return k == Kind::kCardSave || k == Kind::kCardNew; }

std::wstring FileName(const std::wstring& path) {
    const std::size_t cut = path.find_last_of(L"\\/");
    return cut == std::wstring::npos ? path : path.substr(cut + 1);
}

// A text of a message or a question on one line, for a check's report: " | " for each line break.
std::string OneLine(const std::string& text) {
    std::string out;
    for (const char c : text) out += c == '\n' ? std::string(" | ") : std::string(1, c);
    return out;
}

// A message with an OK button (the Win32 MB_OK boxes), kept for a check's report.
void Tell(State& s, const std::string& text, bool error) {
    s.said.push_back("message: " + OneLine(text));
    Message(kTitle, text.c_str(), error);
}
void Warn(State& s, const std::string& why) { Tell(s, why, false); }

void AskThen(State& s, const std::string& text, bool no_default, std::function<void(bool)> then) {
    s.question = Question{text, no_default, std::move(then)};
}

// The release a card save's file name belongs to, in words: by the language
// installed from that disc, else by the two names measured, else the name.
std::string ReleaseWords(const State& s, const std::string& prefix) {
    std::string tag;
    for (const save::NameSet& set : s.name_sets)
        if (!set.prefix.empty() && set.prefix == prefix) tag = set.id;
    if (tag == "en-US" || (tag.empty() && prefix == save::kCardPrefixUs)) return "US release";
    if (tag == "ja-JP" || (tag.empty() && prefix == save::kCardPrefixJp)) return "Japan release";
    if (tag == "en-150") return "European release";
    if (tag == "fr-FR") return "French release";
    if (tag == "de-DE") return "German release";
    return prefix;
}

// The file name prefix of the card save in a directory frame.
std::string CardPrefixAt(const State& s, int slot) {
    for (const save::CardEntry& e : save::CardEntries(s.card))
        if (e.slot == slot) return e.prefix;
    return {};
}

// ---- the file boxes --------------------------------------------------------------

std::string SlotLabel(const State& s, int slot) {
    char head[32];
    std::snprintf(head, sizeof head, "Slot %X - ", slot);
    save::Block b;
    std::string why;
    if (!Exists(SlotPath(s, slot))) return std::string(head) + "empty";
    if (!save::ReadPcFile(SlotPath(s, slot), b, why)) return std::string(head) + "not a save file";
    return head + save::Describe(b, s.names) + (save::Checksum(b) == save::StoredChecksum(b) ? "" : " (damaged)");
}

std::string CardSaveLabel(const State& s, const save::CardEntry& e) {
    save::Block b;
    std::string why;
    std::string text = "Card save " + std::to_string(e.number + 1) + ", " + ReleaseWords(s, e.prefix) + " - ";
    if (!save::CardGet(s.card, e.slot, b, why)) return text + "not a save that the editor can open";
    const bool good = save::Checksum(b) == save::StoredChecksum(b);
    // For the line only: the names the game gave, as this install writes them, so that the line can be read.
    for (const save::NameSet& set : s.name_sets)
        if (set.id == "pc" && set.Known()) save::TranslateNames(b, s.name_sets, set);
    return text + save::Describe(b, s.names) + (good ? "" : " (damaged)");
}

int FindEntry(const std::vector<Entry>& rows, const Entry& e) {
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (SameEntry(rows[i], e)) return static_cast<int>(i);
    return -1;
}

// The row of the open save in a box, or -1: a card save is a row only while its card is the card shown.
int OpenedRow(const State& s, const std::vector<Entry>& rows) {
    if (!s.loaded) return -1;
    if (s.opened.kind == Kind::kCardSave && s.opened_path != s.card_path) return -1;
    return FindEntry(rows, RowOf(s.opened));
}

// Fills the two boxes. The first selects the open save; the second keeps `target` when it is still a row.
void FillSlots(State& s, const Entry* target = nullptr) {
    Entry keep;
    const int was = s.target_sel;
    const bool had = was >= 0 && was < static_cast<int>(s.targets.size());
    if (target) keep = *target;
    else if (had) keep = s.targets[static_cast<std::size_t>(was)];

    s.sources.clear();
    s.targets.clear();
    s.source_rows.clear();
    s.target_rows.clear();
    for (const bool source : {true, false}) {
        std::vector<Entry>& rows = source ? s.sources : s.targets;
        std::vector<std::string>& texts = source ? s.source_rows : s.target_rows;
        const auto add = [&](const Entry& e, const std::string& text) {
            rows.push_back(e);
            texts.push_back(text);
        };
        for (int i = 0; i < save::kSlots; ++i) add({Kind::kPcSlot, i, {}, -1}, SlotLabel(s, i));
        if (!s.card_path.empty()) {
            for (const save::CardEntry& e : save::CardEntries(s.card))
                if (e.bof3) add({Kind::kCardSave, e.slot, {}, -1}, CardSaveLabel(s, e));
            if (!source) {
                // A new save for each release the player has installed as a language (its disc gives the
                // file name and the names), and for the release of the open card save: it can keep its name.
                std::vector<std::string> prefixes;
                const auto offer = [&prefixes](const std::string& pre) {
                    for (const std::string& have : prefixes)
                        if (have == pre) return;
                    if (!pre.empty()) prefixes.push_back(pre);
                };
                for (const save::NameSet& set : s.name_sets) offer(set.prefix);
                if (s.loaded && s.opened.kind == Kind::kCardSave) offer(s.opened.prefix);
                for (const std::string& pre : prefixes) {
                    const int n = save::CardFreeNumber(s.card, pre);
                    if (n >= 0 && save::CardFreeSlot(s.card))
                        add({Kind::kCardNew, 0, pre, n},
                            "Card: a new save, " + ReleaseWords(s, pre) + " (save " + std::to_string(n + 1) + ")");
                }
            }
        }
        if (s.loaded && s.opened.kind == Kind::kPcFile)
            add({Kind::kPcFile, 0, {}, -1}, source ? "File: " + Utf8Of(s.opened_path) : std::string("The file that is open"));
        if (source) add({Kind::kPickFile, 0, {}, -1}, "A different file or memory card...");
        else add({Kind::kPickCard, 0, {}, -1}, s.card_path.empty() ? "A memory card file..." : "A different memory card file...");
    }
    s.source_sel = OpenedRow(s, s.sources);
    int row = target || had ? FindEntry(s.targets, keep) : -1;
    if (row < 0) row = OpenedRow(s, s.targets);
    s.target_sel = row;
}

// ---- the sections and the fields ---------------------------------------------------

void FillSections(State& s) {
    const int keep = s.section_sel;
    const int kept = keep >= 0 && keep < static_cast<int>(s.sections.size()) ? s.sections[static_cast<std::size_t>(keep)] : 0;
    s.sections.clear();
    s.section_rows.clear();
    int select = 0;
    const std::vector<save::Section>& all = save::Sections();
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        if (all[static_cast<std::size_t>(i)].advanced && !s.advanced) continue;
        if (i == kept) select = static_cast<int>(s.sections.size());
        s.sections.push_back(i);
        s.section_rows.push_back(all[static_cast<std::size_t>(i)].name);
    }
    s.section_sel = s.sections.empty() ? -1 : select;
}

int Section(const State& s) {
    const int row = s.section_sel;
    return row >= 0 && row < static_cast<int>(s.sections.size()) ? s.sections[static_cast<std::size_t>(row)] : -1;
}

// The record box: its entries for a section of several records of an open
// save, and the entry kept when it is still one (CB_SETCURSEL of an empty box
// selects nothing).
void FillRecords(State& s) {
    const int section = Section(s);
    const int records = section < 0 ? 1 : save::Sections()[static_cast<std::size_t>(section)].records;
    const int keep = s.record_sel;
    s.record_rows.clear();
    if (records > 1 && s.loaded)
        for (int i = 0; i < records; ++i) s.record_rows.push_back(save::RecordLabel(s.block, section, i, s.names));
    s.record_sel = s.record_rows.empty() ? -1 : keep >= 0 && keep < records ? keep : 0;
    s.record_on = records > 1 && s.loaded;
}

// One row's value, and the value the file has when the player changed it.
void ShowRow(State& s, int row) {
    const save::Field& f = s.fields[static_cast<std::size_t>(row)];
    const std::string now = save::FieldText(s.block, f, s.names), was = save::FieldText(s.original, f, s.names);
    std::vector<std::string>& cells = s.field_rows[static_cast<std::size_t>(row)];
    cells[1] = now;
    cells[2] = now == was ? std::string() : was;
}

// The value box for the row selected: its text, its list, its note.
void ShowValue(State& s) {
    const int row = s.field_sel;
    s.choices.clear();
    std::string note;
    bool can = false;
    if (s.loaded && row >= 0 && row < static_cast<int>(s.fields.size())) {
        const save::Field& f = s.fields[static_cast<std::size_t>(row)];
        s.choices = save::FieldChoices(s.block, f, s.names);
        s.value = save::FieldText(s.block, f, s.names);
        can = !save::FieldReadOnly(f);
        note = f.note;
        if (!can) note = (note.empty() ? "" : note + " ") + "You cannot change this field.";
    } else {
        s.value.clear();
    }
    s.note = note;
    s.value_on = can;
}

void FillFields(State& s) {
    s.fields.clear();
    s.field_rows.clear();
    s.field_sel = -1;   // the list made again: no row selected
    const int section = Section(s);
    if (s.loaded && section >= 0) {
        const int record = s.record_sel;
        for (save::Field& f : save::SectionFields(section, record < 0 ? 0 : record, s.names))
            if (s.advanced || f.cls == 'E') s.fields.push_back(std::move(f));
        for (int i = 0; i < static_cast<int>(s.fields.size()); ++i) {
            s.field_rows.push_back({s.fields[static_cast<std::size_t>(i)].label, std::string(), std::string()});
            ShowRow(s, i);
        }
    }
    ShowValue(s);
}

void ShowFileNote(State& s) {
    std::string note;
    if (!s.loaded) note = "Select a save file.";
    else if (Dirty(s)) note = "You changed this save. The file does not change until you click Save.";
    else note = "The file and the editor are the same.";
    if (!s.card_path.empty())
        note += " Memory card: " + Utf8Of(FileName(s.card_path)) + (s.card_new ? " (a new file)" : "");
    s.file_note = note;
}

void Refresh(State& s) {
    FillRecords(s);
    FillFields(s);
    ShowFileNote(s);
}

// ---- open and save ----------------------------------------------------------------

// The Win32 DiscardOk: `yes` when there is no change to lose or the player
// discards it, else `no`.
void DiscardThen(State& s, std::function<void()> yes, std::function<void()> no) {
    if (!Dirty(s)) {
        yes();
        return;
    }
    AskThen(s, "You changed this save and did not save it.\nDiscard the changes?", true,
            [yes = std::move(yes), no = std::move(no)](bool discard) {
                if (discard) yes();
                else if (no) no();
            });
}

// The Win32 pickers' filters.
const std::vector<PickFilter> kCardFilter = {
    {"Memory card files (*.mcr;*.mcd;*.srm;*.mc;*.mcs;*.mem;*.ps;*.bin)", "mcr;mcd;srm;mc;mcs;mem;ps;bin"},
    {"All files", "*"},
};
const std::vector<PickFilter> kAnyFilter = {
    {"Save files and memory card files", "DAT;mcr;mcd;srm;mc;mcs;mem;ps;bin"},
    {"All files", "*"},
};

// A file name from the player, answered in a later frame. `may_be_new` lets
// the player write the name of a new file.
void PickThen(State& s, int ask, const std::vector<PickFilter>& filter, bool may_be_new,
              std::function<void(const std::string&)> then) {
    s.picking = ask;
    s.picked = std::move(then);
    PickFileIn(ask, filter, Utf8Of(s.game_dir).c_str(), may_be_new,
               may_be_new ? "Select a memory card file, or type the name of a new one" : nullptr);
}

bool FileIsCard(const std::wstring& path) {
    std::error_code ec;
    const std::filesystem::path p(path);
    return std::filesystem::is_regular_file(p, ec) && std::filesystem::file_size(p, ec) == save::kCardSize && !ec;
}

// Makes `path` the memory card that the boxes show. A file that is not there
// yet is a new, empty card when `may_be_new`. False, with a message, when the
// file is not a card.
bool AttachCard(State& s, const std::wstring& path, bool may_be_new) {
    save::Card card;
    bool fresh = false;
    if (may_be_new && !Exists(path)) {
        card = save::EmptyCard();
        fresh = true;
    } else {
        std::string why;
        if (!save::ReadCardFile(path, card, why)) {
            Warn(s, why);
            return false;
        }
    }
    s.card = std::move(card);
    s.card_path = path;
    s.card_new = fresh;
    return true;
}

// Opens a row of the file box; `done` gets true when it is open. False, with
// the box put back, when the save cannot be opened or the player keeps what is
// open.
void Open(State& s, const Entry& entry, const std::wstring& path, std::function<void(bool)> done = {}) {
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
        if (!ok && entry.kind == Kind::kPcSlot && !Exists(path)) why = "This slot is empty.";
    }
    const std::wstring card_path = s.card_path;
    auto finish = [&s, entry, path, opened, b, header = std::move(header), card_path, done](bool good) mutable {
        if (!good) {
            s.source_sel = OpenedRow(s, s.sources);
            if (done) done(false);
            return;
        }
        s.loaded = true;
        s.opened = opened;
        s.opened_path = entry.kind == Kind::kCardSave ? card_path : path;
        s.opened_header = std::move(header);
        s.original = s.block = b;
        const Entry same{opened.kind, opened.slot, {}, -1};
        FillSlots(s, &same);
        Status(s, "");
        Refresh(s);
        if (done) done(true);
    };
    if (ok && save::Checksum(b) != save::StoredChecksum(b)) {
        AskThen(s,
                "The checksum of this save is incorrect. The game shows this save as damaged.\n\n"
                "Open it? When you save, the editor writes a correct checksum.",
                true, std::move(finish));
        return;
    }
    if (!ok) Warn(s, why);
    finish(ok);
}

void PickSlot(State& s, int row) {
    if (row < 0 || row >= static_cast<int>(s.sources.size())) return;
    const Entry entry = s.sources[static_cast<std::size_t>(row)];
    if (row == OpenedRow(s, s.sources)) return;
    DiscardThen(
        s,
        [&s, entry] {
            if (entry.kind == Kind::kPickFile) {
                PickThen(s, kAskOpen, kAnyFilter, false, [&s](const std::string& chosen) {
                    if (chosen.empty()) {
                        s.source_sel = OpenedRow(s, s.sources);
                        return;
                    }
                    const std::wstring path = WideOf(chosen);
                    if (!FileIsCard(path)) {
                        Open(s, {Kind::kPcFile, 0, {}, -1}, path);
                        return;
                    }
                    if (!AttachCard(s, path, false)) {
                        s.source_sel = OpenedRow(s, s.sources);
                        return;
                    }
                    // The card's first save of this game, when it has one.
                    for (const save::CardEntry& e : save::CardEntries(s.card))
                        if (e.bof3) {
                            Open(s, {Kind::kCardSave, e.slot, {}, -1}, path, [&s](bool good) {
                                if (!good) FillSlots(s);
                                ShowFileNote(s);
                            });
                            return;
                        }
                    FillSlots(s);
                    ShowFileNote(s);
                    Status(s, "This memory card has no save of this game. To write a save to the card, select the card "
                              "in \"Write to\".");
                });
                return;
            }
            if (entry.kind == Kind::kCardSave && !s.card_new) {
                // The card file again, as it is now.
                const int slot = entry.slot;
                if (!AttachCard(s, s.card_path, false)) return;
                Open(s, {Kind::kCardSave, slot, {}, -1}, s.card_path);
                return;
            }
            Open(s, entry, entry.kind == Kind::kPcSlot ? SlotPath(s, entry.slot) : s.opened_path);
        },
        [&s] { s.source_sel = OpenedRow(s, s.sources); });
}

// The player picked "A memory card file..." in the box to write to.
void PickTargetCard(State& s) {
    PickThen(s, kAskCard, kCardFilter, true, [&s](const std::string& chosen) {
        if (!chosen.empty()) {
            std::wstring path = WideOf(chosen);
            const std::size_t leaf = path.find_last_of(L"\\/");
            if (path.find(L'.', leaf == std::wstring::npos ? 0 : leaf) == std::wstring::npos) path += L".mcr";
            if (AttachCard(s, path, true)) {
                FillSlots(s);
                // A new save for the release of the open save, else the first row of the card.
                int row = -1;
                for (std::size_t i = 0; i < s.targets.size() && row < 0; ++i)
                    if (s.targets[i].kind == Kind::kCardNew &&
                        (s.opened.kind != Kind::kCardSave || s.targets[i].prefix == s.opened.prefix))
                        row = static_cast<int>(i);
                for (std::size_t i = 0; i < s.targets.size() && row < 0; ++i)
                    if (IsCard(s.targets[i].kind)) row = static_cast<int>(i);
                if (row < 0) {
                    bool any = false;
                    for (const save::NameSet& set : s.name_sets) any = any || !set.prefix.empty();
                    Status(s, any ? "This memory card has no room for a save of this game."
                                  : "The editor makes a new card save for a PlayStation release that you installed as a "
                                    "language. Install one, or build the language again, then open the editor.");
                    row = OpenedRow(s, s.targets);
                } else {
                    Status(s, s.card_new ? "The editor makes this memory card file when you click Save." : "");
                }
                s.target_sel = row;
                ShowFileNote(s);
                return;
            }
        }
        s.target_sel = OpenedRow(s, s.targets);
    });
}

// What Save carries from one question to the next.
struct SaveJob {
    Entry target;
    bool to_card = false, from_card = false;
    std::wstring path;
    save::Block out{};
};

// The write, after the questions.
void SaveWrite(State& s, const SaveJob& job) {
    std::wstring backup;
    std::string why;
    Entry now = job.target;
    std::vector<std::uint8_t> header = s.opened_header;
    const std::wstring backups = s.game_dir + StartSep(L"\\save-backups");
    if (job.to_card) {
        save::Card card = s.card;
        int slot = job.target.slot;
        if (job.target.kind == Kind::kCardSave) {
            if (!save::CardPut(card, slot, job.out, why)) slot = 0;
        } else {
            // The icon: the open save's when it came from a card, else that of a save of this game on the card.
            const std::uint8_t* icon = job.from_card && s.opened_header.size() == 0x200 ? s.opened_header.data() : nullptr;
            for (const save::CardEntry& e : save::CardEntries(card))
                if (!icon && e.bof3) icon = save::CardHeader(s.card, e.slot);
            slot = save::CardAdd(card, job.target.prefix, job.target.number, job.out, icon, why);
        }
        if (!slot || !save::WriteCardFile(job.path, card, backups, backup, why)) {
            Tell(s, why, true);
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
        if (!save::WritePcFile(job.path, job.out, backups, backup, why)) {
            Tell(s, why, true);
            return;
        }
        header.clear();
    }
    s.original = s.block = job.out;
    s.opened = now;
    s.opened_path = job.path;
    s.opened_header = std::move(header);
    const Entry same = RowOf(now);
    FillSlots(s, &same);
    Refresh(s);
    Status(s, backup.empty() ? "Saved." : "Saved. A copy of the file from before is in " + Utf8Of(backup));
}

// The question before a write that replaces a save, then the write.
void SaveReplace(State& s, const std::shared_ptr<SaveJob>& job) {
    const bool same_place = s.loaded && SameEntry(RowOf(s.opened), job->target) &&
                            (job->target.kind == Kind::kPcSlot || s.opened_path == job->path);
    const bool replaces = job->target.kind == Kind::kCardSave || (!job->to_card && Exists(job->path));
    if (!same_place && replaces) {
        AskThen(s,
                job->to_card ? "This place on the memory card has a save. Replace it?\nThe editor keeps a copy of the "
                               "memory card file in the save-backups folder."
                             : "This slot has a save. Replace it?\nThe editor keeps a copy of the save in the "
                               "save-backups folder.",
                true, [&s, job](bool yes) {
                    if (yes) SaveWrite(s, *job);
                });
        return;
    }
    SaveWrite(s, *job);
}

void Save(State& s) {
    if (!s.loaded) return;
    const int row = s.target_sel;
    if (row < 0 || row >= static_cast<int>(s.targets.size())) {
        Status(s, "Select where to write the save in \"Write to\".");
        return;
    }
    auto job = std::make_shared<SaveJob>();
    job->target = s.targets[static_cast<std::size_t>(row)];
    const Entry& target = job->target;
    if (target.kind == Kind::kPickCard) {
        PickTargetCard(s);
        return;
    }
    job->to_card = IsCard(target.kind);
    job->from_card = s.opened.kind == Kind::kCardSave;
    const bool to_card = job->to_card, from_card = job->from_card;
    if (!to_card && GameRuns()) {
        Warn(s, "The game is running. Close the game, then save.");
        return;
    }
    job->path = target.kind == Kind::kPcSlot ? SlotPath(s, target.slot)
                : target.kind == Kind::kPcFile ? s.opened_path : s.card_path;

    save::Block& out = job->out;
    out = s.block;
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
    std::string errors, warnings;
    for (const save::Issue& i : save::Check(s.original, out, s.table)) (i.error ? errors : warnings) += "- " + i.text + "\n";
    std::string why;
    if (to_card) {
        std::vector<std::uint8_t> psx(save::kBlockSize);
        if (!save::PcToPsx(out, psx.data(), why)) errors += "- " + why + "\n";
    }
    if (!errors.empty()) {
        Warn(s, "The editor cannot save these values:\n\n" + errors);
        return;
    }

    // A save that goes from one game to the other: say what changes, then ask.
    std::string text;
    if (to_card != from_card) {
        text = to_card ? "This save goes from the PC game to a PlayStation memory card.\n\n"
                       : "This save goes from a PlayStation memory card to the PC game.\n\n";
        if (names_to) {
            text += "- The game gave " + std::to_string(renamed.chars) + " character names and " +
                    std::to_string(renamed.faeries) + " faerie names of this save. The editor writes them as the names of " +
                    std::string(to_card ? "that release" : "this install") + ".\n";
            if (!renamed.kept.empty()) {
                text += "- These names are not names that the game gave. The editor writes them as they are, and they "
                        "can show incorrectly in the other game:";
                for (std::size_t i = 0; i < renamed.kept.size() && i < 12; ++i) text += (i ? ", " : " ") + renamed.kept[i];
                if (renamed.kept.size() > 12) text += ", and " + std::to_string(renamed.kept.size() - 12) + " more";
                text += ". You can change them before you save.\n";
            }
        } else {
            text += "- The editor writes the names of the characters and of the faeries as they are, because the release "
                    "of this card save is not installed as a language, or its language was built by an older version. "
                    "Each release of the game has its own letters, thus the names can show incorrectly in the other "
                    "game. To change the names automatically, install that release as a language or build it again. "
                    "You can also change the names in Characters before you save.\n";
        }
        text += "- The button setup goes with the save.\n";
        if (to_card && (out[0x1040] || out[0x1041] || out[0x7C] || out[0x7D]))
            text += "- The editor clears the values that only the PC game has (Mountain passes open, Solo champion, two "
                    "options).\n";
        if (to_card)
            text += "\nNobody has loaded a save from the PC game in a PlayStation game or an emulator yet. Keep a "
                    "copy of your memory card.\n";
        text += "\nContinue?";
    }
    // The conversion's question (Yes by default), then the replace's.
    const auto convert = [&s, job, text]() {
        if (job->to_card == job->from_card) {
            SaveReplace(s, job);
            return;
        }
        AskThen(s, text, false, [&s, job](bool yes) {
            if (!yes) return;
            if (job->to_card) save::ClearPcOnly(job->out);
            SaveReplace(s, job);
        });
    };
    if (!warnings.empty()) {
        AskThen(s, "These values are unusual:\n\n" + warnings + "\nSave them?", true, [convert](bool yes) {
            if (yes) convert();
        });
        return;
    }
    convert();
}

// The value box's text into the row selected.
void Apply(State& s, const std::string& text) {
    const int row = s.field_sel;
    if (!s.loaded || row < 0 || row >= static_cast<int>(s.fields.size())) return;
    std::string why;
    if (!save::FieldSet(s.block, s.fields[static_cast<std::size_t>(row)], text, s.names, why)) {
        Status(s, why);
        return;
    }
    Status(s, "");
    ShowRow(s, row);
    ShowValue(s);
    ShowFileNote(s);
}

void Reload(State& s) {
    if (!s.loaded) return;
    DiscardThen(
        s,
        [&s] {
            s.block = s.original;   // DiscardThen asked; Open reads the file again
            const Entry row = RowOf(s.opened);
            if (row.kind == Kind::kCardSave) {
                // The card file again; a card that is not a file yet has nothing newer.
                if (!s.card_new || s.opened_path != s.card_path)
                    if (!AttachCard(s, s.opened_path, false)) return;
                Open(s, row, s.opened_path);
            } else {
                Open(s, row, row.kind == Kind::kPcSlot ? SlotPath(s, row.slot) : s.opened_path);
            }
        },
        {});
}

void CloseEditor(State& s) {
    DiscardThen(s, [&s] { s.p->saves_open = false; }, {});
}

// ---- the install's names and tables (SaveEditorRun) ----------------------------------

void ReadInstall(State& s, const std::wstring& cache, const std::string& language) {
    // The names: the language layer's FIRST.DAT, in the game's DAT\ or in the cache.
    const NameTableTags t = ExeNameTableTags();
    const save::NameTags tags{t.items, t.key_items, t.weapons, t.armour, t.accessories, t.abilities};
    const bool latin = save::LanguageIsLatin(language);
    const std::wstring tag = ConfigFromUtf8(language);
    s.names = save::LoadNames(s.game_dir + StartSep(L"\\DAT\\") + tag + L".FIRST.DAT", tags, latin);
    if (s.names.item[0].empty() && !cache.empty())
        s.names = save::LoadNames(cache + StartSep(L"\\loc\\") + tag + StartSep(L"\\dat\\FIRST.DAT"), tags, latin);

    // The reference names, beside the launcher (data/save-names.tsv; as the Jukebox's music-names.tsv).
    std::string self;
    std::wstring beside;
    if (bof3::os::ExecutablePath(&self)) {
        beside = bof3::os::ToWide(self.c_str());
        const std::size_t cut = beside.find_last_of(L"\\/");
        beside.erase(cut == std::wstring::npos ? 0 : cut);
    }
    save::LoadReferenceNames(beside + StartSep(L"\\save-names.tsv"), s.names);
    // A kind of fish is an item of the install (item 0x38 + kind, which the
    // fishing panel's row reads: FieldPanel_DrawKindRow), so where the install
    // gives item names the fish take them, in the install's language.
    for (unsigned k = 0; k < 22; ++k)
        if (0x38 + k < s.names.item[0].size() && !s.names.item[0][0x38 + k].empty())
            s.names.fish[k] = s.names.item[0][0x38 + k];

    // The game's tables, from the install's own image of .data (docs/exe-retire-round.md X4): the cache's
    // base\exe with its table files laid in, as the engine maps it, on every install kind. No BOF3.exe is
    // opened, but under the developer's switch BOF3X_EXEFILE=1, which reads the game folder's file first, as
    // the engine then does. Where the image does not hold a table the editor offers less and says so.
    ExeImage image;
    std::string why;
    bool have_image = InstallExeFileSwitch() && ExeImageFromFile(s.game_dir + StartSep(L"\\BOF3.exe"), image, why);
    if (!have_image) have_image = ExeImageFromCache(cache, image, why);
    const auto note = [&s](const char* text) { s.image_note += (s.image_note.empty() ? "" : " ") + std::string(text); };
    if (!have_image)
        note("This install has no game data that the editor can read. Set up the install on the Sources page. "
             "Until then, you can change the order of the party, but not its members, and the editor shows no "
             "entrances, chests or passages.");
    if (have_image && !ExePartyTable(image, s.table, why)) {
        s.table.clear();
        note("The game data of this install does not have the party table. You can change the order of the party, "
             "but not its members.");
    }

    // Each release's default names (save/namemap.h), all from what the player installed:
    //  - the image's own, from its tables (ExeDefaultNameTables): the names of the seven default records New
    //    Game copies and the whelp's, and of the sixty faerie trait records. A disc-built image does not hold
    //    them (a gap and a language layer's names there, recipes/exe-gaps.tsv): then there is no such set;
    //  - for each language installed (the Language box's list): the disc's own names and card file name, and
    //    the names as the PC plays that language, both from the layer's FIRST.DAT.
    {
        save::NameSet original;
        DefaultNameTables tables;
        if (have_image && ExeDefaultNameTables(image, tables, why))
            original = save::NameSetFromTables("pc-original", tables.chars.data(), tables.faeries.data());
        else if (have_image)
            note("The game data of this install does not have the default names of the original release, so the "
                 "editor does not know them.");
        bool played = false;
        for (const std::string& code : ConfigLanguagesAvailable(s.game_dir, cache)) {
            const std::wstring w = ConfigFromUtf8(code);
            const std::wstring files[2] = {
                s.game_dir + StartSep(L"\\DAT\\") + w + L".FIRST.DAT",
                cache.empty() ? std::wstring() : cache + StartSep(L"\\loc\\") + w + StartSep(L"\\dat\\FIRST.DAT")};
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
    // A read the image refuses (a range it does not hold) leaves that link or find out: the lists are then
    // not complete, and the first status line says so.
    const std::uint32_t refused_before = image.not_held;
    std::vector<AreaLink> links;
    if (have_image && ExeAreaLinks(image, links, why)) {
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

    // The records-and-finds bits (save/finds.h): the chests and the passages from the
    // install's exe, with the item names above; the enemy kinds from each area file's
    // enemy table, read as the game reads it - the cache's base and loc\zh-CN, or the
    // install's DAT\, then the language layer's names - and only for a Latin layer,
    // as the other names.
    {
        std::vector<save::Find> chests, passages;
        if (have_image && ExeAreaFinds(image, chests, passages, why)) {
            s.names.chest = save::FindTexts(chests, s.names);
            s.names.passage = save::FindTexts(passages, s.names);
        }
        if (have_image && image.not_held != refused_before)
            note("The game data of this install does not have all of the area data. The lists of entrances, chests "
                 "and passages are not complete.");
        std::error_code ec;
        const bool layer_cached = !cache.empty() && std::filesystem::is_directory(cache + StartSep(L"\\loc\\") + tag, ec);
        for (unsigned area = 0; latin && area < 200; ++area) {
            wchar_t name[16];
            std::swprintf(name, 16, L"AREA%03u.DAT", area);
            std::vector<save::EnemyChunk> chunks;
            const std::wstring base = cache.empty() ? std::wstring() : cache + StartSep(L"\\base\\dat\\") + name;
            if (!base.empty() && std::filesystem::is_regular_file(base, ec)) {
                save::ReadEnemyChunks(base, chunks);
                save::ReadEnemyChunks(cache + StartSep(L"\\loc\\zh-CN\\dat\\") + name, chunks);
            } else {
                save::ReadEnemyChunks(s.game_dir + StartSep(L"\\DAT\\") + name, chunks);
            }
            if (layer_cached) save::ReadEnemyChunks(cache + StartSep(L"\\loc\\") + tag + StartSep(L"\\dat\\") + name, chunks);
            else save::ReadEnemyChunks(s.game_dir + StartSep(L"\\DAT\\") + tag + L"." + name, chunks);
            save::AddEnemyNames(chunks, s.names.enemy);
        }
    }
}

}  // namespace

void SavesBegin(Pages& p) {
    State& s = g_s;
    s = State{};
    s.p = &p;
    s.game_dir = p.game_dir;
    while (!s.game_dir.empty() && (s.game_dir.back() == L'\\' || s.game_dir.back() == L'/')) s.game_dir.pop_back();
    // The names come from the layer of the language that the game plays now (config_dialog.cpp IDC_SAVEEDITOR).
    ReadInstall(s, ConfigCacheDataRoot(*p.cfg), ConfigLanguagePlayed(p.game_dir, *p.cfg));
    p.saves_open = true;

    // WM_INITDIALOG.
    FillSlots(s);
    FillSections(s);
    Refresh(s);
    // The window opens on the first slot that holds a good save, so it never
    // starts empty on an install that has saves.
    for (int i = 0; i < save::kSlots; ++i) {
        save::Block b;
        std::string why;
        if (save::ReadPcFile(SlotPath(s, i), b, why) && save::Checksum(b) == save::StoredChecksum(b)) {
            Open(s, {Kind::kPcSlot, i, {}, -1}, SlotPath(s, i));
            break;
        }
    }
    std::string intro = s.names.item[0].empty() ? "This install has no names that the editor can show. The lists show numbers."
                                                : "";
    if (!s.image_note.empty()) intro += (intro.empty() ? "" : " ") + s.image_note;
    Status(s, intro);
}

void SavesPage(Pages&) {
    State& s = g_s;

    // A question the last frames' action asked, and its answer.
    if (s.question) {
        const std::string text = s.question->text;
        const int answer = s.question->no_default ? AskNo(kTitle, text.c_str()) : Ask(kTitle, text.c_str());
        if (answer >= 0) {
            Question q = std::move(*s.question);
            s.question.reset();
            s.said.push_back("asked: " + OneLine(q.text));
            s.said.push_back(std::string("answered: ") + (answer == 1 ? "yes" : "no"));
            q.then(answer == 1);
        }
    }
    // A picker's answer.
    if (s.picking) {
        std::string chosen;
        if (Picked(s.picking, &chosen)) {
            s.picking = 0;
            auto then = std::move(s.picked);
            s.picked = nullptr;
            if (then) then(chosen);
        }
    }
    const bool modal = s.question.has_value() || s.picking != 0;
    const char* modal_why = s.question ? "Answer the question first." : "Close the file window first.";
    // The next widget is grey while a question or a picker waits, else when `on` is false, with `why`.
    const auto usable = [&](bool on, const char* why) {
        if (modal) Grey(modal_why);
        else if (!on) Grey(why);
    };

    Place(0, Dy(2));
    Label("Save file:");
    Place(Dx(42), 0);
    Size(Dx(246));
    usable(true, "");
    int source = s.source_sel;
    if (ComboWide("##file", &source, s.source_rows, Dx(416)) && !modal) {
        s.source_sel = source;
        PickSlot(s, source);
    }
    Place(Dx(292), 0);
    Size(Dx(54), Dy(12));
    usable(s.loaded, "Select a save file.");
    if (Button("Reload") && !modal) Reload(s);
    Place(0, Dy(15));
    Size(Dx(346));
    LabelCut(s.file_note.c_str(), false);

    Place(0, Dy(26));
    Size(Dx(86), Dy(120));
    usable(true, "");
    int section = s.section_sel;
    if (List("##sections", &section, s.section_rows) && !modal) {
        s.section_sel = section;
        s.record_sel = -1;
        FillRecords(s);
        FillFields(s);
    }
    Place(Dx(90), Dy(26));
    Size(Dx(256));
    usable(s.record_on, s.loaded ? "This section has one record." : "Select a save file.");
    int record = s.record_sel;
    if (Combo("##record", &record, s.record_rows) && !modal) {
        s.record_sel = record;
        FillFields(s);
    }
    Place(Dx(90), Dy(41));
    Size(Dx(256), Dy(105));
    usable(true, "");
    int field = s.field_sel, used = -1;
    if (ColumnList("##fields", &field, {"Field", "Value", "In the file"}, {32, 43, 25}, s.field_rows, &used) && !modal) {
        s.field_sel = field;
        ShowValue(s);
    }
    if (used >= 0 && !modal && used < static_cast<int>(s.fields.size())) {
        // A switch changes on a double-click; any other field takes the keys.
        s.field_sel = used;
        const save::Field& f = s.fields[static_cast<std::size_t>(used)];
        if (f.type == save::Type::kBit && !save::FieldReadOnly(f))
            Apply(s, save::FieldText(s.block, f, s.names) == "On" ? "Off" : "On");
        else
            s.focus_value = true;
    }

    Place(0, Dy(151));
    usable(true, "");
    bool advanced = s.advanced;
    if (Check("Show advanced fields##advanced", &advanced) && !modal) {
        s.advanced = advanced;
        FillSections(s);
        Refresh(s);
    }
    Place(Dx(90), Dy(152));
    Label("Value:");
    Place(Dx(118), Dy(150));
    Size(Dx(170));
    const bool has_field = s.loaded && s.field_sel >= 0;
    usable(s.value_on, has_field ? "You cannot change this field." : "Select a field.");
    int picked = -1;
    std::string value = s.value;
    const bool enter = EditCombo("##value", &value, s.choices, &picked, s.focus_value);
    s.focus_value = false;
    if (!modal) s.value = value;
    if (!modal && picked >= 0) Apply(s, value);   // an entry picked from the list is set at once
    Place(Dx(292), Dy(149));
    Size(Dx(54), Dy(14));
    usable(s.value_on, has_field ? "You cannot change this field." : "Select a field.");
    if ((Button("Set") || (enter && s.value_on)) && !modal) Apply(s, s.value);   // Set, and Enter in the value box
    Place(Dx(90), Dy(166));
    Size(Dx(256));
    Label(s.note.c_str());

    Place(0, Dy(184));
    Size(Dx(346));
    LabelCut(s.status.c_str(), false);
    Place(0, Dy(198));
    Label("Write to:");
    Place(Dx(36), Dy(196));
    Size(Dx(194));
    usable(true, "");
    int target = s.target_sel;
    if (ComboWide("##target", &target, s.target_rows, Dx(416)) && !modal) {
        s.target_sel = target;
        if (target >= 0 && target < static_cast<int>(s.targets.size()) &&
            s.targets[static_cast<std::size_t>(target)].kind == Kind::kPickCard)
            PickTargetCard(s);
    }
    Place(Dx(234), Dy(195));
    Size(Dx(54), Dy(14));
    usable(s.loaded, "Select a save file.");
    if (Button("Save") && !modal) Save(s);
    Place(Dx(292), Dy(195));
    Size(Dx(54), Dy(14));
    usable(true, "");
    const bool close = Button("Close");
    if ((close || Pressed(Press::kBack)) && !modal) CloseEditor(s);
}

std::string SavesText() {
    const State& s = g_s;
    std::string out = "file note: " + s.file_note + "\nstatus: " + s.status + "\n";
    const auto mark = [](bool on) { return on ? " [selected]" : ""; };
    for (std::size_t i = 0; i < s.source_rows.size(); ++i)
        out += "file " + std::to_string(i) + ": " + s.source_rows[i] + mark(static_cast<int>(i) == s.source_sel) + "\n";
    for (std::size_t i = 0; i < s.target_rows.size(); ++i)
        out += "write to " + std::to_string(i) + ": " + s.target_rows[i] + mark(static_cast<int>(i) == s.target_sel) + "\n";
    for (std::size_t i = 0; i < s.section_rows.size(); ++i)
        out += "section " + std::to_string(i) + ": " + s.section_rows[i] + mark(static_cast<int>(i) == s.section_sel) + "\n";
    out += std::string("record box: ") + (s.record_on ? "on" : "grey") + "\n";
    for (std::size_t i = 0; i < s.record_rows.size(); ++i)
        out += "record " + std::to_string(i) + ": " + s.record_rows[i] + mark(static_cast<int>(i) == s.record_sel) + "\n";
    for (std::size_t i = 0; i < s.field_rows.size(); ++i) {
        const std::vector<std::string>& r = s.field_rows[i];
        out += "field " + std::to_string(i) + ": " + r[0] + "\t" + r[1] + "\t" + r[2] +
               mark(static_cast<int>(i) == s.field_sel) + "\n";
    }
    out += "value: " + s.value + (s.value_on ? "" : " (grey)") + "\n";
    out += "choices: " + std::to_string(s.choices.size()) + "\n";
    out += "note: " + s.note + "\n";
    out += std::string("advanced: ") + (s.advanced ? "on" : "off") + "\n";
    out += std::string("dirty: ") + (Dirty(s) ? "yes" : "no") + "\n";
    out += std::string("game running: ") + (GameRuns() ? "yes" : "no") + "\n";
    for (const std::string& line : s.said) out += line + "\n";
    return out;
}

void SavesGameMarks(const std::string* program, const std::string* window_class) {
    if (program) g_program = *program;
    if (window_class) g_window_class = *window_class;
}

}  // namespace bof3x::ui

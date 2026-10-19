// DIVERGENCE (the quicksave round's draft D, docs/quicksave.md, section QM): F5's
// quick menu - the sixteen quicksaves BISLPS10.DAT..BISLPS1F.DAT listed
// newest first (slot 0 is 0x10, the one F7 loads), three shown at a time,
// drawn as the title's load menu draws its screens: its backdrop and title
// box with its system messages, the save slots, and on a slot that holds a
// quicksave its "load this game?" Yes / No with the pointing hand. Yes asks
// the snapshot core for a quick-load of that slot; No goes back to the slots;
// an empty slot takes no confirm (as the load menu). Load only: F6 is the one
// way to make a quicksave (the owner, 2026-10-17; docs/quicksave-round.md 5).
// A load the core refuses (another build, no snapshot, ...) leaves the game
// untouched and the menu open: the load menu's own "could not load" message in
// the title box, and the core's reason on a line of the 8 x 8 font below the
// slots - the overlay line the core writes reaches only the log while the
// back buffer is ours (win_main.cpp, Overlay).
//
// It runs while Game_Paused is set, in WinMain's paused branch where the
// pause lines are drawn: no task runs, so the menu reads the pad itself each
// frame - through Pad_Read, as the game's latch would, or the playing
// recipe's word (InputScript_PausedWord) - and draws with the load menu's
// draws, not its state: save_menu::DrawLoadWindows (Menu_DrawBackdrop of the
// Config's background byte, Menu_DrawTitleBox, Text_DrawAt of a system
// message: LoadWindows / LoadMessage), SaveMenu_DrawSlots' body over the quick
// list (save_menu::DrawSlotsOver) with the menu's own first-shown and cursor
// words, and Menu_YesNo's draw half (its line, system message 0xF, and
// Menu_DrawHand at its stops, DIV-0027's under a Latin overlay) with the
// menu's own Yes / No - not Menu_YesNo itself, which reads Input_Pressed,
// keeps its answer in 0x929F0B and plays sounds. The text is the game's, so a
// language overlay translates it. Confirm and cancel are the game's words 7
// and 8 (Field_ConfirmButtons, Field_CancelButtons), left / right flip the
// answer (0xA000, as Menu_YesNo): they follow sigma (DIV-0133) and the
// release's buttons as every menu of the game does.
//
// What it owns is ours and outside the snapshot span: the two tables (the
// listing, read fresh at every F5 with a reader of our own - not Save_ReadFile,
// which reads through Save_Staging and File_Slots, both inside the span), the
// cursor, the first shown and the answer. The game's load-menu state
// (0x9036D4, 0x8034D0, 0x6BDF86, 0x929F0B, the summaries at 0x905BC0,
// Save_Staging) is never written. The draws write what any paused frame's
// draw writes: the packet pool, the ordering table, the text scratch 0x904BA0.
//
// A confirm or cancel that ends the menu - Yes, or cancel on the slots - is
// acted on at its release, so the button is up when the game's latch runs
// again: a press kept down would be an edge to the game's next latch (after a
// load, against the Input_Held the quicksave holds).
#include "game/quick_menu.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/save_menu.h"
#include "game/win_main.h"
#include "game/yes_no_layout.h"
#include "hook/host.h"
#include "hook/detour.h"
#include "hook/input_script.h"
#include "hook/log.h"
#include "hook/snapshot.h"

namespace {

using U = std::uint32_t;

// The pad's bits (Pad_Read's layout, input/bindings.h).
constexpr std::uint16_t kUp = 0x1000, kDown = 0x4000, kLeftRight = 0xA000;
constexpr unsigned kFirstSlot = 0x10;       // BISLPS10.DAT, the newest
constexpr const char* kQuickPattern = "BISLPS1?.DAT";
constexpr unsigned kRepeatDelay = 20, kRepeatEvery = 4;   // frames: a held direction's repeat
// The load menu's system messages (save_menu.cpp: LoadMenu_Choose, _Confirm, _Error).
constexpr unsigned kMsgChoose = 0xAC;       // the slots (LoadMenu_Open / LoadMenu_Choose)
constexpr unsigned kMsgConfirm = 0xB9;      // load this game? (LoadMenu_Confirm)
constexpr unsigned kMsgCouldNot = 0xB8;     // 0xA6 + 0x12: LoadMenu_Error's error 2, the read or the sum failed
constexpr unsigned kMsgYesNo = 0xF;         // Menu_YesNo's line

// The menu as a value: the self-test steps it without a window.
enum class Act { None, Close, Load };
struct Menu {
    U cursor = 0;              // 0..15: the slot, 0 the newest
    U first = 0;               // the first of the three shown
    bool asking = false;       // "load this game?" over the cursor's slot
    U yes = 1;                 // Menu_YesNo's byte: 1 Yes (the left stop), 0 No; LoadMenu_Choose starts it at 1
    Act waiting = Act::None;   // acted on when confirm and cancel are both up
    std::uint16_t prev = 0xFFFF;   // every button down: nothing counts as pressed until released once
    U held = 0;                // frames a direction has been held
};

bool Present(const unsigned char* summaries, U slot) { return summaries[slot * save_menu::kSummaryBytes + 0x15] != 0xFF; }

Act Step(Menu& m, std::uint16_t pad, std::uint16_t confirm, std::uint16_t cancel, const unsigned char* summaries) {
    const std::uint16_t pressed = static_cast<std::uint16_t>(pad & ~m.prev);
    m.prev = pad;
    if (m.waiting != Act::None) {
        if ((pad & (confirm | cancel)) != 0) return Act::None;
        const Act a = m.waiting;
        m.waiting = Act::None;
        return a;
    }
    if (m.asking) {
        // Menu_YesNo's order: cancel (No), confirm, then left / right.
        if (pressed & cancel) {
            m.asking = false;
        } else if (pressed & confirm) {
            if (m.yes != 0) m.waiting = Act::Load;
            else m.asking = false;
        } else if (pressed & kLeftRight) {
            m.yes ^= 1;
        }
        m.held = 0;
        return Act::None;
    }
    // Up and down: on the press, then every kRepeatEvery frames once held kRepeatDelay.
    std::uint16_t move = pressed & (kUp | kDown);
    if ((pad & (kUp | kDown)) != 0) {
        ++m.held;
        if (move == 0 && m.held >= kRepeatDelay && (m.held - kRepeatDelay) % kRepeatEvery == 0) move = pad & (kUp | kDown);
    } else {
        m.held = 0;
    }
    if ((move & kUp) && m.cursor != 0) {
        --m.cursor;
        if (m.cursor < m.first) m.first = m.cursor;
    } else if ((move & kDown) && m.cursor < 15) {
        ++m.cursor;
        if (m.cursor > m.first + 2) m.first = m.cursor - 2;
    }
    if (pressed & confirm) {
        if (Present(summaries, m.cursor)) {   // an empty slot: nothing (LoadMenu_Choose buzzes, 0x107)
            m.asking = true;
            m.yes = 1;
        }
    } else if (pressed & cancel) {
        m.waiting = Act::Close;
    }
    return Act::None;
}

// --- the lists -------------------------------------------------------------------

// Ours, outside the span: a listing, not game state.
unsigned char g_table[save_menu::kListEntries * save_menu::kEntryBytes];
unsigned char g_summaries[16 * save_menu::kSummaryBytes];

// The summary of a quicksave's save half (0x1C bytes at 0xCA0, where the
// save menu's reader looks), read with stdio: nothing of the game's file
// layer or its staging buffer.
bool ReadSummary(const char* name, unsigned char* into, void* context) {
    const std::string path = std::string(static_cast<const char*>(context)) + name;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        bof3::Log("quickmenu   %s: cannot open; shown empty", path.c_str());
        return false;
    }
    const bool ok = std::fseek(f, 0xCA0, SEEK_SET) == 0 && std::fread(into, 1, save_menu::kSummaryBytes, f) == save_menu::kSummaryBytes;
    std::fclose(f);
    if (!ok) bof3::Log("quickmenu   %s: no summary at 0xCA0 (a short file); shown empty", path.c_str());
    return ok;
}

// The quick list of `dir` ("" for the game's directory, else ending in a
// separator) into g_table and g_summaries; the count listed.
int ListQuicksaves(const char* dir) {
    const std::string pattern = std::string(dir) + kQuickPattern;
    const int n = save_menu::ListFiles(pattern.c_str(), g_table, true, true);
    save_menu::ReadSummaries(g_table, g_summaries, &ReadSummary, const_cast<char*>(dir));
    return n;
}

// --- the menu in the game -----------------------------------------------------------

Menu g_menu;
bool g_open = false;
bool g_paused_by_us = false;   // the game was not paused (F9) when F5 opened the menu
bool g_loading = false;        // a Load asked; Snapshot_Pending runs it at this frame's end
unsigned g_loading_slot = 0;
char g_refusal[64];            // the last refusal's reason, upper case for the 8 x 8 font; "" for none

constexpr int kSlotsX = 0x20, kSlotsY = 0x30;   // the load menu's (LoadMenu_Choose's draw_slots)

void Open() {
    const int n = ListQuicksaves("");
    g_menu = Menu{};
    g_open = true;
    g_refusal[0] = 0;
    g_paused_by_us = Game_Paused == 0;
    if (g_paused_by_us) {
        Game_Paused = 1;
        Sound_PauseAll();
    }
    bof3::Log("quickmenu   F5 at Frame_Counter %u: open, %d quicksave%s listed", static_cast<U>(Frame_Counter), n,
              n == 1 ? "" : "s");
}

// Closing ends the pause, as any key ends F9's.
void Close(const char* why) {
    g_open = false;
    Game_Paused = 0;
    Sound_ResumeAll();
    bof3::Log("quickmenu   closed at Frame_Counter %u (%s)", static_cast<U>(Frame_Counter), why);
}

std::uint16_t ReadPad() {
    unsigned short word = 0;
    if (bof3::InputScript_PausedWord(&word)) return word;
    const std::uint16_t pad = static_cast<std::uint16_t>(Pad_Read() & 0xFFFF);
    return WinMain_InputAllowed() ? pad : 0;   // DIV-0033: nothing read while another window is in front
}

// Menu_YesNo's draw (menu_windows.cpp), without its input, its sound or its
// byte: the line through the line function DIV-0027 put in (Msg_SystemPtr
// without one) at (0x1C, 0x16), the hand at 0xFE - 36 * yes, or DIV-0027's
// 0x112 - 56 * yes, y 0x18.
void DrawYesNo(U yes) {
    const YesNoLayout_LineFn line_fn = YesNoLayout_ActiveLine();
    const unsigned char* const line = line_fn ? line_fn(kMsgYesNo) : Msg_SystemPtr(kMsgYesNo);
    Text_DrawAt(0x1C, 0x16, 0, 0xFF, line);
    const U hand = YesNoLayout_StopsMoved() ? 0x112u - 56u * yes : 0xFEu - 36u * yes;
    Menu_DrawHand(static_cast<int>(hand), 0x18, 0);
}

// The load menu's screens, in LoadMenu_Choose's and LoadMenu_Confirm's
// order: the windows and the message, the slots (lit while choosing), the
// Yes / No. Menu_DrawBlackScreen first, under the backdrop: it covers a wide
// picture's sides (DIV-0041), which the backdrop's ten columns do not.
void Draw() {
    Menu_DrawBlackScreen();
    const unsigned message = g_menu.asking ? kMsgConfirm : g_refusal[0] != 0 ? kMsgCouldNot : kMsgChoose;
    save_menu::DrawLoadWindows(message);
    save_menu::DrawSlotsOver(kSlotsX, kSlotsY, g_menu.asking ? 0 : 1, g_summaries, &g_menu.first, &g_menu.cursor);
    if (g_menu.asking) DrawYesNo(g_menu.yes);
    else if (g_refusal[0] != 0) Text_DrawFont8(0x10, 0xE0, 0, reinterpret_cast<const unsigned char*>(g_refusal));
}

// --- the self-test ------------------------------------------------------------------

[[noreturn]] void Fail(const char* what) { bof3::Fatal("quick menu self-test: %s", what); }

void WriteFake(const std::string& path, unsigned char mark) {
    // Not a save: 0xCBC bytes of a mark, so each listed file's summary says which file it came from.
    std::string bytes(0xCA0 + save_menu::kSummaryBytes, static_cast<char>(mark));
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f || std::fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size()) Fail("cannot write a test file");
    std::fclose(f);
}

void ListsSelfTest() {
    char tmp[MAX_PATH];
    if (!GetTempPathA(sizeof tmp, tmp)) Fail("GetTempPath");
    char dir_a[MAX_PATH], dir_b[MAX_PATH];
    std::snprintf(dir_a, sizeof dir_a, "%sbof3x_qm_a_%lu\\", tmp, GetCurrentProcessId());
    std::snprintf(dir_b, sizeof dir_b, "%sbof3x_qm_b_%lu\\", tmp, GetCurrentProcessId());
    CreateDirectoryA(dir_a, nullptr);
    CreateDirectoryA(dir_b, nullptr);
    std::vector<std::string> made;
    const auto Make = [&](const char* dir, const char* name, unsigned char mark) {
        made.push_back(std::string(dir) + name);
        WriteFake(made.back(), mark);
    };
    char name[16];
    // A: the sixteen saves 00..0F, a seventeenth match BISLPS0G.DAT, and three
    // quicksaves with gaps (10, 13, 1F).
    for (U s = 0; s < 16; ++s) {
        std::snprintf(name, sizeof name, "BISLPS0%X.DAT", s);
        Make(dir_a, name, static_cast<unsigned char>(s));
    }
    Make(dir_a, "BISLPS0G.DAT", 0x60);
    Make(dir_a, "BISLPS10.DAT", 0x80);
    Make(dir_a, "BISLPS13.DAT", 0x83);
    Make(dir_a, "BISLPS1F.DAT", 0x8F);
    // B: all sixteen quicksaves and a seventeenth, BISLPS1G.DAT.
    for (U s = 0; s < 16; ++s) {
        std::snprintf(name, sizeof name, "BISLPS1%X.DAT", s);
        Make(dir_b, name, static_cast<unsigned char>(0x80 + s));
    }
    Make(dir_b, "BISLPS1G.DAT", 0xE0);

    unsigned char table[save_menu::kListEntries * save_menu::kEntryBytes + 0x40];
    unsigned char sums[16 * save_menu::kSummaryBytes];
    const auto Guard = [&](const char* what) {
        for (U i = save_menu::kListEntries * save_menu::kEntryBytes; i < sizeof table; ++i)
            if (table[i] != 0xA5) Fail(what);
    };
    const auto Mark = [&](U slot) { return sums[slot * save_menu::kSummaryBytes]; };
    const auto Shown = [&](U slot) { return sums[slot * save_menu::kSummaryBytes + 0x15] != 0xFF; };

    // The saves of A as the armed Save_ListFiles lists them: sixteen, no quicksave, the seventeenth not stored.
    std::memset(table, 0xA5, sizeof table);
    std::string pattern = std::string(dir_a) + "BISLPS0?.DAT";
    int n = save_menu::ListFiles(pattern.c_str(), table, true, true);
    Guard("the save list wrote past its sixteen entries");
    if (n != 16) Fail("the save list of sixteen saves and BISLPS0G.DAT is not 16");
    for (U i = 0; i < 16; ++i)
        if (std::memcmp(table + i * save_menu::kEntryBytes, "BISLPS0", 7) != 0) Fail("a save list entry is not BISLPS0?");
    save_menu::ReadSummaries(table, sums, &ReadSummary, dir_a);
    for (U s = 0; s < 16; ++s)
        if (!Shown(s) || Mark(s) != s) Fail("a save slot's summary is not its own file's");

    // The quicksaves of A: three, slots 0, 3 and F, the rest empty.
    std::memset(table, 0xA5, sizeof table);
    pattern = std::string(dir_a) + kQuickPattern;
    n = save_menu::ListFiles(pattern.c_str(), table, true, true);
    Guard("the quick list wrote past its table");
    if (n != 3) Fail("the quick list of A is not 3");
    save_menu::ReadSummaries(table, sums, &ReadSummary, dir_a);
    for (U s = 0; s < 16; ++s) {
        const bool want = s == 0 || s == 3 || s == 0xF;
        if (Shown(s) != want) Fail("the quick list of A shows the wrong slots");
        if (want && Mark(s) != 0x80 + s) Fail("a quick slot's summary is not its own file's");
    }

    // B: sixteen quicksaves and BISLPS1G.DAT - sixteen listed, the bound held.
    std::memset(table, 0xA5, sizeof table);
    pattern = std::string(dir_b) + kQuickPattern;
    n = save_menu::ListFiles(pattern.c_str(), table, true, true);
    Guard("the quick list of seventeen wrote past its table");
    if (n != 16) Fail("the quick list of seventeen is not 16");
    save_menu::ReadSummaries(table, sums, &ReadSummary, dir_b);
    for (U s = 0; s < 16; ++s)
        if (!Shown(s) || Mark(s) != 0x80 + s) Fail("a quick slot of B is not its own file's");

    // The trap the split removes, measured: B listed as the original lists
    // (BISLPS??.DAT, here bounded so the table holds) shows quicksaves as
    // saves - BISLPS10.DAT's summary in save slot 0.
    std::memset(table, 0xA5, sizeof table);
    pattern = std::string(dir_b) + "BISLPS??.DAT";
    n = save_menu::ListFiles(pattern.c_str(), table, true, true);
    Guard("the ?? list wrote past its table");
    save_menu::ReadSummaries(table, sums, &ReadSummary, dir_b);
    if (!Shown(0) || Mark(0) != 0x80) Fail("the original's pattern did not show BISLPS10.DAT as save slot 0");
    // And A under it: twenty matches, sixteen stored - the original stores all twenty, 0x60 bytes past Save_Directory.
    std::memset(table, 0xA5, sizeof table);
    pattern = std::string(dir_a) + "BISLPS??.DAT";
    const int twenty = save_menu::ListFiles(pattern.c_str(), table, true, true);
    Guard("the ?? list of A wrote past its table");
    if (twenty != 16) Fail("the ?? list of A's twenty matches is not 16");

    for (const std::string& f : made) DeleteFileA(f.c_str());
    RemoveDirectoryA(dir_a);
    RemoveDirectoryA(dir_b);
    bof3::Log("shadow      quick menu self-test: the save list 16 of 17 (BISLPS0?.DAT, BISLPS0G.DAT not stored), the quick "
              "list 3 with gaps (slots 0, 3, F) and 16 of 17, each summary its own file's; BISLPS??.DAT shows "
              "BISLPS10.DAT as save slot 0 (the trap)");
}

void CursorSelfTest() {
    unsigned char sums[16 * save_menu::kSummaryBytes];
    std::memset(sums, 0, sizeof sums);
    for (U s = 0; s < 16; ++s) sums[s * save_menu::kSummaryBytes + 0x15] = (s == 0 || s == 3 || s == 15) ? 0 : 0xFF;
    constexpr std::uint16_t kConfirm = 0x40, kCancel = 0x10;
    Menu m;
    const auto Tap = [&](std::uint16_t b) {
        if (Step(m, b, kConfirm, kCancel, sums) != Act::None) Fail("a tap acted");
        if (Step(m, 0, kConfirm, kCancel, sums) != Act::None) Fail("a release acted");
    };
    // Held at open: nothing until released.
    if (Step(m, kConfirm, kConfirm, kCancel, sums) != Act::None || m.asking) Fail("a button held at the open counted");
    Step(m, 0, kConfirm, kCancel, sums);
    // Down through every slot, gaps included; the three shown follow.
    for (U i = 1; i <= 15; ++i) {
        Tap(kDown);
        if (m.cursor != i) Fail("down did not step the cursor");
        if (m.first != (i > 2 ? i - 2 : 0)) Fail("the first shown did not follow the cursor down");
    }
    Tap(kDown);
    if (m.cursor != 15 || m.first != 13) Fail("down moved past slot F");
    for (U i = 15; i-- > 0;) {
        Tap(kUp);
        if (m.cursor != i) Fail("up did not step the cursor");
        if (m.first > m.cursor || m.cursor > m.first + 2) Fail("the cursor left the three shown");
    }
    if (m.first != 0) Fail("the first shown is not 0 at slot 0");
    Tap(kUp);
    if (m.cursor != 0) Fail("up moved past slot 0");
    // A held direction repeats: the press, then frames 20, 24, 28 of the hold.
    for (U f = 0; f < 30; ++f) Step(m, kDown, kConfirm, kCancel, sums);
    Step(m, 0, kConfirm, kCancel, sums);
    if (m.cursor != 4) Fail("a held down did not repeat as 1 + 3 in 30 frames");
    // Slot 1 (empty): confirm does nothing, as the load menu.
    while (m.cursor != 1) Tap(kUp);
    Tap(kConfirm);
    if (m.asking) Fail("confirm on an empty slot asked");
    // Slot 3 (present): load this game?, Yes first.
    Tap(kDown);
    Tap(kDown);
    Tap(kConfirm);
    if (!m.asking || m.yes != 1) Fail("confirm on slot 3 did not ask with Yes");
    Tap(kDown);
    if (m.cursor != 3 || !m.asking) Fail("down moved the cursor while asking");
    Tap(kLeftRight & 0x2000);
    if (m.yes != 0) Fail("right did not flip to No");
    Tap(kConfirm);
    if (m.asking) Fail("No did not return to the slots");
    Tap(kConfirm);
    Tap(kCancel);
    if (m.asking) Fail("cancel did not answer No");
    Tap(kConfirm);
    Tap(0x8000);
    Tap(0x8000);
    if (m.yes != 1) Fail("left twice did not come back to Yes");
    if (Step(m, kConfirm, kConfirm, kCancel, sums) != Act::None) Fail("Yes acted while held");
    if (Step(m, kConfirm, kConfirm, kCancel, sums) != Act::None) Fail("Yes acted while held");
    if (Step(m, 0, kConfirm, kCancel, sums) != Act::Load || m.cursor != 3) Fail("Yes on slot 3 did not load at the release");
    // Cancel on the slots closes, at the release.
    m.asking = false;
    if (Step(m, kCancel, kConfirm, kCancel, sums) != Act::None) Fail("cancel acted on the press");
    if (Step(m, 0, kConfirm, kCancel, sums) != Act::Close) Fail("cancel on the slots did not close at the release");
    bof3::Log("shadow      quick menu self-test: the cursor over 0..15 with gaps (slots 0, 3, F present), the three "
              "shown following it, the held repeat, no question on an empty slot, Yes / No by left and right, No and "
              "cancel back to the slots, Yes loading and cancel closing at the release");
}

}  // namespace

namespace bof3 {

bool QuickMenu_Key(unsigned vk, long lparam) {
    const bool repeat = (lparam & (1L << 30)) != 0;
    if (g_open) {
        if (vk == VK_F5 && !repeat) Close("F5");
        return true;   // every key is the menu's while it is open
    }
    if (vk != VK_F5) return false;
    if (repeat) return true;
    if (Frame_Counter == 0) return false;   // before the loop's first frame (the intro videos): F5 is nothing, as before
    Open();
    return true;
}

bool QuickMenu_Frame() {
    if (!g_open) return false;
    const Act act = Step(g_menu, ReadPad(), Field_ConfirmButtons, Field_CancelButtons, g_summaries);
    if (act == Act::Close) {
        Close("cancel");
        return true;
    }
    if (act == Act::Load) {
        // Still paused and open: the snapshot core runs the load at this
        // frame's end (Snapshot_Pending), and QuickMenu_AfterPoint reads how
        // it went.
        g_loading_slot = kFirstSlot + g_menu.cursor;
        g_loading = true;
        g_menu.asking = false;
        bof3::Log("quickmenu   Load of BISLPS%02X.DAT asked at Frame_Counter %u", g_loading_slot, static_cast<U>(Frame_Counter));
        Snapshot_RequestLoad(g_loading_slot);
    }
    if (g_menu.asking) g_refusal[0] = 0;   // a new question: the last refusal is answered
    Draw();
    return true;
}

void QuickMenu_AfterPoint() {
    if (!g_loading) return;
    g_loading = false;
    const char* const line = Snapshot_LastLine();
    constexpr char kRefused[] = "Quickload refused: ";
    if (std::strncmp(line, kRefused, sizeof kRefused - 1) == 0) {
        // Refused, the game untouched: the menu stays, with the reason.
        std::snprintf(g_refusal, sizeof g_refusal, "REFUSED - %s", line + sizeof kRefused - 1);
        for (char* c = g_refusal; *c; ++c)
            if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
        bof3::Log("quickmenu   BISLPS%02X.DAT not loaded (%s); the menu stays open", g_loading_slot, line);
        return;
    }
    // Restored: the span holds the take's Game_Paused (0: a take is never
    // made while paused - F6's WM_KEYDOWN ends a pause first, and the menu
    // takes F6 while it is open). The sound the menu paused is resumed.
    g_open = false;
    if (Game_Paused != 0) {
        // Not a path known to exist: then the game is paused as the take
        // had it, the pause lines up, and the next key resumes it and its sound.
        bof3::Log("quickmenu   BISLPS%02X.DAT loaded with Game_Paused set (the take's): left paused", g_loading_slot);
        return;
    }
    Sound_ResumeAll();
    bof3::Log("quickmenu   closed at Frame_Counter %u (BISLPS%02X.DAT loaded: %s)", static_cast<U>(Frame_Counter),
              g_loading_slot, line);
}

void QuickMenu_Inject() {
    if (IsExeHost() ? GetEnvironmentVariableA("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : WantsShadow("quick_menu")) {
        ListsSelfTest();
        CursorSelfTest();
    }
    SaveLists_Arm();
}

}  // namespace bof3

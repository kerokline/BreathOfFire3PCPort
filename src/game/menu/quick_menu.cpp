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
//
// DIVERGENCE (the state-by-name round's group A4, ledger draft B,
// docs/state-by-name-tier0.md 2): a refused quicksave loads as its save. When
// the core refuses a load and its save half is whole (Snapshot_LastLoad's
// RefusedSaveOffered), the menu asks a second question: the reason and "Load
// the save in this quicksave?" in the 8 x 8 font below the slots, the game's
// own Yes / No in the title box. No keeps the game as it is (the slots and the
// reason, as any refusal). Yes takes the game's own road: the save half read
// (Snapshot_ReadSaveHalf), then the restart F9's second press asks
// (Game_RestartFlag: the loop sets the stack base again and creates the tasks
// afresh, Boot_Task first); when the title's task 1 has begun (Title_Task has
// put its state word, which the restart left at a mark of ours, to 0), its
// state is put to 7, Title_StateMenu - the state Start on the logo leads to -
// so the title's own code makes task 0 the title's menu task; the menu's
// first step (TitleFlow_Begin) then goes to the load menu's read
// (LoadMenu_Read) with this save half as the bytes read in place of a
// BISLPS0?.DAT, and LoadMenu_Loaded, LoadMenu_Apply and TitleFlow_EnterGame
// run unchanged. F7 asks nothing: its line says F5 can load the save.
#include "game/quick_menu.h"

#include <filesystem>
#include <system_error>

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
#include "game/save_tail.h"
#include "input/key_codes.h"
#include "os/os.h"

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
constexpr unsigned kBattleMode = 5;         // Game_Mode in a battle (docs/mode-rest.md 1.1)

// The menu as a value: the self-test steps it without a window.
enum class Act { None, Close, Load, LoadSave };
struct Menu {
    U cursor = 0;              // 0..15: the slot, 0 the newest
    U first = 0;               // the first of the three shown
    bool asking = false;       // "load this game?" over the cursor's slot
    bool offer = false;        // refused, the save half whole: "load the save in this quicksave?" (draft B)
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
    if (m.asking || m.offer) {
        // Menu_YesNo's order: cancel (No), confirm, then left / right. No
        // leaves either question for the slots.
        if (pressed & cancel) {
            m.asking = m.offer = false;
        } else if (pressed & confirm) {
            if (m.yes != 0) m.waiting = m.offer ? Act::LoadSave : Act::Load;
            else m.asking = m.offer = false;
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
    FILE* f = bof3::os::OpenNarrow(path.c_str(), "rb");   // std::fopen on Windows; the backslash a separator on POSIX
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

// Draft B: the save half's road (the comment at the top). Ours, outside the
// span: the restart rewrites the span's game state, these carry the request
// across it.
enum class Tier0 { None, Restart, Title, Read };
Tier0 g_tier0 = Tier0::None;
std::vector<unsigned char> g_half;   // the save half's 0x12B0 bytes, as Snapshot_ReadSaveHalf gave them
unsigned g_half_slot = 0;
// Title_State (task 1's private word +0x18) while the restart is on its way:
// no Title_States entry, so its change to 0 is Title_Task's own entry.
// Task_Create keeps a record's private words and Task_ClearPrivate clears
// +8..+0x17 only, so nothing else writes it before then.
constexpr std::uint16_t kTitleStateMark = 0xFFFF;
constexpr std::uint16_t kTitleStateMenu = 7;   // Title_StateMenu, Title_States[7]
// The question below the reason, ours (Simplified Technical English,
// CLAUDE.md): there is no message for it in the game's pools, so it is drawn
// as the reason is, in the 8 x 8 font, under every language.
constexpr char kSaveQuestion[] = "LOAD THE SAVE IN THIS QUICKSAVE?";
char g_f7_line[160];           // F7's refusal line with the way to the save (draft B)

constexpr int kSlotsX = 0x20, kSlotsY = 0x30;   // the load menu's (LoadMenu_Choose's draw_slots)

// The load menu's four lines as FIRST.DAT's pool holds them, copied when the
// title loads it (QuickMenu_TitlePoolLoaded) - after its language layers, so
// translated as the title's own load menu is. The live pool is the scene's:
// a battle's (BATTLE.DAT, BOSS*.DAT) has "You can't escape" at 0xF and
// nothing at 0xAC, 0xB8 or 0xB9 (measured over the en-US cache, 2026-11-06),
// which drew a blank heading and stopped the game in DIV-0027's shape check.
constexpr unsigned kTitleIds[4] = {kMsgChoose, kMsgConfirm, kMsgCouldNot, kMsgYesNo};
constexpr unsigned kTitleLineRoom = 128;
unsigned char g_title_lines[4][kTitleLineRoom];
bool g_title_taken = false;

const unsigned char* TitleLine(unsigned id) {
    for (unsigned i = 0; i < 4; ++i)
        if (kTitleIds[i] == id) return g_title_lines[i];
    bof3::Fatal("quick menu: system message 0x%X is not one of the load menu's four", id);
}

// --- the backdrop ----------------------------------------------------------------
//
// DIVERGENCE (DIV-0147 amended, docs/quicksave.md QM.9): the load menu's
// backdrop in every scene. Menu_DrawBackdrop's tiles are cells of the page
// (0x340, 0x100) and its four CLUTs sixteen cells each of row 0x1E2 from x
// 0x30; a scene may keep pictures of its own there (a battle does, QM.8). So
// the cells are copied when the title's menu has loaded START.DAT, whose image
// chunk at (0x340, 0x100) is the menus' page (FIRST.DAT's there is DEMO.DAT's
// to overwrite; row 0x1E2 is FIRST.DAT's strip chunk 0x8400, in the shadow by
// then) - the player's own data, kept in memory of ours outside the span, as
// the four lines are - and
// while the menu is open on a scene whose cells differ, the title's are in the
// VRAM shadow and the scene's are kept here; closing puts the scene's back.
// The tiles' cells are the box around every rectangle the pattern names
// (Menu_BackdropRects by Menu_BackdropPattern), read from the image, 4-bit
// texels: four to a cell.
//
// A quick-load that runs writes the span over the shadow, the take's cells
// with it, so nothing is put back then. F6 is the menu's while it is open, so
// no take holds the title's cells in a scene's place.
struct Cells {
    short rect[4];                     // x, y, w, h in VRAM cells
    std::vector<std::uint16_t> title;  // as the title had them
    std::vector<std::uint16_t> scene;  // the scene's, while the title's are in
};
Cells g_tiles, g_cluts;
bool g_backdrop_taken = false;   // the title's cells are held
bool g_backdrop_in = false;      // the title's cells are in the shadow, the scene's in `scene`
bool g_backdrop_draw = false;    // this opening draws the backdrop

constexpr int kPageX = 0x340, kPageY = 0x100;                    // Menu_DrawBackdrop's page
constexpr int kClutX = 0x30, kClutY = 0x1E2, kClutCells = 0x40;   // its four CLUTs: 0x30, 0x40, 0x50, 0x60
constexpr unsigned kPatternBytes = Menu_BackdropPattern_count;   // the pattern, up to the kinds' starts
constexpr unsigned kRectCount = Menu_BackdropRects_count / 4;    // the rectangles, up to the pattern

void StoreCells(const Cells& c, std::vector<std::uint16_t>& into) {
    into.resize(static_cast<std::size_t>(c.rect[2]) * static_cast<std::size_t>(c.rect[3]));
    Gfx_StoreImage(c.rect, into.data());
}

// The cells written and the textures under them dropped (Gfx_LoadImage); a
// CLUT row's converted copy made again, as world_map_ours.cpp's KeepDimCluts
// does it: the row's first-use conversion if not yet made, then our cells.
void LoadCells(const Cells& c, const std::vector<std::uint16_t>& from, bool clut) {
    Gfx_LoadImage(c.rect, from.data());
    if (!clut) return;
    struct ClutRow { std::uint32_t generation; ::bof3::Slot32<void> pixels; };   // gfx_clut.cpp's
    auto* const rows = reinterpret_cast<ClutRow*>(Gfx_ClutRows);
    if (rows[c.rect[1]].pixels == nullptr) Gfx_ClutPixels(static_cast<unsigned>(c.rect[1] << 6));
    Gfx_ConvertRow(c.rect[0], c.rect[1], c.rect[2]);
}

// The title's cells taken (QuickMenu_MenuPageLoaded). Tiles that are all one
// value are no picture: the backdrop is then drawn as before this entry (from
// the scene's cells, and not in a battle).
void TakeBackdrop() {
    const unsigned char* const pattern = Menu_BackdropPattern;
    const unsigned char* const rects = Menu_BackdropRects;
    int u0 = 256, v0 = 256, u1 = 0, v1 = 0;
    for (unsigned i = 0; i < kPatternBytes; ++i) {
        if (pattern[i] >= kRectCount)
            bof3::Fatal("quick menu: the backdrop's pattern byte %u names rectangle %u of %u", i, pattern[i], kRectCount);
        const unsigned char* const r = rects + pattern[i] * 4u;
        if (r[2] == 0 || r[3] == 0) continue;
        if (r[0] < u0) u0 = r[0];
        if (r[1] < v0) v0 = r[1];
        if (r[0] + r[2] > u1) u1 = r[0] + r[2];
        if (r[1] + r[3] > v1) v1 = r[1] + r[3];
    }
    if (u0 >= u1 || v0 >= v1 || v1 > 256) bof3::Fatal("quick menu: the backdrop's rectangles cover u %d..%d, v %d..%d", u0, u1, v0, v1);
    const int x0 = u0 / 4, x1 = (u1 + 3) / 4;
    const short tiles[4] = {static_cast<short>(kPageX + x0), static_cast<short>(kPageY + v0), static_cast<short>(x1 - x0),
                            static_cast<short>(v1 - v0)};
    const short cluts[4] = {kClutX, kClutY, kClutCells, 1};
    std::memcpy(g_tiles.rect, tiles, sizeof tiles);
    std::memcpy(g_cluts.rect, cluts, sizeof cluts);
    StoreCells(g_tiles, g_tiles.title);
    StoreCells(g_cluts, g_cluts.title);
    bool picture = false;
    for (const std::uint16_t cell : g_tiles.title) picture = picture || cell != g_tiles.title[0];
    if (picture != g_backdrop_taken || !picture)
        bof3::Log("quickmenu   the load menu's backdrop %s START.DAT's load: tiles at (0x%X, 0x%X) %d x %d cells, CLUTs at "
                  "(0x%X, 0x%X) %d cells", picture ? "taken after" : "is not in the VRAM shadow after", tiles[0], tiles[1],
                  tiles[2], tiles[3], kClutX, kClutY, kClutCells);
    g_backdrop_taken = picture;
}

// At the menu's open: the title's cells in, when the scene's differ.
void BackdropIn() {
    g_backdrop_in = false;
    if (!g_backdrop_taken) {
        g_backdrop_draw = Game_Mode != kBattleMode;   // QM.8's test, as before
        return;
    }
    g_backdrop_draw = true;
    StoreCells(g_tiles, g_tiles.scene);
    StoreCells(g_cluts, g_cluts.scene);
    unsigned tiles = 0, cluts = 0;
    for (std::size_t i = 0; i < g_tiles.title.size(); ++i) tiles += g_tiles.scene[i] != g_tiles.title[i];
    for (std::size_t i = 0; i < g_cluts.title.size(); ++i) cluts += g_cluts.scene[i] != g_cluts.title[i];
    if (tiles == 0 && cluts == 0) {
        bof3::Log("quickmenu   the backdrop's cells are the title's in this scene (Game_Mode %u): drawn as they are",
                  static_cast<U>(Game_Mode));
        return;
    }
    LoadCells(g_tiles, g_tiles.title, false);
    LoadCells(g_cluts, g_cluts.title, true);
    g_backdrop_in = true;
    bof3::Log("quickmenu   the backdrop's cells differ from the title's in this scene (Game_Mode %u: %u of %u tile cells, "
              "%u of %u CLUT cells): the title's put in while the menu is open", static_cast<U>(Game_Mode), tiles,
              static_cast<unsigned>(g_tiles.title.size()), cluts, static_cast<unsigned>(g_cluts.title.size()));
}

// The scene's cells back (the menu closed, or the restart asked).
void BackdropOut() {
    if (!g_backdrop_in) return;
    g_backdrop_in = false;
    LoadCells(g_tiles, g_tiles.scene, false);
    LoadCells(g_cluts, g_cluts.scene, true);
    bof3::Log("quickmenu   the scene's cells put back under the backdrop");
}

void Open() {
    if (!g_title_taken) {
        // Only before the title has loaded FIRST.DAT through our LoadDatFile
        // (or with Capcom's, BOF3X_ORIGINAL=LoadDatFile): no text to draw with.
        bof3::Log("quickmenu   F5 at Frame_Counter %u: not opened - the title's system pool (FIRST.DAT) has not "
                  "been loaded yet, so the load menu's text is not known", static_cast<U>(Frame_Counter));
        return;
    }
    const int n = ListQuicksaves("");
    g_menu = Menu{};
    g_open = true;
    g_refusal[0] = 0;
    BackdropIn();
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
    BackdropOut();
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
    const unsigned char* const own = TitleLine(kMsgYesNo);
    const unsigned char* const line = YesNoLayout_ActiveLine() ? YesNoLayout_SystemLine(own) : own;
    Text_DrawAt(0x1C, 0x16, 0, 0xFF, line);
    const U hand = YesNoLayout_StopsMoved() ? 0x112u - 56u * yes : 0xFEu - 36u * yes;
    Menu_DrawHand(static_cast<int>(hand), 0x18, 0);
}

// The load menu's screens, in LoadMenu_Choose's and LoadMenu_Confirm's
// order: the windows and the message, the slots (lit while choosing), the
// Yes / No. Menu_DrawBlackScreen first, under the backdrop: it covers a wide
// picture's sides (DIV-0041), which the backdrop's ten columns do not. In a
// battle the backdrop's texture page (0x340) and CLUT rows (y 0x1E2) hold the
// battle's pictures, so the backdrop is left out and the black screen is the
// background; the title box, the slots, the portraits, the font and the hand
// draw right there (the capture of 2026-11-06, docs/quicksave.md QM.8).
// Since QM.9 that is only when the title's backdrop was not taken: with it
// held, BackdropIn has the title's cells in the shadow and the backdrop is
// drawn in every scene (g_backdrop_draw).
//
// Draft B's question: the title box holds the game's Yes / No alone (its line
// is spaces and then the two words, so no message of the box's is under it),
// the slots unlit, and below them the reason and the question, two lines of
// the 8 x 8 font.
void Draw() {
    Menu_DrawBlackScreen();
    static const unsigned char kNoMessage[1] = {0};
    const unsigned message = g_menu.asking ? kMsgConfirm : g_refusal[0] != 0 ? kMsgCouldNot : kMsgChoose;
    save_menu::DrawLoadWindows(g_backdrop_draw, g_menu.offer ? kNoMessage : TitleLine(message));
    save_menu::DrawSlotsOver(kSlotsX, kSlotsY, g_menu.asking || g_menu.offer ? 0 : 1, g_summaries, &g_menu.first, &g_menu.cursor);
    if (g_menu.asking || g_menu.offer) DrawYesNo(g_menu.yes);
    if (g_menu.offer) {
        char two[sizeof g_refusal + sizeof kSaveQuestion + 1];
        std::snprintf(two, sizeof two, "%s\n%s", g_refusal, kSaveQuestion);
        Text_DrawFont8(0x10, 0xE0, 0, reinterpret_cast<const unsigned char*>(two));
    } else if (!g_menu.asking && g_refusal[0] != 0) {
        Text_DrawFont8(0x10, 0xE0, 0, reinterpret_cast<const unsigned char*>(g_refusal));
    }
}

// Draft B, Yes: the save half read, the restart asked as F9's second press
// asks it - Game_Paused 0 and Game_RestartFlag, the sound left paused as that
// press leaves it (the title's music is started afresh) - and the menu closed.
// A save half that cannot be read (the file changed since the refusal) keeps
// the menu, with the reason.
void AskSaveLoad(unsigned slot) {
    std::string why;
    if (!bof3::Snapshot_ReadSaveHalf(slot, g_half, why)) {
        std::snprintf(g_refusal, sizeof g_refusal, "NO SAVE - %s", why.c_str());
        for (char* c = g_refusal; *c; ++c)
            if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
        bof3::Log("quickmenu   BISLPS%02X.DAT: the save half cannot be read (%s); the menu stays open", slot, why.c_str());
        return;
    }
    g_half_slot = slot;
    BackdropOut();   // the restart's scene is the game's to load: the cells as the scene had them
    g_tier0 = Tier0::Restart;
    Title_State = kTitleStateMark;
    g_open = false;
    Game_Paused = 0;
    Game_RestartFlag = 1;
    bof3::Log("quickmenu   BISLPS%02X.DAT's save half at Frame_Counter %u: the restart asked (Game_RestartFlag), then "
              "the title's load of it (draft B)", slot, static_cast<U>(Frame_Counter));
}

// Draft B at the frame point: once the title's task 1 has begun after the
// restart, its state 7, the way Start on the logo goes (Title_CheckStart):
// Title_StateMenu then stops the music, makes task 0 the title's menu task
// TitleTask_Run and ends task 1. Title_StartArmed 0, as Title_CheckStart
// leaves it. No transition is started: the load menu's read follows at once
// and LoadMenu_Loaded starts its own.
void Tier0Frame() {
    if (g_tier0 != Tier0::Restart || Title_State == kTitleStateMark) return;
    const unsigned was = Title_State;
    Title_State = kTitleStateMenu;
    Title_StartArmed = 0;
    g_tier0 = Tier0::Title;
    bof3::Log("quickmenu   the title's task began at Frame_Counter %u (its state %u): state 7, the title's menu (draft B)",
              static_cast<U>(Frame_Counter), was);
}

// F7 (draft B): a refusal whose save half is whole says that F5 can load it.
// Right after the point, where the core has just put its line in Overlay_Text:
// the line is copied with the addition into ours, and Overlay_Text names ours.
void F7Line() {
    if (g_open || g_tier0 != Tier0::None) return;   // the menu's own refusal, or its road under way
    const char* const line = bof3::Snapshot_LastLine();
    const char* const shown = Overlay_Text;
    constexpr char kRefused[] = "Quickload refused: ";
    if (shown != line || std::strncmp(line, kRefused, sizeof kRefused - 1) != 0) return;
    if (bof3::Snapshot_LastLoad().answer != bof3::SnapshotAnswer::RefusedSaveOffered) return;
    std::snprintf(g_f7_line, sizeof g_f7_line, "%s - use F5 to load the save in it", line);
    Overlay_Text = g_f7_line;
    bof3::Log("quickmenu   F7: %s (draft B)", g_f7_line);
}

// --- the self-test ------------------------------------------------------------------

[[noreturn]] void Fail(const char* what) { bof3::Fatal("quick menu self-test: %s", what); }

void WriteFake(const std::string& path, unsigned char mark) {
    // Not a save: 0xCBC bytes of a mark, so each listed file's summary says which file it came from.
    std::string bytes(0xCA0 + save_menu::kSummaryBytes, static_cast<char>(mark));
    FILE* f = bof3::os::OpenNarrow(path.c_str(), "wb");   // the self-test's folders end in the system's separator
    if (!f || std::fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size()) Fail("cannot write a test file");
    std::fclose(f);
}

void ListsSelfTest() {
    std::error_code ec;
    // In the narrow encoding (os/file.h), as GetTempPathA gave it: these names go to fopen and to the game's lists.
    const std::string tmp_dir = bof3::os::Narrow(std::filesystem::temp_directory_path(ec));
    if (ec || tmp_dir.empty()) Fail("temp_directory_path");
    const char sep = static_cast<char>(std::filesystem::path::preferred_separator);
    const std::string tmp = tmp_dir.back() == sep ? tmp_dir : tmp_dir + sep;
    char dir_a[bof3::os::kMaxPath], dir_b[bof3::os::kMaxPath];
    // Each folder ends in the system's separator, as `tmp` does: a backslash
    // on Windows, as these names were always built; a slash on POSIX, so the
    // patterns and names built on them carry no backslash there (the seam
    // would read one as a separator, but the log showed it in the name).
    std::snprintf(dir_a, sizeof dir_a, "%sbof3x_qm_a_%u%c", tmp.c_str(), static_cast<std::uint32_t>(bof3::os::ProcessId()), sep);
    std::snprintf(dir_b, sizeof dir_b, "%sbof3x_qm_b_%u%c", tmp.c_str(), static_cast<std::uint32_t>(bof3::os::ProcessId()), sep);
    std::filesystem::create_directories(bof3::os::Path(dir_a), ec);
    std::filesystem::create_directories(bof3::os::Path(dir_b), ec);
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

    for (const std::string& f : made) std::filesystem::remove(bof3::os::Path(f.c_str()), ec);
    std::filesystem::remove(bof3::os::Path(dir_a), ec);
    std::filesystem::remove(bof3::os::Path(dir_b), ec);
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
    // Draft B's question (a refusal with the save half whole): Yes first;
    // right and confirm is No, back to the slots; cancel is No; Yes acts at
    // the release as LoadSave, on the cursor's slot.
    m = Menu{};
    Step(m, 0, kConfirm, kCancel, sums);
    m.cursor = 3;
    m.offer = true;
    m.yes = 1;
    Tap(kLeftRight & 0x2000);
    if (m.yes != 0 || !m.offer) Fail("right did not flip the save question to No");
    Tap(kConfirm);
    if (m.offer || m.asking) Fail("No on the save question did not return to the slots");
    m.offer = true;
    m.yes = 1;
    Tap(kCancel);
    if (m.offer) Fail("cancel did not answer the save question No");
    m.offer = true;
    m.yes = 1;
    Tap(kDown);
    if (m.cursor != 3 || !m.offer) Fail("down moved the cursor during the save question");
    if (Step(m, kConfirm, kConfirm, kCancel, sums) != Act::None) Fail("Yes on the save question acted while held");
    if (Step(m, 0, kConfirm, kCancel, sums) != Act::LoadSave || m.cursor != 3)
        Fail("Yes on the save question did not ask the save's load at the release");
    // Cancel on the slots closes, at the release.
    m.asking = m.offer = false;
    if (Step(m, kCancel, kConfirm, kCancel, sums) != Act::None) Fail("cancel acted on the press");
    if (Step(m, 0, kConfirm, kCancel, sums) != Act::Close) Fail("cancel on the slots did not close at the release");
    bof3::Log("shadow      quick menu self-test: the cursor over 0..15 with gaps (slots 0, 3, F present), the three "
              "shown following it, the held repeat, no question on an empty slot, Yes / No by left and right, No and "
              "cancel back to the slots, Yes loading and cancel closing at the release; the save question's No, cancel and Yes "
              "(LoadSave at the release)");
}

}  // namespace

namespace bof3 {

bool QuickMenu_Key(bool its_key, bool repeat) {
    if (g_open) {
        if (its_key && !repeat) Close("its key");
        return true;   // every key is the menu's while it is open
    }
    if (!its_key) return false;
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
    if (act == Act::LoadSave) {
        AskSaveLoad(kFirstSlot + g_menu.cursor);
        if (!g_open) return true;   // the restart comes at the loop's next pass: nothing more drawn
        g_menu.offer = false;
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

void QuickMenu_TitlePoolLoaded() {
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char* const m = Msg_SystemPtr(kTitleIds[i]);
        const std::size_t len = std::strlen(reinterpret_cast<const char*>(m));
        if (len >= kTitleLineRoom)
            bof3::Fatal("quick menu: FIRST.DAT's system message 0x%X is %u bytes, room is %u", kTitleIds[i],
                        static_cast<unsigned>(len), kTitleLineRoom - 1);
        std::memcpy(g_title_lines[i], m, len + 1);
    }
    if (!g_title_taken)
        bof3::Log("quickmenu   the load menu's lines taken from FIRST.DAT's pool (0x%X, 0x%X, 0x%X, 0x%X)", kTitleIds[0],
                  kTitleIds[1], kTitleIds[2], kTitleIds[3]);
    g_title_taken = true;
}

void QuickMenu_MenuPageLoaded() { TakeBackdrop(); }

void QuickMenu_AfterPoint() {
    Tier0Frame();
    SaveTail_Frame();
    if (!g_loading) {
        F7Line();
        return;
    }
    g_loading = false;
    const char* const line = Snapshot_LastLine();
    constexpr char kRefused[] = "Quickload refused: ";
    if (std::strncmp(line, kRefused, sizeof kRefused - 1) == 0) {
        // Refused, the game untouched: the menu stays, with the reason.
        std::snprintf(g_refusal, sizeof g_refusal, "REFUSED - %s", line + sizeof kRefused - 1);
        for (char* c = g_refusal; *c; ++c)
            if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
        bof3::Log("quickmenu   BISLPS%02X.DAT not loaded (%s); the menu stays open", g_loading_slot, line);
        // The line stays the core's text, in a buffer of ours, so F7Line (which
        // acts on the core's own buffer) does not take the menu's refusal for F7's.
        std::snprintf(g_f7_line, sizeof g_f7_line, "%s", line);
        Overlay_Text = g_f7_line;
        // Draft B: its save half is whole - ask whether to load it.
        const SnapshotLoadResult& r = Snapshot_LastLoad();
        if (r.answer == SnapshotAnswer::RefusedSaveOffered && r.slot == g_loading_slot) {
            g_menu.offer = true;
            g_menu.yes = 1;
            bof3::Log("quickmenu   BISLPS%02X.DAT: its save half is whole; asked whether to load it (draft B)", g_loading_slot);
        }
        return;
    }
    // Restored: the span holds the take's Game_Paused (0: a take is never
    // made while paused - F6's WM_KEYDOWN ends a pause first, and the menu
    // takes F6 while it is open). The sound the menu paused is resumed.
    // The backdrop's cells are the take's now (the shadow is in the span):
    // nothing of this scene is put back.
    g_open = false;
    g_backdrop_in = false;
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

const unsigned char* QuickMenu_TitleBegins() {
    SaveTail_Forget();
    if (g_tier0 != Tier0::Title) return nullptr;
    g_tier0 = Tier0::Read;
    bof3::Log("quickmenu   the title's menu began at Frame_Counter %u: to the load menu's read of BISLPS%02X.DAT's save "
              "half (draft B)", static_cast<U>(Frame_Counter), g_half_slot);
    return g_half.data();
}

const unsigned char* QuickMenu_SaveHalfForRead() {
    if (g_tier0 != Tier0::Read) return nullptr;
    g_tier0 = Tier0::None;
    return g_half.data();
}

unsigned QuickMenu_SaveHalfSlot() { return g_half_slot; }

void QuickMenu_Inject() {
    const bool selftest = IsExeHost() ? bof3::os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : WantsShadow("quick_menu");
    if (selftest) {
        ListsSelfTest();
        CursorSelfTest();
    }
    SaveLists_Arm();
    SaveTail_Inject(selftest);
}

}  // namespace bof3

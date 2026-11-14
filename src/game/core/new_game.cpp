// The New Game sequence, ours: the name screen of START.EMI's New Game
// sub-steps, then the Config screen on the title's Game_Step 4 path
// (new_game.h; docs/new-game-round.md section 3a M1, 3b, stage B group TF).
//
// Read for this file (capstone over the sibling's START capture b8cc1561,
// through group M-A's scratch lib, 2026-11-14): the New Game step
// 0x801E4490..0x801E44D3 and its sub-steps n0 0x801E44D4, n1 0x801E4550, the
// input step's commit 0x801E4D80..0x801E4E08 and n3 0x801E4E0C..0x801E4E53;
// the draw-all 0x801E6F80..0x801E70D8; the name panel 0x801E6308..0x801E6417;
// its portrait helper 0x801DC37C. What differs from them is said where it
// happens, marked "ours:" (the ledger entries are stage C's).
//
// DIVERGENCE (ledger entry numbered at stage C) NewGameNameEntry and
// NewGameOptions: the PlayStation's name entry and options screen back between
// the title's New Game and the game.
#include "game/core/new_game.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "common/image_base.h"
#include "game/menu/name_grid.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/snapshot.h"

namespace new_game {
namespace {

using name_grid::Cells;
using name_grid::Copy;

// The switches: 0 in the image, 1 through PatchBytes at Arm (after every
// module's self-test); BOF3X_OFF names them to keep the port's flow.
std::uint8_t g_name_on = 0;      // NewGameNameEntry
std::uint8_t g_options_on = 0;   // NewGameOptions

// New Game's cells, ours and new (round doc section 5): START's buffer
// 0x801ED070, name cursor 0x801ED75C, column 0x801ED760, row 0x801ED764, page
// 0x801ED768 (research 4.6). Game state: carried by the snapshot participant
// NEWGAME below.
Cells g_cells{};
// The buffer and the page are reset where the PlayStation reloads START, the
// title task's start (M1: n0 resets the cursor, column and row only, and a
// quit leaves both as they were). True until the first open after
// TitleFlow_Begin.
bool g_fresh = true;

// The name panel's frame, a Menu_DrawPieces list in the PC's .data:
// START's list at 0x801ECF28 (the panel's Menu_DrawPieces, 0x801E63F0) equals
// these bytes through its end marker, 162 bytes (scratch panel_cmp2.py over
// the capture and BOF3.exe, 2026-11-14). The same list CommuMember_DrawPanel
// 0x45EE10 draws (commu.cpp kPanelPieces).
constexpr ::bof3::image::Va kPanelPieces{0x652F5C};

// Character record 0's name: CharacterRecords +0, an 8-byte field (the PC's
// member commit 0x45F5A0 writes 8, research 4.5; M3).
constexpr unsigned kNameRoom = 8;

// The sub-steps on SaveMenu_FlowState 0x6BDF86 under Game_Step 2.
constexpr unsigned kOpen = 0;   // the records, then the first screen wanted

// --- the step list --------------------------------------------------------

struct Screen {
    const char* name;
    unsigned first, last;          // its sub-steps, first..last
    bool (*wanted)();              // whether it shows at this New Game
    void (*step)(unsigned sub);    // one frame of sub-step first + sub
};

bool NameWanted();
void NameStep(unsigned sub);

// In order. IDEAS I56's champion screen joins here: its sub-steps after the
// last one in use (5..), its own test, its step - and a NextScreen() call
// where it ends, as the name screen's fade-out has.
constexpr Screen kScreens[] = {
    {"the name entry", 1, 4, NameWanted, NameStep},
};
constexpr int kScreenCount = static_cast<int>(sizeof kScreens / sizeof kScreens[0]);

int NextWanted(int after) {
    for (int i = after + 1; i < kScreenCount; ++i)
        if (kScreens[i].wanted()) return i;
    return -1;
}

int ScreenOf(unsigned sub) {
    for (int i = 0; i < kScreenCount; ++i)
        if (sub >= kScreens[i].first && sub <= kScreens[i].last) return i;
    return -1;
}

// After the last screen (or with none): the port's rest of TitleFlow_NewGame,
// then the Config screen at Game_Step 4 with the screen black and no fade
// running - as the title's third row enters it after TitleMenu_Leave's wait -
// or, with the options off, the port's Transition_Start(0) and Game_Step 5.
// ours: on the PlayStation the options come after EnterGame's growth roll and
// their closing starts the scenario; the port moved that tail here (M1,
// correction a), and the order stays the port's.
void Finish() {
    SaveMenu_FlowState = kOpen;
    if (g_options_on != 0) TitleFlow_NewGameRest(false, 4);
    else TitleFlow_NewGameRest(true, 5);
}

void NextScreen(int after) {
    const int next = NextWanted(after);
    if (next < 0) {
        Finish();
        return;
    }
    SaveMenu_FlowState = static_cast<unsigned char>(kScreens[next].first);
}

// --- the name screen ------------------------------------------------------------

bool NameWanted() { return g_name_on != 0 && name_grid::NameGrid_Available(); }

unsigned char* RecordName() { return move_script::At(::bof3::Img(CharacterRecords_addr)); }

// 0x801E6308 (x, y): the PC's CommuMember_DrawPanel with a box 0x2F high,
// portrait entry 0 and the underline (M1). START's portrait helper 0x801DC37C
// is CommuMember_DrawPortrait's shape, and its table 0x801EC908's entry 0
// equals the PC table 0x652F38's entry 0 (scratch panel_cmp.py): the PC's
// function draws it. The name is the cells as the layer stores them (the
// PlayStation draws its one-byte codes as they are); the colour is the
// PlayStation's (buffer byte 5 non-zero: 2), always 0 in effect.
void DrawPanel(int x, int y) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x7D, 0x2F, 0, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(CommuMember_DrawPortrait)(x + 6, y + 2, 0, 0);
    unsigned char name[16];
    name_grid::NameGrid_Encode(g_cells.buffer, name, sizeof name);
    SH_CALL(Text_DrawAt)(x + 0x3B, y + 0x19, g_cells.buffer[5] != 0 ? 2 : 0, 5, name);
    SH_CALL(Menu_DrawGreyHLine)(x + 0x3B + 12 * g_cells.cursor, y + 0x25, 0xC, 0);
    SH_CALL(Menu_DrawPieces)(x, y, move_script::At(::bof3::Img(kPanelPieces.v)), 0);
}

template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }

// The grid's frame (START's 0x801DF56C, called as COMMU02's 0x801D9C44 is,
// research 4.3): the PC's Commu_DrawTiledFrame 0x45F1A0 term for term - five
// SPRTs of page 0x1D, each through its own 8 x 8 texture window, CLUT
// Gpu_GetClut(0xB0, 0x1E1), the window put back to (0, 0, 0x100, 0x100), the
// four corner pieces 0x23, 0x26, 0x27, 0x29 - but with each window RECT in the
// packet pool, as Menu_DrawTitleBox 0x574AB0 keeps its own.
// ours: Capcom's 0x45F1A0 passes Gpu_SetDrawMode a RECT on its own stack, and
// the PC's draw mode keeps the pointer (dword +8, read at the present), so the
// windows are read after the frame has returned: at New Game the texture
// build read a window of 13064 x 130 and wrote past its surface (measured
// 2026-11-14, BOF3X_TEXPAGELOG=1: "build page 29 ... key 7492,126 13064x130",
// then an access violation in D3d_BuildPageTexture). The PC never ran that
// function in play (the renamer's entry steps that call it were cut, research
// 5.1), so nothing showed it; the PlayStation's SetDrawMode copies the window
// into the packet at the call.
void DrawGridFrame(int x, int y, unsigned w, unsigned h) {
    const int sx = static_cast<short>(x), sy = static_cast<short>(y);
    const int a = static_cast<int>(w & 0xFF), b = static_cast<int>(h & 0xFF);
    const auto mode = [](unsigned wx, unsigned wy, unsigned ww, unsigned wh) {
        unsigned char* const r = Gfx_PacketNext;
        Gfx_PacketNext = r + 8;
        Put<std::uint16_t>(r, 0, static_cast<std::uint16_t>(wx));
        Put<std::uint16_t>(r, 2, static_cast<std::uint16_t>(wy));
        Put<std::uint16_t>(r, 4, static_cast<std::uint16_t>(ww));
        Put<std::uint16_t>(r, 6, static_cast<std::uint16_t>(wh));
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1D, Low32(r));
        SH_CALL(Gfx_CommitPrim)(1, 0xC);
    };
    const auto sprite = [](int fx, int fy, int sw, int sh) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetSprt)(p);
        Put<float>(p, 8, static_cast<float>(fx));
        Put<float>(p, 0xC, static_cast<float>(fy));
        p[0x14] = 0;
        p[0x15] = 0;
        Put<std::uint16_t>(p, 0x18, static_cast<std::uint16_t>(sw));
        Put<std::uint16_t>(p, 0x1A, static_cast<std::uint16_t>(sh));
        Put<std::uint16_t>(p, 0x16, static_cast<std::uint16_t>(SH_CALL(Gpu_GetClut)(0xB0, 0x1E1)));
        p[4] = p[5] = p[6] = 0x80;
        SH_CALL(Gfx_CommitPrim)(1, 0x1C);
    };
    mode(0x70, 0x98, 8, 8);
    sprite(sx + 0x10, sy, 8 * a - 0x20, 8);
    mode(0x98, 0xA0, 8, 8);
    sprite(sx + 0x10, sy + 8 * b - 8, 8 * a - 0x20, 8);
    mode(0xB0, 0x98, 8, 8);
    sprite(sx, sy + 0x10, 8, 8 * b - 0x20);
    mode(0xB0, 0xA0, 8, 8);
    sprite(sx + 8 * a - 8, sy + 0x10, 8, 8 * b - 0x20);
    mode(0x70, 0xA0, 8, 8);
    sprite(sx + 8, sy + 8, 8 * a - 0x10, 8 * b - 0x10);
    mode(0, 0, 0x100, 0x100);
    const int right = x + 8 * a - 0x10, bottom = y + 8 * b - 0x10;
    SH_CALL(Menu_DrawPiece)(x, y, 0x23, 1);
    SH_CALL(Menu_DrawPiece)(right, y, 0x26, 1);
    SH_CALL(Menu_DrawPiece)(x, bottom, 0x27, 1);
    SH_CALL(Menu_DrawPiece)(right, bottom, 0x29, 1);
}

// The draw-all 0x801E6F80, in its order: the backdrop, the title box, system
// message 0xBA or 0xBB by bit 0x40 of the frame counter (PSX 0x80143E6C, PC
// Frame_Counter: M4), the name panel, the legend, the grid's frame
// (DrawGridFrame above), the grid; then the actor TitleFlow_Begin set up in Sprite_Current (M9): every
// 10 frames its +6 steps through 0..7 with animation +6 + 8, every frame it is
// queued and ticked.
void DrawAll() {
    SH_CALL(Menu_DrawBackdrop)(static_cast<unsigned char>(Config_Background));
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x12, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Text_DrawAt)(0x1C, 0x15, 0, 0xFF, SH_CALL(Msg_SystemPtr)((Frame_Counter & 0x40u) != 0 ? 0xBA : 0xBB));
    DrawPanel(0x20, 0x2B);
    name_grid::NameGrid_DrawLegend(Copy::kNewGame, g_cells.page, 0xA8, 0x2B);
    DrawGridFrame(0x12, 0x61, 0x24, 0x11);
    name_grid::NameGrid_Draw(Copy::kNewGame, g_cells, 0x18, 0x67);
    rv::Current<records::SpriteObject>()->step_frames = static_cast<std::uint8_t>(rv::Current<records::SpriteObject>()->step_frames + 1);
    if (rv::Current<records::SpriteObject>()->step_frames >= 10) {
        rv::Current<records::SpriteObject>()->step_frames = 0;
        rv::Current<records::SpriteObject>()->type = static_cast<std::uint8_t>((rv::Current<records::SpriteObject>()->type + 1) & 7);
        SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(rv::Current<records::SpriteObject>()->type + 8));
    }
    SH_CALL(Sprite_QueueOverlay)();
    SH_CALL(Sprite_ScriptTick)();
}

// Sub-steps 1..4 = the PlayStation's n0..n3 (its n0 also makes the records;
// ours does that at kOpen, before any screen, so a later screen in the list
// finds them made whether the name screen shows or not).
void NameStep(unsigned sub) {
    switch (sub) {
    case 0:   // n0, the open
        g_cells.cursor = 0;
        g_cells.row = 0;
        g_cells.column = 0;
        if (g_fresh) {
            std::memset(g_cells.buffer, 0xFF, 5);
            g_cells.buffer[5] = 0;
            g_cells.page = static_cast<unsigned char>(name_grid::NameGrid_FirstPage());
            g_fresh = false;
        }
        Draw_PassFlags = 1;
        DrawAll();
        SH_CALL(Transition_Start)(1);
        SaveMenu_FlowState = static_cast<unsigned char>(SaveMenu_FlowState + 1);
        return;
    case 1:   // n1, the fade in
        DrawAll();
        if (Transition_Busy == 0) SaveMenu_FlowState = static_cast<unsigned char>(SaveMenu_FlowState + 1);
        return;
    case 2: {   // n2, the input step: it draws first (research 4.6)
        DrawAll();
        // The all-blank finish's default: record 0's name as cells (DIV-0020's
        // default under a language, made by NewGame_InitCharacters at kOpen).
        // A name the grid cannot hold back gives five blanks, which leave the
        // record as it is - as does the default itself, below.
        unsigned char fallback[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        name_grid::NameGrid_Decode(RecordName(), kNameRoom, fallback);
        const unsigned r = name_grid::NameGrid_Input(Copy::kNewGame, g_cells, kNameRoom, fallback);
        if (r & name_grid::kQuit) {
            // Special code 4: back to the title menu's first sub-step, no fade
            // (sound 0x103 played by the routine); the buffer and page kept.
            Game_Step = 1;
            SaveMenu_FlowState = 0;
        }
        if (r & name_grid::kFinished) {
            // The commit: the PlayStation writes the five cells into record 0.
            // ours: the layer's bytes through NameGrid_Encode, NUL-filled to
            // the 8-byte field; the default copied back (the cells equal to
            // the record's own, decoded) leaves the record untouched, which
            // is the PlayStation's copy-back without a re-encoding (a pair
            // code would come back as its two glyphs).
            if (std::memcmp(g_cells.buffer, fallback, 5) != 0) {
                const unsigned bytes = name_grid::NameGrid_Encode(g_cells.buffer, RecordName(), kNameRoom);
                bof3::Log("NewGameNameEntry: record 0 named, %u byte(s)", bytes);
            } else {
                bof3::Log("NewGameNameEntry: record 0 keeps its default name");
            }
            SH_CALL(Transition_Start)(0);
            // Sub-step + 1, read now: after a quit in the same frame, 1 - as
            // the PlayStation's, whose quit does not leave the step either.
            SaveMenu_FlowState = static_cast<unsigned char>(SaveMenu_FlowState + 1);
        }
        return;
    }
    case 3:   // n3, the fade out: drawn while it runs; then on, with no draw
        if (Transition_Busy != 0) {
            DrawAll();
            return;
        }
        NextScreen(0);
        return;
    default:
        bof3::Fatal("New Game: the name screen has no sub-step %u", sub);
    }
}

// --- the snapshot participant (round doc section 5, "Quicksave") ----------------

constexpr std::uint32_t kSnapVersion = 1;

bool NameScreenActive() {
    return Game_Mode == 1 && Game_Step == 2 && ScreenOf(SaveMenu_FlowState) == 0;
}

struct Record {
    std::uint8_t name_on, options_on, active, family;
    Cells cells;
    std::uint8_t fresh;
};

Record Live() {
    Record r{};
    r.name_on = g_name_on;
    r.options_on = g_options_on;
    r.active = NameScreenActive() ? 1 : 0;
    r.family = name_grid::NameGrid_Available() ? static_cast<std::uint8_t>(name_grid::NameGrid_Family()) : 0xFF;
    r.cells = g_cells;
    r.fresh = g_fresh ? 1 : 0;
    return r;
}

// A take on the name screen loads only where the name screen runs as it ran:
// the switch on and a grid of the same family (another language is ST's
// refusal already). Elsewhere - the title menu, the Config screen, play -
// nothing of ours is read and any record loads.
bool Admits(const Record& r, const Record& live, std::string& why, std::string& detail) {
    if (r.active == 0) return true;
    if (live.name_on == 0) {
        why = "name screen, switch off";
        detail = "NEWGAME: the take is on the New Game name screen and NewGameNameEntry is off in this start";
        return false;
    }
    if (live.family != r.family) {
        why = "name screen, no grid";
        detail = "NEWGAME: the take is on the New Game name screen and this start has no name grid of that family";
        return false;
    }
    return true;
}

void SnapSave(bof3::SnapshotWriter& w) {
    w.Put(kSnapVersion);
    w.Put(Live());
}

bool SnapCheck(bof3::SnapshotReader& r, std::string& why, std::string& detail) {
    if (r.Left() != sizeof(std::uint32_t) + sizeof(Record) || r.Get<std::uint32_t>() != kSnapVersion) {
        why = "New Game record";
        detail = "the NEWGAME record is another version";
        return false;
    }
    return Admits(r.Get<Record>(), Live(), why, detail);
}

void SnapRestore(bof3::SnapshotReader& r) {
    if (r.Get<std::uint32_t>() != kSnapVersion) bof3::Fatal("snapshot: NEWGAME's record is another version");
    const Record rec = r.Get<Record>();
    g_cells = rec.cells;
    g_fresh = rec.fresh != 0;
}

void SelfTest() {
    int checks = 0;
    const auto check = [&checks](bool ok, const char* what) {
        if (!ok) bof3::Fatal("new_game self-test: %s", what);
        ++checks;
    };
    // The step list: sub-steps in order, disjoint, after kOpen.
    unsigned next = kOpen + 1;
    for (int i = 0; i < kScreenCount; ++i) {
        check(kScreens[i].first == next && kScreens[i].last >= kScreens[i].first, "the screens' sub-steps follow on");
        next = kScreens[i].last + 1;
    }
    check(ScreenOf(kOpen) < 0 && ScreenOf(1) == 0 && ScreenOf(4) == 0 && ScreenOf(5) < 0, "ScreenOf");
    // The snapshot rule.
    std::string why, detail;
    Record take{}, live{};
    take.active = 0;
    live.name_on = 0;
    check(Admits(take, live, why, detail), "a take off the name screen loads with the switch off");
    take.active = 1;
    take.name_on = 1;
    take.family = 1;
    check(!Admits(take, live, why, detail) && why == "name screen, switch off", "a name-screen take is refused with the switch off");
    live.name_on = 1;
    live.family = 0xFF;
    check(!Admits(take, live, why, detail) && why == "name screen, no grid", "a name-screen take is refused with no grid");
    live.family = 0;
    check(!Admits(take, live, why, detail), "a name-screen take is refused with another family");
    live.family = 1;
    check(Admits(take, live, why, detail), "a name-screen take loads where the screen runs as it ran");
    // The record's round trip.
    {
        std::vector<unsigned char> bytes;
        bof3::SnapshotWriter w(bytes);
        const Cells saved = g_cells;
        const bool fresh = g_fresh;
        g_cells = Cells{{1, 2, 3, 0xFF, 0xFF, 0}, 2, 3, 4, 1};
        g_fresh = false;
        SnapSave(w);
        g_cells = Cells{};
        g_fresh = true;
        bof3::SnapshotReader rd("NEWGAME", bytes.data(), bytes.size());
        SnapRestore(rd);
        check(g_cells.buffer[0] == 1 && g_cells.buffer[2] == 3 && g_cells.cursor == 2 && g_cells.row == 4 && g_cells.page == 1 && !g_fresh,
              "the record carries the cells");
        bof3::SnapshotReader short_rd("NEWGAME", bytes.data(), bytes.size() - 1);
        check(!SnapCheck(short_rd, why, detail), "a short record is refused");
        g_cells = saved;
        g_fresh = fresh;
    }
    bof3::Log("new_game self-test: %d checks, 0 mismatches", checks);
}

bool g_logged = false;

}  // namespace

bool NewGame_Sequence() {
    if (g_name_on == 0 && g_options_on == 0) return false;
    if (SaveMenu_FlowState == kOpen) {
        const int first = NextWanted(-1);
        if (!g_logged) {
            g_logged = true;
            if (g_name_on == 0) bof3::Log("NewGameNameEntry off: no name screen");
            else if (first < 0) bof3::Log("NewGameNameEntry: no name screen here (%s)", name_grid::NameGrid_Why());
            bof3::Log("NewGameOptions %s: %s", g_options_on ? "on" : "off",
                      g_options_on ? "the Config screen after New Game" : "the game after New Game");
        }
        if (first < 0 && g_options_on == 0) return false;   // the port's body, whole
        TitleFlow_NewGameRecords();
        if (first < 0) {
            Finish();
            return true;
        }
        SaveMenu_FlowState = static_cast<unsigned char>(kScreens[first].first);
    }
    const unsigned sub = SaveMenu_FlowState;
    const int i = ScreenOf(sub);
    if (i < 0) bof3::Fatal("New Game: sub-step %u is no screen's", sub);
    if (!kScreens[i].wanted())
        bof3::Fatal("New Game: sub-step %u of %s, which is not on in this start (a quickload across the switch is refused)", sub,
                    kScreens[i].name);
    kScreens[i].step(sub - kScreens[i].first);
    SH_CALL(Field_DrawFrame)();   // after every sub-step, as START's step 0x801E44BC
    return true;
}

void NewGame_TitleBegins() { g_fresh = true; }

void Arm() {
    if (bof3::WantsShadow("new_game")) SelfTest();
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("NewGameNameEntry", Low32(&g_name_on), &was, &is, 1);
        bof3::PatchBytes("NewGameOptions", Low32(&g_options_on), &was, &is, 1);
    }
    bof3::Log("New Game: name entry %s (%s), options %s", g_name_on ? "on" : "off",
              name_grid::NameGrid_Available() ? "a grid is taken" : name_grid::NameGrid_Why(), g_options_on ? "on" : "off");
    bof3::Snapshot_Register("NEWGAME", &SnapSave, &SnapRestore, nullptr, &SnapCheck);
}

}  // namespace new_game

#include "input/button_tests.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

// docs/button-tests.md section 4. The sites' masks below are Japan's bits as
// the original tests them (the addresses are the original functions'); no
// game data.

namespace bof3x::input::button_tests {
namespace {

namespace sg = bof3x::input::sigma;
using U16 = std::uint16_t;
using U = std::uint32_t;

// Every site group HT changed: the original function and the Japan mask it
// tests (one row per distinct mask in the function; docs/button-tests.md 2
// has the file:line of each).
struct Site {
    const char* name;
    U address;
    U16 mask;
};
constexpr Site kSites[] = {
    // the fishing (stage 1, the reel, the catch, the panel, the look)
    {"LeaderPanel_S1Buttons", 0x528A90, 0x0074},
    {"LeaderPanel_S1Buttons", 0x528A90, 0x0004},
    {"LeaderPanel_S1Buttons", 0x528A90, 0x0010},
    {"LeaderPanel_S1Buttons", 0x528A90, 0x0020},
    {"LeaderPanel_S1Buttons", 0x528A90, 0x0040},
    {"LeaderPanel_S1Choose", 0x528CD0, 0x0060},
    {"LeaderPanel_S1Choose", 0x528CD0, 0x0040},
    {"LeaderPanel_S2Steer", 0x529070, 0x0020},
    {"LeaderPanel_S3Run", 0x5291D0, 0x0040},
    {"LeaderPanel_S3Run", 0x5291D0, 0x0020},
    {"LeaderPanel_S4Run", 0x5295F0, 0x0020},
    {"LeaderPanel_S4Count", 0x529AE0, 0x0020},
    {"LeaderPanel_S4Take", 0x529C70, 0x0060},
    {"LeaderPanel_S4ResultWait", 0x529E70, 0x0060},
    {"LeaderPanel_S4Again", 0x529FA0, 0x0060},
    {"LeaderPanel_S4Again", 0x529FA0, 0x0020},
    {"LeaderPanel_Leave", 0x52A280, 0x0060},
    {"LeaderPanel_S10Look", 0x52AF80, 0x0040},
    {"LeaderPanel_PressLatch", 0x52B460, 0xE020},
    // the field minigames
    {"Area75_PlayerPress", 0x40E520, 0x0020},      // the rope pull
    {"Area75_EarlyPress", 0x40E5A0, 0x0020},
    {"Area135_HeldMove1", 0x41DEA0, 0x0020},       // the crane
    {"Area135_HeldMove7", 0x41DF00, 0x0020},
    {"Area135_Tail18", 0x41E630, 0x0020},
    {"EffectKind8C_WaitA", 0x48ACA0, 0x0040},      // the well (kind 0x8C, area 68)
    {"EffectKind8C_PressA", 0x48AD10, 0x0040},
    {"EffectKind8C_WaitB", 0x48AD50, 0x0040},
    {"EffectKind8C_Ready", 0x48B070, 0x0020},
    {"EffectKind8C_CountA", 0x48B090, 0x0020},
    {"EffectKind8C_CountPress", 0x48B100, 0x0020},
    {"EffectKind8C_CountB", 0x48B140, 0x0020},
    {"Scena01_Scene06", 0x53AC30, 0x0040},         // the chop (area 8)
    {"Scena13_Run5", 0x563090, 0x0020},            // the dial (the antenna)
    {"Scena17_Outro08", 0x56C9F0, 0x0020},         // the ending's skip
    {"Scena17_Outro09", 0x56CA60, 0x0020},
    // the dock's parts menu
    {"EffectKind5E_Pick", 0x47F610, 0x0010},
    {"EffectKind5E_Pick", 0x47F610, 0x0040},
    {"EffectKind5E_Pick", 0x47F610, 0x0020},
    // the item lists: the repeat mask, then L1 / R1 on the repeat
    {"BattleExtra_EquipListInput", 0x42DE50, 0xF00C},
    {"BattleItem_Browse", 0x4474B0, 0xF00C},
    {"BattleItemCmd_List", 0x448210, 0xF00C},
    {"BattleItemCmd_EquipItemPick", 0x448FC0, 0xF00C},
    {"BattleItemCmd_EquipItemPick", 0x448FC0, 0x500C},
    {"DragonCmd_SlotsCursor", 0x450340, 0x500C},
    {"DragonCmd_SlotsCursor", 0x450340, 0x0010},
    {"DragonCmd_Slots2Cursor", 0x450BE0, 0x500C},
    {"DragonCmd_StoreCursor", 0x450FC0, 0x500C},
    {"DragonCmd_StoreSource", 0x451290, 0x500C},
    {"DragonCmd_StoreSource", 0x451290, 0x0010},
    {"CommuItem_Choose", 0x45FE90, 0xF00C},
    {"ShopTrade_SellList", 0x5827F0, 0xF00C},
    {"ShopBrowse_Choose", 0x5833E0, 0xF00C},
    {"SharedList_PickShared", 0x5849B0, 0x500C},
    {"SharedList_Arrange", 0x584D30, 0x500C},
    {"FieldMenuItems_List", 0x58AD30, 0xF00C},
    {"FieldItems_ArrangeCategory", 0x58B1D0, 0xF00C},
    {"FieldItems_ArrangeMove", 0x58B4B0, 0x500C},
    {"FieldItems_ViewList32", 0x58BA40, 0x500C},
    {"Equip_ChooseItem", 0x58CAE0, 0xF00C},
    {"FieldAbility_ArrangeMember", 0x58E420, 0xF00C},
    {"FieldAbility_ArrangeMove", 0x58E7E0, 0x500C},
    {"FieldAbility_ViewBrowse", 0x58EB00, 0x500C},
    {"(the lists' L1 test)", 0, 0x0004},
    {"(the lists' R1 test)", 0, 0x0008},
    // group HT-2 (the owner's decisions of 2026-10-17, docs/button-tests.md 3)
    {"MsgBox_StatePrint", 0x497B30, 0x0020},          // the held speed-up (0x497DC6)
    {"CommuMusic_Browse", 0x45F830, 0x0020},          // the music room: play / stop
    {"CommuMusic_Browse", 0x45F830, 0x0040},          // close
    {"BattleMenu_CommandSelect", 0x42EAD0, 0xF00C},   // the command cross: anything held
};

// The command cross's six masks, read from Battle_CommandPadMasks in place
// (the table is Capcom's and stays as it is; ours asks each through sigma).
// kPadMasks: now Battle_CommandPadMasks (symbols.toml).
constexpr unsigned kNPadMasks = 6;
U16 PadMask(unsigned k) {
    return *reinterpret_cast<const U16*>(static_cast<std::uintptr_t>(Battle_CommandPadMasks_addr + 2 * k));
}

unsigned g_bad = 0;
void Bad(const char* what, const char* site, unsigned a, unsigned b) {
    if (g_bad < 20) bof3::Log("shadow      button_tests MISMATCH: %s at %s (0x%X, 0x%X)", what, site, a, b);
    ++g_bad;
}

// The stand-in swap, by hand: Speak <-> Action moves circle <-> cross, View
// <-> Change moves R1 <-> L1; nothing else moves.
U16 Swap(U16 m) {
    U16 r = static_cast<U16>(m & ~0x6Cu);
    if (m & 0x20) r |= 0x40;
    if (m & 0x40) r |= 0x20;
    if (m & 0x04) r |= 0x08;
    if (m & 0x08) r |= 0x04;
    return r;
}

// Release R's map over a mask, bit by bit.
U16 MapMask(sg::Release r, U16 m) {
    U16 out = 0;
    for (unsigned b = 0; b < 16; ++b)
        if (m & (1u << b)) out = static_cast<U16>(out | sg::ReleaseMap(r, static_cast<U16>(1u << b)));
    return out;
}

void SwappedWords(U16 w[9]) {
    sg::DefaultWords(sg::Release::kJapan, false, w);
    w[0] = static_cast<U16>((w[0] & sg::kL2R2) | 0x40);   // Speak on cross
    w[6] = 0x20;                                          // Action on circle
    w[3] = 0x04;                                          // View on L1
    w[4] = 0x08;                                          // Change on R1
    w[7] = w[0];
    w[8] = w[6];
}

// Part 1: the masks under stand-in words.
unsigned PureMasks() {
    unsigned n = 0;
    const sg::Release releases[] = {sg::Release::kJapan, sg::Release::kWest, sg::Release::kPspEu};
    for (sg::Release r : releases) {
        U16 w[9];
        sg::DefaultWords(r, false, w);
        std::string why;
        const sg::Table t = sg::Build(w, why);
        if (!t.valid) bof3::Fatal("button_tests: %s's defaults refused (%s)", sg::ReleaseName(r), why.c_str());
        for (const Site& s : kSites) {
            const U16 got = sg::Apply(t, s.mask);
            if (r == sg::Release::kJapan && got != s.mask) Bad("Japan's defaults moved the mask", s.name, s.mask, got);
            if (got != MapMask(r, s.mask)) Bad("a release's defaults do not give its own map", s.name, s.mask, got);
            ++n;
        }
        for (unsigned k = 0; k < kNPadMasks; ++k) {
            const U16 m = PadMask(k);
            const U16 got = sg::Apply(t, m);
            if (r == sg::Release::kJapan && got != m) Bad("Japan's defaults moved a pad mask", "Battle_CommandPadMasks", m, got);
            if (got != MapMask(r, m)) Bad("a pad mask not the release's map", "Battle_CommandPadMasks", m, got);
            ++n;
        }
        // The text's speed-up is word 0's button with L2 / R2 set aside: the
        // Speak line's button, never L2 or R2 (word 0 also holds those).
        const U16 speak = sg::Apply(t, 0x20);
        if (speak != (w[0] & sg::kSixBits)) Bad("the speed-up is not word 0's button", "MsgBox_StatePrint", w[0], speak);
        if (speak & sg::kL2R2) Bad("the speed-up takes L2 / R2", "MsgBox_StatePrint", w[0], speak);
        ++n;
    }
    U16 w[9];
    SwappedWords(w);
    std::string why;
    const sg::Table t = sg::Build(w, why);
    if (!t.valid) bof3::Fatal("button_tests: the stand-in swap refused (%s)", why.c_str());
    for (const Site& s : kSites) {
        const U16 got = sg::Apply(t, s.mask);
        if (got != Swap(s.mask)) Bad("the swap's mask", s.name, s.mask, got);
        ++n;
    }
    for (unsigned k = 0; k < kNPadMasks; ++k) {
        const U16 m = PadMask(k);
        const U16 got = sg::Apply(t, m);
        if (got != Swap(m)) Bad("the swap's pad mask", "Battle_CommandPadMasks", m, got);
        ++n;
    }
    // LeaderPanel_S1Buttons's exact match, its four bits one by one: a press
    // of the swapped button lands on Japan's case; a press of two lands on none.
    for (U16 press = 0; press < 0x100; ++press) {
        const U16 back = sg::ApplyInverse(t, static_cast<U16>(press & sg::Apply(t, 0x74)));
        const U16 want = static_cast<U16>(Swap(press) & 0x74);
        if (back != want) Bad("S1Buttons' case under the swap", "LeaderPanel_S1Buttons", press, back);
        ++n;
    }
    return n;
}

// Part 2: four of ours under the live table.
U16* Words() { return reinterpret_cast<U16*>(static_cast<std::uintptr_t>(0x903580)); }
unsigned char& B(U address) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U16& W(U address) { return *reinterpret_cast<U16*>(static_cast<std::uintptr_t>(address)); }

struct Cell { U address, size; };
constexpr Cell kCells[] = {
    {0x903580, 18},   // the nine pad words
    {0x7E1BEC, 2},    // Input_Pressed
    {0x937F88, 4},    // Sprite_Current
    {0x7E1261, 1},    // Effect_Objects record 1 +1
    {0x7E1361, 1},    // record 3 +1
    {0x90412E, 1},    // the fishing's item bytes
    {0x904130, 1},
    {0x6BC708, 2},    // LeaderPanel_PressLatch's two, LeaderPanel_S1Buttons' warned byte
    {0x6BC717, 1},
    {0x676284, 1},    // kind 0x8C's frame count
    {0x67628C, 2},    // its count
    {0x7E1BE8, 2},    // Input_Held
    {0x904AA1, 2},    // the battle's step and sub-step
    {0x904AA5, 2},    // the command cross's tap timer and tap command
    {0x904AB4, 1},    // the command the cross points at
    {0x904AC3, 1},    // the menu index
};
struct Saved { unsigned char bytes[64]; };

void SaveCells(Saved& s) {
    unsigned at = 0;
    for (const Cell& c : kCells) {
        std::memcpy(s.bytes + at, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(c.address)), c.size);
        at += c.size;
    }
}
void RestoreCells(const Saved& s) {
    unsigned at = 0;
    for (const Cell& c : kCells) {
        std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(c.address)), s.bytes + at, c.size);
        at += c.size;
    }
}

unsigned char g_scratch[0x200];

// One run of LeaderPanel_S1Buttons with the item byte 0 (the cast's path that
// calls nothing): the leader's +2 / +3 and records 1 and 3.
U RunS1(U16 press) {
    std::memset(g_scratch, 0, sizeof g_scratch);
    g_scratch[2] = 0x33;
    g_scratch[3] = 0x44;
    Sprite_Current = g_scratch;
    B(0x7E1261) = 0x55;
    B(0x7E1361) = 0x66;
    B(0x90412E) = 0;
    B(0x904130) = 1;
    B(0x6BC709) = 0;
    Input_Pressed = press;
    ::LeaderPanel_S1Buttons();
    return static_cast<U>(g_scratch[2]) | static_cast<U>(g_scratch[3]) << 8 | static_cast<U>(B(0x7E1261)) << 16 |
           static_cast<U>(B(0x7E1361)) << 24;
}
U RunLatch(U16 press) {
    B(0x6BC717) = 0x5A;
    B(0x6BC708) = 0;
    Input_Pressed = press;
    ::LeaderPanel_PressLatch();
    return static_cast<U>(B(0x6BC708)) | static_cast<U>(B(0x6BC717)) << 8;
}
U RunReady(U16 press) {
    std::memset(g_scratch, 0, sizeof g_scratch);
    g_scratch[1] = 0xF;
    Sprite_Current = g_scratch;
    W(0x67628C) = 5;
    Input_Pressed = press;
    ::EffectKind8C_Ready();
    return static_cast<U>(g_scratch[1]) | static_cast<U>(W(0x67628C)) << 8;
}
U RunCountPress(U16 press) {
    std::memset(g_scratch, 0, sizeof g_scratch);
    g_scratch[1] = 0x11;
    Sprite_Current = g_scratch;
    W(0x67628C) = 5;
    B(0x676284) = 3;
    Input_Pressed = press;
    ::EffectKind8C_CountPress();
    return static_cast<U>(g_scratch[1]) | static_cast<U>(W(0x67628C)) << 8 | static_cast<U>(B(0x676284)) << 24;
}

// BattleMenu_CommandSelect with the press held and nothing pressed this frame:
// the path that calls nothing (no cancel, no confirm, no tap), which leaves the
// cross on the command of the lowest of the six masks held (1..6, 0 none).
// Select (0x100) is taken off the held word: it opens the status and draws.
U RunCross(U16 press) {
    B(0x904AA5) = 0;      // the tap timer
    B(0x904AA6) = 0;
    B(0x904AB4) = 0x77;   // the cross's command, a stand-in
    B(0x904AA1) = 0x31;
    B(0x904AA2) = 0x42;
    B(0x904AC3) = 1;
    W(0x7E1BE8) = static_cast<U16>(press & ~0x100u);
    Input_Pressed = 0;
    ::BattleMenu_CommandSelect();
    return static_cast<U>(B(0x904AB4)) | static_cast<U>(B(0x904AA5)) << 8 | static_cast<U>(B(0x904AA6)) << 16 |
           static_cast<U>(B(0x904AA1) ^ B(0x904AA2) ^ B(0x904AC3)) << 24;
}

struct Live {
    const char* name;
    U (*run)(U16);
};
constexpr Live kLive[] = {
    {"LeaderPanel_S1Buttons", &RunS1},
    {"LeaderPanel_PressLatch", &RunLatch},
    {"EffectKind8C_Ready", &RunReady},
    {"EffectKind8C_CountPress", &RunCountPress},
    {"BattleMenu_CommandSelect", &RunCross},
};
// The presses: none, each single bit, and pairs of face / shoulder buttons.
constexpr U16 kPresses[] = {0,      0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100,
                            0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000, 0x0024, 0x0060, 0x0050,
                            0x0014, 0x000C, 0x0048, 0x00A0, 0x2020, 0x8040};
constexpr unsigned kNPresses = sizeof kPresses / sizeof kPresses[0];
constexpr unsigned kNLive = sizeof kLive / sizeof kLive[0];

// Rebuild the live table from stand-in words.
void UseWords(const U16 w[9]) {
    std::memcpy(Words(), w, 18);
    sg::Rebound();
}

unsigned LiveRuns(bool& ran) {
    ran = false;
    Saved saved;
    SaveCells(saved);
    U16 jp[9], sw[9];
    sg::DefaultWords(sg::Release::kJapan, false, jp);
    SwappedWords(sw);
    UseWords(sw);
    if (sg::Sigma(0x20) != 0x40) {
        // ButtonSigma is off (BOF3X_ORIGINAL=ButtonSigma): sigma is the
        // identity, there is no live table to stand in.
        RestoreCells(saved);
        return 0;
    }
    ran = true;
    static U identity[kNLive][kNPresses];
    UseWords(jp);
    if (sg::Sigma(0x74) != 0x74) Bad("Japan's words: sigma not the identity", "the live table", 0x74, sg::Sigma(0x74));
    for (unsigned f = 0; f < kNLive; ++f)
        for (unsigned p = 0; p < kNPresses; ++p) identity[f][p] = kLive[f].run(kPresses[p]);
    unsigned n = 0;
    // The checks have teeth: under Japan's words circle and cross differ at
    // S1Buttons and kind 0x8C.
    if (identity[0][6] == identity[0][7]) Bad("S1Buttons: circle and cross alike", "LeaderPanel_S1Buttons", 0x20, 0x40);
    if (identity[2][6] == identity[2][7]) Bad("8C_Ready: circle and cross alike", "EffectKind8C_Ready", 0x20, 0x40);
    // ... and L1 and R1 differ at the command cross (kPresses[3] L1, [4] R1),
    // each on a command, as the d-pad's up (kPresses[13]) is.
    if (identity[4][3] == identity[4][4]) Bad("the cross: L1 and R1 alike", "BattleMenu_CommandSelect", 4, 8);
    if ((identity[4][3] & 0xFF) == 0 || (identity[4][4] & 0xFF) == 0 || (identity[4][13] & 0xFF) == 0)
        Bad("the cross: a direction on no command", "BattleMenu_CommandSelect", identity[4][3], identity[4][4]);
    // Under Japan's words the cross does what the original's loop does with
    // Capcom's table as it is: the lowest k whose mask is held gives k + 1,
    // nothing held of 0xF00C gives 0.
    for (unsigned p = 0; p < kNPresses; ++p) {
        const U16 held = static_cast<U16>(kPresses[p] & ~0x100u);
        unsigned want = 0;
        if (held & 0xF00C)
            for (int k = static_cast<int>(kNPadMasks) - 1; k >= 0; --k)
                if (PadMask(static_cast<unsigned>(k)) & held) want = static_cast<unsigned>(k) + 1;
        if ((held & 0xF00C) != 0 && want == 0) want = 0x77;   // held but no mask: the stand-in left
        if ((identity[4][p] & 0xFF) != want) Bad("the cross under Japan's words", "BattleMenu_CommandSelect", held, identity[4][p]);
        ++n;
    }
    UseWords(sw);
    for (unsigned f = 0; f < kNLive; ++f)
        for (unsigned p = 0; p < kNPresses; ++p) {
            // The swapped button under the swap does what Japan's does under
            // Japan's words ...
            const U got = kLive[f].run(Swap(kPresses[p]));
            if (got != identity[f][p]) Bad("the swapped press under the swap", kLive[f].name, kPresses[p], got);
            ++n;
        }
    for (unsigned f = 0; f < kNLive; ++f)
        for (unsigned p = 0; p < kNPresses; ++p) {
            // ... and Japan's own button under the swap does what the swapped
            // one does under Japan's (circle now casts nothing a cross would not).
            const U got = kLive[f].run(kPresses[p]);
            unsigned q = 0;
            while (q < kNPresses && kPresses[q] != Swap(kPresses[p])) ++q;
            if (q == kNPresses) continue;
            if (got != identity[f][q]) Bad("Japan's press under the swap", kLive[f].name, kPresses[p], got);
            ++n;
        }
    UseWords(jp);   // the table left as Japan's (the identity) until Boot_Task's writer rebuilds it
    RestoreCells(saved);
    return n;
}

}  // namespace

void Check() {
    g_bad = 0;
    const unsigned pure = PureMasks();
    bool ran = false;
    const unsigned live = LiveRuns(ran);
    if (ran)
        bof3::Log("shadow      button_tests: %u masks under stand-in words (3 releases' defaults and the swap), %u runs of "
                  "5 of ours under the live table (Japan's words and the swap); %u MISMATCHES",
                  pure, live, g_bad);
    else
        bof3::Log("shadow      button_tests: %u masks under stand-in words; the live half skipped (ButtonSigma off: sigma "
                  "is the identity); %u MISMATCHES",
                  pure, g_bad);
    if (g_bad) bof3::Fatal("button_tests: %u mismatches against docs/button-tests.md", g_bad);
}

}  // namespace bof3x::input::button_tests

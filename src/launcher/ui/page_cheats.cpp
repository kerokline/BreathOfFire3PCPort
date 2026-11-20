// Game Options, Cheats, two pages (launcher.rc IDD_PAGE_GAME, IDD_PAGE_GAME2;
// DIV-0045, DIV-0046, DIV-0187, DIV-0188, DIV-0226; docs/cheats.md).
// config_dialog.cpp's kCheatSliders, CheatsPopulate, CheatsReadBack and
// CheatsSliderMoved, less the windows. The texts are the Win32 pages', word
// for word.
//
// The pages are read back only once touched (DialogState::cheats_touched): an
// ini that holds a multiplier over the cap is shown as the cap, and written
// back as it was read while no control of the two pages was used.
#include "launcher/ui/pages.h"

#include "launcher/ui/ui.h"

#include <string>

namespace bof3x::ui {

namespace {

constexpr int kCheatMultiplierMax = 10;   // the DLL clamps more to this (src/game/cheats.cpp)

struct Boxes {
    int exp = 1, zenny = 1;
    bool steal = false, enemy_info = false, no_fail = false, learn = false;
    bool touched = false;
};
Boxes g_boxes;

int Capped(int v) { return v > kCheatMultiplierMax ? kCheatMultiplierMax : v; }

// CheatsPopulate: the boxes from `ch`, each multiplier shown as the cap at most.
void FillBoxes(const Config::Cheats& ch) {
    Boxes& b = g_boxes;
    b.exp = Capped(ch.exp);
    b.zenny = Capped(ch.zenny);
    b.steal = ch.steal;
    b.enemy_info = ch.enemy_info;
    b.no_fail = ch.no_fail;
    b.learn = ch.learn;
}

// Either page's Defaults: every cheat, both pages.
void DefaultsButton() {
    Place(0, Dy(114));
    Size(Dx(52), 23);
    if (Button("Defaults")) {
        FillBoxes(Config::Cheats{});
        g_boxes.touched = true;
    }
}

// One multiplier's row: its name and the slider, which shows its value.
void MultiplierRow(const char* text, const char* name, int* value, float y) {
    Place(Dx(7), Dy(y + 3));
    Label(text);
    Place(Dx(99), Dy(y));
    Size(Dx(150));
    if (Slider(name, value, 0, kCheatMultiplierMax)) g_boxes.touched = true;
    // No number beside the slider (the Win32 page's value text at 253): the slider shows it (the owner, 2026-11-20).
}

}  // namespace

void CheatsOpen(Pages& p) {
    g_boxes = Boxes{};
    FillBoxes(p.cfg->cheats);
}

// CheatsReadBack, only once the pages were touched.
void CheatsApply(Pages& p) {
    const Boxes& b = g_boxes;
    if (!b.touched) return;
    Config::Cheats& ch = p.cfg->cheats;
    ch.exp = b.exp < 0 ? 0 : Capped(b.exp);
    ch.zenny = b.zenny < 0 ? 0 : Capped(b.zenny);
    ch.steal = b.steal;
    ch.enemy_info = b.enemy_info;
    ch.no_fail = b.no_fail;
    ch.learn = b.learn;
}

// Page 1: the battle rewards and stealing.
void CheatsPage1(Pages&) {
    Boxes& b = g_boxes;
    GroupBegin("Battle rewards", 0, 0, Dx(286), Dy(64));
    MultiplierRow("EXP multiplier", "exp", &b.exp, 11);
    MultiplierRow("Zenny multiplier", "zenny", &b.zenny, 27);
    Place(Dx(7), Dy(47));
    Size(Dx(276));
    Label("1 is normal. 0 gives no EXP or Zenny (no level-ups). Applies when each enemy is defeated.");
    GroupEnd();

    GroupBegin("Stealing", 0, Dy(69), Dx(286), Dy(42));
    Place(Dx(7), Dy(81 - 69));
    if (Check("Steal always succeeds##steal", &b.steal)) b.touched = true;
    Place(Dx(19), Dy(93 - 69));
    Size(Dx(260));
    Label("Pilfer and Steal always take the enemy's item on the first try, if the enemy has an item.");
    GroupEnd();

    DefaultsButton();
}

// Page 2: the enemy information, the minigames and the enemy skills.
void CheatsPage2(Pages&) {
    Boxes& b = g_boxes;
    GroupBegin("Enemy information", 0, 0, Dx(286), Dy(42));
    Place(Dx(7), Dy(12));
    if (Check("Show every enemy's HP bar and stats##info", &b.enemy_info)) b.touched = true;
    Place(Dx(19), Dy(24));
    Size(Dx(260));
    Label("The HP bar and the Identify stats show for every enemy, bosses too, before you defeat that kind.");
    GroupEnd();

    GroupBegin("Minigames", 0, Dy(44), Dx(286), Dy(42));
    Place(Dx(7), Dy(56 - 44));
    if (Check("You cannot fail a minigame##nofail", &b.no_fail)) b.touched = true;
    Place(Dx(19), Dy(68 - 44));
    Size(Dx(260));
    Label("Every minigame ends in a win. A minigame with a score gives its best prize.");
    GroupEnd();

    // The third group has no sentence under its box (launcher.rc: no room).
    GroupBegin("Enemy skills", 0, Dy(88), Dx(286), Dy(24));
    Place(Dx(7), Dy(99 - 88));
    if (Check("Examine always learns the skill that the enemy uses##learn", &b.learn)) b.touched = true;
    GroupEnd();

    DefaultsButton();
}

}  // namespace bof3x::ui

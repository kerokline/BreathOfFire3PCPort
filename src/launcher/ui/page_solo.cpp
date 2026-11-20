// Game Options, Solo character (launcher.rc IDD_PAGE_SOLO;
// docs/solo-character-round.md 4 and 6a). config_dialog.cpp's kSoloNone,
// kSoloKept, SoloRefresh, SoloPopulate and SoloReadBack, less the windows.
// The texts are the Win32 page's, word for word.
//
// Two boxes that stand for two behaviours in on= (SoloCharacter, and
// SoloChampionIfPresent), and the champion, solo.champion=. Each box is
// written back through ConfigBehaviourEdit, which leaves on= byte for byte
// when the box says what the line said. The champion box: None, the six
// champions, and - only when the ini names a value that is no champion - that
// value as it is, so an untouched box writes it back for the game to refuse.
// The champion and the option are grey while the mode's box is not ticked
// (ConfigSoloState says why); every control is shown, greyed or not.
#include "launcher/ui/pages.h"

#include "launcher/ui/ui.h"

#include <iterator>

namespace bof3x::ui {

namespace {

constexpr int kSoloNone = 0;   // the champion box's first entry; the champions follow it
constexpr int kSoloKept = kSoloNone + 1 + static_cast<int>(std::size(kSoloChampions));   // the ini's other value

struct Boxes {
    bool mode = false, if_present = false;
    std::vector<std::string> champions;
    int champion = kSoloNone;
};
Boxes g_boxes;

}  // namespace

void SoloOpen(Pages& p) {
    const Config& cfg = *p.cfg;
    Boxes& b = g_boxes;
    b = Boxes{};
    const SoloState s = ConfigSoloState(cfg, false);
    b.mode = s.mode;
    b.if_present = s.if_present;
    b.champions.push_back("None");
    for (const SoloChampion& c : kSoloChampions) b.champions.push_back(PagesText(c.label));
    const int at = ConfigSoloChampionIndex(cfg.solo_champion);
    if (at < 0 && !cfg.solo_champion.empty())
        b.champions.push_back(PagesText(ConfigFromUtf8(cfg.solo_champion) + L" - bof3x.ini: this is not a champion"));
    b.champion = at >= 0 ? kSoloNone + 1 + at : cfg.solo_champion.empty() ? kSoloNone : kSoloKept;
}

// SoloReadBack: an untouched page writes on= and solo.champion= back as read.
void SoloApply(Pages& p) {
    Config& cfg = *p.cfg;
    const Boxes& b = g_boxes;
    cfg.on = ConfigBehaviourEdit(cfg.on, kSoloBehaviour, b.mode);
    cfg.on = ConfigBehaviourEdit(cfg.on, kSoloIfPresentBehaviour, b.if_present);
    if (b.champion == kSoloNone) cfg.solo_champion.clear();
    else if (b.champion > kSoloNone && b.champion < kSoloKept)
        cfg.solo_champion = kSoloChampions[b.champion - kSoloNone - 1].id;
    // kSoloKept: the ini's value as read
}

void SoloPage(Pages& p) {
    Boxes& b = g_boxes;
    // SoloRefresh, each frame: the champion and the option follow the mode's box.
    const SoloState s = ConfigSoloState(*p.cfg, b.mode);
    const std::string why = PagesText(s.why);

    GroupBegin("Solo character", 0, 0, Dx(286), Dy(158));
    Place(Dx(7), Dy(12));
    if (!s.mode_enabled) Grey(why.c_str());
    Check("Solo Character Run - Chosen character fights all battles alone##mode", &b.mode);
    Place(Dx(19), Dy(24));
    Size(Dx(262));
    Label("Rule does not start until after reaching Cedar Woods.");

    Place(Dx(19), Dy(47));
    if (!s.choices_enabled) Grey(why.c_str());
    Label("Character:");
    Place(Dx(71), Dy(45));
    Size(Dx(110));
    if (!s.choices_enabled) Grey(why.c_str());
    Combo("champion", &b.champion, b.champions);
    Place(Dx(19), Dy(63));
    Size(Dx(262));
    if (!s.choices_enabled) Grey(why.c_str());
    Label("Character choice is written on a new game or first save. After that, a change here does not alter it.");

    Place(Dx(19), Dy(94));
    if (!s.if_present_enabled) Grey(why.c_str());
    Check("The chosen character only fights when they are in the party.##ifpresent", &b.if_present);
    Place(Dx(31), Dy(106));
    Size(Dx(250));
    if (!s.if_present_enabled) Grey(why.c_str());
    Label("If they are not, the party fights.");

    Place(Dx(7), Dy(137));
    Size(Dx(274));
    Label(why.c_str());
    GroupEnd();
}

}  // namespace bof3x::ui

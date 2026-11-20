// Mods, Extras, Page 1: the launcher's extras (launcher.rc IDD_PAGE_EXTRA;
// docs/launcher-settings.md 2a). Four toggles, each with its sentence. The
// texts are the Win32 page's, word for word.
#include "launcher/ui/pages.h"

#include "launcher/ui/ui.h"

namespace bof3x::ui {

namespace {

struct Boxes {
    bool text_faster = false, unlearned_blue = false, desert_faerie = false;
};
Boxes g_boxes;

}  // namespace

void ExtrasOpen(Pages& p) {
    const Config& cfg = *p.cfg;
    p.ruby_ui = cfg.ruby_ui;
    g_boxes.text_faster = cfg.text_faster;
    g_boxes.unlearned_blue = cfg.unlearned_blue;
    g_boxes.desert_faerie = cfg.desert_faerie;
}

void ExtrasApply(Pages& p) {
    Config& cfg = *p.cfg;
    cfg.ruby_ui = p.ruby_ui;
    cfg.ruby_ui_named = true;
    cfg.text_faster = g_boxes.text_faster;
    cfg.unlearned_blue = g_boxes.unlearned_blue;
    cfg.desert_faerie = g_boxes.desert_faerie;
}

void ExtrasPage(Pages& p) {
    GroupBegin("Extras", 0, 0, Dx(336), Dy(162));
    Place(Dx(7), Dy(14));
    Check("Show the Row above the text box on Settings (furigana or a second language)", &p.ruby_ui);
    Place(Dx(19), Dy(27));
    Size(Dx(312));
    Label("The row is a second line of text above the game text: furigana from a mod, or a different language of this install. When you add a furigana mod, this box turns on. Off: only the box is hidden. The game still shows the row that you selected.");
    Place(Dx(7), Dy(63));
    Check("Faster text: Msg Speed Med. runs at Fast speed, and Fast shows each page at once", &g_boxes.text_faster);
    Place(Dx(19), Dy(76));
    Size(Dx(312));
    Label("For text boxes outside battle. Battle messages already show at once. The pauses in the text stay. Slow does not change.");
    Place(Dx(7), Dy(97));
    Check("Show the name of a skill that you can learn in blue", &g_boxes.unlearned_blue);
    Place(Dx(19), Dy(110));
    Size(Dx(312));
    Label("In battle only. When an enemy uses a skill that the party can learn and does not know, the name of the skill shows in blue. Menus do not change.");
    Place(Dx(7), Dy(131));
    Check("Add a faerie village entrance on the desert map, near Caer Xhan", &g_boxes.desert_faerie);
    Place(Dx(19), Dy(144));
    Size(Dx(312));
    Label("A ring of flowers on the sand shows the entrance, as on the other maps. When you leave the village, you go back to the desert map.");
    GroupEnd();
}

}  // namespace bof3x::ui

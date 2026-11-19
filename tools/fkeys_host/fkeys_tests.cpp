// The function keys' table, on the host (CMakeLists.txt says how to build
// it). Each check prints only when it fails; the last line counts them.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "input/bindings.h"
#include "input/function_keys.h"

namespace in = bof3x::input;

namespace {

int g_checks = 0, g_failures = 0;

void Check(bool ok, const char* what, const std::string& got = std::string()) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("FAIL %s%s%s\n", what, got.empty() ? "" : ": ", got.c_str());
}

constexpr int A(in::FnAction a) { return static_cast<int>(a); }

// lParam as Windows gives a key going down: one press, the scan code, the extended flag.
std::int32_t Msg(int scan, bool extended, bool repeat = false) {
    return 1 | (scan << 16) | (extended ? (1 << 24) : 0) | (repeat ? (1 << 30) : 0);
}

void TestTable() {
    Check(in::kFnActionCount == 9, "nine actions");
    const in::FnBindings d = in::FnBindings::Defaults();
    const int want[9] = {0x3B, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x57, 0x58};   // F1 F4 F5 F6 F7 F8 F9 F11 F12
    for (int a = 0; a < 9; ++a) {
        Check(d.key[a] == want[a], "a default key", in::FnActions()[a].name);
        Check(d.pad[a] == -1, "no default pad input", in::FnActions()[a].name);
        Check(in::FnActions()[a].action == static_cast<in::FnAction>(a), "the table in FnAction's order");
        Check(in::FnActionFromName(in::FnActions()[a].name) == a, "an action's name read back");
        Check(std::string(in::FnActions()[a].label).size() <= 34, "a label fits its 141-unit column",
              in::FnActions()[a].label);
        Check(std::string(in::FnActions()[a].note).size() <= 13, "a note fits its 53-unit column", in::FnActions()[a].note);
    }
    const char* names[9] = {"F1", "F4", "F5", "F6", "F7", "F8", "F9", "F11", "F12"};
    for (int a = 0; a < 9; ++a) Check(in::KeyName(d.key[a]) == names[a], "a default key's name", in::KeyName(d.key[a]));
    for (int n = 1; n <= 12; ++n) Check(in::KeyName(in::FnKeyOfF(n)) == "F" + std::to_string(n), "FnKeyOfF names F n");
    Check(in::FnKeyOfF(0) == -1 && in::FnKeyOfF(13) == -1, "FnKeyOfF outside 1..12");
    Check(in::FnActionFromName("QUICKSAVE") == A(in::FnAction::kQuicksave), "an action's name in any case");
    Check(in::FnActionFromName("f6") == -1 && in::FnActionFromName("") == -1, "a name that is no action");
    // kFnOrder: each action once, Game_WndProc's old order.
    bool seen[9] = {};
    for (in::FnAction a : in::kFnOrder) seen[A(a)] = true;
    bool all = true;
    for (bool s : seen) all = all && s;
    Check(all, "kFnOrder holds every action");
    Check(in::kFnOrder[0] == in::FnAction::kQuickMenu && in::kFnOrder[1] == in::FnAction::kPause &&
              in::kFnOrder[2] == in::FnAction::kWindow && in::kFnOrder[3] == in::FnAction::kRows &&
              in::kFnOrder[4] == in::FnAction::kQuicksave && in::kFnOrder[8] == in::FnAction::kSpeed,
          "kFnOrder is F5, F9, F8, the study key, F6 ... F1");
}

void TestText() {
    const in::FnBindings d = in::FnBindings::Defaults();
    // Round trip, both separators.
    for (const char* sep : {",", "\r\n", "\n"}) {
        in::FnBindings r = in::FnBindings::Defaults();
        r.key[0] = -1;
        std::vector<std::string> problems;
        const int n = in::FnParse(in::FnFormat(d, sep), sep[0] == '\r' ? '\n' : sep[0], r, &problems);
        Check(n == 18 && problems.empty() && r == d, "the defaults' text read back", sep[0] == ',' ? "," : "newline");
    }
    Check(in::FnFormat(d, ",").rfind("fkey.speed=F1,fpad.speed=none,fkey.reading_rows=F4,", 0) == 0,
          "the text form", in::FnFormat(d, ","));
    // Any key, any pad input - the ones added for this page too - and none.
    in::FnBindings c = d;
    c.key[A(in::FnAction::kQuicksave)] = in::KeyFromName("Z");
    c.key[A(in::FnAction::kSpeed)] = -1;
    c.key[A(in::FnAction::kPicture)] = 0xC8;                       // Up
    c.key[A(in::FnAction::kSaveWrite)] = 0x1A;                     // an unnamed scan code, written 0x1A
    c.pad[A(in::FnAction::kQuickMenu)] = static_cast<int>(in::PadInput::kRightPaddle1);
    c.pad[A(in::FnAction::kPause)] = static_cast<int>(in::PadInput::kMisc6);
    c.pad[A(in::FnAction::kRows)] = static_cast<int>(in::PadInput::kLt);
    in::FnBindings r = d;
    std::vector<std::string> problems;
    Check(in::FnParse(in::FnFormat(c, ","), ',', r, &problems) == 18 && problems.empty() && r == c,
          "a changed table read back", in::FnFormat(c, ","));
    Check(in::FnFormat(c, ",").find("fpad.pause=misc6,") != std::string::npos &&
              in::FnFormat(c, ",").find("fkey.speed=none,") != std::string::npos &&
              in::FnFormat(c, ",").find("fpad.quick_menu=rpaddle1,") != std::string::npos,
          "misc6, rpaddle1 and none written by name");
    // Partial: items over what is there, in order, the later winning.
    r = d;
    Check(in::FnParse("fkey.quicksave=F1, FKEY.Speed = none ,fpad.window=SOUTH,fkey.quicksave=F2", ',', r, nullptr) == 4,
          "four items understood");
    Check(r.key[A(in::FnAction::kQuicksave)] == in::FnKeyOfF(2) && r.key[A(in::FnAction::kSpeed)] == -1 &&
              r.pad[A(in::FnAction::kWindow)] == static_cast<int>(in::PadInput::kSouth) &&
              r.key[A(in::FnAction::kQuickload)] == in::FnKeyOfF(7),
          "items applied in order, case and spaces aside, the rest kept");
    // No swap rule: F1 now held twice (speed was set to none above, so give it back).
    r.key[A(in::FnAction::kSpeed)] = in::FnKeyOfF(1);
    r.key[A(in::FnAction::kQuicksave)] = in::FnKeyOfF(1);
    Check(r.key[A(in::FnAction::kSpeed)] == r.key[A(in::FnAction::kQuicksave)], "one key may hold two actions");
    // Bad items: each left out, each described, the good ones kept.
    r = d;
    problems.clear();
    const int n = in::FnParse("fkey.nope=F1,fkey.speed=NoSuchKey,fpad.speed=bogus,speed=F2,junk,key.speed=F3,"
                              "fkey.quickload=F3,fkey.window=0x100",
                              ',', r, &problems);
    Check(n == 1 && problems.size() == 7, "seven bad items, one good", std::to_string(n) + " " + std::to_string(problems.size()));
    Check(r.key[A(in::FnAction::kQuickload)] == in::FnKeyOfF(3) && r.key[A(in::FnAction::kSpeed)] == in::FnKeyOfF(1) &&
              r.key[A(in::FnAction::kWindow)] == in::FnKeyOfF(8),
          "a bad item changes nothing");
    if (problems.size() == 7)
        Check(problems[0].find("no such action") != std::string::npos &&
                  problems[1].find("not a key name") != std::string::npos &&
                  problems[2].find("not a pad input") != std::string::npos && problems[4].find("no '='") != std::string::npos,
              "each problem says what is wrong", problems[0] + " | " + problems[1]);
    Check(in::FnParse("", ',', r, &problems) == 0, "an empty text: nothing");
}

void TestRubyKey() {
    for (int n = 1; n <= 12; ++n) Check(in::FnRubyKey("f" + std::to_string(n)) == in::FnKeyOfF(n), "f n");
    Check(in::FnRubyKey("none") == -1, "none");
    for (const char* bad : {"f0", "f13", "f01", "F4", "", "z", "f4 "}) Check(in::FnRubyKey(bad) == -2, "not a value", bad);
}

void TestMessages() {
    // Producer 1 and 2, the Win32 window and the SDL window's front procedure
    // on Windows: WM_KEYDOWN as Windows gives it.
    for (int n = 1; n <= 12; ++n) {
        const unsigned vk = 0x6F + static_cast<unsigned>(n);
        const int scan = in::FnKeyOfF(n);
        Check(in::FnKeyOfMessage(vk, Msg(scan, false)) == in::FnKeyOfF(n), "an F key's message");
        // Producer 4 and 5, a recipe's key (game_window::QueueKey, os::PostWindowKey): lParam 1.
        Check(in::FnKeyOfMessage(vk, 1) == in::FnKeyOfF(n), "a recipe's F key, no scan code");
        // An F key by its virtual key, whatever the scan code says.
        Check(in::FnKeyOfMessage(vk, Msg(0x2C, false)) == in::FnKeyOfF(n), "an F key by its virtual key alone");
    }
    Check(in::FnKeyOfMessage('Z', Msg(0x2C, false)) == 0x2C, "Z by its scan code");
    Check(in::FnKeyOfMessage(0x26, Msg(0x48, true)) == 0xC8, "Up: 0x48 extended is 0xC8");
    Check(in::FnKeyOfMessage(0x68, Msg(0x48, false)) == 0x48, "Numpad8: 0x48 plain");
    Check(in::FnKeyOfMessage(0x0D, Msg(0x1C, true)) == 0x9C, "the keypad's Enter: 0x9C");
    Check(in::FnKeyOfMessage(0x13, Msg(0x45, false)) == 0x45, "Pause comes as 0x45 plain: 0x45, as the capture stores it");
    Check(in::FnKeyOfMessage(0x90, Msg(0x45, true)) == 0xC5, "NumLock comes as 0x45 extended: 0xC5, as the capture");
    Check(in::FnKeyOfMessage('Z', 1) == -1, "a recipe's Z: no scan code, no key");
    Check(in::FnKeyOfMessage('Q', Msg(0x40, false)) == -1, "F6's scan code under another virtual key: no key");
    Check(in::FnKeyOfMessage('Z', Msg(0x2C, false, true)) == 0x2C, "a repeat is the same key");
    // Producer 3, the SDL window off Windows: key_sdl.h's DIK number through
    // FnMessageBitsOfDik, then read back as the capture would store the key.
    for (int dik = 1; dik < 256; ++dik) {
        bool fkey = false;
        for (int n = 1; n <= 12; ++n) fkey = fkey || dik == in::FnKeyOfF(n);
        if (fkey) continue;           // F keys come by their virtual key (game_window.cpp VkOf)
        if (dik == 0x80) continue;    // scan code 0, extended: no key has it
        const int want = dik == 0x45 ? 0xC5 : dik == 0xC5 ? 0x45 : dik;
        const int got = in::FnKeyOfMessage('A', 1 | in::FnMessageBitsOfDik(dik));
        if (got != want) {
            Check(false, "an SDL key's number read back", std::to_string(dik) + " -> " + std::to_string(got));
            break;
        }
    }
    Check(in::FnMessageBitsOfDik(0) == 0 && in::FnMessageBitsOfDik(0x100) == 0, "no bits for 0 or past 0xFF");
    Check(in::FnMessageBitsOfDik(0xC5) == (0x45 << 16), "SDL's Pause (DIK 0xC5) as Windows': 0x45 plain");
    Check(in::FnMessageBitsOfDik(0x45) == ((0x45 << 16) | (1 << 24)), "SDL's NumLock (DIK 0x45) as Windows': extended");
}

void TestOrder() {
    in::FnBindings b = in::FnBindings::Defaults();
    for (int a = 0; a < 9; ++a) Check(in::FnActionOfKey(b, b.key[a]) == a, "each default key starts its action");
    Check(in::FnActionOfKey(b, -1) == -1 && in::FnActionOfKey(b, 0x2C) == -1 && in::FnActionOfKey(b, 0x44) == -1,
          "none, Z and F10 start nothing");
    // BOF3X_RUBY_KEY on another action's key, as Game_WndProc ordered them before.
    b.key[A(in::FnAction::kRows)] = in::FnKeyOfF(6);
    Check(in::FnActionOfKey(b, in::FnKeyOfF(6)) == A(in::FnAction::kRows), "f6: the row above the text, not the quicksave");
    b.key[A(in::FnAction::kRows)] = in::FnKeyOfF(5);
    Check(in::FnActionOfKey(b, in::FnKeyOfF(5)) == A(in::FnAction::kQuickMenu), "f5: the quick menu");
    b.key[A(in::FnAction::kRows)] = in::FnKeyOfF(9);
    Check(in::FnActionOfKey(b, in::FnKeyOfF(9)) == A(in::FnAction::kPause), "f9: the pause");
    b.key[A(in::FnAction::kRows)] = in::FnKeyOfF(8);
    Check(in::FnActionOfKey(b, in::FnKeyOfF(8)) == A(in::FnAction::kWindow), "f8: the window");
    b.key[A(in::FnAction::kRows)] = in::FnKeyOfF(1);
    Check(in::FnActionOfKey(b, in::FnKeyOfF(1)) == A(in::FnAction::kRows), "f1: the row above the text, not the speed");
    Check(in::FnActionOfKey(b, in::FnKeyOfF(4)) == -1, "F4 then starts nothing");
    b.key[A(in::FnAction::kRows)] = -1;
    Check(in::FnActionOfKey(b, in::FnKeyOfF(4)) == -1, "none: F4 starts nothing");
}

void TestRed() {
    Check(in::FnDoubled({}).empty(), "no cells");
    const std::vector<bool> r = in::FnDoubled({1, -1, 2, 1, -1, 3});
    Check(r == std::vector<bool>{true, false, false, true, false, false}, "a value twice red, empty cells never");
    // The three pages: the default game keys (one cell each), no function key among them.
    std::vector<int> game_keys, game_pads;
    for (const in::KeyBinding& k : in::DefaultKeys()) game_keys.push_back(k.dik);
    for (const in::PadBinding& p : in::Bindings::Defaults().pad) game_pads.push_back(static_cast<int>(p.input));
    in::FnBindings fk = in::FnBindings::Defaults();
    in::FnRed red = in::FnRedCells(game_keys, game_pads, fk);
    bool any = false;
    for (bool b : red.game_keys) any = any || b;
    for (bool b : red.game_pads) any = any || b;
    for (int a = 0; a < 9; ++a) any = any || red.fn_key[a] || red.fn_pad[a];
    Check(!any, "the defaults: nothing red");
    // Two actions on F6: both red, the game's cells not.
    fk.key[A(in::FnAction::kSpeed)] = in::FnKeyOfF(6);
    red = in::FnRedCells(game_keys, game_pads, fk);
    Check(red.fn_key[A(in::FnAction::kSpeed)] && red.fn_key[A(in::FnAction::kQuicksave)] &&
              !red.fn_key[A(in::FnAction::kQuickload)],
          "two actions on one key: both red, the others not");
    // An action on Z, the game's triangle key (the default table): both red.
    fk = in::FnBindings::Defaults();
    fk.key[A(in::FnAction::kQuickload)] = in::KeyFromName("Z");
    red = in::FnRedCells(game_keys, game_pads, fk);
    size_t z = 0;
    while (z < game_keys.size() && game_keys[z] != in::KeyFromName("Z")) ++z;
    Check(z < game_keys.size() && red.game_keys[z] && red.fn_key[A(in::FnAction::kQuickload)],
          "an action on a game button's key: both pages red");
    // An action on the pad's south, a game button's input: both red; on a
    // paddle no game button holds: not red.
    fk = in::FnBindings::Defaults();
    fk.pad[A(in::FnAction::kPause)] = static_cast<int>(in::PadInput::kSouth);
    fk.pad[A(in::FnAction::kQuicksave)] = static_cast<int>(in::PadInput::kLeftPaddle2);
    red = in::FnRedCells(game_keys, game_pads, fk);
    size_t s = 0;
    while (s < game_pads.size() && game_pads[s] != static_cast<int>(in::PadInput::kSouth)) ++s;
    Check(s < game_pads.size() && red.game_pads[s] && red.fn_pad[A(in::FnAction::kPause)] &&
              !red.fn_pad[A(in::FnAction::kQuicksave)],
          "a pad input a game button holds: red; a free paddle: not");
    // A key and a pad input are never compared with each other.
    fk = in::FnBindings::Defaults();
    fk.pad[A(in::FnAction::kSpeed)] = game_keys[0];
    red = in::FnRedCells(std::vector<int>{}, std::vector<int>{}, fk);
    Check(!red.fn_pad[A(in::FnAction::kSpeed)], "a pad input the same number as a key: not red");
    // Two game buttons on one key (a hand-written ini): both red too.
    red = in::FnRedCells({0x2C, 0x2C, 0x2D}, {}, in::FnBindings::Defaults());
    Check(red.game_keys[0] && red.game_keys[1] && !red.game_keys[2], "two game buttons on one key: red");
    // A kept key (the keypad's 7, a diagonal) that a function key takes: red.
    fk = in::FnBindings::Defaults();
    fk.key[A(in::FnAction::kWindow)] = 0x47;
    red = in::FnRedCells({0x47}, {}, fk);
    Check(red.fn_key[A(in::FnAction::kWindow)], "an action on a diagonal's key: red");
}

void TestPadInputs() {
    // The inputs added for this page: names and labels, read back, the old ones where they were.
    Check(static_cast<int>(in::PadInput::kCount) == 36, "36 pad inputs");
    Check(static_cast<int>(in::PadInput::kRsRight) == 24, "the old 25 keep their numbers");
    for (int i = 0; i < static_cast<int>(in::PadInput::kCount); ++i) {
        Check(in::PadInputs()[i].input == static_cast<in::PadInput>(i), "the inputs' table in enum order");
        Check(in::PadInputFromName(in::PadInputName(static_cast<in::PadInput>(i))) == i, "a pad input's name read back",
              in::PadInputName(static_cast<in::PadInput>(i)));
    }
    for (const char* n : {"misc1", "rpaddle1", "lpaddle1", "rpaddle2", "lpaddle2", "touchpad", "misc2", "misc3", "misc4",
                          "misc5", "misc6", "guide", "ls", "rs", "lt", "rt", "ls_up", "rs_right"})
        Check(in::PadInputFromName(n) >= 0, "a pad input by name", n);
    // An old ini's pad lines read as before.
    std::vector<in::PadBinding> pad;
    in::Layout layout = in::Layout::kPositional;
    Check(in::ParsePad("south=cross,lt=l2,rs_right=r1,layout=nintendo", ',', pad, layout) == 4 && pad.size() == 3 &&
              pad[1].input == in::PadInput::kLt && pad[2].input == in::PadInput::kRsRight && layout == in::Layout::kNintendo,
          "old pad items read as before");
    pad.clear();
    Check(in::ParsePad("rpaddle1=cross,misc6=start", ',', pad, layout) == 2 && pad.size() == 2 &&
              pad[0].input == in::PadInput::kRightPaddle1,
          "the new inputs bind game buttons too");
}

}  // namespace

int main() {
    std::printf("function keys host test (%u-bit)\n", static_cast<unsigned>(sizeof(void*) * 8));
    TestTable();
    TestText();
    TestRubyKey();
    TestMessages();
    TestOrder();
    TestRed();
    TestPadInputs();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

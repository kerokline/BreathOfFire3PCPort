// bof3x-launcher with no window (docs/launcher-portable.md, stage P0; the
// Linux round's C7): it reads bof3x.ini, applies it as the Windows launcher
// does (launcher/start.h: BOF3.CFG and the environment the game inherits)
// and starts bof3x. It behaves as the Windows launcher with --no-config,
// always. It also changes one setting (--set) or shows the settings (--list).
//
//   bof3x-launcher [--game <folder>] [--set <key>=<value>]... [--list] [-- <arguments for the game>]
//
// Compiled for Linux by tools/linux_probe/linux_tree.py; linked in the
// round's wave B. Its arguments are UTF-8 (a Linux program's). The game is
// the program beside this one with this one's name ending: bof3x, or
// bof3x.exe beside a launcher named *.exe. The hosted build and a moved image
// (BOF3X_IMAGEBASE other than 0x400000) are the Windows developer
// launcher's: refused here.
//
// The texts are the player's (CLAUDE.md, ASD-STE100): short sentences, active
// voice. The refusals of the start's own steps keep the texts the Windows
// launcher shows (launcher/start.cpp).
#include "launcher/config.h"
#include "launcher/start.h"

#include "os/env.h"
#include "os/text.h"
#include "os/wide_text.h"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

namespace {

const char kUsage[] =
    "usage: bof3x-launcher [--game <folder>] [--set <key>=<value>]... [--list] [-- <arguments for the game>]\n"
    "  --game <folder>      The folder of the game. Without it, the launcher uses the current folder if it\n"
    "                       is a game folder, then the game folder of the home, then the PC game folder\n"
    "                       that the home plays in place.\n"
    "  --set <key>=<value>  Changes one setting in bof3x.ini, then stops. You can use --set more than one time.\n"
    "  --list               Shows the settings in bof3x.ini, then stops.\n"
    "Without --set and --list, the launcher starts the game with the settings in bof3x.ini.\n"
    "This launcher has no window. To change a setting, use --set or edit bof3x.ini.\n";

[[noreturn]] void Refuse(const std::string& sentence) {
    std::fprintf(stderr, "bof3x-launcher: %s\n%s", sentence.c_str(), kUsage);
    std::exit(1);
}

// The setting lines of bof3x.ini as ConfigSave writes them: no comment, no section.
void List(const bof3x::Config& cfg) {
    const std::string text = bof3x::ConfigText(cfg);
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        std::printf("%s\n", line.c_str());
    }
}

}  // namespace

[[noreturn]] void bof3x::StartDie(const std::wstring& message) {
    bof3::os::PrintW(stderr, L"bof3x-launcher: " + message + L"\n");
    std::exit(1);
}

int main(int argc, char** argv) {
    std::wstring named;
    std::vector<std::wstring> game_args;
    std::vector<std::pair<std::string, std::string>> sets;
    bool list = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--game" && i + 1 < argc) {
            named = bof3::os::ToWide(argv[++i]);
        } else if (arg == "--set" && i + 1 < argc) {
            const std::string kv = argv[++i];
            const std::size_t eq = kv.find('=');
            if (eq == std::string::npos || eq == 0) Refuse("--set needs a key and a value: --set <key>=<value>.");
            sets.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
        } else if (arg == "--list") {
            list = true;
        } else if (arg == "--no-config") {
            // the only way this launcher runs: accepted, so that a harness's command keeps its meaning
        } else if (arg == "--host" && i + 1 < argc) {
            if (std::string(argv[++i]) != "exe")
                Refuse("This launcher starts bof3x only (--host exe). The hosted build is on Windows only.");
        } else if (arg == "--config") {
            Refuse("This launcher has no window. Use --set and --list to change and see the settings.");
        } else if (arg == "--") {
            for (++i; i < argc; ++i) game_args.push_back(bof3::os::ToWide(argv[i]));
        } else {
            Refuse("The argument " + arg + " is not known.");
        }
    }

    const std::wstring own = bof3x::StartOwnDirectory();
    // Settings. The file lives next to the launcher, not in the game folder.
    const std::wstring ini = own + bof3x::StartSep(L"\\bof3x.ini");

    if (list || !sets.empty()) {
        bof3x::Config cfg;
        std::string why;
        bof3x::ConfigLoad(ini, cfg, why);   // no file: the defaults
        if (!why.empty()) bof3x::StartDie(bof3x::StartWide(why) + L"\n\n" + ini);
        for (const auto& [key, value] : sets) {
            if (!bof3x::ConfigSetLine(cfg, key, value, why)) Refuse(why);
            std::printf("bof3x-launcher: bof3x.ini: %s=%s\n", key.c_str(), value.c_str());
        }
        if (!sets.empty() && !bof3x::ConfigSave(ini, cfg))
            bof3x::StartDie(L"Cannot write " + ini + L". The settings did not change.");
        if (list) List(cfg);
        return 0;
    }

    // The game: the program beside this one, with this one's ending.
    std::string self;
    if (!bof3::os::ExecutablePath(&self)) bof3x::StartDie(L"Cannot find the folder of this launcher.");
    const bool dot_exe = self.size() > 4 && self.compare(self.size() - 4, 4, ".exe") == 0;
    const std::wstring ours = own + bof3x::StartSep(dot_exe ? L"\\bof3x.exe" : L"\\bof3x");
    if (wchar_t base[32]; bof3::os::GetEnvW(L"BOF3X_IMAGEBASE", base, 32) > 0) {
        wchar_t* end = nullptr;
        if (std::wcstoul(base, &end, 0) != 0x400000 || !end || *end)
            bof3x::StartDie(L"BOF3X_IMAGEBASE asks for a moved image. This launcher does not start one. Remove "
                            L"BOF3X_IMAGEBASE or set it to 0x400000.");
    }
    if (!bof3x::StartFileExists(ours))
        bof3x::StartDie(ours + L" is not there. It belongs next to this launcher.");

    std::wstring game_dir, exe;
    bof3x::Config cfg;
    const bool exe_read = bof3x::StartFindGame(named, false, &game_dir, &exe);
    bof3x::StartReadSettings(ini, game_dir, named, false, exe_read, 0, cfg);   // --no-config: no Sources page
    bof3x::StartApply(game_dir, cfg, false, exe_read);
    bool waited = false;
    const std::uint32_t code = bof3x::StartGame(ours, game_args, game_dir, nullptr, &waited);
    return waited ? static_cast<int>(code) : 0;
}

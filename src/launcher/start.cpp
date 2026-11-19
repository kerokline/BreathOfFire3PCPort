// The start of the game with no window: see launcher/start.h. Moved out of
// launcher.cpp (docs/launcher-portable.md, stage P0) with its texts and its
// order unchanged; the comments are the ones it had there.
#include "launcher/start.h"

#include "common/cache_manifest.h"
#include "common/home.h"
#include "common/sha256.h"
#include "launcher/exe_image.h"
#include "os/env.h"
#include "os/path_file.h"
#include "os/process.h"
#include "os/text.h"
#include "os/wide_file.h"
#include "os/wide_text.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace bof3x {

namespace {

std::wstring Num(std::uint64_t n) { return std::to_wstring(n); }

// The file's SHA-256 as lowercase hex (common/sha256.cpp, the engine's routine
// too). Any I/O or CNG failure ends the launcher.
std::string Sha256HexOf(const std::wstring& path) {
    std::string hex, why;
    if (!Sha256Hex(path.c_str(), hex, why)) StartDie(L"cannot hash " + path + L": " + StartWide(why));
    return hex;
}

// Whether <cache>\base\exe\data.toml is an image tools/exe_tables.py built from
// BOF3.exe (`build = "pc-zh"`): the PC's own bytes, which a PC install may read
// in place of the file's (DIV-0212). A disc-built image is not.
bool CacheImageIsPcBuilt(const std::wstring& cache) {
    std::FILE* f = bof3::os::OpenPath(bof3::os::PathW(cache + StartSep(L"\\base\\exe\\data.toml")), "rb");
    if (!f) return false;
    char line[1024];
    bool pc = false;
    while (!pc && std::fgets(line, sizeof line, f)) pc = std::strncmp(line, "build = \"pc-zh\"", 15) == 0;
    std::fclose(f);
    return pc;
}

}  // namespace

std::wstring StartWide(const std::string& narrow) { return bof3::os::WideOfMessage(narrow); }

void StartSay(const std::wstring& line) { bof3::os::PrintW(stderr, L"bof3x-launcher: " + line); }

bool StartSelfTestOnly() { return bof3::os::GetEnvW(L"BOF3X_SELFTEST_ONLY", nullptr, 0) > 0; }

std::wstring StartSep(const wchar_t* part) {
    std::wstring s(part);
    for (wchar_t& c : s)
        if (c == L'\\') c = static_cast<wchar_t>(std::filesystem::path::preferred_separator);
    return s;
}

bool StartFileExists(const std::wstring& path) { return bof3::os::IsFileW(path); }

std::wstring StartFullPath(const std::wstring& path) {
    std::wstring full;
    if (!bof3::os::FullPathW(path, &full)) StartDie(L"cannot resolve path: " + path);
    return full;
}

std::wstring StartOwnDirectory() {
    std::string utf8;
    if (!bof3::os::ExecutablePath(&utf8)) StartDie(L"GetModuleFileName failed (" + Num(bof3::os::LastError()) + L")");
    const std::wstring path = bof3::os::ToWide(utf8.c_str());
    return path.substr(0, path.find_last_of(L"\\/"));
}

bool StartFindGame(const std::wstring& named, bool hosted, std::wstring* game_dir_out, std::wstring* exe_out) {
    std::wstring game_dir = named;
    if (game_dir.empty()) {
        if (StartFileExists(L"BOF3.exe")) game_dir = L".";
        else if (const std::wstring home_game = HomeGameDefault(); !home_game.empty()) game_dir = home_game;
        else game_dir = L"bof3";
    }
    game_dir = StartFullPath(game_dir);

    const std::wstring exe = game_dir + StartSep(L"\\BOF3.exe");
    *game_dir_out = game_dir;
    *exe_out = exe;
    // Optional for our own executable when a cache is the .data source (a
    // disc-built install, docs/disc-only-round.md): decided once the settings
    // are read, below. The hosted build runs inside the file.
    const bool exe_present = StartFileExists(exe);
    if (hosted && !exe_present)
        StartDie(L"no BOF3.exe in " + game_dir +
                 L".\nThe hosted build (--host bof3, or a hosted-only switch: BOF3X_ORIGINAL, "
                 L"BOF3X_SHADOW, BOF3X_CALLTRACE, BOF3X_MP3_COMPARE, BOF3X_MP3=original) runs inside the player's own BOF3.exe; a "
                 L"disc-built install has none. Point --game or BOF3_GAME_DIR at an install that has it.");
    if (exe_present) {
        const std::string sha = Sha256HexOf(exe);
        if (sha != kExpectedSha256)
            StartDie(exe + L" is not the build this project targets.\n\nsha256   " + StartWide(sha) + L"\nexpected " +
                     StartWide(kExpectedSha256) +
                     L"\n\nEvery address bof3x patches is specific to that one file; refusing to start. "
                     L"If this is an uncatalogued release, see fixtures.toml.");

        // The default key table is the engine's rule (input::DefaultKeys, the
        // owner's call of 2026-10-11); where the file is there, it must be
        // Key_TableDefault's 24 entries exactly.
        std::vector<input::KeyBinding> keys;
        std::string why;
        if (!ExeDefaultKeys(exe, keys, why))
            StartDie(L"cannot read the default key table from " + exe + L": " + StartWide(why));
        const std::vector<input::KeyBinding>& rule = input::DefaultKeys();
        bool same = keys.size() == rule.size();
        for (size_t i = 0; same && i < keys.size(); ++i) same = keys[i].dik == rule[i].dik && keys[i].bits == rule[i].bits;
        if (!same)
            StartDie(L"the default key table in " + exe + L" (" + Num(keys.size()) + L" keys) is not the engine's rule (" +
                     Num(rule.size()) + L" keys):\n" + StartWide(input::FormatKeys(keys, "", " ")) + L"\nbut\n" +
                     StartWide(input::FormatKeys(rule, "", " ")));
        if (StartSelfTestOnly())
            StartSay(L"the default key table, " + Num(rule.size()) + L" keys by the engine's rule, equal to BOF3.exe's: " +
                     StartWide(input::FormatKeys(rule, "", " ")) + L"\n");
    }
    return exe_present;
}

bool StartReadSettings(const std::wstring& ini, const std::wstring& game_dir, const std::wstring& named, bool hosted,
                       bool exe_present, int want_dialog, Config& cfg) {
    // DIV-0005: a retired bare language code (en, fr, de, ja) in the ini
    // or the environment stops here, before the dialog, naming its tag.
    std::string why;
    const bool had_ini = ConfigLoad(ini, cfg, why);
    if (!why.empty()) StartDie(StartWide(why) + L"\n\n" + ini);
    if (!had_ini) ConfigSeedFromGameCfg(game_dir, cfg);
    // An ini without ruby_ui= (older than the toggle): on when a
    // reading-row mod is installed (docs/launcher-settings.md 2a, Extra).
    ConfigRubyUiDefault(cfg, HomeDir());
    if (!ConfigCheckEnvironmentLanguage(why)) StartDie(StartWide(why));
    // No install at all - no BOF3.exe, no cache, no BOF3X_EXEIMAGE, and a
    // game folder the player did not name: the Sources page makes one (its
    // first run). Never under a self-test or --no-config, which keep the
    // refusals.
    const bool no_install = !hosted && !exe_present && named.empty() && ConfigCacheDataRoot(cfg).empty() &&
                            bof3::os::GetEnvW(L"BOF3X_EXEIMAGE", nullptr, 0) == 0 && !StartSelfTestOnly() &&
                            want_dialog != 0;
    // DIV-0102: "original" on an install without the Chinese text is
    // refused here, before the dialog; the default becomes a language
    // held. Not asked of no install: there is no language to hold yet.
    if (!no_install && !ConfigResolveLanguage(game_dir, cfg, why)) StartDie(StartWide(why));
    // DIV-0086 / DIV-0080: a bad opt= is refused, not dropped - dropped, it
    // would read as empty and play the default layers the player may have
    // meant to turn off.
    if (!ConfigOptValid(cfg.opt, why))
        StartDie(ini + L" says opt=" + StartWide(cfg.opt) + L": " + StartWide(why) +
                 L".\n\nopt= is empty (the default layers installed), none, or a "
                 L"comma-separated list of layer names (docs/launcher-settings.md).");
    // The PSP's battle rules (psp=), refused here by the DLL's own rule
    // rather than by a game that has started (psp_code.cpp keeps its check
    // for a hand-set BOF3X_PSP).
    if (!ConfigPspValid(cfg.psp, why))
        StartDie(ini + L" says psp=" + StartWide(cfg.psp) + L": " + StartWide(why) +
                 L".\n\npsp= is empty (the default: on when the cache's base is the PSP's), "
                 L"none, or a comma-separated list of the PSP's battle changes, duel-loss and myria-item "
                 L"(docs/psp-battle-changes.md).");
    return no_install;
}

void StartApply(const std::wstring& game_dir, Config& cfg, bool hosted, bool exe_present) {
    {
        // The cache's manifest through the one reader the DLL shares
        // (common/cache_manifest.h): a base_policy or exe_tables_from it cannot
        // honour stops the launcher here, with the sentence the DLL would stop
        // the game on. The cache BOF3X_CACHE or cache= names, whatever
        // BOF3X_CACHE_DATA says, as the DLL reads base_policy.
        const std::wstring root = ConfigCacheRoot(cfg);
        const CacheManifest manifest = ManifestRead(root);
        if (!manifest.error.empty())
            StartDie(ConfigFromUtf8(ManifestRefusal(manifest, ConfigToUtf8(root + StartSep(L"\\manifest.toml")))));
    }

    if (!exe_present) {
        // A disc-built install: the cache is the .data source (BOF3X_EXEIMAGE,
        // docs/exe-import-engine.md section 4.4), nothing is read from a file.
        // BOF3X_EXEIMAGE already set names it; else the settings' cache does.
        // Without either the refusal is the one there always was.
        const std::wstring cache = ConfigCacheDataRoot(cfg);
        wchar_t set[4];
        const bool image_set = bof3::os::GetEnvW(L"BOF3X_EXEIMAGE", set, 4) > 0;
        if (cache.empty() && !image_set)
            StartDie(L"no BOF3.exe in " + game_dir + L".\nPoint --game or BOF3_GAME_DIR at your own install.");
        if (!image_set) bof3::os::SetEnvW(L"BOF3X_EXEIMAGE", cache.c_str());
        StartSay(L"no BOF3.exe in " + game_dir +
                 L": a disc-built install - no hash check, the default keys by the engine's rule, .data from " +
                 (image_set ? std::wstring(L"BOF3X_EXEIMAGE's cache (the environment's)") : cache) + L"\n");
    }

    // The preset (launcher/config.h, docs/launcher-settings.md "preset="), at
    // launch on top of the settings, which it does not rewrite: its code names
    // go into BOF3X_OFF with the player's off=, its ini values are what the game
    // gets in place of the ini's (BOF3.CFG's display line too), its env values
    // are set where the environment has none, and BOF3X_PRESET names it for the
    // game's log. BOF3X_PRESET in the environment chooses it, as every variable wins.
    std::string preset_error;
    if (!ConfigPresetFromEnvironment(cfg, preset_error)) StartDie(StartWide(preset_error));
    const PresetPlan plan = ConfigPresetPlan(game_dir, cfg, cfg.preset, ConfigLanguagePlayed(game_dir, cfg));
    ConfigPresetReport(plan);
    const Config played = ConfigPresetApply(cfg, plan);

    std::wstring cfg_error;
    if (!StartSelfTestOnly() && !ConfigApplyGameCfg(game_dir, played, cfg_error))
        StartDie(cfg_error + L"\n\nDisplay and renderer are set through that file, which is the game's "
                             L"own input. Check that the game directory is writable.");
    ConfigPresetEnvironment(plan);   // before the ini's, which then keep them
    ConfigApplyEnvironment(game_dir, played, hosted);   // hosted: no layer that build refuses

    if (exe_present && !hosted) {
        // DIVERGENCE DIV-0212: a PC install whose cache holds the image built
        // from BOF3.exe reads .data from the cache too - the image and its
        // table files (docs/image-layer-round.md 5b) - so that every install
        // kind has one source. The bytes are the file's: the engine compares
        // them with BOF3.exe's .data before it goes on (exe_image.cpp). The
        // file stays the source with no cache, with a cache whose image a
        // disc built (those are not the PC's bytes), under BOF3X_EXEFILE=1,
        // and in the hosted build, which runs inside the file. After the
        // preset and the ini, which may set the switch or the cache.
        const std::wstring cache = ConfigCacheDataRoot(played);
        wchar_t set[4];
        const bool image_set = bof3::os::GetEnvW(L"BOF3X_EXEIMAGE", set, 4) > 0;
        const std::uint32_t from_file = bof3::os::GetEnvW(L"BOF3X_EXEFILE", set, 4);
        if (from_file != 0 && !(from_file == 1 && (set[0] == L'0' || set[0] == L'1')))
            StartDie(L"BOF3X_EXEFILE takes 1 (read .data from BOF3.exe though the cache holds its image) or 0.");
        const bool want_file = from_file == 1 && set[0] == L'1';
        if (!image_set && !want_file && !cache.empty() && CacheImageIsPcBuilt(cache)) {
            bof3::os::SetEnvW(L"BOF3X_EXEIMAGE", cache.c_str());
            StartSay(L".data from " + cache + L" (the image built from BOF3.exe and its table files; "
                                              L"BOF3X_EXEFILE=1 for the file)\n");
        }
    }
}

std::uint32_t StartGame(const std::wstring& program, const std::vector<std::wstring>& args,
                        const std::wstring& game_dir, void (*while_suspended)(bof3::os::Child&), bool* waited) {
    *waited = false;
    // Our own executable: the game directory as its current directory, as
    // BOF3.exe's (it opens DAT\, BGM\, SND\ and BOF3.exe itself by
    // relative path); nothing else to do from here - it maps the player's
    // data, binds its imports and starts the game on its own main thread.
    // Started by us: bof3x.exe started any other way hands over to us (exe_host.cpp HandOffToLauncher).
    bof3::os::SetEnvW(L"BOF3X_LAUNCHER", L"1");
    bof3::os::ChildSpec spec;
    spec.program = program;
    spec.args = args;
    spec.directory = game_dir;
    spec.suspended = while_suspended != nullptr;
    bof3::os::Child child;
    std::uint32_t error = 0;
    if (!bof3::os::ChildStart(spec, &child, &error))
        StartDie(L"CreateProcess failed for " + program + L" (" + Num(error) + L")");
    if (while_suspended) {
        while_suspended(child);
        if (!bof3::os::ChildResume(child)) StartDie(L"ResumeThread failed (" + Num(bof3::os::LastError()) + L")");
    }
    if (StartSelfTestOnly()) {   // as the hosted path: the pid, then the game process's exit code
        StartSay(L"self-test only, game pid " + Num(child.pid) + L" (bof3x.exe)\n");
        std::fflush(stderr);
        std::uint32_t code = 1;
        bof3::os::ChildWait(child, &code);
        StartSay(L"self-test only, game process exit code " + Num(code) + L"\n");
        *waited = true;
        return code;
    }
    bof3::os::PrintW(stdout, L"bof3x-launcher: started " + program + L" (pid " + Num(child.pid) + L")\n");
    bof3::os::ChildClose(child);
    return 0;
}

}  // namespace bof3x

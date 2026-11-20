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

// BOF3X_EXEFILE, the developer's switch (docs/exe-retire-round.md, X2): 1, the
// engine opens BOF3.exe in the game folder and reads .data from it; 0 or
// unset, a player's start, which reads no BOF3.exe. Any other value stops the
// launcher, as it did before (then only beside a BOF3.exe).
bool ExeFileSwitch() {
    wchar_t set[4];
    const std::uint32_t from_file = bof3::os::GetEnvW(L"BOF3X_EXEFILE", set, 4);
    if (from_file != 0 && !(from_file == 1 && (set[0] == L'0' || set[0] == L'1')))
        StartDie(L"BOF3X_EXEFILE takes 1 (read .data from BOF3.exe, not from the cache's image: a developer's run) "
                 L"or 0.");
    return from_file == 1 && set[0] == L'1';
}

// Rows 2 and 3 of docs/exe-retire-round.md: BOF3.exe hashed whole, and its
// default key table against the engine's rule. Only on a start that reads the
// file (the hosted build, BOF3X_EXEFILE=1).
void CheckExe(const std::wstring& exe) {
    const std::string sha = Sha256HexOf(exe);
    if (sha != kExpectedSha256)
        StartDie(exe + L" is not the build this project targets.\n\nsha256   " + StartWide(sha) + L"\nexpected " +
                 StartWide(kExpectedSha256) +
                 L"\n\nEvery address bof3x patches is specific to that one file; refusing to start. "
                 L"If this is an uncatalogued release, see fixtures.toml.");

    // The default key table is the engine's rule (input::DefaultKeys, the
    // owner's call of 2026-10-11); where the file is read, it must be
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

// Whether `cache` holds an image of .data (base\exe\data.toml, which
// tools/exe_tables.py writes beside data.bin and its table files): from the
// PC's executable or from a disc, the one source of .data for a player.
bool CacheHasImage(const std::wstring& cache) {
    return !cache.empty() && StartFileExists(cache + StartSep(L"\\base\\exe\\data.toml"));
}

// Whether the game folder's BGM\ holds a file: the PC game's music, which a PC
// folder has and a setup from it copies (player_sources.py copy_pc), and which
// a game folder laid from a disc does not (importer.py install_base leaves BGM\
// empty). The test call 3's line asks in place of BOF3.exe.
bool GameFolderHoldsPcMusic(const std::wstring& game_dir) {
    for (const bof3::os::MatchW& m : bof3::os::MatchesW(game_dir + StartSep(L"\\BGM\\*")))
        if (!m.directory) return true;
    return false;
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
    // Only the developer's start reads BOF3.exe: the hosted build, which runs
    // inside it, and BOF3X_EXEFILE=1 (docs/exe-retire-round.md, X2). A
    // player's start never opens, hashes or tests for the file.
    const bool developer = hosted || ExeFileSwitch();
    std::wstring game_dir = named;
    if (game_dir.empty()) {
        // The working folder when it is a game folder: a player's start asks
        // InstallGameFolder (DAT\FIRST.DAT, a PC folder and a disc-built game
        // folder alike); the developer's asks for BOF3.exe, as before.
        const bool here = developer ? StartFileExists(L"BOF3.exe") : InstallGameFolder(L".");
        if (here) game_dir = L".";
        else if (const std::wstring home_game = HomeGameDefault(); !home_game.empty()) game_dir = home_game;
        // A PC folder set up in place (docs/exe-retire-round.md X3, X6): the
        // home holds its cache and no game\, and its source list names the
        // folder. Taken when it is a game folder; never the home's game\
        // (HomeGameDefault), so the stale check and its re-setup, which swap
        // the home's game\ whole, never reach the player's folder.
        else if (const std::wstring in_place = HomeInPlaceFolder(); InstallGameFolder(in_place))
            game_dir = in_place;
        else game_dir = L"bof3";
    }
    game_dir = StartFullPath(game_dir);

    const std::wstring exe = game_dir + StartSep(L"\\BOF3.exe");
    *game_dir_out = game_dir;
    *exe_out = exe;
    if (!developer) return false;
    // The hosted build runs inside the file; under BOF3X_EXEFILE=1 the engine
    // reads .data from it when it is there, else from the cache's image.
    const bool exe_present = StartFileExists(exe);
    if (hosted && !exe_present)
        StartDie(L"no BOF3.exe in " + game_dir +
                 L".\nThe hosted build (--host bof3, or a hosted-only switch: BOF3X_ORIGINAL, "
                 L"BOF3X_SHADOW, BOF3X_CALLTRACE, BOF3X_MP3_COMPARE, BOF3X_MP3=original) runs inside the player's own BOF3.exe; the "
                 L"game folder that the importer makes holds none. Point --game or BOF3_GAME_DIR at the PC game's own folder.");
    if (exe_present) CheckExe(exe);
    return exe_present;
}

StartSetUp StartReadSettings(const std::wstring& ini, const std::wstring& game_dir, const std::wstring& named,
                             bool hosted, bool exe_read, int want_dialog, Config& cfg) {
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
    // What the Sources page sets up first (StartSetUpFor, docs/exe-retire-round.md
    // X3): no install at all - no game folder (InstallGameFolder), no cache, no
    // BOF3X_EXEIMAGE, a game folder the player did not name - gets its first
    // run; a game folder with no image of .data, where the home's cache is the
    // one to build, gets the first run (the home's own game\) or is set up in
    // place (any other folder). Never under a self-test or --no-config, which
    // keep StartApply's refusal (StartRefuseNoImage). Nothing here opens or
    // tests for BOF3.exe: the importer says what the folder is.
    StartSetUpAsk ask;
    ask.hosted = hosted;
    ask.exe_read = exe_read;
    ask.self_test = StartSelfTestOnly();
    ask.want_dialog = want_dialog;
    ask.named = !named.empty();
    ask.game_folder = InstallGameFolder(game_dir);
    const std::wstring home_game = HomeGameDefault();
    ask.home_game = !home_game.empty() && StartFullPath(home_game) == game_dir;
    const std::wstring data_cache = ConfigCacheDataRoot(cfg);
    ask.cache = !data_cache.empty();
    ask.cache_image = CacheHasImage(data_cache);
    {
        wchar_t data[4];
        const std::uint32_t n = bof3::os::GetEnvW(L"BOF3X_CACHE_DATA", data, 4);
        ask.cache_chosen = bof3::os::GetEnvW(L"BOF3X_CACHE", nullptr, 0) > 0 || !cfg.cache.empty() ||
                           (n == 1 && data[0] == L'0');
    }
    ask.exe_image_set = bof3::os::GetEnvW(L"BOF3X_EXEIMAGE", nullptr, 0) > 0;
    const StartSetUp set_up = StartSetUpFor(ask);
    // DIV-0102: "original" on an install without the Chinese text is
    // refused here, before the dialog; the default becomes a language
    // held. Not asked before a set-up: there is no language to hold yet, and
    // the next pass, after the page, asks it of the install the page made.
    if (set_up == StartSetUp::kNone && !ConfigResolveLanguage(game_dir, cfg, why, hosted)) StartDie(StartWide(why));
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
    return set_up;
}

[[noreturn]] void StartRefuseNoImage(const std::wstring& game_dir, const std::wstring& cache) {
    StartDie(L"This install has no game data, so the game cannot start (" +
             (cache.empty() ? std::wstring(L"no cache") : L"no base" + StartSep(L"\\exe") + L" in " + cache) +
             L"; the game folder is " + game_dir +
             L").\nSet up the install: open the launcher, go to the Sources page and add the game.\n"
             L"A developer can run tools/importer.py setup --home <install folder>, or set BOF3X_EXEFILE=1 to read "
             L"BOF3.exe.");
}

void StartApply(const std::wstring& game_dir, Config& cfg, bool hosted, bool exe_read) {
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

    // .data's source (docs/exe-retire-round.md, X2 and call 2). The hosted
    // build runs inside BOF3.exe; a start that reads the file (BOF3X_EXEFILE=1
    // with the file there) has it from the file, as before. Every other start
    // - a player's - has it from the cache's image, whatever built it: a PC
    // source or a disc. BOF3X_EXEIMAGE already set names it; else the settings'
    // cache does. Here, before the preset and BOF3.CFG, so that a start with
    // no image is refused before the game folder is written. The engine hashes
    // the image as it maps it against the manifest's [exe] sha256.
    if (!hosted && !exe_read) {
        const std::wstring cache = ConfigCacheDataRoot(cfg);
        wchar_t set[4];
        const bool image_set = bof3::os::GetEnvW(L"BOF3X_EXEIMAGE", set, 4) > 0;
        // A game folder with no image: at a player's start with the launcher's
        // windows the Sources page has set it up before this (StartSetUpFor,
        // docs/exe-retire-round.md X3). Under --no-config, a self-test, the
        // windowless launcher and a cache the player chose, the refusal.
        if (!image_set && !CacheHasImage(cache)) StartRefuseNoImage(game_dir, cache);
        if (!image_set) bof3::os::SetEnvW(L"BOF3X_EXEIMAGE", cache.c_str());
        // What the install is, from the cache's record (common/cache_manifest.h
        // ManifestInstallKind), never from a file beside the game.
        std::wstring kind = L"the cache's image";
        bool kind_disc = false;
        if (!image_set) {
            const CacheManifest m = ManifestRead(cache);
            if (const std::string note = ManifestKindNoteOnce(m); !note.empty()) StartSay(StartWide(note) + L"\n");
            kind_disc = ManifestInstallKind(m) == InstallKind::kDisc;
            if (ManifestInstallKind(m) == InstallKind::kPc) kind = L"a PC install: the image built from the PC game";
            else if (kind_disc) kind = L"a disc-built install: the image built from a disc";
        }
        StartSay(L".data from " + (image_set ? std::wstring(L"BOF3X_EXEIMAGE's cache (the environment's)") : cache) +
                 L" (" + kind + L", its table files laid in); BOF3.exe is not read (BOF3X_EXEFILE=1 reads it)\n");
        // Call 3 (docs/exe-retire-round.md section 7): a PC folder beside a
        // disc-built cache plays the disc-built image, as the cache's record
        // says. The folder is the PC's when its BGM\ holds the PC's music: a game
        // folder the importer lays from a disc has an empty BGM\ (importer.py
        // install_base; the music is the cache's base\bgm). No BOF3.exe is asked.
        if (!image_set && kind_disc && GameFolderHoldsPcMusic(game_dir))
            StartSay(L"the game folder " + game_dir + L" holds the PC game's music, but the game data is from a disc. "
                     L"To play the data of the PC game, add this folder on the Sources page.\n");
    } else if (exe_read && !hosted) {
        StartSay(L".data from " + game_dir + StartSep(L"\\BOF3.exe") + L" (BOF3X_EXEFILE=1, a developer's run)\n");
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

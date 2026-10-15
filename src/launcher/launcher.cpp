// bof3x-launcher: the settings, then the game - our own bof3x.exe by default,
// or the player's own BOF3.exe with bof3x.dll injected (--host bof3).
//
//   bof3x-launcher [--game <dir containing BOF3.exe>] [--config|--no-config]
//                  [--host exe|bof3] [-- <args for the game>]
//
// The game directory is, in order: --game, the BOF3_GAME_DIR environment
// variable, the current directory, the home's game\ (common/home.h), then
// ./bof3. With none of them an install and no cache, the Sources page offers
// to make one from the player's own copy of the game
// (docs/install-sources.md).
//
// --host exe (the default, docs/state3-design.md section 8): bof3x.exe, beside
// this launcher, started with the game directory as its current directory -
// no suspension, no reservation, no remote thread; the executable imports
// bof3x.dll and maps the player's data itself (src/hook/exe_host.cpp).
// BOF3.exe is optional there when the settings name a cache (cache= or
// BOF3X_CACHE) or the environment sets BOF3X_EXEIMAGE: a disc-built install,
// whose .data the cache holds (the launcher sets BOF3X_EXEIMAGE to the
// settings' cache unless the environment already does).
//
// --host bof3: the hosted build, kept for the shadow fuzz, the A/B switch and
// the reference pairs - and chosen without --host when the environment sets
// a switch only it can serve (HostedOnlyAsked). The original file is never modified and never copied
// (CLAUDE.md rule 1): the process is created suspended, a remote thread runs
// LoadLibraryW on our DLL, and only then is the game's main thread resumed - so
// every detour is in place before the entry point runs. Technique as described
// in docs/prior-art/tr1x.md section 2.1; implemented from that description.
//
// Must be built 32-bit, like the DLL: the remote thread's start address is OUR
// kernel32!LoadLibraryW, which is only the target's too when both processes
// load the same (32-bit) kernel32 - true within one boot session.
#include <windows.h>

#include "common/cache_manifest.h"
#include "common/home.h"
#include "common/icon_stamp.h"
#include "common/sha256.h"
#include "launcher/config.h"
#include "launcher/config_dialog.h"
#include "launcher/exe_image.h"
#include "launcher/sources_dialog.h"
#include "game/draw_pool_room.h"
#include "input/pad_sdl.h"

#include <cstdio>
#include <cwchar>
#include <initializer_list>
#include <cstdint>
#include <string>
#include <vector>

namespace {

// The suspended game process, while there is one that has not been resumed.
HANDLE g_child = nullptr;

// BOF3X_SELFTEST_ONLY: the DLL ends the process once every inject and
// start-up self-test has run, before the game's main thread is resumed - no
// window, no dialogs; the launcher's exit code is the game process's (0 all
// passed, 3 a Fatal). Lets several checkouts self-test at once
// (docs/SCAFFOLDING.md section 2).
bool SelfTestOnly() { return GetEnvironmentVariableW(L"BOF3X_SELFTEST_ONLY", nullptr, 0) > 0; }

// The switches that need Capcom's code, which only the hosted build maps
// (src/hook/exe_host.cpp RefuseHostedSwitches).
bool HostedOnlyAsked() {
    for (const wchar_t* name : {L"BOF3X_ORIGINAL", L"BOF3X_SHADOW", L"BOF3X_CALLTRACE"})
        if (GetEnvironmentVariableW(name, nullptr, 0) > 0) return true;
    wchar_t mp3[16] = {};
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_MP3", mp3, 16);
    return n > 0 && n < 16 && std::wcscmp(mp3, L"original") == 0;
}

// Every failure path: ends a suspended game process if there is one, reports
// on stderr and (not under BOF3X_SELFTEST_ONLY) in a message box, and exits 1.
[[noreturn]] void Die(const wchar_t* fmt, ...) {
    if (g_child) TerminateProcess(g_child, 1);
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vswprintf(buf, 1024, fmt, ap);
    va_end(ap);
    std::fwprintf(stderr, L"bof3x-launcher: %ls\n", buf);
    // A headless self-test shows no window, as the DLL's Fatal shows none
    // (src/hook/log.cpp): the refusal is on stderr and in the exit code.
    if (!SelfTestOnly()) MessageBoxW(nullptr, buf, L"bof3x-launcher", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring FullPath(const std::wstring& path) {
    wchar_t buf[MAX_PATH];
    DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH, buf, nullptr);
    if (n == 0 || n >= MAX_PATH) Die(L"cannot resolve path: %ls", path.c_str());
    return buf;
}

std::wstring OwnDirectory() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) Die(L"GetModuleFileName failed (%lu)", GetLastError());
    std::wstring path(buf);
    return path.substr(0, path.find_last_of(L"\\/"));
}

// The file's SHA-256 as lowercase hex (common/sha256.cpp, the engine's routine
// too). Any I/O or CNG failure Die()s.
std::string Sha256Hex(const std::wstring& path) {
    std::string hex, why;
    if (!bof3x::Sha256Hex(path.c_str(), hex, why)) Die(L"cannot hash %ls: %hs", path.c_str(), why.c_str());
    return hex;
}

// DIV-0095: the icon the engine generated from the game's data
// (docs/exe-icon.md). bof3x.exe cannot rewrite its own file while it runs, so
// it writes bof3x.exe.new and the bof3x.icon stamp beside itself, and the
// move is made here, before bof3x.exe starts. Only when bof3x.exe is still
// the file the engine copied and the new file is the one it verified: a
// stale copy (bof3x.exe rebuilt or replaced since) is deleted instead, and
// the engine makes a fresh one at its next run. Never stops the launcher.
void InstallGeneratedIcon(const std::wstring& ours) {
    const std::wstring pending = ours + L".new";
    if (!FileExists(pending)) return;
    bof3x::IconStamp stamp;
    std::string current, written, why;
    const bool have_stamp = bof3x::IconStampRead(OwnDirectory() + L"\\bof3x.icon", stamp);
    if (!have_stamp || !bof3x::Sha256Hex(ours.c_str(), current, why) || !bof3x::Sha256Hex(pending.c_str(), written, why) ||
        current != stamp.from || written != stamp.to) {
        DeleteFileW(pending.c_str());
        std::fwprintf(stderr, L"bof3x-launcher: %ls is not the icon written for this bof3x.exe (%hs); deleted - the "
                              L"game writes a fresh one\n",
                      pending.c_str(), !have_stamp ? "no bof3x.icon stamp" : !why.empty() ? why.c_str() : "the stamp does not match");
        return;
    }
    if (!MoveFileExW(pending.c_str(), ours.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::fwprintf(stderr, L"bof3x-launcher: cannot move %ls into place (error %lu); kept for the next start\n",
                      pending.c_str(), GetLastError());
        return;
    }
    std::wprintf(L"bof3x-launcher: bof3x.exe now carries the icon generated from the game's data (DIV-0095)\n");
}

#if BOF3X_HOSTED
// DIV-0062: the draw-item pool's room. The pool's items are linked into the
// game's ordering table by 24-bit addresses, so its array must sit below
// 16 MB - and by the time the DLL runs, the child's low memory is cut up by
// its heaps and mapped files (the largest gap measured 2026-09-27 was
// 128 KB). Here the child is suspended with only its image and ntdll mapped,
// so the block is taken at the first free candidate and stamped; the DLL
// looks for the stamp at the same candidates (src/game/draw_pool.cpp; both
// sides' constants are src/game/draw_pool_room.h's).
// Failing quietly is right: the DLL then keeps the original's pool and says so.
void ReserveDrawPoolIn(HANDLE process) {
    using namespace draw_pool_room;
    for (std::uint32_t at : kCandidates) {
        void* block = VirtualAllocEx(process, reinterpret_cast<void*>(static_cast<std::uintptr_t>(at)), kBytes,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!block) continue;
        if (WriteProcessMemory(process, block, kStamp, sizeof kStamp, nullptr)) return;
        VirtualFreeEx(process, block, 0, MEM_RELEASE);
    }
}

// Runs LoadLibraryW(dll) inside `process` and returns only once it has.
void LoadDllInto(HANDLE process, const std::wstring& dll) {
    ReserveDrawPoolIn(process);
    SIZE_T bytes = (dll.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE,
                                  PAGE_READWRITE);
    if (!remote) Die(L"VirtualAllocEx failed (%lu)", GetLastError());
    if (!WriteProcessMemory(process, remote, dll.c_str(), bytes, nullptr))
        Die(L"WriteProcessMemory failed (%lu)", GetLastError());

    auto start = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                               "LoadLibraryW")));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, start, remote, 0, nullptr);
    if (!thread) Die(L"CreateRemoteThread failed (%lu)", GetLastError());
    WaitForSingleObject(thread, INFINITE);
    if (SelfTestOnly()) {   // the DLL has ended the process: report how
        WaitForSingleObject(process, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(process, &code);
        std::fwprintf(stderr, L"bof3x-launcher: self-test only, game process exit code %lu\n", code);
        ExitProcess(code);
    }

    // A process ended by the DLL's own Fatal() during DllMain lands here too:
    // Fatal has shown its own message box, so the launcher ends quietly with
    // the game's exit code (3, Fatal's TerminateProcess code, src/hook/log.cpp)
    // rather than resuming a dead process or showing a second box.
    if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        ExitProcess(code);
    }
    // The thread's exit code is LoadLibraryW's return value: the module handle, or 0.
    DWORD module = 0;
    GetExitCodeThread(thread, &module);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    if (module == 0)
        Die(L"the game process could not load %ls.\nSee bof3x.log next to it, if one was written.",
            dll.c_str());
}
#else
// The player's launcher (CMakeLists.txt bof3x_add_launcher): --host bof3 and
// the hosted-only switches need code that writes into another process, which
// this launcher does not hold - it hands the whole run to the developer's
// launcher beside it, with the same arguments, environment and console or
// pipes, and ends with its exit code, so the harnesses' commands keep their
// meaning in a checkout. A release ships no developer launcher: it stops here.
[[noreturn]] void ForwardToDevLauncher(int argc, wchar_t** argv) {
    const std::wstring dev = OwnDirectory() + L"\\bof3x-launcher-dev.exe";
    if (!FileExists(dev))
        Die(L"--host bof3 and the hosted-only switches (BOF3X_ORIGINAL, BOF3X_SHADOW, BOF3X_CALLTRACE, "
            L"BOF3X_MP3=original) are the developer launcher's, bof3x-launcher-dev.exe, which a source build "
            L"writes beside this one; a release does not ship it.");
    std::wstring line = L"\"" + dev + L"\"";
    for (int i = 1; i < argc; ++i) line += L" \"" + std::wstring(argv[i]) + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE),
           err = GetStdHandle(STD_ERROR_HANDLE);
    // Redirected - a harness's pipe or a file, not a console - the developer
    // launcher writes to the same handles as this one would.
    auto redirected = [](HANDLE h) {
        if (!h || h == INVALID_HANDLE_VALUE) return false;
        const DWORD type = GetFileType(h);
        return type == FILE_TYPE_PIPE || type == FILE_TYPE_DISK;
    };
    const bool piped = redirected(out) && redirected(err);
    if (piped) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = in;
        si.hStdOutput = out;
        si.hStdError = err;
    }
    std::fflush(stdout);
    std::fflush(stderr);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(dev.c_str(), line.data(), nullptr, nullptr, piped, 0, nullptr, nullptr, &si, &pi))
        Die(L"CreateProcess failed for %ls (%lu)", dev.c_str(), GetLastError());
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    ExitProcess(code);
}
#endif

// The launcher is a windowed program, so a player who double-clicks it sees
// its window and no console (docs/launcher-settings.md section 2). Started
// from a terminal, it writes its lines there as the console program it was
// did; started with stdout and stderr redirected (a script's pipe or file),
// the C runtime already holds those, and nothing changes for the harnesses.
void AttachParentConsole() {
    auto redirected = [](DWORD which) {
        HANDLE h = GetStdHandle(which);
        return h != nullptr && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_UNKNOWN;
    };
    const bool out = redirected(STD_OUTPUT_HANDLE), err = redirected(STD_ERROR_HANDLE);
    if (out && err) return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;   // double-clicked: no terminal to write to
    if (!out) std::freopen("CONOUT$", "w", stdout);
    if (!err) std::freopen("CONOUT$", "w", stderr);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    AttachParentConsole();
    std::wstring game_dir;
    std::wstring game_args;
    // Neither flag given: the settings file decides, and its default is to
    // show the dialog. --config forces it (the way back after "Show this
    // window every time" is unticked); --no-config suppresses it, which is
    // what the scripted runs in docs/HANDOFF.md want.
    int want_dialog = -1;
    int host_asked = -1;   // --host: 0 exe, 1 bof3; -1 not given
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--game" && i + 1 < argc) {
            game_dir = argv[++i];
        } else if (arg == L"--host" && i + 1 < argc) {
            const std::wstring host = argv[++i];
            if (host == L"bof3") host_asked = 1;
            else if (host == L"exe") host_asked = 0;
            else Die(L"--host %ls: exe (bof3x.exe, the default) or bof3 (BOF3.exe, hosted)", host.c_str());
        } else if (arg == L"--config") {
            want_dialog = 1;
        } else if (arg == L"--no-config") {
            want_dialog = 0;
        } else if (arg == L"--") {
            for (++i; i < argc; ++i) {
                game_args += L" \"";
                game_args += argv[i];
                game_args += L"\"";
            }
        } else {
            Die(L"unknown argument: %ls\nusage: bof3x-launcher [--game <dir>] "
                L"[--config|--no-config] [--host exe|bof3] [-- <game args>]",
                arg.c_str());
        }
    }

    // The game directory: --game, BOF3_GAME_DIR, the current directory when it
    // holds BOF3.exe, the home's game\ when the Sources page laid one out
    // (common/home.h, docs/install-sources.md), else ./bof3.
    if (game_dir.empty()) {
        wchar_t env[MAX_PATH];
        DWORD n = GetEnvironmentVariableW(L"BOF3_GAME_DIR", env, MAX_PATH);
        if (n > 0 && n < MAX_PATH) game_dir = env;
    }
    // Named by the player, the folder is theirs to change: the Sources page,
    // which lays out the home's game\, is not offered for it.
    const std::wstring named_game = game_dir;

    std::wstring dll = OwnDirectory() + L"\\bof3x.dll";
    if (!FileExists(dll)) Die(L"%ls is missing; it belongs next to this launcher.", dll.c_str());
    // Not given: our own executable - unless the environment asks for what
    // only the hosted build has (Capcom's code: the A/B switch, the shadow
    // fuzz, the call trace, Capcom's MP3 decoder; bof3x.exe refuses each), so
    // the documented self-test and A/B commands keep their meaning.
    const bool hosted = host_asked >= 0 ? host_asked == 1 : HostedOnlyAsked();
#if !BOF3X_HOSTED
    if (hosted) ForwardToDevLauncher(argc, argv);
#endif
    if (host_asked < 0 && hosted)
        std::fwprintf(stderr, L"bof3x-launcher: a hosted-only switch is set (BOF3X_ORIGINAL, BOF3X_SHADOW, "
                              L"BOF3X_CALLTRACE or BOF3X_MP3=original): starting the hosted build, as --host bof3\n");
    const std::wstring ours = OwnDirectory() + L"\\bof3x.exe";
    if (!hosted && !FileExists(ours))
        Die(L"%ls is missing; it belongs next to this launcher (or start the hosted build with --host bof3).",
            ours.c_str());
    if (FileExists(ours)) InstallGeneratedIcon(ours);

    // Settings. The file lives next to the launcher, not in the game directory,
    // which stays the player's own (CLAUDE.md rule 1) apart from BOF3.CFG -
    // and that one is the original program's own documented input, not a patch.
    const std::wstring ini = OwnDirectory() + L"\\bof3x.ini";
    // From here to the settings dialog it may all run again: the Sources page
    // (docs/install-sources.md) can lay out a new game folder and cache, so
    // both are found again, and the settings read again, after it builds.
    std::wstring exe;
    bool exe_present = false;
    bof3x::Config cfg;
    for (bool again = true; again;) {
        again = false;
        game_dir = named_game;
        if (game_dir.empty()) {
            if (FileExists(L"BOF3.exe")) game_dir = L".";
            else if (const std::wstring home_game = bof3x::HomeGameDefault(); !home_game.empty()) game_dir = home_game;
            else game_dir = L"bof3";
        }
        game_dir = FullPath(game_dir);

        exe = game_dir + L"\\BOF3.exe";
        // Optional for our own executable when a cache is the .data source (a
        // disc-built install, docs/disc-only-round.md): decided once the settings
        // are read, below. The hosted build runs inside the file.
        exe_present = FileExists(exe);
        bool no_install = false;
        if (hosted && !exe_present)
            Die(L"no BOF3.exe in %ls.\nThe hosted build (--host bof3, or a hosted-only switch: BOF3X_ORIGINAL, "
                L"BOF3X_SHADOW, BOF3X_CALLTRACE, BOF3X_MP3=original) runs inside the player's own BOF3.exe; a "
                L"disc-built install has none. Point --game or BOF3_GAME_DIR at an install that has it.",
                game_dir.c_str());
        if (exe_present) {
            std::string sha = Sha256Hex(exe);
            if (sha != bof3x::kExpectedSha256)
                Die(L"%ls is not the build this project targets.\n\nsha256   %hs\nexpected %hs\n\n"
                    L"Every address bof3x patches is specific to that one file; refusing to start. "
                    L"If this is an uncatalogued release, see fixtures.toml.",
                    exe.c_str(), sha.c_str(), bof3x::kExpectedSha256);

            // The default key table is the engine's rule (input::DefaultKeys, the
            // owner's call of 2026-10-11); where the file is there, it must be
            // Key_TableDefault's 24 entries exactly.
            std::vector<bof3x::input::KeyBinding> keys;
            std::string why;
            if (!bof3x::ExeDefaultKeys(exe, keys, why))
                Die(L"cannot read the default key table from %ls: %hs", exe.c_str(), why.c_str());
            const std::vector<bof3x::input::KeyBinding>& rule = bof3x::input::DefaultKeys();
            bool same = keys.size() == rule.size();
            for (size_t i = 0; same && i < keys.size(); ++i) same = keys[i].dik == rule[i].dik && keys[i].bits == rule[i].bits;
            if (!same)
                Die(L"the default key table in %ls (%u keys) is not the engine's rule (%u keys):\n%hs\nbut\n%hs", exe.c_str(),
                    static_cast<unsigned>(keys.size()), static_cast<unsigned>(rule.size()),
                    bof3x::input::FormatKeys(keys, "", " ").c_str(), bof3x::input::FormatKeys(rule, "", " ").c_str());
            if (SelfTestOnly())
                std::fwprintf(stderr, L"bof3x-launcher: the default key table, %u keys by the engine's rule, equal to "
                                      L"BOF3.exe's: %hs\n",
                              static_cast<unsigned>(rule.size()), bof3x::input::FormatKeys(rule, "", " ").c_str());
        }

        {
            // DIV-0005: a retired bare language code (en, fr, de, ja) in the ini
            // or the environment stops here, before the dialog, naming its tag.
            std::string why;
            const bool had_ini = bof3x::ConfigLoad(ini, cfg, why);
            if (!why.empty()) Die(L"%hs\n\n%ls", why.c_str(), ini.c_str());
            if (!had_ini) bof3x::ConfigSeedFromGameCfg(game_dir, cfg);
            if (!bof3x::ConfigCheckEnvironmentLanguage(why)) Die(L"%hs", why.c_str());
            // No install at all - no BOF3.exe, no cache, no BOF3X_EXEIMAGE, and a
            // game folder the player did not name: the Sources page below makes
            // one (its first run). Never under a self-test or --no-config, which
            // keep the refusals.
            no_install = !hosted && !exe_present && named_game.empty() && bof3x::ConfigCacheDataRoot(cfg).empty() &&
                         GetEnvironmentVariableW(L"BOF3X_EXEIMAGE", nullptr, 0) == 0 && !SelfTestOnly() &&
                         want_dialog != 0;
            // DIV-0102: "original" on an install without the Chinese text is
            // refused here, before the dialog; the default becomes a language
            // held. Not asked of no install: there is no language to hold yet.
            if (!no_install && !bof3x::ConfigResolveLanguage(game_dir, cfg, why)) Die(L"%hs", why.c_str());
            // DIV-0086 / DIV-0080: a bad opt= is refused, not dropped - dropped, it
            // would read as empty and play the default layers the player may have
            // meant to turn off.
            if (!bof3x::ConfigOptValid(cfg.opt, why))
                Die(L"%ls says opt=%hs: %hs.\n\nopt= is empty (the default layers installed), none, or a "
                    L"comma-separated list of layer names (docs/launcher-settings.md).",
                    ini.c_str(), cfg.opt.c_str(), why.c_str());
            // The PSP's battle rules (psp=), refused here by the DLL's own rule
            // rather than by a game that has started (psp_code.cpp keeps its check
            // for a hand-set BOF3X_PSP).
            if (!bof3x::ConfigPspValid(cfg.psp, why))
                Die(L"%ls says psp=%hs: %hs.\n\npsp= is empty (the default: on when the cache's base is the PSP's), "
                    L"none, or a comma-separated list of the PSP's battle changes, duel-loss and myria-item "
                    L"(docs/psp-battle-changes.md).",
                    ini.c_str(), cfg.psp.c_str(), why.c_str());
        }

        if (no_install) {   // the Sources page's first run (above)
            const bool built = bof3x::SourcesDialogRun(nullptr, cfg, true);
            if (!bof3x::ConfigSave(ini, cfg))   // Locate ffmpeg's choice, kept whatever happened
                std::fwprintf(stderr, L"bof3x-launcher: cannot write %ls; settings not saved\n", ini.c_str());
            if (!built) return 0;   // closed without an install: start nothing
            cfg = bof3x::Config{};
            again = true;
            continue;
        }

        if (want_dialog == 1 || (want_dialog == -1 && cfg.show_launcher)) {
            // The pad, for the dialogs' navigation and the Controls capture; stopped
            // before the game starts, which opens it for itself (DIV-0050).
            bof3x::input::PadSdl_Start(cfg.bindings.layout, nullptr);
            const bof3x::ConfigDialogResult result = bof3x::ConfigDialogRun(game_dir, cfg);
            bof3x::input::PadSdl_Stop();
            if (result == bof3x::kDialogClosed) return 0;   // closed: start nothing
            if (result == bof3x::kDialogSave) {
                // left after Back to the title page: what the pages said, saved;
                // nothing started
                if (!bof3x::ConfigSave(ini, cfg))
                    std::fwprintf(stderr, L"bof3x-launcher: cannot write %ls; settings not saved\n", ini.c_str());
                return 0;
            }
            if (result == bof3x::kDialogSourcesChanged) {
                // the install was rebuilt: save what the page said, then find the
                // game folder and the cache again and show the page anew
                if (!bof3x::ConfigSave(ini, cfg))
                    std::fwprintf(stderr, L"bof3x-launcher: cannot write %ls; settings not saved\n", ini.c_str());
                cfg = bof3x::Config{};
                want_dialog = 1;
                again = true;
                continue;
            }
            std::string why;   // DIV-0102: the dialog's choice, under the cache it may have changed
            if (!bof3x::ConfigResolveLanguage(game_dir, cfg, why)) Die(L"%hs", why.c_str());
            if (!bof3x::ConfigSave(ini, cfg))
                std::fwprintf(stderr, L"bof3x-launcher: cannot write %ls; settings not saved\n",
                              ini.c_str());
        }
    }

    {
        // The cache's manifest through the one reader the DLL shares
        // (common/cache_manifest.h): a base_policy or exe_tables_from it cannot
        // honour stops the launcher here, with the sentence the DLL would stop
        // the game on. The cache BOF3X_CACHE or cache= names, whatever
        // BOF3X_CACHE_DATA says, as the DLL reads base_policy.
        const std::wstring root = bof3x::ConfigCacheRoot(cfg);
        const bof3x::CacheManifest manifest = bof3x::ManifestRead(root);
        if (!manifest.error.empty())
            Die(L"%ls", bof3x::ConfigFromUtf8(bof3x::ManifestRefusal(manifest, bof3x::ConfigToUtf8(root + L"\\manifest.toml")))
                            .c_str());
    }

    if (!exe_present) {
        // A disc-built install: the cache is the .data source (BOF3X_EXEIMAGE,
        // docs/exe-import-engine.md section 4.4), nothing is read from a file.
        // BOF3X_EXEIMAGE already set names it; else the settings' cache does.
        // Without either the refusal is the one there always was.
        const std::wstring cache = bof3x::ConfigCacheDataRoot(cfg);
        wchar_t set[4];
        const bool image_set = GetEnvironmentVariableW(L"BOF3X_EXEIMAGE", set, 4) > 0;
        if (cache.empty() && !image_set)
            Die(L"no BOF3.exe in %ls.\nPoint --game or BOF3_GAME_DIR at your own install.", game_dir.c_str());
        if (!image_set) SetEnvironmentVariableW(L"BOF3X_EXEIMAGE", cache.c_str());
        std::fwprintf(stderr, L"bof3x-launcher: no BOF3.exe in %ls: a disc-built install - no hash check, the default "
                              L"keys by the engine's rule, .data from %ls\n",
                      game_dir.c_str(), image_set ? L"BOF3X_EXEIMAGE's cache (the environment's)" : cache.c_str());
    }

    std::wstring cfg_error;
    if (!SelfTestOnly() && !bof3x::ConfigApplyGameCfg(game_dir, cfg, cfg_error))
        Die(L"%ls\n\nDisplay and renderer are set through that file, which is the game's "
            L"own input. Check that the game directory is writable.", cfg_error.c_str());
    bof3x::ConfigApplyEnvironment(game_dir, cfg, hosted);   // hosted: no layer that build refuses

    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!hosted) {
        // Our own executable: the game directory as its current directory, as
        // BOF3.exe's (it opens DAT\, BGM\, SND\ and BOF3.exe itself by
        // relative path); nothing else to do from here - it maps the player's
        // data, binds its imports and starts the game on its own main thread.
        std::wstring line = L"\"" + ours + L"\"" + game_args;
        // Started by us: bof3x.exe started any other way hands over to us (exe_host.cpp HandOffToLauncher).
        SetEnvironmentVariableW(L"BOF3X_LAUNCHER", L"1");
        if (!CreateProcessW(ours.c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr, game_dir.c_str(), &si, &pi))
            Die(L"CreateProcess failed for %ls (%lu)", ours.c_str(), GetLastError());
        if (SelfTestOnly()) {   // as the hosted path: the pid, then the game process's exit code
            std::fwprintf(stderr, L"bof3x-launcher: self-test only, game pid %lu (bof3x.exe)\n", pi.dwProcessId);
            std::fflush(stderr);
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD code = 1;
            GetExitCodeProcess(pi.hProcess, &code);
            std::fwprintf(stderr, L"bof3x-launcher: self-test only, game process exit code %lu\n", code);
            ExitProcess(code);
        }
        std::wprintf(L"bof3x-launcher: started %ls (pid %lu)\n", ours.c_str(), pi.dwProcessId);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }

#if BOF3X_HOSTED
    std::wstring cmdline = L"\"" + exe + L"\"" + game_args;
    // Current directory = the game directory: BOF3.exe opens DAT\, BGM\ and SND\
    // by relative path.
    if (!CreateProcessW(exe.c_str(), cmdline.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED,
                        nullptr, game_dir.c_str(), &si, &pi))
        Die(L"CreateProcess failed for %ls (%lu)", exe.c_str(), GetLastError());

    // From here on a failure must not leave a suspended game process behind;
    // Die() ends it.
    g_child = pi.hProcess;
    if (SelfTestOnly()) {   // a hung self-test is killed by this pid, not by image name
        std::fwprintf(stderr, L"bof3x-launcher: self-test only, game pid %lu\n", pi.dwProcessId);
        std::fflush(stderr);
    }

    LoadDllInto(pi.hProcess, dll);

    if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1))
        Die(L"ResumeThread failed (%lu)", GetLastError());
    g_child = nullptr;

    std::wprintf(L"bof3x-launcher: started %ls (pid %lu) with %ls\n", exe.c_str(),
                 pi.dwProcessId, dll.c_str());
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
#else
    Die(L"the hosted build reached the player's launcher, which hands it to bof3x-launcher-dev.exe above");
#endif
}

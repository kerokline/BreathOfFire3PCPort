// bof3x-launcher: start the player's own BOF3.exe with bof3x.dll loaded.
//
//   bof3x-launcher [--game <dir containing BOF3.exe>] [--config|--no-config]
//                  [-- <args for the game>]
//
// The game directory is, in order: --game, the BOF3_GAME_DIR environment
// variable, the current directory, then ./bof3.
//
// The original file is never modified and never copied (CLAUDE.md rule 1): the
// process is created suspended, a remote thread runs LoadLibraryW on our DLL,
// and only then is the game's main thread resumed - so every detour is in place
// before the entry point runs. Technique as described in
// docs/prior-art/tr1x.md section 2.1; implemented from that description.
//
// Must be built 32-bit, like the DLL: the remote thread's start address is OUR
// kernel32!LoadLibraryW, which is only the target's too when both processes
// load the same (32-bit) kernel32 - true within one boot session.
#include <windows.h>

#include <bcrypt.h>

#include "launcher/config.h"
#include "launcher/config_dialog.h"
#include "input/pad_sdl.h"

#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

namespace {

// fixtures.toml build "pc-zh" / symbols.toml [meta].sha256.
constexpr char kExpectedSha256[] =
    "15984158e0badb4b2f9471c1e9003bb18b2ab11aff47af2376f9ebcd31dfb8cb";

// The suspended game process, while there is one that has not been resumed.
HANDLE g_child = nullptr;

// BOF3X_SELFTEST_ONLY: the DLL ends the process once every inject and
// start-up self-test has run, before the game's main thread is resumed - no
// window, no dialogs; the launcher's exit code is the game process's (0 all
// passed, 3 a Fatal). Lets several checkouts self-test at once
// (docs/SCAFFOLDING.md section 2).
bool SelfTestOnly() { return GetEnvironmentVariableW(L"BOF3X_SELFTEST_ONLY", nullptr, 0) > 0; }

// Every failure path: ends a suspended game process if there is one, reports
// on stderr and in a message box, and exits 1.
[[noreturn]] void Die(const wchar_t* fmt, ...) {
    if (g_child) TerminateProcess(g_child, 1);
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vswprintf(buf, 1024, fmt, ap);
    va_end(ap);
    std::fwprintf(stderr, L"bof3x-launcher: %ls\n", buf);
    MessageBoxW(nullptr, buf, L"bof3x-launcher", MB_OK | MB_ICONERROR);
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

// The file's SHA-256 as lowercase hex, the form kExpectedSha256 is written in.
// Any I/O or CNG failure Die()s.
std::string Sha256Hex(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) Die(L"cannot open %ls (%lu)", path.c_str(), GetLastError());

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) < 0)
        Die(L"cannot initialise SHA-256");

    std::vector<unsigned char> buf(1 << 16);
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(file, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr))
            Die(L"read error on %ls (%lu)", path.c_str(), GetLastError());
        if (got == 0) break;
        if (BCryptHashData(hash, buf.data(), got, 0) < 0) Die(L"SHA-256 update failed");
    }
    unsigned char digest[32];
    if (BCryptFinishHash(hash, digest, sizeof digest, 0) < 0) Die(L"SHA-256 finish failed");
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(file);

    static const char kHex[] = "0123456789abcdef";
    std::string hex;
    for (unsigned char b : digest) {
        hex += kHex[b >> 4];
        hex += kHex[b & 15];
    }
    return hex;
}

// Runs LoadLibraryW(dll) inside `process` and returns only once it has.
void LoadDllInto(HANDLE process, const std::wstring& dll) {
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

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring game_dir;
    std::wstring game_args;
    // Neither flag given: the settings file decides, and its default is to
    // show the dialog. --config forces it (the way back after "Show this
    // window every time" is unticked); --no-config suppresses it, which is
    // what the scripted runs in docs/HANDOFF.md want.
    int want_dialog = -1;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--game" && i + 1 < argc) {
            game_dir = argv[++i];
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
                L"[--config|--no-config] [-- <game args>]",
                arg.c_str());
        }
    }

    if (game_dir.empty()) {
        wchar_t env[MAX_PATH];
        DWORD n = GetEnvironmentVariableW(L"BOF3_GAME_DIR", env, MAX_PATH);
        if (n > 0 && n < MAX_PATH) game_dir = env;
    }
    if (game_dir.empty()) game_dir = FileExists(L"BOF3.exe") ? L"." : L"bof3";
    game_dir = FullPath(game_dir);

    std::wstring exe = game_dir + L"\\BOF3.exe";
    if (!FileExists(exe))
        Die(L"no BOF3.exe in %ls.\nPoint --game or BOF3_GAME_DIR at your own install.",
            game_dir.c_str());

    std::wstring dll = OwnDirectory() + L"\\bof3x.dll";
    if (!FileExists(dll)) Die(L"%ls is missing; it belongs next to this launcher.", dll.c_str());

    std::string sha = Sha256Hex(exe);
    if (sha != kExpectedSha256)
        Die(L"%ls is not the build this project targets.\n\nsha256   %hs\nexpected %hs\n\n"
            L"Every address bof3x patches is specific to that one file; refusing to start. "
            L"If this is an uncatalogued release, see fixtures.toml.",
            exe.c_str(), sha.c_str(), kExpectedSha256);

    // Settings. The file lives next to the launcher, not in the game directory,
    // which stays the player's own (CLAUDE.md rule 1) apart from BOF3.CFG -
    // and that one is the original program's own documented input, not a patch.
    const std::wstring ini = OwnDirectory() + L"\\bof3x.ini";
    bof3x::Config cfg;
    if (!bof3x::ConfigLoad(ini, cfg)) bof3x::ConfigSeedFromGameCfg(game_dir, cfg);

    if (want_dialog == 1 || (want_dialog == -1 && cfg.show_launcher)) {
        // The pad, for the dialogs' navigation and the Controls capture; stopped
        // before the game starts, which opens it for itself (DIV-0050).
        bof3x::input::PadSdl_Start(cfg.bindings.layout, nullptr);
        const bool play = bof3x::ConfigDialogRun(game_dir, cfg);
        bof3x::input::PadSdl_Stop();
        if (!play) return 0;   // closed: start nothing
        if (!bof3x::ConfigSave(ini, cfg))
            std::fwprintf(stderr, L"bof3x-launcher: cannot write %ls; settings not saved\n",
                          ini.c_str());
    }

    std::wstring cfg_error;
    if (!SelfTestOnly() && !bof3x::ConfigApplyGameCfg(game_dir, cfg, cfg_error))
        Die(L"%ls\n\nDisplay and renderer are set through that file, which is the game's "
            L"own input. Check that the game directory is writable.", cfg_error.c_str());
    bof3x::ConfigApplyEnvironment(cfg);

    std::wstring cmdline = L"\"" + exe + L"\"" + game_args;
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
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
}

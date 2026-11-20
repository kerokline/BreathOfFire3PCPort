// Windows-only by design (docs/linux-round.md, LB1): the launcher is a Windows program (section 5).
// bof3x-launcher: the settings, then the game - our own bof3x.exe by default,
// or the player's own BOF3.exe with bof3x.dll injected (--host bof3).
// This file is the window half: the windows, the dialogs, the hosted
// injection and ForwardToDevLauncher. The steps with no window between them
// (the game folder, bof3x.ini, the environment, the start of bof3x) are
// launcher/start.cpp's, which the windowless launcher (headless.cpp) shares
// (docs/launcher-portable.md, stage P0).
//
//   bof3x-launcher [--game <dir containing BOF3.exe>] [--config|--no-config]
//                  [--host exe|bof3] [-- <args for the game>]
//
// The game directory is, in order: --game, the BOF3_GAME_DIR environment
// variable, the current directory, the home's game\ (common/home.h), then
// ./bof3. With none of them an install and no cache, the Sources page offers
// to make one from the player's own copy of the game
// (docs/install-sources.md); a game folder with no image of .data is set up
// by the same page before its first start.
//
// --host exe (the default, docs/state3-design.md section 8): bof3x.exe, beside
// this launcher, started with the game directory as its current directory -
// no suspension, no reservation, no remote thread; the executable imports
// bof3x.dll and maps the player's data itself (src/hook/exe_host.cpp).
// No BOF3.exe is read there (docs/exe-retire-round.md, X2): .data is the
// cache's image, whatever built it - the launcher sets BOF3X_EXEIMAGE to the
// settings' cache unless the environment already does. A game folder with no
// image is set up first through the Sources page (start.h StartSetUpFor,
// docs/exe-retire-round.md X3); under --no-config or a self-test it is
// refused (start.cpp StartRefuseNoImage). Only the developer's
// BOF3X_EXEFILE=1 has the engine read the file.
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
#include "common/sha256.h"
#include "launcher/config.h"
#include "launcher/config_dialog.h"
#include "launcher/exe_image.h"
#include "launcher/sources_dialog.h"
#include "launcher/start.h"
#include "game/draw_pool_room.h"
#include "os/process.h"
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
bool SelfTestOnly() { return bof3x::StartSelfTestOnly(); }

// The switches that need Capcom's code, which only the hosted build maps
// (src/hook/exe_host.cpp RefuseHostedSwitches).
bool HostedOnlyAsked() {
    for (const wchar_t* name : {L"BOF3X_ORIGINAL", L"BOF3X_SHADOW", L"BOF3X_CALLTRACE", L"BOF3X_MP3_COMPARE"})
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

// The start's own (launcher/start.cpp): the steps with no window that this
// file runs between its windows.
bool FileExists(const std::wstring& path) { return bof3x::StartFileExists(path); }
std::wstring FullPath(const std::wstring& path) { return bof3x::StartFullPath(path); }
std::wstring OwnDirectory() { return bof3x::StartOwnDirectory(); }

// Until 2026-10-15 the game wrote bof3x.exe.new with an icon generated from
// the game's data and the bof3x.icon stamp, and the launcher moved the new
// file over bof3x.exe here (DIV-0095). Rewriting an executable and running it
// is what Windows Defender took for defence evasion, so bof3x.exe now carries
// our own icon from the build and is never rewritten. What an older build left
// behind beside it is deleted; nothing of it is used.
void RemoveOldIconFiles() {
    for (const wchar_t* name : {L"\\bof3x.exe.new", L"\\bof3x.icon", L"\\bof3x.icon-work.exe"}) {
        const std::wstring path = OwnDirectory() + name;
        if (FileExists(path) && DeleteFileW(path.c_str()))
            std::fwprintf(stderr, L"bof3x-launcher: %ls, an older build's icon file, deleted (bof3x.exe keeps its own icon)\n",
                          path.c_str());
    }
}

#if BOF3X_HOSTED
// BOF3X_IMAGEBASE (docs/relocatable-image-design.md 1.5): the moved
// executable's fence, laid while the child is suspended - its image, ntdll
// and its first thread's stack mapped, nothing of its heaps or mapped files
// yet. Every free 64 KB-aligned part of the linked span 0x400000..SizeOfImage
// (the span bof3x.exe holds at its own base, read from the child's own header
// at `base`, as the engine's Fence reads it - exe_host.cpp LinkedEnd - so a
// section make_exe.py grows is fenced too) is reserved no-access for the life
// of the process; the engine's own fence counts these and takes what is still
// free at Engine_Main.
void FenceLinkedSpan(HANDLE process, std::uint32_t base) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS32 nt{};
    if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(base)), &dos, sizeof dos, nullptr) ||
        dos.e_magic != IMAGE_DOS_SIGNATURE ||
        !ReadProcessMemory(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(base + dos.e_lfanew)), &nt, sizeof nt,
                           nullptr) ||
        nt.Signature != IMAGE_NT_SIGNATURE)
        Die(L"BOF3X_IMAGEBASE: no PE header at 0x%08X in the game process (%lu)", base, GetLastError());
    const std::uint32_t kLo = 0x400000, kHi = kLo + nt.OptionalHeader.SizeOfImage;
    std::uint32_t fenced = 0, taken = 0;
    for (std::uint32_t a = kLo; a < kHi;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQueryEx(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(a)), &m, sizeof m))
            Die(L"BOF3X_IMAGEBASE: VirtualQueryEx(0x%08X) failed (%lu)", a, GetLastError());
        const auto lo = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m.BaseAddress));
        std::uint32_t hi = lo + static_cast<std::uint32_t>(m.RegionSize);
        if (hi > kHi) hi = kHi;
        const std::uint32_t from = (a + 0xFFFF) & ~0xFFFFu;
        if (m.State == MEM_FREE && from < hi) {
            if (!VirtualAllocEx(process, reinterpret_cast<LPVOID>(static_cast<std::uintptr_t>(from)), hi - from, MEM_RESERVE,
                                PAGE_NOACCESS))
                Die(L"BOF3X_IMAGEBASE: cannot reserve 0x%08X..0x%08X in the game process (%lu)", from, hi, GetLastError());
            fenced += hi - from;
        } else if (m.State != MEM_FREE) {
            taken += hi - a;
        }
        a = hi;
    }
    std::fwprintf(stderr, L"bof3x-launcher: BOF3X_IMAGEBASE: 0x%X bytes of 0x%08X..0x%08X reserved no-access in the "
                          L"suspended game process, 0x%X already held\n",
                  fenced, kLo, kHi, taken);
}

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
        Die(L"--host bof3, the hosted-only switches (BOF3X_ORIGINAL, BOF3X_SHADOW, BOF3X_CALLTRACE, BOF3X_MP3_COMPARE, "
            L"BOF3X_MP3=original) and a moved image (BOF3X_IMAGEBASE) are the developer launcher's, bof3x-launcher-dev.exe, which a source build "
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

#if BOF3X_HOSTED
std::uint32_t g_moved_base = 0;   // BOF3X_IMAGEBASE's base, for the fence

// The developer launcher's moved image (BOF3X_IMAGEBASE): the suspended
// child fenced before it runs (launcher/start.h StartGame). From here a
// failure must not leave a suspended game process behind; Die() ends it.
void FenceSuspended(bof3::os::Child& child) {
    g_child = reinterpret_cast<HANDLE>(child.process);
    FenceLinkedSpan(g_child, g_moved_base);
}
#endif

}  // namespace

// The start's refusals (launcher/start.h): Die's own, a message box included.
[[noreturn]] void bof3x::StartDie(const std::wstring& message) { Die(L"%ls", message.c_str()); }

int wmain(int argc, wchar_t** argv) {
    AttachParentConsole();
    std::wstring game_dir;
    std::vector<std::wstring> game_args;
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
            for (++i; i < argc; ++i) game_args.push_back(argv[i]);
        } else {
            Die(L"unknown argument: %ls\nusage: bof3x-launcher [--game <dir>] "
                L"[--config|--no-config] [--host exe|bof3] [-- <game args>]",
                arg.c_str());
        }
    }

    // The game directory: --game, BOF3_GAME_DIR, then StartFindGame: the
    // current directory when it is a game folder (the developer's start: when
    // it holds BOF3.exe), the home's game\ when the Sources page laid one out
    // (common/home.h, docs/install-sources.md), the PC folder the home plays
    // in place, else ./bof3.
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
    // fuzz, the call trace, Capcom's MP3 decoder and its comparison; bof3x.exe refuses each), so
    // the documented self-test and A/B commands keep their meaning.
    const bool hosted = host_asked >= 0 ? host_asked == 1 : HostedOnlyAsked();
#if !BOF3X_HOSTED
    if (hosted) ForwardToDevLauncher(argc, argv);
#endif
    if (host_asked < 0 && hosted)
        std::fwprintf(stderr, L"bof3x-launcher: a hosted-only switch is set (BOF3X_ORIGINAL, BOF3X_SHADOW, "
                              L"BOF3X_CALLTRACE, BOF3X_MP3_COMPARE or BOF3X_MP3=original): starting the hosted build, as --host bof3\n");
    // BOF3X_IMAGEBASE (a developer switch, docs/relocatable-image-design.md
    // 1.5; no ini key): our executable written at another base by
    // tools/make_exe.py --image-base, bof3x-<BASE>.exe beside this launcher
    // (the build writes bof3x-A00000.exe). 0x400000 is bof3x.exe itself.
    // BOF3.exe cannot move: the hosted build refuses the switch. The game
    // inherits the variable and checks its own base against it (host.cpp).
    //
    // A moved executable is the developer launcher's to start: before the
    // game runs, it reserves the free parts of the linked span
    // 0x400000..0x98F000 in the new process no-access (FenceLinkedSpan), so an
    // address the source left absolute faults at its own VA rather than
    // reading the heap or a mapped file that would otherwise land there
    // (measured 2026-10-28: with bof3x.exe at 0xA00000 the process's own early
    // allocations hold all but 64 KB of the span by Engine_Main). The player's
    // launcher writes nothing into another process, so it hands the switch on.
    std::wstring ours = OwnDirectory() + L"\\bof3x.exe";
    [[maybe_unused]] bool moved = false;   // the developer launcher's suspended start (BOF3X_HOSTED)
    [[maybe_unused]] std::uint32_t moved_base = 0;   // the developer launcher's fence (BOF3X_HOSTED)
    if (wchar_t base_text[32]; GetEnvironmentVariableW(L"BOF3X_IMAGEBASE", base_text, 32) > 0) {
        wchar_t* end = nullptr;
        const unsigned long base = std::wcstoul(base_text, &end, 0);
        if (!end || *end || end == base_text) Die(L"BOF3X_IMAGEBASE=%ls is not a number", base_text);
        // Hosted, 0x400000 is where BOF3.exe is (DetectHost accepts it): an
        // A/B harness may set it on both sides and then ask a hosted-only switch.
        if (hosted && base != 0x400000)
            Die(L"BOF3X_IMAGEBASE=%ls: BOF3.exe is fixed at 0x400000; only bof3x.exe moves (--host exe)", base_text);
#if !BOF3X_HOSTED
        if (base != 0x400000) ForwardToDevLauncher(argc, argv);
#endif
        moved = base != 0x400000;
        moved_base = base;
        if (base != 0x400000) {
            wchar_t name[32];
            std::swprintf(name, 32, L"\\bof3x-%lX.exe", base);
            ours = OwnDirectory() + name;
            if (!FileExists(ours))
                Die(L"BOF3X_IMAGEBASE=%ls: %ls is missing (tools/make_exe.py --image-base 0x%lX writes it)", base_text,
                    ours.c_str(), base);
        }
        std::fwprintf(stderr, L"bof3x-launcher: BOF3X_IMAGEBASE=%ls: starting %ls\n", base_text, ours.c_str());
    }
    if (!hosted && !FileExists(ours))
        Die(L"%ls is missing; it belongs next to this launcher (or start the hosted build with --host bof3).",
            ours.c_str());
    RemoveOldIconFiles();

    // Settings. The file lives next to the launcher, not in the game directory,
    // which stays the player's own (CLAUDE.md rule 1) apart from BOF3.CFG -
    // and that one is the original program's own documented input, not a patch.
    const std::wstring ini = OwnDirectory() + L"\\bof3x.ini";
    // From here to the settings dialog it may all run again: the Sources page
    // (docs/install-sources.md) can lay out a new game folder and cache, so
    // both are found again, and the settings read again, after it builds.
    std::wstring exe;
    bool exe_read = false;
    bof3x::Config cfg;
    for (bool again = true; again;) {
        again = false;
        exe_read = bof3x::StartFindGame(named_game, hosted, &game_dir, &exe);
        const bof3x::StartSetUp set_up =
            bof3x::StartReadSettings(ini, game_dir, named_game, hosted, exe_read, want_dialog, cfg);

        if (set_up != bof3x::StartSetUp::kNone) {
            // The Sources page first (start.h StartSetUpFor): its first run, or
            // this game folder set up in place - the page adds it as a source,
            // and the importer, not the launcher, says whether it is the PC
            // game; then the cache's image is built from it and the game
            // starts from the same folder.
            const bool built = bof3x::SourcesDialogRun(
                nullptr, cfg, true, false, set_up == bof3x::StartSetUp::kThisFolder ? game_dir : std::wstring());
            if (!bof3x::ConfigSave(ini, cfg))   // Locate ffmpeg's choice, kept whatever happened
                std::fwprintf(stderr, L"bof3x-launcher: cannot write %ls; settings not saved\n", ini.c_str());
            if (!built) return 0;   // closed without an install: start nothing
            cfg = bof3x::Config{};
            again = true;
            continue;
        }

        // HANDOFF item 11 (docs/install-staleness.md): the home's install against
        // this release's importer, piece by piece. Only the install the Sources
        // page made and can make again - its game\ and its cache\, no other cache
        // named - and never under a self-test or --no-config. Stale: the player
        // is asked; yes rebuilds the stale pieces (the rest kept), no starts nothing.
        if (!hosted && !SelfTestOnly() && want_dialog != 0 && named_game.empty() && cfg.cache.empty() &&
            GetEnvironmentVariableW(L"BOF3X_CACHE", nullptr, 0) == 0 &&
            GetEnvironmentVariableW(L"BOF3X_EXEIMAGE", nullptr, 0) == 0 &&
            !bof3x::HomeGameDefault().empty() && game_dir == FullPath(bof3x::HomeGameDefault())) {
            const bof3x::StaleCheck check = bof3x::SourcesCheckStale();
            if (!check.error.empty())
                std::fwprintf(stderr, L"bof3x-launcher: %ls\n", check.error.c_str());
            if (check.stale) {
                if (!bof3x::SourcesAskRebuild(check)) return 0;
                const bool built = bof3x::SourcesDialogRun(nullptr, cfg, false, true);
                if (!bof3x::ConfigSave(ini, cfg))
                    std::fwprintf(stderr, L"bof3x-launcher: cannot write %ls; settings not saved\n", ini.c_str());
                if (!built) return 0;   // stopped or failed: the install as it was, and nothing started
                cfg = bof3x::Config{};
                again = true;
                continue;
            }
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
            if (!bof3x::ConfigResolveLanguage(game_dir, cfg, why, hosted)) Die(L"%hs", why.c_str());
            if (!bof3x::ConfigSave(ini, cfg))
                std::fwprintf(stderr, L"bof3x-launcher: cannot write %ls; settings not saved\n",
                              ini.c_str());
        }
    }

    bof3x::StartApply(game_dir, cfg, hosted, exe_read);

    if (!hosted) {
        bool waited = false;
#if BOF3X_HOSTED
        g_moved_base = moved_base;
        const std::uint32_t code = bof3x::StartGame(ours, game_args, game_dir, moved ? FenceSuspended : nullptr, &waited);
        g_child = nullptr;
#else
        const std::uint32_t code = bof3x::StartGame(ours, game_args, game_dir, nullptr, &waited);
#endif
        if (waited) ExitProcess(code);
        return 0;
    }

#if BOF3X_HOSTED
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::wstring cmdline = L"\"" + exe + L"\"";
    for (const std::wstring& a : game_args) cmdline += L" \"" + a + L"\"";
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

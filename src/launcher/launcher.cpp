// Windows-only by design (docs/linux-round.md, LB1; docs/launcher-portable.md
// section 6): the developer launcher, bof3x-launcher-dev.exe. It is not a
// player's program and does not go to Linux.
//
// Since stage P6 of docs/launcher-portable.md (2026-11-20) the player's
// launcher is the drawn one (src/launcher/ui/main.cpp, bof3x-launcher on both
// systems), and this file has no window: what is left is the developer's
// start - the hosted build (BOF3.exe with bof3x.dll injected) and a moved
// image (bof3x-<BASE>.exe fenced while it is suspended). The player's launcher
// holds no code that writes into another process; it hands such a run to this
// one, beside it, with the same arguments, environment and redirected output,
// and ends with its exit code. A release does not ship this launcher.
//
//   bof3x-launcher-dev [--game <dir containing BOF3.exe>] [--no-config]
//                      [--host exe|bof3] [-- <args for the game>]
//
// It reads bof3x.ini beside it and starts the game as the player's launcher
// does with --no-config, always: no window, no Sources page, no stale-install
// check, and no message box - a refusal is said on stderr and the exit code is
// 1 (a box under a harness waits for ever: docs/HANDOFF.md, Traps). --config
// is refused with a sentence. The game directory is, in order: --game, the
// BOF3_GAME_DIR environment variable, then start.h StartFindGame. The steps
// with no window (the game folder, bof3x.ini, the environment, the start of
// bof3x) are launcher/start.cpp's, which the player's launcher shares.
//
// --host exe (the default, docs/state3-design.md section 8): bof3x.exe, beside
// this launcher, started with the game directory as its current directory;
// with BOF3X_IMAGEBASE other than 0x400000, bof3x-<BASE>.exe, created
// suspended and fenced (FenceLinkedSpan) before it runs. No BOF3.exe is read
// there (docs/exe-retire-round.md, X2): .data is the cache's image; only the
// developer's BOF3X_EXEFILE=1 has the engine read the file.
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

#include "launcher/config.h"
#include "launcher/start.h"
#include "game/draw_pool_room.h"
#include "os/process.h"

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
// (src/hook/exe_host.cpp RefuseHostedSwitches). The player's launcher asks the
// same (src/launcher/ui/main.cpp HostedOnlyAsked) to hand the run here.
bool HostedOnlyAsked() {
    for (const wchar_t* name : {L"BOF3X_ORIGINAL", L"BOF3X_SHADOW", L"BOF3X_CALLTRACE", L"BOF3X_MP3_COMPARE"})
        if (GetEnvironmentVariableW(name, nullptr, 0) > 0) return true;
    wchar_t mp3[16] = {};
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_MP3", mp3, 16);
    return n > 0 && n < 16 && std::wcscmp(mp3, L"original") == 0;
}

// Every failure path: ends a suspended game process if there is one, reports
// on stderr and exits 1. No message box: this launcher always runs as
// --no-config does (the header).
[[noreturn]] void Die(const wchar_t* fmt, ...) {
    if (g_child) TerminateProcess(g_child, 1);
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vswprintf(buf, 1024, fmt, ap);
    va_end(ap);
    std::fwprintf(stderr, L"bof3x-launcher: %ls\n", buf);
    ExitProcess(1);
}

// The start's own (launcher/start.cpp).
bool FileExists(const std::wstring& path) { return bof3x::StartFileExists(path); }
std::wstring OwnDirectory() { return bof3x::StartOwnDirectory(); }

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

std::uint32_t g_moved_base = 0;   // BOF3X_IMAGEBASE's base, for the fence

// The moved image (BOF3X_IMAGEBASE): the suspended child fenced before it
// runs (launcher/start.h StartGame). From here a failure must not leave a
// suspended game process behind; Die() ends it.
void FenceSuspended(bof3::os::Child& child) {
    g_child = reinterpret_cast<HANDLE>(child.process);
    FenceLinkedSpan(g_child, g_moved_base);
}

}  // namespace

// The start's refusals (launcher/start.h): Die's own.
[[noreturn]] void bof3x::StartDie(const std::wstring& message) { Die(L"%ls", message.c_str()); }

int wmain(int argc, wchar_t** argv) {
    // A windowed program started from a terminal writes its lines there.
    bof3::os::AttachParentTerminal();
    std::wstring game_dir;
    std::vector<std::wstring> game_args;
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
            Die(L"--config: the developer launcher has no window. It starts the game as --no-config does; "
                L"bof3x-launcher.exe shows the window.");
        } else if (arg == L"--no-config") {
            // the only way this launcher runs: accepted, so that a harness's command keeps its meaning
        } else if (arg == L"--") {
            for (++i; i < argc; ++i) game_args.push_back(argv[i]);
        } else {
            Die(L"unknown argument: %ls\nusage: bof3x-launcher-dev [--game <dir>] "
                L"[--no-config] [--host exe|bof3] [-- <game args>]",
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
    const std::wstring named_game = game_dir;

    std::wstring dll = OwnDirectory() + L"\\bof3x.dll";
    if (!FileExists(dll)) Die(L"%ls is missing; it belongs next to this launcher.", dll.c_str());
    // Not given: our own executable - unless the environment asks for what
    // only the hosted build has (Capcom's code: the A/B switch, the shadow
    // fuzz, the call trace, Capcom's MP3 decoder and its comparison; bof3x.exe refuses each), so
    // the documented self-test and A/B commands keep their meaning.
    const bool hosted = host_asked >= 0 ? host_asked == 1 : HostedOnlyAsked();
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
    // Before the game runs, this launcher reserves the free parts of the
    // linked span 0x400000..0x98F000 in the new process no-access
    // (FenceLinkedSpan), so an address the source left absolute faults at its
    // own VA rather than reading the heap or a mapped file that would
    // otherwise land there (measured 2026-10-28: with bof3x.exe at 0xA00000 the
    // process's own early allocations hold all but 64 KB of the span by
    // Engine_Main).
    std::wstring ours = OwnDirectory() + L"\\bof3x.exe";
    bool moved = false;
    std::uint32_t moved_base = 0;
    if (wchar_t base_text[32]; GetEnvironmentVariableW(L"BOF3X_IMAGEBASE", base_text, 32) > 0) {
        wchar_t* end = nullptr;
        const std::uint32_t base = static_cast<std::uint32_t>(std::wcstoul(base_text, &end, 0));
        if (!end || *end || end == base_text) Die(L"BOF3X_IMAGEBASE=%ls is not a number", base_text);
        // Hosted, 0x400000 is where BOF3.exe is (DetectHost accepts it): an
        // A/B harness may set it on both sides and then ask a hosted-only switch.
        if (hosted && base != 0x400000)
            Die(L"BOF3X_IMAGEBASE=%ls: BOF3.exe is fixed at 0x400000; only bof3x.exe moves (--host exe)", base_text);
        moved = base != 0x400000;
        moved_base = base;
        if (moved) {
            wchar_t name[32];
            std::swprintf(name, 32, L"\\bof3x-%X.exe", base);
            ours = OwnDirectory() + name;
            if (!FileExists(ours))
                Die(L"BOF3X_IMAGEBASE=%ls: %ls is missing (tools/make_exe.py --image-base 0x%X writes it)", base_text,
                    ours.c_str(), base);
        }
        std::fwprintf(stderr, L"bof3x-launcher: BOF3X_IMAGEBASE=%ls: starting %ls\n", base_text, ours.c_str());
    }
    if (!hosted && !FileExists(ours))
        Die(L"%ls is missing; it belongs next to this launcher (or start the hosted build with --host bof3).",
            ours.c_str());

    // Settings. The file lives next to the launcher, not in the game directory,
    // which stays the player's own (CLAUDE.md rule 1) apart from BOF3.CFG -
    // and that one is the original program's own documented input, not a patch.
    // As --no-config: no Sources page first (StartSetUpFor is kNone for it), no stale check.
    const std::wstring ini = OwnDirectory() + L"\\bof3x.ini";
    std::wstring exe;
    bof3x::Config cfg;
    const bool exe_read = bof3x::StartFindGame(named_game, hosted, &game_dir, &exe);
    bof3x::StartReadSettings(ini, game_dir, named_game, hosted, exe_read, 0, cfg);

    bof3x::StartApply(game_dir, cfg, hosted, exe_read);

    if (!hosted) {
        bool waited = false;
        g_moved_base = moved_base;
        const std::uint32_t code = bof3x::StartGame(ours, game_args, game_dir, moved ? FenceSuspended : nullptr, &waited);
        g_child = nullptr;
        if (waited) ExitProcess(code);
        return 0;
    }

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
}

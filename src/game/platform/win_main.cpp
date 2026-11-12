// The window and the frame loop, replaced (docs/window-modes.md). Five of
// Capcom's functions are taken over, read whole on 2026-09-23:
//
//   Game_WinMain          0x4FCB00..0x4FD02A (0x52A bytes)  int __stdcall (hinstance, hprev, cmdline, show)
//   Game_WndProc          0x4FC6F0..0x4FCAB4 (0x3C4 bytes)  long __stdcall (hwnd, msg, wparam, lparam)
//   Cursor_Sync           0x4FCAC0..0x4FCAFD (0x3D bytes)   void (void)
//   Display_WindowMoved   0x5A5130..0x5A515E (0x2E bytes)   void (int x, int y)
//   Display_DeviceName    0x5A6690..0x5A66A4 (0x14 bytes)   char *(int device)
//
// WinMain is the program: the disc probe, the window class and window, the
// two FMVs, the display set-up, and then the frame loop that paces logic
// frames at 30 Hz against a deadline kept in a 32-bit float (docs/windowed-mode.md,
// known-defects D5, DIV-0022), renders when it is early and skips when it is
// late, and runs the four cooperative tasks. WndProc is the keyboard's
// function keys (F7 renderer, F8 fullscreen, F9 pause, F11 frame rate, F12 a
// quick save to slot 0 - F7 and F11 are nothing since DIV-0040), the activation that freezes the loop, and the screensaver and
// suspend refusals. Both are written to the original's order, call for call,
// so that a frame of ours asks Capcom's code for exactly what a frame of the
// original did (the frame hash is the check), with these differences:
//
// DIVERGENCE DIV-0032 (docs/DIVERGENCE.md): the window modes. Windowed is a
// resizable WS_OVERLAPPEDWINDOW with a 640 x 480 client to start; fullscreen
// (Cfg_Fullscreen, BOF3.CFG line 1, F8) is a borderless WS_POPUP window the
// size of its monitor, and no exclusive display mode is ever set. F8 changes
// the window's style and size and nothing else - no Display_Teardown, no
// Display_Setup. (F7 cycled the device name without re-making the display
// until DIV-0040 made it nothing.) The three on-screen overlays (F7's device name, F11's frame
// rate, F12's Save OK) were TextOut through the back buffer's GDI device
// context, which the backend's surface does not have: they go to the log.
// The saved F8 restores the windowed size and position the player last had.
//
// DIVERGENCE DIV-0033: the game keeps running while its window is not in
// front (IDEAS I12). App_Active stays set on WM_ACTIVATEAPP deactivation,
// the sound is not paused, and while the window is not the foreground
// application the pad words are zeroed after Input_Latch (the DirectInput
// keyboard is DISCL_BACKGROUND: it reads keys typed into other windows).
// BOF3X_BACKGROUND=0 restores the freeze - and its replay.
//
// DIVERGENCE DIV-0034: frame debt is clamped. When the deadline is more
// than kMaxDebtMs behind the clock (a window drag, a suspend, the F9 pause
// under BOF3X_BACKGROUND=0 and the freeze it keeps), the deadline restarts
// from now as at the loop's start, so the time away is dropped rather than
// replayed as unrendered logic frames.
//
// Every original quirk not covered by those three is kept: the frame
// deadline in a float with its 2^32 wrap, Sound_Tick on every spin of the
// wait, the fps string once a second, the pause lines, F9's second press
// quitting to the title in game and the program on the title, WM_MOVE's
// window rect for Capcom's Blt, the F10 / Alt swallow, the screensaver and
// suspend refusals, and the task stack base taken from this function's own
// frame (Task_SetStackBase reads esp: the task stacks lie 16 KB below the
// loop, which is why the loop's locals stay small).
#include "game/win_main.h"

#include "audio/mp3.h"

#include "game/game_clock.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "game/display_setup.h"
#include "game/msgbox.h"
#include "game/pause_text.h"
#include "game/quick_menu.h"
#include "hook/cpu_context.h"
#include "hook/detour.h"
#include "hook/exe_icon.h"
#include "hook/exe_host.h"
#include "hook/exe_image.h"
#include "hook/input_script.h"
#include "hook/run_speed.h"
#include "hook/snapshot.h"
#include "hook/stallwatch.h"
#include "hook/log.h"
#include "render/render_d3d11.h"
#include "render/render_shim.h"
#include "os/os.h"

namespace {

using U = std::uint32_t;

}  // namespace
namespace win_main { bool g_background = true; }     // DIV-0033 on; BOF3X_BACKGROUND=0 turns it off
namespace win_main { bool g_foreground = true; }  namespace {     // the last WM_ACTIVATEAPP's word
}  namespace win_main { bool g_loop_ours = false; }  namespace {     // set by Game_WinMain: WinMain_InputAllowed answers true until then

}  // namespace
// DIVERGENCE DIV-0152: the Western discs' pad reset. Every US and European
// boot EXE tests the held word for Start + Select (0x900) in its frame loop
// (SLUS_004.22 0x8014ABB8..0x8014AC3C; the SLES 0x8014ABCC); the JP disc's,
// which the PC was built from, has no such test, and neither has BOF3.exe.
// Held 60 frames - a release on any frame starts the count again - with the
// loader idle (0x80162D00), the US restarts task 0. The PC's loads are
// synchronous (File_LoadDone is mov eax, 1), so the wait is always met; the
// restart is the PC's own, F9's (Game_RestartFlag, read by the loop). 1
// through PatchBytes("PadReset") in WinMain_Inject.
namespace win_main { std::uint8_t g_pad_reset = 0; }  // DIV-0202: the second movie (LOGOS.AVI, the 2001 porting house's logos) is
// not played, whether or not the file is in the folder: every install starts
// with Capcom's logo alone. 1 through PatchBytes("CapcomMovieOnly") in
// WinMain_Inject; BOF3X_OFF=CapcomMovieOnly plays a LOGOS.AVI that is there.
namespace win_main { std::uint8_t g_capcom_movie_only = 0; }
bool WinMain_InputAllowed() { return !win_main::g_loop_ours || !win_main::g_background || win_main::g_foreground; }
bool WinMain_Background() { return win_main::g_background; }

// --- BOF3X_EXITTRACE: who ends the process ---------------------------------
//
// Tooling. A run that ends without a CRASH line or a FATAL line left through
// the game's own code - a return from WinMain, the CRT's exit, a WM_QUIT -
// and nothing says from where. With BOF3X_EXITTRACE set, the exe's import
// slots for ExitProcess, TerminateProcess, PostQuitMessage and MessageBoxA
// are pointed at loggers that write the caller's return address and the
// first dozen return addresses found on the stack inside the image, then go
// on to the real import; and a last vectored handler logs any breakpoint or
// single step that no other handler took. Nothing of Capcom's is changed:
// the slots are the loader's.
namespace {

// kImpExitProcess: now Imp_ExitProcess (symbols.toml).
// kImpTerminateProcess: now Imp_TerminateProcess (symbols.toml).
// kImpPostQuitMessage: now Imp_PostQuitMessage (symbols.toml).
void (WINAPI* g_real_exit)(UINT);
BOOL (WINAPI* g_real_terminate)(HANDLE, UINT);
void (WINAPI* g_real_post_quit)(int);

void LogStack(const char* what, const void* frame) {
    char line[1024];
    int n = std::snprintf(line, sizeof line, "exittrace   %s from", what);
    const U* p = static_cast<const U*>(frame);
    int found = 0;
    for (int i = 0; i < 4096 && found < 12; ++i) {
        const U v = p[i];
        if (v >= ::bof3::Img(0x401000) && v < ::bof3::Img(0x5C4000)) {   // a return into the stub page: printed as its VA
            n += std::snprintf(line + n, sizeof line - static_cast<size_t>(n), " 0x%06X", ::bof3::VaOf(v));
            ++found;
        }
    }
    bof3::Log("%s", line);
    bof3::LogFlush();
}

void WINAPI TracedExitProcess(UINT code) {
    bof3::Log("exittrace   ExitProcess(0x%08X)", code);
    LogStack("ExitProcess", __builtin_frame_address(0));
    g_real_exit(code);
}

// The CRT's own __except in WinMainCRTStartup (0x5BA154) turns an exception
// nobody handled into _exit(code) with no dialog, and the crash reporter
// skips breakpoints and single steps, which are signals, not faults. This
// handler is called last, so it sees only what every other vectored handler
// passed on - the calltrace's among them.
LONG CALLBACK TracedUnhandled(EXCEPTION_POINTERS* info) {
    static volatile LONG reports = 0;
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_SINGLE_STEP && code != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&reports) > 4) return EXCEPTION_CONTINUE_SEARCH;
    // The context's registers by width (hook/cpu_context.h: read and printed
    // as wide as the register, so a system DLL's address above 4 GB on x86-64
    // shows whole; no #if here, a game file).
    bof3::Log("exittrace   exception 0x%08lX at 0x%s, esp 0x%s, eflags 0x%08lX, thread %lu, passed on by every handler",
              code, bof3::CtxHex(bof3::CtxIp(*info->ContextRecord)).text,
              bof3::CtxHex(bof3::CtxSp(*info->ContextRecord)).text,
              static_cast<unsigned long>(info->ContextRecord->EFlags), static_cast<unsigned long>(bof3::os::ThreadId()));
    LogStack("the exception", reinterpret_cast<const void*>(bof3::CtxSp(*info->ContextRecord)));
    return EXCEPTION_CONTINUE_SEARCH;
}
BOOL WINAPI TracedTerminateProcess(HANDLE h, UINT code) {
    LogStack("TerminateProcess", __builtin_frame_address(0));
    return g_real_terminate(h, code);
}
void WINAPI TracedPostQuitMessage(int code) {
    LogStack("PostQuitMessage", __builtin_frame_address(0));
    g_real_post_quit(code);
}
// kImpMessageBoxA: now PsxRest_ImportMessageBoxA (symbols.toml).
int (WINAPI* g_real_message_box)(HWND, const char*, const char*, UINT);
int WINAPI TracedMessageBoxA(HWND h, const char* text, const char* caption, UINT type) {
    LogStack("MessageBoxA", __builtin_frame_address(0));
    const int r = g_real_message_box(h, text, caption, type);
    bof3::Log("exittrace   MessageBoxA returned %d", r);
    return r;
}

// Written directly, not through PatchBytes: this is an instrument, and
// BOF3X_ORIGINAL=* must not switch it off (the RetargetCall `instrument`
// rule, HANDOFF Traps 2026-09-23).
template <typename F>
void PatchImport(U slot, F& real, F ours) {
    void* at = reinterpret_cast<void*>(static_cast<std::uintptr_t>(slot));
    std::uint32_t word;   // the slot is a 32-bit word on every width (a thunk's place on x86-64)
    std::memcpy(&word, at, 4);
    real = reinterpret_cast<F>(static_cast<std::uintptr_t>(word));
    bof3::os::ProtSave old;
    if (!bof3::os::MemProtect(at, 4, bof3::os::Prot::ReadWrite, &old)) bof3::Fatal("exittrace: MemProtect of the import slot 0x%08X failed", ::bof3::VaOf(slot));
    word = Low32(reinterpret_cast<const void*>(ours));
    std::memcpy(at, &word, 4);
    bof3::os::MemRestore(at, 4, old);
}

}  // namespace

void WinMain_Inject() {
    if (bof3::os::GetEnv("BOF3X_EXITTRACE", nullptr, 0) > 0) {
        PatchImport(::bof3::Img(Imp_ExitProcess_addr), g_real_exit, &TracedExitProcess);
        PatchImport(::bof3::Img(Imp_TerminateProcess_addr), g_real_terminate, &TracedTerminateProcess);
        PatchImport(::bof3::Img(Imp_PostQuitMessage_addr), g_real_post_quit, &TracedPostQuitMessage);
        PatchImport(::bof3::Img(PsxRest_ImportMessageBoxA_addr), g_real_message_box, &TracedMessageBoxA);
        if (!AddVectoredExceptionHandler(0, TracedUnhandled)) bof3::Fatal("exittrace: no vectored handler");
        bof3::Log("exittrace   ExitProcess, TerminateProcess, PostQuitMessage and MessageBoxA log their callers; "
                  "breakpoints and single steps no handler took are logged");
    }
    {
        // DIV-0152: Start + Select held a second returns to the title.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("PadReset", Low32(&win_main::g_pad_reset), &was, &is, 1);
        bof3::Log("DIV-0152    Start + Select held 60 frames returns to the title, as on the Western discs (on unless the line above says OFF)");
    }
    {
        // DIV-0202: Capcom's logo alone before the title.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("CapcomMovieOnly", Low32(&win_main::g_capcom_movie_only), &was, &is, 1);
        bof3::Log("DIV-0202    the start plays CAPCOM.AVI alone: %s",
                  win_main::g_capcom_movie_only ? "on" : "OFF (LOGOS.AVI plays if it is in the folder)");
    }
}

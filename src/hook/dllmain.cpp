// bof3x.dll entry point. Loaded into BOF3.exe by bof3x-launcher while the
// process is still suspended, so everything here runs before the game's entry
// point 0x5BA057 and no game thread can be inside a function we patch.
//
// Order: the log first (Fatal writes to it), the image check before anything
// that trusts an address, and the tracer after InjectAll, which it needs to
// know what is owned (calltrace.h). BOF3X_SELFTEST_ONLY ends the
// process after the injects, and Fatal skips its message box.
#include <windows.h>

#include "hook/calltrace.h"
#include "hook/crash.h"
#include "hook/detour.h"
#include "hook/inject_all.h"
#include "hook/input_script.h"
#include "hook/log.h"

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        bof3::LogOpen(module);
        bof3::Log("bof3x attached to process %lu", GetCurrentProcessId());
        bof3::VerifyImage();
        bof3::Crash_Start(module);
        bof3::InjectAll();
        if (GetEnvironmentVariableA("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0) {
            // Every inject and start-up self-test has run; the game never starts.
            bof3::Log("self-test only: done, ending the process");
            bof3::LogClose();
            TerminateProcess(GetCurrentProcess(), 0);
        }
        bof3::CallTrace_Start(module);
        bof3::InputScript_Start();
    } else if (reason == DLL_PROCESS_DETACH) {
        bof3::Log("bof3x detaching");
        bof3::LogClose();
    }
    return TRUE;
}

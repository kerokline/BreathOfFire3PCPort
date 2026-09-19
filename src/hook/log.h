// Log file for the injected DLL: bof3x.log, next to bof3x.dll.
#pragma once

namespace bof3 {

void LogOpen(void* dll_module);
void LogClose();
// Force what has been logged onto disk. For the moments after which there may
// be no process left to do it.
void LogFlush();
void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Log, tell the player, and end the process. For states we refuse to run in:
// continuing after a failed patch means running a game that is half ours and
// half not, without knowing which half (CLAUDE.md rule 4).
[[noreturn]] void Fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace bof3

// One log line the first time control reaches this point, then nothing. The
// cheapest available proof that a replacement is the code actually running.
#define BOF3_LOG_FIRST_CALL(...)                     \
    do {                                             \
        static bool bof3_seen_ = false;              \
        if (!bof3_seen_) {                           \
            bof3_seen_ = true;                       \
            ::bof3::Log("first call  " __VA_ARGS__); \
        }                                            \
    } while (0)

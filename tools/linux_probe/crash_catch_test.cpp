// The Linux crash catcher proven without the game (docs/linux-round.md LB7,
// "B2b as run"): this program arms src/hook/crash_posix.cpp as the engine
// would, faults on purpose in the way its argument names, and leaves the
// report in crash_catch_test.log beside itself, for tools/crash_report.py --log.
//
//   tools/linux_probe/crash_catch_test.sh <gen> <out>     (builds it with g++ and runs every mode)
//   <out>/crash_catch_test <mode>
//
// <gen> holds bof3/symbols.gen.h (tools/gen_symbols.py, as tools/linux_probe/
// linux_tree.py makes it). The modes:
//
//   read write exec bus fpe ill abort trap selftest
//       each fault before the exe host's step 23: the process ends the way
//       the fault would have without the catcher, by its signal.
//   overflow
//       the main thread moved onto a placed stack with a no-access guard page
//       under it (os::RunOnPlacedStack, as exe_host_posix.cpp's row 3), the
//       catcher armed there (as step 16), then a recursion with no end: the
//       report from the alternate stack, its line naming the guard page, and
//       the end by SIGSEGV.
//   unhandled, overflow_unhandled, trap_unhandled
//       read, overflow and trap after Crash_EndUnhandledAsFatal (step 23):
//       the CRASH lines (for the trap, the exittrace line), then Fatal's line
//       "a signal nobody handled" and exit code 3.
//
// The doubles below stand in for the engine's start-up: the image's base
// (host.cpp SetImageBase; the start-up's variable off Windows, image_base.h),
// the ELF's own base (exe_host_posix.cpp step 5), Capcom's code range
// (host.cpp), the bind table (bind.cpp), the state hash (statehash.cpp). The
// image itself is not mapped, so the report's reads of the game's words find
// nothing, as after a crash that took them.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "common/image_base.h"
#include "hook/crash.h"
#include "hook/log.h"
#include "os/os.h"

namespace bof3 {
namespace image {
std::uint32_t g_runBase = 0x400000;   // the player's base, as SetImageBase leaves it
std::uintptr_t g_ownBase = 0;         // main() sets it: the program's own base, as step 5 does
[[noreturn]] void ImageBaseUnset() { Fatal("ImageBase() read before the start-up set it"); }
}  // namespace image
std::uint32_t CodeLo() { return 0x401000; }
std::uint32_t CodeHi() { return 0x5C4000; }
bool Bind_Active() { return false; }
const char* Bind_FuncName(std::uint32_t, std::uint32_t* start) {
    *start = 0;
    return nullptr;
}
void StateHash_Flush() {}
}  // namespace bof3

namespace {

[[gnu::noinline]] void Read() {
    volatile int* nowhere = reinterpret_cast<int*>(0x10);
    std::printf("%d\n", *nowhere);
}

[[gnu::noinline]] void WriteReadOnly() {
    void* p = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    *static_cast<volatile int*>(p) = 1;
}

[[gnu::noinline]] void ExecuteData() {
    void* p = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    std::memset(p, 0xC3, 4096);   // ret, were it executable
    reinterpret_cast<void (*)()>(p)();
}

[[gnu::noinline]] void Bus() {
    char name[] = "/tmp/crash_catch_test_XXXXXX";
    const int fd = mkstemp(name);
    unlink(name);
    if (fd < 0 || ftruncate(fd, 4096) != 0) std::abort();
    char* p = static_cast<char*>(mmap(nullptr, 8192, PROT_READ, MAP_SHARED, fd, 0));
    std::printf("%d\n", static_cast<volatile char*>(p)[4096]);   // the page past the file's end
}

[[gnu::noinline]] void DivideByZero() {
    volatile int zero = 0;
    std::printf("%d\n", 7 / zero);
}

void OnBreak(const char* what, std::uintptr_t ip, std::uintptr_t sp, std::uint32_t flags) {
    bof3::Log("exittrace   %s at 0x%016llX, esp 0x%016llX, eflags 0x%08X, passed on by every handler", what,
              static_cast<unsigned long long>(ip), static_cast<unsigned long long>(sp), flags);
    bof3::LogFlush();
}

// A recursion the compiler cannot make a loop: each frame keeps a buffer it
// reads after the call.
[[gnu::noinline]] int Deeper(int depth) {
    volatile char frame[512];
    frame[0] = static_cast<char>(depth);
    const int below = depth < 0x7FFFFFFF ? Deeper(depth + 1) : 0;   // never 0x7FFFFFFF: the guard comes first
    return below + frame[0];
}

// The placed stack's size: the game's (exe_host_posix.cpp kStackLo..kStackHi),
// a guard page under it. Mapped where the system puts it; the place does not
// matter to the catcher, only the guard does.
constexpr std::size_t kStack = 0x100000, kGuard = 0x1000;

struct Placed {
    bool unhandled;
};

void OnPlacedStack(void* p) {
    const Placed& run = *static_cast<const Placed*>(p);
    const bof3::os::StackRange s = bof3::os::CurrentStack();
    bof3::Log("crash_catch_test: on the placed stack 0x%llX..0x%llX",
              static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(s.limit)),
              static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(s.base)));
    bof3::Crash_Start(reinterpret_cast<void*>(&OnPlacedStack));
    if (run.unhandled) bof3::Crash_EndUnhandledAsFatal();
    std::printf("%d\n", Deeper(0));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s read|write|exec|bus|fpe|ill|abort|trap|selftest|overflow|unhandled|"
                             "overflow_unhandled|trap_unhandled\n", argv[0]);
        return 2;
    }
    bof3::os::ModuleImageInfo self;
    if (!bof3::os::ModuleImage(reinterpret_cast<const void*>(&main), &self)) return 2;
    bof3::image::g_ownBase = self.base;
    const std::string mode = argv[1];
    if (mode == "selftest") setenv("BOF3X_CRASH_TEST", "1", 1);
    // Fatal's message is the player's; this test reads the log.
    setenv("BOF3X_SELFTEST_ONLY", "1", 1);
    bof3::LogOpen(reinterpret_cast<void*>(&main));
    bof3::Log("crash_catch_test: mode %s", argv[1]);
    if (mode == "overflow" || mode == "overflow_unhandled") {
        char* const m = static_cast<char*>(
            mmap(nullptr, kStack + kGuard, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (m == MAP_FAILED || mprotect(m, kGuard, PROT_NONE) != 0) return 2;
        Placed run{mode == "overflow_unhandled"};
        if (!bof3::os::RunOnPlacedStack(m + kGuard, m + kGuard + kStack, &OnPlacedStack, &run)) return 2;
        bof3::Log("crash_catch_test: survived the overflow (a failure)");
        bof3::LogClose();
        return 1;
    }
    bof3::Crash_Start(reinterpret_cast<void*>(&main));
    if (mode == "unhandled" || mode == "trap_unhandled") bof3::Crash_EndUnhandledAsFatal();
    if (mode == "read" || mode == "unhandled") Read();
    else if (mode == "write") WriteReadOnly();
    else if (mode == "exec") ExecuteData();
    else if (mode == "bus") Bus();
    else if (mode == "fpe") DivideByZero();
    else if (mode == "ill") asm volatile("ud2");
    else if (mode == "abort") std::abort();
    else if (mode == "trap" || mode == "trap_unhandled") {
        if (!bof3::Crash_WatchBreaks(OnBreak)) return 3;
        asm volatile("int3");
    } else if (mode == "selftest") {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    } else {
        std::fprintf(stderr, "no mode %s\n", argv[1]);
        return 2;
    }
    bof3::Log("crash_catch_test: survived the fault (a failure)");
    bof3::LogClose();
    return 1;
}

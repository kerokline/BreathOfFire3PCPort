// The Linux crash catcher proven without the game (docs/linux-round.md LB7):
// this program arms src/hook/crash_posix.cpp as the engine would, faults on
// purpose in the way its argument names, and leaves the report in
// crash_catch_test.log beside itself, for tools/crash_report.py --log.
//
//   tools/linux_probe/crash_catch_test.sh <gen> <out>     (builds it with g++ and runs every mode)
//   <out>/crash_catch_test read|write|exec|bus|fpe|ill|abort|trap|selftest
//
// <gen> holds bof3/symbols.gen.h (tools/gen_symbols.py, as tools/linux_probe/
// linux_tree.py makes it). Each mode ends the process the way the fault would have
// without the catcher: by its signal.
//
// The doubles below stand in for the engine's start-up, which a Linux build
// does not have yet (stage LC): Capcom's code range (host.cpp), the bind
// table (bind.cpp), the state hash (statehash.cpp). And ImageBase() reads the
// image's base from the Windows PEB through gs:0x60 (common/image_base.h):
// this program gives itself a gs block whose PEB says 0x400000, as a Windows
// process at the linked base would. The image itself is not mapped, so the
// report's reads of the game's words find nothing, as after a crash that took
// them.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <asm/prctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "hook/crash.h"
#include "hook/log.h"

namespace bof3 {
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

std::uintptr_t g_peb[8];
std::uintptr_t g_gs[16];

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

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s read|write|exec|bus|fpe|ill|abort|trap|selftest\n", argv[0]);
        return 2;
    }
    g_peb[2] = 0x400000;   // PEB +0x10: ImageBaseAddress
    g_gs[0x60 / 8] = reinterpret_cast<std::uintptr_t>(g_peb);
    if (syscall(SYS_arch_prctl, ARCH_SET_GS, g_gs) != 0) {   // before any thread: a new thread keeps its maker's gs
        std::perror("arch_prctl");
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "selftest") setenv("BOF3X_CRASH_TEST", "1", 1);
    bof3::LogOpen(reinterpret_cast<void*>(&main));
    bof3::Log("crash_catch_test: mode %s", argv[1]);
    bof3::Crash_Start(reinterpret_cast<void*>(&main));
    if (mode == "read") Read();
    else if (mode == "write") WriteReadOnly();
    else if (mode == "exec") ExecuteData();
    else if (mode == "bus") Bus();
    else if (mode == "fpe") DivideByZero();
    else if (mode == "ill") asm volatile("ud2");
    else if (mode == "abort") std::abort();
    else if (mode == "trap") {
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

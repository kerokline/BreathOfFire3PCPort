// A probe for the native Linux build (docs/linux-round.md section 4, LA2): can an
// x86-64 Linux ELF hold the game's narrow image - the range 0x400000..0x98F000
// that bof3x.exe occupies on Windows (src/hook/exe_layout.h: .stub, .game, .pool,
// then our own sections to 0x98F000) - and the task stacks below 4 GB?
// Layout constants only; no game data. Not a player file.
//
// It reads /proc/sys/vm/mmap_min_addr, says where its own ELF is, then:
//   image    mmap(base, 0x58F000, MAP_FIXED_NOREPLACE), writes and reads every
//            page, makes the stub range RX and runs a stub there that jumps (a
//            rel32 near jmp, as detour.cpp writes) into this ELF's code - which
//            reaches only when the ELF lies within 2 GB of the stub
//   stacks   a 1 MB task-stack arena three ways (a fixed place below the image,
//            MAP_32BIT, a hint) and a switch of rsp onto each, saving the stack
//            pointer in a 32-bit word and returning through a reload of it
//   arena    a 1 GB low reserve (PROT_NONE, no commit) at 0x40000000, the way a
//            custom allocator at the Crt_malloc chokepoint would take it
//   heap     where malloc puts 1 MB, 17 MB and 48 blocks of 64 MB: by default,
//            with the mmap threshold raised to 32 MB, and with mmap off (break
//            heap only). Reported, not judged
//
// Build (Linux, x86-64):
//   clang++ -std=c++20 -O1 -no-pie -Wl,-Ttext-segment=0x10000000 tools/linux_probe/lowmem_probe.cpp -o lowmem_hi
//   clang++ -std=c++20 -O1 -no-pie                                tools/linux_probe/lowmem_probe.cpp -o lowmem_def
//   clang++ -std=c++20 -O1 -pie                                   tools/linux_probe/lowmem_probe.cpp -o lowmem_pie
// Run: ./lowmem_hi [image-base, default 0x400000]   (0xA00000 is the moved base)
// Exit 0 when the image, the stub jump, the arena and all three stack places
// worked. The heap lines inform the allocator choice; they do not fail the run.
#include <sys/mman.h>
#include <malloc.h>
#include <unistd.h>

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::uintptr_t kSpan = 0x98F000 - 0x400000;   // exe_layout.h: kOwnVa 0x987000 + our sections to 0x98F000
constexpr std::uintptr_t kStubOff = 0x401000 - 0x400000;   // where .stub starts in the image
constexpr std::uintptr_t kStubSize = 0x1C3000;           // exe_layout.h kStubSize
constexpr std::size_t kStackBytes = 0x100000;            // exe_layout.h kStackReserve
constexpr std::uint64_t k4G = 0x100000000ull;

int g_fail = 0;

void Report(const char* name, bool ok, const char* fmt = "", ...) __attribute__((format(printf, 3, 4)));
void Report(const char* name, bool ok, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::printf("%-34s %s  %s\n", name, ok ? "ok  " : "FAIL", buf);
    if (!ok) ++g_fail;
}

// The stub's jump target: an address in this ELF, so the rel32 distance is measured on the real layout.
extern "C" __attribute__((noinline)) int OurFunction(int x) { return x + 42; }

std::uint32_t g_saved_sp32;   // the task record's saved-sp word: 32 bits, as the game's records hold it
volatile int g_on_stack_ok;

// extern "C" so RunOnStack's asm can call it by name.
extern "C" __attribute__((noinline)) void OnLowStack() {
    char probe;
    g_on_stack_ok = reinterpret_cast<std::uintptr_t>(&probe) < k4G ? 1 : 0;
}

// Switch rsp to `top` (below 4 GB), keep the old rsp in a callee-saved register, call
// OnLowStack, switch back. The "saved" word round trip is the task switch's contract:
// a 32-bit store of rsp and a 32-bit load into rsp (a mov to esp zero-extends).
bool RunOnStack(std::uintptr_t top) {
    g_on_stack_ok = 0;
    g_saved_sp32 = static_cast<std::uint32_t>(top);   // the slot's word; Low32() would stop above 4 GB
    __asm__ volatile(
        "movq %%rsp, %%rbx\n\t"
        "movl %[sp32], %%esp\n\t"
        "andq $-16, %%rsp\n\t"
        "call OnLowStack\n\t"
        "movq %%rbx, %%rsp\n\t"
        :
        : [sp32] "m"(g_saved_sp32)
        : "rbx", "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory", "cc");
    return g_on_stack_ok == 1;
}

const char* ReadLine(const char* path, char* buf, size_t n) {
    FILE* f = std::fopen(path, "r");
    if (!f) return "(unreadable)";
    if (!std::fgets(buf, static_cast<int>(n), f)) buf[0] = 0;
    std::fclose(f);
    buf[std::strcspn(buf, "\n")] = 0;
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    std::uintptr_t base = 0x400000;
    if (argc > 1) base = std::strtoull(argv[1], nullptr, 0);
    char b[128];
    std::printf("mmap_min_addr            %s\n", ReadLine("/proc/sys/vm/mmap_min_addr", b, sizeof b));
    std::printf("randomize_va_space       %s\n", ReadLine("/proc/sys/kernel/randomize_va_space", b, sizeof b));
    std::printf("overcommit_memory        %s\n", ReadLine("/proc/sys/vm/overcommit_memory", b, sizeof b));
    std::printf("this ELF: main at %p, OurFunction at %p, brk %p\n", reinterpret_cast<void*>(&main),
                reinterpret_cast<void*>(&OurFunction), sbrk(0));
    std::printf("image base asked         0x%llX .. 0x%llX\n", static_cast<unsigned long long>(base),
                static_cast<unsigned long long>(base + kSpan));

    // ---- the image
    void* img = mmap(reinterpret_cast<void*>(base), kSpan, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    bool mapped = img != MAP_FAILED && reinterpret_cast<std::uintptr_t>(img) == base;
    Report("image mmap FIXED_NOREPLACE", mapped, "%s", mapped ? "mapped at the asked base" : std::strerror(errno));
    if (mapped) {
        auto* p = static_cast<volatile std::uint8_t*>(img);
        bool ok = true;
        for (std::uintptr_t o = 0; o < kSpan; o += 0x1000) p[o] = static_cast<std::uint8_t>(o >> 12);
        for (std::uintptr_t o = 0; o < kSpan; o += 0x1000) ok &= p[o] == static_cast<std::uint8_t>(o >> 12);
        Report("image write and read back", ok, "%llu pages", static_cast<unsigned long long>(kSpan / 0x1000));

        // a near jmp at the stub's first byte into OurFunction, as detour.cpp writes (rel32)
        auto* stub = reinterpret_cast<std::uint8_t*>(base + kStubOff);
        std::int64_t rel = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(&OurFunction)) -
                           static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(stub) + 5);
        bool fits = rel >= INT32_MIN && rel <= INT32_MAX;
        Report("stub jmp rel32 reaches the ELF", fits, "distance %+lld bytes", static_cast<long long>(rel));
        if (fits) {
            stub[0] = 0xE9;
            auto r32 = static_cast<std::int32_t>(rel);
            std::memcpy(stub + 1, &r32, 4);
            int ok_prot = mprotect(stub, kStubSize, PROT_READ | PROT_EXEC);   // RWX while written, then RX
            int (*f)(int) = reinterpret_cast<int (*)(int)>(stub);
            int v = ok_prot == 0 ? f(1) : -1;
            Report("stub (RX, low) jumps into the ELF", v == 43, "returned %d", v);
        }
    }

    // a low arena of our own: 1 GB reserved (no commit) at 0x40000000, as a custom allocator at the
    // Crt_malloc chokepoint would take it
    void* arena = mmap(reinterpret_cast<void*>(0x40000000), 1u << 30, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    Report("low arena 1 GB reserve", arena != MAP_FAILED && reinterpret_cast<std::uintptr_t>(arena) + (1u << 30) <= k4G, "%p",
           arena == MAP_FAILED ? nullptr : arena);
    if (arena != MAP_FAILED) munmap(arena, 1u << 30);   // only the reserve is asked here; the heap test follows

    // ---- the heap: where do the game-visible blocks go?
    auto where = [&](const char* name) {
        void* a = std::malloc(1u << 20);
        void* c = std::malloc(17u << 20);
        int below = 0, null = 0;
        std::uintptr_t first = 0, last = 0;
        static void* blocks[48];
        for (int i = 0; i < 48; ++i) {
            blocks[i] = std::malloc(64u << 20);
            if (!blocks[i]) {
                ++null;
                continue;
            }
            auto u = reinterpret_cast<std::uintptr_t>(blocks[i]);
            if (!first) first = u;
            last = u;
            if (u + (64u << 20) <= k4G) ++below;
        }
        std::printf("heap %-26s malloc(1M)=%p malloc(17M)=%p; 64M x48: %d below 4 GB, %d null, first %#llx last %#llx\n", name, a,
                    c, below, null, static_cast<unsigned long long>(first), static_cast<unsigned long long>(last));
        for (void* q : blocks) std::free(q);
        std::free(a);
        std::free(c);
    };
    where("default (glibc)");
    mallopt(M_MMAP_THRESHOLD, 32 << 20);   // glibc's own ceiling for the threshold (HEAP_MAX / 2)
    mallopt(M_TRIM_THRESHOLD, 1 << 30);   // the break heap keeps what the freed blocks leave at its top
    where("mmap threshold 32 MB");
    mallopt(M_MMAP_MAX, 0);
    where("M_MMAP_MAX 0 (break only)");
    // ---- the task stacks: where does a 1 MB arena go?
    struct Try {
        const char* name;
        void* addr;
    };
    void* st[3] = {MAP_FAILED, MAP_FAILED, MAP_FAILED};
    st[0] = mmap(reinterpret_cast<void*>(0x60000), kStackBytes, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    st[1] = mmap(nullptr, kStackBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    st[2] = mmap(reinterpret_cast<void*>(0x20000000), kStackBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    const char* names[3] = {"stack at 0x60000 (Windows' place)", "stack MAP_32BIT", "stack by hint 0x20000000"};
    for (int i = 0; i < 3; ++i) {
        bool low = st[i] != MAP_FAILED && reinterpret_cast<std::uintptr_t>(st[i]) + kStackBytes <= k4G;
        Report(names[i], low, "%p", st[i] == MAP_FAILED ? nullptr : st[i]);
        if (low) {
            std::uintptr_t top = reinterpret_cast<std::uintptr_t>(st[i]) + kStackBytes - 0x100;
            Report("  rsp switched onto it, 32-bit sp", RunOnStack(top), "top word 0x%08X", static_cast<unsigned>(top));
        }
    }

    std::printf("result: %s (%d failed)\n", g_fail ? "FAIL" : "ok", g_fail);
    return g_fail ? 1 : 0;
}

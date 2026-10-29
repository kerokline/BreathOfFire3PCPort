// A 64-bit probe for docs/x64-build-round.md section 0: can a 64-bit process on
// this machine map the game image at 0x400000, what is free below 16 MB (the
// ordering table links are 24-bit), and a low heap at 0x1000000. Build and run:
//   x86_64-w64-mingw32-clang++ -O1 -o lowmem_probe.exe tools/probes/lowmem_probe.cpp && ./lowmem_probe.exe
// Measured 2026-10-29 on Windows 11: all three succeed, the window entirely free.
#include <windows.h>
#include <cstdio>
int main() {
    printf("sizeof(void*)=%u module=%p\n", (unsigned)sizeof(void*), GetModuleHandleW(nullptr));
    // 1. the game's image span at its linked base, 0x400000..0x98F000 (+.pool to +0x587000)
    void* a = VirtualAlloc((void*)0x400000, 0x600000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    printf("0x400000 x 0x600000: %p (err %lu)\n", a, a ? 0 : GetLastError());
    // 2. what is free below 16 MB
    MEMORY_BASIC_INFORMATION m; unsigned char* p = (unsigned char*)0x10000; size_t freeb = 0;
    while (p < (unsigned char*)0x1000000 && VirtualQuery(p, &m, sizeof m)) {
        size_t len = m.RegionSize; if (p + len > (unsigned char*)0x1000000) len = (unsigned char*)0x1000000 - p;
        if (m.State == MEM_FREE) freeb += len; else printf("  used %p + 0x%zx state %lx\n", m.BaseAddress, m.RegionSize, m.State);
        p += m.RegionSize;
    }
    printf("free below 16 MB: 0x%zx of 0xFF0000\n", freeb);
    // 3. a heap for records below 4 GB: top-down allocation stays high, so ask low
    void* h = VirtualAlloc((void*)0x1000000, 0x4000000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    printf("64 MB at 0x1000000: %p\n", h);
    return 0;
}

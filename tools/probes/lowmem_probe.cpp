// A 64-bit probe for docs/x64-build-design.md (the 64-bit build round, stage
// A group XA2): what a 64-bit process on this machine gives the narrow image.
//
//   1. Does Windows load a PE32+ with no DYNAMIC_BASE at the preferred base
//      0x400000 (with and without a .reloc section), and what the PEB says
//      (gs:0x60 + 0x10 is ImageBaseAddress; fs:0x30 + 8 on x86)?
//   2. What is free below 16 MB (the ordering table's links are 24-bit) at
//      main, after the CRT; and again after a Direct3D 11 device, a
//      DirectSound device and SDL3 audio are up?
//   3. 64 MB at 0x1000000 for the low arena, and what is free below 4 GB.
//   4. Where the main thread's stack lies (the task slices hang below it on
//      the 32-bit build; a saved esp is a 32-bit word).
//   5. Does the loader honour a 64-bit DLL's low fixed base (lowdll_probe.dll
//      at 0x10000000, DYNAMIC_BASE off), and does a 5-byte `jmp rel32` row of
//      the bind table's shape ((top << 24) | VA) reach it under each measured
//      top byte? A 14-byte absolute jump thunk for an import slot, called
//      through a 32-bit word.
//   6. Where the 64-bit dsound puts its objects, and the x64 DSBUFFERDESC's
//      size against the game's 0x24.
//
// Build (x86_64-w64-mingw32-clang++ on PATH, beside the i686 driver) and run
// from one directory, the DLL beside the exe:
//   x86_64-w64-mingw32-clang++ -O1 -shared -o lowdll_probe.dll tools/probes/lowdll_probe.cpp \
//       -Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase
//   x86_64-w64-mingw32-clang++ -O1 -o lowmem_fixed.exe tools/probes/lowmem_probe.cpp -ld3d11 -lpsapi \
//       -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase            # .reloc kept
//   x86_64-w64-mingw32-clang++ -O1 -o lowmem_noreloc.exe tools/probes/lowmem_probe.cpp -ld3d11 -lpsapi \
//       -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase -Wl,--disable-reloc-section
//   x86_64-w64-mingw32-clang++ -O1 -o lowmem_aslr.exe tools/probes/lowmem_probe.cpp -ld3d11 -lpsapi
//   ./lowmem_noreloc.exe   (and the other two)
// SDL3 is loaded by name (SDL3.dll beside the exe, a 64-bit build of the
// tree's SDL 3.4.16); without it that stage is reported as not measured.
//
// Measured 2026-10-29 (the first version: the three VirtualAlloc questions)
// and 2026-11-01 (this one): the results are in docs/x64-build-design.md.
#include <windows.h>
#include <d3d11.h>
#include <dsound.h>
#include <psapi.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

std::uintptr_t PebImageBase() {
    // The PEB from the TEB (gs:0x60 on x86-64), ImageBaseAddress at +0x10.
    std::uintptr_t peb;
    asm("movq %%gs:0x60, %0" : "=r"(peb));
    std::uintptr_t base;
    std::memcpy(&base, reinterpret_cast<const void*>(peb + 0x10), sizeof base);
    return base;
}

// The header of the running executable: magic, machine, DllCharacteristics,
// the .reloc directory's size.
void Header(const void* module) {
    const auto* base = static_cast<const std::uint8_t*>(module);
    std::uint32_t lfanew;
    std::memcpy(&lfanew, base + 0x3C, 4);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + lfanew);
    std::printf("header: magic 0x%X machine 0x%X characteristics 0x%X dllcharacteristics 0x%X ImageBase 0x%llX "
                "SizeOfImage 0x%X .reloc %u bytes\n",
                nt->OptionalHeader.Magic, nt->FileHeader.Machine, nt->FileHeader.Characteristics,
                nt->OptionalHeader.DllCharacteristics, (unsigned long long)nt->OptionalHeader.ImageBase,
                (unsigned)nt->OptionalHeader.SizeOfImage,
                (unsigned)nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);
}

// The free bytes in [lo, hi), the largest free run, and (when `list`) every
// used region.
void Window(const char* when, std::uintptr_t lo, std::uintptr_t hi, bool list) {
    MEMORY_BASIC_INFORMATION m;
    std::uintptr_t p = lo, freeb = 0, largest = 0, largest_at = 0;
    while (p < hi && VirtualQuery(reinterpret_cast<const void*>(p), &m, sizeof m)) {
        std::uintptr_t len = m.RegionSize;
        if (p + len > hi) len = hi - p;
        if (m.State == MEM_FREE) {
            freeb += len;
            if (len > largest) largest = len, largest_at = p;
        } else if (list) {
            char path[MAX_PATH] = "";
            if (m.Type == MEM_IMAGE) GetModuleFileNameA(static_cast<HMODULE>(m.AllocationBase), path, sizeof path);
            else if (m.Type == MEM_MAPPED) GetMappedFileNameA(GetCurrentProcess(), m.BaseAddress, path, sizeof path);
            const char* name = std::strrchr(path, '\\');
            std::printf("    used 0x%09llX + 0x%-8llX %s %s\n", (unsigned long long)p, (unsigned long long)len,
                        m.Type == MEM_IMAGE ? "image" : m.Type == MEM_MAPPED ? "mapped" : "private",
                        name ? name + 1 : path);
        }
        p += m.RegionSize;
    }
    std::printf("  %s: free 0x%llX of 0x%llX in 0x%llX..0x%llX, largest run 0x%llX at 0x%llX\n", when,
                (unsigned long long)freeb, (unsigned long long)(hi - lo), (unsigned long long)lo,
                (unsigned long long)hi, (unsigned long long)largest, (unsigned long long)largest_at);
}

void Below16MB(const char* when, bool list) { Window(when, 0x10000, 0x1000000, list); }

// A jmp rel32 row at (top << 24) | va to `to`: the bind table's shape
// (src/hook/bind.cpp BuildTable), one row, in a page reserved at that address.
// Calls through the row as a 32-bit word and reports.
void Row(std::uint32_t top, std::uint32_t va, const void* to) {
    const std::uintptr_t at = (static_cast<std::uintptr_t>(top) << 24) | va;
    const std::uintptr_t page = at & ~static_cast<std::uintptr_t>(0xFFFF);
    void* got = VirtualAlloc(reinterpret_cast<void*>(page), 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (got != reinterpret_cast<void*>(page)) {
        std::printf("  row at 0x%09llX: cannot reserve its page (got %p, error %lu)\n", (unsigned long long)at, got,
                    GetLastError());
        if (got) VirtualFree(got, 0, MEM_RELEASE);
        return;
    }
    const std::intptr_t delta = reinterpret_cast<std::intptr_t>(to) - static_cast<std::intptr_t>(at + 5);
    const bool reaches = delta >= -(std::intptr_t{1} << 31) && delta < (std::intptr_t{1} << 31);
    std::printf("  row at 0x%09llX -> %p: delta %lld bytes, %s a jmp rel32", (unsigned long long)at, to,
                (long long)delta, reaches ? "within" : "BEYOND");
    if (reaches) {
        auto* e = reinterpret_cast<std::uint8_t*>(at);
        const auto rel = static_cast<std::int32_t>(delta);
        e[0] = 0xE9;
        std::memcpy(e + 1, &rel, 4);
        FlushInstructionCache(GetCurrentProcess(), e, 5);
        const auto word = static_cast<std::uint32_t>(at);   // the 32-bit code word the game would hold
        auto fn = reinterpret_cast<unsigned (*)(unsigned)>(static_cast<std::uintptr_t>(word));
        std::printf("; called through the 32-bit word 0x%08X: %u\n", word, fn(20));
    } else {
        std::printf("\n");
    }
    VirtualFree(got, 0, MEM_RELEASE);
}

}  // namespace

int main() {
    const void* module = GetModuleHandleW(nullptr);
    std::printf("sizeof(void*)=%u module=%p peb ImageBaseAddress=0x%llX\n", (unsigned)sizeof(void*), module,
                (unsigned long long)PebImageBase());
    Header(module);

    // 4. the main thread's stack: a local, and the TIB's StackBase / StackLimit
    int local = 0;
    const std::uintptr_t stack_base = __readgsqword(0x08), stack_limit = __readgsqword(0x10);
    std::printf("stack: a local at %p; TIB StackBase 0x%llX StackLimit 0x%llX (%s 4 GB)\n", (void*)&local,
                (unsigned long long)stack_base, (unsigned long long)stack_limit,
                stack_base > 0xFFFFFFFFull ? "above" : "below");

    // 1. the game's image span at its linked base, when the executable is not there
    if (reinterpret_cast<std::uintptr_t>(module) != 0x400000) {
        void* a = VirtualAlloc(reinterpret_cast<void*>(0x400000), 0x600000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        std::printf("VirtualAlloc(0x400000, 0x600000): %p (error %lu)\n", a, a ? 0 : GetLastError());
    } else {
        std::printf("the executable is at 0x400000: the image span is the loader's\n");
    }

    // 2. what is free below 16 MB at main, after the CRT
    Below16MB("below 16 MB at main", true);

    // 3. a low arena: 64 MB at 0x1000000, then what is free below 4 GB
    void* h = VirtualAlloc(reinterpret_cast<void*>(0x1000000), 0x4000000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    std::printf("64 MB at 0x1000000: %p (error %lu)\n", h, h ? 0 : GetLastError());
    Window("below 4 GB at main", 0x10000, 0x100000000ull, false);
    Window("0x1000000..0x2000000 at main", 0x1000000, 0x2000000, true);
    // 5. the low DLL, and the bind table's reach to it
    HMODULE low = LoadLibraryA("lowdll_probe.dll");
    if (!low) {
        std::printf("lowdll_probe.dll: not loaded (error %lu): build it beside this exe\n", GetLastError());
    } else {
        const void* fn = reinterpret_cast<const void*>(GetProcAddress(low, "LowDll_Answer"));
        std::printf("lowdll_probe.dll at %p (preferred 0x10000000: %s), LowDll_Answer at %p\n", (void*)low,
                    reinterpret_cast<std::uintptr_t>(low) == 0x10000000 ? "honoured" : "MOVED", fn);
        static const std::uint32_t kTops[] = {0x7C, 0x70, 0x7E, 0x7B, 0x7D, 0x20, 0x7F};
        for (const std::uint32_t top : kTops) Row(top, 0x401000, fn);
    }

    // 5b. an import slot's thunk: a 14-byte absolute jump in a page of ours
    // below 4 GB (the low block, or one page from VirtualAlloc(nullptr)), the
    // 32-bit slot word holding the thunk's address
    void* tp = h ? h : VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (tp && reinterpret_cast<std::uintptr_t>(tp) < 0x100000000ull) {
        auto* t = static_cast<std::uint8_t*>(tp);
        DWORD old;
        VirtualProtect(t, 0x1000, PAGE_EXECUTE_READWRITE, &old);
        const void* target = reinterpret_cast<const void*>(&GetTickCount);
        t[0] = 0xFF, t[1] = 0x25, t[2] = t[3] = t[4] = t[5] = 0;   // jmp [rip+0]
        std::memcpy(t + 6, &target, 8);
        FlushInstructionCache(GetCurrentProcess(), t, 14);
        const auto slot = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(t));
        auto through = reinterpret_cast<DWORD(WINAPI*)()>(static_cast<std::uintptr_t>(slot));
        const DWORD before = GetTickCount(), got = through(), after = GetTickCount();
        std::printf("import-slot thunk at 0x%08X -> GetTickCount %p: %lu in %lu..%lu (%s)\n", slot, target, got, before,
                    after, got >= before && got <= after ? "ok" : "WRONG");
    }

    // 6. Direct3D 11, DirectSound, SDL3: the platform objects up, then the window again
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL level;
    const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                         &dev, &level, &ctx);
    std::printf("D3D11CreateDevice: 0x%08lX, device %p\n", hr, (void*)dev);

    HMODULE ds = LoadLibraryA("dsound.dll");
    using CreateFn = HRESULT(WINAPI*)(const GUID*, IDirectSound**, IUnknown*);
    IDirectSound* dsd = nullptr;
    if (ds) {
        auto create = reinterpret_cast<CreateFn>(GetProcAddress(ds, reinterpret_cast<LPCSTR>(1)));
        const HRESULT r = create ? create(nullptr, &dsd, nullptr) : E_FAIL;
        std::printf("dsound.dll at %p, DirectSoundCreate: 0x%08lX, IDirectSound %p (%s 4 GB)\n", (void*)ds, r, (void*)dsd,
                    reinterpret_cast<std::uintptr_t>(dsd) > 0xFFFFFFFFull ? "above" : "below");
        if (dsd) {
            dsd->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
            WAVEFORMATEX wf = {};
            wf.wFormatTag = WAVE_FORMAT_PCM, wf.nChannels = 2, wf.nSamplesPerSec = 44100, wf.wBitsPerSample = 16;
            wf.nBlockAlign = 4, wf.nAvgBytesPerSec = 44100 * 4;
            DSBUFFERDESC d = {};
            d.dwSize = sizeof d, d.dwFlags = DSBCAPS_GLOBALFOCUS, d.dwBufferBytes = 0x24000, d.lpwfxFormat = &wf;
            IDirectSoundBuffer* buf = nullptr;
            const HRESULT b = dsd->CreateSoundBuffer(&d, &buf, nullptr);
            std::printf("  sizeof(DSBUFFERDESC) %u (the game's 0x24), CreateSoundBuffer 0x%08lX, buffer %p (%s 4 GB)\n",
                        (unsigned)sizeof d, b, (void*)buf,
                        reinterpret_cast<std::uintptr_t>(buf) > 0xFFFFFFFFull ? "above" : "below");
        }
    } else {
        std::printf("dsound.dll: not loaded (error %lu)\n", GetLastError());
    }

    HMODULE sdl = LoadLibraryA("SDL3.dll");
    if (!sdl) {
        std::printf("SDL3.dll: not beside the exe (error %lu): the SDL3 stage is not measured\n", GetLastError());
    } else {
        auto init = reinterpret_cast<bool (*)(std::uint32_t)>(GetProcAddress(sdl, "SDL_Init"));
        auto ver = reinterpret_cast<int (*)()>(GetProcAddress(sdl, "SDL_GetVersion"));
        auto open = reinterpret_cast<void* (*)(std::uint32_t, const void*, void*, void*)>(
            GetProcAddress(sdl, "SDL_OpenAudioDeviceStream"));
        auto err = reinterpret_cast<const char* (*)()>(GetProcAddress(sdl, "SDL_GetError"));
        const bool ok = init && init(0x10u);   // SDL_INIT_AUDIO
        void* stream = nullptr;
        if (ok && open) {
            struct Spec { int format, channels, freq; } spec = {0x8010, 2, 44100};   // SDL_AUDIO_S16LE
            stream = open(0xFFFFFFFFu, &spec, nullptr, nullptr);   // SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK
        }
        std::printf("SDL3.dll at %p, version %d, SDL_Init(AUDIO) %s, playback stream %p%s%s\n", (void*)sdl,
                    ver ? ver() : 0, ok ? "ok" : "FAILED", stream, stream ? "" : ": ", stream ? "" : (err ? err() : ""));
    }

    Below16MB("below 16 MB after D3D11, dsound and SDL3", true);
    Window("below 4 GB after them", 0x10000, 0x100000000ull, false);

    // 7. The toolchain's heap under this executable: where its blocks land,
    // and how far they stay below 4 GB (64 MB at a time, up to 3 GB; last,
    // so that nothing above measures a process the climb has filled).
    std::printf("malloc(1 MB) %p, malloc(17 MB) %p\n", std::malloc(1 << 20), std::malloc(17 << 20));
    unsigned low_blocks = 0;
    void* first_high = nullptr;
    void* blocks[48] = {};
    for (unsigned i = 0; i < 48; ++i) {
        blocks[i] = std::malloc(64 << 20);
        if (!blocks[i]) break;
        if (reinterpret_cast<std::uintptr_t>(blocks[i]) < 0x100000000ull) ++low_blocks;
        else if (!first_high) first_high = blocks[i];
    }
    std::printf("malloc(64 MB) x 48: %u below 4 GB, first above %p\n", low_blocks, first_high);
    for (void* b : blocks) std::free(b);
    return 0;
}

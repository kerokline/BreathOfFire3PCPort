// A probe for the native Linux build (docs/linux-round.md section 4, LA3): the
// x86-64 asm of the game names the Win64 calling convention (the first argument
// in rcx, the second in rdx, 32 bytes of home space under a call). Does marking
// the functions involved __attribute__((ms_abi)) keep one asm text on Linux,
// where the default is System V (rdi, rsi, no home space)?
//
// Each case below carries the asm text of the repository's x86-64 half, copied
// as it is (the file is named), against a function declared once System V and
// once ms_abi. Exit 0 when the ms_abi side works and the System V side fails as
// the doc says (the probe prints which). The probe is evidence for the doc, not
// a test of the game's code.
//
//   mp3     src/audio/mp3.cpp Decode: switches rsp to a block of ours, rcx/rdx
//           arguments, 32 bytes of home space, one call
//   tint    src/game/battle/battle_3.cpp Battle_StatusTint: `testb $0x80, %cl`, the
//           first argument read from rcx by a naked entry (ms_abi side only: the
//           System V side would read whatever rcx held)
//   frame   src/game/platform/magic_080.cpp Cfg_Load: `leaq 0x20(%rsp), %rcx` as
//           the callee's one argument
//
// Build (Linux, x86-64):
//   clang++ -std=c++20 -O2 tools/linux_probe/msabi_probe.cpp -o msabi_probe && ./msabi_probe
#include <sys/mman.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#define MS __attribute__((ms_abi))

namespace {
int g_fail = 0;
void Check(const char* name, bool got, bool want) {
    const bool ok = got == want;
    std::printf("%-44s %s\n", name, ok ? "as expected" : "UNEXPECTED");
    if (!ok) ++g_fail;
}
}  // namespace

// ---- mp3: Decode's x86-64 block, verbatim; fn is DecodeFrame.
// rsi keeps the old rsp across the call: it is callee-saved in the Win64 convention, so only an
// ms_abi callee is bound to preserve it. The System V case fails by reading the wrong
// arguments; it restores rsp correctly only because these callees never write rsi.
extern "C" int DecodeMs(void* decoder, void* out) MS;
extern "C" int DecodeSysV(void* decoder, void* out);
extern "C" int DecodeMs(void* decoder, void* out) { return static_cast<int>(reinterpret_cast<std::intptr_t>(decoder) * 10 + reinterpret_cast<std::intptr_t>(out)); }
extern "C" int DecodeSysV(void* decoder, void* out) { return static_cast<int>(reinterpret_cast<std::intptr_t>(decoder) * 10 + reinterpret_cast<std::intptr_t>(out)); }

template <class Fn>
int SwitchAndCall(Fn fn, void* decoder, void* out, std::uintptr_t top) {
    int result;
    void* dec = decoder;
    void* out_r = out;
    asm volatile(
        "movq %%rsp, %%rsi\n\t"
        "movq %[top], %%rsp\n\t"
        "subq $32, %%rsp\n\t"
        "call *%[fn]\n\t"
        "movq %%rsi, %%rsp\n\t"
        : "=a"(result), "+c"(dec), "+d"(out_r)
        : [top] "r"(top), [fn] "r"(fn)
        : "rsi", "r8", "r9", "r10", "r11", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "memory", "cc");
    return result;
}

// ---- tint: Battle_StatusTint's x86-64 half; the body is a function that reports it was reached
extern "C" int g_tint_seen;
int g_tint_seen = -1;
extern "C" MS unsigned long TintBodyMs(unsigned) { g_tint_seen = 1; return 1; }
extern "C" __attribute__((naked)) MS unsigned long TintEntryMs(unsigned) {
    asm("testb $0x80, %cl\n\t"
        "jne TintBodyMs\n\t"
        "ret");
}

// ---- frame: Cfg_Load's x86-64 half; the callee gets its one argument in rcx
extern "C" std::uintptr_t g_frame_seen;
std::uintptr_t g_frame_seen;
extern "C" void FrameBodyMs(unsigned char* frame) MS;
extern "C" void FrameBodyMs(unsigned char* frame) { g_frame_seen = reinterpret_cast<std::uintptr_t>(frame); }
extern "C" void FrameBodySysV(unsigned char* frame);
extern "C" void FrameBodySysV(unsigned char* frame) { g_frame_seen = reinterpret_cast<std::uintptr_t>(frame); }
#define FRAME_ENTRY(NAME, BODY)                      \
    extern "C" __attribute__((naked)) void NAME() {  \
        asm("subq $0x68, %rsp\n\t"                   \
            "leaq 0x20(%rsp), %rcx\n\t"              \
            "call " #BODY "\n\t"                     \
            "addq $0x68, %rsp\n\t"                   \
            "ret");                                  \
    }
FRAME_ENTRY(FrameEntryMs, FrameBodyMs)
FRAME_ENTRY(FrameEntrySysV, FrameBodySysV)

int main() {
    // mp3
    void* stack = mmap(nullptr, 0x40000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    const std::uintptr_t top = (reinterpret_cast<std::uintptr_t>(stack) + 0x40000 - 16) & ~static_cast<std::uintptr_t>(15);
    const int a = SwitchAndCall(&DecodeMs, reinterpret_cast<void*>(7), reinterpret_cast<void*>(3), top);
    Check("mp3 block, callee ms_abi: sees (7, 3)", a == 73, true);
    const int b = SwitchAndCall(&DecodeSysV, reinterpret_cast<void*>(7), reinterpret_cast<void*>(3), top);
    Check("mp3 block, callee System V: sees (7, 3)", b == 73, false);

    // tint
    g_tint_seen = -1;
    reinterpret_cast<unsigned long(MS*)(unsigned)>(&TintEntryMs)(0x80);
    Check("Battle_StatusTint, declared ms_abi: reads 0x80", g_tint_seen == 1, true);

    // frame: the entry hands on its rsp + 0x20, which is (rsp before the call) - 8 - 0x68 + 0x20
    auto frame_of = [](void (*f)()) {
        std::uintptr_t sp;
        g_frame_seen = 0;
        asm volatile(
            "movq %%rsp, %[sp]\n\t"
            "call *%[f]\n\t"
            : [sp] "=&m"(sp)
            : [f] "r"(f)
            : "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory", "cc");
        return g_frame_seen == sp - 8 - 0x68 + 0x20;
    };
    Check("Cfg_Load frame, callee ms_abi: frame as built", frame_of(&FrameEntryMs), true);
    Check("Cfg_Load frame, callee System V: frame as built", frame_of(&FrameEntrySysV), false);
    std::printf("result: %s\n", g_fail ? "UNEXPECTED" : "ok");
    return g_fail ? 1 : 0;
}

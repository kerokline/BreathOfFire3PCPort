// Runtime for lifted C: the guest address space, the x87 control word, the
// indirect-call table. docs/lifter-feasibility.md.
#include "lift_rt.h"

#include <fenv.h>
#include <setjmp.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

Cpu R;
u8* M;

typedef struct { u32 addr; void (*fn)(void); } Entry;
extern const Entry lift_table[];   // emitted, sorted by address
extern const u32 lift_table_n;

// The whole 32-bit guest space, reserved and committed on touch: a guest
// address is an offset, whatever the host's pointer width.
int rt_init(void) {
    if (M) return 0;
    void* p = mmap(NULL, (size_t)1 << 32, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) return -1;
    M = (u8*)p;
    R.cw = 0x027F;
    fesetround(FE_TONEAREST);
    return 0;
}

u8* rt_memory(void) { return M; }
Cpu* rt_cpu(void) { return &R; }

// The fuzz calls through rt_call, so a failure returns to it with the reason
// instead of ending the process. Outside rt_call a failure aborts.
static jmp_buf* rt_trap;
static char rt_why[160];

const char* rt_last_failure(void) { return rt_why; }

int rt_call(u32 target) {
    jmp_buf here;
    jmp_buf* outer = rt_trap;
    rt_trap = &here;
    rt_why[0] = 0;
    int failed = setjmp(here);
    if (!failed) rt_dispatch(target);
    rt_trap = outer;
    return failed;
}

void rt_fail(const char* what, u32 at) {
    if (rt_trap) {
        snprintf(rt_why, sizeof rt_why, "%s at 0x%08X", what, at);
        longjmp(*rt_trap, 1);
    }
    fprintf(stderr, "lifted code: %s at 0x%08X\n", what, at);
    fflush(stderr);
    abort();
}

void rt_fldcw(u16 cw) {
    if (((cw >> 8) & 3) != 2) rt_fail("fldcw: precision control is not 53-bit", cw);
    static const int modes[4] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    R.cw = cw;
    fesetround(modes[(cw >> 10) & 3]);
}

// fist: the host's rounding mode already mirrors the guest's (rt_fldcw).
s64 rt_fist(double v, int bits) {
    double r = nearbyint(v);
    double lim = bits == 16 ? 32768.0 : bits == 32 ? 2147483648.0 : 9223372036854775808.0;
    if (!(r >= -lim && r < lim)) return bits == 16 ? (s64)(s16)0x8000 : bits == 32 ? (s64)(s32)0x80000000u
                                                                                   : (s64)0x8000000000000000ull;
    return (s64)r;
}

double rt_frndint(double v) { return nearbyint(v); }

void rt_dispatch(u32 target) {
    u32 lo = 0, hi = lift_table_n;
    while (lo < hi) {
        u32 mid = (lo + hi) / 2;
        if (lift_table[mid].addr < target) lo = mid + 1;
        else hi = mid;
    }
    if (lo < lift_table_n && lift_table[lo].addr == target) {
        lift_table[lo].fn();
        return;
    }
    rt_fail("indirect transfer to a function that was not lifted", target);
}

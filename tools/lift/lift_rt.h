// Runtime for C emitted by tools/lift/lift_x86.py (docs/lifter-feasibility.md).
//
// The model is the crude one PLAN.md section 2B describes: the guest's eight
// registers, its flags and its x87 stack as C variables, and the guest's whole
// 32-bit address space as one byte array M, so a guest address is an offset
// and the lifted C is 64-bit clean. Built for a little-endian host.
//
// x87: the stack holds doubles. That is exact only under a 53-bit precision
// control word, which is what BOF3.exe runs (0x027F, measured on 11 million
// calls - docs/psx-library-layer.md); fldcw of anything else aborts. The
// rounding control is mirrored into the host's with fesetround, so the
// emitted C must be compiled with -frounding-math.
#pragma once
#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

typedef struct {
    u32 eax, ecx, edx, ebx, esp, ebp, esi, edi;
    double st[8];
    u32 top;      // x87 TOP
    u16 cw, sw;   // control word; status word's C0..C3 (TOP is merged on read)
} Cpu;

extern Cpu R;
extern u8* M;

static inline u8 LD8(u32 a) { return M[a]; }
static inline u16 LD16(u32 a) { u16 v; memcpy(&v, M + a, 2); return v; }
static inline u32 LD32(u32 a) { u32 v; memcpy(&v, M + a, 4); return v; }
static inline u64 LD64(u32 a) { u64 v; memcpy(&v, M + a, 8); return v; }
static inline void ST8(u32 a, u8 v) { M[a] = v; }
static inline void ST16(u32 a, u16 v) { memcpy(M + a, &v, 2); }
static inline void ST32(u32 a, u32 v) { memcpy(M + a, &v, 4); }
static inline void ST64(u32 a, u64 v) { memcpy(M + a, &v, 8); }

static inline u8 PARITY(u32 v) { return (u8)(!__builtin_parity(v & 0xFF)); }

// x87 stack.
#define FST(i) R.st[(R.top + (i)) & 7]
static inline void FPUSH(double v) { R.top = (R.top - 1) & 7; R.st[R.top] = v; }
static inline void FPOP(void) { R.top = (R.top + 1) & 7; }
static inline float LDF32(u32 a) { float f; memcpy(&f, M + a, 4); return f; }
static inline double LDF64(u32 a) { double d; memcpy(&d, M + a, 8); return d; }
static inline void STF32(u32 a, double v) { float f = (float)v; memcpy(M + a, &f, 4); }
static inline void STF64(u32 a, double v) { memcpy(M + a, &v, 8); }
// fcom: C3 C2 C0 = 000 greater, 001 less, 100 equal, 111 unordered.
static inline void FCOM(double a, double b) {
    u16 c = a > b ? 0 : a < b ? 0x0100 : a == b ? 0x4000 : 0x4500;
    R.sw = (u16)((R.sw & ~0x4500) | c);
}
static inline u16 FNSTSW(void) { return (u16)((R.sw & ~0x3800) | ((R.top & 7) << 11)); }

// Implemented in lift_rt.c.
void rt_fldcw(u16 cw);
s64 rt_fist(double v, int bits);   // round by the control word; out of range -> indefinite
double rt_frndint(double v);
void rt_dispatch(u32 target);      // an indirect call or tail jump
void rt_fail(const char* what, u32 at);   // aborts: no silent fork (CLAUDE.md rule 4)

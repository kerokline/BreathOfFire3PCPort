// Moved here by tools/move_funcs.py from move_cmds.cpp, psx_gpu.cpp, psx_gte.cpp, psx_gte_float.cpp, psx_gte_matrix.cpp, psx_gte_transform.cpp, psx_rest.cpp (docs/source-layout-round.md).
#include "game/platform/gte.h"

#include <cstdint>
#include <cstring>
#include <emmintrin.h>

#include "bof3/symbols.gen.h"
#include "game/move_cmds_callees.h"
#include "game/move_script_bytes.h"
#include "game/psx_gpu.h"
#include "game/psx_gte.h"
#include "game/psx_gte_float.h"
#include "game/psx_gte_matrix.h"
#include "game/psx_gte_transform.h"
#include "game/psx_rest_callees.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

using namespace move_cmds;

}  // namespace

using namespace psx_rest;

namespace {

// Math_Ratan2's doubles, bit for bit the image's (the fuzz checks them).
constexpr double kScale = 2048.0;

constexpr std::uint64_t kInversePiBits = 0x3FD461D59AE78A99ull;   // 0.3184713375796178 = 1 / 3.14

double InversePi() {
    double v;
    std::memcpy(&v, &kInversePiBits, sizeof v);
    return v;
}

const double kInversePi = InversePi();

constexpr long kStackDepth = 20;

// 2^-14, the float at 0x5C466C: a depth as the primitives' per-vertex float.
constexpr double kDepthScale = 1.0 / 16384.0;

float DepthAsFloat(long depth) { return static_cast<float>(static_cast<double>(depth) * kDepthScale); }

void StoreAt(void* prim, unsigned offset, float value) {
    std::memcpy(static_cast<unsigned char*>(prim) + offset, &value, sizeof value);
}

// The depths oldest first are Gte_DepthOlder[2], [1], [0], then Gte_Depth.
void PrimDepths3(void* prim, unsigned stride) {
    StoreAt(prim, 0x10, DepthAsFloat(Gte_DepthOlder[1]));
    StoreAt(prim, 0x10 + stride, DepthAsFloat(Gte_DepthOlder[0]));
    StoreAt(prim, 0x10 + 2 * stride, DepthAsFloat(Gte_Depth));
}

void PrimDepths4(void* prim, unsigned stride) {
    StoreAt(prim, 0x10, DepthAsFloat(Gte_DepthOlder[2]));
    StoreAt(prim, 0x10 + stride, DepthAsFloat(Gte_DepthOlder[1]));
    StoreAt(prim, 0x10 + 2 * stride, DepthAsFloat(Gte_DepthOlder[0]));
    StoreAt(prim, 0x10 + 3 * stride, DepthAsFloat(Gte_Depth));
}

// The newest depth at all four vertices, converted once. The original stores
// two from the x87 register and copies the last two out of the primitive as
// integers; the same four dwords either way (a control that stored all four
// from the register was not refused by the fuzz).
void PrimDepthFlat4(void* prim, unsigned stride) {
    const float depth = DepthAsFloat(Gte_Depth);
    for (unsigned v = 0; v < 4; ++v) StoreAt(prim, 0x10 + v * stride, depth);
}

long DepthRamp(long value) {
    const long lo = Gte_RampNear;
    if (value < lo) return 0;
    const long hi = Gte_RampFar;
    if (value > hi) return 0x1000;
    // Both differences wrap in 32 bits, as the original's do.
    const std::int32_t num = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) - static_cast<std::uint32_t>(lo));
    const std::int32_t den = static_cast<std::int32_t>(static_cast<std::uint32_t>(hi) - static_cast<std::uint32_t>(lo));
    // lo == hi == value: 0 / 0, and _ftol of a NaN is the integer indefinite,
    // 0x8000000000000000, whose low dword - what the caller gets - is 0.
    if (den == 0) return 0;
    const double scaled = static_cast<double>(num) / static_cast<double>(den) * 4096.0;
    // _ftol 0x5B9550 truncates to 64 bits; eax is the low half. |scaled| is at
    // most 2^43.
    return static_cast<long>(static_cast<std::uint32_t>(static_cast<std::int64_t>(scaled)));
}

std::int32_t MulWrap(long a, long b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

// The globals are read where the original reads them and not before, so that
// an out pointer aimed at one of them gives what the original gives.
void Perspective(float* sx, float* sy, long* sz) {
    if (Gte_Transformed[2] >= Gte_NearZ) {
        // In front of the near plane: x * h / z + offset, the product an
        // integer one that wraps.
        const std::int32_t px = MulWrap(Gte_ProjDistance, Gte_Transformed[0]);
        *sx = static_cast<float>(static_cast<double>(px) / static_cast<double>(Gte_Transformed[2]) +
                                 static_cast<double>(Gte_OffsetX));
        const std::int32_t py = MulWrap(Gte_Transformed[1], Gte_ProjDistance);
        *sy = static_cast<float>(static_cast<double>(py) / static_cast<double>(Gte_Transformed[2]) +
                                 static_cast<double>(Gte_OffsetY));
        *sz = Gte_Transformed[2];
        return;
    }
    if (Gte_Transformed[2] <= 0) {
        // At or behind the eye: projected as if on the near plane, depth 0.
        const std::int32_t px = MulWrap(Gte_ProjDistance, Gte_Transformed[0]);
        *sx = static_cast<float>(static_cast<double>(px) / static_cast<double>(Gte_NearZ) +
                                 static_cast<double>(Gte_OffsetX));
        const std::int32_t py = MulWrap(Gte_Transformed[1], Gte_ProjDistance);
        *sy = static_cast<float>(static_cast<double>(py) / static_cast<double>(Gte_NearZ) +
                                 static_cast<double>(Gte_OffsetY));
        *sz = 0;
        return;
    }
    // Between the eye and the near plane: the same projection onto the near
    // plane, but in floating point throughout - nothing wraps - by way of
    // z / near, with y's product passing through a 32-bit float.
    const double z = static_cast<double>(Gte_Transformed[2]);
    const double ratio = z / static_cast<double>(Gte_NearZ);
    const float y_scaled = static_cast<float>(static_cast<double>(Gte_Transformed[1]) * ratio);
    *sx = static_cast<float>(static_cast<double>(Gte_Transformed[0]) * ratio * static_cast<double>(Gte_ProjDistance) / z +
                             static_cast<double>(Gte_OffsetX));
    *sy = static_cast<float>(static_cast<double>(Gte_ProjDistance) * static_cast<double>(y_scaled) /
                                 static_cast<double>(Gte_Transformed[2]) +
                             static_cast<double>(Gte_OffsetY));
    *sz = Gte_Transformed[2];
}

// --- scaffolding: the x87 control word -------------------------------------------
// Only the checks below touch it; nothing of ours depends on it.
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}

struct {
    unsigned ramp_calls, perspective_calls, mismatches;
    unsigned branch[3];
    unsigned short word[4];
    unsigned word_calls[4], word_other;
} g_live;

void LiveCountWord() {
    const unsigned short cw = GetControlWord();
    for (unsigned i = 0; i < 4; ++i) {
        if (g_live.word_calls[i] == 0) g_live.word[i] = cw;
        if (g_live.word[i] == cw) { ++g_live.word_calls[i]; return; }
    }
    ++g_live.word_other;
}

void LiveReport() {
    bof3::Log("shadow      psx_gte_float live: %u Gte_DepthRamp, %u Gte_Perspective (%u in front of the near plane, "
              "%u behind the eye, %u between), %u MISMATCHES; x87 control word %04X x %u, %04X x %u, %04X x %u, "
              "%04X x %u, others x %u",
              g_live.ramp_calls, g_live.perspective_calls, g_live.branch[0], g_live.branch[1], g_live.branch[2],
              g_live.mismatches, g_live.word[0], g_live.word_calls[0], g_live.word[1], g_live.word_calls[1],
              g_live.word[2], g_live.word_calls[2], g_live.word[3], g_live.word_calls[3], g_live.word_other);
}

void LiveTick() {
    const unsigned calls = g_live.ramp_calls + g_live.perspective_calls;
    if (calls == 1 || calls % 65536 == 0) LiveReport();
}

// The rotations premultiply: matrix = R * matrix. The original's R is a stack
// matrix whose padding it never writes; the product never reads it.
short* Premultiply(short r0, short r1, short r2, short r3, short r4, short r5, short r6, short r7, short r8,
                   short* matrix) {
    const short r[9] = {r0, r1, r2, r3, r4, r5, r6, r7, r8};
    Gte_MulMatrix0(r, matrix, matrix);
    return matrix;
}

constexpr short kOne = 0x1000;

long AddWrap(long a, unsigned long b) { return static_cast<long>(static_cast<unsigned long>(a) + b); }

// One vertex: rotation, translation, perspective. Gte_Transformed is left
// holding the vertex in view space, as the original leaves it.
// Gte_Depth as the `long *` the projections take: the row is the image's
// 32-bit word (std::int32_t in the generated header), long on Win32.
long* Depth() { return reinterpret_cast<long*>(&Gte_Depth); }

void Project(const short* vertex, float* sx, float* sy, long* sz) {
    Gte_ApplyMatrix(reinterpret_cast<const short*>(Gte_Matrix), vertex, Gte_Transformed);
    Gte_Transformed[0] = AddWrap(Gte_Transformed[0], Gte_Matrix[5]);
    Gte_Transformed[1] = AddWrap(Gte_Transformed[1], Gte_Matrix[6]);
    Gte_Transformed[2] = AddWrap(Gte_Transformed[2], Gte_Matrix[7]);
    Gte_Perspective(sx, sy, sz);
}

float* Screen(unsigned point) { return reinterpret_cast<float*>(&Gte_ScreenXY[point * 2]); }

const short* Vertex(unsigned slot) { return reinterpret_cast<const short*>(&Gte_Vertices[slot * 2]); }

// The vertex registers in memory order are V1, V0, V2 (Gte_LoadVertices3).
constexpr unsigned kV0 = 1, kV1 = 0, kV2 = 2;

// One vertex through the FIFOs: the two older screen points and the three
// older depths move up, the new ones go in at the end.
void ProjectThroughFifos(const short* vertex) {
    Gte_ApplyMatrix(reinterpret_cast<const short*>(Gte_Matrix), vertex, Gte_Transformed);
    Gte_Transformed[0] = AddWrap(Gte_Transformed[0], Gte_Matrix[5]);
    Gte_Transformed[1] = AddWrap(Gte_Transformed[1], Gte_Matrix[6]);
    Gte_Transformed[2] = AddWrap(Gte_Transformed[2], Gte_Matrix[7]);
    Gte_DepthOlder[2] = Gte_DepthOlder[1];
    Gte_DepthOlder[1] = Gte_DepthOlder[0];
    Gte_DepthOlder[0] = Gte_Depth;
    Gte_ScreenXY[0] = Gte_ScreenXY[2];
    Gte_ScreenXY[1] = Gte_ScreenXY[3];
    Gte_ScreenXY[2] = Gte_ScreenXY[4];
    Gte_ScreenXY[3] = Gte_ScreenXY[5];
    Gte_Perspective(Screen(2), Screen(2) + 1, Depth());
}

// _ftol 0x5B9550: fistp to 64 bits, rounding toward zero, and the caller gets
// the low half. Out of range - or a NaN - is the integer indefinite,
// 0x8000000000000000, whose low half is 0.
long Ftol(double v) {
    if (!(v < 9223372036854775808.0 && v >= -9223372036854775808.0)) return 0;
    return static_cast<long>(static_cast<std::uint32_t>(static_cast<std::int64_t>(v)));
}

struct Normal { long x, y, z, length_squared; };

// x and y pass through 32-bit floats before anything else is done with them;
// z does not. The returned square is the sum as a 32-bit float. Those three
// the fuzz can see. Two things here it cannot, kept because the original has
// them: the order of the sum (z*z, then y*y, then x*x), and a zero length
// dividing by 1 - without it the three results are _ftol of a NaN, which is
// 0 as well.
Normal Normalise(const long* in) {
    const float xf = static_cast<float>(in[0]);
    const float yf = static_cast<float>(in[1]);
    const double z = static_cast<double>(in[2]);
    double sum = z * z;
    sum = sum + static_cast<double>(yf) * static_cast<double>(yf);
    sum = sum + static_cast<double>(xf) * static_cast<double>(xf);
    const float sum_f = static_cast<float>(sum);
    // sqrtsd: correctly rounded, as fsqrt is at 53 bits - and not a libm call,
    // whose rounding would be the library's business.
    double length = _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(sum)));
    if (length == 0.0) length = 1.0;
    Normal n;
    n.x = Ftol(static_cast<double>(xf) * 4096.0 / length);
    n.y = Ftol(static_cast<double>(yf) * 4096.0 / length);
    n.z = Ftol(z * 4096.0 / length);
    n.length_squared = Ftol(static_cast<double>(sum_f));
    return n;
}

}  // namespace

// original 0x5A7A00. Sine of a 4096-step angle, 4096 to the unit, from the
// exe's own quarter-wave table - which is read in place, never copied.
extern "C" int __cdecl Math_Sin(int angle) {
    const int sign = (angle & 0x800) ? -1 : 1;
    const int a = angle & 0x7FF;
    return static_cast<int>(Math_SinTable[a < 0x400 ? a : 0x800 - a]) * sign;
}

// original 0x5A7A50: the cosine, as the sine a quarter turn on.
extern "C" int __cdecl Math_Cos(int angle) {
    return Math_Sin(static_cast<int>(static_cast<unsigned>(angle) + 0x400u));
}

// original 0x5A7A70: an angle in 4096ths of a turn from a slope - libgte's
// ratan2 with pi taken as 3.14 (PSX 0x8017AAB0 is the real one, in integers).
// x87 throughout, as the original: fpatan of y over x at the full 64 bits
// (precision control does not touch it), then two fmul by doubles, each
// rounded to whatever the control word's precision is at the call - 53 bits
// in game - and the CRT's _ftol 0x5B9550: round-toward-zero set in a copy of
// the control word for one fistp to 64 bits, then the word put back. The
// caller gets the low dword; _ftol also leaves the high one in edx, which no
// caller reads (int result; 70 call sites, rel32 scan).
extern "C" int __cdecl Math_Ratan2(float y, float x) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds %[y]\n\t"
        "flds %[x]\n\t"
        "fpatan\n\t"
        "fmull %[scale]\n\t"
        "fmull %[inverse_pi]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [y] "m"(y), [x] "m"(x), [scale] "m"(kScale), [inverse_pi] "m"(kInversePi)
        : "eax", "st", "st(1)");
    return static_cast<int>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(result)));
}

// original 0x5A7A90. libgte SquareRoot0: fild, fsqrt, and a tail jmp to the
// CRT's _ftol 0x5B9550 - the root truncated toward zero, eax the low dword.
// Under the game's control word 0x027F fsqrt is the correctly rounded double
// root (docs/psx-library-layer.md section 3), so sqrtsd is the same: an
// integer below 2^31 has a root whose distance to the next integer is far
// above a double's last bit, so the truncation is the integer square root.
// A negative n gives the NaN, and _ftol of a NaN is the integer indefinite
// 0x8000000000000000, whose low dword is 0.
extern "C" long __cdecl Gte_SquareRoot0(long n) {
    if (n < 0) return 0;
    const double root = _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(static_cast<double>(n))));
    return static_cast<long>(static_cast<std::uint32_t>(static_cast<std::int64_t>(root)));
}

// originals 0x5A7AA0, 0x5A7AE0, 0x5A7B00: what the functions above read, set.
// Integer all three, and here because the fuzz that owns those globals is.
// By shape libgte's InitGeom, SetGeomOffset and SetGeomScreen - with the near
// plane at half the projection distance, as the GTE clips, and the depth
// ramp's two ends scaled with it: scale * h / 1000, the product wrapping, the
// division toward zero.
extern "C" void __cdecl Gte_InitGeom(void) {
    Gte_ProjDistance = 1000;
    Gte_NearZ = 500;
    Gte_RampNearScale = 0x3333;
    Gte_RampFarScale = 0x10000;
    Gte_RampNear = 0x3333;
    Gte_RampFar = 0x10000;
    Gte_MatrixDepth = 0;
}

extern "C" void __cdecl Gte_SetGeomOffset(long x, long y) {
    Gte_OffsetX = x;
    Gte_OffsetY = y;
}

extern "C" void __cdecl Gte_SetGeomScreen(long h) {
    Gte_ProjDistance = h;
    Gte_NearZ = h / 2;
    Gte_RampNear = MulWrap(Gte_RampNearScale, h) / 1000;
    Gte_RampFar = MulWrap(Gte_RampFarScale, h) / 1000;
}

extern "C" void __cdecl Gte_SetBackColor(long r, long g, long b) {
    Gte_BackColor[0] = static_cast<short>(static_cast<unsigned long>(r) << 4);
    Gte_BackColor[1] = static_cast<short>(static_cast<unsigned long>(g) << 4);
    Gte_BackColor[2] = static_cast<short>(static_cast<unsigned long>(b) << 4);
}

// originals 0x5A7B90, 0x5A7BC0: libgte PushMatrix and PopMatrix, with room for
// twenty where the PSX had one. A push onto a full stack and a pop from an
// empty one do nothing, silently.
extern "C" void __cdecl Gte_PushMatrix(void) {
    const long depth = Gte_MatrixDepth;
    if (depth >= kStackDepth) return;
    // A negative depth passes the signed test and indexes below the stack, as
    // the original's does.
    std::memcpy(reinterpret_cast<unsigned char*>(Gte_MatrixStack) + static_cast<std::uint32_t>(depth) * 0x20u,
                Gte_Matrix, 0x20);
    Gte_MatrixDepth = depth + 1;
}

extern "C" void __cdecl Gte_PopMatrix(void) {
    long depth = Gte_MatrixDepth;
    if (depth <= 0) return;
    Gte_MatrixDepth = --depth;
    std::memcpy(Gte_Matrix, reinterpret_cast<unsigned char*>(Gte_MatrixStack) + static_cast<std::uint32_t>(depth) * 0x20u,
                0x20);
}

// original 0x5A7BF0. out = (matrix . vector) >> 12: the rotation part of a PSX
// MATRIX, nine s16 at 4096 to the unit, times an SVECTOR. The most-called
// function of the attract run, 2,625,115 times.
//
// As the original has it: sums in 32 bits, wrapping; arithmetic shifts;
// everything read before anything is written, so out may be the vector.
extern "C" void __cdecl Gte_ApplyMatrix(const short* matrix, const short* vector, long* out) {
    const std::int32_t x = vector[0], y = vector[1], z = vector[2];
    std::int32_t row[3];
    for (int r = 0; r < 3; ++r) {
        const std::uint32_t sum = static_cast<std::uint32_t>(matrix[3 * r] * x) +
                                  static_cast<std::uint32_t>(matrix[3 * r + 1] * y) +
                                  static_cast<std::uint32_t>(matrix[3 * r + 2] * z);
        row[r] = static_cast<std::int32_t>(sum) >> 12;
    }
    out[0] = row[0];
    out[1] = row[1];
    out[2] = row[2];
}

// original 0x5A7C70. libgte ApplyMatrixSV(MATRIX *, SVECTOR *, SVECTOR *): the
// rotation times an SVECTOR, each row three products summed in 32 bits (they
// wrap), >> 12 arithmetic, stored as s16. Every read before the first store;
// the stores z, x, y as the original makes them.
extern "C" void __cdecl Gte_ApplyMatrixSV(const short* matrix, const short* vector, short* out) {
    const std::int32_t x = vector[0], y = vector[1], z = vector[2];
    std::int32_t row[3];
    for (int r = 0; r < 3; ++r) {
        const std::uint32_t sum = static_cast<std::uint32_t>(matrix[3 * r] * x) +
                                  static_cast<std::uint32_t>(matrix[3 * r + 1] * y) +
                                  static_cast<std::uint32_t>(matrix[3 * r + 2] * z);
        row[r] = static_cast<std::int32_t>(sum) >> 12;
    }
    out[2] = static_cast<short>(row[2]);
    out[0] = static_cast<short>(row[0]);
    out[1] = static_cast<short>(row[1]);
}

// original 0x5A7CF0: the same with a VECTOR in - 32-bit components, so here the
// products wrap too. libgte ApplyMatrixLV.
//
// DIV-0178: the PlayStation's ApplyMatrixLV (SLPS 0x80179390, Psy-Q
// MTX_004.OBJ) splits each component into v >> 15 and v & 0x7FFF, runs the
// high parts through MVMVA unshifted and the low parts shifted by 12, and
// sums them (high << 3): the exact (m . v) >> 12 of a 64-bit product. The
// port multiplies in 32 bits, so a component past 8 units (0x80000) against
// a matrix entry of 0x1000 wraps negative - area 99's console flash, whose
// arms are 15 units long, folds over the horizon at the frame a product
// crosses 2^31 (issue #121's second report, known-defects D358). With the
// switch armed the product is 64-bit; the shift and the stores are the same.
extern "C" void __cdecl Gte_ApplyMatrixLV(const short* matrix, const long* vector, long* out) {
    if (psx_gte::g_lv_exact) {
        const std::int64_t x = vector[0], y = vector[1], z = vector[2];
        std::int32_t row[3];
        for (int r = 0; r < 3; ++r) {
            const std::int64_t sum = matrix[3 * r] * x + matrix[3 * r + 1] * y + matrix[3 * r + 2] * z;
            row[r] = static_cast<std::int32_t>(sum >> 12);
        }
        out[0] = row[0];
        out[1] = row[1];
        out[2] = row[2];
        return;
    }
    const std::uint32_t x = static_cast<std::uint32_t>(vector[0]), y = static_cast<std::uint32_t>(vector[1]),
                        z = static_cast<std::uint32_t>(vector[2]);
    std::int32_t row[3];
    for (int r = 0; r < 3; ++r) {
        const std::uint32_t sum = static_cast<std::uint32_t>(static_cast<std::int32_t>(matrix[3 * r])) * x +
                                  static_cast<std::uint32_t>(static_cast<std::int32_t>(matrix[3 * r + 1])) * y +
                                  static_cast<std::uint32_t>(static_cast<std::int32_t>(matrix[3 * r + 2])) * z;
        row[r] = static_cast<std::int32_t>(sum) >> 12;
    }
    out[0] = row[0];
    out[1] = row[1];
    out[2] = row[2];
}

// original 0x5A7D70: libgte MulMatrix0 by shape - out = a * b, each element
// the three products summed in 32 bits and >> 12. Every element of a and b is
// read before anything is stored, so out may be either of them.
extern "C" short* __cdecl Gte_MulMatrix0(const short* a, const short* b, short* out) {
    short result[10];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            const std::uint32_t sum = static_cast<std::uint32_t>(a[3 * r] * b[c]) +
                                      static_cast<std::uint32_t>(a[3 * r + 1] * b[3 + c]) +
                                      static_cast<std::uint32_t>(a[3 * r + 2] * b[6 + c]);
            result[3 * r + c] = static_cast<short>(static_cast<std::int32_t>(sum) >> 12);
        }
    result[9] = 0;   // DIV-0021: the original's is whatever its stack held
    std::memcpy(out, result, 20);
    return out;
}

// originals 0x5A7F10, 0x5A7F80, 0x5A7FF0: libgte RotMatrixX, Y and Z by
// shape. The cosine is taken first, then the sine, as the original.
extern "C" short* __cdecl Gte_RotMatrixX(int angle, short* matrix) {
    const short c = static_cast<short>(Math_Cos(angle));
    const short s = static_cast<short>(Math_Sin(angle));
    return Premultiply(kOne, 0, 0, 0, c, static_cast<short>(-s), 0, s, c, matrix);
}

extern "C" short* __cdecl Gte_RotMatrixY(int angle, short* matrix) {
    const short c = static_cast<short>(Math_Cos(angle));
    const short s = static_cast<short>(Math_Sin(angle));
    return Premultiply(c, 0, s, 0, kOne, 0, static_cast<short>(-s), 0, c, matrix);
}

extern "C" short* __cdecl Gte_RotMatrixZ(int angle, short* matrix) {
    const short c = static_cast<short>(Math_Cos(angle));
    const short s = static_cast<short>(Math_Sin(angle));
    return Premultiply(c, static_cast<short>(-s), 0, s, c, 0, 0, 0, kOne, matrix);
}

// originals 0x5A8060, 0x5A80B0: libgte RotMatrix - Rx Ry Rz - and the twin
// that makes Ry Rx Rz. The rotation part and its padding start as the
// identity; the translation is left alone. Each angle is read just before its
// rotation, after the matrix has been written, as the original reads them.
extern "C" short* __cdecl Gte_RotMatrix(const short* angles, short* matrix) {
    std::memcpy(matrix, Gte_IdentityRotation, 20);
    Gte_RotMatrixZ(angles[2], matrix);
    Gte_RotMatrixY(angles[1], matrix);
    Gte_RotMatrixX(angles[0], matrix);
    return matrix;
}

extern "C" short* __cdecl Gte_RotMatrixYXZ(const short* angles, short* matrix) {
    std::memcpy(matrix, Gte_IdentityRotation, 20);
    Gte_RotMatrixZ(angles[2], matrix);
    Gte_RotMatrixX(angles[0], matrix);
    Gte_RotMatrixY(angles[1], matrix);
    return matrix;
}

// original 0x5A8100: libgte TransMatrix - a MATRIX's translation from a VECTOR.
extern "C" void __cdecl Gte_TransMatrix(unsigned long* matrix, const unsigned long* vector) {
    matrix[5] = vector[0];
    matrix[6] = vector[1];
    matrix[7] = vector[2];
}

// original 0x5A8120: libgte ScaleMatrix - each column of a rotation matrix
// times a component of the vector, >> 12, one s16 at a time, in place.
extern "C" short* __cdecl Gte_ScaleMatrix(short* matrix, const long* scale) {
    for (unsigned i = 0; i < 9; ++i) {
        const std::uint32_t product = static_cast<std::uint32_t>(static_cast<std::int32_t>(matrix[i])) *
                                      static_cast<std::uint32_t>(scale[i % 3]);
        matrix[i] = static_cast<short>(static_cast<std::int32_t>(product) >> 12);
    }
    return matrix;
}

// original 0x5A81B0: libgte TransposeMatrix, one s16 at a time, each read and
// then stored, in this order - so that `out == in` gives what the original
// gives, which is not the transpose.
extern "C" void __cdecl Gte_TransposeMatrix(const short* in, short* out) {
    static const unsigned char kFrom[9] = {0, 3, 6, 1, 4, 7, 2, 5, 8};
    for (unsigned i = 0; i < 9; ++i) {
        const short v = in[kFrom[i]];
        out[i] = v;
    }
}

// original 0x5A8200: the current matrix applied to an SVECTOR, rotation and
// then translation. libgte RotTrans, without its flag argument.
extern "C" void __cdecl Gte_RotTrans(const short* vector, long* out) {
    Gte_ApplyMatrix(reinterpret_cast<const short*>(Gte_Matrix), vector, out);
    const long rotated[3] = {out[0], out[1], out[2]};
    out[0] = rotated[0] + static_cast<long>(Gte_Matrix[5]);
    out[1] = rotated[1] + static_cast<long>(Gte_Matrix[6]);
    out[2] = rotated[2] + static_cast<long>(Gte_Matrix[7]);
}

// original 0x5A8250: libgte RotTransPers, without its flag argument, which
// the original never reads. The screen point comes back as two floats.
extern "C" long __cdecl Gte_RotTransPers(const short* vertex, unsigned long* sxy, long* p) {
    ProjectThroughFifos(vertex);
    *p = Gte_DepthRamp(Gte_Depth);
    sxy[0] = Gte_ScreenXY[4];
    sxy[1] = Gte_ScreenXY[5];
    return Gte_Depth >> 2;
}

// original 0x5A8340: where a depth lies between two planes, 0 to 0x1000 - the
// GTE's depth-cue interpolation factor by its shape. Truncated, not rounded.
extern "C" long __cdecl Gte_DepthRamp(long value) {
    if (!psx_gte_float::g_ramp_clone) return DepthRamp(value);
    LiveCountWord();
    const long theirs = psx_gte_float::g_ramp_clone(value);
    const long ours = DepthRamp(value);
    ++g_live.ramp_calls;
    if (theirs != ours && ++g_live.mismatches <= 8)
        bof3::Log("shadow      Gte_DepthRamp MISMATCH: value %ld in %ld..%ld, ours %ld, original %ld", value,
                  static_cast<long>(Gte_RampNear), static_cast<long>(Gte_RampFar), ours, theirs);
    LiveTick();
    return ours;
}

// original 0x5A8380: the perspective division. Gte_Transformed, the vertex in
// view space, to a screen position in FLOATS - where the PSX had two s16 - and
// a depth. 2.5 million calls an attract run.
extern "C" void __cdecl Gte_Perspective(float* sx, float* sy, long* sz) {
    if (!psx_gte_float::g_perspective_clone) { Perspective(sx, sy, sz); return; }
    LiveCountWord();
    ++g_live.branch[Gte_Transformed[2] >= Gte_NearZ ? 0 : Gte_Transformed[2] <= 0 ? 1 : 2];
    std::uint32_t their_bits[3] = {0, 0, 0}, our_bits[3];
    psx_gte_float::g_perspective_clone(reinterpret_cast<float*>(&their_bits[0]), reinterpret_cast<float*>(&their_bits[1]),
                        reinterpret_cast<long*>(&their_bits[2]));
    Perspective(sx, sy, sz);
    std::memcpy(&our_bits[0], sx, 4);
    std::memcpy(&our_bits[1], sy, 4);
    std::memcpy(&our_bits[2], sz, 4);
    ++g_live.perspective_calls;
    // Outs that overlap each other would differ here without being wrong; the
    // game's never do, and the start-up fuzz is where overlap is compared.
    if (std::memcmp(their_bits, our_bits, sizeof our_bits) != 0 && ++g_live.mismatches <= 8)
        bof3::Log("shadow      Gte_Perspective MISMATCH: (%ld, %ld, %ld) near %ld h %ld: ours %08X %08X %08X, "
                  "original %08X %08X %08X", Gte_Transformed[0], Gte_Transformed[1], Gte_Transformed[2],
                  static_cast<long>(Gte_NearZ), static_cast<long>(Gte_ProjDistance), (unsigned)our_bits[0], (unsigned)our_bits[1], (unsigned)our_bits[2],
                  (unsigned)their_bits[0], (unsigned)their_bits[1], (unsigned)their_bits[2]);
    LiveTick();
}

// originals 0x5A84A0, 0x5A85F0: libgte RotTransPers3 and RotTransPers4 by
// shape. The screen points go to the caller and NOT through the FIFO; the
// depths do go to the depth registers.
extern "C" long __cdecl Gte_RotTransPers3(const short* v0, const short* v1, const short* v2, float* sxy0, float* sxy1,
                                          float* sxy2, long* p) {
    Gte_DepthOlder[2] = Gte_Depth;
    Project(v0, sxy0, sxy0 + 1, &Gte_DepthOlder[1]);
    Project(v1, sxy1, sxy1 + 1, &Gte_DepthOlder[0]);
    Project(v2, sxy2, sxy2 + 1, Depth());
    *p = Gte_DepthRamp(Gte_Depth);
    return Gte_Depth >> 2;
}

extern "C" long __cdecl Gte_RotTransPers4(const short* v0, const short* v1, const short* v2, const short* v3,
                                          float* sxy0, float* sxy1, float* sxy2, float* sxy3, long* p) {
    Project(v0, sxy0, sxy0 + 1, &Gte_DepthOlder[2]);
    Project(v1, sxy1, sxy1 + 1, &Gte_DepthOlder[1]);
    Project(v2, sxy2, sxy2 + 1, &Gte_DepthOlder[0]);
    Project(v3, sxy3, sxy3 + 1, Depth());
    *p = Gte_DepthRamp(Gte_Depth);
    return Gte_Depth >> 2;
}

// originals 0x5A87A0, 0x5A8950: libgte RotAverage3 and RotAverage4 by shape,
// with 12-byte outs - x, y, and the depth / 16384 as a float, once all the
// vertices are through. The result, and Gte_Otz, is a quarter of the average
// depth: a sum over 12 toward zero for three, a sum >> 4 for four.
extern "C" long __cdecl Gte_RotAverage3(const short* v0, const short* v1, const short* v2, float* out0, float* out1,
                                        float* out2, long* p) {
    Gte_DepthOlder[2] = Gte_Depth;
    Project(v0, out0, out0 + 1, &Gte_DepthOlder[1]);
    Project(v1, out1, out1 + 1, &Gte_DepthOlder[0]);
    Project(v2, out2, out2 + 1, Depth());
    Gte_StoreDepthF3(out0 + 2, out1 + 2, out2 + 2);
    *p = Gte_DepthRamp(Gte_Depth);
    const unsigned long sum = static_cast<unsigned long>(Gte_DepthOlder[0]) +
                              static_cast<unsigned long>(Gte_DepthOlder[1]) + static_cast<unsigned long>(Gte_Depth);
    Gte_Otz = static_cast<long>(sum) / 12;
    return Gte_Otz;
}

extern "C" long __cdecl Gte_RotAverage4(const short* v0, const short* v1, const short* v2, const short* v3, float* out0,
                                        float* out1, float* out2, float* out3, long* p) {
    Project(v0, out0, out0 + 1, &Gte_DepthOlder[2]);
    Project(v1, out1, out1 + 1, &Gte_DepthOlder[1]);
    Project(v2, out2, out2 + 1, &Gte_DepthOlder[0]);
    Project(v3, out3, out3 + 1, Depth());
    Gte_StoreDepthF4(out0 + 2, out1 + 2, out2 + 2, out3 + 2);
    *p = Gte_DepthRamp(Gte_Depth);
    const unsigned long sum = static_cast<unsigned long>(Gte_DepthOlder[0]) + static_cast<unsigned long>(Gte_Depth) +
                              static_cast<unsigned long>(Gte_DepthOlder[1]) +
                              static_cast<unsigned long>(Gte_DepthOlder[2]);
    Gte_Otz = static_cast<long>(sum) >> 4;
    return Gte_Otz;
}

// originals 0x5A8B60, 0x5A8C00: libgte VectorNormal and VectorNormalS by
// shape - a VECTOR to length 4096, out as three s32 or three s16; the result
// is the square of the length. Everything is read before anything is
// stored, so out may be in.
extern "C" long __cdecl Gte_VectorNormal(const long* in, long* out) {
    const Normal n = Normalise(in);
    out[0] = n.x;
    out[1] = n.y;
    out[2] = n.z;
    return n.length_squared;
}

extern "C" long __cdecl Gte_VectorNormalS(const long* in, short* out) {
    const Normal n = Normalise(in);
    out[0] = static_cast<short>(n.x);
    out[1] = static_cast<short>(n.y);
    out[2] = static_cast<short>(n.z);
    return n.length_squared;
}

// original 0x5A8CA0: libgte NormalColor by shape - a normal through the light
// matrix, normalised, through the colour matrix, normalised again, the
// background colour added, times the colour in, / 4096 - and then **the
// colour in is copied over the result**: as shipped, lighting does nothing.
// The arithmetic stays, because it is not quite dead: each byte out is
// stored before the next byte in is read, so an out a byte or two below the
// in sees it.
extern "C" void __cdecl Gte_NormalColor(const short* normal, const unsigned char* in, unsigned char* out) {
    long light[3];
    Gte_ApplyMatrix(reinterpret_cast<const short*>(Gte_Matrix2), normal, light);
    Gte_VectorNormal(light, light);
    Gte_ApplyMatrixLV(reinterpret_cast<const short*>(Gte_ColorMatrix), light, light);
    Gte_VectorNormal(light, light);
    const long lit[3] = {AddWrap(light[0], static_cast<unsigned long>(static_cast<long>(Gte_BackColor[0]))),
                         AddWrap(light[1], static_cast<unsigned long>(static_cast<long>(Gte_BackColor[1]))),
                         AddWrap(light[2], static_cast<unsigned long>(static_cast<long>(Gte_BackColor[2])))};
    for (unsigned i = 0; i < 3; ++i) {
        const double scaled = static_cast<double>(lit[i]) * (1.0 / 4096.0) * static_cast<double>(in[i]);
        out[i] = static_cast<unsigned char>(Ftol(scaled));
    }
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char c = in[i];
        out[i] = c;
    }
}

extern "C" void __cdecl Gte_SetMatrix2(const unsigned long* matrix) { std::memmove(Gte_Matrix2, matrix, 8 * 4); }

// originals 0x5A8DC0, 0x5A7B60: libgte SetColorMatrix and SetBackColor by
// shape - the second stores each component << 4, as a word.
extern "C" void __cdecl Gte_SetColorMatrix(const unsigned long* matrix) { std::memmove(Gte_ColorMatrix, matrix, 8 * 4); }

// originals 0x5A8DE0, 0x5A8E00, 0x5A8DA0: libgte SetRotMatrix and
// SetTransMatrix - the two halves of Gte_Matrix from a PSX MATRIX - and the
// same for the second matrix, whole.
extern "C" void __cdecl Gte_SetRotMatrix(const unsigned long* matrix) { std::memmove(Gte_Matrix, matrix, 5 * 4); }

extern "C" void __cdecl Gte_SetTransMatrix(const unsigned long* matrix) {
    Gte_Matrix[5] = matrix[5];
    Gte_Matrix[6] = matrix[6];
    Gte_Matrix[7] = matrix[7];
}

// originals 0x5A8E30, 0x5A8E50. One vertex into the middle slot; or three,
// the first into the middle slot, the second into the first, the third into
// the last.
extern "C" void __cdecl Gte_LoadVertex(const unsigned long* vertex) {
    Gte_Vertices[2] = vertex[0];
    Gte_Vertices[3] = vertex[1];
}

extern "C" void __cdecl Gte_LoadVertices3(const unsigned long* vertices) {
    Gte_Vertices[2] = vertices[0];
    Gte_Vertices[3] = vertices[1];
    Gte_Vertices[0] = vertices[2];
    Gte_Vertices[1] = vertices[3];
    Gte_Vertices[4] = vertices[4];
    Gte_Vertices[5] = vertices[5];
}

// original 0x5A8E90: the GTE's RTPS by shape - V0 through the matrix and the
// FIFOs, and the depth-cue factor of the new depth to IR0. 4 million calls a
// whole attract cycle.
extern "C" void __cdecl Gte_Rtps(void) {
    ProjectThroughFifos(Vertex(kV0));
    Gte_Ir0 = Gte_DepthRamp(Gte_Depth);
}

// original 0x5A8F60: RTPT by shape - V0, V1, V2. Written as the original is:
// the newest depth copied to the oldest slot, then the three results stored
// straight to the three screen points and the three newest depths. That
// leaves every register as three Gte_Rtps would - a control that did exactly
// that was not refused by the fuzz - so the difference is one of form.
extern "C" void __cdecl Gte_Rtpt(void) {
    Gte_DepthOlder[2] = Gte_Depth;
    Project(Vertex(kV0), Screen(0), Screen(0) + 1, &Gte_DepthOlder[1]);
    Project(Vertex(kV1), Screen(1), Screen(1) + 1, &Gte_DepthOlder[0]);
    Project(Vertex(kV2), Screen(2), Screen(2) + 1, Depth());
    Gte_Ir0 = Gte_DepthRamp(Gte_Depth);
}

// originals 0x5A90B0, 0x5A90D0: the newest screen point; or all three, oldest
// first, a dword at a time in this order - which matters if the outs overlap.
extern "C" void __cdecl Gte_StoreScreenXY(unsigned long* out) {
    out[0] = Gte_ScreenXY[4];
    out[1] = Gte_ScreenXY[5];
}

extern "C" void __cdecl Gte_StoreScreenXY3(unsigned long* out0, unsigned long* out1, unsigned long* out2) {
    out0[0] = Gte_ScreenXY[0];
    out0[1] = Gte_ScreenXY[1];
    out1[0] = Gte_ScreenXY[2];
    out1[1] = Gte_ScreenXY[3];
    out2[0] = Gte_ScreenXY[4];
    out2[1] = Gte_ScreenXY[5];
}

// originals 0x5A9110, 0x5A9130, 0x5A9170: the newest depth as a float, depth
// / 16384; or the newest three, or all four, oldest first - one store at a
// time in this order, which is what overlapping outs would show.
extern "C" void __cdecl Gte_StoreDepthF(float* out) { *out = DepthAsFloat(Gte_Depth); }

extern "C" void __cdecl Gte_StoreDepthF3(float* out0, float* out1, float* out2) {
    *out0 = DepthAsFloat(Gte_DepthOlder[1]);
    *out1 = DepthAsFloat(Gte_DepthOlder[0]);
    *out2 = DepthAsFloat(Gte_Depth);
}

extern "C" void __cdecl Gte_StoreDepthF4(float* out0, float* out1, float* out2, float* out3) {
    *out0 = DepthAsFloat(Gte_DepthOlder[2]);
    *out1 = DepthAsFloat(Gte_DepthOlder[1]);
    *out2 = DepthAsFloat(Gte_DepthOlder[0]);
    *out3 = DepthAsFloat(Gte_Depth);
}

// originals 0x5A91C0..0x5A9460: the same depths straight into a primitive's
// per-vertex float, the first at +0x10 and the rest a stride apart - the
// field the primitive setters fill with 0.01 (psx_gpu.cpp). One function per
// primitive layout in the original, several of them byte for byte the same;
// they stay separate entries because they are separate addresses.
extern "C" void __cdecl Gte_PrimDepths3_0C(void* prim) { PrimDepths3(prim, 0x0C); }

extern "C" void __cdecl Gte_PrimDepths3_10(void* prim) { PrimDepths3(prim, 0x10); }

extern "C" void __cdecl Gte_PrimDepths4_0C(void* prim) { PrimDepths4(prim, 0x0C); }

extern "C" void __cdecl Gte_PrimDepths4_10(void* prim) { PrimDepths4(prim, 0x10); }

extern "C" void __cdecl Gte_PrimDepthFlat4_10(void* prim) { PrimDepthFlat4(prim, 0x10); }

extern "C" void __cdecl Gte_PrimDepths3_10B(void* prim) { PrimDepths3(prim, 0x10); }

extern "C" void __cdecl Gte_PrimDepths4_10B(void* prim) { PrimDepths4(prim, 0x10); }

extern "C" void __cdecl Gte_PrimDepths4_14(void* prim) { PrimDepths4(prim, 0x14); }

extern "C" void __cdecl Gte_PrimDepthFlat4_14(void* prim) { PrimDepthFlat4(prim, 0x14); }

extern "C" void __cdecl Gte_PrimDepths3_10C(void* prim) { PrimDepths3(prim, 0x10); }

extern "C" void __cdecl Gte_PrimDepths4_10C(void* prim) { PrimDepths4(prim, 0x10); }

// original 0x5A94B0.
extern "C" void __cdecl Gte_StoreDepthQuarter(long* out) { *out = Gte_Depth >> 2; }

void Gte_Inject() {
    BOF3_INJECT(Math_Sin);
    BOF3_INJECT(Math_Cos);
    BOF3_INJECT(Math_Ratan2);
    BOF3_INJECT(Gte_SquareRoot0);
    BOF3_INJECT(Gte_InitGeom);
    BOF3_INJECT(Gte_SetGeomOffset);
    BOF3_INJECT(Gte_SetGeomScreen);
    BOF3_INJECT(Gte_SetBackColor);
    BOF3_INJECT(Gte_PushMatrix);
    BOF3_INJECT(Gte_PopMatrix);
    BOF3_INJECT(Gte_ApplyMatrix);
    BOF3_INJECT(Gte_ApplyMatrixSV);
    BOF3_INJECT(Gte_ApplyMatrixLV);
    BOF3_INJECT(Gte_MulMatrix0);
    BOF3_INJECT(Gte_RotMatrixX);
    BOF3_INJECT(Gte_RotMatrixY);
    BOF3_INJECT(Gte_RotMatrixZ);
    BOF3_INJECT(Gte_RotMatrix);
    BOF3_INJECT(Gte_RotMatrixYXZ);
    BOF3_INJECT(Gte_TransMatrix);
    BOF3_INJECT(Gte_ScaleMatrix);
    BOF3_INJECT(Gte_TransposeMatrix);
    BOF3_INJECT(Gte_RotTrans);
    BOF3_INJECT(Gte_RotTransPers);
    BOF3_INJECT(Gte_DepthRamp);
    BOF3_INJECT(Gte_Perspective);
    BOF3_INJECT(Gte_RotTransPers3);
    BOF3_INJECT(Gte_RotTransPers4);
    BOF3_INJECT(Gte_RotAverage3);
    BOF3_INJECT(Gte_RotAverage4);
    BOF3_INJECT(Gte_VectorNormal);
    BOF3_INJECT(Gte_VectorNormalS);
    BOF3_INJECT(Gte_NormalColor);
    BOF3_INJECT(Gte_SetMatrix2);
    BOF3_INJECT(Gte_SetColorMatrix);
    BOF3_INJECT(Gte_SetRotMatrix);
    BOF3_INJECT(Gte_SetTransMatrix);
    BOF3_INJECT(Gte_LoadVertex);
    BOF3_INJECT(Gte_LoadVertices3);
    BOF3_INJECT(Gte_Rtps);
    BOF3_INJECT(Gte_Rtpt);
    BOF3_INJECT(Gte_StoreScreenXY);
    BOF3_INJECT(Gte_StoreScreenXY3);
    BOF3_INJECT(Gte_StoreDepthF);
    BOF3_INJECT(Gte_StoreDepthF3);
    BOF3_INJECT(Gte_StoreDepthF4);
    BOF3_INJECT(Gte_PrimDepths3_0C);
    BOF3_INJECT(Gte_PrimDepths3_10);
    BOF3_INJECT(Gte_PrimDepths4_0C);
    BOF3_INJECT(Gte_PrimDepths4_10);
    BOF3_INJECT(Gte_PrimDepthFlat4_10);
    BOF3_INJECT(Gte_PrimDepths3_10B);
    BOF3_INJECT(Gte_PrimDepths4_10B);
    BOF3_INJECT(Gte_PrimDepths4_14);
    BOF3_INJECT(Gte_PrimDepthFlat4_14);
    BOF3_INJECT(Gte_PrimDepths3_10C);
    BOF3_INJECT(Gte_PrimDepths4_10C);
    BOF3_INJECT(Gte_StoreDepthQuarter);
}

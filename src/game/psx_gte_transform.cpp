#include "game/psx_gte_transform.h"

#include <emmintrin.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The GTE's transform commands, and libgte's functions over them: a vertex
// through the current matrix, its translation added, the perspective division
// - Gte_ApplyMatrix, Gte_Perspective and Gte_DepthRamp, which are ours
// already - and the results pushed through the coprocessor's FIFOs or handed
// to the caller. Integer but for two vector normalisations, which are x87 in
// the original and `double` here for the reason psx_gte_float.cpp gives.

namespace {

long AddWrap(long a, unsigned long b) { return static_cast<long>(static_cast<unsigned long>(a) + b); }

// One vertex: rotation, translation, perspective. Gte_Transformed is left
// holding the vertex in view space, as the original leaves it.
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
    Gte_Perspective(Screen(2), Screen(2) + 1, &Gte_Depth);
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

// --- BOF3X_SHADOW=psx_gte_transform: a differential fuzz, once at start-up --------
// Byte-copies of the originals, their calls re-aimed at byte-copies of what
// they call (never at ours), run under the control word the game runs under.
// One round: every register block these touch and a scratch area of arguments
// randomised; theirs, then from the same state ours; all of it compared.

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

struct State {
    unsigned long matrix[8], matrix2[8], color_matrix[8], vertices[6], screen[6];
    short back_color[4];
    long transformed[3], near_z, proj, offset_x, offset_y, ramp_near, ramp_far, older[3], depth, ir0, otz;
    unsigned long scratch[40];
};

void Capture(State& s, const unsigned long* scratch) {
    std::memcpy(s.matrix, Gte_Matrix, sizeof s.matrix);
    std::memcpy(s.matrix2, Gte_Matrix2, sizeof s.matrix2);
    std::memcpy(s.color_matrix, Gte_ColorMatrix, sizeof s.color_matrix);
    std::memcpy(s.back_color, Gte_BackColor, 3 * sizeof(short));
    s.back_color[3] = 0;
    std::memcpy(s.vertices, Gte_Vertices, sizeof s.vertices);
    std::memcpy(s.screen, Gte_ScreenXY, sizeof s.screen);
    std::memcpy(s.transformed, Gte_Transformed, sizeof s.transformed);
    s.near_z = Gte_NearZ; s.proj = Gte_ProjDistance;
    s.offset_x = Gte_OffsetX; s.offset_y = Gte_OffsetY;
    s.ramp_near = Gte_RampNear; s.ramp_far = Gte_RampFar;
    std::memcpy(s.older, Gte_DepthOlder, sizeof s.older);
    s.depth = Gte_Depth; s.ir0 = Gte_Ir0; s.otz = Gte_Otz;
    if (scratch) std::memcpy(s.scratch, scratch, sizeof s.scratch);
}
void Apply(const State& s, unsigned long* scratch) {
    std::memcpy(Gte_Matrix, s.matrix, sizeof s.matrix);
    std::memcpy(Gte_Matrix2, s.matrix2, sizeof s.matrix2);
    std::memcpy(Gte_ColorMatrix, s.color_matrix, sizeof s.color_matrix);
    std::memcpy(Gte_BackColor, s.back_color, 3 * sizeof(short));
    std::memcpy(Gte_Vertices, s.vertices, sizeof s.vertices);
    std::memcpy(Gte_ScreenXY, s.screen, sizeof s.screen);
    std::memcpy(Gte_Transformed, s.transformed, sizeof s.transformed);
    Gte_NearZ = s.near_z; Gte_ProjDistance = s.proj;
    Gte_OffsetX = s.offset_x; Gte_OffsetY = s.offset_y;
    Gte_RampNear = s.ramp_near; Gte_RampFar = s.ramp_far;
    std::memcpy(Gte_DepthOlder, s.older, sizeof s.older);
    Gte_Depth = s.depth; Gte_Ir0 = s.ir0; Gte_Otz = s.otz;
    if (scratch) std::memcpy(scratch, s.scratch, sizeof s.scratch);
}

using Fn = long (__cdecl*)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
template <class F> Fn AsFn(F* f) { return reinterpret_cast<Fn>(reinterpret_cast<void*>(f)); }

enum Callee { kApply, kApplyLV, kPerspective, kRamp, kSin, kFtol, kNormal, kCallees };
struct CallAt { std::uint32_t offset; Callee callee; };
struct Entry {
    const char* name;
    std::uint32_t original, size;
    Fn ours;
    bool by_value, returns;   // by_value: all its arguments are numbers
    unsigned n_calls;
    CallAt calls[9];
    Fn theirs;
};

void SelfTest(Entry* entries, unsigned n) {
    constexpr unsigned kRounds = 36000;
    static State saved, input, their_out, our_out;
    static unsigned long scratch[40];
    Capture(saved, nullptr);
    const unsigned short saved_word = GetControlWord();

    std::uint32_t rng = 0x7F4A7C15u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    auto number = [&next]() -> long {
        static const long kEdge[] = {0, 1, -1, 2, 0x7FFFFFFF, static_cast<long>(0x80000000u), 0x1000, 500, 1000};
        switch (next() % 8) {
            case 0: return static_cast<long>(next());
            case 1: return kEdge[next() % 9];
            case 2: return static_cast<long>(next() % 0x2000000u) - 0x1000000;
            default: return static_cast<long>(next() % 0x4000u) - 0x1000;
        }
    };

    unsigned bad = 0, overlapping = 0, zero_vectors = 0, byte_overlaps = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        input.matrix[5] = static_cast<unsigned long>(number());
        input.matrix[6] = static_cast<unsigned long>(number());
        input.matrix[7] = static_cast<unsigned long>(number());
        input.near_z = number(); input.proj = number();
        input.offset_x = number(); input.offset_y = number();
        input.ramp_near = number(); input.ramp_far = number();
        for (long& v : input.older) v = number();
        input.depth = number();
        // Half the time the scratch holds numbers rather than noise: a vector
        // to normalise is three of them, and all zero now and then.
        if (next() % 2 == 0)
            for (unsigned long& v : input.scratch) v = static_cast<unsigned long>(number());
        const bool zero = next() % 16 == 0;
        if (zero) { std::memset(input.scratch, 0, sizeof input.scratch); ++zero_vectors; }

        const Entry& e = entries[round % n];
        // Nine argument cells: four vertices, four outs of three dwords, one
        // more. One round in eight they land anywhere, overlapping.
        unsigned at[9] = {0, 2, 4, 6, 8, 11, 14, 17, 20};
        if (next() % 8 == 0) { for (unsigned& a : at) a = next() % 22; ++overlapping; }
        const long value = number(), value2 = number(), value3 = number();
        // The colours are bytes: when cells overlap, in and out a few bytes apart.
        unsigned char* color_out = nullptr;
        if (e.original == bof3::addr::Gte_NormalColor && next() % 4 == 0) {
            color_out = reinterpret_cast<unsigned char*>(scratch + at[1]) + static_cast<int>(next() % 7) - 3;
            if (color_out < reinterpret_cast<unsigned char*>(scratch)) color_out = nullptr; else ++byte_overlaps;
        }

        long result[2] = {0, 0};
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, scratch);
            void* a[9];
            for (unsigned i = 0; i < 9; ++i) a[i] = scratch + at[i];
            if (e.by_value) {
                a[0] = reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value)));
                a[1] = reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value2)));
                a[2] = reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value3)));
            }
            if (color_out) a[2] = color_out;
            if (pass == 0) SetControlWord(kGameControlWord);
            result[pass] = (pass ? e.ours : e.theirs)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]);
            if (pass == 0) SetControlWord(saved_word);
            Capture(pass ? our_out : their_out, scratch);
        }
        const bool same = std::memcmp(&their_out, &our_out, sizeof their_out) == 0 &&
                          (!e.returns || result[0] == result[1]);
        if (!same && ++bad <= 8)
            bof3::Log("shadow      psx_gte_transform self-test MISMATCH: %s, round %u, cells %u %u %u %u %u %u %u %u %u, "
                      "results %08lX / %08lX", e.name, round, at[0], at[1], at[2], at[3], at[4], at[5], at[6], at[7],
                      at[8], static_cast<unsigned long>(result[0]), static_cast<unsigned long>(result[1]));
    }
    Apply(saved, nullptr);
    SetControlWord(saved_word);
    bof3::Log("shadow      psx_gte_transform self-test: %u functions, %u rounds under control word %04X (%u with "
              "overlapping arguments, %u with an all-zero scratch, %u with colours a few bytes apart), %u MISMATCHES; "
              "every register block, the arguments and the results compared", n, kRounds, kGameControlWord,
              overlapping, zero_vectors, byte_overlaps, bad);
    if (bad) bof3::Fatal("the GTE's transforms differ from the originals in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

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
    Project(Vertex(kV2), Screen(2), Screen(2) + 1, &Gte_Depth);
    Gte_Ir0 = Gte_DepthRamp(Gte_Depth);
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

// originals 0x5A84A0, 0x5A85F0: libgte RotTransPers3 and RotTransPers4 by
// shape. The screen points go to the caller and NOT through the FIFO; the
// depths do go to the depth registers.
extern "C" long __cdecl Gte_RotTransPers3(const short* v0, const short* v1, const short* v2, float* sxy0, float* sxy1,
                                          float* sxy2, long* p) {
    Gte_DepthOlder[2] = Gte_Depth;
    Project(v0, sxy0, sxy0 + 1, &Gte_DepthOlder[1]);
    Project(v1, sxy1, sxy1 + 1, &Gte_DepthOlder[0]);
    Project(v2, sxy2, sxy2 + 1, &Gte_Depth);
    *p = Gte_DepthRamp(Gte_Depth);
    return Gte_Depth >> 2;
}
extern "C" long __cdecl Gte_RotTransPers4(const short* v0, const short* v1, const short* v2, const short* v3,
                                          float* sxy0, float* sxy1, float* sxy2, float* sxy3, long* p) {
    Project(v0, sxy0, sxy0 + 1, &Gte_DepthOlder[2]);
    Project(v1, sxy1, sxy1 + 1, &Gte_DepthOlder[1]);
    Project(v2, sxy2, sxy2 + 1, &Gte_DepthOlder[0]);
    Project(v3, sxy3, sxy3 + 1, &Gte_Depth);
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
    Project(v2, out2, out2 + 1, &Gte_Depth);
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
    Project(v3, out3, out3 + 1, &Gte_Depth);
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

// originals 0x5A8DC0, 0x5A7B60: libgte SetColorMatrix and SetBackColor by
// shape - the second stores each component << 4, as a word.
extern "C" void __cdecl Gte_SetColorMatrix(const unsigned long* matrix) { std::memmove(Gte_ColorMatrix, matrix, 8 * 4); }
extern "C" void __cdecl Gte_SetBackColor(long r, long g, long b) {
    Gte_BackColor[0] = static_cast<short>(static_cast<unsigned long>(r) << 4);
    Gte_BackColor[1] = static_cast<short>(static_cast<unsigned long>(g) << 4);
    Gte_BackColor[2] = static_cast<short>(static_cast<unsigned long>(b) << 4);
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

// original 0x5A7A50: the cosine, as the sine a quarter turn on.
extern "C" int __cdecl Math_Cos(int angle) {
    return Math_Sin(static_cast<int>(static_cast<unsigned>(angle) + 0x400u));
}

void PsxGteTransform_Inject() {
    // Called BEFORE the modules that own what these call, because a function
    // can only be cloned before its entry is patched.
    if (bof3::WantsShadow("psx_gte_transform")) {
        const void* callee[kCallees];
        callee[kApply] = bof3::CloneOriginal("Gte_ApplyMatrix", bof3::addr::Gte_ApplyMatrix, 0x7B);
        callee[kApplyLV] = bof3::CloneOriginal("Gte_ApplyMatrixLV", bof3::addr::Gte_ApplyMatrixLV, 0x78);
        callee[kNormal] = nullptr;   // filled in below, once Gte_VectorNormal is cloned
        callee[kPerspective] = bof3::CloneOriginal("Gte_Perspective", bof3::addr::Gte_Perspective, 0x111);
        const bof3::CloneCall ftol{0x3B, nullptr};
        callee[kRamp] = bof3::CloneOriginal("Gte_DepthRamp", bof3::addr::Gte_DepthRamp, 0x40, &ftol, 1);
        callee[kSin] = bof3::CloneOriginal("Math_Sin", bof3::addr::Math_Sin, 0x42);
        callee[kFtol] = nullptr;   // the C runtime's, never ours: where the original called

        // Offsets and targets of every call: capstone over each function, 2026-09-20.
        // No jump leaves any of them.
        static Entry entries[] = {
            {"Gte_Rtps", bof3::addr::Gte_Rtps, 0xC6, AsFn(&Gte_Rtps), false, false, 3,
             {{0xF, kApply}, {0xAC, kPerspective}, {0xB8, kRamp}}, nullptr},
            {"Gte_Rtpt", bof3::addr::Gte_Rtpt, 0x145, AsFn(&Gte_Rtpt), false, false, 7,
             {{0x19, kApply}, {0x66, kPerspective}, {0x7A, kApply}, {0xC7, kPerspective}, {0xDB, kApply},
              {0x128, kPerspective}, {0x137, kRamp}}, nullptr},
            {"Gte_RotTransPers", bof3::addr::Gte_RotTransPers, 0xE4, AsFn(&Gte_RotTransPers), false, true, 3,
             {{0xF, kApply}, {0xAC, kPerspective}, {0xB8, kRamp}}, nullptr},
            {"Gte_RotTransPers3", bof3::addr::Gte_RotTransPers3, 0x14B, AsFn(&Gte_RotTransPers3), false, true, 7,
             {{0x19, kApply}, {0x63, kPerspective}, {0x77, kApply}, {0xC3, kPerspective}, {0xD7, kApply},
              {0x125, kPerspective}, {0x134, kRamp}}, nullptr},
            {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, 0x1A7, AsFn(&Gte_RotTransPers4), false, true, 9,
             {{0xF, kApply}, {0x5B, kPerspective}, {0x6F, kApply}, {0xBD, kPerspective}, {0xD1, kApply},
              {0x11F, kPerspective}, {0x136, kApply}, {0x184, kPerspective}, {0x190, kRamp}}, nullptr},
            {"Gte_RotAverage3", bof3::addr::Gte_RotAverage3, 0x1A6, AsFn(&Gte_RotAverage3), false, true, 7,
             {{0x1C, kApply}, {0x6A, kPerspective}, {0x7E, kApply}, {0xCC, kPerspective}, {0xE0, kApply},
              {0x12C, kPerspective}, {0x168, kRamp}}, nullptr},
            {"Gte_RotAverage4", bof3::addr::Gte_RotAverage4, 0x208, AsFn(&Gte_RotAverage4), false, true, 9,
             {{0x13, kApply}, {0x61, kPerspective}, {0x75, kApply}, {0xC3, kPerspective}, {0xD7, kApply},
              {0x125, kPerspective}, {0x13C, kApply}, {0x188, kPerspective}, {0x1D0, kRamp}}, nullptr},
            {"Gte_VectorNormal", bof3::addr::Gte_VectorNormal, 0x9C, AsFn(&Gte_VectorNormal), false, true, 4,
             {{0x59, kFtol}, {0x72, kFtol}, {0x86, kFtol}, {0x92, kFtol}}, nullptr},
            {"Gte_VectorNormalS", bof3::addr::Gte_VectorNormalS, 0x9F, AsFn(&Gte_VectorNormalS), false, true, 4,
             {{0x59, kFtol}, {0x73, kFtol}, {0x88, kFtol}, {0x95, kFtol}}, nullptr},
            {"Gte_ScaleMatrix", bof3::addr::Gte_ScaleMatrix, 0x8B, AsFn(&Gte_ScaleMatrix), false, true, 0, {}, nullptr},
            {"Math_Cos", bof3::addr::Math_Cos, 0x13, AsFn(&Math_Cos), true, true, 1, {{0xA, kSin}}, nullptr},
            {"Gte_NormalColor", bof3::addr::Gte_NormalColor, 0xFA, AsFn(&Gte_NormalColor), false, false, 7,
             {{0x14, kApply}, {0x23, kNormal}, {0x37, kApplyLV}, {0x46, kNormal}, {0x9B, kFtol}, {0xBD, kFtol},
              {0xDC, kFtol}}, nullptr},
            {"Gte_SetColorMatrix", bof3::addr::Gte_SetColorMatrix, 0x15, AsFn(&Gte_SetColorMatrix), false, false, 0, {},
             nullptr},
            {"Gte_SetBackColor", bof3::addr::Gte_SetBackColor, 0x2A, AsFn(&Gte_SetBackColor), true, false, 0, {}, nullptr},
        };
        for (auto& e : entries) {
            bof3::CloneCall calls[9];
            for (unsigned i = 0; i < e.n_calls; ++i) calls[i] = {e.calls[i].offset, callee[e.calls[i].callee]};
            e.theirs = reinterpret_cast<Fn>(
                bof3::CloneOriginal(e.name, e.original, e.size, calls, static_cast<int>(e.n_calls)));
            if (e.original == bof3::addr::Gte_VectorNormal) callee[kNormal] = reinterpret_cast<const void*>(e.theirs);
        }
        SelfTest(entries, sizeof entries / sizeof entries[0]);
    }
    BOF3_INJECT(Gte_Rtps);
    BOF3_INJECT(Gte_Rtpt);
    BOF3_INJECT(Gte_RotTransPers);
    BOF3_INJECT(Gte_RotTransPers3);
    BOF3_INJECT(Gte_RotTransPers4);
    BOF3_INJECT(Gte_RotAverage3);
    BOF3_INJECT(Gte_RotAverage4);
    BOF3_INJECT(Gte_VectorNormal);
    BOF3_INJECT(Gte_VectorNormalS);
    BOF3_INJECT(Gte_ScaleMatrix);
    BOF3_INJECT(Math_Cos);
    BOF3_INJECT(Gte_NormalColor);
    BOF3_INJECT(Gte_SetColorMatrix);
    BOF3_INJECT(Gte_SetBackColor);
}

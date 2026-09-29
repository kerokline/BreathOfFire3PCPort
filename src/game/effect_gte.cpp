// The effect engine's GTE helpers - round thirteen, stage-A group EGT, the four
// functions 0x494060..0x494270 of the cut (analysis/round13_cut.tsv), taken
// before the round's 35 effect groups so that every one of them calls these by
// name. Each read with capstone to its last instruction (docs/effect_gte.md
// section 1); the labelling tool's "effect kind 186" for them was a hypothesis
// by address, and they are not a kind's code but helpers the kinds call.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. Where the original divides by a
// depth that can be 0 (EffectGte_ProjectSize), ours aborts with a message.
#include "game/effect_gte.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;

// A value's low 16 bits, as the originals' `mov word ptr [...], reg` stores it.
short Low(U v) { return static_cast<short>(static_cast<std::uint16_t>(v)); }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32.
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

// A world point (x, z, height) to the camera's s16 vector, as 0x494110 and
// 0x4941E0 both build it: all three dwords read first (z, x, height in the
// original's order - nothing is written between), then x and z shifted
// arithmetically by 9 less 0x4000, the height's integer part (>> 16) halved
// toward zero (cdq; sub eax, edx; sar eax, 1) and negated. The fourth s16 is
// not written; no callee reads it.
void CameraVector(const long* point, short* v) {
    const long z = point[1];
    const long x = point[0];
    const long height = point[2];
    v[1] = Low(static_cast<U>(z >> 9) - 0x4000u);
    v[2] = Low(static_cast<U>(-((height >> 16) / 2)));
    v[0] = Low(static_cast<U>(x >> 9) - 0x4000u);
}

}  // namespace

// original 0x494060 (0xAB bytes): the map camera loaded into the GTE. A stack
// MATRIX gets Camera_Angles' rotation; the focus words - MapView_FocusX,
// MapView_FocusZ, MapView_Elevation each shifted right by 1 (shr: the stored
// low word is the same as an arithmetic shift's) - are read after that call and
// turned by it; Camera_ShiftX and Camera_ShiftY (s16) are added to the turned x
// and y and Camera_Distance (s16) plus 0x1194 to its z, all read after the
// turn, into the MATRIX's translation. The rotation is computed again from
// Camera_Angles (the original's second Gte_RotMatrix, the angles re-read) and
// both halves loaded. eax at the original's ret is Gte_SetTransMatrix's last
// load, the translation's z.
extern "C" long __cdecl EffectGte_LoadMapCamera(void) {
    Matrix matrix;
    SH_CALL(Gte_RotMatrix)(Camera_Angles, matrix.m);
    short focus[4];
    focus[0] = Low(static_cast<U>(MapView_FocusX) >> 1);
    focus[1] = Low(static_cast<U>(MapView_FocusZ) >> 1);
    focus[2] = Low(static_cast<U>(MapView_Elevation) >> 1);
    long turned[4];
    SH_CALL(Gte_ApplyMatrix)(matrix.m, focus, turned);
    matrix.t[0] = static_cast<long>(static_cast<U>(static_cast<long>(Camera_ShiftX)) + static_cast<U>(turned[0]));
    matrix.t[1] = static_cast<long>(static_cast<U>(static_cast<long>(Camera_ShiftY)) + static_cast<U>(turned[1]));
    matrix.t[2] = static_cast<long>(static_cast<U>(static_cast<long>(Camera_Distance)) + static_cast<U>(turned[2]) +
                                    0x1194u);
    SH_CALL(Gte_RotMatrix)(Camera_Angles, matrix.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&matrix));
    return matrix.t[2];
}

// original 0x494110 (0x65 bytes): a world point projected. The camera vector
// (CameraVector) through Gte_RotTransPers(vector, out, p, flag) - the screen x
// and y as two floats to out[0..1] - where the original hands its OWN argument
// slots as p and flag: the depth-cue value lands in the slot that held `out`
// (the original has `out` in esi by then) and the flag, which the callee never
// touches, is the slot of `point`. Ours hands the same slot for p; our
// Gte_RotTransPers has no flag argument. Then Gte_StoreDepthF(out + 2). eax at
// the original's ret is Gte_StoreDepthF's argument.
extern "C" float* __cdecl EffectGte_ProjectPoint(const long* point, float* out) {
    short vector[4];
    CameraVector(point, vector);
    float* const screen = out;
    SH_CALL(Gte_RotTransPers)(vector, reinterpret_cast<unsigned long*>(screen), reinterpret_cast<long*>(&out));
    SH_CALL(Gte_StoreDepthF)(screen + 2);
    return screen + 2;
}

// original 0x494180 (0x2F bytes): nine word stores - 1, 0, 0, 0, 1, 0, 0, 0,
// 1 - at +0..+0x10, in that order. eax is the matrix (loaded first).
extern "C" short* __cdecl EffectGte_SetDiagonalOne(short* matrix) {
    matrix[0] = 1;
    matrix[1] = 0;
    matrix[2] = 0;
    matrix[3] = 0;
    matrix[4] = 1;
    matrix[5] = 0;
    matrix[6] = 0;
    matrix[7] = 0;
    matrix[8] = 1;
    return matrix;
}

// original 0x4941E0 (0x91 bytes): a size at a point's depth. The camera vector
// (CameraVector) through Gte_RotTrans(vector, turned, flag) - the flag the
// slot of `point`, never touched by the callee; then, the depth turned[2]:
// size[0] (s16) * 1000 / depth (idiv: toward zero) stored as a word to out[0],
// then size[1] likewise to out[1] - size[1] read after out[0] is written.
// eax at the original's ret is the second quotient. A depth of 0 is a divide
// fault in the original (no handler: the process ends); ours aborts with a
// message (docs/effect_gte.md section 5 on whether play reaches it).
extern "C" long __cdecl EffectGte_ProjectSize(const long* point, const short* size, short* out) {
    short vector[4];
    CameraVector(point, vector);
    long turned[4];
    SH_CALL(Gte_RotTrans)(vector, turned);
    const long depth = turned[2];
    if (depth == 0)
        bof3::Fatal("EffectGte_ProjectSize (0x4941E0): the point (%ld, %ld, %ld) is at depth 0 in the camera; the "
                    "original divides by it (a divide fault)",
                    point[0], point[1], point[2]);
    out[0] = Low(static_cast<U>(static_cast<long>(size[0]) * 1000 / depth));
    const long second = static_cast<long>(size[1]) * 1000 / depth;
    out[1] = Low(static_cast<U>(second));
    return second;
}

void EffectGte_Inject() {
    if (bof3::WantsShadow("effect_gte")) effect_gte::SelfTest();
    BOF3_INJECT(EffectGte_LoadMapCamera);
    BOF3_INJECT(EffectGte_ProjectPoint);
    BOF3_INJECT(EffectGte_SetDiagonalOne);
    BOF3_INJECT(EffectGte_ProjectSize);
}

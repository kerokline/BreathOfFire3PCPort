// Moved here by tools/move_funcs.py from effect_gte.cpp, frame_callees.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/effect/effect_gte.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/effect_gte.h"
#include "game/frame_callees.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3g::at;

}  // namespace

namespace {

using namespace move_script;

}  // namespace

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

using Fn = void (__cdecl*)();

Fn HandlerFrameCallees(unsigned long entry) { return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(entry)); }

constexpr unsigned kEffects = 20, kEffectBytes = 0x80;

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 (its low dword 0); the callers keep eax or its low word.
U FtolRest3g(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

}  // namespace

// original 0x494030: the effect objects' frame (PSX FUN_8019B0EC, term for
// term). Each of the 20 whose byte +0 is non-zero - any bit, read afresh each
// turn, so an object a handler spawns further on runs this frame - becomes
// Sprite_Current and runs Effect_KindHandlers[+5], unbounded as the
// original's.
extern "C" void __cdecl Effect_RunObjects(void) {
    unsigned char* object = rv::Bytes(Effect_Objects);
    static int log_on = -1;
    if (log_on < 0) {
        char v[8];
        log_on = GetEnvironmentVariableA("BOF3X_EFFECTLOG", v, sizeof v) != 0 && v[0] == '1';
    }
    for (unsigned n = kEffects; n != 0; --n, object += kEffectBytes) {
        if (object[0] == 0) continue;
        if (log_on == 1 && object[1] == 0)
            bof3::Log("effect      run slot %u handler 0x%02X param 0x%02X +9 %u frame %lu", kEffects - n, object[5],
                      object[6], object[9], static_cast<unsigned long>(Frame_Counter));
        Sprite_Current = object;
        HandlerFrameCallees(Effect_KindHandlers[object[5]])();
    }
}

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
// DIV-0126: with the switch armed the diagonal is 0x1000, the GTE's identity in
// 4.12, which the PlayStation twin 0x801B0F5C stores (`addiu v0, zero, 0x1000`;
// read in JP GAME.EMI). Capcom's PC 1 collapses the seven callers' turned
// edges to 0, 1 and -1 (known-defects D215): the lighthouse's rays (D243),
// kind 0x4B's 32 POLY_G3, drawn as points.
extern "C" short* __cdecl EffectGte_SetDiagonalOne(short* matrix) {
    const short one = effect_gte::g_identity ? 0x1000 : 1;
    matrix[0] = one;
    matrix[1] = 0;
    matrix[2] = 0;
    matrix[3] = 0;
    matrix[4] = one;
    matrix[5] = 0;
    matrix[6] = 0;
    matrix[7] = 0;
    matrix[8] = one;
    return matrix;
}

// original 0x4941B0 (PSX 0x801B0F88; cdecl; EffectKind44_RingCone, EyeBeam_DrawCylinder and
// Shisu_DrawModel test its ax): the z of (b - a) x (c - b) of three screen
// points, (c.y - b.y)(b.x - a.x) - (b.y - a.y)(c.x - b.x) on x87, through a
// tail jump to _ftol (truncated; edx:eax, eax answered).
extern "C" int __cdecl Screen_TriangleWinding(const float* a, const float* b, const float* c) {
    const long double first = (static_cast<long double>(c[1]) - static_cast<long double>(b[1])) *
                              (static_cast<long double>(b[0]) - static_cast<long double>(a[0]));
    const long double second = (static_cast<long double>(b[1]) - static_cast<long double>(a[1])) *
                               (static_cast<long double>(c[0]) - static_cast<long double>(b[0]));
    return static_cast<int>(FtolRest3g(first - second));
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

void EffectEffectGteFromEffectGte_Inject() {
    BOF3_INJECT(EffectGte_LoadMapCamera);
    BOF3_INJECT(EffectGte_ProjectPoint);
    BOF3_INJECT(EffectGte_SetDiagonalOne);
    BOF3_INJECT(EffectGte_ProjectSize);
}

void EffectEffectGteFromFrameCallees_Inject() {
    BOF3_INJECT(Effect_RunObjects);
}

void EffectEffectGteFromRest3g_Inject() {
    BOF3_INJECT(Screen_TriangleWinding);
}

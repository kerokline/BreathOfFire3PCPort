// The effect engine's four GTE helpers, originals 0x494060..0x494270 (round
// thirteen's stage-A group EGT): the map camera loaded into the GTE, a world
// point projected to the screen, a 3 x 3 of s16 set to a diagonal of ones, and
// a world-space size scaled to the screen at a point's depth. Every effect
// group of round thirteen calls them, so they are taken first and called by
// name. docs/effect_gte.md.
//
// All four are cdecl and read their arguments from the stack; none reads a
// register it was not handed. The world point they take (`point`) is three
// dwords of the engine's world space: x, z and the height, the height's
// integer part in its high word (docs/effect_gte.md section 2).
#pragma once

extern "C" {

// original 0x494060 (PSX twin 0x801B0E10): the map camera into the GTE. The
// rotation from Camera_Angles (Gte_RotMatrix); the view focus halved
// (MapView_FocusX, MapView_FocusZ, MapView_Elevation, each >> 1 to an s16)
// turned by that rotation (Gte_ApplyMatrix); the translation that vector plus
// (Camera_ShiftX, Camera_ShiftY, Camera_Distance + 0x1194); both loaded
// (Gte_SetRotMatrix, Gte_SetTransMatrix). Takes nothing. Answers in eax the
// translation's z, as the original leaves it (no caller found reading it).
long __cdecl EffectGte_LoadMapCamera(void);

// original 0x494110 (PSX twin 0x801B0EFC): `point` projected through the
// current GTE matrix. The camera vector is (x >> 9) - 0x4000, (z >> 9) -
// 0x4000, -((height >> 16) / 2), each to an s16; Gte_RotTransPers writes the
// screen x and y to out[0] and out[1] (floats), Gte_StoreDepthF the depth /
// 16384 to out[2]. `point` is read whole before `out` is written, so the two
// may overlap. Answers in eax out + 2, as the original leaves it (no caller
// found reading it).
float* __cdecl EffectGte_ProjectPoint(const long* point, float* out);

// original 0x494180 (PSX twin 0x801B0F5C): the nine s16 of a MATRIX's rotation
// set to 1 on the diagonal and 0 elsewhere, in order; the padding and the
// translation are not touched. NOT the identity - the GTE's one is 0x1000,
// which is what the PSX twin writes (docs/effect_gte.md section 5). Answers
// in eax the matrix it was handed.
short* __cdecl EffectGte_SetDiagonalOne(short* matrix);

// original 0x4941E0 (PSX twin 0x801B0FD0): `point` turned into the camera
// vector as EffectGte_ProjectPoint turns it and through the current matrix
// (Gte_RotTrans); then out[0] = size[0] * 1000 / depth and out[1] = size[1] *
// 1000 / depth, the depth the turned vector's z, each quotient truncated
// toward zero and stored as its low 16 bits. size[0] is read, out[0] written,
// then size[1] read, out[1] written - so out may be size (callers pass it in
// place). A depth of 0 is the original's divide fault: ours aborts with a
// message. Answers in eax the second quotient, all 32 bits (no caller found
// reading it).
long __cdecl EffectGte_ProjectSize(const long* point, const short* size, short* out);

}  // extern "C"

void EffectGte_Inject();

namespace effect_gte {
// BOF3X_SHADOW=effect_gte: the start-up fuzz, effect_gte_fuzz.cpp. Clones the
// four originals before EffectGte_Inject patches them.
void SelfTest();
}  // namespace effect_gte

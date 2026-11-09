// Moved here by tools/move_funcs.py from effect_1a.cpp, rest_4f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_09.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/config_text.h"
#include "game/effect/effect_kind_09_callees.h"
#include "game/effect_1a.h"
#include "game/effect_1a_callees.h"
#include "game/fishing_text.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4f.h"
#include "game/rest_4f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_kind_09::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

unsigned char* S() { return Sprite_Current; }

U L(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U L(U a) { return L(At(a)); }

void SetL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }

void SetW(unsigned char* p, U v) { move_script::SetWord(p, v); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(move_script::Word(p)); }

using Handler = void (__cdecl*)();

// jmp [table + byte * 4]: the table's `entries` handlers, read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next table's entry or data.
void Run(const char* who, U table, unsigned entries, unsigned state) {
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_1a.md section 6)",
                    who, state, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * state)))();
}

// Sprite_ObjectsExtra record `index` (a dword of the effect record): four of them.
unsigned char* Extra(const char* who, U index) {
    if (index >= at::kExtraCount)
        bof3::Fatal("%s: extra sprite index %u, past the four Sprite_ObjectsExtra records - the original reads what "
                    "follows (docs/effect_1a.md section 6)",
                    who, (unsigned)index);
    return At(::bof3::Img(Sprite_ObjectsExtra_addr) + index * at::kObjectStride);
}

void PrimFromRect(unsigned char* prim, unsigned char* rect) {
    SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), at::kPrimFromRect.v)(prim, rect);
}

// `fld dword [a]; fadd dword [c]; fstp dword [o]`
void FldAdd(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%1)\n\tfadds (%2)\n\tfstps (%0)" : : "r"(o), "r"(a), "r"(c) : "st", "memory");
}

// `fld dword [a]; fsub dword [c]; fstp dword [o]`
void FldSub(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%1)\n\tfsubs (%2)\n\tfstps (%0)" : : "r"(o), "r"(a), "r"(c) : "st", "memory");
}

// `fld dword [a]; fstp dword [o]` (a quiet copy through the FPU)
void Fld(const void* a, void* o) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(o), "r"(a) : "st", "memory");
}

// The CRT's _ftol 0x5B9550 on st(0): truncation toward zero set in a copy of
// the control word for one fistp to 64 bits, the word put back; the low dword.
// `fild qword [lo, 0]; fadd dword [f]; call _ftol`
U FildAddFtol(U lo, const void* f) {
    const std::uint64_t v = lo;
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "fildll %[v]\n\t"
        "fadds (%[f])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [v] "m"(v), [f] "r"(f)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// `fld dword [a]; fsub dword [b]; call _ftol`
U FldSubFtol(const void* a, const void* b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fsubs (%[b])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a), [b] "r"(b)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

void CopyDword(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, 4); }

// The RECT the two quad kinds wrap their primitive in: 8 bytes taken off the
// packet, then the library's primitive built from it at the cursor (12 bytes)
// and linked at Sprite_Current's point (0xC).
void AreaRect(U y, U w, U h) {
    unsigned char* const rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetW(rect, 0);
    SetW(rect + 2, y);
    SetW(rect + 4, w);
    SetW(rect + 6, h);
    PrimFromRect(Gfx_PacketNext, rect);
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(L(s + 0x34), L(s + 0x38), 0x11, 0xC);
}

// A world point with an attachment's offset turned into the GTE's vector: x
// and z as (v >> 9) - 0x4000, the height -(s16 +0x3E + out[2]) / 2, each an
// s16. The fourth word is 0: the original never writes it (stale stack), so
// it cannot be reproduced, only replaced - DIV-0023's ruling, as field_e2's
// Mode11_ObjectDraw; the projection reads x, y and z.
void AttachVector(const unsigned char* o, const long* out, short* v) {
    v[0] = static_cast<short>(static_cast<U>((static_cast<std::int32_t>(L(o + 0x34) + static_cast<U>(out[0]))) >> 9) - 0x4000u);
    v[1] = static_cast<short>(static_cast<U>((static_cast<std::int32_t>(L(o + 0x38) + static_cast<U>(out[1]))) >> 9) - 0x4000u);
    const std::int32_t h = static_cast<std::int32_t>(static_cast<U>(SW(o + 0x3E)) + static_cast<U>(out[2]));
    v[2] = static_cast<short>(-(h / 2));
    v[3] = 0;
}

using LD = long double;

using move_script::Long;

using move_script::SetLong;

unsigned char& BRest4f(U a) { return move_script::At(a)[0]; }

std::int32_t SLRest4f(const unsigned char* p) { return Long(p); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// --- x87 as the original has it (rest_3f.cpp's form): `fld dword` through inline
// assembly, every operation in long double, `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}

LD I(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

// The Sprite_Objects record kind 8 traces to: +6's byte, unchecked in the
// original (ours aborts past the thirty).
const unsigned char* TraceObject(const char* who) {
    const unsigned idx = S()[6];
    if (idx >= rest_4f::at::kObjectCount)
        bof3::Fatal("%s: the object byte +6 is %u, past the thirty Sprite_Objects records - the original reads what "
                    "follows (docs/rest_4f.md section 7)",
                    who, idx);
    return rv::Bytes(&Sprite_Objects[idx]);
}

// One run of kind 8's line along z, in steps of 0x8000 (`dir` +1 or -1) from z to
// the object's z: each step's height the ground's (its high word; the low word
// is what the frame's height cell held - never read, EffectKind08_DrawLine reads
// the high word), the last the object's own height; the line from the new point
// back to the last (`back`: from the last to the new).
struct Trace {
    U x, z, h, hd;
};

void RunRest4f(Trace& t, std::int32_t target, U oh, int dir, bool back, U colour) {
    for (bool more = true; more;) {
        std::int32_t nz = static_cast<std::int32_t>(t.z + static_cast<U>(dir * 0x8000));
        U nh;
        if (dir > 0 ? nz > target : nz < target) {
            nz = target;
            t.hd = oh;
            nh = oh;
            more = false;
        } else {
            const long e = SH_CALL(AreaMap_Elevation)(static_cast<long>(t.x), nz);
            t.hd = (t.hd & 0xFFFFu) | (static_cast<U>(e) << 16);
            nh = t.hd;
        }
        if (back)
            SH_CALL(EffectKind08_DrawLine)(static_cast<int>(t.x), static_cast<int>(t.z), static_cast<int>(t.h),
                                           static_cast<int>(t.x), nz, static_cast<int>(nh), static_cast<int>(colour));
        else
            SH_CALL(EffectKind08_DrawLine)(static_cast<int>(t.x), nz, static_cast<int>(nh), static_cast<int>(t.x),
                                           static_cast<int>(t.z), static_cast<int>(t.h), static_cast<int>(colour));
        t.z = static_cast<U>(nz);
        t.h = nh;
    }
}

// Kind 9's fans, the n-th drawn with its length and its three angles (each
// state draws the first n; Sprite_Current read before each).
void Fan(unsigned n) {
    const unsigned char* const s = S();
    switch (n) {
    case 1: SH_CALL(EffectKind09_DrawFan)(SLRest4f(s + 0x18), 0x200, 0, 0); break;
    case 2: SH_CALL(EffectKind09_DrawFan)(SLRest4f(s + 0x1C), 0x71, 0, 0x8E3); break;
    case 3: SH_CALL(EffectKind09_DrawFan)(s[0xB], 0x155, 0, 0x200); break;
    case 4: SH_CALL(EffectKind09_DrawFan)(s[8], 0xE3, 0, 0xE38); break;
    case 5: SH_CALL(EffectKind09_DrawFan)(s[0xA], 0x31C, 0, 0x555); break;
    case 6: SH_CALL(EffectKind09_DrawFan)(s[6], 0x238, 0, 0xAAA); break;
    case 7: SH_CALL(EffectKind09_DrawFan)(s[7], 0, 0, 0x71C); break;
    default: SH_CALL(EffectKind09_DrawFan)(SLRest4f(s + 0x20), 0, 0, 0xC00); break;
    }
}

void Fans(unsigned n) {
    for (unsigned i = 1; i <= n; ++i) Fan(i);
}

// A byte length up by `step`; sound 0x20E at `mid` (0 for none); at 0x60 the
// sound and +1 = next.
void GrowByte(unsigned off, unsigned step, unsigned mid, unsigned char next) {
    unsigned char* const s = S();
    s[off] = static_cast<unsigned char>(s[off] + step);
    if (mid != 0 && S()[off] == mid) Sound(0x20E);
    if (S()[off] != 0x60) return;
    Sound(0x20E);
    S()[1] = next;
}

// Kind 9's triangle: a POLY_G3 at Gfx_PacketNext of the three vectors, projected
// under the record's object matrix (Gte_PushMatrix, Sprite_ObjectMatrix,
// Camera_LoadMatrix ... Gte_PopMatrix). `colours` sets the vertex colours before
// the semi-transparency; committed (+0x29, 0x34).
template <typename C> void Triangle(const short* v0, const short* v1, const short* v2, C colours) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(prim);
    SH_CALL(Gte_PushMatrix)();
    short m[16];
    SH_CALL(Sprite_ObjectMatrix)(m);
    SH_CALL(Camera_LoadMatrix)(m);
    long p;
    SH_CALL(Gte_RotTransPers3)(v0, v1, v2, reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                               reinterpret_cast<float*>(prim + 0x28), &p);
    SH_CALL(Gte_PrimDepths3_10B)(prim);
    colours(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0x34);
    SH_CALL(Gte_PopMatrix)();
}

// (Math_Cos or Math_Sin of `angle`) * `len` >> 8, as the original's imul / sar.
short Arm(int (__cdecl* f)(int), int angle, U len) {
    const U v = static_cast<U>(f(angle));
    return static_cast<short>(Sar(v * len, 8));
}

// Kinds 0xB's fades: +0x5D down `step`, +0x5F and +0x5E down `step` (a byte);
// +6 up one when +9 & mask is 0. False (nothing changed) at +0x5D 0.
bool Fade0B(unsigned step, unsigned mask) {
    unsigned char* const s = S();
    const unsigned char v = s[0x5D];
    if (v == 0) return false;
    s[0x5D] = static_cast<unsigned char>(v - step);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] - step);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] - step);
    if ((s[9] & mask) == 0) s[6] = static_cast<unsigned char>(s[6] + 1);
    return true;
}

}  // namespace

// original 0x462BA0 (Effect_KindHandlers 1): EffectKind01_States by +1.
extern "C" void __cdecl EffectKind01_Run(void) { Run("EffectKind01_Run", ::bof3::Img(EffectKind01_States_addr), 2, S()[1]); }

// original 0x462BC0 (EffectKind01_States[0]): +0x54 = the dword +0x54 of the
// Sprite_ObjectsExtra record the dword +0x18 names; +1 = 1.
extern "C" void __cdecl EffectKind01_Start(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->frames, rv::As<records::SpriteObject>(Extra("EffectKind01_Start", static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18)))->frames);
    S()[1] = 1;
}

// original 0x462BF0 (EffectKind01_States[1]): with Sprite_Current the extra
// record +0x18 names, its point copied into the effect record's +0x34..+0x3F;
// a RECT (0, 0xF0, 16, 16) primitive linked at it; a POLY_FT4 at the cursor:
// the extra's point plus MoveCmd_AttachOffset(+0x1C) projected (the depth to
// the effect's +0x60, Gte_PrimDepthFlat4_10), the corners x - 8 / x + 8 over
// y (the top pair's y 0) - floats; then Sprite_Current back to the effect
// record, CLUT (0xB0, 0x1E3), tpage (0, 0, 0x2C0, 0x100), u 0 / 0x10, v
// (Frame_Counter >> 2) & 0x15 on top and that plus the bottom's y through
// _ftol below, shade 0x80, Gpu_SetShadeTex(1), linked (0x48); a RECT (0, 0,
// 0x100, 0x100) primitive linked (0xC). PSX: no twin (catalog part 2).
extern "C" void __cdecl EffectKind01_Draw(void) {
    unsigned char* const e = S();
    auto* const eo = rv::As<records::EffectObject>(e);
    unsigned char* const x = Extra("EffectKind01_Draw", static_cast<std::uint32_t>(rv::As<records::EffectObject>(e)->work_18));
    Sprite_Current = x;
    eo->x = L(S() + 0x34);
    eo->z = L(S() + 0x38);
    eo->height = L(S() + 0x3C);
    AreaRect(0xF0, 0x10, 0x10);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    long out[3];
    SH_CALL(MoveCmd_AttachOffset)(out, e[0x1C]);
    alignas(4) short v[4];
    AttachVector(S(), out, v);
    long depth[2];
    const long z = SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 8), depth);
    eo->depth = static_cast<U>(z);
    SH_CALL(Gte_PrimDepthFlat4_10)(p);
    SetL(p + 0x1C, 0);
    FldAdd(p + 8, At(at::kQuadEight), p + 0x18);
    FldAdd(p + 8, At(at::kQuadEight), p + 0x38);
    FldSub(p + 8, At(at::kQuadEight), p + 0x28);
    FldSub(p + 8, At(at::kQuadEight), p + 8);
    Fld(p + 0xC, p + 0x3C);
    Fld(p + 0xC, p + 0x2C);
    SetL(p + 0xC, 0);
    Sprite_Current = e;
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, SH_CALL(Gpu_GetClut)(0xB0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, SH_CALL(Gpu_GetTPage)(0, 0, 0x2C0, 0x100));
    p[0x14] = 0;
    p[0x15] = static_cast<unsigned char>((Frame_Counter >> 2) & 0x15);
    p[0x24] = 0x10;
    p[0x34] = 0;
    p[0x25] = static_cast<unsigned char>((Frame_Counter >> 2) & 0x15);
    p[0x35] = static_cast<unsigned char>(FildAddFtol((Frame_Counter >> 2) & 0x15, p + 0x2C));
    p[0x44] = 0x10;
    p[0x45] = static_cast<unsigned char>(FildAddFtol((Frame_Counter >> 2) & 0x15, p + 0x2C));
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    SH_CALL(Gpu_SetShadeTex)(p, 1);
    SH_CALL(MapView_LinkPrimAt)(L(S() + 0x34), L(S() + 0x38), 0x11, 0x48);
    AreaRect(0, 0x100, 0x100);
}

// original 0x462E50 (Effect_KindHandlers 7): EffectKind07_States by +1.
extern "C" void __cdecl EffectKind07_Run(void) { Run("EffectKind07_Run", ::bof3::Img(EffectKind07_States_addr), 5, S()[1]); }

// original 0x462E70 (EffectKind07_States[0]): +0x5F, +0x5E, +0x5D = 0, +9 =
// 0xF0, +1 = 1.
extern "C" void __cdecl EffectKind07_Start(void) {
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[9] = 0xF0;
    S()[1] = 1;
}

// original 0x462EB0 (EffectKind07_States[1]): EffectKind07_DrawSprite(1) (a sprite
// primitive); +0x5D, +0x5E, +0x5F up by 2; at +0x5D 0x80 the counter byte
// 0x903848 = 0x14, +1 = 2, +9 = 0xFF.
extern "C" void __cdecl EffectKind07_FadeIn(void) {
    SH_AT(void (__cdecl*)(U), at::kKind07Sprite.v)(1);
    S()[0x5D] = static_cast<unsigned char>(S()[0x5D] + 2);
    S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 2);
    S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 2);
    unsigned char* const s = S();
    if (s[0x5D] != 0x80) return;
    At(::bof3::Img(at::kCounterByte.v))[0] = 0x14;
    s[1] = 2;
    S()[9] = 0xFF;
}

// original 0x462F10 (cdecl (semi); PSX 0x801F7134): a draw mode (0, 0, page 0xDD, 0) committed
// (+0x29, 0xC); a SPRT at (32.0, 32.0), u 0, v 0x20, CLUT word 0x7A80, w 0x100,
// h 0x6E8, the colour +0x5D..+0x5F, Gpu_SetShadeTex 0, semi-transparency the
// argument's byte; committed (+0x29, 0x1C). PSX twin 0x801F7134 (a SCENA
// overlay, analysis/pairs_propagated.json; the sibling names none).
extern "C" void __cdecl EffectKind07_DrawSprite(int semi) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xDD, 0);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(prim);
    prim[0x14] = 0;
    SetUL(prim + 8, 0x42000000u);
    SetUL(prim + 0xC, 0x42000000u);
    prim[0x15] = 0x20;
    rv::As<records::GpuSprt>(prim)->w = 0x100;
    rv::As<records::GpuSprt>(prim)->h = 0x6E8;
    rv::As<records::GpuSprt>(prim)->clut = 0x7A80;
    const unsigned char* const s = S();
    prim[4] = s[0x5D];
    prim[5] = s[0x5E];
    prim[6] = s[0x5F];
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    SH_CALL(Gpu_SetSemiTrans)(prim, static_cast<unsigned>(semi) & 0xFF);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0x1C);
}

// original 0x462FC0 (EffectKind07_States[2] and [3]): the sprite drawn
// semi-transparent; +9 down one while not 0, else +1 up and +9 0xFF.
extern "C" void __cdecl EffectKind07_Hold(void) {
    SH_CALL(EffectKind07_DrawSprite)(1);
    unsigned char* const s = S();
    if (s[9] != 0) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    s[1] = static_cast<unsigned char>(s[1] + 1);
    s[9] = 0xFF;
}

// original 0x462FF0 (EffectKind07_States[4]): the sprite drawn; +0x5D..+0x5F down
// two; at +0x5D 0, +9 0 and Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind07_FadeOut(void) {
    SH_CALL(EffectKind07_DrawSprite)(1);
    unsigned char* const s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xFE);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xFE);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xFE);
    if (s[0x5D] != 0) return;
    s[9] = 0;
    SH_CALL(Effect_Release)();
}

// original 0x463040 (Effect_KindHandlers 8): EffectKind08_States by +1.
extern "C" void __cdecl EffectKind08_Run(void) { Run("EffectKind08_Run", ::bof3::Img(EffectKind08_States_addr), 4, S()[1]); }

// original 0x463060 (EffectKind08_States[0]): the point (+0x34 0xB0000, +0x38
// 0xF8000), the word +0x3E the ground's AreaMap_Elevation there, the slot +0x29
// 5, +1 = 1.
extern "C" void __cdecl EffectKind08_Start(void) {
    unsigned char* s = S();
    rv::As<records::EffectObject>(s)->x = 0xB0000;
    rv::As<records::EffectObject>(s)->z = 0xF8000;
    const long e = SH_CALL(AreaMap_Elevation)(SLRest4f(s + 0x34), SLRest4f(s + 0x38));
    s = S();
    rv::As<records::EffectObject>(s)->height_whole = static_cast<U>(e);
    s[0x29] = 5;
    s[1] = 1;
}

// original 0x4630B0 (EffectKind08_States[1]): a grey (0xC0) line along the ground
// from the record's point (+0x34, +0x38, +0x3C) to the z of the Sprite_Objects
// record +6 names, in steps of 0x8000 (the last step the object's z and height);
// +1 = 2 when the chapter's byte 0x90384A is 0x32.
extern "C" void __cdecl EffectKind08_TraceLine(void) {
    const unsigned char* const s = S();
    const unsigned char* const o = TraceObject("EffectKind08_TraceLine");
    Trace t{static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height)};
    RunRest4f(t, SLRest4f(o + 0x38), static_cast<std::uint32_t>(rv::As<records::SpriteObject>(o)->height), +1, false, 0xC0C0C0);
    if (BRest4f(::bof3::Img(at::kChapterByte4A.v)) == 0x32) S()[1] = 2;
}

// original 0x4631B0 (EffectKind08_States[2]): the grey line as state 1, then a
// dark (0x30) one from (0xB0000, 0x138000) - its height the ground's there -
// down z to the object's; +1 = 3 when 0x90384A is 0x34. The frame's height cell
// is not set before the first step (stale stack in the original; its low word is
// never read - ours 0).
extern "C" void __cdecl EffectKind08_TraceTwo(void) {
    const unsigned char* const s = S();
    const unsigned char* const o = TraceObject("EffectKind08_TraceTwo");
    const std::int32_t target = SLRest4f(o + 0x38);
    const U oh = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(o)->height);
    Trace t{static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height), 0};
    RunRest4f(t, target, oh, +1, false, 0xC0C0C0);
    t.x = 0xB0000;
    t.z = 0x138000;
    const long e = SH_CALL(AreaMap_Elevation)(0xB0000, 0x138000);
    t.h = (t.h & 0xFFFFu) | (static_cast<U>(e) << 16);
    RunRest4f(t, target, oh, -1, true, 0x303030);
    if (BRest4f(::bof3::Img(at::kChapterByte4A.v)) == 0x34) S()[1] = 3;
}

// original 0x463350 (cdecl, seven words by value: a point A (x, z, a height
// dword whose high word is read), a point B the same, a colour's three bytes): a
// LINE_F2 at Gfx_PacketNext - B then A turned into the GTE's vector (x and z as
// (v >> 9) - 0x4000, the height -(s16 high word) / 2) and projected
// (Gte_RotTransPers, Gte_StoreDepthF) to +8 / +0x10 and +0x14 / +0x1C; the colour
// to +4..+6; committed (+0x29, 0x20). The vector's fourth word is the original's
// stale stack, ours 0 (DIV-0023's ruling, as effect_1a.cpp's AttachVector).
extern "C" void __cdecl EffectKind08_DrawLine(int ax, int az, int ah, int bx, int bz, int bh, int colour) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    const auto vector = [](short* v, int x, int z, int h) {
        v[0] = static_cast<short>(Sar(static_cast<U>(x), 9) - 0x4000u);
        v[1] = static_cast<short>(Sar(static_cast<U>(z), 9) - 0x4000u);
        v[2] = static_cast<short>(-(static_cast<std::int32_t>(static_cast<std::int16_t>(static_cast<U>(h) >> 16)) / 2));
        v[3] = 0;
    };
    short v[4];
    long p;
    vector(v, bx, bz, bh);
    SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(prim + 8), &p);
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
    vector(v, ax, az, ah);
    SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(prim + 0x14), &p);
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x1C));
    const U c = static_cast<U>(colour);
    prim[4] = static_cast<unsigned char>(c);
    prim[5] = static_cast<unsigned char>(c >> 8);
    prim[6] = static_cast<unsigned char>(c >> 16);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0x20);
}

// original 0x463440 (Effect_KindHandlers 9): EffectKind09_States by +1.
extern "C" void __cdecl EffectKind09_Run(void) { Run("EffectKind09_Run", ::bof3::Img(EffectKind09_States_addr), 10, S()[1]); }

// original 0x463460 (EffectKind09_States[0]): the point (0x638000, 0xD0000, the
// height word 0x300); +0x64..+0x6C 0; +9 0x78; the colour 0xFF; +0xC 1; +0x10,
// +0x20, +0x1C, +0x18 0; +0xA, +0xB, +8, +7, +6 0; sound 0x20E; +1 = 1.
extern "C" void __cdecl EffectKind09_Start(void) {
    unsigned char* s = S();
    rv::As<records::EffectObject>(s)->x = 0x638000;
    rv::As<records::EffectObject>(s)->z = 0xD0000;
    rv::As<records::EffectObject>(s)->height_whole = 0x300;
    rv::As<records::EffectObject>(s)->angle[0] = 0;
    rv::As<records::EffectObject>(s)->angle[1] = 0;
    rv::As<records::EffectObject>(s)->angle[2] = 0;
    s[9] = 0x78;
    s[0x5F] = s[0x5E] = s[0x5D] = 0xFF;
    rv::As<records::EffectWork>(s)->work_0c = 1;
    rv::As<records::EffectWork>(s)->work_10 = 0;
    rv::As<records::EffectObject>(s)->work_20 = 0;
    rv::As<records::EffectObject>(s)->work_1c = 0;
    rv::As<records::EffectObject>(s)->work_18 = 0;
    s[0xA] = s[0xB] = s[8] = s[7] = s[6] = 0;
    Sound(0x20E);
    S()[1] = 1;
}

// original 0x463540 (EffectKind09_States[1]): fan 1; its length +0x18 up 8; at
// 0x60 sound 0x20E and +1 = 2.
extern "C" void __cdecl EffectKind09_Open1(void) {
    Fans(1);
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->work_18 = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + 8;
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18) != 0x60) return;
    Sound(0x20E);
    S()[1] = 2;
}

// original 0x463590 (EffectKind09_States[2]): fans 1, 2; +0x1C up 0x10; sound
// 0x20E at 0x20; at 0xA0 the sound and +1 = 3.
extern "C" void __cdecl EffectKind09_Open2(void) {
    Fans(2);
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->work_1c = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x10;
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c) == 0x20) Sound(0x20E);
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c) != 0xA0) return;
    Sound(0x20E);
    S()[1] = 3;
}

// original 0x463610 (EffectKind09_States[3]): fans 1..3; +0xB up 0x10; at 0x60
// sound 0x20E and +1 = 4.
extern "C" void __cdecl EffectKind09_Open3(void) {
    Fans(3);
    GrowByte(0xB, 0x10, 0, 4);
}

// original 0x463690 (EffectKind09_States[4]): fans 1..4; +8 up 8; at 0x60 the
// sound and +1 = 5.
extern "C" void __cdecl EffectKind09_Open4(void) {
    Fans(4);
    GrowByte(8, 8, 0, 5);
}

// original 0x463730 (EffectKind09_States[5]): fans 1..5; +0xA up 8; sound at
// 0x10; at 0x60 the sound and +1 = 6.
extern "C" void __cdecl EffectKind09_Open5(void) {
    Fans(5);
    GrowByte(0xA, 8, 0x10, 6);
}

// original 0x463810 (EffectKind09_States[6]): fans 1..6; +6 up 8; sound at
// 0x10; at 0x60 the sound and +1 = 7.
extern "C" void __cdecl EffectKind09_Open6(void) {
    Fans(6);
    GrowByte(6, 8, 0x10, 7);
}

// original 0x463910 (EffectKind09_States[7]): fans 1..7; +7 up 8; at 0x60 the
// sound and +1 = 8.
extern "C" void __cdecl EffectKind09_Open7(void) {
    Fans(7);
    GrowByte(7, 8, 0, 8);
}

// original 0x463A10 (EffectKind09_States[8]): fans 1..8; +0x20 up 8; sound 0x20E
// at 0x10; at 0x60 sound 0x203, the chapter's count 0x903848 up one, +1 = 9.
extern "C" void __cdecl EffectKind09_Open8(void) {
    Fans(8);
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->work_20 = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20) + 8;
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20) == 0x10) Sound(0x20E);
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20) != 0x60) return;
    Sound(0x203);
    BRest4f(::bof3::Img(at::kChapterCount.v)) = static_cast<unsigned char>(BRest4f(::bof3::Img(at::kChapterCount.v)) + 1);
    S()[1] = 9;
}

// original 0x463B40 (EffectKind09_States[9]): at +9 0, Effect_Release (a tail
// jmp); else the blades drawn, +0xC up 4 while below 0x80, +0x10 up 2 to 0xB6
// (s32 compares), +9 down one.
extern "C" void __cdecl EffectKind09_Spin(void) {
    if (S()[9] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(EffectKind09_DrawBlades)();
    unsigned char* const s = S();
    const std::int32_t c = SLRest4f(s + 0xC);
    if (c < 0x80) rv::As<records::EffectWork>(s)->work_0c = static_cast<U>(c) + 4;
    const std::int32_t d = SLRest4f(s + 0x10);
    if (d < 0xB6) rv::As<records::EffectWork>(s)->work_10 = static_cast<U>(d) + 2;
    else rv::As<records::EffectWork>(s)->work_10 = 0xB6;
    s[9] = static_cast<unsigned char>(s[9] - 1);
}

// original 0x463BA0 (void, Sprite_Current): twenty-four blades. +0x6C set to
// (+9 << 12) / 360 (the original's multiply by 0xB60B60B7); two triangles of
// the length L = +0xC: (0, 0, 0) - (L << 4, 0, 0) - (L cos(+0x10) >> 8, L
// sin(+0x10) >> 8, 0) on odd blades, the same with the angle 0xAC on even ones;
// each blade's colour +0x5D..+0x5F at its root and -(2 * +9) at the other two
// corners; +0x6C up 0xAA after each.
extern "C" void __cdecl EffectKind09_DrawBlades(void) {
    unsigned char* const s0 = S();
    const U n = static_cast<U>(s0[9]) << 12;
    const U len = rv::As<records::EffectWork>(s0)->work_0c;
    const U angle = rv::As<records::EffectWork>(s0)->work_10;
    const U hi = static_cast<U>((static_cast<std::int64_t>(static_cast<std::int32_t>(n)) *
                                 static_cast<std::int64_t>(static_cast<std::int32_t>(0xB60B60B7u))) >> 32);
    U q = Sar(hi + n, 8);
    q += q >> 31;
    rv::As<records::EffectObject>(s0)->angle[2] = q;
    const short c1 = Arm(SH_CALL(Math_Cos), static_cast<int>(angle), len);
    const short s1 = Arm(SH_CALL(Math_Sin), static_cast<int>(angle), len);
    const short reach = static_cast<short>(len << 4);
    const short odd0[4] = {0, 0, 0, 0}, odd1[4] = {reach, 0, 0, 0}, odd2[4] = {c1, s1, 0, 0};
    const short c2 = Arm(SH_CALL(Math_Cos), 0xAC, len);
    const short s2 = Arm(SH_CALL(Math_Sin), 0xAC, len);
    const short even0[4] = {0, 0, 0, 0}, even1[4] = {reach, 0, 0, 0}, even2[4] = {c2, s2, 0, 0};
    for (unsigned blade = 0; blade < 0x18; ++blade) {
        const auto colours = [](unsigned char* prim) {
            const unsigned char* const s = S();
            prim[4] = s[0x5D];
            prim[5] = s[0x5E];
            prim[6] = s[0x5F];
            const auto tip = static_cast<unsigned char>(-(s[9] << 1));
            prim[0x14] = prim[0x15] = prim[0x16] = tip;
            prim[0x24] = prim[0x25] = prim[0x26] = tip;
        };
        if (blade & 1) Triangle(odd0, odd1, odd2, colours);
        else Triangle(even0, even1, even2, colours);
        unsigned char* const s = S();
        rv::As<records::EffectObject>(s)->angle[2] = rv::As<records::EffectObject>(s)->angle[2] + 0xAA;
    }
}

// original 0x463D80 (cdecl (len, a1, a2, a3)): one fan. The record's +0x64..+0x70
// kept; +0x64, +0x68, +0x6C set to a1, a2, a3 (the object matrix's angles); a
// triangle (0, 0, 0) - (len << 4, 0, 0) - (len cos(0x4F) >> 8, len sin(0x4F) >> 8,
// 0), its colours 0x28 / 0x6E / 0x64; the kept four dwords put back into the
// record current afterwards.
extern "C" void __cdecl EffectKind09_DrawFan(int len, int a1, int a2, int a3) {
    unsigned char* const s0 = S();
    const U k64 = rv::As<records::EffectObject>(s0)->angle[0], k68 = rv::As<records::EffectObject>(s0)->angle[1], k6C = rv::As<records::EffectObject>(s0)->angle[2], k70 = rv::As<records::EffectObject>(s0)->reach;
    rv::As<records::EffectObject>(s0)->angle[0] = static_cast<U>(a1);
    rv::As<records::EffectObject>(s0)->angle[1] = static_cast<U>(a2);
    rv::As<records::EffectObject>(s0)->angle[2] = static_cast<U>(a3);
    const short c = Arm(SH_CALL(Math_Cos), 0x4F, static_cast<U>(len));
    const short sn = Arm(SH_CALL(Math_Sin), 0x4F, static_cast<U>(len));
    const short v0[4] = {0, 0, 0, 0};
    const short v1[4] = {static_cast<short>(static_cast<U>(len) << 4), 0, 0, 0};
    const short v2[4] = {c, sn, 0, 0};
    Triangle(v0, v1, v2, [](unsigned char* prim) {
        prim[4] = prim[5] = prim[6] = 0x28;
        prim[0x14] = prim[0x15] = prim[0x16] = 0x6E;
        prim[0x24] = prim[0x25] = prim[0x26] = 0x64;
    });
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->angle[0] = k64;
    rv::As<records::EffectObject>(s)->angle[1] = k68;
    rv::As<records::EffectObject>(s)->angle[2] = k6C;
    rv::As<records::EffectObject>(s)->reach = k70;
}

// original 0x463EE0 (Effect_KindHandlers 0xB): EffectKind0B_States by +1.
extern "C" void __cdecl EffectKind0B_Run(void) { Run("EffectKind0B_Run", ::bof3::Img(EffectKind0B_States_addr), 5, S()[1]); }

// original 0x463F00 (EffectKind0B_States[0]): the screen point +0x74 / +0x78
// (floats) the first extra sprite's words +0x2E / +0x30 plus the two s8 of
// 0x653AAC; +0x7C 0.01f; +9 0; +6 0x20; the colour 0x80; the quad drawn; +1 = +7.
extern "C" void __cdecl EffectKind0B_Start(void) {
    unsigned char* s = S();
    StF(s + 0x74, I(static_cast<U>(Rest4f_Kind0BOffset +
                                   Sprite_ObjectsExtra[0].screen_x)));
    StF(s + 0x78, I(static_cast<U>(static_cast<signed char>(BRest4f(::bof3::Img(at::kKind0BOffset.v) + 1)) +
                                   Sprite_ObjectsExtra[0].screen_y)));
    rv::As<records::EffectObject>(s)->depth_f = 0x3C23D70Au;
    s[9] = 0;
    s[6] = 0x20;
    s[0x5F] = s[0x5E] = s[0x5D] = 0x80;
    SH_CALL(EffectKind0B_DrawQuad)();
    s = S();
    s[1] = s[7];
}

// original 0x463FA0 (EffectKind0B_States[1]): x -= 4.0; y += the s8 of
// 0x653AB0 by +9 >> 2 (read in place); at +0x5D 0 Effect_Release; else the
// colour down 2, +9 up one, +6 up one each fourth, the quad drawn.
extern "C" void __cdecl EffectKind0B_Rise(void) {
    unsigned char* const s = S();
    StF(s + 0x74, F(s + 0x74) - F(At(at::kKind0BStepX)));
    const auto rise = static_cast<U>(static_cast<signed char>(BRest4f(::bof3::Img(at::kKind0BRise.v) + (s[9] >> 2))));
    StF(s + 0x78, I(rise) + F(s + 0x78));
    if (s[0x5D] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xFE);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xFE);
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if ((s[9] & 3) == 0) s[6] = static_cast<unsigned char>(s[6] + 1);
    SH_CALL(EffectKind0B_DrawQuad)();
}

// original 0x464030 (EffectKind0B_States[2]): y -= 2.0; at +0x5D 0 Effect_Release
// (a tail jmp); else the colour down 4, +6 up one when +9 & 7 is 0, the quad (a
// tail jmp).
extern "C" void __cdecl EffectKind0B_Drop(void) {
    unsigned char* const s = S();
    StF(s + 0x78, F(s + 0x78) - F(At(at::kKind0BStepY)));
    if (!Fade0B(4, 7)) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(EffectKind0B_DrawQuad)();
}

// original 0x464090 (EffectKind0B_States[3]): the point +0x34..+0x3C and the
// angles +0x64..+0x6C the first extra sprite's; plus MoveCmd_AttachOffset of the
// byte +0x1C (x, z, and the height word +0x3E); +1 up.
extern "C" void __cdecl EffectKind0B_Attach(void) {
    unsigned char* s = S();
    rv::As<records::EffectObject>(s)->x = static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].x);
    rv::As<records::EffectObject>(s)->z = static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].z);
    rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].height);
    rv::As<records::EffectObject>(s)->angle[0] = Sprite_ObjectsExtra[0].angle[0];
    rv::As<records::EffectObject>(s)->angle[1] = Sprite_ObjectsExtra[0].angle[1];
    rv::As<records::EffectObject>(s)->angle[2] = Sprite_ObjectsExtra[0].angle[2];
    long out[3];
    SH_CALL(MoveCmd_AttachOffset)(out, s[0x1C]);
    s = S();
    rv::As<records::EffectObject>(s)->x = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + static_cast<U>(out[0]);
    rv::As<records::EffectObject>(s)->z = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) + static_cast<U>(out[1]);
    rv::As<records::EffectObject>(s)->height_whole = rv::As<records::EffectObject>(s)->height_whole + static_cast<U>(out[2]);
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x464140 (EffectKind0B_States[4]; PSX 0x801F8AB8): the height word +0x3E up 0x20; the
// point (+0x34, +0x38, +0x3C) projected into +0x74 (EffectGte_ProjectPoint); at
// +0x5D 0 Effect_Release; else the colour down 4, +6 up one when +9 & 7 is 0, the
// quad drawn. PSX twin 0x801F8AB8 (a SCENA overlay).
extern "C" void __cdecl EffectKind0B_Follow(void) {
    unsigned char* s = S();
    rv::As<records::EffectObject>(s)->height_whole = rv::As<records::EffectObject>(s)->height_whole + 0x20;
    const long point[3] = {SLRest4f(s + 0x34), SLRest4f(s + 0x38), SLRest4f(s + 0x3C)};
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(s + 0x74));
    if (!Fade0B(4, 7)) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(EffectKind0B_DrawQuad)();
}

// original 0x4641D0 (void, Sprite_Current; PSX 0x801F8BCC): a POLY_FT4 square of side +6 centred
// on +0x74 / +0x78 less h = +6 >> 1 (x87: (x - h), (x - h) + side), the colour
// +0x5D..+0x5F, the depth +0x7C on each corner (copied through the FPU), u / v
// (0xE0, 0x30)-(0xFF, 0x50), Gpu_GetClut(0xA0, 0x1E3), Gpu_GetTPage(0, 1, 0x2C0,
// 0x100), no shading, semi-transparent; committed (3, 0x48) only while
// Draw_PassFlags is set. PSX twin 0x801F8BCC (a SCENA overlay).
extern "C" void __cdecl EffectKind0B_DrawQuad(void) {
    unsigned char* const prim = Gfx_PacketNext;
    const unsigned half = static_cast<unsigned char>(S()[6] >> 1);
    SH_CALL(Gpu_SetPolyFT4)(prim);
    const unsigned char* const s = S();
    const LD h = I(half);
    const LD side = I(s[6]);
    const LD x = F(s + 0x74) - h, y = F(s + 0x78) - h;
    StF(prim + 8, x);
    StF(prim + 0xC, y);
    StF(prim + 0x18, F(s + 0x74) - h + side);
    StF(prim + 0x1C, F(s + 0x78) - h);
    StF(prim + 0x28, F(s + 0x74) - h);
    StF(prim + 0x2C, F(s + 0x78) - h + side);
    StF(prim + 0x38, F(s + 0x74) - h + side);
    prim[0x15] = prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x50;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x50;
    StF(prim + 0x3C, F(s + 0x78) - h + side);
    prim[4] = s[0x5D];
    prim[5] = s[0x5E];
    prim[6] = s[0x5F];
    const LD depth = F(s + 0x7C);
    StF(prim + 0x40, depth);
    StF(prim + 0x30, depth);
    StF(prim + 0x20, depth);
    StF(prim + 0x10, depth);
    const unsigned clut = SH_CALL(Gpu_GetClut)(0xA0, 0x1E3);
    rv::As<records::GpuPolyFT4>(prim)->clut = clut;
    const unsigned page = SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100);
    rv::As<records::GpuPolyFT4>(prim)->tpage = page;
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    if (Draw_PassFlags != 0) SH_CALL(Gfx_CommitPrim)(3, 0x48);
}

// original 0x464350 (Effect_KindHandlers 0x10; the entry a jmp over eleven
// nops to 0x464360): EffectKind01_Draw's quad between two extra records: the
// point of the one +0xC names (Sprite_Current while its half is built, copied
// into the effect's +0x34..+0x3F) plus MoveCmd_AttachOffset(+0x10) gives the
// top pair (x - 8, x + 8 at its y; the depth Gte_StoreDepthF to +0x10, +0x20
// its copy), the one +0x18 names plus MoveCmd_AttachOffset(+0x1C) the bottom
// pair (+0x28.. likewise, the depth to +0x30 / +0x40); the effect's +0x60 the
// second projection's answer. v on the bottom row is the top's plus |the
// bottom's y - the top's| through _ftol. The rest as kind 1's.
extern "C" void __cdecl EffectKind10_Run(void) {
    unsigned char* const e = S();
    auto* const eo = rv::As<records::EffectObject>(e);
    Sprite_Current = Extra("EffectKind10_Run", rv::As<records::EffectWork>(e)->work_0c);
    eo->x = L(S() + 0x34);
    eo->z = L(S() + 0x38);
    eo->height = L(S() + 0x3C);
    AreaRect(0xF0, 0x10, 0x10);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    long out[3];
    SH_CALL(MoveCmd_AttachOffset)(out, e[0x10]);
    alignas(4) short v[4];
    AttachVector(S(), out, v);
    long depth[2];
    eo->depth = static_cast<U>(SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 8), depth));
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
    CopyDword(p + 0x20, p + 0x10);
    CopyDword(p + 0x1C, p + 0xC);
    FldAdd(p + 8, At(at::kQuadEight), p + 0x18);
    FldSub(p + 8, At(at::kQuadEight), p + 8);
    Sprite_Current = Extra("EffectKind10_Run", static_cast<std::uint32_t>(rv::As<records::EffectObject>(e)->work_18));
    SH_CALL(MoveCmd_AttachOffset)(out, e[0x1C]);
    AttachVector(S(), out, v);
    eo->depth = static_cast<U>(SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 0x28), depth));
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x30));
    CopyDword(p + 0x40, p + 0x30);
    CopyDword(p + 0x3C, p + 0x2C);
    FldAdd(p + 0x28, At(at::kQuadEight), p + 0x38);
    FldSub(p + 0x28, At(at::kQuadEight), p + 0x28);
    Sprite_Current = e;
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, SH_CALL(Gpu_GetClut)(0xB0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, SH_CALL(Gpu_GetTPage)(0, 0, 0x2C0, 0x100));
    p[0x14] = 0;
    const U v0 = (Frame_Counter >> 2) & 0x15;
    p[0x15] = static_cast<unsigned char>(v0);
    p[0x24] = 0x10;
    p[0x34] = 0;
    p[0x25] = static_cast<unsigned char>((Frame_Counter >> 2) & 0x15);
    U span = FldSubFtol(p + 0x2C, p + 0xC);
    if (static_cast<std::int32_t>(span) < 0) span = 0u - span;
    p[0x44] = 0x10;
    p[0x35] = static_cast<unsigned char>(((Frame_Counter >> 2) & 0x15) + span);
    p[0x45] = static_cast<unsigned char>(((Frame_Counter >> 2) & 0x15) + span);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    SH_CALL(Gpu_SetShadeTex)(p, 1);
    SH_CALL(MapView_LinkPrimAt)(L(S() + 0x34), L(S() + 0x38), 0x11, 0x48);
    AreaRect(0, 0x100, 0x100);
}

void EffectKind09_InjectFromEffect1a() {
    BOF3_INJECT(EffectKind01_Run);
    BOF3_INJECT(EffectKind01_Start);
    BOF3_INJECT(EffectKind01_Draw);
    BOF3_INJECT(EffectKind07_Run);
    BOF3_INJECT(EffectKind07_Start);
    BOF3_INJECT(EffectKind07_FadeIn);
    BOF3_INJECT(EffectKind08_Run);
    BOF3_INJECT(EffectKind09_Run);
    BOF3_INJECT(EffectKind0B_Run);
    BOF3_INJECT(EffectKind10_Run);
}

void EffectKind09_InjectFromRest4f() {
    BOF3_INJECT(EffectKind07_DrawSprite);
    BOF3_INJECT(EffectKind07_Hold);
    BOF3_INJECT(EffectKind07_FadeOut);
    BOF3_INJECT(EffectKind08_Start);
    BOF3_INJECT(EffectKind08_TraceLine);
    BOF3_INJECT(EffectKind08_TraceTwo);
    BOF3_INJECT(EffectKind08_DrawLine);
    BOF3_INJECT(EffectKind09_Start);
    BOF3_INJECT(EffectKind09_Open1);
    BOF3_INJECT(EffectKind09_Open2);
    BOF3_INJECT(EffectKind09_Open3);
    BOF3_INJECT(EffectKind09_Open4);
    BOF3_INJECT(EffectKind09_Open5);
    BOF3_INJECT(EffectKind09_Open6);
    BOF3_INJECT(EffectKind09_Open7);
    BOF3_INJECT(EffectKind09_Open8);
    BOF3_INJECT(EffectKind09_Spin);
    BOF3_INJECT(EffectKind09_DrawBlades);
    BOF3_INJECT(EffectKind09_DrawFan);
    BOF3_INJECT(EffectKind0B_Start);
    BOF3_INJECT(EffectKind0B_Rise);
    BOF3_INJECT(EffectKind0B_Drop);
    BOF3_INJECT(EffectKind0B_Attach);
    BOF3_INJECT(EffectKind0B_Follow);
    BOF3_INJECT(EffectKind0B_DrawQuad);
}

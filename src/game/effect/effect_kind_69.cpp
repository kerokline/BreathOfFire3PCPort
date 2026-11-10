// Moved here by tools/move_funcs.py from effect_3b.cpp, rest_3f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_69.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_69_callees.h"
#include "game/effect_3b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3f.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_69::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetUL(U a, U v) { SetUL(At(a), v); }

U W(U a) { return Word(At(a)); }

U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v & 0xFFFFu))); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul (32 bits kept)
U Mul(int a, U b) { return static_cast<U>(a) * b; }

void AddWord(U a, U d) { SetWord(At(a), W(a) + d); }

// --- x87 as the original has it (the game's control word: round to nearest, 53
// bits); every value goes through the FPU as Capcom's does.
// `fild dword [v]; fstp dword [o]`
void Fild(U v, void* o) {
    const std::int32_t i = static_cast<std::int32_t>(v);
    __asm__ volatile("fildl %1\n\tfstps (%0)" : : "r"(o), "m"(i) : "st", "memory");
}

// `fld dword [a]; call _ftol` (the CRT's 0x5B9550: truncation toward zero in a
// copy of the control word, one fistp to 64 bits, the word put back; NaN and
// out-of-range the integer indefinite, whose low dword is 0) - the low dword.
U FldFtol(const void* a) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// `fld dword [c]; fcomp dword [y]; fnstsw ax; test ah, 1`: C0 - c below y, or
// unordered.
bool BelowOrUnordered(const void* c, const void* y) {
    float a, b;
    std::memcpy(&a, c, 4);
    std::memcpy(&b, y, 4);
    return !(static_cast<long double>(a) >= static_cast<long double>(b));
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + byte]; jmp / call
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original goes
// through the dword after - the next table.
void Through(const char* who, U table, unsigned entries, unsigned index, unsigned byte) {
    if (index >= entries)
        bof3::Fatal("%s: +%u is %u, past the %u entries of 0x%X - the original goes through the dword after "
                    "(docs/effect_3b.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * index)))();
}

void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    Through(who, table, entries, Sprite_Current[byte], byte);
}

// Effect_FindFree's record (the answer's low byte, as the originals widen it):
// a Fatal past the twenty - the original writes record (answer * 0x80) past the
// pool, 0xFF included where it does not test for none.
unsigned char* Spawned(const char* who, U answer) {
    const unsigned k = answer & 0xFF;
    if (k >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes record %u past "
                    "the pool (docs/effect_3b.md section 7)",
                    who, k, k);
    return rv::Bytes(&Effect_Objects[k]);
}

unsigned char* Parent() { return EffectKind69_Parent; }

void Kind69Lines() { SH_AT(void (__cdecl*)(), at::kKind69Lines.v)(); }

// Sprite_Current's (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)), the
// SVECTOR the matrix and the screen point are built from (the pad word is not
// written by the original either).
void PointVector(const unsigned char* s, short* v) {
    v[0] = static_cast<short>(Sar(UL(s + 0x34), 9) - 0x4000u);
    v[1] = static_cast<short>(Sar(UL(s + 0x38), 9) - 0x4000u);
    v[2] = static_cast<short>(-(static_cast<std::int16_t>(Word(s + 0x3E)) / 2));
}

// The orbit kind 0x69's parts 1 and 2 share: SharedScratch = radius, the angle
// (+0xC & 0x7F) << 5 to 0x903854; x = cos * the scratch (re-read) + the parent's
// x, z = sin(0x903854, re-read) * the scratch + the parent's z - no shift.
void Orbit(U radius) {
    at::kScale.d_00 = radius;
    const U angle = (rv::As<records::EffectWork>(S())->work_0c & 0x7F) << 5;
    at::kAngle = angle;
    const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
    rv::Store<U>(rv::Current<records::EffectObject>()->x, Mul(c, static_cast<std::uint32_t>(at::kScale.d_00)) + UL(Parent() + 0x34));
    const int n = SH_CALL(Math_Sin)(at::kAngle);
    rv::Store<U>(rv::Current<records::EffectObject>()->z, Mul(n, static_cast<std::uint32_t>(at::kScale.d_00)) + UL(Parent() + 0x38));
}

// A part's place: +0xB = Rand() & 0xF, +9 = +0xA = 0, +0xC = (+4 << 5) + start,
// the orbit at radius, the parent's height, +3 up.
void PlaceOnOrbit(U start, U radius) {
    const int r = SH_CALL(Rand)();
    unsigned char* const s = S();
    s[0xB] = static_cast<unsigned char>(r & 0xF);
    s[9] = 0;
    s[0xA] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, (static_cast<U>(s[4]) << 5) + start);
    Orbit(radius);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, Word(Parent() + 0x3E));
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// +0xA up by 2 below `top`; at it +9 = 60 and +3 up.
void Rise(unsigned top) {
    unsigned char* const s = S();
    if (s[0xA] < top) {
        s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
        return;
    }
    s[9] = 0x3C;
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// The part's end: +0xA down by `step`; at 0 the parent's +0xB up and a tail
// jump to Effect_Release.
void Fade(unsigned step) {
    unsigned char* const s = S();
    s[0xA] = static_cast<unsigned char>(s[0xA] - step);
    if (S()[0xA] != 0) return;
    Parent()[0xB] = static_cast<unsigned char>(Parent()[0xB] + 1);
    SH_CALL(Effect_Release)();
}

// A part's frame: its step by +3 through `steps` (a call), its screen point; with
// +3 past 0 (read again) the matrix at its point pushed, `draw`, popped.
void PartFrame(const char* who, U steps, unsigned count, void (*draw)()) {
    Through(who, steps, count, S()[3], 3);
    SH_CALL(EffectKind69_UpdateScreenXY)();
    if (S()[3] == 0) return;
    SH_CALL(EffectKind69_PushPointMatrix)();
    draw();
    SH_CALL(Gte_PopMatrix)();
}

void DrawColumn0() { SH_CALL(EffectKind69_DrawColumn)(0x60, 0x20, 7); }

void DrawColumn1() {
    SH_CALL(EffectKind69_DrawColumn)(0x20, 0x50, 0x1F);
    SH_CALL(EffectKind69_DrawGlow)();
}

void DrawLines2() {
    Kind69Lines();
    SH_CALL(EffectKind69_DrawGlow)();
}

}  // namespace

// original 0x482A80 (Effect_KindHandlers[0x69], hidden in 0x482740): call
// [esp + +1 * 4] of the stack table {EffectKind69_Spawn, _WaitParts,
// _PartRun}, unbounded (past it the original calls its saved registers and
// return address).
extern "C" void __cdecl EffectKind69_Run(void) {
    static const U kStates[] = {bof3::addr::EffectKind69_Spawn, bof3::addr::EffectKind69_WaitParts,
                                bof3::addr::EffectKind69_PartRun};
    const unsigned state = S()[1];
    if (state >= 3)
        bof3::Fatal("EffectKind69_Run: +1 is %u, past its stack table of three - the original calls what lies "
                    "above it on the stack (docs/effect_3b.md section 7)",
                    state);
    scenario_harness::Phase(kStates[state])();
}

// original 0x482AB0 (stack table state 0): Sprite_Current to 0x676268 (the
// parent); nine records from Effect_FindFree (its 0xFF not tested), each +0 = 1,
// kind 0x69, +1 = 2, +3 = 0: one with +2 = 0, four with +2 = 1 and +4 = 0..3,
// four with +2 = 2 and +4 = 0..3; its own +0xB = 0, +1 up;
// Sound_PlayEffect(0x203).
extern "C" void __cdecl EffectKind69_Spawn(void) {
    SetUL(::bof3::Img(at::kParent.v), AddressOf(Sprite_Current));
    unsigned char* e = Spawned("EffectKind69_Spawn", SH_CALL(Effect_FindFree)());
    e[0] = 1;
    e[5] = 0x69;
    e[1] = 2;
    e[2] = 0;
    e[3] = 0;
    for (unsigned part = 1; part <= 2; ++part)
        for (unsigned i = 0; i < 4; ++i) {
            e = Spawned("EffectKind69_Spawn", SH_CALL(Effect_FindFree)());
            e[0] = 1;
            e[5] = 0x69;
            e[1] = 2;
            e[2] = static_cast<unsigned char>(part);
            e[3] = 0;
            e[4] = static_cast<unsigned char>(i);
        }
    S()[0xB] = 0;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    SH_CALL(Sound_PlayEffect)(0x203);
}

// original 0x482BB0 (stack table state 1): +0xB at 9 (the nine parts counted
// in): a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind69_WaitParts(void) {
    if (S()[0xB] == 9) SH_CALL(Effect_Release)();
}

// original 0x482BD0 (stack table state 2): jmp [EffectKind69_Parts + +2 * 4],
// unbounded.
extern "C" void __cdecl EffectKind69_PartRun(void) {
    Dispatch("EffectKind69_PartRun", AddressOf(EffectKind69_Parts), EffectKind69_Parts_count, 2);
}

// original 0x482BF0 (EffectKind69_Parts[0]): call [EffectKind69_Part0Steps +
// +3 * 4] (unbounded), EffectKind69_UpdateScreenXY; with +3 past 0 (read
// again) the matrix pushed at the point, the column (0x60, 0x20, 7), a tail
// jump to Gte_PopMatrix.
extern "C" void __cdecl EffectKind69_Part0(void) {
    PartFrame("EffectKind69_Part0", AddressOf(EffectKind69_Part0Steps), EffectKind69_Part0Steps_count, &DrawColumn0);
}

// original 0x482C30 (cdecl, no arguments; called by the three parts): the GTE
// matrix pushed; then a MATRIX on the stack: its translation Gte_RotTrans of
// the point's SVECTOR (PointVector), its rotation Gte_RotMatrix of (0, 0, 0x400 -
// 0 when +8 bit 0 is set), multiplied by Camera_Matrix (Gte_MulMatrix0(camera,
// m, m)), set as the rotation and the translation. The caller pops it.
extern "C" void __cdecl EffectKind69_PushPointMatrix(void) {
    SH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = S();
    short angles[4];
    angles[0] = 0;
    angles[1] = 0;
    angles[2] = 0x400;
    if (s[8] & 1) angles[2] = 0;
    short v[4];
    PointVector(s, v);
    alignas(4) unsigned char m[0x20];
    SH_CALL(Gte_RotTrans)(v, reinterpret_cast<long*>(m + 0x14));
    SH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(m));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(m), reinterpret_cast<short*>(m));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(m));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(m));
}

// original 0x482CE0 (part 0 step 0): +0xB = Rand() & 0xF, +9 = +0xA = 0, the
// parent's x and z (0x676268 read again for each), the height
// AreaMap_Elevation(x, z) to the word +0x3E, +3 up.
extern "C" void __cdecl EffectKind69_Part0Place(void) {
    const int r = SH_CALL(Rand)();
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    s[0xB] = static_cast<unsigned char>(r & 0xF);
    s[9] = 0;
    s[0xA] = 0;
    so->x = UL(Parent() + 0x34);
    so->z = UL(Parent() + 0x38);
    const long h = SH_CALL(AreaMap_Elevation)(rv::Current<records::EffectObject>()->x, rv::Current<records::EffectObject>()->z);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<U>(h));
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// original 0x482D50 (part 0 step 1): +0xB up; +0xA up by 2 below 0x12, at it +9
// = 60 and +3 up.
extern "C" void __cdecl EffectKind69_Part0Grow(void) {
    S()[0xB] = static_cast<unsigned char>(S()[0xB] + 1);
    Rise(0x12);
}

// original 0x482D80 (part 0 step 2): +0xB up; +9 down, at 0 +3 up.
extern "C" void __cdecl EffectKind69_Part0Hold(void) {
    unsigned char* const s = S();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x482DB0 (part 0 step 3): +0xB up; +0xA down, at 0 the parent's +0xB
// up and a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind69_Part0Fade(void) {
    S()[0xB] = static_cast<unsigned char>(S()[0xB] + 1);
    Fade(1);
}

// original 0x482DF0 (EffectKind69_Parts[1]): its step through
// EffectKind69_Part1Steps by +3, the screen point; with +3 past 0 the matrix,
// the column (0x20, 0x50, 0x1F), the glow, a tail jump to Gte_PopMatrix.
extern "C" void __cdecl EffectKind69_Part1(void) {
    PartFrame("EffectKind69_Part1", AddressOf(EffectKind69_Part1Steps), EffectKind69_Part1Steps_count, &DrawColumn1);
}

// original 0x482E40 (part 1 step 0): +0xB = Rand() & 0xF, +9 = +0xA = 0, +0xC =
// +4 << 5; on the orbit of radius 0x20 round the parent; the parent's height;
// +3 up.
extern "C" void __cdecl EffectKind69_Part1Place(void) { PlaceOnOrbit(0, 0x20); }

// original 0x482F00 (part 1 step 1): +0xB up; +0xA up by 2 below 0x10, at it +9
// = 60 and +3 up.
extern "C" void __cdecl EffectKind69_Part1Grow(void) {
    S()[0xB] = static_cast<unsigned char>(S()[0xB] + 1);
    Rise(0x10);
}

// original 0x482F30 (part 1 step 2): +0xB up, +0xC up, the orbit at 0x20; +9
// down, at 0 +3 up.
extern "C" void __cdecl EffectKind69_Part1Orbit(void) {
    unsigned char* const s = S();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c + 1);
    Orbit(0x20);
    unsigned char* const t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    if (t[9] == 0) t[3] = static_cast<unsigned char>(t[3] + 1);
}

// original 0x482FD0 (part 1 step 3): +0xB up, +0xC up, the orbit at 0x20; +0xA
// down, at 0 the parent's +0xB up and a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind69_Part1Fade(void) {
    unsigned char* const s = S();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c + 1);
    Orbit(0x20);
    Fade(1);
}

// original 0x483080 (cdecl, called by parts 0 and 1): a draw mode (0x35, dtd 1)
// in slot 2; then 17 rings i = 1..0x11 of a flickering column along the point's
// frame (Gte_RotTransPers4 of four SVECTORs in Prim_VertexScratch, x 0): each
// ring's y = sin(((+0xB + i) & 0xF) << 8) * R sar 12, R = base + or - (Rand() &
// spread) (Rand's bit 0 picks), its z = -64 i; the previous ring's y and z
// kept. Four semi-transparent POLY_G4s a ring, offset by `width` (its low word)
// down, then two up, then one up, shaded from 7 * +0xA (SharedScratch + 8, its
// low byte read again for each) and the low byte of 13 * +0xA, 1 on the first
// ring; drawn only while the float at 0x5C41DC is below the first quad's third
// corner's y (or unordered). SharedScratch holds R, +4 the angle; both read
// again after the calls.
extern "C" void __cdecl EffectKind69_DrawColumn(unsigned width, unsigned base, unsigned spread) {
    const unsigned char* s = S();
    at::kScale.d_00 = 0x10;
    const U first = (static_cast<U>(s[0xB]) & 0xF) << 8;
    at::kAngle = first;
    at::kShade = 7u * s[0xA];
    const unsigned char bright = static_cast<unsigned char>(13u * s[0xA]);
    const int sn0 = SH_CALL(Math_Sin)(static_cast<int>(first));
    unsigned char* const mode = Gfx_PacketNext;
    Prim_VertexScratch[0].y = Sar(Mul(sn0, static_cast<std::uint32_t>(at::kScale.d_00)), 12);
    Prim_VertexScratch[0].z = 0;
    SH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    const U d = width & 0xFFFF;
    const U d2 = (2 * width) & 0xFFFF;
    const auto shade = [] { return SharedScratch.b_08; };
    const auto quad = [](unsigned char* p) {
        long depth;
        SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x8)),
                                   reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x10)), reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x18)),
                                   reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                                   reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10B)(p);
    };
    const auto shift = [](U delta) {
        AddWord(::bof3::Img(at::kV0.v) + 2, delta);
        AddWord(::bof3::Img(at::kV1.v) + 2, delta);
        AddWord(::bof3::Img(at::kV2.v) + 2, delta);
        AddWord(::bof3::Img(at::kV3.v) + 2, delta);
    };
    for (U i = 1; i < 0x12; ++i) {
        const U angle = ((static_cast<U>(S()[0xB]) + i) & 0xF) << 8;
        at::kAngle = angle;
        U radius;
        if (SH_CALL(Rand)() & 1)
            radius = (static_cast<U>(SH_CALL(Rand)()) & S16(spread)) + S16(base);
        else
            radius = S16(base) - (static_cast<U>(SH_CALL(Rand)()) & S16(spread));
        at::kScale.d_00 = radius;
        const U y = static_cast<std::uint16_t>(Prim_VertexScratch[0].y), z = static_cast<std::uint16_t>(Prim_VertexScratch[0].z);
        Prim_VertexScratch[2].y = y;
        Prim_VertexScratch[3].y = y - d;
        Prim_VertexScratch[2].x = 0;
        Prim_VertexScratch[2].z = z;
        Prim_VertexScratch[3].x = 0;
        Prim_VertexScratch[3].z = z;
        Prim_VertexScratch[0].x = 0;
        const int sn = SH_CALL(Math_Sin)(at::kAngle);
        const U ny = Sar(Mul(sn, static_cast<std::uint32_t>(at::kScale.d_00)), 12);
        Prim_VertexScratch[1].x = 0;
        unsigned char* p = Gfx_PacketNext;
        const U nz = (0u - i) << 6;
        Prim_VertexScratch[0].y = ny;
        Prim_VertexScratch[0].z = nz;
        Prim_VertexScratch[1].y = ny - d;
        Prim_VertexScratch[1].z = nz;
        // the first quad: the ring between this y and the previous, `width` down
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = shade();
        p[5] = shade();
        p[6] = shade();
        p[0x14] = bright;
        p[0x15] = shade();
        p[0x16] = 1;
        if (i == 1) {
            p[0x24] = 1;
            p[0x25] = 1;
            p[0x26] = 1;
            p[0x34] = 1;
            p[0x35] = 1;
        } else {
            p[0x24] = shade();
            p[0x25] = shade();
            p[0x26] = shade();
            p[0x34] = bright;
            p[0x35] = shade();
        }
        p[0x36] = 1;
        quad(p);
        if (!BelowOrUnordered(At(at::kCullY), p + 0x2C)) continue;
        SH_CALL(Gfx_CommitPrim)(2, 0x44);
        // the second: all four `width` down
        p = Gfx_PacketNext;
        shift(0u - d);
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = bright;
        p[5] = shade();
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        if (i == 1) {
            p[0x24] = 1;
            p[0x25] = 1;
        } else {
            p[0x24] = bright;
            p[0x25] = shade();
        }
        p[0x26] = 1;
        p[0x34] = 1;
        p[0x35] = 1;
        p[0x36] = 1;
        quad(p);
        SH_CALL(Gfx_CommitPrim)(2, 0x44);
        // the third: twice `width` up
        p = Gfx_PacketNext;
        shift(d2);
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = bright;
        p[5] = shade();
        p[6] = 1;
        p[0x14] = shade();
        p[0x15] = shade();
        p[0x16] = shade();
        if (i == 1) {
            p[0x24] = 1;
            p[0x25] = 1;
            p[0x26] = 1;
            p[0x34] = 1;
            p[0x35] = 1;
            p[0x36] = 1;
        } else {
            p[0x24] = bright;
            p[0x25] = shade();
            p[0x26] = 1;
            p[0x34] = shade();
            p[0x35] = shade();
            p[0x36] = shade();
        }
        quad(p);
        SH_CALL(Gfx_CommitPrim)(2, 0x44);
        // the fourth: `width` up again
        p = Gfx_PacketNext;
        shift(d);
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = bright;
        p[0x15] = shade();
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        if (i == 1) {
            p[0x34] = 1;
            p[0x35] = 1;
        } else {
            p[0x34] = bright;
            p[0x35] = shade();
        }
        p[0x36] = 1;
        quad(p);
        SH_CALL(Gfx_CommitPrim)(2, 0x44);
        AddWord(::bof3::Img(at::kV0.v) + 2, 0u - d2);
    }
}

// original 0x483540 (EffectKind69_Parts[2]): its step through
// EffectKind69_Part2Steps by +3, the screen point; with +3 past 0 the matrix,
// the lines EffectKind69_DrawLines, the glow, a tail jump to Gte_PopMatrix.
extern "C" void __cdecl EffectKind69_Part2(void) {
    PartFrame("EffectKind69_Part2", AddressOf(EffectKind69_Part2Steps), EffectKind69_Part2Steps_count, &DrawLines2);
}

// original 0x483580 (part 2 step 0): as part 1's, +0xC = (+4 << 5) + 0x10, the
// orbit of radius 0x18.
extern "C" void __cdecl EffectKind69_Part2Place(void) { PlaceOnOrbit(0x10, 0x18); }

// original 0x483640 (part 2 step 1): +0xA up by 2 below 0x10, at it +9 = 60 and
// +3 up (no +0xB step here).
extern "C" void __cdecl EffectKind69_Part2Grow(void) { Rise(0x10); }

// original 0x483660 (part 2 step 2): +0xB up, +0xC up, the orbit at 0x10; +9
// down, at 0 +3 up.
extern "C" void __cdecl EffectKind69_Part2Orbit(void) {
    unsigned char* const s = S();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c + 1);
    Orbit(0x10);
    unsigned char* const t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    if (t[9] == 0) t[3] = static_cast<unsigned char>(t[3] + 1);
}

// original 0x483700 (part 2 step 3): +0xB up, +0xC up, the orbit at 0x18; +0xA
// down by 2, at 0 the parent's +0xB up and a tail jump to Effect_Release (an odd
// +0xA wraps and never reaches 0).
extern "C" void __cdecl EffectKind69_Part2Fade(void) {
    unsigned char* const s = S();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c + 1);
    Orbit(0x18);
    Fade(2);
}

// original 0x4837B0 (EffectKind69_Part2's lines; PSX twin 0x801D218C): in the
// scratch cells (SharedScratch +0, +4, +8, Scratch_Swap) the amplitude 0x40,
// the angle ((+0xB + 2) & 0xF) << 8, the count +0xA, a shade Rand & 0x3F;
// Prim_VertexScratch (0, Math_Sin(angle) * amplitude sar 12, 0). For each i
// from 1 below the count (read again after every call): a draw mode (page
// 0xB5, dtd 1) committed 0xC to slot 2; a semi-transparent LINE_G2 from the
// vertex projected; the amplitude 0x80 +/- Rand & 0x3F (Rand & 1 chooses);
// the vertex (0, Math_Sin(((+0xB + i + 2) & 0xF) << 8) * amplitude sar 12,
// -i << 6) projected for the other end; the shades (shade, shade, 0x20) and
// (next, next, 0x20), next Rand & 0x3F kept; committed 0x24 to slot 2.
extern "C" void __cdecl EffectKind69_DrawLines(void) {
    unsigned char* s = S();
    at::kLineAmp = 0x40;
    at::kLineAngle = ((s[0xB] + 2u) & 0xFu) << 8;
    Scratch_Swap = s[0xA];
    SharedScratch.d_08 = static_cast<U>(SH_CALL(Rand)()) & 0x3Fu;
    unsigned char* const v = reinterpret_cast<unsigned char*>(Prim_VertexScratch);
    {
        const U angle = static_cast<std::uint32_t>(at::kLineAngle);
        SetWord(v, 0);
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
        SetWord(v + 2, Sar(sn * static_cast<std::uint32_t>(at::kLineAmp), 12));
        SetWord(v + 4, 0);
    }
    if (static_cast<std::int32_t>(Scratch_Swap) <= 1) return;
    for (U i = 1;; ++i) {
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
        SH_CALL(Gfx_CommitPrim)(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineG2)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        long depth;
        SH_CALL(Gte_RotTransPers)(reinterpret_cast<short*>(Prim_VertexScratch), reinterpret_cast<unsigned long*>(p + 8), &depth);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        if ((SH_CALL(Rand)() & 1) != 0)
            at::kLineAmp = (static_cast<U>(SH_CALL(Rand)()) & 0x3Fu) + 0x80u;
        else
            at::kLineAmp = 0x80u - (static_cast<U>(SH_CALL(Rand)()) & 0x3Fu);
        s = S();
        const U angle = ((s[0xB] + i + 2u) & 0xFu) << 8;
        SetWord(v, 0);
        at::kLineAngle = angle;
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
        SetWord(v + 2, Sar(sn * static_cast<std::uint32_t>(at::kLineAmp), 12));
        SetWord(v + 4, (0u - i) << 6);
        SH_CALL(Gte_RotTransPers)(reinterpret_cast<short*>(Prim_VertexScratch), reinterpret_cast<unsigned long*>(p + 0x18), &depth);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x20));
        p[4] = at::kLineShade;
        p[5] = at::kLineShade;
        p[6] = 0x20;
        const U next = static_cast<U>(SH_CALL(Rand)()) & 0x3Fu;
        SharedScratch.d_08 = next;
        p[0x14] = static_cast<unsigned char>(next);
        p[0x15] = at::kLineShade;
        p[0x16] = 0x20;
        SH_CALL(Gfx_CommitPrim)(2, 0x24);
        if (!(static_cast<std::int32_t>(i + 1) < static_cast<std::int32_t>(Scratch_Swap))) break;
    }
}

// original 0x483970 (called by parts 1 and 2): SharedScratch = (Rand() & 7) +
// 3 * +0xA; a draw mode (0x35, dtd 1) in slot 2; eight semi-transparent
// POLY_G3s in slot 2 round the screen point (the s16 +0x2E, +0x30): the
// centre shaded (+0xA << 3, +0xA << 3, 6 * +0xA), the rim points (sin a * R
// sar 12 + x, cos a * R sar 12 + y) at a and a + 0x200 shaded 1; a = 0, 0x200 ..
// 0xE00 (SharedScratch and Sprite_Current read again after each call).
extern "C" void __cdecl EffectKind69_DrawGlow(void) {
    const U rnd = static_cast<U>(SH_CALL(Rand)());
    at::kScale.d_00 = (rnd & 7) + 3u * S()[0xA];
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    U a = 0;
    do {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Fild(S16(rv::As<records::EffectWork>(S())->work_2e), p + 8);
        Fild(S16(rv::As<records::EffectWork>(S())->work_30), p + 0xC);
        // each answer scaled by SharedScratch read after its call
        int t = SH_CALL(Math_Sin)(static_cast<int>(a));
        Fild(Sar(Mul(t, static_cast<std::uint32_t>(at::kScale.d_00)), 12) + S16(rv::As<records::EffectWork>(S())->work_2e), p + 0x18);
        t = SH_CALL(Math_Cos)(static_cast<int>(a));
        Fild(Sar(Mul(t, static_cast<std::uint32_t>(at::kScale.d_00)), 12) + S16(rv::As<records::EffectWork>(S())->work_30), p + 0x1C);
        a += 0x200;
        t = SH_CALL(Math_Sin)(static_cast<int>(a));
        Fild(Sar(Mul(t, static_cast<std::uint32_t>(at::kScale.d_00)), 12) + S16(rv::As<records::EffectWork>(S())->work_2e), p + 0x28);
        t = SH_CALL(Math_Cos)(static_cast<int>(a));
        Fild(Sar(Mul(t, static_cast<std::uint32_t>(at::kScale.d_00)), 12) + S16(rv::As<records::EffectWork>(S())->work_30), p + 0x2C);
        const unsigned char level = S()[0xA];
        p[4] = static_cast<unsigned char>(level << 3);
        p[5] = static_cast<unsigned char>(level << 3);
        p[0x14] = 1;
        p[6] = static_cast<unsigned char>(level * 6u);
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        SH_CALL(Gfx_CommitPrim)(2, 0x34);
    } while (static_cast<std::int32_t>(a) < 0x1000);
}

// original 0x483B00 (called by the three parts): BattleActor_UpdateScreenXY's
// shape for the part: the point's SVECTOR, Gpu_SetTile1(Gfx_PacketNext) (never
// committed), Gte_RotTransPers(v, prim + 8, ..), Gte_StoreDepthF(prim + 0x10);
// the float x / y at prim + 8 / + 0xC through _ftol to the words +0x2E / +0x30
// (Sprite_Current read for each).
extern "C" void __cdecl EffectKind69_UpdateScreenXY(void) {
    unsigned char* const p = Gfx_PacketNext;
    short v[4];
    PointVector(S(), v);
    SH_CALL(Gpu_SetTile1)(p);
    long depth;
    SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 8), &depth);
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, FldFtol(p + 8));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, FldFtol(p + 0xC));
}

// original 0x483BA0 (Effect_KindHandlers[0x6C], hidden in 0x483B00): jmp
// [EffectKind6C_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind6C_Run(void) {
    Dispatch("EffectKind6C_Run", AddressOf(EffectKind6C_States), EffectKind6C_States_count, 1);
}

// original 0x483BC0 (state 0): the point (0xB8000, 0x548000, 0x1000000), the
// sparks scattered from it, +1 up.
extern "C" void __cdecl EffectKind6C_Start(void) {
    rv::Current<records::EffectObject>()->x = 0xB8000;
    rv::Current<records::EffectObject>()->z = 0x548000;
    rv::Current<records::EffectObject>()->height = 0x1000000;
    SH_CALL(EffectKind6C_ScatterSparks)();
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x483C00 (state 1): the sparks drawn; none live (al 0): a tail jump
// to Effect_Release.
extern "C" void __cdecl EffectKind6C_Live(void) {
    if ((SH_CALL(EffectKind6C_DrawSparks)() & 0xFF) == 0) SH_CALL(Effect_Release)();
}

void EffectKind69FromEffect3b_Inject() {
    BOF3_INJECT(EffectKind69_Run);
    BOF3_INJECT(EffectKind69_Spawn);
    BOF3_INJECT(EffectKind69_WaitParts);
    BOF3_INJECT(EffectKind69_PartRun);
    BOF3_INJECT(EffectKind69_Part0);
    BOF3_INJECT(EffectKind69_PushPointMatrix);
    BOF3_INJECT(EffectKind69_Part0Place);
    BOF3_INJECT(EffectKind69_Part0Grow);
    BOF3_INJECT(EffectKind69_Part0Hold);
    BOF3_INJECT(EffectKind69_Part0Fade);
    BOF3_INJECT(EffectKind69_Part1);
    BOF3_INJECT(EffectKind69_Part1Place);
    BOF3_INJECT(EffectKind69_Part1Grow);
    BOF3_INJECT(EffectKind69_Part1Orbit);
    BOF3_INJECT(EffectKind69_Part1Fade);
    BOF3_INJECT(EffectKind69_DrawColumn);
    BOF3_INJECT(EffectKind69_Part2);
    BOF3_INJECT(EffectKind69_Part2Place);
    BOF3_INJECT(EffectKind69_Part2Grow);
    BOF3_INJECT(EffectKind69_Part2Orbit);
    BOF3_INJECT(EffectKind69_Part2Fade);
    BOF3_INJECT(EffectKind69_DrawGlow);
    BOF3_INJECT(EffectKind69_UpdateScreenXY);
    BOF3_INJECT(EffectKind6C_Run);
    BOF3_INJECT(EffectKind6C_Start);
    BOF3_INJECT(EffectKind6C_Live);
}

void EffectKind69FromRest3f_Inject() {
    BOF3_INJECT(EffectKind69_DrawLines);
}

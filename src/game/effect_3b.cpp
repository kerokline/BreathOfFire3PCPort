// Round thirteen group E3B (docs/effect_3b.md): the 49 functions of
// analysis/round13_cut.tsv's group E3B, 0x4823D0..0x48404A, each read with
// capstone to its last instruction. Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x63   a disc of 32 shaded triangles on the ground at a fixed point
//               (0x24, 0x41): it grows to 0x190, holds, shrinks while a
//               full-screen tint is drawn and every live sprite and member is
//               refreshed, raises the counter 0x903848, waits for it to reach
//               0x2B, fades the tint and marks the members (+0x29 = 6)
//   kind 0x65   a field wait: on Field_Request 3, unless the save block's dword
//               0x904134 is a multiple of five, a held button toggles story
//               flag 0x4F and shakes the camera for 60 frames, then opens
//               script message 1 and waits for the request to leave 2
//   kind 0x67   spawns a kind-0x13 record every frame of its state 1 (state 0
//               is EffectKind54_Start, state 2 BareRet)
//   kind 0x69   nine parts spawned by one record (kept in 0x676268): +2 picks
//               a part's role - one column at the parent's point, four orbiting
//               at radius 0x20, four at 0x18 / 0x10 - each with its own steps by
//               +3, a matrix pushed at its point, a flickering column of quads
//               and a glow of eight triangles; the parent ends when all nine
//               have counted into its +0xB
//   kind 0x6C   sixteen sparks at (0xB8000, 0x548000, 0x1000000) scattered over
//               EffectKind30_Shards, flying and fading as textured quads; the
//               record ends when none is live
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes a pool past its records or writes a spawned
// record past the pool (Effect_FindFree's 0xFF unchecked), ours aborts with a
// message (docs/effect_3b.md section 7).
#include "game/effect_3b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3b::at;
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
unsigned char& B(U a) { return *At(a); }
U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v & 0xFFFFu))); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
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
// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(U v, const void* c, void* o) {
    const std::int32_t i = static_cast<std::int32_t>(v);
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(i) : "st", "memory");
}
// `fild dword [h]; fsubr dword [c]; fstp dword [o]`: c - h
void FildSubr(U h, const void* c, void* o) {
    const std::int32_t i = static_cast<std::int32_t>(h);
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(i) : "st", "memory");
}
// `fild dword [h]; fsubr dword [c]; fiadd dword [w]; fstp dword [o]`: c - h + w
void FildSubrIadd(U h, const void* c, U w, void* o) {
    const std::int32_t i = static_cast<std::int32_t>(h), j = static_cast<std::int32_t>(w);
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfiaddl %3\n\tfstps (%0)"
                     :
                     : "r"(o), "r"(c), "m"(i), "m"(j)
                     : "st", "memory");
}
// `fld dword [a]; fstp dword [o]` (a quiet copy through the FPU)
void Fld(const void* a, void* o) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(o), "r"(a) : "st", "memory");
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

// ObjTrio record m (a byte index below Field_MemberCount): a Fatal past the three.
unsigned char* Member(const char* who, unsigned m) {
    if (m >= at::kMembers)
        bof3::Fatal("%s: member %u of Field_MemberCount %u, past ObjTrio's three records - the original writes "
                    "what follows (docs/effect_3b.md section 7)",
                    who, m, (unsigned)Field_MemberCount);
    return ObjTrio + m * at::kObjTrioStride;
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
    return Effect_Objects + k * at::kEffectStride;
}

unsigned char* Parent() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(UL(at::kParent))); }
unsigned char* Spark() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(UL(at::kSparkCursor))); }

void ScreenTint() { SH_AT(void (__cdecl*)(), at::kScreenTint)(); }
void Kind69Lines() { SH_AT(void (__cdecl*)(), at::kKind69Lines)(); }

// The point +0x34 projected into the floats +0x74.. (EffectGte_ProjectPoint).
void ProjectHere() {
    unsigned char* const s = S();
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(s + 0x34), reinterpret_cast<float*>(s + 0x74));
}

// Sprite_Current's (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)), the
// SVECTOR the matrix and the screen point are built from (the pad word is not
// written by the original either).
void PointVector(const unsigned char* s, short* v) {
    v[0] = static_cast<short>(Sar(UL(s + 0x34), 9) - 0x4000u);
    v[1] = static_cast<short>(Sar(UL(s + 0x38), 9) - 0x4000u);
    v[2] = static_cast<short>(-(static_cast<std::int16_t>(Word(s + 0x3E)) / 2));
}

// The orbit kind 0x69's parts 1 and 2 share: DamageScratch = radius, the angle
// (+0xC & 0x7F) << 5 to 0x903854; x = cos * the scratch (re-read) + the parent's
// x, z = sin(0x903854, re-read) * the scratch + the parent's z - no shift.
void Orbit(U radius) {
    SetUL(at::kScale, radius);
    const U angle = (UL(S() + 0xC) & 0x7F) << 5;
    SetUL(at::kAngle, angle);
    const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
    SetUL(S() + 0x34, Mul(c, UL(at::kScale)) + UL(Parent() + 0x34));
    const int n = SH_CALL(Math_Sin)(static_cast<int>(UL(at::kAngle)));
    SetUL(S() + 0x38, Mul(n, UL(at::kScale)) + UL(Parent() + 0x38));
}

// A part's place: +0xB = Rand() & 0xF, +9 = +0xA = 0, +0xC = (+4 << 5) + start,
// the orbit at radius, the parent's height, +3 up.
void PlaceOnOrbit(U start, U radius) {
    const int r = SH_CALL(Rand)();
    unsigned char* const s = S();
    s[0xB] = static_cast<unsigned char>(r & 0xF);
    s[9] = 0;
    s[0xA] = 0;
    SetUL(s + 0xC, (static_cast<U>(s[4]) << 5) + start);
    Orbit(radius);
    SetWord(S() + 0x3E, Word(Parent() + 0x3E));
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

}  // namespace

// ===========================================================================
// Kind 0x63: Effect_KindHandlers[0x63] (0x6554DC), EffectKind63_States (seven)
// ===========================================================================

// original 0x4823D0 (Effect_KindHandlers[0x63], hidden in E3A's 0x482360):
// jmp [EffectKind63_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind63_Run(void) {
    Dispatch("EffectKind63_Run", AddressOf(EffectKind63_States), EffectKind63_States_count, 1);
}

// original 0x4823F0 (state 0): the point (0x240000, 0x418000), its height
// AreaMap_Elevation(x, z) to the word +0x3E, the disc's radius +0x18 = 0, its
// rim shade +0x5C = 0x3D, +1 = 1, the tint +0x5D..+0x5F = 0 (eax cleared after
// the call; Sprite_Current read again for each store).
extern "C" void __cdecl EffectKind63_Start(void) {
    SetUL(S() + 0x34, 0x240000);
    SetUL(S() + 0x38, 0x418000);
    const long h = SH_CALL(AreaMap_Elevation)(Long(S() + 0x34), Long(S() + 0x38));
    SetWord(S() + 0x3E, static_cast<U>(h));
    SetUL(S() + 0x18, 0);
    S()[0x5C] = 0x3D;
    S()[1] = 1;
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
}

// original 0x482470 (state 1): the point projected, the disc (radius the word
// +0x18, centre 0xFF, rim +0x5C); radius up 5, rim up 2; past 0x190 (signed)
// +9 = 0xF and +1 up.
extern "C" void __cdecl EffectKind63_Grow(void) {
    ProjectHere();
    SH_CALL(EffectKind63_DrawDisc)(Word(S() + 0x18), 0xFF, S()[0x5C]);
    unsigned char* const s = S();
    SetUL(s + 0x18, UL(s + 0x18) + 5);
    s[0x5C] = static_cast<unsigned char>(s[0x5C] + 2);
    if (static_cast<std::int32_t>(UL(s + 0x18)) > 0x190) {
        s[9] = 0xF;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x4824E0 (state 2): the point projected, the disc white to its rim;
// +9 down, at 0 the tint +0x5D..+0x5F = 0x60 and +1 up.
extern "C" void __cdecl EffectKind63_Hold(void) {
    ProjectHere();
    SH_CALL(EffectKind63_DrawDisc)(Word(S() + 0x18), 0xFF, 0xFF);
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    s[0x5F] = 0x60;
    s[0x5E] = 0x60;
    s[0x5D] = 0x60;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x482550 (state 3): the point projected, the disc (rim +0x5C), the
// screen tint 0x48CA90, the sprites refreshed; radius down 10, rim down 2; at a
// radius of 0 or below the counter 0x903848 up and +1 up.
extern "C" void __cdecl EffectKind63_Shrink(void) {
    ProjectHere();
    SH_CALL(EffectKind63_DrawDisc)(Word(S() + 0x18), 0xFF, S()[0x5C]);
    ScreenTint();
    SH_CALL(EffectKind63_RefreshSprites)();
    unsigned char* const s = S();
    SetUL(s + 0x18, UL(s + 0x18) - 10);
    s[0x5C] = static_cast<unsigned char>(s[0x5C] - 2);
    if (static_cast<std::int32_t>(UL(s + 0x18)) > 0) return;
    B(at::kCounter) = static_cast<unsigned char>(B(at::kCounter) + 1);
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4825D0 (state 4): with the counter 0x903848 at 0x2B, +9 = 0x10 and
// +1 = 5; then the tint and (a tail jump) the sprites refreshed.
extern "C" void __cdecl EffectKind63_WaitCue(void) {
    if (B(at::kCounter) == at::kCounterCue) {
        S()[9] = 0x10;
        S()[1] = 5;
    }
    ScreenTint();
    SH_CALL(EffectKind63_RefreshSprites)();
}

// original 0x482600 (state 5): +9 down; not at 0 the tint +0x5D..+0x5F each down
// 6, at 0 +1 = 6; then the tint and (a tail jump) the sprites refreshed.
extern "C" void __cdecl EffectKind63_FadeOut(void) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xFA);
        s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xFA);
        s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xFA);
    } else {
        s[1] = 6;
    }
    ScreenTint();
    SH_CALL(EffectKind63_RefreshSprites)();
}

// original 0x482650 (state 6): each member below Field_MemberCount (read again
// each turn) gets +0x29 = 6; Sprite_Current written back; Effect_Release.
extern "C" void __cdecl EffectKind63_End(void) {
    unsigned char* const saved = Sprite_Current;
    unsigned i = 0;
    if (Field_MemberCount != 0) {
        do {
            const unsigned m = i;
            i = (i + 1) & 0xFF;
            Member("EffectKind63_End", m)[0x29] = 6;
        } while (i < Field_MemberCount);
    }
    Sprite_Current = saved;
    SH_CALL(Effect_Release)();
}

// original 0x4826B0 (called by state 3, tail-jumped to by states 4 and 5): with
// Sprite_Current each of the thirty Sprite_Objects records in turn (written
// for every record), a live one (+0 bit 0) gets +0x29 = 5 and
// Sprite_UpdateScreen; then each member below Field_MemberCount (read again
// after each call) the same; Sprite_Current put back.
extern "C" void __cdecl EffectKind63_RefreshSprites(void) {
    unsigned char* const saved = Sprite_Current;
    unsigned char* o = Sprite_Objects;
    for (unsigned n = at::kSprites; n != 0; --n, o += at::kSpriteStride) {
        Sprite_Current = o;
        if ((o[0] & 1) == 0) continue;
        o[0x29] = 5;
        SH_CALL(Sprite_UpdateScreen)();
    }
    unsigned i = 0;
    if (Field_MemberCount != 0) {
        do {
            unsigned char* const m = Member("EffectKind63_RefreshSprites", i);
            Sprite_Current = m;
            m[0x29] = 5;
            SH_CALL(Sprite_UpdateScreen)();
            i = (i + 1) & 0xFF;
        } while (i < Field_MemberCount);
    }
    Sprite_Current = saved;
}

// original 0x482740 (cdecl, called by states 1..3): a draw mode (page
// Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1) in slot 1, then 32 semi-transparent
// POLY_G3s in slot 1: the centre the floats +0x74 / +0x78 (copied as dwords)
// and +0x7C (fld / fst: quiet), shade `centre` (its low byte); the rim points
// (cos a * r sar 12 + x, sin a * r sar 12 + y) at a and at b = a + 0x80, shade
// `rim`; r the s16 `radius`, a = 0, 0x80 .. 0xF80 (Sprite_Current read after
// each call). The original keeps a in its first argument's slot.
extern "C" void __cdecl EffectKind63_DrawDisc(unsigned radius, unsigned centre, unsigned rim) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const U r = S16(radius);
    const unsigned char rim_shade = static_cast<unsigned char>(rim);
    U a = 0;
    U next = 0;
    do {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        next += 0x80;
        const U b = next & 0xFFF;
        std::memcpy(p + 8, S() + 0x74, 4);
        std::memcpy(p + 0xC, S() + 0x78, 4);
        const U c1 = Sar(Mul(SH_CALL(Math_Cos)(static_cast<int>(a)), r), 12);
        FildAdd(c1, S() + 0x74, p + 0x18);
        const U s1 = Sar(Mul(SH_CALL(Math_Sin)(static_cast<int>(a)), r), 12);
        FildAdd(s1, S() + 0x78, p + 0x1C);
        const U c2 = Sar(Mul(SH_CALL(Math_Cos)(static_cast<int>(b)), r), 12);
        FildAdd(c2, S() + 0x74, p + 0x28);
        const U s2 = Sar(Mul(SH_CALL(Math_Sin)(static_cast<int>(b)), r), 12);
        unsigned char* const s = S();
        FildAdd(s2, s + 0x78, p + 0x2C);
        p[4] = static_cast<unsigned char>(centre);
        p[5] = static_cast<unsigned char>(centre);
        Fld(s + 0x7C, p + 0x30);
        Fld(s + 0x7C, p + 0x20);
        Fld(s + 0x7C, p + 0x10);
        p[6] = static_cast<unsigned char>(centre);
        p[0x14] = rim_shade;
        p[0x15] = rim_shade;
        p[0x16] = rim_shade;
        p[0x24] = rim_shade;
        p[0x25] = rim_shade;
        p[0x26] = rim_shade;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        a += 0x80;
    } while ((next & 0xFFFF) < 0x1000);
}

// ===========================================================================
// Kind 0x65: Effect_KindHandlers[0x65] (0x6554E4), EffectKind65_States (five)
// ===========================================================================

// original 0x4828B0 (Effect_KindHandlers[0x65], hidden in 0x482740): jmp
// [EffectKind65_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind65_Run(void) {
    Dispatch("EffectKind65_Run", AddressOf(EffectKind65_States), EffectKind65_States_count, 1);
}

// original 0x4828D0 (state 0): Field_Request 3: +1 = 1.
extern "C" void __cdecl EffectKind65_WaitRequest(void) {
    if (Field_Request == 3) S()[1] = 1;
}

// original 0x4828F0 (state 1): nothing while Field_Request is 3; else the dword
// 0x904134 not a multiple of 5 (unsigned): +1 = 0; a multiple:
// ScriptFlags_Set40 and +1 = 2.
extern "C" void __cdecl EffectKind65_Check(void) {
    if (Field_Request == 3) return;
    if (UL(at::kKind65Count) % 5 != 0) {
        S()[1] = 0;
        return;
    }
    SH_CALL(ScriptFlags_Set40)();
    S()[1] = 2;
}

// original 0x482930 (state 2): with Input_Held not 0: Sound_PlayEffect(0x202),
// story flag 0x4F toggled, +9 = 60, +1 = 3.
extern "C" void __cdecl EffectKind65_WaitInput(void) {
    if (Input_Held == 0) return;
    SH_CALL(Sound_PlayEffect)(0x202);
    SH_CALL(Flags_Toggle)(At(at::kStoryFlags), at::kKind65Flag);
    S()[9] = 0x3C;
    S()[1] = 3;
}

// original 0x482970 (state 3): +9 down; at 0 Msg_OpenScript(1), Field_Request =
// 2, +1 = 4; else MapView_Redraw = 2 and Camera_ShiftY += 4 * the signed byte
// EffectKind65_ShakeSteps[+9 & 3] (16 bits).
extern "C" void __cdecl EffectKind65_Shake(void) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    const unsigned left = s[9];
    if (left == 0) {
        SH_CALL(Msg_OpenScript)(1);
        Field_Request = 2;
        S()[1] = 4;
        return;
    }
    MapView_Redraw = 2;
    const U step = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kShakeSteps + (left & 3)))));
    Camera_ShiftY = static_cast<short>(static_cast<U>(static_cast<unsigned short>(Camera_ShiftY)) + (step << 2));
}

// original 0x4829D0 (state 4): with Field_Request no longer 2:
// ScriptFlags_Clear40, Sound_PlayEffect(0x206), +1 = 0.
extern "C" void __cdecl EffectKind65_Close(void) {
    if (Field_Request == 2) return;
    SH_CALL(ScriptFlags_Clear40)();
    SH_CALL(Sound_PlayEffect)(0x206);
    S()[1] = 0;
}

// ===========================================================================
// Kind 0x67: Effect_KindHandlers[0x67] (0x6554EC), EffectKind67_States (three:
// EffectKind54_Start, this one, BareRet)
// ===========================================================================

// original 0x482A00 (Effect_KindHandlers[0x67], hidden in 0x482740): jmp
// [EffectKind67_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind67_Run(void) {
    Dispatch("EffectKind67_Run", AddressOf(EffectKind67_States), EffectKind67_States_count, 1);
}

// original 0x482A20 (state 1): Effect_FindFree's byte to 0x903850; not 0xFF:
// that record +0 = 1, kind +5 = 0x13, +0x64 = -0x386, +0x68 = the s16
// Camera_Angles + 2, +0x6C = 0xE2, +9 = 0x1E. Its own +1 is left: one a frame.
extern "C" void __cdecl EffectKind67_SpawnKind13(void) {
    const U answer = SH_CALL(Effect_FindFree)();
    B(at::kScale) = static_cast<unsigned char>(answer);
    if ((answer & 0xFF) == 0xFF) return;
    const U angle = S16(W(at::kCameraAngle1));
    unsigned char* const e = Spawned("EffectKind67_SpawnKind13", UL(at::kScale));
    e[0] = 1;
    e[5] = 0x13;
    SetUL(e + 0x64, 0xFFFFFC7Au);
    SetUL(e + 0x68, angle);
    SetUL(e + 0x6C, 0xE2);
    e[9] = 0x1E;
}

// ===========================================================================
// Kind 0x69: Effect_KindHandlers[0x69] (0x6554F4) - a stack table of three by
// +1; state 2 by +2 through EffectKind69_Parts (three), each part by +3 through
// its own steps (four each)
// ===========================================================================

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
    SetUL(at::kParent, AddressOf(Sprite_Current));
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

namespace {
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
    s[0xB] = static_cast<unsigned char>(r & 0xF);
    s[9] = 0;
    s[0xA] = 0;
    SetUL(s + 0x34, UL(Parent() + 0x34));
    SetUL(s + 0x38, UL(Parent() + 0x38));
    const long h = SH_CALL(AreaMap_Elevation)(Long(S() + 0x34), Long(S() + 0x38));
    SetWord(S() + 0x3E, static_cast<U>(h));
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
    SetUL(s + 0xC, UL(s + 0xC) + 1);
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
    SetUL(s + 0xC, UL(s + 0xC) + 1);
    Orbit(0x20);
    Fade(1);
}

// original 0x483080 (cdecl, called by parts 0 and 1): a draw mode (0x35, dtd 1)
// in slot 2; then 17 rings i = 1..0x11 of a flickering column along the point's
// frame (Gte_RotTransPers4 of four SVECTORs in Prim_VertexScratch, x 0): each
// ring's y = sin(((+0xB + i) & 0xF) << 8) * R sar 12, R = base + or - (Rand() &
// spread) (Rand's bit 0 picks), its z = -64 i; the previous ring's y and z
// kept. Four semi-transparent POLY_G4s a ring, offset by `width` (its low word)
// down, then two up, then one up, shaded from 7 * +0xA (DamageScratch + 8, its
// low byte read again for each) and the low byte of 13 * +0xA, 1 on the first
// ring; drawn only while the float at 0x5C41DC is below the first quad's third
// corner's y (or unordered). DamageScratch holds R, +4 the angle; both read
// again after the calls.
extern "C" void __cdecl EffectKind69_DrawColumn(unsigned width, unsigned base, unsigned spread) {
    const unsigned char* s = S();
    SetUL(at::kScale, 0x10);
    const U first = (static_cast<U>(s[0xB]) & 0xF) << 8;
    SetUL(at::kAngle, first);
    SetUL(at::kShade, 7u * s[0xA]);
    const unsigned char bright = static_cast<unsigned char>(13u * s[0xA]);
    const int sn0 = SH_CALL(Math_Sin)(static_cast<int>(first));
    unsigned char* const mode = Gfx_PacketNext;
    SetWord(At(at::kV0 + 2), Sar(Mul(sn0, UL(at::kScale)), 12));
    SetWord(At(at::kV0 + 4), 0);
    SH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    const U d = width & 0xFFFF;
    const U d2 = (2 * width) & 0xFFFF;
    const auto shade = [] { return B(at::kShade); };
    const auto quad = [](unsigned char* p) {
        long depth;
        SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(At(at::kV0)), reinterpret_cast<const short*>(At(at::kV1)),
                                   reinterpret_cast<const short*>(At(at::kV2)), reinterpret_cast<const short*>(At(at::kV3)),
                                   reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                                   reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10B)(p);
    };
    const auto shift = [](U delta) {
        AddWord(at::kV0 + 2, delta);
        AddWord(at::kV1 + 2, delta);
        AddWord(at::kV2 + 2, delta);
        AddWord(at::kV3 + 2, delta);
    };
    for (U i = 1; i < 0x12; ++i) {
        const U angle = ((static_cast<U>(S()[0xB]) + i) & 0xF) << 8;
        SetUL(at::kAngle, angle);
        U radius;
        if (SH_CALL(Rand)() & 1)
            radius = (static_cast<U>(SH_CALL(Rand)()) & S16(spread)) + S16(base);
        else
            radius = S16(base) - (static_cast<U>(SH_CALL(Rand)()) & S16(spread));
        SetUL(at::kScale, radius);
        const U y = W(at::kV0 + 2), z = W(at::kV0 + 4);
        SetWord(At(at::kV2 + 2), y);
        SetWord(At(at::kV3 + 2), y - d);
        SetWord(At(at::kV2), 0);
        SetWord(At(at::kV2 + 4), z);
        SetWord(At(at::kV3), 0);
        SetWord(At(at::kV3 + 4), z);
        SetWord(At(at::kV0), 0);
        const int sn = SH_CALL(Math_Sin)(static_cast<int>(UL(at::kAngle)));
        const U ny = Sar(Mul(sn, UL(at::kScale)), 12);
        SetWord(At(at::kV1), 0);
        unsigned char* p = Gfx_PacketNext;
        const U nz = (0u - i) << 6;
        SetWord(At(at::kV0 + 2), ny);
        SetWord(At(at::kV0 + 4), nz);
        SetWord(At(at::kV1 + 2), ny - d);
        SetWord(At(at::kV1 + 4), nz);
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
        AddWord(at::kV0 + 2, 0u - d2);
    }
}

// original 0x483540 (EffectKind69_Parts[2]): its step through
// EffectKind69_Part2Steps by +3, the screen point; with +3 past 0 the matrix,
// the lines 0x4837B0, the glow, a tail jump to Gte_PopMatrix.
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
    SetUL(s + 0xC, UL(s + 0xC) + 1);
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
    SetUL(s + 0xC, UL(s + 0xC) + 1);
    Orbit(0x18);
    Fade(2);
}

// original 0x483970 (called by parts 1 and 2): DamageScratch = (Rand() & 7) +
// 3 * +0xA; a draw mode (0x35, dtd 1) in slot 2; eight semi-transparent
// POLY_G3s in slot 2 round the screen point (the s16 +0x2E, +0x30): the
// centre shaded (+0xA << 3, +0xA << 3, 6 * +0xA), the rim points (sin a * R
// sar 12 + x, cos a * R sar 12 + y) at a and a + 0x200 shaded 1; a = 0, 0x200 ..
// 0xE00 (DamageScratch and Sprite_Current read again after each call).
extern "C" void __cdecl EffectKind69_DrawGlow(void) {
    const U rnd = static_cast<U>(SH_CALL(Rand)());
    SetUL(at::kScale, (rnd & 7) + 3u * S()[0xA]);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    U a = 0;
    do {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Fild(S16(Word(S() + 0x2E)), p + 8);
        Fild(S16(Word(S() + 0x30)), p + 0xC);
        U v = Sar(Mul(SH_CALL(Math_Sin)(static_cast<int>(a)), UL(at::kScale)), 12);
        Fild(v + S16(Word(S() + 0x2E)), p + 0x18);
        v = Sar(Mul(SH_CALL(Math_Cos)(static_cast<int>(a)), UL(at::kScale)), 12);
        Fild(v + S16(Word(S() + 0x30)), p + 0x1C);
        a += 0x200;
        v = Sar(Mul(SH_CALL(Math_Sin)(static_cast<int>(a)), UL(at::kScale)), 12);
        Fild(v + S16(Word(S() + 0x2E)), p + 0x28);
        v = Sar(Mul(SH_CALL(Math_Cos)(static_cast<int>(a)), UL(at::kScale)), 12);
        Fild(v + S16(Word(S() + 0x30)), p + 0x2C);
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
    SetWord(S() + 0x2E, FldFtol(p + 8));
    SetWord(S() + 0x30, FldFtol(p + 0xC));
}

// ===========================================================================
// Kind 0x6C: Effect_KindHandlers[0x6C] (0x655500), EffectKind6C_States (two);
// its sparks by their +1 through EffectKind6C_SparkStates (two)
// ===========================================================================

// original 0x483BA0 (Effect_KindHandlers[0x6C], hidden in 0x483B00): jmp
// [EffectKind6C_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind6C_Run(void) {
    Dispatch("EffectKind6C_Run", AddressOf(EffectKind6C_States), EffectKind6C_States_count, 1);
}

// original 0x483BC0 (state 0): the point (0xB8000, 0x548000, 0x1000000), the
// sparks scattered from it, +1 up.
extern "C" void __cdecl EffectKind6C_Start(void) {
    SetUL(S() + 0x34, 0xB8000);
    SetUL(S() + 0x38, 0x548000);
    SetUL(S() + 0x3C, 0x1000000);
    SH_CALL(EffectKind6C_ScatterSparks)();
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x483C00 (state 1): the sparks drawn; none live (al 0): a tail jump
// to Effect_Release.
extern "C" void __cdecl EffectKind6C_Live(void) {
    if ((SH_CALL(EffectKind6C_DrawSparks)() & 0xFF) == 0) SH_CALL(Effect_Release)();
}

// original 0x483C10 (cdecl, no arguments; state 0's, and E3C's 0x484070): the
// cursor 0x67626C at EffectKind30_Shards, then for sixteen records of 0x28 (the
// cursor read again for every access, stepped in memory): +4 / +8 / +0xC
// Sprite_Current's +0x34 / +0x38 / +0x3C; a distance (Rand() & 0x7F) << 2 and an
// angle (Rand() & 0x3FF) + 0x200; +0x14 = cos * distance sar 7, +0x18 = sin *
// distance sar 7; +0 = 1, +1 = 0, the word +0x24 = 0, +3 = 0x40, +0x26 = 7, +2 =
// 4.
extern "C" void __cdecl EffectKind6C_ScatterSparks(void) {
    SetUL(at::kSparkCursor, at::kSparks);
    for (unsigned n = at::kSparkCount; n != 0; --n) {
        SetUL(Spark() + 4, UL(S() + 0x34));
        SetUL(Spark() + 8, UL(S() + 0x38));
        SetUL(Spark() + 0xC, UL(S() + 0x3C));
        const U distance = (static_cast<U>(SH_CALL(Rand)()) & 0x7F) << 2;
        const U angle = ((static_cast<U>(SH_CALL(Rand)()) & 0x3FF) + 0x200) & 0xFFFF;
        SetUL(Spark() + 0x14, Sar(Mul(SH_CALL(Math_Cos)(static_cast<int>(angle)), distance), 7));
        SetUL(Spark() + 0x18, Sar(Mul(SH_CALL(Math_Sin)(static_cast<int>(angle)), distance), 7));
        Spark()[0] = 1;
        Spark()[1] = 0;
        SetWord(Spark() + 0x24, 0);
        Spark()[3] = 0x40;
        Spark()[0x26] = 7;
        Spark()[2] = 4;
        SetUL(at::kSparkCursor, UL(at::kSparkCursor) + at::kSparkStride);
    }
}

// original 0x483D10 (state 1's; answers al): the map camera
// (EffectGte_LoadMapCamera), a draw mode (page Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 0) in slot 2; for the sixteen spark records (the cursor 0x67626C stepped
// in memory, a register copy tested) each live one (+0 not 0): its state
// through EffectKind6C_SparkStates by +1 (a call, unbounded), its quad
// (EffectKind6C_DrawSpark of the cursor, read again), the cursor read back;
// al 1 when any was live, else 0.
extern "C" unsigned char __cdecl EffectKind6C_DrawSparks(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char any = 0;
    U c = at::kSparks;
    SetUL(at::kSparkCursor, c);
    for (unsigned n = at::kSparkCount; n != 0; --n) {
        if (At(c)[0] != 0) {
            Through("EffectKind6C_DrawSparks", AddressOf(EffectKind6C_SparkStates), EffectKind6C_SparkStates_count,
                    At(c)[1], 1);
            SH_CALL(EffectKind6C_DrawSpark)(Spark());
            c = UL(at::kSparkCursor);
            any = 1;
        }
        c += at::kSparkStride;
        SetUL(at::kSparkCursor, c);
    }
    return any;
}

// original 0x483DA0 (cdecl; called by 0x483D10 and E3C's 0x4841E0): a spark's
// semi-transparent POLY_FT4 at the packet cursor (read once): its point +4
// projected (EffectGte_ProjectPoint, x / y / depth), its size the word +0x24
// handed twice as (w, h) and scaled at that depth into the same two words
// (EffectGte_ProjectSize); corners x - (w sar 1) and that + w, y - (h sar 1) and
// that + h (fild, fsubr, fiadd); the depth copied to +0x10 / +0x20 / +0x30 /
// +0x40; u 0xE0 / 0xFF, v 0x30 / 0x4F; CLUT Gpu_GetClut(0x50, 0x1E3), page
// Gpu_GetTPage(0, 1, 0x2C0, 0x100); red / green / blue the spark's +3 where
// +0x26 has bit 2 / 1 / 0 (read after the page), else 0; slot 2, 0x48.
extern "C" void __cdecl EffectKind6C_DrawSpark(unsigned char* spark) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    alignas(4) unsigned char screen[12];
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(spark + 4), reinterpret_cast<float*>(screen));
    short size[2];
    size[0] = static_cast<short>(Word(spark + 0x24));
    size[1] = size[0];
    SH_CALL(EffectGte_ProjectSize)(reinterpret_cast<const long*>(spark + 4), size, size);
    const U w = S16(static_cast<unsigned short>(size[0])), h = S16(static_cast<unsigned short>(size[1]));
    const U hw = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(size[0]) >> 1));
    const U hh = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(size[1]) >> 1));
    FildSubr(hw, screen, p + 8);
    FildSubr(hh, screen + 4, p + 0xC);
    FildSubrIadd(hw, screen, w, p + 0x18);
    FildSubr(hh, screen + 4, p + 0x1C);
    FildSubr(hw, screen, p + 0x28);
    FildSubrIadd(hh, screen + 4, h, p + 0x2C);
    FildSubrIadd(hw, screen, w, p + 0x38);
    p[0x15] = 0x30;
    p[0x25] = 0x30;
    p[0x14] = 0xE0;
    p[0x24] = 0xFF;
    p[0x34] = 0xE0;
    p[0x35] = 0x4F;
    p[0x44] = 0xFF;
    p[0x45] = 0x4F;
    FildSubrIadd(hh, screen + 4, h, p + 0x3C);
    std::memcpy(p + 0x40, screen + 8, 4);
    std::memcpy(p + 0x30, screen + 8, 4);
    std::memcpy(p + 0x20, screen + 8, 4);
    std::memcpy(p + 0x10, screen + 8, 4);
    SetWord(p + 0x16, SH_CALL(Gpu_GetClut)(0x50, 0x1E3));
    SetWord(p + 0x26, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    const unsigned char bits = spark[0x26];
    p[4] = (bits & 4) ? spark[3] : 0;
    p[5] = (bits & 2) ? spark[3] : 0;
    p[6] = (bits & 1) ? spark[3] : 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// original 0x483FA0 (EffectKind6C_SparkStates[0]; the spark at the cursor): +4
// += +0x14 << 1, +8 += +0x18 << 1, the word +0x24 up 0x80; +2 down, at 0 +2 =
// 0x40 and +1 up.
extern "C" void __cdecl EffectKind6C_SparkFly(void) {
    SetUL(Spark() + 4, UL(Spark() + 4) + (UL(Spark() + 0x14) << 1));
    SetUL(Spark() + 8, UL(Spark() + 8) + (UL(Spark() + 0x18) << 1));
    SetWord(Spark() + 0x24, Word(Spark() + 0x24) + 0x80);
    Spark()[2] = static_cast<unsigned char>(Spark()[2] - 1);
    if (Spark()[2] != 0) return;
    Spark()[2] = 0x40;
    Spark()[1] = static_cast<unsigned char>(Spark()[1] + 1);
}

// original 0x484000 (EffectKind6C_SparkStates[1]): +4 += +0x14, +8 += +0x18,
// +3 down, +2 down, at 0 the spark freed (+0 = 0).
extern "C" void __cdecl EffectKind6C_SparkFade(void) {
    SetUL(Spark() + 4, UL(Spark() + 4) + UL(Spark() + 0x14));
    SetUL(Spark() + 8, UL(Spark() + 8) + UL(Spark() + 0x18));
    Spark()[3] = static_cast<unsigned char>(Spark()[3] - 1);
    Spark()[2] = static_cast<unsigned char>(Spark()[2] - 1);
    if (Spark()[2] == 0) Spark()[0] = 0;
}

void Effect3B_Inject() {
    if (bof3::WantsShadow("effect_3b")) effect_3b::SelfTest();
    BOF3_INJECT(EffectKind63_Run);
    BOF3_INJECT(EffectKind63_Start);
    BOF3_INJECT(EffectKind63_Grow);
    BOF3_INJECT(EffectKind63_Hold);
    BOF3_INJECT(EffectKind63_Shrink);
    BOF3_INJECT(EffectKind63_WaitCue);
    BOF3_INJECT(EffectKind63_FadeOut);
    BOF3_INJECT(EffectKind63_End);
    BOF3_INJECT(EffectKind63_RefreshSprites);
    BOF3_INJECT(EffectKind63_DrawDisc);
    BOF3_INJECT(EffectKind65_Run);
    BOF3_INJECT(EffectKind65_WaitRequest);
    BOF3_INJECT(EffectKind65_Check);
    BOF3_INJECT(EffectKind65_WaitInput);
    BOF3_INJECT(EffectKind65_Shake);
    BOF3_INJECT(EffectKind65_Close);
    BOF3_INJECT(EffectKind67_Run);
    BOF3_INJECT(EffectKind67_SpawnKind13);
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
    BOF3_INJECT(EffectKind6C_ScatterSparks);
    BOF3_INJECT(EffectKind6C_DrawSparks);
    BOF3_INJECT(EffectKind6C_DrawSpark);
    BOF3_INJECT(EffectKind6C_SparkFly);
    BOF3_INJECT(EffectKind6C_SparkFade);
}

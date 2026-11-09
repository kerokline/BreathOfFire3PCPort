// Moved here by tools/move_funcs.py from effect_4b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_88.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_4b.h"
#include "game/effect_4b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/sprite_draw.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_4b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }

// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}

// `fld dword [a]; fsub dword [c]; fstp dword [o]`
void FldSub(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%0)\n\tfsubs (%1)\n\tfstps (%2)" : : "r"(a), "r"(c), "r"(o) : "st", "memory");
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the
// dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4b.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// ObjTrio record m; past the three, ours aborts (the original writes what
// follows).
unsigned char* Member(unsigned m, const char* who) {
    if (m >= at::kObjTrioCount)
        bof3::Fatal("%s: party member %u, past ObjTrio's three records - the original writes what follows "
                    "(docs/effect_4b.md section 6)",
                    who, m);
    return rv::Bytes(&ObjTrio[m]);
}

// States 1 and 2: every eighth count (+9 & 7 0) a free particle set up; +9
// up; +6 set - the particles moved and drawn; extra object 0's x at `goal`:
// +1 up.
void Kind88Emit(U goal) {
    if ((S()[9] & 7) == 0) {
        unsigned char* const r = SH_CALL(EffectKind88_FindParticle)();
        if (r != nullptr) SH_CALL(EffectKind88_InitParticle)(r);
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* s = S();
    if (s[6] != 0) {
        SH_CALL(EffectKind88_MoveParticles)();
        s = S();
    }
    if (static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].x) == goal) s[1] = static_cast<unsigned char>(s[1] + 1);
}

}  // namespace

// original 0x4896A0 (Effect_KindHandlers[0x88], hidden after 0x489630):
// jmp [EffectKind88_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind88_Run(void) {
    Dispatch("EffectKind88_Run", AddressOf(EffectKind88_States), EffectKind88_States_count);
}

// original 0x4896C0 (state 0): +6 set - the 32 particles cleared
// (EffectKind88_ClearParticles); +9 0, +1 up.
extern "C" void __cdecl EffectKind88_Start(void) {
    if (S()[6] != 0) SH_CALL(EffectKind88_ClearParticles)();
    S()[9] = 0;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x4896F0 (state 1): Kind88Emit until x 0x3C8000.
extern "C" void __cdecl EffectKind88_Emit(void) { Kind88Emit(0x3C8000); }

// original 0x489740 (state 2): Kind88Emit until x 0x408000.
extern "C" void __cdecl EffectKind88_EmitOn(void) { Kind88Emit(0x408000); }

// original 0x489790 (state 3): the particles moved and drawn; none live (al 0):
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind88_Fade(void) {
    if (SH_CALL(EffectKind88_MoveParticles)() == 0) SH_CALL(Effect_Release)();
}

// original 0x4897A0: the 32 particles' +3 cleared.
extern "C" void __cdecl EffectKind88_ClearParticles(void) {
    for (unsigned i = 0; i < at::kParticleCount; ++i) At(::bof3::Img(EffectKind30_Shards_addr) + at::kParticleStride * i)[3] = 0;
}

// original 0x4897C0: the first of the 32 particles whose +3 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind88_FindParticle(void) {
    for (unsigned i = 0; i < at::kParticleCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kParticleStride * i);
        if (r[3] == 0) return r;
    }
    return nullptr;
}

// original 0x4897E0: EffectGte_LoadMapCamera; each live particle (+3) moved by
// its velocity (+0xC.. += +0x1C..), its angle +6 the screen angle from the
// record's point to its goal (+0x34.. to +0xC..), its life +5 down - at 0, or
// its x at or past extra object 0's x - 0x10000, dead (+3 0) - and drawn
// (EffectKind88_DrawBurst(its point, its size +8 / +0xA, +6, 0x80, 0)). al 1
// when any was live, else 0.
extern "C" unsigned char __cdecl EffectKind88_MoveParticles(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kParticleCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kParticleStride * i);
        if (r[3] == 0) continue;
        SetUL(r + 0xC, UL(r + 0xC) + UL(r + 0x1C));
        SetUL(r + 0x10, UL(r + 0x10) + UL(r + 0x20));
        any = 1;
        SetUL(r + 0x14, UL(r + 0x14) + UL(r + 0x24));
        unsigned char* const s = S();
        const int angle = SH_CALL(EffectKind88_ScreenAngle)(reinterpret_cast<const long*>(s + 0x34),
                                                            reinterpret_cast<const long*>(s + 0xC));
        SetWord(r + 6, static_cast<U>(angle));
        r[5] = static_cast<unsigned char>(r[5] - 1);
        if (r[5] == 0 || S32(UL(r + 0xC)) >= S32(static_cast<std::uint32_t>(Sprite_ObjectsExtra[0].x) + 0xFFFF0000u)) r[3] = 0;
        SH_CALL(EffectKind88_DrawBurst)(reinterpret_cast<const long*>(r + 0xC), Word(r + 8), Word(r + 0xA), Word(r + 6), 0x80,
                                        0);
    }
    return any;
}

// original 0x489890 (point, w, h, turn - low words -, centre, rim - bytes): a
// draw mode (page (0x3C0, 0), dtd 1) linked at the point (dy 0); the point
// projected and its size EffectGte_ProjectSize(point, {w, h}), each half up
// Frame_Counter & 1; then sixteen POLY_G3s at the cursor, semi-transparent,
// from the projected centre (its depth all three corners') to two points of
// an ellipse of that size at 0x100 * k and 0x100 * (k + 1) turned by `turn`
// ((cos, sin) * radius >> 12 and the rotation >> 12, the 32 bits wrapping),
// the centre `centre` grey and the rim `rim`; each linked at the point (dy 0,
// 0x34).
extern "C" void __cdecl EffectKind88_DrawBurst(const long* point, unsigned w, unsigned h, unsigned turn, unsigned centre,
                                               unsigned rim) {
    DrawMode(1, 0x3C0, 0, 1);
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]), 0, 0xC);
    float l[3];
    SH_CALL(EffectGte_ProjectPoint)(point, l);
    short size[2] = {static_cast<short>(w), static_cast<short>(h)};
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(point, size, out);
    const unsigned short odd = static_cast<unsigned short>(Frame_Counter & 1u);
    out[0] = static_cast<short>(static_cast<unsigned short>(out[0]) + odd);
    out[1] = static_cast<short>(static_cast<unsigned short>(out[1]) + odd);
    const int t = static_cast<int>(turn & 0xFFFFu);
    U around = 0;
    U e = 0;
    for (int n = 0x10; n != 0; --n) {
        unsigned char* const q = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(q);
        SH_CALL(Gpu_SetSemiTrans)(q, 1);
        std::memcpy(q + 8, &l[0], 4);
        std::memcpy(q + 0xC, &l[1], 4);
        std::memcpy(q + 0x30, &l[2], 4);
        std::memcpy(q + 0x20, &l[2], 4);
        std::memcpy(q + 0x10, &l[2], 4);
        // the first rim point, at e
        const int c0 = SH_CALL(Math_Cos)(static_cast<int>(e));
        const U rx0 = Sar(Mul(static_cast<U>(c0), static_cast<short>(out[0])), 12);
        const int s0 = SH_CALL(Math_Sin)(static_cast<int>(e));
        const std::int32_t ry = static_cast<short>(Sar(Mul(static_cast<U>(s0), static_cast<short>(out[1])), 12));
        const std::int32_t rx = static_cast<short>(rx0);
        const U cx = Mul(static_cast<U>(SH_CALL(Math_Cos)(t)), rx);
        const U sy = Mul(static_cast<U>(SH_CALL(Math_Sin)(t)), ry);
        FildAdd(S32(Sar(cx - sy, 12)), &l[0], q + 0x18);
        const U sx = Mul(static_cast<U>(SH_CALL(Math_Sin)(t)), rx);
        const U cy = Mul(static_cast<U>(SH_CALL(Math_Cos)(t)), ry);
        around += 0x100;
        e = around & 0xFFFFu;
        FildAdd(S32(Sar(sx + cy, 12)), &l[1], q + 0x1C);
        // the second, at e + 0x100
        const int c1 = SH_CALL(Math_Cos)(static_cast<int>(e));
        const U rx1w = Sar(Mul(static_cast<U>(c1), static_cast<short>(out[0])), 12);
        const int s1 = SH_CALL(Math_Sin)(static_cast<int>(e));
        const std::int32_t ry1 = static_cast<short>(Sar(Mul(static_cast<U>(s1), static_cast<short>(out[1])), 12));
        const std::int32_t rx1 = static_cast<short>(rx1w);
        const U cx1 = Mul(static_cast<U>(SH_CALL(Math_Cos)(t)), rx1);
        const U sy1 = Mul(static_cast<U>(SH_CALL(Math_Sin)(t)), ry1);
        FildAdd(S32(Sar(cx1 - sy1, 12)), &l[0], q + 0x28);
        const U sx1 = Mul(static_cast<U>(SH_CALL(Math_Sin)(t)), rx1);
        const U cy1 = Mul(static_cast<U>(SH_CALL(Math_Cos)(t)), ry1);
        FildAdd(S32(Sar(sx1 + cy1, 12)), &l[1], q + 0x2C);
        q[4] = q[5] = q[6] = static_cast<unsigned char>(centre);
        q[0x14] = q[0x15] = q[0x16] = static_cast<unsigned char>(rim);
        q[0x24] = q[0x25] = q[0x26] = static_cast<unsigned char>(rim);
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]), 0, 0x34);
    }
}

// original 0x489AF0 (from, to: points of three s32): both projected; the
// screen angle Math_Ratan2(to.y - from.y, to.x - from.x), the differences
// taken in the FPU as floats. eax its answer (the callers keep ax).
extern "C" int __cdecl EffectKind88_ScreenAngle(const long* from, const long* to) {
    float a[3], b[3];
    SH_CALL(EffectGte_ProjectPoint)(from, a);
    SH_CALL(EffectGte_ProjectPoint)(to, b);
    float dx, dy;
    FldSub(&b[0], &a[0], &dx);
    FldSub(&b[1], &a[1], &dy);
    return SH_CALL(Math_Ratan2)(dy, dx);
}

// original 0x489B40 (a particle of 0x2C): live (+3 1), +4 0, at the record's
// point (+0xC.. = +0x34..), its velocity a 32nd of the way to the record's
// goal (+0x1C.. = (+0xC.. - +0x34..) sar 5), its size (0x40, 0x20) (+8 /
// +0xA), its life +5 0x20, its angle +6 EffectKind88_ScreenAngle(point, goal).
extern "C" void __cdecl EffectKind88_InitParticle(unsigned char* particle) {
    particle[3] = 1;
    particle[4] = 0;
    SetUL(particle + 0xC, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetUL(particle + 0x10, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    SetUL(particle + 0x14, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height));
    unsigned char* s = S();
    SetUL(particle + 0x1C, Sar(rv::As<records::EffectWork>(s)->work_0c - static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), 5));
    s = S();
    SetUL(particle + 0x20, Sar(rv::As<records::EffectWork>(s)->work_10 - static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 5));
    s = S();
    SetUL(particle + 0x24, Sar(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y) - static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height), 5));
    SetWord(particle + 8, 0x40);
    SetWord(particle + 0xA, 0x20);
    particle[5] = 0x20;
    s = S();
    const int angle = SH_CALL(EffectKind88_ScreenAngle)(reinterpret_cast<const long*>(s + 0x34),
                                                        reinterpret_cast<const long*>(s + 0xC));
    SetWord(particle + 6, static_cast<U>(angle));
}

// original 0x489BE0 (Effect_KindHandlers[0x89], hidden after 0x489B40):
// jmp [EffectKind89_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind89_Run(void) {
    Dispatch("EffectKind89_Run", AddressOf(EffectKind89_States), EffectKind89_States_count);
}

// original 0x489C00 (state 0): each party member below Field_MemberCount (read
// again after each) made Sprite_Current, pose +0x29 3, Sprite_UpdateScreen;
// Sprite_Current put back; the tile's colour +0x5D..+0x5F 0x80 (+0x5F on the
// record kept, the others read afresh); +1 = 1.
extern "C" void __cdecl EffectKind89_Start(void) {
    unsigned char* const saved = Sprite_Current;
    if (Field_MemberCount != 0) {
        unsigned char b = 0;
        do {
            unsigned char* const m = Member(b, "EffectKind89_Start");
            Sprite_Current = m;
            m[0x29] = 3;
            SH_CALL(Sprite_UpdateScreen)();
            b = static_cast<unsigned char>(b + 1);
        } while (b < Field_MemberCount);
    }
    Sprite_Current = saved;
    saved[0x5F] = 0x80;
    S()[0x5E] = 0x80;
    S()[0x5D] = 0x80;
    S()[1] = 1;
}

// original 0x489C80 (state 1): the tile (a tail jump).
extern "C" void __cdecl EffectKind89_Hold(void) { SH_CALL(EffectKind89_DrawTint)(); }

// original 0x489C90 (state 2): +0x5D above 0 - the colour down 2 each; else
// the chapter's step byte up and +1 = 3. Then the tile (a tail jump).
extern "C" void __cdecl EffectKind89_Fade(void) {
    unsigned char* const s = S();
    if (s[0x5D] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 0xFE);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 0xFE);
    } else {
        MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 1);
        s[1] = 3;
    }
    SH_CALL(EffectKind89_DrawTint)();
}

// original 0x489CD0 (kinds 0x89, 0x9F and 0xA4 draw it): a draw mode (page
// (0x3C0, 0), abr 2, dtd 1) committed in slot 3; a TILE at the cursor, (0, 0)
// 320 x 240 - DIV-0041's fill at 0x489D47: (Widescreen_FillX(), 0)
// Widescreen_FillWidth() x 240, Capcom's floats while the fills are unarmed or
// the picture narrow -, of the record's +0x5D..+0x5F, semi-transparent,
// untextured-shaded off, committed (3, 0x1C).
extern "C" void __cdecl EffectKind89_DrawTint(void) {
    DrawMode(2, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const t = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(t);
    t[4] = S()[0x5D];
    t[5] = S()[0x5E];
    t[6] = S()[0x5F];
    SetUL(t + 8, std::bit_cast<std::uint32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    SetUL(t + 0xC, 0);
    SetUL(t + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetUL(t + 0x18, 0x43700000u);   // 240.0f
    SH_CALL(Gpu_SetSemiTrans)(t, 1);
    SH_CALL(Gpu_SetShadeTex)(t, 0);
    SH_CALL(Gfx_CommitPrim)(3, 0x1C);
}

void EffectKind88_Inject() {
    BOF3_INJECT(EffectKind88_Run);
    BOF3_INJECT(EffectKind88_Start);
    BOF3_INJECT(EffectKind88_Emit);
    BOF3_INJECT(EffectKind88_EmitOn);
    BOF3_INJECT(EffectKind88_Fade);
    BOF3_INJECT(EffectKind88_ClearParticles);
    BOF3_INJECT(EffectKind88_FindParticle);
    BOF3_INJECT(EffectKind88_MoveParticles);
    BOF3_INJECT(EffectKind88_DrawBurst);
    BOF3_INJECT(EffectKind88_ScreenAngle);
    BOF3_INJECT(EffectKind88_InitParticle);
    BOF3_INJECT(EffectKind89_Run);
    BOF3_INJECT(EffectKind89_Start);
    BOF3_INJECT(EffectKind89_Hold);
    BOF3_INJECT(EffectKind89_Fade);
    BOF3_INJECT(EffectKind89_DrawTint);
}

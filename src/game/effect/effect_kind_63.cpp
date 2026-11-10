// Moved here by tools/move_funcs.py from effect_3b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_63.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

unsigned char& B(U a) { return *At(a); }

U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v & 0xFFFFu))); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul (32 bits kept)
U Mul(int a, U b) { return static_cast<U>(a) * b; }

// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(U v, const void* c, void* o) {
    const std::int32_t i = static_cast<std::int32_t>(v);
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(i) : "st", "memory");
}

// `fld dword [a]; fstp dword [o]` (a quiet copy through the FPU)
void Fld(const void* a, void* o) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(o), "r"(a) : "st", "memory");
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
    return rv::Bytes(&ObjTrio[m]);
}

void ScreenTint() { SH_AT(void (__cdecl*)(), at::kScreenTint.v)(); }

// The point +0x34 projected into the floats +0x74.. (EffectGte_ProjectPoint).
void ProjectHere() {
    unsigned char* const s = S();
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(s + 0x34), reinterpret_cast<float*>(s + 0x74));
}

}  // namespace

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
    rv::Current<records::EffectObject>()->x = 0x240000;
    rv::Current<records::EffectObject>()->z = 0x418000;
    const long h = SH_CALL(AreaMap_Elevation)(rv::Current<records::EffectObject>()->x, rv::Current<records::EffectObject>()->z);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<U>(h));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18, 0);
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
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) + 5);
    s[0x5C] = static_cast<unsigned char>(s[0x5C] + 2);
    if (static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18)) > 0x190) {
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
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) - 10);
    s[0x5C] = static_cast<unsigned char>(s[0x5C] - 2);
    if (static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18)) > 0) return;
    B(::bof3::Img(at::kCounter.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter.v)) + 1);
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4825D0 (state 4): with the counter 0x903848 at 0x2B, +9 = 0x10 and
// +1 = 5; then the tint and (a tail jump) the sprites refreshed.
extern "C" void __cdecl EffectKind63_WaitCue(void) {
    if (B(::bof3::Img(at::kCounter.v)) == at::kCounterCue) {
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
    unsigned char* o = rv::Bytes(Sprite_Objects);
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

void EffectKind63_Inject() {
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
}

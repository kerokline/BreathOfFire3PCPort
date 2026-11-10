// Moved here by tools/move_funcs.py from effect_5a.cpp, effect_5b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_0d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_18_sub_0d_callees.h"
#include "game/effect_5a_callees.h"
#include "game/effect_5b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_18_sub_0d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

unsigned char* S() { return Sprite_Current; }

unsigned char T(U a) { return *At(a); }

std::int32_t TS(U a) { return static_cast<signed char>(*At(a)); }

U TL(U a) { return static_cast<U>(Long(At(a))); }

std::int32_t Sar(U v, unsigned n) { return static_cast<std::int32_t>(v) >> n; }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, bytes or a null.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5a.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(TL(table + 4 * sub)))();
}

// Prim_VertexScratch's four SVECTORs (x, y, z words at +0, +2, +4; the pads
// +6 left as they are).
// kV: now Prim_VertexScratch (symbols.toml).
void Vertex(unsigned k, U x, U y, U z) {
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k), x & 0xFFFF);
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k + 2), y & 0xFFFF);
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k + 4), z & 0xFFFF);
}

// Gte_RotTransPers4 of the four into the POLY_FT4's corners (the tenth word
// Capcom pushes, a second local, is read by no Gte_RotTransPers4 of ours),
// then Gte_PrimDepths4_10.
void Project4(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 8)),
                               reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x10)), reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x18)),
                               reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                               reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

// A POLY_FT4 begun at Gfx_PacketNext: Gpu_SetPolyFT4, Gpu_SetShadeTex(0).
unsigned char* BeginQuad() {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    return p;
}

// The GTE matrix pushed, then a MATRIX on the stack: its translation
// Gte_RotTrans(point) (+0x14), its rotation Gte_RotMatrix(angles), multiplied
// by Camera_Matrix (Gte_MulMatrix0(camera, m, m)), set as the rotation and
// the translation. Capcom's third word to Gte_RotTrans (a flag out) is read by
// no Gte_RotTrans of ours.
void PushMatrix(const short* point, const short* angles) {
    SH_CALL(Gte_PushMatrix)();
    alignas(4) unsigned char m[0x20];
    SH_CALL(Gte_RotTrans)(point, reinterpret_cast<long*>(m + 0x14));
    SH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(m));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(m), reinterpret_cast<short*>(m));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(m));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(m));
}

// (v << 12) / 256 as the original computes it (cdq; and edx, 0xFF; add; sar 8).
U Div256(U v) {
    const std::int32_t x = static_cast<std::int32_t>(v << 12);
    return static_cast<U>((x + ((x >> 31) & 0xFF)) >> 8);
}

using I = std::int32_t;

using move_script::Word;

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

unsigned char& B(U a) { return *At(a); }

// Prim_VertexScratch's four SVECTORs (0x9037A0, A8, B0, B8): word k of vertex v.
// kScratch: now Prim_VertexScratch (symbols.toml).
U VW(unsigned v, unsigned k) { return Word(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v + 2 * k)); }

I VS(unsigned v, unsigned k) { return static_cast<std::int16_t>(VW(v, k)); }

void SetV(unsigned v, unsigned k, U value) { SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v + 2 * k), value); }

const short* VertexEffect5b(unsigned v) { return reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v)); }

// cdq; xor eax, edx; sub eax, edx (INT_MIN stays itself)
I Abs(I v) {
    const U sign = static_cast<U>(v >> 31);
    return static_cast<I>((static_cast<U>(v) ^ sign) - sign);
}

// cdq; sub eax, edx; sar eax, 1 on a whole dword
I Halve(I v) { return static_cast<I>(static_cast<U>(v) - static_cast<U>(v >> 31)) >> 1; }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, a data dword or a null.
void DispatchEffect5b(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// One of the image's byte arrays by an index the record supplies (unchecked by
// the original, which reads the next array or table past its extent).
unsigned Byte(const char* who, U array, unsigned extent, U index) {
    if (index >= extent)
        bof3::Fatal("%s: index %d past the %u bytes of 0x%X - the original reads what follows "
                    "(docs/effect_5b.md section 7)",
                    who, static_cast<int>(index), extent, (unsigned)::bof3::VaOf(array));
    return B(array + index);
}

// fld dword [p]; call _ftol
U FldFtol(const void* p) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[p])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [p] "r"(p)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// fild dword [i]; fadd dword [p]; call _ftol (the original's `fild; fld st(0);
// fadd [p]` and `fild; fadd [p]`)
U FiAddFtol(I i, const void* p) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "fildl %[i]\n\t"
        "fadds (%[p])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [i] "m"(i), [p] "r"(p)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// fld dword [p]; fisub dword [i]; call _ftol (the original's `fld [p]; fsub
// st(1)` over a fild'ed i, and `fild i; fsubr [p]`: p - i, one rounding)
U FldFiSubFtol(const void* p, I i) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[p])\n\t"
        "fisubl %[i]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [i] "m"(i), [p] "r"(p)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// fld dword [from]; fst dword [a]; fstp dword [b]: a copy through the FPU (a
// signalling NaN comes out quiet, as in the original)
void FpuCopy2(const void* from, void* a, void* b) {
    __asm__ volatile(
        "flds (%[from])\n\t"
        "fsts (%[a])\n\t"
        "fstps (%[b])\n\t"
        :
        : [from] "r"(from), [a] "r"(a), [b] "r"(b)
        : "st", "memory");
}

// fild dword (a sign-extended word); fstp dword
void StoreFloat(unsigned char* to, I v) {
    const float f = static_cast<float>(v);
    std::memcpy(to, &f, 4);
}

float F(U a) {
    float f;
    std::memcpy(&f, At(a), 4);
    return f;
}

unsigned char* Prim() { return Gfx_PacketNext; }

unsigned char* Flags() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The four-vertex projection every quad here makes of Prim_VertexScratch into a
// primitive whose vertices are `stride` apart from +8 (0x10 textured, 0xC flat).
void Project4Effect5b(unsigned char* prim, unsigned stride) {
    long p;
    SH_CALL(Gte_RotTransPers4)(VertexEffect5b(0), VertexEffect5b(1), VertexEffect5b(2), VertexEffect5b(3), reinterpret_cast<float*>(prim + 8),
                               reinterpret_cast<float*>(prim + 8 + stride), reinterpret_cast<float*>(prim + 8 + 2 * stride),
                               reinterpret_cast<float*>(prim + 8 + 3 * stride), &p);
}

// A VRAM move of an 8 x 0x78 column: the rect at (frame + 1) * 8 + `from`,
// 0x160, onto (`to`, 0x160); committed to slot 6.
void MoveColumn(U from, U to) {
    unsigned char rect[8];
    SetWord(rect, from);
    SetWord(rect + 2, 0x160);
    SetWord(rect + 4, 8);
    SetWord(rect + 6, 0x78);
    SH_CALL(Gpu_SetDrawMove)(Prim(), rect, to, 0x160);
    SH_CALL(Gfx_CommitPrim)(6, 0x18);
}

// +9 up one; when past `last`, +9 0 (when `clear`) and +2 to `next` (or up one
// when next is negative).
void Step(unsigned last, bool clear, int next) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] <= last) return;
    if (clear) S()[9] = 0;
    if (next < 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    else S()[2] = static_cast<unsigned char>(next);
}

}  // namespace

// original 0x4FEF50 (EffectKind18_States[10]).
extern "C" void __cdecl EffectKind18_0A_Run(void) {
    Dispatch("EffectKind18_0A_Run", AddressOf(EffectKind18_0A_States), EffectKind18_0A_States_count);
}

// original 0x4FEF70 (state 0): the doors shut; Cond_ByteFE not 0: +2 up one, +9 0.
extern "C" void __cdecl EffectKind18_0A_Shut(void) {
    SH_CALL(EffectKind18_0A_DrawDoor)(0, 0x858000, 0);
    SH_CALL(EffectKind18_0A_DrawDoor)(0x800, 0x878000, 1);
    if (Cond_ByteFE == 0) return;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    S()[9] = 0;
}

// original 0x4FEFC0 (state 1): the doors swung by +9 (variant + 0x10 past
// 0x30); +9 up; past 0x40: +2 up one, +9 0.
extern "C" void __cdecl EffectKind18_0A_Swing(void) {
    U n = S()[9];
    SH_CALL(EffectKind18_0A_DrawDoor)(Div256(n), 0x858000, n > 0x30 ? 0x10u : 0u);
    n = S()[9];
    SH_CALL(EffectKind18_0A_DrawDoor)(Div256(n + 0x80), 0x878000, (n > 0x30 ? 0x10u : 0u) + 1);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    if (s[9] > 0x40) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        S()[9] = 0;
    }
}

// original 0x4FF060 (state 2): the doors held open; +9 up; past 4: +2 up one, +9 0.
extern "C" void __cdecl EffectKind18_0A_Hold(void) {
    SH_CALL(EffectKind18_0A_DrawDoor)(0x400, 0x858000, 0x10);
    SH_CALL(EffectKind18_0A_DrawDoor)(0xC00, 0x878000, 0x11);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    if (s[9] > 4) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        S()[9] = 0;
    }
}

// original 0x4FF0B0 (state 3): the doors slid apart by +9; +9 up; past 0x10: +2
// up one, +9 0.
extern "C" void __cdecl EffectKind18_0A_Slide(void) {
    SH_CALL(EffectKind18_0A_DrawDoor)(0x400, (0x858u - S()[9]) << 12, 0x10);
    SH_CALL(EffectKind18_0A_DrawDoor)(0xC00, (S()[9] + 0x878u) << 12, 0x11);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    if (s[9] > 0x10) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        S()[9] = 0;
    }
}

// original 0x4FF120 (state 4): the doors open.
extern "C" void __cdecl EffectKind18_0A_Open(void) {
    SH_CALL(EffectKind18_0A_DrawDoor)(0x400, 0x848000, 0x10);
    SH_CALL(EffectKind18_0A_DrawDoor)(0xC00, 0x888000, 0x11);
}

// original 0x4FF150 (cdecl, three words): a door at x turned by the angle about
// z: eight parts, each drawn when its texture (by the variant's bit 0) is not
// 0, linked at (x, 0xB8000) with the variant's dy.
extern "C" void __cdecl EffectKind18_0A_DrawDoor(unsigned angle, unsigned long x, unsigned variant) {
    const std::int32_t row = static_cast<std::int32_t>(variant) >> 4;
    if (row < 0 || row + 2 * static_cast<std::int32_t>(variant & 1) > 3)
        bof3::Fatal("EffectKind18_0A_DrawDoor: variant 0x%X reads 0x65DF2C past its 32 bytes - the original reads what "
                    "follows (docs/effect_5a.md section 7)",
                    variant);
    short point[4];
    point[0] = static_cast<short>(Sar(static_cast<U>(x), 9) - 0x4000);
    point[1] = static_cast<short>(0xC5C0);
    point[2] = static_cast<short>(0xFF80);
    short angles[4];
    angles[0] = 0;
    angles[1] = 0;
    angles[2] = static_cast<short>(angle);
    PushMatrix(point, angles);
    const U side = variant & 1;
    for (U i = 0; i < 8; ++i) {
        unsigned char* const p = BeginQuad();
        const U texture_at = ::bof3::Img(at::kDoorTextures.v) + 8 * i + 4 * side;
        if (TL(texture_at) == 0) continue;
        const U shape = ::bof3::Img(at::kDoorShapes.v) + 12 * i;
        for (unsigned k = 0; k < 4; ++k) {
            const std::int32_t d = TS(shape + 3 * k + 2);
            Vertex(k, static_cast<U>(TS(shape + 3 * k) << 7), static_cast<U>(TS(shape + 3 * k + 1) << 7),
                   static_cast<U>(-320 * d));
        }
        Project4(p);
        const U texture = TL(texture_at);
        const U page = (static_cast<U>(Sar(angle, 5)) & 0x38) + 0x50;
        const U keep = (texture & 0xF80000) != 0 ? 0 : 1;
        SH_CALL(Prim_SetTexture)(((page * keep) << 16) | texture, p, 1);
        const U dy = T(::bof3::Img(at::kDoorDy.v) + static_cast<U>(row) + 2 * side + 4 * i);
        SH_CALL(MapView_LinkPrimAt)(x, 0xB8000, static_cast<int>(dy), 0x48);
    }
    SH_CALL(Gte_PopMatrix)();
}

// original 0x4FF320 (EffectKind18_States[13], hidden in E5A's 0x4FF150): jmp
// [EffectKind18Sub0D_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0D_Run(void) {
    DispatchEffect5b("EffectKind18Sub0D_Run", AddressOf(EffectKind18Sub0D_States), EffectKind18Sub0D_States_count);
}

// original 0x4FF340 (state 0): +9 0; column 0 with story flag 0xD set: frame 3
// moved and +2 3; else the column's frame 0 and +2 1.
extern "C" void __cdecl EffectKind18Sub0D_Start(void) {
    S()[9] = 0;
    if (rv::As<records::EffectObject>(S())->x_whole == 0 && SH_CALL(Flags_Test)(Flags(), 0xD)) {
        SH_CALL(EffectKind18Sub0D_MoveFrame)(0, 3);
        S()[2] = 3;
        return;
    }
    SH_CALL(EffectKind18Sub0D_MoveFrame)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(S())->x_whole)), 0);
    S()[2] = 1;
}

// original 0x4FF3A0 (state 1): column 0 with story flag 0xD, or another column
// with area 48's count not 0: sound 0x20D, +9 0, +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub0D_Wait(void) {
    // +0x36 read again after the test, as the original reads Sprite_Current again
    const bool lit = (rv::As<records::EffectObject>(S())->x_whole == 0 && SH_CALL(Flags_Test)(Flags(), 0xD) != 0) ||
                     (rv::As<records::EffectObject>(S())->x_whole != 0 && AreaW1c_Count48 != 0);
    if (!lit) return;
    SH_CALL(Sound_PlayEffect)(0x20D);
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FF400 (state 2): the frame _OpenFrames[+9 >> 1] moved; +9 up, past
// 4: +9 0 and +2 up; the glow.
extern "C" void __cdecl EffectKind18Sub0D_Open(void) {
    unsigned char* const s = S();
    const unsigned frame = Byte("EffectKind18Sub0D_Open", ::bof3::Img(at::kSub0DOpenFrames.v), at::kSub0DArray, s[9] >> 1);
    SH_CALL(EffectKind18Sub0D_MoveFrame)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), frame);
    Step(4, true, -1);
    SH_CALL(EffectKind18Sub0D_DrawGlow)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(S())->x_whole)));
}

// original 0x4FF460 (state 3): the light stays while its condition holds
// (column 0: story flag 0xD; another: area 48's count not 0), else +9 0 and +2
// up; +9 up, past 0x40: +9 0 and, on an odd Rand, +2 5; the glow.
extern "C" void __cdecl EffectKind18Sub0D_Idle(void) {
    bool off;
    if (rv::As<records::EffectObject>(S())->x_whole == 0) {
        const unsigned char set = SH_CALL(Flags_Test)(Flags(), 0xD);
        if (set == 0) off = true;
        else if (rv::As<records::EffectObject>(S())->x_whole == 0) off = false;
        else off = AreaW1c_Count48 == 0;
    } else {
        off = AreaW1c_Count48 == 0;
    }
    if (off) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] > 0x40) {
        S()[9] = 0;
        if (SH_CALL(Rand)() & 1) S()[2] = 5;
    }
    SH_CALL(EffectKind18Sub0D_DrawGlow)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(S())->x_whole)));
}

// original 0x4FF4F0 (state 4): the frame _ShutFrames[+9 / 3] moved; +9 up, past
// 3: +2 1. No glow.
extern "C" void __cdecl EffectKind18Sub0D_Shut(void) {
    unsigned char* const s = S();
    const unsigned frame = Byte("EffectKind18Sub0D_Shut", ::bof3::Img(at::kSub0DShutFrames.v), at::kSub0DArray, s[9] / 3u);
    SH_CALL(EffectKind18Sub0D_MoveFrame)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), frame);
    Step(3, false, 1);
}

// original 0x4FF540 (state 5): the frame _FlashFrames[+9 >> 1] moved; +9 up,
// past 6: +9 0 and +2 3; the glow.
extern "C" void __cdecl EffectKind18Sub0D_Flash(void) {
    unsigned char* const s = S();
    const unsigned frame = Byte("EffectKind18Sub0D_Flash", ::bof3::Img(at::kSub0DFlashFrames.v), at::kSub0DArray, s[9] >> 1);
    SH_CALL(EffectKind18Sub0D_MoveFrame)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), frame);
    Step(6, true, 3);
    SH_CALL(EffectKind18Sub0D_DrawGlow)(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(S())->x_whole)));
}

// original 0x4FF5A0 (column, frame): Gpu_SetDrawMove of the rect ((frame + 1) *
// 8 + column * 64 + 0x240, 0x160, 8, 0x78) onto (column * 64 + 0x240, 0x160),
// committed to slot 6 (0x18 bytes). Each product through the original's
// halving of a doubled value (exact).
extern "C" void __cdecl EffectKind18Sub0D_MoveFrame(int column, unsigned frame) {
    const U left = static_cast<U>(Halve(static_cast<I>(static_cast<U>(column) << 7)));
    const U offset = static_cast<U>(Halve(static_cast<I>((frame + 1) << 4)));
    MoveColumn(offset + left + 0x240, left + 0x240);
}

// original 0x4FF610 (column): the column's point (_GlowX / Z - 0x80 cells, y
// -_GlowY * 8) through Gte_RotTransPers into MapView_ScreenXY; culled unless
// -60 <= x <= 380, -150 <= y <= 300 (.rdata, fcomp) and the depth not 0. Then
// a draw mode (tpage 0xB5, dtd 1) linked at the cell; a diamond about the
// point in Prim_VertexScratch, its half-widths _GlowSize * 1125 / depth across
// and (|sin(Camera_Angles) * size >> 13| + size) * 1125 / depth up and down
// (x87, _ftol); four semi-transparent POLY_G3 fans from the centre (colour
// 0x50 on column 0's red and blue, (area 48's count + 1) * column << 4 on red
// and green; the rim black), each linked (0x34); a closing draw mode (dtd 0)
// linked.
extern "C" void __cdecl EffectKind18Sub0D_DrawGlow(int column) {
    const char* const who = "EffectKind18Sub0D_DrawGlow";
    const U i = static_cast<U>(column);
    const unsigned gx = Byte(who, ::bof3::Img(at::kSub0DGlowX.v), at::kSub0DArray, i);
    const unsigned gz = B(::bof3::Img(at::kSub0DGlowZ.v) + i);
    const unsigned gy = B(::bof3::Img(at::kSub0DGlowY.v) + i);
    SetV(0, 0, (gx - 0x80u) << 7);
    SetV(0, 1, (gz - 0x80u) << 7);
    SetV(0, 2, (0u - gy) << 3);
    const U x = static_cast<U>(gx) << 16;
    const U z = static_cast<U>(gz) << 16;
    long p;
    const long depth = SH_CALL(Gte_RotTransPers)(VertexEffect5b(0), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &p);
    const unsigned char* const sx = At(::bof3::Img(0x903820));
    const unsigned char* const sy = At(::bof3::Img(0x903824));
    const float fill = static_cast<float>(Widescreen_Fill());   // DIV-0041 (UC2): x out by the columns
    if (!(F(::bof3::Img(0x903820)) >= F(at::kCullLeft) - fill)) return;
    if (F(::bof3::Img(0x903820)) > F(at::kCullRight) + fill) return;
    if (!(F(::bof3::Img(0x903824)) >= F(at::kCullTop))) return;
    if (F(::bof3::Img(0x903824)) > F(at::kCullBottom)) return;
    if (depth == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, 0xB5, 0);
    SH_CALL(MapView_LinkPrimAt)(x, z, 1, 0xC);
    const U cx = FldFtol(sx);
    SetV(2, 0, cx);
    SetV(0, 0, cx);
    const I d = static_cast<I>(depth);
    const I across = static_cast<I>(B(::bof3::Img(at::kSub0DGlowSize.v) + i) * 1125u) / d;
    SetV(1, 0, FiAddFtol(across, sx));
    SetV(3, 0, FldFiSubFtol(sx, across));
    const U cy = FldFtol(sy);
    SetV(3, 1, cy);
    SetV(1, 1, cy);
    {
        const I angle = Camera_Angles[0];
        const U size = B(::bof3::Img(at::kSub0DGlowSize.v) + i);
        const I sine = SH_CALL(Math_Sin)(angle);
        const U reach = static_cast<U>(Abs(static_cast<I>(static_cast<U>(sine) * size) >> 13)) + size;
        const I down = static_cast<I>(reach * 1125u) / d;
        SetV(2, 1, FiAddFtol(down, sy));
    }
    {
        const I angle = Camera_Angles[0];
        const U size = B(::bof3::Img(at::kSub0DGlowSize.v) + i);
        const I sine = SH_CALL(Math_Sin)(angle);
        const U reach = static_cast<U>(Abs(static_cast<I>(static_cast<U>(sine) * size) >> 13)) + size;
        const I up = static_cast<I>(reach * 1125u) / d;
        SetV(0, 1, FldFiSubFtol(sy, up));
    }
    const unsigned char centre = i == 0 ? 0x50 : 0;
    for (unsigned k = 1; k < 5; ++k) {
        unsigned char* const prim = Prim();
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
        FpuCopy2(prim + 0x10, prim + 0x30, prim + 0x20);
        std::memcpy(prim + 8, sx, 4);
        std::memcpy(prim + 0xC, sy, 4);
        const unsigned from = k - 1, to = k & 3;
        StoreFloat(prim + 0x18, VS(from, 0));
        StoreFloat(prim + 0x1C, VS(from, 1));
        StoreFloat(prim + 0x28, VS(to, 0));
        StoreFloat(prim + 0x2C, VS(to, 1));
        const auto green = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(AreaW1c_Count48 + 1) * static_cast<unsigned char>(i)) << 4);
        prim[4] = static_cast<unsigned char>(green | centre);
        prim[6] = centre;
        prim[5] = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(AreaW1c_Count48 + 1) * static_cast<unsigned char>(i)) << 4);
        prim[0x26] = 0;
        prim[0x25] = 0;
        prim[0x24] = 0;
        prim[0x16] = 0;
        prim[0x15] = 0;
        prim[0x14] = 0;
        SH_CALL(MapView_LinkPrimAt)(x, z, 1, 0x34);
    }
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0xB5, 0);
    SH_CALL(MapView_LinkPrimAt)(x, z, 1, 0xC);
}

// original 0x4FF970 (EffectKind18_States[11], hidden in 0x4FF610): jmp
// [EffectKind18Sub0B_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0B_Run(void) {
    DispatchEffect5b("EffectKind18Sub0B_Run", AddressOf(EffectKind18Sub0B_States), EffectKind18Sub0B_States_count);
}

// original 0x4FF990 (state 0): story flag 0x11 set: sound 0x202, the height
// +0x3C -0x180, the colour +0xB 0x80, +9 0, +2 up and the tiles drawn at -0x180.
extern "C" void __cdecl EffectKind18Sub0B_Start(void) {
    if (!SH_CALL(Flags_Test)(Flags(), 0x11)) return;
    SH_CALL(Sound_PlayEffect)(0x202);
    rv::Current<records::EffectObject>()->height = 0xFFFFFE80u;
    S()[0xB] = 0x80;
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub0B_DrawTiles)(-0x180, 0x800000);
}

// original 0x4FF9F0 (state 1): the tiles at +0x3C (colour 0x80); +0x3C up 4;
// above -0x100: +2 up.
extern "C" void __cdecl EffectKind18Sub0B_Rise(void) {
    SH_CALL(EffectKind18Sub0B_DrawTiles)(rv::Current<records::EffectObject>()->height, 0x800000);
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    so->height = static_cast<std::uint32_t>(so->height) + 4;
    if (rv::Current<records::EffectObject>()->height > -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FFA30 (state 2): the tiles at +0x3C, colour (+0xB & 0xF8) << 16;
// +0x3C up 4, +0xB down 4; above -0x80: 0x9039F4 = 4 and a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind18Sub0B_Fade(void) {
    unsigned char* s = S();
    const U colour = static_cast<U>(s[0xB] & 0xF8u) << 16;
    SH_CALL(EffectKind18Sub0B_DrawTiles)(rv::As<records::EffectObject>(s)->height, colour);
    s = S();
    rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 4;
    S()[0xB] = static_cast<unsigned char>(S()[0xB] - 4);
    if (rv::Current<records::EffectObject>()->height <= -0x80) return;
    AreaW0c_TailState = 4;
    SH_CALL(Effect_Release)();
}

// original 0x4FFA90 (height, colour): the eighteen tiles of _Tiles (cell x, cell
// z, row dy): each a POLY_FT4 a cell square (x * 0x80 - 0x40C0 .. - 0x3FC0, the
// same in z, y the low word of height), Gte_RotTransPers4, Gte_PrimDepths4_10,
// Prim_SetTexture(colour | 0xB600B100, prim, 1), linked at the cell with its dy
// (0x48).
extern "C" void __cdecl EffectKind18Sub0B_DrawTiles(int height, unsigned long colour) {
    const U texture = static_cast<U>(colour) | 0xB600B100u;
    const U y = static_cast<U>(height);
    for (U t = ::bof3::Img(at::kSub0BTiles) + 1; static_cast<I>(t) < static_cast<I>(::bof3::Img(at::kSub0BTilesEnd)); t += 3) {
        unsigned char* const prim = Prim();
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        const U x = static_cast<U>(B(t - 1)) << 7;
        SetV(3, 2, y);
        SetV(2, 2, y);
        SetV(2, 0, x - 0x40C0);
        SetV(0, 0, x - 0x40C0);
        const U z = static_cast<U>(B(t)) << 7;
        SetV(3, 0, x - 0x3FC0);
        SetV(1, 0, x - 0x3FC0);
        SetV(3, 1, z - 0x3FC0);
        SetV(2, 1, z - 0x3FC0);
        SetV(1, 1, z - 0x40C0);
        SetV(0, 1, z - 0x40C0);
        SetV(1, 2, y);
        SetV(0, 2, y);
        Project4Effect5b(prim, 0x10);
        SH_CALL(Gte_PrimDepths4_10)(prim);
        SH_CALL(Prim_SetTexture)(texture, prim, 1);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(B(t - 1)) << 16, static_cast<U>(B(t)) << 16, B(t + 1), 0x48);
    }
}

void EffectKind18Sub0d_Inject() {
    BOF3_INJECT(EffectKind18_0A_Run);
    BOF3_INJECT(EffectKind18_0A_Shut);
    BOF3_INJECT(EffectKind18_0A_Swing);
    BOF3_INJECT(EffectKind18_0A_Hold);
    BOF3_INJECT(EffectKind18_0A_Slide);
    BOF3_INJECT(EffectKind18_0A_Open);
    BOF3_INJECT(EffectKind18_0A_DrawDoor);
    BOF3_INJECT(EffectKind18Sub0D_Run);
    BOF3_INJECT(EffectKind18Sub0D_Start);
    BOF3_INJECT(EffectKind18Sub0D_Wait);
    BOF3_INJECT(EffectKind18Sub0D_Open);
    BOF3_INJECT(EffectKind18Sub0D_Idle);
    BOF3_INJECT(EffectKind18Sub0D_Shut);
    BOF3_INJECT(EffectKind18Sub0D_Flash);
    BOF3_INJECT(EffectKind18Sub0D_MoveFrame);
    BOF3_INJECT(EffectKind18Sub0D_DrawGlow);
    BOF3_INJECT(EffectKind18Sub0B_Run);
    BOF3_INJECT(EffectKind18Sub0B_Start);
    BOF3_INJECT(EffectKind18Sub0B_Rise);
    BOF3_INJECT(EffectKind18Sub0B_Fade);
    BOF3_INJECT(EffectKind18Sub0B_DrawTiles);
}

// Round thirteen group E5E (docs/effect_5e.md): the 51 functions of
// analysis/round13_cut.tsv's group E5E, 0x506A10..0x508CBB, and three starts
// the cut does not list (sub-kind 0x23's shared draw 0x506AB0, the dispatchers
// 0x506BD0 and 0x507620 of EffectKind18_States 36 and 63), each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's handler jumps through
// EffectKind18_States by +1 (EffectKind18_Start sets +1 from the record's
// +0xB, the sub-kind), and each sub-kind here is a dispatcher by +2 through its
// own table (none bounded by a compare) and the states it names. What each
// sub-kind is, as far as the code says:
//
//   0x23 (35)  a textured panel at the record's map point: it waits for story
//              flag 0x28 and then slides 4 a frame until +0x30 reaches 0x100
//              (released) - or is released at once when the flag is already set
//   0x24 (36)  at the counter 0x903848 = 5 (story row 0x50 bit 4 set): four
//              textured quads round the record's point, three beats of 16
//              frames with sounds, seven shaded trails drawn away and four
//              drawn out, then a spin that speeds up (random jitter, the
//              counter 6 to 7) and fades; the counter raised twice
//   0x25 (37)  a lid of two leaves over a map cell (eight variants by +0x36):
//              it opens (+0x30 to 0x80) while the leader stands near, waits for
//              the leader to leave, closes; sounds 0x200 / 0x201 when no
//              message is up; a textured cap when the variant has one
//   0x26 (38)  the same with five variants of its own (the texture word in
//              +0x20, the height less +0x2E)
//   0x39 (57)  three textured quads in two halves that part (+0x30 to 0x80)
//              once story flag 0x5A is set, then hold with Cond_ByteFE 1
//   0x3F (63)  a scene on the counter's cues 0x26..0x3B: CLUT row 4 dimmed, a
//              sky gradient over the frame that warms and fades, a trail,
//              sub-kind 0x24's spin, and a white frame to end (Cond row 4 at
//              full, the leader's shade cleared, released)
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. The two
// full-frame fills (0x507BC0's white POLY_F4, 0x507CB0's gradient POLY_G4) are
// DIV-0041's listed sites 0x507BDC and 0x507CE3 (section 3c): they span
// Widescreen_FillX() .. Widescreen_FillX() + Widescreen_FillWidth(), which is the
// original's 0 .. 320 until Widescreen_ArmFills has run and whenever the
// picture is narrow. Otherwise no divergence: each is a faithful replacement.
// Where the original jumps through a state table past its end or indexes one of
// its .data tables past its entries, ours aborts with a message
// (docs/effect_5e.md section 6).
#include "game/effect_5e.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5e_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5e::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U W(const unsigned char* p) { return Word(p); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
// movsx of a .data byte, as the 32 bits the original holds
U SB(U a) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(*At(a)))); }
unsigned char& B(U a) { return *At(a); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// mov eax, magic; imul x; sar edx, shift; mov eax, edx; shr eax, 31; add edx,
// eax: MSVC's signed division by a constant.
U MulHi(U magic, U x) {
    const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(magic)) * static_cast<std::int32_t>(x);
    return static_cast<U>(static_cast<std::uint64_t>(p) >> 32);
}
U Div(U magic, U x, unsigned shift) {
    const U q = Sar(MulHi(magic, x), shift);
    return q + (q >> 31);
}
// cdq; xor eax, edx; sub eax, edx: |v| as the original's signed compare sees
// it (0x80000000 stays negative).
std::int32_t Abs(U v) {
    const U m = Sar(v, 31);
    return static_cast<std::int32_t>((v ^ m) - m);
}
unsigned char* Vtx() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
const short* Vector(U offset) { return reinterpret_cast<const short*>(Vtx() + offset); }
float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the originals' 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next sub-kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5e.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

// +2 up, Sprite_Current read afresh.
void Step() {
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}
// +9 up; answers whether it is now above `limit` (cmp byte [S + 9], limit;
// jbe: unsigned), Sprite_Current read again for the compare.
bool Count(unsigned limit) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    return s[9] > limit;
}
bool Cue(unsigned value) { return B(at::kCounter) == value; }

// Gte_RotTransPers4 of Prim_VertexScratch's four vectors (0x9037A0, A8, B0, B8)
// into the quad at p; the original's depth and flag are locals of its own.
void Project4(unsigned char* p) {
    long depth = 0;
    SH_CALL(Gte_RotTransPers4)(Vector(0), Vector(8), Vector(0x10), Vector(0x18), F(p + 8), F(p + 0x18), F(p + 0x28),
                               F(p + 0x38), &depth);
}
// A POLY_FT4 at the cursor, opaque shading (Gpu_SetShadeTex 0): answers it.
unsigned char* TexturedQuad() {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    return p;
}
// MapView_LinkPrimAt at the record's point (+0x34, +0x38), Sprite_Current read
// for it.
void LinkAtRecord(int dy, unsigned size) {
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), dy, size);
}

// The x87 as the original has it (the game's control word: round to nearest,
// 53 bits): `fild dword [j]; fld dword [src]; fadd dword [0x5C41B8]; fsub st,
// st(1); fstp dword [dst]; fstp st(0)` - the original keeps j on the stack for
// four of these; each is the same two roundings.
void Offset(const unsigned char* src, std::int32_t j, unsigned char* dst) {
    __asm__ volatile("fildl %3\n\tflds (%1)\n\tfadds (%2)\n\t.byte 0xD8, 0xE1\n\tfstps (%0)\n\tfstp %%st(0)"
                     :
                     : "r"(dst), "r"(src), "r"(At(at::kShadeConst)), "m"(j)
                     : "st", "st(1)", "memory");
}

}  // namespace

// ===========================================================================
// Sub-kind 0x23: EffectKind18_States[35] (0x6540F8), EffectKind18Sub23_States
// (three) by +2
// ===========================================================================

// original 0x506A10 (EffectKind18_States[35]): jmp [EffectKind18Sub23_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub23_Run(void) {
    Dispatch("EffectKind18Sub23_Run", AddressOf(EffectKind18Sub23_States), EffectKind18Sub23_States_count);
}

// original 0x506A30 (sub-state 0): +0x30 = 0; story flag 0x28 already set: a
// tail jump to Effect_Release; else +2 up and a tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub23_Start(void) {
    SetWord(S() + 0x30, 0);
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), at::kSub23Flag)) {
        SH_CALL(Effect_Release)();
        return;
    }
    Step();
    SH_CALL(EffectKind18Sub23_Draw)();
}

// original 0x506A60 (sub-state 1): once story flag 0x28 is set, +2 up; the draw.
extern "C" void __cdecl EffectKind18Sub23_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), at::kSub23Flag)) Step();
    SH_CALL(EffectKind18Sub23_Draw)();
}

// original 0x506A80 (sub-state 2): +0x30 up 4; at 0x100 or more (signed word)
// Effect_Release - and the draw all the same.
extern "C" void __cdecl EffectKind18Sub23_Slide(void) {
    unsigned char* s = S();
    SetWord(s + 0x30, W(s + 0x30) + 4);
    s = S();
    if (SW(s + 0x30) >= 0x100) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub23_Draw)();
}

// original 0x506AB0 (the tail the three sub-states jump to; inside 0x506A80's
// tool extent, its own frame and ret): a draw mode (tpage 0x95, dtd 0) at the
// cursor linked at the record's point (0xC bytes); a POLY_FT4 at the cursor
// (read after the link): x +0x30 - 0x37C0 and +0x30 - 0x36C0, y 0x16C0, z 0x140
// and 0x280 in Prim_VertexScratch, Gte_RotTransPers4, Gte_PrimDepths4_10,
// texture 0x2378013D, linked at the record's point (0x48 bytes).
extern "C" void __cdecl EffectKind18Sub23_Draw(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    LinkAtRecord(0, 0xC);
    unsigned char* const p = TexturedQuad();
    unsigned char* const v = Vtx();
    unsigned char* const s = S();
    const U near = W(s + 0x30) - 0x37C0u, far = W(s + 0x30) - 0x36C0u;
    SetWord(v + 0x10, near);
    SetWord(v + 0x0, near);
    SetWord(v + 0x18, far);
    SetWord(v + 0x8, far);
    SetWord(v + 0x1A, 0x16C0);
    SetWord(v + 0x12, 0x16C0);
    SetWord(v + 0xA, 0x16C0);
    SetWord(v + 0x2, 0x16C0);
    SetWord(v + 0xC, 0x140);
    SetWord(v + 0x4, 0x140);
    SetWord(v + 0x1C, 0x280);
    SetWord(v + 0x14, 0x280);
    Project4(p);
    SH_CALL(Gte_PrimDepths4_10)(p);
    SH_CALL(Prim_SetTexture)(0x2378013D, p, 1);
    LinkAtRecord(0, 0x48);
}

// ===========================================================================
// Sub-kind 0x24: EffectKind18_States[36] (0x6540FC), EffectKind18Sub24_States
// (eight) by +2
// ===========================================================================

namespace {

// The record placed: +0x34 / +0x38 the cell offsets +0xC / +0x10 plus dx / dz,
// shifted down 9, less 0x4000; the height +0x3C; the angle +0x32 0; the speed
// +0x14 0x53020; the level +0x30; +0x10 and +0xC 0 (Sprite_Current read for
// each, as the original does - no call between).
void Place(U dx, U dz, U height, U level) {
    unsigned char* const s = S();
    SetUL(s + 0x34, Sar(UL(s + 0xC) + dx, 9) - 0x4000u);
    SetUL(s + 0x38, Sar(UL(s + 0x10) + dz, 9) - 0x4000u);
    SetUL(s + 0x3C, height);
    SetWord(s + 0x32, 0);
    SetUL(s + 0x14, 0x53020);
    SetWord(s + 0x30, level);
    SetUL(s + 0x10, 0);
    SetUL(s + 0xC, 0);
}

// The speed's step: +0x14 / 10000 (0x68DB8BAD, sar 12).
U Speed(U v) { return Div(0x68DB8BAD, v, 12); }

// The angle +0x32 turned back by the speed, kept to 0xFFD (and eax, 0xFFD: bit
// 1 cleared too).
void Turn() {
    unsigned char* const s = S();
    SetWord(s + 0x32, (W(s + 0x32) - Speed(UL(s + 0x14))) & 0xFFDu);
}

// Each time bit 11 of the angle changes (and, for the fade, the level +0x30 is
// above 0x40): the sound byte +0xB = the angle's top bits + 1. +0x2E keeps the
// angle. Answers Sprite_Current as the original holds it.
unsigned char* Kick(unsigned char* s, bool fading) {
    const U angle = W(s + 0x32);
    if (((W(s + 0x2E) ^ angle) & 0x800) != 0 && (!fading || SW(s + 0x30) > 0x40)) {
        s[0xB] = static_cast<unsigned char>((S16(angle) >> 11) + 1);
        s = S();
    }
    SetWord(s + 0x2E, W(s + 0x32));
    return s;
}

// The jitter of the cell offsets: +0xC and +0x10 each Rand & mask (Sprite_Current
// read after each Rand).
void Jitter(U mask) {
    const U x = static_cast<U>(SH_CALL(Rand)());
    SetUL(S() + 0xC, x & mask);
    const U z = static_cast<U>(SH_CALL(Rand)());
    SetUL(S() + 0x10, z & mask);
}

// One of the trails' corners: the (x, z) byte pair k of kSub24Points, each
// times 3, << 16, plus 0x4B8000 / 0x568000, sar 9, less 0x4000.
U CornerX(unsigned k) { return Sar(((SB(at::kSub24Points + 2 * k) * 3u) << 16) + 0x4B8000u, 9) - 0x4000u; }
U CornerZ(unsigned k) { return Sar(((SB(at::kSub24Points + 2 * k + 1) * 3u) << 16) + 0x568000u, 9) - 0x4000u; }

// The beats 1 and 2: +9 up; above 0x10: +9 0, the sound byte +0xB, +2 up. The draw.
void Beat(unsigned char kick) {
    if (Count(0x10)) {
        unsigned char* const s = S();
        s[9] = 0;
        S()[0xB] = kick;
        Step();
    }
    SH_CALL(EffectKind18Sub24_Draw)();
}

}  // namespace

// original 0x506BD0 (EffectKind18_States[36]): jmp [EffectKind18Sub24_States +
// +2 * 4], unbounded. A dispatcher no list had.
extern "C" void __cdecl EffectKind18Sub24_Run(void) {
    Dispatch("EffectKind18Sub24_Run", AddressOf(EffectKind18Sub24_States), EffectKind18Sub24_States_count);
}

// original 0x506BF0 (sub-state 0): nothing until the counter 0x903848 is 5;
// then Flags_Set(Cond_Flags + 0x50, 4), the record placed (0x4B8000, 0x568000,
// height 0x3A0, level 0x80), +0xB = 2, +9 = 0, sound 0x204, +2 up, the draw.
extern "C" void __cdecl EffectKind18Sub24_Start(void) {
    if (!Cue(at::kSub24Cue)) return;
    SH_CALL(Flags_Set)(At(at::kFlagRow50), 4);
    Place(0x4B8000, 0x568000, 0x3A0, 0x80);
    unsigned char* const s = S();
    s[0xB] = 2;
    s[9] = 0;
    SH_CALL(Sound_PlayEffect)(0x204);
    Step();
    SH_CALL(EffectKind18Sub24_Draw)();
}

// original 0x506CB0 (sub-state 1): a beat of 0x11 frames, then +0xB = 1.
extern "C" void __cdecl EffectKind18Sub24_Beat1(void) { Beat(1); }

// original 0x506CF0 (sub-state 2): the same, +0xB = 2.
extern "C" void __cdecl EffectKind18Sub24_Beat2(void) { Beat(2); }

// original 0x506D30 (sub-state 3): +9 up; above 0x10: sound 0x201, +9 0 (stored
// before the call), +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub24_Beat3(void) {
    if (Count(0x10)) {
        S()[9] = 0;
        SH_CALL(Sound_PlayEffect)(0x201);
        Step();
    }
    SH_CALL(EffectKind18Sub24_Draw)();
}

// original 0x506D70 (sub-state 4): Prim_VertexScratch's 0x9037A0 = (0xE5C0,
// 0xEB40, 0x160), its angles 0x9037A8 = (0x620, 0, ...); seven trails from
// +9 * 4 to 0x1F, the angle 0xE00 + 0x400 k, scale 0x18, each turned by
// (k * 32 + Frame_Counter) * 32; from +9 = 3 on, four more from the corners
// (y 0x3A0, angles (0x400, 0, ...)), each the whole polyline (0 to 0x1E, scale
// 0x10). The draw; +9 up; above 7: +2 up and sound 0x200.
extern "C" void __cdecl EffectKind18Sub24_Trails(void) {
    unsigned char* const v = Vtx();
    SetWord(v + 0x0, 0xE5C0);
    SetWord(v + 0x2, 0xEB40);
    SetWord(v + 0x4, 0x160);
    SetWord(v + 0x8, 0x620);
    SetWord(v + 0xA, 0);
    U k = 0;
    for (U angle = 0xE00; angle < 0x1E00; angle += 0x400, ++k) {
        const U frame = Frame_Counter;
        unsigned char* const s = S();
        SetWord(v + 0xC, ((k << 5) + frame) << 5);
        SH_CALL(EffectKind18Sub24_DrawTrail)(static_cast<int>(static_cast<U>(s[9]) << 2), 0x1F, static_cast<int>(angle), 0x18);
    }
    if (S()[9] >= 3) {
        SetWord(v + 0x4, 0x3A0);
        SetWord(v + 0x8, 0x400);
        SetWord(v + 0xA, 0);
        for (U j = 0; j < at::kSub24PointCount; ++j) {
            SetWord(v + 0x2, CornerZ(j));
            SetWord(v + 0xC, ((j << 5) + Frame_Counter) << 5);
            SetWord(v + 0x0, CornerX(j));
            SH_CALL(EffectKind18Sub24_DrawTrail)(0, 0x1E, static_cast<int>(j << 10), 0x10);
        }
    }
    SH_CALL(EffectKind18Sub24_Draw)();
    if (Count(7)) {
        Step();
        SH_CALL(Sound_PlayEffect)(0x200);
    }
}

// original 0x506ED0 (sub-state 5): the four corner trails from +9 * 4 - 0x20 to
// 0x1E (the angle 0x400 j, scale 0x10); the draw; +9 up; above 0xF: the counter
// 0x903848 up, +9 0, +2 up.
extern "C" void __cdecl EffectKind18Sub24_TrailsOut(void) {
    unsigned char* const v = Vtx();
    SetWord(v + 0x4, 0x3A0);
    SetWord(v + 0x8, 0x400);
    SetWord(v + 0xA, 0);
    for (U j = 0; j < at::kSub24PointCount; ++j) {
        SetWord(v + 0x0, CornerX(j));
        SetWord(v + 0x2, CornerZ(j));
        SetWord(v + 0xC, ((j << 5) + Frame_Counter) << 5);
        const U from = (static_cast<U>(S()[9]) << 2) - 0x20u;
        SH_CALL(EffectKind18Sub24_DrawTrail)(static_cast<int>(from), 0x1E, static_cast<int>(j << 10), 0x10);
    }
    SH_CALL(EffectKind18Sub24_Draw)();
    if (Count(0xF)) {
        unsigned char* const s = S();
        B(at::kCounter) = static_cast<unsigned char>(B(at::kCounter) + 1);
        s[9] = 0;
        Step();
    }
}

// original 0x506FC0 (sub-state 6): the angle turned; the speed +0x14 += (q - 10)
// * q, q = +0x14 / 10000; the sound byte on each half turn; while the speed is
// 0xA6040..0xF9060 the cell jitters by 0x1000, from 0xF9060 by 0x2000 (and the
// counter 6 becomes 7); from 0x14E790, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub24_Spin(void) {
    Turn();
    unsigned char* s = S();
    const U speed = UL(s + 0x14), q = Speed(speed);
    SetUL(s + 0x14, (q - 10u) * q + speed);
    s = Kick(S(), false);
    s = S();
    const std::int32_t now = static_cast<std::int32_t>(UL(s + 0x14));
    if (now >= 0xA6040 && now <= 0xF9060) {
        Jitter(0x1000);
        s = S();
    }
    if (static_cast<std::int32_t>(UL(s + 0x14)) >= 0xF9060) {
        Jitter(0x2000);
        if (Cue(at::kSub24Hand)) B(at::kCounter) = 7;
        s = S();
    }
    if (static_cast<std::int32_t>(UL(s + 0x14)) >= 0x14E790) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub24_Draw)();
}

// original 0x5070E0 (sub-state 7): the angle turned; the cell jitters by
// 0x2000; the sound byte on each half turn while the level is above 0x40; the
// level +0x30 down 2; at 0 the counter up and Effect_Release. The draw.
extern "C" void __cdecl EffectKind18Sub24_Fade(void) {
    Turn();
    Jitter(0x2000);
    Kick(S(), true);
    unsigned char* s = S();
    SetWord(s + 0x30, W(s + 0x30) - 2);
    s = S();
    if (W(s + 0x30) == 0) {
        B(at::kCounter) = static_cast<unsigned char>(B(at::kCounter) + 1);
        SH_CALL(Effect_Release)();
    }
    SH_CALL(EffectKind18Sub24_Draw)();
}

// original 0x507190 (the draw sub-states 0..7 and sub-kind 0x3F's spins call or
// jump to): Gte_PushMatrix; a matrix of the record's angle (0, 0, +0x32) times
// Camera_Matrix, its translation the record's point (+0x34, +0x38, +0x3C as
// s16) turned; the sound byte +0xB: when set, sound +0xB + 0x201 and +0xB 0, the
// texture 0x1300900F, else 0x1300980C; four POLY_FT4s from the centre (0, 0,
// -0x240) to each pair of neighbouring corners (x 3 << 7), their level the
// record's +0x30 - or, above 0x40, +0x30 less Math_Cos(+0x32 + 0x400 k) >> 7
// less 0x20 - in bits 16..31 of the texture word (low three bits cleared),
// committed (5, 0x48). Gte_PopMatrix.
extern "C" void __cdecl EffectKind18Sub24_Draw(void) {
    unsigned char* s = S();
    short pos[4] = {static_cast<short>(W(s + 0x34)), static_cast<short>(W(s + 0x38)), static_cast<short>(W(s + 0x3C)), 0};
    short angles[4] = {0, 0, static_cast<short>(W(s + 0x32)), 0};
    SH_CALL(Gte_PushMatrix)();
    Matrix m;
    SH_CALL(Gte_RotTrans)(pos, m.t);
    SH_CALL(Gte_RotMatrix)(angles, m.m);
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
    s = S();
    const unsigned char kick = s[0xB];
    U texture;
    if (kick != 0) {
        texture = 0x1300900F;
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(kick + 0x201u));
        S()[0xB] = 0;
    } else {
        texture = 0x1300980C;
    }
    unsigned char* const v = Vtx();
    U turn = 0;
    for (U k = 0; k < at::kSub24PointCount; ++k) {
        unsigned char* const p = TexturedQuad();
        const U n = ((k + 1) & 3) * 2;
        SetWord(v + 0xA, 0);
        SetWord(v + 0x2, 0);
        SetWord(v + 0x8, 0);
        SetWord(v + 0x10, (SB(at::kSub24Points + 2 * k) * 3u) << 7);
        SetWord(v + 0x0, 0);
        SetWord(v + 0x1C, 0);
        SetWord(v + 0x14, 0);
        SetWord(v + 0x12, (SB(at::kSub24Points + 2 * k + 1) * 3u) << 7);
        SetWord(v + 0x1A, (SB(at::kSub24Points + n + 1) * 3u) << 7);
        SetWord(v + 0x18, (SB(at::kSub24Points + n) * 3u) << 7);
        SetWord(v + 0xC, 0xFDC0);
        SetWord(v + 0x4, 0xFDC0);
        Project4(p);
        SH_CALL(Gte_PrimDepths4_10)(p);
        s = S();
        U level;
        if (SW(s + 0x30) > 0x40) {
            const U cosine = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(static_cast<U>(SW(s + 0x32)) + turn)));
            s = S();
            level = static_cast<U>(SW(s + 0x30)) - Sar(cosine, 7) - 0x20u;
        } else {
            level = static_cast<U>(SW(s + 0x30));
        }
        SH_CALL(Prim_SetTexture)(((level & 0xFFF8u) << 16) | texture, p, 1);
        SH_CALL(Gfx_CommitPrim)(5, 0x48);
        turn += 0x400;
    }
    SH_CALL(Gte_PopMatrix)();
}

// original 0x5073D0 (cdecl (from, to, angle, scale); sub-states 4, 5 and
// sub-kind 0x3F's state 9): Gte_PushMatrix; a draw mode (tpage 0x35, dtd 1)
// committed (5, 0xC); a matrix of Prim_VertexScratch's angles 0x9037A8 turned
// about z by `angle`, times Camera_Matrix, its translation 0x9037A0 turned; for
// each i of from..to - 1 a segment of the polyline kTrailPairs from pair i to
// pair i + 1 (y the pair's second byte times `scale`, z its first times -48),
// drawn twice (j = 0, 2) as a semi-transparent POLY_G4: the segment projected
// (Gte_RotTransPers3 with 0x9037A0 into MapView_ScreenXY), its two depths, the
// far edge the near one moved by the float 0x5C41B8 less j, colour (0xC8,
// 0xB4, 0x0A) fading to black, committed (5, 0x44). A draw mode (tpage 0x35,
// dtd 0) committed (5, 0xC); Gte_PopMatrix. A pair past the polyline's 34 (an i
// below 0 or above 32) aborts: the original reads the bytes either side.
extern "C" void __cdecl EffectKind18Sub24_DrawTrail(int from, int to, int angle, int scale) {
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    Matrix m;
    SH_CALL(Gte_RotTrans)(Vector(0), m.t);
    SH_CALL(Gte_RotMatrix)(Vector(8), m.m);
    SH_CALL(Gte_RotMatrixZ)(angle, m.m);
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
    if (from < to) {
        unsigned char* const v = Vtx();
        U i = static_cast<U>(from);
        for (U count = static_cast<U>(to) - static_cast<U>(from); count != 0; --count, ++i) {
            if (static_cast<std::int32_t>(i) < 0 || i + 1 >= at::kTrailPairCount)
                bof3::Fatal("EffectKind18Sub24_DrawTrail: segment %d reads pairs %d and %d of the trail's %u - the "
                            "original reads the .data either side (docs/effect_5e.md section 6)",
                            static_cast<int>(i), static_cast<int>(i), static_cast<int>(i + 1), at::kTrailPairCount);
            const U pair = at::kTrailPairs + 2 * i;
            const U s = static_cast<U>(scale);
            SetWord(v + 0x18, 0);
            SetWord(v + 0x12, SB(pair + 1) * s);
            SetWord(v + 0x10, 0);
            SetWord(v + 0x1A, SB(pair + 3) * s);
            SetWord(v + 0x14, (0u - SB(pair) * 3u) << 4);
            SetWord(v + 0x1C, (0u - SB(pair + 2) * 3u) << 4);
            for (std::int32_t j = 0; j < 4; j += 2) {
                unsigned char* const p = Gfx_PacketNext;
                SH_CALL(Gpu_SetPolyG4)(p);
                SH_CALL(Gpu_SetSemiTrans)(p, 1);
                long depth = 0;
                SH_CALL(Gte_RotTransPers3)(Vector(0x10), Vector(0x18), Vector(0), F(p + 8), F(p + 0x18), MapView_ScreenXY,
                                           &depth);
                float third = 0;
                SH_CALL(Gte_StoreDepthF3)(F(p + 0x10), F(p + 0x20), &third);
                Offset(p + 8, j, p + 0x28);
                SetUL(p + 0x30, UL(p + 0x10));
                SetUL(p + 0x40, UL(p + 0x20));
                p[4] = 0xC8;
                p[5] = 0xB4;
                p[6] = 0x0A;
                p[0x14] = 0xC8;
                Offset(p + 0x18, j, p + 0x38);
                p[0x15] = 0xB4;
                p[0x16] = 0x0A;
                p[0x24] = 0;
                p[0x25] = 0;
                p[0x26] = 0;
                p[0x34] = 0;
                p[0x35] = 0;
                p[0x36] = 0;
                Offset(p + 0xC, j, p + 0x2C);
                Offset(p + 0x1C, j, p + 0x3C);
                SH_CALL(Gfx_CommitPrim)(5, 0x44);
            }
        }
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x35, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    SH_CALL(Gte_PopMatrix)();
}

// ===========================================================================
// Sub-kind 0x3F: EffectKind18_States[63] (0x654168), EffectKind18Sub3F_States
// (sixteen) by +2
// ===========================================================================

namespace {

// The sky's colours while it warms (+9 = n, 0..0x5A): the top 0x7C00 - (q <<
// 10) | 0x320 - (q << 5), q = 27 n / 100; the bottom n / 17 | 0x1000 (bits 16..).
U SkyWarm(U n) {
    const U q = Div(0x51EB851F, n * 27u, 5);
    U c = 0x7C00u - (q << 10);
    c |= 0x320u - (q << 5);
    return c | Div(0x78787879, n, 3) | 0x10000000u;
}
// ... while it cools (+9 = n, 0..0x78): q = 20 n / 100, r = n / 24: 5 - r |
// (q + 6) << 10 | (q | 0x800000) << 5.
U SkyCool(U n) {
    const U q = Div(0x51EB851F, n * 20u, 5);
    const U r = Div(0x2AAAAAAB, n, 2);
    return (5u - r) | ((q + 6u) << 10) | ((q | 0x800000u) << 5);
}
constexpr U kSkyNight = 0x10001805;   // the colours most states hold
constexpr U kSkyWhite = 0xFFFFFFFFu;

// MapView_BuildFlags 0 and the sky (most states draw it first).
void Sky(U colours) {
    MapView_BuildFlags = 0;
    SH_CALL(EffectKind18Sub3F_DrawSky)(colours);
}
// The sky held until the counter reaches `cue`, then +2 up.
void HoldUntil(unsigned cue) {
    Sky(kSkyNight);
    if (Cue(cue)) Step();
}
// The leader's shade bytes (ObjTrio record 0's +0x5F, +0x5E, +0x5D).
void LeaderShade(unsigned char v) {
    B(at::kLeaderShade + 2) = v;
    B(at::kLeaderShade + 1) = v;
    B(at::kLeaderShade + 0) = v;
}

}  // namespace

// original 0x507620 (EffectKind18_States[63]): jmp [EffectKind18Sub3F_States +
// +2 * 4], unbounded. A dispatcher no list had.
extern "C" void __cdecl EffectKind18Sub3F_Run(void) {
    Dispatch("EffectKind18Sub3F_Run", AddressOf(EffectKind18Sub3F_States), EffectKind18Sub3F_States_count);
}

// original 0x507640 (sub-state 0): the record placed (0x238000, 0x1B8000,
// height -0x4C0, level 0x60); +2 up.
extern "C" void __cdecl EffectKind18Sub3F_Start(void) {
    Place(0x238000, 0x1B8000, 0xFFFFFB40u, 0x60);
    Step();
}

// original 0x5076C0 (sub-state 1): at the counter 0x26: CLUT row 4 at half
// (0x40), MapView_BuildFlags 0, the sky white, +9 0, +2 up.
extern "C" void __cdecl EffectKind18Sub3F_WaitDim(void) {
    if (!Cue(at::kSub3FDim)) return;
    SH_CALL(EffectKind18Sub3F_ShadeClut)(0x40);
    Sky(kSkyWhite);
    S()[9] = 0;
    Step();
}

// original 0x507700 (sub-state 2): CLUT row 4 at full (0x80), MapView_BuildFlags
// 1, +2 up.
extern "C" void __cdecl EffectKind18Sub3F_Undim(void) {
    SH_CALL(EffectKind18Sub3F_ShadeClut)(0x80);
    unsigned char* const s = S();
    MapView_BuildFlags = 1;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x507720 (sub-state 3): the sky warming by +9; +9 up; above 0x5A:
// +9 0, +2 up.
extern "C" void __cdecl EffectKind18Sub3F_SkyWarm(void) {
    const U n = S()[9];
    Sky(SkyWarm(n));
    if (Count(0x5A)) {
        S()[9] = 0;
        Step();
    }
}

// original 0x5077B0 (sub-state 4): the sky held until the counter is 0x29.
extern "C" void __cdecl EffectKind18Sub3F_WaitCue29(void) { HoldUntil(at::kSub3FCue29); }

// original 0x5077E0 (sub-state 5): the sky cooling by +9; +9 up; above 0x78:
// MapView_BuildFlags 1, +2 up (+9 kept).
extern "C" void __cdecl EffectKind18Sub3F_SkyCool(void) {
    const U n = S()[9];
    Sky(SkyCool(n));
    if (Count(0x78)) {
        unsigned char* const s = S();
        MapView_BuildFlags = 1;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
}

// original 0x507870 (sub-state 6): at the counter 0x32: sound 0x208, +9 0, +2
// up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub3F_WaitCue32(void) {
    if (!Cue(at::kSub3FCue32)) return;
    SH_CALL(Sound_PlayEffect)(0x208);
    S()[9] = 0;
    Step();
}

// original 0x5078A0 (sub-state 7): the sky warming by +9; CLUT row 4 at 0x80 -
// +9 / 3; the leader's shade bytes (+9 / 3 - +9) / 2; +9 up; above 0x5A: +9 0,
// +2 up.
extern "C" void __cdecl EffectKind18Sub3F_SkyDim(void) {
    const U n = S()[9];
    Sky(SkyWarm(n));
    SH_CALL(EffectKind18Sub3F_ShadeClut)(static_cast<int>(0x80u - Div(0x55555556, S()[9], 0)));
    unsigned char* const s = S();
    const U m = s[9];
    U t = Sar(MulHi(0x55555555, m) - m, 1);
    t += t >> 31;
    LeaderShade(static_cast<unsigned char>(t));
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] > 0x5A) {
        S()[9] = 0;
        Step();
    }
}

// original 0x507980 (sub-state 8): the sky held; at the counter 0x35, +2 up
// and sound 0x201.
extern "C" void __cdecl EffectKind18Sub3F_WaitCue35(void) {
    Sky(kSkyNight);
    if (!Cue(at::kSub3FCue35)) return;
    Step();
    SH_CALL(Sound_PlayEffect)(0x201);
}

// original 0x5079C0 (sub-state 9): Prim_VertexScratch 0x9037A0 = (0xD1C0,
// 0xCDC0, 0xFC20), its angles (0, 0, Frame_Counter * 32); the trail from +9 * 4
// to 0x1F (angle 0, scale 0x18); the sky held; +9 up; above 7, +2 up.
extern "C" void __cdecl EffectKind18Sub3F_Trail(void) {
    const U frame = Frame_Counter;
    unsigned char* const s = S();
    unsigned char* const v = Vtx();
    SetWord(v + 0x0, 0xD1C0);
    SetWord(v + 0x2, 0xCDC0);
    SetWord(v + 0x4, 0xFC20);
    SetWord(v + 0x8, 0);
    SetWord(v + 0xA, 0);
    SetWord(v + 0xC, frame << 5);
    SH_CALL(EffectKind18Sub24_DrawTrail)(static_cast<int>(static_cast<U>(s[9]) << 2), 0x1F, 0, 0x18);
    Sky(kSkyNight);
    if (Count(7)) Step();
}

// original 0x507A50 (sub-state 10): the sky held until the counter is 0x37.
extern "C" void __cdecl EffectKind18Sub3F_WaitCue37(void) { HoldUntil(at::kSub3FCue37); }

// original 0x507A80 (sub-state 11): the sky white; sounds 0x202 and 0x200; +9 0,
// +2 up.
extern "C" void __cdecl EffectKind18Sub3F_Flash(void) {
    Sky(kSkyWhite);
    SH_CALL(Sound_PlayEffect)(0x202);
    SH_CALL(Sound_PlayEffect)(0x200);
    S()[9] = 0;
    Step();
}

// original 0x507AC0 (sub-state 12): the sky held; the angle turned; sub-kind
// 0x24's draw; at the counter 0x3A, +2 up.
extern "C" void __cdecl EffectKind18Sub3F_Spin(void) {
    Sky(kSkyNight);
    Turn();
    SH_CALL(EffectKind18Sub24_Draw)();
    if (Cue(at::kSub3FCue3A)) Step();
}

// original 0x507B20 (sub-state 13): the sky held; the angle turned; the level
// +0x30 down 6; sub-kind 0x24's draw; +9 up; above 0xF, +2 up.
extern "C" void __cdecl EffectKind18Sub3F_SpinFade(void) {
    Sky(kSkyNight);
    Turn();
    unsigned char* const s = S();
    SetWord(s + 0x30, W(s + 0x30) - 6);
    SH_CALL(EffectKind18Sub24_Draw)();
    if (Count(0xF)) Step();
}

// original 0x507B90 (sub-state 14): the sky held until the counter is 0x3B.
extern "C" void __cdecl EffectKind18Sub3F_WaitCue3B(void) { HoldUntil(at::kSub3FCue3B); }

// original 0x507BC0 (sub-state 15): an opaque white POLY_F4 over the frame
// (DIV-0041's site 0x507BDC: (Widescreen_FillX(), 0) .. (FillX + FillWidth, 240),
// the original's (0, 0) .. (320, 240) narrow) committed (4, 0x38); sound 0x202;
// MapView_BuildFlags 1; CLUT row 4 at full; the leader's shade 0; a tail call of
// Effect_Release.
extern "C" void __cdecl EffectKind18Sub3F_WhiteOut(void) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    const U left = std::bit_cast<U>(Widescreen_FillX());
    const U right = std::bit_cast<U>(Widescreen_FillX() + Widescreen_FillWidth());   // 0x43A00000, 320.0f narrow
    SetUL(p + 0x24, 0x43700000u);   // 240.0f
    SetUL(p + 0x30, 0x43700000u);
    SetUL(p + 0x8, left);
    SetUL(p + 0xC, 0);
    SetUL(p + 0x14, right);
    SetUL(p + 0x18, 0);
    SetUL(p + 0x20, left);
    SetUL(p + 0x2C, right);
    p[4] = 0xFF;
    p[5] = 0xFF;
    p[6] = 0xFF;
    SH_CALL(Gfx_CommitPrim)(4, 0x38);
    SH_CALL(Sound_PlayEffect)(0x202);
    MapView_BuildFlags = 1;
    SH_CALL(EffectKind18Sub3F_ShadeClut)(0x80);
    LeaderShade(0);
    SH_CALL(Effect_Release)();
}

// original 0x507C40 (cdecl (level); sub-kind 0x3F's states 1, 2, 7, 15): each of
// CLUT row 4's 0x100 colours as loaded (Gfx_ClutStripSource + 0x800) scaled by
// level / 128 - each 5-bit channel times the level, sar 7, packed back
// unclamped (a channel past 31 runs into the next) - with its bit 15 kept,
// written to the live strip (Gfx_ClutStrip + 0x800); Gfx_ClutStripDirty 1.
extern "C" void __cdecl EffectKind18Sub3F_ShadeClut(int level) {
    const U l = static_cast<U>(level);
    for (U k = 0; k < at::kClutRowWords; ++k) {
        const U colour = W(At(at::kClutRow4 + 2 * k));
        const U blue = ((colour >> 10) & 0x1F) * l, green = ((colour >> 5) & 0x1F) * l, red = (colour & 0x1F) * l;
        U out = Sar(blue, 7) << 5;
        out |= Sar(green, 7);
        out <<= 5;
        out |= Sar(red, 7);
        out |= colour & 0x8000;
        SetWord(At(at::kClutRow4Live + 2 * k), out);
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x507CB0 (cdecl (colours); sub-kind 0x3F's states): a draw mode
// (tpage 0x95, dtd 1) committed (6, 0xC); a POLY_G4 over the frame (DIV-0041's
// site 0x507CE3: x Widescreen_FillX() .. FillX + FillWidth, the original's 0 ..
// 320 narrow; y 0 .. 240), the top corners the 15-bit colour in bits 0..14,
// the bottom ones bits 16..30 (each channel << 3), committed (6, 0x44); a draw
// mode (tpage 0x95, dtd 0) committed (6, 0xC).
extern "C" void __cdecl EffectKind18Sub3F_DrawSky(U colours) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    const U left = std::bit_cast<U>(Widescreen_FillX());
    const U right = std::bit_cast<U>(Widescreen_FillX() + Widescreen_FillWidth());   // 0x43A00000, 320.0f narrow
    SetUL(p + 0x2C, 0x43700000u);   // 240.0f
    SetUL(p + 0x3C, 0x43700000u);
    SetUL(p + 0x18, right);
    SetUL(p + 0x38, right);
    const auto channel = [colours](unsigned shift) { return static_cast<unsigned char>(((colours >> shift) & 0x1F) << 3); };
    p[0x14] = p[4] = channel(0);
    SetUL(p + 0x8, left);
    p[0x15] = p[5] = channel(5);
    SetUL(p + 0xC, 0);
    p[0x16] = p[6] = channel(10);
    SetUL(p + 0x1C, 0);
    SetUL(p + 0x28, left);
    p[0x34] = p[0x24] = channel(16);
    p[0x35] = p[0x25] = channel(21);
    p[0x36] = p[0x26] = channel(26);
    SH_CALL(Gfx_CommitPrim)(6, 0x44);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
}

// ===========================================================================
// Sub-kinds 0x25 and 0x26: EffectKind18_States[37] (0x654100) and [38]
// (0x654104), EffectKind18Sub25_States and EffectKind18Sub26_States (five
// each) by +2; sub-state 3 of both (and of E5F's table 0x65E9A8) is one function
// ===========================================================================

namespace {

// Whether the leader (ObjTrio record 0's x, z) stands at the record's cell:
// with +8 set (the lid lies along x) its z within `across` of the cell's
// middle row (+0x3A + 1) << 16 | 0x8000, and its x within `along` of the cell
// +0x36 << 16 or the next; with +8 clear the same with x and z swapped.
bool LeaderAt(const unsigned char* s, std::int32_t across, std::int32_t along) {
    const bool lengthwise = s[8] != 0;
    const U first = lengthwise ? UL(at::kLeaderZ) : UL(at::kLeaderX);
    const U middle = ((static_cast<U>(SW(s + (lengthwise ? 0x3A : 0x36))) + 1u) << 16) | 0x8000u;
    if (Abs(first - middle) > across) return false;
    const U second = (lengthwise ? UL(at::kLeaderX) : UL(at::kLeaderZ)) - (static_cast<U>(SW(s + (lengthwise ? 0x36 : 0x3A))) << 16);
    if (Abs(second) <= along) return true;
    return Abs(second - 0x10000u) <= along;
}

// The start of both: +8 = (+0x3A == 0); the variant +0x36 picks the cell
// (+0x36, +0x3A), the height +0x3E, the page +0x2E, the lid +0x32 (and for 0x26
// the texture word +0x20); +0x30 0; +2 up; when the leader is at the cell,
// open at once: +0x30 0x80, +2 = 3.
struct Variants {
    const char* who;
    U cells, heights, pages, lids, words;
    unsigned count;
};
void StartLid(const Variants& t) {
    unsigned char* s = S();
    const std::int32_t v = SW(s + 0x36);
    s[8] = W(s + 0x3A) == 0 ? 1 : 0;
    if (v < 0 || v >= static_cast<std::int32_t>(t.count))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u of its tables - the original reads the .data after "
                    "(docs/effect_5e.md section 6)",
                    t.who, v, t.count);
    const U i = static_cast<U>(v);
    s = S();
    SetWord(s + 0x36, B(t.cells + 2 * i));
    SetWord(s + 0x3A, B(t.cells + 2 * i + 1));
    SetWord(s + 0x3E, W(At(t.heights + 2 * i)));
    if (t.words) SetUL(s + 0x20, UL(t.words + 4 * i));
    SetWord(s + 0x2E, W(At(t.pages + 2 * i)));
    SetWord(s + 0x32, B(t.lids + i));
    SetWord(s + 0x30, 0);
    Step();
    s = S();
    if (LeaderAt(s, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0x80);
        S()[2] = 3;
    }
}
const Variants kSub25 = {"EffectKind18Sub25_Start", at::kSub25Cells, at::kSub25Heights, at::kSub25Pages,
                         at::kSub25Lids, 0, at::kSub25Variants};
const Variants kSub26 = {"EffectKind18Sub26_Start", at::kSub26Cells, at::kSub26Heights, at::kSub26Lifts,
                         at::kSub26Lids, at::kSub26Pages, at::kSub26Variants};

// Sub-state 1 of both: when the leader is at the cell, sound 0x200 unless
// Field_Request is set, +2 up.
void WaitNear() {
    unsigned char* s = S();
    if (!LeaderAt(s, 0x8000, 0x10000)) return;
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(0x200);
        s = S();
    }
    s[2] = static_cast<unsigned char>(s[2] + 1);
}
// Sub-state 2 of both: +0x30 up 0x10; at 0x80, +2 up. Answers the draw's dy:
// 0 with a lid (+0x32), else -1.
int Open() {
    unsigned char* s = S();
    SetWord(s + 0x30, W(s + 0x30) + 0x10);
    s = S();
    if (SW(s + 0x30) >= 0x80) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        s = S();
    }
    return W(s + 0x32) != 0 ? 0 : -1;
}
// Sub-state 4 of both: +0x30 down 0x10; at 0 or below, sound 0x201 unless
// Field_Request is set and +2 = 1.
int Close() {
    unsigned char* s = S();
    SetWord(s + 0x30, W(s + 0x30) - 0x10);
    s = S();
    if (SW(s + 0x30) <= 0) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
        S()[2] = 1;
    }
    return W(S() + 0x32) != 0 ? 0 : -1;
}

// The lid's texture: the dword `index` of the table (entry 0 is the leaves'
// signs, which the lid never asks for: the callers skip a lid of 0).
U LidTexture(const char* who, U table, unsigned count, std::int32_t index) {
    if (index < 0 || index >= static_cast<std::int32_t>(count))
        bof3::Fatal("%s: the lid +0x32 is %d, past the %u textures of 0x%X - the original reads the .data after "
                    "(docs/effect_5e.md section 6)",
                    who, index, count, (unsigned)table);
    return UL(table + 4 * static_cast<U>(index));
}

// The two leaves of both draws: each leaf i (0, 1) moves by +0x30 times the
// sign byte i of the table (1, -1) across the cell; along x when +8 is set
// (the leaf's x from (i + +0x36) << 7, its z edge -0x4040 / -0x3FC0), else
// along z ((+0x3A - i) << 7, -0x3F40 / -0x3FC0).
void Leaf(U i, U signs) {
    unsigned char* const v = Vtx();
    unsigned char* const s = S();
    if (s[8]) {
        const U sign = SB(signs + i);
        const U base = (i + W(s + 0x36)) << 7;
        const U a = base - W(s + 0x30) * sign - 0x4040u;
        SetWord(v + 0x10, a);
        SetWord(v + 0x0, a);
        const U b = base - W(s + 0x30) * sign - 0x3FC0u;
        SetWord(v + 0x18, b);
        SetWord(v + 0x8, b);
    } else {
        const U sign = SB(signs + i);
        const U base = (W(s + 0x3A) - i) << 7;
        const U a = base + W(s + 0x30) * sign - 0x3F40u;
        SetWord(v + 0x12, a);
        SetWord(v + 0x2, a);
        const U b = base + W(s + 0x30) * sign - 0x3FC0u;
        SetWord(v + 0x1A, b);
        SetWord(v + 0xA, b);
    }
}

// The lid's corners over the cell.
void LidCorners(const unsigned char* s) {
    unsigned char* const v = Vtx();
    if (s[8]) {
        const U a = (W(s + 0x36) << 7) - 0x40C0u, b = (W(s + 0x36) << 7) - 0x4040u;
        SetWord(v + 0x10, a);
        SetWord(v + 0x0, a);
        SetWord(v + 0x18, b);
        SetWord(v + 0x8, b);
    } else {
        const U a = (W(s + 0x3A) << 7) - 0x4040u, b = (W(s + 0x3A) << 7) - 0x40C0u;
        SetWord(v + 0x12, a);
        SetWord(v + 0x2, a);
        SetWord(v + 0x1A, b);
        SetWord(v + 0xA, b);
    }
}

// The fixed edge of the cell both draws start with: with +8 set the z
// (+0x3A << 7) - 0x3FC0 in all four vectors' y... (the record's cell row),
// else the x.
void CellEdge(const unsigned char* s) {
    unsigned char* const v = Vtx();
    if (s[8]) {
        const U y = (W(s + 0x3A) << 7) - 0x3FC0u;
        SetWord(v + 0x1A, y);
        SetWord(v + 0x12, y);
        SetWord(v + 0xA, y);
        SetWord(v + 0x2, y);
    } else {
        const U x = (W(s + 0x36) << 7) - 0x3FC0u;
        SetWord(v + 0x18, x);
        SetWord(v + 0x10, x);
        SetWord(v + 0x8, x);
        SetWord(v + 0x0, x);
    }
}

}  // namespace

// original 0x507D90 (EffectKind18_States[37]): jmp [EffectKind18Sub25_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub25_Run(void) {
    Dispatch("EffectKind18Sub25_Run", AddressOf(EffectKind18Sub25_States), EffectKind18Sub25_States_count);
}

// original 0x507DB0 (sub-state 0): the variant's cell, height, page and lid
// (eight variants; past them ours aborts); open at once when the leader is at
// the cell. The draw (dy 0).
extern "C" void __cdecl EffectKind18Sub25_Start(void) {
    StartLid(kSub25);
    SH_CALL(EffectKind18Sub25_Draw)(0);
}

// original 0x507F10 (sub-state 1): waits for the leader at the cell. The draw.
extern "C" void __cdecl EffectKind18Sub25_WaitNear(void) {
    WaitNear();
    SH_CALL(EffectKind18Sub25_Draw)(0);
}

// original 0x508000 (sub-state 2): opens. The draw (dy 0 with a lid, else -1).
extern "C" void __cdecl EffectKind18Sub25_Open(void) {
    const int dy = Open();
    SH_CALL(EffectKind18Sub25_Draw)(dy);
}

// original 0x508670 (sub-state 3 of sub-kinds 0x25 and 0x26, and of E5F's table
// 0x65E9A8): +2 up once the leader is away from the cell - more than 0x20000
// across, or along it more than 0x20000 from both the cell and the next.
// Nothing drawn.
extern "C" void __cdecl EffectKind18Sub25_WaitAway(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x508040 (sub-state 4): closes, then back to sub-state 1. The draw.
extern "C" void __cdecl EffectKind18Sub25_Close(void) {
    const int dy = Close();
    SH_CALL(EffectKind18Sub25_Draw)(dy);
}

// original 0x5080A0 (cdecl (dy)): the cell's edge and the height +0x3E - 0x180
// .. +0x3E in Prim_VertexScratch; two leaves as POLY_FT4s, projected, their
// texture word (+0x2E + i) | +8 << 21 | 0x14500000, each linked at the record's
// point with dy (0x48 bytes); with a lid (+0x32 not 0) a third over the cell
// (height +0x3E - 0x140), its texture the lid's of kSub25Textures, linked with
// dy 0.
extern "C" void __cdecl EffectKind18Sub25_Draw(int dy) {
    unsigned char* s = S();
    unsigned char* const v = Vtx();
    CellEdge(s);
    const U low = W(s + 0x3E) - 0x180u;
    SetWord(v + 0xC, low);
    SetWord(v + 0x4, low);
    const U high = W(s + 0x3E);
    SetWord(v + 0x1C, high);
    SetWord(v + 0x14, high);
    for (U i = 0; i < 2; ++i) {
        unsigned char* const p = TexturedQuad();
        Leaf(i, at::kSub25Textures);
        Project4(p);
        SH_CALL(Gte_PrimDepths4_10)(p);
        s = S();
        const U word = (static_cast<U>(SW(s + 0x2E)) + i) | (static_cast<U>(s[8]) << 21) | 0x14500000u;
        SH_CALL(Prim_SetTexture)(word, p, 1);
        LinkAtRecord(dy, 0x48);
    }
    s = S();
    if (W(s + 0x32) == 0) return;
    unsigned char* const p = TexturedQuad();
    s = S();
    LidCorners(s);
    const U lid = W(s + 0x3E) - 0x140u;
    SetWord(v + 0xC, lid);
    SetWord(v + 0x4, lid);
    Project4(p);
    SH_CALL(Gte_PrimDepths4_10)(p);
    const U texture = LidTexture("EffectKind18Sub25_Draw", at::kSub25Textures, at::kSub25TextureCount, SW(S() + 0x32));
    SH_CALL(Prim_SetTexture)(texture, p, 1);
    LinkAtRecord(0, 0x48);
}

// original 0x5083B0 (EffectKind18_States[38]): jmp [EffectKind18Sub26_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub26_Run(void) {
    Dispatch("EffectKind18Sub26_Run", AddressOf(EffectKind18Sub26_States), EffectKind18Sub26_States_count);
}

// original 0x5083D0 (sub-state 0): as sub-kind 0x25's with its own five
// variants (and the texture word +0x20). The draw (dy 0).
extern "C" void __cdecl EffectKind18Sub26_Start(void) {
    StartLid(kSub26);
    SH_CALL(EffectKind18Sub26_Draw)(0);
}

// original 0x508540 (sub-state 1): waits for the leader at the cell. The draw.
extern "C" void __cdecl EffectKind18Sub26_WaitNear(void) {
    WaitNear();
    SH_CALL(EffectKind18Sub26_Draw)(0);
}

// original 0x508630 (sub-state 2): opens. The draw.
extern "C" void __cdecl EffectKind18Sub26_Open(void) {
    const int dy = Open();
    SH_CALL(EffectKind18Sub26_Draw)(dy);
}

// original 0x508730 (sub-state 4): closes. The draw.
extern "C" void __cdecl EffectKind18Sub26_Close(void) {
    const int dy = Close();
    SH_CALL(EffectKind18Sub26_Draw)(dy);
}

// original 0x508790 (cdecl (dy)): as sub-kind 0x25's draw, but the leaves from
// +0x3E - +0x2E to +0x3E, their texture word (+0x20 + i) | +8 << 21, and the
// lid's height left as the leaves had it, its texture from kSub26Textures.
extern "C" void __cdecl EffectKind18Sub26_Draw(int dy) {
    unsigned char* s = S();
    unsigned char* const v = Vtx();
    CellEdge(s);
    const U low = W(s + 0x3E) - W(s + 0x2E);
    SetWord(v + 0xC, low);
    SetWord(v + 0x4, low);
    const U high = W(s + 0x3E);
    SetWord(v + 0x1C, high);
    SetWord(v + 0x14, high);
    for (U i = 0; i < 2; ++i) {
        unsigned char* const p = TexturedQuad();
        Leaf(i, at::kSub26Textures);
        Project4(p);
        SH_CALL(Gte_PrimDepths4_10)(p);
        s = S();
        const U word = (UL(s + 0x20) + i) | (static_cast<U>(s[8]) << 21);
        SH_CALL(Prim_SetTexture)(word, p, 1);
        LinkAtRecord(dy, 0x48);
    }
    s = S();
    if (W(s + 0x32) == 0) return;
    unsigned char* const p = TexturedQuad();
    LidCorners(S());
    Project4(p);
    SH_CALL(Gte_PrimDepths4_10)(p);
    const U texture = LidTexture("EffectKind18Sub26_Draw", at::kSub26Textures, at::kSub26TextureCount, SW(S() + 0x32));
    SH_CALL(Prim_SetTexture)(texture, p, 1);
    LinkAtRecord(0, 0x48);
}

// ===========================================================================
// Sub-kind 0x39: EffectKind18_States[57] (0x654150), EffectKind18Sub39_States
// (four) by +2
// ===========================================================================

// original 0x508A80 (EffectKind18_States[57]): jmp [EffectKind18Sub39_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub39_Run(void) {
    Dispatch("EffectKind18Sub39_Run", AddressOf(EffectKind18Sub39_States), EffectKind18Sub39_States_count);
}

// original 0x508AA0 (sub-state 0): story flag 0x5A set: +0x30 0x80, +2 = 3, the
// draw of the halves (0, -2); clear: +0x30 0, +2 up, the draws (0, 1), (1, 1).
extern "C" void __cdecl EffectKind18Sub39_Start(void) {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), at::kSub39Flag)) {
        SetWord(S() + 0x30, 0x80);
        S()[2] = 3;
        SH_CALL(EffectKind18Sub39_Draw)(0, -2);
        return;
    }
    SetWord(S() + 0x30, 0);
    Step();
    SH_CALL(EffectKind18Sub39_Draw)(0, 1);
    SH_CALL(EffectKind18Sub39_Draw)(1, 1);
}

// original 0x508B00 (sub-state 1): once story flag 0x5A is set, sound 0x208
// unless Field_Request is set, +2 up. The draws (0, 1), (1, 1).
extern "C" void __cdecl EffectKind18Sub39_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), at::kSub39Flag)) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x208);
        Step();
    }
    SH_CALL(EffectKind18Sub39_Draw)(0, 1);
    SH_CALL(EffectKind18Sub39_Draw)(1, 1);
}

// original 0x508B50 (sub-state 2): +0x30 up 8; at 0x80, +2 up. The draws (0,
// -1), (1, 1).
extern "C" void __cdecl EffectKind18Sub39_Open(void) {
    unsigned char* s = S();
    SetWord(s + 0x30, W(s + 0x30) + 8);
    s = S();
    if (SW(s + 0x30) >= 0x80) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub39_Draw)(0, -1);
    SH_CALL(EffectKind18Sub39_Draw)(1, 1);
}

// original 0x508B80 (sub-state 3): Cond_ByteFE 1; the draw (2, 0).
extern "C" void __cdecl EffectKind18Sub39_Hold(void) {
    Cond_ByteFE = 1;
    SH_CALL(EffectKind18Sub39_Draw)(2, 0);
}

// original 0x508BA0 (cdecl (variant, dy)): three POLY_FT4s from kSub39Vertices
// (four (x, y, z) each), x moved by +0x30 times the sign byte (variant & 1) of
// kSub39Signs and by (variant & 1) << 7, projected, textured with dword quad +
// 3 * variant of kSub39Textures (nine; past them ours aborts), each linked at
// the record's point with dy (0x48 bytes).
extern "C" void __cdecl EffectKind18Sub39_Draw(U variant, int dy) {
    const U sign_at = at::kSub39Signs + (variant & 1);
    const U shift = (variant & 1) << 7;
    unsigned char* const v = Vtx();
    for (U q = 0; q < at::kSub39Quads; ++q) {
        unsigned char* const p = TexturedQuad();
        const U sign = SB(sign_at);
        const unsigned char* const s = S();
        for (U k = 0; k < 4; ++k) {
            const U vertex = at::kSub39Vertices + 0x18 * q + 6 * k;
            const U x = W(At(vertex)) - W(s + 0x30) * sign + shift;
            SetWord(v + 8 * k + 2, W(At(vertex + 2)));
            SetWord(v + 8 * k, x);
            SetWord(v + 8 * k + 4, W(At(vertex + 4)));
        }
        Project4(p);
        SH_CALL(Gte_PrimDepths4_10)(p);
        const U index = q + 3 * variant;
        if (index >= at::kSub39TextureCount)
            bof3::Fatal("EffectKind18Sub39_Draw: variant %d asks for texture %u of the %u at 0x%X - the original reads "
                        "the .data after (docs/effect_5e.md section 6)",
                        static_cast<int>(variant), index, at::kSub39TextureCount, (unsigned)at::kSub39Textures);
        SH_CALL(Prim_SetTexture)(UL(at::kSub39Textures + 4 * index), p, 1);
        LinkAtRecord(dy, 0x48);
    }
}

void Effect5E_Inject() {
    if (bof3::WantsShadow("effect_5e")) effect_5e::SelfTest();
    BOF3_INJECT(EffectKind18Sub23_Run);
    BOF3_INJECT(EffectKind18Sub23_Start);
    BOF3_INJECT(EffectKind18Sub23_WaitFlag);
    BOF3_INJECT(EffectKind18Sub23_Slide);
    BOF3_INJECT(EffectKind18Sub23_Draw);
    BOF3_INJECT(EffectKind18Sub24_Run);
    BOF3_INJECT(EffectKind18Sub24_Start);
    BOF3_INJECT(EffectKind18Sub24_Beat1);
    BOF3_INJECT(EffectKind18Sub24_Beat2);
    BOF3_INJECT(EffectKind18Sub24_Beat3);
    BOF3_INJECT(EffectKind18Sub24_Trails);
    BOF3_INJECT(EffectKind18Sub24_TrailsOut);
    BOF3_INJECT(EffectKind18Sub24_Spin);
    BOF3_INJECT(EffectKind18Sub24_Fade);
    BOF3_INJECT(EffectKind18Sub24_Draw);
    BOF3_INJECT(EffectKind18Sub24_DrawTrail);
    BOF3_INJECT(EffectKind18Sub3F_Run);
    BOF3_INJECT(EffectKind18Sub3F_Start);
    BOF3_INJECT(EffectKind18Sub3F_WaitDim);
    BOF3_INJECT(EffectKind18Sub3F_Undim);
    BOF3_INJECT(EffectKind18Sub3F_SkyWarm);
    BOF3_INJECT(EffectKind18Sub3F_WaitCue29);
    BOF3_INJECT(EffectKind18Sub3F_SkyCool);
    BOF3_INJECT(EffectKind18Sub3F_WaitCue32);
    BOF3_INJECT(EffectKind18Sub3F_SkyDim);
    BOF3_INJECT(EffectKind18Sub3F_WaitCue35);
    BOF3_INJECT(EffectKind18Sub3F_Trail);
    BOF3_INJECT(EffectKind18Sub3F_WaitCue37);
    BOF3_INJECT(EffectKind18Sub3F_Flash);
    BOF3_INJECT(EffectKind18Sub3F_Spin);
    BOF3_INJECT(EffectKind18Sub3F_SpinFade);
    BOF3_INJECT(EffectKind18Sub3F_WaitCue3B);
    BOF3_INJECT(EffectKind18Sub3F_WhiteOut);
    BOF3_INJECT(EffectKind18Sub3F_ShadeClut);
    BOF3_INJECT(EffectKind18Sub3F_DrawSky);
    BOF3_INJECT(EffectKind18Sub25_Run);
    BOF3_INJECT(EffectKind18Sub25_Start);
    BOF3_INJECT(EffectKind18Sub25_WaitNear);
    BOF3_INJECT(EffectKind18Sub25_Open);
    BOF3_INJECT(EffectKind18Sub25_WaitAway);
    BOF3_INJECT(EffectKind18Sub25_Close);
    BOF3_INJECT(EffectKind18Sub25_Draw);
    BOF3_INJECT(EffectKind18Sub26_Run);
    BOF3_INJECT(EffectKind18Sub26_Start);
    BOF3_INJECT(EffectKind18Sub26_WaitNear);
    BOF3_INJECT(EffectKind18Sub26_Open);
    BOF3_INJECT(EffectKind18Sub26_Close);
    BOF3_INJECT(EffectKind18Sub26_Draw);
    BOF3_INJECT(EffectKind18Sub39_Run);
    BOF3_INJECT(EffectKind18Sub39_Start);
    BOF3_INJECT(EffectKind18Sub39_WaitFlag);
    BOF3_INJECT(EffectKind18Sub39_Open);
    BOF3_INJECT(EffectKind18Sub39_Hold);
    BOF3_INJECT(EffectKind18Sub39_Draw);
}

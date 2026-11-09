// Moved here by tools/move_funcs.py from effect_4d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_97.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4d.h"
#include "game/effect_4d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4d::at;

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

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }

unsigned char* Debris(unsigned i) { return At(::bof3::Img(EffectKind30_Shards_addr) + 0xc0 + at::kDebrisStride * i); }

unsigned char* Spark(unsigned i) { return EffectKind30_Shards + at::kSparkStride * i; }

// --- x87 as the original has it (the game's control word: round to nearest, 53
// bits); every value goes through the FPU as Capcom's does.
// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}

// `fld dword [a]; fst dword [o1]; fst dword [o2]; fstp dword [o3]` (a copy
// through the FPU, which turns a signalling NaN quiet)
void FldCopy3(const void* a, void* o1, void* o2, void* o3) {
    __asm__ volatile("flds (%0)\n\tfsts (%1)\n\tfsts (%2)\n\tfstps (%3)" : : "r"(a), "r"(o1), "r"(o2), "r"(o3) : "st", "memory");
}

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the original's 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4d.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void Step() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// mov eax, [S]; mov dl, [eax + 9]; dec dl; mov [eax + 9], dl; mov eax, [S];
// test [eax + 9]: +9 down one; answers whether it is now 0.
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page), dtd 1 - its fifth
// argument is the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
}

// cmp ax, 0; jge; 0 / cmp ax, 0xFF; jle; 0xFF / the low byte: a word clamped
// to a byte.
unsigned char Clamp(std::int32_t v) { return v < 0 ? 0 : v > 0xFF ? 0xFF : static_cast<unsigned char>(v); }

}  // namespace

// The ring at the record's point (+0x34) of its size +0x2E, shade 7: each
// state's first draw (Sprite_Current read for the word, its point the same
// read's).
void Kind97Ring() {
    unsigned char* const s = S();
    SH_CALL(EffectKind97_DrawRing)(reinterpret_cast<const long*>(s + 0x34), rv::As<records::EffectWork>(s)->work_2e, 7);
}

// The two burst states' opening: +6 up, and a spark emitted when it was a
// multiple of 4 (one every fourth frame).
void Kind97Emit() {
    unsigned char* const s = S();
    const unsigned char n = s[6];
    s[6] = static_cast<unsigned char>(n + 1);
    if ((n & 3) == 0) SH_CALL(EffectKind97_EmitSpark)();
}

// The burst states' opening: Gte_PushMatrix, EffectGte_LoadMapCamera, the
// record's point (+0x34..+0x3C, copied to a local) projected into its own
// +0x74..+0x7F, Gte_PopMatrix.
void Kind98Project() {
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    long point[4];
    point[0] = so->x;
    point[1] = so->z;
    point[2] = so->height;
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(s + 0x74));
    SH_CALL(Gte_PopMatrix)();
}

// EffectKind98_DrawBurst(+0x32, +0xC, +0x10), Sprite_Current read once.
void Kind98Burst() {
    const unsigned char* const s = S();
    SH_CALL(EffectKind98_DrawBurst)(rv::As<records::EffectWork>(s)->work_32, Word(s + 0xC), Word(s + 0x10));
}

// original 0x48D070 (Effect_KindHandlers[0x97], hidden in 0x48CD40):
// jmp [EffectKind97_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind97_Run(void) {
    Dispatch("EffectKind97_Run", AddressOf(EffectKind97_States), EffectKind97_States_count);
}

// original 0x48D090 (state 0): the ring's size word +0x2E 0; +9 = 8; +1 up.
extern "C" void __cdecl EffectKind97_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
    S()[9] = 8;
    Step();
}

// original 0x48D0B0 (state 1): the size +0x2E up 0x18; the ring; +9 down; at 0
// +9 = 0x40 and +1 up.
extern "C" void __cdecl EffectKind97_Grow(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e + 0x18u);
    Kind97Ring();
    if (CountDown()) {
        S()[9] = 0x40;
        Step();
    }
}

// original 0x48D100 (state 2): the ring; at the counter 0x903848 = 10:
// EffectKind64_ClearSparks (E3A's: the eight sparks out of use), Gte_PushMatrix,
// the 32 debris at 0x92C040 set up (EffectKind97_DebrisInit), Gte_PopMatrix;
// +6 0, +0x5D (the debris' shade) 8, +9 = 0x78, +1 up.
extern "C" void __cdecl EffectKind97_WaitCue(void) {
    Kind97Ring();
    if (At(::bof3::Img(at::kCounter.v))[0] != at::kKind97Cue) return;
    SH_CALL(EffectKind64_ClearSparks)();
    SH_CALL(Gte_PushMatrix)();
    for (unsigned i = 0; i < at::kDebrisCount; ++i) SH_CALL(EffectKind97_DebrisInit)(Debris(i));
    SH_CALL(Gte_PopMatrix)();
    S()[6] = 0;
    S()[0x5D] = 8;
    S()[9] = 0x78;
    Step();
}

// original 0x48D180 (state 3): a spark every fourth frame; the ring; the
// debris; the sparks moved; +0x5D up 1 while below 0x40 (signed); every debris'
// shade +0x2A = +0x5D (signed) >> 1; +9 down through the same pointer, tested
// through Sprite_Current read again: at 0 Transition_Start(8) and +1 up.
extern "C" void __cdecl EffectKind97_Burst(void) {
    Kind97Emit();
    Kind97Ring();
    SH_CALL(EffectKind97_DrawDebris)();
    SH_CALL(EffectKind97_MoveSparks)();
    unsigned char* const s = S();
    const auto shade = static_cast<signed char>(s[0x5D]);
    if (shade < 0x40) s[0x5D] = static_cast<unsigned char>(shade + 1);
    for (unsigned i = 0; i < at::kDebrisCount; ++i)
        SetWord(Debris(i) + 0x2A, static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(s[0x5D]) >> 1)));
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] != 0) return;
    SH_CALL(Transition_Start)(8);
    Step();
}

// original 0x48D220 (state 4): a spark every fourth frame; the debris; the
// sparks moved; once Transition_Busy is 0: Transition_Start(9),
// Draw_PassFlags 0, +1 up.
extern "C" void __cdecl EffectKind97_Fade(void) {
    Kind97Emit();
    SH_CALL(EffectKind97_DrawDebris)();
    SH_CALL(EffectKind97_MoveSparks)();
    if (Transition_Busy != 0) return;
    SH_CALL(Transition_Start)(9);
    unsigned char* const s = S();
    Draw_PassFlags = 0;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x48D270 (state 5): once Transition_Busy is 0, the chapter's
// step 0x8034E5 = 0x14 and a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind97_End(void) {
    if (Transition_Busy != 0) return;
    MoveScript_PartyRecords[0x65] = 0x14;
    SH_CALL(Effect_Release)();
}

// original 0x48D290 (Effect_KindHandlers[0x98], hidden in 0x48CD40):
// jmp [EffectKind98_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind98_Run(void) {
    Dispatch("EffectKind98_Run", AddressOf(EffectKind98_States), EffectKind98_States_count);
}

// original 0x48D2B0 (state 0): the flash level +0xC 0; +9 = 0x20; +1 up;
// Draw_PassFlags 0.
extern "C" void __cdecl EffectKind98_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    S()[9] = 0x20;
    Step();
    Draw_PassFlags = 0;
}

// original 0x48D2E0 (state 1): the level +0xC up 8, the flash drawn at it; +9
// down; at 0 the burst's radius word +0x32 = 0x140, its centre and rim levels
// +0xC and +0x10 0x100, +9 = 0x10, +1 up, Draw_PassFlags 0x1F.
extern "C" void __cdecl EffectKind98_Flash(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c + 8u);
    s = S();
    SH_CALL(EffectKind98_DrawFlash)(static_cast<int>(rv::As<records::EffectWork>(s)->work_0c));
    if (!CountDown()) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, 0x140);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x100);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0x100);
    S()[9] = 0x10;
    Step();
    Draw_PassFlags = 0x1F;
}

// original 0x48D360 (state 2): the point projected; the rim level +0x10 down
// 0x10; the burst; +9 down; at 0 +9 = 0x10 and +1 up.
extern "C" void __cdecl EffectKind98_Rise(void) {
    Kind98Project();
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, rv::As<records::EffectWork>(s)->work_10 - 0x10u);
    Kind98Burst();
    if (CountDown()) {
        S()[9] = 0x10;
        Step();
    }
}

// original 0x48D3F0 (state 3; PSX twin 0x801F3D24, call-disputed): the point
// projected; the centre level +0xC down 0x10, the radius +0x32 down 0x14; the
// burst; +9 down; at 0 Effect_Release.
extern "C" void __cdecl EffectKind98_Fall(void) {
    Kind98Project();
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 0x10u);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, rv::As<records::EffectWork>(s)->work_32 + 0xFFECu);
    Kind98Burst();
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x48D490 (point, size, shade): a draw mode (page (0x2C0, 0x100),
// blend 1, dtd 1) committed at slot 2 (0xC); EffectGte_LoadMapCamera; the
// point projected (x, y, depth) and the size (both words the argument's low
// s16) projected at it; the radius r the projected width + Frame_Counter & 1
// (s16). 32 POLY_G3s at the cursor (read for each), semi-transparent: the
// centre, the last rim point (first (x + r, y)) and the next, (x + r cos a, y
// + r sin a) for a = 0x80, 0x100, .. 0x1000 (Math_Cos / Math_Sin * r >> 12,
// added through the FPU); the depth at all three; the centre coloured (bit 2,
// bit 1, bit 0 of the shade's low byte each 0x40), the rim black; committed at
// slot 2 (0x34).
extern "C" void __cdecl EffectKind97_DrawRing(const long* point, int size, int shade) {
    DrawMode(1, 0x2C0, 0x100);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    float p[3];
    SH_CALL(EffectGte_ProjectPoint)(point, p);
    short wide[2];
    wide[1] = static_cast<short>(size);
    wide[0] = static_cast<short>(size);
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(point, wide, out);
    const auto r = static_cast<std::int16_t>(static_cast<unsigned>(out[0]) + (Frame_Counter & 1u));
    U rimx, rimy;
    FildAdd(r, &p[0], &rimx);
    std::memcpy(&rimy, &p[1], 4);
    const auto bits = static_cast<unsigned char>(shade);
    const auto red = static_cast<unsigned char>((bits & 4) << 4);
    const auto green = static_cast<unsigned char>((bits & 2) << 5);
    const auto blue = static_cast<unsigned char>((bits & 1) << 6);
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const prim = Gfx_PacketNext;
        angle += 0x80;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, &p[0], 4);
        std::memcpy(prim + 0xC, &p[1], 4);
        SetUL(prim + 0x18, rimx);
        SetUL(prim + 0x1C, rimy);
        const U a = angle & 0xFFFFu;
        const U c = Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))), r), 12);
        FildAdd(static_cast<std::int32_t>(c), &p[0], &rimx);
        const U sn = Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))), r), 12);
        FildAdd(static_cast<std::int32_t>(sn), &p[1], &rimy);
        SetUL(prim + 0x28, rimx);
        SetUL(prim + 0x2C, rimy);
        std::memcpy(prim + 0x30, &p[2], 4);
        std::memcpy(prim + 0x20, &p[2], 4);
        std::memcpy(prim + 0x10, &p[2], 4);
        prim[4] = red;
        prim[5] = green;
        prim[6] = blue;
        prim[0x14] = prim[0x15] = prim[0x16] = 0;
        prim[0x24] = prim[0x25] = prim[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x34);
    }
}

// original 0x48D650: the first of the eight sparks (0x18 bytes at
// EffectKind30_Shards) whose +0 is 0, if any: +0 1, +1 0, its life +2 0x20, its
// point +4..+0xC Sprite_Current's +0x34..+0x3C, its size word +0x14 0x180, its
// centre shade +0x16 Sprite_Current's +0x5D, its rim +0x17 0.
extern "C" void __cdecl EffectKind97_EmitSpark(void) {
    unsigned char* r = nullptr;
    for (unsigned i = 0; i < at::kSparkCount; ++i)
        if (Spark(i)[0] == 0) {
            r = Spark(i);
            break;
        }
    if (r == nullptr) return;
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    r[0] = 1;
    r[1] = 0;
    r[2] = 0x20;
    SetUL(r + 4, static_cast<std::uint32_t>(so->x));
    SetUL(r + 8, static_cast<std::uint32_t>(so->z));
    SetUL(r + 0xC, static_cast<std::uint32_t>(so->height));
    SetWord(r + 0x14, 0x180);
    r[0x16] = s[0x5D];
    r[0x17] = 0;
}

// original 0x48D6A0: EffectGte_LoadMapCamera; each live spark (+0 not 0) its
// size +0x14 down 0xC, its life +2 down (at 0 +0 = 0, out of use) and drawn
// (EffectKind97_DrawSpark) - the frame it dies too. Answers in al 1 when any
// was live, else 0.
extern "C" unsigned char __cdecl EffectKind97_MoveSparks(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const r = Spark(i);
        if (r[0] == 0) continue;
        SetWord(r + 0x14, Word(r + 0x14) + 0xFFF4u);
        const auto life = static_cast<unsigned char>(r[2] - 1);
        any = 1;
        r[2] = life;
        if (life == 0) r[0] = 0;
        SH_CALL(EffectKind97_DrawSpark)(r);
    }
    return any;
}

// original 0x48D6F0 (spark): a draw mode (page (0x3C0, 0), blend 1, dtd 1)
// committed at slot 1 (0xC); the spark's size (+0x14, one word: the second the
// original never writes, a projected half nobody reads) projected at its point
// +4, then the point; the radius r the projected width + Frame_Counter & 1
// (s16). 32 POLY_G3s at the cursor (read for each), semi-transparent: the
// centre and the rim points at a and a + 0x80 (a = 0, 0x80, .. 0xF80; Math_Cos
// / Math_Sin * r >> 12 through the FPU); the depth at all three; the centre
// coloured the spark's +0x16 (grey), the rim +0x17; committed at slot 1 (0x34).
extern "C" void __cdecl EffectKind97_DrawSpark(unsigned char* spark) {
    DrawMode(1, 0x3C0, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    short wide[2];
    wide[0] = static_cast<short>(Word(spark + 0x14));
    wide[1] = wide[0];   // the original's stale stack word (docs/effect_4d.md section 6)
    short out[2];
    const long* const point = reinterpret_cast<const long*>(spark + 4);
    SH_CALL(EffectGte_ProjectSize)(point, wide, out);
    float p[3];
    SH_CALL(EffectGte_ProjectPoint)(point, p);
    const auto r = static_cast<std::int16_t>(static_cast<unsigned>(out[0]) + (Frame_Counter & 1u));
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, &p[0], 4);
        std::memcpy(prim + 0xC, &p[1], 4);
        std::int32_t v = static_cast<std::int32_t>(Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))), r), 12));
        FildAdd(v, &p[0], prim + 0x18);
        v = static_cast<std::int32_t>(Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), r), 12));
        angle += 0x80;
        FildAdd(v, &p[1], prim + 0x1C);
        v = static_cast<std::int32_t>(Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))), r), 12));
        FildAdd(v, &p[0], prim + 0x28);
        v = static_cast<std::int32_t>(Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), r), 12));
        FildAdd(v, &p[1], prim + 0x2C);
        std::memcpy(prim + 0x30, &p[2], 4);
        std::memcpy(prim + 0x20, &p[2], 4);
        std::memcpy(prim + 0x10, &p[2], 4);
        const unsigned char centre = spark[0x16];
        prim[6] = prim[5] = prim[4] = centre;
        const unsigned char rim = spark[0x17];
        prim[0x26] = prim[0x25] = prim[0x24] = rim;
        prim[0x16] = prim[0x15] = prim[0x14] = rim;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x48D860 (a 0x2C-byte debris record; EffectDebris_InitOne's twin
// with 0x18, a y angle of -0x400..0x3FF and 4 + Rand % 4): its point +0 / +4 /
// +8 Sprite_Current's +0x34 / +0x38 / +0x3C; three angles Rand & 0xFFF, (Rand &
// 0x7FF) - 0x400, Rand & 0xFFF; two edges (Math_Cos, Math_Sin of 0x18; of
// -0x18; z 0) at +0x10 and +0x18 turned in place by a matrix of the three
// angles (EffectGte_SetDiagonalOne, Gte_RotMatrixX, _Y by the second negated,
// _Z; 0x5A7C70 for each edge); the scale +0x28 4 + Rand & 3; +0x20, +0x22,
// +0x24 (its angle), +0x2A (its shade) 0.
extern "C" void __cdecl EffectKind97_DebrisInit(unsigned char* d) {
    SetUL(d + 0, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetUL(d + 4, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    SetUL(d + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height));
    const U ax = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U ay = (static_cast<U>(SH_CALL(Rand)()) & 0x7FFu) - 0x400u;
    const U az = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    SetWord(d + 0x10, static_cast<U>(SH_CALL(Math_Cos)(0x18)));
    SetWord(d + 0x12, static_cast<U>(SH_CALL(Math_Sin)(0x18)));
    SetWord(d + 0x14, 0);
    SetWord(d + 0x18, static_cast<U>(SH_CALL(Math_Cos)(-0x18)));
    SetWord(d + 0x1A, static_cast<U>(SH_CALL(Math_Sin)(-0x18)));
    Matrix m;
    SetWord(d + 0x1C, 0);
    SH_CALL(EffectGte_SetDiagonalOne)(m.m);
    SH_CALL(Gte_RotMatrixX)(static_cast<short>(ax), m.m);
    SH_CALL(Gte_RotMatrixY)(-static_cast<int>(static_cast<short>(ay)), m.m);
    SH_CALL(Gte_RotMatrixZ)(static_cast<short>(az), m.m);
    using Turn = void (__cdecl*)(const short*, const short*, short*);
    SH_AT(Turn, at::kMatrixVector.v)(m.m, reinterpret_cast<const short*>(d + 0x10), reinterpret_cast<short*>(d + 0x10));
    SH_AT(Turn, at::kMatrixVector.v)(m.m, reinterpret_cast<const short*>(d + 0x18), reinterpret_cast<short*>(d + 0x18));
    SetWord(d + 0x28, (static_cast<U>(SH_CALL(Rand)()) & 3u) + 4u);
    SetWord(d + 0x20, 0);
    SetWord(d + 0x22, 0);
    SetWord(d + 0x24, 0);
    SetWord(d + 0x2A, 0);
}

// original 0x48D980: a draw mode (page (0x380, 0x100), blend 1, dtd 1)
// committed at slot 1 (0xC); EffectGte_LoadMapCamera; each of the 32 debris at
// 0x92C040 drawn (EffectKind97_DebrisDraw), its angle +0x24 up 0x10.
extern "C" void __cdecl EffectKind97_DrawDebris(void) {
    DrawMode(1, 0x380, 0x100);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kDebrisCount; ++i) {
        unsigned char* const d = Debris(i);
        SH_CALL(EffectKind97_DebrisDraw)(d);
        SetWord(d + 0x24, Word(d + 0x24) + 0x10u);
    }
}

// original 0x48D9F0 (a 0x2C-byte debris record; EffectDebris_Draw's twin, its
// first corner grey): a POLY_G3 at the cursor (read once), semi-transparent;
// its first corner the record's point (+0..+0xB) projected; the other two the
// edges +0x10 / +0x18 (three s16 each) turned in x / z by the angle +0x24
// (Math_Cos / Math_Sin, >> 8) and scaled by +0x28, the height (+0x14 / +0x1C)
// << 12 times the scale, added to the point and projected. Colour (v, v, v) at
// the first corner, v the shade +0x2A clamped to 0..0xFF; black at the others.
// Gfx_CommitPrim(1, 0x34).
extern "C" void __cdecl EffectKind97_DebrisDraw(unsigned char* d) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(d), reinterpret_cast<float*>(prim + 8));
    for (U edge = 0x10, out = 0x18; edge <= 0x18; edge += 8, out += 0x10) {
        const U c1 = static_cast<U>(SH_CALL(Math_Cos)(SW(d + 0x24)));
        U e = Mul(c1, SW(d + edge));
        const U s1 = static_cast<U>(SH_CALL(Math_Sin)(SW(d + 0x24)));
        const std::int32_t scale = SW(d + 0x28);
        e = Mul(Sar(e - Mul(s1, SW(d + edge + 2)), 8), scale);
        const U s2 = static_cast<U>(SH_CALL(Math_Sin)(SW(d + 0x24)));
        U f = Mul(s2, SW(d + edge));
        const U c2 = static_cast<U>(SH_CALL(Math_Cos)(SW(d + 0x24)));
        f += Mul(c2, SW(d + edge + 2));
        long q[3];
        q[0] = static_cast<long>(e + UL(d + 0));
        q[2] = static_cast<long>(Mul(static_cast<U>(SW(d + edge + 4)) << 12, scale) + UL(d + 8));
        q[1] = static_cast<long>(Mul(Sar(f, 8), scale) + UL(d + 4));
        SH_CALL(EffectGte_ProjectPoint)(q, reinterpret_cast<float*>(prim + out));
    }
    const unsigned char v = Clamp(SW(d + 0x2A));
    prim[4] = v;
    prim[5] = v;
    prim[6] = v;
    prim[0x14] = prim[0x15] = prim[0x16] = 0;
    prim[0x24] = prim[0x25] = prim[0x26] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x34);
}

// original 0x48DBA0 (level): the argument's low s16 clamped to 0..0xFF; a draw
// mode (page (0x380, 0x100), blend 1, dtd 1) committed at slot 1 (0xC); a
// semi-transparent TILE at the cursor (read after the commit) at (0, 0) 320.0
// x 240.0 - DIV-0041's fill - grey (level, level, level); committed at slot 1
// (0x1C).
extern "C" void __cdecl EffectKind98_DrawFlash(int level) {
    const unsigned char v = Clamp(static_cast<std::int16_t>(level));
    DrawMode(1, 0x380, 0x100);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SetUL(prim + 8, std::bit_cast<std::uint32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetUL(prim + 0x18, 0x43700000u);   // 240.0f
    prim[4] = v;
    prim[5] = v;
    prim[6] = v;
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x48DC40 (radius, centre, rim; PSX twin 0x801F4138, call-anchored):
// the centre and rim levels (low s16) clamped to 0..0xFF; a draw mode (page
// (0x380, 0x100), blend 1, dtd 1) committed at slot 1 (0xC); 32 POLY_G3s at
// the cursor (read for each), semi-transparent, round Sprite_Current's
// projected point +0x74 / +0x78 (read afresh at each use): the rim points at a
// and a + 0x80 (a = 0, 0x80, .. 0xF80; Math_Cos / Math_Sin * radius >> 12
// through the FPU); the depth +0x7C copied through the FPU to all three; the
// centre coloured the centre level, the rim the rim level; committed at slot 1
// (0x34).
extern "C" void __cdecl EffectKind98_DrawBurst(int radius, int centre, int rim) {
    const unsigned char middle = Clamp(static_cast<std::int16_t>(centre));
    const unsigned char edge = Clamp(static_cast<std::int16_t>(rim));
    DrawMode(1, 0x380, 0x100);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const std::int32_t r = static_cast<std::int16_t>(radius);
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SetUL(prim + 8, rv::Current<records::EffectObject>()->screen_xf);
        SetUL(prim + 0xC, rv::Current<records::EffectObject>()->screen_yf);
        const U a = angle & 0xFFFFu;
        std::int32_t v = static_cast<std::int32_t>(Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))), r), 12));
        FildAdd(v, S() + 0x74, prim + 0x18);
        v = static_cast<std::int32_t>(Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))), r), 12));
        angle += 0x80;
        const U b = angle & 0xFFFFu;
        FildAdd(v, S() + 0x78, prim + 0x1C);
        v = static_cast<std::int32_t>(Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(b))), r), 12));
        FildAdd(v, S() + 0x74, prim + 0x28);
        v = static_cast<std::int32_t>(Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(b))), r), 12));
        FildAdd(v, S() + 0x78, prim + 0x2C);
        FldCopy3(S() + 0x7C, prim + 0x30, prim + 0x20, prim + 0x10);
        prim[4] = prim[5] = prim[6] = middle;
        prim[0x14] = prim[0x15] = prim[0x16] = edge;
        prim[0x24] = prim[0x25] = prim[0x26] = edge;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

void EffectKind97_Inject() {
    BOF3_INJECT(EffectKind97_Run);
    BOF3_INJECT(EffectKind97_Start);
    BOF3_INJECT(EffectKind97_Grow);
    BOF3_INJECT(EffectKind97_WaitCue);
    BOF3_INJECT(EffectKind97_Burst);
    BOF3_INJECT(EffectKind97_Fade);
    BOF3_INJECT(EffectKind97_End);
    BOF3_INJECT(EffectKind98_Run);
    BOF3_INJECT(EffectKind98_Start);
    BOF3_INJECT(EffectKind98_Flash);
    BOF3_INJECT(EffectKind98_Rise);
    BOF3_INJECT(EffectKind98_Fall);
    BOF3_INJECT(EffectKind97_DrawRing);
    BOF3_INJECT(EffectKind97_EmitSpark);
    BOF3_INJECT(EffectKind97_MoveSparks);
    BOF3_INJECT(EffectKind97_DrawSpark);
    BOF3_INJECT(EffectKind97_DebrisInit);
    BOF3_INJECT(EffectKind97_DrawDebris);
    BOF3_INJECT(EffectKind97_DebrisDraw);
    BOF3_INJECT(EffectKind98_DrawFlash);
    BOF3_INJECT(EffectKind98_DrawBurst);
}

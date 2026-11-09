// Moved here by tools/move_funcs.py from effect_2e.cpp, effect_2f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_4e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_4e_callees.h"
#include "game/effect_2e.h"
#include "game/effect_2e_callees.h"
#include "game/effect_2f.h"
#include "game/effect_2f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_4e::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + byte]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through
// the dword after - the next table or data.
void DispatchEffect2e(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned index = Sprite_Current[byte];
    if (index >= entries)
        bof3::Fatal("%s: byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2e.md section 2)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * index))))();
}

void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// The byte +9 down one; true at 0.
bool CountDown9(unsigned char** out) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    *out = s;
    return s[9] == 0;
}

// E2F's disc (0x47B7D0) at the record's screen point: radius (read as an s16;
// the callers push a whole register), centre 0x80, rim 0.
void Disc(unsigned radius) { SH_AT(void (__cdecl*)(unsigned, unsigned, unsigned), at::kDiscDraw.v)(radius, 0x80, 0); }

using LD = long double;

using move_script::SetLong;

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); }

// imul then sar 12: a trigonometric answer scaled by a length.
U Scale(int trig, U length) { return Sar(static_cast<U>(trig) * length, 12); }

// --- x87 as the original has it -------------------------------------------------
// `fld dword [p]`: the float widened (a signalling NaN comes out quiet, as fld
// makes it).
LD X(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return static_cast<LD>(f);
}

// `fild dword`: an int widened, exactly.
LD XI(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }

// `fstp dword [p]`: rounded to a float at the control word's rounding.
void Fstp(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// `fld dword [src]; fstp dword [dst]`: a copy through the FPU (a signalling NaN
// quietened), kept in assembly so the compiler cannot fold it into a move.
void FCopy(unsigned char* dst, const unsigned char* src) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(dst), "r"(src) : "st", "memory");
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Jump(const char* who, U table, unsigned entries, unsigned index) {
    if (index >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2f.md section 2)",
                    who, index, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * index))))();
}

// A Sprite_Objects record by the index a state keeps: a Fatal past the thirty
// (Sprite_FindFree answers 0..29 or 0xFF; the original would write past the pool).
unsigned char* Sprite(unsigned index, const char* who) {
    if (index >= at::kSprites)
        bof3::Fatal("%s: Sprite_Objects index %u, past the 30 records - the original writes past the pool "
                    "(docs/effect_2f.md section 2)",
                    who, index);
    return rv::Bytes(&Sprite_Objects[index]);
}

// Gpu_GetTPage(tp, abr, x, y), then the draw mode of that page at the packet
// cursor (dfe 0, dtd as given), committed to slot `slot` (0xC bytes).
void DrawMode(unsigned tp, unsigned abr, int x, int y, int dtd, unsigned slot) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(tp, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

}  // namespace

// original 0x47B630: jmp [EffectKind4E_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4E_Run(void) {
    DispatchEffect2e("EffectKind4E_Run", AddressOf(EffectKind4E_States), EffectKind4E_States_count, 1);
}

// original 0x47B650 (state 0): the leader's point as the camera vector ((x sar
// 9) - 0x4000, (z sar 9) - 0x4000, -(height / 2)) in Prim_VertexScratch,
// Gte_RotTransPers into the record's +0x74 (the screen x, y floats; the
// record read before the call), its answer to +0x60 and Gte_StoreDepthF to
// +0x7C (the record read after); +9 = 0, +1 up. The original hands a fourth
// argument, a stack slot the callee never reads.
extern "C" void __cdecl EffectKind4E_Start(void) {
    const U x = static_cast<std::uint32_t>(ObjTrio[0].x);
    const U z = static_cast<std::uint32_t>(ObjTrio[0].z);
    Prim_VertexScratch[0].x = static_cast<short>(Sar(x, 9) - 0x4000u);
    const std::int32_t height = static_cast<std::int16_t>(ObjTrio[0].height_whole);
    Prim_VertexScratch[0].y = static_cast<short>(Sar(z, 9) - 0x4000u);
    Prim_VertexScratch[0].z = static_cast<short>(-(height / 2));
    long depth = 0;
    const long answer = SH_CALL(Gte_RotTransPers)(reinterpret_cast<short*>(Prim_VertexScratch), reinterpret_cast<unsigned long*>(S() + 0x74), &depth);
    rv::Current<records::EffectObject>()->depth = static_cast<U>(answer);
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(S() + 0x7C));
    S()[9] = 0;
    NextState();
}

// original 0x47B6E0 (state 1): the disc, radius +9; +9 up 0xF; above 0x3C
// (unsigned), +9 = 0xFF and +1 up.
extern "C" void __cdecl EffectKind4E_Grow(void) {
    Disc(S()[9]);
    unsigned char* t = S();
    t[9] = static_cast<unsigned char>(t[9] + 0xF);
    t = S();
    if (t[9] <= 0x3C) return;
    t[9] = 0xFF;
    NextState();
}

// original 0x47B720 (state 2): the disc, radius 0x3C + (+9 & 1); at +9 = 0xD7
// Sound_PlayEffect(0x203); +9 down, at 0 +9 = 0x3C and +1 up.
extern "C" void __cdecl EffectKind4E_Glow(void) {
    Disc(0x3Cu + (S()[9] & 1u));
    if (S()[9] == 0xD7) SH_CALL(Sound_PlayEffect)(0x203);
    unsigned char* s;
    if (!CountDown9(&s)) return;
    s[9] = 0x3C;
    NextState();
}

// original 0x47B790 (state 3): the disc, radius +9; +9 down; below 0 (s8), +1
// up (to Effect_StateRelease).
extern "C" void __cdecl EffectKind4E_Shrink(void) {
    Disc(S()[9]);
    unsigned char* t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    t = S();
    if (static_cast<signed char>(t[9]) >= 0) return;
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x47B7D0 (0x19D bytes): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1, committed 0xC); then for a = 0, 0x80, .. 0xF80 (b = a + 0x80 & 0xFFF) a
// Gouraud triangle at the packet cursor (read afresh): Gpu_SetPolyG3,
// Gpu_SetSemiTrans(1), Gte_StoreDepthF(prim + 0x10) copied to +0x30 and +0x20;
// vertex 0 the record's screen point (+0x74, +0x78, copied as they are); vertex 1
// its x + (cos(a) r sar 12), its y + (sin(a) r sar 12) (floats, fild + fadd);
// vertex 2 likewise at b; the depth +0x7C to +0x30, +0x20, +0x10; vertex 0
// shaded `centre`, 1 and 2 `rim`; committed 0x34. Sprite_Current is read afresh
// after every call, as the original reads it. r is read as s16, the shades as
// bytes (the callers push whole registers).
extern "C" void __cdecl EffectKind4E_DrawDisc(unsigned radius, unsigned centre, unsigned rim) {
    DrawMode(0, 1, 0x3C0, 0, 1, 1);
    const U r = S16(radius);
    const auto rim_shade = static_cast<unsigned char>(rim);
    const auto centre_shade = static_cast<unsigned char>(centre);
    for (U a = 0; a < 0x1000; a += 0x80) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
        FCopy(prim + 0x30, prim + 0x10);
        FCopy(prim + 0x20, prim + 0x10);
        const U b = (a + 0x80) & 0xFFFu;
        std::memcpy(prim + 8, S() + 0x74, 4);
        std::memcpy(prim + 0xC, S() + 0x78, 4);
        U v = Scale(SH_CALL(Math_Cos)(static_cast<int>(a)), r);
        Fstp(prim + 0x18, XI(v) + X(S() + 0x74));
        v = Scale(SH_CALL(Math_Sin)(static_cast<int>(a)), r);
        Fstp(prim + 0x1C, XI(v) + X(S() + 0x78));
        v = Scale(SH_CALL(Math_Cos)(static_cast<int>(b)), r);
        Fstp(prim + 0x28, XI(v) + X(S() + 0x74));
        v = Scale(SH_CALL(Math_Sin)(static_cast<int>(b)), r);
        Fstp(prim + 0x2C, XI(v) + X(S() + 0x78));
        const unsigned char* const s = S();
        prim[4] = centre_shade;
        prim[5] = centre_shade;
        FCopy(prim + 0x30, s + 0x7C);
        FCopy(prim + 0x20, s + 0x7C);
        FCopy(prim + 0x10, s + 0x7C);
        prim[6] = centre_shade;
        prim[0x14] = prim[0x15] = prim[0x16] = rim_shade;
        prim[0x24] = prim[0x25] = prim[0x26] = rim_shade;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x47B970 (hidden in 0x47B7D0's recorded extent): jmp
// [EffectKind4F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4F_Run(void) {
    Jump("EffectKind4F_Run", AddressOf(EffectKind4F_States), EffectKind4F_States_count, S()[1]);
}

// original 0x47B990 (EffectKind4F_States[0]): FC1's EffectKind3C_Start with the
// op EffectKind4F_Op. Two free sprites into +3 and +4 (none: nothing, the first
// given back when only the second fails), each marked in use; for each,
// SharedScratch's word = its index and EventOp_0x(EffectKind4F_Op); Sprite_Current
// put back; the first moved by (-0x1000, +0x1000), the second by (+0x1000,
// -0x1000), both +0 |= 0x20, +0x5C = 0, tinted (0xF, 0, 0, 1) and (0, 0, 0xF, 1);
// +9 = 4 and +1 = 1 (Sprite_Current read again after the tints).
extern "C" void __cdecl EffectKind4F_Start(void) {
    const char* const who = "EffectKind4F_Start";
    S()[3] = SH_CALL(Sprite_FindFree)();
    if (S()[3] == 0xFF) return;
    Sprite(S()[3], who)[0] = 1;
    S()[4] = SH_CALL(Sprite_FindFree)();
    unsigned char* const self = S();
    if (self[4] == 0xFF) {
        Sprite(self[3], who)[0] = 0;
        return;
    }
    Sprite(self[4], who)[0] = 1;
    SharedScratch.w_00 = self[3];
    SH_CALL(EventOp_0x)(EffectKind4F_Op);
    SharedScratch.w_00 = self[4];
    SH_CALL(EventOp_0x)(EffectKind4F_Op);
    Sprite_Current = self;
    unsigned char* a = Sprite(self[3], who);
    SetUL(a + 0x34, UL(a + 0x34) + 0xFFFFF000u);
    a = Sprite(self[3], who);
    SetUL(a + 0x38, UL(a + 0x38) + 0x1000u);
    unsigned char* b = Sprite(self[4], who);
    SetUL(b + 0x34, UL(b + 0x34) + 0x1000u);
    b = Sprite(self[4], who);
    SetUL(b + 0x38, UL(b + 0x38) + 0xFFFFF000u);
    a = Sprite(self[3], who);
    a[0] = static_cast<unsigned char>(a[0] | 0x20);
    b = Sprite(self[4], who);
    b[0] = static_cast<unsigned char>(b[0] | 0x20);
    Sprite(self[4], who)[0x5C] = 0;
    Sprite(self[3], who)[0x5C] = 0;
    SH_CALL(Sprite_SetTint)(Sprite(self[3], who), 0xF, 0, 0, 1);
    SH_CALL(Sprite_SetTint)(Sprite(self[4], who), 0, 0, 0xF, 1);
    S()[9] = 4;
    S()[1] = 1;
}

// original 0x47BB70: jmp [EffectKind50_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind50_Run(void) {
    Jump("EffectKind50_Run", AddressOf(EffectKind50_States), EffectKind50_States_count, S()[1]);
}

// original 0x47BB90 (state 0): the specks cleared, the direction dword +0xC =
// 0, +1 up.
extern "C" void __cdecl EffectKind50_Start(void) {
    SH_CALL(EffectKind50_ClearSpecks)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    NextState();
}

// original 0x47BBB0 (state 1): one frame in four (Frame_Counter's low two bits
// 0) a speck spawned; the specks moved and drawn; the direction +0xC turned
// 0x80 (& 0xFFF); +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind50_Emit(void) {
    if ((Frame_Counter & 3u) == 0) SH_CALL(EffectKind50_SpawnSpeck)();
    SH_CALL(EffectKind50_MoveSpecks)();
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, (rv::As<records::EffectWork>(s)->work_0c + 0x80u) & 0xFFFu);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x47BC00 (state 2): the specks moved and drawn; once none was in
// use, a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind50_Drain(void) {
    if (SH_CALL(EffectKind50_MoveSpecks)() != 0) SH_CALL(Effect_Release)();
}

void EffectKind4e_InjectFromEffect2e() {
    BOF3_INJECT(EffectKind4E_Run);
    BOF3_INJECT(EffectKind4E_Start);
    BOF3_INJECT(EffectKind4E_Grow);
    BOF3_INJECT(EffectKind4E_Glow);
    BOF3_INJECT(EffectKind4E_Shrink);
}

void EffectKind4e_InjectFromEffect2f() {
    BOF3_INJECT(EffectKind4E_DrawDisc);
    BOF3_INJECT(EffectKind4F_Run);
    BOF3_INJECT(EffectKind4F_Start);
    BOF3_INJECT(EffectKind50_Run);
    BOF3_INJECT(EffectKind50_Start);
    BOF3_INJECT(EffectKind50_Emit);
    BOF3_INJECT(EffectKind50_Drain);
}

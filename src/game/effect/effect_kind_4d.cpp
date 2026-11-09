// Moved here by tools/move_funcs.py from effect_2e.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_4d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2e.h"
#include "game/effect_2e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2e::at;

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

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t SL(const unsigned char* p) { return Long(p); }

U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }

void AddWord(unsigned char* p, U d) { SetWord(p, Word(p) + d); }

void AddLong(unsigned char* p, U d) { SetUL(p, UL(p) + d); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// `fild dword i; fadd dword [f]; fstp dword [out]`, on the x87 as the original
// does it: the precision and rounding the control word holds round the sum
// (magic_s32.cpp's AddIntToFloat idiom).
void StoreSum(unsigned char* out, U whole, const unsigned char* f) {
    const auto i = static_cast<std::int32_t>(whole);
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(out))
                     : "m"(i), "m"(*reinterpret_cast<const float*>(f))
                     : "st");
}

void Copy12(unsigned char* to, const void* from) { std::memcpy(to, from, 12); }

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

// Sprite_Current's +0x3C = the ground under its +0x34 / +0x38 (AreaMap_Elevation's
// low word, sign-extended, plus `lift`) << 16, the record read again after the call.
void Ground(U lift) {
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    const long g = SH_CALL(AreaMap_Elevation)(so->x, so->z);
    rv::Store<U>(rv::Current<records::EffectObject>()->height, (SW(reinterpret_cast<const unsigned char*>(&g)) + lift) << 16);
}

// A draw mode: Gpu_GetTPage(0, 1, x, y) into Gpu_SetDrawMode(packet, 0, dtd,
// it, 0), committed 0xC.
void DrawMode(int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
}

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32.
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};

}  // namespace

// original 0x47AA50: jmp [EffectKind4B_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4B_Run(void) {
    DispatchEffect2e("EffectKind4B_Run", AddressOf(EffectKind4B_States), EffectKind4B_States_count, 1);
}

// original 0x47AA70 (state 0): the record at the cell x 0x738000, z 0xB8000,
// its height the ground there + 0x80 (<< 16); Gte_PushMatrix, the 32 debris
// set (EffectKind4B_DebrisInit), Gte_PopMatrix; +0xC = 0, +1 up.
extern "C" void __cdecl EffectKind4B_Start(void) {
    rv::Current<records::EffectObject>()->x = 0x738000;
    rv::Current<records::EffectObject>()->z = 0xB8000;
    Ground(0x80);
    SH_CALL(Gte_PushMatrix)();
    for (unsigned i = 0; i < at::kDebrisCount; ++i) SH_CALL(EffectKind4B_DebrisInit)(At(::bof3::Img(at::kDebris.v) + at::kDebrisStride * i));
    SH_CALL(Gte_PopMatrix)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    NextState();
}

// original 0x47AAF0 (state 1): a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100),
// Gpu_SetDrawMode(packet, 0, 0, it, 0), committed 0xC); Gte_PushMatrix,
// EffectGte_LoadMapCamera; each debris drawn (E3C's 0x485030), its angle +0x24
// up 0x10 and its shade +0x2A up 0x20 while the record's +9 is below 4, else
// down 2; Gte_PopMatrix; +0xC up, above 0xB8 (signed) a tail jmp to
// Effect_Release.
extern "C" void __cdecl EffectKind4B_Scatter(void) {
    DrawMode(0x380, 0x100, 0);
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kDebrisCount; ++i) {
        unsigned char* const d = At(::bof3::Img(at::kDebris.v) + at::kDebrisStride * i);
        SH_AT(void (__cdecl*)(unsigned char*), at::kDebrisDraw.v)(d);
        const unsigned char* const s = S();
        AddWord(d + 0x24, 0x10);
        AddWord(d + 0x2A, s[9] < 4 ? 0x20u : 0xFFFEu);
    }
    SH_CALL(Gte_PopMatrix)();
    unsigned char* s = S();
    AddLong(s + 0xC, 1);
    s = S();
    if (SL(s + 0xC) <= 0xB8) return;
    SH_CALL(Effect_Release)();
}

// original 0x47ABA0: jmp [EffectKind4C_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4C_Run(void) {
    DispatchEffect2e("EffectKind4C_Run", AddressOf(EffectKind4C_States), EffectKind4C_States_count, 1);
}

// original 0x47ABC0 (state 0): the record at the cell x 0x740000, z 0x60000,
// its height the ground there (<< 16); +0xC = 300 frames, +1 up.
extern "C" void __cdecl EffectKind4C_Start(void) {
    rv::Current<records::EffectObject>()->x = 0x740000;
    rv::Current<records::EffectObject>()->z = 0x60000;
    Ground(0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x12C);
    NextState();
}

// original 0x47AC20 (state 1): at +0xC % 30 = 0 Sound_PlayEffect(0x20B); while
// +0xC % 30 is below 15 (signed idiv) a spark line (EffectKind4C_DrawSpark); +0xC
// down, at 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind4C_Crackle(void) {
    unsigned char* s = S();
    if (SL(s + 0xC) % 30 == 0) {
        SH_CALL(Sound_PlayEffect)(0x20B);
        s = S();
    }
    if (SL(s + 0xC) % 30 < 15) {
        SH_CALL(EffectKind4C_DrawSpark)();
        s = S();
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1u);
    if (rv::As<records::EffectWork>(S())->work_0c != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x47AC80 (0x3F bytes): the trail's turn word 0x92CC80 and spin byte
// 0x92CC82 = 0; each of its 64 points at Sprite_Current's point (read once),
// its angle word +0x30 = 0.
extern "C" void __cdecl EffectKind4A_TrailInit(void) {
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    Effect2e_TrailTurn = 0;
    Effect2e_TrailSpin = 0;
    for (unsigned i = 0; i < at::kTrailCount; ++i) {
        unsigned char* const p = At(::bof3::Img(EffectKind30_Shards_addr) + at::kTrailStride * i);
        SetUL(p, static_cast<std::uint32_t>(so->x));
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->z, static_cast<std::uint32_t>(so->z));
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->height, static_cast<std::uint32_t>(so->height));
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->angle, 0);
    }
}

// original 0x47ACC0 (0xF9 bytes): the trail's points moved one on (62 down to 0
// copied over the next, 0x34 bytes each); the newest at Sprite_Current's +0x34
// / +0x38 and +0x3C + Math_Sin(~(Frame_Counter << 7) & 0xFFFF) << 10, its angle
// word the turn word 0x92CC80 - which, while the spin byte is set, turns 0x40
// (the byte cleared when it comes round to 0). Then each point's two edge
// points: c = Math_Cos(its angle) << 3, s = Math_Sin(its angle) << 3; +0x10 /
// +0x14 / +0x18 = x, z + c, height + (s << 8); +0x20 / +0x24 / +0x28 = x, z -
// c, height - (s << 8).
extern "C" void __cdecl EffectKind4A_TrailStep(void) {
    for (int i = static_cast<int>(at::kTrailCount) - 2; i >= 0; --i)
        std::memcpy(At(::bof3::Img(EffectKind30_Shards_addr) + at::kTrailStride * (i + 1)), At(::bof3::Img(EffectKind30_Shards_addr) + at::kTrailStride * i), at::kTrailStride);
    const U frame = Frame_Counter;
    const unsigned char* s = S();
    unsigned char* const head = At(::bof3::Img(EffectKind30_Shards_addr));
    SetUL(head, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x));
    rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(head)->z, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    const U bob = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(~(frame << 7) & 0xFFFFu)));
    s = S();
    const unsigned char spin = Effect2e_TrailSpin;
    rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(head)->height, (bob << 10) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(head)->angle, Effect2e_TrailTurn);
    if (spin != 0) {
        AddWord(At(::bof3::Img(Effect2e_TrailTurn_addr)), 0x40);
        if (Effect2e_TrailTurn == 0) Effect2e_TrailSpin = 0;
    }
    for (unsigned i = 0; i < at::kTrailCount; ++i) {
        unsigned char* const p = At(::bof3::Img(EffectKind30_Shards_addr) + at::kTrailStride * i);
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(rv::As<records::EffectTrailPoint>(p)->angle))) << 3;
        const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(rv::As<records::EffectTrailPoint>(p)->angle))) << 3;
        const U x = UL(p);
        const U z = static_cast<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->z);
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->edge0_z, z + c);
        const U height = static_cast<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->height);
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->edge0_x, x);
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->edge0_height, (n << 8) + height);
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->edge1_x, x);
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->edge1_z, z - c);
        rv::Store<std::uint32_t>(rv::As<records::EffectTrailPoint>(p)->edge1_height, height - (n << 8));
    }
}

// original 0x47ADC0 (0x144 bytes): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// committed 0xC), EffectGte_LoadMapCamera; the newest point's edge points
// projected (A, B); then for points 1..63 a Gouraud quad at the packet cursor
// (Gpu_SetPolyG4, Gpu_SetSemiTrans(1)): A and B as they stand, then the point's
// own edge points projected into A and B; the red shade 0x40 falling by one a
// quad, green and blue half the red (the first pair's green and blue 0x20);
// committed 0x44.
extern "C" void __cdecl EffectKind4A_TrailDraw(void) {
    DrawMode(0x3C0, 0, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    float a[3];
    float b[3];
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(::bof3::Img(EffectKind30_Shards_addr) + 0x10)), a);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(::bof3::Img(EffectKind30_Shards_addr) + 0x20)), b);
    unsigned char red = 0x40;
    unsigned char half = 0x20;
    U edge = ::bof3::Img(EffectKind30_Shards_addr) + 0x20;
    for (unsigned n = 0; n < at::kTrailCount - 1; ++n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        edge += at::kTrailStride;
        Copy12(prim + 8, a);
        Copy12(prim + 0x18, b);
        prim[0x14] = prim[4] = red;
        prim[0x16] = prim[0x15] = prim[6] = prim[5] = half;
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(edge - 0x10)), a);
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(edge)), b);
        Copy12(prim + 0x28, a);
        Copy12(prim + 0x38, b);
        red = static_cast<unsigned char>(red - 1);
        prim[0x34] = prim[0x24] = red;
        half = static_cast<unsigned char>(red >> 1);
        prim[0x36] = prim[0x35] = prim[0x26] = prim[0x25] = half;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

// original 0x47AF10 (0x15F bytes): `point` projected (L: x, y, depth); its
// radius EffectGte_ProjectSize(point, {size's low word, the high word of
// point's address}, out) - the original hands the callee its own argument slot
// with `size` stored over point's low word - then r = out[0] + (Frame_Counter &
// 1), as an s16. For a = 0, 0x80, .. 0xF80 (b = (a + 0x80) & 0xFFF) a
// Gouraud triangle at the packet cursor (Gpu_SetPolyG3, Gpu_SetSemiTrans(1)):
// the centre L, the rim points L + ((cos a) * r sar 12, (sin a) * r sar 12)
// and likewise at b (fild + fadd: each an s32 plus L's float), every depth L's;
// the centre shaded `centre`, the rim `rim` (bytes); committed 0x34.
extern "C" void __cdecl EffectKind4A_DrawGlow(const long* point, unsigned size, unsigned centre, unsigned rim) {
    float l[3];
    SH_CALL(EffectGte_ProjectPoint)(point, l);
    const short slot[2] = {static_cast<short>(size & 0xFFFFu), static_cast<short>(AddressOf(point) >> 16)};
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(point, slot, out);
    const U r = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(static_cast<std::uint16_t>(out[0]) + (Frame_Counter & 1u))));
    const auto* const lb = reinterpret_cast<const unsigned char*>(l);
    U a = 0;
    U next = 0;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, lb, 4);
        next += 0x80;
        std::memcpy(prim + 0xC, lb + 4, 4);
        const U b = next & 0xFFFu;
        StoreSum(prim + 0x18, Sar(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))) * r, 12), lb);
        StoreSum(prim + 0x1C, Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))) * r, 12), lb + 4);
        StoreSum(prim + 0x28, Sar(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(b))) * r, 12), lb);
        StoreSum(prim + 0x2C, Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(b))) * r, 12), lb + 4);
        std::memcpy(prim + 0x30, lb + 8, 4);
        std::memcpy(prim + 0x20, lb + 8, 4);
        std::memcpy(prim + 0x10, lb + 8, 4);
        prim[4] = prim[5] = prim[6] = static_cast<unsigned char>(centre);
        prim[0x14] = prim[0x15] = prim[0x16] = static_cast<unsigned char>(rim);
        prim[0x24] = prim[0x25] = prim[0x26] = static_cast<unsigned char>(rim);
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        a += 0x80;
    } while ((next & 0xFFFFu) < 0x1000u);
}

// original 0x47B070 (0x10A bytes): a debris record: its point +0 / +4 / +8
// Sprite_Current's +0x34 / +0x38 / +0x3C (read for each); three angles Rand &
// 0xFFF, Rand & 0x3FF, Rand & 0xFFF; two edges (Math_Cos, Math_Sin of 0x20; of
// -0x20; z 0) at +0x10 and +0x18 turned in place by a matrix of the three
// angles (EffectGte_SetDiagonalOne, Gte_RotMatrixX, _Y by the second negated,
// _Z, then 0x5A7C70 twice - no push of the GTE matrix here: the caller pushes
// once round all 32); its scale +0x28 1 + (Rand & 0xF); +0x20, +0x22, +0x24,
// +0x2A 0. EffectKind1E_DebrisInitOne (E1C) is the same with 0x10, 10 + Rand
// % 4 and its own push.
extern "C" void __cdecl EffectKind4B_DebrisInit(unsigned char* debris) {
    SetUL(debris + 0, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetUL(debris + 4, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    SetUL(debris + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height));
    const U ax = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U ay = static_cast<U>(SH_CALL(Rand)()) & 0x3FFu;
    const U az = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    SetWord(debris + 0x10, static_cast<U>(SH_CALL(Math_Cos)(0x20)));
    SetWord(debris + 0x12, static_cast<U>(SH_CALL(Math_Sin)(0x20)));
    SetWord(debris + 0x14, 0);
    SetWord(debris + 0x18, static_cast<U>(SH_CALL(Math_Cos)(-0x20)));
    SetWord(debris + 0x1A, static_cast<U>(SH_CALL(Math_Sin)(-0x20)));
    SetWord(debris + 0x1C, 0);
    Matrix m;
    SH_CALL(EffectGte_SetDiagonalOne)(m.m);
    SH_CALL(Gte_RotMatrixX)(static_cast<short>(ax), m.m);
    SH_CALL(Gte_RotMatrixY)(-static_cast<int>(static_cast<short>(ay)), m.m);
    SH_CALL(Gte_RotMatrixZ)(static_cast<short>(az), m.m);
    using Turn = void (__cdecl*)(const short*, const short*, short*);
    SH_AT(Turn, at::kMatrixVector.v)(m.m, reinterpret_cast<const short*>(debris + 0x10), reinterpret_cast<short*>(debris + 0x10));
    SH_AT(Turn, at::kMatrixVector.v)(m.m, reinterpret_cast<const short*>(debris + 0x18), reinterpret_cast<short*>(debris + 0x18));
    SetWord(debris + 0x28, (static_cast<U>(SH_CALL(Rand)()) & 0xFu) + 1u);
    SetWord(debris + 0x20, 0);
    SetWord(debris + 0x22, 0);
    SetWord(debris + 0x24, 0);
    SetWord(debris + 0x2A, 0);
}

// original 0x47B180 (0x121 bytes): two points round the record's (x and z
// each moved by ((Rand & 0xFF) - 0x80) << 8: the first's x, z, the second's x,
// z); Gte_PushMatrix, EffectGte_LoadMapCamera; a line at the packet cursor
// (Gpu_SetLineF2, Gpu_SetSemiTrans(0)) between the two projected, grey 0xC0,
// committed 0x20; Gte_PopMatrix.
extern "C" void __cdecl EffectKind4C_DrawSpark(void) {
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    long from[3] = {so->x, so->z, so->height};
    long to[3] = {from[0], from[1], from[2]};
    auto jitter = [](long v) { return static_cast<long>(static_cast<U>(v) + (((static_cast<U>(SH_CALL(Rand)()) & 0xFFu) - 0x80u) << 8)); };
    from[0] = jitter(from[0]);
    from[1] = jitter(from[1]);
    to[0] = jitter(to[0]);
    to[1] = jitter(to[1]);
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    float out[3];
    SH_CALL(EffectGte_ProjectPoint)(from, out);
    Copy12(prim + 8, out);
    SH_CALL(EffectGte_ProjectPoint)(to, out);
    Copy12(prim + 0x14, out);
    prim[6] = prim[5] = prim[4] = 0xC0;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
    SH_CALL(Gte_PopMatrix)();
}

// original 0x47B2B0: jmp [EffectKind4D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4D_Run(void) {
    DispatchEffect2e("EffectKind4D_Run", AddressOf(EffectKind4D_States), EffectKind4D_States_count, 1);
}

// original 0x47B2D0 (state 0): the column's base +0xC / +0x10 / +0x14 = the
// record's point, its top +0x1C = the base height; its turn word +0x20 = 0, its
// fade +0x24 = 0x1000000; +9 = 0x3C, +1 up; Sound_PlayEffect(0x208, or 0x209
// when the byte 0x6761D8 is set).
extern "C" void __cdecl EffectKind4D_Start(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::uint32_t>(so->x));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::uint32_t>(so->z));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, static_cast<std::uint32_t>(so->height));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y));
    SetWord(s + 0x20, 0);
    SetUL(s + 0x24, 0x1000000);
    s[9] = 0x3C;
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(0x208 + (Effect2e_SoundSwitch != 0 ? 1 : 0)));
}

// original 0x47B350 (state 1): the top +0x1C up 0x1000000, at most the base +
// 0x8000000 (signed); the turn word down 0x200; the column drawn; +9 down, at 0
// +9 = 0x10 and +1 up.
extern "C" void __cdecl EffectKind4D_Rise(void) {
    unsigned char* const s = S();
    AddLong(s + 0x1C, 0x1000000u);
    const U most = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y) + 0x8000000u;
    if (SL(s + 0x1C) > static_cast<std::int32_t>(most)) SetUL(s + 0x1C, most);
    AddWord(s + 0x20, 0xFE00u);
    SH_CALL(EffectKind4D_DrawColumn)();
    unsigned char* t;
    if (!CountDown9(&t)) return;
    t[9] = 0x10;
    NextState();
}

// original 0x47B3B0 (state 2): +9 down; at 0 a tail jmp to Effect_Release; else
// the turn word down 0x200, the fade +0x24 down 0x100000, and a tail jmp to
// EffectKind4D_DrawColumn.
extern "C" void __cdecl EffectKind4D_Fade(void) {
    unsigned char* s;
    if (CountDown9(&s)) {
        SH_CALL(Effect_Release)();
        return;
    }
    AddWord(s + 0x20, 0xFE00u);
    AddLong(S() + 0x24, 0xFFF00000u);
    SH_CALL(EffectKind4D_DrawColumn)();
}

// original 0x47B3F0 (0x238 bytes): EffectGte_LoadMapCamera; with the turn t =
// +0x20, the base (x0, z0, y = +0x14) and the ring point (x0 + cos t << 5, z0 +
// sin t << 5): the lower point at height max(y - +0x24, +0x14) and the upper at
// min(y, +0x1C) projected (O1, O2; the record read afresh after each call);
// then while y is below +0x1C + 0x1000000 (signed): a textured quad at the
// packet cursor (Gpu_SetPolyFT4, Gpu_SetSemiTrans(0)) from O1 / O2 as they
// stand, t up 0x100 and y up 0x100000, O1 / O2 of the new ring point, its uv
// (0x7F, 0xBF), (0x7F, 0xA0), (0x60, 0xBF), (0x60, 0xA0), colour 0x80, tpage
// Gpu_GetTPage(1, 0, 0x140, 0x100), clut Gpu_GetClut(0, 0x1E7), committed
// 0x48.
extern "C" void __cdecl EffectKind4D_DrawColumn(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned char* s = S();
    U turn = Word(s + 0x20);
    const U x0 = rv::As<records::EffectWork>(s)->work_0c;
    const U z0 = rv::As<records::EffectWork>(s)->work_10;
    U y = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y);
    long ring[3];
    float lower[3];
    float upper[3];
    // the ring point at `turn` and y: the lower and upper projections
    auto project = [&](int angle) {
        ring[0] = static_cast<long>((static_cast<U>(SH_CALL(Math_Cos)(angle)) << 5) + x0);
        ring[1] = static_cast<long>((static_cast<U>(SH_CALL(Math_Sin)(angle)) << 5) + z0);
        const unsigned char* r = S();
        ring[2] = static_cast<long>(y - UL(r + 0x24));
        if (ring[2] < SL(r + 0x14)) ring[2] = SL(r + 0x14);
        SH_CALL(EffectGte_ProjectPoint)(ring, lower);
        r = S();
        ring[2] = static_cast<long>(y);
        if (static_cast<std::int32_t>(y) > SL(r + 0x1C)) ring[2] = SL(r + 0x1C);
        SH_CALL(EffectGte_ProjectPoint)(ring, upper);
    };
    project(static_cast<int>(turn & 0xFFFFu));
    s = S();
    if (!(static_cast<std::int32_t>(y) < static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1000000u))) return;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        Copy12(prim + 8, lower);
        turn += 0x100;
        Copy12(prim + 0x18, upper);
        y += 0x100000;
        project(static_cast<int>(turn & 0xFFFFu));
        Copy12(prim + 0x28, lower);
        Copy12(prim + 0x38, upper);
        prim[0x14] = 0x7F;
        prim[0x15] = 0xBF;
        prim[0x24] = 0x7F;
        prim[0x25] = 0xA0;
        prim[0x34] = 0x60;
        prim[0x35] = 0xBF;
        prim[0x44] = 0x60;
        prim[0x45] = 0xA0;
        prim[4] = prim[5] = prim[6] = 0x80;
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(1, 0, 0x140, 0x100));
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0, 0x1E7));
        SH_CALL(Gfx_CommitPrim)(1, 0x48);
        s = S();
    } while (static_cast<std::int32_t>(y) < static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1000000u));
}

void EffectKind4d_Inject() {
    BOF3_INJECT(EffectKind4B_Run);
    BOF3_INJECT(EffectKind4B_Start);
    BOF3_INJECT(EffectKind4B_Scatter);
    BOF3_INJECT(EffectKind4C_Run);
    BOF3_INJECT(EffectKind4C_Start);
    BOF3_INJECT(EffectKind4C_Crackle);
    BOF3_INJECT(EffectKind4A_TrailInit);
    BOF3_INJECT(EffectKind4A_TrailStep);
    BOF3_INJECT(EffectKind4A_TrailDraw);
    BOF3_INJECT(EffectKind4A_DrawGlow);
    BOF3_INJECT(EffectKind4B_DebrisInit);
    BOF3_INJECT(EffectKind4C_DrawSpark);
    BOF3_INJECT(EffectKind4D_Run);
    BOF3_INJECT(EffectKind4D_Start);
    BOF3_INJECT(EffectKind4D_Rise);
    BOF3_INJECT(EffectKind4D_Fade);
    BOF3_INJECT(EffectKind4D_DrawColumn);
}

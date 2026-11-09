// Moved here by tools/move_funcs.py from effect_2b.cpp, effect_2c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_3d.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_3d_callees.h"
#include "game/effect_2b.h"
#include "game/effect_2b_callees.h"
#include "game/effect_2c.h"
#include "game/effect_2c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_3d::at;

}  // namespace

namespace {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }

U Key(const void* p) { return Low32(p); }

U AddressOf(const void* p) { return Key(p); }

void StoreFloat(unsigned char* at, double v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// imul of two dwords (the product's low 32 bits), then sar: how every point
// here is placed round its centre (>> 12 for the fixed-point trig, >> 4 for
// kind 0x3B's spirals).
std::int32_t MulSar(int t, U v, unsigned shift) { return static_cast<std::int32_t>(static_cast<U>(t) * v) >> shift; }

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

// Gpu_GetTPage(0, abr, x, y) and a draw mode of it at Gfx_PacketNext (dfe 0,
// dtd `dtd`); the fifth zero the original pushed for Gpu_GetTPage and left on
// the stack is the mode's texture window.
void Mode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// Kinds 0x3B's and 0x3D's glow: the map camera loaded, `point` projected,
// `size` scaled at its depth (EffectGte_ProjectSize: in = v, out = v + 2 - the
// original's second in word is its stack's leftover, ours 0; out's second word
// is not read); the radius that plus (Frame_Counter & 1) << `parity`, 16 bits;
// 32 semi-transparent G3 triangles fanned round the screen point at steps of
// 0x80 (the first angle whole, the second & 0xFFF), the centre `centre`'s
// shade and the two rim points `rim`'s, the depth at every vertex; each
// committed at slot 1 (0x34 bytes).
void GlowFan(const long* point, U size, U centre, U rim, unsigned parity) {
    SH_CALL(EffectGte_LoadMapCamera)();
    float p[3];
    SH_CALL(EffectGte_ProjectPoint)(point, p);
    short v[4] = {static_cast<short>(size), 0, 0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, v, v + 2);
    const auto radius = static_cast<int>(static_cast<short>((Frame_Counter & 1u) << parity) + v[2]);
    const U r = static_cast<U>(static_cast<int>(static_cast<short>(radius)));
    const auto rim_shade = static_cast<unsigned char>(rim);
    U a1 = 0, next = 0;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, &p[0], 4);
        next += 0x80;
        const U a2 = next & 0xFFFu;
        std::memcpy(prim + 0xC, &p[1], 4);
        StoreFloat(prim + 0x18, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a1)), r, 12)) + static_cast<double>(p[0]));
        StoreFloat(prim + 0x1C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a1)), r, 12)) + static_cast<double>(p[1]));
        StoreFloat(prim + 0x28, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a2)), r, 12)) + static_cast<double>(p[0]));
        StoreFloat(prim + 0x2C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a2)), r, 12)) + static_cast<double>(p[1]));
        std::memcpy(prim + 0x30, &p[2], 4);
        std::memcpy(prim + 0x20, &p[2], 4);
        std::memcpy(prim + 0x10, &p[2], 4);
        prim[4] = prim[5] = prim[6] = static_cast<unsigned char>(centre);
        prim[0x14] = prim[0x15] = prim[0x16] = rim_shade;
        prim[0x24] = prim[0x25] = prim[0x26] = rim_shade;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        a1 += 0x80;
    } while ((next & 0xFFFFu) < 0x1000u);
}

using LD = long double;

unsigned char* S() { return Sprite_Current; }

U ULEffect2c(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect2c(U a) { return ULEffect2c(At(a)); }

void SetULEffect2c(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetULEffect2c(U a, U v) { SetULEffect2c(At(a), v); }

std::int32_t SWEffect2c(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t SWEffect2c(U a) { return SWEffect2c(At(a)); }

unsigned char& B(U a) { return At(a)[0]; }

U AddressOfEffect2c(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSarEffect2c(std::int32_t a, std::int32_t b, unsigned n) { return Sar(static_cast<U>(a) * static_cast<U>(b), n); }

// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide toward zero.
std::int32_t DivPow2(U v, unsigned n) { return static_cast<std::int32_t>(v) / (1 << n); }

// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetULEffect2c(to + i, ULEffect2c(from + i));
}

// --- x87 as the original has it: `fld dword` through inline assembly, so the
// optimizer cannot narrow an operation of two floats to SSE (whose NaN rule
// differs); every operation in long double on the x87 at the game's control
// word, `fstp dword` on the way out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}

LD FAt(U a) { return F(At(a)); }

LD I(std::int32_t v) { return static_cast<LD>(v); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// The CRT's _ftol 0x5B9550: truncation to 64 bits, the low dword; NaN and
// values out of range give the integer indefinite 0x8000000000000000, whose
// low dword is 0.
U Ftol(LD v) {
    if (!(v > -9223372036854775808.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + k]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, const void* table, unsigned entries, unsigned state) {
    if (state >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2c.md section 6)",
                    who, state, entries, (unsigned)AddressOfEffect2c(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(ULEffect2c(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet every draw here opens with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }

const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

}  // namespace

// original 0x474940 (Effect_KindHandlers[0x3D], hidden in 0x4747D0's recorded
// extent): a tail jump through EffectKind3D_States 0x654458 by +1.
extern "C" void __cdecl EffectKind3D_Run(void) {
    Entry(AddressOf(EffectKind3D_States), Cur()[1], 4, "EffectKind3D_Run")();
}

// original 0x474960 (EffectKind3D_States[0]): the timer +0xC (a dword) 0x3C,
// +1 up.
extern "C" void __cdecl EffectKind3D_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(Cur())->work_0c, 0x3C);
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x474980 (EffectKind3D_States[1]): the timer down 1 - at 0, the
// timer 0x230, the rings cleared (EffectKind3D_RingsClear), +1 up.
extern "C" void __cdecl EffectKind3D_Wait(void) {
    unsigned char* s = Cur();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1);
    s = Cur();
    if (rv::As<records::EffectWork>(s)->work_0c != 0) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, 0x230);
    SH_CALL(EffectKind3D_RingsClear)();
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4749B0 (EffectKind3D_States[2]): the glow at the leader
// (EffectKind3D_DrawGlow(0x80)), the rings stepped, a ring started every
// eighth frame (Frame_Counter & 7 == 0); the timer down 1 - at 0, the timer
// 0x10 and +1 up.
extern "C" void __cdecl EffectKind3D_Glow(void) {
    SH_CALL(EffectKind3D_DrawGlow)(0x80);
    SH_CALL(EffectKind3D_RingsStep)();
    if ((Frame_Counter & 7u) == 0) SH_CALL(EffectKind3D_RingSpawn)();
    unsigned char* s = Cur();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1);
    s = Cur();
    if (rv::As<records::EffectWork>(s)->work_0c != 0) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, 0x10);
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x474A00 (EffectKind3D_States[3]): the timer down 1 while above 0
// (signed); the glow at the timer's low word << 3; the rings stepped - none
// in use, Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind3D_Fade(void) {
    unsigned char* s = Cur();
    const auto timer = static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c);
    if (timer > 0) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<U>(timer - 1));
        s = Cur();
    }
    SH_CALL(EffectKind3D_DrawGlow)((Word(s + 0xC) << 3) & 0xFFFFu);
    if (!SH_CALL(EffectKind3D_RingsStep)()) SH_CALL(Effect_Release)();
}

// original 0x474A40: the glow's draw mode (the page at (0x380, 0x100), abr 1,
// dtd 1) committed at slot 1; the glow (EffectKind3D_DrawFan) at a copy of
// the leader's point (ObjTrio +0x34, +0x38, +0x3C + 0x80 << 16), `size` (its
// low word; the whole dword passed on), grey 0x60 at the centre, black at the
// rim.
extern "C" void __cdecl EffectKind3D_DrawGlow(unsigned size) {
    Mode(1, 0x380, 0x100, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char* const leader = At(::bof3::Img(ObjTrio_addr) + 0x34);
    const long point[3] = {Long(leader), Long(leader + 4), static_cast<long>(UL(leader + 8) + 0x800000u)};
    SH_CALL(EffectKind3D_DrawFan)(point, size, 0x60, 0);
}

// original 0x474AC0: EffectKind3B_DrawGlow with the frame's parity doubled in
// the radius (GlowFan).
extern "C" void __cdecl EffectKind3D_DrawFan(const long* point, unsigned size, unsigned centre, unsigned rim) {
    GlowFan(point, size, centre, rim, 1);
}

// original 0x474C30: the sixteen rings' in-use bytes (+0x14) cleared.
extern "C" void __cdecl EffectKind3D_RingsClear(void) {
    for (U i = 0; i < at::kRingCount; ++i) At(::bof3::Img(EffectKind30_Shards_addr) + at::kRingStride * i + 0x14)[0] = 0;
}

// original 0x474C50: the first free ring of the sixteen (+0x14 0) in use, its
// state +0x15 0; none free, nothing.
extern "C" void __cdecl EffectKind3D_RingSpawn(void) {
    for (U i = 0; i < at::kRingCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kRingStride * i);
        if (r[0x14]) continue;
        r[0x14] = 1;
        r[0x15] = 0;
        return;
    }
}

// original 0x474C80: each ring in use by its state +0x15 - 0: width +0x10 0,
// half-thickness +0x12 0x10, shade +0x17 0x20, count +0x16 0x20, state up; 1:
// the point the leader's (ObjTrio +0x34, +0x38, +0x3C + 0x80 << 16), the width
// up 8, the shade and count down 1 - the count at 0, the ring freed - and
// drawn (EffectKind3D_DrawRing); any other, nothing. al 1 when any was in use.
extern "C" unsigned char __cdecl EffectKind3D_RingsStep(void) {
    unsigned char any = 0;
    for (U i = 0; i < at::kRingCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kRingStride * i);
        if (!r[0x14]) continue;
        any = 1;
        if (r[0x15] == 0) {
            const auto state = static_cast<unsigned char>(r[0x15] + 1);
            SetWord(r + 0x10, 0);
            SetWord(r + 0x12, 0x10);
            r[0x17] = 0x20;
            r[0x15] = state;
            r[0x16] = 0x20;
        } else if (r[0x15] == 1) {
            const unsigned char* const leader = At(::bof3::Img(ObjTrio_addr) + 0x34);
            const U x = UL(leader), z = UL(leader + 4);
            SetWord(r + 0x10, Word(r + 0x10) + 8u);
            SetUL(r + 4, z);
            const auto shade = static_cast<unsigned char>(r[0x17] - 1);
            SetUL(r + 0, x);
            SetUL(r + 8, UL(leader + 8) + 0x800000u);
            const auto count = static_cast<unsigned char>(r[0x16] - 1);
            r[0x17] = shade;
            r[0x16] = count;
            if (count == 0) r[0x14] = 0;
            SH_CALL(EffectKind3D_DrawRing)(r);
        }
    }
    return any;
}

// original 0x474D20 (PSX twin 0x801F2F60 by callers): a ring (its point +0..+8)
// projected; twice - the half-thickness +0x12 added, then taken off -: the
// width +0x10 and the width plus or minus the half-thickness (16 bits) scaled
// at the point's depth (EffectGte_ProjectSize: in {w +- h, w}, out the
// original's argument slot); 32 semi-transparent G4 quads round the screen
// point at steps of 0x80 (the first angle whole, the second & 0xFFF), the
// outer two corners at w +- h black, the inner two at w the shade +0x17, the
// depth at every corner; each committed at slot 1 (0x44 bytes).
extern "C" void __cdecl EffectKind3D_DrawRing(unsigned char* ring) {
    const long* const point = reinterpret_cast<const long*>(ring);
    float p[3];
    SH_CALL(EffectGte_ProjectPoint)(point, p);
    for (unsigned pass = 0; pass < 2; ++pass) {
        int h = SW(ring + 0x12);
        if (pass != 0) h = -h;
        const short w = SW(ring + 0x10);
        short buf[6] = {static_cast<short>(h + w), w, 0, 0, 0, 0};   // in = buf, out = buf + 4
        SH_CALL(EffectGte_ProjectSize)(point, buf, buf + 4);
        const U outer = static_cast<U>(static_cast<int>(buf[4]));
        const U inner = static_cast<U>(static_cast<int>(buf[5]));
        U a1 = 0, next = 0;
        do {
            unsigned char* const prim = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(prim);
            SH_CALL(Gpu_SetSemiTrans)(prim, 1);
            next += 0x80;
            const U a2 = next & 0xFFFu;
            StoreFloat(prim + 0x08, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a1)), outer, 12)) + static_cast<double>(p[0]));
            StoreFloat(prim + 0x0C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a1)), outer, 12)) + static_cast<double>(p[1]));
            StoreFloat(prim + 0x18, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a2)), outer, 12)) + static_cast<double>(p[0]));
            StoreFloat(prim + 0x1C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a2)), outer, 12)) + static_cast<double>(p[1]));
            StoreFloat(prim + 0x28, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a1)), inner, 12)) + static_cast<double>(p[0]));
            StoreFloat(prim + 0x2C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a1)), inner, 12)) + static_cast<double>(p[1]));
            StoreFloat(prim + 0x38, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a2)), inner, 12)) + static_cast<double>(p[0]));
            StoreFloat(prim + 0x3C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a2)), inner, 12)) + static_cast<double>(p[1]));
            std::memcpy(prim + 0x40, &p[2], 4);
            std::memcpy(prim + 0x30, &p[2], 4);
            std::memcpy(prim + 0x20, &p[2], 4);
            std::memcpy(prim + 0x10, &p[2], 4);
            prim[4] = prim[5] = prim[6] = 0;
            prim[0x14] = prim[0x15] = prim[0x16] = 0;
            prim[0x24] = prim[0x25] = prim[0x26] = ring[0x17];
            prim[0x34] = prim[0x35] = prim[0x36] = ring[0x17];
            SH_CALL(Gfx_CommitPrim)(1, 0x44);
            a1 += 0x80;
        } while ((next & 0xFFFFu) < 0x1000u);
    }
}

// original 0x474F40: jmp [EffectKind3E_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind3E_Run(void) {
    Dispatch("EffectKind3E_Run", EffectKind3E_States, EffectKind3E_States_count, S()[1]);
}

// original 0x474F60 (state 0): the point (+0x34, +0x38) = (0x318000, 0x638000),
// the heights +0x20 and +0x14 = 0x500000; Sound_PlayById(0x208); +1 up, +2 = 0.
extern "C" void __cdecl EffectKind3E_Start(void) {
    rv::Current<records::EffectObject>()->x = 0x318000;
    rv::Current<records::EffectObject>()->z = 0x638000;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, 0x500000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0x500000);
    SH_CALL(Sound_PlayById)(0x208);
    NextState();
    S()[2] = 0;
}

// original 0x474FC0 (state 1): four bolts, each from the cell centre (c0, c1)
// to (c2, c3) of EffectKind3E_BoltCells' next four bytes (x = c << 16 | 0x8000
// into +0xC, +0x10, +0x18, +0x1C; the heights are +0x14 and +0x20), drawn by
// EffectKind3E_DrawBolt(record + 0xC, record + 0x18); then EffectKind3E_SubStates
// by +2, a call, unbounded.
extern "C" void __cdecl EffectKind3E_Bolts(void) {
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char* const c = At(::bof3::Img(EffectKind3E_BoltCells_addr) + 4 * i);
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, static_cast<U>(c[0]) << 16 | 0x8000u);
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, static_cast<U>(c[1]) << 16 | 0x8000u);
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18, static_cast<U>(c[2]) << 16 | 0x8000u);
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, static_cast<U>(c[3]) << 16 | 0x8000u);
        unsigned char* const s = S();
        SH_CALL(EffectKind3E_DrawBolt)(Point(s + 0xC), Point(s + 0x18));
    }
    Dispatch("EffectKind3E_Bolts", EffectKind3E_SubStates, EffectKind3E_SubStates_count, S()[2]);
}

// original 0x475050 (sub-state 0): Field_Request not 5 - +2 = 1.
extern "C" void __cdecl EffectKind3E_SubWait(void) {
    if (Field_Request == 5) return;
    S()[2] = 1;
}

// original 0x475070 (sub-state 1): Field_Request 5 - Sound_PlayEffect(0x209).
extern "C" void __cdecl EffectKind3E_SubSound(void) {
    if (Field_Request != 5) return;
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x475090: jmp [EffectKind3F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind3F_Run(void) {
    Dispatch("EffectKind3F_Run", EffectKind3F_States, EffectKind3F_States_count, S()[1]);
}

// original 0x4750B0 (state 0): EffectKind3F_DrawBeam(record + 0xC, record +
// 0x18); +1 up.
extern "C" void __cdecl EffectKind3F_Draw(void) {
    unsigned char* const s = S();
    SH_CALL(EffectKind3F_DrawBeam)(Point(s + 0xC), Point(s + 0x18));
    NextState();
}

// original 0x4750D0 (0x165 bytes): EffectGte_LoadMapCamera; a draw mode (abr 1,
// committed to slot 1); the world point L = from's x, z and height projected;
// then 16 times: a grey semi-transparent LINE_F2 from the last projection to
// the next (L's x and z stepped by (to - from) / 16, its height from's plus
// ((Rand & 0xFFF) - 0x800) << 12 on all but the last), committed 0x20, and
// EffectKind3E_BoltGlow of its two ends.
extern "C" void __cdecl EffectKind3E_DrawBolt(const long* from, const long* to) {
    SH_CALL(EffectGte_LoadMapCamera)();
    DrawMode(1, 1, 1);
    const auto* const a = reinterpret_cast<const unsigned char*>(from);
    const auto* const b = reinterpret_cast<const unsigned char*>(to);
    alignas(4) unsigned char l[12];
    SetULEffect2c(l, ULEffect2c(a));
    SetULEffect2c(l + 4, ULEffect2c(a + 4));
    const U height = ULEffect2c(a + 8);
    SetULEffect2c(l + 8, height);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(l), Out(o));
    for (unsigned step = 1; step <= 0x10; ++step) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineF2)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Copy12(prim + 8, o);
        SetULEffect2c(l, ULEffect2c(l) + static_cast<U>(DivPow2(ULEffect2c(b) - ULEffect2c(a), 4)));
        SetULEffect2c(l + 4, ULEffect2c(l + 4) + static_cast<U>(DivPow2(ULEffect2c(b + 4) - ULEffect2c(a + 4), 4)));
        SetULEffect2c(l + 8, height);
        if (step < 0x10) SetULEffect2c(l + 8, height + (((static_cast<U>(SH_CALL(Rand)()) & 0xFFFu) - 0x800u) << 12));
        SH_CALL(EffectGte_ProjectPoint)(Point(l), Out(o));
        Copy12(prim + 0x14, o);
        prim[4] = prim[5] = prim[6] = 0x80;
        SH_CALL(Gfx_CommitPrim)(1, 0x20);
        SH_CALL(EffectKind3E_BoltGlow)(prim + 8, prim + 0x14);
    }
}

// original 0x475240 (0x146 bytes): the segment's normal - Gte_VectorNormal of
// (_ftol(-(p1.y - p0.y)), _ftol(p1.x - p0.x), 0), in place - times 5 >> 12 is
// (nx, ny); a semi-transparent POLY_G4 of p0, p1 (shade 0x40) and p0 + n, p1 +
// n (black), the depths p0's and p1's, committed 0x44; the same quad copied to
// the next packet with p0 - n, p1 - n, committed 0x44.
extern "C" void __cdecl EffectKind3E_BoltGlow(const unsigned char* p0, const unsigned char* p1) {
    alignas(4) long v[3];
    v[0] = static_cast<long>(Ftol(-(F(p1 + 4) - F(p0 + 4))));
    v[1] = static_cast<long>(Ftol(F(p1) - F(p0)));
    v[2] = 0;
    SH_CALL(Gte_VectorNormal)(v, v);
    unsigned char* const prim = Gfx_PacketNext;
    const auto nx = static_cast<std::int32_t>(MulSarEffect2c(static_cast<std::int32_t>(v[0]), 5, 12));
    const auto ny = static_cast<std::int32_t>(MulSarEffect2c(static_cast<std::int32_t>(v[1]), 5, 12));
    SH_CALL(Gpu_SetPolyG4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SetULEffect2c(prim + 8, ULEffect2c(p0));
    SetULEffect2c(prim + 0xC, ULEffect2c(p0 + 4));
    StF(prim + 0x18, I(nx) + F(p0));
    StF(prim + 0x1C, I(ny) + F(p0 + 4));
    const LD z0 = F(p0 + 8);
    StF(prim + 0x20, z0);
    StF(prim + 0x10, z0);
    SetULEffect2c(prim + 0x28, ULEffect2c(p1));
    SetULEffect2c(prim + 0x2C, ULEffect2c(p1 + 4));
    StF(prim + 0x38, I(nx) + F(p1));
    StF(prim + 0x3C, I(ny) + F(p1 + 4));
    const LD z1 = F(p1 + 8);
    StF(prim + 0x40, z1);
    StF(prim + 0x30, z1);
    prim[4] = prim[5] = prim[6] = 0x40;
    prim[0x14] = prim[0x15] = prim[0x16] = 0;
    prim[0x24] = prim[0x25] = prim[0x26] = 0x40;
    prim[0x34] = prim[0x35] = prim[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
    unsigned char* const next = Gfx_PacketNext;
    for (unsigned i = 0; i < 0x44; i += 4) SetULEffect2c(next + i, ULEffect2c(next - 0x44 + i));   // rep movsd, forward
    StF(next + 0x18, F(p0) - I(nx));
    StF(next + 0x1C, F(p0 + 4) - I(ny));
    StF(next + 0x38, F(p1) - I(nx));
    StF(next + 0x3C, F(p1 + 4) - I(ny));
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// original 0x475390 (0x10D bytes): a draw mode (abr 1, slot 1);
// EffectGte_LoadMapCamera; both ends projected; the angle Math_Ratan2(dy, dx)
// of the screen segment; each end's radius the size (0x10, 0) projected at it
// (EffectGte_ProjectSize) plus Frame_Counter & 1; a cap at `from` at angle +
// 0x400 and one at `to` at angle + 0xC00 (EffectKind3F_DrawCap), and the band
// between (EffectKind3F_DrawBand).
extern "C" void __cdecl EffectKind3F_DrawBeam(const long* from, const long* to) {
    DrawMode(1, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char p0[12];
    alignas(4) unsigned char p1[12];
    SH_CALL(EffectGte_ProjectPoint)(from, Out(p0));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(p1));
    const auto dx = static_cast<float>(F(p1) - F(p0));
    const auto dy = static_cast<float>(F(p1 + 4) - F(p0 + 4));
    const U angle = static_cast<U>(SH_CALL(Math_Ratan2)(dy, dx));
    alignas(4) short size[2] = {0x10, 0};
    alignas(4) short out[2];
    SH_CALL(EffectGte_ProjectSize)(from, size, out);
    unsigned char out_bytes[4];
    std::memcpy(out_bytes, out, 4);
    const U r0 = (Frame_Counter & 1u) + ULEffect2c(out_bytes);
    const U a0 = angle + 0x400;
    SH_CALL(EffectKind3F_DrawCap)(p0, r0, a0);
    SH_CALL(EffectGte_ProjectSize)(to, size, out);
    std::memcpy(out_bytes, out, 4);
    const U r1 = ULEffect2c(out_bytes) + (Frame_Counter & 1u);
    const U a1 = angle + 0xC00;
    SH_CALL(EffectKind3F_DrawCap)(p1, r1, a1);
    SH_CALL(EffectKind3F_DrawBand)(p0, r0, a0, p1, r1, a1);
}

// original 0x4754A0 (0x108 bytes): eight semi-transparent POLY_G3 of a fan
// round `centre` (its three floats; its depth at all three vertices), the rim
// at (cos, sin)(t) * radius >> 12 for t = angle, angle + 0x100, .. (each masked
// to 16 bits), each triangle from t to t + 0x100; the centre yellow (0x80,
// 0x80, 0), the rim black; committed 0x34 each. The radius is read as an s16.
extern "C" void __cdecl EffectKind3F_DrawCap(const unsigned char* centre, unsigned radius, unsigned angle) {
    const std::int32_t r = static_cast<std::int16_t>(radius);
    U slot = angle;
    U t = angle & 0xFFFFu;
    for (unsigned n = 8; n != 0; --n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SetULEffect2c(prim + 8, ULEffect2c(centre));
        SetULEffect2c(prim + 0xC, ULEffect2c(centre + 4));
        const LD z = F(centre + 8);
        StF(prim + 0x30, z);
        StF(prim + 0x20, z);
        StF(prim + 0x10, z);
        std::int32_t v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 12));
        StF(prim + 0x18, I(v) + F(centre));
        v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 12));
        slot += 0x100;
        t = slot & 0xFFFFu;
        StF(prim + 0x1C, I(v) + F(centre + 4));
        v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 12));
        StF(prim + 0x28, I(v) + F(centre));
        v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 12));
        prim[4] = 0x80;
        prim[5] = 0x80;
        prim[6] = 0;
        prim[0x14] = prim[0x15] = prim[0x16] = 0;
        StF(prim + 0x2C, I(v) + F(centre + 4));
        prim[0x24] = prim[0x25] = prim[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x4755B0 (0x1A4 bytes): at the packet cursor, a semi-transparent
// POLY_G4 of p0, p1 (yellow) and p0 + (cos, sin)(a0) * r0 >> 12, p1 + (cos,
// sin)(a1 + 0x800) * r1 >> 12 (black), the depths p0's and p1's, committed
// 0x44; the same quad copied 0x44 bytes on (the cursor not read again) with
// p0's rim at a0 + 0x800 and p1's at a1, committed 0x44. The radii are read as
// s16, the angles' low words (a0's + 0x800 passed whole).
extern "C" void __cdecl EffectKind3F_DrawBand(const unsigned char* p0, unsigned r0, unsigned a0, const unsigned char* p1,
                                              unsigned r1, unsigned a1) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const U t0 = a0 & 0xFFFFu;
    std::int32_t r = static_cast<std::int16_t>(r0);
    SetULEffect2c(prim + 8, ULEffect2c(p0));
    SetULEffect2c(prim + 0xC, ULEffect2c(p0 + 4));
    LD z = F(p0 + 8);
    StF(prim + 0x30, z);
    StF(prim + 0x10, z);
    SetULEffect2c(prim + 0x18, ULEffect2c(p1));
    SetULEffect2c(prim + 0x1C, ULEffect2c(p1 + 4));
    z = F(p1 + 8);
    StF(prim + 0x40, z);
    StF(prim + 0x20, z);
    std::int32_t v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(t0)), r, 12));
    StF(prim + 0x28, I(v) + F(prim + 8));
    v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(t0)), r, 12));
    const U t1 = a1 & 0xFFFFu;
    r = static_cast<std::int16_t>(r1);
    StF(prim + 0x2C, I(v) + F(prim + 0xC));
    U t = t1 + 0x800;
    v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 12));
    StF(prim + 0x38, I(v) + F(prim + 0x18));
    v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 12));
    prim[4] = 0x80;
    prim[5] = 0x80;
    prim[6] = 0;
    prim[0x14] = 0x80;
    prim[0x15] = 0x80;
    prim[0x16] = 0;
    prim[0x24] = prim[0x25] = prim[0x26] = 0;
    StF(prim + 0x3C, I(v) + F(prim + 0x1C));
    prim[0x34] = prim[0x35] = prim[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
    unsigned char* const q = prim + 0x44;
    t = t0 + 0x800;
    for (unsigned i = 0; i < 0x44; i += 4) SetULEffect2c(q + i, ULEffect2c(prim + i));   // rep movsd, forward
    r = static_cast<std::int16_t>(r0);
    v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 12));
    StF(q + 0x28, I(v) + F(q + 8));
    v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 12));
    StF(q + 0x2C, I(v) + F(q + 0xC));
    r = static_cast<std::int16_t>(r1);
    v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(t1)), r, 12));
    StF(q + 0x38, I(v) + F(q + 0x18));
    v = static_cast<std::int32_t>(MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(t1)), r, 12));
    StF(q + 0x3C, I(v) + F(q + 0x1C));
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// original 0x475760: jmp [EffectKind40_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind40_Run(void) {
    Dispatch("EffectKind40_Run", EffectKind40_States, EffectKind40_States_count, S()[1]);
}

// original 0x475780 (state 0): EffectKind40_ShardsClear; the radius word +0x2E
// = 0; +1 up.
extern "C" void __cdecl EffectKind40_Start(void) {
    SH_CALL(EffectKind40_ShardsClear)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
    NextState();
}

// original 0x4757A0 (state 1): the counter byte 0x903848 at 0x19 - +1 up.
// Else the disc of radius +0x2E (EffectKind40_DrawDisc; the original pushes
// the register whose low word it loaded, its high half the dispatcher's
// Sprite_Current) and EffectKind40_ShardsStep; +0x2E up 0x40, at most 0x400
// (signed).
extern "C" void __cdecl EffectKind40_Grow(void) {
    const unsigned char counter = B(::bof3::Img(at::kCounter.v));
    unsigned char* s = S();
    if (counter == 0x19) {
        s[1] = static_cast<unsigned char>(s[1] + 1);
        return;
    }
    SH_CALL(EffectKind40_DrawDisc)(rv::As<records::EffectWork>(s)->work_2e);
    SH_CALL(EffectKind40_ShardsStep)();
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e + 0x40u);
    s = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e)) >= 0x400) rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, 0x400);
}

// original 0x4757F0 (0x2B bytes): the 32 shard records' in-use bytes cleared
// through the cursor 0x67610C (left past the last).
extern "C" void __cdecl EffectKind40_ShardsClear(void) {
    SetULEffect2c(::bof3::Img(at::kShardCursor.v), ::bof3::Img(EffectKind30_Shards_addr));
    for (unsigned n = at::kShardCount; n != 0; --n) {
        EffectKind40_ShardCursor[0] = 0;
        SetULEffect2c(::bof3::Img(at::kShardCursor.v), ULEffect2c(::bof3::Img(at::kShardCursor.v)) + at::kShardStride);
    }
}

// original 0x475820 (0xA8 bytes): EffectGte_LoadMapCamera; a draw mode (abr
// 1, slot 3); each live shard of the 32 (the cursor 0x67610C on it): its
// height word +8 down 0x20, below 0x80 (signed) no longer in use, drawn
// (EffectKind40_ShardDraw) either way; then twice a free record
// (EffectKind40_ShardFind, the cursor its answer) started
// (EffectKind40_ShardSpawn).
extern "C" void __cdecl EffectKind40_ShardsStep(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    DrawMode(1, 1, 3);
    U p = ::bof3::Img(EffectKind30_Shards_addr);
    SetULEffect2c(::bof3::Img(at::kShardCursor.v), p);
    for (unsigned n = at::kShardCount; n != 0; --n) {
        if (At(p)[0] != 0) {
            SetWord(At(p) + 8, Word(At(p) + 8) - 0x20u);
            p = ULEffect2c(::bof3::Img(at::kShardCursor.v));
            if (SWEffect2c(At(p) + 8) < 0x80) At(p)[0] = 0;
            SH_CALL(EffectKind40_ShardDraw)();
            p = ULEffect2c(::bof3::Img(at::kShardCursor.v));
        }
        p += at::kShardStride;
        SetULEffect2c(::bof3::Img(at::kShardCursor.v), p);
    }
    for (unsigned k = 0; k < 2; ++k) {
        unsigned char* const found = SH_CALL(EffectKind40_ShardFind)();
        SetULEffect2c(::bof3::Img(at::kShardCursor.v), AddressOfEffect2c(found));
        if (found != nullptr) SH_CALL(EffectKind40_ShardSpawn)(found);
    }
}

// original 0x4758D0 (0x14B bytes), on the cursor's shard: its screen point
// (+0xC, +0x10) the disc's centre 0x92C280 plus (s16 +4 << 7) / s16 +8 and
// (s16 +6 << 7) / s16 +8, its depth +0x14 the centre's; drawn only inside the
// disc's rim (the 32 points 0x92C28C: every edge's cross product with the
// point 0.0 or more) - a TILE_1 (not semi-transparent) at it, shaded (0x80 -
// s16 +8) * 192 / 512 - 0x40, committed to slot 3 (0x14). A height of 0 is the
// original's divide fault: ours aborts.
extern "C" void __cdecl EffectKind40_ShardDraw(void) {
    unsigned char* p = EffectKind40_ShardCursor;
    std::int32_t d = SWEffect2c(p + 8);
    if (d == 0)
        bof3::Fatal("EffectKind40_ShardDraw: the shard's height +8 is 0 - the original faults on the divide "
                    "(docs/effect_2c.md section 6)");
    std::int32_t q = static_cast<std::int32_t>(static_cast<U>(SWEffect2c(p + 4)) << 7) / d;
    StF(p + 0xC, I(q) + FAt(::bof3::Img(EffectKind30_Shards_addr) + 0x300));
    p = EffectKind40_ShardCursor;
    d = SWEffect2c(p + 8);
    if (d == 0)
        bof3::Fatal("EffectKind40_ShardDraw: the shard's height +8 is 0 - the original faults on the divide "
                    "(docs/effect_2c.md section 6)");
    q = static_cast<std::int32_t>(static_cast<U>(SWEffect2c(p + 6)) << 7) / d;
    StF(p + 0x10, I(q) + FAt(::bof3::Img(EffectKind30_Shards_addr) + 0x300 + 4));
    const LD depth = FAt(::bof3::Img(EffectKind30_Shards_addr) + 0x300 + 8);
    StF(EffectKind40_ShardCursor + 0x14, depth);
    const unsigned char* const s = EffectKind40_ShardCursor;
    unsigned prev = 0;
    unsigned char step = 0;
    do {
        ++step;
        const unsigned next = step & 0x1Fu;
        const U vn = ::bof3::Img(EffectKind30_Shards_addr) + 0x30c + 12u * next;
        const U vp = ::bof3::Img(EffectKind30_Shards_addr) + 0x30c + 12u * prev;
        const LD dx = FAt(vn) - FAt(vp);
        const LD dy = FAt(vn + 4) - FAt(vp + 4);
        const LD px = F(s + 0xC) - FAt(vp);
        const LD py = F(s + 0x10) - FAt(vp + 4);
        const LD cross = py * dx - px * dy;
        if (!(cross >= FAt(at::kZero))) return;
        prev = step;
    } while (step < 0x20);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    Copy12(prim + 8, EffectKind40_ShardCursor + 0xC);
    const U shade = (0x80u - static_cast<U>(SWEffect2c(EffectKind40_ShardCursor + 8))) * 3u << 6;
    const auto c = static_cast<unsigned char>(DivPow2(shade, 9) - 0x40);
    prim[4] = prim[5] = prim[6] = c;
    SH_CALL(Gfx_CommitPrim)(3, 0x14);
}

// original 0x475A20 (0x36 bytes): the shard in use; its offset words +4, +6
// (Rand & 0xFF) - 0x80 each (+4 first); its height +8 = 0x280.
extern "C" void __cdecl EffectKind40_ShardSpawn(unsigned char* shard) {
    shard[0] = 1;
    U r = static_cast<U>(SH_CALL(Rand)());
    SetWord(shard + 4, (r & 0xFFu) - 0x80u);
    r = static_cast<U>(SH_CALL(Rand)());
    SetWord(shard + 8, 0x280);
    SetWord(shard + 6, (r & 0xFFu) - 0x80u);
}

// original 0x475A60 (0x23 bytes): the first shard record of the 32 not in use,
// the cursor 0x67610C left on it (past the last, and 0 answered, when none).
extern "C" unsigned char* __cdecl EffectKind40_ShardFind(void) {
    U p = ::bof3::Img(EffectKind30_Shards_addr);
    SetULEffect2c(::bof3::Img(at::kShardCursor.v), p);
    for (unsigned char n = 0;;) {
        if (At(p)[0] == 0) return At(p);
        p += at::kShardStride;
        ++n;
        SetULEffect2c(::bof3::Img(at::kShardCursor.v), p);
        if (n >= at::kShardCount) return nullptr;
    }
}

// original 0x475A90 (0x201 bytes): a draw mode (abr 2, slot 3);
// EffectGte_LoadMapCamera; the record's point projected - its screen point the
// disc's centre, kept at 0x92C280; the rim point at angle 0xFF80 ((cos, sin) *
// radius >> 4 from the record's point, Sprite_Current read after each call)
// projected; then 32 white-to-black POLY_G3 fans (centre white, rim black,
// semi-transparent) round it, the angle up 0x80 each, every new rim point kept
// in 0x92C28C.. as well, committed to slot 3 (0x34). The radius is read as an s16.
extern "C" void __cdecl EffectKind40_DrawDisc(unsigned radius) {
    DrawMode(2, 1, 3);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    alignas(4) unsigned char q[12];
    SetULEffect2c(q, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x));
    SetULEffect2c(q + 4, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    SetULEffect2c(q + 8, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    alignas(4) unsigned char c[12];
    alignas(4) unsigned char p[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(c));
    const std::int32_t r = static_cast<std::int16_t>(radius);
    U angle = 0xFF80;
    SetULEffect2c(::bof3::Img(at::kDiscCentre.v), ULEffect2c(c));
    SetULEffect2c(::bof3::Img(at::kDiscCentre.v) + 4, ULEffect2c(c + 4));
    SetULEffect2c(::bof3::Img(at::kDiscCentre.v) + 8, ULEffect2c(c + 8));
    U v = MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(angle)), r, 4);
    SetULEffect2c(q, v + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    v = MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(angle)), r, 4);
    s = S();
    SetULEffect2c(q + 4, v + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    SetULEffect2c(q + 8, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(p));
    U rim = ::bof3::Img(EffectKind30_Shards_addr) + 0x30c;
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Copy12(prim + 8, c);
        angle += 0x80;
        Copy12(prim + 0x18, p);
        const U t = angle & 0xFFFFu;
        v = MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 4);
        SetULEffect2c(q, v + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
        v = MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 4);
        s = S();
        SetULEffect2c(q + 4, v + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
        SetULEffect2c(q + 8, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(p));
        Copy12(prim + 0x28, p);
        Copy12(At(rim), p);
        prim[4] = prim[5] = prim[6] = 0xFF;
        prim[0x14] = prim[0x15] = prim[0x16] = 0;
        prim[0x24] = prim[0x25] = prim[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(3, 0x34);
        rim += 0xC;
    }
}

// original 0x475CA0: jmp [EffectKind42_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind42_Run(void) {
    Dispatch("EffectKind42_Run", EffectKind42_States, EffectKind42_States_count, S()[1]);
}

// original 0x475CC0 (state 1): Area146_DrawGlowCylinder at a copy of the
// record's point; +1 stays (the record runs until something releases it).
extern "C" void __cdecl EffectKind42_Glow(void) {
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    alignas(4) long point[4];
    point[0] = so->x;
    point[1] = so->z;
    point[2] = so->height;
    point[3] = 0;   // the original's fourth dword is never written or read
    SH_CALL(Area146_DrawGlowCylinder)(point);
}

void EffectKind3dFromEffect2b_Inject() {
    BOF3_INJECT(EffectKind3D_Run);
    BOF3_INJECT(EffectKind3D_Start);
    BOF3_INJECT(EffectKind3D_Wait);
    BOF3_INJECT(EffectKind3D_Glow);
    BOF3_INJECT(EffectKind3D_Fade);
    BOF3_INJECT(EffectKind3D_DrawGlow);
    BOF3_INJECT(EffectKind3D_DrawFan);
    BOF3_INJECT(EffectKind3D_RingsClear);
    BOF3_INJECT(EffectKind3D_RingSpawn);
    BOF3_INJECT(EffectKind3D_RingsStep);
    BOF3_INJECT(EffectKind3D_DrawRing);
}

void EffectKind3dFromEffect2c_Inject() {
    BOF3_INJECT(EffectKind3E_Run);
    BOF3_INJECT(EffectKind3E_Start);
    BOF3_INJECT(EffectKind3E_Bolts);
    BOF3_INJECT(EffectKind3E_SubWait);
    BOF3_INJECT(EffectKind3E_SubSound);
    BOF3_INJECT(EffectKind3F_Run);
    BOF3_INJECT(EffectKind3F_Draw);
    BOF3_INJECT(EffectKind3E_DrawBolt);
    BOF3_INJECT(EffectKind3E_BoltGlow);
    BOF3_INJECT(EffectKind3F_DrawBeam);
    BOF3_INJECT(EffectKind3F_DrawCap);
    BOF3_INJECT(EffectKind3F_DrawBand);
    BOF3_INJECT(EffectKind40_Run);
    BOF3_INJECT(EffectKind40_Start);
    BOF3_INJECT(EffectKind40_Grow);
    BOF3_INJECT(EffectKind40_ShardsClear);
    BOF3_INJECT(EffectKind40_ShardsStep);
    BOF3_INJECT(EffectKind40_ShardDraw);
    BOF3_INJECT(EffectKind40_ShardSpawn);
    BOF3_INJECT(EffectKind40_ShardFind);
    BOF3_INJECT(EffectKind40_DrawDisc);
    BOF3_INJECT(EffectKind42_Run);
    BOF3_INJECT(EffectKind42_Glow);
}

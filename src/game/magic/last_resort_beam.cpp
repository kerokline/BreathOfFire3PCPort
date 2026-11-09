// Moved here by tools/move_funcs.py from magic_s35.cpp (docs/source-layout-round.md).
#include "game/magic/last_resort_beam.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s35.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `imul` alone: the 32-bit product, wrapped.
std::int32_t Mul32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

std::int32_t Add32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

// `cdq / and edx, 2^n - 1 / add / sar n` (or `cdq / sub eax, edx / sar 1`):
// a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using Fn1 = void (__cdecl*)(int);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

void Call1(std::uint32_t address, int a) { MH_WORD(Fn1, address)(a); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The projections with the arguments the originals push: the depth and a
// flag pointer after the outputs (the flag one more than the callee takes).
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);

using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S35_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned char* sxy) {
    long p, flag;
    S35_AS(Rtp1Fn, Gte_RotTransPers)(VP(0), sxy, &p, &flag);
}

// Three points at +8, +0x18, +0x28 (a gouraud triangle's).
void Rtp3(unsigned char* prim) {
    long p, flag;
    S35_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// Four points at +8, +0x18, +0x28, +0x38 (a gouraud quad's).
void Rtp4(unsigned char* prim) {
    long p, flag;
    S35_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}

// Math_Sin / Math_Cos of `angle` times a scratch value the original reads
// after the call (the product wrapped, >> 12): the dword 0x903850, or a s16.
int SinSD0(int angle) {
    const int v = MH_CALL(Math_Sin)(angle);
    return Mul12(v, SD(0));
}

void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void Link(std::int32_t x, std::int32_t z, int dy, unsigned size) {
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), dy, size);
}

}  // namespace

#define S35_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EF620: the kind-2 task. A six-entry stack table by +1:
// LastResort_Start, MAGIC077's Revive_TintSource, BattleFx_Brighten,
// LastResort_WaitChildren, MAGIC086's Barrier_Fade, BattleFx_Finish.
S35_EXPORT void __cdecl LastResort_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::LastResort_Start,       bof3::addr::Revive_TintSource,
                                                 bof3::addr::BattleFx_Brighten,      bof3::addr::LastResort_WaitChildren,
                                                 bof3::addr::Barrier_Fade,           bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 6) PastTable("LastResort_Task", phase, 6);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4EF670: the source sprite's (0x904B4C) +8, +0x34, +0x38, +0x3C
// to the task; +0xB 0, +9 0x10, +1 on. One ring child (kind 1, 0x47: +1 0,
// +9 0) at the task's +0x34 / +0x38 / word +0x3E, then eight beam children (+1
// 1, +0xB (i & 3) << 3, +9 (i >> 2) x 0x28 + 0x11) at its +0x34..+0x3C, each
// counted in +0xB; sound 0x100.
S35_EXPORT void __cdecl LastResort_Start(void) {
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    Sc()[8] = src[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Inc(Sc()[1]);
    {
        unsigned char* const c = TaskSlot(NewTask(0x47));
        records::BattleTask* const tc = rv::As<records::BattleTask>(c);
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        tc->owner = static_cast<std::int32_t>(Key(s));
        c[1] = 0;
        c[9] = 0;
        tc->x = ts->x;
        tc->z = ts->z;
        rv::Store<unsigned>(tc->height_whole, ts->height_whole);
        Inc(s[0xB]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const c = TaskSlot(NewTask(0x47));
        records::BattleTask* const tc = rv::As<records::BattleTask>(c);
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        tc->owner = static_cast<std::int32_t>(Key(s));
        c[1] = 1;
        c[0xB] = static_cast<unsigned char>((i & 3) << 3);
        c[9] = static_cast<unsigned char>((i >> 2) * 0x28 + 0x11);
        tc->x = ts->x;
        tc->z = ts->z;
        rv::Store(tc->height, ts->height);
        Inc(s[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4EF7C0: +1 on once +0xB (the children still running) is 1 or less.
// Also entry 1 of Snap_Task (MAGIC013) and a table entry of MAGIC088's.
S35_EXPORT void __cdecl LastResort_WaitChildren(void) {
    if (Sc()[0xB] <= 1) Inc(Sc()[1]);
}

// original 0x4EF7D0: the children's kind-1 task (parameter 0x47), a jmp
// through LastResortChild_Kinds (two entries: the ring, the beam) by +1,
// unchecked.
S35_EXPORT void __cdecl LastResortChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::LastResortRing_Run, bof3::addr::LastResortBeam_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("LastResortChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4EF7F0: a call through LastResortRing_Steps (three entries:
// BarrierRing_Grow, MagicFx_WaitOwnerChildren, MagicFx_CountDownRelease) by +2;
// then while +0 is set: the actor matrix, dword 0x903854 +9 x 5, the disc and
// the band, the matrix popped.
S35_EXPORT void __cdecl LastResortRing_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::BarrierRing_Grow, bof3::addr::MagicFx_WaitOwnerChildren,
                                                bof3::addr::MagicFx_CountDownRelease};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("LastResortRing_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    SetSD(4, Sc()[9] * 5u);
    Call0(bof3::addr::LastResortRing_DrawDisc);
    Call0(bof3::addr::LastResortRing_DrawBand);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EF840: +2 on once the owner's +0xB (its children still running)
// is 1 or less. A step of thirteen overlays' tables.
S35_EXPORT void __cdecl MagicFx_WaitOwnerChildren(void) {
    if (Owner()[0xB] <= 1) Inc(Sc()[2]);
}

// original 0x4EF860: a disc of sixteen semi-transparent gouraud triangles
// round the matrix's origin, radius 0xC0: the centre shaded byte 0x903854 in
// all three channels, the rim (s, s / 2, 1) with s the dword 0x903854; each
// committed to layer 5 between two draw-mode packets.
S35_EXPORT void __cdecl LastResortRing_DrawDisc(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetVW(0x10, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(0), 0xC0)));
    SetVW(0x12, static_cast<unsigned>(Mul12(MH_CALL(Math_Cos)(0), 0xC0)));
    for (int angle = 0x100; angle < 0x1100; angle += 0x100) {
        const std::uint16_t y = VW(0x12);
        const std::uint16_t x = VW(0x10);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        SetVW(0x10, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(angle), 0xC0)));
        const int c = MH_CALL(Math_Cos)(angle);
        unsigned char* const p = Gfx_PacketNext;
        SetVW(0x12, static_cast<unsigned>(Mul12(c, 0xC0)));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(4);
        p[5] = SB(4);
        p[6] = SB(4);
        p[0x14] = SB(4);
        p[0x16] = 1;
        p[0x15] = static_cast<unsigned char>(DivPow2(SD(4), 1));
        p[0x24] = SB(4);
        p[0x25] = static_cast<unsigned char>(DivPow2(SD(4), 1));
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4EFA00: a band of sixteen semi-transparent
// gouraud quads between radius 0xC0 and 0x10 round the origin: the outer
// edge (s, s / 2, 1) with s the dword 0x903854, the inner edge (1, 1, 1).
S35_EXPORT void __cdecl LastResortRing_DrawBand(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetVW(8, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(0), 0xC0)));
    SetVW(0xA, static_cast<unsigned>(Mul12(MH_CALL(Math_Cos)(0), 0xC0)));
    SetVW(0x18, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(0), 0x100)));
    SetVW(0x1A, static_cast<unsigned>(Mul12(MH_CALL(Math_Cos)(0), 0x100)));
    for (int angle = 0x100; angle < 0x1100; angle += 0x100) {
        const std::uint16_t inner_y = VW(0x1A);
        const std::uint16_t outer_x = VW(8);
        const std::uint16_t outer_y = VW(0xA);
        SetVW(0, outer_x);
        const std::uint16_t inner_x = VW(0x18);
        SetVW(2, outer_y);
        SetVW(0x10, inner_x);
        SetVW(0x12, inner_y);
        SetVW(8, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(angle), 0xC0)));
        SetVW(0xA, static_cast<unsigned>(Mul12(MH_CALL(Math_Cos)(angle), 0xC0)));
        SetVW(0x18, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(angle), 0x100)));
        const int c = MH_CALL(Math_Cos)(angle);
        unsigned char* const p = Gfx_PacketNext;
        SetVW(0x1C, 0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(c, 0x100)));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = SB(4);
        p[6] = 1;
        p[5] = static_cast<unsigned char>(DivPow2(SD(4), 1));
        p[0x14] = SB(4);
        p[0x16] = 1;
        p[0x15] = static_cast<unsigned char>(DivPow2(SD(4), 1));
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4EFC00: a call through LastResortBeam_Steps (four entries) by +2;
// then while +0 and +2 are set: the actor matrix, the column while +2 is below
// 3, the two trails (2 and 3), the matrix popped.
S35_EXPORT void __cdecl LastResortBeam_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::LastResortBeam_Wait, bof3::addr::LastResortBeam_Rise,
                                                bof3::addr::LastResortBeam_Shrink, bof3::addr::LastResortBeam_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("LastResortBeam_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] == 0 || s[2] == 0) return;
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    if (Sc()[2] < 3) Call0(bof3::addr::LastResortBeam_DrawColumn);
    Call1(bof3::addr::LastResortBeam_DrawSparks, 2);
    Call1(bof3::addr::LastResortBeam_DrawSparks, 3);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EFC50: +9 down; at 0 +0xA 1 and +2 on.
S35_EXPORT void __cdecl LastResortBeam_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4EFC80: +0xB up by 2, +9 up, +0xA up while below 0x10; at +9
// 0x18 +2 on.
S35_EXPORT void __cdecl LastResortBeam_Rise(void) {
    AddB(Sc()[0xB], 2);
    Inc(Sc()[9]);
    if (Sc()[0xA] < 0x10) Inc(Sc()[0xA]);
    if (Sc()[9] == 0x18) Inc(Sc()[2]);
}

// original 0x4EFCC0: +0xB up by 2, +9 up, +0xA down; at +9 0x28 +2 on.
S35_EXPORT void __cdecl LastResortBeam_Shrink(void) {
    AddB(Sc()[0xB], 2);
    Inc(Sc()[9]);
    Dec(Sc()[0xA]);
    if (Sc()[9] == 0x28) Inc(Sc()[2]);
}

// original 0x4EFD00: +0xA up; at 0x18 the owner's +0xB down and a tail jmp to
// BattleTask_FreeCurrent.
S35_EXPORT void __cdecl LastResortBeam_End(void) {
    Inc(Sc()[0xA]);
    if (Sc()[0xA] != 0x18) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EFD30: the column. Rows i = 1 .. +0xA - 1, each a ring point at
// angle ((+0xB - i) & 0x1F) << 7, radius (i - +9) x 2 + 0xC0 (dword
// 0x903850), height (i - +9) x 12 (dword 0x903858), joined to the previous
// row's point by four semi-transparent gouraud quads stacked in height, each
// linked at the point (x << 9 + +0x34, y << 9 + +0x38) under a draw-mode
// packet: the shade 3 x (0x10 - i) (from step 2 on, 3 x (0x10 - min(+9 + i -
// 0x18, 0xF))) in dword 0x903854, the edges 12 x that, the first row's lower
// edge black. The heights of the quads: -(3 +0xA - i / 2) .. 0, then the
// running sums the original keeps in its frame (half of 2 +0xA, then of the
// previous row's 3 +0xA - i / 2, plus the previous row's height word).
S35_EXPORT void __cdecl LastResortBeam_DrawColumn(void) {
    std::uint16_t px, py;   // di / bp: the previous row's point
    std::int32_t l10;       // [esp+0x1C]: 2 +0xA, then the previous row's 3 +0xA - i / 2
    std::uint16_t l2c;      // [esp+0x38]'s low word: the previous row's height word
    unsigned char* s = Sc();
    {
        const std::uint32_t angle = static_cast<std::uint32_t>(s[0xB] & 0x1F) << 7;
        SetSD(0xC, angle);
        SetSD(0, static_cast<std::uint32_t>(Shl(0x60 - s[9], 1)));
        SetSD(8, static_cast<std::uint32_t>(Shl(-(3 * static_cast<int>(s[9])), 2)));
        l10 = 2 * static_cast<std::int32_t>(s[0xA]);
        const int sn = MH_CALL(Math_Sin)(static_cast<int>(angle));
        px = static_cast<std::uint16_t>(Mul12(sn, SD(0)));
        const int cs = MH_CALL(Math_Cos)(SD(0xC));
        py = static_cast<std::uint16_t>(Mul12(cs, SD(0)));
        l2c = SW(8);
        s = Sc();
        if (s[0xA] <= 1) return;
    }
    int i = 1;
    for (;;) {
        const std::uint32_t angle = static_cast<std::uint32_t>((s[0xB] - i) & 0x1F) << 7;
        SetSD(0xC, angle);
        SetSD(0, static_cast<std::uint32_t>(2 * (i - s[9]) + 0xC0));
        SetSD(8, static_cast<std::uint32_t>(12 * (i - s[9])));
        const std::int32_t top = 3 * static_cast<int>(s[0xA]) - DivPow2(i, 1);   // esi, then [esp+0x34]
        SetVW(8, px);
        SetVW(0xA, py);
        SetVW(0xC, static_cast<unsigned>(l2c - l10));
        SetVW(0x18, px);
        std::int32_t l20 = DivPow2(l10, 1);
        SetVW(0x1A, py);
        const std::int32_t l1c = l2c - l20;
        SetVW(0x1C, static_cast<unsigned>(l1c));
        // the first quad: from the height -top up to the previous row's words
        SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(angle))));
        {
            const int cs = MH_CALL(Math_Cos)(SD(0xC));
            const int cy = Mul12(cs, SD(0));
            const std::int32_t h = SD(8);
            SetVW(4, static_cast<unsigned>(h - top));
            const std::uint16_t x0 = VW(0);
            SetVW(2, static_cast<unsigned>(cy));
            SetVW(0x12, static_cast<unsigned>(cy));
            SetVW(0x10, x0);
            SetVW(0x14, static_cast<unsigned>(h - DivPow2(top, 1)));
        }
        const std::int32_t half = DivPow2(top, 1);   // [esp+0x18]
        s = Sc();
        const std::int32_t x = Add32(Shl(VS(0x18), 9), rv::As<records::BattleTask>(s)->x);
        const std::int32_t z = Add32(Shl(VS(0x1A), 9), rv::As<records::BattleTask>(s)->z);
        DrawMode(0x35);
        Link(x, z, 0, 0xC);
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        int level;
        s = Sc();
        if (s[2] == 2) {
            int e = static_cast<int>(s[9]) + i - 0x18;
            if (e > 0x10) e = 0xF;
            SetSD(4, static_cast<std::uint32_t>(3 * (0x10 - e)));
            level = 0x10 - e;
        } else {
            level = 0x10 - i;
            SetSD(4, static_cast<std::uint32_t>(3 * level));
        }
        const auto edge = static_cast<unsigned char>(level * 12);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = edge;
        p[0x25] = SB(4);
        p[0x26] = SB(4);
        if (i == 1) {
            p[0x34] = 1;
            p[0x35] = 1;
            p[0x36] = 1;
        } else {
            p[0x34] = edge;
            p[0x35] = SB(4);
            p[0x36] = SB(4);
        }
        Link(x, z, 0, 0x44);
        // the second quad
        {
            const std::uint32_t a2 = static_cast<std::uint32_t>(SD(0xC));
            SetVW(8, px);
            SetVW(0xA, py);
            SetVW(0xC, static_cast<unsigned>(l1c));
            SetVW(0x18, px);
            SetVW(0x1A, py);
            SetVW(0x1C, l2c);
            SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(a2))));
            const int cs = MH_CALL(Math_Cos)(SD(0xC));
            const int cy = Mul12(cs, SD(0));
            const std::int32_t h = SD(8);
            SetVW(0x14, static_cast<unsigned>(h));
            SetVW(4, static_cast<unsigned>(h - half));
            p = Gfx_PacketNext;
            const std::uint16_t x0 = VW(0);
            SetVW(2, static_cast<unsigned>(cy));
            SetVW(0x10, x0);
            SetVW(0x12, static_cast<unsigned>(cy));
        }
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = edge;
        p[5] = SB(4);
        p[6] = SB(4);
        p[0x14] = edge;
        p[0x15] = SB(4);
        p[0x16] = SB(4);
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = edge;
        Link(x, z, 0, 0x44);
        // the third quad
        {
            SetVW(0xC, l2c);
            SetVW(8, px);
            SetVW(0xA, py);
            SetVW(0x18, px);
            l20 = Add32(l20, l2c);
            SetVW(0x1C, static_cast<unsigned>(l20));
            const std::uint32_t a3 = static_cast<std::uint32_t>(SD(0xC));
            SetVW(0x1A, py);
            SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(a3))));
            const int cs = MH_CALL(Math_Cos)(SD(0xC));
            const std::int32_t h = SD(8);
            p = Gfx_PacketNext;
            const std::uint16_t x0 = VW(0);
            const int cy = Mul12(cs, SD(0));
            SetVW(2, static_cast<unsigned>(cy));
            SetVW(0x12, static_cast<unsigned>(cy));
            SetVW(4, static_cast<unsigned>(h));
            SetVW(0x10, x0);
            SetVW(0x14, static_cast<unsigned>(half + h));
        }
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u}) p[k] = edge;
        p[0x25] = SB(4);
        p[0x26] = SB(4);
        p[0x34] = edge;
        p[0x35] = SB(4);
        p[0x36] = SB(4);
        Link(x, z, 0, 0x44);
        // the fourth quad, under its own draw-mode packet
        {
            SetVW(0xC, static_cast<unsigned>(l20));
            const std::uint32_t a4 = static_cast<std::uint32_t>(SD(0xC));
            SetVW(8, px);
            SetVW(0xA, py);
            SetVW(0x18, px);
            SetVW(0x1A, py);
            SetVW(0x1C, static_cast<unsigned>(Add32(l10, l2c)));
            SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(a4))));
            const int cs = MH_CALL(Math_Cos)(SD(0xC));
            const std::int32_t h = SD(8);
            SetVW(4, static_cast<unsigned>(half + h));
            px = VW(0);
            const int cy = Mul12(cs, SD(0));
            SetVW(0x14, static_cast<unsigned>(Add32(h, top)));
            SetVW(2, static_cast<unsigned>(cy));
            SetVW(0x12, static_cast<unsigned>(cy));
            py = static_cast<std::uint16_t>(cy);
            unsigned char* const mode = Gfx_PacketNext;
            SetVW(0x10, px);
            l2c = static_cast<std::uint16_t>(h);
            l10 = top;
            MH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
        }
        Link(x, z, 0, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        if (i == 1) {
            p[0x14] = 1;
            p[0x15] = 1;
            p[0x16] = 1;
        } else {
            p[0x14] = edge;
            p[0x15] = SB(4);
            p[0x16] = SB(4);
        }
        p[4] = edge;
        p[5] = SB(4);
        p[6] = SB(4);
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        Link(x, z, 0, 0x44);
        s = Sc();
        ++i;
        if (i >= static_cast<int>(s[0xA])) return;
    }
}

// original 0x4F0470 (cdecl, one argument k: 2 or 3 from LastResortBeam_Run):
// a trail of semi-transparent flat tiles up the column, one every k rows
// from row 1 while the row is below +9 (from step 3 on, while +0xA + the row
// is at most 0x18), the shade 0x7B down by 5 k a tile (from step 3 on, 0x80 -
// 5 (+0xA + row)) until it is 8 or less; each tile's point the row's (as the
// column's) lifted by (k + 2) x row + Rand & 3 + 8, kept at or below 0, under
// a draw-mode packet.
S35_EXPORT void __cdecl LastResortBeam_DrawSparks(int k) {
    std::int32_t shade = 0x7B;         // [esp+0x10]
    const std::int32_t step = -5 * k;  // [esp+0x14]
    int row = 1;                       // ebp
    for (;;) {
        unsigned char* s = Sc();
        if (row >= static_cast<int>(s[9])) return;
        std::int32_t lift;
        if (s[2] == 3) {
            const int r = static_cast<int>(s[0xA]) + row;
            if (r > 0x18) return;
            const std::uint32_t rnd = RandCall();
            lift = Add32(static_cast<std::int32_t>(rnd & 3), Mul32(k + 2, r));
            SetSD(4, static_cast<std::uint32_t>(0x80 - 5 * r));
        } else {
            const std::uint32_t rnd = RandCall();
            lift = Add32(static_cast<std::int32_t>(rnd & 3), Mul32(k + 2, row));
            SetSD(4, static_cast<std::uint32_t>(shade));
        }
        s = Sc();
        const std::uint32_t angle = static_cast<std::uint32_t>((s[0xB] - row) & 0x1F) << 7;
        SetSD(0xC, angle);
        SetSD(0, static_cast<std::uint32_t>(2 * (row - s[9]) + 0xC0));
        SetSD(8, static_cast<std::uint32_t>(12 * (row - s[9])));
        SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(angle))));
        const int cs = MH_CALL(Math_Cos)(SD(0xC));
        const int cy = Mul12(cs, SD(0));
        const std::int32_t height = Add32(Add32(SD(8), lift), 8);
        SetVW(2, static_cast<unsigned>(cy));
        SetVW(4, static_cast<unsigned>(height));
        if (static_cast<short>(height) > 0) SetVW(4, 0);
        s = Sc();
        const std::int32_t x = Add32(Shl(VS(0), 9), rv::As<records::BattleTask>(s)->x);
        const std::int32_t z = Add32(Shl(static_cast<short>(cy), 9), rv::As<records::BattleTask>(s)->z);
        DrawMode(0x35);
        Link(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetTile1)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp1(p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        p[4] = SB(4);
        p[5] = SB(4);
        p[6] = SB(4);
        Link(x, z, 0, 0x44);
        shade = Add32(shade, step);
        row += k;
        if (shade <= 8) return;
    }
}

void LastResortBeam_Inject() {
    BOF3_INJECT(LastResort_Task);
    BOF3_INJECT(LastResort_Start);
    BOF3_INJECT(LastResort_WaitChildren);
    BOF3_INJECT(LastResortChild_Task);
    BOF3_INJECT(LastResortRing_Run);
    BOF3_INJECT(MagicFx_WaitOwnerChildren);
    BOF3_INJECT(LastResortRing_DrawDisc);
    BOF3_INJECT(LastResortRing_DrawBand);
    BOF3_INJECT(LastResortBeam_Run);
    BOF3_INJECT(LastResortBeam_Wait);
    BOF3_INJECT(LastResortBeam_Rise);
    BOF3_INJECT(LastResortBeam_Shrink);
    BOF3_INJECT(LastResortBeam_End);
    BOF3_INJECT(LastResortBeam_DrawColumn);
    BOF3_INJECT(LastResortBeam_DrawSparks);
}

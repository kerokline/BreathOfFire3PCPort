// Moved here by tools/move_funcs.py from magic_s27.cpp (docs/source-layout-round.md).
#include "game/magic/burn.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s27.h"
#include "game/magic_s27_callees.h"
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

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap its +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, std::uint32_t v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, std::uint32_t v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl eax, n` then `sar eax, 0xC`.
int Shl12(int a, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(a) << n) >> 12; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

void SetPtr(unsigned char* at, const void* p) {
    SetLong(at, static_cast<std::int32_t>(Low32(p)));
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// The originals read the scratch words a product or a sum needs after the
// call: these sequence it so (C++ leaves the order of a call and a read in one
// expression open).
int SinK(int a, unsigned k) {
    const int v = Sin(a);
    return Mul12(v, SS(k));
}

int CosK(int a, unsigned k) {
    const int v = Cos(a);
    return Mul12(v, SS(k));
}

// (sin << n) >> 12 plus a scratch word, 16 bits.
std::uint32_t SinShlW(int a, unsigned n, unsigned add) {
    const int v = Sin(a);
    return static_cast<std::uint32_t>(Shl12(v, n)) + SW(add);
}

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// The projections with the arguments the originals push: the depth and flag
// pointers past the vertices (ours reads the first).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S27_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// POLY_G3 / POLY_FT3-shaped: the three points at +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S27_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// POLY_G4 / POLY_FT4: the four points at +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S27_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}

// POLY_GT4: the four points at +8, +0x1C, +0x30, +0x44.
void Rtp4GT(unsigned char* prim) {
    long p, flag;
    S27_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x1C, prim + 0x30, prim + 0x44,
                                     &p, &flag);
}

// A draw mode (textured off, dithered, `tpage`) committed to ordering slot
// `slot`, as each draw brackets its primitives.
void ModeCommit(unsigned slot, unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The CLUT row the three spells light: row 26 of Gfx_ClutStrip (entries
// 0x1A00..) from its source, `runs` runs of fifteen entries (the first of
// each 0), each run's STP bit set as `stp` says, and the strip marked dirty.
void LightClutRow(unsigned runs, const bool* stp) {
    for (unsigned r = 0; r < runs; ++r)
        for (unsigned i = 1; i < 0x10; ++i) {
            const unsigned k = 0x1A00 + r * 0x10 + i;
            Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | (stp[r] ? 0x8000 : 0));
        }
    for (unsigned r = 0; r < runs; ++r) Gfx_ClutStrip[0x1A00 + r * 0x10] = 0;
    Gfx_ClutStripDirty = 1;
}

// The task at its owner: +0x34 / +0x38 / +0x3C from the owner's.
void AtOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

}  // namespace

#define S27_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The flame's climb: its speed +0x20 up by +0xA, its height +0x14 up by the
// speed (dwords).
void Climb() {
    unsigned char* const s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->work_20, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20) + s[0xA]));
    rv::Store(rv::As<records::BattleTask>(s)->step_y, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->step_y) +
                                                static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20)));
}

}  // namespace

// original 0x4DA220: the kind-2 task. A five-entry stack table by +1 -
// Burn_Start, Burn_WaitFlag, MAGIC041's wait for one child, SpellFx_Countdown,
// BattleFx_Finish - then, while +0 and +1 are set, the ring and the disc under
// the actor's matrix.
S27_EXPORT void __cdecl Burn_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Burn_Start, bof3::addr::Burn_WaitFlag,
                                                 bof3::addr::Berserk_WaitChildren, bof3::addr::SpellFx_Countdown,
                                                 bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("Burn_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[1] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::Burn_DrawRing);
    Call0(bof3::addr::Burn_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4DA290: the task at the source sprite (0x904B4C), +0xB 0; eight
// flames (kind 1, 0x2F), each +0x80 this task and +9 its delay (i + 1) x 4,
// +0xB counting them; CLUT row 26's two runs made semi-transparent; sound
// 0x100; +9 0, +1 on.
S27_EXPORT void __cdecl Burn_Start(void) {
    const unsigned char* const src = Source();
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x2F);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, self);
        child[9] = static_cast<unsigned char>((i + 1) << 2);
        Inc(self[0xB]);
    }
    static constexpr bool kStp[2] = {true, true};
    LightClutRow(2, kStp);
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4DA370: +9 up; at 0x10 the target flags 0x10 and +1 on.
S27_EXPORT void __cdecl Burn_WaitFlag(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x10) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[1]);
}

// original 0x4DA3B0: +9 down; at 0 +1 on. One body the linker kept for three
// overlays: Burn's fourth phase, Typhoon's third (MAGIC101, group S23) and
// MAGIC009's.
S27_EXPORT void __cdecl SpellFx_Countdown(void) {
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[1]);
}

// original 0x4DA3D0: the flame's kind-1 task: a call through BurnFlame_Phases
// (three entries) by +1; then, while +0 and +1 are set, the flame under the
// actor's matrix.
S27_EXPORT void __cdecl BurnFlame_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::BurnFlame_Start, bof3::addr::BurnFlame_Rise,
                                                 bof3::addr::BurnFlame_Fade};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("BurnFlame_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[1] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::BurnFlame_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4DA410: +9 down; at 0 the flame at its owner, +0x14 0, +0x20 8,
// +0xA 2, +0xB 0x10, +9 0x20, +1 on.
S27_EXPORT void __cdecl BurnFlame_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    AtOwner();
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 8);
    Sc()[0xA] = 2;
    Sc()[0xB] = 0x10;
    Sc()[9] = 0x20;
    Inc(Sc()[1]);
}

// original 0x4DA4A0: the climb; +9 down by two; at 0x10 +1 on.
S27_EXPORT void __cdecl BurnFlame_Rise(void) {
    Climb();
    AddB(Sc()[9], 0xFE);
    if (Sc()[9] == 0x10) Inc(Sc()[1]);
}

// original 0x4DA4E0: +0xB down; the climb; +9 down; at 0 the owner's count
// +0xB down and the task freed.
S27_EXPORT void __cdecl BurnFlame_Fade(void) {
    Dec(Sc()[0xB]);
    Climb();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DA540: the flame - a ring of 32 semi-transparent POLY_GT4 round
// the actor matrix's origin, radius +9 x 4, from the ground up to a wavering
// height (+0x14 + 0x20 plus a sine of the frame counter, two points a quad);
// the ground edge dark, the top +0xB x 8; page (0x340, 0x100), CLUT (0,
// 0x1FA), u from the quad's index x 8, v from +0xA. Tpage 0x35; each quad
// sorted at the task moved by its first point's x << 9 in both x and z.
S27_EXPORT void __cdecl BurnFlame_Draw(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0xA, static_cast<std::uint32_t>(s[0xB]) << 3);
        SetSW(0, static_cast<std::uint32_t>(s[9]) << 2);
        SetSW(6, Word(s + 0x14) + 0x20u);
        SetSW(8, (Frame_Counter & 0xF) << 8);
    }
    SetVW(8, static_cast<std::uint32_t>(CosK(0, 0)));
    SetVW(0xA, static_cast<std::uint32_t>(SinK(0, 0)));
    SetVW(0xC, 0u - SinShlW(0, 5, 6));
    SetVW(0x18, static_cast<std::uint32_t>(CosK(0, 0)));
    SetVW(0x1A, static_cast<std::uint32_t>(SinK(0, 0)));
    SetVW(0x1C, 0);
    unsigned k = 1;
    for (int a = 0x80; a < 0x1080; a += 0x80, ++k) {
        SetSW(8, ((Frame_Counter + k) & 0xF) << 8);
        {
            const std::uint16_t x = VW(8), y = VW(0xA), z = VW(0xC);
            SetVW(0, x);
            SetVW(2, y);
            SetVW(4, z);
        }
        SetVW(8, static_cast<std::uint32_t>(CosK(a, 0)));
        SetVW(0xA, static_cast<std::uint32_t>(SinK(a, 0)));
        {
            const std::uint32_t top = SinShlW(SS(8), 5, 6);
            const std::uint16_t x = VW(0x18), y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
            SetVW(0xC, 0u - top);
            SetVW(0x14, 0);
        }
        SetVW(0x18, static_cast<std::uint32_t>(CosK(a, 0)));
        SetVW(0x1A, static_cast<std::uint32_t>(SinK(a, 0)));
        SetVW(0x1C, 0);
        const unsigned char* s = Sc();
        const std::uint32_t shift = static_cast<std::uint32_t>(static_cast<int>(VS(8))) << 9;
        const std::uint32_t x = static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->x) + shift;
        const std::uint32_t z = static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->z) + shift;
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x18] = 1;
        p[0x19] = 1;
        p[0x1A] = 1;
        p[0x2C] = SB(0xA);
        p[0x2D] = SB(0xA);
        p[0x2E] = SB(0xA);
        p[0x40] = SB(0xA);
        p[0x41] = SB(0xA);
        p[0x42] = SB(0xA);
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        const auto u0 = static_cast<unsigned char>(k << 3);
        const auto u1 = static_cast<unsigned char>((k + 1) << 3);
        s = Sc();
        const auto v0 = static_cast<unsigned char>((s[0xA] + 8) << 2);
        const auto v1 = static_cast<unsigned char>((s[0xA] + 0xA) << 2);
        p[0x14] = u0;
        p[0x15] = v0;
        p[0x28] = u1;
        p[0x29] = v0;
        p[0x3C] = u0;
        p[0x3D] = v1;
        p[0x50] = u1;
        p[0x51] = v1;
        Rtp4GT(p);
        MH_CALL(Gte_PrimDepths4_14)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x54);
    }
}

// original 0x4DA8B0: a ring of 32 semi-transparent POLY_G4 round the actor
// matrix's origin, inner radius 0x80 lit +9 x 6, outer 0xC0 dark; tpage 0x55
// for the ring, 0x15 after it, all committed to slot 5.
S27_EXPORT void __cdecl Burn_DrawRing(void) {
    ModeCommit(5, 0x55);
    SetSW(2, 0x80);
    SetSW(4, 0xC0);
    SetSW(0xA, Sc()[9] * 6u);
    SetVW(8, static_cast<std::uint32_t>(SinK(0, 4)));
    SetVW(0xA, static_cast<std::uint32_t>(CosK(0, 4)));
    SetVW(0x18, static_cast<std::uint32_t>(SinK(0, 2)));
    SetVW(0x1A, static_cast<std::uint32_t>(CosK(0, 2)));
    SetVW(0x1C, 0);
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(8), y = VW(0xA), ix = VW(0x18), iy = VW(0x1A);
            SetVW(0, x);
            SetVW(2, y);
            SetVW(0x10, ix);
            SetVW(0x12, iy);
        }
        SetVW(8, static_cast<std::uint32_t>(SinK(a, 4)));
        SetVW(0xA, static_cast<std::uint32_t>(CosK(a, 4)));
        SetVW(0x18, static_cast<std::uint32_t>(SinK(a, 2)));
        SetVW(0x1A, static_cast<std::uint32_t>(CosK(a, 2)));
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned i : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[i] = 1;
        for (unsigned i : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[i] = SB(0xA);
        Commit(5, 0x44);
    }
    ModeCommit(5, 0x15);
}

// original 0x4DAB00: a disc of 32 semi-transparent POLY_G3 round the actor
// matrix's origin, radius 0x80; the centre (4a, 4a, 6a) and the rim (4a, 6a,
// 6a) for a = +9. Tpage 0x55 for the disc, 0x15 after it, slot 5.
S27_EXPORT void __cdecl Burn_DrawDisc(void) {
    ModeCommit(5, 0x55);
    {
        const unsigned char* const s = Sc();
        SetSW(0, 0x80);
        SetSW(0xA, static_cast<std::uint32_t>(s[9]) << 2);
        SetSW(0xC, s[9] * 6u);
    }
    SetVW(0x10, static_cast<std::uint32_t>(SinK(0, 0)));
    SetVW(0x12, static_cast<std::uint32_t>(CosK(0, 0)));
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(0x10), y = VW(0x12);
            SetVW(0, 0);
            SetVW(2, 0);
            SetVW(8, x);
            SetVW(0xA, y);
        }
        SetVW(0x10, static_cast<std::uint32_t>(SinK(a, 0)));
        SetVW(0x12, static_cast<std::uint32_t>(CosK(a, 0)));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(0xA);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        p[0x14] = SB(0xA);
        p[0x15] = SB(0xC);
        p[0x16] = SB(0xC);
        p[0x24] = SB(0xA);
        p[0x25] = SB(0xC);
        p[0x26] = SB(0xC);
        Commit(5, 0x34);
    }
    ModeCommit(5, 0x15);
}

void Burn_Inject() {
    BOF3_INJECT(Burn_Task);
    BOF3_INJECT(Burn_Start);
    BOF3_INJECT(Burn_WaitFlag);
    BOF3_INJECT(SpellFx_Countdown);
    BOF3_INJECT(BurnFlame_Task);
    BOF3_INJECT(BurnFlame_Start);
    BOF3_INJECT(BurnFlame_Rise);
    BOF3_INJECT(BurnFlame_Fade);
    BOF3_INJECT(BurnFlame_Draw);
    BOF3_INJECT(Burn_DrawRing);
    BOF3_INJECT(Burn_DrawDisc);
}
